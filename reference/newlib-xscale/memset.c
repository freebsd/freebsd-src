#include <string.h>
#include "xscale.h"

void *
memset (void *dst, int c, size_t len)
{
  int dummy;

  asm volatile ("tst	%0, #0x3"
#ifndef __OPTIMIZE_SIZE__
"\n\
	beq	1f\n\
	b	2f\n\
0:\n\
	strb	%1, [%0], #1\n\
	tst	%0, #0x3\n\
	beq	1f\n\
2:\n\
	movs	r3, %2\n\
	sub	%2, %2, #1\n\
	bne	0b\n\
# At this point we know that %2 == len == -1 (since the SUB has already taken\n\
# place).  If we fall through to the 1: label (as the code used to do), the\n\
# CMP will detect this negative value and branch to the 2: label.  This will\n\
# test %2 again, but this time against 0.  The test will fail and the loop\n\
# at 2: will go on for (almost) ever.  Hence the explicit branch to the end\n\
# of the hand written assembly code.\n\
	b       4f\n\
1:\n\
	cmp	%2, #0x3\n\
	bls	2f\n\
	and	%1, %1, #0xff\n\
	orr	lr, %1, %1, asl #8\n\
	cmp	%2, #0xf\n\
	orr	lr, lr, lr, asl #16\n\
	bls	1f\n\
	mov	r3, lr\n\
	mov	r4, lr\n\
	mov	r5, lr\n\
0:\n\
	sub	%2, %2, #16\n\
	stmia	%0!, { r3, r4, r5, lr }\n\
	cmp	%2, #0xf\n\
	bhi	0b\n\
1:\n\
	cmp	%2, #0x7\n\
	bls	1f\n\
	mov	r3, lr\n\
0:\n\
	sub	%2, %2, #8\n\
	stmia	%0!, { r3, lr }\n\
	cmp	%2, #0x7\n\
	bhi	0b\n\
1:\n\
	cmp	%2, #0x3\n\
	bls	2f\n\
0:\n\
	sub	%2, %2, #4\n\
	str	lr, [%0], #4\n\
	cmp	%2, #0x3\n\
	bhi	0b\n\
"
#endif /* !__OPTIMIZE_SIZE__ */
"\n\
2:\n\
	movs	r3, %2\n\
	sub	%2, %2, #1\n\
	beq	4f\n\
0:\n\
	movs	r3, %2\n\
	sub	%2, %2, #1\n\
	strb	%1, [%0], #1\n\
	bne	0b\n\
4:"

       : "=&r" (dummy), "=&r" (c), "=&r" (len)
       : "0" (dst), "1" (c), "2" (len)
       : "memory", "r3", "r4", "r5", "lr");

  return dst;
}
