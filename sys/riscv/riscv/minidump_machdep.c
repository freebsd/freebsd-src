/*-
 * Copyright (c) 2006 Peter Wemm
 * Copyright (c) 2015 The FreeBSD Foundation
 * All rights reserved.
 * Copyright (c) 2019 Mitchell Horne
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

CTASSERT(sizeof(struct kerneldumpheader) == 512);

static struct kerneldumpheader kdh;

/* Handle chunked writes. */
static size_t fragsz;
static void *dump_va;
static size_t dumpsize;

static uint64_t tmpbuffer[PAGE_SIZE / sizeof(uint64_t)];

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

/*
 * Write a block of data to the dump file.
 *
 * Caller can provide data through a pointer or by specifying its
 * physical address.
 *
 * XXX writes using pa should be no larger than PAGE_SIZE.
 */
static int
blk_write(struct dumperinfo *di, char *ptr, vm_paddr_t pa, size_t sz)
{
	size_t len;
	int error, c;
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
		printf("address not page aligned %#lx\n", (uintptr_t)pa);
		return (EINVAL);
	}
	if (ptr != NULL) {
		/*
		 * If we're doing a virtual dump, flush any
		 * pre-existing pa pages.
		 */
		error = blk_flush(di);
		if (error != 0)
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
			if (error != 0)
				return (error);
			ptr += len;
			sz -= len;
		} else {
			dump_va = (void *)PHYS_TO_DMAP(pa);
			fragsz += len;
			pa += len;
			sz -= len;
			error = blk_flush(di);
			if (error != 0)
				return (error);
		}

		/* Check for user abort */
		c = cncheckc();
		if (c == 0x03)
			return (ECANCELED);
		if (c != -1)
			printf(" (CTRL-C to abort) ");
	}

	return (0);
}

int
cpu_minidumpsys(struct dumperinfo *di, const struct minidumpstate *state)
{
	pd_entry_t *l1, *l2, l2e;
	pt_entry_t *l3, l3e;
	struct minidumphdr mdhdr;
	struct msgbuf *mbp;
	uint32_t pmapsize;
	vm_offset_t va, kva_max;
	vm_paddr_t pa;
	int error;
	int i;
	int retry_count;

	retry_count = 0;
retry:
	retry_count++;
	error = 0;
	pmapsize = 0;

	/* Snapshot the KVA upper bound in case it grows. */
	kva_max = kernel_vm_end;

	/*
	 * Walk the kernel page table pages, setting the active entries in the
	 * dump bitmap.
	 *
	 * NB: for a live dump, we may be racing with updates to the page
	 * tables, so care must be taken to read each entry only once.
	 */
	for (va = VM_MIN_KERNEL_ADDRESS; va < kva_max; va += L2_SIZE) {
		pmapsize += PAGE_SIZE;
		if (!pmap_get_tables(pmap_kernel(), va, &l1, &l2, &l3))
			continue;

		/* We should always be using the l2 table for kvm */
		if (l2 == NULL)
			continue;

		/* l2 may be a superpage */
		l2e = atomic_load_64(l2);
		if ((l2e & PTE_RWX) != 0) {
			pa = (l2e >> PTE_PPN1_S) << L2_SHIFT;
			for (i = 0; i < Ln_ENTRIES; i++, pa += PAGE_SIZE) {
				if (vm_phys_is_dumpable(pa))
					vm_page_dump_add(state->dump_bitset,
					    pa);
			}
		} else {
			for (i = 0; i < Ln_ENTRIES; i++) {
				l3e = atomic_load_64(&l3[i]);
				if ((l3e & PTE_V) == 0)
					continue;
				pa = (l3e >> PTE_PPN0_S) * PAGE_SIZE;
				if (PHYS_IN_DMAP(pa) && vm_phys_is_dumpable(pa))
					vm_page_dump_add(state->dump_bitset,
					    pa);
			}
		}
	}

	/* Calculate dump size */
	mbp = state->msgbufp;
	dumpsize = pmapsize;
	dumpsize += round_page(mbp->msg_size);
	dumpsize += round_page(sizeof(dump_avail));
	dumpsize += round_page(BITSET_SIZE(vm_page_dump_pages));
	VM_PAGE_DUMP_FOREACH(state->dump_bitset, pa) {
		/* Clear out undumpable pages now if needed */
		if (PHYS_IN_DMAP(pa) && vm_phys_is_dumpable(pa))
			dumpsize += PAGE_SIZE;
		else
			vm_page_dump_drop(state->dump_bitset, pa);
	}
	dumpsize += PAGE_SIZE;

	dumpsys_pb_init(dumpsize);

	/* Initialize mdhdr */
	bzero(&mdhdr, sizeof(mdhdr));
	strcpy(mdhdr.magic, MINIDUMP_MAGIC);
	mdhdr.version = MINIDUMP_VERSION;
	mdhdr.msgbufsize = mbp->msg_size;
	mdhdr.bitmapsize = round_page(BITSET_SIZE(vm_page_dump_pages));
	mdhdr.pmapsize = pmapsize;
	mdhdr.kernbase = KERNBASE;
	mdhdr.dmapphys = DMAP_MIN_PHYSADDR;
	mdhdr.dmapbase = DMAP_MIN_ADDRESS;
	mdhdr.dmapend = DMAP_MAX_ADDRESS;
	mdhdr.dumpavailsize = round_page(sizeof(dump_avail));

	dump_init_header(di, &kdh, KERNELDUMPMAGIC, KERNELDUMP_RISCV_VERSION,
	    dumpsize);

	error = dump_start(di, &kdh);
	if (error != 0)
		goto fail;

	printf("Dumping %llu out of %ju MB:", (long long)dumpsize >> 20,
	    ptoa((uintmax_t)physmem) / 1048576);

	/* Dump minidump header */
	bzero(&tmpbuffer, sizeof(tmpbuffer));
	bcopy(&mdhdr, &tmpbuffer, sizeof(mdhdr));
	error = blk_write(di, (char *)&tmpbuffer, 0, PAGE_SIZE);
	if (error)
		goto fail;

	/* Dump msgbuf up front */
	error = blk_write(di, mbp->msg_ptr, 0, round_page(mbp->msg_size));
	if (error)
		goto fail;

	/* Dump dump_avail */
	_Static_assert(sizeof(dump_avail) <= sizeof(tmpbuffer),
	    "Large dump_avail not handled");
	bzero(tmpbuffer, sizeof(tmpbuffer));
	memcpy(tmpbuffer, dump_avail, sizeof(dump_avail));
	error = blk_write(di, (char *)&tmpbuffer, 0, PAGE_SIZE);
	if (error)
		goto fail;

	/* Dump bitmap */
	error = blk_write(di, (char *)vm_page_dump, 0,
	    round_page(BITSET_SIZE(vm_page_dump_pages)));
	if (error)
		goto fail;

	/* Dump kernel page directory pages */
	bzero(&tmpbuffer, sizeof(tmpbuffer));
	for (va = VM_MIN_KERNEL_ADDRESS; va < kva_max; va += L2_SIZE) {
		if (!pmap_get_tables(pmap_kernel(), va, &l1, &l2, &l3)) {
			/* We always write a page, even if it is zero */
			error = blk_write(di, (char *)&tmpbuffer, 0, PAGE_SIZE);
			if (error)
				goto fail;
			/* Flush, in case we reuse tmpbuffer in the same block */
			error = blk_flush(di);
			if (error)
				goto fail;
			continue;
		}

		l2e = atomic_load_64(l2);
		if ((l2e & PTE_RWX) != 0) {
			/* Generate fake l3 entries based on the l2 superpage */
			for (i = 0; i < Ln_ENTRIES; i++) {
				tmpbuffer[i] = (l2e | (i << PTE_PPN0_S));
			}
			/* We always write a page, even if it is zero */
			error = blk_write(di, (char *)&tmpbuffer, 0, PAGE_SIZE);
			if (error)
				goto fail;
			/* Flush, in case we reuse tmpbuffer in the same block */
			error = blk_flush(di);
			if (error)
				goto fail;
			bzero(&tmpbuffer, sizeof(tmpbuffer));
		} else {
			pa = (l2e >> PTE_PPN0_S) * PAGE_SIZE;

			/*
			 * We always write a page, even if it is zero. If pa
			 * is malformed, write the zeroed tmpbuffer.
			 */
			if (PHYS_IN_DMAP(pa) && vm_phys_is_dumpable(pa))
				error = blk_write(di, NULL, pa, PAGE_SIZE);
			else
				error = blk_write(di, (char *)&tmpbuffer, 0,
				    PAGE_SIZE);
			if (error)
				goto fail;
		}
	}

	/* Dump memory chunks */
	/* XXX cluster it up and use blk_dump() */
	VM_PAGE_DUMP_FOREACH(state->dump_bitset, pa) {
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
		if (retry_count < 5) {
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
