/*
 * $Id: etc.c,v 1.8 1994/06/15 22:39:32 rich Exp $
 */

#include <err.h>
#include <stdlib.h>
#include <string.h>

/*
 * Like malloc but get fatal error if memory is exhausted.
 */
void *
xmalloc(size)
	size_t size;
{
	register void	*result = (void *)malloc(size);

	if (!result)
		errx(1, "virtual memory exhausted");

	return result;
}

/*
 * Like realloc but get fatal error if memory is exhausted.
 */
void *
xrealloc(ptr, size)
	void *ptr;
	size_t size;
{
	register void	*result;

	if (ptr == NULL)
		result = (void *)malloc(size);
	else
		result = (void *)realloc(ptr, size);

	if (!result)
		errx(1, "virtual memory exhausted");

	return result;
}

/*
 * Return a newly-allocated string whose contents concatenate
 * the strings S1, S2, S3.
 */
char *
concat(s1, s2, s3)
	const char *s1, *s2, *s3;
{
	register int	len1 = strlen(s1),
			len2 = strlen(s2),
			len3 = strlen(s3);

	register char *result = (char *)xmalloc(len1 + len2 + len3 + 1);

	strcpy(result, s1);
	strcpy(result + len1, s2);
	strcpy(result + len1 + len2, s3);
	result[len1 + len2 + len3] = 0;

	return result;
}

