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
#else
char *malloc ();
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

#define isdigit(c) ((c) >= '0' && (c) <= '9')

char *strdup ();
static int isnumber ();

/* Extract from NAME, which has the form "[user][:.][group]",
   a USERNAME, UID U, GROUPNAME, and GID G.
   Either user or group, or both, must be present.
   If the group is omitted but the ":" or "." separator is given,
   use the given user's login group.

   USERNAME and GROUPNAME will be in newly malloc'd memory.
   Either one might be NULL instead, indicating that it was not
   given and the corresponding numeric ID was left unchanged.
   Might write NULs into NAME.

   Return NULL if successful, a static error message string if not.  */

char *
parse_user_spec (name, uid, gid, username, groupname)
     char *name;
     uid_t *uid;
     gid_t *gid;
     char **username, **groupname;
{
  static char *tired = "virtual memory exhausted";
  struct passwd *pwd;
  struct group *grp;
  char *cp;
  int use_login_group = 0;

  *username = *groupname = NULL;

  /* Check whether a group is given.  */
  cp = index (name, ':');
  if (cp == NULL)
    cp = index (name, '.');
  if (cp != NULL)
    {
      *cp++ = '\0';
      if (*cp == '\0')
	{
	  if (cp == name + 1)
	    /* Neither user nor group given, just "." or ":".  */
	    return "can not omit both user and group";
	  else
	    /* "user.".  */
	    use_login_group = 1;
	}
      else
	{
	  /* Explicit group.  */
	  *groupname = strdup (cp);
	  if (*groupname == NULL)
	    return tired;
	  grp = getgrnam (cp);
	  if (grp == NULL)
	    {
	      if (!isnumber (cp))
		return "invalid group";
	      *gid = atoi (cp);
	    }
	  else
	    *gid = grp->gr_gid;
	  endgrent ();		/* Save a file descriptor.  */
	}
    }

  /* Parse the user name, now that any group has been removed.  */

  if (name[0] == '\0')
    /* No user name was given, just a group.  */
    return NULL;

  *username = strdup (name);
  if (*username == NULL)
    return tired;

  pwd = getpwnam (name);
  if (pwd == NULL)
    {
      if (!isnumber (name))
	return "invalid user";
      if (use_login_group)
	return "cannot get the login group of a numeric UID";
      *uid = atoi (name);
    }
  else
    {
      *uid = pwd->pw_uid;
      if (use_login_group)
	{
	  *gid = pwd->pw_gid;
	  grp = getgrgid (pwd->pw_gid);
	  if (grp == NULL)
	    {
	      *groupname = malloc (15);
	      if (*groupname == NULL)
		return tired;
	      sprintf (*groupname, "%u", pwd->pw_gid);
	    }
	  else
	    {
	      *groupname = strdup (grp->gr_name);
	      if (*groupname == NULL)
		return tired;
	    }
	  endgrent ();
	}
    }
  endpwent ();
  return NULL;
}

/* Return nonzero if STR represents an unsigned decimal integer,
   otherwise return 0. */

static int
isnumber (str)
     char *str;
{
  for (; *str; str++)
    if (!isdigit (*str))
      return 0;
  return 1;
}
