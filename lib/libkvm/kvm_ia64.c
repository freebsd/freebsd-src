/* $FreeBSD$ */
/*	$NetBSD: kvm_alpha.c,v 1.7.2.1 1997/11/02 20:34:26 mellon Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <nlist.h>
#include <kvm.h>

#include <vm/vm.h>
#include <vm/vm_param.h>

#include <limits.h>
#include <stdlib.h>
#include <machine/pmap.h>
#include "kvm_private.h"

static off_t   _kvm_pa2off(kvm_t *kd, u_long pa);

struct vmstate {
	u_int64_t       kptdir;		/* PA of page table directory */
        u_int64_t	page_size;	/* Page size */
};

void
_kvm_freevtop(kvm_t *kd)
{

	/* Not actually used for anything right now, but safe. */
	if (kd->vmst != 0)
		free(kd->vmst);
}

int
_kvm_initvtop(kvm_t *kd)
{
	struct vmstate *vm;
	struct nlist nlist[2];
	u_int64_t va;

	vm = (struct vmstate *)_kvm_malloc(kd, sizeof(*vm));
	if (vm == 0) {
		_kvm_err(kd, kd->program, "cannot allocate vm");
		return (-1);
	}
	kd->vmst = vm;
	vm->page_size = getpagesize(); /* XXX wrong for crashdumps */

	nlist[0].n_name = "kptdir";
	nlist[1].n_name = 0;

	if (kvm_nlist(kd, nlist) != 0) {
		_kvm_err(kd, kd->program, "bad namelist");
		return (-1);
	}

	if(!ISALIVE(kd)) {
		if (kvm_read(kd, (nlist[0].n_value), &va, sizeof(va)) != sizeof(va)) {
			_kvm_err(kd, kd->program, "cannot read kptdir");
			return (-1);
		}
	} else 
		if (kvm_read(kd, (nlist[0].n_value), &va, sizeof(va)) != sizeof(va)) {
			_kvm_err(kd, kd->program, "cannot read kptdir");
			return (-1);
		}
	vm->kptdir = IA64_RR_MASK(va);
	return (0);

}

int
_kvm_kvatop(kvm_t *kd, u_long va, u_long *pa)
{
	u_int64_t	kptdir;			/* PA of kptdir */
        u_int64_t	page_size;
	int		rv, page_off;
	struct ia64_lpte pte;
	off_t		pteoff;
	struct vmstate	*vm;

	vm = kd->vmst;
	
        if (ISALIVE(kd)) {
                _kvm_err(kd, 0, "vatop called in live kernel!");
                return(0);
        }
	kptdir = vm->kptdir;
	page_size = vm->page_size;

	page_off = va & (page_size - 1);
	if (va >= IA64_RR_BASE(6) && va <= IA64_RR_BASE(7) + ((1L<<61)-1)) {
		/*
		 * Direct-mapped address: just convert it.
		 */

		*pa = IA64_RR_MASK(va);
		rv = page_size - page_off;
	} else if (va >= IA64_RR_BASE(5) && va < IA64_RR_BASE(6)) {
		/*
		 * Real kernel virtual address: do the translation.
		 */
#define KPTE_DIR_INDEX(va, ps) \
	(IA64_RR_MASK(va) / ((ps) * (ps) * sizeof(struct ia64_lpte)))
#define KPTE_PTE_INDEX(va, ps) \
	(((va) / (ps)) % (ps / sizeof(struct ia64_lpte)))

		int maxpt = page_size / sizeof(u_int64_t);
		int ptno = KPTE_DIR_INDEX(va, page_size);
		int pgno = KPTE_PTE_INDEX(va, page_size);
		u_int64_t ptoff, pgoff;

		if (ptno >= maxpt) {
			_kvm_err(kd, 0, "invalid translation (va too large)");
			goto lose;
		}
		ptoff = kptdir + ptno * sizeof(u_int64_t);
		if (lseek(kd->pmfd, _kvm_pa2off(kd, ptoff), 0) == -1 ||
		    read(kd->pmfd, &pgoff, sizeof(pgoff)) != sizeof(pgoff)) {
			_kvm_syserr(kd, 0, "could not read page table address");
			goto lose;
		}
		pgoff = IA64_RR_MASK(pgoff);
		if (!pgoff) {
			_kvm_err(kd, 0, "invalid translation (no page table)");
			goto lose;
		}
		if (lseek(kd->pmfd, _kvm_pa2off(kd, pgoff), 0) == -1 ||
		    read(kd->pmfd, &pte, sizeof(pte)) != sizeof(pte)) {
			_kvm_syserr(kd, 0, "could not read PTE");
			goto lose;
		}
		if (!pte.pte_p) {
			_kvm_err(kd, 0, "invalid translation (invalid PTE)");
			goto lose;
		}
		*pa = pte.pte_ppn << 12;
		rv = page_size - page_off;
	} else {
		/*
		 * Bogus address (not in KV space): punt.
		 */

		_kvm_err(kd, 0, "invalid kernel virtual address");
lose:
		*pa = -1;
		rv = 0;
	}

	return (rv);
}

/*
 * Translate a physical address to a file-offset in the crash-dump.
 */
off_t   
_kvm_pa2off(kd, pa)
	kvm_t *kd;
	u_long pa;
{
	return IA64_PHYS_TO_RR7(pa);
}

