/*
 *	from ns.h	4.33 (Berkeley) 8/23/90
 *	$Id: ns_defs.h,v 1.3 1995/08/20 21:18:34 peter Exp $
 */

/*
 * ++Copyright++ 1986
 * -
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
 * -
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
 * -
 * --Copyright--
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
#define MINROOTS	2	/* min number of root hints */
#define NSMAX		16	/* max number of NS addrs to try ([0..255]) */
#define RETRYBASE	4 	/* base time between retries */
#define	MAXCLASS	255	/* XXX - may belong elsewhere */
#define MAXRETRY	3	/* max number of retries per addr */
#define MAXCNAMES	8	/* max # of CNAMES tried per addr */
#define MAXQUERIES	20	/* max # of queries to be made */
#define	MAXQSERIAL	4	/* max # of outstanding QSERIAL's */
				/* (prevent "recursive" loops) */
#define	INIT_REFRESH	600	/* retry time for initial secondary */
				/* contact (10 minutes) */
#define NADDRECS	20	/* max addt'l rr's per resp */

#define XFER_TIMER	120	/* named-xfer's connect timeout */
#define MAX_XFER_TIME	60*60*2	/* max seconds for an xfer */
#define XFER_TIME_FUDGE	10	/* MAX_XFER_TIME fudge */
#define MAX_XFERS_RUNNING 10	/* default max value of xfers_running */
#define	MAX_XFERS_PER_NS 2	/* max # of xfers per peer nameserver */
#define	XFER_BUFSIZE	(16*1024) /* arbitrary but bigger than most MTU's */

#define ALPHA    0.7	/* How much to preserve of old response time */
#define	BETA	 1.2	/* How much to penalize response time on failure */
#define	GAMMA	 0.98	/* How much to decay unused response times */

#define	USE_MINIMUM	0xffffffff

	/* sequence-space arithmetic */
#define SEQ_GT(a,b)	((int32_t)((a)-(b)) > 0)

	/* wildcard predicate */
#define WILDCARD_P(str) (str[0] == '*' && str[1] == '\0')

	/* cheap garbage collection */
#define	FREE_ONCE(p) { if (p) { free(p); p = NULL; } }

/* these fields are ordered to maintain word-alignment;
 * be careful about changing them.
 */
struct zoneinfo {
	char		*z_origin;	/* root domain name of zone */
	time_t		z_time;		/* time for next refresh */
	time_t		z_lastupdate;	/* time of last refresh */
	u_int32_t	z_refresh;	/* refresh interval */
	u_int32_t	z_retry;	/* refresh retry interval */
	u_int32_t	z_expire;	/* expiration time for cached info */
	u_int32_t	z_minimum;	/* minimum TTL value */
	u_int32_t	z_serial;	/* changes if zone modified */
	char		*z_source;	/* source location of data */
	time_t		z_ftime;	/* modification time of source file */
	struct in_addr	z_xaddr;	/* override server for next xfer */
	struct in_addr	z_addr[NSMAX];	/* list of master servers for zone */
	u_char		z_addrcnt;	/* number of entries in z_addr[] */
	u_char		z_type;		/* type of zone; see below */
	u_int16_t	z_flags;	/* state bits; see below */
	pid_t		z_xferpid;	/* xfer child pid */
	int		z_class;	/* class of zone */
#ifdef SECURE_ZONES
	struct netinfo *secure_nets;	/* list of secure networks for zone */
#endif
#ifdef BIND_NOTIFY
	/* XXX - this will have to move to the name when we do !SOA notify */
	struct notify	*z_notifylist;	/* list of servers we should notify */
#endif
};

#ifdef BIND_NOTIFY
struct notify {
	struct in_addr	addr;		/* of server */
	time_t		last;		/* when they asked */
	struct notify	*next;
	/* XXX - this will need a type field when we do !SOA notify */
};
#endif

	/* zone types (z_type) */
#define	Z_NIL		0		/* zone slot not in use */
#define Z_PRIMARY	1
#define Z_SECONDARY	2
#define Z_CACHE		3
#define Z_STUB		4

	/* zone state bits (16 bits) */
#define	Z_AUTH		0x0001		/* zone is authoritative */
#define	Z_NEED_XFER	0x0002		/* waiting to do xfer */
#define	Z_XFER_RUNNING	0x0004		/* asynch. xfer is running */
#define	Z_NEED_RELOAD	0x0008		/* waiting to do reload */
#define	Z_SYSLOGGED	0x0010		/* have logged timeout */
#define	Z_QSERIAL	0x0020		/* sysquery()'ing for serial number */
#define	Z_FOUND		0x0040		/* found in boot file when reloading */
#define	Z_INCLUDE	0x0080		/* set if include used in file */
#define	Z_DB_BAD	0x0100		/* errors when loading file */
#define	Z_TMP_FILE	0x0200		/* backup file for xfer is temporary */
#ifdef ALLOW_UPDATES
#define	Z_DYNAMIC	0x0400		/* allow dynamic updates */
#define	Z_DYNADDONLY	0x0800		/* dynamic mode: add new data only */
#define	Z_CHANGED	0x1000		/* zone has changed */
#endif /* ALLOW_UPDATES */
#define	Z_XFER_ABORTED	0x2000		/* zone transfer has been aborted */
#define	Z_XFER_GONE	0x4000		/* zone transfer process is gone */

	/* named_xfer exit codes */
#define	XFER_UPTODATE	0		/* zone is up-to-date */
#define	XFER_SUCCESS	1		/* performed transfer successfully */
#define	XFER_TIMEOUT	2		/* no server reachable/xfer timeout */
#define	XFER_FAIL	3		/* other failure, has been logged */

#include <sys/time.h>

/* XXX - "struct qserv" is deprecated in favor of "struct nameser" */
struct qserv {
	struct sockaddr_in
			ns_addr;	/* address of NS */
	struct databuf	*ns;		/* databuf for NS record */
	struct databuf	*nsdata;	/* databuf for server address */
	struct timeval	stime;		/* time first query started */
	int		nretry;		/* # of times addr retried */
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
			q_cmsglen;	/* len of cname message */
	int16_t		q_dfd;		/* UDP file descriptor */
	struct fwdinfo	*q_fwd;		/* last	forwarder used */
	time_t		q_time;		/* time to retry */
	time_t		q_expire;	/* time to expire */
	struct qinfo	*q_next;	/* rexmit list (sorted by time) */
	struct qinfo	*q_link;	/* storage list (random order) */
	struct databuf	*q_usedns[NSMAX]; /* databuf for NS that we've tried */
	struct qserv	q_addr[NSMAX];	/* addresses of NS's */
#ifdef notyet
	struct nameser	*q_ns[NSMAX];	/* name servers */
#endif
	u_char		q_naddr;	/* number of addr's in q_addr */
	u_char		q_curaddr;	/* last addr sent to */
	u_char		q_nusedns;	/* number of elements in q_usedns[] */
	u_int8_t	q_flags;	/* see below */
	int16_t		q_cname;	/* # of cnames found */
	int16_t		q_nqueries;	/* # of queries required */
	struct qstream	*q_stream;	/* TCP stream, null if UDP */
	struct zoneinfo	*q_zquery;	/* Zone query is about (Q_ZSERIAL) */
#if defined(LAME_DELEGATION) || defined(VALIDATE)
	char    q_domain[MAXDNAME];	/* domain for servers we are querying */
#endif
#ifdef BIND_NOTIFY
	int		q_notifyzone;	/* zone which needs a sysnotify()
					 * when the reply to this comes in.
					 */
#endif
};

	/* q_flags bits (8 bits) */
#define	Q_SYSTEM	0x01		/* is a system query */
#define	Q_PRIMING	0x02		/* generated during priming phase */
#define	Q_ZSERIAL	0x04		/* getting zone serial for xfer test */

#define	Q_NEXTADDR(qp,n)	\
	(((qp)->q_fwd == (struct fwdinfo *)0) ? \
	 &(qp)->q_addr[n].ns_addr : &(qp)->q_fwd->fwdaddr)

#define	RETRY_TIMEOUT	45
#define QINFO_NULL	((struct qinfo *)0)

/*
 * Return codes from ns_forw:
 */
#define	FW_OK		0
#define	FW_DUP		1
#define	FW_NOSERVER	2
#define	FW_SERVFAIL	3

struct qstream {
	int		s_rfd;		/* stream file descriptor */
	int		s_size;		/* expected amount of data to recive */
	int		s_bufsize;	/* amount of data recived in s_buf */
	u_char		*s_buf;		/* buffer of received data */
	u_char		*s_bufp;	/* pointer into s_buf of recived data*/
	struct qstream	*s_next;	/* next stream */
	struct sockaddr_in
			s_from;		/* address query came from */
	u_int32_t	s_time;		/* time stamp of last transaction */
	int		s_refcnt;	/* number of outstanding queries */
	u_int16_t	s_tempsize;	/* temporary for size from net */
};
#define QSTREAM_NULL	((struct qstream *)0)

struct qdatagram {
	int		dq_dfd;		/* datagram file descriptor */
	time_t		dq_gen;		/* generation number */
	struct qdatagram
			*dq_next;	/* next datagram */
	struct in_addr	dq_addr;	/* interface address */
};
#define QDATAGRAM_NULL	((struct qdatagram *)0)

struct netinfo {
	struct netinfo	*next;
	u_int32_t	addr;
	u_int32_t	mask;
	struct in_addr	my_addr;
};

#define ALLOW_NETS	0x0001
#define	ALLOW_HOSTS	0x0002
#define	ALLOW_ALL	(ALLOW_NETS | ALLOW_HOSTS)

struct fwdinfo {
	struct fwdinfo	*next;
	struct sockaddr_in
			fwdaddr;
};

enum nameserStats {	nssRcvdQ,	/* sent us a query */
			nssRcvdR,	/* sent us an answer */
			nssRcvdIQ,	/* sent us an inverse query */
			nssRcvdNXD,	/* sent us a negative response */
			nssRcvdFwdQ,	/* sent us a query we had to fwd */
			nssRcvdFwdR,	/* sent us a response we had to fwd */
			nssRcvdDupQ,	/* sent us a retry */
			nssRcvdDupR,	/* sent us an extra answer */
			nssRcvdFail,	/* sent us a SERVFAIL */
			nssRcvdFErr,	/* sent us a FORMERR */
			nssRcvdErr,	/* sent us some other error */
			nssRcvdTCP,	/* sent us a query using TCP */
			nssRcvdAXFR,	/* sent us an AXFR */
			nssRcvdLDel,	/* sent us a lame delegation */
			nssRcvdOpts,	/* sent us some IP options */
			nssSentSysQ,	/* sent them a sysquery */
			nssSentAns,	/* sent them an answer */
			nssSentFwdQ,	/* fwdd a query to them */
			nssSentFwdR,	/* fwdd a response to them */
			nssSentDupQ,	/* sent them a retry */
			nssSentFail,	/* sent them a SERVFAIL */
			nssSentFErr,	/* sent them a FORMERR */
			nssSendtoErr,	/* error in sendto */
#ifdef XSTATS
			nssNotNsQ,      /* query received from remote port != ns_port */
			nssSentNaAns,   /* sent them a non autoritative answer */
			nssSentNXD,     /* sent them a negative response */
#endif
			nssLast };

struct nameser {
	struct in_addr	addr;		/* key */
	u_long		stats[nssLast];	/* statistics */
#ifdef notyet
	u_int32_t	rtt;		/* round trip time */
	/* XXX - need to add more stuff from "struct qserv", and use our rtt */
	u_int16_t	flags;		/* see below */
#endif
	u_int8_t	xfers;		/* #/xfers running right now */
};


#ifdef NCACHE
#define NOERROR_NODATA   6 /* only used internally by the server, used for
                            * -ve $ing non-existence of records. 6 is not
                            * a code used as yet anyway. anant@isi.edu
                            */
#define NTTL		600 /* ttl for negative data: 10 minutes? */
#endif /*NCACHE*/

#define VQEXPIRY	900 /* a VQ entry expires in 15*60 = 900 seconds */

#ifdef VALIDATE

#define INVALID		0
#define VALID_NO_CACHE	1
#define VALID_CACHE	2
#define MAXNAMECACHE	100
#define MAXVQ		100 /* Max number of elements in TO_Validate queue */

struct _nameaddr {
	struct in_addr	ns_addr;
	char		*nsname;
};
typedef struct _nameaddr NAMEADDR;

struct _to_validate {
	int16_t		class;		/* Name Class */
	int16_t		type;		/* RR type */
	char		*data;		/* RR data */
	char		*dname;		/* Name */
	time_t		time;		/* time at which inserted in queue */
	struct _to_validate
			*next,
			*prev;
};
typedef struct _to_validate TO_Validate;

#endif /*VALIDATE*/


#ifdef DEBUG
# define dprintf(lev, args) (ddt && (debug >= lev) && fprintf args)
#else
# define dprintf(lev, args)
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
