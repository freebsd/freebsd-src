/*      $FreeBSD$       */

#include "glue.h"

#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <regex.h>
#include <string.h>
#include <wchar.h>

int
tre_convert_pattern(const char *regex, size_t n, tre_char_t **w,
		    size_t *wn)
{
#if TRE_WCHAR
  tre_char_t *wregex;
  size_t wlen;

  wregex = malloc(sizeof(tre_char_t) * (n + 1));
  if (wregex == NULL)
    return REG_ESPACE;

  /* If the current locale uses the standard single byte encoding of
     characters, we don't do a multibyte string conversion.  If we did,
     many applications which use the default locale would break since
     the default "C" locale uses the 7-bit ASCII character set, and
     all characters with the eighth bit set would be considered invalid. */
#if TRE_MULTIBYTE
  if (TRE_MB_CUR_MAX == 1)
#endif /* TRE_MULTIBYTE */
    {
      unsigned int i;
      const unsigned char *str = (const unsigned char *)regex;
      tre_char_t *wstr = wregex;

      for (i = 0; i < n; i++)
	*(wstr++) = *(str++);
      wlen = n;
    }
#if TRE_MULTIBYTE
  else
    {
      int consumed;
      tre_char_t *wcptr = wregex;
#ifdef HAVE_MBSTATE_T
      mbstate_t state;
      memset(&state, '\0', sizeof(state));
#endif /* HAVE_MBSTATE_T */
      while (n > 0)
	{
	  consumed = tre_mbrtowc(wcptr, regex, n, &state);

	  switch (consumed)
	    {
	    case 0:
	      if (*regex == '\0')
		consumed = 1;
	      else
		{
		  free(wregex);
		  return REG_BADPAT;
		}
	      break;
	    case -1:
	      DPRINT(("mbrtowc: error %d: %s.\n", errno, strerror(errno)));
	      free(wregex);
	      return REG_BADPAT;
	    case -2:
	      /* The last character wasn't complete.  Let's not call it a
		 fatal error. */
	      consumed = n;
	      break;
	    }
	  regex += consumed;
	  n -= consumed;
	  wcptr++;
	}
      wlen = wcptr - wregex;
    }
#endif /* TRE_MULTIBYTE */
  wregex[wlen] = L'\0';
  *w = wregex;
  *wn = wlen;
  return REG_OK;
#else /* !TRE_WCHAR */
  {
    *w = (tre_char_t * const *)regex;
    *wn = n;
    return REG_OK;
  }
#endif /* !TRE_WCHAR */
}

void
tre_free_pattern(tre_char_t *wregex)
{
#if TRE_WCHAR
  free(wregex);
#endif
}
