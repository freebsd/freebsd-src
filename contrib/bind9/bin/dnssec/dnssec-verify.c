/*
 * Copyright (C) 2012  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: dnssec-verify.c,v 1.1.2.1 2011/03/16 06:37:51 each Exp $ */

/*! \file */

#include <config.h>

#include <stdlib.h>
#include <time.h>

#include <isc/app.h>
#include <isc/base32.h>
#include <isc/commandline.h>
#include <isc/entropy.h>
#include <isc/event.h>
#include <isc/file.h>
#include <isc/hash.h>
#include <isc/hex.h>
#include <isc/mem.h>
#include <isc/mutex.h>
#include <isc/os.h>
#include <isc/print.h>
#include <isc/random.h>
#include <isc/rwlock.h>
#include <isc/serial.h>
#include <isc/stdio.h>
#include <isc/stdlib.h>
#include <isc/string.h>
#include <isc/time.h>
#include <isc/util.h>

#include <dns/db.h>
#include <dns/dbiterator.h>
#include <dns/diff.h>
#include <dns/dnssec.h>
#include <dns/ds.h>
#include <dns/fixedname.h>
#include <dns/keyvalues.h>
#include <dns/log.h>
#include <dns/master.h>
#include <dns/masterdump.h>
#include <dns/nsec.h>
#include <dns/nsec3.h>
#include <dns/rdata.h>
#include <dns/rdatalist.h>
#include <dns/rdataset.h>
#include <dns/rdataclass.h>
#include <dns/rdatasetiter.h>
#include <dns/rdatastruct.h>
#include <dns/rdatatype.h>
#include <dns/result.h>
#include <dns/soa.h>
#include <dns/time.h>

#include <dst/dst.h>

#include "dnssectool.h"

const char *program = "dnssec-verify";
int verbose;

static isc_stdtime_t now;
static isc_mem_t *mctx = NULL;
static isc_entropy_t *ectx = NULL;
static dns_masterformat_t inputformat = dns_masterformat_text;
static dns_db_t *gdb;			/* The database */
static dns_dbversion_t *gversion;	/* The database version */
static dns_rdataclass_t gclass;		/* The class */
static dns_name_t *gorigin;		/* The database origin */
static isc_boolean_t ignore_kskflag = ISC_FALSE;
static isc_boolean_t keyset_kskonly = ISC_FALSE;

/*%
 * Load the zone file from disk
 */
static void
loadzone(char *file, char *origin, dns_rdataclass_t rdclass, dns_db_t **db) {
	isc_buffer_t b;
	int len;
	dns_fixedname_t fname;
	dns_name_t *name;
	isc_result_t result;

	len = strlen(origin);
	isc_buffer_init(&b, origin, len);
	isc_buffer_add(&b, len);

	dns_fixedname_init(&fname);
	name = dns_fixedname_name(&fname);
	result = dns_name_fromtext(name, &b, dns_rootname, 0, NULL);
	if (result != ISC_R_SUCCESS)
		fatal("failed converting name '%s' to dns format: %s",
		      origin, isc_result_totext(result));

	result = dns_db_create(mctx, "rbt", name, dns_dbtype_zone,
			       rdclass, 0, NULL, db);
	check_result(result, "dns_db_create()");

	result = dns_db_load2(*db, file, inputformat);
	if (result != ISC_R_SUCCESS && result != DNS_R_SEENINCLUDE)
		fatal("failed loading zone from '%s': %s",
		      file, isc_result_totext(result));
}

ISC_PLATFORM_NORETURN_PRE static void
usage(void) ISC_PLATFORM_NORETURN_POST;

static void
usage(void) {
	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "\t%s [options] zonefile [keys]\n", program);

	fprintf(stderr, "\n");

	fprintf(stderr, "Version: %s\n", VERSION);

	fprintf(stderr, "Options: (default value in parenthesis) \n");
	fprintf(stderr, "\t-v debuglevel (0)\n");
	fprintf(stderr, "\t-o origin:\n");
	fprintf(stderr, "\t\tzone origin (name of zonefile)\n");
	fprintf(stderr, "\t-I format:\n");
	fprintf(stderr, "\t\tfile format of input zonefile (text)\n");
	fprintf(stderr, "\t-c class (IN)\n");
	fprintf(stderr, "\t-E engine:\n");
#ifdef USE_PKCS11
	fprintf(stderr, "\t\tname of an OpenSSL engine to use "
				"(default is \"pkcs11\")\n");
#else
	fprintf(stderr, "\t\tname of an OpenSSL engine to use\n");
#endif
	fprintf(stderr, "\t-x:\tDNSKEY record signed with KSKs only, "
			"not ZSKs\n");
	fprintf(stderr, "\t-z:\tAll records signed with KSKs\n");
	exit(0);
}

int
main(int argc, char *argv[]) {
	char *origin = NULL, *file = NULL;
	char *inputformatstr = NULL;
	isc_result_t result;
	isc_log_t *log = NULL;
#ifdef USE_PKCS11
	const char *engine = "pkcs11";
#else
	const char *engine = NULL;
#endif
	char *classname = NULL;
	dns_rdataclass_t rdclass;
	char ch, *endp;

#define CMDLINE_FLAGS \
	"m:o:I:c:E:v:xz"

	/*
	 * Process memory debugging argument first.
	 */
	while ((ch = isc_commandline_parse(argc, argv, CMDLINE_FLAGS)) != -1) {
		switch (ch) {
		case 'm':
			if (strcasecmp(isc_commandline_argument, "record") == 0)
				isc_mem_debugging |= ISC_MEM_DEBUGRECORD;
			if (strcasecmp(isc_commandline_argument, "trace") == 0)
				isc_mem_debugging |= ISC_MEM_DEBUGTRACE;
			if (strcasecmp(isc_commandline_argument, "usage") == 0)
				isc_mem_debugging |= ISC_MEM_DEBUGUSAGE;
			if (strcasecmp(isc_commandline_argument, "size") == 0)
				isc_mem_debugging |= ISC_MEM_DEBUGSIZE;
			if (strcasecmp(isc_commandline_argument, "mctx") == 0)
				isc_mem_debugging |= ISC_MEM_DEBUGCTX;
			break;
		default:
			break;
		}
	}
	isc_commandline_reset = ISC_TRUE;
	check_result(isc_app_start(), "isc_app_start");

	result = isc_mem_create(0, 0, &mctx);
	if (result != ISC_R_SUCCESS)
		fatal("out of memory");

	dns_result_register();

	isc_commandline_errprint = ISC_FALSE;

	while ((ch = isc_commandline_parse(argc, argv, CMDLINE_FLAGS)) != -1) {
		switch (ch) {
		case 'c':
			classname = isc_commandline_argument;
			break;

		case 'E':
			engine = isc_commandline_argument;
			break;

		case 'h':
			usage();
			break;

		case 'I':
			inputformatstr = isc_commandline_argument;
			break;

		case 'm':
			break;

		case 'o':
			origin = isc_commandline_argument;
			break;

		case 'v':
			endp = NULL;
			verbose = strtol(isc_commandline_argument, &endp, 0);
			if (*endp != '\0')
				fatal("verbose level must be numeric");
			break;

		case 'x':
			keyset_kskonly = ISC_TRUE;
			break;

		case 'z':
			ignore_kskflag = ISC_TRUE;
			break;

		case '?':
			if (isc_commandline_option != '?')
				fprintf(stderr, "%s: invalid argument -%c\n",
					program, isc_commandline_option);
			usage();
			break;

		default:
			fprintf(stderr, "%s: unhandled option -%c\n",
				program, isc_commandline_option);
			exit(1);
		}
	}

	if (ectx == NULL)
		setup_entropy(mctx, NULL, &ectx);

	result = isc_hash_create(mctx, ectx, DNS_NAME_MAXWIRE);
	if (result != ISC_R_SUCCESS)
		fatal("could not create hash context");

	result = dst_lib_init2(mctx, ectx, engine, ISC_ENTROPY_BLOCKING);
	if (result != ISC_R_SUCCESS)
		fatal("could not initialize dst: %s",
		      isc_result_totext(result));

	isc_stdtime_get(&now);

	rdclass = strtoclass(classname);

	setup_logging(verbose, mctx, &log);

	argc -= isc_commandline_index;
	argv += isc_commandline_index;

	if (argc < 1)
		usage();

	file = argv[0];

	argc -= 1;
	argv += 1;

	POST(argc);
	POST(argv);

	if (origin == NULL)
		origin = file;

	if (inputformatstr != NULL) {
		if (strcasecmp(inputformatstr, "text") == 0)
			inputformat = dns_masterformat_text;
		else if (strcasecmp(inputformatstr, "raw") == 0)
			inputformat = dns_masterformat_raw;
		else
			fatal("unknown file format: %s\n", inputformatstr);
	}

	gdb = NULL;
	fprintf(stderr, "Loading zone '%s' from file '%s'\n", origin, file);
	loadzone(file, origin, rdclass, &gdb);
	gorigin = dns_db_origin(gdb);
	gclass = dns_db_class(gdb);

	gversion = NULL;
	result = dns_db_newversion(gdb, &gversion);
	check_result(result, "dns_db_newversion()");

	verifyzone(gdb, gversion, gorigin, mctx,
		   ignore_kskflag, keyset_kskonly);

	dns_db_closeversion(gdb, &gversion, ISC_FALSE);
	dns_db_detach(&gdb);

	cleanup_logging(&log);
	dst_lib_destroy();
	isc_hash_destroy();
	cleanup_entropy(&ectx);
	dns_name_destroy();
	if (verbose > 10)
		isc_mem_stats(mctx, stdout);
	isc_mem_destroy(&mctx);

	(void) isc_app_finish();

	return (0);
}
