/*
 * Copyright (C) 2009  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: sample-request.c,v 1.5 2009-09-29 15:06:07 fdupont Exp $ */

#include <config.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>

#include <isc/base64.h>
#include <isc/buffer.h>
#include <isc/lib.h>
#include <isc/mem.h>
#include <isc/sockaddr.h>
#include <isc/util.h>

#include <dns/client.h>
#include <dns/fixedname.h>
#include <dns/keyvalues.h>
#include <dns/lib.h>
#include <dns/masterdump.h>
#include <dns/message.h>
#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/rdataset.h>
#include <dns/rdatastruct.h>
#include <dns/rdatatype.h>
#include <dns/result.h>
#include <dns/secalg.h>

#include <dst/dst.h>

static isc_mem_t *mctx;
static dns_fixedname_t fixedqname;

ISC_PLATFORM_NORETURN_PRE static void
usage(void) ISC_PLATFORM_NORETURN_POST;

static void
usage(void) {
	fprintf(stderr, "sample-request [-t RRtype] server_address hostname\n");

	exit(1);
}

static isc_result_t
make_querymessage(dns_message_t *message, const char *namestr,
		  dns_rdatatype_t rdtype)
{
	dns_name_t *qname = NULL, *qname0;
	dns_rdataset_t *qrdataset = NULL;
	isc_result_t result;
	isc_buffer_t b;
	size_t namelen;

	/* Construct qname */
	namelen = strlen(namestr);
	isc_buffer_init(&b, namestr, namelen);
	isc_buffer_add(&b, namelen);
	dns_fixedname_init(&fixedqname);
	qname0 = dns_fixedname_name(&fixedqname);
	result = dns_name_fromtext(qname0, &b, dns_rootname, 0, NULL);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "failed to convert qname: %d\n", result);
		return (result);
	}

	/* Construct query message */
	message->opcode = dns_opcode_query;
	message->rdclass = dns_rdataclass_in;

	result = dns_message_gettempname(message, &qname);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	result = dns_message_gettemprdataset(message, &qrdataset);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	dns_name_init(qname, NULL);
	dns_name_clone(qname0, qname);
	dns_rdataset_init(qrdataset);
	dns_rdataset_makequestion(qrdataset, message->rdclass, rdtype);
	ISC_LIST_APPEND(qname->list, qrdataset, link);
	dns_message_addname(message, qname, DNS_SECTION_QUESTION);

	return (ISC_R_SUCCESS);

 cleanup:
	if (qname != NULL)
		dns_message_puttempname(message, &qname);
	if (qrdataset != NULL)
		dns_message_puttemprdataset(message, &qrdataset);
	if (message != NULL)
		dns_message_destroy(&message);
	return (result);
}

static void
print_section(dns_message_t *message, int section, isc_buffer_t *buf) {
	isc_result_t result;
	isc_region_t r;

	result = dns_message_sectiontotext(message, section,
					   &dns_master_style_full, 0, buf);
	if (result != ISC_R_SUCCESS)
		goto fail;

	isc_buffer_usedregion(buf, &r);
	printf("%.*s", (int)r.length, (char *)r.base);

	return;

 fail:
	fprintf(stderr, "failed to convert a section\n");
}

int
main(int argc, char *argv[]) {
	int ch, i, gai_error;
	struct addrinfo hints, *res;
	isc_textregion_t tr;
	dns_client_t *client = NULL;
	isc_result_t result;
	isc_sockaddr_t sa;
	dns_message_t *qmessage, *rmessage;
	dns_rdatatype_t type = dns_rdatatype_a;
	isc_buffer_t *outputbuf;

	while ((ch = getopt(argc, argv, "t:")) != -1) {
		switch (ch) {
		case 't':
			tr.base = optarg;
			tr.length = strlen(optarg);
			result = dns_rdatatype_fromtext(&type, &tr);
			if (result != ISC_R_SUCCESS) {
				fprintf(stderr,
					"invalid RRtype: %s\n", optarg);
				exit(1);
			}
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;
	if (argc < 2)
		usage();

	isc_lib_register();
	result = dns_lib_init();
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "dns_lib_init failed: %d\n", result);
		exit(1);
	}

	result = dns_client_create(&client, 0);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "dns_client_create failed: %d\n", result);
		exit(1);
	}

	/* Prepare message structures */
	mctx = NULL;
	qmessage = NULL;
	rmessage = NULL;

	result = isc_mem_create(0, 0, &mctx);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "failed to create a memory context\n");
		exit(1);
	}
	result = dns_message_create(mctx, DNS_MESSAGE_INTENTRENDER, &qmessage);
	if (result == ISC_R_SUCCESS) {
		result = dns_message_create(mctx, DNS_MESSAGE_INTENTPARSE,
					    &rmessage);
	}
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "failed to create messages\n");
		exit(1);
	}

	/* Initialize the nameserver address */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	hints.ai_flags = AI_NUMERICHOST;
	gai_error = getaddrinfo(argv[0], "53", &hints, &res);
	if (gai_error != 0) {
		fprintf(stderr, "getaddrinfo failed: %s\n",
			gai_strerror(gai_error));
		exit(1);
	}
	INSIST(res->ai_addrlen <= sizeof(sa.type));
	memcpy(&sa.type, res->ai_addr, res->ai_addrlen);
	freeaddrinfo(res);
	sa.length = res->ai_addrlen;
	ISC_LINK_INIT(&sa, link);

	/* Construct qname */
	result = make_querymessage(qmessage, argv[1], type);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "failed to create a query\n");
		exit(1);
	}

	/* Send request and wait for a response */
	result = dns_client_request(client, qmessage, rmessage, &sa, 0, 0,
				    NULL, 60, 0, 3);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "failed to get a response: %s\n",
			dns_result_totext(result));
	}

	/* Dump the response */
	outputbuf = NULL;
	result = isc_buffer_allocate(mctx, &outputbuf, 65535);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "failed to allocate a result buffer\n");
		exit(1);
	}
	for (i = 0; i < DNS_SECTION_MAX; i++) {
		print_section(rmessage, i, outputbuf);
		isc_buffer_clear(outputbuf);
	}
	isc_buffer_free(&outputbuf);

	/* Cleanup */
	dns_message_destroy(&qmessage);
	dns_message_destroy(&rmessage);
	isc_mem_destroy(&mctx);
	dns_client_destroy(&client);
	dns_lib_shutdown();

	exit(0);
}
