#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/lib/libc/gen/getprogname.c,v 1.4.34.1 2009/04/15 03:14:26 kensmith Exp $");

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
