#include <config.h>

#include <string.h>
#include "ntp_malloc.h"

#ifndef HAVE_STRDUP

char *strdup(const char *s);

char *
strdup(
	const char *s
	)
{
	size_t	octets;
	char *	cp;

	if (s) {
		octets = 1 + strlen(s);
		cp = malloc(octets);
		if (NULL != cp)
			memcpy(cp, s, octets);
	else
		cp = NULL;

	return(cp);
}
#else
int strdup_c_nonempty_compilation_unit;
#endif
