/*
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE".  Use of the mrouted program represents acceptance of
 * the terms and conditions listed in that file.
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 *
 * main.c,v 3.8.4.29 1998/03/01 01:49:00 fenner Exp
 */

/*
 * Written by Steve Deering, Stanford University, February 1989.
 *
 * (An earlier version of DVMRP was implemented by David Waitzman of
 *  BBN STC by extending Berkeley's routed program.  Some of Waitzman's
 *  extensions have been incorporated into mrouted, but none of the
 *  original routed code has been adopted.)
 */


#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif

#include <err.h>
#include "defs.h"
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <fcntl.h>
#include <paths.h>

#ifdef SNMP
#include "snmp.h"
#endif

extern char *configfilename;
char versionstring[100];

static char pidfilename[]  = _PATH_MROUTED_PID;
static char dumpfilename[] = _PATH_MROUTED_DUMP;
static char cachefilename[] = _PATH_MROUTED_CACHE;
static char genidfilename[] = _PATH_MROUTED_GENID;

static int haveterminal = 1;
int did_final_init = 0;

static int sighandled = 0;
#define	GOT_SIGINT	0x01
#define	GOT_SIGHUP	0x02
#define	GOT_SIGUSR1	0x04
#define	GOT_SIGUSR2	0x08

int cache_lifetime 	= DEFAULT_CACHE_LIFETIME;
int prune_lifetime	= AVERAGE_PRUNE_LIFETIME;

int debug = 0;
char *progname;
time_t mrouted_init_time;

#ifdef SNMP
#define NHANDLERS	34
#else
#define NHANDLERS	2
#endif

static struct ihandler {
    int fd;			/* File descriptor		 */
    ihfunc_t func;		/* Function to call with &fd_set */
} ihandlers[NHANDLERS];
static int nhandlers = 0;

static struct debugname {
    char	*name;
    int		 level;
    int		 nchars;
} debugnames[] = {
    {	"packet",	DEBUG_PKT,	2	},
    {	"pkt",		DEBUG_PKT,	3	},
    {	"pruning",	DEBUG_PRUNE,	1	},
    {	"prunes",	DEBUG_PRUNE,	1	},
    {	"routing",	DEBUG_ROUTE,	1	},
    {	"routes",	DEBUG_ROUTE,	1	},
    {   "route_detail",	DEBUG_RTDETAIL, 6	},
    {   "rtdetail",	DEBUG_RTDETAIL, 2	},
    {	"peers",	DEBUG_PEER,	2	},
    {	"neighbors",	DEBUG_PEER,	1	},
    {	"cache",	DEBUG_CACHE,	1	},
    {	"timeout",	DEBUG_TIMEOUT,	1	},
    {	"callout",	DEBUG_TIMEOUT,	2	},
    {	"interface",	DEBUG_IF,	2	},
    {	"vif",		DEBUG_IF,	1	},
    {	"membership",	DEBUG_MEMBER,	1	},
    {	"groups",	DEBUG_MEMBER,	1	},
    {	"traceroute",	DEBUG_TRACE,	2	},
    {	"mtrace",	DEBUG_TRACE,	2	},
    {	"igmp",		DEBUG_IGMP,	1	},
    {	"icmp",		DEBUG_ICMP,	2	},
    {	"rsrr",		DEBUG_RSRR,	2	},
    {	"3",		0xffffffff,	1	}	/* compat. */
};

/*
 * Forward declarations.
 */
static void final_init __P((void *));
static void fasttimer __P((void *));
static void timer __P((void *));
static void dump __P((void));
static void dump_version __P((FILE *));
static void fdump __P((void));
static void cdump __P((void));
static void restart __P((void));
static void handler __P((int));
static void cleanup __P((void));
static void resetlogging __P((void *));
static void usage __P((void));

/* To shut up gcc -Wstrict-prototypes */
int main __P((int argc, char **argv));

int
register_input_handler(fd, func)
    int fd;
    ihfunc_t func;
{
    if (nhandlers >= NHANDLERS)
	return -1;

    ihandlers[nhandlers].fd = fd;
    ihandlers[nhandlers++].func = func;

    return 0;
}

int
main(argc, argv)
    int argc;
    char *argv[];
{
    register int recvlen;
    int dummy;
    FILE *fp;
    struct timeval tv, difftime, curtime, lasttime, *timeout;
    u_int32 prev_genid;
    int vers;
    fd_set rfds, readers;
    int nfds, n, i, secs;
    extern char todaysversion[];
    struct sigaction sa;
#ifdef SNMP
    struct timeval  timeout, *tvp = &timeout;
    struct timeval  sched, *svp = &sched, now, *nvp = &now;
    int index, block;
#endif

    setlinebuf(stderr);

    if (geteuid() != 0)
	errx(1, "must be root");

    progname = strrchr(argv[0], '/');
    if (progname)
	progname++;
    else
	progname = argv[0];

    argv++, argc--;
    while (argc > 0 && *argv[0] == '-') {
	if (strcmp(*argv, "-d") == 0) {
	    if (argc > 1 && *(argv + 1)[0] != '-') {
		char *p,*q;
		int i, len;
		struct debugname *d;

		argv++, argc--;
		debug = 0;
		p = *argv; q = NULL;
		while (p) {
		    q = strchr(p, ',');
		    if (q)
			*q++ = '\0';
		    len = strlen(p);
		    for (i = 0, d = debugnames;
				i < sizeof(debugnames) / sizeof(debugnames[0]);
				i++, d++)
			if (len >= d->nchars && strncmp(d->name, p, len) == 0)
				break;
		    if (i == sizeof(debugnames) / sizeof(debugnames[0])) {
			int j = 0xffffffff;
			int k = 0;
			fprintf(stderr, "Valid debug levels: ");
			for (i = 0, d = debugnames;
				i < sizeof(debugnames) / sizeof(debugnames[0]);
				i++, d++) {
			    if ((j & d->level) == d->level) {
				if (k++)
				    putc(',', stderr);
				fputs(d->name, stderr);
				j &= ~d->level;
			    }
			}
			putc('\n', stderr);
			usage();
		    }
		    debug |= d->level;
		    p = q;
		}
	    } else
		debug = DEFAULT_DEBUG;
	} else if (strcmp(*argv, "-c") == 0) {
	    if (argc > 1) {
		argv++, argc--;
		configfilename = *argv;
	    } else
		usage();
	} else if (strcmp(*argv, "-p") == 0) {
	    log(LOG_WARNING, 0, "disabling pruning is no longer supported");
#ifdef SNMP
   } else if (strcmp(*argv, "-P") == 0) {
	    if (argc > 1 && isdigit(*(argv + 1)[0])) {
		argv++, argc--;
		dest_port = atoi(*argv);
	    } else
		dest_port = DEFAULT_PORT;
#endif
	} else
	    usage();
	argv++, argc--;
    }

    if (argc > 0)
	usage();

    if (debug != 0) {
	struct debugname *d;
	char c;
	int tmpd = debug;

	fprintf(stderr, "debug level 0x%x ", debug);
	c = '(';
	for (d = debugnames; d < debugnames +
			sizeof(debugnames) / sizeof(debugnames[0]); d++) {
	    if ((tmpd & d->level) == d->level) {
		tmpd &= ~d->level;
		fprintf(stderr, "%c%s", c, d->name);
		c = ',';
	    }
	}
	fprintf(stderr, ")\n");
    }

#ifdef LOG_DAEMON
    (void)openlog("mrouted", LOG_PID, LOG_DAEMON);
    (void)setlogmask(LOG_UPTO(LOG_NOTICE));
#else
    (void)openlog("mrouted", LOG_PID);
#endif
    sprintf(versionstring, "mrouted version %s", todaysversion);

    log(LOG_DEBUG, 0, "%s starting", versionstring);

#ifdef SYSV
    srand48(time(NULL));
#endif

    /*
     * Get generation id 
     */
    gettimeofday(&tv, 0);
    dvmrp_genid = tv.tv_sec;

    fp = fopen(genidfilename, "r");
    if (fp != NULL) {
	fscanf(fp, "%d", &prev_genid);
	if (prev_genid == dvmrp_genid)
	    dvmrp_genid++;
	(void) fclose(fp);
    }

    fp = fopen(genidfilename, "w");
    if (fp != NULL) {
	fprintf(fp, "%d", dvmrp_genid);
	(void) fclose(fp);
    }

    /* Start up the log rate-limiter */
    resetlogging(NULL);

    callout_init();
    init_igmp();
    init_icmp();
    init_ipip();
    init_routes();
    init_ktable();
#ifndef OLD_KERNEL
    /*
     * Unfortunately, you can't k_get_version() unless you've
     * k_init_dvmrp()'d.  Now that we want to move the
     * k_init_dvmrp() to later in the initialization sequence,
     * we have to do the disgusting hack of initializing,
     * getting the version, then stopping the kernel multicast
     * forwarding.
     */
    k_init_dvmrp();
    vers = k_get_version();
    k_stop_dvmrp();
    /*XXX
     * This function must change whenever the kernel version changes
     */
    if ((((vers >> 8) & 0xff) != 3) ||
	 ((vers & 0xff) != 5))
	log(LOG_ERR, 0, "kernel (v%d.%d)/mrouted (v%d.%d) version mismatch",
		(vers >> 8) & 0xff, vers & 0xff,
		PROTOCOL_VERSION, MROUTED_VERSION);
#endif

#ifdef SNMP
    if (i = snmp_init())
       return i;

    gettimeofday(nvp, 0);
    if (nvp->tv_usec < 500000L){
   svp->tv_usec = nvp->tv_usec + 500000L;
   svp->tv_sec = nvp->tv_sec;
    } else {
   svp->tv_usec = nvp->tv_usec - 500000L;
   svp->tv_sec = nvp->tv_sec + 1;
    }
#endif /* SNMP */

    init_vifs();

#ifdef RSRR
    rsrr_init();
#endif /* RSRR */

    sa.sa_handler = handler;
    sa.sa_flags = 0;	/* Interrupt system calls */
    sigemptyset(&sa.sa_mask);
    sigaction(SIGHUP, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);

    FD_ZERO(&readers);
    FD_SET(igmp_socket, &readers);
    nfds = igmp_socket + 1;
    for (i = 0; i < nhandlers; i++) {
	FD_SET(ihandlers[i].fd, &readers);
	if (ihandlers[i].fd >= nfds)
	    nfds = ihandlers[i].fd + 1;
    }

    IF_DEBUG(DEBUG_IF)
	dump_vifs(stderr);
    IF_DEBUG(DEBUG_ROUTE)
	dump_routes(stderr);

    /* schedule first timer interrupt */
    timer_setTimer(1, fasttimer, NULL);
    timer_setTimer(TIMER_INTERVAL, timer, NULL);

    if (debug == 0) {
	/*
	 * Detach from the terminal
	 */
	int t;

	haveterminal = 0;
	if (fork()) exit(0);
	(void)close(0);
	(void)close(1);
	(void)close(2);
	(void)open("/", 0);
	(void)dup2(0, 1);
	(void)dup2(0, 2);
#if defined(SYSV) || defined(linux)
	(void)setpgrp();
#else
#ifdef TIOCNOTTY
	t = open(_PATH_TTY, 2);
	if (t >= 0) {
	    (void)ioctl(t, TIOCNOTTY, (char *)0);
	    (void)close(t);
	}
#else
	if (setsid() < 0)
	    perror("setsid");
#endif
#endif
    }

    fp = fopen(pidfilename, "w");		
    if (fp != NULL) {
	fprintf(fp, "%d\n", (int)getpid());
	(void) fclose(fp);
    }

    /* XXX HACK
     * This will cause black holes for the first few seconds after startup,
     * since we are exchanging routes but not actually forwarding.
     * However, it eliminates much of the startup transient.
     *
     * It's possible that we can set a flag which says not to report any
     * routes (just accept reports) until this timer fires, and then
     * do a report_to_all_neighbors(ALL_ROUTES) immediately before
     * turning on DVMRP.
     */
    timer_setTimer(10, final_init, NULL);

    /*
     * Main receive loop.
     */
    dummy = 0;
    difftime.tv_usec = 0;
    gettimeofday(&curtime, NULL);
    lasttime = curtime;
    for(;;) {
	bcopy((char *)&readers, (char *)&rfds, sizeof(rfds));
	secs = timer_nextTimer();
	if (secs == -1)
	    timeout = NULL;
	else {
	    timeout = &tv;
	    timeout->tv_sec = secs;
	    timeout->tv_usec = 0;
	}
#ifdef SNMP
   THIS IS BROKEN
   if (nvp->tv_sec > svp->tv_sec
       || (nvp->tv_sec == svp->tv_sec && nvp->tv_usec > svp->tv_usec)){
       alarmTimer(nvp);
       eventTimer(nvp);
       if (nvp->tv_usec < 500000L){
      svp->tv_usec = nvp->tv_usec + 500000L;
      svp->tv_sec = nvp->tv_sec;
       } else {
      svp->tv_usec = nvp->tv_usec - 500000L;
      svp->tv_sec = nvp->tv_sec + 1;
       }
   }

	tvp =  &timeout;
	tvp->tv_sec = 0;
	tvp->tv_usec = 500000L;

	block = 0;
	snmp_select_info(&nfds, &rfds, tvp, &block);
	if (block == 1)
		tvp = NULL; /* block without timeout */
	if ((n = select(nfds, &rfds, NULL, NULL, tvp)) < 0) 
#endif
	if (sighandled) {
	    if (sighandled & GOT_SIGINT) {
		sighandled &= ~GOT_SIGINT;
		break;
	    }
	    if (sighandled & GOT_SIGHUP) {
		sighandled &= ~GOT_SIGHUP;
		restart();
	    }
	    if (sighandled & GOT_SIGUSR1) {
		sighandled &= ~GOT_SIGUSR1;
		fdump();
	    }
	    if (sighandled & GOT_SIGUSR2) {
		sighandled &= ~GOT_SIGUSR2;
		cdump();
	    }
	}
	if ((n = select(nfds, &rfds, NULL, NULL, timeout)) < 0) {
            if (errno != EINTR)
                log(LOG_WARNING, errno, "select failed");
            continue;
        }

	if (n > 0) {
	    if (FD_ISSET(igmp_socket, &rfds)) {
		recvlen = recvfrom(igmp_socket, recv_buf, RECV_BUF_SIZE,
				   0, NULL, &dummy);
		if (recvlen < 0) {
		    if (errno != EINTR) log(LOG_ERR, errno, "recvfrom");
		    continue;
		}
		accept_igmp(recvlen);
	    }

	    for (i = 0; i < nhandlers; i++) {
		if (FD_ISSET(ihandlers[i].fd, &rfds)) {
		    (*ihandlers[i].func)(ihandlers[i].fd, &rfds);
		}
	    }
	}

#ifdef SNMP
	THIS IS BROKEN
	snmp_read(&rfds); 
	snmp_timeout(); /* poll */
#endif
	/*
	 * Handle timeout queue.
	 *
	 * If select + packet processing took more than 1 second,
	 * or if there is a timeout pending, age the timeout queue.
	 *
	 * If not, collect usec in difftime to make sure that the
	 * time doesn't drift too badly.
	 *
	 * If the timeout handlers took more than 1 second,
	 * age the timeout queue again.  XXX This introduces the
	 * potential for infinite loops!
	 */
	do {
	    /*
	     * If the select timed out, then there's no other
	     * activity to account for and we don't need to
	     * call gettimeofday.
	     */
	    if (n == 0) {
		curtime.tv_sec = lasttime.tv_sec + secs;
		curtime.tv_usec = lasttime.tv_usec;
		n = -1;	/* don't do this next time through the loop */
	    } else
		gettimeofday(&curtime, NULL);
	    difftime.tv_sec = curtime.tv_sec - lasttime.tv_sec;
	    difftime.tv_usec += curtime.tv_usec - lasttime.tv_usec;
	    while (difftime.tv_usec >= 1000000) {
		difftime.tv_sec++;
		difftime.tv_usec -= 1000000;
	    }
	    if (difftime.tv_usec < 0) {
		difftime.tv_sec--;
		difftime.tv_usec += 1000000;
	    }
	    lasttime = curtime;
	    if (secs == 0 || difftime.tv_sec > 0)
		age_callout_queue(difftime.tv_sec);
	    secs = -1;
	} while (difftime.tv_sec > 0);
    }
    log(LOG_NOTICE, 0, "%s exiting", versionstring);
    cleanup();
    exit(0);
}

static void
usage()
{
	fprintf(stderr,
		"usage: mrouted [-p] [-c configfile] [-d [debug_level]]\n");
	exit(1);
}

static void
final_init(i)
    void *i;
{
    char *s = (char *)i;

    log(LOG_NOTICE, 0, "%s%s", versionstring, s ? s : "");
    if (s)
	free(s);

    k_init_dvmrp();		/* enable DVMRP routing in kernel */

    /*
     * Install the vifs in the kernel as late as possible in the
     * initialization sequence.
     */
    init_installvifs();

    time(&mrouted_init_time);
    did_final_init = 1;
}

/*
 * routine invoked every second.  Its main goal is to cycle through
 * the routing table and send partial updates to all neighbors at a
 * rate that will cause the entire table to be sent in ROUTE_REPORT_INTERVAL
 * seconds.  Also, every TIMER_INTERVAL seconds it calls timer() to
 * do all the other time-based processing.
 */
static void
fasttimer(i)
    void *i;
{
    static unsigned int tlast;
    static unsigned int nsent;
    register unsigned int t = tlast + 1;
    register int n;

    /*
     * if we're in the last second, send everything that's left.
     * otherwise send at least the fraction we should have sent by now.
     */
    if (t >= ROUTE_REPORT_INTERVAL) {
	register int nleft = nroutes - nsent;
	while (nleft > 0) {
	    if ((n = report_next_chunk()) <= 0)
		break;
	    nleft -= n;
	}
	tlast = 0;
	nsent = 0;
    } else {
	register unsigned int ncum = nroutes * t / ROUTE_REPORT_INTERVAL;
	while (nsent < ncum) {
	    if ((n = report_next_chunk()) <= 0)
		break;
	    nsent += n;
	}
	tlast = t;
    }

    timer_setTimer(1, fasttimer, NULL);
}

/*
 * The 'virtual_time' variable is initialized to a value that will cause the
 * first invocation of timer() to send a probe or route report to all vifs
 * and send group membership queries to all subnets for which this router is
 * querier.  This first invocation occurs approximately TIMER_INTERVAL seconds
 * after the router starts up.   Note that probes for neighbors and queries
 * for group memberships are also sent at start-up time, as part of initial-
 * ization.  This repetition after a short interval is desirable for quickly
 * building up topology and membership information in the presence of possible
 * packet loss.
 *
 * 'virtual_time' advances at a rate that is only a crude approximation of
 * real time, because it does not take into account any time spent processing,
 * and because the timer intervals are sometimes shrunk by a random amount to
 * avoid unwanted synchronization with other routers.
 */

u_long virtual_time = 0;


/*
 * Timer routine.  Performs periodic neighbor probing, route reporting, and
 * group querying duties, and drives various timers in routing entries and
 * virtual interface data structures.
 */
static void
timer(i)
    void *i;
{
    age_routes();	/* Advance the timers in the route entries     */
    age_vifs();		/* Advance the timers for neighbors */
    age_table_entry();	/* Advance the timers for the cache entries */

    if (virtual_time % IGMP_QUERY_INTERVAL == 0) {
	/*
	 * Time to query the local group memberships on all subnets
	 * for which this router is the elected querier.
	 */
	query_groups();
    }

    if (virtual_time % NEIGHBOR_PROBE_INTERVAL == 0) {
	/*
	 * Time to send a probe on all vifs from which no neighbors have
	 * been heard.  Also, check if any inoperative interfaces have now
	 * come up.  (If they have, they will also be probed as part of
	 * their initialization.)
	 */
	probe_for_neighbors();

	if (vifs_down)
	    check_vif_state();
    }

    delay_change_reports = FALSE;
    if (routes_changed) {
	/*
	 * Some routes have changed since the last timer interrupt, but
	 * have not been reported yet.  Report the changed routes to all
	 * neighbors.
	 */
	report_to_all_neighbors(CHANGED_ROUTES);
    }

#ifdef SNMP
    sync_timer();
#endif

    /*
     * Advance virtual time
     */
    virtual_time += TIMER_INTERVAL;
    timer_setTimer(TIMER_INTERVAL, timer, NULL);
}


static void
cleanup()
{
    static in_cleanup = 0;

    if (!in_cleanup) {
	in_cleanup++;
#ifdef RSRR
	rsrr_clean();
#endif /* RSRR */
	expire_all_routes();
	report_to_all_neighbors(ALL_ROUTES);
	if (did_final_init)
	    k_stop_dvmrp();
    }
}

/*
 * Signal handler.  Take note of the fact that the signal arrived
 * so that the main loop can take care of it.
 */
static void
handler(sig)
    int sig;
{
    switch (sig) {
	case SIGINT:
	case SIGTERM:
	    sighandled |= GOT_SIGINT;
	    break;

	case SIGHUP:
	    sighandled |= GOT_SIGHUP;
	    break;

	case SIGUSR1:
	    sighandled |= GOT_SIGUSR1;
	    break;

	case SIGUSR2:
	    sighandled |= GOT_SIGUSR2;
	    break;
    }
}

/*
 * Dump internal data structures to stderr.
 */
static void
dump()
{
    dump_vifs(stderr);
    dump_routes(stderr);
}

static void
dump_version(fp)
    FILE *fp;
{
    time_t t;

    time(&t);
    fprintf(fp, "%s ", versionstring);
    if (did_final_init)
	    fprintf(fp, "up %s",
		    scaletime(t - mrouted_init_time));
    else
	    fprintf(fp, "(not yet initialized)");
    fprintf(fp, " %s\n", ctime(&t));
}

/*
 * Dump internal data structures to a file.
 */
static void
fdump()
{
    FILE *fp;

    fp = fopen(dumpfilename, "w");
    if (fp != NULL) {
	dump_version(fp);
	dump_vifs(fp);
	dump_routes(fp);
	(void) fclose(fp);
    }
}


/*
 * Dump local cache contents to a file.
 */
static void
cdump()
{
    FILE *fp;

    fp = fopen(cachefilename, "w");
    if (fp != NULL) {
	dump_version(fp);
	dump_cache(fp); 
	(void) fclose(fp);
    }
}


/*
 * Restart mrouted
 */
static void
restart()
{
    char *s;

    s = (char *)malloc(sizeof(" restart"));
    if (s == NULL)
	log(LOG_ERR, 0, "out of memory");
    strcpy(s, " restart");

    /*
     * reset all the entries
     */
    free_all_prunes();
    free_all_routes();
    free_all_callouts();
    stop_all_vifs();
    k_stop_dvmrp();
    close(igmp_socket);
    close(udp_socket);
    did_final_init = 0;

    /*
     * start processing again
     */
    dvmrp_genid++;

    init_igmp();
    init_routes();
    init_ktable();
    init_vifs();
    /*XXX Schedule final_init() as main does? */
    final_init(s);

    /* schedule timer interrupts */
    timer_setTimer(1, fasttimer, NULL);
    timer_setTimer(TIMER_INTERVAL, timer, NULL);
}

#define LOG_MAX_MSGS	20	/* if > 20/minute then shut up for a while */
#define LOG_SHUT_UP	600	/* shut up for 10 minutes */
static int log_nmsgs = 0;

static void
resetlogging(arg)
    void *arg;
{
    int nxttime = 60;
    void *narg = NULL;

    if (arg == NULL && log_nmsgs > LOG_MAX_MSGS) {
	nxttime = LOG_SHUT_UP;
	narg = (void *)&log_nmsgs;	/* just need some valid void * */
	syslog(LOG_WARNING, "logging too fast, shutting up for %d minutes",
			LOG_SHUT_UP / 60);
    } else {
	log_nmsgs = 0;
    }

    timer_setTimer(nxttime, resetlogging, narg);
}

char *
scaletime(t)
    u_long t;
{
#define SCALETIMEBUFLEN 20
    static char buf1[20];
    static char buf2[20];
    static char *buf = buf1;
    char *p;

    p = buf;
    if (buf == buf1)
	buf = buf2;
    else
	buf = buf1;

    /* XXX snprintf */
    sprintf(p, "%2ld:%02ld:%02ld", t / 3600, (t % 3600) / 60, t % 60);
    p[SCALETIMEBUFLEN - 1] = '\0';
    return p;
}

#ifdef RINGBUFFER
#define NLOGMSGS 10000
#define LOGMSGSIZE 200
char *logmsg[NLOGMSGS];
static int logmsgno = 0;

void
printringbuf()
{
    FILE *f;
    int i;

    f = fopen("/var/tmp/mrouted.log", "a");
    if (f == NULL) {
	log(LOG_ERR, errno, "can't open /var/tmp/mrouted.log");
	/*NOTREACHED*/
    }
    fprintf(f, "--------------------------------------------\n");

    i = (logmsgno + 1) % NLOGMSGS;

    while (i != logmsgno) {
	if (*logmsg[i]) {
	    fprintf(f, "%s\n", logmsg[i]);
	    *logmsg[i] = '\0';
	}
	i = (i + 1) % NLOGMSGS;
    }

    fclose(f);
}
#endif

/*
 * Log errors and other messages to the system log daemon and to stderr,
 * according to the severity of the message and the current debug level.
 * For errors of severity LOG_ERR or worse, terminate the program.
 */
#ifdef __STDC__
void
log(int severity, int syserr, char *format, ...)
{
    va_list ap;
    static char fmt[211] = "warning - ";
    char *msg;
    struct timeval now;
    time_t now_sec;
    struct tm *thyme;
#ifdef RINGBUFFER
    static int ringbufinit = 0;
#endif

    va_start(ap, format);
#else
/*VARARGS3*/
void
log(severity, syserr, format, va_alist)
    int severity, syserr;
    char *format;
    va_dcl
{
    va_list ap;
    static char fmt[311] = "warning - ";
    char *msg;
    char tbuf[20];
    struct timeval now;
    time_t now_sec;
    struct tm *thyme;
#ifdef RINGBUFFER
    static int ringbufinit = 0;
#endif

    va_start(ap);
#endif
    vsnprintf(&fmt[10], sizeof(fmt) - 10, format, ap);
    va_end(ap);
    msg = (severity == LOG_WARNING) ? fmt : &fmt[10];

#ifdef RINGBUFFER
    if (!ringbufinit) {
	int i;

	for (i = 0; i < NLOGMSGS; i++) {
	    logmsg[i] = malloc(LOGMSGSIZE);
	    if (logmsg[i] == 0) {
		syslog(LOG_ERR, "out of memory");
		exit(-1);
	    }
	    *logmsg[i] = 0;
	}
	ringbufinit = 1;
    }
    gettimeofday(&now,NULL);
    now_sec = now.tv_sec;
    thyme = localtime(&now_sec);
    snprintf(logmsg[logmsgno++], LOGMSGSIZE, "%02d:%02d:%02d.%03ld %s err %d",
		    thyme->tm_hour, thyme->tm_min, thyme->tm_sec,
		    now.tv_usec / 1000, msg, syserr);
    logmsgno %= NLOGMSGS;
    if (severity <= LOG_NOTICE)
#endif
    /*
     * Log to stderr if we haven't forked yet and it's a warning or worse,
     * or if we're debugging.
     */
    if (haveterminal && (debug || severity <= LOG_WARNING)) {
	gettimeofday(&now,NULL);
	now_sec = now.tv_sec;
	thyme = localtime(&now_sec);
	if (!debug)
	    fprintf(stderr, "%s: ", progname);
	fprintf(stderr, "%02d:%02d:%02d.%03ld %s", thyme->tm_hour,
		    thyme->tm_min, thyme->tm_sec, now.tv_usec / 1000, msg);
	if (syserr == 0)
	    fprintf(stderr, "\n");
	else if (syserr < sys_nerr)
	    fprintf(stderr, ": %s\n", sys_errlist[syserr]);
	else
	    fprintf(stderr, ": errno %d\n", syserr);
    }

    /*
     * Always log things that are worse than warnings, no matter what
     * the log_nmsgs rate limiter says.
     * Only count things worse than debugging in the rate limiter
     * (since if you put daemon.debug in syslog.conf you probably
     * actually want to log the debugging messages so they shouldn't
     * be rate-limited)
     */
    if ((severity < LOG_WARNING) || (log_nmsgs < LOG_MAX_MSGS)) {
	if (severity < LOG_DEBUG)
	    log_nmsgs++;
	if (syserr != 0) {
	    errno = syserr;
	    syslog(severity, "%s: %m", msg);
	} else
	    syslog(severity, "%s", msg);
    }

    if (severity <= LOG_ERR) exit(-1);
}

#ifdef DEBUG_MFC
void
md_log(what, origin, mcastgrp)
    int what;
    u_int32 origin, mcastgrp;
{
    static FILE *f = NULL;
    struct timeval tv;
    u_int32 buf[4];

    if (!f) {
	if ((f = fopen("/tmp/mrouted.clog", "w")) == NULL) {
	    log(LOG_ERR, errno, "open /tmp/mrouted.clog");
	}
    }

    gettimeofday(&tv, NULL);
    buf[0] = tv.tv_sec;
    buf[1] = what;
    buf[2] = origin;
    buf[3] = mcastgrp;

    fwrite(buf, sizeof(u_int32), 4, f);
}
#endif
