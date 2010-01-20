/*
 * Copyright (C) 2004, 2005, 2007  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000, 2001  Internet Software Consortium.
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

/* $Id: listenlist.h,v 1.15 2007/06/19 23:46:59 tbox Exp $ */

#ifndef NAMED_LISTENLIST_H
#define NAMED_LISTENLIST_H 1

/*****
 ***** Module Info
 *****/

/*! \file
 * \brief
 * "Listen lists", as in the "listen-on" configuration statement.
 */

/***
 *** Imports
 ***/
#include <isc/net.h>

#include <dns/types.h>

/***
 *** Types
 ***/

typedef struct ns_listenelt ns_listenelt_t;
typedef struct ns_listenlist ns_listenlist_t;

struct ns_listenelt {
	isc_mem_t *	       		mctx;
	in_port_t			port;
	dns_acl_t *	       		acl;
	ISC_LINK(ns_listenelt_t)	link;
};

struct ns_listenlist {
	isc_mem_t *			mctx;
	int				refcount;
	ISC_LIST(ns_listenelt_t)	elts;
};

/***
 *** Functions
 ***/

isc_result_t
ns_listenelt_create(isc_mem_t *mctx, in_port_t port,
		    dns_acl_t *acl, ns_listenelt_t **target);
/*%
 * Create a listen-on list element.
 */

void
ns_listenelt_destroy(ns_listenelt_t *elt);
/*%
 * Destroy a listen-on list element.
 */

isc_result_t
ns_listenlist_create(isc_mem_t *mctx, ns_listenlist_t **target);
/*%
 * Create a new, empty listen-on list.
 */

void
ns_listenlist_attach(ns_listenlist_t *source, ns_listenlist_t **target);
/*%
 * Attach '*target' to '*source'.
 */

void
ns_listenlist_detach(ns_listenlist_t **listp);
/*%
 * Detach 'listp'.
 */

isc_result_t
ns_listenlist_default(isc_mem_t *mctx, in_port_t port,
		      isc_boolean_t enabled, ns_listenlist_t **target);
/*%
 * Create a listen-on list with default contents, matching
 * all addresses with port 'port' (if 'enabled' is ISC_TRUE),
 * or no addresses (if 'enabled' is ISC_FALSE).
 */

#endif /* NAMED_LISTENLIST_H */


