/*	$KAME: rtsold.c,v 1.31 2001/05/22 06:03:06 jinmei Exp $	*/

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
#include <sys/time.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>

#include <netinet/in.h>
#include <netinet/icmp6.h>

#include <signal.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <stdarg.h>
#include <ifaddrs.h>
#include "rtsold.h"

struct ifinfo *iflist;
struct timeval tm_max =	{0x7fffffff, 0x7fffffff};
int aflag = 0;
int dflag = 0;
static int log_upto = 999;
static int fflag = 0;

/* protocol constatns */
#define MAX_RTR_SOLICITATION_DELAY	1 /* second */
#define RTR_SOLICITATION_INTERVAL	4 /* seconds */
#define MAX_RTR_SOLICITATIONS		3 /* times */

/* implementation dependent constants */
#define PROBE_INTERVAL 60	/* secondes XXX: should be configurable */

/* utility macros */
/* a < b */
#define TIMEVAL_LT(a, b) (((a).tv_sec < (b).tv_sec) ||\
			  (((a).tv_sec == (b).tv_sec) && \
			    ((a).tv_usec < (b).tv_usec)))

/* a <= b */
#define TIMEVAL_LEQ(a, b) (((a).tv_sec < (b).tv_sec) ||\
			   (((a).tv_sec == (b).tv_sec) &&\
 			    ((a).tv_usec <= (b).tv_usec)))

/* a == b */
#define TIMEVAL_EQ(a, b) (((a).tv_sec==(b).tv_sec) && ((a).tv_usec==(b).tv_usec))

int main __P((int argc, char *argv[]));

/* static variables and functions */
static int mobile_node = 0;
static int do_dump;
static char *dumpfilename = "/var/run/rtsold.dump"; /* XXX: should be configurable */
static char *pidfilename = "/var/run/rtsold.pid"; /* should be configurable */

static int ifconfig __P((char *ifname));
#if 0
static int ifreconfig __P((char *ifname));
#endif
static int make_packet __P((struct ifinfo *ifinfo));
static struct timeval *rtsol_check_timer __P((void));
static void TIMEVAL_ADD __P((struct timeval *a, struct timeval *b,
			     struct timeval *result));
static void TIMEVAL_SUB __P((struct timeval *a, struct timeval *b,
			     struct timeval *result));

static void rtsold_set_dump_file __P((void));
static void usage __P((char *progname));
static char **autoifprobe __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int s, rtsock, maxfd, ch;
	int once = 0;
	struct timeval *timeout;
	struct fd_set fdset;
	char *argv0;
	char *opts;

	/*
	 * Initialization
	 */
	argv0 = argv[0];

	/* get option */
	if (argv0 && argv0[strlen(argv0) - 1] != 'd') {
		fflag = 1;
		once = 1;
		opts = "adD";
	} else
		opts = "adDfm1";

	while ((ch = getopt(argc, argv, opts)) != -1) {
		switch (ch) {
		case 'a':
			aflag = 1;
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
		case 'm':
			mobile_node = 1;
			break;
		case '1':
			once = 1;
			break;
		default:
			usage(argv0);
			/*NOTREACHED*/
		}
	}
	argc -= optind;
	argv += optind;

	if (aflag) {
		int i;

		if (argc != 0) {
			usage(argv0);
			/*NOTREACHED*/
		}

		argv = autoifprobe();
		if (!argv) {
			errx(1, "could not autoprobe interface");
			/*NOTREACHED*/
		}

		for (i = 0; argv[i]; i++)
			;
		argc = i;
	}
	if (argc == 0) {
		usage(argv0);
		/*NOTREACHED*/
	}

	/* set log level */
	if (dflag == 0)
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

#ifndef HAVE_ARC4RANDOM
	/* random value initilization */
	srandom((u_long)time(NULL));
#endif

	/* warn if accept_rtadv is down */
	if (!getinet6sysctl(IPV6CTL_ACCEPT_RTADV))
		warnx("kernel is configured not to accept RAs");
	/* warn if forwarding is up */
	if (getinet6sysctl(IPV6CTL_FORWARDING))
		warnx("kernel is configured as a router, not a host");

	/* initialization to dump internal status to a file */
	if (signal(SIGUSR1, (void *)rtsold_set_dump_file) < 0) {
		errx(1, "failed to set signal for dump status");
		/*NOTREACHED*/
	}

	/*
	 * Open a socket for sending RS and receiving RA.
	 * This should be done before calling ifinit(), since the function
	 * uses the socket.
	 */
	if ((s = sockopen()) < 0) {
		errx(1, "failed to open a socket");
		/*NOTREACHED*/
	}
	maxfd = s;
	if ((rtsock = rtsock_open()) < 0) {
		errx(1, "failed to open a socket");
		/*NOTREACHED*/
	}
	if (rtsock > maxfd)
		maxfd = rtsock;

	/* configuration per interface */
	if (ifinit()) {
		errx(1, "failed to initilizatoin interfaces");
		/*NOTREACHED*/
	}
	while (argc--) {
		if (ifconfig(*argv)) {
			errx(1, "failed to initialize %s", *argv);
			/*NOTREACHED*/
		}
		argv++;
	}

	/* setup for probing default routers */
	if (probe_init()) {
		errx(1, "failed to setup for probing routers");
		/*NOTREACHED*/
	}

	if (!fflag)
		daemon(0, 0);		/* act as a daemon */

	/* dump the current pid */
	if (!once) {
		pid_t pid = getpid();
		FILE *fp;

		if ((fp = fopen(pidfilename, "w")) == NULL)
			warnmsg(LOG_ERR, __FUNCTION__,
				"failed to open a log file(%s): %s",
				pidfilename, strerror(errno));
		else {
			fprintf(fp, "%d\n", pid);
			fclose(fp);
		}
	}

	FD_ZERO(&fdset);
	FD_SET(s, &fdset);
	FD_SET(rtsock, &fdset);
	while (1) {		/* main loop */
		int e;
		struct fd_set select_fd = fdset;

		if (do_dump) {	/* SIGUSR1 */
			do_dump = 0;
			rtsold_dump_file(dumpfilename);
		}
			
		timeout = rtsol_check_timer();

		if (once) {
			struct ifinfo *ifi;

			/* if we have no timeout, we are done (or failed) */
			if (timeout == NULL)
				break;

			/* if all interfaces have got RA packet, we are done */
			for (ifi = iflist; ifi; ifi = ifi->next) {
				if (ifi->state != IFS_DOWN && ifi->racnt == 0)
					break;
			}
			if (ifi == NULL)
				break;
		}
		e = select(maxfd + 1, &select_fd, NULL, NULL, timeout);
		if (e < 1) {
			if (e < 0 && errno != EINTR) {
				warnmsg(LOG_ERR, __FUNCTION__, "select: %s",
				       strerror(errno));
			}
			continue;
		}

		/* packet reception */
		if (FD_ISSET(rtsock, &select_fd))
			rtsock_input(rtsock);
		if (FD_ISSET(s, &select_fd))
			rtsol_input(s);
	}
	/* NOTREACHED */

	return 0;
}

static int
ifconfig(char *ifname)
{
	struct ifinfo *ifinfo;
	struct sockaddr_dl *sdl;
	int flags;

	if ((sdl = if_nametosdl(ifname)) == NULL) {
		warnmsg(LOG_ERR, __FUNCTION__,
		       "failed to get link layer information for %s", ifname);
		return(-1);
	}
	if (find_ifinfo(sdl->sdl_index)) {
		warnmsg(LOG_ERR, __FUNCTION__,
			"interface %s was already configured", ifname);
		free(sdl);
		return(-1);
	}

	if ((ifinfo = malloc(sizeof(*ifinfo))) == NULL) {
		warnmsg(LOG_ERR, __FUNCTION__, "memory allocation failed");
		free(sdl);
		return(-1);
	}
	memset(ifinfo, 0, sizeof(*ifinfo));
	ifinfo->sdl = sdl;

	strncpy(ifinfo->ifname, ifname, sizeof(ifinfo->ifname));

	/* construct a router solicitation message */
	if (make_packet(ifinfo))
		goto bad;

	/*
	 * check if the interface is available.
	 * also check if SIOCGIFMEDIA ioctl is OK on the interface.
	 */
	ifinfo->mediareqok = 1;
	ifinfo->active = interface_status(ifinfo);
	if (!ifinfo->mediareqok) {
		/*
		 * probe routers periodically even if the link status
		 * does not change.
		 */
		ifinfo->probeinterval = PROBE_INTERVAL;
	}

	/* activate interface: interface_up returns 0 on success */
	flags = interface_up(ifinfo->ifname);
	if (flags == 0)
		ifinfo->state = IFS_DELAY;
	else if (flags == IFS_TENTATIVE)
		ifinfo->state = IFS_TENTATIVE;
	else
		ifinfo->state = IFS_DOWN;

	rtsol_timer_update(ifinfo);

	/* link into chain */
	if (iflist)
		ifinfo->next = iflist;
	iflist = ifinfo;

	return(0);

  bad:
	free(ifinfo->sdl);
	free(ifinfo);
	return(-1);
}

#if 0
static int
ifreconfig(char *ifname)
{
	struct ifinfo *ifi, *prev;
	int rv;

	prev = NULL;
	for (ifi = iflist; ifi; ifi = ifi->next) {
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

	return rv;
}
#endif

struct ifinfo *
find_ifinfo(int ifindex)
{
	struct ifinfo *ifi;

	for (ifi = iflist; ifi; ifi = ifi->next)
		if (ifi->sdl->sdl_index == ifindex)
			return(ifi);

	return(NULL);
}

static int
make_packet(struct ifinfo *ifinfo)
{
	char *buf;
	struct nd_router_solicit *rs;
	size_t packlen = sizeof(struct nd_router_solicit), lladdroptlen = 0;

	if ((lladdroptlen = lladdropt_length(ifinfo->sdl)) == 0) {
		warnmsg(LOG_INFO, __FUNCTION__,
			"link-layer address option has null length"
		       " on %s. Treat as not included.", ifinfo->ifname);
	}
	packlen += lladdroptlen;
	ifinfo->rs_datalen = packlen;

	/* allocate buffer */
	if ((buf = malloc(packlen)) == NULL) {
		warnmsg(LOG_ERR, __FUNCTION__,
			"memory allocation failed for %s", ifinfo->ifname);
		return(-1);
	}
	ifinfo->rs_data = buf;

	/* fill in the message */
	rs = (struct nd_router_solicit *)buf;
	rs->nd_rs_type = ND_ROUTER_SOLICIT;
	rs->nd_rs_code = 0;
	rs->nd_rs_cksum = 0;
	rs->nd_rs_reserved = 0;
	buf += sizeof(*rs);

	/* fill in source link-layer address option */
	if (lladdroptlen)
		lladdropt_fill(ifinfo->sdl, (struct nd_opt_hdr *)buf);

	return(0);
}

static struct timeval *
rtsol_check_timer()
{
	static struct timeval returnval;
	struct timeval now, rtsol_timer;
	struct ifinfo *ifinfo;
	int flags;

	gettimeofday(&now, NULL);

	rtsol_timer = tm_max;

	for (ifinfo = iflist; ifinfo; ifinfo = ifinfo->next) {
		if (TIMEVAL_LEQ(ifinfo->expire, now)) {
			if (dflag > 1)
				warnmsg(LOG_DEBUG, __FUNCTION__,
					"timer expiration on %s, "
				       "state = %d", ifinfo->ifname,
				       ifinfo->state);

			switch (ifinfo->state) {
			case IFS_DOWN:
			case IFS_TENTATIVE:
				/* interface_up returns 0 on success */
				flags = interface_up(ifinfo->ifname);
				if (flags == 0)
					ifinfo->state = IFS_DELAY;
				else if (flags == IFS_TENTATIVE)
					ifinfo->state = IFS_TENTATIVE;
				else
					ifinfo->state = IFS_DOWN;
				break;
			case IFS_IDLE:
			{
				int oldstatus = ifinfo->active;
				int probe = 0;

				ifinfo->active =
					interface_status(ifinfo);

				if (oldstatus != ifinfo->active) {
					warnmsg(LOG_DEBUG, __FUNCTION__,
						"%s status is changed"
						" from %d to %d",
						ifinfo->ifname,
						oldstatus, ifinfo->active);
					probe = 1;
					ifinfo->state = IFS_DELAY;
				}
				else if (ifinfo->probeinterval &&
					 (ifinfo->probetimer -=
					  ifinfo->timer.tv_sec) <= 0) {
					/* probe timer expired */
					ifinfo->probetimer =
						ifinfo->probeinterval;
					probe = 1;
					ifinfo->state = IFS_PROBE;
				}

				if (probe && mobile_node)
					defrouter_probe(ifinfo->sdl->sdl_index);
				break;
			}
			case IFS_DELAY:
				ifinfo->state = IFS_PROBE;
				sendpacket(ifinfo);
				break;
			case IFS_PROBE:
				if (ifinfo->probes < MAX_RTR_SOLICITATIONS)
					sendpacket(ifinfo);
				else {
					warnmsg(LOG_INFO, __FUNCTION__,
						"No answer "
						"after sending %d RSs",
						ifinfo->probes);
					ifinfo->probes = 0;
					ifinfo->state = IFS_IDLE;
				}
				break;
			}
			rtsol_timer_update(ifinfo);
		}

		if (TIMEVAL_LT(ifinfo->expire, rtsol_timer))
			rtsol_timer = ifinfo->expire;
	}

	if (TIMEVAL_EQ(rtsol_timer, tm_max)) {
		warnmsg(LOG_DEBUG, __FUNCTION__, "there is no timer");
		return(NULL);
	}
	else if (TIMEVAL_LT(rtsol_timer, now))
		/* this may occur when the interval is too small */
		returnval.tv_sec = returnval.tv_usec = 0;
	else
		TIMEVAL_SUB(&rtsol_timer, &now, &returnval);

	if (dflag > 1)
		warnmsg(LOG_DEBUG, __FUNCTION__, "New timer is %ld:%08ld",
			(long)returnval.tv_sec, (long)returnval.tv_usec);

	return(&returnval);
}

void
rtsol_timer_update(struct ifinfo *ifinfo)
{
#define MILLION 1000000
#define DADRETRY 10		/* XXX: adhoc */
	long interval;
	struct timeval now;

	bzero(&ifinfo->timer, sizeof(ifinfo->timer));

	switch (ifinfo->state) {
	case IFS_DOWN:
	case IFS_TENTATIVE:
		if (++ifinfo->dadcount > DADRETRY) {
			ifinfo->dadcount = 0;
			ifinfo->timer.tv_sec = PROBE_INTERVAL;
		}
		else
			ifinfo->timer.tv_sec = 1;
		break;
	case IFS_IDLE:
		if (mobile_node) {
			/* XXX should be configurable */ 
			ifinfo->timer.tv_sec = 3;
		}
		else
			ifinfo->timer = tm_max;	/* stop timer(valid?) */
		break;
	case IFS_DELAY:
#ifndef HAVE_ARC4RANDOM
		interval = random() % (MAX_RTR_SOLICITATION_DELAY * MILLION);
#else
		interval = arc4random() % (MAX_RTR_SOLICITATION_DELAY * MILLION);
#endif
		ifinfo->timer.tv_sec = interval / MILLION;
		ifinfo->timer.tv_usec = interval % MILLION;
		break;
	case IFS_PROBE:
		if (ifinfo->probes < MAX_RTR_SOLICITATIONS)
			ifinfo->timer.tv_sec = RTR_SOLICITATION_INTERVAL;
		else {
			/*
			 * After sending MAX_RTR_SOLICITATIONS solicitations,
			 * we're just waiting for possible replies; there
			 * will be no more solicatation.  Thus, we change
			 * the timer value to MAX_RTR_SOLICITATION_DELAY based
			 * on RFC 2461, Section 6.3.7.
			 */
			ifinfo->timer.tv_sec = MAX_RTR_SOLICITATION_DELAY;
		}
		break;
	default:
		warnmsg(LOG_ERR, __FUNCTION__,
			"illegal interface state(%d) on %s",
			ifinfo->state, ifinfo->ifname);
		return;
	}

	/* reset the timer */
	if (TIMEVAL_EQ(ifinfo->timer, tm_max)) {
		ifinfo->expire = tm_max;
		warnmsg(LOG_DEBUG, __FUNCTION__,
			"stop timer for %s", ifinfo->ifname);
	}
	else {
		gettimeofday(&now, NULL);
		TIMEVAL_ADD(&now, &ifinfo->timer, &ifinfo->expire);

		if (dflag > 1)
			warnmsg(LOG_DEBUG, __FUNCTION__,
				"set timer for %s to %d:%d", ifinfo->ifname,
			       (int)ifinfo->timer.tv_sec,
			       (int)ifinfo->timer.tv_usec);
	}

#undef MILLION
}

/* timer related utility functions */
#define MILLION 1000000

/* result = a + b */
static void
TIMEVAL_ADD(struct timeval *a, struct timeval *b, struct timeval *result)
{
	long l;

	if ((l = a->tv_usec + b->tv_usec) < MILLION) {
		result->tv_usec = l;
		result->tv_sec = a->tv_sec + b->tv_sec;
	}
	else {
		result->tv_usec = l - MILLION;
		result->tv_sec = a->tv_sec + b->tv_sec + 1;
	}
}

/*
 * result = a - b
 * XXX: this function assumes that a >= b.
 */
void
TIMEVAL_SUB(struct timeval *a, struct timeval *b, struct timeval *result)
{
	long l;

	if ((l = a->tv_usec - b->tv_usec) >= 0) {
		result->tv_usec = l;
		result->tv_sec = a->tv_sec - b->tv_sec;
	}
	else {
		result->tv_usec = MILLION + l;
		result->tv_sec = a->tv_sec - b->tv_sec - 1;
	}
}

static void
rtsold_set_dump_file()
{
	do_dump = 1;
}

static void
usage(char *progname)
{
	if (progname && progname[strlen(progname) - 1] != 'd') {
		fprintf(stderr, "usage: rtsol [-dD] interfaces...\n");
		fprintf(stderr, "usage: rtsol [-dD] -a\n");
	} else {
		fprintf(stderr, "usage: rtsold [-adDfm1] interfaces...\n");
		fprintf(stderr, "usage: rtsold [-dDfm1] -a\n");
	}
	exit(1);
}

void
#if __STDC__
warnmsg(int priority, const char *func, const char *msg, ...)
#else
warnmsg(priority, func, msg, va_alist)
	int priority;
	const char *func;
	const char *msg;
	va_dcl
#endif
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

static char **
autoifprobe()
{
#ifndef HAVE_GETIFADDRS
	errx(1, "-a is not available with the configuration");
#else
	static char ifname[IFNAMSIZ + 1];
	static char *argv[2];
	struct ifaddrs *ifap, *ifa, *target;

	if (getifaddrs(&ifap) != 0)
		return NULL;

	target = NULL;
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

		if (target && strcmp(target->ifa_name, ifa->ifa_name) == 0)
			continue;

		if (!target)
			target = ifa;
		else {
			/* if we find multiple candidates, failure. */
			if (dflag > 1)
				warnx("multiple interfaces found");
			target = NULL;
			break;
		}
	}

	if (target) {
		strncpy(ifname, target->ifa_name, sizeof(ifname) - 1);
		ifname[sizeof(ifname) - 1] = '\0';
		argv[0] = ifname;
		argv[1] = NULL;

		if (dflag > 0)
			warnx("probing %s", argv[0]);
	}
	freeifaddrs(ifap);
	if (target)
		return argv;
	else
		return (char **)NULL;
#endif
}
