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

#ifdef LOCORE

#define PTE_P		(1<<0)
#define PTE_MA_WB	(0<<2)
#define PTE_MA_UC	(4<<2)
#define PTE_MA_UCE	(5<<2)
#define PTE_MA_WC	(6<<2)
#define PTE_MA_NATPAGE	(7<<2)
#define PTE_A		(1<<5)
#define PTE_D		(1<<6)
#define PTE_PL_KERN	(0<<7)
#define PTE_PL_USER	(3<<7)
#define PTE_AR_R	(0<<9)
#define PTE_AR_RX	(1<<9)
#define PTE_AR_RW	(2<<9)
#define PTE_AR_RWX	(3<<9)
#define PTE_AR_R_RW	(4<<9)
#define PTE_AR_RX_RWX	(5<<9)
#define PTE_AR_RWX_RW	(6<<9)
#define PTE_AR_X_RX	(7<<9)

#else

#define PTE_MA_WB	0
#define PTE_MA_UC	4
#define PTE_MA_UCE	5
#define PTE_MA_WC	6
#define PTE_MA_NATPAGE	7

#define PTE_PL_KERN	0
#define PTE_PL_USER	3

#define PTE_AR_R	0
#define PTE_AR_RX	1
#define PTE_AR_RW	2
#define PTE_AR_RWX	3
#define PTE_AR_R_RW	4
#define PTE_AR_RX_RWX	5
#define PTE_AR_RWX_RW	6
#define PTE_AR_X_RX	7

#define PTE_IG_WIRED	1
#define PTE_IG_MANAGED	2

/*
 * A short-format VHPT entry. Also matches the TLB insertion format.
 */
struct ia64_pte {
	u_int64_t	pte_p	:1;	/* bits 0..0 */
	u_int64_t	pte_rv1	:1;	/* bits 1..1 */
	u_int64_t	pte_ma	:3;	/* bits 2..4 */
	u_int64_t	pte_a	:1;	/* bits 5..5 */
	u_int64_t	pte_d	:1;	/* bits 6..6 */
	u_int64_t	pte_pl	:2;	/* bits 7..8 */
	u_int64_t	pte_ar	:3;	/* bits 9..11 */
	u_int64_t	pte_ppn	:38;	/* bits 12..49 */
	u_int64_t	pte_rv2	:2;	/* bits 50..51 */
	u_int64_t	pte_ed	:1;	/* bits 52..52 */
	u_int64_t	pte_ig	:11;	/* bits 53..63 */
};

/*
 * A long-format VHPT entry.
 */
struct ia64_lpte {
	u_int64_t	pte_p	:1;	/* bits 0..0 */
	u_int64_t	pte_rv1	:1;	/* bits 1..1 */
	u_int64_t	pte_ma	:3;	/* bits 2..4 */
	u_int64_t	pte_a	:1;	/* bits 5..5 */
	u_int64_t	pte_d	:1;	/* bits 6..6 */
	u_int64_t	pte_pl	:2;	/* bits 7..8 */
	u_int64_t	pte_ar	:3;	/* bits 9..11 */
	u_int64_t	pte_ppn	:38;	/* bits 12..49 */
	u_int64_t	pte_rv2	:2;	/* bits 50..51 */
	u_int64_t	pte_ed	:1;	/* bits 52..52 */
	u_int64_t	pte_ig	:11;	/* bits 53..63 */

	u_int64_t	pte_rv3	:2;	/* bits 0..1 */
	u_int64_t	pte_ps	:6;	/* bits 2..7 */
	u_int64_t	pte_key	:24;	/* bits 8..31 */
	u_int64_t	pte_rv4	:32;	/* bits 32..63 */

	u_int64_t	pte_tag;	/* includes ti */

	u_int64_t	pte_chain;	/* pa of collision chain */
};

#include <sys/queue.h>

#ifdef _KERNEL

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
	int			pm_count;	/* reference count */
	int			pm_flags;	/* pmap flags */
	int			pm_active;	/* active flag */
	int			pm_asn;		/* address space number */
	u_int			pm_asngen;	/* generation number of pm_asn */
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
	struct ia64_lpte pv_pte;	/* pte for collision walker */
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
extern char *ptvmmap;		/* poor name! */
extern vm_offset_t virtual_avail;
extern vm_offset_t virtual_end;

vm_offset_t pmap_steal_memory __P((vm_size_t));
void	pmap_bootstrap __P((void));
void	pmap_setdevram __P((unsigned long long basea, vm_offset_t sizea));
int	pmap_uses_prom_console __P((void));
pmap_t	pmap_kernel __P((void));
void	*pmap_mapdev __P((vm_offset_t, vm_size_t));
unsigned *pmap_pte __P((pmap_t, vm_offset_t)) __pure2;
vm_page_t pmap_use_pt __P((pmap_t, vm_offset_t));
void	pmap_set_opt	__P((unsigned *));
void	pmap_set_opt_bsp	__P((void));
void	pmap_deactivate __P((struct proc *p));
void	pmap_emulate_reference __P((struct proc *p, vm_offset_t v, int user, int write));

#endif /* _KERNEL */

#endif /* !LOCORE */

#endif /* !_MACHINE_PMAP_H_ */
