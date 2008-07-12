/*
 * Copyright (C) 2004-2006  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000, 2001, 2003  Internet Software Consortium.
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

/* $Id: ssu.h,v 1.13.18.4 2006/02/16 23:51:32 marka Exp $ */

#ifndef DNS_SSU_H
#define DNS_SSU_H 1

/*! \file */

#include <isc/lang.h>

#include <dns/types.h>

ISC_LANG_BEGINDECLS

#define DNS_SSUMATCHTYPE_NAME 0
#define DNS_SSUMATCHTYPE_SUBDOMAIN 1
#define DNS_SSUMATCHTYPE_WILDCARD 2
#define DNS_SSUMATCHTYPE_SELF 3
#define DNS_SSUMATCHTYPE_SELFSUB 4
#define DNS_SSUMATCHTYPE_SELFWILD 5
#define DNS_SSUMATCHTYPE_MAX 5		/* maximum defined value */


isc_result_t
dns_ssutable_create(isc_mem_t *mctx, dns_ssutable_t **table);
/*%<
 *	Creates a table that will be used to store simple-secure-update rules.
 *	Note: all locking must be provided by the client.
 *
 *	Requires:
 *\li		'mctx' is a valid memory context
 *\li		'table' is not NULL, and '*table' is NULL
 *
 *	Returns:
 *\li		ISC_R_SUCCESS
 *\li		ISC_R_NOMEMORY
 */

void
dns_ssutable_attach(dns_ssutable_t *source, dns_ssutable_t **targetp);
/*%<
 *	Attach '*targetp' to 'source'.
 *
 *	Requires:
 *\li		'source' is a valid SSU table
 *\li		'targetp' points to a NULL dns_ssutable_t *.
 *
 *	Ensures:
 *\li		*targetp is attached to source.
 */

void
dns_ssutable_detach(dns_ssutable_t **tablep);
/*%<
 *	Detach '*tablep' from its simple-secure-update rule table.
 *
 *	Requires:
 *\li		'tablep' points to a valid dns_ssutable_t
 *
 *	Ensures:
 *\li		*tablep is NULL
 *\li		If '*tablep' is the last reference to the SSU table, all
 *			resources used by the table will be freed.
 */

isc_result_t
dns_ssutable_addrule(dns_ssutable_t *table, isc_boolean_t grant,
		     dns_name_t *identity, unsigned int matchtype,
		     dns_name_t *name, unsigned int ntypes,
		     dns_rdatatype_t *types);
/*%<
 *	Adds a new rule to a simple-secure-update rule table.  The rule
 *	either grants or denies update privileges of an identity (or set of
 *	identities) to modify a name (or set of names) or certain types present
 *	at that name.
 *
 *	Notes:
 *\li		If 'matchtype' is SELF, this rule only matches if the name
 *		to be updated matches the signing identity.
 *
 *\li		If 'ntypes' is 0, this rule applies to all types except
 *		NS, SOA, RRSIG, and NSEC.
 *
 *\li		If 'types' includes ANY, this rule applies to all types
 *		except NSEC.
 *
 *	Requires:
 *\li		'table' is a valid SSU table
 *\li		'identity' is a valid absolute name
 *\li		'matchtype' must be one of the defined constants.
 *\li		'name' is a valid absolute name
 *\li		If 'ntypes' > 0, 'types' must not be NULL
 *
 *	Returns:
 *\li		ISC_R_SUCCESS
 *\li		ISC_R_NOMEMORY
 */

isc_boolean_t
dns_ssutable_checkrules(dns_ssutable_t *table, dns_name_t *signer,
			dns_name_t *name, dns_rdatatype_t type);
/*%<
 *	Checks that the attempted update of (name, type) is allowed according
 *	to the rules specified in the simple-secure-update rule table.  If
 *	no rules are matched, access is denied.  If signer is NULL, access
 *	is denied.
 *
 *	Requires:
 *\li		'table' is a valid SSU table
 *\li		'signer' is NULL or a valid absolute name
 *\li		'name' is a valid absolute name
 */


/*% Accessor functions to extract rule components */
isc_boolean_t	dns_ssurule_isgrant(const dns_ssurule_t *rule);
/*% Accessor functions to extract rule components */
dns_name_t *	dns_ssurule_identity(const dns_ssurule_t *rule);
/*% Accessor functions to extract rule components */
unsigned int	dns_ssurule_matchtype(const dns_ssurule_t *rule);
/*% Accessor functions to extract rule components */
dns_name_t *	dns_ssurule_name(const dns_ssurule_t *rule);
/*% Accessor functions to extract rule components */
unsigned int	dns_ssurule_types(const dns_ssurule_t *rule,
				  dns_rdatatype_t **types);

isc_result_t	dns_ssutable_firstrule(const dns_ssutable_t *table,
				       dns_ssurule_t **rule);
/*%<
 * Initiates a rule iterator.  There is no need to maintain any state.
 *
 * Returns:
 *\li	#ISC_R_SUCCESS
 *\li	#ISC_R_NOMORE
 */

isc_result_t	dns_ssutable_nextrule(dns_ssurule_t *rule,
				      dns_ssurule_t **nextrule);
/*%<
 * Returns the next rule in the table.
 *
 * Returns:
 *\li	#ISC_R_SUCCESS
 *\li	#ISC_R_NOMORE
 */

ISC_LANG_ENDDECLS

#endif /* DNS_SSU_H */
