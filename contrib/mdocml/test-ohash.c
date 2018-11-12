#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <ohash.h>

void *xmalloc(size_t sz, void *arg) { return calloc(1,sz); }
void *xcalloc(size_t nmemb, size_t sz, void *arg) { return calloc(nmemb,sz); }
void xfree(void *p, void *arg) { free(p); }

int
main(void)
{
	struct ohash h;
	struct ohash_info i;
	i.alloc = xmalloc;
	i.calloc = xcalloc;
	i.free = xfree;
	ohash_init(&h, 2, &i);
	ohash_delete(&h);
	return 0;
}
