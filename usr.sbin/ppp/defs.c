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
 *	$Id: defs.c,v 1.13 1998/05/21 21:45:03 brian Exp $
 */


#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <time.h>
#include <unistd.h>

#include "defs.h"

void
randinit()
{
#if __FreeBSD__ >= 3
  static int initdone;

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
  { PHYS_MANUAL, "interactive" },
  { PHYS_DEMAND, "auto" },
  { PHYS_DIRECT, "direct" },
  { PHYS_DEDICATED, "dedicated" },
  { PHYS_PERM, "ddial" },
  { PHYS_1OFF, "background" },
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
