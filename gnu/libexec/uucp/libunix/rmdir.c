/* rmdir.c
   Remove a directory on a system which doesn't have the rmdir system
   call.  This is only called by uupick, which is not setuid, so we
   don't have to worry about the problems of invoking the setuid
   /bin/rmdir program.  */

#include "uucp.h"

#include "sysdep.h"

#include <errno.h>

int
rmdir (zdir)
     const char *zdir;
{
  const char *azargs[3];
  int aidescs[3];
  pid_t ipid;

  azargs[0] = RMDIR_PROGRAM;
  azargs[1] = zdir;
  azargs[2] = NULL;
  aidescs[0] = SPAWN_NULL;
  aidescs[1] = SPAWN_NULL;
  aidescs[2] = SPAWN_NULL;

  ipid = ixsspawn (azargs, aidescs, TRUE, FALSE, (const char *) NULL,
		   TRUE, TRUE, (const char *) NULL,
		   (const char *) NULL, (const char *) NULL);

  if (ipid < 0)
    return -1;

  if (ixswait ((unsigned long) ipid, (const char *) NULL) != 0)
    {
      /* Make up an errno value.  */
      errno = EBUSY;
      return -1;
    }

  return 0;
}
