#ifndef _EXTERR_H_
#define _EXTERR_H_

#include <sys/types.h>

static inline int
uexterr_gettext(char *buf, size_t bufsz)
{
	if (bufsz > 0)
		buf[0] = '\0';
	return (0);
}

#endif
