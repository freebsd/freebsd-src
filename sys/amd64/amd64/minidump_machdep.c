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

#include "opt_pmap.h"
#include "opt_watchdog.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/kernel.h>
#include <sys/kerneldump.h>
#include <sys/msgbuf.h>
#include <sys/sysctl.h>
#include <sys/watchdog.h>
#include <sys/vmmeter.h>
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
#include <machine/vmparam.h>

CTASSERT(sizeof(struct kerneldumpheader) == 512);

static struct kerneldumpheader kdh;

/* Handle chunked writes. */
static size_t fragsz;
static void *dump_va;
static size_t progress, dumpsize, wdog_next;

static int dump_retry_count = 5;
SYSCTL_INT(_machdep, OID_AUTO, dump_retry_count, CTLFLAG_RWTUN,
    &dump_retry_count, 0, "Number of times dump has to retry before bailing out");

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

/* Pat the watchdog approximately every 128MB of the dump. */
#define	WDOG_DUMP_INTERVAL	(128 * 1024 * 1024)

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
	if ((((uintptr_t)pa) % PAGE_SIZE) != 0) {
		printf("address not page aligned %p\n", ptr);
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
		progress -= len;

		dumpsys_pb_progress(len);
		if (progress <= wdog_next) {
			wdog_kern_pat(WD_LASTVAL);
			if (wdog_next > WDOG_DUMP_INTERVAL)
				wdog_next -= WDOG_DUMP_INTERVAL;
			else
				wdog_next = 0;
		}

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
static pd_entry_t fakepd[NPDEPG];

int
cpu_minidumpsys(struct dumperinfo *di, const struct minidumpstate *state)
{
	uint32_t pmapsize;
	vm_offset_t va, kva_end;
	int error;
	uint64_t *pml4, *pdp, *pd, *pt, pa;
	uint64_t pdpe, pde, pte;
	int ii, j, k, n;
	int retry_count;
	struct minidumphdr mdhdr;

	retry_count = 0;
 retry:
	retry_count++;

	/* Snapshot the KVA upper bound in case it grows. */
	kva_end = MAX(KERNBASE + nkpt * NBPDR, kernel_vm_end);

	/*
	 * Walk the kernel page table pages, setting the active entries in the
	 * dump bitmap.
	 *
	 * NB: for a live dump, we may be racing with updates to the page
	 * tables, so care must be taken to read each entry only once.
	 */
	pmapsize = 0;
	for (va = VM_MIN_KERNEL_ADDRESS; va < kva_end; ) {
		/*
		 * We always write a page, even if it is zero. Each
		 * page written corresponds to 1GB of space
		 */
		pmapsize += PAGE_SIZE;
		ii = pmap_pml4e_index(va);
		pml4 = (uint64_t *)PHYS_TO_DMAP(KPML4phys) + ii;
		pdp = (uint64_t *)PHYS_TO_DMAP(*pml4 & PG_FRAME);
		pdpe = atomic_load_64(&pdp[pmap_pdpe_index(va)]);
		if ((pdpe & PG_V) == 0) {
			va += NBPDP;
			continue;
		}

		/*
		 * 1GB page is represented as 512 2MB pages in a dump.
		 */
		if ((pdpe & PG_PS) != 0) {
			va += NBPDP;
			pa = pdpe & PG_PS_FRAME;
			for (n = 0; n < NPDEPG * NPTEPG; n++) {
				if (vm_phys_is_dumpable(pa))
					dump_add_page(pa);
				pa += PAGE_SIZE;
			}
			continue;
		}

		pd = (uint64_t *)PHYS_TO_DMAP(pdpe & PG_FRAME);
		for (n = 0; n < NPDEPG; n++, va += NBPDR) {
			pde = atomic_load_64(&pd[pmap_pde_index(va)]);

			if ((pde & PG_V) == 0)
				continue;

			if ((pde & PG_PS) != 0) {
				/* This is an entire 2M page. */
				pa = pde & PG_PS_FRAME;
				for (k = 0; k < NPTEPG; k++) {
					if (vm_phys_is_dumpable(pa))
						dump_add_page(pa);
					pa += PAGE_SIZE;
				}
				continue;
			}

			pa = pde & PG_FRAME;
			/* set bit for this PTE page */
			if (vm_phys_is_dumpable(pa))
				dump_add_page(pa);
			/* and for each valid page in this 2MB block */
			pt = (uint64_t *)PHYS_TO_DMAP(pde & PG_FRAME);
			for (k = 0; k < NPTEPG; k++) {
				pte = atomic_load_64(&pt[k]);
				if ((pte & PG_V) == 0)
					continue;
				pa = pte & PG_FRAME;
				if (PHYS_IN_DMAP(pa) && vm_phys_is_dumpable(pa))
					dump_add_page(pa);
			}
		}
	}

	/* Calculate dump size. */
	dumpsize = pmapsize;
	dumpsize += round_page(msgbufp->msg_size);
	dumpsize += round_page(sizeof(dump_avail));
	dumpsize += round_page(BITSET_SIZE(vm_page_dump_pages));
	VM_PAGE_DUMP_FOREACH(pa) {
		/* Clear out undumpable pages now if needed */
		if (PHYS_IN_DMAP(pa) && vm_phys_is_dumpable(pa)) {
			dumpsize += PAGE_SIZE;
		} else {
			dump_drop_page(pa);
		}
	}
	dumpsize += PAGE_SIZE;

	wdog_next = progress = dumpsize;
	dumpsys_pb_init(dumpsize);

	/* Initialize mdhdr */
	bzero(&mdhdr, sizeof(mdhdr));
	strcpy(mdhdr.magic, MINIDUMP_MAGIC);
	mdhdr.version = MINIDUMP_VERSION;
	mdhdr.msgbufsize = msgbufp->msg_size;
	mdhdr.bitmapsize = round_page(BITSET_SIZE(vm_page_dump_pages));
	mdhdr.pmapsize = pmapsize;
	mdhdr.kernbase = VM_MIN_KERNEL_ADDRESS;
	mdhdr.dmapbase = DMAP_MIN_ADDRESS;
	mdhdr.dmapend = DMAP_MAX_ADDRESS;
	mdhdr.dumpavailsize = round_page(sizeof(dump_avail));

	dump_init_header(di, &kdh, KERNELDUMPMAGIC, KERNELDUMP_AMD64_VERSION,
	    dumpsize);

	error = dump_start(di, &kdh);
	if (error != 0)
		goto fail;

	printf("Dumping %llu out of %ju MB:", (long long)dumpsize >> 20,
	    ptoa((uintmax_t)physmem) / 1048576);

	/* Dump my header */
	bzero(&fakepd, sizeof(fakepd));
	bcopy(&mdhdr, &fakepd, sizeof(mdhdr));
	error = blk_write(di, (char *)&fakepd, 0, PAGE_SIZE);
	if (error)
		goto fail;

	/* Dump msgbuf up front */
	error = blk_write(di, (char *)msgbufp->msg_ptr, 0, round_page(msgbufp->msg_size));
	if (error)
		goto fail;

	/* Dump dump_avail */
	_Static_assert(sizeof(dump_avail) <= sizeof(fakepd),
	    "Large dump_avail not handled");
	bzero(&fakepd, sizeof(fakepd));
	memcpy(fakepd, dump_avail, sizeof(dump_avail));
	error = blk_write(di, (char *)fakepd, 0, PAGE_SIZE);
	if (error)
		goto fail;

	/* Dump bitmap */
	error = blk_write(di, (char *)vm_page_dump, 0,
	    round_page(BITSET_SIZE(vm_page_dump_pages)));
	if (error)
		goto fail;

	/* Dump kernel page directory pages */
	bzero(fakepd, sizeof(fakepd));
	for (va = VM_MIN_KERNEL_ADDRESS; va < kva_end; va += NBPDP) {
		ii = pmap_pml4e_index(va);
		pml4 = (uint64_t *)PHYS_TO_DMAP(KPML4phys) + ii;
		pdp = (uint64_t *)PHYS_TO_DMAP(*pml4 & PG_FRAME);
		pdpe = atomic_load_64(&pdp[pmap_pdpe_index(va)]);

		/* We always write a page, even if it is zero */
		if ((pdpe & PG_V) == 0) {
			error = blk_write(di, (char *)&fakepd, 0, PAGE_SIZE);
			if (error)
				goto fail;
			/* flush, in case we reuse fakepd in the same block */
			error = blk_flush(di);
			if (error)
				goto fail;
			continue;
		}

		/* 1GB page is represented as 512 2MB pages in a dump */
		if ((pdpe & PG_PS) != 0) {
			/* PDPE and PDP have identical layout in this case */
			fakepd[0] = pdpe;
			for (j = 1; j < NPDEPG; j++)
				fakepd[j] = fakepd[j - 1] + NBPDR;
			error = blk_write(di, (char *)&fakepd, 0, PAGE_SIZE);
			if (error)
				goto fail;
			/* flush, in case we reuse fakepd in the same block */
			error = blk_flush(di);
			if (error)
				goto fail;
			bzero(fakepd, sizeof(fakepd));
			continue;
		}

		pa = pdpe & PG_FRAME;
		if (PHYS_IN_DMAP(pa) && vm_phys_is_dumpable(pa)) {
			pd = (uint64_t *)PHYS_TO_DMAP(pa);
			error = blk_write(di, (char *)pd, 0, PAGE_SIZE);
		} else {
			/* Malformed pa, write the zeroed fakepd. */
			error = blk_write(di, (char *)&fakepd, 0, PAGE_SIZE);
		}
		if (error)
			goto fail;
		error = blk_flush(di);
		if (error)
			goto fail;
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

	printf("\n");
	if (error == ENOSPC) {
		printf("Dump map grown while dumping. ");
		if (retry_count < dump_retry_count) {
			printf("Retrying...\n");
			goto retry;
		}
		printf("Dump failed.\n");
	}
	else if (error == ECANCELED)
		printf("Dump aborted\n");
	else if (error == E2BIG) {
		printf("Dump failed. Partition too small (about %lluMB were "
		    "needed this time).\n", (long long)dumpsize >> 20);
	} else
		printf("** DUMP FAILED (ERROR %d) **\n", error);
	return (error);
}
