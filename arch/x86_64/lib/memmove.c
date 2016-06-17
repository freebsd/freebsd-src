/* Normally compiler builtins are used, but sometimes the compiler calls out
   of line code. Based on asm-i386/string.h.
 */
#define _STRING_C
#include <linux/string.h>

#undef memmove
void *memmove(void * dest,const void *src,size_t count)
{
	if (dest < src) { 
		__inline_memcpy(dest,src,count);
	} else {
		/* Could be more clever and move longs */
		unsigned long d0, d1, d2;
		__asm__ __volatile__(
			"std\n\t"
			"rep\n\t"
			"movsb\n\t"
			"cld"
			: "=&c" (d0), "=&S" (d1), "=&D" (d2)
			:"0" (count),
			"1" (count-1+(const char *)src),
			"2" (count-1+(char *)dest)
			:"memory");
	}
	return dest;
} 
