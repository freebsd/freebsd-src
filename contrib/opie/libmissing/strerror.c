/* strerror.c: A replacement for the strerror function

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

char *strerror FUNCTION((errnum), int errnum)
{
#if HAVE_SYS_ERRLIST
  extern char *sys_errlist[];
  return sys_errlist[errnum];
#else /* NEED_STRERROR */
#if HAVE__SYS_ERRLIST
  extern char *_sys_errlist[];
  return sys_errlist[errnum];
#else /* HAVE__SYS_ERRLIST */
  static char hexdigits[] = "0123456789abcdef";
  static char buffer[] = "System error 0x42";
  buffer[15] = hexdigits[(errnum >> 4) & 0x0f];
  buffer[16] = hexdigits[errnum & 0x0f];
  return buffer;
#endif /* HAVE__SYS_ERRLIST */
#endif	/* NEED_STRERROR */
}
