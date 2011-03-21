/*-
 * Copyright (c) 2006 Marcel Moolenaar
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

#include <stand.h>
#include <ia64/include/vmparam.h>

#include "libia64.h"

uint64_t *ia64_pgtbl;
uint32_t ia64_pgtblsz;

static int
pgtbl_extend(u_int idx)
{
	uint64_t *pgtbl;
	uint32_t pgtblsz;
	u_int pot;

	pgtblsz = (idx + 1) << 3;

	/* The minimum size is 4KB. */
	if (pgtblsz < 4096)
		pgtblsz = 4096;

	/* Find the next higher power of 2. */
	pgtblsz--;
	for (pot = 1; pot < 32; pot <<= 1)
		pgtblsz = pgtblsz | (pgtblsz >> pot);
	pgtblsz++;

	/* The maximum size is 1MB. */
	if (pgtblsz > 1048576)
		return (ENOMEM);

	/* Make sure the size is a valid (mappable) page size. */
	if (pgtblsz == 32*1024 || pgtblsz == 128*1024 || pgtblsz == 512*1024)
		pgtblsz <<= 1;

	/* Allocate naturally aligned memory. */
	pgtbl = (void *)ia64_platform_alloc(0, pgtblsz);
	if (pgtbl == NULL)
		return (ENOMEM);

	/* Initialize new page table. */
	if (ia64_pgtbl != NULL && ia64_pgtbl != pgtbl)
		bcopy(ia64_pgtbl, pgtbl, ia64_pgtblsz);
	bzero(pgtbl + (ia64_pgtblsz >> 3), pgtblsz - ia64_pgtblsz);

	if (ia64_pgtbl != NULL && ia64_pgtbl != pgtbl)
		ia64_platform_free(0, (uintptr_t)ia64_pgtbl, ia64_pgtblsz);

	ia64_pgtbl = pgtbl;
	ia64_pgtblsz = pgtblsz;
	return (0);
}

static void *
va2pa(vm_offset_t va, size_t *len)
{
	uint64_t pa;
	u_int idx, ofs;
	int error;

	/* Backward compatibility. */
	if (va >= IA64_RR_BASE(7)) {
		pa = IA64_RR_MASK(va);
		return ((void *)pa);
	}

	if (va < IA64_PBVM_BASE) {
		error = EINVAL;
		goto fail;
	}

	idx = (va - IA64_PBVM_BASE) >> IA64_PBVM_PAGE_SHIFT;
	if (idx >= (ia64_pgtblsz >> 3)) {
		error = pgtbl_extend(idx);
		if (error)
			goto fail;
	}

	ofs = va & IA64_PBVM_PAGE_MASK;
	pa = ia64_pgtbl[idx];
	if (pa == 0) {
		pa = ia64_platform_alloc(va - ofs, IA64_PBVM_PAGE_SIZE);
		if (pa == 0) {
			error = ENOMEM;
			goto fail;
		}
		ia64_pgtbl[idx] = pa;
	}
	pa += ofs;

	/* We can not cross page boundaries (in general). */
	if (*len + ofs > IA64_PBVM_PAGE_SIZE)
		*len = IA64_PBVM_PAGE_SIZE - ofs;

	return ((void *)pa);

 fail:
	*len = 0;
	return (NULL);
}

ssize_t
ia64_copyin(const void *src, vm_offset_t va, size_t len)
{
	void *pa;
	ssize_t res;
	size_t sz;

	res = 0;
	while (len > 0) {
		sz = len;
		pa = va2pa(va, &sz);
		if (sz == 0)
			break;
		bcopy(src, pa, sz);
		len -= sz;
		res += sz;
		va += sz;
	}
	return (res);
}

ssize_t
ia64_copyout(vm_offset_t va, void *dst, size_t len)
{
	void *pa;
	ssize_t res;
	size_t sz;

	res = 0;
	while (len > 0) {
		sz = len;
		pa = va2pa(va, &sz);
		if (sz == 0)
			break;
		bcopy(pa, dst, sz);
		len -= sz;
		res += sz;
		va += sz;
	}
	return (res);
}

ssize_t
ia64_readin(int fd, vm_offset_t va, size_t len)
{
	void *pa;
	ssize_t res, s;
	size_t sz;

	res = 0;
	while (len > 0) {
		sz = len;
		pa = va2pa(va, &sz);
		if (sz == 0)
			break;
		s = read(fd, pa, sz);
		if (s <= 0)
			break;
		len -= s;
		res += s;
		va += s;
	}
	return (res);
}
