/*
 * emalloc - return new memory obtained from the system.  Belch if none.
 */
#include "ntp_types.h"
#include "ntp_malloc.h"
#include "ntp_stdlib.h"
#include "ntp_syslog.h"

char *
emalloc(size)
	u_int size;
{
	char *mem;

	if ((mem = (char *)malloc(size)) == 0) {
		syslog(LOG_ERR, "No more memory!");
		exit(1);
	}
	return mem;
}
