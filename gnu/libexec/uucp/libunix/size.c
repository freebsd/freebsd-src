/* size.c
   Get the size in bytes of a file.  */

#include "uucp.h"

#include "uudefs.h"
#include "sysdep.h"
#include "system.h"

#include <errno.h>

long
csysdep_size (zfile)
     const char *zfile;
{
  struct stat s;

  if (stat ((char *) zfile, &s) < 0)
    {
      if (errno == ENOENT)
	return -1;
      ulog (LOG_ERROR, "stat (%s): %s", zfile, strerror (errno));
      return -2;
    }

  return s.st_size;
}
