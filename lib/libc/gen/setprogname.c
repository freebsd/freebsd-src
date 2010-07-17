#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/gen/setprogname.c,v 1.8.36.1.4.1 2010/06/14 02:09:06 kensmith Exp $");

#include <stdlib.h>
#include <string.h>

#include "libc_private.h"

void
setprogname(const char *progname)
{
	const char *p;

	p = strrchr(progname, '/');
	if (p != NULL)
		__progname = p + 1;
	else
		__progname = progname;
}
