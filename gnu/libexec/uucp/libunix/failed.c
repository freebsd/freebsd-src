/* failed.c
   Save a file in the .Failed directory.  */

#include "uucp.h"

#include "sysdep.h"
#include "uudefs.h"
#include "system.h"

char *
zsysdep_save_failed_file (zfile)
     const char *zfile;
{
  char *zto;

  zto = zsappend3 (zSspooldir, FAILEDDIR, zfile);

  if (! fsysdep_move_file (zfile, zto, TRUE, FALSE, FALSE,
			   (const char *) NULL))
    {
      ubuffree (zto);
      return NULL;
    }

  return zto;
}
