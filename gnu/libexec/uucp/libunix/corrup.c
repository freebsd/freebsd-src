/* corrup.c
   Save a file in the .Corrupt directory.  */

#include "uucp.h"

#include "sysdep.h"
#include "uudefs.h"
#include "system.h"

char *
zsysdep_save_corrupt_file (zfile)
     const char *zfile;
{
  const char *zslash;
  char *zto;

  zslash = strrchr (zfile, '/');
  if (zslash == NULL)
    zslash = zfile;
  else
    ++zslash;

  zto = zsappend3 (zSspooldir, CORRUPTDIR, zslash);

  if (! fsysdep_move_file (zfile, zto, TRUE, FALSE, FALSE,
			   (const char *) NULL))
    {
      ubuffree (zto);
      return NULL;
    }

  return zto;
}
