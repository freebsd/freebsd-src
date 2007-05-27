/*-
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$NetBSD: vmparam.h,v 1.11 2000/02/11 19:25:16 thorpej Exp $
 * $FreeBSD$
 */

#ifndef _MACHINE_VMPARAM_H_
#define	_MACHINE_VMPARAM_H_

#define	USRSTACK	VM_MAXUSER_ADDRESS

#ifndef	MAXTSIZ
#define	MAXTSIZ		(16*1024*1024)		/* max text size */
#endif

#ifndef	DFLDSIZ
#define	DFLDSIZ		(32*1024*1024)		/* default data size */
#endif

#ifndef	MAXDSIZ
#define	MAXDSIZ		(512*1024*1024)		/* max data size */
#endif

#ifndef	DFLSSIZ
#define	DFLSSIZ		(1*1024*1024)		/* default stack size */
#endif

#ifndef	MAXSSIZ
#define	MAXSSIZ		(32*1024*1024)		/* max stack size */
#endif

/*
 * Size of shared memory map
 */
#ifndef	SHMMAXPGS
#define	SHMMAXPGS	1024
#endif

/*
 * The time for a process to be blocked before being very swappable.
 * This is a number of seconds which the system takes as being a non-trivial
 * amount of real time.  You probably shouldn't change this;
 * it is used in subtle ways (fractions and multiples of it are, that is, like
 * half of a ``long time'', almost a long time, etc.)
 * It is related to human patience and other factors which don't really
 * change over time.
 */
#define	MAXSLP 		20

/*
 * Use the direct-mapped BAT registers for UMA small allocs. This
 * takes pressure off the small amount of available KVA.
 */
#define UMA_MD_SMALL_ALLOC

/*
 * Would like to have MAX addresses = 0, but this doesn't (currently) work
 */
#define	VM_MIN_ADDRESS		((vm_offset_t)0)
#define	VM_MAXUSER_ADDRESS	((vm_offset_t)0x7ffff000)
#define	VM_MAX_ADDRESS		VM_MAXUSER_ADDRESS
#define	VM_MIN_KERNEL_ADDRESS	((vm_offset_t)(KERNEL_SR << ADDR_SR_SHFT))
#define	VM_MAX_KERNEL_ADDRESS	(VM_MIN_KERNEL_ADDRESS + 2*SEGMENT_LENGTH - 1)

#define	KERNBASE	0x100000	/* start of kernel virtual */

/* XXX max. amount of KVM to be used by buffers. */
#ifndef VM_MAX_KERNEL_BUF
#define	VM_MAX_KERNEL_BUF	(SEGMENT_LENGTH * 7 / 10)
#endif

struct pmap_physseg {
	struct pv_entry *pvent;
	char *attrs;
};

#define	VM_PHYSSEG_MAX		16	/* 1? */
#define	VM_PHYSSEG_STRAT	VM_PSTRAT_BSEARCH
#define	VM_PHYSSEG_NOADD		/* can't add RAM after vm_mem_init */

/*
 * The physical address space is densely populated.
 */
#define	VM_PHYSSEG_DENSE

#define	VM_NFREELIST		1
#define	VM_FREELIST_DEFAULT	0

#ifndef VM_INITIAL_PAGEIN
#define	VM_INITIAL_PAGEIN	16
#endif

#ifndef SGROWSIZ
#define	SGROWSIZ	(128UL*1024)		/* amount to grow stack */
#endif

#ifndef VM_KMEM_SIZE
#define	VM_KMEM_SIZE		(12 * 1024 * 1024)
#endif

#endif /* _MACHINE_VMPARAM_H_ */
