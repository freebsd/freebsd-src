/*
 * Copyright (c) 1983, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 */

char copyright[] =
"@(#) Copyright (c) 1983, 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#if !defined(lint) && !defined(sgi) && !defined(__NetBSD__)
static char sccsid[] = "@(#)main.c	8.1 (Berkeley) 6/5/93";
#elif defined(__NetBSD__)
static char rcsid[] = "$NetBSD$";
#endif

#include "defs.h"
#include "pathnames.h"
#ifdef sgi
#include "math.h"
#endif
#include <signal.h>
#include <fcntl.h>
#include <sys/file.h>

pid_t	mypid;

naddr	myaddr;				/* system address */
char	myname[MAXHOSTNAMELEN+1];

int	supplier;			/* supply or broadcast updates */
int	supplier_set;
int	ipforwarding = 1;		/* kernel forwarding on */

int	default_gateway;		/* 1=advertise default */
int	background = 1;
int	ridhosts;			/* 1=reduce host routes */
int	mhome;				/* 1=want multi-homed host route */
int	advertise_mhome;		/* 1=must continue adverising it */
int	auth_ok = 1;			/* 1=ignore auth if we do not care */

struct timeval epoch;			/* when started */
struct timeval clk, prev_clk;
struct timeval now;			/* current idea of time */
time_t	now_stale;
time_t	now_expire;
time_t	now_garbage;

struct timeval next_bcast;		/* next general broadcast */
struct timeval no_flash = {EPOCH+SUPPLY_INTERVAL};  /* inhibit flash update */

fd_set	fdbits;
int	sock_max;
int	rip_sock = -1;			/* RIP socket */
struct interface *rip_sock_mcast;	/* current multicast interface */
int	rt_sock;			/* routing socket */
int	rt_sock_seqno;


static  int get_rip_sock(naddr, int);
static void timevalsub(struct timeval *, struct timeval *, struct timeval *);

int
main(int argc,
     char *argv[])
{
	int n, mib[4], off;
	size_t len;
	char *p, *q;
	struct timeval wtime, t2;
	time_t dt;
	fd_set ibits;
	naddr p_net, p_mask;
	struct interface *ifp;
	struct parm parm;
	char *tracename = 0;


	/* Some shells are badly broken and send SIGHUP to backgrounded
	 * processes.
	 */
	signal(SIGHUP, SIG_IGN);

	openlog("routed", LOG_PID | LOG_ODELAY, LOG_DAEMON);
	ftrace = stdout;

	gettimeofday(&clk, 0);
	prev_clk = clk;
	epoch = clk;
	epoch.tv_sec -= EPOCH;
	now.tv_sec = EPOCH;
	now_stale = EPOCH - STALE_TIME;
	now_expire = EPOCH - EXPIRE_TIME;
	now_garbage = EPOCH - GARBAGE_TIME;
	wtime.tv_sec = 0;

	(void)gethostname(myname, sizeof(myname)-1);
	(void)gethost(myname, &myaddr);

	while ((n = getopt(argc, argv, "sqdghmpAtT:F:P:")) != EOF) {
		switch (n) {
		case 's':
			supplier = 1;
			supplier_set = 1;
			break;

		case 'q':
			supplier = 0;
			supplier_set = 1;
			break;

		case 'd':
			background = 0;
			break;

		case 'g':
			bzero(&parm, sizeof(parm));
			parm.parm_d_metric = 1;
			p = check_parms(&parm);
			if (p != 0)
				msglog("bad -g: %s", p);
			else
				default_gateway = 1;
			break;

		case 'h':		/* suppress extra host routes */
			ridhosts = 1;
			break;

		case 'm':		/* advertise host route */
			mhome = 1;	/* on multi-homed hosts */
			break;

		case 'A':
			/* Ignore authentication if we do not care.
			 * Crazy as it is, that is what RFC 1723 requires.
			 */
			auth_ok = 0;
			break;

		case 't':
			new_tracelevel++;
			break;

		case 'T':
			tracename = optarg;
			break;

		case 'F':		/* minimal routes for SLIP */
			n = FAKE_METRIC;
			p = strchr(optarg,',');
			if (p && *p != '\0') {
				n = (int)strtoul(p+1, &q, 0);
				if (*q == '\0'
				    && n <= HOPCNT_INFINITY-1
				    && n >= 1)
					*p = '\0';
			}
			if (!getnet(optarg, &p_net, &p_mask)) {
				msglog("bad network; \"-F %s\"",
				       optarg);
				break;
			}
			bzero(&parm, sizeof(parm));
			parm.parm_net = p_net;
			parm.parm_mask = p_mask;
			parm.parm_d_metric = n;
			p = check_parms(&parm);
			if (p != 0)
				msglog("bad -F: %s", p);
			break;

		case 'P':
			/* handle arbirary, (usually) per-interface
			 * parameters.
			 */
			p = parse_parms(optarg, 0);
			if (p != 0) {
				if (strcasecmp(p,optarg))
					msglog("%s in \"%s\"", p, optarg);
				else
					msglog("bad \"-P %s\"", optarg);
			}
			break;

		default:
			goto usage;
		}
	}
	argc -= optind;
	argv += optind;

	if (tracename == 0 && argc >= 1) {
		tracename = *argv++;
		argc--;
	}
	if (tracename != 0 && tracename[0] == '\0')
		goto usage;
	if (argc != 0) {
usage:
		logbad(0, "usage: routed [-sqdghmpAt] [-T tracefile]"
		       " [-F net[,metric]] [-P parms]");
	}
	if (geteuid() != 0)
		logbad(0, "requires UID 0");

	mib[0] = CTL_NET;
	mib[1] = PF_INET;
	mib[2] = IPPROTO_IP;
	mib[3] = IPCTL_FORWARDING;
	len = sizeof(ipforwarding);
	if (sysctl(mib, 4, &ipforwarding, &len, 0, 0) < 0)
		LOGERR("sysctl(IPCTL_FORWARDING)");

	if (!ipforwarding) {
		if (supplier)
			msglog("-s incompatible with ipforwarding=0");
		if (default_gateway) {
			msglog("-g incompatible with ipforwarding=0");
			default_gateway = 0;
		}
		supplier = 0;
		supplier_set = 1;
	}
	if (default_gateway) {
		if (supplier_set && !supplier) {
			msglog("-g and -q incompatible");
		} else {
			supplier = 1;
			supplier_set = 1;
		}
	}


	signal(SIGALRM, sigalrm);
	if (!background)
		signal(SIGHUP, sigterm);    /* SIGHUP fatal during debugging */
	signal(SIGTERM, sigterm);
	signal(SIGINT, sigterm);
	signal(SIGUSR1, sigtrace_on);
	signal(SIGUSR2, sigtrace_off);

	/* get into the background */
#ifdef sgi
	if (0 > _daemonize(background ? 0 : (_DF_NOCHDIR|_DF_NOFORK),
			   new_tracelevel == 0 ? -1 : STDOUT_FILENO,
			   new_tracelevel == 0 ? -1 : STDERR_FILENO,
			   -1))
		BADERR(0, "_daemonize()");
#else
	if (background && daemon(0, new_tracelevel) < 0)
		BADERR(0,"daemon()");
#endif

	mypid = getpid();
	srandom((int)(clk.tv_sec ^ clk.tv_usec ^ mypid));

	/* prepare socket connected to the kernel.
	 */
	rt_sock = socket(AF_ROUTE, SOCK_RAW, 0);
	if (rt_sock < 0)
		BADERR(1,"rt_sock = socket()");
	if (fcntl(rt_sock, F_SETFL, O_NONBLOCK) == -1)
		logbad(1, "fcntl(rt_sock) O_NONBLOCK: %s", strerror(errno));
	off = 0;
	if (setsockopt(rt_sock, SOL_SOCKET,SO_USELOOPBACK,
		       &off,sizeof(off)) < 0)
		LOGERR("setsockopt(SO_USELOOPBACK,0)");

	fix_select();


	if (background && new_tracelevel == 0)
		ftrace = 0;
	if (tracename != 0) {
		strncpy(inittracename, tracename, sizeof(inittracename)-1);
		set_tracefile(inittracename, "%s\n", -1);
	} else {
		tracelevel_msg("%s\n", -1); /* turn on tracing to stdio */
	}

	bufinit();

	/* initialize radix tree */
	rtinit();

	/* Pick a random part of the second for our output to minimize
	 * collisions.
	 *
	 * Start broadcasting after hearing from other routers, and
	 * at a random time so a bunch of systems do not get synchronized
	 * after a power failure.
	 */
	intvl_random(&next_bcast, EPOCH+MIN_WAITTIME, EPOCH+SUPPLY_INTERVAL);
	age_timer.tv_usec = next_bcast.tv_usec;
	age_timer.tv_sec = EPOCH+MIN_WAITTIME;
	rdisc_timer = next_bcast;
	ifinit_timer.tv_usec = next_bcast.tv_usec;

	/* Collect an initial view of the world by checking the interface
	 * configuration and the kludge file.
	 */
	gwkludge();
	ifinit();
	flush_kern();

	/* Ask for routes */
	rip_query();
	rdisc_sol();

	/* Loop forever, listening and broadcasting.
	 */
	for (;;) {
		prev_clk = clk;
		gettimeofday(&clk, 0);
		timevalsub(&t2, &clk, &prev_clk);
		if (t2.tv_sec < 0
		    || t2.tv_sec > wtime.tv_sec + 5) {
			/* Deal with time changes before other housekeeping to
			 * keep everything straight.
			 */
			dt = t2.tv_sec;
			if (dt > 0)
				dt -= wtime.tv_sec;
			trace_act("time changed by %d sec", dt);
			epoch.tv_sec += dt;
		}
		timevalsub(&now, &clk, &epoch);
		now_stale = now.tv_sec - STALE_TIME;
		now_expire = now.tv_sec - EXPIRE_TIME;
		now_garbage = now.tv_sec - GARBAGE_TIME;

		/* deal with signals that should affect tracing */
		set_tracelevel();

		if (stopint != 0) {
			rip_bcast(0);
			rdisc_adv();
			trace_off("exiting with signal %d\n", stopint);
			exit(stopint | 128);
		}

		/* look for new or dead interfaces */
		timevalsub(&wtime, &ifinit_timer, &now);
		if (wtime.tv_sec <= 0) {
			wtime.tv_sec = 0;
			ifinit();
			rip_query();
			continue;
		}

		/* If it is time, then broadcast our routes.
		 */
		if (supplier || advertise_mhome) {
			timevalsub(&t2, &next_bcast, &now);
			if (t2.tv_sec <= 0) {
				/* Synchronize the aging and broadcast
				 * timers to minimize awakenings
				 */
				age(0);

				rip_bcast(0);

				/* It is desirable to send routing updates
				 * regularly.  So schedule the next update
				 * 30 seconds after the previous one was
				 * secheduled, instead of 30 seconds after
				 * the previous update was finished.
				 * Even if we just started after discovering
				 * a 2nd interface or were otherwise delayed,
				 * pick a 30-second aniversary of the
				 * original broadcast time.
				 */
				n = 1 + (0-t2.tv_sec)/SUPPLY_INTERVAL;
				next_bcast.tv_sec += n*SUPPLY_INTERVAL;

				continue;
			}

			if (timercmp(&t2, &wtime, <))
				wtime = t2;
		}

		/* If we need a flash update, either do it now or
		 * set the delay to end when it is time.
		 *
		 * If we are within MIN_WAITTIME seconds of a full update,
		 * do not bother.
		 */
		if (need_flash
		    && supplier
		    && no_flash.tv_sec+MIN_WAITTIME < next_bcast.tv_sec) {
			/* accurate to the millisecond */
			if (!timercmp(&no_flash, &now, >))
				rip_bcast(1);
			timevalsub(&t2, &no_flash, &now);
			if (timercmp(&t2, &wtime, <))
				wtime = t2;
		}

		/* trigger the main aging timer.
		 */
		timevalsub(&t2, &age_timer, &now);
		if (t2.tv_sec <= 0) {
			age(0);
			continue;
		}
		if (timercmp(&t2, &wtime, <))
			wtime = t2;

		/* update the kernel routing table
		 */
		timevalsub(&t2, &need_kern, &now);
		if (t2.tv_sec <= 0) {
			age(0);
			continue;
		}
		if (timercmp(&t2, &wtime, <))
			wtime = t2;

		/* take care of router discovery,
		 * but do it to the millisecond
		 */
		if (!timercmp(&rdisc_timer, &now, >)) {
			rdisc_age(0);
			continue;
		}
		timevalsub(&t2, &rdisc_timer, &now);
		if (timercmp(&t2, &wtime, <))
			wtime = t2;


		/* wait for input or a timer to expire.
		 */
		trace_flush();
		ibits = fdbits;
		n = select(sock_max, &ibits, 0, 0, &wtime);
		if (n <= 0) {
			if (n < 0 && errno != EINTR && errno != EAGAIN)
				BADERR(1,"select");
			continue;
		}

		if (FD_ISSET(rt_sock, &ibits)) {
			read_rt();
			n--;
		}
		if (rdisc_sock >= 0 && FD_ISSET(rdisc_sock, &ibits)) {
			read_d();
			n--;
		}
		if (rip_sock >= 0 && FD_ISSET(rip_sock, &ibits)) {
			read_rip(rip_sock, 0);
			n--;
		}

		for (ifp = ifnet; n > 0 && 0 != ifp; ifp = ifp->int_next) {
			if (ifp->int_rip_sock >= 0
			    && FD_ISSET(ifp->int_rip_sock, &ibits)) {
				read_rip(ifp->int_rip_sock, ifp);
				n--;
			}
		}
	}
}


/* ARGSUSED */
void
sigalrm(int s)
{
	/* Historically, SIGALRM would cause the daemon to check for
	 * new and broken interfaces.
	 */
	ifinit_timer.tv_sec = now.tv_sec;
	trace_act("SIGALRM");
}


/* watch for fatal signals */
void
sigterm(int sig)
{
	stopint = sig;
	(void)signal(sig, SIG_DFL);	/* catch it only once */
}


void
fix_select(void)
{
	struct interface *ifp;


	FD_ZERO(&fdbits);
	sock_max = 0;

	FD_SET(rt_sock, &fdbits);
	if (sock_max <= rt_sock)
		sock_max = rt_sock+1;
	if (rip_sock >= 0) {
		FD_SET(rip_sock, &fdbits);
		if (sock_max <= rip_sock)
			sock_max = rip_sock+1;
	}
	for (ifp = ifnet; 0 != ifp; ifp = ifp->int_next) {
		if (ifp->int_rip_sock >= 0) {
			FD_SET(ifp->int_rip_sock, &fdbits);
			if (sock_max <= ifp->int_rip_sock)
				sock_max = ifp->int_rip_sock+1;
		}
	}
	if (rdisc_sock >= 0) {
		FD_SET(rdisc_sock, &fdbits);
		if (sock_max <= rdisc_sock)
			sock_max = rdisc_sock+1;
	}
}


void
fix_sock(int sock,
	 char *name)
{
	int on;
#define MIN_SOCKBUF (4*1024)
	static int rbuf;

	if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1)
		logbad(1, "fcntl(%s) O_NONBLOCK: %s",
		       name, strerror(errno));
	on = 1;
	if (setsockopt(sock, SOL_SOCKET,SO_BROADCAST, &on,sizeof(on)) < 0)
		msglog("setsockopt(%s,SO_BROADCAST): %s",
		       name, strerror(errno));
#ifdef USE_PASSIFNAME
	on = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_PASSIFNAME, &on,sizeof(on)) < 0)
		msglog("setsockopt(%s,SO_PASSIFNAME): %s",
		       name, strerror(errno));
#endif

	if (rbuf >= MIN_SOCKBUF) {
		if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF,
			       &rbuf, sizeof(rbuf)) < 0)
			msglog("setsockopt(%s,SO_RCVBUF=%d): %s",
			       name, rbuf, strerror(errno));
	} else {
		for (rbuf = 60*1024; ; rbuf -= 4096) {
			if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF,
				       &rbuf, sizeof(rbuf)) == 0) {
				trace_act("RCVBUF=%d", rbuf);
				break;
			}
			if (rbuf < MIN_SOCKBUF) {
				msglog("setsockopt(%s,SO_RCVBUF = %d): %s",
				       name, rbuf, strerror(errno));
				break;
			}
		}
	}
}


/* get a rip socket
 */
static int				/* <0 or file descriptor */
get_rip_sock(naddr addr,
	     int serious)		/* 1=failure to bind is serious */
{
	struct sockaddr_in sin;
	unsigned char ttl;
	int s;


	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		BADERR(1,"rip_sock = socket()");

	bzero(&sin,sizeof(sin));
#ifdef _HAVE_SIN_LEN
	sin.sin_len = sizeof(sin);
#endif
	sin.sin_family = AF_INET;
	sin.sin_port = htons(RIP_PORT);
	sin.sin_addr.s_addr = addr;
	if (bind(s, (struct sockaddr *)&sin,sizeof(sin)) < 0) {
		if (serious)
			BADERR(errno != EADDRINUSE, "bind(rip_sock)");
		return -1;
	}
	fix_sock(s,"rip_sock");

	ttl = 1;
	if (setsockopt(s, IPPROTO_IP, IP_MULTICAST_TTL,
		       &ttl, sizeof(ttl)) < 0)
		DBGERR(1,"rip_sock setsockopt(IP_MULTICAST_TTL)");

	return s;
}


/* turn off main RIP socket */
void
rip_off(void)
{
	struct interface *ifp;
	register naddr addr;


	if (rip_sock >= 0 && !mhome) {
		trace_act("turn off RIP");

		(void)close(rip_sock);
		rip_sock = -1;

		/* get non-broadcast sockets to listen to queries.
		 */
		for (ifp = ifnet; ifp != 0; ifp = ifp->int_next) {
			if (ifp->int_state & IS_REMOTE)
				continue;
			if (ifp->int_rip_sock < 0) {
				addr = ((ifp->int_if_flags & IFF_POINTOPOINT)
					? ifp->int_dstaddr
					: ifp->int_addr);
				ifp->int_rip_sock = get_rip_sock(addr, 0);
			}
		}

		fix_select();

		age(0);
	}
}


/* turn on RIP multicast input via an interface
 */
static void
rip_mcast_on(struct interface *ifp)
{
	struct ip_mreq m;

	if (!IS_RIP_IN_OFF(ifp->int_state)
	    && (ifp->int_if_flags & IFF_MULTICAST)
#ifdef MCAST_PPP_BUG
	    && !(ifp->int_if_flags & IFF_POINTOPOINT)
#endif
	    && !(ifp->int_state & IS_ALIAS)) {
		m.imr_multiaddr.s_addr = htonl(INADDR_RIP_GROUP);
		m.imr_interface.s_addr = ((ifp->int_if_flags & IFF_POINTOPOINT)
					  ? ifp->int_dstaddr
					  : ifp->int_addr);
		if (setsockopt(rip_sock,IPPROTO_IP, IP_ADD_MEMBERSHIP,
			       &m, sizeof(m)) < 0)
			LOGERR("setsockopt(IP_ADD_MEMBERSHIP RIP)");
	}
}


/* Prepare socket used for RIP.
 */
void
rip_on(struct interface *ifp)
{
	/* If the main RIP socket is already alive, only start receiving
	 * multicasts for this interface.
	 */
	if (rip_sock >= 0) {
		if (ifp != 0)
			rip_mcast_on(ifp);
		return;
	}

	/* If the main RIP socket is off and it makes sense to turn it on,
	 * then turn it on for all of the interfaces.
	 */
	if (rip_interfaces > 0 && !rdisc_ok) {
		trace_act("turn on RIP");

		/* Close all of the query sockets so that we can open
		 * the main socket.  SO_REUSEPORT is not a solution,
		 * since that would let two daemons bind to the broadcast
		 * socket.
		 */
		for (ifp = ifnet; ifp != 0; ifp = ifp->int_next) {
			if (ifp->int_rip_sock >= 0) {
				(void)close(ifp->int_rip_sock);
				ifp->int_rip_sock = -1;
			}
		}

		rip_sock = get_rip_sock(INADDR_ANY, 1);
		rip_sock_mcast = 0;

		/* Do not advertise anything until we have heard something
		 */
		if (next_bcast.tv_sec < now.tv_sec+MIN_WAITTIME)
			next_bcast.tv_sec = now.tv_sec+MIN_WAITTIME;

		for (ifp = ifnet; ifp != 0; ifp = ifp->int_next) {
			ifp->int_query_time = NEVER;
			rip_mcast_on(ifp);
		}
		ifinit_timer.tv_sec = now.tv_sec;

	} else if (ifp != 0
		   && !(ifp->int_state & IS_REMOTE)
		   && ifp->int_rip_sock < 0) {
		/* RIP is off, so ensure there are sockets on which
		 * to listen for queries.
		 */
		ifp->int_rip_sock = get_rip_sock(ifp->int_addr, 0);
	}

	fix_select();
}


/* die if malloc(3) fails
 */
void *
rtmalloc(size_t size,
	 char *msg)
{
	void *p = malloc(size);
	if (p == 0)
		logbad(1,"malloc() failed in %s", msg);
	return p;
}


/* get a random instant in an interval
 */
void
intvl_random(struct timeval *tp,	/* put value here */
	     u_long lo,			/* value is after this second */
	     u_long hi)			/* and before this */
{
	tp->tv_sec = (time_t)(hi == lo
			      ? lo
			      : (lo + random() % ((hi - lo))));
	tp->tv_usec = random() % 1000000;
}


void
timevaladd(struct timeval *t1,
	   struct timeval *t2)
{

	t1->tv_sec += t2->tv_sec;
	if ((t1->tv_usec += t2->tv_usec) > 1000000) {
		t1->tv_sec++;
		t1->tv_usec -= 1000000;
	}
}


/* t1 = t2 - t3
 */
static void
timevalsub(struct timeval *t1,
	   struct timeval *t2,
	   struct timeval *t3)
{
	t1->tv_sec = t2->tv_sec - t3->tv_sec;
	if ((t1->tv_usec = t2->tv_usec - t3->tv_usec) < 0) {
		t1->tv_sec--;
		t1->tv_usec += 1000000;
	}
}


/* put a message into the system log
 */
void
msglog(char *p, ...)
{
	va_list args;

	trace_flush();

	va_start(args, p);
	vsyslog(LOG_ERR, p, args);

	if (ftrace != 0) {
		if (ftrace == stdout)
			(void)fputs("routed: ", ftrace);
		(void)vfprintf(ftrace, p, args);
		(void)fputc('\n', ftrace);
	}
}


/* Put a message about a bad system into the system log if
 * we have not complained about it recently.
 *
 * It is desirable to complain about all bad systems, but not too often.
 * In the worst case, it is not practical to keep track of all bad systems.
 * For example, there can be many systems with the wrong password.
 */
void
msglim(struct msg_limit *lim, naddr addr, char *p, ...)
{
	va_list args;
	int i;
	struct msg_sub *ms1, *ms;
	char *p1;

	va_start(args, p);

	/* look for the oldest slot in the table
	 * or the slot for the bad router.
	 */
	ms = ms1 = lim->subs;
	for (i = MSG_SUBJECT_N; ; i--, ms1++) {
		if (i == 0) {
			/* Reuse a slot at most once every 10 minutes.
			 */
			if (lim->reuse > now.tv_sec) {
				ms = 0;
			} else {
				ms = ms1;
				lim->reuse = now.tv_sec + 10*60;
			}
			break;
		}
		if (ms->addr == addr) {
			/* Repeat a complaint about a given system at
			 * most once an hour.
			 */
			if (ms->until > now.tv_sec)
				ms = 0;
			break;
		}
		if (ms->until < ms1->until)
			ms = ms1;
	}
	if (ms != 0) {
		ms->addr = addr;
		ms->until = now.tv_sec + 60*60;	/* 60 minutes */

		trace_flush();
		for (p1 = p; *p1 == ' '; p1++)
			continue;
		vsyslog(LOG_ERR, p1, args);
	}

	/* always display the message if tracing */
	if (ftrace != 0) {
		(void)vfprintf(ftrace, p, args);
		(void)fputc('\n', ftrace);
	}
}


void
logbad(int dump, char *p, ...)
{
	va_list args;

	trace_flush();

	va_start(args, p);
	vsyslog(LOG_ERR, p, args);

	(void)fputs("routed: ", stderr);
	(void)vfprintf(stderr, p, args);
	(void)fputs("; giving up\n",stderr);
	(void)fflush(stderr);

	if (dump)
		abort();
	exit(1);
}
