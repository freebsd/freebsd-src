/* environ.c -- library for manipulating environments for GNU.
   Copyright (C) 1986, 1989 Free Software Foundation, Inc.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) > (b) ? (a) : (b))

#include "defs.h"
#include "environ.h"
#include <string.h>
#include "defs.h" /* For strsave().  */


/* Return a new environment object.  */

struct environ *
make_environ ()
{
  register struct environ *e;

  e = (struct environ *) xmalloc (sizeof (struct environ));

  e->allocated = 10;
  e->vector = (char **) xmalloc ((e->allocated + 1) * sizeof (char *));
  e->vector[0] = 0;
  return e;
}

/* Free an environment and all the strings in it.  */

void
free_environ (e)
     register struct environ *e;
{
  register char **vector = e->vector;

  while (*vector)
    free (*vector++);

  free (e);
}

/* Copy the environment given to this process into E.
   Also copies all the strings in it, so we can be sure
   that all strings in these environments are safe to free.  */

void
init_environ (e)
     register struct environ *e;
{
  extern char **environ;
  register int i;

  for (i = 0; environ[i]; i++) /*EMPTY*/;

  if (e->allocated < i)
    {
      e->allocated = max (i, e->allocated + 10);
      e->vector = (char **) xrealloc ((char *)e->vector,
				      (e->allocated + 1) * sizeof (char *));
    }

  memcpy (e->vector, environ, (i + 1) * sizeof (char *));

  while (--i >= 0)
    {
      register int len = strlen (e->vector[i]);
      register char *new = (char *) xmalloc (len + 1);
      memcpy (new, e->vector[i], len + 1);
      e->vector[i] = new;
    }
}

/* Return the vector of environment E.
   This is used to get something to pass to execve.  */

char **
environ_vector (e)
     struct environ *e;
{
  return e->vector;
}

/* Return the value in environment E of variable VAR.  */

char *
get_in_environ (e, var)
     const struct environ *e;
     const char *var;
{
  register int len = strlen (var);
  register char **vector = e->vector;
  register char *s;

  for (; (s = *vector) != NULL; vector++)
    if (STREQN (s, var, len) && s[len] == '=')
      return &s[len + 1];

  return 0;
}

/* Store the value in E of VAR as VALUE.  */

void
set_in_environ (e, var, value)
     struct environ *e;
     const char *var;
     const char *value;
{
  register int i;
  register int len = strlen (var);
  register char **vector = e->vector;
  register char *s;

  for (i = 0; (s = vector[i]) != NULL; i++)
    if (STREQN (s, var, len) && s[len] == '=')
      break;

  if (s == 0)
    {
      if (i == e->allocated)
	{
	  e->allocated += 10;
	  vector = (char **) xrealloc ((char *)vector,
				       (e->allocated + 1) * sizeof (char *));
	  e->vector = vector;
	}
      vector[i + 1] = 0;
    }
  else
    free (s);

  s = (char *) xmalloc (len + strlen (value) + 2);
  strcpy (s, var);
  strcat (s, "=");
  strcat (s, value);
  vector[i] = s;

  /* Certain variables get exported back to the parent (e.g. our) 
     environment, too.  FIXME: this is a hideous hack and should not be
     allowed to live.  What if we want to change the environment we pass to
     the program without affecting GDB's behavior?  */
  if (STREQ(var, "PATH"))		/* Object file location */
    {
      putenv (strsave (s));
    }

  /* This is a compatibility hack, since GDB 4.10 and older didn't have
     `set gnutarget'.  Eventually it should go away, so that (for example)
     you can debug objdump's handling of GNUTARGET without affecting GDB's
     behavior.  */
  if (STREQ (var, "GNUTARGET"))
    {
      set_gnutarget (value);
    }
  return;
}

/* Remove the setting for variable VAR from environment E.  */

void
unset_in_environ (e, var)
     struct environ *e;
     char *var;
{
  register int len = strlen (var);
  register char **vector = e->vector;
  register char *s;

  for (; (s = *vector) != NULL; vector++)
    {
      if (STREQN (s, var, len) && s[len] == '=')
	{
	  free (s);
	  /* Walk through the vector, shuffling args down by one, including
	     the NULL terminator.  Can't use memcpy() here since the regions
	     overlap, and memmove() might not be available. */
	  while ((vector[0] = vector[1]) != NULL)
	    {
	      vector++;
	    }
	  break;
	}
    }
}
