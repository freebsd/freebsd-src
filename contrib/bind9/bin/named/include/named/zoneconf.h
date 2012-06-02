/*
 * Copyright (C) 2004-2007, 2010  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2002  Internet Software Consortium.
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

/* $Id: zoneconf.h,v 1.28 2010/12/20 23:47:20 tbox Exp $ */

#ifndef NS_ZONECONF_H
#define NS_ZONECONF_H 1

/*! \file */

#include <isc/lang.h>
#include <isc/types.h>

#include <isccfg/aclconf.h>
#include <isccfg/cfg.h>

ISC_LANG_BEGINDECLS

isc_result_t
ns_zone_configure(const cfg_obj_t *config, const cfg_obj_t *vconfig,
		  const cfg_obj_t *zconfig, cfg_aclconfctx_t *ac,
		  dns_zone_t *zone);
/*%<
 * Configure or reconfigure a zone according to the named.conf
 * data in 'cctx' and 'czone'.
 *
 * The zone origin is not configured, it is assumed to have been set
 * at zone creation time.
 *
 * Require:
 * \li	'lctx' to be initialized or NULL.
 * \li	'cctx' to be initialized or NULL.
 * \li	'ac' to point to an initialized ns_aclconfctx_t.
 * \li	'czone' to be initialized.
 * \li	'zone' to be initialized.
 */

isc_boolean_t
ns_zone_reusable(dns_zone_t *zone, const cfg_obj_t *zconfig);
/*%<
 * If 'zone' can be safely reconfigured according to the configuration
 * data in 'zconfig', return ISC_TRUE.  If the configuration data is so
 * different from the current zone state that the zone needs to be destroyed
 * and recreated, return ISC_FALSE.
 */


isc_result_t
ns_zone_configure_writeable_dlz(dns_dlzdb_t *dlzdatabase, dns_zone_t *zone,
				dns_rdataclass_t rdclass, dns_name_t *name);
/*%>
 * configure a DLZ zone, setting up the database methods and calling
 * postload to load the origin values
 *
 * Require:
 * \li	'dlzdatabase' to be a valid dlz database
 * \li	'zone' to be initialized.
 * \li	'rdclass' to be a valid rdataclass
 * \li	'name' to be a valid zone origin name
 */

ISC_LANG_ENDDECLS

#endif /* NS_ZONECONF_H */
