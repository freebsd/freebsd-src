/*
 * emalloc - return new memory obtained from the system.  Belch if none.
 */
#include "ntp_types.h"
#include "ntp_malloc.h"
#include "ntp_stdlib.h"
#include "ntp_syslog.h"

#if defined SYS_WINNT && defined DEBUG
#include <crtdbg.h>
#endif

#if defined SYS_WINNT && defined DEBUG

void *
debug_emalloc(
	u_int size,
	char *filename,
	int line
	)
{
	char *mem;

	if ((mem = (char *)_malloc_dbg(size, _NORMAL_BLOCK, filename, line)) == 0) {
		msyslog(LOG_ERR, "No more memory!");
		exit(1);
	}
	return mem;
}

#else

void *
emalloc(
	u_int size
	)
{
	char *mem;

	if ((mem = (char *)malloc(size)) == 0) {
		msyslog(LOG_ERR, "No more memory!");
		exit(1);
	}
	return mem;
}


#endif
