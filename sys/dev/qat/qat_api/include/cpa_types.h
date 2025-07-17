/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */

/*
 *****************************************************************************
 * Doxygen group definitions
 ****************************************************************************/

/**
 *****************************************************************************
 * @file cpa_types.h
 *
 * @defgroup cpa_Types CPA Type Definition
 *
 * @ingroup cpa
 *
 * @description
 *      This is the CPA Type Definitions.
 *
 *****************************************************************************/

#ifndef CPA_TYPES_H
#define CPA_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#if   defined (__FreeBSD__) && defined (_KERNEL)

/* FreeBSD kernel mode */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/kernel.h>

#else

/* Linux, FreeBSD, or Windows user mode */
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>

#endif

#if defined (WIN32) || defined (_WIN64)
/* nonstandard extension used : zero-sized array in struct/union */
#pragma warning (disable: 4200)
#endif

typedef uint8_t Cpa8U;
/**<
 * @file cpa_types.h
 * @ingroup cpa_Types
 * Unsigned byte base type. */
typedef int8_t Cpa8S;
/**<
 * @file cpa_types.h
 * @ingroup cpa_Types
 * Signed byte base type. */
typedef uint16_t Cpa16U;
/**<
 * @file cpa_types.h
 * @ingroup cpa_Types
 * Unsigned double-byte base type. */
typedef int16_t Cpa16S;
/**<
 * @file cpa_types.h
 * @ingroup cpa_Types
 * Signed double-byte base type. */
typedef uint32_t Cpa32U;
/**<
 * @file cpa_types.h
 * @ingroup cpa_Types
 * Unsigned quad-byte base type. */
typedef int32_t Cpa32S;
/**<
 * @file cpa_types.h
 * @ingroup cpa_Types
 * Signed quad-byte base type. */
typedef uint64_t Cpa64U;
/**<
 * @file cpa_types.h
 * @ingroup cpa_Types
 * Unsigned double-quad-byte base type. */
typedef int64_t Cpa64S;
/**<
 * @file cpa_types.h
 * @ingroup cpa_Types
 * Signed double-quad-byte base type. */

/*****************************************************************************
 *      Generic Base Data Type definitions
 *****************************************************************************/
#ifndef NULL
#define NULL (0)
/**<
 * @file cpa_types.h
 * @ingroup cpa_Types
 * NULL definition. */
#endif

/**
 *****************************************************************************
 * @ingroup cpa_Types
 *      Boolean type.
 *
 * @description
 *      Functions in this API use this type for Boolean variables that take
 *      true or false values.
 *
 *****************************************************************************/
typedef enum _CpaBoolean
{
    CPA_FALSE = (0==1), /**< False value */
    CPA_TRUE = (1==1) /**< True value */
} CpaBoolean;


/**
 *****************************************************************************
 * @ingroup cpa_Types
 *      Declare a bitmap of specified size (in bits).
 *
 * @description
 *      This macro is used to declare a bitmap of arbitrary size.
 *
 *      To test whether a bit in the bitmap is set, use @ref
 *      CPA_BITMAP_BIT_TEST.
 *
 *      While most uses of bitmaps on the API are read-only, macros are also
 *      provided to set (see @ref CPA_BITMAP_BIT_SET) and clear (see @ref
 *      CPA_BITMAP_BIT_CLEAR) bits in the bitmap.
 *****************************************************************************/
#define CPA_BITMAP(name, sizeInBits) \
        Cpa32U name[((sizeInBits)+31)/32]

#define CPA_BITMAP_BIT_TEST(bitmask, bit) \
        ((bitmask[(bit)/32]) & (0x1 << ((bit)%32)))
/**<
 * @ingroup cpa_Types
 * Test a specified bit in the specified bitmap.  The bitmap may have been
 * declared using @ref CPA_BITMAP.  Returns a Boolean (true if the bit is
 * set, false otherwise). */

#define CPA_BITMAP_BIT_SET(bitmask, bit) \
        (bitmask[(bit)/32] |= (0x1 << ((bit)%32)))
/**<
 * @file cpa_types.h
 * @ingroup cpa_Types
 * Set a specified bit in the specified bitmap.  The bitmap may have been
 * declared using @ref CPA_BITMAP. */

#define CPA_BITMAP_BIT_CLEAR(bitmask, bit) \
        (bitmask[(bit)/32] &= ~(0x1 << ((bit)%32)))
/**<
 * @ingroup cpa_Types
 * Clear a specified bit in the specified bitmap.  The bitmap may have been
 * declared using @ref CPA_BITMAP. */


/**
 **********************************************************************
 *
 * @ingroup cpa_Types
 *
 * @description
 *       Declare a function or type and mark it as deprecated so that
 *       usages get flagged with a warning.
 *
 **********************************************************************
 */
#if defined(__GNUC__) || defined(__INTEL_COMPILER) || defined(_WIN64)
/*
 * gcc and icc support the __attribute__ ((deprecated)) syntax for marking
 * functions and other constructs as deprecated.
 */
/*
 * Uncomment the deprecated macro if you need to see which structs are deprecated
 */
#define CPA_DEPRECATED 
/*#define CPA_DEPRECATED __attribute__ ((deprecated)) */
#else
/*
 * for all other compilers, define deprecated to do nothing
 *
 */
/* #define CPA_DEPRECATED_FUNC(func) func; #pragma deprecated(func) */
#pragma message("WARNING: You need to implement the CPA_DEPRECATED macro for this compiler")
#define CPA_DEPRECATED
#endif

#ifdef __cplusplus
} /* close the extern "C" { */
#endif

#endif /* CPA_TYPES_H */
