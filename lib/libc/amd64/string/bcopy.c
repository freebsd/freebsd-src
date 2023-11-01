/*-
 * Public domain.
 */

#include <string.h>

void
bcopy(const void *src, void *dst, size_t len)
{

	memmove(dst, src, len);
}
