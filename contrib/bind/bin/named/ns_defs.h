/*
 *	from ns.h	4.33 (Berkeley) 8/23/90
 *	$Id: ns_defs.h,v 8.121 2002/06/26 03:27:19 marka Exp $
 */

/*
 * Copyright (c) 1986
 *    The Regents of the University of California.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * Portions Copyright (c) 1996-2000 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * Portions Copyright (c) 1999 by Check Point Software Technologies, Inc.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Check Point Software Technologies Incorporated not be used 
 * in advertising or publicity pertaining to distribution of the document 
 * or software without specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND CHECK POINT SOFTWARE TECHNOLOGIES 
 * INCORPORATED DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.   
 * IN NO EVENT SHALL CHECK POINT SOFTWARE TECHNOLOGIES INCORPRATED
 * BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR 
 * ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT 
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Global definitions for the name server.
 */

/*
 * Effort has been expended here to make all structure members 32 bits or
 * larger land on 32-bit boundaries; smaller structure members have been
 * deliberately shuffled and smaller integer sizes chosen where possible
 * to make sure this happens.  This is all meant to avoid structure member
 * padding which can cost a _lot_ of memory when you have hundreds of 
 * thousands of entries in your cache.
 */

/*
 * Timeout time should be around 1 minute or so.  Using the
 * the current simplistic backoff strategy, the sequence
 * retrys after 4, 8, and 16 seconds.  With 3 servers, this
 * dies out in a little more than a minute.
 * (sequence RETRYBASE, 2*RETRYBASE, 4*RETRYBASE... for MAXRETRY)
 */
#define	NEWZONES	64	/* how many zones to grow the zone table by */
#define	INITIALZONES	NEWZONES /* how many zones are allocated initially */
#define MINROOTS	2	/* min number of root hints */
#define NSMAX		16	/* max number of NS addrs to try ([0..255]) */
#define RETRYBASE	4 	/* base time between retries */
#define	MAXCLASS	255	/* XXX - may belong elsewhere */
#define MAXRETRY	3	/* max number of retries per addr */
#define MAXCNAMES	8	/* max # of CNAMES tried per addr */
#define MAXQUERIES	20	/* max # of queries to be made */
#define	MAXQSERIAL	4	/* max # of outstanding QSERIAL's */
				/* (prevent "recursive" loops) */
#define	INIT_REFRESH	600	/* retry time for initial slave */
				/* contact (10 minutes) */
#define MIN_REFRESH	2	/* never refresh more frequently than once */
				/* every MIN_REFRESH seconds */
#define MIN_RETRY	1	/* never retry more frequently than once */
				/* every MIN_RETRY seconds */
#define MAX_REFRESH	2419200	/* perform a refresh query at least */
				/* every 4 weeks*/
#define MAX_RETRY	1209600	/* perform a retry after no more than 2 weeks */
#define MAX_EXPIRE	31536000 /* expire a zone if we have not talked to */
				/* the primary in 1 year */
#define NADDRECS	20	/* max addt'l rr's per resp */

#define XFER_TIMER	120	/* named-xfer's connect timeout */
#define MAX_XFER_TIME	60*60*2	/* default max seconds for an xfer */
#define XFER_TIME_FUDGE	10	/* MAX_XFER_TIME fudge */
#define MAX_XFERS_RUNNING 20	/* max value of transfers_in */
#define DEFAULT_XFERS_RUNNING 10  /* default value of transfers_in */
#define DEFAULT_XFERS_PER_NS 2	  /* default # of xfers per peer nameserver */
#define	XFER_BUFSIZE	(16*1024) /* arbitrary but bigger than most MTU's */
#define	MAX_SYNCDELAY	3	/* Presumed timeout in use by our clients. */
#define	MAX_SYNCDRAIN	100000	/* How long we'll spin in drain_all_rcvbuf. */
#define	MAX_SYNCSTORE	500
#define NS_MAX_DISTANCE 3	/* maximum nameserver chaining before failure */

				  /* maximum time to cache negative answers */
#define DEFAULT_MAX_NCACHE_TTL (3*60*60)

#define ALPHA    0.7	/* How much to preserve of old response time */
#define	BETA	 1.2	/* How much to penalize response time on failure */
#define	GAMMA	 0.98	/* How much to decay unused response times */

	/* What maintainance operations need to be performed sometime soon? */
typedef enum need {
	main_need_zreload = 0,	/* ns_zreload() needed. */
	main_need_reload,	/* ns_reload() needed. */
	main_need_reconfig,	/* ns_reconfig() needed. */
	main_need_endxfer,	/* endxfer() needed. */
	main_need_zoneload,	/* loadxfer() needed. */
	main_need_dump,		/* doadump() needed. */
	main_need_statsdump,	/* ns_stats() needed. */
	main_need_statsdumpandclear, /* ns_stats() needed. */
	main_need_exit,		/* exit() needed. */
	main_need_qrylog,	/* toggle_qrylog() needed. */
	main_need_debug,	/* use_desired_debug() needed. */
	main_need_restart,	/* exec() needed. */
	main_need_reap,		/* need to reap dead children. */
	main_need_noexpired,	/* ns_reconfig() needed w/ noexpired set. */
	main_need_tryxfer,	/* attemt to start a zone transfer. */
	main_need_num		/* MUST BE LAST. */
} main_need;

	/* What global options are set? */
#define	OPTION_NORECURSE	0x00000001 /* Don't recurse even if asked. */
#define	OPTION_NOFETCHGLUE	0x00000002 /* Don't fetch missing glue. */
#define	OPTION_FORWARD_ONLY	0x00000004 /* Don't use NS RR's, just forward. */
#define	OPTION_FAKE_IQUERY	0x00000008 /* Fake up bogus response to IQUERY. */
#ifdef BIND_NOTIFY
/* #define	OPTION_NONOTIFY		0x00000010 */ /* Turn off notify */
#define	OPTION_SUPNOTIFY_INITIAL 0x00000020 /* Supress initial notify */
#endif
#define	OPTION_NONAUTH_NXDOMAIN	0x00000040 /* Generate non-auth NXDOMAINs? */
#define	OPTION_MULTIPLE_CNAMES	0x00000080 /* Allow a name to have multiple
					    * CNAME RRs */
#define OPTION_HOSTSTATS	0x00000100 /* Maintain per-host statistics? */
#define OPTION_DEALLOC_ON_EXIT	0x00000200 /* Deallocate everything on exit? */
#define OPTION_NODIALUP		0x00000400 /* Turn off dialup support */
#define OPTION_NORFC2308_TYPE1	0x00000800 /* Prevent type1 respones (RFC 2308)
					    * to cached negative respones */
#define	OPTION_USE_ID_POOL	0x00001000 /* Use the memory hogging query ID */
#define	OPTION_TREAT_CR_AS_SPACE 0x00002000 /* Treat CR in zone files as
					     * space */
#define OPTION_USE_IXFR		0x00004000 /* Use by default ixfr in zone
					    * transfer */
#define OPTION_MAINTAIN_IXFR_BASE 0x00008000 /* Part of IXFR file name logic. */
#define OPTION_HITCOUNT		0x00010000 /* Keep track of each time an
					    * RR gets hit in the database */

#define	DEFAULT_OPTION_FLAGS	(OPTION_NODIALUP|OPTION_NONAUTH_NXDOMAIN|\
				 OPTION_USE_ID_POOL|OPTION_NORFC2308_TYPE1)

#ifdef BIND_UPDATE
#define SOAINCRINTVL 300 /* default value for the time after which
			  * the zone serial number must be incremented
			  * after a successful update has occurred */
#define DUMPINTVL 3600   /* default interval at which to dump changed zones
			  * randomized, not exact */
#define DEFERUPDCNT	100	/* default number of updates that can happen
				 * before the zone serial number will be
				 * incremented */
#define UPDATE_TIMER XFER_TIMER
#endif /* BIND_UPDATE */

#define	USE_MINIMUM	0xffffffff
#define	MAXIMUM_TTL	0x7fffffff

#define CLEAN_TIMER		0x01
#define INTERFACE_TIMER		0x02
#define STATS_TIMER		0x04
#define HEARTBEAT_TIMER		0x08

	/* IP address accessor, network byte order. */
#define	ina_ulong(ina)	(ina.s_addr)

	/* IP address accessor, host byte order, read only. */
#define	ina_hlong(ina)	ntohl(ina.s_addr)

	/* IP address equality. */
	/* XXX: assumes that network byte order won't affect equality. */
#define	ina_equal(a, b)	(ina_ulong(a) == ina_ulong(b))

	/* IP address equality with a mask. */
#define	ina_onnet(h, n, m) ((ina_ulong(h) & ina_ulong(m)) == ina_ulong(n))

	/* Sequence space arithmetic. */
#define SEQ_GT(a,b)	((int32_t)((a)-(b)) > 0)
#define SEQ_LT(a,b)	((int32_t)((a)-(b)) < 0)

#define	NS_OPTION_P(option) ((server_options == NULL) ? \
		(panic(panic_msg_no_options, NULL), 0) : \
		((server_options->flags & option) != 0))

#define	NS_ZOPTION_P(zp, option) \
		(((zp) != NULL && (((zp)->z_optset & option) != 0)) ? \
		(((zp)->z_options & option) != 0) : NS_OPTION_P(option))

#define	NS_ZFWDTAB(zp)	(((zp) == NULL) ? \
		server_options->fwdtab : (zp)->z_fwdtab)

#define	NS_INCRSTAT(addr, which) \
	do { \
		if ((int)which >= (int)nssLast) \
			ns_panic(ns_log_insist, 1, panic_msg_bad_which, \
				 __FILE__, __LINE__, #which); \
		else { \
			if (NS_OPTION_P(OPTION_HOSTSTATS)) { \
				struct nameser *ns = \
					nameserFind(addr, NS_F_INSERT); \
				if (ns != NULL) \
					ns->stats[(int)which]++; \
			} \
			globalStats[(int)which]++; \
		} \
	} while (0)

enum severity { ignore, warn, fail, not_set };

#ifdef BIND_NOTIFY
enum notify { notify_use_default=0, notify_yes, notify_no, notify_explicit };
#endif

enum zdialup { zdialup_use_default=0, zdialup_yes, zdialup_no };

enum axfr_format { axfr_use_default=0, axfr_one_answer, axfr_many_answers };

struct ip_match_direct {
	struct in_addr address;
	struct in_addr mask;
};

struct ip_match_indirect {
	struct ip_match_list *list;
};

struct ip_match_key {
	struct dst_key *key;
};

typedef enum { ip_match_pattern, ip_match_indirect, ip_match_localhost,
	       ip_match_localnets, ip_match_key } ip_match_type;

typedef struct ip_match_element {
	ip_match_type type;
	u_int flags;
	union {
		struct ip_match_direct direct;
		struct ip_match_indirect indirect;
		struct ip_match_key key;
	} u;
	struct ip_match_element *next;
} *ip_match_element;

/* Flags for ip_match_element */
#define IP_MATCH_NEGATE			0x01	/* match means deny access */

typedef struct ip_match_list {
	ip_match_element first;
	ip_match_element last;
} *ip_match_list;

typedef struct ztimer_info {
	char *name;
	int class;
	int type;
} *ztimer_info;

/*
 * These fields are ordered to maintain word-alignment;
 * be careful about changing them.
 */
struct zoneinfo {
	char		*z_origin;	/* root domain name of zone */
	time_t		z_time;		/* time for next refresh */
	time_t		z_lastupdate;	/* time of last soa serial increment */
	u_int32_t	z_refresh;	/* refresh interval */
	u_int32_t	z_retry;	/* refresh retry interval */
	u_int32_t	z_expire;	/* expiration time for cached info */
	u_int32_t	z_minimum;	/* minimum TTL value */
	u_int32_t	z_serial;	/* changes if zone modified */
	char		*z_source;	/* source location of data */
	time_t		z_ftime;	/* modification time of source file */
	struct in_addr	z_axfr_src;	/* bind() the axfr socket to this */
	struct in_addr	z_addr[NSMAX];	/* list of master servers for zone */
	struct dst_key * z_keys[NSMAX];	/* tsig key associated with master */
	u_char		z_addrcnt;	/* number of entries in z_addr[] */
	struct in_addr	z_xaddr[NSMAX];	/* list of master servers for xfer */
	u_char		z_xaddrcnt;	/* number of entries in z_xaddr[] */
	u_char		z_type;		/* type of zone; see below */
	u_int32_t	z_flags;	/* state bits; see below */
	pid_t		z_xferpid;	/* xfer child pid */
	u_int		z_options;	/* options set specific to this zone */
	u_int		z_optset;	/* which opts override global opts */
	int		z_class;	/* class of zone */
	int		z_numxfrs;	/* Ref count of concurrent xfrs. */
	enum severity	z_checknames;	/* How to handle non-RFC-compliant names */
#ifdef BIND_UPDATE
	time_t		z_dumptime;	/* randomized time for next zone dump
					 * if Z_NEED_DUMP is set */
	u_int32_t	z_dumpintvl;	/* time interval between zone dumps */
	time_t		z_soaincrintvl; /* interval for updating soa serial */
	time_t		z_soaincrtime;	/* time for soa increment */
	u_int32_t	z_deferupdcnt;  /* max number of updates before SOA
					 * serial number incremented */
	u_int32_t	z_updatecnt;    /* number of update requests processed
					 * since the last SOA serial update */
	char 		*z_updatelog;	/* log file for updates */
#endif	
	ip_match_list 	z_update_acl;  	/* list of who can issue dynamic
					   updates */
	ip_match_list	z_query_acl;	/* sites we'll answer questions for */
	ip_match_list	z_transfer_acl;	/* sites that may get a zone transfer
					   from us */
	long		z_max_transfer_time_in;	/* max num seconds for AXFR */
#ifdef BIND_NOTIFY
	enum notify	z_notify;	/* Notify mode */
	struct in_addr *z_also_notify; /* More nameservers to notify */
	int		z_notify_count;
#endif
	enum zdialup	z_dialup;	/* secondaries over a dialup link */
	char            *z_ixfr_base;	/* where to find the history of the zone */
	char            *z_ixfr_tmp;    /* tmp file for the ixfr */
	int		    z_maintain_ixfr_base;
	long		z_max_log_size_ixfr;
	u_int32_t	z_serial_ixfr_start;	
	evTimerID	z_timer;	/* maintenance timer */
	ztimer_info	z_timerinfo;	/* UAP associated with timer */
	time_t		z_nextmaint;	/* time of next maintenance */
	u_int16_t	z_port;		/* perform AXFR to this port */
	struct fwdinfo	*z_fwdtab;	/* zone-specific forwarders */
	LINK(struct zoneinfo) z_freelink; /* if it's on the free list. */
	LINK(struct zoneinfo) z_reloadlink; /* if it's on the reload list. */
};

	/* zone types (z_type) */
enum zonetype { z_nil, z_master, z_slave, z_hint, z_stub, z_forward,
		z_cache, z_any };
#define	Z_NIL		z_nil		/* XXX */
#define Z_MASTER	z_master	/* XXX */
#define Z_PRIMARY	z_master	/* XXX */
#define Z_SLAVE		z_slave		/* XXX */
#define Z_SECONDARY	z_slave		/* XXX */
#define Z_HINT		z_hint		/* XXX */
#define Z_CACHE		z_cache		/* XXX */
#define Z_STUB		z_stub		/* XXX */
#define Z_FORWARD	z_forward	/* XXX */
#define Z_ANY		z_any		/* XXX*2 */

	/* zone state bits (32 bits) */
#define	Z_AUTH		0x00000001	/* zone is authoritative */
#define	Z_NEED_XFER	0x00000002	/* waiting to do xfer */
#define	Z_XFER_RUNNING	0x00000004	/* asynch. xfer is running */
#define	Z_NEED_RELOAD	0x00000008	/* waiting to do reload */
#define	Z_SYSLOGGED	0x00000010	/* have logged timeout */
#define	Z_QSERIAL	0x00000020	/* sysquery()'ing for serial number */
#define	Z_FOUND		0x00000040	/* found in boot file when reloading */
#define	Z_INCLUDE	0x00000080	/* set if include used in file */
#define	Z_DB_BAD	0x00000100	/* errors when loading file */
#define	Z_TMP_FILE	0x00000200	/* backup file for xfer is temporary */
#ifdef BIND_UPDATE
#define	Z_DYNAMIC	0x00000400	/* allow dynamic updates */
#define	Z_NEED_DUMP	0x00000800	/* zone has changed, needs a dump */
#define	Z_NEED_SOAUPDATE 0x00001000	/* soa serial number needs increment */
#endif /* BIND_UPDATE */
#define	Z_XFER_ABORTED	0x00002000	/* zone transfer has been aborted */
#define	Z_XFER_GONE	0x00004000	/* zone transfer process is gone */
#define	Z_TIMER_SET	0x00008000	/* z_timer contains a valid id */
#ifdef BIND_NOTIFY
#define	Z_NOTIFY	0x00010000	/* has an outbound notify executing */
#endif
#define	Z_NEED_QSERIAL  0x00020000	/* we need to re-call qserial() */
#define	Z_PARENT_RELOAD	0x00040000	/* we need to reload this as parent */
#define Z_FORWARD_SET	0x00080000	/* has forwarders been set */
#define Z_EXPIRED	0x00100000	/* expire timer has gone off */
#define Z_NEEDREFRESH	0x00200000	/* need to perform a refresh check */

	/* named_xfer exit codes */
#define	XFER_UPTODATE	0		/* zone is up-to-date */
#define	XFER_SUCCESS	1		/* performed transfer successfully */
#define	XFER_TIMEOUT	2		/* no server reachable/xfer timeout */
#define	XFER_FAIL	3		/* other failure, has been logged */
#define XFER_SUCCESSAXFR 4              /* named-xfr recived a xfr */
#define XFER_SUCCESSIXFR 5              /* named-xfr recived a ixfr */
#define XFER_SUCCESSAXFRIXFRFILE 6      /* named-xfr received AXFR for IXFR */
#define XFER_REFUSED 7      		/* one master returned REFUSED */
#define XFER_ISAXFR     -1              /* the last XFR is AXFR */
#define XFER_ISIXFR     -2              /* the last XFR is IXFR */
#define XFER_ISAXFRIXFR	-3		/* the last XFR is AXFR but we must create IXFR base */

struct qserv {
	struct sockaddr_in
			ns_addr;	/* address of NS */
	struct databuf	*ns;		/* databuf for NS record */
	struct databuf	*nsdata;	/* databuf for server address */
	struct timeval	stime;		/* time first query started */
	unsigned int	forwarder:1;	/* this entry is for a forwarder */
	unsigned int	noedns:1;	/* don't try edns */
	unsigned int	nretry:30;	/* # of times addr retried */
	u_int32_t	serial;		/* valid if Q_ZSERIAL */
};

/*
 * Structure for recording info on forwarded or generated queries.
 */
struct qinfo {
	u_int16_t	q_id;		/* id of query */
	u_int16_t	q_nsid;		/* id of forwarded query */
	struct sockaddr_in
			q_from;		/* requestor's address */
	u_char		*q_msg,		/* the message */
			*q_cmsg;	/* the cname message */
	int16_t		q_msglen,	/* len of message */
			q_msgsize,	/* allocated size of message */
			q_cmsglen,	/* len of cname message */
			q_cmsgsize;	/* allocated size of cname message */
	int16_t		q_dfd;		/* UDP file descriptor */
	u_int16_t	q_udpsize;	/* UDP message size */
	int		q_distance;	/* distance this query is from the
					 * original query that the server
					 * received. */
	time_t		q_time;		/* time to retry */
	time_t		q_expire;	/* time to expire */
	struct qinfo	*q_next;	/* rexmit list (sorted by time) */
	struct qinfo	*q_link;	/* storage list (random order) */
	struct databuf	*q_usedns[NSMAX]; /* databuf for NS that we've tried */
	struct qserv	q_addr[NSMAX];	/* addresses of NS's */
#ifdef notyet
	struct nameser	*q_ns[NSMAX];	/* name servers */
#endif
	struct dst_key *q_keys[NSMAX];	/* keys to use with this address */
	u_char		q_naddr;	/* number of addr's in q_addr */
	u_char		q_curaddr;	/* last addr sent to */
	u_char		q_nusedns;	/* number of elements in q_usedns[] */
	u_int8_t	q_flags;	/* see below */
	int16_t		q_cname;	/* # of cnames found */
	int16_t		q_nqueries;	/* # of queries required */
	struct qstream	*q_stream;	/* TCP stream, null if UDP */
	struct zoneinfo	*q_zquery;	/* Zone query is about (Q_ZSERIAL) */
	struct zoneinfo	*q_fzone;	/* Forwarding zone, if any */
	char		*q_domain;	/* domain of most enclosing zone cut */
	char		*q_name;	/* domain of query */
	u_int16_t	q_class;	/* class of query */
	u_int16_t	q_type;		/* type of query */
#ifdef BIND_NOTIFY
	int		q_notifyzone;	/* zone which needs another notify()
					 * when the reply to this comes in.
					 */
#endif
	struct tsig_record *q_tsig;	/* forwarded query's TSIG record */
	struct tsig_record *q_nstsig;	/* forwarded query's TSIG record */
};

	/* q_flags bits (8 bits) */
#define	Q_SYSTEM	0x01		/* is a system query */
#define	Q_PRIMING	0x02		/* generated during priming phase */
#define	Q_ZSERIAL	0x04		/* getting zone serial for xfer test */
#define	Q_USEVC		0x08		/* forward using tcp not udp */
#define	Q_EDNS		0x10		/* add edns opt record to answer */

#define Q_NEXTADDR(qp,n) (&(qp)->q_addr[n].ns_addr)

#define	RETRY_TIMEOUT	45

/*
 * Return codes from ns_forw:
 */
#define	FW_OK		0
#define	FW_DUP		1
#define	FW_NOSERVER	2
#define	FW_SERVFAIL	3

typedef void (*sq_closure)(struct qstream *qs);

#ifdef BIND_UPDATE
struct fdlist {
	int		fd;
	struct fdlist	*next;
};
#endif


typedef struct ns_delta {
		LINK(struct ns_delta)   d_link;
		ns_updque               d_changes;
} ns_delta;

typedef LIST(ns_delta) ns_deltalist;

typedef struct _interface {
	int			dfd,		/* Datagram file descriptor */
				sfd;		/* Stream file descriptor. */
	time_t			gen;		/* Generation number. */
	struct in_addr		addr;		/* Interface address. */
	u_int16_t		port;		/* Interface port. */
	u_int16_t		flags;		/* Valid bits for evXXXXID. */
	evFileID		evID_d;		/* Datagram read-event. */
	evConnID		evID_s;		/* Stream listen-event. */
	LINK(struct _interface)	link;
} interface;

#define INTERFACE_FILE_VALID	0x01
#define INTERFACE_CONN_VALID	0x02
#define INTERFACE_FORWARDING	0x04

struct qstream {
	int		s_rfd;		/* stream file descriptor */
	int		s_size;		/* expected amount of data to rcv */
	int		s_bufsize;	/* amount of data received in s_buf */
	u_char		*s_buf;		/* buffer of received data */
	u_char		*s_wbuf;	/* send buffer */
	u_char		*s_wbuf_send;	/* next sendable byte of send buffer */
	u_char		*s_wbuf_free;	/* next free byte of send buffer */
	u_char		*s_wbuf_end;	/* byte after end of send buffer */
	sq_closure	s_wbuf_closure;	/* callback for writable descriptor */
	struct qstream	*s_next;	/* next stream */
	struct sockaddr_in
			s_from;		/* address query came from */
	interface	*s_ifp;		/* interface query came from */
	time_t		s_time;		/* time stamp of last transaction */
	int		s_refcnt;	/* number of outstanding queries */
	u_char		s_temp[HFIXEDSZ];
#ifdef BIND_UPDATE
	int		s_opcode;	/* type of request */
	int		s_linkcnt;	/* number of client connections using
					 * this connection to forward updates
					 * to the primary */
	struct fdlist	*s_fds;		/* linked list of connections to the
					 * primaries that have been used by
					 * the server to forward this client's
					 * update requests */
#endif
	evStreamID	evID_r;		/* read event. */
	evFileID	evID_w;		/* writable event handle. */
	evConnID	evID_c;		/* connect event handle */
	u_int		flags;		/* see below */
	struct qstream_xfr {
		enum { s_x_base, s_x_firstsoa, s_x_zone,
 		       s_x_lastsoa, s_x_done, s_x_adding,
		       s_x_deleting, s_x_addsoa, s_x_deletesoa }
				state;		/* state of transfer. */
		u_char		*msg,		/* current assembly message. */
				*cp,		/* where are we in msg? */
				*eom,		/* end of msg. */
				*ptrs[128];	/* ptrs for dn_comp(). */
		int		class,		/* class of an XFR. */
				type,		/* type of XFR. */
				id,		/* id of an XFR. */
				opcode;		/* opcode of an XFR. */
		u_int		zone;		/* zone being XFR'd. */
		union {
			struct namebuf	*axfr;	/* top np of an AXFR. */
			ns_deltalist *ixfr;	/* top udp of an IXFR. */
		}		top;
 		int             ixfr_zone;
	        u_int32_t	serial;	/* serial number requested in IXFR */
		ns_tcp_tsig_state *tsig_state;	/* used by ns_sign_tcp */
		int		tsig_skip;	/* skip calling ns_sign_tcp
						 * during the next flush */
		int		tsig_size;	/* need to reserve this space
						 * for the tsig. */
		struct qs_x_lev {		/* decompose the recursion. */
			enum {sxl_ns, sxl_all, sxl_sub}
					state;	/* what's this level doing? */
			int		flags;	/* see below (SXL_*). */
			char		dname[MAXDNAME];
			struct namebuf	*np,	/* this node. */
					*nnp,	/* next node to process. */
					**npp,	/* subs. */
					**npe;	/* end of subs. */
			struct databuf	*dp;	/* current rr. */
			struct qs_x_lev	*next;	/* link. */
		}		*lev;	/* LIFO. */
		enum axfr_format transfer_format;
	} xfr;
};
#define SXL_GLUING	0x01
#define SXL_ZONECUT	0x02

	/* flags */
#define STREAM_MALLOC		0x01
#define STREAM_WRITE_EV		0x02
#define STREAM_READ_EV		0x04
#define STREAM_CONNECT_EV	0x08
#define STREAM_DONE_CLOSE	0x10
#define STREAM_AXFR		0x20
#define STREAM_AXFRIXFR		0x40

#define ALLOW_NETS	0x0001
#define	ALLOW_HOSTS	0x0002
#define	ALLOW_ALL	(ALLOW_NETS | ALLOW_HOSTS)

struct fwddata {
	struct sockaddr_in
			fwdaddr;	/* address of NS */
	struct databuf	*ns;		/* databuf for NS record */
	struct databuf	*nsdata;	/* databuf for server address */
	int		ref_count;	/* how many users of this */
};

struct fwdinfo {
	struct fwdinfo	*next;
	struct fwddata  *fwddata;
};

enum nameserStats {	nssRcvdR,	/* sent us an answer */
			nssRcvdNXD,	/* sent us a negative response */
			nssRcvdFwdR,	/* sent us a response we had to fwd */
			nssRcvdDupR,	/* sent us an extra answer */
			nssRcvdFail,	/* sent us a SERVFAIL */
			nssRcvdFErr,	/* sent us a FORMERR */
			nssRcvdErr,	/* sent us some other error */
			nssRcvdAXFR,	/* sent us an AXFR */
			nssRcvdLDel,	/* sent us a lame delegation */
			nssRcvdOpts,	/* sent us some IP options */
			nssSentSysQ,	/* sent them a sysquery */
			nssSentAns,	/* sent them an answer */
			nssSentFwdQ,	/* fwdd a query to them */
			nssSentDupQ,	/* sent them a retry */
			nssSendtoErr,	/* error in sendto */
			nssRcvdQ,	/* sent us a query */
			nssRcvdIQ,	/* sent us an inverse query */
			nssRcvdFwdQ,	/* sent us a query we had to fwd */
			nssRcvdDupQ,	/* sent us a retry */
			nssRcvdTCP,	/* sent us a query using TCP */
			nssSentFwdR,	/* fwdd a response to them */
			nssSentFail,	/* sent them a SERVFAIL */
			nssSentFErr,	/* sent them a FORMERR */
			nssSentNaAns,   /* sent them a non autoritative answer */
			nssSentNXD,	/* sent them a negative response */
			nssRcvdUQ,	/* sent us an unapproved query */
			nssRcvdURQ,	/* sent us an unapproved recursive query */
			nssRcvdUXFR,	/* sent us an unapproved AXFR or IXFR */
			nssRcvdUUpd,	/* sent us an unapproved update */
			nssLast };

struct nameser {
	struct in_addr	addr;		/* key */
	u_long		stats[nssLast];	/* statistics */
#ifdef notyet
	u_int32_t	rtt;		/* round trip time */
	/* XXX - need to add more stuff from "struct qserv", and use our rtt */
	u_int16_t	flags;		/* see below */
	u_int8_t	xfers;		/* #/xfers running right now */
#endif
};
		
enum transport { primary_trans, secondary_trans, response_trans, update_trans,
		 num_trans };

/* types used by the parser or config routines */

typedef struct zone_config {
	void *opaque;
} zone_config;

typedef struct listen_info {
	u_short port;
	ip_match_list list;
	struct listen_info *next;
} *listen_info;

typedef struct listen_info_list {
	listen_info first;
	listen_info last;
} *listen_info_list;

#ifndef RLIMIT_TYPE
#define RLIMIT_TYPE u_long
#endif
typedef RLIMIT_TYPE rlimit_type;

struct control;
typedef struct control *control;
typedef LIST(struct control) controls;

enum ordering { unknown_order, fixed_order, cyclic_order, random_order };

#define DEFAULT_ORDERING cyclic_order

typedef struct rrset_order_element {
	int class;
	int type;
	char *name;
	enum ordering order;
	struct rrset_order_element *next;
} *rrset_order_element ;

typedef struct rrset_order_list {
	rrset_order_element first;
	rrset_order_element last;
} *rrset_order_list;


typedef struct options {
	u_int32_t flags;
	char *hostname;
	char *version;
	char *directory;
	char *dump_filename;
	char *pid_filename;
	char *stats_filename;
	char *memstats_filename;
	char *named_xfer;
	int transfers_in;
	int transfers_per_ns;
	int transfers_out;
	int serial_queries;
 	int max_log_size_ixfr;
	enum axfr_format transfer_format;
	long max_transfer_time_in;
	struct sockaddr_in query_source;
	struct in_addr axfr_src;
#ifdef BIND_NOTIFY
	int notify_count;
	struct in_addr *also_notify;
#endif
	ip_match_list query_acl;
	ip_match_list recursion_acl;
	ip_match_list transfer_acl;
	ip_match_list blackhole_acl;
	ip_match_list topology;
	ip_match_list sortlist;
	enum severity check_names[num_trans];
	u_long data_size;
	u_long stack_size;
	u_long core_size;
	u_long files;
	listen_info_list listen_list;
	struct fwdinfo *fwdtab;
	/* XXX need to add forward option */
	int clean_interval;
	int interface_interval;
	int stats_interval;
	rrset_order_list ordering;
	int heartbeat_interval;
	u_int max_ncache_ttl;
	u_int max_host_stats;
	u_int lame_ttl;
	int minroots;
	u_int16_t preferred_glue;
	enum notify notify;
} *options;

typedef struct key_list_element {
	struct dst_key *key;
	struct key_list_element *next;
} *key_list_element;

typedef struct key_info_list {
	key_list_element first;
	key_list_element last;
} *key_info_list;

typedef struct topology_config {
	void *opaque;
} topology_config;

#define UNKNOWN_TOPOLOGY_DISTANCE	9998
#define MAX_TOPOLOGY_DISTANCE		9999

typedef struct topology_distance {
	ip_match_list patterns;
	struct topology_distance *next;
} *topology_distance;

typedef struct topology_context {
	topology_distance first;
	topology_distance last;
} *topology_context;

typedef struct acl_table_entry {
	char *name;
	ip_match_list list;
	struct acl_table_entry *next;
} *acl_table_entry;

typedef struct server_config {
	void *opaque;
} server_config;

#define SERVER_INFO_BOGUS	0x01
#define SERVER_INFO_SUPPORT_IXFR	0x02
#define SERVER_INFO_EDNS	0x04

typedef struct server_info {
	struct in_addr address;
	u_int flags;
	int transfers;
	enum axfr_format transfer_format;
	key_info_list key_list;
	/* could move statistics to here, too */
	struct server_info *next;
} *server_info;

/*
 * enum <--> name translation
 */

struct ns_sym {
	int		number;	/* Identifying number, like ns_log_default */
	const char *	name;	/* Its symbolic name, like "default" */
};

/*
 * Logging options
 */

typedef enum ns_logging_categories {
	ns_log_default = 0,
	ns_log_config,
	ns_log_parser,
	ns_log_queries,
	ns_log_lame_servers,
	ns_log_statistics,
	ns_log_panic,
	ns_log_update,
	ns_log_ncache,
	ns_log_xfer_in,
	ns_log_xfer_out,
	ns_log_db,
	ns_log_eventlib,
	ns_log_packet,
#ifdef BIND_NOTIFY
	ns_log_notify,
#endif
	ns_log_cname,
	ns_log_security,
	ns_log_os,
	ns_log_insist,
	ns_log_maint,
	ns_log_load,
	ns_log_resp_checks,
	ns_log_control,
	ns_log_max_category
} ns_logging_categories;

typedef struct log_config {
	log_context log_ctx;
	log_channel eventlib_channel;
	log_channel packet_channel;
	int	    default_debug_active;
} *log_config;

struct map {
	const char *		token;
	int			val;
};

#define NOERROR_NODATA   15	/* only used internally by the server, used for
				 * -ve $ing non-existence of records. 15 is not
				 * a code used as yet anyway.
				 */

#define NTTL		600 /* ttl for negative data: 10 minutes? */

#define VQEXPIRY	900 /* a VQ entry expires in 15*60 = 900 seconds */

#ifdef BIND_UPDATE
enum req_action { Finish, Refuse, Return };
#endif

#ifdef INIT
	error "INIT already defined, check system include files"
#endif
#ifdef DECL
	error "DECL already defined, check system include files"
#endif

#ifdef MAIN_PROGRAM
#define INIT(x) = x
#define	DECL
#else
#define INIT(x)
#define DECL extern
#endif

#define EDNS_MESSAGE_SZ	4096
