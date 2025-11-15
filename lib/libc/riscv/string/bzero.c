/*-
 * Public domain.
 */

#include <string.h>

#undef bzero	/* _FORTIFY_SOURCE */

void
bzero(void *b, size_t len)
{

	memset(b, 0, len);
}
