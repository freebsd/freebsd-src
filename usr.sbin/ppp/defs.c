/*
 * $Id: $
 */

#include <stdlib.h>

#include "defs.h"

int mode = MODE_INTER;
int BGFiledes[2] = { -1, -1 };
int modem = -1;
int tun_in = -1;
int tun_out = -1;
int netfd = -1;
char *dstsystem = NULL;

void
randinit()
{
  static int initdone;

  if (!initdone) {
    initdone = 1;
    srandomdev();
  }
}
