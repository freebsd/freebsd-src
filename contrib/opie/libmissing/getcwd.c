/* getcwd.c: A replacement for the getcwd function

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

char *getcwd FUNCTION((c, l), char *c AND int l)
{
#if HAVE_GETWD
  return getwd(c);
#else /* HAVE_INDEX */
#error Need getwd() to build a replacement getcwd()
#endif /* HAVE_INDEX */
}
