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
/*
 * Copyright (c) 2013 ARM Ltd
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

    /* Prototype: void *memcpy (void *dst, const void *src, size_t count).  */

        // This version is tuned for the Cortex-A15 processor.

        .text
        .syntax unified
        .fpu    neon

#define CACHE_LINE_SIZE 64

	.globl memcpy
	.type memcpy,%function
memcpy:
	.fnstart
        // Assumes that n >= 0, and dst, src are valid pointers.
        // For any sizes less than 832 use the neon code that doesn't
        // care about the src alignment. This avoids any checks
        // for src alignment, and offers the best improvement since
        // smaller sized copies are dominated by the overhead of
        // the pre and post main loop.
        // For larger copies, if src and dst cannot both be aligned to
        // word boundaries, use the neon code.
        // For all other copies, align dst to a double word boundary
        // and copy using LDRD/STRD instructions.

        // Save registers (r0 holds the return value):
        // optimized push {r0, lr}.
        .save   {r0, lr}
        pld     [r1, #(CACHE_LINE_SIZE*16)]
        push    {r0, lr}

        cmp     r2, #16
        blo     copy_less_than_16_unknown_align

        cmp     r2, #832
        bge     check_alignment

copy_unknown_alignment:
        // Unknown alignment of src and dst.
        // Assumes that the first few bytes have already been prefetched.

        // Align destination to 128 bits. The mainloop store instructions
        // require this alignment or they will throw an exception.
        rsb         r3, r0, #0
        ands        r3, r3, #0xF
        beq         2f

        // Copy up to 15 bytes (count in r3).
        sub         r2, r2, r3
        movs        ip, r3, lsl #31

        itt         mi
        ldrbmi      lr, [r1], #1
        strbmi      lr, [r0], #1
        itttt       cs
        ldrbcs      ip, [r1], #1
        ldrbcs      lr, [r1], #1
        strbcs      ip, [r0], #1
        strbcs      lr, [r0], #1

        movs        ip, r3, lsl #29
        bge         1f
        // Copies 4 bytes, dst 32 bits aligned before, at least 64 bits after.
        vld4.8      {d0[0], d1[0], d2[0], d3[0]}, [r1]!
        vst4.8      {d0[0], d1[0], d2[0], d3[0]}, [r0, :32]!
1:      bcc         2f
        // Copies 8 bytes, dst 64 bits aligned before, at least 128 bits after.
        vld1.8      {d0}, [r1]!
        vst1.8      {d0}, [r0, :64]!

2:      // Make sure we have at least 64 bytes to copy.
        subs        r2, r2, #64
        blo         2f

1:      // The main loop copies 64 bytes at a time.
        vld1.8      {d0  - d3},   [r1]!
        vld1.8      {d4  - d7},   [r1]!
        pld         [r1, #(CACHE_LINE_SIZE*4)]
        subs        r2, r2, #64
        vst1.8      {d0  - d3},   [r0, :128]!
        vst1.8      {d4  - d7},   [r0, :128]!
        bhs         1b

2:      // Fix-up the remaining count and make sure we have >= 32 bytes left.
        adds        r2, r2, #32
        blo         3f

        // 32 bytes. These cache lines were already preloaded.
        vld1.8      {d0 - d3},  [r1]!
        sub         r2, r2, #32
        vst1.8      {d0 - d3},  [r0, :128]!
3:      // Less than 32 left.
        add         r2, r2, #32
        tst         r2, #0x10
        beq         copy_less_than_16_unknown_align
        // Copies 16 bytes, destination 128 bits aligned.
        vld1.8      {d0, d1}, [r1]!
        vst1.8      {d0, d1}, [r0, :128]!

copy_less_than_16_unknown_align:
        // Copy up to 15 bytes (count in r2).
        movs        ip, r2, lsl #29
        bcc         1f
        vld1.8      {d0}, [r1]!
        vst1.8      {d0}, [r0]!
1:      bge         2f
        vld4.8      {d0[0], d1[0], d2[0], d3[0]}, [r1]!
        vst4.8      {d0[0], d1[0], d2[0], d3[0]}, [r0]!

2:      // Copy 0 to 4 bytes.
        lsls        r2, r2, #31
        itt         ne
        ldrbne      lr, [r1], #1
        strbne      lr, [r0], #1
        itttt       cs
        ldrbcs      ip, [r1], #1
        ldrbcs      lr, [r1]
        strbcs      ip, [r0], #1
        strbcs      lr, [r0]

        pop         {r0, pc}

check_alignment:
        // If src and dst cannot both be aligned to a word boundary,
        // use the unaligned copy version.
        eor     r3, r0, r1
        ands    r3, r3, #0x3
        bne     copy_unknown_alignment

        // To try and improve performance, stack layout changed,
        // i.e., not keeping the stack looking like users expect
        // (highest numbered register at highest address).
        // TODO: Add debug frame directives.
        // We don't need exception unwind directives, because the code below
        // does not throw any exceptions and does not call any other functions.
        // Generally, newlib functions like this lack debug information for
        // assembler source.
        .save   {r4, r5}
        strd    r4, r5, [sp, #-8]!
        .save   {r6, r7}
        strd    r6, r7, [sp, #-8]!
        .save   {r8, r9}
        strd    r8, r9, [sp, #-8]!

        // Optimized for already aligned dst code.
        ands    ip, r0, #3
        bne     dst_not_word_aligned

word_aligned:
        // Align the destination buffer to 8 bytes, to make sure double
        // loads and stores don't cross a cache line boundary,
        // as they are then more expensive even if the data is in the cache
        // (require two load/store issue cycles instead of one).
        // If only one of the buffers is not 8 bytes aligned,
        // then it's more important to align dst than src,
        // because there is more penalty for stores
        // than loads that cross a cacheline boundary.
        // This check and realignment are only done if there is >= 832
        // bytes to copy.

        // Dst is word aligned, but check if it is already double word aligned.
        ands    r3, r0, #4
        beq     1f
        ldr     r3, [r1], #4
        str     r3, [r0], #4
        sub     r2, #4

1:      // Can only get here if > 64 bytes to copy, so don't do check r2.
        sub     r2, #64

2:      // Every loop iteration copies 64 bytes.
        .irp    offset, #0, #8, #16, #24, #32
        ldrd    r4, r5, [r1, \offset]
        strd    r4, r5, [r0, \offset]
        .endr

        ldrd    r4, r5, [r1, #40]
        ldrd    r6, r7, [r1, #48]
        ldrd    r8, r9, [r1, #56]

        // Keep the pld as far from the next load as possible.
        // The amount to prefetch was determined experimentally using
        // large sizes, and verifying the prefetch size does not affect
        // the smaller copies too much.
        // WARNING: If the ldrd and strd instructions get too far away
        //          from each other, performance suffers. Three loads
        //          in a row is the best tradeoff.
        pld     [r1, #(CACHE_LINE_SIZE*16)]
        strd    r4, r5, [r0, #40]
        strd    r6, r7, [r0, #48]
        strd    r8, r9, [r0, #56]

        add     r0, r0, #64
        add     r1, r1, #64
        subs    r2, r2, #64
        bge     2b

        // Fix-up the remaining count and make sure we have >= 32 bytes left.
        adds    r2, r2, #32
        blo     4f

        // Copy 32 bytes. These cache lines were already preloaded.
        .irp    offset, #0, #8, #16, #24
        ldrd    r4, r5, [r1, \offset]
        strd    r4, r5, [r0, \offset]
        .endr
        add     r1, r1, #32
        add     r0, r0, #32
        sub     r2, r2, #32
4:      // Less than 32 left.
        add     r2, r2, #32
        tst     r2, #0x10
        beq     5f
        // Copy 16 bytes.
        .irp    offset, #0, #8
        ldrd    r4, r5, [r1, \offset]
        strd    r4, r5, [r0, \offset]
        .endr
        add     r1, r1, #16
        add     r0, r0, #16

5:      // Copy up to 15 bytes (count in r2).
        movs    ip, r2, lsl #29
        bcc     1f
        // Copy 8 bytes.
        ldrd    r4, r5, [r1], #8
        strd    r4, r5, [r0], #8
1:      bge         2f
        // Copy 4 bytes.
        ldr     r4, [r1], #4
        str     r4, [r0], #4
2:      // Copy 0 to 4 bytes.
        lsls    r2, r2, #31
        itt     ne
        ldrbne  lr, [r1], #1
        strbne  lr, [r0], #1
        itttt   cs
        ldrbcs  ip, [r1], #1
        ldrbcs  lr, [r1]
        strbcs  ip, [r0], #1
        strbcs  lr, [r0]

        // Restore registers: optimized pop {r0, pc}
        ldrd    r8, r9, [sp], #8
        ldrd    r6, r7, [sp], #8
        ldrd    r4, r5, [sp], #8
        pop     {r0, pc}

dst_not_word_aligned:
        // Align dst to word.
        rsb     ip, ip, #4
        cmp     ip, #2

        itt     gt
        ldrbgt  lr, [r1], #1
        strbgt  lr, [r0], #1

        itt     ge
        ldrbge  lr, [r1], #1
        strbge  lr, [r0], #1

        ldrb    lr, [r1], #1
        strb    lr, [r0], #1

        sub     r2, r2, ip

        // Src is guaranteed to be at least word aligned by this point.
        b       word_aligned
	.fnend
	.size memcpy, .-memcpy
