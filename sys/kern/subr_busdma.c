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

static struct busdma_tag busdma_root_tag = {
	.dt_maxaddr = ~0UL,
	.dt_align = 1,
	.dt_maxsz = ~0UL,
	.dt_nsegs = ~0U,
	.dt_maxsegsz = ~0UL,
};

static MALLOC_DEFINE(M_BUSDMA_TAG, "busdma_tag", "busdma tag structures");

static void
_busdma_tag_dump(const char *func, device_t dev, busdma_tag_t tag)
{

	printf("[%s: %s: tag=%p (minaddr=%jx, maxaddr=%jx, align=%jx, "
	    "bndry=%jx, maxsz=%jx, nsegs=%u, maxsegsz=%jx)]\n",
	    func, (dev != NULL) ? device_get_nameunit(dev) : "*", tag,
	    (uintmax_t)tag->dt_minaddr, (uintmax_t)tag->dt_maxaddr,
	    (uintmax_t)tag->dt_align, (uintmax_t)tag->dt_bndry,
	    (uintmax_t)tag->dt_maxsz,
	    tag->dt_nsegs, (uintmax_t)tag->dt_maxsegsz);
}

static busdma_tag_t
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
_busdma_tag_make(device_t dev, busdma_tag_t base, bus_addr_t maxaddr,
    bus_addr_t align, bus_addr_t bndry, bus_size_t maxsz, u_int nsegs,
    bus_size_t maxsegsz, u_int flags, busdma_tag_t *tag_p)
{
	busdma_tag_t tag;

	/*
	 * If nsegs is 1, ignore maxsegsz. What this means is that if we have
	 * just 1 segment, then maxsz should be equal to maxsegsz. Make it so.
	 */
	if (nsegs == 1)
		maxsegsz = maxsz;

	tag = (busdma_tag_t)malloc(sizeof(*tag), M_BUSDMA_TAG,
	    M_WAITOK | M_ZERO);
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
busdma_tag_create(device_t dev, bus_addr_t maxaddr, bus_addr_t align,
    bus_addr_t bndry, bus_size_t maxsz, u_int nsegs, bus_size_t maxsegsz,
    u_int flags, busdma_tag_t *tag_p)
{
	busdma_tag_t base, first, tag;
	int error;

	base = _busdma_tag_get_base(dev);
	error = _busdma_tag_make(dev, base, maxaddr, align, bndry, maxsz,
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
busdma_tag_derive(busdma_tag_t base, bus_addr_t maxaddr, bus_addr_t align,
    bus_addr_t bndry, bus_size_t maxsz, u_int nsegs, bus_size_t maxsegsz,
    u_int flags, busdma_tag_t *tag_p)
{
	busdma_tag_t tag;
	int error;

	error = _busdma_tag_make(base->dt_device, base, maxaddr, align, bndry,
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
