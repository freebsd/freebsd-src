#include "config.h"

#include <sys/types.h>

#include <stdio.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

/*
 * PUBLIC: #ifndef HAVE_SNPRINTF
 * PUBLIC: int snprintf __P((char *, size_t, const char *, ...));
 * PUBLIC: #endif
 */
int
#ifdef __STDC__
snprintf(char *str, size_t n, const char *fmt, ...)
#else
snprintf(str, n, fmt, va_alist)
	char *str;
	size_t n;
	const char *fmt;
	va_dcl
#endif
{
	va_list ap;
	int rval;
#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
#ifdef SPRINTF_RET_CHARPNT
	(void)vsprintf(str, fmt, ap);
	va_end(ap);
	return (strlen(str));
#else
	rval = vsprintf(str, fmt, ap);
	va_end(ap);
	return (rval);
#endif
}
