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
 * $Id: main.c,v 1.121.2.51 1998/04/25 10:49:26 brian Exp $
 *
 *	TODO:
 */

#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <sys/un.h>
#include <net/if_tun.h>

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "mbuf.h"
#include "log.h"
#include "defs.h"
#include "id.h"
#include "timer.h"
#include "fsm.h"
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
#include "link.h"
#include "mp.h"
#include "bundle.h"
#include "loadalias.h"
#include "auth.h"
#include "systems.h"
#include "ip.h"
#include "sig.h"
#include "main.h"
#include "pathnames.h"
#include "tun.h"
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

static void DoLoop(struct bundle *, struct prompt *);
static void TerminalStop(int);
static const char *ex_desc(int);

static struct bundle *SignalBundle;
static struct prompt *SignalPrompt;

void
Cleanup(int excode)
{
  SignalBundle->CleaningUp = 1;
  if (bundle_Phase(SignalBundle) != PHASE_DEAD)
    bundle_Close(SignalBundle, NULL, 1);
}

void
AbortProgram(int excode)
{
  ServerClose(SignalBundle);
  ID0unlink(pid_filename);
  LogPrintf(LogPHASE, "PPP Terminated (%s).\n", ex_desc(excode));
  bundle_Close(SignalBundle, NULL, 1);
  bundle_Destroy(SignalBundle);
  LogClose();
  exit(excode);
}

static void
CloseConnection(int signo)
{
  /* NOTE, these are manual, we've done a setsid() */
  struct datalink *dl;

  pending_signal(SIGINT, SIG_IGN);
  LogPrintf(LogPHASE, "Caught signal %d, abort connection(s)\n", signo);
  for (dl = SignalBundle->links; dl; dl = dl->next)
    datalink_Down(dl, 1);
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
  signal(SIGCONT, SIG_DFL);
  prompt_Continue(SignalPrompt);
}

static void
TerminalStop(int signo)
{
  prompt_Suspend(SignalPrompt);
  signal(SIGCONT, TerminalCont);
  raise(SIGSTOP);
}

#if 0 /* What's our passwd :-O */
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
#endif

static void
BringDownServer(int signo)
{
  /* Drops all child prompts too ! */
  ServerClose(SignalBundle);
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
ProcessArgs(int argc, char **argv, int *mode)
{
  int optc, labelrequired;
  char *cp;

  optc = labelrequired = 0;
  *mode = PHYS_MANUAL;
  while (argc > 0 && **argv == '-') {
    cp = *argv + 1;
    if (strcmp(cp, "auto") == 0) {
      *mode = PHYS_DEMAND;
      labelrequired = 1;
    } else if (strcmp(cp, "background") == 0) {
      *mode = PHYS_1OFF;
      labelrequired = 1;
    } else if (strcmp(cp, "direct") == 0)
      *mode = PHYS_STDIN;
    else if (strcmp(cp, "dedicated") == 0)
      *mode = PHYS_DEDICATED;
    else if (strcmp(cp, "ddial") == 0) {
      *mode = PHYS_PERM;
      labelrequired = 1;
    } else if (strcmp(cp, "alias") == 0) {
#ifndef NOALIAS
      if (loadAliasHandlers() != 0)
#endif
	LogPrintf(LogWARN, "Cannot load alias library\n");
      optc--;			/* this option isn't exclusive */
    } else
      Usage();
    optc++;
    argv++;
    argc--;
  }
  if (argc > 1) {
    fprintf(stderr, "You may specify only one system label.\n");
    exit(EX_START);
  }

  if (optc > 1) {
    fprintf(stderr, "You may specify only one mode.\n");
    exit(EX_START);
  }

  if (labelrequired && argc != 1) {
    fprintf(stderr, "Destination system must be specified in"
            " auto, background or ddial mode.\n");
    exit(EX_START);
  }

  return argc == 1 ? *argv : NULL;	/* Don't SetLabel yet ! */
}

int
main(int argc, char **argv)
{
  FILE *lockfile;
  char *name, *label;
  int nfds, mode;
  struct bundle *bundle;
  struct prompt *prompt;

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
  label = ProcessArgs(argc, argv, &mode);

#ifdef __FreeBSD__
  /*
   * A FreeBSD hack to dodge a bug in the tty driver that drops output
   * occasionally.... I must find the real reason some time.  To display
   * the dodgy behaviour, comment out this bit, make yourself a large
   * routing table and then run ppp in interactive mode.  The `show route'
   * command will drop chunks of data !!!
   */
  if (mode == PHYS_MANUAL) {
    close(STDIN_FILENO);
    if (open(_PATH_TTY, O_RDONLY) != STDIN_FILENO) {
      fprintf(stderr, "Cannot open %s for input !\n", _PATH_TTY);
      return 2;
    }
  }
#endif

  /* Allow output for the moment (except in direct mode) */
  if (mode == PHYS_STDIN)
    prompt = NULL;
  else {
    const char *m;

    SignalPrompt = prompt = prompt_Create(NULL, NULL, PROMPT_STD);
    if (mode == PHYS_PERM)
      m = "direct dial";
    else if (mode & PHYS_1OFF)
      m = "background";
    else if (mode & PHYS_DEMAND)
      m = "auto";
    else if (mode & PHYS_DEDICATED)
      m = "dedicated";
    else if (mode & PHYS_MANUAL)
      m = "interactive";
    else
      m = NULL;

    if (m)
      prompt_Printf(prompt, "Working in %s mode\n", m);
  }

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

  if (!ValidSystem(label, prompt, mode)) {
    fprintf(stderr, "You may not use ppp in this mode with this label\n");
    if (mode == PHYS_STDIN) {
      const char *l;
      l = label ? label : "default";
      LogPrintf(LogWARN, "Label %s rejected -direct connection\n", l);
    }
    LogClose();
    return 1;
  }

  if ((bundle = bundle_Create(TUN_PREFIX, prompt, mode)) == NULL) {
    LogPrintf(LogWARN, "bundle_Create: %s\n", strerror(errno));
    return EX_START;
  }
  SignalBundle = bundle;

  if (SelectSystem(bundle, "default", CONFFILE, prompt) < 0)
    prompt_Printf(prompt, "Warning: No default entry found in config file.\n");

  pending_signal(SIGHUP, CloseSession);
  pending_signal(SIGTERM, CloseSession);
  pending_signal(SIGINT, CloseConnection);
  pending_signal(SIGQUIT, CloseSession);
  pending_signal(SIGALRM, SIG_IGN);
  signal(SIGPIPE, SIG_IGN);

  if (mode == PHYS_MANUAL)
    pending_signal(SIGTSTP, TerminalStop);

#if 0 /* What's our passwd :-O */
  pending_signal(SIGUSR1, SetUpServer);
#endif
  pending_signal(SIGUSR2, BringDownServer);

  if (label) {
    /*
     * Set label both before and after SelectSystem !
     * This way, "set enddisc label" works during SelectSystem, and we
     * also end up with the correct label if we have embedded load
     * commands.
     */
    bundle_SetLabel(bundle, label);
    if (SelectSystem(bundle, label, CONFFILE, prompt) < 0) {
      prompt_Printf(prompt, "Destination system (%s) not found.\n", label);
      AbortProgram(EX_START);
    }
    bundle_SetLabel(bundle, label);
    if (mode == PHYS_DEMAND &&
	bundle->ncp.ipcp.cfg.peer_range.ipaddr.s_addr == INADDR_ANY) {
      prompt_Printf(prompt, "You must \"set ifaddr\" with a peer address "
                    "in label %s for auto mode.\n", label);
      AbortProgram(EX_START);
    }
  }

  if (mode != PHYS_MANUAL) {
    if (mode != PHYS_STDIN) {
      int bgpipe[2];
      pid_t bgpid;

      if (mode == PHYS_1OFF && pipe(bgpipe)) {
        LogPrintf(LogERROR, "pipe: %s\n", strerror(errno));
	AbortProgram(EX_SOCK);
      }

      bgpid = fork();
      if (bgpid == -1) {
	LogPrintf(LogERROR, "fork: %s\n", strerror(errno));
	AbortProgram(EX_SOCK);
      }

      if (bgpid) {
	char c = EX_NORMAL;

	if (mode == PHYS_1OFF) {
	  close(bgpipe[1]);
	  BGPid = bgpid;
          /* If we get a signal, kill the child */
          signal(SIGHUP, KillChild);
          signal(SIGTERM, KillChild);
          signal(SIGINT, KillChild);
          signal(SIGQUIT, KillChild);

	  /* Wait for our child to close its pipe before we exit */
	  if (read(bgpipe[0], &c, 1) != 1) {
	    prompt_Printf(prompt, "Child exit, no status.\n");
	    LogPrintf(LogPHASE, "Parent: Child exit, no status.\n");
	  } else if (c == EX_NORMAL) {
	    prompt_Printf(prompt, "PPP enabled.\n");
	    LogPrintf(LogPHASE, "Parent: PPP enabled.\n");
	  } else {
	    prompt_Printf(prompt, "Child failed (%s).\n", ex_desc((int) c));
	    LogPrintf(LogPHASE, "Parent: Child failed (%s).\n",
		      ex_desc((int) c));
	  }
	  close(bgpipe[0]);
	}
	return c;
      } else if (mode == PHYS_1OFF) {
	close(bgpipe[0]);
        bundle->notify.fd = bgpipe[1];
      }

      /* -auto, -dedicated, -ddial & -background */
      prompt_Destroy(prompt, 0);
      close(STDOUT_FILENO);
      close(STDERR_FILENO);
      close(STDIN_FILENO);
      setsid();
    } else {
      /* -direct: STDIN_FILENO gets used by modem_Open */
      prompt_TtyInit(NULL);
      close(STDOUT_FILENO);
      close(STDERR_FILENO);
    }
  } else {
    /* Interactive mode */
    close(STDERR_FILENO);
    prompt_TtyInit(prompt);
    prompt_TtyCommandMode(prompt);
    prompt_Required(prompt);
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

  LogPrintf(LogPHASE, "PPP Started (%s mode).\n", mode2Nam(mode));
  DoLoop(bundle, prompt);
  AbortProgram(EX_NORMAL);

  return EX_NORMAL;
}

static void
DoLoop(struct bundle *bundle, struct prompt *prompt)
{
  fd_set rfds, wfds, efds;
  int pri, i, n, nfds;
  int qlen;
  struct tun_data tun;

  do {
    nfds = 0;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_ZERO(&efds);

    qlen = bundle_FillQueues(bundle);

    handle_signals();

    descriptor_UpdateSet(&bundle->desc, &rfds, &wfds, &efds, &nfds);
    descriptor_UpdateSet(&server.desc, &rfds, &wfds, &efds, &nfds);
    descriptor_UpdateSet(&bundle->ncp.mp.server.desc, &rfds, &wfds,
                         &efds, &nfds);

    /* If there are aren't many packets queued, look for some more. */
    if (qlen < 20 && bundle->tun_fd >= 0) {
      if (bundle->tun_fd + 1 > nfds)
	nfds = bundle->tun_fd + 1;
      FD_SET(bundle->tun_fd, &rfds);
    }

    if (bundle_IsDead(bundle))
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

    if (descriptor_IsSet(&bundle->ncp.mp.server.desc, &rfds))
      descriptor_Read(&bundle->ncp.mp.server.desc, bundle, &rfds);

    if (descriptor_IsSet(&server.desc, &rfds))
      descriptor_Read(&server.desc, bundle, &rfds);

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
        if (Enabled(bundle, OPT_LOOPBACK)) {
	  pri = PacketCheck(bundle, tun.data, n, &bundle->filter.in);
	  if (pri >= 0) {
	    struct mbuf *bp;

#ifndef NOALIAS
            if (AliasEnabled()) {
	      (*PacketAlias.In)(tun.data, sizeof tun.data);
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
      if (bundle_Phase(bundle) == PHASE_DEAD) {
        /*
         * Note, we must be in AUTO mode :-/ otherwise our interface should
         * *not* be UP and we can't receive data
         */
        if ((pri = PacketCheck(bundle, tun.data, n, &bundle->filter.dial)) >= 0)
          bundle_Open(bundle, NULL, PHYS_DEMAND);
        else
          /*
           * Drop the packet.  If we were to queue it, we'd just end up with
           * a pile of timed-out data in our output queue by the time we get
           * around to actually dialing.  We'd also prematurely reach the 
           * threshold at which we stop select()ing to read() the tun
           * device - breaking auto-dial.
           */
          continue;
      }

      pri = PacketCheck(bundle, tun.data, n, &bundle->filter.out);
      if (pri >= 0) {
#ifndef NOALIAS
        if (AliasEnabled()) {
	  (*PacketAlias.Out)(tun.data, sizeof tun.data);
	  n = ntohs(((struct ip *)tun.data)->ip_len);
	}
#endif
	IpEnqueue(pri, tun.data, n);
      }
    }
  } while (bundle_CleanDatalinks(bundle), !bundle_IsDead(bundle));

  LogPrintf(LogDEBUG, "DoLoop done.\n");
}
