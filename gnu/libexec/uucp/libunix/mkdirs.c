/* mkdirs.c
   Create any directories needed for a file name.  */

#include "uucp.h"

#include "uudefs.h"
#include "sysdep.h"
#include "system.h"

#include <errno.h>

boolean
fsysdep_make_dirs (zfile, fpublic)
     const char *zfile;
     boolean fpublic;
{
  char *zcopy, *z;
  int imode;

  zcopy = zbufcpy (zfile);

  if (fpublic)
    imode = IPUBLIC_DIRECTORY_MODE;
  else
    imode = IDIRECTORY_MODE;

  for (z = zcopy; *z != '\0'; z++)
    {
      if (*z == '/' && z != zcopy)
	{
	  *z = '\0';
	  if (mkdir (zcopy, imode) != 0
	      && errno != EEXIST
	      && (errno != EACCES || ! fsysdep_directory (zcopy)))
	    {
	      ulog (LOG_ERROR, "mkdir (%s): %s", zcopy,
		    strerror (errno));
	      ubuffree (zcopy);
	      return FALSE;
	    }
	  *z = '/';     /* replace '/' in its place */
          		/* now skips over multiple '/' in name */
          while ( (*(z + 1)) && (*(z + 1)) == '/')
              z++;
	}
    }

  ubuffree (zcopy);

  return TRUE;
}
