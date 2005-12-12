/*
 * Copyright (c) 2000-2002 Silicon Graphics, Inc.  All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 *
 * http://www.sgi.com
 *
 * For further information regarding this notice, see:
 *
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 */
#ifndef __XFS_ARCH_H__
#define __XFS_ARCH_H__

#ifndef XFS_BIG_INUMS
# error XFS_BIG_INUMS must be defined true or false
#endif

#ifdef __KERNEL__

#include <sys/endian.h>

#define	__LITTLE_ENDIAN	_LITTLE_ENDIAN
#define	__BIG_ENDIAN	_BIG_ENDIAN
#define	__BYTE_ORDER	_BYTE_ORDER

/* Compatibiliy defines */
#define	__swab16	__bswap16
#define	__swab32	__bswap32
#define	__swab64	__bswap64
#endif	/* __KERNEL__ */

/* do we need conversion? */

#define ARCH_NOCONVERT 1
#if __BYTE_ORDER == __LITTLE_ENDIAN
# define ARCH_CONVERT	0
#else
# define ARCH_CONVERT	ARCH_NOCONVERT
#endif

/* generic swapping macros */

#define INT_SWAP16(type,var) ((typeof(type))(__swab16((__u16)(var))))
#define INT_SWAP32(type,var) ((typeof(type))(__swab32((__u32)(var))))
#define INT_SWAP64(type,var) ((typeof(type))(__swab64((__u64)(var))))

#define INT_SWAP(type, var) \
    ((sizeof(type) == 8) ? INT_SWAP64(type,var) : \
    ((sizeof(type) == 4) ? INT_SWAP32(type,var) : \
    ((sizeof(type) == 2) ? INT_SWAP16(type,var) : \
    (var))))

#define INT_SWAP_UNALIGNED_32(from,to) \
    { \
	((__u8*)(to))[0] = ((__u8*)(from))[3]; \
	((__u8*)(to))[1] = ((__u8*)(from))[2]; \
	((__u8*)(to))[2] = ((__u8*)(from))[1]; \
	((__u8*)(to))[3] = ((__u8*)(from))[0]; \
    }

#define INT_SWAP_UNALIGNED_64(from,to) \
    { \
	INT_SWAP_UNALIGNED_32( ((__u8*)(from)) + 4, ((__u8*)(to))); \
	INT_SWAP_UNALIGNED_32( ((__u8*)(from)), ((__u8*)(to)) + 4); \
    }

/*
 * get and set integers from potentially unaligned locations
 */

#define INT_GET_UNALIGNED_16_LE(pointer) \
   ((__u16)((((__u8*)(pointer))[0]	) | (((__u8*)(pointer))[1] << 8 )))
#define INT_GET_UNALIGNED_16_BE(pointer) \
   ((__u16)((((__u8*)(pointer))[0] << 8) | (((__u8*)(pointer))[1])))
#define INT_SET_UNALIGNED_16_LE(pointer,value) \
    { \
	((__u8*)(pointer))[0] = (((value)     ) & 0xff); \
	((__u8*)(pointer))[1] = (((value) >> 8) & 0xff); \
    }
#define INT_SET_UNALIGNED_16_BE(pointer,value) \
    { \
	((__u8*)(pointer))[0] = (((value) >> 8) & 0xff); \
	((__u8*)(pointer))[1] = (((value)     ) & 0xff); \
    }

#define INT_GET_UNALIGNED_32_LE(pointer) \
   ((__u32)((((__u8*)(pointer))[0]	) | (((__u8*)(pointer))[1] << 8 ) \
	   |(((__u8*)(pointer))[2] << 16) | (((__u8*)(pointer))[3] << 24)))
#define INT_GET_UNALIGNED_32_BE(pointer) \
   ((__u32)((((__u8*)(pointer))[0] << 24) | (((__u8*)(pointer))[1] << 16) \
	   |(((__u8*)(pointer))[2] << 8)  | (((__u8*)(pointer))[3]	)))
#define INT_SET_UNALIGNED_32_LE(pointer, value) \
    { \
	  INT_SET_UNALIGNED_16_LE(pointer, \
		((value) & 0xffff)); \
	  INT_SET_UNALIGNED_16_LE(((__u8*)(pointer))+2, \
		(((value) >> 16) & 0xffff) ); \
    }
#define INT_SET_UNALIGNED_32_BE(pointer, value) \
    { \
	  INT_SET_UNALIGNED_16_BE(pointer, \
	      	(((value) >> 16) & 0xffff) ); \
	  INT_SET_UNALIGNED_16_BE(((__u8*)(pointer))+2, \
	      	((value) & 0xffff) ); \
    }

#define INT_GET_UNALIGNED_64_LE(pointer) \
   (((__u64)(INT_GET_UNALIGNED_32_LE(((__u8*)(pointer))+4)) << 32 ) \
   |((__u64)(INT_GET_UNALIGNED_32_LE(((__u8*)(pointer))	 ))	  ))
#define INT_GET_UNALIGNED_64_BE(pointer) \
   (((__u64)(INT_GET_UNALIGNED_32_BE(((__u8*)(pointer))	 )) << 32  ) \
   |((__u64)(INT_GET_UNALIGNED_32_BE(((__u8*)(pointer))+4))	   ))
#define INT_SET_UNALIGNED_64_LE(pointer, value) \
    { \
	  INT_SET_UNALIGNED_32_LE(pointer, \
		((value) & 0xffffffff)); \
	  INT_SET_UNALIGNED_32_LE(((__u8*)(pointer))+4, \
		(((value) >> 32) & 0xffffffff) ); \
    }
#define INT_SET_UNALIGNED_64_BE(pointer, value) \
    { \
	  INT_SET_UNALIGNED_32_BE(pointer, \
	      	(((value) >> 16) & 0xffff) ); \
	  INT_SET_UNALIGNED_32_BE(((__u8*)(pointer))+4, \
	      	((value) & 0xffff) ); \
    }

/*
 * now pick the right ones for our MACHINE ARCHITECTURE
 */

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define INT_GET_UNALIGNED_16(pointer)	    INT_GET_UNALIGNED_16_LE(pointer)
#define INT_SET_UNALIGNED_16(pointer,value) INT_SET_UNALIGNED_16_LE(pointer,value)
#define INT_GET_UNALIGNED_32(pointer)	    INT_GET_UNALIGNED_32_LE(pointer)
#define INT_SET_UNALIGNED_32(pointer,value) INT_SET_UNALIGNED_32_LE(pointer,value)
#define INT_GET_UNALIGNED_64(pointer)	    INT_GET_UNALIGNED_64_LE(pointer)
#define INT_SET_UNALIGNED_64(pointer,value) INT_SET_UNALIGNED_64_LE(pointer,value)
#else
#define INT_GET_UNALIGNED_16(pointer)	    INT_GET_UNALIGNED_16_BE(pointer)
#define INT_SET_UNALIGNED_16(pointer,value) INT_SET_UNALIGNED_16_BE(pointer,value)
#define INT_GET_UNALIGNED_32(pointer)	    INT_GET_UNALIGNED_32_BE(pointer)
#define INT_SET_UNALIGNED_32(pointer,value) INT_SET_UNALIGNED_32_BE(pointer,value)
#define INT_GET_UNALIGNED_64(pointer)	    INT_GET_UNALIGNED_64_BE(pointer)
#define INT_SET_UNALIGNED_64(pointer,value) INT_SET_UNALIGNED_64_BE(pointer,value)
#endif

/* define generic INT_ macros */

#define INT_GET(reference,arch) \
    (((arch) == ARCH_NOCONVERT) \
	? \
	    (reference) \
	: \
	    INT_SWAP((reference),(reference)) \
    )

/* does not return a value */
#define INT_SET(reference,arch,valueref) \
    (__builtin_constant_p(valueref) ? \
	(void)( (reference) = ( ((arch) != ARCH_NOCONVERT) ? (INT_SWAP((reference),(valueref))) : (valueref)) ) : \
	(void)( \
	    ((reference) = (valueref)), \
	    ( ((arch) != ARCH_NOCONVERT) ? (reference) = INT_SWAP((reference),(reference)) : 0 ) \
	) \
    )

/* does not return a value */
#define INT_MOD_EXPR(reference,arch,code) \
    (void)(((arch) == ARCH_NOCONVERT) \
	? \
	    ((reference) code) \
	: \
	    ( \
		(reference) = INT_GET((reference),arch) , \
		((reference) code), \
		INT_SET(reference, arch, reference) \
	    ) \
    )

/* does not return a value */
#define INT_MOD(reference,arch,delta) \
    (void)( \
	INT_MOD_EXPR(reference,arch,+=(delta)) \
    )

/*
 * INT_COPY - copy a value between two locations with the
 *	      _same architecture_ but _potentially different sizes_
 *
 *	    if the types of the two parameters are equal or they are
 *		in native architecture, a simple copy is done
 *
 *	    otherwise, architecture conversions are done
 *
 */

/* does not return a value */
#define INT_COPY(dst,src,arch) \
    (void)( \
	((sizeof(dst) == sizeof(src)) || ((arch) == ARCH_NOCONVERT)) \
	    ? \
		((dst) = (src)) \
	    : \
		INT_SET(dst, arch, INT_GET(src, arch)) \
    )

/*
 * INT_XLATE - copy a value in either direction between two locations
 *	       with different architectures
 *
 *		    dir < 0	- copy from memory to buffer (native to arch)
 *		    dir > 0	- copy from buffer to memory (arch to native)
 */

/* does not return a value */
#define INT_XLATE(buf,mem,dir,arch) {\
    ASSERT(dir); \
    if (dir>0) { \
	(mem)=INT_GET(buf, arch); \
    } else { \
	INT_SET(buf, arch, mem); \
    } \
}

#define INT_ISZERO(reference,arch) \
    ((reference) == 0)

#define INT_ZERO(reference,arch) \
    ((reference) = 0)

#define INT_GET_UNALIGNED_16_ARCH(pointer,arch) \
    ( ((arch) == ARCH_NOCONVERT) \
	? \
	    (INT_GET_UNALIGNED_16(pointer)) \
	: \
	    (INT_GET_UNALIGNED_16_BE(pointer)) \
    )
#define INT_SET_UNALIGNED_16_ARCH(pointer,value,arch) \
    if ((arch) == ARCH_NOCONVERT) { \
	INT_SET_UNALIGNED_16(pointer,value); \
    } else { \
	INT_SET_UNALIGNED_16_BE(pointer,value); \
    }

#define INT_GET_UNALIGNED_64_ARCH(pointer,arch) \
    ( ((arch) == ARCH_NOCONVERT) \
	? \
	    (INT_GET_UNALIGNED_64(pointer)) \
	: \
	    (INT_GET_UNALIGNED_64_BE(pointer)) \
    )
#define INT_SET_UNALIGNED_64_ARCH(pointer,value,arch) \
    if ((arch) == ARCH_NOCONVERT) { \
	INT_SET_UNALIGNED_64(pointer,value); \
    } else { \
	INT_SET_UNALIGNED_64_BE(pointer,value); \
    }

#define DIRINO4_GET_ARCH(pointer,arch) \
    ( ((arch) == ARCH_NOCONVERT) \
	? \
	    (INT_GET_UNALIGNED_32(pointer)) \
	: \
	    (INT_GET_UNALIGNED_32_BE(pointer)) \
    )

#if XFS_BIG_INUMS
#define DIRINO_GET_ARCH(pointer,arch) \
    ( ((arch) == ARCH_NOCONVERT) \
	? \
	    (INT_GET_UNALIGNED_64(pointer)) \
	: \
	    (INT_GET_UNALIGNED_64_BE(pointer)) \
    )
#else
/* MACHINE ARCHITECTURE dependent */
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define DIRINO_GET_ARCH(pointer,arch) \
    DIRINO4_GET_ARCH((((__u8*)pointer)+4),arch)
#else
#define DIRINO_GET_ARCH(pointer,arch) \
    DIRINO4_GET_ARCH(pointer,arch)
#endif
#endif

#define DIRINO_COPY_ARCH(from,to,arch) \
    if ((arch) == ARCH_NOCONVERT) { \
	memcpy(to,from,sizeof(xfs_ino_t)); \
    } else { \
	INT_SWAP_UNALIGNED_64(from,to); \
    }
#define DIRINO4_COPY_ARCH(from,to,arch) \
    if ((arch) == ARCH_NOCONVERT) { \
	memcpy(to,(((__u8*)from+4)),sizeof(xfs_dir2_ino4_t)); \
    } else { \
	INT_SWAP_UNALIGNED_32(from,to); \
    }

#endif	/* __XFS_ARCH_H__ */

