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

VOIDPTR *memcpy FUNCTION((d, s, n), unsigned char *d AND unsigned char *s AND int n)
{
#if HAVE_BCOPY
	bcopy(s, d, n);
#else /* HAVE_BCOPY */
	char *d2 = d;
	while(n--) (*d2++) = (*s++);
#endif /* HAVE_BCOPY */
	return d;
}
