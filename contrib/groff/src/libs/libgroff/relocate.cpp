// -*- C++ -*-
/* Provide relocation for macro and font files.
   Copyright (C) 2005 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU Library General Public License as published
   by the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301,
   USA.  */

// Made after relocation code in kpathsea and gettext.

#include "lib.h"

#include <errno.h>
#include <stdlib.h>

#include "defs.h"
#include "posix.h"
#include "nonposix.h"
#include "relocate.h"

#if defined _WIN32
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#endif

#define INSTALLPATHLEN (sizeof(INSTALLPATH) - 1)
#ifndef DEBUG
# define DEBUG 0
#endif

extern "C" const char *program_name;

// The prefix (parent directory) corresponding to the binary.
char *curr_prefix = 0;
size_t curr_prefix_len = 0;

// Return the directory part of a filename, or `.' if no path separators.
char *xdirname(char *s)
{
  static const char dot[] = ".";
  if (!s)
    return 0;
  // DIR_SEPS[] are possible directory separator characters, see nonposix.h.
  // We want the rightmost separator of all possible ones.
  // Example: d:/foo\\bar.
  char *p = strrchr(s, DIR_SEPS[0]);
  const char *sep = &DIR_SEPS[1];
  while (*sep) {
    char *p1 = strrchr(s, *sep);
    if (p1 && (!p || p1 > p))
      p = p1;
    sep++;
  }
  if (p)
    *p = '\0';
  else
    s = (char *)dot;
  return s;
}

// Return the full path of NAME along the path PATHP.
// Adapted from search_path::open_file in searchpath.cpp.
char *searchpath(const char *name, const char *pathp)
{
  char *path;
  if (!name || !*name)
    return 0;
#if DEBUG
  fprintf(stderr, "searchpath: pathp: `%s'\n", pathp);
  fprintf(stderr, "searchpath: trying `%s'\n", name);
#endif
  // Try first NAME as such; success if NAME is an absolute filename,
  // or if NAME is found in the current directory.
  if (!access (name, F_OK)) {
    path = new char[path_name_max()];
#ifdef _WIN32
    path = _fullpath(path, name, path_name_max());
#else
    path = realpath(name, path);
#endif
#if DEBUG
    fprintf(stderr, "searchpath: found `%s'\n", path);
#endif
    return path;
  }
  // Secondly, try the current directory.
  // Now search along PATHP.
  size_t namelen = strlen(name);
  char *p = (char *)pathp;
  for (;;) {
    char *end = strchr(p, PATH_SEP_CHAR);
    if (!end)
      end = strchr(p, '\0');
    int need_slash = end > p && strchr(DIR_SEPS, end[-1]) == 0;
    path = new char[end - p + need_slash + namelen + 1];
    memcpy(path, p, end - p);
    if (need_slash)
      path[end - p] = '/';
    strcpy(path + (end - p) + need_slash, name);
#if DEBUG
    fprintf(stderr, "searchpath: trying `%s'\n", path);
#endif
    if (!access(path, F_OK)) {
#if DEBUG
      fprintf(stderr, "searchpath: found `%s'\n", name);
#endif
      return path;
    }
    a_delete path;
    if (*end == '\0')
      break;
    p = end + 1;
  }
  return 0;
}

// Search NAME along PATHP with the elements of PATHEXT in turn added.
char *searchpathext(const char *name, const char *pathext, const char *pathp)
{
  char *found = 0;
  char *tmpathext = strsave(pathext);	// strtok modifies this string,
					// so make a copy
  char *ext = strtok(tmpathext, PATH_SEP);
  while (ext) {
    char *namex = new char[strlen(name) + strlen(ext) + 1];
    strcpy(namex, name);
    strcat(namex, ext);
    found = searchpath(namex, pathp);
    a_delete namex;
    if (found)
       break;
    ext = strtok(0, PATH_SEP);
  }
  a_delete tmpathext;
  return found;
}

// Convert an MS path to a POSIX path.
char *msw2posixpath(char *path)
{
  char *s = path;
  while (*s) {
    if (*s == '\\')
      *s = '/';
    *s++;
  }
  return path;
}

// Compute the current prefix.
void set_current_prefix()
{
  char *pathextstr;
  curr_prefix = new char[path_name_max()];
  // Obtain the full path of the current binary;
  // using GetModuleFileName on MS-Windows,
  // and searching along PATH on other systems.
#ifdef _WIN32
  int len = GetModuleFileName(0, curr_prefix, path_name_max());
  if (len)
    len = GetShortPathName(curr_prefix, curr_prefix, path_name_max());
# if DEBUG
  fprintf(stderr, "curr_prefix: %s\n", curr_prefix);
# endif /* DEBUG */
#else /* !_WIN32 */
  curr_prefix = searchpath(program_name, getenv("PATH"));
  if (!curr_prefix && !strchr(program_name, '.')) {	// try with extensions
    pathextstr = strsave(getenv("PATHEXT"));
    if (!pathextstr)
      pathextstr = strsave(PATH_EXT);
    curr_prefix = searchpathext(program_name, pathextstr, getenv("PATH"));
    a_delete pathextstr;
  }
  if (!curr_prefix)
    return;
#endif /* !_WIN32 */
  msw2posixpath(curr_prefix);
#if DEBUG
  fprintf(stderr, "curr_prefix: %s\n", curr_prefix);
#endif
  curr_prefix = xdirname(curr_prefix);	// directory of executable
  curr_prefix = xdirname(curr_prefix);	// parent directory of executable
  curr_prefix_len = strlen(curr_prefix);
#if DEBUG
  fprintf(stderr, "curr_prefix: %s\n", curr_prefix);
  fprintf(stderr, "curr_prefix_len: %d\n", curr_prefix_len);
#endif
}

// Strip the installation prefix and replace it
// with the current installation prefix; return the relocated path.
char *relocatep(const char *path)
{
#if DEBUG
  fprintf(stderr, "relocatep: path = %s\n", path);
  fprintf(stderr, "relocatep: INSTALLPATH = %s\n", INSTALLPATH);
  fprintf(stderr, "relocatep: INSTALLPATHLEN = %d\n", INSTALLPATHLEN);
#endif
  if (!curr_prefix)
    set_current_prefix();
  if (strncmp(INSTALLPATH, path, INSTALLPATHLEN))
    return strsave(path);
  char *relative_path = (char *)path + INSTALLPATHLEN;
  size_t relative_path_len = strlen(relative_path);
  char *relocated_path = new char[curr_prefix_len + relative_path_len + 1];
  strcpy(relocated_path, curr_prefix);
  strcat(relocated_path, relative_path);
#if DEBUG
  fprintf(stderr, "relocated_path: %s\n", relocated_path);
#endif /* DEBUG */
  return relocated_path;
}

// Return the original pathname if it exists;
// otherwise return the relocated path.
char *relocate(const char *path)
{
  char *p;
  if (access(path, F_OK))
    p = relocatep(path);
  else
    p = strsave(path);
#if DEBUG
  fprintf (stderr, "relocate: %s\n", p);
#endif
  return p;
}
