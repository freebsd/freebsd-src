/*-
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
__FBSDID("$FreeBSD$");

#include "opt_watchdog.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/kernel.h>
#include <sys/kerneldump.h>
#include <sys/msgbuf.h>
#ifdef SW_WATCHDOG
#include <sys/watchdog.h>
#endif
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/pmap.h>
#include <machine/atomic.h>
#include <machine/elf.h>
#include <machine/md_var.h>
#include <machine/vmparam.h>
#include <machine/minidump.h>
#include <machine/cpufunc.h>

CTASSERT(sizeof(struct kerneldumpheader) == 512);

/*
 * Don't touch the first SIZEOF_METADATA bytes on the dump device. This
 * is to protect us from metadata and to protect metadata from us.
 */
#define	SIZEOF_METADATA		(64*1024)

uint32_t *vm_page_dump;
int vm_page_dump_size;

static struct kerneldumpheader kdh;
static off_t dumplo;

/* Handle chunked writes. */
static size_t fragsz, offset;
static void *dump_va;
static uint64_t counter, progress;

CTASSERT(sizeof(*vm_page_dump) == 4);

static int
is_dumpable(vm_paddr_t pa)
{
	int i;

	for (i = 0; dump_avail[i] != 0 || dump_avail[i + 1] != 0; i += 2) {
		if (pa >= dump_avail[i] && pa < dump_avail[i + 1])
			return (1);
	}
	return (0);
}

#define PG2MB(pgs) (((pgs) + (1 << 8) - 1) >> 8)

static int
blk_flush(struct dumperinfo *di)
{
	int error;

	if (fragsz == 0)
		return (0);

	error = dump_write(di, (char*)dump_va + offset, 0, dumplo, fragsz - offset);
	dumplo += (fragsz - offset);
	fragsz = 0;
	offset = 0;
	return (error);
}

static int
blk_write(struct dumperinfo *di, char *ptr, vm_paddr_t pa, size_t sz)
{
	size_t len;
	int error, i, c;
	u_int maxdumpsz;

	maxdumpsz = di->maxiosize;

	if (maxdumpsz == 0)	/* seatbelt */
		maxdumpsz = PAGE_SIZE;

	error = 0;

	if (ptr != NULL && pa != 0) {
		printf("cant have both va and pa!\n");
		return (EINVAL);
	}

	if (ptr != NULL) {
		/* If we're doing a virtual dump, flush any pre-existing pa pages */
		error = blk_flush(di);
		if (error)
			return (error);
	}

	while (sz) {
		if (fragsz == 0) {
			offset = pa & PAGE_MASK;
			fragsz += offset;
		}
		len = maxdumpsz - fragsz;
		if (len > sz)
			len = sz;
		counter += len;
		progress -= len;

		if (counter >> 22) {
			printf(" %lld", PG2MB(progress >> PAGE_SHIFT));
			counter &= (1<<22) - 1;
		}

#ifdef SW_WATCHDOG
		wdog_kern_pat(WD_LASTVAL);
#endif
		if (ptr) {
			error = dump_write(di, ptr, 0, dumplo, len);
			if (error)
				return (error);
			dumplo += len;
			ptr += len;
			sz -= len;
		} else {
			for (i = 0; i < len; i += PAGE_SIZE)
				dump_va = pmap_kenter_temp(pa + i,
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

static int
blk_write_cont(struct dumperinfo *di, vm_paddr_t pa, size_t sz)
{
	int error;

	error = blk_write(di, 0, pa, sz);
	if (error)
		return (error);

	error = blk_flush(di);
	if (error)
		return (error);

	return (0);
}

/* A fake page table page, to avoid having to handle both 4K and 2M pages */
static pt_entry_t fakept[NPTEPG];

void
minidumpsys(struct dumperinfo *di)
{
	struct minidumphdr mdhdr;
	uint64_t dumpsize;
	uint32_t ptesize;
	uint32_t bits;
	uint32_t pa, prev_pa = 0, count = 0;
	vm_offset_t va;
	pd_entry_t *pdp;
	pt_entry_t *pt, *ptp;
	int i, k, bit, error;
	char *addr;

	/* Flush cache */
	cpu_idcache_wbinv_all();
	cpu_l2cache_wbinv_all();

	counter = 0;
	/* Walk page table pages, set bits in vm_page_dump */
	ptesize = 0;
	for (va = KERNBASE; va < kernel_vm_end; va += NBPDR) {
		/*
		 * We always write a page, even if it is zero. Each
		 * page written corresponds to 2MB of space
		 */
		ptesize += L2_TABLE_SIZE_REAL;
		pmap_get_pde_pte(pmap_kernel(), va, &pdp, &ptp);
		if (pmap_pde_v(pdp) && pmap_pde_section(pdp)) {
			/* This is a section mapping 1M page. */
			pa = (*pdp & L1_S_ADDR_MASK) | (va & ~L1_S_ADDR_MASK);
			for (k = 0; k < (L1_S_SIZE / PAGE_SIZE); k++) {
				if (is_dumpable(pa))
					dump_add_page(pa);
				pa += PAGE_SIZE;
			}
			continue;
		}
		if (pmap_pde_v(pdp) && pmap_pde_page(pdp)) {
			/* Set bit for each valid page in this 1MB block */
			addr = pmap_kenter_temp(*pdp & L1_C_ADDR_MASK, 0);
			pt = (pt_entry_t*)(addr +
			    (((uint32_t)*pdp  & L1_C_ADDR_MASK) & PAGE_MASK));
			for (k = 0; k < 256; k++) {
				if ((pt[k] & L2_TYPE_MASK) == L2_TYPE_L) {
					pa = (pt[k] & L2_L_FRAME) |
					    (va & L2_L_OFFSET);
					for (i = 0; i < 16; i++) {
						if (is_dumpable(pa))
							dump_add_page(pa);
						k++;
						pa += PAGE_SIZE;
					}
				} else if ((pt[k] & L2_TYPE_MASK) == L2_TYPE_S) {
					pa = (pt[k] & L2_S_FRAME) |
					    (va & L2_S_OFFSET);
					if (is_dumpable(pa))
						dump_add_page(pa);
				}
			}
		} else {
			/* Nothing, we're going to dump a null page */
		}
	}

	/* Calculate dump size. */
	dumpsize = ptesize;
	dumpsize += round_page(msgbufp->msg_size);
	dumpsize += round_page(vm_page_dump_size);

	for (i = 0; i < vm_page_dump_size / sizeof(*vm_page_dump); i++) {
		bits = vm_page_dump[i];
		while (bits) {
			bit = ffs(bits) - 1;
			pa = (((uint64_t)i * sizeof(*vm_page_dump) * NBBY) +
			    bit) * PAGE_SIZE;
			/* Clear out undumpable pages now if needed */
			if (is_dumpable(pa))
				dumpsize += PAGE_SIZE;
			else
				dump_drop_page(pa);
			bits &= ~(1ul << bit);
		}
	}

	dumpsize += PAGE_SIZE;

	/* Determine dump offset on device. */
	if (di->mediasize < SIZEOF_METADATA + dumpsize + sizeof(kdh) * 2) {
		error = ENOSPC;
		goto fail;
	}

	dumplo = di->mediaoffset + di->mediasize - dumpsize;
	dumplo -= sizeof(kdh) * 2;
	progress = dumpsize;

	/* Initialize mdhdr */
	bzero(&mdhdr, sizeof(mdhdr));
	strcpy(mdhdr.magic, MINIDUMP_MAGIC);
	mdhdr.version = MINIDUMP_VERSION;
	mdhdr.msgbufsize = msgbufp->msg_size;
	mdhdr.bitmapsize = vm_page_dump_size;
	mdhdr.ptesize = ptesize;
	mdhdr.kernbase = KERNBASE;

	mkdumpheader(&kdh, KERNELDUMPMAGIC, KERNELDUMP_ARM_VERSION, dumpsize,
	    di->blocksize);

	printf("Physical memory: %u MB\n", ptoa((uintmax_t)physmem) / 1048576);
	printf("Dumping %llu MB:", (long long)dumpsize >> 20);

	/* Dump leader */
	error = dump_write(di, &kdh, 0, dumplo, sizeof(kdh));
	if (error)
		goto fail;
	dumplo += sizeof(kdh);

	/* Dump my header */
	bzero(&fakept, sizeof(fakept));
	bcopy(&mdhdr, &fakept, sizeof(mdhdr));
	error = blk_write(di, (char *)&fakept, 0, PAGE_SIZE);
	if (error)
		goto fail;

	/* Dump msgbuf up front */
	error = blk_write(di, (char *)msgbufp->msg_ptr, 0, round_page(msgbufp->msg_size));
	if (error)
		goto fail;

	/* Dump bitmap */
	error = blk_write(di, (char *)vm_page_dump, 0,
	    round_page(vm_page_dump_size));
	if (error)
		goto fail;

	/* Dump kernel page table pages */
	for (va = KERNBASE; va < kernel_vm_end; va += NBPDR) {
		/* We always write a page, even if it is zero */
		pmap_get_pde_pte(pmap_kernel(), va, &pdp, &ptp);

		if (pmap_pde_v(pdp) && pmap_pde_section(pdp))  {
			if (count) {
				error = blk_write_cont(di, prev_pa,
				    count * L2_TABLE_SIZE_REAL);
				if (error)
					goto fail;
				count = 0;
				prev_pa = 0;
			}
			/* This is a single 2M block. Generate a fake PTP */
			pa = (*pdp & L1_S_ADDR_MASK) | (va & ~L1_S_ADDR_MASK);
			for (k = 0; k < (L1_S_SIZE / PAGE_SIZE); k++) {
				fakept[k] = L2_S_PROTO | (pa + (k * PAGE_SIZE)) |
				    L2_S_PROT(PTE_KERNEL,
				    VM_PROT_READ | VM_PROT_WRITE);
			}
			error = blk_write(di, (char *)&fakept, 0,
			    L2_TABLE_SIZE_REAL);
			if (error)
				goto fail;
			/* Flush, in case we reuse fakept in the same block */
			error = blk_flush(di);
			if (error)
				goto fail;
			continue;
		}
		if (pmap_pde_v(pdp) && pmap_pde_page(pdp)) {
			pa = *pdp & L1_C_ADDR_MASK;
			if (!count) {
				prev_pa = pa;
				count++;
			}
			else {
				if (pa == (prev_pa + count * L2_TABLE_SIZE_REAL))
					count++;
				else {
					error = blk_write_cont(di, prev_pa,
					    count * L2_TABLE_SIZE_REAL);
					if (error)
						goto fail;
					count = 1;
					prev_pa = pa;
				}
			}
		} else {
			if (count) {
				error = blk_write_cont(di, prev_pa,
				    count * L2_TABLE_SIZE_REAL);
				if (error)
					goto fail;
				count = 0;
				prev_pa = 0;
			}
			bzero(fakept, sizeof(fakept));
			error = blk_write(di, (char *)&fakept, 0,
			    L2_TABLE_SIZE_REAL);
			if (error)
				goto fail;
			/* Flush, in case we reuse fakept in the same block */
			error = blk_flush(di);
			if (error)
				goto fail;
		}
	}

	if (count) {
		error = blk_write_cont(di, prev_pa, count * L2_TABLE_SIZE_REAL);
		if (error)
			goto fail;
		count = 0;
		prev_pa = 0;
	}

	/* Dump memory chunks */
	for (i = 0; i < vm_page_dump_size / sizeof(*vm_page_dump); i++) {
		bits = vm_page_dump[i];
		while (bits) {
			bit = ffs(bits) - 1;
			pa = (((uint64_t)i * sizeof(*vm_page_dump) * NBBY) +
			    bit) * PAGE_SIZE;
			if (!count) {
				prev_pa = pa;
				count++;
			} else {
				if (pa == (prev_pa + count * PAGE_SIZE))
					count++;
				else {
					error = blk_write_cont(di, prev_pa,
					    count * PAGE_SIZE);
					if (error)
						goto fail;
					count = 1;
					prev_pa = pa;
				}
			}
			bits &= ~(1ul << bit);
		}
	}
	if (count) {
		error = blk_write_cont(di, prev_pa, count * PAGE_SIZE);
		if (error)
			goto fail;
		count = 0;
		prev_pa = 0;
	}

	/* Dump trailer */
	error = dump_write(di, &kdh, 0, dumplo, sizeof(kdh));
	if (error)
		goto fail;
	dumplo += sizeof(kdh);

	/* Signal completion, signoff and exit stage left. */
	dump_write(di, NULL, 0, 0, 0);
	printf("\nDump complete\n");
	return;

fail:
	if (error < 0)
		error = -error;

	if (error == ECANCELED)
		printf("\nDump aborted\n");
	else if (error == ENOSPC)
		printf("\nDump failed. Partition too small.\n");
	else
		printf("\n** DUMP FAILED (ERROR %d) **\n", error);
}

void
dump_add_page(vm_paddr_t pa)
{
	int idx, bit;

	pa >>= PAGE_SHIFT;
	idx = pa >> 5;		/* 2^5 = 32 */
	bit = pa & 31;
	atomic_set_int(&vm_page_dump[idx], 1ul << bit);
}

void
dump_drop_page(vm_paddr_t pa)
{
	int idx, bit;

	pa >>= PAGE_SHIFT;
	idx = pa >> 5;		/* 2^5 = 32 */
	bit = pa & 31;
	atomic_clear_int(&vm_page_dump[idx], 1ul << bit);
}
