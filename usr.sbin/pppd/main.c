/*
 * main.c - Point-to-Point Protocol main module
 *
 * Copyright (c) 1989 Carnegie Mellon University.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Carnegie Mellon University.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef lint
static char rcsid[] = "$Id: main.c,v 1.4.4.1 1996/03/01 19:34:53 phk Exp $";
#endif

#define SETSID

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <syslog.h>
#include <netdb.h>
#include <utmp.h>
#include <pwd.h>

/*
 * If REQ_SYSOPTIONS is defined to 1, pppd will not run unless
 * /etc/ppp/options exists.
 */
#ifndef	REQ_SYSOPTIONS
#define REQ_SYSOPTIONS	1
#endif

#ifdef SGTTY
#include <sgtty.h>
#else
#ifndef sun
#include <sys/ioctl.h>
#endif
#include <termios.h>
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <net/if.h>

#include "callout.h"
#include "ppp.h"
#include "magic.h"
#include "fsm.h"
#include "lcp.h"
#include "ipcp.h"
#include "upap.h"
#include "chap.h"

#include "pppd.h"
#include "pathnames.h"
#include "patchlevel.h"


#ifndef TRUE
#define TRUE (1)
#endif /*TRUE*/

#ifndef FALSE
#define FALSE (0)
#endif /*FALSE*/

#ifdef PIDPATH
static char *pidpath = PIDPATH;	/* filename in which pid will be stored */
#else
static char *pidpath = _PATH_PIDFILE;
#endif /* PIDFILE */

/* interface vars */
char ifname[IFNAMSIZ];		/* Interface name */
int ifunit;			/* Interface unit number */

char *progname;			/* Name of this program */
char *username;
char hostname[MAXNAMELEN];	/* Our hostname */
char our_name[MAXNAMELEN];
char remote_name[MAXNAMELEN];
static char pidfilename[MAXPATHLEN];
char peername[MAXNAMELEN];
time_t t1,t2;

static pid_t	pid;		/* Our pid */
static pid_t	pgrpid;		/* Process Group ID */
uid_t uid;			/* Our real user-id */
gid_t gid;

char devnam[MAXPATHLEN] = "/dev/tty";	/* Device name */
int default_device = TRUE;	/* use default device (stdin/out) */

int fd = -1;			/* Device file descriptor */
int s;				/* Socket file descriptor */

int phase;			/* where the link is at */

#ifdef SGTTY
static struct sgttyb initsgttyb;	/* Initial TTY sgttyb */
#else
static struct termios inittermios;	/* Initial TTY termios */
#endif

static int initfdflags = -1;	/* Initial file descriptor flags */

static int restore_term;	/* 1 => we've munged the terminal */

u_char outpacket_buf[MTU+PPP_HDRLEN]; /* buffer for outgoing packet */
static u_char inpacket_buf[MTU+PPP_HDRLEN]; /* buffer for incoming packet */

int hungup;			/* terminal has been hung up */
static int n_children;		/* # child processes still running */

/* configured variables */

int debug = 0;		        /* Debug flag */
int kdebugflag = 0;		/* Kernel debugging flag */
char user[MAXNAMELEN];		/* username for PAP */
char passwd[MAXSECRETLEN];	/* password for PAP */
char *connector = NULL;		/* "connect" command */
char *disconnector = NULL;	/* "disconnect" command */
int inspeed = 0;		/* Input/Output speed requested */
int baud_rate;			/* bits/sec currently used */
u_long netmask = 0;		/* netmask to use on ppp interface */
u_long dns1 = 0;		/* primary DNS */
u_long dns2 = 0;		/* secondary DNS */
int crtscts = 0;		/* use h/w flow control */
int nodetach = 0;		/* don't fork */
int modem = 0;			/* use modem control lines */
int auth_required = 0;		/* require peer to authenticate */
int defaultroute = 0;		/* assign default route through interface */
int proxyarp = 0;		/* set entry in arp table */
int persist = 0;		/* re-initiate on termination */
int answer = 0;			/* wait for incoming call */
int uselogin = 0;		/* check PAP info against /etc/passwd */
int lockflag = 0;		/* lock the serial device */


/* prototypes */
static void hup __P((int));
static void term __P((int));
static void alrm __P((int));
static void io __P((int));
static void chld __P((int));
static void incdebug __P((int));
static void nodebug __P((int));
void establish_ppp __P((void));

void reap_kids __P((void));
void cleanup __P((int, caddr_t));
void die __P((int));
void novm __P((char *));

void log_packet __P((u_char *, int, char *));
void format_packet __P((u_char *, int,
			   void (*) (void *, char *, ...), void *));
void pr_log __P((void *, char *, ...));

/*
 * PPP Data Link Layer "protocol" table.
 * One entry per supported protocol.
 */
static struct protent {
    u_short protocol;
    void (*init)();
    void (*input)();
    void (*protrej)();
    int  (*printpkt)();
    char *name;
} prottbl[] = {
    { LCP, lcp_init, lcp_input, lcp_protrej, lcp_printpkt, "LCP" },
    { PPP_IPCP, ipcp_init, ipcp_input, ipcp_protrej, ipcp_printpkt, "PPP_IPCP" },
    { PPP_PAP, upap_init, upap_input, upap_protrej, upap_printpkt, "PAP" },
    { PPP_CHAP, ChapInit, ChapInput, ChapProtocolReject, ChapPrintPkt, "CHAP" },
};

#define N_PROTO		(sizeof(prottbl) / sizeof(prottbl[0]))

main(argc, argv)
    int argc;
    char *argv[];
{
    int mask, i;
    struct sigaction sa;
    struct cmd *cmdp;
    FILE *pidfile;
    char *p;
    struct passwd *pw;

    time(&t1);
    strcpy(peername,"[startup]");
    p = ttyname(0);
    if (p)
	strcpy(devnam, p);

    if (gethostname(hostname, MAXNAMELEN) < 0 ) {
	perror("couldn't get hostname");
	die(1);
    }
    hostname[MAXNAMELEN-1] = 0;

    pid = getpid();
    uid = getuid();
    gid = getgid();

    if (!ppp_available()) {
	fprintf(stderr, "Sorry - PPP is not available on this system\n");
	exit(1);
    }

    /*
     * Initialize to the standard option set, then parse, in order,
     * the system options file, the user's options file, and the command
     * line arguments.
     */
    for (i = 0; i < N_PROTO; i++)
	(*prottbl[i].init)(0);

    progname = *argv;

    if (!options_from_file(_PATH_SYSOPTIONS, REQ_SYSOPTIONS, 0) ||
	!options_from_user() ||
	!parse_args(argc-1, argv+1) ||
	!options_for_tty())
	die(1);
    check_auth_options();
    setipdefault();

    /*
     * Initialize syslog system and magic number package.
     */
#if !defined(ultrix)
    openlog("pppd", LOG_PID | LOG_NDELAY, LOG_PPP);
    setlogmask(LOG_UPTO(LOG_INFO));
#else
    openlog("pppd", LOG_PID);
#define LOG_UPTO(x) (x)
#define setlogmask(x) (x)
#endif
    if (debug)
	setlogmask(LOG_UPTO(LOG_DEBUG));

    magic_init();

    username = getlogin();
    if (username == NULL) {
	pw = getpwuid(uid);
	if (pw != NULL && pw->pw_name != NULL)
	    username = pw->pw_name;
	else
	    username = "(unknown)";
    }
    syslog(LOG_NOTICE, "pppd %s.%d started by %s, uid %d",
	   VERSION, PATCHLEVEL, username, uid);

#ifdef SETSID
    /*
     * Make sure we can set the serial device to be our controlling terminal.
     */
    if (default_device) {
	/*
	 * No device name was specified:
	 * we are in the device's session already.
	 */
	if ((pgrpid = getpgrp()) < 0) {
	    syslog(LOG_ERR, "getpgrp(): %m");
	    die(1);
	}

    } else {
	/*
	 * Not default device: make sure we're not a process group leader,
	 * then become session leader of a new session (so we can make
	 * our device its controlling terminal and thus get SIGHUPs).
	 */
	if (!nodetach) {
	    /* fork so we're not a process group leader */
	    if (pid = fork()) {
		exit(0);	/* parent is finished */
	    }
	    if (pid < 0) {
		syslog(LOG_ERR, "fork: %m");
		die(1);
	    }
	    pid = getpid();	/* otherwise pid is 0 in child */
	} else {
	    /*
	     * try to put ourself into our parent's process group,
	     * so we're not a process group leader
	     */
	    if (setpgrp(pid, getppid()) < 0)
		syslog(LOG_WARNING, "setpgrp: %m");
	}

	/* create new session */
	if ((pgrpid = setsid()) < 0) {
	    syslog(LOG_ERR, "setsid(): %m");
	    die(1);
	}
    }
#endif

    if (lockflag && !default_device)
	if (lock(devnam) < 0)
	    die(1);

    /* Get an internet socket for doing socket ioctl's on. */
    if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
	syslog(LOG_ERR, "socket : %m");
	die(1);
    }

    /*
     * Compute mask of all interesting signals and install signal handlers
     * for each.  Only one signal handler may be active at a time.  Therefore,
     * all other signals should be masked when any handler is executing.
     */
    sigemptyset(&mask);
    sigaddset(&mask, SIGHUP);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGALRM);
    sigaddset(&mask, SIGIO);
    sigaddset(&mask, SIGCHLD);
#ifdef	STREAMS
    sigaddset(&mask, SIGPOLL);
#endif

#define SIGNAL(s, handler)	{ \
	sa.sa_handler = handler; \
	if (sigaction(s, &sa, NULL) < 0) { \
	    syslog(LOG_ERR, "Couldn't establish signal handler (%d): %m", s); \
	    die(1); \
	} \
    }

    sa.sa_mask = mask;
    sa.sa_flags = 0;
    SIGNAL(SIGHUP, hup);		/* Hangup */
    SIGNAL(SIGINT, term);		/* Interrupt */
    SIGNAL(SIGTERM, term);		/* Terminate */
    SIGNAL(SIGALRM, alrm);		/* Timeout */
    SIGNAL(SIGIO, io);			/* Input available */
    SIGNAL(SIGCHLD, chld);		/* Death of child process */
#ifdef	STREAMS
    SIGNAL(SIGPOLL, io);		/* Input available */
#endif

    signal(SIGUSR1, incdebug);		/* Increment debug flag */
    signal(SIGUSR2, nodebug);		/* Reset debug flag */

    /*
     * Block SIGIOs and SIGPOLLs for now
     */
    sigemptyset(&mask);
    sigaddset(&mask, SIGIO);
#ifdef	STREAMS
    sigaddset(&mask, SIGPOLL);
#endif
    sigprocmask(SIG_BLOCK, &mask, NULL);

    /*
     * Open the serial device and set it up to be the ppp interface.
     */
    if ((fd = open(devnam, O_RDWR /*| O_NDELAY*/)) < 0) {
	syslog(LOG_ERR, "open(%s): %m", devnam);
	die(1);
    }
    hungup = 0;

#ifdef TIOCSCTTY
    /* set device to be controlling tty */
    if (!default_device && ioctl(fd, TIOCSCTTY) < 0) {
	syslog(LOG_ERR, "ioctl(TIOCSCTTY): %m");
	die(1);
    }
#endif /* TIOCSCTTY */

    /* run connection script */
    if (connector) {
	MAINDEBUG((LOG_INFO, "Connecting with <%s>", connector));

	/* set line speed, flow control, etc.; set CLOCAL for now */
	set_up_tty(fd, 1);

	/* drop dtr to hang up in case modem is off hook */
	if (!default_device && modem) {
	    setdtr(fd, FALSE);
	    sleep(1);
	    setdtr(fd, TRUE);
	}

	if (device_script(connector, fd, fd) < 0) {
	    syslog(LOG_ERR, "could not set up connection");
	    setdtr(fd, FALSE);
	    die(1);
	}

	syslog(LOG_INFO, "Connected...");
	sleep(1);		/* give it time to set up its terminal */
    }

    /* set line speed, flow control, etc.; clear CLOCAL if modem option */
    set_up_tty(fd, 0);

    /* set up the serial device as a ppp interface */
    establish_ppp();

    syslog(LOG_INFO, "Using interface ppp%d", ifunit);
    (void) sprintf(ifname, "ppp%d", ifunit);

    /* write pid to file */
    (void) sprintf(pidfilename, "%s/%s.pid", pidpath, ifname);
    if ((pidfile = fopen(pidfilename, "w")) != NULL) {
	fprintf(pidfile, "%d\n", pid);
	(void) fclose(pidfile);
    } else {
	syslog(LOG_ERR, "unable to create pid file: %m");
	pidfilename[0] = 0;
    }

    /*
     * Set process group of device to our process group so we can get
     * SIGIOs and SIGHUPs.
     */
#ifdef SETSID
    if (default_device) {
	int id = tcgetpgrp(fd);
	if (id != pgrpid) {
	    syslog(LOG_WARNING, "warning: not in tty's process group");
	}
    } else {
	if (tcsetpgrp(fd, pgrpid) < 0) {
	    syslog(LOG_ERR, "tcsetpgrp(): %m");
	    die(1);
	}
    }
#else
    /* set process group on tty so we get SIGIO's */
    if (ioctl(fd, TIOCSPGRP, &pgrpid) < 0) {
	syslog(LOG_ERR, "ioctl(TIOCSPGRP): %m");
	die(1);
    }
#endif

    /*
     * Record initial device flags, then set device to cause SIGIO
     * signals to be generated.
     */
    if ((initfdflags = fcntl(fd, F_GETFL)) == -1) {
	syslog(LOG_ERR, "fcntl(F_GETFL): %m");
	die(1);
    }

#ifdef _linux_ /* This is a kludge for Linux. FIXME !!! -- later. */
#undef	FASYNC
#define FASYNC	0
#endif

    if (fcntl(fd, F_SETFL, FNDELAY | FASYNC) == -1) {
	syslog(LOG_ERR, "fcntl(F_SETFL, FNDELAY | FASYNC): %m");
	die(1);
    }

    /*
     * Block all signals, start opening the connection, and  wait for
     * incoming signals (reply, timeout, etc.).
     */
    syslog(LOG_NOTICE, "Connect: %s <--> %s", ifname, devnam);
    sigprocmask(SIG_BLOCK, &mask, NULL); /* Block signals now */
    lcp_lowerup(0);		/* XXX Well, sort of... */
    lcp_open(0);		/* Start protocol */
    for (phase = PHASE_ESTABLISH; phase != PHASE_DEAD; )
	sigpause(0);		/* Wait for next signal */

    /*
     * Run disconnector script, if requested
     */
    if (disconnector) {
	if (device_script(disconnector, fd, fd) < 0) {
	    syslog(LOG_WARNING, "disconnect script failed");
	    die(1);
	}

	syslog(LOG_INFO, "Disconnected...");
    }

    quit();
}

#if B9600 == 9600
/*
 * XXX assume speed_t values numerically equal bits per second
 * (so we can ask for any speed).
 */
#define translate_speed(bps)	(bps)
#define baud_rate_of(speed)	(speed)

#else
/*
 * List of valid speeds.
 */
struct speed {
    int speed_int, speed_val;
} speeds[] = {
#ifdef B50
    { 50, B50 },
#endif
#ifdef B75
    { 75, B75 },
#endif
#ifdef B110
    { 110, B110 },
#endif
#ifdef B134
    { 134, B134 },
#endif
#ifdef B150
    { 150, B150 },
#endif
#ifdef B200
    { 200, B200 },
#endif
#ifdef B300
    { 300, B300 },
#endif
#ifdef B600
    { 600, B600 },
#endif
#ifdef B1200
    { 1200, B1200 },
#endif
#ifdef B1800
    { 1800, B1800 },
#endif
#ifdef B2000
    { 2000, B2000 },
#endif
#ifdef B2400
    { 2400, B2400 },
#endif
#ifdef B3600
    { 3600, B3600 },
#endif
#ifdef B4800
    { 4800, B4800 },
#endif
#ifdef B7200
    { 7200, B7200 },
#endif
#ifdef B9600
    { 9600, B9600 },
#endif
#ifdef B19200
    { 19200, B19200 },
#endif
#ifdef B38400
    { 38400, B38400 },
#endif
#ifdef EXTA
    { 19200, EXTA },
#endif
#ifdef EXTB
    { 38400, EXTB },
#endif
#ifdef B57600
    { 57600, B57600 },
#endif
#ifdef B115200
    { 115200, B115200 },
#endif
    { 0, 0 }
};

/*
 * Translate from bits/second to a speed_t.
 */
int
translate_speed(bps)
    int bps;
{
    struct speed *speedp;

    if (bps == 0)
	return 0;
    for (speedp = speeds; speedp->speed_int; speedp++)
	if (bps == speedp->speed_int)
	    return speedp->speed_val;
    syslog(LOG_WARNING, "speed %d not supported", bps);
    return 0;
}

/*
 * Translate from a speed_t to bits/second.
 */
int
baud_rate_of(speed)
    int speed;
{
    struct speed *speedp;

    if (speed == 0)
	return 0;
    for (speedp = speeds; speedp->speed_int; speedp++)
	if (speed == speedp->speed_val)
	    return speedp->speed_int;
    return 0;
}
#endif

/*
 * set_up_tty: Set up the serial port on `fd' for 8 bits, no parity,
 * at the requested speed, etc.  If `local' is true, set CLOCAL
 * regardless of whether the modem option was specified.
 */
set_up_tty(fd, local)
    int fd, local;
{
#ifndef SGTTY
    int speed, x;
    struct termios tios;

    if (tcgetattr(fd, &tios) < 0) {
	syslog(LOG_ERR, "tcgetattr: %m");
	die(1);
    }

    if (!restore_term)
	inittermios = tios;

#ifdef CRTSCTS
    tios.c_cflag &= ~(CSIZE | CSTOPB | PARENB | CLOCAL | CRTSCTS);
    if (crtscts == 1)
	tios.c_cflag |= CRTSCTS;
#else
    tios.c_cflag &= ~(CSIZE | CSTOPB | PARENB | CLOCAL);
#endif	/* CRTSCTS */

    tios.c_cflag |= CS8 | CREAD | HUPCL;
    if (local || !modem)
	tios.c_cflag |= CLOCAL;
    tios.c_iflag = IGNBRK | IGNPAR;
    tios.c_oflag = 0;
    tios.c_lflag = 0;
    tios.c_cc[VMIN] = 1;
    tios.c_cc[VTIME] = 0;

    if (crtscts == 2) {
	tios.c_iflag |= IXOFF;
	tios.c_cc[VSTOP] = 0x13;	/* DC3 = XOFF = ^S */
	tios.c_cc[VSTART] = 0x11;	/* DC1 = XON  = ^Q */
    }

    speed = translate_speed(inspeed);
    if (speed) {
	cfsetospeed(&tios, speed);
	cfsetispeed(&tios, speed);
    } else {
	speed = cfgetospeed(&tios);
    }

    if (tcsetattr(fd, TCSAFLUSH, &tios) < 0) {
	syslog(LOG_ERR, "tcsetattr: %m");
	die(1);
    }

#ifdef ultrix
    x = 0;
    if (ioctl(fd, (crtscts || modem)? TIOCMODEM: TIOCNMODEM, &x) < 0)
	syslog(LOG_WARNING, "TIOC(N)MODEM: %m");
    if (ioctl(fd, (local || !modem)? TIOCNCAR: TIOCCAR) < 0)
	syslog(LOG_WARNING, "TIOC(N)CAR: %m");
#endif

#else	/* SGTTY */
    int speed;
    struct sgttyb sgttyb;

    /*
     * Put the tty in raw mode.
     */
    if (ioctl(fd, TIOCGETP, &sgttyb) < 0) {
	syslog(LOG_ERR, "ioctl(TIOCGETP): %m");
	die(1);
    }

    if (!restore_term)
	initsgttyb = sgttyb;

    sgttyb.sg_flags = RAW | ANYP;
    speed = translate_speed(inspeed);
    if (speed)
	sgttyb.sg_ispeed = speed;
    else
	speed = sgttyb.sg_ispeed;

    if (ioctl(fd, TIOCSETP, &sgttyb) < 0) {
	syslog(LOG_ERR, "ioctl(TIOCSETP): %m");
	die(1);
    }
#endif

    baud_rate = baud_rate_of(speed);
    restore_term = TRUE;
}

/*
 * setdtr - control the DTR line on the serial port.
 * This is called from die(), so it shouldn't call die().
 */
setdtr(fd, on)
int fd, on;
{
    int modembits = TIOCM_DTR;

    ioctl(fd, (on? TIOCMBIS: TIOCMBIC), &modembits);
}


/*
 * quit - Clean up state and exit.
 */
void
quit()
{
    die(0);
}

/*
 * die - like quit, except we can specify an exit status.
 */
void
die(status)
    int status;
{
    cleanup(0, NULL);
    time(&t2);
    syslog(LOG_INFO, "Exit User %s Peer %s %d seconds",
	username, peername, t2-t1);
    exit(status);
}

/*
 * cleanup - restore anything which needs to be restored before we exit
 */
/* ARGSUSED */
void
cleanup(status, arg)
    int status;
    caddr_t arg;
{
    if (fd >= 0) {
	/* drop dtr to hang up */
	if (modem)
	    setdtr(fd, FALSE);

	if (initfdflags != -1 && fcntl(fd, F_SETFL, initfdflags) < 0)
	    syslog(LOG_WARNING, "fcntl(F_SETFL, fdflags): %m");
	initfdflags = -1;

	disestablish_ppp();

	if (restore_term) {
#ifndef SGTTY
	    if (tcsetattr(fd, TCSAFLUSH, &inittermios) < 0)
		syslog(LOG_WARNING, "tcsetattr: %m");
#else
	    if (ioctl(fd, TIOCSETP, &initsgttyb) < 0)
		syslog(LOG_WARNING, "ioctl(TIOCSETP): %m");
#endif
	}

	close(fd);
	fd = -1;
    }

    if (pidfilename[0] != 0 && unlink(pidfilename) < 0)
	syslog(LOG_WARNING, "unable to unlink pid file: %m");
    pidfilename[0] = 0;

    if (lockflag && !default_device)
	unlock();
}


static struct callout *callout = NULL;		/* Callout list */
static struct timeval schedtime;		/* Time last timeout was set */

/*
 * timeout - Schedule a timeout.
 *
 * Note that this timeout takes the number of seconds, NOT hz (as in
 * the kernel).
 */
void
timeout(func, arg, time)
    void (*func)();
    caddr_t arg;
    int time;
{
    struct itimerval itv;
    struct callout *newp, **oldpp;

    MAINDEBUG((LOG_DEBUG, "Timeout %x:%x in %d seconds.",
	       (int) func, (int) arg, time));

    /*
     * Allocate timeout.
     */
    if ((newp = (struct callout *) malloc(sizeof(struct callout))) == NULL) {
	syslog(LOG_ERR, "Out of memory in timeout()!");
	die(1);
    }
    newp->c_arg = arg;
    newp->c_func = func;

    /*
     * Find correct place to link it in and decrement its time by the
     * amount of time used by preceding timeouts.
     */
    for (oldpp = &callout;
	 *oldpp && (*oldpp)->c_time <= time;
	 oldpp = &(*oldpp)->c_next)
	time -= (*oldpp)->c_time;
    newp->c_time = time;
    newp->c_next = *oldpp;
    if (*oldpp)
	(*oldpp)->c_time -= time;
    *oldpp = newp;

    /*
     * If this is now the first callout then we have to set a new
     * itimer.
     */
    if (callout == newp) {
	itv.it_interval.tv_sec = itv.it_interval.tv_usec =
	    itv.it_value.tv_usec = 0;
	itv.it_value.tv_sec = callout->c_time;
	MAINDEBUG((LOG_DEBUG, "Setting itimer for %d seconds in timeout.",
		   itv.it_value.tv_sec));
	if (setitimer(ITIMER_REAL, &itv, NULL)) {
	    syslog(LOG_ERR, "setitimer(ITIMER_REAL): %m");
	    die(1);
	}
	if (gettimeofday(&schedtime, NULL)) {
	    syslog(LOG_ERR, "gettimeofday: %m");
	    die(1);
	}
    }
}


/*
 * untimeout - Unschedule a timeout.
 */
void
untimeout(func, arg)
    void (*func)();
    caddr_t arg;
{
    struct itimerval itv;
    struct callout **copp, *freep;
    int reschedule = 0;

    MAINDEBUG((LOG_DEBUG, "Untimeout %x:%x.", (int) func, (int) arg));

    /*
     * If the first callout is unscheduled then we have to set a new
     * itimer.
     */
    if (callout &&
	callout->c_func == func &&
	callout->c_arg == arg)
	reschedule = 1;

    /*
     * Find first matching timeout.  Add its time to the next timeouts
     * time.
     */
    for (copp = &callout; *copp; copp = &(*copp)->c_next)
	if ((*copp)->c_func == func &&
	    (*copp)->c_arg == arg) {
	    freep = *copp;
	    *copp = freep->c_next;
	    if (*copp)
		(*copp)->c_time += freep->c_time;
	    (void) free((char *) freep);
	    break;
	}

    if (reschedule) {
	itv.it_interval.tv_sec = itv.it_interval.tv_usec =
	    itv.it_value.tv_usec = 0;
	itv.it_value.tv_sec = callout ? callout->c_time : 0;
	MAINDEBUG((LOG_DEBUG, "Setting itimer for %d seconds in untimeout.",
		   itv.it_value.tv_sec));
	if (setitimer(ITIMER_REAL, &itv, NULL)) {
	    syslog(LOG_ERR, "setitimer(ITIMER_REAL): %m");
	    die(1);
	}
	if (gettimeofday(&schedtime, NULL)) {
	    syslog(LOG_ERR, "gettimeofday: %m");
	    die(1);
	}
    }
}


/*
 * adjtimeout - Decrement the first timeout by the amount of time since
 * it was scheduled.
 */
void
adjtimeout()
{
    struct timeval tv;
    int timediff;

    if (callout == NULL)
	return;
    /*
     * Make sure that the clock hasn't been warped dramatically.
     * Account for recently expired, but blocked timer by adding
     * small fudge factor.
     */
    if (gettimeofday(&tv, NULL)) {
	syslog(LOG_ERR, "gettimeofday: %m");
	die(1);
    }
    timediff = tv.tv_sec - schedtime.tv_sec;
    if (timediff < 0 ||
	timediff > callout->c_time + 1)
	return;

    callout->c_time -= timediff;	/* OK, Adjust time */
}


/*
 * hup - Catch SIGHUP signal.
 *
 * Indicates that the physical layer has been disconnected.
 */
static void
hup(sig)
    int sig;
{
    syslog(LOG_INFO, "Hangup (SIGHUP)");

    hungup = 1;			/* they hung up on us! */
    persist = 0;		/* don't try to restart */
    adjtimeout();		/* Adjust timeouts */
    lcp_lowerdown(0);		/* Reset connection */
    quit();			/* and die */
}


/*
 * term - Catch SIGTERM signal.
 *
 * Indicates that we should initiate a graceful disconnect and exit.
 */
static void
term(sig)
    int sig;
{
    syslog(LOG_INFO, "Terminating link.");
    persist = 0;		/* don't try to restart */
    adjtimeout();		/* Adjust timeouts */
    lcp_close(0);		/* Close connection */
}


/*
 * alrm - Catch SIGALRM signal.
 *
 * Indicates a timeout.
 */
static void
alrm(sig)
    int sig;
{
    struct itimerval itv;
    struct callout *freep, *list, *last;

    MAINDEBUG((LOG_DEBUG, "Alarm"));

    if (callout == NULL)
	return;
    /*
     * Get the first scheduled timeout and any that were scheduled
     * for the same time as a list, and remove them all from callout
     * list.
     */
    list = last = callout;
    while (last->c_next != NULL && last->c_next->c_time == 0)
	last = last->c_next;
    callout = last->c_next;
    last->c_next = NULL;

    /*
     * Set a new itimer if there are more timeouts scheduled.
     */
    if (callout) {
	itv.it_interval.tv_sec = itv.it_interval.tv_usec = 0;
	itv.it_value.tv_usec = 0;
	itv.it_value.tv_sec = callout->c_time;
	MAINDEBUG((LOG_DEBUG, "Setting itimer for %d seconds in alrm.",
		   itv.it_value.tv_sec));
	if (setitimer(ITIMER_REAL, &itv, NULL)) {
	    syslog(LOG_ERR, "setitimer(ITIMER_REAL): %m");
	    die(1);
	}
	if (gettimeofday(&schedtime, NULL)) {
	    syslog(LOG_ERR, "gettimeofday: %m");
	    die(1);
	}
    }

    /*
     * Now call all the timeout routines scheduled for this time.
     */
    while (list) {
	(*list->c_func)(list->c_arg);
	freep = list;
	list = list->c_next;
	(void) free((char *) freep);
    }

}


/*
 * chld - Catch SIGCHLD signal.
 * Calls reap_kids to get status for any dead kids.
 */
static void
chld(sig)
    int sig;
{
    reap_kids();
}


/*
 * io - Catch SIGIO signal.
 *
 * Indicates that incoming data is available.
 */
static void
io(sig)
    int sig;
{
    int len, i;
    u_char *p;
    u_short protocol;
    fd_set fdset;
    struct timeval notime;
    int ready;

    MAINDEBUG((LOG_DEBUG, "IO signal received"));
    adjtimeout();		/* Adjust timeouts */

    /* Yup, this is for real */
    for (;;) {			/* Read all available packets */
	p = inpacket_buf;	/* point to beginning of packet buffer */

	len = read_packet(inpacket_buf);
	if (len < 0)
	    return;

	if (len == 0) {
	    MAINDEBUG((LOG_DEBUG, "End of file on fd!"));
	    hup(0);
	    return;
	}

	if (debug /*&& (debugflags & DBG_INPACKET)*/)
	    log_packet(p, len, "rcvd ");

	if (len < PPP_HDRLEN) {
	    MAINDEBUG((LOG_INFO, "io(): Received short packet."));
	    return;
	}

	p += 2;				/* Skip address and control */
	GETSHORT(protocol, p);
	len -= PPP_HDRLEN;

	/*
	 * Toss all non-LCP packets unless LCP is OPEN.
	 */
	if (protocol != LCP && lcp_fsm[0].state != OPENED) {
	    MAINDEBUG((LOG_INFO,
		       "io(): Received non-LCP packet when LCP not open."));
	    return;
	}

	/*
	 * Upcall the proper protocol input routine.
	 */
	for (i = 0; i < sizeof (prottbl) / sizeof (struct protent); i++)
	    if (prottbl[i].protocol == protocol) {
		(*prottbl[i].input)(0, p, len);
		break;
	    }

	if (i == sizeof (prottbl) / sizeof (struct protent)) {
	    syslog(LOG_WARNING, "input: Unknown protocol (%x) received!",
		   protocol);
	    lcp_sprotrej(0, p - PPP_HDRLEN, len + PPP_HDRLEN);
	}
    }
}

/*
 * demuxprotrej - Demultiplex a Protocol-Reject.
 */
void
demuxprotrej(unit, protocol)
    int unit;
    u_short protocol;
{
    int i;

    /*
     * Upcall the proper Protocol-Reject routine.
     */
    for (i = 0; i < sizeof (prottbl) / sizeof (struct protent); i++)
	if (prottbl[i].protocol == protocol) {
	    (*prottbl[i].protrej)(unit);
	    return;
	}

    syslog(LOG_WARNING,
	   "demuxprotrej: Unrecognized Protocol-Reject for protocol %d!",
	   protocol);
}


/*
 * incdebug - Catch SIGUSR1 signal.
 *
 * Increment debug flag.
 */
/*ARGSUSED*/
static void
incdebug(sig)
    int sig;
{
    syslog(LOG_INFO, "Debug turned ON, Level %d", debug);
    setlogmask(LOG_UPTO(LOG_DEBUG));
    debug++;
}


/*
 * nodebug - Catch SIGUSR2 signal.
 *
 * Turn off debugging.
 */
/*ARGSUSED*/
static void
nodebug(sig)
    int sig;
{
    setlogmask(LOG_UPTO(LOG_WARNING));
    debug = 0;
}


/*
 * device_script - run a program to connect or disconnect the
 * serial device.
 */
int
device_script(program, in, out)
    char *program;
    int in, out;
{
    int pid;
    int status;
    sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGHUP);
    sigprocmask(SIG_BLOCK, &mask, &mask);

    pid = fork();

    if (pid < 0) {
	syslog(LOG_ERR, "fork: %m");
	die(1);
    }

    if (pid == 0) {
	initgroups(username, gid);
	setgid(gid);
	setuid(uid);
	sigprocmask(SIG_SETMASK, &mask, NULL);
	dup2(in, 0);
	dup2(out, 1);
	execl("/bin/sh", "sh", "-c", program, (char *)0);
	syslog(LOG_ERR, "could not exec /bin/sh: %m");
	_exit(99);
	/* NOTREACHED */
    }

    while (waitpid(pid, &status, 0) < 0) {
	if (errno == EINTR)
	    continue;
	syslog(LOG_ERR, "waiting for (dis)connection process: %m");
	die(1);
    }
    sigprocmask(SIG_SETMASK, &mask, NULL);

    return (status == 0 ? 0 : -1);
}


/*
 * run-program - execute a program with given arguments,
 * but don't wait for it.
 * If the program can't be executed, logs an error unless
 * must_exist is 0 and the program file doesn't exist.
 */
int
run_program(prog, args, must_exist)
    char *prog;
    char **args;
    int must_exist;
{
    int pid;

    pid = fork();
    if (pid == -1) {
	syslog(LOG_ERR, "can't fork to run %s: %m", prog);
	return -1;
    }
    if (pid == 0) {
	execv(prog, args);
	if (must_exist || errno != ENOENT)
	    syslog(LOG_WARNING, "can't execute %s: %m", prog);
	_exit(-1);
    }
    MAINDEBUG((LOG_DEBUG, "Script %s started; pid = %d", prog, pid));
    ++n_children;
    return 0;
}


/*
 * reap_kids - get status from any dead child processes,
 * and log a message for abnormal terminations.
 */
void
reap_kids()
{
    int pid, status;

    if (n_children == 0)
	return;
    if ((pid = waitpid(-1, &status, WNOHANG)) == -1) {
	if (errno != ECHILD)
	    syslog(LOG_ERR, "waitpid: %m");
	return;
    }
    if (pid > 0) {
	--n_children;
	if (WIFSIGNALED(status)) {
	    syslog(LOG_WARNING, "child process %d terminated with signal %d",
		   pid, WTERMSIG(status));
	}
    }
}


/*
 * log_packet - format a packet and log it.
 */

char line[256];			/* line to be logged accumulated here */
char *linep;

void
log_packet(p, len, prefix)
    u_char *p;
    int len;
    char *prefix;
{
    strcpy(line, prefix);
    linep = line + strlen(line);
    format_packet(p, len, pr_log, NULL);
    if (linep != line)
	syslog(LOG_DEBUG, "%s", line);
}

/*
 * format_packet - make a readable representation of a packet,
 * calling `printer(arg, format, ...)' to output it.
 */
void
format_packet(p, len, printer, arg)
    u_char *p;
    int len;
    void (*printer) __P((void *, char *, ...));
    void *arg;
{
    int i, n;
    u_short proto;
    u_char x;

    if (len >= PPP_HDRLEN && p[0] == ALLSTATIONS && p[1] == UI) {
	p += 2;
	GETSHORT(proto, p);
	len -= PPP_HDRLEN;
	for (i = 0; i < N_PROTO; ++i)
	    if (proto == prottbl[i].protocol)
		break;
	if (i < N_PROTO) {
	    printer(arg, "[%s", prottbl[i].name);
	    n = (*prottbl[i].printpkt)(p, len, printer, arg);
	    printer(arg, "]");
	    p += n;
	    len -= n;
	} else {
	    printer(arg, "[proto=0x%x]", proto);
	}
    }

    for (; len > 0; --len) {
	GETCHAR(x, p);
	printer(arg, " %.2x", x);
    }
}

#ifdef __STDC__
#include <stdarg.h>

void
pr_log(void *arg, char *fmt, ...)
{
    int n;
    va_list pvar;
    char buf[256];

    va_start(pvar, fmt);
    vsprintf(buf, fmt, pvar);
    va_end(pvar);

    n = strlen(buf);
    if (linep + n + 1 > line + sizeof(line)) {
	syslog(LOG_DEBUG, "%s", line);
	linep = line;
    }
    strcpy(linep, buf);
    linep += n;
}

#else /* __STDC__ */
#include <varargs.h>

void
pr_log(arg, fmt, va_alist)
void *arg;
char *fmt;
va_dcl
{
    int n;
    va_list pvar;
    char buf[256];

    va_start(pvar);
    vsprintf(buf, fmt, pvar);
    va_end(pvar);

    n = strlen(buf);
    if (linep + n + 1 > line + sizeof(line)) {
	syslog(LOG_DEBUG, "%s", line);
	linep = line;
    }
    strcpy(linep, buf);
    linep += n;
}
#endif

/*
 * print_string - print a readable representation of a string using
 * printer.
 */
void
print_string(p, len, printer, arg)
    char *p;
    int len;
    void (*printer) __P((void *, char *, ...));
    void *arg;
{
    int c;

    printer(arg, "\"");
    for (; len > 0; --len) {
	c = *p++;
	if (' ' <= c && c <= '~')
	    printer(arg, "%c", c);
	else
	    printer(arg, "\\%.3o", c);
    }
    printer(arg, "\"");
}

/*
 * novm - log an error message saying we ran out of memory, and die.
 */
void
novm(msg)
    char *msg;
{
    syslog(LOG_ERR, "Virtual memory exhausted allocating %s\n", msg);
    die(1);
}
