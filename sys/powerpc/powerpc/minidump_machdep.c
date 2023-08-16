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
#include <vm/vm_dumpset.h>
#include <vm/pmap.h>

#include <machine/atomic.h>
#include <machine/dump.h>
#include <machine/md_var.h>
#include <machine/minidump.h>

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

static int dump_retry_count = 5;
SYSCTL_INT(_machdep, OID_AUTO, dump_retry_count, CTLFLAG_RWTUN,
    &dump_retry_count, 0,
    "Number of times dump has to retry before bailing out");

static struct kerneldumpheader kdh;
static char pgbuf[PAGE_SIZE];

static size_t dumpsize;

/* Handle chunked writes. */
static size_t fragsz;

static void
pmap_kenter_temporary(vm_offset_t va, vm_paddr_t pa)
{
	pmap_kremove(va);
	pmap_kenter(va, pa);
}

static int
blk_flush(struct dumperinfo *di)
{
	int error;

	if (fragsz == 0)
		return (0);

	error = dump_append(di, crashdumpmap, fragsz);
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

		dumpsys_pb_progress(len);

		if (ptr) {
			error = dump_append(di, ptr, len);
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
cpu_minidumpsys(struct dumperinfo *di, const struct minidumpstate *state)
{
	vm_paddr_t pa;
	int error, retry_count;
	uint32_t pmapsize;
	struct minidumphdr mdhdr;
	struct msgbuf *mbp;

	retry_count = 0;
retry:
	retry_count++;
	fragsz = 0;
	DBG(total = dumptotal = 0;)

	/* Build set of dumpable pages from kernel pmap */
	pmapsize = dumpsys_scan_pmap(state->dump_bitset);
	if (pmapsize % PAGE_SIZE != 0) {
		printf("pmapsize not page aligned: 0x%x\n", pmapsize);
		return (EINVAL);
	}

	/* Calculate dump size */
	mbp = state->msgbufp;
	dumpsize = PAGE_SIZE;				/* header */
	dumpsize += round_page(mbp->msg_size);
	dumpsize += round_page(sizeof(dump_avail));
	dumpsize += round_page(BITSET_SIZE(vm_page_dump_pages));
	dumpsize += pmapsize;
	VM_PAGE_DUMP_FOREACH(state->dump_bitset, pa) {
		/* Clear out undumpable pages now if needed */
		if (vm_phys_is_dumpable(pa))
			dumpsize += PAGE_SIZE;
		else
			vm_page_dump_drop(state->dump_bitset, pa);
	}
	dumpsys_pb_init(dumpsize);

	/* Initialize mdhdr */
	bzero(&mdhdr, sizeof(mdhdr));
	strcpy(mdhdr.magic, MINIDUMP_MAGIC);
	strncpy(mdhdr.mmu_name, pmap_mmu_name(), sizeof(mdhdr.mmu_name) - 1);
	mdhdr.version = MINIDUMP_VERSION;
	mdhdr.msgbufsize = mbp->msg_size;
	mdhdr.bitmapsize = round_page(BITSET_SIZE(vm_page_dump_pages));
	mdhdr.pmapsize = pmapsize;
	mdhdr.kernbase = VM_MIN_KERNEL_ADDRESS;
	mdhdr.kernend = VM_MAX_SAFE_KERNEL_ADDRESS;
	mdhdr.dmapbase = DMAP_BASE_ADDRESS;
	mdhdr.dmapend = DMAP_MAX_ADDRESS;
	mdhdr.hw_direct_map = hw_direct_map;
	mdhdr.startkernel = __startkernel;
	mdhdr.endkernel = __endkernel;
	mdhdr.dumpavailsize = round_page(sizeof(dump_avail));

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
	error = blk_write(di, mbp->msg_ptr, 0, round_page(mbp->msg_size));
	dump_total("msgbuf", round_page(mbp->msg_size));

	/* Dump dump_avail */
	_Static_assert(sizeof(dump_avail) <= sizeof(pgbuf),
	    "Large dump_avail not handled");
	bzero(pgbuf, sizeof(mdhdr));
	memcpy(pgbuf, dump_avail, sizeof(dump_avail));
	error = blk_write(di, pgbuf, 0, PAGE_SIZE);
	if (error)
		goto fail;
	dump_total("dump_avail", round_page(sizeof(dump_avail)));

	/* Dump bitmap */
	error = blk_write(di, (char *)vm_page_dump, 0,
	    round_page(BITSET_SIZE(vm_page_dump_pages)));
	if (error)
		goto fail;
	dump_total("bitmap", round_page(BITSET_SIZE(vm_page_dump_pages)));

	/* Dump kernel page directory pages */
	error = dump_pmap(di);
	if (error)
		goto fail;
	dump_total("pmap", pmapsize);

	/* Dump memory chunks */
	VM_PAGE_DUMP_FOREACH(state->dump_bitset, pa) {
		error = blk_write(di, 0, pa, PAGE_SIZE);
		if (error)
			goto fail;
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
