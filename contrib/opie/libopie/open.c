/* open.c: The __opieopen() library function.

%%% copyright-cmetz-96
This software is Copyright 1996-1998 by Craig Metz, All Rights Reserved.
The Inner Net License Version 2 applies to this software.
You should have received a copy of the license with this software. If
you didn't get a copy, you may request one from <license@inner.net>.

	History:

	Created by cmetz for OPIE 2.3.
*/
#include "opie_cfg.h"

#include <stdio.h>
#include <sys/types.h>
#if HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <sys/stat.h>
#include <errno.h>

#include "opie.h"

#if !HAVE_LSTAT
#define lstat(x, y) stat(x, y)
#endif /* !HAVE_LSTAT */

FILE *__opieopen FUNCTION((file, rw, mode), char *file AND int rw AND int mode)
{
  FILE *f;
  struct stat st;

  if (lstat(file, &st)) {
    if (errno != ENOENT)
      return NULL;

    if (!(f = fopen(file, "w")))
      return NULL;

    fclose(f);

    if (chmod(file, mode))
      return NULL;

    if (lstat(file, &st))
      return NULL;
  }

  if (!S_ISREG(st.st_mode))
    return NULL;

  {
    char *fmodes[] = { "r", "r+", "a" };

    if (!(f = fopen(file, fmodes[rw])))
      return NULL;
  }

  return f;
}
