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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
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
	u_int64_t       lev1map_pa;             /* PA of Lev1map */
        u_int64_t       page_size;              /* Page size */
        u_int64_t       nmemsegs;               /* Number of RAM segm */
};

void
_kvm_freevtop(kd)
	kvm_t *kd;
{

	/* Not actually used for anything right now, but safe. */
	if (kd->vmst != 0)
		free(kd->vmst);
}

int
_kvm_initvtop(kd)
	kvm_t *kd;
{
	struct vmstate *vm;
	struct nlist nlist[2];
	u_long pa;

	vm = (struct vmstate *)_kvm_malloc(kd, sizeof(*vm));
	if (vm == 0) {
		_kvm_err(kd, kd->program, "cannot allocate vm");
		return (-1);
	}
	kd->vmst = vm;
	vm->page_size = ALPHA_PGBYTES;

	nlist[0].n_name = "_Lev1map";
	nlist[1].n_name = 0;

	if (kvm_nlist(kd, nlist) != 0) {
		_kvm_err(kd, kd->program, "bad namelist");
		return (-1);
	}

	if(!ISALIVE(kd)) {
		if (kvm_read(kd, (nlist[0].n_value), &pa, sizeof(pa)) != sizeof(pa)) {
			_kvm_err(kd, kd->program, "cannot read Lev1map");
			return (-1);
		}
	} else 
		if (kvm_read(kd, (nlist[0].n_value), &pa, sizeof(pa)) != sizeof(pa)) {
			_kvm_err(kd, kd->program, "cannot read Lev1map");
			return (-1);
		}
	vm->lev1map_pa = pa;
	return (0);

}

int
_kvm_kvatop(kd, va, pa)
	kvm_t *kd;
	u_long va;
	u_long *pa;
{
	u_int64_t       lev1map_pa;             /* PA of Lev1map */
        u_int64_t       page_size;
	int rv, page_off;
	alpha_pt_entry_t pte;
	off_t pteoff;
	struct vmstate *vm;
	vm = kd->vmst ;
	

        if (ISALIVE(kd)) {
                _kvm_err(kd, 0, "vatop called in live kernel!");
                return(0);
        }
	lev1map_pa = vm->lev1map_pa;
	page_size  = vm->page_size;

	page_off = va & (page_size - 1);
	if (va >= ALPHA_K0SEG_BASE && va <= ALPHA_K0SEG_END) {
		/*
		 * Direct-mapped address: just convert it.
		 */

		*pa = ALPHA_K0SEG_TO_PHYS(va);
		rv = page_size - page_off;
	} else if (va >= ALPHA_K1SEG_BASE && va <= ALPHA_K1SEG_END) {
		/*
		 * Real kernel virtual address: do the translation.
		 */
#define PTMASK			((1 << ALPHA_PTSHIFT) - 1)
#define pmap_lev1_index(va)	(((va) >> ALPHA_L1SHIFT) & PTMASK)
#define pmap_lev2_index(va)	(((va) >> ALPHA_L2SHIFT) & PTMASK)
#define pmap_lev3_index(va)	(((va) >> ALPHA_L3SHIFT) & PTMASK)

		/* Find and read the L1 PTE. */
		pteoff = lev1map_pa +
			pmap_lev1_index(va)  * sizeof(alpha_pt_entry_t);
		if (lseek(kd->pmfd, _kvm_pa2off(kd, pteoff), 0) == -1 ||
		    read(kd->pmfd, (char *)&pte, sizeof(pte)) != sizeof(pte)) {
			_kvm_syserr(kd, 0, "could not read L1 PTE");
			goto lose;
		}

		/* Find and read the L2 PTE. */
		if ((pte & ALPHA_PTE_VALID) == 0) {
			_kvm_err(kd, 0, "invalid translation (invalid L1 PTE)");
			goto lose;
		}
		pteoff = ALPHA_PTE_TO_PFN(pte) * page_size +
		    pmap_lev2_index(va) * sizeof(alpha_pt_entry_t);
		if (lseek(kd->pmfd, _kvm_pa2off(kd, pteoff), 0) == -1 ||
		    read(kd->pmfd, (char *)&pte, sizeof(pte)) != sizeof(pte)) {
			_kvm_syserr(kd, 0, "could not read L2 PTE");
			goto lose;
		}

		/* Find and read the L3 PTE. */
		if ((pte & ALPHA_PTE_VALID) == 0) {
			_kvm_err(kd, 0, "invalid translation (invalid L2 PTE)");
			goto lose;
		}
		pteoff = ALPHA_PTE_TO_PFN(pte) * page_size +
		    pmap_lev3_index(va) * sizeof(alpha_pt_entry_t);
		if (lseek(kd->pmfd, _kvm_pa2off(kd, pteoff), 0) == -1 ||
		    read(kd->pmfd, (char *)&pte, sizeof(pte)) != sizeof(pte)) {
			_kvm_syserr(kd, 0, "could not read L3 PTE");
			goto lose;
		}

		/* Fill in the PA. */
		if ((pte & ALPHA_PTE_VALID) == 0) {
			_kvm_err(kd, 0, "invalid translation (invalid L3 PTE)");
			goto lose;
		}
		*pa = ALPHA_PTE_TO_PFN(pte) * page_size + page_off;
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
	return ALPHA_K0SEG_TO_PHYS(pa);
}

