/*-
 * Copyright (c) 2001 David E. O'Brien
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
 *	from: @(#)param.h	5.8 (Berkeley) 6/28/91
 * $FreeBSD$
 */

/*
 * Machine dependent constants for PowerPC (32-bit only currently)
 */

/*
 * Round p (pointer or byte index) up to a correctly-aligned value
 * for all data types (int, long, ...).   The result is unsigned int
 * and must be cast to any desired pointer type.
 */
#ifndef _ALIGNBYTES
#define	_ALIGNBYTES	(sizeof(int) - 1)
#endif
#ifndef _ALIGN
#define	_ALIGN(p)	(((unsigned)(p) + _ALIGNBYTES) & ~_ALIGNBYTES)
#endif

#ifndef _NO_NAMESPACE_POLLUTION

#ifndef _MACHINE_PARAM_H_
#define	_MACHINE_PARAM_H_

#ifndef MACHINE
#define	MACHINE		"powerpc"
#endif
#ifndef MACHINE_ARCH
#define	MACHINE_ARCH	"powerpc"
#endif
#define	MID_MACHINE	MID_POWERPC

#if defined(SMP) || defined(KLD_MODULE)
#define MAXCPU		2
#else
#define	MAXCPU		1
#endif /* SMP */

#define	ALIGNBYTES	_ALIGNBYTES
#define	ALIGN(p)	_ALIGN(p)
/*
 * ALIGNED_POINTER is a boolean macro that checks whether an address
 * is valid to fetch data elements of type t from on this architecture.
 * This does not reflect the optimal alignment, just the possibility
 * (within reasonable limits). 
 */
#define	ALIGNED_POINTER(p, t)	((((unsigned)(p)) & (sizeof (t) - 1)) == 0)

#define	PAGE_SHIFT	12
#define	PAGE_SIZE	(1 << PAGE_SHIFT)	/* Page size */
#define	PAGE_MASK	(PAGE_SIZE - 1)
#define	NPTEPG		(PAGE_SIZE/(sizeof (pt_entry_t)))

#define	MAXPAGESIZES	1		/* maximum number of supported page sizes */

#ifndef KSTACK_PAGES
#define	KSTACK_PAGES		4		/* includes pcb */
#endif
#define	KSTACK_GUARD_PAGES	1	/* pages of kstack guard; 0 disables */
#define	USPACE		(KSTACK_PAGES * PAGE_SIZE)	/* total size of pcb */

/*
 * Mach derived conversion macros
 */
#define	trunc_page(x)		((unsigned long)(x) & ~(PAGE_MASK))
#define	round_page(x)		(((x) + PAGE_MASK) & ~PAGE_MASK)
#define	trunc_4mpage(x)		((unsigned)(x) & ~PDRMASK)
#define	round_4mpage(x)		((((unsigned)(x)) + PDRMASK) & ~PDRMASK)

#define	atop(x)			((unsigned long)(x) >> PAGE_SHIFT)
#define	ptoa(x)			((unsigned long)(x) << PAGE_SHIFT)

#define	powerpc_btop(x)		((unsigned)(x) >> PAGE_SHIFT)
#define	powerpc_ptob(x)		((unsigned)(x) << PAGE_SHIFT)

#define	pgtok(x)		((x) * (PAGE_SIZE / 1024))

#endif /* !_MACHINE_PARAM_H_ */
#endif /* !_NO_NAMESPACE_POLLUTION */
