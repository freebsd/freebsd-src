/*
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE".  Use of the mrouted program represents acceptance of
 * the terms and conditions listed in that file.
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 *
 * $Id: main.c,v 1.2 1994/09/08 02:51:18 wollman Exp $
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

extern char *configfilename;

static char pidfilename[]  = "/var/run/mrouted.pid";
static char dumpfilename[] = "/var/tmp/mrouted.dump";
static char cachefilename[] = "/var/tmp/mrouted.cache";
static char genidfilename[] = "/var/tmp/mrouted.genid";

int cache_lifetime 	= DEFAULT_CACHE_LIFETIME;
int max_prune_lifetime 	= DEFAULT_CACHE_LIFETIME * 2;

int debug = 0;
u_char pruning = 1;	/* Enable pruning by default */


/*
 * Forward declarations.
 */
static void fasttimer();
static void timer();
static void hup();
static void dump();
static void fdump();
static void cdump();
static void restart();

main(argc, argv)
    int argc;
    char *argv[];
{
    register int recvlen;
    register int omask;
    int dummy;
    FILE *fp;
    extern uid_t geteuid();
    struct timeval tv;
    struct timezone tzp;
    u_long prev_genid;

    setlinebuf(stderr);

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
	t = open("/dev/tty", 2);
	if (t >= 0) {
	    (void)ioctl(t, TIOCNOTTY, (char *)0);
	    (void)close(t);
	}
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

    srandom(gethostid());

    /*
     * Get generation id 
     */
    gettimeofday(&tv, &tzp);
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
    init_igmp();
    k_init_dvmrp();		/* enable DVMRP routing in kernel */
    init_routes();
    init_ktable();
    init_vifs();

    if (debug)
	fprintf(stderr, "pruning %s\n", pruning ? "on" : "off");

    fp = fopen(pidfilename, "w");		
    if (fp != NULL) {
	fprintf(fp, "%d\n", getpid());
	(void) fclose(fp);
    }

    if (debug >= 2) dump();

    (void)signal(SIGALRM, fasttimer);

    (void)signal(SIGHUP,  restart);
    (void)signal(SIGTERM, hup);
    (void)signal(SIGINT,  hup);
    (void)signal(SIGUSR1, fdump);
    (void)signal(SIGUSR2, cdump);
    if (debug != 0)
	(void)signal(SIGQUIT, dump);

    (void)alarm(1);	 /* schedule first timer interrupt */

    /*
     * Main receive loop.
     */
    dummy = 0;
    for(;;) {
	recvlen = recvfrom(igmp_socket, recv_buf, sizeof(recv_buf),
			   0, NULL, &dummy);
	if (recvlen < 0) {	
	    if (errno != EINTR) log(LOG_ERR, errno, "recvfrom");
	    continue;
	}
	omask = sigblock(sigmask(SIGALRM));
	accept_igmp(recvlen);
	(void)sigsetmask(omask);
    }
}


/*
 * routine invoked every second.  It's main goal is to cycle through
 * the routing table and send partial updates to all neighbors at a
 * rate that will cause the entire table to be sent in ROUTE_REPORT_INTERVAL
 * seconds.  Also, every TIMER_INTERVAL seconds it calls timer() to
 * do all the other time-based processing.
 */
static void fasttimer()
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
static void timer()
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

    /*
     * Advance virtual time
     */
    virtual_time += TIMER_INTERVAL;
}


/*
 * On hangup signal, let everyone know we're going away.
 */
static void hup()
{
    log(LOG_INFO, 0, "hup");
    expire_all_routes();
    report_to_all_neighbors(ALL_ROUTES);
    exit(1);
}


/*
 * Dump internal data structures to stderr.
 */
static void dump()
{
    dump_vifs(stderr);
    dump_routes(stderr);
}


/*
 * Dump internal data structures to a file.
 */
static void fdump()
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
static void cdump()
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
static void restart()
{
    register int omask;

    log(LOG_INFO, 0, "restart");

    /*
     * reset all the entries
     */
    omask = sigblock(sigmask(SIGALRM));
    free_all_prunes();
    free_all_routes();
    stop_all_vifs();
    k_stop_dvmrp();

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
void log(severity, syserr, format, a, b, c, d, e)
    int severity, syserr;
    char *format;
    int a, b, c, d, e;
{
    char fmt[100];

    switch (debug) {
	case 0: break;
	case 1: if (severity > LOG_NOTICE) break;
	case 2: if (severity > LOG_INFO  ) break;
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

    if (severity <= LOG_NOTICE) {
	fmt[0] = '\0';
	if (severity == LOG_WARNING) strcat(fmt, "warning - ");
	strncat(fmt, format, 80);
	if (syserr != 0) {
		strcat(fmt, ": %m");
		errno = syserr;
	}
	syslog(severity, fmt, a, b, c, d, e);

	if (severity <= LOG_ERR) exit(-1);
    }
}
