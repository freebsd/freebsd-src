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
 * documentation in source and binary forms for non-commercial purposes
 * and without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both the copyright notice and
 * this permission notice appear in supporting documentation, and that
 * any documentation, advertising materials, and other materials related
 * to such distribution and use acknowledge that the software was
 * developed by the University of Southern California, Information
 * Sciences Institute.  The name of the University may not be used to
 * endorse or promote products derived from this software without
 * specific prior written permission.
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
 * In particular, parts of the prototype version of this program may
 * have been derived from mrouted programs sources covered by the
 * license in the accompanying file named "LICENSE".
 *
 * $Id: mtrace.c,v 3.5 1995/05/09 01:24:19 fenner Exp $
 */

#include <netdb.h>
#include <sys/time.h>
#include <sys/filio.h>
#include <memory.h>
#include <string.h>
#include "defs.h"
#include <arpa/inet.h>

#define DEFAULT_TIMEOUT	3	/* How long to wait before retrying requests */
#define DEFAULT_RETRIES 3	/* How many times to try */
#define MAXHOPS UNREACHABLE	/* Don't need more hops than max metric */
#define UNICAST_TTL 255		/* TTL for unicast response */
#define MULTICAST_TTL1 64	/* Default TTL for multicast query/response */
#define MULTICAST_TTL_INC 32	/* TTL increment for increase after timeout */
#define MULTICAST_TTL_MAX 192	/* Maximum TTL allowed (protect low-BW links */

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

char names[MAXHOPS][40];

int timeout = DEFAULT_TIMEOUT;
int nqueries = DEFAULT_RETRIES;
int numeric = FALSE;
int debug = 0;
int passive = FALSE;
int multicast = FALSE;

u_int32 defgrp;				/* Default group if not specified */
u_int32 query_cast;			/* All routers multicast addr */
u_int32 resp_cast;			/* Mtrace response multicast addr */

u_int32 lcl_addr = 0;			/* This host address, in NET order */
u_int32 dst_netmask;			/* netmask to go with qdst */

/*
 * Query/response parameters, all initialized to zero and set later
 * to default values or from options.
 */
u_int32 qsrc = 0;
u_int32 qgrp = 0;
u_int32 qdst = 0;
u_char qno  = 0;
u_int32 raddr = 0;
int    qttl = 0;
u_char rttl = 0;
u_int32 gwy = 0;

vifi_t  numvifs;		/* to keep loader happy */
				/* (see kern.c) */
extern void k_join();
extern void k_leave();
extern void k_set_ttl();
extern void exit();
#ifndef SYSV
extern long random();
#endif
extern int errno;


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
    struct hostent *e = gethostbyname(name);
    u_int32  addr;
    int	i, dots = 3;
    char	buf[40];
    char	*ip = name;
    char	*op = buf;

    if (e) memcpy((char *)&addr, e->h_addr_list[0], e->h_length);
    else {
	/*
	 * Undo BSD's favor -- take fewer than 4 octets as net/subnet address.
	 */
	for (i = sizeof(buf) - 7; i > 0; --i) {
	    if (*ip == '.') --dots;
	    if (*ip == '\0') break;
	    *op++ = *ip++;
	}
	for (i = 0; i < dots; ++i) {
	    *op++ = '.';
	    *op++ = '0';
	}
	*op = '\0';
	addr = inet_addr(buf);
	if (addr == -1) {
	    addr = 0;
	    printf("Could not parse %s as host name or address\n", name);
	}
    }
    return addr;
}


char *
proto_type(type)
    u_char type;
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
      default:
	(void) sprintf(buf, "Unknown protocol code %d", type);
	return (buf);
    }
}


char *
flag_type(type)
    u_char type;
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
      case TR_OLD_ROUTER:
	return ("Next router no mtrace");
      case TR_NO_FWD:
	return ("Not forwarding");
      case TR_NO_SPACE:
	return ("No space in packet");
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
    u_int32 dst;
{
    unsigned int i;
    char ifbuf[5000];
    struct ifconf ifc;
    struct ifreq *ifr;
    u_int32 if_addr, if_mask;
    u_int32 retval = 0xFFFFFFFF;
    int found = FALSE;

    ifc.ifc_buf = ifbuf;
    ifc.ifc_len = sizeof(ifbuf);
    if (ioctl(s, SIOCGIFCONF, (char *) &ifc) < 0) {
	perror("ioctl (SIOCGIFCONF)");
	return (retval);
    }
    i = ifc.ifc_len / sizeof(struct ifreq);
    ifr = ifc.ifc_req;
    for (; i > 0; i--, ifr++) {
	if_addr = ((struct sockaddr_in *)&(ifr->ifr_addr))->sin_addr.s_addr;
	if (ioctl(s, SIOCGIFNETMASK, (char *)ifr) >= 0) {
	    if_mask = ((struct sockaddr_in *)&(ifr->ifr_addr))->sin_addr.s_addr;
	    if ((dst & if_mask) == (if_addr & if_mask)) {
		retval = if_mask;
		if (lcl_addr == 0) lcl_addr = if_addr;
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


int
get_ttl(buf)
    struct resp_buf *buf;
{
    register rno;
    register struct tr_resp *b;
    register ttl;

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
 * Fixup for incorrect time format in 3.3 mrouted.
 * This is possible because (JAN_1970 mod 64K) is quite close to 32K,
 * so correct and incorrect times will be far apart.
 */
u_long
fixtime(time)
    u_long time;
{
    if (abs((int)(time-base.qtime)) > 0x3FFFFFFF)
        time = ((time & 0xFFFF0000) + (JAN_1970 << 16)) +
	       ((time & 0xFFFF) << 14) / 15625;
    return (time);
}

int
send_recv(dst, type, code, tries, save)
    u_int32 dst;
    int type, code, tries;
    struct resp_buf *save;
{
    fd_set  fds;
    struct timeval tq, tr, tv;
    struct ip *ip;
    struct igmp *igmp;
    struct tr_query *query, *rquery;
    int ipdatalen, iphdrlen, igmpdatalen;
    u_int32 local, group;
    int datalen;
    int count, recvlen, dummy = 0;
    int len;
    int i, j;

    if (type == IGMP_MTRACE) {
	group = qgrp;
	datalen = sizeof(struct tr_query);
    } else {
	group = htonl(MROUTED_LEVEL);
	datalen = 0;
    }
    if (IN_MULTICAST(ntohl(dst))) local = lcl_addr;
    else local = INADDR_ANY;

    /*
     * If the reply address was not explictly specified, start off
     * with the unicast address of this host.  Then, if there is no
     * response after trying half the tries with unicast, switch to
     * the standard multicast reply address.  If the TTL was also not
     * specified, set a multicast TTL and if needed increase it for the
     * last quarter of the tries.
     */
    query = (struct tr_query *)(send_buf + MIN_IP_HEADER_LEN + IGMP_MINLEN);
    query->tr_raddr = raddr ? raddr : multicast ? resp_cast : lcl_addr;
    query->tr_rttl  = rttl ? rttl :
      IN_MULTICAST(ntohl(query->tr_raddr)) ? get_ttl(save) : UNICAST_TTL;

    for (i = tries ; i > 0; --i) {
	if (tries == nqueries && raddr == 0) {
	    if (i == ((nqueries + 1) >> 1)) {
		query->tr_raddr = resp_cast;
		if (rttl == 0) query->tr_rttl = get_ttl(save);
	    }
	    if (i <= ((nqueries + 3) >> 2) && rttl == 0) {
		query->tr_rttl += MULTICAST_TTL_INC;
		if (query->tr_rttl > MULTICAST_TTL_MAX)
		  query->tr_rttl = MULTICAST_TTL_MAX;
	    }
	}

	/*
	 * Change the qid for each request sent to avoid being confused
	 * by duplicate responses
	 */
#ifdef SYSV    
	query->tr_qid  = ((u_int32)lrand48() >> 8);
#else
	query->tr_qid  = ((u_int32)random() >> 8);
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
		if (errno != EINTR) perror("select");
		continue;
	    } else if (count == 0) {
		printf("* ");
		fflush(stdout);
		break;
	    }

	    gettimeofday(&tr, 0);
	    recvlen = recvfrom(igmp_socket, recv_buf, RECV_BUF_SIZE,
			       0, (struct sockaddr *)0, &dummy);

	    if (recvlen <= 0) {
		if (recvlen && errno != EINTR) perror("recvfrom");
		continue;
	    }

	    if (recvlen < sizeof(struct ip)) {
		fprintf(stderr,
			"packet too short (%u bytes) for IP header", recvlen);
		continue;
	    }
	    ip = (struct ip *) recv_buf;
	    if (ip->ip_p == 0)	/* ignore cache creation requests */
		continue;

	    iphdrlen = ip->ip_hl << 2;
	    ipdatalen = ip->ip_len;
	    if (iphdrlen + ipdatalen != recvlen) {
		fprintf(stderr,
			"packet shorter (%u bytes) than hdr+data len (%u+%u)\n",
			recvlen, iphdrlen, ipdatalen);
		continue;
	    }

	    igmp = (struct igmp *) (recv_buf + iphdrlen);
	    igmpdatalen = ipdatalen - IGMP_MINLEN;
	    if (igmpdatalen < 0) {
		fprintf(stderr,
			"IP data field too short (%u bytes) for IGMP from %s\n",
			ipdatalen, inet_fmt(ip->ip_src.s_addr, s1));
		continue;
	    }

	    switch (igmp->igmp_type) {

	      case IGMP_DVMRP:
		if (igmp->igmp_code != DVMRP_NEIGHBORS2) continue;
		if (ip->ip_src.s_addr != dst) continue;
		len = igmpdatalen;
		break;

	      case IGMP_MTRACE:	    /* For backward compatibility with 3.3 */
	      case IGMP_MTRACE_RESP:
		if (igmpdatalen <= QLEN) continue;
		if ((igmpdatalen - QLEN)%RLEN) {
		    printf("packet with incorrect datalen\n");
		    continue;
		}

		/*
		 * Ignore responses that don't match query.
		 */
		rquery = (struct tr_query *)(igmp + 1);
		if (rquery->tr_qid != query->tr_qid) continue;
		if (rquery->tr_src != qsrc) continue;
		if (rquery->tr_dst != qdst) continue;
		len = (igmpdatalen - QLEN)/RLEN;

		/*
		 * Ignore trace queries passing through this node when
		 * mtrace is run on an mrouter that is in the path
		 * (needed only because IGMP_MTRACE is accepted above
		 * for backward compatibility with multicast release 3.3).
		 */
		if (igmp->igmp_type == IGMP_MTRACE) {
		    struct tr_resp *r = (struct tr_resp *)(rquery+1) + len - 1;
		    u_int32 smask;

		    VAL_TO_MASK(smask, r->tr_smask);
		    if (len < code && (r->tr_inaddr & smask) != (qsrc & smask)
			&& r->tr_rmtaddr != 0 && !(r->tr_rflags & 0x80))
		      continue;
		}

		/*
		 * A match, we'll keep this one.
		 */
		if (len > code) {
		    fprintf(stderr,
			    "Num hops received (%d) exceeds request (%d)\n",
			    len, code);
		}
		rquery->tr_raddr = query->tr_raddr;	/* Insure these are */
		rquery->tr_rttl = query->tr_rttl;	/* as we sent them */
		break;

	      default:
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


char *
print_host(addr)
    u_int32 addr;
{
    char *name;

    if (numeric) {
	printf("%s", inet_fmt(addr, s1));
	return ("");
    }
    name = inet_name(addr);
    printf("%s (%s)", name, inet_fmt(addr, s1));
    return (name);
}

/*
 * Print responses as received (reverse path from dst to src)
 */
void
print_trace(index, buf)
    int index;
    struct resp_buf *buf;
{
    struct tr_resp *r;
    char *name;
    int i;

    i = abs(index);
    r = buf->resps + i - 1;

    for (; i <= buf->len; ++i, ++r) {
	if (index > 0) printf("%3d  ", -i);
	name = print_host(r->tr_outaddr);
	printf("  %s  thresh^ %d  %d ms  %s\n", proto_type(r->tr_rproto),
	       r->tr_fttl, t_diff(fixtime(ntohl(r->tr_qarr)), buf->qtime),
	       flag_type(r->tr_rflags));
	memcpy(names[i-1], name, sizeof(names[0]) - 1);
	names[i-1][sizeof(names[0])-1] = '\0';
    }
}

/*
 * See what kind of router is the next hop
 */
void
what_kind(buf)
    struct resp_buf *buf;
{
    u_int32 smask;
    int recvlen;
    int hops = buf->len;
    struct tr_resp *r = buf->resps + hops - 1;
    u_int32 next = r->tr_rmtaddr;

    recvlen = send_recv(next, IGMP_DVMRP, DVMRP_ASK_NEIGHBORS2, 1, &incr[0]);
    print_host(next);
    if (recvlen) {
	u_int32 version = ntohl(incr[0].igmp.igmp_group.s_addr);
	u_int32 *p = (u_int32 *)incr[0].ndata;
	u_int32 *ep = p + (incr[0].len >> 2);
	printf(" [%s%d.%d] didn't respond\n",
	       (version == 1) ? "proteon/mrouted " :
	       ((version & 0xff) == 2) ? "mrouted " :
	       ((version & 0xff) == 3) ? "mrouted " :
	       ((version & 0xff) == 4) ? "mrouted " :
	       ((version & 0xff) == 10) ? "cisco " : "",
	       version & 0xff, (version >> 8) & 0xff);
	VAL_TO_MASK(smask, r->tr_smask);
	while (p < ep) {
	    register u_int32 laddr = *p++;
	    register int n = ntohl(*p++) & 0xFF;
	    if ((laddr & smask) == (qsrc & smask)) {
		printf("%3d  ", -(hops+2));
		print_host(qsrc);
		printf("\n");
		break;
	    }
	    p += n;
	}
	return;
    }
    printf(" didn't respond\n");
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
stat_line(r, s, have_next)
    struct tr_resp *r, *s;
    int have_next;
{
    register timediff = (fixtime(ntohl(s->tr_qarr)) -
			 fixtime(ntohl(r->tr_qarr))) >> 16;
    register v_lost, v_pct;
    register g_lost, g_pct;
    register v_out = ntohl(s->tr_vifout) - ntohl(r->tr_vifout);
    register g_out = ntohl(s->tr_pktcnt) - ntohl(r->tr_pktcnt);
    register v_pps, g_pps;
    char v_str[8], g_str[8];
    register have = NEITHER;

    if (timediff == 0) timediff = 1;
    v_pps = v_out / timediff;
    g_pps = g_out / timediff;

    if (v_out || s->tr_vifout != 0xFFFFFFFF) have |= OUTS;

    if (have_next) {
	--r,  --s;
	if (s->tr_vifin != 0xFFFFFFFF || r->tr_vifin != 0xFFFFFFFF)
	  have |= INS;
    }

    switch (have) {
      case BOTH:
	v_lost = v_out - (ntohl(s->tr_vifin) - ntohl(r->tr_vifin));
	if (v_out) v_pct = (v_lost * 100 + (v_out >> 1)) / v_out;
	else v_pct = 0;
	if (-100 < v_pct && v_pct < 101 && v_out > 10)
	  sprintf(v_str, "%3d", v_pct);
	else memcpy(v_str, " --", 4);

	g_lost = g_out - (ntohl(s->tr_pktcnt) - ntohl(r->tr_pktcnt));
	if (g_out) g_pct = (g_lost * 100 + (g_out >> 1))/ g_out;
	else g_pct = 0;
	if (-100 < g_pct && g_pct < 101 && g_out > 10)
	  sprintf(g_str, "%3d", g_pct);
	else memcpy(g_str, " --", 4);

	printf("%6d/%-5d=%s%%%4d pps%6d/%-5d=%s%%%4d pps\n",
	       v_lost, v_out, v_str, v_pps, g_lost, g_out, g_str, g_pps);
	if (debug > 2) {
	    printf("\t\t\t\tv_in: %ld ", ntohl(s->tr_vifin));
	    printf("v_out: %ld ", ntohl(s->tr_vifout));
	    printf("pkts: %ld\n", ntohl(s->tr_pktcnt));
	    printf("\t\t\t\tv_in: %ld ", ntohl(r->tr_vifin));
	    printf("v_out: %ld ", ntohl(r->tr_vifout));
	    printf("pkts: %ld\n", ntohl(r->tr_pktcnt));
	    printf("\t\t\t\tv_in: %ld ",ntohl(s->tr_vifin)-ntohl(r->tr_vifin));
	    printf("v_out: %ld ", ntohl(s->tr_vifout) - ntohl(r->tr_vifout));
	    printf("pkts: %ld ", ntohl(s->tr_pktcnt) - ntohl(r->tr_pktcnt));
	    printf("time: %d\n", timediff);
	}
	break;

      case INS:
	v_out = (ntohl(s->tr_vifin) - ntohl(r->tr_vifin));
	g_out = (ntohl(s->tr_pktcnt) - ntohl(r->tr_pktcnt));
	v_pps = v_out / timediff;
	g_pps = g_out / timediff;
	/* Fall through */

      case OUTS:
	printf("       %-5d     %4d pps       %-5d     %4d pps\n",
	       v_out, v_pps, g_out, g_pps);
	break;

      case NEITHER:
	printf("\n");
	break;
    }
}

/*
 * A fixup to check if any pktcnt has been reset.
 */
void
fixup_stats(base, new)
    struct resp_buf *base, *new;
{
    register rno = base->len;
    register struct tr_resp *b = base->resps + rno;
    register struct tr_resp *n = new->resps + rno;

    while (--rno >= 0)
      if (ntohl((--n)->tr_pktcnt) < ntohl((--b)->tr_pktcnt)) break;

    if (rno < 0) return;

    rno = base->len;
    b = base->resps + rno;
    n = new->resps + rno;

    while (--rno >= 0) (--b)->tr_pktcnt = (--n)->tr_pktcnt;
}

/*
 * Print responses with statistics for forward path (from src to dst)
 */
void
print_stats(base, prev, new)
    struct resp_buf *base, *prev, *new;
{
    int rtt, hop;
    register char *ms;
    register u_int32 smask;
    register rno = base->len - 1;
    register struct tr_resp *b = base->resps + rno;
    register struct tr_resp *p = prev->resps + rno;
    register struct tr_resp *n = new->resps + rno;
    register u_long resptime = new->rtime;
    register u_long qarrtime = fixtime(ntohl(n->tr_qarr));
    register ttl = n->tr_fttl;

    VAL_TO_MASK(smask, b->tr_smask);
    printf("  Source        Response Dest");
    printf("    Packet Statistics For     Only For Traffic\n");
    printf("%-15s %-15s  All Multicast Traffic     From %s\n",
	   ((b->tr_inaddr & smask) == (qsrc & smask)) ? s1 : "   * * *       ",
	   inet_fmt(base->qhdr.tr_raddr, s2), inet_fmt(qsrc, s1));
    rtt = t_diff(resptime, new->qtime);
    ms = scale(&rtt);
    printf("     |       __/  rtt%5d%s    Lost/Sent = Pct  Rate       To %s\n",
	   rtt, ms, inet_fmt(qgrp, s2));
    hop = t_diff(resptime, qarrtime);
    ms = scale(&hop);
    printf("     v      /     hop%5d%s", hop, ms);
    printf("    ---------------------     --------------------\n");
    if (debug > 2) {
	printf("\t\t\t\tv_in: %ld ", ntohl(n->tr_vifin));
	printf("v_out: %ld ", ntohl(n->tr_vifout));
	printf("pkts: %ld\n", ntohl(n->tr_pktcnt));
	printf("\t\t\t\tv_in: %ld ", ntohl(b->tr_vifin));
	printf("v_out: %ld ", ntohl(b->tr_vifout));
	printf("pkts: %ld\n", ntohl(b->tr_pktcnt));
	printf("\t\t\t\tv_in: %ld ", ntohl(n->tr_vifin) - ntohl(b->tr_vifin));
	printf("v_out: %ld ", ntohl(n->tr_vifout) - ntohl(b->tr_vifout));
	printf("pkts: %ld\n", ntohl(n->tr_pktcnt) - ntohl(b->tr_pktcnt));
    }

    while (TRUE) {
	if ((n->tr_inaddr != b->tr_inaddr) || (n->tr_inaddr != b->tr_inaddr)) {
	    printf("Route changed, start again.\n");
	    exit(1);
	}
	if ((n->tr_inaddr != n->tr_outaddr))
	  printf("%-15s\n", inet_fmt(n->tr_inaddr, s1));
	printf("%-15s %-14s %s\n", inet_fmt(n->tr_outaddr, s1), names[rno],
		 flag_type(n->tr_rflags));

	if (rno-- < 1) break;

	printf("     |     ^      ttl%5d   ", ttl);
	if (prev == new) printf("\n");
	else stat_line(p, n, TRUE);
	resptime = qarrtime;
	qarrtime = fixtime(ntohl((n-1)->tr_qarr));
	hop = t_diff(resptime, qarrtime);
	ms = scale(&hop);
	printf("     v     |      hop%5d%s", hop, ms);
	stat_line(b, n, TRUE);

	--b, --p, --n;
	if (ttl < n->tr_fttl) ttl = n->tr_fttl;
	else ++ttl;
    }
	   
    printf("     |      \\__   ttl%5d   ", ttl);
    if (prev == new) printf("\n");
    else stat_line(p, n, FALSE);
    hop = t_diff(qarrtime, new->qtime);
    ms = scale(&hop);
    printf("     v         \\  hop%5d%s", hop, ms);
    stat_line(b, n, FALSE);
    printf("%-15s %s\n", inet_fmt(qdst, s1), inet_fmt(lcl_addr, s2));
    printf("  Receiver      Query Source\n\n");
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
    struct tr_query *query;
    struct tr_resp *r;
    u_int32 smask;
    int rno;
    int hops, tries;
    int numstats = 1;
    int waittime;
    int seed;

    if (geteuid() != 0) {
	fprintf(stderr, "mtrace: must be root\n");
	exit(1);
    }

    argv++, argc--;
    if (argc == 0) goto usage;

    while (argc > 0 && *argv[0] == '-') {
	register char *p = *argv++;  argc--;
	p++;
	do {
	    register char c = *p++;
	    register char *arg = (char *) 0;
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
		    goto usage;
	      case 'M':			/* Use multicast for reponse */
		multicast = TRUE;
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
		    goto usage;
	      case 'm':			/* Max number of hops to trace */
		if (arg && isdigit(*arg)) {
		    qno = atoi(arg);
		    if (qno > MAXHOPS) qno = MAXHOPS;
		    else if (qno < 1) qno = 0;
		    if (arg == argv[0]) argv++, argc--;
		    break;
		} else
		    goto usage;
	      case 'q':			/* Number of query retries */
		if (arg && isdigit(*arg)) {
		    nqueries = atoi(arg);
		    if (nqueries < 1) nqueries = 1;
		    if (arg == argv[0]) argv++, argc--;
		    break;
		} else
		    goto usage;
	      case 'g':			/* Last-hop gateway (dest of query) */
		if (arg && (gwy = host_addr(arg))) {
		    if (arg == argv[0]) argv++, argc--;
		    break;
		} else
		    goto usage;
	      case 't':			/* TTL for query packet */
		if (arg && isdigit(*arg)) {
		    qttl = atoi(arg);
		    if (qttl < 1) qttl = 1;
		    rttl = qttl;
		    if (arg == argv[0]) argv++, argc--;
		    break;
		} else
		    goto usage;
	      case 'r':			/* Dest for response packet */
		if (arg && (raddr = host_addr(arg))) {
		    if (arg == argv[0]) argv++, argc--;
		    break;
		} else
		    goto usage;
	      case 'i':			/* Local interface address */
		if (arg && (lcl_addr = host_addr(arg))) {
		    if (arg == argv[0]) argv++, argc--;
		    break;
		} else
		    goto usage;
	      default:
		goto usage;
	    }
	} while (*p);
    }

    if (argc > 0 && (qsrc = host_addr(argv[0]))) {          /* Source of path */
	if (IN_MULTICAST(ntohl(qsrc))) goto usage;
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
		if (IN_MULTICAST(ntohl(qdst))) goto usage;
	    } else if (qgrp && !IN_MULTICAST(ntohl(qgrp))) goto usage;
	}
    }

    if (argc > 0 || qsrc == 0) {
usage:	printf("\
Usage: mtrace [-Mlnps] [-w wait] [-m max_hops] [-q nqueries] [-g gateway]\n\
              [-t ttl] [-r resp_dest] [-i if_addr] source [receiver] [group]\n");
	exit(1);
    }

    init_igmp();

    /*
     * Set useful defaults for as many parameters as possible.
     */

    defgrp = htonl(0xE0020001);		/* MBone Audio (224.2.0.1) */
    query_cast = htonl(0xE0000002);	/* All routers multicast addr */
    resp_cast = htonl(0xE0000120);	/* Mtrace response multicast addr */
    if (qgrp == 0) qgrp = defgrp;

    /*
     * Get default local address for multicasts to use in setting defaults.
     */
    addr.sin_family = AF_INET;
#if (defined(BSD) && (BSD >= 199103))
    addr.sin_len = sizeof(addr);
#endif
    addr.sin_addr.s_addr = qgrp;
    addr.sin_port = htons(2000);	/* Any port above 1024 will do */

    if (((udp = socket(AF_INET, SOCK_DGRAM, 0)) < 0) ||
	(connect(udp, (struct sockaddr *) &addr, sizeof(addr)) < 0) ||
	getsockname(udp, (struct sockaddr *) &addr, &addrlen) < 0) {
	perror("Determining local address");
	exit(-1);
    }

    /*
     * Default destination for path to be queried is the local host.
     */
    if (qdst == 0) qdst = lcl_addr ? lcl_addr : addr.sin_addr.s_addr;

    /*
     * If the destination is on the local net, the last-hop router can
     * be found by multicast to the all-routers multicast group.
     * Otherwise, use the group address that is the subject of the
     * query since by definition the last hop router will be a member.
     * Set default TTLs for local remote multicasts.
     */
    dst_netmask = get_netmask(udp, qdst);
    close(udp);
    if (lcl_addr == 0) lcl_addr = addr.sin_addr.s_addr;
    if (gwy == 0)
      if ((qdst & dst_netmask) == (lcl_addr & dst_netmask)) gwy = query_cast;
      else gwy = qgrp;

    if (IN_MULTICAST(ntohl(gwy))) {
      k_set_loop(1);	/* If I am running on a router, I need to hear this */
      if (gwy == query_cast) k_set_ttl(qttl ? qttl : 1);
      else k_set_ttl(qttl ? qttl : MULTICAST_TTL1);
    } else
      if (send_recv(gwy, IGMP_DVMRP, DVMRP_ASK_NEIGHBORS2, 1, &incr[0]))
	if (ntohl(incr[0].igmp.igmp_group.s_addr) == 0x0303) {
	    printf("Don't use -g to address an mrouted 3.3, it might crash\n");
	    exit(0);
	}

    printf("Mtrace from %s to %s via group %s\n",
	   inet_fmt(qsrc, s1), inet_fmt(qdst, s2), inet_fmt(qgrp, s3));

    if ((qdst & dst_netmask) == (qsrc & dst_netmask)) {
	printf("Source & receiver are directly connected, no path to trace\n");
	exit(0);
    }

    /*
     * Make up the IGMP_MTRACE query packet to send (some parameters
     * are set later), including initializing the seed for random
     * query identifiers.
     */
    query = (struct tr_query *)(send_buf + MIN_IP_HEADER_LEN + IGMP_MINLEN);
    query->tr_src   = qsrc;
    query->tr_dst   = qdst;

    gettimeofday(&tv, 0);
    seed = tv.tv_usec ^ lcl_addr;
#ifdef SYSV    
    srand48(seed);
#else
    srandom(seed);
#endif

    /*
     * If the response is to be a multicast address, make sure we 
     * are listening on that multicast address.
     */
    if (raddr && IN_MULTICAST(ntohl(raddr))) k_join(raddr, lcl_addr);
    else k_join(resp_cast, lcl_addr);

    /*
     * Try a query at the requested number of hops or MAXOPS if unspecified.
     */
    if (qno == 0) {
	hops = MAXHOPS;
	tries = 1;
	printf("Querying full reverse path... ");
	fflush(stdout);
    } else {
	hops = qno;
	tries = nqueries;
	printf("Querying reverse path, maximum %d hops... ", qno);
	fflush(stdout); 
   }
    base.rtime = 0;
    base.len = 0;

    recvlen = send_recv(gwy, IGMP_MTRACE, hops, tries, &base);

    /*
     * If the initial query was successful, print it.  Otherwise, if
     * the query max hop count is the default of zero, loop starting
     * from one until a timeout occurs.
     */
    if (recvlen) {
	printf("\n  0  ");
	print_host(qdst);
	printf("\n");
	print_trace(1, &base);
	r = base.resps + base.len - 1;
	if (r->tr_rflags == TR_OLD_ROUTER) {
	    printf("%3d  ", -(base.len+1));
	    fflush(stdout);
	    what_kind(&base);
	} else {
	    VAL_TO_MASK(smask, r->tr_smask);
	    if ((r->tr_inaddr & smask) == (qsrc & smask)) {
		printf("%3d  ", -(base.len+1));
		print_host(qsrc);
		printf("\n");
	    }
	}
    } else if (qno == 0) {
	printf("switching to hop-by-hop:\n  0  ");
	print_host(qdst);
	printf("\n");

	for (hops = 1; hops <= MAXHOPS; ++hops) {
	    printf("%3d  ", -hops);
	    fflush(stdout);

	    recvlen = send_recv(gwy, IGMP_MTRACE, hops, nqueries, &base);

	    if (recvlen == 0) {
		if (--hops == 0) break;
		what_kind(&base);
		break;
	    }
	    r = base.resps + base.len - 1;
	    if (base.len == hops) print_trace(-hops, &base);
	    else {
		hops = base.len;
		if (r->tr_rflags == TR_OLD_ROUTER) {
		    what_kind(&base);
		    break;
		}
		if (r->tr_rflags == TR_NO_SPACE) {
		    printf("No space left in trace packet for further hops\n");
		    break;	/* XXX could do segmented trace */
		}
		printf("Route must have changed...\n\n");
		print_trace(1, &base);
	    }

	    VAL_TO_MASK(smask, r->tr_smask);
	    if ((r->tr_inaddr & smask) == (qsrc & smask)) {
		printf("%3d  ", -(hops+1));
		print_host(qsrc);
		printf("\n");
		break;
	    }
	    if (r->tr_rmtaddr == 0 || (r->tr_rflags & 0x80)) break;
	}
    }

    if (base.rtime == 0) {
	printf("Timed out receiving responses\n");
	if (IN_MULTICAST(ntohl(gwy)))
	  if (gwy == query_cast)
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

    printf("Round trip time %d ms\n\n", t_diff(base.rtime, base.qtime));

    /*
     * Use the saved response which was the longest one received,
     * and make additional probes after delay to measure loss.
     */
    raddr = base.qhdr.tr_raddr;
    rttl = base.qhdr.tr_rttl;
    gettimeofday(&tv, 0);
    waittime = 10 - (((tv.tv_sec + JAN_1970) & 0xFFFF) - (base.qtime >> 16));
    prev = new = &incr[numstats&1];

    while (numstats--) {
	if (waittime < 1) printf("\n");
	else {
	    printf("Waiting to accumulate statistics... ");
	    fflush(stdout);
	    sleep((unsigned)waittime);
	}
	rno = base.len;
	recvlen = send_recv(gwy, IGMP_MTRACE, rno, nqueries, new);

	if (recvlen == 0) {
	    printf("Timed out.\n");
	    exit(1);
	}

	if (rno != new->len) {
	    printf("Trace length doesn't match.\n");
	    exit(1);
	}

	printf("Results after %d seconds:\n\n",
	       (new->qtime - base.qtime) >> 16);
	fixup_stats(&base, new);
	print_stats(&base, prev, new);
	prev = new;
	new = &incr[numstats&1];
	waittime = 10;
    }

    /*
     * If the response was multicast back, leave the group
     */
    if (raddr && IN_MULTICAST(ntohl(raddr))) k_leave(raddr, lcl_addr);
    else k_leave(resp_cast, lcl_addr);

    return (0);
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
/*VARARGS3*/
void
log(severity, syserr, format, a, b, c, d, e)
    int severity, syserr;
    char *format;
    int a, b, c, d, e;
{
    char fmt[100];

    switch (debug) {
	case 0: if (severity > LOG_WARNING) return;
	case 1: if (severity > LOG_NOTICE) return;
	case 2: if (severity > LOG_INFO  ) return;
	default:
	    fmt[0] = '\0';
	    if (severity == LOG_WARNING) strcat(fmt, "warning - ");
	    strncat(fmt, format, 80);
	    fprintf(stderr, fmt, a, b, c, d, e);
	    if (syserr == 0)
		fprintf(stderr, "\n");
	    else if(syserr < sys_nerr)
		fprintf(stderr, ": %s\n", sys_errlist[syserr]);
	    else
		fprintf(stderr, ": errno %d\n", syserr);
    }
    if (severity <= LOG_ERR) exit(-1);
}

/* dummies */

/*VARARGS*/
void accept_probe() {} /*VARARGS*/
void accept_group_report() {} /*VARARGS*/
void accept_neighbors() {} /*VARARGS*/
void accept_neighbors2() {} /*VARARGS*/
void accept_neighbor_request() {} /*VARARGS*/
void accept_neighbor_request2() {} /*VARARGS*/
void accept_report() {} /*VARARGS*/
void accept_prune() {} /*VARARGS*/
void accept_graft() {} /*VARARGS*/
void accept_g_ack() {} /*VARARGS*/
void add_table_entry() {} /*VARARGS*/
void accept_mtrace() {} /*VARARGS*/
void accept_leave_message() {} /*VARARGS*/
void accept_membership_query() {} /*VARARGS*/
