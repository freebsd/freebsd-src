/*-
 * Copyright (c) 2012 Marcel Moolenaar
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
#include <sys/malloc.h>
#include <machine/stdarg.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>

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
	u_int		dt_nsegs;
	bus_size_t	dt_maxsegsz;
};

struct busdma_md_seg {
	bus_addr_t	mds_busaddr;
	vm_paddr_t	mds_paddr;
	vm_offset_t	mds_vaddr;
	vm_size_t	mds_size;
};

struct busdma_md {
	struct busdma_tag *md_tag;
	u_int		md_flags;
	u_int		md_nsegs;
	struct busdma_md_seg md_seg[0];
};

#define	BUSDMA_MD_FLAG_ALLOCATED	0x1	/* busdma_mem_alloc() created
						   this MD. */
#define	BUSDMA_MD_FLAG_LOADED		0x2	/* The MD is loaded. */
#define	BUSDMA_MD_FLAG_MAPPED		0x4	/* KVA is valid. */
#define	BUSDMA_MD_FLAG_USED		\
		(BUSDMA_MD_FLAG_ALLOCATED | BUSDMA_MD_FLAG_LOADED)

static struct busdma_tag busdma_root_tag = {
	.dt_maxaddr = ~0UL,
	.dt_align = 1,

	/*
	 * Make dt_maxsz the largest power of 2. I don't like ~0 as the
	 * maximum size. 0 would be a good number to signal (virtually)
	 * unrestricted DMA sizes, but that creates an irregularity for
	 * merging restrictions.
	 */
	.dt_maxsz = (~0UL >> 1) + 1,

	/*
	 * Arbitrarily limit the number of scatter/gather segments to
	 * 1K. This to avoid that some driver actually tries to do
	 * DMA with unlimited segments and we try to allocate a memory
	 * descriptor for it. Why 1K? "It looked like a good idea at
	 * the time" (read: no particular reason).
	 */
	.dt_nsegs = 1024,

	/*
	 * Just like dt_maxsz, limit to the largest power of 2.
	 */
	.dt_maxsegsz = (~0UL >> 1) + 1,
};

static MALLOC_DEFINE(M_BUSDMA_MD, "busdma_md", "DMA memory descriptors");
static MALLOC_DEFINE(M_BUSDMA_TAG, "busdma_tag", "DMA tags");

static void
_busdma_tag_dump(const char *func, device_t dev, struct busdma_tag *tag)
{

	printf("[%s: %s: tag=%p (minaddr=%jx, maxaddr=%jx, align=%jx, "
	    "bndry=%jx, maxsz=%jx, nsegs=%u, maxsegsz=%jx)]\n",
	    func, (dev != NULL) ? device_get_nameunit(dev) : "*", tag,
	    (uintmax_t)tag->dt_minaddr, (uintmax_t)tag->dt_maxaddr,
	    (uintmax_t)tag->dt_align, (uintmax_t)tag->dt_bndry,
	    (uintmax_t)tag->dt_maxsz,
	    tag->dt_nsegs, (uintmax_t)tag->dt_maxsegsz);
}

static void
_busdma_md_dump(const char *func, struct busdma_md *md) 
{
	struct busdma_tag *tag;
	struct busdma_md_seg *seg;
	int idx;

	tag = md->md_tag;
	printf("[%s: %s: md=%p (tag=%p, flags=%x, nsegs=%u)", func,
	    device_get_nameunit(tag->dt_device), md, tag, md->md_flags,
	    md->md_nsegs);
	if (md->md_nsegs == 0) {
		printf(" -- UNUSED]\n");
		return;
	}
	for (idx = 0; idx < md->md_nsegs; idx++) {
		seg = &md->md_seg[idx];
		printf(", %u={size=%jx, busaddr=%jx, paddr=%jx, vaddr=%jx}",
		    idx, seg->mds_size, seg->mds_busaddr, seg->mds_paddr,
		    seg->mds_vaddr);
	}
	printf("]\n");
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
		base = &busdma_root_tag;
		parent = NULL;
	}
	_busdma_tag_dump(__func__, parent, base);
	return (base);
}

static int
_busdma_tag_make(device_t dev, struct busdma_tag *base, bus_addr_t align,
    bus_addr_t bndry, bus_addr_t maxaddr, bus_size_t maxsz, u_int nsegs,
    bus_size_t maxsegsz, u_int flags, struct busdma_tag **tag_p)
{
	struct busdma_tag *tag;

	/*
	 * If nsegs is 1, ignore maxsegsz. What this means is that if we have
	 * just 1 segment, then maxsz should be equal to maxsegsz. To keep it
	 * simple for us, limit maxsegsz to maxsz in any case.
	 */
	if (maxsegsz > maxsz || nsegs == 1)
		maxsegsz = maxsz;

	tag = malloc(sizeof(*tag), M_BUSDMA_TAG, M_NOWAIT | M_ZERO);
	tag->dt_device = dev;
	tag->dt_minaddr = MAX(0, base->dt_minaddr);
	tag->dt_maxaddr = MIN(maxaddr, base->dt_maxaddr);
	tag->dt_align = MAX(align, base->dt_align);
	tag->dt_bndry = MIN(bndry, base->dt_bndry);
	tag->dt_maxsz = MIN(maxsz, base->dt_maxsz);
	tag->dt_nsegs = MIN(nsegs, base->dt_nsegs);
	tag->dt_maxsegsz = MIN(maxsegsz, base->dt_maxsegsz);
	_busdma_tag_dump(__func__, dev, tag);
	*tag_p = tag;
	return (0);
}

int
busdma_tag_create(device_t dev, bus_addr_t align, bus_addr_t bndry,
    bus_addr_t maxaddr, bus_size_t maxsz, u_int nsegs, bus_size_t maxsegsz,
    u_int flags, struct busdma_tag **tag_p)
{
	struct busdma_tag *base, *first, *tag;
	int error;

	base = _busdma_tag_get_base(dev);
	error = _busdma_tag_make(dev, base, align, bndry, maxaddr, maxsz,
	    nsegs, maxsegsz, flags, &tag);
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
    u_int flags, struct busdma_tag **tag_p)
{
	struct busdma_tag *tag;
	int error;

	error = _busdma_tag_make(base->dt_device, base, align, bndry, maxaddr,
	    maxsz, nsegs, maxsegsz, flags, &tag);
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

	return (0);
}

int
busdma_md_create(struct busdma_tag *tag, u_int flags, struct busdma_md **md_p)
{

	return (ENOSYS);
}

int
busdma_md_destroy(struct busdma_md *md)
{

	return (ENOSYS);
}

bus_addr_t
busdma_md_get_busaddr(struct busdma_md *md, u_int idx)
{

	if (idx >= md->md_tag->dt_nsegs)
		return (0);

	return (md->md_seg[idx].mds_busaddr);
}

u_int
busdma_md_get_nsegs(struct busdma_md *md)
{

	return (md->md_nsegs);
}

vm_paddr_t
busdma_md_get_paddr(struct busdma_md *md, u_int idx)
{

	if (idx >= md->md_tag->dt_nsegs)
		return (0);

	return (md->md_seg[idx].mds_paddr);
}

vm_size_t
busdma_md_get_size(struct busdma_md *md, u_int idx)
{

	if (idx >= md->md_tag->dt_nsegs)
		return (0);

	return (md->md_seg[idx].mds_size);
}

vm_offset_t
busdma_md_get_vaddr(struct busdma_md *md, u_int idx)
{

	if (idx >= md->md_tag->dt_nsegs)
		return (0);

	return (md->md_seg[idx].mds_vaddr);
}

int
busdma_md_load_linear(struct busdma_md *md, void *buf, size_t len,
    busdma_callback_f cb, void *arg, u_int flags)
{

	return (ENOSYS);
}

int
busdma_md_unload(struct busdma_md *md)
{

	return (ENOSYS);
}

int
busdma_mem_alloc(struct busdma_tag *tag, u_int flags, struct busdma_md **md_p)
{
	struct busdma_md *md;
	struct busdma_md_seg *seg;
	size_t mdsz;
	vm_size_t maxsz;
	u_int idx;

	mdsz = sizeof(struct busdma_md) +
	    sizeof(struct busdma_md_seg) * tag->dt_nsegs;
	md = malloc(mdsz, M_BUSDMA_MD, M_NOWAIT | M_ZERO);
	md->md_tag = tag;

	idx = 0;
	maxsz = tag->dt_maxsz;
	while (maxsz > 0 && idx < tag->dt_nsegs) {
		seg = &md->md_seg[idx];
		seg->mds_size = MIN(maxsz, tag->dt_maxsegsz);
		seg->mds_vaddr = kmem_alloc_contig(kernel_map, seg->mds_size,
		    0, tag->dt_minaddr, tag->dt_maxaddr, tag->dt_align,
		    tag->dt_bndry, VM_MEMATTR_DEFAULT);
		if (seg->mds_vaddr == 0) {
			/* TODO: try a smaller segment size */
			goto fail;
		}
		seg->mds_paddr = pmap_kextract(seg->mds_vaddr);
		seg->mds_busaddr = seg->mds_paddr;
		maxsz -= seg->mds_size;
		idx++;
	}
	if (maxsz == 0) {
		md->md_nsegs = idx;
		_busdma_md_dump(__func__, md);
		*md_p = md;
		return (0);
	}

 fail:
	seg = &md->md_seg[0];
	while (seg != &md->md_seg[idx]) {
		kmem_free(kernel_map, seg->mds_vaddr, seg->mds_size);
		seg++;
	}
	free(md, M_BUSDMA_MD);
	return (ENOMEM);
}

int
busdma_mem_free(struct busdma_md *md)
{

	free(md, M_BUSDMA_MD);
	return (0);
}

void
busdma_sync(struct busdma_md *md, u_int op)
{
}

void
busdma_sync_range(struct busdma_md *md, u_int op, bus_addr_t addr,
    bus_size_t len)
{
}
