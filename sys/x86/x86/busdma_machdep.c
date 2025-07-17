/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 1997, 1998 Justin T. Gibbs.
 * Copyright (c) 2013 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include "opt_acpi.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/memdesc.h>
#include <sys/mutex.h>
#include <sys/uio.h>
#include <sys/vmmeter.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_param.h>
#include <vm/vm_page.h>
#include <vm/vm_phys.h>
#include <vm/pmap.h>
#include <machine/bus.h>
#include <x86/include/busdma_impl.h>

int
common_bus_dma_tag_create(struct bus_dma_tag_common *parent,
    bus_size_t alignment, bus_addr_t boundary, bus_addr_t lowaddr,
    bus_addr_t highaddr, bus_size_t maxsize, int nsegments, bus_size_t maxsegsz,
    int flags, bus_dma_lock_t *lockfunc, void *lockfuncarg, size_t sz,
    void **dmat)
{
	void *newtag;
	struct bus_dma_tag_common *common;

	KASSERT(sz >= sizeof(struct bus_dma_tag_common), ("sz"));
	/* Basic sanity checking */
	if (boundary != 0 && boundary < maxsegsz)
		maxsegsz = boundary;
	if (maxsegsz == 0)
		return (EINVAL);
	/* Return a NULL tag on failure */
	*dmat = NULL;

	newtag = malloc(sz, M_DEVBUF, M_ZERO | M_NOWAIT);
	if (newtag == NULL) {
		CTR4(KTR_BUSDMA, "%s returned tag %p tag flags 0x%x error %d",
		    __func__, newtag, 0, ENOMEM);
		return (ENOMEM);
	}

	common = newtag;
	common->impl = &bus_dma_bounce_impl;
	common->alignment = alignment;
	common->boundary = boundary;
	common->lowaddr = trunc_page((vm_paddr_t)lowaddr) + (PAGE_SIZE - 1);
	common->highaddr = trunc_page((vm_paddr_t)highaddr) + (PAGE_SIZE - 1);
	common->maxsize = maxsize;
	common->nsegments = nsegments;
	common->maxsegsz = maxsegsz;
	common->flags = flags;
	if (lockfunc != NULL) {
		common->lockfunc = lockfunc;
		common->lockfuncarg = lockfuncarg;
	} else {
		common->lockfunc = _busdma_dflt_lock;
		common->lockfuncarg = NULL;
	}

	/* Take into account any restrictions imposed by our parent tag */
	if (parent != NULL) {
		common->impl = parent->impl;
		common->lowaddr = MIN(parent->lowaddr, common->lowaddr);
		common->highaddr = MAX(parent->highaddr, common->highaddr);
		if (common->boundary == 0)
			common->boundary = parent->boundary;
		else if (parent->boundary != 0) {
			common->boundary = MIN(parent->boundary,
			    common->boundary);
		}

		common->domain = parent->domain;
	}
	common->domain = vm_phys_domain_match(common->domain, 0ul,
	    common->lowaddr);
	*dmat = common;
	return (0);
}

int
bus_dma_tag_set_domain(bus_dma_tag_t dmat, int domain)
{
	struct bus_dma_tag_common *tc;

	tc = (struct bus_dma_tag_common *)dmat;
	domain = vm_phys_domain_match(domain, 0ul, tc->lowaddr);
	/* Only call the callback if it changes. */
	if (domain == tc->domain)
		return (0);
	tc->domain = domain;
	return (tc->impl->tag_set_domain(dmat));
}

/*
 * Allocate a device specific dma_tag.
 */
int
bus_dma_tag_create(bus_dma_tag_t parent, bus_size_t alignment,
    bus_addr_t boundary, bus_addr_t lowaddr, bus_addr_t highaddr,
    bus_dma_filter_t *filter, void *filterarg, bus_size_t maxsize,
    int nsegments, bus_size_t maxsegsz, int flags, bus_dma_lock_t *lockfunc,
    void *lockfuncarg, bus_dma_tag_t *dmat)
{
	struct bus_dma_tag_common *tc;
	int error;

	/* Filters are no longer supported. */
	if (filter != NULL || filterarg != NULL)
		return (EINVAL);

	if (parent == NULL) {
		error = bus_dma_bounce_impl.tag_create(parent, alignment,
		    boundary, lowaddr, highaddr, maxsize, nsegments, maxsegsz,
		    flags, lockfunc, lockfuncarg, dmat);
	} else {
		tc = (struct bus_dma_tag_common *)parent;
		error = tc->impl->tag_create(parent, alignment,
		    boundary, lowaddr, highaddr, maxsize, nsegments, maxsegsz,
		    flags, lockfunc, lockfuncarg, dmat);
	}
	return (error);
}

void
bus_dma_template_clone(bus_dma_template_t *t, bus_dma_tag_t dmat)
{
	struct bus_dma_tag_common *common;

	if (t == NULL || dmat == NULL)
		return;

	common = (struct bus_dma_tag_common *)dmat;

	t->alignment = common->alignment;
	t->boundary = common->boundary;
	t->lowaddr = common->lowaddr;
	t->highaddr = common->highaddr;
	t->maxsize = common->maxsize;
	t->nsegments = common->nsegments;
	t->maxsegsize = common->maxsegsz;
	t->flags = common->flags;
	t->lockfunc = common->lockfunc;
	t->lockfuncarg = common->lockfuncarg;
}

int
bus_dma_tag_destroy(bus_dma_tag_t dmat)
{
	struct bus_dma_tag_common *tc;

	tc = (struct bus_dma_tag_common *)dmat;
	return (tc->impl->tag_destroy(dmat));
}
