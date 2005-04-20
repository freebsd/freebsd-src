/*
 * Portions Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Portions Copyright (C) 2000-2003  Internet Software Consortium.
 * Portions Copyright (C) 1995-2000 by Network Associates, Inc.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC AND NETWORK ASSOCIATES DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE
 * FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
 * IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: dnssec-makekeyset.c,v 1.52.2.1.10.7 2004/08/28 06:25:27 marka Exp $ */

#include <config.h>

#include <stdlib.h>

#include <isc/commandline.h>
#include <isc/entropy.h>
#include <isc/mem.h>
#include <isc/print.h>
#include <isc/string.h>
#include <isc/util.h>

#include <dns/db.h>
#include <dns/diff.h>
#include <dns/dnssec.h>
#include <dns/fixedname.h>
#include <dns/log.h>
#include <dns/rdata.h>
#include <dns/rdataset.h>
#include <dns/result.h>
#include <dns/secalg.h>
#include <dns/time.h>

#include <dst/dst.h>

#include "dnssectool.h"

const char *program = "dnssec-makekeyset";
int verbose;

typedef struct keynode keynode_t;
struct keynode {
	dst_key_t *key;
	ISC_LINK(keynode_t) link;
};
typedef ISC_LIST(keynode_t) keylist_t;

static isc_stdtime_t starttime = 0, endtime = 0, now;
static int ttl = -1;

static isc_mem_t *mctx = NULL;
static isc_entropy_t *ectx = NULL;

static keylist_t keylist;

static void
usage(void) {
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "\t%s [options] keys\n", program);

	fprintf(stderr, "\n");

	fprintf(stderr, "Version: %s\n", VERSION);

	fprintf(stderr, "Options: (default value in parenthesis) \n");
	fprintf(stderr, "\t-a\n");
	fprintf(stderr, "\t\tverify generated signatures\n");
	fprintf(stderr, "\t-s YYYYMMDDHHMMSS|+offset:\n");
	fprintf(stderr, "\t\tSIG start time - absolute|offset (now)\n");
	fprintf(stderr, "\t-e YYYYMMDDHHMMSS|+offset|\"now\"+offset]:\n");
	fprintf(stderr, "\t\tSIG end time  - "
			     "absolute|from start|from now (now + 30 days)\n");
	fprintf(stderr, "\t-t ttl\n");
	fprintf(stderr, "\t-p\n");
	fprintf(stderr, "\t\tuse pseudorandom data (faster but less secure)\n");
	fprintf(stderr, "\t-r randomdev:\n");
	fprintf(stderr, "\t\ta file containing random data\n");
	fprintf(stderr, "\t-v level:\n");
	fprintf(stderr, "\t\tverbose level (0)\n");

	fprintf(stderr, "\n");

	fprintf(stderr, "keys:\n");
	fprintf(stderr, "\tkeyfile (Kname+alg+tag)\n");

	fprintf(stderr, "\n");

	fprintf(stderr, "Output:\n");
	fprintf(stderr, "\tkeyset (keyset-<name>)\n");
	exit(0);
}

static isc_boolean_t
zonekey_on_list(dst_key_t *key) {
	keynode_t *keynode;
	for (keynode = ISC_LIST_HEAD(keylist);
	     keynode != NULL;
	     keynode = ISC_LIST_NEXT(keynode, link))
	{
		if (dst_key_compare(keynode->key, key))
			return (ISC_TRUE);
	}
	return (ISC_FALSE);
}

int
main(int argc, char *argv[]) {
	int i, ch;
	char *startstr = NULL, *endstr = NULL;
	dns_fixedname_t fdomain;
	dns_name_t *domain = NULL;
	char *output = NULL;
	char *endp;
	unsigned char data[65536];
	dns_db_t *db;
	dns_dbversion_t *version;
	dns_diff_t diff;
	dns_difftuple_t *tuple;
	dns_fixedname_t tname;
	dst_key_t *key = NULL;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdataset_t rdataset;
	dns_rdataclass_t rdclass;
	isc_result_t result;
	isc_buffer_t b;
	isc_region_t r;
	isc_log_t *log = NULL;
	keynode_t *keynode;
	unsigned int eflags;
	isc_boolean_t pseudorandom = ISC_FALSE;
	isc_boolean_t tryverify = ISC_FALSE;

	result = isc_mem_create(0, 0, &mctx);
	if (result != ISC_R_SUCCESS)
		fatal("failed to create memory context: %s",
		      isc_result_totext(result));

	dns_result_register();

	while ((ch = isc_commandline_parse(argc, argv, "as:e:t:r:v:ph")) != -1)
	{
		switch (ch) {
		case 'a':
			tryverify = ISC_TRUE;
			break;
		case 's':
			startstr = isc_commandline_argument;
			break;

		case 'e':
			endstr = isc_commandline_argument;
			break;

		case 't':
			endp = NULL;
			ttl = strtol(isc_commandline_argument, &endp, 0);
			if (*endp != '\0')
				fatal("TTL must be numeric");
			break;

		case 'r':
			setup_entropy(mctx, isc_commandline_argument, &ectx);
			break;

		case 'v':
			endp = NULL;
			verbose = strtol(isc_commandline_argument, &endp, 0);
			if (*endp != '\0')
				fatal("verbose level must be numeric");
			break;

		case 'p':
			pseudorandom = ISC_TRUE;
			break;

		case 'h':
		default:
			usage();

		}
	}

	argc -= isc_commandline_index;
	argv += isc_commandline_index;

	if (argc < 1)
		usage();

	if (ectx == NULL)
		setup_entropy(mctx, NULL, &ectx);
	eflags = ISC_ENTROPY_BLOCKING;
	if (!pseudorandom)
		eflags |= ISC_ENTROPY_GOODONLY;
	result = dst_lib_init(mctx, ectx, eflags);
	if (result != ISC_R_SUCCESS)
		fatal("could not initialize dst: %s", 
		      isc_result_totext(result));

	isc_stdtime_get(&now);

	if (startstr != NULL)
		starttime = strtotime(startstr, now, now);
	else
		starttime = now;

	if (endstr != NULL)
		endtime = strtotime(endstr, now, starttime);
	else
		endtime = starttime + (30 * 24 * 60 * 60);

	if (ttl == -1) {
		ttl = 3600;
		fprintf(stderr, "%s: TTL not specified, assuming 3600\n",
			program);
	}

	setup_logging(verbose, mctx, &log);

	dns_diff_init(mctx, &diff);
	rdclass = 0;

	ISC_LIST_INIT(keylist);

	for (i = 0; i < argc; i++) {
		char namestr[DNS_NAME_FORMATSIZE];
		isc_buffer_t namebuf;

		key = NULL;
		result = dst_key_fromnamedfile(argv[i], DST_TYPE_PUBLIC,
					       mctx, &key);
		if (result != ISC_R_SUCCESS)
			fatal("error loading key from %s: %s", argv[i],
			      isc_result_totext(result));
		if (rdclass == 0)
			rdclass = dst_key_class(key);

		isc_buffer_init(&namebuf, namestr, sizeof(namestr));
		result = dns_name_tofilenametext(dst_key_name(key),
						 ISC_FALSE,
						 &namebuf);
		check_result(result, "dns_name_tofilenametext");
		isc_buffer_putuint8(&namebuf, 0);

		if (domain == NULL) {
			dns_fixedname_init(&fdomain);
			domain = dns_fixedname_name(&fdomain);
			dns_name_copy(dst_key_name(key), domain, NULL);
		} else if (!dns_name_equal(domain, dst_key_name(key))) {
			char str[DNS_NAME_FORMATSIZE];
			dns_name_format(domain, str, sizeof(str));
			fatal("all keys must have the same owner - %s "
			      "and %s do not match", str, namestr);
		}

		if (output == NULL) {
			output = isc_mem_allocate(mctx,
						  strlen("keyset-") +
						  strlen(namestr) + 1);
			if (output == NULL)
				fatal("out of memory");
			sprintf(output, "keyset-%s", namestr);
		}

		if (dst_key_iszonekey(key)) {
			dst_key_t *zonekey = NULL;
			result = dst_key_fromnamedfile(argv[i],
						       DST_TYPE_PUBLIC |
						       DST_TYPE_PRIVATE,
						       mctx, &zonekey);
			if (result != ISC_R_SUCCESS)
				fatal("failed to read private key %s: %s",
				      argv[i], isc_result_totext(result));
			if (!zonekey_on_list(zonekey)) {
				keynode = isc_mem_get(mctx, sizeof(keynode_t));
				if (keynode == NULL)
					fatal("out of memory");
				keynode->key = zonekey;
				ISC_LIST_INITANDAPPEND(keylist, keynode, link);
			} else
				dst_key_free(&zonekey);
		}
		dns_rdata_reset(&rdata);
		isc_buffer_init(&b, data, sizeof(data));
		result = dst_key_todns(key, &b);
		dst_key_free(&key);
		if (result != ISC_R_SUCCESS)
			fatal("failed to convert key %s to a DNS KEY: %s",
			      argv[i], isc_result_totext(result));
		isc_buffer_usedregion(&b, &r);
		dns_rdata_fromregion(&rdata, rdclass, dns_rdatatype_dnskey, &r);
		tuple = NULL;
		result = dns_difftuple_create(mctx, DNS_DIFFOP_ADD,
					      domain, ttl, &rdata, &tuple);
		check_result(result, "dns_difftuple_create");
		dns_diff_append(&diff, &tuple);
	}

	db = NULL;
	result = dns_db_create(mctx, "rbt", dns_rootname, dns_dbtype_zone,
			       rdclass, 0, NULL, &db);
	if (result != ISC_R_SUCCESS)
		fatal("failed to create a database");

	version = NULL;
	dns_db_newversion(db, &version);

	result = dns_diff_apply(&diff, db, version);
	check_result(result, "dns_diff_apply");
	dns_diff_clear(&diff);

	dns_fixedname_init(&tname);
	dns_rdataset_init(&rdataset);
	result = dns_db_find(db, domain, version, dns_rdatatype_dnskey, 0, 0,
			     NULL, dns_fixedname_name(&tname), &rdataset,
			     NULL);
	check_result(result, "dns_db_find");

	if (ISC_LIST_EMPTY(keylist))
		fprintf(stderr,
			"%s: no private zone key found; not self-signing\n",
			program);
	for (keynode = ISC_LIST_HEAD(keylist);
	     keynode != NULL;
	     keynode = ISC_LIST_NEXT(keynode, link))
	{
		dns_rdata_reset(&rdata);
		isc_buffer_init(&b, data, sizeof(data));
		result = dns_dnssec_sign(domain, &rdataset, keynode->key,
					 &starttime, &endtime, mctx, &b,
					 &rdata);
		isc_entropy_stopcallbacksources(ectx);
		if (result != ISC_R_SUCCESS) {
			char keystr[KEY_FORMATSIZE];
			key_format(keynode->key, keystr, sizeof(keystr));
			fatal("failed to sign keyset with key %s: %s",
			      keystr, isc_result_totext(result));
		}
		if (tryverify) {
			result = dns_dnssec_verify(domain, &rdataset,
						   keynode->key, ISC_TRUE,
						   mctx, &rdata);
			if (result != ISC_R_SUCCESS) {
				char keystr[KEY_FORMATSIZE];
				key_format(keynode->key, keystr, sizeof(keystr));
				fatal("signature from key '%s' failed to "
				      "verify: %s",
				      keystr, isc_result_totext(result));
			}
		}
		tuple = NULL;
		result = dns_difftuple_create(mctx, DNS_DIFFOP_ADD,
					      domain, ttl, &rdata, &tuple);
		check_result(result, "dns_difftuple_create");
		dns_diff_append(&diff, &tuple);
	}

	result = dns_diff_apply(&diff, db, version);
	check_result(result, "dns_diff_apply");
	dns_diff_clear(&diff);

	dns_rdataset_disassociate(&rdataset);

	dns_db_closeversion(db, &version, ISC_TRUE);
	result = dns_db_dump(db, version, output);
	if (result != ISC_R_SUCCESS) {
		char domainstr[DNS_NAME_FORMATSIZE];
		dns_name_format(domain, domainstr, sizeof(domainstr));
		fatal("failed to write database for %s to %s",
		      domainstr, output);
	}

	printf("%s\n", output);

	dns_db_detach(&db);

	while (!ISC_LIST_EMPTY(keylist)) {
		keynode = ISC_LIST_HEAD(keylist);
		ISC_LIST_UNLINK(keylist, keynode, link);
		dst_key_free(&keynode->key);
		isc_mem_put(mctx, keynode, sizeof(keynode_t));
	}

	cleanup_logging(&log);
	cleanup_entropy(&ectx);

	isc_mem_free(mctx, output);
	dst_lib_destroy();
	if (verbose > 10)
		isc_mem_stats(mctx, stdout);
	isc_mem_destroy(&mctx);
	return (0);
}
