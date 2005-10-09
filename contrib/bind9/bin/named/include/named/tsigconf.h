/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: tsigconf.h,v 1.9.208.1 2004/03/06 10:21:26 marka Exp $ */

#ifndef NS_TSIGCONF_H
#define NS_TSIGCONF_H 1

#include <isc/types.h>
#include <isc/lang.h>

ISC_LANG_BEGINDECLS

isc_result_t
ns_tsigkeyring_fromconfig(cfg_obj_t *config, cfg_obj_t *vconfig,
			  isc_mem_t *mctx, dns_tsig_keyring_t **ringp);
/*
 * Create a TSIG key ring and configure it according to the 'key'
 * statements in the global and view configuration objects.
 *
 *	Requires:
 *		'config' is not NULL.
 *		'mctx' is not NULL
 *		'ring' is not NULL, and '*ring' is NULL
 *
 *	Returns:
 *		ISC_R_SUCCESS
 *		ISC_R_NOMEMORY
 */

ISC_LANG_ENDDECLS

#endif /* NS_TSIGCONF_H */
