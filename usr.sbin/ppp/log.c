/*
 * $Id: $
 */

#include <sys/param.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>

#include "mbuf.h"
#include "log.h"
#include "loadalias.h"
#include "command.h"
#include "vars.h"

static char *LogNames[] = {
  "Async",
  "Carrier",
  "CCP",
  "Chat",
  "Command",
  "Connect",
  "Debug",
  "HDLC",
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
LogDiscard(int id)
{
  if (id >= LogMIN && id <= LogMAXCONF)
    LogMask &= ~MSK(id);
}

void
LogDiscardAll()
{
  LogMask = 0;
}

int
LogIsKept(int id)
{
  if (id < LogMIN)
    return 0;
  if (id <= LogMAXCONF)
    return LogMask & MSK(id);
  return id <= LogMAX;
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
LogPrintf(int lev, char *fmt,...)
{
  va_list ap;

  va_start(ap, fmt);
  if (LogIsKept(lev)) {
    static char nfmt[200];

    if (LogIsKept(LogTUN) && LogTunno != -1)
      snprintf(nfmt, sizeof nfmt, "tun%d: %s: %s",
	       LogTunno, LogName(lev), fmt);
    else
      snprintf(nfmt, sizeof nfmt, "%s: %s", LogName(lev), fmt);
    if ((lev == LogERROR || lev == LogALERT || lev == LogWARN) && VarTerm)
      vfprintf(VarTerm, fmt, ap);
    if (lev != LogWARN || !VarTerm)
      vsyslog(syslogLevel(lev), nfmt, ap);
  }
  va_end(ap);
}

void
LogDumpBp(int lev, char *hdr, struct mbuf * bp)
{
  if (LogIsKept(lev)) {
    char buf[49];
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
        if (b == buf + sizeof buf - 1) {
          LogPrintf(lev, buf);
          b = buf;
        }
      }
    } while ((bp = bp->next) != NULL);

    if (b > buf)
      LogPrintf(lev, buf);
  }
}

void
LogDumpBuff(int lev, char *hdr, u_char * ptr, int n)
{
  if (LogIsKept(lev)) {
    char buf[49];
    char *b;
    int f;

    if (hdr && *hdr)
      LogPrintf(lev, "%s\n", hdr);
    while (n > 0) {
      b = buf;
      for (f = 0; f < 16 && n--; f++, b += 3)
	sprintf(b, " %02x", (int) *ptr++);
      LogPrintf(lev, buf);
    }
  }
}
