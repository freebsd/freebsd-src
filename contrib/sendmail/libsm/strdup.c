/*
 * Copyright (c) 2000-2001 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#include <sm/gen.h>
SM_RCSID("@(#)$Id: strdup.c,v 1.13 2001/09/11 04:04:49 gshapiro Exp $")

#include <sm/heap.h>
#include <sm/string.h>

/*
**  SM_STRNDUP_X -- Duplicate a string of a given length
**
**	Allocates memory and copies source string (of given length) into it.
**
**	Parameters:
**		s -- string to copy.
**		n -- length to copy.
**
**	Returns:
**		copy of string, raises exception if out of memory.
**
**	Side Effects:
**		allocate memory for new string.
*/

char *
sm_strndup_x(s, n)
	const char *s;
	size_t n;
{
	char *d = sm_malloc_x(n + 1);

	(void) memcpy(d, s, n);
	d[n] = '\0';
	return d;
}

/*
**  SM_STRDUP -- Duplicate a string
**
**	Allocates memory and copies source string into it.
**
**	Parameters:
**		s -- string to copy.
**
**	Returns:
**		copy of string, NULL if out of memory.
**
**	Side Effects:
**		allocate memory for new string.
*/

char *
sm_strdup(s)
	char *s;
{
	size_t l;
	char *d;

	l = strlen(s) + 1;
	d = sm_malloc_tagged(l, "sm_strdup", 0, sm_heap_group());
	if (d != NULL)
		(void) sm_strlcpy(d, s, l);
	return d;
}
