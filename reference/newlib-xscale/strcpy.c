#include <string.h>
#include "xscale.h"
#undef strcpy

char *
strcpy (char *dest, const char *src)
{
  char *dest0 = dest;

  asm (PRELOADSTR ("%0") : : "r" (src));

#ifndef __OPTIMIZE_SIZE__
  if (((long)src & 3) == ((long)dest & 3))
    {
      /* Skip unaligned part.  */
      while ((long)src & 3)
	{
	  if (! (*dest++ = *src++))
	    return dest0;
	}

  /* Load two constants:
     R4 = 0xfefefeff [ == ~(0x80808080 << 1) ]
     R5 = 0x80808080  */

  asm ("mov	r5, #0x80\n\
	ldr	r1, [%1, #0]\n\
	add	r5, r5, #0x8000\n\
	add	r5, r5, r5, lsl #16\n\
	mvn	r4, r5, lsl #1\n\
\n\
	add	r3, r1, r5\n\
	bic	r3, r3, r1\n\
	ands	r2, r3, r4\n\
	bne	1f\n\
0:\n\
	ldr	r3, [%1, #0]\n\
	ldr	r1, [%1, #4]!\n\
"	PRELOADSTR("%1") "\n\
	str	r3, [%0], #4\n\
	add	r2, r1, r4\n\
	bic	r2, r2, r1\n\
	ands	r3, r2, r5\n\
	beq	0b\n\
1:"
       : "=&r" (dest), "=&r" (src)
       : "0" (dest), "1" (src)
       : "r1", "r2", "r3", "r4", "r5", "memory", "cc");
    }
#endif

  while (*dest++ = *src++)
    asm (PRELOADSTR ("%0") : : "r" (src));
  return dest0;
}
