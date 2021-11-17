/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2006 Peter Wemm
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_watchdog.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/kernel.h>
#include <sys/kerneldump.h>
#include <sys/msgbuf.h>
#include <sys/watchdog.h>
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_page.h>
#include <vm/vm_phys.h>
#include <vm/vm_dumpset.h>
#include <vm/pmap.h>
#include <machine/atomic.h>
#include <machine/elf.h>
#include <machine/md_var.h>
#include <machine/minidump.h>

CTASSERT(sizeof(struct kerneldumpheader) == 512);

#define	MD_ALIGN(x)	(((off_t)(x) + PAGE_MASK) & ~PAGE_MASK)
#define	DEV_ALIGN(x)	roundup2((off_t)(x), DEV_BSIZE)

static struct kerneldumpheader kdh;

/* Handle chunked writes. */
static size_t fragsz;
static void *dump_va;

static int
blk_flush(struct dumperinfo *di)
{
	int error;

	if (fragsz == 0)
		return (0);

	error = dump_append(di, dump_va, 0, fragsz);
	fragsz = 0;
	return (error);
}

static int
blk_write(struct dumperinfo *di, char *ptr, vm_paddr_t pa, size_t sz)
{
	size_t len;
	int error, i, c;
	u_int maxdumpsz;

	maxdumpsz = min(di->maxiosize, MAXDUMPPGS * PAGE_SIZE);
	if (maxdumpsz == 0)	/* seatbelt */
		maxdumpsz = PAGE_SIZE;
	error = 0;
	if ((sz % PAGE_SIZE) != 0) {
		printf("size not page aligned\n");
		return (EINVAL);
	}
	if (ptr != NULL && pa != 0) {
		printf("cant have both va and pa!\n");
		return (EINVAL);
	}
	if (pa != 0 && (((uintptr_t)ptr) % PAGE_SIZE) != 0) {
		printf("address not page aligned\n");
		return (EINVAL);
	}
	if (ptr != NULL) {
		/* If we're doing a virtual dump, flush any pre-existing pa pages */
		error = blk_flush(di);
		if (error)
			return (error);
	}
	while (sz) {
		len = maxdumpsz - fragsz;
		if (len > sz)
			len = sz;

		dumpsys_pb_progress(len);
		wdog_kern_pat(WD_LASTVAL);

		if (ptr) {
			error = dump_append(di, ptr, 0, len);
			if (error)
				return (error);
			ptr += len;
			sz -= len;
		} else {
			for (i = 0; i < len; i += PAGE_SIZE)
				dump_va = pmap_kenter_temporary(pa + i, (i + fragsz) >> PAGE_SHIFT);
			fragsz += len;
			pa += len;
			sz -= len;
			if (fragsz == maxdumpsz) {
				error = blk_flush(di);
				if (error)
					return (error);
			}
		}

		/* Check for user abort. */
		c = cncheckc();
		if (c == 0x03)
			return (ECANCELED);
		if (c != -1)
			printf(" (CTRL-C to abort) ");
	}

	return (0);
}

/* A fake page table page, to avoid having to handle both 4K and 2M pages */
static pt_entry_t fakept[NPTEPG];

#ifdef PMAP_PAE_COMP
#define	cpu_minidumpsys		cpu_minidumpsys_pae
#define	IdlePTD			IdlePTD_pae
#else
#define	cpu_minidumpsys		cpu_minidumpsys_nopae
#define	IdlePTD			IdlePTD_nopae
#endif

int
cpu_minidumpsys(struct dumperinfo *di, const struct minidumpstate *state)
{
	uint64_t dumpsize;
	uint32_t ptesize;
	vm_offset_t va, kva_end;
	int error;
	uint64_t pa;
	pd_entry_t *pd, pde;
	pt_entry_t *pt, pte;
	int j, k;
	struct minidumphdr mdhdr;
	struct msgbuf *mbp;

	/* Snapshot the KVA upper bound in case it grows. */
	kva_end = kernel_vm_end;

	/*
	 * Walk the kernel page table pages, setting the active entries in the
	 * dump bitmap.
	 *
	 * NB: for a live dump, we may be racing with updates to the page
	 * tables, so care must be taken to read each entry only once.
	 */
	ptesize = 0;
	for (va = KERNBASE; va < kva_end; va += NBPDR) {
		/*
		 * We always write a page, even if it is zero. Each
		 * page written corresponds to 2MB of space
		 */
		ptesize += PAGE_SIZE;
		pd = IdlePTD;	/* always mapped! */
		j = va >> PDRSHIFT;
		pde = pte_load(&pd[va >> PDRSHIFT]);
		if ((pde & (PG_PS | PG_V)) == (PG_PS | PG_V))  {
			/* This is an entire 2M page. */
			pa = pde & PG_PS_FRAME;
			for (k = 0; k < NPTEPG; k++) {
				if (vm_phys_is_dumpable(pa))
					dump_add_page(pa);
				pa += PAGE_SIZE;
			}
			continue;
		}
		if ((pde & PG_V) == PG_V) {
			/* set bit for each valid page in this 2MB block */
			pt = pmap_kenter_temporary(pde & PG_FRAME, 0);
			for (k = 0; k < NPTEPG; k++) {
				pte = pte_load(&pt[k]);
				if ((pte & PG_V) == PG_V) {
					pa = pte & PG_FRAME;
					if (vm_phys_is_dumpable(pa))
						dump_add_page(pa);
				}
			}
		} else {
			/* nothing, we're going to dump a null page */
		}
	}

	/* Calculate dump size. */
	mbp = state->msgbufp;
	dumpsize = ptesize;
	dumpsize += round_page(mbp->msg_size);
	dumpsize += round_page(sizeof(dump_avail));
	dumpsize += round_page(BITSET_SIZE(vm_page_dump_pages));
	VM_PAGE_DUMP_FOREACH(pa) {
		/* Clear out undumpable pages now if needed */
		if (vm_phys_is_dumpable(pa)) {
			dumpsize += PAGE_SIZE;
		} else {
			dump_drop_page(pa);
		}
	}
	dumpsize += PAGE_SIZE;

	dumpsys_pb_init(dumpsize);

	/* Initialize mdhdr */
	bzero(&mdhdr, sizeof(mdhdr));
	strcpy(mdhdr.magic, MINIDUMP_MAGIC);
	mdhdr.version = MINIDUMP_VERSION;
	mdhdr.msgbufsize = mbp->msg_size;
	mdhdr.bitmapsize = round_page(BITSET_SIZE(vm_page_dump_pages));
	mdhdr.ptesize = ptesize;
	mdhdr.kernbase = KERNBASE;
	mdhdr.paemode = pae_mode;
	mdhdr.dumpavailsize = round_page(sizeof(dump_avail));

	dump_init_header(di, &kdh, KERNELDUMPMAGIC, KERNELDUMP_I386_VERSION,
	    dumpsize);

	error = dump_start(di, &kdh);
	if (error != 0)
		goto fail;

	printf("Physical memory: %ju MB\n", ptoa((uintmax_t)physmem) / 1048576);
	printf("Dumping %llu MB:", (long long)dumpsize >> 20);

	/* Dump my header */
	bzero(&fakept, sizeof(fakept));
	bcopy(&mdhdr, &fakept, sizeof(mdhdr));
	error = blk_write(di, (char *)&fakept, 0, PAGE_SIZE);
	if (error)
		goto fail;

	/* Dump msgbuf up front */
	error = blk_write(di, (char *)mbp->msg_ptr, 0,
	    round_page(mbp->msg_size));
	if (error)
		goto fail;

	/* Dump dump_avail */
	_Static_assert(sizeof(dump_avail) <= sizeof(fakept),
	    "Large dump_avail not handled");
	bzero(fakept, sizeof(fakept));
	memcpy(fakept, dump_avail, sizeof(dump_avail));
	error = blk_write(di, (char *)&fakept, 0, PAGE_SIZE);
	if (error)
		goto fail;

	/* Dump bitmap */
	error = blk_write(di, (char *)vm_page_dump, 0,
	    round_page(BITSET_SIZE(vm_page_dump_pages)));
	if (error)
		goto fail;

	/* Dump kernel page table pages */
	for (va = KERNBASE; va < kva_end; va += NBPDR) {
		/* We always write a page, even if it is zero */
		pd = IdlePTD;	/* always mapped! */
		pde = pte_load(&pd[va >> PDRSHIFT]);
		if ((pde & (PG_PS | PG_V)) == (PG_PS | PG_V))  {
			/* This is a single 2M block. Generate a fake PTP */
			pa = pde & PG_PS_FRAME;
			for (k = 0; k < NPTEPG; k++) {
				fakept[k] = (pa + (k * PAGE_SIZE)) | PG_V | PG_RW | PG_A | PG_M;
			}
			error = blk_write(di, (char *)&fakept, 0, PAGE_SIZE);
			if (error)
				goto fail;
			/* flush, in case we reuse fakept in the same block */
			error = blk_flush(di);
			if (error)
				goto fail;
			continue;
		}
		if ((pde & PG_V) == PG_V) {
			pa = pde & PG_FRAME;
			error = blk_write(di, 0, pa, PAGE_SIZE);
			if (error)
				goto fail;
		} else {
			bzero(fakept, sizeof(fakept));
			error = blk_write(di, (char *)&fakept, 0, PAGE_SIZE);
			if (error)
				goto fail;
			/* flush, in case we reuse fakept in the same block */
			error = blk_flush(di);
			if (error)
				goto fail;
		}
	}

	/* Dump memory chunks */
	VM_PAGE_DUMP_FOREACH(pa) {
		error = blk_write(di, 0, pa, PAGE_SIZE);
		if (error)
			goto fail;
	}

	error = blk_flush(di);
	if (error)
		goto fail;

	error = dump_finish(di, &kdh);
	if (error != 0)
		goto fail;

	printf("\nDump complete\n");
	return (0);

 fail:
	if (error < 0)
		error = -error;

	if (error == ECANCELED)
		printf("\nDump aborted\n");
	else if (error == E2BIG || error == ENOSPC) {
		printf("\nDump failed. Partition too small (about %lluMB were "
		    "needed this time).\n", (long long)dumpsize >> 20);
	} else
		printf("\n** DUMP FAILED (ERROR %d) **\n", error);
	return (error);
}
