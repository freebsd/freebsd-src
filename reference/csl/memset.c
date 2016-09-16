/* Copyright (c) 2009 CodeSourcery, Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of CodeSourcery nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY CODESOURCERY, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CODESOURCERY BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "arm_asm.h"
#include <string.h>
#include <stdint.h>

/* Standard operations for word-sized values.  */
#define WORD_REF(ADDRESS, OFFSET) \
	*((WORD_TYPE*)((char*)(ADDRESS) + (OFFSET)))

/* On processors with NEON, we use 128-bit vectors.  Also,
   we need to include arm_neon.h to use these.  */
#if defined(__ARM_NEON__)
  #include <arm_neon.h>

  #define WORD_TYPE uint8x16_t
  #define WORD_SIZE 16

  #define WORD_DUPLICATE(VALUE) \
	vdupq_n_u8(VALUE)

/* On ARM processors with 64-bit ldrd instructions, we use those,
   except on Cortex-M* where benchmarking has shown them to
   be slower.  */
#elif defined(__ARM_ARCH_5E__) || defined(__ARM_ARCH_5TE__) \
	|| defined(__ARM_ARCH_5TEJ__) || defined(_ISA_ARM_6)
  #define WORD_TYPE uint64_t
  #define WORD_SIZE 8

  /* ARM stores 64-bit values in two 32-bit registers and does not
     have 64-bit multiply or bitwise-or instructions, so this union
     operation results in optimal code.  */
  static inline uint64_t splat8(value) {
	union { uint32_t ints[2]; uint64_t result; } quad;
	quad.ints[0] = (unsigned char)(value) * 0x01010101;
	quad.ints[1] = quad.ints[0];
	return quad.result;
  }
  #define WORD_DUPLICATE(VALUE) \
	splat8(VALUE)

/* On everything else, we use 32-bit loads and stores.  */
#else
  #define WORD_TYPE uint32_t
  #define WORD_SIZE 4
  #define WORD_DUPLICATE(VALUE) \
	(unsigned char)(VALUE) * 0x01010101
#endif

/* On all ARM platforms, 'SHORTWORD' is a 32-bit value.  */
#define SHORTWORD_TYPE uint32_t
#define SHORTWORD_SIZE 4
#define SHORTWORD_REF(ADDRESS, OFFSET) \
	*((SHORTWORD_TYPE*)((char*)(ADDRESS) + (OFFSET)))
#define SHORTWORD_DUPLICATE(VALUE) \
	(uint32_t)(unsigned char)(VALUE) * 0x01010101

void *memset(void *DST, int C, size_t LENGTH)
{
  void* DST0 = DST;
  unsigned char C_BYTE = C;

#if defined(PREFER_SIZE_OVER_SPEED) || defined(__OPTIMIZE_SIZE__)
  const char* DST_end = (char*)DST + LENGTH;
  while ((char*)DST < DST_end) {
    *((char*)DST) = C_BYTE;
    DST++;
  }

  return DST0;
#else /* not PREFER_SIZE_OVER_SPEED */
  /* Handle short strings and immediately return.  */
  if (__builtin_expect(LENGTH < SHORTWORD_SIZE, 1)) {
    size_t i = 0;
    while (i < LENGTH) {
      ((char*)DST)[i] = C_BYTE;
      i++;
    }
    return DST;
  }

  const char* DST_end = (char*)DST + LENGTH;

  /* Align DST to SHORTWORD_SIZE.  */
  while ((uintptr_t)DST % SHORTWORD_SIZE != 0) {
    *(char*) (DST++) = C_BYTE;
  }

#if WORD_SIZE > SHORTWORD_SIZE
  SHORTWORD_TYPE C_SHORTWORD = SHORTWORD_DUPLICATE(C_BYTE);

  /* Align DST to WORD_SIZE in steps of SHORTWORD_SIZE.  */
  if (__builtin_expect(DST_end - (char*)DST >= WORD_SIZE, 0)) {
    while ((uintptr_t)DST % WORD_SIZE != 0) {
      SHORTWORD_REF(DST, 0) = C_SHORTWORD;
      DST += SHORTWORD_SIZE;
    }
#endif /* WORD_SIZE > SHORTWORD_SIZE */

    WORD_TYPE C_WORD = WORD_DUPLICATE(C_BYTE);

#if defined(__ARM_NEON__)
    /* Testing on Cortex-A8 indicates that the following idiom
       produces faster assembly code when doing vector copies,
       but not when doing regular copies.  */
    size_t i = 0;
    LENGTH = DST_end - (char*)DST;
    while (i + WORD_SIZE * 16 <= LENGTH) {
      WORD_REF(DST, i) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 1) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 2) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 3) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 4) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 5) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 6) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 7) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 8) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 9) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 10) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 11) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 12) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 13) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 14) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 15) = C_WORD;
      i += WORD_SIZE * 16;
    }
    while (i + WORD_SIZE * 4 <= LENGTH) {
      WORD_REF(DST, i) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 1) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 2) = C_WORD;
      WORD_REF(DST, i + WORD_SIZE * 3) = C_WORD;
      i += WORD_SIZE * 4;
    }
    while (i + WORD_SIZE <= LENGTH) {
      WORD_REF(DST, i) = C_WORD;
      i += WORD_SIZE;
    }
    DST += i;
#else /* not defined(__ARM_NEON__) */
    /* Note: 16-times unrolling is about 50% faster than 4-times
       unrolling on both ARM Cortex-A8 and Cortex-M3.  */
    while (DST_end - (char*) DST >= WORD_SIZE * 16) {
      WORD_REF(DST, 0) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 1) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 2) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 3) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 4) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 5) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 6) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 7) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 8) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 9) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 10) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 11) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 12) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 13) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 14) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 15) = C_WORD;
      DST += WORD_SIZE * 16;
    }
    while (WORD_SIZE * 4 <= DST_end - (char*) DST) {
      WORD_REF(DST, 0) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 1) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 2) = C_WORD;
      WORD_REF(DST, WORD_SIZE * 3) = C_WORD;
      DST += WORD_SIZE * 4;
    }
    while (WORD_SIZE <= DST_end - (char*) DST) {
      WORD_REF(DST, 0) = C_WORD;
      DST += WORD_SIZE;
    }
#endif /* not defined(__ARM_NEON__) */

#if WORD_SIZE > SHORTWORD_SIZE
  } /* end if N >= WORD_SIZE */

  while (SHORTWORD_SIZE <= DST_end - (char*)DST) {
    SHORTWORD_REF(DST, 0) = C_SHORTWORD;
    DST += SHORTWORD_SIZE;
  }
#endif /* WORD_SIZE > SHORTWORD_SIZE */

  while ((char*)DST < DST_end) {
    *((char*)DST) = C_BYTE;
    DST++;
  }

  return DST0;
#endif /* not PREFER_SIZE_OVER_SPEED */
}
