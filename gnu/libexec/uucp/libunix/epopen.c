/* epopen.c
   A version of popen that goes through ixsspawn.

   Copyright (C) 1992 Ian Lance Taylor

   This file is part of the Taylor UUCP package.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   The author of the program may be contacted at ian@airs.com or
   c/o Infinity Development Systems, P.O. Box 520, Waltham, MA 02254.
   */

#include "uucp.h"

#include "sysdep.h"

#include <errno.h>

/* A version of popen that goes through ixsspawn.  This actually takes
   an array of arguments rather than a string, and takes a boolean
   read/write value rather than a string.  It sets *pipid to the
   process ID of the child.  */

FILE *
espopen (pazargs, frd, pipid)
     const char **pazargs;
     boolean frd;
     pid_t *pipid;
{
  int aidescs[3];
  pid_t ipid;
  FILE *eret;

  if (frd)
    {
      aidescs[0] = SPAWN_NULL;
      aidescs[1] = SPAWN_READ_PIPE;
    }
  else
    {
      aidescs[0] = SPAWN_WRITE_PIPE;
      aidescs[1] = SPAWN_NULL;
    }
  aidescs[2] = SPAWN_NULL;

  ipid = ixsspawn (pazargs, aidescs, FALSE, FALSE,
		   (const char *) NULL, FALSE, TRUE,
		   (const char *) NULL, (const char *) NULL,
		   (const char *) NULL);
  if (ipid < 0)
    return NULL;

  if (frd)
    eret = fdopen (aidescs[1], (char *) "r");
  else
    eret = fdopen (aidescs[0], (char *) "w");
  if (eret == NULL)
    {
      int ierr;

      ierr = errno;
      (void) close (frd ? aidescs[1] : aidescs[0]);
      (void) kill (ipid, SIGKILL);
      (void) ixswait ((unsigned long) ipid, (const char *) NULL);
      errno = ierr;
      return NULL;
    }
    
  *pipid = ipid;

  return eret;
}
