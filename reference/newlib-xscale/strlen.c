#include <string.h>
#include "xscale.h"
#define _CONST  const

size_t
strlen (const char *str)
{
  _CONST char *start = str;

  /* Skip unaligned part.  */
  if ((long)str & 3)
    {
      str--;
      do
	{
	  if (*++str == '\0')
	    goto out;
	}
      while ((long)str & 3);
    }

  /* Load two constants:
     R4 = 0xfefefeff [ == ~(0x80808080 << 1) ]
     R5 = 0x80808080  */

  asm ("mov	r5, #0x80\n\
	add	r5, r5, #0x8000\n\
	add	r5, r5, r5, lsl #16\n\
	mvn	r4, r5, lsl #1\n\
"

#if defined __ARM_ARCH_5__ || defined __ARM_ARCH_5T__ || defined __ARM_ARCH_5E__ || defined __ARM_ARCH_5TE__ || defined __ARM_ARCH_7A__

"	tst	%0, #0x7\n\
	itt	eq\n\
	ldreqd	r6, [%0]\n\
	beq	1f\n\
	ldr	r2, [%0]\n\
        add     r3, r2, r4\n\
        bic     r3, r3, r2\n\
        ands    r2, r3, r5\n\
	bne	2f\n\
	sub	%0, %0, #4\n\
\n\
0:\n\
	ldrd	r6, [%0, #8]!\n\
"
	PRELOADSTR ("%0")
"\n\
1:\n\
	add	r3, r6, r4\n\
	add	r2, r7, r4\n\
	bic	r3, r3, r6\n\
	bic	r2, r2, r7\n\
	and	r3, r3, r5\n\
	and	r2, r2, r5\n\
	orrs	r3, r2, r3\n\
	beq	0b\n\
"
#else

"	sub	%0, %0, #4\n\
\n\
0:\n\
	ldr	r6, [%0, #4]!\n\
"
	PRELOADSTR ("%0")
"\n\
	add	r3, r6, r4\n\
	bic	r3, r3, r6\n\
	ands	r3, r3, r5\n\
	beq	0b\n\
"
#endif /* __ARM_ARCH_5[T][E]__ */
"\n\
2:\n\
	ldrb	r3, [%0]\n\
	cmp	r3, #0x0\n\
	beq	1f\n\
\n\
0:\n\
	ldrb	r3, [%0, #1]!\n\
"
	PRELOADSTR ("%0")
"\n\
	cmp	r3, #0x0\n\
	bne	0b\n\
1:\n\
"
       : "=r" (str) : "0" (str) : "r2", "r3", "r4", "r5", "r6", "r7");

  out:
  return str - start;
}
