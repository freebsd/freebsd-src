#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdlib.h>

extern const char *__progname;

const char *
getprogname(void)
{

	return (__progname);
}
