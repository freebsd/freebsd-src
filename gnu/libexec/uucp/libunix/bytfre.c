/* bytfre.c
   Get the number of bytes free on a file system.  */

#include "uucp.h"

#include "system.h"
#include "sysdep.h"
#include "fsusg.h"

long
csysdep_bytes_free (zfile)
     const char *zfile;
{
  struct fs_usage s;

  if (get_fs_usage ((char *) zfile, (char *) NULL, &s) < 0)
    return -1;
  return s.fsu_bavail * (long) 512;
}
