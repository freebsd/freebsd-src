// -*- C++ -*-
/* Copyright (C) 1989, 1990, 1991, 1992, 2000, 2001
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
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "posix.h"
#include "lib.h"
#include "errarg.h"
#include "error.h"
#include "nonposix.h"

#ifndef HAVE_MKSTEMP_PROTO
extern "C" {
  extern int mkstemp (char *);
}
#endif

// If this is set, create temporary files there
#define GROFF_TMPDIR_ENVVAR "GROFF_TMPDIR"
// otherwise if this is set, create temporary files there
#define TMPDIR_ENVVAR "TMPDIR"
// otherwise if P_tmpdir is defined, create temporary files there
#ifdef P_tmpdir
# define DEFAULT_TMPDIR P_tmpdir
#else
// otherwise create temporary files here.
# define DEFAULT_TMPDIR "/tmp"
#endif
// Use this as the prefix for temporary filenames.
#ifdef __MSDOS__
#define TMPFILE_PREFIX ""
#else
#define TMPFILE_PREFIX "groff"
#endif

/*
 *  Generate a temporary name template with a postfix
 *  immediately after the TMPFILE_PREFIX.
 *  It uses the groff preferences for a temporary directory.
 *  Note that no file name is either created or opened,
 *  only the *template* is returned.
 */

char *xtmptemplate(char *postfix)
{
  const char *dir = getenv(GROFF_TMPDIR_ENVVAR);
  int postlen = 0;

  if (postfix)
    postlen = strlen(postfix);
    
  if (!dir) {
    dir = getenv(TMPDIR_ENVVAR);
    if (!dir)
      dir = DEFAULT_TMPDIR;
  }

  size_t dir_len = strlen(dir);
  const char *dir_end = dir + dir_len - 1;
  int needs_slash = strchr(DIR_SEPS, *dir_end) == NULL;
  char *templ = new char[strlen(dir) + needs_slash
		+ sizeof(TMPFILE_PREFIX) - 1 + 6 + 1 + postlen];
  strcpy(templ, dir);
  if (needs_slash)
    strcat(templ, "/");
  strcat(templ, TMPFILE_PREFIX);
  if (postlen > 0)
    strcat(templ, postfix);
  strcat(templ, "XXXXXX");

  return( templ );
}

// The trick with unlinking the temporary file while it is still in
// use is not portable, it will fail on MS-DOS and most MS-Windows
// filesystems.  So it cannot be used on non-Posix systems.
// Instead, we maintain a list of files to be deleted on exit, and
// register an atexit function that will remove them all in one go.
// This should be portable to all platforms.

static struct xtmpfile_list {
  struct xtmpfile_list *next;
  char fname[1];
} *xtmpfiles_to_delete;

static void remove_tmp_files()
{
  struct xtmpfile_list *p = xtmpfiles_to_delete;

  while (p) {
    if (unlink(p->fname) < 0)
      error("cannot unlink `%1': %2", p->fname, strerror(errno));
    struct xtmpfile_list *old = p;
    p = p->next;
    free(old);
  }
}

static void add_tmp_file(const char *name)
{
  if (xtmpfiles_to_delete == NULL)
    atexit(remove_tmp_files);

  struct xtmpfile_list *p
    = (struct xtmpfile_list *)malloc(sizeof(struct xtmpfile_list)
				     + strlen (name));
  if (p == NULL) {
    error("cannot unlink `%1': %2", name, strerror(errno));
    return;
  }
  p->next = xtmpfiles_to_delete;
  strcpy(p->fname, name);
  xtmpfiles_to_delete = p;
}

// Open a temporary file and with fatal error on failure.

FILE *xtmpfile(char **namep, char *postfix, int do_unlink)
{
  char *templ = xtmptemplate(postfix);

#ifdef HAVE_MKSTEMP
  errno = 0;
  int fd = mkstemp(templ);
  if (fd < 0)
    fatal("cannot create temporary file: %1", strerror(errno));
  errno = 0;
  FILE *fp = fdopen(fd, FOPEN_RWB); // many callers of xtmpfile use binary I/O
  if (!fp)
    fatal("fdopen: %1", strerror(errno));
#else /* not HAVE_MKSTEMP */
  if (!mktemp(templ) || !templ[0])
    fatal("cannot create file name for temporary file");
  errno = 0;
  FILE *fp = fopen(templ, FOPEN_RWB);
  if (!fp)
    fatal("cannot open `%1': %2", templ, strerror(errno));
#endif /* not HAVE_MKSTEMP */
  if (do_unlink)
    add_tmp_file(templ);
  if ((namep != 0) && ((*namep) != 0))
    *namep = templ;
  else
    a_delete templ;
  return fp;
}
