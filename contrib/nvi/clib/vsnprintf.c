#include "config.h"

#include <sys/types.h>

#include <stdio.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

/*
 * PUBLIC: #ifndef HAVE_VSNPRINTF
 * PUBLIC: int vsnprintf __P((char *, size_t, const char *, ...));
 * PUBLIC: #endif
 */
int
vsnprintf(str, n, fmt, ap)
	char *str;
	size_t n;
	const char *fmt;
	va_list ap;
{
#ifdef SPRINTF_RET_CHARPNT
	(void)vsprintf(str, fmt, ap);
	return (strlen(str));
#else
	return (vsprintf(str, fmt, ap));
#endif
}
