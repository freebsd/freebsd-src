/*-
 * Copyright (c) 1989, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software developed by the Computer Systems
 * Engineering group at Lawrence Berkeley Laboratory under DARPA contract
 * BG 91-66 and contributed to Berkeley.
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
 *	from: FreeBSD: src/lib/libkvm/kvm_i386.c,v 1.15 2001/10/10 17:48:43
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)kvm_hp300.c	8.1 (Berkeley) 6/4/93";
#endif
#endif /* LIBC_SCCS and not lint */

/*
 * sparc64 machine dependent routines for kvm.
 */

#include <sys/param.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <nlist.h>
#include <kvm.h>

#include <vm/vm.h>
#include <vm/vm_param.h>

#include <machine/kerneldump.h>
#include <machine/tte.h>
#include <machine/tlb.h>
#include <machine/tsb.h>

#include <limits.h>

#include "kvm_private.h"

#ifndef btop
#define	btop(x)		(sparc64_btop(x))
#define	ptob(x)		(sparc64_ptob(x))
#endif

struct vmstate {
	off_t		vm_tsb_off;
	vm_size_t	vm_tsb_mask;
	int		vm_nregions;
	struct sparc64_dump_reg	*vm_regions;
};

void
_kvm_freevtop(kvm_t *kd)
{
	if (kd->vmst != 0) {
		free(kd->vmst->vm_regions);
		free(kd->vmst);
	}
}

static int
_kvm_read_phys(kvm_t *kd, off_t pos, void *buf, size_t size)
{

	/* XXX This has to be a raw file read, kvm_read is virtual. */
	if (lseek(kd->pmfd, pos, SEEK_SET) == -1) {
		_kvm_syserr(kd, kd->program, "_kvm_read_phys: lseek");
		return (0);
	}
	if (read(kd->pmfd, buf, size) != size) {
		_kvm_syserr(kd, kd->program, "_kvm_read_phys: read");
		return (0);
	}
	return (1);
}

static int
_kvm_reg_cmp(const void *a, const void *b)
{
	const struct sparc64_dump_reg *ra, *rb;

	ra = a;
	rb = b;
	if (ra->dr_pa < rb->dr_pa)
		return (-1);
	else if (ra->dr_pa >= rb->dr_pa + rb->dr_size)
		return (1);
	else
		return (0);
}

#define	KVM_OFF_NOTFOUND	0

static off_t
_kvm_find_off(struct vmstate *vm, vm_offset_t pa, vm_size_t size)
{
	struct sparc64_dump_reg *reg, key;
	vm_offset_t o;

	key.dr_pa = pa;
	reg = bsearch(&key, vm->vm_regions, vm->vm_nregions,
	    sizeof(*vm->vm_regions), _kvm_reg_cmp);
	if (reg == NULL)
		return (KVM_OFF_NOTFOUND);
	o = pa - reg->dr_pa;
	if (o + size > reg->dr_size)
		return (KVM_OFF_NOTFOUND);
	return (reg->dr_offs + o);
}

int
_kvm_initvtop(kvm_t *kd)
{
	struct sparc64_dump_hdr hdr;
	struct sparc64_dump_reg	*regs;
	struct vmstate *vm;
	size_t regsz;
	vm_offset_t pa;
	vm_size_t mask;

	vm = (struct vmstate *)_kvm_malloc(kd, sizeof(*vm));
	if (vm == NULL) {
		_kvm_err(kd, kd->program, "cannot allocate vm");
		return (-1);
	}
	kd->vmst = vm;

	if (!_kvm_read_phys(kd, 0, &hdr, sizeof(hdr)))
		goto fail_vm;
	pa = hdr.dh_tsb_pa;

	regsz = hdr.dh_nregions * sizeof(*regs);
	regs = _kvm_malloc(kd, regsz);
	if (regs == NULL) {
		_kvm_err(kd, kd->program, "cannot allocate regions");
		goto fail_vm;
	}
	if (!_kvm_read_phys(kd, sizeof(hdr), regs, regsz))
		goto fail_regs;
	qsort(regs, hdr.dh_nregions, sizeof(*regs), _kvm_reg_cmp);

	vm->vm_tsb_mask = hdr.dh_tsb_mask;
	vm->vm_regions = regs;
	vm->vm_nregions = hdr.dh_nregions;
	vm->vm_tsb_off = _kvm_find_off(vm, hdr.dh_tsb_pa, hdr.dh_tsb_size);
	if (vm->vm_tsb_off == KVM_OFF_NOTFOUND) {
		_kvm_err(kd, kd->program, "tsb not found in dump");
		goto fail_regs;
	}
	return (0);

fail_regs:
	free(regs);
fail_vm:
	free(vm);
	return (-1);
}

int
_kvm_kvatop(kvm_t *kd, u_long va, off_t *pa)
{
	struct vmstate *vm;
	struct tte tte;
	off_t tte_off, pa_off;
	u_long pg_off, vpn;
	int rest;

	pg_off = va & PAGE_MASK;
	if (va >= VM_MIN_DIRECT_ADDRESS)
		pa_off = TLB_DIRECT_TO_PHYS(va) & ~PAGE_MASK;
	else {
		vpn = btop(va);
		tte_off = kd->vmst->vm_tsb_off +
		    ((vpn & kd->vmst->vm_tsb_mask) << TTE_SHIFT);
		if (!_kvm_read_phys(kd, tte_off, &tte, sizeof(tte)))
			goto invalid;
		if (!tte_match(&tte, va))
			goto invalid;
		pa_off = TTE_GET_PA(&tte);
	}
	rest = PAGE_SIZE - pg_off;
	pa_off = _kvm_find_off(kd->vmst, pa_off, rest);
	if (pa_off == KVM_OFF_NOTFOUND)
		goto invalid;
	*pa = pa_off + pg_off;
	return (rest);

invalid:
	_kvm_err(kd, 0, "invalid address (%x)", va);
	return (0);
}
