/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000-2002  Internet Software Consortium.
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

/* $Id: check-tool.c,v 1.4.12.7 2004/11/30 01:15:40 marka Exp $ */

#include <config.h>

#include <stdio.h>
#include <string.h>

#include "check-tool.h"
#include <isc/util.h>

#include <isc/buffer.h>
#include <isc/log.h>
#include <isc/region.h>
#include <isc/stdio.h>
#include <isc/types.h>

#include <dns/fixedname.h>
#include <dns/log.h>
#include <dns/name.h>
#include <dns/rdataclass.h>
#include <dns/types.h>
#include <dns/zone.h>

#define CHECK(r) \
        do { \
		result = (r); \
                if (result != ISC_R_SUCCESS) \
                        goto cleanup; \
        } while (0)   

static const char *dbtype[] = { "rbt" };

int debug = 0;
isc_boolean_t nomerge = ISC_TRUE;
unsigned int zone_options = DNS_ZONEOPT_CHECKNS | 
			    DNS_ZONEOPT_MANYERRORS |
			    DNS_ZONEOPT_CHECKNAMES;

isc_result_t
setup_logging(isc_mem_t *mctx, isc_log_t **logp) {
	isc_logdestination_t destination;
	isc_logconfig_t *logconfig = NULL;
	isc_log_t *log = NULL;

	RUNTIME_CHECK(isc_log_create(mctx, &log, &logconfig) == ISC_R_SUCCESS);
	isc_log_setcontext(log);

	destination.file.stream = stdout;
	destination.file.name = NULL;
	destination.file.versions = ISC_LOG_ROLLNEVER;
	destination.file.maximum_size = 0;
	RUNTIME_CHECK(isc_log_createchannel(logconfig, "stderr",
				       ISC_LOG_TOFILEDESC,
				       ISC_LOG_DYNAMIC,
				       &destination, 0) == ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_log_usechannel(logconfig, "stderr",
					 NULL, NULL) == ISC_R_SUCCESS);

	*logp = log;
	return (ISC_R_SUCCESS);
}

isc_result_t
load_zone(isc_mem_t *mctx, const char *zonename, const char *filename,
	  const char *classname, dns_zone_t **zonep)
{
	isc_result_t result;
	dns_rdataclass_t rdclass;
	isc_textregion_t region;
	isc_buffer_t buffer;
	dns_fixedname_t fixorigin;
	dns_name_t *origin;
	dns_zone_t *zone = NULL;

	REQUIRE(zonep == NULL || *zonep == NULL);

	if (debug)
		fprintf(stderr, "loading \"%s\" from \"%s\" class \"%s\"\n",
			zonename, filename, classname);

	CHECK(dns_zone_create(&zone, mctx));

	dns_zone_settype(zone, dns_zone_master);

	isc_buffer_init(&buffer, zonename, strlen(zonename));
	isc_buffer_add(&buffer, strlen(zonename));
	dns_fixedname_init(&fixorigin);
	origin = dns_fixedname_name(&fixorigin);
	CHECK(dns_name_fromtext(origin, &buffer, dns_rootname,
			        ISC_FALSE, NULL));
	CHECK(dns_zone_setorigin(zone, origin));
	CHECK(dns_zone_setdbtype(zone, 1, (const char * const *) dbtype));
	CHECK(dns_zone_setfile(zone, filename));

	DE_CONST(classname, region.base);
	region.length = strlen(classname);
	CHECK(dns_rdataclass_fromtext(&rdclass, &region));

	dns_zone_setclass(zone, rdclass);
	dns_zone_setoption(zone, zone_options, ISC_TRUE);
	dns_zone_setoption(zone, DNS_ZONEOPT_NOMERGE, nomerge);

	CHECK(dns_zone_load(zone));
	if (zonep != NULL){
		*zonep = zone;
		zone = NULL;
	}

 cleanup:
	if (zone != NULL)
		dns_zone_detach(&zone);
	return (result);
}

isc_result_t
dump_zone(const char *zonename, dns_zone_t *zone, const char *filename)
{
	isc_result_t result;
	FILE *output = stdout;

	if (debug) {
		if (filename != NULL)
			fprintf(stderr, "dumping \"%s\" to \"%s\"\n",
				zonename, filename);
		else
			fprintf(stderr, "dumping \"%s\"\n", zonename);
	}

	if (filename != NULL) {
		result = isc_stdio_open(filename, "w+", &output);

		if (result != ISC_R_SUCCESS) {
			fprintf(stderr, "could not open output "
				"file \"%s\" for writing\n", filename);
			return (ISC_R_FAILURE);
		}
	}

	result = dns_zone_fulldumptostream(zone, output);

	if (filename != NULL)
		(void)isc_stdio_close(output);

	return (result);
}
