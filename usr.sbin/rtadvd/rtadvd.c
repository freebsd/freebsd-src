/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/time.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>

#include <arpa/inet.h>

#ifdef IPSEC
#include <netinet6/ipsec.h>
#endif /*IPSEC*/

#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <sysexits.h>
#include <stdlib.h>
#include <syslog.h>
#include "rtadvd.h"
#include "rrenum.h"
#include "advcap.h"
#include "timer.h"
#include "if.h"
#include "config.h"

struct	msghdr rcvmhdr;
static u_char	rcvcmsgbuf[CMSG_SPACE(sizeof(struct in6_pktinfo)) +
			CMSG_SPACE(sizeof(int))];
struct	msghdr sndmhdr;
struct	iovec rcviov[2];
struct	iovec sndiov[2];
struct	sockaddr_in6 from;
struct	sockaddr_in6 sin6_allnodes = {sizeof(sin6_allnodes), AF_INET6};
int	sock, rrsock, rtsock;
int	accept_rr = 0;
int	dflag = 0, sflag = 0;

u_char	*conffile = NULL;

struct	rainfo *ralist = NULL;
struct	nd_optlist {
	struct	nd_optlist *next;
	struct	nd_opt_hdr *opt;
};
union nd_opts {
	struct	nd_opt_hdr *nd_opt_array[7];
	struct {
		struct	nd_opt_hdr *zero;
		struct	nd_opt_hdr *src_lladdr;
		struct	nd_opt_hdr *tgt_lladdr;
		struct	nd_opt_prefix_info *pi;
		struct	nd_opt_rd_hdr *rh;
		struct	nd_opt_mtu *mtu;
		struct	nd_optlist *list;
	} nd_opt_each;
};
#define	nd_opts_src_lladdr	nd_opt_each.src_lladdr
#define	nd_opts_tgt_lladdr	nd_opt_each.tgt_lladdr
#define	nd_opts_pi		nd_opt_each.pi
#define	nd_opts_rh		nd_opt_each.rh
#define	nd_opts_mtu		nd_opt_each.mtu
#define	nd_opts_list		nd_opt_each.list

#define	NDOPT_FLAG_SRCLINKADDR 0x1
#define	NDOPT_FLAG_TGTLINKADDR 0x2
#define	NDOPT_FLAG_PREFIXINFO 0x4
#define	NDOPT_FLAG_RDHDR 0x8
#define	NDOPT_FLAG_MTU 0x10

u_int32_t ndopt_flags[] = {
	0, NDOPT_FLAG_SRCLINKADDR, NDOPT_FLAG_TGTLINKADDR,
	NDOPT_FLAG_PREFIXINFO, NDOPT_FLAG_RDHDR, NDOPT_FLAG_MTU
};

int	main __P((int, char *[]));
static void	sock_open __P((int *, int, char *));
static void	rtsock_open __P((void));
static void	rtadvd_input __P((int));
static void	rs_input __P((int, struct nd_router_solicit *,
			  struct in6_pktinfo *, struct sockaddr_in6 *));
static void	ra_input __P((int, struct nd_router_advert *,
			  struct in6_pktinfo *, struct sockaddr_in6 *));
static void	prefix_check __P((struct nd_opt_prefix_info *, struct rainfo *,
			      struct sockaddr_in6 *));
static int	nd6_options __P((struct nd_opt_hdr *, int,
			    union nd_opts *, u_int32_t));
static void	free_ndopts __P((union nd_opts *));
static struct	rainfo *if_indextorainfo __P((int));
static void	ra_output __P((struct rainfo *));
static void	rtmsg_input __P((void));
struct	prefix *find_prefix __P((struct rainfo *, struct in6_addr *, int));


int
main(argc, argv)
	int argc;
	char *argv[];
{
	fd_set fdset;
	int maxfd = 0;
	struct timeval *timeout;
	int i, ch;
	int fflag = 0;
#ifdef IPSEC
#ifdef IPSEC_POLICY_IPSEC
	char *policy = NULL;
#endif /*IPSEC_POLICY_IPSEC*/
#endif /*IPSEC*/

	openlog(*argv, LOG_NDELAY|LOG_PID, LOG_DAEMON);

	/* get command line options and arguments */
#if defined(IPSEC) && defined(IPSEC_POLICY_IPSEC)
	while ((ch = getopt(argc, argv, "c:dDfP:R:s")) != -1)
#else
	while ((ch = getopt(argc, argv, "c:dDfR:s")) != -1)
#endif
	{
		switch(ch) {
		 case 'c':
			 conffile = optarg;
			 break;
		 case 'd':
			 dflag = 1;
			 break;
		 case 'D':
			 dflag = 2;
			 break;
		 case 'f':
			 fflag = 1;
			 break;
#ifdef IPSEC
#ifdef IPSEC_POLICY_IPSEC
		 case 'P':
			 policy = strdup(optarg);
			 accept_rr = 1;
			 break;
#endif /*IPSEC_POLICY_IPSEC*/
#endif /*IPSEC*/
		 case 's':
			 sflag = 1;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc == 0) {
		fprintf(stderr,
			"usage: rtadvd [-c conffile] [-d|D] [-f] [-s]"
			"interfaces...\n");
		exit(1);
	}

	/* set log level */
	if (dflag == 0)
		(void)setlogmask(LOG_UPTO(LOG_ERR));
	if (dflag == 1)
		(void)setlogmask(LOG_UPTO(LOG_INFO));

	/* timer initialization */
	rtadvd_timer_init();

	/* random value initialization */
	srandom((u_long)time(NULL));

	/* get iflist block from kernel */
	init_iflist();

	while (argc--)
		getconfig(*argv++);

	if (inet_pton(AF_INET6, ALLNODES, &sin6_allnodes.sin6_addr) != 1) {
		fprintf(stderr, "fatal: inet_pton failed\n");
		exit(1);
	}
	sock_open(&sock, 0, NULL);
	if (accept_rr != 0)
		sock_open(&rrsock, 1, policy);

	if (!fflag)
		daemon(1, 0);

	FD_ZERO(&fdset);
	FD_SET(sock, &fdset);
	if (accept_rr)
		FD_SET(rrsock, &fdset);
	maxfd = sock > rrsock ? sock : rrsock;
	if (sflag == 0) {
		rtsock_open();
		FD_SET(rtsock, &fdset);
		if (rtsock > maxfd)
			maxfd = rtsock;
	}

	while (1) {
		struct fd_set select_fd = fdset; /* reinitialize */

		/* timer expiration check and reset the timer */
		timeout = rtadvd_check_timer();

		syslog(LOG_DEBUG,
		       "<%s> set timer to %ld:%ld. waiting for inputs "
		       "or timeout",
		       __FUNCTION__,
		       timeout->tv_sec, timeout->tv_usec);

		if ((i = select(maxfd + 1, &select_fd,
				NULL, NULL, timeout)) < 0){
			syslog(LOG_ERR, "<%s> select: %s",
			       __FUNCTION__, strerror(errno));
			continue;
		}
		if (i == 0)	/* timeout */
			continue;
		if (sflag == 0 && FD_ISSET(rtsock, &select_fd))
			rtmsg_input();
		if (FD_ISSET(sock, &select_fd))
			rtadvd_input(sock);
		if (accept_rr && FD_ISSET(rrsock, &select_fd))
			rtadvd_input(rrsock);
	}
	exit(0);		/* NOTREACHED */
}

static void
rtmsg_input()
{
	int n, type, ifindex, plen;
	size_t len;
	char msg[2048], *next, *lim;
	u_char ifname[16];
	struct prefix *prefix;
	struct rainfo *rai;
	struct in6_addr *addr;
	char addrbuf[INET6_ADDRSTRLEN];

	n = read(rtsock, msg, 2048);
	if (dflag > 1) {
		syslog(LOG_DEBUG,
		       "<%s> received a routing message "
		       "(type = %d, len = %d)",
		       __FUNCTION__,
		       rtmsg_type(msg), n);
	}
	if (n > rtmsg_len(msg)) {
		/*
		 * This usually won't happen for messages received on
		 * an routing socket.
		 */
		if (dflag > 1)
			syslog(LOG_DEBUG,
			       "<%s> received data length is larger than"
			       "1st routing message len. multiple messages?"
			       " read %d bytes, but 1st msg len = %d",
			       __FUNCTION__, n, rtmsg_len(msg));
	}

	lim = msg + n;
	for (next = msg; next < lim; next += len) {
		next = get_next_msg(next, lim, 0, &len,
				    RTADV_TYPE2BITMASK(RTM_ADD) |
				    RTADV_TYPE2BITMASK(RTM_DELETE) |
				    RTADV_TYPE2BITMASK(RTM_NEWADDR) |
				    RTADV_TYPE2BITMASK(RTM_DELADDR) |
				    RTADV_TYPE2BITMASK(RTM_IFINFO));
		if (len == 0)
			break;
		type = rtmsg_type(next);
		switch (type) {
		case RTM_ADD:
		case RTM_DELETE:
			ifindex = get_rtm_ifindex(next);
			break;
		case RTM_NEWADDR:
		case RTM_DELADDR:
			ifindex = get_ifam_ifindex(next);
			break;
		case RTM_IFINFO:
			ifindex = get_ifm_ifindex(next);
			break;
		default:
			/* should not reach here */
			if (dflag > 1) {
				syslog(LOG_DEBUG,
				       "<%s:%d> unknown rtmsg %d on %s",
				       __FUNCTION__, __LINE__, type,
				       if_indextoname(ifindex, ifname));
			}
			return;
		}

		if ((rai = if_indextorainfo(ifindex)) == NULL) {
			if (dflag > 1) {
				syslog(LOG_DEBUG,
				       "<%s> route changed on "
				       "non advertising interface(%s)",
				       __FUNCTION__,
				       if_indextoname(ifindex, ifname));
			}
			return;
		}

		switch(type) {
		 case RTM_ADD:
			 /* init iffalgs because it may have changed */
			 iflist[ifindex]->ifm_flags =
			 	if_getflags(ifindex,
					    iflist[ifindex]->ifm_flags);

			 addr = get_addr(msg);
			 plen = get_prefixlen(msg);
			 /* sanity check for plen */
			 if (plen < 4 /* as RFC2373, prefixlen is at least 4 */
			     || plen > 127) {
				syslog(LOG_INFO, "<%s> new interface route's"
				       "plen %d is invalid for a prefix",
				       __FUNCTION__, plen);
				return;
			 }
			 prefix = find_prefix(rai, addr, plen);
			 if (prefix) {
				 if (dflag > 1) {
					 syslog(LOG_DEBUG,
						"<%s> new prefix(%s/%d) "
						"added on %s, "
						"but it was already in list",
						__FUNCTION__,
						inet_ntop(AF_INET6,
							  addr, (char *)addrbuf,
							  INET6_ADDRSTRLEN),
						plen,
						rai->ifname);
				 }
				 return;
			 }
			 make_prefix(rai, ifindex, addr, plen);
			 break;
		 case RTM_DELETE:
			 /* init ifflags because it may have changed */
			 iflist[ifindex]->ifm_flags =
			 	if_getflags(ifindex,
					    iflist[ifindex]->ifm_flags);

			 addr = get_addr(msg);
			 plen = get_prefixlen(msg);
			 /* sanity check for plen */
			 if (plen < 4 /* as RFC2373, prefixlen is at least 4 */
			     || plen > 127) {
				syslog(LOG_INFO, "<%s> deleted interface"
				       "route's"
				       "plen %d is invalid for a prefix",
				       __FUNCTION__, plen);
				return;
			 }
			 prefix = find_prefix(rai, addr, plen);
			 if (prefix == NULL) {
				 if (dflag > 1) {
					 syslog(LOG_DEBUG,
						"<%s> prefix(%s/%d) was "
						"deleted on %s, "
						"but it was not in list",
						__FUNCTION__,
						inet_ntop(AF_INET6,
							  addr, (char *)addrbuf,
							  INET6_ADDRSTRLEN),
						plen,
						rai->ifname);
				 }
				 return;
			 }
			 delete_prefix(rai, prefix);
			 break;
		case RTM_NEWADDR:
		case RTM_DELADDR:
			 /* init ifflags because it may have changed */
			 iflist[ifindex]->ifm_flags =
			 	if_getflags(ifindex,
					    iflist[ifindex]->ifm_flags);
			 break;
		case RTM_IFINFO:
			 iflist[ifindex]->ifm_flags = get_ifm_flags(next);
			 break;
		default:
			/* should not reach here */
			if (dflag > 1) {
				syslog(LOG_DEBUG,
				       "<%s:%d> unknown rtmsg %d on %s",
				       __FUNCTION__, __LINE__, type,
				       if_indextoname(ifindex, ifname));
			}
			return;
		}
	}

	return;
}

void
rtadvd_input(skt)
{
	int i;
	int *hlimp = NULL;
	struct icmp6_hdr *icp;
	int ifindex = 0;
	struct cmsghdr *cm;
	struct in6_pktinfo *pi = NULL;
	u_char ntopbuf[INET6_ADDRSTRLEN], ifnamebuf[IFNAMSIZ];
	struct in6_addr dst = in6addr_any;

	/*
	 * Get message. We reset msg_controllen since the field could
	 * be modified if we had received a message before setting
	 * receive options.
	 */
	rcvmhdr.msg_controllen = sizeof(rcvcmsgbuf);
	if ((i = recvmsg(skt, &rcvmhdr, 0)) < 0)
		return;

	/* extract optional information via Advanced API */
	for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(&rcvmhdr);
	     cm;
	     cm = (struct cmsghdr *)CMSG_NXTHDR(&rcvmhdr, cm)) {
		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_PKTINFO &&
		    cm->cmsg_len == CMSG_LEN(sizeof(struct in6_pktinfo))) {
			pi = (struct in6_pktinfo *)(CMSG_DATA(cm));
			ifindex = pi->ipi6_ifindex;
			dst = pi->ipi6_addr;
		}
		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_HOPLIMIT &&
		    cm->cmsg_len == CMSG_LEN(sizeof(int)))
			hlimp = (int *)CMSG_DATA(cm);
	}
	if (ifindex == 0) {
		syslog(LOG_ERR,
		       "<%s> failed to get receiving interface",
		       __FUNCTION__);
		return;
	}
	if (hlimp == NULL) {
		syslog(LOG_ERR,
		       "<%s> failed to get receiving hop limit",
		       __FUNCTION__);
		return;
	}

	if (i < sizeof(struct icmp6_hdr)) {
		syslog(LOG_ERR,
		       "<%s> packet size(%d) is too short",
		       __FUNCTION__, i);
		return;
	}

	icp = (struct icmp6_hdr *)rcvmhdr.msg_iov[0].iov_base;

	switch(icp->icmp6_type) {
	 case ND_ROUTER_SOLICIT:
		 /* hop limit verification - RFC-2461 6.1.1 */
		 if (*hlimp != 255) {
			 syslog(LOG_NOTICE,
				"<%s> invalid hop limit(%d) "
				"received from %s on %s",
				__FUNCTION__, *hlimp,
				inet_ntop(AF_INET6, &from.sin6_addr, ntopbuf,
					  INET6_ADDRSTRLEN),
				if_indextoname(pi->ipi6_ifindex, ifnamebuf));
			 return;
		 }
		 rs_input(i, (struct nd_router_solicit *)icp, pi, &from);
		 break;
	 case ND_ROUTER_ADVERT:
		 /* hop limit verification - RFC-2461 6.1.1 */
		 if (*hlimp != 255) {
			 syslog(LOG_NOTICE,
				"<%s> invalid hop limit(%d) "
				"received from %s on %s",
				__FUNCTION__, *hlimp,
				inet_ntop(AF_INET6, &from.sin6_addr, ntopbuf,
					  INET6_ADDRSTRLEN),
				if_indextoname(pi->ipi6_ifindex, ifnamebuf));
			 return;
		 }
		 ra_input(i, (struct nd_router_advert *)icp, pi, &from);
		 break;
	 case ICMP6_ROUTER_RENUMBERING:
		 if (accept_rr == 0) {
			 syslog(LOG_ERR,
				"<%s> received a router renumbering "
				"message, but not allowed to be accepted",
				__FUNCTION__);
			 break;
		 }
		 rr_input(i, (struct icmp6_router_renum *)icp, pi, &from,
			  &dst);
		 break;
	 default:
		 /*
		  * Note that this case is POSSIBLE, especially just
		  * after invocation of the daemon. This is because we
		  * could receive message after opening the socket and
		  * before setting ICMP6 type filter(see sock_open()).
		  */
		 syslog(LOG_ERR,
			"<%s> invalid icmp type(%d)",
			__FUNCTION__, icp->icmp6_type);
		 return;
	}

	return;
}

static void
rs_input(int len, struct nd_router_solicit *rs,
	 struct in6_pktinfo *pi, struct sockaddr_in6 *from)
{
	u_char ntopbuf[INET6_ADDRSTRLEN], ifnamebuf[IFNAMSIZ];
	union nd_opts ndopts;
	struct rainfo *ra;

	syslog(LOG_DEBUG,
	       "<%s> RS received from %s on %s",
	       __FUNCTION__,
	       inet_ntop(AF_INET6, &from->sin6_addr,
			 ntopbuf, INET6_ADDRSTRLEN),
	       if_indextoname(pi->ipi6_ifindex, ifnamebuf));

	/* ND option check */
	memset(&ndopts, 0, sizeof(ndopts));
	if (nd6_options((struct nd_opt_hdr *)(rs + 1),
			len - sizeof(struct nd_router_solicit),
			 &ndopts, NDOPT_FLAG_SRCLINKADDR)) {
		syslog(LOG_DEBUG,
		       "<%s> ND option check failed for an RS from %s on %s",
		       __FUNCTION__,
		       inet_ntop(AF_INET6, &from->sin6_addr,
				 ntopbuf, INET6_ADDRSTRLEN),
		       if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		return;
	}

	/*
	 * If the IP source address is the unspecified address, there
	 * must be no source link-layer address option in the message.
	 * (RFC-2461 6.1.1)
	 */
	if (IN6_IS_ADDR_UNSPECIFIED(&from->sin6_addr) &&
	    ndopts.nd_opts_src_lladdr) {
		syslog(LOG_ERR,
		       "<%s> RS from unspecified src on %s has a link-layer"
		       " address option",
		       __FUNCTION__,
		       if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		goto done;
	}

	ra = ralist;
	while (ra != NULL) {
		if (pi->ipi6_ifindex == ra->ifindex)
			break;
		ra = ra->next;
	}
	if (ra == NULL) {
		syslog(LOG_INFO,
		       "<%s> RS received on non advertising interface(%s)",
		       __FUNCTION__,
		       if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		goto done;
	}

	/*
	 * Decide whether to send RA according to the rate-limit
	 * consideration.
	 */
	{
		long delay;	/* must not be greater than 1000000 */
		struct timeval interval, now, min_delay, tm_tmp, *rest;

		/*
		 * If there is already a waiting RS packet, don't
		 * update the timer.
		 */
		if (ra->waiting++)
			goto done;

		/*
		 * Compute a random delay. If the computed value
		 * corresponds to a time later than the time the next
		 * multicast RA is scheduled to be sent, ignore the random
		 * delay and send the advertisement at the
		 * already-scheduled time. RFC-2461 6.2.6
		 */
		delay = random() % MAX_RA_DELAY_TIME;
		interval.tv_sec = 0;
		interval.tv_usec = delay;
		rest = rtadvd_timer_rest(ra->timer);
		if (TIMEVAL_LT(*rest, interval)) {
			syslog(LOG_DEBUG,
			       "<%s> random delay is larger than "
			       "the rest of normal timer",
			       __FUNCTION__);
			interval = *rest;
		}

		/*
		 * If we sent a multicast Router Advertisement within
		 * the last MIN_DELAY_BETWEEN_RAS seconds, schedule
		 * the advertisement to be sent at a time corresponding to
		 * MIN_DELAY_BETWEEN_RAS plus the random value after the
		 * previous advertisement was sent.
		 */
		gettimeofday(&now, NULL);
		TIMEVAL_SUB(&now, &ra->lastsent, &tm_tmp);
		min_delay.tv_sec = MIN_DELAY_BETWEEN_RAS;
		min_delay.tv_usec = 0;
		if (TIMEVAL_LT(tm_tmp, min_delay)) {
			TIMEVAL_SUB(&min_delay, &tm_tmp, &min_delay);
			TIMEVAL_ADD(&min_delay, &interval, &interval);
		}
		rtadvd_set_timer(&interval, ra->timer);
			goto done;
	}

  done:
	free_ndopts(&ndopts);
	return;
}

static void
ra_input(int len, struct nd_router_advert *ra,
	 struct in6_pktinfo *pi, struct sockaddr_in6 *from)
{
	struct rainfo *rai;
	u_char ntopbuf[INET6_ADDRSTRLEN], ifnamebuf[IFNAMSIZ];
	union nd_opts ndopts;
	char *on_off[] = {"OFF", "ON"};
	u_int32_t reachabletime, retranstimer, mtu;

	syslog(LOG_DEBUG,
	       "<%s> RA received from %s on %s",
	       __FUNCTION__,
	       inet_ntop(AF_INET6, &from->sin6_addr,
			 ntopbuf, INET6_ADDRSTRLEN),
	       if_indextoname(pi->ipi6_ifindex, ifnamebuf));

	/* ND option check */
	memset(&ndopts, 0, sizeof(ndopts));
	if (nd6_options((struct nd_opt_hdr *)(ra + 1),
			len - sizeof(struct nd_router_advert),
			 &ndopts,
			NDOPT_FLAG_PREFIXINFO | NDOPT_FLAG_MTU)) {
		syslog(LOG_ERR,
		       "<%s> ND option check failed for an RA from %s on %s",
		       __FUNCTION__,
		       inet_ntop(AF_INET6, &from->sin6_addr,
				 ntopbuf, INET6_ADDRSTRLEN),
		       if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		return;
	}

	/*
	 * RA consistency check according to RFC-2461 6.2.7
	 */
	if ((rai = if_indextorainfo(pi->ipi6_ifindex)) == 0) {
		syslog(LOG_INFO,
		       "<%s> received RA from %s on non-advertising"
		       " interface(%s)",
		       __FUNCTION__,
		       inet_ntop(AF_INET6, &from->sin6_addr,
				 ntopbuf, INET6_ADDRSTRLEN),
		       if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		goto done;
	}
	/* Cur Hop Limit value */
	if (ra->nd_ra_curhoplimit && rai->hoplimit &&
	    ra->nd_ra_curhoplimit != rai->hoplimit) {
		syslog(LOG_WARNING,
		       "<%s> CurHopLimit inconsistent on %s:"
		       " %d from %s, %d from us",
		       __FUNCTION__,
		       rai->ifname,
		       ra->nd_ra_curhoplimit,
		       inet_ntop(AF_INET6, &from->sin6_addr,
				 ntopbuf, INET6_ADDRSTRLEN),
		       rai->hoplimit);
	}
	/* M flag */
	if ((ra->nd_ra_flags_reserved & ND_RA_FLAG_MANAGED) !=
	    rai->managedflg) {
		syslog(LOG_WARNING,
		       "<%s> M flag inconsistent on %s:"
		       " %s from %s, %s from us",
		       __FUNCTION__,
		       rai->ifname,
		       on_off[!rai->managedflg],
		       inet_ntop(AF_INET6, &from->sin6_addr,
				 ntopbuf, INET6_ADDRSTRLEN),
		       on_off[rai->managedflg]);
	}
	/* O flag */
	if ((ra->nd_ra_flags_reserved & ND_RA_FLAG_OTHER) !=
	    rai->otherflg) {
		syslog(LOG_WARNING,
		       "<%s> O flag inconsistent on %s:"
		       " %s from %s, %s from us",
		       __FUNCTION__,
		       rai->ifname,
		       on_off[!rai->otherflg],
		       inet_ntop(AF_INET6, &from->sin6_addr,
				 ntopbuf, INET6_ADDRSTRLEN),
		       on_off[rai->otherflg]);
	}
	/* Reachable Time */
	reachabletime = ntohl(ra->nd_ra_reachable);
	if (reachabletime && rai->reachabletime &&
	    reachabletime != rai->reachabletime) {
		syslog(LOG_WARNING,
		       "<%s> ReachableTime inconsistent on %s:"
		       " %d from %s, %d from us",
		       __FUNCTION__,
		       rai->ifname,
		       reachabletime,
		       inet_ntop(AF_INET6, &from->sin6_addr,
				 ntopbuf, INET6_ADDRSTRLEN),
		       rai->reachabletime);
	}
	/* Retrans Timer */
	retranstimer = ntohl(ra->nd_ra_retransmit);
	if (retranstimer && rai->retranstimer &&
	    retranstimer != rai->retranstimer) {
		syslog(LOG_WARNING,
		       "<%s> RetranceTimer inconsistent on %s:"
		       " %d from %s, %d from us",
		       __FUNCTION__,
		       rai->ifname,
		       retranstimer,
		       inet_ntop(AF_INET6, &from->sin6_addr,
				 ntopbuf, INET6_ADDRSTRLEN),
		       rai->retranstimer);
	}
	/* Values in the MTU options */
	if (ndopts.nd_opts_mtu) {
		mtu = ntohl(ndopts.nd_opts_mtu->nd_opt_mtu_mtu);
		if (mtu && rai->linkmtu && mtu != rai->linkmtu) {
			syslog(LOG_WARNING,
			       "<%s> MTU option value inconsistent on %s:"
			       " %d from %s, %d from us",
			       __FUNCTION__,
			       rai->ifname, mtu,
			       inet_ntop(AF_INET6, &from->sin6_addr,
					 ntopbuf, INET6_ADDRSTRLEN),
			       rai->linkmtu);
		}
	}
	/* Preferred and Valid Lifetimes for prefixes */
	{
		struct nd_optlist *optp = ndopts.nd_opts_list;

		if (ndopts.nd_opts_pi)
			prefix_check(ndopts.nd_opts_pi, rai, from);
		while (optp) {
			prefix_check((struct nd_opt_prefix_info *)optp->opt,
				     rai, from);
			optp = optp->next;
		}
	}

  done:
	free_ndopts(&ndopts);
	return;
}

static void
prefix_check(struct nd_opt_prefix_info *pinfo,
	     struct rainfo *rai, struct sockaddr_in6 *from)
{
	u_int32_t preferred_time, valid_time;
	struct prefix *pp;
	u_char ntopbuf[INET6_ADDRSTRLEN], prefixbuf[INET6_ADDRSTRLEN];

	/*
	 * log if the adveritsed prefix has link-local scope(sanity check?)
	 */
	if (IN6_IS_ADDR_LINKLOCAL(&pinfo->nd_opt_pi_prefix)) {
		syslog(LOG_INFO,
		       "<%s> link-local prefix %s/%d is advertised "
		       "from %s on %s",
		       __FUNCTION__,
		       inet_ntop(AF_INET6, &pinfo->nd_opt_pi_prefix,
				 prefixbuf, INET6_ADDRSTRLEN),
		       pinfo->nd_opt_pi_prefix_len,
		       inet_ntop(AF_INET6, &from->sin6_addr,
				 ntopbuf, INET6_ADDRSTRLEN),
		       rai->ifname);
	}

	if ((pp = find_prefix(rai, &pinfo->nd_opt_pi_prefix,
			      pinfo->nd_opt_pi_prefix_len)) == NULL) {
		syslog(LOG_INFO,
		       "<%s> prefix %s/%d from %s on %s is not in our list",
		       __FUNCTION__,
		       inet_ntop(AF_INET6, &pinfo->nd_opt_pi_prefix,
				 prefixbuf, INET6_ADDRSTRLEN),
		       pinfo->nd_opt_pi_prefix_len,
		       inet_ntop(AF_INET6, &from->sin6_addr,
				 ntopbuf, INET6_ADDRSTRLEN),
		       rai->ifname);
		return;
	}

	preferred_time = ntohl(pinfo->nd_opt_pi_preferred_time);
	if (preferred_time != pp->preflifetime)
		syslog(LOG_WARNING,
		       "<%s> prefeerred lifetime for %s/%d"
		       " inconsistent on %s:"
		       " %d from %s, %d from us",
		       __FUNCTION__,
		       inet_ntop(AF_INET6, &pinfo->nd_opt_pi_prefix,
				 prefixbuf, INET6_ADDRSTRLEN),
		       pinfo->nd_opt_pi_prefix_len,
		       rai->ifname, preferred_time,
		       inet_ntop(AF_INET6, &from->sin6_addr,
				 ntopbuf, INET6_ADDRSTRLEN),
		       pp->preflifetime);

	valid_time = ntohl(pinfo->nd_opt_pi_valid_time);
	if (valid_time != pp->validlifetime)
		syslog(LOG_WARNING,
		       "<%s> valid lifetime for %s/%d"
		       " inconsistent on %s:"
		       " %d from %s, %d from us",
		       __FUNCTION__,
		       inet_ntop(AF_INET6, &pinfo->nd_opt_pi_prefix,
				 prefixbuf, INET6_ADDRSTRLEN),
		       pinfo->nd_opt_pi_prefix_len,
		       rai->ifname, valid_time,
		       inet_ntop(AF_INET6, &from->sin6_addr,
				 ntopbuf, INET6_ADDRSTRLEN),
		       pp->validlifetime);
}

struct prefix *
find_prefix(struct rainfo *rai, struct in6_addr *prefix, int plen)
{
	struct prefix *pp;
	int bytelen, bitlen;

	for (pp = rai->prefix.next; pp != &rai->prefix; pp = pp->next) {
		if (plen != pp->prefixlen)
			continue;
		bytelen = plen / 8;
		bitlen = plen % 8;
		if (memcmp((void *)prefix, (void *)&pp->prefix, bytelen))
			continue;
		if (prefix->s6_addr[bytelen] >> (8 - bitlen) ==
		    pp->prefix.s6_addr[bytelen] >> (8 - bitlen))
			return(pp);
	}

	return(NULL);
}

static int
nd6_options(struct nd_opt_hdr *hdr, int limit,
	    union nd_opts *ndopts, u_int32_t optflags)
{
	int optlen = 0;

	for (; limit > 0; limit -= optlen) {
		hdr = (struct nd_opt_hdr *)((caddr_t)hdr + optlen);
		optlen = hdr->nd_opt_len << 3;
		if (hdr->nd_opt_len == 0) {
			syslog(LOG_ERR,
			       "<%s> bad ND option length(0) (type = %d)",
			       __FUNCTION__, hdr->nd_opt_type);
			goto bad;
		}

		if (hdr->nd_opt_type > ND_OPT_MTU) {
			syslog(LOG_INFO,
			       "<%s> unknown ND option(type %d)",
			       __FUNCTION__,
			       hdr->nd_opt_type);
			continue;
		}

		if ((ndopt_flags[hdr->nd_opt_type] & optflags) == 0) {
			syslog(LOG_INFO,
			       "<%s> unexpected ND option(type %d)",
			       __FUNCTION__,
			       hdr->nd_opt_type);
			continue;
		}

		switch(hdr->nd_opt_type) {
		 case ND_OPT_SOURCE_LINKADDR:
		 case ND_OPT_TARGET_LINKADDR:
		 case ND_OPT_REDIRECTED_HEADER:
		 case ND_OPT_MTU:
			 if (ndopts->nd_opt_array[hdr->nd_opt_type]) {
				 syslog(LOG_INFO,
					"<%s> duplicated ND option"
					" (type = %d)",
					__FUNCTION__,
					hdr->nd_opt_type);
			 }
			 ndopts->nd_opt_array[hdr->nd_opt_type] = hdr;
			 break;
		 case ND_OPT_PREFIX_INFORMATION:
		 {
			 struct nd_optlist *pfxlist;

			 if (ndopts->nd_opts_pi == 0) {
				 ndopts->nd_opts_pi =
					 (struct nd_opt_prefix_info *)hdr;
				 continue;
			 }
			 if ((pfxlist = malloc(sizeof(*pfxlist))) == NULL) {
				 syslog(LOG_ERR,
					"<%s> can't allocate memory",
					__FUNCTION__);
				 goto bad;
			 }
			 pfxlist->next = ndopts->nd_opts_list;
			 pfxlist->opt = hdr;
			 ndopts->nd_opts_list = pfxlist;

			 break;
		 }
		 default:	/* impossible */
			 break;
		}
	}

	return(0);

  bad:
	free_ndopts(ndopts);

	return(-1);
}

static void
free_ndopts(union nd_opts *ndopts)
{
	struct nd_optlist *opt = ndopts->nd_opts_list, *next;

	while(opt) {
		next = opt->next;
		free(opt);
		opt = next;
	}
}

void
sock_open(int *sockp, int is_rr, char *policy)
{
	struct icmp6_filter filt;
	struct ipv6_mreq mreq;
	struct rainfo *ra = ralist;
	int on;
	char *rtr_str;
	/* XXX: should be max MTU attached to the node */
	static u_char answer[1500];
	static u_char sndcmsgbuf[CMSG_SPACE(sizeof(struct in6_pktinfo)) +
				CMSG_SPACE(sizeof(int))];

	if ((*sockp = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6)) < 0) {
		syslog(LOG_ERR, "<%s> socket: %s", __FUNCTION__,
		       strerror(errno));
		exit(1);
	}

	/* specify to tell receiving interface */
	on = 1;
	if (setsockopt(*sockp, IPPROTO_IPV6, IPV6_PKTINFO, &on,
		       sizeof(on)) < 0) {
		syslog(LOG_ERR, "<%s> IPV6_PKTINFO: %s",
		       __FUNCTION__, strerror(errno));
		exit(1);
	}

	on = 1;
	/* specify to tell value of hoplimit field of received IP6 hdr */
	if (setsockopt(*sockp, IPPROTO_IPV6, IPV6_HOPLIMIT, &on,
		       sizeof(on)) < 0) {
		syslog(LOG_ERR, "<%s> IPV6_HOPLIMIT: %s",
		       __FUNCTION__, strerror(errno));
		exit(1);
	}

	ICMP6_FILTER_SETBLOCKALL(&filt);
	if (is_rr)
		ICMP6_FILTER_SETPASS(ICMP6_ROUTER_RENUMBERING, &filt);
	else {
		ICMP6_FILTER_SETPASS(ND_ROUTER_SOLICIT, &filt);
		ICMP6_FILTER_SETPASS(ND_ROUTER_ADVERT, &filt);
	}
	if (setsockopt(*sockp, IPPROTO_ICMPV6, ICMP6_FILTER, &filt,
		       sizeof(filt)) < 0) {
		syslog(LOG_ERR, "<%s> IICMP6_FILTER: %s",
		       __FUNCTION__, strerror(errno));
		exit(1);
	}


#ifdef IPSEC
#ifdef IPSEC_POLICY_IPSEC
	if (is_rr && policy != NULL) {
		char *buf;

		buf = ipsec_set_policy(policy, strlen(policy));
		if (buf == NULL)
			errx(EX_CONFIG, ipsec_strerror());
		if (setsockopt(*sockp, IPPROTO_IPV6, IPV6_IPSEC_POLICY,
			       buf, ipsec_get_policylen(buf)) < 0)
			err(EX_CONFIG, "ipsec policy cannot be configured");
		free(buf);
	}
#endif /*IPSEC_POLICY_IPSEC*/
#endif /*IPSEC*/

	/*
	 * join all routers multicast address on each advertising interface.
	 */
	rtr_str = is_rr ? ALLSITEROUTERS : ALLROUTERS;
	if (inet_pton(AF_INET6, rtr_str, &mreq.ipv6mr_multiaddr.s6_addr)
	    != 1) {
		syslog(LOG_ERR, "<%s> inet_pton for %s failed(library bug?)",
		       __FUNCTION__, rtr_str);
		exit(1);
	}
	while(ra) {
		mreq.ipv6mr_interface = ra->ifindex;
		if (setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq,
			       sizeof(mreq)) < 0) {
			syslog(LOG_ERR, "<%s> IPV6_JOIN_GROUP for %s on"
			       "%s: %s", __FUNCTION__,
			       rtr_str, ra->ifname, strerror(errno));
			exit(1);
		}
		ra = ra->next;
	}
	if (is_rr) /* msghdrs should have been alreadey initialized. */
		return;

	/* initialize msghdr for receiving packets */
	rcviov[0].iov_base = (caddr_t)answer;
	rcviov[0].iov_len = sizeof(answer);
	rcvmhdr.msg_name = (caddr_t)&from;
	rcvmhdr.msg_namelen = sizeof(from);
	rcvmhdr.msg_iov = rcviov;
	rcvmhdr.msg_iovlen = 1;
	rcvmhdr.msg_control = (caddr_t) rcvcmsgbuf;
	rcvmhdr.msg_controllen = sizeof(rcvcmsgbuf);

	/* initialize msghdr for sending packets */
	sndmhdr.msg_namelen = sizeof(struct sockaddr_in6);
	sndmhdr.msg_iov = sndiov;
	sndmhdr.msg_iovlen = 1;
	sndmhdr.msg_control = (caddr_t)sndcmsgbuf;
	sndmhdr.msg_controllen = sizeof(sndcmsgbuf);

	return;
}

/* open a routing socket to watch the routing table */
static void
rtsock_open()
{
	if ((rtsock = socket(PF_ROUTE, SOCK_RAW, 0)) < 0) {
		syslog(LOG_ERR,
		       "<%s> socket: %s", __FUNCTION__, strerror(errno));
		exit(1);
	}
}

static struct rainfo *
if_indextorainfo(int index)
{
	struct rainfo *rai = ralist;

	for (rai = ralist; rai; rai = rai->next) {
		if (rai->ifindex == index)
			return(rai);
	}

	return(NULL);		/* search failed */
}

static void
ra_output(rainfo)
struct rainfo *rainfo;
{
	int i;

	struct cmsghdr *cm;
	struct in6_pktinfo *pi;

	sndmhdr.msg_name = (caddr_t)&sin6_allnodes;
	sndmhdr.msg_iov[0].iov_base = (caddr_t)rainfo->ra_data;
	sndmhdr.msg_iov[0].iov_len = rainfo->ra_datalen;

	cm = CMSG_FIRSTHDR(&sndmhdr);
	/* specify the outgoing interface */
	cm->cmsg_level = IPPROTO_IPV6;
	cm->cmsg_type = IPV6_PKTINFO;
	cm->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
	pi = (struct in6_pktinfo *)CMSG_DATA(cm);
	memset(&pi->ipi6_addr, 0, sizeof(pi->ipi6_addr));	/*XXX*/
	pi->ipi6_ifindex = rainfo->ifindex;

	/* specify the hop limit of the packet */
	{
		int hoplimit = 255;

		cm = CMSG_NXTHDR(&sndmhdr, cm);
		cm->cmsg_level = IPPROTO_IPV6;
		cm->cmsg_type = IPV6_HOPLIMIT;
		cm->cmsg_len = CMSG_LEN(sizeof(int));
		memcpy(CMSG_DATA(cm), &hoplimit, sizeof(int));
	}

	syslog(LOG_DEBUG,
	       "<%s> send RA on %s, # of waitings = %d",
	       __FUNCTION__, rainfo->ifname, rainfo->waiting);

	i = sendmsg(sock, &sndmhdr, 0);

	if (i < 0 || i != rainfo->ra_datalen)  {
		if (i < 0) {
			syslog(LOG_ERR, "<%s> sendmsg: %s",
			       __FUNCTION__, strerror(errno));
		}
	}

	/* update counter */
	if (rainfo->initcounter < MAX_INITIAL_RTR_ADVERTISEMENTS)
		rainfo->initcounter++;

	/* update timestamp */
	gettimeofday(&rainfo->lastsent, NULL);

	/* reset waiting conter */
	rainfo->waiting = 0;
}

/* process RA timer */
void
ra_timeout(void *data)
{
	struct rainfo *rai = (struct rainfo *)data;

#ifdef notyet
	/* if necessary, reconstruct the packet. */
#endif

	syslog(LOG_DEBUG,
	       "<%s> RA timer on %s is expired",
	       __FUNCTION__, rai->ifname);

	if (iflist[rai->ifindex]->ifm_flags & IFF_UP)
		ra_output(rai);
	else
		syslog(LOG_DEBUG, "<%s> %s is not up, skip sending RA",
		       __FUNCTION__, rai->ifname);
}

/* update RA timer */
void
ra_timer_update(void *data, struct timeval *tm)
{
	struct rainfo *rai = (struct rainfo *)data;
	long interval;

	/*
	 * Whenever a multicast advertisement is sent from an interface,
	 * the timer is reset to a uniformly-distributed random value
	 * between the interface's configured MinRtrAdvInterval and
	 * MaxRtrAdvInterval(discovery-v2-02 6.2.4).
	 */
	interval = rai->mininterval;
	interval += random() % (rai->maxinterval - rai->mininterval);

	/*
	 * For the first few advertisements (up to
	 * MAX_INITIAL_RTR_ADVERTISEMENTS), if the randomly chosen interval
	 * is greater than MAX_INITIAL_RTR_ADVERT_INTERVAL, the timer
	 * SHOULD be set to MAX_INITIAL_RTR_ADVERT_INTERVAL instead.
	 * (RFC-2461 6.2.4)
	 */
	if (rai->initcounter < MAX_INITIAL_RTR_ADVERTISEMENTS &&
	    interval > MAX_INITIAL_RTR_ADVERT_INTERVAL)
		interval = MAX_INITIAL_RTR_ADVERT_INTERVAL;

	tm->tv_sec = interval;
	tm->tv_usec = 0;

	syslog(LOG_DEBUG,
	       "<%s> RA timer on %s is set to %ld:%ld",
	       __FUNCTION__, rai->ifname, tm->tv_sec, tm->tv_usec);

	return;
}
