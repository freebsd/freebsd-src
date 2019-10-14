/*-
 * Copyright (c) 2019 Leandro Lupori
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
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/param.h>

#include <sys/cons.h>
#include <sys/kerneldump.h>
#include <sys/msgbuf.h>
#include <sys/proc.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_page.h>
#include <vm/vm_phys.h>
#include <vm/pmap.h>

#include <machine/atomic.h>
#include <machine/dump.h>
#include <machine/md_var.h>
#include <machine/minidump.h>

/*
 * bit to physical address
 *
 * bm - bitmap
 * i - bitmap entry index
 * bit - bit number
 */
#define BTOP(bm, i, bit) \
	(((uint64_t)(i) * sizeof(*(bm)) * NBBY + (bit)) * PAGE_SIZE)

/* Debugging stuff */
#define	MINIDUMP_DEBUG	0
#if	MINIDUMP_DEBUG
#define dprintf(fmt, ...)	printf(fmt, ## __VA_ARGS__)
#define DBG(...)	__VA_ARGS__
static size_t total, dumptotal;
static void dump_total(const char *id, size_t sz);
#else
#define dprintf(fmt, ...)
#define DBG(...)
#define dump_total(...)
#endif


extern vm_offset_t __startkernel, __endkernel;

int vm_page_dump_size;
uint64_t *vm_page_dump;

static int dump_retry_count = 5;
SYSCTL_INT(_machdep, OID_AUTO, dump_retry_count, CTLFLAG_RWTUN,
    &dump_retry_count, 0,
    "Number of times dump has to retry before bailing out");

static struct kerneldumpheader kdh;
static char pgbuf[PAGE_SIZE];

static struct {
	int min_per;
	int max_per;
	int visited;
} progress_track[10] = {
	{  0,  10, 0},
	{ 10,  20, 0},
	{ 20,  30, 0},
	{ 30,  40, 0},
	{ 40,  50, 0},
	{ 50,  60, 0},
	{ 60,  70, 0},
	{ 70,  80, 0},
	{ 80,  90, 0},
	{ 90, 100, 0}
};

static size_t counter, dumpsize, progress;

/* Handle chunked writes. */
static size_t fragsz;

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

int
is_dumpable(vm_paddr_t pa)
{
	vm_page_t m;
	int i;

	if ((m = vm_phys_paddr_to_vm_page(pa)) != NULL)
		return ((m->flags & PG_NODUMP) == 0);
	for (i = 0; dump_avail[i] != 0 || dump_avail[i + 1] != 0; i += 2) {
		if (pa >= dump_avail[i] && pa < dump_avail[i + 1])
			return (1);
	}
	return (0);
}

static void
pmap_kenter_temporary(vm_offset_t va, vm_paddr_t pa)
{
	pmap_kremove(va);
	pmap_kenter(va, pa);
}

static void
report_progress(void)
{
	int sofar, i;

	sofar = 100 - ((progress * 100) / dumpsize);
	for (i = 0; i < nitems(progress_track); i++) {
		if (sofar < progress_track[i].min_per ||
		    sofar > progress_track[i].max_per)
			continue;
		if (progress_track[i].visited)
			return;
		progress_track[i].visited = 1;
		printf("..%d%%", sofar);
		return;
	}
}

static int
blk_flush(struct dumperinfo *di)
{
	int error;

	if (fragsz == 0)
		return (0);

	error = dump_append(di, crashdumpmap, 0, fragsz);
	DBG(dumptotal += fragsz;)
	fragsz = 0;
	return (error);
}

static int
blk_write(struct dumperinfo *di, char *ptr, vm_paddr_t pa, size_t sz)
{
	size_t len, maxdumpsz;
	int error, i, c;

	maxdumpsz = MIN(di->maxiosize, MAXDUMPPGS * PAGE_SIZE);
	if (maxdumpsz == 0)	/* seatbelt */
		maxdumpsz = PAGE_SIZE;
	error = 0;
	if ((sz % PAGE_SIZE) != 0) {
		printf("Size not page aligned\n");
		return (EINVAL);
	}
	if (ptr != NULL && pa != 0) {
		printf("Can't have both va and pa!\n");
		return (EINVAL);
	}
	if ((pa % PAGE_SIZE) != 0) {
		printf("Address not page aligned 0x%lx\n", pa);
		return (EINVAL);
	}
	if (ptr != NULL) {
		/*
		 * If we're doing a virtual dump, flush any pre-existing
		 * pa pages
		 */
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
		if (counter >> 20) {
			report_progress();
			counter &= (1<<20) - 1;
		}

		if (ptr) {
			error = dump_append(di, ptr, 0, len);
			if (error)
				return (error);
			DBG(dumptotal += len;)
			ptr += len;
		} else {
			for (i = 0; i < len; i += PAGE_SIZE)
				pmap_kenter_temporary(
				    (vm_offset_t)crashdumpmap + fragsz + i,
				    pa + i);

			fragsz += len;
			pa += len;
			if (fragsz == maxdumpsz) {
				error = blk_flush(di);
				if (error)
					return (error);
			}
		}
		sz -= len;

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
dump_pmap(struct dumperinfo *di)
{
	void *ctx;
	char *buf;
	u_long nbytes;
	int error;

	ctx = dumpsys_dump_pmap_init(sizeof(pgbuf) / PAGE_SIZE);

	for (;;) {
		buf = dumpsys_dump_pmap(ctx, pgbuf, &nbytes);
		if (buf == NULL)
			break;
		error = blk_write(di, buf, 0, nbytes);
		if (error)
			return (error);
	}

	return (0);
}

int
minidumpsys(struct dumperinfo *di)
{
	vm_paddr_t pa;
	int bit, error, i, retry_count;
	uint32_t pmapsize;
	uint64_t bits;
	struct minidumphdr mdhdr;

	retry_count = 0;
retry:
	retry_count++;
	fragsz = 0;
	DBG(total = dumptotal = 0;)

	/* Reset progress */
	counter = 0;
	for (i = 0; i < nitems(progress_track); i++)
		progress_track[i].visited = 0;

	/* Build set of dumpable pages from kernel pmap */
	pmapsize = dumpsys_scan_pmap();
	if (pmapsize % PAGE_SIZE != 0) {
		printf("pmapsize not page aligned: 0x%x\n", pmapsize);
		return (EINVAL);
	}

	/* Calculate dump size */
	dumpsize = PAGE_SIZE;				/* header */
	dumpsize += round_page(msgbufp->msg_size);
	dumpsize += round_page(vm_page_dump_size);
	dumpsize += pmapsize;
	for (i = 0; i < vm_page_dump_size / sizeof(*vm_page_dump); i++) {
		bits = vm_page_dump[i];
		/* TODO optimize with bit manipulation instructions */
		if (bits == 0)
			continue;
		for (bit = 0; bit < 64; bit++) {
			if ((bits & (1ul<<bit)) == 0)
				continue;

			pa = BTOP(vm_page_dump, i, bit);
			/* Clear out undumpable pages now if needed */
			if (is_dumpable(pa))
				dumpsize += PAGE_SIZE;
			else
				dump_drop_page(pa);
		}
	}
	progress = dumpsize;

	/* Initialize mdhdr */
	bzero(&mdhdr, sizeof(mdhdr));
	strcpy(mdhdr.magic, MINIDUMP_MAGIC);
	strncpy(mdhdr.mmu_name, pmap_mmu_name(), sizeof(mdhdr.mmu_name) - 1);
	mdhdr.version = MINIDUMP_VERSION;
	mdhdr.msgbufsize = msgbufp->msg_size;
	mdhdr.bitmapsize = vm_page_dump_size;
	mdhdr.pmapsize = pmapsize;
	mdhdr.kernbase = VM_MIN_KERNEL_ADDRESS;
	mdhdr.kernend = VM_MAX_SAFE_KERNEL_ADDRESS;
	mdhdr.dmapbase = DMAP_BASE_ADDRESS;
	mdhdr.dmapend = DMAP_MAX_ADDRESS;
	mdhdr.hw_direct_map = hw_direct_map;
	mdhdr.startkernel = __startkernel;
	mdhdr.endkernel = __endkernel;

	dump_init_header(di, &kdh, KERNELDUMPMAGIC, KERNELDUMP_POWERPC_VERSION,
	    dumpsize);

	error = dump_start(di, &kdh);
	if (error)
		goto fail;

	printf("Dumping %lu out of %ju MB:", dumpsize >> 20,
	    ptoa((uintmax_t)physmem) / 1048576);

	/* Dump minidump header */
	bzero(pgbuf, sizeof(pgbuf));
	memcpy(pgbuf, &mdhdr, sizeof(mdhdr));
	error = blk_write(di, pgbuf, 0, PAGE_SIZE);
	if (error)
		goto fail;
	dump_total("header", PAGE_SIZE);

	/* Dump msgbuf up front */
	error = blk_write(di, (char *)msgbufp->msg_ptr, 0,
	    round_page(msgbufp->msg_size));
	dump_total("msgbuf", round_page(msgbufp->msg_size));

	/* Dump bitmap */
	error = blk_write(di, (char *)vm_page_dump, 0,
	    round_page(vm_page_dump_size));
	if (error)
		goto fail;
	dump_total("bitmap", round_page(vm_page_dump_size));

	/* Dump kernel page directory pages */
	error = dump_pmap(di);
	if (error)
		goto fail;
	dump_total("pmap", pmapsize);

	/* Dump memory chunks */
	/* XXX cluster it up and use blk_dump() */
	for (i = 0; i < vm_page_dump_size / sizeof(*vm_page_dump); i++) {
		bits = vm_page_dump[i];
		/* TODO optimize with bit manipulation instructions */
		if (bits == 0)
			continue;
		for (bit = 0; bit < 64; bit++) {
			if ((bits & (1ul<<bit)) == 0)
				continue;

			pa = BTOP(vm_page_dump, i, bit);
			error = blk_write(di, 0, pa, PAGE_SIZE);
			if (error)
				goto fail;
		}
	}

	error = blk_flush(di);
	if (error)
		goto fail;
	dump_total("mem_chunks", dumpsize - total);

	error = dump_finish(di, &kdh);
	if (error)
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
	} else if (error == ECANCELED)
		printf("Dump aborted\n");
	else if (error == E2BIG)
		printf("Dump failed. Partition too small.\n");
	else
		printf("** DUMP FAILED (ERROR %d) **\n", error);
	return (error);
}

#if	MINIDUMP_DEBUG
static void
dump_total(const char *id, size_t sz)
{
	total += sz;
	dprintf("\n%s=%08lx/%08lx/%08lx\n",
		id, sz, total, dumptotal);
}
#endif
