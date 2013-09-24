/*
 * Copyright (C) 2009, 2010, 2012, 2013  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: sample-update.c,v 1.10 2010/12/09 00:54:34 marka Exp $ */

#include <config.h>

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>

#include <isc/buffer.h>
#include <isc/lex.h>
#include <isc/lib.h>
#include <isc/mem.h>
#include <isc/parseint.h>
#include <isc/sockaddr.h>
#include <isc/util.h>

#include <dns/callbacks.h>
#include <dns/client.h>
#include <dns/fixedname.h>
#include <dns/lib.h>
#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/rdataclass.h>
#include <dns/rdatalist.h>
#include <dns/rdataset.h>
#include <dns/rdatastruct.h>
#include <dns/rdatatype.h>
#include <dns/result.h>
#include <dns/secalg.h>
#include <dns/tsec.h>

#include <dst/dst.h>

static dns_tsec_t *tsec = NULL;
static const dns_rdataclass_t default_rdataclass = dns_rdataclass_in;
static isc_bufferlist_t usedbuffers;
static ISC_LIST(dns_rdatalist_t) usedrdatalists;

static void setup_tsec(char *keyfile, isc_mem_t *mctx);
static void update_addordelete(isc_mem_t *mctx, char *cmdline,
			       isc_boolean_t isdelete, dns_name_t *name);
static void evaluate_prereq(isc_mem_t *mctx, char *cmdline, dns_name_t *name);

ISC_PLATFORM_NORETURN_PRE static void
usage(void) ISC_PLATFORM_NORETURN_POST;

static void
usage(void) {
	fprintf(stderr, "sample-update "
		"[-a auth_server] "
		"[-k keyfile] "
		"[-p prerequisite] "
		"[-r recursive_server] "
		"[-z zonename] "
		"(add|delete) \"name TTL RRtype RDATA\"\n");
	exit(1);
}

int
main(int argc, char *argv[]) {
	int ch;
	struct addrinfo hints, *res;
	int gai_error;
	dns_client_t *client = NULL;
	char *zonenamestr = NULL;
	char *keyfilename = NULL;
	char *prereqstr = NULL;
	isc_sockaddrlist_t auth_servers;
	char *auth_server = NULL;
	char *recursive_server = NULL;
	isc_sockaddr_t sa_auth, sa_recursive;
	isc_sockaddrlist_t rec_servers;
	isc_result_t result;
	isc_boolean_t isdelete;
	isc_buffer_t b, *buf;
	dns_fixedname_t zname0, pname0, uname0;
	size_t namelen;
	dns_name_t *zname = NULL, *uname, *pname;
	dns_rdataset_t *rdataset;
	dns_rdatalist_t *rdatalist;
	dns_rdata_t *rdata;
	dns_namelist_t updatelist, prereqlist, *prereqlistp = NULL;
	isc_mem_t *umctx = NULL;

	while ((ch = getopt(argc, argv, "a:k:p:r:z:")) != -1) {
		switch (ch) {
		case 'k':
			keyfilename = optarg;
			break;
		case 'a':
			auth_server = optarg;
			break;
		case 'p':
			prereqstr = optarg;
			break;
		case 'r':
			recursive_server = optarg;
			break;
		case 'z':
			zonenamestr = optarg;
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;
	if (argc < 2)
		usage();

	/* command line argument validation */
	if (strcmp(argv[0], "delete") == 0)
		isdelete = ISC_TRUE;
	else if (strcmp(argv[0], "add") == 0)
		isdelete = ISC_FALSE;
	else {
		fprintf(stderr, "invalid update command: %s\n", argv[0]);
		exit(1);
	}

	if (auth_server == NULL && recursive_server == NULL) {
		fprintf(stderr, "authoritative or recursive server "
			"must be specified\n");
		usage();
	}

	/* Initialization */
	ISC_LIST_INIT(usedbuffers);
	ISC_LIST_INIT(usedrdatalists);
	ISC_LIST_INIT(prereqlist);
	ISC_LIST_INIT(auth_servers);
	isc_lib_register();
	result = dns_lib_init();
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "dns_lib_init failed: %d\n", result);
		exit(1);
	}
	result = isc_mem_create(0, 0, &umctx);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "failed to crate mctx\n");
		exit(1);
	}

	result = dns_client_create(&client, 0);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "dns_client_create failed: %d\n", result);
		exit(1);
	}

	/* Set the authoritative server */
	if (auth_server != NULL) {
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_protocol = IPPROTO_UDP;
		hints.ai_flags = AI_NUMERICHOST;
		gai_error = getaddrinfo(auth_server, "53", &hints, &res);
		if (gai_error != 0) {
			fprintf(stderr, "getaddrinfo failed: %s\n",
				gai_strerror(gai_error));
			exit(1);
		}
		INSIST(res->ai_addrlen <= sizeof(sa_auth.type));
		memcpy(&sa_auth.type, res->ai_addr, res->ai_addrlen);
		freeaddrinfo(res);
		sa_auth.length = res->ai_addrlen;
		ISC_LINK_INIT(&sa_auth, link);

		ISC_LIST_APPEND(auth_servers, &sa_auth, link);
	}

	/* Set the recursive server */
	if (recursive_server != NULL) {
		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_DGRAM;
		hints.ai_protocol = IPPROTO_UDP;
		hints.ai_flags = AI_NUMERICHOST;
		gai_error = getaddrinfo(recursive_server, "53", &hints, &res);
		if (gai_error != 0) {
			fprintf(stderr, "getaddrinfo failed: %s\n",
				gai_strerror(gai_error));
			exit(1);
		}
		INSIST(res->ai_addrlen <= sizeof(sa_recursive.type));
		memcpy(&sa_recursive.type, res->ai_addr, res->ai_addrlen);
		freeaddrinfo(res);
		sa_recursive.length = res->ai_addrlen;
		ISC_LINK_INIT(&sa_recursive, link);
		ISC_LIST_INIT(rec_servers);
		ISC_LIST_APPEND(rec_servers, &sa_recursive, link);
		result = dns_client_setservers(client, dns_rdataclass_in,
					       NULL, &rec_servers);
		if (result != ISC_R_SUCCESS) {
			fprintf(stderr, "set server failed: %d\n", result);
			exit(1);
		}
	}

	/* Construct zone name */
	zname = NULL;
	if (zonenamestr != NULL) {
		namelen = strlen(zonenamestr);
		isc_buffer_init(&b, zonenamestr, namelen);
		isc_buffer_add(&b, namelen);
		dns_fixedname_init(&zname0);
		zname = dns_fixedname_name(&zname0);
		result = dns_name_fromtext(zname, &b, dns_rootname, 0, NULL);
		if (result != ISC_R_SUCCESS)
			fprintf(stderr, "failed to convert zone name: %d\n",
				result);
	}

	/* Construct prerequisite name (if given) */
	if (prereqstr != NULL) {
		dns_fixedname_init(&pname0);
		pname = dns_fixedname_name(&pname0);
		evaluate_prereq(umctx, prereqstr, pname);
		ISC_LIST_APPEND(prereqlist, pname, link);
		prereqlistp = &prereqlist;
	}

	/* Construct update name */
	ISC_LIST_INIT(updatelist);
	dns_fixedname_init(&uname0);
	uname = dns_fixedname_name(&uname0);
	update_addordelete(umctx, argv[1], isdelete, uname);
	ISC_LIST_APPEND(updatelist, uname, link);

	/* Set up TSIG/SIG(0) key (if given) */
	if (keyfilename != NULL)
		setup_tsec(keyfilename, umctx);

	/* Perform update */
	result = dns_client_update(client,
				   default_rdataclass, /* XXX: fixed */
				   zname, prereqlistp, &updatelist,
				   (auth_server == NULL) ? NULL :
				   &auth_servers, tsec, 0);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr,
			"update failed: %s\n", dns_result_totext(result));
	} else
		fprintf(stderr, "update succeeded\n");

	/* Cleanup */
	while ((pname = ISC_LIST_HEAD(prereqlist)) != NULL) {
		while ((rdataset = ISC_LIST_HEAD(pname->list)) != NULL) {
			ISC_LIST_UNLINK(pname->list, rdataset, link);
			dns_rdataset_disassociate(rdataset);
			isc_mem_put(umctx, rdataset, sizeof(*rdataset));
		}
		ISC_LIST_UNLINK(prereqlist, pname, link);
	}
	while ((uname = ISC_LIST_HEAD(updatelist)) != NULL) {
		while ((rdataset = ISC_LIST_HEAD(uname->list)) != NULL) {
			ISC_LIST_UNLINK(uname->list, rdataset, link);
			dns_rdataset_disassociate(rdataset);
			isc_mem_put(umctx, rdataset, sizeof(*rdataset));
		}
		ISC_LIST_UNLINK(updatelist, uname, link);
	}
	while ((rdatalist = ISC_LIST_HEAD(usedrdatalists)) != NULL) {
		while ((rdata = ISC_LIST_HEAD(rdatalist->rdata)) != NULL) {
			ISC_LIST_UNLINK(rdatalist->rdata, rdata, link);
			isc_mem_put(umctx, rdata, sizeof(*rdata));
		}
		ISC_LIST_UNLINK(usedrdatalists, rdatalist, link);
		isc_mem_put(umctx, rdatalist, sizeof(*rdatalist));
	}
	while ((buf = ISC_LIST_HEAD(usedbuffers)) != NULL) {
		ISC_LIST_UNLINK(usedbuffers, buf, link);
		isc_buffer_free(&buf);
	}
	if (tsec != NULL)
		dns_tsec_destroy(&tsec);
	isc_mem_destroy(&umctx);
	dns_client_destroy(&client);
	dns_lib_shutdown();

	return (0);
}

/*
 *  Subroutines borrowed from nsupdate.c
 */
#define MAXWIRE (64 * 1024)
#define TTL_MAX 2147483647U	/* Maximum signed 32 bit integer. */

static char *
nsu_strsep(char **stringp, const char *delim) {
	char *string = *stringp;
	char *s;
	const char *d;
	char sc, dc;

	if (string == NULL)
		return (NULL);

	for (; *string != '\0'; string++) {
		sc = *string;
		for (d = delim; (dc = *d) != '\0'; d++) {
			if (sc == dc)
				break;
		}
		if (dc == 0)
			break;
	}

	for (s = string; *s != '\0'; s++) {
		sc = *s;
		for (d = delim; (dc = *d) != '\0'; d++) {
			if (sc == dc) {
				*s++ = '\0';
				*stringp = s;
				return (string);
			}
		}
	}
	*stringp = NULL;
	return (string);
}

static void
fatal(const char *format, ...) {
	va_list args;

	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
	fprintf(stderr, "\n");
	exit(1);
}

static inline void
check_result(isc_result_t result, const char *msg) {
	if (result != ISC_R_SUCCESS)
		fatal("%s: %s", msg, isc_result_totext(result));
}

static void
parse_name(char **cmdlinep, dns_name_t *name) {
	isc_result_t result;
	char *word;
	isc_buffer_t source;

	word = nsu_strsep(cmdlinep, " \t\r\n");
	if (word == NULL || *word == 0) {
		fprintf(stderr, "could not read owner name\n");
		exit(1);
	}

	isc_buffer_init(&source, word, strlen(word));
	isc_buffer_add(&source, strlen(word));
	result = dns_name_fromtext(name, &source, dns_rootname, 0, NULL);
	check_result(result, "dns_name_fromtext");
	isc_buffer_invalidate(&source);
}

static void
parse_rdata(isc_mem_t *mctx, char **cmdlinep, dns_rdataclass_t rdataclass,
	    dns_rdatatype_t rdatatype, dns_rdata_t *rdata)
{
	char *cmdline = *cmdlinep;
	isc_buffer_t source, *buf = NULL, *newbuf = NULL;
	isc_region_t r;
	isc_lex_t *lex = NULL;
	dns_rdatacallbacks_t callbacks;
	isc_result_t result;

	while (cmdline != NULL && *cmdline != 0 &&
	       isspace((unsigned char)*cmdline))
		cmdline++;

	if (cmdline != NULL && *cmdline != 0) {
		dns_rdatacallbacks_init(&callbacks);
		result = isc_lex_create(mctx, strlen(cmdline), &lex);
		check_result(result, "isc_lex_create");
		isc_buffer_init(&source, cmdline, strlen(cmdline));
		isc_buffer_add(&source, strlen(cmdline));
		result = isc_lex_openbuffer(lex, &source);
		check_result(result, "isc_lex_openbuffer");
		result = isc_buffer_allocate(mctx, &buf, MAXWIRE);
		check_result(result, "isc_buffer_allocate");
		result = dns_rdata_fromtext(rdata, rdataclass, rdatatype, lex,
					    dns_rootname, 0, mctx, buf,
					    &callbacks);
		isc_lex_destroy(&lex);
		if (result == ISC_R_SUCCESS) {
			isc_buffer_usedregion(buf, &r);
			result = isc_buffer_allocate(mctx, &newbuf, r.length);
			check_result(result, "isc_buffer_allocate");
			isc_buffer_putmem(newbuf, r.base, r.length);
			isc_buffer_usedregion(newbuf, &r);
			dns_rdata_reset(rdata);
			dns_rdata_fromregion(rdata, rdataclass, rdatatype, &r);
			isc_buffer_free(&buf);
			ISC_LIST_APPEND(usedbuffers, newbuf, link);
		} else {
			fprintf(stderr, "invalid rdata format: %s\n",
				isc_result_totext(result));
			isc_buffer_free(&buf);
			exit(1);
		}
	} else {
		rdata->flags = DNS_RDATA_UPDATE;
	}
	*cmdlinep = cmdline;
}

static void
update_addordelete(isc_mem_t *mctx, char *cmdline, isc_boolean_t isdelete,
		   dns_name_t *name)
{
	isc_result_t result;
	isc_uint32_t ttl;
	char *word;
	dns_rdataclass_t rdataclass;
	dns_rdatatype_t rdatatype;
	dns_rdata_t *rdata = NULL;
	dns_rdatalist_t *rdatalist = NULL;
	dns_rdataset_t *rdataset = NULL;
	isc_textregion_t region;

	/*
	 * Read the owner name.
	 */
	parse_name(&cmdline, name);

	rdata = isc_mem_get(mctx, sizeof(*rdata));
	if (rdata == NULL) {
		fprintf(stderr, "memory allocation for rdata failed\n");
		exit(1);
	}
	dns_rdata_init(rdata);

	/*
	 * If this is an add, read the TTL and verify that it's in range.
	 * If it's a delete, ignore a TTL if present (for compatibility).
	 */
	word = nsu_strsep(&cmdline, " \t\r\n");
	if (word == NULL || *word == 0) {
		if (!isdelete) {
			fprintf(stderr, "could not read owner ttl\n");
			exit(1);
		}
		else {
			ttl = 0;
			rdataclass = dns_rdataclass_any;
			rdatatype = dns_rdatatype_any;
			rdata->flags = DNS_RDATA_UPDATE;
			goto doneparsing;
		}
	}
	result = isc_parse_uint32(&ttl, word, 10);
	if (result != ISC_R_SUCCESS) {
		if (isdelete) {
			ttl = 0;
			goto parseclass;
		} else {
			fprintf(stderr, "ttl '%s': %s\n", word,
				isc_result_totext(result));
			exit(1);
		}
	}

	if (isdelete)
		ttl = 0;
	else if (ttl > TTL_MAX) {
		fprintf(stderr, "ttl '%s' is out of range (0 to %u)\n",
			word, TTL_MAX);
		exit(1);
	}

	/*
	 * Read the class or type.
	 */
	word = nsu_strsep(&cmdline, " \t\r\n");
 parseclass:
	if (word == NULL || *word == 0) {
		if (isdelete) {
			rdataclass = dns_rdataclass_any;
			rdatatype = dns_rdatatype_any;
			rdata->flags = DNS_RDATA_UPDATE;
		goto doneparsing;
		} else {
			fprintf(stderr, "could not read class or type\n");
			exit(1);
		}
	}
	region.base = word;
	region.length = strlen(word);
	result = dns_rdataclass_fromtext(&rdataclass, &region);
	if (result == ISC_R_SUCCESS) {
		/*
		 * Now read the type.
		 */
		word = nsu_strsep(&cmdline, " \t\r\n");
		if (word == NULL || *word == 0) {
			if (isdelete) {
				rdataclass = dns_rdataclass_any;
				rdatatype = dns_rdatatype_any;
				rdata->flags = DNS_RDATA_UPDATE;
				goto doneparsing;
			} else {
				fprintf(stderr, "could not read type\n");
				exit(1);
			}
		}
		region.base = word;
		region.length = strlen(word);
		result = dns_rdatatype_fromtext(&rdatatype, &region);
		if (result != ISC_R_SUCCESS) {
			fprintf(stderr, "'%s' is not a valid type: %s\n",
				word, isc_result_totext(result));
			exit(1);
		}
	} else {
		rdataclass = default_rdataclass;
		result = dns_rdatatype_fromtext(&rdatatype, &region);
		if (result != ISC_R_SUCCESS) {
			fprintf(stderr, "'%s' is not a valid class or type: "
				"%s\n", word, isc_result_totext(result));
			exit(1);
		}
	}

	parse_rdata(mctx, &cmdline, rdataclass, rdatatype, rdata);

	if (isdelete) {
		if ((rdata->flags & DNS_RDATA_UPDATE) != 0)
			rdataclass = dns_rdataclass_any;
		else
			rdataclass = dns_rdataclass_none;
	} else {
		if ((rdata->flags & DNS_RDATA_UPDATE) != 0) {
			fprintf(stderr, "could not read rdata\n");
			exit(1);
		}
	}

 doneparsing:

	rdatalist = isc_mem_get(mctx, sizeof(*rdatalist));
	if (rdatalist == NULL) {
		fprintf(stderr, "memory allocation for rdatalist failed\n");
		exit(1);
	}
	dns_rdatalist_init(rdatalist);
	rdatalist->type = rdatatype;
	rdatalist->rdclass = rdataclass;
	rdatalist->covers = rdatatype;
	rdatalist->ttl = (dns_ttl_t)ttl;
	ISC_LIST_INIT(rdatalist->rdata);
	ISC_LIST_APPEND(rdatalist->rdata, rdata, link);
	ISC_LIST_APPEND(usedrdatalists, rdatalist, link);

	rdataset = isc_mem_get(mctx, sizeof(*rdataset));
	if (rdataset == NULL) {
		fprintf(stderr, "memory allocation for rdataset failed\n");
		exit(1);
	}
	dns_rdataset_init(rdataset);
	dns_rdatalist_tordataset(rdatalist, rdataset);
	ISC_LIST_INIT(name->list);
	ISC_LIST_APPEND(name->list, rdataset, link);
}

static void
make_prereq(isc_mem_t *mctx, char *cmdline, isc_boolean_t ispositive,
	    isc_boolean_t isrrset, dns_name_t *name)
{
	isc_result_t result;
	char *word;
	isc_textregion_t region;
	dns_rdataset_t *rdataset = NULL;
	dns_rdatalist_t *rdatalist = NULL;
	dns_rdataclass_t rdataclass;
	dns_rdatatype_t rdatatype;
	dns_rdata_t *rdata = NULL;

	/*
	 * Read the owner name
	 */
	parse_name(&cmdline, name);

	/*
	 * If this is an rrset prereq, read the class or type.
	 */
	if (isrrset) {
		word = nsu_strsep(&cmdline, " \t\r\n");
		if (word == NULL || *word == 0) {
			fprintf(stderr, "could not read class or type\n");
			exit(1);
		}
		region.base = word;
		region.length = strlen(word);
		result = dns_rdataclass_fromtext(&rdataclass, &region);
		if (result == ISC_R_SUCCESS) {
			/*
			 * Now read the type.
			 */
			word = nsu_strsep(&cmdline, " \t\r\n");
			if (word == NULL || *word == 0) {
				fprintf(stderr, "could not read type\n");
				exit(1);
			}
			region.base = word;
			region.length = strlen(word);
			result = dns_rdatatype_fromtext(&rdatatype, &region);
			if (result != ISC_R_SUCCESS) {
				fprintf(stderr, "invalid type: %s\n", word);
				exit(1);
			}
		} else {
			rdataclass = default_rdataclass;
			result = dns_rdatatype_fromtext(&rdatatype, &region);
			if (result != ISC_R_SUCCESS) {
				fprintf(stderr, "invalid type: %s\n", word);
				exit(1);
			}
		}
	} else
		rdatatype = dns_rdatatype_any;

	rdata = isc_mem_get(mctx, sizeof(*rdata));
	if (rdata == NULL) {
		fprintf(stderr, "memory allocation for rdata failed\n");
		exit(1);
	}
	dns_rdata_init(rdata);

	if (isrrset && ispositive)
		parse_rdata(mctx, &cmdline, rdataclass, rdatatype, rdata);
	else
		rdata->flags = DNS_RDATA_UPDATE;

	rdatalist = isc_mem_get(mctx, sizeof(*rdatalist));
	if (rdatalist == NULL) {
		fprintf(stderr, "memory allocation for rdatalist failed\n");
		exit(1);
	}
	dns_rdatalist_init(rdatalist);
	rdatalist->type = rdatatype;
	if (ispositive) {
		if (isrrset && rdata->data != NULL)
			rdatalist->rdclass = rdataclass;
		else
			rdatalist->rdclass = dns_rdataclass_any;
	} else
		rdatalist->rdclass = dns_rdataclass_none;
	rdatalist->covers = 0;
	rdatalist->ttl = 0;
	rdata->rdclass = rdatalist->rdclass;
	rdata->type = rdatatype;
	ISC_LIST_INIT(rdatalist->rdata);
	ISC_LIST_APPEND(rdatalist->rdata, rdata, link);
	ISC_LIST_APPEND(usedrdatalists, rdatalist, link);

	rdataset = isc_mem_get(mctx, sizeof(*rdataset));
	if (rdataset == NULL) {
		fprintf(stderr, "memory allocation for rdataset failed\n");
		exit(1);
	}
	dns_rdataset_init(rdataset);
	dns_rdatalist_tordataset(rdatalist, rdataset);
	ISC_LIST_INIT(name->list);
	ISC_LIST_APPEND(name->list, rdataset, link);
}

static void
evaluate_prereq(isc_mem_t *mctx, char *cmdline, dns_name_t *name) {
	char *word;
	isc_boolean_t ispositive, isrrset;

	word = nsu_strsep(&cmdline, " \t\r\n");
	if (word == NULL || *word == 0) {
		fprintf(stderr, "could not read operation code\n");
		exit(1);
	}
	if (strcasecmp(word, "nxdomain") == 0) {
		ispositive = ISC_FALSE;
		isrrset = ISC_FALSE;
	} else if (strcasecmp(word, "yxdomain") == 0) {
		ispositive = ISC_TRUE;
		isrrset = ISC_FALSE;
	} else if (strcasecmp(word, "nxrrset") == 0) {
		ispositive = ISC_FALSE;
		isrrset = ISC_TRUE;
	} else if (strcasecmp(word, "yxrrset") == 0) {
		ispositive = ISC_TRUE;
		isrrset = ISC_TRUE;
	} else {
		fprintf(stderr, "incorrect operation code: %s\n", word);
		exit(1);
	}

	make_prereq(mctx, cmdline, ispositive, isrrset, name);
}

static void
setup_tsec(char *keyfile, isc_mem_t *mctx) {
	dst_key_t *dstkey = NULL;
	isc_result_t result;
	dns_tsectype_t tsectype;

	result = dst_key_fromnamedfile(keyfile, NULL,
				       DST_TYPE_PRIVATE | DST_TYPE_KEY, mctx,
				       &dstkey);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "could not read key from %s: %s\n",
			keyfile, isc_result_totext(result));
		exit(1);
	}

	if (dst_key_alg(dstkey) == DST_ALG_HMACMD5)
		tsectype = dns_tsectype_tsig;
	else
		tsectype = dns_tsectype_sig0;

	result = dns_tsec_create(mctx, tsectype, dstkey, &tsec);
	dst_key_free(&dstkey);
	if (result != ISC_R_SUCCESS) {
		fprintf(stderr, "could not create tsec: %s\n",
			isc_result_totext(result));
		exit(1);
	}
}
