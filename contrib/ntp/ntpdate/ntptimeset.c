/*
 * ntptimeset - get/set the time via ntp
 *
 * GOAL:
 * The goal of ntptime is to set the current time on system startup
 * to the best possible time using the network very wisely. It is assumed
 * that after a resonable time has been sett then ntp daemon will
 * maintain it.
 *
 * PROBLEM DOMAIN:
 * We have three sets of issues related to acheiving the goal. The first
 * issue is using the network when normal traffic is happening or when
 * the entire network world is recovering from a campus wide power failure
 * and is restarting. The second issue is the class of machine whether it
 * is a user's office workstation being handled by an uneducated user or
 * a server computer being handled by a trained operations staff. The third
 * issue is whether the ratio of people to computers and whether the 
 * environment is stable and viable or not.
 *
 * NETWORK USAGE:
 * The first issue of using the network wisely is a question of whether
 * the network load and time server load and state are normal. If things
 * are normal ntptime can do what ntpdate does of sending out 4 packets
 * quickly to each server (new transmit done with each ack). However
 * if network or time load is high then this scheme will simply contribute
 * to problems. Given we have minimal state, we simply weight lost packets
 * significantly and make sure we throttle output as much as possible
 * without performance lost for quick startups.
 *
 * TRAINING AND KNOWLEDGE:
 * The second issue of uneducated user of a office workstation versus a
 * trained operation staff of a server machine translates into simply an
 * issue of untrained and trained users.
 * 
 * The training issue implies that for the sake of the users involved in the
 * handling of their office workstation, problems and options should be
 * communicated simply and effectively and not in terse expert related
 * descriptions without possible options to be taken. The operator's training
 * and education enables them to deal with either type of communication and
 * control.
 *
 * AUTOMATION AND MANUAL CONTROL:
 * The last issue boils down to a design problem. If the design tends to go
 * into a manual mode when the environment is non-viable then one person
 * handling many computers all at the same time will be heavily impacted. On
 * the other hand, if the design tends to be automatic and does not indicate
 * a way for the user to take over control then the computer will be
 * unavailable for the user until the proble is resolved by someone else or
 * the user.
 *
 * NOTE: Please do not have this program print out every minute some line,
 *       of output. If this happens and the environment is in trouble then
 *       many pages of paper on many different machines will be filled up.
 *       Save some tress in your lifetime.
 * 
 * CONCLUSION:
 * The behavior of the program derived from these three issues should be
 * that during normal situations it quickly sets the time and allow the
 * system to startup.
 *
 * However during abnormal conditions as detected by unresponsive servers,
 * out-of-sync or bad responses and other detections, it should print out
 * a simple but clear message and continue in a mellow way to get the best
 * possible time. It may never get the time and if so should also indicate
 * this.
 *
 * Rudy Nedved
 * 18-May-1993
 *
 ****************************************************************
 *
 * Much of the above is confusing or no longer relevant.  For example,
 * it is rare these days for a machine's console to be a printing terminal,
 * so the comment about saving trees doesn't mean much.  Nonetheless,
 * the basic principles still stand:
 *
 * - Work automatically, without human control or intervention.  To
 *   this end, we use the same configuration file as ntpd itself, so
 *   you don't have to specify servers or other information on the
 *   command line.  We also recognize that sometimes we won't be able
 *   to contact any servers, and give up in that event instead of
 *   hanging forever.
 *
 * - Behave in a sane way, both internally and externally, even in the
 *   face of insane conditions.  That means we back off quickly when
 *   we don't hear a response, to avoid network congestion.  Like
 *   ntpd, we verify responses from several servers before accepting
 *   the new time data.
 *
 *   However, we don't assume that the local clock is right, or even
 *   close, because it might not be at boot time, and we want to catch
 *   and correct that situation.  This behaviour has saved us in several
 *   instances.  On HP-UX 9.0x, there used to be a bug in adjtimed which
 *   would cause the time to be set to some wild value, making the machine
 *   essentially unusable (we use Kerberos authentication pervasively,
 *   and it requires workstations and servers to have a time within five
 *   minutes of the Kerberos server).  We also have problems on PC's
 *   running both Linux and some Microsoft OS -- they tend to disagree
 *   on what the BIOS clock should say, and who should update it, and
 *   when.  On those systems, we not only run ntptimeset at boot, we
 *   also reset the BIOS clock based on the result, so the correct
 *   time will be retained across reboots.
 *
 * For these reasons, and others, we have continued to use this tool
 * rather than ntpdate.  It is run automatically at boot time on every
 * workstation and server in our facility.
 *
 * In the past, we called this program 'ntptime'.  Unfortunately, the
 * ntp v4 distribution also includes a program with that name.  In
 * order to avoid confusion, we have renamed our program 'ntptimeset',
 * which more accurately describes what it does.
 *
 * Jeffrey T. Hutzelman (N3NHS) <jhutz+@cmu.edu>
 * School of Computer Science - Research Computing Facility
 * Carnegie Mellon University - Pittsburgh, PA
 * 16-Aug-1999
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "ntp_fp.h"
#include "ntp.h"
#include "ntp_io.h"
#include "iosignal.h"
#include "ntp_unixtime.h"
#include "ntpdate.h"
#include "ntp_string.h"
#include "ntp_syslog.h"
#include "ntp_select.h"
#include "ntp_stdlib.h"

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#include <stdio.h>
#include <signal.h>
#include <ctype.h>
#ifndef SYS_WINNT
# include <netdb.h>
# include <sys/signal.h>
# include <sys/ioctl.h>
#endif /* SYS_WINNT */

#ifdef HAVE_SYS_RESOURCE_H
# include <sys/resource.h>
#endif /* HAVE_SYS_RESOURCE_H */

#ifdef SYS_VXWORKS
# include "ioLib.h"
# include "sockLib.h"
# include "timers.h"
#endif

#include "recvbuff.h"

#ifdef SYS_WINNT
# define TARGET_RESOLUTION 1  /* Try for 1-millisecond accuracy
				on Windows NT timers. */
#pragma comment(lib, "winmm")
#endif /* SYS_WINNT */

/*
 * Scheduling priority we run at
 */
#ifndef SYS_VXWORKS
# define	NTPDATE_PRIO	(-12)
#else
# define	NTPDATE_PRIO	(100)
#endif

#if defined(HAVE_TIMER_SETTIME) || defined (HAVE_TIMER_CREATE)
/* POSIX TIMERS - vxWorks doesn't have itimer - casey */
static timer_t ntpdate_timerid;
#endif

/*
 * Compatibility stuff for Version 2
 */
#define NTP_MAXSKW	0x28f	/* 0.01 sec in fp format */
#define NTP_MINDIST 0x51f	/* 0.02 sec in fp format */
#define NTP_INFIN	15	/* max stratum, infinity a la Bellman-Ford */
#define NTP_MAXWGT	(8*FP_SECOND)	/* maximum select weight 8 seconds */
#define NTP_MAXLIST 5	/* maximum select list size */
#define PEER_SHIFT	8	/* 8 suitable for crystal time base */

/*
 * Debugging flag
 */
volatile int debug = 0;

/*
 * File descriptor masks etc. for call to select
 */
int fd;
fd_set fdmask;

/*
 * Initializing flag.  All async routines watch this and only do their
 * thing when it is clear.
 */
int initializing = 1;

/*
 * Alarm flag.	Set when an alarm occurs
 */
volatile int alarm_flag = 0;

/*
 * Set the time if valid time determined
 */
int set_time = 0;

/*
 * transmission rate control
 */
#define	MINTRANSMITS	(3)	/* minimum total packets per server */
#define	MAXXMITCOUNT	(2)	/* maximum packets per time interrupt */

/*
 * time setting constraints
 */
#define	DESIREDDISP	(4*FP_SECOND)	/* desired dispersion, (fp 4) */
int max_period = DEFMAXPERIOD;
int min_servers = DEFMINSERVERS;
int min_valid = DEFMINVALID;

/*
 * counters related to time setting constraints
 */
int contacted = 0;		/* # of servers we have sent to */
int responding = 0;		/* servers responding */
int validcount = 0;		/* servers with valid time */
int valid_n_low = 0;		/* valid time servers with low dispersion */

/*
 * Unpriviledged port flag.
 */
int unpriv_port = 0;

/*
 * Program name.
 */
char *progname;

/*
 * Systemwide parameters and flags
 */
struct server **sys_servers;	/* the server list */
int sys_numservers = 0; 	/* number of servers to poll */
int sys_authenticate = 0;	/* true when authenticating */
u_int32 sys_authkey = 0;	/* set to authentication key in use */
u_long sys_authdelay = 0;	/* authentication delay */

/*
 * The current internal time
 */
u_long current_time = 0;

/*
 * File of encryption keys
 */

#ifndef KEYFILE
# ifndef SYS_WINNT
#define KEYFILE 	"/etc/ntp.keys"
# else
#define KEYFILE 	"%windir%\\ntp.keys"
# endif /* SYS_WINNT */
#endif /* KEYFILE */

#ifndef SYS_WINNT
const char *key_file = KEYFILE;
#else
char key_file_storage[MAX_PATH+1], *key_file ;
#endif	 /* SYS_WINNT */

/*
 * total packet counts
 */
u_long total_xmit = 0;
u_long total_recv = 0;

/*
 * Miscellaneous flags
 */
int verbose = 0;
#define	HORRIBLEOK	3	/* how many packets to let out */
int horrible = 0;	/* how many packets we drop for testing */
int secondhalf = 0;	/* second half of timeout period */
int printmsg = 0;	/* print time response analysis */

/*
 * The half time and finish time in internal time
 */
u_long half_time = 0;
u_long finish_time = 0;


int	ntptimesetmain	P((int argc, char *argv[]));
static	void	analysis	P((int final));
static	int	have_enough	P((void));
static	void	transmit	P((register struct server *server));
static	void	receive		P((struct recvbuf *rbufp));
static	void	clock_filter P((register struct server *server, s_fp d, l_fp *c));
static	void	clock_count	P((void));
static	struct server *clock_select P((void));
static	void	set_local_clock	P((void));
static	struct server *findserver P((struct sockaddr_in *addr));
static	void	timer		P((void));
#ifndef SYS_WINNT
static	RETSIGTYPE 	alarming	P((int sig));
#endif /* SYS_WINNT */
static	void	init_alarm	P((void));
static	void	init_io		P((void));
static	int	sendpkt		P((struct sockaddr_in *dest, struct pkt *pkt, int len));
	void 	input_handler	P((l_fp *xts));
static	void	printserver	P((register struct server *pp, FILE *fp));
#if !defined(HAVE_VSPRINTF)
int	vsprintf	P((char *str, const char *fmt, va_list ap));
#endif

#ifdef HAVE_SIGNALED_IO
extern  void    wait_for_signal P((void));
extern  void    unblock_io_and_alarm P((void));
extern  void    block_io_and_alarm P((void));
#endif


#ifdef NO_MAIN_ALLOWED
CALL(ntptimeset,"ntptimeset",ntptimesetmain);

void clear_globals()
{
  /*
   * Debugging flag
   */
  debug = 0;

  ntp_optind = 0;

  /*
   * Initializing flag.  All async routines watch this and only do their
   * thing when it is clear.
   */
  initializing = 1;

  /*
   * Alarm flag.  Set when an alarm occurs
   */
  alarm_flag = 0;

  /*
   * Unpriviledged port flag.
   */
  unpriv_port = 0;

  /*
   * Systemwide parameters and flags
   */
  sys_numservers = 0;	  /* number of servers to poll */
  sys_authenticate = 0;   /* true when authenticating */
  sys_authkey = 0;	   /* set to authentication key in use */
  sys_authdelay = 0;   /* authentication delay */

  /*
   * The current internal time
   */
  current_time = 0;

  verbose = 0;
}
#endif /* NO_MAIN_ALLOWED */

/*
 * Main program.  Initialize us and loop waiting for I/O and/or
 * timer expiries.
 */
#ifndef NO_MAIN_ALLOWED
int
main(
	int argc,
	char *argv[]
	)
{
	return ntptimesetmain(argc, argv);
}
#endif /* NO_MAIN_ALLOWED */
	   

int
ntptimesetmain(
	int argc,
	char *argv[]
	)
{
	int was_alarmed;
	struct recvbuf *rbuflist;
	struct recvbuf *rbuf;
	l_fp tmp;
	int errflg;
	int c;
	extern char *ntp_optarg;
	extern int ntp_optind;
	int ltmp;
	char *cfgpath;

#ifdef SYS_WINNT
	HANDLE process_handle;

	wVersionRequested = MAKEWORD(1,1);
	if (WSAStartup(wVersionRequested, &wsaData)) {
		msyslog(LOG_ERR, "No useable winsock.dll: %m");
		exit(1);
	}
#endif /* SYS_WINNT */

#ifdef NO_MAIN_ALLOWED
	clear_globals();
#endif

	errflg = 0;
	cfgpath = 0;
	progname = argv[0];
	syslogit = 0;

	/*
	 * Decode argument list
	 */
	while ((c = ntp_getopt(argc, argv, "a:c:de:slt:uvHS:V:")) != EOF)
		switch (c)
		{
		case 'a':
			c = atoi(ntp_optarg);
			sys_authenticate = 1;
			sys_authkey = c;
			break;
		case 'c':
			cfgpath = ntp_optarg;
			break;
		case 'd':
			++debug;
			break;
		case 'e':
			if (!atolfp(ntp_optarg, &tmp)
			    || tmp.l_ui != 0) {
				(void) fprintf(stderr,
				    "%s: encryption delay %s is unlikely\n",
				    progname, ntp_optarg);
				errflg++;
			} else {
				sys_authdelay = tmp.l_uf;
			}
			break;
		case 's':
			set_time = 1;
			break;
		case 'l':
			syslogit = 1;
			break;
		case 't':
			ltmp = atoi(ntp_optarg);
			if (ltmp <= 0) {
			    (void) fprintf(stderr,
				"%s: maximum time period (%d) is invalid\n",
				progname, ltmp);
			    errflg++;
			}
			else
			    max_period = ltmp;
			break;
		case 'u':
			unpriv_port = 1;
			break;
		case 'v':
			++verbose;
			break;
		case 'H':
			horrible++;
			break;
		case 'S':
			ltmp = atoi(ntp_optarg);
			if (ltmp <= 0) {
			    (void) fprintf(stderr,
				"%s: minimum responding (%d) is invalid\n",
				progname, ltmp);
			    errflg++;
			}
			else
			    min_servers = ltmp;
			break;
		case 'V':
			ltmp = atoi(ntp_optarg);
			if (ltmp <= 0) {
			    (void) fprintf(stderr,
				"%s: minimum valid (%d) is invalid\n",
				progname, ltmp);
			    errflg++;
			}
			else
			    min_valid = ltmp;
			break;
		case '?':
			++errflg;
			break;
		default:
			break;
		}

	
	if (errflg || ntp_optind < argc) {
		fprintf(stderr,"usage: %s [switches...]\n",progname);
		fprintf(stderr,"  -v       (verbose)\n");
		fprintf(stderr,"  -c path  (set config file path)\n");
		fprintf(stderr,"  -a key   (authenticate using key)\n");
		fprintf(stderr,"  -e delay (authentication delay)\n");
		fprintf(stderr,"  -S num   (# of servers that must respond)\n");
		fprintf(stderr,"  -V num   (# of servers that must valid)\n");
		fprintf(stderr,"  -s       (set the time based if okay)\n");
		fprintf(stderr,"  -t secs  (time period before ending)\n");
		fprintf(stderr,"  -l       (use syslog facility)\n");
		fprintf(stderr,"  -u       (use unprivileged port)\n");
		fprintf(stderr,"  -H       (drop packets for debugging)\n");
		fprintf(stderr,"  -d       (debug output)\n");
		exit(2);
	}

	/*
	 * Logging.  Open the syslog if we have to
	 */
	if (syslogit) {
#if !defined (SYS_WINNT) && !defined (SYS_VXWORKS) && !defined SYS_CYGWIN32
# ifndef	LOG_DAEMON
		openlog("ntptimeset", LOG_PID);
# else

#  ifndef	LOG_NTP
#	define	LOG_NTP LOG_DAEMON
#  endif
		openlog("ntptimeset", LOG_PID | LOG_NDELAY, LOG_NTP);
		if (debug)
			setlogmask(LOG_UPTO(LOG_DEBUG));
		else
			setlogmask(LOG_UPTO(LOG_INFO));
# endif /* LOG_DAEMON */
#endif	/* SYS_WINNT */
	}

	if (debug || verbose)
		msyslog(LOG_INFO, "%s", Version);

	if (horrible)
		msyslog(LOG_INFO, "Dropping %d out of %d packets",
			horrible,horrible+HORRIBLEOK);
	/*
	 * Add servers we are going to be polling
	 */
	loadservers(cfgpath);

	if (sys_numservers < min_servers) {
		msyslog(LOG_ERR, "Found %d servers, require %d servers",
			sys_numservers,min_servers);
		exit(2);
	}

	/*
	 * determine when we will end at least
 	 */
	finish_time = max_period * TIMER_HZ;
	half_time = finish_time >> 1;

	/*
	 * Initialize the time of day routines and the I/O subsystem
	 */
	if (sys_authenticate) {
		init_auth();
#ifdef SYS_WINNT
		if (!key_file) key_file = KEYFILE;
		if (!ExpandEnvironmentStrings(key_file, key_file_storage, MAX_PATH))
		{
			msyslog(LOG_ERR, "ExpandEnvironmentStrings(%s) failed: %m\n",
				key_file);
		} else {
			key_file = key_file_storage;
		}
#endif /* SYS_WINNT */

		if (!authreadkeys(key_file)) {
			msyslog(LOG_ERR, "no key file, exiting");
			exit(1);
		}
		if (!authistrusted(sys_authkey)) {
			char buf[10];

			(void) sprintf(buf, "%lu", (unsigned long)sys_authkey);
			msyslog(LOG_ERR, "authentication key %s unknown", buf);
			exit(1);
		}
	}
	init_io();
	init_alarm();

	/*
	 * Set the priority.
	 */
#ifdef SYS_VXWORKS
	taskPrioritySet( taskIdSelf(), NTPDATE_PRIO);
#endif
#if defined(HAVE_ATT_NICE)
	nice (NTPDATE_PRIO);
#endif
#if defined(HAVE_BSD_NICE)
	(void) setpriority(PRIO_PROCESS, 0, NTPDATE_PRIO);
#endif
#ifdef SYS_WINNT
	process_handle = GetCurrentProcess();
	if (!SetPriorityClass(process_handle, (DWORD) REALTIME_PRIORITY_CLASS)) {
		msyslog(LOG_ERR, "SetPriorityClass failed: %m");
	}
#endif /* SYS_WINNT */

	initializing = 0;

	/*
	 * Use select() on all on all input fd's for unlimited
	 * time.  select() will terminate on SIGALARM or on the
	 * reception of input.	Using select() means we can't do
	 * robust signal handling and we get a potential race
	 * between checking for alarms and doing the select().
	 * Mostly harmless, I think.
	 * Keep going until we have enough information, or time is up.
	 */
	/* On VMS, I suspect that select() can't be interrupted
	 * by a "signal" either, so I take the easy way out and
	 * have select() time out after one second.
	 * System clock updates really aren't time-critical,
	 * and - lacking a hardware reference clock - I have
	 * yet to learn about anything else that is.
	 */
	was_alarmed = 0;
	rbuflist = (struct recvbuf *)0;
	while (finish_time > current_time) {
#if !defined(HAVE_SIGNALED_IO) 
		fd_set rdfdes;
		int nfound;
#elif defined(HAVE_SIGNALED_IO)
		block_io_and_alarm();
#endif

		rbuflist = getrecvbufs();	/* get received buffers */
		if (printmsg) {
			printmsg = 0;
			analysis(0);
		}
		if (alarm_flag) {		/* alarmed? */
			was_alarmed = 1;
			alarm_flag = 0;
		}

		if (!was_alarmed && rbuflist == (struct recvbuf *)0) {
			/*
			 * Nothing to do.  Wait for something.
			 */
#ifndef HAVE_SIGNALED_IO
			rdfdes = fdmask;
# if defined(VMS) || defined(SYS_VXWORKS)
			/* make select() wake up after one second */
			{
				struct timeval t1;

				t1.tv_sec = 1; t1.tv_usec = 0;
				nfound = select(fd+1, &rdfdes, (fd_set *)0,
						(fd_set *)0, &t1);
			}
# else
			nfound = select(fd+1, &rdfdes, (fd_set *)0,
					(fd_set *)0, (struct timeval *)0);
# endif /* VMS */
			if (nfound > 0) {
				l_fp ts;
				get_systime(&ts);
				(void)input_handler(&ts);
			}
			else if (nfound == -1 && errno != EINTR)
				msyslog(LOG_ERR, "select() error: %m");
			else if (debug) {
# if !defined SYS_VXWORKS && !defined SYS_CYGWIN32 /* to unclutter log */
				msyslog(LOG_DEBUG, "select(): nfound=%d, error: %m", nfound);
# endif
			}
#else /* HAVE_SIGNALED_IO */
                        
			wait_for_signal();
#endif /* HAVE_SIGNALED_IO */
			if (alarm_flag) 	/* alarmed? */
			{
				was_alarmed = 1;
				alarm_flag = 0;
			}
			rbuflist = getrecvbufs();  /* get received buffers */
		}
#ifdef HAVE_SIGNALED_IO
		unblock_io_and_alarm();
#endif /* HAVE_SIGNALED_IO */

		/*
		 * Out here, signals are unblocked.  Call timer routine
		 * to process expiry.
		 */
		if (was_alarmed)
		{
			timer();
			was_alarmed = 0;
		}

		/*
		 * Call the data procedure to handle each received
		 * packet.
		 */
		while (rbuflist != (struct recvbuf *)0)
		{
			rbuf = rbuflist;
			rbuflist = rbuf->next;
			receive(rbuf);
			freerecvbuf(rbuf);
		}
#if defined DEBUG && defined SYS_WINNT
		if (debug > 4)
		    printf("getrecvbufs: %ld handler interrupts, %ld frames\n",
			   handler_calls, handler_pkts);
#endif

		/*
		 * Do we have enough information to stop now?
		 */
		if (have_enough())
			break;	/* time to end */

		/*
		 * Go around again
		 */
	}

	/*
	 * adjust the clock and exit accordingly
	 */
	set_local_clock();

	/*
	 * if we get here then we are in trouble
	 */
	return(1);
}


/*
 * analysis - print a message indicating what is happening with time service
 *	      must mimic have_enough() procedure.
 */
static void
analysis(
	int final
	)
{
	if (contacted < sys_numservers) {
		printf("%d servers of %d have been probed with %d packets\n",
		       contacted,sys_numservers,MINTRANSMITS);
		return;
	}
	if (!responding) {
		printf("No response from any of %d servers, network problem?\n",
		       sys_numservers);
		return;
	}
	else if (responding < min_servers) {
		printf("%d servers out of %d responding, need at least %d.\n",
		       responding, sys_numservers, min_servers);
		return;
	}
	if (!validcount) {
		printf("%d servers responding but none have valid time\n",
		       responding);
		return;
	}
	else if (validcount < min_valid) {
		printf("%d servers responding, %d are valid, need %d valid\n",
		       responding,validcount,min_valid);
		return;
	}
	if (!final && valid_n_low != validcount) {
		printf("%d valid servers but only %d have low dispersion\n",
		       validcount,valid_n_low);
		return;
	}
}


/* have_enough - see if we have enough information to terminate probing
 */
static int
have_enough(void)
{
	/* have we contacted all servers yet? */
	if (contacted < sys_numservers)
		return 0;	/* no...try some more */

	/* have we got at least minimum servers responding? */
	if (responding < min_servers)
		return 0;	/* no...try some more */

	/* count the clocks */
	(void) clock_count();

	/* have we got at least minimum valid clocks? */
	if (validcount <= 0 || validcount < min_valid)
		return 0;	/* no...try some more */

	/* do we have all valid servers with low dispersion */
	if (!secondhalf && valid_n_low != validcount)
		return 0;

	/* if we get into the secondhalf then we ignore dispersion */

	/* all conditions have been met...end */
	return 1;
}


/*
 * transmit - transmit a packet to the given server, or mark it completed.
 *	      This is called by the timeout routine and by the receive
 *	      procedure.
 */
static void
transmit(
	register struct server *server
	)
{
	struct pkt xpkt;
	int timeout;

	if (debug > 2)
		printf("transmit(%s)\n", ntoa(&server->srcadr));

	if ((server->reach & 01) == 0) {
		l_fp ts;
		/*
		 * Last message to this server timed out.  Shift
		 * zeros into the filter.
		 */
		L_CLR(&ts);
		clock_filter(server, 0, &ts);
	}

	/*
	 * shift reachable register over
	 */
	server->reach <<= 1;

	/*
	 * If we're here, send another message to the server.  Fill in
	 * the packet and let 'er rip.
	 */
	xpkt.li_vn_mode = PKT_LI_VN_MODE(LEAP_NOTINSYNC,
		server->version, MODE_CLIENT);
	xpkt.stratum = STRATUM_TO_PKT(STRATUM_UNSPEC);
	xpkt.ppoll = NTP_MINPOLL;
	xpkt.precision = NTPDATE_PRECISION;
	xpkt.rootdelay = htonl(NTPDATE_DISTANCE);
	xpkt.rootdispersion = htonl(NTPDATE_DISP);
	xpkt.refid = htonl(NTPDATE_REFID);
	L_CLR(&xpkt.reftime);
	L_CLR(&xpkt.org);
	L_CLR(&xpkt.rec);

	/*
	 * Determine whether to authenticate or not.  If so,
	 * fill in the extended part of the packet and do it.
	 * If not, just timestamp it and send it away.
	 */
	if (sys_authenticate) {
		int len;

		xpkt.exten[0] = htonl(sys_authkey);
		get_systime(&server->xmt);
		L_ADDUF(&server->xmt, sys_authdelay);
		HTONL_FP(&server->xmt, &xpkt.xmt);
		len = authencrypt(sys_authkey, (u_int32 *)&xpkt, LEN_PKT_NOMAC);
		if (sendpkt(&(server->srcadr), &xpkt, (int)(LEN_PKT_NOMAC + len))) {
			if (debug > 1)
				printf("failed transmit auth to %s\n",
				    ntoa(&(server->srcadr)));
			return;
		}

		if (debug > 1)
			printf("transmit auth to %s\n",
			    ntoa(&(server->srcadr)));
	} else {
		get_systime(&(server->xmt));
		HTONL_FP(&server->xmt, &xpkt.xmt);
		if (sendpkt(&(server->srcadr), &xpkt, LEN_PKT_NOMAC)) {
			if (debug > 1)
				printf("failed transmit to %s\n", 
				    ntoa(&(server->srcadr)));
			return;
		}

		if (debug > 1)
			printf("transmit to %s\n", ntoa(&(server->srcadr)));
	}

	/*
	 * count transmits, record contacted count and set transmit time
	 */
	if (++server->xmtcnt == MINTRANSMITS)
	    contacted++;
	server->last_xmit = current_time;

	/*
	 * determine timeout for this packet. The more packets we send
	 * to the host, the slower we get. If the host indicates that
	 * it is not "sane" then we expect even less.
	 */
	if (server->xmtcnt < MINTRANSMITS) {
	    /* we have not sent enough */
	    timeout = TIMER_HZ;		/* 1 second probe */
	}
	else if (server->rcvcnt <= 0) {
	    /* we have heard nothing */
	    if (secondhalf)
		timeout = TIMER_HZ<<4;	/* 16 second probe */
	    else
		timeout = TIMER_HZ<<3;	/* 8 second probe */
	}
	else {
	    /* if we have low dispersion then probe infrequently */
	    if (server->dispersion <= DESIREDDISP)
		timeout = TIMER_HZ<<4;	/* 16 second probe */
	    /* if the server is not in sync then let it alone */
	    else if (server->leap == LEAP_NOTINSYNC)
		timeout = TIMER_HZ<<4;	/* 16 second probe */
	    /* if the server looks broken ignore it */
	    else if (server->org.l_ui < server->reftime.l_ui)
		timeout = TIMER_HZ<<5;	/* 32 second probe */
	    else if (secondhalf)
		timeout = TIMER_HZ<<2;	/* 4 second probe */
	    else
		timeout = TIMER_HZ<<1;	/* 2 second probe */
	}

	/*
	 * set next transmit time based on timeout
	 */
	server->event_time = current_time + timeout;
}


/*
 * receive - receive and process an incoming frame
 */
static void
receive(
	struct recvbuf *rbufp
	)
{
	register struct pkt *rpkt;
	register struct server *server;
	register s_fp di;
	l_fp t10, t23;
	l_fp org;
	l_fp rec;
	l_fp ci;
	int has_mac;
	int is_authentic;

	if (debug > 2)
		printf("receive(%s)\n", ntoa(&rbufp->srcadr));
	/*
	 * Check to see if the packet basically looks like something
	 * intended for us.
	 */
	if (rbufp->recv_length == LEN_PKT_NOMAC)
		has_mac = 0;
	else if (rbufp->recv_length >= LEN_PKT_NOMAC)
		has_mac = 1;
	else {
		if (debug > 2)
			printf("receive: packet length %d\n",
			    rbufp->recv_length);
		return;		/* funny length packet */
	}

	rpkt = &(rbufp->recv_pkt);
	if (PKT_VERSION(rpkt->li_vn_mode) < NTP_OLDVERSION ||
	    PKT_VERSION(rpkt->li_vn_mode) > NTP_VERSION) {
		if (debug > 1)
			printf("receive: bad version %d\n",
			       PKT_VERSION(rpkt->li_vn_mode));
		return;
	}

	if ((PKT_MODE(rpkt->li_vn_mode) != MODE_SERVER
	    && PKT_MODE(rpkt->li_vn_mode) != MODE_PASSIVE)
	    || rpkt->stratum >=STRATUM_UNSPEC) {
		if (debug > 1)
			printf("receive: mode %d stratum %d\n",
			    PKT_MODE(rpkt->li_vn_mode), rpkt->stratum);
		return;
	}
	
	/*
	 * So far, so good.  See if this is from a server we know.
	 */
	server = findserver(&(rbufp->srcadr));
	if (server == NULL) {
		if (debug > 1)
			printf("receive: server not found\n");
		return;
	}

	/*
	 * Decode the org timestamp and make sure we're getting a response
	 * to our last request.
	 */
	NTOHL_FP(&rpkt->org, &org);
	if (!L_ISEQU(&org, &server->xmt)) {
		if (debug > 1)
			printf("receive: pkt.org and peer.xmt differ\n");
		return;
	}
	
	/*
	 * Check out the authenticity if we're doing that.
	 */
	if (!sys_authenticate)
		is_authentic = 1;
	else {
		is_authentic = 0;

		if (debug > 3)
			printf("receive: rpkt keyid=%ld sys_authkey=%ld decrypt=%ld\n",
			   (long int)ntohl(rpkt->exten[0]), (long int)sys_authkey,
			   (long int)authdecrypt(sys_authkey, (u_int32 *)rpkt,
				LEN_PKT_NOMAC, (int)(rbufp->recv_length - LEN_PKT_NOMAC)));

		if (has_mac && ntohl(rpkt->exten[0]) == sys_authkey &&
			authdecrypt(sys_authkey, (u_int32 *)rpkt, LEN_PKT_NOMAC,
			(int)(rbufp->recv_length - LEN_PKT_NOMAC)))
			is_authentic = 1;
		if (debug)
			printf("receive: authentication %s\n",
			   is_authentic ? "passed" : "failed");
	}
	server->trust <<= 1;
	if (!is_authentic)
		server->trust |= 1;
	
	/*
	 * Looks good.  Record info from the packet.
	 */
	server->leap = PKT_LEAP(rpkt->li_vn_mode);
	server->stratum = PKT_TO_STRATUM(rpkt->stratum);
	server->precision = rpkt->precision;
	server->rootdelay = ntohl(rpkt->rootdelay);
	server->rootdispersion = ntohl(rpkt->rootdispersion);
	server->refid = rpkt->refid;
	NTOHL_FP(&rpkt->reftime, &server->reftime);
	NTOHL_FP(&rpkt->rec, &rec);
	NTOHL_FP(&rpkt->xmt, &server->org);

	/*
	 * count this guy as responding
	 */
	server->reach |= 1;
	if (server->rcvcnt++ == 0)
		responding++;

	/*
	 * Make sure the server is at least somewhat sane.  If not, ignore
	 * it for later.
	 */
	if (L_ISZERO(&rec) || !L_ISHIS(&server->org, &rec)) {
		if (debug > 1)
			printf("receive: pkt insane\n");
		return;
	}

	/*
	 * Calculate the round trip delay (di) and the clock offset (ci).
	 * We use the equations (reordered from those in the spec):
	 *
	 * d = (t2 - t3) - (t1 - t0)
	 * c = ((t2 - t3) + (t1 - t0)) / 2
	 */
	t10 = server->org;		/* pkt.xmt == t1 */
	L_SUB(&t10, &rbufp->recv_time);	/* recv_time == t0*/

	t23 = rec;			/* pkt.rec == t2 */
	L_SUB(&t23, &org);		/* pkt->org == t3 */

	/* now have (t2 - t3) and (t0 - t1).  Calculate (ci) and (di) */
	ci = t10;
	L_ADD(&ci, &t23);
	L_RSHIFT(&ci);

	/*
	 * Calculate di in t23 in full precision, then truncate
	 * to an s_fp.
	 */
	L_SUB(&t23, &t10);
	di = LFPTOFP(&t23);

	if (debug > 3)
		printf("offset: %s, delay %s\n", lfptoa(&ci, 6), fptoa(di, 5));

	di += (FP_SECOND >> (-(int)NTPDATE_PRECISION))
	    + (FP_SECOND >> (-(int)server->precision)) + NTP_MAXSKW;

	if (di <= 0) {		/* value still too raunchy to use? */
		L_CLR(&ci);
		di = 0;
	} else {
		di = max(di, NTP_MINDIST);
	}


	/*
	 * This one is valid.  Give it to clock_filter(),
	 */
	clock_filter(server, di, &ci);
	if (debug > 1)
		printf("receive from %s\n", ntoa(&rbufp->srcadr));

	/*
	 * See if we should goes the transmission. If not return now
	 * otherwise have the next event time be shortened
	 */
	if (server->stratum <= NTP_INFIN)
	    return;	/* server does not have a stratum */
	if (server->leap == LEAP_NOTINSYNC)
	    return;	/* just booted server or out of sync */
	if (!L_ISHIS(&server->org, &server->reftime))
	    return;	/* broken host */
	if (server->trust != 0)
	    return;	/* can not trust it */

	if (server->dispersion < DESIREDDISP)
	    return;	/* we have the desired dispersion */

	server->event_time -= (TIMER_HZ+1);
}


/*
 * clock_filter - add clock sample, determine a server's delay, dispersion
 *                and offset
 */
static void
clock_filter(
	register struct server *server,
	s_fp di,
	l_fp *c
	)
{
	register int i, j;
	int ord[NTP_SHIFT];

	/*
	 * Insert sample and increment nextpt
	 */

	i = server->filter_nextpt;
	server->filter_delay[i] = di;
	server->filter_offset[i] = *c;
	server->filter_soffset[i] = LFPTOFP(c);
	server->filter_nextpt++;
	if (server->filter_nextpt >= NTP_SHIFT)
		server->filter_nextpt = 0;

	/*
	 * Sort indices into increasing delay order
	 */
	for (i = 0; i < NTP_SHIFT; i++)
		ord[i] = i;
	
	for (i = 0; i < (NTP_SHIFT-1); i++) {
		for (j = i+1; j < NTP_SHIFT; j++) {
			if (server->filter_delay[ord[j]] == 0)
				continue;
			if (server->filter_delay[ord[i]] == 0
			    || (server->filter_delay[ord[i]]
			    > server->filter_delay[ord[j]])) {
				register int tmp;

				tmp = ord[i];
				ord[i] = ord[j];
				ord[j] = tmp;
			}
		}
	}

	/*
	 * Now compute the dispersion, and assign values to delay and
	 * offset.  If there are no samples in the register, delay and
	 * offset go to zero and dispersion is set to the maximum.
	 */
	if (server->filter_delay[ord[0]] == 0) {
		server->delay = 0;
		L_CLR(&server->offset);
		server->soffset = 0;
		server->dispersion = PEER_MAXDISP;
	} else {
		register s_fp d;

		server->delay = server->filter_delay[ord[0]];
		server->offset = server->filter_offset[ord[0]];
		server->soffset = LFPTOFP(&server->offset);
		server->dispersion = 0;
		for (i = 1; i < NTP_SHIFT; i++) {
			if (server->filter_delay[ord[i]] == 0)
				d = PEER_MAXDISP;
			else {
				d = server->filter_soffset[ord[i]]
				    - server->filter_soffset[ord[0]];
				if (d < 0)
					d = -d;
				if (d > PEER_MAXDISP)
					d = PEER_MAXDISP;
			}
			/*
			 * XXX This *knows* PEER_FILTER is 1/2
			 */
			server->dispersion += (u_fp)(d) >> i;
		}
	}
	/*
	 * We're done
	 */
}


/* clock_count - count the clock sources we have
 */
static void
clock_count(void)
{
	register struct server *server;
	register int n;

	/* reset counts */
	validcount = valid_n_low = 0;

	/* go through the list of servers and count the clocks we believe
	 * and that have low dispersion
	 */
	for (n = 0; n < sys_numservers; n++) {
		server = sys_servers[n];
		if (server->delay == 0) {
			continue;	/* no data */
		}
		if (server->stratum > NTP_INFIN) {
			continue;	/* stratum no good */
		}
		if (server->delay > NTP_MAXWGT) {
			continue;	/* too far away */
		}
		if (server->leap == LEAP_NOTINSYNC)
			continue;	/* he's in trouble */
		if (!L_ISHIS(&server->org, &server->reftime)) {
			continue;	/* very broken host */
		}
		if ((server->org.l_ui - server->reftime.l_ui) >= NTP_MAXAGE) {
			continue;	/* too long without sync */
		}
		if (server->trust != 0) {
			continue;
		}

		/*
		 * This one is a valid time source..
		 */
		validcount++;

		/*
		 * See if this one has a okay low dispersion
		 */
		if (server->dispersion <= DESIREDDISP)
		    valid_n_low++;
	}

	if (debug > 1)
		printf("have %d, valid %d, low %d\n",
			responding, validcount, valid_n_low);
}


/*
 * clock_select - select the pick-of-the-litter clock from the samples
 *		  we've got.
 */
static struct server *
clock_select(void)
{
	register struct server *server;
	register int i;
	register int nlist;
	register s_fp d;
	register int j;
	register int n;
	s_fp local_threshold;
	struct server *server_list[NTP_MAXCLOCK];
	u_fp server_badness[NTP_MAXCLOCK];
	struct server *sys_server;

	/*
	 * This first chunk of code is supposed to go through all
	 * servers we know about to find the NTP_MAXLIST servers which
	 * are most likely to succeed.  We run through the list
	 * doing the sanity checks and trying to insert anyone who
	 * looks okay.  We are at all times aware that we should
	 * only keep samples from the top two strata and we only need
	 * NTP_MAXLIST of them.
	 */
	nlist = 0;	/* none yet */
	for (n = 0; n < sys_numservers; n++) {
		server = sys_servers[n];
		if (server->delay == 0)
			continue;	/* no data */
		if (server->stratum > NTP_INFIN)
			continue;	/* stratum no good */
		if (server->delay > NTP_MAXWGT) {
			continue;	/* too far away */
		}
		if (server->leap == LEAP_NOTINSYNC)
			continue;	/* he's in trouble */
		if (!L_ISHIS(&server->org, &server->reftime)) {
			continue;	/* very broken host */
		}
		if ((server->org.l_ui - server->reftime.l_ui)
		    >= NTP_MAXAGE) {
			continue;	/* too long without sync */
		}
		if (server->trust != 0) {
			continue;
		}

		/*
		 * This one seems sane.  Find where he belongs
		 * on the list.
		 */
		d = server->dispersion + server->dispersion;
		for (i = 0; i < nlist; i++)
			if (server->stratum <= server_list[i]->stratum)
				break;
		for ( ; i < nlist; i++) {
			if (server->stratum < server_list[i]->stratum)
				break;
			if (d < (s_fp) server_badness[i])
				break;
		}

		/*
		 * If i points past the end of the list, this
		 * guy is a loser, else stick him in.
		 */
		if (i >= NTP_MAXLIST)
			continue;
		for (j = nlist; j > i; j--)
			if (j < NTP_MAXLIST) {
				server_list[j] = server_list[j-1];
				server_badness[j]
				    = server_badness[j-1];
			}

		server_list[i] = server;
		server_badness[i] = d;
		if (nlist < NTP_MAXLIST)
			nlist++;
	}

	/*
	 * Got the five-or-less best.  Cut the list where the number of
	 * strata exceeds two.
	 */
	j = 0;
	for (i = 1; i < nlist; i++)
		if (server_list[i]->stratum > server_list[i-1]->stratum)
			if (++j == 2) {
				nlist = i;
				break;
			}

	/*
	 * Whew!  What we should have by now is 0 to 5 candidates for
	 * the job of syncing us.  If we have none, we're out of luck.
	 * If we have one, he's a winner.  If we have more, do falseticker
	 * detection.
	 */

	if (nlist == 0)
		sys_server = 0;
	else if (nlist == 1) {
		sys_server = server_list[0];
	} else {
		/*
		 * Re-sort by stratum, bdelay estimate quality and
		 * server.delay.
		 */
		for (i = 0; i < nlist-1; i++)
			for (j = i+1; j < nlist; j++) {
				if (server_list[i]->stratum
				    < server_list[j]->stratum)
					break;	/* already sorted by stratum */
				if (server_list[i]->delay
				    < server_list[j]->delay)
					continue;
				server = server_list[i];
				server_list[i] = server_list[j];
				server_list[j] = server;
			}
		
		/*
		 * Calculate the fixed part of the dispersion limit
		 */
		local_threshold = (FP_SECOND >> (-(int)NTPDATE_PRECISION))
		    + NTP_MAXSKW;

		/*
		 * Now drop samples until we're down to one.
		 */
		while (nlist > 1) {
			for (n = 0; n < nlist; n++) {
				server_badness[n] = 0;
				for (j = 0; j < nlist; j++) {
					if (j == n)	/* with self? */
						continue;
					d = server_list[j]->soffset
					    - server_list[n]->soffset;
					if (d < 0)	/* absolute value */
						d = -d;
					/*
					 * XXX This code *knows* that
					 * NTP_SELECT is 3/4
					 */
					for (i = 0; i < j; i++)
						d = (d>>1) + (d>>2);
					server_badness[n] += d;
				}
			}

			/*
			 * We now have an array of nlist badness
			 * coefficients.  Find the badest.  Find
			 * the minimum precision while we're at
			 * it.
			 */
			i = 0;
			n = server_list[0]->precision;;
			for (j = 1; j < nlist; j++) {
				if (server_badness[j] >= server_badness[i])
					i = j;
				if (n > server_list[j]->precision)
					n = server_list[j]->precision;
			}
			
			/*
			 * i is the index of the server with the worst
			 * dispersion.  If his dispersion is less than
			 * the threshold, stop now, else delete him and
			 * continue around again.
			 */
			if (server_badness[i] < (local_threshold
						 + (FP_SECOND >> (-n))))
				break;
			for (j = i + 1; j < nlist; j++)
				server_list[j-1] = server_list[j];
			nlist--;
		}

		/*
		 * What remains is a list of less than 5 servers.  Take
		 * the best.
		 */
		sys_server = server_list[0];
	}

	/*
	 * That's it.  Return our server.
	 */
	return sys_server;
}


/*
 * set_local_clock -- handle setting the local clock or displaying info.
 */
static void
set_local_clock(void)
{
	register int i;
	register struct server *server;
	time_t tmp;
	double dtemp;

	/*
	 * if setting time then print final analysis
	 */
	if (set_time)
	    analysis(1);

	/*
	 * pick a clock
	 */
	server = clock_select();

	/*
	 * do some display of information
	 */
	if (debug || verbose) {
		for (i = 0; i < sys_numservers; i++)
			printserver(sys_servers[i], stdout);
		if (debug)
			printf("packets sent %ld, received %ld\n",
				total_xmit, total_recv);
	}

	/*
	 * see if we have a server to set the time with
	 */
	if (server == 0) {
	    if (!set_time || verbose)
		fprintf(stdout,"No servers available to sync time with\n");
	    exit(1);
	}

	/*
	 * we have a valid and selected time to use!!!!!
	 */

	/*
	 * if we are not setting the time then display offset and exit
	 */
	if (!set_time) {
		fprintf(stdout,
			"Your clock is off by %s seconds. (%s) [%ld/%ld]\n",
			lfptoa(&server->offset, 7),
			ntoa(&server->srcadr),
			total_xmit, total_recv);
		exit(0);
	}

	/*
	 * set the clock
	 * XXX: Examine the more flexible approach used by ntpdate.
	 * Note that a design consideration here is that we sometimes
	 * _want_ to step the clock by a _huge_ amount in either
	 * direction, because the local clock is completely bogus.
	 * This condition must be recognized and dealt with, so
	 * that we always get a good time when this completes.
	 * -- jhutz+@cmu.edu, 16-Aug-1999
	 */
	LFPTOD(&server->offset, dtemp);
	step_systime(dtemp);
	time(&tmp);
	fprintf(stdout,"Time set to %.20s [%s %s %ld/%ld]\n",
		ctime(&tmp)+4,
		ntoa(&server->srcadr),
		lfptoa(&server->offset, 7),
		total_xmit, total_recv);
	exit(0);
}


/*
 * findserver - find a server in the list given its address
 */
static struct server *
findserver(
	struct sockaddr_in *addr
	)
{
	register int i;
	register u_int32 netnum;

	if (htons(addr->sin_port) != NTP_PORT)
		return 0;
	netnum = addr->sin_addr.s_addr;

	for (i = 0; i < sys_numservers; i++) {
		if (netnum == sys_servers[i]->srcadr.sin_addr.s_addr)
			return sys_servers[i];
	}
	return 0;
}


/*
 * timer - process a timer interrupt
 */
static void
timer(void)
{
	register int k;

	/*
	 * Bump the current idea of the time
	 */
	current_time++;

	/*
	 * see if we have reached half time
	 */
	if (current_time >= half_time && !secondhalf) {
	    secondhalf++;
	    if (debug)
		printf("\nSecond Half of Timeout!\n");
	    printmsg++;
	}

	/*
	 * We only want to send a few packets per transmit interrupt
	 * to throttle things
	 */
	for(k = 0;k < MAXXMITCOUNT;k++) {
	    register int i, oldi;
	    register u_long oldxtime;

	    /*
	     * We want to send a packet out for a server that has an
	     * expired event time. However to be mellow about this, we only
	     * use one expired event timer and to avoid starvation we use
	     * the one with the oldest last transmit time.
	     */
	    oldi = -1;
	    oldxtime = 0;
	    for (i = 0; i < sys_numservers; i++) {
		if (sys_servers[i]->event_time <= current_time) {
		    if (oldi < 0 || oldxtime > sys_servers[i]->last_xmit) {
			oldxtime = sys_servers[i]->last_xmit;
			oldi = i;
		    }
		}
	    }
	    if (oldi >= 0)
		transmit(sys_servers[oldi]);
	    else
		break;	/* no expired event */
	} /* end of transmit loop */
}


#ifndef SYS_WINNT
/*
 * alarming - record the occurance of an alarm interrupt
 */
static RETSIGTYPE
alarming(
	int sig
	)
#else
void CALLBACK 
alarming(UINT uTimerID, UINT uMsg, DWORD dwUser, DWORD dw1, DWORD dw2)
#endif /* SYS_WINNT */
{
	alarm_flag++;
}


/*
 * init_alarm - set up the timer interrupt
 */
static void
init_alarm(void)
{
#ifndef SYS_WINNT
# ifndef HAVE_TIMER_SETTIME
	struct itimerval itimer;
# else
	struct itimerspec ntpdate_itimer;
# endif
#else
	TIMECAPS tc;
	UINT wTimerRes, wTimerID;
# endif /* SYS_WINNT */
#if defined SYS_CYGWIN32 || defined SYS_WINNT
	HANDLE hToken;
	TOKEN_PRIVILEGES tkp;
	DWORD dwUser = 0;
#endif /* SYS_WINNT */

	alarm_flag = 0;

#ifndef SYS_WINNT
# if defined(HAVE_TIMER_CREATE) && defined(HAVE_TIMER_SETTIME)
	alarm_flag = 0;
	/* this code was put in as setitimer() is non existant this us the
	 * POSIX "equivalents" setup - casey
	 */
	/* ntpdate_timerid is global - so we can kill timer later */
	if (timer_create (CLOCK_REALTIME, NULL, &ntpdate_timerid) ==
#  ifdef SYS_VXWORKS
		ERROR
#  else
		-1
#  endif
		)
	{
		fprintf (stderr, "init_alarm(): timer_create (...) FAILED\n");
		return;
	}

	/*	TIMER_HZ = (5)
	 * Set up the alarm interrupt.	The first comes 1/(2*TIMER_HZ)
	 * seconds from now and they continue on every 1/TIMER_HZ seconds.
	 */
	(void) signal_no_reset(SIGALRM, alarming);
	ntpdate_itimer.it_interval.tv_sec = ntpdate_itimer.it_value.tv_sec = 0;
	ntpdate_itimer.it_interval.tv_nsec = 1000000000/TIMER_HZ;
	ntpdate_itimer.it_value.tv_nsec = 1000000000/(TIMER_HZ<<1);
	timer_settime(ntpdate_timerid, 0 /* !TIMER_ABSTIME */, &ntpdate_itimer, NULL);
# else
	/*
	 * Set up the alarm interrupt.	The first comes 1/(2*TIMER_HZ)
	 * seconds from now and they continue on every 1/TIMER_HZ seconds.
	 */
	(void) signal_no_reset(SIGALRM, alarming);
	itimer.it_interval.tv_sec = itimer.it_value.tv_sec = 0;
	itimer.it_interval.tv_usec = 1000000/TIMER_HZ;
	itimer.it_value.tv_usec = 1000000/(TIMER_HZ<<1);
	setitimer(ITIMER_REAL, &itimer, (struct itimerval *)0);
# endif
#if defined SYS_CYGWIN32
	/*
	 * Get previleges needed for fiddling with the clock
	 */

	/* get the current process token handle */
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
		msyslog(LOG_ERR, "OpenProcessToken failed: %m");
		exit(1);
	}
	/* get the LUID for system-time privilege. */
	LookupPrivilegeValue(NULL, SE_SYSTEMTIME_NAME, &tkp.Privileges[0].Luid);
	tkp.PrivilegeCount = 1;  /* one privilege to set */
	tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	/* get set-time privilege for this process. */
	AdjustTokenPrivileges(hToken, FALSE, &tkp, 0,(PTOKEN_PRIVILEGES) NULL, 0);
	/* cannot test return value of AdjustTokenPrivileges. */
	if (GetLastError() != ERROR_SUCCESS)
		msyslog(LOG_ERR, "AdjustTokenPrivileges failed: %m");
#endif
#else	/* SYS_WINNT */
	_tzset();

	/*
	 * Get previleges needed for fiddling with the clock
	 */

	/* get the current process token handle */
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
		msyslog(LOG_ERR, "OpenProcessToken failed: %m");
		exit(1);
	}
	/* get the LUID for system-time privilege. */
	LookupPrivilegeValue(NULL, SE_SYSTEMTIME_NAME, &tkp.Privileges[0].Luid);
	tkp.PrivilegeCount = 1;  /* one privilege to set */
	tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	/* get set-time privilege for this process. */
	AdjustTokenPrivileges(hToken, FALSE, &tkp, 0,(PTOKEN_PRIVILEGES) NULL, 0);
	/* cannot test return value of AdjustTokenPrivileges. */
	if (GetLastError() != ERROR_SUCCESS)
		msyslog(LOG_ERR, "AdjustTokenPrivileges failed: %m");

	/*
	 * Set up timer interrupts for every 2**EVENT_TIMEOUT seconds
	 * Under Win/NT, expiry of timer interval leads to invocation
	 * of a callback function (on a different thread) rather than
	 * generating an alarm signal
	 */

	/* determine max and min resolution supported */
	if(timeGetDevCaps(&tc, sizeof(TIMECAPS)) != TIMERR_NOERROR) {
		msyslog(LOG_ERR, "timeGetDevCaps failed: %m");
		exit(1);
	}
	wTimerRes = min(max(tc.wPeriodMin, TARGET_RESOLUTION), tc.wPeriodMax);
	/* establish the minimum timer resolution that we'll use */
	timeBeginPeriod(wTimerRes);

	/* start the timer event */
	wTimerID = timeSetEvent(
		(UINT) (1000/TIMER_HZ),    /* Delay */
		wTimerRes,			 /* Resolution */
		(LPTIMECALLBACK) alarming, /* Callback function */
		(DWORD) dwUser, 	 /* User data */
		TIME_PERIODIC); 	 /* Event type (periodic) */
	if (wTimerID == 0) {
		msyslog(LOG_ERR, "timeSetEvent failed: %m");
		exit(1);
	}
#endif /* SYS_WINNT */
}


/*
 * init_io - initialize I/O data and open socket
 */
static void
init_io(void)
{
#ifdef SYS_WINNT
    	WORD wVersionRequested;
	WSADATA wsaData;
	init_transmitbuff();
#endif /* SYS_WINNT */

	/*
	 * Init buffer free list and stat counters
	 */
	init_recvbuff(sys_numservers + 2);

#if defined(HAVE_SIGNALED_IO)
	set_signal();
#endif

#ifdef SYS_WINNT
	wVersionRequested = MAKEWORD(1,1);
	if (WSAStartup(wVersionRequested, &wsaData))
	{
		msyslog(LOG_ERR, "No useable winsock.dll: %m");
		exit(1);
	}
#endif /* SYS_WINNT */

	BLOCKIO();

	/* create a datagram (UDP) socket */
	if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		msyslog(LOG_ERR, "socket() failed: %m");
		exit(1);
		/*NOTREACHED*/
	}

	/*
	 * bind the socket to the NTP port
	 */
	if (!debug && set_time && !unpriv_port) {
		struct sockaddr_in addr;

		memset((char *)&addr, 0, sizeof addr);
		addr.sin_family = AF_INET;
		addr.sin_port = htons(NTP_PORT);
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
		if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
#ifndef SYS_WINNT
			if (errno == EADDRINUSE)
#else
			if (WSAGetLastError() == WSAEADDRINUSE)
#endif
				msyslog(LOG_ERR,
				    "the NTP socket is in use, exiting");
			else
				msyslog(LOG_ERR, "bind() fails: %m");
			exit(1);
		}
	}

	FD_ZERO(&fdmask);
	FD_SET(fd, &fdmask);

	/*
	 * set non-blocking,
	 */

#ifdef USE_FIONBIO
	/* in vxWorks we use FIONBIO, but the others are defined for old systems, so
	 * all hell breaks loose if we leave them defined
	 */
#undef O_NONBLOCK
#undef FNDELAY
#undef O_NDELAY
#endif

#if defined(O_NONBLOCK) /* POSIX */
	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0)
	{
		msyslog(LOG_ERR, "fcntl(O_NONBLOCK) fails: %m");
		exit(1);
		/*NOTREACHED*/
	}
#elif defined(FNDELAY)
	if (fcntl(fd, F_SETFL, FNDELAY) < 0)
	{
		msyslog(LOG_ERR, "fcntl(FNDELAY) fails: %m");
		exit(1);
		/*NOTREACHED*/
	}
#elif defined(O_NDELAY) /* generally the same as FNDELAY */
	if (fcntl(fd, F_SETFL, O_NDELAY) < 0)
	{
		msyslog(LOG_ERR, "fcntl(O_NDELAY) fails: %m");
		exit(1);
		/*NOTREACHED*/
	}
#elif defined(FIONBIO)
	if (
# if defined(VMS)
		(ioctl(fd,FIONBIO,&1) < 0)
# elif defined(SYS_WINNT)
		(ioctlsocket(fd,FIONBIO,(u_long *) &on) == SOCKET_ERROR)
# else
		(ioctl(fd,FIONBIO,&on) < 0)
# endif
	   )
	{
		msyslog(LOG_ERR, "ioctl(FIONBIO) fails: %m");
		exit(1);
		/*NOTREACHED*/
	}
#elif defined(FIOSNBIO)
	if (ioctl(fd,FIOSNBIO,&on) < 0)
	{
		msyslog(LOG_ERR, "ioctl(FIOSNBIO) fails: %m");
		exit(1);
		/*NOTREACHED*/
	}
#else
# include "Bletch: Need non-blocking I/O!"
#endif

#ifdef HAVE_SIGNALED_IO
	init_socket_sig(fd);
#endif /* not HAVE_SIGNALED_IO */

	UNBLOCKIO();
}


/*
 * sendpkt - send a packet to the specified destination
 */
static int
sendpkt(
	struct sockaddr_in *dest,
	struct pkt *pkt,
	int len
	)
{
	int cc;
	static int horriblecnt = 0;
#ifdef SYS_WINNT
	DWORD err;
#endif /* SYS_WINNT */

	total_xmit++;	/* count it */

	if (horrible) {
	    if (++horriblecnt > HORRIBLEOK) {
		if (debug > 3)
			printf("dropping send (%s)\n", ntoa(dest));
		if (horriblecnt >= HORRIBLEOK+horrible)
		    horriblecnt = 0;
		return 0;
	    }
	}


	cc = sendto(fd, (char *)pkt, (size_t)len, 0, (struct sockaddr *)dest,
	    sizeof(struct sockaddr_in));
#ifndef SYS_WINNT
	if (cc == -1) {
		if (errno != EWOULDBLOCK && errno != ENOBUFS)
#else
	if (cc == SOCKET_ERROR) {
		err = WSAGetLastError();
		if (err != WSAEWOULDBLOCK && err != WSAENOBUFS)
#endif /* SYS_WINNT */
			msyslog(LOG_ERR, "sendto(%s): %m", ntoa(dest));
		return -1;
	}
	return 0;
}


/*
 * input_handler - receive packets asynchronously
 */
void
input_handler(l_fp *xts)
{
	register int n;
	register struct recvbuf *rb;
	struct timeval tvzero;
	int fromlen;
	fd_set fds;
	l_fp ts;
	ts = *xts; /* we ignore xts, but make the compiler happy */

	/*
	 * Do a poll to see if we have data
	 */
	for (;;) {
		fds = fdmask;
		tvzero.tv_sec = tvzero.tv_usec = 0;
		n = select(fd+1, &fds, (fd_set *)0, (fd_set *)0, &tvzero);

		/*
		 * If nothing to do, just return.  If an error occurred,
		 * complain and return.  If we've got some, freeze a
		 * timestamp.
		 */
		if (n == 0)
			return;
		else if (n == -1) {
			if (errno != EINTR) {
				msyslog(LOG_ERR, "select() error: %m");
			}
			return;
		}
		get_systime(&ts);

		/*
		 * Get a buffer and read the frame.  If we
		 * haven't got a buffer, or this is received
		 * on the wild card socket, just dump the packet.
		 */
		if (initializing || free_recvbuffs == 0) {
			char buf[100];

#ifndef SYS_WINNT
			(void) read(fd, buf, sizeof buf);
#else
			/* NT's _read does not operate on nonblocking sockets
			 * either recvfrom or ReadFile() has to be used here.
			 * ReadFile is used in [ntpd]ntp_intres() and ntpdc,
			 * just to be different use recvfrom() here
			 */
			recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *)0, NULL);
#endif /* SYS_WINNT */
			continue;
		}

		rb = get_free_recv_buffer();

		fromlen = sizeof(struct sockaddr_in);
		rb->recv_length = recvfrom(fd, (char *)&rb->recv_pkt,
		    sizeof(rb->recv_pkt), 0,
		    (struct sockaddr *)&rb->srcadr, &fromlen);
		if (rb->recv_length == -1) {
			freerecvbuf(rb);
			continue;
		}

		/*
		 * Got one.  Mark how and when it got here,
		 * put it on the full list.
		 */
		rb->recv_time = ts;
		add_full_recv_buffer(rb);
		total_recv++;	/* count it */
	}
}


/* XXX ELIMINATE printserver similar in ntptrace.c, ntpdate.c */
/*
 * printserver - print detail information for a server
 */
static void
printserver(
	register struct server *pp,
	FILE *fp
	)
{
	register int i;
	char junk[5];
	char *str;

	if (!debug) {
	    (void) fprintf(fp,
		"%-15s %d/%d %03o v%d s%d offset %9s delay %s disp %s\n",
		ntoa(&pp->srcadr),
		pp->xmtcnt,pp->rcvcnt,pp->reach,
		pp->version,pp->stratum,
		lfptoa(&pp->offset, 6), ufptoa(pp->delay, 5),
		ufptoa(pp->dispersion, 4));
	    return;
	}

	(void) fprintf(fp, "server %s, port %d\n",
			   ntoa(&pp->srcadr), ntohs(pp->srcadr.sin_port));

	(void) fprintf(fp, "stratum %d, precision %d, leap %c%c, trust %03o\n",
			   pp->stratum, pp->precision,
			   pp->leap & 0x2 ? '1' : '0',
			   pp->leap & 0x1 ? '1' : '0',
			   pp->trust);

	if (pp->stratum == 1) {
		junk[4] = 0;
		memmove(junk, (char *)&pp->refid, 4);
		str = junk;
	} else {
		str = numtoa(pp->refid);
	}
	(void) fprintf(fp,
			   "refid [%s], delay %s, dispersion %s\n",
			   str, fptoa((s_fp)pp->delay, 5),
			   ufptoa(pp->dispersion, 5));

	(void) fprintf(fp, "transmitted %d, received %d, reachable %03o\n",
	    pp->xmtcnt, pp->rcvcnt, pp->reach);

	(void) fprintf(fp, "reference time:    %s\n",
			   prettydate(&pp->reftime));
	(void) fprintf(fp, "originate timestamp: %s\n",
			   prettydate(&pp->org));
	(void) fprintf(fp, "transmit timestamp:  %s\n",
			   prettydate(&pp->xmt));

	(void) fprintf(fp, "filter delay: ");
	for (i = 0; i < NTP_SHIFT; i++) {
		(void) fprintf(fp, " %-8.8s", fptoa(pp->filter_delay[i], 5));
		if (i == (NTP_SHIFT>>1)-1)
			(void) fprintf(fp, "\n        ");
	}
	(void) fprintf(fp, "\n");

	(void) fprintf(fp, "filter offset:");
	for (i = 0; i < PEER_SHIFT; i++) {
		(void) fprintf(fp, " %-8.8s", lfptoa(&pp->filter_offset[i], 6));
		if (i == (PEER_SHIFT>>1)-1)
			(void) fprintf(fp, "\n        ");
	}
	(void) fprintf(fp, "\n");

	(void) fprintf(fp, "delay %s, dispersion %s\n",
			   fptoa((s_fp)pp->delay, 5), ufptoa(pp->dispersion, 5));

	(void) fprintf(fp, "offset %s\n\n",
			   lfptoa(&pp->offset, 6));
}

#if !defined(HAVE_VSPRINTF)
int
vsprintf(
	char *str,
	const char *fmt,
	va_list ap
	)
{
	FILE f;
	int len;

	f._flag = _IOWRT+_IOSTRG;
	f._ptr = str;
	f._cnt = 32767;
	len = _doprnt(fmt, ap, &f);
	*f._ptr = 0;
	return (len);
}
#endif
