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
 *	$Id$
 */

#include <sys/param.h>
#include <netinet/in.h>

#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>

#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "loadalias.h"
#include "defs.h"
#include "vars.h"

static const char *LogNames[] = {
  "Async",
  "Carrier",
  "CCP",
  "Chat",
  "Command",
  "Connect",
  "Debug",
  "HDLC",
  "ID0",
  "IPCP",
  "LCP",
  "Link",
  "LQM",
  "Phase",
  "TCP/IP",
  "Tun",
  "Warning",
  "Error",
  "Alert"
};

#define MSK(n) (1<<((n)-1))

static u_long LogMask = MSK(LogLINK) | MSK(LogCARRIER) | MSK(LogPHASE);
static u_long LogMaskLocal = MSK(LogERROR) | MSK(LogALERT) | MSK(LogWARN);
static int LogTunno = -1;

static int
syslogLevel(int lev)
{
  switch (lev) {
    case LogDEBUG:return LOG_DEBUG;
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
LogName(int id)
{
  return id < LogMIN || id > LogMAX ? "Unknown" : LogNames[id - 1];
}

void
LogKeep(int id)
{
  if (id >= LogMIN && id <= LogMAXCONF)
    LogMask |= MSK(id);
}

void
LogKeepLocal(int id)
{
  if (id >= LogMIN && id <= LogMAXCONF)
    LogMaskLocal |= MSK(id);
}

void
LogDiscard(int id)
{
  if (id >= LogMIN && id <= LogMAXCONF)
    LogMask &= ~MSK(id);
}

void
LogDiscardLocal(int id)
{
  if (id >= LogMIN && id <= LogMAXCONF)
    LogMaskLocal &= ~MSK(id);
}

void
LogDiscardAll()
{
  LogMask = 0;
}

void
LogDiscardAllLocal()
{
  LogMaskLocal = 0;
}

int
LogIsKept(int id)
{
  if (id < LogMIN || id > LogMAX)
    return 0;
  if (id > LogMAXCONF)
    return LOG_KEPT_LOCAL | LOG_KEPT_SYSLOG;

  return ((LogMaskLocal & MSK(id)) ? LOG_KEPT_LOCAL : 0) |
    ((LogMask & MSK(id)) ? LOG_KEPT_SYSLOG : 0);
}

void
LogOpen(const char *Name)
{
  openlog(Name, LOG_PID, LOG_DAEMON);
}

void
LogSetTun(int tunno)
{
  LogTunno = tunno;
}

void
LogClose()
{
  closelog();
  LogTunno = -1;
}

void
LogPrintf(int lev, const char *fmt,...)
{
  va_list ap;

  va_start(ap, fmt);
  if (LogIsKept(lev)) {
    static char nfmt[200];

    if ((LogIsKept(lev) & LOG_KEPT_LOCAL) && VarTerm) {
      if ((LogIsKept(LogTUN) & LOG_KEPT_LOCAL) && LogTunno != -1)
        snprintf(nfmt, sizeof nfmt, "tun%d: %s: %s",
	         LogTunno, LogName(lev), fmt);
      else
        snprintf(nfmt, sizeof nfmt, "%s: %s", LogName(lev), fmt);
      vfprintf(VarTerm, nfmt, ap);
      fflush(VarTerm);
    }

    if ((LogIsKept(lev) & LOG_KEPT_SYSLOG) && (lev != LogWARN || !VarTerm)) {
      if ((LogIsKept(LogTUN) & LOG_KEPT_SYSLOG) && LogTunno != -1)
        snprintf(nfmt, sizeof nfmt, "tun%d: %s: %s",
	         LogTunno, LogName(lev), fmt);
      else
        snprintf(nfmt, sizeof nfmt, "%s: %s", LogName(lev), fmt);
      vsyslog(syslogLevel(lev), nfmt, ap);
    }
  }
  va_end(ap);
}

void
LogDumpBp(int lev, const char *hdr, const struct mbuf * bp)
{
  if (LogIsKept(lev)) {
    char buf[50];
    char *b;
    u_char *ptr;
    int f;

    if (hdr && *hdr)
      LogPrintf(lev, "%s\n", hdr);

    b = buf;
    do {
      f = bp->cnt;
      ptr = MBUF_CTOP(bp);
      while (f--) {
	sprintf(b, " %02x", (int) *ptr++);
        b += 3;
        if (b == buf + sizeof buf - 2) {
          strcpy(b, "\n");
          LogPrintf(lev, buf);
          b = buf;
        }
      }
    } while ((bp = bp->next) != NULL);

    if (b > buf) {
      strcpy(b, "\n");
      LogPrintf(lev, buf);
    }
  }
}

void
LogDumpBuff(int lev, const char *hdr, const u_char * ptr, int n)
{
  if (LogIsKept(lev)) {
    char buf[50];
    char *b;

    if (hdr && *hdr)
      LogPrintf(lev, "%s\n", hdr);
    while (n > 0) {
      b = buf;
      for (b = buf; b != buf + sizeof buf - 2 && n--; b += 3)
	sprintf(b, " %02x", (int) *ptr++);
      strcpy(b, "\n");
      LogPrintf(lev, buf);
    }
  }
}
