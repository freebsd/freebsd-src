/* chmod.c
   Change the mode of a file.  */

#include "uucp.h"

#include "uudefs.h"
#include "sysdep.h"
#include "system.h"

#include <errno.h>

/* Change the mode of a file.  */

boolean
fsysdep_change_mode (zfile, imode)
     const char *zfile;
     unsigned int imode;
{
  if (chmod ((char *) zfile, imode) < 0)
    {
      ulog (LOG_ERROR, "chmod (%s): %s", zfile, strerror (errno));
      return FALSE;
    }
  return TRUE;
}
