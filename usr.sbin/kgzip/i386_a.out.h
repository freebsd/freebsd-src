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

#ifndef	_I386_AOUT_H_
#define	_I386_AOUT_H_

#include <sys/types.h>
#include "endian.h"

#define	__I386_LDPGSZ	4096

#define I386_N_GETMAGIC(ex) \
	( LE32TOH((ex).a_midmag) & 0xffff )
#define I386_N_SETMAGIC(ex,mag,mid,flag) \
	( (ex).a_midmag = HTOLE32((((flag) & 0x3f) <<26) | \
	(((mid) & 0x03ff) << 16) | \
	((mag) & 0xffff)) )

#define I386_N_ALIGN(ex,x) \
	(I386_N_GETMAGIC(ex) == ZMAGIC || I386_N_GETMAGIC(ex) == QMAGIC ? \
	 ((x) + __I386_LDPGSZ - 1) & ~(uint32_t)(__I386_LDPGSZ - 1) : (x))

/* Valid magic number check. */
#define	I386_N_BADMAG(ex) \
	(I386_N_GETMAGIC(ex) != OMAGIC && I386_N_GETMAGIC(ex) != NMAGIC && \
	 I386_N_GETMAGIC(ex) != ZMAGIC && I386_N_GETMAGIC(ex) != QMAGIC)


/* Address of the bottom of the text segment. */
/*
 * This can not be done right.  Abuse a_entry in some cases to handle kernels.
 */
#define I386_N_TXTADDR(ex) \
	((I386_N_GETMAGIC(ex) == OMAGIC || I386_N_GETMAGIC(ex) == NMAGIC || \
	I386_N_GETMAGIC(ex) == ZMAGIC) ? \
	(LE32TOH((ex).a_entry) < LE32TOH((ex).a_text) ? 0 : \
	LE32TOH((ex).a_entry) & ~__I386_LDPGSZ) : __I386_LDPGSZ)

/* Address of the bottom of the data segment. */
#define I386_N_DATADDR(ex) \
	I386_N_ALIGN(ex, I386_N_TXTADDR(ex) + LE32TOH((ex).a_text))

/* Text segment offset. */
#define	I386_N_TXTOFF(ex) \
	(I386_N_GETMAGIC(ex) == ZMAGIC ? __I386_LDPGSZ : \
	(I386_N_GETMAGIC(ex) == QMAGIC ? 0 : sizeof(struct exec)))

/* Data segment offset. */
#define	I386_N_DATOFF(ex) \
	I386_N_ALIGN(ex, I386_N_TXTOFF(ex) + LE32TOH((ex).a_text))

/* Relocation table offset. */
#define I386_N_RELOFF(ex) \
	I386_N_ALIGN(ex, I386_N_DATOFF(ex) + LE32TOH((ex).a_data))

/* Symbol table offset. */
#define I386_N_SYMOFF(ex) \
	(I386_N_RELOFF(ex) + LE32TOH((ex).a_trsize) + LE32TOH((ex).a_drsize))

/* String table offset. */
#define	I386_N_STROFF(ex) 	(I386_N_SYMOFF(ex) + LE32TOH((ex).a_syms))

/*
 * Header prepended to each a.out file.
 * only manipulate the a_midmag field via the
 * N_SETMAGIC/N_GET{MAGIC,MID,FLAG} macros in a.out.h
 */

struct i386_exec {
     uint32_t	a_midmag;	/* flags<<26 | mid<<16 | magic */
     uint32_t	a_text;		/* text segment size */
     uint32_t	a_data;		/* initialized data size */
     uint32_t	a_bss;		/* uninitialized data size */
     uint32_t	a_syms;		/* symbol table size */
     uint32_t	a_entry;	/* entry point */
     uint32_t	a_trsize;	/* text relocation size */
     uint32_t	a_drsize;	/* data relocation size */
};
#define a_magic a_midmag /* XXX Hack to work with current kern_execve.c */

/* a_magic */
#define	OMAGIC		0407	/* old impure format */
#define	NMAGIC		0410	/* read-only text */
#define	ZMAGIC		0413	/* demand load format */
#define QMAGIC          0314    /* "compact" demand load format */

/* a_mid */
#define	MID_ZERO	0	/* unknown - implementation dependent */
#define	MID_SUN010	1	/* sun 68010/68020 binary */
#define	MID_SUN020	2	/* sun 68020-only binary */
#define MID_I386	134	/* i386 BSD binary */
#define MID_SPARC	138	/* sparc */
#define	MID_HP200	200	/* hp200 (68010) BSD binary */
#define	MID_HP300	300	/* hp300 (68020+68881) BSD binary */
#define	MID_HPUX	0x20C	/* hp200/300 HP-UX binary */
#define	MID_HPUX800     0x20B   /* hp800 HP-UX binary */

/*
 * a_flags
 */
#define EX_PIC		0x10	/* contains position independent code */
#define EX_DYNAMIC	0x20	/* contains run-time link-edit info */
#define EX_DPMASK	0x30	/* mask for the above */

#endif /* !_I386_AOUT_H_ */
