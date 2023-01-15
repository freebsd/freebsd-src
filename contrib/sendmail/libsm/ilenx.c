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
**  ILENX -- determine the e'x'ternal length of a string in 'i'internal format
**
**	Parameters:
**		str -- string [i]
**
**	Returns:
**		e'x'ternal length of a string in 'i'internal format
*/

int
ilenx(str)
	const char *str;
{
	char c;
	int idx;
	XLENDECL

	if (NULL == str)
		return -1;
	for (idx = 0; (c = str[idx]) != '\0'; idx++)
		XLEN(c);
	return xlen;
}
#endif /* _FFR_8BITENVADDR */
