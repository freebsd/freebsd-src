/* app4.c
   Stick three directories and a file name together.  */

#include "uucp.h"

#include "uudefs.h"
#include "sysdep.h"

char *
zsappend4 (zdir1, zdir2, zdir3, zfile)
     const char *zdir1;
     const char *zdir2;
     const char *zdir3;
     const char *zfile;
{
  size_t cdir1, cdir2, cdir3, cfile;
  char *zret;

  cdir1 = strlen (zdir1);
  cdir2 = strlen (zdir2);
  cdir3 = strlen (zdir3);
  cfile = strlen (zfile);
  zret = zbufalc (cdir1 + cdir2 + cdir3 + cfile + 4);
  if (cdir1 == 1 && *zdir1 == '/')
    cdir1 = 0;
  else
    memcpy (zret, zdir1, cdir1);
  memcpy (zret + cdir1 + 1, zdir2, cdir2);
  memcpy (zret + cdir1 + cdir2 + 2, zdir3, cdir3);
  memcpy (zret + cdir1 + cdir2 + cdir3 + 3, zfile, cfile);
  zret[cdir1] = '/';
  zret[cdir1 + cdir2 + 1] = '/';
  zret[cdir1 + cdir2 + cdir3 + 2] = '/';
  zret[cdir1 + cdir2 + cdir3 + cfile + 3] = '\0';
  return zret;
}
