/*-
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/cons.h>
#include <sys/kernel.h>
#include <sys/kerneldump.h>
#include <sys/msgbuf.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/atomic.h>
#include <machine/elf.h>
#include <machine/md_var.h>
#include <machine/vmparam.h>
#include <machine/minidump.h>

CTASSERT(sizeof(struct kerneldumpheader) == 512);

/*
 * Don't touch the first SIZEOF_METADATA bytes on the dump device. This
 * is to protect us from metadata and to protect metadata from us.
 */
#define	SIZEOF_METADATA		(64*1024)

#define	MD_ALIGN(x)	(((off_t)(x) + PAGE_MASK) & ~PAGE_MASK)
#define	DEV_ALIGN(x)	(((off_t)(x) + (DEV_BSIZE-1)) & ~(DEV_BSIZE-1))

extern uint64_t KPDPphys;

uint64_t *vm_page_dump;
int vm_page_dump_size;

static struct kerneldumpheader kdh;
static off_t dumplo;

/* Handle chunked writes. */
static size_t fragsz;
static void *dump_va;
static size_t counter, progress;

CTASSERT(sizeof(*vm_page_dump) == 8);

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

	error = dump_write(di, dump_va, 0, dumplo, fragsz);
	dumplo += fragsz;
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
		counter += len;
		progress -= len;
		if (counter >> 24) {
			printf(" %ld", PG2MB(progress >> PAGE_SHIFT));
			counter &= (1<<24) - 1;
		}
		if (ptr) {
			error = dump_write(di, ptr, 0, dumplo, len);
			if (error)
				return (error);
			dumplo += len;
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

void
minidumpsys(struct dumperinfo *di)
{
	uint64_t dumpsize;
	uint32_t pmapsize;
	vm_offset_t va;
	int error;
	uint64_t bits;
	uint64_t *pdp, *pd, *pt, pa;
	int i, j, k, n, bit;
	int retry_count;
	struct minidumphdr mdhdr;

	retry_count = 0;
 retry:
	retry_count++;
	counter = 0;
	/* Walk page table pages, set bits in vm_page_dump */
	pmapsize = 0;
	pdp = (uint64_t *)PHYS_TO_DMAP(KPDPphys);
	for (va = VM_MIN_KERNEL_ADDRESS; va < MAX(KERNBASE + NKPT * NBPDR,
	    kernel_vm_end); ) {
		/*
		 * We always write a page, even if it is zero. Each
		 * page written corresponds to 1GB of space
		 */
		pmapsize += PAGE_SIZE;
		i = (va >> PDPSHIFT) & ((1ul << NPDPEPGSHIFT) - 1);
		if ((pdp[i] & PG_V) == 0) {
			va += NBPDP;
			continue;
		}

		/*
		 * 1GB page is represented as 512 2MB pages in a dump.
		 */
		if ((pdp[i] & PG_PS) != 0) {
			va += NBPDP;
			pa = pdp[i] & PG_PS_FRAME;
			for (n = 0; n < NPDEPG * NPTEPG; n++) {
				if (is_dumpable(pa))
					dump_add_page(pa);
				pa += PAGE_SIZE;
			}
			continue;
		}

		pd = (uint64_t *)PHYS_TO_DMAP(pdp[i] & PG_FRAME);
		for (n = 0; n < NPDEPG; n++, va += NBPDR) {
			j = (va >> PDRSHIFT) & ((1ul << NPDEPGSHIFT) - 1);

			if ((pd[j] & PG_V) == 0)
				continue;

			if ((pd[j] & PG_PS) != 0) {
				/* This is an entire 2M page. */
				pa = pd[j] & PG_PS_FRAME;
				for (k = 0; k < NPTEPG; k++) {
					if (is_dumpable(pa))
						dump_add_page(pa);
					pa += PAGE_SIZE;
				}
				continue;
			}

			pa = pd[j] & PG_FRAME;
			/* set bit for this PTE page */
			if (is_dumpable(pa))
				dump_add_page(pa);
			/* and for each valid page in this 2MB block */
			pt = (uint64_t *)PHYS_TO_DMAP(pd[j] & PG_FRAME);
			for (k = 0; k < NPTEPG; k++) {
				if ((pt[k] & PG_V) == 0)
					continue;
				pa = pt[k] & PG_FRAME;
				if (is_dumpable(pa))
					dump_add_page(pa);
			}
		}
	}

	/* Calculate dump size. */
	dumpsize = pmapsize;
	dumpsize += round_page(msgbufp->msg_size);
	dumpsize += round_page(vm_page_dump_size);
	for (i = 0; i < vm_page_dump_size / sizeof(*vm_page_dump); i++) {
		bits = vm_page_dump[i];
		while (bits) {
			bit = bsfq(bits);
			pa = (((uint64_t)i * sizeof(*vm_page_dump) * NBBY) + bit) * PAGE_SIZE;
			/* Clear out undumpable pages now if needed */
			if (is_dumpable(pa)) {
				dumpsize += PAGE_SIZE;
			} else {
				dump_drop_page(pa);
			}
			bits &= ~(1ul << bit);
		}
	}
	dumpsize += PAGE_SIZE;

	/* Determine dump offset on device. */
	if (di->mediasize < SIZEOF_METADATA + dumpsize + sizeof(kdh) * 2) {
		error = E2BIG;
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
	mdhdr.pmapsize = pmapsize;
	mdhdr.kernbase = VM_MIN_KERNEL_ADDRESS;
	mdhdr.dmapbase = DMAP_MIN_ADDRESS;
	mdhdr.dmapend = DMAP_MAX_ADDRESS;

	mkdumpheader(&kdh, KERNELDUMPMAGIC, KERNELDUMP_AMD64_VERSION, dumpsize, di->blocksize);

	printf("Physical memory: %ju MB\n", ptoa((uintmax_t)physmem) / 1048576);
	printf("Dumping %llu MB:", (long long)dumpsize >> 20);

	/* Dump leader */
	error = dump_write(di, &kdh, 0, dumplo, sizeof(kdh));
	if (error)
		goto fail;
	dumplo += sizeof(kdh);

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

	/* Dump bitmap */
	error = blk_write(di, (char *)vm_page_dump, 0, round_page(vm_page_dump_size));
	if (error)
		goto fail;

	/* Dump kernel page directory pages */
	bzero(fakepd, sizeof(fakepd));
	pdp = (uint64_t *)PHYS_TO_DMAP(KPDPphys);
	for (va = VM_MIN_KERNEL_ADDRESS; va < MAX(KERNBASE + NKPT * NBPDR,
	    kernel_vm_end); va += NBPDP) {
		i = (va >> PDPSHIFT) & ((1ul << NPDPEPGSHIFT) - 1);

		/* We always write a page, even if it is zero */
		if ((pdp[i] & PG_V) == 0) {
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
		if ((pdp[i] & PG_PS) != 0) {
			/* PDPE and PDP have identical layout in this case */
			fakepd[0] = pdp[i];
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

		pd = (uint64_t *)PHYS_TO_DMAP(pdp[i] & PG_FRAME);
		error = blk_write(di, (char *)pd, 0, PAGE_SIZE);
		if (error)
			goto fail;
		error = blk_flush(di);
		if (error)
			goto fail;
	}

	/* Dump memory chunks */
	/* XXX cluster it up and use blk_dump() */
	for (i = 0; i < vm_page_dump_size / sizeof(*vm_page_dump); i++) {
		bits = vm_page_dump[i];
		while (bits) {
			bit = bsfq(bits);
			pa = (((uint64_t)i * sizeof(*vm_page_dump) * NBBY) + bit) * PAGE_SIZE;
			error = blk_write(di, 0, pa, PAGE_SIZE);
			if (error)
				goto fail;
			bits &= ~(1ul << bit);
		}
	}

	error = blk_flush(di);
	if (error)
		goto fail;

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
	else if (error == E2BIG)
		printf("Dump failed. Partition too small.\n");
	else
		printf("** DUMP FAILED (ERROR %d) **\n", error);
}

void
dump_add_page(vm_paddr_t pa)
{
	int idx, bit;

	pa >>= PAGE_SHIFT;
	idx = pa >> 6;		/* 2^6 = 64 */
	bit = pa & 63;
	atomic_set_long(&vm_page_dump[idx], 1ul << bit);
}

void
dump_drop_page(vm_paddr_t pa)
{
	int idx, bit;

	pa >>= PAGE_SHIFT;
	idx = pa >> 6;		/* 2^6 = 64 */
	bit = pa & 63;
	atomic_clear_long(&vm_page_dump[idx], 1ul << bit);
}
