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
#include <sys/sx.h>
#include <sys/sysctl.h>
#include <vm/vm.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <machine/bus.h>
#include <dev/iommu/busdma_iommu.h>
#include <machine/vmparam.h>

#ifdef FDT
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#endif

#include "iommu.h"
#include "iommu_if.h"

static MALLOC_DEFINE(M_IOMMU, "IOMMU", "IOMMU framework");

#define	IOMMU_LIST_LOCK()		sx_xlock(&iommu_sx)
#define	IOMMU_LIST_UNLOCK()		sx_xunlock(&iommu_sx)
#define	IOMMU_LIST_ASSERT_LOCKED()	sx_assert(&iommu_sx, SA_XLOCKED)

#define dprintf(fmt, ...)

static struct sx iommu_sx;

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

	return (error);
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
iommu_ctx_alloc(device_t requester, struct iommu_domain *iodom, bool disabled)
{
	struct iommu_unit *iommu;
	struct iommu_ctx *ioctx;

	iommu = iodom->iommu;

	ioctx = IOMMU_CTX_ALLOC(iommu->dev, iodom, requester, disabled);
	if (ioctx == NULL)
		return (NULL);

	ioctx->domain = iodom;

	return (ioctx);
}

static int
iommu_ctx_init(device_t requester, struct iommu_ctx *ioctx)
{
	struct bus_dma_tag_iommu *tag;
	struct iommu_domain *iodom;
	struct iommu_unit *iommu;
	int error;

	iodom = ioctx->domain;
	iommu = iodom->iommu;

	error = IOMMU_CTX_INIT(iommu->dev, ioctx);
	if (error)
		return (error);

	tag = ioctx->tag = malloc(sizeof(struct bus_dma_tag_iommu),
	    M_IOMMU, M_WAITOK | M_ZERO);
	tag->owner = requester;
	tag->ctx = ioctx;
	tag->ctx->domain = iodom;

	iommu_tag_init(tag);

	return (error);
}

static struct iommu_unit *
iommu_lookup(device_t dev)
{
	struct iommu_entry *entry;
	struct iommu_unit *iommu;

	IOMMU_LIST_LOCK();
	LIST_FOREACH(entry, &iommu_list, next) {
		iommu = entry->iommu;
		if (iommu->dev == dev) {
			IOMMU_LIST_UNLOCK();
			return (iommu);
		}
	}
	IOMMU_LIST_UNLOCK();

	return (NULL);
}

struct iommu_ctx *
iommu_get_ctx_ofw(device_t dev, int channel)
{
	struct iommu_domain *iodom;
	struct iommu_unit *iommu;
	struct iommu_ctx *ioctx;
	phandle_t node, parent;
	device_t iommu_dev;
	pcell_t *cells;
	int niommus;
	int ncells;
	int error;

	node = ofw_bus_get_node(dev);
	if (node <= 0) {
		device_printf(dev,
		    "%s called on not ofw based device.\n", __func__);
		return (NULL);
	}

	error = ofw_bus_parse_xref_list_get_length(node,
	    "iommus", "#iommu-cells", &niommus);
	if (error) {
		device_printf(dev, "%s can't get iommu list.\n", __func__);
		return (NULL);
	}

	if (niommus == 0) {
		device_printf(dev, "%s iommu list is empty.\n", __func__);
		return (NULL);
	}

	error = ofw_bus_parse_xref_list_alloc(node, "iommus", "#iommu-cells",
	    channel, &parent, &ncells, &cells);
	if (error != 0) {
		device_printf(dev, "%s can't get iommu device xref.\n",
		    __func__);
		return (NULL);
	}

	iommu_dev = OF_device_from_xref(parent);
	if (iommu_dev == NULL) {
		device_printf(dev, "%s can't get iommu device.\n", __func__);
		return (NULL);
	}

	iommu = iommu_lookup(iommu_dev);
	if (iommu == NULL) {
		device_printf(dev, "%s can't lookup iommu.\n", __func__);
		return (NULL);
	}

	/*
	 * In our current configuration we have a domain per each ctx,
	 * so allocate a domain first.
	 */
	iodom = iommu_domain_alloc(iommu);
	if (iodom == NULL) {
		device_printf(dev, "%s can't allocate domain.\n", __func__);
		return (NULL);
	}

	ioctx = iommu_ctx_alloc(dev, iodom, false);
	if (ioctx == NULL) {
		iommu_domain_free(iodom);
		return (NULL);
	}

	ioctx->domain = iodom;

	error = IOMMU_OFW_MD_DATA(iommu->dev, ioctx, cells, ncells);
	if (error) {
		device_printf(dev, "%s can't set MD data\n", __func__);
		return (NULL);
	}

	error = iommu_ctx_init(dev, ioctx);
	if (error) {
		IOMMU_CTX_FREE(iommu->dev, ioctx);
		iommu_domain_free(iodom);
		return (NULL);
	}

	return (ioctx);
}

struct iommu_ctx *
iommu_get_ctx(struct iommu_unit *iommu, device_t requester,
    uint16_t rid, bool disabled, bool rmrr)
{
	struct iommu_domain *iodom;
	struct iommu_ctx *ioctx;
	int error;

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

	error = iommu_ctx_init(requester, ioctx);
	if (error) {
		IOMMU_CTX_FREE(iommu->dev, ioctx);
		iommu_domain_free(iodom);
		return (NULL);
	}

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
	iommu_gas_free_space(entry);

	if (free)
		iommu_gas_free_entry(entry);
	else
		entry->flags = 0;
}

void
iommu_domain_unload(struct iommu_domain *iodom,
    struct iommu_map_entries_tailq *entries, bool cansleep)
{
	struct iommu_map_entry *entry, *entry1;
	int error __diagused;

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
iommu_domain_unload_entry(struct iommu_map_entry *entry, bool free,
    bool cansleep __unused)
{

	dprintf("%s\n", __func__);

	iommu_domain_free_entry(entry, free);
}

static void
iommu_init(void)
{

	sx_init(&iommu_sx, "IOMMU list");
}

SYSINIT(iommu, SI_SUB_DRIVERS, SI_ORDER_FIRST, iommu_init, NULL);
