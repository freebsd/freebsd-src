/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000, 2001  Internet Software Consortium.
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

/* $Id: stats.c,v 1.5.206.1 2004/03/06 08:13:46 marka Exp $ */

#include <config.h>

#include <isc/mem.h>

#include <dns/stats.h>

LIBDNS_EXTERNAL_DATA const char *dns_statscounter_names[DNS_STATS_NCOUNTERS] =
	{
	"success",
	"referral",
	"nxrrset",
	"nxdomain",
	"recursion",
	"failure"
	};

isc_result_t
dns_stats_alloccounters(isc_mem_t *mctx, isc_uint64_t **ctrp) {
	int i;
	isc_uint64_t *p =
		isc_mem_get(mctx, DNS_STATS_NCOUNTERS * sizeof(isc_uint64_t));
	if (p == NULL)
		return (ISC_R_NOMEMORY);
	for (i = 0; i < DNS_STATS_NCOUNTERS; i++)
		p[i] = 0;
	*ctrp = p;
	return (ISC_R_SUCCESS);
}

void
dns_stats_freecounters(isc_mem_t *mctx, isc_uint64_t **ctrp) {
	isc_mem_put(mctx, *ctrp, DNS_STATS_NCOUNTERS * sizeof(isc_uint64_t));
	*ctrp = NULL;
}
