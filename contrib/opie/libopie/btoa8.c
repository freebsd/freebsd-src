/* btoa8.c: The opiebtoa8() library function.

%%% copyright-cmetz-96
This software is Copyright 1996-1998 by Craig Metz, All Rights Reserved.
The Inner Net License Version 2 applies to this software.
You should have received a copy of the license with this software. If
you didn't get a copy, you may request one from <license@inner.net>.

        History:

	Created by cmetz for OPIE 2.3 (quick re-write).
*/      

#include "opie_cfg.h"
#include "opie.h"

static char hextochar[16] = 
{'0','1','2','3','4','5','6','7','8','9','a','b','c','d','e','f'};

char *opiebtoa8 FUNCTION((out, in), char *out AND char *in)
{
  int i;
  char *c = out;

  for (i = 0; i < 8; i++) {
    *(c++) = hextochar[((*in) >> 4) & 0x0f];
    *(c++) = hextochar[(*in++) & 0x0f];
  }
  *c = 0;

  return out;
}
