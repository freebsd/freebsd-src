/*
 * $Id: etc.c,v 1.7 1994/02/13 20:41:05 jkh Exp $
 */

#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <fcntl.h>
#include <ar.h>
#include <ranlib.h>
#include <a.out.h>
#include <stab.h>
#include <string.h>
#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#include "ld.h"

/*
 * Report a nonfatal error.
 */

void
#if __STDC__
error(char *fmt, ...)
#else
error(fmt, va_alist)
	char	*fmt;
	va_dcl
#endif
{
	va_list	ap;
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)fprintf(stderr, "%s: ", progname);
	(void)vfprintf(stderr, fmt, ap);
	(void)fprintf(stderr, "\n");
	va_end(ap);
}

void	(*fatal_cleanup_hook)__P((void));
/*
 * Report a fatal error.
 */

void
#if __STDC__
fatal(char *fmt, ...)
#else
fatal(fmt, va_alist)
	char	*fmt;
	va_dcl
#endif
{
	va_list	ap;
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)fprintf(stderr, "%s: ", progname);
	(void)vfprintf(stderr, fmt, ap);
	(void)fprintf(stderr, "\n");
	va_end(ap);

	if (fatal_cleanup_hook)
		(*fatal_cleanup_hook)();
	exit(1);
}


/*
 * Return a newly-allocated string whose contents concatenate
 * the strings S1, S2, S3.
 */

char *
concat(s1, s2, s3)
	const char *s1, *s2, *s3;
{
	register int	len1 = strlen (s1),
			len2 = strlen (s2),
			len3 = strlen (s3);

	register char *result = (char *) xmalloc (len1 + len2 + len3 + 1);

	strcpy (result, s1);
	strcpy (result + len1, s2);
	strcpy (result + len1 + len2, s3);
	result[len1 + len2 + len3] = 0;

	return result;
}

/* Parse the string ARG using scanf format FORMAT, and return the result.
   If it does not parse, report fatal error
   generating the error message using format string ERROR and ARG as arg.  */

int
parse(arg, format, error)
	char *arg, *format, *error;
{
	int x;
	if (1 != sscanf (arg, format, &x))
		fatal (error, arg);
	return x;
}

/* Like malloc but get fatal error if memory is exhausted.  */

void *
xmalloc(size)
	int size;
{
	register void	*result = (void *)malloc (size);

	if (!result)
		fatal ("virtual memory exhausted", 0);

	return result;
}

/* Like realloc but get fatal error if memory is exhausted.  */

void *
xrealloc(ptr, size)
	void *ptr;
	int size;
{
	register void	*result;

	if (ptr == NULL)
		result = (void *)malloc (size);
	else
		result = (void *)realloc (ptr, size);

	if (!result)
		fatal ("virtual memory exhausted", 0);

	return result;
}
