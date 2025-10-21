/*-
 * Public domain.
 */

#include <string.h>

#undef bcopy	/* _FORTIFY_SOURCE */

void
bcopy(const void *src, void *dst, size_t len)
{

	memmove(dst, src, len);
}
