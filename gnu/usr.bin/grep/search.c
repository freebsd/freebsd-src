/* search.c - searching subroutines using dfa, kwset and regex for grep.
   Copyright (C) 1992 Free Software Foundation, Inc.

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
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   Written August 1992 by Mike Haertel. */

#include <ctype.h>

#ifdef STDC_HEADERS
#include <limits.h>
#include <stdlib.h>
#else
#define UCHAR_MAX 255
#include <sys/types.h>
extern char *malloc();
#endif

#ifdef HAVE_MEMCHR
#include <string.h>
#ifdef NEED_MEMORY_H
#include <memory.h>
#endif
#else
#ifdef __STDC__
extern void *memchr();
#else
extern char *memchr();
#endif
#endif

#if defined(HAVE_STRING_H) || defined(STDC_HEADERS)
#undef bcopy
#define bcopy(s, d, n) memcpy((d), (s), (n))
#endif

#if defined(isascii) && !defined(__FreeBSD__)
#define ISALNUM(C) (isascii(C) && isalnum(C))
#define ISUPPER(C) (isascii(C) && isupper(C))
#else
#define ISALNUM(C) isalnum(C)
#define ISUPPER(C) isupper(C)
#endif

#define TOLOWER(C) (ISUPPER(C) ? tolower(C) : (C))

#include "grep.h"
#include "dfa.h"
#include "kwset.h"
#include "gnuregex.h"

#define NCHAR (UCHAR_MAX + 1)

#if __STDC__
static void Gcompile(char *, size_t);
static void Ecompile(char *, size_t);
static char *EGexecute(char *, size_t, char **);
static void Fcompile(char *, size_t);
static char *Fexecute(char *, size_t, char **);
#else
static void Gcompile();
static void Ecompile();
static char *EGexecute();
static void Fcompile();
static char *Fexecute();
#endif

/* Here is the matchers vector for the main program. */
struct matcher matchers[] = {
  { "default", Gcompile, EGexecute },
  { "grep", Gcompile, EGexecute },
  { "ggrep", Gcompile, EGexecute },
  { "egrep", Ecompile, EGexecute },
  { "posix-egrep", Ecompile, EGexecute },
  { "gegrep", Ecompile, EGexecute },
  { "fgrep", Fcompile, Fexecute },
  { "gfgrep", Fcompile, Fexecute },
  { 0, 0, 0 },
};

/* For -w, we also consider _ to be word constituent.  */
#define WCHAR(C) (ISALNUM(C) || (C) == '_')

/* DFA compiled regexp. */
static struct dfa dfa;

/* Regex compiled regexp. */
static struct re_pattern_buffer regex;

/* KWset compiled pattern.  For Ecompile and Gcompile, we compile
   a list of strings, at least one of which is known to occur in
   any string matching the regexp. */
static kwset_t kwset;

/* Last compiled fixed string known to exactly match the regexp.
   If kwsexec() returns < lastexact, then we don't need to
   call the regexp matcher at all. */
static int lastexact;

void
dfaerror(mesg)
     char *mesg;
{
  fatal(mesg, 0);
}

static void
kwsinit()
{
  static char trans[NCHAR];
  int i;

  if (match_icase)
    for (i = 0; i < NCHAR; ++i)
      trans[i] = TOLOWER(i);

  if (!(kwset = kwsalloc(match_icase ? trans : (char *) 0)))
    fatal("memory exhausted", 0);
}

/* If the DFA turns out to have some set of fixed strings one of
   which must occur in the match, then we build a kwset matcher
   to find those strings, and thus quickly filter out impossible
   matches. */
static void
kwsmusts()
{
  struct dfamust *dm;
  char *err;

  if (dfa.musts)
    {
      kwsinit();
      /* First, we compile in the substrings known to be exact
	 matches.  The kwset matcher will return the index
	 of the matching string that it chooses. */
      for (dm = dfa.musts; dm; dm = dm->next)
	{
	  if (!dm->exact)
	    continue;
	  ++lastexact;
	  if ((err = kwsincr(kwset, dm->must, strlen(dm->must))) != 0)
	    fatal(err, 0);
	}
      /* Now, we compile the substrings that will require
	 the use of the regexp matcher.  */
      for (dm = dfa.musts; dm; dm = dm->next)
	{
	  if (dm->exact)
	    continue;
	  if ((err = kwsincr(kwset, dm->must, strlen(dm->must))) != 0)
	    fatal(err, 0);
	}
      if ((err = kwsprep(kwset)) != 0)
	fatal(err, 0);
    }
}

static void
Gcompile(pattern, size)
     char *pattern;
     size_t size;
{
#ifdef __STDC__
  const
#endif
  char *err;

  re_set_syntax(RE_SYNTAX_GREP | RE_HAT_LISTS_NOT_NEWLINE);
  dfasyntax(RE_SYNTAX_GREP | RE_HAT_LISTS_NOT_NEWLINE, match_icase);

  if ((err = re_compile_pattern(pattern, size, &regex)) != 0)
    fatal(err, 0);

  dfainit(&dfa);

  /* In the match_words and match_lines cases, we use a different pattern
     for the DFA matcher that will quickly throw out cases that won't work.
     Then if DFA succeeds we do some hairy stuff using the regex matcher
     to decide whether the match should really count. */
  if (match_words || match_lines)
    {
      /* In the whole-word case, we use the pattern:
	 (^|[^A-Za-z_])(userpattern)([^A-Za-z_]|$).
	 In the whole-line case, we use the pattern:
	 ^(userpattern)$.
	 BUG: Using [A-Za-z_] is locale-dependent!  */

      char *n = malloc(size + 50);
      int i = 0;

      strcpy(n, "");

      if (match_lines)
	strcpy(n, "^\\(");
      if (match_words)
	strcpy(n, "\\(^\\|[^0-9A-Za-z_]\\)\\(");

      i = strlen(n);
      bcopy(pattern, n + i, size);
      i += size;

      if (match_words)
	strcpy(n + i, "\\)\\([^0-9A-Za-z_]\\|$\\)");
      if (match_lines)
	strcpy(n + i, "\\)$");

      i += strlen(n + i);
      dfacomp(n, i, &dfa, 1);
    }
  else
    dfacomp(pattern, size, &dfa, 1);

  kwsmusts();
}

static void
Ecompile(pattern, size)
     char *pattern;
     size_t size;
{
#ifdef __STDC__
  const
#endif
  char *err;

  if (strcmp(matcher, "posix-egrep") == 0)
    {
      re_set_syntax(RE_SYNTAX_POSIX_EGREP);
      dfasyntax(RE_SYNTAX_POSIX_EGREP, match_icase);
    }
  else
    {
      re_set_syntax(RE_SYNTAX_EGREP);
      dfasyntax(RE_SYNTAX_EGREP, match_icase);
    }

  if ((err = re_compile_pattern(pattern, size, &regex)) != 0)
    fatal(err, 0);

  dfainit(&dfa);

  /* In the match_words and match_lines cases, we use a different pattern
     for the DFA matcher that will quickly throw out cases that won't work.
     Then if DFA succeeds we do some hairy stuff using the regex matcher
     to decide whether the match should really count. */
  if (match_words || match_lines)
    {
      /* In the whole-word case, we use the pattern:
	 (^|[^A-Za-z_])(userpattern)([^A-Za-z_]|$).
	 In the whole-line case, we use the pattern:
	 ^(userpattern)$.
	 BUG: Using [A-Za-z_] is locale-dependent!  */

      char *n = malloc(size + 50);
      int i = 0;

      strcpy(n, "");

      if (match_lines)
	strcpy(n, "^(");
      if (match_words)
	strcpy(n, "(^|[^0-9A-Za-z_])(");

      i = strlen(n);
      bcopy(pattern, n + i, size);
      i += size;

      if (match_words)
	strcpy(n + i, ")([^0-9A-Za-z_]|$)");
      if (match_lines)
	strcpy(n + i, ")$");

      i += strlen(n + i);
      dfacomp(n, i, &dfa, 1);
    }
  else
    dfacomp(pattern, size, &dfa, 1);

  kwsmusts();
}

static char *
EGexecute(buf, size, endp)
     char *buf;
     size_t size;
     char **endp;
{
  register char *buflim, *beg, *end, save;
  int backref, start, len;
  struct kwsmatch kwsm;
  static struct re_registers regs; /* This is static on account of a BRAIN-DEAD
				    Q@#%!# library interface in regex.c.  */

  buflim = buf + size;

  for (beg = end = buf; end < buflim; beg = end + 1)
    {
      if (kwset)
	{
	  /* Find a possible match using the KWset matcher. */
	  beg = kwsexec(kwset, beg, buflim - beg, &kwsm);
	  if (!beg)
	    goto failure;
	  /* Narrow down to the line containing the candidate, and
	     run it through DFA. */
	  end = memchr(beg, '\n', buflim - beg);
	  if (!end)
	    end = buflim;
	  while (beg > buf && beg[-1] != '\n')
	    --beg;
	  save = *end;
	  if (kwsm.index < lastexact)
	    goto success;
	  if (!dfaexec(&dfa, beg, end, 0, (int *) 0, &backref))
	    {
	      *end = save;
	      continue;
	    }
	  *end = save;
	  /* Successful, no backreferences encountered. */
	  if (!backref)
	    goto success;
	}
      else
	{
	  /* No good fixed strings; start with DFA. */
	  save = *buflim;
	  beg = dfaexec(&dfa, beg, buflim, 0, (int *) 0, &backref);
	  *buflim = save;
	  if (!beg)
	    goto failure;
	  /* Narrow down to the line we've found. */
	  end = memchr(beg, '\n', buflim - beg);
	  if (!end)
	    end = buflim;
	  while (beg > buf && beg[-1] != '\n')
	    --beg;
	  /* Successful, no backreferences encountered! */
	  if (!backref)
	    goto success;
	}
      /* If we've made it to this point, this means DFA has seen
	 a probable match, and we need to run it through Regex. */
      regex.not_eol = 0;
      if ((start = re_search(&regex, beg, end - beg, 0, end - beg, &regs)) >= 0)
	{
	  len = regs.end[0] - start;
	  if (!match_lines && !match_words || match_lines && len == end - beg)
	    goto success;
	  /* If -w, check if the match aligns with word boundaries.
	     We do this iteratively because:
	     (a) the line may contain more than one occurence of the pattern, and
	     (b) Several alternatives in the pattern might be valid at a given
	     point, and we may need to consider a shorter one to find a word
	     boundary. */
	  if (match_words)
	    while (start >= 0)
	      {
		if ((start == 0 || !WCHAR(beg[start - 1]))
		    && (len == end - beg || !WCHAR(beg[start + len])))
		  goto success;
		if (len > 0)
		  {
		    /* Try a shorter length anchored at the same place. */
		    --len;
		    regex.not_eol = 1;
		    len = re_match(&regex, beg, start + len, start, &regs);
		  }
		if (len <= 0)
		  {
		    /* Try looking further on. */
		    if (start == end - beg)
		      break;
		    ++start;
		    regex.not_eol = 0;
		    start = re_search(&regex, beg, end - beg,
				      start, end - beg - start, &regs);
		    len = regs.end[0] - start;
		  }
	      }
	}
    }

 failure:
  return 0;

 success:
  *endp = end < buflim ? end + 1 : end;
  return beg;
}

static void
Fcompile(pattern, size)
     char *pattern;
     size_t size;
{
  char *beg, *lim, *err;

  kwsinit();
  beg = pattern;
  do
    {
      for (lim = beg; lim < pattern + size && *lim != '\n'; ++lim)
	;
      if ((err = kwsincr(kwset, beg, lim - beg)) != 0)
	fatal(err, 0);
      if (lim < pattern + size)
	++lim;
      beg = lim;
    }
  while (beg < pattern + size);

  if ((err = kwsprep(kwset)) != 0)
    fatal(err, 0);
}

static char *
Fexecute(buf, size, endp)
     char *buf;
     size_t size;
     char **endp;
{
  register char *beg, *try, *end;
  register size_t len;
  struct kwsmatch kwsmatch;

  for (beg = buf; beg <= buf + size; ++beg)
    {
      if (!(beg = kwsexec(kwset, beg, buf + size - beg, &kwsmatch)))
	return 0;
      len = kwsmatch.size[0];
      if (match_lines)
	{
	  if (beg > buf && beg[-1] != '\n')
	    continue;
	  if (beg + len < buf + size && beg[len] != '\n')
	    continue;
	  goto success;
	}
      else if (match_words)
	for (try = beg; len && try;)
	  {
	    if (try > buf && WCHAR((unsigned char) try[-1]))
	      break;
	    if (try + len < buf + size && WCHAR((unsigned char) try[len]))
	      {
		try = kwsexec(kwset, beg, --len, &kwsmatch);
		len = kwsmatch.size[0];
	      }
	    else
	      goto success;
	  }
      else
	goto success;
    }

  return 0;

 success:
  if ((end = memchr(beg + len, '\n', (buf + size) - (beg + len))) != 0)
    ++end;
  else
    end = buf + size;
  *endp = end;
  while (beg > buf && beg[-1] != '\n')
    --beg;
  return beg;
}
