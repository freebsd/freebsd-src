/*	$KAME: rtsold.c,v 1.67 2003/05/17 18:16:15 itojun Exp $	*/

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

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/param.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_var.h>

#include <netinet/in.h>
#include <netinet/icmp6.h>
#include <netinet/in_var.h>
#include <arpa/inet.h>

#include <netinet6/nd6.h>

#include <signal.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <err.h>
#include <stdarg.h>
#include <ifaddrs.h>
#ifdef HAVE_POLL_H
#include <poll.h>
#endif

#include "rtsold.h"

#define RTSOL_DUMPFILE	"/var/run/rtsold.dump";
#define RTSOL_PIDFILE	"/var/run/rtsold.pid";

struct timespec tm_max;
static int log_upto = 999;
static int fflag = 0;

int Fflag = 0;	/* force setting sysctl parameters */
int aflag = 0;
int dflag = 0;
int uflag = 0;

const char *otherconf_script;
const char *resolvconf_script = "/sbin/resolvconf";

/* protocol constants */
#define MAX_RTR_SOLICITATION_DELAY	1 /* second */
#define RTR_SOLICITATION_INTERVAL	4 /* seconds */
#define MAX_RTR_SOLICITATIONS		3 /* times */

/*
 * implementation dependent constants in seconds
 * XXX: should be configurable
 */
#define PROBE_INTERVAL 60

/* static variables and functions */
static int mobile_node = 0;
static const char *pidfilename = RTSOL_PIDFILE;

#ifndef SMALL
static int do_dump;
static const char *dumpfilename = RTSOL_DUMPFILE;
#endif

#if 0
static int ifreconfig(char *);
#endif

static int make_packet(struct ifinfo *);
static struct timespec *rtsol_check_timer(void);

#ifndef SMALL
static void rtsold_set_dump_file(int);
#endif
static void usage(void);

int
main(int argc, char **argv)
{
	int s, ch, once = 0;
	struct timespec *timeout;
	const char *opts;
#ifdef HAVE_POLL_H
	struct pollfd set[2];
#else
	fd_set *fdsetp, *selectfdp;
	int fdmasks;
	int maxfd;
#endif
	int rtsock;
	char *argv0;

#ifndef SMALL
	/* rtsold */
	opts = "adDfFm1O:p:R:u";
#else
	/* rtsol */
	opts = "adDFO:R:u";
	fflag = 1;
	once = 1;
#endif
	argv0 = argv[0];

	while ((ch = getopt(argc, argv, opts)) != -1) {
		switch (ch) {
		case 'a':
			aflag = 1;
			break;
		case 'd':
			dflag += 1;
			break;
		case 'D':
			dflag += 2;
			break;
		case 'f':
			fflag = 1;
			break;
		case 'F':
			Fflag = 1;
			break;
		case 'm':
			mobile_node = 1;
			break;
		case '1':
			once = 1;
			break;
		case 'O':
			otherconf_script = optarg;
			break;
		case 'p':
			pidfilename = optarg;
			break;
		case 'R':
			resolvconf_script = optarg;
			break;
		case 'u':
			uflag = 1;
			break;
		default:
			usage();
			exit(1);
		}
	}
	argc -= optind;
	argv += optind;

	if ((!aflag && argc == 0) || (aflag && argc != 0)) {
		usage();
		exit(1);
	}

	/* Generate maximum time in timespec. */
	tm_max.tv_sec = (-1) & ~((time_t)1 << ((sizeof(tm_max.tv_sec) * 8) - 1));
	tm_max.tv_nsec = (-1) & ~((long)1 << ((sizeof(tm_max.tv_nsec) * 8) - 1));

	/* set log level */
	if (dflag > 1)
		log_upto = LOG_DEBUG;
	else if (dflag > 0)
		log_upto = LOG_INFO;
	else
		log_upto = LOG_NOTICE;

	if (!fflag) {
		char *ident;

		ident = strrchr(argv0, '/');
		if (!ident)
			ident = argv0;
		else
			ident++;
		openlog(ident, LOG_NDELAY|LOG_PID, LOG_DAEMON);
		if (log_upto >= 0)
			setlogmask(LOG_UPTO(log_upto));
	}

	if (otherconf_script && *otherconf_script != '/') {
		errx(1, "configuration script (%s) must be an absolute path",
		    otherconf_script);
	}
	if (resolvconf_script && *resolvconf_script != '/') {
		errx(1, "configuration script (%s) must be an absolute path",
		    resolvconf_script);
	}
	if (pidfilename && *pidfilename != '/') {
		errx(1, "pid filename (%s) must be an absolute path",
		    pidfilename);
	}
#ifndef HAVE_ARC4RANDOM
	/* random value initialization */
	srandom((u_long)time(NULL));
#endif

#if (__FreeBSD_version < 900000)
	if (Fflag) {
		setinet6sysctl(IPV6CTL_FORWARDING, 0);
	} else {
		/* warn if forwarding is up */
		if (getinet6sysctl(IPV6CTL_FORWARDING))
			warnx("kernel is configured as a router, not a host");
	}
#endif

#ifndef SMALL
	/* initialization to dump internal status to a file */
	signal(SIGUSR1, rtsold_set_dump_file);
#endif

	if (!fflag)
		daemon(0, 0);		/* act as a daemon */

	/*
	 * Open a socket for sending RS and receiving RA.
	 * This should be done before calling ifinit(), since the function
	 * uses the socket.
	 */
	if ((s = sockopen()) < 0) {
		warnmsg(LOG_ERR, __func__, "failed to open a socket");
		exit(1);
	}
#ifdef HAVE_POLL_H
	set[0].fd = s;
	set[0].events = POLLIN;
#else
	maxfd = s;
#endif

#ifdef HAVE_POLL_H
	set[1].fd = -1;
#endif

	if ((rtsock = rtsock_open()) < 0) {
		warnmsg(LOG_ERR, __func__, "failed to open a socket");
		exit(1);
	}
#ifdef HAVE_POLL_H
	set[1].fd = rtsock;
	set[1].events = POLLIN;
#else
	if (rtsock > maxfd)
		maxfd = rtsock;
#endif

#ifndef HAVE_POLL_H
	fdmasks = howmany(maxfd + 1, NFDBITS) * sizeof(fd_mask);
	if ((fdsetp = malloc(fdmasks)) == NULL) {
		warnmsg(LOG_ERR, __func__, "malloc");
		exit(1);
	}
	if ((selectfdp = malloc(fdmasks)) == NULL) {
		warnmsg(LOG_ERR, __func__, "malloc");
		exit(1);
	}
#endif

	/* configuration per interface */
	if (ifinit()) {
		warnmsg(LOG_ERR, __func__,
		    "failed to initialize interfaces");
		exit(1);
	}
	if (aflag)
		argv = autoifprobe();
	while (argv && *argv) {
		if (ifconfig(*argv)) {
			warnmsg(LOG_ERR, __func__,
			    "failed to initialize %s", *argv);
			exit(1);
		}
		argv++;
	}

	/* setup for probing default routers */
	if (probe_init()) {
		warnmsg(LOG_ERR, __func__,
		    "failed to setup for probing routers");
		exit(1);
		/*NOTREACHED*/
	}

	/* dump the current pid */
	if (!once) {
		pid_t pid = getpid();
		FILE *fp;

		if ((fp = fopen(pidfilename, "w")) == NULL)
			warnmsg(LOG_ERR, __func__,
			    "failed to open a pid log file(%s): %s",
			    pidfilename, strerror(errno));
		else {
			fprintf(fp, "%d\n", pid);
			fclose(fp);
		}
	}
#ifndef HAVE_POLL_H
	memset(fdsetp, 0, fdmasks);
	FD_SET(s, fdsetp);
	FD_SET(rtsock, fdsetp);
#endif
	while (1) {		/* main loop */
		int e;

#ifndef HAVE_POLL_H
		memcpy(selectfdp, fdsetp, fdmasks);
#endif

#ifndef SMALL
		if (do_dump) {	/* SIGUSR1 */
			do_dump = 0;
			rtsold_dump_file(dumpfilename);
		}
#endif

		timeout = rtsol_check_timer();

		if (once) {
			struct ifinfo *ifi;

			/* if we have no timeout, we are done (or failed) */
			if (timeout == NULL)
				break;

			/* if all interfaces have got RA packet, we are done */
			TAILQ_FOREACH(ifi, &ifinfo_head, ifi_next) {
				if (ifi->state != IFS_DOWN && ifi->racnt == 0)
					break;
			}
			if (ifi == NULL)
				break;
		}
#ifdef HAVE_POLL_H
		e = poll(set, 2, timeout ? (timeout->tv_sec * 1000 + timeout->tv_nsec / 1000 / 1000) : INFTIM);
#else
		e = select(maxfd + 1, selectfdp, NULL, NULL, timeout);
#endif
		if (e < 1) {
			if (e < 0 && errno != EINTR) {
				warnmsg(LOG_ERR, __func__, "select: %s",
				    strerror(errno));
			}
			continue;
		}

		/* packet reception */
#ifdef HAVE_POLL_H
		if (set[1].revents & POLLIN)
#else
		if (FD_ISSET(rtsock, selectfdp))
#endif
			rtsock_input(rtsock);
#ifdef HAVE_POLL_H
		if (set[0].revents & POLLIN)
#else
		if (FD_ISSET(s, selectfdp))
#endif
			rtsol_input(s);
	}
	/* NOTREACHED */

	return (0);
}

int
ifconfig(char *ifname)
{
	struct ifinfo *ifi;
	struct sockaddr_dl *sdl;
	int flags;

	if ((sdl = if_nametosdl(ifname)) == NULL) {
		warnmsg(LOG_ERR, __func__,
		    "failed to get link layer information for %s", ifname);
		return (-1);
	}
	if (find_ifinfo(sdl->sdl_index)) {
		warnmsg(LOG_ERR, __func__,
		    "interface %s was already configured", ifname);
		free(sdl);
		return (-1);
	}

	if (Fflag) {
		struct in6_ndireq nd;
		int s;

		if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
			warnmsg(LOG_ERR, __func__, "socket() failed.");
			return (-1);
		}
		memset(&nd, 0, sizeof(nd));
		strlcpy(nd.ifname, ifname, sizeof(nd.ifname));
		if (ioctl(s, SIOCGIFINFO_IN6, (caddr_t)&nd) < 0) {
			warnmsg(LOG_ERR, __func__,
			    "cannot get accept_rtadv flag");
			close(s);
			return (-1);
		}
		nd.ndi.flags |= ND6_IFF_ACCEPT_RTADV;
		if (ioctl(s, SIOCSIFINFO_IN6, (caddr_t)&nd) < 0) {
			warnmsg(LOG_ERR, __func__,
			    "cannot set accept_rtadv flag");
			close(s);
			return (-1);
		}
		close(s);
	}

	if ((ifi = malloc(sizeof(*ifi))) == NULL) {
		warnmsg(LOG_ERR, __func__, "memory allocation failed");
		free(sdl);
		return (-1);
	}
	memset(ifi, 0, sizeof(*ifi));
	ifi->sdl = sdl;
	ifi->ifi_rdnss = IFI_DNSOPT_STATE_NOINFO;
	ifi->ifi_dnssl = IFI_DNSOPT_STATE_NOINFO;
	TAILQ_INIT(&ifi->ifi_rainfo);
	strlcpy(ifi->ifname, ifname, sizeof(ifi->ifname));

	/* construct a router solicitation message */
	if (make_packet(ifi))
		goto bad;

	/* set link ID of this interface. */
#ifdef HAVE_SCOPELIB
	if (inet_zoneid(AF_INET6, 2, ifname, &ifi->linkid))
		goto bad;
#else
	/* XXX: assume interface IDs as link IDs */
	ifi->linkid = ifi->sdl->sdl_index;
#endif

	/*
	 * check if the interface is available.
	 * also check if SIOCGIFMEDIA ioctl is OK on the interface.
	 */
	ifi->mediareqok = 1;
	ifi->active = interface_status(ifi);
	if (!ifi->mediareqok) {
		/*
		 * probe routers periodically even if the link status
		 * does not change.
		 */
		ifi->probeinterval = PROBE_INTERVAL;
	}

	/* activate interface: interface_up returns 0 on success */
	flags = interface_up(ifi->ifname);
	if (flags == 0)
		ifi->state = IFS_DELAY;
	else if (flags == IFS_TENTATIVE)
		ifi->state = IFS_TENTATIVE;
	else
		ifi->state = IFS_DOWN;

	rtsol_timer_update(ifi);

	TAILQ_INSERT_TAIL(&ifinfo_head, ifi, ifi_next);
	return (0);

bad:
	free(ifi->sdl);
	free(ifi);
	return (-1);
}

void
iflist_init(void)
{
	struct ifinfo *ifi;

	while ((ifi = TAILQ_FIRST(&ifinfo_head)) != NULL) {
		TAILQ_REMOVE(&ifinfo_head, ifi, ifi_next);
		if (ifi->sdl != NULL)
			free(ifi->sdl);
		if (ifi->rs_data != NULL)
			free(ifi->rs_data);
		free(ifi);
	}
}

#if 0
static int
ifreconfig(char *ifname)
{
	struct ifinfo *ifi, *prev;
	int rv;

	prev = NULL;
	TAILQ_FOREACH(ifi, &ifinfo_head, ifi_next) {
		if (strncmp(ifi->ifname, ifname, sizeof(ifi->ifname)) == 0)
			break;
		prev = ifi;
	}
	prev->next = ifi->next;

	rv = ifconfig(ifname);

	/* reclaim it after ifconfig() in case ifname is pointer inside ifi */
	if (ifi->rs_data)
		free(ifi->rs_data);
	free(ifi->sdl);
	free(ifi);

	return (rv);
}
#endif

struct rainfo *
find_rainfo(struct ifinfo *ifi, struct sockaddr_in6 *sin6)
{
	struct rainfo *rai;

	TAILQ_FOREACH(rai, &ifi->ifi_rainfo, rai_next)
		if (memcmp(&rai->rai_saddr.sin6_addr, &sin6->sin6_addr,
		    sizeof(rai->rai_saddr.sin6_addr)) == 0)
			return (rai);

	return (NULL);
}

struct ifinfo *
find_ifinfo(int ifindex)
{
	struct ifinfo *ifi;

	TAILQ_FOREACH(ifi, &ifinfo_head, ifi_next) {
		if (ifi->sdl->sdl_index == ifindex)
			return (ifi);
	}
	return (NULL);
}

static int
make_packet(struct ifinfo *ifi)
{
	size_t packlen = sizeof(struct nd_router_solicit), lladdroptlen = 0;
	struct nd_router_solicit *rs;
	char *buf;

	if ((lladdroptlen = lladdropt_length(ifi->sdl)) == 0) {
		warnmsg(LOG_INFO, __func__,
		    "link-layer address option has null length"
		    " on %s. Treat as not included.", ifi->ifname);
	}
	packlen += lladdroptlen;
	ifi->rs_datalen = packlen;

	/* allocate buffer */
	if ((buf = malloc(packlen)) == NULL) {
		warnmsg(LOG_ERR, __func__,
		    "memory allocation failed for %s", ifi->ifname);
		return (-1);
	}
	ifi->rs_data = buf;

	/* fill in the message */
	rs = (struct nd_router_solicit *)buf;
	rs->nd_rs_type = ND_ROUTER_SOLICIT;
	rs->nd_rs_code = 0;
	rs->nd_rs_cksum = 0;
	rs->nd_rs_reserved = 0;
	buf += sizeof(*rs);

	/* fill in source link-layer address option */
	if (lladdroptlen)
		lladdropt_fill(ifi->sdl, (struct nd_opt_hdr *)buf);

	return (0);
}

static struct timespec *
rtsol_check_timer(void)
{
	static struct timespec returnval;
	struct timespec now, rtsol_timer;
	struct ifinfo *ifi;
	struct rainfo *rai;
	struct ra_opt *rao;
	int flags;

	clock_gettime(CLOCK_MONOTONIC_FAST, &now);

	rtsol_timer = tm_max;

	TAILQ_FOREACH(ifi, &ifinfo_head, ifi_next) {
		if (TS_CMP(&ifi->expire, &now, <=)) {
			warnmsg(LOG_DEBUG, __func__, "timer expiration on %s, "
			    "state = %d", ifi->ifname, ifi->state);

			while((rai = TAILQ_FIRST(&ifi->ifi_rainfo)) != NULL) {
				/* Remove all RA options. */
				TAILQ_REMOVE(&ifi->ifi_rainfo, rai, rai_next);
				while ((rao = TAILQ_FIRST(&rai->rai_ra_opt)) !=
				    NULL) {
					TAILQ_REMOVE(&rai->rai_ra_opt, rao,
					    rao_next);
					if (rao->rao_msg != NULL)
						free(rao->rao_msg);
					free(rao);
				}
				free(rai);
			}
			switch (ifi->state) {
			case IFS_DOWN:
			case IFS_TENTATIVE:
				/* interface_up returns 0 on success */
				flags = interface_up(ifi->ifname);
				if (flags == 0)
					ifi->state = IFS_DELAY;
				else if (flags == IFS_TENTATIVE)
					ifi->state = IFS_TENTATIVE;
				else
					ifi->state = IFS_DOWN;
				break;
			case IFS_IDLE:
			{
				int oldstatus = ifi->active;
				int probe = 0;

				ifi->active = interface_status(ifi);

				if (oldstatus != ifi->active) {
					warnmsg(LOG_DEBUG, __func__,
					    "%s status is changed"
					    " from %d to %d",
					    ifi->ifname,
					    oldstatus, ifi->active);
					probe = 1;
					ifi->state = IFS_DELAY;
				} else if (ifi->probeinterval &&
				    (ifi->probetimer -=
				    ifi->timer.tv_sec) <= 0) {
					/* probe timer expired */
					ifi->probetimer =
					    ifi->probeinterval;
					probe = 1;
					ifi->state = IFS_PROBE;
				}

				/*
				 * If we need a probe, clear the previous
				 * status wrt the "other" configuration.
				 */
				if (probe)
					ifi->otherconfig = 0;

				if (probe && mobile_node)
					defrouter_probe(ifi);
				break;
			}
			case IFS_DELAY:
				ifi->state = IFS_PROBE;
				sendpacket(ifi);
				break;
			case IFS_PROBE:
				if (ifi->probes < MAX_RTR_SOLICITATIONS)
					sendpacket(ifi);
				else {
					warnmsg(LOG_INFO, __func__,
					    "No answer after sending %d RSs",
					    ifi->probes);
					ifi->probes = 0;
					ifi->state = IFS_IDLE;
				}
				break;
			}
			rtsol_timer_update(ifi);
		} else {
			/* Expiration check for RA options. */
			int expire = 0;

			TAILQ_FOREACH(rai, &ifi->ifi_rainfo, rai_next) {
				TAILQ_FOREACH(rao, &rai->rai_ra_opt, rao_next) {
					warnmsg(LOG_DEBUG, __func__,
					    "RA expiration timer: "
					    "type=%d, msg=%s, expire=%s",
					    rao->rao_type, (char *)rao->rao_msg,
						sec2str(&rao->rao_expire));
					if (TS_CMP(&now, &rao->rao_expire,
					    >=)) {
						warnmsg(LOG_DEBUG, __func__,
						    "RA expiration timer: "
						    "expired.");
						TAILQ_REMOVE(&rai->rai_ra_opt,
						    rao, rao_next);
						if (rao->rao_msg != NULL)
							free(rao->rao_msg);
						free(rao);
						expire = 1;
					}
				}
			}
			if (expire)
				ra_opt_handler(ifi);
		}
		if (TS_CMP(&ifi->expire, &rtsol_timer, <))
			rtsol_timer = ifi->expire;
	}

	if (TS_CMP(&rtsol_timer, &tm_max, ==)) {
		warnmsg(LOG_DEBUG, __func__, "there is no timer");
		return (NULL);
	} else if (TS_CMP(&rtsol_timer, &now, <))
		/* this may occur when the interval is too small */
		returnval.tv_sec = returnval.tv_nsec = 0;
	else
		TS_SUB(&rtsol_timer, &now, &returnval);

	now.tv_sec += returnval.tv_sec;
	now.tv_nsec += returnval.tv_nsec;
	warnmsg(LOG_DEBUG, __func__, "New timer is %s",
	    sec2str(&now));

	return (&returnval);
}

void
rtsol_timer_update(struct ifinfo *ifi)
{
#define MILLION 1000000
#define DADRETRY 10		/* XXX: adhoc */
	long interval;
	struct timespec now;

	bzero(&ifi->timer, sizeof(ifi->timer));

	switch (ifi->state) {
	case IFS_DOWN:
	case IFS_TENTATIVE:
		if (++ifi->dadcount > DADRETRY) {
			ifi->dadcount = 0;
			ifi->timer.tv_sec = PROBE_INTERVAL;
		} else
			ifi->timer.tv_sec = 1;
		break;
	case IFS_IDLE:
		if (mobile_node) {
			/* XXX should be configurable */
			ifi->timer.tv_sec = 3;
		}
		else
			ifi->timer = tm_max;	/* stop timer(valid?) */
		break;
	case IFS_DELAY:
#ifndef HAVE_ARC4RANDOM
		interval = random() % (MAX_RTR_SOLICITATION_DELAY * MILLION);
#else
		interval = arc4random_uniform(MAX_RTR_SOLICITATION_DELAY * MILLION);
#endif
		ifi->timer.tv_sec = interval / MILLION;
		ifi->timer.tv_nsec = (interval % MILLION) * 1000;
		break;
	case IFS_PROBE:
		if (ifi->probes < MAX_RTR_SOLICITATIONS)
			ifi->timer.tv_sec = RTR_SOLICITATION_INTERVAL;
		else {
			/*
			 * After sending MAX_RTR_SOLICITATIONS solicitations,
			 * we're just waiting for possible replies; there
			 * will be no more solicitation.  Thus, we change
			 * the timer value to MAX_RTR_SOLICITATION_DELAY based
			 * on RFC 2461, Section 6.3.7.
			 */
			ifi->timer.tv_sec = MAX_RTR_SOLICITATION_DELAY;
		}
		break;
	default:
		warnmsg(LOG_ERR, __func__,
		    "illegal interface state(%d) on %s",
		    ifi->state, ifi->ifname);
		return;
	}

	/* reset the timer */
	if (TS_CMP(&ifi->timer, &tm_max, ==)) {
		ifi->expire = tm_max;
		warnmsg(LOG_DEBUG, __func__,
		    "stop timer for %s", ifi->ifname);
	} else {
		clock_gettime(CLOCK_MONOTONIC_FAST, &now);
		TS_ADD(&now, &ifi->timer, &ifi->expire);

		now.tv_sec += ifi->timer.tv_sec;
		now.tv_nsec += ifi->timer.tv_nsec;
		warnmsg(LOG_DEBUG, __func__, "set timer for %s to %s",
		    ifi->ifname, sec2str(&now));
	}

#undef MILLION
}

/* timer related utility functions */
#define MILLION 1000000

#ifndef SMALL
static void
rtsold_set_dump_file(int sig __unused)
{
	do_dump = 1;
}
#endif

static void
usage(void)
{
#ifndef SMALL
	fprintf(stderr, "usage: rtsold [-adDfFm1] [-O script-name] "
	    "[-P pidfile] [-R script-name] interfaces...\n");
	fprintf(stderr, "usage: rtsold [-dDfFm1] [-O script-name] "
	    "[-P pidfile] [-R script-name] -a\n");
#else
	fprintf(stderr, "usage: rtsol [-dDF] [-O script-name] "
	    "[-P pidfile] [-R script-name] interfaces...\n");
	fprintf(stderr, "usage: rtsol [-dDF] [-O script-name] "
	    "[-P pidfile] [-R script-name] -a\n");
#endif
}

void
warnmsg(int priority, const char *func, const char *msg, ...)
{
	va_list ap;
	char buf[BUFSIZ];

	va_start(ap, msg);
	if (fflag) {
		if (priority <= log_upto) {
			(void)vfprintf(stderr, msg, ap);
			(void)fprintf(stderr, "\n");
		}
	} else {
		snprintf(buf, sizeof(buf), "<%s> %s", func, msg);
		msg = buf;
		vsyslog(priority, msg, ap);
	}
	va_end(ap);
}

/*
 * return a list of interfaces which is suitable to sending an RS.
 */
char **
autoifprobe(void)
{
	static char **argv = NULL;
	static int n = 0;
	char **a;
	int s = 0, i, found;
	struct ifaddrs *ifap, *ifa;
	struct in6_ndireq nd;

	/* initialize */
	while (n--)
		free(argv[n]);
	if (argv) {
		free(argv);
		argv = NULL;
	}
	n = 0;

	if (getifaddrs(&ifap) != 0)
		return (NULL);

	if (!Fflag && (s = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
		warnmsg(LOG_ERR, __func__, "socket");
		exit(1);
	}

	/* find an ethernet */
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if ((ifa->ifa_flags & IFF_UP) == 0)
			continue;
		if ((ifa->ifa_flags & IFF_POINTOPOINT) != 0)
			continue;
		if ((ifa->ifa_flags & IFF_LOOPBACK) != 0)
			continue;
		if ((ifa->ifa_flags & IFF_MULTICAST) == 0)
			continue;

		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;

		found = 0;
		for (i = 0; i < n; i++) {
			if (strcmp(argv[i], ifa->ifa_name) == 0) {
				found++;
				break;
			}
		}
		if (found)
			continue;

		/*
		 * Skip the interfaces which IPv6 and/or accepting RA
		 * is disabled.
		 */
		if (!Fflag) {
			memset(&nd, 0, sizeof(nd));
			strlcpy(nd.ifname, ifa->ifa_name, sizeof(nd.ifname));
			if (ioctl(s, SIOCGIFINFO_IN6, (caddr_t)&nd) < 0) {
				warnmsg(LOG_ERR, __func__,
					"ioctl(SIOCGIFINFO_IN6)");
				exit(1);
			}
			if ((nd.ndi.flags & ND6_IFF_IFDISABLED))
				continue;
			if (!(nd.ndi.flags & ND6_IFF_ACCEPT_RTADV))
				continue;
		}

		/* if we find multiple candidates, just warn. */
		if (n != 0 && dflag > 1)
			warnmsg(LOG_WARNING, __func__,
				"multiple interfaces found");

		a = (char **)realloc(argv, (n + 1) * sizeof(char **));
		if (a == NULL) {
			warnmsg(LOG_ERR, __func__, "realloc");
			exit(1);
		}
		argv = a;
		argv[n] = strdup(ifa->ifa_name);
		if (!argv[n]) {
			warnmsg(LOG_ERR, __func__, "malloc");
			exit(1);
		}
		n++;
	}

	if (n) {
		a = (char **)realloc(argv, (n + 1) * sizeof(char **));
		if (a == NULL) {
			warnmsg(LOG_ERR, __func__, "realloc");
			exit(1);
		}
		argv = a;
		argv[n] = NULL;

		if (dflag > 0) {
			for (i = 0; i < n; i++)
				warnmsg(LOG_WARNING, __func__, "probing %s",
					argv[i]);
		}
	}
	if (!Fflag)
		close(s);
	freeifaddrs(ifap);
	return (argv);
}
