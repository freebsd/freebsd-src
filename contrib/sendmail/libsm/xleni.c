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
#include <sm/ixlen.h>

#if _FFR_8BITENVADDR
/*
**  XLENI -- determine the 'i'internal length of a string in e'x'ternal format
**
**	Parameters:
**		str -- string [x]
**
**	Returns:
**		'i'internal length of a string in e'x'ternal format
*/

int
xleni(str)
	const char *str;
{
	char c;
	int idx, ilen;

	if (NULL == str)
		return -1;
	for (ilen = 0, idx = 0; (c = str[idx]) != '\0'; ilen++, idx++)
	{
		if (SM_MM_QUOTE(c))
			ilen++;
	}

	return ilen;
}
#endif /* _FFR_8BITENVADDR */
