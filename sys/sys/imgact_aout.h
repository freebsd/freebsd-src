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
 * $FreeBSD$
 */

#ifndef	_IMGACT_AOUT_H_
#define	_IMGACT_AOUT_H_

#define N_GETMAGIC(ex) \
	( (ex).a_midmag & 0xffff )
#define N_GETMID(ex) \
	( (N_GETMAGIC_NET(ex) == ZMAGIC) ? N_GETMID_NET(ex) : \
	((ex).a_midmag >> 16) & 0x03ff )
#define N_GETFLAG(ex) \
	( (N_GETMAGIC_NET(ex) == ZMAGIC) ? N_GETFLAG_NET(ex) : \
	((ex).a_midmag >> 26) & 0x3f )
#define N_SETMAGIC(ex,mag,mid,flag) \
	( (ex).a_midmag = (((flag) & 0x3f) <<26) | (((mid) & 0x03ff) << 16) | \
	((mag) & 0xffff) )

#define N_GETMAGIC_NET(ex) \
	(ntohl((ex).a_midmag) & 0xffff)
#define N_GETMID_NET(ex) \
	((ntohl((ex).a_midmag) >> 16) & 0x03ff)
#define N_GETFLAG_NET(ex) \
	((ntohl((ex).a_midmag) >> 26) & 0x3f)
#define N_SETMAGIC_NET(ex,mag,mid,flag) \
	( (ex).a_midmag = htonl( (((flag)&0x3f)<<26) | (((mid)&0x03ff)<<16) \
	| (((mag)&0xffff)) ) )

#define N_ALIGN(ex,x) \
	(N_GETMAGIC(ex) == ZMAGIC || N_GETMAGIC(ex) == QMAGIC || \
	 N_GETMAGIC_NET(ex) == ZMAGIC || N_GETMAGIC_NET(ex) == QMAGIC ? \
	 ((x) + __LDPGSZ - 1) & ~(unsigned long)(__LDPGSZ - 1) : (x))

/* Valid magic number check. */
#define	N_BADMAG(ex) \
	(N_GETMAGIC(ex) != OMAGIC && N_GETMAGIC(ex) != NMAGIC && \
	 N_GETMAGIC(ex) != ZMAGIC && N_GETMAGIC(ex) != QMAGIC && \
	 N_GETMAGIC_NET(ex) != OMAGIC && N_GETMAGIC_NET(ex) != NMAGIC && \
	 N_GETMAGIC_NET(ex) != ZMAGIC && N_GETMAGIC_NET(ex) != QMAGIC)


/* Address of the bottom of the text segment. */
/*
 * This can not be done right.  Abuse a_entry in some cases to handle kernels.
 */
#define N_TXTADDR(ex) \
	((N_GETMAGIC(ex) == OMAGIC || N_GETMAGIC(ex) == NMAGIC || \
	N_GETMAGIC(ex) == ZMAGIC) ? \
	((ex).a_entry < (ex).a_text ? 0 : (ex).a_entry & ~__LDPGSZ) : __LDPGSZ)

/* Address of the bottom of the data segment. */
#define N_DATADDR(ex) \
	N_ALIGN(ex, N_TXTADDR(ex) + (ex).a_text)

/* Text segment offset. */
#define	N_TXTOFF(ex) \
	(N_GETMAGIC(ex) == ZMAGIC ? __LDPGSZ : (N_GETMAGIC(ex) == QMAGIC || \
	N_GETMAGIC_NET(ex) == ZMAGIC) ? 0 : sizeof(struct exec))

/* Data segment offset. */
#define	N_DATOFF(ex) \
	N_ALIGN(ex, N_TXTOFF(ex) + (ex).a_text)

/* Relocation table offset. */
#define N_RELOFF(ex) \
	N_ALIGN(ex, N_DATOFF(ex) + (ex).a_data)

/* Symbol table offset. */
#define N_SYMOFF(ex) \
	(N_RELOFF(ex) + (ex).a_trsize + (ex).a_drsize)

/* String table offset. */
#define	N_STROFF(ex) 	(N_SYMOFF(ex) + (ex).a_syms)

/*
 * Header prepended to each a.out file.
 * only manipulate the a_midmag field via the
 * N_SETMAGIC/N_GET{MAGIC,MID,FLAG} macros in a.out.h
 */

struct exec {
     unsigned long	a_midmag;	/* flags<<26 | mid<<16 | magic */
     unsigned long	a_text;		/* text segment size */
     unsigned long	a_data;		/* initialized data size */
     unsigned long	a_bss;		/* uninitialized data size */
     unsigned long	a_syms;		/* symbol table size */
     unsigned long	a_entry;	/* entry point */
     unsigned long	a_trsize;	/* text relocation size */
     unsigned long	a_drsize;	/* data relocation size */
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

#ifdef _KERNEL
struct thread;
struct vnode;

int	aout_coredump(struct thread *td, struct vnode *vp, off_t limit);
#endif

#endif /* !_IMGACT_AOUT_H_ */
