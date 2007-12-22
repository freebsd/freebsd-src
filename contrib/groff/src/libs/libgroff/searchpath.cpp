// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992, 2000, 2001, 2003, 2005
   Free Software Foundation, Inc.
     Written by James Clark (jjc@jclark.com)

This file is part of groff.

groff is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

groff is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License along
with groff; see the file COPYING.  If not, write to the Free Software
Foundation, 51 Franklin St - Fifth Floor, Boston, MA 02110-1301, USA. */

#include "lib.h"

#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#include "searchpath.h"
#include "nonposix.h"

#ifdef _WIN32
# include "relocate.h"
#else
# define relocate(path) strsave(path)
#endif

search_path::search_path(const char *envvar, const char *standard,
			 int add_home, int add_current)
{
  char *home = 0;
  if (add_home)
    home = getenv("HOME");
  char *e = 0;
  if (envvar)
    e = getenv(envvar);
  dirs = new char[((e && *e) ? strlen(e) + 1 : 0)
		  + (add_current ? 1 + 1 : 0)
		  + ((home && *home) ? strlen(home) + 1 : 0)
		  + ((standard && *standard) ? strlen(standard) : 0)
		  + 1];
  *dirs = '\0';
  if (e && *e) {
    strcat(dirs, e);
    strcat(dirs, PATH_SEP);
  }
  if (add_current) {
    strcat(dirs, ".");
    strcat(dirs, PATH_SEP);
  }
  if (home && *home) {
    strcat(dirs, home);
    strcat(dirs, PATH_SEP);
  }
  if (standard && *standard)
    strcat(dirs, standard);
  init_len = strlen(dirs);
}

search_path::~search_path()
{
  // dirs is always allocated
  a_delete dirs;
}

void search_path::command_line_dir(const char *s)
{
  char *old = dirs;
  unsigned old_len = strlen(old);
  unsigned slen = strlen(s);
  dirs = new char[old_len + 1 + slen + 1];
  memcpy(dirs, old, old_len - init_len);
  char *p = dirs;
  p += old_len - init_len;
  if (init_len == 0)
    *p++ = PATH_SEP_CHAR;
  memcpy(p, s, slen);
  p += slen;
  if (init_len > 0) {
    *p++ = PATH_SEP_CHAR;
    memcpy(p, old + old_len - init_len, init_len);
    p += init_len;
  }
  *p++ = '\0';
  a_delete old;
}

FILE *search_path::open_file(const char *name, char **pathp)
{
  assert(name != 0);
  if (IS_ABSOLUTE(name) || *dirs == '\0') {
    FILE *fp = fopen(name, "r");
    if (fp) {
      if (pathp)
	*pathp = strsave(name);
      return fp;
    }
    else
      return 0;
  }
  unsigned namelen = strlen(name);
  char *p = dirs;
  for (;;) {
    char *end = strchr(p, PATH_SEP_CHAR);
    if (!end)
      end = strchr(p, '\0');
    int need_slash = end > p && strchr(DIR_SEPS, end[-1]) == 0;
    char *origpath = new char[(end - p) + need_slash + namelen + 1];
    memcpy(origpath, p, end - p);
    if (need_slash)
      origpath[end - p] = '/';
    strcpy(origpath + (end - p) + need_slash, name);
#if 0
    fprintf(stderr, "origpath `%s'\n", origpath);
#endif
    char *path = relocate(origpath);
    a_delete origpath;
#if 0
    fprintf(stderr, "trying `%s'\n", path);
#endif
    FILE *fp = fopen(path, "r");
    if (fp) {
      if (pathp)
	*pathp = path;
      else
	a_delete path;
      return fp;
    }
    a_delete path;
    if (*end == '\0')
      break;
    p = end + 1;
  }
  return 0;
}

FILE *search_path::open_file_cautious(const char *name, char **pathp,
				      const char *mode)
{
  if (!mode)
    mode = "r";
  bool reading = (strchr(mode, 'r') != 0);
  if (name == 0 || strcmp(name, "-") == 0) {
    if (pathp)
      *pathp = strsave(reading ? "stdin" : "stdout");
    return (reading ? stdin : stdout);
  }
  if (!reading || IS_ABSOLUTE(name) || *dirs == '\0') {
    FILE *fp = fopen(name, mode);
    if (fp) {
      if (pathp)
	*pathp = strsave(name);
      return fp;
    }
    else
      return 0;
  }
  unsigned namelen = strlen(name);
  char *p = dirs;
  for (;;) {
    char *end = strchr(p, PATH_SEP_CHAR);
    if (!end)
      end = strchr(p, '\0');
    int need_slash = end > p && strchr(DIR_SEPS, end[-1]) == 0;
    char *origpath = new char[(end - p) + need_slash + namelen + 1];
    memcpy(origpath, p, end - p);
    if (need_slash)
      origpath[end - p] = '/';
    strcpy(origpath + (end - p) + need_slash, name);
#if 0
    fprintf(stderr, "origpath `%s'\n", origpath);
#endif
    char *path = relocate(origpath);
    a_delete origpath;
#if 0
    fprintf(stderr, "trying `%s'\n", path);
#endif
    FILE *fp = fopen(path, mode);
    if (fp) {
      if (pathp)
	*pathp = path;
      else
	a_delete path;
      return fp;
    }
    int err = errno;
    a_delete path;
    if (err != ENOENT)
    {
      errno = err;
      return 0;
    }
    if (*end == '\0')
      break;
    p = end + 1;
  }
  errno = ENOENT;
  return 0;
}
