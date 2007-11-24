/*
 * Copyright (C) 2004, 2006  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2001  Internet Software Consortium.
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

/* $Id: tkeyconf.h,v 1.9.208.3 2006/03/02 00:37:20 marka Exp $ */

#ifndef NS_TKEYCONF_H
#define NS_TKEYCONF_H 1

#include <isc/types.h>
#include <isc/lang.h>

#include <isccfg/cfg.h>

ISC_LANG_BEGINDECLS

isc_result_t
ns_tkeyctx_fromconfig(const cfg_obj_t *options, isc_mem_t *mctx,
		      isc_entropy_t *ectx, dns_tkeyctx_t **tctxp);
/*
 * 	Create a TKEY context and configure it, including the default DH key
 *	and default domain, according to 'options'.
 *
 *	Requires:
 *		'cfg' is a valid configuration options object.
 *		'mctx' is not NULL
 *		'ectx' is not NULL
 *		'tctx' is not NULL
 *		'*tctx' is NULL
 *
 *	Returns:
 *		ISC_R_SUCCESS
 *		ISC_R_NOMEMORY
 */

ISC_LANG_ENDDECLS

#endif /* NS_TKEYCONF_H */
