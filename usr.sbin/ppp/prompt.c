/*-
 * Copyright (c) 1998 Brian Somers <brian@Awfulhak.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: prompt.c,v 1.1.2.12 1998/03/13 21:07:43 brian Exp $
 */

#include <sys/param.h>
#include <netinet/in.h>

#include <stdarg.h>
#include <stdio.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include "defs.h"
#include "timer.h"
#include "command.h"
#include "log.h"
#include "descriptor.h"
#include "prompt.h"
#include "fsm.h"
#include "lcp.h"
#include "auth.h"
#include "loadalias.h"
#include "vars.h"
#include "main.h"
#include "iplist.h"
#include "throughput.h"
#include "ipcp.h"
#include "filter.h"
#include "bundle.h"
#include "lqr.h"
#include "hdlc.h"
#include "async.h"
#include "mbuf.h"
#include "link.h"
#include "physical.h"
#include "chat.h"
#include "ccp.h"
#include "chap.h"
#include "datalink.h"

static int prompt_nonewline = 1;

static int
prompt_UpdateSet(struct descriptor *d, fd_set *r, fd_set *w, fd_set *e, int *n)
{
  struct prompt *p = descriptor2prompt(d);
  int sets;

  LogPrintf(LogDEBUG, "descriptor2prompt; %p -> %p\n", d, p);

  sets = 0;
  if (p->fd_in >= 0) {
    if (r) {
      FD_SET(p->fd_in, r);
      sets++;
    }
    if (e) {
      FD_SET(p->fd_in, e);
      sets++;
    }
    if (sets && *n < p->fd_in + 1)
      *n = p->fd_in + 1;
  }

  return sets;
}

static int
prompt_IsSet(struct descriptor *d, fd_set *fdset)
{
  struct prompt *p = descriptor2prompt(d);
  LogPrintf(LogDEBUG, "descriptor2prompt; %p -> %p\n", d, p);
  return p->fd_in >= 0 && FD_ISSET(p->fd_in, fdset);
}


static void
prompt_ShowHelp(struct prompt *p)
{
  prompt_Printf(p, "The following commands are available:\r\n");
  prompt_Printf(p, " ~p\tEnter Packet mode\r\n");
  prompt_Printf(p, " ~-\tDecrease log level\r\n");
  prompt_Printf(p, " ~+\tIncrease log level\r\n");
  prompt_Printf(p, " ~t\tShow timers (only in \"log debug\" mode)\r\n");
  prompt_Printf(p, " ~m\tShow memory map (only in \"log debug\" mode)\r\n");
  prompt_Printf(p, " ~.\tTerminate program\r\n");
  prompt_Printf(p, " ~?\tThis help\r\n");
}

static void
prompt_Read(struct descriptor *d, struct bundle *bundle, const fd_set *fdset)
{
  struct prompt *p = descriptor2prompt(d);
  int n;
  char ch;
  static int ttystate;
  char linebuff[LINE_LEN];

  LogPrintf(LogDEBUG, "descriptor2prompt; %p -> %p\n", d, p);
  LogPrintf(LogDEBUG, "termode = %p, p->fd_in = %d, mode = %d\n",
	    p->TermMode, p->fd_in, mode);

  if (p->TermMode == NULL) {
    n = read(p->fd_in, linebuff, sizeof linebuff - 1);
    if (n > 0) {
      if (linebuff[n-1] == '\n')
        linebuff[--n] = '\0';
      else
        linebuff[n] = '\0';
      if (n)
        DecodeCommand(bundle, linebuff, n, IsInteractive(0) ? NULL : "Client");
      prompt_nonewline = 1;
      prompt_Display(&prompt, bundle);
    } else if (n <= 0) {
      LogPrintf(LogPHASE, "Client connection closed.\n");
      prompt_Drop(&prompt, 0);
    }
    return;
  }

  switch (p->TermMode->state) {
    case DATALINK_CLOSED:
      prompt_Printf(p, "Link lost, terminal mode.\n");
      prompt_TtyCommandMode(&prompt);
      prompt_nonewline = 0;
      prompt_Display(&prompt, bundle);
      return;

    case DATALINK_READY:
      break;

    case DATALINK_OPEN:
      prompt_Printf(p, "\nPacket mode detected.\n");
      prompt_TtyCommandMode(&prompt);
      prompt_nonewline = 0;
      /* We'll get a prompt because of our status change */
      /* Fall through */

    default:
      /* Wait 'till we're in a state we care about */
      return;
  }

  /*
   * We are in terminal mode, decode special sequences
   */
  n = read(p->fd_in, &ch, 1);
  LogPrintf(LogDEBUG, "Got %d bytes (reading from the terminal)\n", n);

  if (n > 0) {
    switch (ttystate) {
    case 0:
      if (ch == '~')
	ttystate++;
      else
	/* XXX missing return value check */
	Physical_Write(bundle2physical(bundle, NULL), &ch, n);
      break;
    case 1:
      switch (ch) {
      case '?':
	prompt_ShowHelp(p);
	break;
      case 'p':
        datalink_Up(p->TermMode, 0, 1);
        prompt_Printf(p, "\nPacket mode.\n");
	prompt_TtyCommandMode(&prompt);
        break;
      case '.':
	prompt_TtyCommandMode(&prompt);
        prompt_nonewline = 0;
        prompt_Display(&prompt, bundle);
	break;
      case 't':
	if (LogIsKept(LogDEBUG)) {
	  ShowTimers();
	  break;
	}
      case 'm':
	if (LogIsKept(LogDEBUG)) {
	  ShowMemMap(NULL);
	  break;
	}
      default:
	if (Physical_Write(bundle2physical(bundle, NULL), &ch, n) < 0)
	  LogPrintf(LogERROR, "error writing to modem.\n");
	break;
      }
      ttystate = 0;
      break;
    }
  }
}

static void
prompt_Write(struct descriptor *d, struct bundle *bundle, const fd_set *fdset)
{
  /* We never want to write here ! */
  LogPrintf(LogERROR, "prompt_Write: Internal error: Bad call !\n");
}

struct prompt prompt = {
  {
    PROMPT_DESCRIPTOR,
    NULL,
    prompt_UpdateSet,
    prompt_IsSet,
    prompt_Read,
    prompt_Write
  },
  -1,
  -1,
  NULL
};

int
prompt_Init(struct prompt *p, int fd)
{
  if (p->Term && p->Term != stdout)
    return 0;                       /* must prompt_Drop() first */

  if (fd == PROMPT_NONE) {
    p->fd_in = p->fd_out = -1;
    p->Term = NULL;
  } else if (fd == PROMPT_STD) {
    p->fd_in = STDIN_FILENO;
    p->fd_out = STDOUT_FILENO;
    p->Term = stdout;
  } else {
    p->fd_in = p->fd_out = fd;
    p->Term = fdopen(fd, "a+");
  }
  p->TermMode = NULL;
  tcgetattr(STDIN_FILENO, &p->oldtio);	/* Save original tty mode */

  return 1;
}

void
prompt_Display(struct prompt *p, struct bundle *bundle)
{
  const char *pconnect, *pauth;

  if (!p->Term || p->TermMode != NULL || CleaningUp)
    return;

  if (prompt_nonewline)
    prompt_nonewline = 0;
  else
    fprintf(p->Term, "\n");

  if (VarLocalAuth == LOCAL_AUTH)
    pauth = " ON ";
  else
    pauth = " on ";

  if (bundle->ncp.ipcp.fsm.state == ST_OPENED)
    pconnect = "PPP";
  else if (bundle_Phase(bundle) == PHASE_NETWORK)
    pconnect = "PPp";
  else if (bundle_Phase(bundle) == PHASE_AUTHENTICATE)
    pconnect = "Ppp";
  else
    pconnect = "ppp";

  fprintf(p->Term, "%s%s%s> ", pconnect, pauth, VarShortHost);
  fflush(p->Term);
}

void
prompt_Drop(struct prompt *p, int verbose)
{
  if (p->Term && p->Term != stdout) {
    FILE *oVarTerm;

    oVarTerm = p->Term;
    p->Term = NULL;
    if (oVarTerm)
      fclose(oVarTerm);
    close(p->fd_in);
    if (p->fd_out != p->fd_in)
      close(p->fd_out);
    p->fd_in = p->fd_out = -1;
    if (verbose)
      LogPrintf(LogPHASE, "Client connection dropped.\n");
  }
}

void
prompt_Printf(struct prompt *p, const char *fmt,...)
{
  if (p->Term) {
    va_list ap;

    va_start(ap, fmt);
    vfprintf(p->Term, fmt, ap);
    fflush(p->Term);
    va_end(ap);
  }
}

void
prompt_vPrintf(struct prompt *p, const char *fmt, va_list ap)
{
  if (p->Term) {
    vfprintf(p->Term, fmt, ap);
    fflush(p->Term);
  }
}

void
prompt_TtyInit(struct prompt *p, int DontWantInt)
{
  struct termios newtio;
  int stat;

  stat = fcntl(p->fd_in, F_GETFL, 0);
  if (stat > 0) {
    stat |= O_NONBLOCK;
    (void) fcntl(p->fd_in, F_SETFL, stat);
  }
  newtio = p->oldtio;
  newtio.c_lflag &= ~(ECHO | ISIG | ICANON);
  newtio.c_iflag = 0;
  newtio.c_oflag &= ~OPOST;
  newtio.c_cc[VEOF] = _POSIX_VDISABLE;
  if (DontWantInt)
    newtio.c_cc[VINTR] = _POSIX_VDISABLE;
  newtio.c_cc[VMIN] = 1;
  newtio.c_cc[VTIME] = 0;
  newtio.c_cflag |= CS8;
  tcsetattr(p->fd_in, TCSANOW, &newtio);
  p->comtio = newtio;
}

/*
 *  Set tty into command mode. We allow canonical input and echo processing.
 */
void
prompt_TtyCommandMode(struct prompt *p)
{
  struct termios newtio;
  int stat;

  if (!(mode & MODE_INTER))
    return;

  tcgetattr(p->fd_in, &newtio);
  newtio.c_lflag |= (ECHO | ISIG | ICANON);
  newtio.c_iflag = p->oldtio.c_iflag;
  newtio.c_oflag |= OPOST;
  tcsetattr(p->fd_in, TCSADRAIN, &newtio);
  stat = fcntl(p->fd_in, F_GETFL, 0);
  if (stat > 0) {
    stat |= O_NONBLOCK;
    (void) fcntl(p->fd_in, F_SETFL, stat);
  }
  p->TermMode = NULL;
}

/*
 * Set tty into terminal mode which is used while we invoke term command.
 */
void
prompt_TtyTermMode(struct prompt *p, struct datalink *dl)
{
  int stat;

  tcsetattr(p->fd_in, TCSADRAIN, &p->comtio);
  stat = fcntl(p->fd_in, F_GETFL, 0);
  if (stat > 0) {
    stat &= ~O_NONBLOCK;
    (void) fcntl(p->fd_in, F_SETFL, stat);
  }
  p->TermMode = dl;
}

void
prompt_TtyOldMode(struct prompt *p)
{
  int stat;

  stat = fcntl(p->fd_in, F_GETFL, 0);
  if (stat > 0) {
    stat &= ~O_NONBLOCK;
    (void) fcntl(p->fd_in, F_SETFL, stat);
  }
  tcsetattr(p->fd_in, TCSADRAIN, &p->oldtio);
}

pid_t
prompt_pgrp(struct prompt *p)
{
  return p->Term ? tcgetpgrp(p->fd_in) : -1;
}
