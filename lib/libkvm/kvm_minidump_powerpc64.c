/*-
 * Copyright (c) 2006 Peter Wemm
 * Copyright (c) 2019 Leandro Lupori
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
 * From: FreeBSD: src/lib/libkvm/kvm_minidump_riscv.c
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>

#include <kvm.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../../sys/powerpc/include/minidump.h"
#include "kvm_private.h"
#include "kvm_powerpc64.h"


static int
_powerpc64_minidump_probe(kvm_t *kd)
{
	return (_kvm_probe_elf_kernel(kd, ELFCLASS64, EM_PPC64) &&
	    _kvm_is_minidump(kd));
}

static void
_powerpc64_minidump_freevtop(kvm_t *kd)
{
	struct vmstate *vm = kd->vmst;

	if (vm == NULL)
		return;
	if (PPC64_MMU_OPS(kd))
		PPC64_MMU_OP(kd, cleanup);
	free(vm);
	kd->vmst = NULL;
}

static int
_powerpc64_minidump_initvtop(kvm_t *kd)
{
	struct vmstate *vmst;
	struct minidumphdr *hdr;
	off_t dump_avail_off, bitmap_off, pmap_off, sparse_off;
	const char *mmu_name;

	/* Alloc VM */
	vmst = _kvm_malloc(kd, sizeof(*vmst));
	if (vmst == NULL) {
		_kvm_err(kd, kd->program, "cannot allocate vm");
		return (-1);
	}
	hdr = &vmst->hdr;
	kd->vmst = vmst;
	PPC64_MMU_OPS(kd) = NULL;
	/* Read minidump header */
	if (pread(kd->pmfd, hdr, sizeof(*hdr), 0) != sizeof(*hdr)) {
		_kvm_err(kd, kd->program, "cannot read minidump header");
		goto failed;
	}
	/* Check magic */
	if (strncmp(MINIDUMP_MAGIC, hdr->magic, sizeof(hdr->magic)) != 0) {
		_kvm_err(kd, kd->program, "not a minidump for this platform");
		goto failed;
	}
	/* Check version */
	hdr->version = be32toh(hdr->version);
	if (hdr->version != MINIDUMP_VERSION && hdr->version != 1) {
		_kvm_err(kd, kd->program, "wrong minidump version. "
		    "Expected %d got %d", MINIDUMP_VERSION, hdr->version);
		goto failed;
	}
	/* Convert header fields to host endian */
	hdr->msgbufsize		= be32toh(hdr->msgbufsize);
	hdr->bitmapsize		= be32toh(hdr->bitmapsize);
	hdr->pmapsize		= be32toh(hdr->pmapsize);
	hdr->kernbase		= be64toh(hdr->kernbase);
	hdr->kernend		= be64toh(hdr->kernend);
	hdr->dmapbase		= be64toh(hdr->dmapbase);
	hdr->dmapend		= be64toh(hdr->dmapend);
	hdr->hw_direct_map	= be32toh(hdr->hw_direct_map);
	hdr->startkernel	= be64toh(hdr->startkernel);
	hdr->endkernel		= be64toh(hdr->endkernel);
	hdr->dumpavailsize	= hdr->version == MINIDUMP_VERSION ?
	    be32toh(hdr->dumpavailsize) : 0;

	vmst->kimg_start = PPC64_KERNBASE;
	vmst->kimg_end = PPC64_KERNBASE + hdr->endkernel - hdr->startkernel;

	/* dump header */
	dprintf("%s: mmu_name=%s,\n\t"
	    "msgbufsize=0x%jx, bitmapsize=0x%jx, pmapsize=0x%jx, "
	    "kernbase=0x%jx, kernend=0x%jx,\n\t"
	    "dmapbase=0x%jx, dmapend=0x%jx, hw_direct_map=%d, "
	    "startkernel=0x%jx, endkernel=0x%jx\n\t"
	    "kimg_start=0x%jx, kimg_end=0x%jx\n",
	    __func__, hdr->mmu_name,
	    (uintmax_t)hdr->msgbufsize,
	    (uintmax_t)hdr->bitmapsize, (uintmax_t)hdr->pmapsize,
	    (uintmax_t)hdr->kernbase, (uintmax_t)hdr->kernend,
	    (uintmax_t)hdr->dmapbase, (uintmax_t)hdr->dmapend,
	    hdr->hw_direct_map, hdr->startkernel, hdr->endkernel,
	    (uintmax_t)vmst->kimg_start, (uintmax_t)vmst->kimg_end);

	/* Detect and initialize MMU */
	mmu_name = hdr->mmu_name;
	if (strcmp(mmu_name, PPC64_MMU_G5) == 0 ||
	    strcmp(mmu_name, PPC64_MMU_PHYP) == 0)
		PPC64_MMU_OPS(kd) = ppc64_mmu_ops_hpt;
	else {
		_kvm_err(kd, kd->program, "unsupported MMU: %s", mmu_name);
		goto failed;
	}
	if (PPC64_MMU_OP(kd, init) == -1)
		goto failed;

	/* Get dump parts' offsets */
	dump_avail_off	= PPC64_PAGE_SIZE + ppc64_round_page(hdr->msgbufsize);
	bitmap_off	= dump_avail_off + ppc64_round_page(hdr->dumpavailsize);
	pmap_off	= bitmap_off + ppc64_round_page(hdr->bitmapsize);
	sparse_off	= pmap_off + ppc64_round_page(hdr->pmapsize);

	/* dump offsets */
	dprintf("%s: msgbuf_off=0x%jx, bitmap_off=0x%jx, pmap_off=0x%jx, "
	    "sparse_off=0x%jx\n",
	    __func__, (uintmax_t)PPC64_PAGE_SIZE, (uintmax_t)bitmap_off,
	    (uintmax_t)pmap_off, (uintmax_t)sparse_off);

	/* build physical address lookup table for sparse pages */
	if (_kvm_pt_init(kd, hdr->dumpavailsize, dump_avail_off,
	    hdr->bitmapsize, bitmap_off, sparse_off, PPC64_PAGE_SIZE) == -1)
		goto failed;

	if (_kvm_pmap_init(kd, hdr->pmapsize, pmap_off) == -1)
		goto failed;
	return (0);

failed:
	_powerpc64_minidump_freevtop(kd);
	return (-1);
}

static int
_powerpc64_minidump_kvatop(kvm_t *kd, kvaddr_t va, off_t *pa)
{
	if (ISALIVE(kd)) {
		_kvm_err(kd, 0, "%s called in live kernel!", __func__);
		return (0);
	}
	return (PPC64_MMU_OP(kd, kvatop, va, pa));
}

static int
_powerpc64_native(kvm_t *kd __unused)
{
#ifdef __powerpc64__
	return (1);
#else
	return (0);
#endif
}

static kssize_t
_powerpc64_kerndisp(kvm_t *kd)
{
	return (kd->vmst->hdr.startkernel - PPC64_KERNBASE);
}

static int
_powerpc64_minidump_walk_pages(kvm_t *kd, kvm_walk_pages_cb_t *cb, void *arg)
{
	return (PPC64_MMU_OP(kd, walk_pages, cb, arg));
}

static struct kvm_arch kvm_powerpc64_minidump = {
	.ka_probe	= _powerpc64_minidump_probe,
	.ka_initvtop	= _powerpc64_minidump_initvtop,
	.ka_freevtop	= _powerpc64_minidump_freevtop,
	.ka_kvatop	= _powerpc64_minidump_kvatop,
	.ka_walk_pages	= _powerpc64_minidump_walk_pages,
	.ka_native	= _powerpc64_native,
	.ka_kerndisp	= _powerpc64_kerndisp,
};

KVM_ARCH(kvm_powerpc64_minidump);
