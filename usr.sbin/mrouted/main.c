/*
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE".  Use of the mrouted program represents acceptance of
 * the terms and conditions listed in that file.
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 *
 * $Id: main.c,v 3.6 1995/06/25 18:58:06 fenner Exp $
 */

/*
 * Written by Steve Deering, Stanford University, February 1989.
 *
 * (An earlier version of DVMRP was implemented by David Waitzman of
 *  BBN STC by extending Berkeley's routed program.  Some of Waitzman's
 *  extensions have been incorporated into mrouted, but none of the
 *  original routed code has been adopted.)
 */


#include "defs.h"
#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include <fcntl.h>

#ifdef SNMP
#include "snmp.h"
#endif

extern char *configfilename;

static char pidfilename[]  = _PATH_MROUTED_PID;
static char dumpfilename[] = _PATH_MROUTED_DUMP;
static char cachefilename[] = _PATH_MROUTED_CACHE;
static char genidfilename[] = _PATH_MROUTED_GENID;

int cache_lifetime 	= DEFAULT_CACHE_LIFETIME;
int max_prune_lifetime 	= DEFAULT_CACHE_LIFETIME * 2;

int debug = 0;
u_char pruning = 1;	/* Enable pruning by default */

#define NHANDLERS	2

static struct ihandler {
    int fd;			/* File descriptor		 */
    ihfunc_t func;		/* Function to call with &fd_set */
} ihandlers[NHANDLERS];
static int nhandlers = 0;

/*
 * Forward declarations.
 */
static void fasttimer __P((int));
static void done __P((int));
static void dump __P((int));
static void fdump __P((int));
static void cdump __P((int));
static void restart __P((int));
static void timer __P((void));
static void cleanup __P((void));

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
    register int omask;
    int dummy;
    FILE *fp;
    struct timeval tv;
    u_int32 prev_genid;
    int vers;
    fd_set rfds, readers;
    int nfds, n, i;
#ifdef SNMP
    char *myname;
    fd_set wfds;
  

    if (myname = strrchr(argv[0], '/'))
        myname++;
    if (myname == NULL || *myname == 0)
        myname = argv[0];
    isodetailor (myname, 0);
#endif

#ifdef SYSV
    setvbuf(stderr, NULL, _IOLBF, 0);
#else
    setlinebuf(stderr);
#endif

    if (geteuid() != 0) {
	fprintf(stderr, "must be root\n");
	exit(1);
    }

    argv++, argc--;
    while (argc > 0 && *argv[0] == '-') {
	if (strcmp(*argv, "-d") == 0) {
	    if (argc > 1 && isdigit(*(argv + 1)[0])) {
		argv++, argc--;
		debug = atoi(*argv);
	    } else
		debug = DEFAULT_DEBUG;
	} else if (strcmp(*argv, "-c") == 0) {
	    if (argc > 1) {
		argv++, argc--;
		configfilename = *argv;
	    } else
		goto usage;
	} else if (strcmp(*argv, "-p") == 0) {
	    pruning = 0;
	} else
	    goto usage;
	argv++, argc--;
    }

    if (argc > 0) {
usage:	fprintf(stderr, 
		"usage: mrouted [-p] [-c configfile] [-d [debug_level]]\n");
	exit(1);
    }

    if (debug == 0) {
	/*
	 * Detach from the terminal
	 */
	int t;

	if (fork()) exit(0);
	(void)close(0);
	(void)close(1);
	(void)close(2);
	(void)open("/", 0);
	(void)dup2(0, 1);
	(void)dup2(0, 2);
#ifdef TIOCNOTTY
	t = open("/dev/tty", 2);
	if (t >= 0) {
	    (void)ioctl(t, TIOCNOTTY, (char *)0);
	    (void)close(t);
	}
#else
	if (setsid() < 0)
	    perror("setsid");
#endif
    }
    else
	fprintf(stderr, "debug level %u\n", debug);

#ifdef LOG_DAEMON
    (void)openlog("mrouted", LOG_PID, LOG_DAEMON);
    (void)setlogmask(LOG_UPTO(LOG_NOTICE));
#else
    (void)openlog("mrouted", LOG_PID);
#endif
    log(LOG_NOTICE, 0, "mrouted version %d.%d",
			PROTOCOL_VERSION, MROUTED_VERSION);

#ifdef SYSV
    srand48(time(NULL));
#else
    srandom(gethostid());
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

    callout_init();

#ifdef SNMP
    snmp_init();
#endif

    init_igmp();
    k_init_dvmrp();		/* enable DVMRP routing in kernel */

#ifndef OLD_KERNEL
    vers = k_get_version();
    /*XXX
     * This function must change whenever the kernel version changes
     */
    if ((((vers >> 8) & 0xff) != 3) ||
	 ((vers & 0xff) != 5))
	log(LOG_ERR, 0, "kernel (v%d.%d)/mrouted (v%d.%d) version mismatch",
		(vers >> 8) & 0xff, vers & 0xff,
		PROTOCOL_VERSION, MROUTED_VERSION);
#endif

    init_routes();
    init_ktable();
    init_vifs();
#ifdef RSRR
    rsrr_init();
#endif /* RSRR */

#if defined(__STDC__) || defined(__GNUC__)
    /* Allow cleanup if unexpected exit.  Apparently some architectures
     * have a kernel bug where closing the socket doesn't do an
     * ip_mrouter_done(), so we attempt to do it on exit.
     */
    atexit(cleanup);
#endif

    if (debug)
	fprintf(stderr, "pruning %s\n", pruning ? "on" : "off");

    fp = fopen(pidfilename, "w");		
    if (fp != NULL) {
	fprintf(fp, "%d\n", (int)getpid());
	(void) fclose(fp);
    }

    if (debug >= 2) dump(0);

    (void)signal(SIGALRM, fasttimer);

    (void)signal(SIGHUP,  restart);
    (void)signal(SIGTERM, done);
    (void)signal(SIGINT,  done);
    (void)signal(SIGUSR1, fdump);
    (void)signal(SIGUSR2, cdump);
    if (debug != 0)
	(void)signal(SIGQUIT, dump);

    FD_ZERO(&readers);
    FD_SET(igmp_socket, &readers);
    nfds = igmp_socket + 1;
    for (i = 0; i < nhandlers; i++) {
	FD_SET(ihandlers[i].fd, &readers);
	if (ihandlers[i].fd >= nfds)
	    nfds = ihandlers[i].fd + 1;
    }

    (void)alarm(1);	 /* schedule first timer interrupt */

    /*
     * Main receive loop.
     */
    dummy = 0;
    for(;;) {
	bcopy((char *)&readers, (char *)&rfds, sizeof(rfds));
#ifdef SNMP
        FD_ZERO(&wfds);
  
        if (smux_fd != NOTOK) {
           if (rock_and_roll)
              FD_SET(smux_fd, &rfds);
           else
              FD_SET(smux_fd, &wfds);
           if (smux_fd >= nfds)
              nfds = smux_fd + 1;
        }
  
        if ((n = xselect(nfds, &rfds, &wfds, NULLFD, NOTOK))==NOTOK) {
#else
	if ((n = select(nfds, &rfds, NULL, NULL, NULL)) < 0) {
#endif
            if (errno != EINTR) /* SIGALRM is expected */
                log(LOG_WARNING, errno, "select failed");
            continue;
        }

	if (FD_ISSET(igmp_socket, &rfds)) {
	    recvlen = recvfrom(igmp_socket, recv_buf, RECV_BUF_SIZE,
			       0, NULL, &dummy);
	    if (recvlen < 0) {
		if (errno != EINTR) log(LOG_ERR, errno, "recvfrom");
		continue;
	    }
	    omask = sigblock(sigmask(SIGALRM));
	    accept_igmp(recvlen);
	    (void)sigsetmask(omask);
        }

	for (i = 0; i < nhandlers; i++) {
	    if (FD_ISSET(ihandlers[i].fd, &rfds)) {
		(*ihandlers[i].func)(&rfds);
	    }
	}

#ifdef SNMP
        if (smux_fd != NOTOK) {
            if (rock_and_roll) {
		if (FD_ISSET(smux_fd, &rfds))
		    doit_smux();
	    } else if (FD_ISSET(smux_fd, &wfds)) 
                start_smux();
        }
#endif
    }
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
    int i;
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
    if ((t % TIMER_INTERVAL) == 0)
	timer();

    age_callout_queue();/* Advance the timer for the callout queue
				for groups */	
    alarm(1);
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

static u_long virtual_time = 0;


/*
 * Timer routine.  Performs periodic neighbor probing, route reporting, and
 * group querying duties, and drives various timers in routing entries and
 * virtual interface data structures.
 */
static void
timer()
{
    age_routes();	/* Advance the timers in the route entries     */
    age_vifs();		/* Advance the timers for neighbors */
    age_table_entry();	/* Advance the timers for the cache entries */

    if (virtual_time % GROUP_QUERY_INTERVAL == 0) {
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
    if (smux_fd == NOTOK && !dont_bother_anymore
		 && virtual_time % SNMPD_RETRY_INTERVAL == 0) {
	/*
	 * Time to check for snmpd running.
	 */
        try_smux_init();
    }
#endif

    /*
     * Advance virtual time
     */
    virtual_time += TIMER_INTERVAL;
}


/*
 * On termination, let everyone know we're going away.
 */
static void
done(i)
    int i;
{
    log(LOG_NOTICE, 0, "mrouted version %d.%d exiting",
			PROTOCOL_VERSION, MROUTED_VERSION);
    cleanup();
    _exit(1);
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
	k_stop_dvmrp();
    }
}


/*
 * Dump internal data structures to stderr.
 */
static void
dump(i)
    int i;
{
    dump_vifs(stderr);
    dump_routes(stderr);
}


/*
 * Dump internal data structures to a file.
 */
static void
fdump(i)
    int i;
{
    FILE *fp;

    fp = fopen(dumpfilename, "w");
    if (fp != NULL) {
	dump_vifs(fp);
	dump_routes(fp);
	(void) fclose(fp);
    }
}


/*
 * Dump local cache contents to a file.
 */
static void
cdump(i)
    int i;
{
    FILE *fp;

    fp = fopen(cachefilename, "w");
    if (fp != NULL) {
	dump_cache(fp); 
	(void) fclose(fp);
    }
}


/*
 * Restart mrouted
 */
static void
restart(i)
    int i;
{
    register int omask;

    log(LOG_NOTICE, 0, "mrouted version %d.%d restart",
			PROTOCOL_VERSION, MROUTED_VERSION);

    /*
     * reset all the entries
     */
    omask = sigblock(sigmask(SIGALRM));
    free_all_prunes();
    free_all_routes();
    stop_all_vifs();
    k_stop_dvmrp();
    close(igmp_socket);
    close(udp_socket);

    /*
     * start processing again
     */
    dvmrp_genid++;
    pruning = 1;

    init_igmp();
    k_init_dvmrp();		/* enable DVMRP routing in kernel */
    init_routes();
    init_ktable();
    init_vifs();

    (void)sigsetmask(omask);
}


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
    char tbuf[20];
    struct timeval now;
    struct tm *thyme;

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
    static char fmt[211] = "warning - ";
    char *msg;
    char tbuf[20];
    struct timeval now;
    struct tm *thyme;

    va_start(ap);
#endif
    vsprintf(&fmt[10], format, ap);
    va_end(ap);
    msg = (severity == LOG_WARNING) ? fmt : &fmt[10];

    switch (debug) {
	case 0: break;
	case 1: if (severity > LOG_NOTICE) break;
	case 2: if (severity > LOG_INFO  ) break;
	default:
	    gettimeofday(&now,NULL);
	    thyme = localtime(&now.tv_sec);
	    strftime(tbuf, sizeof(tbuf), "%X.%%03d ", thyme);
	    fprintf(stderr, tbuf, now.tv_usec / 1000);
	    fprintf(stderr, "%s", msg);
	    if (syserr == 0)
		fprintf(stderr, "\n");
	    else if (syserr < sys_nerr)
		fprintf(stderr, ": %s\n", sys_errlist[syserr]);
	    else
		fprintf(stderr, ": errno %d\n", syserr);
    }

    if (severity <= LOG_NOTICE) {
	if (syserr != 0) {
	    errno = syserr;
	    syslog(severity, "%s: %m", msg);
	} else
	    syslog(severity, "%s", msg);

	if (severity <= LOG_ERR) exit(-1);
    }
}
