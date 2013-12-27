/* $FreeBSD$ */

/*-
 * Copyright (C) 2011 Gabor Kovesdan <gabor@FreeBSD.org>
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

#include <errno.h>
#include <fastmatch.h>
#include <regex.h>
#include <string.h>

#include "tre-fastmatch.h"
#include "xmalloc.h"

int
tre_fixncomp(fastmatch_t *preg, const char *regex, size_t n, int cflags)
{
  int ret;
  tre_char_t *wregex;
  size_t wlen;

  if (n != 0)
    {
      ret = tre_convert_pattern(regex, n, &wregex, &wlen);
      if (ret != REG_OK)
	return ret;
      else 
	ret = tre_compile_literal(preg, wregex, wlen, cflags);
      tre_free_pattern(wregex);
      return ret;
    }
  else
    return tre_compile_literal(preg, NULL, 0, cflags);
}

int
tre_fastncomp(fastmatch_t *preg, const char *regex, size_t n, int cflags)
{
  int ret;
  tre_char_t *wregex;
  size_t wlen;

  if (n != 0)
    {
      ret = tre_convert_pattern(regex, n, &wregex, &wlen);
      if (ret != REG_OK)
	return ret;
      else
	ret = (cflags & REG_LITERAL)
	      ? tre_compile_literal(preg, wregex, wlen, cflags)
	      : tre_compile_fast(preg, wregex, wlen, cflags);
      tre_free_pattern(wregex);
      return ret;
    }
  else
    return tre_compile_literal(preg, NULL, 0, cflags);
}


int
tre_fixcomp(fastmatch_t *preg, const char *regex, int cflags)
{
  return tre_fixncomp(preg, regex, regex ? strlen(regex) : 0, cflags);
}

int
tre_fastcomp(fastmatch_t *preg, const char *regex, int cflags)
{
  return tre_fastncomp(preg, regex, regex ? strlen(regex) : 0, cflags);
}

int
tre_fixwncomp(fastmatch_t *preg, const wchar_t *regex, size_t n, int cflags)
{
  return tre_compile_literal(preg, regex, n, cflags);
}

int
tre_fastwncomp(fastmatch_t *preg, const wchar_t *regex, size_t n, int cflags)
{
  return (cflags & REG_LITERAL) ?
    tre_compile_literal(preg, regex, n, cflags) :
    tre_compile_fast(preg, regex, n, cflags);
}

int
tre_fixwcomp(fastmatch_t *preg, const wchar_t *regex, int cflags)
{
  return tre_fixwncomp(preg, regex, regex ? tre_strlen(regex) : 0, cflags);
}

int
tre_fastwcomp(fastmatch_t *preg, const wchar_t *regex, int cflags)
{
  return tre_fastwncomp(preg, regex, regex ? tre_strlen(regex) : 0, cflags);
}

void
tre_fastfree(fastmatch_t *preg)
{
  tre_free_fast(preg);
}

int
tre_fastnexec(const fastmatch_t *preg, const char *string, size_t len,
         size_t nmatch, regmatch_t pmatch[], int eflags)
{
  tre_str_type_t type = (TRE_MB_CUR_MAX == 1) ? STR_BYTE : STR_MBS;

  if (eflags & REG_STARTEND)
    CALL_WITH_OFFSET(tre_match_fast(preg, &string[offset], slen,
		     type, nmatch, pmatch, eflags));
  else
    return tre_match_fast(preg, string, len, type, nmatch,
      pmatch, eflags);
}

int
tre_fastexec(const fastmatch_t *preg, const char *string, size_t nmatch,
	     regmatch_t pmatch[], int eflags)
{
  return tre_fastnexec(preg, string, (size_t)-1, nmatch, pmatch, eflags);
}

int
tre_fastwnexec(const fastmatch_t *preg, const wchar_t *string, size_t len,
          size_t nmatch, regmatch_t pmatch[], int eflags)
{
  tre_str_type_t type = STR_WIDE;

  if (eflags & REG_STARTEND)
    CALL_WITH_OFFSET(tre_match_fast(preg, &string[offset], slen,
		     type, nmatch, pmatch, eflags));
  else
    return tre_match_fast(preg, string, len, type, nmatch,
      pmatch, eflags);
}

int
tre_fastwexec(const fastmatch_t *preg, const wchar_t *string,
         size_t nmatch, regmatch_t pmatch[], int eflags)
{
  return tre_fastwnexec(preg, string, (size_t)-1, nmatch, pmatch, eflags);
}

