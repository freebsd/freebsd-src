/* basnam.c
   Get the base name of a file.  */

#include "uucp.h"

#include "uudefs.h"
#include "sysdep.h"
#include "system.h"

/* Get the base name of a file name.  */

char *
zsysdep_base_name (zfile)
     const char *zfile;
{
  const char *z;

  z = strrchr (zfile, '/');
  if (z != NULL)
    return zbufcpy (z + 1);
  return zbufcpy (zfile);
}
