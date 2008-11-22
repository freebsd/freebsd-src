/*
 * ntpd.c - main program for the fixed point NTP daemon
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "ntp_machine.h"
#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_stdlib.h"
#include <ntp_random.h>

#ifdef SIM
# include "ntpsim.h"
# include "ntpdsim-opts.h"
#else
# include "ntpd-opts.h"
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#include <stdio.h>
#ifndef SYS_WINNT
# if !defined(VMS)	/*wjm*/
#  ifdef HAVE_SYS_PARAM_H
#   include <sys/param.h>
#  endif
# endif /* VMS */
# ifdef HAVE_SYS_SIGNAL_H
#  include <sys/signal.h>
# else
#  include <signal.h>
# endif
# ifdef HAVE_SYS_IOCTL_H
#  include <sys/ioctl.h>
# endif /* HAVE_SYS_IOCTL_H */
# ifdef HAVE_SYS_RESOURCE_H
#  include <sys/resource.h>
# endif /* HAVE_SYS_RESOURCE_H */
#else
# include <signal.h>
# include <process.h>
# include <io.h>
# include <clockstuff.h>
#include "ntp_iocompletionport.h"
#endif /* SYS_WINNT */
#if defined(HAVE_RTPRIO)
# ifdef HAVE_SYS_RESOURCE_H
#  include <sys/resource.h>
# endif
# ifdef HAVE_SYS_LOCK_H
#  include <sys/lock.h>
# endif
# include <sys/rtprio.h>
#else
# ifdef HAVE_PLOCK
#  ifdef HAVE_SYS_LOCK_H
#	include <sys/lock.h>
#  endif
# endif
#endif
#if defined(HAVE_SCHED_SETSCHEDULER)
# ifdef HAVE_SCHED_H
#  include <sched.h>
# else
#  ifdef HAVE_SYS_SCHED_H
#   include <sys/sched.h>
#  endif
# endif
#endif
#if defined(HAVE_SYS_MMAN_H)
# include <sys/mman.h>
#endif

#ifdef HAVE_TERMIOS_H
# include <termios.h>
#endif

#ifdef SYS_DOMAINOS
# include <apollo/base.h>
#endif /* SYS_DOMAINOS */

#include "recvbuff.h"  
#include "ntp_cmdargs.h"  

#if 0				/* HMS: I don't think we need this. 961223 */
#ifdef LOCK_PROCESS
# ifdef SYS_SOLARIS
#  include <sys/mman.h>
# else
#  include <sys/lock.h>
# endif
#endif
#endif

#ifdef _AIX
# include <ulimit.h>
#endif /* _AIX */

#ifdef SCO5_CLOCK
# include <sys/ci/ciioctl.h>
#endif

#ifdef HAVE_DROPROOT
# include <ctype.h>
# include <grp.h>
# include <pwd.h>
#ifdef HAVE_LINUX_CAPABILITIES
# include <sys/capability.h>
# include <sys/prctl.h>
#endif
#endif

/*
 * Signals we catch for debugging.	If not debugging we ignore them.
 */
#define MOREDEBUGSIG	SIGUSR1
#define LESSDEBUGSIG	SIGUSR2

/*
 * Signals which terminate us gracefully.
 */
#ifndef SYS_WINNT
# define SIGDIE1 	SIGHUP
# define SIGDIE3 	SIGQUIT
# define SIGDIE2 	SIGINT
# define SIGDIE4 	SIGTERM
#endif /* SYS_WINNT */

#ifdef HAVE_DNSREGISTRATION
#include <dns_sd.h>
DNSServiceRef mdns;
#endif

/*
 * Scheduling priority we run at
 */
#define NTPD_PRIO	(-12)

int priority_done = 2;		/* 0 - Set priority */
				/* 1 - priority is OK where it is */
				/* 2 - Don't set priority */
				/* 1 and 2 are pretty much the same */

#ifdef DEBUG
/*
 * Debugging flag
 */
volatile int debug = 0;		/* No debugging by default */
#endif

int	listen_to_virtual_ips = 1;
const char *specific_interface = NULL;        /* interface name or IP address to bind to */

/*
 * No-fork flag.  If set, we do not become a background daemon.
 */
int nofork = 0;			/* Fork by default */

#ifdef HAVE_DROPROOT
int droproot = 0;
char *user = NULL;		/* User to switch to */
char *group = NULL;		/* group to switch to */
char *chrootdir = NULL;		/* directory to chroot to */
int sw_uid;
int sw_gid;
char *endp;  
struct group *gr;
struct passwd *pw; 
#endif /* HAVE_DROPROOT */

/*
 * Initializing flag.  All async routines watch this and only do their
 * thing when it is clear.
 */
int initializing;

/*
 * Version declaration
 */
extern const char *Version;

char const *progname;

int was_alarmed;

#ifdef DECL_SYSCALL
/*
 * We put this here, since the argument profile is syscall-specific
 */
extern int syscall	P((int, ...));
#endif /* DECL_SYSCALL */


#ifdef	SIGDIE2
static	RETSIGTYPE	finish		P((int));
#endif	/* SIGDIE2 */

#ifdef	DEBUG
#ifndef SYS_WINNT
static	RETSIGTYPE	moredebug	P((int));
static	RETSIGTYPE	lessdebug	P((int));
#endif
#else /* not DEBUG */
static	RETSIGTYPE	no_debug	P((int));
#endif	/* not DEBUG */

int 		ntpdmain		P((int, char **));
static void	set_process_priority	P((void));
static void	init_logging		P((char const *));
static void	setup_logfile		P((void));

/*
 * Initialize the logging
 */
void
init_logging(char const *name)
{
	const char *cp;

	/*
	 * Logging.  This may actually work on the gizmo board.  Find a name
	 * to log with by using the basename
	 */
	cp = strrchr(name, '/');
	if (cp == 0)
		cp = name;
	else
		cp++;

#if !defined(VMS)

# ifndef LOG_DAEMON
	openlog(cp, LOG_PID);
# else /* LOG_DAEMON */

#  ifndef LOG_NTP
#	define	LOG_NTP LOG_DAEMON
#  endif
	openlog(cp, LOG_PID | LOG_NDELAY, LOG_NTP);
#  ifdef DEBUG
	if (debug)
		setlogmask(LOG_UPTO(LOG_DEBUG));
	else
#  endif /* DEBUG */
		setlogmask(LOG_UPTO(LOG_DEBUG)); /* @@@ was INFO */
# endif /* LOG_DAEMON */
#endif	/* !SYS_WINNT && !VMS */

	NLOG(NLOG_SYSINFO) /* conditional if clause for conditional syslog */
		msyslog(LOG_NOTICE, "%s", Version);
}


/*
 * See if we should redirect the logfile
 */

void
setup_logfile(
	void
	)
{
	if (HAVE_OPT( LOGFILE )) {
		const char *my_optarg = OPT_ARG( LOGFILE );
		FILE *new_file;

		if(strcmp(my_optarg, "stderr") == 0)
			new_file = stderr;
		else if(strcmp(my_optarg, "stdout") == 0)
			new_file = stdout;
		else
			new_file = fopen(my_optarg, "a");
		if (new_file != NULL) {
			NLOG(NLOG_SYSINFO)
				msyslog(LOG_NOTICE, "logging to file %s", my_optarg);
			if (syslog_file != NULL &&
				fileno(syslog_file) != fileno(new_file))
				(void)fclose(syslog_file);

			syslog_file = new_file;
			syslogit = 0;
		}
		else
			msyslog(LOG_ERR,
				"Cannot open log file %s",
				my_optarg);
	}
}

#ifdef SIM
int
main(
	int argc,
	char *argv[]
	)
{
	return ntpsim(argc, argv);
}
#else /* SIM */
#ifdef NO_MAIN_ALLOWED
CALL(ntpd,"ntpd",ntpdmain);
#else
#ifndef SYS_WINNT
int
main(
	int argc,
	char *argv[]
	)
{
	return ntpdmain(argc, argv);
}
#endif /* SYS_WINNT */
#endif /* NO_MAIN_ALLOWED */
#endif /* SIM */

#ifdef _AIX
/*
 * OK. AIX is different than solaris in how it implements plock().
 * If you do NOT adjust the stack limit, you will get the MAXIMUM
 * stack size allocated and PINNED with you program. To check the 
 * value, use ulimit -a. 
 *
 * To fix this, we create an automatic variable and set our stack limit 
 * to that PLUS 32KB of extra space (we need some headroom).
 *
 * This subroutine gets the stack address.
 *
 * Grover Davidson and Matt Ladendorf
 *
 */
static char *
get_aix_stack(void)
{
	char ch;
	return (&ch);
}

/*
 * Signal handler for SIGDANGER.
 */
static void
catch_danger(int signo)
{
	msyslog(LOG_INFO, "ntpd: setpgid(): %m");
	/* Make the system believe we'll free something, but don't do it! */
	return;
}
#endif /* _AIX */

/*
 * Set the process priority
 */
static void
set_process_priority(void)
{

#ifdef DEBUG
	if (debug > 1)
		msyslog(LOG_DEBUG, "set_process_priority: %s: priority_done is <%d>",
			((priority_done)
			 ? "Leave priority alone"
			 : "Attempt to set priority"
				),
			priority_done);
#endif /* DEBUG */

#ifdef SYS_WINNT
	priority_done += NT_set_process_priority();
#endif

#if defined(HAVE_SCHED_SETSCHEDULER)
	if (!priority_done) {
		extern int config_priority_override, config_priority;
		int pmax, pmin;
		struct sched_param sched;

		pmax = sched_get_priority_max(SCHED_FIFO);
		sched.sched_priority = pmax;
		if ( config_priority_override ) {
			pmin = sched_get_priority_min(SCHED_FIFO);
			if ( config_priority > pmax )
				sched.sched_priority = pmax;
			else if ( config_priority < pmin )
				sched.sched_priority = pmin;
			else
				sched.sched_priority = config_priority;
		}
		if ( sched_setscheduler(0, SCHED_FIFO, &sched) == -1 )
			msyslog(LOG_ERR, "sched_setscheduler(): %m");
		else
			++priority_done;
	}
#endif /* HAVE_SCHED_SETSCHEDULER */
#if defined(HAVE_RTPRIO)
# ifdef RTP_SET
	if (!priority_done) {
		struct rtprio srtp;

		srtp.type = RTP_PRIO_REALTIME;	/* was: RTP_PRIO_NORMAL */
		srtp.prio = 0;		/* 0 (hi) -> RTP_PRIO_MAX (31,lo) */

		if (rtprio(RTP_SET, getpid(), &srtp) < 0)
			msyslog(LOG_ERR, "rtprio() error: %m");
		else
			++priority_done;
	}
# else /* not RTP_SET */
	if (!priority_done) {
		if (rtprio(0, 120) < 0)
			msyslog(LOG_ERR, "rtprio() error: %m");
		else
			++priority_done;
	}
# endif /* not RTP_SET */
#endif  /* HAVE_RTPRIO */
#if defined(NTPD_PRIO) && NTPD_PRIO != 0
# ifdef HAVE_ATT_NICE
	if (!priority_done) {
		errno = 0;
		if (-1 == nice (NTPD_PRIO) && errno != 0)
			msyslog(LOG_ERR, "nice() error: %m");
		else
			++priority_done;
	}
# endif /* HAVE_ATT_NICE */
# ifdef HAVE_BSD_NICE
	if (!priority_done) {
		if (-1 == setpriority(PRIO_PROCESS, 0, NTPD_PRIO))
			msyslog(LOG_ERR, "setpriority() error: %m");
		else
			++priority_done;
	}
# endif /* HAVE_BSD_NICE */
#endif /* NTPD_PRIO && NTPD_PRIO != 0 */
	if (!priority_done)
		msyslog(LOG_ERR, "set_process_priority: No way found to improve our priority");
}


/*
 * Main program.  Initialize us, disconnect us from the tty if necessary,
 * and loop waiting for I/O and/or timer expiries.
 */
int
ntpdmain(
	int argc,
	char *argv[]
	)
{
	l_fp now;
	struct recvbuf *rbuf;
#ifdef _AIX			/* HMS: ifdef SIGDANGER? */
	struct sigaction sa;
#endif

	progname = argv[0];

	initializing = 1;		/* mark that we are initializing */

	{
		int optct = optionProcess(
#ifdef SIM
					  &ntpdsimOptions
#else
					  &ntpdOptions
#endif
					  , argc, argv);
		argc -= optct;
		argv += optct;
	}

	/* HMS: is this lame? Should we process -l first? */

	init_logging(progname);		/* Open the log file */

#ifdef HAVE_UMASK
	{
		mode_t uv;

		uv = umask(0);
		if(uv)
			(void) umask(uv);
		else
			(void) umask(022);
	}
#endif

#if defined(HAVE_GETUID) && !defined(MPE) /* MPE lacks the concept of root */
	{
		uid_t uid;

		uid = getuid();
		if (uid)
		{
			msyslog(LOG_ERR, "ntpd: must be run as root, not uid %ld", (long)uid);
			printf("must be run as root, not uid %ld", (long)uid);
			exit(1);
		}
	}
#endif

#ifdef OPENSSL
	if ((SSLeay() ^ OPENSSL_VERSION_NUMBER) & ~0xff0L) {
		msyslog(LOG_ERR,
		    "ntpd: OpenSSL version mismatch. Built against %lx, you have %lx\n",
		    OPENSSL_VERSION_NUMBER, SSLeay());
		exit(1);
	}
#endif

	/* getstartup(argc, argv); / * startup configuration, may set debug */

#ifdef DEBUG
	debug = DESC(DEBUG_LEVEL).optOccCt;
	if (debug)
	    printf("%s\n", Version);
#endif

/*
 * Enable the Multi-Media Timer for Windows?
 */
#ifdef SYS_WINNT
	if (HAVE_OPT( MODIFYMMTIMER ))
		set_mm_timer(MM_TIMER_HIRES);
#endif

	if (HAVE_OPT( NOFORK ) || HAVE_OPT( QUIT ))
		nofork = 1;

	if (HAVE_OPT( NOVIRTUALIPS ))
		listen_to_virtual_ips = 0;

	if (HAVE_OPT( INTERFACE )) {
#if 0
		int	ifacect = STACKCT_OPT( INTERFACE );
		char**	ifaces  = STACKLST_OPT( INTERFACE );

		/* malloc space for the array of names */
		while (ifacect-- > 0) {
			next_iface = *ifaces++;
		}
#else
		specific_interface = OPT_ARG( INTERFACE );
#endif
	}

	if (HAVE_OPT( NICE ))
		priority_done = 0;

#if defined(HAVE_SCHED_SETSCHEDULER)
	if (HAVE_OPT( PRIORITY )) {
		config_priority = OPT_VALUE_PRIORITY;
		config_priority_override = 1;
		priority_done = 0;
	}
#endif

#ifdef SYS_WINNT
	/*
	 * Initialize the time structures and variables
	 */
	init_winnt_time();
#endif

	setup_logfile();

	/*
	 * Initialize random generator and public key pair
	 */
	get_systime(&now);

	ntp_srandom((int)(now.l_i * now.l_uf));

#ifdef HAVE_DNSREGISTRATION
	/* HMS: does this have to happen this early? */
	msyslog(LOG_INFO, "Attemping to register mDNS");
	if ( DNSServiceRegister (&mdns, 0, 0, NULL, "_ntp._udp", NULL, NULL, htons(NTP_PORT), 0, NULL, NULL, NULL) != kDNSServiceErr_NoError ) {
		msyslog(LOG_ERR, "Unable to register mDNS");
	}
#endif

#if !defined(VMS)
# ifndef NODETACH
	/*
	 * Detach us from the terminal.  May need an #ifndef GIZMO.
	 */
	if (
#  ifdef DEBUG
	    !debug &&
#  endif /* DEBUG */
	    !nofork)
	{
#  ifndef SYS_WINNT
#   ifdef HAVE_DAEMON
		daemon(0, 0);
#   else /* not HAVE_DAEMON */
		if (fork())	/* HMS: What about a -1? */
			exit(0);

		{
#if !defined(F_CLOSEM)
			u_long s;
			int max_fd;
#endif /* not F_CLOSEM */

#if defined(F_CLOSEM)
			/*
			 * From 'Writing Reliable AIX Daemons,' SG24-4946-00,
			 * by Eric Agar (saves us from doing 32767 system
			 * calls)
			 */
			if (fcntl(0, F_CLOSEM, 0) == -1)
			    msyslog(LOG_ERR, "ntpd: failed to close open files(): %m");
#else  /* not F_CLOSEM */

# if defined(HAVE_SYSCONF) && defined(_SC_OPEN_MAX)
			max_fd = sysconf(_SC_OPEN_MAX);
# else /* HAVE_SYSCONF && _SC_OPEN_MAX */
			max_fd = getdtablesize();
# endif /* HAVE_SYSCONF && _SC_OPEN_MAX */
			for (s = 0; s < max_fd; s++)
				(void) close((int)s);
#endif /* not F_CLOSEM */
			(void) open("/", 0);
			(void) dup2(0, 1);
			(void) dup2(0, 2);
#ifdef SYS_DOMAINOS
			{
				uid_$t puid;
				status_$t st;

				proc2_$who_am_i(&puid);
				proc2_$make_server(&puid, &st);
			}
#endif /* SYS_DOMAINOS */
#if defined(HAVE_SETPGID) || defined(HAVE_SETSID)
# ifdef HAVE_SETSID
			if (setsid() == (pid_t)-1)
				msyslog(LOG_ERR, "ntpd: setsid(): %m");
# else
			if (setpgid(0, 0) == -1)
				msyslog(LOG_ERR, "ntpd: setpgid(): %m");
# endif
#else /* HAVE_SETPGID || HAVE_SETSID */
			{
# if defined(TIOCNOTTY)
				int fid;

				fid = open("/dev/tty", 2);
				if (fid >= 0)
				{
					(void) ioctl(fid, (u_long) TIOCNOTTY, (char *) 0);
					(void) close(fid);
				}
# endif /* defined(TIOCNOTTY) */
# ifdef HAVE_SETPGRP_0
				(void) setpgrp();
# else /* HAVE_SETPGRP_0 */
				(void) setpgrp(0, getpid());
# endif /* HAVE_SETPGRP_0 */
			}
#endif /* HAVE_SETPGID || HAVE_SETSID */
#ifdef _AIX
			/* Don't get killed by low-on-memory signal. */
			sa.sa_handler = catch_danger;
			sigemptyset(&sa.sa_mask);
			sa.sa_flags = SA_RESTART;

			(void) sigaction(SIGDANGER, &sa, NULL);
#endif /* _AIX */
		}
#   endif /* not HAVE_DAEMON */
#  endif /* SYS_WINNT */
	}
# endif /* NODETACH */
#endif /* VMS */

	setup_logfile();	/* We lost any redirect when we daemonized */

#ifdef SCO5_CLOCK
	/*
	 * SCO OpenServer's system clock offers much more precise timekeeping
	 * on the base CPU than the other CPUs (for multiprocessor systems),
	 * so we must lock to the base CPU.
	 */
	{
	    int fd = open("/dev/at1", O_RDONLY);
	    if (fd >= 0) {
		int zero = 0;
		if (ioctl(fd, ACPU_LOCK, &zero) < 0)
		    msyslog(LOG_ERR, "cannot lock to base CPU: %m");
		close( fd );
	    } /* else ...
	       *   If we can't open the device, this probably just isn't
	       *   a multiprocessor system, so we're A-OK.
	       */
	}
#endif

#if defined(HAVE_MLOCKALL) && defined(MCL_CURRENT) && defined(MCL_FUTURE)
# ifdef HAVE_SETRLIMIT
	/*
	 * Set the stack limit to something smaller, so that we don't lock a lot
	 * of unused stack memory.
	 */
	{
	    struct rlimit rl;

	    /* HMS: must make the rlim_cur amount configurable */
	    if (getrlimit(RLIMIT_STACK, &rl) != -1
		&& (rl.rlim_cur = 50 * 4096) < rl.rlim_max)
	    {
		    if (setrlimit(RLIMIT_STACK, &rl) == -1)
		    {
			    msyslog(LOG_ERR,
				"Cannot adjust stack limit for mlockall: %m");
		    }
	    }
#  ifdef RLIMIT_MEMLOCK
	    /*
	     * The default RLIMIT_MEMLOCK is very low on Linux systems.
	     * Unless we increase this limit malloc calls are likely to
	     * fail if we drop root privlege.  To be useful the value
	     * has to be larger than the largest ntpd resident set size.
	     */
	    rl.rlim_cur = rl.rlim_max = 32*1024*1024;
	    if (setrlimit(RLIMIT_MEMLOCK, &rl) == -1) {
	    	msyslog(LOG_ERR, "Cannot set RLIMIT_MEMLOCK: %m");
	    }
#  endif /* RLIMIT_MEMLOCK */
	}
# endif /* HAVE_SETRLIMIT */
	/*
	 * lock the process into memory
	 */
	if (mlockall(MCL_CURRENT|MCL_FUTURE) < 0)
		msyslog(LOG_ERR, "mlockall(): %m");
#else /* not (HAVE_MLOCKALL && MCL_CURRENT && MCL_FUTURE) */
# ifdef HAVE_PLOCK
#  ifdef PROCLOCK
#   ifdef _AIX
	/* 
	 * set the stack limit for AIX for plock().
	 * see get_aix_stack() for more info.
	 */
	if (ulimit(SET_STACKLIM, (get_aix_stack() - 8*4096)) < 0)
	{
		msyslog(LOG_ERR,"Cannot adjust stack limit for plock on AIX: %m");
	}
#   endif /* _AIX */
	/*
	 * lock the process into memory
	 */
	if (plock(PROCLOCK) < 0)
		msyslog(LOG_ERR, "plock(PROCLOCK): %m");
#  else /* not PROCLOCK */
#   ifdef TXTLOCK
	/*
	 * Lock text into ram
	 */
	if (plock(TXTLOCK) < 0)
		msyslog(LOG_ERR, "plock(TXTLOCK) error: %m");
#   else /* not TXTLOCK */
	msyslog(LOG_ERR, "plock() - don't know what to lock!");
#   endif /* not TXTLOCK */
#  endif /* not PROCLOCK */
# endif /* HAVE_PLOCK */
#endif /* not (HAVE_MLOCKALL && MCL_CURRENT && MCL_FUTURE) */

	/*
	 * Set up signals we pay attention to locally.
	 */
#ifdef SIGDIE1
	(void) signal_no_reset(SIGDIE1, finish);
#endif	/* SIGDIE1 */
#ifdef SIGDIE2
	(void) signal_no_reset(SIGDIE2, finish);
#endif	/* SIGDIE2 */
#ifdef SIGDIE3
	(void) signal_no_reset(SIGDIE3, finish);
#endif	/* SIGDIE3 */
#ifdef SIGDIE4
	(void) signal_no_reset(SIGDIE4, finish);
#endif	/* SIGDIE4 */

#ifdef SIGBUS
	(void) signal_no_reset(SIGBUS, finish);
#endif /* SIGBUS */

#if !defined(SYS_WINNT) && !defined(VMS)
# ifdef DEBUG
	(void) signal_no_reset(MOREDEBUGSIG, moredebug);
	(void) signal_no_reset(LESSDEBUGSIG, lessdebug);
# else
	(void) signal_no_reset(MOREDEBUGSIG, no_debug);
	(void) signal_no_reset(LESSDEBUGSIG, no_debug);
# endif /* DEBUG */
#endif /* !SYS_WINNT && !VMS */

	/*
	 * Set up signals we should never pay attention to.
	 */
#if defined SIGPIPE
	(void) signal_no_reset(SIGPIPE, SIG_IGN);
#endif	/* SIGPIPE */

	/*
	 * Call the init_ routines to initialize the data structures.
	 *
	 * Exactly what command-line options are we expecting here?
	 */
	init_auth();
	init_util();
	init_restrict();
	init_mon();
	init_timer();
#if defined (HAVE_IO_COMPLETION_PORT)
	init_io_completion_port();
#endif
	init_lib();
	init_request();
	init_control();
	init_peer();
#ifdef REFCLOCK
	init_refclock();
#endif
	set_process_priority();
	init_proto();		/* Call at high priority */
	init_io();
	init_loopfilter();
	mon_start(MON_ON);	/* monitor on by default now	  */
				/* turn off in config if unwanted */

	/*
	 * Get the configuration.  This is done in a separate module
	 * since this will definitely be different for the gizmo board.
	 */

	getconfig(argc, argv);

	loop_config(LOOP_DRIFTCOMP, old_drift / 1e6);
#ifdef OPENSSL
	crypto_setup();
#endif /* OPENSSL */
	initializing = 0;

#ifdef HAVE_DROPROOT
	if( droproot ) {
		/* Drop super-user privileges and chroot now if the OS supports this */

#ifdef HAVE_LINUX_CAPABILITIES
		/* set flag: keep privileges accross setuid() call (we only really need cap_sys_time): */
		if( prctl( PR_SET_KEEPCAPS, 1L, 0L, 0L, 0L ) == -1 ) {
			msyslog( LOG_ERR, "prctl( PR_SET_KEEPCAPS, 1L ) failed: %m" );
			exit(-1);
		}
#else
		/* we need a user to switch to */
		if( user == NULL ) {
			msyslog(LOG_ERR, "Need user name to drop root privileges (see -u flag!)" );
			exit(-1);
		}
#endif /* HAVE_LINUX_CAPABILITIES */
	
		if (user != NULL) {
			if (isdigit((unsigned char)*user)) {
				sw_uid = (uid_t)strtoul(user, &endp, 0);
				if (*endp != '\0') 
					goto getuser;
			} else {
getuser:	
				if ((pw = getpwnam(user)) != NULL) {
					sw_uid = pw->pw_uid;
				} else {
					errno = 0;
					msyslog(LOG_ERR, "Cannot find user `%s'", user);
					exit (-1);
				}
			}
		}
		if (group != NULL) {
			if (isdigit((unsigned char)*group)) {
				sw_gid = (gid_t)strtoul(group, &endp, 0);
				if (*endp != '\0') 
					goto getgroup;
			} else {
getgroup:	
				if ((gr = getgrnam(group)) != NULL) {
					sw_gid = gr->gr_gid;
				} else {
					errno = 0;
					msyslog(LOG_ERR, "Cannot find group `%s'", group);
					exit (-1);
				}
			}
		}
		
		if( chrootdir ) {
			/* make sure cwd is inside the jail: */
			if( chdir(chrootdir) ) {
				msyslog(LOG_ERR, "Cannot chdir() to `%s': %m", chrootdir);
				exit (-1);
			}
			if( chroot(chrootdir) ) {
				msyslog(LOG_ERR, "Cannot chroot() to `%s': %m", chrootdir);
				exit (-1);
			}
		}
		if (group && setgid(sw_gid)) {
			msyslog(LOG_ERR, "Cannot setgid() to group `%s': %m", group);
			exit (-1);
		}
		if (group && setegid(sw_gid)) {
			msyslog(LOG_ERR, "Cannot setegid() to group `%s': %m", group);
			exit (-1);
		}
		if (user && setuid(sw_uid)) {
			msyslog(LOG_ERR, "Cannot setuid() to user `%s': %m", user);
			exit (-1);
		}
		if (user && seteuid(sw_uid)) {
			msyslog(LOG_ERR, "Cannot seteuid() to user `%s': %m", user);
			exit (-1);
		}
	
#ifndef HAVE_LINUX_CAPABILITIES
		/*
		 * for now assume that the privilege to bind to privileged ports
		 * is associated with running with uid 0 - should be refined on
		 * ports that allow binding to NTP_PORT with uid != 0
		 */
		disable_dynamic_updates |= (sw_uid != 0);  /* also notifies routing message listener */
#endif

		if (disable_dynamic_updates && interface_interval) {
			interface_interval = 0;
			msyslog(LOG_INFO, "running in unprivileged mode disables dynamic interface tracking");
		}

#ifdef HAVE_LINUX_CAPABILITIES
		do {
			/*
			 *  We may be running under non-root uid now, but we still hold full root privileges!
			 *  We drop all of them, except for the crucial one or two: cap_sys_time and
			 *  cap_net_bind_service if doing dynamic interface tracking.
			 */
			cap_t caps;
			char *captext = interface_interval ?
			       	"cap_sys_time,cap_net_bind_service=ipe" :
			       	"cap_sys_time=ipe";
			if( ! ( caps = cap_from_text( captext ) ) ) {
				msyslog( LOG_ERR, "cap_from_text() failed: %m" );
				exit(-1);
			}
			if( cap_set_proc( caps ) == -1 ) {
				msyslog( LOG_ERR, "cap_set_proc() failed to drop root privileges: %m" );
				exit(-1);
			}
			cap_free( caps );
		} while(0);
#endif /* HAVE_LINUX_CAPABILITIES */

	}    /* if( droproot ) */
#endif /* HAVE_DROPROOT */
	
	/*
	 * Report that we're up to any trappers
	 */
	report_event(EVNT_SYSRESTART, (struct peer *)0);

	/*
	 * Use select() on all on all input fd's for unlimited
	 * time.  select() will terminate on SIGALARM or on the
	 * reception of input.	Using select() means we can't do
	 * robust signal handling and we get a potential race
	 * between checking for alarms and doing the select().
	 * Mostly harmless, I think.
	 */
	/* On VMS, I suspect that select() can't be interrupted
	 * by a "signal" either, so I take the easy way out and
	 * have select() time out after one second.
	 * System clock updates really aren't time-critical,
	 * and - lacking a hardware reference clock - I have
	 * yet to learn about anything else that is.
	 */
#if defined(HAVE_IO_COMPLETION_PORT)

	for (;;) {
		int tot_full_recvbufs = GetReceivedBuffers();
#else /* normal I/O */

	BLOCK_IO_AND_ALARM();
	was_alarmed = 0;
	for (;;)
	{
# if !defined(HAVE_SIGNALED_IO) 
		extern fd_set activefds;
		extern int maxactivefd;

		fd_set rdfdes;
		int nfound;
# endif

		if (alarm_flag) 	/* alarmed? */
		{
			was_alarmed = 1;
			alarm_flag = 0;
		}

		if (!was_alarmed && has_full_recv_buffer() == ISC_FALSE)
		{
			/*
			 * Nothing to do.  Wait for something.
			 */
# ifndef HAVE_SIGNALED_IO
			rdfdes = activefds;
#  if defined(VMS) || defined(SYS_VXWORKS)
			/* make select() wake up after one second */
			{
				struct timeval t1;

				t1.tv_sec = 1; t1.tv_usec = 0;
				nfound = select(maxactivefd+1, &rdfdes, (fd_set *)0,
						(fd_set *)0, &t1);
			}
#  else
			nfound = select(maxactivefd+1, &rdfdes, (fd_set *)0,
					(fd_set *)0, (struct timeval *)0);
#  endif /* VMS */
			if (nfound > 0)
			{
				l_fp ts;

				get_systime(&ts);

				(void)input_handler(&ts);
			}
			else if (nfound == -1 && errno != EINTR)
				netsyslog(LOG_ERR, "select() error: %m");
#  ifdef DEBUG
			else if (debug > 5)
				netsyslog(LOG_DEBUG, "select(): nfound=%d, error: %m", nfound);
#  endif /* DEBUG */
# else /* HAVE_SIGNALED_IO */
                        
			wait_for_signal();
# endif /* HAVE_SIGNALED_IO */
			if (alarm_flag) 	/* alarmed? */
			{
				was_alarmed = 1;
				alarm_flag = 0;
			}
		}

		if (was_alarmed)
		{
			UNBLOCK_IO_AND_ALARM();
			/*
			 * Out here, signals are unblocked.  Call timer routine
			 * to process expiry.
			 */
			timer();
			was_alarmed = 0;
                        BLOCK_IO_AND_ALARM();
		}

#endif /* HAVE_IO_COMPLETION_PORT */

#ifdef DEBUG_TIMING
		{
			l_fp pts;
			l_fp tsa, tsb;
			int bufcount = 0;
			
			get_systime(&pts);
			tsa = pts;
#endif
			rbuf = get_full_recv_buffer();
			while (rbuf != NULL)
			{
				if (alarm_flag)
				{
					was_alarmed = 1;
					alarm_flag = 0;
				}
				UNBLOCK_IO_AND_ALARM();

				if (was_alarmed)
				{	/* avoid timer starvation during lengthy I/O handling */
					timer();
					was_alarmed = 0;
				}

				/*
				 * Call the data procedure to handle each received
				 * packet.
				 */
				if (rbuf->receiver != NULL)	/* This should always be true */
				{
#ifdef DEBUG_TIMING
					l_fp dts = pts;

					L_SUB(&dts, &rbuf->recv_time);
					DPRINTF(2, ("processing timestamp delta %s (with prec. fuzz)\n", lfptoa(&dts, 9)));
					collect_timing(rbuf, "buffer processing delay", 1, &dts);
					bufcount++;
#endif
					(rbuf->receiver)(rbuf);
				} else {
					msyslog(LOG_ERR, "receive buffer corruption - receiver found to be NULL - ABORTING");
					abort();
				}

				BLOCK_IO_AND_ALARM();
				freerecvbuf(rbuf);
				rbuf = get_full_recv_buffer();
			}
#ifdef DEBUG_TIMING
			get_systime(&tsb);
			L_SUB(&tsb, &tsa);
			if (bufcount) {
				collect_timing(NULL, "processing", bufcount, &tsb);
				DPRINTF(2, ("processing time for %d buffers %s\n", bufcount, lfptoa(&tsb, 9)));
			}
		}
#endif

		/*
		 * Go around again
		 */
	}
	UNBLOCK_IO_AND_ALARM();
	return 1;
}


#ifdef SIGDIE2
/*
 * finish - exit gracefully
 */
static RETSIGTYPE
finish(
	int sig
	)
{

	msyslog(LOG_NOTICE, "ntpd exiting on signal %d", sig);
	write_stats();
#ifdef HAVE_DNSREGISTRATION
	if (mdns != NULL)
	DNSServiceRefDeallocate(mdns);
#endif

	switch (sig)
	{
# ifdef SIGBUS
		case SIGBUS:
		printf("\nfinish(SIGBUS)\n");
		exit(0);
# endif
		case 0: 		/* Should never happen... */
		return;
		default:
		exit(0);
	}
}
#endif	/* SIGDIE2 */


#ifdef DEBUG
#ifndef SYS_WINNT
/*
 * moredebug - increase debugging verbosity
 */
static RETSIGTYPE
moredebug(
	int sig
	)
{
	int saved_errno = errno;

	if (debug < 255)
	{
		debug++;
		msyslog(LOG_DEBUG, "debug raised to %d", debug);
	}
	errno = saved_errno;
}

/*
 * lessdebug - decrease debugging verbosity
 */
static RETSIGTYPE
lessdebug(
	int sig
	)
{
	int saved_errno = errno;

	if (debug > 0)
	{
		debug--;
		msyslog(LOG_DEBUG, "debug lowered to %d", debug);
	}
	errno = saved_errno;
}
#endif
#else /* not DEBUG */
#ifndef SYS_WINNT
/*
 * no_debug - We don't do the debug here.
 */
static RETSIGTYPE
no_debug(
	int sig
	)
{
	int saved_errno = errno;

	msyslog(LOG_DEBUG, "ntpd not compiled for debugging (signal %d)", sig);
	errno = saved_errno;
}
#endif  /* not SYS_WINNT */
#endif	/* not DEBUG */
