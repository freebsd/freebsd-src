/*
 * Copyright (C) 2004-2015  Internet Systems Consortium, Inc. ("ISC")
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

/*! \file */

#include <config.h>
#include <errno.h>

#include <isc/file.h>
#include <isc/hex.h>
#include <isc/mutex.h>
#include <isc/pool.h>
#include <isc/print.h>
#include <isc/random.h>
#include <isc/ratelimiter.h>
#include <isc/refcount.h>
#include <isc/rwlock.h>
#include <isc/serial.h>
#include <isc/stats.h>
#include <isc/stdtime.h>
#include <isc/strerror.h>
#include <isc/string.h>
#include <isc/taskpool.h>
#include <isc/thread.h>
#include <isc/timer.h>
#include <isc/util.h>

#include <dns/acache.h>
#include <dns/acl.h>
#include <dns/adb.h>
#include <dns/callbacks.h>
#include <dns/db.h>
#include <dns/dbiterator.h>
#include <dns/dnssec.h>
#include <dns/ds.h>
#include <dns/events.h>
#include <dns/journal.h>
#include <dns/keydata.h>
#include <dns/keytable.h>
#include <dns/keyvalues.h>
#include <dns/log.h>
#include <dns/master.h>
#include <dns/masterdump.h>
#include <dns/message.h>
#include <dns/name.h>
#include <dns/nsec.h>
#include <dns/nsec3.h>
#include <dns/peer.h>
#include <dns/private.h>
#include <dns/rbt.h>
#include <dns/rcode.h>
#include <dns/rdata.h>
#include <dns/rdataclass.h>
#include <dns/rdatalist.h>
#include <dns/rdataset.h>
#include <dns/rdatasetiter.h>
#include <dns/rdatastruct.h>
#include <dns/rdatatype.h>
#include <dns/request.h>
#include <dns/resolver.h>
#include <dns/result.h>
#include <dns/rriterator.h>
#include <dns/soa.h>
#include <dns/ssu.h>
#include <dns/stats.h>
#include <dns/time.h>
#include <dns/tsig.h>
#include <dns/update.h>
#include <dns/xfrin.h>
#include <dns/zone.h>
#include <dns/zt.h>

#include <dst/dst.h>

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

#define NSEC3REMOVE(x) (((x) & DNS_NSEC3FLAG_REMOVE) != 0)

/*%
 * Key flags
 */
#define REVOKE(x) ((dst_key_flags(x) & DNS_KEYFLAG_REVOKE) != 0)
#define KSK(x) ((dst_key_flags(x) & DNS_KEYFLAG_KSK) != 0)
#define ALG(x) dst_key_alg(x)

/*
 * Default values.
 */
#define DNS_DEFAULT_IDLEIN 3600		/*%< 1 hour */
#define DNS_DEFAULT_IDLEOUT 3600	/*%< 1 hour */
#define MAX_XFER_TIME (2*3600)		/*%< Documented default is 2 hours */
#define RESIGN_DELAY 3600		/*%< 1 hour */

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
typedef ISC_LIST(dns_forward_t) dns_forwardlist_t;
typedef struct dns_io dns_io_t;
typedef ISC_LIST(dns_io_t) dns_iolist_t;
typedef struct dns_signing dns_signing_t;
typedef ISC_LIST(dns_signing_t) dns_signinglist_t;
typedef struct dns_nsec3chain dns_nsec3chain_t;
typedef ISC_LIST(dns_nsec3chain_t) dns_nsec3chainlist_t;
typedef struct dns_keyfetch dns_keyfetch_t;
typedef struct dns_asyncload dns_asyncload_t;

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
#define TRYLOCK_ZONE(result, z) \
	do { \
	      result = isc_mutex_trylock(&(z)->lock); \
	      if (result == ISC_R_SUCCESS) {  \
		     INSIST((z)->locked == ISC_FALSE); \
		     (z)->locked = ISC_TRUE; \
	      } \
	} while (0)
#else
#define LOCK_ZONE(z) LOCK(&(z)->lock)
#define UNLOCK_ZONE(z) UNLOCK(&(z)->lock)
#define LOCKED_ZONE(z) ISC_TRUE
#define TRYLOCK_ZONE(result, z) \
	do { result = isc_mutex_trylock(&(z)->lock); } while (0)
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
	isc_time_t		resigntime;
	isc_time_t		keywarntime;
	isc_time_t		signingtime;
	isc_time_t		nsec3chaintime;
	isc_time_t		refreshkeytime;
	isc_uint32_t		refreshkeyinterval;
	isc_uint32_t		refreshkeycount;
	isc_uint32_t		refresh;
	isc_uint32_t		retry;
	isc_uint32_t		expire;
	isc_uint32_t		minimum;
	isc_stdtime_t		key_expiry;
	isc_stdtime_t		log_key_expired_timer;
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
	dns_name_t		**notifykeynames;
	unsigned int		notifycnt;
	isc_sockaddr_t		notifyfrom;
	isc_task_t		*task;
	isc_task_t		*loadtask;
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
	dns_acl_t		*queryon_acl;
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
	isc_uint32_t		sigresigninginterval;
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
	 * Statistics counters about zone management.
	 */
	isc_stats_t		*stats;
	/*%
	 * Optional per-zone statistics counters.  Counted outside of this
	 * module.
	 */
	dns_zonestat_level_t	statlevel;
	isc_boolean_t		requeststats_on;
	isc_stats_t		*requeststats;
	dns_stats_t		*rcvquerystats;
	isc_uint32_t		notifydelay;
	dns_isselffunc_t	isself;
	void			*isselfarg;

	char *			strnamerd;
	char *			strname;
	char *			strrdclass;
	char *			strviewname;

	/*%
	 * Serial number for deferred journal compaction.
	 */
	isc_uint32_t		compact_serial;
	/*%
	 * Keys that are signing the zone for the first time.
	 */
	dns_signinglist_t	signing;
	dns_nsec3chainlist_t	nsec3chain;
	/*%
	 * Signing / re-signing quantum stopping parameters.
	 */
	isc_uint32_t		signatures;
	isc_uint32_t		nodes;
	dns_rdatatype_t		privatetype;

	/*%
	 * Autosigning/key-maintenance options
	 */
	isc_uint32_t		keyopts;

	/*%
	 * True if added by "rndc addzone"
	 */
	isc_boolean_t           added;

	/*%
	 * whether this is a response policy zone
	 */
	isc_boolean_t           is_rpz;

	/*%
	 * Serial number update method.
	 */
	dns_updatemethod_t	updatemethod;

	/*%
	 * whether ixfr is requested
	 */
	isc_boolean_t		requestixfr;

	/*%
	 * Outstanding forwarded UPDATE requests.
	 */
	dns_forwardlist_t	forwards;

	dns_zone_t		*raw;
	dns_zone_t		*secure;

	isc_boolean_t		sourceserialset;
	isc_uint32_t		sourceserial;

	/*%
	 * maximum zone ttl
	 */
	dns_ttl_t		maxttl;

	/*
	 * Inline zone signing state.
	 */
	dns_diff_t		rss_diff;
	isc_eventlist_t		rss_events;
	dns_dbversion_t		*rss_newver;
	dns_dbversion_t		*rss_oldver;
	dns_db_t		*rss_db;
	dns_zone_t		*rss_raw;
	isc_event_t		*rss_event;
	dns_update_state_t      *rss_state;
};

typedef struct {
	dns_diff_t	*diff;
	isc_boolean_t	offline;
} zonediff_t;

#define zonediff_init(z, d) \
	do { \
		zonediff_t *_z = (z); \
		(_z)->diff = (d); \
		(_z)->offline = ISC_FALSE; \
	} while (0)

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
#define DNS_ZONEFLG_FORCEXFER	0x00008000U	/*%< Force a zone xfer */
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
#define DNS_ZONEFLG_LOADPENDING	0x10000000U	/*%< Loading scheduled */
#define DNS_ZONEFLG_NODELAY	0x20000000U
#define DNS_ZONEFLG_SENDSECURE  0x40000000U
#define DNS_ZONEFLG_NEEDSTARTUPNOTIFY 0x80000000U /*%< need to send out notify
						   *   due to the zone just
						   *   being loaded for the
						   *   first time.  */

#define DNS_ZONE_OPTION(z,o) (((z)->options & (o)) != 0)
#define DNS_ZONEKEY_OPTION(z,o) (((z)->keyopts & (o)) != 0)

/* Flags for zone_load() */
#define DNS_ZONELOADFLAG_NOSTAT	0x00000001U	/* Do not stat() master files */
#define DNS_ZONELOADFLAG_THAW	0x00000002U	/* Thaw the zone on successful
						   load. */

#define UNREACH_CHACHE_SIZE	10U
#define UNREACH_HOLD_TIME	600	/* 10 minutes */

#define CHECK(op) \
	do { result = (op); \
		if (result != ISC_R_SUCCESS) goto failure; \
	} while (0)

struct dns_unreachable {
	isc_sockaddr_t	remote;
	isc_sockaddr_t	local;
	isc_uint32_t	expire;
	isc_uint32_t	last;
	isc_uint32_t	count;
};

struct dns_zonemgr {
	unsigned int		magic;
	isc_mem_t *		mctx;
	int			refs;		/* Locked by rwlock */
	isc_taskmgr_t *		taskmgr;
	isc_timermgr_t *	timermgr;
	isc_socketmgr_t *	socketmgr;
	isc_taskpool_t *	zonetasks;
	isc_taskpool_t *	loadtasks;
	isc_task_t *		task;
	isc_pool_t *		mctxpool;
	isc_ratelimiter_t *	notifyrl;
	isc_ratelimiter_t *	refreshrl;
	isc_ratelimiter_t *	startupnotifyrl;
	isc_ratelimiter_t *	startuprefreshrl;
	isc_rwlock_t		rwlock;
	isc_mutex_t		iolock;
	isc_rwlock_t		urlock;

	/* Locked by rwlock. */
	dns_zonelist_t		zones;
	dns_zonelist_t		waiting_for_xfrin;
	dns_zonelist_t		xfrin_in_progress;

	/* Configuration data. */
	isc_uint32_t		transfersin;
	isc_uint32_t		transfersperns;
	unsigned int		notifyrate;
	unsigned int		startupnotifyrate;
	unsigned int		serialqueryrate;
	unsigned int		startupserialqueryrate;

	/* Locked by iolock */
	isc_uint32_t		iolimit;
	isc_uint32_t		ioactive;
	dns_iolist_t		high;
	dns_iolist_t		low;

	/* Locked by urlock. */
	/* LRU cache */
	struct dns_unreachable	unreachable[UNREACH_CHACHE_SIZE];
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
	dns_tsigkey_t		*key;
	ISC_LINK(dns_notify_t)	link;
	isc_event_t		*event;
};

#define DNS_NOTIFY_NOSOA	0x0001U
#define DNS_NOTIFY_STARTUP	0x0002U

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
	unsigned int		options;
	ISC_LINK(dns_forward_t)	link;
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

/*%
 *	Hold state for when we are signing a zone with a new
 *	DNSKEY as result of an update.
 */
struct dns_signing {
	unsigned int		magic;
	dns_db_t		*db;
	dns_dbiterator_t	*dbiterator;
	dns_secalg_t		algorithm;
	isc_uint16_t		keyid;
	isc_boolean_t		delete;
	isc_boolean_t		done;
	ISC_LINK(dns_signing_t)	link;
};

struct dns_nsec3chain {
	unsigned int			magic;
	dns_db_t			*db;
	dns_dbiterator_t		*dbiterator;
	dns_rdata_nsec3param_t		nsec3param;
	unsigned char			salt[255];
	isc_boolean_t			done;
	isc_boolean_t			seen_nsec;
	isc_boolean_t			delete_nsec;
	isc_boolean_t			save_delete_nsec;
	ISC_LINK(dns_nsec3chain_t)	link;
};
/*%<
 * 'dbiterator' contains a iterator for the database.  If we are creating
 * a NSEC3 chain only the non-NSEC3 nodes will be iterated.  If we are
 * removing a NSEC3 chain then both NSEC3 and non-NSEC3 nodes will be
 * iterated.
 *
 * 'nsec3param' contains the parameters of the NSEC3 chain being created
 * or removed.
 *
 * 'salt' is buffer space and is referenced via 'nsec3param.salt'.
 *
 * 'seen_nsec' will be set to true if, while iterating the zone to create a
 * NSEC3 chain, a NSEC record is seen.
 *
 * 'delete_nsec' will be set to true if, at the completion of the creation
 * of a NSEC3 chain, 'seen_nsec' is true.  If 'delete_nsec' is true then we
 * are in the process of deleting the NSEC chain.
 *
 * 'save_delete_nsec' is used to store the initial state of 'delete_nsec'
 * so it can be recovered in the event of a error.
 */

struct dns_keyfetch {
	dns_fixedname_t name;
	dns_rdataset_t keydataset;
	dns_rdataset_t dnskeyset;
	dns_rdataset_t dnskeysigset;
	dns_zone_t *zone;
	dns_db_t *db;
	dns_fetch_t *fetch;
};

/*%
 * Hold state for an asynchronous load
 */
struct dns_asyncload {
	dns_zone_t *zone;
	dns_zt_zoneloaded_t loaded;
	void *loaded_arg;
};

#define HOUR 3600
#define DAY (24*HOUR)
#define MONTH (30*DAY)

/*
 * These can be overridden by the -T mkeytimers option on the command
 * line, so that we can test with shorter periods than specified in
 * RFC 5011.
 */
unsigned int dns_zone_mkey_hour = HOUR;
unsigned int dns_zone_mkey_day = DAY;
unsigned int dns_zone_mkey_month = MONTH;

#define SEND_BUFFER_SIZE 2048

static void zone_settimer(dns_zone_t *, isc_time_t *);
static void cancel_refresh(dns_zone_t *);
static void zone_debuglog(dns_zone_t *zone, const char *, int debuglevel,
			  const char *msg, ...) ISC_FORMAT_PRINTF(4, 5);
static void notify_log(dns_zone_t *zone, int level, const char *fmt, ...)
     ISC_FORMAT_PRINTF(3, 4);
static void queue_xfrin(dns_zone_t *zone);
static isc_result_t update_one_rr(dns_db_t *db, dns_dbversion_t *ver,
				  dns_diff_t *diff, dns_diffop_t op,
				  dns_name_t *name, dns_ttl_t ttl,
				  dns_rdata_t *rdata);
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
static void zone_namerd_tostr(dns_zone_t *zone, char *buf, size_t length);
static void zone_name_tostr(dns_zone_t *zone, char *buf, size_t length);
static void zone_rdclass_tostr(dns_zone_t *zone, char *buf, size_t length);
static void zone_viewname_tostr(dns_zone_t *zone, char *buf, size_t length);
static isc_result_t zone_send_secureserial(dns_zone_t *zone,
					   isc_uint32_t serial);

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
static isc_result_t zone_signwithkey(dns_zone_t *zone, dns_secalg_t algorithm,
				     isc_uint16_t keyid, isc_boolean_t delete);
static isc_result_t delete_nsec(dns_db_t *db, dns_dbversion_t *ver,
				dns_dbnode_t *node, dns_name_t *name,
				dns_diff_t *diff);
static void zone_rekey(dns_zone_t *zone);
static isc_result_t zone_send_securedb(dns_zone_t *zone, dns_db_t *db);
static void setrl(isc_ratelimiter_t *rl, unsigned int *rate,
		  unsigned int value);

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

typedef struct nsec3param nsec3param_t;
struct nsec3param {
	unsigned char data[DNS_NSEC3PARAM_BUFFERSIZE + 1];
	unsigned int length;
	isc_boolean_t nsec;
	isc_boolean_t replace;
	ISC_LINK(nsec3param_t)	link;
};
typedef ISC_LIST(nsec3param_t) nsec3paramlist_t;
struct np3event {
	isc_event_t event;
	nsec3param_t params;
};

/*%
 * Increment resolver-related statistics counters.  Zone must be locked.
 */
static inline void
inc_stats(dns_zone_t *zone, isc_statscounter_t counter) {
	if (zone->stats != NULL)
		isc_stats_increment(zone->stats, counter);
}

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
	zone->strnamerd = NULL;
	zone->strname = NULL;
	zone->strrdclass = NULL;
	zone->strviewname = NULL;
	zone->masterfile = NULL;
	zone->masterformat = dns_masterformat_none;
	zone->keydirectory = NULL;
	zone->journalsize = -1;
	zone->journal = NULL;
	zone->rdclass = dns_rdataclass_none;
	zone->type = dns_zone_none;
	zone->flags = 0;
	zone->options = 0;
	zone->keyopts = 0;
	zone->db_argc = 0;
	zone->db_argv = NULL;
	isc_time_settoepoch(&zone->expiretime);
	isc_time_settoepoch(&zone->refreshtime);
	isc_time_settoepoch(&zone->dumptime);
	isc_time_settoepoch(&zone->loadtime);
	zone->notifytime = now;
	isc_time_settoepoch(&zone->resigntime);
	isc_time_settoepoch(&zone->keywarntime);
	isc_time_settoepoch(&zone->signingtime);
	isc_time_settoepoch(&zone->nsec3chaintime);
	isc_time_settoepoch(&zone->refreshkeytime);
	zone->refreshkeyinterval = 0;
	zone->refreshkeycount = 0;
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
	zone->notifykeynames = NULL;
	zone->notifytype = dns_notifytype_yes;
	zone->notifycnt = 0;
	zone->task = NULL;
	zone->loadtask = NULL;
	zone->update_acl = NULL;
	zone->forward_acl = NULL;
	zone->notify_acl = NULL;
	zone->query_acl = NULL;
	zone->queryon_acl = NULL;
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
	zone->log_key_expired_timer = 0;
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
	zone->sigresigninginterval = 7 * 24 * 3600;
	zone->view = NULL;
	zone->acache = NULL;
	zone->checkmx = NULL;
	zone->checksrv = NULL;
	zone->checkns = NULL;
	ISC_LINK_INIT(zone, statelink);
	zone->statelist = NULL;
	zone->stats = NULL;
	zone->requeststats_on = ISC_FALSE;
	zone->statlevel = dns_zonestat_none;
	zone->requeststats = NULL;
	zone->rcvquerystats = NULL;
	zone->notifydelay = 5;
	zone->isself = NULL;
	zone->isselfarg = NULL;
	ISC_LIST_INIT(zone->signing);
	ISC_LIST_INIT(zone->nsec3chain);
	zone->signatures = 10;
	zone->nodes = 100;
	zone->privatetype = (dns_rdatatype_t)0xffffU;
	zone->added = ISC_FALSE;
	zone->is_rpz = ISC_FALSE;
	ISC_LIST_INIT(zone->forwards);
	zone->raw = NULL;
	zone->secure = NULL;
	zone->sourceserial = 0;
	zone->sourceserialset = ISC_FALSE;
	ISC_LIST_INIT(zone->rss_events);
	zone->rss_db = NULL;
	zone->rss_raw = NULL;
	zone->rss_newver = NULL;
	zone->rss_oldver = NULL;
	zone->rss_event = NULL;
	zone->rss_state = NULL;
	zone->updatemethod = dns_updatemethod_increment;

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
	dns_signing_t *signing;
	dns_nsec3chain_t *nsec3chain;

	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(isc_refcount_current(&zone->erefs) == 0);
	REQUIRE(zone->irefs == 0);
	REQUIRE(!LOCKED_ZONE(zone));
	REQUIRE(zone->timer == NULL);
	REQUIRE(zone->zmgr == NULL);

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
	if (zone->loadtask != NULL)
		isc_task_detach(&zone->loadtask);

	/* Unmanaged objects */
	for (signing = ISC_LIST_HEAD(zone->signing);
	     signing != NULL;
	     signing = ISC_LIST_HEAD(zone->signing)) {
		ISC_LIST_UNLINK(zone->signing, signing, link);
		dns_db_detach(&signing->db);
		dns_dbiterator_destroy(&signing->dbiterator);
		isc_mem_put(zone->mctx, signing, sizeof *signing);
	}
	for (nsec3chain = ISC_LIST_HEAD(zone->nsec3chain);
	     nsec3chain != NULL;
	     nsec3chain = ISC_LIST_HEAD(zone->nsec3chain)) {
		ISC_LIST_UNLINK(zone->nsec3chain, nsec3chain, link);
		dns_db_detach(&nsec3chain->db);
		dns_dbiterator_destroy(&nsec3chain->dbiterator);
		isc_mem_put(zone->mctx, nsec3chain, sizeof *nsec3chain);
	}
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
	if (zone->stats != NULL)
		isc_stats_detach(&zone->stats);
	if (zone->requeststats != NULL)
		isc_stats_detach(&zone->requeststats);
	if(zone->rcvquerystats != NULL )
		dns_stats_detach(&zone->rcvquerystats);
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
	if (zone->queryon_acl != NULL)
		dns_acl_detach(&zone->queryon_acl);
	if (zone->xfr_acl != NULL)
		dns_acl_detach(&zone->xfr_acl);
	if (dns_name_dynamic(&zone->origin))
		dns_name_free(&zone->origin, zone->mctx);
	if (zone->strnamerd != NULL)
		isc_mem_free(zone->mctx, zone->strnamerd);
	if (zone->strname != NULL)
		isc_mem_free(zone->mctx, zone->strname);
	if (zone->strrdclass != NULL)
		isc_mem_free(zone->mctx, zone->strrdclass);
	if (zone->strviewname != NULL)
		isc_mem_free(zone->mctx, zone->strviewname);
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
 * Returns ISC_TRUE iff this the signed side of an inline-signing zone.
 * Caller should hold zone lock.
 */
static inline isc_boolean_t
inline_secure(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));
	if (zone->raw != NULL)
		return (ISC_TRUE);
	return (ISC_FALSE);
}

/*
 * Returns ISC_TRUE iff this the unsigned side of an inline-signing zone
 * Caller should hold zone lock.
 */
static inline isc_boolean_t
inline_raw(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));
	if (zone->secure != NULL)
		return (ISC_TRUE);
	return (ISC_FALSE);
}

/*
 *	Single shot.
 */
void
dns_zone_setclass(dns_zone_t *zone, dns_rdataclass_t rdclass) {
	char namebuf[1024];

	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(rdclass != dns_rdataclass_none);

	/*
	 * Test and set.
	 */
	LOCK_ZONE(zone);
	REQUIRE(zone->rdclass == dns_rdataclass_none ||
		zone->rdclass == rdclass);
	zone->rdclass = rdclass;

	if (zone->strnamerd != NULL)
		isc_mem_free(zone->mctx, zone->strnamerd);
	if (zone->strrdclass != NULL)
		isc_mem_free(zone->mctx, zone->strrdclass);

	zone_namerd_tostr(zone, namebuf, sizeof namebuf);
	zone->strnamerd = isc_mem_strdup(zone->mctx, namebuf);
	zone_rdclass_tostr(zone, namebuf, sizeof namebuf);
	zone->strrdclass = isc_mem_strdup(zone->mctx, namebuf);

	if (inline_secure(zone))
		dns_zone_setclass(zone->raw, rdclass);
	UNLOCK_ZONE(zone);
}

dns_rdataclass_t
dns_zone_getclass(dns_zone_t *zone) {
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

isc_result_t
dns_zone_getserial2(dns_zone_t *zone, isc_uint32_t *serialp) {
	isc_result_t result;
	unsigned int soacount;

	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(serialp != NULL);

	LOCK_ZONE(zone);
	ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_read);
	if (zone->db != NULL) {
		result = zone_get_from_db(zone, zone->db, NULL, &soacount,
					  serialp, NULL, NULL, NULL, NULL,
					  NULL);
		if (result == ISC_R_SUCCESS && soacount == 0)
			result = ISC_R_FAILURE;
	} else
		result = DNS_R_NOTLOADED;
	ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);
	UNLOCK_ZONE(zone);

	return (result);
}

isc_uint32_t
dns_zone_getserial(dns_zone_t *zone) {
	isc_result_t result;
	isc_uint32_t serial;

	result = dns_zone_getserial2(zone, &serial);
	if (result != ISC_R_SUCCESS)
		serial = 0; /* XXX: not really correct, but no other choice */

	return (serial);
}

/*
 *	Single shot.
 */
void
dns_zone_settype(dns_zone_t *zone, dns_zonetype_t type) {
	char namebuf[1024];

	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(type != dns_zone_none);

	/*
	 * Test and set.
	 */
	LOCK_ZONE(zone);
	REQUIRE(zone->type == dns_zone_none || zone->type == type);
	zone->type = type;

	if (zone->strnamerd != NULL)
		isc_mem_free(zone->mctx, zone->strnamerd);

	zone_namerd_tostr(zone, namebuf, sizeof namebuf);
	zone->strnamerd = isc_mem_strdup(zone->mctx, namebuf);
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
	char namebuf[1024];
	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	if (zone->view != NULL)
		dns_view_weakdetach(&zone->view);
	dns_view_weakattach(view, &zone->view);

	if (zone->strviewname != NULL)
		isc_mem_free(zone->mctx, zone->strviewname);
	if (zone->strnamerd != NULL)
		isc_mem_free(zone->mctx, zone->strnamerd);

	zone_namerd_tostr(zone, namebuf, sizeof namebuf);
	zone->strnamerd = isc_mem_strdup(zone->mctx, namebuf);
	zone_viewname_tostr(zone, namebuf, sizeof namebuf);
	zone->strviewname = isc_mem_strdup(zone->mctx, namebuf);

	if (inline_secure(zone))
		dns_zone_setview(zone->raw, view);

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
	char namebuf[1024];

	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(origin != NULL);

	LOCK_ZONE(zone);
	if (dns_name_dynamic(&zone->origin)) {
		dns_name_free(&zone->origin, zone->mctx);
		dns_name_init(&zone->origin, NULL);
	}
	result = dns_name_dup(origin, zone->mctx, &zone->origin);

	if (zone->strnamerd != NULL)
		isc_mem_free(zone->mctx, zone->strnamerd);
	if (zone->strname != NULL)
		isc_mem_free(zone->mctx, zone->strname);

	zone_namerd_tostr(zone, namebuf, sizeof namebuf);
	zone->strnamerd = isc_mem_strdup(zone->mctx, namebuf);
	zone_name_tostr(zone, namebuf, sizeof namebuf);
	zone->strname = isc_mem_strdup(zone->mctx, namebuf);

	if (result == ISC_R_SUCCESS && inline_secure(zone))
		result = dns_zone_setorigin(zone->raw, origin);
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
 * This is true for slave zones, stub zones, key zones, and zones that
 * allow dynamic updates either by having an update policy ("ssutable")
 * or an "allow-update" ACL with a value other than exactly "{ none; }".
 */
isc_boolean_t
dns_zone_isdynamic(dns_zone_t *zone, isc_boolean_t ignore_freeze) {
	REQUIRE(DNS_ZONE_VALID(zone));

	if (zone->type == dns_zone_slave || zone->type == dns_zone_stub ||
	    zone->type == dns_zone_key ||
	    (zone->type == dns_zone_redirect && zone->masters != NULL))
		return (ISC_TRUE);

	/* If !ignore_freeze, we need check whether updates are disabled.  */
	if (zone->type == dns_zone_master &&
	    (!zone->update_disabled || ignore_freeze) &&
	    ((zone->ssutable != NULL) ||
	     (zone->update_acl != NULL && !dns_acl_isnone(zone->update_acl))))
		return (ISC_TRUE);

	return (ISC_FALSE);

}

/*
 * Set the response policy index and information for a zone.
 */
isc_result_t
dns_zone_rpz_enable(dns_zone_t *zone) {
	/*
	 * Only RBTDB zones can be used for response policy zones,
	 * because only they have the code to load the create the summary data.
	 * Only zones that are loaded instead of mmap()ed create the
	 * summary data and so can be policy zones.
	 */
	if (strcmp(zone->db_argv[0], "rbt") != 0 &&
	    strcmp(zone->db_argv[0], "rbt64") != 0)
		return (ISC_R_NOTIMPLEMENTED);

	zone->is_rpz = ISC_TRUE;

	return (ISC_R_SUCCESS);
}

isc_boolean_t
dns_zone_get_rpz(dns_zone_t *zone) {
	return (zone->is_rpz);
}

/*
 * If a zone is a response policy zone, mark its new database.
 */
isc_result_t
dns_zone_rpz_enable_db(dns_zone_t *zone, dns_db_t *db) {
#ifdef BIND9
	if (zone->is_rpz)
		return (dns_db_rpz_enabled(db, NULL));
#endif
	return (ISC_R_SUCCESS);
}

static isc_result_t
zone_load(dns_zone_t *zone, unsigned int flags, isc_boolean_t locked) {
	isc_result_t result;
	isc_time_t now;
	isc_time_t loadtime, filetime;
	dns_db_t *db = NULL;
	isc_boolean_t rbt, hasraw;

	REQUIRE(DNS_ZONE_VALID(zone));

	if (!locked)
		LOCK_ZONE(zone);
	hasraw = inline_secure(zone);
	if (hasraw) {
		result = zone_load(zone->raw, flags, ISC_FALSE);
		if (result != ISC_R_SUCCESS) {
			if (!locked)
				UNLOCK_ZONE(zone);
			return(result);
		}
		LOCK_ZONE(zone->raw);
	}

	TIME_NOW(&now);

	INSIST(zone->type != dns_zone_none);

	if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_LOADING)) {
		if ((flags & DNS_ZONELOADFLAG_THAW) != 0)
			DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_THAW);
		result = DNS_R_CONTINUE;
		goto cleanup;
	}

	INSIST(zone->db_argc >= 1);

	rbt = strcmp(zone->db_argv[0], "rbt") == 0 ||
	      strcmp(zone->db_argv[0], "rbt64") == 0;

	if (zone->db != NULL && zone->masterfile == NULL && rbt) {
		/*
		 * The zone has no master file configured.
		 */
		result = ISC_R_SUCCESS;
		goto cleanup;
	}

	if (zone->db != NULL && dns_zone_isdynamic(zone, ISC_FALSE)) {
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
		 * The file is already loaded.	If we are just doing a
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

	/*
	 * Built in zones (with the exception of empty zones) don't need
	 * to be reloaded.
	 */
	if (zone->type == dns_zone_master &&
	    strcmp(zone->db_argv[0], "_builtin") == 0 &&
	    (zone->db_argc < 2 || strcmp(zone->db_argv[1], "empty") != 0) &&
	    DNS_ZONE_FLAG(zone, DNS_ZONEFLG_LOADED)) {
		result = ISC_R_SUCCESS;
		goto cleanup;
	}

	if ((zone->type == dns_zone_slave || zone->type == dns_zone_stub ||
	     (zone->type == dns_zone_redirect && zone->masters != NULL)) &&
	    rbt) {
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
			if (zone->type == dns_zone_master ||
			    (zone->type == dns_zone_redirect &&
			     zone->masters == NULL)) {
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
	if (hasraw)
		UNLOCK_ZONE(zone->raw);
	if (!locked)
		UNLOCK_ZONE(zone);
	if (db != NULL)
		dns_db_detach(&db);
	return (result);
}

isc_result_t
dns_zone_load(dns_zone_t *zone) {
	return (zone_load(zone, 0, ISC_FALSE));
}

isc_result_t
dns_zone_loadnew(dns_zone_t *zone) {
	return (zone_load(zone, DNS_ZONELOADFLAG_NOSTAT, ISC_FALSE));
}

static void
zone_asyncload(isc_task_t *task, isc_event_t *event) {
	dns_asyncload_t *asl = event->ev_arg;
	dns_zone_t *zone = asl->zone;
	isc_result_t result = ISC_R_SUCCESS;
	isc_boolean_t load_pending;

	UNUSED(task);

	REQUIRE(DNS_ZONE_VALID(zone));

	if ((event->ev_attributes & ISC_EVENTATTR_CANCELED) != 0)
		result = ISC_R_CANCELED;
	isc_event_free(&event);

	if (result == ISC_R_CANCELED)
		goto cleanup;

	/* Make sure load is still pending */
	LOCK_ZONE(zone);
	load_pending = ISC_TF(DNS_ZONE_FLAG(zone, DNS_ZONEFLG_LOADPENDING));

	if (!load_pending) {
		UNLOCK_ZONE(zone);
		goto cleanup;
	}

	zone_load(zone, 0, ISC_TRUE);

	DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_LOADPENDING);
	UNLOCK_ZONE(zone);

	/* Inform the zone table we've finished loading */
	if (asl->loaded != NULL)
		(asl->loaded)(asl->loaded_arg, zone, task);

 cleanup:
	isc_mem_put(zone->mctx, asl, sizeof (*asl));
	dns_zone_idetach(&zone);
}

isc_result_t
dns_zone_asyncload(dns_zone_t *zone, dns_zt_zoneloaded_t done, void *arg) {
	isc_event_t *e;
	dns_asyncload_t *asl = NULL;
	isc_result_t result = ISC_R_SUCCESS;

	REQUIRE(DNS_ZONE_VALID(zone));

	if (zone->zmgr == NULL)
		return (ISC_R_FAILURE);

	/* If we already have a load pending, stop now */
	if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_LOADPENDING))
		return (ISC_R_ALREADYRUNNING);

	asl = isc_mem_get(zone->mctx, sizeof (*asl));
	if (asl == NULL)
		CHECK(ISC_R_NOMEMORY);

	asl->zone = NULL;
	asl->loaded = done;
	asl->loaded_arg = arg;

	e = isc_event_allocate(zone->zmgr->mctx, zone->zmgr,
			       DNS_EVENT_ZONELOAD,
			       zone_asyncload, asl,
			       sizeof(isc_event_t));
	if (e == NULL)
		CHECK(ISC_R_NOMEMORY);

	LOCK_ZONE(zone);
	zone_iattach(zone, &asl->zone);
	DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_LOADPENDING);
	isc_task_send(zone->loadtask, &e);
	UNLOCK_ZONE(zone);

	return (ISC_R_SUCCESS);

  failure:
	if (asl != NULL)
		isc_mem_put(zone->mctx, asl, sizeof (*asl));
	return (result);
}

isc_boolean_t
dns__zone_loadpending(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));

	return (ISC_TF(DNS_ZONE_FLAG(zone, DNS_ZONEFLG_LOADPENDING)));
}

isc_result_t
dns_zone_loadandthaw(dns_zone_t *zone) {
	isc_result_t result;

	if (inline_raw(zone))
		result = zone_load(zone->secure, DNS_ZONELOADFLAG_THAW,
				   ISC_FALSE);
	else
		result = zone_load(zone, DNS_ZONELOADFLAG_THAW, ISC_FALSE);

	switch (result) {
	case DNS_R_CONTINUE:
		/* Deferred thaw. */
		break;
	case DNS_R_UPTODATE:
	case ISC_R_SUCCESS:
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

static unsigned int
get_master_options(dns_zone_t *zone) {
	unsigned int options;

	options = DNS_MASTER_ZONE;
	if (zone->type == dns_zone_slave ||
	    (zone->type == dns_zone_redirect && zone->masters == NULL))
		options |= DNS_MASTER_SLAVE;
	if (zone->type == dns_zone_key)
		options |= DNS_MASTER_KEY;
	if (DNS_ZONE_OPTION(zone, DNS_ZONEOPT_CHECKNS))
		options |= DNS_MASTER_CHECKNS;
	if (DNS_ZONE_OPTION(zone, DNS_ZONEOPT_FATALNS))
		options |= DNS_MASTER_FATALNS;
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
	if (inline_secure(zone) || (zone->type == dns_zone_master &&
	    ((zone->update_acl != NULL && !dns_acl_isnone(zone->update_acl)) ||
	      zone->ssutable != NULL)))
		options |= DNS_MASTER_RESIGN;
	return (options);
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

	options = get_master_options(load->zone);

	result = dns_master_loadfileinc3(load->zone->masterfile,
					 dns_db_origin(load->db),
					 dns_db_origin(load->db),
					 load->zone->rdclass, options, 0,
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
get_raw_serial(dns_zone_t *raw, dns_masterrawheader_t *rawdata) {
	isc_result_t result;
	unsigned int soacount;

	LOCK(&raw->lock);
	if (raw->db != NULL) {
		result = zone_get_from_db(raw, raw->db, NULL, &soacount,
					  &rawdata->sourceserial,
					  NULL, NULL, NULL, NULL,
					  NULL);
		if (result == ISC_R_SUCCESS && soacount > 0U)
			rawdata->flags |= DNS_MASTERRAW_SOURCESERIALSET;
	}
	UNLOCK(&raw->lock);
}

static void
zone_gotwritehandle(isc_task_t *task, isc_event_t *event) {
	const char me[] = "zone_gotwritehandle";
	dns_zone_t *zone = event->ev_arg;
	isc_result_t result = ISC_R_SUCCESS;
	dns_dbversion_t *version = NULL;
	dns_masterrawheader_t rawdata;

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
	if (zone->db != NULL) {
		const dns_master_style_t *output_style;

		dns_db_currentversion(zone->db, &version);
		dns_master_initrawheader(&rawdata);
		if (inline_secure(zone))
			get_raw_serial(zone->raw, &rawdata);
		if (zone->type == dns_zone_key)
			output_style = &dns_master_style_keyzone;
		else
			output_style = &dns_master_style_default;
		result = dns_master_dumpinc3(zone->mctx, zone->db, version,
					     output_style, zone->masterfile,
					     zone->task, dump_done, zone,						     &zone->dctx, zone->masterformat,
					     &rawdata);
		dns_db_closeversion(zone->db, &version, ISC_FALSE);
	} else
		result = ISC_R_CANCELED;
	ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);
	UNLOCK_ZONE(zone);
	if (result != DNS_R_CONTINUE)
		goto fail;
	return;

 fail:
	dump_done(zone, result);
}

/*
 * Save the raw serial number for inline-signing zones.
 * (XXX: Other information from the header will be used
 * for other purposes in the future, but for now this is
 * all we're interested in.)
 */
static void
zone_setrawdata(dns_zone_t *zone, dns_masterrawheader_t *header) {
	if ((header->flags & DNS_MASTERRAW_SOURCESERIALSET) == 0)
		return;

	zone->sourceserial = header->sourceserial;
	zone->sourceserialset = ISC_TRUE;
}

void
dns_zone_setrawdata(dns_zone_t *zone, dns_masterrawheader_t *header) {
	if (zone == NULL)
		return;

	LOCK_ZONE(zone);
	zone_setrawdata(zone, header);
	UNLOCK_ZONE(zone);
}

static isc_result_t
zone_startload(dns_db_t *db, dns_zone_t *zone, isc_time_t loadtime) {
	dns_load_t *load;
	isc_result_t result;
	isc_result_t tresult;
	unsigned int options;

	result = dns_zone_rpz_enable_db(zone, db);
	if (result != ISC_R_SUCCESS)
		return (result);
	options = get_master_options(zone);
	if (DNS_ZONE_OPTION(zone, DNS_ZONEOPT_MANYERRORS))
		options |= DNS_MASTER_MANYERRORS;

	if (zone->zmgr != NULL && zone->db != NULL && zone->loadtask != NULL) {
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
		load->callbacks.rawdata = zone_setrawdata;
		zone_iattach(zone, &load->callbacks.zone);
		result = dns_db_beginload(db, &load->callbacks.add,
					  &load->callbacks.add_private);
		if (result != ISC_R_SUCCESS)
			goto cleanup;
		result = zonemgr_getio(zone->zmgr, ISC_TRUE, zone->loadtask,
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
		callbacks.rawdata = zone_setrawdata;
		zone_iattach(zone, &callbacks.zone);
		result = dns_db_beginload(db, &callbacks.add,
					  &callbacks.add_private);
		if (result != ISC_R_SUCCESS) {
			zone_idetach(&callbacks.zone);
			return (result);
		}
		result = dns_master_loadfile3(zone->masterfile,
					      &zone->origin, &zone->origin,
					      zone->rdclass, options, 0,
					      &callbacks, zone->mctx,
					      zone->masterformat);
		tresult = dns_db_endload(db, &callbacks.add_private);
		if (result == ISC_R_SUCCESS)
			result = tresult;
		zone_idetach(&callbacks.zone);
	}

	return (result);

 cleanup:
	load->magic = 0;
	dns_db_detach(&load->db);
	zone_idetach(&load->zone);
	zone_idetach(&load->callbacks.zone);
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
	 * "." means the services does not exist.
	 */
	if (dns_name_equal(name, dns_rootname))
		return (ISC_TRUE);

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
		if (!DNS_ZONE_OPTION(zone, DNS_ZONEOPT_CHECKMXFAIL))
			level = ISC_LOG_WARNING;
		dns_zone_log(zone, level,
			     "%s/MX '%s' has no address records (A or AAAA)",
			     ownerbuf, namebuf);
		return ((level == ISC_LOG_WARNING) ? ISC_TRUE : ISC_FALSE);
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
			if (dns_rdataset_isassociated(&a))
				dns_rdataset_disassociate(&a);
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
	}

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
zone_rrset_check_dup(dns_zone_t *zone, dns_name_t *owner,
		     dns_rdataset_t *rdataset)
{
	dns_rdataset_t tmprdataset;
	isc_result_t result;
	isc_boolean_t answer = ISC_TRUE;
	isc_boolean_t format = ISC_TRUE;
	int level = ISC_LOG_WARNING;
	char ownerbuf[DNS_NAME_FORMATSIZE];
	char typebuf[DNS_RDATATYPE_FORMATSIZE];
	unsigned int count1 = 0;

	if (DNS_ZONE_OPTION(zone, DNS_ZONEOPT_CHECKDUPRRFAIL))
		level = ISC_LOG_ERROR;

	dns_rdataset_init(&tmprdataset);
	for (result = dns_rdataset_first(rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(rdataset)) {
		dns_rdata_t rdata1 = DNS_RDATA_INIT;
		unsigned int count2 = 0;

		count1++;
		dns_rdataset_current(rdataset, &rdata1);
		dns_rdataset_clone(rdataset, &tmprdataset);
		for (result = dns_rdataset_first(&tmprdataset);
		     result == ISC_R_SUCCESS;
		     result = dns_rdataset_next(&tmprdataset)) {
			dns_rdata_t rdata2 = DNS_RDATA_INIT;
			count2++;
			if (count1 >= count2)
				continue;
			dns_rdataset_current(&tmprdataset, &rdata2);
			if (dns_rdata_casecompare(&rdata1, &rdata2) == 0) {
				if (format) {
					dns_name_format(owner, ownerbuf,
							sizeof ownerbuf);
					dns_rdatatype_format(rdata1.type,
							     typebuf,
							     sizeof(typebuf));
					format = ISC_FALSE;
				}
				dns_zone_log(zone, level, "%s/%s has "
					     "semantically identical records",
					     ownerbuf, typebuf);
				if (level == ISC_LOG_ERROR)
					answer = ISC_FALSE;
				break;
			}
		}
		dns_rdataset_disassociate(&tmprdataset);
		if (!format)
			break;
	}
	return (answer);
}

static isc_boolean_t
zone_check_dup(dns_zone_t *zone, dns_db_t *db) {
	dns_dbiterator_t *dbiterator = NULL;
	dns_dbnode_t *node = NULL;
	dns_fixedname_t fixed;
	dns_name_t *name;
	dns_rdataset_t rdataset;
	dns_rdatasetiter_t *rdsit = NULL;
	isc_boolean_t ok = ISC_TRUE;
	isc_result_t result;

	dns_fixedname_init(&fixed);
	name = dns_fixedname_name(&fixed);
	dns_rdataset_init(&rdataset);

	result = dns_db_createiterator(db, 0, &dbiterator);
	if (result != ISC_R_SUCCESS)
		return (ISC_TRUE);

	for (result = dns_dbiterator_first(dbiterator);
	     result == ISC_R_SUCCESS;
	     result = dns_dbiterator_next(dbiterator)) {
		result = dns_dbiterator_current(dbiterator, &node, name);
		if (result != ISC_R_SUCCESS)
			continue;

		result = dns_db_allrdatasets(db, node, NULL, 0, &rdsit);
		if (result != ISC_R_SUCCESS)
			continue;

		for (result = dns_rdatasetiter_first(rdsit);
		     result == ISC_R_SUCCESS;
		     result = dns_rdatasetiter_next(rdsit)) {
			dns_rdatasetiter_current(rdsit, &rdataset);
			if (!zone_rrset_check_dup(zone, name, &rdataset))
				ok = ISC_FALSE;
			dns_rdataset_disassociate(&rdataset);
		}
		dns_rdatasetiter_destroy(&rdsit);
		dns_db_detachnode(db, &node);
	}

	if (node != NULL)
		dns_db_detachnode(db, &node);
	dns_dbiterator_destroy(&dbiterator);

	return (ok);
}

static isc_boolean_t
isspf(const dns_rdata_t *rdata) {
	char buf[1024];
	const unsigned char *data = rdata->data;
	unsigned int rdl = rdata->length, i = 0, tl, len;

	while (rdl > 0U) {
		len = tl = *data;
		++data;
		--rdl;
		INSIST(tl <= rdl);
		if (len > sizeof(buf) - i - 1)
			len = sizeof(buf) - i - 1;
		memmove(buf + i, data, len);
		i += len;
		data += tl;
		rdl -= tl;
	}

	if (i < 6U)
		return(ISC_FALSE);

	buf[i] = 0;
	if (strncmp(buf, "v=spf1", 6) == 0 && (buf[6] == 0 || buf[6] == ' '))
		return (ISC_TRUE);
	return (ISC_FALSE);
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
	isc_boolean_t ok = ISC_TRUE, have_spf, have_txt;

	dns_fixedname_init(&fixed);
	name = dns_fixedname_name(&fixed);
	dns_fixedname_init(&fixedbottom);
	bottom = dns_fixedname_name(&fixedbottom);
	dns_rdataset_init(&rdataset);
	dns_rdata_init(&rdata);

	result = dns_db_createiterator(db, 0, &dbiterator);
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
		goto next;

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
			goto checkspf;
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

 checkspf:
		/*
		 * Check if there is a type SPF record without an
		 * SPF-formatted type TXT record also being present.
		 */
		if (!DNS_ZONE_OPTION(zone, DNS_ZONEOPT_CHECKSPF))
			goto next;
		if (zone->rdclass != dns_rdataclass_in)
			goto next;
		have_spf = have_txt = ISC_FALSE;
		result = dns_db_findrdataset(db, node, NULL, dns_rdatatype_spf,
					     0, 0, &rdataset, NULL);
		if (result == ISC_R_SUCCESS) {
			dns_rdataset_disassociate(&rdataset);
			have_spf = ISC_TRUE;
		}
		result = dns_db_findrdataset(db, node, NULL, dns_rdatatype_txt,
					     0, 0, &rdataset, NULL);
		if (result != ISC_R_SUCCESS)
			goto notxt;
		result = dns_rdataset_first(&rdataset);
		while (result == ISC_R_SUCCESS) {
			dns_rdataset_current(&rdataset, &rdata);
			have_txt = isspf(&rdata);
			dns_rdata_reset(&rdata);
			if (have_txt)
				break;
			result = dns_rdataset_next(&rdataset);
		}
		dns_rdataset_disassociate(&rdataset);

 notxt:
		if (have_spf && !have_txt) {
			char namebuf[DNS_NAME_FORMATSIZE];

			dns_name_format(name, namebuf, sizeof(namebuf));
			dns_zone_log(zone, ISC_LOG_WARNING, "'%s' found type "
				     "SPF record but no SPF TXT record found, "
				     "add matching type TXT record", namebuf);
		}

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
 * broken prior OpenSSL 0.9.8c/0.9.7k.	Look for such keys and warn
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

static void
resume_signingwithkey(dns_zone_t *zone) {
	dns_dbnode_t *node = NULL;
	dns_dbversion_t *version = NULL;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdataset_t rdataset;
	isc_result_t result;
	dns_db_t *db = NULL;

	ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_read);
	if (zone->db != NULL)
		dns_db_attach(zone->db, &db);
	ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);
	if (db == NULL)
		goto cleanup;

	result = dns_db_findnode(db, &zone->origin, ISC_FALSE, &node);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	dns_db_currentversion(db, &version);
	dns_rdataset_init(&rdataset);
	result = dns_db_findrdataset(db, node, version,
				     zone->privatetype,
				     dns_rdatatype_none, 0,
				     &rdataset, NULL);
	if (result != ISC_R_SUCCESS) {
		INSIST(!dns_rdataset_isassociated(&rdataset));
		goto cleanup;
	}

	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset))
	{
		dns_rdataset_current(&rdataset, &rdata);
		if (rdata.length != 5 ||
		    rdata.data[0] == 0 || rdata.data[4] != 0) {
			dns_rdata_reset(&rdata);
			continue;
		}

		result = zone_signwithkey(zone, rdata.data[0],
					  (rdata.data[1] << 8) | rdata.data[2],
					  ISC_TF(rdata.data[3]));
		if (result != ISC_R_SUCCESS) {
			dns_zone_log(zone, ISC_LOG_ERROR,
				     "zone_signwithkey failed: %s",
				     dns_result_totext(result));
		}
		dns_rdata_reset(&rdata);
	}
	dns_rdataset_disassociate(&rdataset);

 cleanup:
	if (db != NULL) {
		if (node != NULL)
			dns_db_detachnode(db, &node);
		if (version != NULL)
			dns_db_closeversion(db, &version, ISC_FALSE);
		dns_db_detach(&db);
	}
}

static isc_result_t
zone_addnsec3chain(dns_zone_t *zone, dns_rdata_nsec3param_t *nsec3param) {
	dns_nsec3chain_t *nsec3chain, *current;
	dns_dbversion_t *version = NULL;
	isc_boolean_t nseconly = ISC_FALSE, nsec3ok = ISC_FALSE;
	isc_result_t result;
	isc_time_t now;
	unsigned int options = 0;
	char saltbuf[255*2+1];
	char flags[sizeof("INITIAL|REMOVE|CREATE|NONSEC|OPTOUT")];
	dns_db_t *db = NULL;
	int i;

	ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_read);
	if (zone->db != NULL)
		dns_db_attach(zone->db, &db);
	ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);

	if (db == NULL) {
		result = ISC_R_SUCCESS;
		goto cleanup;
	}

	dns_db_currentversion(db, &version);
	result = dns_nsec_nseconly(db, version, &nseconly);
	nsec3ok = (result == ISC_R_SUCCESS && !nseconly);
	dns_db_closeversion(db, &version, ISC_FALSE);
	if (!nsec3ok && (nsec3param->flags & DNS_NSEC3FLAG_REMOVE) == 0) {
		result = ISC_R_SUCCESS;
		goto cleanup;
	}

	nsec3chain = isc_mem_get(zone->mctx, sizeof *nsec3chain);
	if (nsec3chain == NULL) {
		result = ISC_R_NOMEMORY;
		goto cleanup;
	}

	nsec3chain->magic = 0;
	nsec3chain->done = ISC_FALSE;
	nsec3chain->db = NULL;
	nsec3chain->dbiterator = NULL;
	nsec3chain->nsec3param.common.rdclass = nsec3param->common.rdclass;
	nsec3chain->nsec3param.common.rdtype = nsec3param->common.rdtype;
	nsec3chain->nsec3param.hash = nsec3param->hash;
	nsec3chain->nsec3param.iterations = nsec3param->iterations;
	nsec3chain->nsec3param.flags = nsec3param->flags;
	nsec3chain->nsec3param.salt_length = nsec3param->salt_length;
	memmove(nsec3chain->salt, nsec3param->salt, nsec3param->salt_length);
	nsec3chain->nsec3param.salt = nsec3chain->salt;
	nsec3chain->seen_nsec = ISC_FALSE;
	nsec3chain->delete_nsec = ISC_FALSE;
	nsec3chain->save_delete_nsec = ISC_FALSE;

	if (nsec3param->flags == 0)
		strlcpy(flags, "NONE", sizeof(flags));
	else {
		flags[0] = '\0';
		if (nsec3param->flags & DNS_NSEC3FLAG_REMOVE)
			strlcat(flags, "REMOVE", sizeof(flags));
		if (nsec3param->flags & DNS_NSEC3FLAG_INITIAL) {
			if (flags[0] == '\0')
				strlcpy(flags, "INITIAL", sizeof(flags));
			else
				strlcat(flags, "|INITIAL", sizeof(flags));
		}
		if (nsec3param->flags & DNS_NSEC3FLAG_CREATE) {
			if (flags[0] == '\0')
				strlcpy(flags, "CREATE", sizeof(flags));
			else
				strlcat(flags, "|CREATE", sizeof(flags));
		}
		if (nsec3param->flags & DNS_NSEC3FLAG_NONSEC) {
			if (flags[0] == '\0')
				strlcpy(flags, "NONSEC", sizeof(flags));
			else
				strlcat(flags, "|NONSEC", sizeof(flags));
		}
		if (nsec3param->flags & DNS_NSEC3FLAG_OPTOUT) {
			if (flags[0] == '\0')
				strlcpy(flags, "OPTOUT", sizeof(flags));
			else
				strlcat(flags, "|OPTOUT", sizeof(flags));
		}
	}
	if (nsec3param->salt_length == 0)
		strlcpy(saltbuf, "-", sizeof(saltbuf));
	else
		for (i = 0; i < nsec3param->salt_length; i++)
			sprintf(&saltbuf[i*2], "%02X", nsec3chain->salt[i]);
	dns_zone_log(zone, ISC_LOG_INFO,
		     "zone_addnsec3chain(%u,%s,%u,%s)",
		      nsec3param->hash, flags, nsec3param->iterations,
		      saltbuf);

	for (current = ISC_LIST_HEAD(zone->nsec3chain);
	     current != NULL;
	     current = ISC_LIST_NEXT(current, link)) {
		if (current->db == db &&
		    current->nsec3param.hash == nsec3param->hash &&
		    current->nsec3param.iterations == nsec3param->iterations &&
		    current->nsec3param.salt_length == nsec3param->salt_length
		    && !memcmp(current->nsec3param.salt, nsec3param->salt,
			       nsec3param->salt_length))
			current->done = ISC_TRUE;
	}

	dns_db_attach(db, &nsec3chain->db);
	if ((nsec3chain->nsec3param.flags & DNS_NSEC3FLAG_CREATE) != 0)
		options = DNS_DB_NONSEC3;
	result = dns_db_createiterator(nsec3chain->db, options,
				       &nsec3chain->dbiterator);
	if (result == ISC_R_SUCCESS)
		dns_dbiterator_first(nsec3chain->dbiterator);
	if (result == ISC_R_SUCCESS) {
		dns_dbiterator_pause(nsec3chain->dbiterator);
		ISC_LIST_INITANDAPPEND(zone->nsec3chain,
				       nsec3chain, link);
		nsec3chain = NULL;
		if (isc_time_isepoch(&zone->nsec3chaintime)) {
			TIME_NOW(&now);
			zone->nsec3chaintime = now;
			if (zone->task != NULL)
				zone_settimer(zone, &now);
		}
	}

	if (nsec3chain != NULL) {
		if (nsec3chain->db != NULL)
			dns_db_detach(&nsec3chain->db);
		if (nsec3chain->dbiterator != NULL)
			dns_dbiterator_destroy(&nsec3chain->dbiterator);
		isc_mem_put(zone->mctx, nsec3chain, sizeof *nsec3chain);
	}

 cleanup:
	if (db != NULL)
		dns_db_detach(&db);
	return (result);
}

static void
resume_addnsec3chain(dns_zone_t *zone) {
	dns_dbnode_t *node = NULL;
	dns_dbversion_t *version = NULL;
	dns_rdataset_t rdataset;
	isc_result_t result;
	dns_rdata_nsec3param_t nsec3param;
	isc_boolean_t nseconly = ISC_FALSE, nsec3ok = ISC_FALSE;
	dns_db_t *db = NULL;

	if (zone->privatetype == 0)
		return;

	ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_read);
	if (zone->db != NULL)
		dns_db_attach(zone->db, &db);
	ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);
	if (db == NULL)
		goto cleanup;

	result = dns_db_findnode(db, &zone->origin, ISC_FALSE, &node);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	dns_db_currentversion(db, &version);

	result = dns_nsec_nseconly(db, version, &nseconly);
	nsec3ok = (result == ISC_R_SUCCESS && !nseconly);

	dns_rdataset_init(&rdataset);
	result = dns_db_findrdataset(db, node, version,
				     zone->privatetype, dns_rdatatype_none,
				     0, &rdataset, NULL);
	if (result != ISC_R_SUCCESS) {
		INSIST(!dns_rdataset_isassociated(&rdataset));
		goto cleanup;
	}

	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset))
	{
		unsigned char buf[DNS_NSEC3PARAM_BUFFERSIZE];
		dns_rdata_t rdata = DNS_RDATA_INIT;
		dns_rdata_t private = DNS_RDATA_INIT;

		dns_rdataset_current(&rdataset, &private);
		if (!dns_nsec3param_fromprivate(&private, &rdata, buf,
						sizeof(buf)))
			continue;
		result = dns_rdata_tostruct(&rdata, &nsec3param, NULL);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);
		if (((nsec3param.flags & DNS_NSEC3FLAG_REMOVE) != 0) ||
		    ((nsec3param.flags & DNS_NSEC3FLAG_CREATE) != 0 && nsec3ok))
		{
			result = zone_addnsec3chain(zone, &nsec3param);
			if (result != ISC_R_SUCCESS) {
				dns_zone_log(zone, ISC_LOG_ERROR,
					     "zone_addnsec3chain failed: %s",
					     dns_result_totext(result));
			}
		}
	}
	dns_rdataset_disassociate(&rdataset);
 cleanup:
	if (db != NULL) {
		if (node != NULL)
			dns_db_detachnode(db, &node);
		if (version != NULL)
			dns_db_closeversion(db, &version, ISC_FALSE);
		dns_db_detach(&db);
	}
}

static void
set_resigntime(dns_zone_t *zone) {
	dns_rdataset_t rdataset;
	dns_fixedname_t fixed;
	unsigned int resign;
	isc_result_t result;
	isc_uint32_t nanosecs;
	dns_db_t *db = NULL;

	dns_rdataset_init(&rdataset);
	dns_fixedname_init(&fixed);

	ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_read);
	if (zone->db != NULL)
		dns_db_attach(zone->db, &db);
	ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);
	if (db == NULL) {
		isc_time_settoepoch(&zone->resigntime);
		return;
	}

	result = dns_db_getsigningtime(db, &rdataset,
				       dns_fixedname_name(&fixed));
	if (result != ISC_R_SUCCESS) {
		isc_time_settoepoch(&zone->resigntime);
		goto cleanup;
	}

	resign = rdataset.resign - zone->sigresigninginterval;
	dns_rdataset_disassociate(&rdataset);
	isc_random_get(&nanosecs);
	nanosecs %= 1000000000;
	isc_time_set(&zone->resigntime, resign, nanosecs);
 cleanup:
	dns_db_detach(&db);
	return;
}

static isc_result_t
check_nsec3param(dns_zone_t *zone, dns_db_t *db) {
	dns_dbnode_t *node = NULL;
	dns_rdataset_t rdataset;
	dns_dbversion_t *version = NULL;
	dns_rdata_nsec3param_t nsec3param;
	isc_boolean_t ok = ISC_FALSE;
	isc_result_t result;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	isc_boolean_t dynamic = (zone->type == dns_zone_master) ?
				dns_zone_isdynamic(zone, ISC_FALSE) : ISC_FALSE;

	dns_rdataset_init(&rdataset);
	result = dns_db_findnode(db, &zone->origin, ISC_FALSE, &node);
	if (result != ISC_R_SUCCESS) {
		dns_zone_log(zone, ISC_LOG_ERROR,
			     "nsec3param lookup failure: %s",
			     dns_result_totext(result));
		return (result);
	}
	dns_db_currentversion(db, &version);

	result = dns_db_findrdataset(db, node, version,
				     dns_rdatatype_nsec3param,
				     dns_rdatatype_none, 0, &rdataset, NULL);
	if (result == ISC_R_NOTFOUND) {
		INSIST(!dns_rdataset_isassociated(&rdataset));
		result = ISC_R_SUCCESS;
		goto cleanup;
	}
	if (result != ISC_R_SUCCESS) {
		INSIST(!dns_rdataset_isassociated(&rdataset));
		dns_zone_log(zone, ISC_LOG_ERROR,
			     "nsec3param lookup failure: %s",
			     dns_result_totext(result));
		goto cleanup;
	}

	/*
	 * For dynamic zones we must support every algorithm so we can
	 * regenerate all the NSEC3 chains.
	 * For non-dynamic zones we only need to find a supported algorithm.
	 */
	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset))
	{
		dns_rdataset_current(&rdataset, &rdata);
		result = dns_rdata_tostruct(&rdata, &nsec3param, NULL);
		dns_rdata_reset(&rdata);
		INSIST(result == ISC_R_SUCCESS);
		if (DNS_ZONE_OPTION(zone, DNS_ZONEOPT_NSEC3TESTZONE) &&
		    nsec3param.hash == DNS_NSEC3_UNKNOWNALG && !dynamic)
		{
			dns_zone_log(zone, ISC_LOG_WARNING,
			     "nsec3 test \"unknown\" hash algorithm found: %u",
				     nsec3param.hash);
			ok = ISC_TRUE;
		} else if (!dns_nsec3_supportedhash(nsec3param.hash)) {
			if (dynamic) {
				dns_zone_log(zone, ISC_LOG_ERROR,
					     "unsupported nsec3 hash algorithm"
					     " in dynamic zone: %u",
					     nsec3param.hash);
				result = DNS_R_BADZONE;
				/* Stop second error message. */
				ok = ISC_TRUE;
				break;
			} else
				dns_zone_log(zone, ISC_LOG_WARNING,
				     "unsupported nsec3 hash algorithm: %u",
					     nsec3param.hash);
		} else
			ok = ISC_TRUE;
	}
	if (result == ISC_R_NOMORE)
		result = ISC_R_SUCCESS;

	if (!ok) {
		result = DNS_R_BADZONE;
		dns_zone_log(zone, ISC_LOG_ERROR,
			     "no supported nsec3 hash algorithm");
	}

 cleanup:
	if (dns_rdataset_isassociated(&rdataset))
		dns_rdataset_disassociate(&rdataset);
	dns_db_closeversion(db, &version, ISC_FALSE);
	dns_db_detachnode(db, &node);
	return (result);
}

/*
 * Set the timer for refreshing the key zone to the soonest future time
 * of the set (current timer, keydata->refresh, keydata->addhd,
 * keydata->removehd).
 */
static void
set_refreshkeytimer(dns_zone_t *zone, dns_rdata_keydata_t *key,
		    isc_stdtime_t now, isc_boolean_t force)
{
	const char me[] = "set_refreshkeytimer";
	isc_stdtime_t then;
	isc_time_t timenow, timethen;
	char timebuf[80];

	ENTER;
	then = key->refresh;
	if (force)
		then = now;
	if (key->addhd > now && key->addhd < then)
		then = key->addhd;
	if (key->removehd > now && key->removehd < then)
		then = key->removehd;

	TIME_NOW(&timenow);
	if (then > now)
		DNS_ZONE_TIME_ADD(&timenow, then - now, &timethen);
	else
		timethen = timenow;
	if (isc_time_compare(&zone->refreshkeytime, &timenow) < 0 ||
	    isc_time_compare(&timethen, &zone->refreshkeytime) < 0)
		zone->refreshkeytime = timethen;

	isc_time_formattimestamp(&zone->refreshkeytime, timebuf, 80);
	dns_zone_log(zone, ISC_LOG_DEBUG(1), "next key refresh: %s", timebuf);
	zone_settimer(zone, &timenow);
}

/*
 * Convert key(s) linked from 'keynode' to KEYDATA and add to the key zone.
 * If the key zone is changed, set '*changed' to ISC_TRUE.
 */
static isc_result_t
create_keydata(dns_zone_t *zone, dns_db_t *db, dns_dbversion_t *ver,
	       dns_diff_t *diff, dns_keytable_t *keytable,
	       dns_keynode_t **keynodep, isc_boolean_t *changed)
{
	const char me[] = "create_keydata";
	isc_result_t result = ISC_R_SUCCESS;
	isc_buffer_t keyb, dstb;
	unsigned char key_buf[4096], dst_buf[DST_KEY_MAXSIZE];
	dns_rdata_keydata_t keydata;
	dns_rdata_dnskey_t dnskey;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_keynode_t *keynode;
	isc_stdtime_t now;
	isc_region_t r;
	dst_key_t *key;

	REQUIRE(keynodep != NULL);
	keynode = *keynodep;

	ENTER;
	isc_stdtime_get(&now);

	/* Loop in case there's more than one key. */
	while (result == ISC_R_SUCCESS) {
		dns_keynode_t *nextnode = NULL;

		key = dns_keynode_key(keynode);
		if (key == NULL)
			goto skip;

		isc_buffer_init(&dstb, dst_buf, sizeof(dst_buf));
		CHECK(dst_key_todns(key, &dstb));

		/* Convert DST key to DNSKEY. */
		dns_rdata_reset(&rdata);
		isc_buffer_usedregion(&dstb, &r);
		dns_rdata_fromregion(&rdata, dst_key_class(key),
				     dns_rdatatype_dnskey, &r);

		/* DSTKEY to KEYDATA. */
		CHECK(dns_rdata_tostruct(&rdata, &dnskey, NULL));
		CHECK(dns_keydata_fromdnskey(&keydata, &dnskey, now, 0, 0,
					     NULL));

		/* KEYDATA to rdata. */
		dns_rdata_reset(&rdata);
		isc_buffer_init(&keyb, key_buf, sizeof(key_buf));
		CHECK(dns_rdata_fromstruct(&rdata,
					   zone->rdclass, dns_rdatatype_keydata,
					   &keydata, &keyb));

		/* Add rdata to zone. */
		CHECK(update_one_rr(db, ver, diff, DNS_DIFFOP_ADD,
				    dst_key_name(key), 0, &rdata));
		*changed = ISC_TRUE;

		/* Refresh new keys from the zone apex as soon as possible. */
		set_refreshkeytimer(zone, &keydata, now, ISC_TRUE);

 skip:
		result = dns_keytable_nextkeynode(keytable, keynode, &nextnode);
		if (result != ISC_R_NOTFOUND) {
			dns_keytable_detachkeynode(keytable, &keynode);
			keynode = nextnode;
		}
	}

	if (keynode != NULL)
		dns_keytable_detachkeynode(keytable, &keynode);
	*keynodep = NULL;

	return (ISC_R_SUCCESS);

  failure:
	return (result);
}

/*
 * Remove from the key zone all the KEYDATA records found in rdataset.
 */
static isc_result_t
delete_keydata(dns_db_t *db, dns_dbversion_t *ver, dns_diff_t *diff,
	       dns_name_t *name, dns_rdataset_t *rdataset)
{
	dns_rdata_t rdata = DNS_RDATA_INIT;
	isc_result_t result, uresult;

	for (result = dns_rdataset_first(rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(rdataset)) {
		dns_rdata_reset(&rdata);
		dns_rdataset_current(rdataset, &rdata);
		uresult = update_one_rr(db, ver, diff, DNS_DIFFOP_DEL,
					name, 0, &rdata);
		if (uresult != ISC_R_SUCCESS)
			return (uresult);
	}
	if (result == ISC_R_NOMORE)
		result = ISC_R_SUCCESS;
	return (result);
}

/*
 * Compute the DNSSEC key ID for a DNSKEY record.
 */
static isc_result_t
compute_tag(dns_name_t *name, dns_rdata_dnskey_t *dnskey, isc_mem_t *mctx,
	    dns_keytag_t *tag)
{
	isc_result_t result;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	unsigned char data[4096];
	isc_buffer_t buffer;
	dst_key_t *dstkey = NULL;

	isc_buffer_init(&buffer, data, sizeof(data));
	dns_rdata_fromstruct(&rdata, dnskey->common.rdclass,
			     dns_rdatatype_dnskey, dnskey, &buffer);

	result = dns_dnssec_keyfromrdata(name, &rdata, mctx, &dstkey);
	if (result == ISC_R_SUCCESS)
		*tag = dst_key_id(dstkey);
	dst_key_free(&dstkey);

	return (result);
}

/*
 * Add key to the security roots.
 */
static void
trust_key(dns_zone_t *zone, dns_name_t *keyname,
	  dns_rdata_dnskey_t *dnskey, isc_mem_t *mctx) {
	isc_result_t result;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	unsigned char data[4096];
	isc_buffer_t buffer;
	dns_keytable_t *sr = NULL;
	dst_key_t *dstkey = NULL;

	/* Convert dnskey to DST key. */
	isc_buffer_init(&buffer, data, sizeof(data));
	dns_rdata_fromstruct(&rdata, dnskey->common.rdclass,
			     dns_rdatatype_dnskey, dnskey, &buffer);

	result = dns_view_getsecroots(zone->view, &sr);
	if (result != ISC_R_SUCCESS)
		goto failure;

	CHECK(dns_dnssec_keyfromrdata(keyname, &rdata, mctx, &dstkey));
	CHECK(dns_keytable_add(sr, ISC_TRUE, &dstkey));
	dns_keytable_detach(&sr);

  failure:
	if (dstkey != NULL)
		dst_key_free(&dstkey);
	if (sr != NULL)
		dns_keytable_detach(&sr);
	return;
}

/*
 * Add a null key to the security roots for so that all queries
 * to the zone will fail.
 */
static void
fail_secure(dns_zone_t *zone, dns_name_t *keyname) {
	isc_result_t result;
	dns_keytable_t *sr = NULL;

	result = dns_view_getsecroots(zone->view, &sr);
	if (result == ISC_R_SUCCESS) {
		dns_keytable_marksecure(sr, keyname);
		dns_keytable_detach(&sr);
	}
}

/*
 * Scan a set of KEYDATA records from the key zone.  The ones that are
 * valid (i.e., the add holddown timer has expired) become trusted keys.
 */
static void
load_secroots(dns_zone_t *zone, dns_name_t *name, dns_rdataset_t *rdataset) {
	isc_result_t result;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdata_keydata_t keydata;
	dns_rdata_dnskey_t dnskey;
	isc_mem_t *mctx = zone->mctx;
	int trusted = 0, revoked = 0, pending = 0;
	isc_stdtime_t now;
	dns_keytable_t *sr = NULL;

	isc_stdtime_get(&now);

	result = dns_view_getsecroots(zone->view, &sr);
	if (result == ISC_R_SUCCESS) {
		dns_keytable_delete(sr, name);
		dns_keytable_detach(&sr);
	}

	/* Now insert all the accepted trust anchors from this keydata set. */
	for (result = dns_rdataset_first(rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(rdataset)) {
		dns_rdata_reset(&rdata);
		dns_rdataset_current(rdataset, &rdata);

		/* Convert rdata to keydata. */
		result = dns_rdata_tostruct(&rdata, &keydata, NULL);
		if (result == ISC_R_UNEXPECTEDEND)
			continue;
		RUNTIME_CHECK(result == ISC_R_SUCCESS);

		/* Set the key refresh timer to force a fast refresh. */
		set_refreshkeytimer(zone, &keydata, now, ISC_TRUE);

		/* If the removal timer is nonzero, this key was revoked. */
		if (keydata.removehd != 0) {
			revoked++;
			continue;
		}

		/*
		 * If the add timer is still pending, this key is not
		 * trusted yet.
		 */
		if (now < keydata.addhd) {
			pending++;
			continue;
		}

		/* Convert keydata to dnskey. */
		dns_keydata_todnskey(&keydata, &dnskey, NULL);

		/* Add to keytables. */
		trusted++;
		trust_key(zone, name, &dnskey, mctx);
	}

	if (trusted == 0 && pending != 0) {
		char namebuf[DNS_NAME_FORMATSIZE];
		dns_name_format(name, namebuf, sizeof namebuf);
		dns_zone_log(zone, ISC_LOG_ERROR,
			     "No valid trust anchors for '%s'!", namebuf);
		dns_zone_log(zone, ISC_LOG_ERROR,
			     "%d key(s) revoked, %d still pending",
			     revoked, pending);
		dns_zone_log(zone, ISC_LOG_ERROR,
			     "All queries to '%s' will fail", namebuf);
		fail_secure(zone, name);
	}
}

static isc_result_t
do_one_tuple(dns_difftuple_t **tuple, dns_db_t *db, dns_dbversion_t *ver,
	     dns_diff_t *diff)
{
	dns_diff_t temp_diff;
	isc_result_t result;

	/*
	 * Create a singleton diff.
	 */
	dns_diff_init(diff->mctx, &temp_diff);
	ISC_LIST_APPEND(temp_diff.tuples, *tuple, link);

	/*
	 * Apply it to the database.
	 */
	result = dns_diff_apply(&temp_diff, db, ver);
	ISC_LIST_UNLINK(temp_diff.tuples, *tuple, link);
	if (result != ISC_R_SUCCESS) {
		dns_difftuple_free(tuple);
		return (result);
	}

	/*
	 * Merge it into the current pending journal entry.
	 */
	dns_diff_appendminimal(diff, tuple);

	/*
	 * Do not clear temp_diff.
	 */
	return (ISC_R_SUCCESS);
}

static isc_result_t
update_one_rr(dns_db_t *db, dns_dbversion_t *ver, dns_diff_t *diff,
	      dns_diffop_t op, dns_name_t *name, dns_ttl_t ttl,
	      dns_rdata_t *rdata)
{
	dns_difftuple_t *tuple = NULL;
	isc_result_t result;
	result = dns_difftuple_create(diff->mctx, op,
				      name, ttl, rdata, &tuple);
	if (result != ISC_R_SUCCESS)
		return (result);
	return (do_one_tuple(&tuple, db, ver, diff));
}

static isc_result_t
update_soa_serial(dns_db_t *db, dns_dbversion_t *ver, dns_diff_t *diff,
		  isc_mem_t *mctx, dns_updatemethod_t method) {
	dns_difftuple_t *deltuple = NULL;
	dns_difftuple_t *addtuple = NULL;
	isc_uint32_t serial;
	isc_result_t result;

	CHECK(dns_db_createsoatuple(db, ver, mctx, DNS_DIFFOP_DEL, &deltuple));
	CHECK(dns_difftuple_copy(deltuple, &addtuple));
	addtuple->op = DNS_DIFFOP_ADD;

	serial = dns_soa_getserial(&addtuple->rdata);
	serial = dns_update_soaserial(serial, method);
	dns_soa_setserial(serial, &addtuple->rdata);
	CHECK(do_one_tuple(&deltuple, db, ver, diff));
	CHECK(do_one_tuple(&addtuple, db, ver, diff));
	result = ISC_R_SUCCESS;

	failure:
	if (addtuple != NULL)
		dns_difftuple_free(&addtuple);
	if (deltuple != NULL)
		dns_difftuple_free(&deltuple);
	return (result);
}

/*
 * Write all transactions in 'diff' to the zone journal file.
 */
static isc_result_t
zone_journal(dns_zone_t *zone, dns_diff_t *diff, isc_uint32_t *sourceserial,
	     const char *caller)
{
	const char me[] = "zone_journal";
	const char *journalfile;
	isc_result_t result = ISC_R_SUCCESS;
	dns_journal_t *journal = NULL;
	unsigned int mode = DNS_JOURNAL_CREATE|DNS_JOURNAL_WRITE;

	ENTER;
	journalfile = dns_zone_getjournal(zone);
	if (journalfile != NULL) {
		result = dns_journal_open(zone->mctx, journalfile, mode,
					  &journal);
		if (result != ISC_R_SUCCESS) {
			dns_zone_log(zone, ISC_LOG_ERROR,
				     "%s:dns_journal_open -> %s",
				     caller, dns_result_totext(result));
			return (result);
		}

		if (sourceserial != NULL)
			dns_journal_set_sourceserial(journal, *sourceserial);

		result = dns_journal_write_transaction(journal, diff);
		if (result != ISC_R_SUCCESS) {
			dns_zone_log(zone, ISC_LOG_ERROR,
				     "%s:dns_journal_write_transaction -> %s",
				     caller, dns_result_totext(result));
		}
		dns_journal_destroy(&journal);
	}

	return (result);
}

/*
 * Create an SOA record for a newly-created zone
 */
static isc_result_t
add_soa(dns_zone_t *zone, dns_db_t *db) {
	isc_result_t result;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	unsigned char buf[DNS_SOA_BUFFERSIZE];
	dns_dbversion_t *ver = NULL;
	dns_diff_t diff;

	dns_zone_log(zone, ISC_LOG_DEBUG(1), "creating SOA");

	dns_diff_init(zone->mctx, &diff);
	result = dns_db_newversion(db, &ver);
	if (result != ISC_R_SUCCESS) {
		dns_zone_log(zone, ISC_LOG_ERROR,
			     "add_soa:dns_db_newversion -> %s",
			     dns_result_totext(result));
		goto failure;
	}

	/* Build SOA record */
	result = dns_soa_buildrdata(&zone->origin, dns_rootname, zone->rdclass,
				    0, 0, 0, 0, 0, buf, &rdata);
	if (result != ISC_R_SUCCESS) {
		dns_zone_log(zone, ISC_LOG_ERROR,
			     "add_soa:dns_soa_buildrdata -> %s",
			     dns_result_totext(result));
		goto failure;
	}

	result = update_one_rr(db, ver, &diff, DNS_DIFFOP_ADD,
			       &zone->origin, 0, &rdata);

failure:
	dns_diff_clear(&diff);
	if (ver != NULL)
		dns_db_closeversion(db, &ver, ISC_TF(result == ISC_R_SUCCESS));

	INSIST(ver == NULL);

	return (result);
}

/*
 * Synchronize the set of initializing keys found in managed-keys {}
 * statements with the set of trust anchors found in the managed-keys.bind
 * zone.  If a domain is no longer named in managed-keys, delete all keys
 * from that domain from the key zone.	If a domain is mentioned in in
 * managed-keys but there are no references to it in the key zone, load
 * the key zone with the initializing key(s) for that domain.
 */
static isc_result_t
sync_keyzone(dns_zone_t *zone, dns_db_t *db) {
	isc_result_t result = ISC_R_SUCCESS;
	isc_boolean_t changed = ISC_FALSE;
	isc_boolean_t commit = ISC_FALSE;
	dns_rbtnodechain_t chain;
	dns_fixedname_t fn;
	dns_name_t foundname, *origin;
	dns_keynode_t *keynode = NULL;
	dns_view_t *view = zone->view;
	dns_keytable_t *sr = NULL;
	dns_dbversion_t *ver = NULL;
	dns_diff_t diff;
	dns_rriterator_t rrit;

	dns_zone_log(zone, ISC_LOG_DEBUG(1), "synchronizing trusted keys");

	dns_name_init(&foundname, NULL);
	dns_fixedname_init(&fn);
	origin = dns_fixedname_name(&fn);

	dns_diff_init(zone->mctx, &diff);

	CHECK(dns_view_getsecroots(view, &sr));

	result = dns_db_newversion(db, &ver);
	if (result != ISC_R_SUCCESS) {
		dns_zone_log(zone, ISC_LOG_ERROR,
			     "sync_keyzone:dns_db_newversion -> %s",
			     dns_result_totext(result));
		goto failure;
	}

	/*
	 * Walk the zone DB.  If we find any keys whose names are no longer
	 * in managed-keys (or *are* in trusted-keys, meaning they are
	 * permanent and not RFC5011-maintained), delete them from the
	 * zone.  Otherwise call load_secroots(), which loads keys into
	 * secroots as appropriate.
	 */
	dns_rriterator_init(&rrit, db, ver, 0);
	for (result = dns_rriterator_first(&rrit);
	     result == ISC_R_SUCCESS;
	     result = dns_rriterator_nextrrset(&rrit)) {
		dns_rdataset_t *rdataset = NULL;
		dns_name_t *rrname = NULL;
		isc_uint32_t ttl;

		dns_rriterator_current(&rrit, &rrname, &ttl,
				       &rdataset, NULL);
		if (!dns_rdataset_isassociated(rdataset)) {
			dns_rriterator_destroy(&rrit);
			goto failure;
		}

		if (rdataset->type != dns_rdatatype_keydata)
			continue;

		result = dns_keytable_find(sr, rrname, &keynode);
		if ((result != ISC_R_SUCCESS &&
		     result != DNS_R_PARTIALMATCH) ||
		    dns_keynode_managed(keynode) == ISC_FALSE)
		{
			CHECK(delete_keydata(db, ver, &diff,
					     rrname, rdataset));
			changed = ISC_TRUE;
		} else {
			load_secroots(zone, rrname, rdataset);
		}

		if (keynode != NULL)
			dns_keytable_detachkeynode(sr, &keynode);
	}
	dns_rriterator_destroy(&rrit);

	/*
	 * Now walk secroots to find any managed keys that aren't
	 * in the zone.  If we find any, we add them to the zone.
	 */
	RWLOCK(&sr->rwlock, isc_rwlocktype_write);
	dns_rbtnodechain_init(&chain, zone->mctx);
	result = dns_rbtnodechain_first(&chain, sr->table, &foundname, origin);
	if (result == ISC_R_NOTFOUND)
		result = ISC_R_NOMORE;
	while (result == DNS_R_NEWORIGIN || result == ISC_R_SUCCESS) {
		dns_rbtnode_t *rbtnode = NULL;

		dns_rbtnodechain_current(&chain, &foundname, origin, &rbtnode);
		if (rbtnode->data == NULL)
			goto skip;

		dns_keytable_attachkeynode(sr, rbtnode->data, &keynode);
		if (dns_keynode_managed(keynode)) {
			dns_fixedname_t fname;
			dns_name_t *keyname;
			dst_key_t *key;

			key = dns_keynode_key(keynode);
			dns_fixedname_init(&fname);

			if (key == NULL)   /* fail_secure() was called. */
				goto skip;

			keyname = dst_key_name(key);
			result = dns_db_find(db, keyname, ver,
					     dns_rdatatype_keydata,
					     DNS_DBFIND_NOWILD, 0, NULL,
					     dns_fixedname_name(&fname),
					     NULL, NULL);
			if (result != ISC_R_SUCCESS)
				result = create_keydata(zone, db, ver, &diff,
							sr, &keynode, &changed);
			if (result != ISC_R_SUCCESS)
				break;
		}
  skip:
		result = dns_rbtnodechain_next(&chain, &foundname, origin);
		if (keynode != NULL)
			dns_keytable_detachkeynode(sr, &keynode);
	}
	RWUNLOCK(&sr->rwlock, isc_rwlocktype_write);

	if (result == ISC_R_NOMORE)
		result = ISC_R_SUCCESS;

	if (changed) {
		/* Write changes to journal file. */
		CHECK(update_soa_serial(db, ver, &diff, zone->mctx,
					zone->updatemethod));
		CHECK(zone_journal(zone, &diff, NULL, "sync_keyzone"));

		DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_LOADED);
		zone_needdump(zone, 30);
		commit = ISC_TRUE;
	}

 failure:
	if (result != ISC_R_SUCCESS &&
	    !DNS_ZONE_FLAG(zone, DNS_ZONEFLG_LOADED)) {
		dns_zone_log(zone, ISC_LOG_ERROR,
			     "unable to synchronize managed keys: %s",
			     dns_result_totext(result));
		isc_time_settoepoch(&zone->refreshkeytime);
	}
	if (keynode != NULL)
		dns_keytable_detachkeynode(sr, &keynode);
	if (sr != NULL)
		dns_keytable_detach(&sr);
	if (ver != NULL)
		dns_db_closeversion(db, &ver, commit);
	dns_diff_clear(&diff);

	INSIST(ver == NULL);

	return (result);
}

isc_result_t
dns_zone_synckeyzone(dns_zone_t *zone) {
	isc_result_t result;
	dns_db_t *db = NULL;

	if (zone->type != dns_zone_key)
		return (DNS_R_BADZONE);

	CHECK(dns_zone_getdb(zone, &db));

	LOCK_ZONE(zone);
	result = sync_keyzone(zone, db);
	UNLOCK_ZONE(zone);

 failure:
	if (db != NULL)
		dns_db_detach(&db);
	return (result);
}

static void
maybe_send_secure(dns_zone_t *zone) {
	isc_result_t result;

	/*
	 * We've finished loading, or else failed to load, an inline-signing
	 * 'secure' zone.  We now need information about the status of the
	 * 'raw' zone.  If we failed to load, then we need it to send a
	 * copy of its database; if we succeeded, we need it to send its
	 * serial number so that we can sync with it.  If it has not yet
	 * loaded, we set a flag so that it will send the necessary
	 * information when it has finished loading.
	 */
	if (zone->raw->db != NULL) {
		if (zone->db != NULL) {
			isc_uint32_t serial;
			unsigned int soacount;

			result = zone_get_from_db(zone->raw, zone->raw->db,
						  NULL, &soacount, &serial, NULL,
						  NULL, NULL, NULL, NULL);
			if (result == ISC_R_SUCCESS && soacount > 0U)
				zone_send_secureserial(zone->raw, serial);
		} else
			zone_send_securedb(zone->raw, zone->raw->db);

	} else
		DNS_ZONE_SETFLAG(zone->raw, DNS_ZONEFLG_SENDSECURE);
}

static isc_boolean_t
zone_unchanged(dns_db_t *db1, dns_db_t *db2, isc_mem_t *mctx) {
	isc_result_t result;
	isc_boolean_t answer = ISC_FALSE;
	dns_diff_t diff;

	dns_diff_init(mctx, &diff);
	result = dns_db_diffx(&diff, db1, NULL, db2, NULL, NULL);
	if (result == ISC_R_SUCCESS && ISC_LIST_EMPTY(diff.tuples))
		answer = ISC_TRUE;
	dns_diff_clear(&diff);
	return (answer);
}

/*
 * The zone is presumed to be locked.
 * If this is a inline_raw zone the secure version is also locked.
 */
static isc_result_t
zone_postload(dns_zone_t *zone, dns_db_t *db, isc_time_t loadtime,
	      isc_result_t result)
{
	unsigned int soacount = 0;
	unsigned int nscount = 0;
	unsigned int errors = 0;
	isc_uint32_t serial, oldserial, refresh, retry, expire, minimum;
	isc_time_t now;
	isc_boolean_t needdump = ISC_FALSE;
	isc_boolean_t hasinclude = DNS_ZONE_FLAG(zone, DNS_ZONEFLG_HASINCLUDE);
	isc_boolean_t nomaster = ISC_FALSE;
	unsigned int options;

	INSIST(LOCKED_ZONE(zone));
	if (inline_raw(zone))
		INSIST(LOCKED_ZONE(zone->secure));

	TIME_NOW(&now);

	/*
	 * Initiate zone transfer?  We may need a error code that
	 * indicates that the "permanent" form does not exist.
	 * XXX better error feedback to log.
	 */
	if (result != ISC_R_SUCCESS && result != DNS_R_SEENINCLUDE) {
		if (zone->type == dns_zone_slave ||
		    zone->type == dns_zone_stub ||
		    (zone->type == dns_zone_redirect &&
		     zone->masters == NULL)) {
			if (result == ISC_R_FILENOTFOUND)
				dns_zone_log(zone, ISC_LOG_DEBUG(1),
					     "no master file");
			else if (result != DNS_R_NOMASTERFILE)
				dns_zone_log(zone, ISC_LOG_ERROR,
					     "loading from master file %s "
					     "failed: %s",
					     zone->masterfile,
					     dns_result_totext(result));
		} else if (zone->type == dns_zone_master &&
			   inline_secure(zone) && result == ISC_R_FILENOTFOUND)
		{
			dns_zone_log(zone, ISC_LOG_DEBUG(1),
				     "no master file, requesting db");
			maybe_send_secure(zone);
		} else {
			int level = ISC_LOG_ERROR;
			if (zone->type == dns_zone_key &&
			    result == ISC_R_FILENOTFOUND)
				level = ISC_LOG_DEBUG(1);
			dns_zone_log(zone, level,
				     "loading from master file %s failed: %s",
				     zone->masterfile,
				     dns_result_totext(result));
			nomaster = ISC_TRUE;
		}

		if (zone->type != dns_zone_key)
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
	 * If there's no master file for a key zone, then the zone is new:
	 * create an SOA record.  (We do this now, instead of later, so that
	 * if there happens to be a journal file, we can roll forward from
	 * a sane starting point.)
	 */
	if (nomaster && zone->type == dns_zone_key) {
		result = add_soa(zone, db);
		if (result != ISC_R_SUCCESS)
			goto cleanup;
	}

	/*
	 * Apply update log, if any, on initial load.
	 */
	if (zone->journal != NULL &&
	    ! DNS_ZONE_OPTION(zone, DNS_ZONEOPT_NOMERGE) &&
	    ! DNS_ZONE_FLAG(zone, DNS_ZONEFLG_LOADED))
	{
		if (zone->type == dns_zone_master &&
		    (zone->update_acl != NULL || zone->ssutable != NULL))
			options = DNS_JOURNALOPT_RESIGN;
		else
			options = 0;
		result = dns_journal_rollforward2(zone->mctx, db, options,
						  0, zone->journal);
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

	/*
	 * Obtain ns, soa and cname counts for top of zone.
	 */
	INSIST(db != NULL);
	result = zone_get_from_db(zone, db, &nscount, &soacount, &serial,
				  &refresh, &retry, &expire, &minimum,
				  &errors);
	if (result != ISC_R_SUCCESS && zone->type != dns_zone_key) {
		dns_zone_log(zone, ISC_LOG_ERROR,
			     "could not find NS and/or SOA records");
	}

	/*
	 * Check to make sure the journal is up to date, and remove the
	 * journal file if it isn't, as we wouldn't be able to apply
	 * updates otherwise.
	 */
	if (zone->journal != NULL && dns_zone_isdynamic(zone, ISC_TRUE) &&
	    ! DNS_ZONE_OPTION(zone, DNS_ZONEOPT_IXFRFROMDIFFS)) {
		isc_uint32_t jserial;
		dns_journal_t *journal = NULL;

		result = dns_journal_open(zone->mctx, zone->journal,
					  DNS_JOURNAL_READ, &journal);
		if (result == ISC_R_SUCCESS) {
			jserial = dns_journal_last_serial(journal);
			dns_journal_destroy(&journal);
		} else {
			jserial = serial;
			result = ISC_R_SUCCESS;
		}

		if (jserial != serial) {
			dns_zone_log(zone, ISC_LOG_INFO,
				     "journal file is out of date: "
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

	dns_zone_log(zone, ISC_LOG_DEBUG(1), "loaded; checking validity");

	/*
	 * Master / Slave / Stub zones require both NS and SOA records at
	 * the top of the zone.
	 */

	switch (zone->type) {
	case dns_zone_dlz:
	case dns_zone_master:
	case dns_zone_slave:
	case dns_zone_stub:
	case dns_zone_redirect:
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
		if (zone->type != dns_zone_stub &&
		    zone->type != dns_zone_redirect) {
			result = check_nsec3param(zone, db);
			if (result != ISC_R_SUCCESS)
				goto cleanup;
		}
		if (zone->type == dns_zone_master &&
		    DNS_ZONE_OPTION(zone, DNS_ZONEOPT_CHECKINTEGRITY) &&
		    !integrity_checks(zone, db)) {
			result = DNS_R_BADZONE;
			goto cleanup;
		}
		if (zone->type == dns_zone_master &&
		    DNS_ZONE_OPTION(zone, DNS_ZONEOPT_CHECKDUPRR) &&
		    !zone_check_dup(zone, db)) {
			result = DNS_R_BADZONE;
			goto cleanup;
		}

		if (zone->db != NULL) {
			unsigned int oldsoacount;

			/*
			 * This is checked in zone_replacedb() for slave zones
			 * as they don't reload from disk.
			 */
			result = zone_get_from_db(zone, zone->db, NULL,
						  &oldsoacount, &oldserial,
						  NULL, NULL, NULL, NULL,
						  NULL);
			RUNTIME_CHECK(result == ISC_R_SUCCESS);
			RUNTIME_CHECK(soacount > 0U);
			if (DNS_ZONE_OPTION(zone, DNS_ZONEOPT_IXFRFROMDIFFS) &&
			    !isc_serial_gt(serial, oldserial)) {
				isc_uint32_t serialmin, serialmax;

				INSIST(zone->type == dns_zone_master);

				if (serial == oldserial &&
				    zone_unchanged(zone->db, db, zone->mctx)) {
					dns_zone_log(zone, ISC_LOG_INFO,
						     "ixfr-from-differences: "
						     "unchanged");
					return(ISC_R_SUCCESS);
				}

				serialmin = (oldserial + 1) & 0xffffffffU;
				serialmax = (oldserial + 0x7fffffffU) &
					     0xffffffffU;
				dns_zone_log(zone, ISC_LOG_ERROR,
					     "ixfr-from-differences: "
					     "new serial (%u) out of range "
					     "[%u - %u]", serial, serialmin,
					     serialmax);
				result = DNS_R_BADZONE;
				goto cleanup;
			} else if (!isc_serial_ge(serial, oldserial))
				dns_zone_log(zone, ISC_LOG_ERROR,
					     "zone serial (%u/%u) has gone "
					     "backwards", serial, oldserial);
			else if (serial == oldserial && !hasinclude &&
				 strcmp(zone->db_argv[0], "_builtin") != 0)
				dns_zone_log(zone, ISC_LOG_ERROR,
					     "zone serial (%u) unchanged. "
					     "zone may fail to transfer "
					     "to slaves.", serial);
		}

		if (zone->type == dns_zone_master &&
		    (zone->update_acl != NULL || zone->ssutable != NULL) &&
		    zone->sigresigninginterval < (3 * refresh) &&
		    dns_db_issecure(db))
		{
			dns_zone_log(zone, ISC_LOG_WARNING,
				     "sig-re-signing-interval less than "
				     "3 * refresh.");
		}

		zone->refresh = RANGE(refresh,
				      zone->minrefresh, zone->maxrefresh);
		zone->retry = RANGE(retry,
				    zone->minretry, zone->maxretry);
		zone->expire = RANGE(expire, zone->refresh + zone->retry,
				     DNS_MAX_EXPIRE);
		zone->minimum = minimum;
		DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_HAVETIMERS);

		if (zone->type == dns_zone_slave ||
		    zone->type == dns_zone_stub ||
		    (zone->type == dns_zone_redirect &&
		     zone->masters != NULL)) {
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

	case dns_zone_key:
		result = sync_keyzone(zone, db);
		if (result != ISC_R_SUCCESS)
			goto cleanup;
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

	/*
	 * Schedule DNSSEC key refresh.
	 */
	if (zone->type == dns_zone_master &&
	    DNS_ZONEKEY_OPTION(zone, DNS_ZONEKEY_MAINTAIN))
		zone->refreshkeytime = now;

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
				 DNS_ZONEFLG_LOADED|
				 DNS_ZONEFLG_NEEDSTARTUPNOTIFY);
		if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_SENDSECURE) &&
		    inline_raw(zone))
		{
			if (zone->secure->db == NULL)
				zone_send_securedb(zone, db);
			else
				zone_send_secureserial(zone, serial);
		}
	}

	/*
	 * Finished loading inline-signing zone; need to get status
	 * from the raw side now.
	 */
	if (zone->type == dns_zone_master && inline_secure(zone))
		maybe_send_secure(zone);


	result = ISC_R_SUCCESS;

	if (needdump) {
		if (zone->type == dns_zone_key)
			zone_needdump(zone, 30);
		else
			zone_needdump(zone, DNS_DUMP_DELAY);
	}

	if (zone->task != NULL) {
		if (zone->type == dns_zone_master) {
			set_resigntime(zone);
			resume_signingwithkey(zone);
			resume_addnsec3chain(zone);
		}

		if (zone->type == dns_zone_master &&
		    !DNS_ZONEKEY_OPTION(zone, DNS_ZONEKEY_NORESIGN) &&
		    dns_zone_isdynamic(zone, ISC_FALSE) &&
		    dns_db_issecure(db)) {
			dns_name_t *name;
			dns_fixedname_t fixed;
			dns_rdataset_t next;

			dns_rdataset_init(&next);
			dns_fixedname_init(&fixed);
			name = dns_fixedname_name(&fixed);

			result = dns_db_getsigningtime(db, &next, name);
			if (result == ISC_R_SUCCESS) {
				isc_stdtime_t timenow;
				char namebuf[DNS_NAME_FORMATSIZE];
				char typebuf[DNS_RDATATYPE_FORMATSIZE];

				isc_stdtime_get(&timenow);
				dns_name_format(name, namebuf, sizeof(namebuf));
				dns_rdatatype_format(next.covers,
						     typebuf, sizeof(typebuf));
				dns_zone_log(zone, ISC_LOG_DEBUG(3),
					     "next resign: %s/%s in %d seconds",
					     namebuf, typebuf,
					     next.resign - timenow -
					     zone->sigresigninginterval);
				dns_rdataset_disassociate(&next);
			} else
				dns_zone_log(zone, ISC_LOG_WARNING,
					     "signed dynamic zone has no "
					     "resign event scheduled");
		}

		zone_settimer(zone, &now);
	}

	if (! dns_db_ispersistent(db))
		dns_zone_log(zone, ISC_LOG_INFO, "loaded serial %u%s", serial,
			     dns_db_issecure(db) ? " (DNSSEC signed)" : "");

	zone->loadtime = loadtime;
	DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_LOADPENDING);
	return (result);

 cleanup:
	if (zone->type == dns_zone_slave ||
	    zone->type == dns_zone_stub ||
	    zone->type == dns_zone_key ||
	    (zone->type == dns_zone_redirect && zone->masters != NULL)) {
		if (result != ISC_R_NOMEMORY) {
			if (zone->journal != NULL)
				zone_saveunique(zone, zone->journal,
						"jn-XXXXXXXX");
			if (zone->masterfile != NULL)
				zone_saveunique(zone, zone->masterfile,
						"db-XXXXXXXX");
		}

		/* Mark the zone for immediate refresh. */
		zone->refreshtime = now;
		if (zone->task != NULL)
			zone_settimer(zone, &now);
		result = ISC_R_SUCCESS;
	} else if (zone->type == dns_zone_master ||
		   zone->type == dns_zone_redirect) {
		if (!(inline_secure(zone) && result == ISC_R_FILENOTFOUND))
			dns_zone_log(zone, ISC_LOG_ERROR,
				     "not loaded due to errors.");
		else if (zone->type == dns_zone_master)
			result = ISC_R_SUCCESS;
	}

	return (result);
}

static isc_boolean_t
exit_check(dns_zone_t *zone) {
	REQUIRE(LOCKED_ZONE(zone));

	if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_SHUTDOWN) && zone->irefs == 0) {
		/*
		 * DNS_ZONEFLG_SHUTDOWN can only be set if erefs == 0.
		 */
		INSIST(isc_refcount_current(&zone->erefs) == 0);
		return (ISC_TRUE);
	}
	return (ISC_FALSE);
}

static isc_boolean_t
zone_check_ns(dns_zone_t *zone, dns_db_t *db, dns_dbversion_t *version,
	      dns_name_t *name, isc_boolean_t logit)
{
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

	result = dns_db_find(db, name, version, dns_rdatatype_a,
			     0, 0, NULL, foundname, NULL, NULL);
	if (result == ISC_R_SUCCESS)
		return (ISC_TRUE);

	if (result == DNS_R_NXRRSET) {
		result = dns_db_find(db, name, version, dns_rdatatype_aaaa,
				     0, 0, NULL, foundname, NULL, NULL);
		if (result == ISC_R_SUCCESS)
			return (ISC_TRUE);
	}

	if (result == DNS_R_NXRRSET || result == DNS_R_NXDOMAIN ||
	    result == DNS_R_EMPTYNAME) {
		if (logit) {
			dns_name_format(name, namebuf, sizeof namebuf);
			dns_zone_log(zone, level, "NS '%s' has no address "
				     "records (A or AAAA)", namebuf);
		}
		return (ISC_FALSE);
	}

	if (result == DNS_R_CNAME) {
		if (logit) {
			dns_name_format(name, namebuf, sizeof namebuf);
			dns_zone_log(zone, level, "NS '%s' is a CNAME "
				     "(illegal)", namebuf);
		}
		return (ISC_FALSE);
	}

	if (result == DNS_R_DNAME) {
		if (logit) {
			dns_name_format(name, namebuf, sizeof namebuf);
			dns_name_format(foundname, altbuf, sizeof altbuf);
			dns_zone_log(zone, level, "NS '%s' is below a DNAME "
				     "'%s' (illegal)", namebuf, altbuf);
		}
		return (ISC_FALSE);
	}

	return (ISC_TRUE);
}

static isc_result_t
zone_count_ns_rr(dns_zone_t *zone, dns_db_t *db, dns_dbnode_t *node,
		 dns_dbversion_t *version, unsigned int *nscount,
		 unsigned int *errors, isc_boolean_t logit)
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
	if (result == ISC_R_NOTFOUND) {
		INSIST(!dns_rdataset_isassociated(&rdataset));
		goto success;
	}
	if (result != ISC_R_SUCCESS) {
		INSIST(!dns_rdataset_isassociated(&rdataset));
		goto invalidate_rdataset;
	}

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
			    !zone_check_ns(zone, db, version, &ns.name, logit))
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
		INSIST(!dns_rdataset_isassociated(&rdataset));
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
	if (result != ISC_R_SUCCESS) {
		INSIST(!dns_rdataset_isassociated(&rdataset));
		goto invalidate_rdataset;
	}

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
	} else {
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
	isc_result_t result;
	isc_result_t answer = ISC_R_SUCCESS;
	dns_dbversion_t *version = NULL;
	dns_dbnode_t *node;

	REQUIRE(db != NULL);
	REQUIRE(zone != NULL);

	dns_db_currentversion(db, &version);

	if (nscount != NULL)
		*nscount = 0;
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
	if (errors != NULL)
		*errors = 0;

	node = NULL;
	result = dns_db_findnode(db, &zone->origin, ISC_FALSE, &node);
	if (result != ISC_R_SUCCESS) {
		answer = result;
		goto closeversion;
	}

	if (nscount != NULL || errors != NULL) {
		result = zone_count_ns_rr(zone, db, node, version,
					  nscount, errors, ISC_TRUE);
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
	dns_zone_t *raw = NULL;
	dns_zone_t *secure = NULL;
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
			 * This zone is being managed.	Post
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
			raw = zone->raw;
			zone->raw = NULL;
			secure = zone->secure;
			zone->secure = NULL;
		}
		UNLOCK_ZONE(zone);
	}
	*zonep = NULL;
	if (free_now) {
		if (raw != NULL)
			dns_zone_detach(&raw);
		if (secure != NULL)
			dns_zone_idetach(&secure);
		zone_free(zone);
	}
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

void
dns_zone_setkeyopt(dns_zone_t *zone, unsigned int keyopt, isc_boolean_t value)
{
	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	if (value)
		zone->keyopts |= keyopt;
	else
		zone->keyopts &= ~keyopt;
	UNLOCK_ZONE(zone);
}

unsigned int
dns_zone_getkeyopts(dns_zone_t *zone) {

	REQUIRE(DNS_ZONE_VALID(zone));

	return (zone->keyopts);
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

static isc_boolean_t
same_addrs(const isc_sockaddr_t *old, const isc_sockaddr_t *new,
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

static void
clear_addresskeylist(isc_sockaddr_t **addrsp, dns_name_t ***keynamesp,
		     unsigned int *countp, isc_mem_t *mctx)
{
	unsigned int count;
	isc_sockaddr_t *addrs;
	dns_name_t **keynames;

	REQUIRE(countp != NULL && addrsp != NULL && keynamesp != NULL);

	count = *countp;
	*countp = 0;
	addrs = *addrsp;
	*addrsp = NULL;
	keynames = *keynamesp;
	*keynamesp = NULL;

	if (addrs != NULL)
		isc_mem_put(mctx, addrs, count * sizeof(isc_sockaddr_t));

	if (keynames != NULL) {
		unsigned int i;
		for (i = 0; i < count; i++) {
			if (keynames[i] != NULL) {
				dns_name_free(keynames[i], mctx);
				isc_mem_put(mctx, keynames[i],
					    sizeof(dns_name_t));
				keynames[i] = NULL;
			}
		}
		isc_mem_put(mctx, keynames, count * sizeof(dns_name_t *));
	}
}

static isc_result_t
set_addrkeylist(unsigned int count,
		const isc_sockaddr_t *addrs, isc_sockaddr_t **newaddrsp,
		dns_name_t **names, dns_name_t ***newnamesp,
		isc_mem_t *mctx)
{
	isc_result_t result;
	isc_sockaddr_t *newaddrs = NULL;
	dns_name_t **newnames = NULL;
	unsigned int i;

	REQUIRE(newaddrsp != NULL && *newaddrsp == NULL);
	REQUIRE(newnamesp != NULL && *newnamesp == NULL);

	newaddrs = isc_mem_get(mctx, count * sizeof(*newaddrs));
	if (newaddrs == NULL)
		return (ISC_R_NOMEMORY);
	memmove(newaddrs, addrs, count * sizeof(*newaddrs));

	newnames = NULL;
	if (names != NULL) {
		newnames = isc_mem_get(mctx, count * sizeof(*newnames));
		if (newnames == NULL) {
			isc_mem_put(mctx, newaddrs, count * sizeof(*newaddrs));
			return (ISC_R_NOMEMORY);
		}
		for (i = 0; i < count; i++)
			newnames[i] = NULL;
		for (i = 0; i < count; i++) {
			if (names[i] != NULL) {
				newnames[i] = isc_mem_get(mctx,
							 sizeof(dns_name_t));
				if (newnames[i] == NULL)
					goto allocfail;
				dns_name_init(newnames[i], NULL);
				result = dns_name_dup(names[i], mctx,
						      newnames[i]);
				if (result != ISC_R_SUCCESS) {
				allocfail:
					for (i = 0; i < count; i++)
						if (newnames[i] != NULL)
							dns_name_free(
							       newnames[i],
							       mctx);
					isc_mem_put(mctx, newaddrs,
						    count * sizeof(*newaddrs));
					isc_mem_put(mctx, newnames,
						    count * sizeof(*newnames));
					return (ISC_R_NOMEMORY);
				}
			}
		}
	}

	*newaddrsp = newaddrs;
	*newnamesp = newnames;
	return (ISC_R_SUCCESS);
}

isc_result_t
dns_zone_setalsonotify(dns_zone_t *zone, const isc_sockaddr_t *notify,
		       isc_uint32_t count)
{
	return (dns_zone_setalsonotifywithkeys(zone, notify, NULL, count));
}

isc_result_t
dns_zone_setalsonotifywithkeys(dns_zone_t *zone, const isc_sockaddr_t *notify,
			       dns_name_t **keynames, isc_uint32_t count)
{
	isc_result_t result;
	isc_sockaddr_t *newaddrs = NULL;
	dns_name_t **newnames = NULL;

	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(count == 0 || notify != NULL);
	if (keynames != NULL)
		REQUIRE(count != 0);

	LOCK_ZONE(zone);

	if (count == zone->notifycnt &&
	    same_addrs(zone->notify, notify, count) &&
	    same_keynames(zone->notifykeynames, keynames, count))
		goto unlock;

	clear_addresskeylist(&zone->notify, &zone->notifykeynames,
			     &zone->notifycnt, zone->mctx);

	if (count == 0)
		goto unlock;

	/*
	 * Set up the notify and notifykey lists
	 */
	result = set_addrkeylist(count, notify, &newaddrs,
				 keynames, &newnames, zone->mctx);
	if (result != ISC_R_SUCCESS)
		goto unlock;

	/*
	 * Everything is ok so attach to the zone.
	 */
	zone->notify = newaddrs;
	zone->notifykeynames = newnames;
	zone->notifycnt = count;
 unlock:
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

isc_result_t
dns_zone_setmasterswithkeys(dns_zone_t *zone,
			    const isc_sockaddr_t *masters,
			    dns_name_t **keynames,
			    isc_uint32_t count)
{
	isc_result_t result = ISC_R_SUCCESS;
	isc_sockaddr_t *newaddrs = NULL;
	dns_name_t **newnames = NULL;
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
	    !same_addrs(zone->masters, masters, count) ||
	    !same_keynames(zone->masterkeynames, keynames, count)) {
		if (zone->request != NULL)
			dns_request_cancel(zone->request);
	} else
		goto unlock;

	/*
	 * This needs to happen before clear_addresskeylist() sets
	 * zone->masterscnt to 0:
	 */
	if (zone->mastersok != NULL) {
		isc_mem_put(zone->mctx, zone->mastersok,
			    zone->masterscnt * sizeof(isc_boolean_t));
		zone->mastersok = NULL;
	}
	clear_addresskeylist(&zone->masters, &zone->masterkeynames,
			     &zone->masterscnt, zone->mctx);
	/*
	 * If count == 0, don't allocate any space for masters, mastersok or
	 * keynames so internally, those pointers are NULL if count == 0
	 */
	if (count == 0)
		goto unlock;

	/*
	 * mastersok must contain count elements
	 */
	newok = isc_mem_get(zone->mctx, count * sizeof(*newok));
	if (newok == NULL) {
		result = ISC_R_NOMEMORY;
		isc_mem_put(zone->mctx, newaddrs, count * sizeof(*newaddrs));
		goto unlock;
	};
	for (i = 0; i < count; i++)
		newok[i] = ISC_FALSE;

	/*
	 * Now set up the masters and masterkey lists
	 */
	result = set_addrkeylist(count, masters, &newaddrs,
				 keynames, &newnames, zone->mctx);
	if (result != ISC_R_SUCCESS) {
		isc_mem_put(zone->mctx, newok, count * sizeof(*newok));
		goto unlock;
	}

	/*
	 * Everything is ok so attach to the zone.
	 */
	zone->curmaster = 0;
	zone->mastersok = newok;
	zone->masters = newaddrs;
	zone->masterkeynames = newnames;
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

void
dns_zone_setdb(dns_zone_t *zone, dns_db_t *db) {
	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(zone->type == dns_zone_staticstub);

	ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_write);
	REQUIRE(zone->db == NULL);
	dns_db_attach(db, &zone->db);
	ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_write);
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

static isc_result_t
find_zone_keys(dns_zone_t *zone, dns_db_t *db, dns_dbversion_t *ver,
	       isc_mem_t *mctx, unsigned int maxkeys,
	       dst_key_t **keys, unsigned int *nkeys)
{
	isc_result_t result;
	dns_dbnode_t *node = NULL;
	const char *directory = dns_zone_getkeydirectory(zone);

	CHECK(dns_db_findnode(db, dns_db_origin(db), ISC_FALSE, &node));
	memset(keys, 0, sizeof(*keys) * maxkeys);
	result = dns_dnssec_findzonekeys2(db, ver, node, dns_db_origin(db),
					  directory, mctx, maxkeys, keys,
					  nkeys);
	if (result == ISC_R_NOTFOUND)
		result = ISC_R_SUCCESS;
 failure:
	if (node != NULL)
		dns_db_detachnode(db, &node);
	return (result);
}

static isc_result_t
offline(dns_db_t *db, dns_dbversion_t *ver, zonediff_t *zonediff,
	dns_name_t *name, dns_ttl_t ttl, dns_rdata_t *rdata)
{
	isc_result_t result;

	if ((rdata->flags & DNS_RDATA_OFFLINE) != 0)
		return (ISC_R_SUCCESS);
	result = update_one_rr(db, ver, zonediff->diff, DNS_DIFFOP_DELRESIGN,
			       name, ttl, rdata);
	if (result != ISC_R_SUCCESS)
		return (result);
	rdata->flags |= DNS_RDATA_OFFLINE;
	result = update_one_rr(db, ver, zonediff->diff, DNS_DIFFOP_ADDRESIGN,
			       name, ttl, rdata);
	zonediff->offline = ISC_TRUE;
	return (result);
}

static void
set_key_expiry_warning(dns_zone_t *zone, isc_stdtime_t when, isc_stdtime_t now)
{
	unsigned int delta;
	char timebuf[80];

	zone->key_expiry = when;
	if (when <= now) {
		dns_zone_log(zone, ISC_LOG_ERROR,
			     "DNSKEY RRSIG(s) have expired");
		isc_time_settoepoch(&zone->keywarntime);
	} else if (when < now + 7 * 24 * 3600) {
		isc_time_t t;
		isc_time_set(&t, when, 0);
		isc_time_formattimestamp(&t, timebuf, 80);
		dns_zone_log(zone, ISC_LOG_WARNING,
			     "DNSKEY RRSIG(s) will expire within 7 days: %s",
			     timebuf);
		delta = when - now;
		delta--;		/* loop prevention */
		delta /= 24 * 3600;	/* to whole days */
		delta *= 24 * 3600;	/* to seconds */
		isc_time_set(&zone->keywarntime, when - delta, 0);
	}  else {
		isc_time_set(&zone->keywarntime, when - 7 * 24 * 3600, 0);
		isc_time_formattimestamp(&zone->refreshkeytime, timebuf, 80);
		dns_zone_log(zone, ISC_LOG_NOTICE,
			     "setting keywarntime to %s", timebuf);
	}
}

/*
 * Helper function to del_sigs(). We don't want to delete RRSIGs that
 * have no new key.
 */
static isc_boolean_t
delsig_ok(dns_rdata_rrsig_t *rrsig_ptr, dst_key_t **keys, unsigned int nkeys,
	  isc_boolean_t *warn)
{
	unsigned int i = 0;
	isc_boolean_t have_ksk = ISC_FALSE, have_zsk = ISC_FALSE;
	isc_boolean_t have_pksk = ISC_FALSE, have_pzsk = ISC_FALSE;

	for (i = 0; i < nkeys; i++) {
		if (rrsig_ptr->algorithm != dst_key_alg(keys[i]))
			continue;
		if (dst_key_isprivate(keys[i])) {
			if (KSK(keys[i]))
				have_ksk = have_pksk = ISC_TRUE;
			else
				have_zsk = have_pzsk = ISC_TRUE;
		} else {
			if (KSK(keys[i]))
				have_ksk = ISC_TRUE;
			else
				have_zsk = ISC_TRUE;
		}
	}

	if (have_zsk && have_ksk && !have_pzsk)
		*warn = ISC_TRUE;

	/*
	 * It's okay to delete a signature if there is an active key
	 * with the same algorithm to replace it.
	 */
	if (have_pksk || have_pzsk)
		return (ISC_TRUE);

	/*
	 * Failing that, it is *not* okay to delete a signature
	 * if the associated public key is still in the DNSKEY RRset
	 */
	for (i = 0; i < nkeys; i++) {
		if ((rrsig_ptr->algorithm == dst_key_alg(keys[i])) &&
		    (rrsig_ptr->keyid == dst_key_id(keys[i])))
			return (ISC_FALSE);
	}

	/*
	 * But if the key is gone, then go ahead.
	 */
	return (ISC_TRUE);
}

/*
 * Delete expired RRsigs and any RRsigs we are about to re-sign.
 * See also update.c:del_keysigs().
 */
static isc_result_t
del_sigs(dns_zone_t *zone, dns_db_t *db, dns_dbversion_t *ver, dns_name_t *name,
	 dns_rdatatype_t type, zonediff_t *zonediff, dst_key_t **keys,
	 unsigned int nkeys, isc_stdtime_t now, isc_boolean_t incremental)
{
	isc_result_t result;
	dns_dbnode_t *node = NULL;
	dns_rdataset_t rdataset;
	unsigned int i;
	dns_rdata_rrsig_t rrsig;
	isc_boolean_t found;
	isc_int64_t timewarn = 0, timemaybe = 0;

	dns_rdataset_init(&rdataset);

	if (type == dns_rdatatype_nsec3)
		result = dns_db_findnsec3node(db, name, ISC_FALSE, &node);
	else
		result = dns_db_findnode(db, name, ISC_FALSE, &node);
	if (result == ISC_R_NOTFOUND)
		return (ISC_R_SUCCESS);
	if (result != ISC_R_SUCCESS)
		goto failure;
	result = dns_db_findrdataset(db, node, ver, dns_rdatatype_rrsig, type,
				     (isc_stdtime_t) 0, &rdataset, NULL);
	dns_db_detachnode(db, &node);

	if (result == ISC_R_NOTFOUND) {
		INSIST(!dns_rdataset_isassociated(&rdataset));
		return (ISC_R_SUCCESS);
	}
	if (result != ISC_R_SUCCESS) {
		INSIST(!dns_rdataset_isassociated(&rdataset));
		goto failure;
	}

	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset)) {
		dns_rdata_t rdata = DNS_RDATA_INIT;

		dns_rdataset_current(&rdataset, &rdata);
		result = dns_rdata_tostruct(&rdata, &rrsig, NULL);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);

		if (type != dns_rdatatype_dnskey) {
			isc_boolean_t warn = ISC_FALSE, deleted = ISC_FALSE;
			if (delsig_ok(&rrsig, keys, nkeys, &warn)) {
				result = update_one_rr(db, ver, zonediff->diff,
					       DNS_DIFFOP_DELRESIGN, name,
					       rdataset.ttl, &rdata);
				if (result != ISC_R_SUCCESS)
					break;
				deleted = ISC_TRUE;
			}
			if (warn) {
				/*
				 * At this point, we've got an RRSIG,
				 * which is signed by an inactive key.
				 * An administrator needs to provide a new
				 * key/alg, but until that time, we want to
				 * keep the old RRSIG.  Marking the key as
				 * offline will prevent us spinning waiting
				 * for the private part.
				 */
				if (incremental && !deleted) {
					result = offline(db, ver, zonediff,
							 name, rdataset.ttl,
							 &rdata);
					if (result != ISC_R_SUCCESS)
						break;
				}

				/*
				 * Log the key id and algorithm of
				 * the inactive key with no replacement
				 */
				if (zone->log_key_expired_timer <= now) {
					char origin[DNS_NAME_FORMATSIZE];
					char algbuf[DNS_NAME_FORMATSIZE];
					dns_name_format(&zone->origin, origin,
							sizeof(origin));
					dns_secalg_format(rrsig.algorithm,
							  algbuf,
							  sizeof(algbuf));
					dns_zone_log(zone, ISC_LOG_WARNING,
						     "Key %s/%s/%d "
						     "missing or inactive "
						     "and has no replacement: "
						     "retaining signatures.",
						     origin, algbuf,
						     rrsig.keyid);
					zone->log_key_expired_timer = now +
									3600;
				}
			}
			continue;
		}

		/*
		 * RRSIG(DNSKEY) requires special processing.
		 */
		found = ISC_FALSE;
		for (i = 0; i < nkeys; i++) {
			if (rrsig.algorithm == dst_key_alg(keys[i]) &&
			    rrsig.keyid == dst_key_id(keys[i])) {
				found = ISC_TRUE;
				/*
				 * Mark offline RRSIG(DNSKEY).
				 * We want the earliest offline expire time
				 * iff there is a new offline signature.
				 */
				if (!dst_key_inactive(keys[i]) &&
				    !dst_key_isprivate(keys[i]))
				{
					isc_int64_t timeexpire =
					   dns_time64_from32(rrsig.timeexpire);
					if (timewarn != 0 &&
					    timewarn > timeexpire)
						timewarn = timeexpire;
					if (rdata.flags & DNS_RDATA_OFFLINE) {
						if (timemaybe == 0 ||
						    timemaybe > timeexpire)
							timemaybe = timeexpire;
						break;
					}
					if (timewarn == 0)
						timewarn = timemaybe;
					if (timewarn == 0 ||
					    timewarn > timeexpire)
						timewarn = timeexpire;
					result = offline(db, ver, zonediff,
							 name, rdataset.ttl,
							 &rdata);
					break;
				}
				result = update_one_rr(db, ver, zonediff->diff,
						       DNS_DIFFOP_DELRESIGN,
						       name, rdataset.ttl,
						       &rdata);
				break;
			}
		}

		/*
		 * If there is not a matching DNSKEY then
		 * delete the RRSIG.
		 */
		if (!found)
			result = update_one_rr(db, ver, zonediff->diff,
					       DNS_DIFFOP_DELRESIGN, name,
					       rdataset.ttl, &rdata);
		if (result != ISC_R_SUCCESS)
			break;
	}

	dns_rdataset_disassociate(&rdataset);
	if (result == ISC_R_NOMORE)
		result = ISC_R_SUCCESS;
	if (timewarn > 0) {
#if defined(STDTIME_ON_32BITS)
		isc_stdtime_t stdwarn = (isc_stdtime_t)timewarn;
		if (timewarn == stdwarn)
#endif
			set_key_expiry_warning(zone, (isc_stdtime_t)timewarn,
					       now);
#if defined(STDTIME_ON_32BITS)
		else
			dns_zone_log(zone, ISC_LOG_ERROR,
				     "key expiry warning time out of range");
#endif
	}
 failure:
	if (node != NULL)
		dns_db_detachnode(db, &node);
	return (result);
}

static isc_result_t
add_sigs(dns_db_t *db, dns_dbversion_t *ver, dns_name_t *name,
	 dns_rdatatype_t type, dns_diff_t *diff, dst_key_t **keys,
	 unsigned int nkeys, isc_mem_t *mctx, isc_stdtime_t inception,
	 isc_stdtime_t expire, isc_boolean_t check_ksk,
	 isc_boolean_t keyset_kskonly)
{
	isc_result_t result;
	dns_dbnode_t *node = NULL;
	dns_rdataset_t rdataset;
	dns_rdata_t sig_rdata = DNS_RDATA_INIT;
	unsigned char data[1024]; /* XXX */
	isc_buffer_t buffer;
	unsigned int i, j;

	dns_rdataset_init(&rdataset);
	isc_buffer_init(&buffer, data, sizeof(data));

	if (type == dns_rdatatype_nsec3)
		result = dns_db_findnsec3node(db, name, ISC_FALSE, &node);
	else
		result = dns_db_findnode(db, name, ISC_FALSE, &node);
	if (result == ISC_R_NOTFOUND)
		return (ISC_R_SUCCESS);
	if (result != ISC_R_SUCCESS)
		goto failure;
	result = dns_db_findrdataset(db, node, ver, type, 0,
				     (isc_stdtime_t) 0, &rdataset, NULL);
	dns_db_detachnode(db, &node);
	if (result == ISC_R_NOTFOUND) {
		INSIST(!dns_rdataset_isassociated(&rdataset));
		return (ISC_R_SUCCESS);
	}
	if (result != ISC_R_SUCCESS) {
		INSIST(!dns_rdataset_isassociated(&rdataset));
		goto failure;
	}

	for (i = 0; i < nkeys; i++) {
		isc_boolean_t both = ISC_FALSE;

		if (!dst_key_isprivate(keys[i]))
			continue;

		if (check_ksk && !REVOKE(keys[i])) {
			isc_boolean_t have_ksk, have_nonksk;
			if (KSK(keys[i])) {
				have_ksk = ISC_TRUE;
				have_nonksk = ISC_FALSE;
			} else {
				have_ksk = ISC_FALSE;
				have_nonksk = ISC_TRUE;
			}
			for (j = 0; j < nkeys; j++) {
				if (j == i || ALG(keys[i]) != ALG(keys[j]))
					continue;
				if (REVOKE(keys[j]))
					continue;
				if (KSK(keys[j]))
					have_ksk = ISC_TRUE;
				else
					have_nonksk = ISC_TRUE;
				both = have_ksk && have_nonksk;
				if (both)
					break;
			}
		}
		if (both) {
			if (type == dns_rdatatype_dnskey) {
				if (!KSK(keys[i]) && keyset_kskonly)
					continue;
			} else if (KSK(keys[i]))
				continue;
		} else if (REVOKE(keys[i]) && type != dns_rdatatype_dnskey)
				continue;

		/* Calculate the signature, creating a RRSIG RDATA. */
		isc_buffer_clear(&buffer);
		CHECK(dns_dnssec_sign(name, &rdataset, keys[i],
				      &inception, &expire,
				      mctx, &buffer, &sig_rdata));
		/* Update the database and journal with the RRSIG. */
		/* XXX inefficient - will cause dataset merging */
		CHECK(update_one_rr(db, ver, diff, DNS_DIFFOP_ADDRESIGN,
				    name, rdataset.ttl, &sig_rdata));
		dns_rdata_reset(&sig_rdata);
		isc_buffer_init(&buffer, data, sizeof(data));
	}

 failure:
	if (dns_rdataset_isassociated(&rdataset))
		dns_rdataset_disassociate(&rdataset);
	if (node != NULL)
		dns_db_detachnode(db, &node);
	return (result);
}

static void
zone_resigninc(dns_zone_t *zone) {
	const char *me = "zone_resigninc";
	dns_db_t *db = NULL;
	dns_dbversion_t *version = NULL;
	dns_diff_t _sig_diff;
	zonediff_t zonediff;
	dns_fixedname_t fixed;
	dns_name_t *name;
	dns_rdataset_t rdataset;
	dns_rdatatype_t covers;
	dst_key_t *zone_keys[DNS_MAXZONEKEYS];
	isc_boolean_t check_ksk, keyset_kskonly = ISC_FALSE;
	isc_result_t result;
	isc_stdtime_t now, inception, soaexpire, expire, stop;
	isc_uint32_t jitter;
	unsigned int i;
	unsigned int nkeys = 0;
	unsigned int resign;

	ENTER;

	dns_rdataset_init(&rdataset);
	dns_fixedname_init(&fixed);
	dns_diff_init(zone->mctx, &_sig_diff);
	zonediff_init(&zonediff, &_sig_diff);

	/*
	 * Zone is frozen or automatic resigning is disabled.
	 * Pause for 5 minutes.
	 */
	if (zone->update_disabled ||
	    DNS_ZONEKEY_OPTION(zone, DNS_ZONEKEY_NORESIGN))
	{
		result = ISC_R_FAILURE;
		goto failure;
	}

	ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_read);
	dns_db_attach(zone->db, &db);
	ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);

	result = dns_db_newversion(db, &version);
	if (result != ISC_R_SUCCESS) {
		dns_zone_log(zone, ISC_LOG_ERROR,
			     "zone_resigninc:dns_db_newversion -> %s",
			     dns_result_totext(result));
		goto failure;
	}

	result = find_zone_keys(zone, db, version, zone->mctx, DNS_MAXZONEKEYS,
				zone_keys, &nkeys);
	if (result != ISC_R_SUCCESS) {
		dns_zone_log(zone, ISC_LOG_ERROR,
			     "zone_resigninc:find_zone_keys -> %s",
			     dns_result_totext(result));
		goto failure;
	}

	isc_stdtime_get(&now);
	inception = now - 3600;	/* Allow for clock skew. */
	soaexpire = now + dns_zone_getsigvalidityinterval(zone);
	/*
	 * Spread out signatures over time if they happen to be
	 * clumped.  We don't do this for each add_sigs() call as
	 * we still want some clustering to occur.
	 */
	isc_random_get(&jitter);
	expire = soaexpire - jitter % 3600;
	stop = now + 5;

	check_ksk = DNS_ZONE_OPTION(zone, DNS_ZONEOPT_UPDATECHECKKSK);
	keyset_kskonly = DNS_ZONE_OPTION(zone, DNS_ZONEOPT_DNSKEYKSKONLY);

	name = dns_fixedname_name(&fixed);
	result = dns_db_getsigningtime(db, &rdataset, name);
	if (result != ISC_R_SUCCESS && result != ISC_R_NOTFOUND) {
		dns_zone_log(zone, ISC_LOG_ERROR,
			     "zone_resigninc:dns_db_getsigningtime -> %s",
			     dns_result_totext(result));
	}

	i = 0;
	while (result == ISC_R_SUCCESS) {
		resign = rdataset.resign - zone->sigresigninginterval;
		covers = rdataset.covers;
		dns_rdataset_disassociate(&rdataset);

		/*
		 * Stop if we hit the SOA as that means we have walked the
		 * entire zone.  The SOA record should always be the most
		 * recent signature.
		 */
		/* XXXMPA increase number of RRsets signed pre call */
		if (covers == dns_rdatatype_soa || i++ > zone->signatures ||
		    resign > stop)
			break;

		result = del_sigs(zone, db, version, name, covers, &zonediff,
				  zone_keys, nkeys, now, ISC_TRUE);
		if (result != ISC_R_SUCCESS) {
			dns_zone_log(zone, ISC_LOG_ERROR,
				     "zone_resigninc:del_sigs -> %s",
				     dns_result_totext(result));
			break;
		}

		result = add_sigs(db, version, name, covers, zonediff.diff,
				  zone_keys, nkeys, zone->mctx, inception,
				  expire, check_ksk, keyset_kskonly);
		if (result != ISC_R_SUCCESS) {
			dns_zone_log(zone, ISC_LOG_ERROR,
				     "zone_resigninc:add_sigs -> %s",
				     dns_result_totext(result));
			break;
		}
		result	= dns_db_getsigningtime(db, &rdataset, name);
		if (nkeys == 0 && result == ISC_R_NOTFOUND) {
			result = ISC_R_SUCCESS;
			break;
		}
		if (result != ISC_R_SUCCESS)
			dns_zone_log(zone, ISC_LOG_ERROR,
			     "zone_resigninc:dns_db_getsigningtime -> %s",
				     dns_result_totext(result));
	}

	if (result != ISC_R_NOMORE && result != ISC_R_SUCCESS)
		goto failure;

	result = del_sigs(zone, db, version, &zone->origin, dns_rdatatype_soa,
			  &zonediff, zone_keys, nkeys, now, ISC_TRUE);
	if (result != ISC_R_SUCCESS) {
		dns_zone_log(zone, ISC_LOG_ERROR,
			     "zone_resigninc:del_sigs -> %s",
			     dns_result_totext(result));
		goto failure;
	}

	/*
	 * Did we change anything in the zone?
	 */
	if (ISC_LIST_EMPTY(zonediff.diff->tuples)) {
		/*
		 * Commit the changes if any key has been marked as offline.			 */
		if (zonediff.offline)
			dns_db_closeversion(db, &version, ISC_TRUE);
		goto failure;
	}

	/* Increment SOA serial if we have made changes */
	result = update_soa_serial(db, version, zonediff.diff, zone->mctx,
				   zone->updatemethod);
	if (result != ISC_R_SUCCESS) {
		dns_zone_log(zone, ISC_LOG_ERROR,
			     "zone_resigninc:update_soa_serial -> %s",
			     dns_result_totext(result));
		goto failure;
	}

	/*
	 * Generate maximum life time signatures so that the above loop
	 * termination is sensible.
	 */
	result = add_sigs(db, version, &zone->origin, dns_rdatatype_soa,
			  zonediff.diff, zone_keys, nkeys, zone->mctx,
			  inception, soaexpire, check_ksk, keyset_kskonly);
	if (result != ISC_R_SUCCESS) {
		dns_zone_log(zone, ISC_LOG_ERROR,
			     "zone_resigninc:add_sigs -> %s",
			     dns_result_totext(result));
		goto failure;
	}

	/* Write changes to journal file. */
	CHECK(zone_journal(zone, zonediff.diff, NULL, "zone_resigninc"));

	/* Everything has succeeded. Commit the changes. */
	dns_db_closeversion(db, &version, ISC_TRUE);

 failure:
	dns_diff_clear(&_sig_diff);
	for (i = 0; i < nkeys; i++)
		dst_key_free(&zone_keys[i]);
	if (version != NULL) {
		dns_db_closeversion(zone->db, &version, ISC_FALSE);
		dns_db_detach(&db);
	} else if (db != NULL)
		dns_db_detach(&db);
	if (result == ISC_R_SUCCESS) {
		set_resigntime(zone);
		LOCK_ZONE(zone);
		zone_needdump(zone, DNS_DUMP_DELAY);
		DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_NEEDNOTIFY);
		UNLOCK_ZONE(zone);
	} else {
		/*
		 * Something failed.  Retry in 5 minutes.
		 */
		isc_interval_t ival;
		isc_interval_set(&ival, 300, 0);
		isc_time_nowplusinterval(&zone->resigntime, &ival);
	}

	INSIST(version == NULL);
}

static isc_result_t
next_active(dns_db_t *db, dns_dbversion_t *version, dns_name_t *oldname,
	    dns_name_t *newname, isc_boolean_t bottom)
{
	isc_result_t result;
	dns_dbiterator_t *dbit = NULL;
	dns_rdatasetiter_t *rdsit = NULL;
	dns_dbnode_t *node = NULL;

	CHECK(dns_db_createiterator(db, DNS_DB_NONSEC3, &dbit));
	CHECK(dns_dbiterator_seek(dbit, oldname));
	do {
		result = dns_dbiterator_next(dbit);
		if (result == ISC_R_NOMORE)
			CHECK(dns_dbiterator_first(dbit));
		CHECK(dns_dbiterator_current(dbit, &node, newname));
		if (bottom && dns_name_issubdomain(newname, oldname) &&
		    !dns_name_equal(newname, oldname)) {
			dns_db_detachnode(db, &node);
			continue;
		}
		/*
		 * Is this node empty?
		 */
		CHECK(dns_db_allrdatasets(db, node, version, 0, &rdsit));
		result = dns_rdatasetiter_first(rdsit);
		dns_db_detachnode(db, &node);
		dns_rdatasetiter_destroy(&rdsit);
		if (result != ISC_R_NOMORE)
			break;
	} while (1);
 failure:
	if (node != NULL)
		dns_db_detachnode(db, &node);
	if (dbit != NULL)
		dns_dbiterator_destroy(&dbit);
	return (result);
}

static isc_boolean_t
signed_with_key(dns_db_t *db, dns_dbnode_t *node, dns_dbversion_t *version,
		dns_rdatatype_t type, dst_key_t *key)
{
	isc_result_t result;
	dns_rdataset_t rdataset;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdata_rrsig_t rrsig;

	dns_rdataset_init(&rdataset);
	result = dns_db_findrdataset(db, node, version, dns_rdatatype_rrsig,
				     type, 0, &rdataset, NULL);
	if (result != ISC_R_SUCCESS) {
		INSIST(!dns_rdataset_isassociated(&rdataset));
		return (ISC_FALSE);
	}
	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset)) {
		dns_rdataset_current(&rdataset, &rdata);
		result = dns_rdata_tostruct(&rdata, &rrsig, NULL);
		INSIST(result == ISC_R_SUCCESS);
		if (rrsig.algorithm == dst_key_alg(key) &&
		    rrsig.keyid == dst_key_id(key)) {
			dns_rdataset_disassociate(&rdataset);
			return (ISC_TRUE);
		}
		dns_rdata_reset(&rdata);
	}
	dns_rdataset_disassociate(&rdataset);
	return (ISC_FALSE);
}

static isc_result_t
add_nsec(dns_db_t *db, dns_dbversion_t *version, dns_name_t *name,
	 dns_dbnode_t *node, dns_ttl_t ttl, isc_boolean_t bottom,
	 dns_diff_t *diff)
{
	dns_fixedname_t fixed;
	dns_name_t *next;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	isc_result_t result;
	unsigned char nsecbuffer[DNS_NSEC_BUFFERSIZE];

	dns_fixedname_init(&fixed);
	next = dns_fixedname_name(&fixed);

	CHECK(next_active(db, version, name, next, bottom));
	CHECK(dns_nsec_buildrdata(db, version, node, next, nsecbuffer,
				  &rdata));
	CHECK(update_one_rr(db, version, diff, DNS_DIFFOP_ADD, name, ttl,
			    &rdata));
 failure:
	return (result);
}

static isc_result_t
sign_a_node(dns_db_t *db, dns_name_t *name, dns_dbnode_t *node,
	    dns_dbversion_t *version, isc_boolean_t build_nsec3,
	    isc_boolean_t build_nsec, dst_key_t *key,
	    isc_stdtime_t inception, isc_stdtime_t expire,
	    unsigned int minimum, isc_boolean_t is_ksk,
	    isc_boolean_t keyset_kskonly, isc_boolean_t *delegation,
	    dns_diff_t *diff, isc_int32_t *signatures, isc_mem_t *mctx)
{
	isc_result_t result;
	dns_rdatasetiter_t *iterator = NULL;
	dns_rdataset_t rdataset;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	isc_buffer_t buffer;
	unsigned char data[1024];
	isc_boolean_t seen_soa, seen_ns, seen_rr, seen_dname, seen_nsec,
		      seen_nsec3, seen_ds;
	isc_boolean_t bottom;

	result = dns_db_allrdatasets(db, node, version, 0, &iterator);
	if (result != ISC_R_SUCCESS) {
		if (result == ISC_R_NOTFOUND)
			result = ISC_R_SUCCESS;
		return (result);
	}

	dns_rdataset_init(&rdataset);
	isc_buffer_init(&buffer, data, sizeof(data));
	seen_rr = seen_soa = seen_ns = seen_dname = seen_nsec =
	seen_nsec3 = seen_ds = ISC_FALSE;
	for (result = dns_rdatasetiter_first(iterator);
	     result == ISC_R_SUCCESS;
	     result = dns_rdatasetiter_next(iterator)) {
		dns_rdatasetiter_current(iterator, &rdataset);
		if (rdataset.type == dns_rdatatype_soa)
			seen_soa = ISC_TRUE;
		else if (rdataset.type == dns_rdatatype_ns)
			seen_ns = ISC_TRUE;
		else if (rdataset.type == dns_rdatatype_ds)
			seen_ds = ISC_TRUE;
		else if (rdataset.type == dns_rdatatype_dname)
			seen_dname = ISC_TRUE;
		else if (rdataset.type == dns_rdatatype_nsec)
			seen_nsec = ISC_TRUE;
		else if (rdataset.type == dns_rdatatype_nsec3)
			seen_nsec3 = ISC_TRUE;
		if (rdataset.type != dns_rdatatype_rrsig)
			seen_rr = ISC_TRUE;
		dns_rdataset_disassociate(&rdataset);
	}
	if (result != ISC_R_NOMORE)
		goto failure;
	if (seen_ns && !seen_soa)
		*delegation = ISC_TRUE;
	/*
	 * Going from insecure to NSEC3.
	 * Don't generate NSEC3 records for NSEC3 records.
	 */
	if (build_nsec3 && !seen_nsec3 && seen_rr) {
		isc_boolean_t unsecure = !seen_ds && seen_ns && !seen_soa;
		CHECK(dns_nsec3_addnsec3s(db, version, name, minimum,
					  unsecure, diff));
		(*signatures)--;
	}
	/*
	 * Going from insecure to NSEC.
	 * Don't generate NSEC records for NSEC3 records.
	 */
	if (build_nsec && !seen_nsec3 && !seen_nsec && seen_rr) {
		/* Build and add NSEC. */
		bottom = (seen_ns && !seen_soa) || seen_dname;
		/*
		 * Build a NSEC record except at the origin.
		 */
		if (!dns_name_equal(name, dns_db_origin(db))) {
			CHECK(add_nsec(db, version, name, node, minimum,
				       bottom, diff));
			/* Count a NSEC generation as a signature generation. */
			(*signatures)--;
		}
	}
	result = dns_rdatasetiter_first(iterator);
	while (result == ISC_R_SUCCESS) {
		dns_rdatasetiter_current(iterator, &rdataset);
		if (rdataset.type == dns_rdatatype_soa ||
		    rdataset.type == dns_rdatatype_rrsig)
			goto next_rdataset;
		if (rdataset.type == dns_rdatatype_dnskey) {
			if (!is_ksk && keyset_kskonly)
				goto next_rdataset;
		} else if (is_ksk) {
			/*
			 * CDS and CDNSKEY are signed with KSK (RFC 7344, 4.1).
			 */
			if (rdataset.type != dns_rdatatype_cds &&
			    rdataset.type != dns_rdatatype_cdnskey)
				goto next_rdataset;
		}
		if (*delegation &&
		    rdataset.type != dns_rdatatype_ds &&
		    rdataset.type != dns_rdatatype_nsec)
			goto next_rdataset;
		if (signed_with_key(db, node, version, rdataset.type, key))
			goto next_rdataset;
		/* Calculate the signature, creating a RRSIG RDATA. */
		isc_buffer_clear(&buffer);
		CHECK(dns_dnssec_sign(name, &rdataset, key, &inception,
				      &expire, mctx, &buffer, &rdata));
		/* Update the database and journal with the RRSIG. */
		/* XXX inefficient - will cause dataset merging */
		CHECK(update_one_rr(db, version, diff, DNS_DIFFOP_ADDRESIGN,
				    name, rdataset.ttl, &rdata));
		dns_rdata_reset(&rdata);
		(*signatures)--;
 next_rdataset:
		dns_rdataset_disassociate(&rdataset);
		result = dns_rdatasetiter_next(iterator);
	}
	if (result == ISC_R_NOMORE)
		result = ISC_R_SUCCESS;
	if (seen_dname)
		*delegation = ISC_TRUE;
 failure:
	if (dns_rdataset_isassociated(&rdataset))
		dns_rdataset_disassociate(&rdataset);
	if (iterator != NULL)
		dns_rdatasetiter_destroy(&iterator);
	return (result);
}

/*
 * If 'update_only' is set then don't create a NSEC RRset if it doesn't exist.
 */
static isc_result_t
updatesecure(dns_db_t *db, dns_dbversion_t *version, dns_name_t *name,
	     dns_ttl_t minimum, isc_boolean_t update_only, dns_diff_t *diff)
{
	isc_result_t result;
	dns_rdataset_t rdataset;
	dns_dbnode_t *node = NULL;

	CHECK(dns_db_getoriginnode(db, &node));
	if (update_only) {
		dns_rdataset_init(&rdataset);
		result = dns_db_findrdataset(db, node, version,
					     dns_rdatatype_nsec,
					     dns_rdatatype_none,
					     0, &rdataset, NULL);
		if (dns_rdataset_isassociated(&rdataset))
			dns_rdataset_disassociate(&rdataset);
		if (result == ISC_R_NOTFOUND)
			goto success;
		if (result != ISC_R_SUCCESS)
			goto failure;
	}
	CHECK(delete_nsec(db, version, node, name, diff));
	CHECK(add_nsec(db, version, name, node, minimum, ISC_FALSE, diff));
 success:
	result = ISC_R_SUCCESS;
 failure:
	if (node != NULL)
		dns_db_detachnode(db, &node);
	return (result);
}

static isc_result_t
updatesignwithkey(dns_zone_t *zone, dns_signing_t *signing,
		  dns_dbversion_t *version, isc_boolean_t build_nsec3,
		  dns_ttl_t minimum, dns_diff_t *diff)
{
	isc_result_t result;
	dns_dbnode_t *node = NULL;
	dns_rdataset_t rdataset;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	unsigned char data[5];
	isc_boolean_t seen_done = ISC_FALSE;
	isc_boolean_t have_rr = ISC_FALSE;

	dns_rdataset_init(&rdataset);
	result = dns_db_getoriginnode(signing->db, &node);
	if (result != ISC_R_SUCCESS)
		goto failure;

	result = dns_db_findrdataset(signing->db, node, version,
				     zone->privatetype, dns_rdatatype_none,
				     0, &rdataset, NULL);
	if (result == ISC_R_NOTFOUND) {
		INSIST(!dns_rdataset_isassociated(&rdataset));
		result = ISC_R_SUCCESS;
		goto failure;
	}
	if (result != ISC_R_SUCCESS) {
		INSIST(!dns_rdataset_isassociated(&rdataset));
		goto failure;
	}
	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset)) {
		dns_rdataset_current(&rdataset, &rdata);
		/*
		 * If we don't match the algorithm or keyid skip the record.
		 */
		if (rdata.length != 5 ||
		    rdata.data[0] != signing->algorithm ||
		    rdata.data[1] != ((signing->keyid >> 8) & 0xff) ||
		    rdata.data[2] != (signing->keyid & 0xff)) {
			have_rr = ISC_TRUE;
			dns_rdata_reset(&rdata);
			continue;
		}
		/*
		 * We have a match.  If we were signing (!signing->delete)
		 * and we already have a record indicating that we have
		 * finished signing (rdata.data[4] != 0) then keep it.
		 * Otherwise it needs to be deleted as we have removed all
		 * the signatures (signing->delete), so any record indicating
		 * completion is now out of date, or we have finished signing
		 * with the new record so we no longer need to remember that
		 * we need to sign the zone with the matching key across a
		 * nameserver re-start.
		 */
		if (!signing->delete && rdata.data[4] != 0) {
			seen_done = ISC_TRUE;
			have_rr = ISC_TRUE;
		} else
			CHECK(update_one_rr(signing->db, version, diff,
					    DNS_DIFFOP_DEL, &zone->origin,
					    rdataset.ttl, &rdata));
		dns_rdata_reset(&rdata);
	}
	if (result == ISC_R_NOMORE)
		result = ISC_R_SUCCESS;
	if (!signing->delete && !seen_done) {
		/*
		 * If we were signing then we need to indicate that we have
		 * finished signing the zone with this key.  If it is already
		 * there we don't need to add it a second time.
		 */
		data[0] = signing->algorithm;
		data[1] = (signing->keyid >> 8) & 0xff;
		data[2] = signing->keyid & 0xff;
		data[3] = 0;
		data[4] = 1;
		rdata.length = sizeof(data);
		rdata.data = data;
		rdata.type = zone->privatetype;
		rdata.rdclass = dns_db_class(signing->db);
		CHECK(update_one_rr(signing->db, version, diff, DNS_DIFFOP_ADD,
				    &zone->origin, rdataset.ttl, &rdata));
	} else if (!have_rr) {
		dns_name_t *origin = dns_db_origin(signing->db);
		/*
		 * Rebuild the NSEC/NSEC3 record for the origin as we no
		 * longer have any private records.
		 */
		if (build_nsec3)
			CHECK(dns_nsec3_addnsec3s(signing->db, version, origin,
						  minimum, ISC_FALSE, diff));
		CHECK(updatesecure(signing->db, version, origin, minimum,
				   ISC_TRUE, diff));
	}

 failure:
	if (dns_rdataset_isassociated(&rdataset))
		dns_rdataset_disassociate(&rdataset);
	if (node != NULL)
		dns_db_detachnode(signing->db, &node);
	return (result);
}

/*
 * If 'active' is set then we are not done with the chain yet so only
 * delete the nsec3param record which indicates a full chain exists
 * (flags == 0).
 */
static isc_result_t
fixup_nsec3param(dns_db_t *db, dns_dbversion_t *ver, dns_nsec3chain_t *chain,
		 isc_boolean_t active, dns_rdatatype_t privatetype,
		 dns_diff_t *diff)
{
	dns_dbnode_t *node = NULL;
	dns_name_t *name = dns_db_origin(db);
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdataset_t rdataset;
	dns_rdata_nsec3param_t nsec3param;
	isc_result_t result;
	isc_buffer_t buffer;
	unsigned char parambuf[DNS_NSEC3PARAM_BUFFERSIZE];
	dns_ttl_t ttl = 0;
	isc_boolean_t nseconly = ISC_FALSE, nsec3ok = ISC_FALSE;

	dns_rdataset_init(&rdataset);

	result = dns_db_getoriginnode(db, &node);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);
	result = dns_db_findrdataset(db, node, ver, dns_rdatatype_nsec3param,
				     0, 0, &rdataset, NULL);
	if (result == ISC_R_NOTFOUND)
		goto try_private;
	if (result != ISC_R_SUCCESS)
		goto failure;

	/*
	 * Preserve the existing ttl.
	 */
	ttl = rdataset.ttl;

	/*
	 * Delete all NSEC3PARAM records which match that in nsec3chain.
	 */
	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset)) {

		dns_rdataset_current(&rdataset, &rdata);
		CHECK(dns_rdata_tostruct(&rdata, &nsec3param, NULL));

		if (nsec3param.hash != chain->nsec3param.hash ||
		    (active && nsec3param.flags != 0) ||
		    nsec3param.iterations != chain->nsec3param.iterations ||
		    nsec3param.salt_length != chain->nsec3param.salt_length ||
		    memcmp(nsec3param.salt, chain->nsec3param.salt,
			   nsec3param.salt_length)) {
			dns_rdata_reset(&rdata);
			continue;
		}

		CHECK(update_one_rr(db, ver, diff, DNS_DIFFOP_DEL,
				    name, rdataset.ttl, &rdata));
		dns_rdata_reset(&rdata);
	}
	if (result != ISC_R_NOMORE)
		goto failure;

	dns_rdataset_disassociate(&rdataset);

 try_private:

	if (active)
		goto add;

	result = dns_nsec_nseconly(db, ver, &nseconly);
	nsec3ok = (result == ISC_R_SUCCESS && !nseconly);

	/*
	 * Delete all private records which match that in nsec3chain.
	 */
	result = dns_db_findrdataset(db, node, ver, privatetype,
				     0, 0, &rdataset, NULL);
	if (result == ISC_R_NOTFOUND)
		goto add;
	if (result != ISC_R_SUCCESS)
		goto failure;

	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset)) {
		dns_rdata_t private = DNS_RDATA_INIT;
		unsigned char buf[DNS_NSEC3PARAM_BUFFERSIZE];

		dns_rdataset_current(&rdataset, &private);
		if (!dns_nsec3param_fromprivate(&private, &rdata,
						buf, sizeof(buf)))
			continue;
		CHECK(dns_rdata_tostruct(&rdata, &nsec3param, NULL));

		if ((!nsec3ok &&
		     (nsec3param.flags & DNS_NSEC3FLAG_INITIAL) != 0) ||
		    nsec3param.hash != chain->nsec3param.hash ||
		    nsec3param.iterations != chain->nsec3param.iterations ||
		    nsec3param.salt_length != chain->nsec3param.salt_length ||
		    memcmp(nsec3param.salt, chain->nsec3param.salt,
			   nsec3param.salt_length)) {
			dns_rdata_reset(&rdata);
			continue;
		}

		CHECK(update_one_rr(db, ver, diff, DNS_DIFFOP_DEL,
				    name, rdataset.ttl, &private));
		dns_rdata_reset(&rdata);
	}
	if (result != ISC_R_NOMORE)
		goto failure;

  add:
	if ((chain->nsec3param.flags & DNS_NSEC3FLAG_REMOVE) != 0) {
		result = ISC_R_SUCCESS;
		goto failure;
	}

	/*
	 * Add a NSEC3PARAM record which matches that in nsec3chain but
	 * with all flags bits cleared.
	 *
	 * Note: we do not clear chain->nsec3param.flags as this change
	 * may be reversed.
	 */
	isc_buffer_init(&buffer, &parambuf, sizeof(parambuf));
	CHECK(dns_rdata_fromstruct(&rdata, dns_db_class(db),
				   dns_rdatatype_nsec3param,
				   &chain->nsec3param, &buffer));
	rdata.data[1] = 0;	/* Clear flag bits. */
	CHECK(update_one_rr(db, ver, diff, DNS_DIFFOP_ADD, name, ttl, &rdata));

  failure:
	dns_db_detachnode(db, &node);
	if (dns_rdataset_isassociated(&rdataset))
		dns_rdataset_disassociate(&rdataset);
	return (result);
}

static isc_result_t
delete_nsec(dns_db_t *db, dns_dbversion_t *ver, dns_dbnode_t *node,
	    dns_name_t *name, dns_diff_t *diff)
{
	dns_rdataset_t rdataset;
	isc_result_t result;

	dns_rdataset_init(&rdataset);

	result = dns_db_findrdataset(db, node, ver, dns_rdatatype_nsec,
				     0, 0, &rdataset, NULL);
	if (result == ISC_R_NOTFOUND)
		return (ISC_R_SUCCESS);
	if (result != ISC_R_SUCCESS)
		return (result);
	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset)) {
		dns_rdata_t rdata = DNS_RDATA_INIT;

		dns_rdataset_current(&rdataset, &rdata);
		CHECK(update_one_rr(db, ver, diff, DNS_DIFFOP_DEL, name,
				    rdataset.ttl, &rdata));
	}
	if (result == ISC_R_NOMORE)
		result = ISC_R_SUCCESS;
 failure:
	dns_rdataset_disassociate(&rdataset);
	return (result);
}

static isc_result_t
deletematchingnsec3(dns_db_t *db, dns_dbversion_t *ver, dns_dbnode_t *node,
		    dns_name_t *name, const dns_rdata_nsec3param_t *param,
		    dns_diff_t *diff)
{
	dns_rdataset_t rdataset;
	dns_rdata_nsec3_t nsec3;
	isc_result_t result;

	dns_rdataset_init(&rdataset);
	result = dns_db_findrdataset(db, node, ver, dns_rdatatype_nsec3,
				     0, 0, &rdataset, NULL);
	if (result == ISC_R_NOTFOUND)
		return (ISC_R_SUCCESS);
	if (result != ISC_R_SUCCESS)
		return (result);

	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset)) {
		dns_rdata_t rdata = DNS_RDATA_INIT;

		dns_rdataset_current(&rdataset, &rdata);
		CHECK(dns_rdata_tostruct(&rdata, &nsec3, NULL));
		if (nsec3.hash != param->hash ||
		    nsec3.iterations != param->iterations ||
		    nsec3.salt_length != param->salt_length ||
		    memcmp(nsec3.salt, param->salt, nsec3.salt_length))
			continue;
		CHECK(update_one_rr(db, ver, diff, DNS_DIFFOP_DEL, name,
				    rdataset.ttl, &rdata));
	}
	if (result == ISC_R_NOMORE)
		result = ISC_R_SUCCESS;
 failure:
	dns_rdataset_disassociate(&rdataset);
	return (result);
}

static isc_result_t
need_nsec_chain(dns_db_t *db, dns_dbversion_t *ver,
		const dns_rdata_nsec3param_t *param,
		isc_boolean_t *answer)
{
	dns_dbnode_t *node = NULL;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdata_nsec3param_t myparam;
	dns_rdataset_t rdataset;
	isc_result_t result;

	*answer = ISC_FALSE;

	result = dns_db_getoriginnode(db, &node);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);

	dns_rdataset_init(&rdataset);

	result = dns_db_findrdataset(db, node, ver, dns_rdatatype_nsec,
				     0, 0, &rdataset, NULL);
	if (result == ISC_R_SUCCESS) {
		dns_rdataset_disassociate(&rdataset);
		dns_db_detachnode(db, &node);
		return (result);
	}
	if (result != ISC_R_NOTFOUND) {
		dns_db_detachnode(db, &node);
		return (result);
	}

	result = dns_db_findrdataset(db, node, ver, dns_rdatatype_nsec3param,
				     0, 0, &rdataset, NULL);
	if (result == ISC_R_NOTFOUND) {
		*answer = ISC_TRUE;
		dns_db_detachnode(db, &node);
		return (ISC_R_SUCCESS);
	}
	if (result != ISC_R_SUCCESS) {
		dns_db_detachnode(db, &node);
		return (result);
	}

	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset)) {
		dns_rdataset_current(&rdataset, &rdata);
		CHECK(dns_rdata_tostruct(&rdata, &myparam, NULL));
		dns_rdata_reset(&rdata);
		/*
		 * Ignore any NSEC3PARAM removals.
		 */
		if (NSEC3REMOVE(myparam.flags))
			continue;
		/*
		 * Ignore the chain that we are in the process of deleting.
		 */
		if (myparam.hash == param->hash &&
		    myparam.iterations == param->iterations &&
		    myparam.salt_length == param->salt_length &&
		    !memcmp(myparam.salt, param->salt, myparam.salt_length))
			continue;
		/*
		 * Found an active NSEC3 chain.
		 */
		break;
	}
	if (result == ISC_R_NOMORE) {
		*answer = ISC_TRUE;
		result = ISC_R_SUCCESS;
	}

 failure:
	if (dns_rdataset_isassociated(&rdataset))
		dns_rdataset_disassociate(&rdataset);
	dns_db_detachnode(db, &node);
	return (result);
}

static isc_result_t
update_sigs(dns_diff_t *diff, dns_db_t *db, dns_dbversion_t *version,
	    dst_key_t *zone_keys[], unsigned int nkeys, dns_zone_t *zone,
	    isc_stdtime_t inception, isc_stdtime_t expire, isc_stdtime_t now,
	    isc_boolean_t check_ksk, isc_boolean_t keyset_kskonly,
	    zonediff_t *zonediff)
{
	dns_difftuple_t *tuple;
	isc_result_t result;

	for (tuple = ISC_LIST_HEAD(diff->tuples);
	     tuple != NULL;
	     tuple = ISC_LIST_HEAD(diff->tuples)) {
		result = del_sigs(zone, db, version, &tuple->name,
				  tuple->rdata.type, zonediff,
				  zone_keys, nkeys, now, ISC_FALSE);
		if (result != ISC_R_SUCCESS) {
			dns_zone_log(zone, ISC_LOG_ERROR,
				     "update_sigs:del_sigs -> %s",
				     dns_result_totext(result));
			return (result);
		}
		result = add_sigs(db, version, &tuple->name,
				  tuple->rdata.type, zonediff->diff,
				  zone_keys, nkeys, zone->mctx, inception,
				  expire, check_ksk, keyset_kskonly);
		if (result != ISC_R_SUCCESS) {
			dns_zone_log(zone, ISC_LOG_ERROR,
				     "update_sigs:add_sigs -> %s",
				     dns_result_totext(result));
			return (result);
		}

		do {
			dns_difftuple_t *next = ISC_LIST_NEXT(tuple, link);
			while (next != NULL &&
			       (tuple->rdata.type != next->rdata.type ||
				!dns_name_equal(&tuple->name, &next->name)))
				next = ISC_LIST_NEXT(next, link);
			ISC_LIST_UNLINK(diff->tuples, tuple, link);
			dns_diff_appendminimal(zonediff->diff, &tuple);
			INSIST(tuple == NULL);
			tuple = next;
		} while (tuple != NULL);
	}
	return (ISC_R_SUCCESS);
}

/*
 * Incrementally build and sign a new NSEC3 chain using the parameters
 * requested.
 */
static void
zone_nsec3chain(dns_zone_t *zone) {
	const char *me = "zone_nsec3chain";
	dns_db_t *db = NULL;
	dns_dbnode_t *node = NULL;
	dns_dbversion_t *version = NULL;
	dns_diff_t _sig_diff;
	dns_diff_t nsec_diff;
	dns_diff_t nsec3_diff;
	dns_diff_t param_diff;
	zonediff_t zonediff;
	dns_fixedname_t fixed;
	dns_fixedname_t nextfixed;
	dns_name_t *name, *nextname;
	dns_rdataset_t rdataset;
	dns_nsec3chain_t *nsec3chain = NULL, *nextnsec3chain;
	dns_nsec3chainlist_t cleanup;
	dst_key_t *zone_keys[DNS_MAXZONEKEYS];
	isc_int32_t signatures;
	isc_boolean_t check_ksk, keyset_kskonly;
	isc_boolean_t delegation;
	isc_boolean_t first;
	isc_result_t result;
	isc_stdtime_t now, inception, soaexpire, expire;
	isc_uint32_t jitter;
	unsigned int i;
	unsigned int nkeys = 0;
	isc_uint32_t nodes;
	isc_boolean_t unsecure = ISC_FALSE;
	isc_boolean_t seen_soa, seen_ns, seen_dname, seen_ds;
	isc_boolean_t seen_nsec, seen_nsec3, seen_rr;
	dns_rdatasetiter_t *iterator = NULL;
	isc_boolean_t buildnsecchain;
	isc_boolean_t updatensec = ISC_FALSE;
	dns_rdatatype_t privatetype = zone->privatetype;

	ENTER;

	dns_rdataset_init(&rdataset);
	dns_fixedname_init(&fixed);
	name = dns_fixedname_name(&fixed);
	dns_fixedname_init(&nextfixed);
	nextname = dns_fixedname_name(&nextfixed);
	dns_diff_init(zone->mctx, &param_diff);
	dns_diff_init(zone->mctx, &nsec3_diff);
	dns_diff_init(zone->mctx, &nsec_diff);
	dns_diff_init(zone->mctx, &_sig_diff);
	zonediff_init(&zonediff, &_sig_diff);
	ISC_LIST_INIT(cleanup);

	/*
	 * Updates are disabled.  Pause for 5 minutes.
	 */
	if (zone->update_disabled) {
		result = ISC_R_FAILURE;
		goto failure;
	}

	ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_read);
	dns_db_attach(zone->db, &db);
	ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);

	result = dns_db_newversion(db, &version);
	if (result != ISC_R_SUCCESS) {
		dns_zone_log(zone, ISC_LOG_ERROR,
			     "zone_nsec3chain:dns_db_newversion -> %s",
			     dns_result_totext(result));
		goto failure;
	}

	result = find_zone_keys(zone, db, version, zone->mctx,
				DNS_MAXZONEKEYS, zone_keys, &nkeys);
	if (result != ISC_R_SUCCESS) {
		dns_zone_log(zone, ISC_LOG_ERROR,
			     "zone_nsec3chain:find_zone_keys -> %s",
			     dns_result_totext(result));
		goto failure;
	}

	isc_stdtime_get(&now);
	inception = now - 3600;	/* Allow for clock skew. */
	soaexpire = now + dns_zone_getsigvalidityinterval(zone);

	/*
	 * Spread out signatures over time if they happen to be
	 * clumped.  We don't do this for each add_sigs() call as
	 * we still want some clustering to occur.
	 */
	isc_random_get(&jitter);
	expire = soaexpire - jitter % 3600;

	check_ksk = DNS_ZONE_OPTION(zone, DNS_ZONEOPT_UPDATECHECKKSK);
	keyset_kskonly = DNS_ZONE_OPTION(zone, DNS_ZONEOPT_DNSKEYKSKONLY);

	/*
	 * We keep pulling nodes off each iterator in turn until
	 * we have no more nodes to pull off or we reach the limits
	 * for this quantum.
	 */
	nodes = zone->nodes;
	signatures = zone->signatures;
	LOCK_ZONE(zone);
	nsec3chain = ISC_LIST_HEAD(zone->nsec3chain);
	UNLOCK_ZONE(zone);
	first = ISC_TRUE;

	if (nsec3chain != NULL)
		nsec3chain->save_delete_nsec = nsec3chain->delete_nsec;
	/*
	 * Generate new NSEC3 chains first.
	 */
	while (nsec3chain != NULL && nodes-- > 0 && signatures > 0) {
		LOCK_ZONE(zone);
		nextnsec3chain = ISC_LIST_NEXT(nsec3chain, link);

		ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_read);
		if (nsec3chain->done || nsec3chain->db != zone->db) {
			ISC_LIST_UNLINK(zone->nsec3chain, nsec3chain, link);
			ISC_LIST_APPEND(cleanup, nsec3chain, link);
		}
		ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);
		UNLOCK_ZONE(zone);
		if (ISC_LIST_TAIL(cleanup) == nsec3chain)
			goto next_addchain;

		/*
		 * Possible future db.
		 */
		if (nsec3chain->db != db) {
			goto next_addchain;
		}

		if (NSEC3REMOVE(nsec3chain->nsec3param.flags))
			goto next_addchain;

		dns_dbiterator_current(nsec3chain->dbiterator, &node, name);

		if (nsec3chain->delete_nsec) {
			delegation = ISC_FALSE;
			dns_dbiterator_pause(nsec3chain->dbiterator);
			CHECK(delete_nsec(db, version, node, name, &nsec_diff));
			goto next_addnode;
		}
		/*
		 * On the first pass we need to check if the current node
		 * has not been obscured.
		 */
		delegation = ISC_FALSE;
		unsecure = ISC_FALSE;
		if (first) {
			dns_fixedname_t ffound;
			dns_name_t *found;
			dns_fixedname_init(&ffound);
			found = dns_fixedname_name(&ffound);
			result = dns_db_find(db, name, version,
					     dns_rdatatype_soa,
					     DNS_DBFIND_NOWILD, 0, NULL, found,
					     NULL, NULL);
			if ((result == DNS_R_DELEGATION ||
			    result == DNS_R_DNAME) &&
			    !dns_name_equal(name, found)) {
				/*
				 * Remember the obscuring name so that
				 * we skip all obscured names.
				 */
				dns_name_copy(found, name, NULL);
				delegation = ISC_TRUE;
				goto next_addnode;
			}
		}

		/*
		 * Check to see if this is a bottom of zone node.
		 */
		result = dns_db_allrdatasets(db, node, version, 0, &iterator);
		if (result == ISC_R_NOTFOUND)	/* Empty node? */
			goto next_addnode;
		if (result != ISC_R_SUCCESS)
			goto failure;

		seen_soa = seen_ns = seen_dname = seen_ds = seen_nsec =
			ISC_FALSE;
		for (result = dns_rdatasetiter_first(iterator);
		     result == ISC_R_SUCCESS;
		     result = dns_rdatasetiter_next(iterator)) {
			dns_rdatasetiter_current(iterator, &rdataset);
			INSIST(rdataset.type != dns_rdatatype_nsec3);
			if (rdataset.type == dns_rdatatype_soa)
				seen_soa = ISC_TRUE;
			else if (rdataset.type == dns_rdatatype_ns)
				seen_ns = ISC_TRUE;
			else if (rdataset.type == dns_rdatatype_dname)
				seen_dname = ISC_TRUE;
			else if (rdataset.type == dns_rdatatype_ds)
				seen_ds = ISC_TRUE;
			else if (rdataset.type == dns_rdatatype_nsec)
				seen_nsec = ISC_TRUE;
			dns_rdataset_disassociate(&rdataset);
		}
		dns_rdatasetiter_destroy(&iterator);
		/*
		 * Is there a NSEC chain than needs to be cleaned up?
		 */
		if (seen_nsec)
			nsec3chain->seen_nsec = ISC_TRUE;
		if (seen_ns && !seen_soa && !seen_ds)
			unsecure = ISC_TRUE;
		if ((seen_ns && !seen_soa) || seen_dname)
			delegation = ISC_TRUE;

		/*
		 * Process one node.
		 */
		dns_dbiterator_pause(nsec3chain->dbiterator);
		result = dns_nsec3_addnsec3(db, version, name,
					    &nsec3chain->nsec3param,
					    zone->minimum, unsecure,
					    &nsec3_diff);
		if (result != ISC_R_SUCCESS) {
			dns_zone_log(zone, ISC_LOG_ERROR, "zone_nsec3chain:"
				     "dns_nsec3_addnsec3 -> %s",
				     dns_result_totext(result));
			goto failure;
		}

		/*
		 * Treat each call to dns_nsec3_addnsec3() as if it's cost is
		 * two signatures.  Additionally there will, in general, be
		 * two signature generated below.
		 *
		 * If we are only changing the optout flag the cost is half
		 * that of the cost of generating a completely new chain.
		 */
		signatures -= 4;

		/*
		 * Go onto next node.
		 */
 next_addnode:
		first = ISC_FALSE;
		dns_db_detachnode(db, &node);
		do {
			result = dns_dbiterator_next(nsec3chain->dbiterator);

			if (result == ISC_R_NOMORE && nsec3chain->delete_nsec) {
				dns_dbiterator_pause(nsec3chain->dbiterator);
				CHECK(fixup_nsec3param(db, version, nsec3chain,
						       ISC_FALSE, privatetype,
						       &param_diff));
				LOCK_ZONE(zone);
				ISC_LIST_UNLINK(zone->nsec3chain, nsec3chain,
						link);
				UNLOCK_ZONE(zone);
				ISC_LIST_APPEND(cleanup, nsec3chain, link);
				goto next_addchain;
			}
			if (result == ISC_R_NOMORE) {
				dns_dbiterator_pause(nsec3chain->dbiterator);
				if (nsec3chain->seen_nsec) {
					CHECK(fixup_nsec3param(db, version,
							       nsec3chain,
							       ISC_TRUE,
							       privatetype,
							       &param_diff));
					nsec3chain->delete_nsec = ISC_TRUE;
					goto same_addchain;
				}
				CHECK(fixup_nsec3param(db, version, nsec3chain,
						       ISC_FALSE, privatetype,
						       &param_diff));
				LOCK_ZONE(zone);
				ISC_LIST_UNLINK(zone->nsec3chain, nsec3chain,
						link);
				UNLOCK_ZONE(zone);
				ISC_LIST_APPEND(cleanup, nsec3chain, link);
				goto next_addchain;
			} else if (result != ISC_R_SUCCESS) {
				dns_zone_log(zone, ISC_LOG_ERROR,
					     "zone_nsec3chain:"
					     "dns_dbiterator_next -> %s",
					     dns_result_totext(result));
				goto failure;
			} else if (delegation) {
				dns_dbiterator_current(nsec3chain->dbiterator,
						       &node, nextname);
				dns_db_detachnode(db, &node);
				if (!dns_name_issubdomain(nextname, name))
					break;
			} else
				break;
		} while (1);
		continue;

 same_addchain:
		CHECK(dns_dbiterator_first(nsec3chain->dbiterator));
		first = ISC_TRUE;
		continue;

 next_addchain:
		dns_dbiterator_pause(nsec3chain->dbiterator);
		nsec3chain = nextnsec3chain;
		first = ISC_TRUE;
		if (nsec3chain != NULL)
			nsec3chain->save_delete_nsec = nsec3chain->delete_nsec;
	}

	/*
	 * Process removals.
	 */
	LOCK_ZONE(zone);
	nsec3chain = ISC_LIST_HEAD(zone->nsec3chain);
	UNLOCK_ZONE(zone);
	first = ISC_TRUE;
	buildnsecchain = ISC_FALSE;
	while (nsec3chain != NULL && nodes-- > 0 && signatures > 0) {
		LOCK_ZONE(zone);
		nextnsec3chain = ISC_LIST_NEXT(nsec3chain, link);
		UNLOCK_ZONE(zone);

		if (nsec3chain->db != db)
			goto next_removechain;

		if (!NSEC3REMOVE(nsec3chain->nsec3param.flags))
			goto next_removechain;

		/*
		 * Work out if we need to build a NSEC chain as a consequence
		 * of removing this NSEC3 chain.
		 */
		if (first && !updatensec &&
		    (nsec3chain->nsec3param.flags & DNS_NSEC3FLAG_NONSEC) == 0)
		{
			result = need_nsec_chain(db, version,
						 &nsec3chain->nsec3param,
						 &buildnsecchain);
			if (result != ISC_R_SUCCESS) {
				dns_zone_log(zone, ISC_LOG_ERROR,
					     "zone_nsec3chain:"
					     "need_nsec_chain -> %s",
					     dns_result_totext(result));
				goto failure;
			}
		}

		if (first)
			dns_zone_log(zone, ISC_LOG_DEBUG(3), "zone_nsec3chain:"
				     "buildnsecchain = %u\n", buildnsecchain);

		dns_dbiterator_current(nsec3chain->dbiterator, &node, name);
		delegation = ISC_FALSE;

		if (!buildnsecchain) {
			/*
			 * Delete the NSECPARAM record that matches this chain.
			 */
			if (first) {
				result = fixup_nsec3param(db, version,
							  nsec3chain,
							  ISC_TRUE, privatetype,
							  &param_diff);
				if (result != ISC_R_SUCCESS) {
					dns_zone_log(zone, ISC_LOG_ERROR,
						     "zone_nsec3chain:"
						     "fixup_nsec3param -> %s",
						     dns_result_totext(result));
					goto failure;
				}
			}

			/*
			 *  Delete the NSEC3 records.
			 */
			result = deletematchingnsec3(db, version, node, name,
						     &nsec3chain->nsec3param,
						     &nsec3_diff);
			if (result != ISC_R_SUCCESS) {
				dns_zone_log(zone, ISC_LOG_ERROR,
					     "zone_nsec3chain:"
					     "deletematchingnsec3 -> %s",
					     dns_result_totext(result));
				goto failure;
			}
			goto next_removenode;
		}

		if (first) {
			dns_fixedname_t ffound;
			dns_name_t *found;
			dns_fixedname_init(&ffound);
			found = dns_fixedname_name(&ffound);
			result = dns_db_find(db, name, version,
					     dns_rdatatype_soa,
					     DNS_DBFIND_NOWILD, 0, NULL, found,
					     NULL, NULL);
			if ((result == DNS_R_DELEGATION ||
			     result == DNS_R_DNAME) &&
			    !dns_name_equal(name, found)) {
				/*
				 * Remember the obscuring name so that
				 * we skip all obscured names.
				 */
				dns_name_copy(found, name, NULL);
				delegation = ISC_TRUE;
				goto next_removenode;
			}
		}

		/*
		 * Check to see if this is a bottom of zone node.
		 */
		result = dns_db_allrdatasets(db, node, version, 0, &iterator);
		if (result == ISC_R_NOTFOUND)	/* Empty node? */
			goto next_removenode;
		if (result != ISC_R_SUCCESS)
			goto failure;

		seen_soa = seen_ns = seen_dname = seen_nsec3 = seen_nsec =
			seen_rr = ISC_FALSE;
		for (result = dns_rdatasetiter_first(iterator);
		     result == ISC_R_SUCCESS;
		     result = dns_rdatasetiter_next(iterator)) {
			dns_rdatasetiter_current(iterator, &rdataset);
			if (rdataset.type == dns_rdatatype_soa)
				seen_soa = ISC_TRUE;
			else if (rdataset.type == dns_rdatatype_ns)
				seen_ns = ISC_TRUE;
			else if (rdataset.type == dns_rdatatype_dname)
				seen_dname = ISC_TRUE;
			else if (rdataset.type == dns_rdatatype_nsec)
				seen_nsec = ISC_TRUE;
			else if (rdataset.type == dns_rdatatype_nsec3)
				seen_nsec3 = ISC_TRUE;
			if (rdataset.type != dns_rdatatype_rrsig)
				seen_rr = ISC_TRUE;
			dns_rdataset_disassociate(&rdataset);
		}
		dns_rdatasetiter_destroy(&iterator);

		if (!seen_rr || seen_nsec3 || seen_nsec)
			goto next_removenode;
		if ((seen_ns && !seen_soa) || seen_dname)
			delegation = ISC_TRUE;

		/*
		 * Add a NSEC record except at the origin.
		 */
		if (!dns_name_equal(name, dns_db_origin(db))) {
			dns_dbiterator_pause(nsec3chain->dbiterator);
			CHECK(add_nsec(db, version, name, node, zone->minimum,
				       delegation, &nsec_diff));
		}

 next_removenode:
		first = ISC_FALSE;
		dns_db_detachnode(db, &node);
		do {
			result = dns_dbiterator_next(nsec3chain->dbiterator);
			if (result == ISC_R_NOMORE && buildnsecchain) {
				/*
				 * The NSEC chain should now be built.
				 * We can now remove the NSEC3 chain.
				 */
				updatensec = ISC_TRUE;
				goto same_removechain;
			}
			if (result == ISC_R_NOMORE) {
				LOCK_ZONE(zone);
				ISC_LIST_UNLINK(zone->nsec3chain, nsec3chain,
						link);
				UNLOCK_ZONE(zone);
				ISC_LIST_APPEND(cleanup, nsec3chain, link);
				dns_dbiterator_pause(nsec3chain->dbiterator);
				result = fixup_nsec3param(db, version,
							  nsec3chain, ISC_FALSE,
							  privatetype,
							  &param_diff);
				if (result != ISC_R_SUCCESS) {
					dns_zone_log(zone, ISC_LOG_ERROR,
						     "zone_nsec3chain:"
						     "fixup_nsec3param -> %s",
						     dns_result_totext(result));
					goto failure;
				}
				goto next_removechain;
			} else if (result != ISC_R_SUCCESS) {
				dns_zone_log(zone, ISC_LOG_ERROR,
					     "zone_nsec3chain:"
					     "dns_dbiterator_next -> %s",
					     dns_result_totext(result));
				goto failure;
			} else if (delegation) {
				dns_dbiterator_current(nsec3chain->dbiterator,
						       &node, nextname);
				dns_db_detachnode(db, &node);
				if (!dns_name_issubdomain(nextname, name))
					break;
			} else
				break;
		} while (1);
		continue;

 same_removechain:
		CHECK(dns_dbiterator_first(nsec3chain->dbiterator));
		buildnsecchain = ISC_FALSE;
		first = ISC_TRUE;
		continue;

 next_removechain:
		dns_dbiterator_pause(nsec3chain->dbiterator);
		nsec3chain = nextnsec3chain;
		first = ISC_TRUE;
	}

	/*
	 * We may need to update the NSEC/NSEC3 records for the zone apex.
	 */
	if (!ISC_LIST_EMPTY(param_diff.tuples)) {
		isc_boolean_t rebuild_nsec = ISC_FALSE,
			      rebuild_nsec3 = ISC_FALSE;
		result = dns_db_getoriginnode(db, &node);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);
		result = dns_db_allrdatasets(db, node, version, 0, &iterator);
		if (result != ISC_R_SUCCESS) {
			dns_zone_log(zone, ISC_LOG_ERROR, "zone_nsec3chain:"
				     "dns_db_allrdatasets -> %s",
				     dns_result_totext(result));
			goto failure;
		}
		for (result = dns_rdatasetiter_first(iterator);
		     result == ISC_R_SUCCESS;
		     result = dns_rdatasetiter_next(iterator)) {
			dns_rdatasetiter_current(iterator, &rdataset);
			if (rdataset.type == dns_rdatatype_nsec)
				rebuild_nsec = ISC_TRUE;
			if (rdataset.type == dns_rdatatype_nsec3param)
				rebuild_nsec3 = ISC_TRUE;
			dns_rdataset_disassociate(&rdataset);
		}
		dns_rdatasetiter_destroy(&iterator);
		dns_db_detachnode(db, &node);

		if (rebuild_nsec) {
			if (nsec3chain != NULL)
				dns_dbiterator_pause(nsec3chain->dbiterator);

			result = updatesecure(db, version, &zone->origin,
					      zone->minimum, ISC_TRUE,
					      &nsec_diff);
			if (result != ISC_R_SUCCESS) {
				dns_zone_log(zone, ISC_LOG_ERROR,
					     "zone_nsec3chain:"
					     "updatesecure -> %s",
					     dns_result_totext(result));
				goto failure;
			}
		}

		if (rebuild_nsec3) {
			if (nsec3chain != NULL)
				dns_dbiterator_pause(nsec3chain->dbiterator);

			result = dns_nsec3_addnsec3s(db, version,
						     dns_db_origin(db),
						     zone->minimum, ISC_FALSE,
						     &nsec3_diff);
			if (result != ISC_R_SUCCESS) {
				dns_zone_log(zone, ISC_LOG_ERROR,
					     "zone_nsec3chain:"
					     "dns_nsec3_addnsec3s -> %s",
					     dns_result_totext(result));
				goto failure;
			}
		}
	}

	if (nsec3chain != NULL)
		dns_dbiterator_pause(nsec3chain->dbiterator);

	/*
	 * Add / update signatures for the NSEC3 records.
	 */
	if (nsec3chain != NULL)
		dns_dbiterator_pause(nsec3chain->dbiterator);
	result = update_sigs(&nsec3_diff, db, version, zone_keys,
			     nkeys, zone, inception, expire, now,
			     check_ksk, keyset_kskonly, &zonediff);
	if (result != ISC_R_SUCCESS) {
		dns_zone_log(zone, ISC_LOG_ERROR, "zone_nsec3chain:"
			     "update_sigs -> %s", dns_result_totext(result));
		goto failure;
	}

	/*
	 * We have changed the NSEC3PARAM or private RRsets
	 * above so we need to update the signatures.
	 */
	result = update_sigs(&param_diff, db, version, zone_keys,
			     nkeys, zone, inception, expire, now,
			     check_ksk, keyset_kskonly, &zonediff);
	if (result != ISC_R_SUCCESS) {
		dns_zone_log(zone, ISC_LOG_ERROR, "zone_nsec3chain:"
			     "update_sigs -> %s", dns_result_totext(result));
		goto failure;
	}

	if (updatensec) {
		result = updatesecure(db, version, &zone->origin,
				      zone->minimum, ISC_FALSE, &nsec_diff);
		if (result != ISC_R_SUCCESS) {
			dns_zone_log(zone, ISC_LOG_ERROR, "zone_nsec3chain:"
				     "updatesecure -> %s",
				     dns_result_totext(result));
			goto failure;
		}
	}

	result = update_sigs(&nsec_diff, db, version, zone_keys,
			     nkeys, zone, inception, expire, now,
			     check_ksk, keyset_kskonly, &zonediff);
	if (result != ISC_R_SUCCESS) {
		dns_zone_log(zone, ISC_LOG_ERROR, "zone_nsec3chain:"
			     "update_sigs -> %s", dns_result_totext(result));
		goto failure;
	}

	/*
	 * If we made no effective changes to the zone then we can just
	 * cleanup otherwise we need to increment the serial.
	 */
	if (ISC_LIST_EMPTY(zonediff.diff->tuples)) {
		/*
		 * No need to call dns_db_closeversion() here as it is
		 * called with commit = ISC_TRUE below.
		 */
		goto done;
	}

	result = del_sigs(zone, db, version, &zone->origin, dns_rdatatype_soa,
			  &zonediff, zone_keys, nkeys, now, ISC_FALSE);
	if (result != ISC_R_SUCCESS) {
		dns_zone_log(zone, ISC_LOG_ERROR, "zone_nsec3chain:"
			     "del_sigs -> %s", dns_result_totext(result));
		goto failure;
	}

	result = update_soa_serial(db, version, zonediff.diff, zone->mctx,
				   zone->updatemethod);
	if (result != ISC_R_SUCCESS) {
		dns_zone_log(zone, ISC_LOG_ERROR, "zone_nsec3chain:"
			     "update_soa_serial -> %s",
			     dns_result_totext(result));
		goto failure;
	}

	result = add_sigs(db, version, &zone->origin, dns_rdatatype_soa,
			  zonediff.diff, zone_keys, nkeys, zone->mctx,
			  inception, soaexpire, check_ksk, keyset_kskonly);
	if (result != ISC_R_SUCCESS) {
		dns_zone_log(zone, ISC_LOG_ERROR, "zone_nsec3chain:"
			     "add_sigs -> %s", dns_result_totext(result));
		goto failure;
	}

	/* Write changes to journal file. */
	CHECK(zone_journal(zone, zonediff.diff, NULL, "zone_nsec3chain"));

	LOCK_ZONE(zone);
	zone_needdump(zone, DNS_DUMP_DELAY);
	DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_NEEDNOTIFY);
	UNLOCK_ZONE(zone);

 done:
	/*
	 * Pause all iterators so that dns_db_closeversion() can succeed.
	 */
	LOCK_ZONE(zone);
	for (nsec3chain = ISC_LIST_HEAD(zone->nsec3chain);
	     nsec3chain != NULL;
	     nsec3chain = ISC_LIST_NEXT(nsec3chain, link))
		dns_dbiterator_pause(nsec3chain->dbiterator);
	UNLOCK_ZONE(zone);

	/*
	 * Everything has succeeded. Commit the changes.
	 * Unconditionally commit as zonediff.offline not checked above.
	 */
	dns_db_closeversion(db, &version, ISC_TRUE);

	/*
	 * Everything succeeded so we can clean these up now.
	 */
	nsec3chain = ISC_LIST_HEAD(cleanup);
	while (nsec3chain != NULL) {
		ISC_LIST_UNLINK(cleanup, nsec3chain, link);
		dns_db_detach(&nsec3chain->db);
		dns_dbiterator_destroy(&nsec3chain->dbiterator);
		isc_mem_put(zone->mctx, nsec3chain, sizeof *nsec3chain);
		nsec3chain = ISC_LIST_HEAD(cleanup);
	}

	set_resigntime(zone);

 failure:
	if (result != ISC_R_SUCCESS)
		dns_zone_log(zone, ISC_LOG_ERROR, "zone_nsec3chain: %s",
			     dns_result_totext(result));
	/*
	 * On error roll back the current nsec3chain.
	 */
	if (result != ISC_R_SUCCESS && nsec3chain != NULL) {
		if (nsec3chain->done) {
			dns_db_detach(&nsec3chain->db);
			dns_dbiterator_destroy(&nsec3chain->dbiterator);
			isc_mem_put(zone->mctx, nsec3chain, sizeof *nsec3chain);
		} else {
			result = dns_dbiterator_first(nsec3chain->dbiterator);
			RUNTIME_CHECK(result == ISC_R_SUCCESS);
			dns_dbiterator_pause(nsec3chain->dbiterator);
			nsec3chain->delete_nsec = nsec3chain->save_delete_nsec;
		}
	}

	/*
	 * Rollback the cleanup list.
	 */
	nsec3chain = ISC_LIST_TAIL(cleanup);
	while (nsec3chain != NULL) {
		ISC_LIST_UNLINK(cleanup, nsec3chain, link);
		if (nsec3chain->done) {
			dns_db_detach(&nsec3chain->db);
			dns_dbiterator_destroy(&nsec3chain->dbiterator);
			isc_mem_put(zone->mctx, nsec3chain, sizeof *nsec3chain);
		} else {
			LOCK_ZONE(zone);
			ISC_LIST_PREPEND(zone->nsec3chain, nsec3chain, link);
			UNLOCK_ZONE(zone);
			result = dns_dbiterator_first(nsec3chain->dbiterator);
			RUNTIME_CHECK(result == ISC_R_SUCCESS);
			dns_dbiterator_pause(nsec3chain->dbiterator);
			nsec3chain->delete_nsec = nsec3chain->save_delete_nsec;
		}
		nsec3chain = ISC_LIST_TAIL(cleanup);
	}

	LOCK_ZONE(zone);
	for (nsec3chain = ISC_LIST_HEAD(zone->nsec3chain);
	     nsec3chain != NULL;
	     nsec3chain = ISC_LIST_NEXT(nsec3chain, link))
		dns_dbiterator_pause(nsec3chain->dbiterator);
	UNLOCK_ZONE(zone);

	dns_diff_clear(&param_diff);
	dns_diff_clear(&nsec3_diff);
	dns_diff_clear(&nsec_diff);
	dns_diff_clear(&_sig_diff);

	if (iterator != NULL)
		dns_rdatasetiter_destroy(&iterator);

	for (i = 0; i < nkeys; i++)
		dst_key_free(&zone_keys[i]);

	if (node != NULL)
		dns_db_detachnode(db, &node);
	if (version != NULL) {
		dns_db_closeversion(db, &version, ISC_FALSE);
		dns_db_detach(&db);
	} else if (db != NULL)
		dns_db_detach(&db);

	LOCK_ZONE(zone);
	if (ISC_LIST_HEAD(zone->nsec3chain) != NULL) {
		isc_interval_t interval;
		if (zone->update_disabled || result != ISC_R_SUCCESS)
			isc_interval_set(&interval, 60, 0);	  /* 1 minute */
		else
			isc_interval_set(&interval, 0, 10000000); /* 10 ms */
		isc_time_nowplusinterval(&zone->nsec3chaintime, &interval);
	} else
		isc_time_settoepoch(&zone->nsec3chaintime);
	UNLOCK_ZONE(zone);

	INSIST(version == NULL);
}

static isc_result_t
del_sig(dns_db_t *db, dns_dbversion_t *version, dns_name_t *name,
	dns_dbnode_t *node, unsigned int nkeys, dns_secalg_t algorithm,
	isc_uint16_t keyid, dns_diff_t *diff)
{
	dns_rdata_rrsig_t rrsig;
	dns_rdataset_t rdataset;
	dns_rdatasetiter_t *iterator = NULL;
	isc_result_t result;

	result = dns_db_allrdatasets(db, node, version, 0, &iterator);
	if (result != ISC_R_SUCCESS) {
		if (result == ISC_R_NOTFOUND)
			result = ISC_R_SUCCESS;
		return (result);
	}

	dns_rdataset_init(&rdataset);
	for (result = dns_rdatasetiter_first(iterator);
	     result == ISC_R_SUCCESS;
	     result = dns_rdatasetiter_next(iterator)) {
		dns_rdatasetiter_current(iterator, &rdataset);
		if (nkeys == 0 && rdataset.type == dns_rdatatype_nsec) {
			for (result = dns_rdataset_first(&rdataset);
			     result == ISC_R_SUCCESS;
			     result = dns_rdataset_next(&rdataset)) {
				dns_rdata_t rdata = DNS_RDATA_INIT;
				dns_rdataset_current(&rdataset, &rdata);
				CHECK(update_one_rr(db, version, diff,
						    DNS_DIFFOP_DEL, name,
						    rdataset.ttl, &rdata));
			}
			if (result != ISC_R_NOMORE)
				goto failure;
			dns_rdataset_disassociate(&rdataset);
			continue;
		}
		if (rdataset.type != dns_rdatatype_rrsig) {
			dns_rdataset_disassociate(&rdataset);
			continue;
		}
		for (result = dns_rdataset_first(&rdataset);
		     result == ISC_R_SUCCESS;
		     result = dns_rdataset_next(&rdataset)) {
			dns_rdata_t rdata = DNS_RDATA_INIT;
			dns_rdataset_current(&rdataset, &rdata);
			CHECK(dns_rdata_tostruct(&rdata, &rrsig, NULL));
			if (rrsig.algorithm != algorithm ||
			    rrsig.keyid != keyid)
				continue;
			CHECK(update_one_rr(db, version, diff,
					    DNS_DIFFOP_DELRESIGN, name,
					    rdataset.ttl, &rdata));
		}
		dns_rdataset_disassociate(&rdataset);
		if (result != ISC_R_NOMORE)
			break;
	}
	if (result == ISC_R_NOMORE)
		result = ISC_R_SUCCESS;
 failure:
	if (dns_rdataset_isassociated(&rdataset))
		dns_rdataset_disassociate(&rdataset);
	dns_rdatasetiter_destroy(&iterator);
	return (result);
}

/*
 * Incrementally sign the zone using the keys requested.
 * Builds the NSEC chain if required.
 */
static void
zone_sign(dns_zone_t *zone) {
	const char *me = "zone_sign";
	dns_db_t *db = NULL;
	dns_dbnode_t *node = NULL;
	dns_dbversion_t *version = NULL;
	dns_diff_t _sig_diff;
	dns_diff_t post_diff;
	zonediff_t zonediff;
	dns_fixedname_t fixed;
	dns_fixedname_t nextfixed;
	dns_name_t *name, *nextname;
	dns_rdataset_t rdataset;
	dns_signing_t *signing, *nextsigning;
	dns_signinglist_t cleanup;
	dst_key_t *zone_keys[DNS_MAXZONEKEYS];
	isc_int32_t signatures;
	isc_boolean_t check_ksk, keyset_kskonly, is_ksk;
	isc_boolean_t commit = ISC_FALSE;
	isc_boolean_t delegation;
	isc_boolean_t build_nsec = ISC_FALSE;
	isc_boolean_t build_nsec3 = ISC_FALSE;
	isc_boolean_t first;
	isc_result_t result;
	isc_stdtime_t now, inception, soaexpire, expire;
	isc_uint32_t jitter;
	unsigned int i, j;
	unsigned int nkeys = 0;
	isc_uint32_t nodes;

	ENTER;

	dns_rdataset_init(&rdataset);
	dns_fixedname_init(&fixed);
	name = dns_fixedname_name(&fixed);
	dns_fixedname_init(&nextfixed);
	nextname = dns_fixedname_name(&nextfixed);
	dns_diff_init(zone->mctx, &_sig_diff);
	dns_diff_init(zone->mctx, &post_diff);
	zonediff_init(&zonediff, &_sig_diff);
	ISC_LIST_INIT(cleanup);

	/*
	 * Updates are disabled.  Pause for 5 minutes.
	 */
	if (zone->update_disabled) {
		result = ISC_R_FAILURE;
		goto failure;
	}

	ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_read);
	if (zone->db != NULL)
		dns_db_attach(zone->db, &db);
	ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);
	if (db == NULL) {
		result = ISC_R_FAILURE;
		goto failure;
	}

	result = dns_db_newversion(db, &version);
	if (result != ISC_R_SUCCESS) {
		dns_zone_log(zone, ISC_LOG_ERROR,
			     "zone_sign:dns_db_newversion -> %s",
			     dns_result_totext(result));
		goto failure;
	}

	result = find_zone_keys(zone, db, version, zone->mctx,
				DNS_MAXZONEKEYS, zone_keys, &nkeys);
	if (result != ISC_R_SUCCESS) {
		dns_zone_log(zone, ISC_LOG_ERROR,
			     "zone_sign:find_zone_keys -> %s",
			     dns_result_totext(result));
		goto failure;
	}

	isc_stdtime_get(&now);
	inception = now - 3600;	/* Allow for clock skew. */
	soaexpire = now + dns_zone_getsigvalidityinterval(zone);

	/*
	 * Spread out signatures over time if they happen to be
	 * clumped.  We don't do this for each add_sigs() call as
	 * we still want some clustering to occur.
	 */
	isc_random_get(&jitter);
	expire = soaexpire - jitter % 3600;

	/*
	 * We keep pulling nodes off each iterator in turn until
	 * we have no more nodes to pull off or we reach the limits
	 * for this quantum.
	 */
	nodes = zone->nodes;
	signatures = zone->signatures;
	signing = ISC_LIST_HEAD(zone->signing);
	first = ISC_TRUE;

	check_ksk = DNS_ZONE_OPTION(zone, DNS_ZONEOPT_UPDATECHECKKSK);
	keyset_kskonly = DNS_ZONE_OPTION(zone, DNS_ZONEOPT_DNSKEYKSKONLY);

	/* Determine which type of chain to build */
	CHECK(dns_private_chains(db, version, zone->privatetype,
				 &build_nsec, &build_nsec3));

	/* If neither chain is found, default to NSEC */
	if (!build_nsec && !build_nsec3)
		build_nsec = ISC_TRUE;

	while (signing != NULL && nodes-- > 0 && signatures > 0) {
		nextsigning = ISC_LIST_NEXT(signing, link);

		ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_read);
		if (signing->done || signing->db != zone->db) {
			/*
			 * The zone has been reloaded.	We will have
			 * created new signings as part of the reload
			 * process so we can destroy this one.
			 */
			ISC_LIST_UNLINK(zone->signing, signing, link);
			ISC_LIST_APPEND(cleanup, signing, link);
			ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);
			goto next_signing;
		}
		ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);

		if (signing->db != db)
			goto next_signing;

		delegation = ISC_FALSE;

		if (first && signing->delete) {
			/*
			 * Remove the key we are deleting from consideration.
			 */
			for (i = 0, j = 0; i < nkeys; i++) {
				/*
				 * Find the key we want to remove.
				 */
				if (ALG(zone_keys[i]) == signing->algorithm &&
				    dst_key_id(zone_keys[i]) == signing->keyid)
				{
					if (KSK(zone_keys[i]))
						dst_key_free(&zone_keys[i]);
					continue;
				}
				zone_keys[j] = zone_keys[i];
				j++;
			}
			nkeys = j;
		}

		dns_dbiterator_current(signing->dbiterator, &node, name);

		if (signing->delete) {
			dns_dbiterator_pause(signing->dbiterator);
			CHECK(del_sig(db, version, name, node, nkeys,
				      signing->algorithm, signing->keyid,
				      zonediff.diff));
		}

		/*
		 * On the first pass we need to check if the current node
		 * has not been obscured.
		 */
		if (first) {
			dns_fixedname_t ffound;
			dns_name_t *found;
			dns_fixedname_init(&ffound);
			found = dns_fixedname_name(&ffound);
			result = dns_db_find(db, name, version,
					     dns_rdatatype_soa,
					     DNS_DBFIND_NOWILD, 0, NULL, found,
					     NULL, NULL);
			if ((result == DNS_R_DELEGATION ||
			    result == DNS_R_DNAME) &&
			    !dns_name_equal(name, found)) {
				/*
				 * Remember the obscuring name so that
				 * we skip all obscured names.
				 */
				dns_name_copy(found, name, NULL);
				delegation = ISC_TRUE;
				goto next_node;
			}
		}

		/*
		 * Process one node.
		 */
		dns_dbiterator_pause(signing->dbiterator);
		for (i = 0; i < nkeys; i++) {
			isc_boolean_t both = ISC_FALSE;

			/*
			 * Find the keys we want to sign with.
			 */
			if (!dst_key_isprivate(zone_keys[i]))
				continue;

			/*
			 * When adding look for the specific key.
			 */
			if (!signing->delete &&
			    (dst_key_alg(zone_keys[i]) != signing->algorithm ||
			     dst_key_id(zone_keys[i]) != signing->keyid))
				continue;

			/*
			 * When deleting make sure we are properly signed
			 * with the algorithm that was being removed.
			 */
			if (signing->delete &&
			    ALG(zone_keys[i]) != signing->algorithm)
				continue;

			/*
			 * Do we do KSK processing?
			 */
			if (check_ksk && !REVOKE(zone_keys[i])) {
				isc_boolean_t have_ksk, have_nonksk;
				if (KSK(zone_keys[i])) {
					have_ksk = ISC_TRUE;
					have_nonksk = ISC_FALSE;
				} else {
					have_ksk = ISC_FALSE;
					have_nonksk = ISC_TRUE;
				}
				for (j = 0; j < nkeys; j++) {
					if (j == i ||
					    ALG(zone_keys[i]) !=
					    ALG(zone_keys[j]))
						continue;
					if (REVOKE(zone_keys[j]))
						continue;
					if (KSK(zone_keys[j]))
						have_ksk = ISC_TRUE;
					else
						have_nonksk = ISC_TRUE;
					both = have_ksk && have_nonksk;
					if (both)
						break;
				}
			}
			if (both || REVOKE(zone_keys[i]))
				is_ksk = KSK(zone_keys[i]);
			else
				is_ksk = ISC_FALSE;

			CHECK(sign_a_node(db, name, node, version, build_nsec3,
					  build_nsec, zone_keys[i], inception,
					  expire, zone->minimum, is_ksk,
					  ISC_TF(both && keyset_kskonly),
					  &delegation, zonediff.diff,
					  &signatures, zone->mctx));
			/*
			 * If we are adding we are done.  Look for other keys
			 * of the same algorithm if deleting.
			 */
			if (!signing->delete)
				break;
		}

		/*
		 * Go onto next node.
		 */
 next_node:
		first = ISC_FALSE;
		dns_db_detachnode(db, &node);
		do {
			result = dns_dbiterator_next(signing->dbiterator);
			if (result == ISC_R_NOMORE) {
				ISC_LIST_UNLINK(zone->signing, signing, link);
				ISC_LIST_APPEND(cleanup, signing, link);
				dns_dbiterator_pause(signing->dbiterator);
				if (nkeys != 0 && build_nsec) {
					/*
					 * We have finished regenerating the
					 * zone with a zone signing key.
					 * The NSEC chain is now complete and
					 * there is a full set of signatures
					 * for the zone.  We can now clear the
					 * OPT bit from the NSEC record.
					 */
					result = updatesecure(db, version,
							      &zone->origin,
							      zone->minimum,
							      ISC_FALSE,
							      &post_diff);
					if (result != ISC_R_SUCCESS) {
						dns_zone_log(zone,
							     ISC_LOG_ERROR,
						    "updatesecure -> %s",
						    dns_result_totext(result));
						goto failure;
					}
				}
				result = updatesignwithkey(zone, signing,
							   version,
							   build_nsec3,
							   zone->minimum,
							   &post_diff);
				if (result != ISC_R_SUCCESS) {
					dns_zone_log(zone, ISC_LOG_ERROR,
						     "updatesignwithkey -> %s",
						     dns_result_totext(result));
					goto failure;
				}
				build_nsec = ISC_FALSE;
				goto next_signing;
			} else if (result != ISC_R_SUCCESS) {
				dns_zone_log(zone, ISC_LOG_ERROR,
					"zone_sign:dns_dbiterator_next -> %s",
					     dns_result_totext(result));
				goto failure;
			} else if (delegation) {
				dns_dbiterator_current(signing->dbiterator,
						       &node, nextname);
				dns_db_detachnode(db, &node);
				if (!dns_name_issubdomain(nextname, name))
					break;
			} else
				break;
		} while (1);
		continue;

 next_signing:
		dns_dbiterator_pause(signing->dbiterator);
		signing = nextsigning;
		first = ISC_TRUE;
	}

	if (ISC_LIST_HEAD(post_diff.tuples) != NULL) {
		result = update_sigs(&post_diff, db, version, zone_keys,
				     nkeys, zone, inception, expire, now,
				     check_ksk, keyset_kskonly, &zonediff);
		if (result != ISC_R_SUCCESS) {
			dns_zone_log(zone, ISC_LOG_ERROR, "zone_sign:"
				     "update_sigs -> %s",
				     dns_result_totext(result));
			goto failure;
		}
	}

	/*
	 * Have we changed anything?
	 */
	if (ISC_LIST_EMPTY(zonediff.diff->tuples)) {
		if (zonediff.offline)
			commit = ISC_TRUE;
		result = ISC_R_SUCCESS;
		goto pauseall;
	}

	commit = ISC_TRUE;

	result = del_sigs(zone, db, version, &zone->origin, dns_rdatatype_soa,
			  &zonediff, zone_keys, nkeys, now, ISC_FALSE);
	if (result != ISC_R_SUCCESS) {
		dns_zone_log(zone, ISC_LOG_ERROR,
			     "zone_sign:del_sigs -> %s",
			     dns_result_totext(result));
		goto failure;
	}

	result = update_soa_serial(db, version, zonediff.diff, zone->mctx,
				   zone->updatemethod);
	if (result != ISC_R_SUCCESS) {
		dns_zone_log(zone, ISC_LOG_ERROR,
			     "zone_sign:update_soa_serial -> %s",
			     dns_result_totext(result));
		goto failure;
	}

	/*
	 * Generate maximum life time signatures so that the above loop
	 * termination is sensible.
	 */
	result = add_sigs(db, version, &zone->origin, dns_rdatatype_soa,
			  zonediff.diff, zone_keys, nkeys, zone->mctx,
			  inception, soaexpire, check_ksk, keyset_kskonly);
	if (result != ISC_R_SUCCESS) {
		dns_zone_log(zone, ISC_LOG_ERROR,
			     "zone_sign:add_sigs -> %s",
			     dns_result_totext(result));
		goto failure;
	}

	/*
	 * Write changes to journal file.
	 */
	CHECK(zone_journal(zone, zonediff.diff, NULL, "zone_sign"));

 pauseall:
	/*
	 * Pause all iterators so that dns_db_closeversion() can succeed.
	 */
	for (signing = ISC_LIST_HEAD(zone->signing);
	     signing != NULL;
	     signing = ISC_LIST_NEXT(signing, link))
		dns_dbiterator_pause(signing->dbiterator);

	for (signing = ISC_LIST_HEAD(cleanup);
	     signing != NULL;
	     signing = ISC_LIST_NEXT(signing, link))
		dns_dbiterator_pause(signing->dbiterator);

	/*
	 * Everything has succeeded. Commit the changes.
	 */
	dns_db_closeversion(db, &version, commit);

	/*
	 * Everything succeeded so we can clean these up now.
	 */
	signing = ISC_LIST_HEAD(cleanup);
	while (signing != NULL) {
		ISC_LIST_UNLINK(cleanup, signing, link);
		dns_db_detach(&signing->db);
		dns_dbiterator_destroy(&signing->dbiterator);
		isc_mem_put(zone->mctx, signing, sizeof *signing);
		signing = ISC_LIST_HEAD(cleanup);
	}

	set_resigntime(zone);

	if (commit) {
		LOCK_ZONE(zone);
		DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_NEEDNOTIFY);
		zone_needdump(zone, DNS_DUMP_DELAY);
		UNLOCK_ZONE(zone);
	}

 failure:
	/*
	 * Rollback the cleanup list.
	 */
	signing = ISC_LIST_HEAD(cleanup);
	while (signing != NULL) {
		ISC_LIST_UNLINK(cleanup, signing, link);
		ISC_LIST_PREPEND(zone->signing, signing, link);
		dns_dbiterator_first(signing->dbiterator);
		dns_dbiterator_pause(signing->dbiterator);
		signing = ISC_LIST_HEAD(cleanup);
	}

	for (signing = ISC_LIST_HEAD(zone->signing);
	     signing != NULL;
	     signing = ISC_LIST_NEXT(signing, link))
		dns_dbiterator_pause(signing->dbiterator);

	dns_diff_clear(&_sig_diff);

	for (i = 0; i < nkeys; i++)
		dst_key_free(&zone_keys[i]);

	if (node != NULL)
		dns_db_detachnode(db, &node);

	if (version != NULL) {
		dns_db_closeversion(db, &version, ISC_FALSE);
		dns_db_detach(&db);
	} else if (db != NULL)
		dns_db_detach(&db);

	if (ISC_LIST_HEAD(zone->signing) != NULL) {
		isc_interval_t interval;
		if (zone->update_disabled || result != ISC_R_SUCCESS)
			isc_interval_set(&interval, 60, 0);	  /* 1 minute */
		else
			isc_interval_set(&interval, 0, 10000000); /* 10 ms */
		isc_time_nowplusinterval(&zone->signingtime, &interval);
	} else
		isc_time_settoepoch(&zone->signingtime);

	INSIST(version == NULL);
}

static isc_result_t
normalize_key(dns_rdata_t *rr, dns_rdata_t *target,
	      unsigned char *data, int size)
{
	dns_rdata_dnskey_t dnskey;
	dns_rdata_keydata_t keydata;
	isc_buffer_t buf;
	isc_result_t result;

	dns_rdata_reset(target);
	isc_buffer_init(&buf, data, size);

	switch (rr->type) {
	    case dns_rdatatype_dnskey:
		result = dns_rdata_tostruct(rr, &dnskey, NULL);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);
		dnskey.flags &= ~DNS_KEYFLAG_REVOKE;
		dns_rdata_fromstruct(target, rr->rdclass, dns_rdatatype_dnskey,
				     &dnskey, &buf);
		break;
	    case dns_rdatatype_keydata:
		result = dns_rdata_tostruct(rr, &keydata, NULL);
		if (result == ISC_R_UNEXPECTEDEND)
			return (result);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);
		dns_keydata_todnskey(&keydata, &dnskey, NULL);
		dns_rdata_fromstruct(target, rr->rdclass, dns_rdatatype_dnskey,
				     &dnskey, &buf);
		break;
	    default:
		INSIST(0);
	}
	return (ISC_R_SUCCESS);
}

/*
 * 'rdset' contains either a DNSKEY rdataset from the zone apex, or
 * a KEYDATA rdataset from the key zone.
 *
 * 'rr' contains either a DNSKEY record, or a KEYDATA record
 *
 * After normalizing keys to the same format (DNSKEY, with revoke bit
 * cleared), return ISC_TRUE if a key that matches 'rr' is found in
 * 'rdset', or ISC_FALSE if not.
 */

static isc_boolean_t
matchkey(dns_rdataset_t *rdset, dns_rdata_t *rr) {
	unsigned char data1[4096], data2[4096];
	dns_rdata_t rdata, rdata1, rdata2;
	isc_result_t result;

	dns_rdata_init(&rdata);
	dns_rdata_init(&rdata1);
	dns_rdata_init(&rdata2);

	result = normalize_key(rr, &rdata1, data1, sizeof(data1));
	if (result != ISC_R_SUCCESS)
		return (ISC_FALSE);

	for (result = dns_rdataset_first(rdset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(rdset)) {
		dns_rdata_reset(&rdata);
		dns_rdataset_current(rdset, &rdata);
		result = normalize_key(&rdata, &rdata2, data2, sizeof(data2));
		if (result != ISC_R_SUCCESS)
			continue;
		if (dns_rdata_compare(&rdata1, &rdata2) == 0)
			return (ISC_TRUE);
	}

	return (ISC_FALSE);
}

/*
 * Calculate the refresh interval for a keydata zone, per
 * RFC5011: MAX(1 hr,
 *		MIN(15 days,
 *		    1/2 * OrigTTL,
 *		    1/2 * RRSigExpirationInterval))
 * or for retries: MAX(1 hr,
 *		       MIN(1 day,
 *			   1/10 * OrigTTL,
 *			   1/10 * RRSigExpirationInterval))
 */
static inline isc_stdtime_t
refresh_time(dns_keyfetch_t *kfetch, isc_boolean_t retry) {
	isc_result_t result;
	isc_uint32_t t;
	dns_rdataset_t *rdset;
	dns_rdata_t sigrr = DNS_RDATA_INIT;
	dns_rdata_sig_t sig;
	isc_stdtime_t now;

	isc_stdtime_get(&now);

	if (dns_rdataset_isassociated(&kfetch->dnskeysigset))
		rdset = &kfetch->dnskeysigset;
	else
		return (now + dns_zone_mkey_hour);

	result = dns_rdataset_first(rdset);
	if (result != ISC_R_SUCCESS)
		return (now + dns_zone_mkey_hour);

	dns_rdataset_current(rdset, &sigrr);
	result = dns_rdata_tostruct(&sigrr, &sig, NULL);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);

	if (!retry) {
		t = sig.originalttl / 2;

		if (isc_serial_gt(sig.timeexpire, now)) {
			isc_uint32_t exp = (sig.timeexpire - now) / 2;
			if (t > exp)
				t = exp;
		}

		if (t > (15 * dns_zone_mkey_day))
			t = (15 * dns_zone_mkey_day);

		if (t < dns_zone_mkey_hour)
			t = dns_zone_mkey_hour;
	} else {
		t = sig.originalttl / 10;

		if (isc_serial_gt(sig.timeexpire, now)) {
			isc_uint32_t exp = (sig.timeexpire - now) / 10;
			if (t > exp)
				t = exp;
		}

		if (t > dns_zone_mkey_day)
			t = dns_zone_mkey_day;

		if (t < dns_zone_mkey_hour)
			t = dns_zone_mkey_hour;
	}

	return (now + t);
}

/*
 * This routine is called when no changes are needed in a KEYDATA
 * record except to simply update the refresh timer.  Caller should
 * hold zone lock.
 */
static isc_result_t
minimal_update(dns_keyfetch_t *kfetch, dns_dbversion_t *ver, dns_diff_t *diff)
{
	isc_result_t result;
	isc_buffer_t keyb;
	unsigned char key_buf[4096];
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdata_keydata_t keydata;
	dns_name_t *name;
	dns_zone_t *zone = kfetch->zone;
	isc_stdtime_t now;

	name = dns_fixedname_name(&kfetch->name);
	isc_stdtime_get(&now);

	for (result = dns_rdataset_first(&kfetch->keydataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&kfetch->keydataset)) {
		dns_rdata_reset(&rdata);
		dns_rdataset_current(&kfetch->keydataset, &rdata);

		/* Delete old version */
		CHECK(update_one_rr(kfetch->db, ver, diff, DNS_DIFFOP_DEL,
				    name, 0, &rdata));

		/* Update refresh timer */
		result = dns_rdata_tostruct(&rdata, &keydata, NULL);
		if (result == ISC_R_UNEXPECTEDEND)
			continue;
		if (result != ISC_R_SUCCESS)
			goto failure;
		keydata.refresh = refresh_time(kfetch, ISC_TRUE);
		set_refreshkeytimer(zone, &keydata, now, ISC_FALSE);

		dns_rdata_reset(&rdata);
		isc_buffer_init(&keyb, key_buf, sizeof(key_buf));
		CHECK(dns_rdata_fromstruct(&rdata,
					   zone->rdclass, dns_rdatatype_keydata,
					   &keydata, &keyb));

		/* Insert updated version */
		CHECK(update_one_rr(kfetch->db, ver, diff, DNS_DIFFOP_ADD,
				    name, 0, &rdata));
	}
	result = ISC_R_SUCCESS;
  failure:
	return (result);
}

/*
 * Verify that DNSKEY set is signed by the key specified in 'keydata'.
 */
static isc_boolean_t
revocable(dns_keyfetch_t *kfetch, dns_rdata_keydata_t *keydata) {
	isc_result_t result;
	dns_name_t *keyname;
	isc_mem_t *mctx;
	dns_rdata_t sigrr = DNS_RDATA_INIT;
	dns_rdata_t rr = DNS_RDATA_INIT;
	dns_rdata_rrsig_t sig;
	dns_rdata_dnskey_t dnskey;
	dst_key_t *dstkey = NULL;
	unsigned char key_buf[4096];
	isc_buffer_t keyb;
	isc_boolean_t answer = ISC_FALSE;

	REQUIRE(kfetch != NULL && keydata != NULL);
	REQUIRE(dns_rdataset_isassociated(&kfetch->dnskeysigset));

	keyname = dns_fixedname_name(&kfetch->name);
	mctx = kfetch->zone->view->mctx;

	/* Generate a key from keydata */
	isc_buffer_init(&keyb, key_buf, sizeof(key_buf));
	dns_keydata_todnskey(keydata, &dnskey, NULL);
	dns_rdata_fromstruct(&rr, keydata->common.rdclass,
			     dns_rdatatype_dnskey, &dnskey, &keyb);
	result = dns_dnssec_keyfromrdata(keyname, &rr, mctx, &dstkey);
	if (result != ISC_R_SUCCESS)
		return (ISC_FALSE);

	/* See if that key generated any of the signatures */
	for (result = dns_rdataset_first(&kfetch->dnskeysigset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&kfetch->dnskeysigset))
	{
		dns_fixedname_t fixed;
		dns_fixedname_init(&fixed);

		dns_rdata_reset(&sigrr);
		dns_rdataset_current(&kfetch->dnskeysigset, &sigrr);
		result = dns_rdata_tostruct(&sigrr, &sig, NULL);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);

		if (dst_key_alg(dstkey) == sig.algorithm &&
		    dst_key_rid(dstkey) == sig.keyid)
		{
			result = dns_dnssec_verify2(keyname,
					    &kfetch->dnskeyset,
					    dstkey, ISC_FALSE, mctx, &sigrr,
					    dns_fixedname_name(&fixed));

			dns_zone_log(kfetch->zone, ISC_LOG_DEBUG(3),
				     "Confirm revoked DNSKEY is self-signed: "
				     "%s", dns_result_totext(result));

			if (result == ISC_R_SUCCESS) {
				answer = ISC_TRUE;
				break;
			}
		}
	}

	dst_key_free(&dstkey);
	return (answer);
}

/*
 * A DNSKEY set has been fetched from the zone apex of a zone whose trust
 * anchors are being managed; scan the keyset, and update the key zone and the
 * local trust anchors according to RFC5011.
 */
static void
keyfetch_done(isc_task_t *task, isc_event_t *event) {
	isc_result_t result, eresult;
	dns_fetchevent_t *devent;
	dns_keyfetch_t *kfetch;
	dns_zone_t *zone;
	isc_mem_t *mctx = NULL;
	dns_keytable_t *secroots = NULL;
	dns_dbversion_t *ver = NULL;
	dns_diff_t diff;
	isc_boolean_t alldone = ISC_FALSE;
	isc_boolean_t commit = ISC_FALSE;
	dns_name_t *keyname;
	dns_rdata_t sigrr = DNS_RDATA_INIT;
	dns_rdata_t dnskeyrr = DNS_RDATA_INIT;
	dns_rdata_t keydatarr = DNS_RDATA_INIT;
	dns_rdata_rrsig_t sig;
	dns_rdata_dnskey_t dnskey;
	dns_rdata_keydata_t keydata;
	isc_boolean_t initializing;
	char namebuf[DNS_NAME_FORMATSIZE];
	unsigned char key_buf[4096];
	isc_buffer_t keyb;
	dst_key_t *dstkey;
	isc_stdtime_t now;
	int pending = 0;
	isc_boolean_t secure;
	isc_boolean_t free_needed;

	UNUSED(task);
	INSIST(event != NULL && event->ev_type == DNS_EVENT_FETCHDONE);
	INSIST(event->ev_arg != NULL);

	kfetch = event->ev_arg;
	zone = kfetch->zone;
	isc_mem_attach(zone->mctx, &mctx);
	keyname = dns_fixedname_name(&kfetch->name);

	devent = (dns_fetchevent_t *) event;
	eresult = devent->result;

	/* Free resources which are not of interest */
	if (devent->node != NULL)
		dns_db_detachnode(devent->db, &devent->node);
	if (devent->db != NULL)
		dns_db_detach(&devent->db);
	isc_event_free(&event);
	dns_resolver_destroyfetch(&kfetch->fetch);

	LOCK_ZONE(zone);
	if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_EXITING) || zone->view == NULL)
		goto cleanup;

	isc_stdtime_get(&now);
	dns_name_format(keyname, namebuf, sizeof(namebuf));

	result = dns_view_getsecroots(zone->view, &secroots);
	INSIST(result == ISC_R_SUCCESS);

	dns_diff_init(mctx, &diff);

	CHECK(dns_db_newversion(kfetch->db, &ver));

	zone->refreshkeycount--;
	alldone = ISC_TF(zone->refreshkeycount == 0);

	if (alldone)
		DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_REFRESHING);

	/* Fetch failed */
	if (eresult != ISC_R_SUCCESS ||
	    !dns_rdataset_isassociated(&kfetch->dnskeyset)) {
		dns_zone_log(zone, ISC_LOG_WARNING,
			     "Unable to fetch DNSKEY set "
			     "'%s': %s", namebuf, dns_result_totext(eresult));
		CHECK(minimal_update(kfetch, ver, &diff));
		goto done;
	}

	/* No RRSIGs found */
	if (!dns_rdataset_isassociated(&kfetch->dnskeysigset)) {
		dns_zone_log(zone, ISC_LOG_WARNING,
			     "No DNSKEY RRSIGs found for "
			     "'%s': %s", namebuf, dns_result_totext(eresult));
		CHECK(minimal_update(kfetch, ver, &diff));
		goto done;
	}

	/*
	 * Clear any cached trust level, as we need to run validation
	 * over again; trusted keys might have changed.
	 */
	kfetch->dnskeyset.trust = kfetch->dnskeysigset.trust = dns_trust_none;

	/*
	 * Validate the dnskeyset against the current trusted keys.
	 */
	for (result = dns_rdataset_first(&kfetch->dnskeysigset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&kfetch->dnskeysigset)) {
		dns_keynode_t *keynode = NULL;

		dns_rdata_reset(&sigrr);
		dns_rdataset_current(&kfetch->dnskeysigset, &sigrr);
		result = dns_rdata_tostruct(&sigrr, &sig, NULL);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);

		result = dns_keytable_find(secroots, keyname, &keynode);
		while (result == ISC_R_SUCCESS) {
			dns_keynode_t *nextnode = NULL;
			dns_fixedname_t fixed;
			dns_fixedname_init(&fixed);

			dstkey = dns_keynode_key(keynode);
			if (dstkey == NULL) /* fail_secure() was called */
				break;

			if (dst_key_alg(dstkey) == sig.algorithm &&
			    dst_key_id(dstkey) == sig.keyid) {
				result = dns_dnssec_verify2(keyname,
						    &kfetch->dnskeyset,
						    dstkey, ISC_FALSE,
						    zone->view->mctx, &sigrr,
						    dns_fixedname_name(&fixed));

				dns_zone_log(zone, ISC_LOG_DEBUG(3),
					     "Verifying DNSKEY set for zone "
					     "'%s' using key %d/%d: %s",
					     namebuf, sig.keyid, sig.algorithm,
					     dns_result_totext(result));

				if (result == ISC_R_SUCCESS) {
					kfetch->dnskeyset.trust =
						dns_trust_secure;
					kfetch->dnskeysigset.trust =
						dns_trust_secure;
					break;
				}
			}

			result = dns_keytable_nextkeynode(secroots,
							  keynode, &nextnode);
			dns_keytable_detachkeynode(secroots, &keynode);
			keynode = nextnode;
		}

		if (keynode != NULL)
			dns_keytable_detachkeynode(secroots, &keynode);

		if (kfetch->dnskeyset.trust == dns_trust_secure)
			break;
	}

	/*
	 * If we were not able to verify the answer using the current
	 * trusted keys then all we can do is look at any revoked keys.
	 */
	secure = ISC_TF(kfetch->dnskeyset.trust == dns_trust_secure);

	/*
	 * First scan keydataset to find keys that are not in dnskeyset
	 *   - Missing keys which are not scheduled for removal,
	 *     log a warning
	 *   - Missing keys which are scheduled for removal and
	 *     the remove hold-down timer has completed should
	 *     be removed from the key zone
	 *   - Missing keys whose acceptance timers have not yet
	 *     completed, log a warning and reset the acceptance
	 *     timer to 30 days in the future
	 *   - All keys not being removed have their refresh timers
	 *     updated
	 */
	initializing = ISC_TRUE;
	for (result = dns_rdataset_first(&kfetch->keydataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&kfetch->keydataset)) {
		dns_rdata_reset(&keydatarr);
		dns_rdataset_current(&kfetch->keydataset, &keydatarr);
		result = dns_rdata_tostruct(&keydatarr, &keydata, NULL);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);

		/*
		 * If any keydata record has a nonzero add holddown, then
		 * there was a pre-existing trust anchor for this domain;
		 * that means we are *not* initializing it and shouldn't
		 * automatically trust all the keys we find at the zone apex.
		 */
		initializing = initializing && ISC_TF(keydata.addhd == 0);

		if (! matchkey(&kfetch->dnskeyset, &keydatarr)) {
			isc_boolean_t deletekey = ISC_FALSE;

			if (!secure) {
				if (keydata.removehd != 0 &&
				    keydata.removehd <= now)
					deletekey = ISC_TRUE;
			} else if (keydata.addhd == 0) {
				deletekey = ISC_TRUE;
			} else if (keydata.addhd > now) {
				dns_zone_log(zone, ISC_LOG_WARNING,
					     "Pending key unexpectedly missing "
					     "from %s; restarting acceptance "
					     "timer", namebuf);
				if (keydata.addhd < now + dns_zone_mkey_month)
					keydata.addhd =
						now + dns_zone_mkey_month;
				keydata.refresh = refresh_time(kfetch,
							       ISC_FALSE);
			} else if (keydata.removehd == 0) {
				dns_zone_log(zone, ISC_LOG_WARNING,
					     "Active key unexpectedly missing "
					     "from %s", namebuf);
				keydata.refresh = now + dns_zone_mkey_hour;
			} else if (keydata.removehd <= now) {
				deletekey = ISC_TRUE;
			} else {
				keydata.refresh = refresh_time(kfetch,
							       ISC_FALSE);
			}

			if (secure || deletekey) {
				/* Delete old version */
				CHECK(update_one_rr(kfetch->db, ver, &diff,
						    DNS_DIFFOP_DEL, keyname, 0,
						    &keydatarr));
			}

			if (!secure || deletekey)
				continue;

			dns_rdata_reset(&keydatarr);
			isc_buffer_init(&keyb, key_buf, sizeof(key_buf));
			dns_rdata_fromstruct(&keydatarr, zone->rdclass,
					     dns_rdatatype_keydata,
					     &keydata, &keyb);

			/* Insert updated version */
			CHECK(update_one_rr(kfetch->db, ver, &diff,
					    DNS_DIFFOP_ADD, keyname, 0,
					    &keydatarr));

			set_refreshkeytimer(zone, &keydata, now, ISC_FALSE);
		}
	}

	/*
	 * Next scan dnskeyset:
	 *   - If new keys are found (i.e., lacking a match in keydataset)
	 *     add them to the key zone and set the acceptance timer
	 *     to 30 days in the future (or to immediately if we've
	 *     determined that we're initializing the zone for the
	 *     first time)
	 *   - Previously-known keys that have been revoked
	 *     must be scheduled for removal from the key zone (or,
	 *     if they hadn't been accepted as trust anchors yet
	 *     anyway, removed at once)
	 *   - Previously-known unrevoked keys whose acceptance timers
	 *     have completed are promoted to trust anchors
	 *   - All keys not being removed have their refresh
	 *     timers updated
	 */
	for (result = dns_rdataset_first(&kfetch->dnskeyset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&kfetch->dnskeyset))
	{
		isc_boolean_t revoked = ISC_FALSE;
		isc_boolean_t newkey = ISC_FALSE;
		isc_boolean_t updatekey = ISC_FALSE;
		isc_boolean_t deletekey = ISC_FALSE;
		isc_boolean_t trustkey = ISC_FALSE;

		dns_rdata_reset(&dnskeyrr);
		dns_rdataset_current(&kfetch->dnskeyset, &dnskeyrr);
		result = dns_rdata_tostruct(&dnskeyrr, &dnskey, NULL);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);

		/* Skip ZSK's */
		if (!ISC_TF(dnskey.flags & DNS_KEYFLAG_KSK))
			continue;

		revoked = ISC_TF(dnskey.flags & DNS_KEYFLAG_REVOKE);

		if (matchkey(&kfetch->keydataset, &dnskeyrr)) {
			dns_rdata_reset(&keydatarr);
			dns_rdataset_current(&kfetch->keydataset, &keydatarr);
			result = dns_rdata_tostruct(&keydatarr, &keydata, NULL);
			RUNTIME_CHECK(result == ISC_R_SUCCESS);

			if (revoked && revocable(kfetch, &keydata)) {
				if (keydata.addhd > now) {
					/*
					 * Key wasn't trusted yet, and now
					 * it's been revoked?  Just remove it
					 */
					deletekey = ISC_TRUE;
				} else if (keydata.removehd == 0) {
					/* Remove from secroots */
					dns_view_untrust(zone->view, keyname,
							 &dnskey, mctx);

					/* But ensure there's a null key */
					fail_secure(zone, keyname);

					/* If initializing, delete now */
					if (keydata.addhd == 0)
						deletekey = ISC_TRUE;
					else {
						keydata.removehd = now +
							dns_zone_mkey_month;
						keydata.flags |=
							DNS_KEYFLAG_REVOKE;
					}
				} else if (keydata.removehd < now) {
					/* Scheduled for removal */
					deletekey = ISC_TRUE;
				}
			} else if (revoked && keydata.removehd == 0) {
				dns_zone_log(zone, ISC_LOG_WARNING,
					     "Active key for zone "
					     "'%s' is revoked but "
					     "did not self-sign; "
					     "ignoring.", namebuf);
					continue;
			} else if (secure) {
				if (keydata.removehd != 0) {
					/*
					 * Key isn't revoked--but it
					 * seems it used to be.
					 * Remove it now and add it
					 * back as if it were a fresh key,
					 * with a 30 day acceptance timer.
					 */
					deletekey = ISC_TRUE;
					newkey = ISC_TRUE;
					keydata.removehd = 0;
					keydata.addhd =
						now + dns_zone_mkey_month;
				} else if (keydata.addhd > now)
					pending++;
				else if (keydata.addhd == 0)
					keydata.addhd = now;

				if (keydata.addhd <= now)
					trustkey = ISC_TRUE;
			} else if (keydata.addhd > now) {
				/*
				 * Not secure, and key is pending:
				 * reset the acceptance timer
				 */
				pending++;
				keydata.addhd = now + dns_zone_mkey_month;
			}

			if (!deletekey && !newkey)
				updatekey = ISC_TRUE;
		} else if (secure) {
			/*
			 * Key wasn't in the key zone but it's
			 * revoked now anyway, so just skip it
			 */
			if (revoked)
				continue;

			/* Key wasn't in the key zone: add it */
			newkey = ISC_TRUE;

			if (initializing) {
				dns_keytag_t tag = 0;
				CHECK(compute_tag(keyname, &dnskey,
						  mctx, &tag));
				dns_zone_log(zone, ISC_LOG_WARNING,
					     "Initializing automatic trust "
					     "anchor management for zone '%s'; "
					     "DNSKEY ID %d is now trusted, "
					     "waiving the normal 30-day "
					     "waiting period.",
					     namebuf, tag);
				trustkey = ISC_TRUE;
			}
		} else {
			/*
			 * No previously known key, and the key is not
			 * secure, so skip it.
			 */
			continue;
		}

		/* Delete old version */
		if (deletekey || !newkey)
			CHECK(update_one_rr(kfetch->db, ver, &diff,
					    DNS_DIFFOP_DEL, keyname, 0,
					    &keydatarr));

		if (updatekey) {
			/* Set refresh timer */
			keydata.refresh = refresh_time(kfetch, ISC_FALSE);
			dns_rdata_reset(&keydatarr);
			isc_buffer_init(&keyb, key_buf, sizeof(key_buf));
			dns_rdata_fromstruct(&keydatarr, zone->rdclass,
					     dns_rdatatype_keydata,
					     &keydata, &keyb);

			/* Insert updated version */
			CHECK(update_one_rr(kfetch->db, ver, &diff,
					    DNS_DIFFOP_ADD, keyname, 0,
					    &keydatarr));
		} else if (newkey) {
			/* Convert DNSKEY to KEYDATA */
			result = dns_rdata_tostruct(&dnskeyrr, &dnskey, NULL);
			RUNTIME_CHECK(result == ISC_R_SUCCESS);
			dns_keydata_fromdnskey(&keydata, &dnskey, 0, 0, 0,
					       NULL);
			keydata.addhd = initializing
					 ? now : now + dns_zone_mkey_month;
			keydata.refresh = refresh_time(kfetch, ISC_FALSE);
			dns_rdata_reset(&keydatarr);
			isc_buffer_init(&keyb, key_buf, sizeof(key_buf));
			dns_rdata_fromstruct(&keydatarr, zone->rdclass,
					     dns_rdatatype_keydata,
					     &keydata, &keyb);

			/* Insert into key zone */
			CHECK(update_one_rr(kfetch->db, ver, &diff,
					    DNS_DIFFOP_ADD, keyname, 0,
					    &keydatarr));
		}

		if (trustkey) {
			/* Trust this key. */
			result = dns_rdata_tostruct(&dnskeyrr, &dnskey, NULL);
			RUNTIME_CHECK(result == ISC_R_SUCCESS);
			trust_key(zone, keyname, &dnskey, mctx);
		}

		if (secure && !deletekey) {
			INSIST(newkey || updatekey);
			set_refreshkeytimer(zone, &keydata, now, ISC_FALSE);
		}
	}

	/*
	 * RFC5011 says, "A trust point that has all of its trust anchors
	 * revoked is considered deleted and is treated as if the trust
	 * point was never configured."  But if someone revoked their
	 * active key before the standby was trusted, that would mean the
	 * zone would suddenly be nonsecured.  We avoid this by checking to
	 * see if there's pending keydata.  If so, we put a null key in
	 * the security roots; then all queries to the zone will fail.
	 */
	if (pending != 0)
		fail_secure(zone, keyname);

 done:

	if (!ISC_LIST_EMPTY(diff.tuples)) {
		/* Write changes to journal file. */
		CHECK(update_soa_serial(kfetch->db, ver, &diff, mctx,
					zone->updatemethod));
		CHECK(zone_journal(zone, &diff, NULL, "keyfetch_done"));
		commit = ISC_TRUE;

		DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_LOADED);
		zone_needdump(zone, 30);
	}

 failure:

	dns_diff_clear(&diff);
	if (ver != NULL)
		dns_db_closeversion(kfetch->db, &ver, commit);

 cleanup:
	dns_db_detach(&kfetch->db);

	INSIST(zone->irefs > 0);
	zone->irefs--;
	kfetch->zone = NULL;

	if (dns_rdataset_isassociated(&kfetch->keydataset))
		dns_rdataset_disassociate(&kfetch->keydataset);
	if (dns_rdataset_isassociated(&kfetch->dnskeyset))
		dns_rdataset_disassociate(&kfetch->dnskeyset);
	if (dns_rdataset_isassociated(&kfetch->dnskeysigset))
		dns_rdataset_disassociate(&kfetch->dnskeysigset);

	dns_name_free(keyname, mctx);
	isc_mem_put(mctx, kfetch, sizeof(dns_keyfetch_t));
	isc_mem_detach(&mctx);

	if (secroots != NULL)
		dns_keytable_detach(&secroots);

	free_needed = exit_check(zone);
	UNLOCK_ZONE(zone);
	if (free_needed)
		zone_free(zone);

	INSIST(ver == NULL);
}

/*
 * Refresh the data in the key zone.  Initiate a fetch to get new DNSKEY
 * records from the zone apex.
 */
static void
zone_refreshkeys(dns_zone_t *zone) {
	const char me[] = "zone_refreshkeys";
	isc_result_t result;
	dns_rriterator_t rrit;
	dns_db_t *db = NULL;
	dns_dbversion_t *ver = NULL;
	dns_diff_t diff;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdata_keydata_t kd;
	isc_stdtime_t now;
	isc_boolean_t commit = ISC_FALSE;
	isc_boolean_t fetching = ISC_FALSE, fetch_err = ISC_FALSE;

	ENTER;
	REQUIRE(zone->db != NULL);

	isc_stdtime_get(&now);

	LOCK_ZONE(zone);
	if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_EXITING)) {
		isc_time_settoepoch(&zone->refreshkeytime);
		UNLOCK_ZONE(zone);
		return;
	}

	ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_read);
	dns_db_attach(zone->db, &db);
	ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);

	dns_diff_init(zone->mctx, &diff);

	CHECK(dns_db_newversion(db, &ver));

	DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_REFRESHING);

	dns_rriterator_init(&rrit, db, ver, 0);
	for (result = dns_rriterator_first(&rrit);
	     result == ISC_R_SUCCESS;
	     result = dns_rriterator_nextrrset(&rrit)) {
		isc_stdtime_t timer = 0xffffffff;
		dns_name_t *name = NULL, *kname = NULL;
		dns_rdataset_t *kdset = NULL;
		dns_keyfetch_t *kfetch;
		isc_uint32_t ttl;

		dns_rriterator_current(&rrit, &name, &ttl, &kdset, NULL);
		if (kdset == NULL || kdset->type != dns_rdatatype_keydata ||
		    !dns_rdataset_isassociated(kdset))
			continue;

		/*
		 * Scan the stored keys looking for ones that need
		 * removal or refreshing
		 */
		for (result = dns_rdataset_first(kdset);
		     result == ISC_R_SUCCESS;
		     result = dns_rdataset_next(kdset)) {
			dns_rdata_reset(&rdata);
			dns_rdataset_current(kdset, &rdata);
			result = dns_rdata_tostruct(&rdata, &kd, NULL);
			RUNTIME_CHECK(result == ISC_R_SUCCESS);

			/* Removal timer expired? */
			if (kd.removehd != 0 && kd.removehd < now) {
				CHECK(update_one_rr(db, ver, &diff,
						    DNS_DIFFOP_DEL, name, ttl,
						    &rdata));
				continue;
			}

			/* Acceptance timer expired? */
			if (kd.addhd != 0 && kd.addhd < now)
				timer = kd.addhd;

			/* Or do we just need to refresh the keyset? */
			if (timer > kd.refresh)
				timer = kd.refresh;
		}

		if (timer > now)
			continue;

		kfetch = isc_mem_get(zone->mctx, sizeof(dns_keyfetch_t));
		if (kfetch == NULL) {
			fetch_err = ISC_TRUE;
			goto failure;
		}

		zone->refreshkeycount++;
		kfetch->zone = zone;
		zone->irefs++;
		INSIST(zone->irefs != 0);
		dns_fixedname_init(&kfetch->name);
		kname = dns_fixedname_name(&kfetch->name);
		dns_name_dup(name, zone->mctx, kname);
		dns_rdataset_init(&kfetch->dnskeyset);
		dns_rdataset_init(&kfetch->dnskeysigset);
		dns_rdataset_init(&kfetch->keydataset);
		dns_rdataset_clone(kdset, &kfetch->keydataset);
		kfetch->db = NULL;
		dns_db_attach(db, &kfetch->db);
		kfetch->fetch = NULL;

		result = dns_resolver_createfetch(zone->view->resolver,
						  kname, dns_rdatatype_dnskey,
						  NULL, NULL, NULL,
						  DNS_FETCHOPT_NOVALIDATE,
						  zone->task,
						  keyfetch_done, kfetch,
						  &kfetch->dnskeyset,
						  &kfetch->dnskeysigset,
						  &kfetch->fetch);
		if (result == ISC_R_SUCCESS)
			fetching = ISC_TRUE;
		else {
			zone->refreshkeycount--;
			zone->irefs--;
			dns_db_detach(&kfetch->db);
			dns_rdataset_disassociate(&kfetch->keydataset);
			dns_name_free(kname, zone->mctx);
			isc_mem_put(zone->mctx, kfetch, sizeof(dns_keyfetch_t));
			dns_zone_log(zone, ISC_LOG_WARNING,
				     "Failed to create fetch for "
				     "DNSKEY update");
			fetch_err = ISC_TRUE;
		}
	}
	if (!ISC_LIST_EMPTY(diff.tuples)) {
		CHECK(update_soa_serial(db, ver, &diff, zone->mctx,
					zone->updatemethod));
		CHECK(zone_journal(zone, &diff, NULL, "zone_refreshkeys"));
		commit = ISC_TRUE;
		DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_LOADED);
		zone_needdump(zone, 30);
	}

  failure:
	if (fetch_err) {
		/*
		 * Error during a key fetch; retry in an hour.
		 */
		isc_time_t timenow, timethen;
		char timebuf[80];

		TIME_NOW(&timenow);
		DNS_ZONE_TIME_ADD(&timenow, dns_zone_mkey_hour, &timethen);
		zone->refreshkeytime = timethen;
		zone_settimer(zone, &timenow);

		isc_time_formattimestamp(&zone->refreshkeytime, timebuf, 80);
		dns_zone_log(zone, ISC_LOG_DEBUG(1), "retry key refresh: %s",
			     timebuf);

		if (!fetching)
			DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_REFRESHING);
	}

	UNLOCK_ZONE(zone);

	dns_diff_clear(&diff);
	if (ver != NULL) {
		dns_rriterator_destroy(&rrit);
		dns_db_closeversion(db, &ver, commit);
	}
	dns_db_detach(&db);

	INSIST(ver == NULL);
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
	 * Are we pending load/reload?
	 */
	if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_LOADPENDING))
		return;

	/*
	 * Configuring the view of this zone may have
	 * failed, for example because the config file
	 * had a syntax error.	In that case, the view
	 * adb or resolver will be NULL, and we had better not try
	 * to do further maintenance on it.
	 */
	if (zone->view == NULL || zone->view->adb == NULL)
		return;

	TIME_NOW(&now);

	/*
	 * Expire check.
	 */
	switch (zone->type) {
	case dns_zone_redirect:
		if (zone->masters == NULL)
			break;
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
	case dns_zone_redirect:
		if (zone->masters == NULL)
			break;
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
	 * Slaves send notifies before backing up to disk, masters after.
	 */
	if (zone->type == dns_zone_slave &&
	    (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NEEDNOTIFY) ||
	     DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NEEDSTARTUPNOTIFY)) &&
	    isc_time_compare(&now, &zone->notifytime) >= 0)
		zone_notify(zone, &now);

	/*
	 * Do we need to consolidate the backing store?
	 */
	switch (zone->type) {
	case dns_zone_master:
	case dns_zone_slave:
	case dns_zone_key:
	case dns_zone_redirect:
	case dns_zone_stub:
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
	 * Master/redirect zones send notifies now, if needed
	 */
	switch (zone->type) {
	case dns_zone_master:
	case dns_zone_redirect:
		if ((DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NEEDNOTIFY) ||
		     DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NEEDSTARTUPNOTIFY))&&
		    isc_time_compare(&now, &zone->notifytime) >= 0)
			zone_notify(zone, &now);
	default:
		break;
	}

	/*
	 * Do we need to refresh keys?
	 */
	switch (zone->type) {
	case dns_zone_key:
		if (isc_time_compare(&now, &zone->refreshkeytime) >= 0) {
			if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_LOADED) &&
			    !DNS_ZONE_FLAG(zone, DNS_ZONEFLG_REFRESHING)) {
				zone_refreshkeys(zone);
			}
		}
		break;
	case dns_zone_master:
		if (!isc_time_isepoch(&zone->refreshkeytime) &&
		    isc_time_compare(&now, &zone->refreshkeytime) >= 0 &&
		    zone->rss_event == NULL)
			zone_rekey(zone);
	default:
		break;
	}

	switch (zone->type) {
	case dns_zone_master:
	case dns_zone_redirect:
	case dns_zone_slave:
		/*
		 * Do we need to sign/resign some RRsets?
		 */
		if (zone->rss_event != NULL)
			break;
		if (!isc_time_isepoch(&zone->signingtime) &&
		    isc_time_compare(&now, &zone->signingtime) >= 0)
			zone_sign(zone);
		else if (!isc_time_isepoch(&zone->resigntime) &&
		    isc_time_compare(&now, &zone->resigntime) >= 0)
			zone_resigninc(zone);
		else if (!isc_time_isepoch(&zone->nsec3chaintime) &&
			isc_time_compare(&now, &zone->nsec3chaintime) >= 0)
			zone_nsec3chain(zone);
		/*
		 * Do we need to issue a key expiry warning?
		 */
		if (!isc_time_isepoch(&zone->keywarntime) &&
		    isc_time_compare(&now, &zone->keywarntime) >= 0)
			set_key_expiry_warning(zone, zone->key_expiry,
					       isc_time_seconds(&now));
		break;

	default:
		break;
	}
	zone_settimer(zone, &now);
}

void
dns_zone_markdirty(dns_zone_t *zone) {
	isc_uint32_t serial;
	isc_result_t result = ISC_R_SUCCESS;
	dns_zone_t *secure = NULL;

	/*
	 * Obtaining a lock on the zone->secure (see zone_send_secureserial)
	 * could result in a deadlock due to a LOR so we will spin if we
	 * can't obtain the both locks.
	 */
 again:
	LOCK_ZONE(zone);
	if (zone->type == dns_zone_master) {
		if (inline_raw(zone)) {
			unsigned int soacount;
			secure = zone->secure;
			INSIST(secure != zone);
			TRYLOCK_ZONE(result, secure);
			if (result != ISC_R_SUCCESS) {
				UNLOCK_ZONE(zone);
				secure = NULL;
#ifdef ISC_PLATFORM_USETHREADS
				isc_thread_yield();
#endif
				goto again;
			}

			ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_read);
			if (zone->db != NULL) {
				result = zone_get_from_db(zone, zone->db, NULL,
							  &soacount, &serial,
							  NULL, NULL, NULL,
							  NULL, NULL);
			} else
				result = DNS_R_NOTLOADED;
			ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);
			if (result == ISC_R_SUCCESS && soacount > 0U)
				zone_send_secureserial(zone, serial);
		}

		/* XXXMPA make separate call back */
		if (result == ISC_R_SUCCESS)
			set_resigntime(zone);
	}
	if (secure != NULL)
		UNLOCK_ZONE(secure);
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
	if (result != ISC_R_SUCCESS)
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
	const char me[] = "zone_needdump";
	isc_time_t dumptime;
	isc_time_t now;

	/*
	 * 'zone' locked by caller
	 */

	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(LOCKED_ZONE(zone));
	ENTER;

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
		 * If there is a secure version of this zone
		 * use its serial if it is less than ours.
		 */
		if (tresult == ISC_R_SUCCESS && inline_raw(zone) &&
		    zone->secure->db != NULL)
		{
			isc_uint32_t sserial;
			isc_result_t mresult;

			mresult = dns_db_getsoaserial(zone->secure->db,
						      NULL, &sserial);
			if (mresult == ISC_R_SUCCESS &&
			    isc_serial_lt(sserial, serial))
				serial = sserial;
		}
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

	if (compact && zone->type != dns_zone_stub) {
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
		const dns_master_style_t *output_style;

		dns_masterrawheader_t rawdata;
		dns_db_currentversion(db, &version);
		dns_master_initrawheader(&rawdata);
		if (inline_secure(zone))
			get_raw_serial(zone->raw, &rawdata);
		if (zone->type == dns_zone_key)
			output_style = &dns_master_style_keyzone;
		else
			output_style = &dns_master_style_default;
		result = dns_master_dump3(zone->mctx, db, version,
					  output_style, masterfile,
					  masterformat, &rawdata);
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
	     dns_masterformat_t format, const isc_uint32_t rawversion)
{
	isc_result_t result;
	dns_dbversion_t *version = NULL;
	dns_db_t *db = NULL;
	dns_masterrawheader_t rawdata;

	REQUIRE(DNS_ZONE_VALID(zone));

	ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_read);
	if (zone->db != NULL)
		dns_db_attach(zone->db, &db);
	ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);
	if (db == NULL)
		return (DNS_R_NOTLOADED);

	dns_db_currentversion(db, &version);
	dns_master_initrawheader(&rawdata);
	if (rawversion == 0)
		rawdata.flags |= DNS_MASTERRAW_COMPAT;
	else if (inline_secure(zone))
		get_raw_serial(zone->raw, &rawdata);
	else if (zone->sourceserialset) {
		rawdata.flags = DNS_MASTERRAW_SOURCESERIALSET;
		rawdata.sourceserial = zone->sourceserial;
	}
	result = dns_master_dumptostream3(zone->mctx, db, version, style,
					  format, &rawdata, fd);
	dns_db_closeversion(db, &version, ISC_FALSE);
	dns_db_detach(&db);
	return (result);
}

isc_result_t
dns_zone_dumptostream3(dns_zone_t *zone, FILE *fd, dns_masterformat_t format,
		       const dns_master_style_t *style,
		       const isc_uint32_t rawversion)
{
	return (dumptostream(zone, fd, style, format, rawversion));
}

isc_result_t
dns_zone_dumptostream2(dns_zone_t *zone, FILE *fd, dns_masterformat_t format,
		       const dns_master_style_t *style) {
	return (dumptostream(zone, fd, style, format, DNS_RAWFORMAT_VERSION));
}

isc_result_t
dns_zone_dumptostream(dns_zone_t *zone, FILE *fd) {
	return (dumptostream(zone, fd, &dns_master_style_default,
			     dns_masterformat_text, 0));
}

isc_result_t
dns_zone_fulldumptostream(dns_zone_t *zone, FILE *fd) {
	return (dumptostream(zone, fd, &dns_master_style_full,
			     dns_masterformat_text, 0));
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
forward_cancel(dns_zone_t *zone) {
	dns_forward_t *forward;

	/*
	 * 'zone' locked by caller.
	 */

	REQUIRE(LOCKED_ZONE(zone));

	for (forward = ISC_LIST_HEAD(zone->forwards);
	     forward != NULL;
	     forward = ISC_LIST_NEXT(forward, link)) {
		if (forward->request != NULL)
			dns_request_cancel(forward->request);
	}
}

static void
zone_unload(dns_zone_t *zone) {
	/*
	 * 'zone' locked by caller.
	 */

	REQUIRE(LOCKED_ZONE(zone));

	if (!DNS_ZONE_FLAG(zone, DNS_ZONEFLG_FLUSH) ||
	    !DNS_ZONE_FLAG(zone, DNS_ZONEFLG_DUMPING)) {
		if (zone->writeio != NULL)
			zonemgr_cancelio(zone->writeio);

		if (zone->dctx != NULL)
			dns_dumpctx_cancel(zone->dctx);
	}
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
notify_isqueued(dns_zone_t *zone, unsigned int flags, dns_name_t *name,
		isc_sockaddr_t *addr, dns_tsigkey_t *key)
{
	dns_notify_t *notify;
	dns_zonemgr_t *zmgr;
	isc_result_t result;

	for (notify = ISC_LIST_HEAD(zone->notifies);
	     notify != NULL;
	     notify = ISC_LIST_NEXT(notify, link)) {
		if (notify->request != NULL)
			continue;
		if (name != NULL && dns_name_dynamic(&notify->ns) &&
		    dns_name_equal(name, &notify->ns))
			goto requeue;
		if (addr != NULL && isc_sockaddr_equal(addr, &notify->dst) &&
		    notify->key == key)
			goto requeue;
	}
	return (ISC_FALSE);

requeue:
	/*
	 * If we are enqueued on the startup ratelimiter and this is
	 * not a startup notify, re-enqueue on the normal notify
	 * ratelimiter.
	 */
	if (notify->event != NULL && (flags & DNS_NOTIFY_STARTUP) == 0 &&
	    (notify->flags & DNS_NOTIFY_STARTUP) != 0) {
		zmgr = notify->zone->zmgr;
		result = isc_ratelimiter_dequeue(zmgr->startupnotifyrl,
						 notify->event);
		if (result != ISC_R_SUCCESS)
			return (ISC_TRUE);

		notify->flags &= ~DNS_NOTIFY_STARTUP;
		result = isc_ratelimiter_enqueue(notify->zone->zmgr->notifyrl,
						 notify->zone->task,
						 &notify->event);
		if (result != ISC_R_SUCCESS) {
			isc_event_free(&notify->event);
			return (ISC_FALSE);
		}
	}

	return (ISC_TRUE);
}

static isc_boolean_t
notify_isself(dns_zone_t *zone, isc_sockaddr_t *dst) {
	dns_tsigkey_t *key = NULL;
	isc_sockaddr_t src;
	isc_sockaddr_t any;
	isc_boolean_t isself;
	isc_netaddr_t dstaddr;
	isc_result_t result;

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
	result = dns_view_getpeertsig(zone->view, &dstaddr, &key);
	if (result != ISC_R_SUCCESS && result != ISC_R_NOTFOUND)
		return (ISC_FALSE);
	isself = (zone->isself)(zone->view, key, &src, dst, zone->rdclass,
				zone->isselfarg);
	if (key != NULL)
		dns_tsigkey_detach(&key);
	return (isself);
}

static void
notify_destroy(dns_notify_t *notify, isc_boolean_t locked) {
	isc_mem_t *mctx;

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
	if (notify->key != NULL)
		dns_tsigkey_detach(&notify->key);
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
	notify->key = NULL;
	notify->event = NULL;
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
notify_send_queue(dns_notify_t *notify, isc_boolean_t startup) {
	isc_event_t *e;
	isc_result_t result;

	INSIST(notify->event == NULL);
	e = isc_event_allocate(notify->mctx, NULL, DNS_EVENT_NOTIFYSENDTOADDR,
			       notify_send_toaddr, notify, sizeof(isc_event_t));
	if (e == NULL)
		return (ISC_R_NOMEMORY);
	if (startup)
		notify->event = e;
	e->ev_arg = notify;
	e->ev_sender = NULL;
	result = isc_ratelimiter_enqueue(startup
					  ? notify->zone->zmgr->startupnotifyrl
					  : notify->zone->zmgr->notifyrl,
					 notify->zone->task, &e);
	if (result != ISC_R_SUCCESS) {
		isc_event_free(&e);
		notify->event = NULL;
	}
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

	notify->event = NULL;

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

	if (notify->key != NULL) {
		/* Transfer ownership of key */
		key = notify->key;
		notify->key = NULL;
	} else {
		isc_netaddr_fromsockaddr(&dstip, &notify->dst);
		isc_sockaddr_format(&notify->dst, addrbuf, sizeof(addrbuf));
		result = dns_view_getpeertsig(notify->zone->view, &dstip, &key);
		if (result != ISC_R_SUCCESS && result != ISC_R_NOTFOUND) {
			notify_log(notify->zone, ISC_LOG_ERROR,
				   "NOTIFY to %s not sent. "
				   "Peer TSIG key lookup failure.", addrbuf);
			goto cleanup_message;
		}
	}

	/* XXX: should we log the tsig key too? */
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
	if (result == ISC_R_SUCCESS) {
		if (isc_sockaddr_pf(&notify->dst) == AF_INET) {
			inc_stats(notify->zone,
				  dns_zonestatscounter_notifyoutv4);
		} else {
			inc_stats(notify->zone,
				  dns_zonestatscounter_notifyoutv6);
		}
	}

 cleanup_key:
	if (key != NULL)
		dns_tsigkey_detach(&key);
 cleanup_message:
	dns_message_destroy(&message);
 cleanup:
	UNLOCK_ZONE(notify->zone);
	isc_event_free(&event);
	if (result != ISC_R_SUCCESS)
		notify_destroy(notify, ISC_FALSE);
}

static void
notify_send(dns_notify_t *notify) {
	dns_adbaddrinfo_t *ai;
	isc_sockaddr_t dst;
	isc_result_t result;
	dns_notify_t *new = NULL;
	unsigned int flags;
	isc_boolean_t startup;

	/*
	 * Zone lock held by caller.
	 */
	REQUIRE(DNS_NOTIFY_VALID(notify));
	REQUIRE(LOCKED_ZONE(notify->zone));

	for (ai = ISC_LIST_HEAD(notify->find->list);
	     ai != NULL;
	     ai = ISC_LIST_NEXT(ai, publink)) {
		dst = ai->sockaddr;
		if (notify_isqueued(notify->zone, notify->flags, NULL, &dst,
				    NULL))
			continue;
		if (notify_isself(notify->zone, &dst))
			continue;
		new = NULL;
		flags = notify->flags & DNS_NOTIFY_NOSOA;
		result = notify_create(notify->mctx, flags, &new);
		if (result != ISC_R_SUCCESS)
			goto cleanup;
		zone_iattach(notify->zone, &new->zone);
		ISC_LIST_APPEND(new->zone->notifies, new, link);
		new->dst = dst;
		startup = ISC_TF((notify->flags & DNS_NOTIFY_STARTUP) != 0);
		result = notify_send_queue(new, startup);
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
	unsigned int i;
	isc_sockaddr_t dst;
	isc_boolean_t isqueued;
	dns_notifytype_t notifytype;
	unsigned int flags = 0;
	isc_boolean_t loggednotify = ISC_FALSE;
	isc_boolean_t startup;

	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	startup = !DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NEEDNOTIFY);
	DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_NEEDNOTIFY);
	DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_NEEDSTARTUPNOTIFY);
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
	 * Record that this was a notify due to starting up.
	 */
	if (startup)
		flags |= DNS_NOTIFY_STARTUP;

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
		dns_tsigkey_t *key = NULL;
		dns_notify_t *notify = NULL;

		if ((zone->notifykeynames != NULL) &&
		    (zone->notifykeynames[i] != NULL)) {
			dns_view_t *view = dns_zone_getview(zone);
			dns_name_t *keyname = zone->notifykeynames[i];
			(void)dns_view_gettsig(view, keyname, &key);
		}

		dst = zone->notify[i];
		if (notify_isqueued(zone, flags, NULL, &dst, key)) {
			if (key != NULL)
				dns_tsigkey_detach(&key);
			continue;
		}

		result = notify_create(zone->mctx, flags, &notify);
		if (result != ISC_R_SUCCESS) {
			if (key != NULL)
				dns_tsigkey_detach(&key);
			continue;
		}

		zone_iattach(zone, &notify->zone);
		notify->dst = dst;

		INSIST(notify->key == NULL);

		if (key != NULL) {
			notify->key = key;
			key = NULL;
		}

		ISC_LIST_APPEND(zone->notifies, notify, link);
		result = notify_send_queue(notify, startup);
		if (result != ISC_R_SUCCESS)
			notify_destroy(notify, ISC_TRUE);
		if (!loggednotify) {
			notify_log(zone, ISC_LOG_INFO,
				   "sending notifies (serial %u)",
				   serial);
			loggednotify = ISC_TRUE;
		}
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
		dns_notify_t *notify = NULL;

		dns_rdataset_current(&nsrdset, &rdata);
		result = dns_rdata_tostruct(&rdata, &ns, NULL);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);
		dns_rdata_reset(&rdata);
		/*
		 * Don't notify the master server unless explicitly
		 * configured to do so.
		 */
		if (!DNS_ZONE_OPTION(zone, DNS_ZONEOPT_NOTIFYTOSOA) &&
		    dns_name_compare(&master, &ns.name) == 0) {
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
		isqueued = notify_isqueued(zone, flags, &ns.name, NULL, NULL);
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
	isc_uint32_t nscnt, cnamecnt, refresh, retry, expire;
	isc_result_t result;
	isc_time_t now;
	isc_boolean_t exiting = ISC_FALSE;
	isc_interval_t i;
	unsigned int j, soacount;

	stub = revent->ev_arg;
	INSIST(DNS_STUB_VALID(stub));

	UNUSED(task);

	zone = stub->zone;

	ENTER;

	TIME_NOW(&now);

	LOCK_ZONE(zone);

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
			DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_NOEDNS);
			dns_zone_log(zone, ISC_LOG_DEBUG(1),
				     "refreshing stub: timeout retrying "
				     " without EDNS master %s (source %s)",
				     master, source);
			goto same_master;
		}
		dns_zonemgr_unreachableadd(zone->zmgr, &zone->masteraddr,
					   &zone->sourceaddr, &now);
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
			DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_NOEDNS);
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
		DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_USEVC);
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
	result = zone_get_from_db(zone, zone->db, NULL, &soacount, NULL,
				  &refresh, &retry, &expire, NULL, NULL);
	if (result == ISC_R_SUCCESS && soacount > 0U) {
		zone->refresh = RANGE(refresh, zone->minrefresh,
				      zone->maxrefresh);
		zone->retry = RANGE(retry, zone->minretry, zone->maxretry);
		zone->expire = RANGE(expire, zone->refresh + zone->retry,
				     DNS_MAX_EXPIRE);
		DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_HAVETIMERS);
	}
	ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_write);
	dns_db_detach(&stub->db);

	dns_message_destroy(&msg);
	isc_event_free(&event);
	dns_request_destroy(&zone->request);

	DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_REFRESH);
	DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_LOADED);
	DNS_ZONE_JITTER_ADD(&now, zone->refresh, &zone->refreshtime);
	isc_interval_set(&i, zone->expire, 0);
	DNS_ZONE_TIME_ADD(&now, zone->expire, &zone->expiretime);

	if (zone->masterfile != NULL)
		zone_needdump(zone, 0);

	zone_settimer(zone, &now);
	goto free_stub;

 next_master:
	if (stub->version != NULL)
		dns_db_closeversion(stub->db, &stub->version, ISC_FALSE);
	if (stub->db != NULL)
		dns_db_detach(&stub->db);
	if (msg != NULL)
		dns_message_destroy(&msg);
	isc_event_free(&event);
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
			goto free_stub;
		}
	}
	queue_soa_query(zone);
	goto free_stub;

 same_master:
	if (msg != NULL)
		dns_message_destroy(&msg);
	isc_event_free(&event);
	dns_request_destroy(&zone->request);
	ns_query(zone, NULL, stub);
	UNLOCK_ZONE(zone);
	goto done;

 free_stub:
	UNLOCK_ZONE(zone);
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
	isc_uint32_t serial, oldserial = 0;
	unsigned int j;
	isc_boolean_t do_queue_xfrin = ISC_FALSE;

	zone = revent->ev_arg;
	INSIST(DNS_ZONE_VALID(zone));

	UNUSED(task);

	ENTER;

	TIME_NOW(&now);

	LOCK_ZONE(zone);

	if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_EXITING)) {
		isc_event_free(&event);
		dns_request_destroy(&zone->request);
		goto detach;
	}

	/*
	 * if timeout log and next master;
	 */

	isc_sockaddr_format(&zone->masteraddr, master, sizeof(master));
	isc_sockaddr_format(&zone->sourceaddr, source, sizeof(source));

	if (revent->result != ISC_R_SUCCESS) {
		if (revent->result == ISC_R_TIMEDOUT &&
		    !DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NOEDNS)) {
			DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_NOEDNS);
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
			if ((zone->type == dns_zone_slave ||
			     zone->type == dns_zone_redirect) &&
			    DNS_ZONE_OPTION(zone, DNS_ZONEOPT_TRYTCPREFRESH)) {
				if (!dns_zonemgr_unreachable(zone->zmgr,
							     &zone->masteraddr,
							     &zone->sourceaddr,
							     &now))
				{
					DNS_ZONE_SETFLAG(zone,
						     DNS_ZONEFLG_SOABEFOREAXFR);
					goto tcp_transfer;
				}
				dns_zone_log(zone, ISC_LOG_DEBUG(1),
					     "refresh: skipped tcp fallback "
					     "as master %s (source %s) is "
					     "unreachable (cached)",
					      master, source);
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
			DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_NOEDNS);
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
		    (zone->type == dns_zone_slave ||
		     zone->type == dns_zone_redirect))
			goto tcp_transfer;
		goto next_master;
	}

	/*
	 * If truncated punt to zone transfer which will query again.
	 */
	if ((msg->flags & DNS_MESSAGEFLAG_TC) != 0) {
		if (zone->type == dns_zone_slave ||
		    zone->type == dns_zone_redirect) {
			dns_zone_log(zone, ISC_LOG_INFO,
				     "refresh: truncated UDP answer, "
				     "initiating TCP zone xfer "
				     "for master %s (source %s)",
				     master, source);
			DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_SOABEFOREAXFR);
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
			DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_USEVC);
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
	if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_LOADED)) {
		unsigned int dbsoacount;
		result = zone_get_from_db(zone, zone->db, NULL, &dbsoacount,
					  &oldserial, NULL, NULL, NULL, NULL,
					  NULL);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);
		RUNTIME_CHECK(dbsoacount > 0U);
		zone_debuglog(zone, me, 1, "serial: new %u, old %u",
			      serial, oldserial);
	} else
		zone_debuglog(zone, me, 1, "serial: new %u, old not loaded",
			      serial);

	if (!DNS_ZONE_FLAG(zone, DNS_ZONEFLG_LOADED) ||
	    DNS_ZONE_FLAG(zone, DNS_ZONEFLG_FORCEXFER) ||
	    isc_serial_gt(serial, oldserial)) {
		if (dns_zonemgr_unreachable(zone->zmgr, &zone->masteraddr,
					    &zone->sourceaddr, &now))
		{
			dns_zone_log(zone, ISC_LOG_INFO,
				     "refresh: skipping %s as master %s "
				     "(source %s) is unreachable (cached)",
				     (zone->type == dns_zone_slave ||
				      zone->type == dns_zone_redirect) ?
				     "zone transfer" : "NS query",
				     master, source);
			goto next_master;
		}
 tcp_transfer:
		isc_event_free(&event);
		dns_request_destroy(&zone->request);
		if (zone->type == dns_zone_slave ||
		    zone->type == dns_zone_redirect) {
			do_queue_xfrin = ISC_TRUE;
		} else {
			INSIST(zone->type == dns_zone_stub);
			ns_query(zone, rdataset, NULL);
		}
		if (msg != NULL)
			dns_message_destroy(&msg);
	} else if (isc_serial_eq(soa.serial, oldserial)) {
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
				zone_needdump(zone, DNS_DUMP_DELAY);
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
				     soa.serial, master, oldserial);
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
		goto detach;
	}

 requeue:
	queue_soa_query(zone);
	goto detach;

 same_master:
	if (msg != NULL)
		dns_message_destroy(&msg);
	isc_event_free(&event);
	dns_request_destroy(&zone->request);
	queue_soa_query(zone);

 detach:
	UNLOCK_ZONE(zone);
	if (do_queue_xfrin)
		queue_xfrin(zone);
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
	result = isc_ratelimiter_enqueue(zone->zmgr->refreshrl, zone->task, &e);
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
add_opt(dns_message_t *message, isc_uint16_t udpsize, isc_boolean_t reqnsid) {
	isc_result_t result;
	dns_rdataset_t *rdataset = NULL;
	dns_ednsopt_t ednsopts[DNS_EDNSOPTIONS];
	int count = 0;

	/* Set EDNS options if applicable */
	if (reqnsid) {
		INSIST(count < DNS_EDNSOPTIONS);
		ednsopts[count].code = DNS_OPT_NSID;
		ednsopts[count].length = 0;
		ednsopts[count].value = NULL;
		count++;
	}
	result = dns_message_buildopt(message, &rdataset, 0, udpsize, 0,
				      ednsopts, count);
	if (result != ISC_R_SUCCESS)
		return (result);

	return (dns_message_setopt(message, rdataset));
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
	isc_boolean_t have_xfrsource, reqnsid;
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

 again:
	result = create_query(zone, dns_rdatatype_soa, &message);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

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
			goto skip_master;
		}
	}
	if (key == NULL) {
		result = dns_view_getpeertsig(zone->view, &masterip, &key);
		if (result != ISC_R_SUCCESS && result != ISC_R_NOTFOUND) {
			char addrbuf[ISC_NETADDR_FORMATSIZE];
			isc_netaddr_format(&masterip, addrbuf, sizeof(addrbuf));
			dns_zone_log(zone, ISC_LOG_ERROR,
				     "unable to find TSIG key for %s", addrbuf);
			goto skip_master;
		}
	}

	have_xfrsource = ISC_FALSE;
	reqnsid = zone->view->requestnsid;
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
			(void)dns_peer_getrequestnsid(peer, &reqnsid);
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
		result = add_opt(message, udpsize, reqnsid);
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
			      "dns_request_createvia4() failed: %s",
			      dns_result_totext(result));
		goto skip_master;
	} else {
		if (isc_sockaddr_pf(&zone->masteraddr) == PF_INET)
			inc_stats(zone, dns_zonestatscounter_soaoutv4);
		else
			inc_stats(zone, dns_zonestatscounter_soaoutv6);
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
	dns_message_destroy(&message);
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
	isc_boolean_t have_xfrsource = ISC_FALSE, reqnsid;
	isc_uint16_t udpsize = SEND_BUFFER_SIZE;

	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(LOCKED_ZONE(zone));
	REQUIRE((soardataset != NULL && stub == NULL) ||
		(soardataset == NULL && stub != NULL));
	REQUIRE(stub == NULL || DNS_STUB_VALID(stub));

	ENTER;

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

		result = dns_db_newversion(stub->db, &stub->version);
		if (result != ISC_R_SUCCESS) {
			dns_zone_log(zone, ISC_LOG_INFO, "refreshing stub: "
				     "dns_db_newversion() failed: %s",
				     dns_result_totext(result));
			goto cleanup;
		}

		/*
		 * Update SOA record.
		 */
		result = dns_db_findnode(stub->db, &zone->origin, ISC_TRUE,
					 &node);
		if (result != ISC_R_SUCCESS) {
			dns_zone_log(zone, ISC_LOG_INFO, "refreshing stub: "
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
	INSIST(result == ISC_R_SUCCESS);

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

	reqnsid = zone->view->requestnsid;
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
			(void)dns_peer_getrequestnsid(peer, &reqnsid);
		}

	}
	if (!DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NOEDNS)) {
		result = add_opt(message, udpsize, reqnsid);
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
		POST(result);
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
	dns_zone_t *raw = NULL, *secure = NULL;

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
		if (zone->statelist == &zone->zmgr->xfrin_in_progress) {
			ISC_LIST_UNLINK(zone->zmgr->xfrin_in_progress, zone,
					statelink);
			zone->statelist = NULL;
			zmgr_resume_xfrs(zone->zmgr, ISC_FALSE);
		}
		RWUNLOCK(&zone->zmgr->rwlock, isc_rwlocktype_write);
	}

	/*
	 * In task context, no locking required.  See zone_xfrdone().
	 */
	if (zone->xfr != NULL)
		dns_xfrin_shutdown(zone->xfr);

	/* Safe to release the zone now */
	if (zone->zmgr != NULL)
		dns_zonemgr_releasezone(zone->zmgr, zone);

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

	forward_cancel(zone);

	if (zone->timer != NULL) {
		isc_timer_detach(&zone->timer);
		INSIST(zone->irefs > 0);
		zone->irefs--;
	}

	if (zone->view != NULL)
		dns_view_weakdetach(&zone->view);

	/*
	 * We have now canceled everything set the flag to allow exit_check()
	 * to succeed.	We must not unlock between setting this flag and
	 * calling exit_check().
	 */
	DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_SHUTDOWN);
	free_needed = exit_check(zone);
	if (inline_secure(zone)) {
		raw = zone->raw;
		zone->raw = NULL;
	}
	if (inline_raw(zone)) {
		secure = zone->secure;
		zone->secure = NULL;
	}
	UNLOCK_ZONE(zone);
	if (raw != NULL)
		dns_zone_detach(&raw);
	if (secure != NULL)
		dns_zone_idetach(&secure);
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

	ENTER;
	REQUIRE(DNS_ZONE_VALID(zone));
	if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_EXITING))
		return;

	isc_time_settoepoch(&next);

	switch (zone->type) {
	case dns_zone_redirect:
		if (zone->masters != NULL)
			goto treat_as_slave;
		/* FALLTHROUGH */

	case dns_zone_master:
		if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NEEDNOTIFY) ||
		    DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NEEDSTARTUPNOTIFY))
			next = zone->notifytime;
		if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NEEDDUMP) &&
		    !DNS_ZONE_FLAG(zone, DNS_ZONEFLG_DUMPING)) {
			INSIST(!isc_time_isepoch(&zone->dumptime));
			if (isc_time_isepoch(&next) ||
			    isc_time_compare(&zone->dumptime, &next) < 0)
				next = zone->dumptime;
		}
		if (zone->type == dns_zone_redirect)
			break;
		if (!DNS_ZONE_FLAG(zone, DNS_ZONEFLG_REFRESHING) &&
		    !isc_time_isepoch(&zone->refreshkeytime)) {
			if (isc_time_isepoch(&next) ||
			    isc_time_compare(&zone->refreshkeytime, &next) < 0)
				next = zone->refreshkeytime;
		}
		if (!isc_time_isepoch(&zone->resigntime)) {
			if (isc_time_isepoch(&next) ||
			    isc_time_compare(&zone->resigntime, &next) < 0)
				next = zone->resigntime;
		}
		if (!isc_time_isepoch(&zone->keywarntime)) {
			if (isc_time_isepoch(&next) ||
			    isc_time_compare(&zone->keywarntime, &next) < 0)
				next = zone->keywarntime;
		}
		if (!isc_time_isepoch(&zone->signingtime)) {
			if (isc_time_isepoch(&next) ||
			    isc_time_compare(&zone->signingtime, &next) < 0)
				next = zone->signingtime;
		}
		if (!isc_time_isepoch(&zone->nsec3chaintime)) {
			if (isc_time_isepoch(&next) ||
			    isc_time_compare(&zone->nsec3chaintime, &next) < 0)
				next = zone->nsec3chaintime;
		}
		break;

	case dns_zone_slave:
	treat_as_slave:
		if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NEEDNOTIFY) ||
		    DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NEEDSTARTUPNOTIFY))
			next = zone->notifytime;
		/* FALLTHROUGH */

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
		if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_LOADED) &&
		    !isc_time_isepoch(&zone->expiretime)) {
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

	case dns_zone_key:
		if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NEEDDUMP) &&
		    !DNS_ZONE_FLAG(zone, DNS_ZONEFLG_DUMPING)) {
			INSIST(!isc_time_isepoch(&zone->dumptime));
			if (isc_time_isepoch(&next) ||
			    isc_time_compare(&zone->dumptime, &next) < 0)
				next = zone->dumptime;
		}
		if (!DNS_ZONE_FLAG(zone, DNS_ZONEFLG_REFRESHING)) {
			if (isc_time_isepoch(&next) ||
			    (!isc_time_isepoch(&zone->refreshkeytime) &&
			    isc_time_compare(&zone->refreshkeytime, &next) < 0))
				next = zone->refreshkeytime;
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
	temprdatalist->ttl = rdataset.ttl;
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
	isc_sockaddr_t local, remote;
	dns_tsigkey_t *tsigkey;
	dns_name_t *tsig;

	REQUIRE(DNS_ZONE_VALID(zone));

	/*
	 * If type != T_SOA return DNS_R_NOTIMP.  We don't yet support
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
	 * Notify messages are processed by the raw zone.
	 */
	LOCK_ZONE(zone);
	if (inline_secure(zone)) {
		result = dns_zone_notifyreceive(zone->raw, from, msg);
		UNLOCK_ZONE(zone);
		return (result);
	}
	/*
	 *  We only handle NOTIFY (SOA) at the present.
	 */
	if (isc_sockaddr_pf(from) == PF_INET)
		inc_stats(zone, dns_zonestatscounter_notifyinv4);
	else
		inc_stats(zone, dns_zonestatscounter_notifyinv6);
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
	tsigkey = dns_message_gettsigkey(msg);
	tsig = dns_tsigkey_identity(tsigkey);
	if (i >= zone->masterscnt && zone->notify_acl != NULL &&
	    dns_acl_match(&netaddr, tsig, zone->notify_acl,
			  &zone->view->aclenv,
			  &match, NULL) == ISC_R_SUCCESS &&
	    match > 0)
	{
		/* Accept notify. */
	} else if (i >= zone->masterscnt) {
		UNLOCK_ZONE(zone);
		dns_zone_log(zone, ISC_LOG_INFO,
			     "refused notify from non-master: %s", fromtext);
		inc_stats(zone, dns_zonestatscounter_notifyrej);
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
			isc_uint32_t serial = 0, oldserial;
			unsigned int soacount;

			dns_rdataset_current(rdataset, &rdata);
			result = dns_rdata_tostruct(&rdata, &soa, NULL);
			RUNTIME_CHECK(result == ISC_R_SUCCESS);
			serial = soa.serial;
			/*
			 * The following should safely be performed without DB
			 * lock and succeed in this context.
			 */
			result = zone_get_from_db(zone, zone->db, NULL,
						  &soacount, &oldserial, NULL,
						  NULL, NULL, NULL, NULL);
			RUNTIME_CHECK(result == ISC_R_SUCCESS);
			RUNTIME_CHECK(soacount > 0U);
			if (isc_serial_le(serial, oldserial)) {
				dns_zone_log(zone,
					     ISC_LOG_INFO,
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
	remote = zone->masteraddr;
	local = zone->sourceaddr;
	UNLOCK_ZONE(zone);
	dns_zonemgr_unreachabledel(zone->zmgr, &remote, &local);
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
dns_zone_setqueryonacl(dns_zone_t *zone, dns_acl_t *acl) {

	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	if (zone->queryon_acl != NULL)
		dns_acl_detach(&zone->queryon_acl);
	dns_acl_attach(acl, &zone->queryon_acl);
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
dns_zone_getqueryonacl(dns_zone_t *zone) {

	REQUIRE(DNS_ZONE_VALID(zone));

	return (zone->queryon_acl);
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
dns_zone_clearqueryonacl(dns_zone_t *zone) {

	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	if (zone->queryon_acl != NULL)
		dns_acl_detach(&zone->queryon_acl);
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
zone_namerd_tostr(dns_zone_t *zone, char *buf, size_t length) {
	isc_result_t result = ISC_R_FAILURE;
	isc_buffer_t buffer;

	REQUIRE(buf != NULL);
	REQUIRE(length > 1U);

	/*
	 * Leave space for terminating '\0'.
	 */
	isc_buffer_init(&buffer, buf, (unsigned int)length - 1);
	if (zone->type != dns_zone_redirect && zone->type != dns_zone_key) {
		if (dns_name_dynamic(&zone->origin))
			result = dns_name_totext(&zone->origin, ISC_TRUE, &buffer);
		if (result != ISC_R_SUCCESS &&
		    isc_buffer_availablelength(&buffer) >= (sizeof("<UNKNOWN>") - 1))
			isc_buffer_putstr(&buffer, "<UNKNOWN>");

		if (isc_buffer_availablelength(&buffer) > 0)
			isc_buffer_putstr(&buffer, "/");
		(void)dns_rdataclass_totext(zone->rdclass, &buffer);
	}

	if (zone->view != NULL && strcmp(zone->view->name, "_bind") != 0 &&
	    strcmp(zone->view->name, "_default") != 0 &&
	    strlen(zone->view->name) < isc_buffer_availablelength(&buffer)) {
		isc_buffer_putstr(&buffer, "/");
		isc_buffer_putstr(&buffer, zone->view->name);
	}
	if (inline_secure(zone) && 9U < isc_buffer_availablelength(&buffer))
		isc_buffer_putstr(&buffer, " (signed)");
	if (inline_raw(zone) && 11U < isc_buffer_availablelength(&buffer))
		isc_buffer_putstr(&buffer, " (unsigned)");

	buf[isc_buffer_usedlength(&buffer)] = '\0';
}

static void
zone_name_tostr(dns_zone_t *zone, char *buf, size_t length) {
	isc_result_t result = ISC_R_FAILURE;
	isc_buffer_t buffer;

	REQUIRE(buf != NULL);
	REQUIRE(length > 1U);

	/*
	 * Leave space for terminating '\0'.
	 */
	isc_buffer_init(&buffer, buf, (unsigned int)length - 1);
	if (dns_name_dynamic(&zone->origin))
		result = dns_name_totext(&zone->origin, ISC_TRUE, &buffer);
	if (result != ISC_R_SUCCESS &&
	    isc_buffer_availablelength(&buffer) >= (sizeof("<UNKNOWN>") - 1))
		isc_buffer_putstr(&buffer, "<UNKNOWN>");

	buf[isc_buffer_usedlength(&buffer)] = '\0';
}

static void
zone_rdclass_tostr(dns_zone_t *zone, char *buf, size_t length) {
	isc_buffer_t buffer;

	REQUIRE(buf != NULL);
	REQUIRE(length > 1U);

	/*
	 * Leave space for terminating '\0'.
	 */
	isc_buffer_init(&buffer, buf, (unsigned int)length - 1);
	(void)dns_rdataclass_totext(zone->rdclass, &buffer);

	buf[isc_buffer_usedlength(&buffer)] = '\0';
}

static void
zone_viewname_tostr(dns_zone_t *zone, char *buf, size_t length) {
	isc_buffer_t buffer;

	REQUIRE(buf != NULL);
	REQUIRE(length > 1U);


	/*
	 * Leave space for terminating '\0'.
	 */
	isc_buffer_init(&buffer, buf, (unsigned int)length - 1);

	if (zone->view == NULL) {
		isc_buffer_putstr(&buffer, "_none");
	} else if (strlen(zone->view->name)
		   < isc_buffer_availablelength(&buffer)) {
		isc_buffer_putstr(&buffer, zone->view->name);
	} else {
		isc_buffer_putstr(&buffer, "_toolong");
	}

	buf[isc_buffer_usedlength(&buffer)] = '\0';
}

void
dns_zone_name(dns_zone_t *zone, char *buf, size_t length) {
	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(buf != NULL);
	zone_namerd_tostr(zone, buf, length);
}

void
dns_zone_nameonly(dns_zone_t *zone, char *buf, size_t length) {
	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(buf != NULL);
	zone_name_tostr(zone, buf, length);
}

static void
notify_log(dns_zone_t *zone, int level, const char *fmt, ...) {
	va_list ap;
	char message[4096];

	if (isc_log_wouldlog(dns_lctx, level) == ISC_FALSE)
		return;

	va_start(ap, fmt);
	vsnprintf(message, sizeof(message), fmt, ap);
	va_end(ap);
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_NOTIFY, DNS_LOGMODULE_ZONE,
		      level, "zone %s: %s", zone->strnamerd, message);
}

void
dns_zone_logc(dns_zone_t *zone, isc_logcategory_t *category,
	      int level, const char *fmt, ...) {
	va_list ap;
	char message[4096];

	if (isc_log_wouldlog(dns_lctx, level) == ISC_FALSE)
		return;

	va_start(ap, fmt);
	vsnprintf(message, sizeof(message), fmt, ap);
	va_end(ap);
	isc_log_write(dns_lctx, category, DNS_LOGMODULE_ZONE,
		      level, "%s%s: %s", (zone->type == dns_zone_key) ?
		      "managed-keys-zone" : (zone->type == dns_zone_redirect) ?
		      "redirect-zone" : "zone ", zone->strnamerd, message);
}

void
dns_zone_log(dns_zone_t *zone, int level, const char *fmt, ...) {
	va_list ap;
	char message[4096];

	if (isc_log_wouldlog(dns_lctx, level) == ISC_FALSE)
		return;

	va_start(ap, fmt);
	vsnprintf(message, sizeof(message), fmt, ap);
	va_end(ap);
	isc_log_write(dns_lctx, DNS_LOGCATEGORY_GENERAL, DNS_LOGMODULE_ZONE,
		      level, "%s%s: %s", (zone->type == dns_zone_key) ?
		      "managed-keys-zone" : (zone->type == dns_zone_redirect) ?
		      "redirect-zone" : "zone ", zone->strnamerd, message);
}

static void
zone_debuglog(dns_zone_t *zone, const char *me, int debuglevel,
	      const char *fmt, ...)
{
	va_list ap;
	char message[4096];
	int level = ISC_LOG_DEBUG(debuglevel);
	const char *zstr;

	if (isc_log_wouldlog(dns_lctx, level) == ISC_FALSE)
		return;

	va_start(ap, fmt);
	vsnprintf(message, sizeof(message), fmt, ap);
	va_end(ap);

	switch (zone->type) {
	case dns_zone_key:
		zstr = "managed-keys-zone";
		break;
	case dns_zone_redirect:
		zstr = "redirect-zone";
		break;
	default:
		zstr = "zone";
	}

	isc_log_write(dns_lctx, DNS_LOGCATEGORY_GENERAL, DNS_LOGMODULE_ZONE,
		      level, "%s: %s %s: %s", me, zstr, zone->strnamerd,
		      message);
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
	 * Old bind's return formerr if they see a soa record.	Retry w/o
	 * the soa if we see a formerr and had sent a SOA.
	 */
	isc_event_free(&event);
	if (message != NULL && message->rcode == dns_rcode_formerr &&
	    (notify->flags & DNS_NOTIFY_NOSOA) == 0) {
		isc_boolean_t startup;

		notify->flags |= DNS_NOTIFY_NOSOA;
		dns_request_destroy(&notify->request);
		startup = ISC_TF((notify->flags & DNS_NOTIFY_STARTUP) != 0);
		result = notify_send_queue(notify, startup);
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

struct secure_event {
	isc_event_t e;
	dns_db_t *db;
	isc_uint32_t serial;
};

static void
update_log_cb(void *arg, dns_zone_t *zone, int level, const char *message) {
	UNUSED(arg);
	dns_zone_log(zone, level, "%s", message);
}

static isc_result_t
sync_secure_journal(dns_zone_t *zone, dns_zone_t *raw, dns_journal_t *journal,
		    isc_uint32_t start, isc_uint32_t end,
		    dns_difftuple_t **soatuplep, dns_diff_t *diff)
{
	isc_result_t result;
	dns_difftuple_t *tuple = NULL;
	dns_diffop_t op = DNS_DIFFOP_ADD;
	int n_soa = 0;

	REQUIRE(soatuplep != NULL);

	if (start == end)
		return (DNS_R_UNCHANGED);

	CHECK(dns_journal_iter_init(journal, start, end));
	for (result = dns_journal_first_rr(journal);
	     result == ISC_R_SUCCESS;
	     result = dns_journal_next_rr(journal))
	{
		dns_name_t *name = NULL;
		isc_uint32_t ttl;
		dns_rdata_t *rdata = NULL;
		dns_journal_current_rr(journal, &name, &ttl, &rdata);

		if (rdata->type == dns_rdatatype_soa) {
			n_soa++;
			if (n_soa == 2) {
				/*
				 * Save the latest raw SOA record.
				 */
				if (*soatuplep != NULL)
					dns_difftuple_free(soatuplep);
				CHECK(dns_difftuple_create(diff->mctx,
							   DNS_DIFFOP_ADD,
							   name, ttl, rdata,
							   soatuplep));
			}
			if (n_soa == 3)
				n_soa = 1;
			continue;
		}

		/* Sanity. */
		if (n_soa == 0) {
			dns_zone_log(raw, ISC_LOG_ERROR,
				     "corrupt journal file: '%s'\n",
				     raw->journal);
			return (ISC_R_FAILURE);
		}

		if (zone->privatetype != 0 &&
		    rdata->type == zone->privatetype)
			continue;

		if (rdata->type == dns_rdatatype_nsec ||
		    rdata->type == dns_rdatatype_rrsig ||
		    rdata->type == dns_rdatatype_nsec3 ||
		    rdata->type == dns_rdatatype_dnskey ||
		    rdata->type == dns_rdatatype_nsec3param)
			continue;

		op = (n_soa == 1) ? DNS_DIFFOP_DEL : DNS_DIFFOP_ADD;

		CHECK(dns_difftuple_create(diff->mctx, op, name, ttl, rdata,
					   &tuple));
		dns_diff_appendminimal(diff, &tuple);
	}
	if (result == ISC_R_NOMORE)
		result = ISC_R_SUCCESS;

 failure:
	return(result);
}

static isc_result_t
sync_secure_db(dns_zone_t *seczone, dns_zone_t *raw, dns_db_t *secdb,
	       dns_dbversion_t *secver, dns_difftuple_t **soatuple,
	       dns_diff_t *diff)
{
	isc_result_t result;
	dns_db_t *rawdb = NULL;
	dns_dbversion_t *rawver = NULL;
	dns_difftuple_t *tuple = NULL, *next;
	dns_difftuple_t *oldtuple = NULL, *newtuple = NULL;
	dns_rdata_soa_t oldsoa, newsoa;

	REQUIRE(DNS_ZONE_VALID(seczone));
	REQUIRE(soatuple != NULL && *soatuple == NULL);

	if (!seczone->sourceserialset)
		return (DNS_R_UNCHANGED);

	dns_db_attach(raw->db, &rawdb);
	dns_db_currentversion(rawdb, &rawver);
	result = dns_db_diffx(diff, rawdb, rawver, secdb, secver, NULL);
	dns_db_closeversion(rawdb, &rawver, ISC_FALSE);
	dns_db_detach(&rawdb);

	if (result != ISC_R_SUCCESS)
		return (result);

	for (tuple = ISC_LIST_HEAD(diff->tuples);
	     tuple != NULL;
	     tuple = next)
	{
		next = ISC_LIST_NEXT(tuple, link);
		if (tuple->rdata.type == dns_rdatatype_nsec ||
		    tuple->rdata.type == dns_rdatatype_rrsig ||
		    tuple->rdata.type == dns_rdatatype_dnskey ||
		    tuple->rdata.type == dns_rdatatype_nsec3 ||
		    tuple->rdata.type == dns_rdatatype_nsec3param)
		{
			ISC_LIST_UNLINK(diff->tuples, tuple, link);
			dns_difftuple_free(&tuple);
			continue;
		}
		if (tuple->rdata.type == dns_rdatatype_soa) {
			if (tuple->op == DNS_DIFFOP_DEL) {
				INSIST(oldtuple == NULL);
				oldtuple = tuple;
			}
			if (tuple->op == DNS_DIFFOP_ADD) {
				INSIST(newtuple == NULL);
				newtuple = tuple;
			}
		}
	}

	if (oldtuple != NULL && newtuple != NULL) {

		result = dns_rdata_tostruct(&oldtuple->rdata, &oldsoa, NULL);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);

		result = dns_rdata_tostruct(&newtuple->rdata, &newsoa, NULL);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);

		/*
		 * If the SOA records are the same except for the serial
		 * remove them from the diff.
		 */
		if (oldsoa.refresh == newsoa.refresh &&
		    oldsoa.retry == newsoa.retry &&
		    oldsoa.minimum == newsoa.minimum &&
		    oldsoa.expire == newsoa.expire &&
		    dns_name_equal(&oldsoa.origin, &newsoa.origin) &&
		    dns_name_equal(&oldsoa.contact, &newsoa.contact)) {
			ISC_LIST_UNLINK(diff->tuples, oldtuple, link);
			dns_difftuple_free(&oldtuple);
			ISC_LIST_UNLINK(diff->tuples, newtuple, link);
			dns_difftuple_free(&newtuple);
		}
	}

	if (ISC_LIST_EMPTY(diff->tuples))
		return (DNS_R_UNCHANGED);

	/*
	 * If there are still SOA records in the diff they can now be removed
	 * saving the new SOA record.
	 */
	if (oldtuple != NULL) {
		ISC_LIST_UNLINK(diff->tuples, oldtuple, link);
		dns_difftuple_free(&oldtuple);
	}

	if (newtuple != NULL) {
		ISC_LIST_UNLINK(diff->tuples, newtuple, link);
		*soatuple = newtuple;
	}

	return (ISC_R_SUCCESS);
}

static void
receive_secure_serial(isc_task_t *task, isc_event_t *event) {
	static char me[] = "receive_secure_serial";
	isc_result_t result = ISC_R_SUCCESS;
	dns_journal_t *rjournal = NULL;
	dns_journal_t *sjournal = NULL;
	isc_uint32_t start, end;
	dns_zone_t *zone;
	dns_difftuple_t *tuple = NULL, *soatuple = NULL;
	dns_update_log_t log = { update_log_cb, NULL };
	isc_time_t timenow;

	UNUSED(task);

	zone = event->ev_arg;
	end = ((struct secure_event *)event)->serial;

	ENTER;

	LOCK_ZONE(zone);

	/*
	 * If we are already processing a receive secure serial event
	 * for the zone, just queue the new one and exit.
	 */
	if (zone->rss_event != NULL && zone->rss_event != event) {
		ISC_LIST_APPEND(zone->rss_events, event, ev_link);
		UNLOCK_ZONE(zone);
		return;
	}

 nextevent:
	if (zone->rss_event != NULL) {
		INSIST(zone->rss_event == event);
		UNLOCK_ZONE(zone);
	} else {
		zone->rss_event = event;
		dns_diff_init(zone->mctx, &zone->rss_diff);

		/*
		 * zone->db may be NULL, if the load from disk failed.
		 */
		result = ISC_R_SUCCESS;
		ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_read);
		if (zone->db != NULL)
			dns_db_attach(zone->db, &zone->rss_db);
		else
			result = ISC_R_FAILURE;
		ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);

		if (result == ISC_R_SUCCESS && zone->raw != NULL)
			dns_zone_attach(zone->raw, &zone->rss_raw);
		else
			result = ISC_R_FAILURE;

		UNLOCK_ZONE(zone);

		CHECK(result);

		/*
		 * We first attempt to sync the raw zone to the secure zone
		 * by using the raw zone's journal, applying all the deltas
		 * from the latest source-serial of the secure zone up to
		 * the current serial number of the raw zone.
		 *
		 * If that fails, then we'll fall back to a direct comparison
		 * between raw and secure zones.
		 */
		CHECK(dns_journal_open(zone->rss_raw->mctx,
				       zone->rss_raw->journal,
				       DNS_JOURNAL_WRITE, &rjournal));

		result = dns_journal_open(zone->mctx, zone->journal,
					  DNS_JOURNAL_READ, &sjournal);
		if (result != ISC_R_SUCCESS && result != ISC_R_NOTFOUND)
			goto failure;

		if (!dns_journal_get_sourceserial(rjournal, &start)) {
			start = dns_journal_first_serial(rjournal);
			dns_journal_set_sourceserial(rjournal, start);
		}
		if (sjournal != NULL) {
			isc_uint32_t serial;
			/*
			 * We read the secure journal first, if that
			 * exists use its value provided it is greater
			 * that from the raw journal.
			 */
			if (dns_journal_get_sourceserial(sjournal, &serial)) {
				if (isc_serial_gt(serial, start))
					start = serial;
			}
			dns_journal_destroy(&sjournal);
		}

		dns_db_currentversion(zone->rss_db, &zone->rss_oldver);
		CHECK(dns_db_newversion(zone->rss_db, &zone->rss_newver));

		/*
		 * Try to apply diffs from the raw zone's journal to the secure
		 * zone.  If that fails, we recover by syncing up the databases
		 * directly.
		 */
		result = sync_secure_journal(zone, zone->rss_raw, rjournal,
					     start, end, &soatuple,
					     &zone->rss_diff);
		if (result == DNS_R_UNCHANGED)
			goto failure;
		else if (result != ISC_R_SUCCESS)
			CHECK(sync_secure_db(zone, zone->rss_raw, zone->rss_db,
					     zone->rss_oldver, &soatuple,
					     &zone->rss_diff));

		CHECK(dns_diff_apply(&zone->rss_diff, zone->rss_db,
				     zone->rss_newver));

		if (soatuple != NULL) {
			isc_uint32_t oldserial, newserial, desired;

			CHECK(dns_db_createsoatuple(zone->rss_db,
						    zone->rss_oldver,
						    zone->rss_diff.mctx,
						    DNS_DIFFOP_DEL, &tuple));
			oldserial = dns_soa_getserial(&tuple->rdata);
			newserial = desired =
				    dns_soa_getserial(&soatuple->rdata);
			if (!isc_serial_gt(newserial, oldserial)) {
				newserial = oldserial + 1;
				if (newserial == 0)
					newserial++;
				dns_soa_setserial(newserial, &soatuple->rdata);
			}
			CHECK(do_one_tuple(&tuple, zone->rss_db,
					   zone->rss_newver, &zone->rss_diff));
			CHECK(do_one_tuple(&soatuple, zone->rss_db,
					   zone->rss_newver, &zone->rss_diff));
			dns_zone_log(zone, ISC_LOG_INFO,
				     "serial %u (unsigned %u)",
				     newserial, desired);
		} else
			CHECK(update_soa_serial(zone->rss_db, zone->rss_newver,
						&zone->rss_diff, zone->mctx,
						zone->updatemethod));

	}
	result = dns_update_signaturesinc(&log, zone, zone->rss_db,
					  zone->rss_oldver, zone->rss_newver,
					  &zone->rss_diff,
					  zone->sigvalidityinterval,
					  &zone->rss_state);
	if (result == DNS_R_CONTINUE) {
		if (rjournal != NULL)
			dns_journal_destroy(&rjournal);
		isc_task_send(task, &event);
		fprintf(stderr, "looping on dns_update_signaturesinc\n");
		return;
	}
	if (result != ISC_R_SUCCESS)
		goto failure;

	if (rjournal == NULL)
		CHECK(dns_journal_open(zone->rss_raw->mctx,
				       zone->rss_raw->journal,
				       DNS_JOURNAL_WRITE, &rjournal));
	CHECK(zone_journal(zone, &zone->rss_diff, &end,
			   "receive_secure_serial"));

	dns_journal_set_sourceserial(rjournal, end);
	dns_journal_commit(rjournal);

	LOCK_ZONE(zone);
	DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_NEEDNOTIFY);

	zone->sourceserial = end;
	zone->sourceserialset = ISC_TRUE;
	zone_needdump(zone, DNS_DUMP_DELAY);

	TIME_NOW(&timenow);
	zone_settimer(zone, &timenow);
	UNLOCK_ZONE(zone);

	dns_db_closeversion(zone->rss_db, &zone->rss_oldver, ISC_FALSE);
	dns_db_closeversion(zone->rss_db, &zone->rss_newver, ISC_TRUE);

 failure:
	isc_event_free(&zone->rss_event);
	event = ISC_LIST_HEAD(zone->rss_events);

	if (zone->rss_raw != NULL)
		dns_zone_detach(&zone->rss_raw);
	if (result != ISC_R_SUCCESS)
		dns_zone_log(zone, ISC_LOG_ERROR, "receive_secure_serial: %s",
			     dns_result_totext(result));
	if (tuple != NULL)
		dns_difftuple_free(&tuple);
	if (soatuple != NULL)
		dns_difftuple_free(&soatuple);
	if (zone->rss_db != NULL) {
		if (zone->rss_oldver != NULL)
			dns_db_closeversion(zone->rss_db, &zone->rss_oldver,
					    ISC_FALSE);
		if (zone->rss_newver != NULL)
			dns_db_closeversion(zone->rss_db, &zone->rss_newver,
					    ISC_FALSE);
		dns_db_detach(&zone->rss_db);
	}
	INSIST(zone->rss_oldver == NULL);
	INSIST(zone->rss_newver == NULL);
	if (rjournal != NULL)
		dns_journal_destroy(&rjournal);
	dns_diff_clear(&zone->rss_diff);

	if (event != NULL) {
		LOCK_ZONE(zone);
		INSIST(zone->irefs > 1);
		zone->irefs--;
		goto nextevent;
	}
	dns_zone_idetach(&zone);
}

static isc_result_t
zone_send_secureserial(dns_zone_t *zone, isc_uint32_t serial) {
	isc_event_t *e;
	dns_zone_t *dummy = NULL;

	e = isc_event_allocate(zone->secure->mctx, zone,
			       DNS_EVENT_ZONESECURESERIAL,
			       receive_secure_serial, zone->secure,
			       sizeof(struct secure_event));
	if (e == NULL)
		return (ISC_R_NOMEMORY);
	((struct secure_event *)e)->serial = serial;
	INSIST(LOCKED_ZONE(zone->secure));
	zone_iattach(zone->secure, &dummy);
	isc_task_send(zone->secure->task, &e);

	DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_SENDSECURE);
	return (ISC_R_SUCCESS);
}

static isc_result_t
checkandaddsoa(dns_db_t *db, dns_dbnode_t *node, dns_dbversion_t *version,
	       dns_rdataset_t *rdataset, isc_uint32_t oldserial)
{
	dns_rdata_soa_t soa;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdatalist_t temprdatalist;
	dns_rdataset_t temprdataset;
	isc_buffer_t b;
	isc_result_t result;
	unsigned char buf[DNS_SOA_BUFFERSIZE];

	result = dns_rdataset_first(rdataset);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);
	dns_rdataset_current(rdataset, &rdata);
	result = dns_rdata_tostruct(&rdata, &soa, NULL);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);

	if (isc_serial_gt(soa.serial, oldserial))
		return (dns_db_addrdataset(db, node, version, 0, rdataset, 0,
					   NULL));
	/*
	 * Always bump the serial.
	 */
	oldserial++;
	if (oldserial == 0)
		oldserial++;
	soa.serial = oldserial;

	/*
	 * Construct a replacement rdataset.
	 */
	dns_rdata_reset(&rdata);
	isc_buffer_init(&b, buf, sizeof(buf));
	result = dns_rdata_fromstruct(&rdata, rdataset->rdclass,
				      dns_rdatatype_soa, &soa, &b);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);
	dns_rdatalist_init(&temprdatalist);
	temprdatalist.rdclass = rdata.rdclass;
	temprdatalist.type = rdata.type;
	temprdatalist.ttl = rdataset->ttl;
	ISC_LIST_APPEND(temprdatalist.rdata, &rdata, link);

	dns_rdataset_init(&temprdataset);
	result = dns_rdatalist_tordataset(&temprdatalist, &temprdataset);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);
	return (dns_db_addrdataset(db, node, version, 0, &temprdataset,
				   0, NULL));
}

/*
 * This function should populate an nsec3paramlist_t with the
 * nsecparam_t data from a zone.
 */
static isc_result_t
save_nsec3param(dns_zone_t *zone, nsec3paramlist_t *nsec3list) {
	isc_result_t result;
	dns_dbnode_t *node = NULL;
	dns_rdataset_t rdataset, prdataset;
	dns_dbversion_t *version = NULL;
	nsec3param_t *nsec3param = NULL;
	nsec3param_t *nsec3p = NULL;
	nsec3param_t *next;
	dns_db_t *db = NULL;
	unsigned char buf[DNS_NSEC3PARAM_BUFFERSIZE];

	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(nsec3list != NULL);
	REQUIRE(ISC_LIST_EMPTY(*nsec3list));

	dns_rdataset_init(&rdataset);
	dns_rdataset_init(&prdataset);

	dns_db_attach(zone->db, &db);
	CHECK(dns_db_getoriginnode(db, &node));

	dns_db_currentversion(db, &version);
	result = dns_db_findrdataset(db, node, version,
				     dns_rdatatype_nsec3param,
				     dns_rdatatype_none, 0, &rdataset, NULL);

	if (result != ISC_R_SUCCESS)
		goto getprivate;

	/*
	 * walk nsec3param rdataset making a list of parameters (note that
	 * multiple simultaneous nsec3 chains are annoyingly legal -- this
	 * is why we use an nsec3list, even tho we will usually only have
	 * one)
	 */
	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset))
	{
		dns_rdata_t rdata = DNS_RDATA_INIT;
		dns_rdata_t private = DNS_RDATA_INIT;

		dns_rdataset_current(&rdataset, &rdata);
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_GENERAL,
			      DNS_LOGMODULE_ZONE, ISC_LOG_DEBUG(3),
			      "looping through nsec3param data");
		nsec3param = isc_mem_get(zone->mctx, sizeof(nsec3param_t));
		if (nsec3param == NULL)
			CHECK(ISC_R_NOMEMORY);
		ISC_LINK_INIT(nsec3param, link);

		/*
		 * now transfer the data from the rdata to
		 * the nsec3param
		 */
		dns_nsec3param_toprivate(&rdata, &private,
					 zone->privatetype, nsec3param->data,
					 sizeof(nsec3param->data));
		nsec3param->length = private.length;
		ISC_LIST_APPEND(*nsec3list, nsec3param, link);
	}

 getprivate:
	result = dns_db_findrdataset(db, node, version, zone->privatetype,
				     dns_rdatatype_none, 0, &prdataset, NULL);
	if (result != ISC_R_SUCCESS)
		goto done;

	/*
	 * walk private type records, converting them to nsec3 parameters
	 * using dns_nsec3param_fromprivate(), do the right thing based on
	 * CREATE and REMOVE flags
	 */
	for (result = dns_rdataset_first(&prdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&prdataset))
	{
		dns_rdata_t rdata = DNS_RDATA_INIT;
		dns_rdata_t private = DNS_RDATA_INIT;

		dns_rdataset_current(&prdataset, &private);
		isc_log_write(dns_lctx, DNS_LOGCATEGORY_GENERAL,
			      DNS_LOGMODULE_ZONE, ISC_LOG_DEBUG(3),
			      "looping through nsec3param private data");

		/*
		 * Do we have a valid private record?
		 */
		if (!dns_nsec3param_fromprivate(&private, &rdata,
						buf, sizeof(buf)))
			continue;

		/*
		 * Remove any NSEC3PARAM records scheduled to be removed.
		 */
		if (NSEC3REMOVE(rdata.data[1])) {
			/*
			 * Zero out the flags.
			 */
			rdata.data[1] = 0;

			for (nsec3p = ISC_LIST_HEAD(*nsec3list);
			     nsec3p != NULL;
			     nsec3p = next)
			{
				next = ISC_LIST_NEXT(nsec3p, link);

				if (nsec3p->length == rdata.length + 1 &&
				    memcmp(rdata.data, nsec3p->data + 1,
					   nsec3p->length - 1) == 0) {
					ISC_LIST_UNLINK(*nsec3list,
							nsec3p, link);
					isc_mem_put(zone->mctx, nsec3p,
						    sizeof(nsec3param_t));
				}
			}
			continue;
		}

		nsec3param = isc_mem_get(zone->mctx, sizeof(nsec3param_t));
		if (nsec3param == NULL)
			CHECK(ISC_R_NOMEMORY);
		ISC_LINK_INIT(nsec3param, link);

		/*
		 * Copy the remaining private records so the nsec/nsec3
		 * chain gets created.
		 */
		INSIST(private.length <= sizeof(nsec3param->data));
		memmove(nsec3param->data, private.data, private.length);
		nsec3param->length = private.length;
		ISC_LIST_APPEND(*nsec3list, nsec3param, link);
	}

 done:
	if (result == ISC_R_NOMORE || result == ISC_R_NOTFOUND)
		result = ISC_R_SUCCESS;

 failure:
	if (node != NULL)
		dns_db_detachnode(db, &node);
	if (version != NULL)
		dns_db_closeversion(db, &version, ISC_FALSE);
	if (db != NULL)
		dns_db_detach(&db);
	if (dns_rdataset_isassociated(&rdataset))
		dns_rdataset_disassociate(&rdataset);
	if (dns_rdataset_isassociated(&prdataset))
		dns_rdataset_disassociate(&prdataset);
	return (result);
}

/*
 * Walk the list of the nsec3 chains desired for the zone, converting
 * parameters to private type records using dns_nsec3param_toprivate(),
 * and insert them into the new zone db.
 */
static isc_result_t
restore_nsec3param(dns_zone_t *zone, dns_db_t *db, dns_dbversion_t *version,
		   nsec3paramlist_t *nsec3list)
{
	isc_result_t result;
	dns_diff_t diff;
	dns_rdata_t rdata;
	nsec3param_t *nsec3p = NULL;
	nsec3param_t *next;

	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(!ISC_LIST_EMPTY(*nsec3list));

	dns_diff_init(zone->mctx, &diff);

	/*
	 * Loop through the list of private-type records, set the INITIAL
	 * and CREATE flags, and the add the record to the apex of the tree
	 * in db.
	 */
	for (nsec3p = ISC_LIST_HEAD(*nsec3list);
	     nsec3p != NULL;
	     nsec3p = next)
	{
		next = ISC_LIST_NEXT(nsec3p, link);
		dns_rdata_init(&rdata);
		nsec3p->data[2] = DNS_NSEC3FLAG_CREATE | DNS_NSEC3FLAG_INITIAL;
		rdata.length = nsec3p->length;
		rdata.data = nsec3p->data;
		rdata.type = zone->privatetype;
		rdata.rdclass = zone->rdclass;
		CHECK(update_one_rr(db, version, &diff, DNS_DIFFOP_ADD,
				    &zone->origin, 0, &rdata));
	}

	result = ISC_R_SUCCESS;

failure:
	for (nsec3p = ISC_LIST_HEAD(*nsec3list);
	     nsec3p != NULL;
	     nsec3p = next)
	{
		next = ISC_LIST_NEXT(nsec3p, link);
		ISC_LIST_UNLINK(*nsec3list, nsec3p, link);
		isc_mem_put(zone->mctx, nsec3p, sizeof(nsec3param_t));
	}

	dns_diff_clear(&diff);
	return (result);
}

static void
receive_secure_db(isc_task_t *task, isc_event_t *event) {
	isc_result_t result;
	dns_zone_t *zone;
	dns_db_t *rawdb, *db = NULL;
	dns_dbnode_t *rawnode = NULL, *node = NULL;
	dns_fixedname_t fname;
	dns_name_t *name;
	dns_dbiterator_t *dbiterator = NULL;
	dns_rdatasetiter_t *rdsit = NULL;
	dns_rdataset_t rdataset;
	dns_dbversion_t *version = NULL;
	isc_time_t loadtime;
	unsigned int oldserial = 0;
	isc_boolean_t have_oldserial = ISC_FALSE;
	nsec3paramlist_t nsec3list;

	UNUSED(task);

	ISC_LIST_INIT(nsec3list);

	zone = event->ev_arg;
	rawdb = ((struct secure_event *)event)->db;
	isc_event_free(&event);

	dns_fixedname_init(&fname);
	name = dns_fixedname_name(&fname);
	dns_rdataset_init(&rdataset);

	LOCK_ZONE(zone);
	if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_EXITING) || !inline_secure(zone)) {
		result = ISC_R_SHUTTINGDOWN;
		goto failure;
	}

	TIME_NOW(&loadtime);
	ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_read);
	if (zone->db != NULL) {
		result = dns_db_getsoaserial(zone->db, NULL, &oldserial);
		if (result == ISC_R_SUCCESS)
			have_oldserial = ISC_TRUE;

		/*
		 * assemble nsec3parameters from the old zone, and set a flag
		 * if any are found
		 */
		result = save_nsec3param(zone, &nsec3list);
		if (result != ISC_R_SUCCESS) {
			ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);
			goto failure;
		}
	}
	ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);

	result = dns_db_create(zone->mctx, zone->db_argv[0],
			       &zone->origin, dns_dbtype_zone, zone->rdclass,
			       zone->db_argc - 1, zone->db_argv + 1, &db);
	if (result != ISC_R_SUCCESS)
		goto failure;

	result = dns_db_newversion(db, &version);
	if (result != ISC_R_SUCCESS)
		goto failure;

	result = dns_db_createiterator(rawdb, 0, &dbiterator);
	if (result != ISC_R_SUCCESS)
		goto failure;

	for (result = dns_dbiterator_first(dbiterator);
	     result == ISC_R_SUCCESS;
	     result = dns_dbiterator_next(dbiterator)) {
		result = dns_dbiterator_current(dbiterator, &rawnode, name);
		if (result != ISC_R_SUCCESS)
			continue;

		result = dns_db_findnode(db, name, ISC_TRUE, &node);
		if (result != ISC_R_SUCCESS)
			goto failure;

		result = dns_db_allrdatasets(rawdb, rawnode, NULL, 0, &rdsit);
		if (result != ISC_R_SUCCESS)
			goto failure;

		for (result = dns_rdatasetiter_first(rdsit);
		     result == ISC_R_SUCCESS;
		     result = dns_rdatasetiter_next(rdsit)) {
			dns_rdatasetiter_current(rdsit, &rdataset);
			if (rdataset.type == dns_rdatatype_nsec ||
			    rdataset.type == dns_rdatatype_rrsig ||
			    rdataset.type == dns_rdatatype_nsec3 ||
			    rdataset.type == dns_rdatatype_dnskey ||
			    rdataset.type == dns_rdatatype_nsec3param) {
				dns_rdataset_disassociate(&rdataset);
				continue;
			}
			if (rdataset.type == dns_rdatatype_soa &&
			    have_oldserial) {
				result = checkandaddsoa(db, node, version,
							&rdataset, oldserial);
			} else
				result = dns_db_addrdataset(db, node, version,
							    0, &rdataset, 0,
							    NULL);
			if (result != ISC_R_SUCCESS)
				goto failure;

			dns_rdataset_disassociate(&rdataset);
		}
		dns_rdatasetiter_destroy(&rdsit);
		dns_db_detachnode(rawdb, &rawnode);
		dns_db_detachnode(db, &node);
	}

	/*
	 * Call restore_nsec3param() to create private-type records from
	 * the old nsec3 parameters and insert them into db
	 */
	if (!ISC_LIST_EMPTY(nsec3list))
		restore_nsec3param(zone, db, version, &nsec3list);

	dns_db_closeversion(db, &version, ISC_TRUE);

	/*
	 * Lock hierarchy: zmgr, zone, raw.
	 */
	INSIST(zone != zone->raw);
	LOCK_ZONE(zone->raw);
	DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_NEEDNOTIFY);
	result = zone_postload(zone, db, loadtime, ISC_R_SUCCESS);
	zone_needdump(zone, 0); /* XXXMPA */
	UNLOCK_ZONE(zone->raw);

 failure:
	UNLOCK_ZONE(zone);
	if (result != ISC_R_SUCCESS)
		dns_zone_log(zone, ISC_LOG_ERROR, "receive_secure_db: %s",
			     dns_result_totext(result));

	while (!ISC_LIST_EMPTY(nsec3list)) {
		nsec3param_t *nsec3p;
		nsec3p = ISC_LIST_HEAD(nsec3list);
		ISC_LIST_UNLINK(nsec3list, nsec3p, link);
		isc_mem_put(zone->mctx, nsec3p, sizeof(nsec3param_t));
	}
	if (dns_rdataset_isassociated(&rdataset))
		dns_rdataset_disassociate(&rdataset);
	if (db != NULL) {
		if (node != NULL)
			dns_db_detachnode(db, &node);
		if (version != NULL)
			dns_db_closeversion(db, &version, ISC_FALSE);
		dns_db_detach(&db);
	}
	if (rawnode != NULL)
		dns_db_detachnode(rawdb, &rawnode);
	dns_db_detach(&rawdb);
	if (dbiterator != NULL)
		dns_dbiterator_destroy(&dbiterator);
	dns_zone_idetach(&zone);

	INSIST(version == NULL);
}

static isc_result_t
zone_send_securedb(dns_zone_t *zone, dns_db_t *db) {
	isc_event_t *e;
	dns_db_t *dummy = NULL;
	dns_zone_t *secure = NULL;

	e = isc_event_allocate(zone->secure->mctx, zone,
			       DNS_EVENT_ZONESECUREDB,
			       receive_secure_db, zone->secure,
			       sizeof(struct secure_event));
	if (e == NULL)
		return (ISC_R_NOMEMORY);
	dns_db_attach(db, &dummy);
	((struct secure_event *)e)->db = dummy;
	INSIST(LOCKED_ZONE(zone->secure));
	zone_iattach(zone->secure, &secure);
	isc_task_send(zone->secure->task, &e);
	DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_SENDSECURE);
	return (ISC_R_SUCCESS);
}

isc_result_t
dns_zone_replacedb(dns_zone_t *zone, dns_db_t *db, isc_boolean_t dump) {
	isc_result_t result;
	dns_zone_t *secure = NULL;

	REQUIRE(DNS_ZONE_VALID(zone));
 again:
	LOCK_ZONE(zone);
	if (inline_raw(zone)) {
		secure = zone->secure;
		INSIST(secure != zone);
		TRYLOCK_ZONE(result, secure);
		if (result != ISC_R_SUCCESS) {
			UNLOCK_ZONE(zone);
			secure = NULL;
#if ISC_PLATFORM_USETHREADS
			isc_thread_yield();
#endif
			goto again;
		}
	}
	ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_write);
	result = zone_replacedb(zone, db, dump);
	ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_write);
	if (secure != NULL)
		UNLOCK_ZONE(secure);
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
	if (inline_raw(zone))
		REQUIRE(LOCKED_ZONE(zone->secure));

	result = zone_get_from_db(zone, db, &nscount, &soacount,
				  NULL, NULL, NULL, NULL, NULL, NULL);
	if (result == ISC_R_SUCCESS) {
		if (soacount != 1) {
			dns_zone_log(zone, ISC_LOG_ERROR,
				     "has %d SOA records", soacount);
			result = DNS_R_BADZONE;
		}
		if (nscount == 0 && zone->type != dns_zone_key) {
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

	result = check_nsec3param(zone, db);
	if (result != ISC_R_SUCCESS)
		return (result);

	ver = NULL;
	dns_db_currentversion(db, &ver);

	/*
	 * The initial version of a slave zone is always dumped;
	 * subsequent versions may be journaled instead if this
	 * is enabled in the configuration.
	 */
	if (zone->db != NULL && zone->journal != NULL &&
	    DNS_ZONE_OPTION(zone, DNS_ZONEOPT_IXFRFROMDIFFS) &&
	    !DNS_ZONE_FLAG(zone, DNS_ZONEFLG_FORCEXFER))
	{
		isc_uint32_t serial, oldserial;

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
		result = zone_get_from_db(zone, zone->db, NULL, &soacount,
					  &oldserial, NULL, NULL, NULL, NULL,
					  NULL);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);
		RUNTIME_CHECK(soacount > 0U);
		if ((zone->type == dns_zone_slave ||
		     (zone->type == dns_zone_redirect &&
		      zone->masters != NULL))
		    && !isc_serial_gt(serial, oldserial)) {
			isc_uint32_t serialmin, serialmax;
			serialmin = (oldserial + 1) & 0xffffffffU;
			serialmax = (oldserial + 0x7fffffffU) & 0xffffffffU;
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
		if (zone->type == dns_zone_master && inline_raw(zone))
			zone_send_secureserial(zone, serial);
	} else {
		if (dump && zone->masterfile != NULL) {
			/*
			 * If DNS_ZONEFLG_FORCEXFER was set we don't want
			 * to keep the old masterfile.
			 */
			if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_FORCEXFER) &&
			    remove(zone->masterfile) < 0 && errno != ENOENT) {
				char strbuf[ISC_STRERRORSIZE];
				isc__strerror(errno, strbuf, sizeof(strbuf));
				isc_log_write(dns_lctx,
					      DNS_LOGCATEGORY_GENERAL,
					      DNS_LOGMODULE_ZONE,
					      ISC_LOG_WARNING,
					      "unable to remove masterfile "
					      "'%s': '%s'",
					      zone->masterfile, strbuf);
			}
			if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_LOADED) == 0)
				DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_NODELAY);
			else
				zone_needdump(zone, 0);
		}
		if (dump && zone->journal != NULL) {
			/*
			 * The in-memory database just changed, and
			 * because 'dump' is set, it didn't change by
			 * being loaded from disk.  Also, we have not
			 * journaled diffs for this change.
			 * Therefore, the on-disk journal is missing
			 * the deltas for this change.	Since it can
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

		if (inline_raw(zone))
			zone_send_securedb(zone, db);
	}

	dns_db_closeversion(db, &ver, ISC_FALSE);

	dns_zone_log(zone, ISC_LOG_DEBUG(3), "replacing zone database");

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
	dns_zone_t *secure = NULL;

	REQUIRE(DNS_ZONE_VALID(zone));

	dns_zone_log(zone, ISC_LOG_DEBUG(1),
		     "zone transfer finished: %s", dns_result_totext(result));

	/*
	 * Obtaining a lock on the zone->secure (see zone_send_secureserial)
	 * could result in a deadlock due to a LOR so we will spin if we
	 * can't obtain the both locks.
	 */
 again:
	LOCK_ZONE(zone);
	if (inline_raw(zone)) {
		secure = zone->secure;
		INSIST(secure != zone);
		TRYLOCK_ZONE(result, secure);
		if (result != ISC_R_SUCCESS) {
			UNLOCK_ZONE(zone);
			secure = NULL;
#if ISC_PLATFORM_USETHREADS
			isc_thread_yield();
#endif
			goto again;
		}
	}

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
			if (inline_raw(zone))
				zone_send_secureserial(zone, serial);
		}

		/*
		 * This is not necessary if we just performed a AXFR
		 * however it is necessary for an IXFR / UPTODATE and
		 * won't hurt with an AXFR.
		 */
		if (zone->masterfile != NULL || zone->journal != NULL) {
			unsigned int delay = DNS_DUMP_DELAY;

			result = ISC_R_FAILURE;
			if (zone->journal != NULL)
				result = isc_file_settime(zone->journal, &now);
			if (result != ISC_R_SUCCESS &&
			    zone->masterfile != NULL)
				result = isc_file_settime(zone->masterfile,
							  &now);

			if ((DNS_ZONE_FLAG(zone, DNS_ZONEFLG_NODELAY) != 0) ||
			    result == ISC_R_FILENOTFOUND)
				delay = 0;

			if ((result == ISC_R_SUCCESS ||
			    result == ISC_R_FILENOTFOUND) &&
			    zone->masterfile != NULL)
				zone_needdump(zone, delay);
			else if (result != ISC_R_SUCCESS)
				dns_zone_log(zone, ISC_LOG_ERROR,
					     "transfer: could not set file "
					     "modification time of '%s': %s",
					     zone->masterfile,
					     dns_result_totext(result));
		}
		DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_NODELAY);
		inc_stats(zone, dns_zonestatscounter_xfrsuccess);
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
		inc_stats(zone, dns_zonestatscounter_xfrfail);
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

	if (secure != NULL)
		UNLOCK_ZONE(secure);
	/*
	 * This transfer finishing freed up a transfer quota slot.
	 * Let any other zones waiting for quota have it.
	 */
	if (zone->zmgr != NULL &&
	    zone->statelist == &zone->zmgr->xfrin_in_progress) {
		UNLOCK_ZONE(zone);
		RWLOCK(&zone->zmgr->rwlock, isc_rwlocktype_write);
		ISC_LIST_UNLINK(zone->zmgr->xfrin_in_progress, zone, statelink);
		zone->statelist = NULL;
		zmgr_resume_xfrs(zone->zmgr, ISC_FALSE);
		RWUNLOCK(&zone->zmgr->rwlock, isc_rwlocktype_write);
		LOCK_ZONE(zone);
	}

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
	dns_zone_t *secure = NULL;

	REQUIRE(DNS_LOAD_VALID(load));
	zone = load->zone;

	ENTER;

	tresult = dns_db_endload(load->db, &load->callbacks.add_private);
	if (tresult != ISC_R_SUCCESS &&
	    (result == ISC_R_SUCCESS || result == DNS_R_SEENINCLUDE))
		result = tresult;

	/*
	 * Lock hierarchy: zmgr, zone, raw.
	 */
 again:
	LOCK_ZONE(zone);
	if (inline_secure(zone))
		LOCK_ZONE(zone->raw);
	else if (inline_raw(zone)) {
		secure = zone->secure;
		TRYLOCK_ZONE(result, secure);
		if (result != ISC_R_SUCCESS) {
			UNLOCK_ZONE(zone);
			secure = NULL;
#if ISC_PLATFORM_USETHREADS
			isc_thread_yield();
#endif
			goto again;
		}
	}
	(void)zone_postload(zone, load->db, load->loadtime, result);
	zonemgr_putio(&zone->readio);
	DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_LOADING);
	zone_idetach(&load->callbacks.zone);
	/*
	 * Leave the zone frozen if the reload fails.
	 */
	if ((result == ISC_R_SUCCESS || result == DNS_R_SEENINCLUDE) &&
	     DNS_ZONE_FLAG(zone, DNS_ZONEFLG_THAW))
		zone->update_disabled = ISC_FALSE;
	DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLG_THAW);
	if (inline_secure(zone))
		UNLOCK_ZONE(zone->raw);
	else if (secure != NULL)
		UNLOCK_ZONE(secure);
	UNLOCK_ZONE(zone);

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

void
dns_zone_setsigresigninginterval(dns_zone_t *zone, isc_uint32_t interval) {
	isc_time_t now;

	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	zone->sigresigninginterval = interval;
	set_resigntime(zone);
	if (zone->task != NULL) {
		TIME_NOW(&now);
		zone_settimer(zone, &now);
	}
	UNLOCK_ZONE(zone);
}

isc_uint32_t
dns_zone_getsigresigninginterval(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));

	return (zone->sigresigninginterval);
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
	isc_result_t result = ISC_R_SUCCESS;
	dns_peer_t *peer = NULL;
	char master[ISC_SOCKADDR_FORMATSIZE];
	char source[ISC_SOCKADDR_FORMATSIZE];
	dns_rdatatype_t xfrtype;
	dns_zone_t *zone = event->ev_arg;
	isc_netaddr_t masterip;
	isc_sockaddr_t sourceaddr;
	isc_sockaddr_t masteraddr;
	isc_time_t now;
	const char *soa_before = "";
	isc_boolean_t loaded;

	UNUSED(task);

	INSIST(task == zone->task);

	if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_EXITING)) {
		result = ISC_R_CANCELED;
		goto cleanup;
	}

	TIME_NOW(&now);

	isc_sockaddr_format(&zone->masteraddr, master, sizeof(master));
	if (dns_zonemgr_unreachable(zone->zmgr, &zone->masteraddr,
				    &zone->sourceaddr, &now))
	{
		isc_sockaddr_format(&zone->sourceaddr, source, sizeof(source));
		dns_zone_log(zone, ISC_LOG_INFO,
			     "got_transfer_quota: skipping zone transfer as "
			     "master %s (source %s) is unreachable (cached)",
			     master, source);
		result = ISC_R_CANCELED;
		goto cleanup;
	}

	isc_netaddr_fromsockaddr(&masterip, &zone->masteraddr);
	(void)dns_peerlist_peerbyaddr(zone->view->peers, &masterip, &peer);

	if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_SOABEFOREAXFR))
		soa_before = "SOA before ";
	/*
	 * Decide whether we should request IXFR or AXFR.
	 */
	ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_read);
	loaded = ISC_TF(zone->db != NULL);
	ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);

	if (!loaded) {
		dns_zone_log(zone, ISC_LOG_DEBUG(1),
			     "no database exists yet, requesting AXFR of "
			     "initial version from %s", master);
		xfrtype = dns_rdatatype_axfr;
	} else if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_FORCEXFER)) {
		dns_zone_log(zone, ISC_LOG_DEBUG(1),
			     "forced reload, requesting AXFR of "
			     "initial version from %s", master);
		xfrtype = dns_rdatatype_axfr;
	} else if (DNS_ZONE_FLAG(zone, DNS_ZONEFLAG_NOIXFR)) {
		dns_zone_log(zone, ISC_LOG_DEBUG(1),
			     "retrying with AXFR from %s due to "
			     "previous IXFR failure", master);
		xfrtype = dns_rdatatype_axfr;
		LOCK_ZONE(zone);
		DNS_ZONE_CLRFLAG(zone, DNS_ZONEFLAG_NOIXFR);
		UNLOCK_ZONE(zone);
	} else {
		isc_boolean_t use_ixfr = ISC_TRUE;
		if (peer != NULL)
			result = dns_peer_getrequestixfr(peer, &use_ixfr);
		if (peer == NULL || result != ISC_R_SUCCESS)
			use_ixfr = zone->requestixfr;
		if (use_ixfr == ISC_FALSE) {
			dns_zone_log(zone, ISC_LOG_DEBUG(1),
				     "IXFR disabled, requesting %sAXFR from %s",
				     soa_before, master);
			if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_SOABEFOREAXFR))
				xfrtype = dns_rdatatype_soa;
			else
				xfrtype = dns_rdatatype_axfr;
		} else {
			dns_zone_log(zone, ISC_LOG_DEBUG(1),
				     "requesting IXFR from %s", master);
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
			     "could not get TSIG key for zone transfer: %s",
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
	if (result == ISC_R_SUCCESS) {
		LOCK_ZONE(zone);
		if (xfrtype == dns_rdatatype_axfr) {
			if (isc_sockaddr_pf(&masteraddr) == PF_INET)
				inc_stats(zone, dns_zonestatscounter_axfrreqv4);
			else
				inc_stats(zone, dns_zonestatscounter_axfrreqv6);
		} else if (xfrtype == dns_rdatatype_ixfr) {
			if (isc_sockaddr_pf(&masteraddr) == PF_INET)
				inc_stats(zone, dns_zonestatscounter_ixfrreqv4);
			else
				inc_stats(zone, dns_zonestatscounter_ixfrreqv6);
		}
		UNLOCK_ZONE(zone);
	}
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
	if (forward->zone != NULL) {
		LOCK(&forward->zone->lock);
		if (ISC_LINK_LINKED(forward, link))
			ISC_LIST_UNLINK(forward->zone->forwards, forward, link);
		UNLOCK(&forward->zone->lock);
		dns_zone_idetach(&forward->zone);
	}
	isc_mem_putanddetach(&forward->mctx, forward, sizeof(*forward));
}

static isc_result_t
sendtomaster(dns_forward_t *forward) {
	isc_result_t result;
	isc_sockaddr_t src;

	LOCK_ZONE(forward->zone);

	if (DNS_ZONE_FLAG(forward->zone, DNS_ZONEFLG_EXITING)) {
		UNLOCK_ZONE(forward->zone);
		return (ISC_R_CANCELED);
	}

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
				       forward->options, 15 /* XXX */,
				       forward->zone->task,
				       forward_callback, forward,
				       &forward->request);
	if (result == ISC_R_SUCCESS) {
		if (!ISC_LINK_LINKED(forward, link))
			ISC_LIST_APPEND(forward->zone->forwards, forward, link);
	}

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
	case dns_rcode_nxdomain: {
		char rcode[128];
		isc_buffer_t rb;

		isc_buffer_init(&rb, rcode, sizeof(rcode));
		(void)dns_rcode_totext(msg->rcode, &rb);
		dns_zone_log(zone, ISC_LOG_INFO,
			     "forwarded dynamic update: "
			     "master %s returned: %.*s",
			     master, (int)rb.used, rcode);
		break;
	}

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
	ISC_LINK_INIT(forward, link);
	forward->magic = FORWARD_MAGIC;
	forward->options = DNS_REQUESTOPT_TCP;
	/*
	 * If we have a SIG(0) signed message we need to preserve the
	 * query id as that is included in the SIG(0) computation.
	 */
	if (msg->sig0 != NULL)
		forward->options |= DNS_REQUESTOPT_FIXEDID;

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
	zmgr->loadtasks = NULL;
	zmgr->mctxpool = NULL;
	zmgr->task = NULL;
	zmgr->notifyrl = NULL;
	zmgr->refreshrl = NULL;
	zmgr->startupnotifyrl = NULL;
	zmgr->startuprefreshrl = NULL;
	ISC_LIST_INIT(zmgr->zones);
	ISC_LIST_INIT(zmgr->waiting_for_xfrin);
	ISC_LIST_INIT(zmgr->xfrin_in_progress);
	memset(zmgr->unreachable, 0, sizeof(zmgr->unreachable));
	result = isc_rwlock_init(&zmgr->rwlock, 0, 0);
	if (result != ISC_R_SUCCESS)
		goto free_mem;

	zmgr->transfersin = 10;
	zmgr->transfersperns = 2;

	/* Unreachable lock. */
	result = isc_rwlock_init(&zmgr->urlock, 0, 0);
	if (result != ISC_R_SUCCESS)
		goto free_rwlock;

	/* Create a single task for queueing of SOA queries. */
	result = isc_task_create(taskmgr, 1, &zmgr->task);
	if (result != ISC_R_SUCCESS)
		goto free_urlock;

	isc_task_setname(zmgr->task, "zmgr", zmgr);
	result = isc_ratelimiter_create(mctx, timermgr, zmgr->task,
					&zmgr->notifyrl);
	if (result != ISC_R_SUCCESS)
		goto free_task;

	result = isc_ratelimiter_create(mctx, timermgr, zmgr->task,
					&zmgr->refreshrl);
	if (result != ISC_R_SUCCESS)
		goto free_notifyrl;

	result = isc_ratelimiter_create(mctx, timermgr, zmgr->task,
					&zmgr->startupnotifyrl);
	if (result != ISC_R_SUCCESS)
		goto free_refreshrl;

	result = isc_ratelimiter_create(mctx, timermgr, zmgr->task,
					&zmgr->startuprefreshrl);
	if (result != ISC_R_SUCCESS)
		goto free_startupnotifyrl;

	/* default to 20 refresh queries / notifies per second. */
	setrl(zmgr->notifyrl, &zmgr->notifyrate, 20);
	setrl(zmgr->startupnotifyrl, &zmgr->startupnotifyrate, 20);
	setrl(zmgr->refreshrl, &zmgr->serialqueryrate, 20);
	setrl(zmgr->startuprefreshrl, &zmgr->startupserialqueryrate, 20);

	zmgr->iolimit = 1;
	zmgr->ioactive = 0;
	ISC_LIST_INIT(zmgr->high);
	ISC_LIST_INIT(zmgr->low);

	result = isc_mutex_init(&zmgr->iolock);
	if (result != ISC_R_SUCCESS)
		goto free_startuprefreshrl;

	zmgr->magic = ZONEMGR_MAGIC;

	*zmgrp = zmgr;
	return (ISC_R_SUCCESS);

#if 0
 free_iolock:
	DESTROYLOCK(&zmgr->iolock);
#endif
 free_startuprefreshrl:
	isc_ratelimiter_detach(&zmgr->startuprefreshrl);
 free_startupnotifyrl:
	isc_ratelimiter_detach(&zmgr->startupnotifyrl);
 free_refreshrl:
	isc_ratelimiter_detach(&zmgr->refreshrl);
 free_notifyrl:
	isc_ratelimiter_detach(&zmgr->notifyrl);
 free_task:
	isc_task_detach(&zmgr->task);
 free_urlock:
	isc_rwlock_destroy(&zmgr->urlock);
 free_rwlock:
	isc_rwlock_destroy(&zmgr->rwlock);
 free_mem:
	isc_mem_put(zmgr->mctx, zmgr, sizeof(*zmgr));
	isc_mem_detach(&mctx);
	return (result);
}

isc_result_t
dns_zonemgr_createzone(dns_zonemgr_t *zmgr, dns_zone_t **zonep) {
	isc_result_t result;
	isc_mem_t *mctx = NULL;
	dns_zone_t *zone = NULL;
	void *item;

	REQUIRE(DNS_ZONEMGR_VALID(zmgr));
	REQUIRE(zonep != NULL && *zonep == NULL);

	if (zmgr->mctxpool == NULL)
		return (ISC_R_FAILURE);

	item = isc_pool_get(zmgr->mctxpool);
	if (item == NULL)
		return (ISC_R_FAILURE);

	isc_mem_attach((isc_mem_t *) item, &mctx);
	result = dns_zone_create(&zone, mctx);
	isc_mem_detach(&mctx);

	if (result == ISC_R_SUCCESS)
		*zonep = zone;

	return (result);
}

isc_result_t
dns_zonemgr_managezone(dns_zonemgr_t *zmgr, dns_zone_t *zone) {
	isc_result_t result;

	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(DNS_ZONEMGR_VALID(zmgr));

	if (zmgr->zonetasks == NULL)
		return (ISC_R_FAILURE);

	RWLOCK(&zmgr->rwlock, isc_rwlocktype_write);
	LOCK_ZONE(zone);
	REQUIRE(zone->task == NULL);
	REQUIRE(zone->timer == NULL);
	REQUIRE(zone->zmgr == NULL);

	isc_taskpool_gettask(zmgr->zonetasks, &zone->task);
	isc_taskpool_gettask(zmgr->loadtasks, &zone->loadtask);

	/*
	 * Set the task name.  The tag will arbitrarily point to one
	 * of the zones sharing the task (in practice, the one
	 * to be managed last).
	 */
	isc_task_setname(zone->task, "zone", zone);
	isc_task_setname(zone->loadtask, "loadzone", zone);

	result = isc_timer_create(zmgr->timermgr, isc_timertype_inactive,
				  NULL, NULL,
				  zone->task, zone_timer, zone,
				  &zone->timer);

	if (result != ISC_R_SUCCESS)
		goto cleanup_tasks;

	/*
	 * The timer "holds" a iref.
	 */
	zone->irefs++;
	INSIST(zone->irefs != 0);

	ISC_LIST_APPEND(zmgr->zones, zone, link);
	zone->zmgr = zmgr;
	zmgr->refs++;

	goto unlock;

 cleanup_tasks:
	isc_task_detach(&zone->loadtask);
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
	*zmgrp = NULL;
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
	dns_zone_t *zone;

	REQUIRE(DNS_ZONEMGR_VALID(zmgr));

	isc_ratelimiter_shutdown(zmgr->notifyrl);
	isc_ratelimiter_shutdown(zmgr->refreshrl);
	isc_ratelimiter_shutdown(zmgr->startupnotifyrl);
	isc_ratelimiter_shutdown(zmgr->startuprefreshrl);

	if (zmgr->task != NULL)
		isc_task_destroy(&zmgr->task);
	if (zmgr->zonetasks != NULL)
		isc_taskpool_destroy(&zmgr->zonetasks);
	if (zmgr->loadtasks != NULL)
		isc_taskpool_destroy(&zmgr->loadtasks);
	if (zmgr->mctxpool != NULL)
		isc_pool_destroy(&zmgr->mctxpool);

	RWLOCK(&zmgr->rwlock, isc_rwlocktype_read);
	for (zone = ISC_LIST_HEAD(zmgr->zones);
	     zone != NULL;
	     zone = ISC_LIST_NEXT(zone, link))
	{
		LOCK_ZONE(zone);
		forward_cancel(zone);
		UNLOCK_ZONE(zone);
	}
	RWUNLOCK(&zmgr->rwlock, isc_rwlocktype_read);
}

static isc_result_t
mctxinit(void **target, void *arg) {
	isc_result_t result;
	isc_mem_t *mctx = NULL;

	UNUSED(arg);

	REQUIRE(target != NULL && *target == NULL);

	result = isc_mem_create(0, 0, &mctx);
	if (result != ISC_R_SUCCESS)
		return (result);
	isc_mem_setname(mctx, "zonemgr-pool", NULL);

	*target = mctx;
	return (ISC_R_SUCCESS);
}

static void
mctxfree(void **target) {
	isc_mem_t *mctx = *(isc_mem_t **) target;
	isc_mem_detach(&mctx);
	*target = NULL;
}

#define ZONES_PER_TASK 100
#define ZONES_PER_MCTX 1000

isc_result_t
dns_zonemgr_setsize(dns_zonemgr_t *zmgr, int num_zones) {
	isc_result_t result;
	int ntasks = num_zones / ZONES_PER_TASK;
	int nmctx = num_zones / ZONES_PER_MCTX;
	isc_taskpool_t *pool = NULL;
	isc_pool_t *mctxpool = NULL;

	REQUIRE(DNS_ZONEMGR_VALID(zmgr));

	/*
	 * For anything fewer than 1000 zones we use 10 tasks in
	 * the task pools.  More than that, and we'll scale at one
	 * task per 100 zones.  Similarly, for anything smaller than
	 * 2000 zones we use 2 memory contexts, then scale at 1:1000.
	 */
	if (ntasks < 10)
		ntasks = 10;
	if (nmctx < 2)
		nmctx = 2;

	/* Create or resize the zone task pools. */
	if (zmgr->zonetasks == NULL)
		result = isc_taskpool_create(zmgr->taskmgr, zmgr->mctx,
					     ntasks, 2, &pool);
	else
		result = isc_taskpool_expand(&zmgr->zonetasks, ntasks, &pool);

	if (result == ISC_R_SUCCESS)
		zmgr->zonetasks = pool;

	pool = NULL;
	if (zmgr->loadtasks == NULL)
		result = isc_taskpool_create(zmgr->taskmgr, zmgr->mctx,
					     ntasks, 2, &pool);
	else
		result = isc_taskpool_expand(&zmgr->loadtasks, ntasks, &pool);

	if (result == ISC_R_SUCCESS)
		zmgr->loadtasks = pool;

#ifdef BIND9
	/*
	 * We always set all tasks in the zone-load task pool to
	 * privileged.  This prevents other tasks in the system from
	 * running while the server task manager is in privileged
	 * mode.
	 *
	 * NOTE: If we start using task privileges for any other
	 * part of the system than zone tasks, then this will need to be
	 * revisted.  In that case we'd want to turn on privileges for
	 * zone tasks only when we were loading, and turn them off the
	 * rest of the time.  For now, however, it's okay to just
	 * set it and forget it.
	 */
	isc_taskpool_setprivilege(zmgr->loadtasks, ISC_TRUE);
#endif

	/* Create or resize the zone memory context pool. */
	if (zmgr->mctxpool == NULL)
		result = isc_pool_create(zmgr->mctx, nmctx, mctxfree,
					 mctxinit, NULL, &mctxpool);
	else
		result = isc_pool_expand(&zmgr->mctxpool, nmctx, &mctxpool);

	if (result == ISC_R_SUCCESS)
		zmgr->mctxpool = mctxpool;

	return (result);
}

static void
zonemgr_free(dns_zonemgr_t *zmgr) {
	isc_mem_t *mctx;

	INSIST(zmgr->refs == 0);
	INSIST(ISC_LIST_EMPTY(zmgr->zones));

	zmgr->magic = 0;

	DESTROYLOCK(&zmgr->iolock);
	isc_ratelimiter_detach(&zmgr->notifyrl);
	isc_ratelimiter_detach(&zmgr->refreshrl);
	isc_ratelimiter_detach(&zmgr->startupnotifyrl);
	isc_ratelimiter_detach(&zmgr->startuprefreshrl);

	isc_rwlock_destroy(&zmgr->urlock);
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
	 * If we are exiting just pretend we got quota so the zone will
	 * be cleaned up in the zone's task context.
	 */
	LOCK_ZONE(zone);
	if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_EXITING)) {
		UNLOCK_ZONE(zone);
		goto gotquota;
	}

	/*
	 * Find any configured information about the server we'd
	 * like to transfer this zone from.
	 */
	isc_netaddr_fromsockaddr(&masterip, &zone->masteraddr);
	(void)dns_peerlist_peerbyaddr(zone->view->peers, &masterip, &peer);
	UNLOCK_ZONE(zone);

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

		LOCK_ZONE(x);
		isc_netaddr_fromsockaddr(&xip, &x->masteraddr);
		UNLOCK_ZONE(x);

		nxfrsin++;
		if (isc_netaddr_equal(&xip, &masterip))
			nxfrsperns++;
	}

	/* Enforce quota. */
	if (nxfrsin >= maxtransfersin)
		return (ISC_R_QUOTA);

	if (nxfrsperns >= maxtransfersperns)
		return (ISC_R_QUOTA);

 gotquota:
	/*
	 * We have sufficient quota.  Move the zone to the "xfrin_in_progress"
	 * list and send it an event to let it start the actual transfer in the
	 * context of its own task.
	 */
	e = isc_event_allocate(zmgr->mctx, zmgr, DNS_EVENT_ZONESTARTXFRIN,
			       got_transfer_quota, zone, sizeof(isc_event_t));
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

	if (!queue)
		isc_task_send(io->task, &io->event);
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

	dns_zone_log(zone, ISC_LOG_WARNING, "unable to load from '%s'; "
		     "renaming file to '%s' for failure analysis and "
		     "retransferring.", path, buf);

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

static void
setrl(isc_ratelimiter_t *rl, unsigned int *rate, unsigned int value) {
	isc_interval_t interval;
	isc_uint32_t s, ns;
	isc_uint32_t pertic;
	isc_result_t result;

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

	result = isc_ratelimiter_setinterval(rl, &interval);
	RUNTIME_CHECK(result == ISC_R_SUCCESS);
	isc_ratelimiter_setpertic(rl, pertic);

	*rate = value;
}

void
dns_zonemgr_setserialqueryrate(dns_zonemgr_t *zmgr, unsigned int value) {

	REQUIRE(DNS_ZONEMGR_VALID(zmgr));

	setrl(zmgr->refreshrl, &zmgr->serialqueryrate, value);

	/* Seperately controlled in BIND 9.11.x */
	setrl(zmgr->notifyrl, &zmgr->notifyrate, value);
	setrl(zmgr->startupnotifyrl, &zmgr->startupnotifyrate, value);

	/* XXXMPA seperate out once we have the code to support this. */
	setrl(zmgr->startuprefreshrl, &zmgr->startupserialqueryrate, value);
}

unsigned int
dns_zonemgr_getserialqueryrate(dns_zonemgr_t *zmgr) {
	REQUIRE(DNS_ZONEMGR_VALID(zmgr));

	return (zmgr->serialqueryrate);
}

isc_boolean_t
dns_zonemgr_unreachable(dns_zonemgr_t *zmgr, isc_sockaddr_t *remote,
			isc_sockaddr_t *local, isc_time_t *now)
{
	unsigned int i;
	isc_rwlocktype_t locktype;
	isc_result_t result;
	isc_uint32_t seconds = isc_time_seconds(now);
	isc_uint32_t count = 0;

	REQUIRE(DNS_ZONEMGR_VALID(zmgr));

	locktype = isc_rwlocktype_read;
	RWLOCK(&zmgr->urlock, locktype);
	for (i = 0; i < UNREACH_CHACHE_SIZE; i++) {
		if (zmgr->unreachable[i].expire >= seconds &&
		    isc_sockaddr_equal(&zmgr->unreachable[i].remote, remote) &&
		    isc_sockaddr_equal(&zmgr->unreachable[i].local, local)) {
			result = isc_rwlock_tryupgrade(&zmgr->urlock);
			if (result == ISC_R_SUCCESS) {
				locktype = isc_rwlocktype_write;
				zmgr->unreachable[i].last = seconds;
				count = zmgr->unreachable[i].count;
			}
			break;
		}
	}
	RWUNLOCK(&zmgr->urlock, locktype);
	return (ISC_TF(i < UNREACH_CHACHE_SIZE && count > 1U));
}

void
dns_zonemgr_unreachabledel(dns_zonemgr_t *zmgr, isc_sockaddr_t *remote,
			   isc_sockaddr_t *local)
{
	unsigned int i;
	isc_rwlocktype_t locktype;
	isc_result_t result;

	char master[ISC_SOCKADDR_FORMATSIZE];
	char source[ISC_SOCKADDR_FORMATSIZE];

	isc_sockaddr_format(remote, master, sizeof(master));
	isc_sockaddr_format(local, source, sizeof(source));

	REQUIRE(DNS_ZONEMGR_VALID(zmgr));

	locktype = isc_rwlocktype_read;
	RWLOCK(&zmgr->urlock, locktype);
	for (i = 0; i < UNREACH_CHACHE_SIZE; i++) {
		if (isc_sockaddr_equal(&zmgr->unreachable[i].remote, remote) &&
		    isc_sockaddr_equal(&zmgr->unreachable[i].local, local)) {
			if (zmgr->unreachable[i].expire == 0)
				break;
			result = isc_rwlock_tryupgrade(&zmgr->urlock);
			if (result == ISC_R_SUCCESS) {
				locktype = isc_rwlocktype_write;
				zmgr->unreachable[i].expire = 0;
				isc_log_write(dns_lctx, DNS_LOGCATEGORY_GENERAL,
					      DNS_LOGMODULE_ZONE, ISC_LOG_INFO,
					      "master %s (source %s) deleted "
					      "from unreachable cache",
					      master, source);
			}
			break;
		}
	}
	RWUNLOCK(&zmgr->urlock, locktype);
}

void
dns_zonemgr_unreachableadd(dns_zonemgr_t *zmgr, isc_sockaddr_t *remote,
			   isc_sockaddr_t *local, isc_time_t *now)
{
	isc_uint32_t seconds = isc_time_seconds(now);
	isc_uint32_t last = seconds;
	unsigned int i, slot = UNREACH_CHACHE_SIZE, oldest = 0;

	REQUIRE(DNS_ZONEMGR_VALID(zmgr));

	RWLOCK(&zmgr->urlock, isc_rwlocktype_write);
	for (i = 0; i < UNREACH_CHACHE_SIZE; i++) {
		/* Existing entry? */
		if (isc_sockaddr_equal(&zmgr->unreachable[i].remote, remote) &&
		    isc_sockaddr_equal(&zmgr->unreachable[i].local, local))
			break;
		/* Empty slot? */
		if (zmgr->unreachable[i].expire < seconds)
			slot = i;
		/* Least recently used slot? */
		if (zmgr->unreachable[i].last < last) {
			last = zmgr->unreachable[i].last;
			oldest = i;
		}
	}
	if (i < UNREACH_CHACHE_SIZE) {
		/*
		 * Found a existing entry.  Update the expire timer and
		 * last usage timestamps.
		 */
		zmgr->unreachable[i].expire = seconds + UNREACH_HOLD_TIME;
		zmgr->unreachable[i].last = seconds;
		if (zmgr->unreachable[i].expire < seconds)
			zmgr->unreachable[i].count = 1;
		else
			zmgr->unreachable[i].count++;
	} else if (slot != UNREACH_CHACHE_SIZE) {
		/*
		 * Found a empty slot. Add a new entry to the cache.
		 */
		zmgr->unreachable[slot].expire = seconds + UNREACH_HOLD_TIME;
		zmgr->unreachable[slot].last = seconds;
		zmgr->unreachable[slot].remote = *remote;
		zmgr->unreachable[slot].local = *local;
		zmgr->unreachable[slot].count = 1;
	} else {
		/*
		 * Replace the least recently used entry in the cache.
		 */
		zmgr->unreachable[oldest].expire = seconds + UNREACH_HOLD_TIME;
		zmgr->unreachable[oldest].last = seconds;
		zmgr->unreachable[oldest].remote = *remote;
		zmgr->unreachable[oldest].local = *local;
		zmgr->unreachable[oldest].count = 1;
	}
	RWUNLOCK(&zmgr->urlock, isc_rwlocktype_write);
}

void
dns_zone_forcereload(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));

	if (zone->type == dns_zone_master ||
	    (zone->type == dns_zone_redirect && zone->masters == NULL))
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
	/*
	 * This function is obsoleted.
	 */
	UNUSED(zone);
	UNUSED(on);
	return (ISC_R_NOTIMPLEMENTED);
}

isc_uint64_t *
dns_zone_getstatscounters(dns_zone_t *zone) {
	/*
	 * This function is obsoleted.
	 */
	UNUSED(zone);
	return (NULL);
}

void
dns_zone_setstats(dns_zone_t *zone, isc_stats_t *stats) {
	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(zone->stats == NULL);

	LOCK_ZONE(zone);
	zone->stats = NULL;
	isc_stats_attach(stats, &zone->stats);
	UNLOCK_ZONE(zone);
}

void
dns_zone_setrequeststats(dns_zone_t *zone, isc_stats_t *stats) {

	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	if (zone->requeststats_on && stats == NULL)
		zone->requeststats_on = ISC_FALSE;
	else if (!zone->requeststats_on && stats != NULL) {
		if (zone->requeststats == NULL) {
			isc_stats_attach(stats, &zone->requeststats);
			zone->requeststats_on = ISC_TRUE;
		}
	}
	UNLOCK_ZONE(zone);
}

#ifdef NEWSTATS
void
dns_zone_setrcvquerystats(dns_zone_t *zone, dns_stats_t *stats) {

	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);
	if (zone->requeststats_on && stats != NULL) {
		if (zone->rcvquerystats == NULL) {
			dns_stats_attach(stats, &zone->rcvquerystats);
			zone->requeststats_on = ISC_TRUE;
		}
	}
	UNLOCK_ZONE(zone);
}
#endif

isc_stats_t *
dns_zone_getrequeststats(dns_zone_t *zone) {
	/*
	 * We don't lock zone for efficiency reason.  This is not catastrophic
	 * because requeststats must always be valid when requeststats_on is
	 * true.
	 * Some counters may be incremented while requeststats_on is becoming
	 * false, or some cannot be incremented just after the statistics are
	 * installed, but it shouldn't matter much in practice.
	 */
	if (zone->requeststats_on)
		return (zone->requeststats);
	else
		return (NULL);
}

#ifdef NEWSTATS
/*
 * Return the received query stats bucket
 * see note from dns_zone_getrequeststats()
 */
dns_stats_t *
dns_zone_getrcvquerystats(dns_zone_t *zone) {
	if (zone->requeststats_on)
		return (zone->rcvquerystats);
	else
		return (NULL);
}
#endif

void
dns_zone_dialup(dns_zone_t *zone) {

	REQUIRE(DNS_ZONE_VALID(zone));

	zone_debuglog(zone, "dns_zone_dialup", 3,
		      "notify = %d, refresh = %d",
		      DNS_ZONE_FLAG(zone, DNS_ZONEFLG_DIALNOTIFY),
		      DNS_ZONE_FLAG(zone, DNS_ZONEFLG_DIALREFRESH));

	if (DNS_ZONE_FLAG(zone, DNS_ZONEFLG_DIALNOTIFY))
		dns_zone_notify(zone);
	if (zone->type != dns_zone_master && zone->masters != NULL &&
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
		DNS_ZONE_SETFLAG(zone,	(DNS_ZONEFLG_DIALNOTIFY |
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

	if (!DNS_ZONE_OPTION(zone, DNS_ZONEOPT_CHECKNAMES) &&
	    rdata->type != dns_rdatatype_nsec3)
		return (ISC_R_SUCCESS);

	if (DNS_ZONE_OPTION(zone, DNS_ZONEOPT_CHECKNAMESFAIL) ||
	    rdata->type == dns_rdatatype_nsec3) {
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

isc_result_t
dns_zone_signwithkey(dns_zone_t *zone, dns_secalg_t algorithm,
		     isc_uint16_t keyid, isc_boolean_t delete)
{
	isc_result_t result;
	REQUIRE(DNS_ZONE_VALID(zone));

	dns_zone_log(zone, ISC_LOG_NOTICE,
		     "dns_zone_signwithkey(algorithm=%u, keyid=%u)",
		     algorithm, keyid);
	LOCK_ZONE(zone);
	result = zone_signwithkey(zone, algorithm, keyid, delete);
	UNLOCK_ZONE(zone);

	return (result);
}

static const char *hex = "0123456789ABCDEF";

isc_result_t
dns_zone_addnsec3chain(dns_zone_t *zone, dns_rdata_nsec3param_t *nsec3param) {
	isc_result_t result;
	char salt[255*2+1];
	unsigned int i, j;

	REQUIRE(DNS_ZONE_VALID(zone));

	if (nsec3param->salt_length != 0) {
		INSIST((nsec3param->salt_length * 2U) < sizeof(salt));
		for (i = 0, j = 0; i < nsec3param->salt_length; i++) {
			salt[j++] = hex[(nsec3param->salt[i] >> 4) & 0xf];
			salt[j++] = hex[nsec3param->salt[i] & 0xf];
		}
		salt[j] = '\0';
	} else
		strcpy(salt, "-");
	dns_zone_log(zone, ISC_LOG_NOTICE,
		     "dns_zone_addnsec3chain(hash=%u, iterations=%u, salt=%s)",
		     nsec3param->hash, nsec3param->iterations,
		     salt);
	LOCK_ZONE(zone);
	result = zone_addnsec3chain(zone, nsec3param);
	UNLOCK_ZONE(zone);

	return (result);
}

void
dns_zone_setnodes(dns_zone_t *zone, isc_uint32_t nodes) {
	REQUIRE(DNS_ZONE_VALID(zone));

	if (nodes == 0)
		nodes = 1;
	zone->nodes = nodes;
}

void
dns_zone_setsignatures(dns_zone_t *zone, isc_uint32_t signatures) {
	REQUIRE(DNS_ZONE_VALID(zone));

	/*
	 * We treat signatures as a signed value so explicitly
	 * limit its range here.
	 */
	if (signatures > ISC_INT32_MAX)
		signatures = ISC_INT32_MAX;
	else if (signatures == 0)
		signatures = 1;
	zone->signatures = signatures;
}

isc_uint32_t
dns_zone_getsignatures(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));
	return (zone->signatures);
}

void
dns_zone_setprivatetype(dns_zone_t *zone, dns_rdatatype_t type) {
	REQUIRE(DNS_ZONE_VALID(zone));
	zone->privatetype = type;
}

dns_rdatatype_t
dns_zone_getprivatetype(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));
	return (zone->privatetype);
}

static isc_result_t
zone_signwithkey(dns_zone_t *zone, dns_secalg_t algorithm, isc_uint16_t keyid,
		 isc_boolean_t delete)
{
	dns_signing_t *signing;
	dns_signing_t *current;
	isc_result_t result = ISC_R_SUCCESS;
	isc_time_t now;
	dns_db_t *db = NULL;

	signing = isc_mem_get(zone->mctx, sizeof *signing);
	if (signing == NULL)
		return (ISC_R_NOMEMORY);

	signing->magic = 0;
	signing->db  = NULL;
	signing->dbiterator = NULL;
	signing->algorithm = algorithm;
	signing->keyid = keyid;
	signing->delete = delete;
	signing->done = ISC_FALSE;

	TIME_NOW(&now);

	ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_read);
	if (zone->db != NULL)
		dns_db_attach(zone->db, &db);
	ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);

	if (db == NULL) {
		result = ISC_R_NOTFOUND;
		goto cleanup;
	}

	dns_db_attach(db, &signing->db);

	for (current = ISC_LIST_HEAD(zone->signing);
	     current != NULL;
	     current = ISC_LIST_NEXT(current, link)) {
		if (current->db == signing->db &&
		    current->algorithm == signing->algorithm &&
		    current->keyid == signing->keyid) {
			if (current->delete != signing->delete)
				current->done = ISC_TRUE;
			else
				goto cleanup;
		}
	}

	result = dns_db_createiterator(signing->db, 0,
				       &signing->dbiterator);

	if (result == ISC_R_SUCCESS)
		result = dns_dbiterator_first(signing->dbiterator);
	if (result == ISC_R_SUCCESS) {
		dns_dbiterator_pause(signing->dbiterator);
		ISC_LIST_INITANDAPPEND(zone->signing, signing, link);
		signing = NULL;
		if (isc_time_isepoch(&zone->signingtime)) {
			zone->signingtime = now;
			if (zone->task != NULL)
				zone_settimer(zone, &now);
		}
	}

 cleanup:
	if (signing != NULL) {
		if (signing->db != NULL)
			dns_db_detach(&signing->db);
		if (signing->dbiterator != NULL)
			dns_dbiterator_destroy(&signing->dbiterator);
		isc_mem_put(zone->mctx, signing, sizeof *signing);
	}
	if (db != NULL)
		dns_db_detach(&db);
	return (result);
}

static void
logmsg(const char *format, ...) {
	va_list args;
	va_start(args, format);
	isc_log_vwrite(dns_lctx, DNS_LOGCATEGORY_GENERAL, DNS_LOGMODULE_ZONE,
		       ISC_LOG_DEBUG(1), format, args);
	va_end(args);
}

static void
clear_keylist(dns_dnsseckeylist_t *list, isc_mem_t *mctx) {
	dns_dnsseckey_t *key;
	while (!ISC_LIST_EMPTY(*list)) {
		key = ISC_LIST_HEAD(*list);
		ISC_LIST_UNLINK(*list, key, link);
		dns_dnsseckey_destroy(mctx, &key);
	}
}

/* Called once; *timep should be set to the current time. */
static isc_result_t
next_keyevent(dst_key_t *key, isc_stdtime_t *timep) {
	isc_result_t result;
	isc_stdtime_t now, then = 0, event;
	int i;

	now = *timep;

	for (i = 0; i <= DST_MAX_TIMES; i++) {
		result = dst_key_gettime(key, i, &event);
		if (result == ISC_R_SUCCESS && event > now &&
		    (then == 0 || event < then))
			then = event;
	}

	if (then != 0) {
		*timep = then;
		return (ISC_R_SUCCESS);
	}

	return (ISC_R_NOTFOUND);
}

static isc_result_t
rr_exists(dns_db_t *db, dns_dbversion_t *ver, dns_name_t *name,
	  const dns_rdata_t *rdata, isc_boolean_t *flag)
{
	dns_rdataset_t rdataset;
	dns_dbnode_t *node = NULL;
	isc_result_t result;

	dns_rdataset_init(&rdataset);
	if (rdata->type == dns_rdatatype_nsec3)
		CHECK(dns_db_findnsec3node(db, name, ISC_FALSE, &node));
	else
		CHECK(dns_db_findnode(db, name, ISC_FALSE, &node));
	result = dns_db_findrdataset(db, node, ver, rdata->type, 0,
				     (isc_stdtime_t) 0, &rdataset, NULL);
	if (result == ISC_R_NOTFOUND) {
		*flag = ISC_FALSE;
		result = ISC_R_SUCCESS;
		goto failure;
	}

	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset)) {
		dns_rdata_t myrdata = DNS_RDATA_INIT;
		dns_rdataset_current(&rdataset, &myrdata);
		if (!dns_rdata_compare(&myrdata, rdata))
			break;
	}
	dns_rdataset_disassociate(&rdataset);
	if (result == ISC_R_SUCCESS) {
		*flag = ISC_TRUE;
	} else if (result == ISC_R_NOMORE) {
		*flag = ISC_FALSE;
		result = ISC_R_SUCCESS;
	}

 failure:
	if (node != NULL)
		dns_db_detachnode(db, &node);
	return (result);
}

/*
 * Add records to signal the state of signing or of key removal.
 */
static isc_result_t
add_signing_records(dns_db_t *db, dns_rdatatype_t privatetype,
		    dns_dbversion_t *ver, dns_diff_t *diff,
		    isc_boolean_t sign_all)
{
	dns_difftuple_t *tuple, *newtuple = NULL;
	dns_rdata_dnskey_t dnskey;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	isc_boolean_t flag;
	isc_region_t r;
	isc_result_t result = ISC_R_SUCCESS;
	isc_uint16_t keyid;
	unsigned char buf[5];
	dns_name_t *name = dns_db_origin(db);

	for (tuple = ISC_LIST_HEAD(diff->tuples);
	     tuple != NULL;
	     tuple = ISC_LIST_NEXT(tuple, link)) {
		if (tuple->rdata.type != dns_rdatatype_dnskey)
			continue;

		result = dns_rdata_tostruct(&tuple->rdata, &dnskey, NULL);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);
		if ((dnskey.flags &
		     (DNS_KEYFLAG_OWNERMASK|DNS_KEYTYPE_NOAUTH))
			 != DNS_KEYOWNER_ZONE)
			continue;

		dns_rdata_toregion(&tuple->rdata, &r);

		keyid = dst_region_computeid(&r, dnskey.algorithm);

		buf[0] = dnskey.algorithm;
		buf[1] = (keyid & 0xff00) >> 8;
		buf[2] = (keyid & 0xff);
		buf[3] = (tuple->op == DNS_DIFFOP_ADD) ? 0 : 1;
		buf[4] = 0;
		rdata.data = buf;
		rdata.length = sizeof(buf);
		rdata.type = privatetype;
		rdata.rdclass = tuple->rdata.rdclass;

		if (sign_all || tuple->op == DNS_DIFFOP_DEL) {
			CHECK(rr_exists(db, ver, name, &rdata, &flag));
			if (flag)
				continue;
			CHECK(dns_difftuple_create(diff->mctx, DNS_DIFFOP_ADD,
						   name, 0, &rdata, &newtuple));
			CHECK(do_one_tuple(&newtuple, db, ver, diff));
			INSIST(newtuple == NULL);
		}

		/*
		 * Remove any record which says this operation has already
		 * completed.
		 */
		buf[4] = 1;
		CHECK(rr_exists(db, ver, name, &rdata, &flag));
		if (flag) {
			CHECK(dns_difftuple_create(diff->mctx, DNS_DIFFOP_DEL,
						   name, 0, &rdata, &newtuple));
			CHECK(do_one_tuple(&newtuple, db, ver, diff));
			INSIST(newtuple == NULL);
		}
	}
 failure:
	return (result);
}

static isc_result_t
sign_apex(dns_zone_t *zone, dns_db_t *db, dns_dbversion_t *ver,
	  dns_diff_t *diff, zonediff_t *zonediff)
{
	isc_result_t result;
	isc_stdtime_t now, inception, soaexpire;
	isc_boolean_t check_ksk, keyset_kskonly;
	dst_key_t *zone_keys[DNS_MAXZONEKEYS];
	unsigned int nkeys = 0, i;
	dns_difftuple_t *tuple;

	result = find_zone_keys(zone, db, ver, zone->mctx, DNS_MAXZONEKEYS,
				zone_keys, &nkeys);
	if (result != ISC_R_SUCCESS) {
		dns_zone_log(zone, ISC_LOG_ERROR,
			     "sign_apex:find_zone_keys -> %s",
			     dns_result_totext(result));
		return (result);
	}

	isc_stdtime_get(&now);
	inception = now - 3600;	/* Allow for clock skew. */
	soaexpire = now + dns_zone_getsigvalidityinterval(zone);

	check_ksk = DNS_ZONE_OPTION(zone, DNS_ZONEOPT_UPDATECHECKKSK);
	keyset_kskonly = DNS_ZONE_OPTION(zone, DNS_ZONEOPT_DNSKEYKSKONLY);

	/*
	 * See if update_sigs will update DNSKEY signature and if not
	 * cause them to sign so that so that newly activated keys
	 * are used.
	 */
	for (tuple = ISC_LIST_HEAD(diff->tuples);
	     tuple != NULL;
	     tuple = ISC_LIST_NEXT(tuple, link)) {
		if (tuple->rdata.type == dns_rdatatype_dnskey &&
		    dns_name_equal(&tuple->name, &zone->origin))
			break;
	}

	if (tuple == NULL) {
		result = del_sigs(zone, db, ver, &zone->origin,
				  dns_rdatatype_dnskey, zonediff,
				  zone_keys, nkeys, now, ISC_FALSE);
		if (result != ISC_R_SUCCESS) {
			dns_zone_log(zone, ISC_LOG_ERROR,
				     "sign_apex:del_sigs -> %s",
				     dns_result_totext(result));
			goto failure;
		}
		result = add_sigs(db, ver, &zone->origin, dns_rdatatype_dnskey,
				  zonediff->diff, zone_keys, nkeys, zone->mctx,
				  inception, soaexpire, check_ksk,
				  keyset_kskonly);
		if (result != ISC_R_SUCCESS) {
			dns_zone_log(zone, ISC_LOG_ERROR,
				     "sign_apex:add_sigs -> %s",
				     dns_result_totext(result));
			goto failure;
		}
	}

	result = update_sigs(diff, db, ver, zone_keys, nkeys, zone,
			     inception, soaexpire, now, check_ksk,
			     keyset_kskonly, zonediff);

	if (result != ISC_R_SUCCESS) {
		dns_zone_log(zone, ISC_LOG_ERROR,
			     "sign_apex:update_sigs -> %s",
			     dns_result_totext(result));
		goto failure;
	}

 failure:
	for (i = 0; i < nkeys; i++)
		dst_key_free(&zone_keys[i]);
	return (result);
}

/*
 * Prevent the zone entering a inconsistent state where
 * NSEC only DNSKEYs are present with NSEC3 chains.
 * See update.c:check_dnssec()
 */
static isc_boolean_t
dnskey_sane(dns_zone_t *zone, dns_db_t *db, dns_dbversion_t *ver,
	    dns_diff_t *diff)
{
	isc_result_t result;
	dns_difftuple_t *tuple;
	isc_boolean_t nseconly = ISC_FALSE, nsec3 = ISC_FALSE;
	dns_rdatatype_t privatetype = dns_zone_getprivatetype(zone);

	/* Scan the tuples for an NSEC-only DNSKEY */
	for (tuple = ISC_LIST_HEAD(diff->tuples);
	     tuple != NULL;
	     tuple = ISC_LIST_NEXT(tuple, link)) {
		isc_uint8_t alg;
		if (tuple->rdata.type != dns_rdatatype_dnskey ||
		    tuple->op != DNS_DIFFOP_ADD)
			continue;

		alg = tuple->rdata.data[3];
		if (alg == DST_ALG_RSAMD5 || alg == DST_ALG_RSASHA1 ||
		    alg == DST_ALG_DSA || alg == DST_ALG_ECC) {
			nseconly = ISC_TRUE;
			break;
		}
	}

	/* Check existing DB for NSEC-only DNSKEY */
	if (!nseconly) {
		result = dns_nsec_nseconly(db, ver, &nseconly);
		if (result == ISC_R_NOTFOUND)
			result = ISC_R_SUCCESS;
		CHECK(result);
	}

	/* Check existing DB for NSEC3 */
	if (!nsec3)
		CHECK(dns_nsec3_activex(db, ver, ISC_FALSE,
					privatetype, &nsec3));

	/* Refuse to allow NSEC3 with NSEC-only keys */
	if (nseconly && nsec3) {
		dns_zone_log(zone, ISC_LOG_ERROR,
			   "NSEC only DNSKEYs and NSEC3 chains not allowed");
		goto failure;
	}

	return (ISC_TRUE);

 failure:
	return (ISC_FALSE);
}

static isc_result_t
clean_nsec3param(dns_zone_t *zone, dns_db_t *db, dns_dbversion_t *ver,
		 dns_diff_t *diff)
{
	isc_result_t result;
	dns_dbnode_t *node = NULL;
	dns_rdataset_t rdataset;

	dns_rdataset_init(&rdataset);
	CHECK(dns_db_getoriginnode(db, &node));

	result = dns_db_findrdataset(db, node, ver, dns_rdatatype_dnskey,
				     dns_rdatatype_none, 0, &rdataset, NULL);
	if (dns_rdataset_isassociated(&rdataset))
		dns_rdataset_disassociate(&rdataset);
	if (result != ISC_R_NOTFOUND)
		goto failure;

	result = dns_nsec3param_deletechains(db, ver, zone, ISC_TRUE, diff);

 failure:
	if (node != NULL)
		dns_db_detachnode(db, &node);
	return (result);
}

/*
 * Given an RRSIG rdataset and an algorithm, determine whether there
 * are any signatures using that algorithm.
 */
static isc_boolean_t
signed_with_alg(dns_rdataset_t *rdataset, dns_secalg_t alg) {
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_rdata_rrsig_t rrsig;
	isc_result_t result;

	REQUIRE(rdataset == NULL || rdataset->type == dns_rdatatype_rrsig);
	if (rdataset == NULL || !dns_rdataset_isassociated(rdataset)) {
		return (ISC_FALSE);
	}

	for (result = dns_rdataset_first(rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(rdataset))
	{
		dns_rdataset_current(rdataset, &rdata);
		result = dns_rdata_tostruct(&rdata, &rrsig, NULL);
		RUNTIME_CHECK(result == ISC_R_SUCCESS);
		dns_rdata_reset(&rdata);
		if (rrsig.algorithm == alg)
			return (ISC_TRUE);
	}

	return (ISC_FALSE);
}

static isc_result_t
add_chains(dns_zone_t *zone, dns_db_t *db, dns_dbversion_t *ver,
	   dns_diff_t *diff)
{
	dns_name_t *origin;
	isc_boolean_t build_nsec3;
	isc_result_t result;

	origin = dns_db_origin(db);
	CHECK(dns_private_chains(db, ver, zone->privatetype, NULL,
				 &build_nsec3));
	if (build_nsec3)
		CHECK(dns_nsec3_addnsec3sx(db, ver, origin, zone->minimum,
					   ISC_FALSE, zone->privatetype, diff));
	CHECK(updatesecure(db, ver, origin, zone->minimum, ISC_TRUE, diff));

 failure:
	return (result);
}

static void
zone_rekey(dns_zone_t *zone) {
	isc_result_t result;
	dns_db_t *db = NULL;
	dns_dbnode_t *node = NULL;
	dns_dbversion_t *ver = NULL;
	dns_rdataset_t soaset, soasigs, keyset, keysigs;
	dns_dnsseckeylist_t dnskeys, keys, rmkeys;
	dns_dnsseckey_t *key;
	dns_diff_t diff, _sig_diff;
	zonediff_t zonediff;
	isc_boolean_t commit = ISC_FALSE, newactive = ISC_FALSE;
	isc_boolean_t newalg = ISC_FALSE;
	isc_boolean_t fullsign;
	dns_ttl_t ttl = 3600;
	const char *dir;
	isc_mem_t *mctx;
	isc_stdtime_t now;
	isc_time_t timenow;
	isc_interval_t ival;
	char timebuf[80];

	REQUIRE(DNS_ZONE_VALID(zone));

	ISC_LIST_INIT(dnskeys);
	ISC_LIST_INIT(keys);
	ISC_LIST_INIT(rmkeys);
	dns_rdataset_init(&soaset);
	dns_rdataset_init(&soasigs);
	dns_rdataset_init(&keyset);
	dns_rdataset_init(&keysigs);
	dir = dns_zone_getkeydirectory(zone);
	mctx = zone->mctx;
	dns_diff_init(mctx, &diff);
	dns_diff_init(mctx, &_sig_diff);
	zonediff_init(&zonediff, &_sig_diff);

	CHECK(dns_zone_getdb(zone, &db));
	CHECK(dns_db_newversion(db, &ver));
	CHECK(dns_db_getoriginnode(db, &node));

	TIME_NOW(&timenow);
	now = isc_time_seconds(&timenow);

	dns_zone_log(zone, ISC_LOG_INFO, "reconfiguring zone keys");

	/* Get the SOA record's TTL */
	CHECK(dns_db_findrdataset(db, node, ver, dns_rdatatype_soa,
				  dns_rdatatype_none, 0, &soaset, &soasigs));
	ttl = soaset.ttl;
	dns_rdataset_disassociate(&soaset);

	/* Get the DNSKEY rdataset */
	result = dns_db_findrdataset(db, node, ver, dns_rdatatype_dnskey,
				     dns_rdatatype_none, 0, &keyset, &keysigs);
	if (result == ISC_R_SUCCESS) {
		ttl = keyset.ttl;
		CHECK(dns_dnssec_keylistfromrdataset(&zone->origin, dir,
						     mctx, &keyset,
						     &keysigs, &soasigs,
						     ISC_FALSE, ISC_FALSE,
						     &dnskeys));
	} else if (result != ISC_R_NOTFOUND)
		goto failure;

	/*
	 * True when called from "rndc sign".  Indicates the zone should be
	 * fully signed now.
	 */
	fullsign = ISC_TF(DNS_ZONEKEY_OPTION(zone, DNS_ZONEKEY_FULLSIGN) != 0);

	result = dns_dnssec_findmatchingkeys(&zone->origin, dir, mctx, &keys);
	if (result == ISC_R_SUCCESS) {
		isc_boolean_t check_ksk;
		check_ksk = DNS_ZONE_OPTION(zone, DNS_ZONEOPT_UPDATECHECKKSK);

		result = dns_dnssec_updatekeys(&dnskeys, &keys, &rmkeys,
					       &zone->origin, ttl, &diff,
					       ISC_TF(!check_ksk),
					       mctx, logmsg);

		/* Keys couldn't be updated for some reason;
		 * try again later. */
		if (result != ISC_R_SUCCESS) {
			dns_zone_log(zone, ISC_LOG_ERROR, "zone_rekey:"
				     "couldn't update zone keys: %s",
				     isc_result_totext(result));
			goto failure;
		}

		/*
		 * See if any pre-existing keys have newly become active;
		 * also, see if any new key is for a new algorithm, as in that
		 * event, we need to sign the zone fully.  (If there's a new
		 * key, but it's for an already-existing algorithm, then
		 * the zone signing can be handled incrementally.)
		 */
		for (key = ISC_LIST_HEAD(dnskeys);
		     key != NULL;
		     key = ISC_LIST_NEXT(key, link)) {
			if (!key->first_sign)
				continue;

			newactive = ISC_TRUE;

			if (!dns_rdataset_isassociated(&keysigs)) {
				newalg = ISC_TRUE;
				break;
			}

			if (signed_with_alg(&keysigs, dst_key_alg(key->key))) {
				/*
				 * This isn't a new algorithm; clear
				 * first_sign so we won't sign the
				 * whole zone with this key later
				 */
				key->first_sign = ISC_FALSE;
			} else {
				newalg = ISC_TRUE;
				break;
			}
		}

		if ((newactive || fullsign || !ISC_LIST_EMPTY(diff.tuples)) &&
		    dnskey_sane(zone, db, ver, &diff)) {
			CHECK(dns_diff_apply(&diff, db, ver));
			CHECK(clean_nsec3param(zone, db, ver, &diff));
			CHECK(add_signing_records(db, zone->privatetype,
						  ver, &diff,
						  ISC_TF(newalg || fullsign)));
			CHECK(update_soa_serial(db, ver, &diff, mctx,
						zone->updatemethod));
			CHECK(add_chains(zone, db, ver, &diff));
			CHECK(sign_apex(zone, db, ver, &diff, &zonediff));
			CHECK(zone_journal(zone, zonediff.diff, NULL,
					   "zone_rekey"));
			commit = ISC_TRUE;
		}
	}

	dns_db_closeversion(db, &ver, ISC_TRUE);

	if (commit) {
		dns_difftuple_t *tuple;

		LOCK_ZONE(zone);
		DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_NEEDNOTIFY);

		zone_needdump(zone, DNS_DUMP_DELAY);

		zone_settimer(zone, &timenow);

		/* Remove any signatures from removed keys.  */
		if (!ISC_LIST_EMPTY(rmkeys)) {
			for (key = ISC_LIST_HEAD(rmkeys);
			     key != NULL;
			     key = ISC_LIST_NEXT(key, link)) {
				result = zone_signwithkey(zone,
							  dst_key_alg(key->key),
							  dst_key_id(key->key),
							  ISC_TRUE);
				if (result != ISC_R_SUCCESS) {
					dns_zone_log(zone, ISC_LOG_ERROR,
					     "zone_signwithkey failed: %s",
					     dns_result_totext(result));
				}
			}
		}

		if (fullsign) {
			/*
			 * "rndc sign" was called, so we now sign the zone
			 * with all active keys, whether they're new or not.
			 */
			for (key = ISC_LIST_HEAD(dnskeys);
			     key != NULL;
			     key = ISC_LIST_NEXT(key, link)) {
				if (!key->force_sign && !key->hint_sign)
					continue;

				result = zone_signwithkey(zone,
							  dst_key_alg(key->key),
							  dst_key_id(key->key),
							  ISC_FALSE);
				if (result != ISC_R_SUCCESS) {
					dns_zone_log(zone, ISC_LOG_ERROR,
					     "zone_signwithkey failed: %s",
					     dns_result_totext(result));
				}
			}
		} else if (newalg) {
			/*
			 * We haven't been told to sign fully, but a new
			 * algorithm was added to the DNSKEY.  We sign
			 * the full zone, but only with newly active
			 * keys.
			 */
			for (key = ISC_LIST_HEAD(dnskeys);
			     key != NULL;
			     key = ISC_LIST_NEXT(key, link)) {
				if (!key->first_sign)
					continue;

				result = zone_signwithkey(zone,
							  dst_key_alg(key->key),
							  dst_key_id(key->key),
							  ISC_FALSE);
				if (result != ISC_R_SUCCESS) {
					dns_zone_log(zone, ISC_LOG_ERROR,
					     "zone_signwithkey failed: %s",
					     dns_result_totext(result));
				}
			}
		}

		/*
		 * Clear fullsign flag, if it was set, so we don't do
		 * another full signing next time
		 */
		zone->keyopts &= ~DNS_ZONEKEY_FULLSIGN;

		/*
		 * Cause the zone to add/delete NSEC3 chains for the
		 * deferred NSEC3PARAM changes.
		 */
		for (tuple = ISC_LIST_HEAD(zonediff.diff->tuples);
		     tuple != NULL;
		     tuple = ISC_LIST_NEXT(tuple, link)) {
			unsigned char buf[DNS_NSEC3PARAM_BUFFERSIZE];
			dns_rdata_t rdata = DNS_RDATA_INIT;
			dns_rdata_nsec3param_t nsec3param;

			if (tuple->rdata.type != zone->privatetype ||
			    tuple->op != DNS_DIFFOP_ADD)
				continue;

			if (!dns_nsec3param_fromprivate(&tuple->rdata, &rdata,
							buf, sizeof(buf)))
				continue;
			result = dns_rdata_tostruct(&rdata, &nsec3param, NULL);
			RUNTIME_CHECK(result == ISC_R_SUCCESS);
			if (nsec3param.flags == 0)
				continue;

			result = zone_addnsec3chain(zone, &nsec3param);
			if (result != ISC_R_SUCCESS) {
				dns_zone_log(zone, ISC_LOG_ERROR,
					     "zone_addnsec3chain failed: %s",
					     dns_result_totext(result));
			}
		}

		/*
		 * Activate any NSEC3 chain updates that may have
		 * been scheduled before this rekey.
		 */
		if (fullsign || newalg)
			resume_addnsec3chain(zone);

		/*
		 * Schedule the next resigning event
		 */
		set_resigntime(zone);
		UNLOCK_ZONE(zone);
	}

	isc_time_settoepoch(&zone->refreshkeytime);

	/*
	 * If we're doing key maintenance, set the key refresh timer to
	 * the next scheduled key event or to 'dnssec-loadkeys-interval'
	 * seconds in the future, whichever is sooner.
	 */
	if (DNS_ZONEKEY_OPTION(zone, DNS_ZONEKEY_MAINTAIN)) {
		isc_time_t timethen;
		isc_stdtime_t then;

		LOCK_ZONE(zone);
		DNS_ZONE_TIME_ADD(&timenow, zone->refreshkeyinterval,
				  &timethen);
		zone->refreshkeytime = timethen;
		UNLOCK_ZONE(zone);

		for (key = ISC_LIST_HEAD(dnskeys);
		     key != NULL;
		     key = ISC_LIST_NEXT(key, link)) {
			then = now;
			result = next_keyevent(key->key, &then);
			if (result != ISC_R_SUCCESS)
				continue;

			DNS_ZONE_TIME_ADD(&timenow, then - now, &timethen);
			LOCK_ZONE(zone);
			if (isc_time_compare(&timethen,
					     &zone->refreshkeytime) < 0) {
				zone->refreshkeytime = timethen;
			}
			UNLOCK_ZONE(zone);
		}

		zone_settimer(zone, &timenow);

		isc_time_formattimestamp(&zone->refreshkeytime, timebuf, 80);
		dns_zone_log(zone, ISC_LOG_INFO, "next key event: %s", timebuf);
	}

 done:
	dns_diff_clear(&diff);
	dns_diff_clear(&_sig_diff);

	clear_keylist(&dnskeys, mctx);
	clear_keylist(&keys, mctx);
	clear_keylist(&rmkeys, mctx);

	if (ver != NULL)
		dns_db_closeversion(db, &ver, ISC_FALSE);
	if (dns_rdataset_isassociated(&keyset))
		dns_rdataset_disassociate(&keyset);
	if (dns_rdataset_isassociated(&keysigs))
		dns_rdataset_disassociate(&keysigs);
	if (dns_rdataset_isassociated(&soasigs))
		dns_rdataset_disassociate(&soasigs);
	if (node != NULL)
		dns_db_detachnode(db, &node);
	if (db != NULL)
		dns_db_detach(&db);

	INSIST(ver == NULL);
	return;

 failure:
	/*
	 * Something went wrong; try again in ten minutes or
	 * after a key refresh interval, whichever is shorter.
	 */
	isc_interval_set(&ival, ISC_MIN(zone->refreshkeyinterval, 600), 0);
	isc_time_nowplusinterval(&zone->refreshkeytime, &ival);
	goto done;
}

void
dns_zone_rekey(dns_zone_t *zone, isc_boolean_t fullsign) {
	isc_time_t now;

	if (zone->type == dns_zone_master && zone->task != NULL) {
		LOCK_ZONE(zone);

		if (fullsign)
			zone->keyopts |= DNS_ZONEKEY_FULLSIGN;

		TIME_NOW(&now);
		zone->refreshkeytime = now;
		zone_settimer(zone, &now);

		UNLOCK_ZONE(zone);
	}
}

isc_result_t
dns_zone_nscheck(dns_zone_t *zone, dns_db_t *db, dns_dbversion_t *version,
		 unsigned int *errors)
{
	isc_result_t result;
	dns_dbnode_t *node = NULL;

	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(errors != NULL);

	result = dns_db_getoriginnode(db, &node);
	if (result != ISC_R_SUCCESS)
		return (result);
	result = zone_count_ns_rr(zone, db, node, version, NULL, errors,
				  ISC_FALSE);
	dns_db_detachnode(db, &node);
	return (result);
}

isc_result_t
dns_zone_cdscheck(dns_zone_t *zone, dns_db_t *db, dns_dbversion_t *version) {
	isc_result_t result;
	dns_dbnode_t *node = NULL;
	dns_rdataset_t dnskey, cds, cdnskey;
	unsigned char buffer[DNS_DS_BUFFERSIZE];
	unsigned char algorithms[256];
	unsigned int i;

	REQUIRE(DNS_ZONE_VALID(zone));

	result = dns_db_getoriginnode(db, &node);
	if (result != ISC_R_SUCCESS)
		return (result);

	dns_rdataset_init(&cds);
	dns_rdataset_init(&dnskey);
	dns_rdataset_init(&cdnskey);

	result = dns_db_findrdataset(db, node, version, dns_rdatatype_cds,
				     dns_rdatatype_none, 0, &cds, NULL);
	if (result != ISC_R_SUCCESS && result != ISC_R_NOTFOUND)
		goto failure;

	result = dns_db_findrdataset(db, node, version, dns_rdatatype_cdnskey,
				     dns_rdatatype_none, 0, &cdnskey, NULL);
	if (result != ISC_R_SUCCESS && result != ISC_R_NOTFOUND)
		goto failure;

	if (!dns_rdataset_isassociated(&cds) &&
	    !dns_rdataset_isassociated(&cdnskey)) {
		result = ISC_R_SUCCESS;
		goto failure;
	}

	result = dns_db_findrdataset(db, node, version, dns_rdatatype_dnskey,
				     dns_rdatatype_none, 0, &dnskey, NULL);
	if (result == ISC_R_NOTFOUND) {
		if (dns_rdataset_isassociated(&cds))
			result = DNS_R_BADCDS;
		else
			result = DNS_R_BADCDNSKEY;
		goto failure;
	}
	if (result != ISC_R_SUCCESS)
		goto failure;

	/*
	 * For each DNSSEC algorithm in the CDS RRset there must be
	 * a matching DNSKEY record.
	 */
	if (dns_rdataset_isassociated(&cds)) {
		memset(algorithms, 0, sizeof(algorithms));
		for (result = dns_rdataset_first(&cds);
		     result == ISC_R_SUCCESS;
		     result = dns_rdataset_next(&cds)) {
			dns_rdata_t crdata = DNS_RDATA_INIT;
			dns_rdata_cds_t structcds;

			dns_rdataset_current(&cds, &crdata);
			CHECK(dns_rdata_tostruct(&crdata, &structcds, NULL));
			if (algorithms[structcds.algorithm] == 0)
				algorithms[structcds.algorithm] = 1;
			for (result = dns_rdataset_first(&dnskey);
			     result == ISC_R_SUCCESS;
			     result = dns_rdataset_next(&dnskey)) {
				dns_rdata_t rdata = DNS_RDATA_INIT;
				dns_rdata_t dsrdata = DNS_RDATA_INIT;

				dns_rdataset_current(&dnskey, &rdata);
				CHECK(dns_ds_buildrdata(&zone->origin, &rdata,
							structcds.digest_type,
							buffer, &dsrdata));
				if (crdata.length == dsrdata.length &&
				    memcmp(crdata.data, dsrdata.data,
					   dsrdata.length) == 0) {
					algorithms[structcds.algorithm] = 2;
				}
			}
			if (result != ISC_R_NOMORE)
				goto failure;
		}
		for (i = 0; i < sizeof(algorithms); i++) {
			if (algorithms[i] == 1) {
				result = DNS_R_BADCDNSKEY;
				goto failure;
			}
		}
	}

	/*
	 * For each DNSSEC algorithm in the CDNSKEY RRset there must be
	 * a matching DNSKEY record.
	 */
	if (dns_rdataset_isassociated(&cdnskey)) {
		memset(algorithms, 0, sizeof(algorithms));
		for (result = dns_rdataset_first(&cdnskey);
		     result == ISC_R_SUCCESS;
		     result = dns_rdataset_next(&cdnskey)) {
			dns_rdata_t crdata = DNS_RDATA_INIT;
			dns_rdata_cdnskey_t structcdnskey;

			dns_rdataset_current(&cdnskey, &crdata);
			CHECK(dns_rdata_tostruct(&crdata, &structcdnskey,
						 NULL));
			if (algorithms[structcdnskey.algorithm] == 0)
				algorithms[structcdnskey.algorithm] = 1;
			for (result = dns_rdataset_first(&dnskey);
			     result == ISC_R_SUCCESS;
			     result = dns_rdataset_next(&dnskey)) {
				dns_rdata_t rdata = DNS_RDATA_INIT;

				dns_rdataset_current(&dnskey, &rdata);
				if (crdata.length == rdata.length &&
				    memcmp(crdata.data, rdata.data,
					   rdata.length) == 0) {
					algorithms[structcdnskey.algorithm] = 2;
				}
			}
			if (result != ISC_R_NOMORE)
				goto failure;
		}
		for (i = 0; i < sizeof(algorithms); i++) {
			if (algorithms[i] == 1) {
				result = DNS_R_BADCDS;
				goto failure;
			}
		}
	}
	result = ISC_R_SUCCESS;

 failure:
	if (dns_rdataset_isassociated(&cds))
		dns_rdataset_disassociate(&cds);
	if (dns_rdataset_isassociated(&dnskey))
		dns_rdataset_disassociate(&dnskey);
	if (dns_rdataset_isassociated(&cdnskey))
		dns_rdataset_disassociate(&cdnskey);
	dns_db_detachnode(db, &node);
	return (result);
}

void
dns_zone_setadded(dns_zone_t *zone, isc_boolean_t added) {
	REQUIRE(DNS_ZONE_VALID(zone));
	LOCK_ZONE(zone);
	zone->added = added;
	UNLOCK_ZONE(zone);
}

isc_boolean_t
dns_zone_getadded(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));
	return (zone->added);
}

isc_result_t
dns_zone_dlzpostload(dns_zone_t *zone, dns_db_t *db)
{
	isc_time_t loadtime;
	isc_result_t result;
	dns_zone_t *secure = NULL;

	TIME_NOW(&loadtime);

	/*
	 * Lock hierarchy: zmgr, zone, raw.
	 */
 again:
	LOCK_ZONE(zone);
	if (inline_secure(zone))
		LOCK_ZONE(zone->raw);
	else if (inline_raw(zone)) {
		secure = zone->secure;
		TRYLOCK_ZONE(result, secure);
		if (result != ISC_R_SUCCESS) {
			UNLOCK_ZONE(zone);
			secure = NULL;
#if ISC_PLATFORM_USETHREADS
			isc_thread_yield();
#endif
			goto again;
		}
	}
	result = zone_postload(zone, db, loadtime, ISC_R_SUCCESS);
	if (inline_secure(zone))
		UNLOCK_ZONE(zone->raw);
	else if (secure != NULL)
		UNLOCK_ZONE(secure);
	UNLOCK_ZONE(zone);
	return result;
}

isc_result_t
dns_zone_setrefreshkeyinterval(dns_zone_t *zone, isc_uint32_t interval) {
	REQUIRE(DNS_ZONE_VALID(zone));
	if (interval == 0)
		return (ISC_R_RANGE);
	/* Maximum value: 24 hours (3600 minutes) */
	if (interval > (24 * 60))
		interval = (24 * 60);
	/* Multiply by 60 for seconds */
	zone->refreshkeyinterval = interval * 60;
	return (ISC_R_SUCCESS);
}

void
dns_zone_setrequestixfr(dns_zone_t *zone, isc_boolean_t flag) {
	REQUIRE(DNS_ZONE_VALID(zone));
	zone->requestixfr = flag;
}

isc_boolean_t
dns_zone_getrequestixfr(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));
	return (zone->requestixfr);
}

void
dns_zone_setserialupdatemethod(dns_zone_t *zone, dns_updatemethod_t method) {
	REQUIRE(DNS_ZONE_VALID(zone));
	zone->updatemethod = method;
}

dns_updatemethod_t
dns_zone_getserialupdatemethod(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));
	return(zone->updatemethod);
}

/*
 * Lock hierarchy: zmgr, zone, raw.
 */
isc_result_t
dns_zone_link(dns_zone_t *zone, dns_zone_t *raw) {
	isc_result_t result;
	dns_zonemgr_t *zmgr;

	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(zone->zmgr != NULL);
	REQUIRE(zone->task != NULL);
	REQUIRE(zone->loadtask != NULL);
	REQUIRE(zone->raw == NULL);

	REQUIRE(DNS_ZONE_VALID(raw));
	REQUIRE(raw->zmgr == NULL);
	REQUIRE(raw->task == NULL);
	REQUIRE(raw->loadtask == NULL);
	REQUIRE(raw->secure == NULL);

	/*
	 * Lock hierarchy: zmgr, zone, raw.
	 */
	zmgr = zone->zmgr;
	RWLOCK(&zmgr->rwlock, isc_rwlocktype_write);
	LOCK_ZONE(zone);
	LOCK_ZONE(raw);

	result = isc_timer_create(zmgr->timermgr, isc_timertype_inactive,
				  NULL, NULL, zone->task, zone_timer, raw,
				  &raw->timer);
	if (result != ISC_R_SUCCESS)
		goto unlock;

	/*
	 * The timer "holds" a iref.
	 */
	raw->irefs++;
	INSIST(raw->irefs != 0);


	/* dns_zone_attach(raw, &zone->raw); */
	isc_refcount_increment(&raw->erefs, NULL);
	zone->raw = raw;

	/* dns_zone_iattach(zone,  &raw->secure); */
	zone_iattach(zone, &raw->secure);

	isc_task_attach(zone->task, &raw->task);
	isc_task_attach(zone->loadtask, &raw->loadtask);

	ISC_LIST_APPEND(zmgr->zones, raw, link);
	raw->zmgr = zmgr;
	zmgr->refs++;

 unlock:
	UNLOCK_ZONE(raw);
	UNLOCK_ZONE(zone);
	RWUNLOCK(&zmgr->rwlock, isc_rwlocktype_write);
	return (result);
}

void
dns_zone_getraw(dns_zone_t *zone, dns_zone_t **raw) {
	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(raw != NULL && *raw == NULL);

	LOCK(&zone->lock);
	if (zone->raw != NULL)
		dns_zone_attach(zone->raw, raw);
	UNLOCK(&zone->lock);
}

struct keydone {
	isc_event_t event;
	isc_boolean_t all;
	unsigned char data[5];
};

#define PENDINGFLAGS (DNS_NSEC3FLAG_CREATE|DNS_NSEC3FLAG_INITIAL)

static void
keydone(isc_task_t *task, isc_event_t *event) {
	const char *me = "keydone";
	isc_boolean_t commit = ISC_FALSE;
	isc_result_t result;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	dns_dbversion_t *oldver = NULL, *newver = NULL;
	dns_zone_t *zone;
	dns_db_t *db = NULL;
	dns_dbnode_t *node = NULL;
	dns_rdataset_t rdataset;
	dns_diff_t diff;
	struct keydone *kd = (struct keydone *)event;
	dns_update_log_t log = { update_log_cb, NULL };
	isc_boolean_t clear_pending = ISC_FALSE;

	UNUSED(task);

	zone = event->ev_arg;
	INSIST(DNS_ZONE_VALID(zone));

	ENTER;

	dns_rdataset_init(&rdataset);
	dns_diff_init(zone->mctx, &diff);

	ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_read);
	if (zone->db != NULL) {
		dns_db_attach(zone->db, &db);
		dns_db_currentversion(db, &oldver);
		result = dns_db_newversion(db, &newver);
		if (result != ISC_R_SUCCESS) {
			dns_zone_log(zone, ISC_LOG_ERROR,
				     "keydone:dns_db_newversion -> %s",
				     dns_result_totext(result));
			goto failure;
		}
	}
	ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);
	if (db == NULL)
		goto failure;

	result = dns_db_getoriginnode(db, &node);
	if (result != ISC_R_SUCCESS)
		goto failure;

	result = dns_db_findrdataset(db, node, newver, zone->privatetype,
				     dns_rdatatype_none, 0, &rdataset, NULL);
	if (result == ISC_R_NOTFOUND) {
		INSIST(!dns_rdataset_isassociated(&rdataset));
		goto failure;
	}
	if (result != ISC_R_SUCCESS) {
		INSIST(!dns_rdataset_isassociated(&rdataset));
		goto failure;
	}

	for (result = dns_rdataset_first(&rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(&rdataset)) {
		isc_boolean_t found = ISC_FALSE;

		dns_rdataset_current(&rdataset, &rdata);

		if (kd->all) {
			if (rdata.length == 5 && rdata.data[0] != 0 &&
			       rdata.data[3] == 0 && rdata.data[4] == 1)
				found = ISC_TRUE;
			else if (rdata.data[0] == 0 &&
				 (rdata.data[2] & PENDINGFLAGS) != 0) {
				found = ISC_TRUE;
				clear_pending = ISC_TRUE;
			}
		} else if (rdata.length == 5 &&
			   memcmp(rdata.data, kd->data, 5) == 0)
			found = ISC_TRUE;

		if (found)
			CHECK(update_one_rr(db, newver, &diff, DNS_DIFFOP_DEL,
					    &zone->origin, rdataset.ttl,
					    &rdata));
		dns_rdata_reset(&rdata);
	}

	if (!ISC_LIST_EMPTY(diff.tuples)) {
		/* Write changes to journal file. */
		CHECK(update_soa_serial(db, newver, &diff, zone->mctx,
					zone->updatemethod));

		result = dns_update_signatures(&log, zone, db,
					       oldver, newver, &diff,
					       zone->sigvalidityinterval);
		if (!clear_pending)
			CHECK(result);

		CHECK(zone_journal(zone, &diff, NULL, "keydone"));
		commit = ISC_TRUE;

		LOCK_ZONE(zone);
		DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_LOADED);
		zone_needdump(zone, 30);
		UNLOCK_ZONE(zone);
	}

 failure:
	if (dns_rdataset_isassociated(&rdataset))
		dns_rdataset_disassociate(&rdataset);
	if (db != NULL) {
		if (node != NULL)
			dns_db_detachnode(db, &node);
		if (oldver != NULL)
			dns_db_closeversion(db, &oldver, ISC_FALSE);
		if (newver != NULL)
			dns_db_closeversion(db, &newver, commit);
		dns_db_detach(&db);
	}
	dns_diff_clear(&diff);
	isc_event_free(&event);
	dns_zone_idetach(&zone);

	INSIST(oldver == NULL);
	INSIST(newver == NULL);
}

isc_result_t
dns_zone_keydone(dns_zone_t *zone, const char *keystr) {
	isc_result_t result = ISC_R_SUCCESS;
	isc_event_t *e;
	isc_buffer_t b;
	dns_zone_t *dummy = NULL;
	struct keydone *kd;

	REQUIRE(DNS_ZONE_VALID(zone));

	LOCK_ZONE(zone);

	e = isc_event_allocate(zone->mctx, zone, DNS_EVENT_KEYDONE, keydone,
			       zone, sizeof(struct keydone));
	if (e == NULL) {
		result = ISC_R_NOMEMORY;
		goto failure;
	}

	kd = (struct keydone *) e;
	if (strcasecmp(keystr, "all") == 0)
		kd->all = ISC_TRUE;
	else {
		isc_textregion_t r;
		char *algstr;
		dns_keytag_t keyid;
		dns_secalg_t alg;
		size_t n;

		kd->all = ISC_FALSE;

		n = sscanf(keystr, "%hd/", &keyid);
		if (n == 0U)
			CHECK(ISC_R_FAILURE);

		algstr = strchr(keystr, '/');
		if (algstr != NULL)
			algstr++;
		else
			CHECK(ISC_R_FAILURE);

		n = sscanf(algstr, "%hhd", &alg);
		if (n == 0U) {
			DE_CONST(algstr, r.base);
			r.length = strlen(algstr);
			CHECK(dns_secalg_fromtext(&alg, &r));
		}

		/* construct a private-type rdata */
		isc_buffer_init(&b, kd->data, sizeof(kd->data));
		isc_buffer_putuint8(&b, alg);
		isc_buffer_putuint8(&b, (keyid & 0xff00) >> 8);
		isc_buffer_putuint8(&b, (keyid & 0xff));
		isc_buffer_putuint8(&b, 0);
		isc_buffer_putuint8(&b, 1);
	}

	zone_iattach(zone, &dummy);
	isc_task_send(zone->task, &e);

 failure:
	if (e != NULL)
		isc_event_free(&e);
	UNLOCK_ZONE(zone);
	return (result);
}

static void
setnsec3param(isc_task_t *task, isc_event_t *event) {
	const char *me = "setnsec3param";
	isc_boolean_t commit = ISC_FALSE;
	isc_result_t result;
	dns_dbversion_t *oldver = NULL, *newver = NULL;
	dns_zone_t *zone;
	dns_db_t *db = NULL;
	dns_dbnode_t *node = NULL;
	dns_rdataset_t prdataset, nrdataset;
	dns_diff_t diff;
	struct np3event *npe = (struct np3event *)event;
	nsec3param_t *np;
	dns_update_log_t log = { update_log_cb, NULL };
	dns_rdata_t rdata;
	isc_boolean_t nseconly;
	isc_boolean_t exists = ISC_FALSE;

	UNUSED(task);

	zone = event->ev_arg;
	INSIST(DNS_ZONE_VALID(zone));

	ENTER;

	np = &npe->params;

	dns_rdataset_init(&prdataset);
	dns_rdataset_init(&nrdataset);
	dns_diff_init(zone->mctx, &diff);

	ZONEDB_LOCK(&zone->dblock, isc_rwlocktype_read);
	if (zone->db != NULL) {
		dns_db_attach(zone->db, &db);
		dns_db_currentversion(db, &oldver);
		result = dns_db_newversion(db, &newver);
		if (result != ISC_R_SUCCESS) {
			dns_zone_log(zone, ISC_LOG_ERROR,
				     "setnsec3param:dns_db_newversion -> %s",
				     dns_result_totext(result));
			goto failure;
		}
	}
	ZONEDB_UNLOCK(&zone->dblock, isc_rwlocktype_read);
	if (db == NULL)
		goto failure;

	CHECK(dns_db_getoriginnode(db, &node));

	/*
	 * Does a private-type record already exist for this chain?
	 */
	result = dns_db_findrdataset(db, node, newver, zone->privatetype,
				     dns_rdatatype_none, 0, &prdataset, NULL);
	if (result == ISC_R_SUCCESS) {
		for (result = dns_rdataset_first(&prdataset);
		     result == ISC_R_SUCCESS;
		     result = dns_rdataset_next(&prdataset)) {
			dns_rdata_init(&rdata);
			dns_rdataset_current(&prdataset, &rdata);

			if (np->length == rdata.length &&
			    memcmp(rdata.data, np->data, np->length) == 0) {
				exists = ISC_TRUE;
				break;
			}
		}
	} else if (result != ISC_R_NOTFOUND) {
		INSIST(!dns_rdataset_isassociated(&prdataset));
		goto failure;
	}

	/*
	 * Does the chain already exist?
	 */
	result = dns_db_findrdataset(db, node, newver,
				     dns_rdatatype_nsec3param,
				     dns_rdatatype_none, 0, &nrdataset, NULL);
	if (result == ISC_R_SUCCESS) {
		for (result = dns_rdataset_first(&nrdataset);
		     result == ISC_R_SUCCESS;
		     result = dns_rdataset_next(&nrdataset)) {
			dns_rdata_init(&rdata);
			dns_rdataset_current(&nrdataset, &rdata);

			if (np->length == (rdata.length + 1) &&
			    memcmp(rdata.data, np->data + 1,
				   np->length - 1) == 0)
			{
				exists = ISC_TRUE;
				break;
			}
		}
	} else if (result != ISC_R_NOTFOUND) {
		INSIST(!dns_rdataset_isassociated(&nrdataset));
		goto failure;
	}


	/*
	 * We need to remove any existing NSEC3 chains.
	 */
	if (!exists && np->replace && (np->length != 0 || np->nsec))
		CHECK(dns_nsec3param_deletechains(db, newver, zone,
						  !np->nsec, &diff));

	if (!exists && np->length != 0) {
		/*
		 * We're creating an NSEC3 chain.
		 *
		 * If the zone is not currently capable of supporting
		 * an NSEC3 chain, add the INITIAL flag, so these
		 * parameters can be used later when NSEC3 becomes
		 * available.
		 */
		dns_rdata_init(&rdata);

		np->data[2] |= DNS_NSEC3FLAG_CREATE;
		result = dns_nsec_nseconly(db, newver, &nseconly);
		if (result == ISC_R_NOTFOUND || nseconly)
			np->data[2] |= DNS_NSEC3FLAG_INITIAL;

		rdata.length = np->length;
		rdata.data = np->data;
		rdata.type = zone->privatetype;
		rdata.rdclass = zone->rdclass;
		CHECK(update_one_rr(db, newver, &diff, DNS_DIFFOP_ADD,
				    &zone->origin, 0, &rdata));
	}

	if (!ISC_LIST_EMPTY(diff.tuples)) {
		/* Write changes to journal file. */
		CHECK(update_soa_serial(db, newver, &diff, zone->mctx,
					zone->updatemethod));
		result = dns_update_signatures(&log, zone, db,
					       oldver, newver, &diff,
					       zone->sigvalidityinterval);
		if (result != ISC_R_NOTFOUND)
			CHECK(result);
		CHECK(zone_journal(zone, &diff, NULL, "setnsec3param"));
		commit = ISC_TRUE;

		LOCK_ZONE(zone);
		DNS_ZONE_SETFLAG(zone, DNS_ZONEFLG_LOADED);
		zone_needdump(zone, 30);
		UNLOCK_ZONE(zone);
	}

 failure:
	if (dns_rdataset_isassociated(&prdataset))
		dns_rdataset_disassociate(&prdataset);
	if (dns_rdataset_isassociated(&nrdataset))
		dns_rdataset_disassociate(&nrdataset);
	if (node != NULL)
		dns_db_detachnode(db, &node);
	if (oldver != NULL)
		dns_db_closeversion(db, &oldver, ISC_FALSE);
	if (newver != NULL)
		dns_db_closeversion(db, &newver, commit);
	if (db != NULL)
		dns_db_detach(&db);
	if (commit)
		resume_addnsec3chain(zone);
	dns_diff_clear(&diff);
	isc_event_free(&event);
	dns_zone_idetach(&zone);

	INSIST(oldver == NULL);
	INSIST(newver == NULL);
}

isc_result_t
dns_zone_setnsec3param(dns_zone_t *zone, isc_uint8_t hash, isc_uint8_t flags,
		       isc_uint16_t iter, isc_uint8_t saltlen,
		       unsigned char *salt, isc_boolean_t replace)
{
	isc_result_t result = ISC_R_SUCCESS;
	dns_rdata_nsec3param_t param;
	dns_rdata_t nrdata = DNS_RDATA_INIT;
	dns_rdata_t prdata = DNS_RDATA_INIT;
	unsigned char nbuf[DNS_NSEC3PARAM_BUFFERSIZE];
	struct np3event *npe;
	nsec3param_t *np;
	dns_zone_t *dummy = NULL;
	isc_buffer_t b;
	isc_event_t *e;

	REQUIRE(DNS_ZONE_VALID(zone));
	REQUIRE(salt != NULL);

	LOCK_ZONE(zone);

	e = isc_event_allocate(zone->mctx, zone, DNS_EVENT_SETNSEC3PARAM,
			       setnsec3param, zone, sizeof(struct np3event));
	if (e == NULL) {
		result = ISC_R_NOMEMORY;
		goto failure;
	}

	npe = (struct np3event *) e;
	np = &npe->params;

	np->replace = replace;
	if (hash == 0) {
		np->length = 0;
		np->nsec = ISC_TRUE;
	} else {
		param.common.rdclass = zone->rdclass;
		param.common.rdtype = dns_rdatatype_nsec3param;
		ISC_LINK_INIT(&param.common, link);
		param.mctx = NULL;
		param.hash = hash;
		param.flags = flags;
		param.iterations = iter;
		param.salt_length = saltlen;
		param.salt = salt;
		isc_buffer_init(&b, nbuf, sizeof(nbuf));
		CHECK(dns_rdata_fromstruct(&nrdata, zone->rdclass,
					   dns_rdatatype_nsec3param,
					   &param, &b));
		dns_nsec3param_toprivate(&nrdata, &prdata, zone->privatetype,
					 np->data, sizeof(np->data));
		np->length = prdata.length;
	}

	zone_iattach(zone, &dummy);
	isc_task_send(zone->task, &e);

 failure:
	if (e != NULL)
		isc_event_free(&e);
	UNLOCK_ZONE(zone);
	return (result);
}

void
dns_zone_setstatlevel(dns_zone_t *zone, dns_zonestat_level_t level) {
	REQUIRE(DNS_ZONE_VALID(zone));

	zone->statlevel = level;
}

dns_zonestat_level_t
dns_zone_getstatlevel(dns_zone_t *zone) {
	REQUIRE(DNS_ZONE_VALID(zone));

	return (zone->statlevel);
}
