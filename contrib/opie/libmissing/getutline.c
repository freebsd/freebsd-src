/* getutline.c: A replacement for the getutline() function

%%% copyright-cmetz
This software is Copyright 1996 by Craig Metz, All Rights Reserved.
The Inner Net License Version 2 applies to this software.
You should have received a copy of the license with this software. If
you didn't get a copy, you may request one from <license@inner.net>.

        History:

	Created by cmetz for OPIE 2.3.
*/

#include "opie_cfg.h"
#include <stdio.h>
#include <utmp.h>
#include "opie.h"

static struct utmp u;

struct utmp *getutline FUNCTION((utmp), struct utmp *utmp)
{
  FILE *f;
  int i;

  if (!(f = __opieopen(_PATH_UTMP, 0, 0644)))
    return 0;

#if HAVE_TTYSLOT
  if (i = ttyslot()) {
    if (fseek(f, i * sizeof(struct utmp), SEEK_SET) < 0)
      goto ret;
    if (fread(&u, sizeof(struct utmp), 1, f) != sizeof(struct utmp))
      goto ret;
    fclose(f);
    return &u;
  }
#endif /* HAVE_TTYSLOT */

  while(fread(&u, sizeof(struct utmp), 1, f) == sizeof(struct utmp)) {
    if (!strncmp(utmp->ut_line, u.ut_line, sizeof(u.ut_line) - 1)) {
      fclose(f);
      return &u;
    }
  }

ret:
  fclose(f);
  return NULL;
}
