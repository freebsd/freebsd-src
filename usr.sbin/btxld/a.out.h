/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)exec.h	8.1 (Berkeley) 6/11/93
 * from: FreeBSD: src/sys/sys/imgact_aout.h,v 1.18 2002/09/05 07:54:03 bde Exp $
 * $FreeBSD$
 */

#include <sys/types.h>

#define N_GETMAGIC(ex) \
	( (ex).a_midmag & 0xffff )
#define N_SETMAGIC(ex,mag,mid,flag) \
	( (ex).a_midmag = (((flag) & 0x3f) <<26) | (((mid) & 0x03ff) << 16) | \
	((mag) & 0xffff) )

#define N_GETMAGIC_NET(ex) \
	(ntohl((ex).a_midmag) & 0xffff)

#define	__LDPGSZ	4096
#define N_ALIGN(ex,x) \
	(N_GETMAGIC(ex) == ZMAGIC || N_GETMAGIC(ex) == QMAGIC || \
	 N_GETMAGIC_NET(ex) == ZMAGIC || N_GETMAGIC_NET(ex) == QMAGIC ? \
	 ((x) + __LDPGSZ - 1) & ~(uint32_t)(__LDPGSZ - 1) : (x))

/* Valid magic number check. */
#define	N_BADMAG(ex) \
	(N_GETMAGIC(ex) != OMAGIC && N_GETMAGIC(ex) != NMAGIC && \
	 N_GETMAGIC(ex) != ZMAGIC && N_GETMAGIC(ex) != QMAGIC && \
	 N_GETMAGIC_NET(ex) != OMAGIC && N_GETMAGIC_NET(ex) != NMAGIC && \
	 N_GETMAGIC_NET(ex) != ZMAGIC && N_GETMAGIC_NET(ex) != QMAGIC)

/*
 * Header prepended to each a.out file.
 * only manipulate the a_midmag field via the
 * N_SETMAGIC/N_GET{MAGIC,MID,FLAG} macros in a.out.h
 */

struct exec {
     uint32_t	a_midmag;	/* flags<<26 | mid<<16 | magic */
     uint32_t	a_text;		/* text segment size */
     uint32_t	a_data;		/* initialized data size */
     uint32_t	a_bss;		/* uninitialized data size */
     uint32_t	a_syms;		/* symbol table size */
     uint32_t	a_entry;	/* entry point */
     uint32_t	a_trsize;	/* text relocation size */
     uint32_t	a_drsize;	/* data relocation size */
};

/* a_magic */
#define	OMAGIC		0407	/* old impure format */
#define	NMAGIC		0410	/* read-only text */
#define	ZMAGIC		0413	/* demand load format */
#define QMAGIC          0314    /* "compact" demand load format */

/* a_mid */
#define	MID_ZERO	0	/* unknown - implementation dependent */
