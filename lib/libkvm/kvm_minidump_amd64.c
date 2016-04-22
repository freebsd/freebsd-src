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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * AMD64 machine dependent routines for kvm and minidumps.
 */

#include <sys/param.h>
#include <sys/endian.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <kvm.h>

#include "../../sys/amd64/include/minidump.h"

#include <limits.h>

#include "kvm_private.h"
#include "kvm_amd64.h"

#define	amd64_round_page(x)	roundup2((kvaddr_t)(x), AMD64_PAGE_SIZE)

struct vmstate {
	struct minidumphdr hdr;
	struct hpt hpt;
	amd64_pte_t *page_map;
};

static int
_amd64_minidump_probe(kvm_t *kd)
{

	return (_kvm_probe_elf_kernel(kd, ELFCLASS64, EM_X86_64) &&
	    _kvm_is_minidump(kd));
}

static void
_amd64_minidump_freevtop(kvm_t *kd)
{
	struct vmstate *vm = kd->vmst;

	_kvm_hpt_free(&vm->hpt);
	if (vm->page_map)
		free(vm->page_map);
	free(vm);
	kd->vmst = NULL;
}

static int
_amd64_minidump_initvtop(kvm_t *kd)
{
	struct vmstate *vmst;
	uint64_t *bitmap;
	off_t off;

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
	if (strncmp(MINIDUMP_MAGIC, vmst->hdr.magic, sizeof(vmst->hdr.magic)) != 0) {
		_kvm_err(kd, kd->program, "not a minidump for this platform");
		return (-1);
	}

	/*
	 * NB: amd64 minidump header is binary compatible between version 1
	 * and version 2; this may not be the case for the future versions.
	 */
	vmst->hdr.version = le32toh(vmst->hdr.version);
	if (vmst->hdr.version != MINIDUMP_VERSION && vmst->hdr.version != 1) {
		_kvm_err(kd, kd->program, "wrong minidump version. expected %d got %d",
		    MINIDUMP_VERSION, vmst->hdr.version);
		return (-1);
	}
	vmst->hdr.msgbufsize = le32toh(vmst->hdr.msgbufsize);
	vmst->hdr.bitmapsize = le32toh(vmst->hdr.bitmapsize);
	vmst->hdr.pmapsize = le32toh(vmst->hdr.pmapsize);
	vmst->hdr.kernbase = le64toh(vmst->hdr.kernbase);
	vmst->hdr.dmapbase = le64toh(vmst->hdr.dmapbase);
	vmst->hdr.dmapend = le64toh(vmst->hdr.dmapend);

	/* Skip header and msgbuf */
	off = AMD64_PAGE_SIZE + amd64_round_page(vmst->hdr.msgbufsize);

	bitmap = _kvm_malloc(kd, vmst->hdr.bitmapsize);
	if (bitmap == NULL) {
		_kvm_err(kd, kd->program, "cannot allocate %d bytes for bitmap", vmst->hdr.bitmapsize);
		return (-1);
	}
	if (pread(kd->pmfd, bitmap, vmst->hdr.bitmapsize, off) !=
	    (ssize_t)vmst->hdr.bitmapsize) {
		_kvm_err(kd, kd->program, "cannot read %d bytes for page bitmap", vmst->hdr.bitmapsize);
		free(bitmap);
		return (-1);
	}
	off += amd64_round_page(vmst->hdr.bitmapsize);

	vmst->page_map = _kvm_malloc(kd, vmst->hdr.pmapsize);
	if (vmst->page_map == NULL) {
		_kvm_err(kd, kd->program, "cannot allocate %d bytes for page_map", vmst->hdr.pmapsize);
		free(bitmap);
		return (-1);
	}
	if (pread(kd->pmfd, vmst->page_map, vmst->hdr.pmapsize, off) !=
	    (ssize_t)vmst->hdr.pmapsize) {
		_kvm_err(kd, kd->program, "cannot read %d bytes for page_map", vmst->hdr.pmapsize);
		free(bitmap);
		return (-1);
	}
	off += vmst->hdr.pmapsize;

	/* build physical address hash table for sparse pages */
	_kvm_hpt_init(kd, &vmst->hpt, bitmap, vmst->hdr.bitmapsize, off,
	    AMD64_PAGE_SIZE, sizeof(*bitmap));
	free(bitmap);

	return (0);
}

static int
_amd64_minidump_vatop_v1(kvm_t *kd, kvaddr_t va, off_t *pa)
{
	struct vmstate *vm;
	amd64_physaddr_t offset;
	amd64_pte_t pte;
	kvaddr_t pteindex;
	amd64_physaddr_t a;
	off_t ofs;

	vm = kd->vmst;
	offset = va & AMD64_PAGE_MASK;

	if (va >= vm->hdr.kernbase) {
		pteindex = (va - vm->hdr.kernbase) >> AMD64_PAGE_SHIFT;
		if (pteindex >= vm->hdr.pmapsize / sizeof(*vm->page_map))
			goto invalid;
		pte = le64toh(vm->page_map[pteindex]);
		if ((pte & AMD64_PG_V) == 0) {
			_kvm_err(kd, kd->program,
			    "_amd64_minidump_vatop_v1: pte not valid");
			goto invalid;
		}
		a = pte & AMD64_PG_FRAME;
		ofs = _kvm_hpt_find(&vm->hpt, a);
		if (ofs == -1) {
			_kvm_err(kd, kd->program,
	    "_amd64_minidump_vatop_v1: physical address 0x%jx not in minidump",
			    (uintmax_t)a);
			goto invalid;
		}
		*pa = ofs + offset;
		return (AMD64_PAGE_SIZE - offset);
	} else if (va >= vm->hdr.dmapbase && va < vm->hdr.dmapend) {
		a = (va - vm->hdr.dmapbase) & ~AMD64_PAGE_MASK;
		ofs = _kvm_hpt_find(&vm->hpt, a);
		if (ofs == -1) {
			_kvm_err(kd, kd->program,
    "_amd64_minidump_vatop_v1: direct map address 0x%jx not in minidump",
			    (uintmax_t)va);
			goto invalid;
		}
		*pa = ofs + offset;
		return (AMD64_PAGE_SIZE - offset);
	} else {
		_kvm_err(kd, kd->program,
	    "_amd64_minidump_vatop_v1: virtual address 0x%jx not minidumped",
		    (uintmax_t)va);
		goto invalid;
	}

invalid:
	_kvm_err(kd, 0, "invalid address (0x%jx)", (uintmax_t)va);
	return (0);
}

static int
_amd64_minidump_vatop(kvm_t *kd, kvaddr_t va, off_t *pa)
{
	amd64_pte_t pt[AMD64_NPTEPG];
	struct vmstate *vm;
	amd64_physaddr_t offset;
	amd64_pde_t pde;
	amd64_pte_t pte;
	kvaddr_t pteindex;
	kvaddr_t pdeindex;
	amd64_physaddr_t a;
	off_t ofs;

	vm = kd->vmst;
	offset = va & AMD64_PAGE_MASK;

	if (va >= vm->hdr.kernbase) {
		pdeindex = (va - vm->hdr.kernbase) >> AMD64_PDRSHIFT;
		if (pdeindex >= vm->hdr.pmapsize / sizeof(*vm->page_map))
			goto invalid;
		pde = le64toh(vm->page_map[pdeindex]);
		if ((pde & AMD64_PG_V) == 0) {
			_kvm_err(kd, kd->program,
			    "_amd64_minidump_vatop: pde not valid");
			goto invalid;
		}
		if ((pde & AMD64_PG_PS) == 0) {
			a = pde & AMD64_PG_FRAME;
			ofs = _kvm_hpt_find(&vm->hpt, a);
			if (ofs == -1) {
				_kvm_err(kd, kd->program,
	    "_amd64_minidump_vatop: pt physical address 0x%jx not in minidump",
				    (uintmax_t)a);
				goto invalid;
			}
			/* TODO: Just read the single PTE */
			if (pread(kd->pmfd, &pt, AMD64_PAGE_SIZE, ofs) !=
			    AMD64_PAGE_SIZE) {
				_kvm_err(kd, kd->program,
				    "cannot read %d bytes for page table",
				    AMD64_PAGE_SIZE);
				return (-1);
			}
			pteindex = (va >> AMD64_PAGE_SHIFT) &
			    (AMD64_NPTEPG - 1);
			pte = le64toh(pt[pteindex]);
			if ((pte & AMD64_PG_V) == 0) {
				_kvm_err(kd, kd->program,
				    "_amd64_minidump_vatop: pte not valid");
				goto invalid;
			}
			a = pte & AMD64_PG_FRAME;
		} else {
			a = pde & AMD64_PG_PS_FRAME;
			a += (va & AMD64_PDRMASK) ^ offset;
		}
		ofs = _kvm_hpt_find(&vm->hpt, a);
		if (ofs == -1) {
			_kvm_err(kd, kd->program,
	    "_amd64_minidump_vatop: physical address 0x%jx not in minidump",
			    (uintmax_t)a);
			goto invalid;
		}
		*pa = ofs + offset;
		return (AMD64_PAGE_SIZE - offset);
	} else if (va >= vm->hdr.dmapbase && va < vm->hdr.dmapend) {
		a = (va - vm->hdr.dmapbase) & ~AMD64_PAGE_MASK;
		ofs = _kvm_hpt_find(&vm->hpt, a);
		if (ofs == -1) {
			_kvm_err(kd, kd->program,
	    "_amd64_minidump_vatop: direct map address 0x%jx not in minidump",
			    (uintmax_t)va);
			goto invalid;
		}
		*pa = ofs + offset;
		return (AMD64_PAGE_SIZE - offset);
	} else {
		_kvm_err(kd, kd->program,
	    "_amd64_minidump_vatop: virtual address 0x%jx not minidumped",
		    (uintmax_t)va);
		goto invalid;
	}

invalid:
	_kvm_err(kd, 0, "invalid address (0x%jx)", (uintmax_t)va);
	return (0);
}

static int
_amd64_minidump_kvatop(kvm_t *kd, kvaddr_t va, off_t *pa)
{

	if (ISALIVE(kd)) {
		_kvm_err(kd, 0,
		    "_amd64_minidump_kvatop called in live kernel!");
		return (0);
	}
	if (((struct vmstate *)kd->vmst)->hdr.version == 1)
		return (_amd64_minidump_vatop_v1(kd, va, pa));
	else
		return (_amd64_minidump_vatop(kd, va, pa));
}

struct kvm_arch kvm_amd64_minidump = {
	.ka_probe = _amd64_minidump_probe,
	.ka_initvtop = _amd64_minidump_initvtop,
	.ka_freevtop = _amd64_minidump_freevtop,
	.ka_kvatop = _amd64_minidump_kvatop,
	.ka_native = _amd64_native,
};

KVM_ARCH(kvm_amd64_minidump);
