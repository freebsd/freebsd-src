/*-
 * Copyright (c) 2012 Marcel Moolenaar
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
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/md_var.h>
#include <machine/vmparam.h>

static u_int phys_avail_segs;

vm_paddr_t phys_avail[2 * VM_PHYSSEG_MAX + 2];

vm_paddr_t paddr_max;
long Maxmem;
long realmem;

static u_int
ia64_physmem_find(vm_paddr_t base, vm_paddr_t lim)
{
	u_int idx;

	for (idx = 0; phys_avail[idx + 1] != 0; idx += 2) {
		if (phys_avail[idx] >= lim ||
		    phys_avail[idx + 1] > base)
			break;
	}
	return (idx);
}

static int
ia64_physmem_insert(u_int idx, vm_paddr_t base, vm_paddr_t lim)
{
	u_int ridx;

	if (phys_avail_segs == VM_PHYSSEG_MAX)
		return (ENOMEM);

	ridx = phys_avail_segs * 2;
	while (idx < ridx) {
		phys_avail[ridx + 1] = phys_avail[ridx - 1];
		phys_avail[ridx] = phys_avail[ridx - 2];
		ridx -= 2;
	}
	phys_avail[idx] = base;
	phys_avail[idx + 1] = lim;
	phys_avail_segs++;
	return (0);
}

static int
ia64_physmem_remove(u_int idx)
{

	if (phys_avail_segs == 0)
		return (ENOENT);
	do {
		phys_avail[idx] = phys_avail[idx + 2];
		phys_avail[idx + 1] = phys_avail[idx + 3];
		idx += 2;
	} while (phys_avail[idx + 1] != 0);
	phys_avail_segs--;
	return (0);
}

int
ia64_physmem_add(vm_paddr_t base, vm_size_t len)
{
	vm_paddr_t lim;
	u_int idx;

	realmem += len;

	lim = base + len;
	idx = ia64_physmem_find(base, lim);
	if (phys_avail[idx] == lim) {
		phys_avail[idx] = base;
		return (0);
	}
	if (idx > 0 && phys_avail[idx - 1] == base) {
		phys_avail[idx - 1] = lim;
		return (0);
	}
	return (ia64_physmem_insert(idx, base, lim));
}

int
ia64_physmem_delete(vm_paddr_t base, vm_size_t len)
{
	vm_paddr_t lim;
	u_int idx;

	lim = base + len;
	idx = ia64_physmem_find(base, lim);
	if (phys_avail[idx] >= lim || phys_avail[idx + 1] == 0)
		return (ENOENT);
	if (phys_avail[idx] < base && phys_avail[idx + 1] > lim) {
		len = phys_avail[idx + 1] - lim;
		phys_avail[idx + 1] = base;
		base = lim;
		lim = base + len;
		return (ia64_physmem_insert(idx + 2, base, lim));
	} else {
		if (phys_avail[idx] == base)
			phys_avail[idx] = lim;
		if (phys_avail[idx + 1] == lim)
			phys_avail[idx + 1] = base;
		if (phys_avail[idx] >= phys_avail[idx + 1])
			return (ia64_physmem_remove(idx));
	}
	return (0);
}

int
ia64_physmem_fini(void)
{
	vm_paddr_t base, lim, size;
	u_int idx;

	idx = 0;
	while (phys_avail[idx + 1] != 0) {
		base = round_page(phys_avail[idx]);
		lim = trunc_page(phys_avail[idx + 1]);
		if (base < lim) {
			phys_avail[idx] = base;
			phys_avail[idx + 1] = lim;
			size = lim - base;
			physmem += atop(size);
			paddr_max = lim;
			idx += 2;
		} else
			ia64_physmem_remove(idx);
	}

	/*
	 * Round realmem to a multple of 128MB. Hopefully that compensates
	 * for any loss of DRAM that isn't accounted for in the memory map.
	 * I'm thinking legacy BIOS or VGA here. In any case, it's ok if
	 * we got it wrong, because we don't actually use realmem. It's
	 * just for show...
	 */
	size = 1U << 27;
	realmem = (realmem + size - 1) & ~(size - 1);
	realmem = atop(realmem);

	/*
	 * Maxmem isn't the "maximum memory", it's one larger than the
	 * highest page of the physical address space.
	 */
	Maxmem = atop(paddr_max);
	return (0);
}

int
ia64_physmem_init(void)
{

	/* Nothing to do just yet. */
	return (0);
}

int
ia64_physmem_track(vm_paddr_t base, vm_size_t len)
{

	realmem += len;
	return (0);
}

void *
ia64_physmem_alloc(vm_size_t len, vm_size_t align)
{
	vm_paddr_t base, lim, pa;
	void *ptr;
	u_int idx;

	if (phys_avail_segs == 0)
		return (NULL);

	len = round_page(len);

	/*
	 * Try and allocate with least effort.
	 */
	idx = phys_avail_segs * 2;
	while (idx > 0) {
		idx -= 2;
		base = phys_avail[idx];
		lim = phys_avail[idx + 1];

		if (lim - base < len)
			continue;

		/* First try from the end. */
		pa = lim - len;
		if ((pa & (align - 1)) == 0) {
			if (pa == base)
				ia64_physmem_remove(idx);
			else
				phys_avail[idx + 1] = pa;
			goto gotit;
		}

		/* Try from the start next. */
		pa = base;
		if ((pa & (align - 1)) == 0) {
			if (pa + len == lim)
				ia64_physmem_remove(idx);
			else
				phys_avail[idx] += len;
			goto gotit;
		}
	}

	/*
	 * Find a good segment and split it up.
	 */
	idx = phys_avail_segs * 2;
	while (idx > 0) {
		idx -= 2;
		base = phys_avail[idx];
		lim = phys_avail[idx + 1];

		pa = (base + align - 1) & ~(align - 1);
		if (pa + len <= lim) {
			ia64_physmem_delete(pa, len);
			goto gotit;
		}
	}

	/* Out of luck. */
	return (NULL);

 gotit:
	ptr = (void *)IA64_PHYS_TO_RR7(pa);
	bzero(ptr, len);
	return (ptr);
}
