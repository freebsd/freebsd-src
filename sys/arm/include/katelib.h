/*	$NetBSD: katelib.h,v 1.3 2001/11/23 19:21:48 thorpej Exp $	*/

/*-
 * Copyright (c) 1994-1996 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * katelib.h
 *
 * Prototypes for machine specific functions. Most of these
 * could be inlined.
 *
 * This should not really be a separate header file. Eventually I will merge
 * this into other header files once I have decided where the declarations
 * should go.
 *
 * Created      : 18/09/94
 *
 * Based on kate/katelib/prototypes.h
 *
 * $FreeBSD$
 */

/*
 * USE OF THIS FILE IS DEPRECATED
 */

#ifndef _MACHINE_KATELIB_H_
#define _MACHINE_KATELIB_H_
#include <sys/types.h>
#include <machine/cpufunc.h>

#ifdef _KERNEL

/* Assembly modules */

/* In blockio.S */
#include <machine/blockio.h>

/* Macros for reading and writing words, shorts, bytes */

#define WriteWord(a, b) \
*((volatile unsigned int *)(a)) = (b)

#define ReadWord(a) \
(*((volatile unsigned int *)(a)))

#define WriteShort(a, b) \
*((volatile unsigned int *)(a)) = ((b) | ((b) << 16))

#define ReadShort(a) \
((*((volatile unsigned int *)(a))) & 0xffff)

#define WriteByte(a, b) \
*((volatile unsigned char *)(a)) = (b)

#define ReadByte(a) \
(*((volatile unsigned char *)(a)))

/* Define in/out macros */

#define inb(port)		ReadByte((port))
#define outb(port, byte)	WriteByte((port), (byte))
#define inw(port)		ReadShort((port))
#define outw(port, word)	WriteShort((port), (word))
#define inl(port)		ReadWord((port))
#define outl(port, lword)	WriteWord((port), (lword))

#endif

#endif /* !_MACHINE_KATELIB_H_ */
/* End of katelib.h */
