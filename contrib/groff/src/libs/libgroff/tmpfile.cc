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

#include "lib.h"

#include <errno.h>
#include <stdlib.h>

#include "posix.h"
#include "errarg.h"
#include "error.h"
#include "nonposix.h"

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
#define TMPFILE_PREFIX_SHORT ""
#define TMPFILE_PREFIX_LONG "groff"

char *tmpfile_prefix;
size_t tmpfile_prefix_len;
int use_short_postfix = 0;

struct temp_init {
  temp_init();
  ~temp_init();
} _temp_init;

temp_init::temp_init()
{
  const char *tem = getenv(GROFF_TMPDIR_ENVVAR);
  if (!tem) {
    tem = getenv(TMPDIR_ENVVAR);
    if (!tem)
      tem = DEFAULT_TMPDIR;
  }
  size_t tem_len = strlen(tem);
  const char *tem_end = tem + tem_len - 1;
  int need_slash = strchr(DIR_SEPS, *tem_end) == NULL ? 1 : 0;
  char *tem2 = new char[tem_len + need_slash + 1];
  strcpy(tem2, tem);
  if (need_slash)
    strcat(tem2, "/");
  const char *tem3 = TMPFILE_PREFIX_LONG;
  if (file_name_max(tem2) <= 14) {
    tem3 = TMPFILE_PREFIX_SHORT;
    use_short_postfix = 1;
  }
  tmpfile_prefix_len = tem_len + need_slash + strlen(tem3);
  tmpfile_prefix = new char[tmpfile_prefix_len + 1];
  strcpy(tmpfile_prefix, tem2);
  strcat(tmpfile_prefix, tem3);
  a_delete tem2;
}

temp_init::~temp_init()
{
  a_delete tmpfile_prefix;
}

/*
 *  Generate a temporary name template with a postfix
 *  immediately after the TMPFILE_PREFIX.
 *  It uses the groff preferences for a temporary directory.
 *  Note that no file name is either created or opened,
 *  only the *template* is returned.
 */

char *xtmptemplate(const char *postfix_long, const char *postfix_short)
{
  const char *postfix = use_short_postfix ? postfix_short : postfix_long;
  int postlen = 0;
  if (postfix)
    postlen = strlen(postfix);
  char *templ = new char[tmpfile_prefix_len + postlen + 6 + 1];
  strcpy(templ, tmpfile_prefix);
  if (postlen > 0)
    strcat(templ, postfix);
  strcat(templ, "XXXXXX");
  return templ;
}

// The trick with unlinking the temporary file while it is still in
// use is not portable, it will fail on MS-DOS and most MS-Windows
// filesystems.  So it cannot be used on non-Posix systems.
// Instead, we maintain a list of files to be deleted on exit.
// This should be portable to all platforms.

struct xtmpfile_list {
  char *fname;
  xtmpfile_list *next;
  xtmpfile_list(char *fn) : fname(fn), next(0) {}
};

xtmpfile_list *xtmpfiles_to_delete = 0;

struct xtmpfile_list_init {
  ~xtmpfile_list_init();
} _xtmpfile_list_init;

xtmpfile_list_init::~xtmpfile_list_init()
{
  xtmpfile_list *x = xtmpfiles_to_delete;
  while (x != 0) {
    if (unlink(x->fname) < 0)
      error("cannot unlink `%1': %2", x->fname, strerror(errno));
    xtmpfile_list *tmp = x;
    x = x->next;
    a_delete tmp->fname;
    delete tmp;
  }
}

static void add_tmp_file(const char *name)
{
  char *s = new char[strlen(name)+1];
  strcpy(s, name);
  xtmpfile_list *x = new xtmpfile_list(s);
  x->next = xtmpfiles_to_delete;
  xtmpfiles_to_delete = x;
}

// Open a temporary file and with fatal error on failure.

FILE *xtmpfile(char **namep,
	       const char *postfix_long, const char *postfix_short,
	       int do_unlink)
{
  char *templ = xtmptemplate(postfix_long, postfix_short);
  errno = 0;
  int fd = mkstemp(templ);
  if (fd < 0)
    fatal("cannot create temporary file: %1", strerror(errno));
  errno = 0;
  FILE *fp = fdopen(fd, FOPEN_RWB); // many callers of xtmpfile use binary I/O
  if (!fp)
    fatal("fdopen: %1", strerror(errno));
  if (do_unlink)
    add_tmp_file(templ);
  if (namep)
    *namep = templ;
  else
    a_delete templ;
  return fp;
}
