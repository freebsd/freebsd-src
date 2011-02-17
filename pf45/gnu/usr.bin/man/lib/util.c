/*
 * util.c
 *
 * Copyright (c) 1990, 1991, John W. Eaton.
 *
 * You may distribute under the terms of the GNU General Public
 * License as specified in the file COPYING that comes with the man
 * distribution.
 *
 * John W. Eaton
 * jwe@che.utexas.edu
 * Department of Chemical Engineering
 * The University of Texas at Austin
 * Austin, Texas  78712
 */

/* $FreeBSD$ */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#ifdef STDC_HEADERS
#include <stdlib.h>
#else
extern int fprintf ();
extern int tolower ();
#endif

extern char *strdup ();
extern int system ();

#include "gripes.h"

/*
 * Extract last element of a name like /foo/bar/baz.
 */
char *
mkprogname (s)
     register char *s;
{
  char *t;

  t = strrchr (s, '/');
  if (t == (char *)NULL)
    t = s;
  else
    t++;

  return strdup (t);
}

void
downcase (s)
     unsigned char *s;
{
  register unsigned char c;
  while ((c = *s) != '\0')
    {
      if (isalpha (c))
	*s = tolower (c);
      s++;
    }
}

/*
 * Is file a newer than file b?
 *
 * case:
 *
 *   a newer than b         returns    1
 *   a older than b         returns    0
 *   stat on a fails        returns   -1
 *   stat on b fails        returns   -2
 *   stat on a and b fails  returns   -3
 */
int
is_newer (fa, fb)
  register char *fa;
  register char *fb;
{
  struct stat fa_sb;
  struct stat fb_sb;
  register int fa_stat;
  register int fb_stat;
  register int status = 0;

  fa_stat = stat (fa, &fa_sb);
  if (fa_stat != 0)
    status = 1;

  fb_stat = stat (fb, &fb_sb);
  if (fb_stat != 0)
    status |= 2;

  if (status != 0)
    return -status;

  return (fa_sb.st_mtime > fb_sb.st_mtime);
}

/*
 * Is path a directory?
 */
int
is_directory (path)
     char *path;
{
  struct stat sb;
  register int status;

  status = stat (path, &sb);

  if (status != 0)
    return -1;

  return ((sb.st_mode & S_IFDIR) == S_IFDIR);

}

/*
 * Is path a regular file?
 */
int
is_file (path)
     char *path;
{
  struct stat sb;
  register int status;

  status = stat (path, &sb);

  if (status != 0)
    return -1;

  return ((sb.st_mode & S_IFREG) == S_IFREG);
}

/*
 * Attempt a system () call.  Return 1 for success and 0 for failure
 * (handy for counting successes :-).
 */
int
do_system_command (command)
     char *command;
{
  int status = 0;
  extern int debug;

  /*
   * If we're debugging, don't really execute the command -- you never
   * know what might be in that mangled string :-O.
   */
  if (debug)
    fprintf (stderr, "\ntrying command: %s\n", command);
  else
    status = system (command);

  /* check return value from system() function first */
  if (status == -1) {
    fprintf(stderr, 
	    "wait() for exit status of shell failed in function system()\n");
    return 0;
  } else if (status == 127 || status == (127 << 8)) {
    fprintf(stderr, "execution of the shell failed in function system()\n");
    return 0;
  }

  if (WIFSIGNALED(status))
    return -1;
  else if (WEXITSTATUS(status)) {
    gripe_system_command (status);
    return 0;
  }
  else
    return 1;
}
