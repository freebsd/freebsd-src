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
 * $Id: main.c,v 1.121.2.37 1998/03/25 00:59:38 brian Exp $
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
#include "lqr.h"
#include "hdlc.h"
#include "lcp.h"
#include "ccp.h"
#include "iplist.h"
#include "throughput.h"
#include "slcompress.h"
#include "ipcp.h"
#include "filter.h"
#include "descriptor.h"
#include "bundle.h"
#include "loadalias.h"
#include "vars.h"
#include "auth.h"
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
#include "physical.h"
#include "server.h"
#include "prompt.h"
#include "chat.h"
#include "chap.h"
#include "datalink.h"

#ifndef O_NONBLOCK
#ifdef O_NDELAY
#define	O_NONBLOCK O_NDELAY
#endif
#endif

static char pid_filename[MAXPATHLEN];

static void DoLoop(struct bundle *);
static void TerminalStop(int);
static const char *ex_desc(int);

static struct bundle *SignalBundle;

void
Cleanup(int excode)
{
  SignalBundle->CleaningUp = 1;
  if (bundle_Phase(SignalBundle) != PHASE_DEAD)
    bundle_Close(SignalBundle, NULL, 0);
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
  bundle_Close(SignalBundle, NULL, 1);
  bundle_Destroy(SignalBundle);
  LogClose();
  exit(excode);
}

static void
CloseConnection(int signo)
{
  /* NOTE, these are manual, we've done a setsid() */
  pending_signal(SIGINT, SIG_IGN);
  LogPrintf(LogPHASE, "Caught signal %d, abort connection\n", signo);
  /* XXX close 'em all ! */
  link_Close(bundle2link(SignalBundle, NULL), SignalBundle, 0, 1);
  pending_signal(SIGINT, CloseConnection);
}

static void
CloseSession(int signo)
{
  LogPrintf(LogPHASE, "Signal %d, terminate.\n", signo);
  Cleanup(EX_TERM);
}

static pid_t BGPid = 0;

static void
KillChild(int signo)
{
  LogPrintf(LogPHASE, "Parent: Signal %d\n", signo);
  kill(BGPid, SIGINT);
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
	bundle->ncp.ipcp.cfg.peer_range.ipaddr.s_addr == INADDR_ANY) {
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
	  close(BGFiledes[1]);
	  BGPid = bgpid;
          /* If we get a signal, kill the child */
          signal(SIGHUP, KillChild);
          signal(SIGTERM, KillChild);
          signal(SIGINT, KillChild);
          signal(SIGQUIT, KillChild);

	  /* Wait for our child to close its pipe before we exit */
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

static void
DoLoop(struct bundle *bundle)
{
  fd_set rfds, wfds, efds;
  int pri, i, n, nfds;
  int qlen;
  struct tun_data tun;

  if (mode & (MODE_DIRECT|MODE_DEDICATED|MODE_BACKGROUND))
    bundle_Open(bundle, NULL);

  while (!bundle->CleaningUp || bundle_Phase(SignalBundle) != PHASE_DEAD) {
    nfds = 0;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_ZERO(&efds);

    qlen = bundle_FillQueues(bundle);

    handle_signals();

    descriptor_UpdateSet(&bundle->desc, &rfds, &wfds, &efds, &nfds);
    descriptor_UpdateSet(&server.desc, &rfds, &wfds, &efds, &nfds);

    /* If there are aren't many packets queued, look for some more. */
    if (qlen < 20 && bundle->tun_fd >= 0) {
      if (bundle->tun_fd + 1 > nfds)
	nfds = bundle->tun_fd + 1;
      FD_SET(bundle->tun_fd, &rfds);
    }

    descriptor_UpdateSet(&prompt.desc, &rfds, &wfds, &efds, &nfds);

    if (bundle->CleaningUp && bundle_Phase(bundle) == PHASE_DEAD)
      /* Don't select - we'll be here forever */
      break;

    i = select(nfds, &rfds, &wfds, &efds, NULL);

    if (i == 0)
      continue;

    if (i < 0) {
      if (errno == EINTR) {
	handle_signals();
	continue;
      }
      LogPrintf(LogERROR, "DoLoop: select(): %s\n", strerror(errno));
      break;
    }

    for (i = 0; i <= nfds; i++)
      if (FD_ISSET(i, &efds)) {
        LogPrintf(LogALERT, "Exception detected on descriptor %d\n", i);
        break;
      }

    if (descriptor_IsSet(&server.desc, &rfds))
      descriptor_Read(&server.desc, bundle, &rfds);

    if (descriptor_IsSet(&prompt.desc, &rfds))
      descriptor_Read(&prompt.desc, bundle, &rfds);

    if (descriptor_IsSet(&bundle->desc, &wfds))
      descriptor_Write(&bundle->desc, bundle, &wfds);

    if (descriptor_IsSet(&bundle->desc, &rfds))
      descriptor_Read(&bundle->desc, bundle, &rfds);

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
      if (((struct ip *)tun.data)->ip_dst.s_addr ==
          bundle->ncp.ipcp.my_ip.s_addr) {
	/* we've been asked to send something addressed *to* us :( */
	if (VarLoopback) {
	  pri = PacketCheck(bundle, tun.data, n, &bundle->filter.in);
	  if (pri >= 0) {
	    struct mbuf *bp;

#ifndef NOALIAS
	    if (mode & MODE_ALIAS) {
	      VarPacketAliasIn(tun.data, sizeof tun.data);
	      n = ntohs(((struct ip *)tun.data)->ip_len);
	    }
#endif
	    bp = mballoc(n, MB_IPIN);
	    memcpy(MBUF_CTOP(bp), tun.data, n);
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
      if (bundle_Phase(bundle) == PHASE_DEAD && (mode & MODE_AUTO) &&
	  (pri = PacketCheck(bundle, tun.data, n, &bundle->filter.dial)) >= 0)
        bundle_Open(bundle, NULL);

      pri = PacketCheck(bundle, tun.data, n, &bundle->filter.out);
      if (pri >= 0) {
#ifndef NOALIAS
	if (mode & MODE_ALIAS) {
	  VarPacketAliasOut(tun.data, sizeof tun.data);
	  n = ntohs(((struct ip *)tun.data)->ip_len);
	}
#endif
	IpEnqueue(pri, tun.data, n);
      }
    }
  }
  LogPrintf(LogDEBUG, "Job (DoLoop) done.\n");
}
