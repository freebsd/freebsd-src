#include <sys/cdefs.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

void	err __P((int, const char *, ...));
void	verr __P((int, const char *, va_list));
void	errx __P((int, const char *, ...));
void	verrx __P((int, const char *, va_list));
void	warn __P((const char *, ...));
void	vwarn __P((const char *, va_list));
void	warnx __P((const char *, ...));
void	vwarnx __P((const char *, va_list));
