#include <string.h>
#include "xscale.h"

void *
memchr (const void *start, int c, size_t len)
{
  const char *str = start;

  if (len == 0)
    return 0;

  asm (PRELOADSTR ("%0") : : "r" (start));

  c &= 0xff;

#ifndef __OPTIMIZE_SIZE__
  /* Skip unaligned part.  */
  if ((long)str & 3)
    {
      str--;
      do
	{
	  if (*++str == c)
	    return (void *)str;
	}
      while (((long)str & 3) != 0 && --len > 0);
    }

  if (len > 3)
    {
      unsigned int c2 = c + (c << 8);
      c2 += c2 << 16;

      /* Load two constants:
         R7 = 0xfefefeff [ == ~(0x80808080 << 1) ]
         R6 = 0x80808080  */

      asm (
       "mov	r6, #0x80\n\
	add	r6, r6, #0x8000\n\
	add	r6, r6, r6, lsl #16\n\
	mvn	r7, r6, lsl #1\n\
\n\
0:\n\
	cmp	%1, #0x7\n\
	bls	1f\n\
\n\
	ldmia	%0!, { r3, r9 }\n\
"	PRELOADSTR ("%0") "\n\
	sub	%1, %1, #8\n\
	eor	r3, r3, %2\n\
	eor	r9, r9, %2\n\
	add	r2, r3, r7\n\
	add	r8, r9, r7\n\
	bic	r2, r2, r3\n\
	bic	r8, r8,	r9\n\
	and	r1, r2, r6\n\
	and	r9, r8, r6\n\
	orrs	r1, r1, r9\n\
	beq	0b\n\
\n\
	add	%1, %1, #8\n\
	sub	%0, %0, #8\n\
1:\n\
	cmp	%1, #0x3\n\
	bls	2f\n\
\n\
	ldr	r3, [%0], #4\n\
"	PRELOADSTR ("%0") "\n\
	sub	%1, %1, #4\n\
	eor	r3, r3, %2\n\
	add	r2, r3, r7\n\
	bic	r2, r2, r3\n\
	ands	r1, r2, r6\n\
	beq	1b\n\
\n\
	sub	%0, %0, #4\n\
	add	%1, %1, #4\n\
2:\n\
"
       : "=&r" (str), "=&r" (len)
       : "r" (c2), "0" (str), "1" (len)
       : "r1", "r2", "r3", "r6", "r7", "r8", "r9", "cc");
    }
#endif

  while (len-- > 0)
    { 
      if (*str == c)
        return (void *)str;
      str++;
    } 

  return 0;
}
