/* link.c
   Link two files.  */

#include "uucp.h"

#include "uudefs.h"
#include "sysdep.h"
#include "system.h"

#include <errno.h>

boolean
fsysdep_link (zfrom, zto, pfworked)
     const char *zfrom;
     const char *zto;
     boolean *pfworked;
{
  *pfworked = FALSE;
  if (link (zfrom, zto) == 0)
    {
      *pfworked = TRUE;
      return TRUE;
    }
  if (errno == ENOENT)
    {
      if (! fsysdep_make_dirs (zto, TRUE))
	return FALSE;
      if (link (zfrom, zto) == 0)
	{
	  *pfworked = TRUE;
	  return TRUE;
	}
    }
  if (errno == EXDEV)
    return TRUE;
  ulog (LOG_ERROR, "link (%s, %s): %s", zfrom, zto, strerror (errno));
  return FALSE;
}
