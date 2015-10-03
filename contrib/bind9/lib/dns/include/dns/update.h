/*
 * Copyright (C) 2011, 2015  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: update.h,v 1.5 2011/08/30 23:46:53 tbox Exp $ */

#ifndef DNS_UPDATE_H
#define DNS_UPDATE_H 1

/*! \file dns/update.h */

/***
 ***	Imports
 ***/

#include <isc/lang.h>

#include <dns/types.h>
#include <dns/diff.h>

typedef struct {
	void (*func)(void *arg, dns_zone_t *zone, int level,
		     const char *message);
	void *arg;
} dns_update_log_t;

ISC_LANG_BEGINDECLS

/***
 ***	Functions
 ***/

isc_uint32_t
dns_update_soaserial(isc_uint32_t serial, dns_updatemethod_t method);
/*%<
 * Return the next serial number after 'serial', depending on the
 * update method 'method':
 *
 *\li	* dns_updatemethod_increment increments the serial number by one
 *\li	* dns_updatemethod_unixtime sets the serial number to the current
 *	  time (seconds since UNIX epoch) if possible, or increments by one
 *	  if not.
 */

isc_result_t
dns_update_signatures(dns_update_log_t *log, dns_zone_t *zone, dns_db_t *db,
		      dns_dbversion_t *oldver, dns_dbversion_t *newver,
		      dns_diff_t *diff, isc_uint32_t sigvalidityinterval);

isc_result_t
dns_update_signaturesinc(dns_update_log_t *log, dns_zone_t *zone, dns_db_t *db,
			 dns_dbversion_t *oldver, dns_dbversion_t *newver,
			 dns_diff_t *diff, isc_uint32_t sigvalidityinterval,
			 dns_update_state_t **state);

ISC_LANG_ENDDECLS

#endif /* DNS_UPDATE_H */
