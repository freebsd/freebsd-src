/* exclude.c -- exclude file names

   Copyright 1992, 1993, 1994, 1997, 1999, 2000, 2001 Free Software
   Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.
   If not, write to the Free Software Foundation,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* Written by Paul Eggert <eggert@twinsun.com>  */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if HAVE_STDBOOL_H
# include <stdbool.h>
#else
typedef enum {false = 0, true = 1} bool;
#endif

#include <errno.h>
#ifndef errno
extern int errno;
#endif
#include <stdio.h>
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_STDLIB_H
# include <stdlib.h>
#endif
#if HAVE_STRING_H
# include <string.h>
#endif
#if HAVE_STRINGS_H
# include <strings.h>
#endif
#if HAVE_INTTYPES_H
# include <inttypes.h>
#else
# if HAVE_STDINT_H
#  include <stdint.h>
# endif
#endif

#include "exclude.h"
#include "fnmatch.h"
#include "xalloc.h"

#ifndef SIZE_MAX
# define SIZE_MAX ((size_t) -1)
#endif

/* Verify a requirement at compile-time (unlike assert, which is runtime).  */
#define verify(name, assertion) struct name { char a[(assertion) ? 1 : -1]; }

verify (EXCLUDE_macros_do_not_collide_with_FNM_macros,
	(((EXCLUDE_ANCHORED | EXCLUDE_INCLUDE | EXCLUDE_WILDCARDS)
	  & (FNM_FILE_NAME | FNM_NOESCAPE | FNM_PERIOD | FNM_LEADING_DIR
	     | FNM_CASEFOLD))
	 == 0));

/* An exclude pattern-options pair.  The options are fnmatch options
   ORed with EXCLUDE_* options.  */

struct patopts
  {
    char const *pattern;
    int options;
  };

/* An exclude list, of pattern-options pairs.  */

struct exclude
  {
    struct patopts *exclude;
    size_t exclude_alloc;
    size_t exclude_count;
  };

/* Return a newly allocated and empty exclude list.  */

struct exclude *
new_exclude (void)
{
  struct exclude *ex = (struct exclude *) xmalloc (sizeof *ex);
  ex->exclude_count = 0;
  ex->exclude_alloc = (1 << 6); /* This must be a power of 2.  */
  ex->exclude = (struct patopts *) xmalloc (ex->exclude_alloc
					    * sizeof ex->exclude[0]);
  return ex;
}

/* Free the storage associated with an exclude list.  */

void
free_exclude (struct exclude *ex)
{
  free (ex->exclude);
  free (ex);
}

/* Return zero if PATTERN matches F, obeying OPTIONS, except that
   (unlike fnmatch) wildcards are disabled in PATTERN.  */

static int
fnmatch_no_wildcards (char const *pattern, char const *f, int options)
{
  if (! (options & FNM_LEADING_DIR))
    return ((options & FNM_CASEFOLD)
	    ? strcasecmp (pattern, f)
	    : strcmp (pattern, f));
  else
    {
      size_t patlen = strlen (pattern);
      int r = ((options & FNM_CASEFOLD)
		? strncasecmp (pattern, f, patlen)
		: strncmp (pattern, f, patlen));
      if (! r)
	{
	  r = f[patlen];
	  if (r == '/')
	    r = 0;
	}
      return r;
    }
}

/* Return true if EX excludes F.  */

bool
excluded_filename (struct exclude const *ex, char const *f)
{
  size_t exclude_count = ex->exclude_count;

  /* If no options are given, the default is to include.  */
  if (exclude_count == 0)
    return 0;
  else
    {
      struct patopts const *exclude = ex->exclude;
      size_t i;

      /* Otherwise, the default is the opposite of the first option.  */
      bool excluded = !! (exclude[0].options & EXCLUDE_INCLUDE);

      /* Scan through the options, seeing whether they change F from
	 excluded to included or vice versa.  */
      for (i = 0;  i < exclude_count;  i++)
	{
	  char const *pattern = exclude[i].pattern;
	  int options = exclude[i].options;
	  if (excluded == !! (options & EXCLUDE_INCLUDE))
	    {
	      int (*matcher) PARAMS ((char const *, char const *, int)) =
		(options & EXCLUDE_WILDCARDS
		 ? fnmatch
		 : fnmatch_no_wildcards);
	      bool matched = ((*matcher) (pattern, f, options) == 0);
	      char const *p;

	      if (! (options & EXCLUDE_ANCHORED))
		for (p = f; *p && ! matched; p++)
		  if (*p == '/' && p[1] != '/')
		    matched = ((*matcher) (pattern, p + 1, options) == 0);

	      excluded ^= matched;
	    }
	}

      return excluded;
    }
}

/* Append to EX the exclusion PATTERN with OPTIONS.  */

void
add_exclude (struct exclude *ex, char const *pattern, int options)
{
  struct patopts *patopts;

  if (ex->exclude_alloc <= ex->exclude_count)
    {
      size_t s = 2 * ex->exclude_alloc;
      if (! (0 < s && s <= SIZE_MAX / sizeof ex->exclude[0]))
	xalloc_die ();
      ex->exclude_alloc = s;
      ex->exclude = (struct patopts *) xrealloc (ex->exclude,
						 s * sizeof ex->exclude[0]);
    }

  patopts = &ex->exclude[ex->exclude_count++];
  patopts->pattern = pattern;
  patopts->options = options;
}

/* Use ADD_FUNC to append to EX the patterns in FILENAME, each with
   OPTIONS.  LINE_END terminates each pattern in the file.  Return -1
   on failure, 0 on success.  */

int
add_exclude_file (void (*add_func) PARAMS ((struct exclude *,
					    char const *, int)),
		  struct exclude *ex, char const *filename, int options,
		  char line_end)
{
  bool use_stdin = filename[0] == '-' && !filename[1];
  FILE *in;
  char *buf;
  char *p;
  char const *pattern;
  char const *lim;
  size_t buf_alloc = (1 << 10);  /* This must be a power of two.  */
  size_t buf_count = 0;
  int c;
  int e = 0;

  if (use_stdin)
    in = stdin;
  else if (! (in = fopen (filename, "r")))
    return -1;

  buf = xmalloc (buf_alloc);

  while ((c = getc (in)) != EOF)
    {
      buf[buf_count++] = c;
      if (buf_count == buf_alloc)
	{
	  buf_alloc *= 2;
	  if (! buf_alloc)
	    xalloc_die ();
	  buf = xrealloc (buf, buf_alloc);
	}
    }

  if (ferror (in))
    e = errno;

  if (!use_stdin && fclose (in) != 0)
    e = errno;

  buf = xrealloc (buf, buf_count + 1);

  for (pattern = p = buf, lim = buf + buf_count;  p <= lim;  p++)
    if (p < lim ? *p == line_end : buf < p && p[-1])
      {
	*p = '\0';
	(*add_func) (ex, pattern, options);
	pattern = p + 1;
      }

  errno = e;
  return e ? -1 : 0;
}
