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
 *	$Id: defs.c,v 1.11.2.1 1998/01/26 20:04:34 brian Exp $
 */

#include <sys/param.h>
#include <netinet/in.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "defs.h"
#include "command.h"
#include "mbuf.h"
#include "log.h"
#include "loadalias.h"
#include "vars.h"

int mode = MODE_INTER;
int BGFiledes[2] = { -1, -1 };
int modem = -1;
int tun_in = -1;
int tun_out = -1;
int netfd = -1;

static char dstsystem[50];

void
SetLabel(const char *label)
{
  if (label)
    strncpy(dstsystem, label, sizeof dstsystem - 1);
  else
    *dstsystem = '\0';
}

const char *
GetLabel()
{
  return *dstsystem ? dstsystem : NULL;
}

void
randinit()
{
#ifdef __FreeBSD__
  static int initdone;

  if (!initdone) {
    initdone = 1;
    srandomdev();
  }
#else
  srandom(time(NULL)^getpid());
#endif
}


int
GetShortHost()
{
  char *p;

  if (gethostname(VarShortHost, sizeof VarShortHost)) {
    LogPrintf(LogERROR, "GetShortHost: gethostname: %s\n", strerror(errno));
    return 0;
  }

  if ((p = strchr(VarShortHost, '.')))
    *p = '\0';

  return 1;
}

void
DropClient(int verbose)
{
  FILE *oVarTerm;

  if (VarTerm && !(mode & MODE_INTER)) {
    oVarTerm = VarTerm;
    VarTerm = 0;
    if (oVarTerm)
      fclose(oVarTerm);
    close(netfd);
    netfd = -1;
    if (verbose)
      LogPrintf(LogPHASE, "Client connection dropped.\n");
  }
}
