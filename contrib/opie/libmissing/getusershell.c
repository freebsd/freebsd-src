/* getusershell.c: minimal implementation of the getusershell() and
   endusershell() library routines for systems that don't have them.

%%% portions-copyright-cmetz
Portions of this software are Copyright 1996 by Craig Metz, All Rights
Reserved. The Inner Net License Version 2 applies to these portions of
the software.
You should have received a copy of the license with this software. If
you didn't get a copy, you may request one from <license@inner.net>.

Portions of this software are Copyright 1995 by Randall Atkinson and Dan
McDonald, All Rights Reserved. All Rights under this copyright are assigned
to the U.S. Naval Research Laboratory (NRL). The NRL Copyright Notice and
License Agreement applies to this software.

	History:

	Modified by cmetz for OPIE 2.2. Use FUNCTION declaration et al.
	Modified at NRL for OPIE 2.1. Remove trailing newlines from
	        /etc/shells entries. Fixed infinite loop. Fixed a bug
                where second invocation on would fail.
	Written at NRL for OPIE 2.0.
*/
#include "opie_cfg.h"
#include <stdio.h>
#if HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#include "opie.h"

static FILE *fh = NULL;
static char *internal[] = {"/bin/sh", "/bin/csh", NULL};
static int i = 0;
static char buffer[1024];

char *getusershell FUNCTION_NOARGS
{
  char *c;

  if (!fh)
    fh = fopen("/etc/shells", "r");

  if (fh) {
    if (fgets(buffer, sizeof(buffer), fh)) {
      if (c = strchr(buffer, '\n'))
	*c = 0;
      return buffer;
    } else {
      fclose(fh);
      return NULL;
    }
  } else {
    if (internal[i])
      return internal[i++];
    else
      return NULL;
  }
}

VOIDRET endusershell FUNCTION_NOARGS
{
  if (fh) {
    fclose(fh);
    fh = NULL;
  }
  i = 0;
}
