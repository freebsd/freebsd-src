/*
 * Copyright (C) 2008-2010  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: dnssec-dsfromkey.c,v 1.2.14.6 2010-01-11 23:47:22 tbox Exp $ */

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
#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/rdataclass.h>
#include <dns/rdataset.h>
#include <dns/rdatasetiter.h>
#include <dns/rdatatype.h>
#include <dns/result.h>

#include <dst/dst.h>

#include "dnssectool.h"

const char *program = "dnssec-dsfromkey";
int verbose;

static dns_rdataclass_t rdclass;
static dns_fixedname_t  fixed;
static dns_name_t       *name = NULL;
static dns_db_t         *db = NULL;
static dns_dbnode_t     *node = NULL;
static dns_rdataset_t   keyset;
static isc_mem_t        *mctx = NULL;

static void
loadkeys(char *dirname, char *setname)
{
	isc_result_t     result;
	char             filename[1024];
	isc_buffer_t     buf;

	dns_rdataset_init(&keyset);
	dns_fixedname_init(&fixed);
	name = dns_fixedname_name(&fixed);

	isc_buffer_init(&buf, setname, strlen(setname));
	isc_buffer_add(&buf, strlen(setname));
	result = dns_name_fromtext(name, &buf, dns_rootname, ISC_FALSE, NULL);
	if (result != ISC_R_SUCCESS)
		fatal("can't convert DNS name %s", setname);

	isc_buffer_init(&buf, filename, sizeof(filename));
	if (dirname != NULL) {
		if (isc_buffer_availablelength(&buf) < strlen(dirname))
			fatal("directory name '%s' too long", dirname);
		isc_buffer_putstr(&buf, dirname);
		if (dirname[strlen(dirname) - 1] != '/') {
			if (isc_buffer_availablelength(&buf) < 1)
				fatal("directory name '%s' too long", dirname);
			isc_buffer_putstr(&buf, "/");
		}
	}

	if (isc_buffer_availablelength(&buf) < strlen("keyset-"))
		fatal("directory name '%s' too long", dirname);
	isc_buffer_putstr(&buf, "keyset-");
	result = dns_name_tofilenametext(name, ISC_FALSE, &buf);
	check_result(result, "dns_name_tofilenametext()");
	if (isc_buffer_availablelength(&buf) == 0)
		fatal("name %s too long", setname);
	isc_buffer_putuint8(&buf, 0);

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
				     0, 0, &keyset, NULL);
	if (result == ISC_R_NOTFOUND)
		fatal("no DNSKEY RR for %s in %s", setname, filename);
	else if (result != ISC_R_SUCCESS)
		fatal("dns_db_findrdataset");
}

static void
loadkey(char *filename, unsigned char *key_buf, unsigned int key_buf_size,
	dns_rdata_t *rdata)
{
	isc_result_t  result;
	dst_key_t     *key = NULL;
	isc_buffer_t  keyb;
	isc_region_t  r;

	dns_rdataset_init(&keyset);
	dns_rdata_init(rdata);

	isc_buffer_init(&keyb, key_buf, key_buf_size);

	result = dst_key_fromnamedfile(filename, DST_TYPE_PUBLIC, mctx, &key);
	if (result != ISC_R_SUCCESS)
		fatal("invalid keyfile name %s: %s",
		      filename, isc_result_totext(result));

	if (verbose > 2) {
		char keystr[KEY_FORMATSIZE];

		key_format(key, keystr, sizeof(keystr));
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
	char         keystr[KEY_FORMATSIZE];

	isc_buffer_init(&buf, rdata->data, rdata->length);
	isc_buffer_add(&buf, rdata->length);
	result = dst_key_fromdns(name, rdclass, &buf, mctx, &key);
	if (result != ISC_R_SUCCESS)
		return;

	key_format(key, keystr, sizeof(keystr));
	fprintf(stderr, "%s: %s\n", program, keystr);

	dst_key_free(&key);
}

static void
emitds(unsigned int dtype, dns_rdata_t *rdata)
{
	isc_result_t   result;
	unsigned char  buf[DNS_DS_BUFFERSIZE];
	char           text_buf[DST_KEY_MAXTEXTSIZE];
	char           class_buf[10];
	isc_buffer_t   textb, classb;
	isc_region_t   r;
	dns_rdata_t    ds;

	isc_buffer_init(&textb, text_buf, sizeof(text_buf));
	isc_buffer_init(&classb, class_buf, sizeof(class_buf));

	dns_rdata_init(&ds);

	result = dns_ds_buildrdata(name, rdata, dtype, buf, &ds);
	if (result != ISC_R_SUCCESS)
		fatal("can't build DS");

	result = dns_rdata_totext(&ds, (dns_name_t *) NULL, &textb);
	if (result != ISC_R_SUCCESS)
		fatal("can't print DS rdata");

	result = dns_rdataclass_totext(rdclass, &classb);
	if (result != ISC_R_SUCCESS)
		fatal("can't print DS class");

	result = dns_name_print(name, stdout);
	if (result != ISC_R_SUCCESS)
		fatal("can't print DS name");

	putchar(' ');

	isc_buffer_usedregion(&classb, &r);
	isc_util_fwrite(r.base, 1, r.length, stdout);

	printf(" DS ");

	isc_buffer_usedregion(&textb, &r);
	isc_util_fwrite(r.base, 1, r.length, stdout);
	putchar('\n');
}

static void
usage(void) {
	fprintf(stderr, "Usage:\n");
	fprintf(stderr,	"    %s options keyfile\n\n", program);
	fprintf(stderr, "    %s options [-c class] [-d dir] -s dnsname\n\n",
		program);
	fprintf(stderr, "Version: %s\n", VERSION);
	fprintf(stderr, "Options:\n");
	fprintf(stderr, "    -v <verbose level>\n");
	fprintf(stderr, "    -1: use SHA-1\n");
	fprintf(stderr, "    -2: use SHA-256\n");
	fprintf(stderr, "    -a algorithm: use algorithm\n");
	fprintf(stderr, "Keyset options:\n");
	fprintf(stderr, "    -s: keyset mode\n");
	fprintf(stderr, "    -c class\n");
	fprintf(stderr, "    -d directory\n");
	fprintf(stderr, "Output: DS RRs\n");

	exit (-1);
}

int
main(int argc, char **argv) {
	char           *algname = NULL, *classname = NULL, *dirname = NULL;
	char           *endp;
	int            ch;
	unsigned int   dtype = DNS_DSDIGEST_SHA1;
	isc_boolean_t  both = ISC_TRUE;
	isc_boolean_t  usekeyset = ISC_FALSE;
	isc_result_t   result;
	isc_log_t      *log = NULL;
	isc_entropy_t  *ectx = NULL;
	dns_rdata_t    rdata;

	dns_rdata_init(&rdata);

	if (argc == 1)
		usage();

	result = isc_mem_create(0, 0, &mctx);
	if (result != ISC_R_SUCCESS)
		fatal("out of memory");

	dns_result_register();

	isc_commandline_errprint = ISC_FALSE;

	while ((ch = isc_commandline_parse(argc, argv,
					   "12a:c:d:sv:h")) != -1) {
		switch (ch) {
		case '1':
			dtype = DNS_DSDIGEST_SHA1;
			both = ISC_FALSE;
			break;
		case '2':
			dtype = DNS_DSDIGEST_SHA256;
			both = ISC_FALSE;
			break;
		case 'a':
			algname = isc_commandline_argument;
			both = ISC_FALSE;
			break;
		case 'c':
			classname = isc_commandline_argument;
			break;
		case 'd':
			dirname = isc_commandline_argument;
			break;
		case 's':
			usekeyset = ISC_TRUE;
			break;
		case 'v':
			verbose = strtol(isc_commandline_argument, &endp, 0);
			if (*endp != '\0')
				fatal("-v must be followed by a number");
			break;
		case '?':
			if (isc_commandline_option != '?')
				fprintf(stderr, "%s: invalid argument -%c\n",
					program, isc_commandline_option);
			/* Falls into */
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
		else
			fatal("unknown algorithm %s", algname);
	}

	rdclass = strtoclass(classname);

	if (argc < isc_commandline_index + 1)
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
		fatal("could not initialize dst");
	isc_entropy_stopcallbacksources(ectx);

	setup_logging(verbose, mctx, &log);

	if (usekeyset) {
		loadkeys(dirname, argv[isc_commandline_index]);

		for (result = dns_rdataset_first(&keyset);
		     result == ISC_R_SUCCESS;
		     result = dns_rdataset_next(&keyset)) {
			dns_rdata_init(&rdata);
			dns_rdataset_current(&keyset, &rdata);

			if (verbose > 2)
				logkey(&rdata);

			if (both) {
				emitds(DNS_DSDIGEST_SHA1, &rdata);
				emitds(DNS_DSDIGEST_SHA256, &rdata);
			} else
				emitds(dtype, &rdata);
		}
	} else {
		unsigned char key_buf[DST_KEY_MAXSIZE];

		loadkey(argv[isc_commandline_index], key_buf,
			DST_KEY_MAXSIZE, &rdata);

		if (both) {
			emitds(DNS_DSDIGEST_SHA1, &rdata);
			emitds(DNS_DSDIGEST_SHA256, &rdata);
		} else
			emitds(dtype, &rdata);
	}

	if (dns_rdataset_isassociated(&keyset))
		dns_rdataset_disassociate(&keyset);
	if (node != NULL)
		dns_db_detachnode(db, &node);
	if (db != NULL)
		dns_db_detach(&db);
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
