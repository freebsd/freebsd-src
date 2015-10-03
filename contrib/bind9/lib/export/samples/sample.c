/*
 * Copyright (C) 2009, 2012-2015  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: sample.c,v 1.5 2009/09/29 15:06:07 fdupont Exp $ */

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
#include <isc/print.h>
#include <isc/sockaddr.h>
#include <isc/util.h>

#include <dns/client.h>
#include <dns/fixedname.h>
#include <dns/keyvalues.h>
#include <dns/lib.h>
#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/rdataset.h>
#include <dns/rdatastruct.h>
#include <dns/rdatatype.h>
#include <dns/result.h>
#include <dns/secalg.h>

#include <dst/dst.h>

static char *algname;

static isc_result_t
printdata(dns_rdataset_t *rdataset, dns_name_t *owner) {
	isc_buffer_t target;
	isc_result_t result;
	isc_region_t r;
	char t[4096];

	if (!dns_rdataset_isassociated(rdataset)) {
		printf("[WARN: empty]\n");
		return (ISC_R_SUCCESS);
	}

	isc_buffer_init(&target, t, sizeof(t));

	result = dns_rdataset_totext(rdataset, owner, ISC_FALSE, ISC_FALSE,
				     &target);
	if (result != ISC_R_SUCCESS)
		return (result);
	isc_buffer_usedregion(&target, &r);
	printf("%.*s", (int)r.length, (char *)r.base);

	return (ISC_R_SUCCESS);
}

ISC_PLATFORM_NORETURN_PRE static void
usage(void) ISC_PLATFORM_NORETURN_POST;

static void
usage(void) {
	fprintf(stderr, "sample [-t RRtype] "
		"[[-a algorithm] [-e] -k keyname -K keystring] "
		"[-s domain:serveraddr_for_domain ] "
		"server_address hostname\n");

	exit(1);
}

static void
set_key(dns_client_t *client, char *keynamestr, char *keystr,
	isc_boolean_t is_sep, isc_mem_t **mctxp)
{
	isc_result_t result;
	dns_fixedname_t fkeyname;
	size_t namelen;
	dns_name_t *keyname;
	dns_rdata_dnskey_t keystruct;
	unsigned char keydata[4096];
	isc_buffer_t keydatabuf;
	unsigned char rrdata[4096];
	isc_buffer_t rrdatabuf;
	isc_buffer_t b;
	isc_textregion_t tr;
	isc_region_t r;
	dns_secalg_t alg;

	result = isc_mem_create(0, 0, mctxp);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "failed to crate mctx\n");
		exit(1);
	}

	if (algname != NULL) {
		tr.base = algname;
		tr.length = strlen(algname);
		result = dns_secalg_fromtext(&alg, &tr);
		if (result != ISC_R_SUCCESS) {
			fprintf(stderr, "failed to identify the algorithm\n");
			exit(1);
		}
	} else
		alg = DNS_KEYALG_RSASHA1;

	keystruct.common.rdclass = dns_rdataclass_in;
	keystruct.common.rdtype = dns_rdatatype_dnskey;
	keystruct.flags = DNS_KEYOWNER_ZONE; /* fixed */
	if (is_sep)
		keystruct.flags |= DNS_KEYFLAG_KSK;
	keystruct.protocol = DNS_KEYPROTO_DNSSEC; /* fixed */
	keystruct.algorithm = alg;

	isc_buffer_init(&keydatabuf, keydata, sizeof(keydata));
	isc_buffer_init(&rrdatabuf, rrdata, sizeof(rrdata));
	result = isc_base64_decodestring(keystr, &keydatabuf);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "base64 decode failed\n");
		exit(1);
	}
	isc_buffer_usedregion(&keydatabuf, &r);
	keystruct.datalen = r.length;
	keystruct.data = r.base;

	result = dns_rdata_fromstruct(NULL, keystruct.common.rdclass,
				      keystruct.common.rdtype,
				      &keystruct, &rrdatabuf);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "failed to construct key rdata\n");
		exit(1);
	}
	namelen = strlen(keynamestr);
	isc_buffer_init(&b, keynamestr, namelen);
	isc_buffer_add(&b, namelen);
	dns_fixedname_init(&fkeyname);
	keyname = dns_fixedname_name(&fkeyname);
	result = dns_name_fromtext(keyname, &b, dns_rootname, 0, NULL);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "failed to construct key name\n");
		exit(1);
	}
	result = dns_client_addtrustedkey(client, dns_rdataclass_in,
					  keyname, &rrdatabuf);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "failed to add key for %s\n",
			keynamestr);
		exit(1);
	}
}

static void
addserver(dns_client_t *client, const char *addrstr, const char *port,
	  const char *namespace)
{
	struct addrinfo hints, *res;
	int gai_error;
	isc_sockaddr_t sa;
	isc_sockaddrlist_t servers;
	isc_result_t result;
	size_t namelen;
	isc_buffer_t b;
	dns_fixedname_t fname;
	dns_name_t *name = NULL;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_protocol = IPPROTO_UDP;
	hints.ai_flags = AI_NUMERICHOST;
	gai_error = getaddrinfo(addrstr, port, &hints, &res);
	if (gai_error != 0) {
		fprintf(stderr, "getaddrinfo failed: %s\n",
			gai_strerror(gai_error));
		exit(1);
	}
	INSIST(res->ai_addrlen <= sizeof(sa.type));
	memmove(&sa.type, res->ai_addr, res->ai_addrlen);
	sa.length = res->ai_addrlen;
	freeaddrinfo(res);
	ISC_LINK_INIT(&sa, link);
	ISC_LIST_INIT(servers);
	ISC_LIST_APPEND(servers, &sa, link);

	if (namespace != NULL) {
		namelen = strlen(namespace);
		isc_buffer_constinit(&b, namespace, namelen);
		isc_buffer_add(&b, namelen);
		dns_fixedname_init(&fname);
		name = dns_fixedname_name(&fname);
		result = dns_name_fromtext(name, &b, dns_rootname, 0, NULL);
		if (result != ISC_R_SUCCESS) {
			fprintf(stderr, "failed to convert qname: %d\n",
				result);
			exit(1);
		}
	}

	result = dns_client_setservers(client, dns_rdataclass_in, name,
				       &servers);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "set server failed: %d\n", result);
		exit(1);
	}
}

int
main(int argc, char *argv[]) {
	int ch;
	isc_textregion_t tr;
	char *altserver = NULL;
	char *altserveraddr = NULL;
	char *altservername = NULL;
	dns_client_t *client = NULL;
	char *keynamestr = NULL;
	char *keystr = NULL;
	isc_result_t result;
	isc_buffer_t b;
	dns_fixedname_t qname0;
	size_t namelen;
	dns_name_t *qname, *name;
	dns_rdatatype_t type = dns_rdatatype_a;
	dns_rdataset_t *rdataset;
	dns_namelist_t namelist;
	isc_mem_t *keymctx = NULL;
	unsigned int clientopt, resopt;
	isc_boolean_t is_sep = ISC_FALSE;
	const char *port = "53";

	while ((ch = getopt(argc, argv, "a:es:t:k:K:p:")) != -1) {
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
		case 'a':
			algname = optarg;
			break;
		case 'e':
			is_sep = ISC_TRUE;
			break;
		case 's':
			if (altserver != NULL) {
				fprintf(stderr, "alternate server "
					"already defined: %s\n",
					altserver);
				exit(1);
			}
			altserver = optarg;
			break;
		case 'k':
			keynamestr = optarg;
			break;
		case 'K':
			keystr = optarg;
			break;
		case 'p':
			port = optarg;
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;
	if (argc < 2)
		usage();

	if (altserver != NULL) {
		char *cp;

		cp = strchr(altserver, ':');
		if (cp == NULL) {
			fprintf(stderr, "invalid alternate server: %s\n",
				altserver);
			exit(1);
		}
		*cp = '\0';
		altservername = altserver;
		altserveraddr = cp + 1;
	}

	isc_lib_register();
	result = dns_lib_init();
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "dns_lib_init failed: %d\n", result);
		exit(1);
	}

	clientopt = 0;
	result = dns_client_create(&client, clientopt);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "dns_client_create failed: %d\n", result);
		exit(1);
	}

	/* Set the nameserver */
	addserver(client, argv[0], port, NULL);

	/* Set the alternate nameserver (when specified) */
	if (altserver != NULL)
		addserver(client, altserveraddr, port, altservername);

	/* Install DNSSEC key (if given) */
	if (keynamestr != NULL) {
		if (keystr == NULL) {
			fprintf(stderr,
				"key string is missing "
				"while key name is provided\n");
			exit(1);
		}
		set_key(client, keynamestr, keystr, is_sep, &keymctx);
	}

	/* Construct qname */
	namelen = strlen(argv[1]);
	isc_buffer_init(&b, argv[1], namelen);
	isc_buffer_add(&b, namelen);
	dns_fixedname_init(&qname0);
	qname = dns_fixedname_name(&qname0);
	result = dns_name_fromtext(qname, &b, dns_rootname, 0, NULL);
	if (result != ISC_R_SUCCESS)
		fprintf(stderr, "failed to convert qname: %d\n", result);

	/* Perform resolution */
	resopt = 0;
	if (keynamestr == NULL)
		resopt |= DNS_CLIENTRESOPT_NODNSSEC;
	ISC_LIST_INIT(namelist);
	result = dns_client_resolve(client, qname, dns_rdataclass_in, type,
				    resopt, &namelist);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr,
			"resolution failed: %s\n", dns_result_totext(result));
	}
	for (name = ISC_LIST_HEAD(namelist); name != NULL;
	     name = ISC_LIST_NEXT(name, link)) {
		for (rdataset = ISC_LIST_HEAD(name->list);
		     rdataset != NULL;
		     rdataset = ISC_LIST_NEXT(rdataset, link)) {
			if (printdata(rdataset, name) != ISC_R_SUCCESS)
				fprintf(stderr, "print data failed\n");
		}
	}

	dns_client_freeresanswer(client, &namelist);

	/* Cleanup */
	dns_client_destroy(&client);
	if (keynamestr != NULL)
		isc_mem_destroy(&keymctx);
	dns_lib_shutdown();

	return (0);
}
