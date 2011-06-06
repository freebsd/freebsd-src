/*	$FreeBSD$	*/
/*	$KAME: rtadvd.c,v 1.82 2003/08/05 12:34:23 itojun Exp $	*/

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
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/queue.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_media.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>

#include <arpa/inet.h>

#include <net/if_var.h>
#include <netinet/in_var.h>
#include <netinet6/nd6.h>

#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <err.h>
#include <errno.h>
#include <libutil.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#include "rtadvd.h"
#include "rrenum.h"
#include "advcap.h"
#include "timer.h"
#include "if.h"
#include "config.h"
#include "dump.h"
#include "pathnames.h"

struct msghdr rcvmhdr;
static u_char *rcvcmsgbuf;
static size_t rcvcmsgbuflen;
static u_char *sndcmsgbuf = NULL;
static size_t sndcmsgbuflen;
volatile sig_atomic_t do_dump;
volatile sig_atomic_t do_die;
struct msghdr sndmhdr;
struct iovec rcviov[2];
struct iovec sndiov[2];
struct sockaddr_in6 rcvfrom;
static const char *dumpfilename = _PATH_RTADVDDUMP;
static const char *pidfilename = _PATH_RTADVDPID;
const char *conffile = _PATH_RTADVDCONF;
static struct pidfh *pfh;
static char *mcastif;
int sock;
int rtsock = -1;
int accept_rr = 0;
int dflag = 0, sflag = 0;
static int ifl_len;
static char **ifl_names;

struct railist_head_t railist =
    TAILQ_HEAD_INITIALIZER(railist);

struct nd_optlist {
	TAILQ_ENTRY(nd_optlist)	nol_next;
	struct nd_opt_hdr *nol_opt;
};
union nd_opt {
	struct nd_opt_hdr *opt_array[9];
	struct {
		struct nd_opt_hdr *zero;
		struct nd_opt_hdr *src_lladdr;
		struct nd_opt_hdr *tgt_lladdr;
		struct nd_opt_prefix_info *pi;
		struct nd_opt_rd_hdr *rh;
		struct nd_opt_mtu *mtu;
		TAILQ_HEAD(, nd_optlist) opt_list;
	} nd_opt_each;
};
#define opt_src_lladdr	nd_opt_each.src_lladdr
#define opt_tgt_lladdr	nd_opt_each.tgt_lladdr
#define opt_pi		nd_opt_each.pi
#define opt_rh		nd_opt_each.rh
#define opt_mtu		nd_opt_each.mtu
#define opt_list	nd_opt_each.opt_list

#define NDOPT_FLAG_SRCLINKADDR	(1 << 0)
#define NDOPT_FLAG_TGTLINKADDR	(1 << 1)
#define NDOPT_FLAG_PREFIXINFO	(1 << 2)
#define NDOPT_FLAG_RDHDR	(1 << 3)
#define NDOPT_FLAG_MTU		(1 << 4)
#define NDOPT_FLAG_RDNSS	(1 << 5)
#define NDOPT_FLAG_DNSSL	(1 << 6)

u_int32_t ndopt_flags[] = {
	[ND_OPT_SOURCE_LINKADDR]	= NDOPT_FLAG_SRCLINKADDR,
	[ND_OPT_TARGET_LINKADDR]	= NDOPT_FLAG_TGTLINKADDR,
	[ND_OPT_PREFIX_INFORMATION]	= NDOPT_FLAG_PREFIXINFO,
	[ND_OPT_REDIRECTED_HEADER]	= NDOPT_FLAG_RDHDR,
	[ND_OPT_MTU]			= NDOPT_FLAG_MTU,
	[ND_OPT_RDNSS]			= NDOPT_FLAG_RDNSS,
	[ND_OPT_DNSSL]			= NDOPT_FLAG_DNSSL,
};

struct sockaddr_in6 sin6_linklocal_allnodes = {
        .sin6_len =     sizeof(sin6_linklocal_allnodes),
        .sin6_family =  AF_INET6,
        .sin6_addr =    IN6ADDR_LINKLOCAL_ALLNODES_INIT,
};

struct sockaddr_in6 sin6_linklocal_allrouters = {
        .sin6_len =     sizeof(sin6_linklocal_allrouters),
        .sin6_family =  AF_INET6,
        .sin6_addr =    IN6ADDR_LINKLOCAL_ALLROUTERS_INIT,
};

struct sockaddr_in6 sin6_sitelocal_allrouters = {
        .sin6_len =     sizeof(sin6_sitelocal_allrouters),
        .sin6_family =  AF_INET6,
        .sin6_addr =    IN6ADDR_SITELOCAL_ALLROUTERS_INIT,
};

static void	set_die(int);
static void	die(void);
static void	sock_open(void);
static void	rtsock_open(void);
static void	rtadvd_input(void);
static void	rs_input(int, struct nd_router_solicit *,
		    struct in6_pktinfo *, struct sockaddr_in6 *);
static void	ra_input(int, struct nd_router_advert *,
		    struct in6_pktinfo *, struct sockaddr_in6 *);
static int	prefix_check(struct nd_opt_prefix_info *, struct rainfo *,
		    struct sockaddr_in6 *);
static int	nd6_options(struct nd_opt_hdr *, int,
		    union nd_opt *, u_int32_t);
static void	free_ndopts(union nd_opt *);
static void	ra_output(struct rainfo *);
static void	rtmsg_input(void);
static void	rtadvd_set_dump_file(int);
static void	set_short_delay(struct rainfo *);
static int	ifl_lookup(char *, char **, int);
static int	check_accept_rtadv(int);
static int	getinet6sysctl(int);

int
main(int argc, char *argv[])
{
#ifdef HAVE_POLL_H
	struct pollfd set[2];
#else
	fd_set *fdsetp, *selectfdp;
	int fdmasks;
	int maxfd = 0;
#endif
	struct timeval *timeout;
	int i, ch;
	int fflag = 0, logopt;
	pid_t pid, otherpid;
	int error;

	/* get command line options and arguments */
	while ((ch = getopt(argc, argv, "c:dDfF:M:p:Rs")) != -1) {
		switch (ch) {
		case 'c':
			conffile = optarg;
			break;
		case 'd':
			dflag++;
			break;
		case 'D':
			dflag += 2;
			break;
		case 'f':
			fflag = 1;
			break;
		case 'M':
			mcastif = optarg;
			break;
		case 'R':
			fprintf(stderr, "rtadvd: "
				"the -R option is currently ignored.\n");
			/* accept_rr = 1; */
			/* run anyway... */
			break;
		case 's':
			sflag = 1;
			break;
		case 'p':
			pidfilename = optarg;
			break;
		case 'F':
			dumpfilename = optarg;
			break;
		}
	}
	argc -= optind;
	argv += optind;
	if (argc == 0) {
		fprintf(stderr,
		    "usage: rtadvd [-dDfRs] [-c conffile] "
		    "[-F dumpfile] [-M ifname] "
		    "[-p pidfile] interfaces...\n");
		exit(1);
	}

	logopt = LOG_NDELAY | LOG_PID;
	if (fflag)
		logopt |= LOG_PERROR;
	openlog("rtadvd", logopt, LOG_DAEMON);

	/* set log level */
	if (dflag > 1)
		(void)setlogmask(LOG_UPTO(LOG_DEBUG));
	else if (dflag > 0)
		(void)setlogmask(LOG_UPTO(LOG_INFO));
	else
		(void)setlogmask(LOG_UPTO(LOG_ERR));

	/* timer initialization */
	rtadvd_timer_init();

#ifndef HAVE_ARC4RANDOM
	/* random value initialization */
#ifdef __FreeBSD__
	srandomdev();
#else
	srandom((u_long)time(NULL));
#endif
#endif
	/* get iflist block from kernel */
	init_iflist();
	ifl_names = argv;
	ifl_len = argc;

	for (i = 0; i < ifl_len; i++) {
		int idx;

		idx = if_nametoindex(ifl_names[i]);
		if (idx == 0) {
			syslog(LOG_INFO,
			    "<%s> interface %s not found."
			    "Ignored at this moment.", __func__, ifl_names[i]);
			continue;
		}
		error = getconfig(idx);
		if (error)
			syslog(LOG_INFO,
			    "<%s> invalid configuration for %s."
			    "Ignored at this moment.", __func__, ifl_names[i]);
	}

	pfh = pidfile_open(pidfilename, 0600, &otherpid);
	if (pfh == NULL) {
		if (errno == EEXIST)
			errx(1, "%s already running, pid: %d",
			    getprogname(), otherpid);
		syslog(LOG_ERR,
		    "<%s> failed to open the pid log file, run anyway.",
		    __func__);
	}

	if (!fflag)
		daemon(1, 0);

	sock_open();

	/* record the current PID */
	pid = getpid();
	pidfile_write(pfh);

#ifdef HAVE_POLL_H
	set[0].fd = sock;
	set[0].events = POLLIN;
	if (sflag == 0) {
		rtsock_open();
		set[1].fd = rtsock;
		set[1].events = POLLIN;
	} else
		set[1].fd = -1;
#else
	maxfd = sock;
	if (sflag == 0) {
		rtsock_open();
		if (rtsock > sock)
			maxfd = rtsock;
	} else
		rtsock = -1;

	fdmasks = howmany(maxfd + 1, NFDBITS) * sizeof(fd_mask);
	if ((fdsetp = malloc(fdmasks)) == NULL) {
		err(1, "malloc");
		/*NOTREACHED*/
	}
	if ((selectfdp = malloc(fdmasks)) == NULL) {
		err(1, "malloc");
		/*NOTREACHED*/
	}
	memset(fdsetp, 0, fdmasks);
	FD_SET(sock, fdsetp);
	if (rtsock >= 0)
		FD_SET(rtsock, fdsetp);
#endif
	signal(SIGTERM, set_die);
	signal(SIGUSR1, rtadvd_set_dump_file);

	while (1) {
#ifndef HAVE_POLL_H
		memcpy(selectfdp, fdsetp, fdmasks); /* reinitialize */
#endif
		if (do_dump) {	/* SIGUSR1 */
			do_dump = 0;
			rtadvd_dump_file(dumpfilename);
		}

		if (do_die) {
			die();
			/*NOTREACHED*/
		}

		/* timer expiration check and reset the timer */
		timeout = rtadvd_check_timer();

		if (timeout != NULL) {
			syslog(LOG_DEBUG,
			    "<%s> set timer to %ld:%ld. waiting for "
			    "inputs or timeout", __func__,
			    (long int)timeout->tv_sec,
			    (long int)timeout->tv_usec);
		} else {
			syslog(LOG_DEBUG,
			    "<%s> there's no timer. waiting for inputs",
			    __func__);
		}
#ifdef HAVE_POLL_H
		if ((i = poll(set, 2, timeout ? (timeout->tv_sec * 1000 +
		    timeout->tv_usec / 1000) : INFTIM)) < 0)
#else
		if ((i = select(maxfd + 1, selectfdp, NULL, NULL,
		    timeout)) < 0)
#endif
		{
			/* EINTR would occur upon SIGUSR1 for status dump */
			if (errno != EINTR)
				syslog(LOG_ERR, "<%s> select: %s",
				    __func__, strerror(errno));
			continue;
		}
		if (i == 0)	/* timeout */
			continue;
#ifdef HAVE_POLL_H
		if (rtsock != -1 && set[1].revents & POLLIN)
#else
		if (rtsock != -1 && FD_ISSET(rtsock, selectfdp))
#endif
			rtmsg_input();
#ifdef HAVE_POLL_H
		if (set[0].revents & POLLIN)
#else
		if (FD_ISSET(sock, selectfdp))
#endif
			rtadvd_input();
	}
	exit(0);		/* NOTREACHED */
}

static int
ifl_lookup(char *ifn, char **names, int len)
{
	while (len--)
		if (strncmp(names[len], ifn, IFNAMSIZ) == 0)
			return (0);
	return (-1);
}

static void
rtadvd_set_dump_file(int sig __unused)
{

	do_dump = 1;
}

static void
set_die(int sig __unused)
{

	do_die = 1;
}

static void
die(void)
{
	struct rainfo *rai;
	struct rdnss *rdn;
	struct dnssl *dns;
	int i;
	const int retrans = MAX_FINAL_RTR_ADVERTISEMENTS;

	syslog(LOG_DEBUG, "<%s> cease to be an advertising router\n",
	    __func__);

	TAILQ_FOREACH(rai, &railist, rai_next) {
		rai->rai_lifetime = 0;
		TAILQ_FOREACH(rdn, &rai->rai_rdnss, rd_next)
			rdn->rd_ltime = 0;
		TAILQ_FOREACH(dns, &rai->rai_dnssl, dn_next)
			dns->dn_ltime = 0;
		make_packet(rai);
	}
	for (i = 0; i < retrans; i++) {
		TAILQ_FOREACH(rai, &railist, rai_next)
			ra_output(rai);
		sleep(MIN_DELAY_BETWEEN_RAS);
	}
	pidfile_remove(pfh);

	exit(0);
}

static void
rtmsg_input(void)
{
	int n, type, ifindex = 0, plen;
	size_t len;
	char msg[2048], *next, *lim;
	u_char ifname[IFNAMSIZ];
	struct if_announcemsghdr *ifan;
	struct prefix *pfx;
	struct rainfo *rai;
	struct in6_addr *addr;
	char addrbuf[INET6_ADDRSTRLEN];
	int prefixchange = 0;
	int error;

	n = read(rtsock, msg, sizeof(msg));
	syslog(LOG_DEBUG, "<%s> received a routing message "
	    "(type = %d, len = %d)", __func__, rtmsg_type(msg), n);

	if (n > rtmsg_len(msg)) {
		/*
		 * This usually won't happen for messages received on
		 * a routing socket.
		 */
		syslog(LOG_DEBUG,
		    "<%s> received data length is larger than "
		    "1st routing message len. multiple messages? "
		    "read %d bytes, but 1st msg len = %d",
		    __func__, n, rtmsg_len(msg));
#if 0
		/* adjust length */
		n = rtmsg_len(msg);
#endif
	}

	lim = msg + n;
	for (next = msg; next < lim; next += len) {
		int oldifflags;

		next = get_next_msg(next, lim, 0, &len,
		    RTADV_TYPE2BITMASK(RTM_ADD) |
		    RTADV_TYPE2BITMASK(RTM_DELETE) |
		    RTADV_TYPE2BITMASK(RTM_NEWADDR) |
		    RTADV_TYPE2BITMASK(RTM_DELADDR) |
		    RTADV_TYPE2BITMASK(RTM_IFINFO) |
		    RTADV_TYPE2BITMASK(RTM_IFANNOUNCE));
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
		case RTM_IFANNOUNCE:
			ifan = (struct if_announcemsghdr *)next;
			switch (ifan->ifan_what) {
			case IFAN_ARRIVAL:
			case IFAN_DEPARTURE:
				break;
			default:
				syslog(LOG_DEBUG,
				    "<%s:%d> unknown ifan msg (ifan_what=%d)",
				   __func__, __LINE__, ifan->ifan_what);
				continue;
			}

			syslog(LOG_INFO, "<%s>: if_announcemsg (idx=%d:%d)",
			       __func__, ifan->ifan_index, ifan->ifan_what);
			init_iflist();
			error = ifl_lookup(ifan->ifan_name,
			    ifl_names, ifl_len);
			if (error) {
				syslog(LOG_INFO, "<%s>: not a target "
				    "interface (idx=%d)", __func__,
				    ifan->ifan_index);
				continue;
			}

			switch (ifan->ifan_what) {
			case IFAN_ARRIVAL:
				error = getconfig(ifan->ifan_index);
				if (error)
					syslog(LOG_ERR,
					    "<%s>: getconfig failed (idx=%d)"
					    "  Ignored.", __func__,
					    ifan->ifan_index);
				break;
			case IFAN_DEPARTURE:
				error = rmconfig(ifan->ifan_index);
				if (error)
					syslog(LOG_ERR,
					    "<%s>: rmconfig failed (idx=%d)"
					    "  Ignored.", __func__,
					    ifan->ifan_index);
				break;
			}
			continue;
		default:
			/* should not reach here */
			syslog(LOG_DEBUG,
			       "<%s:%d> unknown rtmsg %d on %s",
			       __func__, __LINE__, type,
			       if_indextoname(ifindex, ifname));
			continue;
		}

		if ((rai = if_indextorainfo(ifindex)) == NULL) {
			syslog(LOG_DEBUG,
			       "<%s> route changed on "
			       "non advertising interface(%s)",
			       __func__,
			       if_indextoname(ifindex, ifname));
			continue;
		}
		oldifflags = iflist[ifindex]->ifm_flags;

		switch (type) {
		case RTM_ADD:
			/* init ifflags because it may have changed */
			iflist[ifindex]->ifm_flags =
			    if_getflags(ifindex, iflist[ifindex]->ifm_flags);

			if (sflag)
				break;	/* we aren't interested in prefixes  */

			addr = get_addr(msg);
			plen = get_prefixlen(msg);
			/* sanity check for plen */
			/* as RFC2373, prefixlen is at least 4 */
			if (plen < 4 || plen > 127) {
				syslog(LOG_INFO, "<%s> new interface route's"
				    "plen %d is invalid for a prefix",
				    __func__, plen);
				break;
			}
			pfx = find_prefix(rai, addr, plen);
			if (pfx) {
				if (pfx->pfx_timer) {
					/*
					 * If the prefix has been invalidated,
					 * make it available again.
					 */
					update_prefix(pfx);
					prefixchange = 1;
				} else
					syslog(LOG_DEBUG,
					    "<%s> new prefix(%s/%d) "
					    "added on %s, "
					    "but it was already in list",
					    __func__,
					    inet_ntop(AF_INET6, addr,
						(char *)addrbuf,
						sizeof(addrbuf)),
					    plen, rai->rai_ifname);
				break;
			}
			make_prefix(rai, ifindex, addr, plen);
			prefixchange = 1;
			break;
		case RTM_DELETE:
			/* init ifflags because it may have changed */
			iflist[ifindex]->ifm_flags =
			    if_getflags(ifindex, iflist[ifindex]->ifm_flags);

			if (sflag)
				break;

			addr = get_addr(msg);
			plen = get_prefixlen(msg);
			/* sanity check for plen */
			/* as RFC2373, prefixlen is at least 4 */
			if (plen < 4 || plen > 127) {
				syslog(LOG_INFO,
				    "<%s> deleted interface route's "
				    "plen %d is invalid for a prefix",
				    __func__, plen);
				break;
			}
			pfx = find_prefix(rai, addr, plen);
			if (pfx == NULL) {
				syslog(LOG_DEBUG,
				    "<%s> prefix(%s/%d) was deleted on %s, "
				    "but it was not in list",
				    __func__, inet_ntop(AF_INET6, addr,
					(char *)addrbuf, sizeof(addrbuf)),
					plen, rai->rai_ifname);
				break;
			}
			invalidate_prefix(pfx);
			prefixchange = 1;
			break;
		case RTM_NEWADDR:
		case RTM_DELADDR:
			/* init ifflags because it may have changed */
			iflist[ifindex]->ifm_flags =
			    if_getflags(ifindex, iflist[ifindex]->ifm_flags);
			break;
		case RTM_IFINFO:
			iflist[ifindex]->ifm_flags = get_ifm_flags(next);
			break;
		default:
			/* should not reach here */
			syslog(LOG_DEBUG,
			    "<%s:%d> unknown rtmsg %d on %s",
			    __func__, __LINE__, type,
			    if_indextoname(ifindex, ifname));
			return;
		}

		/* check if an interface flag is changed */
		if ((oldifflags & IFF_UP) && /* UP to DOWN */
		    !(iflist[ifindex]->ifm_flags & IFF_UP)) {
			syslog(LOG_INFO,
			    "<%s> interface %s becomes down. stop timer.",
			    __func__, rai->rai_ifname);
			rtadvd_remove_timer(rai->rai_timer);
			rai->rai_timer = NULL;
		} else if (!(oldifflags & IFF_UP) && /* DOWN to UP */
		    (iflist[ifindex]->ifm_flags & IFF_UP)) {
			syslog(LOG_INFO,
			    "<%s> interface %s becomes up. restart timer.",
			    __func__, rai->rai_ifname);

			rai->rai_initcounter = 0; /* reset the counter */
			rai->rai_waiting = 0; /* XXX */
			rai->rai_timer = rtadvd_add_timer(ra_timeout,
			    ra_timer_update, rai, rai);
			ra_timer_update(rai, &rai->rai_timer->rat_tm);
			rtadvd_set_timer(&rai->rai_timer->rat_tm,
			    rai->rai_timer);
		} else if (prefixchange &&
		    (iflist[ifindex]->ifm_flags & IFF_UP)) {
			/*
			 * An advertised prefix has been added or invalidated.
			 * Will notice the change in a short delay.
			 */
			rai->rai_initcounter = 0;
			set_short_delay(rai);
		}
	}

	return;
}

void
rtadvd_input(void)
{
	ssize_t i;
	int *hlimp = NULL;
#ifdef OLDRAWSOCKET
	struct ip6_hdr *ip;
#endif
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
	rcvmhdr.msg_controllen = rcvcmsgbuflen;
	if ((i = recvmsg(sock, &rcvmhdr, 0)) < 0)
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
		    __func__);
		return;
	}
	if (hlimp == NULL) {
		syslog(LOG_ERR,
		    "<%s> failed to get receiving hop limit",
		    __func__);
		return;
	}

	/*
	 * If we happen to receive data on an interface which is now gone
	 * or down, just discard the data.
	 */
	if (iflist[pi->ipi6_ifindex] == NULL ||
	    (iflist[pi->ipi6_ifindex]->ifm_flags & IFF_UP) == 0) {
		syslog(LOG_INFO,
		    "<%s> received data on a disabled interface (%s)",
		    __func__,
		    (iflist[pi->ipi6_ifindex] == NULL) ? "[gone]" :
		    if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		return;
	}

#ifdef OLDRAWSOCKET
	if ((size_t)i < sizeof(struct ip6_hdr) + sizeof(struct icmp6_hdr)) {
		syslog(LOG_ERR,
		    "<%s> packet size(%d) is too short",
		    __func__, i);
		return;
	}

	ip = (struct ip6_hdr *)rcvmhdr.msg_iov[0].iov_base;
	icp = (struct icmp6_hdr *)(ip + 1); /* XXX: ext. hdr? */
#else
	if ((size_t)i < sizeof(struct icmp6_hdr)) {
		syslog(LOG_ERR,
		    "<%s> packet size(%zd) is too short",
		    __func__, i);
		return;
	}

	icp = (struct icmp6_hdr *)rcvmhdr.msg_iov[0].iov_base;
#endif

	switch (icp->icmp6_type) {
	case ND_ROUTER_SOLICIT:
		/*
		 * Message verification - RFC 4861 6.1.1
		 * XXX: these checks must be done in the kernel as well,
		 *      but we can't completely rely on them.
		 */
		if (*hlimp != 255) {
			syslog(LOG_NOTICE,
			    "<%s> RS with invalid hop limit(%d) "
			    "received from %s on %s",
			    __func__, *hlimp,
			    inet_ntop(AF_INET6, &rcvfrom.sin6_addr, ntopbuf,
			    sizeof(ntopbuf)),
			    if_indextoname(pi->ipi6_ifindex, ifnamebuf));
			return;
		}
		if (icp->icmp6_code) {
			syslog(LOG_NOTICE,
			    "<%s> RS with invalid ICMP6 code(%d) "
			    "received from %s on %s",
			    __func__, icp->icmp6_code,
			    inet_ntop(AF_INET6, &rcvfrom.sin6_addr, ntopbuf,
			    sizeof(ntopbuf)),
			    if_indextoname(pi->ipi6_ifindex, ifnamebuf));
			return;
		}
		if ((size_t)i < sizeof(struct nd_router_solicit)) {
			syslog(LOG_NOTICE,
			    "<%s> RS from %s on %s does not have enough "
			    "length (len = %zd)",
			    __func__,
			    inet_ntop(AF_INET6, &rcvfrom.sin6_addr, ntopbuf,
			    sizeof(ntopbuf)),
			    if_indextoname(pi->ipi6_ifindex, ifnamebuf), i);
			return;
		}
		rs_input(i, (struct nd_router_solicit *)icp, pi, &rcvfrom);
		break;
	case ND_ROUTER_ADVERT:
		/*
		 * Message verification - RFC 4861 6.1.2
		 * XXX: there's the same dilemma as above...
		 */
		if (!IN6_IS_ADDR_LINKLOCAL(&rcvfrom.sin6_addr)) {
			syslog(LOG_NOTICE,
			    "<%s> RA witn non-linklocal source address "
			    "received from %s on %s",
			    __func__, inet_ntop(AF_INET6, &rcvfrom.sin6_addr,
			    ntopbuf, sizeof(ntopbuf)),
			    if_indextoname(pi->ipi6_ifindex, ifnamebuf));
			return;
		}
		if (*hlimp != 255) {
			syslog(LOG_NOTICE,
			    "<%s> RA with invalid hop limit(%d) "
			    "received from %s on %s",
			    __func__, *hlimp,
			    inet_ntop(AF_INET6, &rcvfrom.sin6_addr, ntopbuf,
			    sizeof(ntopbuf)),
			    if_indextoname(pi->ipi6_ifindex, ifnamebuf));
			return;
		}
		if (icp->icmp6_code) {
			syslog(LOG_NOTICE,
			    "<%s> RA with invalid ICMP6 code(%d) "
			    "received from %s on %s",
			    __func__, icp->icmp6_code,
			    inet_ntop(AF_INET6, &rcvfrom.sin6_addr, ntopbuf,
			    sizeof(ntopbuf)),
			    if_indextoname(pi->ipi6_ifindex, ifnamebuf));
			return;
		}
		if ((size_t)i < sizeof(struct nd_router_advert)) {
			syslog(LOG_NOTICE,
			    "<%s> RA from %s on %s does not have enough "
			    "length (len = %zd)",
			    __func__,
			    inet_ntop(AF_INET6, &rcvfrom.sin6_addr, ntopbuf,
			    sizeof(ntopbuf)),
			    if_indextoname(pi->ipi6_ifindex, ifnamebuf), i);
			return;
		}
		ra_input(i, (struct nd_router_advert *)icp, pi, &rcvfrom);
		break;
	case ICMP6_ROUTER_RENUMBERING:
		if (accept_rr == 0) {
			syslog(LOG_ERR, "<%s> received a router renumbering "
			    "message, but not allowed to be accepted",
			    __func__);
			break;
		}
		rr_input(i, (struct icmp6_router_renum *)icp, pi, &rcvfrom,
		    &dst);
		break;
	default:
		/*
		 * Note that this case is POSSIBLE, especially just
		 * after invocation of the daemon. This is because we
		 * could receive message after opening the socket and
		 * before setting ICMP6 type filter(see sock_open()).
		 */
		syslog(LOG_ERR, "<%s> invalid icmp type(%d)",
		    __func__, icp->icmp6_type);
		return;
	}

	return;
}

static void
rs_input(int len, struct nd_router_solicit *rs,
	 struct in6_pktinfo *pi, struct sockaddr_in6 *from)
{
	u_char ntopbuf[INET6_ADDRSTRLEN];
	u_char ifnamebuf[IFNAMSIZ];
	union nd_opt ndopts;
	struct rainfo *rai;
	struct soliciter *sol;

	syslog(LOG_DEBUG,
	    "<%s> RS received from %s on %s",
	    __func__,
	    inet_ntop(AF_INET6, &from->sin6_addr, ntopbuf, sizeof(ntopbuf)),
	    if_indextoname(pi->ipi6_ifindex, ifnamebuf));

	/* ND option check */
	memset(&ndopts, 0, sizeof(ndopts));
	if (nd6_options((struct nd_opt_hdr *)(rs + 1),
			len - sizeof(struct nd_router_solicit),
			&ndopts, NDOPT_FLAG_SRCLINKADDR)) {
		syslog(LOG_INFO,
		    "<%s> ND option check failed for an RS from %s on %s",
		    __func__,
		    inet_ntop(AF_INET6, &from->sin6_addr, ntopbuf,
			sizeof(ntopbuf)),
		    if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		return;
	}

	/*
	 * If the IP source address is the unspecified address, there
	 * must be no source link-layer address option in the message.
	 * (RFC 4861 6.1.1)
	 */
	if (IN6_IS_ADDR_UNSPECIFIED(&from->sin6_addr) &&
	    ndopts.opt_src_lladdr) {
		syslog(LOG_INFO,
		    "<%s> RS from unspecified src on %s has a link-layer"
		    " address option",
		    __func__, if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		goto done;
	}

	TAILQ_FOREACH(rai, &railist, rai_next)
		if (pi->ipi6_ifindex == (unsigned int)rai->rai_ifindex)
			break;

	if (rai == NULL) {
		syslog(LOG_INFO,
		       "<%s> RS received on non advertising interface(%s)",
		       __func__,
		       if_indextoname(pi->ipi6_ifindex, ifnamebuf));
		goto done;
	}

	rai->rai_rsinput++;		/* increment statistics */

	/*
	 * Decide whether to send RA according to the rate-limit
	 * consideration.
	 */

	/* record sockaddr waiting for RA, if possible */
	sol = (struct soliciter *)malloc(sizeof(*sol));
	if (sol) {
		sol->sol_addr = *from;
		/* XXX RFC 2553 need clarification on flowinfo */
		sol->sol_addr.sin6_flowinfo = 0;
		TAILQ_INSERT_TAIL(&rai->rai_soliciter, sol, sol_next);
	}

	/*
	 * If there is already a waiting RS packet, don't
	 * update the timer.
	 */
	if (rai->rai_waiting++)
		goto done;

	set_short_delay(rai);

  done:
	free_ndopts(&ndopts);
	return;
}

static void
set_short_delay(struct rainfo *rai)
{
	long delay;	/* must not be greater than 1000000 */
	struct timeval interval, now, min_delay, tm_tmp, *rest;

	/*
	 * Compute a random delay. If the computed value
	 * corresponds to a time later than the time the next
	 * multicast RA is scheduled to be sent, ignore the random
	 * delay and send the advertisement at the
	 * already-scheduled time. RFC 4861 6.2.6
	 */
#ifdef HAVE_ARC4RANDOM
	delay = arc4random_uniform(MAX_RA_DELAY_TIME);
#else
	delay = random() % MAX_RA_DELAY_TIME;
#endif
	interval.tv_sec = 0;
	interval.tv_usec = delay;
	rest = rtadvd_timer_rest(rai->rai_timer);
	if (TIMEVAL_LT(rest, &interval)) {
		syslog(LOG_DEBUG, "<%s> random delay is larger than "
		    "the rest of the current timer", __func__);
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
	TIMEVAL_SUB(&now, &rai->rai_lastsent, &tm_tmp);
	min_delay.tv_sec = MIN_DELAY_BETWEEN_RAS;
	min_delay.tv_usec = 0;
	if (TIMEVAL_LT(&tm_tmp, &min_delay)) {
		TIMEVAL_SUB(&min_delay, &tm_tmp, &min_delay);
		TIMEVAL_ADD(&min_delay, &interval, &interval);
	}
	rtadvd_set_timer(&interval, rai->rai_timer);
}

static int
check_accept_rtadv(int idx)
{
	struct in6_ndireq nd;
	u_char ifname[IFNAMSIZ];
	int s6;
	int error;

	if ((s6 = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		syslog(LOG_ERR,
		    "<%s> open socket failed for idx=%d.",
		    __func__, idx);
		return (0);
	}
	if ((if_indextoname(idx, ifname)) == NULL) {
		syslog(LOG_ERR,
		    "<%s> ifindex->ifname failed (idx=%d).",
		    __func__, idx);
		close(s6);
		return (0);
	}
	memset(&nd, 0, sizeof(nd));
	strncpy(nd.ifname, ifname, sizeof(nd.ifname));
	error = ioctl(s6, SIOCGIFINFO_IN6, &nd);
	if (error) {
		syslog(LOG_ERR,
		    "<%s> ioctl(SIOCGIFINFO_IN6) failed for idx=%d.",
		    __func__, idx);
		nd.ndi.flags = 0;
	}
	close(s6);

	return (nd.ndi.flags & ND6_IFF_ACCEPT_RTADV);
}

static int
getinet6sysctl(int code)
{
	int mib[] = { CTL_NET, PF_INET6, IPPROTO_IPV6, 0 };
	int value;
	size_t size;

	mib[3] = code;
	size = sizeof(value);
	if (sysctl(mib, sizeof(mib)/sizeof(mib[0]), &value, &size, NULL, 0)
	    < 0) {
		syslog(LOG_ERR, "<%s>: failed to get ip6 sysctl(%d): %s",
		    __func__, code,
		    strerror(errno));
		return (-1);
	}
	else
		return (value);
}

static void
ra_input(int len, struct nd_router_advert *nra,
	 struct in6_pktinfo *pi, struct sockaddr_in6 *from)
{
	struct rainfo *rai;
	u_char ntopbuf[INET6_ADDRSTRLEN];
	u_char ifnamebuf[IFNAMSIZ];
	union nd_opt ndopts;
	const char *on_off[] = {"OFF", "ON"};
	u_int32_t reachabletime, retranstimer, mtu;
	int inconsistent = 0;
	int error;

	syslog(LOG_DEBUG, "<%s> RA received from %s on %s", __func__,
	    inet_ntop(AF_INET6, &from->sin6_addr, ntopbuf, sizeof(ntopbuf)),
	    if_indextoname(pi->ipi6_ifindex, ifnamebuf));

	if (!check_accept_rtadv(pi->ipi6_ifindex)) {
		syslog(LOG_INFO,
		    "<%s> An RA from %s on %s ignored (no ACCEPT_RTADV flag).",
		    __func__,
		    inet_ntop(AF_INET6, &from->sin6_addr, ntopbuf,
			sizeof(ntopbuf)), if_indextoname(pi->ipi6_ifindex,
			ifnamebuf));
		return;
	}

	/* ND option check */
	memset(&ndopts, 0, sizeof(ndopts));
	error = nd6_options((struct nd_opt_hdr *)(nra + 1),
	    len - sizeof(struct nd_router_advert), &ndopts,
	    NDOPT_FLAG_SRCLINKADDR | NDOPT_FLAG_PREFIXINFO | NDOPT_FLAG_MTU |
	    NDOPT_FLAG_RDNSS | NDOPT_FLAG_DNSSL);
	if (error) {
		syslog(LOG_INFO,
		    "<%s> ND option check failed for an RA from %s on %s",
		    __func__,
		    inet_ntop(AF_INET6, &from->sin6_addr, ntopbuf,
			sizeof(ntopbuf)), if_indextoname(pi->ipi6_ifindex,
			ifnamebuf));
		return;
	}

	/*
	 * RA consistency check according to RFC 4861 6.2.7
	 */
	rai = if_indextorainfo(pi->ipi6_ifindex);
	if (rai == NULL) {
		syslog(LOG_INFO,
		    "<%s> received RA from %s on non-advertising"
		    " interface(%s)",
		    __func__,
		    inet_ntop(AF_INET6, &from->sin6_addr, ntopbuf,
			sizeof(ntopbuf)), if_indextoname(pi->ipi6_ifindex,
			ifnamebuf));
		goto done;
	}
	rai->rai_rainput++;		/* increment statistics */

	/* Cur Hop Limit value */
	if (nra->nd_ra_curhoplimit && rai->rai_hoplimit &&
	    nra->nd_ra_curhoplimit != rai->rai_hoplimit) {
		syslog(LOG_INFO,
		    "<%s> CurHopLimit inconsistent on %s:"
		    " %d from %s, %d from us",
		    __func__, rai->rai_ifname, nra->nd_ra_curhoplimit,
		    inet_ntop(AF_INET6, &from->sin6_addr, ntopbuf,
			sizeof(ntopbuf)), rai->rai_hoplimit);
		inconsistent++;
	}
	/* M flag */
	if ((nra->nd_ra_flags_reserved & ND_RA_FLAG_MANAGED) !=
	    rai->rai_managedflg) {
		syslog(LOG_INFO,
		    "<%s> M flag inconsistent on %s:"
		    " %s from %s, %s from us",
		    __func__, rai->rai_ifname, on_off[!rai->rai_managedflg],
		    inet_ntop(AF_INET6, &from->sin6_addr, ntopbuf,
			sizeof(ntopbuf)), on_off[rai->rai_managedflg]);
		inconsistent++;
	}
	/* O flag */
	if ((nra->nd_ra_flags_reserved & ND_RA_FLAG_OTHER) !=
	    rai->rai_otherflg) {
		syslog(LOG_INFO,
		    "<%s> O flag inconsistent on %s:"
		    " %s from %s, %s from us",
		    __func__, rai->rai_ifname, on_off[!rai->rai_otherflg],
		    inet_ntop(AF_INET6, &from->sin6_addr, ntopbuf,
			sizeof(ntopbuf)), on_off[rai->rai_otherflg]);
		inconsistent++;
	}
	/* Reachable Time */
	reachabletime = ntohl(nra->nd_ra_reachable);
	if (reachabletime && rai->rai_reachabletime &&
	    reachabletime != rai->rai_reachabletime) {
		syslog(LOG_INFO,
		    "<%s> ReachableTime inconsistent on %s:"
		    " %d from %s, %d from us",
		    __func__, rai->rai_ifname, reachabletime,
		    inet_ntop(AF_INET6, &from->sin6_addr, ntopbuf,
			sizeof(ntopbuf)), rai->rai_reachabletime);
		inconsistent++;
	}
	/* Retrans Timer */
	retranstimer = ntohl(nra->nd_ra_retransmit);
	if (retranstimer && rai->rai_retranstimer &&
	    retranstimer != rai->rai_retranstimer) {
		syslog(LOG_INFO,
		    "<%s> RetranceTimer inconsistent on %s:"
		    " %d from %s, %d from us",
		    __func__, rai->rai_ifname, retranstimer,
		    inet_ntop(AF_INET6, &from->sin6_addr, ntopbuf,
			sizeof(ntopbuf)), rai->rai_retranstimer);
		inconsistent++;
	}
	/* Values in the MTU options */
	if (ndopts.opt_mtu) {
		mtu = ntohl(ndopts.opt_mtu->nd_opt_mtu_mtu);
		if (mtu && rai->rai_linkmtu && mtu != rai->rai_linkmtu) {
			syslog(LOG_INFO,
			    "<%s> MTU option value inconsistent on %s:"
			    " %d from %s, %d from us",
			    __func__, rai->rai_ifname, mtu,
			    inet_ntop(AF_INET6, &from->sin6_addr, ntopbuf,
				sizeof(ntopbuf)), rai->rai_linkmtu);
			inconsistent++;
		}
	}
	/* Preferred and Valid Lifetimes for prefixes */
	{
		struct nd_optlist *nol;

		if (ndopts.opt_pi)
			if (prefix_check(ndopts.opt_pi, rai, from))
				inconsistent++;

		TAILQ_FOREACH(nol, &ndopts.opt_list, nol_next)
			if (prefix_check((struct nd_opt_prefix_info *)nol->nol_opt,
				rai, from))
				inconsistent++;
	}

	if (inconsistent)
		rai->rai_rainconsistent++;

  done:
	free_ndopts(&ndopts);
	return;
}

/* return a non-zero value if the received prefix is inconsitent with ours */
static int
prefix_check(struct nd_opt_prefix_info *pinfo,
	struct rainfo *rai, struct sockaddr_in6 *from)
{
	u_int32_t preferred_time, valid_time;
	struct prefix *pfx;
	int inconsistent = 0;
	u_char ntopbuf[INET6_ADDRSTRLEN];
	u_char prefixbuf[INET6_ADDRSTRLEN];
	struct timeval now;

#if 0				/* impossible */
	if (pinfo->nd_opt_pi_type != ND_OPT_PREFIX_INFORMATION)
		return (0);
#endif

	/*
	 * log if the adveritsed prefix has link-local scope(sanity check?)
	 */
	if (IN6_IS_ADDR_LINKLOCAL(&pinfo->nd_opt_pi_prefix))
		syslog(LOG_INFO,
		    "<%s> link-local prefix %s/%d is advertised "
		    "from %s on %s",
		    __func__,
		    inet_ntop(AF_INET6, &pinfo->nd_opt_pi_prefix, prefixbuf,
			sizeof(prefixbuf)),
		    pinfo->nd_opt_pi_prefix_len,
		    inet_ntop(AF_INET6, &from->sin6_addr, ntopbuf,
			sizeof(ntopbuf)), rai->rai_ifname);

	if ((pfx = find_prefix(rai, &pinfo->nd_opt_pi_prefix,
		pinfo->nd_opt_pi_prefix_len)) == NULL) {
		syslog(LOG_INFO,
		    "<%s> prefix %s/%d from %s on %s is not in our list",
		    __func__,
		    inet_ntop(AF_INET6, &pinfo->nd_opt_pi_prefix, prefixbuf,
			sizeof(prefixbuf)),
		    pinfo->nd_opt_pi_prefix_len,
		    inet_ntop(AF_INET6, &from->sin6_addr, ntopbuf,
			sizeof(ntopbuf)), rai->rai_ifname);
		return (0);
	}

	preferred_time = ntohl(pinfo->nd_opt_pi_preferred_time);
	if (pfx->pfx_pltimeexpire) {
		/*
		 * The lifetime is decremented in real time, so we should
		 * compare the expiration time.
		 * (RFC 2461 Section 6.2.7.)
		 * XXX: can we really expect that all routers on the link
		 * have synchronized clocks?
		 */
		gettimeofday(&now, NULL);
		preferred_time += now.tv_sec;

		if (!pfx->pfx_timer && rai->rai_clockskew &&
		    abs(preferred_time - pfx->pfx_pltimeexpire) > rai->rai_clockskew) {
			syslog(LOG_INFO,
			    "<%s> preferred lifetime for %s/%d"
			    " (decr. in real time) inconsistent on %s:"
			    " %d from %s, %ld from us",
			    __func__,
			    inet_ntop(AF_INET6, &pinfo->nd_opt_pi_prefix, prefixbuf,
				sizeof(prefixbuf)),
			    pinfo->nd_opt_pi_prefix_len,
			    rai->rai_ifname, preferred_time,
			    inet_ntop(AF_INET6, &from->sin6_addr, ntopbuf,
				sizeof(ntopbuf)), pfx->pfx_pltimeexpire);
			inconsistent++;
		}
	} else if (!pfx->pfx_timer && preferred_time != pfx->pfx_preflifetime)
		syslog(LOG_INFO,
		    "<%s> preferred lifetime for %s/%d"
		    " inconsistent on %s:"
		    " %d from %s, %d from us",
		    __func__,
		    inet_ntop(AF_INET6, &pinfo->nd_opt_pi_prefix, prefixbuf,
			sizeof(prefixbuf)),
		    pinfo->nd_opt_pi_prefix_len,
		    rai->rai_ifname, preferred_time,
		    inet_ntop(AF_INET6, &from->sin6_addr, ntopbuf,
			sizeof(ntopbuf)), pfx->pfx_preflifetime);

	valid_time = ntohl(pinfo->nd_opt_pi_valid_time);
	if (pfx->pfx_vltimeexpire) {
		gettimeofday(&now, NULL);
		valid_time += now.tv_sec;

		if (!pfx->pfx_timer && rai->rai_clockskew &&
		    abs(valid_time - pfx->pfx_vltimeexpire) > rai->rai_clockskew) {
			syslog(LOG_INFO,
			    "<%s> valid lifetime for %s/%d"
			    " (decr. in real time) inconsistent on %s:"
			    " %d from %s, %ld from us",
			    __func__,
			    inet_ntop(AF_INET6, &pinfo->nd_opt_pi_prefix, prefixbuf,
				sizeof(prefixbuf)),
			    pinfo->nd_opt_pi_prefix_len,
			    rai->rai_ifname, preferred_time,
			    inet_ntop(AF_INET6, &from->sin6_addr, ntopbuf,
				sizeof(ntopbuf)), pfx->pfx_vltimeexpire);
			inconsistent++;
		}
	} else if (!pfx->pfx_timer && valid_time != pfx->pfx_validlifetime) {
		syslog(LOG_INFO,
		    "<%s> valid lifetime for %s/%d"
		    " inconsistent on %s:"
		    " %d from %s, %d from us",
		    __func__,
		    inet_ntop(AF_INET6, &pinfo->nd_opt_pi_prefix, prefixbuf,
			sizeof(prefixbuf)),
		    pinfo->nd_opt_pi_prefix_len,
		    rai->rai_ifname, valid_time,
		    inet_ntop(AF_INET6, &from->sin6_addr, ntopbuf,
			sizeof(ntopbuf)), pfx->pfx_validlifetime);
		inconsistent++;
	}

	return (inconsistent);
}

struct prefix *
find_prefix(struct rainfo *rai, struct in6_addr *prefix, int plen)
{
	struct prefix *pfx;
	int bytelen, bitlen;
	u_char bitmask;

	TAILQ_FOREACH(pfx, &rai->rai_prefix, pfx_next) {
		if (plen != pfx->pfx_prefixlen)
			continue;

		bytelen = plen / 8;
		bitlen = plen % 8;
		bitmask = 0xff << (8 - bitlen);

		if (memcmp((void *)prefix, (void *)&pfx->pfx_prefix, bytelen))
			continue;

		if (bitlen == 0 ||
		    ((prefix->s6_addr[bytelen] & bitmask) ==
		     (pfx->pfx_prefix.s6_addr[bytelen] & bitmask))) {
			return (pfx);
		}
	}

	return (NULL);
}

/* check if p0/plen0 matches p1/plen1; return 1 if matches, otherwise 0. */
int
prefix_match(struct in6_addr *p0, int plen0,
	struct in6_addr *p1, int plen1)
{
	int bytelen, bitlen;
	u_char bitmask;

	if (plen0 < plen1)
		return (0);

	bytelen = plen1 / 8;
	bitlen = plen1 % 8;
	bitmask = 0xff << (8 - bitlen);

	if (memcmp((void *)p0, (void *)p1, bytelen))
		return (0);

	if (bitlen == 0 ||
	    ((p0->s6_addr[bytelen] & bitmask) ==
	     (p1->s6_addr[bytelen] & bitmask))) {
		return (1);
	}

	return (0);
}

static int
nd6_options(struct nd_opt_hdr *hdr, int limit,
	union nd_opt *ndopts, u_int32_t optflags)
{
	int optlen = 0;

	for (; limit > 0; limit -= optlen) {
		if ((size_t)limit < sizeof(struct nd_opt_hdr)) {
			syslog(LOG_INFO, "<%s> short option header", __func__);
			goto bad;
		}

		hdr = (struct nd_opt_hdr *)((caddr_t)hdr + optlen);
		if (hdr->nd_opt_len == 0) {
			syslog(LOG_INFO,
			    "<%s> bad ND option length(0) (type = %d)",
			    __func__, hdr->nd_opt_type);
			goto bad;
		}
		optlen = hdr->nd_opt_len << 3;
		if (optlen > limit) {
			syslog(LOG_INFO, "<%s> short option", __func__);
			goto bad;
		}

		if (hdr->nd_opt_type > ND_OPT_MTU &&
		    hdr->nd_opt_type != ND_OPT_RDNSS &&
		    hdr->nd_opt_type != ND_OPT_DNSSL) {
			syslog(LOG_INFO, "<%s> unknown ND option(type %d)",
			    __func__, hdr->nd_opt_type);
			continue;
		}

		if ((ndopt_flags[hdr->nd_opt_type] & optflags) == 0) {
			syslog(LOG_INFO, "<%s> unexpected ND option(type %d)",
			    __func__, hdr->nd_opt_type);
			continue;
		}

		/*
		 * Option length check.  Do it here for all fixed-length
		 * options.
		 */
		switch (hdr->nd_opt_type) {
		case ND_OPT_MTU:
			if (optlen == sizeof(struct nd_opt_mtu))
				break;
			goto skip;
		case ND_OPT_RDNSS:
			if (optlen >= 24 &&
			    (optlen - sizeof(struct nd_opt_rdnss)) % 16 == 0)
				break;
			goto skip;
		case ND_OPT_DNSSL:
			if (optlen >= 16 &&
			    (optlen - sizeof(struct nd_opt_dnssl)) % 8 == 0)
				break;
			goto skip;
		case ND_OPT_PREFIX_INFORMATION:
			if (optlen == sizeof(struct nd_opt_prefix_info))
				break;
skip:
			syslog(LOG_INFO, "<%s> invalid option length",
			    __func__);
			continue;
		}

		switch (hdr->nd_opt_type) {
		case ND_OPT_TARGET_LINKADDR:
		case ND_OPT_REDIRECTED_HEADER:
		case ND_OPT_RDNSS:
		case ND_OPT_DNSSL:
			break;	/* we don't care about these options */
		case ND_OPT_SOURCE_LINKADDR:
		case ND_OPT_MTU:
			if (ndopts->opt_array[hdr->nd_opt_type]) {
				syslog(LOG_INFO,
				    "<%s> duplicated ND option (type = %d)",
				    __func__, hdr->nd_opt_type);
			}
			ndopts->opt_array[hdr->nd_opt_type] = hdr;
			break;
		case ND_OPT_PREFIX_INFORMATION:
		{
			struct nd_optlist *nol;

			if (ndopts->opt_pi == 0) {
				ndopts->opt_pi =
				    (struct nd_opt_prefix_info *)hdr;
				continue;
			}
			nol = malloc(sizeof(*nol));
			if (nol == NULL) {
				syslog(LOG_ERR, "<%s> can't allocate memory",
				    __func__);
				goto bad;
			}
			nol->nol_opt = hdr;
			TAILQ_INSERT_TAIL(&(ndopts->opt_list), nol, nol_next);

			break;
		}
		default:	/* impossible */
			break;
		}
	}

	return (0);

  bad:
	free_ndopts(ndopts);

	return (-1);
}

static void
free_ndopts(union nd_opt *ndopts)
{
	struct nd_optlist *nol;

	while ((nol = TAILQ_FIRST(&ndopts->opt_list)) != NULL) {
		TAILQ_REMOVE(&ndopts->opt_list, nol, nol_next);
		free(nol);
	}
}

void
sock_open(void)
{
	struct icmp6_filter filt;
	struct ipv6_mreq mreq;
	struct rainfo *rai;
	int on;
	/* XXX: should be max MTU attached to the node */
	static u_char answer[1500];

	rcvcmsgbuflen = CMSG_SPACE(sizeof(struct in6_pktinfo)) +
	    CMSG_SPACE(sizeof(int));
	rcvcmsgbuf = (u_char *)malloc(rcvcmsgbuflen);
	if (rcvcmsgbuf == NULL) {
		syslog(LOG_ERR, "<%s> not enough core", __func__);
		exit(1);
	}

	sndcmsgbuflen = CMSG_SPACE(sizeof(struct in6_pktinfo)) +
	    CMSG_SPACE(sizeof(int));
	sndcmsgbuf = (u_char *)malloc(sndcmsgbuflen);
	if (sndcmsgbuf == NULL) {
		syslog(LOG_ERR, "<%s> not enough core", __func__);
		exit(1);
	}

	if ((sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6)) < 0) {
		syslog(LOG_ERR, "<%s> socket: %s", __func__, strerror(errno));
		exit(1);
	}
	/* specify to tell receiving interface */
	on = 1;
	if (setsockopt(sock, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on,
	    sizeof(on)) < 0) {
		syslog(LOG_ERR, "<%s> IPV6_RECVPKTINFO: %s", __func__,
		    strerror(errno));
		exit(1);
	}
	on = 1;
	/* specify to tell value of hoplimit field of received IP6 hdr */
	if (setsockopt(sock, IPPROTO_IPV6, IPV6_RECVHOPLIMIT, &on,
		sizeof(on)) < 0) {
		syslog(LOG_ERR, "<%s> IPV6_RECVHOPLIMIT: %s", __func__,
		    strerror(errno));
		exit(1);
	}
	ICMP6_FILTER_SETBLOCKALL(&filt);
	ICMP6_FILTER_SETPASS(ND_ROUTER_SOLICIT, &filt);
	ICMP6_FILTER_SETPASS(ND_ROUTER_ADVERT, &filt);
	if (accept_rr)
		ICMP6_FILTER_SETPASS(ICMP6_ROUTER_RENUMBERING, &filt);

	if (setsockopt(sock, IPPROTO_ICMPV6, ICMP6_FILTER, &filt,
	    sizeof(filt)) < 0) {
		syslog(LOG_ERR, "<%s> IICMP6_FILTER: %s",
		    __func__, strerror(errno));
		exit(1);
	}

	/*
	 * join all routers multicast address on each advertising interface.
	 */
	memcpy(&mreq.ipv6mr_multiaddr.s6_addr,
	    &sin6_linklocal_allrouters.sin6_addr,
	    sizeof(mreq.ipv6mr_multiaddr.s6_addr));
	TAILQ_FOREACH(rai, &railist, rai_next) {
		mreq.ipv6mr_interface = rai->rai_ifindex;
		if (setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq,
		    sizeof(mreq)) < 0) {
			syslog(LOG_ERR, "<%s> IPV6_JOIN_GROUP(link) on %s: %s",
			    __func__, rai->rai_ifname, strerror(errno));
			exit(1);
		}
	}

	/*
	 * When attending router renumbering, join all-routers site-local
	 * multicast group.
	 */
	if (accept_rr) {
		memcpy(&mreq.ipv6mr_multiaddr.s6_addr,
		    &sin6_sitelocal_allrouters.sin6_addr,
		    sizeof(mreq.ipv6mr_multiaddr.s6_addr));
		if (mcastif) {
			if ((mreq.ipv6mr_interface = if_nametoindex(mcastif))
			    == 0) {
				syslog(LOG_ERR,
				    "<%s> invalid interface: %s",
				    __func__, mcastif);
				exit(1);
			}
		} else
			mreq.ipv6mr_interface =
			    TAILQ_FIRST(&railist)->rai_ifindex;
		if (setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP,
		    &mreq, sizeof(mreq)) < 0) {
			syslog(LOG_ERR,
			    "<%s> IPV6_JOIN_GROUP(site) on %s: %s", __func__,
			    mcastif ? mcastif :
				TAILQ_FIRST(&railist)->rai_ifname,
			    strerror(errno));
			exit(1);
		}
	}

	/* initialize msghdr for receiving packets */
	rcviov[0].iov_base = (caddr_t)answer;
	rcviov[0].iov_len = sizeof(answer);
	rcvmhdr.msg_name = (caddr_t)&rcvfrom;
	rcvmhdr.msg_namelen = sizeof(rcvfrom);
	rcvmhdr.msg_iov = rcviov;
	rcvmhdr.msg_iovlen = 1;
	rcvmhdr.msg_control = (caddr_t) rcvcmsgbuf;
	rcvmhdr.msg_controllen = rcvcmsgbuflen;

	/* initialize msghdr for sending packets */
	sndmhdr.msg_namelen = sizeof(struct sockaddr_in6);
	sndmhdr.msg_iov = sndiov;
	sndmhdr.msg_iovlen = 1;
	sndmhdr.msg_control = (caddr_t)sndcmsgbuf;
	sndmhdr.msg_controllen = sndcmsgbuflen;

	return;
}

/* open a routing socket to watch the routing table */
static void
rtsock_open(void)
{
	if ((rtsock = socket(PF_ROUTE, SOCK_RAW, 0)) < 0) {
		syslog(LOG_ERR,
		    "<%s> socket: %s", __func__, strerror(errno));
		exit(1);
	}
}

struct rainfo *
if_indextorainfo(int idx)
{
	struct rainfo *rai;

	TAILQ_FOREACH(rai, &railist, rai_next) {
		syslog(LOG_DEBUG, "<%s> rai->rai_ifindex %d == idx %d?",
		    __func__, rai->rai_ifindex, idx);
		if (rai->rai_ifindex == idx)
			return (rai);
	}

	return (NULL);		/* search failed */
}

static void
ra_output(struct rainfo *rai)
{
	int i;
	struct cmsghdr *cm;
	struct in6_pktinfo *pi;
	struct soliciter *sol;

	if ((iflist[rai->rai_ifindex]->ifm_flags & IFF_UP) == 0) {
		syslog(LOG_DEBUG, "<%s> %s is not up, skip sending RA",
		    __func__, rai->rai_ifname);
		return;
	}

	/*
	 * Check lifetime, ACCEPT_RTADV flag, and ip6.forwarding.
	 *
	 * (lifetime == 0) = output
	 * (lifetime != 0 && (ACCEPT_RTADV || !ip6.forwarding) = no output
	 *
	 * Basically, hosts MUST NOT send Router Advertisement
	 * messages at any time (RFC 4861, Section 6.2.3). However, it
	 * would sometimes be useful to allow hosts to advertise some
	 * parameters such as prefix information and link MTU. Thus,
	 * we allow hosts to invoke rtadvd only when router lifetime
	 * (on every advertising interface) is explicitly set
	 * zero. (see also the above section)
	 */
	syslog(LOG_DEBUG,
	    "<%s> check lifetime=%d, ACCEPT_RTADV=%d, ip6.forwarding=%d on %s",
	    __func__, rai->rai_lifetime, check_accept_rtadv(rai->rai_ifindex),
	    getinet6sysctl(IPV6CTL_FORWARDING), rai->rai_ifname);
	if (rai->rai_lifetime != 0) {
		if (check_accept_rtadv(rai->rai_ifindex)) {
			syslog(LOG_INFO,
			    "<%s> non-zero lifetime RA "
			    "on RA receiving interface %s."
			    "  Ignored.", __func__, rai->rai_ifname);
			return;
		}
		if (getinet6sysctl(IPV6CTL_FORWARDING) == 0) {
			syslog(LOG_INFO,
			    "<%s> non-zero lifetime RA "
			    "but net.inet6.ip6.forwarding=0.  "
			    "Ignored.", __func__);
			return;
		}
	}

	make_packet(rai);	/* XXX: inefficient */

	sndmhdr.msg_name = (caddr_t)&sin6_linklocal_allnodes;
	sndmhdr.msg_iov[0].iov_base = (caddr_t)rai->rai_ra_data;
	sndmhdr.msg_iov[0].iov_len = rai->rai_ra_datalen;

	cm = CMSG_FIRSTHDR(&sndmhdr);
	/* specify the outgoing interface */
	cm->cmsg_level = IPPROTO_IPV6;
	cm->cmsg_type = IPV6_PKTINFO;
	cm->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
	pi = (struct in6_pktinfo *)CMSG_DATA(cm);
	memset(&pi->ipi6_addr, 0, sizeof(pi->ipi6_addr));	/*XXX*/
	pi->ipi6_ifindex = rai->rai_ifindex;

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
	    __func__, rai->rai_ifname, rai->rai_waiting);

	i = sendmsg(sock, &sndmhdr, 0);

	if (i < 0 || (size_t)i != rai->rai_ra_datalen)  {
		if (i < 0) {
			syslog(LOG_ERR, "<%s> sendmsg on %s: %s",
			    __func__, rai->rai_ifname,
			    strerror(errno));
		}
	}
	/* update counter */
	if (rai->rai_initcounter < MAX_INITIAL_RTR_ADVERTISEMENTS)
		rai->rai_initcounter++;
	rai->rai_raoutput++;

	/*
	 * unicast advertisements
	 * XXX commented out.  reason: though spec does not forbit it, unicast
	 * advert does not really help
	 */
	while ((sol = TAILQ_FIRST(&rai->rai_soliciter)) != NULL) {
		TAILQ_REMOVE(&rai->rai_soliciter, sol, sol_next);
		free(sol);
	}

	/* update timestamp */
	gettimeofday(&rai->rai_lastsent, NULL);

	/* reset waiting conter */
	rai->rai_waiting = 0;
}

/* process RA timer */
struct rtadvd_timer *
ra_timeout(void *arg)
{
	struct rainfo *rai;

#ifdef notyet
	/* if necessary, reconstruct the packet. */
#endif
	rai = (struct rainfo *)arg;
	syslog(LOG_DEBUG, "<%s> RA timer on %s is expired",
	    __func__, rai->rai_ifname);

	ra_output(rai);

	return (rai->rai_timer);
}

/* update RA timer */
void
ra_timer_update(void *arg, struct timeval *tm)
{
	long interval;
	struct rainfo *rai;

	rai = (struct rainfo *)arg;
	/*
	 * Whenever a multicast advertisement is sent from an interface,
	 * the timer is reset to a uniformly-distributed random value
	 * between the interface's configured MinRtrAdvInterval and
	 * MaxRtrAdvInterval (RFC2461 6.2.4).
	 */
	interval = rai->rai_mininterval;
#ifdef HAVE_ARC4RANDOM
	interval += arc4random_uniform(rai->rai_maxinterval -
	    rai->rai_mininterval);
#else
	interval += random() % (rai->rai_maxinterval -
	    rai->rai_mininterval);
#endif

	/*
	 * For the first few advertisements (up to
	 * MAX_INITIAL_RTR_ADVERTISEMENTS), if the randomly chosen interval
	 * is greater than MAX_INITIAL_RTR_ADVERT_INTERVAL, the timer
	 * SHOULD be set to MAX_INITIAL_RTR_ADVERT_INTERVAL instead.
	 * (RFC 4861 6.2.4)
	 */
	if (rai->rai_initcounter < MAX_INITIAL_RTR_ADVERTISEMENTS &&
	    interval > MAX_INITIAL_RTR_ADVERT_INTERVAL)
		interval = MAX_INITIAL_RTR_ADVERT_INTERVAL;

	tm->tv_sec = interval;
	tm->tv_usec = 0;

	syslog(LOG_DEBUG,
	    "<%s> RA timer on %s is set to %ld:%ld",
	    __func__, rai->rai_ifname,
	    (long int)tm->tv_sec, (long int)tm->tv_usec);

	return;
}
