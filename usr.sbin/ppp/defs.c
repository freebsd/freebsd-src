/*
 * $Id: defs.c,v 1.4 1997/11/18 00:19:30 brian Exp $
 */

#include <sys/param.h>
#include <netinet/in.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
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
    strncpy(dstsystem, label, sizeof dstsystem);
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

  if (gethostname(VarShortHost, sizeof(VarShortHost))) {
    LogPrintf(LogERROR, "GetShortHost: gethostbyname: %s\n", strerror(errno));
    return 0;
  }

  if ((p = strchr(VarShortHost, '.')))
    *p = '\0';

  return 1;
}

void
DropClient()
{
  FILE *oVarTerm;

  if (VarTerm && !(mode & MODE_INTER)) {
    oVarTerm = VarTerm;
    VarTerm = 0;
    if (oVarTerm)
      fclose(oVarTerm);
    close(netfd);
    netfd = -1;
    LogPrintf(LogPHASE, "Client connection closed.\n");
  }
}
