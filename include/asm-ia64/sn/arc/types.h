/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 1999,2001-2003 Silicon Graphics, Inc.  All Rights Reserved.
 * Copyright 1999 Ralf Baechle (ralf@gnu.org)
 */
#ifndef _ASM_SN_ARC_TYPES_H
#define _ASM_SN_ARC_TYPES_H

typedef char		CHAR;
typedef short		SHORT;
typedef long		LARGE_INTEGER __attribute__ ((__mode__ (__DI__)));
typedef	long		LONG __attribute__ ((__mode__ (__DI__)));
typedef unsigned char	UCHAR;
typedef unsigned short	USHORT;
typedef unsigned long	ULONG __attribute__ ((__mode__ (__DI__)));
typedef void		VOID;

/* The pointer types.  We're 64-bit and the firmware is also 64-bit, so
   live is sane ...  */
typedef CHAR		*_PCHAR;
typedef SHORT		*_PSHORT;
typedef LARGE_INTEGER	*_PLARGE_INTEGER;
typedef	LONG		*_PLONG;
typedef UCHAR		*_PUCHAR;
typedef USHORT		*_PUSHORT;
typedef ULONG		*_PULONG;
typedef VOID		*_PVOID;

typedef CHAR		*PCHAR;
typedef SHORT		*PSHORT;
typedef LARGE_INTEGER	*PLARGE_INTEGER;
typedef	LONG		*PLONG;
typedef UCHAR		*PUCHAR;
typedef USHORT		*PUSHORT;
typedef ULONG		*PULONG;
typedef VOID		*PVOID;

#endif /* _ASM_SN_ARC_TYPES_H */
