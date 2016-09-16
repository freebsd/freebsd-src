#include <string.h>
#include "xscale.h"
#undef strcmp

int
strcmp (const char *s1, const char *s2)
{
  asm (PRELOADSTR ("%0") : : "r" (s1));
  asm (PRELOADSTR ("%0") : : "r" (s2));

#ifndef __OPTIMIZE_SIZE__
  if (((long)s1 & 3) == ((long)s2 & 3))
    {
      int result;

      /* Skip unaligned part.  */
      while ((long)s1 & 3)
	{
	  if (*s1 == '\0' || *s1 != *s2)
	    goto out;
	  s1++;
	  s2++;
	}

  /* Load two constants:
     lr = 0xfefefeff [ == ~(0x80808080 << 1) ]
     ip = 0x80808080  */

      asm (
       "ldr	r2, [%1, #0]\n\
	ldr	r3, [%2, #0]\n\
	cmp	r2, r3\n\
	bne	2f\n\
\n\
	mov	ip, #0x80\n\
	add	ip, ip, #0x8000\n\
	add	ip, ip, ip, lsl #16\n\
	mvn	lr, ip, lsl #1\n\
\n\
0:\n\
	ldr	r2, [%1, #0]\n\
	add	r3, r2, lr\n\
	bic	r3, r3, r2\n\
	tst	r3, ip\n\
	beq	1f\n\
	mov	%0, #0x0\n\
	b	3f\n\
1:\n\
	ldr	r2, [%1, #4]!\n\
	ldr	r3, [%2, #4]!\n\
"	PRELOADSTR("%1") "\n\
"	PRELOADSTR("%2") "\n\
	cmp	r2, r3\n\
	beq	0b"

       /* The following part could be done in a C loop as well, but it needs
	  to be assembler to save some cycles in the case where the optimized
	  loop above finds the strings to be equal.  */
"\n\
2:\n\
	ldrb	r2, [%1, #0]\n\
"	PRELOADSTR("%1") "\n\
"	PRELOADSTR("%2") "\n\
	cmp	r2, #0x0\n\
	beq	1f\n\
	ldrb	r3, [%2, #0]\n\
	cmp	r2, r3\n\
	bne	1f\n\
0:\n\
	ldrb	r3, [%1, #1]!\n\
	add	%2, %2, #1\n\
	ands	ip, r3, #0xff\n\
	beq	1f\n\
	ldrb	r3, [%2]\n\
	cmp	ip, r3\n\
	beq	0b\n\
1:\n\
	ldrb	lr, [%1, #0]\n\
	ldrb	ip, [%2, #0]\n\
	rsb	%0, ip, lr\n\
3:\n\
"

       : "=r" (result), "=&r" (s1), "=&r" (s2)
       : "1" (s1), "2" (s2)
       : "lr", "ip", "r2", "r3", "cc");
      return result;
    }
#endif

  while (*s1 != '\0' && *s1 == *s2)
    {
      asm (PRELOADSTR("%0") : : "r" (s1));
      asm (PRELOADSTR("%0") : : "r" (s2));
      s1++;
      s2++;
    }
 out:
  return (*(unsigned char *) s1) - (*(unsigned char *) s2);
}
