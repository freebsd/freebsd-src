/* bytfre.c
   Get the number of bytes free on a file system.  */

#include "uucp.h"

#include "system.h"
#include "sysdep.h"
#include "fsusg.h"

#if HAVE_LIMITS_H
#include <limits.h>
#else
#define LONG_MAX 2147483647
#endif

long
csysdep_bytes_free (zfile)
     const char *zfile;
{
  struct fs_usage s;

  if (get_fs_usage ((char *) zfile, (char *) NULL, &s) < 0)
    return -1;
  if (s.fsu_bavail >= LONG_MAX / (long) 512)
    return LONG_MAX;
  return s.fsu_bavail * (long) 512;
}
