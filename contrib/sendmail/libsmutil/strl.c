/*
 * Copyright (c) 1999 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#ifndef lint
static char id[] = "@(#)$Id: strl.c,v 8.5.14.1 2000/05/12 20:46:17 ca Exp $";
#endif /* ! lint */

#include <sendmail.h>

#if !HASSTRL
/*
**  strlcpy -- copy string obeying length and '\0' terminate it
**
**		terminates with '\0' if len > 0
**
**	Parameters:
**		dst -- "destination" string.
**		src -- "from" string.
**		len -- length of space available in "destination" string.
**
**	Returns:
**		total length of the string tried to create (=strlen(src))
**		if this is greater than len then an overflow would have
**		occurred.
*/

size_t
strlcpy(dst, src, len)
	register char *dst;
	register const char *src;
	size_t len;
{
	register size_t i;

	if (len-- <= 0)
		return strlen(src);
	for (i = 0; i < len && (dst[i] = src[i]) != 0; i++)
		continue;
	dst[i] = '\0';
	if (src[i] == '\0')
		return i;
	else
		return i + strlen(src + i);
}
/*
**  strlcat -- catenate strings obeying length and '\0' terminate it
**
**		strlcat will append at most len - strlen(dst) - 1 chars.
**		terminates with '\0' if len > 0
**
**	Parameters:
**		dst -- "destination" string.
**		src -- "from" string.
**		len -- max. length of "destination" string.
**
**	Returns:
**		total length of the string tried to create
**		(= initial length of dst + length of src)
**		if this is greater than len then an overflow would have
**		occurred.
*/

size_t
strlcat(dst, src, len)
	register char *dst;
	register const char *src;
	size_t len;
{
	register size_t i, j, o;

	o = strlen(dst);
	if (len < o + 1)
		return o + strlen(src);
	len -= o + 1;
	for (i = 0, j = o; i < len && (dst[j] = src[i]) != 0; i++, j++)
		continue;
	dst[j] = '\0';
	if (src[i] == '\0')
		return j;
	else
		return j + strlen(src + i);
}

#endif /* !HASSTRL */
