/* sysconf.c: A (partial) replacement for the sysconf function

%%% copyright-cmetz
This software is Copyright 1996 by Craig Metz, All Rights Reserved.
The Inner Net License Version 2 applies to this software.
You should have received a copy of the license with this software. If
you didn't get a copy, you may request one from <license@inner.net>.

        History:

	Created by cmetz for OPIE 2.3.
*/
#include "opie_cfg.h"
#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif /* HAVE_SYS_PARAM_H */
#include "opie.h"

long sysconf(int name)
{
  switch(name) {
    case _SC_OPEN_MAX:
#if HAVE_GETDTABLESIZE
      return getdtablesize();
#else /* HAVE_GETDTABLESIZE */
#error Need getdtablesize() to build a replacement sysconf()
#endif /* HAVE_GETDTABLESIZE */

  return -1;
}
