/* userspec.c -- Parse a user and group string.
   Copyright (C) 1989, 1990, 1991, 1992 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

/* Written by David MacKenzie <djm@gnu.ai.mit.edu>.  */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef __GNUC__
#define alloca __builtin_alloca
#else
#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#else
#ifdef _AIX
 #pragma alloca
#else
char *alloca ();
#endif
#endif
#endif

#include <stdio.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#if defined(STDC_HEADERS) || defined(HAVE_STRING_H)
#include <string.h>
#ifndef index
#define index strchr
#endif
#else
#include <strings.h>
#endif

#ifdef STDC_HEADERS
#include <stdlib.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifndef _POSIX_VERSION
struct passwd *getpwnam ();
struct group *getgrnam ();
struct group *getgrgid ();
#endif

#ifdef _POSIX_SOURCE
#define endpwent()
#define endgrent()
#endif

/* Perform the equivalent of the statement `dest = strdup (src);',
   but obtaining storage via alloca instead of from the heap.  */

#define V_STRDUP(dest, src)						\
  do									\
    {									\
      int _len = strlen ((src));					\
      (dest) = (char *) alloca (_len + 1);				\
      strcpy (dest, src);						\
    }									\
  while (0)

#define isdigit(c) ((c) >= '0' && (c) <= '9')

char *strdup ();

/* Return nonzero if STR represents an unsigned decimal integer,
   otherwise return 0. */

static int
isnumber (str)
     const char *str;
{
  for (; *str; str++)
    if (!isdigit (*str))
      return 0;
  return 1;
}

/* Extract from NAME, which has the form "[user][:.][group]",
   a USERNAME, UID U, GROUPNAME, and GID G.
   Either user or group, or both, must be present.
   If the group is omitted but the ":" or "." separator is given,
   use the given user's login group.

   USERNAME and GROUPNAME will be in newly malloc'd memory.
   Either one might be NULL instead, indicating that it was not
   given and the corresponding numeric ID was left unchanged.

   Return NULL if successful, a static error message string if not.  */

const char *
parse_user_spec (spec_arg, uid, gid, username_arg, groupname_arg)
     const char *spec_arg;
     uid_t *uid;
     gid_t *gid;
     char **username_arg, **groupname_arg;
{
  static const char *tired = "virtual memory exhausted";
  const char *error_msg;
  char *spec;			/* A copy we can write on.  */
  struct passwd *pwd;
  struct group *grp;
  char *g, *u, *separator;
  char *groupname;

  error_msg = NULL;
  *username_arg = *groupname_arg = NULL;
  groupname = NULL;

  V_STRDUP (spec, spec_arg);

  /* Find the separator if there is one.  */
  separator = index (spec, ':');
  if (separator == NULL)
    separator = index (spec, '.');

  /* Replace separator with a NUL.  */
  if (separator != NULL)
    *separator = '\0';

  /* Set U and G to non-zero length strings corresponding to user and
     group specifiers or to NULL.  */
  u = (*spec == '\0' ? NULL : spec);

  g = (separator == NULL || *(separator + 1) == '\0'
       ? NULL
       : separator + 1);

  if (u == NULL && g == NULL)
    return "can not omit both user and group";

  if (u != NULL)
    {
      pwd = getpwnam (u);
      if (pwd == NULL)
	{

	  if (!isnumber (u))
	    error_msg = "invalid user";
	  else
	    {
	      int use_login_group;
	      use_login_group = (separator != NULL && g == NULL);
	      if (use_login_group)
		error_msg = "cannot get the login group of a numeric UID";
	      else
		*uid = atoi (u);
	    }
	}
      else
	{
	  *uid = pwd->pw_uid;
	  if (g == NULL && separator != NULL)
	    {
	      /* A separator was given, but a group was not specified,
	         so get the login group.  */
	      *gid = pwd->pw_gid;
	      grp = getgrgid (pwd->pw_gid);
	      if (grp == NULL)
		{
		  /* This is enough room to hold the unsigned decimal
		     representation of any 32-bit quantity and the trailing
		     zero byte.  */
		  char uint_buf[21];
		  sprintf (uint_buf, "%u", (unsigned) (pwd->pw_gid));
		  V_STRDUP (groupname, uint_buf);
		}
	      else
		{
		  V_STRDUP (groupname, grp->gr_name);
		}
	      endgrent ();
	    }
	}
      endpwent ();
    }

  if (g != NULL && error_msg == NULL)
    {
      /* Explicit group.  */
      grp = getgrnam (g);
      if (grp == NULL)
	{
	  if (!isnumber (g))
	    error_msg = "invalid group";
	  else
	    *gid = atoi (g);
	}
      else
	*gid = grp->gr_gid;
      endgrent ();		/* Save a file descriptor.  */

      if (error_msg == NULL)
	V_STRDUP (groupname, g);
    }

  if (error_msg == NULL)
    {
      if (u != NULL)
	{
	  *username_arg = strdup (u);
	  if (*username_arg == NULL)
	    error_msg = tired;
	}

      if (groupname != NULL && error_msg == NULL)
	{
	  *groupname_arg = strdup (groupname);
	  if (*groupname_arg == NULL)
	    {
	      if (*username_arg != NULL)
		{
		  free (*username_arg);
		  *username_arg = NULL;
		}
	      error_msg = tired;
	    }
	}
    }

  return error_msg;
}

#ifdef TEST

#define NULL_CHECK(s) ((s) == NULL ? "(null)" : (s))

int
main (int argc, char **argv)
{
  int i;

  for (i = 1; i < argc; i++)
    {
      const char *e;
      char *username, *groupname;
      uid_t uid;
      gid_t gid;
      char *tmp;

      tmp = strdup (argv[i]);
      e = parse_user_spec (tmp, &uid, &gid, &username, &groupname);
      free (tmp);
      printf ("%s: %u %u %s %s %s\n",
	      argv[i],
	      (unsigned int) uid,
	      (unsigned int) gid,
	      NULL_CHECK (username),
	      NULL_CHECK (groupname),
	      NULL_CHECK (e));
    }

  exit (0);
}

#endif
