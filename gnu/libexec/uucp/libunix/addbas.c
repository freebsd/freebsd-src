/* addbas.c
   If we have a directory, add in a base name.  */

#include "uucp.h"

#include "uudefs.h"
#include "sysdep.h"
#include "system.h"

/* If we have a directory, add a base name.  */

char *
zsysdep_add_base (zfile, zname)
     const char *zfile;
     const char *zname;
{
  size_t clen;
  const char *zlook;
  char *zfree;
  char *zret;

#if DEBUG > 0
  if (*zfile != '/')
    ulog (LOG_FATAL, "zsysdep_add_base: %s: Can't happen", zfile);
#endif

  clen = strlen (zfile);

  if (zfile[clen - 1] != '/')
    {
      if (! fsysdep_directory (zfile))
	return zbufcpy (zfile);
      zfree = NULL;
    }
  else
    {
      /* Trim out the trailing '/'.  */
      zfree = zbufcpy (zfile);
      zfree[clen - 1] = '\0';
      zfile = zfree;
    }

  zlook = strrchr (zname, '/');
  if (zlook != NULL)
    zname = zlook + 1;

  zret = zsysdep_in_dir (zfile, zname);
  ubuffree (zfree);
  return zret;
}
