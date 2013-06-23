/*-
 * Copyright (c) 2012-2013 Marcel Moolenaar
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
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/busdma.h>
#include <sys/ktr.h>
#include <sys/queue.h>
#include <machine/stdarg.h>
#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <vm/uma.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>

#include "busdma_if.h"

/*
 * Section 1: Private types.
 */

struct busdma_tag {
	struct busdma_tag *dt_chain;
	struct busdma_tag *dt_child;
	struct busdma_tag *dt_parent;
	device_t	dt_device;
	bus_addr_t	dt_minaddr;
	bus_addr_t	dt_maxaddr;
	bus_addr_t	dt_align;
	bus_addr_t	dt_bndry;
	bus_size_t	dt_maxsz;
	bus_size_t	dt_maxsegsz;
	u_int		dt_nsegs;
	u_int		dt_datarate;
};

struct busdma_md_seg {
	TAILQ_ENTRY(busdma_md_seg) mds_chain;
	u_int		mds_idx;
	bus_addr_t	mds_busaddr;
	vm_paddr_t	mds_paddr;
	vm_offset_t	mds_vaddr;
	vm_size_t	mds_size;
};

struct busdma_md {
	struct busdma_tag *md_tag;
	u_int		md_flags;
	u_int		md_nsegs;
	TAILQ_HEAD(busdma_md_head, busdma_md_seg) md_seg;
};

#define	BUSDMA_MD_FLAG_ALLOCATED	0x1	/* busdma_mem_alloc() created
						   this MD. */
#define	BUSDMA_MD_FLAG_LOADED		0x2	/* The MD is loaded. */
#define	BUSDMA_MD_FLAG_MAPPED		0x4	/* KVA is valid. */
#define	BUSDMA_MD_FLAG_USED		\
		(BUSDMA_MD_FLAG_ALLOCATED | BUSDMA_MD_FLAG_LOADED)

/*
 * Section 2: Private data.
 */

static uma_zone_t busdma_tag_zone;
static uma_zone_t busdma_md_zone;
static uma_zone_t busdma_md_seg_zone;

static struct busdma_tag *busdma_root_tag;

/*
 * Section 3: Private functions.
 */

/* Section 3.1: Initialization. */

static void
busdma_init(void *arg)
{

	/*
	 * Create our zones.  Note that the align argument is a bitmask that
	 * relays which bits of the address must be 0.  Hence the decrement.
	 */
	busdma_tag_zone = uma_zcreate("busdma_tags",
	    sizeof(struct busdma_tag),
	    NULL /*ctor*/, NULL /*dtor*/,
	    NULL /*init*/, NULL /*fini*/,
	    __alignof(struct busdma_tag) - 1 /*align*/,
	    0);

	busdma_md_zone = uma_zcreate("busdma_mds",
	    sizeof(struct busdma_md),
	    NULL /*ctor*/, NULL /*dtor*/,
	    NULL /*init*/, NULL /*fini*/,
	    __alignof(struct busdma_md) - 1 /*align*/,
	    0);

	busdma_md_seg_zone = uma_zcreate("busdma_md_segs",
	    sizeof(struct busdma_md_seg),
	    NULL /*ctor*/, NULL /*dtor*/,
	    NULL /*init*/, NULL /*fini*/,
	    __alignof(struct busdma_tag) - 1 /*align*/,
	    0);

	/*
	 * Allocate and initialize our root tag.
	 */
	busdma_root_tag = uma_zalloc(busdma_tag_zone, M_WAITOK|M_ZERO);

	/* Make dt_maxaddr the largest possible address. */
	busdma_root_tag->dt_maxaddr = ~0UL;

	/* Make dt_align the least restrictive alignment. */
	busdma_root_tag->dt_align = 1;

	/* Make dt_maxsz the largest power of 2. */
	busdma_root_tag->dt_maxsz = (~0UL >> 1) + 1;

	/* Just like dt_maxsz, limit to the largest power of 2. */
	busdma_root_tag->dt_maxsegsz = (~0UL >> 1) + 1;

	/*
	 * Arbitrarily limit the number of scatter/gather segments to 1K
	 * so as to protect the kernel from bad drivers or bugs.  Why 1K?
	 * "It looked like a good idea at the time".
	 */
	busdma_root_tag->dt_nsegs = 1024;

	/* No limitations on the datarate. */
	busdma_root_tag->dt_datarate = ~0U;
}
SYSINIT(busdma_kmem, SI_SUB_KMEM, SI_ORDER_ANY, busdma_init, NULL);

/* Section 3.2: Debugging & tracing. */

static void
_busdma_mtag_dump(const char *func, device_t dev, struct busdma_mtag *mtag)
{

#ifdef BUSDMA_DEBUG
	printf("[%s: %s: flags=%x, min=%#jx, max=%#jx, size=%#jx, align=%#jx, "
	    "bndry=%#jx]\n", __func__,
	    (dev != NULL) ? device_get_nameunit(dev) : "*", mtag->dmt_flags,
	    (uintmax_t)mtag->dmt_minaddr, (uintmax_t)mtag->dmt_maxaddr,
	    (uintmax_t)mtag->dmt_maxsz, (uintmax_t)mtag->dmt_align,
	    (uintmax_t)mtag->dmt_bndry);
#endif
}

static void
_busdma_tag_dump(const char *func, device_t dev, struct busdma_tag *tag)
{

#ifdef BUSDMA_DEBUG
	printf("[%s: %s: tag=%p (minaddr=%#jx, maxaddr=%#jx, align=%#jx, "
	    "bndry=%#jx, maxsz=%#jx, nsegs=%u, maxsegsz=%#jx)]\n",
	    func, (dev != NULL) ? device_get_nameunit(dev) : "*", tag,
	    (uintmax_t)tag->dt_minaddr, (uintmax_t)tag->dt_maxaddr,
	    (uintmax_t)tag->dt_align, (uintmax_t)tag->dt_bndry,
	    (uintmax_t)tag->dt_maxsz,
	    tag->dt_nsegs, (uintmax_t)tag->dt_maxsegsz);
#endif
}

static void
_busdma_md_dump(const char *func, device_t dev, struct busdma_md *md) 
{
#ifdef BUSDMA_DEBUG
	struct busdma_tag *tag;
	struct busdma_md_seg *seg;

	tag = md->md_tag;
	if (dev == NULL)
		dev = tag->dt_device;
	printf("[%s: %s: md=%p (tag=%p, flags=%#x, nsegs=%u)", func,
	    device_get_nameunit(dev), md, tag, md->md_flags,
	    md->md_nsegs);
	if (md->md_nsegs == 0) {
		printf(" -- UNUSED]\n");
		return;
	}
	TAILQ_FOREACH(seg, &md->md_seg, mds_chain) {
		printf(", {idx=%u, size=%#jx, busaddr=%#jx, paddr=%#jx, "
		    "vaddr=%#jx}", seg->mds_idx, (uintmax_t)seg->mds_size,
		    (uintmax_t)seg->mds_busaddr, (uintmax_t)seg->mds_paddr,
		    (uintmax_t)seg->mds_vaddr);
	}
	printf("]\n");
#endif
}

static void
_busdma_data_dump(const char *func, struct busdma_md *md, bus_addr_t addr,
    bus_size_t len)
{
#ifdef BUSDMA_DEBUG
	struct busdma_md_seg *seg;
	void *va;
	bus_addr_t pa;
	int sz;

	printf("[%s: %s: md=%p, addr=%#jx, size=%#jx {\n", func,
	    device_get_nameunit(md->md_tag->dt_device), md,
	    (uintmax_t)addr, (uintmax_t)len);
	TAILQ_FOREACH(seg, &md->md_seg, mds_chain) {
		if (seg->mds_busaddr + seg->mds_size <= addr ||
		    addr + len <= seg->mds_busaddr)
			continue;
		pa = MAX(seg->mds_busaddr, addr);
		sz = MIN(seg->mds_busaddr + seg->mds_size, addr + len) - pa;
		sz = MIN(sz, 128);	/* XXX arbitrary limit */
		va = (void *)(seg->mds_vaddr + (pa - seg->mds_busaddr));
		printf("%#jx: %*D\n", (uintmax_t)pa, sz, va, " ");
	}
	printf("}]\n");
#endif
}

/* Section 3.3: API support functions. */

static u_int
_busdma_flags(const char *func, device_t dev, u_int flags)
{

	if (flags & BUSDMA_MD_PLATFORM_FLAGS)
		device_printf(dev, "called %s() with invalid flags %#x\n",
		    func, flags);

	return (flags & ~BUSDMA_MD_PLATFORM_FLAGS);
}

static struct busdma_md_seg *
_busdma_md_get_seg(struct busdma_md *md, u_int idx)
{
	struct busdma_md_seg *seg;

	if (md == NULL || idx >= md->md_nsegs)
		return (NULL);

	TAILQ_FOREACH(seg, &md->md_seg, mds_chain) {
		if (seg->mds_idx == idx)
			return (seg);
	}

	return (NULL);
}

static void
_busdma_md_seg_reserve(struct busdma_tag *tag)
{
}

static void
_busdma_md_seg_unreserve(struct busdma_tag *tag)
{
}

static struct busdma_tag *
_busdma_tag_get_base(device_t dev)
{
	device_t parent;
	void *base;

	base = NULL;
	parent = device_get_parent(dev);
	while (base == NULL && parent != NULL) {
		base = device_get_busdma_tag(parent);
		if (base == NULL)
			parent = device_get_parent(parent);
	}
	if (base == NULL) {
		base = busdma_root_tag;
		parent = NULL;
	}
	_busdma_tag_dump(__func__, parent, base);
	return (base);
}

static int
_busdma_tag_make(device_t dev, struct busdma_tag *base, bus_addr_t align,
    bus_addr_t bndry, bus_addr_t maxaddr, bus_size_t maxsz, u_int nsegs,
    bus_size_t maxsegsz, u_int datarate, u_int flags,
    struct busdma_tag **tag_p)
{
	struct busdma_tag *tag;

	/*
	 * If nsegs is 1, ignore maxsegsz. What this means is that if we have
	 * just 1 segment, then maxsz should be equal to maxsegsz. To keep it
	 * simple for us, limit maxsegsz to maxsz in any case.
	 */
	if (maxsegsz > maxsz || nsegs == 1)
		maxsegsz = maxsz;

	tag = uma_zalloc(busdma_tag_zone, M_NOWAIT);
	if (tag == NULL)
		return (ENOMEM);

	tag->dt_chain = NULL;
	tag->dt_child = NULL;
	tag->dt_parent = NULL;
	tag->dt_device = dev;
	tag->dt_minaddr = MAX(0, base->dt_minaddr);
	tag->dt_maxaddr = MIN(maxaddr, base->dt_maxaddr);
	tag->dt_align = MAX(align, base->dt_align);
	tag->dt_bndry = MIN(bndry, base->dt_bndry);
	tag->dt_maxsz = MIN(maxsz, base->dt_maxsz);
	tag->dt_nsegs = MIN(nsegs, base->dt_nsegs);
	tag->dt_maxsegsz = MIN(maxsegsz, base->dt_maxsegsz);
	tag->dt_datarate = MIN(datarate, base->dt_datarate);
	_busdma_tag_dump(__func__, dev, tag);
	*tag_p = tag;
	return (0);
}

static struct busdma_md *
_busdma_md_create(struct busdma_tag *tag, u_int flags)
{
	struct busdma_md *md;

	md = uma_zalloc(busdma_md_zone, M_NOWAIT);
	if (md != NULL) {
		md->md_tag = tag;
		md->md_flags = flags;
		md->md_nsegs = 0;
		TAILQ_INIT(&md->md_seg);

		/* Reserve (pre-allocate) segments */
		_busdma_md_seg_reserve(tag);
	}
	return (md);
}

static int
_busdma_iommu_xlate(device_t dev, struct busdma_mtag *mtag)
{
	device_t bus;
	int error;

	error = 0;
	bus = device_get_parent(dev);
	while (!error && bus != root_bus) {
		_busdma_mtag_dump(__func__, bus, mtag);
		error = BUSDMA_IOMMU_XLATE(bus, dev, mtag);
		if (!error)
			bus = device_get_parent(bus);
	}

	/*
	 * Always align allocated memory on cache line boundaries and round
	 * the size up to the next cache line.
	 */
	mtag->dmt_align = MAX(mtag->dmt_align, CACHE_LINE_SIZE);
	mtag->dmt_maxsz = roundup2(mtag->dmt_maxsz, CACHE_LINE_SIZE);

	_busdma_mtag_dump(__func__, bus, mtag);
	return (error);
}

static int
_busdma_iommu_map_r(device_t bus, device_t dev, struct busdma_md *md,
    struct busdma_md_seg *seg)
{
	int error;

	if (bus == root_bus) {
		/*
		 * A bus address and a physical address are one and the same
		 * at this level.
		 */
		seg->mds_busaddr = seg->mds_paddr;
		return (0);
	}

	error = _busdma_iommu_map_r(device_get_parent(bus), dev, md, seg);
	if (!error)
		error = BUSDMA_IOMMU_MAP(bus, dev, md, seg->mds_idx,
		    &seg->mds_busaddr);
	return (error);
}

static int
_busdma_iommu_map(device_t dev, struct busdma_md *md)
{
	struct busdma_md_seg *seg;
	device_t bus;
	int error;
 
	_busdma_md_dump(__func__, root_bus, md);
	bus = device_get_parent(dev);
	error = 0;
	TAILQ_FOREACH(seg, &md->md_seg, mds_chain) {
		error = _busdma_iommu_map_r(bus, dev, md, seg);
		if (error)
			break;
	}
	if (!error)
		_busdma_md_dump(__func__, dev, md);
	return (error);
}

static int
_busdma_iommu_unmap(device_t dev, struct busdma_md *md)
{
	struct busdma_md_seg *seg;
	device_t bus;
	int error;

	bus = device_get_parent(dev);
	error = 0;
	TAILQ_FOREACH(seg, &md->md_seg, mds_chain) {
		error = BUSDMA_IOMMU_UNMAP(bus, dev, md, seg->mds_idx);
		if (error)
			break;
	}
	if (error)
		printf("%s: error=%d\n", __func__, error);

	return (error);
}

static int
_busdma_iommu_sync(device_t dev, struct busdma_md *md, u_int op,
    bus_addr_t addr, bus_size_t size)
{
	device_t bus;
	int error;

	error = 0;
	bus = device_get_parent(dev);
	while (!error && bus != root_bus) {
		error = BUSDMA_IOMMU_SYNC(bus, dev, md, op, addr, size);
		if (!error)
			bus = device_get_parent(bus);
	}
	if (error)
		return (error);

	if ((op & BUSDMA_SYNC_PREWRITE) == BUSDMA_SYNC_PREWRITE ||
	    (op & BUSDMA_SYNC_POSTREAD) == BUSDMA_SYNC_POSTREAD)
		_busdma_data_dump(__func__, md, addr, size);
	return (0);
}

static int
_busdma_md_load(struct busdma_md *md, pmap_t pm, vm_offset_t va, vm_size_t len)
{
	struct busdma_md_seg *seg;
	vm_paddr_t pa;
	vm_size_t catsz, maxsegsz, pgsz, sz;
	u_int idx;

	maxsegsz = md->md_tag->dt_maxsegsz;
	seg = TAILQ_LAST(&md->md_seg, busdma_md_head);
	idx = (seg != NULL) ? seg->mds_idx + 1 : 0;
	while (len != 0) {
		pa = (pm != NULL) ? pmap_extract(pm, va) : pmap_kextract(va);
		pgsz = PAGE_SIZE - (va & PAGE_MASK);
		sz = MIN(len, maxsegsz);
		sz = MIN(pgsz, sz);
		/* Extend last segment if possible. */
		if (seg != NULL && seg->mds_size < maxsegsz &&
		    seg->mds_paddr + seg->mds_size == pa &&
		    seg->mds_vaddr + seg->mds_size == va) {
			catsz = maxsegsz - seg->mds_size;
			catsz = MIN(sz, catsz);
			seg->mds_size += catsz;
			pa += catsz;
			sz -= catsz;
			len -= catsz;
			va += catsz;
		}

		if (sz == 0)
			continue;

		/*
		 * The remaining sz bytes go in a new segment.
		 */
		seg = uma_zalloc(busdma_md_seg_zone, M_NOWAIT);
		if (seg == NULL)
			return (ENOMEM);
		seg->mds_idx = idx++;
		TAILQ_INSERT_TAIL(&md->md_seg, seg, mds_chain);
		md->md_nsegs++;
		seg->mds_busaddr = ~0UL;
		seg->mds_paddr = pa;
		seg->mds_vaddr = va;
		seg->mds_size = sz;
		len -= sz;
		va += sz;
	}

	_busdma_md_dump(__func__, NULL, md);
	return (0);
}

/*
 * Section 4: Public interface.
 */

int
busdma_tag_create(device_t dev, bus_addr_t align, bus_addr_t bndry,
    bus_addr_t maxaddr, bus_size_t maxsz, u_int nsegs, bus_size_t maxsegsz,
    u_int datarate, u_int flags, struct busdma_tag **tag_p)
{
	struct busdma_tag *base, *first, *tag;
	int error;

	CTR5(KTR_BUSDMA, "%s: dev=%s, align=%#jx, bndry=%#jx, maxaddr=%#jx",
	    __func__, device_get_nameunit(dev), (uintmax_t)align,
	    (uintmax_t)bndry, (uintmax_t)maxaddr);
	CTR5(KTR_BUSDMA, "%s: maxsz=%#jx, nsegs=%u, maxsegsz=%#jx, flags=%#x",
	    __func__, maxsz, nsegs, (uintmax_t)maxsegsz, flags);

	flags = _busdma_flags(__func__, dev, flags);

	if (dev == NULL || tag_p == NULL)
		return (EINVAL);

	base = _busdma_tag_get_base(dev);
	error = _busdma_tag_make(dev, base, align, bndry, maxaddr, maxsz,
	    nsegs, maxsegsz, datarate, flags, &tag);
	if (error != 0)
		return (error);

	/*
	 * This is a root tag. Link it with the device.
	 */
	first = device_set_busdma_tag(dev, tag);
	tag->dt_chain = first;
	*tag_p = tag;
	return (0);
}

int
busdma_tag_derive(struct busdma_tag *base, bus_addr_t align, bus_addr_t bndry,
    bus_addr_t maxaddr, bus_size_t maxsz, u_int nsegs, bus_size_t maxsegsz,
    u_int datarate, u_int flags, struct busdma_tag **tag_p)
{
	struct busdma_tag *tag;
	int error;

	CTR5(KTR_BUSDMA, "%s: base=%p, align=%#jx, bndry=%#jx, maxaddr=%#jx",
	    __func__, base, (uintmax_t)align, (uintmax_t)bndry,
	    (uintmax_t)maxaddr);
	CTR5(KTR_BUSDMA, "%s: maxsz=%#jx, nsegs=%u, maxsegsz=%#jx, flags=%#x",
	    __func__, maxsz, nsegs, (uintmax_t)maxsegsz, flags);

	flags = _busdma_flags(__func__, base->dt_device, flags);

	if (base == NULL || tag_p == NULL)
		return (EINVAL);

	error = _busdma_tag_make(base->dt_device, base, align, bndry, maxaddr,
	    maxsz, nsegs, maxsegsz, datarate, flags, &tag);
	if (error != 0)
		return (error);

	/*
	 * This is a derived tag. Link it with the base tag.
	 */
	tag->dt_parent = base;
	tag->dt_chain = base->dt_child;
	base->dt_child = tag;
	*tag_p = tag;
	return (0);
}

int
busdma_tag_destroy(struct busdma_tag *tag)
{

	CTR2(KTR_BUSDMA, "%s: tag=%p", __func__, tag);

	if (tag == NULL)
		return (EINVAL);

	/* TODO */
	return (0);
}

bus_addr_t
busdma_tag_get_maxaddr(struct busdma_tag *tag)
{

	CTR2(KTR_BUSDMA, "%s: tag=%p", __func__, tag);

	return ((tag != NULL) ? tag->dt_maxaddr : 0UL);
}

int
busdma_md_create(struct busdma_tag *tag, u_int flags, struct busdma_md **md_p)
{
	struct busdma_md *md;

	CTR3(KTR_BUSDMA, "%s: tag=%p, flags=%#x", __func__, tag, flags);

	flags = _busdma_flags(__func__, tag->dt_device, flags);

	if (tag == NULL || md_p == NULL)
		return (EINVAL);

	md = _busdma_md_create(tag, 0);
	if (md == NULL)
		return (ENOMEM);

	_busdma_md_dump(__func__, NULL, md);
	*md_p = md;
	return (0);
}

int
busdma_md_destroy(struct busdma_md *md)
{

	CTR2(KTR_BUSDMA, "%s: md=%p", __func__, md);

	if (md == NULL)
		return (EINVAL);
	if ((md->md_flags & BUSDMA_MD_FLAG_ALLOCATED) != 0)
		return (EINVAL);
	if (md->md_nsegs > 0)
		return (EBUSY);

	_busdma_md_seg_unreserve(md->md_tag);
	uma_zfree(busdma_md_zone, md);
	return (0);
}

bus_addr_t
busdma_md_get_busaddr(struct busdma_md *md, u_int idx)
{
	struct busdma_md_seg *seg;
	bus_addr_t busaddr;

	CTR3(KTR_BUSDMA, "%s: md=%p, idx=%u", __func__, md, idx);

	seg = _busdma_md_get_seg(md, idx);
	busaddr = (seg != NULL) ? seg->mds_busaddr : ~0UL;
	KASSERT(busaddr != ~0UL, ("%s: invalid busaddr", __func__));
	return (busaddr);
}

u_int
busdma_md_get_flags(struct busdma_md *md)
{

	CTR2(KTR_BUSDMA, "%s: md=%p", __func__, md);

	return ((md != NULL) ? md->md_flags : 0);
}

u_int
busdma_md_get_nsegs(struct busdma_md *md)
{

	CTR2(KTR_BUSDMA, "%s: md=%p", __func__, md);

	return ((md != NULL) ? md->md_nsegs : 0);
}

vm_paddr_t
busdma_md_get_paddr(struct busdma_md *md, u_int idx)
{
	struct busdma_md_seg *seg;
	vm_paddr_t paddr;

	CTR3(KTR_BUSDMA, "%s: md=%p, idx=%u", __func__, md, idx);

	seg = _busdma_md_get_seg(md, idx);
	paddr = (seg != NULL) ? seg->mds_paddr : ~0UL;
	KASSERT(paddr != ~0UL, ("%s: invalid paddr", __func__));
	return (paddr);
}

vm_size_t
busdma_md_get_size(struct busdma_md *md, u_int idx)
{
	struct busdma_md_seg *seg;
	vm_size_t size;

	CTR3(KTR_BUSDMA, "%s: md=%p, idx=%u", __func__, md, idx);

	seg = _busdma_md_get_seg(md, idx);
	size = (seg != NULL) ? seg->mds_size : 0UL;
	KASSERT(size != 0UL, ("%s: invalid size", __func__));
	return (size);
}

struct busdma_tag *
busdma_md_get_tag(struct busdma_md *md)
{

	CTR2(KTR_BUSDMA, "%s: md=%p", __func__, md);

	return ((md != NULL) ? md->md_tag : NULL);
}

vm_offset_t
busdma_md_get_vaddr(struct busdma_md *md, u_int idx)
{
	struct busdma_md_seg *seg;
	vm_offset_t vaddr;

	CTR3(KTR_BUSDMA, "%s: md=%p, idx=%u", __func__, md, idx);

	seg = _busdma_md_get_seg(md, idx);
	vaddr = (seg != NULL) ? seg->mds_vaddr : 0UL;
	KASSERT(vaddr != 0UL, ("%s: invalid vaddr", __func__));
	return (vaddr);
}

int
busdma_md_load_ccb(busdma_md_t md, union ccb *ccb, busdma_callback_f cb,
    void *arg, u_int flags)
{
	struct ccb_hdr *ccb_h;
	void *buf;
	size_t cnt, len;
	int error;

	CTR6(KTR_BUSDMA, "%s: md=%p, ccb=%p, cb=%p, arg=%p, flags=%#x",
	    __func__, md, ccb, cb, arg, flags);

	flags = _busdma_flags(__func__, md->md_tag->dt_device, flags);

	if (md == NULL || ccb == NULL)
		return (EINVAL);

	ccb_h = &ccb->ccb_h;
	if ((ccb_h->flags & CAM_DIR_MASK) == CAM_DIR_NONE) {
		(*cb)(arg, NULL, 0);
		return (0);
	}

	switch (ccb_h->func_code) {
	case XPT_SCSI_IO:
		buf = ccb->csio.data_ptr;
		len = ccb->csio.dxfer_len;
		cnt = ccb->csio.sglist_cnt;
		break;
	case XPT_CONT_TARGET_IO:
		buf = ccb->ctio.data_ptr;
		len = ccb->ctio.dxfer_len;
		cnt = ccb->ctio.sglist_cnt;
		break;
	case XPT_ATA_IO:
		buf = ccb->ataio.data_ptr;
		len = ccb->ataio.dxfer_len;
		cnt = 0;
		break;
	default:
		return (EINVAL);
	}

	switch (ccb_h->flags & CAM_DATA_MASK) {
	case CAM_DATA_VADDR:
		error = _busdma_md_load(md, NULL, (uintptr_t)buf, len);
		break;
	case CAM_DATA_PADDR:
	case CAM_DATA_SG:
	case CAM_DATA_SG_PADDR:
	case CAM_DATA_BIO:
	default:
		panic(__func__);
	}

	if (!error) {
		error = _busdma_iommu_map(md->md_tag->dt_device, md);
		if (error)
			printf("_busdma_iommu_map: error=%d\n", error);
	}
	(*cb)(arg, md, error);
	return (0);
}

int
busdma_md_load_linear(struct busdma_md *md, void *buf, size_t len,
    busdma_callback_f cb, void *arg, u_int flags)
{
	int error;

	CTR6(KTR_BUSDMA, "busdma_md_load_linear: md=%p, buf=%p, len=%zu, "
	    "cb=%p, arg=%p, flags=%#x", md, buf, len, cb, arg, flags);

	flags = _busdma_flags(__func__, md->md_tag->dt_device, flags);

	if (md == NULL || buf == NULL || len == 0)
		return (EINVAL);

	error = _busdma_md_load(md, NULL, (uintptr_t)buf, len);
	if (!error) {
		error = _busdma_iommu_map(md->md_tag->dt_device, md);
		if (error)
			printf("_busdma_iommu_map: error=%d\n", error);
	}
	(*cb)(arg, md, error);
	return (0);
}

int
busdma_md_load_phys(struct busdma_md *md, vm_paddr_t buf, size_t len,
    busdma_callback_f cb, void *arg, u_int flags)
{

	CTR6(KTR_BUSDMA, "busdma_md_load_phys: md=%p, buf=%#jx, len=%zu, "
	    "cb=%p, arg=%p, flags=%#x", md, (uintmax_t)buf, len, cb, arg,
	    flags);

	flags = _busdma_flags(__func__, md->md_tag->dt_device, flags);

	panic(__func__);
	(*cb)(arg, md, ENOSYS);
	return (0);
}

int
busdma_md_load_uio(struct busdma_md *md, struct uio *uio,
    busdma_callback_f cb, void *arg, u_int flags)
{

	CTR6(KTR_BUSDMA, "%s: md=%p, uio=%p, cb=%p, arg=%p, flags=%#x",
	    __func__, md, uio, cb, arg, flags);

	flags = _busdma_flags(__func__, md->md_tag->dt_device, flags);

	panic(__func__);
	(*cb)(arg, md, ENOSYS);
	return (0);
}

int
busdma_md_unload(struct busdma_md *md)
{
	struct busdma_md_seg *seg;
	int error;

	CTR2(KTR_BUSDMA, "%s: md=%p", __func__, md);

	if (md == NULL)
		return (EINVAL);
	if ((md->md_flags & BUSDMA_MD_FLAG_ALLOCATED) != 0)
		return (EINVAL);

	if (md->md_nsegs == 0)
		return (0);

	error = _busdma_iommu_unmap(md->md_tag->dt_device, md);
	if (error)
		return (error);

	while ((seg = TAILQ_FIRST(&md->md_seg)) != NULL) {
		TAILQ_REMOVE(&md->md_seg, seg, mds_chain);
		uma_zfree(busdma_md_seg_zone, seg);
	}

	md->md_nsegs = 0;
	return (0);
}

int
busdma_mem_alloc(struct busdma_tag *tag, u_int flags, struct busdma_md **md_p)
{
	struct busdma_md *md;
	struct busdma_md_seg *seg;
	struct busdma_mtag mtag;
	vm_size_t maxsz;
	u_int idx;
	int error, mflags;

	CTR3(KTR_BUSDMA, "%s: tag=%p, flags=%#x", __func__, tag, flags);

	flags = _busdma_flags(__func__, tag->dt_device, flags);

	if (tag == NULL || md_p == NULL)
		return (EINVAL);

	md = _busdma_md_create(tag, BUSDMA_MD_FLAG_ALLOCATED);
	if (md == NULL)
		return (ENOMEM);

	mtag.dmt_flags = flags;
	mtag.dmt_minaddr = tag->dt_minaddr;
	mtag.dmt_maxaddr = tag->dt_maxaddr;
	mtag.dmt_maxsz = tag->dt_maxsegsz;
	mtag.dmt_align = tag->dt_align;
	mtag.dmt_bndry = tag->dt_bndry;

	error = _busdma_iommu_xlate(tag->dt_device, &mtag);
	if (error) {
		printf("_busdma_iommu_xlate: error=%d\n", error);
		goto fail;
	}

	md->md_flags = mtag.dmt_flags;
	mflags = (flags & BUSDMA_ALLOC_ZERO) ? M_ZERO : 0;

	idx = 0;
	maxsz = tag->dt_maxsz;
	while (maxsz > 0 && idx < tag->dt_nsegs) {
		seg = uma_zalloc(busdma_md_seg_zone, M_NOWAIT);
		if (seg == NULL)
			goto fail;
		seg->mds_idx = idx;
		TAILQ_INSERT_TAIL(&md->md_seg, seg, mds_chain);
		md->md_nsegs++;
		seg->mds_busaddr = ~0UL;
		seg->mds_paddr = ~0UL;
		seg->mds_size = MIN(maxsz, mtag.dmt_maxsz);
		seg->mds_vaddr = kmem_alloc_contig(kernel_map, seg->mds_size,
		    mflags, mtag.dmt_minaddr, mtag.dmt_maxaddr, mtag.dmt_align,
		    mtag.dmt_bndry, VM_MEMATTR_DEFAULT);
		if (seg->mds_vaddr == 0) {
			/* TODO: try a smaller segment size */
			goto fail;
		}
		seg->mds_paddr = pmap_kextract(seg->mds_vaddr);
		maxsz -= seg->mds_size;
		idx++;
	}
	if (maxsz == 0) {
		error = _busdma_iommu_map(tag->dt_device, md);
		if (error)
			printf("_busdma_iommu_map: error=%d\n", error);
		*md_p = md;
		return (0);
	}

 fail:
	while ((seg = TAILQ_FIRST(&md->md_seg)) != NULL) {
		if (seg->mds_paddr != ~0UL)
			kmem_free(kernel_map, seg->mds_vaddr, seg->mds_size);
		TAILQ_REMOVE(&md->md_seg, seg, mds_chain);
		uma_zfree(busdma_md_seg_zone, seg);
	}
	uma_zfree(busdma_md_zone, md);
	return (ENOMEM);
}

int
busdma_mem_free(struct busdma_md *md)
{
	struct busdma_md_seg *seg;
	int error;

	CTR2(KTR_BUSDMA, "%s: md=%p", __func__, md);

	if (md == NULL)
		return (EINVAL);
	if ((md->md_flags & BUSDMA_MD_FLAG_ALLOCATED) == 0)
		return (EINVAL);

	error = _busdma_iommu_unmap(md->md_tag->dt_device, md);
	if (error)
		return (error);

	while ((seg = TAILQ_FIRST(&md->md_seg)) != NULL) {
		kmem_free(kernel_map, seg->mds_vaddr, seg->mds_size);
		TAILQ_REMOVE(&md->md_seg, seg, mds_chain);
		uma_zfree(busdma_md_seg_zone, seg);
	}
	uma_zfree(busdma_md_zone, md);
	return (0);
}

int
busdma_sync_range(struct busdma_md *md, u_int op, bus_addr_t addr,
    bus_size_t size)
{
	int error;

	CTR5(KTR_BUSDMA, "%s: md=%p, op=%#x, addr=%#jx, size=%#jx", __func__,
	    md, op, (uintmax_t)addr, (uintmax_t)size);

	error = _busdma_iommu_sync(md->md_tag->dt_device, md, op, addr, size);
	return (error);
}
