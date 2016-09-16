/*
 * Copyright (c) 2008 ARM Ltd
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the company may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ARM LTD ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL ARM LTD BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "arm_asm.h"
#include <limits.h>
#include <stddef.h>

#if defined (__OPTIMIZE_SIZE__) || defined (PREFER_SIZE_OVER_SPEED) || \
  (defined (__thumb__) && !defined (__thumb2__))

size_t
strlen (const char* str)
{
  int scratch;
#if defined (__thumb__) && !defined (__thumb2__)
  size_t len;
  asm ("mov	%0, #0\n"
       "1:\n\t"
       "ldrb	%1, [%2, %0]\n\t"
       "add 	%0, %0, #1\n\t"
       "cmp	%1, #0\n\t"
       "bne	1b"
       : "=&r" (len), "=&r" (scratch) : "r" (str) : "memory", "cc");
  return len - 1;
#else
  const char* end;
  asm ("1:\n\t"
       "ldrb	%1, [%0], #1\n\t"
       "cmp	%1, #0\n\t"
       "bne	1b"
       : "=&r" (end), "=&r" (scratch) : "0" (str) : "memory", "cc");
  return end - str - 1;
#endif
}
#else

size_t __attribute__((naked))
strlen (const char* str)
{
  asm ("len .req r0\n\t"
       "data .req r3\n\t"
       "addr .req r1\n\t"

       "optpld r0\n\t"
       /* Word-align address */
       "bic	addr, r0, #3\n\t"
       /* Get adjustment for start ... */
       "ands	len, r0, #3\n\t"
       "neg	len, len\n\t"
       /* First word of data */
       "ldr	data, [addr], #4\n\t"
       /* Ensure bytes preceeding start ... */
       "add	ip, len, #4\n\t"
       "mov	ip, ip, asl #3\n\t"
       "mvn	r2, #0\n\t"
       /* ... are masked out */
#ifdef __thumb__
       "itt	ne\n\t"
# ifdef __ARMEB__
       "lslne	r2, ip\n\t"
# else
       "lsrne	r2, ip\n\t"
# endif
       "orrne	data, data, r2\n\t"
#else
       "it	ne\n\t"
# ifdef __ARMEB__
       "orrne	data, data, r2, lsl ip\n\t"
# else
       "orrne	data, data, r2, lsr ip\n\t"
# endif
#endif
       /* Magic const 0x01010101 */
#ifdef _ISA_ARM_7
       "movw	ip, #0x101\n\t"
#else
       "mov	ip, #0x1\n\t"
       "orr	ip, ip, ip, lsl #8\n\t"
#endif
       "orr	ip, ip, ip, lsl #16\n"

	/* This is the main loop.  We subtract one from each byte in
	   the word: the sign bit changes iff the byte was zero or
	   0x80 -- we eliminate the latter case by anding the result
	   with the 1-s complement of the data.  */
       "1:\n\t"
       /* test (data - 0x01010101)  */
       "sub	r2, data, ip\n\t"
       /* ... & ~data */
       "bic	r2, r2, data\n\t"
       /* ... & 0x80808080 == 0? */
       "ands	r2, r2, ip, lsl #7\n\t"
#ifdef _ISA_ARM_7
       /* yes, get more data... */
       "itt	eq\n\t"
       "ldreq	data, [addr], #4\n\t"
       /* and 4 more bytes  */
       "addeq	len, len, #4\n\t"
	/* If we have PLD, then unroll the loop a bit.  */
       "optpld addr, #8\n\t"
       /*  test (data - 0x01010101)  */
       "ittt	eq\n\t"
       "subeq	r2, data, ip\n\t"
       /* ... & ~data */
       "biceq	r2, r2, data\n\t"
       /* ... & 0x80808080 == 0? */
       "andeqs	r2, r2, ip, lsl #7\n\t"
#endif
       "itt	eq\n\t"
       /* yes, get more data... */
       "ldreq	data, [addr], #4\n\t"
       /* and 4 more bytes  */
       "addeq	len, len, #4\n\t"
       "beq	1b\n\t"
#ifdef __ARMEB__
       "tst	data, #0xff000000\n\t"
       "itttt	ne\n\t"
       "addne	len, len, #1\n\t"
       "tstne	data, #0xff0000\n\t"
       "addne	len, len, #1\n\t"
       "tstne	data, #0xff00\n\t"
       "it	ne\n\t"
       "addne	len, len, #1\n\t"
#else
# ifdef _ISA_ARM_5
	/* R2 is the residual sign bits from the above test.  All we
	need to do now is establish the position of the first zero
	byte... */
	/* Little-endian is harder, we need the number of trailing
	zeros / 8 */
#  ifdef _ISA_ARM_7
       "rbit	r2, r2\n\t"
       "clz	r2, r2\n\t"
#  else
       "rsb	r1, r2, #0\n\t"
       "and	r2, r2, r1\n\t"
       "clz	r2, r2\n\t"
       "rsb	r2, r2, #31\n\t"
#  endif
       "add	len, len, r2, lsr #3\n\t"
# else  /* No CLZ instruction */
       "tst	data, #0xff\n\t"
       "itttt	ne\n\t"
       "addne	len, len, #1\n\t"
       "tstne	data, #0xff00\n\t"
       "addne	len, len, #1\n\t"
       "tstne	data, #0xff0000\n\t"
       "it	ne\n\t"
       "addne	len, len, #1\n\t"
# endif
#endif
       "RETURN");
}
#endif
