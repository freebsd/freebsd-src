/* search.c - searching subroutines using dfa, kwset and regex for grep.
   Copyright 1992, 1998, 2000 Free Software Foundation, Inc.

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
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
   02111-1307, USA.  */

/* Written August 1992 by Mike Haertel. */

/* $FreeBSD$ */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif
#include <sys/types.h>
#if defined HAVE_WCTYPE_H && defined HAVE_WCHAR_H && defined HAVE_MBRTOWC
/* We can handle multibyte string.  */
# define MBS_SUPPORT
# include <wchar.h>
# include <wctype.h>
#endif

#include "system.h"
#include "grep.h"
#include "regex.h"
#include "dfa.h"
#include "kwset.h"
#include "error.h"
#include "xalloc.h"
#ifdef HAVE_LIBPCRE
# include <pcre.h>
#endif

#define NCHAR (UCHAR_MAX + 1)

/* For -w, we also consider _ to be word constituent.  */
#define WCHAR(C) (ISALNUM(C) || (C) == '_')

/* DFA compiled regexp. */
static struct dfa dfa;

/* The Regex compiled patterns.  */
static struct patterns
{
  /* Regex compiled regexp. */
  struct re_pattern_buffer regexbuf;
  struct re_registers regs; /* This is here on account of a BRAIN-DEAD
			       Q@#%!# library interface in regex.c.  */
} patterns0;

struct patterns *patterns;
size_t pcount;

/* KWset compiled pattern.  For Ecompile and Gcompile, we compile
   a list of strings, at least one of which is known to occur in
   any string matching the regexp. */
static kwset_t kwset;

/* Number of compiled fixed strings known to exactly match the regexp.
   If kwsexec returns < kwset_exact_matches, then we don't need to
   call the regexp matcher at all. */
static int kwset_exact_matches;

#if defined(MBS_SUPPORT)
static char* check_multibyte_string PARAMS ((char const *buf, size_t size));
#endif
static void kwsinit PARAMS ((void));
static void kwsmusts PARAMS ((void));
static void Gcompile PARAMS ((char const *, size_t));
static void Ecompile PARAMS ((char const *, size_t));
static size_t EGexecute PARAMS ((char const *, size_t, size_t *, int ));
static void Fcompile PARAMS ((char const *, size_t));
static size_t Fexecute PARAMS ((char const *, size_t, size_t *, int));
static void Pcompile PARAMS ((char const *, size_t ));
static size_t Pexecute PARAMS ((char const *, size_t, size_t *, int));

void
dfaerror (char const *mesg)
{
  error (2, 0, mesg);
}

static void
kwsinit (void)
{
  static char trans[NCHAR];
  int i;

  if (match_icase)
    for (i = 0; i < NCHAR; ++i)
      trans[i] = TOLOWER (i);

  if (!(kwset = kwsalloc (match_icase ? trans : (char *) 0)))
    error (2, 0, _("memory exhausted"));
}

/* If the DFA turns out to have some set of fixed strings one of
   which must occur in the match, then we build a kwset matcher
   to find those strings, and thus quickly filter out impossible
   matches. */
static void
kwsmusts (void)
{
  struct dfamust const *dm;
  char const *err;

  if (dfa.musts)
    {
      kwsinit ();
      /* First, we compile in the substrings known to be exact
	 matches.  The kwset matcher will return the index
	 of the matching string that it chooses. */
      for (dm = dfa.musts; dm; dm = dm->next)
	{
	  if (!dm->exact)
	    continue;
	  ++kwset_exact_matches;
	  if ((err = kwsincr (kwset, dm->must, strlen (dm->must))) != 0)
	    error (2, 0, err);
	}
      /* Now, we compile the substrings that will require
	 the use of the regexp matcher.  */
      for (dm = dfa.musts; dm; dm = dm->next)
	{
	  if (dm->exact)
	    continue;
	  if ((err = kwsincr (kwset, dm->must, strlen (dm->must))) != 0)
	    error (2, 0, err);
	}
      if ((err = kwsprep (kwset)) != 0)
	error (2, 0, err);
    }
}

#ifdef MBS_SUPPORT
/* This function allocate the array which correspond to "buf".
   Then this check multibyte string and mark on the positions which
   are not singlebyte character nor the first byte of a multibyte
   character.  Caller must free the array.  */
static char*
check_multibyte_string(char const *buf, size_t size)
{
  char *mb_properties = xmalloc(size);
  mbstate_t cur_state;
  wchar_t wc;
  int i;
  memset(&cur_state, 0, sizeof(mbstate_t));
  memset(mb_properties, 0, sizeof(char)*size);
  for (i = 0; i < size ;)
    {
      size_t mbclen;
      mbclen = mbrtowc(&wc, buf + i, size - i, &cur_state);

      if (mbclen == (size_t) -1 || mbclen == (size_t) -2 || mbclen == 0)
	{
	  /* An invalid sequence, or a truncated multibyte character.
	     We treat it as a singlebyte character.  */
	  mbclen = 1;
	}
      else if (match_icase)
	{
	  if (iswupper((wint_t)wc))
	    {
	      wc = towlower((wint_t)wc);
	      wcrtomb(buf + i, wc, &cur_state);
	    }
	}
      mb_properties[i] = mbclen;
      i += mbclen;
    }

  return mb_properties;
}
#endif

static void
Gcompile (char const *pattern, size_t size)
{
  const char *err;
  char const *sep;
  size_t total = size;
  char const *motif = pattern;

  re_set_syntax (RE_SYNTAX_GREP | RE_HAT_LISTS_NOT_NEWLINE);
  dfasyntax (RE_SYNTAX_GREP | RE_HAT_LISTS_NOT_NEWLINE, match_icase, eolbyte);

  /* For GNU regex compiler we have to pass the patterns separately to detect
     errors like "[\nallo\n]\n".  The patterns here are "[", "allo" and "]"
     GNU regex should have raise a syntax error.  The same for backref, where
     the backref should have been local to each pattern.  */
  do
    {
      size_t len;
      sep = memchr (motif, '\n', total);
      if (sep)
	{
	  len = sep - motif;
	  sep++;
	  total -= (len + 1);
	}
      else
	{
	  len = total;
	  total = 0;
	}

      patterns = realloc (patterns, (pcount + 1) * sizeof (*patterns));
      if (patterns == NULL)
	error (2, errno, _("memory exhausted"));

      patterns[pcount] = patterns0;

      if ((err = re_compile_pattern (motif, len,
				    &(patterns[pcount].regexbuf))) != 0)
	error (2, 0, err);
      pcount++;

      motif = sep;
    } while (sep && total != 0);

  /* In the match_words and match_lines cases, we use a different pattern
     for the DFA matcher that will quickly throw out cases that won't work.
     Then if DFA succeeds we do some hairy stuff using the regex matcher
     to decide whether the match should really count. */
  if (match_words || match_lines)
    {
      /* In the whole-word case, we use the pattern:
	 \(^\|[^[:alnum:]_]\)\(userpattern\)\([^[:alnum:]_]|$\).
	 In the whole-line case, we use the pattern:
	 ^\(userpattern\)$.  */

      static char const line_beg[] = "^\\(";
      static char const line_end[] = "\\)$";
      static char const word_beg[] = "\\(^\\|[^[:alnum:]_]\\)\\(";
      static char const word_end[] = "\\)\\([^[:alnum:]_]\\|$\\)";
      char *n = xmalloc (sizeof word_beg - 1 + size + sizeof word_end);
      size_t i;
      strcpy (n, match_lines ? line_beg : word_beg);
      i = strlen (n);
      memcpy (n + i, pattern, size);
      i += size;
      strcpy (n + i, match_lines ? line_end : word_end);
      i += strlen (n + i);
      pattern = n;
      size = i;
    }

  dfacomp (pattern, size, &dfa, 1);
  kwsmusts ();
}

static void
Ecompile (char const *pattern, size_t size)
{
  const char *err;
  const char *sep;
  size_t total = size;
  char const *motif = pattern;

  if (strcmp (matcher, "awk") == 0)
    {
      re_set_syntax (RE_SYNTAX_AWK);
      dfasyntax (RE_SYNTAX_AWK, match_icase, eolbyte);
    }
  else
    {
      re_set_syntax (RE_SYNTAX_POSIX_EGREP);
      dfasyntax (RE_SYNTAX_POSIX_EGREP, match_icase, eolbyte);
    }

  /* For GNU regex compiler we have to pass the patterns separately to detect
     errors like "[\nallo\n]\n".  The patterns here are "[", "allo" and "]"
     GNU regex should have raise a syntax error.  The same for backref, where
     the backref should have been local to each pattern.  */
  do
    {
      size_t len;
      sep = memchr (motif, '\n', total);
      if (sep)
	{
	  len = sep - motif;
	  sep++;
	  total -= (len + 1);
	}
      else
	{
	  len = total;
	  total = 0;
	}

      patterns = realloc (patterns, (pcount + 1) * sizeof (*patterns));
      if (patterns == NULL)
	error (2, errno, _("memory exhausted"));
      patterns[pcount] = patterns0;

      if ((err = re_compile_pattern (motif, len,
				    &(patterns[pcount].regexbuf))) != 0)
	error (2, 0, err);
      pcount++;

      motif = sep;
    } while (sep && total != 0);

  /* In the match_words and match_lines cases, we use a different pattern
     for the DFA matcher that will quickly throw out cases that won't work.
     Then if DFA succeeds we do some hairy stuff using the regex matcher
     to decide whether the match should really count. */
  if (match_words || match_lines)
    {
      /* In the whole-word case, we use the pattern:
	 (^|[^[:alnum:]_])(userpattern)([^[:alnum:]_]|$).
	 In the whole-line case, we use the pattern:
	 ^(userpattern)$.  */

      static char const line_beg[] = "^(";
      static char const line_end[] = ")$";
      static char const word_beg[] = "(^|[^[:alnum:]_])(";
      static char const word_end[] = ")([^[:alnum:]_]|$)";
      char *n = xmalloc (sizeof word_beg - 1 + size + sizeof word_end);
      size_t i;
      strcpy (n, match_lines ? line_beg : word_beg);
      i = strlen(n);
      memcpy (n + i, pattern, size);
      i += size;
      strcpy (n + i, match_lines ? line_end : word_end);
      i += strlen (n + i);
      pattern = n;
      size = i;
    }

  dfacomp (pattern, size, &dfa, 1);
  kwsmusts ();
}

static size_t
EGexecute (char const *buf, size_t size, size_t *match_size, int exact)
{
  register char const *buflim, *beg, *end;
  char eol = eolbyte;
  int backref, start, len;
  struct kwsmatch kwsm;
  size_t i, ret_val;
#ifdef MBS_SUPPORT
  char *mb_properties = NULL;
  if (MB_CUR_MAX > 1)
    {
      if (match_icase)
        {
          char *case_buf = xmalloc(size);
          memcpy(case_buf, buf, size);
          buf = case_buf;
        }
      if (kwset)
        mb_properties = check_multibyte_string(buf, size);
    }
#endif /* MBS_SUPPORT */

  buflim = buf + size;

  for (beg = end = buf; end < buflim; beg = end)
    {
      if (!exact)
	{
	  if (kwset)
	    {
	      /* Find a possible match using the KWset matcher. */
	      size_t offset = kwsexec (kwset, beg, buflim - beg, &kwsm);
	      if (offset == (size_t) -1)
	        goto failure;
	      beg += offset;
	      /* Narrow down to the line containing the candidate, and
		 run it through DFA. */
	      end = memchr(beg, eol, buflim - beg);
	      end++;
#ifdef MBS_SUPPORT
	      if (MB_CUR_MAX > 1 && mb_properties[beg - buf] == 0)
		continue;
#endif
	      while (beg > buf && beg[-1] != eol)
		--beg;
	      if (kwsm.index < kwset_exact_matches)
		goto success_in_beg_and_end;
	      if (dfaexec (&dfa, beg, end - beg, &backref) == (size_t) -1)
		continue;
	    }
	  else
	    {
	      /* No good fixed strings; start with DFA. */
	      size_t offset = dfaexec (&dfa, beg, buflim - beg, &backref);
	      if (offset == (size_t) -1)
		break;
	      /* Narrow down to the line we've found. */
	      beg += offset;
	      end = memchr (beg, eol, buflim - beg);
	      end++;
	      while (beg > buf && beg[-1] != eol)
		--beg;
	    }
	  /* Successful, no backreferences encountered! */
	  if (!backref)
	    goto success_in_beg_and_end;
	}
      else
	end = beg + size;

      /* If we've made it to this point, this means DFA has seen
	 a probable match, and we need to run it through Regex. */
      for (i = 0; i < pcount; i++)
	{
	  patterns[i].regexbuf.not_eol = 0;
	  if (0 <= (start = re_search (&(patterns[i].regexbuf), beg,
				       end - beg - 1, 0,
				       end - beg - 1, &(patterns[i].regs))))
	    {
	      len = patterns[i].regs.end[0] - start;
	      if (exact && !match_words)
	        goto success_in_start_and_len;
	      if ((!match_lines && !match_words)
		  || (match_lines && len == end - beg - 1))
		goto success_in_beg_and_end;
	      /* If -w, check if the match aligns with word boundaries.
		 We do this iteratively because:
		 (a) the line may contain more than one occurence of the
		 pattern, and
		 (b) Several alternatives in the pattern might be valid at a
		 given point, and we may need to consider a shorter one to
		 find a word boundary.  */
	      if (match_words)
		while (start >= 0)
		  {
		    if ((start == 0 || !WCHAR ((unsigned char) beg[start - 1]))
			&& (len == end - beg - 1
			    || !WCHAR ((unsigned char) beg[start + len])))
		      goto success_in_beg_and_end;
		    if (len > 0)
		      {
			/* Try a shorter length anchored at the same place. */
			--len;
			patterns[i].regexbuf.not_eol = 1;
			len = re_match (&(patterns[i].regexbuf), beg,
					start + len, start,
					&(patterns[i].regs));
		      }
		    if (len <= 0)
		      {
			/* Try looking further on. */
			if (start == end - beg - 1)
			  break;
			++start;
			patterns[i].regexbuf.not_eol = 0;
			start = re_search (&(patterns[i].regexbuf), beg,
					   end - beg - 1,
					   start, end - beg - 1 - start,
					   &(patterns[i].regs));
			len = patterns[i].regs.end[0] - start;
		      }
		  }
	    }
	} /* for Regex patterns.  */
    } /* for (beg = end ..) */

 failure:
#ifdef MBS_SUPPORT
  if (MB_CUR_MAX > 1)
    {
      if (mb_properties)
	free (mb_properties);
      if (match_icase)
	free ((char *) buf);
    }
#endif /* MBS_SUPPORT */
  return (size_t) -1;

 success_in_beg_and_end:
  len = end - beg;
  start = beg - buf;
  /* FALLTHROUGH */

 success_in_start_and_len:
#ifdef MBS_SUPPORT
  if (MB_CUR_MAX > 1)
    {
      if (mb_properties)
	free (mb_properties);
      if (match_icase)
	free ((char *) buf);
    }
#endif /* MBS_SUPPORT */
  *match_size = len;
  return start;
}

static void
Fcompile (char const *pattern, size_t size)
{
  char const *beg, *lim, *err;

  kwsinit ();
  beg = pattern;
  do
    {
      for (lim = beg; lim < pattern + size && *lim != '\n'; ++lim)
	;
      if ((err = kwsincr (kwset, beg, lim - beg)) != 0)
	error (2, 0, err);
      if (lim < pattern + size)
	++lim;
      beg = lim;
    }
  while (beg < pattern + size);

  if ((err = kwsprep (kwset)) != 0)
    error (2, 0, err);
}

static size_t
Fexecute (char const *buf, size_t size, size_t *match_size, int exact)
{
  register char const *beg, *try, *end;
  register size_t len;
  char eol = eolbyte;
  struct kwsmatch kwsmatch;
  size_t ret_val;
#ifdef MBS_SUPPORT
  char *mb_properties = NULL;
  if (MB_CUR_MAX > 1)
    {
      if (match_icase)
        {
          char *case_buf = xmalloc(size);
          memcpy(case_buf, buf, size);
          buf = case_buf;
        }
      mb_properties = check_multibyte_string(buf, size);
    }
#endif /* MBS_SUPPORT */

  for (beg = buf; beg <= buf + size; ++beg)
    {
      size_t offset = kwsexec (kwset, beg, buf + size - beg, &kwsmatch);
      if (offset == (size_t) -1)
	goto failure;
#ifdef MBS_SUPPORT
      if (MB_CUR_MAX > 1 && offset + beg - buf < size
          && mb_properties[offset+beg-buf] == 0)
	continue; /* It is a part of multibyte character.  */
#endif /* MBS_SUPPORT */
      beg += offset;
      len = kwsmatch.size[0];
      if (exact && !match_words)
	goto success_in_beg_and_len;
      if (match_lines)
	{
	  if (beg > buf && beg[-1] != eol)
	    continue;
	  if (beg + len < buf + size && beg[len] != eol)
	    continue;
	  goto success;
	}
      else if (match_words)
	for (try = beg; len; )
	  {
	    if (try > buf && WCHAR((unsigned char) try[-1]))
	      break;
	    if (try + len < buf + size && WCHAR((unsigned char) try[len]))
	      {
		offset = kwsexec (kwset, beg, --len, &kwsmatch);
		if (offset == (size_t) -1)
		  {
#ifdef MBS_SUPPORT
		    if (MB_CUR_MAX > 1)
		      free (mb_properties);
#endif /* MBS_SUPPORT */
		    return offset;
		  }
		try = beg + offset;
		len = kwsmatch.size[0];
	      }
	    else
	      goto success;
	  }
      else
	goto success;
    }

 failure:
#ifdef MBS_SUPPORT
  if (MB_CUR_MAX > 1)
    {
      if (match_icase)
        free((char *) buf);
      if (mb_properties)
        free(mb_properties);
    }
#endif /* MBS_SUPPORT */
  return -1;

 success:
  end = memchr (beg + len, eol, (buf + size) - (beg + len));
  end++;
  while (buf < beg && beg[-1] != eol)
    --beg;
  len = end - beg;
  /* FALLTHROUGH */

 success_in_beg_and_len:
  *match_size = len;
#ifdef MBS_SUPPORT
  if (MB_CUR_MAX > 1)
    {
      if (mb_properties)
	free (mb_properties);
      if (match_icase)
	free ((char *) buf);
    }
#endif /* MBS_SUPPORT */
  return beg - buf;
}

#if HAVE_LIBPCRE
/* Compiled internal form of a Perl regular expression.  */
static pcre *cre;

/* Additional information about the pattern.  */
static pcre_extra *extra;
#endif

static void
Pcompile (char const *pattern, size_t size)
{
#if !HAVE_LIBPCRE
  error (2, 0, _("The -P option is not supported"));
#else
  int e;
  char const *ep;
  char *re = xmalloc (4 * size + 7);
  int flags = PCRE_MULTILINE | (match_icase ? PCRE_CASELESS : 0);
  char const *patlim = pattern + size;
  char *n = re;
  char const *p;
  char const *pnul;

  /* FIXME: Remove this restriction.  */
  if (eolbyte != '\n')
    error (2, 0, _("The -P and -z options cannot be combined"));

  *n = '\0';
  if (match_lines)
    strcpy (n, "^(");
  if (match_words)
    strcpy (n, "\\b(");
  n += strlen (n);

  /* The PCRE interface doesn't allow NUL bytes in the pattern, so
     replace each NUL byte in the pattern with the four characters
     "\000", removing a preceding backslash if there are an odd
     number of backslashes before the NUL.

     FIXME: This method does not work with some multibyte character
     encodings, notably Shift-JIS, where a multibyte character can end
     in a backslash byte.  */
  for (p = pattern; (pnul = memchr (p, '\0', patlim - p)); p = pnul + 1)
    {
      memcpy (n, p, pnul - p);
      n += pnul - p;
      for (p = pnul; pattern < p && p[-1] == '\\'; p--)
	continue;
      n -= (pnul - p) & 1;
      strcpy (n, "\\000");
      n += 4;
    }

  memcpy (n, p, patlim - p);
  n += patlim - p;
  *n = '\0';
  if (match_words)
    strcpy (n, ")\\b");
  if (match_lines)
    strcpy (n, ")$");

  cre = pcre_compile (re, flags, &ep, &e, pcre_maketables ());
  if (!cre)
    error (2, 0, ep);

  extra = pcre_study (cre, 0, &ep);
  if (ep)
    error (2, 0, ep);

  free (re);
#endif
}

static size_t
Pexecute (char const *buf, size_t size, size_t *match_size, int exact)
{
#if !HAVE_LIBPCRE
  abort ();
  return -1;
#else
  /* This array must have at least two elements; everything after that
     is just for performance improvement in pcre_exec.  */
  int sub[300];

  int e = pcre_exec (cre, extra, buf, size, 0, 0,
		     sub, sizeof sub / sizeof *sub);

  if (e <= 0)
    {
      switch (e)
	{
	case PCRE_ERROR_NOMATCH:
	  return -1;

	case PCRE_ERROR_NOMEMORY:
	  error (2, 0, _("Memory exhausted"));

	default:
	  abort ();
	}
    }
  else
    {
      /* Narrow down to the line we've found.  */
      char const *beg = buf + sub[0];
      char const *end = buf + sub[1];
      char const *buflim = buf + size;
      char eol = eolbyte;
      if (!exact)
	{
	  end = memchr (end, eol, buflim - end);
	  end++;
	  while (buf < beg && beg[-1] != eol)
	    --beg;
	}

      *match_size = end - beg;
      return beg - buf;
    }
#endif
}

struct matcher const matchers[] = {
  { "default", Gcompile, EGexecute },
  { "grep", Gcompile, EGexecute },
  { "egrep", Ecompile, EGexecute },
  { "awk", Ecompile, EGexecute },
  { "fgrep", Fcompile, Fexecute },
  { "perl", Pcompile, Pexecute },
  { "", 0, 0 },
};
