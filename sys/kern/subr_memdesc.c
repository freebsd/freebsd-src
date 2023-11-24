/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/memdesc.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_param.h>
#include <machine/bus.h>

static void
phys_copyback(vm_paddr_t pa, int off, int size, const void *src)
{
	const char *cp;
	u_int page_off;
	int todo;
	void *p;

	KASSERT(PMAP_HAS_DMAP, ("direct-map required"));

	cp = src;
	pa += off;
	page_off = pa & PAGE_MASK;
	while (size > 0) {
		todo = min(PAGE_SIZE - page_off, size);
		p = (void *)PHYS_TO_DMAP(pa);
		memcpy(p, cp, todo);
		size -= todo;
		cp += todo;
		pa += todo;
		page_off = 0;
	}
}

static void
vlist_copyback(struct bus_dma_segment *vlist, int sglist_cnt, int off,
    int size, const void *src)
{
	const char *p;
	int todo;

	while (vlist->ds_len <= off) {
		KASSERT(sglist_cnt > 1, ("out of sglist entries"));

		off -= vlist->ds_len;
		vlist++;
		sglist_cnt--;
	}

	p = src;
	while (size > 0) {
		KASSERT(sglist_cnt >= 1, ("out of sglist entries"));

		todo = size;
		if (todo > vlist->ds_len - off)
			todo = vlist->ds_len - off;

		memcpy((char *)(uintptr_t)vlist->ds_addr + off, p, todo);
		off = 0;
		vlist++;
		sglist_cnt--;
		size -= todo;
		p += todo;
	}
}

static void
plist_copyback(struct bus_dma_segment *plist, int sglist_cnt, int off,
    int size, const void *src)
{
	const char *p;
	int todo;

	while (plist->ds_len <= off) {
		KASSERT(sglist_cnt > 1, ("out of sglist entries"));

		off -= plist->ds_len;
		plist++;
		sglist_cnt--;
	}

	p = src;
	while (size > 0) {
		KASSERT(sglist_cnt >= 1, ("out of sglist entries"));

		todo = size;
		if (todo > plist->ds_len - off)
			todo = plist->ds_len - off;

		phys_copyback(plist->ds_addr, off, todo, p);
		off = 0;
		plist++;
		sglist_cnt--;
		size -= todo;
		p += todo;
	}
}

static void
vmpages_copyback(vm_page_t *m, int off, int size, const void *src)
{
	struct iovec iov[1];
	struct uio uio;
	int error __diagused;

	iov[0].iov_base = __DECONST(void *, src);
	iov[0].iov_len = size;
	uio.uio_iov = iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = 0;
	uio.uio_resid = size;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_WRITE;
	error = uiomove_fromphys(m, off, size, &uio);
	KASSERT(error == 0 && uio.uio_resid == 0, ("copy failed"));
}

void
memdesc_copyback(struct memdesc *mem, int off, int size, const void *src)
{
	KASSERT(off >= 0, ("%s: invalid offset %d", __func__, off));
	KASSERT(size >= 0, ("%s: invalid size %d", __func__, off));

	switch (mem->md_type) {
	case MEMDESC_VADDR:
		KASSERT(off + size <= mem->md_len, ("copy out of bounds"));
		memcpy((char *)mem->u.md_vaddr + off, src, size);
		break;
	case MEMDESC_PADDR:
		KASSERT(off + size <= mem->md_len, ("copy out of bounds"));
		phys_copyback(mem->u.md_paddr, off, size, src);
		break;
	case MEMDESC_VLIST:
		vlist_copyback(mem->u.md_list, mem->md_nseg, off, size, src);
		break;
	case MEMDESC_PLIST:
		plist_copyback(mem->u.md_list, mem->md_nseg, off, size, src);
		break;
	case MEMDESC_UIO:
		panic("Use uiomove instead");
		break;
	case MEMDESC_MBUF:
		m_copyback(mem->u.md_mbuf, off, size, src);
		break;
	case MEMDESC_VMPAGES:
		KASSERT(off + size <= mem->md_len, ("copy out of bounds"));
		vmpages_copyback(mem->u.md_ma, mem->md_offset + off, size,
		    src);
		break;
	default:
		__assert_unreachable();
	}
}

static void
phys_copydata(vm_paddr_t pa, int off, int size, void *dst)
{
	char *cp;
	u_int page_off;
	int todo;
	const void *p;

	KASSERT(PMAP_HAS_DMAP, ("direct-map required"));

	cp = dst;
	pa += off;
	page_off = pa & PAGE_MASK;
	while (size > 0) {
		todo = min(PAGE_SIZE - page_off, size);
		p = (const void *)PHYS_TO_DMAP(pa);
		memcpy(cp, p, todo);
		size -= todo;
		cp += todo;
		pa += todo;
		page_off = 0;
	}
}

static void
vlist_copydata(struct bus_dma_segment *vlist, int sglist_cnt, int off,
    int size, void *dst)
{
	char *p;
	int todo;

	while (vlist->ds_len <= off) {
		KASSERT(sglist_cnt > 1, ("out of sglist entries"));

		off -= vlist->ds_len;
		vlist++;
		sglist_cnt--;
	}

	p = dst;
	while (size > 0) {
		KASSERT(sglist_cnt >= 1, ("out of sglist entries"));

		todo = size;
		if (todo > vlist->ds_len - off)
			todo = vlist->ds_len - off;

		memcpy(p, (char *)(uintptr_t)vlist->ds_addr + off, todo);
		off = 0;
		vlist++;
		sglist_cnt--;
		size -= todo;
		p += todo;
	}
}

static void
plist_copydata(struct bus_dma_segment *plist, int sglist_cnt, int off,
    int size, void *dst)
{
	char *p;
	int todo;

	while (plist->ds_len <= off) {
		KASSERT(sglist_cnt > 1, ("out of sglist entries"));

		off -= plist->ds_len;
		plist++;
		sglist_cnt--;
	}

	p = dst;
	while (size > 0) {
		KASSERT(sglist_cnt >= 1, ("out of sglist entries"));

		todo = size;
		if (todo > plist->ds_len - off)
			todo = plist->ds_len - off;

		phys_copydata(plist->ds_addr, off, todo, p);
		off = 0;
		plist++;
		sglist_cnt--;
		size -= todo;
		p += todo;
	}
}

static void
vmpages_copydata(vm_page_t *m, int off, int size, void *dst)
{
	struct iovec iov[1];
	struct uio uio;
	int error __diagused;

	iov[0].iov_base = dst;
	iov[0].iov_len = size;
	uio.uio_iov = iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = 0;
	uio.uio_resid = size;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_READ;
	error = uiomove_fromphys(m, off, size, &uio);
	KASSERT(error == 0 && uio.uio_resid == 0, ("copy failed"));
}

void
memdesc_copydata(struct memdesc *mem, int off, int size, void *dst)
{
	KASSERT(off >= 0, ("%s: invalid offset %d", __func__, off));
	KASSERT(size >= 0, ("%s: invalid size %d", __func__, off));

	switch (mem->md_type) {
	case MEMDESC_VADDR:
		KASSERT(off + size <= mem->md_len, ("copy out of bounds"));
		memcpy(dst, (const char *)mem->u.md_vaddr + off, size);
		break;
	case MEMDESC_PADDR:
		KASSERT(off + size <= mem->md_len, ("copy out of bounds"));
		phys_copydata(mem->u.md_paddr, off, size, dst);
		break;
	case MEMDESC_VLIST:
		vlist_copydata(mem->u.md_list, mem->md_nseg, off, size, dst);
		break;
	case MEMDESC_PLIST:
		plist_copydata(mem->u.md_list, mem->md_nseg, off, size, dst);
		break;
	case MEMDESC_UIO:
		panic("Use uiomove instead");
		break;
	case MEMDESC_MBUF:
		m_copydata(mem->u.md_mbuf, off, size, dst);
		break;
	case MEMDESC_VMPAGES:
		KASSERT(off + size <= mem->md_len, ("copy out of bounds"));
		vmpages_copydata(mem->u.md_ma, mem->md_offset + off, size,
		    dst);
		break;
	default:
		__assert_unreachable();
	}
}
