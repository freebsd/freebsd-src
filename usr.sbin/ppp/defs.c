/*
 * $Id: defs.c,v 1.1 1997/10/26 01:02:30 brian Exp $
 */

#include <stdlib.h>
#include <string.h>

#include "defs.h"

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
  static int initdone;

  if (!initdone) {
    initdone = 1;
    srandomdev();
  }
}
