/*
 * This module derived from code donated to the FreeBSD Project by 
 * Matthew Dillon <dillon@backplane.com>
 *
 * Copyright (c) 1998 The FreeBSD Project
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/lib/libstand/zalloc_defs.h,v 1.6 1999/08/28 00:05:35 peter Exp $
 */

/*
 * DEFS.H
 */

#define USEGUARD		/* use stard/end guard bytes */
#define USEENDGUARD
#define DMALLOCDEBUG		/* add debugging code to gather stats */
#define ZALLOCDEBUG

#include <string.h>
#include "stand.h"

#ifdef __i386__
typedef unsigned int iaddr_t;	/* unsigned int same size as pointer	*/
typedef int saddr_t;		/* signed int same size as pointer	*/
#endif
#ifdef __alpha__
typedef unsigned long iaddr_t;	/* unsigned int same size as pointer	*/
typedef long saddr_t;		/* signed int same size as pointer	*/
#endif

#include "zalloc_mem.h"

#define Prototype extern
#define Library extern

#ifndef NULL
#define NULL	((void *)0)
#endif

/*
 * block extension for sbrk()
 */

#define BLKEXTEND	(4 * 1024)
#define BLKEXTENDMASK	(BLKEXTEND - 1)

/*
 * required malloc alignment.  Use sizeof(long double) for architecture
 * independance.
 *
 * Note: if we implement a more sophisticated realloc, we should ensure that
 * MALLOCALIGN is at least as large as MemNode.
 */

typedef struct Guard {
    size_t	ga_Bytes;
    size_t	ga_Magic;	/* must be at least 32 bits */
} Guard;

#define MATYPE		long double
#define MALLOCALIGN	((sizeof(MATYPE) > sizeof(Guard)) ? sizeof(MATYPE) : sizeof(Guard))
#define GAMAGIC		0x55FF44FD

#include "zalloc_protos.h"

