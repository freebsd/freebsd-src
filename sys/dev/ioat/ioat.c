/*-
 * Copyright (C) 2012 Intel Corporation
 * All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/ioccom.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/stdarg.h>

#include "ioat.h"
#include "ioat_hw.h"
#include "ioat_internal.h"

static int ioat_probe(device_t device);
static int ioat_attach(device_t device);
static int ioat_detach(device_t device);
static int ioat_setup_intr(struct ioat_softc *ioat);
static int ioat_teardown_intr(struct ioat_softc *ioat);
static int ioat3_attach(device_t device);
static int ioat_map_pci_bar(struct ioat_softc *ioat);
static void ioat_dmamap_cb(void *arg, bus_dma_segment_t *segs, int nseg,
    int error);
static void ioat_interrupt_handler(void *arg);
static boolean_t ioat_model_resets_msix(struct ioat_softc *ioat);
static void ioat_process_events(struct ioat_softc *ioat);
static inline uint32_t ioat_get_active(struct ioat_softc *ioat);
static inline uint32_t ioat_get_ring_space(struct ioat_softc *ioat);
static void ioat_free_ring_entry(struct ioat_softc *ioat,
    struct ioat_descriptor *desc);
static struct ioat_descriptor * ioat_alloc_ring_entry(struct ioat_softc *ioat);
static int ioat_reserve_space_and_lock(struct ioat_softc *ioat, int num_descs);
static struct ioat_descriptor * ioat_get_ring_entry(struct ioat_softc *ioat,
    uint32_t index);
static boolean_t resize_ring(struct ioat_softc *ioat, int order);
static void ioat_timer_callback(void *arg);
static void dump_descriptor(void *hw_desc);
static void ioat_submit_single(struct ioat_softc *ioat);
static void ioat_comp_update_map(void *arg, bus_dma_segment_t *seg, int nseg,
    int error);
static int ioat_reset_hw(struct ioat_softc *ioat);
static void ioat_setup_sysctl(device_t device);

MALLOC_DEFINE(M_IOAT, "ioat", "ioat driver memory allocations");
SYSCTL_NODE(_hw, OID_AUTO, ioat, CTLFLAG_RD, 0, "ioat node");

static int g_force_legacy_interrupts;
SYSCTL_INT(_hw_ioat, OID_AUTO, force_legacy_interrupts, CTLFLAG_RDTUN,
    &g_force_legacy_interrupts, 0, "Set to non-zero to force MSI-X disabled");

static int g_ioat_debug_level = 0;
SYSCTL_INT(_hw_ioat, OID_AUTO, debug_level, CTLFLAG_RWTUN, &g_ioat_debug_level,
    0, "Set log level (0-3) for ioat(4). Higher is more verbose.");

/*
 * OS <-> Driver interface structures
 */
static device_method_t ioat_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,     ioat_probe),
	DEVMETHOD(device_attach,    ioat_attach),
	DEVMETHOD(device_detach,    ioat_detach),
	{ 0, 0 }
};

static driver_t ioat_pci_driver = {
	"ioat",
	ioat_pci_methods,
	sizeof(struct ioat_softc),
};

static devclass_t ioat_devclass;
DRIVER_MODULE(ioat, pci, ioat_pci_driver, ioat_devclass, 0, 0);

/*
 * Private data structures
 */
static struct ioat_softc *ioat_channel[IOAT_MAX_CHANNELS];
static int ioat_channel_index = 0;
SYSCTL_INT(_hw_ioat, OID_AUTO, channels, CTLFLAG_RD, &ioat_channel_index, 0,
    "Number of IOAT channels attached");

static struct _pcsid
{
	u_int32_t   type;
	const char  *desc;
} pci_ids[] = {
	{ 0x34308086, "TBG IOAT Ch0" },
	{ 0x34318086, "TBG IOAT Ch1" },
	{ 0x34328086, "TBG IOAT Ch2" },
	{ 0x34338086, "TBG IOAT Ch3" },
	{ 0x34298086, "TBG IOAT Ch4" },
	{ 0x342a8086, "TBG IOAT Ch5" },
	{ 0x342b8086, "TBG IOAT Ch6" },
	{ 0x342c8086, "TBG IOAT Ch7" },

	{ 0x37108086, "JSF IOAT Ch0" },
	{ 0x37118086, "JSF IOAT Ch1" },
	{ 0x37128086, "JSF IOAT Ch2" },
	{ 0x37138086, "JSF IOAT Ch3" },
	{ 0x37148086, "JSF IOAT Ch4" },
	{ 0x37158086, "JSF IOAT Ch5" },
	{ 0x37168086, "JSF IOAT Ch6" },
	{ 0x37178086, "JSF IOAT Ch7" },
	{ 0x37188086, "JSF IOAT Ch0 (RAID)" },
	{ 0x37198086, "JSF IOAT Ch1 (RAID)" },

	{ 0x3c208086, "SNB IOAT Ch0" },
	{ 0x3c218086, "SNB IOAT Ch1" },
	{ 0x3c228086, "SNB IOAT Ch2" },
	{ 0x3c238086, "SNB IOAT Ch3" },
	{ 0x3c248086, "SNB IOAT Ch4" },
	{ 0x3c258086, "SNB IOAT Ch5" },
	{ 0x3c268086, "SNB IOAT Ch6" },
	{ 0x3c278086, "SNB IOAT Ch7" },
	{ 0x3c2e8086, "SNB IOAT Ch0 (RAID)" },
	{ 0x3c2f8086, "SNB IOAT Ch1 (RAID)" },

	{ 0x0e208086, "IVB IOAT Ch0" },
	{ 0x0e218086, "IVB IOAT Ch1" },
	{ 0x0e228086, "IVB IOAT Ch2" },
	{ 0x0e238086, "IVB IOAT Ch3" },
	{ 0x0e248086, "IVB IOAT Ch4" },
	{ 0x0e258086, "IVB IOAT Ch5" },
	{ 0x0e268086, "IVB IOAT Ch6" },
	{ 0x0e278086, "IVB IOAT Ch7" },
	{ 0x0e2e8086, "IVB IOAT Ch0 (RAID)" },
	{ 0x0e2f8086, "IVB IOAT Ch1 (RAID)" },

	{ 0x2f208086, "HSW IOAT Ch0" },
	{ 0x2f218086, "HSW IOAT Ch1" },
	{ 0x2f228086, "HSW IOAT Ch2" },
	{ 0x2f238086, "HSW IOAT Ch3" },
	{ 0x2f248086, "HSW IOAT Ch4" },
	{ 0x2f258086, "HSW IOAT Ch5" },
	{ 0x2f268086, "HSW IOAT Ch6" },
	{ 0x2f278086, "HSW IOAT Ch7" },
	{ 0x2f2e8086, "HSW IOAT Ch0 (RAID)" },
	{ 0x2f2f8086, "HSW IOAT Ch1 (RAID)" },

	{ 0x0c508086, "BWD IOAT Ch0" },
	{ 0x0c518086, "BWD IOAT Ch1" },
	{ 0x0c528086, "BWD IOAT Ch2" },
	{ 0x0c538086, "BWD IOAT Ch3" },

	{ 0x6f508086, "BDXDE IOAT Ch0" },
	{ 0x6f518086, "BDXDE IOAT Ch1" },
	{ 0x6f528086, "BDXDE IOAT Ch2" },
	{ 0x6f538086, "BDXDE IOAT Ch3" },

	{ 0x00000000, NULL           }
};

/*
 * OS <-> Driver linkage functions
 */
static int
ioat_probe(device_t device)
{
	struct _pcsid *ep;
	u_int32_t type;

	type = pci_get_devid(device);
	for (ep = pci_ids; ep->type; ep++) {
		if (ep->type == type) {
			device_set_desc(device, ep->desc);
			return (0);
		}
	}
	return (ENXIO);
}

static int
ioat_attach(device_t device)
{
	struct ioat_softc *ioat;
	int error;

	ioat = DEVICE2SOFTC(device);
	ioat->device = device;

	error = ioat_map_pci_bar(ioat);
	if (error != 0)
		goto err;

	ioat->version = ioat_read_cbver(ioat);
	if (ioat->version < IOAT_VER_3_0) {
		error = ENODEV;
		goto err;
	}

	error = ioat_setup_intr(ioat);
	if (error != 0)
		return (error);

	error = ioat3_attach(device);
	if (error != 0)
		goto err;

	error = pci_enable_busmaster(device);
	if (error != 0)
		goto err;

	ioat_channel[ioat_channel_index++] = ioat;

err:
	if (error != 0)
		ioat_detach(device);
	return (error);
}

static int
ioat_detach(device_t device)
{
	struct ioat_softc *ioat;
	uint32_t i;

	ioat = DEVICE2SOFTC(device);
	callout_drain(&ioat->timer);

	pci_disable_busmaster(device);

	if (ioat->pci_resource != NULL)
		bus_release_resource(device, SYS_RES_MEMORY,
		    ioat->pci_resource_id, ioat->pci_resource);

	if (ioat->ring != NULL) {
		for (i = 0; i < (1 << ioat->ring_size_order); i++)
			ioat_free_ring_entry(ioat, ioat->ring[i]);
		free(ioat->ring, M_IOAT);
	}

	if (ioat->comp_update != NULL) {
		bus_dmamap_unload(ioat->comp_update_tag, ioat->comp_update_map);
		bus_dmamem_free(ioat->comp_update_tag, ioat->comp_update,
		    ioat->comp_update_map);
		bus_dma_tag_destroy(ioat->comp_update_tag);
	}

	bus_dma_tag_destroy(ioat->hw_desc_tag);

	ioat_teardown_intr(ioat);

	return (0);
}

static int
ioat_teardown_intr(struct ioat_softc *ioat)
{

	if (ioat->tag != NULL)
		bus_teardown_intr(ioat->device, ioat->res, ioat->tag);

	if (ioat->res != NULL)
		bus_release_resource(ioat->device, SYS_RES_IRQ,
		    rman_get_rid(ioat->res), ioat->res);

	pci_release_msi(ioat->device);
	return (0);
}

static int
ioat3_selftest(struct ioat_softc *ioat)
{
	uint64_t status;
	uint32_t chanerr;
	int i;

	ioat_acquire(&ioat->dmaengine);
	ioat_null(&ioat->dmaengine, NULL, NULL, 0);
	ioat_release(&ioat->dmaengine);

	for (i = 0; i < 100; i++) {
		DELAY(1);
		status = ioat_get_chansts(ioat);
		if (is_ioat_idle(status))
			return (0);
	}

	chanerr = ioat_read_4(ioat, IOAT_CHANERR_OFFSET);
	ioat_log_message(0, "could not start channel: "
	    "status = %#jx error = %x\n", (uintmax_t)status, chanerr);
	return (ENXIO);
}

/*
 * Initialize Hardware
 */
static int
ioat3_attach(device_t device)
{
	struct ioat_softc *ioat;
	struct ioat_descriptor **ring;
	struct ioat_descriptor *next;
	struct ioat_dma_hw_descriptor *dma_hw_desc;
	uint32_t capabilities;
	int i, num_descriptors;
	int error;
	uint8_t xfercap;

	error = 0;
	ioat = DEVICE2SOFTC(device);
	capabilities = ioat_read_dmacapability(ioat);

	xfercap = ioat_read_xfercap(ioat);

	/* Only bits [4:0] are valid. */
	xfercap &= 0x1f;
	ioat->max_xfer_size = 1 << xfercap;

	/* TODO: need to check DCA here if we ever do XOR/PQ */

	mtx_init(&ioat->submit_lock, "ioat_submit", NULL, MTX_DEF);
	mtx_init(&ioat->cleanup_lock, "ioat_process_events", NULL, MTX_DEF);
	callout_init(&ioat->timer, CALLOUT_MPSAFE);

	ioat->is_resize_pending = FALSE;
	ioat->is_completion_pending = FALSE;
	ioat->is_reset_pending = FALSE;
	ioat->is_channel_running = FALSE;
	ioat->is_waiting_for_ack = FALSE;

	bus_dma_tag_create(bus_get_dma_tag(ioat->device), sizeof(uint64_t), 0x0,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL,
	    sizeof(uint64_t), 1, sizeof(uint64_t), 0, NULL, NULL,
	    &ioat->comp_update_tag);

	error = bus_dmamem_alloc(ioat->comp_update_tag,
	    (void **)&ioat->comp_update, BUS_DMA_ZERO, &ioat->comp_update_map);
	if (ioat->comp_update == NULL)
		return (ENOMEM);

	error = bus_dmamap_load(ioat->comp_update_tag, ioat->comp_update_map,
	    ioat->comp_update, sizeof(uint64_t), ioat_comp_update_map, ioat,
	    0);
	if (error != 0)
		return (error);

	ioat->ring_size_order = IOAT_MIN_ORDER;

	num_descriptors = 1 << ioat->ring_size_order;

	bus_dma_tag_create(bus_get_dma_tag(ioat->device), 0x40, 0x0,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL,
	    sizeof(struct ioat_dma_hw_descriptor), 1,
	    sizeof(struct ioat_dma_hw_descriptor), 0, NULL, NULL,
	    &ioat->hw_desc_tag);

	ioat->ring = malloc(num_descriptors * sizeof(*ring), M_IOAT,
	    M_ZERO | M_NOWAIT);
	if (ioat->ring == NULL)
		return (ENOMEM);

	ring = ioat->ring;
	for (i = 0; i < num_descriptors; i++) {
		ring[i] = ioat_alloc_ring_entry(ioat);
		if (ring[i] == NULL)
			return (ENOMEM);

		ring[i]->id = i;
	}

	for (i = 0; i < num_descriptors - 1; i++) {
		next = ring[i + 1];
		dma_hw_desc = ring[i]->u.dma;

		dma_hw_desc->next = next->hw_desc_bus_addr;
	}

	ring[i]->u.dma->next = ring[0]->hw_desc_bus_addr;

	ioat->head = 0;
	ioat->tail = 0;
	ioat->last_seen = 0;

	error = ioat_reset_hw(ioat);
	if (error != 0)
		return (error);

	ioat_write_chanctrl(ioat, IOAT_CHANCTRL_RUN);
	ioat_write_chancmp(ioat, ioat->comp_update_bus_addr);
	ioat_write_chainaddr(ioat, ring[0]->hw_desc_bus_addr);

	error = ioat3_selftest(ioat);
	if (error != 0)
		return (error);

	ioat_process_events(ioat);
	ioat_setup_sysctl(device);
	return (0);
}

static int
ioat_map_pci_bar(struct ioat_softc *ioat)
{

	ioat->pci_resource_id = PCIR_BAR(0);
	ioat->pci_resource = bus_alloc_resource(ioat->device, SYS_RES_MEMORY,
	    &ioat->pci_resource_id, 0, ~0, 1, RF_ACTIVE);

	if (ioat->pci_resource == NULL) {
		ioat_log_message(0, "unable to allocate pci resource\n");
		return (ENODEV);
	}

	ioat->pci_bus_tag = rman_get_bustag(ioat->pci_resource);
	ioat->pci_bus_handle = rman_get_bushandle(ioat->pci_resource);
	return (0);
}

static void
ioat_comp_update_map(void *arg, bus_dma_segment_t *seg, int nseg, int error)
{
	struct ioat_softc *ioat = arg;

	ioat->comp_update_bus_addr = seg[0].ds_addr;
}

static void
ioat_dmamap_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	bus_addr_t *baddr;

	baddr = arg;
	*baddr = segs->ds_addr;
}

/*
 * Interrupt setup and handlers
 */
static int
ioat_setup_intr(struct ioat_softc *ioat)
{
	uint32_t num_vectors;
	int error;
	boolean_t use_msix;
	boolean_t force_legacy_interrupts;

	use_msix = FALSE;
	force_legacy_interrupts = FALSE;

	if (!g_force_legacy_interrupts && pci_msix_count(ioat->device) >= 1) {
		num_vectors = 1;
		pci_alloc_msix(ioat->device, &num_vectors);
		if (num_vectors == 1)
			use_msix = TRUE;
	}

	if (use_msix) {
		ioat->rid = 1;
		ioat->res = bus_alloc_resource_any(ioat->device, SYS_RES_IRQ,
		    &ioat->rid, RF_ACTIVE);
	} else {
		ioat->rid = 0;
		ioat->res = bus_alloc_resource_any(ioat->device, SYS_RES_IRQ,
		    &ioat->rid, RF_SHAREABLE | RF_ACTIVE);
	}
	if (ioat->res == NULL) {
		ioat_log_message(0, "bus_alloc_resource failed\n");
		return (ENOMEM);
	}

	ioat->tag = NULL;
	error = bus_setup_intr(ioat->device, ioat->res, INTR_MPSAFE |
	    INTR_TYPE_MISC, NULL, ioat_interrupt_handler, ioat, &ioat->tag);
	if (error != 0) {
		ioat_log_message(0, "bus_setup_intr failed\n");
		return (error);
	}

	ioat_write_intrctrl(ioat, IOAT_INTRCTRL_MASTER_INT_EN);
	return (0);
}

static boolean_t
ioat_model_resets_msix(struct ioat_softc *ioat)
{
	u_int32_t pciid;

	pciid = pci_get_devid(ioat->device);
	switch (pciid) {
		/* BWD: */
	case 0x0c508086:
	case 0x0c518086:
	case 0x0c528086:
	case 0x0c538086:
		/* BDXDE: */
	case 0x6f508086:
	case 0x6f518086:
	case 0x6f528086:
	case 0x6f538086:
		return (TRUE);
	}

	return (FALSE);
}

static void
ioat_interrupt_handler(void *arg)
{
	struct ioat_softc *ioat = arg;

	ioat_process_events(ioat);
}

static void
ioat_process_events(struct ioat_softc *ioat)
{
	struct ioat_descriptor *desc;
	struct bus_dmadesc *dmadesc;
	uint64_t comp_update, status;
	uint32_t completed;

	mtx_lock(&ioat->cleanup_lock);

	completed = 0;
	comp_update = *ioat->comp_update;
	status = comp_update & IOAT_CHANSTS_COMPLETED_DESCRIPTOR_MASK;

	ioat_log_message(3, "%s\n", __func__);

	if (status == ioat->last_seen) {
	 	mtx_unlock(&ioat->cleanup_lock);
		return;
	}

	while (1) {
		desc = ioat_get_ring_entry(ioat, ioat->tail);
		dmadesc = &desc->bus_dmadesc;
		ioat_log_message(3, "completing desc %d\n", ioat->tail);

		if (dmadesc->callback_fn)
			(*dmadesc->callback_fn)(dmadesc->callback_arg);

		ioat->tail++;
		if (desc->hw_desc_bus_addr == status)
			break;
	}

	ioat->last_seen = desc->hw_desc_bus_addr;

	if (ioat->head == ioat->tail) {
		ioat->is_completion_pending = FALSE;
		callout_reset(&ioat->timer, 5 * hz, ioat_timer_callback, ioat);
	}

	ioat_write_chanctrl(ioat, IOAT_CHANCTRL_RUN);
	mtx_unlock(&ioat->cleanup_lock);
}

/*
 * User API functions
 */
bus_dmaengine_t
ioat_get_dmaengine(uint32_t index)
{

	if (index < ioat_channel_index)
		return (&ioat_channel[index]->dmaengine);
	return (NULL);
}

void
ioat_acquire(bus_dmaengine_t dmaengine)
{
	struct ioat_softc *ioat;

	ioat = to_ioat_softc(dmaengine);
	mtx_lock(&ioat->submit_lock);
	ioat_log_message(3, "%s\n", __func__);
}

void
ioat_release(bus_dmaengine_t dmaengine)
{
	struct ioat_softc *ioat;

	ioat_log_message(3, "%s\n", __func__);
	ioat = to_ioat_softc(dmaengine);
	ioat_write_2(ioat, IOAT_DMACOUNT_OFFSET, (uint16_t)ioat->head);
	mtx_unlock(&ioat->submit_lock);
}

struct bus_dmadesc *
ioat_null(bus_dmaengine_t dmaengine, bus_dmaengine_callback_t callback_fn,
    void *callback_arg, uint32_t flags)
{
	struct ioat_softc *ioat;
	struct ioat_descriptor *desc;
	struct ioat_dma_hw_descriptor *hw_desc;

	KASSERT((flags & ~DMA_ALL_FLAGS) == 0, ("Unrecognized flag(s): %#x",
		flags & ~DMA_ALL_FLAGS));

	ioat = to_ioat_softc(dmaengine);

	if (ioat_reserve_space_and_lock(ioat, 1) != 0)
		return (NULL);

	ioat_log_message(3, "%s\n", __func__);

	desc = ioat_get_ring_entry(ioat, ioat->head);
	hw_desc = desc->u.dma;

	hw_desc->u.control_raw = 0;
	hw_desc->u.control.null = 1;
	hw_desc->u.control.completion_update = 1;

	if ((flags & DMA_INT_EN) != 0)
		hw_desc->u.control.int_enable = 1;

	hw_desc->size = 8;
	hw_desc->src_addr = 0;
	hw_desc->dest_addr = 0;

	desc->bus_dmadesc.callback_fn = callback_fn;
	desc->bus_dmadesc.callback_arg = callback_arg;

	ioat_submit_single(ioat);
	return (&desc->bus_dmadesc);
}

struct bus_dmadesc *
ioat_copy(bus_dmaengine_t dmaengine, bus_addr_t dst,
    bus_addr_t src, bus_size_t len, bus_dmaengine_callback_t callback_fn,
    void *callback_arg, uint32_t flags)
{
	struct ioat_descriptor *desc;
	struct ioat_dma_hw_descriptor *hw_desc;
	struct ioat_softc *ioat;

	KASSERT((flags & ~DMA_ALL_FLAGS) == 0, ("Unrecognized flag(s): %#x",
		flags & ~DMA_ALL_FLAGS));

	ioat = to_ioat_softc(dmaengine);

	if (len > ioat->max_xfer_size) {
		ioat_log_message(0, "%s: max_xfer_size = %d, requested = %d\n",
		    __func__, ioat->max_xfer_size, (int)len);
		return (NULL);
	}

	if (ioat_reserve_space_and_lock(ioat, 1) != 0)
		return (NULL);

	ioat_log_message(3, "%s\n", __func__);

	desc = ioat_get_ring_entry(ioat, ioat->head);
	hw_desc = desc->u.dma;

	hw_desc->u.control_raw = 0;
	hw_desc->u.control.completion_update = 1;

	if ((flags & DMA_INT_EN) != 0)
		hw_desc->u.control.int_enable = 1;

	hw_desc->size = len;
	hw_desc->src_addr = src;
	hw_desc->dest_addr = dst;

	if (g_ioat_debug_level >= 3)
		dump_descriptor(hw_desc);

	desc->bus_dmadesc.callback_fn = callback_fn;
	desc->bus_dmadesc.callback_arg = callback_arg;

	ioat_submit_single(ioat);
	return (&desc->bus_dmadesc);
}

/*
 * Ring Management
 */
static inline uint32_t
ioat_get_active(struct ioat_softc *ioat)
{

	return ((ioat->head - ioat->tail) & ((1 << ioat->ring_size_order) - 1));
}

static inline uint32_t
ioat_get_ring_space(struct ioat_softc *ioat)
{

	return ((1 << ioat->ring_size_order) - ioat_get_active(ioat) - 1);
}

static struct ioat_descriptor *
ioat_alloc_ring_entry(struct ioat_softc *ioat)
{
	struct ioat_dma_hw_descriptor *hw_desc;
	struct ioat_descriptor *desc;

	desc = malloc(sizeof(struct ioat_descriptor), M_IOAT, M_NOWAIT);
	if (desc == NULL)
		return (NULL);

	bus_dmamem_alloc(ioat->hw_desc_tag, (void **)&hw_desc, BUS_DMA_ZERO,
	    &ioat->hw_desc_map);
	if (hw_desc == NULL) {
		free(desc, M_IOAT);
		return (NULL);
	}

	bus_dmamap_load(ioat->hw_desc_tag, ioat->hw_desc_map, hw_desc,
	    sizeof(*hw_desc), ioat_dmamap_cb, &desc->hw_desc_bus_addr, 0);

	desc->u.dma = hw_desc;
	return (desc);
}

static void
ioat_free_ring_entry(struct ioat_softc *ioat, struct ioat_descriptor *desc)
{

	if (desc == NULL)
		return;

	if (desc->u.dma)
		bus_dmamem_free(ioat->hw_desc_tag, desc->u.dma,
		    ioat->hw_desc_map);
	free(desc, M_IOAT);
}

static int
ioat_reserve_space_and_lock(struct ioat_softc *ioat, int num_descs)
{
	boolean_t retry;

	while (1) {
		if (ioat_get_ring_space(ioat) >= num_descs)
			return (0);

		mtx_lock(&ioat->cleanup_lock);
		retry = resize_ring(ioat, ioat->ring_size_order + 1);
		mtx_unlock(&ioat->cleanup_lock);

		if (!retry)
			return (ENOMEM);
	}
}

static struct ioat_descriptor *
ioat_get_ring_entry(struct ioat_softc *ioat, uint32_t index)
{

	return (ioat->ring[index % (1 << ioat->ring_size_order)]);
}

static boolean_t
resize_ring(struct ioat_softc *ioat, int order)
{
	struct ioat_descriptor **ring;
	struct ioat_descriptor *next;
	struct ioat_dma_hw_descriptor *hw;
	struct ioat_descriptor *ent;
	uint32_t current_size, active, new_size, i, new_idx, current_idx;
	uint32_t new_idx2;

	current_size = 1 << ioat->ring_size_order;
	active = (ioat->head - ioat->tail) & (current_size - 1);
	new_size = 1 << order;

	if (order > IOAT_MAX_ORDER)
		return (FALSE);

	/*
	 * when shrinking, verify that we can hold the current active
	 * set in the new ring
	 */
	if (active >= new_size)
		return (FALSE);

	/* allocate the array to hold the software ring */
	ring = malloc(new_size * sizeof(*ring), M_IOAT, M_ZERO | M_NOWAIT);
	if (ring == NULL)
		return (FALSE);

	ioat_log_message(2, "ring resize: new: %d old: %d\n",
	    new_size, current_size);

	/* allocate/trim descriptors as needed */
	if (new_size > current_size) {
		/* copy current descriptors to the new ring */
		for (i = 0; i < current_size; i++) {
			current_idx = (ioat->tail + i) & (current_size - 1);
			new_idx = (ioat->tail + i) & (new_size - 1);

			ring[new_idx] = ioat->ring[current_idx];
			ring[new_idx]->id = new_idx;
		}

		/* add new descriptors to the ring */
		for (i = current_size; i < new_size; i++) {
			new_idx = (ioat->tail + i) & (new_size - 1);

			ring[new_idx] = ioat_alloc_ring_entry(ioat);
			if (ring[new_idx] == NULL) {
				while (i--) {
					new_idx2 = (ioat->tail + i) &
					    (new_size - 1);

					ioat_free_ring_entry(ioat,
					    ring[new_idx2]);
				}
				free(ring, M_IOAT);
				return (FALSE);
			}
			ring[new_idx]->id = new_idx;
		}

		for (i = current_size - 1; i < new_size; i++) {
			new_idx = (ioat->tail + i) & (new_size - 1);
			next = ring[(new_idx + 1) & (new_size - 1)];
			hw = ring[new_idx]->u.dma;

			hw->next = next->hw_desc_bus_addr;
		}
	} else {
		/*
		 * copy current descriptors to the new ring, dropping the
		 * removed descriptors
		 */
		for (i = 0; i < new_size; i++) {
			current_idx = (ioat->tail + i) & (current_size - 1);
			new_idx = (ioat->tail + i) & (new_size - 1);

			ring[new_idx] = ioat->ring[current_idx];
			ring[new_idx]->id = new_idx;
		}

		/* free deleted descriptors */
		for (i = new_size; i < current_size; i++) {
			ent = ioat_get_ring_entry(ioat, ioat->tail + i);
			ioat_free_ring_entry(ioat, ent);
		}

		/* fix up hardware ring */
		hw = ring[(ioat->tail + new_size - 1) & (new_size - 1)]->u.dma;
		next = ring[(ioat->tail + new_size) & (new_size - 1)];
		hw->next = next->hw_desc_bus_addr;
	}

	free(ioat->ring, M_IOAT);
	ioat->ring = ring;
	ioat->ring_size_order = order;

	return (TRUE);
}

static void
ioat_timer_callback(void *arg)
{
	struct ioat_descriptor *desc;
	struct ioat_softc *ioat;
	uint64_t status;
	uint32_t chanerr;

	ioat = arg;
	ioat_log_message(2, "%s\n", __func__);

	if (ioat->is_completion_pending) {
		status = ioat_get_chansts(ioat);

		/*
		 * When halted due to errors, check for channel programming
		 * errors before advancing the completion state.
		 */
		if (is_ioat_halted(status)) {
			chanerr = ioat_read_4(ioat, IOAT_CHANERR_OFFSET);
			ioat_log_message(0, "Channel halted (%x)\n", chanerr);

			desc = ioat_get_ring_entry(ioat, ioat->tail + 0);
			dump_descriptor(desc->u.raw);

			desc = ioat_get_ring_entry(ioat, ioat->tail + 1);
			dump_descriptor(desc->u.raw);
		}
		ioat_process_events(ioat);
	} else {
		mtx_lock(&ioat->submit_lock);
		mtx_lock(&ioat->cleanup_lock);

		if (ioat_get_active(ioat) == 0 &&
		    ioat->ring_size_order > IOAT_MIN_ORDER)
			resize_ring(ioat, ioat->ring_size_order - 1);

		mtx_unlock(&ioat->cleanup_lock);
		mtx_unlock(&ioat->submit_lock);

		if (ioat->ring_size_order > IOAT_MIN_ORDER)
			callout_reset(&ioat->timer, 5 * hz,
			    ioat_timer_callback, ioat);
	}
}

/*
 * Support Functions
 */
static void
ioat_submit_single(struct ioat_softc *ioat)
{

	atomic_add_rel_int(&ioat->head, 1);

	if (!ioat->is_completion_pending) {
		ioat->is_completion_pending = TRUE;
		callout_reset(&ioat->timer, 10 * hz, ioat_timer_callback,
		    ioat);
	}
}

static int
ioat_reset_hw(struct ioat_softc *ioat)
{
	uint64_t status;
	uint32_t chanerr;
	int timeout;

	status = ioat_get_chansts(ioat);
	if (is_ioat_active(status) || is_ioat_idle(status))
		ioat_suspend(ioat);

	/* Wait at most 20 ms */
	for (timeout = 0; (is_ioat_active(status) || is_ioat_idle(status)) &&
	    timeout < 20; timeout++) {
		DELAY(1000);
		status = ioat_get_chansts(ioat);
	}
	if (timeout == 20)
		return (ETIMEDOUT);

	chanerr = ioat_read_4(ioat, IOAT_CHANERR_OFFSET);
	ioat_write_4(ioat, IOAT_CHANERR_OFFSET, chanerr);

	/*
	 * IOAT v3 workaround - CHANERRMSK_INT with 3E07h to masks out errors
	 *  that can cause stability issues for IOAT v3.
	 */
	pci_write_config(ioat->device, IOAT_CFG_CHANERRMASK_INT_OFFSET, 0x3e07,
	    4);
	chanerr = pci_read_config(ioat->device, IOAT_CFG_CHANERR_INT_OFFSET, 4);
	pci_write_config(ioat->device, IOAT_CFG_CHANERR_INT_OFFSET, chanerr, 4);

	/*
	 * BDXDE and BWD models reset MSI-X registers on device reset.
	 * Save/restore their contents manually.
	 */
	if (ioat_model_resets_msix(ioat))
		pci_save_state(ioat->device);

	ioat_reset(ioat);

	/* Wait at most 20 ms */
	for (timeout = 0; ioat_reset_pending(ioat) && timeout < 20; timeout++)
		DELAY(1000);
	if (timeout == 20)
		return (ETIMEDOUT);

	if (ioat_model_resets_msix(ioat))
		pci_restore_state(ioat->device);

	return (0);
}

static void
dump_descriptor(void *hw_desc)
{
	int i, j;

	for (i = 0; i < 2; i++) {
		for (j = 0; j < 8; j++)
			printf("%08x ", ((uint32_t *)hw_desc)[i * 8 + j]);
		printf("\n");
	}
}

static void
ioat_setup_sysctl(device_t device)
{
	struct sysctl_ctx_list *sysctl_ctx;
	struct sysctl_oid *sysctl_tree;
	struct ioat_softc *ioat;

	ioat = DEVICE2SOFTC(device);
	sysctl_ctx = device_get_sysctl_ctx(device);
	sysctl_tree = device_get_sysctl_tree(device);

	SYSCTL_ADD_UINT(sysctl_ctx, SYSCTL_CHILDREN(sysctl_tree), OID_AUTO,
	    "ring_size_order", CTLFLAG_RD, &ioat->ring_size_order,
	    0, "HW descriptor ring size order");
	SYSCTL_ADD_UINT(sysctl_ctx, SYSCTL_CHILDREN(sysctl_tree), OID_AUTO,
	    "head", CTLFLAG_RD, &ioat->head,
	    0, "HW descriptor head pointer index");
	SYSCTL_ADD_UINT(sysctl_ctx, SYSCTL_CHILDREN(sysctl_tree), OID_AUTO,
	    "tail", CTLFLAG_RD, &ioat->tail,
	    0, "HW descriptor tail pointer index");
}

void
ioat_log_message(int verbosity, char *fmt, ...)
{
	va_list argp;
	char buffer[512];
	struct timeval tv;

	if (verbosity > g_ioat_debug_level)
		return;

	va_start(argp, fmt);
	vsnprintf(buffer, sizeof(buffer) - 1, fmt, argp);
	va_end(argp);
	microuptime(&tv);

	printf("[%d:%06d] ioat: %s", (int)tv.tv_sec, (int)tv.tv_usec, buffer);
}
