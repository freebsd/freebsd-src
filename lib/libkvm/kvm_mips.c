/*-
 * Copyright (c) 1989, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software developed by the Computer Systems
 * Engineering group at Lawrence Berkeley Laboratory under DARPA contract
 * BG 91-66 and contributed to Berkeley. Modified for MIPS by Ralph Campbell.
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
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)kvm_mips.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */
/*
 * MIPS machine dependent routines for kvm.  Hopefully, the forthcoming 
 * vm code will one day obsolete this module.
 */

#include <sys/param.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <unistd.h>
#include <nlist.h>
#include <kvm.h>

#include <vm/vm.h>
#include <vm/vm_param.h>

#include <limits.h>
#include <db.h>

#include "kvm_private.h"

#include <machine/machConst.h>
#include <machine/pte.h>
#include <machine/pmap.h>

struct vmstate {
	pt_entry_t	*Sysmap;
	u_int		Sysmapsize;
};

#define KREAD(kd, addr, p)\
	(kvm_read(kd, addr, (char *)(p), sizeof(*(p))) != sizeof(*(p)))

void
_kvm_freevtop(kd)
	kvm_t *kd;
{
	if (kd->vmst != 0)
		free(kd->vmst);
}

int
_kvm_initvtop(kd)
	kvm_t *kd;
{
	struct vmstate *vm;
	struct nlist nlist[3];

	vm = (struct vmstate *)_kvm_malloc(kd, sizeof(*vm));
	if (vm == 0)
		return (-1);
	kd->vmst = vm;

	nlist[0].n_name = "Sysmap";
	nlist[1].n_name = "Sysmapsize";
	nlist[2].n_name = 0;

	if (kvm_nlist(kd, nlist) != 0) {
		_kvm_err(kd, kd->program, "bad namelist");
		return (-1);
	}
	if (KREAD(kd, (u_long)nlist[0].n_value, &vm->Sysmap)) {
		_kvm_err(kd, kd->program, "cannot read Sysmap");
		return (-1);
	}
	if (KREAD(kd, (u_long)nlist[1].n_value, &vm->Sysmapsize)) {
		_kvm_err(kd, kd->program, "cannot read mmutype");
		return (-1);
	}
	return (0);
}

/*
 * Translate a kernel virtual address to a physical address.
 */
int
_kvm_kvatop(kd, va, pa)
	kvm_t *kd;
	u_long va;
	u_long *pa;
{
	register struct vmstate *vm;
	u_long pte, addr, offset;

	if (ISALIVE(kd)) {
		_kvm_err(kd, 0, "vatop called in live kernel!");
		return((off_t)0);
	}
	vm = kd->vmst;
	offset = va & PGOFSET;
	/*
	 * If we are initializing (kernel segment table pointer not yet set)
	 * then return pa == va to avoid infinite recursion.
	 */
	if (vm->Sysmap == 0) {
		*pa = va;
		return (NBPG - offset);
	}
	if (va < KERNBASE ||
	    va >= VM_MIN_KERNEL_ADDRESS + vm->Sysmapsize * NBPG)
		goto invalid;
	if (va < VM_MIN_KERNEL_ADDRESS) {
		*pa = MACH_CACHED_TO_PHYS(va);
		return (NBPG - offset);
	}
	addr = (u_long)(vm->Sysmap + ((va - VM_MIN_KERNEL_ADDRESS) >> PGSHIFT));
	/*
	 * Can't use KREAD to read kernel segment table entries.
	 * Fortunately it is 1-to-1 mapped so we don't have to. 
	 */
	if (lseek(kd->pmfd, (off_t)addr, 0) < 0 ||
	    read(kd->pmfd, (char *)&pte, sizeof(pte)) < 0)
		goto invalid;
	if (!(pte & PG_V))
		goto invalid;
	*pa = (pte & PG_FRAME) | offset;
	return (NBPG - offset);

invalid:
	_kvm_err(kd, 0, "invalid address (%x)", va);
	return (0);
}

/*
 * Translate a user virtual address to a physical address.
 */
int
_kvm_uvatop(kd, p, va, pa)
	kvm_t *kd;
	const struct proc *p;
	u_long va;
	u_long *pa;
{
	register struct vmspace *vms = p->p_vmspace;
	u_long kva, offset;

	if (va >= KERNBASE)
		goto invalid;

	/* read the address of the first level table */
	kva = (u_long)&vms->vm_pmap.pm_segtab;
	if (kvm_read(kd, kva, (char *)&kva, sizeof(kva)) != sizeof(kva))
		goto invalid;
	if (kva == 0)
		goto invalid;

	/* read the address of the second level table */
	kva += (va >> SEGSHIFT) * sizeof(caddr_t);
	if (kvm_read(kd, kva, (char *)&kva, sizeof(kva)) != sizeof(kva))
		goto invalid;
	if (kva == 0)
		goto invalid;

	/* read the pte from the second level table */
	kva += (va >> PGSHIFT) & (NPTEPG - 1);
	if (kvm_read(kd, kva, (char *)&kva, sizeof(kva)) != sizeof(kva))
		goto invalid;
	if (!(kva & PG_V))
		goto invalid;
	offset = va & PGOFSET;
	*pa = (kva & PG_FRAME) | offset;
	return (NBPG - offset);

invalid:
	_kvm_err(kd, 0, "invalid address (%x)", va);
	return (0);
}
