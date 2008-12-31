#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/gen/getprogname.c,v 1.4.32.1 2008/11/25 02:59:29 kensmith Exp $");

#include "namespace.h"
#include <stdlib.h>
#include "un-namespace.h"

#include "libc_private.h"

__weak_reference(_getprogname, getprogname);

const char *
_getprogname(void)
{

	return (__progname);
}
