/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2014 Ian Lepore <ian@freebsd.org>
 * All rights reserved.
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
 */

#include <sys/cdefs.h>
#ifdef _KERNEL
#include "opt_acpi.h"
#include "opt_ddb.h"
#endif

/*
 * Routines for describing and initializing anything related to physical memory.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/physmem.h>

#ifdef _KERNEL
#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_page.h>
#include <vm/vm_phys.h>
#include <vm/vm_dumpset.h>

#include <machine/md_var.h>
#include <machine/resource.h>
#else
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#endif

/*
 * These structures are used internally to keep track of regions of physical
 * ram, and regions within the physical ram that need to be excluded.  An
 * exclusion region can be excluded from crash dumps, from the vm pool of pages
 * that can be allocated, or both, depending on the exclusion flags associated
 * with the region.
 */
#ifdef DEV_ACPI
#define	MAX_HWCNT	32	/* ACPI needs more regions */
#define	MAX_EXCNT	32
#else
#define	MAX_HWCNT	16
#define	MAX_EXCNT	16
#endif

#if defined(__arm__)
#define	MAX_PHYS_ADDR	0xFFFFFFFFull
#elif defined(__aarch64__) || defined(__amd64__) || defined(__riscv)
#define	MAX_PHYS_ADDR	0xFFFFFFFFFFFFFFFFull
#endif

struct region {
	vm_paddr_t	addr;
	vm_size_t	size;
	uint32_t	flags;
};

static struct region hwregions[MAX_HWCNT];
static struct region exregions[MAX_EXCNT];

static size_t hwcnt;
static size_t excnt;

/*
 * realmem is the total number of hardware pages, excluded or not.
 * Maxmem is one greater than the last physical page number.
 */
long realmem;
long Maxmem;

#ifndef _KERNEL
static void
panic(const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	vfprintf(stderr, fmt, va);
	fprintf(stderr, "\n");
	va_end(va);
	__builtin_trap();
}
#endif

/*
 * Print the contents of the physical and excluded region tables using the
 * provided printf-like output function (which will be either printf or
 * db_printf).
 */
static void
physmem_dump_tables(int (*prfunc)(const char *, ...) __printflike(1, 2))
{
	size_t i;
	int flags;
	uintmax_t addr, size;
	const unsigned int mbyte = 1024 * 1024;

	prfunc("Physical memory chunk(s):\n");
	for (i = 0; i < hwcnt; ++i) {
		addr = hwregions[i].addr;
		size = hwregions[i].size;
		prfunc("  0x%08jx - 0x%08jx, %5ju MB (%7ju pages)\n", addr,
		    addr + size - 1, size / mbyte, size / PAGE_SIZE);
	}

	prfunc("Excluded memory regions:\n");
	for (i = 0; i < excnt; ++i) {
		addr  = exregions[i].addr;
		size  = exregions[i].size;
		flags = exregions[i].flags;
		prfunc("  0x%08jx - 0x%08jx, %5ju MB (%7ju pages) %s %s\n",
		    addr, addr + size - 1, size / mbyte, size / PAGE_SIZE,
		    (flags & EXFLAG_NOALLOC) ? "NoAlloc" : "",
		    (flags & EXFLAG_NODUMP)  ? "NoDump" : "");
	}

#ifdef DEBUG
	prfunc("Avail lists:\n");
	for (i = 0; phys_avail[i] != 0; ++i) {
		prfunc("  phys_avail[%zu] 0x%08jx\n", i,
		    (uintmax_t)phys_avail[i]);
	}
	for (i = 0; dump_avail[i] != 0; ++i) {
		prfunc("  dump_avail[%zu] 0x%08jx\n", i,
		    (uintmax_t)dump_avail[i]);
	}
#endif
}

/*
 * Print the contents of the static mapping table.  Used for bootverbose.
 */
void
physmem_print_tables(void)
{

	physmem_dump_tables(printf);
}

/*
 * Walk the list of hardware regions, processing it against the list of
 * exclusions that contain the given exflags, and generating an "avail list".
 * 
 * If maxphyssz is not zero it sets upper limit, in bytes, for the total
 * "avail list" size. Walk stops once the limit is reached and the last region
 * is cut short if necessary.
 *
 * Updates the value at *pavail with the sum of all pages in all hw regions.
 *
 * Returns the number of entries in the avail list, which is twice the number
 * of returned regions.
 */
static size_t
regions_to_avail(vm_paddr_t *avail, uint32_t exflags, size_t maxavail,
    uint64_t maxphyssz, long *pavail, long *prealmem)
{
	size_t acnt, exi, hwi;
	uint64_t adj, end, start, xend, xstart;
	long availmem, totalmem;
	const struct region *exp, *hwp;
	uint64_t availsz;

	bzero(avail, maxavail * sizeof(vm_paddr_t));

	totalmem = 0;
	availmem = 0;
	availsz = 0;
	acnt = 0;
	for (hwi = 0, hwp = hwregions; hwi < hwcnt; ++hwi, ++hwp) {
		adj   = round_page(hwp->addr) - hwp->addr;
		start = round_page(hwp->addr);
		end   = trunc_page(hwp->size + adj) + start;
		totalmem += atop((vm_offset_t)(end - start));
		for (exi = 0, exp = exregions; exi < excnt; ++exi, ++exp) {
			/*
			 * If the excluded region does not match given flags,
			 * continue checking with the next excluded region.
			 */
			if ((exp->flags & exflags) == 0)
				continue;
			xstart = exp->addr;
			xend   = exp->size + xstart;
			/*
			 * If the excluded region ends before this hw region,
			 * continue checking with the next excluded region.
			 */
			if (xend <= start)
				continue;
			/*
			 * If the excluded region begins after this hw region
			 * we're done because both lists are sorted.
			 */
			if (xstart >= end)
				break;
			/*
			 * If the excluded region completely covers this hw
			 * region, shrink this hw region to zero size.
			 */
			if ((start >= xstart) && (end <= xend)) {
				start = xend;
				end = xend;
				break;
			}
			/*
			 * If the excluded region falls wholly within this hw
			 * region without abutting or overlapping the beginning
			 * or end, create an available entry from the leading
			 * fragment, then adjust the start of this hw region to
			 * the end of the excluded region, and continue checking
			 * the next excluded region because another exclusion
			 * could affect the remainder of this hw region.
			 */
			if ((xstart > start) && (xend < end)) {

				if ((maxphyssz != 0) &&
				    (availsz + xstart - start > maxphyssz)) {
					xstart = maxphyssz + start - availsz;
				}
				if (xstart <= start)
					continue;
				if (acnt > 0 &&
				    avail[acnt - 1] == (vm_paddr_t)start) {
					avail[acnt - 1] = (vm_paddr_t)xstart;
				} else {
					avail[acnt++] = (vm_paddr_t)start;
					avail[acnt++] = (vm_paddr_t)xstart;
				}
				availsz += (xstart - start);
				availmem += atop((vm_offset_t)(xstart - start));
				start = xend;
				continue;
			}
			/*
			 * We know the excluded region overlaps either the start
			 * or end of this hardware region (but not both), trim
			 * the excluded portion off the appropriate end.
			 */
			if (xstart <= start)
				start = xend;
			else
				end = xstart;
		}
		/*
		 * If the trimming actions above left a non-zero size, create an
		 * available entry for it.
		 */
		if (end > start) {
			if ((maxphyssz != 0) &&
			    (availsz + end - start > maxphyssz)) {
				end = maxphyssz + start - availsz;
			}
			if (end <= start)
				break;

			if (acnt > 0 && avail[acnt - 1] == (vm_paddr_t)start) {
				avail[acnt - 1] = (vm_paddr_t)end;
			} else {
				avail[acnt++] = (vm_paddr_t)start;
				avail[acnt++] = (vm_paddr_t)end;
			}
			availsz += end - start;
			availmem += atop((vm_offset_t)(end - start));
		}
		if (acnt >= maxavail)
			panic("Not enough space in the dump/phys_avail arrays");
	}

	if (pavail != NULL)
		*pavail = availmem;
	if (prealmem != NULL)
		*prealmem = totalmem;
	return (acnt);
}

/*
 * Check if the region at idx can be merged with the region above it.
 */
static size_t
merge_upper_regions(struct region *regions, size_t rcnt, size_t idx)
{
	struct region *lower, *upper;
	vm_paddr_t lend, uend;
	size_t i, mergecnt, movecnt;

	lower = &regions[idx];
	lend = lower->addr + lower->size;

	/*
	 * Continue merging in upper entries as long as we have entries to
	 * merge; the new block could have spanned more than one, although one
	 * is likely the common case.
	 */
	for (i = idx + 1; i < rcnt; i++) {
		upper = &regions[i];
		if (lend < upper->addr || lower->flags != upper->flags)
			break;

		uend = upper->addr + upper->size;
		if (uend > lend) {
			lower->size += uend - lend;
			lend = lower->addr + lower->size;
		}

		if (uend >= lend) {
			/*
			 * If we didn't move past the end of the upper region,
			 * then we don't need to bother checking for another
			 * merge because it would have been done already.  Just
			 * increment i once more to maintain the invariant that
			 * i is one past the last entry merged.
			 */
			i++;
			break;
		}
	}

	/*
	 * We merged in the entries from [idx + 1, i); physically move the tail
	 * end at [i, rcnt) if we need to.
	 */
	mergecnt = i - (idx + 1);
	if (mergecnt > 0) {
		movecnt = rcnt - i;
		if (movecnt == 0) {
			/* Merged all the way to the end, just decrease rcnt. */
			rcnt = idx + 1;
		} else {
			memmove(&regions[idx + 1], &regions[idx + mergecnt + 1],
			    movecnt * sizeof(*regions));
			rcnt -= mergecnt;
		}
	}
	return (rcnt);
}

/*
 * Insertion-sort a new entry into a regions list; sorted by start address.
 */
static size_t
insert_region(struct region *regions, size_t rcnt, vm_paddr_t addr,
    vm_size_t size, uint32_t flags)
{
	size_t i;
	vm_paddr_t nend, rend;
	struct region *ep, *rp;

	nend = addr + size;
	ep = regions + rcnt;
	for (i = 0, rp = regions; i < rcnt; ++i, ++rp) {
		rend = rp->addr + rp->size;
		if (flags == rp->flags) {
			if (addr <= rp->addr && nend >= rp->addr) {
				/*
				 * New mapping overlaps at the beginning, shift
				 * for any difference in the beginning then
				 * shift if the new mapping extends past.
				 */
				rp->size += rp->addr - addr;
				rp->addr = addr;
				if (nend > rend) {
					rp->size += nend - rend;
					rcnt = merge_upper_regions(regions,
					    rcnt, i);
				}
				return (rcnt);
			} else if (addr <= rend && nend > rp->addr) {
				/*
				 * New mapping is either entirely contained
				 * within or it's overlapping at the end.
				 */
				if (nend > rend) {
					rp->size += nend - rend;
					rcnt = merge_upper_regions(regions,
					    rcnt, i);
				}
				return (rcnt);
			}
		} else if ((flags != 0) && (rp->flags != 0)) {
			/*
			 * If we're duplicating an entry that already exists
			 * exactly, just upgrade its flags as needed.  We could
			 * do more if we find that we have differently specified
			 * flags clipping existing excluding regions, but that's
			 * probably rare.
			 */
			if (addr == rp->addr && nend == rend) {
				rp->flags |= flags;
				return (rcnt);
			}
		}

		if (addr < rp->addr) {
			bcopy(rp, rp + 1, (ep - rp) * sizeof(*rp));
			break;
		}
	}
	rp->addr  = addr;
	rp->size  = size;
	rp->flags = flags;
	rcnt++;

	return (rcnt);
}

/*
 * Add a hardware memory region.
 */
void
physmem_hardware_region(uint64_t pa, uint64_t sz)
{
	/*
	 * Filter out the page at PA 0x00000000.  The VM can't handle it, as
	 * pmap_extract() == 0 means failure.
	 */
	if (pa == 0) {
		if (sz <= PAGE_SIZE)
			return;
		pa  = PAGE_SIZE;
		sz -= PAGE_SIZE;
	} else if (pa > MAX_PHYS_ADDR) {
		/* This range is past usable memory, ignore it */
		return;
	}

	/*
	 * Also filter out the page at the end of the physical address space --
	 * if addr is non-zero and addr+size is zero we wrapped to the next byte
	 * beyond what vm_paddr_t can express.  That leads to a NULL pointer
	 * deref early in startup; work around it by leaving the last page out.
	 *
	 * XXX This just in:  subtract out a whole megabyte, not just 1 page.
	 * Reducing the size by anything less than 1MB results in the NULL
	 * pointer deref in _vm_map_lock_read().  Better to give up a megabyte
	 * than leave some folks with an unusable system while we investigate.
	 */
	if ((pa + sz) > (MAX_PHYS_ADDR - 1024 * 1024)) {
		sz = MAX_PHYS_ADDR - pa + 1;
		if (sz <= 1024 * 1024)
			return;
		sz -= 1024 * 1024;
	}

	if (sz > 0 && hwcnt < nitems(hwregions))
		hwcnt = insert_region(hwregions, hwcnt, pa, sz, 0);
}

/*
 * Add an exclusion region.
 */
void
physmem_exclude_region(vm_paddr_t pa, vm_size_t sz, uint32_t exflags)
{
	vm_offset_t adj;

	/*
	 * Truncate the starting address down to a page boundary, and round the
	 * ending page up to a page boundary.
	 */
	adj = pa - trunc_page(pa);
	pa  = trunc_page(pa);
	sz  = round_page(sz + adj);

	if (excnt >= nitems(exregions))
		panic("failed to exclude region %#jx-%#jx", (uintmax_t)pa,
		    (uintmax_t)(pa + sz));
	excnt = insert_region(exregions, excnt, pa, sz, exflags);
}

size_t
physmem_avail(vm_paddr_t *avail, size_t maxavail)
{

	return (regions_to_avail(avail, EXFLAG_NOALLOC, maxavail, 0, NULL, NULL));
}

bool
physmem_excluded(vm_paddr_t pa, vm_size_t sz)
{
	const struct region *exp;
	size_t exi;

	for (exi = 0, exp = exregions; exi < excnt; ++exi, ++exp) {
		if (pa < exp->addr || pa + sz > exp->addr + exp->size)
			continue;
		return (true);
	}
	return (false);
}

#ifdef _KERNEL
/*
 * Process all the regions added earlier into the global avail lists.
 *
 * Updates the kernel global 'physmem' with the number of physical pages
 * available for use (all pages not in any exclusion region).
 *
 * Updates the kernel global 'Maxmem' with the page number one greater then the
 * last page of physical memory in the system.
 */
void
physmem_init_kernel_globals(void)
{
	size_t nextidx;
	u_long hwphyssz;

	hwphyssz = 0;
	TUNABLE_ULONG_FETCH("hw.physmem", &hwphyssz);

	regions_to_avail(dump_avail, EXFLAG_NODUMP, PHYS_AVAIL_ENTRIES,
	    hwphyssz, NULL, NULL);
	nextidx = regions_to_avail(phys_avail, EXFLAG_NOALLOC,
	    PHYS_AVAIL_ENTRIES, hwphyssz, &physmem, &realmem);
	if (nextidx == 0)
		panic("No memory entries in phys_avail");
	Maxmem = atop(phys_avail[nextidx - 1]);
}

#ifdef DDB
#include <ddb/ddb.h>

DB_SHOW_COMMAND_FLAGS(physmem, db_show_physmem, DB_CMD_MEMSAFE)
{

	physmem_dump_tables(db_printf);
}

#endif /* DDB */

/*
 * ram pseudo driver - this reserves I/O space resources corresponding to physical
 * memory regions.
 */

static void
ram_identify(driver_t *driver, device_t parent)
{

	if (resource_disabled("ram", 0))
		return;
	if (BUS_ADD_CHILD(parent, 0, "ram", 0) == NULL)
		panic("ram_identify");
}

static int
ram_probe(device_t dev)
{

	device_quiet(dev);
	device_set_desc(dev, "System RAM");
	return (BUS_PROBE_SPECIFIC);
}

static int
ram_attach(device_t dev)
{
	vm_paddr_t avail_list[PHYS_AVAIL_COUNT];
	rman_res_t start, end;
	int rid, i;

	rid = 0;

	/* Get the avail list. */
	regions_to_avail(avail_list, EXFLAG_NOALLOC | EXFLAG_NODUMP,
	    PHYS_AVAIL_COUNT, 0, NULL, NULL);

	/* Reserve all memory regions. */
	for (i = 0; avail_list[i + 1] != 0; i += 2) {
		start = avail_list[i];
		end = avail_list[i + 1];

		if (bootverbose)
			device_printf(dev,
			    "reserving memory region:   %jx-%jx\n",
			    (uintmax_t)start, (uintmax_t)end);

		if (bus_alloc_resource(dev, SYS_RES_MEMORY, &rid, start, end,
		    end - start, 0) == NULL)
			panic("ram_attach: resource %d failed to attach", rid);
		rid++;
	}

	return (0);
}

static device_method_t ram_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	ram_identify),
	DEVMETHOD(device_probe,		ram_probe),
	DEVMETHOD(device_attach,	ram_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_0(ram, ram_driver, ram_methods, /* no softc */ 1);
EARLY_DRIVER_MODULE(ram, nexus, ram_driver, 0, 0,
    BUS_PASS_ROOT + BUS_PASS_ORDER_MIDDLE);
#endif /* _KERNEL */
