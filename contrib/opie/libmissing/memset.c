/* memcpy.c: A replacement for the memcpy function

%%% copyright-cmetz
This software is Copyright 1996 by Craig Metz, All Rights Reserved.
The Inner Net License Version 2 applies to this software.
You should have received a copy of the license with this software. If
you didn't get a copy, you may request one from <license@inner.net>.

        History:

	Created by cmetz for OPIE 2.2.
*/
#include "opie_cfg.h"
#include "opie.h"

VOIDPTR *memset FUNCTION((d, v, n), unsigned char *d AND int v AND int n)
{
	unsigned char *d2 = d;
	while(n--) (*d2++) = (unsigned char)v;
	return d;
}
