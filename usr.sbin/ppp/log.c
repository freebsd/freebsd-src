/*-
 * Copyright (c) 1997 Brian Somers <brian@Awfulhak.org>
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
 *	$Id: log.c,v 1.25.2.14 1998/05/01 22:39:35 brian Exp $
 */

#include <sys/types.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <termios.h>

#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "descriptor.h"
#include "prompt.h"

static const char *LogNames[] = {
  "Async",
  "CCP",
  "Chat",
  "Command",
  "Connect",
  "Debug",
  "HDLC",
  "ID0",
  "IPCP",
  "LCP",
  "LQM",
  "Phase",
  "TCP/IP",
  "Timer",
  "Tun",
  "Warning",
  "Error",
  "Alert"
};

#define MSK(n) (1<<((n)-1))

static u_long LogMask = MSK(LogPHASE);
static u_long LogMaskLocal = MSK(LogERROR) | MSK(LogALERT) | MSK(LogWARN);
static int LogTunno = -1;
static struct prompt *logprompt;	/* Where to log local stuff */

void
log_RegisterPrompt(struct prompt *prompt)
{
  if (prompt) {
    prompt->lognext = logprompt;
    logprompt = prompt;
    LogMaskLocal |= prompt->logmask;
  }
}

static void
LogSetMaskLocal(void)
{
  struct prompt *p;

  LogMaskLocal = MSK(LogERROR) | MSK(LogALERT) | MSK(LogWARN);
  for (p = logprompt; p; p = p->lognext)
    LogMaskLocal |= p->logmask;
}

void
log_UnRegisterPrompt(struct prompt *prompt)
{
  if (prompt) {
    struct prompt **p;

    for (p = &logprompt; *p; p = &(*p)->lognext)
      if (*p == prompt) {
        *p = prompt->lognext;
        prompt->lognext = NULL;
        break;
      }
    LogSetMaskLocal();
  }
}

static int
syslogLevel(int lev)
{
  switch (lev) {
  case LogDEBUG:
  case LogTIMER:
    return LOG_DEBUG;
  case LogWARN:
    return LOG_WARNING;
  case LogERROR:
    return LOG_ERR;
  case LogALERT:
    return LOG_ALERT;
  }
  return lev >= LogMIN && lev <= LogMAX ? LOG_INFO : 0;
}

const char *
log_Name(int id)
{
  return id < LogMIN || id > LogMAX ? "Unknown" : LogNames[id - 1];
}

void
log_Keep(int id)
{
  if (id >= LogMIN && id <= LogMAXCONF)
    LogMask |= MSK(id);
}

void
log_KeepLocal(int id, u_long *mask)
{
  if (id >= LogMIN && id <= LogMAXCONF) {
    LogMaskLocal |= MSK(id);
    *mask |= MSK(id);
  }
}

void
log_Discard(int id)
{
  if (id >= LogMIN && id <= LogMAXCONF)
    LogMask &= ~MSK(id);
}

void
log_DiscardLocal(int id, u_long *mask)
{
  if (id >= LogMIN && id <= LogMAXCONF) {
    *mask &= ~MSK(id);
    LogSetMaskLocal();
  }
}

void
log_DiscardAll()
{
  LogMask = 0;
}

void
log_DiscardAllLocal(u_long *mask)
{
  *mask = MSK(LogERROR) | MSK(LogALERT) | MSK(LogWARN);
  LogSetMaskLocal();
}

int
log_IsKept(int id)
{
  if (id < LogMIN || id > LogMAX)
    return 0;
  if (id > LogMAXCONF)
    return LOG_KEPT_LOCAL | LOG_KEPT_SYSLOG;

  return ((LogMaskLocal & MSK(id)) ? LOG_KEPT_LOCAL : 0) |
    ((LogMask & MSK(id)) ? LOG_KEPT_SYSLOG : 0);
}

int
log_IsKeptLocal(int id, u_long mask)
{
  if (id < LogMIN || id > LogMAX)
    return 0;
  if (id > LogMAXCONF)
    return LOG_KEPT_LOCAL | LOG_KEPT_SYSLOG;

  return ((mask & MSK(id)) ? LOG_KEPT_LOCAL : 0) |
    ((LogMask & MSK(id)) ? LOG_KEPT_SYSLOG : 0);
}

void
log_Open(const char *Name)
{
  openlog(Name, LOG_PID, LOG_DAEMON);
}

void
log_SetTun(int tunno)
{
  LogTunno = tunno;
}

void
log_Close()
{
  closelog();
  LogTunno = -1;
}

void
log_Printf(int lev, const char *fmt,...)
{
  va_list ap;
  struct prompt *prompt;

  va_start(ap, fmt);
  if (log_IsKept(lev)) {
    static char nfmt[200];

    if ((log_IsKept(lev) & LOG_KEPT_LOCAL) && logprompt) {
      if ((log_IsKept(LogTUN) & LOG_KEPT_LOCAL) && LogTunno != -1)
        snprintf(nfmt, sizeof nfmt, "tun%d: %s: %s",
	         LogTunno, log_Name(lev), fmt);
      else
        snprintf(nfmt, sizeof nfmt, "%s: %s", log_Name(lev), fmt);
  
      for (prompt = logprompt; prompt; prompt = prompt->lognext)
        if (lev > LogMAXCONF || (prompt->logmask & MSK(lev)))
          prompt_vPrintf(prompt, nfmt, ap);
    }

    if ((log_IsKept(lev) & LOG_KEPT_SYSLOG) && (lev != LogWARN || !logprompt)) {
      if ((log_IsKept(LogTUN) & LOG_KEPT_SYSLOG) && LogTunno != -1)
        snprintf(nfmt, sizeof nfmt, "tun%d: %s: %s",
	         LogTunno, log_Name(lev), fmt);
      else
        snprintf(nfmt, sizeof nfmt, "%s: %s", log_Name(lev), fmt);
      vsyslog(syslogLevel(lev), nfmt, ap);
    }
  }
  va_end(ap);
}

void
log_DumpBp(int lev, const char *hdr, const struct mbuf * bp)
{
  if (log_IsKept(lev)) {
    char buf[50];
    char *b;
    u_char *ptr;
    int f;

    if (hdr && *hdr)
      log_Printf(lev, "%s\n", hdr);

    b = buf;
    do {
      f = bp->cnt;
      ptr = MBUF_CTOP(bp);
      while (f--) {
	sprintf(b, " %02x", (int) *ptr++);
        b += 3;
        if (b == buf + sizeof buf - 2) {
          strcpy(b, "\n");
          log_Printf(lev, buf);
          b = buf;
        }
      }
    } while ((bp = bp->next) != NULL);

    if (b > buf) {
      strcpy(b, "\n");
      log_Printf(lev, buf);
    }
  }
}

void
log_DumpBuff(int lev, const char *hdr, const u_char * ptr, int n)
{
  if (log_IsKept(lev)) {
    char buf[50];
    char *b;

    if (hdr && *hdr)
      log_Printf(lev, "%s\n", hdr);
    while (n > 0) {
      b = buf;
      for (b = buf; b != buf + sizeof buf - 2 && n--; b += 3)
	sprintf(b, " %02x", (int) *ptr++);
      strcpy(b, "\n");
      log_Printf(lev, buf);
    }
  }
}

int
log_ShowLevel(struct cmdargs const *arg)
{
  int i;

  prompt_Printf(arg->prompt, "Log:  ");
  for (i = LogMIN; i <= LogMAX; i++)
    if (log_IsKept(i) & LOG_KEPT_SYSLOG)
      prompt_Printf(arg->prompt, " %s", log_Name(i));

  prompt_Printf(arg->prompt, "\nLocal:");
  for (i = LogMIN; i <= LogMAX; i++)
    if (log_IsKeptLocal(i, arg->prompt->logmask) & LOG_KEPT_LOCAL)
      prompt_Printf(arg->prompt, " %s", log_Name(i));

  prompt_Printf(arg->prompt, "\n");

  return 0;
}

int
log_SetLevel(struct cmdargs const *arg)
{
  int i, res, argc, local;
  char const *const *argv, *argp;

  argc = arg->argc - arg->argn;
  argv = arg->argv + arg->argn;
  res = 0;

  if (argc == 0 || strcasecmp(argv[0], "local"))
    local = 0;
  else {
    if (arg->prompt == NULL) {
      log_Printf(LogWARN, "set log local: Only available on the command line\n");
      return 1;
    }
    argc--;
    argv++;
    local = 1;
  }

  if (argc == 0 || (argv[0][0] != '+' && argv[0][0] != '-')) {
    if (local)
      log_DiscardAllLocal(&arg->prompt->logmask);
    else
      log_DiscardAll();
  }

  while (argc--) {
    argp = **argv == '+' || **argv == '-' ? *argv + 1 : *argv;
    for (i = LogMIN; i <= LogMAX; i++)
      if (strcasecmp(argp, log_Name(i)) == 0) {
	if (**argv == '-') {
          if (local)
            log_DiscardLocal(i, &arg->prompt->logmask);
          else
	    log_Discard(i);
	} else if (local)
          log_KeepLocal(i, &arg->prompt->logmask);
        else
          log_Keep(i);
	break;
      }
    if (i > LogMAX) {
      log_Printf(LogWARN, "%s: Invalid log value\n", argp);
      res = -1;
    }
    argv++;
  }
  return res;
}

int
log_ShowWho(struct cmdargs const *arg)
{
  struct prompt *p;

  for (p = logprompt; p; p = p->lognext) {
    prompt_Printf(arg->prompt, "%s (%s)", p->src.type, p->src.from);
    if (p == arg->prompt)
      prompt_Printf(arg->prompt, " *");
    if (!p->active)
      prompt_Printf(arg->prompt, " ^Z");
    prompt_Printf(arg->prompt, "\n");
  }

  return 0;
}
