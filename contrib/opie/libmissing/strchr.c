/* strchr.c: A replacement for the strchr function

%%% copyright-cmetz
This software is Copyright 1996 by Craig Metz, All Rights Reserved.
The Inner Net License Version 2 applies to this software.
You should have received a copy of the license with this software. If
you didn't get a copy, you may request one from <license@inner.net>.

        History:

	Created by cmetz for OPIE 2.3.
*/
#include "opie_cfg.h"
#include "opie.h"

char *strchr FUNCTION((s, c), char *s AND int c)
{
#if HAVE_INDEX
  return index(s, c);
#else /* HAVE_INDEX */
  while(*s && (*s != c)) s++;
  return *s ? s : (char *)0;
#endif /* HAVE_INDEX */
}
