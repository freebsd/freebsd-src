/* $FreeBSD$ */

/*-
 * Copyright (c) 1999 James Howard and Dag-Erling Coïdan Smørgrav
 * Copyright (C) 2008-2011 Gabor Kovesdan <gabor@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "glue.h"

#include <ctype.h>
#include <limits.h>
#include <regex.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#ifdef TRE_WCHAR
#include <wchar.h>
#include <wctype.h>
#endif

#include "hashtable.h"
#include "tre-fastmatch.h"
#include "xmalloc.h"

static int	fastcmp(const fastmatch_t *fg, const void *data,
			tre_str_type_t type);

/*
 * Clean up if pattern compilation fails.
 */
#define FAIL_COMP(errcode)						\
  {									\
    if (fg->pattern)							\
      xfree(fg->pattern);						\
    if (fg->wpattern)							\
      xfree(fg->wpattern);						\
    if (fg->qsBc_table)							\
      hashtable_free(fg->qsBc_table);					\
    fg = NULL;								\
    return errcode;							\
  }

/*
 * Skips n characters in the input string and assigns the start
 * address to startptr. Note: as per IEEE Std 1003.1-2008
 * matching is based on bit pattern not character representations
 * so we can handle MB strings as byte sequences just like
 * SB strings.
 */
#define SKIP_CHARS(n)							\
  switch (type)								\
    {									\
      case STR_WIDE:							\
	startptr = str_wide + n;					\
	break;								\
      default:								\
	startptr = str_byte + n;					\
    }

/*
 * Converts the wide string pattern to SB/MB string and stores
 * it in fg->pattern. Sets fg->len to the byte length of the
 * converted string.
 */
#define STORE_MBS_PAT							\
  {									\
    size_t siz;								\
									\
    siz = wcstombs(NULL, fg->wpattern, 0);				\
    if (siz == (size_t)-1)						\
      return REG_BADPAT;						\
    fg->len = siz;							\
    fg->pattern = xmalloc(siz + 1);					\
    if (fg->pattern == NULL)						\
      return REG_ESPACE;						\
    wcstombs(fg->pattern, fg->wpattern, siz);				\
    fg->pattern[siz] = '\0';						\
  }									\

#define IS_OUT_OF_BOUNDS						\
  ((!fg->reversed							\
    ? ((type == STR_WIDE) ? ((j + fg->wlen) > len)			\
			  : ((j + fg->len) > len))			\
    : (j < 0)))

/*
 * Checks whether the new position after shifting in the input string
 * is out of the bounds and break out from the loop if so.
 */
#define CHECKBOUNDS							\
  if (IS_OUT_OF_BOUNDS)							\
    break;								\

/*
 * Shifts in the input string after a mismatch. The position of the
 * mismatch is stored in the mismatch variable.
 */
#define SHIFT								\
  CHECKBOUNDS;								\
									\
  {									\
    int r = -1;								\
    unsigned int bc = 0, gs = 0, ts;					\
									\
    switch (type)							\
      {									\
	case STR_WIDE:							\
	  if (!fg->hasdot)						\
	    {								\
	      if (u != 0 && (unsigned)mismatch == fg->wlen - 1 - shift)	\
		mismatch -= u;						\
	      v = fg->wlen - 1 - mismatch;				\
	      r = hashtable_get(fg->qsBc_table,				\
		&str_wide[!fg->reversed ? (size_t)j + fg->wlen		\
			  : (size_t)j - 1], &bc);			\
	      gs = fg->bmGs[mismatch];					\
	    }								\
	    bc = (r == HASH_OK) ? bc : fg->defBc;			\
	    DPRINT(("tre_fast_match: mismatch on character" CHF ", "	\
		    "BC %d, GS %d\n",					\
		    ((const tre_char_t *)startptr)[mismatch + 1],	\
		    bc, gs));						\
            break;							\
	default:							\
	  if (!fg->hasdot)						\
	    {								\
	      if (u != 0 && (unsigned)mismatch == fg->len - 1 - shift)	\
		mismatch -= u;						\
	      v = fg->len - 1 - mismatch;				\
	      gs = fg->sbmGs[mismatch];					\
	    }								\
	  bc = fg->qsBc[((const unsigned char *)str_byte)		\
			[!fg->reversed ? (size_t)j + fg->len		\
			 : (size_t)j - 1]];				\
	  DPRINT(("tre_fast_match: mismatch on character %c, "		\
		 "BC %d, GS %d\n",					\
		 ((const unsigned char *)startptr)[mismatch + 1],	\
		 bc, gs));						\
      }									\
    if (fg->hasdot)							\
      shift = bc;							\
    else								\
      {									\
	ts = (u >= v) ? (u - v) : 0;					\
	shift = MAX(ts, bc);						\
	shift = MAX(shift, gs);						\
	if (shift == gs)						\
	  u = MIN((type == STR_WIDE ? fg->wlen : fg->len) - shift, v);	\
	else								\
	  {								\
	    if (ts < bc)						\
	      shift = MAX(shift, u + 1);				\
	    u = 0;							\
	  }								\
      }									\
      DPRINT(("tre_fast_match: shifting %u characters\n", shift));	\
      j = !fg->reversed ? j + shift : j - shift;			\
  }

/*
 * Normal Quick Search would require a shift based on the position the
 * next character after the comparison is within the pattern.  With
 * wildcards, the position of the last dot effects the maximum shift
 * distance.
 * The closer to the end the wild card is the slower the search.
 *
 * Examples:
 * Pattern    Max shift
 * -------    ---------
 * this               5
 * .his               4
 * t.is               3
 * th.s               2
 * thi.               1
 */

/*
 * Fills in the bad character shift array for SB/MB strings.
 */
#define FILL_QSBC							\
  if (fg->reversed)							\
    {									\
      _FILL_QSBC_REVERSED						\
    }									\
  else									\
    {									\
      _FILL_QSBC							\
    }

#define _FILL_QSBC							\
  for (unsigned int i = 0; i <= UCHAR_MAX; i++)				\
    fg->qsBc[i] = fg->len - hasdot;					\
  for (unsigned int i = hasdot + 1; i < fg->len; i++)			\
    {									\
      fg->qsBc[(unsigned char)fg->pattern[i]] = fg->len - i;		\
      DPRINT(("BC shift for char %c is %zu\n", fg->pattern[i],		\
	     fg->len - i));						\
      if (fg->icase)							\
	{								\
	  char c = islower((unsigned char)fg->pattern[i]) ?		\
		   toupper((unsigned char)fg->pattern[i]) :		\
		   tolower((unsigned char)fg->pattern[i]);		\
	  fg->qsBc[(unsigned char)c] = fg->len - i;			\
	  DPRINT(("BC shift for char %c is %zu\n", c, fg->len - i));	\
	}								\
    }

#define _FILL_QSBC_REVERSED						\
  for (unsigned int i = 0; i <= UCHAR_MAX; i++)				\
    fg->qsBc[i] = firstdot + 1;						\
  for (int i = firstdot - 1; i >= 0; i--)				\
    {									\
      fg->qsBc[(unsigned char)fg->pattern[i]] = i + 1;			\
      DPRINT(("Reverse BC shift for char %c is %d\n", fg->pattern[i],	\
	     i + 1));							\
      if (fg->icase)							\
        {								\
          char c = islower((unsigned char)fg->pattern[i]) ?		\
		   toupper((unsigned char)fg->pattern[i]) :		\
		   tolower((unsigned char)fg->pattern[i]);		\
          fg->qsBc[(unsigned char)c] = i + 1;				\
	  DPRINT(("Reverse BC shift for char %c is %d\n", c, i + 1));	\
        }								\
    }

/*
 * Fills in the bad character shifts into a hastable for wide strings.
 * With wide characters it is not possible any more to use a normal
 * array because there are too many characters and we could not
 * provide enough memory. Fortunately, we only have to store distinct
 * values for so many characters as the number of distinct characters
 * in the pattern, so we can store them in a hashtable and store a
 * default shift value for the rest.
 */
#define FILL_QSBC_WIDE							\
  if (fg->reversed)							\
    {									\
      _FILL_QSBC_WIDE_REVERSED						\
    }									\
  else									\
    {									\
      _FILL_QSBC_WIDE							\
    }

#define _FILL_QSBC_WIDE							\
  /* Adjust the shift based on location of the last dot ('.'). */	\
  fg->defBc = fg->wlen - whasdot;					\
									\
  /* Preprocess pattern. */						\
  fg->qsBc_table = hashtable_init(fg->wlen * (fg->icase ? 8 : 4),	\
				  sizeof(tre_char_t), sizeof(int));	\
  if (!fg->qsBc_table)							\
    FAIL_COMP(REG_ESPACE);						\
  for (unsigned int i = whasdot + 1; i < fg->wlen; i++)			\
    {									\
      int k = fg->wlen - i;						\
      int r;								\
									\
      r = hashtable_put(fg->qsBc_table, &fg->wpattern[i], &k);		\
      if ((r == HASH_FAIL) || (r == HASH_FULL))				\
	FAIL_COMP(REG_ESPACE);						\
      DPRINT(("BC shift for wide char " CHF " is %d\n", fg->wpattern[i],\
	     k));							\
      if (fg->icase)							\
	{								\
	  tre_char_t wc = iswlower(fg->wpattern[i]) ?			\
	    towupper(fg->wpattern[i]) : towlower(fg->wpattern[i]);	\
	  r = hashtable_put(fg->qsBc_table, &wc, &k);			\
	  if ((r == HASH_FAIL) || (r == HASH_FULL))			\
	    FAIL_COMP(REG_ESPACE);					\
	  DPRINT(("BC shift for wide char " CHF " is %d\n", wc, k));	\
	}								\
    }

#define _FILL_QSBC_WIDE_REVERSED					\
  /* Adjust the shift based on location of the last dot ('.'). */	\
  fg->defBc = (size_t)wfirstdot;					\
									\
  /* Preprocess pattern. */						\
  fg->qsBc_table = hashtable_init(fg->wlen * (fg->icase ? 8 : 4),	\
				  sizeof(tre_char_t), sizeof(int));	\
  if (!fg->qsBc_table)							\
    FAIL_COMP(REG_ESPACE);						\
  for (int i = wfirstdot - 1; i >= 0; i--)				\
    {									\
      int k = i + 1;							\
      int r;								\
									\
      r = hashtable_put(fg->qsBc_table, &fg->wpattern[i], &k);		\
      if ((r == HASH_FAIL) || (r == HASH_FULL))				\
	FAIL_COMP(REG_ESPACE);						\
      DPRINT(("Reverse BC shift for wide char " CHF " is %d\n",		\
	     fg->wpattern[i], k));					\
      if (fg->icase)							\
	{								\
	  tre_char_t wc = iswlower(fg->wpattern[i]) ?			\
	    towupper(fg->wpattern[i]) : towlower(fg->wpattern[i]);	\
	  r = hashtable_put(fg->qsBc_table, &wc, &k);			\
	  if ((r == HASH_FAIL) || (r == HASH_FULL))			\
	    FAIL_COMP(REG_ESPACE);					\
	  DPRINT(("Reverse BC shift for wide char " CHF " is %d\n", wc,	\
		 k));							\
	}								\
    }

#ifdef _GREP_DEBUG
#define DPRINT_BMGS(len, fmt_str, sh)					\
  for (unsigned int i = 0; i < len; i++)				\
    DPRINT((fmt_str, i, sh[i]));
#else
#define DPRINT_BMGS(len, fmt_str, sh)					\
  do { } while(/*CONSTCOND*/0)
#endif

/*
 * Fills in the good suffix table for SB/MB strings.
 */
#define FILL_BMGS							\
  if (!fg->hasdot)							\
    {									\
      fg->sbmGs = xmalloc(fg->len * sizeof(int));			\
      if (!fg->sbmGs)							\
	return REG_ESPACE;						\
      if (fg->len == 1)							\
	fg->sbmGs[0] = 1;						\
      else								\
	_FILL_BMGS(fg->sbmGs, fg->pattern, fg->len, false);		\
      DPRINT_BMGS(fg->len, "GS shift for pos %d is %d\n", fg->sbmGs);	\
    }

/*
 * Fills in the good suffix table for wide strings.
 */
#define FILL_BMGS_WIDE							\
  if (!fg->hasdot)							\
    {									\
      fg->bmGs = xmalloc(fg->wlen * sizeof(int));			\
      if (!fg->bmGs)							\
	return REG_ESPACE;						\
      if (fg->wlen == 1)						\
	fg->bmGs[0] = 1;						\
      else								\
	_FILL_BMGS(fg->bmGs, fg->wpattern, fg->wlen, true);		\
      DPRINT_BMGS(fg->wlen, "GS shift (wide) for pos %d is %d\n",	\
		  fg->bmGs);						\
    }

#define _FILL_BMGS(arr, pat, plen, wide)				\
  {									\
    char *p;								\
    tre_char_t *wp;							\
									\
    if (wide)								\
      {									\
	if (fg->icase)							\
	  {								\
	    wp = xmalloc(plen * sizeof(tre_char_t));			\
	    if (wp == NULL)						\
	      return REG_ESPACE;					\
	    for (unsigned int i = 0; i < plen; i++)			\
	      wp[i] = towlower(pat[i]);					\
	    _CALC_BMGS(arr, wp, plen);					\
	    xfree(wp);							\
	  }								\
	else								\
	  _CALC_BMGS(arr, pat, plen);					\
      }									\
    else								\
      {									\
	if (fg->icase)							\
	  {								\
	    p = xmalloc(plen);						\
	    if (p == NULL)						\
	      return REG_ESPACE;					\
	    for (unsigned int i = 0; i < plen; i++)			\
	      p[i] = tolower((unsigned char)pat[i]);                    \
	    _CALC_BMGS(arr, p, plen);					\
	    xfree(p);							\
	  }								\
	else								\
	  _CALC_BMGS(arr, pat, plen);					\
      }									\
  }

#define _CALC_BMGS(arr, pat, plen)					\
  {									\
    int f = 0, g;							\
									\
    int *suff = xmalloc(plen * sizeof(int));				\
    if (suff == NULL)							\
      return REG_ESPACE;						\
									\
    suff[plen - 1] = plen;						\
    g = plen - 1;							\
    for (int i = plen - 2; i >= 0; i--)					\
      {									\
	if (i > g && suff[i + plen - 1 - f] < i - g)			\
	  suff[i] = suff[i + plen - 1 - f];				\
	else								\
	  {								\
	    if (i < g)							\
	      g = i;							\
	    f = i;							\
	    while (g >= 0 && pat[g] == pat[g + plen - 1 - f])		\
	      g--;							\
	    suff[i] = f - g;						\
	  }								\
      }									\
									\
    for (unsigned int i = 0; i < plen; i++)				\
      arr[i] = plen;							\
    g = 0;								\
    for (int i = plen - 1; i >= 0; i--)					\
      if (suff[i] == i + 1)						\
	for(; (unsigned long)g < plen - 1 - i; g++)			\
	  if (arr[g] == plen)						\
	    arr[g] = plen - 1 - i;					\
    for (unsigned int i = 0; i <= plen - 2; i++)			\
      arr[plen - 1 - suff[i]] = plen - 1 - i;				\
									\
    xfree(suff);							\
  }

/*
 * Copies the pattern pat having length n to p and stores
 * the size in l.
 */
#define SAVE_PATTERN(src, srclen, dst, dstlen)				\
  dstlen = srclen;							\
  dst = xmalloc((dstlen + 1) * sizeof(tre_char_t));			\
  if (dst == NULL)							\
    return REG_ESPACE;							\
  if (dstlen > 0)							\
    memcpy(dst, src, dstlen * sizeof(tre_char_t));			\
  dst[dstlen] = TRE_CHAR('\0');

/*
 * Initializes pattern compiling.
 */
#define INIT_COMP							\
  /* Initialize. */							\
  memset(fg, 0, sizeof(*fg));						\
  fg->icase = (cflags & REG_ICASE);					\
  fg->word = (cflags & REG_WORD);					\
  fg->newline = (cflags & REG_NEWLINE);					\
  fg->nosub = (cflags & REG_NOSUB);					\
									\
  /* Cannot handle REG_ICASE with MB string */				\
  if (fg->icase && (TRE_MB_CUR_MAX > 1) && n > 0)			\
    {									\
      DPRINT(("Cannot use fast matcher for MBS with REG_ICASE\n"));	\
      return REG_BADPAT;						\
    }

/*
 * Checks whether we have a 0-length pattern that will match
 * anything. If literal is set to false, the EOL anchor is also
 * taken into account.
 */
#define CHECK_MATCHALL(literal)						\
  if (!literal && n == 1 && pat[0] == TRE_CHAR('$'))			\
    {									\
      n--;								\
      fg->eol = true;							\
    }									\
									\
  if (n == 0)								\
    {									\
      fg->matchall = true;						\
      fg->pattern = xmalloc(sizeof(char));				\
      if (!fg->pattern)							\
	FAIL_COMP(REG_ESPACE);						\
      fg->pattern[0] = '\0';						\
      fg->wpattern = xmalloc(sizeof(tre_char_t));			\
      if (!fg->wpattern)						\
	FAIL_COMP(REG_ESPACE);						\
      fg->wpattern[0] = TRE_CHAR('\0');					\
      DPRINT(("Matching every input\n"));				\
      return REG_OK;							\
    }

/*
 * Returns: REG_OK on success, error code otherwise
 */
int
tre_compile_literal(fastmatch_t *fg, const tre_char_t *pat, size_t n,
		    int cflags)
{
  size_t hasdot = 0, whasdot = 0;
  ssize_t firstdot = -1, wfirstdot = -1;

  INIT_COMP;

  CHECK_MATCHALL(true);

  /* Cannot handle word boundaries with MB string */
  if (fg->word && (TRE_MB_CUR_MAX > 1))
    return REG_BADPAT;

#ifdef TRE_WCHAR
  SAVE_PATTERN(pat, n, fg->wpattern, fg->wlen);
  STORE_MBS_PAT;
#else
  SAVE_PATTERN(pat, n, fg->pattern, fg->len);
#endif

  DPRINT(("tre_compile_literal: pattern: %s, len %zu, icase: %c, word: %c, "
	 "newline %c\n", fg->pattern, fg->len, fg->icase ? 'y' : 'n',
	 fg->word ? 'y' : 'n', fg->newline ? 'y' : 'n'));

  FILL_QSBC;
  FILL_BMGS;
#ifdef TRE_WCHAR
  FILL_QSBC_WIDE;
  FILL_BMGS_WIDE;
#endif

  return REG_OK;
}

/*
 * Returns: REG_OK on success, error code otherwise
 */
int
tre_compile_fast(fastmatch_t *fg, const tre_char_t *pat, size_t n,
		 int cflags)
{
  tre_char_t *tmp;
  size_t pos = 0, hasdot = 0, whasdot = 0;
  ssize_t firstdot = -1, wfirstdot = -1;
  bool escaped = false;
  bool *_escmap = NULL;

  INIT_COMP;

  /* Remove beginning-of-line character ('^'). */
  if (pat[0] == TRE_CHAR('^'))
    {
      fg->bol = true;
      n--;
      pat++;
    }

  CHECK_MATCHALL(false);

  /* Handle word-boundary matching when GNU extensions are enabled */
  if ((cflags & REG_GNU) && (n >= 14) &&
      (memcmp(pat, TRE_CHAR("[[:<:]]"), 7 * sizeof(tre_char_t)) == 0) &&
      (memcmp(pat + n - 7, TRE_CHAR("[[:>:]]"),
	      7 * sizeof(tre_char_t)) == 0))
    {
      n -= 14;
      pat += 7;
      fg->word = true;
    }

  /* Cannot handle word boundaries with MB string */
  if (fg->word && (TRE_MB_CUR_MAX > 1))
    return REG_BADPAT;

  tmp = xmalloc((n + 1) * sizeof(tre_char_t));
  if (tmp == NULL)
    return REG_ESPACE;

/* Copies the char into the stored pattern and skips to the next char. */
#define STORE_CHAR							\
  do									\
    {									\
      tmp[pos++] = pat[i];						\
      escaped = false;							\
      continue;								\
    } while (0)

  /* Traverse the input pattern for processing */
  for (unsigned int i = 0; i < n; i++)
    {
      switch (pat[i])
	{
	  case TRE_CHAR('\\'):
	    if (escaped)
	      STORE_CHAR;
	    else if (i == n - 1)
	      goto badpat;
	    else
	      escaped = true;
	    continue;
	  case TRE_CHAR('['):
	    if (escaped)
	      STORE_CHAR;
	    else
	      goto badpat;
	    continue;
	  case TRE_CHAR('*'):
	    if (escaped || (!(cflags & REG_EXTENDED) && (i == 0)))
	      STORE_CHAR;
	    else
	      goto badpat;
	    continue;
	  case TRE_CHAR('+'):
	  case TRE_CHAR('?'):
	    if ((cflags & REG_EXTENDED) && (i == 0))
	      goto badpat;
	    else if ((cflags & REG_EXTENDED) ^ !escaped)
	      STORE_CHAR;
	    else
	      goto badpat;
	    continue;
	  case TRE_CHAR('.'):
	    if (escaped)
	      {
		if (!_escmap)
		  _escmap = xmalloc(n * sizeof(bool));
		if (!_escmap)
		  {
		    xfree(tmp);
		    return REG_ESPACE;
		  }
		_escmap[i] = true;
		STORE_CHAR;
	      }
	    else
	      {
		whasdot = i;
		if (wfirstdot == -1)
			wfirstdot = i;
		STORE_CHAR;
	      }
	    continue;
	  case TRE_CHAR('^'):
	    STORE_CHAR;
	    continue;
	  case TRE_CHAR('$'):
	    if (!escaped && (i == n - 1))
	      fg->eol = true;
	    else
	      STORE_CHAR;
	    continue;
	  case TRE_CHAR('('):
	    if ((cflags & REG_EXTENDED) ^ escaped)
	      goto badpat;
	    else
	      STORE_CHAR;
	    continue;
	  case TRE_CHAR('{'):
	    if (!(cflags & REG_EXTENDED) ^ escaped)
	      STORE_CHAR;
	    else if (!(cflags & REG_EXTENDED) && (i == 0))
	      STORE_CHAR;
	    else if ((cflags & REG_EXTENDED) && (i == 0))
	      continue;
	    else
	      goto badpat;
	    continue;
	  case TRE_CHAR('|'):
	    if ((cflags & REG_EXTENDED) ^ escaped)
	      goto badpat;
	    else
	      STORE_CHAR;
	    continue;
	  default:
	    if (escaped)
	      goto badpat;
	    else
	      STORE_CHAR;
	    continue;
	}
      continue;
badpat:
      xfree(tmp);
      DPRINT(("tre_compile_fast: compilation of pattern failed, falling"
	      "back to NFA\n"));
      return REG_BADPAT;
    }

  fg->hasdot = wfirstdot > -1;

  /*
   * The pattern has been processed and copied to tmp as a literal string
   * with escapes, anchors (^$) and the word boundary match character
   * classes stripped out.
   */
#ifdef TRE_WCHAR
  SAVE_PATTERN(tmp, pos, fg->wpattern, fg->wlen);
  fg->wescmap = _escmap;
  STORE_MBS_PAT;

  /*
   * The position of dots and escaped dots is different in the MB string
   * than in to the wide string so traverse the converted string, as well,
   * to store these positions.
   */
  if (fg->hasdot || (fg->wescmap != NULL))
    {
      if (fg->wescmap != NULL)
	{
	  fg->escmap = xmalloc(fg->len * sizeof(bool));
	  if (!fg->escmap)
	    {
	      tre_free_fast(fg);
	      return REG_ESPACE;
	    }
	}

      escaped = false;
      for (unsigned int i = 0; i < fg->len; i++)
	if (fg->pattern[i] == '\\')
	  escaped = !escaped;
	else if (fg->pattern[i] == '.' && fg->escmap && escaped)
	  {
	    fg->escmap[i] = true;
	    escaped = false;
	  }
	else if (fg->pattern[i] == '.' && !escaped)
	  {
	    hasdot = i;
	    if (firstdot == -1)
	      firstdot = i;
	  }
	else
	  escaped = false;
    }
#else
  SAVE_PATTERN(tmp, pos, fg->pattern, fg->len);
  fg->escmap = _escmap;
#endif

  xfree(tmp);

  DPRINT(("tre_compile_fast: pattern: %s, len %zu, bol %c, eol %c, "
	 "icase: %c, word: %c, newline %c\n", fg->pattern, fg->len,
	 fg->bol ? 'y' : 'n', fg->eol ? 'y' : 'n',
	 fg->icase ? 'y' : 'n', fg->word ? 'y' : 'n',
	 fg->newline ? 'y' : 'n'));

  /* Check whether reverse QS algorithm is more efficient */
  if ((wfirstdot > -1) && (fg->wlen - whasdot + 1 < (size_t)wfirstdot) &&
      fg->nosub)
    {
      fg->reversed = true;
      DPRINT(("tre_compile_fast: using reverse QS algorithm\n"));
    }

  FILL_QSBC;
  FILL_BMGS;
#ifdef TRE_WCHAR
  FILL_QSBC_WIDE;
  FILL_BMGS_WIDE;
#endif

  return REG_OK;
}

#define _SHIFT_ONE							\
  {									\
    shift = 1;								\
    j = !fg->reversed ? j + shift : j - shift;				\
    continue;								\
  }

#define _BBOUND_COND							\
  ((type == STR_WIDE) ?							\
    ((j == 0) || !(tre_isalnum(str_wide[j - 1]) ||			\
      (str_wide[j - 1] == TRE_CHAR('_')))) :				\
    ((j == 0) || !(tre_isalnum(str_byte[j - 1]) ||			\
      (str_byte[j - 1] == '_'))))

#define _EBOUND_COND							\
  ((type == STR_WIDE) ?							\
    ((j + fg->wlen == len) || !(tre_isalnum(str_wide[j + fg->wlen]) ||	\
      (str_wide[j + fg->wlen] == TRE_CHAR('_')))) :			\
    ((j + fg->len == len) || !(tre_isalnum(str_byte[j + fg->len]) ||	\
      (str_byte[j + fg->len] == '_'))))

/*
 * Condition to check whether the match on position j is on a
 * word boundary.
 */
#define IS_ON_WORD_BOUNDARY						\
  (_BBOUND_COND && _EBOUND_COND)

/*
 * Checks word boundary and shifts one if match is not on a
 * boundary.
 */
#define CHECK_WORD_BOUNDARY						\
    if (!IS_ON_WORD_BOUNDARY)						\
      _SHIFT_ONE;

#define _BOL_COND							\
  ((j == 0) || ((type == STR_WIDE) ? (str_wide[j - 1] == TRE_CHAR('\n'))\
				   : (str_byte[j - 1] == '\n')))

/*
 * Checks BOL anchor and shifts one if match is not on a
 * boundary.
 */
#define CHECK_BOL_ANCHOR						\
    if (!_BOL_COND)							\
      _SHIFT_ONE;

#define _EOL_COND							\
  ((type == STR_WIDE)							\
    ? ((j + fg->wlen == len) ||						\
		(str_wide[j + fg->wlen] == TRE_CHAR('\n')))		\
    : ((j + fg->len == len) || (str_byte[j + fg->wlen] == '\n')))

/*
 * Checks EOL anchor and shifts one if match is not on a
 * boundary.
 */
#define CHECK_EOL_ANCHOR						\
    if (!_EOL_COND)							\
      _SHIFT_ONE;

/*
 * Executes matching of the precompiled pattern on the input string.
 * Returns REG_OK or REG_NOMATCH depending on if we find a match or not.
 */
int
tre_match_fast(const fastmatch_t *fg, const void *data, size_t len,
    tre_str_type_t type, int nmatch, regmatch_t pmatch[], int eflags)
{
  unsigned int shift, u = 0, v = 0;
  ssize_t j = 0;
  int ret = REG_NOMATCH;
  int mismatch;
  const char *str_byte = data;
  const void *startptr = NULL;
  const tre_char_t *str_wide = data;

  /* Calculate length if unspecified. */
  if (len == (size_t)-1)
    switch (type)
      {
	case STR_WIDE:
	  len = tre_strlen(str_wide);
	  break;
	default:
	  len = strlen(str_byte);
	  break;
      }

  /* Shortcut for empty pattern */
  if (fg->matchall)
    {
      if (!fg->nosub && nmatch >= 1)
	{
	  pmatch[0].rm_so = 0;
	  pmatch[0].rm_eo = len;
	}
      if (fg->bol && fg->eol)
	return (len == 0) ? REG_OK : REG_NOMATCH;
      else
	return REG_OK;
    }

  /* No point in going farther if we do not have enough data. */
  switch (type)
    {
      case STR_WIDE:
	if (len < fg->wlen)
	  return ret;
	shift = fg->wlen;
	break;
      default:
	if (len < fg->len)
	  return ret;
	shift = fg->len;
    }

  /*
   * REG_NOTBOL means not anchoring ^ to the beginning of the line, so we
   * can shift one because there can't be a match at the beginning.
   */
  if (fg->bol && (eflags & REG_NOTBOL))
    j = 1;

  /*
   * Like above, we cannot have a match at the very end when anchoring to
   * the end and REG_NOTEOL is specified.
   */
  if (fg->eol && (eflags & REG_NOTEOL))
    len--;

  if (fg->reversed)
    j = len - (type == STR_WIDE ? fg->wlen : fg->len);


  /* Only try once at the beginning or ending of the line. */
  if ((fg->bol || fg->eol) && !fg->newline && !(eflags & REG_NOTBOL) &&
      !(eflags & REG_NOTEOL))
    {
      /* Simple text comparison. */
      if (!((fg->bol && fg->eol) &&
	  (type == STR_WIDE ? (len != fg->wlen) : (len != fg->len))))
	{
	  /* Determine where in data to start search at. */
	  j = fg->eol ? len - (type == STR_WIDE ? fg->wlen : fg->len) : 0;
	  SKIP_CHARS(j);
	  mismatch = fastcmp(fg, startptr, type);
	  if (mismatch == REG_OK)
	    {
	      if (fg->word && !IS_ON_WORD_BOUNDARY)
		return ret;
	      if (!fg->nosub && nmatch >= 1)
		{
		  pmatch[0].rm_so = j;
		  pmatch[0].rm_eo = j + (type == STR_WIDE ? fg->wlen : fg->len);
		}
	      return REG_OK;
            }
        }
    }
  else
    {
      /* Quick Search / Turbo Boyer-Moore algorithm. */
      do
	{
	  SKIP_CHARS(j);
	  mismatch = fastcmp(fg, startptr, type);
	  if (mismatch == REG_OK)
	    {
	      if (fg->word)
		CHECK_WORD_BOUNDARY;
	      if (fg->bol)
		CHECK_BOL_ANCHOR;
	      if (fg->eol)
		CHECK_EOL_ANCHOR;
	      if (!fg->nosub && nmatch >= 1)
		{
		  pmatch[0].rm_so = j;
		  pmatch[0].rm_eo = j + ((type == STR_WIDE) ? fg->wlen : fg->len);
		}
	      return REG_OK;
	    }
	  else if (mismatch > 0)
	    return mismatch;
	  mismatch = -mismatch - 1;
	  SHIFT;
        } while (!IS_OUT_OF_BOUNDS);
    }
    return ret;
}

/*
 * Frees the resources that were allocated when the pattern was compiled.
 */
void
tre_free_fast(fastmatch_t *fg)
{

  DPRINT(("tre_fast_free: freeing structures for pattern %s\n",
	 fg->pattern));

#ifdef TRE_WCHAR
  hashtable_free(fg->qsBc_table);
  if (!fg->hasdot)
    xfree(fg->bmGs);
  if (fg->wescmap)
    xfree(fg->wescmap);
  xfree(fg->wpattern);
#endif
  if (!fg->hasdot)
    xfree(fg->sbmGs);
  if (fg->escmap)
    xfree(fg->escmap);
  xfree(fg->pattern);
}

/*
 * Returns:	-(i + 1) on failure (position that it failed with minus sign)
 *		error code on error
 *		REG_OK on success
 */
static inline int
fastcmp(const fastmatch_t *fg, const void *data, tre_str_type_t type)
{
  const char *str_byte = data;
  const char *pat_byte = fg->pattern;
  const tre_char_t *str_wide = data;
  const tre_char_t *pat_wide = fg->wpattern;
  const bool *escmap = (type == STR_WIDE) ? fg->wescmap : fg->escmap;
  size_t len = (type == STR_WIDE) ? fg->wlen : fg->len;
  int ret = REG_OK;

  /* Compare the pattern and the input char-by-char from the last position. */
  for (int i = len - 1; i >= 0; i--) {
    switch (type)
      {
	case STR_WIDE:

	  /* Check dot */
	  if (fg->hasdot && pat_wide[i] == TRE_CHAR('.') &&
	      (!escmap || !escmap[i]) &&
	      (!fg->newline || (str_wide[i] != TRE_CHAR('\n'))))
	    continue;

	  /* Compare */
	  if (fg->icase ? (towlower(pat_wide[i]) == towlower(str_wide[i]))
		    : (pat_wide[i] == str_wide[i]))
	    continue;
	  break;
	default:
	  /* Check dot */
	  if (fg->hasdot && pat_byte[i] == '.' &&
	      (!escmap || !escmap[i]) &&
	      (!fg->newline || (str_byte[i] != '\n')))
	    continue;

	  /* Compare */
	  if (fg->icase ? (tolower((unsigned char)pat_byte[i]) == tolower((unsigned char)str_byte[i]))
		    : (pat_byte[i] == str_byte[i]))
	  continue;
      }
    DPRINT(("fastcmp: mismatch at position %d\n", i));
    ret = -(i + 1);
    break;
  }
  return ret;
}
