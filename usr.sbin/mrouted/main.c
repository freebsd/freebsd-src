/*
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE".  Use of the mrouted program represents acceptance of
 * the terms and conditions listed in that file.
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 *
 * $Id: main.c,v 1.1.1.1 1994/05/17 20:59:33 jkh Exp $
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

static char pidfilename[]  = "/etc/mrouted.pid";
static char dumpfilename[] = "/usr/tmp/mrouted.dump";

static int debug = 0;


/*
 * Forward declarations.
 */
static void timer();
static void hup();
static void dump();
static void fdump();


main(argc, argv)
    int argc;
    char *argv[];
{
    register int recvlen;
    register int omask;
    int dummy;
    FILE *fp;
    extern uid_t geteuid();

    setlinebuf(stderr);

    if (geteuid() != 0) {
	fprintf(stderr, "mrouted: must be root\n");
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
	} else
	    goto usage;
	argv++, argc--;
    }

    if (argc > 0) {
usage:	fprintf(stderr, "usage: mrouted [-c configfile] [-d [debug_level]]\n");
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
    else fprintf(stderr, "debug level %u\n", debug);

#ifdef LOG_DAEMON
    (void)openlog("mrouted", LOG_PID, LOG_DAEMON);
    (void)setlogmask(LOG_UPTO(LOG_NOTICE));
#else
    (void)openlog("mrouted", LOG_PID);
#endif
    log(LOG_NOTICE, 0, "mrouted version %d.%d",
			PROTOCOL_VERSION, MROUTED_VERSION);

    fp = fopen(pidfilename, "w");
    if (fp != NULL) {
	fprintf(fp, "%d\n", getpid());
	(void) fclose(fp);
    }

    srandom(gethostid());

    init_igmp();
    k_init_dvmrp();		/* enable DVMRP routing in kernel */
    init_routes();
    init_vifs();

    if (debug >= 2) dump();

    (void)signal(SIGALRM, timer);
    (void)signal(SIGHUP,  hup);
    (void)signal(SIGTERM, hup);
    (void)signal(SIGINT,  hup);
    (void)signal(SIGUSR1, fdump);
    if (debug != 0)
	(void)signal(SIGQUIT, dump);

    (void)alarm(TIMER_INTERVAL);	 /* schedule first timer interrupt */

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
    int next_interval;

    age_routes();	/* Advance the timers in the route entries     */
    age_vifs();		/* Advance the timers for neighbors and groups */

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
    next_interval = TIMER_INTERVAL;

    if (virtual_time % ROUTE_REPORT_INTERVAL == 0) {
	/*
	 * Time for the periodic report of all routes to all neighbors.
	 */
	report_to_all_neighbors(ALL_ROUTES);

	/*
	 * Schedule the next timer interrupt for a random time between
	 * 1 and TIMER_INTERVAL seconds from now.  This randomization is
	 * intended to counteract the undesirable synchronizing tendency
	 * of periodic transmissions from multiple sources.
	 */
	next_interval = (random() % TIMER_INTERVAL) + 1;
    }
    else if (routes_changed) {
	/*
	 * Some routes have changed since the last timer interrupt, but
	 * have not been reported yet.  Report the changed routes to all
	 * neighbors.
	 */
	report_to_all_neighbors(CHANGED_ROUTES);
    }

    /*
     * Advance virtual time and schedule the next timer interrupt.
     */
    virtual_time += TIMER_INTERVAL;
    (void)alarm(next_interval);
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
