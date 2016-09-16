/*
 * Copyright (c) 2012 ARM Ltd
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

#ifdef __ARMEB__
#define S2LOMEM lsl
#define S2LOMEMEQ lsleq
#define S2HIMEM lsr
#define MSB 0x000000ff
#define LSB 0xff000000
#define BYTE0_OFFSET 24
#define BYTE1_OFFSET 16
#define BYTE2_OFFSET 8
#define BYTE3_OFFSET 0
#else /* not  __ARMEB__ */
#define S2LOMEM lsr
#define S2LOMEMEQ lsreq
#define S2HIMEM lsl
#define BYTE0_OFFSET 0
#define BYTE1_OFFSET 8
#define BYTE2_OFFSET 16
#define BYTE3_OFFSET 24
#define MSB 0xff000000
#define LSB 0x000000ff
#endif /* not  __ARMEB__ */

.syntax         unified

#if defined (__thumb__)
        .thumb
        .thumb_func
#endif
        .global strcmp
        .type   strcmp, %function
strcmp:

#if (defined (__thumb__) && !defined (__thumb2__))
1:
        ldrb    r2, [r0]
        ldrb    r3, [r1]
        adds    r0, r0, #1
        adds    r1, r1, #1
        cmp     r2, #0
        beq     2f
        cmp     r2, r3
        beq     1b
2:
        subs    r0, r2, r3
        bx      lr
#elif (defined (__OPTIMIZE_SIZE__) || defined (PREFER_SIZE_OVER_SPEED))
1:
        ldrb    r2, [r0], #1
        ldrb    r3, [r1], #1
        cmp     r2, #1
        it      cs
        cmpcs   r2, r3
        beq     1b
        subs    r0, r2, r3
        RETURN


#elif (defined (_ISA_THUMB_2) || defined (_ISA_ARM_6))
	/* Use LDRD whenever possible.  */

/* The main thing to look out for when comparing large blocks is that
   the loads do not cross a page boundary when loading past the index
   of the byte with the first difference or the first string-terminator.

   For example, if the strings are identical and the string-terminator
   is at index k, byte by byte comparison will not load beyond address
   s1+k and s2+k; word by word comparison may load up to 3 bytes beyond
   k; double word - up to 7 bytes.  If the load of these bytes crosses
   a page boundary, it might cause a memory fault (if the page is not mapped)
   that would not have happened in byte by byte comparison.

   If an address is (double) word aligned, then a load of a (double) word
   from that address will not cross a page boundary.
   Therefore, the algorithm below considers word and double-word alignment
   of strings separately.  */

/* High-level description of the algorithm.

   * The fast path: if both strings are double-word aligned,
     use LDRD to load two words from each string in every loop iteration.
   * If the strings have the same offset from a word boundary,
     use LDRB to load and compare byte by byte until
     the first string is aligned to a word boundary (at most 3 bytes).
     This is optimized for quick return on short unaligned strings.
   * If the strings have the same offset from a double-word boundary,
     use LDRD to load two words from each string in every loop iteration, as in the fast path.
   * If the strings do not have the same offset from a double-word boundary,
     load a word from the second string before the loop to initialize the queue.
     Use LDRD to load two words from every string in every loop iteration.
     Inside the loop, load the second word from the second string only after comparing
     the first word, using the queued value, to guarantee safety across page boundaries.
   * If the strings do not have the same offset from a word boundary,
     use LDR and a shift queue. Order of loads and comparisons matters,
     similarly to the previous case.

   * Use UADD8 and SEL to compare words, and use REV and CLZ to compute the return value.
   * The only difference between ARM and Thumb modes is the use of CBZ instruction.
   * The only difference between big and little endian is the use of REV in little endian
     to compute the return value, instead of MOV.
   * No preload. [TODO.]
*/

        .macro m_cbz reg label
#ifdef __thumb2__
        cbz     \reg, \label
#else   /* not defined __thumb2__ */
        cmp     \reg, #0
        beq     \label
#endif /* not defined __thumb2__ */
        .endm /* m_cbz */

        .macro m_cbnz reg label
#ifdef __thumb2__
        cbnz    \reg, \label
#else   /* not defined __thumb2__ */
        cmp     \reg, #0
        bne     \label
#endif /* not defined __thumb2__ */
        .endm /* m_cbnz */

        .macro  init
        /* Macro to save temporary registers and prepare magic values.  */
        subs    sp, sp, #16
        strd    r4, r5, [sp, #8]
        strd    r6, r7, [sp]
        mvn     r6, #0  /* all F */
        mov     r7, #0  /* all 0 */
        .endm   /* init */

        .macro  magic_compare_and_branch w1 w2 label
        /* Macro to compare registers w1 and w2 and conditionally branch to label.  */
        cmp     \w1, \w2        /* Are w1 and w2 the same?  */
        magic_find_zero_bytes \w1
        it      eq
        cmpeq   ip, #0          /* Is there a zero byte in w1?  */
        bne     \label
        .endm /* magic_compare_and_branch */

        .macro  magic_find_zero_bytes w1
        /* Macro to find all-zero bytes in w1, result is in ip.  */
#if (defined (__ARM_FEATURE_DSP))
        uadd8   ip, \w1, r6
        sel     ip, r7, r6
#else /* not defined (__ARM_FEATURE_DSP) */
        /* __ARM_FEATURE_DSP is not defined for some Cortex-M processors.
        Coincidently, these processors only have Thumb-2 mode, where we can use the
        the (large) magic constant available directly as an immediate in instructions.
        Note that we cannot use the magic constant in ARM mode, where we need
        to create the constant in a register.  */
        sub     ip, \w1, #0x01010101
        bic     ip, ip, \w1
        and     ip, ip, #0x80808080
#endif /* not defined (__ARM_FEATURE_DSP) */
        .endm /* magic_find_zero_bytes */

        .macro  setup_return w1 w2
#ifdef __ARMEB__
        mov     r1, \w1
        mov     r2, \w2
#else /* not  __ARMEB__ */
        rev     r1, \w1
        rev     r2, \w2
#endif /* not  __ARMEB__ */
        .endm /* setup_return */

        /*
        optpld r0, #0
        optpld r1, #0
        */

        /* Are both strings double-word aligned?  */
        orr     ip, r0, r1
        tst     ip, #7
        bne     do_align

        /* Fast path.  */
        init

doubleword_aligned:

        /* Get here when the strings to compare are double-word aligned.  */
        /* Compare two words in every iteration.  */
        .p2align        2
2:
        /*
        optpld r0, #16
        optpld r1, #16
        */

        /* Load the next double-word from each string.  */
        ldrd    r2, r3, [r0], #8
        ldrd    r4, r5, [r1], #8

        magic_compare_and_branch w1=r2, w2=r4, label=return_24
        magic_compare_and_branch w1=r3, w2=r5, label=return_35
        b       2b

do_align:
        /* Is the first string word-aligned?  */
        ands    ip, r0, #3
        beq     word_aligned_r0

        /* Fast compare byte by byte until the first string is word-aligned.  */
        /* The offset of r0 from a word boundary is in ip. Thus, the number of bytes
        to read until the next word boudnary is 4-ip.  */
        bic     r0, r0, #3
        ldr     r2, [r0], #4
        lsls    ip, ip, #31
        beq     byte2
        bcs     byte3

byte1:
        ldrb    ip, [r1], #1
        uxtb    r3, r2, ror #BYTE1_OFFSET
        subs    ip, r3, ip
        bne     fast_return
        m_cbz   reg=r3, label=fast_return

byte2:
        ldrb    ip, [r1], #1
        uxtb    r3, r2, ror #BYTE2_OFFSET
        subs    ip, r3, ip
        bne     fast_return
        m_cbz   reg=r3, label=fast_return

byte3:
        ldrb    ip, [r1], #1
        uxtb    r3, r2, ror #BYTE3_OFFSET
        subs    ip, r3, ip
        bne     fast_return
        m_cbnz  reg=r3, label=word_aligned_r0

fast_return:
        mov     r0, ip
        bx      lr

word_aligned_r0:
        init
        /* The first string is word-aligned.  */
        /* Is the second string word-aligned?  */
        ands    ip, r1, #3
        bne     strcmp_unaligned

word_aligned:
        /* The strings are word-aligned. */
        /* Is the first string double-word aligned?  */
        tst     r0, #4
        beq     doubleword_aligned_r0

        /* If r0 is not double-word aligned yet, align it by loading
        and comparing the next word from each string.  */
        ldr     r2, [r0], #4
        ldr     r4, [r1], #4
        magic_compare_and_branch w1=r2 w2=r4 label=return_24

doubleword_aligned_r0:
        /* Get here when r0 is double-word aligned.  */
        /* Is r1 doubleword_aligned?  */
        tst     r1, #4
        beq     doubleword_aligned

        /* Get here when the strings to compare are word-aligned,
        r0 is double-word aligned, but r1 is not double-word aligned.  */

        /* Initialize the queue.  */
        ldr     r5, [r1], #4

        /* Compare two words in every iteration.  */
        .p2align        2
3:
        /*
        optpld r0, #16
        optpld r1, #16
        */

        /* Load the next double-word from each string and compare.  */
        ldrd    r2, r3, [r0], #8
        magic_compare_and_branch w1=r2 w2=r5 label=return_25
        ldrd    r4, r5, [r1], #8
        magic_compare_and_branch w1=r3 w2=r4 label=return_34
        b       3b

        .macro miscmp_word offsetlo offsethi
        /* Macro to compare misaligned strings.  */
        /* r0, r1 are word-aligned, and at least one of the strings
        is not double-word aligned.  */
        /* Compare one word in every loop iteration.  */
        /* OFFSETLO is the original bit-offset of r1 from a word-boundary,
        OFFSETHI is 32 - OFFSETLO (i.e., offset from the next word).  */

        /* Initialize the shift queue.  */
        ldr     r5, [r1], #4

        /* Compare one word from each string in every loop iteration.  */
        .p2align        2
7:
        ldr     r3, [r0], #4
        S2LOMEM r5, r5, #\offsetlo
        magic_find_zero_bytes w1=r3
        cmp     r7, ip, S2HIMEM #\offsetlo
        and     r2, r3, r6, S2LOMEM #\offsetlo
        it      eq
        cmpeq   r2, r5
        bne     return_25
        ldr     r5, [r1], #4
        cmp     ip, #0
        eor	r3, r2, r3
        S2HIMEM r2, r5, #\offsethi
        it      eq
        cmpeq   r3, r2
        bne     return_32
        b       7b
        .endm /* miscmp_word */

strcmp_unaligned:
        /* r0 is word-aligned, r1 is at offset ip from a word.  */
        /* Align r1 to the (previous) word-boundary.  */
        bic     r1, r1, #3

        /* Unaligned comparison word by word using LDRs. */
        cmp     ip, #2
        beq     miscmp_word_16                    /* If ip == 2.  */
        bge     miscmp_word_24                    /* If ip == 3.  */
        miscmp_word offsetlo=8 offsethi=24        /* If ip == 1.  */
miscmp_word_16:  miscmp_word offsetlo=16 offsethi=16
miscmp_word_24:  miscmp_word offsetlo=24 offsethi=8


return_32:
        setup_return w1=r3, w2=r2
        b       do_return
return_34:
        setup_return w1=r3, w2=r4
        b       do_return
return_25:
        setup_return w1=r2, w2=r5
        b       do_return
return_35:
        setup_return w1=r3, w2=r5
        b       do_return
return_24:
        setup_return w1=r2, w2=r4

do_return:

#ifdef __ARMEB__
        mov     r0, ip
#else /* not  __ARMEB__ */
        rev     r0, ip
#endif /* not  __ARMEB__ */

        /* Restore temporaries early, before computing the return value.  */
        ldrd    r6, r7, [sp]
        ldrd    r4, r5, [sp, #8]
        adds    sp, sp, #16

        /* There is a zero or a different byte between r1 and r2.  */
        /* r0 contains a mask of all-zero bytes in r1.  */
        /* Using r0 and not ip here because cbz requires low register.  */
        m_cbz   reg=r0, label=compute_return_value
        clz     r0, r0
        /* r0 contains the number of bits on the left of the first all-zero byte in r1.  */
        rsb     r0, r0, #24
        /* Here, r0 contains the number of bits on the right of the first all-zero byte in r1.  */
        lsr     r1, r1, r0
        lsr     r2, r2, r0

compute_return_value:
        subs    r0, r1, r2
        bx      lr


#else   /* !(defined (_ISA_THUMB_2) || defined (_ISA_ARM_6)
             defined (__OPTIMIZE_SIZE__) || defined (PREFER_SIZE_OVER_SPEED) ||
             (defined (__thumb__) && !defined (__thumb2__))) */

        /* Use LDR whenever possible. */

#ifdef __thumb2__
#define magic1(REG) 0x01010101
#define magic2(REG) 0x80808080
#else
#define magic1(REG) REG
#define magic2(REG) REG, lsl #7
#endif

        optpld  r0
        optpld  r1
        eor     r2, r0, r1
        tst     r2, #3
        /* Strings not at same byte offset from a word boundary.  */
        bne     strcmp_unaligned
        ands    r2, r0, #3
        bic     r0, r0, #3
        bic     r1, r1, #3
        ldr     ip, [r0], #4
        it      eq
        ldreq   r3, [r1], #4
        beq     1f
        /* Although s1 and s2 have identical initial alignment, they are
        not currently word aligned.  Rather than comparing bytes,
	make sure that any bytes fetched from before the addressed
	bytes are forced to 0xff.  Then they will always compare
	equal.  */
        eor     r2, r2, #3
        lsl     r2, r2, #3
        mvn     r3, MSB
        S2LOMEM        r2, r3, r2
        ldr     r3, [r1], #4
        orr     ip, ip, r2
        orr     r3, r3, r2
1:
#ifndef __thumb2__
              /* Load the 'magic' constant 0x01010101.  */
        str     r4, [sp, #-4]!
        mov     r4, #1
        orr     r4, r4, r4, lsl #8
        orr     r4, r4, r4, lsl #16
#endif
        .p2align        2
4:
        optpld  r0, #8
        optpld  r1, #8
        sub     r2, ip, magic1(r4)
        cmp     ip, r3
        itttt   eq
        /* check for any zero bytes in first word */
        biceq   r2, r2, ip
        tsteq   r2, magic2(r4)
        ldreq   ip, [r0], #4
        ldreq   r3, [r1], #4
        beq     4b
2:
        /* There's a zero or a different byte in the word */
        S2HIMEM  r0, ip, #24
        S2LOMEM  ip, ip, #8
        cmp     r0, #1
        it      cs
        cmpcs   r0, r3, S2HIMEM #24
        it      eq
        S2LOMEMEQ r3, r3, #8
        beq     2b
        /* On a big-endian machine, r0 contains the desired byte in bits
        0-7; on a little-endian machine they are in bits 24-31.  In
        both cases the other bits in r0 are all zero.  For r3 the
        interesting byte is at the other end of the word, but the
        other bits are not necessarily zero.  We need a signed result
        representing the differnece in the unsigned bytes, so for the
        little-endian case we can't just shift the interesting bits
        up.  */
#ifdef __ARMEB__
        sub     r0, r0, r3, lsr #24
#else
        and     r3, r3, #255
#ifdef __thumb2__
        /* No RSB instruction in Thumb2 */
        lsr     r0, r0, #24
        sub     r0, r0, r3
#else
        rsb     r0, r3, r0, lsr #24
#endif
#endif
#ifndef __thumb2__
        ldr     r4, [sp], #4
#endif
        RETURN


strcmp_unaligned:

#if 0
        /* The assembly code below is based on the following alogrithm.  */
#ifdef __ARMEB__
#define RSHIFT <<
#define LSHIFT >>
#else
#define RSHIFT >>
#define LSHIFT <<
#endif

#define body(shift)							\
  mask = 0xffffffffU RSHIFT shift;					\
  w1 = *wp1++;								\
  w2 = *wp2++;								\
  do									\
    {									\
      t1 = w1 & mask;							\
      if (__builtin_expect(t1 != w2 RSHIFT shift, 0))			\
	{								\
	  w2 RSHIFT= shift;						\
	  break;							\
	}								\
      if (__builtin_expect(((w1 - b1) & ~w1) & (b1 << 7), 0))		\
	{								\
	  /* See comment in assembler below re syndrome on big-endian */\
	  if ((((w1 - b1) & ~w1) & (b1 << 7)) & mask)			\
	    w2 RSHIFT= shift;						\
	  else								\
	    {								\
	      w2 = *wp2;						\
	      t1 = w1 RSHIFT (32 - shift);				\
	      w2 = (w2 LSHIFT (32 - shift)) RSHIFT (32 - shift);	\
	    }								\
	  break;							\
	}								\
      w2 = *wp2++;							\
      t1 ^= w1;								\
      if (__builtin_expect(t1 != w2 LSHIFT (32 - shift), 0))		\
	{								\
	  t1 = w1 >> (32 - shift);					\
	  w2 = (w2 << (32 - shift)) RSHIFT (32 - shift);		\
	  break;							\
	}								\
      w1 = *wp1++;							\
    } while (1)

  const unsigned* wp1;
  const unsigned* wp2;
  unsigned w1, w2;
  unsigned mask;
  unsigned shift;
  unsigned b1 = 0x01010101;
  char c1, c2;
  unsigned t1;

  while (((unsigned) s1) & 3)
    {
      c1 = *s1++;
      c2 = *s2++;
      if (c1 == 0 || c1 != c2)
	return c1 - (int)c2;
    }
  wp1 = (unsigned*) (((unsigned)s1) & ~3);
  wp2 = (unsigned*) (((unsigned)s2) & ~3);
  t1 = ((unsigned) s2) & 3;
  if (t1 == 1)
    {
      body(8);
    }
  else if (t1 == 2)
    {
      body(16);
    }
  else
    {
      body (24);
    }

  do
    {
#ifdef __ARMEB__
      c1 = (char) t1 >> 24;
      c2 = (char) w2 >> 24;
#else /* not  __ARMEB__ */
      c1 = (char) t1;
      c2 = (char) w2;
#endif /* not  __ARMEB__ */
      t1 RSHIFT= 8;
      w2 RSHIFT= 8;
    } while (c1 != 0 && c1 == c2);
  return c1 - c2;
#endif /* 0 */


        wp1 .req r0
        wp2 .req r1
        b1  .req r2
        w1  .req r4
        w2  .req r5
        t1  .req ip
        @ r3 is scratch

        /* First of all, compare bytes until wp1(sp1) is word-aligned. */
1:
        tst     wp1, #3
        beq     2f
        ldrb    r2, [wp1], #1
        ldrb    r3, [wp2], #1
        cmp     r2, #1
        it      cs
        cmpcs   r2, r3
        beq     1b
        sub     r0, r2, r3
        RETURN

2:
        str     r5, [sp, #-4]!
        str     r4, [sp, #-4]!
        //stmfd   sp!, {r4, r5}
        mov     b1, #1
        orr     b1, b1, b1, lsl #8
        orr     b1, b1, b1, lsl #16

        and     t1, wp2, #3
        bic     wp2, wp2, #3
        ldr     w1, [wp1], #4
        ldr     w2, [wp2], #4
        cmp     t1, #2
        beq     2f
        bhi     3f

        /* Critical inner Loop: Block with 3 bytes initial overlap */
        .p2align        2
1:
        bic     t1, w1, MSB
        cmp     t1, w2, S2LOMEM #8
        sub     r3, w1, b1
        bic     r3, r3, w1
        bne     4f
        ands    r3, r3, b1, lsl #7
        it      eq
        ldreq   w2, [wp2], #4
        bne     5f
        eor     t1, t1, w1
        cmp     t1, w2, S2HIMEM #24
        bne     6f
        ldr     w1, [wp1], #4
        b       1b
4:
        S2LOMEM        w2, w2, #8
        b       8f

5:
#ifdef __ARMEB__
        /* The syndrome value may contain false ones if the string ends
        with the bytes 0x01 0x00 */
        tst     w1, #0xff000000
        itt     ne
        tstne   w1, #0x00ff0000
        tstne   w1, #0x0000ff00
        beq     7f
#else
        bics    r3, r3, #0xff000000
        bne     7f
#endif
        ldrb    w2, [wp2]
        S2LOMEM  t1, w1, #24
#ifdef __ARMEB__
        lsl     w2, w2, #24
#endif
        b       8f

6:
        S2LOMEM  t1, w1, #24
        and     w2, w2, LSB
        b       8f

        /* Critical inner Loop: Block with 2 bytes initial overlap */
        .p2align        2
2:
        S2HIMEM  t1, w1, #16
        sub     r3, w1, b1
        S2LOMEM  t1, t1, #16
        bic     r3, r3, w1
        cmp     t1, w2, S2LOMEM #16
        bne     4f
        ands    r3, r3, b1, lsl #7
        it      eq
        ldreq   w2, [wp2], #4
        bne     5f
        eor     t1, t1, w1
        cmp     t1, w2, S2HIMEM #16
        bne     6f
        ldr     w1, [wp1], #4
        b       2b

5:
#ifdef __ARMEB__
        /* The syndrome value may contain false ones if the string ends
        with the bytes 0x01 0x00 */
        tst     w1, #0xff000000
        it      ne
        tstne   w1, #0x00ff0000
        beq     7f
#else
        lsls    r3, r3, #16
        bne     7f
#endif
        ldrh    w2, [wp2]
        S2LOMEM  t1, w1, #16
#ifdef __ARMEB__
        lsl     w2, w2, #16
#endif
        b       8f

6:
        S2HIMEM  w2, w2, #16
        S2LOMEM  t1, w1, #16
4:
        S2LOMEM  w2, w2, #16
        b       8f

        /* Critical inner Loop: Block with 1 byte initial overlap */
        .p2align        2
3:
        and     t1, w1, LSB
        cmp     t1, w2, S2LOMEM #24
        sub     r3, w1, b1
        bic     r3, r3, w1
        bne     4f
        ands    r3, r3, b1, lsl #7
        it      eq
        ldreq   w2, [wp2], #4
        bne     5f
        eor     t1, t1, w1
        cmp     t1, w2, S2HIMEM #8
        bne     6f
        ldr     w1, [wp1], #4
        b       3b
4:
        S2LOMEM  w2, w2, #24
        b       8f
5:
        /* The syndrome value may contain false ones if the string ends
        with the bytes 0x01 0x00 */
        tst     w1, LSB
        beq     7f
        ldr     w2, [wp2], #4
6:
        S2LOMEM  t1, w1, #8
        bic     w2, w2, MSB
        b       8f
7:
        mov     r0, #0
        //ldmfd   sp!, {r4, r5}
        ldr     r4, [sp], #4
        ldr     r5, [sp], #4
        RETURN
8:
        and     r2, t1, LSB
        and     r0, w2, LSB
        cmp     r0, #1
        it      cs
        cmpcs   r0, r2
        itt     eq
        S2LOMEMEQ        t1, t1, #8
        S2LOMEMEQ        w2, w2, #8
        beq     8b
        sub     r0, r2, r0
        //ldmfd   sp!, {r4, r5}
        ldr     r4, [sp], #4
        ldr     r5, [sp], #4
        RETURN

#endif /* !(defined (_ISA_THUMB_2) || defined (_ISA_ARM_6)
            defined (__OPTIMIZE_SIZE__) || defined (PREFER_SIZE_OVER_SPEED) ||
            (defined (__thumb__) && !defined (__thumb2__))) */
