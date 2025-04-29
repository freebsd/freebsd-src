/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2006 Peter Wemm
 * Copyright (c) 2008 Semihalf, Grzegorz Bernacki
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
 *
 * from: FreeBSD: src/sys/i386/i386/minidump_machdep.c,v 1.6 2008/08/17 23:27:27
 */

#include <sys/cdefs.h>
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
#include <machine/cpu.h>
#include <machine/elf.h>
#include <machine/md_var.h>
#include <machine/minidump.h>

CTASSERT(sizeof(struct kerneldumpheader) == 512);

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

	error = dump_append(di, dump_va, fragsz);
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
	if (ptr != NULL && pa != 0) {
		printf("cant have both va and pa!\n");
		return (EINVAL);
	}
	if (pa != 0) {
		if ((sz % PAGE_SIZE) != 0) {
			printf("size not page aligned\n");
			return (EINVAL);
		}
		if ((pa & PAGE_MASK) != 0) {
			printf("address not page aligned\n");
			return (EINVAL);
		}
	}
	if (ptr != NULL) {
		/* Flush any pre-existing pa pages before a virtual dump. */
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
			error = dump_append(di, ptr, len);
			if (error)
				return (error);
			ptr += len;
			sz -= len;
		} else {
			for (i = 0; i < len; i += PAGE_SIZE)
				dump_va = pmap_kenter_temporary(pa + i,
				    (i + fragsz) >> PAGE_SHIFT);
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

/* A buffer for general use. Its size must be one page at least. */
static char dumpbuf[PAGE_SIZE] __aligned(sizeof(uint64_t));
CTASSERT(sizeof(dumpbuf) % sizeof(pt2_entry_t) == 0);

int
cpu_minidumpsys(struct dumperinfo *di, const struct minidumpstate *state)
{
	struct minidumphdr mdhdr;
	struct msgbuf *mbp;
	uint64_t dumpsize, *dump_avail_buf;
	uint32_t ptesize;
	uint32_t pa, prev_pa = 0, count = 0;
	vm_offset_t va, kva_end;
	int error, i;
	char *addr;

	/*
	 * Flush caches.  Note that in the SMP case this operates only on the
	 * current CPU's L1 cache.  Before we reach this point, code in either
	 * the system shutdown or kernel debugger has called stop_cpus() to stop
	 * all cores other than this one.  Part of the ARM handling of
	 * stop_cpus() is to call wbinv_all() on that core's local L1 cache.  So
	 * by time we get to here, all that remains is to flush the L1 for the
	 * current CPU, then the L2.
	 */
	dcache_wbinv_poc_all();

	/* Snapshot the KVA upper bound in case it grows. */
	kva_end = kernel_vm_end;

	/*
	 * Walk the kernel page table pages, setting the active entries in the
	 * dump bitmap.
	 */
	ptesize = 0;
	for (va = KERNBASE; va < kva_end; va += PAGE_SIZE) {
		pa = pmap_dump_kextract(va, NULL);
		if (pa != 0 && vm_phys_is_dumpable(pa))
			vm_page_dump_add(state->dump_bitset, pa);
		ptesize += sizeof(pt2_entry_t);
	}

	/* Calculate dump size. */
	mbp = state->msgbufp;
	dumpsize = ptesize;
	dumpsize += round_page(mbp->msg_size);
	dumpsize += round_page(nitems(dump_avail) * sizeof(uint64_t));
	dumpsize += round_page(BITSET_SIZE(vm_page_dump_pages));
	VM_PAGE_DUMP_FOREACH(state->dump_bitset, pa) {
		/* Clear out undumpable pages now if needed */
		if (vm_phys_is_dumpable(pa))
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
	mdhdr.ptesize = ptesize;
	mdhdr.kernbase = KERNBASE;
	mdhdr.arch = __ARM_ARCH;
	mdhdr.mmuformat = MINIDUMP_MMU_FORMAT_V6;
	mdhdr.dumpavailsize = round_page(nitems(dump_avail) * sizeof(uint64_t));

	dump_init_header(di, &kdh, KERNELDUMPMAGIC, KERNELDUMP_ARM_VERSION,
	    dumpsize);

	error = dump_start(di, &kdh);
	if (error != 0)
		goto fail;

	printf("Physical memory: %ju MB\n",
	    ptoa((uintmax_t)physmem) / 1048576);
	printf("Dumping %llu MB:", (long long)dumpsize >> 20);

	/* Dump my header */
	bzero(dumpbuf, sizeof(dumpbuf));
	bcopy(&mdhdr, dumpbuf, sizeof(mdhdr));
	error = blk_write(di, dumpbuf, 0, PAGE_SIZE);
	if (error)
		goto fail;

	/* Dump msgbuf up front */
	error = blk_write(di, mbp->msg_ptr, 0, round_page(mbp->msg_size));
	if (error)
		goto fail;

	/* Dump dump_avail.  Make a copy using 64-bit physical addresses. */
	_Static_assert(nitems(dump_avail) * sizeof(uint64_t) <= sizeof(dumpbuf),
	    "Large dump_avail not handled");
	bzero(dumpbuf, sizeof(dumpbuf));
	dump_avail_buf = (uint64_t *)dumpbuf;
	for (i = 0; dump_avail[i] != 0 || dump_avail[i + 1] != 0; i += 2) {
		dump_avail_buf[i] = dump_avail[i];
		dump_avail_buf[i + 1] = dump_avail[i + 1];
	}
	error = blk_write(di, dumpbuf, 0, PAGE_SIZE);
	if (error)
		goto fail;

	/* Dump bitmap */
	error = blk_write(di, (char *)state->dump_bitset, 0,
	    round_page(BITSET_SIZE(vm_page_dump_pages)));
	if (error)
		goto fail;

	/* Dump kernel page table pages */
	addr = dumpbuf;
	for (va = KERNBASE; va < kva_end; va += PAGE_SIZE) {
		pmap_dump_kextract(va, (pt2_entry_t *)addr);
		addr += sizeof(pt2_entry_t);
		if (addr == dumpbuf + sizeof(dumpbuf)) {
			error = blk_write(di, dumpbuf, 0, sizeof(dumpbuf));
			if (error != 0)
				goto fail;
			addr = dumpbuf;
		}
	}
	if (addr != dumpbuf) {
		error = blk_write(di, dumpbuf, 0, addr - dumpbuf);
		if (error != 0)
			goto fail;
	}

	/* Dump memory chunks */
	VM_PAGE_DUMP_FOREACH(state->dump_bitset, pa) {
		if (!count) {
			prev_pa = pa;
			count++;
		} else {
			if (pa == (prev_pa + count * PAGE_SIZE))
				count++;
			else {
				error = blk_write(di, NULL, prev_pa,
				    count * PAGE_SIZE);
				if (error)
					goto fail;
				count = 1;
				prev_pa = pa;
			}
		}
	}
	if (count) {
		error = blk_write(di, NULL, prev_pa, count * PAGE_SIZE);
		if (error)
			goto fail;
		count = 0;
		prev_pa = 0;
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
