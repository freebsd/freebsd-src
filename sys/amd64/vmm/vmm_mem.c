/*-
 * Copyright (c) 2011 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/linker.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/md_var.h>
#include <machine/metadata.h>
#include <machine/pc/bios.h>
#include <machine/vmparam.h>
#include <machine/pmap.h>

#include "vmm_util.h"
#include "vmm_mem.h"

static MALLOC_DEFINE(M_VMM_MEM, "vmm memory", "vmm memory");

#define	MB		(1024 * 1024)
#define	GB		(1024 * MB)

#define	VMM_MEM_MAXSEGS	64

/* protected by vmm_mem_mtx */
static struct {
	vm_paddr_t	base;
	vm_size_t	length;
} vmm_mem_avail[VMM_MEM_MAXSEGS];

static int vmm_mem_nsegs;
size_t vmm_mem_total_bytes;

static vm_paddr_t maxaddr;

static struct mtx vmm_mem_mtx;

/*
 * Steal any memory that was deliberately hidden from FreeBSD either by
 * the use of MAXMEM kernel config option or the hw.physmem loader tunable.
 */
static int
vmm_mem_steal_memory(void)
{
	int nsegs;
	caddr_t kmdp;
	uint32_t smapsize;
	uint64_t base, length;
	struct bios_smap *smapbase, *smap, *smapend;

	/*
	 * Borrowed from hammer_time() and getmemsize() in machdep.c
	 */
	kmdp = preload_search_by_type("elf kernel");
	if (kmdp == NULL)
		kmdp = preload_search_by_type("elf64 kernel");

	smapbase = (struct bios_smap *)preload_search_info(kmdp,
		MODINFO_METADATA | MODINFOMD_SMAP);
	if (smapbase == NULL)
		panic("No BIOS smap info from loader!");

	smapsize = *((uint32_t *)smapbase - 1);
	smapend = (struct bios_smap *)((uintptr_t)smapbase + smapsize);

	vmm_mem_total_bytes = 0;
	nsegs = 0;
	for (smap = smapbase; smap < smapend; smap++) {
		/*
		 * XXX
		 * Assuming non-overlapping, monotonically increasing
		 * memory segments.
		 */
		if (smap->type != SMAP_TYPE_MEMORY)
			continue;
		if (smap->length == 0)
			break;

		base = roundup(smap->base, NBPDR);
		length = rounddown(smap->length, NBPDR);

		/* Skip this segment if FreeBSD is using all of it. */
		if (base + length <= ptoa(Maxmem))
			continue;

		/*
		 * If FreeBSD is using part of this segment then adjust
		 * 'base' and 'length' accordingly.
		 */
		if (base < ptoa(Maxmem)) {
			uint64_t used;
			used = roundup(ptoa(Maxmem), NBPDR) - base;
			base += used;
			length -= used;
		}

		if (length == 0)
			continue;

		vmm_mem_avail[nsegs].base = base;
		vmm_mem_avail[nsegs].length = length;
		vmm_mem_total_bytes += length;

		if (base + length > maxaddr)
			maxaddr = base + length;

		if (0 && bootverbose) {
			printf("vmm_mem_populate: index %d, base 0x%0lx, "
			       "length %ld\n",
			       nsegs, vmm_mem_avail[nsegs].base,
			       vmm_mem_avail[nsegs].length);
		}

		nsegs++;
		if (nsegs >= VMM_MEM_MAXSEGS) {
			printf("vmm_mem_populate: maximum number of vmm memory "
			       "segments reached!\n");
			return (ENOSPC);
		}
	}

	vmm_mem_nsegs = nsegs;

	return (0);
}

static void
vmm_mem_direct_map(vm_paddr_t start, vm_paddr_t end)
{
	vm_paddr_t addr, remaining;
	int pdpi, pdi, superpage_size;
	pml4_entry_t *pml4p;
	pdp_entry_t *pdp;
	pd_entry_t *pd;
	uint64_t page_attr_bits;

	if (end >= NBPML4)
		panic("Cannot map memory beyond %ldGB", NBPML4 / GB);

	if (vmm_supports_1G_pages())
		superpage_size = NBPDP;
	else
		superpage_size = NBPDR;

	/*
	 * Get the page directory pointer page that contains the direct
	 * map address mappings.
	 */
	pml4p = kernel_pmap->pm_pml4;
	pdp = (pdp_entry_t *)PHYS_TO_DMAP(pml4p[DMPML4I] & ~PAGE_MASK);

	page_attr_bits = PG_RW | PG_V | PG_PS | PG_G;
	addr = start;
	while (addr < end) {
		remaining = end - addr;
		pdpi = addr / NBPDP;
		if (superpage_size == NBPDP &&
		    remaining >= NBPDP &&
		    addr % NBPDP == 0) {
			/*
			 * If there isn't a mapping for this address then
			 * create one but if there is one already make sure
			 * it matches what we expect it to be.
			 */
			if (pdp[pdpi] == 0) {
				pdp[pdpi] = addr | page_attr_bits;
				if (0 && bootverbose) {
					printf("vmm_mem_populate: mapping "
					       "0x%lx with 1GB page at "
					       "pdpi %d\n", addr, pdpi);
				}
			} else {
				pdp_entry_t pdpe = pdp[pdpi];
				if ((pdpe & ~PAGE_MASK) != addr ||
				    (pdpe & page_attr_bits) != page_attr_bits) {
					panic("An invalid mapping 0x%016lx "
					      "already exists for 0x%016lx\n",
					      pdpe, addr);
				}
			}
			addr += NBPDP;
		} else {
			if (remaining < NBPDR) {
				panic("vmm_mem_populate: remaining (%ld) must "
				      "be greater than NBPDR (%d)\n",
				      remaining, NBPDR);
			}
			if (pdp[pdpi] == 0) {
				/*
				 * XXX we lose this memory forever because
				 * we do not keep track of the virtual address
				 * that would be required to free this page.
				 */
				pd = malloc(PAGE_SIZE, M_VMM_MEM,
					    M_WAITOK | M_ZERO);
				if ((uintptr_t)pd & PAGE_MASK) {
					panic("vmm_mem_populate: page directory"
					      "page not aligned on %d "
					      "boundary\n", PAGE_SIZE);
				}
				pdp[pdpi] = vtophys(pd);
				pdp[pdpi] |= PG_RW | PG_V | PG_U;
				if (0 && bootverbose) {
					printf("Creating page directory "
					       "at pdp index %d for 0x%016lx\n",
					       pdpi, addr);
				}
			}
			pdi = (addr % NBPDP) / NBPDR;
			pd = (pd_entry_t *)PHYS_TO_DMAP(pdp[pdpi] & ~PAGE_MASK);

			/*
			 * Create a new mapping if one doesn't already exist
			 * or validate it if it does.
			 */
			if (pd[pdi] == 0) {
				pd[pdi] = addr | page_attr_bits;
				if (0 && bootverbose) {
					printf("vmm_mem_populate: mapping "
					       "0x%lx with 2MB page at "
					       "pdpi %d, pdi %d\n",
					       addr, pdpi, pdi);
				}
			} else {
				pd_entry_t pde = pd[pdi];
				if ((pde & ~PAGE_MASK) != addr ||
				    (pde & page_attr_bits) != page_attr_bits) {
					panic("An invalid mapping 0x%016lx "
					      "already exists for 0x%016lx\n",
					      pde, addr);
				}
			}
			addr += NBPDR;
		}
	}
}

static int
vmm_mem_populate(void)
{
	int seg, error;
	vm_paddr_t start, end;

	/* populate the vmm_mem_avail[] array */
	error = vmm_mem_steal_memory();
	if (error)
		return (error);
	
	/*
	 * Now map the memory that was hidden from FreeBSD in
	 * the direct map VA space.
	 */
	for (seg = 0; seg < vmm_mem_nsegs; seg++) {
		start = vmm_mem_avail[seg].base;
		end = start + vmm_mem_avail[seg].length;
		if ((start & PDRMASK) != 0 || (end & PDRMASK) != 0) {
			panic("start (0x%016lx) and end (0x%016lx) must be "
			      "aligned on a %dMB boundary\n",
			      start, end, NBPDR / MB);
		}
		vmm_mem_direct_map(start, end);
	}

	return (0);
}

int
vmm_mem_init(void)
{
	int error;

	mtx_init(&vmm_mem_mtx, "vmm_mem_mtx", NULL, MTX_DEF);

	error = vmm_mem_populate();
	if (error)
		return (error);

	return (0);
}

vm_paddr_t
vmm_mem_alloc(size_t size)
{
	int i;
	vm_paddr_t addr;

	if ((size & PAGE_MASK) != 0) {
		panic("vmm_mem_alloc: size 0x%0lx must be "
		      "aligned on a 0x%0x boundary\n", size, PAGE_SIZE);
	}

	addr = 0;

	mtx_lock(&vmm_mem_mtx);
	for (i = 0; i < vmm_mem_nsegs; i++) {
		if (vmm_mem_avail[i].length >= size) {
			addr = vmm_mem_avail[i].base;
			vmm_mem_avail[i].base += size;
			vmm_mem_avail[i].length -= size;
			/* remove a zero length segment */
			if (vmm_mem_avail[i].length == 0) {
				memmove(&vmm_mem_avail[i],
					&vmm_mem_avail[i + 1],
					(vmm_mem_nsegs - (i + 1)) *
					 sizeof(vmm_mem_avail[0]));
				vmm_mem_nsegs--;
			}
			break;
		}
	}
	mtx_unlock(&vmm_mem_mtx);

	return (addr);
}

size_t
vmm_mem_get_mem_total(void)
{
	return vmm_mem_total_bytes;
}

size_t
vmm_mem_get_mem_free(void)
{
	size_t length = 0;
	int i;

	mtx_lock(&vmm_mem_mtx);
	for (i = 0; i < vmm_mem_nsegs; i++) {
		length += vmm_mem_avail[i].length;
	}
	mtx_unlock(&vmm_mem_mtx);

	return(length);
}

void
vmm_mem_free(vm_paddr_t base, size_t length)
{
	int i;

	if ((base & PAGE_MASK) != 0 || (length & PAGE_MASK) != 0) {
		panic("vmm_mem_free: base 0x%0lx and length 0x%0lx must be "
		      "aligned on a 0x%0x boundary\n", base, length, PAGE_SIZE);
	}

	mtx_lock(&vmm_mem_mtx);

	for (i = 0; i < vmm_mem_nsegs; i++) {
		if (vmm_mem_avail[i].base > base)
			break;
	}

	if (vmm_mem_nsegs >= VMM_MEM_MAXSEGS)
		panic("vmm_mem_free: cannot free any more segments");

	/* Create a new segment at index 'i' */
	memmove(&vmm_mem_avail[i + 1], &vmm_mem_avail[i],
		(vmm_mem_nsegs - i) * sizeof(vmm_mem_avail[0]));

	vmm_mem_avail[i].base = base;
	vmm_mem_avail[i].length = length;

	vmm_mem_nsegs++;

coalesce_some_more:
	for (i = 0; i < vmm_mem_nsegs - 1; i++) {
		if (vmm_mem_avail[i].base + vmm_mem_avail[i].length ==
		    vmm_mem_avail[i + 1].base) {
			vmm_mem_avail[i].length += vmm_mem_avail[i + 1].length;
			memmove(&vmm_mem_avail[i + 1], &vmm_mem_avail[i + 2],
			  (vmm_mem_nsegs - (i + 2)) * sizeof(vmm_mem_avail[0]));
			vmm_mem_nsegs--;
			goto coalesce_some_more;
		}
	}

	mtx_unlock(&vmm_mem_mtx);
}

vm_paddr_t
vmm_mem_maxaddr(void)
{

	return (maxaddr);
}

void
vmm_mem_dump(void)
{
	int i;
	vm_paddr_t base;
	vm_size_t length;

	mtx_lock(&vmm_mem_mtx);
	for (i = 0; i < vmm_mem_nsegs; i++) {
		base = vmm_mem_avail[i].base;
		length = vmm_mem_avail[i].length;
		printf("%-4d0x%016lx    0x%016lx\n", i, base, base + length);
	}
	mtx_unlock(&vmm_mem_mtx);
}
