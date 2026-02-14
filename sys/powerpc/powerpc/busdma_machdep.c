/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 1997, 1998 Justin T. Gibbs.
 * All rights reserved.
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

/*
 * From amd64/busdma_machdep.c, r204214
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/memdesc.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>

#include <machine/atomic.h>
#include <machine/bus.h>
#include <machine/cpufunc.h>
#include <machine/md_var.h>

#include "iommu_if.h"

/*
 * Allocate a device specific dma_tag.
 *
 * For now this directly calls the bounce dma implementation.
 * This will need refactoring after this.
 */
int
bus_dma_tag_create(bus_dma_tag_t parent, bus_size_t alignment,
		   bus_addr_t boundary, bus_addr_t lowaddr,
		   bus_addr_t highaddr, bus_dma_filter_t *filter,
		   void *filterarg, bus_size_t maxsize, int nsegments,
		   bus_size_t maxsegsz, int flags, bus_dma_lock_t *lockfunc,
		   void *lockfuncarg, bus_dma_tag_t *dmat)
{

	/* Filters are no longer supported. */
	if (filter != NULL || filterarg != NULL)
		return (EINVAL);


	return bus_dma_bounce_impl.tag_create(parent, alignment,
	    boundary, lowaddr, highaddr, maxsize, nsegments,
	    maxsegsz, flags, lockfunc, lockfuncarg, dmat);
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

int
bus_dma_tag_set_domain(bus_dma_tag_t dmat, int domain)
{
	struct bus_dma_tag_common *tc;

	tc = (struct bus_dma_tag_common *)dmat;

	return (tc->impl->tag_set_domain(dmat, domain));
}
