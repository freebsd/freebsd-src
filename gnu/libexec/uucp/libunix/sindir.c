/* sindir.c
   Stick a directory and file name together.  */

#include "uucp.h"

#include "uudefs.h"
#include "sysdep.h"
#include "system.h"

char *
zsysdep_in_dir (zdir, zfile)
     const char *zdir;
     const char *zfile;
{
  size_t cdir, cfile;
  char *zret;

  cdir = strlen (zdir);
  cfile = strlen (zfile);
  zret = zbufalc (cdir + cfile + 2);
  memcpy (zret, zdir, cdir);
  memcpy (zret + cdir + 1, zfile, cfile);
  zret[cdir] = '/';
  zret[cdir + cfile + 1] = '\0';
  return zret;
}
