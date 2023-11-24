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

#if USE_EAI
#include <sm/string.h>
#include <sm/heap.h>
#include <sm/ixlen.h>

/*
**  SM_STRCASEEQ -- are two strings equal (case-insenstive)?
**
**	Parameters:
**		s1 -- string
**		s2 -- string
**
**	Returns:
**		true iff s1 == s2
*/

bool
sm_strcaseeq(s1, s2)
	const char *s1;
	const char *s2;
{
	char *l1, *l2;
	char *f1;
	bool same;

	if (asciistr(s1))
	{
		if (!asciistr(s2))
			return false;
		return (sm_strcasecmp(s1, s2) == 0);
	}
	if (asciistr(s2))
		return false;
	l1 = sm_lowercase(s1);
	if (l1 != s1)
	{
		f1 = sm_strdup_x(l1);
		l1 = f1;
	}
	else
		f1 = NULL;
	l2 = sm_lowercase(s2);

	while (*l1 == *l2 && '\0' != *l1)
		l1++, l2++;
	same = *l1 == *l2;

	SM_FREE(f1);
	return same;
}

/*
**  SM_STRNCASEEQ -- are two strings (up to a length) equal (case-insenstive)?
**
**	Parameters:
**		s1 -- string
**		s2 -- string
**		n -- maximum length to compare
**
**	Returns:
**		true iff s1 == s2 (for up to the first n char)
*/

bool
sm_strncaseeq(s1, s2, n)
	const char *s1;
	const char *s2;
	size_t n;
{
	char *l1, *l2;
	char *f1;
	bool same;

	if (0 == n)
		return true;
	if (asciistr(s1))
	{
		if (!asciistr(s2))
			return false;
		return (sm_strncasecmp(s1, s2, n) == 0);
	}
	if (asciistr(s2))
		return false;
	l1 = sm_lowercase(s1);
	if (l1 != s1)
	{
		f1 = sm_strdup_x(l1);
		l1 = f1;
	}
	else
		f1 = NULL;
	l2 = sm_lowercase(s2);

	while (*l1 == *l2 && '\0' != *l1 && n-- > 0)
		l1++, l2++;
	same = *l1 == *l2;

	SM_FREE(f1);
	return same;
}
#endif /* USE_EAI */
