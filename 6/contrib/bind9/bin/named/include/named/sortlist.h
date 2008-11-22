/*
 * Copyright (C) 2004, 2006  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: sortlist.h,v 1.4.208.3 2006/03/02 00:37:20 marka Exp $ */

#ifndef NAMED_SORTLIST_H
#define NAMED_SORTLIST_H 1

#include <isc/types.h>

#include <dns/types.h>

/*
 * Type for callback functions that rank addresses.
 */
typedef int 
(*dns_addressorderfunc_t)(const isc_netaddr_t *address, const void *arg);

/*
 * Return value type for setup_sortlist.
 */
typedef enum {
	NS_SORTLISTTYPE_NONE,
	NS_SORTLISTTYPE_1ELEMENT,
	NS_SORTLISTTYPE_2ELEMENT
} ns_sortlisttype_t;

ns_sortlisttype_t
ns_sortlist_setup(dns_acl_t *acl, isc_netaddr_t *clientaddr,
		  const void **argp);
/*
 * Find the sortlist statement in 'acl' that applies to 'clientaddr', if any.
 *
 * If a 1-element sortlist item applies, return NS_SORTLISTTYPE_1ELEMENT and
 * make '*argp' point to the matching subelement.
 *
 * If a 2-element sortlist item applies, return NS_SORTLISTTYPE_2ELEMENT and
 * make '*argp' point to ACL that forms the second element.
 *
 * If no sortlist item applies, return NS_SORTLISTTYPE_NONE and set '*argp'
 * to NULL.
 */

int
ns_sortlist_addrorder1(const isc_netaddr_t *addr, const void *arg);
/*
 * Find the sort order of 'addr' in 'arg', the matching element
 * of a 1-element top-level sortlist statement.
 */

int
ns_sortlist_addrorder2(const isc_netaddr_t *addr, const void *arg);
/*
 * Find the sort order of 'addr' in 'arg', a topology-like
 * ACL forming the second element in a 2-element top-level
 * sortlist statement.
 */

void
ns_sortlist_byaddrsetup(dns_acl_t *sortlist_acl, isc_netaddr_t *client_addr,
			dns_addressorderfunc_t *orderp,
			const void **argp);
/*
 * Find the sortlist statement in 'acl' that applies to 'clientaddr', if any.
 * If a sortlist statement applies, return in '*orderp' a pointer to a function
 * for ranking network addresses based on that sortlist statement, and in
 * '*argp' an argument to pass to said function.  If no sortlist statement
 * applies, set '*orderp' and '*argp' to NULL.
 */

#endif /* NAMED_SORTLIST_H */
