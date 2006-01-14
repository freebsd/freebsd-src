/*
 * Copyright (C) 2004, 2005  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2003  Internet Software Consortium.
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

/* $Id: rbtdb.c,v 1.168.2.11.2.22 2005/10/14 01:38:48 marka Exp $ */

/*
 * Principal Author: Bob Halley
 */

#include <config.h>

#include <isc/event.h>
#include <isc/mem.h>
#include <isc/print.h>
#include <isc/mutex.h>
#include <isc/random.h>
#include <isc/refcount.h>
#include <isc/rwlock.h>
#include <isc/string.h>
#include <isc/task.h>
#include <isc/util.h>

#include <dns/db.h>
#include <dns/dbiterator.h>
#include <dns/events.h>
#include <dns/fixedname.h>
#include <dns/log.h>
#include <dns/masterdump.h>
#include <dns/rbt.h>
#include <dns/rdata.h>
#include <dns/rdataset.h>
#include <dns/rdatasetiter.h>
#include <dns/rdataslab.h>
#include <dns/result.h>
#include <dns/zonekey.h>

#ifdef DNS_RBTDB_VERSION64
#include "rbtdb64.h"
#else
#include "rbtdb.h"
#endif

#ifdef DNS_RBTDB_VERSION64
#define RBTDB_MAGIC			ISC_MAGIC('R', 'B', 'D', '8')
#else
#define RBTDB_MAGIC			ISC_MAGIC('R', 'B', 'D', '4')
#endif

/*
 * Note that "impmagic" is not the first four bytes of the struct, so
 * ISC_MAGIC_VALID cannot be used.
 */
#define VALID_RBTDB(rbtdb)	((rbtdb) != NULL && \
				 (rbtdb)->common.impmagic == RBTDB_MAGIC)

#ifdef DNS_RBTDB_VERSION64
typedef isc_uint64_t			rbtdb_serial_t;
/*
 * Make casting easier in symbolic debuggers by using different names
 * for the 64 bit version.
 */
#define dns_rbtdb_t dns_rbtdb64_t
#define rdatasetheader_t rdatasetheader64_t
#define rbtdb_version_t rbtdb_version64_t
#else
typedef isc_uint32_t			rbtdb_serial_t;
#endif

typedef isc_uint32_t			rbtdb_rdatatype_t;

#define RBTDB_RDATATYPE_BASE(type)	((dns_rdatatype_t)((type) & 0xFFFF))
#define RBTDB_RDATATYPE_EXT(type)	((dns_rdatatype_t)((type) >> 16))
#define RBTDB_RDATATYPE_VALUE(b, e)	(((e) << 16) | (b))

#define RBTDB_RDATATYPE_SIGNSEC \
		RBTDB_RDATATYPE_VALUE(dns_rdatatype_rrsig, dns_rdatatype_nsec)
#define RBTDB_RDATATYPE_SIGNS \
		RBTDB_RDATATYPE_VALUE(dns_rdatatype_rrsig, dns_rdatatype_ns)
#define RBTDB_RDATATYPE_SIGCNAME \
		RBTDB_RDATATYPE_VALUE(dns_rdatatype_rrsig, dns_rdatatype_cname)
#define RBTDB_RDATATYPE_SIGDNAME \
		RBTDB_RDATATYPE_VALUE(dns_rdatatype_rrsig, dns_rdatatype_dname)
#define RBTDB_RDATATYPE_NCACHEANY \
		RBTDB_RDATATYPE_VALUE(0, dns_rdatatype_any)

/*
 * Allow clients with a virtual time of upto 5 minutes in the past to see
 * records that would have otherwise have expired.
 */
#define RBTDB_VIRTUAL 300

struct noqname {
	dns_name_t name;
	void *	   nsec;
	void *	   nsecsig;
};

typedef struct rdatasetheader {
	/*
	 * Locked by the owning node's lock.
	 */
	rbtdb_serial_t			serial;
	dns_ttl_t			ttl;
	rbtdb_rdatatype_t		type;
	isc_uint16_t			attributes;
	dns_trust_t			trust;
	struct noqname			*noqname;
	/*
	 * We don't use the LIST macros, because the LIST structure has
	 * both head and tail pointers, and is doubly linked.
	 */

	struct rdatasetheader		*next;
	/*
	 * If this is the top header for an rdataset, 'next' points
	 * to the top header for the next rdataset (i.e., the next type).
	 * Otherwise, it points up to the header whose down pointer points
	 * at this header.
	 */
	  
	struct rdatasetheader		*down;
	/*
	 * Points to the header for the next older version of
	 * this rdataset.
	 */

	isc_uint32_t			count;
	/*
	 * Monotonously increased every time this rdataset is bound so that
	 * it is used as the base of the starting point in DNS responses
	 * when the "cyclic" rrset-order is required.  Since the ordering
	 * should not be so crucial, no lock is set for the counter for
	 * performance reasons.
	 */
} rdatasetheader_t;

#define RDATASET_ATTR_NONEXISTENT	0x0001
#define RDATASET_ATTR_STALE		0x0002
#define RDATASET_ATTR_IGNORE		0x0004
#define RDATASET_ATTR_RETAIN		0x0008
#define RDATASET_ATTR_NXDOMAIN		0x0010

/*
 * XXX
 * When the cache will pre-expire data (due to memory low or other
 * situations) before the rdataset's TTL has expired, it MUST
 * respect the RETAIN bit and not expire the data until its TTL is
 * expired.
 */

#undef IGNORE			/* WIN32 winbase.h defines this. */

#define EXISTS(header) \
	(((header)->attributes & RDATASET_ATTR_NONEXISTENT) == 0)
#define NONEXISTENT(header) \
	(((header)->attributes & RDATASET_ATTR_NONEXISTENT) != 0)
#define IGNORE(header) \
	(((header)->attributes & RDATASET_ATTR_IGNORE) != 0)
#define RETAIN(header) \
	(((header)->attributes & RDATASET_ATTR_RETAIN) != 0)
#define NXDOMAIN(header) \
	(((header)->attributes & RDATASET_ATTR_NXDOMAIN) != 0)

#define DEFAULT_NODE_LOCK_COUNT		7		/* Should be prime. */

typedef struct {
	isc_mutex_t			lock;
	/* Locked by lock. */
	unsigned int			references;
	isc_boolean_t			exiting;
} rbtdb_nodelock_t;

typedef struct rbtdb_changed {
	dns_rbtnode_t *			node;
	isc_boolean_t			dirty;
	ISC_LINK(struct rbtdb_changed)	link;
} rbtdb_changed_t;

typedef ISC_LIST(rbtdb_changed_t)	rbtdb_changedlist_t;

typedef struct rbtdb_version {
	/* Not locked */
	rbtdb_serial_t			serial;
	/* Locked by database lock. */
	isc_boolean_t			writer;
	unsigned int			references;
	isc_boolean_t			commit_ok;
	rbtdb_changedlist_t		changed_list;
	ISC_LINK(struct rbtdb_version)	link;
} rbtdb_version_t;

typedef ISC_LIST(rbtdb_version_t)	rbtdb_versionlist_t;

typedef struct {
	/* Unlocked. */
	dns_db_t			common;
	isc_mutex_t			lock;
	isc_rwlock_t			tree_lock;
	unsigned int			node_lock_count;
	rbtdb_nodelock_t *	       	node_locks;
	dns_rbtnode_t *			origin_node;
	/* Locked by lock. */
	unsigned int			active;
	isc_refcount_t			references;
	unsigned int			attributes;
	rbtdb_serial_t			current_serial;
	rbtdb_serial_t			least_serial;
	rbtdb_serial_t			next_serial;
	rbtdb_version_t *		current_version;
	rbtdb_version_t *		future_version;
	rbtdb_versionlist_t		open_versions;
	isc_boolean_t			overmem;
	isc_task_t *			task;
	/* Locked by tree_lock. */
	dns_rbt_t *			tree;
	isc_boolean_t			secure;
} dns_rbtdb_t;

#define RBTDB_ATTR_LOADED		0x01
#define RBTDB_ATTR_LOADING		0x02

/*
 * Search Context
 */
typedef struct {
	dns_rbtdb_t *		rbtdb;
	rbtdb_version_t *	rbtversion;
	rbtdb_serial_t		serial;
	unsigned int		options;
	dns_rbtnodechain_t	chain;
	isc_boolean_t		copy_name;
	isc_boolean_t		need_cleanup;
	isc_boolean_t		wild;
	dns_rbtnode_t *	       	zonecut;
	rdatasetheader_t *	zonecut_rdataset;
	rdatasetheader_t *	zonecut_sigrdataset;
	dns_fixedname_t		zonecut_name;
	isc_stdtime_t		now;
} rbtdb_search_t;

/*
 * Load Context
 */
typedef struct {
	dns_rbtdb_t *		rbtdb;
	isc_stdtime_t		now;
} rbtdb_load_t;

static void rdataset_disassociate(dns_rdataset_t *rdataset);
static isc_result_t rdataset_first(dns_rdataset_t *rdataset);
static isc_result_t rdataset_next(dns_rdataset_t *rdataset);
static void rdataset_current(dns_rdataset_t *rdataset, dns_rdata_t *rdata);
static void rdataset_clone(dns_rdataset_t *source, dns_rdataset_t *target);
static unsigned int rdataset_count(dns_rdataset_t *rdataset);
static isc_result_t rdataset_getnoqname(dns_rdataset_t *rdataset,
				        dns_name_t *name,
					dns_rdataset_t *nsec,
					dns_rdataset_t *nsecsig);

static dns_rdatasetmethods_t rdataset_methods = {
	rdataset_disassociate,
	rdataset_first,
	rdataset_next,
	rdataset_current,
	rdataset_clone,
	rdataset_count,
	NULL,
	rdataset_getnoqname
};

static void rdatasetiter_destroy(dns_rdatasetiter_t **iteratorp);
static isc_result_t rdatasetiter_first(dns_rdatasetiter_t *iterator);
static isc_result_t rdatasetiter_next(dns_rdatasetiter_t *iterator);
static void rdatasetiter_current(dns_rdatasetiter_t *iterator,
				 dns_rdataset_t *rdataset);

static dns_rdatasetitermethods_t rdatasetiter_methods = {
	rdatasetiter_destroy,
	rdatasetiter_first,
	rdatasetiter_next,
	rdatasetiter_current
};

typedef struct rbtdb_rdatasetiter {
	dns_rdatasetiter_t		common;
	rdatasetheader_t *		current;
} rbtdb_rdatasetiter_t;

static void		dbiterator_destroy(dns_dbiterator_t **iteratorp);
static isc_result_t	dbiterator_first(dns_dbiterator_t *iterator);
static isc_result_t	dbiterator_last(dns_dbiterator_t *iterator);
static isc_result_t	dbiterator_seek(dns_dbiterator_t *iterator,
					dns_name_t *name);
static isc_result_t	dbiterator_prev(dns_dbiterator_t *iterator);
static isc_result_t	dbiterator_next(dns_dbiterator_t *iterator);
static isc_result_t	dbiterator_current(dns_dbiterator_t *iterator,
					   dns_dbnode_t **nodep,
					   dns_name_t *name);
static isc_result_t	dbiterator_pause(dns_dbiterator_t *iterator);
static isc_result_t	dbiterator_origin(dns_dbiterator_t *iterator,
					  dns_name_t *name);

static dns_dbiteratormethods_t dbiterator_methods = {
	dbiterator_destroy,
	dbiterator_first,
	dbiterator_last,
	dbiterator_seek,
	dbiterator_prev,
	dbiterator_next,
	dbiterator_current,
	dbiterator_pause,
	dbiterator_origin
};

#define DELETION_BATCH_MAX 64

/*
 * If 'paused' is ISC_TRUE, then the tree lock is not being held.
 */
typedef struct rbtdb_dbiterator {
	dns_dbiterator_t		common;
	isc_boolean_t			paused;
	isc_boolean_t			new_origin;
	isc_rwlocktype_t		tree_locked;
	isc_result_t			result;
	dns_fixedname_t			name;
	dns_fixedname_t			origin;
	dns_rbtnodechain_t		chain;
	dns_rbtnode_t			*node;
	dns_rbtnode_t			*deletions[DELETION_BATCH_MAX];
	int				delete;
} rbtdb_dbiterator_t;


#define IS_STUB(rbtdb)  (((rbtdb)->common.attributes & DNS_DBATTR_STUB)  != 0)
#define IS_CACHE(rbtdb) (((rbtdb)->common.attributes & DNS_DBATTR_CACHE) != 0)

static void free_rbtdb(dns_rbtdb_t *rbtdb, isc_boolean_t log,
		       isc_event_t *event);

/*
 * Locking
 *
 * If a routine is going to lock more than one lock in this module, then
 * the locking must be done in the following order:
 *
 *	Tree Lock
 *
 *	Node Lock	(Only one from the set may be locked at one time by
 *			 any caller)
 *
 *	Database Lock
 *
 * Failure to follow this hierarchy can result in deadlock.
 */

/*
 * Deleting Nodes
 *
 * Currently there is no deletion of nodes from the database, except when
 * the database is being destroyed.
 *
 * If node deletion is added in the future, then for zone databases the node
 * for the origin of the zone MUST NOT be deleted.
 */


/*
 * DB Routines
 */

static void
attach(dns_db_t *source, dns_db_t **targetp) {
	dns_rbtdb_t *rbtdb = (dns_rbtdb_t *)source;

	REQUIRE(VALID_RBTDB(rbtdb));

	isc_refcount_increment(&rbtdb->references, NULL);

	*targetp = source;
}

static void
free_rbtdb_callback(isc_task_t *task, isc_event_t *event) {
	dns_rbtdb_t *rbtdb = event->ev_arg;

	UNUSED(task);

	free_rbtdb(rbtdb, ISC_TRUE, event);
}

static void
free_rbtdb(dns_rbtdb_t *rbtdb, isc_boolean_t log, isc_event_t *event) {
	unsigned int i;
	isc_ondestroy_t ondest;
	isc_result_t result;
	char buf[DNS_NAME_FORMATSIZE];

	REQUIRE(EMPTY(rbtdb->open_versions));
	REQUIRE(rbtdb->future_version == NULL);

	if (rbtdb->current_version != NULL)
		isc_mem_put(rbtdb->common.mctx, rbtdb->current_version,
			    sizeof(rbtdb_version_t));
 again:
	if (rbtdb->tree != NULL) {
		result = dns_rbt_destroy2(&rbtdb->tree,
					  (rbtdb->task != NULL) ? 1000 : 0);
		if (result == ISC_R_QUOTA) {
			INSIST(rbtdb->task != NULL);
			if (event == NULL)
				event = isc_event_allocate(rbtdb->common.mctx,
							   NULL,
						         DNS_EVENT_FREESTORAGE,
							   free_rbtdb_callback,
							   rbtdb,
							   sizeof(isc_event_t));
			if (event == NULL)
				goto again;
			isc_task_send(rbtdb->task, &event);
			return;
		}
		INSIST(result == ISC_R_SUCCESS && rbtdb->tree == NULL);
	}
	if (event != NULL)
		isc_event_free(&event);
	if (log) {
		if (dns_name_dynamic(&rbtdb->common.origin))
			dns_name_format(&rbtdb->common.origin, buf,
					sizeof(buf));
		else
			strcpy(buf, "<UNKNOWN>");
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_CACHE, ISC_LOG_DEBUG(1),
			      "done free_rbtdb(%s)", buf);
	}
	if (dns_name_dynamic(&rbtdb->common.origin))
		dns_name_free(&rbtdb->common.origin, rbtdb->common.mctx);
	for (i = 0; i < rbtdb->node_lock_count; i++)
		DESTROYLOCK(&rbtdb->node_locks[i].lock);
	isc_mem_put(rbtdb->common.mctx, rbtdb->node_locks,
		    rbtdb->node_lock_count * sizeof(rbtdb_nodelock_t));
	isc_rwlock_destroy(&rbtdb->tree_lock);
	isc_refcount_destroy(&rbtdb->references);
	if (rbtdb->task != NULL)
		isc_task_detach(&rbtdb->task);
	DESTROYLOCK(&rbtdb->lock);
	rbtdb->common.magic = 0;
	rbtdb->common.impmagic = 0;
	ondest = rbtdb->common.ondest;
	isc_mem_putanddetach(&rbtdb->common.mctx, rbtdb, sizeof(*rbtdb));
	isc_ondestroy_notify(&ondest, rbtdb);
}

static inline void
maybe_free_rbtdb(dns_rbtdb_t *rbtdb) {
	isc_boolean_t want_free = ISC_FALSE;
	unsigned int i;
	unsigned int inactive = 0;

	/* XXX check for open versions here */

	/*
	 * Even though there are no external direct references, there still
	 * may be nodes in use.
	 */
	for (i = 0; i < rbtdb->node_lock_count; i++) {
		LOCK(&rbtdb->node_locks[i].lock);
		rbtdb->node_locks[i].exiting = ISC_TRUE;
		if (rbtdb->node_locks[i].references == 0)
			inactive++;
		UNLOCK(&rbtdb->node_locks[i].lock);
	}

	if (inactive != 0) {
		LOCK(&rbtdb->lock);
		rbtdb->active -= inactive;
		if (rbtdb->active == 0)
			want_free = ISC_TRUE;
		UNLOCK(&rbtdb->lock);
		if (want_free) {
			char buf[DNS_NAME_FORMATSIZE];
			if (dns_name_dynamic(&rbtdb->common.origin))
				dns_name_format(&rbtdb->common.origin, buf,
						sizeof(buf));
			else
				strcpy(buf, "<UNKNOWN>");
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
				      DNS_LOGMODULE_CACHE, ISC_LOG_DEBUG(1),
				      "calling free_rbtdb(%s)", buf);
			free_rbtdb(rbtdb, ISC_TRUE, NULL);
		}
	}
}

static void
detach(dns_db_t **dbp) {
	dns_rbtdb_t *rbtdb = (dns_rbtdb_t *)(*dbp);
	unsigned int refs;

	REQUIRE(VALID_RBTDB(rbtdb));

	isc_refcount_decrement(&rbtdb->references, &refs);

	if (refs == 0)
		maybe_free_rbtdb(rbtdb);

	*dbp = NULL;
}

static void
currentversion(dns_db_t *db, dns_dbversion_t **versionp) {
	dns_rbtdb_t *rbtdb = (dns_rbtdb_t *)db;
	rbtdb_version_t *version;

	REQUIRE(VALID_RBTDB(rbtdb));

	LOCK(&rbtdb->lock);
	version = rbtdb->current_version;
	if (version->references == 0)
		PREPEND(rbtdb->open_versions, version, link);
	version->references++;
	UNLOCK(&rbtdb->lock);

	*versionp = (dns_dbversion_t *)version;
}

static inline rbtdb_version_t *
allocate_version(isc_mem_t *mctx, rbtdb_serial_t serial,
		 unsigned int references, isc_boolean_t writer)
{
	rbtdb_version_t *version;

	version = isc_mem_get(mctx, sizeof(*version));
	if (version == NULL)
		return (NULL);
	version->serial = serial;
	version->references = references;
	version->writer = writer;
	version->commit_ok = ISC_FALSE;
	ISC_LIST_INIT(version->changed_list);
	ISC_LINK_INIT(version, link);

	return (version);
}

static isc_result_t
newversion(dns_db_t *db, dns_dbversion_t **versionp) {
	dns_rbtdb_t *rbtdb = (dns_rbtdb_t *)db;
	rbtdb_version_t *version;

	REQUIRE(VALID_RBTDB(rbtdb));
	REQUIRE(versionp != NULL && *versionp == NULL);
	REQUIRE(rbtdb->future_version == NULL);

	LOCK(&rbtdb->lock);
	RUNTIME_CHECK(rbtdb->next_serial != 0);		/* XXX Error? */
	version = allocate_version(rbtdb->common.mctx, rbtdb->next_serial, 1,
				   ISC_TRUE);
	if (version != NULL) {
		version->commit_ok = ISC_TRUE;
		rbtdb->next_serial++;
		rbtdb->future_version = version;
	}
	UNLOCK(&rbtdb->lock);

	if (version == NULL)
		return (ISC_R_NOMEMORY);

	*versionp = version;

	return (ISC_R_SUCCESS);
}

static void
attachversion(dns_db_t *db, dns_dbversion_t *source,
	      dns_dbversion_t **targetp)
{
	dns_rbtdb_t *rbtdb = (dns_rbtdb_t *)db;
	rbtdb_version_t *rbtversion = source;

	REQUIRE(VALID_RBTDB(rbtdb));

	LOCK(&rbtdb->lock);

	INSIST(rbtversion->references > 0);
	rbtversion->references++;
	INSIST(rbtversion->references != 0);

	UNLOCK(&rbtdb->lock);

	*targetp = rbtversion;
}

static rbtdb_changed_t *
add_changed(dns_rbtdb_t *rbtdb, rbtdb_version_t *version,
	    dns_rbtnode_t *node)
{
	rbtdb_changed_t *changed;

	/*
	 * Caller must be holding the node lock.
	 */

	changed = isc_mem_get(rbtdb->common.mctx, sizeof(*changed));

	LOCK(&rbtdb->lock);

	REQUIRE(version->writer);

	if (changed != NULL) {
		INSIST(node->references > 0);
		node->references++;
		INSIST(node->references != 0);
		changed->node = node;
		changed->dirty = ISC_FALSE;
		ISC_LIST_INITANDAPPEND(version->changed_list, changed, link);
	} else
		version->commit_ok = ISC_FALSE;

	UNLOCK(&rbtdb->lock);

	return (changed);
}

static inline void
free_noqname(isc_mem_t *mctx, struct noqname **noqname) {

	if (dns_name_dynamic(&(*noqname)->name))
		dns_name_free(&(*noqname)->name, mctx);
	if ((*noqname)->nsec != NULL)
		isc_mem_put(mctx, (*noqname)->nsec,
			    dns_rdataslab_size((*noqname)->nsec, 0));
	if ((*noqname)->nsec != NULL)
		isc_mem_put(mctx, (*noqname)->nsecsig,
			    dns_rdataslab_size((*noqname)->nsecsig, 0));
	isc_mem_put(mctx, *noqname, sizeof(**noqname));
	*noqname = NULL;
}

static inline void
free_rdataset(isc_mem_t *mctx, rdatasetheader_t *rdataset) {
	unsigned int size;

	if (rdataset->noqname != NULL)
		free_noqname(mctx, &rdataset->noqname);
	
	if ((rdataset->attributes & RDATASET_ATTR_NONEXISTENT) != 0)
		size = sizeof(*rdataset);
	else
		size = dns_rdataslab_size((unsigned char *)rdataset,
					  sizeof(*rdataset));
	isc_mem_put(mctx, rdataset, size);
}

static inline void
rollback_node(dns_rbtnode_t *node, rbtdb_serial_t serial) {
	rdatasetheader_t *header, *dcurrent;
	isc_boolean_t make_dirty = ISC_FALSE;

	/*
	 * Caller must hold the node lock.
	 */

	/*
	 * We set the IGNORE attribute on rdatasets with serial number
	 * 'serial'.  When the reference count goes to zero, these rdatasets
	 * will be cleaned up; until that time, they will be ignored.
	 */
	for (header = node->data; header != NULL; header = header->next) {
		if (header->serial == serial) {
			header->attributes |= RDATASET_ATTR_IGNORE;
			make_dirty = ISC_TRUE;
		}
		for (dcurrent = header->down;
		     dcurrent != NULL;
		     dcurrent = dcurrent->down) {
			if (dcurrent->serial == serial) {
				dcurrent->attributes |= RDATASET_ATTR_IGNORE;
				make_dirty = ISC_TRUE;
			}
		}
	}
	if (make_dirty)
		node->dirty = 1;
}

static inline void
clean_cache_node(dns_rbtdb_t *rbtdb, dns_rbtnode_t *node) {
	rdatasetheader_t *current, *dcurrent, *top_prev, *top_next, *down_next;
	isc_mem_t *mctx = rbtdb->common.mctx;

	/*
	 * Caller must be holding the node lock.
	 */

	top_prev = NULL;
	for (current = node->data; current != NULL; current = top_next) {
		top_next = current->next;
		dcurrent = current->down;
		if (dcurrent != NULL) {
			do {
				down_next = dcurrent->down;
				free_rdataset(mctx, dcurrent);
				dcurrent = down_next;
			} while (dcurrent != NULL);
			current->down = NULL;
		}
		/*
		 * If current is nonexistent or stale, we can clean it up.
		 */
		if ((current->attributes &
		     (RDATASET_ATTR_NONEXISTENT|RDATASET_ATTR_STALE)) != 0) {
			if (top_prev != NULL)
				top_prev->next = current->next;
			else
				node->data = current->next;
			free_rdataset(mctx, current);
		} else
			top_prev = current;
	}
	node->dirty = 0;
}

static inline void
clean_zone_node(dns_rbtdb_t *rbtdb, dns_rbtnode_t *node,
		rbtdb_serial_t least_serial)
{
	rdatasetheader_t *current, *dcurrent, *down_next, *dparent;
	rdatasetheader_t *top_prev, *top_next;
	isc_mem_t *mctx = rbtdb->common.mctx;
	isc_boolean_t still_dirty = ISC_FALSE;

	/*
	 * Caller must be holding the node lock.
	 */
	REQUIRE(least_serial != 0);

	top_prev = NULL;
	for (current = node->data; current != NULL; current = top_next) {
		top_next = current->next;

		/*
		 * First, we clean up any instances of multiple rdatasets
		 * with the same serial number, or that have the IGNORE
		 * attribute.
		 */
		dparent = current;
		for (dcurrent = current->down;
		     dcurrent != NULL;
		     dcurrent = down_next) {
			down_next = dcurrent->down;
			INSIST(dcurrent->serial <= dparent->serial);
			if (dcurrent->serial == dparent->serial ||
			    IGNORE(dcurrent)) {
				if (down_next != NULL)
					down_next->next = dparent;
				dparent->down = down_next;
				free_rdataset(mctx, dcurrent);
			} else
				dparent = dcurrent;
		}

		/*
		 * We've now eliminated all IGNORE datasets with the possible
		 * exception of current, which we now check.
		 */
		if (IGNORE(current)) {
			down_next = current->down;
			if (down_next == NULL) {
				if (top_prev != NULL)
					top_prev->next = current->next;
				else
					node->data = current->next;
				free_rdataset(mctx, current);
				/*
				 * current no longer exists, so we can
				 * just continue with the loop.
				 */
				continue;
			} else {
				/*
				 * Pull up current->down, making it the new
				 * current.
				 */
				if (top_prev != NULL)
					top_prev->next = down_next;
				else
					node->data = down_next;
				down_next->next = top_next;
				free_rdataset(mctx, current);
				current = down_next;
			}
		}

		/*
		 * We now try to find the first down node less than the
		 * least serial.
		 */
		dparent = current;
		for (dcurrent = current->down;
		     dcurrent != NULL;
		     dcurrent = down_next) {
			down_next = dcurrent->down;
			if (dcurrent->serial < least_serial)
				break;
			dparent = dcurrent;
		}

		/*
		 * If there is a such an rdataset, delete it and any older
		 * versions.
		 */
		if (dcurrent != NULL) {
			do {
				down_next = dcurrent->down;
				INSIST(dcurrent->serial <= least_serial);
				free_rdataset(mctx, dcurrent);
				dcurrent = down_next;
			} while (dcurrent != NULL);
			dparent->down = NULL;
		}

		/*
		 * Note.  The serial number of 'current' might be less than
		 * least_serial too, but we cannot delete it because it is
		 * the most recent version, unless it is a NONEXISTENT
		 * rdataset.
		 */
		if (current->down != NULL) {
			still_dirty = ISC_TRUE;
			top_prev = current;
		} else {
			/*
			 * If this is a NONEXISTENT rdataset, we can delete it.
			 */
			if (NONEXISTENT(current)) {
				if (top_prev != NULL)
					top_prev->next = current->next;
				else
					node->data = current->next;
				free_rdataset(mctx, current);
			} else
				top_prev = current;
		}
	}
	if (!still_dirty)
		node->dirty = 0;
}

static inline void
new_reference(dns_rbtdb_t *rbtdb, dns_rbtnode_t *node) {
	if (node->references == 0) {
		rbtdb->node_locks[node->locknum].references++;
		INSIST(rbtdb->node_locks[node->locknum].references != 0);
	}
	node->references++;
	INSIST(node->references != 0);
}

static void
no_references(dns_rbtdb_t *rbtdb, dns_rbtnode_t *node,
	      rbtdb_serial_t least_serial, isc_rwlocktype_t lock)
{
	isc_result_t result;
	isc_boolean_t write_locked;
	unsigned int locknum;

	/*
	 * Caller must be holding the node lock.
	 */

	REQUIRE(node->references == 0);

	if (node->dirty) {
		if (IS_CACHE(rbtdb))
			clean_cache_node(rbtdb, node);
		else {
			if (least_serial == 0) {
				/*
				 * Caller doesn't know the least serial.
				 * Get it.
				 */
				LOCK(&rbtdb->lock);
				least_serial = rbtdb->least_serial;
				UNLOCK(&rbtdb->lock);
			}
			clean_zone_node(rbtdb, node, least_serial);
		}
	}

	locknum = node->locknum;

	INSIST(rbtdb->node_locks[locknum].references > 0);
	rbtdb->node_locks[locknum].references--;

	/*
	 * XXXDCL should this only be done for cache zones?
	 */
	if (node->data != NULL || node->down != NULL)
		return;

	/*
	 * XXXDCL need to add a deferred delete method for ISC_R_LOCKBUSY.
	 */
	if (lock != isc_rwlocktype_write) {
		/*
		 * Locking hierarchy notwithstanding, we don't need to free
		 * the node lock before acquiring the tree write lock because
		 * we only do a trylock.
		 */
		if (lock == isc_rwlocktype_read)
			result = isc_rwlock_tryupgrade(&rbtdb->tree_lock);
		else
			result = isc_rwlock_trylock(&rbtdb->tree_lock,
						    isc_rwlocktype_write);
		RUNTIME_CHECK(result == ISC_R_SUCCESS ||
			      result == ISC_R_LOCKBUSY);
 
		write_locked = ISC_TF(result == ISC_R_SUCCESS);
	} else
		write_locked = ISC_TRUE;

	if (write_locked) {
		if (isc_log_wouldlog(dns_lctx, ISC_LOG_DEBUG(1))) {
			char printname[DNS_NAME_FORMATSIZE];

			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
				      DNS_LOGMODULE_CACHE, ISC_LOG_DEBUG(1),
				      "no_references: delete from rbt: %p %s",
				      node,
				      dns_rbt_formatnodename(node, printname,
							   sizeof(printname)));
		}

		result = dns_rbt_deletenode(rbtdb->tree, node, ISC_FALSE);
		if (result != ISC_R_SUCCESS)
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
				      DNS_LOGMODULE_CACHE, ISC_LOG_WARNING,
				      "no_references: dns_rbt_deletenode: %s",
				      isc_result_totext(result));
	}

	/*
	 * Relock a read lock, or unlock the write lock if no lock was held.
	 */
	if (lock == isc_rwlocktype_none)
		if (write_locked)
			RWUNLOCK(&rbtdb->tree_lock, isc_rwlocktype_write);

	if (lock == isc_rwlocktype_read)
		if (write_locked)
			isc_rwlock_downgrade(&rbtdb->tree_lock);
}

static inline void
make_least_version(dns_rbtdb_t *rbtdb, rbtdb_version_t *version,
		   rbtdb_changedlist_t *cleanup_list)
{
	/*
	 * Caller must be holding the database lock.
	 */

	rbtdb->least_serial = version->serial;
	*cleanup_list = version->changed_list;
	ISC_LIST_INIT(version->changed_list);
}

static inline void
cleanup_nondirty(rbtdb_version_t *version, rbtdb_changedlist_t *cleanup_list) {
	rbtdb_changed_t *changed, *next_changed;

	/*
	 * If the changed record is dirty, then
	 * an update created multiple versions of
	 * a given rdataset.  We keep this list
	 * until we're the least open version, at
	 * which point it's safe to get rid of any
	 * older versions.
	 *
	 * If the changed record isn't dirty, then
	 * we don't need it anymore since we're
	 * committing and not rolling back.
	 *
	 * The caller must be holding the database lock.
	 */
	for (changed = HEAD(version->changed_list);
	     changed != NULL;
	     changed = next_changed) {
		next_changed = NEXT(changed, link);
		if (!changed->dirty) {
			UNLINK(version->changed_list,
			       changed, link);
			APPEND(*cleanup_list,
			       changed, link);
		}
	}
}

static void
closeversion(dns_db_t *db, dns_dbversion_t **versionp, isc_boolean_t commit) {
	dns_rbtdb_t *rbtdb = (dns_rbtdb_t *)db;
	rbtdb_version_t *version, *cleanup_version, *least_greater;
	isc_boolean_t rollback = ISC_FALSE;
	rbtdb_changedlist_t cleanup_list;
	rbtdb_changed_t *changed, *next_changed;
	rbtdb_serial_t serial, least_serial;
	dns_rbtnode_t *rbtnode;
	isc_mutex_t *lock;

	REQUIRE(VALID_RBTDB(rbtdb));
	version = (rbtdb_version_t *)*versionp;

	cleanup_version = NULL;
	ISC_LIST_INIT(cleanup_list);

	LOCK(&rbtdb->lock);
	INSIST(version->references > 0);
	INSIST(!version->writer || !(commit && version->references > 1));
	version->references--;
	serial = version->serial;
	if (version->references == 0) {
		if (version->writer) {
			if (commit) {
				INSIST(version->commit_ok);
				INSIST(version == rbtdb->future_version);
				if (EMPTY(rbtdb->open_versions)) {
					/*
					 * We're going to become the least open
					 * version.
					 */
					make_least_version(rbtdb, version,
							   &cleanup_list);
				} else {
					/*
					 * Some other open version is the
					 * least version.  We can't cleanup
					 * records that were changed in this
					 * version because the older versions
					 * may still be in use by an open
					 * version.
					 *
					 * We can, however, discard the
					 * changed records for things that
					 * we've added that didn't exist in
					 * prior versions.
					 */
					cleanup_nondirty(version,
							 &cleanup_list);
				}
				/*
				 * If the (soon to be former) current version
				 * isn't being used by anyone, we can clean
				 * it up.
				 */
				if (rbtdb->current_version->references == 0) {
					cleanup_version =
						rbtdb->current_version;
					APPENDLIST(version->changed_list,
						 cleanup_version->changed_list,
						   link);
				}
				/*
				 * Become the current version.
				 */
				version->writer = ISC_FALSE;
				rbtdb->current_version = version;
				rbtdb->current_serial = version->serial;
				rbtdb->future_version = NULL;
			} else {
				/*
				 * We're rolling back this transaction.
				 */
				cleanup_list = version->changed_list;
				ISC_LIST_INIT(version->changed_list);
				rollback = ISC_TRUE;
				cleanup_version = version;
				rbtdb->future_version = NULL;
			}
		} else {
			if (version != rbtdb->current_version) {
				/*
				 * There are no external or internal references
				 * to this version and it can be cleaned up.
				 */
				cleanup_version = version;

				/*
				 * Find the version with the least serial
				 * number greater than ours.
				 */
				least_greater = PREV(version, link);
				if (least_greater == NULL)
					least_greater = rbtdb->current_version;

				INSIST(version->serial < least_greater->serial);
				/*
				 * Is this the least open version?
				 */
				if (version->serial == rbtdb->least_serial) {
					/*
					 * Yes.  Install the new least open
					 * version.
					 */
					make_least_version(rbtdb,
							   least_greater,
							   &cleanup_list);
				} else {
					/*
					 * Add any unexecuted cleanups to
					 * those of the least greater version.
					 */
					APPENDLIST(least_greater->changed_list,
						   version->changed_list,
						   link);
				}
			} else if (version->serial == rbtdb->least_serial)
				INSIST(EMPTY(version->changed_list));
			UNLINK(rbtdb->open_versions, version, link);
		}
	}
	least_serial = rbtdb->least_serial;
	UNLOCK(&rbtdb->lock);

	if (cleanup_version != NULL) {
		INSIST(EMPTY(cleanup_version->changed_list));
		isc_mem_put(rbtdb->common.mctx, cleanup_version,
			    sizeof(*cleanup_version));
	}

	if (!EMPTY(cleanup_list)) {
		for (changed = HEAD(cleanup_list);
		     changed != NULL;
		     changed = next_changed) {
			next_changed = NEXT(changed, link);
			rbtnode = changed->node;
			lock = &rbtdb->node_locks[rbtnode->locknum].lock;

			LOCK(lock);

			INSIST(rbtnode->references > 0);
			rbtnode->references--;
			if (rollback)
				rollback_node(rbtnode, serial);

			if (rbtnode->references == 0)
				no_references(rbtdb, rbtnode, least_serial,
					      isc_rwlocktype_none);

			UNLOCK(lock);

			isc_mem_put(rbtdb->common.mctx, changed,
				    sizeof(*changed));
		}
	}

	*versionp = NULL;
}

/*
 * Add the necessary magic for the wildcard name 'name'
 * to be found in 'rbtdb'.
 *
 * In order for wildcard matching to work correctly in
 * zone_find(), we must ensure that a node for the wildcarding
 * level exists in the database, and has its 'find_callback'
 * and 'wild' bits set.
 *
 * E.g. if the wildcard name is "*.sub.example." then we
 * must ensure that "sub.example." exists and is marked as
 * a wildcard level.
 */
static isc_result_t
add_wildcard_magic(dns_rbtdb_t *rbtdb, dns_name_t *name) {
	isc_result_t result;
	dns_name_t foundname;
	dns_offsets_t offsets;
	unsigned int n;
	dns_rbtnode_t *node = NULL;

	dns_name_init(&foundname, offsets);
	n = dns_name_countlabels(name);
	INSIST(n >= 2);
	n--;
	dns_name_getlabelsequence(name, 1, n, &foundname);
	result = dns_rbt_addnode(rbtdb->tree, &foundname, &node);
	if (result != ISC_R_SUCCESS && result != ISC_R_EXISTS)
		return (result);
	node->find_callback = 1;
	node->wild = 1;
	return (ISC_R_SUCCESS);
}

static isc_result_t
add_empty_wildcards(dns_rbtdb_t *rbtdb, dns_name_t *name) {
	isc_result_t result;
	dns_name_t foundname;
	dns_offsets_t offsets;
	unsigned int n, l, i;

	dns_name_init(&foundname, offsets);
	n = dns_name_countlabels(name);
	l = dns_name_countlabels(&rbtdb->common.origin);
	i = l + 1;
	while (i < n) {
		dns_rbtnode_t *node = NULL;	/* dummy */
		dns_name_getlabelsequence(name, n - i, i, &foundname);
		if (dns_name_iswildcard(&foundname)) {
			result = add_wildcard_magic(rbtdb, &foundname);
			if (result != ISC_R_SUCCESS)
				return (result);
			result = dns_rbt_addnode(rbtdb->tree, &foundname,
						 &node);
			if (result != ISC_R_SUCCESS && result != ISC_R_EXISTS)
				return (result);
		}
		i++;
	}
	return (ISC_R_SUCCESS);
}

static isc_result_t
findnode(dns_db_t *db, dns_name_t *name, isc_boolean_t create,
	 dns_dbnode_t **nodep)
{
	dns_rbtdb_t *rbtdb = (dns_rbtdb_t *)db;
	dns_rbtnode_t *node = NULL;
	dns_name_t nodename;
	unsigned int locknum;
	isc_result_t result;
	isc_rwlocktype_t locktype = isc_rwlocktype_read;

	REQUIRE(VALID_RBTDB(rbtdb));

	dns_name_init(&nodename, NULL);
	RWLOCK(&rbtdb->tree_lock, locktype);
	result = dns_rbt_findnode(rbtdb->tree, name, NULL, &node, NULL,
				  DNS_RBTFIND_EMPTYDATA, NULL, NULL);
	if (result != ISC_R_SUCCESS) {
		RWUNLOCK(&rbtdb->tree_lock, locktype);
		if (!create) {
			if (result == DNS_R_PARTIALMATCH)
				result = ISC_R_NOTFOUND;
			return (result);
		}
		/*
		 * It would be nice to try to upgrade the lock instead of
		 * unlocking then relocking.
		 */
		locktype = isc_rwlocktype_write;
		RWLOCK(&rbtdb->tree_lock, locktype);
		node = NULL;
		result = dns_rbt_addnode(rbtdb->tree, name, &node);
		if (result == ISC_R_SUCCESS) {
			dns_rbt_namefromnode(node, &nodename);
#ifdef DNS_RBT_USEHASH
			node->locknum = node->hashval % rbtdb->node_lock_count;
#else
			node->locknum = dns_name_hash(&nodename, ISC_TRUE) %
				rbtdb->node_lock_count;
#endif
			add_empty_wildcards(rbtdb, name);

			if (dns_name_iswildcard(name)) {
				result = add_wildcard_magic(rbtdb, name);
				if (result != ISC_R_SUCCESS) {
					RWUNLOCK(&rbtdb->tree_lock, locktype);
					return (result);
				}
			}
		} else if (result != ISC_R_EXISTS) {
			RWUNLOCK(&rbtdb->tree_lock, locktype);
			return (result);
		}
	}
	locknum = node->locknum;
	LOCK(&rbtdb->node_locks[locknum].lock);
	new_reference(rbtdb, node);
	UNLOCK(&rbtdb->node_locks[locknum].lock);
	RWUNLOCK(&rbtdb->tree_lock, locktype);

	*nodep = (dns_dbnode_t *)node;

	return (ISC_R_SUCCESS);
}

static isc_result_t
zone_zonecut_callback(dns_rbtnode_t *node, dns_name_t *name, void *arg) {
	rbtdb_search_t *search = arg;
	rdatasetheader_t *header, *header_next;
	rdatasetheader_t *dname_header, *sigdname_header, *ns_header;
	rdatasetheader_t *found;
	isc_result_t result;
	dns_rbtnode_t *onode;

	/*
	 * We only want to remember the topmost zone cut, since it's the one
	 * that counts, so we'll just continue if we've already found a
	 * zonecut.
	 */
	if (search->zonecut != NULL)
		return (DNS_R_CONTINUE);

	found = NULL;
	result = DNS_R_CONTINUE;
	onode = search->rbtdb->origin_node;

	LOCK(&(search->rbtdb->node_locks[node->locknum].lock));

	/*
	 * Look for an NS or DNAME rdataset active in our version.
	 */
	ns_header = NULL;
	dname_header = NULL;
	sigdname_header = NULL;
	for (header = node->data; header != NULL; header = header_next) {
		header_next = header->next;
		if (header->type == dns_rdatatype_ns ||
		    header->type == dns_rdatatype_dname ||
		    header->type == RBTDB_RDATATYPE_SIGDNAME) {
			do {
				if (header->serial <= search->serial &&
				    !IGNORE(header)) {
					/*
					 * Is this a "this rdataset doesn't
					 * exist" record?
					 */
					if (NONEXISTENT(header))
						header = NULL;
					break;
				} else
					header = header->down;
			} while (header != NULL);
			if (header != NULL) {
				if (header->type == dns_rdatatype_dname)
					dname_header = header;
				else if (header->type == 
					   RBTDB_RDATATYPE_SIGDNAME)
					sigdname_header = header;
				else if (node != onode ||
					 IS_STUB(search->rbtdb)) {
					/*
					 * We've found an NS rdataset that
					 * isn't at the origin node.  We check
					 * that they're not at the origin node,
					 * because otherwise we'd erroneously
					 * treat the zone top as if it were
					 * a delegation.
					 */
					ns_header = header;
				}
			}
		}
	}

	/*
	 * Did we find anything?
	 */
	if (dname_header != NULL) {
		/*
		 * Note that DNAME has precedence over NS if both exist.
		 */
		found = dname_header;
		search->zonecut_sigrdataset = sigdname_header;
	} else if (ns_header != NULL) {
		found = ns_header;
		search->zonecut_sigrdataset = NULL;
	}

	if (found != NULL) {
		/*
		 * We increment the reference count on node to ensure that
		 * search->zonecut_rdataset will still be valid later.
		 */
		new_reference(search->rbtdb, node);
		search->zonecut = node;
		search->zonecut_rdataset = found;
		search->need_cleanup = ISC_TRUE;
		/*
		 * Since we've found a zonecut, anything beneath it is
		 * glue and is not subject to wildcard matching, so we
		 * may clear search->wild.
		 */
		search->wild = ISC_FALSE;
		if ((search->options & DNS_DBFIND_GLUEOK) == 0) {
			/*
			 * If the caller does not want to find glue, then
			 * this is the best answer and the search should
			 * stop now.
			 */
			result = DNS_R_PARTIALMATCH;
		} else {
			dns_name_t *zcname;

			/*
			 * The search will continue beneath the zone cut.
			 * This may or may not be the best match.  In case it
			 * is, we need to remember the node name.
			 */
			zcname = dns_fixedname_name(&search->zonecut_name);
			RUNTIME_CHECK(dns_name_copy(name, zcname, NULL) ==
				      ISC_R_SUCCESS);
			search->copy_name = ISC_TRUE;
		}
	} else {
		/*
		 * There is no zonecut at this node which is active in this
		 * version.
		 *
		 * If this is a "wild" node and the caller hasn't disabled
		 * wildcard matching, remember that we've seen a wild node
		 * in case we need to go searching for wildcard matches
		 * later on.
		 */
		if (node->wild && (search->options & DNS_DBFIND_NOWILD) == 0)
			search->wild = ISC_TRUE;
	}

	UNLOCK(&(search->rbtdb->node_locks[node->locknum].lock));

	return (result);
}

static inline void
bind_rdataset(dns_rbtdb_t *rbtdb, dns_rbtnode_t *node,
	      rdatasetheader_t *header, isc_stdtime_t now,
	      dns_rdataset_t *rdataset)
{
	unsigned char *raw;

	/*
	 * Caller must be holding the node lock.
	 */

	if (rdataset == NULL)
		return;

	new_reference(rbtdb, node);

	INSIST(rdataset->methods == NULL);	/* We must be disassociated. */

	rdataset->methods = &rdataset_methods;
	rdataset->rdclass = rbtdb->common.rdclass;
	rdataset->type = RBTDB_RDATATYPE_BASE(header->type);
	rdataset->covers = RBTDB_RDATATYPE_EXT(header->type);
	rdataset->ttl = header->ttl - now;
	rdataset->trust = header->trust;
	if (NXDOMAIN(header))
		rdataset->attributes |= DNS_RDATASETATTR_NXDOMAIN;
	rdataset->private1 = rbtdb;
	rdataset->private2 = node;
	raw = (unsigned char *)header + sizeof(*header);
	rdataset->private3 = raw;
	rdataset->count = header->count++;
	if (header->count == ISC_UINT32_MAX)
		header->count = 0;

	/*
	 * Reset iterator state.
	 */
	rdataset->privateuint4 = 0;
	rdataset->private5 = NULL;

	/*
	 * Add noqname proof.
	 */
	rdataset->private6 = header->noqname;
	if (rdataset->private6 != NULL)
		rdataset->attributes |=  DNS_RDATASETATTR_NOQNAME;
}

static inline isc_result_t
setup_delegation(rbtdb_search_t *search, dns_dbnode_t **nodep,
		 dns_name_t *foundname, dns_rdataset_t *rdataset,
		 dns_rdataset_t *sigrdataset)
{
	isc_result_t result;
	dns_name_t *zcname;
	rbtdb_rdatatype_t type;
	dns_rbtnode_t *node;

	/*
	 * The caller MUST NOT be holding any node locks.
	 */

	node = search->zonecut;
	type = search->zonecut_rdataset->type;

	/*
	 * If we have to set foundname, we do it before anything else.
	 * If we were to set foundname after we had set nodep or bound the
	 * rdataset, then we'd have to undo that work if dns_name_copy()
	 * failed.  By setting foundname first, there's nothing to undo if
	 * we have trouble.
	 */
	if (foundname != NULL && search->copy_name) {
		zcname = dns_fixedname_name(&search->zonecut_name);
		result = dns_name_copy(zcname, foundname, NULL);
		if (result != ISC_R_SUCCESS)
			return (result);
	}
	if (nodep != NULL) {
		/*
		 * Note that we don't have to increment the node's reference
		 * count here because we're going to use the reference we
		 * already have in the search block.
		 */
		*nodep = node;
		search->need_cleanup = ISC_FALSE;
	}
	if (rdataset != NULL) {
		LOCK(&(search->rbtdb->node_locks[node->locknum].lock));
		bind_rdataset(search->rbtdb, node, search->zonecut_rdataset,
			      search->now, rdataset);
		if (sigrdataset != NULL && search->zonecut_sigrdataset != NULL)
			bind_rdataset(search->rbtdb, node,
				      search->zonecut_sigrdataset,
				      search->now, sigrdataset);
		UNLOCK(&(search->rbtdb->node_locks[node->locknum].lock));
	}

	if (type == dns_rdatatype_dname)
		return (DNS_R_DNAME);
	return (DNS_R_DELEGATION);
}

static inline isc_boolean_t
valid_glue(rbtdb_search_t *search, dns_name_t *name, rbtdb_rdatatype_t type,
	   dns_rbtnode_t *node)
{
	unsigned char *raw;
	unsigned int count, size;
	dns_name_t ns_name;
	isc_boolean_t valid = ISC_FALSE;
	dns_offsets_t offsets;
	isc_region_t region;
	rdatasetheader_t *header;

	/*
	 * No additional locking is required.
	 */

	/*
	 * Valid glue types are A, AAAA, A6.  NS is also a valid glue type
	 * if it occurs at a zone cut, but is not valid below it.
	 */
	if (type == dns_rdatatype_ns) {
		if (node != search->zonecut) {
			return (ISC_FALSE);
		}
	} else if (type != dns_rdatatype_a &&
		   type != dns_rdatatype_aaaa &&
		   type != dns_rdatatype_a6) {
		return (ISC_FALSE);
	}

	header = search->zonecut_rdataset;
	raw = (unsigned char *)header + sizeof(*header);
	count = raw[0] * 256 + raw[1];
	raw += 2;

	while (count > 0) {
		count--;
		size = raw[0] * 256 + raw[1];
		raw += 2;
		region.base = raw;
		region.length = size;
		raw += size;
		/*
		 * XXX Until we have rdata structures, we have no choice but
		 * to directly access the rdata format.
		 */
		dns_name_init(&ns_name, offsets);
		dns_name_fromregion(&ns_name, &region);
		if (dns_name_compare(&ns_name, name) == 0) {
			valid = ISC_TRUE;
			break;
		}
	}

	return (valid);
}

static inline isc_boolean_t
activeempty(rbtdb_search_t *search, dns_rbtnodechain_t *chain,
	    dns_name_t *name)
{
	dns_fixedname_t fnext;
	dns_fixedname_t forigin;
	dns_name_t *next;
	dns_name_t *origin;
	dns_name_t prefix;
	dns_rbtdb_t *rbtdb;
	dns_rbtnode_t *node;
	isc_result_t result;
	isc_boolean_t answer = ISC_FALSE;
	rdatasetheader_t *header;

	rbtdb = search->rbtdb;

	dns_name_init(&prefix, NULL);
	dns_fixedname_init(&fnext);
	next = dns_fixedname_name(&fnext);
	dns_fixedname_init(&forigin);
	origin = dns_fixedname_name(&forigin);

	result = dns_rbtnodechain_next(chain, NULL, NULL);
	while (result == ISC_R_SUCCESS || result == DNS_R_NEWORIGIN) {
		node = NULL;
		result = dns_rbtnodechain_current(chain, &prefix,
						  origin, &node);
		if (result != ISC_R_SUCCESS)
			break;
		LOCK(&(rbtdb->node_locks[node->locknum].lock));
		for (header = node->data;
		     header != NULL;
		     header = header->next) {
			if (header->serial <= search->serial &&
			    !IGNORE(header) && EXISTS(header))
				break;
		}
		UNLOCK(&(rbtdb->node_locks[node->locknum].lock));
		if (header != NULL)
			break;
		result = dns_rbtnodechain_next(chain, NULL, NULL);
	}
	if (result == ISC_R_SUCCESS)
		result = dns_name_concatenate(&prefix, origin, next, NULL);
	if (result == ISC_R_SUCCESS && dns_name_issubdomain(next, name))
		answer = ISC_TRUE;
	return (answer);
}

static inline isc_boolean_t
activeemtpynode(rbtdb_search_t *search, dns_name_t *qname, dns_name_t *wname) {
	dns_fixedname_t fnext;
	dns_fixedname_t forigin;
	dns_fixedname_t fprev;
	dns_name_t *next;
	dns_name_t *origin;
	dns_name_t *prev;
	dns_name_t name;
	dns_name_t rname;
	dns_name_t tname;
	dns_rbtdb_t *rbtdb;
	dns_rbtnode_t *node;
	dns_rbtnodechain_t chain;
	isc_boolean_t check_next = ISC_TRUE;
	isc_boolean_t check_prev = ISC_TRUE;
	isc_boolean_t answer = ISC_FALSE;
	isc_result_t result;
	rdatasetheader_t *header;
	unsigned int n;

	rbtdb = search->rbtdb;

	dns_name_init(&name, NULL);
	dns_name_init(&tname, NULL);
	dns_name_init(&rname, NULL);
	dns_fixedname_init(&fnext);
	next = dns_fixedname_name(&fnext);
	dns_fixedname_init(&fprev);
	prev = dns_fixedname_name(&fprev);
	dns_fixedname_init(&forigin);
	origin = dns_fixedname_name(&forigin);

	/*
	 * Find if qname is at or below a empty node.
	 * Use our own copy of the chain.
	 */

	chain = search->chain;
	do {
		node = NULL;
		result = dns_rbtnodechain_current(&chain, &name,
						  origin, &node);
		if (result != ISC_R_SUCCESS)
			break;
		LOCK(&(rbtdb->node_locks[node->locknum].lock));
		for (header = node->data;
		     header != NULL;
		     header = header->next) {
			if (header->serial <= search->serial &&
			    !IGNORE(header) && EXISTS(header))
				break;
		}
		UNLOCK(&(rbtdb->node_locks[node->locknum].lock));
		if (header != NULL)
			break;
		result = dns_rbtnodechain_prev(&chain, NULL, NULL);
	} while (result == ISC_R_SUCCESS || result == DNS_R_NEWORIGIN);
	if (result == ISC_R_SUCCESS)
		result = dns_name_concatenate(&name, origin, prev, NULL);
	if (result != ISC_R_SUCCESS)
		check_prev = ISC_FALSE;

	result = dns_rbtnodechain_next(&chain, NULL, NULL);
	while (result == ISC_R_SUCCESS || result == DNS_R_NEWORIGIN) {
		node = NULL;
		result = dns_rbtnodechain_current(&chain, &name,
						  origin, &node);
		if (result != ISC_R_SUCCESS)
			break;
		LOCK(&(rbtdb->node_locks[node->locknum].lock));
		for (header = node->data;
		     header != NULL;
		     header = header->next) {
			if (header->serial <= search->serial &&
			    !IGNORE(header) && EXISTS(header))
				break;
		}
		UNLOCK(&(rbtdb->node_locks[node->locknum].lock));
		if (header != NULL)
			break;
		result = dns_rbtnodechain_next(&chain, NULL, NULL);
	}
	if (result == ISC_R_SUCCESS)
		result = dns_name_concatenate(&name, origin, next, NULL);
	if (result != ISC_R_SUCCESS)
		check_next = ISC_FALSE;

	dns_name_clone(qname, &rname);

	/*
	 * Remove the wildcard label to find the terminal name.
	 */
	n = dns_name_countlabels(wname);
	dns_name_getlabelsequence(wname, 1, n - 1, &tname);

	do {
		if ((check_prev && dns_name_issubdomain(prev, &rname)) ||
		    (check_next && dns_name_issubdomain(next, &rname))) {
			answer = ISC_TRUE;
			break;
		}
		/*
		 * Remove the left hand label.
		 */
		n = dns_name_countlabels(&rname);
		dns_name_getlabelsequence(&rname, 1, n - 1, &rname);
	} while (!dns_name_equal(&rname, &tname));
	return (answer);
}

static inline isc_result_t
find_wildcard(rbtdb_search_t *search, dns_rbtnode_t **nodep,
	      dns_name_t *qname)
{
	unsigned int i, j;
	dns_rbtnode_t *node, *level_node, *wnode;
	rdatasetheader_t *header;
	isc_result_t result = ISC_R_NOTFOUND;
	dns_name_t name;
	dns_name_t *wname;
	dns_fixedname_t fwname;
	dns_rbtdb_t *rbtdb;
	isc_boolean_t done, wild, active;
	dns_rbtnodechain_t wchain;

	/*
	 * Caller must be holding the tree lock and MUST NOT be holding
	 * any node locks.
	 */

	/*
	 * Examine each ancestor level.  If the level's wild bit
	 * is set, then construct the corresponding wildcard name and
	 * search for it.  If the wildcard node exists, and is active in
	 * this version, we're done.  If not, then we next check to see
	 * if the ancestor is active in this version.  If so, then there
	 * can be no possible wildcard match and again we're done.  If not,
	 * continue the search.
	 */

	rbtdb = search->rbtdb;
	i = search->chain.level_matches;
	done = ISC_FALSE;
	node = *nodep;
	do {
		LOCK(&(rbtdb->node_locks[node->locknum].lock));

		/*
		 * First we try to figure out if this node is active in
		 * the search's version.  We do this now, even though we
		 * may not need the information, because it simplifies the
		 * locking and code flow.
		 */
		for (header = node->data;
		     header != NULL;
		     header = header->next) {
			if (header->serial <= search->serial &&
			    !IGNORE(header) && EXISTS(header))
				break;
		}
		if (header != NULL)
			active = ISC_TRUE;
		else
			active = ISC_FALSE;

		if (node->wild)
			wild = ISC_TRUE;
		else
			wild = ISC_FALSE;

		UNLOCK(&(rbtdb->node_locks[node->locknum].lock));

		if (wild) {
			/*
			 * Construct the wildcard name for this level.
			 */
			dns_name_init(&name, NULL);
			dns_rbt_namefromnode(node, &name);
			dns_fixedname_init(&fwname);
			wname = dns_fixedname_name(&fwname);
			result = dns_name_concatenate(dns_wildcardname, &name,
						      wname, NULL);
			j = i;
			while (result == ISC_R_SUCCESS && j != 0) {
				j--;
				level_node = search->chain.levels[j];
				dns_name_init(&name, NULL);
				dns_rbt_namefromnode(level_node, &name);
				result = dns_name_concatenate(wname,
							      &name,
							      wname,
							      NULL);
			}
			if (result != ISC_R_SUCCESS)
				break;

			wnode = NULL;
			dns_rbtnodechain_init(&wchain, NULL);
			result = dns_rbt_findnode(rbtdb->tree, wname,
						  NULL, &wnode, &wchain,
						  DNS_RBTFIND_EMPTYDATA,
						  NULL, NULL);
			if (result == ISC_R_SUCCESS) {
			    /*
			     * We have found the wildcard node.  If it
			     * is active in the search's version, we're
			     * done.
			     */
			    LOCK(&(rbtdb->node_locks[wnode->locknum].lock));
			    for (header = wnode->data;
				 header != NULL;
				 header = header->next) {
				    if (header->serial <= search->serial &&
					!IGNORE(header) && EXISTS(header))
					    break;
			    }
			    UNLOCK(&(rbtdb->node_locks[wnode->locknum].lock));
			    if (header != NULL ||
				activeempty(search, &wchain, wname)) {
				    if (activeemtpynode(search, qname, wname))
						return (ISC_R_NOTFOUND);
				    /*
				     * The wildcard node is active!
				     *
				     * Note: result is still ISC_R_SUCCESS
				     * so we don't have to set it.
				     */
				    *nodep = wnode;
				    break;
			    }
			} else if (result != ISC_R_NOTFOUND &&
				   result != DNS_R_PARTIALMATCH) {
				/*
				 * An error has occurred.  Bail out.
				 */
				break;
			}
		}

		if (active) {
			/*
			 * The level node is active.  Any wildcarding
			 * present at higher levels has no
			 * effect and we're done.
			 */
			result = ISC_R_NOTFOUND;
			break;
		}

		if (i > 0) {
			i--;
			node = search->chain.levels[i];
		} else
			done = ISC_TRUE;
	} while (!done);

	return (result);
}

static inline isc_result_t
find_closest_nsec(rbtdb_search_t *search, dns_dbnode_t **nodep,
		  dns_name_t *foundname, dns_rdataset_t *rdataset,
		  dns_rdataset_t *sigrdataset, isc_boolean_t need_sig)
{
	dns_rbtnode_t *node;
	rdatasetheader_t *header, *header_next, *found, *foundsig;
	isc_boolean_t empty_node;
	isc_result_t result;
	dns_fixedname_t fname, forigin;
	dns_name_t *name, *origin;

	do {
		node = NULL;
		dns_fixedname_init(&fname);
		name = dns_fixedname_name(&fname);
		dns_fixedname_init(&forigin);
		origin = dns_fixedname_name(&forigin);
		result = dns_rbtnodechain_current(&search->chain, name,
						  origin, &node);
		if (result != ISC_R_SUCCESS)
			return (result);
		LOCK(&(search->rbtdb->node_locks[node->locknum].lock));
		found = NULL;
		foundsig = NULL;
		empty_node = ISC_TRUE;
		for (header = node->data;
		     header != NULL;
		     header = header_next) {
			header_next = header->next;
			/*
			 * Look for an active, extant NSEC or RRSIG NSEC.
			 */
			do {
				if (header->serial <= search->serial &&
				    !IGNORE(header)) {
					/*
					 * Is this a "this rdataset doesn't
					 * exist" record?
					 */
					if (NONEXISTENT(header))
						header = NULL;
					break;
				} else
					header = header->down;
			} while (header != NULL);
			if (header != NULL) {
				/*
				 * We now know that there is at least one
				 * active rdataset at this node.
				 */
				empty_node = ISC_FALSE;
				if (header->type == dns_rdatatype_nsec) {
					found = header;
					if (foundsig != NULL)
						break;
				} else if (header->type ==
					   RBTDB_RDATATYPE_SIGNSEC) {
					foundsig = header;
					if (found != NULL)
						break;
				}
			}
		}
		if (!empty_node) {
			if (found != NULL &&
			    (foundsig != NULL || !need_sig))
			{
				/*
				 * We've found the right NSEC record.
				 *
				 * Note: for this to really be the right
				 * NSEC record, it's essential that the NSEC
				 * records of any nodes obscured by a zone
				 * cut have been removed; we assume this is
				 * the case.
				 */
				result = dns_name_concatenate(name, origin,
							      foundname, NULL);
				if (result == ISC_R_SUCCESS) {
					if (nodep != NULL) {
						new_reference(search->rbtdb,
							      node);
						*nodep = node;
					}
					bind_rdataset(search->rbtdb, node,
						      found, search->now,
						      rdataset);
					if (foundsig != NULL)
						bind_rdataset(search->rbtdb,
							      node,
							      foundsig,
							      search->now,
							      sigrdataset);
				}
			} else if (found == NULL && foundsig == NULL) {
				/*
				 * This node is active, but has no NSEC or
				 * RRSIG NSEC.  That means it's glue or
				 * other obscured zone data that isn't
				 * relevant for our search.  Treat the
				 * node as if it were empty and keep looking.
				 */
				empty_node = ISC_TRUE;
				result = dns_rbtnodechain_prev(&search->chain,
							       NULL, NULL);
			} else {
				/*
				 * We found an active node, but either the
				 * NSEC or the RRSIG NSEC is missing.  This
				 * shouldn't happen.
				 */
				result = DNS_R_BADDB;
			}
		} else {
			/*
			 * This node isn't active.  We've got to keep
			 * looking.
			 */
			result = dns_rbtnodechain_prev(&search->chain, NULL,
						       NULL);
		}
		UNLOCK(&(search->rbtdb->node_locks[node->locknum].lock));
	} while (empty_node && result == ISC_R_SUCCESS);

	/*
	 * If the result is ISC_R_NOMORE, then we got to the beginning of
	 * the database and didn't find a NSEC record.  This shouldn't
	 * happen.
	 */
	if (result == ISC_R_NOMORE)
		result = DNS_R_BADDB;

	return (result);
}

static isc_result_t
zone_find(dns_db_t *db, dns_name_t *name, dns_dbversion_t *version,
	  dns_rdatatype_t type, unsigned int options, isc_stdtime_t now,
	  dns_dbnode_t **nodep, dns_name_t *foundname,
	  dns_rdataset_t *rdataset, dns_rdataset_t *sigrdataset)
{
	dns_rbtnode_t *node = NULL;
	isc_result_t result;
	rbtdb_search_t search;
	isc_boolean_t cname_ok = ISC_TRUE;
	isc_boolean_t close_version = ISC_FALSE;
	isc_boolean_t maybe_zonecut = ISC_FALSE;
	isc_boolean_t at_zonecut = ISC_FALSE;
	isc_boolean_t wild;
	isc_boolean_t empty_node;
	isc_mutex_t *lock;
	rdatasetheader_t *header, *header_next, *found, *nsecheader;
	rdatasetheader_t *foundsig, *cnamesig, *nsecsig;
	rbtdb_rdatatype_t sigtype;
	isc_boolean_t active;
	dns_rbtnodechain_t chain;


	search.rbtdb = (dns_rbtdb_t *)db;

	REQUIRE(VALID_RBTDB(search.rbtdb));

	/*
	 * We don't care about 'now'.
	 */
	UNUSED(now);

	/*
	 * If the caller didn't supply a version, attach to the current
	 * version.
	 */
	if (version == NULL) {
		currentversion(db, &version);
		close_version = ISC_TRUE;
	}

	search.rbtversion = version;
	search.serial = search.rbtversion->serial;
	search.options = options;
	search.copy_name = ISC_FALSE;
	search.need_cleanup = ISC_FALSE;
	search.wild = ISC_FALSE;
	search.zonecut = NULL;
	dns_fixedname_init(&search.zonecut_name);
	dns_rbtnodechain_init(&search.chain, search.rbtdb->common.mctx);
	search.now = 0;

	/*
	 * 'wild' will be true iff. we've matched a wildcard.
	 */
	wild = ISC_FALSE;

	RWLOCK(&search.rbtdb->tree_lock, isc_rwlocktype_read);

	/*
	 * Search down from the root of the tree.  If, while going down, we
	 * encounter a callback node, zone_zonecut_callback() will search the
	 * rdatasets at the zone cut for active DNAME or NS rdatasets.
	 */
	result = dns_rbt_findnode(search.rbtdb->tree, name, foundname, &node,
				  &search.chain, DNS_RBTFIND_EMPTYDATA,
				  zone_zonecut_callback, &search);

	if (result == DNS_R_PARTIALMATCH) {
	partial_match:
		if (search.zonecut != NULL) {
		    result = setup_delegation(&search, nodep, foundname,
					      rdataset, sigrdataset);
		    goto tree_exit;
		}

		if (search.wild) {
			/*
			 * At least one of the levels in the search chain
			 * potentially has a wildcard.  For each such level,
			 * we must see if there's a matching wildcard active
			 * in the current version.
			 */
			result = find_wildcard(&search, &node, name);
			if (result == ISC_R_SUCCESS) {
				result = dns_name_copy(name, foundname, NULL);
				if (result != ISC_R_SUCCESS)
					goto tree_exit;
				wild = ISC_TRUE;
				goto found;
			}
			else if (result != ISC_R_NOTFOUND)
				goto tree_exit;
		}

		chain = search.chain;
		active = activeempty(&search, &chain, name);

		/*
		 * If we're here, then the name does not exist, is not
		 * beneath a zonecut, and there's no matching wildcard.
		 */
		if (search.rbtdb->secure ||
		    (search.options & DNS_DBFIND_FORCENSEC) != 0)
		{
			result = find_closest_nsec(&search, nodep, foundname,
						  rdataset, sigrdataset,
						  search.rbtdb->secure);
			if (result == ISC_R_SUCCESS)
				result = active ? DNS_R_EMPTYNAME :
						  DNS_R_NXDOMAIN;
		} else
			result = active ? DNS_R_EMPTYNAME : DNS_R_NXDOMAIN;
		goto tree_exit;
	} else if (result != ISC_R_SUCCESS)
		goto tree_exit;

 found:
	/*
	 * We have found a node whose name is the desired name, or we
	 * have matched a wildcard.
	 */

	if (search.zonecut != NULL) {
		/*
		 * If we're beneath a zone cut, we don't want to look for
		 * CNAMEs because they're not legitimate zone glue.
		 */
		cname_ok = ISC_FALSE;
	} else {
		/*
		 * The node may be a zone cut itself.  If it might be one,
		 * make sure we check for it later.
		 */
		if (node->find_callback &&
		    (node != search.rbtdb->origin_node ||
		     IS_STUB(search.rbtdb)) &&
		    !dns_rdatatype_atparent(type))
			maybe_zonecut = ISC_TRUE;
	}

	/*
	 * Certain DNSSEC types are not subject to CNAME matching
	 * (RFC 2535, section 2.3.5).
	 *
	 * We don't check for RRSIG, because we don't store RRSIG records
	 * directly.
	 */
	if (type == dns_rdatatype_dnskey || type == dns_rdatatype_nsec)
		cname_ok = ISC_FALSE;

	/*
	 * We now go looking for rdata...
	 */

	LOCK(&(search.rbtdb->node_locks[node->locknum].lock));

	found = NULL;
	foundsig = NULL;
	sigtype = RBTDB_RDATATYPE_VALUE(dns_rdatatype_rrsig, type);
	nsecheader = NULL;
	nsecsig = NULL;
	cnamesig = NULL;
	empty_node = ISC_TRUE;
	for (header = node->data; header != NULL; header = header_next) {
		header_next = header->next;
		/*
		 * Look for an active, extant rdataset.
		 */
		do {
			if (header->serial <= search.serial &&
			    !IGNORE(header)) {
				/*
				 * Is this a "this rdataset doesn't
				 * exist" record?
				 */
				if (NONEXISTENT(header))
					header = NULL;
				break;
			} else
				header = header->down;
		} while (header != NULL);
		if (header != NULL) {
			/*
			 * We now know that there is at least one active
			 * rdataset at this node.
			 */
			empty_node = ISC_FALSE;

			/*
			 * Do special zone cut handling, if requested.
			 */
			if (maybe_zonecut &&
			    header->type == dns_rdatatype_ns) {
				/*
				 * We increment the reference count on node to
				 * ensure that search->zonecut_rdataset will
				 * still be valid later.
				 */
				new_reference(search.rbtdb, node);
				search.zonecut = node;
				search.zonecut_rdataset = header;
				search.zonecut_sigrdataset = NULL;
				search.need_cleanup = ISC_TRUE;
				maybe_zonecut = ISC_FALSE;
				at_zonecut = ISC_TRUE;
				if ((search.options & DNS_DBFIND_GLUEOK) == 0
				    && type != dns_rdatatype_nsec
				    && type != dns_rdatatype_dnskey) {
					/*
					 * Glue is not OK, but any answer we
					 * could return would be glue.  Return
					 * the delegation.
					 */
					found = NULL;
					break;
				}
				if (found != NULL && foundsig != NULL)
					break;
			}

			/*
			 * If we found a type we were looking for,
			 * remember it.
			 */
			if (header->type == type ||
			    type == dns_rdatatype_any ||
			    (header->type == dns_rdatatype_cname &&
			     cname_ok)) {
				/*
				 * We've found the answer!
				 */
				found = header;
				if (header->type == dns_rdatatype_cname &&
				    cname_ok) {
					/*
					 * We may be finding a CNAME instead
					 * of the desired type.
					 *
					 * If we've already got the CNAME RRSIG,
					 * use it, otherwise change sigtype
					 * so that we find it.
					 */
					if (cnamesig != NULL)
						foundsig = cnamesig;
					else
						sigtype =
						    RBTDB_RDATATYPE_SIGCNAME;
				}
				/*
				 * If we've got all we need, end the search.
				 */
				if (!maybe_zonecut && foundsig != NULL)
					break;
			} else if (header->type == sigtype) {
				/*
				 * We've found the RRSIG rdataset for our
				 * target type.  Remember it.
				 */
				foundsig = header;
				/*
				 * If we've got all we need, end the search.
				 */
				if (!maybe_zonecut && found != NULL)
					break;
			} else if (header->type == dns_rdatatype_nsec) {
				/*
				 * Remember a NSEC rdataset even if we're
				 * not specifically looking for it, because
				 * we might need it later.
				 */
				nsecheader = header;
			} else if (header->type == RBTDB_RDATATYPE_SIGNSEC) {
				/*
				 * If we need the NSEC rdataset, we'll also
				 * need its signature.
				 */
				nsecsig = header;
			} else if (cname_ok &&
				   header->type == RBTDB_RDATATYPE_SIGCNAME) {
				/*
				 * If we get a CNAME match, we'll also need
				 * its signature.
				 */
				cnamesig = header;
			}
		}
	}

	if (empty_node) {
		/*
		 * We have an exact match for the name, but there are no
		 * active rdatasets in the desired version.  That means that
		 * this node doesn't exist in the desired version, and that
		 * we really have a partial match.
		 */
		if (!wild) {
			UNLOCK(&(search.rbtdb->node_locks[node->locknum].lock));
			goto partial_match;
		}
	}

	/*
	 * If we didn't find what we were looking for...
	 */
	if (found == NULL) {
		if (search.zonecut != NULL) {
		    /*
		     * We were trying to find glue at a node beneath a
		     * zone cut, but didn't.
		     *
		     * Return the delegation.
		     */
		    UNLOCK(&(search.rbtdb->node_locks[node->locknum].lock));
		    result = setup_delegation(&search, nodep, foundname,
					      rdataset, sigrdataset);
		    goto tree_exit;
		}
		/*
		 * The desired type doesn't exist.
		 */
		result = DNS_R_NXRRSET;
		if (search.rbtdb->secure &&
		    (nsecheader == NULL || nsecsig == NULL)) {
			/*
			 * The zone is secure but there's no NSEC,
			 * or the NSEC has no signature!
			 */
			if (!wild) {
				result = DNS_R_BADDB;
				goto node_exit;
			}
			
			UNLOCK(&(search.rbtdb->node_locks[node->locknum].lock));
			result = find_closest_nsec(&search, nodep, foundname,
						  rdataset, sigrdataset,
						  search.rbtdb->secure);
			if (result == ISC_R_SUCCESS)
				result = DNS_R_EMPTYWILD;
			goto tree_exit;
		}
		if ((search.options & DNS_DBFIND_FORCENSEC) != 0 &&
		    nsecheader == NULL)
		{
			/*
			 * There's no NSEC record, and we were told
			 * to find one.
			 */
			result = DNS_R_BADDB;
			goto node_exit;
		}
		if (nodep != NULL) {
			new_reference(search.rbtdb, node);
			*nodep = node;
		}
		if (search.rbtdb->secure ||
		    (search.options & DNS_DBFIND_FORCENSEC) != 0)
		{
			bind_rdataset(search.rbtdb, node, nsecheader,
				      0, rdataset);
			if (nsecsig != NULL)
				bind_rdataset(search.rbtdb, node,
					      nsecsig, 0, sigrdataset);
		}
		if (wild)
			foundname->attributes |= DNS_NAMEATTR_WILDCARD;
		goto node_exit;
	}

	/*
	 * We found what we were looking for, or we found a CNAME.
	 */

	if (type != found->type &&
	    type != dns_rdatatype_any &&
	    found->type == dns_rdatatype_cname) {
		/*
		 * We weren't doing an ANY query and we found a CNAME instead
		 * of the type we were looking for, so we need to indicate
		 * that result to the caller.
		 */
		result = DNS_R_CNAME;
	} else if (search.zonecut != NULL) {
		/*
		 * If we're beneath a zone cut, we must indicate that the
		 * result is glue, unless we're actually at the zone cut
		 * and the type is NSEC or KEY.
		 */
		if (search.zonecut == node) {
			if (type == dns_rdatatype_nsec ||
			    type == dns_rdatatype_dnskey)
				result = ISC_R_SUCCESS;
			else if (type == dns_rdatatype_any)
				result = DNS_R_ZONECUT;
			else
				result = DNS_R_GLUE;
		} else
			result = DNS_R_GLUE;
		/*
		 * We might have found data that isn't glue, but was occluded
		 * by a dynamic update.  If the caller cares about this, they
		 * will have told us to validate glue.
		 *
		 * XXX We should cache the glue validity state!
		 */
		if (result == DNS_R_GLUE &&
		    (search.options & DNS_DBFIND_VALIDATEGLUE) != 0 &&
		    !valid_glue(&search, foundname, type, node)) {
		    UNLOCK(&(search.rbtdb->node_locks[node->locknum].lock));
		    result = setup_delegation(&search, nodep, foundname,
					      rdataset, sigrdataset);
		    goto tree_exit;
		}
	} else {
		/*
		 * An ordinary successful query!
		 */
		result = ISC_R_SUCCESS;
	}

	if (nodep != NULL) {
		if (!at_zonecut)
			new_reference(search.rbtdb, node);
		else
			search.need_cleanup = ISC_FALSE;
		*nodep = node;
	}

	if (type != dns_rdatatype_any) {
		bind_rdataset(search.rbtdb, node, found, 0, rdataset);
		if (foundsig != NULL)
			bind_rdataset(search.rbtdb, node, foundsig, 0,
				      sigrdataset);
	}

	if (wild)
		foundname->attributes |= DNS_NAMEATTR_WILDCARD;

 node_exit:
	UNLOCK(&(search.rbtdb->node_locks[node->locknum].lock));

 tree_exit:
	RWUNLOCK(&search.rbtdb->tree_lock, isc_rwlocktype_read);

	/*
	 * If we found a zonecut but aren't going to use it, we have to
	 * let go of it.
	 */
	if (search.need_cleanup) {
		node = search.zonecut;
		lock = &(search.rbtdb->node_locks[node->locknum].lock);

		LOCK(lock);
		INSIST(node->references > 0);
		node->references--;
		if (node->references == 0)
			no_references(search.rbtdb, node, 0,
				      isc_rwlocktype_none);

		UNLOCK(lock);
	}

	if (close_version)
		closeversion(db, &version, ISC_FALSE);

	dns_rbtnodechain_reset(&search.chain);

	return (result);
}

static isc_result_t
zone_findzonecut(dns_db_t *db, dns_name_t *name, unsigned int options,
		 isc_stdtime_t now, dns_dbnode_t **nodep,
		 dns_name_t *foundname,
		 dns_rdataset_t *rdataset, dns_rdataset_t *sigrdataset)
{
	UNUSED(db);
	UNUSED(name);
	UNUSED(options);
	UNUSED(now);
	UNUSED(nodep);
	UNUSED(foundname);
	UNUSED(rdataset);
	UNUSED(sigrdataset);

	FATAL_ERROR(__FILE__, __LINE__, "zone_findzonecut() called!");

	return (ISC_R_NOTIMPLEMENTED);
}

static isc_result_t
cache_zonecut_callback(dns_rbtnode_t *node, dns_name_t *name, void *arg) {
	rbtdb_search_t *search = arg;
	rdatasetheader_t *header, *header_prev, *header_next;
	rdatasetheader_t *dname_header, *sigdname_header;
	isc_result_t result;

	/* XXX comment */

	REQUIRE(search->zonecut == NULL);

	/*
	 * Keep compiler silent.
	 */
	UNUSED(name);

	LOCK(&(search->rbtdb->node_locks[node->locknum].lock));

	/*
	 * Look for a DNAME or RRSIG DNAME rdataset.
	 */
	dname_header = NULL;
	sigdname_header = NULL;
	header_prev = NULL;
	for (header = node->data; header != NULL; header = header_next) {
		header_next = header->next;
		if (header->ttl <= search->now) {
			/*
			 * This rdataset is stale.  If no one else is
			 * using the node, we can clean it up right
			 * now, otherwise we mark it as stale, and
			 * the node as dirty, so it will get cleaned
			 * up later.
			 */
			if (node->references == 0) {
				INSIST(header->down == NULL);
				if (header_prev != NULL)
					header_prev->next =
						header->next;
				else
					node->data = header->next;
				free_rdataset(search->rbtdb->common.mctx,
					      header);
			} else {
				header->attributes |=
					RDATASET_ATTR_STALE;
				node->dirty = 1;
				header_prev = header;
			}
		} else if (header->type == dns_rdatatype_dname &&
			   EXISTS(header)) {
			dname_header = header;
			header_prev = header;
		} else if (header->type == RBTDB_RDATATYPE_SIGDNAME &&
			 EXISTS(header)) {
			sigdname_header = header;
			header_prev = header;
		} else
			header_prev = header;
	}

	if (dname_header != NULL &&
	    (dname_header->trust != dns_trust_pending ||
	     (search->options & DNS_DBFIND_PENDINGOK) != 0)) {
		/*
		 * We increment the reference count on node to ensure that
		 * search->zonecut_rdataset will still be valid later.
		 */
		new_reference(search->rbtdb, node);
		search->zonecut = node;
		search->zonecut_rdataset = dname_header;
		search->zonecut_sigrdataset = sigdname_header;
		search->need_cleanup = ISC_TRUE;
		result = DNS_R_PARTIALMATCH;
	} else
		result = DNS_R_CONTINUE;

	UNLOCK(&(search->rbtdb->node_locks[node->locknum].lock));

	return (result);
}

static inline isc_result_t
find_deepest_zonecut(rbtdb_search_t *search, dns_rbtnode_t *node,
		     dns_dbnode_t **nodep, dns_name_t *foundname,
		     dns_rdataset_t *rdataset, dns_rdataset_t *sigrdataset)
{
	unsigned int i;
	dns_rbtnode_t *level_node;
	rdatasetheader_t *header, *header_prev, *header_next;
	rdatasetheader_t *found, *foundsig;
	isc_result_t result = ISC_R_NOTFOUND;
	dns_name_t name;
	dns_rbtdb_t *rbtdb;
	isc_boolean_t done;

	/*
	 * Caller must be holding the tree lock.
	 */

	rbtdb = search->rbtdb;
	i = search->chain.level_matches;
	done = ISC_FALSE;
	do {
		LOCK(&(rbtdb->node_locks[node->locknum].lock));

		/*
		 * Look for NS and RRSIG NS rdatasets.
		 */
		found = NULL;
		foundsig = NULL;
		header_prev = NULL;
		for (header = node->data;
		     header != NULL;
		     header = header_next) {
			header_next = header->next;
			if (header->ttl <= search->now) {
				/*
				 * This rdataset is stale.  If no one else is
				 * using the node, we can clean it up right
				 * now, otherwise we mark it as stale, and
				 * the node as dirty, so it will get cleaned
				 * up later.
				 */
				if (node->references == 0) {
					INSIST(header->down == NULL);
					if (header_prev != NULL)
						header_prev->next =
							header->next;
					else
						node->data = header->next;
					free_rdataset(rbtdb->common.mctx,
						      header);
				} else {
					header->attributes |=
						RDATASET_ATTR_STALE;
					node->dirty = 1;
					header_prev = header;
				}
			} else if (EXISTS(header)) {
				/*
				 * We've found an extant rdataset.  See if
				 * we're interested in it.
				 */
				if (header->type == dns_rdatatype_ns) {
					found = header;
					if (foundsig != NULL)
						break;
				} else if (header->type ==
					   RBTDB_RDATATYPE_SIGNS) {
					foundsig = header;
					if (found != NULL)
						break;
				}
				header_prev = header;
			} else
				header_prev = header;
		}

		if (found != NULL) {
			/*
			 * If we have to set foundname, we do it before
			 * anything else.  If we were to set foundname after
			 * we had set nodep or bound the rdataset, then we'd
			 * have to undo that work if dns_name_concatenate()
			 * failed.  By setting foundname first, there's
			 * nothing to undo if we have trouble.
			 */
			if (foundname != NULL) {
				dns_name_init(&name, NULL);
				dns_rbt_namefromnode(node, &name);
				result = dns_name_copy(&name, foundname, NULL);
				while (result == ISC_R_SUCCESS && i > 0) {
					i--;
					level_node = search->chain.levels[i];
					dns_name_init(&name, NULL);
					dns_rbt_namefromnode(level_node,
							     &name);
					result =
						dns_name_concatenate(foundname,
								     &name,
								     foundname,
								     NULL);
				}
				if (result != ISC_R_SUCCESS) {
					*nodep = NULL;
					goto node_exit;
				}
			}
			result = DNS_R_DELEGATION;
			if (nodep != NULL) {
				new_reference(search->rbtdb, node);
				*nodep = node;
			}
			bind_rdataset(search->rbtdb, node, found, search->now,
				      rdataset);
			if (foundsig != NULL)
				bind_rdataset(search->rbtdb, node, foundsig,
					      search->now, sigrdataset);
		}

	node_exit:
		UNLOCK(&(search->rbtdb->node_locks[node->locknum].lock));

		if (found == NULL && i > 0) {
			i--;
			node = search->chain.levels[i];
		} else
			done = ISC_TRUE;

	} while (!done);

	return (result);
}

static isc_result_t
find_coveringnsec(rbtdb_search_t *search, dns_dbnode_t **nodep,
		  isc_stdtime_t now, dns_name_t *foundname,
		  dns_rdataset_t *rdataset, dns_rdataset_t *sigrdataset)
{
	dns_rbtnode_t *node;
	rdatasetheader_t *header, *header_next, *header_prev;
	rdatasetheader_t *found, *foundsig;
	isc_boolean_t empty_node;
	isc_result_t result;
	dns_fixedname_t fname, forigin;
	dns_name_t *name, *origin;
	rbtdb_rdatatype_t matchtype, sigmatchtype;

	matchtype = RBTDB_RDATATYPE_VALUE(dns_rdatatype_nsec, 0);
	sigmatchtype = RBTDB_RDATATYPE_VALUE(dns_rdatatype_rrsig,
					     dns_rdatatype_nsec);
	
	do {
		node = NULL;
		dns_fixedname_init(&fname);
		name = dns_fixedname_name(&fname);
		dns_fixedname_init(&forigin);
		origin = dns_fixedname_name(&forigin);
		result = dns_rbtnodechain_current(&search->chain, name,
						  origin, &node);
		if (result != ISC_R_SUCCESS)
			return (result);
		LOCK(&(search->rbtdb->node_locks[node->locknum].lock));
		found = NULL;
		foundsig = NULL;
		empty_node = ISC_TRUE;
		header_prev = NULL;
		for (header = node->data;
		     header != NULL;
		     header = header_next) {
			header_next = header->next;
			if (header->ttl <= now) {
				/*
				 * This rdataset is stale.  If no one else is
				 * using the node, we can clean it up right
				 * now, otherwise we mark it as stale, and the
				 * node as dirty, so it will get cleaned up 
				 * later.
				 */
				if (header->ttl > search->now - RBTDB_VIRTUAL)
					header_prev = header;
				else if (node->references == 0) {
					INSIST(header->down == NULL);
					if (header_prev != NULL)
						header_prev->next =
							header->next;
					else
						node->data = header->next;
					free_rdataset(search->rbtdb->common.mctx,
						      header);
				} else {
					header->attributes |=
						 RDATASET_ATTR_STALE;
					node->dirty = 1;
					header_prev = header;
				}
				continue;
			}
			if (NONEXISTENT(header) || NXDOMAIN(header)) {
				header_prev = header;
				continue;
			}
			empty_node = ISC_FALSE;
			if (header->type == matchtype)
				found = header;
			else if (header->type == sigmatchtype)
				foundsig = header;
			header_prev = header;
		}
		if (found != NULL) {
			result = dns_name_concatenate(name, origin,
						      foundname, NULL);
			if (result != ISC_R_SUCCESS)
				goto unlock_node;
			bind_rdataset(search->rbtdb, node, found,
				      now, rdataset);
			if (foundsig != NULL)
				bind_rdataset(search->rbtdb, node, foundsig,
					      now, sigrdataset);
			new_reference(search->rbtdb, node);
			*nodep = node;
			result = DNS_R_COVERINGNSEC;
		} else if (!empty_node) {
			result = ISC_R_NOTFOUND;
		}else
			result = dns_rbtnodechain_prev(&search->chain, NULL,
						       NULL);
 unlock_node:
		UNLOCK(&(search->rbtdb->node_locks[node->locknum].lock));
	} while (empty_node && result == ISC_R_SUCCESS);
	return (result);
}

static isc_result_t
cache_find(dns_db_t *db, dns_name_t *name, dns_dbversion_t *version,
	   dns_rdatatype_t type, unsigned int options, isc_stdtime_t now,
	   dns_dbnode_t **nodep, dns_name_t *foundname,
	   dns_rdataset_t *rdataset, dns_rdataset_t *sigrdataset)
{
	dns_rbtnode_t *node = NULL;
	isc_result_t result;
	rbtdb_search_t search;
	isc_boolean_t cname_ok = ISC_TRUE;
	isc_boolean_t empty_node;
	isc_mutex_t *lock;
	rdatasetheader_t *header, *header_prev, *header_next;
	rdatasetheader_t *found, *nsheader;
	rdatasetheader_t *foundsig, *nssig, *cnamesig;
	rbtdb_rdatatype_t sigtype, nsectype;

	UNUSED(version);

	search.rbtdb = (dns_rbtdb_t *)db;

	REQUIRE(VALID_RBTDB(search.rbtdb));
	REQUIRE(version == NULL);

	if (now == 0)
		isc_stdtime_get(&now);

	search.rbtversion = NULL;
	search.serial = 1;
	search.options = options;
	search.copy_name = ISC_FALSE;
	search.need_cleanup = ISC_FALSE;
	search.wild = ISC_FALSE;
	search.zonecut = NULL;
	dns_fixedname_init(&search.zonecut_name);
	dns_rbtnodechain_init(&search.chain, search.rbtdb->common.mctx);
	search.now = now;

	RWLOCK(&search.rbtdb->tree_lock, isc_rwlocktype_read);

	/*
	 * Search down from the root of the tree.  If, while going down, we
	 * encounter a callback node, cache_zonecut_callback() will search the
	 * rdatasets at the zone cut for a DNAME rdataset.
	 */
	result = dns_rbt_findnode(search.rbtdb->tree, name, foundname, &node,
				  &search.chain, DNS_RBTFIND_EMPTYDATA,
				  cache_zonecut_callback, &search);

	if (result == DNS_R_PARTIALMATCH) {
		if ((search.options & DNS_DBFIND_COVERINGNSEC) != 0) {
			result = find_coveringnsec(&search, nodep, now,
						   foundname, rdataset,
						   sigrdataset);
			if (result == DNS_R_COVERINGNSEC)
				goto tree_exit;
		}
		if (search.zonecut != NULL) {
		    result = setup_delegation(&search, nodep, foundname,
					      rdataset, sigrdataset);
		    goto tree_exit;
		} else {
		find_ns:
			result = find_deepest_zonecut(&search, node, nodep,
						      foundname, rdataset,
						      sigrdataset);
			goto tree_exit;
		}
	} else if (result != ISC_R_SUCCESS)
		goto tree_exit;

	/*
	 * Certain DNSSEC types are not subject to CNAME matching
	 * (RFC 2535, section 2.3.5).
	 *
	 * We don't check for RRSIG, because we don't store RRSIG records
	 * directly.
	 */
	if (type == dns_rdatatype_dnskey || type == dns_rdatatype_nsec)
		cname_ok = ISC_FALSE;

	/*
	 * We now go looking for rdata...
	 */

	LOCK(&(search.rbtdb->node_locks[node->locknum].lock));

	found = NULL;
	foundsig = NULL;
	sigtype = RBTDB_RDATATYPE_VALUE(dns_rdatatype_rrsig, type);
	nsectype = RBTDB_RDATATYPE_VALUE(0, type);
	nsheader = NULL;
	nssig = NULL;
	cnamesig = NULL;
	empty_node = ISC_TRUE;
	header_prev = NULL;
	for (header = node->data; header != NULL; header = header_next) {
		header_next = header->next;
		if (header->ttl <= now) {
			/*
			 * This rdataset is stale.  If no one else is using the
			 * node, we can clean it up right now, otherwise we
			 * mark it as stale, and the node as dirty, so it will
			 * get cleaned up later.
			 */
			if (header->ttl > now - RBTDB_VIRTUAL)
				header_prev = header;
			else if (node->references == 0) {
				INSIST(header->down == NULL);
				if (header_prev != NULL)
					header_prev->next = header->next;
				else
					node->data = header->next;
				free_rdataset(search.rbtdb->common.mctx,
					      header);
			} else {
				header->attributes |= RDATASET_ATTR_STALE;
				node->dirty = 1;
				header_prev = header;
			}
		} else if (EXISTS(header)) {
			/*
			 * We now know that there is at least one active
			 * non-stale rdataset at this node.
			 */
			empty_node = ISC_FALSE;

			/*
			 * If we found a type we were looking for, remember
			 * it.
			 */
			if (header->type == type ||
			    (type == dns_rdatatype_any &&
			     RBTDB_RDATATYPE_BASE(header->type) != 0) ||
			    (cname_ok && header->type ==
			     dns_rdatatype_cname)) {
				/*
				 * We've found the answer.
				 */
				found = header;
				if (header->type == dns_rdatatype_cname &&
				    cname_ok &&
				    cnamesig != NULL) {
					/*
					 * If we've already got the CNAME RRSIG,
					 * use it, otherwise change sigtype
					 * so that we find it.
					 */
					if (cnamesig != NULL)
						foundsig = cnamesig;
					else
						sigtype =
						    RBTDB_RDATATYPE_SIGCNAME;
					foundsig = cnamesig;
				}
			} else if (header->type == sigtype) {
				/*
				 * We've found the RRSIG rdataset for our
				 * target type.  Remember it.
				 */
				foundsig = header;
			} else if (header->type == RBTDB_RDATATYPE_NCACHEANY ||
				   header->type == nsectype) {
				/*
				 * We've found a negative cache entry.
				 */
				found = header;
			} else if (header->type == dns_rdatatype_ns) {
				/*
				 * Remember a NS rdataset even if we're
				 * not specifically looking for it, because
				 * we might need it later.
				 */
				nsheader = header;
			} else if (header->type == RBTDB_RDATATYPE_SIGNS) {
				/*
				 * If we need the NS rdataset, we'll also
				 * need its signature.
				 */
				nssig = header;
			} else if (cname_ok &&
				   header->type == RBTDB_RDATATYPE_SIGCNAME) {
				/*
				 * If we get a CNAME match, we'll also need
				 * its signature.
				 */
				cnamesig = header;
			}
			header_prev = header;
		} else
			header_prev = header;
	}

	if (empty_node) {
		/*
		 * We have an exact match for the name, but there are no
		 * extant rdatasets.  That means that this node doesn't
		 * meaningfully exist, and that we really have a partial match.
		 */
		UNLOCK(&(search.rbtdb->node_locks[node->locknum].lock));
		goto find_ns;
	}

	/*
	 * If we didn't find what we were looking for...
	 */
	if (found == NULL ||
	    (found->trust == dns_trust_glue &&
	     ((options & DNS_DBFIND_GLUEOK) == 0)) ||
	    (found->trust == dns_trust_pending &&
	     ((options & DNS_DBFIND_PENDINGOK) == 0))) {
		/*
		 * If there is an NS rdataset at this node, then this is the
		 * deepest zone cut.
		 */
		if (nsheader != NULL) {
			if (nodep != NULL) {
				new_reference(search.rbtdb, node);
				*nodep = node;
			}
			bind_rdataset(search.rbtdb, node, nsheader, search.now,
				      rdataset);
			if (nssig != NULL)
				bind_rdataset(search.rbtdb, node, nssig,
					      search.now, sigrdataset);
			result = DNS_R_DELEGATION;
			goto node_exit;
		}

		/*
		 * Go find the deepest zone cut.
		 */
		UNLOCK(&(search.rbtdb->node_locks[node->locknum].lock));
		goto find_ns;
	}

	/*
	 * We found what we were looking for, or we found a CNAME.
	 */

	if (nodep != NULL) {
		new_reference(search.rbtdb, node);
		*nodep = node;
	}

	if (RBTDB_RDATATYPE_BASE(found->type) == 0) {
		/*
		 * We found a negative cache entry.
		 */
		if (NXDOMAIN(found))
			result = DNS_R_NCACHENXDOMAIN;
		else
			result = DNS_R_NCACHENXRRSET;
	} else if (type != found->type &&
		   type != dns_rdatatype_any &&
		   found->type == dns_rdatatype_cname) {
		/*
		 * We weren't doing an ANY query and we found a CNAME instead
		 * of the type we were looking for, so we need to indicate
		 * that result to the caller.
		 */
		result = DNS_R_CNAME;
	} else {
		/*
		 * An ordinary successful query!
		 */
		result = ISC_R_SUCCESS;
	}

	if (type != dns_rdatatype_any || result == DNS_R_NCACHENXDOMAIN ||
	    result == DNS_R_NCACHENXRRSET) {
		bind_rdataset(search.rbtdb, node, found, search.now,
			      rdataset);
		if (foundsig != NULL)
			bind_rdataset(search.rbtdb, node, foundsig, search.now,
				      sigrdataset);
	}

 node_exit:
	UNLOCK(&(search.rbtdb->node_locks[node->locknum].lock));

 tree_exit:
	RWUNLOCK(&search.rbtdb->tree_lock, isc_rwlocktype_read);

	/*
	 * If we found a zonecut but aren't going to use it, we have to
	 * let go of it.
	 */
	if (search.need_cleanup) {
		node = search.zonecut;
		lock = &(search.rbtdb->node_locks[node->locknum].lock);

		LOCK(lock);
		INSIST(node->references > 0);
		node->references--;
		if (node->references == 0)
			no_references(search.rbtdb, node, 0,
				      isc_rwlocktype_none);
		UNLOCK(lock);
	}

	dns_rbtnodechain_reset(&search.chain);

	return (result);
}

static isc_result_t
cache_findzonecut(dns_db_t *db, dns_name_t *name, unsigned int options,
		  isc_stdtime_t now, dns_dbnode_t **nodep,
		  dns_name_t *foundname,
		  dns_rdataset_t *rdataset, dns_rdataset_t *sigrdataset)
{
	dns_rbtnode_t *node = NULL;
	isc_result_t result;
	rbtdb_search_t search;
	rdatasetheader_t *header, *header_prev, *header_next;
	rdatasetheader_t *found, *foundsig;
	unsigned int rbtoptions = DNS_RBTFIND_EMPTYDATA;

	search.rbtdb = (dns_rbtdb_t *)db;

	REQUIRE(VALID_RBTDB(search.rbtdb));

	if (now == 0)
		isc_stdtime_get(&now);

	search.rbtversion = NULL;
	search.serial = 1;
	search.options = options;
	search.copy_name = ISC_FALSE;
	search.need_cleanup = ISC_FALSE;
	search.wild = ISC_FALSE;
	search.zonecut = NULL;
	dns_fixedname_init(&search.zonecut_name);
	dns_rbtnodechain_init(&search.chain, search.rbtdb->common.mctx);
	search.now = now;

	if ((options & DNS_DBFIND_NOEXACT) != 0)
		rbtoptions |= DNS_RBTFIND_NOEXACT;

	RWLOCK(&search.rbtdb->tree_lock, isc_rwlocktype_read);

	/*
	 * Search down from the root of the tree.
	 */
	result = dns_rbt_findnode(search.rbtdb->tree, name, foundname, &node,
				  &search.chain, rbtoptions, NULL, &search);

	if (result == DNS_R_PARTIALMATCH) {
	find_ns:
		result = find_deepest_zonecut(&search, node, nodep, foundname,
					      rdataset, sigrdataset);
		goto tree_exit;
	} else if (result != ISC_R_SUCCESS)
		goto tree_exit;

	/*
	 * We now go looking for an NS rdataset at the node.
	 */

	LOCK(&(search.rbtdb->node_locks[node->locknum].lock));

	found = NULL;
	foundsig = NULL;
	header_prev = NULL;
	for (header = node->data; header != NULL; header = header_next) {
		header_next = header->next;
		if (header->ttl <= now) {
			/*
			 * This rdataset is stale.  If no one else is using the
			 * node, we can clean it up right now, otherwise we
			 * mark it as stale, and the node as dirty, so it will
			 * get cleaned up later.
			 */
			if (header->ttl > now - RBTDB_VIRTUAL)
				header_prev = header;
			else if (node->references == 0) {
				INSIST(header->down == NULL);
				if (header_prev != NULL)
					header_prev->next = header->next;
				else
					node->data = header->next;
				free_rdataset(search.rbtdb->common.mctx,
					      header);
			} else {
				header->attributes |= RDATASET_ATTR_STALE;
				node->dirty = 1;
				header_prev = header;
			}
		} else if (EXISTS(header)) {
			/*
			 * If we found a type we were looking for, remember
			 * it.
			 */
			if (header->type == dns_rdatatype_ns) {
				/*
				 * Remember a NS rdataset even if we're
				 * not specifically looking for it, because
				 * we might need it later.
				 */
				found = header;
			} else if (header->type == RBTDB_RDATATYPE_SIGNS) {
				/*
				 * If we need the NS rdataset, we'll also
				 * need its signature.
				 */
				foundsig = header;
			}
			header_prev = header;
		} else
			header_prev = header;
	}

	if (found == NULL) {
		/*
		 * No NS records here.
		 */
		UNLOCK(&(search.rbtdb->node_locks[node->locknum].lock));
		goto find_ns;
	}

	if (nodep != NULL) {
		new_reference(search.rbtdb, node);
		*nodep = node;
	}

	bind_rdataset(search.rbtdb, node, found, search.now, rdataset);
	if (foundsig != NULL)
		bind_rdataset(search.rbtdb, node, foundsig, search.now,
			      sigrdataset);

	UNLOCK(&(search.rbtdb->node_locks[node->locknum].lock));

 tree_exit:
	RWUNLOCK(&search.rbtdb->tree_lock, isc_rwlocktype_read);

	INSIST(!search.need_cleanup);

	dns_rbtnodechain_reset(&search.chain);

	if (result == DNS_R_DELEGATION)
		result = ISC_R_SUCCESS;

	return (result);
}

static void
attachnode(dns_db_t *db, dns_dbnode_t *source, dns_dbnode_t **targetp) {
	dns_rbtdb_t *rbtdb = (dns_rbtdb_t *)db;
	dns_rbtnode_t *node = (dns_rbtnode_t *)source;

	REQUIRE(VALID_RBTDB(rbtdb));
	REQUIRE(targetp != NULL && *targetp == NULL);

	LOCK(&rbtdb->node_locks[node->locknum].lock);
	INSIST(node->references > 0);
	node->references++;
	INSIST(node->references != 0);			/* Catch overflow. */
	UNLOCK(&rbtdb->node_locks[node->locknum].lock);

	*targetp = source;
}

static void
detachnode(dns_db_t *db, dns_dbnode_t **targetp) {
	dns_rbtdb_t *rbtdb = (dns_rbtdb_t *)db;
	dns_rbtnode_t *node;
	isc_boolean_t want_free = ISC_FALSE;
	isc_boolean_t inactive = ISC_FALSE;
	unsigned int locknum;

	REQUIRE(VALID_RBTDB(rbtdb));
	REQUIRE(targetp != NULL && *targetp != NULL);

	node = (dns_rbtnode_t *)(*targetp);
	locknum = node->locknum;

	LOCK(&rbtdb->node_locks[locknum].lock);

	INSIST(node->references > 0);
	node->references--;
	if (node->references == 0) {
		no_references(rbtdb, node, 0, isc_rwlocktype_none);
		if (rbtdb->node_locks[locknum].references == 0 &&
		    rbtdb->node_locks[locknum].exiting)
			inactive = ISC_TRUE;
	}

	UNLOCK(&rbtdb->node_locks[locknum].lock);

	*targetp = NULL;

	if (inactive) {
		LOCK(&rbtdb->lock);
		rbtdb->active--;
		if (rbtdb->active == 0)
			want_free = ISC_TRUE;
		UNLOCK(&rbtdb->lock);
		if (want_free) {
			char buf[DNS_NAME_FORMATSIZE];
			if (dns_name_dynamic(&rbtdb->common.origin))
				dns_name_format(&rbtdb->common.origin, buf,
						sizeof(buf));
			else
				strcpy(buf, "<UNKNOWN>");
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
				      DNS_LOGMODULE_CACHE, ISC_LOG_DEBUG(1),
				      "calling free_rbtdb(%s)", buf);
			free_rbtdb(rbtdb, ISC_TRUE, NULL);
		}
	}
}

static isc_result_t
expirenode(dns_db_t *db, dns_dbnode_t *node, isc_stdtime_t now) {
	dns_rbtdb_t *rbtdb = (dns_rbtdb_t *)db;
	dns_rbtnode_t *rbtnode = node;
	rdatasetheader_t *header;
	isc_boolean_t force_expire = ISC_FALSE;
	/*
	 * These are the category and module used by the cache cleaner.
	 */
	isc_boolean_t log = ISC_FALSE;
	isc_logcategory_t *category = DNS_LOGCATEGORY_DATABASE;
	isc_logmodule_t *module = DNS_LOGMODULE_CACHE;
	int level = ISC_LOG_DEBUG(2);
	char printname[DNS_NAME_FORMATSIZE];

	REQUIRE(VALID_RBTDB(rbtdb));

	/*
	 * Caller must hold a tree lock.
	 */

	if (now == 0)
		isc_stdtime_get(&now);

	if (rbtdb->overmem) {
		isc_uint32_t val;

		isc_random_get(&val);
		/*
		 * XXXDCL Could stand to have a better policy, like LRU.
		 */
		force_expire = ISC_TF(rbtnode->down == NULL && val % 4 == 0);

		/*
		 * Note that 'log' can be true IFF rbtdb->overmem is also true.
		 * rbtdb->ovemem can currently only be true for cache databases
		 * -- hence all of the "overmem cache" log strings.
		 */
		log = ISC_TF(isc_log_wouldlog(dns_lctx, level));
		if (log)
			isc_log_write(dns_lctx, category, module, level,
				      "overmem cache: %s %s",
				      force_expire ? "FORCE" : "check",
				      dns_rbt_formatnodename(rbtnode,
							   printname,
							   sizeof(printname)));
	}

	LOCK(&rbtdb->node_locks[rbtnode->locknum].lock);

	for (header = rbtnode->data; header != NULL; header = header->next)
		if (header->ttl <= now - RBTDB_VIRTUAL) {
			/*
			 * We don't check if rbtnode->references == 0 and try
			 * to free like we do in cache_find(), because
			 * rbtnode->references must be non-zero.  This is so
			 * because 'node' is an argument to the function.
			 */
			header->attributes |= RDATASET_ATTR_STALE;
			rbtnode->dirty = 1;
			if (log)
				isc_log_write(dns_lctx, category, module,
					      level, "overmem cache: stale %s",
					      printname);
		} else if (force_expire) {
			if (! RETAIN(header)) {
				header->ttl = 0;
				header->attributes |= RDATASET_ATTR_STALE;
				rbtnode->dirty = 1;
			} else if (log) {
				isc_log_write(dns_lctx, category, module,
					      level, "overmem cache: "
					      "reprieve by RETAIN() %s",
					      printname);
			}
		} else if (rbtdb->overmem && log)
			isc_log_write(dns_lctx, category, module, level,
				      "overmem cache: saved %s", printname);

	UNLOCK(&rbtdb->node_locks[rbtnode->locknum].lock);

	return (ISC_R_SUCCESS);
}

static void
overmem(dns_db_t *db, isc_boolean_t overmem) {
	dns_rbtdb_t *rbtdb = (dns_rbtdb_t *)db;

	if (IS_CACHE(rbtdb)) {
		rbtdb->overmem = overmem;
	}
}

static void
printnode(dns_db_t *db, dns_dbnode_t *node, FILE *out) {
	dns_rbtdb_t *rbtdb = (dns_rbtdb_t *)db;
	dns_rbtnode_t *rbtnode = node;
	isc_boolean_t first;

	REQUIRE(VALID_RBTDB(rbtdb));

	LOCK(&rbtdb->node_locks[rbtnode->locknum].lock);

	fprintf(out, "node %p, %u references, locknum = %u\n",
		rbtnode, rbtnode->references, rbtnode->locknum);
	if (rbtnode->data != NULL) {
		rdatasetheader_t *current, *top_next;

		for (current = rbtnode->data; current != NULL;
		     current = top_next) {
			top_next = current->next;
			first = ISC_TRUE;
			fprintf(out, "\ttype %u", current->type);
			do {
				if (!first)
					fprintf(out, "\t");
				first = ISC_FALSE;
				fprintf(out,
					"\tserial = %lu, ttl = %u, "
					"trust = %u, attributes = %u\n",
					(unsigned long)current->serial,
					current->ttl,
					current->trust,
					current->attributes);
				current = current->down;
			} while (current != NULL);
		}
	} else
		fprintf(out, "(empty)\n");

	UNLOCK(&rbtdb->node_locks[rbtnode->locknum].lock);
}

static isc_result_t
createiterator(dns_db_t *db, isc_boolean_t relative_names,
	       dns_dbiterator_t **iteratorp)
{
	dns_rbtdb_t *rbtdb = (dns_rbtdb_t *)db;
	rbtdb_dbiterator_t *rbtdbiter;

	REQUIRE(VALID_RBTDB(rbtdb));

	rbtdbiter = isc_mem_get(rbtdb->common.mctx, sizeof(*rbtdbiter));
	if (rbtdbiter == NULL)
		return (ISC_R_NOMEMORY);

	rbtdbiter->common.methods = &dbiterator_methods;
	rbtdbiter->common.db = NULL;
	dns_db_attach(db, &rbtdbiter->common.db);
	rbtdbiter->common.relative_names = relative_names;
	rbtdbiter->common.magic = DNS_DBITERATOR_MAGIC;
	rbtdbiter->common.cleaning = ISC_FALSE;
	rbtdbiter->paused = ISC_TRUE;
	rbtdbiter->tree_locked = isc_rwlocktype_none;
	rbtdbiter->result = ISC_R_SUCCESS;
	dns_fixedname_init(&rbtdbiter->name);
	dns_fixedname_init(&rbtdbiter->origin);
	rbtdbiter->node = NULL;
	rbtdbiter->delete = 0;
	memset(rbtdbiter->deletions, 0, sizeof(rbtdbiter->deletions));
	dns_rbtnodechain_init(&rbtdbiter->chain, db->mctx);

	*iteratorp = (dns_dbiterator_t *)rbtdbiter;

	return (ISC_R_SUCCESS);
}

static isc_result_t
zone_findrdataset(dns_db_t *db, dns_dbnode_t *node, dns_dbversion_t *version,
		  dns_rdatatype_t type, dns_rdatatype_t covers,
		  isc_stdtime_t now, dns_rdataset_t *rdataset,
		  dns_rdataset_t *sigrdataset)
{
	dns_rbtdb_t *rbtdb = (dns_rbtdb_t *)db;
	dns_rbtnode_t *rbtnode = (dns_rbtnode_t *)node;
	rdatasetheader_t *header, *header_next, *found, *foundsig;
	rbtdb_serial_t serial;
	rbtdb_version_t *rbtversion = version;
	isc_boolean_t close_version = ISC_FALSE;
	rbtdb_rdatatype_t matchtype, sigmatchtype;

	REQUIRE(VALID_RBTDB(rbtdb));
	REQUIRE(type != dns_rdatatype_any);

	if (rbtversion == NULL) {
		currentversion(db, (dns_dbversion_t **) (void *)(&rbtversion));
		close_version = ISC_TRUE;
	}
	serial = rbtversion->serial;
	now = 0;

	LOCK(&rbtdb->node_locks[rbtnode->locknum].lock);

	found = NULL;
	foundsig = NULL;
	matchtype = RBTDB_RDATATYPE_VALUE(type, covers);
	if (covers == 0)
		sigmatchtype = RBTDB_RDATATYPE_VALUE(dns_rdatatype_rrsig, type);
	else
		sigmatchtype = 0;

	for (header = rbtnode->data; header != NULL; header = header_next) {
		header_next = header->next;
		do {
			if (header->serial <= serial &&
			    !IGNORE(header)) {
				/*
				 * Is this a "this rdataset doesn't
				 * exist" record?
				 */
				if (NONEXISTENT(header))
					header = NULL;
				break;
			} else
				header = header->down;
		} while (header != NULL);
		if (header != NULL) {
			/*
			 * We have an active, extant rdataset.  If it's a
			 * type we're looking for, remember it.
			 */
			if (header->type == matchtype) {
				found = header;
				if (foundsig != NULL)
					break;
			} else if (header->type == sigmatchtype) {
				foundsig = header;
				if (found != NULL)
					break;
			}
		}
	}
	if (found != NULL) {
		bind_rdataset(rbtdb, rbtnode, found, now, rdataset);
		if (foundsig != NULL)
			bind_rdataset(rbtdb, rbtnode, foundsig, now,
				      sigrdataset);
	}

	UNLOCK(&rbtdb->node_locks[rbtnode->locknum].lock);

	if (close_version)
		closeversion(db, (dns_dbversion_t **) (void *)(&rbtversion),
			     ISC_FALSE);

	if (found == NULL)
		return (ISC_R_NOTFOUND);

	return (ISC_R_SUCCESS);
}

static isc_result_t
cache_findrdataset(dns_db_t *db, dns_dbnode_t *node, dns_dbversion_t *version,
		   dns_rdatatype_t type, dns_rdatatype_t covers,
		   isc_stdtime_t now, dns_rdataset_t *rdataset,
		   dns_rdataset_t *sigrdataset)
{
	dns_rbtdb_t *rbtdb = (dns_rbtdb_t *)db;
	dns_rbtnode_t *rbtnode = (dns_rbtnode_t *)node;
	rdatasetheader_t *header, *header_next, *found, *foundsig;
	rbtdb_rdatatype_t matchtype, sigmatchtype, nsectype;
	isc_result_t result;

	REQUIRE(VALID_RBTDB(rbtdb));
	REQUIRE(type != dns_rdatatype_any);

	UNUSED(version);

	result = ISC_R_SUCCESS;

	if (now == 0)
		isc_stdtime_get(&now);

	LOCK(&rbtdb->node_locks[rbtnode->locknum].lock);

	found = NULL;
	foundsig = NULL;
	matchtype = RBTDB_RDATATYPE_VALUE(type, covers);
	nsectype = RBTDB_RDATATYPE_VALUE(0, type);
	if (covers == 0)
		sigmatchtype = RBTDB_RDATATYPE_VALUE(dns_rdatatype_rrsig, type);
	else
		sigmatchtype = 0;

	for (header = rbtnode->data; header != NULL; header = header_next) {
		header_next = header->next;
		if (header->ttl <= now) {
			/*
			 * We don't check if rbtnode->references == 0 and try
			 * to free like we do in cache_find(), because
			 * rbtnode->references must be non-zero.  This is so
			 * because 'node' is an argument to the function.
			 */
			if (header->ttl <= now - RBTDB_VIRTUAL) {
				header->attributes |= RDATASET_ATTR_STALE;
				rbtnode->dirty = 1;
			}
		} else if (EXISTS(header)) {
			if (header->type == matchtype)
				found = header;
			else if (header->type == RBTDB_RDATATYPE_NCACHEANY ||
				 header->type == nsectype)
				found = header;
			else if (header->type == sigmatchtype)
				foundsig = header;
		}
	}
	if (found != NULL) {
		bind_rdataset(rbtdb, rbtnode, found, now, rdataset);
		if (foundsig != NULL)
			bind_rdataset(rbtdb, rbtnode, foundsig, now,
				      sigrdataset);
	}

	UNLOCK(&rbtdb->node_locks[rbtnode->locknum].lock);

	if (found == NULL)
		return (ISC_R_NOTFOUND);

	if (RBTDB_RDATATYPE_BASE(found->type) == 0) {
		/*
		 * We found a negative cache entry.
		 */
		if (NXDOMAIN(found))
			result = DNS_R_NCACHENXDOMAIN;
		else
			result = DNS_R_NCACHENXRRSET;
	}

	return (result);
}

static isc_result_t
allrdatasets(dns_db_t *db, dns_dbnode_t *node, dns_dbversion_t *version,
	     isc_stdtime_t now, dns_rdatasetiter_t **iteratorp)
{
	dns_rbtdb_t *rbtdb = (dns_rbtdb_t *)db;
	dns_rbtnode_t *rbtnode = (dns_rbtnode_t *)node;
	rbtdb_version_t *rbtversion = version;
	rbtdb_rdatasetiter_t *iterator;

	REQUIRE(VALID_RBTDB(rbtdb));

	iterator = isc_mem_get(rbtdb->common.mctx, sizeof(*iterator));
	if (iterator == NULL)
		return (ISC_R_NOMEMORY);

	if ((db->attributes & DNS_DBATTR_CACHE) == 0) {
		now = 0;
		if (rbtversion == NULL)
			currentversion(db,
				 (dns_dbversion_t **) (void *)(&rbtversion));
		else {
			LOCK(&rbtdb->lock);
			INSIST(rbtversion->references > 0);
			rbtversion->references++;
			INSIST(rbtversion->references != 0);
			UNLOCK(&rbtdb->lock);
		}
	} else {
		if (now == 0)
			isc_stdtime_get(&now);
		rbtversion = NULL;
	}

	iterator->common.magic = DNS_RDATASETITER_MAGIC;
	iterator->common.methods = &rdatasetiter_methods;
	iterator->common.db = db;
	iterator->common.node = node;
	iterator->common.version = (dns_dbversion_t *)rbtversion;
	iterator->common.now = now;

	LOCK(&rbtdb->node_locks[rbtnode->locknum].lock);

	INSIST(rbtnode->references > 0);
	rbtnode->references++;
	INSIST(rbtnode->references != 0);
	iterator->current = NULL;

	UNLOCK(&rbtdb->node_locks[rbtnode->locknum].lock);

	*iteratorp = (dns_rdatasetiter_t *)iterator;

	return (ISC_R_SUCCESS);
}

static isc_boolean_t
cname_and_other_data(dns_rbtnode_t *node, rbtdb_serial_t serial) {
	rdatasetheader_t *header, *header_next;
	isc_boolean_t cname, other_data;
	dns_rdatatype_t rdtype;

	/*
	 * The caller must hold the node lock.
	 */

	/*
	 * Look for CNAME and "other data" rdatasets active in our version.
	 */
	cname = ISC_FALSE;
	other_data = ISC_FALSE;
	for (header = node->data; header != NULL; header = header_next) {
		header_next = header->next;
		if (header->type == dns_rdatatype_cname) {
			/*
			 * Look for an active extant CNAME.
			 */
			do {
				if (header->serial <= serial &&
				    !IGNORE(header)) {
					/*
					 * Is this a "this rdataset doesn't
					 * exist" record?
					 */
					if (NONEXISTENT(header))
						header = NULL;
					break;
				} else
					header = header->down;
			} while (header != NULL);
			if (header != NULL)
				cname = ISC_TRUE;
		} else {
			/*
			 * Look for active extant "other data".
			 *
			 * "Other data" is any rdataset whose type is not
			 * DNSKEY, RRSIG DNSKEY, NSEC, RRSIG NSEC,
			 * or RRSIG CNAME.
			 */
			rdtype = RBTDB_RDATATYPE_BASE(header->type);
			if (rdtype == dns_rdatatype_rrsig ||
			    rdtype == dns_rdatatype_sig)
				rdtype = RBTDB_RDATATYPE_EXT(header->type);
			if (rdtype != dns_rdatatype_nsec &&
			    rdtype != dns_rdatatype_dnskey &&
			    rdtype != dns_rdatatype_nxt &&
			    rdtype != dns_rdatatype_key &&
			    rdtype != dns_rdatatype_cname) {
				/*
				 * We've found a type that isn't
				 * NSEC, KEY, CNAME, or one of their
				 * signatures.  Is it active and extant?
				 */
				do {
					if (header->serial <= serial &&
					    !IGNORE(header)) {
						/*
						 * Is this a "this rdataset
						 * doesn't exist" record?
						 */
						if (NONEXISTENT(header))
							header = NULL;
						break;
					} else
						header = header->down;
				} while (header != NULL);
				if (header != NULL)
					other_data = ISC_TRUE;
			}
		}
	}

	if (cname && other_data)
		return (ISC_TRUE);

	return (ISC_FALSE);
}

static isc_result_t
add(dns_rbtdb_t *rbtdb, dns_rbtnode_t *rbtnode, rbtdb_version_t *rbtversion,
    rdatasetheader_t *newheader, unsigned int options, isc_boolean_t loading,
    dns_rdataset_t *addedrdataset, isc_stdtime_t now)
{
	rbtdb_changed_t *changed = NULL;
	rdatasetheader_t *topheader, *topheader_prev, *header;
	unsigned char *merged;
	isc_result_t result;
	isc_boolean_t header_nx;
	isc_boolean_t newheader_nx;
	isc_boolean_t merge;
	dns_rdatatype_t nsectype, rdtype, covers;
	dns_trust_t trust;

	/*
	 * Add an rdatasetheader_t to a node.
	 */

	/*
	 * Caller must be holding the node lock.
	 */

	if ((options & DNS_DBADD_MERGE) != 0) {
		REQUIRE(rbtversion != NULL);
		merge = ISC_TRUE;
	} else
		merge = ISC_FALSE;

	if ((options & DNS_DBADD_FORCE) != 0)
		trust = dns_trust_ultimate;
	else
		trust = newheader->trust;

	if (rbtversion != NULL && !loading) {
		/*
		 * We always add a changed record, even if no changes end up
		 * being made to this node, because it's harmless and
		 * simplifies the code.
		 */
		changed = add_changed(rbtdb, rbtversion, rbtnode);
		if (changed == NULL) {
			free_rdataset(rbtdb->common.mctx, newheader);
			return (ISC_R_NOMEMORY);
		}
	}

	newheader_nx = NONEXISTENT(newheader) ? ISC_TRUE : ISC_FALSE;
	topheader_prev = NULL;

	nsectype = 0;
	if (rbtversion == NULL && !newheader_nx) {
		rdtype = RBTDB_RDATATYPE_BASE(newheader->type);
		if (rdtype == 0) {
			/*
			 * We're adding a negative cache entry.
			 */
			covers = RBTDB_RDATATYPE_EXT(newheader->type);
			if (covers == dns_rdatatype_any) {
				/*
				 * We're adding an NXDOMAIN negative cache
				 * entry.
				 *
				 * We make all other data stale so that the
				 * only rdataset that can be found at this
				 * node is the NXDOMAIN negative cache entry.
				 */
				for (topheader = rbtnode->data;
				     topheader != NULL;
				     topheader = topheader->next) {
					topheader->ttl = 0;
					topheader->attributes |=
						RDATASET_ATTR_STALE;
				}
				rbtnode->dirty = 1;
				goto find_header;
			}
			nsectype = RBTDB_RDATATYPE_VALUE(covers, 0);
		} else {
			/*
			 * We're adding something that isn't a
			 * negative cache entry.  Look for an extant
			 * non-stale NXDOMAIN negative cache entry.
			 */
			for (topheader = rbtnode->data;
			     topheader != NULL;
			     topheader = topheader->next) {
				if (NXDOMAIN(topheader))
					break;
			}
			if (topheader != NULL && EXISTS(topheader) &&
			    topheader->ttl > now) {
				/*
				 * Found one.
				 */
				if (trust < topheader->trust) {
					/*
					 * The NXDOMAIN is more trusted.
					 */
					free_rdataset(rbtdb->common.mctx,
						      newheader);
					if (addedrdataset != NULL)
						bind_rdataset(rbtdb, rbtnode,
							      topheader, now,
							      addedrdataset);
					return (DNS_R_UNCHANGED);
				}
				/*
				 * The new rdataset is better.  Expire the
				 * NXDOMAIN.
				 */
				topheader->ttl = 0;
				topheader->attributes |= RDATASET_ATTR_STALE;
				rbtnode->dirty = 1;
				topheader = NULL;
				goto find_header;
			}
			nsectype = RBTDB_RDATATYPE_VALUE(0, rdtype);
		}
	}

	for (topheader = rbtnode->data;
	     topheader != NULL;
	     topheader = topheader->next) {
		if (topheader->type == newheader->type ||
		    topheader->type == nsectype)
			break;
		topheader_prev = topheader;
	}

 find_header:
	/*
	 * If header isn't NULL, we've found the right type.  There may be
	 * IGNORE rdatasets between the top of the chain and the first real
	 * data.  We skip over them.
	 */
	header = topheader;
	while (header != NULL && IGNORE(header))
		header = header->down;
	if (header != NULL) {
		header_nx = NONEXISTENT(header) ? ISC_TRUE : ISC_FALSE;

		/*
		 * Deleting an already non-existent rdataset has no effect.
		 */
		if (header_nx && newheader_nx) {
			free_rdataset(rbtdb->common.mctx, newheader);
			return (DNS_R_UNCHANGED);
		}

		/*
		 * Trying to add an rdataset with lower trust to a cache DB
		 * has no effect, provided that the cache data isn't stale.
		 */
		if (rbtversion == NULL && trust < header->trust &&
		    (header->ttl > now || header_nx)) {
			free_rdataset(rbtdb->common.mctx, newheader);
			if (addedrdataset != NULL)
				bind_rdataset(rbtdb, rbtnode, header, now,
					      addedrdataset);
			return (DNS_R_UNCHANGED);
		}

		/*
		 * Don't merge if a nonexistent rdataset is involved.
		 */
		if (merge && (header_nx || newheader_nx))
			merge = ISC_FALSE;

		/*
		 * If 'merge' is ISC_TRUE, we'll try to create a new rdataset
		 * that is the union of 'newheader' and 'header'.
		 */
		if (merge) {
			unsigned int flags = 0;
			INSIST(rbtversion->serial >= header->serial);
			merged = NULL;
			result = ISC_R_SUCCESS;
			
			if ((options & DNS_DBADD_EXACT) != 0)
				flags |= DNS_RDATASLAB_EXACT;
			if ((options & DNS_DBADD_EXACTTTL) != 0 &&
			     newheader->ttl != header->ttl)
					result = DNS_R_NOTEXACT;
			else if (newheader->ttl != header->ttl)
				flags |= DNS_RDATASLAB_FORCE;
			if (result == ISC_R_SUCCESS)
				result = dns_rdataslab_merge(
					     (unsigned char *)header,
					     (unsigned char *)newheader,
					     (unsigned int)(sizeof(*newheader)),
					     rbtdb->common.mctx,
					     rbtdb->common.rdclass,
					     (dns_rdatatype_t)header->type,
					     flags, &merged);
			if (result == ISC_R_SUCCESS) {
				/*
				 * If 'header' has the same serial number as
				 * we do, we could clean it up now if we knew
				 * that our caller had no references to it.
				 * We don't know this, however, so we leave it
				 * alone.  It will get cleaned up when
				 * clean_zone_node() runs.
				 */
				free_rdataset(rbtdb->common.mctx, newheader);
				newheader = (rdatasetheader_t *)merged;
			} else {
				free_rdataset(rbtdb->common.mctx, newheader);
				return (result);
			}
		}
		/*
		 * Don't replace existing NS, A and AAAA RRsets
		 * in the cache if they are already exist.  This
		 * prevents named being locked to old servers.
		 * Don't lower trust of existing record if the
		 * update is forced.
		 */
		if (IS_CACHE(rbtdb) && header->ttl > now &&
		    header->type == dns_rdatatype_ns &&
		    !header_nx && !newheader_nx &&
		    header->trust >= newheader->trust &&
		    dns_rdataslab_equalx((unsigned char *)header,
					 (unsigned char *)newheader,
				         (unsigned int)(sizeof(*newheader)),
					 rbtdb->common.rdclass,
				         (dns_rdatatype_t)header->type)) {
			/*
			 * Honour the new ttl if it is less than the
			 * older one.
			 */
			if (header->ttl > newheader->ttl)
				header->ttl = newheader->ttl;
			if (header->noqname == NULL &&
			    newheader->noqname != NULL) {
				header->noqname = newheader->noqname;
				newheader->noqname = NULL;
			}
			free_rdataset(rbtdb->common.mctx, newheader);
			if (addedrdataset != NULL)
				bind_rdataset(rbtdb, rbtnode, header, now,
					      addedrdataset);
			return (ISC_R_SUCCESS);
		}
		if (IS_CACHE(rbtdb) && header->ttl > now &&
		    (header->type == dns_rdatatype_a ||
		     header->type == dns_rdatatype_aaaa) &&
		    !header_nx && !newheader_nx &&
		    header->trust >= newheader->trust &&
		    dns_rdataslab_equal((unsigned char *)header,
					(unsigned char *)newheader,
				        (unsigned int)(sizeof(*newheader)))) {
			/*
			 * Honour the new ttl if it is less than the
			 * older one.
			 */
			if (header->ttl > newheader->ttl)
				header->ttl = newheader->ttl;
			if (header->noqname == NULL &&
			    newheader->noqname != NULL) {
				header->noqname = newheader->noqname;
				newheader->noqname = NULL;
			}
			free_rdataset(rbtdb->common.mctx, newheader);
			if (addedrdataset != NULL)
				bind_rdataset(rbtdb, rbtnode, header, now,
					      addedrdataset);
			return (ISC_R_SUCCESS);
		}
		INSIST(rbtversion == NULL ||
		       rbtversion->serial >= topheader->serial);
		if (topheader_prev != NULL)
			topheader_prev->next = newheader;
		else
			rbtnode->data = newheader;
		newheader->next = topheader->next;
		if (loading) {
			/*
			 * There are no other references to 'header' when
			 * loading, so we MAY clean up 'header' now.
			 * Since we don't generate changed records when
			 * loading, we MUST clean up 'header' now.
			 */
			newheader->down = NULL;
			free_rdataset(rbtdb->common.mctx, header);
		} else {
			newheader->down = topheader;
			topheader->next = newheader;
			rbtnode->dirty = 1;
			if (changed != NULL)
				changed->dirty = ISC_TRUE;
		}
	} else {
		/*
		 * No non-IGNORED rdatasets of the given type exist at
		 * this node.
		 */

		/*
		 * If we're trying to delete the type, don't bother.
		 */
		if (newheader_nx) {
			free_rdataset(rbtdb->common.mctx, newheader);
			return (DNS_R_UNCHANGED);
		}

		if (topheader != NULL) {
			/*
			 * We have an list of rdatasets of the given type,
			 * but they're all marked IGNORE.  We simply insert
			 * the new rdataset at the head of the list.
			 *
			 * Ignored rdatasets cannot occur during loading, so
			 * we INSIST on it.
			 */
			INSIST(!loading);
			INSIST(rbtversion == NULL ||
			       rbtversion->serial >= topheader->serial);
			if (topheader_prev != NULL)
				topheader_prev->next = newheader;
			else
				rbtnode->data = newheader;
			newheader->next = topheader->next;
			newheader->down = topheader;
			topheader->next = newheader;
			rbtnode->dirty = 1;
			if (changed != NULL)
				changed->dirty = ISC_TRUE;
		} else {
			/*
			 * No rdatasets of the given type exist at the node.
			 */
			newheader->next = rbtnode->data;
			newheader->down = NULL;
			rbtnode->data = newheader;
		}
	}

	/*
	 * Check if the node now contains CNAME and other data.
	 */
	if (rbtversion != NULL &&
	    cname_and_other_data(rbtnode, rbtversion->serial))
		return (DNS_R_CNAMEANDOTHER);

	if (addedrdataset != NULL)
		bind_rdataset(rbtdb, rbtnode, newheader, now, addedrdataset);

	return (ISC_R_SUCCESS);
}

static inline isc_boolean_t
delegating_type(dns_rbtdb_t *rbtdb, dns_rbtnode_t *node,
		rbtdb_rdatatype_t type)
{
	if (IS_CACHE(rbtdb)) {
		if (type == dns_rdatatype_dname)
			return (ISC_TRUE);
		else
			return (ISC_FALSE);
	} else if (type == dns_rdatatype_dname ||
		   (type == dns_rdatatype_ns &&
		    (node != rbtdb->origin_node || IS_STUB(rbtdb))))
		return (ISC_TRUE);
	return (ISC_FALSE);
}

static inline isc_result_t
addnoqname(dns_rbtdb_t *rbtdb, rdatasetheader_t *newheader,
	   dns_rdataset_t *rdataset)
{
	struct noqname *noqname;
	isc_mem_t *mctx = rbtdb->common.mctx;
	dns_name_t name;
	dns_rdataset_t nsec, nsecsig;
	isc_result_t result;
	isc_region_t r;

	dns_name_init(&name, NULL);
	dns_rdataset_init(&nsec);
	dns_rdataset_init(&nsecsig);

	result = dns_rdataset_getnoqname(rdataset, &name, &nsec, &nsecsig);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);

	noqname = isc_mem_get(mctx, sizeof(*noqname));
	if (noqname == NULL) {
		result = ISC_R_NOMEMORY;
		goto cleanup;
	}
	dns_name_init(&noqname->name, NULL);
	noqname->nsec = NULL;
	noqname->nsecsig = NULL;
	result = dns_name_dup(&name, mctx, &noqname->name);
	if (result != ISC_R_SUCCESS)
		goto cleanup;
	result = dns_rdataslab_fromrdataset(&nsec, mctx, &r, 0);
	if (result != ISC_R_SUCCESS)
		goto cleanup;
	noqname->nsec = r.base;
	result = dns_rdataslab_fromrdataset(&nsecsig, mctx, &r, 0);
	if (result != ISC_R_SUCCESS)
		goto cleanup;
	noqname->nsecsig = r.base;
	dns_rdataset_disassociate(&nsec);
	dns_rdataset_disassociate(&nsecsig);
	newheader->noqname = noqname;
	return (ISC_R_SUCCESS);

cleanup:
	dns_rdataset_disassociate(&nsec);
	dns_rdataset_disassociate(&nsecsig);
	free_noqname(mctx, &noqname);
	return(result);
}

static isc_result_t
addrdataset(dns_db_t *db, dns_dbnode_t *node, dns_dbversion_t *version,
	    isc_stdtime_t now, dns_rdataset_t *rdataset, unsigned int options,
	    dns_rdataset_t *addedrdataset)
{
	dns_rbtdb_t *rbtdb = (dns_rbtdb_t *)db;
	dns_rbtnode_t *rbtnode = (dns_rbtnode_t *)node;
	rbtdb_version_t *rbtversion = version;
	isc_region_t region;
	rdatasetheader_t *newheader;
	isc_result_t result;
	isc_boolean_t delegating;

	REQUIRE(VALID_RBTDB(rbtdb));

	if (rbtversion == NULL) {
		if (now == 0)
			isc_stdtime_get(&now);
	} else
		now = 0;

	result = dns_rdataslab_fromrdataset(rdataset, rbtdb->common.mctx,
					    &region,
					    sizeof(rdatasetheader_t));
	if (result != ISC_R_SUCCESS)
		return (result);

	newheader = (rdatasetheader_t *)region.base;
	newheader->ttl = rdataset->ttl + now;
	newheader->type = RBTDB_RDATATYPE_VALUE(rdataset->type,
						rdataset->covers);
	newheader->attributes = 0;
	newheader->noqname = NULL;
	newheader->count = 0;
	newheader->trust = rdataset->trust;
	if (rbtversion != NULL) {
		newheader->serial = rbtversion->serial;
		now = 0;
	} else {
		newheader->serial = 1;
		if ((rdataset->attributes & DNS_RDATASETATTR_NXDOMAIN) != 0)
			newheader->attributes |= RDATASET_ATTR_NXDOMAIN;
		if ((rdataset->attributes & DNS_RDATASETATTR_NOQNAME) != 0) {
			result = addnoqname(rbtdb, newheader, rdataset);
			if (result != ISC_R_SUCCESS) {
				free_rdataset(rbtdb->common.mctx, newheader);
				return (result);
			}
		}
	}

	/*
	 * If we're adding a delegation type (e.g. NS or DNAME for a zone,
	 * just DNAME for the cache), then we need to set the callback bit
	 * on the node, and to do that we must be holding an exclusive lock
	 * on the tree.
	 */
	if (delegating_type(rbtdb, rbtnode, rdataset->type)) {
		delegating = ISC_TRUE;
		RWLOCK(&rbtdb->tree_lock, isc_rwlocktype_write);
	} else
		delegating = ISC_FALSE;

	LOCK(&rbtdb->node_locks[rbtnode->locknum].lock);

	result = add(rbtdb, rbtnode, rbtversion, newheader, options, ISC_FALSE,
		     addedrdataset, now);
	if (result == ISC_R_SUCCESS && delegating)
		rbtnode->find_callback = 1;

	UNLOCK(&rbtdb->node_locks[rbtnode->locknum].lock);

	if (delegating)
		RWUNLOCK(&rbtdb->tree_lock, isc_rwlocktype_write);

	return (result);
}

static isc_result_t
subtractrdataset(dns_db_t *db, dns_dbnode_t *node, dns_dbversion_t *version,
		 dns_rdataset_t *rdataset, unsigned int options,
		 dns_rdataset_t *newrdataset)
{
	dns_rbtdb_t *rbtdb = (dns_rbtdb_t *)db;
	dns_rbtnode_t *rbtnode = (dns_rbtnode_t *)node;
	rbtdb_version_t *rbtversion = version;
	rdatasetheader_t *topheader, *topheader_prev, *header, *newheader;
	unsigned char *subresult;
	isc_region_t region;
	isc_result_t result;
	rbtdb_changed_t *changed;

	REQUIRE(VALID_RBTDB(rbtdb));

	result = dns_rdataslab_fromrdataset(rdataset, rbtdb->common.mctx,
					    &region,
					    sizeof(rdatasetheader_t));
	if (result != ISC_R_SUCCESS)
		return (result);
	newheader = (rdatasetheader_t *)region.base;
	newheader->ttl = rdataset->ttl;
	newheader->type = RBTDB_RDATATYPE_VALUE(rdataset->type,
						rdataset->covers);
	newheader->attributes = 0;
	newheader->serial = rbtversion->serial;
	newheader->trust = 0;
	newheader->noqname = NULL;
	newheader->count = 0;

	LOCK(&rbtdb->node_locks[rbtnode->locknum].lock);

	changed = add_changed(rbtdb, rbtversion, rbtnode);
	if (changed == NULL) {
		free_rdataset(rbtdb->common.mctx, newheader);
		UNLOCK(&rbtdb->node_locks[rbtnode->locknum].lock);
		return (ISC_R_NOMEMORY);
	}

	topheader_prev = NULL;
	for (topheader = rbtnode->data;
	     topheader != NULL;
	     topheader = topheader->next) {
		if (topheader->type == newheader->type)
			break;
		topheader_prev = topheader;
	}
	/*
	 * If header isn't NULL, we've found the right type.  There may be
	 * IGNORE rdatasets between the top of the chain and the first real
	 * data.  We skip over them.
	 */
	header = topheader;
	while (header != NULL && IGNORE(header))
		header = header->down;
	if (header != NULL && EXISTS(header)) {
		unsigned int flags = 0;
		subresult = NULL;
		result = ISC_R_SUCCESS;
		if ((options & DNS_DBSUB_EXACT) != 0) {
			flags |= DNS_RDATASLAB_EXACT;
			if (newheader->ttl != header->ttl)
				result = DNS_R_NOTEXACT;
		}
		if (result == ISC_R_SUCCESS)
			result = dns_rdataslab_subtract(
					(unsigned char *)header,
					(unsigned char *)newheader,
					(unsigned int)(sizeof(*newheader)),
					rbtdb->common.mctx,
					rbtdb->common.rdclass,
					(dns_rdatatype_t)header->type,
					flags, &subresult);
		if (result == ISC_R_SUCCESS) {
			free_rdataset(rbtdb->common.mctx, newheader);
			newheader = (rdatasetheader_t *)subresult;
			/*
			 * We have to set the serial since the rdataslab
			 * subtraction routine copies the reserved portion of
			 * header, not newheader.
			 */
			newheader->serial = rbtversion->serial;
		} else if (result == DNS_R_NXRRSET) {
			/*
			 * This subtraction would remove all of the rdata;
			 * add a nonexistent header instead.
			 */
			free_rdataset(rbtdb->common.mctx, newheader);
			newheader = isc_mem_get(rbtdb->common.mctx,
						sizeof(*newheader));
			if (newheader == NULL) {
				result = ISC_R_NOMEMORY;
				goto unlock;
			}
			newheader->ttl = 0;
			newheader->type = topheader->type;
			newheader->attributes = RDATASET_ATTR_NONEXISTENT;
			newheader->trust = 0;
			newheader->serial = rbtversion->serial;
			newheader->noqname = NULL;
			newheader->count = 0;
		} else {
			free_rdataset(rbtdb->common.mctx, newheader);
			goto unlock;
		}

		/*
		 * If we're here, we want to link newheader in front of
		 * topheader.
		 */
		INSIST(rbtversion->serial >= topheader->serial);
		if (topheader_prev != NULL)
			topheader_prev->next = newheader;
		else
			rbtnode->data = newheader;
		newheader->next = topheader->next;
		newheader->down = topheader;
		topheader->next = newheader;
		rbtnode->dirty = 1;
		changed->dirty = ISC_TRUE;
	} else {
		/*
		 * The rdataset doesn't exist, so we don't need to do anything
		 * to satisfy the deletion request.
		 */
		free_rdataset(rbtdb->common.mctx, newheader);
		if ((options & DNS_DBSUB_EXACT) != 0)
			result = DNS_R_NOTEXACT;
		else
			result = DNS_R_UNCHANGED;			
	}

	if (result == ISC_R_SUCCESS && newrdataset != NULL)
		bind_rdataset(rbtdb, rbtnode, newheader, 0, newrdataset);

 unlock:
	UNLOCK(&rbtdb->node_locks[rbtnode->locknum].lock);

	return (result);
}

static isc_result_t
deleterdataset(dns_db_t *db, dns_dbnode_t *node, dns_dbversion_t *version,
	       dns_rdatatype_t type, dns_rdatatype_t covers)
{
	dns_rbtdb_t *rbtdb = (dns_rbtdb_t *)db;
	dns_rbtnode_t *rbtnode = (dns_rbtnode_t *)node;
	rbtdb_version_t *rbtversion = version;
	isc_result_t result;
	rdatasetheader_t *newheader;

	REQUIRE(VALID_RBTDB(rbtdb));

	if (type == dns_rdatatype_any)
		return (ISC_R_NOTIMPLEMENTED);
	if (type == dns_rdatatype_rrsig && covers == 0)
		return (ISC_R_NOTIMPLEMENTED);

	newheader = isc_mem_get(rbtdb->common.mctx, sizeof(*newheader));
	if (newheader == NULL)
		return (ISC_R_NOMEMORY);
	newheader->ttl = 0;
	newheader->type = RBTDB_RDATATYPE_VALUE(type, covers);
	newheader->attributes = RDATASET_ATTR_NONEXISTENT;
	newheader->trust = 0;
	newheader->noqname = NULL;
	if (rbtversion != NULL)
		newheader->serial = rbtversion->serial;
	else
		newheader->serial = 0;
	newheader->count = 0;

	LOCK(&rbtdb->node_locks[rbtnode->locknum].lock);

	result = add(rbtdb, rbtnode, rbtversion, newheader, DNS_DBADD_FORCE,
		     ISC_FALSE, NULL, 0);

	UNLOCK(&rbtdb->node_locks[rbtnode->locknum].lock);

	return (result);
}

static isc_result_t
loading_addrdataset(void *arg, dns_name_t *name, dns_rdataset_t *rdataset) {
	rbtdb_load_t *loadctx = arg;
	dns_rbtdb_t *rbtdb = loadctx->rbtdb;
	dns_rbtnode_t *node;
	isc_result_t result;
	isc_region_t region;
	rdatasetheader_t *newheader;

	/*
	 * This routine does no node locking.  See comments in
	 * 'load' below for more information on loading and
	 * locking.
	 */


	/*
	 * SOA records are only allowed at top of zone.
	 */
	if (rdataset->type == dns_rdatatype_soa &&
	    !IS_CACHE(rbtdb) && !dns_name_equal(name, &rbtdb->common.origin))
		return (DNS_R_NOTZONETOP);

	add_empty_wildcards(rbtdb, name);

	if (dns_name_iswildcard(name)) {
		/*
		 * NS record owners cannot legally be wild cards.
		 */
		if (rdataset->type == dns_rdatatype_ns)
			return (DNS_R_INVALIDNS);
		result = add_wildcard_magic(rbtdb, name);
		if (result != ISC_R_SUCCESS)
			return (result);
	}

	node = NULL;
	result = dns_rbt_addnode(rbtdb->tree, name, &node);
	if (result != ISC_R_SUCCESS && result != ISC_R_EXISTS)
		return (result);
	if (result != ISC_R_EXISTS) {
		dns_name_t foundname;
		dns_name_init(&foundname, NULL);
		dns_rbt_namefromnode(node, &foundname);
#ifdef DNS_RBT_USEHASH
		node->locknum = node->hashval % rbtdb->node_lock_count;
#else
		node->locknum = dns_name_hash(&foundname, ISC_TRUE) %
			rbtdb->node_lock_count;
#endif
	}

	result = dns_rdataslab_fromrdataset(rdataset, rbtdb->common.mctx,
					    &region,
					    sizeof(rdatasetheader_t));
	if (result != ISC_R_SUCCESS)
		return (result);
	newheader = (rdatasetheader_t *)region.base;
	newheader->ttl = rdataset->ttl + loadctx->now; /* XXX overflow check */
	newheader->type = RBTDB_RDATATYPE_VALUE(rdataset->type,
						rdataset->covers);
	newheader->attributes = 0;
	newheader->trust = rdataset->trust;
	newheader->serial = 1;
	newheader->noqname = NULL;
	newheader->count = 0;

	result = add(rbtdb, node, rbtdb->current_version, newheader,
		     DNS_DBADD_MERGE, ISC_TRUE, NULL, 0);
	if (result == ISC_R_SUCCESS &&
	    delegating_type(rbtdb, node, rdataset->type))
		node->find_callback = 1;
	else if (result == DNS_R_UNCHANGED)
		result = ISC_R_SUCCESS;

	return (result);
}

static isc_result_t
beginload(dns_db_t *db, dns_addrdatasetfunc_t *addp, dns_dbload_t **dbloadp) {
	rbtdb_load_t *loadctx;
	dns_rbtdb_t *rbtdb;

	rbtdb = (dns_rbtdb_t *)db;

	REQUIRE(VALID_RBTDB(rbtdb));

	loadctx = isc_mem_get(rbtdb->common.mctx, sizeof(*loadctx));
	if (loadctx == NULL)
		return (ISC_R_NOMEMORY);

	loadctx->rbtdb = rbtdb;
	if (IS_CACHE(rbtdb))
		isc_stdtime_get(&loadctx->now);
	else
		loadctx->now = 0;

	LOCK(&rbtdb->lock);

	REQUIRE((rbtdb->attributes & (RBTDB_ATTR_LOADED|RBTDB_ATTR_LOADING))
		== 0);
	rbtdb->attributes |= RBTDB_ATTR_LOADING;

	UNLOCK(&rbtdb->lock);

	*addp = loading_addrdataset;
	*dbloadp = loadctx;

	return (ISC_R_SUCCESS);
}

static isc_boolean_t
iszonesecure(dns_db_t *db, dns_dbnode_t *origin) {
	dns_rdataset_t keyset;
	dns_rdataset_t nsecset, signsecset;
	isc_boolean_t haszonekey = ISC_FALSE;
	isc_boolean_t hasnsec = ISC_FALSE;
	isc_result_t result;

	dns_rdataset_init(&keyset);
	result = dns_db_findrdataset(db, origin, NULL, dns_rdatatype_dnskey, 0,
				     0, &keyset, NULL);
	if (result == ISC_R_SUCCESS) {
		dns_rdata_t keyrdata = DNS_RDATA_INIT;
		result = dns_rdataset_first(&keyset);
		while (result == ISC_R_SUCCESS) {
			dns_rdataset_current(&keyset, &keyrdata);
			if (dns_zonekey_iszonekey(&keyrdata)) {
				haszonekey = ISC_TRUE;
				break;
			}
			result = dns_rdataset_next(&keyset);
		}
		dns_rdataset_disassociate(&keyset);
	}
	if (!haszonekey)
		return (ISC_FALSE);

	dns_rdataset_init(&nsecset);
	dns_rdataset_init(&signsecset);
	result = dns_db_findrdataset(db, origin, NULL, dns_rdatatype_nsec, 0,
				     0, &nsecset, &signsecset);
	if (result == ISC_R_SUCCESS) {
		if (dns_rdataset_isassociated(&signsecset)) {
			hasnsec = ISC_TRUE;
			dns_rdataset_disassociate(&signsecset);
		}
		dns_rdataset_disassociate(&nsecset);
	}
	return (hasnsec);

}

static isc_result_t
endload(dns_db_t *db, dns_dbload_t **dbloadp) {
	rbtdb_load_t *loadctx;
	dns_rbtdb_t *rbtdb = (dns_rbtdb_t *)db;

	REQUIRE(VALID_RBTDB(rbtdb));
	REQUIRE(dbloadp != NULL);
	loadctx = *dbloadp;
	REQUIRE(loadctx->rbtdb == rbtdb);

	LOCK(&rbtdb->lock);

	REQUIRE((rbtdb->attributes & RBTDB_ATTR_LOADING) != 0);
	REQUIRE((rbtdb->attributes & RBTDB_ATTR_LOADED) == 0);

	rbtdb->attributes &= ~RBTDB_ATTR_LOADING;
	rbtdb->attributes |= RBTDB_ATTR_LOADED;

	UNLOCK(&rbtdb->lock);

	/*
	 * If there's a KEY rdataset at the zone origin containing a
	 * zone key, we consider the zone secure.
	 */
	if (! IS_CACHE(rbtdb))
		rbtdb->secure = iszonesecure(db, rbtdb->origin_node);

	*dbloadp = NULL;

	isc_mem_put(rbtdb->common.mctx, loadctx, sizeof(*loadctx));

	return (ISC_R_SUCCESS);
}

static isc_result_t
dump(dns_db_t *db, dns_dbversion_t *version, const char *filename) {
	dns_rbtdb_t *rbtdb;

	rbtdb = (dns_rbtdb_t *)db;

	REQUIRE(VALID_RBTDB(rbtdb));

	return (dns_master_dump(rbtdb->common.mctx, db, version,
				&dns_master_style_default,
				filename));
}

static void
delete_callback(void *data, void *arg) {
	dns_rbtdb_t *rbtdb = arg;
	rdatasetheader_t *current, *next;

	for (current = data; current != NULL; current = next) {
		next = current->next;
		free_rdataset(rbtdb->common.mctx, current);
	}
}

static isc_boolean_t
issecure(dns_db_t *db) {
	dns_rbtdb_t *rbtdb;
	isc_boolean_t secure;

	rbtdb = (dns_rbtdb_t *)db;

	REQUIRE(VALID_RBTDB(rbtdb));

	RWLOCK(&rbtdb->tree_lock, isc_rwlocktype_read);
	secure = rbtdb->secure;
	RWUNLOCK(&rbtdb->tree_lock, isc_rwlocktype_read);

	return (secure);
}

static unsigned int
nodecount(dns_db_t *db) {
	dns_rbtdb_t *rbtdb;
	unsigned int count;

	rbtdb = (dns_rbtdb_t *)db;

	REQUIRE(VALID_RBTDB(rbtdb));

	RWLOCK(&rbtdb->tree_lock, isc_rwlocktype_read);
	count = dns_rbt_nodecount(rbtdb->tree);
	RWUNLOCK(&rbtdb->tree_lock, isc_rwlocktype_read);

	return (count);
}

static void
settask(dns_db_t *db, isc_task_t *task) {
	dns_rbtdb_t *rbtdb;

	rbtdb = (dns_rbtdb_t *)db;

	REQUIRE(VALID_RBTDB(rbtdb));

	LOCK(&rbtdb->lock);
	if (rbtdb->task != NULL)
		isc_task_detach(&rbtdb->task);
	if (task != NULL)
		isc_task_attach(task, &rbtdb->task);
	UNLOCK(&rbtdb->lock);
}

static isc_boolean_t
ispersistent(dns_db_t *db) {
	UNUSED(db);
	return (ISC_FALSE);
}

static dns_dbmethods_t zone_methods = {
	attach,
	detach,
	beginload,
	endload,
	dump,
	currentversion,
	newversion,
	attachversion,
	closeversion,
	findnode,
	zone_find,
	zone_findzonecut,
	attachnode,
	detachnode,
	expirenode,
	printnode,
	createiterator,
	zone_findrdataset,
	allrdatasets,
	addrdataset,
	subtractrdataset,
	deleterdataset,
	issecure,
	nodecount,
	ispersistent,
	overmem,
	settask
};

static dns_dbmethods_t cache_methods = {
	attach,
	detach,
	beginload,
	endload,
	dump,
	currentversion,
	newversion,
	attachversion,
	closeversion,
	findnode,
	cache_find,
	cache_findzonecut,
	attachnode,
	detachnode,
	expirenode,
	printnode,
	createiterator,
	cache_findrdataset,
	allrdatasets,
	addrdataset,
	subtractrdataset,
	deleterdataset,
	issecure,
	nodecount,
	ispersistent,
	overmem,
	settask
};

isc_result_t
#ifdef DNS_RBTDB_VERSION64
dns_rbtdb64_create
#else
dns_rbtdb_create
#endif
		(isc_mem_t *mctx, dns_name_t *origin, dns_dbtype_t type,
		 dns_rdataclass_t rdclass, unsigned int argc, char *argv[],
		 void *driverarg, dns_db_t **dbp)
{
	dns_rbtdb_t *rbtdb;
	isc_result_t result;
	int i;
	dns_name_t name;

	/* Keep the compiler happy. */
	UNUSED(argc);
	UNUSED(argv);
	UNUSED(driverarg);

	rbtdb = isc_mem_get(mctx, sizeof(*rbtdb));
	if (rbtdb == NULL)
		return (ISC_R_NOMEMORY);
	memset(rbtdb, '\0', sizeof(*rbtdb));
	dns_name_init(&rbtdb->common.origin, NULL);
	rbtdb->common.attributes = 0;
	if (type == dns_dbtype_cache) {
		rbtdb->common.methods = &cache_methods;
		rbtdb->common.attributes |= DNS_DBATTR_CACHE;
	} else if (type == dns_dbtype_stub) {
		rbtdb->common.methods = &zone_methods;
		rbtdb->common.attributes |= DNS_DBATTR_STUB;
	} else
		rbtdb->common.methods = &zone_methods;
	rbtdb->common.rdclass = rdclass;
	rbtdb->common.mctx = NULL;

	result = isc_mutex_init(&rbtdb->lock);
	if (result != ISC_R_SUCCESS) {
		isc_mem_put(mctx, rbtdb, sizeof(*rbtdb));
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "isc_mutex_init() failed: %s",
				 isc_result_totext(result));
		return (ISC_R_UNEXPECTED);
	}

	result = isc_rwlock_init(&rbtdb->tree_lock, 0, 0);
	if (result != ISC_R_SUCCESS) {
		DESTROYLOCK(&rbtdb->lock);
		isc_mem_put(mctx, rbtdb, sizeof(*rbtdb));
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "isc_rwlock_init() failed: %s",
				 isc_result_totext(result));
		return (ISC_R_UNEXPECTED);
	}

	INSIST(rbtdb->node_lock_count < (1 << DNS_RBT_LOCKLENGTH));

	if (rbtdb->node_lock_count == 0)
		rbtdb->node_lock_count = DEFAULT_NODE_LOCK_COUNT;
	rbtdb->node_locks = isc_mem_get(mctx, rbtdb->node_lock_count *
					sizeof(rbtdb_nodelock_t));
	rbtdb->active = rbtdb->node_lock_count;
	for (i = 0; i < (int)(rbtdb->node_lock_count); i++) {
		result = isc_mutex_init(&rbtdb->node_locks[i].lock);
		if (result != ISC_R_SUCCESS) {
			i--;
			while (i >= 0) {
				DESTROYLOCK(&rbtdb->node_locks[i].lock);
				i--;
			}
			isc_mem_put(mctx, rbtdb->node_locks,
				    rbtdb->node_lock_count *
				    sizeof(rbtdb_nodelock_t));
			isc_rwlock_destroy(&rbtdb->tree_lock);
			DESTROYLOCK(&rbtdb->lock);
			isc_mem_put(mctx, rbtdb, sizeof(*rbtdb));
			UNEXPECTED_ERROR(__FILE__, __LINE__,
					 "isc_mutex_init() failed: %s",
					 isc_result_totext(result));
			return (ISC_R_UNEXPECTED);
		}
		rbtdb->node_locks[i].references = 0;
		rbtdb->node_locks[i].exiting = ISC_FALSE;
	}

	/*
	 * Attach to the mctx.  The database will persist so long as there
	 * are references to it, and attaching to the mctx ensures that our
	 * mctx won't disappear out from under us.
	 */
	isc_mem_attach(mctx, &rbtdb->common.mctx);

	/*
	 * Must be initalized before free_rbtdb() is called.
	 */
	isc_ondestroy_init(&rbtdb->common.ondest);

	/*
	 * Make a copy of the origin name.
	 */
	result = dns_name_dupwithoffsets(origin, mctx, &rbtdb->common.origin);
	if (result != ISC_R_SUCCESS) {
		free_rbtdb(rbtdb, ISC_FALSE, NULL);
		return (result);
	}

	/*
	 * Make the Red-Black Tree.
	 */
	result = dns_rbt_create(mctx, delete_callback, rbtdb, &rbtdb->tree);
	if (result != ISC_R_SUCCESS) {
		free_rbtdb(rbtdb, ISC_FALSE, NULL);
		return (result);
	}
	/*
	 * In order to set the node callback bit correctly in zone databases,
	 * we need to know if the node has the origin name of the zone.
	 * In loading_addrdataset() we could simply compare the new name
	 * to the origin name, but this is expensive.  Also, we don't know the
	 * node name in addrdataset(), so we need another way of knowing the
	 * zone's top.
	 *
	 * We now explicitly create a node for the zone's origin, and then
	 * we simply remember the node's address.  This is safe, because
	 * the top-of-zone node can never be deleted, nor can its address
	 * change.
	 */
	if (! IS_CACHE(rbtdb)) {
		rbtdb->origin_node = NULL;
		result = dns_rbt_addnode(rbtdb->tree, &rbtdb->common.origin,
					 &rbtdb->origin_node);
		if (result != ISC_R_SUCCESS) {
			INSIST(result != ISC_R_EXISTS);
			free_rbtdb(rbtdb, ISC_FALSE, NULL);
			return (result);
		}
		/*
		 * We need to give the origin node the right locknum.
		 */
		dns_name_init(&name, NULL);
		dns_rbt_namefromnode(rbtdb->origin_node, &name);
#ifdef DNS_RBT_USEHASH
		rbtdb->origin_node->locknum =
			rbtdb->origin_node->hashval %
			rbtdb->node_lock_count;
#else
		rbtdb->origin_node->locknum =
			dns_name_hash(&name, ISC_TRUE) %
			rbtdb->node_lock_count;
#endif
	}

	/*
	 * Misc. Initialization.
	 */
	isc_refcount_init(&rbtdb->references, 1);
	rbtdb->attributes = 0;
	rbtdb->secure = ISC_FALSE;
	rbtdb->overmem = ISC_FALSE;
	rbtdb->task = NULL;

	/*
	 * Version Initialization.
	 */
	rbtdb->current_serial = 1;
	rbtdb->least_serial = 1;
	rbtdb->next_serial = 2;
	rbtdb->current_version = allocate_version(mctx, 1, 0, ISC_FALSE);
	if (rbtdb->current_version == NULL) {
		free_rbtdb(rbtdb, ISC_FALSE, NULL);
		return (ISC_R_NOMEMORY);
	}
	rbtdb->future_version = NULL;
	ISC_LIST_INIT(rbtdb->open_versions);

	rbtdb->common.magic = DNS_DB_MAGIC;
	rbtdb->common.impmagic = RBTDB_MAGIC;

	*dbp = (dns_db_t *)rbtdb;

	return (ISC_R_SUCCESS);
}


/*
 * Slabbed Rdataset Methods
 */

static void
rdataset_disassociate(dns_rdataset_t *rdataset) {
	dns_db_t *db = rdataset->private1;
	dns_dbnode_t *node = rdataset->private2;

	detachnode(db, &node);
}

static isc_result_t
rdataset_first(dns_rdataset_t *rdataset) {
	unsigned char *raw = rdataset->private3;
	unsigned int count;

	count = raw[0] * 256 + raw[1];
	if (count == 0) {
		rdataset->private5 = NULL;
		return (ISC_R_NOMORE);
	}
	raw += 2;
	/*
	 * The privateuint4 field is the number of rdata beyond the cursor
	 * position, so we decrement the total count by one before storing
	 * it.
	 */
	count--;
	rdataset->privateuint4 = count;
	rdataset->private5 = raw;

	return (ISC_R_SUCCESS);
}

static isc_result_t
rdataset_next(dns_rdataset_t *rdataset) {
	unsigned int count;
	unsigned int length;
	unsigned char *raw;

	count = rdataset->privateuint4;
	if (count == 0)
		return (ISC_R_NOMORE);
	count--;
	rdataset->privateuint4 = count;
	raw = rdataset->private5;
	length = raw[0] * 256 + raw[1];
	raw += length + 2;
	rdataset->private5 = raw;

	return (ISC_R_SUCCESS);
}

static void
rdataset_current(dns_rdataset_t *rdataset, dns_rdata_t *rdata) {
	unsigned char *raw = rdataset->private5;
	isc_region_t r;

	REQUIRE(raw != NULL);

	r.length = raw[0] * 256 + raw[1];
	raw += 2;
	r.base = raw;
	dns_rdata_fromregion(rdata, rdataset->rdclass, rdataset->type, &r);
}

static void
rdataset_clone(dns_rdataset_t *source, dns_rdataset_t *target) {
	dns_db_t *db = source->private1;
	dns_dbnode_t *node = source->private2;
	dns_dbnode_t *cloned_node = NULL;

	attachnode(db, node, &cloned_node);
	*target = *source;

	/*
	 * Reset iterator state.
	 */
	target->privateuint4 = 0;
	target->private5 = NULL;
}

static unsigned int
rdataset_count(dns_rdataset_t *rdataset) {
	unsigned char *raw = rdataset->private3;
	unsigned int count;

	count = raw[0] * 256 + raw[1];

	return (count);
}

static isc_result_t
rdataset_getnoqname(dns_rdataset_t *rdataset, dns_name_t *name,
		    dns_rdataset_t *nsec, dns_rdataset_t *nsecsig)
{
	dns_db_t *db = rdataset->private1;
	dns_dbnode_t *node = rdataset->private2;
	dns_dbnode_t *cloned_node;
	struct noqname *noqname = rdataset->private6;

	cloned_node = NULL;
	attachnode(db, node, &cloned_node);
	nsec->methods = &rdataset_methods;
	nsec->rdclass = db->rdclass;
	nsec->type = dns_rdatatype_nsec;
	nsec->covers = 0;
	nsec->ttl = rdataset->ttl;
	nsec->trust = rdataset->trust;
	nsec->private1 = rdataset->private1;
	nsec->private2 = rdataset->private2;
	nsec->private3 = noqname->nsec;
	nsec->privateuint4 = 0;
	nsec->private5 = NULL;
	nsec->private6 = NULL;

	cloned_node = NULL;
	attachnode(db, node, &cloned_node);
	nsecsig->methods = &rdataset_methods;
	nsecsig->rdclass = db->rdclass;
	nsecsig->type = dns_rdatatype_rrsig;
	nsecsig->covers = dns_rdatatype_nsec;
	nsecsig->ttl = rdataset->ttl;
	nsecsig->trust = rdataset->trust;
	nsecsig->private1 = rdataset->private1;
	nsecsig->private2 = rdataset->private2;
	nsecsig->private3 = noqname->nsecsig;
	nsecsig->privateuint4 = 0;
	nsecsig->private5 = NULL;
	nsec->private6 = NULL;

	dns_name_clone(&noqname->name, name);

	return (ISC_R_SUCCESS);
}

/*
 * Rdataset Iterator Methods
 */

static void
rdatasetiter_destroy(dns_rdatasetiter_t **iteratorp) {
	rbtdb_rdatasetiter_t *rbtiterator;

	rbtiterator = (rbtdb_rdatasetiter_t *)(*iteratorp);

	if (rbtiterator->common.version != NULL)
		closeversion(rbtiterator->common.db,
			     &rbtiterator->common.version, ISC_FALSE);
	detachnode(rbtiterator->common.db, &rbtiterator->common.node);
	isc_mem_put(rbtiterator->common.db->mctx, rbtiterator,
		    sizeof(*rbtiterator));

	*iteratorp = NULL;
}

static isc_result_t
rdatasetiter_first(dns_rdatasetiter_t *iterator) {
	rbtdb_rdatasetiter_t *rbtiterator = (rbtdb_rdatasetiter_t *)iterator;
	dns_rbtdb_t *rbtdb = (dns_rbtdb_t *)(rbtiterator->common.db);
	dns_rbtnode_t *rbtnode = rbtiterator->common.node;
	rbtdb_version_t *rbtversion = rbtiterator->common.version;
	rdatasetheader_t *header, *top_next;
	rbtdb_serial_t serial;
	isc_stdtime_t now;

	if (IS_CACHE(rbtdb)) {
		serial = 1;
		now = rbtiterator->common.now;
	} else {
		serial = rbtversion->serial;
		now = 0;
	}

	LOCK(&rbtdb->node_locks[rbtnode->locknum].lock);

	for (header = rbtnode->data; header != NULL; header = top_next) {
		top_next = header->next;
		do {
			if (header->serial <= serial && !IGNORE(header)) {
				/*
				 * Is this a "this rdataset doesn't exist"
				 * record?  Or is it too old in the cache?
				 *
				 * Note: unlike everywhere else, we
				 * check for now > header->ttl instead
				 * of now >= header->ttl.  This allows
				 * ANY and RRSIG queries for 0 TTL
				 * rdatasets to work.
				 */
				if (NONEXISTENT(header) ||
				    (now != 0 && now > header->ttl))
					header = NULL;
				break;
			} else
				header = header->down;
		} while (header != NULL);
		if (header != NULL)
			break;
	}

	UNLOCK(&rbtdb->node_locks[rbtnode->locknum].lock);

	rbtiterator->current = header;

	if (header == NULL)
		return (ISC_R_NOMORE);

	return (ISC_R_SUCCESS);
}

static isc_result_t
rdatasetiter_next(dns_rdatasetiter_t *iterator) {
	rbtdb_rdatasetiter_t *rbtiterator = (rbtdb_rdatasetiter_t *)iterator;
	dns_rbtdb_t *rbtdb = (dns_rbtdb_t *)(rbtiterator->common.db);
	dns_rbtnode_t *rbtnode = rbtiterator->common.node;
	rbtdb_version_t *rbtversion = rbtiterator->common.version;
	rdatasetheader_t *header, *top_next;
	rbtdb_serial_t serial;
	isc_stdtime_t now;
	rbtdb_rdatatype_t type;

	header = rbtiterator->current;
	if (header == NULL)
		return (ISC_R_NOMORE);

	if (IS_CACHE(rbtdb)) {
		serial = 1;
		now = rbtiterator->common.now;
	} else {
		serial = rbtversion->serial;
		now = 0;
	}

	LOCK(&rbtdb->node_locks[rbtnode->locknum].lock);

	type = header->type;
	for (header = header->next; header != NULL; header = top_next) {
		top_next = header->next;
		if (header->type != type) {
			do {
				if (header->serial <= serial &&
				    !IGNORE(header)) {
					/*
					 * Is this a "this rdataset doesn't
					 * exist" record?
					 *
					 * Note: unlike everywhere else, we
					 * check for now > header->ttl instead
					 * of now >= header->ttl.  This allows
					 * ANY and RRSIG queries for 0 TTL
					 * rdatasets to work.
					 */
					if ((header->attributes &
					     RDATASET_ATTR_NONEXISTENT) != 0 ||
					    (now != 0 && now > header->ttl))
						header = NULL;
					break;
				} else
					header = header->down;
			} while (header != NULL);
			if (header != NULL)
				break;
		}
	}

	UNLOCK(&rbtdb->node_locks[rbtnode->locknum].lock);

	rbtiterator->current = header;

	if (header == NULL)
		return (ISC_R_NOMORE);

	return (ISC_R_SUCCESS);
}

static void
rdatasetiter_current(dns_rdatasetiter_t *iterator, dns_rdataset_t *rdataset) {
	rbtdb_rdatasetiter_t *rbtiterator = (rbtdb_rdatasetiter_t *)iterator;
	dns_rbtdb_t *rbtdb = (dns_rbtdb_t *)(rbtiterator->common.db);
	dns_rbtnode_t *rbtnode = rbtiterator->common.node;
	rdatasetheader_t *header;

	header = rbtiterator->current;
	REQUIRE(header != NULL);

	LOCK(&rbtdb->node_locks[rbtnode->locknum].lock);

	bind_rdataset(rbtdb, rbtnode, header, rbtiterator->common.now,
		      rdataset);

	UNLOCK(&rbtdb->node_locks[rbtnode->locknum].lock);
}


/*
 * Database Iterator Methods
 */

static inline void
reference_iter_node(rbtdb_dbiterator_t *rbtdbiter) {
	dns_rbtdb_t *rbtdb = (dns_rbtdb_t *)rbtdbiter->common.db;
	dns_rbtnode_t *node = rbtdbiter->node;

	if (node == NULL)
		return;

	INSIST(rbtdbiter->tree_locked != isc_rwlocktype_none);
	LOCK(&rbtdb->node_locks[node->locknum].lock);
	new_reference(rbtdb, node);
	UNLOCK(&rbtdb->node_locks[node->locknum].lock);
}

static inline void
dereference_iter_node(rbtdb_dbiterator_t *rbtdbiter) {
	dns_rbtdb_t *rbtdb = (dns_rbtdb_t *)rbtdbiter->common.db;
	dns_rbtnode_t *node = rbtdbiter->node;
	isc_mutex_t *lock;

	if (node == NULL)
		return;

	lock = &rbtdb->node_locks[node->locknum].lock;
	LOCK(lock);
	INSIST(rbtdbiter->node->references > 0);
	if (--node->references == 0)
		no_references(rbtdb, node, 0, rbtdbiter->tree_locked);
	UNLOCK(lock);

	rbtdbiter->node = NULL;
}

static void
flush_deletions(rbtdb_dbiterator_t *rbtdbiter) {
	dns_rbtnode_t *node;
	dns_rbtdb_t *rbtdb = (dns_rbtdb_t *)rbtdbiter->common.db;
	isc_boolean_t was_read_locked = ISC_FALSE;
	isc_mutex_t *lock;
	int i;

	if (rbtdbiter->delete != 0) {
		/*
		 * Note that "%d node of %d in tree" can report things like
		 * "flush_deletions: 59 nodes of 41 in tree".  This means
		 * That some nodes appear on the deletions list more than
		 * once.  Only the last occurence will actually be deleted.
		 */
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_DATABASE,
			      DNS_LOGMODULE_CACHE, ISC_LOG_DEBUG(1),
			      "flush_deletions: %d nodes of %d in tree",
			      rbtdbiter->delete,
			      dns_rbt_nodecount(rbtdb->tree));

		if (rbtdbiter->tree_locked == isc_rwlocktype_read) {
			RWUNLOCK(&rbtdb->tree_lock, isc_rwlocktype_read);
			was_read_locked = ISC_TRUE;
		}
		RWLOCK(&rbtdb->tree_lock, isc_rwlocktype_write);
		rbtdbiter->tree_locked = isc_rwlocktype_write;

		for (i = 0; i < rbtdbiter->delete; i++) {
			node = rbtdbiter->deletions[i];
			lock = &rbtdb->node_locks[node->locknum].lock;

			LOCK(lock);
			INSIST(node->references > 0);
			node->references--;
			if (node->references == 0)
				no_references(rbtdb, node, 0,
					      rbtdbiter->tree_locked);
			UNLOCK(lock);
		}

		rbtdbiter->delete = 0;

		RWUNLOCK(&rbtdb->tree_lock, isc_rwlocktype_write);
		if (was_read_locked) {
			RWLOCK(&rbtdb->tree_lock, isc_rwlocktype_read);
			rbtdbiter->tree_locked = isc_rwlocktype_read;

		} else {
			rbtdbiter->tree_locked = isc_rwlocktype_none;
		}
	}
}

static inline void
resume_iteration(rbtdb_dbiterator_t *rbtdbiter) {
	dns_rbtdb_t *rbtdb = (dns_rbtdb_t *)rbtdbiter->common.db;

	REQUIRE(rbtdbiter->paused);
	REQUIRE(rbtdbiter->tree_locked == isc_rwlocktype_none);

	RWLOCK(&rbtdb->tree_lock, isc_rwlocktype_read);
	rbtdbiter->tree_locked = isc_rwlocktype_read;

	rbtdbiter->paused = ISC_FALSE;
}

static void
dbiterator_destroy(dns_dbiterator_t **iteratorp) {
	rbtdb_dbiterator_t *rbtdbiter = (rbtdb_dbiterator_t *)(*iteratorp);
	dns_rbtdb_t *rbtdb = (dns_rbtdb_t *)rbtdbiter->common.db;
	dns_db_t *db = NULL;

	if (rbtdbiter->tree_locked == isc_rwlocktype_read) {
		RWUNLOCK(&rbtdb->tree_lock, isc_rwlocktype_read);
		rbtdbiter->tree_locked = isc_rwlocktype_none;
	} else
		INSIST(rbtdbiter->tree_locked == isc_rwlocktype_none);

	dereference_iter_node(rbtdbiter);

	flush_deletions(rbtdbiter);

	dns_db_attach(rbtdbiter->common.db, &db);
	dns_db_detach(&rbtdbiter->common.db);

	dns_rbtnodechain_reset(&rbtdbiter->chain);
	isc_mem_put(db->mctx, rbtdbiter, sizeof(*rbtdbiter));
	dns_db_detach(&db);

	*iteratorp = NULL;
}

static isc_result_t
dbiterator_first(dns_dbiterator_t *iterator) {
	isc_result_t result;
	rbtdb_dbiterator_t *rbtdbiter = (rbtdb_dbiterator_t *)iterator;
	dns_rbtdb_t *rbtdb = (dns_rbtdb_t *)iterator->db;
	dns_name_t *name, *origin;

	if (rbtdbiter->result != ISC_R_SUCCESS &&
	    rbtdbiter->result != ISC_R_NOMORE)
		return (rbtdbiter->result);

	if (rbtdbiter->paused)
		resume_iteration(rbtdbiter);

	dereference_iter_node(rbtdbiter);

	name = dns_fixedname_name(&rbtdbiter->name);
	origin = dns_fixedname_name(&rbtdbiter->origin);
	dns_rbtnodechain_reset(&rbtdbiter->chain);

	result = dns_rbtnodechain_first(&rbtdbiter->chain, rbtdb->tree, name,
					origin);

	if (result == ISC_R_SUCCESS || result == DNS_R_NEWORIGIN) {
		result = dns_rbtnodechain_current(&rbtdbiter->chain, NULL,
						  NULL, &rbtdbiter->node);
		if (result == ISC_R_SUCCESS) {
			rbtdbiter->new_origin = ISC_TRUE;
			reference_iter_node(rbtdbiter);
		}
	} else {
		INSIST(result == ISC_R_NOTFOUND);
		result = ISC_R_NOMORE; /* The tree is empty. */
	}

	rbtdbiter->result = result;

	return (result);
}

static isc_result_t
dbiterator_last(dns_dbiterator_t *iterator) {
	isc_result_t result;
	rbtdb_dbiterator_t *rbtdbiter = (rbtdb_dbiterator_t *)iterator;
	dns_rbtdb_t *rbtdb = (dns_rbtdb_t *)iterator->db;
	dns_name_t *name, *origin;

	if (rbtdbiter->result != ISC_R_SUCCESS &&
	    rbtdbiter->result != ISC_R_NOMORE)
		return (rbtdbiter->result);

	if (rbtdbiter->paused)
		resume_iteration(rbtdbiter);

	dereference_iter_node(rbtdbiter);

	name = dns_fixedname_name(&rbtdbiter->name);
	origin = dns_fixedname_name(&rbtdbiter->origin);
	dns_rbtnodechain_reset(&rbtdbiter->chain);

	result = dns_rbtnodechain_last(&rbtdbiter->chain, rbtdb->tree, name,
				       origin);
	if (result == ISC_R_SUCCESS || result == DNS_R_NEWORIGIN) {
		result = dns_rbtnodechain_current(&rbtdbiter->chain, NULL,
						  NULL, &rbtdbiter->node);
		if (result == ISC_R_SUCCESS) {
			rbtdbiter->new_origin = ISC_TRUE;
			reference_iter_node(rbtdbiter);
		}
	} else {
		INSIST(result == ISC_R_NOTFOUND);
		result = ISC_R_NOMORE; /* The tree is empty. */
	}

	rbtdbiter->result = result;

	return (result);
}

static isc_result_t
dbiterator_seek(dns_dbiterator_t *iterator, dns_name_t *name) {
	isc_result_t result;
	rbtdb_dbiterator_t *rbtdbiter = (rbtdb_dbiterator_t *)iterator;
	dns_rbtdb_t *rbtdb = (dns_rbtdb_t *)iterator->db;
	dns_name_t *iname, *origin;

	if (rbtdbiter->result != ISC_R_SUCCESS &&
	    rbtdbiter->result != ISC_R_NOMORE)
		return (rbtdbiter->result);

	if (rbtdbiter->paused)
		resume_iteration(rbtdbiter);

	dereference_iter_node(rbtdbiter);

	iname = dns_fixedname_name(&rbtdbiter->name);
	origin = dns_fixedname_name(&rbtdbiter->origin);
	dns_rbtnodechain_reset(&rbtdbiter->chain);

	result = dns_rbt_findnode(rbtdb->tree, name, NULL, &rbtdbiter->node,
				  &rbtdbiter->chain, DNS_RBTFIND_EMPTYDATA,
				  NULL, NULL);
	if (result == ISC_R_SUCCESS) {
		result = dns_rbtnodechain_current(&rbtdbiter->chain, iname,
						  origin, NULL);
		if (result == ISC_R_SUCCESS) {
			rbtdbiter->new_origin = ISC_TRUE;
			reference_iter_node(rbtdbiter);
		}

	} else if (result == DNS_R_PARTIALMATCH)
		result = ISC_R_NOTFOUND;

	rbtdbiter->result = result;

	return (result);
}

static isc_result_t
dbiterator_prev(dns_dbiterator_t *iterator) {
	isc_result_t result;
	rbtdb_dbiterator_t *rbtdbiter = (rbtdb_dbiterator_t *)iterator;
	dns_name_t *name, *origin;

	REQUIRE(rbtdbiter->node != NULL);

	if (rbtdbiter->result != ISC_R_SUCCESS)
		return (rbtdbiter->result);

	if (rbtdbiter->paused)
		resume_iteration(rbtdbiter);

	name = dns_fixedname_name(&rbtdbiter->name);
	origin = dns_fixedname_name(&rbtdbiter->origin);
	result = dns_rbtnodechain_prev(&rbtdbiter->chain, name, origin);

	dereference_iter_node(rbtdbiter);

	if (result == DNS_R_NEWORIGIN || result == ISC_R_SUCCESS) {
		rbtdbiter->new_origin = ISC_TF(result == DNS_R_NEWORIGIN);
		result = dns_rbtnodechain_current(&rbtdbiter->chain, NULL,
						  NULL, &rbtdbiter->node);
	}

	if (result == ISC_R_SUCCESS)
		reference_iter_node(rbtdbiter);

	rbtdbiter->result = result;

	return (result);
}

static isc_result_t
dbiterator_next(dns_dbiterator_t *iterator) {
	isc_result_t result;
	rbtdb_dbiterator_t *rbtdbiter = (rbtdb_dbiterator_t *)iterator;
	dns_name_t *name, *origin;

	REQUIRE(rbtdbiter->node != NULL);

	if (rbtdbiter->result != ISC_R_SUCCESS)
		return (rbtdbiter->result);

	if (rbtdbiter->paused)
		resume_iteration(rbtdbiter);

	name = dns_fixedname_name(&rbtdbiter->name);
	origin = dns_fixedname_name(&rbtdbiter->origin);
	result = dns_rbtnodechain_next(&rbtdbiter->chain, name, origin);

	dereference_iter_node(rbtdbiter);

	if (result == DNS_R_NEWORIGIN || result == ISC_R_SUCCESS) {
		rbtdbiter->new_origin = ISC_TF(result == DNS_R_NEWORIGIN);
		result = dns_rbtnodechain_current(&rbtdbiter->chain, NULL,
						  NULL, &rbtdbiter->node);
	}
	if (result == ISC_R_SUCCESS)
		reference_iter_node(rbtdbiter);

	rbtdbiter->result = result;

	return (result);
}

static isc_result_t
dbiterator_current(dns_dbiterator_t *iterator, dns_dbnode_t **nodep,
		   dns_name_t *name)
{
	dns_rbtdb_t *rbtdb = (dns_rbtdb_t *)iterator->db;
	rbtdb_dbiterator_t *rbtdbiter = (rbtdb_dbiterator_t *)iterator;
	dns_rbtnode_t *node = rbtdbiter->node;
	isc_result_t result;
	dns_name_t *nodename = dns_fixedname_name(&rbtdbiter->name);
	dns_name_t *origin = dns_fixedname_name(&rbtdbiter->origin);

	REQUIRE(rbtdbiter->result == ISC_R_SUCCESS);
	REQUIRE(rbtdbiter->node != NULL);

	if (rbtdbiter->paused)
		resume_iteration(rbtdbiter);

	if (name != NULL) {
		if (rbtdbiter->common.relative_names)
			origin = NULL;
		result = dns_name_concatenate(nodename, origin, name, NULL);
		if (result != ISC_R_SUCCESS)
			return (result);
		if (rbtdbiter->common.relative_names && rbtdbiter->new_origin)
			result = DNS_R_NEWORIGIN;
	} else
		result = ISC_R_SUCCESS;

	LOCK(&rbtdb->node_locks[node->locknum].lock);
	new_reference(rbtdb, node);
	UNLOCK(&rbtdb->node_locks[node->locknum].lock);

	*nodep = rbtdbiter->node;

	if (iterator->cleaning && result == ISC_R_SUCCESS) {
		isc_result_t expire_result;

		/*
		 * If the deletion array is full, flush it before trying
		 * to expire the current node.  The current node can't
		 * fully deleted while the iteration cursor is still on it.
		 */
		if (rbtdbiter->delete == DELETION_BATCH_MAX)
			flush_deletions(rbtdbiter);

		expire_result = expirenode(iterator->db, *nodep, 0);

		/*
		 * expirenode() currently always returns success.
		 */
		if (expire_result == ISC_R_SUCCESS && node->down == NULL) {
			rbtdbiter->deletions[rbtdbiter->delete++] = node;
			LOCK(&rbtdb->node_locks[node->locknum].lock);
			node->references++;
			UNLOCK(&rbtdb->node_locks[node->locknum].lock);
		}
	}

	return (result);
}

static isc_result_t
dbiterator_pause(dns_dbiterator_t *iterator) {
	dns_rbtdb_t *rbtdb = (dns_rbtdb_t *)iterator->db;
	rbtdb_dbiterator_t *rbtdbiter = (rbtdb_dbiterator_t *)iterator;

	if (rbtdbiter->result != ISC_R_SUCCESS &&
	    rbtdbiter->result != ISC_R_NOMORE)
		return (rbtdbiter->result);

	if (rbtdbiter->paused)
		return (ISC_R_SUCCESS);

	rbtdbiter->paused = ISC_TRUE;

	if (rbtdbiter->tree_locked != isc_rwlocktype_none) {
		INSIST(rbtdbiter->tree_locked == isc_rwlocktype_read);
		RWUNLOCK(&rbtdb->tree_lock, isc_rwlocktype_read);
		rbtdbiter->tree_locked = isc_rwlocktype_none;
	}

	flush_deletions(rbtdbiter);

	return (ISC_R_SUCCESS);
}

static isc_result_t
dbiterator_origin(dns_dbiterator_t *iterator, dns_name_t *name) {
	rbtdb_dbiterator_t *rbtdbiter = (rbtdb_dbiterator_t *)iterator;
	dns_name_t *origin = dns_fixedname_name(&rbtdbiter->origin);

	if (rbtdbiter->result != ISC_R_SUCCESS)
		return (rbtdbiter->result);

	return (dns_name_copy(origin, name, NULL));
}
