/*
 * Copyright (c) 1985, 1990 Regents of the University of California.
 * All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *
 *	@(#)ns.h	4.33 (Berkeley) 8/23/90
 */

/*
 * Global definitions and variables for the name server.
 */

#include <arpa/inet.h>
#include <string.h>

/*
 * Timeout time should be around 1 minute or so.  Using the
 * the current simplistic backoff strategy, the sequence
 * retrys after 4, 8, and 16 seconds.  With 3 servers, this
 * dies out in a little more than a minute.
 * (sequence RETRYBASE, 2*RETRYBASE, 4*RETRYBASE... for MAXRETRY)
 */
#define MINROOTS	2	 	/* min number of root hints */
#define NSMAX		16		/* max number of NS addrs to try */
#define RETRYBASE	4 		/* base time between retries */
#define MAXRETRY	3		/* max number of retries per addr */
#define MAXCNAMES	8		/* max # of CNAMES tried per addr */
#define MAXQUERIES	20		/* max # of queries to be made */
					/* (prevent "recursive" loops) */
#define	INIT_REFRESH	600		/* retry time for initial secondary */
					/* contact (10 minutes) */

#define ALPHA    0.7	/* How much to preserver of old response time */
#define	BETA	 1.2	/* How much to penalize response time on failure */
#define	GAMMA	 0.98	/* How much to decay unused response times */

struct zoneinfo {
	int	z_type;			/* type of zone */
	int	z_auth;			/* zone is authoritative */
	char	*z_origin;		/* root domain name of zone */
	time_t	z_time;			/* time for next refresh */
	time_t	z_lastupdate;		/* time of last refresh */
	u_long	z_refresh;		/* refresh interval */
	u_long	z_retry;		/* refresh retry interval */
	u_long	z_expire;		/* expiration time for cached info */
	u_long	z_minimum;		/* minimum TTL value */
	u_long	z_serial;		/* changes if zone modified */
	char	*z_source;		/* source location of data */
	time_t	z_ftime;		/* modification time of source file */
	int	z_addrcnt;		/* address count */
	struct	in_addr z_addr[NSMAX];	/* list of master servers for zone */
	int	z_state;		/* state bits; see below */
	u_short	z_xferpid;		/* xfer child pid */
#ifdef ALLOW_UPDATES
	int	hasChanged;		/* non-zero if zone has been updated
					 * since last checkpoint
					 */
#endif ALLOW_UPDATES
};

	/* zone types (z_type) */
#define Z_PRIMARY	1
#define Z_SECONDARY	2
#define Z_CACHE		3

	/* zone state bits */
#define	Z_AUTH		0x01		/* should replace z_auth */
#define	Z_NEED_XFER	0x02		/* waiting to do xfer */
#define	Z_XFER_RUNNING	0x04		/* asynch. xfer is running */
#define	Z_NEED_RELOAD	0x08		/* waiting to do reload */
#define	Z_SYSLOGGED	0x10		/* have logged timeout */
#define	Z_CHANGED	0x20		/* should replace hasChanged */
#define	Z_FOUND		0x40		/* found in boot file when reloading */
#define	Z_INCLUDE	0x80		/* set if include used in file */
#define	Z_DB_BAD	0x100		/* errors when loading file */
#define	Z_TMP_FILE	0x200		/* backup file for xfer is temporary */
#ifdef ALLOW_UPDATES
#define	Z_DYNAMIC	0x400		/* allow dynamic updates */
#define	Z_DYNADDONLY	0x800		/* dynamic mode: add new data only */
#endif ALLOW_UPDATES

	/* xfer exit codes */
#define	XFER_UPTODATE	0		/* zone is up-to-date */
#define	XFER_SUCCESS	1		/* performed transfer successfully */
#define	XFER_TIMEOUT	2		/* no server reachable/xfer timeout */
#define	XFER_FAIL	3		/* other failure, has been logged */

/*
 * Structure for recording info on forwarded queries.
 */
struct qinfo {
	u_short	q_id;			/* id of query */
	u_short	q_nsid;			/* id of forwarded query */
	int	q_dfd;			/* UDP file descriptor */
	struct	sockaddr_in q_from;	/* requestor's address */
	char	*q_msg;			/* the message */
	int	q_msglen;		/* len of message */
	int	q_naddr;		/* number of addr's in q_addr */
	int	q_curaddr;		/* last addr sent to */
	struct	fwdinfo	*q_fwd;		/* last	forwarder used */
	time_t	q_time;			/* time to retry */
	struct	qinfo *q_next;		/* rexmit list (sorted by time) */
	struct	qinfo *q_link;		/* storage list (random order) */
	struct  qserv {
		struct	sockaddr_in ns_addr;	/* addresses of NS's */
		struct  databuf *ns;	/* databuf for NS record */
		struct  databuf *nsdata; /* databuf for server address */
		struct  timeval stime;	/* time first query started */
		int	nretry;		/* # of times addr retried */
	} q_addr[NSMAX];		/* addresses of NS's */
	struct	databuf *q_usedns[NSMAX]; /* databuf for NS that we've tried */
	int	q_nusedns;
	int	q_cname;		/* # of cnames found */
	int	q_nqueries;		/* # of queries required */
	char	*q_cmsg;		/* the cname message */
	int	q_cmsglen;		/* len of cname message */
	struct	qstream *q_stream;	/* TCP stream, null if UDP */
	int	q_system;		/* boolean, system query */
};

#define	Q_NEXTADDR(qp,n)	\
	(((qp)->q_fwd == (struct fwdinfo *)0) ? \
	 &(qp)->q_addr[n].ns_addr : &(qp)->q_fwd->fwdaddr)

#define PRIMING_CACHE	42
#define QINFO_NULL	((struct qinfo *)0)

#ifndef XFER
extern struct qinfo *qfindid();
extern struct qinfo *qnew();
extern struct qinfo *retryqp;		/* next query to retry */
#endif /* XFER */

/*
 * Return codes from ns_forw:
 */
#define	FW_OK		0
#define	FW_DUP		1
#define	FW_NOSERVER	2
#define	FW_SERVFAIL	3

struct qstream {
	int 	s_rfd;			/* stream file descriptor */
	int 	s_size;			/* expected amount of data to recive */
	int 	s_bufsize;		/* amount of data recived in s_buf */
	char    *s_buf;			/* buffer of recived data */
	char    *s_bufp;		/* pointer into s_buf of recived data */
	struct	qstream *s_next;	/* next stream */
	struct	sockaddr_in s_from;	/* address query came from */
	u_long	s_time;			/* time stamp of last transaction */
	int	s_refcnt;		/* number of outstanding queries */
	u_short	s_tempsize;		/* temporary for size from net */
};

#define QSTREAM_NULL	((struct qstream *)0)
extern struct qstream *streamq;		/* stream queue */

struct qdatagram {
	int 	dq_dfd;			/* datagram file descriptor */
	struct	qdatagram *dq_next;	/* next datagram */
	struct	in_addr  dq_addr;	/* address of interface */
};

#define QDATAGRAM_NULL	((struct qdatagram *)0)
extern struct qdatagram *datagramq;	/* datagram queue */

struct netinfo {
	struct netinfo *next;
	u_long net;
	u_long mask;
	struct in_addr my_addr;
};

struct fwdinfo {
	struct fwdinfo *next;
	struct sockaddr_in fwdaddr;
};

struct nets {
	char *name;
	long net;
	struct nets *next;
}; 

/*
 *  Statistics Defines
 */
struct stats {
	unsigned long	cnt;
	char	*description;
};

/* gross count of UDP packets in and out */
#define	S_INPKTS	0
#define	S_OUTPKTS	1
/* gross count of queries and inverse queries received */
#define	S_QUERIES	2
#define	S_IQUERIES	3
#define S_DUPQUERIES	4
#define	S_RESPONSES	5
#define	S_DUPRESP	6
#define	S_RESPOK	7
#define	S_RESPFAIL	8
#define	S_RESPFORMERR	9
#define	S_SYSQUERIES	10
#define	S_PRIMECACHE	11
#define	S_CHECKNS	12
#define	S_BADRESPONSES	13
#define	S_MARTIANS	14
#define S_NSTATS	15	/* Careful! */
#ifdef STATS
extern struct stats stats[S_NSTATS];
extern unsigned long typestats[T_ANY+1];
#endif

/*
 * Attempt to configure for type of function returned by signal-catching
 * functions (which signal and sigvec.sv_handler take a pointer to).
 * This can guess for BSD; otherwise, define SIG_FN externally.
 */
#ifndef	SIG_FN
#ifdef	BSD
#if BSD >= 199006
#define SIG_FN	void		/* signal-catching functions return void */
#else
#define SIG_FN	int		/* signal-catching functions return int */
#endif
#else	/* BSD */
#define SIG_FN	void		/* signal-catching functions return void */
#endif	/* BSD */
#endif


#ifdef DEBUG
extern int debug;			/* debug flag */
extern FILE *ddt;			/* debug file discriptor */
#endif
#ifndef XFER
extern int ds;				/* datagram socket */
extern struct qdatagram *dqp;
extern struct timeval tt;		/* place to store time */

extern struct itimerval ival;		/* maintenance interval */
extern struct zoneinfo *zones;		/* zone information */
extern int nzones;			/* number of zones in use */

extern int forward_only;		/* true on slave server */
#endif /* XFER */

#ifdef vax
extern u_short htons(), ntohs();
extern u_long htonl(), ntohl();
#endif

#define MAX_XFER_TIME         60 * 60 * 2  /* max seconds for an xfer */
#define XFER_TIME_FUDGE	      10           /* MAX_XFER_TIME fudge */

#ifndef XFER
extern int xfer_running_cnt;	          /* number of xfers running */
extern int xfer_deferred_cnt;	          /* number of deferred xfers */
#define MAX_XFERS_RUNNING     4           /* max value of xfer_running_cnt */
#endif /* XFER */
