/*
 * mtrace.c
 *
 * This tool traces the branch of a multicast tree from a source to a
 * receiver for a particular multicast group and gives statistics
 * about packet rate and loss for each hop along the path.  It can
 * usually be invoked just as
 *
 * 	mtrace source
 *
 * to trace the route from that source to the local host for a default
 * group when only the route is desired and not group-specific packet
 * counts.  See the usage line for more complex forms.
 *
 *
 * Released 4 Apr 1995.  This program was adapted by Steve Casner
 * (USC/ISI) from a prototype written by Ajit Thyagarajan (UDel and
 * Xerox PARC).  It attempts to parallel in command syntax and output
 * format the unicast traceroute program written by Van Jacobson (LBL)
 * for the parts where that makes sense.
 * 
 * Copyright (c) 1995 by the University of Southern California
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation in source and binary forms for any purposes and without
 * fee is hereby granted, provided that the above copyright notice
 * appear in all copies and that both the copyright notice and this
 * permission notice appear in supporting documentation, and that any
 * documentation, advertising materials, and other materials related to
 * such distribution and use acknowledge that the software was developed
 * by the University of Southern California, Information Sciences
 * Institute.  The name of the University may not be used to endorse or
 * promote products derived from this software without specific prior
 * written permission.
 *
 * THE UNIVERSITY OF SOUTHERN CALIFORNIA makes no representations about
 * the suitability of this software for any purpose.  THIS SOFTWARE IS
 * PROVIDED "AS IS" AND WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Other copyrights might apply to parts of this software and are so
 * noted when applicable.
 *
 * Parts of this software are derived from mrouted, which has the
 * following license:
 * 
 * The mrouted program is covered by the following license.  Use of the
 * mrouted program represents acceptance of these terms and conditions.
 * 
 * 1. STANFORD grants to LICENSEE a nonexclusive and nontransferable
 * license to use, copy and modify the computer software ``mrouted''
 * (hereinafter called the ``Program''), upon the terms and conditions
 * hereinafter set out and until Licensee discontinues use of the Licensed
 * Program.
 * 
 * 2. LICENSEE acknowledges that the Program is a research tool still in
 * the development state, that it is being supplied ``as is,'' without any
 * accompanying services from STANFORD, and that this license is entered
 * into in order to encourage scientific collaboration aimed at further
 * development and application of the Program.
 * 
 * 3. LICENSEE may copy the Program and may sublicense others to use
 * object code copies of the Program or any derivative version of the
 * Program.  All copies must contain all copyright and other proprietary
 * notices found in the Program as provided by STANFORD.  Title to
 * copyright to the Program remains with STANFORD.
 * 
 * 4. LICENSEE may create derivative versions of the Program.  LICENSEE
 * hereby grants STANFORD a royalty-free license to use, copy, modify,
 * distribute and sublicense any such derivative works.  At the time
 * LICENSEE provides a copy of a derivative version of the Program to a
 * third party, LICENSEE shall provide STANFORD with one copy of the
 * source code of the derivative version at no charge to STANFORD.
 * 
 * 5. STANFORD MAKES NO REPRESENTATIONS OR WARRANTIES, EXPRESS OR
 * IMPLIED.  By way of example, but not limitation, STANFORD MAKES NO
 * REPRESENTATION OR WARRANTIES OF MERCHANTABILITY OR FITNESS FOR ANY
 * PARTICULAR PURPOSE OR THAT THE USE OF THE LICENSED PROGRAM WILL NOT
 * INFRINGE ANY PATENTS, COPYRIGHTS, TRADEMARKS OR OTHER RIGHTS. STANFORD
 * shall not be held liable for any liability nor for any direct, indirect
 * or consequential damages with respect to any claim by LICENSEE or any
 * third party on account of or arising from this Agreement or use of the
 * Program.
 * 
 * 6. This agreement shall be construed, interpreted and applied in
 * accordance with the State of California and any legal action arising
 * out of this Agreement or use of the Program shall be filed in a court
 * in the State of California.
 * 
 * 7. Nothing in this Agreement shall be construed as conferring rights to
 * use in advertising, publicity or otherwise any trademark or the name
 * of ``Stanford''.
 * 
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 *
 * The mtrace program has been modified and improved by Xerox
 * Corporation.  Xerox grants to LICENSEE a non-exclusive and
 * non-transferable license to use, copy, and modify the Xerox modified
 * and improved mrouted software on the same terms and conditions which
 * govern the license Stanford and ISI grant with respect to the mtrace
 * program.  These terms and conditions are incorporated in this grant
 * by reference and shall be deemed to have been accepted by LICENSEE
 * to cover its relationship with Xerox Corporation with respect to any
 * use of the Xerox improved program.
 * 
 * The mtrace program is COPYRIGHT 1998 by Xerox Corporation.
 *
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <memory.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/igmp.h>
#include <sys/ioctl.h>
#ifdef SYSV
#include <sys/sockio.h>
#endif
#include <arpa/inet.h>
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#ifdef SUNOS5
#include <sys/systeminfo.h>
#endif

typedef unsigned int u_int32;	/* XXX */
#include "mtrace.h"

#define DEFAULT_TIMEOUT	3	/* How long to wait before retrying requests */
#define DEFAULT_RETRIES 3	/* How many times to try */
#define DEFAULT_EXTRAHOPS 3	/* How many hops past a non-responding rtr */
#define MAXHOPS 60		/* Don't need more hops than this */
#define UNICAST_TTL 255		/* TTL for unicast response */
#define MULTICAST_TTL1 127	/* Default TTL for multicast query/response */
#define MULTICAST_TTL_INC 32	/* TTL increment for increase after timeout */
#define MULTICAST_TTL_MAX 192	/* Maximum TTL allowed (protect low-BW links */

#define TRUE 1
#define FALSE 0
#define DVMRP_ASK_NEIGHBORS2	5	/* DVMRP msg requesting neighbors */
#define DVMRP_NEIGHBORS2	6	/* reply to above */
#define DVMRP_NF_DOWN		0x10	/* kernel state of interface */
#define DVMRP_NF_DISABLED	0x20	/* administratively disabled */
#define MAX_IP_PACKET_LEN	576
#define MIN_IP_HEADER_LEN	20
#define MAX_IP_HEADER_LEN	60
#define MAX_DVMRP_DATA_LEN \
		( MAX_IP_PACKET_LEN - MAX_IP_HEADER_LEN - IGMP_MINLEN )

struct resp_buf {
    u_long qtime;		/* Time query was issued */
    u_long rtime;		/* Time response was received */
    int	len;			/* Number of reports or length of data */
    struct igmp igmp;		/* IGMP header */
    union {
	struct {
	    struct tr_query q;		/* Query/response header */
	    struct tr_resp r[MAXHOPS];	/* Per-hop reports */
	} t;
	char d[MAX_DVMRP_DATA_LEN];	/* Neighbor data */
    } u;
} base, incr[2];

#define qhdr u.t.q
#define resps u.t.r
#define ndata u.d

char *names[MAXHOPS];

/*
 * In mrouted 3.3 and 3.4 (and in some Cisco IOS releases),
 * cache entries can get deleted even if there is traffic
 * flowing, which will reset the per-source/group counters.
 */
#define		BUG_RESET	0x01

/*
 * Also in mrouted 3.3 and 3.4, there's a bug in neighbor
 * version processing which can cause them to believe that
 * the neighbor is constantly resetting.  This causes them
 * to constantly delete all their state.
 */
#define		BUG_RESET2X	0x02

/*
 * Pre-3.7 mrouted's forget to byte-swap their reports.
 */
#define		BUG_SWAP	0x04

/*
 * Pre-3.9 mrouted's forgot a parenthesis in the htonl()
 * on the time calculation so supply bogus times.
 */
#define		BUG_BOGUSTIME	0x08

#define BUG_NOPRINT	(BUG_RESET | BUG_RESET2X)

int bugs[MAXHOPS];			/* List of bugs noticed at each hop */

struct mtrace {
	struct mtrace	*next;
	struct resp_buf	 base, incr[2];
	struct resp_buf	*new, *prev;
	int		 nresp;
	struct timeval	 last;
	int		 bugs[MAXHOPS];
	char		*names[MAXHOPS];
	int		 lastqid;
};

int timeout = DEFAULT_TIMEOUT;
int nqueries = DEFAULT_RETRIES;
int numeric = FALSE;
int debug = 0;
int passive = FALSE;
int multicast = FALSE;
int unicast = FALSE;
int statint = 10;
int verbose = FALSE;
int tunstats = FALSE;
int weak = FALSE;
int extrahops = DEFAULT_EXTRAHOPS;
int printstats = TRUE;
int sendopts = TRUE;
int lossthresh = 0;
int fflag = FALSE;
int staticqid = 0;

u_int32 defgrp;				/* Default group if not specified */
u_int32 query_cast;			/* All routers multicast addr */
u_int32 resp_cast;			/* Mtrace response multicast addr */

u_int32 lcl_addr = 0;			/* This host address, in NET order */
u_int32 dst_netmask = 0;		/* netmask to go with qdst */

/*
 * Query/response parameters, all initialized to zero and set later
 * to default values or from options.
 */
u_int32 qsrc = 0;		/* Source address in the query */
u_int32 qgrp = 0;		/* Group address in the query */
u_int32 qdst = 0;		/* Destination (receiver) address in query */
u_char qno  = 0;		/* Max number of hops to query */
u_int32 raddr = 0;		/* Address where response should be sent */
int    qttl = 0;		/* TTL for the query packet */
u_char rttl = 0;		/* TTL for the response packet */
u_int32 gwy = 0;		/* User-supplied last-hop router address */
u_int32 tdst = 0;		/* Address where trace is sent (last-hop) */

char s1[19];		/* buffers to hold the string representations  */
char s2[19];		/* of IP addresses, to be passed to inet_fmt() */
char s3[19];		/* or inet_fmts().                             */

#if !(defined(BSD) && (BSD >= 199103))
extern int		errno;
extern int		sys_nerr;
extern char *		sys_errlist[];
#endif

#define RECV_BUF_SIZE 8192
char	*send_buf, *recv_buf;
int	igmp_socket;
u_int32	allrtrs_group;
char	router_alert[4];	     	/* Router Alert IP Option	    */
#ifndef	IPOPT_RA
#define	IPOPT_RA		148
#endif
#ifdef SUNOS5
char	eol[4];		     		/* EOL IP Option		    */
int ip_addlen = 0;		     	/* Workaround for Option bug #2     */
#endif

/*
 * max macro, with weird case to avoid conflicts
 */
#define	MaX(a,b)	((a) > (b) ? (a) : (b))

#ifndef __P
#ifdef __STDC__
#define __P(x)	x
#else
#define __P(x)	()
#endif
#endif

typedef int (*callback_t) __P((int, u_char *, int, struct igmp *, int,
			struct sockaddr *, int *, struct timeval *));

void			init_igmp __P((void));
void			send_igmp __P((u_int32 src, u_int32 dst, int type,
						int code, u_int32 group,
						int datalen));
int			inet_cksum __P((u_short *addr, u_int len));
void			k_set_rcvbuf __P((int bufsize));
void			k_hdr_include __P((int bool));
void			k_set_ttl __P((int t));
void			k_set_loop __P((int l));
void			k_set_if __P((u_int32 ifa));
void			k_join __P((u_int32 grp, u_int32 ifa));
void			k_leave __P((u_int32 grp, u_int32 ifa));
char *			inet_fmt __P((u_int32 addr, char *s));
char *			inet_fmts __P((u_int32 addr, u_int32 mask, char *s));
char *			inet_name __P((u_int32 addr));
u_int32			host_addr __P((char *name));
/* u_int is promoted u_char */
char *			proto_type __P((u_int type));
char *			flag_type __P((u_int type));

u_int32			get_netmask __P((int s, u_int32 *dst));
int			get_ttl __P((struct resp_buf *buf));
int			t_diff __P((u_long a, u_long b));
u_long			byteswap __P((u_long v));
int			mtrace_callback __P((int, u_char *, int, struct igmp *,
					int, struct sockaddr *, int *,
					struct timeval *));
int			send_recv __P((u_int32 dst, int type, int code,
					int tries, struct resp_buf *save,
					callback_t callback));
void			passive_mode __P((void));
char *			print_host __P((u_int32 addr));
char *			print_host2 __P((u_int32 addr1, u_int32 addr2));
void			print_trace __P((int idx, struct resp_buf *buf,
					char **names));
int			what_kind __P((struct resp_buf *buf, char *why));
char *			scale __P((int *hop));
void			stat_line __P((struct tr_resp *r, struct tr_resp *s,
					int have_next, int *res));
void			fixup_stats __P((struct resp_buf *base,
					struct resp_buf *prev,
					struct resp_buf *new,
					int *bugs));
int			check_thresh __P((int thresh,
					struct resp_buf *base,
					struct resp_buf *prev,
					struct resp_buf *new));
int			print_stats __P((struct resp_buf *base,
					struct resp_buf *prev,
					struct resp_buf *new,
					int *bugs,
					char **names));
int			path_changed __P((struct resp_buf *base,
					struct resp_buf *new));
void			check_vif_state __P((void));

int			main __P((int argc, char *argv[]));
void			log __P((int, int, char *, ...));
static void		usage __P((void));


/*
 * Open and initialize the igmp socket, and fill in the non-changing
 * IP header fields in the output packet buffer.
 */
void
init_igmp()
{
    struct ip *ip;

    recv_buf = (char *)malloc(RECV_BUF_SIZE);
    if (recv_buf == 0)
	log(LOG_ERR, 0, "Out of memory allocating recv_buf!");
    send_buf = (char *)malloc(RECV_BUF_SIZE);
    if (send_buf == 0)
	log(LOG_ERR, 0, "Out of memory allocating send_buf!");

    if ((igmp_socket = socket(AF_INET, SOCK_RAW, IPPROTO_IGMP)) < 0) 
	log(LOG_ERR, errno, "IGMP socket");

    k_hdr_include(TRUE);	/* include IP header when sending */
    k_set_rcvbuf(48*1024);	/* lots of input buffering        */
    k_set_ttl(1);		/* restrict multicasts to one hop */
    k_set_loop(FALSE);		/* disable multicast loopback     */

    ip         = (struct ip *)send_buf;
    ip->ip_hl  = sizeof(struct ip) >> 2;
    ip->ip_v   = IPVERSION;
    ip->ip_tos = 0;
    ip->ip_off = 0;
    ip->ip_p   = IPPROTO_IGMP;
    ip->ip_ttl = MAXTTL;	/* applies to unicasts only */

#ifndef INADDR_ALLRTRS_GROUP
#define	INADDR_ALLRTRS_GROUP	0xe0000002	/* 224.0.0.2 */
#endif
    allrtrs_group  = htonl(INADDR_ALLRTRS_GROUP);

    router_alert[0] = IPOPT_RA;	/* Router Alert */
    router_alert[1] = 4;	/* 4 bytes */
    router_alert[2] = 0;
    router_alert[3] = 0;
}

#ifdef SUNOS5
void
checkforsolarisbug()
{
    u_int32 localhost = htonl(0x7f000001);

    eol[0] = IPOPT_EOL;
    eol[1] = IPOPT_EOL;
    eol[2] = IPOPT_EOL;
    eol[3] = IPOPT_EOL;

    setsockopt(igmp_socket, IPPROTO_IP, IP_OPTIONS, eol, sizeof(eol));
    /*
     * Check if the kernel adds the options length to the packet
     * length.  Send myself an IGMP packet of type 0 (illegal),
     * with 4 IPOPT_EOL options, my PID (for collision detection)
     * and 4 bytes of zero (so that the checksum works whether
     * the 4 bytes of zero get truncated or not).
     */
    bzero(send_buf + MIN_IP_HEADER_LEN + IGMP_MINLEN, 8);
    *(int *)(send_buf + MIN_IP_HEADER_LEN + IGMP_MINLEN) = getpid();
    send_igmp(localhost, localhost, 0, 0, 0, 8);
    while (1) {
	int recvlen, dummy = 0;

	recvlen = recvfrom(igmp_socket, recv_buf, RECV_BUF_SIZE,
				0, NULL, &dummy);
	/* 8 == 4 bytes of options and 4 bytes of PID */
	if (recvlen >= MIN_IP_HEADER_LEN + IGMP_MINLEN + 8) {
	    struct ip *ip = (struct ip *)recv_buf;
	    struct igmp *igmp;
	    int *p;

	    if (ip->ip_hl != 6 ||
		ip->ip_p != IPPROTO_IGMP ||
	        ip->ip_src.s_addr != localhost ||
		ip->ip_dst.s_addr != localhost)
		continue;

	    igmp = (struct igmp *)(recv_buf + (ip->ip_hl << 2));
	    if (igmp->igmp_group.s_addr != 0)
		continue;
	    if (igmp->igmp_type != 0 || igmp->igmp_code != 0)
		continue;

	    p = (int *)((char *)igmp + IGMP_MINLEN);
	    if (*p != getpid())
		continue;

#ifdef RAW_INPUT_IS_RAW
	    ip->ip_len = ntohs(ip->ip_len);
#endif
	    if (ip->ip_len == IGMP_MINLEN + 4)
		ip_addlen = 4;
	    else if (ip->ip_len == IGMP_MINLEN + 8)
		ip_addlen = 0;
	    else
		log(LOG_ERR, 0, "while checking for Solaris bug: Sent %d bytes and got back %d!", IGMP_MINLEN + 8, ip->ip_len);

	    break;
	}
    }
}
#endif

/*
 * Construct an IGMP message in the output packet buffer.  The caller may
 * have already placed data in that buffer, of length 'datalen'.  Then send
 * the message from the interface with IP address 'src' to destination 'dst'.
 */
void
send_igmp(src, dst, type, code, group, datalen)
    u_int32 src, dst;
    int type, code;
    u_int32 group;
    int datalen;
{
    struct sockaddr_in sdst;
    struct ip *ip;
    struct igmp *igmp;
    int setloop = 0;
    static int raset = 0;
    int sendra = 0;
    int sendlen;

    ip                      = (struct ip *)send_buf;
    ip->ip_src.s_addr       = src;
    ip->ip_dst.s_addr       = dst;
    ip->ip_len              = MIN_IP_HEADER_LEN + IGMP_MINLEN + datalen;
    sendlen		    = ip->ip_len;
#ifdef SUNOS5
    ip->ip_len		   += ip_addlen;
#endif
#ifdef RAW_OUTPUT_IS_RAW
    ip->ip_len		    = htons(ip->ip_len);
#endif

    igmp                    = (struct igmp *)(send_buf + MIN_IP_HEADER_LEN);
    igmp->igmp_type         = type;
    igmp->igmp_code         = code;
    igmp->igmp_group.s_addr = group;
    igmp->igmp_cksum        = 0;
    igmp->igmp_cksum        = inet_cksum((u_short *)igmp,
					 IGMP_MINLEN + datalen);

    if (IN_MULTICAST(ntohl(dst))) {
	k_set_if(src);
	setloop = 1;
	k_set_loop(TRUE);
	if (dst != allrtrs_group)
	    sendra = 1;
    }

    if (sendopts && sendra && !raset) {
	setsockopt(igmp_socket, IPPROTO_IP, IP_OPTIONS,
			router_alert, sizeof(router_alert));
	raset = 1;
    } else if (!sendra && raset) {
#ifdef SUNOS5
	/*
	 * SunOS5 < 5.6 cannot properly reset the IP_OPTIONS "socket"
	 * option.  Instead, set up a string of 4 EOL's.
	 */
	setsockopt(igmp_socket, IPPROTO_IP, IP_OPTIONS,
			eol, sizeof(eol));
#else
	setsockopt(igmp_socket, IPPROTO_IP, IP_OPTIONS,
			NULL, 0);
#endif
	raset = 0;
    }

    bzero(&sdst, sizeof(sdst));
    sdst.sin_family = AF_INET;
#if (defined(BSD) && (BSD >= 199103))
    sdst.sin_len = sizeof(sdst);
#endif
    sdst.sin_addr.s_addr = dst;
    if (sendto(igmp_socket, send_buf, sendlen, 0,
			(struct sockaddr *)&sdst, sizeof(sdst)) < 0) {
	    log(LOG_WARNING, errno, "sendto to %s on %s",
		inet_fmt(dst, s1), inet_fmt(src, s2));
    }

    if (setloop)
	    k_set_loop(FALSE);

    log(LOG_DEBUG, 0, "SENT %s from %-15s to %s",
	type == IGMP_MTRACE ? "mtrace request" : "ask_neighbors",
	src == INADDR_ANY ? "INADDR_ANY" : inet_fmt(src, s1),
	inet_fmt(dst, s2));
}

/*
 * inet_cksum extracted from:
 *			P I N G . C
 *
 * Author -
 *	Mike Muuss
 *	U. S. Army Ballistic Research Laboratory
 *	December, 1983
 * Modified at Uc Berkeley
 *
 * (ping.c) Status -
 *	Public Domain.  Distribution Unlimited.
 *
 *			I N _ C K S U M
 *
 * Checksum routine for Internet Protocol family headers (C Version)
 *
 */
int
inet_cksum(addr, len)
	u_short *addr;
	u_int len;
{
	register int nleft = (int)len;
	register u_short *w = addr;
	u_short answer = 0;
	register int sum = 0;

	/*
	 *  Our algorithm is simple, using a 32 bit accumulator (sum),
	 *  we add sequential 16 bit words to it, and at the end, fold
	 *  back all the carry bits from the top 16 bits into the lower
	 *  16 bits.
	 */
	while (nleft > 1)  {
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if (nleft == 1) {
		*(u_char *) (&answer) = *(u_char *)w ;
		sum += answer;
	}

	/*
	 * add back carry outs from top 16 bits to low 16 bits
	 */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	answer = ~sum;				/* truncate to 16 bits */
	return (answer);
}

void
k_set_rcvbuf(bufsize)
    int bufsize;
{
    if (setsockopt(igmp_socket, SOL_SOCKET, SO_RCVBUF,
		   (char *)&bufsize, sizeof(bufsize)) < 0)
	log(LOG_ERR, errno, "setsockopt SO_RCVBUF %u", bufsize);
}


void
k_hdr_include(bool)
    int bool;
{
#ifdef IP_HDRINCL
    if (setsockopt(igmp_socket, IPPROTO_IP, IP_HDRINCL,
		   (char *)&bool, sizeof(bool)) < 0)
	log(LOG_ERR, errno, "setsockopt IP_HDRINCL %u", bool);
#endif
}

void
k_set_ttl(t)
    int t;
{
    u_char ttl;

    ttl = t;
    if (setsockopt(igmp_socket, IPPROTO_IP, IP_MULTICAST_TTL,
		   (char *)&ttl, sizeof(ttl)) < 0)
	log(LOG_ERR, errno, "setsockopt IP_MULTICAST_TTL %u", ttl);
}


void
k_set_loop(l)
    int l;
{
    u_char loop;

    loop = l;
    if (setsockopt(igmp_socket, IPPROTO_IP, IP_MULTICAST_LOOP,
		   (char *)&loop, sizeof(loop)) < 0)
	log(LOG_ERR, errno, "setsockopt IP_MULTICAST_LOOP %u", loop);
}

void
k_set_if(ifa)
    u_int32 ifa;
{
    struct in_addr adr;

    adr.s_addr = ifa;
    if (setsockopt(igmp_socket, IPPROTO_IP, IP_MULTICAST_IF,
		   (char *)&adr, sizeof(adr)) < 0)
	log(LOG_ERR, errno, "setsockopt IP_MULTICAST_IF %s",
	    		    inet_fmt(ifa, s1));
}

void
k_join(grp, ifa)
    u_int32 grp;
    u_int32 ifa;
{
    struct ip_mreq mreq;

    mreq.imr_multiaddr.s_addr = grp;
    mreq.imr_interface.s_addr = ifa;

    if (setsockopt(igmp_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
		   (char *)&mreq, sizeof(mreq)) < 0)
	log(LOG_WARNING, errno, "can't join group %s on interface %s",
				inet_fmt(grp, s1), inet_fmt(ifa, s2));
}


void
k_leave(grp, ifa)
    u_int32 grp;
    u_int32 ifa;
{
    struct ip_mreq mreq;

    mreq.imr_multiaddr.s_addr = grp;
    mreq.imr_interface.s_addr = ifa;

    if (setsockopt(igmp_socket, IPPROTO_IP, IP_DROP_MEMBERSHIP,
		   (char *)&mreq, sizeof(mreq)) < 0)
	log(LOG_WARNING, errno, "can't leave group %s on interface %s",
				inet_fmt(grp, s1), inet_fmt(ifa, s2));
}

/*
 * Convert an IP address in u_long (network) format into a printable string.
 */
char *
inet_fmt(addr, s)
    u_int32 addr;
    char *s;
{
    register u_char *a;

    a = (u_char *)&addr;
    sprintf(s, "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);
    return (s);
}


/*
 * Convert an IP subnet number in u_long (network) format into a printable
 * string including the netmask as a number of bits.
 */
char *
inet_fmts(addr, mask, s)
    u_int32 addr, mask;
    char *s;
{
    register u_char *a, *m;
    int bits;

    if ((addr == 0) && (mask == 0)) {
	sprintf(s, "default");
	return (s);
    }
    a = (u_char *)&addr;
    m = (u_char *)&mask;
    bits = 33 - ffs(ntohl(mask));

    if      (m[3] != 0) sprintf(s, "%u.%u.%u.%u/%d", a[0], a[1], a[2], a[3],
						bits);
    else if (m[2] != 0) sprintf(s, "%u.%u.%u/%d",    a[0], a[1], a[2], bits);
    else if (m[1] != 0) sprintf(s, "%u.%u/%d",       a[0], a[1], bits);
    else                sprintf(s, "%u/%d",          a[0], bits);

    return (s);
}

char   *
inet_name(addr)
    u_int32  addr;
{
    struct hostent *e;

    e = gethostbyaddr((char *)&addr, sizeof(addr), AF_INET);

    return e ? e->h_name : "?";
}


u_int32 
host_addr(name)
    char   *name;
{
    struct hostent *e = (struct hostent *)0;
    u_int32  addr;
    int	i, dots = 3;
    char	buf[40];
    char	*ip = name;
    char	*op = buf;

    /*
     * Undo BSD's favor -- take fewer than 4 octets as net/subnet address
     * if the name is all numeric.
     */
    for (i = sizeof(buf) - 7; i > 0; --i) {
	if (*ip == '.') --dots;
	else if (*ip == '\0') break;
	else if (!isdigit(*ip)) dots = 0;  /* Not numeric, don't add zeroes */
	*op++ = *ip++;
    }
    for (i = 0; i < dots; ++i) {
	*op++ = '.';
	*op++ = '0';
    }
    *op = '\0';

    if (dots <= 0)
	e = gethostbyname(name);
    if (e && (e->h_length == sizeof(addr))) {
	memcpy((char *)&addr, e->h_addr_list[0], e->h_length);
	if (e->h_addr_list[1])
	    fprintf(stderr, "Warning: %s has multiple addresses, using %s\n",
			name, inet_fmt(addr, s1));
    } else {
	addr = inet_addr(buf);
	if (addr == -1 || (IN_MULTICAST(addr) && dots)) {
	    addr = 0;
	    printf("Could not parse %s as host name or address\n", name);
	}
    }
    return addr;
}


char *
proto_type(type)
    u_int type;
{
    static char buf[80];

    switch (type) {
      case PROTO_DVMRP:
	return ("DVMRP");
      case PROTO_MOSPF:
	return ("MOSPF");
      case PROTO_PIM:
	return ("PIM");
      case PROTO_CBT:
	return ("CBT");
      case PROTO_PIM_SPECIAL:
	return ("PIM/Special");
      case PROTO_PIM_STATIC:
	return ("PIM/Static");
      case PROTO_DVMRP_STATIC:
	return ("DVMRP/Static");
      case PROTO_PIM_BGP4PLUS:
	return ("PIM/BGP4+");
      case PROTO_CBT_SPECIAL:
	return ("CBT/Special");
      case PROTO_CBT_STATIC:
	return ("CBT/Static");
      case PROTO_PIM_ASSERT:
	return ("PIM/Assert");
      case 0:
	return ("None");
      default:
	(void) sprintf(buf, "Unknown protocol code %d", type);
	return (buf);
    }
}


char *
flag_type(type)
    u_int type;
{
    static char buf[80];

    switch (type) {
      case TR_NO_ERR:
	return ("");
      case TR_WRONG_IF:
	return ("Wrong interface");
      case TR_PRUNED:
	return ("Prune sent upstream");
      case TR_OPRUNED:
	return ("Output pruned");
      case TR_SCOPED:
	return ("Hit scope boundary");
      case TR_NO_RTE:
	return ("No route");
      case TR_NO_FWD:
	return ("Not forwarding");
      case TR_HIT_RP:
	return ("Reached RP/Core");
      case TR_RPF_IF:
	return ("RPF Interface");
      case TR_NO_MULTI:
	return ("Multicast disabled");
      case TR_OLD_ROUTER:
	return ("Next router no mtrace");
      case TR_NO_SPACE:
	return ("No space in packet");
      case TR_ADMIN_PROHIB:
	return ("Admin. Prohibited");
      default:
	(void) sprintf(buf, "Unknown error code %d", type);
	return (buf);
    }
}    

/*
 * If destination is on a local net, get the netmask, else set the
 * netmask to all ones.  There are two side effects: if the local
 * address was not explicitly set, and if the destination is on a
 * local net, use that one; in either case, verify that the local
 * address is valid.
 */
u_int32
get_netmask(s, dst)
    int s;
    u_int32 *dst;
{
    unsigned int n;
    struct ifconf ifc;
    struct ifreq *ifrp, *ifend;
    u_int32 if_addr, if_mask;
    u_int32 retval = 0xFFFFFFFF;
    int found = FALSE;
    int num_ifreq = 32;

    ifc.ifc_len = num_ifreq * sizeof(struct ifreq);
    ifc.ifc_buf = malloc(ifc.ifc_len);
    while (ifc.ifc_buf) {
	if (ioctl(s, SIOCGIFCONF, (char *)&ifc) < 0) {
	    perror("ioctl SIOCGIFCONF");
	    return retval;
	}

	/*
	 * If the buffer was large enough to hold all the addresses
	 * then break out, otherwise increase the buffer size and
	 * try again.
	 *
	 * The only way to know that we definitely had enough space
	 * is to know that there was enough space for at least one
	 * more struct ifreq. ???
	 */
	if ((num_ifreq * sizeof(struct ifreq)) >=
	     ifc.ifc_len + sizeof(struct ifreq))
	     break;

	num_ifreq *= 2;
	ifc.ifc_len = num_ifreq * sizeof(struct ifreq);
	ifc.ifc_buf = realloc(ifc.ifc_buf, ifc.ifc_len);
    }
    if (ifc.ifc_buf == NULL) {
	fprintf(stderr, "getting interface list: ran out of memory");
	exit(1);
    }

    ifrp = (struct ifreq *)ifc.ifc_buf;
    ifend = (struct ifreq *)(ifc.ifc_buf + ifc.ifc_len);
    /*
     * Loop through all of the interfaces.
     */
    for (; ifrp < ifend && !found; ifrp = (struct ifreq *)((char *)ifrp + n)) {
#if BSD >= 199006
	n = ifrp->ifr_addr.sa_len + sizeof(ifrp->ifr_name);
	if (n < sizeof(*ifrp))
	    n = sizeof(*ifrp);
#else
	n = sizeof(*ifrp);
#endif
	/*
	 * Ignore any interface for an address family other than IP.
	 */
	if (ifrp->ifr_addr.sa_family != AF_INET)
	    continue;

	if_addr = ((struct sockaddr_in *)&(ifrp->ifr_addr))->sin_addr.s_addr;
	if (ioctl(s, SIOCGIFFLAGS, (char *)ifrp) < 0) {
	    fprintf(stderr, "SIOCGIFFLAGS on ");
	    perror(ifrp->ifr_name);
	    continue;
	}
	if ((ifrp->ifr_flags & (IFF_MULTICAST|IFF_UP|IFF_LOOPBACK)) !=
				(IFF_MULTICAST|IFF_UP))
	    continue;
	if (*dst == 0)
	    *dst = if_addr;
	if (ioctl(s, SIOCGIFNETMASK, (char *)ifrp) >= 0) {
	    if_mask = ((struct sockaddr_in *)&(ifrp->ifr_addr))->sin_addr.s_addr;
	    if (if_mask != 0 && (*dst & if_mask) == (if_addr & if_mask)) {
		retval = if_mask;
		if (lcl_addr == 0) lcl_addr = if_addr;	/* XXX what about aliases? */
	    }
	}
	if (lcl_addr == if_addr) found = TRUE;
    }
    if (!found && lcl_addr != 0) {
	printf("Interface address is not valid\n");
	exit(1);
    }
    return (retval);
}


/*
 * Try to pick a TTL that will get past all the thresholds in the path.
 */
int
get_ttl(buf)
    struct resp_buf *buf;
{
    int rno;
    struct tr_resp *b;
    u_int ttl;

    if (buf && (rno = buf->len) > 0) {
	b = buf->resps + rno - 1;
	ttl = b->tr_fttl;

	while (--rno > 0) {
	    --b;
	    if (ttl < b->tr_fttl) ttl = b->tr_fttl;
	    else ++ttl;
	}
	ttl += MULTICAST_TTL_INC;
	if (ttl < MULTICAST_TTL1) ttl = MULTICAST_TTL1;
	if (ttl > MULTICAST_TTL_MAX) ttl = MULTICAST_TTL_MAX;
	return (ttl);
    } else return(MULTICAST_TTL1);
}

/*
 * Calculate the difference between two 32-bit NTP timestamps and return
 * the result in milliseconds.
 */
int
t_diff(a, b)
    u_long a, b;
{
    int d = a - b;

    return ((d * 125) >> 13);
}

/*
 * Swap bytes for poor little-endian machines that don't byte-swap
 */
u_long
byteswap(v)
    u_long v;
{
    return ((v << 24) | ((v & 0xff00) << 8) |
	    ((v >> 8) & 0xff00) | (v >> 24));
}

#if 0
/*
 * XXX incomplete - need private callback data, too?
 * XXX since dst doesn't get passed through?
 */
int
neighbors_callback(tmo, buf, buflen, igmp, igmplen, addr, addrlen, ts)
    int tmo;
    u_char *buf;
    int buflen;
    struct igmp *igmp;
    int igmplen;
    struct sockaddr *addr;
    int *addrlen;
    struct timeval *ts;
{
    int len;
    u_int32 dst;
    struct ip *ip = (struct ip *)buf;

    if (tmo)
	return 0;

    if (igmp->igmp_code != DVMRP_NEIGHBORS2)
	return 0;
    len = igmplen;
    /*
     * Accept DVMRP_NEIGHBORS2 response if it comes from the
     * address queried or if that address is one of the local
     * addresses in the response.
     */
    if (ip->ip_src.s_addr != dst) {
	u_int32 *p = (u_int32 *)(igmp + 1);
	u_int32 *ep = p + (len >> 2);
	while (p < ep) {
	    u_int32 laddr = *p++;
	    int n = ntohl(*p++) & 0xFF;
	    if (laddr == dst) {
		ep = p + 1;		/* ensure p < ep after loop */
		break;
	    }
	    p += n;
	}
	if (p >= ep)
	    return 0;
    }
    return buflen;
}
#endif

int
mtrace_callback(tmo, buf, buflen, igmp, igmplen, addr, addrlen, ts)
    int tmo;
    u_char *buf;
    int buflen;
    struct igmp *igmp;
    int igmplen;
    struct sockaddr *addr;
    int *addrlen;
    struct timeval *ts;
{
    static u_char *savbuf = NULL;
    static int savbuflen;
    static struct sockaddr *savaddr;
    static int savaddrlen;
    static struct timeval savts;

    int len = (igmplen - QLEN) / RLEN;
    struct tr_resp *r = (struct tr_resp *)((struct tr_query *)(igmp + 1) + 1);

    if (tmo == 1) {
	/*
	 * If we timed out with a packet saved, then return that packet.
	 * send_recv won't send this same packet to the callback again.
	 */
	if (savbuf) {
	    bcopy(savbuf, buf, savbuflen);
	    free(savbuf);
	    savbuf = NULL;
	    bcopy(savaddr, addr, savaddrlen);
	    free(savaddr);
	    *addrlen = savaddrlen;
	    bcopy(&savts, ts, sizeof(savts));
	    return savbuflen;
	}
	return 0;
    }
    if (savbuf) {
	free(savbuf);
	savbuf = NULL;
	free(savaddr);
    }
    /*
     * Check for IOS bug described in CSCdi68628, where a router that does
     *  not have multicast enabled responds to an mtrace request with a 1-hop
     *  error packet.
     * Heuristic is:
     *  If there is only one hop reported in the packet,
     *	And the protocol code is 0,
     *  And there is no previous hop,
     *	And the forwarding information is "Not Forwarding",
     *	And the router is not on the same subnet as the destination of the
     *		trace,
     *  then drop this packet.  The "#if 0"'d code saves it and returns
     *   it on timeout, but timeouts are too common (e.g. routers with
     *   limited unicast routing tables, etc).
     */
    if (len == 1 && r->tr_rproto == 0 && r->tr_rmtaddr == 0 &&
					r->tr_rflags == TR_NO_FWD) {
	u_int32 smask;

	VAL_TO_MASK(smask, r->tr_smask);
	if ((r->tr_outaddr & smask) != (qdst & smask)) {
#if 0
	    /* XXX should do this silently? */
	    fprintf(stderr, "mtrace: probably IOS-buggy packet from %s\n",
		inet_fmt(((struct sockaddr_in *)addr)->sin_addr.s_addr, s1));
	    /* Save the packet to return if a timeout occurs. */
	    savbuf = (u_char *)malloc(buflen);
	    if (savbuf != NULL) {
		bcopy(buf, savbuf, buflen);
		savbuflen = buflen;
		savaddr = (struct sockaddr *)malloc(*addrlen);
		if (savaddr != NULL) {
		    bcopy(addr, savaddr, *addrlen);
		    savaddrlen = *addrlen;
		    bcopy(ts, &savts, sizeof(savts));
		} else {
		    free(savbuf);
		    savbuf = NULL;
		}
	    }
#endif
	    return 0;
	}
    }
    return buflen;
}

int
send_recv(dst, type, code, tries, save, callback)
    u_int32 dst;
    int type, code, tries;
    struct resp_buf *save;
    callback_t callback;
{
    fd_set  fds;
    struct timeval tq, tr, tv;
    struct ip *ip;
    struct igmp *igmp;
    struct tr_query *query, *rquery;
    struct tr_resp *r;
    struct sockaddr_in recvaddr;
    u_int32 local, group;
    int ipdatalen, iphdrlen, igmpdatalen;
    int datalen;
    int count, recvlen, socklen = sizeof(recvaddr);
    int len;
    int i;

    if (type == IGMP_MTRACE) {
	group = qgrp;
	datalen = sizeof(struct tr_query);
    } else {
	group = htonl(0xff03);
	datalen = 0;
    }
    if (IN_MULTICAST(ntohl(dst))) local = lcl_addr;
    else local = INADDR_ANY;

    /*
     * If the reply address was not explictly specified, start off
     * with the standard multicast reply address, or the unicast
     * address of this host if the unicast flag was specified.
     * Then, if there is no response after trying half the tries
     * with multicast, switch to the unicast address of this host
     * if the multicast flag was not specified.  If the TTL was
     * also not specified, set a multicast TTL and increase it
     * for every try.
     */
    query = (struct tr_query *)(send_buf + MIN_IP_HEADER_LEN + IGMP_MINLEN);
    query->tr_raddr = raddr ? raddr : unicast ? lcl_addr : resp_cast;
    TR_SETTTL(query->tr_rttlqid, rttl ? rttl :
      IN_MULTICAST(ntohl(query->tr_raddr)) ? get_ttl(save) : UNICAST_TTL);
    query->tr_src   = qsrc;
    query->tr_dst   = qdst;

    for (i = tries ; i > 0; --i) {
	int oqid;

	if (tries == nqueries && raddr == 0) {
	    if (i == (nqueries >> 1)) {
		if (multicast && unicast) {
		    query->tr_raddr = resp_cast;
		    if (!rttl)
			TR_SETTTL(query->tr_rttlqid, get_ttl(save));
		} else if (!multicast) {
		    query->tr_raddr = lcl_addr;
		    TR_SETTTL(query->tr_rttlqid, UNICAST_TTL);
		}
	    }
	    if (i < tries && IN_MULTICAST(ntohl(query->tr_raddr)) &&
								rttl == 0) {
		TR_SETTTL(query->tr_rttlqid,
			TR_GETTTL(query->tr_rttlqid) + MULTICAST_TTL_INC);
		if (TR_GETTTL(query->tr_rttlqid) > MULTICAST_TTL_MAX)
		  TR_SETTTL(query->tr_rttlqid, MULTICAST_TTL_MAX);
	    }
	}

	/*
	 * Change the qid for each request sent to avoid being confused
	 * by duplicate responses
	 */
	oqid = TR_GETQID(query->tr_rttlqid);
	if (staticqid)
	    TR_SETQID(query->tr_rttlqid, staticqid);
	else
#ifdef SYSV    
	    TR_SETQID(query->tr_rttlqid, ((u_int32)lrand48() >> 8));
#else
	    TR_SETQID(query->tr_rttlqid, ((u_int32)arc4random() >> 8));
#endif

	/*
	 * Set timer to calculate delays, then send query
	 */
	gettimeofday(&tq, 0);
	send_igmp(local, dst, type, code, group, datalen);

	/*
	 * Wait for response, discarding false alarms
	 */
	while (TRUE) {
	    if (igmp_socket >= FD_SETSIZE)
		    log(LOG_ERR, 0, "descriptor too big");
	    FD_ZERO(&fds);
	    FD_SET(igmp_socket, &fds);
	    gettimeofday(&tv, 0);
	    tv.tv_sec = tq.tv_sec + timeout - tv.tv_sec;
	    tv.tv_usec = tq.tv_usec - tv.tv_usec;
	    if (tv.tv_usec < 0) tv.tv_usec += 1000000L, --tv.tv_sec;
	    if (tv.tv_sec < 0) tv.tv_sec = tv.tv_usec = 0;

	    count = select(igmp_socket + 1, &fds, (fd_set *)0, (fd_set *)0,
			   &tv);

	    if (count < 0) {
		if (errno != EINTR) warn("select");
		continue;
	    } else if (count == 0) {
		/*
		 * Timed out.  Notify the callback.
		 */
		if (!callback || (recvlen = (callback)(1, recv_buf, 0, NULL, 0, (struct sockaddr *)&recvaddr, &socklen, &tr)) == 0) {
		    printf("* ");
		    fflush(stdout);
		    break;
		}
	    } else {
		/*
		 * Data is available on the socket, so read it.
		 */
		gettimeofday(&tr, 0);
		recvlen = recvfrom(igmp_socket, recv_buf, RECV_BUF_SIZE,
				   0, (struct sockaddr *)&recvaddr, &socklen);
	    }

	    if (recvlen <= 0) {
		if (recvlen && errno != EINTR) warn("recvfrom");
		continue;
	    }

	    if (recvlen < sizeof(struct ip)) {
		warnx("packet too short (%u bytes) for IP header", recvlen);
		continue;
	    }
	    ip = (struct ip *) recv_buf;
	    if (ip->ip_p == 0)	/* ignore cache creation requests */
		continue;

	    iphdrlen = ip->ip_hl << 2;
#ifdef RAW_INPUT_IS_RAW
	    ipdatalen = ntohs(ip->ip_len);
#else
	    ipdatalen = ip->ip_len;
#endif
	    if (iphdrlen + ipdatalen != recvlen) {
		warnx("packet shorter (%u bytes) than hdr+data len (%u+%u)",
			recvlen, iphdrlen, ipdatalen);
		continue;
	    }

	    igmp = (struct igmp *) (recv_buf + iphdrlen);
	    igmpdatalen = ipdatalen - IGMP_MINLEN;
	    if (igmpdatalen < 0) {
		warnx("IP data field too short (%u bytes) for IGMP from %s",
			ipdatalen, inet_fmt(ip->ip_src.s_addr, s1));
		continue;
	    }

	    switch (igmp->igmp_type) {

	      case IGMP_DVMRP:
		if (type != IGMP_DVMRP || code != DVMRP_ASK_NEIGHBORS2)
			continue;
		if (igmp->igmp_code != DVMRP_NEIGHBORS2) continue;
		len = igmpdatalen;
		/*
		 * Accept DVMRP_NEIGHBORS2 response if it comes from the
		 * address queried or if that address is one of the local
		 * addresses in the response.
		 */
		if (ip->ip_src.s_addr != dst) {
		    u_int32 *p = (u_int32 *)(igmp + 1);
		    u_int32 *ep = p + (len >> 2);
		    while (p < ep) {
			u_int32 laddr = *p++;
			int n = ntohl(*p++) & 0xFF;
			if (laddr == dst) {
			    ep = p + 1;		/* ensure p < ep after loop */
			    break;
			}
			p += n;
		    }
		    if (p >= ep) continue;
		}
		break;

	      case IGMP_MTRACE:	    /* For backward compatibility with 3.3 */
	      case IGMP_MTRACE_RESP:
		if (type != IGMP_MTRACE) continue;
		if (igmpdatalen <= QLEN) continue;
		if ((igmpdatalen - QLEN)%RLEN) {
		    printf("packet with incomplete responses (%d bytes)\n",
			igmpdatalen);
		    continue;
		}

		/*
		 * Ignore responses that don't match query.
		 */
		rquery = (struct tr_query *)(igmp + 1);
		if (rquery->tr_src != qsrc || rquery->tr_dst != qdst)
		    continue;
		if (TR_GETQID(rquery->tr_rttlqid) !=
			TR_GETQID(query->tr_rttlqid)) {
		    if (verbose && TR_GETQID(rquery->tr_rttlqid) == oqid)
			printf("[D]");
		    continue;
		}
		len = (igmpdatalen - QLEN)/RLEN;
		r = (struct tr_resp *)(rquery+1) + len - 1;

		/*
		 * Ignore trace queries passing through this node when
		 * mtrace is run on an mrouter that is in the path
		 * (needed only because IGMP_MTRACE is accepted above
		 * for backward compatibility with multicast release 3.3).
		 */
		if (igmp->igmp_type == IGMP_MTRACE) {
		    u_int32 smask;

		    VAL_TO_MASK(smask, r->tr_smask);
		    if (len < code && (r->tr_inaddr & smask) != (qsrc & smask)
			&& r->tr_rmtaddr != 0 && !(r->tr_rflags & 0x80))
		      continue;
		}
		/*
		 * Some routers will return error messages without
		 * filling in their addresses.  We fill in the address
		 * for them.
		 */
		if (r->tr_outaddr == 0)
		    r->tr_outaddr = recvaddr.sin_addr.s_addr;

		/*
		 * A match, we'll keep this one.
		 */
		if (len > code) {
		    warnx("num hops received (%d) exceeds request (%d)",
			    len, code);
		}
		rquery->tr_raddr = query->tr_raddr;	/* Insure these are */
		TR_SETTTL(rquery->tr_rttlqid, TR_GETTTL(query->tr_rttlqid));
							/* as we sent them */
		break;

	      default:
		continue;
	    }

	    /*
	     * We're pretty sure we want to use this packet now,
	     * but if the caller gave a callback function, it might
	     * want to handle it instead.  Give the callback a chance,
	     * unless the select timed out (in which case the only way
	     * to get here is because the callback returned a packet).
	     */
	    if (callback && (count != 0) && ((callback)(0, recv_buf, recvlen, igmp, igmpdatalen, (struct sockaddr*)&recvaddr, &socklen, &tr)) == 0) {
		/*
		 * The callback function didn't like this packet.
		 * Go try receiving another one.
		 */
		continue;
	    }

	    /*
	     * Most of the sanity checking done at this point.
	     * Return this packet we have been waiting for.
	     */
	    if (save) {
		save->qtime = ((tq.tv_sec + JAN_1970) << 16) +
			      (tq.tv_usec << 10) / 15625;
		save->rtime = ((tr.tv_sec + JAN_1970) << 16) +
			      (tr.tv_usec << 10) / 15625;
		save->len = len;
		bcopy((char *)igmp, (char *)&save->igmp, ipdatalen);
	    }
	    return (recvlen);
	}
    }
    return (0);
}

/*
 * Most of this code is duplicated elsewhere.  I'm not sure if
 * the duplication is absolutely required or not.
 *
 * Ideally, this would keep track of ongoing statistics
 * collection and print out statistics.  (& keep track
 * of h-b-h traces and only print the longest)  For now,
 * it just snoops on what traces it can.
 */
void
passive_mode()
{
    struct timeval tr;
    time_t tr_sec;
    struct ip *ip;
    struct igmp *igmp;
    struct tr_resp *r;
    struct sockaddr_in recvaddr;
    struct tm *now;
    char timebuf[32];
    int socklen;
    int ipdatalen, iphdrlen, igmpdatalen;
    int len, recvlen;
    int qid;
    u_int32 smask;
    struct mtrace *remembered = NULL, *m, *n, **nn;
    int pc = 0;

    if (raddr) {
	if (IN_MULTICAST(ntohl(raddr))) k_join(raddr, lcl_addr);
    } else k_join(htonl(0xE0000120), lcl_addr);

    while (1) {
	fflush(stdout);		/* make sure previous trace is flushed */

	socklen = sizeof(recvaddr);
	recvlen = recvfrom(igmp_socket, recv_buf, RECV_BUF_SIZE,
			   0, (struct sockaddr *)&recvaddr, &socklen);
	gettimeofday(&tr,0);

	if (recvlen <= 0) {
	    if (recvlen && errno != EINTR) warn("recvfrom");
	    continue;
	}

	if (recvlen < sizeof(struct ip)) {
	    warnx("packet too short (%u bytes) for IP header", recvlen);
	    continue;
	}
	ip = (struct ip *) recv_buf;
	if (ip->ip_p == 0)	/* ignore cache creation requests */
	    continue;

	iphdrlen = ip->ip_hl << 2;
#ifdef RAW_INPUT_IS_RAW
	ipdatalen = ntohs(ip->ip_len);
#else
	ipdatalen = ip->ip_len;
#endif
	if (iphdrlen + ipdatalen != recvlen) {
	    warnx("packet shorter (%u bytes) than hdr+data len (%u+%u)",
		    recvlen, iphdrlen, ipdatalen);
	    continue;
	}

	igmp = (struct igmp *) (recv_buf + iphdrlen);
	igmpdatalen = ipdatalen - IGMP_MINLEN;
	if (igmpdatalen < 0) {
	    warnx("IP data field too short (%u bytes) for IGMP from %s",
		    ipdatalen, inet_fmt(ip->ip_src.s_addr, s1));
	    continue;
	}

	switch (igmp->igmp_type) {

	  case IGMP_MTRACE:	    /* For backward compatibility with 3.3 */
	  case IGMP_MTRACE_RESP:
	    if (igmpdatalen < QLEN) continue;
	    if ((igmpdatalen - QLEN)%RLEN) {
		printf("packet with incorrect datalen\n");
		continue;
	    }

	    len = (igmpdatalen - QLEN)/RLEN;

	    break;

	  default:
	    continue;
	}

	base.qtime = ((tr.tv_sec + JAN_1970) << 16) +
		      (tr.tv_usec << 10) / 15625;
	base.rtime = ((tr.tv_sec + JAN_1970) << 16) +
		      (tr.tv_usec << 10) / 15625;
	base.len = len;
	bcopy((char *)igmp, (char *)&base.igmp, ipdatalen);
	/*
	 * If the user specified which traces to monitor,
	 * only accept traces that correspond to the
	 * request
	 */
	if ((qsrc != 0 && qsrc != base.qhdr.tr_src) ||
	    (qdst != 0 && qdst != base.qhdr.tr_dst) ||
	    (qgrp != 0 && qgrp != igmp->igmp_group.s_addr))
	    continue;

	/* XXX This should be a hash table */
	/* XXX garbage-collection should be more efficient */
	for (nn = &remembered, n = *nn, m = 0; n; n = *nn) {
	    if ((n->base.qhdr.tr_src == base.qhdr.tr_src) &&
		(n->base.qhdr.tr_dst == base.qhdr.tr_dst) &&
		(n->base.igmp.igmp_group.s_addr == igmp->igmp_group.s_addr)) {
		m = n;
		m->last = tr;
	    }
	    if (tr.tv_sec - n->last.tv_sec > 500) { /* XXX don't hardcode */
		*nn = n->next;
		free(n);
	    } else {
		nn = &n->next;
	    }
	}

	tr_sec = tr.tv_sec;
	now = localtime(&tr_sec);
	strftime(timebuf, sizeof(timebuf) - 1, "%b %e %k:%M:%S", now);
	printf("Mtrace %s at %s",
		len == 0 ? "query" :
			   igmp->igmp_type == IGMP_MTRACE_RESP ? "response" :
								 "in transit",
		timebuf);
	if (len == 0)
		printf(" by %s", inet_fmt(recvaddr.sin_addr.s_addr, s1));
	if (!IN_MULTICAST(base.qhdr.tr_raddr))
		printf(", resp to %s", (len == 0 && recvaddr.sin_addr.s_addr == base.qhdr.tr_raddr) ? "same" : inet_fmt(base.qhdr.tr_raddr, s1));
	else
		printf(", respttl %d", TR_GETTTL(base.qhdr.tr_rttlqid));
	printf(", qid %06x\n", qid = TR_GETQID(base.qhdr.tr_rttlqid));
	printf("packet from %s to %s\n",
		inet_fmt(ip->ip_src.s_addr, s1),
		inet_fmt(ip->ip_dst.s_addr, s2));

	printf("from %s to %s via group %s (mxhop=%d)\n",
		inet_fmt(base.qhdr.tr_dst, s1), inet_fmt(base.qhdr.tr_src, s2),
		inet_fmt(igmp->igmp_group.s_addr, s3), igmp->igmp_code);
	if (len == 0) {
	    printf("\n");
	    continue;
	}
	r = base.resps + base.len - 1;
	/*
	 * Some routers will return error messages without
	 * filling in their addresses.  We fill in the address
	 * for them.
	 */
	if (r->tr_outaddr == 0)
	    r->tr_outaddr = recvaddr.sin_addr.s_addr;

	/*
	 * If there was a previous trace, it see if this is a
	 * statistics candidate.
	 */
	if (m && base.len == m->base.len &&
		!(pc = path_changed(&m->base, &base))) {
	    /*
	     * Some mtrace responders send multiple copies of the same
	     * reply.  Skip this packet if it's got the same query-id
	     * as the last one.
	     */
	    if (m->lastqid == qid) {
		printf("Skipping duplicate reply\n");
		continue;
	    }

	    m->lastqid = qid;

	    ++m->nresp;

	    bcopy(&base, m->new, sizeof(base));

	    printf("Results after %d seconds:\n\n",
		   (int)((m->new->qtime - m->base.qtime) >> 16));
	    fixup_stats(&m->base, m->prev, m->new, m->bugs);
	    print_stats(&m->base, m->prev, m->new, m->bugs, m->names);
	    m->prev = m->new;
	    m->new = &m->incr[(m->nresp & 1)];

	    continue;
	}

	if (m == NULL) {
	    m = (struct mtrace *)malloc(sizeof(struct mtrace));
	    if (m == NULL) {
		fprintf(stderr, "Out of memory!\n");
		continue;
	    }
	    bzero(m, sizeof(struct mtrace));
	    m->next = remembered;
	    remembered = m;
	    bcopy(&tr, &m->last, sizeof(tr));
	}

	/* Either it's a hop-by-hop in progress, or the path changed. */
	if (pc) {
	    printf("[Path Changed...]\n");
	    bzero(m->bugs, sizeof(m->bugs));
	}
	bcopy(&base, &m->base, sizeof(base));
	m->prev = &m->base;
	m->new = &m->incr[0];
	m->nresp = 0;

	printf("  0  ");
	print_host(base.qhdr.tr_dst);
	printf("\n");
	print_trace(1, &base, m->names);
	VAL_TO_MASK(smask, r->tr_smask);
	if ((r->tr_inaddr & smask) == (base.qhdr.tr_src & smask)) {
	    printf("%3d  ", -(base.len+1));
	    print_host(base.qhdr.tr_src);
	    printf("\n");
	} else if (r->tr_rmtaddr != 0) {
	    printf("%3d  ", -(base.len+1));
	    print_host(r->tr_rmtaddr);
	    printf(" %s\n", r->tr_rflags == TR_OLD_ROUTER ?
				   "doesn't support mtrace"
				 : "is the next hop");
	}
	printf("\n");
    }
}

char *
print_host(addr)
    u_int32 addr;
{
    return print_host2(addr, 0);
}

/*
 * On some routers, one interface has a name and the other doesn't.
 * We always print the address of the outgoing interface, but can
 * sometimes get the name from the incoming interface.  This might be
 * confusing but should be slightly more helpful than just a "?".
 */
char *
print_host2(addr1, addr2)
    u_int32 addr1, addr2;
{
    char *name;

    if (numeric) {
	printf("%s", inet_fmt(addr1, s1));
	return ("");
    }
    name = inet_name(addr1);
    if (*name == '?' && *(name + 1) == '\0' && addr2 != 0)
	name = inet_name(addr2);
    printf("%s (%s)", name, inet_fmt(addr1, s1));
    return (name);
}

/*
 * Print responses as received (reverse path from dst to src)
 */
void
print_trace(idx, buf, names)
    int idx;
    struct resp_buf *buf;
    char **names;
{
    struct tr_resp *r;
    char *name;
    int i;
    int hop;
    char *ms;

    i = abs(idx);
    r = buf->resps + i - 1;

    for (; i <= buf->len; ++i, ++r) {
	if (idx > 0) printf("%3d  ", -i);
	name = print_host2(r->tr_outaddr, r->tr_inaddr);
	if (r->tr_rflags != TR_NO_RTE)
	    printf("  %s  thresh^ %d", proto_type(r->tr_rproto), r->tr_fttl);
	if (verbose) {
	    hop = t_diff(ntohl(r->tr_qarr), buf->qtime);
	    ms = scale(&hop);
	    printf("  %d%s", hop, ms);
	}
	printf("  %s", flag_type(r->tr_rflags));
	if (i > 1 && r->tr_outaddr != (r-1)->tr_rmtaddr) {
	    printf(" !RPF!");
	    print_host((r-1)->tr_rmtaddr);
	}
	if (r->tr_rflags != TR_NO_RTE) {
	    if (r->tr_smask <= 1)    /* MASK_TO_VAL() returns 1 for default */
		printf(" [default]");
	    else if (verbose) {
		u_int32 smask;

		VAL_TO_MASK(smask, r->tr_smask);
		printf(" [%s]", inet_fmts(buf->qhdr.tr_src & smask,
							smask, s1));
	    }
	}
	printf("\n");
	if (names[i-1])
	    free(names[i-1]);
	names[i-1]=malloc(strlen(name) + 1);
	strcpy(names[i-1], name);
    }
}

/*
 * See what kind of router is the next hop
 */
int
what_kind(buf, why)
    struct resp_buf *buf;
    char *why;
{
    u_int32 smask;
    int retval;
    int hops = buf->len;
    struct tr_resp *r = buf->resps + hops - 1;
    u_int32 next = r->tr_rmtaddr;

    retval = send_recv(next, IGMP_DVMRP, DVMRP_ASK_NEIGHBORS2, 1, &incr[0], NULL);
    print_host(next);
    if (retval) {
	u_int32 version = ntohl(incr[0].igmp.igmp_group.s_addr);
	u_int32 *p = (u_int32 *)incr[0].ndata;
	u_int32 *ep = p + (incr[0].len >> 2);
	char *type = "version ";

	retval = 0;
	switch (version & 0xFF) {
	  case 1:
	    type = "proteon/mrouted ";
	    retval = 1;
	    break;

	  case 10:
	  case 11:
	    type = "cisco ";
	}
	printf(" [%s%d.%d] %s\n",
	       type, version & 0xFF, (version >> 8) & 0xFF,
	       why);
	VAL_TO_MASK(smask, r->tr_smask);
	while (p < ep) {
	    u_int32 laddr = *p++;
	    int flags = (ntohl(*p) & 0xFF00) >> 8;
	    int n = ntohl(*p++) & 0xFF;
	    if (!(flags & (DVMRP_NF_DOWN | DVMRP_NF_DISABLED)) &&
		 (laddr & smask) == (qsrc & smask)) {
		printf("%3d  ", -(hops+2));
		print_host(qsrc);
		printf("\n");
		return 1;
	    }
	    p += n;
	}
	return retval;
    }
    printf(" %s\n", why);
    return 0;
}


char *
scale(hop)
    int *hop;
{
    if (*hop > -1000 && *hop < 10000) return (" ms");
    *hop /= 1000;
    if (*hop > -1000 && *hop < 10000) return (" s ");
    return ("s ");
}

/*
 * Calculate and print one line of packet loss and packet rate statistics.
 * Checks for count of all ones from mrouted 2.3 that doesn't have counters.
 */
#define NEITHER 0
#define INS     1
#define OUTS    2
#define BOTH    3
void
stat_line(r, s, have_next, rst)
    struct tr_resp *r, *s;
    int have_next;
    int *rst;
{
    int timediff = (ntohl(s->tr_qarr) - ntohl(r->tr_qarr)) >> 16;
    int v_lost, v_pct;
    int g_lost, g_pct;
    int v_out = ntohl(s->tr_vifout) - ntohl(r->tr_vifout);
    int g_out = ntohl(s->tr_pktcnt) - ntohl(r->tr_pktcnt);
    int v_pps, g_pps;
    char v_str[8], g_str[8];
    int vhave = NEITHER;
    int ghave = NEITHER;
    int gmissing = NEITHER;
    char whochar;
    int badtime = 0;

    if (timediff == 0) {
	badtime = 1;
	/* Might be 32 bits of int seconds instead of 16int+16frac */
	timediff = ntohl(s->tr_qarr) - ntohl(r->tr_qarr);
	if (timediff == 0 || abs(timediff - statint) > statint)
	    timediff = 1;
    }
    v_pps = v_out / timediff;
    g_pps = g_out / timediff;

#define STATS_MISSING(x)	((x) == 0xFFFFFFFF)

    if (!STATS_MISSING(s->tr_vifout) && !STATS_MISSING(r->tr_vifout))
	    vhave |= OUTS;
    if (STATS_MISSING(s->tr_pktcnt) || STATS_MISSING(r->tr_pktcnt))
	    gmissing |= OUTS;
    if (!(*rst & BUG_NOPRINT))
	    ghave |= OUTS;

    if (have_next) {
	--r,  --s,  --rst;
	if (!STATS_MISSING(s->tr_vifin) && !STATS_MISSING(r->tr_vifin))
	    vhave |= INS;
	if (STATS_MISSING(s->tr_pktcnt) || STATS_MISSING(r->tr_pktcnt))
	    gmissing |= INS;
	if (!(*rst & BUG_NOPRINT))
	    ghave |= INS;
    }

    /*
     * Stats can be missing for any number of reasons:
     * - The hop may not be capable of collecting stats
     * - Traffic may be getting dropped at the previous hop
     *   and so this hop may not have any state
     *
     * We need a stronger heuristic to tell between these
     * two cases; in case 1 we don't want to print the stats
     * and in case 2 we want to print 100% loss.  We used to
     * err on the side of not printing, which is less useful
     * than printing 100% loss and dealing with it.
     */
#if 0
    /*
     * If both hops report as missing, then it's likely that there's just
     * no traffic flowing.
     *
     * If just one hop is missing, then we really don't have it.
     */
    if (gmissing != BOTH)
	ghave &= ~gmissing;
#endif

    whochar = have_next ? '^' : ' ';
    switch (vhave) {
      case BOTH:
	v_lost = v_out - (ntohl(s->tr_vifin) - ntohl(r->tr_vifin));
	if (v_out) v_pct = v_lost * 100 / v_out;
	else v_pct = 0;
	if (-20 < v_pct && v_pct < 101 && v_out > 10)
	  sprintf(v_str, "%3d%%", v_pct);
	else if (v_pct < -900 && v_out > 10)
	  sprintf(v_str, "%3dx", (int)(-v_pct / 100. + 1.));
	else if (v_pct <= -20 && v_out > 10)
	  sprintf(v_str, "%1.1fx", -v_pct / 100. + 1.);
	else
	  memcpy(v_str, " -- ", 5);

	if (tunstats)
	    printf("%6d/%-5d=%s", v_lost, v_out, v_str);
	else
	    printf("   ");
	printf("%4d pps", v_pps);
	if (v_pps && badtime)
	    printf("?");

	break;

      case INS:
	v_out = ntohl(s->tr_vifin) - ntohl(r->tr_vifin);
	v_pps = v_out / timediff;
	whochar = 'v';
	/* FALLTHROUGH */

      case OUTS:
	if (tunstats)
	    printf("      %c%-5d     ", whochar, v_out);
	else
	    printf("  %c", whochar);
	printf("%4d pps", v_pps);
	if (v_pps && badtime)
	    printf("?");

	break;

      case NEITHER:
	if (ghave != NEITHER)
	    if (tunstats)
		printf("                         ");
	    else
		printf("           ");

	break;
    }

    whochar = have_next ? '^' : ' ';
    switch (ghave) {
      case BOTH:
	g_lost = g_out - (ntohl(s->tr_pktcnt) - ntohl(r->tr_pktcnt));
	if (g_out) g_pct = g_lost * 100 / g_out;
	else g_pct = 0;
	if (-20 < g_pct && g_pct < 101 && g_out > 10)
	  sprintf(g_str, "%3d%%", g_pct);
	else if (g_pct < -900 && g_out > 10)
	  sprintf(g_str, "%3dx", (int)(-g_pct / 100. + 1.));
	else if (g_pct <= -20 && g_out > 10)
	  sprintf(g_str, "%1.1fx", -g_pct / 100. + 1.);
	else
	  memcpy(g_str, " -- ", 5);

	printf("%s%6d/%-5d=%s%4d pps",
	       tunstats ? "" : "   ", g_lost, g_out, g_str, g_pps);
	if (g_pps && badtime)
	    printf("?");
	printf("\n");
	break;

#if 0
      case INS:
	g_out = ntohl(s->tr_pktcnt) - ntohl(r->tr_pktcnt);
	g_pps = g_out / timediff;
	whochar = 'v';
	/* FALLTHROUGH */
#endif

      case OUTS:
	printf("%s     ?/%-5d     %4d pps",
	       tunstats ? "" : "   ", g_out, g_pps);
	if (badtime)
	    printf("?");
	printf("\n");
	break;

      case INS:
      case NEITHER:
	printf("\n");
	break;
    }


    if (debug > 2) {
	printf("\t\t\t\tv_in: %ld ", (long)ntohl(s->tr_vifin));
	printf("v_out: %ld ", (long)ntohl(s->tr_vifout));
	printf("pkts: %ld\n", (long)ntohl(s->tr_pktcnt));
	printf("\t\t\t\tv_in: %ld ", (long)ntohl(r->tr_vifin));
	printf("v_out: %ld ", (long)ntohl(r->tr_vifout));
	printf("pkts: %ld\n", (long)ntohl(r->tr_pktcnt));
	printf("\t\t\t\tv_in: %ld ",
	    (long)(ntohl(s->tr_vifin) - ntohl(r->tr_vifin)));
	printf("v_out: %ld ",
	    (long)(ntohl(s->tr_vifout) - ntohl(r->tr_vifout)));
	printf("pkts: %ld ", (long)(ntohl(s->tr_pktcnt) - ntohl(r->tr_pktcnt)));
	printf("time: %d\n", timediff);
	printf("\t\t\t\treset: %x hoptime: %lx\n", *rst, ntohl(s->tr_qarr));
    }
}

/*
 * A fixup to check if any pktcnt has been reset, and to fix the
 * byteorder bugs in mrouted 3.6 on little-endian machines.
 *
 * XXX Since periodic traffic sources are likely to have their
 *     pktcnt periodically reset, should we save old values when
 *     the reset occurs to keep slightly better statistics over
 *     the long term?  (e.g. SAP)
 */
void
fixup_stats(base, prev, new, bugs)
    struct resp_buf *base, *prev, *new;
    int *bugs;
{
    int rno = base->len;
    struct tr_resp *b = base->resps + rno;
    struct tr_resp *p = prev->resps + rno;
    struct tr_resp *n = new->resps + rno;
    int *r = bugs + rno;
    int res;
    int cleanup = 0;

    /* Check for byte-swappers.  Only check on the first trace,
     * since long-running traces can wrap around and falsely trigger. */
    while (--rno >= 0) {
#ifdef TEST_ONLY
	u_int32 nvifout = ntohl(n->tr_vifout);
	u_int32 pvifout = ntohl(p->tr_vifout);
#endif
	--n; --p; --b;
#ifdef TEST_ONLY	/*XXX this is still buggy, so disable it for release */
	if ((*r & BUG_SWAP) ||
	    ((base == prev) &&
	     (nvifout - pvifout) > (byteswap(nvifout) - byteswap(pvifout)))) {
	    if (1 || debug > 2) {
		printf("ip %s swaps; b %08x p %08x n %08x\n",
			inet_fmt(n->tr_inaddr, s1),
			ntohl(b->tr_vifout), pvifout, nvifout);
	    }
	    /* This host sends byteswapped reports; swap 'em */
	    if (!(*r & BUG_SWAP)) {
		*r |= BUG_SWAP;
		b->tr_qarr = byteswap(b->tr_qarr);
		b->tr_vifin = byteswap(b->tr_vifin);
		b->tr_vifout = byteswap(b->tr_vifout);
		b->tr_pktcnt = byteswap(b->tr_pktcnt);
	    }

	    n->tr_qarr = byteswap(n->tr_qarr);
	    n->tr_vifin = byteswap(n->tr_vifin);
	    n->tr_vifout = byteswap(n->tr_vifout);
	    n->tr_pktcnt = byteswap(n->tr_pktcnt);
	}
#endif
	/*
	 * A missing parenthesis in mrouted 3.5-3.8's prune.c
	 * causes extremely bogus time diff's.
	 * One half of the time calculation was
	 * inside an htonl() and one half wasn't.  Therefore, on
	 * a little-endian machine, both halves of the calculation
	 * would get added together in the little end.  Thus, the
	 * low-order 2 bytes are either 0000 (no overflow) or
	 * 0100 (overflow from the addition).
	 *
	 * Odds are against these particular bit patterns
	 * happening in both prev and new for actual time values.
	 */
	if ((*r & BUG_BOGUSTIME) || (((ntohl(n->tr_qarr) & 0xfeff) == 0x0000) &&
	    ((ntohl(p->tr_qarr) & 0xfeff) == 0x0000))) {
	    *r |= BUG_BOGUSTIME;
	    n->tr_qarr = new->rtime;
	    p->tr_qarr = prev->rtime;
	    b->tr_qarr = base->rtime;
	}
    }

    rno = base->len;
    b = base->resps + rno;
    p = prev->resps + rno;
    n = new->resps + rno;
    r = bugs + rno;

    while (--rno >= 0) {
	--n; --p; --b; --r;
	/*
	 * This hop has reset if:
	 * - There were statistics in the base AND previous pass, AND
	 *   - There are less packets this time than the first time and
	 *     we didn't reset last time, OR
	 *   - There are less packets this time than last time, OR
	 *   - There are no statistics on this pass.
	 *
	 * The "and we didn't reset last time" is necessary in the
	 * first branch of the OR because if the base is large and
	 * we reset last time but the constant-resetter-avoidance
	 * code kicked in so we delayed the copy of prev to base,
	 * new could still be below base so we trigger the
	 * constant-resetter code even though it was really only
	 * a single reset.
	 */
	res = ((b->tr_pktcnt != 0xFFFFFFFF) && (p->tr_pktcnt != 0xFFFFFFFF) &&
	       ((!(*r & BUG_RESET) && ntohl(n->tr_pktcnt) < ntohl(b->tr_pktcnt)) ||
	        (ntohl(n->tr_pktcnt) < ntohl(p->tr_pktcnt)) ||
		(n->tr_pktcnt == 0xFFFFFFFF)));
	if (debug > 2) {
    	    printf("\t\tip=%s, r=%d, res=%d\n", inet_fmt(b->tr_inaddr, s1), *r, res);
	    if (res)
		printf("\t\tbase=%ld, prev=%ld, new=%ld\n", ntohl(b->tr_pktcnt),
			    ntohl(p->tr_pktcnt), ntohl(n->tr_pktcnt));
	}
	if (*r & BUG_RESET) {
	    if (res || (*r & BUG_RESET2X)) {
		/*
		 * This router appears to be a 3.4 with that nasty ol'
		 * neighbor version bug, which causes it to constantly
		 * reset.  Just nuke the statistics for this node, and
		 * don't even bother giving it the benefit of the
		 * doubt from now on.
		 */
		p->tr_pktcnt = b->tr_pktcnt = n->tr_pktcnt;
		*r |= BUG_RESET2X;
	    } else {
		/*
		 * This is simply the situation that the original
		 * fixup_stats was meant to deal with -- that a
		 * 3.3 or 3.4 router deleted a cache entry while
		 * traffic was still active.
		 */
		*r &= ~BUG_RESET;
		cleanup = 1;
	    }
	} else
	    if (res)
		*r |= BUG_RESET;
    }

    if (cleanup == 0) return;

    /*
     * If some hop reset its counters and didn't continue to
     * reset, then we pretend that the previous
     * trace was the first one.
     */
    rno = base->len;
    b = base->resps + rno;
    p = prev->resps + rno;

    while (--rno >= 0) (--b)->tr_pktcnt = (--p)->tr_pktcnt;
    base->qtime = prev->qtime;
    base->rtime = prev->rtime;
}

/*
 * Check per-source losses along path and compare with threshold.
 */
int
check_thresh(thresh, base, prev, new)
    int thresh;
    struct resp_buf *base, *prev, *new;
{
    int rno = base->len - 1;
    struct tr_resp *b = base->resps + rno;
    struct tr_resp *p = prev->resps + rno;
    struct tr_resp *n = new->resps + rno;
    int g_out, g_lost;

    while (TRUE) {
	if ((n->tr_inaddr != b->tr_inaddr) ||
	    (n->tr_outaddr != b->tr_outaddr) ||
	    (n->tr_rmtaddr != b->tr_rmtaddr))
	  return 1;		/* Route changed */

	if (rno-- < 1) break;
    	g_out = ntohl(n->tr_pktcnt) - ntohl(p->tr_pktcnt);
	b--; n--; p--;
	g_lost = g_out - (ntohl(n->tr_pktcnt) - ntohl(p->tr_pktcnt));
	if (g_out && ((g_lost * 100 + (g_out >> 1))/ g_out) > thresh) {
	    return TRUE;
	}
    }
    return FALSE;
}

/*
 * Print responses with statistics for forward path (from src to dst)
 */
int
print_stats(base, prev, new, bugs, names)
    struct resp_buf *base, *prev, *new;
    int *bugs;
    char **names;
{
    int rtt, hop;
    char *ms;
    u_int32 smask;
    int rno = base->len - 1;
    struct tr_resp *b = base->resps + rno;
    struct tr_resp *p = prev->resps + rno;
    struct tr_resp *n = new->resps + rno;
    int *r = bugs + rno;
    u_long resptime = new->rtime;
    u_long qarrtime = ntohl(n->tr_qarr);
    u_int ttl = MaX(1, n->tr_fttl) + 1;
    int first = (base == prev);

    VAL_TO_MASK(smask, b->tr_smask);
    printf("  Source        Response Dest    ");
    if (tunstats)
	printf("Packet Statistics For     Only For Traffic\n");
    else
	printf("Overall     Packet Statistics For Traffic From\n");
    (void)inet_fmt(base->qhdr.tr_src, s1);
    printf("%-15s %-15s  ",
	   ((b->tr_inaddr & smask) == (base->qhdr.tr_src & smask)) ?
		s1 : "   * * *       ",
	   inet_fmt(base->qhdr.tr_raddr, s2));
    (void)inet_fmt(base->igmp.igmp_group.s_addr, s2);
    if (tunstats)
	printf("All Multicast Traffic     From %s\n", s1);
    else
	printf("Packet      %s To %s\n", s1, s2);
    rtt = t_diff(resptime, new->qtime);
    ms = scale(&rtt);
    printf("     %c       __/  rtt%5d%s    ",
	   (first && !verbose) ? 'v' : '|', rtt, ms);
    if (tunstats)
	printf("Lost/Sent = Pct  Rate       To %s\n", s2);
    else
	printf(" Rate       Lost/Sent = Pct  Rate\n");
    if (!first || verbose) {
	hop = t_diff(resptime, qarrtime);
	ms = scale(&hop);
	printf("     v      /     hop%5d%s    ", hop, ms);
	if (tunstats)
	    printf("---------------------     --------------------\n");
	else
	    printf("-------     ---------------------\n");
    }
    if ((b->tr_inaddr & smask) != (base->qhdr.tr_src & smask) &&
	    b->tr_rmtaddr != 0) {
	printf("%-15s %-14s is the previous hop\n", inet_fmt(b->tr_rmtaddr, s1),
		inet_name(b->tr_rmtaddr));
	printf("     v     ^\n");
    }
    if (debug > 2) {
	printf("\t\t\t\tv_in: %ld ", (long)ntohl(n->tr_vifin));
	printf("v_out: %ld ", (long)ntohl(n->tr_vifout));
	printf("pkts: %ld\n", (long)ntohl(n->tr_pktcnt));
	printf("\t\t\t\tv_in: %ld ", (long)ntohl(b->tr_vifin));
	printf("v_out: %ld ", (long)ntohl(b->tr_vifout));
	printf("pkts: %ld\n", (long)ntohl(b->tr_pktcnt));
	printf("\t\t\t\tv_in: %ld ",
	    (long)(ntohl(n->tr_vifin) - ntohl(b->tr_vifin)));
	printf("v_out: %ld ",
	    (long)(ntohl(n->tr_vifout) - ntohl(b->tr_vifout)));
	printf("pkts: %ld\n",
	    (long)(ntohl(n->tr_pktcnt) - ntohl(b->tr_pktcnt)));
	printf("\t\t\t\treset: %x hoptime: %lx\n", *r, (long)ntohl(n->tr_qarr));
    }

    while (TRUE) {
	if ((n->tr_inaddr != b->tr_inaddr) ||
	    (n->tr_outaddr != b->tr_outaddr) ||
	    (n->tr_rmtaddr != b->tr_rmtaddr))
	  return 1;		/* Route changed */

	if ((n->tr_inaddr != n->tr_outaddr) && n->tr_inaddr)
	  printf("%-15s\n", inet_fmt(n->tr_inaddr, s1));
	printf("%-15s %-14s %s%s\n", inet_fmt(n->tr_outaddr, s1), names[rno],
		 flag_type(n->tr_rflags),
		 (*r & BUG_NOPRINT) ? " [reset counters]" : "");

	if (rno-- < 1) break;

	printf("     %c     ^      ttl%5d   ", (first && !verbose) ? 'v' : '|',
								ttl);
	stat_line(p, n, TRUE, r);
	if (!first || verbose) {
	    resptime = qarrtime;
	    qarrtime = ntohl((n-1)->tr_qarr);
	    hop = t_diff(resptime, qarrtime);
	    ms = scale(&hop);
	    printf("     v     |      hop%5d%s", hop, ms);
	    if (first)
		printf("\n");
	    else
		stat_line(b, n, TRUE, r);
	}

	--b, --p, --n, --r;
	ttl = MaX(ttl, MaX(1, n->tr_fttl) + base->len - rno);
    }
	   
    printf("     %c      \\__   ttl%5d   ", (first && !verbose) ? 'v' : '|',
							ttl);
    stat_line(p, n, FALSE, r);
    if (!first || verbose) {
	hop = t_diff(qarrtime, new->qtime);
	ms = scale(&hop);
	printf("     v         \\  hop%5d%s", hop, ms);
	if (first)
	    printf("\n");
	else
	    stat_line(b, n, FALSE, r);
    }
    printf("%-15s %s\n", inet_fmt(base->qhdr.tr_dst, s1),
			!passive ? inet_fmt(lcl_addr, s2) : "   * * *       ");
    printf("  Receiver      Query Source\n\n");
    return 0;
}

/*
 * Determine whether or not the path has changed.
 */
int
path_changed(base, new)
    struct resp_buf *base, *new;
{
    int rno = base->len - 1;
    struct tr_resp *b = base->resps + rno;
    struct tr_resp *n = new->resps + rno;

    while (rno-- >= 0) {
	if ((n->tr_inaddr != b->tr_inaddr) ||
	    (n->tr_outaddr != b->tr_outaddr) ||
	    (n->tr_rmtaddr != b->tr_rmtaddr))
	  return 1;		/* Route changed */
	if ((b->tr_rflags == TR_NO_RTE) &&
	    (n->tr_rflags != TR_NO_RTE))
	  return 1;		/* Route got longer? */
	--n;
	--b;
    }
    return 0;
}


/***************************************************************************
 *	main
 ***************************************************************************/

int
main(argc, argv)
int argc;
char *argv[];
{
    int udp;
    struct sockaddr_in addr;
    int addrlen = sizeof(addr);
    int recvlen;
    struct timeval tv;
    struct resp_buf *prev, *new;
    struct tr_resp *r;
    u_int32 smask;
    int rno;
    int hops, nexthop, tries;
    u_int32 lastout = 0;
    int numstats = 1;
    int waittime;
    int seed;
    int hopbyhop;
    int i;
    int printed = 1;

    if (geteuid() != 0)
	errx(1, "must be root");

    /*
     * We might get spawned by vat with the audio device open.
     * Close everything but stdin, stdout, stderr.
     */
    for (i = 3; i < 255; i++)
	close(i);

    init_igmp();
    setuid(getuid());

    argv++, argc--;
    if (argc == 0) usage();

    while (argc > 0 && *argv[0] == '-') {
	char *p = *argv++;  argc--;
	p++;
	do {
	    char c = *p++;
	    char *arg = (char *) 0;
	    if (isdigit(*p)) {
		arg = p;
		p = "";
	    } else if (argc > 0) arg = argv[0];
	    switch (c) {
	      case 'd':			/* Unlisted debug print option */
		if (arg && isdigit(*arg)) {
		    debug = atoi(arg);
		    if (debug < 0) debug = 0;
		    if (debug > 3) debug = 3;
		    if (arg == argv[0]) argv++, argc--;
		    break;
		} else
		    usage();
	      case 'M':			/* Use multicast for reponse */
		multicast = TRUE;
		break;
	      case 'U':			/* Use unicast for response */
		unicast = TRUE;
		break;
	      case 'L':			/* Trace w/ loss threshold */
		if (arg && isdigit(*arg)) {
		    lossthresh = atoi(arg);
		    if (lossthresh < 0)
			lossthresh = 0;
		    numstats = 3153600;
		    if (arg == argv[0]) argv++, argc--;
		    break;
		} else
		    usage();
		break;
	      case 'O':			/* Don't use IP options */
		sendopts = FALSE;
		break;
	      case 'P':			/* Just watch the path */
		printstats = FALSE;
		numstats = 3153600;
		break;
	      case 'Q':			/* (undoc.) always use this QID */
		if (arg && isdigit(*arg)) {
		    staticqid = atoi(arg);
		    if (staticqid < 0)
			staticqid = 0;
		    if (arg == argv[0]) argv++, argc--;
		    break;
		} else
		    usage();
		break;
	      case 'T':			/* Print confusing tunnel stats */
		tunstats = TRUE;
		break;
	      case 'W':			/* Cisco's "weak" mtrace */
		weak = TRUE;
		break;
	      case 'V':			/* Print version and exit */
		/*
		 * FreeBSD wants to have its own Id string, so
		 * determination of the version number has to change.
		 * XXX Note that this must be changed by hand on importing
		 * XXX new versions!
		 */
		{
		    char *r = strdup(rcsid);
		    char *s = strchr(r, ',');

		    while (s && *(s+1) != 'v')
			s = strchr(s + 1, ',');

		    if (s) {
			char *q;

			s += 3;		/* , v sp */
			q = strchr(s, ' ');
			if (q)
				*q = '\0';
			fprintf(stderr, "mtrace version 5.2/%s\n", s);
		    } else {
			fprintf(stderr, "mtrace could not determine version number!?\n");
		    }
		    exit(1);
		}
		break;
	      case 'l':			/* Loop updating stats indefinitely */
		numstats = 3153600;
		break;
	      case 'n':			/* Don't reverse map host addresses */
		numeric = TRUE;
		break;
	      case 'p':			/* Passive listen for traces */
		passive = TRUE;
		break;
	      case 'v':			/* Verbosity */
		verbose = TRUE;
		break;
	      case 's':			/* Short form, don't wait for stats */
		numstats = 0;
		break;
	      case 'w':			/* Time to wait for packet arrival */
		if (arg && isdigit(*arg)) {
		    timeout = atoi(arg);
		    if (timeout < 1) timeout = 1;
		    if (arg == argv[0]) argv++, argc--;
		    break;
		} else
		    usage();
	      case 'f':			/* first hop */
		if (arg && isdigit(*arg)) {
		    qno = atoi(arg);
		    if (qno > MAXHOPS) qno = MAXHOPS;
		    else if (qno < 1) qno = 0;
		    if (arg == argv[0]) argv++, argc--;
		    fflag++;
		    break;
		} else
		    usage();
	      case 'm':			/* Max number of hops to trace */
		if (arg && isdigit(*arg)) {
		    qno = atoi(arg);
		    if (qno > MAXHOPS) qno = MAXHOPS;
		    else if (qno < 1) qno = 0;
		    if (arg == argv[0]) argv++, argc--;
		    break;
		} else
		    usage();
	      case 'q':			/* Number of query retries */
		if (arg && isdigit(*arg)) {
		    nqueries = atoi(arg);
		    if (nqueries < 1) nqueries = 1;
		    if (arg == argv[0]) argv++, argc--;
		    break;
		} else
		    usage();
	      case 'g':			/* Last-hop gateway (dest of query) */
		if (arg && (gwy = host_addr(arg))) {
		    if (arg == argv[0]) argv++, argc--;
		    break;
		} else
		    usage();
	      case 't':			/* TTL for query packet */
		if (arg && isdigit(*arg)) {
		    qttl = atoi(arg);
		    if (qttl < 1) qttl = 1;
		    rttl = qttl;
		    if (arg == argv[0]) argv++, argc--;
		    break;
		} else
		    usage();
	      case 'e':			/* Extra hops past non-responder */
		if (arg && isdigit(*arg)) {
		    extrahops = atoi(arg);
		    if (extrahops < 0) extrahops = 0;
		    if (arg == argv[0]) argv++, argc--;
		    break;
		} else
		    usage();
	      case 'r':			/* Dest for response packet */
		if (arg && (raddr = host_addr(arg))) {
		    if (arg == argv[0]) argv++, argc--;
		    break;
		} else
		    usage();
	      case 'i':			/* Local interface address */
		if (arg && (lcl_addr = host_addr(arg))) {
		    if (arg == argv[0]) argv++, argc--;
		    break;
		} else
		    usage();
	      case 'S':			/* Stat accumulation interval */
		if (arg && isdigit(*arg)) {
		    statint = atoi(arg);
		    if (statint < 1) statint = 1;
		    if (arg == argv[0]) argv++, argc--;
		    break;
		} else
		    usage();
	      default:
		usage();
	    }
	} while (*p);
    }

    if (argc > 0 && (qsrc = host_addr(argv[0]))) {          /* Source of path */
	if (IN_MULTICAST(ntohl(qsrc))) {
	    if (gwy) {
		/* Should probably rewrite arg parsing at some point, as
		 * this makes "mtrace -g foo 224.1.2.3 224.2.3.4" valid!... */
		qgrp = qsrc;
		qsrc = 0;
	    } else {
		usage();
	    }
	}
	argv++, argc--;
	if (argc > 0 && (qdst = host_addr(argv[0]))) {      /* Dest of path */
	    argv++, argc--;
	    if (argc > 0 && (qgrp = host_addr(argv[0]))) {  /* Path via group */
		argv++, argc--;
	    }
	    if (IN_MULTICAST(ntohl(qdst))) {
		u_int32 temp = qdst;
		qdst = qgrp;
		qgrp = temp;
		if (IN_MULTICAST(ntohl(qdst))) usage();
	    } else if (qgrp && !IN_MULTICAST(ntohl(qgrp))) usage();
	}
    }

    if (passive) {
	passive_mode();
	return(0);
    }

    if (argc > 0) {
	usage();
    }

#ifdef SUNOS5
    if (sendopts)
	checkforsolarisbug();
#endif

    /*
     * Set useful defaults for as many parameters as possible.
     */

    defgrp = 0;				/* Default to no group */
    query_cast = htonl(0xE0000002);	/* All routers multicast addr */
    resp_cast = htonl(0xE0000120);	/* Mtrace response multicast addr */
    if (qgrp == 0) {
	if (!weak)
	    qgrp = defgrp;
	if (printstats && numstats != 0 && !tunstats) {
	    /* Stats are useless without a group */
	    warnx(
	"WARNING: no multicast group specified, so no statistics printed");
	    numstats = 0;
	}
    } else {
	if (weak)
	    warnx(
	"WARNING: group was specified so not performing \"weak\" mtrace");
    }

    /*
     * Get default local address for multicasts to use in setting defaults.
     */
    addr.sin_family = AF_INET;
#if (defined(BSD) && (BSD >= 199103))
    addr.sin_len = sizeof(addr);
#endif
    addr.sin_addr.s_addr = qgrp ? qgrp : query_cast;
    addr.sin_port = htons(2000);	/* Any port above 1024 will do */

    /*
     * Note that getsockname() can return 0 on some systems
     * (notably SunOS 5.x, x < 6).  This is taken care of in
     * get_netmask().  If the default multicast interface (set
     * with the route for 224.0.0.0) is not the same as the
     * hostname, mtrace -i [if_addr] will have to be used.
     */
    if (((udp = socket(AF_INET, SOCK_DGRAM, 0)) < 0) ||
	(connect(udp, (struct sockaddr *) &addr, sizeof(addr)) < 0) ||
	getsockname(udp, (struct sockaddr *) &addr, &addrlen) < 0)
	err(-1, "determining local address");

#ifdef SUNOS5
    /*
     * SunOS 5.X prior to SunOS 2.6, getsockname returns 0 for udp socket.
     * This call to sysinfo will return the hostname.
     * If the default multicast interfface (set with the route
     * for 224.0.0.0) is not the same as the hostname,
     * mtrace -i [if_addr] will have to be used.
     */
    if (addr.sin_addr.s_addr == 0) {
	char myhostname[MAXHOSTNAMELEN];
	struct hostent *hp;
	int error;
    
	error = sysinfo(SI_HOSTNAME, myhostname, sizeof(myhostname));
	if (error == -1)
	    err(1, "getting my hostname");

	hp = gethostbyname(myhostname);
	if (hp == NULL || hp->h_addrtype != AF_INET ||
	    hp->h_length != sizeof(addr.sin_addr))
	    err(1, "finding IP address for my hostname");

	memcpy((char *)&addr.sin_addr.s_addr, hp->h_addr, hp->h_length);
    }
#endif

    /*
     * Default destination for path to be queried is the local host.
     * When gateway specified, default destination is that gateway
     *  and default source is local host.
     */
    if (qdst == 0) {
	qdst = lcl_addr ? lcl_addr : addr.sin_addr.s_addr;
	dst_netmask = get_netmask(udp, &qdst);
	if (gwy && (gwy & dst_netmask) != (qdst & dst_netmask) &&
		!IN_MULTICAST(ntohl(gwy)))
	    qdst = gwy;
    }
    if (qsrc == 0 && gwy)
	qsrc = lcl_addr ? lcl_addr : addr.sin_addr.s_addr;
    if (qsrc == 0)
	usage();
    if (!dst_netmask)
	dst_netmask = get_netmask(udp, &qdst);
    close(udp);
    if (lcl_addr == 0) lcl_addr = addr.sin_addr.s_addr;

    /*
     * Initialize the seed for random query identifiers.
     */
    gettimeofday(&tv, 0);
    seed = tv.tv_usec ^ lcl_addr;
#ifdef SYSV    
    srand48(seed);
#endif

    /*
     * Protect against unicast queries to mrouted versions that might crash.
     * Also use the obsolete "can mtrace" neighbor bit to warn about
     * older implementations.
     */
    if (gwy && !IN_MULTICAST(ntohl(gwy)))
      if (send_recv(gwy, IGMP_DVMRP, DVMRP_ASK_NEIGHBORS2, 1, &incr[0], NULL)) {
	int flags = ntohl(incr[0].igmp.igmp_group.s_addr);
	int version = flags & 0xFFFF;
	int info = (flags & 0xFF0000) >> 16;

	if (version == 0x0303 || version == 0x0503) {
	    printf("Don't use -g to address an mrouted 3.%d, it might crash\n",
		   (version >> 8) & 0xFF);
	    exit(0);
	}
	if ((info & 0x08) == 0) {
	    printf("mtrace: ");
	    print_host(gwy);
	    printf(" probably doesn't support mtrace, trying anyway...\n");
	}
      }

    printf("Mtrace from %s to %s via group %s\n",
	   inet_fmt(qsrc, s1), inet_fmt(qdst, s2), inet_fmt(qgrp, s3));

    if ((qdst & dst_netmask) == (qsrc & dst_netmask))
	fprintf(stderr, "mtrace: Source & receiver appear to be directly connected\n");

    /*
     * If the response is to be a multicast address, make sure we 
     * are listening on that multicast address.
     */
    if (raddr) {
	if (IN_MULTICAST(ntohl(raddr))) k_join(raddr, lcl_addr);
    } else k_join(resp_cast, lcl_addr);

    memset(&base, 0, sizeof(base));

    /*
     * If the destination is on the local net, the last-hop router can
     * be found by multicast to the all-routers multicast group.
     * Otherwise, use the group address that is the subject of the
     * query since by definition the last-hop router will be a member.
     * Set default TTLs for local remote multicasts.
     */
    if (gwy == 0)
      if ((qdst & dst_netmask) == (lcl_addr & dst_netmask)) tdst = query_cast;
      else tdst = qgrp;
    else tdst = gwy;
    if (tdst == 0 && qgrp == 0)
	errx(1, "mtrace: weak mtrace requires -g if destination is not local.\n");

    if (IN_MULTICAST(ntohl(tdst))) {
      k_set_loop(1);	/* If I am running on a router, I need to hear this */
      if (tdst == query_cast) k_set_ttl(qttl ? qttl : 1);
      else k_set_ttl(qttl ? qttl : MULTICAST_TTL1);
    }

    /*
     * Try a query at the requested number of hops or MAXHOPS if unspecified.
     */
    if (qno == 0) {
	hops = MAXHOPS;
	tries = 1;
	printf("Querying full reverse path... ");
	fflush(stdout);
    } else {
	hops = qno;
	tries = nqueries;
	if (fflag)
	    printf("Querying full reverse path, starting at hop %d...", qno);
	else
	    printf("Querying reverse path, maximum %d hops... ", qno);
	fflush(stdout); 
    }
    base.rtime = 0;
    base.len = 0;
    hopbyhop = FALSE;

    recvlen = send_recv(tdst, IGMP_MTRACE, hops, tries, &base, mtrace_callback);

    /*
     * If the initial query was successful, print it.  Otherwise, if
     * the query max hop count is the default of zero, loop starting
     * from one until there is no response for extrahops more hops.  The
     * extra hops allow getting past an mtrace-capable mrouter that can't
     * send multicast packets because all phyints are disabled.
     */
    if (recvlen) {
	printf("\n  0  ");
	print_host(qdst);
	printf("\n");
	print_trace(1, &base, names);
	r = base.resps + base.len - 1;
	if (r->tr_rflags == TR_OLD_ROUTER || r->tr_rflags == TR_NO_SPACE ||
		(qno != 0 && r->tr_rmtaddr != 0 && !fflag)) {
	    printf("%3d  ", -(base.len+1));
	    what_kind(&base, r->tr_rflags == TR_OLD_ROUTER ?
				   "doesn't support mtrace"
				 : "is the next hop");
	} else {
	    if (fflag) {
		nexthop = hops = qno;
		goto continuehop;
	    }
	    VAL_TO_MASK(smask, r->tr_smask);
	    if ((r->tr_inaddr & smask) == (qsrc & smask)) {
		printf("%3d  ", -(base.len+1));
		print_host(qsrc);
		printf("\n");
	    }
	}
    } else if (qno == 0) {
	hopbyhop = TRUE;
	printf("switching to hop-by-hop:\n  0  ");
	print_host(qdst);
	printf("\n");

	for (hops = 1, nexthop = 1; hops <= MAXHOPS; ++hops) {
	    printf("%3d  ", -hops);
	    fflush(stdout);

	    /*
	     * After a successful first hop, try switching to the unicast
	     * address of the last-hop router instead of multicasting the
	     * trace query.  This should be safe for mrouted versions 3.3
	     * and 3.5 because there is a long route timeout with metric
	     * infinity before a route disappears.  Switching to unicast
	     * reduces the amount of multicast traffic and avoids a bug
	     * with duplicate suppression in mrouted 3.5.
	     */
	    if (hops == 2 && gwy == 0 && lastout != 0 &&
		(recvlen = send_recv(lastout, IGMP_MTRACE, hops, 1, &base, mtrace_callback)))
	      tdst = lastout;
	    else recvlen = send_recv(tdst, IGMP_MTRACE, hops, nqueries, &base, mtrace_callback);

	    if (recvlen == 0) {
		/*if (hops == 1) break;*/
		if (hops == nexthop) {
		    if (hops == 1) {
			printf("\n");
		    } else if (what_kind(&base, "didn't respond")) {
			/* the ask_neighbors determined that the
			 * not-responding router is the first-hop. */
			break;
		    }
		    if (extrahops == 0)
			break;
		} else if (hops < nexthop + extrahops) {
		    printf("\n");
		} else {
		    printf("...giving up\n");
		    break;
		}
		continue;
	    }
	    if (base.len == hops &&
		(hops == 1 || (base.resps+nexthop-2)->tr_outaddr == lastout)) {
	    	if (hops == nexthop) {
		    print_trace(-hops, &base, names);
		} else {
		    printf("\nResuming...\n");
		    print_trace(nexthop, &base, names);
		}
	    } else {
		if (base.len < hops) {
		    /*
		     * A shorter trace than requested means a fatal error
		     * occurred along the path, or that the route changed
		     * to a shorter one.
		     *
		     * If the trace is longer than the last one we received,
		     * then we are resuming from a skipped router (but there
		     * is still probably a problem).
		     *
		     * If the trace is shorter than the last one we
		     * received, then the route must have changed (and
		     * there is still probably a problem).
		     */
		    if (nexthop <= base.len) {
			printf("\nResuming...\n");
			print_trace(nexthop, &base, names);
		    } else if (nexthop > base.len + 1) {
			hops = base.len;
			printf("\nRoute must have changed...\n");
			print_trace(1, &base, names);
		    }
		} else {
		    /*
		     * The last hop address is not the same as it was.
		     * If we didn't know the last hop then we just
		     * got the first response from a hop-by-hop trace;
		     * if we did know the last hop then
		     * the route probably changed underneath us.
		     */
		    hops = base.len;
		    if (lastout != 0)
			printf("\nRoute must have changed...\n");
		    else
			printf("\nResuming...\n");
		    print_trace(1, &base, names);
		}
	    }
continuehop:
	    r = base.resps + base.len - 1;
	    lastout = r->tr_outaddr;

	    if (base.len < hops ||
		r->tr_rmtaddr == 0 ||
		(r->tr_rflags & 0x80)) {
		VAL_TO_MASK(smask, r->tr_smask);
		if (r->tr_rmtaddr) {
		    if (hops != nexthop) {
			printf("\n%3d  ", -(base.len+1));
		    }
		    what_kind(&base, r->tr_rflags == TR_OLD_ROUTER ?
				"doesn't support mtrace" :
				"would be the next hop");
		    /* XXX could do segmented trace if TR_NO_SPACE */
		} else if (r->tr_rflags == TR_NO_ERR &&
			   (r->tr_inaddr & smask) == (qsrc & smask)) {
		    printf("%3d  ", -(hops + 1));
		    print_host(qsrc);
		    printf("\n");
		}
		break;
	    }

	    nexthop = hops + 1;
	}
    }

    if (base.rtime == 0) {
	printf("Timed out receiving responses\n");
	if (IN_MULTICAST(ntohl(tdst)))
	  if (tdst == query_cast)
	    printf("Perhaps no local router has a route for source %s\n",
		   inet_fmt(qsrc, s1));
	  else
	    printf("Perhaps receiver %s is not a member of group %s,\n\
or no router local to it has a route for source %s,\n\
or multicast at ttl %d doesn't reach its last-hop router for that source\n",
		   inet_fmt(qdst, s2), inet_fmt(qgrp, s3), inet_fmt(qsrc, s1),
		   qttl ? qttl : MULTICAST_TTL1);
	exit(1);
    }

    printf("Round trip time %d ms; ", t_diff(base.rtime, base.qtime));
    {
	struct tr_resp *n = base.resps + base.len - 1;
	u_int ttl = n->tr_fttl + 1;

	rno = base.len - 1;
	while (--rno > 0) {
	    --n;
	    ttl = MaX(ttl, MaX(1, n->tr_fttl) + base.len - rno);
	}
	printf("total ttl of %d required.\n\n",ttl);
    }

    /*
     * Use the saved response which was the longest one received,
     * and make additional probes after delay to measure loss.
     */
    raddr = base.qhdr.tr_raddr;
    rttl = TR_GETTTL(base.qhdr.tr_rttlqid);
    gettimeofday(&tv, 0);
    waittime = statint - (((tv.tv_sec + JAN_1970) & 0xFFFF) - (base.qtime >> 16));
    prev = &base;
    new = &incr[numstats&1];

    /*
     * Zero out bug-avoidance counters
     */
    memset(bugs, 0, sizeof(bugs));

    if (!printstats)
	printf("Monitoring path..");

    while (numstats--) {
	if (waittime < 1) printf("\n");
	else {
	    if (printstats && (lossthresh == 0 || printed)) {
		printf("Waiting to accumulate statistics...");
	    } else {
		printf(".");
	    }
	    fflush(stdout);
	    sleep((unsigned)waittime);
	}
	printed = 0;
	rno = hopbyhop ? base.len : qno ? qno : MAXHOPS;
	recvlen = send_recv(tdst, IGMP_MTRACE, rno, nqueries, new, mtrace_callback);

	if (recvlen == 0) {
	    printf("Timed out.\n");
	    if (numstats) {
		numstats++;
		continue;
	    } else
		exit(1);
	}

	if (base.len != new->len || path_changed(&base, new)) {
	    printf("%s", base.len == new->len ? "Route changed" :
					"Trace length doesn't match");
	    if (!printstats)
		printf(" after %d seconds",
		   (int)((new->qtime - base.qtime) >> 16));
	    printf(":\n");
printandcontinue:
	    print_trace(1, new, names);
	    numstats++;
	    bcopy(new, &base, sizeof(base));
	    nexthop = hops = new->len;
	    printf("Continuing with hop-by-hop...\n");
	    goto continuehop;
	}

	if (printstats) {
	    if (new->igmp.igmp_group.s_addr != qgrp ||
		new->qhdr.tr_src != qsrc || new->qhdr.tr_dst != qdst)
		printf("\nWARNING: trace modified en route; statistics may be incorrect\n");
	    fixup_stats(&base, prev, new, bugs);
	    if ((lossthresh == 0) || check_thresh(lossthresh, &base, prev, new)) {
		printf("Results after %d seconds",
		       (int)((new->qtime - base.qtime) >> 16));
		if (lossthresh)
		    printf(" (this trace %d seconds)",
			   (int)((new->qtime - prev->qtime) >> 16));
		if (verbose) {
		    time_t t = time(0);
		    struct tm *qr = localtime(&t);

		    printf(" qid 0x%06x at %2d:%02d:%02d",
				TR_GETQID(base.qhdr.tr_rttlqid),
				qr->tm_hour, qr->tm_min, qr->tm_sec);
		}
		printf(":\n\n");
		printed = 1;
		if (print_stats(&base, prev, new, bugs, names)) {
		    printf("This should have been detected earlier, but ");
		    printf("Route changed:\n");
		    goto printandcontinue;
		}
	    }
	}
	prev = new;
	new = &incr[numstats&1];
	waittime = statint;
    }

    /*
     * If the response was multicast back, leave the group
     */
    if (raddr) {
	if (IN_MULTICAST(ntohl(raddr)))	k_leave(raddr, lcl_addr);
    } else k_leave(resp_cast, lcl_addr);

    return (0);
}

static void
usage()
{
	fprintf(stderr, "%s\n%s\n%s\n",
	"usage: mtrace [-MUOPTWVlnpvs] [-e extra_hops] [-f first_hop] [-i if_addr]",
	"              [-g gateway] [-m max_hops] [-q nqueries] [-r resp_dest]",
	"              [-S statint] [-t ttl] [-w wait] source [receiver] [group]");
	exit(1);
}

void
check_vif_state()
{
    log(LOG_WARNING, errno, "sendto");
}

/*
 * Log errors and other messages to stderr, according to the severity
 * of the message and the current debug level.  For errors of severity
 * LOG_ERR or worse, terminate the program.
 */
#ifdef __STDC__
void
log(int severity, int syserr, char *format, ...)
{
	va_list ap;
	char    fmt[100];

	va_start(ap, format);
#else
/*VARARGS3*/
void 
log(severity, syserr, format, va_alist)
	int     severity, syserr;
	char   *format;
	va_dcl
{
	va_list ap;
	char    fmt[100];

	va_start(ap);
#endif

    switch (debug) {
	case 0: if (severity > LOG_WARNING) return;
	case 1: if (severity > LOG_NOTICE) return;
	case 2: if (severity > LOG_INFO  ) return;
	default:
	    fmt[0] = '\0';
	    if (severity == LOG_WARNING) 
		strcpy(fmt, "warning - ");
	    strncat(fmt, format, sizeof(fmt)-strlen(fmt));
	    fmt[sizeof(fmt)-1]='\0';
	    vfprintf(stderr, fmt, ap);
	    if (syserr == 0)
		fprintf(stderr, "\n");
	    else if (syserr < sys_nerr)
		fprintf(stderr, ": %s\n", sys_errlist[syserr]);
	    else
		fprintf(stderr, ": errno %d\n", syserr);
    }
    if (severity <= LOG_ERR) exit(1);
}
