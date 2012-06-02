/*
 * Copyright (C) 2009  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: resconf.h,v 1.3 2009/09/02 23:48:02 tbox Exp $ */

#ifndef IRS_RESCONF_H
#define IRS_RESCONF_H 1

/*! \file
 *
 * \brief
 * The IRS resconf module parses the legacy "/etc/resolv.conf" file and
 * creates the corresponding configuration objects for the DNS library
 * modules.
 */

#include <irs/types.h>

/*%
 * A DNS search list specified in the 'domain' or 'search' statements
 * in the "resolv.conf" file.
 */
typedef struct irs_resconf_search {
	char					*domain;
	ISC_LINK(struct irs_resconf_search)	link;
} irs_resconf_search_t;

typedef ISC_LIST(irs_resconf_search_t) irs_resconf_searchlist_t;

ISC_LANG_BEGINDECLS

isc_result_t
irs_resconf_load(isc_mem_t *mctx, const char *filename, irs_resconf_t **confp);
/*%<
 * Load the resolver configuration file 'filename' in the "resolv.conf" format,
 * and create a new irs_resconf_t object from the configuration.
 *
 * Notes:
 *
 *\li	Currently, only the following options are supported:
 *	nameserver, domain, search, sortlist, ndots, and options.
 *	In addition, 'sortlist' is not actually effective; it's parsed, but
 *	the application cannot use the configuration.
 *
 * Requires:
 *
 *\li	'mctx' is a valid memory context.
 *
 *\li	'filename' != NULL
 *
 *\li	'confp' != NULL && '*confp' == NULL
 */

void
irs_resconf_destroy(irs_resconf_t **confp);
/*%<
 * Destroy the resconf object.
 *
 * Requires:
 *
 *\li	'*confp' is a valid resconf object.
 *
 * Ensures:
 *
 *\li	*confp == NULL
 */

isc_sockaddrlist_t *
irs_resconf_getnameservers(irs_resconf_t *conf);
/*%<
 * Return a list of name server addresses stored in 'conf'.
 *
 * Requires:
 *
 *\li	'conf' is a valid resconf object.
 */

irs_resconf_searchlist_t *
irs_resconf_getsearchlist(irs_resconf_t *conf);
/*%<
 * Return the search list stored in 'conf'.
 *
 * Requires:
 *
 *\li	'conf' is a valid resconf object.
 */

unsigned int
irs_resconf_getndots(irs_resconf_t *conf);
/*%<
 * Return the 'ndots' value stored in 'conf'.
 *
 * Requires:
 *
 *\li	'conf' is a valid resconf object.
 */

ISC_LANG_ENDDECLS

#endif /* IRS_RESCONF_H */
