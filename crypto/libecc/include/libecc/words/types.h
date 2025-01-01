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
#ifndef __TYPES_H__
#define __TYPES_H__

/*** Handling the target compiler and its specificities ***/
#ifdef __GNUC__
/* gcc and clang */
#define ATTRIBUTE_UNUSED __attribute__((unused))
#define ATTRIBUTE_USED __attribute__((used))
#define ATTRIBUTE_PACKED __attribute__((packed))
#define ATTRIBUTE_SECTION(a) __attribute__((__section__((a))))
#ifdef USE_WARN_UNUSED_RET
  #define ATTRIBUTE_WARN_UNUSED_RET __attribute__((warn_unused_result))
  static inline void ignore_result(int unused_result) {
    (void) unused_result;
  }
  /* NOTE: this trick using a dummy function call is here
   * to explicitly avoid "unused return values" when we know
   * what we are doing!
   */
  #define IGNORE_RET_VAL(a) ignore_result((int)(a))
#else
  #define ATTRIBUTE_WARN_UNUSED_RET
  #define IGNORE_RET_VAL(a) (a)
#endif /* USE_WARN_UNUSED_RET */
#else
#define ATTRIBUTE_UNUSED
#define ATTRIBUTE_USED
#define ATTRIBUTE_PACKED
#define ATTRIBUTE_SECTION(a)
#define ATTRIBUTE_WARN_UNUSED_RET
#define IGNORE_RET_VAL(a) (a)
#endif

/* Macro to trick the compiler of thinking a variable is used.
 * Although this should not happen, sometimes because of #define
 * oddities we might force this.
 */
#define FORCE_USED_VAR(a) ((void)(a))

/*** Handling the types ****/
#ifdef WITH_STDLIB

/*
 * User explicitly needs to build w/ stdlib. Let's include the headers
 * we need to get basic types: (uint*_t), NULL, etc. You can see below
 * (i.e. under #else) what is precisely needed.
 */
#include <stdint.h>
#include <inttypes.h>
#include <stddef.h>
#else /* WITH_STDLIB */

/*
 * User does not want to build w/ stdlib. Let's define basic types:
 * (uint*_t), NULL, etc.
 */
#define NULL ((void *)0)
typedef unsigned int size_t;
typedef int ssize_t;
/* Here is the big picture of the main programming models
 * and their primitive types sizes in bits:
 * (see http://www.unix.org/whitepapers/64bit.html)
 *
 *           | LP32 | ILP32 | LLP64 | ILP64 | LP64 |
 *           ---------------------------------------
 *  char     |   8  |   8   |   8   |   8   |   8  |
 *  short    |  16  |  16   |  16   |  16   |  16  |
 *  int      |  16  |  32   |  32   |  64   |  32  |
 *  long     |  32  |  32   |  32   |  64   |  64  |
 *  long long|  64  |  64   |  64   |  64   |  64  |
 *           --------------------------------------
 *           (long long type existence depends on the C compiler
 *	      but should be *mandatory* in C99 compliant ones)
 *
 * This means that:
 *	1) We are sure that long long are 64-bit, short are 16-bit,
 *	   and char are 8-bit (on the vast majority of platforms).
 *	2) The two types that are not consistent across platforms are
 *	the int and long types (e.g., int is 16 bits and long is 32 bits
 *	on msp430-gcc LP32, while int is 32 bits and long is 32 bits
 *	on x86_64 Linux platforms LLP64, and long becomes 64 bits on
 *	x86_64 Windows platforms LP64).
 *
 * Hence, we take a wild guess for uint32_t mapping on a primitive type
 * and check this at compilation time using the check_data_types 'union'
 * defined below.
 * Our guess depends on the WORDSIZE the user provides us, since we assume
 * this corresponds to a 'native' word size of the platform (8-bit platforms
 * such as AVR and 16-bit platforms such as MSP430 are either LP32 and ILP32,
 * which means that a 'long' type will be mapped to a 32 bits native type).
 * Consequently, when the user provides us a WORDSIZE=16, we infer that
 * uint32_t is mapped on a long type.
 *
 */
typedef unsigned long long uint64_t;
/* GCC defines the size of an int, use this information
 * if it is provided
 */
#ifdef __SIZEOF_INT__
#if(__SIZEOF_INT__ == 2)
#define UINT32_IS_LONG /* This will be useful for print formatting */
typedef unsigned long uint32_t;
#else
typedef unsigned int uint32_t;
#endif /* (__SIZEOF_INT__ == 2) */
#else
#if(WORDSIZE == 16)
/* The user has provided WORDSIZE=16, so we guess that
 * we have LP32 or ILP32: a long type would be 32-bit.
 */
#define UINT32_IS_LONG /* This will be useful for print formatting */
typedef unsigned long uint32_t;
#else
/* Wild guess for uint32_t mapping */
typedef unsigned int uint32_t;
#endif /* (WORDSIZE == 16) */
#endif /* __SIZEOF_INT__ */
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
/* Useful macros for our new defined types */
#define UINT8_MAX  (0xff)
#define UINT16_MAX (0xffff)
#define UINT32_MAX (0xffffffff)
#define UINT64_MAX (0xffffffffffffffffULL)
#define UINT8_C(c) ((uint8_t)(c ## UL))
#define UINT16_C(c) ((uint16_t)(c ## UL))
#define UINT32_C(c) ((uint32_t)(c ## UL))
#define UINT64_C(c) (c ## ULL)

/* Sanity check on our guess for primitive types sizes.
 * See https://barrgroup.com/Embedded-Systems/How-To/C-Fixed-Width-Integers-C99
 *
 * TODO: if you get a compilation error at this point, this means that we failed
 * at guessing the C primitive types sizes for the current platform. You should
 * try to adapt the uint8_t/uint16_t/uint32_t/uint64_t types definitions in this
 * file, or find the C99 compliant stdint headers for your compiler/platform
 * and include it.
 */
typedef union {
	char uint8_t_incorrect[sizeof(uint8_t) == 1 ? 1 : -1];
	char uint16_t_incorrect[sizeof(uint16_t) == 2 ? 1 : -1];
	char uint32_t_incorrect[sizeof(uint32_t) == 4 ? 1 : -1];
	char uint64_t_incorrect[sizeof(uint64_t) == 8 ? 1 : -1];
} check_data_types;

#endif /* WITH_STDLIB */

#endif /* __TYPES_H__ */
