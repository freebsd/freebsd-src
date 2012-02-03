/* $FreeBSD$ */

#ifndef GLUE_H
#define GLUE_H

#include <limits.h>
#undef RE_DUP_MAX
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>

#define TRE_WCHAR			1
#define TRE_MULTIBYTE			1
#define HAVE_MBSTATE_T			1

#define TRE_CHAR(n) L##n
#define CHF "%lc"

#define tre_char_t			wchar_t
#define tre_mbrtowc(pwc, s, n, ps)	(mbrtowc((pwc), (s), (n), (ps)))
#define tre_strlen			wcslen
#define tre_isspace			iswspace
#define tre_isalnum			iswalnum

#define REG_OK				0
#define REG_LITERAL			0020
#define REG_WORD			0100
#define REG_GNU				0400

#define TRE_MB_CUR_MAX			MB_CUR_MAX

#ifndef _GREP_DEBUG
#define DPRINT(msg)
#else			
#define DPRINT(msg) do {printf msg; fflush(stdout);} while(/*CONSTCOND*/0)
#endif

#define MIN(a,b)			((a > b) ? (b) : (a))
#define MAX(a,b)			((a > b) ? (a) : (b))

typedef enum { STR_WIDE, STR_BYTE, STR_MBS, STR_USER } tre_str_type_t;

#define CALL_WITH_OFFSET(fn)						\
  do									\
    {									\
      size_t slen = (size_t)(pmatch[0].rm_eo - pmatch[0].rm_so);	\
      size_t offset = pmatch[0].rm_so;					\
      int ret;								\
									\
      if ((long long)pmatch[0].rm_eo - pmatch[0].rm_so < 0)		\
	return REG_NOMATCH;						\
      ret = fn;								\
      for (unsigned i = 0; (!(eflags & REG_NOSUB) && (i < nmatch)); i++)\
	{								\
	  pmatch[i].rm_so += offset;					\
	  pmatch[i].rm_eo += offset;					\
	}								\
      return ret;							\
    } while (0 /*CONSTCOND*/)

int
tre_convert_pattern(const char *regex, size_t n, tre_char_t **w,
    size_t *wn);

void
tre_free_pattern(tre_char_t *wregex);
#endif
