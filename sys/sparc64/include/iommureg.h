/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	from: @(#)sbusreg.h	8.1 (Berkeley) 6/11/93
 *	from: NetBSD: iommureg.h,v 1.6 2001/07/20 00:07:13 eeh Exp
 *
 * $FreeBSD$
 */

#ifndef _MACHINE_IOMMUREG_H_
#define _MACHINE_IOMMUREG_H_

/*
 * UltraSPARC IOMMU registers, common to both the sbus and PCI
 * controllers.
 */

/* iommmu registers */
struct iommureg {
	u_int64_t	iommu_cr;	/* IOMMU control register */
	u_int64_t	iommu_tsb;	/* IOMMU TSB base register */
	u_int64_t	iommu_flush;	/* IOMMU flush register */
};

/* streaming buffer registers */
struct iommu_strbuf {
	u_int64_t	strbuf_ctl;	/* streaming buffer control reg */
	u_int64_t	strbuf_pgflush;	/* streaming buffer page flush */
	u_int64_t	strbuf_flushsync;/* streaming buffer flush sync */
};

/* streaming buffer control register */
#define STRBUF_EN		0x0000000000000001UL
#define STRBUF_D		0x0000000000000002UL

#define	IOMMU_BITS		34
#define	IOMMU_MAXADDR		(1UL << IOMMU_BITS)

/*
 * control register bits
 */
/* Nummber of entries in IOTSB */
#define IOMMUCR_TSB1K		0x0000000000000000UL
#define IOMMUCR_TSB2K		0x0000000000010000UL
#define IOMMUCR_TSB4K		0x0000000000020000UL
#define IOMMUCR_TSB8K		0x0000000000030000UL
#define IOMMUCR_TSB16K		0x0000000000040000UL
#define IOMMUCR_TSB32K		0x0000000000050000UL
#define IOMMUCR_TSB64K		0x0000000000060000UL
#define IOMMUCR_TSB128K		0x0000000000070000UL
/* Mask for above */
#define IOMMUCR_TSBMASK		0xfffffffffff8ffffUL
/* 8K iommu page size */
#define IOMMUCR_8KPG		0x0000000000000000UL
/* 64K iommu page size */
#define IOMMUCR_64KPG		0x0000000000000004UL
/* Diag enable */
#define IOMMUCR_DE		0x0000000000000002UL
/* Enable IOMMU */
#define IOMMUCR_EN		0x0000000000000001UL

/*
 * IOMMU stuff
 */
/* Entry valid */
#define	IOTTE_V			0x8000000000000000UL
/* 8K or 64K page? */
#define IOTTE_64K		0x2000000000000000UL
#define IOTTE_8K		0x0000000000000000UL
/* Is page streamable? */
#define IOTTE_STREAM		0x1000000000000000UL
/* Accesses to same bus segment? */
#define	IOTTE_LOCAL		0x0800000000000000UL
/* Let's assume this is correct */
#define IOTTE_PAMASK		0x000001ffffffe000UL
/* Accesses to cacheable space */
#define IOTTE_C			0x0000000000000010UL
/* Writeable */
#define IOTTE_W			0x0000000000000002UL

/*
 * On sun4u each bus controller has a separate IOMMU.  The IOMMU has 
 * a TSB which must be page aligned and physically contiguous.  Mappings
 * can be of 8K IOMMU pages or 64K IOMMU pages.  We use 8K for compatibility
 * with the CPU's MMU.
 *
 * On sysio, psycho, and psycho+, IOMMU TSBs using 8K pages can map the
 * following size segments:
 *
 *	VA size		VA base		TSB size	tsbsize
 *	--------	--------	---------	-------
 *	8MB		ff800000	8K		0
 *	16MB		ff000000	16K		1
 *	32MB		fe000000	32K		2
 *	64MB		fc000000	64K		3
 *	128MB		f8000000	128K		4
 *	256MB		f0000000	256K		5
 *	512MB		e0000000	512K		6
 *	1GB		c0000000	1MB		7
 *
 * Unfortunately, sabres on UltraSPARC IIi and IIe processors does not use
 * this scheme to determine the IOVA base address.  Instead, bits 31-29 are
 * used to check against the Target Address Space register in the IIi and
 * the the IOMMU is used if they hit.  God knows what goes on in the IIe.
 *
 */

#define IOTSB_VEND		(~PAGE_MASK)
#define IOTSB_VSTART(sz)	(u_int)(IOTSB_VEND << ((sz) + 10)) 

#define MAKEIOTTE(pa,w,c,s)						\
	(((pa) & IOTTE_PAMASK) | ((w) ? IOTTE_W : 0) |			\
	((c) ? IOTTE_C : 0) | ((s) ? IOTTE_STREAM : 0) |		\
	(IOTTE_V | IOTTE_8K))
#define IOTSBSLOT(va,sz)						\
	((u_int)(((vm_offset_t)(va)) - (is->is_dvmabase)) >> PAGE_SHIFT)

#endif /* !_MACHINE_IOMMUREG_H_ */
