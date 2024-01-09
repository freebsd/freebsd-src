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
#include <vm/vm_page.h>
#include <vm/vm_param.h>
#include <machine/bus.h>

/*
 * memdesc_copyback copies data from a source buffer into a buffer
 * described by a memory descriptor.
 */
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

/*
 * memdesc_copydata copies data from a buffer described by a memory
 * descriptor into a destination buffer.
 */
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

/*
 * memdesc_alloc_ext_mbufs allocates a chain of external mbufs backed
 * by the storage of a memory descriptor's data buffer.
 */
static struct mbuf *
vaddr_ext_mbuf(memdesc_alloc_ext_mbuf_t *ext_alloc, void *cb_arg, int how,
    void *buf, size_t len, size_t *actual_len)
{
	*actual_len = len;
	return (ext_alloc(cb_arg, how, buf, len));
}

static bool
can_append_paddr(struct mbuf *m, vm_paddr_t pa)
{
	u_int last_len;

	/* Can always append to an empty mbuf. */
	if (m->m_epg_npgs == 0)
		return (true);

	/* Can't append to a full mbuf. */
	if (m->m_epg_npgs == MBUF_PEXT_MAX_PGS)
		return (false);

	/* Can't append a non-page-aligned address to a non-empty mbuf. */
	if ((pa & PAGE_MASK) != 0)
		return (false);

	/* Can't append if the last page is not a full page. */
	last_len = m->m_epg_last_len;
	if (m->m_epg_npgs == 1)
		last_len += m->m_epg_1st_off;
	return (last_len == PAGE_SIZE);
}

/*
 * Returns amount of data added to an M_EXTPG mbuf.
 */
static size_t
append_paddr_range(struct mbuf *m, vm_paddr_t pa, size_t len)
{
	size_t appended;

	appended = 0;

	/* Append the first page. */
	if (m->m_epg_npgs == 0) {
		m->m_epg_pa[0] = trunc_page(pa);
		m->m_epg_npgs = 1;
		m->m_epg_1st_off = pa & PAGE_MASK;
		m->m_epg_last_len = PAGE_SIZE - m->m_epg_1st_off;
		if (m->m_epg_last_len > len)
			m->m_epg_last_len = len;
		m->m_len = m->m_epg_last_len;
		len -= m->m_epg_last_len;
		pa += m->m_epg_last_len;
		appended += m->m_epg_last_len;
	}
	KASSERT(len == 0 || (pa & PAGE_MASK) == 0,
	    ("PA not aligned before full pages"));

	/* Full pages. */
	while (len >= PAGE_SIZE && m->m_epg_npgs < MBUF_PEXT_MAX_PGS) {
		m->m_epg_pa[m->m_epg_npgs] = pa;
		m->m_epg_npgs++;
		m->m_epg_last_len = PAGE_SIZE;
		m->m_len += PAGE_SIZE;
		pa += PAGE_SIZE;
		len -= PAGE_SIZE;
		appended += PAGE_SIZE;
	}

	/* Final partial page. */
	if (len > 0 && m->m_epg_npgs < MBUF_PEXT_MAX_PGS) {
		KASSERT(len < PAGE_SIZE, ("final page is full page"));
		m->m_epg_pa[m->m_epg_npgs] = pa;
		m->m_epg_npgs++;
		m->m_epg_last_len = len;
		m->m_len += len;
		appended += len;
	}

	return (appended);
}

static struct mbuf *
paddr_ext_mbuf(memdesc_alloc_extpg_mbuf_t *extpg_alloc, void *cb_arg, int how,
    vm_paddr_t pa, size_t len, size_t *actual_len, bool can_truncate)
{
	struct mbuf *m, *tail;
	size_t appended;

	if (can_truncate) {
		vm_paddr_t end;

		/*
		 * Trim any partial page at the end, but not if it's
		 * the only page.
		 */
		end = trunc_page(pa + len);
		if (end > pa)
			len = end - pa;
	}
	*actual_len = len;

	m = tail = extpg_alloc(cb_arg, how);
	if (m == NULL)
		return (NULL);
	while (len > 0) {
		if (!can_append_paddr(tail, pa)) {
			MBUF_EXT_PGS_ASSERT_SANITY(tail);
			tail->m_next = extpg_alloc(cb_arg, how);
			if (tail->m_next == NULL)
				goto error;
			tail = tail->m_next;
		}

		appended = append_paddr_range(tail, pa, len);
		KASSERT(appended > 0, ("did not append anything"));
		KASSERT(appended <= len, ("appended too much"));

		pa += appended;
		len -= appended;
	}

	MBUF_EXT_PGS_ASSERT_SANITY(tail);
	return (m);
error:
	m_freem(m);
	return (NULL);
}

static struct mbuf *
vlist_ext_mbuf(memdesc_alloc_ext_mbuf_t *ext_alloc, void *cb_arg, int how,
    struct bus_dma_segment *vlist, u_int sglist_cnt, size_t offset,
    size_t len, size_t *actual_len)
{
	struct mbuf *m, *n, *tail;
	size_t todo;

	*actual_len = len;

	while (vlist->ds_len <= offset) {
		KASSERT(sglist_cnt > 1, ("out of sglist entries"));

		offset -= vlist->ds_len;
		vlist++;
		sglist_cnt--;
	}

	m = tail = NULL;
	while (len > 0) {
		KASSERT(sglist_cnt >= 1, ("out of sglist entries"));

		todo = len;
		if (todo > vlist->ds_len - offset)
			todo = vlist->ds_len - offset;

		n = ext_alloc(cb_arg, how, (char *)(uintptr_t)vlist->ds_addr +
		    offset, todo);
		if (n == NULL)
			goto error;

		if (m == NULL) {
			m = n;
			tail = m;
		} else {
			tail->m_next = n;
			tail = n;
		}

		offset = 0;
		vlist++;
		sglist_cnt--;
		len -= todo;
	}

	return (m);
error:
	m_freem(m);
	return (NULL);
}

static struct mbuf *
plist_ext_mbuf(memdesc_alloc_extpg_mbuf_t *extpg_alloc, void *cb_arg, int how,
    struct bus_dma_segment *plist, u_int sglist_cnt, size_t offset, size_t len,
    size_t *actual_len, bool can_truncate)
{
	vm_paddr_t pa;
	struct mbuf *m, *tail;
	size_t appended, totlen, todo;

	while (plist->ds_len <= offset) {
		KASSERT(sglist_cnt > 1, ("out of sglist entries"));

		offset -= plist->ds_len;
		plist++;
		sglist_cnt--;
	}

	totlen = 0;
	m = tail = extpg_alloc(cb_arg, how);
	if (m == NULL)
		return (NULL);
	while (len > 0) {
		KASSERT(sglist_cnt >= 1, ("out of sglist entries"));

		pa = plist->ds_addr + offset;
		todo = len;
		if (todo > plist->ds_len - offset)
			todo = plist->ds_len - offset;

		/*
		 * If truncation is enabled, avoid sending a final
		 * partial page, but only if there is more data
		 * available in the current segment.  Also, at least
		 * some data must be sent, so only drop the final page
		 * for this segment if the segment spans multiple
		 * pages or some other data is already queued.
		 */
		else if (can_truncate) {
			vm_paddr_t end;

			end = trunc_page(pa + len);
			if (end <= pa && totlen != 0) {
				/*
				 * This last segment is only a partial
				 * page.
				 */
				len = 0;
				break;
			}
			todo = end - pa;
		}

		offset = 0;
		len -= todo;
		totlen += todo;

		while (todo > 0) {
			if (!can_append_paddr(tail, pa)) {
				MBUF_EXT_PGS_ASSERT_SANITY(tail);
				tail->m_next = extpg_alloc(cb_arg, how);
				if (tail->m_next == NULL)
					goto error;
				tail = tail->m_next;
			}

			appended = append_paddr_range(tail, pa, todo);
			KASSERT(appended > 0, ("did not append anything"));

			pa += appended;
			todo -= appended;
		}
	}

	MBUF_EXT_PGS_ASSERT_SANITY(tail);
	*actual_len = totlen;
	return (m);
error:
	m_freem(m);
	return (NULL);
}

static struct mbuf *
vmpages_ext_mbuf(memdesc_alloc_extpg_mbuf_t *extpg_alloc, void *cb_arg, int how,
    vm_page_t *ma, size_t offset, size_t len, size_t *actual_len,
    bool can_truncate)
{
	struct mbuf *m, *tail;

	while (offset >= PAGE_SIZE) {
		ma++;
		offset -= PAGE_SIZE;
	}

	if (can_truncate) {
		size_t end;

		/*
		 * Trim any partial page at the end, but not if it's
		 * the only page.
		 */
		end = trunc_page(offset + len);
		if (end > offset)
			len = end - offset;
	}
	*actual_len = len;

	m = tail = extpg_alloc(cb_arg, how);
	if (m == NULL)
		return (NULL);

	/* First page. */
	m->m_epg_pa[0] = VM_PAGE_TO_PHYS(*ma);
	ma++;
	m->m_epg_npgs = 1;
	m->m_epg_1st_off = offset;
	m->m_epg_last_len = PAGE_SIZE - offset;
	if (m->m_epg_last_len > len)
		m->m_epg_last_len = len;
	m->m_len = m->m_epg_last_len;
	len -= m->m_epg_last_len;

	/* Full pages. */
	while (len >= PAGE_SIZE) {
		if (tail->m_epg_npgs == MBUF_PEXT_MAX_PGS) {
			MBUF_EXT_PGS_ASSERT_SANITY(tail);
			tail->m_next = extpg_alloc(cb_arg, how);
			if (tail->m_next == NULL)
				goto error;
			tail = tail->m_next;
		}

		tail->m_epg_pa[tail->m_epg_npgs] = VM_PAGE_TO_PHYS(*ma);
		ma++;
		tail->m_epg_npgs++;
		tail->m_epg_last_len = PAGE_SIZE;
		tail->m_len += PAGE_SIZE;
		len -= PAGE_SIZE;
	}

	/* Last partial page. */
	if (len > 0) {
		if (tail->m_epg_npgs == MBUF_PEXT_MAX_PGS) {
			MBUF_EXT_PGS_ASSERT_SANITY(tail);
			tail->m_next = extpg_alloc(cb_arg, how);
			if (tail->m_next == NULL)
				goto error;
			tail = tail->m_next;
		}

		tail->m_epg_pa[tail->m_epg_npgs] = VM_PAGE_TO_PHYS(*ma);
		ma++;
		tail->m_epg_npgs++;
		tail->m_epg_last_len = len;
		tail->m_len += len;
	}

	MBUF_EXT_PGS_ASSERT_SANITY(tail);
	return (m);
error:
	m_freem(m);
	return (NULL);
}

/*
 * Somewhat similar to m_copym but optionally avoids a partial mbuf at
 * the end.
 */
static struct mbuf *
mbuf_subchain(struct mbuf *m0, size_t offset, size_t len,
    size_t *actual_len, bool can_truncate, int how)
{
	struct mbuf *m, *tail;
	size_t totlen;

	while (offset >= m0->m_len) {
		offset -= m0->m_len;
		m0 = m0->m_next;
	}

	/* Always return at least one mbuf. */
	totlen = m0->m_len - offset;
	if (totlen > len)
		totlen = len;

	m = m_get(how, MT_DATA);
	if (m == NULL)
		return (NULL);
	m->m_len = totlen;
	if (m0->m_flags & (M_EXT | M_EXTPG)) {
		m->m_data = m0->m_data + offset;
		mb_dupcl(m, m0);
	} else
		memcpy(mtod(m, void *), mtodo(m0, offset), m->m_len);

	tail = m;
	m0 = m0->m_next;
	len -= totlen;
	while (len > 0) {
		/*
		 * If truncation is enabled, don't send any partial
		 * mbufs besides the first one.
		 */
		if (can_truncate && m0->m_len > len)
			break;

		tail->m_next = m_get(how, MT_DATA);
		if (tail->m_next == NULL)
			goto error;
		tail = tail->m_next;
		tail->m_len = m0->m_len;
		if (m0->m_flags & (M_EXT | M_EXTPG)) {
			tail->m_data = m0->m_data;
			mb_dupcl(tail, m0);
		} else
			memcpy(mtod(tail, void *), mtod(m0, void *),
			    tail->m_len);

		totlen += tail->m_len;
		m0 = m0->m_next;
		len -= tail->m_len;
	}
	*actual_len = totlen;
	return (m);
error:
	m_freem(m);
	return (NULL);
}

struct mbuf *
memdesc_alloc_ext_mbufs(struct memdesc *mem,
    memdesc_alloc_ext_mbuf_t *ext_alloc,
    memdesc_alloc_extpg_mbuf_t *extpg_alloc, void *cb_arg, int how,
    size_t offset, size_t len, size_t *actual_len, bool can_truncate)
{
	struct mbuf *m;
	size_t done;

	switch (mem->md_type) {
	case MEMDESC_VADDR:
		m = vaddr_ext_mbuf(ext_alloc, cb_arg, how,
		    (char *)mem->u.md_vaddr + offset, len, &done);
		break;
	case MEMDESC_PADDR:
		m = paddr_ext_mbuf(extpg_alloc, cb_arg, how, mem->u.md_paddr +
		    offset, len, &done, can_truncate);
		break;
	case MEMDESC_VLIST:
		m = vlist_ext_mbuf(ext_alloc, cb_arg, how, mem->u.md_list,
		    mem->md_nseg, offset, len, &done);
		break;
	case MEMDESC_PLIST:
		m = plist_ext_mbuf(extpg_alloc, cb_arg, how, mem->u.md_list,
		    mem->md_nseg, offset, len, &done, can_truncate);
		break;
	case MEMDESC_UIO:
		panic("uio not supported");
	case MEMDESC_MBUF:
		m = mbuf_subchain(mem->u.md_mbuf, offset, len, &done,
		    can_truncate, how);
		break;
	case MEMDESC_VMPAGES:
		m = vmpages_ext_mbuf(extpg_alloc, cb_arg, how, mem->u.md_ma,
		    mem->md_offset + offset, len, &done, can_truncate);
		break;
	default:
		__assert_unreachable();
	}
	if (m == NULL)
		return (NULL);

	if (can_truncate) {
		KASSERT(done <= len, ("chain too long"));
	} else {
		KASSERT(done == len, ("short chain with no limit"));
	}
	KASSERT(m_length(m, NULL) == done, ("length mismatch"));
	if (actual_len != NULL)
		*actual_len = done;
	return (m);
}
