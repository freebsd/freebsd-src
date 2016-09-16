#include <string.h>
#include "xscale.h"
#undef strchr

char *
strchr (const char *s, int c)
{
  unsigned int c2;
  asm (PRELOADSTR ("%0") : : "r" (s));

  c &= 0xff;

#ifndef __OPTIMIZE_SIZE__
  /* Skip unaligned part.  */
  if ((long)s & 3)
    {
      s--;
      do
	{
	  int c2 = *++s;
	  if (c2 == c)
	    return (char *)s;
	  if (c2 == '\0')
	    return 0;
	}
      while (((long)s & 3) != 0);
    }

  c2 = c + (c << 8);
  c2 += c2 << 16;

  /* Load two constants:
     R6 = 0xfefefeff [ == ~(0x80808080 << 1) ]
     R5 = 0x80808080  */

  asm (PRELOADSTR ("%0") "\n\
	mov	r5, #0x80\n\
	add	r5, r5, #0x8000\n\
	add	r5, r5, r5, lsl #16\n\
	mvn	r6, r5, lsl #1\n\
\n\
	sub	%0, %0, #4\n\
0:\n\
	ldr	r1, [%0, #4]!\n\
"	PRELOADSTR ("%0") "\n\
	add	r3, r1, r6\n\
	bic	r3, r3, r1\n\
	ands	r2, r3, r5\n\
	bne	1f\n\
	eor	r2, r1, %1\n\
	add	r3, r2, r6\n\
	bic	r3, r3, r2\n\
	ands	r1, r3, r5\n\
	beq	0b\n\
1:"
       : "=&r" (s)
       : "r" (c2), "0" (s)
       : "r1", "r2", "r3", "r5", "r6", "cc");
#endif

  while (*s && *s != c)
    s++;
  if (*s == c)
    return (char *)s;
  return NULL;
}
