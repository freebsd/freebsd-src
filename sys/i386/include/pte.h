/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	from: @(#)pte.h	5.5 (Berkeley) 5/9/91
 *	$Id: pte.h,v 1.2 1993/10/16 14:39:27 rgrimes Exp $
 */

/*
 * 386 page table entry and page table directory
 * W.Jolitz, 8/89
 *
 * There are two major kinds of pte's: those which have ever existed (and are
 * thus either now in core or on the swap device), and those which have
 * never existed, but which will be filled on demand at first reference.
 * There is a structure describing each.  There is also an ancillary
 * structure used in page clustering.
 */

#ifndef LOCORE
struct pde
{
unsigned int	
		pd_v:1,			/* valid bit */
		pd_prot:2,		/* access control */
		pd_mbz1:2,		/* reserved, must be zero */
		pd_u:1,			/* hardware maintained 'used' bit */
		:1,			/* not used */
		pd_mbz2:2,		/* reserved, must be zero */
		:3,			/* reserved for software */
		pd_pfnum:20;		/* physical page frame number of pte's*/
};
struct pte
{
unsigned int	
		pg_v:1,			/* valid bit */
		pg_prot:2,		/* access control */
		pg_mbz1:2,		/* reserved, must be zero */
		pg_u:1,			/* hardware maintained 'used' bit */
		pg_m:1,			/* hardware maintained modified bit */
		pg_mbz2:2,		/* reserved, must be zero */
		pg_fod:1,		/* is fill on demand (=0) */
		:1,			/* must write back to swap (unused) */
		pg_nc:1,		/* 'uncacheable page' bit */
		pg_pfnum:20;		/* physical page frame number */
};
struct hpte
{
unsigned int	
		pg_high:12,		/* special for clustering */
		pg_pfnum:20;
};
struct fpte
{
unsigned int	
		pg_v:1,			/* valid bit */
		pg_prot:2,		/* access control */
		:5,
		pg_fileno:1,		/* file mapped from or TEXT or ZERO */
		pg_fod:1,		/* is fill on demand (=1) */
		pg_blkno:22;		/* file system block number */
};
#endif

#define	PD_MASK		0xffc00000	/* page directory address bits */
#define	PD_SHIFT	22		/* page directory address bits */

#define	PG_V		0x00000001
#define	PG_PROT		0x00000006 /* all protection bits . */
#define	PG_FOD		0x00000200
#define	PG_SWAPM	0x00000400
#define PG_N		0x00000800 /* Non-cacheable */
#define	PG_M		0x00000040
#define PG_U		0x00000020 /* not currently used */
#define	PG_FRAME	0xfffff000

#define	PG_FZERO	0
#define	PG_FTEXT	1
#define	PG_FMAX		(PG_FTEXT)

#define	PG_NOACC	0
#define	PG_KR		0x00000000
#define	PG_KW		0x00000002
#define	PG_URKR		0x00000004
#define	PG_URKW		0x00000004
#define	PG_UW		0x00000006

/*
 * Page Protection Exception bits
 */

#define PGEX_P		0x01	/* Protection violation vs. not present */
#define PGEX_W		0x02	/* during a Write cycle */
#define PGEX_U		0x04	/* access from User mode (UPL) */

/*
 * Pte related macros
 */
#define	dirty(pte)	((pte)->pg_m)

#ifndef LOCORE
#ifdef KERNEL
/* utilities defined in pmap.c */
extern	struct pte *Sysmap;
#endif
#endif
