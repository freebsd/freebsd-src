/* exists.c
   Check whether a file exists.  */

#include "uucp.h"

#include "sysdep.h"
#include "system.h"

boolean
fsysdep_file_exists (zfile)
     const char *zfile;
{
  struct stat s;

  return stat ((char *) zfile, &s) == 0;
}
