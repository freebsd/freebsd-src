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
	vm->page_size = PAGE_SIZE;

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
	pv_entry_t pte;
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
		goto lose;
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

