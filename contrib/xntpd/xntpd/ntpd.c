/* ntpd.c,v 3.1 1993/07/06 01:11:32 jbj Exp
 * ntpd.c - main program for the fixed point NTP daemon
 */
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/signal.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/resource.h>
#if defined(SYS_HPUX)
#include <sys/lock.h>
#include <sys/rtprio.h>
#endif

#if defined(SYS_SVR4)
#include <termios.h>
#endif

#if (defined(SYS_SOLARIS)&&!defined(bsd)) || defined(__svr4__)
#include <termios.h>
#endif

#include "ntpd.h"
#include "ntp_select.h"
#include "ntp_io.h"
#include "ntp_stdlib.h"

#ifdef LOCK_PROCESS
#include <sys/lock.h>
#endif

/*
 * Signals we catch for debugging.  If not debugging we ignore them.
 */
#define	MOREDEBUGSIG	SIGUSR1
#define	LESSDEBUGSIG	SIGUSR2

/*
 * Signals which terminate us gracefully.
 */
#define	SIGDIE1		SIGHUP
#define	SIGDIE2		SIGINT
#define	SIGDIE3		SIGQUIT
#define	SIGDIE4		SIGTERM

/*
 * Scheduling priority we run at
 */
#define	NTPD_PRIO	(-12)

/*
 * Debugging flag
 */
int debug;

/*
 * Initializing flag.  All async routines watch this and only do their
 * thing when it is clear.
 */
int initializing;

/*
 * Version declaration
 */
extern char *Version;

/*
 * Alarm flag.  Imported from timer module
 */
extern int alarm_flag;

#if !defined(SYS_386BSD) && !defined(SYS_BSDI)
/*
 * We put this here, since the argument profile is syscall-specific
 */
extern int syscall	P((int, struct timeval *, struct timeval *));
#endif /* !SYS_386BSD */

#ifdef	SIGDIE1
static	RETSIGTYPE	finish		P((int));
#endif	/* SIGDIE1 */

#ifdef	DEBUG
static	RETSIGTYPE	moredebug	P((int));
static	RETSIGTYPE	lessdebug	P((int));
#endif	/* DEBUG */

/*
 * Main program.  Initialize us, disconnect us from the tty if necessary,
 * and loop waiting for I/O and/or timer expiries.
 */
void
main(argc, argv)
	int argc;
	char *argv[];
{
	char *cp;
	int was_alarmed;
	struct recvbuf *rbuflist;
	struct recvbuf *rbuf;

	initializing = 1;	/* mark that we are initializing */
	debug = 0;		/* no debugging by default */

	getstartup(argc, argv);	/* startup configuration, may set debug */

#ifndef NODETACH
	/*
	 * Detach us from the terminal.  May need an #ifndef GIZMO.
	 */
#ifdef	DEBUG
	if (!debug) {
#endif /* DEBUG */
#undef BSD19906
#if defined(BSD)&&!defined(sun)&&!defined(SYS_SINIXM)
#if (BSD >= 199006 && !defined(i386))
#define  BSD19906
#endif /* BSD... */
#endif /* BSD sun */
#if defined(BSD19906)
		daemon(0, 0);
#else /* BSD19906 */
		if (fork())
			exit(0);

		{
                        unsigned long s;
			int max_fd;
#if defined(NTP_POSIX_SOURCE) && !defined(SYS_386BSD)
    			max_fd = sysconf(_SC_OPEN_MAX);
#else /* NTP_POSIX_SOURCE */
			max_fd = getdtablesize();
#endif /* NTP_POSIX_SOURCE */
			for (s = 0; s < max_fd; s++)
				(void) close(s);
			(void) open("/", 0);
			(void) dup2(0, 1);
			(void) dup2(0, 2);
#ifdef NTP_POSIX_SOURCE
#if	defined(SOLARIS) || defined(SYS_PTX) || defined(SYS_AUX3) || defined(SYS_AIX)
			(void) setsid();
#else
			(void) setpgid(0, 0);
#endif
#else /* NTP_POSIX_SOURCE */
#ifdef HAVE_ATT_SETPGRP
			(void) setpgrp();
#else /* HAVE_ATT_SETPGRP */
			(void) setpgrp(0, getpid());
#endif /* HAVE_ATT_SETPGRP */
#if defined(SYS_HPUX)
			if (fork())
				exit(0);
#else /* SYS_HPUX */
#ifdef SYS_DOMAINOS
/*
 * This breaks... the program fails to listen to any packets coming
 * in on the UDP socket.  So how do you break terminal affiliation?
 */
#else /* SYS_DOMAINOS */
			{
				int fid;

				fid = open("/dev/tty", 2);
				if (fid >= 0) {
					(void) ioctl(fid, (U_LONG) TIOCNOTTY,
						(char *) 0);
					(void) close(fid);
				}
				(void) setpgrp(0, getpid());
			}
#endif /* SYS_DOMAINOS */
#endif /* SYS_HPUX */
#endif /* NTP_POSIX_SOURCE */
		}
#endif /* BSD19906 */
#ifdef	DEBUG
	}
#endif /* DEBUG */
#endif /* NODETACH */

	/*
	 * Logging.  This may actually work on the gizmo board.  Find a name
	 * to log with by using the basename of argv[0]
	 */
	cp = strrchr(argv[0], '/');
	if (cp == 0)
		cp = argv[0];
	else
		cp++;

#ifndef	LOG_DAEMON
	openlog(cp, LOG_PID);
#else

#ifndef	LOG_NTP
#define	LOG_NTP	LOG_DAEMON
#endif
	openlog(cp, LOG_PID | LOG_NDELAY, LOG_NTP);
#ifdef	DEBUG
	if (debug)
		setlogmask(LOG_UPTO(LOG_DEBUG));
	else
#endif	/* DEBUG */
		setlogmask(LOG_UPTO(LOG_DEBUG)); /* @@@ was INFO */
#endif	/* LOG_DAEMON */

	syslog(LOG_NOTICE, Version);


#if defined(SYS_HPUX)
	/*
	 * Lock text into ram, set real time priority
	 */
	if (plock(TXTLOCK) < 0)
	    syslog(LOG_ERR, "plock() error: %m");
	if (rtprio(0, 120) < 0)
	    syslog(LOG_ERR, "rtprio() error: %m");
#else
#if defined(PROCLOCK) && defined(LOCK_PROCESS)
	/*
	 * lock the process into memory
	 */
	if (plock(PROCLOCK) < 0)
	    syslog(LOG_ERR, "plock(): %m");
#endif
#if defined(NTPD_PRIO) && NTPD_PRIO != 0
	/*
	 * Set the priority.
	 */
#ifdef	HAVE_ATT_NICE
	nice (NTPD_PRIO);
#endif /* HAVE_ATT_NICE */
#ifdef  HAVE_BSD_NICE
	(void) setpriority(PRIO_PROCESS, 0, NTPD_PRIO);
#endif /* HAVE_BSD_NICE */

#endif /* !PROCLOCK || !LOCK_PROCESS */
#endif /* SYS_HPUX */

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

#ifdef DEBUG
	(void) signal_no_reset(MOREDEBUGSIG, moredebug);
	(void) signal_no_reset(LESSDEBUGSIG, lessdebug);
#else
	(void) signal_no_reset(MOREDEBUGSIG, SIG_IGN);
	(void) signal_no_reset(LESSDEBUGSIG, SIG_IGN);
#endif 	/* DEBUG */

	/*
	 * Call the init_ routines to initialize the data structures.
	 * Note that init_systime() may run a protocol to get a crude
	 * estimate of the time as an NTP client when running on the
	 * gizmo board.  It is important that this be run before
	 * init_subs() since the latter uses the time of day to seed
	 * the random number generator.  That is not the only
	 * dependency between these, either, be real careful about
	 * reordering.
	 */
	init_auth();
	init_util();
	init_restrict();
	init_mon();
	init_systime();
	init_timer();
	init_lib();
	init_random();
	init_request();
	init_control();
	init_leap();
	init_peer();
#ifdef REFCLOCK
	init_refclock();
#endif
	init_proto();
	init_io();
	init_loopfilter();

	/*
	 * Get configuration.  This (including argument list parsing) is
	 * done in a separate module since this will definitely be different
	 * for the gizmo board.
	 */
	getconfig(argc, argv);
	initializing = 0;

	/*
	 * Report that we're up to any trappers
	 */
	report_event(EVNT_SYSRESTART, (struct peer *)0);

	/*
	 * Use select() on all on all input fd's for unlimited
	 * time.  select() will terminate on SIGALARM or on the
	 * reception of input.  Using select() means we can't do
	 * robust signal handling and we get a potential race
	 * between checking for alarms and doing the select().
	 * Mostly harmless, I think.
	 */
	was_alarmed = 0;
	rbuflist = (struct recvbuf *)0;
	for (;;) {
#ifndef HAVE_SIGNALED_IO
		extern fd_set activefds;
		extern int maxactivefd;

		fd_set rdfdes;
		int nfound;
#else
		block_io_and_alarm();
#endif


		rbuflist = getrecvbufs();	/* get received buffers */
		if (alarm_flag) {		/* alarmed? */
			was_alarmed = 1;
			alarm_flag = 0;
		}

		if (!was_alarmed && rbuflist == (struct recvbuf *)0) {
			/*
			 * Nothing to do.  Wait for something.
			 */
#ifndef HAVE_SIGNALED_IO
			rdfdes = activefds;
			nfound = select(maxactivefd+1, &rdfdes, (fd_set *)0,
					(fd_set *)0, (struct timeval *)0);
			if (nfound > 0) {
				l_fp ts;

        			get_systime(&ts);
        			(void)input_handler(&ts);
			}
			else if (nfound == -1 && errno != EINTR) {
				syslog(LOG_ERR, "select() error: %m");
			}
#else
			wait_for_signal();
#endif
			if (alarm_flag) {		/* alarmed? */
				was_alarmed = 1;
				alarm_flag = 0;
			}
			rbuflist = getrecvbufs();  /* get received buffers */
		}
#ifdef HAVE_SIGNALED_IO
		unblock_io_and_alarm();
#endif

		/*
		 * Out here, signals are unblocked.  Call timer routine
		 * to process expiry.
		 */
		if (was_alarmed) {
			timer();
			was_alarmed = 0;
		}

		/*
		 * Call the data procedure to handle each received
		 * packet.
		 */
		while (rbuflist != (struct recvbuf *)0) {
			rbuf = rbuflist;
			rbuflist = rbuf->next;
			(rbuf->receiver)(rbuf);
			freerecvbuf(rbuf);
		}
		/*
		 * Go around again
		 */
	}
}


#ifdef SIGDIE1
/*
 * finish - exit gracefully
 */
static RETSIGTYPE
finish(sig)
int sig;
{

	/*
	 * Log any useful info before exiting.
	 */
#ifdef notdef
	log_exit_stats();
#endif
	exit(0);
}
#endif	/* SIGDIE1 */


#ifdef DEBUG
/*
 * moredebug - increase debugging verbosity
 */
static RETSIGTYPE
moredebug(sig)
int sig;
{
	if (debug < 255) {
		debug++;
		syslog(LOG_DEBUG, "debug raised to %d", debug);
	}
}

/*
 * lessdebug - decrease debugging verbosity
 */
static RETSIGTYPE
lessdebug(sig)
int sig;
{
	if (debug > 0) {
		debug--;
		syslog(LOG_DEBUG, "debug lowered to %d", debug);
	}
}
#endif	/* DEBUG */
