/*
 * Copyright (C) 2008-2011  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: dnssec-dsfromkey.c,v 1.19.14.2 2011/09/05 23:45:53 tbox Exp $ */

/*! \file */

#include <config.h>

#include <stdlib.h>

#include <isc/buffer.h>
#include <isc/commandline.h>
#include <isc/entropy.h>
#include <isc/hash.h>
#include <isc/mem.h>
#include <isc/print.h>
#include <isc/string.h>
#include <isc/util.h>

#include <dns/db.h>
#include <dns/dbiterator.h>
#include <dns/ds.h>
#include <dns/fixedname.h>
#include <dns/log.h>
#include <dns/keyvalues.h>
#include <dns/master.h>
#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/rdataclass.h>
#include <dns/rdataset.h>
#include <dns/rdatasetiter.h>
#include <dns/rdatatype.h>
#include <dns/result.h>

#include <dst/dst.h>

#include "dnssectool.h"

#ifndef PATH_MAX
#define PATH_MAX 1024   /* AIX, WIN32, and others don't define this. */
#endif

const char *program = "dnssec-dsfromkey";
int verbose;

static dns_rdataclass_t rdclass;
static dns_fixedname_t	fixed;
static dns_name_t	*name = NULL;
static isc_mem_t	*mctx = NULL;

static isc_result_t
initname(char *setname) {
	isc_result_t result;
	isc_buffer_t buf;

	dns_fixedname_init(&fixed);
	name = dns_fixedname_name(&fixed);

	isc_buffer_init(&buf, setname, strlen(setname));
	isc_buffer_add(&buf, strlen(setname));
	result = dns_name_fromtext(name, &buf, dns_rootname, 0, NULL);
	return (result);
}

static isc_result_t
loadsetfromfile(char *filename, dns_rdataset_t *rdataset) {
	isc_result_t	 result;
	dns_db_t	 *db = NULL;
	dns_dbnode_t	 *node = NULL;
	char setname[DNS_NAME_FORMATSIZE];

	dns_name_format(name, setname, sizeof(setname));

	result = dns_db_create(mctx, "rbt", name, dns_dbtype_zone,
			       rdclass, 0, NULL, &db);
	if (result != ISC_R_SUCCESS)
		fatal("can't create database");

	result = dns_db_load(db, filename);
	if (result != ISC_R_SUCCESS && result != DNS_R_SEENINCLUDE)
		fatal("can't load %s: %s", filename, isc_result_totext(result));

	result = dns_db_findnode(db, name, ISC_FALSE, &node);
	if (result != ISC_R_SUCCESS)
		fatal("can't find %s node in %s", setname, filename);

	result = dns_db_findrdataset(db, node, NULL, dns_rdatatype_dnskey,
				     0, 0, rdataset, NULL);

	if (result == ISC_R_NOTFOUND)
		fatal("no DNSKEY RR for %s in %s", setname, filename);
	else if (result != ISC_R_SUCCESS)
		fatal("dns_db_findrdataset");

	if (node != NULL)
		dns_db_detachnode(db, &node);
	if (db != NULL)
		dns_db_detach(&db);
	return (result);
}

static isc_result_t
loadkeyset(char *dirname, dns_rdataset_t *rdataset) {
	isc_result_t	 result;
	char		 filename[PATH_MAX + 1];
	isc_buffer_t	 buf;

	dns_rdataset_init(rdataset);

	isc_buffer_init(&buf, filename, sizeof(filename));
	if (dirname != NULL) {
		/* allow room for a trailing slash */
		if (strlen(dirname) >= isc_buffer_availablelength(&buf))
			return (ISC_R_NOSPACE);
		isc_buffer_putstr(&buf, dirname);
		if (dirname[strlen(dirname) - 1] != '/')
			isc_buffer_putstr(&buf, "/");
	}

	if (isc_buffer_availablelength(&buf) < 7)
		return (ISC_R_NOSPACE);
	isc_buffer_putstr(&buf, "keyset-");

	result = dns_name_tofilenametext(name, ISC_FALSE, &buf);
	check_result(result, "dns_name_tofilenametext()");
	if (isc_buffer_availablelength(&buf) == 0)
		return (ISC_R_NOSPACE);
	isc_buffer_putuint8(&buf, 0);

	return (loadsetfromfile(filename, rdataset));
}

static void
loadkey(char *filename, unsigned char *key_buf, unsigned int key_buf_size,
	dns_rdata_t *rdata)
{
	isc_result_t  result;
	dst_key_t     *key = NULL;
	isc_buffer_t  keyb;
	isc_region_t  r;

	dns_rdata_init(rdata);

	isc_buffer_init(&keyb, key_buf, key_buf_size);

	result = dst_key_fromnamedfile(filename, NULL, DST_TYPE_PUBLIC,
				       mctx, &key);
	if (result != ISC_R_SUCCESS)
		fatal("invalid keyfile name %s: %s",
		      filename, isc_result_totext(result));

	if (verbose > 2) {
		char keystr[DST_KEY_FORMATSIZE];

		dst_key_format(key, keystr, sizeof(keystr));
		fprintf(stderr, "%s: %s\n", program, keystr);
	}

	result = dst_key_todns(key, &keyb);
	if (result != ISC_R_SUCCESS)
		fatal("can't decode key");

	isc_buffer_usedregion(&keyb, &r);
	dns_rdata_fromregion(rdata, dst_key_class(key),
			     dns_rdatatype_dnskey, &r);

	rdclass = dst_key_class(key);

	dns_fixedname_init(&fixed);
	name = dns_fixedname_name(&fixed);
	result = dns_name_copy(dst_key_name(key), name, NULL);
	if (result != ISC_R_SUCCESS)
		fatal("can't copy name");

	dst_key_free(&key);
}

static void
logkey(dns_rdata_t *rdata)
{
	isc_result_t result;
	dst_key_t    *key = NULL;
	isc_buffer_t buf;
	char	     keystr[DST_KEY_FORMATSIZE];

	isc_buffer_init(&buf, rdata->data, rdata->length);
	isc_buffer_add(&buf, rdata->length);
	result = dst_key_fromdns(name, rdclass, &buf, mctx, &key);
	if (result != ISC_R_SUCCESS)
		return;

	dst_key_format(key, keystr, sizeof(keystr));
	fprintf(stderr, "%s: %s\n", program, keystr);

	dst_key_free(&key);
}

static void
emit(unsigned int dtype, isc_boolean_t showall, char *lookaside,
     dns_rdata_t *rdata)
{
	isc_result_t result;
	unsigned char buf[DNS_DS_BUFFERSIZE];
	char text_buf[DST_KEY_MAXTEXTSIZE];
	char name_buf[DNS_NAME_MAXWIRE];
	char class_buf[10];
	isc_buffer_t textb, nameb, classb;
	isc_region_t r;
	dns_rdata_t ds;
	dns_rdata_dnskey_t dnskey;

	isc_buffer_init(&textb, text_buf, sizeof(text_buf));
	isc_buffer_init(&nameb, name_buf, sizeof(name_buf));
	isc_buffer_init(&classb, class_buf, sizeof(class_buf));

	dns_rdata_init(&ds);

	result = dns_rdata_tostruct(rdata, &dnskey, NULL);
	if (result != ISC_R_SUCCESS)
		fatal("can't convert DNSKEY");

	if ((dnskey.flags & DNS_KEYFLAG_KSK) == 0 && !showall)
		return;

	result = dns_ds_buildrdata(name, rdata, dtype, buf, &ds);
	if (result != ISC_R_SUCCESS)
		fatal("can't build record");

	result = dns_name_totext(name, ISC_FALSE, &nameb);
	if (result != ISC_R_SUCCESS)
		fatal("can't print name");

	/* Add lookaside origin, if set */
	if (lookaside != NULL) {
		if (isc_buffer_availablelength(&nameb) < strlen(lookaside))
			fatal("DLV origin '%s' is too long", lookaside);
		isc_buffer_putstr(&nameb, lookaside);
		if (lookaside[strlen(lookaside) - 1] != '.') {
			if (isc_buffer_availablelength(&nameb) < 1)
				fatal("DLV origin '%s' is too long", lookaside);
			isc_buffer_putstr(&nameb, ".");
		}
	}

	result = dns_rdata_totext(&ds, (dns_name_t *) NULL, &textb);
	if (result != ISC_R_SUCCESS)
		fatal("can't print rdata");

	result = dns_rdataclass_totext(rdclass, &classb);
	if (result != ISC_R_SUCCESS)
		fatal("can't print class");

	isc_buffer_usedregion(&nameb, &r);
	printf("%.*s ", (int)r.length, r.base);

	isc_buffer_usedregion(&classb, &r);
	printf("%.*s", (int)r.length, r.base);

	if (lookaside == NULL)
		printf(" DS ");
	else
		printf(" DLV ");

	isc_buffer_usedregion(&textb, &r);
	printf("%.*s\n", (int)r.length, r.base);
}

ISC_PLATFORM_NORETURN_PRE static void
usage(void) ISC_PLATFORM_NORETURN_POST;

static void
usage(void) {
	fprintf(stderr, "Usage:\n");
	fprintf(stderr,	"    %s options [-K dir] keyfile\n\n", program);
	fprintf(stderr, "    %s options [-K dir] [-c class] -s dnsname\n\n",
		program);
	fprintf(stderr, "    %s options -f zonefile (as zone name)\n\n", program);
	fprintf(stderr, "    %s options -f zonefile zonename\n\n", program);
	fprintf(stderr, "Version: %s\n", VERSION);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "    -v <verbose level>\n");
	fprintf(stderr, "    -K <directory>: directory in which to find "
			"key file or keyset file\n");
	fprintf(stderr, "    -a algorithm: digest algorithm "
			"(SHA-1, SHA-256 or GOST)\n");
	fprintf(stderr, "    -1: use SHA-1\n");
	fprintf(stderr, "    -2: use SHA-256\n");
	fprintf(stderr, "    -l: add lookaside zone and print DLV records\n");
	fprintf(stderr, "    -s: read keyset from keyset-<dnsname> file\n");
	fprintf(stderr, "    -c class: rdata class for DS set (default: IN)\n");
	fprintf(stderr, "    -f file: read keyset from zone file\n");
	fprintf(stderr, "    -A: when used with -f, "
			"include all keys in DS set, not just KSKs\n");
	fprintf(stderr, "Output: DS or DLV RRs\n");

	exit (-1);
}

int
main(int argc, char **argv) {
	char		*algname = NULL, *classname = NULL;
	char		*filename = NULL, *dir = NULL, *namestr;
	char		*lookaside = NULL;
	char		*endp;
	int		ch;
	unsigned int	dtype = DNS_DSDIGEST_SHA1;
	isc_boolean_t	both = ISC_TRUE;
	isc_boolean_t	usekeyset = ISC_FALSE;
	isc_boolean_t	showall = ISC_FALSE;
	isc_result_t	result;
	isc_log_t	*log = NULL;
	isc_entropy_t	*ectx = NULL;
	dns_rdataset_t	rdataset;
	dns_rdata_t	rdata;

	dns_rdata_init(&rdata);

	if (argc == 1)
		usage();

	result = isc_mem_create(0, 0, &mctx);
	if (result != ISC_R_SUCCESS)
		fatal("out of memory");

	dns_result_register();

	isc_commandline_errprint = ISC_FALSE;

	while ((ch = isc_commandline_parse(argc, argv,
					   "12Aa:c:d:Ff:K:l:sv:h")) != -1) {
		switch (ch) {
		case '1':
			dtype = DNS_DSDIGEST_SHA1;
			both = ISC_FALSE;
			break;
		case '2':
			dtype = DNS_DSDIGEST_SHA256;
			both = ISC_FALSE;
			break;
		case 'A':
			showall = ISC_TRUE;
			break;
		case 'a':
			algname = isc_commandline_argument;
			both = ISC_FALSE;
			break;
		case 'c':
			classname = isc_commandline_argument;
			break;
		case 'd':
			fprintf(stderr, "%s: the -d option is deprecated; "
					"use -K\n", program);
			/* fall through */
		case 'K':
			dir = isc_commandline_argument;
			if (strlen(dir) == 0U)
				fatal("directory must be non-empty string");
			break;
		case 'f':
			filename = isc_commandline_argument;
			break;
		case 'l':
			lookaside = isc_commandline_argument;
			if (strlen(lookaside) == 0U)
				fatal("lookaside must be a non-empty string");
			break;
		case 's':
			usekeyset = ISC_TRUE;
			break;
		case 'v':
			verbose = strtol(isc_commandline_argument, &endp, 0);
			if (*endp != '\0')
				fatal("-v must be followed by a number");
			break;
		case 'F':
			/* Reserved for FIPS mode */
			/* FALLTHROUGH */
		case '?':
			if (isc_commandline_option != '?')
				fprintf(stderr, "%s: invalid argument -%c\n",
					program, isc_commandline_option);
			/* FALLTHROUGH */
		case 'h':
			usage();

		default:
			fprintf(stderr, "%s: unhandled option -%c\n",
				program, isc_commandline_option);
			exit(1);
		}
	}

	if (algname != NULL) {
		if (strcasecmp(algname, "SHA1") == 0 ||
		    strcasecmp(algname, "SHA-1") == 0)
			dtype = DNS_DSDIGEST_SHA1;
		else if (strcasecmp(algname, "SHA256") == 0 ||
			 strcasecmp(algname, "SHA-256") == 0)
			dtype = DNS_DSDIGEST_SHA256;
#ifdef HAVE_OPENSSL_GOST
		else if (strcasecmp(algname, "GOST") == 0)
			dtype = DNS_DSDIGEST_GOST;
#endif
		else
			fatal("unknown algorithm %s", algname);
	}

	rdclass = strtoclass(classname);

	if (usekeyset && filename != NULL)
		fatal("cannot use both -s and -f");

	/* When not using -f, -A is implicit */
	if (filename == NULL)
		showall = ISC_TRUE;

	if (argc < isc_commandline_index + 1 && filename == NULL)
		fatal("the key file name was not specified");
	if (argc > isc_commandline_index + 1)
		fatal("extraneous arguments");

	if (ectx == NULL)
		setup_entropy(mctx, NULL, &ectx);
	result = isc_hash_create(mctx, ectx, DNS_NAME_MAXWIRE);
	if (result != ISC_R_SUCCESS)
		fatal("could not initialize hash");
	result = dst_lib_init(mctx, ectx,
			      ISC_ENTROPY_BLOCKING | ISC_ENTROPY_GOODONLY);
	if (result != ISC_R_SUCCESS)
		fatal("could not initialize dst: %s",
		      isc_result_totext(result));
	isc_entropy_stopcallbacksources(ectx);

	setup_logging(verbose, mctx, &log);

	dns_rdataset_init(&rdataset);

	if (usekeyset || filename != NULL) {
		if (argc < isc_commandline_index + 1 && filename != NULL) {
			/* using zone name as the zone file name */
			namestr = filename;
		} else
			namestr = argv[isc_commandline_index];

		result = initname(namestr);
		if (result != ISC_R_SUCCESS)
			fatal("could not initialize name %s", namestr);

		if (usekeyset)
			result = loadkeyset(dir, &rdataset);
		else
			result = loadsetfromfile(filename, &rdataset);

		if (result != ISC_R_SUCCESS)
			fatal("could not load DNSKEY set: %s\n",
			      isc_result_totext(result));

		for (result = dns_rdataset_first(&rdataset);
		     result == ISC_R_SUCCESS;
		     result = dns_rdataset_next(&rdataset)) {
			dns_rdata_init(&rdata);
			dns_rdataset_current(&rdataset, &rdata);

			if (verbose > 2)
				logkey(&rdata);

			if (both) {
				emit(DNS_DSDIGEST_SHA1, showall, lookaside,
				     &rdata);
				emit(DNS_DSDIGEST_SHA256, showall, lookaside,
				     &rdata);
			} else
				emit(dtype, showall, lookaside, &rdata);
		}
	} else {
		unsigned char key_buf[DST_KEY_MAXSIZE];

		loadkey(argv[isc_commandline_index], key_buf,
			DST_KEY_MAXSIZE, &rdata);

		if (both) {
			emit(DNS_DSDIGEST_SHA1, showall, lookaside, &rdata);
			emit(DNS_DSDIGEST_SHA256, showall, lookaside, &rdata);
		} else
			emit(dtype, showall, lookaside, &rdata);
	}

	if (dns_rdataset_isassociated(&rdataset))
		dns_rdataset_disassociate(&rdataset);
	cleanup_logging(&log);
	dst_lib_destroy();
	isc_hash_destroy();
	cleanup_entropy(&ectx);
	dns_name_destroy();
	if (verbose > 10)
		isc_mem_stats(mctx, stdout);
	isc_mem_destroy(&mctx);

	fflush(stdout);
	if (ferror(stdout)) {
		fprintf(stderr, "write error\n");
		return (1);
	} else
		return (0);
}
