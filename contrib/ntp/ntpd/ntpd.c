/*
 * ntpd.c - main program for the fixed point NTP daemon
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "ntpd.h"
#include "ntp_io.h"
#include "ntp_stdlib.h"

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif
#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif
#include <stdio.h>
#ifndef SYS_WINNT
# if !defined(VMS)	/*wjm*/
#  include <sys/param.h>
# endif /* VMS */
# include <sys/signal.h>
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
# include "../libntp/log.h"
# include <crtdbg.h>
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

#ifdef PUBKEY
#include "ntp_crypto.h"
#endif /* PUBKEY */

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

#if defined SYS_WINNT
/* handles for various threads, process, and objects */
HANDLE ResolverThreadHandle = NULL;
/* variables used to inform the Service Control Manager of our current state */
SERVICE_STATUS ssStatus;
SERVICE_STATUS_HANDLE	sshStatusHandle;
HANDLE WaitHandles[2] = { NULL, NULL };
char szMsgPath[255];
static BOOL WINAPI OnConsoleEvent(DWORD dwCtrlType);
#endif /* SYS_WINNT */

/*
 * Scheduling priority we run at
 */
#define NTPD_PRIO	(-12)

int priority_done = 2;		/* 0 - Set priority */
				/* 1 - priority is OK where it is */
				/* 2 - Don't set priority */
				/* 1 and 2 are pretty much the same */

/*
 * Debugging flag
 */
volatile int debug;

/*
 * No-fork flag.  If set, we do not become a background daemon.
 */
int nofork;

/*
 * Initializing flag.  All async routines watch this and only do their
 * thing when it is clear.
 */
int initializing;

/*
 * Version declaration
 */
extern const char *Version;

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
static	RETSIGTYPE	moredebug	P((int));
static	RETSIGTYPE	lessdebug	P((int));
#else /* not DEBUG */
static	RETSIGTYPE	no_debug	P((int));
#endif	/* not DEBUG */

int 		ntpdmain		P((int, char **));
static void	set_process_priority	P((void));


#ifdef NO_MAIN_ALLOWED
CALL(ntpd,"ntpd",ntpdmain);
#else
int
main(
	int argc,
	char *argv[]
	)
{
	return ntpdmain(argc, argv);
}
#endif

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
	char *cp;
#ifdef AUTOKEY
	u_int n;
	char hostname[MAXFILENAME];
#endif /* AUTOKEY */
	struct recvbuf *rbuflist;
	struct recvbuf *rbuf;
#ifdef _AIX			/* HMS: ifdef SIGDANGER? */
	struct sigaction sa;
#endif

	initializing = 1;		/* mark that we are initializing */
	debug = 0;			/* no debugging by default */
	nofork = 0;			/* will fork by default */

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

#ifdef HAVE_GETUID
	{
		uid_t uid;

		uid = getuid();
		if (uid)
		{
			msyslog(LOG_ERR, "ntpd: must be run as root, not uid %ld", (long)uid);
			exit(1);
		}
	}
#endif

#ifdef SYS_WINNT
	/* Set the Event-ID message-file name. */
	if (!GetModuleFileName(NULL, szMsgPath, sizeof(szMsgPath))) {
		msyslog(LOG_ERR, "GetModuleFileName(PGM_EXE_FILE) failed: %m\n");
		exit(1);
	}
	addSourceToRegistry("NTP", szMsgPath);
#endif
	getstartup(argc, argv); /* startup configuration, may set debug */

	/*
	 * Initialize random generator and public key pair
	 */
	get_systime(&now);
	SRANDOM((int)(now.l_i * now.l_uf));

#if !defined(VMS)
# ifndef NODETACH
	/*
	 * Detach us from the terminal.  May need an #ifndef GIZMO.
	 */
#  ifdef DEBUG
	if (!debug && !nofork)
#  else /* DEBUG */
	if (!nofork)
#  endif /* DEBUG */
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

			/*
			 * From 'Writing Reliable AIX Daemons,' SG24-4946-00,
			 * by Eric Agar (saves us from doing 32767 system
			 * calls)
			 */
#if defined(F_CLOSEM)
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
#  else /* SYS_WINNT */

		{
			SERVICE_TABLE_ENTRY dispatchTable[] = {
				{ TEXT("NetworkTimeProtocol"), (LPSERVICE_MAIN_FUNCTION)service_main },
				{ NULL, NULL }
			};

			/* daemonize */
			if (!StartServiceCtrlDispatcher(dispatchTable))
			{
				msyslog(LOG_ERR, "StartServiceCtrlDispatcher: %m");
				ExitProcess(2);
			}
		}
#  endif /* SYS_WINNT */
	}
# endif /* NODETACH */
# if defined(SYS_WINNT) && !defined(NODETACH)
	else
		service_main(argc, argv);
	return 0;	/* must return a value */
} /* end main */

/*
 * If this runs as a service under NT, the main thread will block at
 * StartServiceCtrlDispatcher() and another thread will be started by the
 * Service Control Dispatcher which will begin execution at the routine
 * specified in that call (viz. service_main)
 */
void
service_main(
	DWORD argc,
	LPTSTR *argv
	)
{
	char *cp;
	struct recvbuf *rbuflist;
	struct recvbuf *rbuf;
#ifdef AUTOKEY
	u_int n;
	char hostname[MAXFILENAME];
#endif /* AUTOKEY */
	if(!debug)
	{
		/* register our service control handler */
		if (!(sshStatusHandle = RegisterServiceCtrlHandler( TEXT("NetworkTimeProtocol"),
									(LPHANDLER_FUNCTION)service_ctrl)))
		{
			msyslog(LOG_ERR, "RegisterServiceCtrlHandler failed: %m");
			return;
		}

		/* report pending status to Service Control Manager */
		ssStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
		ssStatus.dwCurrentState = SERVICE_START_PENDING;
		ssStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
		ssStatus.dwWin32ExitCode = NO_ERROR;
		ssStatus.dwServiceSpecificExitCode = 0;
		ssStatus.dwCheckPoint = 1;
		ssStatus.dwWaitHint = 5000;
		if (!SetServiceStatus(sshStatusHandle, &ssStatus))
		{
			msyslog(LOG_ERR, "SetServiceStatus: %m");
			ssStatus.dwCurrentState = SERVICE_STOPPED;
			SetServiceStatus(sshStatusHandle, &ssStatus);
			return;
		}

	}  /* debug */
# endif /* defined(SYS_WINNT) && !defined(NODETACH) */
#endif /* VMS */

	/*
	 * Logging.  This may actually work on the gizmo board.  Find a name
	 * to log with by using the basename of argv[0]
	 */
	cp = strrchr(argv[0], '/');
	if (cp == 0)
		cp = argv[0];
	else
		cp++;

	debug = 0; /* will be immediately re-initialized 8-( */
	getstartup(argc, argv); /* startup configuration, catch logfile this time */

#if !defined(SYS_WINNT) && !defined(VMS)

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

#ifdef SYS_WINNT
	/* GMS 1/18/1997
	 * TODO: lock the process in memory using SetProcessWorkingSetSize() and VirtualLock() functions
	 *
	 process_handle = GetCurrentProcess();
	 if (SetProcessWorkingSetSize(process_handle, 2097152 , 4194304 ) == TRUE) {
	 if (VirtualLock(0 , 4194304) == FALSE)
	 msyslog(LOG_ERR, "VirtualLock() failed: %m");
	 } else {
	 msyslog(LOG_ERR, "SetProcessWorkingSetSize() failed: %m");
	 }
	*/
#endif /* SYS_WINNT */

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
		    msyslog(LOG_ERR, "cannot lock to base CPU: %m\n");
		close( fd );
	    } /* else ...
	       *   If we can't open the device, this probably just isn't
	       *   a multiprocessor system, so we're A-OK.
	       */
	}
#endif

#if defined(HAVE_MLOCKALL) && defined(MCL_CURRENT) && defined(MCL_FUTURE)
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
	 * see get_aix_stack for more info.
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

#if defined SYS_WINNT
	if (!SetConsoleCtrlHandler(OnConsoleEvent, TRUE)) {
		msyslog(LOG_ERR, "Can't set console control handler: %m");
	}
#endif

	/*
	 * Call the init_ routines to initialize the data structures.
	 */
#if defined (HAVE_IO_COMPLETION_PORT)
	init_io_completion_port();
	init_winnt_time();
#endif
	init_auth();
	init_util();
	init_restrict();
	init_mon();
	init_timer();
	init_lib();
	init_random();
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
	 * Get configuration.  This (including argument list parsing) is
	 * done in a separate module since this will definitely be different
	 * for the gizmo board. While at it, save the host name for later
	 * along with the length. The crypto needs this.
	 */
#ifdef DEBUG
	debug = 0;
#endif
	getconfig(argc, argv);
#ifdef AUTOKEY
	gethostname(hostname, MAXFILENAME);
	n = strlen(hostname) + 1;
	sys_hostname = emalloc(n);
	memcpy(sys_hostname, hostname, n);
#ifdef PUBKEY
	crypto_setup();
#endif /* PUBKEY */
#endif /* AUTOKEY */
	initializing = 0;

#if defined(SYS_WINNT) && !defined(NODETACH)
# if defined(DEBUG)
	if(!debug)
	{
# endif
		/* report to the service control manager that the service is running */
		ssStatus.dwCurrentState = SERVICE_RUNNING;
		ssStatus.dwWin32ExitCode = NO_ERROR;
		if (!SetServiceStatus(sshStatusHandle, &ssStatus))
		{
			msyslog(LOG_ERR, "SetServiceStatus: %m");
			if (ResolverThreadHandle != NULL)
				CloseHandle(ResolverThreadHandle);
			ssStatus.dwCurrentState = SERVICE_STOPPED;
			SetServiceStatus(sshStatusHandle, &ssStatus);
			return;
		}
# if defined(DEBUG)
	}
# endif  
#endif

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
		WaitHandles[0] = CreateEvent(NULL, FALSE, FALSE, NULL); /* exit reques */
		WaitHandles[1] = get_timer_handle();

		for (;;) {
			DWORD Index = WaitForMultipleObjectsEx(sizeof(WaitHandles)/sizeof(WaitHandles[0]), WaitHandles, FALSE, 1000, MWMO_ALERTABLE);
			switch (Index) {
				case WAIT_OBJECT_0 + 0 : /* exit request */
					exit(0);
				break;

				case WAIT_OBJECT_0 + 1 : /* timer */
					timer();
				break;
				case WAIT_OBJECT_0 + 2 : { /* Windows message */
					MSG msg;
					while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
						if (msg.message == WM_QUIT) {
							exit(0);
						}
						DispatchMessage(&msg);
					}
				}
				break;

				case WAIT_IO_COMPLETION : /* loop */
				case WAIT_TIMEOUT :
				break;
				
			} /* switch */
			rbuflist = getrecvbufs();	/* get received buffers */

#else /* normal I/O */

	was_alarmed = 0;
	rbuflist = (struct recvbuf *)0;
	for (;;)
	{
# if !defined(HAVE_SIGNALED_IO) 
		extern fd_set activefds;
		extern int maxactivefd;

		fd_set rdfdes;
		int nfound;
# elif defined(HAVE_SIGNALED_IO)
		block_io_and_alarm();
# endif

		rbuflist = getrecvbufs();	/* get received buffers */
		if (alarm_flag) 	/* alarmed? */
		{
			was_alarmed = 1;
			alarm_flag = 0;
		}

		if (!was_alarmed && rbuflist == (struct recvbuf *)0)
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
				msyslog(LOG_ERR, "select() error: %m");
			else if (debug > 2) {
				msyslog(LOG_DEBUG, "select(): nfound=%d, error: %m", nfound);
			}
# else /* HAVE_SIGNALED_IO */
                        
			wait_for_signal();
# endif /* HAVE_SIGNALED_IO */
			if (alarm_flag) 	/* alarmed? */
			{
				was_alarmed = 1;
				alarm_flag = 0;
			}
			rbuflist = getrecvbufs();  /* get received buffers */
		}
# ifdef HAVE_SIGNALED_IO
		unblock_io_and_alarm();
# endif /* HAVE_SIGNALED_IO */

		/*
		 * Out here, signals are unblocked.  Call timer routine
		 * to process expiry.
		 */
		if (was_alarmed)
		{
			timer();
			was_alarmed = 0;
		}

#endif /* HAVE_IO_COMPLETION_PORT */
		/*
		 * Call the data procedure to handle each received
		 * packet.
		 */
		while (rbuflist != (struct recvbuf *)0)
		{
			rbuf = rbuflist;
			rbuflist = rbuf->next;
			(rbuf->receiver)(rbuf);
			freerecvbuf(rbuf);
		}
#if defined DEBUG && defined SYS_WINNT
		if (debug > 4)
		    printf("getrecvbufs: %ld handler interrupts, %ld frames\n",
			   handler_calls, handler_pkts);
#endif

		/*
		 * Go around again
		 */
	}
	exit(1); /* unreachable */
	return 1;		/* DEC OSF cc braindamage */
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
#else /* not DEBUG */
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
#endif	/* not DEBUG */

#ifdef SYS_WINNT
/* service_ctrl - control handler for NTP service
 * signals the service_main routine of start/stop requests
 * from the control panel or other applications making
 * win32API calls
 */
void
service_ctrl(
	DWORD dwCtrlCode
	)
{
	DWORD  dwState = SERVICE_RUNNING;

	/* Handle the requested control code */
	switch(dwCtrlCode)
	{
		case SERVICE_CONTROL_PAUSE:
		/* see no reason to support this */
		break;

		case SERVICE_CONTROL_CONTINUE:
		/* see no reason to support this */
		break;

		case SERVICE_CONTROL_STOP:
			dwState = SERVICE_STOP_PENDING;
			/*
			 * Report the status, specifying the checkpoint and waithint,
			 *	before setting the termination event.
			 */
			ssStatus.dwCurrentState = dwState;
			ssStatus.dwWin32ExitCode = NO_ERROR;
			ssStatus.dwWaitHint = 3000;
			if (!SetServiceStatus(sshStatusHandle, &ssStatus))
			{
				msyslog(LOG_ERR, "SetServiceStatus: %m");
			}
			if (WaitHandles[0] != NULL) {
				SetEvent(WaitHandles[0]);
			}
		return;

		case SERVICE_CONTROL_INTERROGATE:
		/* Update the service status */
		break;

		default:
		/* invalid control code */
		break;

	}

	ssStatus.dwCurrentState = dwState;
	ssStatus.dwWin32ExitCode = NO_ERROR;
	if (!SetServiceStatus(sshStatusHandle, &ssStatus))
	{
		msyslog(LOG_ERR, "SetServiceStatus: %m");
	}
}

static BOOL WINAPI 
OnConsoleEvent(  
	DWORD dwCtrlType
	)
{
	switch (dwCtrlType) {
		case CTRL_BREAK_EVENT :
			if (debug > 0) {
				debug <<= 1;
			}
			else {
				debug = 1;
			}
			if (debug > 8) {
				debug = 0;
			}
			printf("debug level %d\n", debug);
		break ;

		case CTRL_C_EVENT  :
		case CTRL_CLOSE_EVENT :
		case CTRL_SHUTDOWN_EVENT :
			if (WaitHandles[0] != NULL) {
				SetEvent(WaitHandles[0]);
			}
		break;

		default :
			return FALSE;


	}
	return TRUE;;
}


/*
 *  NT version of exit() - all calls to exit() should be routed to
 *  this function.
 */
void
service_exit(
	int status
	)
{
	if (!debug) { /* did not become a service, simply exit */
		/* service mode, need to have the service_main routine
		 * register with the service control manager that the 
		 * service has stopped running, before exiting
		 */
		ssStatus.dwCurrentState = SERVICE_STOPPED;
		SetServiceStatus(sshStatusHandle, &ssStatus);

	}
	uninit_io_completion_port();
	reset_winnt_time();

# if defined _MSC_VER
	_CrtDumpMemoryLeaks();
# endif 
#undef exit	
	exit(status);
}

#endif /* SYS_WINNT */
