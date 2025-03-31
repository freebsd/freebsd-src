/*
 *  Copyright (C) 2017 - This file is part of libecc project
 *
 *  Authors:
 *      Ryad BENADJILA <ryadbenadjila@gmail.com>
 *      Arnaud EBALARD <arnaud.ebalard@ssi.gouv.fr>
 *      Jean-Pierre FLORI <jean-pierre.flori@ssi.gouv.fr>
 *
 *  Contributors:
 *      Nicolas VIVET <nicolas.vivet@ssi.gouv.fr>
 *      Karim KHALFALLAH <karim.khalfallah@ssi.gouv.fr>
 *
 *  This software is licensed under a dual BSD and GPL v2 license.
 *  See LICENSE file at the root folder of the project.
 */
#ifndef __WORDS_H__
#define __WORDS_H__

/*
 * Types for words and a few useful macros.
 */

/*
 * If a word size is forced, we use the proper words.h definition.
 * By default, 64-bit word size is used since it is the most reasonable
 * choice across the known platforms (see below).
 */

#define __concat(x) #x
#define _concat(file_prefix, x) __concat(file_prefix##x.h)
#define concat(file_prefix, x) _concat(file_prefix, x)

#if defined(WORDSIZE)
	/* The word size is forced by the compilation chain */
#if (WORDSIZE == 16) || (WORDSIZE == 32) || (WORDSIZE == 64)
	/* Dynamic include depending on the word size */
#include concat(words_, WORDSIZE)
#else
#error Unsupported word size concat
#endif
#else
/* The word size is usually deduced from the known platforms.
 * By default when we have fast builtin uint64_t operations,
 * we use WORDSIZE=64. This is obviously the case on 64-bit platforms,
 * but this should also be the case on most 32-bit platforms where
 * native instructions allow a 32-bit x 32-bit to 64-bit multiplication.
 *
 * There might however be some platforms where this is not the case.
 * Cortex-M0/M0+ for example does not have such a native multiplication
 * instruction, yielding in slower code for WORDSIZE=64 than for WORDSIZE=32.
 * This is also the case for old Thumb ARM CPUs (pre Thumb-2).
 *
 * On 8-bit and 16-bit platform, we prefer to let the user decide on the best
 * option (see below)!
 */
#if defined(__x86_64__) || defined(__i386__) || defined(__ppc64__) || defined(__ppc__) ||\
    defined(__arm__) || defined(__aarch64__) || defined(__mips__) || defined(__s390x__) ||\
    defined(__SH4__) || defined(__sparc__) || defined(__riscv)
#define WORDSIZE 64
#include "words_64.h"
#else
/* We let the user fix the WORDSIZE manually */
#error "Unrecognized platform. Please specify the word size of your target (with make 16, make 32, make 64)"
#endif /* Unrecognized */
#endif

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef u16 bitcnt_t;

/*
 * Shift behavior is not defined for a shift count
 * higher than (WORD_BITS - 1). These macros emulate
 * the behavior one would expect, i.e. return 0 when
 * shift count is equal or more than word size.
 */
#define WLSHIFT(w, c) ((word_t)(((c) >= WORD_BITS) ? WORD(0) : (word_t)((w) << (c))))
#define WRSHIFT(w, c) ((word_t)(((c) >= WORD_BITS) ? WORD(0) : (word_t)((w) >> (c))))

/* To be fixed: not really constant-time. */
#define WORD_MIN(a, b) ((a) > (b) ? (b) : (a))

/* WORD_MASK[_IF[NOT]ZERO]: mask of word size and associated macros. */
#define WORD_MASK WORD_MAX

/* These two macros assume two-complement representation. */
#define WORD_MASK_IFZERO(w) ((word_t)(((word_t)((w) != 0)) - WORD(1)))
#define WORD_MASK_IFNOTZERO(w) ((word_t)(((word_t)((w) == 0)) - WORD(1)))

#define HWORD_MASK HWORD_MAX

/* WORD_HIGHBIT: constant of word size with only MSB set. */
#define WORD_HIGHBIT (WORD(1) << (WORD_BITS - 1))

/* WORD_MUL: multiply two words using schoolbook multiplication on half words */
#define WORD_MUL(outh, outl, in1, in2) do {				\
	word_t in1h, in1l, in2h, in2l;					\
	word_t tmph, tmpm, tmpl;					\
	word_t tmpm1, tmpm2;						\
	word_t carrym, carryl;						\
	/* Get high and low half words. */				\
	in1h = (in1) >> HWORD_BITS;					\
	in1l = (in1) & HWORD_MASK;					\
	in2h = (in2) >> HWORD_BITS;					\
	in2l = (in2) & HWORD_MASK;					\
	/* Compute low product. */					\
	tmpl = (word_t)(in2l * in1l);					\
	/* Compute middle product. */					\
	tmpm1 = (word_t)(in2h * in1l);					\
	tmpm2 = (word_t)(in2l * in1h);					\
	tmpm = (word_t)(tmpm1 + tmpm2);					\
	/* Store middle product carry. */				\
	carrym = (word_t)(tmpm < tmpm1);				\
	/* Compute full low product. */					\
	(outl) = tmpl;							\
	(outl) = (word_t)((outl) + ((tmpm & HWORD_MASK) << HWORD_BITS));\
	/* Store full low product carry. */				\
	carryl = (word_t)((outl) < tmpl);				\
	/* Compute full high product. */				\
	carryl = (word_t)(carryl + (tmpm >> HWORD_BITS));		\
	carryl = (word_t)(carryl + (carrym << HWORD_BITS));		\
	tmph = (word_t)(in2h * in1h);					\
	/* No carry can occur below. */					\
	(outh) = (word_t)(tmph + carryl);				\
	} while (0)

#endif /* __WORDS_H__ */
