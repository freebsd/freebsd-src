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
 *	$Id: chat.c,v 1.44.2.10 1998/02/18 00:28:06 brian Exp $
 */

#include <sys/param.h>
#include <netinet/in.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
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
#include "timer.h"
#include "loadalias.h"
#include "vars.h"
#include "modem.h"
#include "hdlc.h"
#include "throughput.h"
#include "fsm.h"
#include "lcp.h"
#include "link.h"
#include "async.h"
#include "descriptor.h"
#include "physical.h"
#include "chat.h"
#include "prompt.h"

#define BUFLEFT(c) (sizeof (c)->buf - ((c)->bufend - (c)->buf))
#define	issep(c)	((c) == '\t' || (c) == ' ')

struct chat chat;
static void ExecStr(struct physical *, char *, char *, int);

static void
chat_PauseTimer(void *v)
{
  struct chat *c = (struct chat *)v;
  StopTimer(&c->pause);
  c->pause.state = TIMER_STOPPED;
  c->pause.load = 0;
}

static void
chat_Pause(struct chat *c, u_long load)
{
  StopTimer(&c->pause);
  c->pause.state = TIMER_STOPPED;
  c->pause.load += load;
  c->pause.func = chat_PauseTimer;
  c->pause.arg = c;
  StartTimer(&c->pause);
}

static void
chat_TimeoutTimer(void *v)
{
  struct chat *c = (struct chat *)v;
  StopTimer(&c->timeout);
  c->TimedOut = 1;
}

static void
chat_SetTimeout(struct chat *c)
{
  StopTimer(&c->timeout);
  c->timeout.state = TIMER_STOPPED;
  if (c->TimeoutSec > 0) {
    c->timeout.load = SECTICKS * c->TimeoutSec;
    c->timeout.func = chat_TimeoutTimer;
    c->timeout.arg = c;
    StartTimer(&c->timeout);
  }
}

static char *
chat_NextChar(char *ptr, char ch)
{
  for (; *ptr; ptr++)
    if (*ptr == ch)
      return ptr;
    else if (*ptr == '\\')
      if (*++ptr == '\0')
        return NULL;

  return NULL;
}

static int
chat_UpdateSet(struct descriptor *d, fd_set *r, fd_set *w, fd_set *e, int *n)
{
  struct chat *c = descriptor2chat(d);
  int special, gotabort, gottimeout, needcr;

  if (c->pause.state == TIMER_RUNNING)
    return 0;

  if (c->TimedOut) {
    LogPrintf(LogCHAT, "Expect timeout\n");
    if ( c->nargptr == NULL)
      c->state = CHAT_FAILED;
    else {
      c->state = CHAT_EXPECT;
      c->argptr = "";
    }
  }

  if (c->state != CHAT_EXPECT && c->state != CHAT_SEND)
    return 0;

  gottimeout = gotabort = 0;

  if (c->arg < c->argc && (c->arg < 0 || *c->argptr == '\0')) {
    /* Go get the next string */
    if (c->arg < 0 || c->state == CHAT_SEND)
      c->state = CHAT_EXPECT;
    else
      c->state = CHAT_SEND;

    special = 1;
    while (special && (c->nargptr || c->arg < c->argc - 1)) {
      if (c->arg < 0 || !c->TimedOut)
        c->nargptr = NULL;

      if (c->nargptr != NULL) {
        /* We're doing expect-send-expect.... */
        c->argptr = c->nargptr;
        /* Put the '-' back in case we ever want to rerun our script */
        c->nargptr[-1] = '-';
        c->nargptr = chat_NextChar(c->nargptr, '-');
        if (c->nargptr != NULL)
          *c->nargptr++ = '\0';
      } else {
        int minus;

        c->argptr = c->argv[++c->arg];

        if (c->state == CHAT_EXPECT) {
          /* Look for expect-send-expect sequence */
          c->nargptr = c->argptr;
          minus = 0;
          while ((c->nargptr = chat_NextChar(c->nargptr, '-'))) {
            c->nargptr++;
            minus++;
          }

          if (minus % 2)
            LogPrintf(LogWARN, "chat_UpdateSet: \"%s\": Uneven number of"
                      " '-' chars, all ignored\n", c->argptr);
          else if (minus) {
            c->nargptr = chat_NextChar(c->argptr, '-');
            *c->nargptr++ = '\0';
          }
        }
      }

      /*
       * c->argptr now temporarily points into c->script (via c->argv)
       * If it's an expect-send-expect sequence, we've just got the correct
       * portion of that sequence.
       */

      needcr = c->state == CHAT_SEND && *c->argptr != '!';

      /* We leave room for a potential HDLC header in the target string */
      chat_ExpandString(c, c->argptr, c->exp + 2, sizeof c->exp - 2, needcr);

      if (gotabort) {
        if (c->numaborts < sizeof c->AbortStrings / sizeof c->AbortStrings[0])
          c->AbortStrings[c->numaborts++] = strdup(c->exp+2);
        else
          LogPrintf(LogERROR, "chat_UpdateSet: AbortStrings overflow\n");
        gotabort = 0;
      } else if (gottimeout) {
        c->TimeoutSec = atoi(c->exp + 2);
        if (c->TimeoutSec <= 0)
          c->TimeoutSec = 30;
        gottimeout = 0;
      } else if (c->nargptr == NULL && !strcmp(c->exp+2, "ABORT"))
        gotabort = 1;
      else if (c->nargptr == NULL && !strcmp(c->exp+2, "TIMEOUT"))
        gottimeout = 1;
      else {
        if (c->exp[2] == '!')
          ExecStr(c->physical, c->exp + 3, c->exp + 2, sizeof c->exp - 2);

        if (c->exp[2] == '\0') {
          /* Empty string, reparse (this may be better as a `goto start') */
          c->argptr = "";
          return chat_UpdateSet(d, r, w, e, n);
        }

        special = 0;
      }
    }

    if (special) {
      if (gottimeout)
        LogPrintf(LogWARN, "chat_UpdateSet: TIMEOUT: Argument expected\n");
      else if (gotabort)
        LogPrintf(LogWARN, "chat_UpdateSet: ABORT: Argument expected\n");

      /* End of script - all ok */
      c->state = CHAT_DONE;
      return 0;
    }

    /* set c->argptr to point in the right place */
    c->argptr = c->exp + 2;
    c->arglen = strlen(c->argptr);

    if (c->state == CHAT_EXPECT) {
      /* We must check to see if the string's already been found ! */
      char *begin, *end;

      end = c->bufend - c->arglen + 1;
      if (end < c->bufstart)
        end = c->bufstart;
      for (begin = c->bufstart; begin < end; begin++)
        if (!strncmp(begin, c->argptr, c->arglen)) {
          c->bufstart = begin + c->arglen;
          c->argptr += c->arglen;
          c->arglen = 0;
          /* Continue - we've already read our expect string */
          return chat_UpdateSet(d, r, w, e, n);
        }

      c->TimedOut = 0;
      LogPrintf(LogCHAT, "Expect(%d): %s\n", c->TimeoutSec, c->argptr);
      chat_SetTimeout(c);
    }
  }

  /*
   * We now have c->argptr pointing at what we want to expect/send and
   * c->state saying what we want to do... we now know what to put in
   * the fd_set :-)
   */

  if (c->state == CHAT_EXPECT)
    return Physical_UpdateSet(&c->physical->desc, r, NULL, e, n, 1);
  else
    return Physical_UpdateSet(&c->physical->desc, NULL, w, e, n, 1);
}

static int
chat_IsSet(struct descriptor *d, fd_set *fdset)
{
  struct chat *c = descriptor2chat(d);
  return Physical_IsSet(&c->physical->desc, fdset);
}

static void
chat_UpdateLog(struct chat *c, int in)
{
  if (LogIsKept(LogCHAT) || LogIsKept(LogCONNECT)) {
    /*
     * If a linefeed appears in the last `in' characters of `c's input
     * buffer, output from there, all the way back to the last linefeed.
     * This is called for every read of `in' bytes.
     */
    char *ptr, *end, *stop;
    int level;

    level = LogIsKept(LogCHAT) ? LogCHAT : LogCONNECT;
    ptr = c->bufend - in;

    for (end = c->bufend - 1; end >= ptr; end--)
      if (*end == '\n')
        break;

    if (end >= ptr) {
      for (ptr = c->bufend - in - 1; ptr >= c->bufstart; ptr--)
        if (*ptr == '\n')
          break;
      ptr++;
      stop = NULL;
      while (stop != end && (stop = strchr(ptr, '\n'))) {
        *stop = '\0';
        if (level == LogCHAT || strstr(ptr, "CONNECT"))
          LogPrintf(level, "Received: %s\n", ptr);
        *stop = '\n';
        ptr = stop + 1;
      }
    }
  }
}

static void
chat_Read(struct descriptor *d, struct bundle *bundle, const fd_set *fdset)
{
  struct chat *c = descriptor2chat(d);

  if (c->state == CHAT_EXPECT) {
    ssize_t in;
    char *begin, *end;

    /*
     * XXX - should this read only 1 byte to guarantee that we don't
     * swallow any ppp talk from the peer ?
     */
    in = BUFLEFT(c);
    if (in > sizeof c->buf / 2)
      in = sizeof c->buf / 2;

    in = Physical_Read(c->physical, c->bufend, in);
    if (in <= 0)
      return;

    /* `begin' and `end' delimit where we're going to strncmp() from */
    begin = c->bufend - c->arglen + 1;
    end = begin + in;
    if (begin < c->bufstart)
      begin = c->bufstart;
    c->bufend += in;

    chat_UpdateLog(c, in);

    if (c->bufend > c->buf + sizeof c->buf / 2) {
      /* Shuffle our receive buffer back a bit */
      int chop;

      for (chop = begin - c->buf; chop; chop--)
        if (c->buf[chop] == '\n')
          /* found some already-logged garbage to remove :-) */
          break;
      if (!chop) {
        chop = begin - c->buf;
      }
      if (chop) {
        char *from, *to;

        to = c->buf;
        from = to + chop;
        while (from < c->bufend)
          *to++ = *from++;
        c->bufstart -= chop;
        c->bufend -= chop;
        begin -= chop;
        end -= chop;
      }
    }

    for (; begin < end; begin++)
      if (!strncmp(begin, c->argptr, c->arglen)) {
        /* Got it ! */
        if (begin[c->arglen - 1] != '\n') {
          /* Now coerce chat_UpdateLog() into logging it.... */
          char ch;

          end = c->bufend;
          c->bufend = begin + c->arglen;
          ch = *c->bufend;
          *c->bufend++ = '\n';
          chat_UpdateLog(c, 1);
          *--c->bufend = ch;
          c->bufend = end;
        }
        c->bufstart = begin + c->arglen;
        c->argptr += c->arglen;
        c->arglen = 0;
        break;
      }
  }
}

static void
chat_Write(struct descriptor *d, struct bundle *bundle, const fd_set *fdset)
{
  struct chat *c = descriptor2chat(d);

  if (c->state == CHAT_SEND) {
    int wrote;

    if (strstr(c->argv[c->arg], "\\P"))            /* Don't log the password */
      LogPrintf(LogCHAT, "Send: %s\n", c->argv[c->arg]);
    else {
      int sz;

      sz = c->arglen - 1;
      while (sz >= 0 && c->argptr[sz] == '\n')
        sz--;
      LogPrintf(LogCHAT, "Send: %.*s\n", sz + 1, c->argptr);
    }

    if (Physical_IsSync(c->physical)) {
      /* There's always room for the HDLC header */
      c->argptr -= 2;
      c->arglen += 2;
      memcpy(c->argptr, "\377\003", 2);	/* Prepend HDLC header */
    }

    wrote = Physical_Write(c->physical, c->argptr, c->arglen);
    if (wrote == -1) {
      if (errno != EINTR)
        LogPrintf(LogERROR, "chat_Write: %s\n", strerror(errno));
      if (Physical_IsSync(c->physical)) {
        c->argptr += 2;
        c->arglen -= 2;
      }
    } else if (wrote < 2 && Physical_IsSync(c->physical)) {
      /* Oops - didn't even write our HDLC header ! */
      c->argptr += 2;
      c->arglen -= 2;
    } else {
      c->argptr += wrote;
      c->arglen -= wrote;
    }
  }
}

void
chat_Init(struct chat *c, struct physical *p, const char *data, int emptybuf)
{
  c->desc.type = CHAT_DESCRIPTOR;
  c->desc.next = NULL;
  c->desc.UpdateSet = chat_UpdateSet;
  c->desc.IsSet = chat_IsSet;
  c->desc.Read = chat_Read;
  c->desc.Write = chat_Write;
  c->physical = p;

  c->state = CHAT_EXPECT;

  if (data == NULL) {
    *c->script = '\0';
    c->argc = 0;
  } else {
    strncpy(c->script, data, sizeof c->script - 1);
    c->script[sizeof c->script - 1] = '\0';
    c->argc =  MakeArgs(c->script, c->argv, VECSIZE(c->argv));
  }

  c->arg = -1;
  c->argptr = NULL;
  c->nargptr = NULL;

  if (emptybuf)
    c->bufstart = c->bufend = c->buf;

  c->TimeoutSec = 30;
  c->TimedOut = 0;
  c->numaborts = 0;

  StopTimer(&c->pause);
  c->pause.state = TIMER_STOPPED;

  StopTimer(&c->timeout);
  c->timeout.state = TIMER_STOPPED;
}

void
chat_Destroy(struct chat *c)
{
  while (c->numaborts)
    free(c->AbortStrings[--c->numaborts]);
}

static char *
findblank(char *p, int instring)
{
  if (instring) {
    while (*p) {
      if (*p == '\\') {
	strcpy(p, p + 1);
	if (!*p)
	  break;
      } else if (*p == '"')
	return (p);
      p++;
    }
  } else {
    while (*p) {
      if (issep(*p))
	return (p);
      p++;
    }
  }
  return p;
}

int
MakeArgs(char *script, char **pvect, int maxargs)
{
  int nargs, nb;
  int instring;

  nargs = 0;
  while (*script) {
    nb = strspn(script, " \t");
    script += nb;
    if (*script) {
      if (*script == '"') {
	instring = 1;
	script++;
	if (*script == '\0')
	  break;		/* Shouldn't return here. Need to null
				 * terminate below */
      } else
	instring = 0;
      if (nargs >= maxargs - 1)
	break;
      *pvect++ = script;
      nargs++;
      script = findblank(script, instring);
      if (*script)
	*script++ = '\0';
    }
  }
  *pvect = NULL;
  return nargs;
}

/*
 *  \c	don't add a cr
 *  \d  Sleep a little (delay 2 seconds
 *  \n  Line feed character
 *  \P  Auth Key password
 *  \p  pause 0.25 sec
 *  \r	Carrige return character
 *  \s  Space character
 *  \T  Telephone number(s) (defined via `set phone')
 *  \t  Tab character
 *  \U  Auth User
 */
char *
chat_ExpandString(struct chat *c, const char *str, char *result, int reslen,
                  int sendmode)
{
  int addcr = 0;
  char *phone;

  result[--reslen] = '\0';
  if (sendmode)
    addcr = 1;
  while (*str && reslen > 0) {
    switch (*str) {
    case '\\':
      str++;
      switch (*str) {
      case 'c':
	if (sendmode)
	  addcr = 0;
	break;
      case 'd':		/* Delay 2 seconds */
        if (c != NULL)
          chat_Pause(c, 2 * SECTICKS);
	break;
      case 'p':
        if (c != NULL)
          chat_Pause(c, SECTICKS / 4);
	break;			/* Pause 0.25 sec */
      case 'n':
	*result++ = '\n';
	reslen--;
	break;
      case 'r':
	*result++ = '\r';
	reslen--;
	break;
      case 's':
	*result++ = ' ';
	reslen--;
	break;
      case 't':
	*result++ = '\t';
	reslen--;
	break;
      case 'P':
	strncpy(result, VarAuthKey, reslen);
	reslen -= strlen(result);
	result += strlen(result);
	break;
      case 'T':
	if (VarAltPhone == NULL) {
	  if (VarNextPhone == NULL) {
	    strncpy(VarPhoneCopy, VarPhoneList, sizeof VarPhoneCopy - 1);
	    VarPhoneCopy[sizeof VarPhoneCopy - 1] = '\0';
	    VarNextPhone = VarPhoneCopy;
	  }
	  VarAltPhone = strsep(&VarNextPhone, ":");
	}
	phone = strsep(&VarAltPhone, "|");
	strncpy(result, phone, reslen);
	reslen -= strlen(result);
	result += strlen(result);
	prompt_Printf(&prompt, "Phone: %s\n", phone);
	LogPrintf(LogPHASE, "Phone: %s\n", phone);
	break;
      case 'U':
	strncpy(result, VarAuthName, reslen);
	reslen -= strlen(result);
	result += strlen(result);
	break;
      default:
	reslen--;
	*result++ = *str;
	break;
      }
      if (*str)
	str++;
      break;
    case '^':
      str++;
      if (*str) {
	*result++ = *str++ & 0x1f;
	reslen--;
      }
      break;
    default:
      *result++ = *str++;
      reslen--;
      break;
    }
  }
  if (--reslen > 0) {
    if (addcr)
      *result++ = '\r';
  }
  if (--reslen > 0)
    *result++ = '\0';
  return (result);
}

static void
ExecStr(struct physical *physical, char *command, char *out, int olen)
{
  pid_t pid;
  int fids[2];
  char *vector[MAXARGS], *startout, *endout;
  int stat, nb;

  LogPrintf(LogCHAT, "Exec: %s\n", command);
  MakeArgs(command, vector, VECSIZE(vector));

  if (pipe(fids) < 0) {
    LogPrintf(LogCHAT, "Unable to create pipe in ExecStr: %s\n",
	      strerror(errno));
    *out = '\0';
    return;
  }
  if ((pid = fork()) == 0) {
    TermTimerService();
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTERM, SIG_DFL);
    signal(SIGHUP, SIG_DFL);
    signal(SIGALRM, SIG_DFL);
	/* XXX-ML This looks like it might need more encapsulation. */
    if (Physical_GetFD(physical) == 2) {
	   Physical_DupAndClose(physical);
    }
    close(fids[0]);
    dup2(fids[1], 2);
    close(fids[1]);
    dup2(Physical_GetFD(physical), 0);
    dup2(Physical_GetFD(physical), 1);
    if ((nb = open("/dev/tty", O_RDWR)) > 3) {
      dup2(nb, 3);
      close(nb);
    }
    setuid(geteuid());
    execvp(vector[0], vector);
    fprintf(stderr, "execvp failed: %s: %s\n", vector[0], strerror(errno));
    exit(127);
  } else {
    char *name = strdup(vector[0]);

    close(fids[1]);
    endout = out + olen - 1;
    startout = out;
    while (out < endout) {
      nb = read(fids[0], out, 1);
      if (nb <= 0)
	break;
      out++;
    }
    *out = '\0';
    close(fids[0]);
    close(fids[1]);
    waitpid(pid, &stat, WNOHANG);
    if (WIFSIGNALED(stat)) {
      LogPrintf(LogWARN, "%s: signal %d\n", name, WTERMSIG(stat));
      free(name);
      *out = '\0';
      return;
    } else if (WIFEXITED(stat)) {
      switch (WEXITSTATUS(stat)) {
        case 0:
          free(name);
          break;
        case 127:
          LogPrintf(LogWARN, "%s: %s\n", name, startout);
          free(name);
          *out = '\0';
          return;
          break;
        default:
          LogPrintf(LogWARN, "%s: exit %d\n", name, WEXITSTATUS(stat));
          free(name);
          *out = '\0';
          return;
          break;
      }
    } else {
      LogPrintf(LogWARN, "%s: Unexpected exit result\n", name);
      free(name);
      *out = '\0';
      return;
    }
  }
}
