#if defined(LIBC_RCS) && !defined(lint)
static const char rcsid[] =
  "$FreeBSD: src/lib/libc/gen/setprogname.c,v 1.1.2.3 2001/08/21 17:22:22 nectar Exp $";
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
