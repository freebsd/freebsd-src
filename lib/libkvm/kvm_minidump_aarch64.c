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
__FBSDID("$FreeBSD$");

/*
 * ARM64 (AArch64) machine dependent routines for kvm and minidumps.
 */

#include <sys/param.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <kvm.h>

#include "../../sys/arm64/include/minidump.h"

#include <limits.h>

#include "kvm_private.h"
#include "kvm_aarch64.h"

#define	aarch64_round_page(x)	roundup2((kvaddr_t)(x), AARCH64_PAGE_SIZE)

struct vmstate {
	struct minidumphdr hdr;
	struct hpt hpt;
	uint64_t *page_map;
};

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

	_kvm_hpt_free(&vm->hpt);
	free(vm->page_map);
	free(vm);
	kd->vmst = NULL;
}

static int
_aarch64_minidump_initvtop(kvm_t *kd)
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
	if (strncmp(MINIDUMP_MAGIC, vmst->hdr.magic,
	    sizeof(vmst->hdr.magic)) != 0) {
		_kvm_err(kd, kd->program, "not a minidump for this platform");
		return (-1);
	}

	vmst->hdr.version = le32toh(vmst->hdr.version);
	if (vmst->hdr.version != MINIDUMP_VERSION) {
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

	/* Skip header and msgbuf */
	off = AARCH64_PAGE_SIZE + aarch64_round_page(vmst->hdr.msgbufsize);

	bitmap = _kvm_malloc(kd, vmst->hdr.bitmapsize);
	if (bitmap == NULL) {
		_kvm_err(kd, kd->program,
		    "cannot allocate %d bytes for bitmap",
		    vmst->hdr.bitmapsize);
		return (-1);
	}
	if (pread(kd->pmfd, bitmap, vmst->hdr.bitmapsize, off) !=
	    (ssize_t)vmst->hdr.bitmapsize) {
		_kvm_err(kd, kd->program,
		    "cannot read %d bytes for page bitmap",
		    vmst->hdr.bitmapsize);
		free(bitmap);
		return (-1);
	}
	off += aarch64_round_page(vmst->hdr.bitmapsize);

	vmst->page_map = _kvm_malloc(kd, vmst->hdr.pmapsize);
	if (vmst->page_map == NULL) {
		_kvm_err(kd, kd->program,
		    "cannot allocate %d bytes for page_map",
		    vmst->hdr.pmapsize);
		free(bitmap);
		return (-1);
	}
	/* This is the end of the dump, savecore may have truncated it. */
	/*
	 * XXX: This doesn't make sense.  The pmap is not at the end,
	 * and if it is truncated we don't have any actual data (it's
	 * all stored after the bitmap and pmap.  -- jhb
	 */
	if (pread(kd->pmfd, vmst->page_map, vmst->hdr.pmapsize, off) <
	    AARCH64_PAGE_SIZE) {
		_kvm_err(kd, kd->program, "cannot read %d bytes for page_map",
		    vmst->hdr.pmapsize);
		free(bitmap);
		return (-1);
	}
	off += vmst->hdr.pmapsize;

	/* build physical address hash table for sparse pages */
	_kvm_hpt_init(kd, &vmst->hpt, bitmap, vmst->hdr.bitmapsize, off,
	    AARCH64_PAGE_SIZE, sizeof(*bitmap));
	free(bitmap);

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
	offset = va & AARCH64_PAGE_MASK;

	if (va >= vm->hdr.dmapbase && va < vm->hdr.dmapend) {
		a = (va - vm->hdr.dmapbase + vm->hdr.dmapphys) &
		    ~AARCH64_PAGE_MASK;
		ofs = _kvm_hpt_find(&vm->hpt, a);
		if (ofs == -1) {
			_kvm_err(kd, kd->program, "_aarch64_minidump_vatop: "
			    "direct map address 0x%jx not in minidump",
			    (uintmax_t)va);
			goto invalid;
		}
		*pa = ofs + offset;
		return (AARCH64_PAGE_SIZE - offset);
	} else if (va >= vm->hdr.kernbase) {
		l3_index = (va - vm->hdr.kernbase) >> AARCH64_L3_SHIFT;
		if (l3_index >= vm->hdr.pmapsize / sizeof(*vm->page_map))
			goto invalid;
		l3 = le64toh(vm->page_map[l3_index]);
		if ((l3 & AARCH64_ATTR_DESCR_MASK) != AARCH64_L3_PAGE) {
			_kvm_err(kd, kd->program,
			    "_aarch64_minidump_vatop: pde not valid");
			goto invalid;
		}
		a = l3 & ~AARCH64_ATTR_MASK;
		ofs = _kvm_hpt_find(&vm->hpt, a);
		if (ofs == -1) {
			_kvm_err(kd, kd->program, "_aarch64_minidump_vatop: "
			    "physical address 0x%jx not in minidump",
			    (uintmax_t)a);
			goto invalid;
		}
		*pa = ofs + offset;
		return (AARCH64_PAGE_SIZE - offset);
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
_aarch64_native(kvm_t *kd)
{

#ifdef __aarch64__
	return (1);
#else
	return (0);
#endif
}

struct kvm_arch kvm_aarch64_minidump = {
	.ka_probe = _aarch64_minidump_probe,
	.ka_initvtop = _aarch64_minidump_initvtop,
	.ka_freevtop = _aarch64_minidump_freevtop,
	.ka_kvatop = _aarch64_minidump_kvatop,
	.ka_native = _aarch64_native,
};

KVM_ARCH(kvm_aarch64_minidump);
