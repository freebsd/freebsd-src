#include <config.h>

#include <ntp_assert.h>
#include <string.h>
#include "ntp_malloc.h"
#include "l_stdlib.h"

#define STRDUP_EMPTY_UNIT

#ifndef HAVE_STRDUP
# undef STRDUP_EMPTY_UNIT
char *strdup(const char *s);
char *
strdup(
	const char *s
	)
{
	size_t	octets;
	char *	cp;

	REQUIRE(s);
	octets = strlen(s) + 1;
	if ((cp = malloc(octets)) == NULL)
		return NULL;
	memcpy(cp, s, octets);

	return cp;
}
#endif

#ifndef HAVE_MEMCHR
# undef STRDUP_EMPTY_UNIT
void *memchr(const void *s, int c, size_t n)
{
	const unsigned char *p = s;
	while (n && *p != c) {
		--n;
		++p;
	}
	return n ? (char*)p : NULL;
}
#endif

#ifndef HAVE_STRNLEN
# undef STRDUP_EMPTY_UNIT
size_t strnlen(const char *s, size_t n)
{
	const char *e = memchr(s, 0, n);
	return e ? (size_t)(e - s) : n;
}
#endif

#ifdef STRDUP_EMPTY_UNIT
int strdup_c_nonempty_compilation_unit;
#endif
