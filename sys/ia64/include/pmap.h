/*
 * Copyright (c) 1991 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and William Jolitz of UUNET Technologies Inc.
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
 * Derived from hp300 version by Mike Hibler, this version by William
 * Jolitz uses a recursive map [a pde points to the page directory] to
 * map the page tables using the pagetables themselves. This is done to
 * reduce the impact on kernel virtual memory for lots of sparse address
 * space, and to reduce the cost of memory to each process.
 *
 *	from: hp300: @(#)pmap.h	7.2 (Berkeley) 12/16/90
 *	from: @(#)pmap.h	7.4 (Berkeley) 5/12/91
 *	from: i386 pmap.h,v 1.54 1997/11/20 19:30:35 bde Exp
 * $FreeBSD$
 */

#ifndef _MACHINE_PMAP_H_
#define	_MACHINE_PMAP_H_

#include <sys/queue.h>
#include <machine/pte.h>

#ifdef _KERNEL

#ifndef NKPT
#define	NKPT		30	/* initial number of kernel page tables */
#endif
#define MAXKPT		(PAGE_SIZE/sizeof(vm_offset_t))


/*
 *	Routine:	pmap_kextract
 *	Function:
 *		Extract the physical page address associated
 *		kernel virtual address.
 */
static __inline vm_offset_t
pmap_kextract(vm_offset_t va)
{
	return ia64_tpa(va);
}

#define	vtophys(va)	pmap_kextract(((vm_offset_t) (va)))

#endif /* _KERNEL */

/*
 * Pmap stuff
 */
struct	pv_entry;

struct md_page {
	int			pv_list_count;
	TAILQ_HEAD(,pv_entry)	pv_list;
};

struct pmap {
	TAILQ_HEAD(,pv_entry)	pm_pvlist;	/* list of mappings in pmap */
	u_int32_t		pm_rid[5];	/* base RID for pmap */
	int			pm_count;	/* reference count */
	int			pm_flags;	/* pmap flags */
	int			pm_active;	/* active flag */
	struct pmap_statistics	pm_stats;	/* pmap statistics */
	struct vm_page		*pm_ptphint;	/* pmap ptp hint */
};

#define pmap_resident_count(pmap) (pmap)->pm_stats.resident_count

#define PM_FLAG_LOCKED	0x1
#define PM_FLAG_WANTED	0x2

typedef struct pmap	*pmap_t;

#ifdef _KERNEL
extern pmap_t		kernel_pmap;
#endif

/*
 * For each vm_page_t, there is a list of all currently valid virtual
 * mappings of that page.  An entry is a pv_entry_t, the list is pv_table.
 */
typedef struct pv_entry {
	pmap_t		pv_pmap;	/* pmap where mapping lies */
	vm_offset_t	pv_va;		/* virtual address for mapping */
	TAILQ_ENTRY(pv_entry)	pv_list;
	TAILQ_ENTRY(pv_entry)	pv_plist;
} *pv_entry_t;

#define	PV_ENTRY_NULL	((pv_entry_t) 0)

#ifdef	_KERNEL

extern vm_offset_t avail_end;
extern vm_offset_t avail_start;
extern vm_offset_t clean_eva;
extern vm_offset_t clean_sva;
extern vm_offset_t phys_avail[];
extern vm_offset_t virtual_avail;
extern vm_offset_t virtual_end;

vm_offset_t pmap_steal_memory __P((vm_size_t));
void	pmap_bootstrap __P((void));
void	pmap_setdevram __P((unsigned long long basea, vm_offset_t sizea));
int	pmap_uses_prom_console __P((void));
pmap_t	pmap_kernel __P((void));
void	*pmap_mapdev __P((vm_offset_t, vm_size_t));
void	pmap_unmapdev __P((vm_offset_t, vm_size_t));
unsigned *pmap_pte __P((pmap_t, vm_offset_t)) __pure2;
vm_page_t pmap_use_pt __P((pmap_t, vm_offset_t));
void	pmap_set_opt	__P((unsigned *));
void	pmap_set_opt_bsp	__P((void));
struct pmap *pmap_install __P((struct pmap *pmap));

#endif /* _KERNEL */

#endif /* !_MACHINE_PMAP_H_ */
