/* regsub.c */

/* This file contains the regsub() function, which performs substitutions
 * after a regexp match has been found.
 */

#include "config.h"
#include "ctype.h"
#include "vi.h"
#ifdef REGEX
# include <regex.h>
#else
# include "regexp.h"
#endif


/* perform substitutions after a regexp match */
#ifdef REGEX
void regsub(rm, startp, endp, src, dst)
	regmatch_t	*rm;	/* the regexp with pointers into matched text */
	char		*startp, *endp;
	REG char	*src;	/* the replacement string */
	REG char	*dst;	/* where to put the result of the subst */
#else
void regsub(re, src, dst)
	regexp		*re;	/* the regexp with pointers into matched text */
	REG char	*src;	/* the replacement string */
	REG char	*dst;	/* where to put the result of the subst */
#endif
{
	REG char	*cpy;	/* pointer to start of text to copy */
	REG char	*end;	/* pointer to end of text to copy */
	REG char	c;
	char		*start;
#ifndef CRUNCH
	int		mod = 0;/* used to track \U, \L, \u, \l, and \E */
	int		len;	/* used to calculate length of subst string */
	static char	*prev;	/* a copy of the text from the previous subst */

	/* replace \~ (or maybe ~) by previous substitution text */

	/* step 1: calculate the length of the new substitution text */
	for (len = strlen(src), c = '\0', cpy = src; *cpy; cpy++)
	{
# ifdef NO_MAGIC
		if (c == '\\' && *cpy == '~')
# else
		if (c == (*o_magic ? '\0' : '\\') && *cpy == '~')
# endif
		{
			if (!prev)
			{
				regerr("No prev text to substitute for ~");

				return;
			}
			len += strlen(prev) - 1;
# ifndef NO_MAGIC
			if (!*o_magic)
# endif
				len -= 1; /* because we lose the \ too */
		}

		/* watch backslash quoting */
		if (c != '\\' && *cpy == '\\')
			c = '\\';
		else
			c = '\0';
	}

	/* allocate memory for the ~ed version of src */
	checkmem();
	start = cpy = (char *)malloc((unsigned)(len + 1));
	if (!cpy)
	{
		regerr("Not enough memory for ~ expansion");
		return;
	}

	/* copy src into start, replacing the ~s by the previous text */
	while (*src)
	{
# ifndef NO_MAGIC
		if (*o_magic && *src == '~')
		{
			strcpy(cpy, prev);
			cpy += strlen(prev);
			src++;
		}
		else if (!*o_magic && *src == '\\' && *(src + 1) == '~')
# else /* NO_MAGIC */
		if (*src == '\\' && *(src + 1) == '~')
# endif /* NO_MAGIC */
		{
			strcpy(cpy, prev);
			cpy += strlen(prev);
			src += 2;
		}
		else
		{
			*cpy++ = *src++;
		}
	}
	*cpy = '\0';
#ifdef DEBUG
	if ((int)(cpy - start) != len)
	{
		msg("Bug in regsub.c! Predicted length = %d, Actual length = %d", len, (int)(cpy - start));
	}
#endif
	checkmem();

	/* remember this as the "previous" for next time */
	if (prev)
		_free_(prev);
	prev = src = start;

#endif /* undef CRUNCH */

	start = src;
	while ((c = *src++) != '\0')
	{
#ifndef NO_MAGIC
		/* recognize any meta characters */
		if (c == '&' && *o_magic)
		{
#ifdef REGEX
			cpy = startp;
			end = endp;
#else
			cpy = re->startp[0];
			end = re->endp[0];
#endif
		}
		else
#endif /* not NO_MAGIC */
		if (c == '\\')
		{
			c = *src++;
			switch (c)
			{
#ifndef NO_MAGIC
			  case '0':
			  case '1':
			  case '2':
			  case '3':
			  case '4':
			  case '5':
			  case '6':
			  case '7':
			  case '8':
			  case '9':
				/* \0 thru \9 mean "copy subexpression" */
				c -= '0';
#ifdef REGEX
				cpy = startp + (rm[c].rm_so - rm[0].rm_so);
				end = endp + (rm[c].rm_eo - rm[0].rm_eo);
#else
				cpy = re->startp[c];
				end = re->endp[c];
#endif
				break;
# ifndef CRUNCH
			  case 'U':
			  case 'u':
			  case 'L':
			  case 'l':
				/* \U and \L mean "convert to upper/lowercase" */
				mod = c;
				continue;

			  case 'E':
			  case 'e':
				/* \E ends the \U or \L */
				mod = 0;
				continue;
# endif /* not CRUNCH */
			  case '&':
				/* "\&" means "original text" */
				if (*o_magic)
				{
					*dst++ = c;
					continue;
				}
#ifdef REGEX
				cpy = startp;
				end = endp;
#else
				cpy = re->startp[0];
				end = re->endp[0];
#endif
				break;

#else /* NO_MAGIC */
			  case '&':
				/* "\&" means "original text" */
#ifdef REGEX
				cpy = startp;
				end = endp;
#else
				cpy = re->startp[0];
				end = re->endp[0];
#endif
				break;
#endif /* NO_MAGIC */
			  default:
				/* ordinary char preceded by backslash */
				*dst++ = c;
				continue;
			}
		}
#ifndef CRUNCH
# if OSK
		else if (c == '\l')
# else
		else if (c == '\r')
# endif
		{
			/* transliterate ^M into newline */
			*dst++ = '\n';
			continue;
		}
#endif /* !CRUNCH */
		else
		{
			/* ordinary character, so just copy it */
			*dst++ = c;
			continue;
		}

		/* Note: to reach this point in the code, we must have evaded
		 * all "continue" statements.  To do that, we must have hit
		 * a metacharacter that involves copying.
		 */

		/* if there is nothing to copy, loop */
		if (!cpy)
			continue;

		/* copy over a portion of the original */
		while (cpy < end)
		{
#ifndef NO_MAGIC
# ifndef CRUNCH
			switch (mod)
			{
			  case 'U':
			  case 'u':
				/* convert to uppercase */
				*dst++ = toupper(*cpy++);
				break;

			  case 'L':
			  case 'l':
				/* convert to lowercase */
				*dst++ = tolower(*cpy++);
				break;

			  default:
				/* copy without any conversion */
				*dst++ = *cpy++;
			}

			/* \u and \l end automatically after the first char */
			if (mod && (mod == 'u' || mod == 'l'))
			{
				mod = 0;
			}
# else /* CRUNCH */
			*dst++ = *cpy++;
# endif /* CRUNCH */
#else /* NO_MAGIC */
			*dst++ = *cpy++;
#endif /* NO_MAGIC */
		}
	}
	*dst = '\0';
}
