#if defined(LIBC_RCS) && !defined(lint)
static const char rcsid[] =
  "$FreeBSD$";
#endif /* LIBC_RCS and not lint */

#include <string.h>

extern const char *__progname;

void
setprogname(const char *progname)
{
	char *p;

	p = strrchr(progname, '/');
	__progname = p ? p + 1 : progname;
}
