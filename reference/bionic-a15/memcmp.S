/*
 * Copyright (C) 2008 The Android Open Source Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef HAVE_32_BYTE_CACHE_LINE
#define CACHE_LINE_SIZE     32
#else
#define CACHE_LINE_SIZE     64
#endif

/*
 * Optimized memcmp() for Cortex-A9.
 */
	.text
	.globl memcmp
	.type memcmp,%function
memcmp:
	.fnstart
        pld         [r0, #(CACHE_LINE_SIZE * 0)]
        pld         [r0, #(CACHE_LINE_SIZE * 1)]

        /* take of the case where length is 0 or the buffers are the same */
        cmp         r0, r1
        moveq       r0, #0
        bxeq        lr

        pld         [r1, #(CACHE_LINE_SIZE * 0)]
        pld         [r1, #(CACHE_LINE_SIZE * 1)]

        /* make sure we have at least 8+4 bytes, this simplify things below
         * and avoid some overhead for small blocks
         */
        cmp        r2, #(8+4)
        bmi        10f
/*
 * Neon optimization
 * Comparing 32 bytes at a time
 */
#if defined(__ARM_NEON__) && defined(NEON_UNALIGNED_ACCESS)
        subs        r2, r2, #32
        blo         3f

        /* preload all the cache lines we need. */
        pld         [r0, #(CACHE_LINE_SIZE * 2)]
        pld         [r1, #(CACHE_LINE_SIZE * 2)]

1:      /* The main loop compares 32 bytes at a time */
        vld1.8      {d0 - d3}, [r0]!
        pld         [r0, #(CACHE_LINE_SIZE * 2)]
        vld1.8      {d4 - d7}, [r1]!
        pld         [r1, #(CACHE_LINE_SIZE * 2)]

        /* Start subtracting the values and merge results */
        vsub.i8     q0, q2
        vsub.i8     q1, q3
        vorr        q2, q0, q1
        vorr        d4, d5
        vmov        r3, ip, d4
        /* Check if there are any differences among the 32 bytes */
        orrs        r3, ip
        bne         2f
        subs        r2, r2, #32
        bhs         1b
        b           3f
2:
        /* Check if the difference was in the first or last 16 bytes */
        sub         r0, #32
        vorr        d0, d1
        sub         r1, #32
        vmov        r3, ip, d0
        orrs        r3, ip
        /* if the first 16 bytes are equal, we only have to rewind 16 bytes */
        ittt        eq
        subeq       r2, #16
        addeq       r0, #16
        addeq       r1, #16

3:      /* fix-up the remaining count */
        add         r2, r2, #32

        cmp        r2, #(8+4)
        bmi        10f
#endif

        .save {r4, lr}
        /* save registers */
        stmfd       sp!, {r4, lr}

        /* since r0 hold the result, move the first source
         * pointer somewhere else
         */
         mov        r4, r0

        /* align first pointer to word boundary
         * offset = -src & 3
         */
        rsb         r3, r4, #0
        ands        r3, r3, #3
        beq         0f

        /* align first pointer  */
        sub         r2, r2, r3
1:      ldrb        r0, [r4], #1
        ldrb        ip, [r1], #1
        subs        r0, r0, ip
        bne         9f
        subs        r3, r3, #1
        bne         1b


0:      /* here the first pointer is aligned, and we have at least 4 bytes
         * to process.
         */

        /* see if the pointers are congruent */
        eor         r0, r4, r1
        ands        r0, r0, #3
        bne         5f

        /* congruent case, 32 bytes per iteration
         * We need to make sure there are at least 32+4 bytes left
         * because we effectively read ahead one word, and we could
         * read past the buffer (and segfault) if we're not careful.
         */

        ldr         ip, [r1]
        subs        r2, r2, #(32 + 4)
        bmi         1f

0:      pld         [r4, #(CACHE_LINE_SIZE * 2)]
        pld         [r1, #(CACHE_LINE_SIZE * 2)]
        ldr         r0, [r4], #4
        ldr         lr, [r1, #4]!
        eors        r0, r0, ip
        ldreq       r0, [r4], #4
        ldreq       ip, [r1, #4]!
        eoreqs      r0, r0, lr
        ldreq       r0, [r4], #4
        ldreq       lr, [r1, #4]!
        eoreqs      r0, r0, ip
        ldreq       r0, [r4], #4
        ldreq       ip, [r1, #4]!
        eoreqs      r0, r0, lr
        ldreq       r0, [r4], #4
        ldreq       lr, [r1, #4]!
        eoreqs      r0, r0, ip
        ldreq       r0, [r4], #4
        ldreq       ip, [r1, #4]!
        eoreqs      r0, r0, lr
        ldreq       r0, [r4], #4
        ldreq       lr, [r1, #4]!
        eoreqs      r0, r0, ip
        ldreq       r0, [r4], #4
        ldreq       ip, [r1, #4]!
        eoreqs      r0, r0, lr
        bne         2f
        subs        r2, r2, #32
        bhs         0b

        /* do we have at least 4 bytes left? */
1:      adds        r2, r2, #(32 - 4 + 4)
        bmi         4f

        /* finish off 4 bytes at a time */
3:      ldr         r0, [r4], #4
        ldr         ip, [r1], #4
        eors        r0, r0, ip
        bne         2f
        subs        r2, r2, #4
        bhs         3b

        /* are we done? */
4:      adds        r2, r2, #4
        moveq       r0, #0
        beq         9f

        /* finish off the remaining bytes */
        b           8f

2:      /* the last 4 bytes are different, restart them */
        sub         r4, r4, #4
        sub         r1, r1, #4
        mov         r2, #4

        /* process the last few bytes */
8:      ldrb        r0, [r4], #1
        ldrb        ip, [r1], #1
        // stall
        subs        r0, r0, ip
        bne         9f
        subs        r2, r2, #1
        bne         8b

9:      /* restore registers and return */
        ldmfd       sp!, {r4, lr}
        bx          lr

10:     /* process less than 12 bytes */
        cmp         r2, #0
        moveq       r0, #0
        bxeq        lr
        mov         r3, r0
11:
        ldrb        r0, [r3], #1
        ldrb        ip, [r1], #1
        subs        r0, ip
        bxne        lr
        subs        r2, r2, #1
        bne         11b
        bx          lr

5:      /*************** non-congruent case ***************/
        and         r0, r1, #3
        cmp         r0, #2
        bne         4f

        /* here, offset is 2 (16-bits aligned, special cased) */

        /* make sure we have at least 16 bytes to process */
        subs        r2, r2, #16
        addmi       r2, r2, #16
        bmi         8b

        /* align the unaligned pointer */
        bic         r1, r1, #3
        ldr         lr, [r1], #4

6:      pld         [r1, #(CACHE_LINE_SIZE * 2)]
        pld         [r4, #(CACHE_LINE_SIZE * 2)]
        mov         ip, lr, lsr #16
        ldr         lr, [r1], #4
        ldr         r0, [r4], #4
        orr         ip, ip, lr, lsl #16
        eors        r0, r0, ip
        moveq       ip, lr, lsr #16
        ldreq       lr, [r1], #4
        ldreq       r0, [r4], #4
        orreq       ip, ip, lr, lsl #16
        eoreqs      r0, r0, ip
        moveq       ip, lr, lsr #16
        ldreq       lr, [r1], #4
        ldreq       r0, [r4], #4
        orreq       ip, ip, lr, lsl #16
        eoreqs      r0, r0, ip
        moveq       ip, lr, lsr #16
        ldreq       lr, [r1], #4
        ldreq       r0, [r4], #4
        orreq       ip, ip, lr, lsl #16
        eoreqs      r0, r0, ip
        bne         7f
        subs        r2, r2, #16
        bhs         6b
        sub         r1, r1, #2
        /* are we done? */
        adds        r2, r2, #16
        moveq       r0, #0
        beq         9b
        /* finish off the remaining bytes */
        b           8b

7:      /* fix up the 2 pointers and fallthrough... */
        sub         r1, r1, #(4+2)
        sub         r4, r4, #4
        mov         r2, #4
        b           8b


4:      /*************** offset is 1 or 3 (less optimized) ***************/

		stmfd		sp!, {r5, r6, r7}

        // r5 = rhs
        // r6 = lhs
        // r7 = scratch

        mov         r5, r0, lsl #3		/* r5 = right shift */
        rsb         r6, r5, #32         /* r6 = left shift */

        /* align the unaligned pointer */
        bic         r1, r1, #3
        ldr         r7, [r1], #4
        sub         r2, r2, #8

6:      mov         ip, r7, lsr r5
        ldr         r7, [r1], #4
        ldr         r0, [r4], #4
        orr         ip, ip, r7, lsl r6
        eors        r0, r0, ip
        moveq       ip, r7, lsr r5
        ldreq       r7, [r1], #4
        ldreq       r0, [r4], #4
        orreq       ip, ip, r7, lsl r6
        eoreqs      r0, r0, ip
        bne         7f
        subs        r2, r2, #8
        bhs         6b

        sub         r1, r1, r6, lsr #3
		ldmfd       sp!, {r5, r6, r7}

        /* are we done? */
        adds        r2, r2, #8
        moveq       r0, #0
        beq         9b

        /* finish off the remaining bytes */
        b           8b

7:      /* fix up the 2 pointers and fallthrough... */
        sub         r1, r1, #4
        sub         r1, r1, r6, lsr #3
        sub         r4, r4, #4
        mov         r2, #4
		ldmfd		sp!, {r5, r6, r7}
        b           8b
	.fnend
	.size memcmp, .-memcmp
