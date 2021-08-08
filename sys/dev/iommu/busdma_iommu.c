/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2013 The FreeBSD Foundation
 *
 * This software was developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/domainset.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/memdesc.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/rman.h>
#include <sys/taskqueue.h>
#include <sys/tree.h>
#include <sys/uio.h>
#include <sys/vmem.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <dev/iommu/iommu.h>
#include <machine/atomic.h>
#include <machine/bus.h>
#include <machine/md_var.h>
#include <machine/iommu.h>
#include <dev/iommu/busdma_iommu.h>

/*
 * busdma_iommu.c, the implementation of the busdma(9) interface using
 * IOMMU units from Intel VT-d.
 */

static bool
iommu_bus_dma_is_dev_disabled(int domain, int bus, int slot, int func)
{
	char str[128], *env;
	int default_bounce;
	bool ret;
	static const char bounce_str[] = "bounce";
	static const char iommu_str[] = "iommu";
	static const char dmar_str[] = "dmar"; /* compatibility */

	default_bounce = 0;
	env = kern_getenv("hw.busdma.default");
	if (env != NULL) {
		if (strcmp(env, bounce_str) == 0)
			default_bounce = 1;
		else if (strcmp(env, iommu_str) == 0 ||
		    strcmp(env, dmar_str) == 0)
			default_bounce = 0;
		freeenv(env);
	}

	snprintf(str, sizeof(str), "hw.busdma.pci%d.%d.%d.%d",
	    domain, bus, slot, func);
	env = kern_getenv(str);
	if (env == NULL)
		return (default_bounce != 0);
	if (strcmp(env, bounce_str) == 0)
		ret = true;
	else if (strcmp(env, iommu_str) == 0 ||
	    strcmp(env, dmar_str) == 0)
		ret = false;
	else
		ret = default_bounce != 0;
	freeenv(env);
	return (ret);
}

/*
 * Given original device, find the requester ID that will be seen by
 * the IOMMU unit and used for page table lookup.  PCI bridges may take
 * ownership of transactions from downstream devices, so it may not be
 * the same as the BSF of the target device.  In those cases, all
 * devices downstream of the bridge must share a single mapping
 * domain, and must collectively be assigned to use either IOMMU or
 * bounce mapping.
 */
device_t
iommu_get_requester(device_t dev, uint16_t *rid)
{
	devclass_t pci_class;
	device_t l, pci, pcib, pcip, pcibp, requester;
	int cap_offset;
	uint16_t pcie_flags;
	bool bridge_is_pcie;

	pci_class = devclass_find("pci");
	l = requester = dev;

	*rid = pci_get_rid(dev);

	/*
	 * Walk the bridge hierarchy from the target device to the
	 * host port to find the translating bridge nearest the IOMMU
	 * unit.
	 */
	for (;;) {
		pci = device_get_parent(l);
		KASSERT(pci != NULL, ("iommu_get_requester(%s): NULL parent "
		    "for %s", device_get_name(dev), device_get_name(l)));
		KASSERT(device_get_devclass(pci) == pci_class,
		    ("iommu_get_requester(%s): non-pci parent %s for %s",
		    device_get_name(dev), device_get_name(pci),
		    device_get_name(l)));

		pcib = device_get_parent(pci);
		KASSERT(pcib != NULL, ("iommu_get_requester(%s): NULL bridge "
		    "for %s", device_get_name(dev), device_get_name(pci)));

		/*
		 * The parent of our "bridge" isn't another PCI bus,
		 * so pcib isn't a PCI->PCI bridge but rather a host
		 * port, and the requester ID won't be translated
		 * further.
		 */
		pcip = device_get_parent(pcib);
		if (device_get_devclass(pcip) != pci_class)
			break;
		pcibp = device_get_parent(pcip);

		if (pci_find_cap(l, PCIY_EXPRESS, &cap_offset) == 0) {
			/*
			 * Do not stop the loop even if the target
			 * device is PCIe, because it is possible (but
			 * unlikely) to have a PCI->PCIe bridge
			 * somewhere in the hierarchy.
			 */
			l = pcib;
		} else {
			/*
			 * Device is not PCIe, it cannot be seen as a
			 * requester by IOMMU unit.  Check whether the
			 * bridge is PCIe.
			 */
			bridge_is_pcie = pci_find_cap(pcib, PCIY_EXPRESS,
			    &cap_offset) == 0;
			requester = pcib;

			/*
			 * Check for a buggy PCIe/PCI bridge that
			 * doesn't report the express capability.  If
			 * the bridge above it is express but isn't a
			 * PCI bridge, then we know pcib is actually a
			 * PCIe/PCI bridge.
			 */
			if (!bridge_is_pcie && pci_find_cap(pcibp,
			    PCIY_EXPRESS, &cap_offset) == 0) {
				pcie_flags = pci_read_config(pcibp,
				    cap_offset + PCIER_FLAGS, 2);
				if ((pcie_flags & PCIEM_FLAGS_TYPE) !=
				    PCIEM_TYPE_PCI_BRIDGE)
					bridge_is_pcie = true;
			}

			if (bridge_is_pcie) {
				/*
				 * The current device is not PCIe, but
				 * the bridge above it is.  This is a
				 * PCIe->PCI bridge.  Assume that the
				 * requester ID will be the secondary
				 * bus number with slot and function
				 * set to zero.
				 *
				 * XXX: Doesn't handle the case where
				 * the bridge is PCIe->PCI-X, and the
				 * bridge will only take ownership of
				 * requests in some cases.  We should
				 * provide context entries with the
				 * same page tables for taken and
				 * non-taken transactions.
				 */
				*rid = PCI_RID(pci_get_bus(l), 0, 0);
				l = pcibp;
			} else {
				/*
				 * Neither the device nor the bridge
				 * above it are PCIe.  This is a
				 * conventional PCI->PCI bridge, which
				 * will use the bridge's BSF as the
				 * requester ID.
				 */
				*rid = pci_get_rid(pcib);
				l = pcib;
			}
		}
	}
	return (requester);
}

struct iommu_ctx *
iommu_instantiate_ctx(struct iommu_unit *unit, device_t dev, bool rmrr)
{
	device_t requester;
	struct iommu_ctx *ctx;
	bool disabled;
	uint16_t rid;

	requester = iommu_get_requester(dev, &rid);

	/*
	 * If the user requested the IOMMU disabled for the device, we
	 * cannot disable the IOMMU unit, due to possibility of other
	 * devices on the same IOMMU unit still requiring translation.
	 * Instead provide the identity mapping for the device
	 * context.
	 */
	disabled = iommu_bus_dma_is_dev_disabled(pci_get_domain(requester),
	    pci_get_bus(requester), pci_get_slot(requester), 
	    pci_get_function(requester));
	ctx = iommu_get_ctx(unit, requester, rid, disabled, rmrr);
	if (ctx == NULL)
		return (NULL);
	if (disabled) {
		/*
		 * Keep the first reference on context, release the
		 * later refs.
		 */
		IOMMU_LOCK(unit);
		if ((ctx->flags & IOMMU_CTX_DISABLED) == 0) {
			ctx->flags |= IOMMU_CTX_DISABLED;
			IOMMU_UNLOCK(unit);
		} else {
			iommu_free_ctx_locked(unit, ctx);
		}
		ctx = NULL;
	}
	return (ctx);
}

struct iommu_ctx *
iommu_get_dev_ctx(device_t dev)
{
	struct iommu_unit *unit;

	unit = iommu_find(dev, bootverbose);
	/* Not in scope of any IOMMU ? */
	if (unit == NULL)
		return (NULL);
	if (!unit->dma_enabled)
		return (NULL);

#if defined(__amd64__) || defined(__i386__)
	dmar_quirks_pre_use(unit);
	dmar_instantiate_rmrr_ctxs(unit);
#endif

	return (iommu_instantiate_ctx(unit, dev, false));
}

bus_dma_tag_t
iommu_get_dma_tag(device_t dev, device_t child)
{
	struct iommu_ctx *ctx;
	bus_dma_tag_t res;

	ctx = iommu_get_dev_ctx(child);
	if (ctx == NULL)
		return (NULL);

	res = (bus_dma_tag_t)ctx->tag;
	return (res);
}

bool
bus_dma_iommu_set_buswide(device_t dev)
{
	struct iommu_unit *unit;
	device_t parent;
	u_int busno, slot, func;

	parent = device_get_parent(dev);
	if (device_get_devclass(parent) != devclass_find("pci"))
		return (false);
	unit = iommu_find(dev, bootverbose);
	if (unit == NULL)
		return (false);
	busno = pci_get_bus(dev);
	slot = pci_get_slot(dev);
	func = pci_get_function(dev);
	if (slot != 0 || func != 0) {
		if (bootverbose) {
			device_printf(dev,
			    "iommu%d pci%d:%d:%d requested buswide busdma\n",
			    unit->unit, busno, slot, func);
		}
		return (false);
	}
	iommu_set_buswide_ctx(unit, busno);
	return (true);
}

void
iommu_set_buswide_ctx(struct iommu_unit *unit, u_int busno)
{

	MPASS(busno <= PCI_BUSMAX);
	IOMMU_LOCK(unit);
	unit->buswide_ctxs[busno / NBBY / sizeof(uint32_t)] |=
	    1 << (busno % (NBBY * sizeof(uint32_t)));
	IOMMU_UNLOCK(unit);
}

bool
iommu_is_buswide_ctx(struct iommu_unit *unit, u_int busno)
{

	MPASS(busno <= PCI_BUSMAX);
	return ((unit->buswide_ctxs[busno / NBBY / sizeof(uint32_t)] &
	    (1U << (busno % (NBBY * sizeof(uint32_t))))) != 0);
}

static MALLOC_DEFINE(M_IOMMU_DMAMAP, "iommu_dmamap", "IOMMU DMA Map");

static void iommu_bus_schedule_dmamap(struct iommu_unit *unit,
    struct bus_dmamap_iommu *map);

static int
iommu_bus_dma_tag_create(bus_dma_tag_t parent, bus_size_t alignment,
    bus_addr_t boundary, bus_addr_t lowaddr, bus_addr_t highaddr,
    bus_dma_filter_t *filter, void *filterarg, bus_size_t maxsize,
    int nsegments, bus_size_t maxsegsz, int flags, bus_dma_lock_t *lockfunc,
    void *lockfuncarg, bus_dma_tag_t *dmat)
{
	struct bus_dma_tag_iommu *newtag, *oldtag;
	int error;

	*dmat = NULL;
	error = common_bus_dma_tag_create(parent != NULL ?
	    &((struct bus_dma_tag_iommu *)parent)->common : NULL, alignment,
	    boundary, lowaddr, highaddr, filter, filterarg, maxsize,
	    nsegments, maxsegsz, flags, lockfunc, lockfuncarg,
	    sizeof(struct bus_dma_tag_iommu), (void **)&newtag);
	if (error != 0)
		goto out;

	oldtag = (struct bus_dma_tag_iommu *)parent;
	newtag->common.impl = &bus_dma_iommu_impl;
	newtag->ctx = oldtag->ctx;
	newtag->owner = oldtag->owner;

	*dmat = (bus_dma_tag_t)newtag;
out:
	CTR4(KTR_BUSDMA, "%s returned tag %p tag flags 0x%x error %d",
	    __func__, newtag, (newtag != NULL ? newtag->common.flags : 0),
	    error);
	return (error);
}

static int
iommu_bus_dma_tag_set_domain(bus_dma_tag_t dmat)
{

	return (0);
}

static int
iommu_bus_dma_tag_destroy(bus_dma_tag_t dmat1)
{
	struct bus_dma_tag_iommu *dmat, *parent;
	struct bus_dma_tag_iommu *dmat_copy __unused;
	int error;

	error = 0;
	dmat_copy = dmat = (struct bus_dma_tag_iommu *)dmat1;

	if (dmat != NULL) {
		if (dmat->map_count != 0) {
			error = EBUSY;
			goto out;
		}
		while (dmat != NULL) {
			parent = (struct bus_dma_tag_iommu *)dmat->common.parent;
			if (atomic_fetchadd_int(&dmat->common.ref_count, -1) ==
			    1) {
				if (dmat == dmat->ctx->tag)
					iommu_free_ctx(dmat->ctx);
				free(dmat->segments, M_IOMMU_DMAMAP);
				free(dmat, M_DEVBUF);
				dmat = parent;
			} else
				dmat = NULL;
		}
	}
out:
	CTR3(KTR_BUSDMA, "%s tag %p error %d", __func__, dmat_copy, error);
	return (error);
}

static bool
iommu_bus_dma_id_mapped(bus_dma_tag_t dmat, vm_paddr_t buf, bus_size_t buflen)
{

	return (false);
}

static int
iommu_bus_dmamap_create(bus_dma_tag_t dmat, int flags, bus_dmamap_t *mapp)
{
	struct bus_dma_tag_iommu *tag;
	struct bus_dmamap_iommu *map;

	tag = (struct bus_dma_tag_iommu *)dmat;
	map = malloc_domainset(sizeof(*map), M_IOMMU_DMAMAP,
	    DOMAINSET_PREF(tag->common.domain), M_NOWAIT | M_ZERO);
	if (map == NULL) {
		*mapp = NULL;
		return (ENOMEM);
	}
	if (tag->segments == NULL) {
		tag->segments = malloc_domainset(sizeof(bus_dma_segment_t) *
		    tag->common.nsegments, M_IOMMU_DMAMAP,
		    DOMAINSET_PREF(tag->common.domain), M_NOWAIT);
		if (tag->segments == NULL) {
			free(map, M_IOMMU_DMAMAP);
			*mapp = NULL;
			return (ENOMEM);
		}
	}
	TAILQ_INIT(&map->map_entries);
	map->tag = tag;
	map->locked = true;
	map->cansleep = false;
	tag->map_count++;
	*mapp = (bus_dmamap_t)map;

	return (0);
}

static int
iommu_bus_dmamap_destroy(bus_dma_tag_t dmat, bus_dmamap_t map1)
{
	struct bus_dma_tag_iommu *tag;
	struct bus_dmamap_iommu *map;
	struct iommu_domain *domain;

	tag = (struct bus_dma_tag_iommu *)dmat;
	map = (struct bus_dmamap_iommu *)map1;
	if (map != NULL) {
		domain = tag->ctx->domain;
		IOMMU_DOMAIN_LOCK(domain);
		if (!TAILQ_EMPTY(&map->map_entries)) {
			IOMMU_DOMAIN_UNLOCK(domain);
			return (EBUSY);
		}
		IOMMU_DOMAIN_UNLOCK(domain);
		free(map, M_IOMMU_DMAMAP);
	}
	tag->map_count--;
	return (0);
}


static int
iommu_bus_dmamem_alloc(bus_dma_tag_t dmat, void** vaddr, int flags,
    bus_dmamap_t *mapp)
{
	struct bus_dma_tag_iommu *tag;
	struct bus_dmamap_iommu *map;
	int error, mflags;
	vm_memattr_t attr;

	error = iommu_bus_dmamap_create(dmat, flags, mapp);
	if (error != 0)
		return (error);

	mflags = (flags & BUS_DMA_NOWAIT) != 0 ? M_NOWAIT : M_WAITOK;
	mflags |= (flags & BUS_DMA_ZERO) != 0 ? M_ZERO : 0;
	attr = (flags & BUS_DMA_NOCACHE) != 0 ? VM_MEMATTR_UNCACHEABLE :
	    VM_MEMATTR_DEFAULT;

	tag = (struct bus_dma_tag_iommu *)dmat;
	map = (struct bus_dmamap_iommu *)*mapp;

	if (tag->common.maxsize < PAGE_SIZE &&
	    tag->common.alignment <= tag->common.maxsize &&
	    attr == VM_MEMATTR_DEFAULT) {
		*vaddr = malloc_domainset(tag->common.maxsize, M_DEVBUF,
		    DOMAINSET_PREF(tag->common.domain), mflags);
		map->flags |= BUS_DMAMAP_IOMMU_MALLOC;
	} else {
		*vaddr = (void *)kmem_alloc_attr_domainset(
		    DOMAINSET_PREF(tag->common.domain), tag->common.maxsize,
		    mflags, 0ul, BUS_SPACE_MAXADDR, attr);
		map->flags |= BUS_DMAMAP_IOMMU_KMEM_ALLOC;
	}
	if (*vaddr == NULL) {
		iommu_bus_dmamap_destroy(dmat, *mapp);
		*mapp = NULL;
		return (ENOMEM);
	}
	return (0);
}

static void
iommu_bus_dmamem_free(bus_dma_tag_t dmat, void *vaddr, bus_dmamap_t map1)
{
	struct bus_dma_tag_iommu *tag;
	struct bus_dmamap_iommu *map;

	tag = (struct bus_dma_tag_iommu *)dmat;
	map = (struct bus_dmamap_iommu *)map1;

	if ((map->flags & BUS_DMAMAP_IOMMU_MALLOC) != 0) {
		free(vaddr, M_DEVBUF);
		map->flags &= ~BUS_DMAMAP_IOMMU_MALLOC;
	} else {
		KASSERT((map->flags & BUS_DMAMAP_IOMMU_KMEM_ALLOC) != 0,
		    ("iommu_bus_dmamem_free for non alloced map %p", map));
		kmem_free((vm_offset_t)vaddr, tag->common.maxsize);
		map->flags &= ~BUS_DMAMAP_IOMMU_KMEM_ALLOC;
	}

	iommu_bus_dmamap_destroy(dmat, map1);
}

static int
iommu_bus_dmamap_load_something1(struct bus_dma_tag_iommu *tag,
    struct bus_dmamap_iommu *map, vm_page_t *ma, int offset, bus_size_t buflen,
    int flags, bus_dma_segment_t *segs, int *segp,
    struct iommu_map_entries_tailq *unroll_list)
{
	struct iommu_ctx *ctx;
	struct iommu_domain *domain;
	struct iommu_map_entry *entry;
	iommu_gaddr_t size;
	bus_size_t buflen1;
	int error, idx, gas_flags, seg;

	KASSERT(offset < IOMMU_PAGE_SIZE, ("offset %d", offset));
	if (segs == NULL)
		segs = tag->segments;
	ctx = tag->ctx;
	domain = ctx->domain;
	seg = *segp;
	error = 0;
	idx = 0;
	while (buflen > 0) {
		seg++;
		if (seg >= tag->common.nsegments) {
			error = EFBIG;
			break;
		}
		buflen1 = buflen > tag->common.maxsegsz ?
		    tag->common.maxsegsz : buflen;
		size = round_page(offset + buflen1);

		/*
		 * (Too) optimistically allow split if there are more
		 * then one segments left.
		 */
		gas_flags = map->cansleep ? IOMMU_MF_CANWAIT : 0;
		if (seg + 1 < tag->common.nsegments)
			gas_flags |= IOMMU_MF_CANSPLIT;

		error = iommu_map(domain, &tag->common, size, offset,
		    IOMMU_MAP_ENTRY_READ |
		    ((flags & BUS_DMA_NOWRITE) == 0 ? IOMMU_MAP_ENTRY_WRITE : 0),
		    gas_flags, ma + idx, &entry);
		if (error != 0)
			break;
		if ((gas_flags & IOMMU_MF_CANSPLIT) != 0) {
			KASSERT(size >= entry->end - entry->start,
			    ("split increased entry size %jx %jx %jx",
			    (uintmax_t)size, (uintmax_t)entry->start,
			    (uintmax_t)entry->end));
			size = entry->end - entry->start;
			if (buflen1 > size)
				buflen1 = size;
		} else {
			KASSERT(entry->end - entry->start == size,
			    ("no split allowed %jx %jx %jx",
			    (uintmax_t)size, (uintmax_t)entry->start,
			    (uintmax_t)entry->end));
		}
		if (offset + buflen1 > size)
			buflen1 = size - offset;
		if (buflen1 > tag->common.maxsegsz)
			buflen1 = tag->common.maxsegsz;

		KASSERT(((entry->start + offset) & (tag->common.alignment - 1))
		    == 0,
		    ("alignment failed: ctx %p start 0x%jx offset %x "
		    "align 0x%jx", ctx, (uintmax_t)entry->start, offset,
		    (uintmax_t)tag->common.alignment));
		KASSERT(entry->end <= tag->common.lowaddr ||
		    entry->start >= tag->common.highaddr,
		    ("entry placement failed: ctx %p start 0x%jx end 0x%jx "
		    "lowaddr 0x%jx highaddr 0x%jx", ctx,
		    (uintmax_t)entry->start, (uintmax_t)entry->end,
		    (uintmax_t)tag->common.lowaddr,
		    (uintmax_t)tag->common.highaddr));
		KASSERT(iommu_test_boundary(entry->start + offset, buflen1,
		    tag->common.boundary),
		    ("boundary failed: ctx %p start 0x%jx end 0x%jx "
		    "boundary 0x%jx", ctx, (uintmax_t)entry->start,
		    (uintmax_t)entry->end, (uintmax_t)tag->common.boundary));
		KASSERT(buflen1 <= tag->common.maxsegsz,
		    ("segment too large: ctx %p start 0x%jx end 0x%jx "
		    "buflen1 0x%jx maxsegsz 0x%jx", ctx,
		    (uintmax_t)entry->start, (uintmax_t)entry->end,
		    (uintmax_t)buflen1, (uintmax_t)tag->common.maxsegsz));

		IOMMU_DOMAIN_LOCK(domain);
		TAILQ_INSERT_TAIL(&map->map_entries, entry, dmamap_link);
		entry->flags |= IOMMU_MAP_ENTRY_MAP;
		IOMMU_DOMAIN_UNLOCK(domain);
		TAILQ_INSERT_TAIL(unroll_list, entry, unroll_link);

		segs[seg].ds_addr = entry->start + offset;
		segs[seg].ds_len = buflen1;

		idx += OFF_TO_IDX(trunc_page(offset + buflen1));
		offset += buflen1;
		offset &= IOMMU_PAGE_MASK;
		buflen -= buflen1;
	}
	if (error == 0)
		*segp = seg;
	return (error);
}

static int
iommu_bus_dmamap_load_something(struct bus_dma_tag_iommu *tag,
    struct bus_dmamap_iommu *map, vm_page_t *ma, int offset, bus_size_t buflen,
    int flags, bus_dma_segment_t *segs, int *segp)
{
	struct iommu_ctx *ctx;
	struct iommu_domain *domain;
	struct iommu_map_entry *entry, *entry1;
	struct iommu_map_entries_tailq unroll_list;
	int error;

	ctx = tag->ctx;
	domain = ctx->domain;
	atomic_add_long(&ctx->loads, 1);

	TAILQ_INIT(&unroll_list);
	error = iommu_bus_dmamap_load_something1(tag, map, ma, offset,
	    buflen, flags, segs, segp, &unroll_list);
	if (error != 0) {
		/*
		 * The busdma interface does not allow us to report
		 * partial buffer load, so unfortunately we have to
		 * revert all work done.
		 */
		IOMMU_DOMAIN_LOCK(domain);
		TAILQ_FOREACH_SAFE(entry, &unroll_list, unroll_link,
		    entry1) {
			/*
			 * No entries other than what we have created
			 * during the failed run might have been
			 * inserted there in between, since we own ctx
			 * pglock.
			 */
			TAILQ_REMOVE(&map->map_entries, entry, dmamap_link);
			TAILQ_REMOVE(&unroll_list, entry, unroll_link);
			TAILQ_INSERT_TAIL(&domain->unload_entries, entry,
			    dmamap_link);
		}
		IOMMU_DOMAIN_UNLOCK(domain);
		taskqueue_enqueue(domain->iommu->delayed_taskqueue,
		    &domain->unload_task);
	}

	if (error == ENOMEM && (flags & BUS_DMA_NOWAIT) == 0 &&
	    !map->cansleep)
		error = EINPROGRESS;
	if (error == EINPROGRESS)
		iommu_bus_schedule_dmamap(domain->iommu, map);
	return (error);
}

static int
iommu_bus_dmamap_load_ma(bus_dma_tag_t dmat, bus_dmamap_t map1,
    struct vm_page **ma, bus_size_t tlen, int ma_offs, int flags,
    bus_dma_segment_t *segs, int *segp)
{
	struct bus_dma_tag_iommu *tag;
	struct bus_dmamap_iommu *map;

	tag = (struct bus_dma_tag_iommu *)dmat;
	map = (struct bus_dmamap_iommu *)map1;
	return (iommu_bus_dmamap_load_something(tag, map, ma, ma_offs, tlen,
	    flags, segs, segp));
}

static int
iommu_bus_dmamap_load_phys(bus_dma_tag_t dmat, bus_dmamap_t map1,
    vm_paddr_t buf, bus_size_t buflen, int flags, bus_dma_segment_t *segs,
    int *segp)
{
	struct bus_dma_tag_iommu *tag;
	struct bus_dmamap_iommu *map;
	vm_page_t *ma, fma;
	vm_paddr_t pstart, pend, paddr;
	int error, i, ma_cnt, mflags, offset;

	tag = (struct bus_dma_tag_iommu *)dmat;
	map = (struct bus_dmamap_iommu *)map1;
	pstart = trunc_page(buf);
	pend = round_page(buf + buflen);
	offset = buf & PAGE_MASK;
	ma_cnt = OFF_TO_IDX(pend - pstart);
	mflags = map->cansleep ? M_WAITOK : M_NOWAIT;
	ma = malloc(sizeof(vm_page_t) * ma_cnt, M_DEVBUF, mflags);
	if (ma == NULL)
		return (ENOMEM);
	fma = NULL;
	for (i = 0; i < ma_cnt; i++) {
		paddr = pstart + ptoa(i);
		ma[i] = PHYS_TO_VM_PAGE(paddr);
		if (ma[i] == NULL || VM_PAGE_TO_PHYS(ma[i]) != paddr) {
			/*
			 * If PHYS_TO_VM_PAGE() returned NULL or the
			 * vm_page was not initialized we'll use a
			 * fake page.
			 */
			if (fma == NULL) {
				fma = malloc(sizeof(struct vm_page) * ma_cnt,
				    M_DEVBUF, M_ZERO | mflags);
				if (fma == NULL) {
					free(ma, M_DEVBUF);
					return (ENOMEM);
				}
			}
			vm_page_initfake(&fma[i], pstart + ptoa(i),
			    VM_MEMATTR_DEFAULT);
			ma[i] = &fma[i];
		}
	}
	error = iommu_bus_dmamap_load_something(tag, map, ma, offset, buflen,
	    flags, segs, segp);
	free(fma, M_DEVBUF);
	free(ma, M_DEVBUF);
	return (error);
}

static int
iommu_bus_dmamap_load_buffer(bus_dma_tag_t dmat, bus_dmamap_t map1, void *buf,
    bus_size_t buflen, pmap_t pmap, int flags, bus_dma_segment_t *segs,
    int *segp)
{
	struct bus_dma_tag_iommu *tag;
	struct bus_dmamap_iommu *map;
	vm_page_t *ma, fma;
	vm_paddr_t pstart, pend, paddr;
	int error, i, ma_cnt, mflags, offset;

	tag = (struct bus_dma_tag_iommu *)dmat;
	map = (struct bus_dmamap_iommu *)map1;
	pstart = trunc_page((vm_offset_t)buf);
	pend = round_page((vm_offset_t)buf + buflen);
	offset = (vm_offset_t)buf & PAGE_MASK;
	ma_cnt = OFF_TO_IDX(pend - pstart);
	mflags = map->cansleep ? M_WAITOK : M_NOWAIT;
	ma = malloc(sizeof(vm_page_t) * ma_cnt, M_DEVBUF, mflags);
	if (ma == NULL)
		return (ENOMEM);
	fma = NULL;
	for (i = 0; i < ma_cnt; i++, pstart += PAGE_SIZE) {
		if (pmap == kernel_pmap)
			paddr = pmap_kextract(pstart);
		else
			paddr = pmap_extract(pmap, pstart);
		ma[i] = PHYS_TO_VM_PAGE(paddr);
		if (ma[i] == NULL || VM_PAGE_TO_PHYS(ma[i]) != paddr) {
			/*
			 * If PHYS_TO_VM_PAGE() returned NULL or the
			 * vm_page was not initialized we'll use a
			 * fake page.
			 */
			if (fma == NULL) {
				fma = malloc(sizeof(struct vm_page) * ma_cnt,
				    M_DEVBUF, M_ZERO | mflags);
				if (fma == NULL) {
					free(ma, M_DEVBUF);
					return (ENOMEM);
				}
			}
			vm_page_initfake(&fma[i], paddr, VM_MEMATTR_DEFAULT);
			ma[i] = &fma[i];
		}
	}
	error = iommu_bus_dmamap_load_something(tag, map, ma, offset, buflen,
	    flags, segs, segp);
	free(ma, M_DEVBUF);
	free(fma, M_DEVBUF);
	return (error);
}

static void
iommu_bus_dmamap_waitok(bus_dma_tag_t dmat, bus_dmamap_t map1,
    struct memdesc *mem, bus_dmamap_callback_t *callback, void *callback_arg)
{
	struct bus_dmamap_iommu *map;

	if (map1 == NULL)
		return;
	map = (struct bus_dmamap_iommu *)map1;
	map->mem = *mem;
	map->tag = (struct bus_dma_tag_iommu *)dmat;
	map->callback = callback;
	map->callback_arg = callback_arg;
}

static bus_dma_segment_t *
iommu_bus_dmamap_complete(bus_dma_tag_t dmat, bus_dmamap_t map1,
    bus_dma_segment_t *segs, int nsegs, int error)
{
	struct bus_dma_tag_iommu *tag;
	struct bus_dmamap_iommu *map;

	tag = (struct bus_dma_tag_iommu *)dmat;
	map = (struct bus_dmamap_iommu *)map1;

	if (!map->locked) {
		KASSERT(map->cansleep,
		    ("map not locked and not sleepable context %p", map));

		/*
		 * We are called from the delayed context.  Relock the
		 * driver.
		 */
		(tag->common.lockfunc)(tag->common.lockfuncarg, BUS_DMA_LOCK);
		map->locked = true;
	}

	if (segs == NULL)
		segs = tag->segments;
	return (segs);
}

/*
 * The limitations of busdma KPI forces the iommu to perform the actual
 * unload, consisting of the unmapping of the map entries page tables,
 * from the delayed context on i386, since page table page mapping
 * might require a sleep to be successfull.  The unfortunate
 * consequence is that the DMA requests can be served some time after
 * the bus_dmamap_unload() call returned.
 *
 * On amd64, we assume that sf allocation cannot fail.
 */
static void
iommu_bus_dmamap_unload(bus_dma_tag_t dmat, bus_dmamap_t map1)
{
	struct bus_dma_tag_iommu *tag;
	struct bus_dmamap_iommu *map;
	struct iommu_ctx *ctx;
	struct iommu_domain *domain;
#ifndef IOMMU_DOMAIN_UNLOAD_SLEEP
	struct iommu_map_entries_tailq entries;
#endif

	tag = (struct bus_dma_tag_iommu *)dmat;
	map = (struct bus_dmamap_iommu *)map1;
	ctx = tag->ctx;
	domain = ctx->domain;
	atomic_add_long(&ctx->unloads, 1);

#if defined(IOMMU_DOMAIN_UNLOAD_SLEEP)
	IOMMU_DOMAIN_LOCK(domain);
	TAILQ_CONCAT(&domain->unload_entries, &map->map_entries, dmamap_link);
	IOMMU_DOMAIN_UNLOCK(domain);
	taskqueue_enqueue(domain->iommu->delayed_taskqueue,
	    &domain->unload_task);
#else
	TAILQ_INIT(&entries);
	IOMMU_DOMAIN_LOCK(domain);
	TAILQ_CONCAT(&entries, &map->map_entries, dmamap_link);
	IOMMU_DOMAIN_UNLOCK(domain);
	THREAD_NO_SLEEPING();
	iommu_domain_unload(domain, &entries, false);
	THREAD_SLEEPING_OK();
	KASSERT(TAILQ_EMPTY(&entries), ("lazy iommu_ctx_unload %p", ctx));
#endif
}

static void
iommu_bus_dmamap_sync(bus_dma_tag_t dmat, bus_dmamap_t map,
    bus_dmasync_op_t op)
{
}

struct bus_dma_impl bus_dma_iommu_impl = {
	.tag_create = iommu_bus_dma_tag_create,
	.tag_destroy = iommu_bus_dma_tag_destroy,
	.tag_set_domain = iommu_bus_dma_tag_set_domain,
	.id_mapped = iommu_bus_dma_id_mapped,
	.map_create = iommu_bus_dmamap_create,
	.map_destroy = iommu_bus_dmamap_destroy,
	.mem_alloc = iommu_bus_dmamem_alloc,
	.mem_free = iommu_bus_dmamem_free,
	.load_phys = iommu_bus_dmamap_load_phys,
	.load_buffer = iommu_bus_dmamap_load_buffer,
	.load_ma = iommu_bus_dmamap_load_ma,
	.map_waitok = iommu_bus_dmamap_waitok,
	.map_complete = iommu_bus_dmamap_complete,
	.map_unload = iommu_bus_dmamap_unload,
	.map_sync = iommu_bus_dmamap_sync,
};

static void
iommu_bus_task_dmamap(void *arg, int pending)
{
	struct bus_dma_tag_iommu *tag;
	struct bus_dmamap_iommu *map;
	struct iommu_unit *unit;

	unit = arg;
	IOMMU_LOCK(unit);
	while ((map = TAILQ_FIRST(&unit->delayed_maps)) != NULL) {
		TAILQ_REMOVE(&unit->delayed_maps, map, delay_link);
		IOMMU_UNLOCK(unit);
		tag = map->tag;
		map->cansleep = true;
		map->locked = false;
		bus_dmamap_load_mem((bus_dma_tag_t)tag, (bus_dmamap_t)map,
		    &map->mem, map->callback, map->callback_arg,
		    BUS_DMA_WAITOK);
		map->cansleep = false;
		if (map->locked) {
			(tag->common.lockfunc)(tag->common.lockfuncarg,
			    BUS_DMA_UNLOCK);
		} else
			map->locked = true;
		map->cansleep = false;
		IOMMU_LOCK(unit);
	}
	IOMMU_UNLOCK(unit);
}

static void
iommu_bus_schedule_dmamap(struct iommu_unit *unit, struct bus_dmamap_iommu *map)
{

	map->locked = false;
	IOMMU_LOCK(unit);
	TAILQ_INSERT_TAIL(&unit->delayed_maps, map, delay_link);
	IOMMU_UNLOCK(unit);
	taskqueue_enqueue(unit->delayed_taskqueue, &unit->dmamap_load_task);
}

int
iommu_init_busdma(struct iommu_unit *unit)
{
	int error;

	unit->dma_enabled = 1;
	error = TUNABLE_INT_FETCH("hw.iommu.dma", &unit->dma_enabled);
	if (error == 0) /* compatibility */
		TUNABLE_INT_FETCH("hw.dmar.dma", &unit->dma_enabled);
	TAILQ_INIT(&unit->delayed_maps);
	TASK_INIT(&unit->dmamap_load_task, 0, iommu_bus_task_dmamap, unit);
	unit->delayed_taskqueue = taskqueue_create("iommu", M_WAITOK,
	    taskqueue_thread_enqueue, &unit->delayed_taskqueue);
	taskqueue_start_threads(&unit->delayed_taskqueue, 1, PI_DISK,
	    "iommu%d busdma taskq", unit->unit);
	return (0);
}

void
iommu_fini_busdma(struct iommu_unit *unit)
{

	if (unit->delayed_taskqueue == NULL)
		return;

	taskqueue_drain(unit->delayed_taskqueue, &unit->dmamap_load_task);
	taskqueue_free(unit->delayed_taskqueue);
	unit->delayed_taskqueue = NULL;
}

int
bus_dma_iommu_load_ident(bus_dma_tag_t dmat, bus_dmamap_t map1,
    vm_paddr_t start, vm_size_t length, int flags)
{
	struct bus_dma_tag_common *tc;
	struct bus_dma_tag_iommu *tag;
	struct bus_dmamap_iommu *map;
	struct iommu_ctx *ctx;
	struct iommu_domain *domain;
	struct iommu_map_entry *entry;
	vm_page_t *ma;
	vm_size_t i;
	int error;
	bool waitok;

	MPASS((start & PAGE_MASK) == 0);
	MPASS((length & PAGE_MASK) == 0);
	MPASS(length > 0);
	MPASS(start + length >= start);
	MPASS((flags & ~(BUS_DMA_NOWAIT | BUS_DMA_NOWRITE)) == 0);

	tc = (struct bus_dma_tag_common *)dmat;
	if (tc->impl != &bus_dma_iommu_impl)
		return (0);

	tag = (struct bus_dma_tag_iommu *)dmat;
	ctx = tag->ctx;
	domain = ctx->domain;
	map = (struct bus_dmamap_iommu *)map1;
	waitok = (flags & BUS_DMA_NOWAIT) != 0;

	entry = iommu_map_alloc_entry(domain, waitok ? 0 : IOMMU_PGF_WAITOK);
	if (entry == NULL)
		return (ENOMEM);
	entry->start = start;
	entry->end = start + length;
	ma = malloc(sizeof(vm_page_t) * atop(length), M_TEMP, waitok ?
	    M_WAITOK : M_NOWAIT);
	if (ma == NULL) {
		iommu_map_free_entry(domain, entry);
		return (ENOMEM);
	}
	for (i = 0; i < atop(length); i++) {
		ma[i] = vm_page_getfake(entry->start + PAGE_SIZE * i,
		    VM_MEMATTR_DEFAULT);
	}
	error = iommu_map_region(domain, entry, IOMMU_MAP_ENTRY_READ |
	    ((flags & BUS_DMA_NOWRITE) ? 0 : IOMMU_MAP_ENTRY_WRITE),
	    waitok ? IOMMU_MF_CANWAIT : 0, ma);
	if (error == 0) {
		IOMMU_DOMAIN_LOCK(domain);
		TAILQ_INSERT_TAIL(&map->map_entries, entry, dmamap_link);
		entry->flags |= IOMMU_MAP_ENTRY_MAP;
		IOMMU_DOMAIN_UNLOCK(domain);
	} else {
		iommu_domain_unload_entry(entry, true);
	}
	for (i = 0; i < atop(length); i++)
		vm_page_putfake(ma[i]);
	free(ma, M_TEMP);
	return (error);
}

static void
iommu_domain_unload_task(void *arg, int pending)
{
	struct iommu_domain *domain;
	struct iommu_map_entries_tailq entries;

	domain = arg;
	TAILQ_INIT(&entries);

	for (;;) {
		IOMMU_DOMAIN_LOCK(domain);
		TAILQ_SWAP(&domain->unload_entries, &entries,
		    iommu_map_entry, dmamap_link);
		IOMMU_DOMAIN_UNLOCK(domain);
		if (TAILQ_EMPTY(&entries))
			break;
		iommu_domain_unload(domain, &entries, true);
	}
}

void
iommu_domain_init(struct iommu_unit *unit, struct iommu_domain *domain,
    const struct iommu_domain_map_ops *ops)
{

	domain->ops = ops;
	domain->iommu = unit;

	TASK_INIT(&domain->unload_task, 0, iommu_domain_unload_task, domain);
	RB_INIT(&domain->rb_root);
	TAILQ_INIT(&domain->unload_entries);
	mtx_init(&domain->lock, "iodom", NULL, MTX_DEF);
}

void
iommu_domain_fini(struct iommu_domain *domain)
{

	mtx_destroy(&domain->lock);
}
