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

/* $Id: check-tool.h,v 1.2.12.5 2004/03/08 04:04:13 marka Exp $ */

#ifndef CHECK_TOOL_H
#define CHECK_TOOL_H

#include <isc/lang.h>

#include <isc/types.h>
#include <dns/types.h>

ISC_LANG_BEGINDECLS

isc_result_t
setup_logging(isc_mem_t *mctx, isc_log_t **logp);

isc_result_t
load_zone(isc_mem_t *mctx, const char *zonename, const char *filename,
	  const char *classname, dns_zone_t **zonep);

isc_result_t
dump_zone(const char *zonename, dns_zone_t *zone, const char *filename);

extern int debug;
extern isc_boolean_t nomerge;
extern unsigned int zone_options;

ISC_LANG_ENDDECLS

#endif
