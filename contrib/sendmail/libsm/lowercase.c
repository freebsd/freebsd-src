/*
 * Copyright (c) 2020 Proofpoint, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#include <sm/gen.h>
#include <sm/sendmail.h>

#include <ctype.h>
#include <sm/string.h>
#include <sm/heap.h>
#if USE_EAI
# include <sm/limits.h>
# include <unicode/ucasemap.h>
# include <unicode/ustring.h>
# include <unicode/uchar.h>
# include <sm/ixlen.h>

/*
**  ASCIISTR -- check whether a string is printable ASCII
**
**	Parameters:
**		str -- string
**
**	Returns:
**		TRUE iff printable ASCII
*/

bool
asciistr(str)
	const char *str;
{
	unsigned char ch;

	if  (str == NULL)
		return true;
	while ((ch = (unsigned char)*str) != '\0' && ch >= 32 && ch < 127)
		str++;
	return ch == '\0';
}

/*
**  ASCIINSTR -- check whether a string is printable ASCII up to len
**
**	Parameters:
**		str -- string
**		len -- length to check
**
**	Returns:
**		TRUE iff printable ASCII
*/

bool
asciinstr(str, len)
	const char *str;
	size_t len;
{
	unsigned char ch;
	int n;

	if (str == NULL)
		return true;
	SM_REQUIRE(len < INT_MAX);
	n = 0;
	while (n < len && (ch = (unsigned char)*str) != '\0'
	       && ch >= 32 && ch < 127)
	{
		n++;
		str++;
	}
	return n == len || ch == '\0';
}
#endif /* USE_EAI */

/*
**  MAKELOWER -- Translate a line into lower case
**
**	Parameters:
**		p -- string to translate (modified in place if possible). [A]
**
**	Returns:
**		lower cased string
**
**	Side Effects:
**		String p is translated to lower case if possible.
*/

char *
makelower(p)
	char *p;
{
	char c;
	char *orig;

	if (p == NULL)
		return p;
	orig = p;
#if USE_EAI
	if (!asciistr(p))
		return (char *)sm_lowercase(p);
#endif
	for (; (c = *p) != '\0'; p++)
		if (isascii(c) && isupper(c))
			*p = tolower(c);
	return orig;
}

#if USE_EAI
/*
**  SM_LOWERCASE -- lower case a UTF-8 string
**	Note: this should ONLY be applied to a UTF-8 string,
**	i.e., the caller should check first if it isn't an ASCII string.
**
**	Parameters:
**		str -- original string
**
**	Returns:
**		lower case version of string [S]
**
**	How to return an error description due to failed unicode calls?
**	However, is that even relevant?
*/

char *
sm_lowercase(str)
	const char *str;
{
	int olen, ilen;
	UErrorCode error;
	ssize_t req;
	int n;
	static UCaseMap *csm = NULL;
	static char *out = NULL;
	static int outlen = 0;

# if SM_CHECK_REQUIRE
	if (sm_debug_active(&SmExpensiveRequire, 3))
		SM_REQUIRE(!asciistr(str));
# endif
	/* an empty string is always ASCII */
	SM_REQUIRE(NULL != str && '\0' != *str);

	if (NULL == csm)
	{
		error = U_ZERO_ERROR;
		csm = ucasemap_open("en_US", U_FOLD_CASE_DEFAULT, &error);
		if (U_SUCCESS(error) == 0)
		{
			/* syserr("ucasemap_open error: %s", u_errorName(error)); */
			return NULL;
		}
	}

	ilen = strlen(str);
	olen = ilen + 1;
	if (olen > outlen)
	{
		outlen = olen;
		out = sm_realloc_x(out, outlen);
	}

	for (n = 0; n < 3; n++)
	{
		error = U_ZERO_ERROR;
		req = ucasemap_utf8FoldCase(csm, out, olen, str, ilen, &error);
		if (U_SUCCESS(error))
		{
			if (req >= olen)
			{
				outlen = req + 1;
				out = sm_realloc_x(out, outlen);
				out[req] = '\0';
			}
			break;
		}
		else if (error == U_BUFFER_OVERFLOW_ERROR)
		{
			outlen = req + 1;
			out = sm_realloc_x(out, outlen);
			olen = outlen;
		}
		else
		{
			/* syserr("conversion error for \"%s\": %s", str, u_errorName(error)); */
			return NULL;
		}
	}
	return out;
}
#endif /* USE_EAI */
