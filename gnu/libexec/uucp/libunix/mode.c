/* mode.c
   Get the Unix file mode of a file.  */

#include "uucp.h"

#include "uudefs.h"
#include "sysdep.h"
#include "system.h"

#include <errno.h>

unsigned int
ixsysdep_file_mode (zfile)
     const char *zfile;
{
  struct stat s;

  if (stat ((char *) zfile, &s) != 0)
    {
      ulog (LOG_ERROR, "stat (%s): %s", zfile, strerror (errno));
      return 0;
    }

#if S_IRWXU != 0700
 #error Files modes need to be translated
#endif

  /* We can't return 0, since that indicate an error.  */
  if ((s.st_mode & 0777) == 0)
    return 0400;

  return s.st_mode & 0777;
}
