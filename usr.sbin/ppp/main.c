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
 * $Id: main.c,v 1.121.2.20 1998/02/10 22:28:51 brian Exp $
 *
 *	TODO:
 *		o Add commands for traffic summary, version display, etc.
 *		o Add signal handler for misc controls.
 */
#include <sys/param.h>
#include <sys/time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/if_tun.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "id.h"
#include "timer.h"
#include "fsm.h"
#include "modem.h"
#include "bundle.h"
#include "hdlc.h"
#include "lcp.h"
#include "ccp.h"
#include "iplist.h"
#include "throughput.h"
#include "ipcp.h"
#include "loadalias.h"
#include "vars.h"
#include "auth.h"
#include "filter.h"
#include "systems.h"
#include "ip.h"
#include "sig.h"
#include "main.h"
#include "vjcomp.h"
#include "async.h"
#include "pathnames.h"
#include "tun.h"
#include "route.h"
#include "link.h"
#include "descriptor.h"
#include "physical.h"
#include "server.h"
#include "prompt.h"
#include "chat.h"

#ifndef O_NONBLOCK
#ifdef O_NDELAY
#define	O_NONBLOCK O_NDELAY
#endif
#endif

static pid_t BGPid = 0;
static char pid_filename[MAXPATHLEN];
int dial_up;
int dialing;

static void DoLoop(struct bundle *);
static void TerminalStop(int);
static const char *ex_desc(int);

static struct bundle *SignalBundle;
int CleaningUp;

void
Cleanup(int excode)
{
  CleaningUp = 1;
  reconnect(RECON_FALSE);
  if (bundle_Phase(SignalBundle) != PHASE_DEAD) {
    bundle_Close(SignalBundle, NULL);
    return;
  }
  AbortProgram(excode);
}

void
AbortProgram(int excode)
{
  prompt_Drop(&prompt, 1);
  ServerClose();
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
  prompt_TtyOldMode(&prompt);
  link_Destroy(physical2link(SignalBundle->physical));
  LogClose();
  bundle_Destroy(SignalBundle);
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
  link_Close(&SignalBundle->physical->link, SignalBundle, 0);
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
  Cleanup(EX_TERM);
}

static void
TerminalCont(int signo)
{
  pending_signal(SIGCONT, SIG_DFL);
  pending_signal(SIGTSTP, TerminalStop);
  prompt_TtyCommandMode(&prompt);
  if (getpgrp() == prompt_pgrp(&prompt))
    prompt_Display(&prompt, SignalBundle);
}

static void
TerminalStop(int signo)
{
  pending_signal(SIGCONT, TerminalCont);
  prompt_TtyOldMode(&prompt);
  pending_signal(SIGTSTP, SIG_DFL);
  kill(getpid(), signo);
}

static void
SetUpServer(int signo)
{
  int res;

  VarHaveLocalAuthKey = 0;
  LocalAuthInit();
  if ((res = ServerTcpOpen(SERVER_PORT + SignalBundle->unit)) != 0)
    LogPrintf(LogERROR, "SIGUSR1: Failed %d to open port %d\n",
	      res, SERVER_PORT + SignalBundle->unit);
}

static void
BringDownServer(int signo)
{
  VarHaveLocalAuthKey = 0;
  LocalAuthInit();
  ServerClose();
}

static const char *
ex_desc(int ex)
{
  static char num[12];
  static const char *desc[] = {
    "normal", "start", "sock", "modem", "dial", "dead", "done",
    "reboot", "errdead", "hangup", "term", "nodial", "nologin"
  };

  if (ex >= 0 && ex < sizeof desc / sizeof *desc)
    return desc[ex];
  snprintf(num, sizeof num, "%d", ex);
  return num;
}

static void
Usage(void)
{
  fprintf(stderr,
	  "Usage: ppp [-auto | -background | -direct | -dedicated | -ddial ]"
#ifndef NOALIAS
          " [ -alias ]"
#endif
          " [system]\n");
  exit(EX_START);
}

static char *
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
#ifndef NOALIAS
    } else if (strcmp(cp, "alias") == 0) {
      if (loadAliasHandlers(&VarAliasHandlers) == 0)
	mode |= MODE_ALIAS;
      else
	LogPrintf(LogWARN, "Cannot load alias library\n");
      optc--;			/* this option isn't exclusive */
#endif
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

  if (optc > 1) {
    fprintf(stderr, "specify only one mode.\n");
    exit(EX_START);
  }

  return argc == 1 ? *argv : NULL;	/* Don't SetLabel yet ! */
}

int
main(int argc, char **argv)
{
  FILE *lockfile;
  char *name, *label;
  int nfds;
  struct bundle *bundle;

  nfds = getdtablesize();
  if (nfds >= FD_SETSIZE)
    /*
     * If we've got loads of file descriptors, make sure they're all
     * closed.  If they aren't, we may end up with a seg fault when our
     * `fd_set's get too big when select()ing !
     */
    while (--nfds > 2)
      close(nfds);

  name = strrchr(argv[0], '/');
  LogOpen(name ? name + 1 : argv[0]);

  argc--;
  argv++;
  label = ProcessArgs(argc, argv);

#ifdef __FreeBSD__
  /*
   * A FreeBSD hack to dodge a bug in the tty driver that drops output
   * occasionally.... I must find the real reason some time.  To display
   * the dodgy behaviour, comment out this bit, make yourself a large
   * routing table and then run ppp in interactive mode.  The `show route'
   * command will drop chunks of data !!!
   */
  if (mode & MODE_INTER) {
    close(STDIN_FILENO);
    if (open(_PATH_TTY, O_RDONLY) != STDIN_FILENO) {
      fprintf(stderr, "Cannot open %s for input !\n", _PATH_TTY);
      return 2;
    }
  }
#endif

  prompt_Init(&prompt, (mode & MODE_DIRECT) ? PROMPT_NONE : PROMPT_STD);

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

  if (!ValidSystem(label)) {
    fprintf(stderr, "You may not use ppp in this mode with this label\n");
    if (mode & MODE_DIRECT) {
      const char *l;
      l = label ? label : "default";
      LogPrintf(LogWARN, "Label %s rejected -direct connection\n", l);
    }
    LogClose();
    return 1;
  }

  if (mode & MODE_INTER)
    VarLocalAuth = LOCAL_AUTH;

  if ((bundle = bundle_Create("/dev/tun")) == NULL) {
    LogPrintf(LogWARN, "bundle_Create: %s\n", strerror(errno));
    return EX_START;
  }

  if (!GetShortHost())
    return 1;
  IsInteractive(1);

  SignalBundle = bundle;

  if (SelectSystem(bundle, "default", CONFFILE) < 0)
    prompt_Printf(&prompt,
                  "Warning: No default entry is given in config file.\n");

  if ((mode & MODE_OUTGOING_DAEMON) && !(mode & MODE_DEDICATED))
    if (label == NULL) {
      prompt_Printf(&prompt, "Destination system must be specified in"
		    " auto, background or ddial mode.\n");
      return EX_START;
    }

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

  if (label) {
    if (SelectSystem(bundle, label, CONFFILE) < 0) {
      LogPrintf(LogWARN, "Destination system %s not found in conf file.\n",
                GetLabel());
      Cleanup(EX_START);
    }
    /*
     * We don't SetLabel() 'till now in case SelectSystem() has an
     * embeded load "otherlabel" command.
     */
    SetLabel(label);
    if (mode & MODE_AUTO &&
	IpcpInfo.DefHisAddress.ipaddr.s_addr == INADDR_ANY) {
      LogPrintf(LogWARN, "You must \"set ifaddr\" in label %s for auto mode.\n",
		label);
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
	    prompt_Printf(&prompt, "Child exit, no status.\n");
	    LogPrintf(LogPHASE, "Parent: Child exit, no status.\n");
	  } else if (c == EX_NORMAL) {
	    prompt_Printf(&prompt, "PPP enabled.\n");
	    LogPrintf(LogPHASE, "Parent: PPP enabled.\n");
	  } else {
	    prompt_Printf(&prompt, "Child failed (%s).\n", ex_desc((int) c));
	    LogPrintf(LogPHASE, "Parent: Child failed (%s).\n",
		      ex_desc((int) c));
	  }
	  close(BGFiledes[0]);
	}
	return c;
      } else if (mode & MODE_BACKGROUND)
	close(BGFiledes[0]);
    }

    prompt_Init(&prompt, PROMPT_NONE);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    if (mode & MODE_DIRECT)
      /* STDIN_FILENO gets used by modem_Open in DIRECT mode */
      prompt_TtyInit(&prompt, PROMPT_DONT_WANT_INT);
    else if (mode & MODE_DAEMON) {
      setsid();
      close(STDIN_FILENO);
    }
  } else {
    close(STDERR_FILENO);
    prompt_TtyInit(&prompt, PROMPT_WANT_INT);
    prompt_TtyCommandMode(&prompt);
    prompt_Display(&prompt, bundle);
  }

  snprintf(pid_filename, sizeof pid_filename, "%stun%d.pid",
           _PATH_VARRUN, bundle->unit);
  lockfile = ID0fopen(pid_filename, "w");
  if (lockfile != NULL) {
    fprintf(lockfile, "%d\n", (int) getpid());
    fclose(lockfile);
  }
#ifndef RELEASE_CRUNCH
  else
    LogPrintf(LogALERT, "Warning: Can't create %s: %s\n",
              pid_filename, strerror(errno));
#endif

  LogPrintf(LogPHASE, "PPP Started.\n");


  do
    DoLoop(bundle);
  while (mode & MODE_DEDICATED);

  AbortProgram(EX_NORMAL);

  return EX_NORMAL;
}

/*
 *  Turn into packet mode, where we speak PPP.
 */
void
PacketMode(struct bundle *bundle, int delay)
{
  if (modem_Raw(bundle->physical) < 0) {
    LogPrintf(LogWARN, "PacketMode: Not connected.\n");
    return;
  }
  LcpInit(bundle, bundle->physical);
  IpcpInit(bundle, physical2link(bundle->physical));
  CcpInit(bundle, physical2link(bundle->physical));
  LcpUp();

  LcpOpen(delay);
  prompt_TtyCommandMode(&prompt);
  prompt_Printf(&prompt, "Packet mode.\n");
}

static struct pppTimer RedialTimer;

static void
RedialTimeout(void *v)
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
DoLoop(struct bundle *bundle)
{
  fd_set rfds, wfds, efds;
  int pri, i, n, wfd, nfds;
  struct timeval timeout, *tp;
  const u_char *cp;
  int tries;
  int qlen;
  int res;
  struct tun_data tun;
#define rbuff tun.data

  if (mode & MODE_DIRECT) {
    LogPrintf(LogDEBUG, "Opening modem\n");
    if (modem_Open(bundle->physical, bundle) < 0)
      return;
    LogPrintf(LogPHASE, "Packet mode enabled\n");
    PacketMode(bundle, VarOpenMode);
  } else if (mode & MODE_DEDICATED) {
    if (!link_IsActive(physical2link(bundle->physical)))
      while (modem_Open(bundle->physical, bundle) < 0)
	nointr_sleep(VarReconnectTimer);
  }

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
    if (mode & MODE_DDIAL && LcpInfo.fsm.state <= ST_CLOSED)
      dial_up = 1;

    /*
     * If we lost carrier and want to re-establish the connection due to the
     * "set reconnect" value, we'd better bring the line back up.
     */
    if (!dialing && LcpInfo.fsm.state <= ST_CLOSED) {
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
        PacketMode(bundle, VarOpenMode);
    }

    /*
     * If Ip packet for output is enqueued and require dial up, Just do it!
     */
    if (dial_up && !dialing && RedialTimer.state != TIMER_RUNNING) {
      LogPrintf(LogDEBUG, "going to dial: modem = %d\n",
		Physical_GetFD(bundle->physical));
      if (modem_Open(bundle->physical, bundle) < 0) {
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

        chat_Init(&chat, bundle->physical, VarDialScript, 1);
        dialing = 1;
        dial_up = 0;
      }
    }

    qlen = link_QueueLen(physical2link(bundle->physical));
    if (qlen == 0) {
      IpStartOutput(physical2link(bundle->physical));
      qlen = link_QueueLen(physical2link(bundle->physical));
    }

    handle_signals();

    if (dialing) {
      descriptor_UpdateSet(&chat.desc, &rfds, &wfds, &efds, &nfds);
      if (dialing == -1) {
        if (chat.state == CHAT_DONE || chat.state == CHAT_FAILED) {
          dialing = 0;
          modem_Close(bundle->physical);
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
          continue;
        }
      } else if (chat.state == CHAT_DONE) {
        if (dialing == 1) {
          chat_Init(&chat, bundle->physical, VarLoginScript, 0);
          dialing++;
          continue;
        } else {
          PacketMode(bundle, VarOpenMode);
          reconnectState = RECON_UNKNOWN;
          tries = 0;
          dialing = 0;
        }
      } else if (chat.state == CHAT_FAILED) {
        chat_Init(&chat, bundle->physical, VarHangupScript, 0);
        dialing = -1;
        continue;
      }
    }

    if (!dialing)
      descriptor_UpdateSet(&bundle->physical->desc, &rfds, &wfds, &efds, &nfds);
    descriptor_UpdateSet(&server.desc, &rfds, &wfds, &efds, &nfds);

#ifndef SIGALRM
    /*
     * *** IMPORTANT ***
     * CPU is serviced every TICKUNIT micro seconds. This value must be chosen
     * with great care. If this values is too big, it results in loss of
     * characters from the modem and poor response.  If this value is too
     * small, ppp eats too much CPU time.
     */
    usleep(TICKUNIT);
    TimerService();
#endif

    /* If there are aren't many packets queued, look for some more. */
    if (qlen < 20 && bundle->tun_fd >= 0) {
      if (bundle->tun_fd + 1 > nfds)
	nfds = bundle->tun_fd + 1;
      FD_SET(bundle->tun_fd, &rfds);
    }

    descriptor_UpdateSet(&prompt.desc, &rfds, &wfds, &efds, &nfds);

#ifndef SIGALRM

    /*
     * Normally, select() will not block because modem is writable. In AUTO
     * mode, select will block until we find packet from tun
     */
    tp = (RedialTimer.state == TIMER_RUNNING) ? &timeout : NULL;
    i = select(nfds, &rfds, &wfds, &efds, tp);
#else

    /*
     * When SIGALRM timer is running, the select function will return -1 and
     * EINTR after the Time Service signal handler is done.  If the redial
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

    if (descriptor_IsSet(&prompt.desc, &efds) ||
        descriptor_IsSet(&bundle->physical->desc, &efds)) {
      LogPrintf(LogALERT, "Exception detected.\n");
      break;
    }

    if (descriptor_IsSet(&server.desc, &rfds))
      descriptor_Read(&server.desc, bundle, &rfds);

    if (descriptor_IsSet(&prompt.desc, &rfds))
      descriptor_Read(&prompt.desc, bundle, &rfds);

    if (dialing) {
      if (descriptor_IsSet(&chat.desc, &wfds))
        descriptor_Write(&chat.desc, &wfds);
      if (descriptor_IsSet(&chat.desc, &rfds))
        descriptor_Read(&chat.desc, bundle, &rfds);
    } else {
      if (descriptor_IsSet(&bundle->physical->desc, &wfds)) {
        /* ready to write into modem */
        descriptor_Write(&bundle->physical->desc, &wfds);
        if (!link_IsActive(physical2link(bundle->physical)))
          dial_up = 1;
      }

      if (descriptor_IsSet(&bundle->physical->desc, &rfds))
        descriptor_Read(&bundle->physical->desc, bundle, &rfds);
    }

    if (bundle->tun_fd >= 0 && FD_ISSET(bundle->tun_fd, &rfds)) {
      /* something to read from tun */
      n = read(bundle->tun_fd, &tun, sizeof tun);
      if (n < 0) {
	LogPrintf(LogERROR, "read from tun: %s\n", strerror(errno));
	continue;
      }
      n -= sizeof tun - sizeof tun.data;
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

#ifndef NOALIAS
	    if (mode & MODE_ALIAS) {
	      VarPacketAliasIn(rbuff, sizeof rbuff);
	      n = ntohs(((struct ip *) rbuff)->ip_len);
	    }
#endif
	    bp = mballoc(n, MB_IPIN);
	    memcpy(MBUF_CTOP(bp), rbuff, n);
	    IpInput(bundle, bp);
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
      if (LcpInfo.fsm.state <= ST_CLOSED && (mode & MODE_AUTO) &&
	  (pri = PacketCheck(rbuff, n, FL_DIAL)) >= 0)
        dial_up = 1;

      pri = PacketCheck(rbuff, n, FL_OUT);
      if (pri >= 0) {
#ifndef NOALIAS
	if (mode & MODE_ALIAS) {
	  VarPacketAliasOut(rbuff, sizeof rbuff);
	  n = ntohs(((struct ip *) rbuff)->ip_len);
	}
#endif
	IpEnqueue(pri, rbuff, n);
      }
    }
  }
  LogPrintf(LogDEBUG, "Job (DoLoop) done.\n");
}
