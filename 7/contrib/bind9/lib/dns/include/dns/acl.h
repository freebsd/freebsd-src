/*
 * Copyright (C) 2004-2006, 2009  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: acl.h,v 1.22.18.6 2009-01-19 23:46:15 tbox Exp $ */

#ifndef DNS_ACL_H
#define DNS_ACL_H 1

/*****
 ***** Module Info
 *****/

/*! \file
 * \brief
 * Address match list handling.
 */

/***
 *** Imports
 ***/

#include <isc/lang.h>
#include <isc/magic.h>
#include <isc/netaddr.h>
#include <isc/refcount.h>

#include <dns/name.h>
#include <dns/types.h>

/***
 *** Types
 ***/

typedef enum {
	dns_aclelementtype_ipprefix,
	dns_aclelementtype_keyname,
	dns_aclelementtype_nestedacl,
	dns_aclelementtype_localhost,
	dns_aclelementtype_localnets,
	dns_aclelementtype_any
} dns_aclelemettype_t;

typedef struct dns_aclipprefix dns_aclipprefix_t;

struct dns_aclipprefix {
	isc_netaddr_t address; /* IP4/IP6 */
	unsigned int prefixlen;
};

struct dns_aclelement {
	dns_aclelemettype_t type;
	isc_boolean_t negative;
	union {
		dns_aclipprefix_t ip_prefix;
		dns_name_t 	  keyname;
		dns_acl_t 	  *nestedacl;
	} u;
};

struct dns_acl {
	unsigned int		magic;
	isc_mem_t		*mctx;
	isc_refcount_t		refcount;
	dns_aclelement_t	*elements;
	unsigned int 		alloc;		/*%< Elements allocated */
	unsigned int 		length;		/*%< Elements initialized */
	char 			*name;		/*%< Temporary use only */
	ISC_LINK(dns_acl_t) 	nextincache;	/*%< Ditto */
};

struct dns_aclenv {
	dns_acl_t *localhost;
	dns_acl_t *localnets;
	isc_boolean_t match_mapped;
};

#define DNS_ACL_MAGIC		ISC_MAGIC('D','a','c','l')
#define DNS_ACL_VALID(a)	ISC_MAGIC_VALID(a, DNS_ACL_MAGIC)

/***
 *** Functions
 ***/

ISC_LANG_BEGINDECLS

isc_result_t
dns_acl_create(isc_mem_t *mctx, int n, dns_acl_t **target);
/*%<
 * Create a new ACL with room for 'n' elements.
 * The elements are uninitialized and the length is 0.
 */

isc_result_t
dns_acl_appendelement(dns_acl_t *acl, const dns_aclelement_t *elt);
/*%<
 * Append an element to an existing ACL.
 */

isc_result_t
dns_acl_any(isc_mem_t *mctx, dns_acl_t **target);
/*%<
 * Create a new ACL that matches everything.
 */

isc_result_t
dns_acl_none(isc_mem_t *mctx, dns_acl_t **target);
/*%<
 * Create a new ACL that matches nothing.
 */

void
dns_acl_attach(dns_acl_t *source, dns_acl_t **target);

void
dns_acl_detach(dns_acl_t **aclp);

isc_boolean_t
dns_aclelement_equal(const dns_aclelement_t *ea, const dns_aclelement_t *eb);

isc_boolean_t
dns_acl_equal(const dns_acl_t *a, const dns_acl_t *b);

isc_boolean_t
dns_acl_isinsecure(const dns_acl_t *a);
/*%<
 * Return #ISC_TRUE iff the acl 'a' is considered insecure, that is,
 * if it contains IP addresses other than those of the local host.
 * This is intended for applications such as printing warning
 * messages for suspect ACLs; it is not intended for making access
 * control decisions.  We make no guarantee that an ACL for which
 * this function returns #ISC_FALSE is safe.
 */

isc_result_t
dns_aclenv_init(isc_mem_t *mctx, dns_aclenv_t *env);

void
dns_aclenv_copy(dns_aclenv_t *t, dns_aclenv_t *s);

void
dns_aclenv_destroy(dns_aclenv_t *env);

isc_result_t
dns_acl_match(const isc_netaddr_t *reqaddr,
	      const dns_name_t *reqsigner,
	      const dns_acl_t *acl,
	      const dns_aclenv_t *env,
	      int *match,
	      const dns_aclelement_t **matchelt);
/*%<
 * General, low-level ACL matching.  This is expected to
 * be useful even for weird stuff like the topology and sortlist statements.
 *
 * Match the address 'reqaddr', and optionally the key name 'reqsigner',
 * against 'acl'.  'reqsigner' may be NULL.
 *
 * If there is a positive match, '*match' will be set to a positive value
 * indicating the distance from the beginning of the list.
 *
 * If there is a negative match, '*match' will be set to a negative value
 * whose absolute value indicates the distance from the beginning of
 * the list.
 *
 * If there is a match (either positive or negative) and 'matchelt' is
 * non-NULL, *matchelt will be attached to the primitive
 * (non-indirect) address match list element that matched.
 *
 * If there is no match, *match will be set to zero.
 *
 * Returns:
 *\li	#ISC_R_SUCCESS		Always succeeds.
 */

isc_boolean_t
dns_aclelement_match(const isc_netaddr_t *reqaddr,
		     const dns_name_t *reqsigner,
		     const dns_aclelement_t *e,
		     const dns_aclenv_t *env,
		     const dns_aclelement_t **matchelt);
/*%<
 * Like dns_acl_match, but matches against the single ACL element 'e'
 * rather than a complete list and returns ISC_TRUE iff it matched.
 *
 * To determine whether the match was positive or negative, the
 * caller should examine e->negative.  Since the element 'e' may be
 * a reference to a named ACL or a nested ACL, the matching element
 * returned through 'matchelt' is not necessarily 'e' itself.
 */

isc_result_t
dns_acl_elementmatch(const dns_acl_t *acl,
		     const dns_aclelement_t *elt,
		     const dns_aclelement_t **matchelt);
/*%<
 * Search for an ACL element in 'acl' which is exactly the same as 'elt'.
 * If there is one, and 'matchelt' is non NULL, then '*matchelt' will point
 * to the entry.
 *
 * This function is intended to be used for avoiding duplicated ACL entries
 * before adding an entry.
 *
 * Returns:
 *\li	#ISC_R_SUCCESS		Match succeeds.
 *\li	#ISC_R_NOTFOUND		Match fails.
 */

ISC_LANG_ENDDECLS

#endif /* DNS_ACL_H */
