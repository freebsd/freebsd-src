/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2003  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
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

/* $Id: named-checkzone.c,v 1.13.2.3.8.11 2004/10/25 01:36:06 marka Exp $ */

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
#include <dns/rdataclass.h>
#include <dns/rdataset.h>
#include <dns/result.h>
#include <dns/zone.h>

#include "check-tool.h"

static int quiet = 0;
static isc_mem_t *mctx = NULL;
static isc_entropy_t *ectx = NULL;
dns_zone_t *zone = NULL;
dns_zonetype_t zonetype = dns_zone_master;
static int dumpzone = 0;
static const char *output_filename;

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
		"usage: named-checkzone [-djqvD] [-c class] [-o output] "
		"[-t directory] [-w directory] [-k (ignore|warn|fail)] "
		"[-n (ignore|warn|fail)] zonename filename\n");
	exit(1);
}

static void
destroy(void) {
	if (zone != NULL)
		dns_zone_detach(&zone);
}

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

	while ((c = isc_commandline_parse(argc, argv, "c:dijk:n:qst:o:vw:D")) != EOF) {
		switch (c) {
		case 'c':
			classname = isc_commandline_argument;
			break;

		case 'd':
			debug++;
			break;

		case 'j':
			nomerge = ISC_FALSE;
			break;

		case 'n':
			if (!strcmp(isc_commandline_argument, "ignore"))
				zone_options &= ~(DNS_ZONEOPT_CHECKNS|
						  DNS_ZONEOPT_FATALNS);
			else if (!strcmp(isc_commandline_argument, "warn")) {
				zone_options |= DNS_ZONEOPT_CHECKNS;
				zone_options &= ~DNS_ZONEOPT_FATALNS;
			} else if (!strcmp(isc_commandline_argument, "fail"))
				zone_options |= DNS_ZONEOPT_CHECKNS|
					        DNS_ZONEOPT_FATALNS;
			break;

		case 'k':
			if (!strcmp(isc_commandline_argument, "warn")) {
				zone_options |= DNS_ZONEOPT_CHECKNAMES;
				zone_options &= ~DNS_ZONEOPT_CHECKNAMESFAIL;
			} else if (!strcmp(isc_commandline_argument,
					   "fail")) {
				zone_options |= DNS_ZONEOPT_CHECKNAMES |
						DNS_ZONEOPT_CHECKNAMESFAIL;
			} else if (!strcmp(isc_commandline_argument,
					   "ignore")) {
				zone_options &= ~(DNS_ZONEOPT_CHECKNAMES |
						  DNS_ZONEOPT_CHECKNAMESFAIL);
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

		default:
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

	if (isc_commandline_index + 2 > argc)
		usage();

	RUNTIME_CHECK(isc_mem_create(0, 0, &mctx) == ISC_R_SUCCESS);
	if (!quiet) {
		RUNTIME_CHECK(setup_logging(mctx, &lctx) == ISC_R_SUCCESS);
		dns_log_init(lctx);
		dns_log_setcontext(lctx);
	}
	RUNTIME_CHECK(isc_entropy_create(mctx, &ectx) == ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_hash_create(mctx, ectx, DNS_NAME_MAXWIRE)
		      == ISC_R_SUCCESS);

	dns_result_register();

	origin = argv[isc_commandline_index++];
	filename = argv[isc_commandline_index++];
	result = load_zone(mctx, origin, filename, classname, &zone);

	if (result == ISC_R_SUCCESS && dumpzone) {
		result = dump_zone(origin, zone, output_filename);
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
