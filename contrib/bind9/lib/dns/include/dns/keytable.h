/*
 * Copyright (C) 2004, 2005, 2007, 2009, 2010  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: keytable.h,v 1.23 2010-06-25 03:24:05 marka Exp $ */

#ifndef DNS_KEYTABLE_H
#define DNS_KEYTABLE_H 1

/*****
 ***** Module Info
 *****/

/*! \file
 * \brief
 * The keytable module provides services for storing and retrieving DNSSEC
 * trusted keys, as well as the ability to find the deepest matching key
 * for a given domain name.
 *
 * MP:
 *\li	The module ensures appropriate synchronization of data structures it
 *	creates and manipulates.
 *
 * Resources:
 *\li	TBS
 *
 * Security:
 *\li	No anticipated impact.
 */

#include <isc/lang.h>
#include <isc/magic.h>
#include <isc/refcount.h>
#include <isc/rwlock.h>
#include <isc/stdtime.h>

#include <dns/types.h>

#include <dst/dst.h>

ISC_LANG_BEGINDECLS

struct dns_keytable {
	/* Unlocked. */
	unsigned int		magic;
	isc_mem_t		*mctx;
	isc_mutex_t		lock;
	isc_rwlock_t		rwlock;
	/* Locked by lock. */
	isc_uint32_t		active_nodes;
	/* Locked by rwlock. */
	isc_uint32_t		references;
	dns_rbt_t		*table;
};

#define KEYTABLE_MAGIC			ISC_MAGIC('K', 'T', 'b', 'l')
#define VALID_KEYTABLE(kt)	 	ISC_MAGIC_VALID(kt, KEYTABLE_MAGIC)

struct dns_keynode {
	unsigned int		magic;
	isc_refcount_t		refcount;
	dst_key_t *		key;
	isc_boolean_t           managed;
	struct dns_keynode *	next;
};

#define KEYNODE_MAGIC			ISC_MAGIC('K', 'N', 'o', 'd')
#define VALID_KEYNODE(kn)	 	ISC_MAGIC_VALID(kn, KEYNODE_MAGIC)

isc_result_t
dns_keytable_create(isc_mem_t *mctx, dns_keytable_t **keytablep);
/*%<
 * Create a keytable.
 *
 * Requires:
 *
 *\li	'mctx' is a valid memory context.
 *
 *\li	keytablep != NULL && *keytablep == NULL
 *
 * Ensures:
 *
 *\li	On success, *keytablep is a valid, empty key table.
 *
 * Returns:
 *
 *\li	ISC_R_SUCCESS
 *
 *\li	Any other result indicates failure.
 */


void
dns_keytable_attach(dns_keytable_t *source, dns_keytable_t **targetp);
/*%<
 * Attach *targetp to source.
 *
 * Requires:
 *
 *\li	'source' is a valid keytable.
 *
 *\li	'targetp' points to a NULL dns_keytable_t *.
 *
 * Ensures:
 *
 *\li	*targetp is attached to source.
 */

void
dns_keytable_detach(dns_keytable_t **keytablep);
/*%<
 * Detach *keytablep from its keytable.
 *
 * Requires:
 *
 *\li	'keytablep' points to a valid keytable.
 *
 * Ensures:
 *
 *\li	*keytablep is NULL.
 *
 *\li	If '*keytablep' is the last reference to the keytable,
 *		all resources used by the keytable will be freed
 */

isc_result_t
dns_keytable_add(dns_keytable_t *keytable, isc_boolean_t managed,
		 dst_key_t **keyp);
/*%<
 * Add '*keyp' to 'keytable' (using the name in '*keyp').
 * The value of keynode->managed is set to 'managed'
 *
 * Notes:
 *
 *\li	Ownership of *keyp is transferred to the keytable.
 *\li   If the key already exists in the table, ISC_R_EXISTS is
 *      returned and the new key is freed.
 *
 * Requires:
 *
 *\li	'keytable' points to a valid keytable.
 *
 *\li	keyp != NULL && *keyp is a valid dst_key_t *.
 *
 * Ensures:
 *
 *\li	On success, *keyp == NULL
 *
 * Returns:
 *
 *\li	ISC_R_SUCCESS
 *\li	ISC_R_EXISTS
 *
 *\li	Any other result indicates failure.
 */

isc_result_t
dns_keytable_marksecure(dns_keytable_t *keytable, dns_name_t *name);
/*%<
 * Add a null key to 'keytable' for name 'name'.  This marks the
 * name as a secure domain, but doesn't supply any key data to allow the
 * domain to be validated.  (Used when automated trust anchor management
 * has gotten broken by a zone misconfiguration; for example, when the
 * active key has been revoked but the stand-by key was still in its 30-day
 * waiting period for validity.)
 *
 * Notes:
 *
 *\li   If a key already exists in the table, ISC_R_EXISTS is
 *      returned and nothing is done.
 *
 * Requires:
 *
 *\li	'keytable' points to a valid keytable.
 *
 *\li	keyp != NULL && *keyp is a valid dst_key_t *.
 *
 * Returns:
 *
 *\li	ISC_R_SUCCESS
 *\li	ISC_R_EXISTS
 *
 *\li	Any other result indicates failure.
 */

isc_result_t
dns_keytable_delete(dns_keytable_t *keytable, dns_name_t *keyname);
/*%<
 * Delete node(s) from 'keytable' matching name 'keyname'
 *
 * Requires:
 *
 *\li	'keytable' points to a valid keytable.
 *
 *\li	'name' is not NULL
 *
 * Returns:
 *
 *\li	ISC_R_SUCCESS
 *
 *\li	Any other result indicates failure.
 */

isc_result_t
dns_keytable_deletekeynode(dns_keytable_t *keytable, dst_key_t *dstkey);
/*%<
 * Delete node(s) from 'keytable' containing copies of the key pointed
 * to by 'dstkey'
 *
 * Requires:
 *
 *\li	'keytable' points to a valid keytable.
 *\li	'dstkey' is not NULL
 *
 * Returns:
 *
 *\li	ISC_R_SUCCESS
 *
 *\li	Any other result indicates failure.
 */

isc_result_t
dns_keytable_find(dns_keytable_t *keytable, dns_name_t *keyname,
		  dns_keynode_t **keynodep);
/*%<
 * Search for the first instance of a key named 'name' in 'keytable',
 * without regard to keyid and algorithm.  Use dns_keytable_nextkeynode()
 * to find subsequent instances.
 *
 * Requires:
 *
 *\li	'keytable' is a valid keytable.
 *
 *\li	'name' is a valid absolute name.
 *
 *\li	keynodep != NULL && *keynodep == NULL
 *
 * Returns:
 *
 *\li	ISC_R_SUCCESS
 *\li	ISC_R_NOTFOUND
 *
 *\li	Any other result indicates an error.
 */

isc_result_t
dns_keytable_nextkeynode(dns_keytable_t *keytable, dns_keynode_t *keynode,
			 dns_keynode_t **nextnodep);
/*%<
 * Return for the next key after 'keynode' in 'keytable', without regard to
 * keyid and algorithm.
 *
 * Requires:
 *
 *\li	'keytable' is a valid keytable.
 *
 *\li	'keynode' is a valid keynode.
 *
 *\li	nextnodep != NULL && *nextnodep == NULL
 *
 * Returns:
 *
 *\li	ISC_R_SUCCESS
 *\li	ISC_R_NOTFOUND
 *
 *\li	Any other result indicates an error.
 */

isc_result_t
dns_keytable_findkeynode(dns_keytable_t *keytable, dns_name_t *name,
			 dns_secalg_t algorithm, dns_keytag_t tag,
			 dns_keynode_t **keynodep);
/*%<
 * Search for a key named 'name', matching 'algorithm' and 'tag' in
 * 'keytable'.  This finds the first instance which matches.  Use
 * dns_keytable_findnextkeynode() to find other instances.
 *
 * Requires:
 *
 *\li	'keytable' is a valid keytable.
 *
 *\li	'name' is a valid absolute name.
 *
 *\li	keynodep != NULL && *keynodep == NULL
 *
 * Returns:
 *
 *\li	ISC_R_SUCCESS
 *\li	DNS_R_PARTIALMATCH	the name existed in the keytable.
 *\li	ISC_R_NOTFOUND
 *
 *\li	Any other result indicates an error.
 */

isc_result_t
dns_keytable_findnextkeynode(dns_keytable_t *keytable, dns_keynode_t *keynode,
					     dns_keynode_t **nextnodep);
/*%<
 * Search for the next key with the same properties as 'keynode' in
 * 'keytable' as found by dns_keytable_findkeynode().
 *
 * Requires:
 *
 *\li	'keytable' is a valid keytable.
 *
 *\li	'keynode' is a valid keynode.
 *
 *\li	nextnodep != NULL && *nextnodep == NULL
 *
 * Returns:
 *
 *\li	ISC_R_SUCCESS
 *\li	ISC_R_NOTFOUND
 *
 *\li	Any other result indicates an error.
 */

isc_result_t
dns_keytable_finddeepestmatch(dns_keytable_t *keytable, dns_name_t *name,
			      dns_name_t *foundname);
/*%<
 * Search for the deepest match of 'name' in 'keytable'.
 *
 * Requires:
 *
 *\li	'keytable' is a valid keytable.
 *
 *\li	'name' is a valid absolute name.
 *
 *\li	'foundname' is a name with a dedicated buffer.
 *
 * Returns:
 *
 *\li	ISC_R_SUCCESS
 *\li	ISC_R_NOTFOUND
 *
 *\li	Any other result indicates an error.
 */

void
dns_keytable_attachkeynode(dns_keytable_t *keytable, dns_keynode_t *source,
			   dns_keynode_t **target);
/*%<
 * Attach a keynode and and increment the active_nodes counter in a
 * corresponding keytable.
 *
 * Requires:
 *
 *\li	'keytable' is a valid keytable.
 *
 *\li	'source' is a valid keynode.
 *
 *\li	'target' is not null and '*target' is null.
 */

void
dns_keytable_detachkeynode(dns_keytable_t *keytable,
			   dns_keynode_t **keynodep);
/*%<
 * Give back a keynode found via dns_keytable_findkeynode().
 *
 * Requires:
 *
 *\li	'keytable' is a valid keytable.
 *
 *\li	*keynodep is a valid keynode returned by a call to
 *	dns_keytable_findkeynode().
 *
 * Ensures:
 *
 *\li	*keynodep == NULL
 */

isc_result_t
dns_keytable_issecuredomain(dns_keytable_t *keytable, dns_name_t *name,
			    isc_boolean_t *wantdnssecp);
/*%<
 * Is 'name' at or beneath a trusted key?
 *
 * Requires:
 *
 *\li	'keytable' is a valid keytable.
 *
 *\li	'name' is a valid absolute name.
 *
 *\li	'*wantsdnssecp' is a valid isc_boolean_t.
 *
 * Ensures:
 *
 *\li	On success, *wantsdnssecp will be ISC_TRUE if and only if 'name'
 *	is at or beneath a trusted key.
 *
 * Returns:
 *
 *\li	ISC_R_SUCCESS
 *
 *\li	Any other result is an error.
 */

isc_result_t
dns_keytable_dump(dns_keytable_t *keytable, FILE *fp);
/*%<
 * Dump the keytable on fp.
 */

dst_key_t *
dns_keynode_key(dns_keynode_t *keynode);
/*%<
 * Get the DST key associated with keynode.
 */

isc_boolean_t
dns_keynode_managed(dns_keynode_t *keynode);
/*%<
 * Is this flagged as a managed key?
 */

isc_result_t
dns_keynode_create(isc_mem_t *mctx, dns_keynode_t **target);
/*%<
 * Allocate space for a keynode
 */

void
dns_keynode_attach(dns_keynode_t *source, dns_keynode_t **target);
/*%<
 * Attach keynode 'source' to '*target'
 */

void
dns_keynode_detach(isc_mem_t *mctx, dns_keynode_t **target);
/*%<
 * Detach a single keynode, without touching any keynodes that
 * may be pointed to by its 'next' pointer
 */

void
dns_keynode_detachall(isc_mem_t *mctx, dns_keynode_t **target);
/*%<
 * Detach a keynode and all its succesors.
 */
ISC_LANG_ENDDECLS

#endif /* DNS_KEYTABLE_H */
