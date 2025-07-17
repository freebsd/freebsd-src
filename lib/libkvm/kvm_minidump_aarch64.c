/*-
 * Copyright (c) 2006 Peter Wemm
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * From: FreeBSD: src/lib/libkvm/kvm_minidump_amd64.c r261799
 */

#include <sys/cdefs.h>
/*
 * ARM64 (AArch64) machine dependent routines for kvm and minidumps.
 */

#include <sys/param.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vm/vm.h>
#include <kvm.h>

#include "../../sys/arm64/include/minidump.h"

#include <limits.h>

#include "kvm_private.h"
#include "kvm_aarch64.h"

#define	aarch64_round_page(x, size)	roundup2((kvaddr_t)(x), size)
#define	aarch64_trunc_page(x, size)	rounddown2((kvaddr_t)(x), size)

struct vmstate {
	struct minidumphdr hdr;
	size_t page_size;
	u_int l3_shift;
};

static aarch64_pte_t
_aarch64_pte_get(kvm_t *kd, u_long pteindex)
{
	aarch64_pte_t *pte = _kvm_pmap_get(kd, pteindex, sizeof(*pte));

	return le64toh(*pte);
}

static int
_aarch64_minidump_probe(kvm_t *kd)
{

	return (_kvm_probe_elf_kernel(kd, ELFCLASS64, EM_AARCH64) &&
	    _kvm_is_minidump(kd));
}

static void
_aarch64_minidump_freevtop(kvm_t *kd)
{
	struct vmstate *vm = kd->vmst;

	free(vm);
	kd->vmst = NULL;
}

static int
_aarch64_minidump_initvtop(kvm_t *kd)
{
	struct vmstate *vmst;
	off_t off, dump_avail_off, sparse_off;

	vmst = _kvm_malloc(kd, sizeof(*vmst));
	if (vmst == NULL) {
		_kvm_err(kd, kd->program, "cannot allocate vm");
		return (-1);
	}
	kd->vmst = vmst;
	if (pread(kd->pmfd, &vmst->hdr, sizeof(vmst->hdr), 0) !=
	    sizeof(vmst->hdr)) {
		_kvm_err(kd, kd->program, "cannot read dump header");
		return (-1);
	}
	if (strncmp(MINIDUMP_MAGIC, vmst->hdr.magic,
	    sizeof(vmst->hdr.magic)) != 0) {
		_kvm_err(kd, kd->program, "not a minidump for this platform");
		return (-1);
	}

	vmst->hdr.version = le32toh(vmst->hdr.version);
	if (vmst->hdr.version > MINIDUMP_VERSION || vmst->hdr.version < 1) {
		_kvm_err(kd, kd->program, "wrong minidump version. "
		    "Expected %d got %d", MINIDUMP_VERSION, vmst->hdr.version);
		return (-1);
	}
	vmst->hdr.msgbufsize = le32toh(vmst->hdr.msgbufsize);
	vmst->hdr.bitmapsize = le32toh(vmst->hdr.bitmapsize);
	vmst->hdr.pmapsize = le32toh(vmst->hdr.pmapsize);
	vmst->hdr.kernbase = le64toh(vmst->hdr.kernbase);
	vmst->hdr.dmapphys = le64toh(vmst->hdr.dmapphys);
	vmst->hdr.dmapbase = le64toh(vmst->hdr.dmapbase);
	vmst->hdr.dmapend = le64toh(vmst->hdr.dmapend);
	/* dumpavailsize added in version 2 */
	if (vmst->hdr.version >= 2) {
		vmst->hdr.dumpavailsize = le32toh(vmst->hdr.dumpavailsize);
	} else {
		vmst->hdr.dumpavailsize = 0;
	}
	/* flags added in version 3 */
	if (vmst->hdr.version >= 3) {
		vmst->hdr.flags = le32toh(vmst->hdr.flags);
	} else {
		vmst->hdr.flags = MINIDUMP_FLAG_PS_4K;
	}

	switch (vmst->hdr.flags & MINIDUMP_FLAG_PS_MASK) {
	case MINIDUMP_FLAG_PS_4K:
		vmst->page_size = AARCH64_PAGE_SIZE_4K;
		vmst->l3_shift = AARCH64_L3_SHIFT_4K;
		break;
	case MINIDUMP_FLAG_PS_16K:
		vmst->page_size = AARCH64_PAGE_SIZE_16K;
		vmst->l3_shift = AARCH64_L3_SHIFT_16K;
		break;
	default:
		_kvm_err(kd, kd->program, "unknown page size flag %x",
		    vmst->hdr.flags & MINIDUMP_FLAG_PS_MASK);
		return (-1);
	}

	/* Skip header and msgbuf */
	dump_avail_off = vmst->page_size +
	    aarch64_round_page(vmst->hdr.msgbufsize, vmst->page_size);

	/* Skip dump_avail */
	off = dump_avail_off +
	    aarch64_round_page(vmst->hdr.dumpavailsize, vmst->page_size);

	/* build physical address lookup table for sparse pages */
	sparse_off = off +
	    aarch64_round_page(vmst->hdr.bitmapsize, vmst->page_size) +
	    aarch64_round_page(vmst->hdr.pmapsize, vmst->page_size);
	if (_kvm_pt_init(kd, vmst->hdr.dumpavailsize, dump_avail_off,
	    vmst->hdr.bitmapsize, off, sparse_off, vmst->page_size) == -1) {
		return (-1);
	}
	off += aarch64_round_page(vmst->hdr.bitmapsize, vmst->page_size);

	if (_kvm_pmap_init(kd, vmst->hdr.pmapsize, off) == -1) {
		return (-1);
	}
	off += aarch64_round_page(vmst->hdr.pmapsize, vmst->page_size);

	return (0);
}

static int
_aarch64_minidump_vatop(kvm_t *kd, kvaddr_t va, off_t *pa)
{
	struct vmstate *vm;
	aarch64_physaddr_t offset;
	aarch64_pte_t l3;
	kvaddr_t l3_index;
	aarch64_physaddr_t a;
	off_t ofs;

	vm = kd->vmst;
	offset = va & (kd->vmst->page_size - 1);

	if (va >= vm->hdr.dmapbase && va < vm->hdr.dmapend) {
		a = aarch64_trunc_page(va - vm->hdr.dmapbase + vm->hdr.dmapphys,
		    kd->vmst->page_size);
		ofs = _kvm_pt_find(kd, a, kd->vmst->page_size);
		if (ofs == -1) {
			_kvm_err(kd, kd->program, "_aarch64_minidump_vatop: "
			    "direct map address 0x%jx not in minidump",
			    (uintmax_t)va);
			goto invalid;
		}
		*pa = ofs + offset;
		return (kd->vmst->page_size - offset);
	} else if (va >= vm->hdr.kernbase) {
		l3_index = (va - vm->hdr.kernbase) >> kd->vmst->l3_shift;
		if (l3_index >= vm->hdr.pmapsize / sizeof(l3))
			goto invalid;
		l3 = _aarch64_pte_get(kd, l3_index);
		if ((l3 & AARCH64_ATTR_DESCR_MASK) != AARCH64_L3_PAGE) {
			_kvm_err(kd, kd->program,
			    "_aarch64_minidump_vatop: pde not valid");
			goto invalid;
		}
		a = l3 & ~AARCH64_ATTR_MASK;
		ofs = _kvm_pt_find(kd, a, kd->vmst->page_size);
		if (ofs == -1) {
			_kvm_err(kd, kd->program, "_aarch64_minidump_vatop: "
			    "physical address 0x%jx not in minidump",
			    (uintmax_t)a);
			goto invalid;
		}
		*pa = ofs + offset;
		return (kd->vmst->page_size - offset);
	} else {
		_kvm_err(kd, kd->program,
	    "_aarch64_minidump_vatop: virtual address 0x%jx not minidumped",
		    (uintmax_t)va);
		goto invalid;
	}

invalid:
	_kvm_err(kd, 0, "invalid address (0x%jx)", (uintmax_t)va);
	return (0);
}

static int
_aarch64_minidump_kvatop(kvm_t *kd, kvaddr_t va, off_t *pa)
{

	if (ISALIVE(kd)) {
		_kvm_err(kd, 0,
		    "_aarch64_minidump_kvatop called in live kernel!");
		return (0);
	}
	return (_aarch64_minidump_vatop(kd, va, pa));
}

static int
_aarch64_native(kvm_t *kd __unused)
{

#ifdef __aarch64__
	return (1);
#else
	return (0);
#endif
}

static vm_prot_t
_aarch64_entry_to_prot(aarch64_pte_t pte)
{
	vm_prot_t prot = VM_PROT_READ;

	/* Source: arm64/arm64/pmap.c:pmap_protect() */
	if ((pte & AARCH64_ATTR_AP(AARCH64_ATTR_AP_RO)) == 0)
		prot |= VM_PROT_WRITE;
	if ((pte & AARCH64_ATTR_XN) == 0)
		prot |= VM_PROT_EXECUTE;
	return prot;
}

static int
_aarch64_minidump_walk_pages(kvm_t *kd, kvm_walk_pages_cb_t *cb, void *arg)
{
	struct vmstate *vm = kd->vmst;
	u_long nptes = vm->hdr.pmapsize / sizeof(aarch64_pte_t);
	u_long bmindex, dva, pa, pteindex, va;
	struct kvm_bitmap bm;
	vm_prot_t prot;
	int ret = 0;

	if (!_kvm_bitmap_init(&bm, vm->hdr.bitmapsize, &bmindex))
		return (0);

	for (pteindex = 0; pteindex < nptes; pteindex++) {
		aarch64_pte_t pte = _aarch64_pte_get(kd, pteindex);

		if ((pte & AARCH64_ATTR_DESCR_MASK) != AARCH64_L3_PAGE)
			continue;

		va = vm->hdr.kernbase + (pteindex << kd->vmst->l3_shift);
		pa = pte & ~AARCH64_ATTR_MASK;
		dva = vm->hdr.dmapbase + pa;
		if (!_kvm_visit_cb(kd, cb, arg, pa, va, dva,
		    _aarch64_entry_to_prot(pte), kd->vmst->page_size, 0)) {
			goto out;
		}
	}

	while (_kvm_bitmap_next(&bm, &bmindex)) {
		pa = _kvm_bit_id_pa(kd, bmindex, kd->vmst->page_size);
		if (pa == _KVM_PA_INVALID)
			break;
		dva = vm->hdr.dmapbase + pa;
		if (vm->hdr.dmapend < (dva + kd->vmst->page_size))
			break;
		va = 0;
		prot = VM_PROT_READ | VM_PROT_WRITE;
		if (!_kvm_visit_cb(kd, cb, arg, pa, va, dva,
		    prot, kd->vmst->page_size, 0)) {
			goto out;
		}
	}
	ret = 1;

out:
	_kvm_bitmap_deinit(&bm);
	return (ret);
}

static struct kvm_arch kvm_aarch64_minidump = {
	.ka_probe = _aarch64_minidump_probe,
	.ka_initvtop = _aarch64_minidump_initvtop,
	.ka_freevtop = _aarch64_minidump_freevtop,
	.ka_kvatop = _aarch64_minidump_kvatop,
	.ka_native = _aarch64_native,
	.ka_walk_pages = _aarch64_minidump_walk_pages,
};

KVM_ARCH(kvm_aarch64_minidump);
