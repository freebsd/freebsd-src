/*
 * Copyright (C) 2004-2009  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: zone.c,v 1.410.18.61 2009-09-24 21:38:52 jinmei Exp $ */

/*! \file */

#include <config.h>
#include <errno.h>

#include <isc/file.h>
#include <isc/mutex.h>
#include <isc/print.h>
#include <isc/random.h>
#include <isc/ratelimiter.h>
#include <isc/refcount.h>
#include <isc/rwlock.h>
#include <isc/serial.h>
#include <isc/strerror.h>
#include <isc/string.h>
#include <isc/taskpool.h>
#include <isc/timer.h>
#include <isc/util.h>

#include <dns/acache.h>
#include <dns/acl.h>
#include <dns/adb.h>
#include <dns/callbacks.h>
#include <dns/db.h>
#include <dns/dbiterator.h>
#include <dns/events.h>
#include <dns/journal.h>
#include <dns/log.h>
#include <dns/master.h>
#include <dns/masterdump.h>
#include <dns/message.h>
#include <dns/name.h>
#include <dns/peer.h>
#include <dns/rcode.h>
#include <dns/rdataclass.h>
#include <dns/rdatalist.h>
#include <dns/rdataset.h>
#include <dns/rdatastruct.h>
#include <dns/rdatatype.h>
#include <dns/request.h>
#include <dns/resolver.h>
#include <dns/result.h>
#include <dns/stats.h>
#include <dns/ssu.h>
#include <dns/tsig.h>
#include <dns/xfrin.h>
#include <dns/zone.h>

#define ZONE_MAGIC			ISC_MAGIC('Z', 'O', 'N', 'E')
#define DNS_ZONE_VALID(zone)		ISC_MAGIC_VALID(zone, ZONE_MAGIC)

#define NOTIFY_MAGIC			ISC_MAGIC('N', 't', 'f', 'y')
#define DNS_NOTIFY_VALID(notify)	ISC_MAGIC_VALID(notify, NOTIFY_MAGIC)

#define STUB_MAGIC			ISC_MAGIC('S', 't', 'u', 'b')
#define DNS_STUB_VALID(stub)		ISC_MAGIC_VALID(stub, STUB_MAGIC)

#define ZONEMGR_MAGIC			ISC_MAGIC('Z', 'm', 'g', 'r')
#define DNS_ZONEMGR_VALID(stub)		ISC_MAGIC_VALID(stub, ZONEMGR_MAGIC)

#define LOAD_MAGIC			ISC_MAGIC('L', 'o', 'a', 'd')
#define DNS_LOAD_VALID(load)		ISC_MAGIC_VALID(load, LOAD_MAGIC)

#define FORWARD_MAGIC			ISC_MAGIC('F', 'o', 'r', 'w')
#define DNS_FORWARD_VALID(load)		ISC_MAGIC_VALID(load, FORWARD_MAGIC)

#define IO_MAGIC			ISC_MAGIC('Z', 'm', 'I', 'O')
#define DNS_IO_VALID(load)		ISC_MAGIC_VALID(load, IO_MAGIC)

/*%
 * Ensure 'a' is at least 'min' but not more than 'max'.
 */
#define RANGE(a, min, max) \
		(((a) < (min)) ? (min) : ((a) < (max) ? (a) : (max)))

/*
 * Default values.
 */
#define DNS_DEFAULT_IDLEIN 3600		/*%< 1 hour */
#define DNS_DEFAULT_IDLEOUT 3600	/*%< 1 hour */
#define MAX_XFER_TIME (2*3600)		/*%< Documented default is 2 hours */

#ifndef DNS_MAX_EXPIRE
#define DNS_MAX_EXPIRE	14515200	/*%< 24 weeks */
#endif

#ifndef DNS_DUMP_DELAY
#define DNS_DUMP_DELAY 900		/*%< 15 minutes */
#endif

typedef struct dns_notify dns_notify_t;
typedef struct dns_stub dns_stub_t;
typedef struct dns_load dns_load_t;
typedef struct dns_forward dns_forward_t;
typedef struct dns_io dns_io_t;
typedef ISC_LIST(dns_io_t) dns_iolist_t;

#define DNS_ZONE_CHECKLOCK
#ifdef DNS_ZONE_CHECKLOCK
#define LOCK_ZONE(z) \
	 do { LOCK(&(z)->lock); \
	      INSIST((z)->locked == ISC_FALSE); \
	     (z)->locked = ISC_TRUE; \
		} while (0)
#define UNLOCK_ZONE(z) \
	do { (z)->locked = ISC_FALSE; UNLOCK(&(z)->lock); } while (0)
#define LOCKED_ZONE(z) ((z)->locked)
#else
#define LOCK_ZONE(z) LOCK(&(z)->lock)
#define UNLOCK_ZONE(z) UNLOCK(&(z)->lock)
#define LOCKED_ZONE(z) ISC_TRUE
#endif

#ifdef ISC_RWLOCK_USEATOMIC
#define ZONEDB_INITLOCK(l)	isc_rwlock_init((l), 0, 0)
#define ZONEDB_DESTROYLOCK(l)	isc_rwlock_destroy(l)
#define ZONEDB_LOCK(l, t)	RWLOCK((l), (t))
#define ZONEDB_UNLOCK(l, t)	RWUNLOCK((l), (t))
#else
#define ZONEDB_INITLOCK(l)	isc_mutex_init(l)
#define ZONEDB_DESTROYLOCK(l)	DESTROYLOCK(l)
#define ZONEDB_LOCK(l, t)	LOCK(l)
#define ZONEDB_UNLOCK(l, t)	UNLOCK(l)
#endif

struct dns_zone {
	/* Unlocked */
	unsigned int		magic;
	isc_mutex_t		lock;
#ifdef DNS_ZONE_CHECKLOCK
	isc_boolean_t		locked;
#endif
	isc_mem_t		*mctx;
	isc_refcount_t		erefs;

#ifdef ISC_RWLOCK_USEATOMIC
	isc_rwlock_t		dblock;
#else
	isc_mutex_t		dblock;
#endif
	dns_db_t		*db;		/* Locked by dblock */

	/* Locked */
	dns_zonemgr_t		*zmgr;
	ISC_LINK(dns_zone_t)	link;		/* Used by zmgr. */
	isc_timer_t		*timer;
	unsigned int		irefs;
	dns_name_t		origin;
	char			*masterfile;
	dns_masterformat_t	masterformat;
	char			*journal;
	isc_int32_t		journalsize;
	dns_rdataclass_t	rdclass;
	dns_zonetype_t		type;
	unsigned int		flags;
	unsigned int		options;
	unsigned int		db_argc;
	char			**db_argv;
	isc_time_t		expiretime;
	isc_time_t		refreshtime;
	isc_time_t		dumptime;
	isc_time_t		loadtime;
	isc_time_t		notifytime;
	isc_uint32_t		serial;
	isc_uint32_t		refresh;
	isc_uint32_t		retry;
	isc_uint32_t		expire;
	isc_uint32_t		minimum;
	char			*keydirectory;

	isc_uint32_t		maxrefresh;
	isc_uint32_t		minrefresh;
	isc_uint32_t		maxretry;
	isc_uint32_t		minretry;

	isc_sockaddr_t		*masters;
	dns_name_t		**masterkeynames;
	isc_boolean_t		*mastersok;
	unsigned int		masterscnt;
	unsigned int		curmaster;
	isc_sockaddr_t		masteraddr;
	dns_notifytype_t	notifytype;
	isc_sockaddr_t		*notify;
	unsigned int		notifycnt;
	isc_sockaddr_t		notifyfrom;
	isc_task_t		*task;
	isc_sockaddr_t		notifysrc4;
	isc_sockaddr_t		notifysrc6;
	isc_sockaddr_t		xfrsource4;
	isc_sockaddr_t		xfrsource6;
	isc_sockaddr_t		altxfrsource4;
	isc_sockaddr_t		altxfrsource6;
	isc_sockaddr_t		sourceaddr;
	dns_xfrin_ctx_t		*xfr;		/* task locked */
	dns_tsigkey_t		*tsigkey;	/* key used for xfr */
	/* Access Control Lists */
	dns_acl_t		*update_acl;
	dns_acl_t		*forward_acl;
	dns_acl_t		*notify_acl;
	dns_acl_t		*query_acl;
	dns_acl_t		*xfr_acl;
	isc_boolean_t		update_disabled;
	isc_boolean_t		zero_no_soa_ttl;
	dns_severity_t		check_names;
	ISC_LIST(dns_notify_t)	notifies;
	dns_request_t		*request;
	dns_loadctx_t		*lctx;
	dns_io_t		*readio;
	dns_dumpctx_t		*dctx;
	dns_io_t		*writeio;
	isc_uint32_t		maxxfrin;
	isc_uint32_t		maxxfrout;
	isc_uint32_t		idlein;
	isc_uint32_t		idleout;
	isc_event_t		ctlevent;
	dns_ssutable_t		*ssutable;
	isc_uint32_t		sigvalidityinterval;
	dns_view_t		*view;
	dns_acache_t		*acache;
	dns_checkmxfunc_t	checkmx;
	dns_checksrvfunc_t	checksrv;
	dns_checknsfunc_t	checkns;
	/*%
	 * Zones in certain states such as "waiting for zone transfer"
	 * or "zone transfer in progress" are kept on per-state linked lists
	 * in the zone manager using the 'statelink' field.  The 'statelist'
	 * field points at the list the zone is currently on.  It the zone
	 * is not on any such list, statelist is NULL.
	 */
	ISC_LINK(dns_zone_t)	statelink;
	dns_zonelist_t		*statelist;
	/*%
	 * Optional per-zone statistics counters (NULL if not present).
	 */
	isc_uint64_t	    	*counters;
	isc_uint32_t		notifydelay;
	dns_isselffunc_t	isself;
	void			*isselfarg;

	/*%
	 * Serial number for deferred journal compaction.
	 */
	isc_uint32_t		compact_serial;
};

#define DNS_ZONE_FLAG(z,f) (ISC_TF(((z)->flags & (f)) != 0))
#define DNS_ZONE_SETFLAG(z,f) do { \
		INSIST(LOCKED_ZONE(z)); \
		(z)->flags |= (f); \
		} while (0)
#define DNS_ZONE_CLRFLAG(z,f) do { \
		INSIST(LOCKED_ZONE(z)); \
		(z)->flags &= ~(f); \
		} while (0)
	/* XXX MPA these may need to go back into zone.h */
#define DNS_ZONEFLG_REFRESH	0x00000001U	/*%< refresh check in progress */
#define DNS_ZONEFLG_NEEDDUMP	0x00000002U	/*%< zone need consolidation */
#define DNS_ZONEFLG_USEVC	0x00000004U	/*%< use tcp for refresh query */
#define DNS_ZONEFLG_DUMPING	0x00000008U	/*%< a dump is in progress */
#define DNS_ZONEFLG_HASINCLUDE	0x00000010U	/*%< $INCLUDE in zone file */
#define DNS_ZONEFLG_LOADED	0x00000020U	/*%< database has loaded */
#define DNS_ZONEFLG_EXITING	0x00000040U	/*%< zone is being destroyed */
#define DNS_ZONEFLG_EXPIRED	0x00000080U	/*%< zone has expired */
#define DNS_ZONEFLG_NEEDREFRESH	0x00000100U	/*%< refresh check needed */
#define DNS_ZONEFLG_UPTODATE	0x00000200U	/*%< zone contents are
						 * uptodate */
#define DNS_ZONEFLG_NEEDNOTIFY	0x00000400U	/*%< need to send out notify
						 * messages */
#define DNS_ZONEFLG_DIFFONRELOAD 0x00000800U	/*%< generate a journal diff on
						 * reload */
#define DNS_ZONEFLG_NOMASTERS	0x00001000U	/*%< an attempt to refresh a
						 * zone with no masters
						 * occurred */
#define DNS_ZONEFLG_LOADING	0x00002000U	/*%< load from disk in progress*/
#define DNS_ZONEFLG_HAVETIMERS	0x00004000U	/*%< timer values have been set
						 * from SOA (if not set, we
						 * are still using
						 * default timer values) */
#define DNS_ZONEFLG_FORCEXFER   0x00008000U     /*%< Force a zone xfer */
#define DNS_ZONEFLG_NOREFRESH	0x00010000U
#define DNS_ZONEFLG_DIALNOTIFY	0x00020000U
#define DNS_ZONEFLG_DIALREFRESH	0x00040000U
#define DNS_ZONEFLG_SHUTDOWN	0x00080000U
#define DNS_ZONEFLAG_NOIXFR	0x00100000U	/*%< IXFR failed, force AXFR */
#define DNS_ZONEFLG_FLUSH	0x00200000U
#define DNS_ZONEFLG_NOEDNS	0x00400000U
#define DNS_ZONEFLG_USEALTXFRSRC 0x00800000U
#define DNS_ZONEFLG_SOABEFOREAXFR 0x01000000U
#define DNS_ZONEFLG_NEEDCOMPACT 0x02000000U
#define DNS_ZONEFLG_REFRESHING	0x04000000U	/*%< Refreshing keydata */
#define DNS_ZONEFLG_THAW	0x08000000U

#define DNS_ZONE_OPTION(z,o) (((z)->options & (o)) != 0)

/* Flags for zone_load() */
#define DNS_ZONELOADFLAG_NOSTAT	0x00000001U	/* Do not stat() master files */
#define DNS_ZONELOADFLAG_THAW	0x00000002U	/* Thaw the zone on successful
						   load. */

struct dns_zonemgr {
	unsigned int		magic;
	isc_mem_t *		mctx;
	int			refs;		/* Locked by rwlock */
	isc_taskmgr_t *		taskmgr;
	isc_timermgr_t *	timermgr;
	isc_socketmgr_t *	socketmgr;
	isc_taskpool_t *	zonetasks;
	isc_task_t *		task;
	isc_ratelimiter_t *	rl;
	isc_rwlock_t		rwlock;
	isc_mutex_t		iolock;

	/* Locked by rwlock. */
	dns_zonelist_t		zones;
	dns_zonelist_t		waiting_for_xfrin;
	dns_zonelist_t		xfrin_in_progress;

	/* Configuration data. */
	isc_uint32_t		transfersin;
	isc_uint32_t		transfersperns;
	unsigned int		serialqueryrate;

	/* Locked by iolock */
	isc_uint32_t		iolimit;
	isc_uint32_t		ioactive;
	dns_iolist_t		high;
	dns_iolist_t		low;
};

/*%
 * Hold notify state.
 */
struct dns_notify {
	unsigned int		magic;
	unsigned int		flags;
	isc_mem_t		*mctx;
	dns_zone_t		*zone;
	dns_adbfind_t		*find;
	dns_request_t		*request;
	dns_name_t		ns;
	isc_sockaddr_t		dst;
	ISC_LINK(dns_notify_t)	link;
};

#define DNS_NOTIFY_NOSOA	0x0001U

/*%
 *	dns_stub holds state while performing a 'stub' transfer.
 *	'db' is the zone's 'db' or a new one if this is the initial
 *	transfer.
 */

struct dns_stub {
	unsigned int		magic;
	isc_mem_t		*mctx;
	dns_zone_t		*zone;
	dns_db_t		*db;
	dns_dbversion_t		*version;
};

/*%
 *	Hold load state.
 */
struct dns_load {
	unsigned int		magic;
	isc_mem_t		*mctx;
	dns_zone_t		*zone;
	dns_db_t		*db;
	isc_time_t		loadtime;
	dns_rdatacallbacks_t	callbacks;
};

/*%
 *	Hold forward state.
 */
struct dns_forward {
	unsigned int		magic;
	isc_mem_t		*mctx;
	dns_zone_t		*zone;
	isc_buffer_t		*msgbuf;
	dns_request_t		*request;
	isc_uint32_t		which;
	isc_sockaddr_t		addr;
	dns_updatecallback_t	callback;
	void			*callback_arg;
};

/*%
 *	Hold IO request state.
 */
struct dns_io {
	unsigned int	magic;
	dns_zonemgr_t	*zmgr;
	isc_boolean_t	high;
	isc_task_t	*task;
	ISC_LINK(dns_io_t) link;
	isc_event_t	*event;
};

#define SEND_BUFFER_SIZE 2048

static void zone_settimer(dns_zone_t *, isc_time_t *);
static void cancel_refresh(dns_zone_t *);
static void zone_debuglog(dns_zone_t *zone, const char *, int debuglevel,
			  const char *msg, ...) ISC_FORMAT_PRINTF(4, 5);
static void notify_log(dns_zone_t *zone, int level, const char *fmt, ...)
     ISC_FORMAT_PRINTF(3, 4);
static void queue_xfrin(dns_zone_t *zone);
static void zone_unload(dns_zone_t *zone);
static void zone_expire(dns_zone_t *zone);
static void zone_iattach(dns_zone_t *source, dns_zone_t **target);
static void zone_idetach(dns_zone_t **zonep);
static isc_result_t zone_replacedb(dns_zone_t *zone, dns_db_t *db,
				   isc_boolean_t dump);
static inline void zone_attachdb(dns_zone_t *zone, dns_db_t *db);
static inline void zone_detachdb(dns_zone_t *zone);
static isc_result_t default_journal(dns_zone_t *zone);
static void zone_xfrdone(dns_zone_t *zone, isc_result_t result);
static isc_result_t zone_postload(dns_zone_t *zone, dns_db_t *db,
				  isc_time_t loadtime, isc_result_t result);
static void zone_needdump(dns_zone_t *zone, unsigned int delay);
static void zone_shutdown(isc_task_t *, isc_event_t *);
static void zone_loaddone(void *arg, isc_result_t result);
static isc_result_t zone_startload(dns_db_t *db, dns_zone_t *zone,
				   isc_time_t loadtime);

#if 0
/* ondestroy example */
static void dns_zonemgr_dbdestroyed(isc_task_t *task, isc_event_t *event);
#endif

static void refresh_callback(isc_task_t *, isc_event_t *);
static void stub_callback(isc_task_t *, isc_event_t *);
static void queue_soa_query(dns_zone_t *zone);
static void soa_query(isc_task_t *, isc_event_t *);
static void ns_query(dns_zone_t *zone, dns_rdataset_t *soardataset,
		     dns_stub_t *stub);
static int message_count(dns_message_t *msg, dns_section_t section,
			 dns_rdatatype_t type);
static void notify_cancel(dns_zone_t *zone);
static void notify_find_address(dns_notify_t *notify);
static void notify_send(dns_notify_t *notify);
static isc_result_t notify_createmessage(dns_zone_t *zone,
					 unsigned int flags,
					 dns_message_t **messagep);
static void notify_done(isc_task_t *task, isc_event_t *event);
static void notify_send_toaddr(isc_task_t *task, isc_event_t *event);
static isc_result_t zone_dump(dns_zone_t *, isc_boolean_t);
static void got_transfer_quota(isc_task_t *task, isc_event_t *event);
static isc_result_t zmgr_start_xfrin_ifquota(dns_zonemgr_t *zmgr,
					     dns_zone_t *zone);
static void zmgr_resume_xfrs(dns_zonemgr_t *zmgr, isc_boolean_t multi);
static void zonemgr_free(dns_zonemgr_t *zmgr);
static isc_result_t zonemgr_getio(dns_zonemgr_t *zmgr, isc_boolean_t high,
				  isc_task_t *task, isc_taskaction_t action,
				  void *arg, dns_io_t **iop);
static void zonemgr_putio(dns_io_t **iop);
static void zonemgr_cancelio(dns_io_t *io);

static isc_result_t
zone_get_from_db(dns_zone_t *zone, dns_db_t *db, unsigned int *nscount,
		 unsigned int *soacount, isc_uint32_t *serial,
		 isc_uint32_t *refresh, isc_uint32_t *retry,
		 isc_uint32_t *expire, isc_uint32_t *minimum,
		 unsigned int *errors);

static void zone_freedbargs(dns_zone_t *zone);
static void forward_callback(isc_task_t *task, isc_event_t *event);
static void zone_saveunique(dns_zone_t *zone, const char *path,
			    const char *templat);
static void zone_maintenance(dns_zone_t *zone);
static void zone_notify(dns_zone_t *zone, isc_time_t *now);
static void dump_done(void *arg, isc_result_t result);

#define ENTER zone_debuglog(zone, me, 1, "enter")

static const unsigned int dbargc_default = 1;
static const char *dbargv_default[] = { "rbt" };

#define DNS_ZONE_JITTER_ADD(a, b, c) \
	do { \
		isc_interval_t _i; \
		isc_uint32_t _j; \
		_j = isc_random_jitter((b), (b)/4); \
		isc_interval_set(&_i, _j, 0); \
		if (isc_time_add((a), &_i, (c)) != ISC_R_SUCCESS) { \
			dns_zone_log(zone, ISC_LOG_WARNING, \
				     "epoch approaching: upgrade required: " \
				     "now + %s failed", #b); \
			isc_interval_set(&_i, _j/2, 0); \
			(void)isc_time_add((a), &_i, (c)); \
		} \
	} while (0)

#define DNS_ZONE_TIME_ADD(a, b, c) \
	do { \
		isc_interval_t _i; \
		isc_interval_set(&_i, (b), 0); \
		if (isc_time_add((a), &_i, (c)) != ISC_R_SUCCESS) { \
			dns_zone_log(zone, ISC_LOG_WARNING, \
				     "epoch approaching: upgrade required: " \
				     "now + %s failed", #b); \
			isc_interval_set(&_i, (b)/2, 0); \
			(void)isc_time_add((a), &_i, (c)); \
		} \
	} while (0)

/***
 ***	Public functions.
 ***/

isc_result_t
dns_zone_create(dns_zone_t **zonep, isc_mem_t *mctx) {
	isc_result_t result;
	dns_zone_t *zone;
	isc_time_t now;

	REQUIRE(zonep != NULL && *zonep == NULL);
	REQUIRE(mctx != NULL);

	TIME_NOW(&now);
	zone = isc_mem_get(mctx, sizeof(*zone));
	if (zone == NULL)
		return (ISC_R_NOMEMORY);

	zone->mctx = NULL;
	isc_mem_attach(mctx, &zone->mctx);

	result = isc_mutex_init(&zone->lock);
	if (result != ISC_R_SUCCESS)
		goto free_zone;

	result = ZONEDB_INITLOCK(&zone->dblock);
	if (result != ISC_R_SUCCESS)
		goto free_mutex;

	/* XXX MPA check that all elements are initialised */
#ifdef DNS_ZONE_CHECKLOCK
	zone->locked = ISC_FALSE;
#endif
	zone->db = NULL;
	zone->zmgr = NULL;
	ISC_LINK_INIT(zone, link);
	result = isc_refcount_init(&zone->erefs, 1);	/* Implicit attach. */
	if (result != ISC_R_SUCCESS)
		goto free_dblock;
	zone->irefs = 0;
	dns_name_init(&zone->origin, NULL);
	zone->masterfile = NULL;
	zone->masterformat =  dns_masterformat_none;
	zone->keydirectory = NULL;
	zone->journalsize = -1;
	zone->journal = NULL;
	zone->rdclass = dns_rdataclass_none;
	zone->type = dns_zone_none;
	zone->flags = 0;
	zone->options = 0;
	zone->db_argc = 0;
	zone->db_argv = NULL;
	isc_time_settoepoch(&zone->expiretime);
	isc_time_settoepoch(&zone->refreshtime);
	isc_time_settoepoch(&zone->dumptime);
	isc_time_settoepoch(&zone->loadtime);
	zone->notifytime = now;
	zone->serial = 0;
	zone->refresh = DNS_ZONE_DEFAULTREFRESH;
	zone->retry = DNS_ZONE_DEFAULTRETRY;
	zone->expire = 0;
	zone->minimum = 0;
	zone->maxrefresh = DNS_ZONE_MAXREFRESH;
	zone->minrefresh = DNS_ZONE_MINREFRESH;
	zone->maxretry = DNS_ZONE_MAXRETRY;
	zone->minretry = DNS_ZONE_MINRETRY;
	zone->masters = NULL;
	zone->masterkeynames = NULL;
	zone->mastersok = NULL;
	zone->masterscnt = 0;
	zone->curmaster = 0;
	zone->notify = NULL;
	zone->notifytype = dns_notifytype_yes;
	zone->notifycnt = 0;
	zone->task = NULL;
	zone->update_acl = NULL;
	zone->forward_acl = NULL;
	zone->notify_acl = NULL;
	zone->query_acl = NULL;
	zone->xfr_acl = NULL;
	zone->update_disabled = ISC_FALSE;
	zone->zero_no_soa_ttl = ISC_TRUE;
	zone->check_names = dns_severity_ignore;
	zone->request = NULL;
	zone->lctx = NULL;
	zone->readio = NULL;
	zone->dctx = NULL;
	zone->writeio = NULL;
	zone->timer = NULL;
	zone->idlein = DNS_DEFAULT_IDLEIN;
	zone->idleout = DNS_DEFAULT_IDLEOUT;
	ISC_LIST_INIT(zone->notifies);
	isc_sockaddr_any(&zone->notifysrc4);
	isc_sockaddr_any6(&zone->notifysrc6);
	isc_sockaddr_any(&zone->xfrsource4);
	isc_sockaddr_any6(&zone->xfrsource6);
	isc_sockaddr_any(&zone->altxfrsource4);
	isc_sockaddr_any6(&zone->altxfrsource6);
	zone->xfr = NULL;
	zone->tsigkey = NULL;
	zone->maxxfrin = MAX_XFER_TIME;
	zone->maxxfrout = MAX_XFER_TIME;
	zone->ssutable = NULL;
	zone->sigvalidityinterval = 30 * 24 * 3600;
	zone->view = NULL;
	zone->acache = NULL;
	zone->checkmx = NULL;
	zone->checksrv = NULL;
	zone->checkns = NULL;
	ISC_LINK_INIT(zone, statelink);
	zone->statelist = NULL;
	zone->counters = NULL;
	zone->notifydelay = 5;
	zone->isself = NULL;
	zone->isselfarg = NULL;

	zone->magic = ZONE_MAGIC;

	/* Must be after magic is set. */
	result = dns_zone_setdbtype(zone, dbargc_default, dbargv_default);
	if (result != ISC_R_SUCCESS)
		goto free_erefs;

	ISC_EVENT_INIT(&zone->ctlevent, sizeof(zone->ctlevent), 0, NULL,
		       DNS_EVENT_ZONECONTROL, zone_shutdown, zone, zone,
		       NULL, NULL);
	*zonep = zone;
	return (ISC_R_SUCCESS);

 free_erefs:
	isc_refcount_decrement(&zone->erefs, NULL);
	isc_refcount_destroy(&zone->erefs);

 free_dblock:
	ZONEDB_DESTROYLOCK(&zone->dblock);

 free_mutex:
	DESTROYLOCK(&zone->lock);

 free_zone:
	isc_mem_putanddetach(&zone->mctx, zone, sizeof(*zone));
	return (result);
}

/*
 * Free a zone.  Because we require that there be no more
 * outstanding events or references, no locking is necessary.
 */
static void
zone_free(dns_zone_t *zone) {
	isc_mem_t *mctx = NULL;

	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(isc_refcount_current(&zone->erefs) == 0);
	REQUIRE(zone->irefs == 0);
	REQUIRE(!LOCKED_ZONE(zone));
	REQUIRE(zone->timer == NULL);

	/*
	 * Managed objects.  Order is important.
	 */
	if (zone->request != NULL)
		dns_request_destroy(&zone->request); /* XXXMPA */
	INSIST(zone->readio == NULL);
	INSIST(zone->statelist == NULL);
	INSIST(zone->writeio == NULL);

	if (zone->task != NULL)
		isc_task_detach(&zone->task);
	if (zone->zmgr)
		dns_zonemgr_releasezone(zone->zmgr, zone);

	/* Unmanaged objects */
	if (zone->masterfile != NULL)
		isc_mem_free(zone->mctx, zone->masterfile);
	zone->masterfile = NULL;
	if (zone->keydirectory != NULL)
		isc_mem_free(zone->mctx, zone->keydirectory);
	zone->keydirectory = NULL;
	zone->journalsize = -1;
	if (zone->journal != NULL)
		isc_mem_free(zone->mctx, zone->journal);
	zone->journal = NULL;
	if (zone->counters != NULL)
		dns_stats_freecounters(zone->mctx, &zone->counters);
	if (zone->db != NULL)
		zone_detachdb(zone);
	if (zone->acache != NULL)
		dns_acache_detach(&zone->acache);
	zone_freedbargs(zone);
	RUNTIME_CHECK(dns_zone_setmasterswithkeys(zone, NULL, NULL, 0)
		      == ISC_R_SUCCESS);
	RUNTIME_CHECK(dns_zone_setalsonotify(zone, NULL, 0)
		      == ISC_R_SUCCESS);
	zone->check_names = dns_severity_ignore;
	if (zone->update_acl != NULL)
		dns_acl_detach(&zone->update_acl);
	if (zone->forward_acl != NULL)
		dns_acl_detach(&zone->forward_acl);
	if (zone->notify_acl != NULL)
		dns_acl_detach(&zone->notify_acl);
	if (zone->query_acl != NULL)
		dns_acl_detach(&zone->query_acl);
	if (zone->xfr_acl != NULL)
		dns_acl_detach(&zone->xfr_acl);
	if (dns_name_dynamic(&zone->origin))
		dns_name_free(&zone->origin, zone->mctx);
	if (zone->ssutable != NULL)
		dns_ssutable_detach(&zone->ssutable);

	/* last stuff */
	ZONEDB_DESTROYLOCK(&zone->dblock);
	DESTROYLOCK(&zone->lock);
	isc_refcount_destroy(&zone->erefs);
	zone->magic = 0;
	mctx = zone->mctx;
	isc_mem_put(mctx, zone, sizeof(*zone));
	isc_mem_detach(&mctx);
}

/*
 *	Single shot.
 */
void
dns_zone_setclass(dns_zone_t *zone, dns_rdataclass_t rdclass) {

	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(rdclass != dns_rdataclass_none);

	/*
	 * Test and set.
	 */
	LOCK_ZONE(zone);
	REQUIRE(zone->rdclass == dns_rdataclass_none ||
		zone->rdclass == rdclass);
	zone->rdclass = rdclass;
	UNLOCK_ZONE(zone);
}

dns_rdataclass_t
dns_zone_getclass(dns_zone_t *zone){
	REQUIRE(DNS_ZONE_VALID(zone));

	return (zone->rdclass);
}

void
dns_zone_setnotifytype(dns_zone_t *zone, dns_notifytype_t notifytype) {
	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	zone->notifytype = notifytype;
	UNLOCK_ZONE(zone);
}

/*
 *	Single shot.
 */
void
dns_zone_settype(dns_zone_t *zone, dns_zonetype_t type) {

	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(type != dns_zone_none);

	/*
	 * Test and set.
	 */
	LOCK_ZONE(zone);
	REQUIRE(zone->type == dns_zone_none || zone->type == type);
	zone->type = type;
	UNLOCK_ZONE(zone);
}

static void
zone_freedbargs(dns_zone_t *zone) {
	unsigned int i;

	/* Free the old database argument list. */
	if (zone->db_argv != NULL) {
		for (i = 0; i < zone->db_argc; i++)
			isc_mem_free(zone->mctx, zone->db_argv[i]);
		isc_mem_put(zone->mctx, zone->db_argv,
			    zone->db_argc * sizeof(*zone->db_argv));
	}
	zone->db_argc = 0;
	zone->db_argv = NULL;
}

isc_result_t
dns_zone_getdbtype(dns_zone_t *zone, char ***argv, isc_mem_t *mctx) {
	size_t size = 0;
	unsigned int i;
	isc_result_t result = ISC_R_SUCCESS;
	void *mem;
	char **tmp, *tmp2;

	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(argv != NULL && *argv == NULL);

	LOCK_ZONE(zone);
	size = (zone->db_argc + 1) * sizeof(char *);
	for (i = 0; i < zone->db_argc; i++)
		size += strlen(zone->db_argv[i]) + 1;
	mem = isc_mem_allocate(mctx, size);
	if (mem != NULL) {
		tmp = mem;
		tmp2 = mem;
		tmp2 += (zone->db_argc + 1) * sizeof(char *);
		for (i = 0; i < zone->db_argc; i++) {
			*tmp++ = tmp2;
			strcpy(tmp2, zone->db_argv[i]);
			tmp2 += strlen(tmp2) + 1;
		}
		*tmp = NULL;
	} else
		result = ISC_R_NOMEMORY;
	UNLOCK_ZONE(zone);
	*argv = mem;
	return (result);
}

isc_result_t
dns_zone_setdbtype(dns_zone_t *zone,
		   unsigned int dbargc, const char * const *dbargv) {
	isc_result_t result = ISC_R_SUCCESS;
	char **new = NULL;
	unsigned int i;

	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(dbargc >= 1);
	REQUIRE(dbargv != NULL);

	LOCK_ZONE(zone);

	/* Set up a new database argument list. */
	new = isc_mem_get(zone->mctx, dbargc * sizeof(*new));
	if (new == NULL)
		goto nomem;
	for (i = 0; i < dbargc; i++)
		new[i] = NULL;
	for (i = 0; i < dbargc; i++) {
		new[i] = isc_mem_strdup(zone->mctx, dbargv[i]);
		if (new[i] == NULL)
			goto nomem;
	}

	/* Free the old list. */
	zone_freedbargs(zone);

	zone->db_argc = dbargc;
	zone->db_argv = new;
	result = ISC_R_SUCCESS;
	goto unlock;

 nomem:
	if (new != NULL) {
		for (i = 0; i < dbargc; i++)
			if (new[i] != NULL)
				isc_mem_free(zone->mctx, new[i]);
		isc_mem_put(zone->mctx, new, dbargc * sizeof(*new));
	}
	result = ISC_R_NOMEMORY;

 unlock:
	UNLOCK_ZONE(zone);
	return (result);
}

void
dns_zone_setview(dns_zone_t *zone, dns_view_t *view) {
	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	if (zone->view != NULL)
		dns_view_weakdetach(&zone->view);
	dns_view_weakattach(view, &zone->view);
	UNLOCK_ZONE(zone);
}


dns_view_t *
dns_zone_getview(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));

	return (zone->view);
}


isc_result_t
dns_zone_setorigin(dns_zone_t *zone, const dns_name_t *origin) {
	isc_result_t result;

	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(origin != NULL);

	LOCK_ZONE(zone);
	if (dns_name_dynamic(&zone->origin)) {
		dns_name_free(&zone->origin, zone->mctx);
		dns_name_init(&zone->origin, NULL);
	}
	result = dns_name_dup(origin, zone->mctx, &zone->origin);
	UNLOCK_ZONE(zone);
	return (result);
}

void
dns_zone_setacache(dns_zone_t *zone, dns_acache_t *acache) {
	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(acache != NULL);

	LOCK_ZONE(zone);
	if (zone->acache != NULL)
		dns_acache_detach(&zone->acache);
	dns_acache_attach(acache, &zone->acache);
	ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_read);
	if (zone->db != NULL) {
		isc_result_t result;

		/*
		 * If the zone reuses an existing DB, the DB needs to be
		 * set in the acache explicitly.  We can safely ignore the
		 * case where the DB is already set.  If other error happens,
		 * the acache will not work effectively.
		 */
		result = dns_acache_setdb(acache, zone->db);
		if (result != ISC_R_SUCCESS && result != ISC_R_EXISTS) {
			UNEXPECTED_ERROR(__FILE__, __LINE__,
					 "dns_acache_setdb() failed: %s",
					 isc_result_totext(result));
		}
	}
	ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);
	UNLOCK_ZONE(zone);
}

static isc_result_t
dns_zone_setstring(dns_zone_t *zone, char **field, const char *value) {
	char *copy;

	if (value != NULL) {
		copy = isc_mem_strdup(zone->mctx, value);
		if (copy == NULL)
			return (ISC_R_NOMEMORY);
	} else {
		copy = NULL;
	}

	if (*field != NULL)
		isc_mem_free(zone->mctx, *field);

	*field = copy;
	return (ISC_R_SUCCESS);
}

isc_result_t
dns_zone_setfile(dns_zone_t *zone, const char *file) {
	return (dns_zone_setfile2(zone, file, dns_masterformat_text));
}

isc_result_t
dns_zone_setfile2(dns_zone_t *zone, const char *file,
		  dns_masterformat_t format) {
	isc_result_t result = ISC_R_SUCCESS;

	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	result = dns_zone_setstring(zone, &zone->masterfile, file);
	if (result == ISC_R_SUCCESS) {
		zone->masterformat = format;
		result = default_journal(zone);
	}
	UNLOCK_ZONE(zone);

	return (result);
}

const char *
dns_zone_getfile(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));

	return (zone->masterfile);
}

static isc_result_t
default_journal(dns_zone_t *zone) {
	isc_result_t result;
	char *journal;

	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(LOCKED_ZONE(zone));

	if (zone->masterfile != NULL) {
		/* Calculate string length including '\0'. */
		int len = strlen(zone->masterfile) + sizeof(".jnl");
		journal = isc_mem_allocate(zone->mctx, len);
		if (journal == NULL)
			return (ISC_R_NOMEMORY);
		strcpy(journal, zone->masterfile);
		strcat(journal, ".jnl");
	} else {
		journal = NULL;
	}
	result = dns_zone_setstring(zone, &zone->journal, journal);
	if (journal != NULL)
		isc_mem_free(zone->mctx, journal);
	return (result);
}

isc_result_t
dns_zone_setjournal(dns_zone_t *zone, const char *journal) {
	isc_result_t result = ISC_R_SUCCESS;

	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	result = dns_zone_setstring(zone, &zone->journal, journal);
	UNLOCK_ZONE(zone);

	return (result);
}

char *
dns_zone_getjournal(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));

	return (zone->journal);
}

/*
 * Return true iff the zone is "dynamic", in the sense that the zone's
 * master file (if any) is written by the server, rather than being
 * updated manually and read by the server.
 *
 * This is true for slave zones, stub zones, and zones that allow
 * dynamic updates either by having an update policy ("ssutable")
 * or an "allow-update" ACL with a value other than exactly "{ none; }".
 */
static isc_boolean_t
zone_isdynamic(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));

	return (ISC_TF(zone->type == dns_zone_slave ||
		       zone->type == dns_zone_stub ||
		       (!zone->update_disabled && zone->ssutable != NULL) ||
		       (!zone->update_disabled && zone->update_acl != NULL &&
			! (zone->update_acl->length == 1 &&
			   zone->update_acl->elements[0].negative == ISC_TRUE
			   &&
			   zone->update_acl->elements[0].type ==
			   dns_aclelementtype_any))));
}


static isc_result_t
zone_load(dns_zone_t *zone, unsigned int flags) {
	isc_result_t result;
	isc_time_t now;
	isc_time_t loadtime, filetime;
	dns_db_t *db = NULL;

	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	TIME_NOW(&now);

	INSIST(zone->type != dns_zone_none);

	if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_LOADING)) {
		if ((flags & DNS_ZONELOADFLAG_THAW) != 0)
			DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_THAW);
		result = DNS_R_CONTINUE;
		goto cleanup;
	}

	if (zone->db != NULL && zone->masterfile == NULL) {
		/*
		 * The zone has no master file configured, but it already
		 * has a database.  It could be the built-in
		 * version.bind. CH zone, a zone with a persistent
		 * database being reloaded, or maybe a zone that
		 * used to have a master file but whose configuration
		 * was changed so that it no longer has one.  Do nothing.
		 */
		result = ISC_R_SUCCESS;
		goto cleanup;
	}

	if (zone->db != NULL && zone_isdynamic(zone)) {
		/*
		 * This is a slave, stub, or dynamically updated
		 * zone being reloaded.  Do nothing - the database
		 * we already have is guaranteed to be up-to-date.
		 */
		if (zone->type == dns_zone_master)
			result = DNS_R_DYNAMIC;
		else
			result = ISC_R_SUCCESS;
		goto cleanup;
	}


	/*
	 * Store the current time before the zone is loaded, so that if the
	 * file changes between the time of the load and the time that
	 * zone->loadtime is set, then the file will still be reloaded
	 * the next time dns_zone_load is called.
	 */
	TIME_NOW(&loadtime);

	/*
	 * Don't do the load if the file that stores the zone is older
	 * than the last time the zone was loaded.  If the zone has not
	 * been loaded yet, zone->loadtime will be the epoch.
	 */
	if (zone->masterfile != NULL) {
		/*
		 * The file is already loaded.  If we are just doing a
		 * "rndc reconfig", we are done.
		 */
		if (!isc_time_isepoch(&zone->loadtime) &&
		    (flags & DNS_ZONELOADFLAG_NOSTAT) != 0) {
			result = ISC_R_SUCCESS;
			goto cleanup;
		}

		result = isc_file_getmodtime(zone->masterfile, &filetime);
		if (result == ISC_R_SUCCESS) {
			if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_LOADED) &&
			    !DNS_ZONE_FLAG(zone, DNS_ZONEFLG_HASINCLUDE) &&
			    isc_time_compare(&filetime, &zone->loadtime) <= 0) {
				dns_zone_log(zone, ISC_LOG_DEBUG(1),
					     "skipping load: master file "
					     "older than last load");
				result = DNS_R_UPTODATE;
				goto cleanup;
			}
			loadtime = filetime;
		}
	}

	INSIST(zone->db_argc >= 1);

	/*
	 * Built in zones don't need to be reloaded.
	 */
	if (zone->type == dns_zone_master &&
	    strcmp(zone->db_argv[0], "_builtin") == 0 &&
	    DNS_ZONE_FLAG(zone, DNS_ZONEFLG_LOADED)) {
		result = ISC_R_SUCCESS;
		goto cleanup;
	}

	if ((zone->type == dns_zone_slave || zone->type == dns_zone_stub) &&
	    (strcmp(zone->db_argv[0], "rbt") == 0 ||
	     strcmp(zone->db_argv[0], "rbt64") == 0)) {
		if (zone->masterfile == NULL ||
		    !isc_file_exists(zone->masterfile)) {
			if (zone->masterfile != NULL) {
				dns_zone_log(zone, ISC_LOG_DEBUG(1),
					     "no master file");
			}
			zone->refreshtime = now;
			if (zone->task != NULL)
				zone_settimer(zone, &now);
			result = ISC_R_SUCCESS;
			goto cleanup;
		}
	}

	dns_zone_log(zone, ISC_LOG_DEBUG(1), "starting load");

	result = dns_db_create(zone->mctx, zone->db_argv[0],
			       &zone->origin, (zone->type == dns_zone_stub) ?
			       dns_dbtype_stub : dns_dbtype_zone,
			       zone->rdclass,
			       zone->db_argc - 1, zone->db_argv + 1,
			       &db);

	if (result != ISC_R_SUCCESS) {
		dns_zone_log(zone, ISC_LOG_ERROR,
			     "loading zone: creating database: %s",
			     isc_result_totext(result));
		goto cleanup;
	}
	dns_db_settask(db, zone->task);

	if (! dns_db_ispersistent(db)) {
		if (zone->masterfile != NULL) {
			result = zone_startload(db, zone, loadtime);
		} else {
			result = DNS_R_NOMASTERFILE;
			if (zone->type == dns_zone_master) {
				dns_zone_log(zone, ISC_LOG_ERROR,
					     "loading zone: "
					     "no master file configured");
				goto cleanup;
			}
			dns_zone_log(zone, ISC_LOG_INFO, "loading zone: "
				     "no master file configured: continuing");
		}
	}

	if (result == DNS_R_CONTINUE) {
		DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_LOADING);
		if ((flags & DNS_ZONELOADFLAG_THAW) != 0)
			DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_THAW);
		goto cleanup;
	}

	result = zone_postload(zone, db, loadtime, result);

 cleanup:
	UNLOCK_ZONE(zone);
	if (db != NULL)
		dns_db_detach(&db);
	return (result);
}

isc_result_t
dns_zone_load(dns_zone_t *zone) {
	return (zone_load(zone, 0));
}

isc_result_t
dns_zone_loadnew(dns_zone_t *zone) {
	return (zone_load(zone, DNS_ZONELOADFLAG_NOSTAT));
}

isc_result_t
dns_zone_loadandthaw(dns_zone_t *zone) {
	isc_result_t result;

	result = zone_load(zone, DNS_ZONELOADFLAG_THAW);
	switch (result) {
	case DNS_R_CONTINUE:
		/* Deferred thaw. */
		break;
	case ISC_R_SUCCESS:
	case DNS_R_UPTODATE:
	case DNS_R_SEENINCLUDE:
		zone->update_disabled = ISC_FALSE;
		break;
	case DNS_R_NOMASTERFILE:
		zone->update_disabled = ISC_FALSE;
		break;
	default:
		/* Error, remain in disabled state. */
		break;
	}
	return (result);
}

static void
zone_gotreadhandle(isc_task_t *task, isc_event_t *event) {
	dns_load_t *load = event->ev_arg;
	isc_result_t result = ISC_R_SUCCESS;
	unsigned int options;

	REQUIRE(DNS_LOAD_VALID(load));

	if ((event->ev_attributes & ISC_EVENTATTR_CANCELED) != 0)
		result = ISC_R_CANCELED;
	isc_event_free(&event);
	if (result == ISC_R_CANCELED)
		goto fail;

	options = DNS_MASTER_ZONE;
	if (load->zone->type == dns_zone_slave)
		options |= DNS_MASTER_SLAVE;
	if (DNS_ZONE_OPTION(load->zone, DNS_ZONEOPT_CHECKNS))
		options |= DNS_MASTER_CHECKNS;
	if (DNS_ZONE_OPTION(load->zone, DNS_ZONEOPT_FATALNS))
		options |= DNS_MASTER_FATALNS;
	if (DNS_ZONE_OPTION(load->zone, DNS_ZONEOPT_CHECKNAMES))
		options |= DNS_MASTER_CHECKNAMES;
	if (DNS_ZONE_OPTION(load->zone, DNS_ZONEOPT_CHECKNAMESFAIL))
		options |= DNS_MASTER_CHECKNAMESFAIL;
	if (DNS_ZONE_OPTION(load->zone, DNS_ZONEOPT_CHECKMX))
		options |= DNS_MASTER_CHECKMX;
	if (DNS_ZONE_OPTION(load->zone, DNS_ZONEOPT_CHECKMXFAIL))
		options |= DNS_MASTER_CHECKMXFAIL;
	if (DNS_ZONE_OPTION(load->zone, DNS_ZONEOPT_CHECKWILDCARD))
		options |= DNS_MASTER_CHECKWILDCARD;
	result = dns_master_loadfileinc2(load->zone->masterfile,
					 dns_db_origin(load->db),
					 dns_db_origin(load->db),
					 load->zone->rdclass,
					 options,
					 &load->callbacks, task,
					 zone_loaddone, load,
					 &load->zone->lctx, load->zone->mctx,
					 load->zone->masterformat);
	if (result != ISC_R_SUCCESS && result != DNS_R_CONTINUE &&
	    result != DNS_R_SEENINCLUDE)
		goto fail;
	return;

 fail:
	zone_loaddone(load, result);
}

static void
zone_gotwritehandle(isc_task_t *task, isc_event_t *event) {
	const char me[] = "zone_gotwritehandle";
	dns_zone_t *zone = event->ev_arg;
	isc_result_t result = ISC_R_SUCCESS;
	dns_dbversion_t *version = NULL;

	REQUIRE(DNS_ZONE_VALID(zone));
	INSIST(task == zone->task);
	ENTER;

	if ((event->ev_attributes & ISC_EVENTATTR_CANCELED) != 0)
		result = ISC_R_CANCELED;
	isc_event_free(&event);
	if (result == ISC_R_CANCELED)
		goto fail;

	LOCK_ZONE(zone);
	ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_read);
	dns_db_currentversion(zone->db, &version);
	result = dns_master_dumpinc2(zone->mctx, zone->db, version,
				     &dns_master_style_default,
				     zone->masterfile, zone->task, dump_done,
				     zone, &zone->dctx, zone->masterformat);
	dns_db_closeversion(zone->db, &version, ISC_FALSE);
	ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);
	UNLOCK_ZONE(zone);
	if (result != DNS_R_CONTINUE)
		goto fail;
	return;

 fail:
	dump_done(zone, result);
}

static isc_result_t
zone_startload(dns_db_t *db, dns_zone_t *zone, isc_time_t loadtime) {
	dns_load_t *load;
	isc_result_t result;
	isc_result_t tresult;
	unsigned int options;

	options = DNS_MASTER_ZONE;
	if (DNS_ZONE_OPTION(zone, DNS_ZONEOPT_MANYERRORS))
		options |= DNS_MASTER_MANYERRORS;
	if (DNS_ZONE_OPTION(zone, DNS_ZONEOPT_CHECKNS))
		options |= DNS_MASTER_CHECKNS;
	if (DNS_ZONE_OPTION(zone, DNS_ZONEOPT_FATALNS))
		options |= DNS_MASTER_FATALNS;
	if (zone->type == dns_zone_slave)
		options |= DNS_MASTER_SLAVE;
	if (DNS_ZONE_OPTION(zone, DNS_ZONEOPT_CHECKNAMES))
		options |= DNS_MASTER_CHECKNAMES;
	if (DNS_ZONE_OPTION(zone, DNS_ZONEOPT_CHECKNAMESFAIL))
		options |= DNS_MASTER_CHECKNAMESFAIL;
	if (DNS_ZONE_OPTION(zone, DNS_ZONEOPT_CHECKMX))
		options |= DNS_MASTER_CHECKMX;
	if (DNS_ZONE_OPTION(zone, DNS_ZONEOPT_CHECKMXFAIL))
		options |= DNS_MASTER_CHECKMXFAIL;
	if (DNS_ZONE_OPTION(zone, DNS_ZONEOPT_CHECKWILDCARD))
		options |= DNS_MASTER_CHECKWILDCARD;

	if (zone->zmgr != NULL && zone->db != NULL && zone->task != NULL) {
		load = isc_mem_get(zone->mctx, sizeof(*load));
		if (load == NULL)
			return (ISC_R_NOMEMORY);

		load->mctx = NULL;
		load->zone = NULL;
		load->db = NULL;
		load->loadtime = loadtime;
		load->magic = LOAD_MAGIC;

		isc_mem_attach(zone->mctx, &load->mctx);
		zone_iattach(zone, &load->zone);
		dns_db_attach(db, &load->db);
		dns_rdatacallbacks_init(&load->callbacks);
		result = dns_db_beginload(db, &load->callbacks.add,
					  &load->callbacks.add_private);
		if (result != ISC_R_SUCCESS)
			goto cleanup;
		result = zonemgr_getio(zone->zmgr, ISC_TRUE, zone->task,
				       zone_gotreadhandle, load,
				       &zone->readio);
		if (result != ISC_R_SUCCESS) {
			/*
			 * We can't report multiple errors so ignore
			 * the result of dns_db_endload().
			 */
			(void)dns_db_endload(load->db,
					     &load->callbacks.add_private);
			goto cleanup;
		} else
			result = DNS_R_CONTINUE;
	} else {
		dns_rdatacallbacks_t callbacks;

		dns_rdatacallbacks_init(&callbacks);
		result = dns_db_beginload(db, &callbacks.add,
					  &callbacks.add_private);
		if (result != ISC_R_SUCCESS)
			return (result);
		result = dns_master_loadfile2(zone->masterfile, &zone->origin,
					      &zone->origin, zone->rdclass,
					      options, &callbacks, zone->mctx,
					      zone->masterformat);
		tresult = dns_db_endload(db, &callbacks.add_private);
		if (result == ISC_R_SUCCESS)
			result = tresult;
	}

	return (result);

 cleanup:
	load->magic = 0;
	dns_db_detach(&load->db);
	zone_idetach(&load->zone);
	isc_mem_detach(&load->mctx);
	isc_mem_put(zone->mctx, load, sizeof(*load));
	return (result);
}

static isc_boolean_t
zone_check_mx(dns_zone_t *zone, dns_db_t *db, dns_name_t *name,
	      dns_name_t *owner)
{
	isc_result_t result;
	char ownerbuf[DNS_NAME_FORMATSIZE];
	char namebuf[DNS_NAME_FORMATSIZE];
	char altbuf[DNS_NAME_FORMATSIZE];
	dns_fixedname_t fixed;
	dns_name_t *foundname;
	int level;

	/*
	 * Outside of zone.
	 */
	if (!dns_name_issubdomain(name, &zone->origin)) {
		if (zone->checkmx != NULL)
			return ((zone->checkmx)(zone, name, owner));
		return (ISC_TRUE);
	}

	if (zone->type == dns_zone_master)
		level = ISC_LOG_ERROR;
	else
		level = ISC_LOG_WARNING;

	dns_fixedname_init(&fixed);
	foundname = dns_fixedname_name(&fixed);

	result = dns_db_find(db, name, NULL, dns_rdatatype_a,
			     0, 0, NULL, foundname, NULL, NULL);
	if (result == ISC_R_SUCCESS)
		return (ISC_TRUE);

	if (result == DNS_R_NXRRSET) {
		result = dns_db_find(db, name, NULL, dns_rdatatype_aaaa,
				     0, 0, NULL, foundname, NULL, NULL);
		if (result == ISC_R_SUCCESS)
			return (ISC_TRUE);
	}

	dns_name_format(owner, ownerbuf, sizeof ownerbuf);
	dns_name_format(name, namebuf, sizeof namebuf);
	if (result == DNS_R_NXRRSET || result == DNS_R_NXDOMAIN ||
	    result == DNS_R_EMPTYNAME) {
		dns_zone_log(zone, level,
			     "%s/MX '%s' has no address records (A or AAAA)",
			     ownerbuf, namebuf);
		/* XXX950 make fatal for 9.5.0. */
		return (ISC_TRUE);
	}

	if (result == DNS_R_CNAME) {
		if (DNS_ZONE_OPTION(zone, DNS_ZONEOPT_WARNMXCNAME) ||
		    DNS_ZONE_OPTION(zone, DNS_ZONEOPT_IGNOREMXCNAME))
			level = ISC_LOG_WARNING;
		if (!DNS_ZONE_OPTION(zone, DNS_ZONEOPT_IGNOREMXCNAME))
			dns_zone_log(zone, level,
				     "%s/MX '%s' is a CNAME (illegal)",
				     ownerbuf, namebuf);
		return ((level == ISC_LOG_WARNING) ? ISC_TRUE : ISC_FALSE);
	}

	if (result == DNS_R_DNAME) {
		if (DNS_ZONE_OPTION(zone, DNS_ZONEOPT_WARNMXCNAME) ||
		    DNS_ZONE_OPTION(zone, DNS_ZONEOPT_IGNOREMXCNAME))
			level = ISC_LOG_WARNING;
		if (!DNS_ZONE_OPTION(zone, DNS_ZONEOPT_IGNOREMXCNAME)) {
			dns_name_format(foundname, altbuf, sizeof altbuf);
			dns_zone_log(zone, level, "%s/MX '%s' is below a DNAME"
				     " '%s' (illegal)", ownerbuf, namebuf,
				     altbuf);
		}
		return ((level == ISC_LOG_WARNING) ? ISC_TRUE : ISC_FALSE);
	}

	if (zone->checkmx != NULL && result == DNS_R_DELEGATION)
		return ((zone->checkmx)(zone, name, owner));

	return (ISC_TRUE);
}

static isc_boolean_t
zone_check_srv(dns_zone_t *zone, dns_db_t *db, dns_name_t *name,
	       dns_name_t *owner)
{
	isc_result_t result;
	char ownerbuf[DNS_NAME_FORMATSIZE];
	char namebuf[DNS_NAME_FORMATSIZE];
	char altbuf[DNS_NAME_FORMATSIZE];
	dns_fixedname_t fixed;
	dns_name_t *foundname;
	int level;

	/*
	 * "." means the services does not exist.
	 */
	if (dns_name_equal(name, dns_rootname))
		return (ISC_TRUE);

	/*
	 * Outside of zone.
	 */
	if (!dns_name_issubdomain(name, &zone->origin)) {
		if (zone->checksrv != NULL)
			return ((zone->checksrv)(zone, name, owner));
		return (ISC_TRUE);
	}

	if (zone->type == dns_zone_master)
		level = ISC_LOG_ERROR;
	else
		level = ISC_LOG_WARNING;

	dns_fixedname_init(&fixed);
	foundname = dns_fixedname_name(&fixed);

	result = dns_db_find(db, name, NULL, dns_rdatatype_a,
			     0, 0, NULL, foundname, NULL, NULL);
	if (result == ISC_R_SUCCESS)
		return (ISC_TRUE);

	if (result == DNS_R_NXRRSET) {
		result = dns_db_find(db, name, NULL, dns_rdatatype_aaaa,
				     0, 0, NULL, foundname, NULL, NULL);
		if (result == ISC_R_SUCCESS)
			return (ISC_TRUE);
	}

	dns_name_format(owner, ownerbuf, sizeof ownerbuf);
	dns_name_format(name, namebuf, sizeof namebuf);
	if (result == DNS_R_NXRRSET || result == DNS_R_NXDOMAIN ||
	    result == DNS_R_EMPTYNAME) {
		dns_zone_log(zone, level,
			     "%s/SRV '%s' has no address records (A or AAAA)",
			     ownerbuf, namebuf);
		/* XXX950 make fatal for 9.5.0. */
		return (ISC_TRUE);
	}

	if (result == DNS_R_CNAME) {
		if (DNS_ZONE_OPTION(zone, DNS_ZONEOPT_WARNSRVCNAME) ||
		    DNS_ZONE_OPTION(zone, DNS_ZONEOPT_IGNORESRVCNAME))
			level = ISC_LOG_WARNING;
		if (!DNS_ZONE_OPTION(zone, DNS_ZONEOPT_IGNORESRVCNAME))
			dns_zone_log(zone, level,
				     "%s/SRV '%s' is a CNAME (illegal)",
				     ownerbuf, namebuf);
		return ((level == ISC_LOG_WARNING) ? ISC_TRUE : ISC_FALSE);
	}

	if (result == DNS_R_DNAME) {
		if (DNS_ZONE_OPTION(zone, DNS_ZONEOPT_WARNSRVCNAME) ||
		    DNS_ZONE_OPTION(zone, DNS_ZONEOPT_IGNORESRVCNAME))
			level = ISC_LOG_WARNING;
		if (!DNS_ZONE_OPTION(zone, DNS_ZONEOPT_IGNORESRVCNAME)) {
			dns_name_format(foundname, altbuf, sizeof altbuf);
			dns_zone_log(zone, level, "%s/SRV '%s' is below a "
				     "DNAME '%s' (illegal)", ownerbuf, namebuf,
				     altbuf);
		}
		return ((level == ISC_LOG_WARNING) ? ISC_TRUE : ISC_FALSE);
	}

	if (zone->checksrv != NULL && result == DNS_R_DELEGATION)
		return ((zone->checksrv)(zone, name, owner));

	return (ISC_TRUE);
}

static isc_boolean_t
zone_check_glue(dns_zone_t *zone, dns_db_t *db, dns_name_t *name,
		dns_name_t *owner)
{
	isc_boolean_t answer = ISC_TRUE;
	isc_result_t result, tresult;
	char ownerbuf[DNS_NAME_FORMATSIZE];
	char namebuf[DNS_NAME_FORMATSIZE];
	char altbuf[DNS_NAME_FORMATSIZE];
	dns_fixedname_t fixed;
	dns_name_t *foundname;
	dns_rdataset_t a;
	dns_rdataset_t aaaa;
	int level;

	/*
	 * Outside of zone.
	 */
	if (!dns_name_issubdomain(name, &zone->origin)) {
		if (zone->checkns != NULL)
			return ((zone->checkns)(zone, name, owner, NULL, NULL));
		return (ISC_TRUE);
	}

	if (zone->type == dns_zone_master)
		level = ISC_LOG_ERROR;
	else
		level = ISC_LOG_WARNING;

	dns_fixedname_init(&fixed);
	foundname = dns_fixedname_name(&fixed);
	dns_rdataset_init(&a);
	dns_rdataset_init(&aaaa);

	result = dns_db_find(db, name, NULL, dns_rdatatype_a,
			     DNS_DBFIND_GLUEOK, 0, NULL,
			     foundname, &a, NULL);

	if (result == ISC_R_SUCCESS) {
		dns_rdataset_disassociate(&a);
		return (ISC_TRUE);
	} else if (result == DNS_R_DELEGATION)
		dns_rdataset_disassociate(&a);

	if (result == DNS_R_NXRRSET || result == DNS_R_DELEGATION ||
	    result == DNS_R_GLUE) {
		tresult = dns_db_find(db, name, NULL, dns_rdatatype_aaaa,
				     DNS_DBFIND_GLUEOK, 0, NULL,
				     foundname, &aaaa, NULL);
		if (tresult == ISC_R_SUCCESS) {
			dns_rdataset_disassociate(&aaaa);
			return (ISC_TRUE);
		}
		if (tresult == DNS_R_DELEGATION)
			dns_rdataset_disassociate(&aaaa);
		if (result == DNS_R_GLUE || tresult == DNS_R_GLUE) {
			/*
			 * Check glue against child zone.
			 */
			if (zone->checkns != NULL)
				answer = (zone->checkns)(zone, name, owner,
							 &a, &aaaa);
			if (dns_rdataset_isassociated(&a))
				dns_rdataset_disassociate(&a);
			if (dns_rdataset_isassociated(&aaaa))
				dns_rdataset_disassociate(&aaaa);
			return (answer);
		}
	} else
		tresult = result;

	dns_name_format(owner, ownerbuf, sizeof ownerbuf);
	dns_name_format(name, namebuf, sizeof namebuf);
	if (result == DNS_R_NXRRSET || result == DNS_R_NXDOMAIN ||
	    result == DNS_R_EMPTYNAME || result == DNS_R_DELEGATION) {
		const char *what;
		isc_boolean_t required = ISC_FALSE;
		if (dns_name_issubdomain(name, owner)) {
			what = "REQUIRED GLUE ";
			required = ISC_TRUE;
		 } else if (result == DNS_R_DELEGATION)
			what = "SIBLING GLUE ";
		else
			what = "";

		if (result != DNS_R_DELEGATION || required ||
		    DNS_ZONE_OPTION(zone, DNS_ZONEOPT_CHECKSIBLING)) {
			dns_zone_log(zone, level, "%s/NS '%s' has no %s"
				     "address records (A or AAAA)",
				     ownerbuf, namebuf, what);
			/*
			 * Log missing address record.
			 */
			if (result == DNS_R_DELEGATION && zone->checkns != NULL)
				(void)(zone->checkns)(zone, name, owner,
						      &a, &aaaa);
			/* XXX950 make fatal for 9.5.0. */
			/* answer = ISC_FALSE; */
		}
	} else if (result == DNS_R_CNAME) {
		dns_zone_log(zone, level, "%s/NS '%s' is a CNAME (illegal)",
			     ownerbuf, namebuf);
		/* XXX950 make fatal for 9.5.0. */
		/* answer = ISC_FALSE; */
	} else if (result == DNS_R_DNAME) {
		dns_name_format(foundname, altbuf, sizeof altbuf);
		dns_zone_log(zone, level,
			     "%s/NS '%s' is below a DNAME '%s' (illegal)",
			     ownerbuf, namebuf, altbuf);
		/* XXX950 make fatal for 9.5.0. */
		/* answer = ISC_FALSE; */
	}

	if (dns_rdataset_isassociated(&a))
		dns_rdataset_disassociate(&a);
	if (dns_rdataset_isassociated(&aaaa))
		dns_rdataset_disassociate(&aaaa);
	return (answer);
}

static isc_boolean_t
integrity_checks(dns_zone_t *zone, dns_db_t *db) {
	dns_dbiterator_t *dbiterator = NULL;
	dns_dbnode_t *node = NULL;
	dns_rdataset_t rdataset;
	dns_fixedname_t fixed;
	dns_fixedname_t fixedbottom;
	dns_rdata_mx_t mx;
	dns_rdata_ns_t ns;
	dns_rdata_in_srv_t srv;
	dns_rdata_t rdata;
	dns_name_t *name;
	dns_name_t *bottom;
	isc_result_t result;
	isc_boolean_t ok = ISC_TRUE;

	dns_fixedname_init(&fixed);
	name = dns_fixedname_name(&fixed);
	dns_fixedname_init(&fixedbottom);
	bottom = dns_fixedname_name(&fixedbottom);
	dns_rdataset_init(&rdataset);
	dns_rdata_init(&rdata);

	result = dns_db_createiterator(db, ISC_FALSE, &dbiterator);
	if (result != ISC_R_SUCCESS)
		return (ISC_TRUE);

	result = dns_dbiterator_first(dbiterator);
	while (result == ISC_R_SUCCESS) {
		result = dns_dbiterator_current(dbiterator, &node, name);
		if (result != ISC_R_SUCCESS)
			goto cleanup;

		/*
		 * Is this name visible in the zone?
		 */
		if (!dns_name_issubdomain(name, &zone->origin) ||
		    (dns_name_countlabels(bottom) > 0 &&
		     dns_name_issubdomain(name, bottom)))
			goto next;

		/*
		 * Don't check the NS records at the origin.
		 */
		if (dns_name_equal(name, &zone->origin))
			goto checkmx;

		result = dns_db_findrdataset(db, node, NULL, dns_rdatatype_ns,
					     0, 0, &rdataset, NULL);
		if (result != ISC_R_SUCCESS)
			goto checkmx;
		/*
		 * Remember bottom of zone.
		 */
		dns_name_copy(name, bottom, NULL);

		result = dns_rdataset_first(&rdataset);
		while (result == ISC_R_SUCCESS) {
			dns_rdataset_current(&rdataset, &rdata);
			result = dns_rdata_tostruct(&rdata, &ns, NULL);
			RUNTIME_CHECK(result == ISC_R_SUCCESS);
			if (!zone_check_glue(zone, db, &ns.name, name))
				ok = ISC_FALSE;
			dns_rdata_reset(&rdata);
			result = dns_rdataset_next(&rdataset);
		}
		dns_rdataset_disassociate(&rdataset);

 checkmx:
		result = dns_db_findrdataset(db, node, NULL, dns_rdatatype_mx,
					     0, 0, &rdataset, NULL);
		if (result != ISC_R_SUCCESS)
			goto checksrv;
		result = dns_rdataset_first(&rdataset);
		while (result == ISC_R_SUCCESS) {
			dns_rdataset_current(&rdataset, &rdata);
			result = dns_rdata_tostruct(&rdata, &mx, NULL);
			RUNTIME_CHECK(result == ISC_R_SUCCESS);
			if (!zone_check_mx(zone, db, &mx.mx, name))
				ok = ISC_FALSE;
			dns_rdata_reset(&rdata);
			result = dns_rdataset_next(&rdataset);
		}
		dns_rdataset_disassociate(&rdataset);

 checksrv:
		if (zone->rdclass != dns_rdataclass_in)
			goto next;
		result = dns_db_findrdataset(db, node, NULL, dns_rdatatype_srv,
					     0, 0, &rdataset, NULL);
		if (result != ISC_R_SUCCESS)
			goto next;
		result = dns_rdataset_first(&rdataset);
		while (result == ISC_R_SUCCESS) {
			dns_rdataset_current(&rdataset, &rdata);
			result = dns_rdata_tostruct(&rdata, &srv, NULL);
			RUNTIME_CHECK(result == ISC_R_SUCCESS);
			if (!zone_check_srv(zone, db, &srv.target, name))
				ok = ISC_FALSE;
			dns_rdata_reset(&rdata);
			result = dns_rdataset_next(&rdataset);
		}
		dns_rdataset_disassociate(&rdataset);

 next:
		dns_db_detachnode(db, &node);
		result = dns_dbiterator_next(dbiterator);
	}

 cleanup:
	if (node != NULL)
		dns_db_detachnode(db, &node);
	dns_dbiterator_destroy(&dbiterator);

	return (ok);
}

/*
 * OpenSSL verification of RSA keys with exponent 3 is known to be
 * broken prior OpenSSL 0.9.8c/0.9.7k.  Look for such keys and warn
 * if they are in use.
 */
static void
zone_check_dnskeys(dns_zone_t *zone, dns_db_t *db) {
	dns_dbnode_t *node = NULL;
	dns_dbversion_t *version = NULL;
	dns_rdata_dnskey_t dnskey;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdataset_t rdataset;
	isc_result_t result;
	isc_boolean_t logit, foundrsa = ISC_FALSE, foundmd5 = ISC_FALSE;
	const char *algorithm;

	result = dns_db_findnode(db, &zone->origin, ISC_FALSE, &node);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	dns_db_currentversion(db, &version);
	dns_rdataset_init(&rdataset);
	result = dns_db_findrdataset(db, node, version, dns_rdatatype_dnskey,
				     dns_rdatatype_none, 0, &rdataset, NULL);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset))
	{
		dns_rdataset_current(&rdataset, &rdata);
		result = dns_rdata_tostruct(&rdata, &dnskey, NULL);
		INSIST(result == ISC_R_SUCCESS);

		if ((dnskey.algorithm == DST_ALG_RSASHA1 ||
		     dnskey.algorithm == DST_ALG_RSAMD5) &&
		     dnskey.datalen > 1 && dnskey.data[0] == 1 &&
		     dnskey.data[1] == 3)
		{
			if (dnskey.algorithm == DST_ALG_RSASHA1) {
				logit = !foundrsa;
				foundrsa = ISC_TRUE;
				algorithm = "RSASHA1";
			} else {
				logit = !foundmd5;
				foundmd5 = ISC_TRUE;
				algorithm = "RSAMD5";
			}
			if (logit)
				dns_zone_log(zone, ISC_LOG_WARNING,
					     "weak %s (%u) key found "
					     "(exponent=3)", algorithm,
					     dnskey.algorithm);
			if (foundrsa && foundmd5)
				break;
		}
		dns_rdata_reset(&rdata);
	}
	dns_rdataset_disassociate(&rdataset);

 cleanup:
	if (node != NULL)
		dns_db_detachnode(db, &node);
	if (version != NULL)
		dns_db_closeversion(db, &version, ISC_FALSE);

}

static isc_result_t
zone_postload(dns_zone_t *zone, dns_db_t *db, isc_time_t loadtime,
	      isc_result_t result)
{
	unsigned int soacount = 0;
	unsigned int nscount = 0;
	unsigned int errors = 0;
	isc_uint32_t serial, refresh, retry, expire, minimum;
	isc_time_t now;
	isc_boolean_t needdump = ISC_FALSE;
	isc_boolean_t hasinclude = DNS_ZONE_FLAG(zone, DNS_ZONEFLG_HASINCLUDE);

	TIME_NOW(&now);

	/*
	 * Initiate zone transfer?  We may need a error code that
	 * indicates that the "permanent" form does not exist.
	 * XXX better error feedback to log.
	 */
	if (result != ISC_R_SUCCESS && result != DNS_R_SEENINCLUDE) {
		if (zone->type == dns_zone_slave ||
		    zone->type == dns_zone_stub) {
			if (result == ISC_R_FILENOTFOUND)
				dns_zone_log(zone, ISC_LOG_DEBUG(1),
					     "no master file");
			else if (result != DNS_R_NOMASTERFILE)
				dns_zone_log(zone, ISC_LOG_ERROR,
					     "loading from master file %s "
					     "failed: %s",
					     zone->masterfile,
					     dns_result_totext(result));
		} else
			dns_zone_log(zone, ISC_LOG_ERROR,
				     "loading from master file %s failed: %s",
				     zone->masterfile,
				     dns_result_totext(result));
		goto cleanup;
	}

	dns_zone_log(zone, ISC_LOG_DEBUG(2),
		     "number of nodes in database: %u",
		     dns_db_nodecount(db));

	if (result == DNS_R_SEENINCLUDE)
		DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_HASINCLUDE);
	else
		DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_HASINCLUDE);

	/*
	 * Apply update log, if any, on initial load.
	 */
	if (zone->journal != NULL &&
	    ! DNS_ZONE_OPTION(zone, DNS_ZONEOPT_NOMERGE) &&
	    ! DNS_ZONE_FLAG(zone, DNS_ZONEFLG_LOADED))
	{
		result = dns_journal_rollforward(zone->mctx, db,
						 zone->journal);
		if (result != ISC_R_SUCCESS && result != ISC_R_NOTFOUND &&
		    result != DNS_R_UPTODATE && result != DNS_R_NOJOURNAL &&
		    result != ISC_R_RANGE) {
			dns_zone_log(zone, ISC_LOG_ERROR,
				     "journal rollforward failed: %s",
				     dns_result_totext(result));
			goto cleanup;
		}
		if (result == ISC_R_NOTFOUND || result == ISC_R_RANGE) {
			dns_zone_log(zone, ISC_LOG_ERROR,
				     "journal rollforward failed: "
				     "journal out of sync with zone");
			goto cleanup;
		}
		dns_zone_log(zone, ISC_LOG_DEBUG(1),
			     "journal rollforward completed "
			     "successfully: %s",
			     dns_result_totext(result));
		if (result == ISC_R_SUCCESS)
			needdump = ISC_TRUE;
	}

	zone->loadtime = loadtime;

	dns_zone_log(zone, ISC_LOG_DEBUG(1), "loaded");

	/*
	 * Obtain ns, soa and cname counts for top of zone.
	 */
	INSIST(db != NULL);
	result = zone_get_from_db(zone, db, &nscount, &soacount, &serial,
				  &refresh, &retry, &expire, &minimum,
				  &errors);
	if (result != ISC_R_SUCCESS) {
		dns_zone_log(zone, ISC_LOG_ERROR,
			     "could not find NS and/or SOA records");
	}

	/*
	 * Master / Slave / Stub zones require both NS and SOA records at
	 * the top of the zone.
	 */

	switch (zone->type) {
	case dns_zone_master:
	case dns_zone_slave:
	case dns_zone_stub:
		if (soacount != 1) {
			dns_zone_log(zone, ISC_LOG_ERROR,
				     "has %d SOA records", soacount);
			result = DNS_R_BADZONE;
		}
		if (nscount == 0) {
			dns_zone_log(zone, ISC_LOG_ERROR,
				     "has no NS records");
			result = DNS_R_BADZONE;
		}
		if (result != ISC_R_SUCCESS)
			goto cleanup;
		if (zone->type == dns_zone_master && errors != 0) {
			result = DNS_R_BADZONE;
			goto cleanup;
		}
		if (zone->type == dns_zone_master &&
		    DNS_ZONE_OPTION(zone, DNS_ZONEOPT_CHECKINTEGRITY) &&
		    !integrity_checks(zone, db)) {
			result = DNS_R_BADZONE;
			goto cleanup;
		}

		if (zone->db != NULL) {
			/*
			 * This is checked in zone_replacedb() for slave zones
			 * as they don't reload from disk.
			 */
			if (DNS_ZONE_OPTION(zone, DNS_ZONEOPT_IXFRFROMDIFFS) &&
			    !isc_serial_gt(serial, zone->serial)) {
				isc_uint32_t serialmin, serialmax;

				INSIST(zone->type == dns_zone_master);

				serialmin = (zone->serial + 1) & 0xffffffffU;
				serialmax = (zone->serial + 0x7fffffffU) &
					     0xffffffffU;
				dns_zone_log(zone, ISC_LOG_ERROR,
					     "ixfr-from-differences: "
					     "new serial (%u) out of range "
					     "[%u - %u]", serial, serialmin,
					     serialmax);
				result = DNS_R_BADZONE;
				goto cleanup;
			} else if (!isc_serial_ge(serial, zone->serial))
				dns_zone_log(zone, ISC_LOG_ERROR,
					     "zone serial has gone backwards");
			else if (serial == zone->serial && !hasinclude)
				dns_zone_log(zone, ISC_LOG_ERROR,
					     "zone serial unchanged. "
					     "zone may fail to transfer "
					     "to slaves.");
		}
		zone->serial = serial;
		zone->refresh = RANGE(refresh,
				      zone->minrefresh, zone->maxrefresh);
		zone->retry = RANGE(retry,
				    zone->minretry, zone->maxretry);
		zone->expire = RANGE(expire, zone->refresh + zone->retry,
				     DNS_MAX_EXPIRE);
		zone->minimum = minimum;
		DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_HAVETIMERS);

		if (zone->type == dns_zone_slave ||
		    zone->type == dns_zone_stub) {
			isc_time_t t;
			isc_uint32_t delay;

			result = isc_file_getmodtime(zone->journal, &t);
			if (result != ISC_R_SUCCESS)
				result = isc_file_getmodtime(zone->masterfile,
							     &t);
			if (result == ISC_R_SUCCESS)
				DNS_ZONE_TIME_ADD(&t, zone->expire,
						  &zone->expiretime);
			else
				DNS_ZONE_TIME_ADD(&now, zone->retry,
						  &zone->expiretime);

			delay = isc_random_jitter(zone->retry,
						  (zone->retry * 3) / 4);
			DNS_ZONE_TIME_ADD(&now, delay, &zone->refreshtime);
			if (isc_time_compare(&zone->refreshtime,
					     &zone->expiretime) >= 0)
				zone->refreshtime = now;
		}
		break;
	default:
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "unexpected zone type %d", zone->type);
		result = ISC_R_UNEXPECTED;
		goto cleanup;
	}

	/*
	 * Check for weak DNSKEY's.
	 */
	if (zone->type == dns_zone_master)
		zone_check_dnskeys(zone, db);

#if 0
	/* destroy notification example. */
	{
		isc_event_t *e = isc_event_allocate(zone->mctx, NULL,
						    DNS_EVENT_DBDESTROYED,
						    dns_zonemgr_dbdestroyed,
						    zone,
						    sizeof(isc_event_t));
		dns_db_ondestroy(db, zone->task, &e);
	}
#endif

	ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_write);
	if (zone->db != NULL) {
		result = zone_replacedb(zone, db, ISC_FALSE);
		ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_write);
		if (result != ISC_R_SUCCESS)
			goto cleanup;
	} else {
		zone_attachdb(zone, db);
		ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_write);
		DNS_ZONE_SETFLAG(zone,
				 DNS_ZONEFLG_LOADED|DNS_ZONEFLG_NEEDNOTIFY);
	}
	result = ISC_R_SUCCESS;
	if (needdump)
		zone_needdump(zone, DNS_DUMP_DELAY);
	if (zone->task != NULL)
		zone_settimer(zone, &now);

	if (! dns_db_ispersistent(db))
		dns_zone_log(zone, ISC_LOG_INFO, "loaded serial %u%s",
			     zone->serial,
			     dns_db_issecure(db) ? " (signed)" : "");

	return (result);

 cleanup:
	if (zone->type == dns_zone_slave ||
	    zone->type == dns_zone_stub) {
		if (zone->journal != NULL)
			zone_saveunique(zone, zone->journal, "jn-XXXXXXXX");
		if (zone->masterfile != NULL)
			zone_saveunique(zone, zone->masterfile, "db-XXXXXXXX");

		/* Mark the zone for immediate refresh. */
		zone->refreshtime = now;
		if (zone->task != NULL)
			zone_settimer(zone, &now);
		result = ISC_R_SUCCESS;
	}
	return (result);
}

static isc_boolean_t
exit_check(dns_zone_t *zone) {

	REQUIRE(LOCKED_ZONE(zone));

	if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_SHUTDOWN) &&
	    zone->irefs == 0)
	{
		/*
		 * DNS_ZONEFLG_SHUTDOWN can only be set if erefs == 0.
		 */
		INSIST(isc_refcount_current(&zone->erefs) == 0);
		return (ISC_TRUE);
	}
	return (ISC_FALSE);
}

static isc_boolean_t
zone_check_ns(dns_zone_t *zone, dns_db_t *db, dns_name_t *name) {
	isc_result_t result;
	char namebuf[DNS_NAME_FORMATSIZE];
	char altbuf[DNS_NAME_FORMATSIZE];
	dns_fixedname_t fixed;
	dns_name_t *foundname;
	int level;

	if (DNS_ZONE_OPTION(zone, DNS_ZONEOPT_NOCHECKNS))
		return (ISC_TRUE);

	if (zone->type == dns_zone_master)
		level = ISC_LOG_ERROR;
	else
		level = ISC_LOG_WARNING;

	dns_fixedname_init(&fixed);
	foundname = dns_fixedname_name(&fixed);

	result = dns_db_find(db, name, NULL, dns_rdatatype_a,
			     0, 0, NULL, foundname, NULL, NULL);
	if (result == ISC_R_SUCCESS)
		return (ISC_TRUE);

	if (result == DNS_R_NXRRSET) {
		result = dns_db_find(db, name, NULL, dns_rdatatype_aaaa,
				     0, 0, NULL, foundname, NULL, NULL);
		if (result == ISC_R_SUCCESS)
			return (ISC_TRUE);
	}

	dns_name_format(name, namebuf, sizeof namebuf);
	if (result == DNS_R_NXRRSET || result == DNS_R_NXDOMAIN ||
	    result == DNS_R_EMPTYNAME) {
		dns_zone_log(zone, level,
			     "NS '%s' has no address records (A or AAAA)",
			     namebuf);
		/* XXX950 Make fatal ISC_FALSE for 9.5.0. */
		return (ISC_TRUE);
	}

	if (result == DNS_R_CNAME) {
		dns_zone_log(zone, level, "NS '%s' is a CNAME (illegal)",
			     namebuf);
		/* XXX950 Make fatal ISC_FALSE for 9.5.0. */
		return (ISC_TRUE);
	}

	if (result == DNS_R_DNAME) {
		dns_name_format(foundname, altbuf, sizeof altbuf);
		dns_zone_log(zone, level,
			     "NS '%s' is below a DNAME '%s' (illegal)",
			     namebuf, altbuf);
		/* XXX950 Make fatal ISC_FALSE for 9.5.0. */
		return (ISC_TRUE);
	}

	return (ISC_TRUE);
}

static isc_result_t
zone_count_ns_rr(dns_zone_t *zone, dns_db_t *db, dns_dbnode_t *node,
		 dns_dbversion_t *version, unsigned int *nscount,
		 unsigned int *errors)
{
	isc_result_t result;
	unsigned int count = 0;
	unsigned int ecount = 0;
	dns_rdataset_t rdataset;
	dns_rdata_t rdata;
	dns_rdata_ns_t ns;

	dns_rdataset_init(&rdataset);
	result = dns_db_findrdataset(db, node, version, dns_rdatatype_ns,
				     dns_rdatatype_none, 0, &rdataset, NULL);
	if (result == ISC_R_NOTFOUND)
		goto success;
	if (result != ISC_R_SUCCESS)
		goto invalidate_rdataset;

	result = dns_rdataset_first(&rdataset);
	while (result == ISC_R_SUCCESS) {
		if (errors != NULL && zone->rdclass == dns_rdataclass_in &&
		    (zone->type == dns_zone_master ||
		     zone->type == dns_zone_slave)) {
			dns_rdata_init(&rdata);
			dns_rdataset_current(&rdataset, &rdata);
			result = dns_rdata_tostruct(&rdata, &ns, NULL);
			RUNTIME_CHECK(result == ISC_R_SUCCESS);
			if (dns_name_issubdomain(&ns.name, &zone->origin) &&
			    !zone_check_ns(zone, db, &ns.name))
				ecount++;
		}
		count++;
		result = dns_rdataset_next(&rdataset);
	}
	dns_rdataset_disassociate(&rdataset);

 success:
	if (nscount != NULL)
		*nscount = count;
	if (errors != NULL)
		*errors = ecount;

	result = ISC_R_SUCCESS;

 invalidate_rdataset:
	dns_rdataset_invalidate(&rdataset);

	return (result);
}

static isc_result_t
zone_load_soa_rr(dns_db_t *db, dns_dbnode_t *node, dns_dbversion_t *version,
		 unsigned int *soacount,
		 isc_uint32_t *serial, isc_uint32_t *refresh,
		 isc_uint32_t *retry, isc_uint32_t *expire,
		 isc_uint32_t *minimum)
{
	isc_result_t result;
	unsigned int count;
	dns_rdataset_t rdataset;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdata_soa_t soa;

	dns_rdataset_init(&rdataset);
	result = dns_db_findrdataset(db, node, version, dns_rdatatype_soa,
				     dns_rdatatype_none, 0, &rdataset, NULL);
	if (result == ISC_R_NOTFOUND) {
		if (soacount != NULL)
			*soacount = 0;
		if (serial != NULL)
			*serial = 0;
		if (refresh != NULL)
			*refresh = 0;
		if (retry != NULL)
			*retry = 0;
		if (expire != NULL)
			*expire = 0;
		if (minimum != NULL)
			*minimum = 0;
		result = ISC_R_SUCCESS;
		goto invalidate_rdataset;
	}
	if (result != ISC_R_SUCCESS)
		goto invalidate_rdataset;

	count = 0;
	result = dns_rdataset_first(&rdataset);
	while (result == ISC_R_SUCCESS) {
		dns_rdata_init(&rdata);
		dns_rdataset_current(&rdataset, &rdata);
		count++;
		if (count == 1) {
			result = dns_rdata_tostruct(&rdata, &soa, NULL);
			RUNTIME_CHECK(result == ISC_R_SUCCESS);
		}

		result = dns_rdataset_next(&rdataset);
		dns_rdata_reset(&rdata);
	}
	dns_rdataset_disassociate(&rdataset);

	if (soacount != NULL)
		*soacount = count;

	if (count > 0) {
		if (serial != NULL)
			*serial = soa.serial;
		if (refresh != NULL)
			*refresh = soa.refresh;
		if (retry != NULL)
			*retry = soa.retry;
		if (expire != NULL)
			*expire = soa.expire;
		if (minimum != NULL)
			*minimum = soa.minimum;
	}

	result = ISC_R_SUCCESS;

 invalidate_rdataset:
	dns_rdataset_invalidate(&rdataset);

	return (result);
}

/*
 * zone must be locked.
 */
static isc_result_t
zone_get_from_db(dns_zone_t *zone, dns_db_t *db, unsigned int *nscount,
		 unsigned int *soacount, isc_uint32_t *serial,
		 isc_uint32_t *refresh, isc_uint32_t *retry,
		 isc_uint32_t *expire, isc_uint32_t *minimum,
		 unsigned int *errors)
{
	dns_dbversion_t *version;
	isc_result_t result;
	isc_result_t answer = ISC_R_SUCCESS;
	dns_dbnode_t *node;

	REQUIRE(db != NULL);
	REQUIRE(zone != NULL);

	version = NULL;
	dns_db_currentversion(db, &version);

	node = NULL;
	result = dns_db_findnode(db, &zone->origin, ISC_FALSE, &node);
	if (result != ISC_R_SUCCESS) {
		answer = result;
		goto closeversion;
	}

	if (nscount != NULL || errors != NULL) {
		result = zone_count_ns_rr(zone, db, node, version,
					  nscount, errors);
		if (result != ISC_R_SUCCESS)
			answer = result;
	}

	if (soacount != NULL || serial != NULL || refresh != NULL
	    || retry != NULL || expire != NULL || minimum != NULL) {
		result = zone_load_soa_rr(db, node, version, soacount,
					  serial, refresh, retry, expire,
					  minimum);
		if (result != ISC_R_SUCCESS)
			answer = result;
	}

	dns_db_detachnode(db, &node);
 closeversion:
	dns_db_closeversion(db, &version, ISC_FALSE);

	return (answer);
}

void
dns_zone_attach(dns_zone_t *source, dns_zone_t **target) {
	REQUIRE(DNS_ZONE_VALID(source));
	REQUIRE(target != NULL && *target == NULL);
	isc_refcount_increment(&source->erefs, NULL);
	*target = source;
}

void
dns_zone_detach(dns_zone_t **zonep) {
	dns_zone_t *zone;
	unsigned int refs;
	isc_boolean_t free_now = ISC_FALSE;

	REQUIRE(zonep != NULL && DNS_ZONE_VALID(*zonep));

	zone = *zonep;

	isc_refcount_decrement(&zone->erefs, &refs);

	if (refs == 0) {
		LOCK_ZONE(zone);
		/*
		 * We just detached the last external reference.
		 */
		if (zone->task != NULL) {
			/*
			 * This zone is being managed.  Post
			 * its control event and let it clean
			 * up synchronously in the context of
			 * its task.
			 */
			isc_event_t *ev = &zone->ctlevent;
			isc_task_send(zone->task, &ev);
		} else {
			/*
			 * This zone is not being managed; it has
			 * no task and can have no outstanding
			 * events.  Free it immediately.
			 */
			/*
			 * Unmanaged zones should not have non-null views;
			 * we have no way of detaching from the view here
			 * without causing deadlock because this code is called
			 * with the view already locked.
			 */
			INSIST(zone->view == NULL);
			free_now = ISC_TRUE;
		}
		UNLOCK_ZONE(zone);
	}
	*zonep = NULL;
	if (free_now)
		zone_free(zone);
}

void
dns_zone_iattach(dns_zone_t *source, dns_zone_t **target) {
	REQUIRE(DNS_ZONE_VALID(source));
	REQUIRE(target != NULL && *target == NULL);
	LOCK_ZONE(source);
	zone_iattach(source, target);
	UNLOCK_ZONE(source);
}

static void
zone_iattach(dns_zone_t *source, dns_zone_t **target) {

	/*
	 * 'source' locked by caller.
	 */
	REQUIRE(LOCKED_ZONE(source));
	REQUIRE(DNS_ZONE_VALID(source));
	REQUIRE(target != NULL && *target == NULL);
	INSIST(source->irefs + isc_refcount_current(&source->erefs) > 0);
	source->irefs++;
	INSIST(source->irefs != 0);
	*target = source;
}

static void
zone_idetach(dns_zone_t **zonep) {
	dns_zone_t *zone;

	/*
	 * 'zone' locked by caller.
	 */
	REQUIRE(zonep != NULL && DNS_ZONE_VALID(*zonep));
	zone = *zonep;
	REQUIRE(LOCKED_ZONE(*zonep));
	*zonep = NULL;

	INSIST(zone->irefs > 0);
	zone->irefs--;
	INSIST(zone->irefs + isc_refcount_current(&zone->erefs) > 0);
}

void
dns_zone_idetach(dns_zone_t **zonep) {
	dns_zone_t *zone;
	isc_boolean_t free_needed;

	REQUIRE(zonep != NULL && DNS_ZONE_VALID(*zonep));
	zone = *zonep;
	*zonep = NULL;

	LOCK_ZONE(zone);
	INSIST(zone->irefs > 0);
	zone->irefs--;
	free_needed = exit_check(zone);
	UNLOCK_ZONE(zone);
	if (free_needed)
		zone_free(zone);
}

isc_mem_t *
dns_zone_getmctx(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));

	return (zone->mctx);
}

dns_zonemgr_t *
dns_zone_getmgr(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));

	return (zone->zmgr);
}

void
dns_zone_setflag(dns_zone_t *zone, unsigned int flags, isc_boolean_t value) {
	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	if (value)
		DNS_ZONE_SETFLAG(zone, flags);
	else
		DNS_ZONE_CLRFLAG(zone, flags);
	UNLOCK_ZONE(zone);
}

void
dns_zone_setoption(dns_zone_t *zone, unsigned int option, isc_boolean_t value)
{
	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	if (value)
		zone->options |= option;
	else
		zone->options &= ~option;
	UNLOCK_ZONE(zone);
}

unsigned int
dns_zone_getoptions(dns_zone_t *zone) {

	REQUIRE(DNS_ZONE_VALID(zone));

	return (zone->options);
}

isc_result_t
dns_zone_setxfrsource4(dns_zone_t *zone, const isc_sockaddr_t *xfrsource) {
	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	zone->xfrsource4 = *xfrsource;
	UNLOCK_ZONE(zone);

	return (ISC_R_SUCCESS);
}

isc_sockaddr_t *
dns_zone_getxfrsource4(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));
	return (&zone->xfrsource4);
}

isc_result_t
dns_zone_setxfrsource6(dns_zone_t *zone, const isc_sockaddr_t *xfrsource) {
	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	zone->xfrsource6 = *xfrsource;
	UNLOCK_ZONE(zone);

	return (ISC_R_SUCCESS);
}

isc_sockaddr_t *
dns_zone_getxfrsource6(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));
	return (&zone->xfrsource6);
}

isc_result_t
dns_zone_setaltxfrsource4(dns_zone_t *zone,
			  const isc_sockaddr_t *altxfrsource)
{
	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	zone->altxfrsource4 = *altxfrsource;
	UNLOCK_ZONE(zone);

	return (ISC_R_SUCCESS);
}

isc_sockaddr_t *
dns_zone_getaltxfrsource4(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));
	return (&zone->altxfrsource4);
}

isc_result_t
dns_zone_setaltxfrsource6(dns_zone_t *zone,
			  const isc_sockaddr_t *altxfrsource)
{
	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	zone->altxfrsource6 = *altxfrsource;
	UNLOCK_ZONE(zone);

	return (ISC_R_SUCCESS);
}

isc_sockaddr_t *
dns_zone_getaltxfrsource6(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));
	return (&zone->altxfrsource6);
}

isc_result_t
dns_zone_setnotifysrc4(dns_zone_t *zone, const isc_sockaddr_t *notifysrc) {
	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	zone->notifysrc4 = *notifysrc;
	UNLOCK_ZONE(zone);

	return (ISC_R_SUCCESS);
}

isc_sockaddr_t *
dns_zone_getnotifysrc4(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));
	return (&zone->notifysrc4);
}

isc_result_t
dns_zone_setnotifysrc6(dns_zone_t *zone, const isc_sockaddr_t *notifysrc) {
	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	zone->notifysrc6 = *notifysrc;
	UNLOCK_ZONE(zone);

	return (ISC_R_SUCCESS);
}

isc_sockaddr_t *
dns_zone_getnotifysrc6(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));
	return (&zone->notifysrc6);
}

isc_result_t
dns_zone_setalsonotify(dns_zone_t *zone, const isc_sockaddr_t *notify,
		       isc_uint32_t count)
{
	isc_sockaddr_t *new;

	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(count == 0 || notify != NULL);

	LOCK_ZONE(zone);
	if (zone->notify != NULL) {
		isc_mem_put(zone->mctx, zone->notify,
			    zone->notifycnt * sizeof(*new));
		zone->notify = NULL;
		zone->notifycnt = 0;
	}
	if (count != 0) {
		new = isc_mem_get(zone->mctx, count * sizeof(*new));
		if (new == NULL) {
			UNLOCK_ZONE(zone);
			return (ISC_R_NOMEMORY);
		}
		memcpy(new, notify, count * sizeof(*new));
		zone->notify = new;
		zone->notifycnt = count;
	}
	UNLOCK_ZONE(zone);
	return (ISC_R_SUCCESS);
}

isc_result_t
dns_zone_setmasters(dns_zone_t *zone, const isc_sockaddr_t *masters,
		    isc_uint32_t count)
{
	isc_result_t result;

	result = dns_zone_setmasterswithkeys(zone, masters, NULL, count);
	return (result);
}

static isc_boolean_t
same_masters(const isc_sockaddr_t *old, const isc_sockaddr_t *new,
	     isc_uint32_t count)
{
	unsigned int i;

	for (i = 0; i < count; i++)
		if (!isc_sockaddr_equal(&old[i], &new[i]))
			return (ISC_FALSE);
	return (ISC_TRUE);
}

static isc_boolean_t
same_keynames(dns_name_t **old, dns_name_t **new, isc_uint32_t count) {
	unsigned int i;

	if (old == NULL && new == NULL)
		return (ISC_TRUE);
	if (old == NULL || new == NULL)
		return (ISC_FALSE);

	for (i = 0; i < count; i++) {
		if (old[i] == NULL && new[i] == NULL)
			continue;
		if (old[i] == NULL || new[i] == NULL ||
		     !dns_name_equal(old[i], new[i]))
			return (ISC_FALSE);
	}
	return (ISC_TRUE);
}

isc_result_t
dns_zone_setmasterswithkeys(dns_zone_t *zone,
			    const isc_sockaddr_t *masters,
			    dns_name_t **keynames,
			    isc_uint32_t count)
{
	isc_sockaddr_t *new;
	isc_result_t result = ISC_R_SUCCESS;
	dns_name_t **newname;
	isc_boolean_t *newok;
	unsigned int i;

	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(count == 0 || masters != NULL);
	if (keynames != NULL) {
		REQUIRE(count != 0);
	}

	LOCK_ZONE(zone);
	/*
	 * The refresh code assumes that 'masters' wouldn't change under it.
	 * If it will change then kill off any current refresh in progress
	 * and update the masters info.  If it won't change then we can just
	 * unlock and exit.
	 */
	if (count != zone->masterscnt ||
	    !same_masters(zone->masters, masters, count) ||
	    !same_keynames(zone->masterkeynames, keynames, count)) {
		if (zone->request != NULL)
			dns_request_cancel(zone->request);
	} else
		goto unlock;
	if (zone->masters != NULL) {
		isc_mem_put(zone->mctx, zone->masters,
			    zone->masterscnt * sizeof(*new));
		zone->masters = NULL;
	}
	if (zone->masterkeynames != NULL) {
		for (i = 0; i < zone->masterscnt; i++) {
			if (zone->masterkeynames[i] != NULL) {
				dns_name_free(zone->masterkeynames[i],
					      zone->mctx);
				isc_mem_put(zone->mctx,
					    zone->masterkeynames[i],
					    sizeof(dns_name_t));
				zone->masterkeynames[i] = NULL;
			}
		}
		isc_mem_put(zone->mctx, zone->masterkeynames,
			    zone->masterscnt * sizeof(dns_name_t *));
		zone->masterkeynames = NULL;
	}
	if (zone->mastersok != NULL) {
		isc_mem_put(zone->mctx, zone->mastersok,
			    zone->masterscnt * sizeof(isc_boolean_t));
		zone->mastersok = NULL;
	}
	zone->masterscnt = 0;
	/*
	 * If count == 0, don't allocate any space for masters, mastersok or
	 * keynames so internally, those pointers are NULL if count == 0
	 */
	if (count == 0)
		goto unlock;

	/*
	 * masters must contain count elements!
	 */
	new = isc_mem_get(zone->mctx, count * sizeof(*new));
	if (new == NULL) {
		result = ISC_R_NOMEMORY;
		goto unlock;
	}
	memcpy(new, masters, count * sizeof(*new));

	/*
	 * Similarly for mastersok.
	 */
	newok = isc_mem_get(zone->mctx, count * sizeof(*newok));
	if (newok == NULL) {
		result = ISC_R_NOMEMORY;
		isc_mem_put(zone->mctx, new, count * sizeof(*new));
		goto unlock;
	};
	for (i = 0; i < count; i++)
		newok[i] = ISC_FALSE;

	/*
	 * if keynames is non-NULL, it must contain count elements!
	 */
	newname = NULL;
	if (keynames != NULL) {
		newname = isc_mem_get(zone->mctx, count * sizeof(*newname));
		if (newname == NULL) {
			result = ISC_R_NOMEMORY;
			isc_mem_put(zone->mctx, new, count * sizeof(*new));
			isc_mem_put(zone->mctx, newok, count * sizeof(*newok));
			goto unlock;
		}
		for (i = 0; i < count; i++)
			newname[i] = NULL;
		for (i = 0; i < count; i++) {
			if (keynames[i] != NULL) {
				newname[i] = isc_mem_get(zone->mctx,
							 sizeof(dns_name_t));
				if (newname[i] == NULL)
					goto allocfail;
				dns_name_init(newname[i], NULL);
				result = dns_name_dup(keynames[i], zone->mctx,
						      newname[i]);
				if (result != ISC_R_SUCCESS) {
				allocfail:
					for (i = 0; i < count; i++)
						if (newname[i] != NULL)
							dns_name_free(
							       newname[i],
							       zone->mctx);
					isc_mem_put(zone->mctx, new,
						    count * sizeof(*new));
					isc_mem_put(zone->mctx, newok,
						    count * sizeof(*newok));
					isc_mem_put(zone->mctx, newname,
						    count * sizeof(*newname));
					goto unlock;
				}
			}
		}
	}

	/*
	 * Everything is ok so attach to the zone.
	 */
	zone->masters = new;
	zone->mastersok = newok;
	zone->masterkeynames = newname;
	zone->masterscnt = count;
	DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_NOMASTERS);

 unlock:
	UNLOCK_ZONE(zone);
	return (result);
}

isc_result_t
dns_zone_getdb(dns_zone_t *zone, dns_db_t **dpb) {
	isc_result_t result = ISC_R_SUCCESS;

	REQUIRE(DNS_ZONE_VALID(zone));

	ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_read);
	if (zone->db == NULL)
		result = DNS_R_NOTLOADED;
	else
		dns_db_attach(zone->db, dpb);
	ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);

	return (result);
}

/*
 * Co-ordinates the starting of routine jobs.
 */

void
dns_zone_maintenance(dns_zone_t *zone) {
	const char me[] = "dns_zone_maintenance";
	isc_time_t now;

	REQUIRE(DNS_ZONE_VALID(zone));
	ENTER;

	LOCK_ZONE(zone);
	TIME_NOW(&now);
	zone_settimer(zone, &now);
	UNLOCK_ZONE(zone);
}

static inline isc_boolean_t
was_dumping(dns_zone_t *zone) {
	isc_boolean_t dumping;

	REQUIRE(LOCKED_ZONE(zone));

	dumping = DNS_ZONE_FLAG(zone, DNS_ZONEFLG_DUMPING);
	DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_DUMPING);
	if (!dumping) {
		DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_NEEDDUMP);
		isc_time_settoepoch(&zone->dumptime);
	}
	return (dumping);
}

static void
zone_maintenance(dns_zone_t *zone) {
	const char me[] = "zone_maintenance";
	isc_time_t now;
	isc_result_t result;
	isc_boolean_t dumping;

	REQUIRE(DNS_ZONE_VALID(zone));
	ENTER;

	/*
	 * Configuring the view of this zone may have
	 * failed, for example because the config file
	 * had a syntax error.  In that case, the view
	 * adb or resolver, and we had better not try
	 * to do maintenance on it.
	 */
	if (zone->view == NULL || zone->view->adb == NULL)
		return;

	TIME_NOW(&now);

	/*
	 * Expire check.
	 */
	switch (zone->type) {
	case dns_zone_slave:
	case dns_zone_stub:
		LOCK_ZONE(zone);
		if (isc_time_compare(&now, &zone->expiretime) >= 0 &&
		    DNS_ZONE_FLAG(zone, DNS_ZONEFLG_LOADED)) {
			zone_expire(zone);
			zone->refreshtime = now;
		}
		UNLOCK_ZONE(zone);
		break;
	default:
		break;
	}

	/*
	 * Up to date check.
	 */
	switch (zone->type) {
	case dns_zone_slave:
	case dns_zone_stub:
		if (!DNS_ZONE_FLAG(zone, DNS_ZONEFLG_DIALREFRESH) &&
		    isc_time_compare(&now, &zone->refreshtime) >= 0)
			dns_zone_refresh(zone);
		break;
	default:
		break;
	}

	/*
	 * Do we need to consolidate the backing store?
	 */
	switch (zone->type) {
	case dns_zone_master:
	case dns_zone_slave:
		LOCK_ZONE(zone);
		if (zone->masterfile != NULL &&
		    isc_time_compare(&now, &zone->dumptime) >= 0 &&
		    DNS_ZONE_FLAG(zone, DNS_ZONEFLG_LOADED) &&
		    DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NEEDDUMP)) {
			dumping = was_dumping(zone);
		} else
			dumping = ISC_TRUE;
		UNLOCK_ZONE(zone);
		if (!dumping) {
			result = zone_dump(zone, ISC_TRUE); /* task locked */
			if (result != ISC_R_SUCCESS)
				dns_zone_log(zone, ISC_LOG_WARNING,
					     "dump failed: %s",
					     dns_result_totext(result));
		}
		break;
	default:
		break;
	}

	/*
	 * Do we need to send out notify messages?
	 */
	switch (zone->type) {
	case dns_zone_master:
	case dns_zone_slave:
		if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NEEDNOTIFY) &&
		    isc_time_compare(&now, &zone->notifytime) >= 0)
			zone_notify(zone, &now);
		break;
	default:
		break;
	}
	zone_settimer(zone, &now);
}

void
dns_zone_markdirty(dns_zone_t *zone) {

	LOCK_ZONE(zone);
	zone_needdump(zone, DNS_DUMP_DELAY);
	UNLOCK_ZONE(zone);
}

void
dns_zone_expire(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	zone_expire(zone);
	UNLOCK_ZONE(zone);
}

static void
zone_expire(dns_zone_t *zone) {
	/*
	 * 'zone' locked by caller.
	 */

	REQUIRE(LOCKED_ZONE(zone));

	dns_zone_log(zone, ISC_LOG_WARNING, "expired");

	DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_EXPIRED);
	zone->refresh = DNS_ZONE_DEFAULTREFRESH;
	zone->retry = DNS_ZONE_DEFAULTRETRY;
	DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_HAVETIMERS);
	zone_unload(zone);
}

void
dns_zone_refresh(dns_zone_t *zone) {
	isc_interval_t i;
	isc_uint32_t oldflags;
	unsigned int j;
	isc_result_t result;

	REQUIRE(DNS_ZONE_VALID(zone));

	if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_EXITING))
		return;

	/*
	 * Set DNS_ZONEFLG_REFRESH so that there is only one refresh operation
	 * in progress at a time.
	 */

	LOCK_ZONE(zone);
	oldflags = zone->flags;
	if (zone->masterscnt == 0) {
		DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_NOMASTERS);
		if ((oldflags & DNS_ZONEFLG_NOMASTERS) == 0)
			dns_zone_log(zone, ISC_LOG_ERROR,
				     "cannot refresh: no masters");
		goto unlock;
	}
	DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_REFRESH);
	DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_NOEDNS);
	DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_USEALTXFRSRC);
	if ((oldflags & (DNS_ZONEFLG_REFRESH|DNS_ZONEFLG_LOADING)) != 0)
		goto unlock;

	/*
	 * Set the next refresh time as if refresh check has failed.
	 * Setting this to the retry time will do that.  XXXMLG
	 * If we are successful it will be reset using zone->refresh.
	 */
	isc_interval_set(&i, isc_random_jitter(zone->retry, zone->retry / 4),
			 0);
	result = isc_time_nowplusinterval(&zone->refreshtime, &i);
	if (result |= ISC_R_SUCCESS)
		dns_zone_log(zone, ISC_LOG_WARNING,
			     "isc_time_nowplusinterval() failed: %s",
			     dns_result_totext(result));

	/*
	 * When lacking user-specified timer values from the SOA,
	 * do exponential backoff of the retry time up to a
	 * maximum of six hours.
	 */
	if (! DNS_ZONE_FLAG(zone, DNS_ZONEFLG_HAVETIMERS))
		zone->retry = ISC_MIN(zone->retry * 2, 6 * 3600);

	zone->curmaster = 0;
	for (j = 0; j < zone->masterscnt; j++)
		zone->mastersok[j] = ISC_FALSE;
	/* initiate soa query */
	queue_soa_query(zone);
 unlock:
	UNLOCK_ZONE(zone);
}

isc_result_t
dns_zone_flush(dns_zone_t *zone) {
	isc_result_t result = ISC_R_SUCCESS;
	isc_boolean_t dumping;

	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_FLUSH);
	if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NEEDDUMP) &&
	    zone->masterfile != NULL) {
		result = ISC_R_ALREADYRUNNING;
		dumping = was_dumping(zone);
	} else
		dumping = ISC_TRUE;
	UNLOCK_ZONE(zone);
	if (!dumping)
		result = zone_dump(zone, ISC_FALSE);	/* Unknown task. */
	return (result);
}

isc_result_t
dns_zone_dump(dns_zone_t *zone) {
	isc_result_t result = ISC_R_ALREADYRUNNING;
	isc_boolean_t dumping;

	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	dumping = was_dumping(zone);
	UNLOCK_ZONE(zone);
	if (!dumping)
		result = zone_dump(zone, ISC_FALSE);	/* Unknown task. */
	return (result);
}

static void
zone_needdump(dns_zone_t *zone, unsigned int delay) {
	isc_time_t dumptime;
	isc_time_t now;

	/*
	 * 'zone' locked by caller
	 */

	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(LOCKED_ZONE(zone));

	/*
	 * Do we have a place to dump to and are we loaded?
	 */
	if (zone->masterfile == NULL ||
	    DNS_ZONE_FLAG(zone, DNS_ZONEFLG_LOADED) == 0)
		return;

	TIME_NOW(&now);
	/* add some noise */
	DNS_ZONE_JITTER_ADD(&now, delay, &dumptime);

	DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_NEEDDUMP);
	if (isc_time_isepoch(&zone->dumptime) ||
	    isc_time_compare(&zone->dumptime, &dumptime) > 0)
		zone->dumptime = dumptime;
	if (zone->task != NULL)
		zone_settimer(zone, &now);
}

static void
dump_done(void *arg, isc_result_t result) {
	const char me[] = "dump_done";
	dns_zone_t *zone = arg;
	dns_db_t *db;
	dns_dbversion_t *version;
	isc_boolean_t again = ISC_FALSE;
	isc_boolean_t compact = ISC_FALSE;
	isc_uint32_t serial;
	isc_result_t tresult;

	REQUIRE(DNS_ZONE_VALID(zone));

	ENTER;

	if (result == ISC_R_SUCCESS && zone->journal != NULL &&
	    zone->journalsize != -1) {

		/*
		 * We don't own these, zone->dctx must stay valid.
		 */
		db = dns_dumpctx_db(zone->dctx);
		version = dns_dumpctx_version(zone->dctx);

		tresult = dns_db_getsoaserial(db, version, &serial);
		/*
		 * Note: we are task locked here so we can test
		 * zone->xfr safely.
		 */
		if (tresult == ISC_R_SUCCESS && zone->xfr == NULL) {
			tresult = dns_journal_compact(zone->mctx,
						      zone->journal,
						      serial,
						      zone->journalsize);
			switch (tresult) {
			case ISC_R_SUCCESS:
			case ISC_R_NOSPACE:
			case ISC_R_NOTFOUND:
				dns_zone_log(zone, ISC_LOG_DEBUG(3),
					     "dns_journal_compact: %s",
					     dns_result_totext(tresult));
				break;
			default:
				dns_zone_log(zone, ISC_LOG_ERROR,
					     "dns_journal_compact failed: %s",
					     dns_result_totext(tresult));
				break;
			}
		} else if (tresult == ISC_R_SUCCESS) {
			compact = ISC_TRUE;
			zone->compact_serial = serial;
		}
	}

	LOCK_ZONE(zone);
	DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_DUMPING);
	if (compact)
		DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_NEEDCOMPACT);
	if (result != ISC_R_SUCCESS && result != ISC_R_CANCELED) {
		/*
		 * Try again in a short while.
		 */
		zone_needdump(zone, DNS_DUMP_DELAY);
	} else if (result == ISC_R_SUCCESS &&
		   DNS_ZONE_FLAG(zone, DNS_ZONEFLG_FLUSH) &&
		   DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NEEDDUMP) &&
		   DNS_ZONE_FLAG(zone, DNS_ZONEFLG_LOADED)) {
		DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_NEEDDUMP);
		DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_DUMPING);
		isc_time_settoepoch(&zone->dumptime);
		again = ISC_TRUE;
	} else if (result == ISC_R_SUCCESS)
		DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_FLUSH);

	if (zone->dctx != NULL)
		dns_dumpctx_detach(&zone->dctx);
	zonemgr_putio(&zone->writeio);
	UNLOCK_ZONE(zone);
	if (again)
		(void)zone_dump(zone, ISC_FALSE);
	dns_zone_idetach(&zone);
}

static isc_result_t
zone_dump(dns_zone_t *zone, isc_boolean_t compact) {
	const char me[] = "zone_dump";
	isc_result_t result;
	dns_dbversion_t *version = NULL;
	isc_boolean_t again;
	dns_db_t *db = NULL;
	char *masterfile = NULL;
	dns_masterformat_t masterformat = dns_masterformat_none;

/*
 * 'compact' MUST only be set if we are task locked.
 */

	REQUIRE(DNS_ZONE_VALID(zone));
	ENTER;

 redo:
	ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_read);
	if (zone->db != NULL)
		dns_db_attach(zone->db, &db);
	ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);
	LOCK_ZONE(zone);
	if (zone->masterfile != NULL) {
		masterfile = isc_mem_strdup(zone->mctx, zone->masterfile);
		masterformat = zone->masterformat;
	}
	UNLOCK_ZONE(zone);
	if (db == NULL) {
		result = DNS_R_NOTLOADED;
		goto fail;
	}
	if (masterfile == NULL) {
		result = DNS_R_NOMASTERFILE;
		goto fail;
	}

	if (compact) {
		dns_zone_t *dummy = NULL;
		LOCK_ZONE(zone);
		zone_iattach(zone, &dummy);
		result = zonemgr_getio(zone->zmgr, ISC_FALSE, zone->task,
				       zone_gotwritehandle, zone,
				       &zone->writeio);
		if (result != ISC_R_SUCCESS)
			zone_idetach(&dummy);
		else
			result = DNS_R_CONTINUE;
		UNLOCK_ZONE(zone);
	} else {
		dns_db_currentversion(db, &version);
		result = dns_master_dump2(zone->mctx, db, version,
					  &dns_master_style_default,
					  masterfile, masterformat);
		dns_db_closeversion(db, &version, ISC_FALSE);
	}
 fail:
	if (db != NULL)
		dns_db_detach(&db);
	if (masterfile != NULL)
		isc_mem_free(zone->mctx, masterfile);
	masterfile = NULL;

	if (result == DNS_R_CONTINUE)
		return (ISC_R_SUCCESS); /* XXXMPA */

	again = ISC_FALSE;
	LOCK_ZONE(zone);
	DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_DUMPING);
	if (result != ISC_R_SUCCESS) {
		/*
		 * Try again in a short while.
		 */
		zone_needdump(zone, DNS_DUMP_DELAY);
	} else if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_FLUSH) &&
		   DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NEEDDUMP) &&
		   DNS_ZONE_FLAG(zone, DNS_ZONEFLG_LOADED)) {
		DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_NEEDDUMP);
		DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_DUMPING);
		isc_time_settoepoch(&zone->dumptime);
		again = ISC_TRUE;
	} else
		DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_FLUSH);
	UNLOCK_ZONE(zone);
	if (again)
		goto redo;

	return (result);
}

static isc_result_t
dumptostream(dns_zone_t *zone, FILE *fd, const dns_master_style_t *style,
	     dns_masterformat_t format)
{
	isc_result_t result;
	dns_dbversion_t *version = NULL;
	dns_db_t *db = NULL;

	REQUIRE(DNS_ZONE_VALID(zone));

	ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_read);
	if (zone->db != NULL)
		dns_db_attach(zone->db, &db);
	ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);
	if (db == NULL)
		return (DNS_R_NOTLOADED);

	dns_db_currentversion(db, &version);
	result = dns_master_dumptostream2(zone->mctx, db, version, style,
					  format, fd);
	dns_db_closeversion(db, &version, ISC_FALSE);
	dns_db_detach(&db);
	return (result);
}

isc_result_t
dns_zone_dumptostream2(dns_zone_t *zone, FILE *fd, dns_masterformat_t format,
		       const dns_master_style_t *style) {
	return dumptostream(zone, fd, style, format);
}

isc_result_t
dns_zone_dumptostream(dns_zone_t *zone, FILE *fd) {
	return dumptostream(zone, fd, &dns_master_style_default,
			    dns_masterformat_text);
}

isc_result_t
dns_zone_fulldumptostream(dns_zone_t *zone, FILE *fd) {
	return dumptostream(zone, fd, &dns_master_style_full,
			    dns_masterformat_text);
}

void
dns_zone_unload(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	zone_unload(zone);
	UNLOCK_ZONE(zone);
}

static void
notify_cancel(dns_zone_t *zone) {
	dns_notify_t *notify;

	/*
	 * 'zone' locked by caller.
	 */

	REQUIRE(LOCKED_ZONE(zone));

	for (notify = ISC_LIST_HEAD(zone->notifies);
	     notify != NULL;
	     notify = ISC_LIST_NEXT(notify, link)) {
		if (notify->find != NULL)
			dns_adb_cancelfind(notify->find);
		if (notify->request != NULL)
			dns_request_cancel(notify->request);
	}
}

static void
zone_unload(dns_zone_t *zone) {

	/*
	 * 'zone' locked by caller.
	 */

	REQUIRE(LOCKED_ZONE(zone));

	ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_write);
	zone_detachdb(zone);
	ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_write);
	DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_LOADED);
	DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_NEEDDUMP);
}

void
dns_zone_setminrefreshtime(dns_zone_t *zone, isc_uint32_t val) {
	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(val > 0);

	zone->minrefresh = val;
}

void
dns_zone_setmaxrefreshtime(dns_zone_t *zone, isc_uint32_t val) {
	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(val > 0);

	zone->maxrefresh = val;
}

void
dns_zone_setminretrytime(dns_zone_t *zone, isc_uint32_t val) {
	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(val > 0);

	zone->minretry = val;
}

void
dns_zone_setmaxretrytime(dns_zone_t *zone, isc_uint32_t val) {
	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(val > 0);

	zone->maxretry = val;
}

static isc_boolean_t
notify_isqueued(dns_zone_t *zone, dns_name_t *name, isc_sockaddr_t *addr) {
	dns_notify_t *notify;

	for (notify = ISC_LIST_HEAD(zone->notifies);
	     notify != NULL;
	     notify = ISC_LIST_NEXT(notify, link)) {
		if (notify->request != NULL)
			continue;
		if (name != NULL && dns_name_dynamic(&notify->ns) &&
		    dns_name_equal(name, &notify->ns))
			return (ISC_TRUE);
		if (addr != NULL && isc_sockaddr_equal(addr, &notify->dst))
			return (ISC_TRUE);
	}
	return (ISC_FALSE);
}

static isc_boolean_t
notify_isself(dns_zone_t *zone, isc_sockaddr_t *dst) {
	dns_tsigkey_t *key = NULL;
	isc_sockaddr_t src;
	isc_sockaddr_t any;
	isc_boolean_t isself;
	isc_netaddr_t dstaddr;

	if (zone->view == NULL || zone->isself == NULL)
		return (ISC_FALSE);

	switch (isc_sockaddr_pf(dst)) {
	case PF_INET:
		src = zone->notifysrc4;
		isc_sockaddr_any(&any);
		break;
	case PF_INET6:
		src = zone->notifysrc6;
		isc_sockaddr_any6(&any);
		break;
	default:
		return (ISC_FALSE);
	}

	/*
	 * When sending from any the kernel will assign a source address
	 * that matches the destination address.
	 */
	if (isc_sockaddr_eqaddr(&any, &src))
		src = *dst;

	isc_netaddr_fromsockaddr(&dstaddr, dst);
	(void)dns_view_getpeertsig(zone->view, &dstaddr, &key);
	isself = (zone->isself)(zone->view, key, &src, dst, zone->rdclass,
				zone->isselfarg);
	if (key != NULL)
		dns_tsigkey_detach(&key);
	return (isself);
}

static void
notify_destroy(dns_notify_t *notify, isc_boolean_t locked) {
	isc_mem_t *mctx;

	/*
	 * Caller holds zone lock.
	 */
	REQUIRE(DNS_NOTIFY_VALID(notify));

	if (notify->zone != NULL) {
		if (!locked)
			LOCK_ZONE(notify->zone);
		REQUIRE(LOCKED_ZONE(notify->zone));
		if (ISC_LINK_LINKED(notify, link))
			ISC_LIST_UNLINK(notify->zone->notifies, notify, link);
		if (!locked)
			UNLOCK_ZONE(notify->zone);
		if (locked)
			zone_idetach(&notify->zone);
		else
			dns_zone_idetach(&notify->zone);
	}
	if (notify->find != NULL)
		dns_adb_destroyfind(&notify->find);
	if (notify->request != NULL)
		dns_request_destroy(&notify->request);
	if (dns_name_dynamic(&notify->ns))
		dns_name_free(&notify->ns, notify->mctx);
	mctx = notify->mctx;
	isc_mem_put(notify->mctx, notify, sizeof(*notify));
	isc_mem_detach(&mctx);
}

static isc_result_t
notify_create(isc_mem_t *mctx, unsigned int flags, dns_notify_t **notifyp) {
	dns_notify_t *notify;

	REQUIRE(notifyp != NULL && *notifyp == NULL);

	notify = isc_mem_get(mctx, sizeof(*notify));
	if (notify == NULL)
		return (ISC_R_NOMEMORY);

	notify->mctx = NULL;
	isc_mem_attach(mctx, &notify->mctx);
	notify->flags = flags;
	notify->zone = NULL;
	notify->find = NULL;
	notify->request = NULL;
	isc_sockaddr_any(&notify->dst);
	dns_name_init(&notify->ns, NULL);
	ISC_LINK_INIT(notify, link);
	notify->magic = NOTIFY_MAGIC;
	*notifyp = notify;
	return (ISC_R_SUCCESS);
}

/*
 * XXXAG should check for DNS_ZONEFLG_EXITING
 */
static void
process_adb_event(isc_task_t *task, isc_event_t *ev) {
	dns_notify_t *notify;
	isc_eventtype_t result;

	UNUSED(task);

	notify = ev->ev_arg;
	REQUIRE(DNS_NOTIFY_VALID(notify));
	INSIST(task == notify->zone->task);
	result = ev->ev_type;
	isc_event_free(&ev);
	if (result == DNS_EVENT_ADBMOREADDRESSES) {
		dns_adb_destroyfind(&notify->find);
		notify_find_address(notify);
		return;
	}
	if (result == DNS_EVENT_ADBNOMOREADDRESSES) {
		LOCK_ZONE(notify->zone);
		notify_send(notify);
		UNLOCK_ZONE(notify->zone);
	}
	notify_destroy(notify, ISC_FALSE);
}

static void
notify_find_address(dns_notify_t *notify) {
	isc_result_t result;
	unsigned int options;

	REQUIRE(DNS_NOTIFY_VALID(notify));
	options = DNS_ADBFIND_WANTEVENT | DNS_ADBFIND_INET |
		  DNS_ADBFIND_INET6 | DNS_ADBFIND_RETURNLAME;

	if (notify->zone->view->adb == NULL)
		goto destroy;

	result = dns_adb_createfind(notify->zone->view->adb,
				    notify->zone->task,
				    process_adb_event, notify,
				    &notify->ns, dns_rootname, 0,
				    options, 0, NULL,
				    notify->zone->view->dstport,
				    &notify->find);

	/* Something failed? */
	if (result != ISC_R_SUCCESS)
		goto destroy;

	/* More addresses pending? */
	if ((notify->find->options & DNS_ADBFIND_WANTEVENT) != 0)
		return;

	/* We have as many addresses as we can get. */
	LOCK_ZONE(notify->zone);
	notify_send(notify);
	UNLOCK_ZONE(notify->zone);

 destroy:
	notify_destroy(notify, ISC_FALSE);
}


static isc_result_t
notify_send_queue(dns_notify_t *notify) {
	isc_event_t *e;
	isc_result_t result;

	e = isc_event_allocate(notify->mctx, NULL,
			       DNS_EVENT_NOTIFYSENDTOADDR,
			       notify_send_toaddr,
			       notify, sizeof(isc_event_t));
	if (e == NULL)
		return (ISC_R_NOMEMORY);
	e->ev_arg = notify;
	e->ev_sender = NULL;
	result = isc_ratelimiter_enqueue(notify->zone->zmgr->rl,
					 notify->zone->task, &e);
	if (result != ISC_R_SUCCESS)
		isc_event_free(&e);
	return (result);
}

static void
notify_send_toaddr(isc_task_t *task, isc_event_t *event) {
	dns_notify_t *notify;
	isc_result_t result;
	dns_message_t *message = NULL;
	isc_netaddr_t dstip;
	dns_tsigkey_t *key = NULL;
	char addrbuf[ISC_SOCKADDR_FORMATSIZE];
	isc_sockaddr_t src;
	int timeout;
	isc_boolean_t have_notifysource = ISC_FALSE;

	notify = event->ev_arg;
	REQUIRE(DNS_NOTIFY_VALID(notify));

	UNUSED(task);

	LOCK_ZONE(notify->zone);

	if (DNS_ZONE_FLAG(notify->zone, DNS_ZONEFLG_LOADED) == 0) {
		result = ISC_R_CANCELED;
		goto cleanup;
	}

	if ((event->ev_attributes & ISC_EVENTATTR_CANCELED) != 0 ||
	    DNS_ZONE_FLAG(notify->zone, DNS_ZONEFLG_EXITING) ||
	    notify->zone->view->requestmgr == NULL ||
	    notify->zone->db == NULL) {
		result = ISC_R_CANCELED;
		goto cleanup;
	}

	/*
	 * The raw IPv4 address should also exist.  Don't send to the
	 * mapped form.
	 */
	if (isc_sockaddr_pf(&notify->dst) == PF_INET6 &&
	    IN6_IS_ADDR_V4MAPPED(&notify->dst.type.sin6.sin6_addr)) {
		isc_sockaddr_format(&notify->dst, addrbuf, sizeof(addrbuf));
		notify_log(notify->zone, ISC_LOG_DEBUG(3),
			   "notify: ignoring IPv6 mapped IPV4 address: %s",
			   addrbuf);
		result = ISC_R_CANCELED;
		goto cleanup;
	}

	result = notify_createmessage(notify->zone, notify->flags, &message);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	isc_netaddr_fromsockaddr(&dstip, &notify->dst);
	(void)dns_view_getpeertsig(notify->zone->view, &dstip, &key);

	isc_sockaddr_format(&notify->dst, addrbuf, sizeof(addrbuf));
	notify_log(notify->zone, ISC_LOG_DEBUG(3), "sending notify to %s",
		   addrbuf);
	if (notify->zone->view->peers != NULL) {
		dns_peer_t *peer = NULL;
		result = dns_peerlist_peerbyaddr(notify->zone->view->peers,
						 &dstip, &peer);
		if (result == ISC_R_SUCCESS) {
			result = dns_peer_getnotifysource(peer, &src);
			if (result == ISC_R_SUCCESS)
				have_notifysource = ISC_TRUE;
		}
	}
	switch (isc_sockaddr_pf(&notify->dst)) {
	case PF_INET:
		if (!have_notifysource)
			src = notify->zone->notifysrc4;
		break;
	case PF_INET6:
		if (!have_notifysource)
			src = notify->zone->notifysrc6;
		break;
	default:
		result = ISC_R_NOTIMPLEMENTED;
		goto cleanup_key;
	}
	timeout = 15;
	if (DNS_ZONE_FLAG(notify->zone, DNS_ZONEFLG_DIALNOTIFY))
		timeout = 30;
	result = dns_request_createvia2(notify->zone->view->requestmgr,
					message, &src, &notify->dst, 0, key,
					timeout * 3, timeout,
					notify->zone->task, notify_done,
					notify, &notify->request);
 cleanup_key:
	if (key != NULL)
		dns_tsigkey_detach(&key);
	dns_message_destroy(&message);
 cleanup:
	UNLOCK_ZONE(notify->zone);
	if (result != ISC_R_SUCCESS)
		notify_destroy(notify, ISC_FALSE);
	isc_event_free(&event);
}

static void
notify_send(dns_notify_t *notify) {
	dns_adbaddrinfo_t *ai;
	isc_sockaddr_t dst;
	isc_result_t result;
	dns_notify_t *new = NULL;

	/*
	 * Zone lock held by caller.
	 */
	REQUIRE(DNS_NOTIFY_VALID(notify));
	REQUIRE(LOCKED_ZONE(notify->zone));

	for (ai = ISC_LIST_HEAD(notify->find->list);
	     ai != NULL;
	     ai = ISC_LIST_NEXT(ai, publink)) {
		dst = ai->sockaddr;
		if (notify_isqueued(notify->zone, NULL, &dst))
			continue;
		if (notify_isself(notify->zone, &dst))
			continue;
		new = NULL;
		result = notify_create(notify->mctx,
				       (notify->flags & DNS_NOTIFY_NOSOA),
				       &new);
		if (result != ISC_R_SUCCESS)
			goto cleanup;
		zone_iattach(notify->zone, &new->zone);
		ISC_LIST_APPEND(new->zone->notifies, new, link);
		new->dst = dst;
		result = notify_send_queue(new);
		if (result != ISC_R_SUCCESS)
			goto cleanup;
		new = NULL;
	}

 cleanup:
	if (new != NULL)
		notify_destroy(new, ISC_TRUE);
}

void
dns_zone_notify(dns_zone_t *zone) {
	isc_time_t now;

	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_NEEDNOTIFY);

	TIME_NOW(&now);
	zone_settimer(zone, &now);
	UNLOCK_ZONE(zone);
}

static void
zone_notify(dns_zone_t *zone, isc_time_t *now) {
	dns_dbnode_t *node = NULL;
	dns_db_t *zonedb = NULL;
	dns_dbversion_t *version = NULL;
	dns_name_t *origin = NULL;
	dns_name_t master;
	dns_rdata_ns_t ns;
	dns_rdata_soa_t soa;
	isc_uint32_t serial;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdataset_t nsrdset;
	dns_rdataset_t soardset;
	isc_result_t result;
	dns_notify_t *notify = NULL;
	unsigned int i;
	isc_sockaddr_t dst;
	isc_boolean_t isqueued;
	dns_notifytype_t notifytype;
	unsigned int flags = 0;
	isc_boolean_t loggednotify = ISC_FALSE;

	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_NEEDNOTIFY);
	notifytype = zone->notifytype;
	DNS_ZONE_TIME_ADD(now, zone->notifydelay, &zone->notifytime);
	UNLOCK_ZONE(zone);

	if (! DNS_ZONE_FLAG(zone, DNS_ZONEFLG_LOADED))
		return;

	if (notifytype == dns_notifytype_no)
		return;

	if (notifytype == dns_notifytype_masteronly &&
	    zone->type != dns_zone_master)
		return;

	origin = &zone->origin;

	/*
	 * If the zone is dialup we are done as we don't want to send
	 * the current soa so as to force a refresh query.
	 */
	if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_DIALNOTIFY))
		flags |= DNS_NOTIFY_NOSOA;

	/*
	 * Get SOA RRset.
	 */
	ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_read);
	if (zone->db != NULL)
		dns_db_attach(zone->db, &zonedb);
	ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);
	if (zonedb == NULL)
		return;
	dns_db_currentversion(zonedb, &version);
	result = dns_db_findnode(zonedb, origin, ISC_FALSE, &node);
	if (result != ISC_R_SUCCESS)
		goto cleanup1;

	dns_rdataset_init(&soardset);
	result = dns_db_findrdataset(zonedb, node, version, dns_rdatatype_soa,
				     dns_rdatatype_none, 0, &soardset, NULL);
	if (result != ISC_R_SUCCESS)
		goto cleanup2;

	/*
	 * Find serial and master server's name.
	 */
	dns_name_init(&master, NULL);
	result = dns_rdataset_first(&soardset);
	if (result != ISC_R_SUCCESS)
		goto cleanup3;
	dns_rdataset_current(&soardset, &rdata);
	result = dns_rdata_tostruct(&rdata, &soa, NULL);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);
	dns_rdata_reset(&rdata);
	result = dns_name_dup(&soa.origin, zone->mctx, &master);
	serial = soa.serial;
	dns_rdataset_disassociate(&soardset);
	if (result != ISC_R_SUCCESS)
		goto cleanup3;

	/*
	 * Enqueue notify requests for 'also-notify' servers.
	 */
	LOCK_ZONE(zone);
	for (i = 0; i < zone->notifycnt; i++) {
		dst = zone->notify[i];
		if (notify_isqueued(zone, NULL, &dst))
			continue;
		result = notify_create(zone->mctx, flags, &notify);
		if (result != ISC_R_SUCCESS)
			continue;
		zone_iattach(zone, &notify->zone);
		notify->dst = dst;
		ISC_LIST_APPEND(zone->notifies, notify, link);
		result = notify_send_queue(notify);
		if (result != ISC_R_SUCCESS)
			notify_destroy(notify, ISC_TRUE);
		if (!loggednotify) {
			notify_log(zone, ISC_LOG_INFO,
				   "sending notifies (serial %u)",
				   serial);
			loggednotify = ISC_TRUE;
		}
		notify = NULL;
	}
	UNLOCK_ZONE(zone);

	if (notifytype == dns_notifytype_explicit)
		goto cleanup3;

	/*
	 * Process NS RRset to generate notifies.
	 */

	dns_rdataset_init(&nsrdset);
	result = dns_db_findrdataset(zonedb, node, version, dns_rdatatype_ns,
				     dns_rdatatype_none, 0, &nsrdset, NULL);
	if (result != ISC_R_SUCCESS)
		goto cleanup3;

	result = dns_rdataset_first(&nsrdset);
	while (result == ISC_R_SUCCESS) {
		dns_rdataset_current(&nsrdset, &rdata);
		result = dns_rdata_tostruct(&rdata, &ns, NULL);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);
		dns_rdata_reset(&rdata);
		/*
		 * don't notify the master server.
		 */
		if (dns_name_compare(&master, &ns.name) == 0) {
			result = dns_rdataset_next(&nsrdset);
			continue;
		}

		if (!loggednotify) {
			notify_log(zone, ISC_LOG_INFO,
				   "sending notifies (serial %u)",
				   serial);
			loggednotify = ISC_TRUE;
		}

		LOCK_ZONE(zone);
		isqueued = notify_isqueued(zone, &ns.name, NULL);
		UNLOCK_ZONE(zone);
		if (isqueued) {
			result = dns_rdataset_next(&nsrdset);
			continue;
		}
		result = notify_create(zone->mctx, flags, &notify);
		if (result != ISC_R_SUCCESS)
			continue;
		dns_zone_iattach(zone, &notify->zone);
		result = dns_name_dup(&ns.name, zone->mctx, &notify->ns);
		if (result != ISC_R_SUCCESS) {
			LOCK_ZONE(zone);
			notify_destroy(notify, ISC_TRUE);
			UNLOCK_ZONE(zone);
			continue;
		}
		LOCK_ZONE(zone);
		ISC_LIST_APPEND(zone->notifies, notify, link);
		UNLOCK_ZONE(zone);
		notify_find_address(notify);
		notify = NULL;
		result = dns_rdataset_next(&nsrdset);
	}
	dns_rdataset_disassociate(&nsrdset);

 cleanup3:
	if (dns_name_dynamic(&master))
		dns_name_free(&master, zone->mctx);
 cleanup2:
	dns_db_detachnode(zonedb, &node);
 cleanup1:
	dns_db_closeversion(zonedb, &version, ISC_FALSE);
	dns_db_detach(&zonedb);
}

/***
 *** Private
 ***/

static inline isc_result_t
save_nsrrset(dns_message_t *message, dns_name_t *name,
	     dns_db_t *db, dns_dbversion_t *version)
{
	dns_rdataset_t *nsrdataset = NULL;
	dns_rdataset_t *rdataset = NULL;
	dns_dbnode_t *node = NULL;
	dns_rdata_ns_t ns;
	isc_result_t result;
	dns_rdata_t rdata = DNS_RDATA_INIT;

	/*
	 * Extract NS RRset from message.
	 */
	result = dns_message_findname(message, DNS_SECTION_ANSWER, name,
				      dns_rdatatype_ns, dns_rdatatype_none,
				      NULL, &nsrdataset);
	if (result != ISC_R_SUCCESS)
		goto fail;

	/*
	 * Add NS rdataset.
	 */
	result = dns_db_findnode(db, name, ISC_TRUE, &node);
	if (result != ISC_R_SUCCESS)
		goto fail;
	result = dns_db_addrdataset(db, node, version, 0,
				    nsrdataset, 0, NULL);
	dns_db_detachnode(db, &node);
	if (result != ISC_R_SUCCESS)
		goto fail;
	/*
	 * Add glue rdatasets.
	 */
	for (result = dns_rdataset_first(nsrdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(nsrdataset)) {
		dns_rdataset_current(nsrdataset, &rdata);
		result = dns_rdata_tostruct(&rdata, &ns, NULL);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);
		dns_rdata_reset(&rdata);
		if (!dns_name_issubdomain(&ns.name, name))
			continue;
		rdataset = NULL;
		result = dns_message_findname(message, DNS_SECTION_ADDITIONAL,
					      &ns.name, dns_rdatatype_aaaa,
					      dns_rdatatype_none, NULL,
					      &rdataset);
		if (result == ISC_R_SUCCESS) {
			result = dns_db_findnode(db, &ns.name,
						 ISC_TRUE, &node);
			if (result != ISC_R_SUCCESS)
				goto fail;
			result = dns_db_addrdataset(db, node, version, 0,
						    rdataset, 0, NULL);
			dns_db_detachnode(db, &node);
			if (result != ISC_R_SUCCESS)
				goto fail;
		}
		rdataset = NULL;
		result = dns_message_findname(message, DNS_SECTION_ADDITIONAL,
					      &ns.name, dns_rdatatype_a,
					      dns_rdatatype_none, NULL,
					      &rdataset);
		if (result == ISC_R_SUCCESS) {
			result = dns_db_findnode(db, &ns.name,
						 ISC_TRUE, &node);
			if (result != ISC_R_SUCCESS)
				goto fail;
			result = dns_db_addrdataset(db, node, version, 0,
						    rdataset, 0, NULL);
			dns_db_detachnode(db, &node);
			if (result != ISC_R_SUCCESS)
				goto fail;
		}
	}
	if (result != ISC_R_NOMORE)
		goto fail;

	return (ISC_R_SUCCESS);

fail:
	return (result);
}

static void
stub_callback(isc_task_t *task, isc_event_t *event) {
	const char me[] = "stub_callback";
	dns_requestevent_t *revent = (dns_requestevent_t *)event;
	dns_stub_t *stub = NULL;
	dns_message_t *msg = NULL;
	dns_zone_t *zone = NULL;
	char master[ISC_SOCKADDR_FORMATSIZE];
	char source[ISC_SOCKADDR_FORMATSIZE];
	isc_uint32_t nscnt, cnamecnt;
	isc_result_t result;
	isc_time_t now;
	isc_boolean_t exiting = ISC_FALSE;
	isc_interval_t i;
	unsigned int j;

	stub = revent->ev_arg;
	INSIST(DNS_STUB_VALID(stub));

	UNUSED(task);

	zone = stub->zone;

	ENTER;

	TIME_NOW(&now);

	if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_EXITING)) {
		zone_debuglog(zone, me, 1, "exiting");
		exiting = ISC_TRUE;
		goto next_master;
	}

	isc_sockaddr_format(&zone->masteraddr, master, sizeof(master));
	isc_sockaddr_format(&zone->sourceaddr, source, sizeof(source));

	if (revent->result != ISC_R_SUCCESS) {
		if (revent->result == ISC_R_TIMEDOUT &&
		    !DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NOEDNS)) {
			LOCK_ZONE(zone);
			DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_NOEDNS);
			UNLOCK_ZONE(zone);
			dns_zone_log(zone, ISC_LOG_DEBUG(1),
				     "refreshing stub: timeout retrying "
				     " without EDNS master %s (source %s)",
				     master, source);
			goto same_master;
		}
		dns_zone_log(zone, ISC_LOG_INFO,
			     "could not refresh stub from master %s"
			     " (source %s): %s", master, source,
			     dns_result_totext(revent->result));
		goto next_master;
	}

	result = dns_message_create(zone->mctx, DNS_MESSAGE_INTENTPARSE, &msg);
	if (result != ISC_R_SUCCESS)
		goto next_master;

	result = dns_request_getresponse(revent->request, msg, 0);
	if (result != ISC_R_SUCCESS)
		goto next_master;

	/*
	 * Unexpected rcode.
	 */
	if (msg->rcode != dns_rcode_noerror) {
		char rcode[128];
		isc_buffer_t rb;

		isc_buffer_init(&rb, rcode, sizeof(rcode));
		(void)dns_rcode_totext(msg->rcode, &rb);

		if (!DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NOEDNS) &&
		    (msg->rcode == dns_rcode_servfail ||
		     msg->rcode == dns_rcode_notimp ||
		     msg->rcode == dns_rcode_formerr)) {
			dns_zone_log(zone, ISC_LOG_DEBUG(1),
				     "refreshing stub: rcode (%.*s) retrying "
				     "without EDNS master %s (source %s)",
				     (int)rb.used, rcode, master, source);
			LOCK_ZONE(zone);
			DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_NOEDNS);
			UNLOCK_ZONE(zone);
			goto same_master;
		}

		dns_zone_log(zone, ISC_LOG_INFO,
			     "refreshing stub: "
			     "unexpected rcode (%.*s) from %s (source %s)",
			     (int)rb.used, rcode, master, source);
		goto next_master;
	}

	/*
	 * We need complete messages.
	 */
	if ((msg->flags & DNS_MESSAGEFLAG_TC) != 0) {
		if (dns_request_usedtcp(revent->request)) {
			dns_zone_log(zone, ISC_LOG_INFO,
				     "refreshing stub: truncated TCP "
				     "response from master %s (source %s)",
				     master, source);
			goto next_master;
		}
		LOCK_ZONE(zone);
		DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_USEVC);
		UNLOCK_ZONE(zone);
		goto same_master;
	}

	/*
	 * If non-auth log and next master.
	 */
	if ((msg->flags & DNS_MESSAGEFLAG_AA) == 0) {
		dns_zone_log(zone, ISC_LOG_INFO, "refreshing stub: "
			     "non-authoritative answer from "
			     "master %s (source %s)", master, source);
		goto next_master;
	}

	/*
	 * Sanity checks.
	 */
	cnamecnt = message_count(msg, DNS_SECTION_ANSWER, dns_rdatatype_cname);
	nscnt = message_count(msg, DNS_SECTION_ANSWER, dns_rdatatype_ns);

	if (cnamecnt != 0) {
		dns_zone_log(zone, ISC_LOG_INFO,
			     "refreshing stub: unexpected CNAME response "
			     "from master %s (source %s)", master, source);
		goto next_master;
	}

	if (nscnt == 0) {
		dns_zone_log(zone, ISC_LOG_INFO,
			     "refreshing stub: no NS records in response "
			     "from master %s (source %s)", master, source);
		goto next_master;
	}

	/*
	 * Save answer.
	 */
	result = save_nsrrset(msg, &zone->origin, stub->db, stub->version);
	if (result != ISC_R_SUCCESS) {
		dns_zone_log(zone, ISC_LOG_INFO,
			     "refreshing stub: unable to save NS records "
			     "from master %s (source %s)", master, source);
		goto next_master;
	}

	/*
	 * Tidy up.
	 */
	dns_db_closeversion(stub->db, &stub->version, ISC_TRUE);
	ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_write);
	if (zone->db == NULL)
		zone_attachdb(zone, stub->db);
	ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_write);
	dns_db_detach(&stub->db);

	if (zone->masterfile != NULL) {
		dns_zone_dump(zone);
		TIME_NOW(&zone->loadtime);
	}

	dns_message_destroy(&msg);
	isc_event_free(&event);
	LOCK_ZONE(zone);
	dns_request_destroy(&zone->request);
	DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_REFRESH);
	DNS_ZONE_JITTER_ADD(&now, zone->refresh, &zone->refreshtime);
	isc_interval_set(&i, zone->expire, 0);
	DNS_ZONE_TIME_ADD(&now, zone->expire, &zone->expiretime);
	zone_settimer(zone, &now);
	UNLOCK_ZONE(zone);
	goto free_stub;

 next_master:
	if (stub->version != NULL)
		dns_db_closeversion(stub->db, &stub->version, ISC_FALSE);
	if (stub->db != NULL)
		dns_db_detach(&stub->db);
	if (msg != NULL)
		dns_message_destroy(&msg);
	isc_event_free(&event);
	LOCK_ZONE(zone);
	dns_request_destroy(&zone->request);
	/*
	 * Skip to next failed / untried master.
	 */
	do {
		zone->curmaster++;
	} while (zone->curmaster < zone->masterscnt &&
		 zone->mastersok[zone->curmaster]);
	DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_NOEDNS);
	if (exiting || zone->curmaster >= zone->masterscnt) {
		isc_boolean_t done = ISC_TRUE;
		if (!exiting &&
		    DNS_ZONE_OPTION(zone, DNS_ZONEOPT_USEALTXFRSRC) &&
		    !DNS_ZONE_FLAG(zone, DNS_ZONEFLG_USEALTXFRSRC)) {
			/*
			 * Did we get a good answer from all the masters?
			 */
			for (j = 0; j < zone->masterscnt; j++)
				if (zone->mastersok[j] == ISC_FALSE) {
					done = ISC_FALSE;
					break;
				}
		} else
			done = ISC_TRUE;
		if (!done) {
			zone->curmaster = 0;
			/*
			 * Find the next failed master.
			 */
			while (zone->curmaster < zone->masterscnt &&
			       zone->mastersok[zone->curmaster])
				zone->curmaster++;
			DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_USEALTXFRSRC);
		} else {
			DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_REFRESH);

			zone_settimer(zone, &now);
			UNLOCK_ZONE(zone);
			goto free_stub;
		}
	}
	queue_soa_query(zone);
	UNLOCK_ZONE(zone);
	goto free_stub;

 same_master:
	if (msg != NULL)
		dns_message_destroy(&msg);
	isc_event_free(&event);
	LOCK_ZONE(zone);
	dns_request_destroy(&zone->request);
	UNLOCK_ZONE(zone);
	ns_query(zone, NULL, stub);
	goto done;

 free_stub:
	stub->magic = 0;
	dns_zone_idetach(&stub->zone);
	INSIST(stub->db == NULL);
	INSIST(stub->version == NULL);
	isc_mem_put(stub->mctx, stub, sizeof(*stub));

 done:
	INSIST(event == NULL);
	return;
}

/*
 * An SOA query has finished (successfully or not).
 */
static void
refresh_callback(isc_task_t *task, isc_event_t *event) {
	const char me[] = "refresh_callback";
	dns_requestevent_t *revent = (dns_requestevent_t *)event;
	dns_zone_t *zone;
	dns_message_t *msg = NULL;
	isc_uint32_t soacnt, cnamecnt, soacount, nscount;
	isc_time_t now;
	char master[ISC_SOCKADDR_FORMATSIZE];
	char source[ISC_SOCKADDR_FORMATSIZE];
	dns_rdataset_t *rdataset = NULL;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdata_soa_t soa;
	isc_result_t result;
	isc_uint32_t serial;
	unsigned int j;

	zone = revent->ev_arg;
	INSIST(DNS_ZONE_VALID(zone));

	UNUSED(task);

	ENTER;

	/*
	 * if timeout log and next master;
	 */

	isc_sockaddr_format(&zone->masteraddr, master, sizeof(master));
	isc_sockaddr_format(&zone->sourceaddr, source, sizeof(source));

	TIME_NOW(&now);

	if (revent->result != ISC_R_SUCCESS) {
		if (revent->result == ISC_R_TIMEDOUT &&
		    !DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NOEDNS)) {
			LOCK_ZONE(zone);
			DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_NOEDNS);
			UNLOCK_ZONE(zone);
			dns_zone_log(zone, ISC_LOG_DEBUG(1),
				     "refresh: timeout retrying without EDNS "
				     "master %s (source %s)", master, source);
			goto same_master;
		}
		if (revent->result == ISC_R_TIMEDOUT &&
		    !dns_request_usedtcp(revent->request)) {
			dns_zone_log(zone, ISC_LOG_INFO,
				     "refresh: retry limit for "
				     "master %s exceeded (source %s)",
				     master, source);
			/* Try with slave with TCP. */
			if (zone->type == dns_zone_slave) {
				LOCK_ZONE(zone);
				DNS_ZONE_SETFLAG(zone,
						 DNS_ZONEFLG_SOABEFOREAXFR);
				UNLOCK_ZONE(zone);
				goto tcp_transfer;
			}
		} else
			dns_zone_log(zone, ISC_LOG_INFO,
				     "refresh: failure trying master "
				     "%s (source %s): %s", master, source,
				     dns_result_totext(revent->result));
		goto next_master;
	}

	result = dns_message_create(zone->mctx, DNS_MESSAGE_INTENTPARSE, &msg);
	if (result != ISC_R_SUCCESS)
		goto next_master;
	result = dns_request_getresponse(revent->request, msg, 0);
	if (result != ISC_R_SUCCESS) {
		dns_zone_log(zone, ISC_LOG_INFO,
			     "refresh: failure trying master "
			     "%s (source %s): %s", master, source,
			     dns_result_totext(result));
		goto next_master;
	}

	/*
	 * Unexpected rcode.
	 */
	if (msg->rcode != dns_rcode_noerror) {
		char rcode[128];
		isc_buffer_t rb;

		isc_buffer_init(&rb, rcode, sizeof(rcode));
		(void)dns_rcode_totext(msg->rcode, &rb);

		if (!DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NOEDNS) &&
		    (msg->rcode == dns_rcode_servfail ||
		     msg->rcode == dns_rcode_notimp ||
		     msg->rcode == dns_rcode_formerr)) {
			dns_zone_log(zone, ISC_LOG_DEBUG(1),
				     "refresh: rcode (%.*s) retrying without "
				     "EDNS master %s (source %s)",
				     (int)rb.used, rcode, master, source);
			LOCK_ZONE(zone);
			DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_NOEDNS);
			UNLOCK_ZONE(zone);
			goto same_master;
		}
		dns_zone_log(zone, ISC_LOG_INFO,
			     "refresh: unexpected rcode (%.*s) from "
			     "master %s (source %s)", (int)rb.used, rcode,
			     master, source);
		/*
		 * Perhaps AXFR/IXFR is allowed even if SOA queries aren't.
		 */
		if (msg->rcode == dns_rcode_refused &&
		    zone->type == dns_zone_slave)
			goto tcp_transfer;
		goto next_master;
	}

	/*
	 * If truncated punt to zone transfer which will query again.
	 */
	if ((msg->flags & DNS_MESSAGEFLAG_TC) != 0) {
		if (zone->type == dns_zone_slave) {
			dns_zone_log(zone, ISC_LOG_INFO,
				     "refresh: truncated UDP answer, "
				     "initiating TCP zone xfer "
				     "for master %s (source %s)",
				     master, source);
			LOCK_ZONE(zone);
			DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_SOABEFOREAXFR);
			UNLOCK_ZONE(zone);
			goto tcp_transfer;
		} else {
			INSIST(zone->type == dns_zone_stub);
			if (dns_request_usedtcp(revent->request)) {
				dns_zone_log(zone, ISC_LOG_INFO,
					     "refresh: truncated TCP response "
					     "from master %s (source %s)",
					     master, source);
				goto next_master;
			}
			LOCK_ZONE(zone);
			DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_USEVC);
			UNLOCK_ZONE(zone);
			goto same_master;
		}
	}

	/*
	 * if non-auth log and next master;
	 */
	if ((msg->flags & DNS_MESSAGEFLAG_AA) == 0) {
		dns_zone_log(zone, ISC_LOG_INFO,
			     "refresh: non-authoritative answer from "
			     "master %s (source %s)", master, source);
		goto next_master;
	}

	cnamecnt = message_count(msg, DNS_SECTION_ANSWER, dns_rdatatype_cname);
	soacnt = message_count(msg, DNS_SECTION_ANSWER, dns_rdatatype_soa);
	nscount = message_count(msg, DNS_SECTION_AUTHORITY, dns_rdatatype_ns);
	soacount = message_count(msg, DNS_SECTION_AUTHORITY,
				 dns_rdatatype_soa);

	/*
	 * There should not be a CNAME record at top of zone.
	 */
	if (cnamecnt != 0) {
		dns_zone_log(zone, ISC_LOG_INFO,
			     "refresh: CNAME at top of zone "
			     "in master %s (source %s)", master, source);
		goto next_master;
	}

	/*
	 * if referral log and next master;
	 */
	if (soacnt == 0 && soacount == 0 && nscount != 0) {
		dns_zone_log(zone, ISC_LOG_INFO,
			     "refresh: referral response "
			     "from master %s (source %s)", master, source);
		goto next_master;
	}

	/*
	 * if nodata log and next master;
	 */
	if (soacnt == 0 && (nscount == 0 || soacount != 0)) {
		dns_zone_log(zone, ISC_LOG_INFO,
			     "refresh: NODATA response "
			     "from master %s (source %s)", master, source);
		goto next_master;
	}

	/*
	 * Only one soa at top of zone.
	 */
	if (soacnt != 1) {
		dns_zone_log(zone, ISC_LOG_INFO,
			     "refresh: answer SOA count (%d) != 1 "
			     "from master %s (source %s)",
			     soacnt, master, source);
		goto next_master;
	}
	/*
	 * Extract serial
	 */
	rdataset = NULL;
	result = dns_message_findname(msg, DNS_SECTION_ANSWER, &zone->origin,
				      dns_rdatatype_soa, dns_rdatatype_none,
				      NULL, &rdataset);
	if (result != ISC_R_SUCCESS) {
		dns_zone_log(zone, ISC_LOG_INFO,
			     "refresh: unable to get SOA record "
			     "from master %s (source %s)", master, source);
		goto next_master;
	}

	result = dns_rdataset_first(rdataset);
	if (result != ISC_R_SUCCESS) {
		dns_zone_log(zone, ISC_LOG_INFO,
			     "refresh: dns_rdataset_first() failed");
		goto next_master;
	}

	dns_rdataset_current(rdataset, &rdata);
	result = dns_rdata_tostruct(&rdata, &soa, NULL);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);

	serial = soa.serial;

	zone_debuglog(zone, me, 1, "serial: new %u, old %u",
		      serial, zone->serial);
	if (!DNS_ZONE_FLAG(zone, DNS_ZONEFLG_LOADED) ||
	    DNS_ZONE_FLAG(zone, DNS_ZONEFLG_FORCEXFER) ||
	    isc_serial_gt(serial, zone->serial)) {
 tcp_transfer:
		isc_event_free(&event);
		LOCK_ZONE(zone);
		dns_request_destroy(&zone->request);
		UNLOCK_ZONE(zone);
		if (zone->type == dns_zone_slave) {
			queue_xfrin(zone);
		} else {
			INSIST(zone->type == dns_zone_stub);
			ns_query(zone, rdataset, NULL);
		}
		if (msg != NULL)
			dns_message_destroy(&msg);
	} else if (isc_serial_eq(soa.serial, zone->serial)) {
		if (zone->masterfile != NULL) {
			result = ISC_R_FAILURE;
			if (zone->journal != NULL)
				result = isc_file_settime(zone->journal, &now);
			if (result == ISC_R_SUCCESS &&
			    !DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NEEDDUMP) &&
			    !DNS_ZONE_FLAG(zone, DNS_ZONEFLG_DUMPING)) {
				result = isc_file_settime(zone->masterfile,
							  &now);
			} else if (result != ISC_R_SUCCESS)
				result = isc_file_settime(zone->masterfile,
							  &now);
			/* Someone removed the file from underneath us! */
			if (result == ISC_R_FILENOTFOUND) {
				LOCK_ZONE(zone);
				zone_needdump(zone, DNS_DUMP_DELAY);
				UNLOCK_ZONE(zone);
			} else if (result != ISC_R_SUCCESS)
				dns_zone_log(zone, ISC_LOG_ERROR,
					     "refresh: could not set file "
					     "modification time of '%s': %s",
					     zone->masterfile,
					     dns_result_totext(result));
		}
		DNS_ZONE_JITTER_ADD(&now, zone->refresh, &zone->refreshtime);
		DNS_ZONE_TIME_ADD(&now, zone->expire, &zone->expiretime);
		zone->mastersok[zone->curmaster] = ISC_TRUE;
		goto next_master;
	} else {
		if (!DNS_ZONE_OPTION(zone, DNS_ZONEOPT_MULTIMASTER))
			dns_zone_log(zone, ISC_LOG_INFO, "serial number (%u) "
				     "received from master %s < ours (%u)",
				     soa.serial, master, zone->serial);
		else
			zone_debuglog(zone, me, 1, "ahead");
		zone->mastersok[zone->curmaster] = ISC_TRUE;
		goto next_master;
	}
	if (msg != NULL)
		dns_message_destroy(&msg);
	goto detach;

 next_master:
	if (msg != NULL)
		dns_message_destroy(&msg);
	isc_event_free(&event);
	LOCK_ZONE(zone);
	dns_request_destroy(&zone->request);
	/*
	 * Skip to next failed / untried master.
	 */
	do {
		zone->curmaster++;
	} while (zone->curmaster < zone->masterscnt &&
		 zone->mastersok[zone->curmaster]);
	DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_NOEDNS);
	if (zone->curmaster >= zone->masterscnt) {
		isc_boolean_t done = ISC_TRUE;
		if (DNS_ZONE_OPTION(zone, DNS_ZONEOPT_USEALTXFRSRC) &&
		    !DNS_ZONE_FLAG(zone, DNS_ZONEFLG_USEALTXFRSRC)) {
			/*
			 * Did we get a good answer from all the masters?
			 */
			for (j = 0; j < zone->masterscnt; j++)
				if (zone->mastersok[j] == ISC_FALSE) {
					done = ISC_FALSE;
					break;
				}
		} else
			done = ISC_TRUE;
		if (!done) {
			DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_USEALTXFRSRC);
			zone->curmaster = 0;
			/*
			 * Find the next failed master.
			 */
			while (zone->curmaster < zone->masterscnt &&
			       zone->mastersok[zone->curmaster])
				zone->curmaster++;
			goto requeue;
		}
		DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_REFRESH);
		if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NEEDREFRESH)) {
			DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_NEEDREFRESH);
			zone->refreshtime = now;
		}
		DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_USEALTXFRSRC);
		zone_settimer(zone, &now);
		UNLOCK_ZONE(zone);
		goto detach;
	}

 requeue:
	queue_soa_query(zone);
	UNLOCK_ZONE(zone);
	goto detach;

 same_master:
	if (msg != NULL)
		dns_message_destroy(&msg);
	isc_event_free(&event);
	LOCK_ZONE(zone);
	dns_request_destroy(&zone->request);
	queue_soa_query(zone);
	UNLOCK_ZONE(zone);

 detach:
	dns_zone_idetach(&zone);
	return;
}

static void
queue_soa_query(dns_zone_t *zone) {
	const char me[] = "queue_soa_query";
	isc_event_t *e;
	dns_zone_t *dummy = NULL;
	isc_result_t result;

	ENTER;
	/*
	 * Locked by caller
	 */
	REQUIRE(LOCKED_ZONE(zone));

	if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_EXITING)) {
		cancel_refresh(zone);
		return;
	}

	e = isc_event_allocate(zone->mctx, NULL, DNS_EVENT_ZONE,
			       soa_query, zone, sizeof(isc_event_t));
	if (e == NULL) {
		cancel_refresh(zone);
		return;
	}

	/*
	 * Attach so that we won't clean up
	 * until the event is delivered.
	 */
	zone_iattach(zone, &dummy);

	e->ev_arg = zone;
	e->ev_sender = NULL;
	result = isc_ratelimiter_enqueue(zone->zmgr->rl, zone->task, &e);
	if (result != ISC_R_SUCCESS) {
		zone_idetach(&dummy);
		isc_event_free(&e);
		cancel_refresh(zone);
	}
}

static inline isc_result_t
create_query(dns_zone_t *zone, dns_rdatatype_t rdtype,
	     dns_message_t **messagep)
{
	dns_message_t *message = NULL;
	dns_name_t *qname = NULL;
	dns_rdataset_t *qrdataset = NULL;
	isc_result_t result;

	result = dns_message_create(zone->mctx, DNS_MESSAGE_INTENTRENDER,
				    &message);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	message->opcode = dns_opcode_query;
	message->rdclass = zone->rdclass;

	result = dns_message_gettempname(message, &qname);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	result = dns_message_gettemprdataset(message, &qrdataset);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	/*
	 * Make question.
	 */
	dns_name_init(qname, NULL);
	dns_name_clone(&zone->origin, qname);
	dns_rdataset_init(qrdataset);
	dns_rdataset_makequestion(qrdataset, zone->rdclass, rdtype);
	ISC_LIST_APPEND(qname->list, qrdataset, link);
	dns_message_addname(message, qname, DNS_SECTION_QUESTION);

	*messagep = message;
	return (ISC_R_SUCCESS);

 cleanup:
	if (qname != NULL)
		dns_message_puttempname(message, &qname);
	if (qrdataset != NULL)
		dns_message_puttemprdataset(message, &qrdataset);
	if (message != NULL)
		dns_message_destroy(&message);
	return (result);
}

static isc_result_t
add_opt(dns_message_t *message, isc_uint16_t udpsize) {
	dns_rdataset_t *rdataset = NULL;
	dns_rdatalist_t *rdatalist = NULL;
	dns_rdata_t *rdata = NULL;
	isc_result_t result;

	result = dns_message_gettemprdatalist(message, &rdatalist);
	if (result != ISC_R_SUCCESS)
		goto cleanup;
	result = dns_message_gettemprdata(message, &rdata);
	if (result != ISC_R_SUCCESS)
		goto cleanup;
	result = dns_message_gettemprdataset(message, &rdataset);
	if (result != ISC_R_SUCCESS)
		goto cleanup;
	dns_rdataset_init(rdataset);

	rdatalist->type = dns_rdatatype_opt;
	rdatalist->covers = 0;

	/*
	 * Set Maximum UDP buffer size.
	 */
	rdatalist->rdclass = udpsize;

	/*
	 * Set EXTENDED-RCODE, VERSION, DO and Z to 0.
	 */
	rdatalist->ttl = 0;

	/*
	 * No EDNS options.
	 */
	rdata->data = NULL;
	rdata->length = 0;
	rdata->rdclass = rdatalist->rdclass;
	rdata->type = rdatalist->type;
	rdata->flags = 0;

	ISC_LIST_INIT(rdatalist->rdata);
	ISC_LIST_APPEND(rdatalist->rdata, rdata, link);
	RUNTIME_CHECK(dns_rdatalist_tordataset(rdatalist, rdataset)
		      == ISC_R_SUCCESS);

	return (dns_message_setopt(message, rdataset));

 cleanup:
	if (rdatalist != NULL)
		dns_message_puttemprdatalist(message, &rdatalist);
	if (rdataset != NULL)
		dns_message_puttemprdataset(message, &rdataset);
	if (rdata != NULL)
		dns_message_puttemprdata(message, &rdata);

	return (result);
}

static void
soa_query(isc_task_t *task, isc_event_t *event) {
	const char me[] = "soa_query";
	isc_result_t result = ISC_R_FAILURE;
	dns_message_t *message = NULL;
	dns_zone_t *zone = event->ev_arg;
	dns_zone_t *dummy = NULL;
	isc_netaddr_t masterip;
	dns_tsigkey_t *key = NULL;
	isc_uint32_t options;
	isc_boolean_t cancel = ISC_TRUE;
	int timeout;
	isc_boolean_t have_xfrsource;
	isc_uint16_t udpsize = SEND_BUFFER_SIZE;

	REQUIRE(DNS_ZONE_VALID(zone));

	UNUSED(task);

	ENTER;

	LOCK_ZONE(zone);
	if (((event->ev_attributes & ISC_EVENTATTR_CANCELED) != 0) ||
	    DNS_ZONE_FLAG(zone, DNS_ZONEFLG_EXITING) ||
	    zone->view->requestmgr == NULL) {
		if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_EXITING))
			cancel = ISC_FALSE;
		goto cleanup;
	}

	/*
	 * XXX Optimisation: Create message when zone is setup and reuse.
	 */
	result = create_query(zone, dns_rdatatype_soa, &message);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

 again:
	INSIST(zone->masterscnt > 0);
	INSIST(zone->curmaster < zone->masterscnt);

	zone->masteraddr = zone->masters[zone->curmaster];

	isc_netaddr_fromsockaddr(&masterip, &zone->masteraddr);
	/*
	 * First, look for a tsig key in the master statement, then
	 * try for a server key.
	 */
	if ((zone->masterkeynames != NULL) &&
	    (zone->masterkeynames[zone->curmaster] != NULL)) {
		dns_view_t *view = dns_zone_getview(zone);
		dns_name_t *keyname = zone->masterkeynames[zone->curmaster];
		result = dns_view_gettsig(view, keyname, &key);
		if (result != ISC_R_SUCCESS) {
			char namebuf[DNS_NAME_FORMATSIZE];
			dns_name_format(keyname, namebuf, sizeof(namebuf));
			dns_zone_log(zone, ISC_LOG_ERROR,
				     "unable to find key: %s", namebuf);
		}
	}
	if (key == NULL)
		(void)dns_view_getpeertsig(zone->view, &masterip, &key);

	have_xfrsource = ISC_FALSE;
	if (zone->view->peers != NULL) {
		dns_peer_t *peer = NULL;
		isc_boolean_t edns;
		result = dns_peerlist_peerbyaddr(zone->view->peers,
						 &masterip, &peer);
		if (result == ISC_R_SUCCESS) {
			result = dns_peer_getsupportedns(peer, &edns);
			if (result == ISC_R_SUCCESS && !edns)
				DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_NOEDNS);
			result = dns_peer_gettransfersource(peer,
							    &zone->sourceaddr);
			if (result == ISC_R_SUCCESS)
				have_xfrsource = ISC_TRUE;
			if (zone->view->resolver != NULL)
				udpsize =
				  dns_resolver_getudpsize(zone->view->resolver);
			(void)dns_peer_getudpsize(peer, &udpsize);
		}
	}

	switch (isc_sockaddr_pf(&zone->masteraddr)) {
	case PF_INET:
		if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_USEALTXFRSRC)) {
			if (isc_sockaddr_equal(&zone->altxfrsource4,
					       &zone->xfrsource4))
				goto skip_master;
			zone->sourceaddr = zone->altxfrsource4;
		} else if (!have_xfrsource)
			zone->sourceaddr = zone->xfrsource4;
		break;
	case PF_INET6:
		if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_USEALTXFRSRC)) {
			if (isc_sockaddr_equal(&zone->altxfrsource6,
					       &zone->xfrsource6))
				goto skip_master;
			zone->sourceaddr = zone->altxfrsource6;
		} else if (!have_xfrsource)
			zone->sourceaddr = zone->xfrsource6;
		break;
	default:
		result = ISC_R_NOTIMPLEMENTED;
		goto cleanup;
	}

	options = DNS_ZONE_FLAG(zone, DNS_ZONEFLG_USEVC) ?
		  DNS_REQUESTOPT_TCP : 0;

	if (!DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NOEDNS)) {
		result = add_opt(message, udpsize);
		if (result != ISC_R_SUCCESS)
			zone_debuglog(zone, me, 1,
				      "unable to add opt record: %s",
				      dns_result_totext(result));
	}

	zone_iattach(zone, &dummy);
	timeout = 15;
	if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_DIALREFRESH))
		timeout = 30;
	result = dns_request_createvia2(zone->view->requestmgr, message,
					&zone->sourceaddr, &zone->masteraddr,
					options, key, timeout * 3, timeout,
					zone->task, refresh_callback, zone,
					&zone->request);
	if (result != ISC_R_SUCCESS) {
		zone_idetach(&dummy);
		zone_debuglog(zone, me, 1,
			      "dns_request_createvia2() failed: %s",
			      dns_result_totext(result));
		goto cleanup;
	}
	cancel = ISC_FALSE;

 cleanup:
	if (key != NULL)
		dns_tsigkey_detach(&key);
	if (result != ISC_R_SUCCESS)
		DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_REFRESH);
	if (message != NULL)
		dns_message_destroy(&message);
	if (cancel)
		cancel_refresh(zone);
	isc_event_free(&event);
	UNLOCK_ZONE(zone);
	dns_zone_idetach(&zone);
	return;

 skip_master:
	if (key != NULL)
		dns_tsigkey_detach(&key);
	/*
	 * Skip to next failed / untried master.
	 */
	do {
		zone->curmaster++;
	} while (zone->curmaster < zone->masterscnt &&
		 zone->mastersok[zone->curmaster]);
	if (zone->curmaster < zone->masterscnt)
		goto again;
	zone->curmaster = 0;
	goto cleanup;
}

static void
ns_query(dns_zone_t *zone, dns_rdataset_t *soardataset, dns_stub_t *stub) {
	const char me[] = "ns_query";
	isc_result_t result;
	dns_message_t *message = NULL;
	isc_netaddr_t masterip;
	dns_tsigkey_t *key = NULL;
	dns_dbnode_t *node = NULL;
	int timeout;
	isc_boolean_t have_xfrsource = ISC_FALSE;
	isc_uint16_t udpsize = SEND_BUFFER_SIZE;

	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE((soardataset != NULL && stub == NULL) ||
		(soardataset == NULL && stub != NULL));
	REQUIRE(stub == NULL || DNS_STUB_VALID(stub));

	ENTER;

	LOCK_ZONE(zone);
	if (stub == NULL) {
		stub = isc_mem_get(zone->mctx, sizeof(*stub));
		if (stub == NULL)
			goto cleanup;
		stub->magic = STUB_MAGIC;
		stub->mctx = zone->mctx;
		stub->zone = NULL;
		stub->db = NULL;
		stub->version = NULL;

		/*
		 * Attach so that the zone won't disappear from under us.
		 */
		zone_iattach(zone, &stub->zone);

		/*
		 * If a db exists we will update it, otherwise we create a
		 * new one and attach it to the zone once we have the NS
		 * RRset and glue.
		 */
		ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_read);
		if (zone->db != NULL) {
			dns_db_attach(zone->db, &stub->db);
			ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);
		} else {
			ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);

			INSIST(zone->db_argc >= 1);
			result = dns_db_create(zone->mctx, zone->db_argv[0],
					       &zone->origin, dns_dbtype_stub,
					       zone->rdclass,
					       zone->db_argc - 1,
					       zone->db_argv + 1,
					       &stub->db);
			if (result != ISC_R_SUCCESS) {
				dns_zone_log(zone, ISC_LOG_ERROR,
					     "refreshing stub: "
					     "could not create "
					     "database: %s",
					     dns_result_totext(result));
				goto cleanup;
			}
			dns_db_settask(stub->db, zone->task);
		}

		dns_db_newversion(stub->db, &stub->version);

		/*
		 * Update SOA record.
		 */
		result = dns_db_findnode(stub->db, &zone->origin, ISC_TRUE,
					 &node);
		if (result != ISC_R_SUCCESS) {
			dns_zone_log(zone, ISC_LOG_INFO,
				     "refreshing stub: "
				     "dns_db_findnode() failed: %s",
				     dns_result_totext(result));
			goto cleanup;
		}

		result = dns_db_addrdataset(stub->db, node, stub->version, 0,
					    soardataset, 0, NULL);
		dns_db_detachnode(stub->db, &node);
		if (result != ISC_R_SUCCESS) {
			dns_zone_log(zone, ISC_LOG_INFO,
				     "refreshing stub: "
				     "dns_db_addrdataset() failed: %s",
				     dns_result_totext(result));
			goto cleanup;
		}
	}

	/*
	 * XXX Optimisation: Create message when zone is setup and reuse.
	 */
	result = create_query(zone, dns_rdatatype_ns, &message);

	INSIST(zone->masterscnt > 0);
	INSIST(zone->curmaster < zone->masterscnt);
	zone->masteraddr = zone->masters[zone->curmaster];

	isc_netaddr_fromsockaddr(&masterip, &zone->masteraddr);
	/*
	 * First, look for a tsig key in the master statement, then
	 * try for a server key.
	 */
	if ((zone->masterkeynames != NULL) &&
	    (zone->masterkeynames[zone->curmaster] != NULL)) {
		dns_view_t *view = dns_zone_getview(zone);
		dns_name_t *keyname = zone->masterkeynames[zone->curmaster];
		result = dns_view_gettsig(view, keyname, &key);
		if (result != ISC_R_SUCCESS) {
			char namebuf[DNS_NAME_FORMATSIZE];
			dns_name_format(keyname, namebuf, sizeof(namebuf));
			dns_zone_log(zone, ISC_LOG_ERROR,
				     "unable to find key: %s", namebuf);
		}
	}
	if (key == NULL)
		(void)dns_view_getpeertsig(zone->view, &masterip, &key);

	if (zone->view->peers != NULL) {
		dns_peer_t *peer = NULL;
		isc_boolean_t edns;
		result = dns_peerlist_peerbyaddr(zone->view->peers,
						 &masterip, &peer);
		if (result == ISC_R_SUCCESS) {
			result = dns_peer_getsupportedns(peer, &edns);
			if (result == ISC_R_SUCCESS && !edns)
				DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_NOEDNS);
			result = dns_peer_gettransfersource(peer,
							    &zone->sourceaddr);
			if (result == ISC_R_SUCCESS)
				have_xfrsource = ISC_TRUE;
			if (zone->view->resolver != NULL)
				udpsize =
				  dns_resolver_getudpsize(zone->view->resolver);
			(void)dns_peer_getudpsize(peer, &udpsize);
		}

	}
	if (!DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NOEDNS)) {
		result = add_opt(message, udpsize);
		if (result != ISC_R_SUCCESS)
			zone_debuglog(zone, me, 1,
				      "unable to add opt record: %s",
				      dns_result_totext(result));
	}

	/*
	 * Always use TCP so that we shouldn't truncate in additional section.
	 */
	switch (isc_sockaddr_pf(&zone->masteraddr)) {
	case PF_INET:
		if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_USEALTXFRSRC))
			zone->sourceaddr = zone->altxfrsource4;
		else if (!have_xfrsource)
			zone->sourceaddr = zone->xfrsource4;
		break;
	case PF_INET6:
		if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_USEALTXFRSRC))
			zone->sourceaddr = zone->altxfrsource6;
		else if (!have_xfrsource)
			zone->sourceaddr = zone->xfrsource6;
		break;
	default:
		result = ISC_R_NOTIMPLEMENTED;
		goto cleanup;
	}
	timeout = 15;
	if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_DIALREFRESH))
		timeout = 30;
	result = dns_request_createvia2(zone->view->requestmgr, message,
					&zone->sourceaddr, &zone->masteraddr,
					DNS_REQUESTOPT_TCP, key, timeout * 3,
					timeout, zone->task, stub_callback,
					stub, &zone->request);
	if (result != ISC_R_SUCCESS) {
		zone_debuglog(zone, me, 1,
			      "dns_request_createvia() failed: %s",
			      dns_result_totext(result));
		goto cleanup;
	}
	dns_message_destroy(&message);
	goto unlock;

 cleanup:
	cancel_refresh(zone);
	if (stub != NULL) {
		stub->magic = 0;
		if (stub->version != NULL)
			dns_db_closeversion(stub->db, &stub->version,
					    ISC_FALSE);
		if (stub->db != NULL)
			dns_db_detach(&stub->db);
		if (stub->zone != NULL)
			zone_idetach(&stub->zone);
		isc_mem_put(stub->mctx, stub, sizeof(*stub));
	}
	if (message != NULL)
		dns_message_destroy(&message);
  unlock:
	if (key != NULL)
		dns_tsigkey_detach(&key);
	UNLOCK_ZONE(zone);
	return;
}

/*
 * Handle the control event.  Note that although this event causes the zone
 * to shut down, it is not a shutdown event in the sense of the task library.
 */
static void
zone_shutdown(isc_task_t *task, isc_event_t *event) {
	dns_zone_t *zone = (dns_zone_t *) event->ev_arg;
	isc_boolean_t free_needed, linked = ISC_FALSE;

	UNUSED(task);
	REQUIRE(DNS_ZONE_VALID(zone));
	INSIST(event->ev_type == DNS_EVENT_ZONECONTROL);
	INSIST(isc_refcount_current(&zone->erefs) == 0);
	zone_debuglog(zone, "zone_shutdown", 3, "shutting down");

	/*
	 * Stop things being restarted after we cancel them below.
	 */
	LOCK_ZONE(zone);
	DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_EXITING);
	UNLOCK_ZONE(zone);

	/*
	 * If we were waiting for xfrin quota, step out of
	 * the queue.
	 * If there's no zone manager, we can't be waiting for the
	 * xfrin quota
	 */
	if (zone->zmgr != NULL) {
		RWLOCK(&zone->zmgr->rwlock, isc_rwlocktype_write);
		if (zone->statelist == &zone->zmgr->waiting_for_xfrin) {
			ISC_LIST_UNLINK(zone->zmgr->waiting_for_xfrin, zone,
					statelink);
			linked = ISC_TRUE;
			zone->statelist = NULL;
		}
		RWUNLOCK(&zone->zmgr->rwlock, isc_rwlocktype_write);
	}

	/*
	 * In task context, no locking required.  See zone_xfrdone().
	 */
	if (zone->xfr != NULL)
		dns_xfrin_shutdown(zone->xfr);

	LOCK_ZONE(zone);
	if (linked) {
		INSIST(zone->irefs > 0);
		zone->irefs--;
	}
	if (zone->request != NULL) {
		dns_request_cancel(zone->request);
	}

	if (zone->readio != NULL)
		zonemgr_cancelio(zone->readio);

	if (zone->lctx != NULL)
		dns_loadctx_cancel(zone->lctx);

	if (!DNS_ZONE_FLAG(zone, DNS_ZONEFLG_FLUSH) ||
	    !DNS_ZONE_FLAG(zone, DNS_ZONEFLG_DUMPING)) {
		if (zone->writeio != NULL)
			zonemgr_cancelio(zone->writeio);

		if (zone->dctx != NULL)
			dns_dumpctx_cancel(zone->dctx);
	}

	notify_cancel(zone);

	if (zone->timer != NULL) {
		isc_timer_detach(&zone->timer);
		INSIST(zone->irefs > 0);
		zone->irefs--;
	}

	if (zone->view != NULL)
		dns_view_weakdetach(&zone->view);

	/*
	 * We have now canceled everything set the flag to allow exit_check()
	 * to succeed.  We must not unlock between setting this flag and
	 * calling exit_check().
	 */
	DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_SHUTDOWN);
	free_needed = exit_check(zone);
	UNLOCK_ZONE(zone);
	if (free_needed)
		zone_free(zone);
}

static void
zone_timer(isc_task_t *task, isc_event_t *event) {
	const char me[] = "zone_timer";
	dns_zone_t *zone = (dns_zone_t *)event->ev_arg;

	UNUSED(task);
	REQUIRE(DNS_ZONE_VALID(zone));

	ENTER;

	zone_maintenance(zone);

	isc_event_free(&event);
}

static void
zone_settimer(dns_zone_t *zone, isc_time_t *now) {
	const char me[] = "zone_settimer";
	isc_time_t next;
	isc_result_t result;

	REQUIRE(DNS_ZONE_VALID(zone));
	if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_EXITING))
		return;

	isc_time_settoepoch(&next);

	switch (zone->type) {
	case dns_zone_master:
		if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NEEDNOTIFY))
			next = zone->notifytime;
		if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NEEDDUMP) &&
		    !DNS_ZONE_FLAG(zone, DNS_ZONEFLG_DUMPING)) {
			INSIST(!isc_time_isepoch(&zone->dumptime));
			if (isc_time_isepoch(&next) ||
			    isc_time_compare(&zone->dumptime, &next) < 0)
				next = zone->dumptime;
		}
		break;

	case dns_zone_slave:
		if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NEEDNOTIFY))
			next = zone->notifytime;
		/*FALLTHROUGH*/

	case dns_zone_stub:
		if (!DNS_ZONE_FLAG(zone, DNS_ZONEFLG_REFRESH) &&
		    !DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NOMASTERS) &&
		    !DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NOREFRESH) &&
		    !DNS_ZONE_FLAG(zone, DNS_ZONEFLG_LOADING)) {
			INSIST(!isc_time_isepoch(&zone->refreshtime));
			if (isc_time_isepoch(&next) ||
			    isc_time_compare(&zone->refreshtime, &next) < 0)
				next = zone->refreshtime;
		}
		if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_LOADED)) {
			INSIST(!isc_time_isepoch(&zone->expiretime));
			if (isc_time_isepoch(&next) ||
			    isc_time_compare(&zone->expiretime, &next) < 0)
				next = zone->expiretime;
		}
		if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NEEDDUMP) &&
		    !DNS_ZONE_FLAG(zone, DNS_ZONEFLG_DUMPING)) {
			INSIST(!isc_time_isepoch(&zone->dumptime));
			if (isc_time_isepoch(&next) ||
			    isc_time_compare(&zone->dumptime, &next) < 0)
				next = zone->dumptime;
		}
		break;

	default:
		break;
	}

	if (isc_time_isepoch(&next)) {
		zone_debuglog(zone, me, 10, "settimer inactive");
		result = isc_timer_reset(zone->timer, isc_timertype_inactive,
					  NULL, NULL, ISC_TRUE);
		if (result != ISC_R_SUCCESS)
			dns_zone_log(zone, ISC_LOG_ERROR,
				     "could not deactivate zone timer: %s",
				     isc_result_totext(result));
	} else {
		if (isc_time_compare(&next, now) <= 0)
			next = *now;
		result = isc_timer_reset(zone->timer, isc_timertype_once,
					 &next, NULL, ISC_TRUE);
		if (result != ISC_R_SUCCESS)
			dns_zone_log(zone, ISC_LOG_ERROR,
				     "could not reset zone timer: %s",
				     isc_result_totext(result));
	}
}

static void
cancel_refresh(dns_zone_t *zone) {
	const char me[] = "cancel_refresh";
	isc_time_t now;

	/*
	 * 'zone' locked by caller.
	 */

	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(LOCKED_ZONE(zone));

	ENTER;

	DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_REFRESH);
	TIME_NOW(&now);
	zone_settimer(zone, &now);
}

static isc_result_t
notify_createmessage(dns_zone_t *zone, unsigned int flags,
		     dns_message_t **messagep)
{
	dns_db_t *zonedb = NULL;
	dns_dbnode_t *node = NULL;
	dns_dbversion_t *version = NULL;
	dns_message_t *message = NULL;
	dns_rdataset_t rdataset;
	dns_rdata_t rdata = DNS_RDATA_INIT;

	dns_name_t *tempname = NULL;
	dns_rdata_t *temprdata = NULL;
	dns_rdatalist_t *temprdatalist = NULL;
	dns_rdataset_t *temprdataset = NULL;

	isc_result_t result;
	isc_region_t r;
	isc_buffer_t *b = NULL;

	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(messagep != NULL && *messagep == NULL);

	result = dns_message_create(zone->mctx, DNS_MESSAGE_INTENTRENDER,
				    &message);
	if (result != ISC_R_SUCCESS)
		return (result);

	message->opcode = dns_opcode_notify;
	message->flags |= DNS_MESSAGEFLAG_AA;
	message->rdclass = zone->rdclass;

	result = dns_message_gettempname(message, &tempname);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	result = dns_message_gettemprdataset(message, &temprdataset);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	/*
	 * Make question.
	 */
	dns_name_init(tempname, NULL);
	dns_name_clone(&zone->origin, tempname);
	dns_rdataset_init(temprdataset);
	dns_rdataset_makequestion(temprdataset, zone->rdclass,
				  dns_rdatatype_soa);
	ISC_LIST_APPEND(tempname->list, temprdataset, link);
	dns_message_addname(message, tempname, DNS_SECTION_QUESTION);
	tempname = NULL;
	temprdataset = NULL;

	if ((flags & DNS_NOTIFY_NOSOA) != 0)
		goto done;

	result = dns_message_gettempname(message, &tempname);
	if (result != ISC_R_SUCCESS)
		goto soa_cleanup;
	result = dns_message_gettemprdata(message, &temprdata);
	if (result != ISC_R_SUCCESS)
		goto soa_cleanup;
	result = dns_message_gettemprdataset(message, &temprdataset);
	if (result != ISC_R_SUCCESS)
		goto soa_cleanup;
	result = dns_message_gettemprdatalist(message, &temprdatalist);
	if (result != ISC_R_SUCCESS)
		goto soa_cleanup;

	ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_read);
	INSIST(zone->db != NULL); /* XXXJT: is this assumption correct? */
	dns_db_attach(zone->db, &zonedb);
	ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);

	dns_name_init(tempname, NULL);
	dns_name_clone(&zone->origin, tempname);
	dns_db_currentversion(zonedb, &version);
	result = dns_db_findnode(zonedb, tempname, ISC_FALSE, &node);
	if (result != ISC_R_SUCCESS)
		goto soa_cleanup;

	dns_rdataset_init(&rdataset);
	result = dns_db_findrdataset(zonedb, node, version,
				     dns_rdatatype_soa,
				     dns_rdatatype_none, 0, &rdataset,
				     NULL);
	if (result != ISC_R_SUCCESS)
		goto soa_cleanup;
	result = dns_rdataset_first(&rdataset);
	if (result != ISC_R_SUCCESS)
		goto soa_cleanup;
	dns_rdataset_current(&rdataset, &rdata);
	dns_rdata_toregion(&rdata, &r);
	result = isc_buffer_allocate(zone->mctx, &b, r.length);
	if (result != ISC_R_SUCCESS)
		goto soa_cleanup;
	isc_buffer_putmem(b, r.base, r.length);
	isc_buffer_usedregion(b, &r);
	dns_rdata_init(temprdata);
	dns_rdata_fromregion(temprdata, rdata.rdclass, rdata.type, &r);
	dns_message_takebuffer(message, &b);
	result = dns_rdataset_next(&rdataset);
	dns_rdataset_disassociate(&rdataset);
	if (result != ISC_R_NOMORE)
		goto soa_cleanup;
	temprdatalist->rdclass = rdata.rdclass;
	temprdatalist->type = rdata.type;
	temprdatalist->covers = 0;
	temprdatalist->ttl = rdataset.ttl;
	ISC_LIST_INIT(temprdatalist->rdata);
	ISC_LIST_APPEND(temprdatalist->rdata, temprdata, link);

	dns_rdataset_init(temprdataset);
	result = dns_rdatalist_tordataset(temprdatalist, temprdataset);
	if (result != ISC_R_SUCCESS)
		goto soa_cleanup;

	ISC_LIST_APPEND(tempname->list, temprdataset, link);
	dns_message_addname(message, tempname, DNS_SECTION_ANSWER);
	temprdatalist = NULL;
	temprdataset = NULL;
	temprdata = NULL;
	tempname = NULL;

 soa_cleanup:
	if (node != NULL)
		dns_db_detachnode(zonedb, &node);
	if (version != NULL)
		dns_db_closeversion(zonedb, &version, ISC_FALSE);
	if (zonedb != NULL)
		dns_db_detach(&zonedb);
	if (tempname != NULL)
		dns_message_puttempname(message, &tempname);
	if (temprdata != NULL)
		dns_message_puttemprdata(message, &temprdata);
	if (temprdataset != NULL)
		dns_message_puttemprdataset(message, &temprdataset);
	if (temprdatalist != NULL)
		dns_message_puttemprdatalist(message, &temprdatalist);

 done:
	*messagep = message;
	return (ISC_R_SUCCESS);

 cleanup:
	if (tempname != NULL)
		dns_message_puttempname(message, &tempname);
	if (temprdataset != NULL)
		dns_message_puttemprdataset(message, &temprdataset);
	dns_message_destroy(&message);
	return (result);
}

isc_result_t
dns_zone_notifyreceive(dns_zone_t *zone, isc_sockaddr_t *from,
		       dns_message_t *msg)
{
	unsigned int i;
	dns_rdata_soa_t soa;
	dns_rdataset_t *rdataset = NULL;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	isc_result_t result;
	char fromtext[ISC_SOCKADDR_FORMATSIZE];
	int match = 0;
	isc_netaddr_t netaddr;

	REQUIRE(DNS_ZONE_VALID(zone));

	/*
	 * If type != T_SOA return DNS_R_REFUSED.  We don't yet support
	 * ROLLOVER.
	 *
	 * SOA:	RFC1996
	 * Check that 'from' is a valid notify source, (zone->masters).
	 *	Return DNS_R_REFUSED if not.
	 *
	 * If the notify message contains a serial number check it
	 * against the zones serial and return if <= current serial
	 *
	 * If a refresh check is progress, if so just record the
	 * fact we received a NOTIFY and from where and return.
	 * We will perform a new refresh check when the current one
	 * completes. Return ISC_R_SUCCESS.
	 *
	 * Otherwise initiate a refresh check using 'from' as the
	 * first address to check.  Return ISC_R_SUCCESS.
	 */

	isc_sockaddr_format(from, fromtext, sizeof(fromtext));

	/*
	 *  We only handle NOTIFY (SOA) at the present.
	 */
	LOCK_ZONE(zone);
	if (msg->counts[DNS_SECTION_QUESTION] == 0 ||
	    dns_message_findname(msg, DNS_SECTION_QUESTION, &zone->origin,
				 dns_rdatatype_soa, dns_rdatatype_none,
				 NULL, NULL) != ISC_R_SUCCESS) {
		UNLOCK_ZONE(zone);
		if (msg->counts[DNS_SECTION_QUESTION] == 0) {
			dns_zone_log(zone, ISC_LOG_NOTICE,
				     "NOTIFY with no "
				     "question section from: %s", fromtext);
			return (DNS_R_FORMERR);
		}
		dns_zone_log(zone, ISC_LOG_NOTICE,
			     "NOTIFY zone does not match");
		return (DNS_R_NOTIMP);
	}

	/*
	 * If we are a master zone just succeed.
	 */
	if (zone->type == dns_zone_master) {
		UNLOCK_ZONE(zone);
		return (ISC_R_SUCCESS);
	}

	isc_netaddr_fromsockaddr(&netaddr, from);
	for (i = 0; i < zone->masterscnt; i++) {
		if (isc_sockaddr_eqaddr(from, &zone->masters[i]))
			break;
		if (zone->view->aclenv.match_mapped &&
		    IN6_IS_ADDR_V4MAPPED(&from->type.sin6.sin6_addr) &&
		    isc_sockaddr_pf(&zone->masters[i]) == AF_INET) {
			isc_netaddr_t na1, na2;
			isc_netaddr_fromv4mapped(&na1, &netaddr);
			isc_netaddr_fromsockaddr(&na2, &zone->masters[i]);
			if (isc_netaddr_equal(&na1, &na2))
				break;
		}
	}

	/*
	 * Accept notify requests from non masters if they are on
	 * 'zone->notify_acl'.
	 */
	if (i >= zone->masterscnt && zone->notify_acl != NULL &&
	    dns_acl_match(&netaddr, NULL, zone->notify_acl,
			  &zone->view->aclenv,
			  &match, NULL) == ISC_R_SUCCESS &&
	    match > 0)
	{
		/* Accept notify. */
	} else if (i >= zone->masterscnt) {
		UNLOCK_ZONE(zone);
		dns_zone_log(zone, ISC_LOG_INFO,
			     "refused notify from non-master: %s", fromtext);
		return (DNS_R_REFUSED);
	}

	/*
	 * If the zone is loaded and there are answers check the serial
	 * to see if we need to do a refresh.  Do not worry about this
	 * check if we are a dialup zone as we use the notify request
	 * to trigger a refresh check.
	 */
	if (msg->counts[DNS_SECTION_ANSWER] > 0 &&
	    DNS_ZONE_FLAG(zone, DNS_ZONEFLG_LOADED) &&
	    !DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NOREFRESH)) {
		result = dns_message_findname(msg, DNS_SECTION_ANSWER,
					      &zone->origin,
					      dns_rdatatype_soa,
					      dns_rdatatype_none, NULL,
					      &rdataset);
		if (result == ISC_R_SUCCESS)
			result = dns_rdataset_first(rdataset);
		if (result == ISC_R_SUCCESS) {
			isc_uint32_t serial = 0;

			dns_rdataset_current(rdataset, &rdata);
			result = dns_rdata_tostruct(&rdata, &soa, NULL);
			RUNTIME_CHECK(result == ISC_R_SUCCESS);
			serial = soa.serial;
			if (isc_serial_le(serial, zone->serial)) {
			  dns_zone_log(zone, ISC_LOG_INFO,
					     "notify from %s: "
					     "zone is up to date",
					     fromtext);
				UNLOCK_ZONE(zone);
				return (ISC_R_SUCCESS);
			}
		}
	}

	/*
	 * If we got this far and there was a refresh in progress just
	 * let it complete.  Record where we got the notify from so we
	 * can perform a refresh check when the current one completes
	 */
	if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_REFRESH)) {
		DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_NEEDREFRESH);
		zone->notifyfrom = *from;
		UNLOCK_ZONE(zone);
		dns_zone_log(zone, ISC_LOG_INFO,
			     "notify from %s: refresh in progress, "
			     "refresh check queued",
			     fromtext);
		return (ISC_R_SUCCESS);
	}
	zone->notifyfrom = *from;
	UNLOCK_ZONE(zone);
	dns_zone_refresh(zone);
	return (ISC_R_SUCCESS);
}

void
dns_zone_setnotifyacl(dns_zone_t *zone, dns_acl_t *acl) {

	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	if (zone->notify_acl != NULL)
		dns_acl_detach(&zone->notify_acl);
	dns_acl_attach(acl, &zone->notify_acl);
	UNLOCK_ZONE(zone);
}

void
dns_zone_setqueryacl(dns_zone_t *zone, dns_acl_t *acl) {

	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	if (zone->query_acl != NULL)
		dns_acl_detach(&zone->query_acl);
	dns_acl_attach(acl, &zone->query_acl);
	UNLOCK_ZONE(zone);
}

void
dns_zone_setupdateacl(dns_zone_t *zone, dns_acl_t *acl) {

	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	if (zone->update_acl != NULL)
		dns_acl_detach(&zone->update_acl);
	dns_acl_attach(acl, &zone->update_acl);
	UNLOCK_ZONE(zone);
}

void
dns_zone_setforwardacl(dns_zone_t *zone, dns_acl_t *acl) {

	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	if (zone->forward_acl != NULL)
		dns_acl_detach(&zone->forward_acl);
	dns_acl_attach(acl, &zone->forward_acl);
	UNLOCK_ZONE(zone);
}

void
dns_zone_setxfracl(dns_zone_t *zone, dns_acl_t *acl) {

	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	if (zone->xfr_acl != NULL)
		dns_acl_detach(&zone->xfr_acl);
	dns_acl_attach(acl, &zone->xfr_acl);
	UNLOCK_ZONE(zone);
}

dns_acl_t *
dns_zone_getnotifyacl(dns_zone_t *zone) {

	REQUIRE(DNS_ZONE_VALID(zone));

	return (zone->notify_acl);
}

dns_acl_t *
dns_zone_getqueryacl(dns_zone_t *zone) {

	REQUIRE(DNS_ZONE_VALID(zone));

	return (zone->query_acl);
}

dns_acl_t *
dns_zone_getupdateacl(dns_zone_t *zone) {

	REQUIRE(DNS_ZONE_VALID(zone));

	return (zone->update_acl);
}

dns_acl_t *
dns_zone_getforwardacl(dns_zone_t *zone) {

	REQUIRE(DNS_ZONE_VALID(zone));

	return (zone->forward_acl);
}

dns_acl_t *
dns_zone_getxfracl(dns_zone_t *zone) {

	REQUIRE(DNS_ZONE_VALID(zone));

	return (zone->xfr_acl);
}

void
dns_zone_clearupdateacl(dns_zone_t *zone) {

	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	if (zone->update_acl != NULL)
		dns_acl_detach(&zone->update_acl);
	UNLOCK_ZONE(zone);
}

void
dns_zone_clearforwardacl(dns_zone_t *zone) {

	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	if (zone->forward_acl != NULL)
		dns_acl_detach(&zone->forward_acl);
	UNLOCK_ZONE(zone);
}

void
dns_zone_clearnotifyacl(dns_zone_t *zone) {

	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	if (zone->notify_acl != NULL)
		dns_acl_detach(&zone->notify_acl);
	UNLOCK_ZONE(zone);
}

void
dns_zone_clearqueryacl(dns_zone_t *zone) {

	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	if (zone->query_acl != NULL)
		dns_acl_detach(&zone->query_acl);
	UNLOCK_ZONE(zone);
}

void
dns_zone_clearxfracl(dns_zone_t *zone) {

	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	if (zone->xfr_acl != NULL)
		dns_acl_detach(&zone->xfr_acl);
	UNLOCK_ZONE(zone);
}

isc_boolean_t
dns_zone_getupdatedisabled(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));
	return (zone->update_disabled);

}

void
dns_zone_setupdatedisabled(dns_zone_t *zone, isc_boolean_t state) {
	REQUIRE(DNS_ZONE_VALID(zone));
	zone->update_disabled = state;
}

isc_boolean_t
dns_zone_getzeronosoattl(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));
	return (zone->zero_no_soa_ttl);

}

void
dns_zone_setzeronosoattl(dns_zone_t *zone, isc_boolean_t state) {
	REQUIRE(DNS_ZONE_VALID(zone));
	zone->zero_no_soa_ttl = state;
}

void
dns_zone_setchecknames(dns_zone_t *zone, dns_severity_t severity) {

	REQUIRE(DNS_ZONE_VALID(zone));

	zone->check_names = severity;
}

dns_severity_t
dns_zone_getchecknames(dns_zone_t *zone) {

	REQUIRE(DNS_ZONE_VALID(zone));

	return (zone->check_names);
}

void
dns_zone_setjournalsize(dns_zone_t *zone, isc_int32_t size) {

	REQUIRE(DNS_ZONE_VALID(zone));

	zone->journalsize = size;
}

isc_int32_t
dns_zone_getjournalsize(dns_zone_t *zone) {

	REQUIRE(DNS_ZONE_VALID(zone));

	return (zone->journalsize);
}

static void
zone_tostr(dns_zone_t *zone, char *buf, size_t length) {
	isc_result_t result = ISC_R_FAILURE;
	isc_buffer_t buffer;

	REQUIRE(buf != NULL);
	REQUIRE(length > 1U);

	/*
	 * Leave space for terminating '\0'.
	 */
	isc_buffer_init(&buffer, buf, length - 1);
	if (dns_name_dynamic(&zone->origin))
		result = dns_name_totext(&zone->origin, ISC_TRUE, &buffer);
	if (result != ISC_R_SUCCESS &&
	    isc_buffer_availablelength(&buffer) >= (sizeof("<UNKNOWN>") - 1))
		isc_buffer_putstr(&buffer, "<UNKNOWN>");

	if (isc_buffer_availablelength(&buffer) > 0)
		isc_buffer_putstr(&buffer, "/");
	(void)dns_rdataclass_totext(zone->rdclass, &buffer);

	if (zone->view != NULL && strcmp(zone->view->name, "_bind") != 0 &&
	    strcmp(zone->view->name, "_default") != 0 &&
	    strlen(zone->view->name) < isc_buffer_availablelength(&buffer)) {
		isc_buffer_putstr(&buffer, "/");
		isc_buffer_putstr(&buffer, zone->view->name);
	}

	buf[isc_buffer_usedlength(&buffer)] = '\0';
}

void
dns_zone_name(dns_zone_t *zone, char *buf, size_t length) {
	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(buf != NULL);
	zone_tostr(zone, buf, length);
}

static void
notify_log(dns_zone_t *zone, int level, const char *fmt, ...) {
	va_list ap;
	char message[4096];
	char namebuf[1024+32];

	if (isc_log_wouldlog(dns_lctx, level) == ISC_FALSE)
		return;

	zone_tostr(zone, namebuf, sizeof(namebuf));

	va_start(ap, fmt);
	vsnprintf(message, sizeof(message), fmt, ap);
	va_end(ap);
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_NOTIFY, DNS_LOGMODULE_ZONE,
		      level, "zone %s: %s", namebuf, message);
}

void
dns_zone_logc(dns_zone_t *zone, isc_logcategory_t *category,
	      int level, const char *fmt, ...) {
	va_list ap;
	char message[4096];
	char namebuf[1024+32];

	if (isc_log_wouldlog(dns_lctx, level) == ISC_FALSE)
		return;

	zone_tostr(zone, namebuf, sizeof(namebuf));

	va_start(ap, fmt);
	vsnprintf(message, sizeof(message), fmt, ap);
	va_end(ap);
	isc_log_write(dns_lctx, category, DNS_LOGMODULE_ZONE,
		      level, "zone %s: %s", namebuf, message);
}

void
dns_zone_log(dns_zone_t *zone, int level, const char *fmt, ...) {
	va_list ap;
	char message[4096];
	char namebuf[1024+32];

	if (isc_log_wouldlog(dns_lctx, level) == ISC_FALSE)
		return;

	zone_tostr(zone, namebuf, sizeof(namebuf));

	va_start(ap, fmt);
	vsnprintf(message, sizeof(message), fmt, ap);
	va_end(ap);
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_GENERAL, DNS_LOGMODULE_ZONE,
		      level, "zone %s: %s", namebuf, message);
}

static void
zone_debuglog(dns_zone_t *zone, const char *me, int debuglevel,
	      const char *fmt, ...)
{
	va_list ap;
	char message[4096];
	char namebuf[1024+32];
	int level = ISC_LOG_DEBUG(debuglevel);

	if (isc_log_wouldlog(dns_lctx, level) == ISC_FALSE)
		return;

	zone_tostr(zone, namebuf, sizeof(namebuf));

	va_start(ap, fmt);
	vsnprintf(message, sizeof(message), fmt, ap);
	va_end(ap);
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_GENERAL, DNS_LOGMODULE_ZONE,
		      level, "%s: zone %s: %s", me, namebuf, message);
}

static int
message_count(dns_message_t *msg, dns_section_t section, dns_rdatatype_t type)
{
	isc_result_t result;
	dns_name_t *name;
	dns_rdataset_t *curr;
	int count = 0;

	result = dns_message_firstname(msg, section);
	while (result == ISC_R_SUCCESS) {
		name = NULL;
		dns_message_currentname(msg, section, &name);

		for (curr = ISC_LIST_TAIL(name->list); curr != NULL;
		     curr = ISC_LIST_PREV(curr, link)) {
			if (curr->type == type)
				count++;
		}
		result = dns_message_nextname(msg, section);
	}

	return (count);
}

void
dns_zone_setmaxxfrin(dns_zone_t *zone, isc_uint32_t maxxfrin) {
	REQUIRE(DNS_ZONE_VALID(zone));

	zone->maxxfrin = maxxfrin;
}

isc_uint32_t
dns_zone_getmaxxfrin(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));

	return (zone->maxxfrin);
}

void
dns_zone_setmaxxfrout(dns_zone_t *zone, isc_uint32_t maxxfrout) {
	REQUIRE(DNS_ZONE_VALID(zone));
	zone->maxxfrout = maxxfrout;
}

isc_uint32_t
dns_zone_getmaxxfrout(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));

	return (zone->maxxfrout);
}

dns_zonetype_t
dns_zone_gettype(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));

	return (zone->type);
}

dns_name_t *
dns_zone_getorigin(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));

	return (&zone->origin);
}

void
dns_zone_settask(dns_zone_t *zone, isc_task_t *task) {
	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	if (zone->task != NULL)
		isc_task_detach(&zone->task);
	isc_task_attach(task, &zone->task);
	ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_read);
	if (zone->db != NULL)
		dns_db_settask(zone->db, zone->task);
	ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);
	UNLOCK_ZONE(zone);
}

void
dns_zone_gettask(dns_zone_t *zone, isc_task_t **target) {
	REQUIRE(DNS_ZONE_VALID(zone));
	isc_task_attach(zone->task, target);
}

void
dns_zone_setidlein(dns_zone_t *zone, isc_uint32_t idlein) {
	REQUIRE(DNS_ZONE_VALID(zone));

	if (idlein == 0)
		idlein = DNS_DEFAULT_IDLEIN;
	zone->idlein = idlein;
}

isc_uint32_t
dns_zone_getidlein(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));

	return (zone->idlein);
}

void
dns_zone_setidleout(dns_zone_t *zone, isc_uint32_t idleout) {
	REQUIRE(DNS_ZONE_VALID(zone));

	zone->idleout = idleout;
}

isc_uint32_t
dns_zone_getidleout(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));

	return (zone->idleout);
}

static void
notify_done(isc_task_t *task, isc_event_t *event) {
	dns_requestevent_t *revent = (dns_requestevent_t *)event;
	dns_notify_t *notify;
	isc_result_t result;
	dns_message_t *message = NULL;
	isc_buffer_t buf;
	char rcode[128];
	char addrbuf[ISC_SOCKADDR_FORMATSIZE];

	UNUSED(task);

	notify = event->ev_arg;
	REQUIRE(DNS_NOTIFY_VALID(notify));
	INSIST(task == notify->zone->task);

	isc_buffer_init(&buf, rcode, sizeof(rcode));
	isc_sockaddr_format(&notify->dst, addrbuf, sizeof(addrbuf));

	result = revent->result;
	if (result == ISC_R_SUCCESS)
		result = dns_message_create(notify->zone->mctx,
					    DNS_MESSAGE_INTENTPARSE, &message);
	if (result == ISC_R_SUCCESS)
		result = dns_request_getresponse(revent->request, message,
					DNS_MESSAGEPARSE_PRESERVEORDER);
	if (result == ISC_R_SUCCESS)
		result = dns_rcode_totext(message->rcode, &buf);
	if (result == ISC_R_SUCCESS)
		notify_log(notify->zone, ISC_LOG_DEBUG(3),
			   "notify response from %s: %.*s",
			   addrbuf, (int)buf.used, rcode);
	else
		notify_log(notify->zone, ISC_LOG_DEBUG(2),
			   "notify to %s failed: %s", addrbuf,
			   dns_result_totext(result));

	/*
	 * Old bind's return formerr if they see a soa record.  Retry w/o
	 * the soa if we see a formerr and had sent a SOA.
	 */
	isc_event_free(&event);
	if (message != NULL && message->rcode == dns_rcode_formerr &&
	    (notify->flags & DNS_NOTIFY_NOSOA) == 0) {
		notify->flags |= DNS_NOTIFY_NOSOA;
		dns_request_destroy(&notify->request);
		result = notify_send_queue(notify);
		if (result != ISC_R_SUCCESS)
			notify_destroy(notify, ISC_FALSE);
	} else {
		if (result == ISC_R_TIMEDOUT)
			notify_log(notify->zone, ISC_LOG_DEBUG(1),
				   "notify to %s: retries exceeded", addrbuf);
		notify_destroy(notify, ISC_FALSE);
	}
	if (message != NULL)
		dns_message_destroy(&message);
}

isc_result_t
dns_zone_replacedb(dns_zone_t *zone, dns_db_t *db, isc_boolean_t dump) {
	isc_result_t result;

	REQUIRE(DNS_ZONE_VALID(zone));
	LOCK_ZONE(zone);
	ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_write);
	result = zone_replacedb(zone, db, dump);
	ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_write);
	UNLOCK_ZONE(zone);
	return (result);
}

static isc_result_t
zone_replacedb(dns_zone_t *zone, dns_db_t *db, isc_boolean_t dump) {
	dns_dbversion_t *ver;
	isc_result_t result;
	unsigned int soacount = 0;
	unsigned int nscount = 0;

	/*
	 * 'zone' and 'zonedb' locked by caller.
	 */
	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(LOCKED_ZONE(zone));

	result = zone_get_from_db(zone, db, &nscount, &soacount,
				  NULL, NULL, NULL, NULL, NULL, NULL);
	if (result == ISC_R_SUCCESS) {
		if (soacount != 1) {
			dns_zone_log(zone, ISC_LOG_ERROR,
				     "has %d SOA records", soacount);
			result = DNS_R_BADZONE;
		}
		if (nscount == 0) {
			dns_zone_log(zone, ISC_LOG_ERROR, "has no NS records");
			result = DNS_R_BADZONE;
		}
		if (result != ISC_R_SUCCESS)
			return (result);
	} else {
		dns_zone_log(zone, ISC_LOG_ERROR,
			    "retrieving SOA and NS records failed: %s",
			    dns_result_totext(result));
		return (result);
	}

	ver = NULL;
	dns_db_currentversion(db, &ver);

	/*
	 * The initial version of a slave zone is always dumped;
	 * subsequent versions may be journaled instead if this
	 * is enabled in the configuration.
	 */
	if (zone->db != NULL && zone->journal != NULL &&
	    DNS_ZONE_OPTION(zone, DNS_ZONEOPT_IXFRFROMDIFFS) &&
	    !DNS_ZONE_FLAG(zone, DNS_ZONEFLG_FORCEXFER)) {
		isc_uint32_t serial;

		dns_zone_log(zone, ISC_LOG_DEBUG(3), "generating diffs");

		result = dns_db_getsoaserial(db, ver, &serial);
		if (result != ISC_R_SUCCESS) {
			dns_zone_log(zone, ISC_LOG_ERROR,
				     "ixfr-from-differences: unable to get "
				     "new serial");
			goto fail;
		}

		/*
		 * This is checked in zone_postload() for master zones.
		 */
		if (zone->type == dns_zone_slave &&
		    !isc_serial_gt(serial, zone->serial)) {
			isc_uint32_t serialmin, serialmax;
			serialmin = (zone->serial + 1) & 0xffffffffU;
			serialmax = (zone->serial + 0x7fffffffU) & 0xffffffffU;
			dns_zone_log(zone, ISC_LOG_ERROR,
				     "ixfr-from-differences: failed: "
				     "new serial (%u) out of range [%u - %u]",
				     serial, serialmin, serialmax);
			result = ISC_R_RANGE;
			goto fail;
		}

		result = dns_db_diff(zone->mctx, db, ver, zone->db, NULL,
				     zone->journal);
		if (result != ISC_R_SUCCESS)
			goto fail;
		if (dump)
			zone_needdump(zone, DNS_DUMP_DELAY);
		else if (zone->journalsize != -1) {
			result = dns_journal_compact(zone->mctx, zone->journal,
						     serial, zone->journalsize);
			switch (result) {
			case ISC_R_SUCCESS:
			case ISC_R_NOSPACE:
			case ISC_R_NOTFOUND:
				dns_zone_log(zone, ISC_LOG_DEBUG(3),
					     "dns_journal_compact: %s",
					     dns_result_totext(result));
				break;
			default:
				dns_zone_log(zone, ISC_LOG_ERROR,
					     "dns_journal_compact failed: %s",
					     dns_result_totext(result));
				break;
			}
		}
	} else {
		if (dump && zone->masterfile != NULL) {
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_GENERAL,
				      DNS_LOGMODULE_ZONE, ISC_LOG_DEBUG(3),
				      "dumping new zone version");
			result = dns_db_dump2(db, ver, zone->masterfile,
					      zone->masterformat);
			if (result != ISC_R_SUCCESS)
				goto fail;

			/*
			 * Update the time the zone was updated, so
			 * dns_zone_load can avoid loading it when
			 * the server is reloaded.  If isc_time_now
			 * fails for some reason, all that happens is
			 * the timestamp is not updated.
			 */
			TIME_NOW(&zone->loadtime);
		}

		if (dump && zone->journal != NULL) {
			/*
			 * The in-memory database just changed, and
			 * because 'dump' is set, it didn't change by
			 * being loaded from disk.  Also, we have not
			 * journaled diffs for this change.
			 * Therefore, the on-disk journal is missing
			 * the deltas for this change.  Since it can
			 * no longer be used to bring the zone
			 * up-to-date, it is useless and should be
			 * removed.
			 */
			isc_log_write(dns_lctx, DNS_LOGCATEGORY_GENERAL,
				      DNS_LOGMODULE_ZONE, ISC_LOG_DEBUG(3),
				      "removing journal file");
			if (remove(zone->journal) < 0 && errno != ENOENT) {
				char strbuf[ISC_STRERRORSIZE];
				isc__strerror(errno, strbuf, sizeof(strbuf));
				isc_log_write(dns_lctx,
					      DNS_LOGCATEGORY_GENERAL,
					      DNS_LOGMODULE_ZONE,
					      ISC_LOG_WARNING,
					      "unable to remove journal "
					      "'%s': '%s'",
					      zone->journal, strbuf);
			}
		}
	}

	dns_db_closeversion(db, &ver, ISC_FALSE);

	isc_log_write(dns_lctx, DNS_LOGCATEGORY_GENERAL,
		      DNS_LOGMODULE_ZONE, ISC_LOG_DEBUG(3),
		      "replacing zone database");

	if (zone->db != NULL)
		zone_detachdb(zone);
	zone_attachdb(zone, db);
	dns_db_settask(zone->db, zone->task);
	DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_LOADED|DNS_ZONEFLG_NEEDNOTIFY);
	return (ISC_R_SUCCESS);

 fail:
	dns_db_closeversion(db, &ver, ISC_FALSE);
	return (result);
}

/* The caller must hold the dblock as a writer. */
static inline void
zone_attachdb(dns_zone_t *zone, dns_db_t *db) {
	REQUIRE(zone->db == NULL && db != NULL);

	dns_db_attach(db, &zone->db);
	if (zone->acache != NULL) {
		isc_result_t result;
		result = dns_acache_setdb(zone->acache, db);
		if (result != ISC_R_SUCCESS && result != ISC_R_EXISTS) {
			UNEXPECTED_ERROR(__FILE__, __LINE__,
					 "dns_acache_setdb() failed: %s",
					 isc_result_totext(result));
		}
	}
}

/* The caller must hold the dblock as a writer. */
static inline void
zone_detachdb(dns_zone_t *zone) {
	REQUIRE(zone->db != NULL);

	if (zone->acache != NULL)
		(void)dns_acache_putdb(zone->acache, zone->db);
	dns_db_detach(&zone->db);
}

static void
zone_xfrdone(dns_zone_t *zone, isc_result_t result) {
	isc_time_t now;
	isc_boolean_t again = ISC_FALSE;
	unsigned int soacount;
	unsigned int nscount;
	isc_uint32_t serial, refresh, retry, expire, minimum;
	isc_result_t xfrresult = result;
	isc_boolean_t free_needed;

	REQUIRE(DNS_ZONE_VALID(zone));

	dns_zone_log(zone, ISC_LOG_DEBUG(1),
		     "zone transfer finished: %s", dns_result_totext(result));

	LOCK_ZONE(zone);
	INSIST((zone->flags & DNS_ZONEFLG_REFRESH) != 0);
	DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_REFRESH);
	DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_SOABEFOREAXFR);

	TIME_NOW(&now);
	switch (result) {
	case ISC_R_SUCCESS:
		DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_NEEDNOTIFY);
		/*FALLTHROUGH*/
	case DNS_R_UPTODATE:
		DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_FORCEXFER);
		/*
		 * Has the zone expired underneath us?
		 */
		ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_read);
		if (zone->db == NULL) {
			ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);
			goto same_master;
		}

		/*
		 * Update the zone structure's data from the actual
		 * SOA received.
		 */
		nscount = 0;
		soacount = 0;
		INSIST(zone->db != NULL);
		result = zone_get_from_db(zone, zone->db, &nscount,
					  &soacount, &serial, &refresh,
					  &retry, &expire, &minimum, NULL);
		ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);
		if (result == ISC_R_SUCCESS) {
			if (soacount != 1)
				dns_zone_log(zone, ISC_LOG_ERROR,
					     "transferred zone "
					     "has %d SOA record%s", soacount,
					     (soacount != 0) ? "s" : "");
			if (nscount == 0) {
				dns_zone_log(zone, ISC_LOG_ERROR,
					     "transferred zone "
					     "has no NS records");
				if (DNS_ZONE_FLAG(zone,
						  DNS_ZONEFLG_HAVETIMERS)) {
					zone->refresh = DNS_ZONE_DEFAULTREFRESH;
					zone->retry = DNS_ZONE_DEFAULTRETRY;
				}
				DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_HAVETIMERS);
				zone_unload(zone);
				goto next_master;
			}
			zone->refresh = RANGE(refresh, zone->minrefresh,
					      zone->maxrefresh);
			zone->retry = RANGE(retry, zone->minretry,
					    zone->maxretry);
			zone->expire = RANGE(expire,
					     zone->refresh + zone->retry,
					     DNS_MAX_EXPIRE);
			zone->minimum = minimum;
			DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_HAVETIMERS);
		}

		/*
		 * Set our next update/expire times.
		 */
		if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NEEDREFRESH)) {
			DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_NEEDREFRESH);
			zone->refreshtime = now;
			DNS_ZONE_TIME_ADD(&now, zone->expire,
					  &zone->expiretime);
		} else {
			DNS_ZONE_JITTER_ADD(&now, zone->refresh,
					    &zone->refreshtime);
			DNS_ZONE_TIME_ADD(&now, zone->expire,
					  &zone->expiretime);
		}
		if (result == ISC_R_SUCCESS && xfrresult == ISC_R_SUCCESS) {
			char buf[DNS_NAME_FORMATSIZE + sizeof(": TSIG ''")];
			if (zone->tsigkey != NULL) {
				char namebuf[DNS_NAME_FORMATSIZE];
				dns_name_format(&zone->tsigkey->name, namebuf,
						sizeof(namebuf));
				snprintf(buf, sizeof(buf), ": TSIG '%s'",
					 namebuf);
			} else
				buf[0] = '\0';
			dns_zone_log(zone, ISC_LOG_INFO,
				     "transferred serial %u%s",
				     serial, buf);
		}

		/*
		 * This is not necessary if we just performed a AXFR
		 * however it is necessary for an IXFR / UPTODATE and
		 * won't hurt with an AXFR.
		 */
		if (zone->masterfile != NULL || zone->journal != NULL) {
			result = ISC_R_FAILURE;
			if (zone->journal != NULL)
				result = isc_file_settime(zone->journal, &now);
			if (result != ISC_R_SUCCESS &&
			    zone->masterfile != NULL)
				result = isc_file_settime(zone->masterfile,
							  &now);
			/* Someone removed the file from underneath us! */
			if (result == ISC_R_FILENOTFOUND &&
			    zone->masterfile != NULL)
				zone_needdump(zone, DNS_DUMP_DELAY);
			else if (result != ISC_R_SUCCESS)
				dns_zone_log(zone, ISC_LOG_ERROR,
					     "transfer: could not set file "
					     "modification time of '%s': %s",
					     zone->masterfile,
					     dns_result_totext(result));
		}

		break;

	case DNS_R_BADIXFR:
		/* Force retry with AXFR. */
		DNS_ZONE_SETFLAG(zone, DNS_ZONEFLAG_NOIXFR);
		goto same_master;

	default:
	next_master:
		/*
		 * Skip to next failed / untried master.
		 */
		do {
			zone->curmaster++;
		} while (zone->curmaster < zone->masterscnt &&
			 zone->mastersok[zone->curmaster]);
		/* FALLTHROUGH */
	same_master:
		if (zone->curmaster >= zone->masterscnt) {
			zone->curmaster = 0;
			if (DNS_ZONE_OPTION(zone, DNS_ZONEOPT_USEALTXFRSRC) &&
			    !DNS_ZONE_FLAG(zone, DNS_ZONEFLG_USEALTXFRSRC)) {
				DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_REFRESH);
				DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_USEALTXFRSRC);
				while (zone->curmaster < zone->masterscnt &&
				       zone->mastersok[zone->curmaster])
					zone->curmaster++;
				again = ISC_TRUE;
			} else
				DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_USEALTXFRSRC);
		} else {
			DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_REFRESH);
			again = ISC_TRUE;
		}
		break;
	}
	zone_settimer(zone, &now);

	/*
	 * If creating the transfer object failed, zone->xfr is NULL.
	 * Otherwise, we are called as the done callback of a zone
	 * transfer object that just entered its shutting-down
	 * state.  Since we are no longer responsible for shutting
	 * it down, we can detach our reference.
	 */
	if (zone->xfr != NULL)
		dns_xfrin_detach(&zone->xfr);

	if (zone->tsigkey != NULL)
		dns_tsigkey_detach(&zone->tsigkey);

	/*
	 * Handle any deferred journal compaction.
	 */
	if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NEEDCOMPACT)) {
		result = dns_journal_compact(zone->mctx, zone->journal,
					     zone->compact_serial,
					     zone->journalsize);
		switch (result) {
		case ISC_R_SUCCESS:
		case ISC_R_NOSPACE:
		case ISC_R_NOTFOUND:
			dns_zone_log(zone, ISC_LOG_DEBUG(3),
				     "dns_journal_compact: %s",
				     dns_result_totext(result));
			break;
		default:
			dns_zone_log(zone, ISC_LOG_ERROR,
				     "dns_journal_compact failed: %s",
				     dns_result_totext(result));
			break;
		}
		DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_NEEDCOMPACT);
	}

	/*
	 * This transfer finishing freed up a transfer quota slot.
	 * Let any other zones waiting for quota have it.
	 */
	RWLOCK(&zone->zmgr->rwlock, isc_rwlocktype_write);
	ISC_LIST_UNLINK(zone->zmgr->xfrin_in_progress, zone, statelink);
	zone->statelist = NULL;
	zmgr_resume_xfrs(zone->zmgr, ISC_FALSE);
	RWUNLOCK(&zone->zmgr->rwlock, isc_rwlocktype_write);

	/*
	 * Retry with a different server if necessary.
	 */
	if (again && !DNS_ZONE_FLAG(zone, DNS_ZONEFLG_EXITING))
		queue_soa_query(zone);

	INSIST(zone->irefs > 0);
	zone->irefs--;
	free_needed = exit_check(zone);
	UNLOCK_ZONE(zone);
	if (free_needed)
		zone_free(zone);
}

static void
zone_loaddone(void *arg, isc_result_t result) {
	static char me[] = "zone_loaddone";
	dns_load_t *load = arg;
	dns_zone_t *zone;
	isc_result_t tresult;

	REQUIRE(DNS_LOAD_VALID(load));
	zone = load->zone;

	ENTER;

	tresult = dns_db_endload(load->db, &load->callbacks.add_private);
	if (tresult != ISC_R_SUCCESS &&
	    (result == ISC_R_SUCCESS || result == DNS_R_SEENINCLUDE))
		result = tresult;

	LOCK_ZONE(load->zone);
	(void)zone_postload(load->zone, load->db, load->loadtime, result);
	zonemgr_putio(&load->zone->readio);
	DNS_ZONE_CLRFLAG(load->zone, DNS_ZONEFLG_LOADING);
	/*
	 * Leave the zone frozen if the reload fails.
	 */
	if ((result == ISC_R_SUCCESS || result == DNS_R_SEENINCLUDE) &&
	     DNS_ZONE_FLAG(load->zone, DNS_ZONEFLG_THAW))
		zone->update_disabled = ISC_FALSE;
	DNS_ZONE_CLRFLAG(load->zone, DNS_ZONEFLG_THAW);
	UNLOCK_ZONE(load->zone);

	load->magic = 0;
	dns_db_detach(&load->db);
	if (load->zone->lctx != NULL)
		dns_loadctx_detach(&load->zone->lctx);
	dns_zone_idetach(&load->zone);
	isc_mem_putanddetach(&load->mctx, load, sizeof(*load));
}

void
dns_zone_getssutable(dns_zone_t *zone, dns_ssutable_t **table) {
	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(table != NULL);
	REQUIRE(*table == NULL);

	LOCK_ZONE(zone);
	if (zone->ssutable != NULL)
		dns_ssutable_attach(zone->ssutable, table);
	UNLOCK_ZONE(zone);
}

void
dns_zone_setssutable(dns_zone_t *zone, dns_ssutable_t *table) {
	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	if (zone->ssutable != NULL)
		dns_ssutable_detach(&zone->ssutable);
	if (table != NULL)
		dns_ssutable_attach(table, &zone->ssutable);
	UNLOCK_ZONE(zone);
}

void
dns_zone_setsigvalidityinterval(dns_zone_t *zone, isc_uint32_t interval) {
	REQUIRE(DNS_ZONE_VALID(zone));

	zone->sigvalidityinterval = interval;
}

isc_uint32_t
dns_zone_getsigvalidityinterval(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));

	return (zone->sigvalidityinterval);
}

static void
queue_xfrin(dns_zone_t *zone) {
	const char me[] = "queue_xfrin";
	isc_result_t result;
	dns_zonemgr_t *zmgr = zone->zmgr;

	ENTER;

	INSIST(zone->statelist == NULL);

	RWLOCK(&zmgr->rwlock, isc_rwlocktype_write);
	ISC_LIST_APPEND(zmgr->waiting_for_xfrin, zone, statelink);
	LOCK_ZONE(zone);
	zone->irefs++;
	UNLOCK_ZONE(zone);
	zone->statelist = &zmgr->waiting_for_xfrin;
	result = zmgr_start_xfrin_ifquota(zmgr, zone);
	RWUNLOCK(&zmgr->rwlock, isc_rwlocktype_write);

	if (result == ISC_R_QUOTA) {
		dns_zone_logc(zone, DNS_LOGCATEGORY_XFER_IN, ISC_LOG_INFO,
			      "zone transfer deferred due to quota");
	} else if (result != ISC_R_SUCCESS) {
		dns_zone_logc(zone, DNS_LOGCATEGORY_XFER_IN, ISC_LOG_ERROR,
			      "starting zone transfer: %s",
			      isc_result_totext(result));
	}
}

/*
 * This event callback is called when a zone has received
 * any necessary zone transfer quota.  This is the time
 * to go ahead and start the transfer.
 */
static void
got_transfer_quota(isc_task_t *task, isc_event_t *event) {
	isc_result_t result;
	dns_peer_t *peer = NULL;
	char mastertext[256];
	dns_rdatatype_t xfrtype;
	dns_zone_t *zone = event->ev_arg;
	isc_netaddr_t masterip;
	isc_sockaddr_t sourceaddr;
	isc_sockaddr_t masteraddr;

	UNUSED(task);

	INSIST(task == zone->task);

	if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_EXITING)) {
		result = ISC_R_CANCELED;
		goto cleanup;
	}

	isc_sockaddr_format(&zone->masteraddr, mastertext, sizeof(mastertext));

	isc_netaddr_fromsockaddr(&masterip, &zone->masteraddr);
	(void)dns_peerlist_peerbyaddr(zone->view->peers,
				      &masterip, &peer);

	/*
	 * Decide whether we should request IXFR or AXFR.
	 */
	if (zone->db == NULL) {
		dns_zone_log(zone, ISC_LOG_DEBUG(1),
			     "no database exists yet, "
			     "requesting AXFR of "
			     "initial version from %s", mastertext);
		xfrtype = dns_rdatatype_axfr;
	} else if (DNS_ZONE_OPTION(zone, DNS_ZONEOPT_IXFRFROMDIFFS)) {
		dns_zone_log(zone, ISC_LOG_DEBUG(1), "ixfr-from-differences "
			     "set, requesting AXFR from %s", mastertext);
		xfrtype = dns_rdatatype_axfr;
	} else if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_FORCEXFER)) {
		dns_zone_log(zone, ISC_LOG_DEBUG(1),
			     "forced reload, requesting AXFR of "
			     "initial version from %s", mastertext);
		xfrtype = dns_rdatatype_axfr;
	} else if (DNS_ZONE_FLAG(zone, DNS_ZONEFLAG_NOIXFR)) {
		dns_zone_log(zone, ISC_LOG_DEBUG(1),
			     "retrying with AXFR from %s due to "
			     "previous IXFR failure", mastertext);
		xfrtype = dns_rdatatype_axfr;
		LOCK_ZONE(zone);
		DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLAG_NOIXFR);
		UNLOCK_ZONE(zone);
	} else {
		isc_boolean_t use_ixfr = ISC_TRUE;
		if (peer != NULL &&
		    dns_peer_getrequestixfr(peer, &use_ixfr) ==
		    ISC_R_SUCCESS) {
			; /* Using peer setting */
		} else {
			use_ixfr = zone->view->requestixfr;
		}
		if (use_ixfr == ISC_FALSE) {
			dns_zone_log(zone, ISC_LOG_DEBUG(1),
				     "IXFR disabled, "
				     "requesting AXFR from %s",
				     mastertext);
			if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_SOABEFOREAXFR))
				xfrtype = dns_rdatatype_soa;
			else
				xfrtype = dns_rdatatype_axfr;
		} else {
			dns_zone_log(zone, ISC_LOG_DEBUG(1),
				     "requesting IXFR from %s",
				     mastertext);
			xfrtype = dns_rdatatype_ixfr;
		}
	}

	/*
	 * Determine if we should attempt to sign the request with TSIG.
	 */
	result = ISC_R_NOTFOUND;
	/*
	 * First, look for a tsig key in the master statement, then
	 * try for a server key.
	 */
	if ((zone->masterkeynames != NULL) &&
	    (zone->masterkeynames[zone->curmaster] != NULL)) {
		dns_view_t *view = dns_zone_getview(zone);
		dns_name_t *keyname = zone->masterkeynames[zone->curmaster];
		result = dns_view_gettsig(view, keyname, &zone->tsigkey);
	}
	if (zone->tsigkey == NULL)
		result = dns_view_getpeertsig(zone->view, &masterip,
					      &zone->tsigkey);

	if (result != ISC_R_SUCCESS && result != ISC_R_NOTFOUND) {
		dns_zone_log(zone, ISC_LOG_ERROR,
			     "could not get TSIG key "
			     "for zone transfer: %s",
			     isc_result_totext(result));
	}

	LOCK_ZONE(zone);
	masteraddr = zone->masteraddr;
	sourceaddr = zone->sourceaddr;
	UNLOCK_ZONE(zone);
	INSIST(isc_sockaddr_pf(&masteraddr) == isc_sockaddr_pf(&sourceaddr));
	result = dns_xfrin_create2(zone, xfrtype, &masteraddr, &sourceaddr,
				   zone->tsigkey, zone->mctx,
				   zone->zmgr->timermgr, zone->zmgr->socketmgr,
				   zone->task, zone_xfrdone, &zone->xfr);
 cleanup:
	/*
	 * Any failure in this function is handled like a failed
	 * zone transfer.  This ensures that we get removed from
	 * zmgr->xfrin_in_progress.
	 */
	if (result != ISC_R_SUCCESS)
		zone_xfrdone(zone, result);

	isc_event_free(&event);
}

/*
 * Update forwarding support.
 */

static void
forward_destroy(dns_forward_t *forward) {

	forward->magic = 0;
	if (forward->request != NULL)
		dns_request_destroy(&forward->request);
	if (forward->msgbuf != NULL)
		isc_buffer_free(&forward->msgbuf);
	if (forward->zone != NULL)
		dns_zone_idetach(&forward->zone);
	isc_mem_putanddetach(&forward->mctx, forward, sizeof(*forward));
}

static isc_result_t
sendtomaster(dns_forward_t *forward) {
	isc_result_t result;
	isc_sockaddr_t src;

	LOCK_ZONE(forward->zone);
	if (forward->which >= forward->zone->masterscnt) {
		UNLOCK_ZONE(forward->zone);
		return (ISC_R_NOMORE);
	}

	forward->addr = forward->zone->masters[forward->which];
	/*
	 * Always use TCP regardless of whether the original update
	 * used TCP.
	 * XXX The timeout may but a bit small if we are far down a
	 * transfer graph and the master has to try several masters.
	 */
	switch (isc_sockaddr_pf(&forward->addr)) {
	case PF_INET:
		src = forward->zone->xfrsource4;
		break;
	case PF_INET6:
		src = forward->zone->xfrsource6;
		break;
	default:
		result = ISC_R_NOTIMPLEMENTED;
		goto unlock;
	}
	result = dns_request_createraw(forward->zone->view->requestmgr,
				       forward->msgbuf,
				       &src, &forward->addr,
				       DNS_REQUESTOPT_TCP, 15 /* XXX */,
				       forward->zone->task,
				       forward_callback, forward,
				       &forward->request);
 unlock:
	UNLOCK_ZONE(forward->zone);
	return (result);
}

static void
forward_callback(isc_task_t *task, isc_event_t *event) {
	const char me[] = "forward_callback";
	dns_requestevent_t *revent = (dns_requestevent_t *)event;
	dns_message_t *msg = NULL;
	char master[ISC_SOCKADDR_FORMATSIZE];
	isc_result_t result;
	dns_forward_t *forward;
	dns_zone_t *zone;

	UNUSED(task);

	forward = revent->ev_arg;
	INSIST(DNS_FORWARD_VALID(forward));
	zone = forward->zone;
	INSIST(DNS_ZONE_VALID(zone));

	ENTER;

	isc_sockaddr_format(&forward->addr, master, sizeof(master));

	if (revent->result != ISC_R_SUCCESS) {
		dns_zone_log(zone, ISC_LOG_INFO,
			     "could not forward dynamic update to %s: %s",
			     master, dns_result_totext(revent->result));
		goto next_master;
	}

	result = dns_message_create(zone->mctx, DNS_MESSAGE_INTENTPARSE, &msg);
	if (result != ISC_R_SUCCESS)
		goto next_master;

	result = dns_request_getresponse(revent->request, msg,
					 DNS_MESSAGEPARSE_PRESERVEORDER |
					 DNS_MESSAGEPARSE_CLONEBUFFER);
	if (result != ISC_R_SUCCESS)
		goto next_master;

	switch (msg->rcode) {
	/*
	 * Pass these rcodes back to client.
	 */
	case dns_rcode_noerror:
	case dns_rcode_yxdomain:
	case dns_rcode_yxrrset:
	case dns_rcode_nxrrset:
	case dns_rcode_refused:
	case dns_rcode_nxdomain:
		break;

	/* These should not occur if the masters/zone are valid. */
	case dns_rcode_notzone:
	case dns_rcode_notauth: {
		char rcode[128];
		isc_buffer_t rb;

		isc_buffer_init(&rb, rcode, sizeof(rcode));
		(void)dns_rcode_totext(msg->rcode, &rb);
		dns_zone_log(zone, ISC_LOG_WARNING,
			     "forwarding dynamic update: "
			     "unexpected response: master %s returned: %.*s",
			     master, (int)rb.used, rcode);
		goto next_master;
	}

	/* Try another server for these rcodes. */
	case dns_rcode_formerr:
	case dns_rcode_servfail:
	case dns_rcode_notimp:
	case dns_rcode_badvers:
	default:
		goto next_master;
	}

	/* call callback */
	(forward->callback)(forward->callback_arg, ISC_R_SUCCESS, msg);
	msg = NULL;
	dns_request_destroy(&forward->request);
	forward_destroy(forward);
	isc_event_free(&event);
	return;

 next_master:
	if (msg != NULL)
		dns_message_destroy(&msg);
	isc_event_free(&event);
	forward->which++;
	dns_request_destroy(&forward->request);
	result = sendtomaster(forward);
	if (result != ISC_R_SUCCESS) {
		/* call callback */
		dns_zone_log(zone, ISC_LOG_DEBUG(3),
			     "exhausted dynamic update forwarder list");
		(forward->callback)(forward->callback_arg, result, NULL);
		forward_destroy(forward);
	}
}

isc_result_t
dns_zone_forwardupdate(dns_zone_t *zone, dns_message_t *msg,
		       dns_updatecallback_t callback, void *callback_arg)
{
	dns_forward_t *forward;
	isc_result_t result;
	isc_region_t *mr;

	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(msg != NULL);
	REQUIRE(callback != NULL);

	forward = isc_mem_get(zone->mctx, sizeof(*forward));
	if (forward == NULL)
		return (ISC_R_NOMEMORY);

	forward->request = NULL;
	forward->zone = NULL;
	forward->msgbuf = NULL;
	forward->which = 0;
	forward->mctx = 0;
	forward->callback = callback;
	forward->callback_arg = callback_arg;
	forward->magic = FORWARD_MAGIC;

	mr = dns_message_getrawmessage(msg);
	if (mr == NULL) {
		result = ISC_R_UNEXPECTEDEND;
		goto cleanup;
	}

	result = isc_buffer_allocate(zone->mctx, &forward->msgbuf, mr->length);
	if (result != ISC_R_SUCCESS)
		goto cleanup;
	result = isc_buffer_copyregion(forward->msgbuf, mr);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	isc_mem_attach(zone->mctx, &forward->mctx);
	dns_zone_iattach(zone, &forward->zone);
	result = sendtomaster(forward);

 cleanup:
	if (result != ISC_R_SUCCESS) {
		forward_destroy(forward);
	}
	return (result);
}

isc_result_t
dns_zone_next(dns_zone_t *zone, dns_zone_t **next) {
	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(next != NULL && *next == NULL);

	*next = ISC_LIST_NEXT(zone, link);
	if (*next == NULL)
		return (ISC_R_NOMORE);
	else
		return (ISC_R_SUCCESS);
}

isc_result_t
dns_zone_first(dns_zonemgr_t *zmgr, dns_zone_t **first) {
	REQUIRE(DNS_ZONEMGR_VALID(zmgr));
	REQUIRE(first != NULL && *first == NULL);

	*first = ISC_LIST_HEAD(zmgr->zones);
	if (*first == NULL)
		return (ISC_R_NOMORE);
	else
		return (ISC_R_SUCCESS);
}

/***
 ***	Zone manager.
 ***/

isc_result_t
dns_zonemgr_create(isc_mem_t *mctx, isc_taskmgr_t *taskmgr,
		   isc_timermgr_t *timermgr, isc_socketmgr_t *socketmgr,
		   dns_zonemgr_t **zmgrp)
{
	dns_zonemgr_t *zmgr;
	isc_result_t result;
	isc_interval_t interval;

	zmgr = isc_mem_get(mctx, sizeof(*zmgr));
	if (zmgr == NULL)
		return (ISC_R_NOMEMORY);
	zmgr->mctx = NULL;
	zmgr->refs = 1;
	isc_mem_attach(mctx, &zmgr->mctx);
	zmgr->taskmgr = taskmgr;
	zmgr->timermgr = timermgr;
	zmgr->socketmgr = socketmgr;
	zmgr->zonetasks = NULL;
	zmgr->task = NULL;
	zmgr->rl = NULL;
	ISC_LIST_INIT(zmgr->zones);
	ISC_LIST_INIT(zmgr->waiting_for_xfrin);
	ISC_LIST_INIT(zmgr->xfrin_in_progress);
	result = isc_rwlock_init(&zmgr->rwlock, 0, 0);
	if (result != ISC_R_SUCCESS)
		goto free_mem;

	zmgr->transfersin = 10;
	zmgr->transfersperns = 2;

	/* Create the zone task pool. */
	result = isc_taskpool_create(taskmgr, mctx,
				     8 /* XXX */, 2, &zmgr->zonetasks);
	if (result != ISC_R_SUCCESS)
		goto free_rwlock;

	/* Create a single task for queueing of SOA queries. */
	result = isc_task_create(taskmgr, 1, &zmgr->task);
	if (result != ISC_R_SUCCESS)
		goto free_taskpool;
	isc_task_setname(zmgr->task, "zmgr", zmgr);
	result = isc_ratelimiter_create(mctx, timermgr, zmgr->task,
					&zmgr->rl);
	if (result != ISC_R_SUCCESS)
		goto free_task;
	/* default to 20 refresh queries / notifies per second. */
	isc_interval_set(&interval, 0, 1000000000/2);
	result = isc_ratelimiter_setinterval(zmgr->rl, &interval);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);
	isc_ratelimiter_setpertic(zmgr->rl, 10);

	zmgr->iolimit = 1;
	zmgr->ioactive = 0;
	ISC_LIST_INIT(zmgr->high);
	ISC_LIST_INIT(zmgr->low);

	result = isc_mutex_init(&zmgr->iolock);
	if (result != ISC_R_SUCCESS)
		goto free_rl;

	zmgr->magic = ZONEMGR_MAGIC;

	*zmgrp = zmgr;
	return (ISC_R_SUCCESS);

#if 0
 free_iolock:
	DESTROYLOCK(&zmgr->iolock);
#endif
 free_rl:
	isc_ratelimiter_detach(&zmgr->rl);
 free_task:
	isc_task_detach(&zmgr->task);
 free_taskpool:
	isc_taskpool_destroy(&zmgr->zonetasks);
 free_rwlock:
	isc_rwlock_destroy(&zmgr->rwlock);
 free_mem:
	isc_mem_put(zmgr->mctx, zmgr, sizeof(*zmgr));
	isc_mem_detach(&mctx);
	return (result);
}

isc_result_t
dns_zonemgr_managezone(dns_zonemgr_t *zmgr, dns_zone_t *zone) {
	isc_result_t result;

	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(DNS_ZONEMGR_VALID(zmgr));

	RWLOCK(&zmgr->rwlock, isc_rwlocktype_write);
	LOCK_ZONE(zone);
	REQUIRE(zone->task == NULL);
	REQUIRE(zone->timer == NULL);
	REQUIRE(zone->zmgr == NULL);

	isc_taskpool_gettask(zmgr->zonetasks,
			     dns_name_hash(dns_zone_getorigin(zone),
					   ISC_FALSE),
			     &zone->task);

	/*
	 * Set the task name.  The tag will arbitrarily point to one
	 * of the zones sharing the task (in practice, the one
	 * to be managed last).
	 */
	isc_task_setname(zone->task, "zone", zone);

	result = isc_timer_create(zmgr->timermgr, isc_timertype_inactive,
				  NULL, NULL,
				  zone->task, zone_timer, zone,
				  &zone->timer);
	if (result != ISC_R_SUCCESS)
		goto cleanup_task;
	/*
	 * The timer "holds" a iref.
	 */
	zone->irefs++;
	INSIST(zone->irefs != 0);

	ISC_LIST_APPEND(zmgr->zones, zone, link);
	zone->zmgr = zmgr;
	zmgr->refs++;

	goto unlock;

 cleanup_task:
	isc_task_detach(&zone->task);

 unlock:
	UNLOCK_ZONE(zone);
	RWUNLOCK(&zmgr->rwlock, isc_rwlocktype_write);
	return (result);
}

void
dns_zonemgr_releasezone(dns_zonemgr_t *zmgr, dns_zone_t *zone) {
	isc_boolean_t free_now = ISC_FALSE;

	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(DNS_ZONEMGR_VALID(zmgr));
	REQUIRE(zone->zmgr == zmgr);

	RWLOCK(&zmgr->rwlock, isc_rwlocktype_write);
	LOCK_ZONE(zone);

	ISC_LIST_UNLINK(zmgr->zones, zone, link);
	zone->zmgr = NULL;
	zmgr->refs--;
	if (zmgr->refs == 0)
		free_now = ISC_TRUE;

	UNLOCK_ZONE(zone);
	RWUNLOCK(&zmgr->rwlock, isc_rwlocktype_write);

	if (free_now)
		zonemgr_free(zmgr);
	ENSURE(zone->zmgr == NULL);
}

void
dns_zonemgr_attach(dns_zonemgr_t *source, dns_zonemgr_t **target) {
	REQUIRE(DNS_ZONEMGR_VALID(source));
	REQUIRE(target != NULL && *target == NULL);

	RWLOCK(&source->rwlock, isc_rwlocktype_write);
	REQUIRE(source->refs > 0);
	source->refs++;
	INSIST(source->refs > 0);
	RWUNLOCK(&source->rwlock, isc_rwlocktype_write);
	*target = source;
}

void
dns_zonemgr_detach(dns_zonemgr_t **zmgrp) {
	dns_zonemgr_t *zmgr;
	isc_boolean_t free_now = ISC_FALSE;

	REQUIRE(zmgrp != NULL);
	zmgr = *zmgrp;
	REQUIRE(DNS_ZONEMGR_VALID(zmgr));

	RWLOCK(&zmgr->rwlock, isc_rwlocktype_write);
	zmgr->refs--;
	if (zmgr->refs == 0)
		free_now = ISC_TRUE;
	RWUNLOCK(&zmgr->rwlock, isc_rwlocktype_write);

	if (free_now)
		zonemgr_free(zmgr);
}

isc_result_t
dns_zonemgr_forcemaint(dns_zonemgr_t *zmgr) {
	dns_zone_t *p;

	REQUIRE(DNS_ZONEMGR_VALID(zmgr));

	RWLOCK(&zmgr->rwlock, isc_rwlocktype_read);
	for (p = ISC_LIST_HEAD(zmgr->zones);
	     p != NULL;
	     p = ISC_LIST_NEXT(p, link))
	{
		dns_zone_maintenance(p);
	}
	RWUNLOCK(&zmgr->rwlock, isc_rwlocktype_read);

	/*
	 * Recent configuration changes may have increased the
	 * amount of available transfers quota.  Make sure any
	 * transfers currently blocked on quota get started if
	 * possible.
	 */
	RWLOCK(&zmgr->rwlock, isc_rwlocktype_write);
	zmgr_resume_xfrs(zmgr, ISC_TRUE);
	RWUNLOCK(&zmgr->rwlock, isc_rwlocktype_write);
	return (ISC_R_SUCCESS);
}

void
dns_zonemgr_resumexfrs(dns_zonemgr_t *zmgr) {

	REQUIRE(DNS_ZONEMGR_VALID(zmgr));

	RWLOCK(&zmgr->rwlock, isc_rwlocktype_write);
	zmgr_resume_xfrs(zmgr, ISC_TRUE);
	RWUNLOCK(&zmgr->rwlock, isc_rwlocktype_write);
}

void
dns_zonemgr_shutdown(dns_zonemgr_t *zmgr) {
	REQUIRE(DNS_ZONEMGR_VALID(zmgr));

	isc_ratelimiter_shutdown(zmgr->rl);

	if (zmgr->task != NULL)
		isc_task_destroy(&zmgr->task);
	if (zmgr->zonetasks != NULL)
		isc_taskpool_destroy(&zmgr->zonetasks);
}

static void
zonemgr_free(dns_zonemgr_t *zmgr) {
	isc_mem_t *mctx;

	INSIST(zmgr->refs == 0);
	INSIST(ISC_LIST_EMPTY(zmgr->zones));

	zmgr->magic = 0;

	DESTROYLOCK(&zmgr->iolock);
	isc_ratelimiter_detach(&zmgr->rl);

	isc_rwlock_destroy(&zmgr->rwlock);
	mctx = zmgr->mctx;
	isc_mem_put(zmgr->mctx, zmgr, sizeof(*zmgr));
	isc_mem_detach(&mctx);
}

void
dns_zonemgr_settransfersin(dns_zonemgr_t *zmgr, isc_uint32_t value) {
	REQUIRE(DNS_ZONEMGR_VALID(zmgr));

	zmgr->transfersin = value;
}

isc_uint32_t
dns_zonemgr_getttransfersin(dns_zonemgr_t *zmgr) {
	REQUIRE(DNS_ZONEMGR_VALID(zmgr));

	return (zmgr->transfersin);
}

void
dns_zonemgr_settransfersperns(dns_zonemgr_t *zmgr, isc_uint32_t value) {
	REQUIRE(DNS_ZONEMGR_VALID(zmgr));

	zmgr->transfersperns = value;
}

isc_uint32_t
dns_zonemgr_getttransfersperns(dns_zonemgr_t *zmgr) {
	REQUIRE(DNS_ZONEMGR_VALID(zmgr));

	return (zmgr->transfersperns);
}

/*
 * Try to start a new incoming zone transfer to fill a quota
 * slot that was just vacated.
 *
 * Requires:
 *	The zone manager is locked by the caller.
 */
static void
zmgr_resume_xfrs(dns_zonemgr_t *zmgr, isc_boolean_t multi) {
	dns_zone_t *zone;
	dns_zone_t *next;

	for (zone = ISC_LIST_HEAD(zmgr->waiting_for_xfrin);
	     zone != NULL;
	     zone = next)
	{
		isc_result_t result;
		next = ISC_LIST_NEXT(zone, statelink);
		result = zmgr_start_xfrin_ifquota(zmgr, zone);
		if (result == ISC_R_SUCCESS) {
			if (multi)
				continue;
			/*
			 * We successfully filled the slot.  We're done.
			 */
			break;
		} else if (result == ISC_R_QUOTA) {
			/*
			 * Not enough quota.  This is probably the per-server
			 * quota, because we usually get called when a unit of
			 * global quota has just been freed.  Try the next
			 * zone, it may succeed if it uses another master.
			 */
			continue;
		} else {
			dns_zone_log(zone, ISC_LOG_DEBUG(1),
				     "starting zone transfer: %s",
				     isc_result_totext(result));
			break;
		}
	}
}

/*
 * Try to start an incoming zone transfer for 'zone', quota permitting.
 *
 * Requires:
 *	The zone manager is locked by the caller.
 *
 * Returns:
 *	ISC_R_SUCCESS	There was enough quota and we attempted to
 *			start a transfer.  zone_xfrdone() has been or will
 *			be called.
 *	ISC_R_QUOTA	Not enough quota.
 *	Others		Failure.
 */
static isc_result_t
zmgr_start_xfrin_ifquota(dns_zonemgr_t *zmgr, dns_zone_t *zone) {
	dns_peer_t *peer = NULL;
	isc_netaddr_t masterip;
	isc_uint32_t nxfrsin, nxfrsperns;
	dns_zone_t *x;
	isc_uint32_t maxtransfersin, maxtransfersperns;
	isc_event_t *e;

	/*
	 * Find any configured information about the server we'd
	 * like to transfer this zone from.
	 */
	isc_netaddr_fromsockaddr(&masterip, &zone->masteraddr);
	(void)dns_peerlist_peerbyaddr(zone->view->peers,
				      &masterip, &peer);

	/*
	 * Determine the total maximum number of simultaneous
	 * transfers allowed, and the maximum for this specific
	 * master.
	 */
	maxtransfersin = zmgr->transfersin;
	maxtransfersperns = zmgr->transfersperns;
	if (peer != NULL)
		(void)dns_peer_gettransfers(peer, &maxtransfersperns);

	/*
	 * Count the total number of transfers that are in progress,
	 * and the number of transfers in progress from this master.
	 * We linearly scan a list of all transfers; if this turns
	 * out to be too slow, we could hash on the master address.
	 */
	nxfrsin = nxfrsperns = 0;
	for (x = ISC_LIST_HEAD(zmgr->xfrin_in_progress);
	     x != NULL;
	     x = ISC_LIST_NEXT(x, statelink))
	{
		isc_netaddr_t xip;
		isc_netaddr_fromsockaddr(&xip, &x->masteraddr);
		nxfrsin++;
		if (isc_netaddr_equal(&xip, &masterip))
			nxfrsperns++;
	}

	/* Enforce quota. */
	if (nxfrsin >= maxtransfersin)
		return (ISC_R_QUOTA);

	if (nxfrsperns >= maxtransfersperns)
		return (ISC_R_QUOTA);

	/*
	 * We have sufficient quota.  Move the zone to the "xfrin_in_progress"
	 * list and send it an event to let it start the actual transfer in the
	 * context of its own task.
	 */
	e = isc_event_allocate(zmgr->mctx, zmgr,
			       DNS_EVENT_ZONESTARTXFRIN,
			       got_transfer_quota, zone,
			       sizeof(isc_event_t));
	if (e == NULL)
		return (ISC_R_NOMEMORY);

	LOCK_ZONE(zone);
	INSIST(zone->statelist == &zmgr->waiting_for_xfrin);
	ISC_LIST_UNLINK(zmgr->waiting_for_xfrin, zone, statelink);
	ISC_LIST_APPEND(zmgr->xfrin_in_progress, zone, statelink);
	zone->statelist = &zmgr->xfrin_in_progress;
	isc_task_send(zone->task, &e);
	dns_zone_log(zone, ISC_LOG_INFO, "Transfer started.");
	UNLOCK_ZONE(zone);

	return (ISC_R_SUCCESS);
}

void
dns_zonemgr_setiolimit(dns_zonemgr_t *zmgr, isc_uint32_t iolimit) {

	REQUIRE(DNS_ZONEMGR_VALID(zmgr));
	REQUIRE(iolimit > 0);

	zmgr->iolimit = iolimit;
}

isc_uint32_t
dns_zonemgr_getiolimit(dns_zonemgr_t *zmgr) {

	REQUIRE(DNS_ZONEMGR_VALID(zmgr));

	return (zmgr->iolimit);
}

/*
 * Get permission to request a file handle from the OS.
 * An event will be sent to action when one is available.
 * There are two queues available (high and low), the high
 * queue will be serviced before the low one.
 *
 * zonemgr_putio() must be called after the event is delivered to
 * 'action'.
 */

static isc_result_t
zonemgr_getio(dns_zonemgr_t *zmgr, isc_boolean_t high,
	      isc_task_t *task, isc_taskaction_t action, void *arg,
	      dns_io_t **iop)
{
	dns_io_t *io;
	isc_boolean_t queue;

	REQUIRE(DNS_ZONEMGR_VALID(zmgr));
	REQUIRE(iop != NULL && *iop == NULL);

	io = isc_mem_get(zmgr->mctx, sizeof(*io));
	if (io == NULL)
		return (ISC_R_NOMEMORY);
	io->event = isc_event_allocate(zmgr->mctx, task, DNS_EVENT_IOREADY,
				       action, arg, sizeof(*io->event));
	if (io->event == NULL) {
		isc_mem_put(zmgr->mctx, io, sizeof(*io));
		return (ISC_R_NOMEMORY);
	}
	io->zmgr = zmgr;
	io->high = high;
	io->task = NULL;
	isc_task_attach(task, &io->task);
	ISC_LINK_INIT(io, link);
	io->magic = IO_MAGIC;

	LOCK(&zmgr->iolock);
	zmgr->ioactive++;
	queue = ISC_TF(zmgr->ioactive > zmgr->iolimit);
	if (queue) {
		if (io->high)
			ISC_LIST_APPEND(zmgr->high, io, link);
		else
			ISC_LIST_APPEND(zmgr->low, io, link);
	}
	UNLOCK(&zmgr->iolock);
	*iop = io;

	if (!queue) {
		isc_task_send(io->task, &io->event);
	}
	return (ISC_R_SUCCESS);
}

static void
zonemgr_putio(dns_io_t **iop) {
	dns_io_t *io;
	dns_io_t *next;
	dns_zonemgr_t *zmgr;

	REQUIRE(iop != NULL);
	io = *iop;
	REQUIRE(DNS_IO_VALID(io));

	*iop = NULL;

	INSIST(!ISC_LINK_LINKED(io, link));
	INSIST(io->event == NULL);

	zmgr = io->zmgr;
	isc_task_detach(&io->task);
	io->magic = 0;
	isc_mem_put(zmgr->mctx, io, sizeof(*io));

	LOCK(&zmgr->iolock);
	INSIST(zmgr->ioactive > 0);
	zmgr->ioactive--;
	next = HEAD(zmgr->high);
	if (next == NULL)
		next = HEAD(zmgr->low);
	if (next != NULL) {
		if (next->high)
			ISC_LIST_UNLINK(zmgr->high, next, link);
		else
			ISC_LIST_UNLINK(zmgr->low, next, link);
		INSIST(next->event != NULL);
	}
	UNLOCK(&zmgr->iolock);
	if (next != NULL)
		isc_task_send(next->task, &next->event);
}

static void
zonemgr_cancelio(dns_io_t *io) {
	isc_boolean_t send_event = ISC_FALSE;

	REQUIRE(DNS_IO_VALID(io));

	/*
	 * If we are queued to be run then dequeue.
	 */
	LOCK(&io->zmgr->iolock);
	if (ISC_LINK_LINKED(io, link)) {
		if (io->high)
			ISC_LIST_UNLINK(io->zmgr->high, io, link);
		else
			ISC_LIST_UNLINK(io->zmgr->low, io, link);

		send_event = ISC_TRUE;
		INSIST(io->event != NULL);
	}
	UNLOCK(&io->zmgr->iolock);
	if (send_event) {
		io->event->ev_attributes |= ISC_EVENTATTR_CANCELED;
		isc_task_send(io->task, &io->event);
	}
}

static void
zone_saveunique(dns_zone_t *zone, const char *path, const char *templat) {
	char *buf;
	int buflen;
	isc_result_t result;

	buflen = strlen(path) + strlen(templat) + 2;

	buf = isc_mem_get(zone->mctx, buflen);
	if (buf == NULL)
		return;

	result = isc_file_template(path, templat, buf, buflen);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	result = isc_file_renameunique(path, buf);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	dns_zone_log(zone, ISC_LOG_WARNING, "saved '%s' as '%s'",
		     path, buf);

 cleanup:
	isc_mem_put(zone->mctx, buf, buflen);
}

#if 0
/* Hook for ondestroy notification from a database. */

static void
dns_zonemgr_dbdestroyed(isc_task_t *task, isc_event_t *event) {
	dns_db_t *db = event->sender;
	UNUSED(task);

	isc_event_free(&event);

	isc_log_write(dns_lctx, DNS_LOGCATEGORY_GENERAL,
		      DNS_LOGMODULE_ZONE, ISC_LOG_DEBUG(3),
		      "database (%p) destroyed", (void*) db);
}
#endif

void
dns_zonemgr_setserialqueryrate(dns_zonemgr_t *zmgr, unsigned int value) {
	isc_interval_t interval;
	isc_uint32_t s, ns;
	isc_uint32_t pertic;
	isc_result_t result;

	REQUIRE(DNS_ZONEMGR_VALID(zmgr));

	if (value == 0)
		value = 1;

	if (value == 1) {
		s = 1;
		ns = 0;
		pertic = 1;
	} else if (value <= 10) {
		s = 0;
		ns = 1000000000 / value;
		pertic = 1;
	} else {
		s = 0;
		ns = (1000000000 / value) * 10;
		pertic = 10;
	}

	isc_interval_set(&interval, s, ns);
	result = isc_ratelimiter_setinterval(zmgr->rl, &interval);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);
	isc_ratelimiter_setpertic(zmgr->rl, pertic);

	zmgr->serialqueryrate = value;
}

unsigned int
dns_zonemgr_getserialqueryrate(dns_zonemgr_t *zmgr) {
	REQUIRE(DNS_ZONEMGR_VALID(zmgr));

	return (zmgr->serialqueryrate);
}

void
dns_zone_forcereload(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));

	if (zone->type == dns_zone_master)
		return;

	LOCK_ZONE(zone);
	DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_FORCEXFER);
	UNLOCK_ZONE(zone);
	dns_zone_refresh(zone);
}

isc_boolean_t
dns_zone_isforced(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));

	return (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_FORCEXFER));
}

isc_result_t
dns_zone_setstatistics(dns_zone_t *zone, isc_boolean_t on) {
	isc_result_t result = ISC_R_SUCCESS;

	LOCK_ZONE(zone);
	if (on) {
		if (zone->counters != NULL)
			goto done;
		result = dns_stats_alloccounters(zone->mctx, &zone->counters);
	} else {
		if (zone->counters == NULL)
			goto done;
		dns_stats_freecounters(zone->mctx, &zone->counters);
	}
 done:
	UNLOCK_ZONE(zone);
	return (result);
}

isc_uint64_t *
dns_zone_getstatscounters(dns_zone_t *zone) {
	return (zone->counters);
}

void
dns_zone_dialup(dns_zone_t *zone) {

	REQUIRE(DNS_ZONE_VALID(zone));

	zone_debuglog(zone, "dns_zone_dialup", 3,
		      "notify = %d, refresh = %d",
		      DNS_ZONE_FLAG(zone, DNS_ZONEFLG_DIALNOTIFY),
		      DNS_ZONE_FLAG(zone, DNS_ZONEFLG_DIALREFRESH));

	if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_DIALNOTIFY))
		dns_zone_notify(zone);
	if (zone->type != dns_zone_master &&
	    DNS_ZONE_FLAG(zone, DNS_ZONEFLG_DIALREFRESH))
		dns_zone_refresh(zone);
}

void
dns_zone_setdialup(dns_zone_t *zone, dns_dialuptype_t dialup) {
	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_DIALNOTIFY |
			 DNS_ZONEFLG_DIALREFRESH |
			 DNS_ZONEFLG_NOREFRESH);
	switch (dialup) {
	case dns_dialuptype_no:
		break;
	case dns_dialuptype_yes:
		DNS_ZONE_SETFLAG(zone,  (DNS_ZONEFLG_DIALNOTIFY |
				 DNS_ZONEFLG_DIALREFRESH |
				 DNS_ZONEFLG_NOREFRESH));
		break;
	case dns_dialuptype_notify:
		DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_DIALNOTIFY);
		break;
	case dns_dialuptype_notifypassive:
		DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_DIALNOTIFY);
		DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_NOREFRESH);
		break;
	case dns_dialuptype_refresh:
		DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_DIALREFRESH);
		DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_NOREFRESH);
		break;
	case dns_dialuptype_passive:
		DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_NOREFRESH);
		break;
	default:
		INSIST(0);
	}
	UNLOCK_ZONE(zone);
}

isc_result_t
dns_zone_setkeydirectory(dns_zone_t *zone, const char *directory) {
	isc_result_t result = ISC_R_SUCCESS;

	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	result = dns_zone_setstring(zone, &zone->keydirectory, directory);
	UNLOCK_ZONE(zone);

	return (result);
}

const char *
dns_zone_getkeydirectory(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));

	return (zone->keydirectory);
}

unsigned int
dns_zonemgr_getcount(dns_zonemgr_t *zmgr, int state) {
	dns_zone_t *zone;
	unsigned int count = 0;

	REQUIRE(DNS_ZONEMGR_VALID(zmgr));

	RWLOCK(&zmgr->rwlock, isc_rwlocktype_read);
	switch (state) {
	case DNS_ZONESTATE_XFERRUNNING:
		for (zone = ISC_LIST_HEAD(zmgr->xfrin_in_progress);
		     zone != NULL;
		     zone = ISC_LIST_NEXT(zone, statelink))
			count++;
		break;
	case DNS_ZONESTATE_XFERDEFERRED:
		for (zone = ISC_LIST_HEAD(zmgr->waiting_for_xfrin);
		     zone != NULL;
		     zone = ISC_LIST_NEXT(zone, statelink))
			count++;
		break;
	case DNS_ZONESTATE_SOAQUERY:
		for (zone = ISC_LIST_HEAD(zmgr->zones);
		     zone != NULL;
		     zone = ISC_LIST_NEXT(zone, link))
			if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_REFRESH))
				count++;
		break;
	case DNS_ZONESTATE_ANY:
		for (zone = ISC_LIST_HEAD(zmgr->zones);
		     zone != NULL;
		     zone = ISC_LIST_NEXT(zone, link)) {
			dns_view_t *view = zone->view;
			if (view != NULL && strcmp(view->name, "_bind") == 0)
				continue;
			count++;
		}
		break;
	default:
		INSIST(0);
	}

	RWUNLOCK(&zmgr->rwlock, isc_rwlocktype_read);

	return (count);
}

isc_result_t
dns_zone_checknames(dns_zone_t *zone, dns_name_t *name, dns_rdata_t *rdata) {
	isc_boolean_t ok = ISC_TRUE;
	isc_boolean_t fail = ISC_FALSE;
	char namebuf[DNS_NAME_FORMATSIZE];
	char namebuf2[DNS_NAME_FORMATSIZE];
	char typebuf[DNS_RDATATYPE_FORMATSIZE];
	int level = ISC_LOG_WARNING;
	dns_name_t bad;

	REQUIRE(DNS_ZONE_VALID(zone));

	if (!DNS_ZONE_OPTION(zone, DNS_ZONEOPT_CHECKNAMES))
		return (ISC_R_SUCCESS);

	if (DNS_ZONE_OPTION(zone, DNS_ZONEOPT_CHECKNAMESFAIL)) {
		level = ISC_LOG_ERROR;
		fail = ISC_TRUE;
	}

	ok = dns_rdata_checkowner(name, rdata->rdclass, rdata->type, ISC_TRUE);
	if (!ok) {
		dns_name_format(name, namebuf, sizeof(namebuf));
		dns_rdatatype_format(rdata->type, typebuf, sizeof(typebuf));
		dns_zone_log(zone, level, "%s/%s: %s", namebuf, typebuf,
			     dns_result_totext(DNS_R_BADOWNERNAME));
		if (fail)
			return (DNS_R_BADOWNERNAME);
	}

	dns_name_init(&bad, NULL);
	ok = dns_rdata_checknames(rdata, name, &bad);
	if (!ok) {
		dns_name_format(name, namebuf, sizeof(namebuf));
		dns_name_format(&bad, namebuf2, sizeof(namebuf2));
		dns_rdatatype_format(rdata->type, typebuf, sizeof(typebuf));
		dns_zone_log(zone, level, "%s/%s: %s: %s ", namebuf, typebuf,
			     namebuf2, dns_result_totext(DNS_R_BADNAME));
		if (fail)
			return (DNS_R_BADNAME);
	}

	return (ISC_R_SUCCESS);
}

void
dns_zone_setcheckmx(dns_zone_t *zone, dns_checkmxfunc_t checkmx) {
	REQUIRE(DNS_ZONE_VALID(zone));
	zone->checkmx = checkmx;
}

void
dns_zone_setchecksrv(dns_zone_t *zone, dns_checksrvfunc_t checksrv) {
	REQUIRE(DNS_ZONE_VALID(zone));
	zone->checksrv = checksrv;
}

void
dns_zone_setcheckns(dns_zone_t *zone, dns_checknsfunc_t checkns) {
	REQUIRE(DNS_ZONE_VALID(zone));
	zone->checkns = checkns;
}

void
dns_zone_setisself(dns_zone_t *zone, dns_isselffunc_t isself, void *arg) {
	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	zone->isself = isself;
	zone->isselfarg = arg;
	UNLOCK_ZONE(zone);
}

void
dns_zone_setnotifydelay(dns_zone_t *zone, isc_uint32_t delay) {
	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	zone->notifydelay = delay;
	UNLOCK_ZONE(zone);
}

isc_uint32_t
dns_zone_getnotifydelay(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));

	return (zone->notifydelay);
}
