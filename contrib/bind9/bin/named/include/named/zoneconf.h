/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2002  Internet Software Consortium.
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

/* $Id: zoneconf.h,v 1.16.2.2.8.1 2004/03/06 10:21:27 marka Exp $ */

#ifndef NS_ZONECONF_H
#define NS_ZONECONF_H 1

#include <isc/lang.h>
#include <isc/types.h>

#include <isccfg/cfg.h>

#include <named/aclconf.h>

ISC_LANG_BEGINDECLS

isc_result_t
ns_zone_configure(cfg_obj_t *config, cfg_obj_t *vconfig, cfg_obj_t *zconfig,
		  ns_aclconfctx_t *ac, dns_zone_t *zone);
/*
 * Configure or reconfigure a zone according to the named.conf
 * data in 'cctx' and 'czone'.
 *
 * The zone origin is not configured, it is assumed to have been set
 * at zone creation time.
 *
 * Require:
 *	'lctx' to be initialized or NULL.
 *	'cctx' to be initialized or NULL.
 *	'ac' to point to an initialized ns_aclconfctx_t.
 *	'czone' to be initialized.
 *	'zone' to be initialized.
 */

isc_boolean_t
ns_zone_reusable(dns_zone_t *zone, cfg_obj_t *zconfig);
/*
 * If 'zone' can be safely reconfigured according to the configuration
 * data in 'zconfig', return ISC_TRUE.  If the configuration data is so
 * different from the current zone state that the zone needs to be destroyed
 * and recreated, return ISC_FALSE.
 */

ISC_LANG_ENDDECLS

#endif /* NS_ZONECONF_H */
