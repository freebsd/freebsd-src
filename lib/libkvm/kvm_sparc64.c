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

#include <machine/tte.h>
#include <machine/tsb.h>

#include <limits.h>

#include "kvm_private.h"

#ifndef btop
#define	btop(x)		(sparc64_btop(x))
#define	ptob(x)		(sparc64_ptob(x))
#endif

struct vmstate {
	vm_offset_t	vm_tsb;
};

void
_kvm_freevtop(kvm_t *kd)
{
	if (kd->vmst != 0) {
		free(kd->vmst);
	}
}

int
_kvm_initvtop(kvm_t *kd)
{
	struct nlist nlist[2];
	struct vmstate *vm;
	u_long pa;

	vm = (struct vmstate *)_kvm_malloc(kd, sizeof(*vm));
	if (vm == NULL) {
		_kvm_err(kd, kd->program, "cannot allocate vm");
		return (-1);
	}
	kd->vmst = vm;
	vm->vm_tsb = 0;

	nlist[0].n_name = "tsb_kernel_phys";
	nlist[1].n_name = 0;

	if (kvm_nlist(kd, nlist) != 0) {
		_kvm_err(kd, kd->program, "bad namelist");
		return (-1);
	}
	if (kvm_read(kd, nlist[0].n_value, &pa, sizeof(pa)) != sizeof(pa)) {
		_kvm_err(kd, kd->program, "cannot read tsb_kernel_phys");
		return (-1);
	}
	vm->vm_tsb = pa;
	return (0);
}

int
_kvm_kvatop(kvm_t *kd, u_long va, u_long *pa)
{
	struct vmstate *vm;
	struct tte tte;
	u_long offset;
	u_long tte_pa;
	u_long vpn;

	vpn = btop(va);
	offset = va & PAGE_MASK;
	tte_pa = kd->vmst->vm_tsb + ((vpn & TSB_KERNEL_MASK) << TTE_SHIFT);

	/* XXX This has to be a physical address read, kvm_read is virtual */
	if (lseek(kd->pmfd, tte_pa, 0) == -1) {
		_kvm_syserr(kd, kd->program, "_kvm_vatop: lseek");
		goto invalid;
	}
	if (read(kd->pmfd, &tte, sizeof(tte)) != sizeof(tte)) {
		_kvm_syserr(kd, kd->program, "_kvm_vatop: read");
		goto invalid;
	}
	if (!tte_match(tte, va))
		goto invalid;

	*pa = TD_PA(tte.tte_data) + offset;
	return (PAGE_SIZE - offset);

invalid:
	_kvm_err(kd, 0, "invalid address (%x)", va);
	return (0);
}
