/*	$File: localtime_r.c,v 1.4 2022/09/24 20:30:13 christos Exp $	*/

#include "file.h"
#ifndef	lint
FILE_RCSID("@(#)$File: localtime_r.c,v 1.4 2022/09/24 20:30:13 christos Exp $")
#endif	/* lint */
#include <time.h>
#include <string.h>

/* asctime_r is not thread-safe anyway */
struct tm *
localtime_r(const time_t *t, struct tm *tm)
{
	struct tm *tmp = localtime(t);
	if (tmp == NULL)
		return NULL;
	memcpy(tm, tmp, sizeof(*tm));
	return tmp;
}
