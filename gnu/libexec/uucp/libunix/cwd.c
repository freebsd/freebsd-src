/* cwd.c
   Routines dealing with the current working directory.  */

#include "uucp.h"

#include "uudefs.h"
#include "sysdep.h"
#include "system.h"

/* See whether running this file through zsysdep_add_cwd would require
   knowing the current working directory.  This is used to avoid
   determining the cwd if it will not be needed.  */

boolean
fsysdep_needs_cwd (zfile)
     const char *zfile;
{
  return *zfile != '/' && *zfile != '~';
}

/* Expand a local file, putting relative pathnames in the current
   working directory.  Note that ~/file is placed in the public
   directory, rather than in the user's home directory.  This is
   consistent with other UUCP packages.  */

char *
zsysdep_local_file_cwd (zfile, zpubdir)
     const char *zfile;
     const char *zpubdir;
{
  if (*zfile == '/')
    return zbufcpy (zfile);
  else if (*zfile == '~')
    return zsysdep_local_file (zfile, zpubdir);
  else
    return zsysdep_add_cwd (zfile);
}      

/* Add the current working directory to a remote file name.  */

char *
zsysdep_add_cwd (zfile)
     const char *zfile;
{
  if (*zfile == '/' || *zfile == '~')
    return zbufcpy (zfile);

  if (zScwd == NULL)
    {
      ulog (LOG_ERROR, "Can't determine current directory");
      return NULL;
    }

  return zsysdep_in_dir (zScwd, zfile);
}
