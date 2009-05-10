/*
 * Copyright (c) 1996, 2001-2003, 2005, Juniper Networks, Inc.
 * All rights reserved.
 *
 * defs.h -- Simple universal types and definitions for use by the microkernel
 * Jim Hayes, November 1996
 *
 *	JNPR: defs.h,v 1.3.2.1 2007/09/10 08:16:32 girish
 * $FreeBSD$
 */

#ifndef __DEFS_H__
#define	__DEFS_H__

/*
 * Paranoid compilation. If defined, the PARANOID flag will enable asserts,
 * data structure magic stamping and a suite of other debug tools. To disable
 * it, comment out its definition.
 */
#define	PARANOID

/*
 * This is the ONLY place you should see hardware specific information
 * encoded as #ifdefs. (Well, except for stdarg.h, perhaps.)
 * I apologize in advance!
 */
#include <machine/defs_mips.h>
#define	CPU_GOT_ONE

#if !defined(CPU_GOT_ONE)
#error "YOU NEED TO SPECIFY ONE CPU TYPE TO USE THIS FILE"
#endif

#ifdef	TRUE
#undef	TRUE
#endif

#ifdef	FALSE
#undef	FALSE
#endif

typedef enum boolean_
{
	FALSE = 0,
	TRUE = 1
} boolean;

/*
 * Make NULL a pointer within the microkernel environment to catch
 * pointer semantic miscreants.
 *
 * The reason it's conditional here is that some of the BSD includes
 * define it multiple times as a straight integer and GCC barfs on
 * the alternative prototypes.
 */

#ifndef NULL
#define	NULL (void *)0
#endif

/*
 * Define some standard sized types.  (Defined in cpu-specific type files
 * included above.)
 */

#define	MAX_U8  255
#define	MAX_S8  128
#define	MIN_S8 -127

#define	MAX_U16  0xffff
#define	MIN_S16  ((int16_t)(1 << 15))
#define	MAX_S16  ((int16_t)~MIN_S16)

#define	MAX_U32  0xffffffff
#define	MIN_S32  ((int32_t)(1 << 31))
#define	MAX_S32  ((int32_t)~MIN_S32)

#define	MAX_U64  ((u_int64_t)0 - 1)
#define	MAX_S64  ((int64_t)(MAX_U64 >> 1))
#define	MIN_S64  (-MAX_S64-1)

/*
 * Solaris uses _SIZE_T to mark the fact that "size_t" has already
 * been defined.  _SYS_TYPES_H_ is used by BSD.
 * 
 */
#if !defined(_SYS_TYPES_H_) && !defined(_SIZE_T)
typedef UNSIGNED_32  size_t;
#define	_SIZE_T
#endif

#if !defined(_SYS_TYPES_H_)
typedef char *		caddr_t;

typedef UNSIGNED_8	u_int8_t;
typedef SIGNED_8	int8_t;

typedef UNSIGNED_16	u_int16_t;
typedef SIGNED_16	int16_t;

typedef UNSIGNED_32	u_int32_t;
typedef SIGNED_32	int32_t;

typedef UNSIGNED_64	u_int64_t;
typedef SIGNED_64	int64_t;

typedef UNSIGNED_32	u_long;
typedef UNSIGNED_16	u_short;
typedef UNSIGNED_8	u_char;


/*
 * Define the standard terminology used in the diag software
 * with regards to bytes, words, etc.
 * BYTE = 8 bits
 * HWORD (halfword) = 2 bytes or 16 bits
 * WORD = 4 bytes or 32 bits
 * QUAD = 8 bytes or 64 bits
 *
 * (The term QUAD seems less-than-intuitive here, but it is
 * derived from BSD sources where it is defined as int64_t.)
 *
 * For consistency use the following defines wherever appropriate.
 */

typedef enum {
	NBI_BYTE  = (sizeof(u_int8_t) * 8),
	NBI_HWORD = (sizeof(u_int16_t) * 8),
	NBI_WORD  = (sizeof(u_int32_t) * 8),
	NBI_QUAD  = (sizeof(u_int64_t) * 8)
} num_bits_t;

typedef enum {
	NBY_BYTE  = sizeof(u_int8_t),
	NBY_HWORD = sizeof(u_int16_t),
	NBY_WORD  = sizeof(u_int32_t),
	NBY_QUAD  = sizeof(u_int64_t)
} num_bytes_t;

/*
 * We assume that pid values are 16 bit integers
 */

typedef u_int16_t pid_t;

#endif /* _SYS_TYPES_H_ */

typedef UNSIGNED_32	magic_t;
typedef int		status_t;

#define	BITS_IN_BYTE	8

/*
 * Packed definition. We use this for fields in network frames where we
 * don't want the compiler to pack out to even alignment
 */

#ifdef PACKED
#undef PACKED
#endif
#define	PACKED(x)	x __attribute__ ((packed))

/*
 * __unused is a FreeBSDism that prevents the compiler from choking
 * on function parameters that remain unused through the life of a
 * function.  This is not an issue for the Cygnus toolchain.  In general
 * it SHOULD NOT BE USED in the martini embedded software repository.
 * It should only be used inside of shared code.
 */
#ifndef __unused
#define	__unused	__attribute__ ((__unused__))
#endif

/*
 * Basic memory multiples
 */

#define	SIZE_1K		0x00000400
#define	SIZE_2K		0x00000800
#define	SIZE_4K		0x00001000
#define	SIZE_8K		0x00002000
#define	SIZE_16K	0x00004000
#define	SIZE_32K	0x00008000
#define	SIZE_64K	0x00010000
#define	SIZE_128K	0x00020000
#define	SIZE_256K	0x00040000
#define	SIZE_512K	0x00080000
#define	SIZE_1M		0x00100000
#define	SIZE_2M		0x00200000
#define	SIZE_4M		0x00400000
#define	SIZE_8M		0x00800000
#define	SIZE_16M	0x01000000
#define	SIZE_32M	0x02000000
#define	SIZE_64M	0x04000000
#define	SIZE_128M	0x08000000
#define	SIZE_256M	0x10000000
#define	SIZE_512M	0x20000000
#define	SIZE_1G		0x40000000
#define	SIZE_2G		0x80000000

/*
 * swap16_inline
 * swap32_inline
 *
 * Byteswap a 16 and 32 bit quantities
 */

static inline u_int16_t
swap16_inline(u_int16_t data)
{
	return(((data & 0x00ff) << 8) |
	       ((data & 0xff00) >> 8));
}

static inline u_int32_t
swap32_inline(u_int32_t data)
{
	return(((data & 0x000000ff) << 24) |
	       ((data & 0x0000ff00) << 8) |
	       ((data & 0x00ff0000) >> 8) |
	       ((data & 0xff000000) >> 24));
}

/*
 * Define errno_t here as it is needed by the rom and ukernel
 */
typedef u_int32_t errno_t;

#define	EOK	0

/*
 * Define the main communication structure used for passing
 * information from the rom to the ukernel (done here as it is
 * used by them both)
 */
typedef struct rom_info_ rom_info_t;

/*
 * Typedef the return code from the ukernel to the ROM
 */
typedef u_int32_t rom_return_t;

/*
 * Pull in the relevant global environment header file
 *
 * This file is shared by the uKernel and the system simulation effort.
 */
#if defined(ENV_UKERN) || defined (ENV_SYS_SIM)
#include "ukern.h"
#endif /* ENV_UKERN */

#if defined(ENV_ROM)
#include "rom.h"
#endif
 
#endif /* __DEFS_H__ */
