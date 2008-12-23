/*
 * Copyright (C) 2004-2008  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2003  Internet Software Consortium.
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

/* $Id: named-checkzone.c,v 1.29.18.21 2008/10/24 01:43:17 tbox Exp $ */

/*! \file */

#include <config.h>

#include <stdlib.h>

#include <isc/app.h>
#include <isc/commandline.h>
#include <isc/dir.h>
#include <isc/entropy.h>
#include <isc/hash.h>
#include <isc/log.h>
#include <isc/mem.h>
#include <isc/socket.h>
#include <isc/string.h>
#include <isc/task.h>
#include <isc/timer.h>
#include <isc/util.h>

#include <dns/db.h>
#include <dns/fixedname.h>
#include <dns/log.h>
#include <dns/masterdump.h>
#include <dns/name.h>
#include <dns/rdataclass.h>
#include <dns/rdataset.h>
#include <dns/result.h>
#include <dns/types.h>
#include <dns/zone.h>

#include "check-tool.h"

static int quiet = 0;
static isc_mem_t *mctx = NULL;
static isc_entropy_t *ectx = NULL;
dns_zone_t *zone = NULL;
dns_zonetype_t zonetype = dns_zone_master;
static int dumpzone = 0;
static const char *output_filename;
static char *prog_name = NULL;
static const dns_master_style_t *outputstyle = NULL;
static enum { progmode_check, progmode_compile } progmode;

#define ERRRET(result, function) \
	do { \
		if (result != ISC_R_SUCCESS) { \
			if (!quiet) \
				fprintf(stderr, "%s() returned %s\n", \
					function, dns_result_totext(result)); \
			return (result); \
		} \
	} while (0)

static void
usage(void) {
	fprintf(stderr,
		"usage: %s [-djqvD] [-c class] [-o output] "
		"[-f inputformat] [-F outputformat] "
		"[-t directory] [-w directory] [-k (ignore|warn|fail)] "
		"[-n (ignore|warn|fail)] [-m (ignore|warn|fail)] "
		"[-i (full|full-sibling|local|local-sibling|none)] "
		"[-M (ignore|warn|fail)] [-S (ignore|warn|fail)] "
		"[-W (ignore|warn)] "
		"zonename filename\n", prog_name);
	exit(1);
}

static void
destroy(void) {
	if (zone != NULL)
		dns_zone_detach(&zone);
	dns_name_destroy();
}

/*% main processing routine */
int
main(int argc, char **argv) {
	int c;
	char *origin = NULL;
	char *filename = NULL;
	isc_log_t *lctx = NULL;
	isc_result_t result;
	char classname_in[] = "IN";
	char *classname = classname_in;
	const char *workdir = NULL;
	const char *inputformatstr = NULL;
	const char *outputformatstr = NULL;
	dns_masterformat_t inputformat = dns_masterformat_text;
	dns_masterformat_t outputformat = dns_masterformat_text;

	outputstyle = &dns_master_style_full;

	prog_name = strrchr(argv[0], '/');
	if (prog_name == NULL)
		prog_name = strrchr(argv[0], '\\');
	if (prog_name != NULL)
		prog_name++;
	else
		prog_name = argv[0];
	/*
	 * Libtool doesn't preserve the program name prior to final
	 * installation.  Remove the libtool prefix ("lt-").
	 */
	if (strncmp(prog_name, "lt-", 3) == 0)
		prog_name += 3;
	if (strcmp(prog_name, "named-checkzone") == 0)
		progmode = progmode_check;
	else if (strcmp(prog_name, "named-compilezone") == 0)
		progmode = progmode_compile;
	else
		INSIST(0);

	/* Compilation specific defaults */
	if (progmode == progmode_compile) {
		zone_options |= (DNS_ZONEOPT_CHECKNS |
				 DNS_ZONEOPT_FATALNS |
				 DNS_ZONEOPT_CHECKNAMES |
				 DNS_ZONEOPT_CHECKNAMESFAIL |
				 DNS_ZONEOPT_CHECKWILDCARD);
	}

#define ARGCMP(X) (strcmp(isc_commandline_argument, X) == 0)

	while ((c = isc_commandline_parse(argc, argv,
					  "c:df:i:jk:m:n:qs:t:o:vw:DF:M:S:W:"))
	       != EOF) {
		switch (c) {
		case 'c':
			classname = isc_commandline_argument;
			break;

		case 'd':
			debug++;
			break;

		case 'i':
			if (ARGCMP("full")) {
				zone_options |= DNS_ZONEOPT_CHECKINTEGRITY |
						DNS_ZONEOPT_CHECKSIBLING;
				docheckmx = ISC_TRUE;
				docheckns = ISC_TRUE;
				dochecksrv = ISC_TRUE;
			} else if (ARGCMP("full-sibling")) {
				zone_options |= DNS_ZONEOPT_CHECKINTEGRITY;
				zone_options &= ~DNS_ZONEOPT_CHECKSIBLING;
				docheckmx = ISC_TRUE;
				docheckns = ISC_TRUE;
				dochecksrv = ISC_TRUE;
			} else if (ARGCMP("local")) {
				zone_options |= DNS_ZONEOPT_CHECKINTEGRITY;
				zone_options |= DNS_ZONEOPT_CHECKSIBLING;
				docheckmx = ISC_FALSE;
				docheckns = ISC_FALSE;
				dochecksrv = ISC_FALSE;
			} else if (ARGCMP("local-sibling")) {
				zone_options |= DNS_ZONEOPT_CHECKINTEGRITY;
				zone_options &= ~DNS_ZONEOPT_CHECKSIBLING;
				docheckmx = ISC_FALSE;
				docheckns = ISC_FALSE;
				dochecksrv = ISC_FALSE;
			} else if (ARGCMP("none")) {
				zone_options &= ~DNS_ZONEOPT_CHECKINTEGRITY;
				zone_options &= ~DNS_ZONEOPT_CHECKSIBLING;
				docheckmx = ISC_FALSE;
				docheckns = ISC_FALSE;
				dochecksrv = ISC_FALSE;
			} else {
				fprintf(stderr, "invalid argument to -i: %s\n",
					isc_commandline_argument);
				exit(1);
			}
			break;

		case 'f':
			inputformatstr = isc_commandline_argument;
			break;

		case 'F':
			outputformatstr = isc_commandline_argument;
			break;

		case 'j':
			nomerge = ISC_FALSE;
			break;

		case 'k':
			if (ARGCMP("warn")) {
				zone_options |= DNS_ZONEOPT_CHECKNAMES;
				zone_options &= ~DNS_ZONEOPT_CHECKNAMESFAIL;
			} else if (ARGCMP("fail")) {
				zone_options |= DNS_ZONEOPT_CHECKNAMES |
						DNS_ZONEOPT_CHECKNAMESFAIL;
			} else if (ARGCMP("ignore")) {
				zone_options &= ~(DNS_ZONEOPT_CHECKNAMES |
						  DNS_ZONEOPT_CHECKNAMESFAIL);
			} else {
				fprintf(stderr, "invalid argument to -k: %s\n",
					isc_commandline_argument);
				exit(1);
			}
			break;

		case 'n':
			if (ARGCMP("ignore")) {
				zone_options &= ~(DNS_ZONEOPT_CHECKNS|
						  DNS_ZONEOPT_FATALNS);
			} else if (ARGCMP("warn")) {
				zone_options |= DNS_ZONEOPT_CHECKNS;
				zone_options &= ~DNS_ZONEOPT_FATALNS;
			} else if (ARGCMP("fail")) {
				zone_options |= DNS_ZONEOPT_CHECKNS|
						DNS_ZONEOPT_FATALNS;
			} else {
				fprintf(stderr, "invalid argument to -n: %s\n",
					isc_commandline_argument);
				exit(1);
			}
			break;

		case 'm':
			if (ARGCMP("warn")) {
				zone_options |= DNS_ZONEOPT_CHECKMX;
				zone_options &= ~DNS_ZONEOPT_CHECKMXFAIL;
			} else if (ARGCMP("fail")) {
				zone_options |= DNS_ZONEOPT_CHECKMX |
						DNS_ZONEOPT_CHECKMXFAIL;
			} else if (ARGCMP("ignore")) {
				zone_options &= ~(DNS_ZONEOPT_CHECKMX |
						  DNS_ZONEOPT_CHECKMXFAIL);
			} else {
				fprintf(stderr, "invalid argument to -m: %s\n",
					isc_commandline_argument);
				exit(1);
			}
			break;

		case 'q':
			quiet++;
			break;

		case 't':
			result = isc_dir_chroot(isc_commandline_argument);
			if (result != ISC_R_SUCCESS) {
				fprintf(stderr, "isc_dir_chroot: %s: %s\n",
					isc_commandline_argument,
					isc_result_totext(result));
				exit(1);
			}
			result = isc_dir_chdir("/");
			if (result != ISC_R_SUCCESS) {
				fprintf(stderr, "isc_dir_chdir: %s\n",
					isc_result_totext(result));
				exit(1);
			}
			break;

		case 's':
			if (ARGCMP("full"))
				outputstyle = &dns_master_style_full;
			else if (ARGCMP("relative")) {
				outputstyle = &dns_master_style_default;
			} else {
				fprintf(stderr,
					"unknown or unsupported style: %s\n",
					isc_commandline_argument);
				exit(1);
			}
			break;

		case 'o':
			output_filename = isc_commandline_argument;
			break;

		case 'v':
			printf(VERSION "\n");
			exit(0);

		case 'w':
			workdir = isc_commandline_argument;
			break;

		case 'D':
			dumpzone++;
			break;

		case 'M':
			if (ARGCMP("fail")) {
				zone_options &= ~DNS_ZONEOPT_WARNMXCNAME;
				zone_options &= ~DNS_ZONEOPT_IGNOREMXCNAME;
			} else if (ARGCMP("warn")) {
				zone_options |= DNS_ZONEOPT_WARNMXCNAME;
				zone_options &= ~DNS_ZONEOPT_IGNOREMXCNAME;
			} else if (ARGCMP("ignore")) {
				zone_options |= DNS_ZONEOPT_WARNMXCNAME;
				zone_options |= DNS_ZONEOPT_IGNOREMXCNAME;
			} else {
				fprintf(stderr, "invalid argument to -M: %s\n",
					isc_commandline_argument);
				exit(1);
			}
			break;

		case 'S':
			if (ARGCMP("fail")) {
				zone_options &= ~DNS_ZONEOPT_WARNSRVCNAME;
				zone_options &= ~DNS_ZONEOPT_IGNORESRVCNAME;
			} else if (ARGCMP("warn")) {
				zone_options |= DNS_ZONEOPT_WARNSRVCNAME;
				zone_options &= ~DNS_ZONEOPT_IGNORESRVCNAME;
			} else if (ARGCMP("ignore")) {
				zone_options |= DNS_ZONEOPT_WARNSRVCNAME;
				zone_options |= DNS_ZONEOPT_IGNORESRVCNAME;
			} else {
				fprintf(stderr, "invalid argument to -S: %s\n",
					isc_commandline_argument);
				exit(1);
			}
			break;

		case 'W':
			if (ARGCMP("warn"))
				zone_options |= DNS_ZONEOPT_CHECKWILDCARD;
			else if (ARGCMP("ignore"))
				zone_options &= ~DNS_ZONEOPT_CHECKWILDCARD;
			break;

		default:
			usage();
		}
	}

	if (progmode == progmode_compile) {
		dumpzone = 1;	/* always dump */
		if (output_filename == NULL) {
			fprintf(stderr,
				"output file required, but not specified\n");
			usage();
		}
	}

	if (workdir != NULL) {
		result = isc_dir_chdir(workdir);
		if (result != ISC_R_SUCCESS) {
			fprintf(stderr, "isc_dir_chdir: %s: %s\n",
				workdir, isc_result_totext(result));
			exit(1);
		}
	}

	if (inputformatstr != NULL) {
		if (strcasecmp(inputformatstr, "text") == 0)
			inputformat = dns_masterformat_text;
		else if (strcasecmp(inputformatstr, "raw") == 0)
			inputformat = dns_masterformat_raw;
		else {
			fprintf(stderr, "unknown file format: %s\n",
			    inputformatstr);
			exit(1);
		}
	}

	if (outputformatstr != NULL) {
		if (strcasecmp(outputformatstr, "text") == 0)
			outputformat = dns_masterformat_text;
		else if (strcasecmp(outputformatstr, "raw") == 0)
			outputformat = dns_masterformat_raw;
		else {
			fprintf(stderr, "unknown file format: %s\n",
				outputformatstr);
			exit(1);
		}
	}

	if (isc_commandline_index + 2 > argc)
		usage();

	RUNTIME_CHECK(isc_mem_create(0, 0, &mctx) == ISC_R_SUCCESS);
	if (!quiet)
		RUNTIME_CHECK(setup_logging(mctx, &lctx) == ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_entropy_create(mctx, &ectx) == ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_hash_create(mctx, ectx, DNS_NAME_MAXWIRE)
		      == ISC_R_SUCCESS);

	dns_result_register();

	origin = argv[isc_commandline_index++];
	filename = argv[isc_commandline_index++];
	result = load_zone(mctx, origin, filename, inputformat, classname,
			   &zone);

	if (result == ISC_R_SUCCESS && dumpzone) {
		if (!quiet && progmode == progmode_compile) {
			fprintf(stdout, "dump zone to %s...", output_filename);
			fflush(stdout);
		}
		result = dump_zone(origin, zone, output_filename,
				   outputformat, outputstyle);
		if (!quiet && progmode == progmode_compile)
			fprintf(stdout, "done\n");
	}

	if (!quiet && result == ISC_R_SUCCESS)
		fprintf(stdout, "OK\n");
	destroy();
	if (lctx != NULL)
		isc_log_destroy(&lctx);
	isc_hash_destroy();
	isc_entropy_detach(&ectx);
	isc_mem_destroy(&mctx);
	return ((result == ISC_R_SUCCESS) ? 0 : 1);
}
