/* idcache.c -- map user and group IDs, cached for speed
   Copyright (C) 1985, 1988, 1989, 1990 Free Software Foundation, Inc.

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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#if defined(STDC_HEADERS) || defined(HAVE_STRING_H)
#include <string.h>
#else
#include <strings.h>
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifndef _POSIX_VERSION
struct passwd *getpwuid ();
struct passwd *getpwnam ();
struct group *getgrgid ();
struct group *getgrnam ();
#endif

char *xmalloc ();
char *xstrdup ();

struct userid
{
  union
    {
      uid_t u;
      gid_t g;
    } id;
  char *name;
  struct userid *next;
};

static struct userid *user_alist;

/* The members of this list have names not in the local passwd file.  */
static struct userid *nouser_alist;

/* Translate UID to a login name or a stringified number,
   with cache.  */

char *
getuser (uid)
     uid_t uid;
{
  register struct userid *tail;
  struct passwd *pwent;
  char usernum_string[20];

  for (tail = user_alist; tail; tail = tail->next)
    if (tail->id.u == uid)
      return tail->name;

  pwent = getpwuid (uid);
  tail = (struct userid *) xmalloc (sizeof (struct userid));
  tail->id.u = uid;
  if (pwent == 0)
    {
      sprintf (usernum_string, "%u", (unsigned) uid);
      tail->name = xstrdup (usernum_string);
    }
  else
    tail->name = xstrdup (pwent->pw_name);

  /* Add to the head of the list, so most recently used is first.  */
  tail->next = user_alist;
  user_alist = tail;
  return tail->name;
}

/* Translate USER to a UID, with cache.
   Return NULL if there is no such user.
   (We also cache which user names have no passwd entry,
   so we don't keep looking them up.)  */

uid_t *
getuidbyname (user)
     char *user;
{
  register struct userid *tail;
  struct passwd *pwent;

  for (tail = user_alist; tail; tail = tail->next)
    /* Avoid a function call for the most common case.  */
    if (*tail->name == *user && !strcmp (tail->name, user))
      return &tail->id.u;

  for (tail = nouser_alist; tail; tail = tail->next)
    /* Avoid a function call for the most common case.  */
    if (*tail->name == *user && !strcmp (tail->name, user))
      return 0;

  pwent = getpwnam (user);

  tail = (struct userid *) xmalloc (sizeof (struct userid));
  tail->name = xstrdup (user);

  /* Add to the head of the list, so most recently used is first.  */
  if (pwent)
    {
      tail->id.u = pwent->pw_uid;
      tail->next = user_alist;
      user_alist = tail;
      return &tail->id.u;
    }

  tail->next = nouser_alist;
  nouser_alist = tail;
  return 0;
}

/* Use the same struct as for userids.  */
static struct userid *group_alist;
static struct userid *nogroup_alist;

/* Translate GID to a group name or a stringified number,
   with cache.  */

char *
getgroup (gid)
     gid_t gid;
{
  register struct userid *tail;
  struct group *grent;
  char groupnum_string[20];

  for (tail = group_alist; tail; tail = tail->next)
    if (tail->id.g == gid)
      return tail->name;

  grent = getgrgid (gid);
  tail = (struct userid *) xmalloc (sizeof (struct userid));
  tail->id.g = gid;
  if (grent == 0)
    {
      sprintf (groupnum_string, "%u", (unsigned int) gid);
      tail->name = xstrdup (groupnum_string);
    }
  else
    tail->name = xstrdup (grent->gr_name);

  /* Add to the head of the list, so most recently used is first.  */
  tail->next = group_alist;
  group_alist = tail;
  return tail->name;
}

/* Translate GROUP to a UID, with cache.
   Return NULL if there is no such group.
   (We also cache which group names have no group entry,
   so we don't keep looking them up.)  */

gid_t *
getgidbyname (group)
     char *group;
{
  register struct userid *tail;
  struct group *grent;

  for (tail = group_alist; tail; tail = tail->next)
    /* Avoid a function call for the most common case.  */
    if (*tail->name == *group && !strcmp (tail->name, group))
      return &tail->id.g;

  for (tail = nogroup_alist; tail; tail = tail->next)
    /* Avoid a function call for the most common case.  */
    if (*tail->name == *group && !strcmp (tail->name, group))
      return 0;

  grent = getgrnam (group);

  tail = (struct userid *) xmalloc (sizeof (struct userid));
  tail->name = xstrdup (group);

  /* Add to the head of the list, so most recently used is first.  */
  if (grent)
    {
      tail->id.g = grent->gr_gid;
      tail->next = group_alist;
      group_alist = tail;
      return &tail->id.g;
    }

  tail->next = nogroup_alist;
  nogroup_alist = tail;
  return 0;
}
