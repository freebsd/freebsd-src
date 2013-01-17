/*
 * Copyright (C) 2004-2007  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: sortlist.c,v 1.17 2007/09/14 01:46:05 marka Exp $ */

/*! \file */

#include <config.h>

#include <isc/mem.h>
#include <isc/util.h>

#include <dns/acl.h>
#include <dns/result.h>

#include <named/globals.h>
#include <named/server.h>
#include <named/sortlist.h>

ns_sortlisttype_t
ns_sortlist_setup(dns_acl_t *acl, isc_netaddr_t *clientaddr,
		  const void **argp)
{
	unsigned int i;

	if (acl == NULL)
		goto dont_sort;

	for (i = 0; i < acl->length; i++) {
		/*
		 * 'e' refers to the current 'top level statement'
		 * in the sortlist (see ARM).
		 */
		dns_aclelement_t *e = &acl->elements[i];
		dns_aclelement_t *try_elt;
		dns_aclelement_t *order_elt = NULL;
		const dns_aclelement_t *matched_elt = NULL;

		if (e->type == dns_aclelementtype_nestedacl) {
			dns_acl_t *inner = e->nestedacl;

			if (inner->length == 0)
				try_elt = e;
			else if (inner->length > 2)
				goto dont_sort;
			else if (inner->elements[0].negative)
				goto dont_sort;
			else {
				try_elt = &inner->elements[0];
				if (inner->length == 2)
					order_elt = &inner->elements[1];
			}
		} else {
			/*
			 * BIND 8 allows bare elements at the top level
			 * as an undocumented feature.
			 */
			try_elt = e;
		}

		if (dns_aclelement_match(clientaddr, NULL, try_elt,
					 &ns_g_server->aclenv,
					 &matched_elt)) {
			if (order_elt != NULL) {
				if (order_elt->type ==
				    dns_aclelementtype_nestedacl) {
					*argp = order_elt->nestedacl;
					return (NS_SORTLISTTYPE_2ELEMENT);
				} else if (order_elt->type ==
					   dns_aclelementtype_localhost &&
					   ns_g_server->aclenv.localhost != NULL) {
					*argp = ns_g_server->aclenv.localhost;
					return (NS_SORTLISTTYPE_2ELEMENT);
				} else if (order_elt->type ==
					   dns_aclelementtype_localnets &&
					   ns_g_server->aclenv.localnets != NULL) {
					*argp = ns_g_server->aclenv.localnets;
					return (NS_SORTLISTTYPE_2ELEMENT);
				} else {
					/*
					 * BIND 8 allows a bare IP prefix as
					 * the 2nd element of a 2-element
					 * sortlist statement.
					 */
					*argp = order_elt;
					return (NS_SORTLISTTYPE_1ELEMENT);
				}
			} else {
				INSIST(matched_elt != NULL);
				*argp = matched_elt;
				return (NS_SORTLISTTYPE_1ELEMENT);
			}
		}
	}

	/* No match; don't sort. */
 dont_sort:
	*argp = NULL;
	return (NS_SORTLISTTYPE_NONE);
}

int
ns_sortlist_addrorder2(const isc_netaddr_t *addr, const void *arg) {
	const dns_acl_t *sortacl = (const dns_acl_t *) arg;
	int match;

	(void)dns_acl_match(addr, NULL, sortacl,
			    &ns_g_server->aclenv,
			    &match, NULL);
	if (match > 0)
		return (match);
	else if (match < 0)
		return (INT_MAX - (-match));
	else
		return (INT_MAX / 2);
}

int
ns_sortlist_addrorder1(const isc_netaddr_t *addr, const void *arg) {
	const dns_aclelement_t *matchelt = (const dns_aclelement_t *) arg;
	if (dns_aclelement_match(addr, NULL, matchelt,
				 &ns_g_server->aclenv,
				 NULL)) {
		return (0);
	} else {
		return (INT_MAX);
	}
}

void
ns_sortlist_byaddrsetup(dns_acl_t *sortlist_acl, isc_netaddr_t *client_addr,
		       dns_addressorderfunc_t *orderp,
		       const void **argp)
{
	ns_sortlisttype_t sortlisttype;

	sortlisttype = ns_sortlist_setup(sortlist_acl, client_addr, argp);

	switch (sortlisttype) {
	case NS_SORTLISTTYPE_1ELEMENT:
		*orderp = ns_sortlist_addrorder1;
		break;
	case NS_SORTLISTTYPE_2ELEMENT:
		*orderp = ns_sortlist_addrorder2;
		break;
	case NS_SORTLISTTYPE_NONE:
		*orderp = NULL;
		break;
	default:
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "unexpected return from ns_sortlist_setup(): "
				 "%d", sortlisttype);
		break;
	}
}

