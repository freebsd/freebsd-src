/* getcwd.c
   Replacement for the getcwd function that just calls /bin/pwd.  */

#include "uucp.h"

#include "sysdep.h"

#include <errno.h>

char *
getcwd (zbuf, cbuf)
     char *zbuf;
     size_t cbuf;
{
  const char *azargs[2];
  FILE *e;
  pid_t ipid;
  int cread;
  int ierr;

  azargs[0] = PWD_PROGRAM;
  azargs[1] = NULL;
  e = espopen (azargs, TRUE, &ipid);
  if (e == NULL)
    return NULL;

  ierr = 0;

  cread = fread (zbuf, sizeof (char), cbuf, e);
  if (cread == 0)
    ierr = errno;

  (void) fclose (e);

  if (ixswait ((unsigned long) ipid, (const char *) NULL) != 0)
    {
      ierr = EACCES;
      cread = 0;
    }

  if (cread != 0)
    {
      if (zbuf[cread - 1] == '\n')
	zbuf[cread - 1] = '\0';
      else
	{
	  ierr = ERANGE;
	  cread = 0;
	}
    }
  
  if (cread == 0)
    {
      errno = ierr;
      return NULL;
    }

  return zbuf;
}
