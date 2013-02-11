/*
 * Copyright (C) 2004-2012  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2003  Internet Software Consortium.
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

/* $Id$ */

#ifndef DNS_VIEW_H
#define DNS_VIEW_H 1

/*****
 ***** Module Info
 *****/

/*! \file dns/view.h
 * \brief
 * DNS View
 *
 * A "view" is a DNS namespace, together with an optional resolver and a
 * forwarding policy.  A "DNS namespace" is a (possibly empty) set of
 * authoritative zones together with an optional cache and optional
 * "hints" information.
 *
 * Views start out "unfrozen".  In this state, core attributes like
 * the cache, set of zones, and forwarding policy may be set.  While
 * "unfrozen", the caller (e.g. nameserver configuration loading
 * code), must ensure exclusive access to the view.  When the view is
 * "frozen", the core attributes become immutable, and the view module
 * will ensure synchronization.  Freezing allows the view's core attributes
 * to be accessed without locking.
 *
 * MP:
 *\li	Before the view is frozen, the caller must ensure synchronization.
 *
 *\li	After the view is frozen, the module guarantees appropriate
 *	synchronization of any data structures it creates and manipulates.
 *
 * Reliability:
 *\li	No anticipated impact.
 *
 * Resources:
 *\li	TBS
 *
 * Security:
 *\li	No anticipated impact.
 *
 * Standards:
 *\li	None.
 */

#include <stdio.h>

#include <isc/lang.h>
#include <isc/magic.h>
#include <isc/event.h>
#include <isc/mutex.h>
#include <isc/net.h>
#include <isc/refcount.h>
#include <isc/rwlock.h>
#include <isc/stdtime.h>

#include <dns/acl.h>
#include <dns/fixedname.h>
#include <dns/rdatastruct.h>
#include <dns/rpz.h>
#include <dns/types.h>

ISC_LANG_BEGINDECLS

struct dns_view {
	/* Unlocked. */
	unsigned int			magic;
	isc_mem_t *			mctx;
	dns_rdataclass_t		rdclass;
	char *				name;
	dns_zt_t *			zonetable;
	dns_dlzdb_t *			dlzdatabase;
	dns_resolver_t *		resolver;
	dns_adb_t *			adb;
	dns_requestmgr_t *		requestmgr;
	dns_acache_t *			acache;
	dns_cache_t *			cache;
	dns_db_t *			cachedb;
	dns_db_t *			hints;

	/*
	 * security roots.
	 * internal use only; access via * dns_view_getsecroots()
	 */
	dns_keytable_t *		secroots_priv;

	isc_mutex_t			lock;
	isc_boolean_t			frozen;
	isc_task_t *			task;
	isc_event_t			resevent;
	isc_event_t			adbevent;
	isc_event_t			reqevent;
	isc_stats_t *			resstats;
	dns_stats_t *			resquerystats;
	isc_boolean_t			cacheshared;

	/* Configurable data. */
	dns_tsig_keyring_t *		statickeys;
	dns_tsig_keyring_t *		dynamickeys;
	dns_peerlist_t *		peers;
	dns_order_t *			order;
	dns_fwdtable_t *		fwdtable;
	isc_boolean_t			recursion;
	isc_boolean_t			auth_nxdomain;
	isc_boolean_t			additionalfromcache;
	isc_boolean_t			additionalfromauth;
	isc_boolean_t			minimalresponses;
	isc_boolean_t			enablednssec;
	isc_boolean_t			enablevalidation;
	isc_boolean_t			acceptexpired;
	dns_transfer_format_t		transfer_format;
	dns_acl_t *			cacheacl;
	dns_acl_t *			cacheonacl;
	dns_acl_t *			queryacl;
	dns_acl_t *			queryonacl;
	dns_acl_t *			recursionacl;
	dns_acl_t *			recursiononacl;
	dns_acl_t *			sortlist;
	dns_acl_t *			notifyacl;
	dns_acl_t *			transferacl;
	dns_acl_t *			updateacl;
	dns_acl_t *			upfwdacl;
	dns_acl_t *			denyansweracl;
	dns_rbt_t *			answeracl_exclude;
	dns_rbt_t *			denyanswernames;
	dns_rbt_t *			answernames_exclude;
	isc_boolean_t			requestixfr;
	isc_boolean_t			provideixfr;
	isc_boolean_t			requestnsid;
	dns_ttl_t			maxcachettl;
	dns_ttl_t			maxncachettl;
	in_port_t			dstport;
	dns_aclenv_t			aclenv;
	dns_rdatatype_t			preferred_glue;
	isc_boolean_t			flush;
	dns_namelist_t *		delonly;
	isc_boolean_t			rootdelonly;
	dns_namelist_t *		rootexclude;
	isc_boolean_t			checknames;
	dns_name_t *			dlv;
	dns_fixedname_t			dlv_fixed;
	isc_uint16_t			maxudp;
	dns_v4_aaaa_t			v4_aaaa;
	dns_acl_t *			v4_aaaa_acl;
	dns_dns64list_t 		dns64;
	unsigned int 			dns64cnt;
	ISC_LIST(dns_rpz_zone_t)	rpz_zones;
	isc_boolean_t			rpz_recursive_only;
	isc_boolean_t			rpz_break_dnssec;

	/*
	 * Configurable data for server use only,
	 * locked by server configuration lock.
	 */
	dns_acl_t *			matchclients;
	dns_acl_t *			matchdestinations;
	isc_boolean_t			matchrecursiveonly;

	/* Locked by themselves. */
	isc_refcount_t			references;

	/* Locked by lock. */
	unsigned int			weakrefs;
	unsigned int			attributes;
	/* Under owner's locking control. */
	ISC_LINK(struct dns_view)	link;
	dns_viewlist_t *		viewlist;

	dns_zone_t *			managed_keys;

#ifdef BIND9
	/* File in which to store configuration for newly added zones */
	char *				new_zone_file;

	void *				new_zone_config;
	void				(*cfg_destroy)(void **);
#endif
};

#define DNS_VIEW_MAGIC			ISC_MAGIC('V','i','e','w')
#define DNS_VIEW_VALID(view)		ISC_MAGIC_VALID(view, DNS_VIEW_MAGIC)

#define DNS_VIEWATTR_RESSHUTDOWN	0x01
#define DNS_VIEWATTR_ADBSHUTDOWN	0x02
#define DNS_VIEWATTR_REQSHUTDOWN	0x04

isc_result_t
dns_view_create(isc_mem_t *mctx, dns_rdataclass_t rdclass,
		const char *name, dns_view_t **viewp);
/*%<
 * Create a view.
 *
 * Notes:
 *
 *\li	The newly created view has no cache, no resolver, and an empty
 *	zone table.  The view is not frozen.
 *
 * Requires:
 *
 *\li	'mctx' is a valid memory context.
 *
 *\li	'rdclass' is a valid class.
 *
 *\li	'name' is a valid C string.
 *
 *\li	viewp != NULL && *viewp == NULL
 *
 * Returns:
 *
 *\li	#ISC_R_SUCCESS
 *\li	#ISC_R_NOMEMORY
 *
 *\li	Other errors are possible.
 */

void
dns_view_attach(dns_view_t *source, dns_view_t **targetp);
/*%<
 * Attach '*targetp' to 'source'.
 *
 * Requires:
 *
 *\li	'source' is a valid, frozen view.
 *
 *\li	'targetp' points to a NULL dns_view_t *.
 *
 * Ensures:
 *
 *\li	*targetp is attached to source.
 *
 *\li	While *targetp is attached, the view will not shut down.
 */

void
dns_view_detach(dns_view_t **viewp);
/*%<
 * Detach '*viewp' from its view.
 *
 * Requires:
 *
 *\li	'viewp' points to a valid dns_view_t *
 *
 * Ensures:
 *
 *\li	*viewp is NULL.
 */

void
dns_view_flushanddetach(dns_view_t **viewp);
/*%<
 * Detach '*viewp' from its view.  If this was the last reference
 * uncommitted changed in zones will be flushed to disk.
 *
 * Requires:
 *
 *\li	'viewp' points to a valid dns_view_t *
 *
 * Ensures:
 *
 *\li	*viewp is NULL.
 */

void
dns_view_weakattach(dns_view_t *source, dns_view_t **targetp);
/*%<
 * Weakly attach '*targetp' to 'source'.
 *
 * Requires:
 *
 *\li	'source' is a valid, frozen view.
 *
 *\li	'targetp' points to a NULL dns_view_t *.
 *
 * Ensures:
 *
 *\li	*targetp is attached to source.
 *
 * \li	While *targetp is attached, the view will not be freed.
 */

void
dns_view_weakdetach(dns_view_t **targetp);
/*%<
 * Detach '*viewp' from its view.
 *
 * Requires:
 *
 *\li	'viewp' points to a valid dns_view_t *.
 *
 * Ensures:
 *
 *\li	*viewp is NULL.
 */

isc_result_t
dns_view_createresolver(dns_view_t *view,
			isc_taskmgr_t *taskmgr, unsigned int ntasks,
			isc_socketmgr_t *socketmgr,
			isc_timermgr_t *timermgr,
			unsigned int options,
			dns_dispatchmgr_t *dispatchmgr,
			dns_dispatch_t *dispatchv4,
			dns_dispatch_t *dispatchv6);
/*%<
 * Create a resolver and address database for the view.
 *
 * Requires:
 *
 *\li	'view' is a valid, unfrozen view.
 *
 *\li	'view' does not have a resolver already.
 *
 *\li	The requirements of dns_resolver_create() apply to 'taskmgr',
 *	'ntasks', 'socketmgr', 'timermgr', 'options', 'dispatchv4', and
 *	'dispatchv6'.
 *
 * Returns:
 *
 *\li   	#ISC_R_SUCCESS
 *
 *\li	Any error that dns_resolver_create() can return.
 */

void
dns_view_setcache(dns_view_t *view, dns_cache_t *cache);
void
dns_view_setcache2(dns_view_t *view, dns_cache_t *cache, isc_boolean_t shared);
/*%<
 * Set the view's cache database.  If 'shared' is true, this means the cache
 * is created by another view and is shared with that view.  dns_view_setcache()
 * is a backward compatible version equivalent to setcache2(..., ISC_FALSE).
 *
 * Requires:
 *
 *\li	'view' is a valid, unfrozen view.
 *
 *\li	'cache' is a valid cache.
 *
 * Ensures:
 *
 * \li    	The cache of 'view' is 'cached.
 *
 *\li	If this is not the first call to dns_view_setcache() for this
 *	view, then previously set cache is detached.
 */

void
dns_view_sethints(dns_view_t *view, dns_db_t *hints);
/*%<
 * Set the view's hints database.
 *
 * Requires:
 *
 *\li	'view' is a valid, unfrozen view, whose hints database has not been
 *	set.
 *
 *\li	'hints' is a valid zone database.
 *
 * Ensures:
 *
 * \li    	The hints database of 'view' is 'hints'.
 */

void
dns_view_setkeyring(dns_view_t *view, dns_tsig_keyring_t *ring);
void
dns_view_setdynamickeyring(dns_view_t *view, dns_tsig_keyring_t *ring);
/*%<
 * Set the view's static TSIG keys
 *
 * Requires:
 *
 *   \li   'view' is a valid, unfrozen view, whose static TSIG keyring has not
 *	been set.
 *
 *\li      'ring' is a valid TSIG keyring
 *
 * Ensures:
 *
 *\li      The static TSIG keyring of 'view' is 'ring'.
 */

void
dns_view_getdynamickeyring(dns_view_t *view, dns_tsig_keyring_t **ringp);
/*%<
 * Return the views dynamic keys.
 *
 *   \li  'view' is a valid, unfrozen view.
 *   \li  'ringp' != NULL && ringp == NULL.
 */

void
dns_view_setdstport(dns_view_t *view, in_port_t dstport);
/*%<
 * Set the view's destination port.  This is the port to
 * which outgoing queries are sent.  The default is 53,
 * the standard DNS port.
 *
 * Requires:
 *
 *\li      'view' is a valid view.
 *
 *\li      'dstport' is a valid TCP/UDP port number.
 *
 * Ensures:
 *\li	External name servers will be assumed to be listening
 *	on 'dstport'.  For servers whose address has already
 *	obtained obtained at the time of the call, the view may
 *	continue to use the previously set port until the address
 *	times out from the view's address database.
 */


isc_result_t
dns_view_addzone(dns_view_t *view, dns_zone_t *zone);
/*%<
 * Add zone 'zone' to 'view'.
 *
 * Requires:
 *
 *\li	'view' is a valid, unfrozen view.
 *
 *\li	'zone' is a valid zone.
 */

void
dns_view_freeze(dns_view_t *view);
/*%<
 * Freeze view.  No changes can be made to view configuration while frozen.
 *
 * Requires:
 *
 *\li	'view' is a valid, unfrozen view.
 *
 * Ensures:
 *
 *\li	'view' is frozen.
 */

void
dns_view_thaw(dns_view_t *view);
/*%<
 * Thaw view.  This allows zones to be added or removed at runtime.  This is
 * NOT thread-safe; the caller MUST have run isc_task_exclusive() prior to
 * thawing the view.
 *
 * Requires:
 *
 *\li	'view' is a valid, frozen view.
 *
 * Ensures:
 *
 *\li	'view' is no longer frozen.
 */
isc_result_t
dns_view_find(dns_view_t *view, dns_name_t *name, dns_rdatatype_t type,
	      isc_stdtime_t now, unsigned int options, isc_boolean_t use_hints,
	      dns_db_t **dbp, dns_dbnode_t **nodep, dns_name_t *foundname,
	      dns_rdataset_t *rdataset, dns_rdataset_t *sigrdataset);
isc_result_t
dns_view_find2(dns_view_t *view, dns_name_t *name, dns_rdatatype_t type,
	       isc_stdtime_t now, unsigned int options,
	       isc_boolean_t use_hints, isc_boolean_t use_static_stub,
	       dns_db_t **dbp, dns_dbnode_t **nodep, dns_name_t *foundname,
	       dns_rdataset_t *rdataset, dns_rdataset_t *sigrdataset);
/*%<
 * Find an rdataset whose owner name is 'name', and whose type is
 * 'type'.
 * In general, this function first searches view's zone and cache DBs for the
 * best match data against 'name'.  If nothing found there, and if 'use_hints'
 * is ISC_TRUE, the view's hint DB (if configured) is searched.
 * If the view is configured with a static-stub zone which gives the longest
 * match for 'name' among the zones, however, the cache DB is not consulted
 * unless 'use_static_stub' is ISC_FALSE (see below about this argument).
 *
 * dns_view_find() is a backward compatible version equivalent to
 * dns_view_find2() with use_static_stub argument being ISC_FALSE.
 *
 * Notes:
 *
 *\li	See the description of dns_db_find() for information about 'options'.
 *	If the caller sets #DNS_DBFIND_GLUEOK, it must ensure that 'name'
 *	and 'type' are appropriate for glue retrieval.
 *
 *\li	If 'now' is zero, then the current time will be used.
 *
 *\li	If 'use_hints' is ISC_TRUE, and the view has a hints database, then
 *	it will be searched last.  If the answer is found in the hints
 *	database, the result code will be DNS_R_HINT.  If the name is found
 *	in the hints database but not the type, the result code will be
 *	#DNS_R_HINTNXRRSET.
 *
 *\li	If 'use_static_stub' is ISC_FALSE and the longest match zone for 'name'
 *	is a static-stub zone, it's ignored and the cache and/or hints will be
 *	searched.  In the majority of the cases this argument should be
 *	ISC_FALSE.  The only known usage of this argument being ISC_TRUE is
 *	if this search is for a "bailiwick" glue A or AAAA RRset that may
 *	best match a static-stub zone.  Consider the following example:
 *	this view is configured with a static-stub zone "example.com",
 *	and an attempt of recursive resolution needs to send a query for the
 *	zone.  In this case it's quite likely that the resolver is trying to
 *	find A/AAAA RRs for the apex name "example.com".  And, to honor the
 *	static-stub configuration it needs to return the glue RRs in the
 *	static-stub zone even if that exact RRs coming from the authoritative
 *	zone has been cached.
 *	In other general cases, the requested data is better to be
 *	authoritative, either locally configured or retrieved from an external
 *	server, and the data in the static-stub zone should better be ignored.
 *
 *\li	'foundname' must meet the requirements of dns_db_find().
 *
 *\li	If 'sigrdataset' is not NULL, and there is a SIG rdataset which
 *	covers 'type', then 'sigrdataset' will be bound to it.
 *
 * Requires:
 *
 *\li	'view' is a valid, frozen view.
 *
 *\li	'name' is valid name.
 *
 *\li	'type' is a valid dns_rdatatype_t, and is not a meta query type
 *	except dns_rdatatype_any.
 *
 *\li	dbp == NULL || *dbp == NULL
 *
 *\li	nodep == NULL || *nodep == NULL.  If nodep != NULL, dbp != NULL.
 *
 *\li	'foundname' is a valid name with a dedicated buffer or NULL.
 *
 *\li	'rdataset' is a valid, disassociated rdataset.
 *
 *\li	'sigrdataset' is NULL, or is a valid, disassociated rdataset.
 *
 * Ensures:
 *
 *\li	In successful cases, 'rdataset', and possibly 'sigrdataset', are
 *	bound to the found data.
 *
 *\li	If dbp != NULL, it points to the database containing the data.
 *
 *\li	If nodep != NULL, it points to the database node containing the data.
 *
 *\li	If foundname != NULL, it contains the full name of the found data.
 *
 * Returns:
 *
 *\li	Any result that dns_db_find() can return, with the exception of
 *	#DNS_R_DELEGATION.
 */

isc_result_t
dns_view_simplefind(dns_view_t *view, dns_name_t *name, dns_rdatatype_t type,
		    isc_stdtime_t now, unsigned int options,
		    isc_boolean_t use_hints,
		    dns_rdataset_t *rdataset, dns_rdataset_t *sigrdataset);
/*%<
 * Find an rdataset whose owner name is 'name', and whose type is
 * 'type'.
 *
 * Notes:
 *
 *\li	This routine is appropriate for simple, exact-match queries of the
 *	view.  'name' must be a canonical name; there is no DNAME or CNAME
 *	processing.
 *
 *\li	See the description of dns_db_find() for information about 'options'.
 *	If the caller sets DNS_DBFIND_GLUEOK, it must ensure that 'name'
 *	and 'type' are appropriate for glue retrieval.
 *
 *\li	If 'now' is zero, then the current time will be used.
 *
 *\li	If 'use_hints' is ISC_TRUE, and the view has a hints database, then
 *	it will be searched last.  If the answer is found in the hints
 *	database, the result code will be DNS_R_HINT.  If the name is found
 *	in the hints database but not the type, the result code will be
 *	DNS_R_HINTNXRRSET.
 *
 *\li	If 'sigrdataset' is not NULL, and there is a SIG rdataset which
 *	covers 'type', then 'sigrdataset' will be bound to it.
 *
 * Requires:
 *
 *\li	'view' is a valid, frozen view.
 *
 *\li	'name' is valid name.
 *
 *\li	'type' is a valid dns_rdatatype_t, and is not a meta query type
 *	(e.g. dns_rdatatype_any), or dns_rdatatype_rrsig.
 *
 *\li	'rdataset' is a valid, disassociated rdataset.
 *
 *\li	'sigrdataset' is NULL, or is a valid, disassociated rdataset.
 *
 * Ensures:
 *
 *\li	In successful cases, 'rdataset', and possibly 'sigrdataset', are
 *	bound to the found data.
 *
 * Returns:
 *
 *\li	#ISC_R_SUCCESS			Success; result is desired type.
 *\li	DNS_R_GLUE			Success; result is glue.
 *\li	DNS_R_HINT			Success; result is a hint.
 *\li	DNS_R_NCACHENXDOMAIN		Success; result is a ncache entry.
 *\li	DNS_R_NCACHENXRRSET		Success; result is a ncache entry.
 *\li	DNS_R_NXDOMAIN			The name does not exist.
 *\li	DNS_R_NXRRSET			The rrset does not exist.
 *\li	#ISC_R_NOTFOUND			No matching data found,
 *					or an error occurred.
 */

/*% See dns_view_findzonecut2() */
isc_result_t
dns_view_findzonecut(dns_view_t *view, dns_name_t *name, dns_name_t *fname,
		     isc_stdtime_t now, unsigned int options,
		     isc_boolean_t use_hints,
		     dns_rdataset_t *rdataset, dns_rdataset_t *sigrdataset);

isc_result_t
dns_view_findzonecut2(dns_view_t *view, dns_name_t *name, dns_name_t *fname,
		      isc_stdtime_t now, unsigned int options,
		      isc_boolean_t use_hints, isc_boolean_t use_cache,
		      dns_rdataset_t *rdataset, dns_rdataset_t *sigrdataset);
/*%<
 * Find the best known zonecut containing 'name'.
 *
 * This uses local authority, cache, and optionally hints data.
 * No external queries are performed.
 *
 * Notes:
 *
 *\li	If 'now' is zero, then the current time will be used.
 *
 *\li	If 'use_hints' is ISC_TRUE, and the view has a hints database, then
 *	it will be searched last.
 *
 *\li	If 'use_cache' is ISC_TRUE, and the view has a cache, then it will be
 *	searched.
 *
 *\li	If 'sigrdataset' is not NULL, and there is a SIG rdataset which
 *	covers 'type', then 'sigrdataset' will be bound to it.
 *
 *\li	If the DNS_DBFIND_NOEXACT option is set, then the zonecut returned
 *	(if any) will be the deepest known ancestor of 'name'.
 *
 * Requires:
 *
 *\li	'view' is a valid, frozen view.
 *
 *\li	'name' is valid name.
 *
 *\li	'rdataset' is a valid, disassociated rdataset.
 *
 *\li	'sigrdataset' is NULL, or is a valid, disassociated rdataset.
 *
 * Returns:
 *
 *\li	#ISC_R_SUCCESS				Success.
 *
 *\li	Many other results are possible.
 */

isc_result_t
dns_viewlist_find(dns_viewlist_t *list, const char *name,
		  dns_rdataclass_t rdclass, dns_view_t **viewp);
/*%<
 * Search for a view with name 'name' and class 'rdclass' in 'list'.
 * If found, '*viewp' is (strongly) attached to it.
 *
 * Requires:
 *
 *\li	'viewp' points to a NULL dns_view_t *.
 *
 * Returns:
 *
 *\li	#ISC_R_SUCCESS		A matching view was found.
 *\li	#ISC_R_NOTFOUND		No matching view was found.
 */

isc_result_t
dns_viewlist_findzone(dns_viewlist_t *list, dns_name_t *name, isc_boolean_t allclasses,
		      dns_rdataclass_t rdclass, dns_zone_t **zonep);

/*%<
 * Search zone with 'name' in view with 'rdclass' in viewlist 'list'
 * If found, zone is returned in *zonep. If allclasses is set rdclass is ignored
 *
 * Returns:
 *\li	#ISC_R_SUCCESS          A matching zone was found.
 *\li	#ISC_R_NOTFOUND         No matching zone was found.
 */

isc_result_t
dns_view_findzone(dns_view_t *view, dns_name_t *name, dns_zone_t **zonep);
/*%<
 * Search for the zone 'name' in the zone table of 'view'.
 * If found, 'zonep' is (strongly) attached to it.  There
 * are no partial matches.
 *
 * Requires:
 *
 *\li	'zonep' points to a NULL dns_zone_t *.
 *
 * Returns:
 *\li	#ISC_R_SUCCESS		A matching zone was found.
 *\li	#ISC_R_NOTFOUND		No matching zone was found.
 *\li	others			An error occurred.
 */

isc_result_t
dns_view_load(dns_view_t *view, isc_boolean_t stop);

isc_result_t
dns_view_loadnew(dns_view_t *view, isc_boolean_t stop);
/*%<
 * Load zones attached to this view.  dns_view_load() loads
 * all zones whose master file has changed since the last
 * load; dns_view_loadnew() loads only zones that have never
 * been loaded.
 *
 * If 'stop' is ISC_TRUE, stop on the first error and return it.
 * If 'stop' is ISC_FALSE, ignore errors.
 *
 * Requires:
 *
 *\li	'view' is valid.
 */

isc_result_t
dns_view_gettsig(dns_view_t *view, dns_name_t *keyname,
		 dns_tsigkey_t **keyp);
/*%<
 * Find the TSIG key configured in 'view' with name 'keyname',
 * if any.
 *
 * Requires:
 *\li	keyp points to a NULL dns_tsigkey_t *.
 *
 * Returns:
 *\li	#ISC_R_SUCCESS	A key was found and '*keyp' now points to it.
 *\li	#ISC_R_NOTFOUND	No key was found.
 *\li	others		An error occurred.
 */

isc_result_t
dns_view_getpeertsig(dns_view_t *view, isc_netaddr_t *peeraddr,
		     dns_tsigkey_t **keyp);
/*%<
 * Find the TSIG key configured in 'view' for the server whose
 * address is 'peeraddr', if any.
 *
 * Requires:
 *	keyp points to a NULL dns_tsigkey_t *.
 *
 * Returns:
 *\li	#ISC_R_SUCCESS	A key was found and '*keyp' now points to it.
 *\li	#ISC_R_NOTFOUND	No key was found.
 *\li	others		An error occurred.
 */

isc_result_t
dns_view_checksig(dns_view_t *view, isc_buffer_t *source, dns_message_t *msg);
/*%<
 * Verifies the signature of a message.
 *
 * Requires:
 *
 *\li	'view' is a valid view.
 *\li	'source' is a valid buffer containing the message
 *\li	'msg' is a valid message
 *
 * Returns:
 *\li	see dns_tsig_verify()
 */

void
dns_view_dialup(dns_view_t *view);
/*%<
 * Perform dialup-time maintenance on the zones of 'view'.
 */

isc_result_t
dns_view_dumpdbtostream(dns_view_t *view, FILE *fp);
/*%<
 * Dump the current state of the view 'view' to the stream 'fp'
 * for purposes of analysis or debugging.
 *
 * Currently the dumped state includes the view's cache; in the future
 * it may also include other state such as the address database.
 * It will not not include authoritative data since it is voluminous and
 * easily obtainable by other means.
 *
 * Requires:
 *
 *\li	'view' is valid.
 *
 *\li	'fp' refers to a file open for writing.
 *
 * Returns:
 * \li	ISC_R_SUCCESS	The cache was successfully dumped.
 * \li	others		An error occurred (see dns_master_dump)
 */

isc_result_t
dns_view_flushcache(dns_view_t *view);
isc_result_t
dns_view_flushcache2(dns_view_t *view, isc_boolean_t fixuponly);
/*%<
 * Flush the view's cache (and ADB).  If 'fixuponly' is true, it only updates
 * the internal reference to the cache DB with omitting actual flush operation.
 * 'fixuponly' is intended to be used for a view that shares a cache with
 * a different view.  dns_view_flushcache() is a backward compatible version
 * that always sets fixuponly to false.
 *
 * Requires:
 * 	'view' is valid.
 *
 * 	No other tasks are executing.
 *
 * Returns:
 *\li	#ISC_R_SUCCESS
 *\li	#ISC_R_NOMEMORY
 */

isc_result_t
dns_view_flushname(dns_view_t *view, dns_name_t *);
/*%<
 * Flush the given name from the view's cache (and ADB).
 *
 * Requires:
 *\li	'view' is valid.
 *\li	'name' is valid.
 *
 * Returns:
 *\li	#ISC_R_SUCCESS
 *	other returns are failures.
 */

isc_result_t
dns_view_adddelegationonly(dns_view_t *view, dns_name_t *name);
/*%<
 * Add the given name to the delegation only table.
 *
 *
 * Requires:
 *\li	'view' is valid.
 *\li	'name' is valid.
 *
 * Returns:
 *\li	#ISC_R_SUCCESS
 *\li	#ISC_R_NOMEMORY
 */

isc_result_t
dns_view_excludedelegationonly(dns_view_t *view, dns_name_t *name);
/*%<
 * Add the given name to be excluded from the root-delegation-only.
 *
 *
 * Requires:
 *\li	'view' is valid.
 *\li	'name' is valid.
 *
 * Returns:
 *\li	#ISC_R_SUCCESS
 *\li	#ISC_R_NOMEMORY
 */

isc_boolean_t
dns_view_isdelegationonly(dns_view_t *view, dns_name_t *name);
/*%<
 * Check if 'name' is in the delegation only table or if
 * rootdelonly is set that name is not being excluded.
 *
 * Requires:
 *\li	'view' is valid.
 *\li	'name' is valid.
 *
 * Returns:
 *\li	#ISC_TRUE if the name is the table.
 *\li	#ISC_FALSE otherwise.
 */

void
dns_view_setrootdelonly(dns_view_t *view, isc_boolean_t value);
/*%<
 * Set the root delegation only flag.
 *
 * Requires:
 *\li	'view' is valid.
 */

isc_boolean_t
dns_view_getrootdelonly(dns_view_t *view);
/*%<
 * Get the root delegation only flag.
 *
 * Requires:
 *\li	'view' is valid.
 */

isc_result_t
dns_view_freezezones(dns_view_t *view, isc_boolean_t freeze);
/*%<
 * Freeze/thaw updates to master zones.
 *
 * Requires:
 * \li	'view' is valid.
 */

void
dns_view_setresstats(dns_view_t *view, isc_stats_t *stats);
/*%<
 * Set a general resolver statistics counter set 'stats' for 'view'.
 *
 * Requires:
 * \li	'view' is valid and is not frozen.
 *
 *\li	stats is a valid statistics supporting resolver statistics counters
 *	(see dns/stats.h).
 */

void
dns_view_getresstats(dns_view_t *view, isc_stats_t **statsp);
/*%<
 * Get the general statistics counter set for 'view'.  If a statistics set is
 * set '*statsp' will be attached to the set; otherwise, '*statsp' will be
 * untouched.
 *
 * Requires:
 * \li	'view' is valid and is not frozen.
 *
 *\li	'statsp' != NULL && '*statsp' != NULL
 */

void
dns_view_setresquerystats(dns_view_t *view, dns_stats_t *stats);
/*%<
 * Set a statistics counter set of rdata type, 'stats', for 'view'.  Once the
 * statistic set is installed, view's resolver will count outgoing queries
 * per rdata type.
 *
 * Requires:
 * \li	'view' is valid and is not frozen.
 *
 *\li	stats is a valid statistics created by dns_rdatatypestats_create().
 */

void
dns_view_getresquerystats(dns_view_t *view, dns_stats_t **statsp);
/*%<
 * Get the rdatatype statistics counter set for 'view'.  If a statistics set is
 * set '*statsp' will be attached to the set; otherwise, '*statsp' will be
 * untouched.
 *
 * Requires:
 * \li	'view' is valid and is not frozen.
 *
 *\li	'statsp' != NULL && '*statsp' != NULL
 */

isc_boolean_t
dns_view_iscacheshared(dns_view_t *view);
/*%<
 * Check if the view shares the cache created by another view.
 *
 * Requires:
 * \li	'view' is valid.
 *
 * Returns:
 *\li	#ISC_TRUE if the cache is shared.
 *\li	#ISC_FALSE otherwise.
 */

isc_result_t
dns_view_initsecroots(dns_view_t *view, isc_mem_t *mctx);
/*%<
 * Initialize security roots for the view.  (Note that secroots is
 * NULL until this function is called, so any function using
 * secroots must check its validity first.  One way to do this is
 * use dns_view_getsecroots() and check its return value.)
 *
 * Requires:
 * \li	'view' is valid.
 * \li	'view->secroots' is NULL.
 *
 * Returns:
 *\li	ISC_R_SUCCESS
 *\li	Any other result indicates failure
 */

isc_result_t
dns_view_getsecroots(dns_view_t *view, dns_keytable_t **ktp);
/*%<
 * Get the security roots for this view.  Returns ISC_R_NOTFOUND if
 * the security roots keytable has not been initialized for the view.
 *
 * '*ktp' is attached on success; the caller is responsible for
 * detaching it with dns_keytable_detach().
 *
 * Requires:
 * \li	'view' is valid.
 * \li	'ktp' is not NULL and '*ktp' is NULL.
 *
 * Returns:
 *\li	ISC_R_SUCCESS
 *\li	ISC_R_NOTFOUND
 */

isc_result_t
dns_view_issecuredomain(dns_view_t *view, dns_name_t *name,
			 isc_boolean_t *secure_domain);
/*%<
 * Is 'name' at or beneath a trusted key?  Put answer in
 * '*secure_domain'.
 *
 * Requires:
 * \li	'view' is valid.
 *
 * Returns:
 *\li	ISC_R_SUCCESS
 *\li	Any other value indicates failure
 */

void
dns_view_untrust(dns_view_t *view, dns_name_t *keyname,
		 dns_rdata_dnskey_t *dnskey, isc_mem_t *mctx);
/*%<
 * Remove keys that match 'keyname' and 'dnskey' from the views trust
 * anchors.
 *
 * Requires:
 * \li	'view' is valid.
 * \li	'keyname' is valid.
 * \li	'mctx' is valid.
 * \li	'dnskey' is valid.
 */

void
dns_view_setnewzones(dns_view_t *view, isc_boolean_t allow, void *cfgctx,
		     void (*cfg_destroy)(void **));
/*%<
 * Set whether or not to allow zones to be created or deleted at runtime.
 *
 * If 'allow' is ISC_TRUE, determines the filename into which new zone
 * configuration will be written.  Preserves the configuration context
 * (a pointer to which is passed in 'cfgctx') for use when parsing new
 * zone configuration.  'cfg_destroy' points to a callback routine to
 * destroy the configuration context when the view is destroyed.  (This
 * roundabout method is used in order to avoid libdns having a dependency
 * on libisccfg and libbind9.)
 *
 * If 'allow' is ISC_FALSE, removes any existing references to
 * configuration context and frees any memory.
 *
 * Requires:
 * \li 'view' is valid.
 */

void
dns_view_restorekeyring(dns_view_t *view);

ISC_LANG_ENDDECLS

#endif /* DNS_VIEW_H */
