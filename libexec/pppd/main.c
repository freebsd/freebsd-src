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

/*
 * There are three scenarios:
 * 1.  pppd used as daemon started from /etc/rc or perhaps /etc/ttys.
 *	a.  server
 *	b.  authentication necessary
 *	c.  want to use constant local ip addr
 *	d.  want to use constant remote ip addr, constant ip addr based on
 *	    authenticated user, or request ip addr
 * 2.  pppd used on /dev/tty after remote login.
 *	a.  server
 *	b.  no authentication necessary or allowed
 *	c.  want to use constant local ip addr
 *	d.  want to use constant remote ip addr, constant ip addr based on
 *	    authenticated user, or request ip addr
 * 3.  pppd used on line after tip'ing out.
 *	a.  client
 *	b.  remote end may request authentication
 *	c.  want to use constant local ip addr or request ip addr
 *	d.  want to use constant remote ip addr based on tip'd host, or
 *	    request remote ip addr
 */

#ifdef __386BSD__
#include <stdlib.h>
#endif

#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <syslog.h>
#include <netdb.h>
#include <utmp.h>

#ifdef sparc
#include <alloca.h>
#endif

#ifdef STREAMS
#include <sys/stream.h>
#include <sys/stropts.h>
#include <sys/termios.h>
#else
#ifdef SGTTY
#include <sgtty.h>
#else
#include <sys/ioctl.h>
#include <termios.h>
#endif
#endif

#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include "callout.h"

#include <net/if.h>
#include <net/if_ppp.h>

#ifdef STREAMS
#include "ppp_str.h"
#endif

#define	DEVNAME_SIZE	128	/* Buffer size for /dev filenames */

#include <string.h>

#ifndef BSD
#define BSD 43
#endif /*BSD*/

#include <net/ppp.h>
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
static char *pidpath = PIDPATH;	/* filename in which pid will be */
				/* stored */
#else
static char *pidpath = _PATH_PIDFILE;
#endif /* PIDFILE */

static char uinfopath[DEVNAME_SIZE];

/* interface vars */

char ifname[IFNAMSIZ];		/* Interface name */
int ifunit;					/* Interface unit number */

char *progname;			/* Name of this program */
char hostname[MAX_HOSTNAME_LEN]; /* hostname */
u_char hostname_len;			/* hostname length */

static pid_t	pid;			/* Our pid */
static pid_t	pgrpid;			/* Process Group ID */
static char pidfilename[DEVNAME_SIZE];

static char devname[DEVNAME_SIZE] = "/dev/tty";	/* Device name */
static int default_device = TRUE; /* use default device (stdin/out) */
int fd;							/* Device file descriptor */
int s;							/* Socket file descriptor */
static int initdisc;			/* Initial TTY discipline */
#ifndef STREAMS
#ifdef SGTTY
static struct sgttyb initsgttyb;	/* Initial TTY sgttyb */
#else
static struct termios inittermios;	/* Initial TTY TIOCGETA */
#endif
#endif

static int initfdflags;		/* Initial file descriptor flags */

u_char outpacket_buf[MTU+DLLHEADERLEN]; /* buffer for outgoing packet */
static u_char inpacket_buf[MTU+DLLHEADERLEN]; /* buffer for incoming packet */

/* configured variables */

int debug = 0;		        /* Debug flag */
static char user[80];		/* User name */
static char passwd[80];		/* password */
static char *connector = NULL;	/* "connect" command */
static int inspeed = 0;		/* Input/Output speed */
static u_long netmask = 0;	/* netmask to use on ppp interface */
static int crtscts = 0;		/* use h/w flow control */
static int nodetach = 0;	/* don't fork */

/* prototypes */
static void hup __ARGS((int, int, struct sigcontext *, char *));
static void intr __ARGS((int, int, struct sigcontext *, char *));
static void term __ARGS((int, int, struct sigcontext *, char *));
static void alrm __ARGS((int, int, struct sigcontext *, char *));
static void io __ARGS((int, int, struct sigcontext *, char *));
static void incdebug __ARGS((int, int, struct sigcontext *, char *));
static void nodebug __ARGS((int, int, struct sigcontext *, char *));
static void getuserpasswd __ARGS((void));

static int setdebug __ARGS((int *, char ***));
static int setpassive __ARGS((int *, char ***));
static int noopt __ARGS((int *, char ***));
static int setnovj __ARGS((int *, char ***));
static int noupap __ARGS((int *, char ***));
static int requpap __ARGS((int *, char ***));
static int nochap __ARGS((int *, char ***));
static int reqchap __ARGS((int *, char ***));
static int setspeed __ARGS((int *, char ***));
static int noaccomp __ARGS((int *, char ***));
static int noasyncmap __ARGS((int *, char ***));
static int noipaddr __ARGS((int *, char ***));
static int nomagicnumber __ARGS((int *, char ***));
static int setasyncmap __ARGS((int *, char ***));
static int setvjmode __ARGS((int *, char ***));
static int setmru __ARGS((int *, char ***));
static int nomru __ARGS((int *, char ***));
static int nopcomp __ARGS((int *, char ***));
static int setconnector __ARGS((int *, char ***));
static int setdomain __ARGS((int *, char ***));
static int setnetmask __ARGS((int *, char ***));
static int setcrtscts __ARGS((int *, char ***));
static int setnodetach __ARGS((int *, char ***));
static void cleanup __ARGS((int, caddr_t));

#ifdef	STREAMS
static void str_restore __ARGS((void));
extern	char	*ttyname __ARGS((int));
#define	MAXMODULES	10		/* max number of module names that we can save */
static struct	modlist {
  char	modname[FMNAMESZ+1];
} str_modules[MAXMODULES];
static int	str_module_count = 0;
#endif

/*
 * Valid arguments.
 */
static struct cmd {
  char *cmd_name;
  int (*cmd_func)();
} cmds[] = {
  "-all", noopt,		/* Don't request/allow any options */
  "-ac", noaccomp,		/* Disable Address/Control compress */
  "-am", noasyncmap,		/* Disable asyncmap negotiation */
  "-as", setasyncmap,		/* set the desired async map */
  "-d", setdebug,		/* Increase debugging level */
  "-detach", setnodetach,	/* don't fork */
  "-ip", noipaddr,		/* Disable IP address negotiation */
  "-mn", nomagicnumber,		/* Disable magic number negotiation */
  "-mru", nomru,		/* Disable mru negotiation */
  "-p", setpassive,		/* Set passive mode */
  "-pc", nopcomp,		/* Disable protocol field compress */
  "+ua", requpap,		/* Require UPAP authentication */
  "-ua", noupap,		/* Don't allow UPAP authentication */
  "+chap", reqchap,		/* Require CHAP authentication */
  "-chap", nochap,		/* Don't allow CHAP authentication */
  "-vj", setnovj,		/* disable VJ compression */
  "asyncmap", setasyncmap,	/* set the desired async map */
  "connect", setconnector,      /* A program to set up a connection */
  "crtscts", setcrtscts,	/* set h/w flow control */
  "debug", setdebug,		/* Increase debugging level */
  "domain", setdomain,		/* Add given domain name to hostname*/
  "mru", setmru,		/* Set MRU value for negotiation */
  "netmask", setnetmask,	/* set netmask */
  "passive", setpassive,	/* Set passive mode */
  "vjmode", setvjmode,		/* set VJ compression mode */
  NULL
  };


/*
 * PPP Data Link Layer "protocol" table.
 * One entry per supported protocol.
 */
static struct protent {
  u_short protocol;
  void (*init)();
  void (*input)();
  void (*protrej)();
} prottbl[] = {
  { LCP, lcp_init, lcp_input, lcp_protrej },
  { IPCP, ipcp_init, ipcp_input, ipcp_protrej },
  { UPAP, upap_init, upap_input, upap_protrej },
  { CHAP, ChapInit, ChapInput, ChapProtocolReject },
};


static char *usage = "pppd version %s patch level %d\n\
Usage: %s [ arguments ], where arguments are:\n\
	-all		Don't request/allow any options\n\
	-ac		Disable Address/Control compression\n\
	-am		Disable asyncmap negotiation\n\
	-as <n>		Set the desired async map to hex <n>\n\
	-d		Increase debugging level\n\
	-detach		Don't fork to background\n\
	-ip		Disable IP address negotiation\n\
	-mn		Disable magic number negotiation\n\
	-mru		Disable mru negotiation\n\
	-p		Set passive mode\n\
	-pc		Disable protocol field compression\n\
	+ua <p>		Require UPAP authentication and use file <p> for\n\
                        remote login data\n\
	-ua		Don't allow UPAP authentication\n\
	+chap		Require CHAP authentication\n\
	-chap		Don't allow CHAP authentication\n\
        -vj             disable VJ compression\n\
        connect <p>     Invoke shell command <p> to set up the serial line\n\
	crtscts		Use hardware RTS/CTS flow control\n\
	debug		Increase debugging level\n\
        domain <d>      Append domain name <d> to hostname for authentication\n\
	mru <n>		Set MRU value to <n> for negotiation\n\
	netmask <n>	Set interface netmask to <n>\n\
	passive		Set passive mode\n\
	vjmode <m>      VJ compression mode {old, rfc1172, rfc1132 (default)}\n\
	<device>	Communicate over the named device\n\
	<speed>		Set the baud rate to <speed>\n\
	<loc>:<rem>	Set the local and/or remote interface IP\n\
			addresses.  Either one may be omitted.\n";


main(argc, argv)
     int argc;
     char *argv[];
{
  int mask, i;
  struct sigvec sv;
  struct cmd *cmdp;
  FILE *pidfile;
#ifndef STREAMS
  int pppdisc = PPPDISC;
#endif

  /*
   * Initialize syslog system and magic number package.
   */
#if BSD >= 43 || defined(sun)
  openlog("pppd", LOG_PID | LOG_NDELAY, LOG_PPP);
  setlogmask(LOG_UPTO(LOG_INFO));
#else
  openlog("pppd", LOG_PID);
#define LOG_UPTO(x) (x)
#define setlogmask(x) (x)
#endif

#ifdef STREAMS
  if (ttyname(fileno(stdin)))
    strcpy(devname, ttyname(fileno(stdin)));
#endif
  
  magic_init();

  if (gethostname(hostname, MAX_HOSTNAME_LEN) < 0 ) {
    syslog(LOG_ERR, "couldn't get hostname: %m");
    exit(1);
  }

  /*
   * Initialize to the standard option set and then parse the command
   * line arguments.
   */
  for (i = 0; i < sizeof (prottbl) / sizeof (struct protent); i++)
    (*prottbl[i].init)(0);
  
  progname = *argv;
  for (argc--, argv++; argc; ) {
    /*
     * First see if it's a command.
     */
    for (cmdp = cmds; cmdp->cmd_name; cmdp++)
      if (!strcmp(*argv, cmdp->cmd_name) &&
	  (*cmdp->cmd_func)(&argc, &argv))
	break;
    
    /*
     * Maybe a tty name, speed or IP address?
     */
    if (cmdp->cmd_name == NULL &&
	!setdevname(&argc, &argv) &&
	!setspeed(&argc, &argv) &&
	!setipaddr(&argc, &argv)) {
      fprintf(stderr, usage, VERSION, PATCHLEVEL, progname);
      exit(1);
    }
  }

  syslog(LOG_INFO, "Starting pppd %s patch level %d",
	 VERSION, PATCHLEVEL); 

  /*
   * Initialize state.
   */


#define SETSID
#ifdef SETSID
  if (default_device) {
    /* No device name was specified... inherit the old controlling
       terminal */
    
    if ((pgrpid = getpgrp(0)) < 0) {
      syslog(LOG_ERR, "getpgrp(0): %m");
      exit(1);
    }
    if (pgrpid != pid) 
      syslog(LOG_WARNING, "warning... not a process group leader");
  }
  else /*default_device*/
  {
      /* become session leader... */

	  if (!nodetach) {
		  /* fork so we're not a process group leader */
		  if (pid = fork()) {
			  exit(0);
		  }
	  }
#ifdef xxx
	  else
		  /* bag controlling terminal */
		  if (ioctl(0, TIOCNOTTY) < 0) {
			  syslog(LOG_ERR, "ioctl(TIOCNOTTY): %m");
			  exit(1);
		  }
#endif

      /* create new session */
      if ((pgrpid = setsid()) < 0) {
	  syslog(LOG_ERR, "setsid(): %m");
	  exit(1);
      }
  }
#endif

  /* open i/o device */
  if ((fd = open(devname, O_RDWR /*| O_NDELAY*/)) < 0) {
    syslog(LOG_ERR, "open(%s): %m", devname);
    exit(1);
  }

  /* drop dtr to hang up incase modem is off hook */
  if (!default_device) {
      setdtr(fd, FALSE);
      sleep(1);
      setdtr(fd, TRUE);
  }

  /* set device to be controlling tty */
  if (ioctl(fd, TIOCSCTTY) < 0) {
    syslog(LOG_ERR, "ioctl(TIOCSCTTY): %m");
    exit(1);
  }

  /* run connection script */
  if (connector) {
      syslog(LOG_NOTICE, "Connecting with <%s>", connector);
	/* set line speed */
	set_up_tty(fd, 0);
    if (set_up_connection(connector, fd, fd) < 0) {
      syslog(LOG_ERR, "could not set up connection");
      setdtr(fd, FALSE);
      exit(1);
    }
    syslog(LOG_NOTICE, "Connected...");
  }
  
  if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    syslog(LOG_ERR, "socket : %m");
    exit(1);
  }
  
  /* if we exit, then try and restore the stream */ 
#ifdef sun
  on_exit(cleanup, NULL);
#endif
  
#ifdef	STREAMS
  /* go through and save the name of all the modules, then pop em */
  while(1)	{ 
    if(!ioctl(fd, I_LOOK, str_modules[str_module_count].modname))
	MAINDEBUG((LOG_DEBUG, "popped stream module : %s",
		str_modules[str_module_count].modname))
    if(!ioctl(fd, I_POP, 0))
      str_module_count++;
    else
      break;
  }

  /* set line speed */
  set_up_tty(fd, 1);

  syslog(LOG_ERR, "about to push modules...");

  /* now push the async/fcs module */
  if(ioctl(fd, I_PUSH, "pppasync") < 0) {
    syslog(LOG_ERR, "ioctl(I_PUSH, ppp_async): %m");
    exit(1);
  }
  /* finally, push the ppp_if module that actually handles the */
  /* network interface */ 
  if(ioctl(fd, I_PUSH, "pppif") < 0) {
    syslog(LOG_ERR, "ioctl(I_PUSH, ppp_if): %m");
    exit(1);
  }
  if(ioctl(fd, I_SETSIG, S_INPUT) < 0) {
    syslog(LOG_ERR, "ioctl(I_SETSIG, S_INPUT): %m");
    exit(1);
  }
  /* read mode, message non-discard mode */
  if(ioctl(fd, I_SRDOPT, RMSGN) < 0) {
    syslog(LOG_ERR, "ioctl(I_SRDOPT, RMSGN): %m");
    exit(1);
  }
  /* Flush any waiting messages, or we'll never get SIGPOLL */
  if(ioctl(fd, I_FLUSH, FLUSHRW) < 0) {
    syslog(LOG_ERR, "ioctl(I_FLUSH, FLUSHRW): %m");
    exit(1);
  }
  /*
   * Find out which interface we were given.
   * (ppp_if handles this ioctl)
   */
  if (ioctl(fd, SIOCGETU, &ifunit) < 0) {
    syslog(LOG_ERR, "ioctl(SIOCGETU): %m");
    exit(1);
  }

  /* if debug, set debug flags in driver */
  {
      int flags = debug ? 0x3 : 0;
syslog(LOG_INFO, "debug 0x%x, flags 0x%x", debug, flags);
      if (ioctl(fd, SIOCSIFDEBUG, &flags) < 0) {
	  syslog(LOG_ERR, "ioctl(SIOCSIFDEBUG): %m");
      }
  }

  syslog(LOG_ERR, "done pushing modules, ifunit %d", ifunit);
#else
  /* set line speed */
  set_up_tty(fd, 1);

  if (ioctl(fd, TIOCGETD, &initdisc) < 0) {
    syslog(LOG_ERR, "ioctl(TIOCGETD): %m");
    exit(1);
  }
  if (ioctl(fd, TIOCSETD, &pppdisc) < 0) {
    syslog(LOG_ERR, "ioctl(TIOCSETD): %m");
    exit(1);
  }

  /*
   * Find out which interface we were given.
   */
  if (ioctl(fd, PPPIOCGUNIT, &ifunit) < 0) {	
    syslog(LOG_ERR, "ioctl(PPPIOCGUNIT): %m");
    exit(1);
  }
#endif

  syslog(LOG_NOTICE, "Using interface ppp%d", ifunit);
  (void) sprintf(ifname, "ppp%d", ifunit);
  pid = getpid();

  (void) sprintf(pidfilename, "%s/%s.pid", pidpath, ifname);

  /* write pid to file */

  if ((pidfile = fopen(pidfilename, "w")) != NULL) {
    fprintf(pidfile, "%d\n", pid);
    (void) fclose(pidfile);
  }

  hostname_len = (u_char) strlen(hostname);

  MAINDEBUG((LOG_DEBUG, "hostname = %s", hostname))
      
#ifdef SETSID
  if (default_device) {
    int id = tcgetpgrp(fd);
    if (id != pgrpid) {
      syslog(LOG_WARNING,
	     "warning: pppd is not the leader of a forground process group");
    }
  }
  else
    if (tcsetpgrp(fd, pgrpid) < 0) {
      syslog(LOG_ERR, "tcsetpgrp(): %m");
      exit(1);
    }
#else
  /* set process group on tty so we get SIGIO's */
  if (ioctl(fd, TIOCSPGRP, &pgrpid) < 0) {
    syslog(LOG_ERR, "ioctl(TIOCSPGRP): %m");
    exit(1);
  }
#endif
  
  /*
   * Compute mask of all interesting signals and install signal handlers
   * for each.  Only one signal handler may be active at a time.  Therefore,
   * all other signals should be masked when any handler is executing.
   */
  mask = sigmask(SIGHUP) | sigmask(SIGINT) | sigmask(SIGALRM) |
    sigmask(SIGIO);
#ifdef	STREAMS
  mask |= sigmask(SIGPOLL);
#endif

  sv.sv_handler = hup;		/* Hangup */
  sv.sv_mask = mask;
  sv.sv_flags = 0;
  if (sigvec(SIGHUP, &sv, NULL)) {
    syslog(LOG_ERR, "sigvec(SIGHUP)");
    exit(1);
  }
  sv.sv_handler = intr;		/* Interrupt */
  sv.sv_mask = mask;
  sv.sv_flags = 0;
  if (sigvec(SIGINT, &sv, NULL)) {
    syslog(LOG_ERR, "sigvec(SIGINT)");
    exit(1);
  }
  sv.sv_handler = term;		/* Terminate */
  sv.sv_mask = mask;
  sv.sv_flags = 0;
  if (sigvec(SIGTERM, &sv, NULL)) {
    syslog(LOG_ERR, "sigvec(SIGTERM)");
    exit(1);
  }
  sv.sv_handler = alrm;		/* Timeout */
  sv.sv_mask = mask;
  sv.sv_flags = 0;
  if (sigvec(SIGALRM, &sv, NULL)) {
    syslog(LOG_ERR, "sigvec(SIGALRM)");
    exit(1);
  }
  sv.sv_handler = io;			/* Input available */
  sv.sv_mask = mask;
  sv.sv_flags = 0;
  if (sigvec(SIGIO, &sv, NULL)) {
    syslog(LOG_ERR, "sigvec(SIGIO)");
    exit(1);
  }
#ifdef	STREAMS
  sv.sv_handler = io;			/* Input available */
  sv.sv_mask = mask;
  sv.sv_flags = 0;
  if (sigvec(SIGPOLL, &sv, NULL)) {
    syslog(LOG_ERR, "sigvec(SIGPOLL)");
    exit(1);
  }
#endif

#ifdef __STDC__
  /* Increment debug flag */
  (void) signal(SIGUSR1, (void (*)(int))incdebug);
  /* Reset debug flag */
  (void) signal(SIGUSR2, (void (*)(int))nodebug);
#else
  /* Increment debug flag */
  (void) signal(SIGUSR1, (void (*)())incdebug);
  /* Reset debug flag */
  (void) signal(SIGUSR2, (void (*)())nodebug);
#endif
  
  /*
   * Record initial device flags, then set device to cause SIGIO
   * signals to be generated.
   */
  if ((initfdflags = fcntl(fd, F_GETFL)) == -1) {
    syslog(LOG_ERR, "fcntl(F_GETFL): %m");
    exit(1);
  }
  if (fcntl(fd, F_SETFL, FNDELAY | FASYNC) == -1) {
    syslog(LOG_ERR, "fcntl(F_SETFL, FNDELAY | FASYNC): %m");
    exit(1);
  }
  
  /*
   * Block all signals, start opening the connection, and  wait for
   * incoming signals (reply, timeout, etc.).
   */
  syslog(LOG_INFO, "Connect: %s <--> %s", ifname, devname);
  sigblock(mask);			/* Block signals now */
  lcp_lowerup(0);			/* XXX Well, sort of... */
  if (lcp_wantoptions[0].passive)
    lcp_passiveopen(0);		/* Start protocol in passive mode */
  else
    lcp_activeopen(0);		/* Start protocol in active mode */
  for (;;) {
    sigpause(0);			/* Wait for next signal */
    
    /* Need to read user/passwd? */
    if (upap[0].us_flags & UPAPF_UPPENDING) {
      sigsetmask(0);		/* Allow other signals to occur */
      getuserpasswd();		/* Get user and passwd */
      upap[0].us_flags &= ~UPAPF_UPPENDING;
      upap[0].us_flags |= UPAPF_UPVALID;
      sigsetmask(mask);		/* Disallow signals */
      upap_authwithpeer(0);
    }
  }
}

set_up_tty(fd, flow)
int fd;
int flow;
{
#ifdef	STREAMS
  int new_cflag;
  struct termios	tios;

  if(ioctl(fd, TCGETS, (caddr_t) &tios) < 0) {
    syslog(LOG_ERR, "ioctl(TCGETS): %m");
    exit(1);
  }

  new_cflag = CS8 | CREAD | HUPCL;
  new_cflag |= inspeed ? inspeed : (tios.c_cflag & CBAUD);
  if (flow)
      new_cflag |= crtscts ? CRTSCTS : 0;

  tios.c_cflag = new_cflag;
  tios.c_iflag = IGNBRK | IGNPAR;
  tios.c_oflag = 0;
  tios.c_lflag = 0;
  
  if(ioctl(fd, TCSETS, (caddr_t) &tios) < 0) {
    syslog(LOG_ERR, "ioctl(TCSETS): %m");
    exit(1);
  }
#else
#ifdef SGTTY
  struct sgttyb sgttyb;

  /*
   * Put the tty in raw mode and set the discipline to PPP.
   */
  if (ioctl(fd, TIOCGETP, &initsgttyb) < 0) {
    syslog(LOG_ERR, "ioctl(TIOCGETP): %m");
    exit(1);
  }

  sgttyb = initsgttyb;
  sgttyb.sg_flags = RAW | ANYP;
  if (inspeed)
    sgttyb.sg_ispeed = inspeed;

  if (ioctl(fd, TIOCSETP, &sgttyb) < 0) {
    syslog(LOG_ERR, "ioctl(TIOCSETP): %m");
    exit(1);
  }
#else
    struct termios tios;

    if (ioctl(fd, TIOCGETA, &tios) < 0) {
	syslog(LOG_ERR, "ioctl(TIOCGETA): %m");
	exit(1);
    }

    inittermios = tios;

    tios.c_cflag = CREAD | CS8 | HUPCL;
    if (flow)
	tios.c_cflag |= crtscts ? CRTSCTS : 0;
    tios.c_iflag = IGNBRK | IGNPAR;
    tios.c_oflag = 0;
    tios.c_lflag = 0;
    tios.c_cc[VERASE] = tios.c_cc[VKILL] = 0;
    tios.c_cc[VMIN] = 1;
    tios.c_cc[VTIME] = 0;
    if (inspeed) {
        tios.c_ispeed = inspeed;
        tios.c_ospeed = inspeed;
    }

    if (ioctl(fd, TIOCSETA, &tios) < 0) {
	syslog(LOG_ERR, "ioctl(TIOCSETA): %m");
	exit(1);
    }
#endif
#endif
}

/*
 * quit - Clean up state and exit.
 */
void 
  quit()
{
  syslog(LOG_NOTICE, "Quitting");

  if (fd == 0)
	return;

  if (fcntl(fd, F_SETFL, initfdflags) == -1) {
    syslog(LOG_ERR, "fcntl(F_SETFL, fdflags): %m");
    exit(1);
  }

#ifdef	STREAMS
  str_restore();
#else
#ifdef SGTTY
  if (ioctl(fd, TIOCSETP, &initsgttyb) < 0) {
    syslog(LOG_ERR, "ioctl(TIOCSETP)");
    exit(1);
  }
#else
  if (ioctl(fd, TIOCSETA, &inittermios) < 0) {
    syslog(LOG_ERR, "ioctl(TIOCSETA)");
    exit(1);
  }

#endif
  if (ioctl(fd, TIOCSETD, &initdisc) < 0) {
    syslog(LOG_ERR, "ioctl(TIOCSETD)");
    exit(1);
  }
#endif

  /* drop dtr to hang up */
  setdtr(fd, FALSE);

  close(fd);
  fd = 0;

  exit(0);
}


static struct callout *callout = NULL;		/* Callout list */
static struct timeval schedtime;		/* Time last timeout was set */


/*
 * timeout - Schedule a timeout.
 *
 * Note that this timeout takes the number of seconds, NOT hz (as in
 * the kernel).
 */
void timeout(func, arg, time)
     void (*func)();
     caddr_t arg;
     int time;
{
  struct itimerval itv;
  struct callout *newp, **oldpp;
  
  MAINDEBUG((LOG_DEBUG, "Timeout %x:%x in %d seconds.",
	    (int) func, (int) arg, time))
  
  /*
   * Allocate timeout.
   */
  if ((newp = (struct callout *) malloc(sizeof(struct callout))) == NULL) {
    syslog(LOG_ERR, "Out of memory in timeout()!");
    exit(1);
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
    MAINDEBUG((LOG_DEBUG, "Setting itimer for %d seconds.",
	      itv.it_value.tv_sec))
    if (setitimer(ITIMER_REAL, &itv, NULL)) {
      syslog(LOG_ERR, "setitimer(ITIMER_REAL)");
      exit(1);
    }
    if (gettimeofday(&schedtime, NULL)) {
      syslog(LOG_ERR, "gettimeofday");
      exit(1);
    }
  }
}


/*
 * untimeout - Unschedule a timeout.
 */
void untimeout(func, arg)
     void (*func)();
     caddr_t arg;
{
  
  struct itimerval itv;
  struct callout **copp, *freep;
  int reschedule = 0;
  
  MAINDEBUG((LOG_DEBUG, "Untimeout %x:%x.", (int) func, (int) arg))
  
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
    MAINDEBUG((LOG_DEBUG, "Setting itimer for %d seconds.",
	      itv.it_value.tv_sec))
    if (setitimer(ITIMER_REAL, &itv, NULL)) {
      syslog(LOG_ERR, "setitimer(ITIMER_REAL)");
      exit(1);
    }
    if (gettimeofday(&schedtime, NULL)) {
      syslog(LOG_ERR, "gettimeofday");
      exit(1);
    }
  }
}


/*
 * adjtimeout - Decrement the first timeout by the amount of time since
 * it was scheduled.
 */
void adjtimeout()
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
    syslog(LOG_ERR, "gettimeofday");
    exit(1);
  }
  timediff = tv.tv_sec - schedtime.tv_sec;
  if (timediff < 0 ||
      timediff > callout->c_time + 1)
    return;
  
  callout->c_time -= timediff;	/* OK, Adjust time */
}


/*
 * output - Output PPP packet.
 */
void
  output(unit, p, len)
int unit;
u_char *p;
int len;
{
#ifdef	STREAMS
  struct strbuf	str;

  str.len = len;
  str.buf = (caddr_t) p;
  if(putmsg(fd, NULL, &str, 0) < 0) {
    syslog(LOG_ERR, "putmsg");
    exit(1);
  }
#else
  if (unit != 0) {
    MAINDEBUG((LOG_WARNING, "output: unit != 0!"))
    abort();
  }

  if (write(fd, p, len) < 0) {
    syslog(LOG_ERR, "write");
    exit(1);
  }
#endif
}


/*
 * hup - Catch SIGHUP signal.
 *
 * Indicates that the physical layer has been disconnected.
 */
/*ARGSUSED*/
static void
  hup(sig, code, scp, addr)
int sig, code;
struct sigcontext *scp;
char *addr;
{
  syslog(LOG_NOTICE, "Hangup (SIGHUP)");
  adjtimeout();		/* Adjust timeouts */
  lcp_lowerdown(0);		/* Reset connection */
}


/*
 * term - Catch SIGTERM signal.
 *
 * Indicates that we should initiate a graceful disconnect and exit.
 */
/*ARGSUSED*/
static void
  term(sig, code, scp, addr)
int sig, code;
struct sigcontext *scp;
char *addr;
{
  syslog(LOG_NOTICE, "Terminate signal received.");
  adjtimeout();		/* Adjust timeouts */
  lcp_close(0);		/* Close connection */
}


/*
 * intr - Catch SIGINT signal (DEL/^C).
 *
 * Indicates that we should initiate a graceful disconnect and exit.
 */
/*ARGSUSED*/
static void
  intr(sig, code, scp, addr)
int sig, code;
struct sigcontext *scp;
char *addr;
{
  syslog(LOG_NOTICE, "Interrupt received.  Exiting.");
  adjtimeout();		/* Adjust timeouts */
  lcp_close(0);		/* Close connection */
}


/*
 * alrm - Catch SIGALRM signal.
 *
 * Indicates a timeout.
 */
/*ARGSUSED*/
static void
  alrm(sig, code, scp, addr)
int sig, code;
struct sigcontext *scp;
char *addr;
{
  struct itimerval itv;
  struct callout *freep;
  
  MAINDEBUG((LOG_DEBUG, "Alarm"))
  
  /*
   * Call and free first scheduled timeout and any that were scheduled
   * for the same time.
   */
  while (callout) {
    freep = callout;	/* Remove entry before calling */
    callout = freep->c_next;
    (*freep->c_func)(freep->c_arg);
    (void) free((char *) freep);
    if (callout && callout->c_time)
      break;
  }
  
  /*
   * Set a new itimer if there are more timeouts scheduled.
   */
  if (callout) {
    itv.it_interval.tv_sec = itv.it_interval.tv_usec =
      itv.it_value.tv_usec = 0;
    itv.it_value.tv_sec = callout->c_time;
    MAINDEBUG((LOG_DEBUG, "Setting itimer for %d seconds.",
	      itv.it_value.tv_sec))
    if (setitimer(ITIMER_REAL, &itv, NULL)) {
      syslog(LOG_ERR, "setitimer(ITIMER_REAL)");
      exit(1);
    }
    if (gettimeofday(&schedtime, NULL)) {
      syslog(LOG_ERR, "gettimeofday");
      exit(1);
    }
  }
}


/*
 * io - Catch SIGIO signal.
 *
 * Indicates that incoming data is available.
 */
/*ARGSUSED*/
static void
  io(sig, code, scp, addr)
int sig, code;
struct sigcontext *scp;
char *addr;
{
  int len, i;
  u_char *p;
  u_short protocol;
  fd_set fdset;
  struct timeval notime;
  int ready;
#ifdef	STREAMS
  struct strbuf str;
#endif


  MAINDEBUG((LOG_DEBUG, "IO signal received"))
  adjtimeout();		/* Adjust timeouts */

  /* we do this to see if the SIGIO handler is being invoked for input */
  /* ready, or for the socket buffer hitting the low-water mark. */

  notime.tv_sec = 0;
  notime.tv_usec = 0;

  FD_ZERO(&fdset);
  FD_SET(fd, &fdset);
  
  if ((ready = select(32, &fdset, (fd_set *) NULL, (fd_set *) NULL,
		      &notime)) == -1) {
    syslog(LOG_ERR, "Error in io() select: %m");
    exit(1);
  }
    
  if (ready == 0) {
    MAINDEBUG((LOG_DEBUG, "IO non-input ready SIGIO occured."));
    return;
  }

  /* Yup, this is for real */
  for (;;) {			/* Read all available packets */
    p = inpacket_buf;		/* point to beggining of packet buffer */

#ifdef STREAMS
    str.maxlen = MTU+DLLHEADERLEN;
    str.buf = (caddr_t) p;
    i = 0;
    len = getmsg(fd, NULL, &str, &i);
    if(len < 0) {
      if(errno == EAGAIN || errno == EWOULDBLOCK) {
	return;
      }
      syslog(LOG_ERR, "getmsg(fd) %m");
      exit(1);
    }
    else if(len) 
      MAINDEBUG((LOG_DEBUG, "getmsg returns with length 0x%x",len))
    
    if(str.len < 0) {
      MAINDEBUG((LOG_DEBUG, "getmsg short return length %d",
		str.len))
      return;
    }
    
    len = str.len;
#else
    if ((len = read(fd, p, MTU + DLLHEADERLEN)) < 0) {
      if (errno == EWOULDBLOCK) {
	MAINDEBUG((LOG_DEBUG, "read(fd): EWOULDBLOCK"))
	return;
      }
      else {
	syslog(LOG_ERR, "read(fd): %m");
	exit(1);
      }
    }
    else 
#endif
      if (len == 0) {
	syslog(LOG_ERR, "End of file on fd!");
	exit(1);
      }
    
    if (len < DLLHEADERLEN) {
      MAINDEBUG((LOG_INFO, "io(): Received short packet."))
      return;
    }
    
    p += 2;				/* Skip address and control */
    GETSHORT(protocol, p);
    len -= DLLHEADERLEN;
    
    /*
     * Toss all non-LCP packets unless LCP is OPEN.
     */
    if (protocol != LCP && lcp_fsm[0].state != OPEN) {
      MAINDEBUG((LOG_INFO, "io(): Received non-LCP packet and LCP is not in open state."))
	  dumpbuffer(inpacket_buf, len + DLLHEADERLEN, LOG_ERR);
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
      p -= DLLHEADERLEN;
      len += DLLHEADERLEN;
      lcp_sprotrej(0, p, len);
    }
  }
}

/*
 * cleanup - clean_up before we exit
 */
/* ARGSUSED */
static void
  cleanup(status, arg)
int status;
caddr_t arg;
{
  adjtimeout();
  lcp_lowerdown(0);
  if (unlink(pidfilename) < 0) 
    syslog(LOG_WARNING, "unable to unlink pid file: %m");
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
  syslog(LOG_WARNING, "demuxprotrej: Unrecognized Protocol-Reject for protocol %d!", protocol);
}


/*
 * incdebug - Catch SIGUSR1 signal.
 *
 * Increment debug flag.
 */
/*ARGSUSED*/
static void
  incdebug(sig, code, scp, addr)
int sig, code;
struct sigcontext *scp;
char *addr;
{
  syslog(LOG_NOTICE, "Debug turned ON, Level %d", debug);
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
  nodebug(sig, code, scp, addr)
int sig, code;
struct sigcontext *scp;
char *addr;
{
  setlogmask(LOG_UPTO(LOG_WARNING));
  debug = 0;
}


/*
 * setdebug - Set debug (command line argument).
 */
static int
  setdebug(argcp, argvp)
int *argcp;
char ***argvp;
{
  debug++;
  setlogmask(LOG_UPTO(LOG_DEBUG));
  --*argcp, ++*argvp;
  return (1);
}

/*
 * noopt - Disable all options.
 */
static int
  noopt(argcp, argvp)
int *argcp;
char ***argvp;
{
  bzero((char *) &lcp_wantoptions[0], sizeof (struct lcp_options));
  bzero((char *) &lcp_allowoptions[0], sizeof (struct lcp_options));
  bzero((char *) &ipcp_wantoptions[0], sizeof (struct ipcp_options));
  bzero((char *) &ipcp_allowoptions[0], sizeof (struct ipcp_options));
  --*argcp, ++*argvp;
  return (1);
}


/*
 * setconnector - Set a program to connect to a serial line
 */
static int
  setconnector(argcp, argvp)
int *argcp;
char ***argvp;
{
  
  --*argcp, ++*argvp;
  
  connector = strdup(**argvp);
  if (connector == NULL) {
    syslog(LOG_ERR, "cannot allocate space for connector string");
    exit(1);
  }
  
  --*argcp, ++*argvp;
  return (1);
}


/*
 * set_up_connection - run a program to initialize the serial connector
 */
int set_up_connection(program, in, out)
     char *program;
     int in, out;
{
  int pid;
  int flags;
  int status;
  
  flags = sigblock(sigmask(SIGINT)|sigmask(SIGHUP));
  pid = fork();
  
  if (pid < 0) {
    syslog(LOG_ERR, "fork");
    exit(1);
  }
  
  if (pid == 0) {
    (void) setreuid(getuid(), getuid());
    (void) setregid(getgid(), getgid());
    (void) sigsetmask(flags);
    (void) dup2(in, 0);
    (void) dup2(out, 1);
    (void) execl("/bin/sh", "sh", "-c", program, (char *)0);
    syslog(LOG_ERR, "could not exec /bin/sh");
    _exit(99);
  }
  else {
    while (waitpid(pid, &status, 0) != pid) {
      if (errno == EINTR)
	continue;
      syslog(LOG_ERR, "waiting for connection process");
      exit(1);
    }
    (void) sigsetmask(flags);
  }
  return (status == 0 ? 0 : -1);
}

/*
 * noaccomp - Disable Address/Control field compression negotiation.
 */
static int
  noaccomp(argcp, argvp)
int *argcp;
char ***argvp;
{
  lcp_wantoptions[0].neg_accompression = 0;
  lcp_allowoptions[0].neg_accompression = 0;
  --*argcp, ++*argvp;
  return (1);
}


/*
 * noasyncmap - Disable async map negotiation.
 */
static int
  noasyncmap(argcp, argvp)
int *argcp;
char ***argvp;
{
  lcp_wantoptions[0].neg_asyncmap = 0;
  lcp_allowoptions[0].neg_asyncmap = 0;
  --*argcp, ++*argvp;
  return (1);
}


/*
 * noipaddr - Disable IP address negotiation.
 */
static int
  noipaddr(argcp, argvp)
int *argcp;
char ***argvp;
{
  ipcp_wantoptions[0].neg_addrs = 0;
  ipcp_allowoptions[0].neg_addrs = 0;
  --*argcp, ++*argvp;
  return (1);
}


/*
 * nomagicnumber - Disable magic number negotiation.
 */
static int
  nomagicnumber(argcp, argvp)
int *argcp;
char ***argvp;
{
  lcp_wantoptions[0].neg_magicnumber = 0;
  lcp_allowoptions[0].neg_magicnumber = 0;
  --*argcp, ++*argvp;
  return (1);
}


/*
 * nomru - Disable mru negotiation.
 */
static int
  nomru(argcp, argvp)
int *argcp;
char ***argvp;
{
  lcp_wantoptions[0].neg_mru = 0;
  lcp_allowoptions[0].neg_mru = 0;
  --*argcp, ++*argvp;
  return (1);
}


/*
 * setmru - Set MRU for negotiation.
 */
static int
  setmru(argcp, argvp)
int *argcp;
char ***argvp;
{
  --*argcp, ++*argvp;
  lcp_wantoptions[0].mru = atoi(**argvp);
  --*argcp, ++*argvp;
  return (1);
}


/*
 * nopcomp - Disable Protocol field compression negotiation.
 */
static int
  nopcomp(argcp, argvp)
int *argcp;
char ***argvp;
{
  lcp_wantoptions[0].neg_pcompression = 0;
  lcp_allowoptions[0].neg_pcompression = 0;
  --*argcp, ++*argvp;
  return (1);
}


/*
 * setpassive - Set passive mode.
 */
static int
  setpassive(argcp, argvp)
int *argcp;
char ***argvp;
{
  lcp_wantoptions[0].passive = 1;
  --*argcp, ++*argvp;
  return (1);
}


/*
 * noupap - Disable UPAP authentication.
 */
static int
  noupap(argcp, argvp)
int *argcp;
char ***argvp;
{
  lcp_allowoptions[0].neg_upap = 0;
  --*argcp, ++*argvp;
  return (1);
}


/*
 * requpap - Require UPAP authentication.
 */
static int
  requpap(argcp, argvp)
int *argcp;
char ***argvp;
{
  FILE * ufile;
  struct stat sbuf;

  lcp_wantoptions[0].neg_upap = 1;
  lcp_allowoptions[0].neg_upap = 0;
  --*argcp, ++*argvp;
  strcpy(uinfopath, **argvp);
  --*argcp, ++*argvp;

  /* open user info file */

  if ((ufile = fopen(uinfopath, "r")) == NULL) {
    fprintf(stderr,  "unable to open user login data file %s\n", uinfopath);
    exit(1);
  };
     
  if (fstat(fileno(ufile), &sbuf) < 0) {
    perror("cannot stat user login data file!");
    exit(1);
  }
  if ((sbuf.st_mode & 077) != 0)
    syslog(LOG_WARNING, "Warning - user info file has world and/or group access!\n");

  /* get username */
  fgets(user, sizeof (user) - 1, ufile);
  if (strlen(user) == 0) {
    fprintf(stderr, "Unable to get user name from user login data file.\n");
    exit(2);
  }
  /* get rid of newline */
  user[strlen(user) - 1] = '\000';

  fgets(passwd, sizeof(passwd) - 1, ufile);

  if (strlen(passwd) == 0) {
    fprintf(stderr, "Unable to get password from user login data file.\n");
    exit(2);
  }

  passwd[strlen(passwd) - 1] = '\000';

  return (1);
}


/*
 * nochap - Disable CHAP authentication.
 */
static int
  nochap(argcp, argvp)
int *argcp;
char ***argvp;
{
  lcp_allowoptions[0].neg_chap = 0;
  --*argcp, ++*argvp;
  return (1);
}


/*
 * reqchap - Require CHAP authentication.
 */
static int
  reqchap(argcp, argvp)
int *argcp;
char ***argvp;
{
  lcp_wantoptions[0].neg_chap = 1;
  lcp_allowoptions[0].neg_chap = 0;
  --*argcp, ++*argvp;
  return (1);
}


/*
 *	setvjmode - Set vj compression mode
 */

static int
  setvjmode(argcp, argvp)
int *argcp;
char ***argvp;
{
  extern int ipcp_vj_mode;
  
  --*argcp, ++*argvp;
  
  if (!strcmp(**argvp, "old")) {	/* "old" mode */
    ipcp_vj_setmode(IPCP_VJMODE_OLD);
  }
  
  else if (!strcmp(**argvp, "rfc1172")) {	/* "rfc1172" mode*/
    ipcp_vj_setmode(IPCP_VJMODE_RFC1172);
  }
  
  else if (!strcmp(**argvp, "rfc1332")) {	/* "rfc1332" default mode */
    ipcp_vj_setmode(IPCP_VJMODE_RFC1332);
  }
  else {
    syslog(LOG_WARNING,
	   "Unknown vj compression mode %s. Defaulting to RFC1332", **argvp);
    ipcp_vj_setmode(IPCP_VJMODE_RFC1332);
  }
  --*argcp, ++*argvp;
  
  return (1);
}
/*
 *	setnovj - diable vj compression
 */

static int
  setnovj(argcp, argvp)
int *argcp;
char ***argvp;
{
  extern int ipcp_vj_mode;
  
  --*argcp, ++*argvp;
  ipcp_wantoptions[0].neg_vj = 0;
  ipcp_allowoptions[0].neg_vj = 0;
  
  return (1);
}

/*
 * setdomain - Set domain name to append to hostname 
 */
static int
  setdomain(argcp, argvp)
int *argcp;
char ***argvp;
{
  
  --*argcp, ++*argvp;

  strcat(hostname, **argvp);
  hostname_len = strlen(hostname);

  --*argcp, ++*argvp;

  return (1);
}

/*
 * Valid speeds.
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

static int
  setasyncmap(argcp, argvp) 
int	*argcp;
char	***argvp;
{
  unsigned long asyncmap;
  
  asyncmap = 0xffffffff;
  ++*argvp;
  sscanf(**argvp,"%lx",&asyncmap);
  ++*argvp;
  lcp_wantoptions[0].asyncmap = asyncmap;
  *argcp -= 2;
  return(1);
}

/*
 * setspeed - Set the speed.
 */
static int
  setspeed(argcp, argvp)
int *argcp;
char ***argvp;
{
  int speed;
  struct speed *speedp;
  
  speed = atoi(**argvp);
  for (speedp = speeds; speedp->speed_int; speedp++)
    if (speed == speedp->speed_int) {
      inspeed = speedp->speed_val;
      --*argcp, ++*argvp;
      return (1);
    }
  return (0);
}


/*
 * setdevname - Set the device name.
 */
int setdevname(argcp, argvp)
     int *argcp;
     char ***argvp;
{
  char dev[DEVNAME_SIZE];
  char *cp = **argvp;
  struct stat statbuf;
  char *tty, *ttyname();
  
  if (strncmp("/dev/", cp, sizeof ("/dev/") - 1)) {
    (void) sprintf(dev, "/dev/%s", cp);
    cp = dev;
  }
  
  /*
   * Check if there is a device by this name.
   */
  if (stat(cp, &statbuf) < 0) {
    if (errno == ENOENT)
      return (0);
    syslog(LOG_ERR, cp);
    exit(1);
  }
  
  (void) strcpy(devname, cp);
  default_device = FALSE;
  --*argcp, ++*argvp;
  
  /*
   * If we haven't already decided to require authentication,
   * or we are running ppp on the control terminal, then we can
   * allow authentication to be requested.
   */
  if ((tty = ttyname(fileno(stdin))) == NULL)
    tty = ""; /* running from init means no stdin.  Null kills strcmp -KWK */
  if (lcp_wantoptions[0].neg_upap == 0 &&
      strcmp(devname, "/dev/tty") &&
      strcmp(devname, tty)) {
    lcp_wantoptions[0].neg_upap = 0;
    lcp_allowoptions[0].neg_upap = 1;
  }
  return (1);
}


/*
 * setipaddr - Set the IP address
 */
int setipaddr(argcp, argvp)
     int *argcp;
     char ***argvp;
{
  u_long local, remote;
  struct hostent *hp;
  char *colon, *index();
  
  /*
   * IP address pair separated by ":".
   */
  if ((colon = index(**argvp, ':')) == NULL)
    return (0);
  
  /*
   * If colon first character, then no local addr.
   */
  if (colon == **argvp) {
    local = 0l;
    ++colon;
  }
  else {
    *colon++ = '\0';
    if ((local = inet_addr(**argvp)) == -1) {
      if ((hp = gethostbyname(**argvp)) == NULL) {
	syslog(LOG_WARNING, "unknown host: %s", **argvp);
	goto ret;
      }
      bcopy(hp->h_addr, (char *) &local, hp->h_length);
    }
  }
  
  /*
   * If colon last character, then no remote addr.
   */
  if (*colon == '\0')
    remote = 0l;
  else {
    if ((remote = inet_addr(colon)) == -1) {
      if ((hp = gethostbyname(colon)) == NULL) {
	syslog(LOG_WARNING,"unknown host: %s", colon);
	goto ret;
      }
      bcopy(hp->h_addr, (char *) &remote, hp->h_length);
    }
  }
  
  ipcp_wantoptions[0].neg_addrs = 1;
  ipcp_wantoptions[0].ouraddr = local;
  ipcp_wantoptions[0].hisaddr = remote;
  
 ret:
  --*argcp, ++*argvp;
  return (1);
}

static int
  setnetmask(argcp, argvp)
int *argcp;
char ***argvp;
{
  u_long mask;
	
  --*argcp, ++*argvp;
  if ((mask = inet_addr(**argvp)) == -1) {
    fprintf(stderr, "Invalid netmask %s\n", **argvp);
    exit(1);
  }

  netmask = mask;
  --*argcp, ++*argvp;
  return (1);
}

static int
  setcrtscts(argcp, argvp)
int *argcp;
char ***argvp;
{
  crtscts = 1;
  --*argcp, ++*argvp;
  return (1);
}

static int
  setnodetach(argcp, argvp)
int *argcp;
char ***argvp;
{
  nodetach = 1;
  --*argcp, ++*argvp;
  return (1);
}

/*
 * getuserpasswd - Get the user name and passwd.
 */
static void
  getuserpasswd()
{

  upap[0].us_user = user;
  upap[0].us_userlen = strlen(upap[0].us_user);

  upap[0].us_passwd = passwd;
  upap[0].us_passwdlen = strlen(upap[0].us_passwd);
}


/*
 * login - Check the user name and passwd and login the user.
 *
 * returns:
 *	UPAP_AUTHNAK: Login failed.
 *	UPAP_AUTHACK: Login succeeded.
 * In either case, msg points to an appropriate message.
 */
u_char
  login(user, userlen, passwd, passwdlen, msg, msglen)
char *user;
int userlen;
char *passwd;
int passwdlen;
char **msg;
int *msglen;
{
  struct passwd *pw;
  char *epasswd, *crypt();
  static int attempts = 0;
  char *tty, *rindex();
  char *tmp_passwd, *tmp_user;
  
  /* why alloca.h doesn't define what alloca() returns is a mystery */
  /* seems to be defined in stdlib.h for FreeBSD, rgrimes */
  
#ifdef sparc
  char *__builtin_alloca __ARGS((int));
#else
#ifndef __386BSD__
  char *alloca __ARGS((int));
#endif /* !__386BSD__ */
#endif /*sparc*/
  tmp_passwd = alloca(passwdlen + 1);	/* we best make copies before */
  /* null terminating the string */ 
  if (tmp_passwd == NULL) {
    syslog(LOG_ERR, "alloca failed");
    exit(1);
  }
  bcopy(passwd, tmp_passwd, passwdlen);
  tmp_passwd[passwdlen] = '\0';
  
  tmp_user = alloca(userlen + 1);
  if (tmp_user == NULL) {
    syslog(LOG_ERR, "alloca failed");
    exit(1);
  }
  bcopy(user, tmp_user, userlen);
  tmp_user[userlen] = '\0';
  
  if ((pw = getpwnam(tmp_user)) == NULL) {
    *msg = "Login incorrect";
    *msglen = strlen(*msg);
    syslog(LOG_WARNING, "upap login userid '%s' incorrect",tmp_user);
    return (UPAP_AUTHNAK);
  }
  
  /*
   * XXX If no passwd, let them login without one.
   */
  if (pw->pw_passwd == '\0') {
    *msg = "Login ok";
    *msglen = strlen(*msg);
    return (UPAP_AUTHACK);
  }
  
  epasswd = crypt(tmp_passwd, pw->pw_passwd);
  if (strcmp(epasswd, pw->pw_passwd)) {
    *msg = "Login incorrect";
    *msglen = strlen(*msg);
    syslog(LOG_WARNING, "upap login password '%s' incorrect", tmp_passwd);
    /*
     * Frustrate passwd stealer programs.
     * Allow 10 tries, but start backing off after 3 (stolen from login).
     * On 10'th, drop the connection.
     */
    if (attempts++ >= 10) {
      syslog(LOG_WARNING, "%d LOGIN FAILURES ON %s, %s",
	     attempts, devname, tmp_user);
      lcp_close(0);		/* Drop DTR? */
    }
    if (attempts > 3)
      sleep((u_int) (attempts - 3) * 5);
    return (UPAP_AUTHNAK);
  }
  
  attempts = 0;			/* Reset count */
  *msg = "Login ok";
  *msglen = strlen(*msg);
  syslog(LOG_NOTICE, "user %s logged in", tmp_user);
  tty = rindex(devname, '/');
  if (tty == NULL)
    tty = devname;
  else
    tty++;
  logwtmp(tty, tmp_user, "");		/* Add wtmp login entry */
  
  return (UPAP_AUTHACK);
}


/*
 * logout - Logout the user.
 */
void logout()
{
  char *tty;
  
  tty = rindex(devname, '/');
  if (tty == NULL)
    tty = devname;
  else
    tty++;
  logwtmp(tty, "", "");		/* Add wtmp logout entry */
}


/*
 * getuseropt - Get the options from /etc/hosts.ppp for this user.
 */
int getuseropt(user)
     char *user;
{
  char buf[1024], *s;
  FILE *fp;
  int rc = 0;
  
  if ((fp = fopen(PPPHOSTS, "r")) == NULL)
    return (0);;
  
  /*
   * Loop till we find an entry for this user.
   */
  for (;;) {
    if (fgets(buf, sizeof (buf), fp) == NULL) {
      if (feof(fp))
	break;
      else {
	syslog(LOG_ERR, "fgets");
	exit(1);
      }
    }
    if ((s = index(buf, ' ')) == NULL)
      continue;
    *s++ = '\0';
    if (!strcmp(user, buf)) {
      rc = 1;
      break;
    }
  }
  fclose(fp);
  return (rc);
}
/*
 * open "secret" file and return the secret matching the given name.
 * If no secret for a given name is found, use the one for "default".
 */

void
  get_secret(name, secret, secret_len)
u_char * name;
u_char * secret;
int * secret_len;
{
  FILE * sfile;
  struct stat sbuf;
  u_char fname[256];
  int match_found, default_found;

  match_found = FALSE;
  default_found = FALSE;

  if ((sfile = fopen(_PATH_CHAPFILE, "r")) == NULL) {
    syslog(LOG_ERR, "unable to open secret file %s", _PATH_CHAPFILE);
    exit(1);
  };
     
  if (fstat(fileno(sfile), &sbuf) < 0) {
    syslog(LOG_ERR, "cannot stat secret file!: %m");
    exit(1);
  }
  if ((sbuf.st_mode & 077) != 0)
    syslog(LOG_WARNING, "Warning - secret file has world and/or group access!");

  while (!feof(sfile) && !match_found) {
    if (fscanf(sfile, "%s %s", fname, secret) == EOF)
      break;
    if (!strcasecmp((char *)fname, (char *)name)) {
      match_found = TRUE;
    }
    if (!strcasecmp("default", (char *)name)) {
      default_found = TRUE;
    }
  }

  if (!match_found && !default_found)  {
    syslog(LOG_ERR, "No match or default entry found for %s in CHAP secret file! Aborting...", name);
    cleanup(0, NULL);		/* shut us down */
  }
#ifdef UNSECURE
/* while this is useful for debugging, it is a security hole as well */

    syslog(LOG_DEBUG, "get_secret: found secret %s", secret);
#endif /*UNSECURE*/
  fclose(sfile);
  *secret_len = strlen((char *)secret);
  if (*secret_len > MAX_SECRET_LEN) { /* don't let it overflow the buffer */
    syslog(LOG_ERR, "Length of secret for host %s is greater than the maximum %d characters! ", name, MAX_SECRET_LEN);
    cleanup(0, NULL);			/* scream and die */
  }
  return;
}
/*
 * Return user specified netmask. A value of zero means no netmask has
 * been set. 
 */
/* ARGSUSED */
u_long
  GetMask(addr)
u_long addr;
{
  return(netmask);
}

#ifdef	STREAMS
/* 
 * this module will attempt to reconstruct the stream with the
 * previously popped modules
 */

/*ARGSUSED*/
static void
  str_restore()
{
    /*EMPTY*/ 
    while(ioctl(fd, I_POP, 0) == 0); /* pop any we pushed */ 
  
    for(; str_module_count > 0; str_module_count--) {
	if(ioctl(fd, I_PUSH, str_modules[str_module_count-1].modname)) {
	    syslog(LOG_ERR, "str_restore: couldn't push module %s: %m",
		   str_modules[str_module_count-1].modname);
	}
	else {
	    MAINDEBUG((LOG_INFO, "str_restore: pushed module %s",
		       str_modules[str_module_count-1].modname))
	    }
    }
}
#endif

dumpbuffer(buffer, size, level)
unsigned char *buffer;
int size;
int level;
{
    register int i;
    char line[256], *p;

    printf("%d bytes:\n", size);
    while (size > 0)
    {
	p = line;
	sprintf(p, "%08lx: ", buffer);
	p += 10;
		
	for (i = 0; i < 8; i++, p += 3)
	    if (size - i <= 0)
		sprintf(p, "xx ");
	    else
		sprintf(p, "%02x ", buffer[i]);

	for (i = 0; i < 8; i++)
	    if (size - i <= 0)
		*p++ = 'x';
	    else
		*p++ = (' ' <= buffer[i] && buffer[i] <= '~') ?
		    buffer[i] : '.';

	*p++ = 0;
	buffer += 8;
	size -= 8;

/*	syslog(level, "%s\n", line); */
	printf("%s\n", line);
	fflush(stdout);
    }
}

#ifdef sun
setdtr(fd, on)
int fd, on;
{
	int linestate;

	ioctl(fd, TIOCMGET, &linestate);

	if (on)
		linestate |= TIOCM_DTR;
	else
		linestate &= ~TIOCM_DTR;

	ioctl(fd, TIOCMSET, &linestate);
}
#endif
#ifdef __386BSD__
setdtr(fd, on)
int fd, on;
{
    int modembits = TIOCM_DTR;

    if (on)
	ioctl(fd, TIOCMBIS, &modembits);
    else
	ioctl(fd, TIOCMBIC, &modembits);
}
#endif

char *
proto_name(proto)
u_short proto;
{
    switch (proto) {
      case LCP: return "lcp";
      case UPAP: return "pap";
      case CHAP: return "chap";
      case IPCP: return "ipcp";
#define LQM 0xc025
      case LQM: return "lqm";
    }
    return "<unknown>";
}

#include <varargs.h>

char line[256];
char *p;

logf(level, fmt, va_alist)
int level;
char *fmt;
va_dcl
{
    va_list pvar;
    char buf[256];

    va_start(pvar);
    vsprintf(buf, fmt, pvar);
    va_end(pvar);

    p = line + strlen(line);
    strcat(p, buf);

    if (buf[strlen(buf)-1] == '\n') {
	syslog(level, "%s", line);
	line[0] = 0;
    }
}
