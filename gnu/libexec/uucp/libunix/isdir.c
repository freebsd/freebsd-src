/* isdir.c
   See whether a file exists and is a directory.  */

#include "uucp.h"

#include "system.h"
#include "sysdep.h"

boolean
fsysdep_directory (z)
     const char *z;
{
  struct stat s;

  if (stat ((char *) z, &s) < 0)
    return FALSE;
  return S_ISDIR (s.st_mode);
}
