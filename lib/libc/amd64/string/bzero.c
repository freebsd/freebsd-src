/*-
 * Public domain.
 */

#include <sys/cdefs.h>
#include <string.h>

void
bzero(void *b, size_t len)
{

	memset(b, 0, len);
}
