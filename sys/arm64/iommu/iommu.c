/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
 *
 * Portions of this work was supported by Innovate UK project 105694,
 * "Digital Security by Design (DSbD) Technology Platform Prototype".
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

#include "opt_platform.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/memdesc.h>
#include <sys/tree.h>
#include <sys/taskqueue.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <vm/vm.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <machine/bus.h>
#include <dev/iommu/busdma_iommu.h>
#include <machine/vmparam.h>

#include "iommu.h"
#include "iommu_if.h"

static MALLOC_DEFINE(M_IOMMU, "IOMMU", "IOMMU framework");

#define	IOMMU_LIST_LOCK()		mtx_lock(&iommu_mtx)
#define	IOMMU_LIST_UNLOCK()		mtx_unlock(&iommu_mtx)
#define	IOMMU_LIST_ASSERT_LOCKED()	mtx_assert(&iommu_mtx, MA_OWNED)

#define dprintf(fmt, ...)

static struct mtx iommu_mtx;

struct iommu_entry {
	struct iommu_unit *iommu;
	LIST_ENTRY(iommu_entry) next;
};
static LIST_HEAD(, iommu_entry) iommu_list = LIST_HEAD_INITIALIZER(iommu_list);

static int
iommu_domain_unmap_buf(struct iommu_domain *iodom, iommu_gaddr_t base,
    iommu_gaddr_t size, int flags)
{
	struct iommu_unit *iommu;
	int error;

	iommu = iodom->iommu;

	error = IOMMU_UNMAP(iommu->dev, iodom, base, size);

	return (error);
}

static int
iommu_domain_map_buf(struct iommu_domain *iodom, iommu_gaddr_t base,
    iommu_gaddr_t size, vm_page_t *ma, uint64_t eflags, int flags)
{
	struct iommu_unit *iommu;
	vm_prot_t prot;
	vm_offset_t va;
	int error;

	dprintf("%s: base %lx, size %lx\n", __func__, base, size);

	prot = 0;
	if (eflags & IOMMU_MAP_ENTRY_READ)
		prot |= VM_PROT_READ;
	if (eflags & IOMMU_MAP_ENTRY_WRITE)
		prot |= VM_PROT_WRITE;

	va = base;

	iommu = iodom->iommu;

	error = IOMMU_MAP(iommu->dev, iodom, va, ma, size, prot);

	return (0);
}

static const struct iommu_domain_map_ops domain_map_ops = {
	.map = iommu_domain_map_buf,
	.unmap = iommu_domain_unmap_buf,
};

static struct iommu_domain *
iommu_domain_alloc(struct iommu_unit *iommu)
{
	struct iommu_domain *iodom;

	iodom = IOMMU_DOMAIN_ALLOC(iommu->dev, iommu);
	if (iodom == NULL)
		return (NULL);

	iommu_domain_init(iommu, iodom, &domain_map_ops);
	iodom->end = VM_MAXUSER_ADDRESS;
	iodom->iommu = iommu;
	iommu_gas_init_domain(iodom);

	return (iodom);
}

static int
iommu_domain_free(struct iommu_domain *iodom)
{
	struct iommu_unit *iommu;

	iommu = iodom->iommu;

	IOMMU_LOCK(iommu);

	if ((iodom->flags & IOMMU_DOMAIN_GAS_INITED) != 0) {
		IOMMU_DOMAIN_LOCK(iodom);
		iommu_gas_fini_domain(iodom);
		IOMMU_DOMAIN_UNLOCK(iodom);
	}

	iommu_domain_fini(iodom);

	IOMMU_DOMAIN_FREE(iommu->dev, iodom);
	IOMMU_UNLOCK(iommu);

	return (0);
}

static void
iommu_tag_init(struct bus_dma_tag_iommu *t)
{
	bus_addr_t maxaddr;

	maxaddr = BUS_SPACE_MAXADDR;

	t->common.ref_count = 0;
	t->common.impl = &bus_dma_iommu_impl;
	t->common.alignment = 1;
	t->common.boundary = 0;
	t->common.lowaddr = maxaddr;
	t->common.highaddr = maxaddr;
	t->common.maxsize = maxaddr;
	t->common.nsegments = BUS_SPACE_UNRESTRICTED;
	t->common.maxsegsz = maxaddr;
}

static struct iommu_ctx *
iommu_ctx_alloc(device_t dev, struct iommu_domain *iodom, bool disabled)
{
	struct iommu_unit *iommu;
	struct iommu_ctx *ioctx;

	iommu = iodom->iommu;

	ioctx = IOMMU_CTX_ALLOC(iommu->dev, iodom, dev, disabled);
	if (ioctx == NULL)
		return (NULL);

	/*
	 * iommu can also be used for non-PCI based devices.
	 * This should be reimplemented as new newbus method with
	 * pci_get_rid() as a default for PCI device class.
	 */
	ioctx->rid = pci_get_rid(dev);

	return (ioctx);
}

struct iommu_ctx *
iommu_get_ctx(struct iommu_unit *iommu, device_t requester,
    uint16_t rid, bool disabled, bool rmrr)
{
	struct iommu_ctx *ioctx;
	struct iommu_domain *iodom;
	struct bus_dma_tag_iommu *tag;

	IOMMU_LOCK(iommu);
	ioctx = IOMMU_CTX_LOOKUP(iommu->dev, requester);
	if (ioctx) {
		IOMMU_UNLOCK(iommu);
		return (ioctx);
	}
	IOMMU_UNLOCK(iommu);

	/*
	 * In our current configuration we have a domain per each ctx.
	 * So allocate a domain first.
	 */
	iodom = iommu_domain_alloc(iommu);
	if (iodom == NULL)
		return (NULL);

	ioctx = iommu_ctx_alloc(requester, iodom, disabled);
	if (ioctx == NULL) {
		iommu_domain_free(iodom);
		return (NULL);
	}

	tag = ioctx->tag = malloc(sizeof(struct bus_dma_tag_iommu),
	    M_IOMMU, M_WAITOK | M_ZERO);
	tag->owner = requester;
	tag->ctx = ioctx;
	tag->ctx->domain = iodom;

	iommu_tag_init(tag);

	ioctx->domain = iodom;

	return (ioctx);
}

void
iommu_free_ctx_locked(struct iommu_unit *iommu, struct iommu_ctx *ioctx)
{
	struct bus_dma_tag_iommu *tag;

	IOMMU_ASSERT_LOCKED(iommu);

	tag = ioctx->tag;

	IOMMU_CTX_FREE(iommu->dev, ioctx);

	free(tag, M_IOMMU);
}

void
iommu_free_ctx(struct iommu_ctx *ioctx)
{
	struct iommu_unit *iommu;
	struct iommu_domain *iodom;
	int error;

	iodom = ioctx->domain;
	iommu = iodom->iommu;

	IOMMU_LOCK(iommu);
	iommu_free_ctx_locked(iommu, ioctx);
	IOMMU_UNLOCK(iommu);

	/* Since we have a domain per each ctx, remove the domain too. */
	error = iommu_domain_free(iodom);
	if (error)
		device_printf(iommu->dev, "Could not free a domain\n");
}

static void
iommu_domain_free_entry(struct iommu_map_entry *entry, bool free)
{
	struct iommu_domain *iodom;

	iodom = entry->domain;

	IOMMU_DOMAIN_LOCK(iodom);
	iommu_gas_free_space(iodom, entry);
	IOMMU_DOMAIN_UNLOCK(iodom);

	if (free)
		iommu_gas_free_entry(iodom, entry);
	else
		entry->flags = 0;
}

void
iommu_domain_unload(struct iommu_domain *iodom,
    struct iommu_map_entries_tailq *entries, bool cansleep)
{
	struct iommu_map_entry *entry, *entry1;
	int error;

	TAILQ_FOREACH_SAFE(entry, entries, dmamap_link, entry1) {
		KASSERT((entry->flags & IOMMU_MAP_ENTRY_MAP) != 0,
		    ("not mapped entry %p %p", iodom, entry));
		error = iodom->ops->unmap(iodom, entry->start, entry->end -
		    entry->start, cansleep ? IOMMU_PGF_WAITOK : 0);
		KASSERT(error == 0, ("unmap %p error %d", iodom, error));
		TAILQ_REMOVE(entries, entry, dmamap_link);
		iommu_domain_free_entry(entry, true);
        }

	if (TAILQ_EMPTY(entries))
		return;

	panic("entries map is not empty");
}

int
iommu_register(struct iommu_unit *iommu)
{
	struct iommu_entry *entry;

	mtx_init(&iommu->lock, "IOMMU", NULL, MTX_DEF);

	entry = malloc(sizeof(struct iommu_entry), M_IOMMU, M_WAITOK | M_ZERO);
	entry->iommu = iommu;

	IOMMU_LIST_LOCK();
	LIST_INSERT_HEAD(&iommu_list, entry, next);
	IOMMU_LIST_UNLOCK();

	iommu_init_busdma(iommu);

	return (0);
}

int
iommu_unregister(struct iommu_unit *iommu)
{
	struct iommu_entry *entry, *tmp;

	IOMMU_LIST_LOCK();
	LIST_FOREACH_SAFE(entry, &iommu_list, next, tmp) {
		if (entry->iommu == iommu) {
			LIST_REMOVE(entry, next);
			free(entry, M_IOMMU);
		}
	}
	IOMMU_LIST_UNLOCK();

	iommu_fini_busdma(iommu);

	mtx_destroy(&iommu->lock);

	return (0);
}

struct iommu_unit *
iommu_find(device_t dev, bool verbose)
{
	struct iommu_entry *entry;
	struct iommu_unit *iommu;
	int error;

	IOMMU_LIST_LOCK();
	LIST_FOREACH(entry, &iommu_list, next) {
		iommu = entry->iommu;
		error = IOMMU_FIND(iommu->dev, dev);
		if (error == 0) {
			IOMMU_LIST_UNLOCK();
			return (entry->iommu);
		}
	}
	IOMMU_LIST_UNLOCK();

	return (NULL);
}

void
iommu_domain_unload_entry(struct iommu_map_entry *entry, bool free)
{

	dprintf("%s\n", __func__);

	iommu_domain_free_entry(entry, free);
}

static void
iommu_init(void)
{

	mtx_init(&iommu_mtx, "IOMMU", NULL, MTX_DEF);
}

SYSINIT(iommu, SI_SUB_DRIVERS, SI_ORDER_FIRST, iommu_init, NULL);
