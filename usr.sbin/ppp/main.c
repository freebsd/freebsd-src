/*
 *			User Process PPP
 *
 *	    Written by Toshiharu OHNO (tony-o@iij.ad.jp)
 *
 *   Copyright (C) 1993, Internet Initiative Japan, Inc. All rights reserverd.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the Internet Initiative Japan, Inc.  The name of the
 * IIJ may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * $Id: main.c,v 1.99 1997/11/16 22:15:05 brian Exp $
 *
 *	TODO:
 *		o Add commands for traffic summary, version display, etc.
 *		o Add signal handler for misc controls.
 */
#include <sys/param.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_tun.h>

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sysexits.h>
#include <termios.h>
#include <unistd.h>

#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "id.h"
#include "timer.h"
#include "fsm.h"
#include "modem.h"
#include "os.h"
#include "hdlc.h"
#include "ccp.h"
#include "lcp.h"
#include "ipcp.h"
#include "loadalias.h"
#include "command.h"
#include "vars.h"
#include "auth.h"
#include "filter.h"
#include "systems.h"
#include "ip.h"
#include "sig.h"
#include "server.h"
#include "lcpproto.h"
#include "main.h"
#include "vjcomp.h"
#include "async.h"
#include "pathnames.h"
#include "tun.h"

#ifndef O_NONBLOCK
#ifdef O_NDELAY
#define	O_NONBLOCK O_NDELAY
#endif
#endif

int TermMode = 0;
int tunno = 0;

static struct termios oldtio;	/* Original tty mode */
static struct termios comtio;	/* Command level tty mode */
static pid_t BGPid = 0;
static char pid_filename[MAXPATHLEN];
static int dial_up;

static void DoLoop(void);
static void TerminalStop(int);
static char *ex_desc(int);

static void
TtyInit(int DontWantInt)
{
  struct termios newtio;
  int stat;

  stat = fcntl(0, F_GETFL, 0);
  if (stat > 0) {
    stat |= O_NONBLOCK;
    (void) fcntl(0, F_SETFL, stat);
  }
  newtio = oldtio;
  newtio.c_lflag &= ~(ECHO | ISIG | ICANON);
  newtio.c_iflag = 0;
  newtio.c_oflag &= ~OPOST;
  newtio.c_cc[VEOF] = _POSIX_VDISABLE;
  if (DontWantInt)
    newtio.c_cc[VINTR] = _POSIX_VDISABLE;
  newtio.c_cc[VMIN] = 1;
  newtio.c_cc[VTIME] = 0;
  newtio.c_cflag |= CS8;
  tcsetattr(0, TCSADRAIN, &newtio);
  comtio = newtio;
}

/*
 *  Set tty into command mode. We allow canonical input and echo processing.
 */
void
TtyCommandMode(int prompt)
{
  struct termios newtio;
  int stat;

  if (!(mode & MODE_INTER))
    return;
  tcgetattr(0, &newtio);
  newtio.c_lflag |= (ECHO | ISIG | ICANON);
  newtio.c_iflag = oldtio.c_iflag;
  newtio.c_oflag |= OPOST;
  tcsetattr(0, TCSADRAIN, &newtio);
  stat = fcntl(0, F_GETFL, 0);
  if (stat > 0) {
    stat |= O_NONBLOCK;
    (void) fcntl(0, F_SETFL, stat);
  }
  TermMode = 0;
  if (prompt)
    Prompt();
}

/*
 * Set tty into terminal mode which is used while we invoke term command.
 */
void
TtyTermMode()
{
  int stat;

  tcsetattr(0, TCSADRAIN, &comtio);
  stat = fcntl(0, F_GETFL, 0);
  if (stat > 0) {
    stat &= ~O_NONBLOCK;
    (void) fcntl(0, F_SETFL, stat);
  }
  TermMode = 1;
}

void
TtyOldMode()
{
  int stat;

  stat = fcntl(0, F_GETFL, 0);
  if (stat > 0) {
    stat &= ~O_NONBLOCK;
    (void) fcntl(0, F_SETFL, stat);
  }
  tcsetattr(0, TCSANOW, &oldtio);
}

void
Cleanup(int excode)
{
  ServerClose();
  OsInterfaceDown(1);
  HangupModem(1);
  nointr_sleep(1);
  DeleteIfRoutes(1);
  ID0unlink(pid_filename);
  if (mode & MODE_BACKGROUND && BGFiledes[1] != -1) {
    char c = EX_ERRDEAD;

    if (write(BGFiledes[1], &c, 1) == 1)
      LogPrintf(LogPHASE, "Parent notified of failure.\n");
    else
      LogPrintf(LogPHASE, "Failed to notify parent of failure.\n");
    close(BGFiledes[1]);
  }
  LogPrintf(LogPHASE, "PPP Terminated (%s).\n", ex_desc(excode));
  TtyOldMode();
  LogClose();

  exit(excode);
}

static void
CloseConnection(int signo)
{
  /* NOTE, these are manual, we've done a setsid() */
  pending_signal(SIGINT, SIG_IGN);
  LogPrintf(LogPHASE, "Caught signal %d, abort connection\n", signo);
  reconnectState = RECON_FALSE;
  reconnectCount = 0;
  DownConnection();
  dial_up = 0;
  pending_signal(SIGINT, CloseConnection);
}

static void
CloseSession(int signo)
{
  if (BGPid) {
    kill(BGPid, SIGINT);
    exit(EX_TERM);
  }
  LogPrintf(LogPHASE, "Signal %d, terminate.\n", signo);
  reconnect(RECON_FALSE);
  LcpClose();
  Cleanup(EX_TERM);
}

static void
TerminalCont()
{
  pending_signal(SIGCONT, SIG_DFL);
  pending_signal(SIGTSTP, TerminalStop);
  TtyCommandMode(getpgrp() == tcgetpgrp(0));
}

static void
TerminalStop(int signo)
{
  pending_signal(SIGCONT, TerminalCont);
  TtyOldMode();
  pending_signal(SIGTSTP, SIG_DFL);
  kill(getpid(), signo);
}

static void
SetUpServer(int signo)
{
  int res;

  VarHaveLocalAuthKey = 0;
  LocalAuthInit();
  if ((res = ServerTcpOpen(SERVER_PORT + tunno)) != 0)
    LogPrintf(LogERROR, "SIGUSR1: Failed %d to open port %d\n",
	      res, SERVER_PORT + tunno);
}

static void
BringDownServer(int signo)
{
  VarHaveLocalAuthKey = 0;
  LocalAuthInit();
  ServerClose();
}

static char *
ex_desc(int ex)
{
  static char num[12];
  static char *desc[] = {"normal", "start", "sock",
    "modem", "dial", "dead", "done", "reboot", "errdead",
  "hangup", "term", "nodial", "nologin"};

  if (ex >= 0 && ex < sizeof(desc) / sizeof(*desc))
    return desc[ex];
  snprintf(num, sizeof num, "%d", ex);
  return num;
}

static void
Usage()
{
  fprintf(stderr,
	  "Usage: ppp [-auto | -background | -direct | -dedicated | -ddial ] [ -alias ] [system]\n");
  exit(EX_START);
}

static void
ProcessArgs(int argc, char **argv)
{
  int optc;
  char *cp;

  optc = 0;
  mode = MODE_INTER;
  while (argc > 0 && **argv == '-') {
    cp = *argv + 1;
    if (strcmp(cp, "auto") == 0) {
      mode |= MODE_AUTO;
      mode &= ~MODE_INTER;
    } else if (strcmp(cp, "background") == 0) {
      mode |= MODE_BACKGROUND;
      mode &= ~MODE_INTER;
    } else if (strcmp(cp, "direct") == 0) {
      mode |= MODE_DIRECT;
      mode &= ~MODE_INTER;
    } else if (strcmp(cp, "dedicated") == 0) {
      mode |= MODE_DEDICATED;
      mode &= ~MODE_INTER;
    } else if (strcmp(cp, "ddial") == 0) {
      mode |= MODE_DDIAL;
      mode &= ~MODE_INTER;
    } else if (strcmp(cp, "alias") == 0) {
      if (loadAliasHandlers(&VarAliasHandlers) == 0)
	mode |= MODE_ALIAS;
      else
	LogPrintf(LogWARN, "Cannot load alias library\n");
      optc--;			/* this option isn't exclusive */
    } else
      Usage();
    optc++;
    argv++;
    argc--;
  }
  if (argc > 1) {
    fprintf(stderr, "specify only one system label.\n");
    exit(EX_START);
  }
  if (argc == 1)
    SetLabel(*argv);

  if (optc > 1) {
    fprintf(stderr, "specify only one mode.\n");
    exit(EX_START);
  }
}

static void
Greetings()
{
  if (VarTerm) {
    fprintf(VarTerm, "User Process PPP. Written by Toshiharu OHNO.\n");
    fflush(VarTerm);
  }
}

int
main(int argc, char **argv)
{
  FILE *lockfile;
  char *name;

  VarTerm = 0;
  name = strrchr(argv[0], '/');
  LogOpen(name ? name + 1 : argv[0]);

  argc--;
  argv++;
  ProcessArgs(argc, argv);
  if (!(mode & MODE_DIRECT))
    VarTerm = stdout;

  ID0init();
  if (ID0realuid() != 0) {
    char conf[200], *ptr;

    snprintf(conf, sizeof conf, "%s/%s", _PATH_PPP, CONFFILE);
    do {
      if (!access(conf, W_OK)) {
        LogPrintf(LogALERT, "ppp: Access violation: Please protect %s\n", conf);
        return -1;
      }
      ptr = conf + strlen(conf)-2;
      while (ptr > conf && *ptr != '/')
        *ptr-- = '\0';
    } while (ptr >= conf);
  }

  if (!ValidSystem(GetLabel())) {
    fprintf(stderr, "You may not use ppp in this mode with this label\n");
    if (mode & MODE_DIRECT) {
      const char *l;
      if ((l = GetLabel()) == NULL)
        l = "default";
      VarTerm = 0;
      LogPrintf(LogWARN, "Label %s rejected -direct connection\n", l);
    }
    LogClose();
    return 1;
  }

  if (!GetShortHost())
    return 1;
  Greetings();
  IpcpDefAddress();

  if (SelectSystem("default", CONFFILE) < 0 && VarTerm)
    fprintf(VarTerm, "Warning: No default entry is given in config file.\n");

  if (OpenTunnel(&tunno) < 0) {
    LogPrintf(LogWARN, "open_tun: %s\n", strerror(errno));
    return EX_START;
  }
  if (mode & MODE_INTER) {
    fprintf(VarTerm, "Interactive mode\n");
    netfd = STDOUT_FILENO;
  } else if ((mode & MODE_OUTGOING_DAEMON) && !(mode & MODE_DEDICATED))
    if (GetLabel() == NULL) {
      if (VarTerm)
	fprintf(VarTerm, "Destination system must be specified in"
		" auto, background or ddial mode.\n");
      return EX_START;
    }

  tcgetattr(0, &oldtio);	/* Save original tty mode */

  pending_signal(SIGHUP, CloseSession);
  pending_signal(SIGTERM, CloseSession);
  pending_signal(SIGINT, CloseConnection);
  pending_signal(SIGQUIT, CloseSession);
#ifdef SIGPIPE
  signal(SIGPIPE, SIG_IGN);
#endif
#ifdef SIGALRM
  pending_signal(SIGALRM, SIG_IGN);
#endif
  if (mode & MODE_INTER) {
#ifdef SIGTSTP
    pending_signal(SIGTSTP, TerminalStop);
#endif
#ifdef SIGTTIN
    pending_signal(SIGTTIN, TerminalStop);
#endif
#ifdef SIGTTOU
    pending_signal(SIGTTOU, SIG_IGN);
#endif
  }
  if (!(mode & MODE_INTER)) {
#ifdef SIGUSR1
    pending_signal(SIGUSR1, SetUpServer);
#endif
#ifdef SIGUSR2
    pending_signal(SIGUSR2, BringDownServer);
#endif
  }

  if (GetLabel()) {
    if (SelectSystem(GetLabel(), CONFFILE) < 0) {
      LogPrintf(LogWARN, "Destination system %s not found in conf file.\n",
                GetLabel());
      Cleanup(EX_START);
    }
    if (mode & MODE_OUTGOING_DAEMON &&
	DefHisAddress.ipaddr.s_addr == INADDR_ANY) {
      LogPrintf(LogWARN, "You must \"set ifaddr\" in label %s for"
		" auto, background or ddial mode.\n", GetLabel());
      Cleanup(EX_START);
    }
  }

  if (mode & MODE_DAEMON) {
    if (mode & MODE_BACKGROUND) {
      if (pipe(BGFiledes)) {
	LogPrintf(LogERROR, "pipe: %s\n", strerror(errno));
	Cleanup(EX_SOCK);
      }
    }

    if (!(mode & MODE_DIRECT)) {
      pid_t bgpid;

      bgpid = fork();
      if (bgpid == -1) {
	LogPrintf(LogERROR, "fork: %s\n", strerror(errno));
	Cleanup(EX_SOCK);
      }
      if (bgpid) {
	char c = EX_NORMAL;

	if (mode & MODE_BACKGROUND) {
	  /* Wait for our child to close its pipe before we exit. */
	  BGPid = bgpid;
	  close(BGFiledes[1]);
	  if (read(BGFiledes[0], &c, 1) != 1) {
	    fprintf(VarTerm, "Child exit, no status.\n");
	    LogPrintf(LogPHASE, "Parent: Child exit, no status.\n");
	  } else if (c == EX_NORMAL) {
	    fprintf(VarTerm, "PPP enabled.\n");
	    LogPrintf(LogPHASE, "Parent: PPP enabled.\n");
	  } else {
	    fprintf(VarTerm, "Child failed (%s).\n", ex_desc((int) c));
	    LogPrintf(LogPHASE, "Parent: Child failed (%s).\n",
		      ex_desc((int) c));
	  }
	  close(BGFiledes[0]);
	}
	return c;
      } else if (mode & MODE_BACKGROUND)
	close(BGFiledes[0]);
    }

    VarTerm = 0;		/* We know it's currently stdout */
    close(1);
    close(2);

    if (mode & MODE_DIRECT)
      TtyInit(1);
    else if (mode & MODE_DAEMON) {
      setsid();
      close(0);
    }
  } else {
    TtyInit(0);
    TtyCommandMode(1);
  }

  snprintf(pid_filename, sizeof(pid_filename), "%stun%d.pid",
           _PATH_VARRUN, tunno);
  lockfile = ID0fopen(pid_filename, "w");
  if (lockfile != NULL) {
    fprintf(lockfile, "%d\n", (int) getpid());
    fclose(lockfile);
  } else
    LogPrintf(LogALERT, "Warning: Can't create %s: %s\n",
              pid_filename, strerror(errno));

  LogPrintf(LogPHASE, "PPP Started.\n");


  do
    DoLoop();
  while (mode & MODE_DEDICATED);

  Cleanup(EX_DONE);
  return 0;
}

/*
 *  Turn into packet mode, where we speak PPP.
 */
void
PacketMode()
{
  if (RawModem() < 0) {
    LogPrintf(LogWARN, "PacketMode: Not connected.\n");
    return;
  }
  AsyncInit();
  VjInit(15);
  LcpInit();
  IpcpInit();
  CcpInit();
  LcpUp();

  LcpOpen(VarOpenMode);
  if (mode & MODE_INTER)
    TtyCommandMode(1);
  if (VarTerm) {
    fprintf(VarTerm, "Packet mode.\n");
    aft_cmd = 1;
  }
}

static void
ShowHelp()
{
  fprintf(stderr, "The following commands are available:\r\n");
  fprintf(stderr, " ~p\tEnter Packet mode\r\n");
  fprintf(stderr, " ~-\tDecrease log level\r\n");
  fprintf(stderr, " ~+\tIncrease log level\r\n");
  fprintf(stderr, " ~t\tShow timers (only in \"log debug\" mode)\r\n");
  fprintf(stderr, " ~m\tShow memory map (only in \"log debug\" mode)\r\n");
  fprintf(stderr, " ~.\tTerminate program\r\n");
  fprintf(stderr, " ~?\tThis help\r\n");
}

static void
ReadTty()
{
  int n;
  char ch;
  static int ttystate;
  FILE *oVarTerm;
  char linebuff[LINE_LEN];

  LogPrintf(LogDEBUG, "termode = %d, netfd = %d, mode = %d\n",
	    TermMode, netfd, mode);
  if (!TermMode) {
    n = read(netfd, linebuff, sizeof(linebuff) - 1);
    if (n > 0) {
      aft_cmd = 1;
      if (linebuff[n-1] == '\n')
        linebuff[--n] = '\0';
      if (n)
        DecodeCommand(linebuff, n, IsInteractive(0) ? NULL : "Client");
      Prompt();
    } else {
      LogPrintf(LogPHASE, "client connection closed.\n");
      oVarTerm = VarTerm;
      VarTerm = 0;
      if (oVarTerm && oVarTerm != stdout)
	fclose(oVarTerm);
      close(netfd);
      netfd = -1;
    }
    return;
  }

  /*
   * We are in terminal mode, decode special sequences
   */
  n = read(fileno(VarTerm), &ch, 1);
  LogPrintf(LogDEBUG, "Got %d bytes (reading from the terminal)\n", n);

  if (n > 0) {
    switch (ttystate) {
    case 0:
      if (ch == '~')
	ttystate++;
      else
	write(modem, &ch, n);
      break;
    case 1:
      switch (ch) {
      case '?':
	ShowHelp();
	break;
      case 'p':

	/*
	 * XXX: Should check carrier.
	 */
	if (LcpFsm.state <= ST_CLOSED) {
	  VarOpenMode = OPEN_ACTIVE;
	  PacketMode();
	}
	break;
      case '.':
	TermMode = 1;
	aft_cmd = 1;
	TtyCommandMode(1);
	break;
      case 't':
	if (LogIsKept(LogDEBUG)) {
	  ShowTimers();
	  break;
	}
      case 'm':
	if (LogIsKept(LogDEBUG)) {
	  ShowMemMap();
	  break;
	}
      default:
	if (write(modem, &ch, n) < 0)
	  LogPrintf(LogERROR, "error writing to modem.\n");
	break;
      }
      ttystate = 0;
      break;
    }
  }
}


/*
 *  Here, we'll try to detect HDLC frame
 */

static char *FrameHeaders[] = {
  "\176\377\003\300\041",
  "\176\377\175\043\300\041",
  "\176\177\175\043\100\041",
  "\176\175\337\175\043\300\041",
  "\176\175\137\175\043\100\041",
  NULL,
};

static u_char *
HdlcDetect(u_char * cp, int n)
{
  char *ptr, *fp, **hp;

  cp[n] = '\0';			/* be sure to null terminated */
  ptr = NULL;
  for (hp = FrameHeaders; *hp; hp++) {
    fp = *hp;
    if (DEV_IS_SYNC)
      fp++;
    ptr = strstr((char *) cp, fp);
    if (ptr)
      break;
  }
  return ((u_char *) ptr);
}

static struct pppTimer RedialTimer;

static void
RedialTimeout()
{
  StopTimer(&RedialTimer);
  LogPrintf(LogPHASE, "Redialing timer expired.\n");
}

static void
StartRedialTimer(int Timeout)
{
  StopTimer(&RedialTimer);

  if (Timeout) {
    RedialTimer.state = TIMER_STOPPED;

    if (Timeout > 0)
      RedialTimer.load = Timeout * SECTICKS;
    else
      RedialTimer.load = (random() % REDIAL_PERIOD) * SECTICKS;

    LogPrintf(LogPHASE, "Enter pause (%d) for redialing.\n",
	      RedialTimer.load / SECTICKS);

    RedialTimer.func = RedialTimeout;
    StartTimer(&RedialTimer);
  }
}


static void
DoLoop()
{
  fd_set rfds, wfds, efds;
  int pri, i, n, wfd, nfds;
  struct sockaddr_in hisaddr;
  struct timeval timeout, *tp;
  int ssize = sizeof(hisaddr);
  u_char *cp;
  int tries;
  int qlen;
  int res;
  pid_t pgroup;
  struct tun_data tun;
#define rbuff tun.data

  pgroup = getpgrp();

  if (mode & MODE_DIRECT) {
    LogPrintf(LogDEBUG, "Opening modem\n");
    if (OpenModem() < 0)
      return;
    LogPrintf(LogPHASE, "Packet mode enabled\n");
    PacketMode();
  } else if (mode & MODE_DEDICATED) {
    if (modem < 0)
      while (OpenModem() < 0)
	nointr_sleep(VarReconnectTimer);
  }
  fflush(VarTerm);

  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
  reconnectState = RECON_UNKNOWN;

  if (mode & MODE_BACKGROUND)
    dial_up = 1;		/* Bring the line up */
  else
    dial_up = 0;		/* XXXX */
  tries = 0;
  for (;;) {
    nfds = 0;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_ZERO(&efds);

    /*
     * If the link is down and we're in DDIAL mode, bring it back up.
     */
    if (mode & MODE_DDIAL && LcpFsm.state <= ST_CLOSED)
      dial_up = 1;

    /*
     * If we lost carrier and want to re-establish the connection due to the
     * "set reconnect" value, we'd better bring the line back up.
     */
    if (LcpFsm.state <= ST_CLOSED) {
      if (!dial_up && reconnectState == RECON_TRUE) {
	if (++reconnectCount <= VarReconnectTries) {
	  LogPrintf(LogPHASE, "Connection lost, re-establish (%d/%d)\n",
		    reconnectCount, VarReconnectTries);
	  StartRedialTimer(VarReconnectTimer);
	  dial_up = 1;
	} else {
	  if (VarReconnectTries)
	    LogPrintf(LogPHASE, "Connection lost, maximum (%d) times\n",
		      VarReconnectTries);
	  reconnectCount = 0;
	  if (mode & MODE_BACKGROUND)
	    Cleanup(EX_DEAD);
	}
	reconnectState = RECON_ENVOKED;
      } else if (mode & MODE_DEDICATED)
        if (VarOpenMode == OPEN_ACTIVE)
          PacketMode();
    }

    /*
     * If Ip packet for output is enqueued and require dial up, Just do it!
     */
    if (dial_up && RedialTimer.state != TIMER_RUNNING) {
      LogPrintf(LogDEBUG, "going to dial: modem = %d\n", modem);
      if (OpenModem() < 0) {
	tries++;
	if (!(mode & MODE_DDIAL) && VarDialTries)
	  LogPrintf(LogCHAT, "Failed to open modem (attempt %u of %d)\n",
		    tries, VarDialTries);
	else
	  LogPrintf(LogCHAT, "Failed to open modem (attempt %u)\n", tries);

	if (!(mode & MODE_DDIAL) && VarDialTries && tries >= VarDialTries) {
	  if (mode & MODE_BACKGROUND)
	    Cleanup(EX_DIAL);	/* Can't get the modem */
	  dial_up = 0;
	  reconnectState = RECON_UNKNOWN;
	  reconnectCount = 0;
	  tries = 0;
	} else
	  StartRedialTimer(VarRedialTimeout);
      } else {
	tries++;		/* Tries are per number, not per list of
				 * numbers. */
	if (!(mode & MODE_DDIAL) && VarDialTries)
	  LogPrintf(LogCHAT, "Dial attempt %u of %d\n", tries, VarDialTries);
	else
	  LogPrintf(LogCHAT, "Dial attempt %u\n", tries);

	if ((res = DialModem()) == EX_DONE) {
	  nointr_sleep(1);		/* little pause to allow peer starts */
	  ModemTimeout();
	  PacketMode();
	  dial_up = 0;
	  reconnectState = RECON_UNKNOWN;
	  tries = 0;
	} else {
	  if (mode & MODE_BACKGROUND) {
	    if (VarNextPhone == NULL || res == EX_SIG)
	      Cleanup(EX_DIAL);	/* Tried all numbers - no luck */
	    else
	      /* Try all numbers in background mode */
	      StartRedialTimer(VarRedialNextTimeout);
	  } else if (!(mode & MODE_DDIAL) &&
		     ((VarDialTries && tries >= VarDialTries) ||
		      res == EX_SIG)) {
	    /* I give up !  Can't get through :( */
	    StartRedialTimer(VarRedialTimeout);
	    dial_up = 0;
	    reconnectState = RECON_UNKNOWN;
	    reconnectCount = 0;
	    tries = 0;
	  } else if (VarNextPhone == NULL)
	    /* Dial failed. Keep quite during redial wait period. */
	    StartRedialTimer(VarRedialTimeout);
	  else
	    StartRedialTimer(VarRedialNextTimeout);
	}
      }
    }
    qlen = ModemQlen();

    if (qlen == 0) {
      IpStartOutput();
      qlen = ModemQlen();
    }
    if (modem >= 0) {
      if (modem + 1 > nfds)
	nfds = modem + 1;
      FD_SET(modem, &rfds);
      FD_SET(modem, &efds);
      if (qlen > 0) {
	FD_SET(modem, &wfds);
      }
    }
    if (server >= 0) {
      if (server + 1 > nfds)
	nfds = server + 1;
      FD_SET(server, &rfds);
    }

    /*
     * *** IMPORTANT ***
     * 
     * CPU is serviced every TICKUNIT micro seconds. This value must be chosen
     * with great care. If this values is too big, it results loss of
     * characters from modem and poor responce. If this values is too small,
     * ppp process eats many CPU time.
     */
#ifndef SIGALRM
    nointr_usleep(TICKUNIT);
    TimerService();
#else
    handle_signals();
#endif

    /* If there are aren't many packets queued, look for some more. */
    if (qlen < 20 && tun_in >= 0) {
      if (tun_in + 1 > nfds)
	nfds = tun_in + 1;
      FD_SET(tun_in, &rfds);
    }
    if (netfd >= 0) {
      if (netfd + 1 > nfds)
	nfds = netfd + 1;
      FD_SET(netfd, &rfds);
      FD_SET(netfd, &efds);
    }
#ifndef SIGALRM

    /*
     * Normally, select() will not block because modem is writable. In AUTO
     * mode, select will block until we find packet from tun
     */
    tp = (RedialTimer.state == TIMER_RUNNING) ? &timeout : NULL;
    i = select(nfds, &rfds, &wfds, &efds, tp);
#else

    /*
     * When SIGALRM timer is running, a select function will be return -1 and
     * EINTR after a Time Service signal hundler is done.  If the redial
     * timer is not running and we are trying to dial, poll with a 0 value
     * timer.
     */
    tp = (dial_up && RedialTimer.state != TIMER_RUNNING) ? &timeout : NULL;
    i = select(nfds, &rfds, &wfds, &efds, tp);
#endif

    if (i == 0) {
      continue;
    }
    if (i < 0) {
      if (errno == EINTR) {
	handle_signals();
	continue;
      }
      LogPrintf(LogERROR, "DoLoop: select(): %s\n", strerror(errno));
      break;
    }
    if ((netfd >= 0 && FD_ISSET(netfd, &efds)) || (modem >= 0 && FD_ISSET(modem, &efds))) {
      LogPrintf(LogALERT, "Exception detected.\n");
      break;
    }
    if (server >= 0 && FD_ISSET(server, &rfds)) {
      LogPrintf(LogPHASE, "connected to client.\n");
      wfd = accept(server, (struct sockaddr *) & hisaddr, &ssize);
      if (wfd < 0) {
	LogPrintf(LogERROR, "DoLoop: accept(): %s\n", strerror(errno));
	continue;
      }
      if (netfd >= 0) {
	write(wfd, "already in use.\n", 16);
	close(wfd);
	continue;
      } else
	netfd = wfd;
      VarTerm = fdopen(netfd, "a+");
      LocalAuthInit();
      Greetings();
      IsInteractive(1);
      Prompt();
    }
    if (netfd >= 0 && FD_ISSET(netfd, &rfds) &&
	((mode & MODE_OUTGOING_DAEMON) || pgroup == tcgetpgrp(0))) {
      /* something to read from tty */
      ReadTty();
    }
    if (modem >= 0) {
      if (FD_ISSET(modem, &wfds)) {	/* ready to write into modem */
	ModemStartOutput(modem);
      }
      if (FD_ISSET(modem, &rfds)) {	/* something to read from modem */
	if (LcpFsm.state <= ST_CLOSED)
	  nointr_usleep(10000);
	n = read(modem, rbuff, sizeof(rbuff));
	if ((mode & MODE_DIRECT) && n <= 0) {
	  DownConnection();
	} else
	  LogDumpBuff(LogASYNC, "ReadFromModem", rbuff, n);

	if (LcpFsm.state <= ST_CLOSED) {

	  /*
	   * In dedicated mode, we just discard input until LCP is started.
	   */
	  if (!(mode & MODE_DEDICATED)) {
	    cp = HdlcDetect(rbuff, n);
	    if (cp) {

	      /*
	       * LCP packet is detected. Turn ourselves into packet mode.
	       */
	      if (cp != rbuff) {
		write(modem, rbuff, cp - rbuff);
		write(modem, "\r\n", 2);
	      }
	      PacketMode();
	    } else
	      write(fileno(VarTerm), rbuff, n);
	  }
	} else {
	  if (n > 0)
	    AsyncInput(rbuff, n);
	}
      }
    }
    if (tun_in >= 0 && FD_ISSET(tun_in, &rfds)) {	/* something to read
							 * from tun */
      n = read(tun_in, &tun, sizeof(tun));
      if (n < 0) {
	LogPrintf(LogERROR, "read from tun: %s\n", strerror(errno));
	continue;
      }
      n -= sizeof(tun)-sizeof(tun.data);
      if (n <= 0) {
	LogPrintf(LogERROR, "read from tun: Only %d bytes read\n", n);
	continue;
      }
      if (!tun_check_header(tun, AF_INET))
          continue;
      if (((struct ip *) rbuff)->ip_dst.s_addr == IpcpInfo.want_ipaddr.s_addr) {
	/* we've been asked to send something addressed *to* us :( */
	if (VarLoopback) {
	  pri = PacketCheck(rbuff, n, FL_IN);
	  if (pri >= 0) {
	    struct mbuf *bp;

	    if (mode & MODE_ALIAS) {
	      VarPacketAliasIn(rbuff, sizeof rbuff);
	      n = ntohs(((struct ip *) rbuff)->ip_len);
	    }
	    bp = mballoc(n, MB_IPIN);
	    memcpy(MBUF_CTOP(bp), rbuff, n);
	    IpInput(bp);
	    LogPrintf(LogDEBUG, "Looped back packet addressed to myself\n");
	  }
	  continue;
	} else
	  LogPrintf(LogDEBUG, "Oops - forwarding packet addressed to myself\n");
      }

      /*
       * Process on-demand dialup. Output packets are queued within tunnel
       * device until IPCP is opened.
       */
      if (LcpFsm.state <= ST_CLOSED && (mode & MODE_AUTO)) {
	pri = PacketCheck(rbuff, n, FL_DIAL);
	if (pri >= 0) {
	  if (mode & MODE_ALIAS) {
	    VarPacketAliasOut(rbuff, sizeof rbuff);
	    n = ntohs(((struct ip *) rbuff)->ip_len);
	  }
	  IpEnqueue(pri, rbuff, n);
	  dial_up = 1;	/* XXX */
	}
	continue;
      }
      pri = PacketCheck(rbuff, n, FL_OUT);
      if (pri >= 0) {
	if (mode & MODE_ALIAS) {
	  VarPacketAliasOut(rbuff, sizeof rbuff);
	  n = ntohs(((struct ip *) rbuff)->ip_len);
	}
	IpEnqueue(pri, rbuff, n);
      }
    }
  }
  LogPrintf(LogDEBUG, "Job (DoLoop) done.\n");
}
