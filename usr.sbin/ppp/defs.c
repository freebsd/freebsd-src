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
 * $FreeBSD$
 */


#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <ctype.h>
#include <errno.h>
#ifdef __NetBSD__
#include <signal.h>	/* for `errno' ?!? */
#endif
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#if !defined(__FreeBSD__) || __FreeBSD__ < 3
#include <time.h>
#endif
#include <unistd.h>

#include "defs.h"

#define	issep(c)	((c) == '\t' || (c) == ' ')

void
randinit()
{
#if __FreeBSD__ >= 3
  static int initdone;		/* srandomdev() call is only required once */

  if (!initdone) {
    initdone = 1;
    srandomdev();
  }
#else
  srandom((time(NULL)^getpid())+random());
#endif
}

ssize_t
fullread(int fd, void *v, size_t n)
{
  size_t got, total;

  for (total = 0; total < n; total += got)
    switch ((got = read(fd, (char *)v + total, n - total))) {
      case 0:
        return total;
      case -1:
        if (errno == EINTR)
          got = 0;
        else
          return -1;
    }
  return total;
}

static struct {
  int mode;
  const char *name;
} modes[] = {
  { PHYS_INTERACTIVE, "interactive" },
  { PHYS_AUTO, "auto" },
  { PHYS_DIRECT, "direct" },
  { PHYS_DEDICATED, "dedicated" },
  { PHYS_DDIAL, "ddial" },
  { PHYS_BACKGROUND, "background" },
  { PHYS_ALL, "*" },
  { 0, 0 }
};

const char *
mode2Nam(int mode)
{
  int m;

  for (m = 0; modes[m].mode; m++)
    if (modes[m].mode == mode)
      return modes[m].name;

  return "unknown";
}

int
Nam2mode(const char *name)
{
  int m, got, len;

  len = strlen(name);
  got = -1;
  for (m = 0; modes[m].mode; m++)
    if (!strncasecmp(name, modes[m].name, len)) {
      if (modes[m].name[len] == '\0')
	return modes[m].mode;
      if (got != -1)
        return 0;
      got = m;
    }

  return got == -1 ? 0 : modes[got].mode;
}

struct in_addr
GetIpAddr(const char *cp)
{
  struct in_addr ipaddr;

  if (!strcasecmp(cp, "default"))
    ipaddr.s_addr = INADDR_ANY;
  else if (inet_aton(cp, &ipaddr) == 0) {
    const char *ptr;

    /* Any illegal characters ? */
    for (ptr = cp; *ptr != '\0'; ptr++)
      if (!isalnum(*ptr) && strchr("-.", *ptr) == NULL)
        break;

    if (*ptr == '\0') {
      struct hostent *hp;

      hp = gethostbyname(cp);
      if (hp && hp->h_addrtype == AF_INET)
        memcpy(&ipaddr, hp->h_addr, hp->h_length);
      else
        ipaddr.s_addr = INADDR_NONE;
    } else
      ipaddr.s_addr = INADDR_NONE;
  }

  return ipaddr;
}

static const struct speeds {
  int nspeed;
  speed_t speed;
} speeds[] = {
#ifdef B50
  { 50, B50, },
#endif
#ifdef B75
  { 75, B75, },
#endif
#ifdef B110
  { 110, B110, },
#endif
#ifdef B134
  { 134, B134, },
#endif
#ifdef B150
  { 150, B150, },
#endif
#ifdef B200
  { 200, B200, },
#endif
#ifdef B300
  { 300, B300, },
#endif
#ifdef B600
  { 600, B600, },
#endif
#ifdef B1200
  { 1200, B1200, },
#endif
#ifdef B1800
  { 1800, B1800, },
#endif
#ifdef B2400
  { 2400, B2400, },
#endif
#ifdef B4800
  { 4800, B4800, },
#endif
#ifdef B9600
  { 9600, B9600, },
#endif
#ifdef B19200
  { 19200, B19200, },
#endif
#ifdef B38400
  { 38400, B38400, },
#endif
#ifndef _POSIX_SOURCE
#ifdef B7200
  { 7200, B7200, },
#endif
#ifdef B14400
  { 14400, B14400, },
#endif
#ifdef B28800
  { 28800, B28800, },
#endif
#ifdef B57600
  { 57600, B57600, },
#endif
#ifdef B76800
  { 76800, B76800, },
#endif
#ifdef B115200
  { 115200, B115200, },
#endif
#ifdef B230400
  { 230400, B230400, },
#endif
#ifdef EXTA
  { 19200, EXTA, },
#endif
#ifdef EXTB
  { 38400, EXTB, },
#endif
#endif				/* _POSIX_SOURCE */
  { 0, 0 }
};

int
SpeedToInt(speed_t speed)
{
  const struct speeds *sp;

  for (sp = speeds; sp->nspeed; sp++) {
    if (sp->speed == speed) {
      return sp->nspeed;
    }
  }
  return 0;
}

speed_t
IntToSpeed(int nspeed)
{
  const struct speeds *sp;

  for (sp = speeds; sp->nspeed; sp++) {
    if (sp->nspeed == nspeed) {
      return sp->speed;
    }
  }
  return B0;
}

static char *
findblank(char *p, int instring)
{
  if (instring) {
    while (*p) {
      if (*p == '\\') {
	memmove(p, p + 1, strlen(p));
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
	  break;		/* Shouldn't return here. Need to NULL
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
