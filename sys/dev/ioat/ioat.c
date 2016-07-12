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

#include "opt_ddb.h"

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
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <sys/time.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/stdarg.h>

#ifdef DDB
#include <ddb/ddb.h>
#endif

#include "ioat.h"
#include "ioat_hw.h"
#include "ioat_internal.h"

#ifndef	BUS_SPACE_MAXADDR_40BIT
#define	BUS_SPACE_MAXADDR_40BIT	0xFFFFFFFFFFULL
#endif
#define	IOAT_REFLK	(&ioat->submit_lock)
#define	IOAT_SHRINK_PERIOD	(10 * hz)

static int ioat_probe(device_t device);
static int ioat_attach(device_t device);
static int ioat_detach(device_t device);
static int ioat_setup_intr(struct ioat_softc *ioat);
static int ioat_teardown_intr(struct ioat_softc *ioat);
static int ioat3_attach(device_t device);
static int ioat_start_channel(struct ioat_softc *ioat);
static int ioat_map_pci_bar(struct ioat_softc *ioat);
static void ioat_dmamap_cb(void *arg, bus_dma_segment_t *segs, int nseg,
    int error);
static void ioat_interrupt_handler(void *arg);
static boolean_t ioat_model_resets_msix(struct ioat_softc *ioat);
static int chanerr_to_errno(uint32_t);
static void ioat_process_events(struct ioat_softc *ioat);
static inline uint32_t ioat_get_active(struct ioat_softc *ioat);
static inline uint32_t ioat_get_ring_space(struct ioat_softc *ioat);
static void ioat_free_ring(struct ioat_softc *, uint32_t size,
    struct ioat_descriptor **);
static void ioat_free_ring_entry(struct ioat_softc *ioat,
    struct ioat_descriptor *desc);
static struct ioat_descriptor *ioat_alloc_ring_entry(struct ioat_softc *,
    int mflags);
static int ioat_reserve_space(struct ioat_softc *, uint32_t, int mflags);
static struct ioat_descriptor *ioat_get_ring_entry(struct ioat_softc *ioat,
    uint32_t index);
static struct ioat_descriptor **ioat_prealloc_ring(struct ioat_softc *,
    uint32_t size, boolean_t need_dscr, int mflags);
static int ring_grow(struct ioat_softc *, uint32_t oldorder,
    struct ioat_descriptor **);
static int ring_shrink(struct ioat_softc *, uint32_t oldorder,
    struct ioat_descriptor **);
static void ioat_halted_debug(struct ioat_softc *, uint32_t);
static void ioat_poll_timer_callback(void *arg);
static void ioat_shrink_timer_callback(void *arg);
static void dump_descriptor(void *hw_desc);
static void ioat_submit_single(struct ioat_softc *ioat);
static void ioat_comp_update_map(void *arg, bus_dma_segment_t *seg, int nseg,
    int error);
static int ioat_reset_hw(struct ioat_softc *ioat);
static void ioat_reset_hw_task(void *, int);
static void ioat_setup_sysctl(device_t device);
static int sysctl_handle_reset(SYSCTL_HANDLER_ARGS);
static inline struct ioat_softc *ioat_get(struct ioat_softc *,
    enum ioat_ref_kind);
static inline void ioat_put(struct ioat_softc *, enum ioat_ref_kind);
static inline void _ioat_putn(struct ioat_softc *, uint32_t,
    enum ioat_ref_kind, boolean_t);
static inline void ioat_putn(struct ioat_softc *, uint32_t,
    enum ioat_ref_kind);
static inline void ioat_putn_locked(struct ioat_softc *, uint32_t,
    enum ioat_ref_kind);
static void ioat_drain_locked(struct ioat_softc *);

#define	ioat_log_message(v, ...) do {					\
	if ((v) <= g_ioat_debug_level) {				\
		device_printf(ioat->device, __VA_ARGS__);		\
	}								\
} while (0)

MALLOC_DEFINE(M_IOAT, "ioat", "ioat driver memory allocations");
SYSCTL_NODE(_hw, OID_AUTO, ioat, CTLFLAG_RD, 0, "ioat node");

static int g_force_legacy_interrupts;
SYSCTL_INT(_hw_ioat, OID_AUTO, force_legacy_interrupts, CTLFLAG_RDTUN,
    &g_force_legacy_interrupts, 0, "Set to non-zero to force MSI-X disabled");

int g_ioat_debug_level = 0;
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
	DEVMETHOD_END
};

static driver_t ioat_pci_driver = {
	"ioat",
	ioat_pci_methods,
	sizeof(struct ioat_softc),
};

static devclass_t ioat_devclass;
DRIVER_MODULE(ioat, pci, ioat_pci_driver, ioat_devclass, 0, 0);
MODULE_VERSION(ioat, 1);

/*
 * Private data structures
 */
static struct ioat_softc *ioat_channel[IOAT_MAX_CHANNELS];
static unsigned ioat_channel_index = 0;
SYSCTL_UINT(_hw_ioat, OID_AUTO, channels, CTLFLAG_RD, &ioat_channel_index, 0,
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

	{ 0x6f208086, "BDX IOAT Ch0" },
	{ 0x6f218086, "BDX IOAT Ch1" },
	{ 0x6f228086, "BDX IOAT Ch2" },
	{ 0x6f238086, "BDX IOAT Ch3" },
	{ 0x6f248086, "BDX IOAT Ch4" },
	{ 0x6f258086, "BDX IOAT Ch5" },
	{ 0x6f268086, "BDX IOAT Ch6" },
	{ 0x6f278086, "BDX IOAT Ch7" },
	{ 0x6f2e8086, "BDX IOAT Ch0 (RAID)" },
	{ 0x6f2f8086, "BDX IOAT Ch1 (RAID)" },

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

	error = ioat3_attach(device);
	if (error != 0)
		goto err;

	error = pci_enable_busmaster(device);
	if (error != 0)
		goto err;

	error = ioat_setup_intr(ioat);
	if (error != 0)
		goto err;

	error = ioat_reset_hw(ioat);
	if (error != 0)
		goto err;

	ioat_process_events(ioat);
	ioat_setup_sysctl(device);

	ioat->chan_idx = ioat_channel_index;
	ioat_channel[ioat_channel_index++] = ioat;
	ioat_test_attach();

err:
	if (error != 0)
		ioat_detach(device);
	return (error);
}

static int
ioat_detach(device_t device)
{
	struct ioat_softc *ioat;

	ioat = DEVICE2SOFTC(device);

	ioat_test_detach();
	taskqueue_drain(taskqueue_thread, &ioat->reset_task);

	mtx_lock(IOAT_REFLK);
	ioat->quiescing = TRUE;
	ioat->destroying = TRUE;
	wakeup(&ioat->quiescing);
	wakeup(&ioat->resetting);

	ioat_channel[ioat->chan_idx] = NULL;

	ioat_drain_locked(ioat);
	mtx_unlock(IOAT_REFLK);

	ioat_teardown_intr(ioat);
	callout_drain(&ioat->poll_timer);
	callout_drain(&ioat->shrink_timer);

	pci_disable_busmaster(device);

	if (ioat->pci_resource != NULL)
		bus_release_resource(device, SYS_RES_MEMORY,
		    ioat->pci_resource_id, ioat->pci_resource);

	if (ioat->ring != NULL)
		ioat_free_ring(ioat, 1 << ioat->ring_size_order, ioat->ring);

	if (ioat->comp_update != NULL) {
		bus_dmamap_unload(ioat->comp_update_tag, ioat->comp_update_map);
		bus_dmamem_free(ioat->comp_update_tag, ioat->comp_update,
		    ioat->comp_update_map);
		bus_dma_tag_destroy(ioat->comp_update_tag);
	}

	bus_dma_tag_destroy(ioat->hw_desc_tag);

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
ioat_start_channel(struct ioat_softc *ioat)
{
	struct ioat_dma_hw_descriptor *hw_desc;
	struct ioat_descriptor *desc;
	struct bus_dmadesc *dmadesc;
	uint64_t status;
	uint32_t chanerr;
	int i;

	ioat_acquire(&ioat->dmaengine);

	/* Submit 'NULL' operation manually to avoid quiescing flag */
	desc = ioat_get_ring_entry(ioat, ioat->head);
	dmadesc = &desc->bus_dmadesc;
	hw_desc = desc->u.dma;

	dmadesc->callback_fn = NULL;
	dmadesc->callback_arg = NULL;

	hw_desc->u.control_raw = 0;
	hw_desc->u.control_generic.op = IOAT_OP_COPY;
	hw_desc->u.control_generic.completion_update = 1;
	hw_desc->size = 8;
	hw_desc->src_addr = 0;
	hw_desc->dest_addr = 0;
	hw_desc->u.control.null = 1;

	ioat_submit_single(ioat);
	ioat_release(&ioat->dmaengine);

	for (i = 0; i < 100; i++) {
		DELAY(1);
		status = ioat_get_chansts(ioat);
		if (is_ioat_idle(status))
			return (0);
	}

	chanerr = ioat_read_4(ioat, IOAT_CHANERR_OFFSET);
	ioat_log_message(0, "could not start channel: "
	    "status = %#jx error = %b\n", (uintmax_t)status, (int)chanerr,
	    IOAT_CHANERR_STR);
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
	int i, num_descriptors;
	int error;
	uint8_t xfercap;

	error = 0;
	ioat = DEVICE2SOFTC(device);
	ioat->capabilities = ioat_read_dmacapability(ioat);

	ioat_log_message(0, "Capabilities: %b\n", (int)ioat->capabilities,
	    IOAT_DMACAP_STR);

	xfercap = ioat_read_xfercap(ioat);
	ioat->max_xfer_size = 1 << xfercap;

	ioat->intrdelay_supported = (ioat_read_2(ioat, IOAT_INTRDELAY_OFFSET) &
	    IOAT_INTRDELAY_SUPPORTED) != 0;
	if (ioat->intrdelay_supported)
		ioat->intrdelay_max = IOAT_INTRDELAY_US_MASK;

	/* TODO: need to check DCA here if we ever do XOR/PQ */

	mtx_init(&ioat->submit_lock, "ioat_submit", NULL, MTX_DEF);
	mtx_init(&ioat->cleanup_lock, "ioat_cleanup", NULL, MTX_DEF);
	callout_init(&ioat->poll_timer, 1);
	callout_init(&ioat->shrink_timer, 1);
	TASK_INIT(&ioat->reset_task, 0, ioat_reset_hw_task, ioat);

	/* Establish lock order for Witness */
	mtx_lock(&ioat->submit_lock);
	mtx_lock(&ioat->cleanup_lock);
	mtx_unlock(&ioat->cleanup_lock);
	mtx_unlock(&ioat->submit_lock);

	ioat->is_resize_pending = FALSE;
	ioat->is_completion_pending = FALSE;
	ioat->is_reset_pending = FALSE;
	ioat->is_channel_running = FALSE;

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
	    BUS_SPACE_MAXADDR_40BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    sizeof(struct ioat_dma_hw_descriptor), 1,
	    sizeof(struct ioat_dma_hw_descriptor), 0, NULL, NULL,
	    &ioat->hw_desc_tag);

	ioat->ring = malloc(num_descriptors * sizeof(*ring), M_IOAT,
	    M_ZERO | M_WAITOK);

	ring = ioat->ring;
	for (i = 0; i < num_descriptors; i++) {
		ring[i] = ioat_alloc_ring_entry(ioat, M_WAITOK);
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

	ioat->head = ioat->hw_head = 0;
	ioat->tail = 0;
	ioat->last_seen = 0;
	*ioat->comp_update = 0;
	return (0);
}

static int
ioat_map_pci_bar(struct ioat_softc *ioat)
{

	ioat->pci_resource_id = PCIR_BAR(0);
	ioat->pci_resource = bus_alloc_resource_any(ioat->device,
	    SYS_RES_MEMORY, &ioat->pci_resource_id, RF_ACTIVE);

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

	KASSERT(error == 0, ("%s: error:%d", __func__, error));
	ioat->comp_update_bus_addr = seg[0].ds_addr;
}

static void
ioat_dmamap_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	bus_addr_t *baddr;

	KASSERT(error == 0, ("%s: error:%d", __func__, error));
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

	ioat->stats.interrupts++;
	ioat_process_events(ioat);
}

static int
chanerr_to_errno(uint32_t chanerr)
{

	if (chanerr == 0)
		return (0);
	if ((chanerr & (IOAT_CHANERR_XSADDERR | IOAT_CHANERR_XDADDERR)) != 0)
		return (EFAULT);
	if ((chanerr & (IOAT_CHANERR_RDERR | IOAT_CHANERR_WDERR)) != 0)
		return (EIO);
	/* This one is probably our fault: */
	if ((chanerr & IOAT_CHANERR_NDADDERR) != 0)
		return (EIO);
	return (EIO);
}

static void
ioat_process_events(struct ioat_softc *ioat)
{
	struct ioat_descriptor *desc;
	struct bus_dmadesc *dmadesc;
	uint64_t comp_update, status;
	uint32_t completed, chanerr;
	boolean_t pending;
	int error;

	CTR0(KTR_IOAT, __func__);

	mtx_lock(&ioat->cleanup_lock);

	/*
	 * Don't run while the hardware is being reset.  Reset is responsible
	 * for blocking new work and draining & completing existing work, so
	 * there is nothing to do until new work is queued after reset anyway.
	 */
	if (ioat->resetting_cleanup) {
		mtx_unlock(&ioat->cleanup_lock);
		return;
	}

	completed = 0;
	comp_update = *ioat->comp_update;
	status = comp_update & IOAT_CHANSTS_COMPLETED_DESCRIPTOR_MASK;

	if (status == ioat->last_seen) {
		/*
		 * If we landed in process_events and nothing has been
		 * completed, check for a timeout due to channel halt.
		 */
		comp_update = ioat_get_chansts(ioat);
		goto out;
	}

	while (1) {
		desc = ioat_get_ring_entry(ioat, ioat->tail);
		dmadesc = &desc->bus_dmadesc;
		CTR1(KTR_IOAT, "completing desc %d", ioat->tail);

		if (dmadesc->callback_fn != NULL)
			dmadesc->callback_fn(dmadesc->callback_arg, 0);

		completed++;
		ioat->tail++;
		if (desc->hw_desc_bus_addr == status)
			break;
	}

	ioat->last_seen = desc->hw_desc_bus_addr;
	ioat->stats.descriptors_processed += completed;

out:
	ioat_write_chanctrl(ioat, IOAT_CHANCTRL_RUN);

	/* Perform a racy check first; only take the locks if it passes. */
	pending = (ioat_get_active(ioat) != 0);
	if (!pending && ioat->is_completion_pending) {
		mtx_unlock(&ioat->cleanup_lock);
		mtx_lock(&ioat->submit_lock);
		mtx_lock(&ioat->cleanup_lock);

		pending = (ioat_get_active(ioat) != 0);
		if (!pending && ioat->is_completion_pending) {
			ioat->is_completion_pending = FALSE;
			callout_reset(&ioat->shrink_timer, IOAT_SHRINK_PERIOD,
			    ioat_shrink_timer_callback, ioat);
			callout_stop(&ioat->poll_timer);
		}
		mtx_unlock(&ioat->submit_lock);
	}
	mtx_unlock(&ioat->cleanup_lock);

	if (pending)
		callout_reset(&ioat->poll_timer, 1, ioat_poll_timer_callback,
		    ioat);

	if (completed != 0) {
		ioat_putn(ioat, completed, IOAT_ACTIVE_DESCR_REF);
		wakeup(&ioat->tail);
	}

	if (!is_ioat_halted(comp_update) && !is_ioat_suspended(comp_update))
		return;

	ioat->stats.channel_halts++;

	/*
	 * Fatal programming error on this DMA channel.  Flush any outstanding
	 * work with error status and restart the engine.
	 */
	ioat_log_message(0, "Channel halted due to fatal programming error\n");
	mtx_lock(&ioat->submit_lock);
	mtx_lock(&ioat->cleanup_lock);
	ioat->quiescing = TRUE;

	chanerr = ioat_read_4(ioat, IOAT_CHANERR_OFFSET);
	ioat_halted_debug(ioat, chanerr);
	ioat->stats.last_halt_chanerr = chanerr;

	while (ioat_get_active(ioat) > 0) {
		desc = ioat_get_ring_entry(ioat, ioat->tail);
		dmadesc = &desc->bus_dmadesc;
		CTR1(KTR_IOAT, "completing err desc %d", ioat->tail);

		if (dmadesc->callback_fn != NULL)
			dmadesc->callback_fn(dmadesc->callback_arg,
			    chanerr_to_errno(chanerr));

		ioat_putn_locked(ioat, 1, IOAT_ACTIVE_DESCR_REF);
		ioat->tail++;
		ioat->stats.descriptors_processed++;
		ioat->stats.descriptors_error++;
	}

	/* Clear error status */
	ioat_write_4(ioat, IOAT_CHANERR_OFFSET, chanerr);

	mtx_unlock(&ioat->cleanup_lock);
	mtx_unlock(&ioat->submit_lock);

	ioat_log_message(0, "Resetting channel to recover from error\n");
	error = taskqueue_enqueue(taskqueue_thread, &ioat->reset_task);
	KASSERT(error == 0,
	    ("%s: taskqueue_enqueue failed: %d", __func__, error));
}

static void
ioat_reset_hw_task(void *ctx, int pending __unused)
{
	struct ioat_softc *ioat;
	int error;

	ioat = ctx;
	ioat_log_message(1, "%s: Resetting channel\n", __func__);

	error = ioat_reset_hw(ioat);
	KASSERT(error == 0, ("%s: reset failed: %d", __func__, error));
	(void)error;
}

/*
 * User API functions
 */
unsigned
ioat_get_nchannels(void)
{

	return (ioat_channel_index);
}

bus_dmaengine_t
ioat_get_dmaengine(uint32_t index, int flags)
{
	struct ioat_softc *ioat;

	KASSERT((flags & ~(M_NOWAIT | M_WAITOK)) == 0,
	    ("invalid flags: 0x%08x", flags));
	KASSERT((flags & (M_NOWAIT | M_WAITOK)) != (M_NOWAIT | M_WAITOK),
	    ("invalid wait | nowait"));

	if (index >= ioat_channel_index)
		return (NULL);

	ioat = ioat_channel[index];
	if (ioat == NULL || ioat->destroying)
		return (NULL);

	if (ioat->quiescing) {
		if ((flags & M_NOWAIT) != 0)
			return (NULL);

		mtx_lock(IOAT_REFLK);
		while (ioat->quiescing && !ioat->destroying)
			msleep(&ioat->quiescing, IOAT_REFLK, 0, "getdma", 0);
		mtx_unlock(IOAT_REFLK);

		if (ioat->destroying)
			return (NULL);
	}

	/*
	 * There's a race here between the quiescing check and HW reset or
	 * module destroy.
	 */
	return (&ioat_get(ioat, IOAT_DMAENGINE_REF)->dmaengine);
}

void
ioat_put_dmaengine(bus_dmaengine_t dmaengine)
{
	struct ioat_softc *ioat;

	ioat = to_ioat_softc(dmaengine);
	ioat_put(ioat, IOAT_DMAENGINE_REF);
}

int
ioat_get_hwversion(bus_dmaengine_t dmaengine)
{
	struct ioat_softc *ioat;

	ioat = to_ioat_softc(dmaengine);
	return (ioat->version);
}

size_t
ioat_get_max_io_size(bus_dmaengine_t dmaengine)
{
	struct ioat_softc *ioat;

	ioat = to_ioat_softc(dmaengine);
	return (ioat->max_xfer_size);
}

int
ioat_set_interrupt_coalesce(bus_dmaengine_t dmaengine, uint16_t delay)
{
	struct ioat_softc *ioat;

	ioat = to_ioat_softc(dmaengine);
	if (!ioat->intrdelay_supported)
		return (ENODEV);
	if (delay > ioat->intrdelay_max)
		return (ERANGE);

	ioat_write_2(ioat, IOAT_INTRDELAY_OFFSET, delay);
	ioat->cached_intrdelay =
	    ioat_read_2(ioat, IOAT_INTRDELAY_OFFSET) & IOAT_INTRDELAY_US_MASK;
	return (0);
}

uint16_t
ioat_get_max_coalesce_period(bus_dmaengine_t dmaengine)
{
	struct ioat_softc *ioat;

	ioat = to_ioat_softc(dmaengine);
	return (ioat->intrdelay_max);
}

void
ioat_acquire(bus_dmaengine_t dmaengine)
{
	struct ioat_softc *ioat;

	ioat = to_ioat_softc(dmaengine);
	mtx_lock(&ioat->submit_lock);
	CTR0(KTR_IOAT, __func__);
}

int
ioat_acquire_reserve(bus_dmaengine_t dmaengine, unsigned n, int mflags)
{
	struct ioat_softc *ioat;
	int error;

	ioat = to_ioat_softc(dmaengine);
	ioat_acquire(dmaengine);

	error = ioat_reserve_space(ioat, n, mflags);
	if (error != 0)
		ioat_release(dmaengine);
	return (error);
}

void
ioat_release(bus_dmaengine_t dmaengine)
{
	struct ioat_softc *ioat;

	ioat = to_ioat_softc(dmaengine);
	CTR0(KTR_IOAT, __func__);
	ioat_write_2(ioat, IOAT_DMACOUNT_OFFSET, (uint16_t)ioat->hw_head);
	mtx_unlock(&ioat->submit_lock);
}

static struct ioat_descriptor *
ioat_op_generic(struct ioat_softc *ioat, uint8_t op,
    uint32_t size, uint64_t src, uint64_t dst,
    bus_dmaengine_callback_t callback_fn, void *callback_arg,
    uint32_t flags)
{
	struct ioat_generic_hw_descriptor *hw_desc;
	struct ioat_descriptor *desc;
	int mflags;

	mtx_assert(&ioat->submit_lock, MA_OWNED);

	KASSERT((flags & ~_DMA_GENERIC_FLAGS) == 0,
	    ("Unrecognized flag(s): %#x", flags & ~_DMA_GENERIC_FLAGS));
	if ((flags & DMA_NO_WAIT) != 0)
		mflags = M_NOWAIT;
	else
		mflags = M_WAITOK;

	if (size > ioat->max_xfer_size) {
		ioat_log_message(0, "%s: max_xfer_size = %d, requested = %u\n",
		    __func__, ioat->max_xfer_size, (unsigned)size);
		return (NULL);
	}

	if (ioat_reserve_space(ioat, 1, mflags) != 0)
		return (NULL);

	desc = ioat_get_ring_entry(ioat, ioat->head);
	hw_desc = desc->u.generic;

	hw_desc->u.control_raw = 0;
	hw_desc->u.control_generic.op = op;
	hw_desc->u.control_generic.completion_update = 1;

	if ((flags & DMA_INT_EN) != 0)
		hw_desc->u.control_generic.int_enable = 1;
	if ((flags & DMA_FENCE) != 0)
		hw_desc->u.control_generic.fence = 1;

	hw_desc->size = size;
	hw_desc->src_addr = src;
	hw_desc->dest_addr = dst;

	desc->bus_dmadesc.callback_fn = callback_fn;
	desc->bus_dmadesc.callback_arg = callback_arg;
	return (desc);
}

struct bus_dmadesc *
ioat_null(bus_dmaengine_t dmaengine, bus_dmaengine_callback_t callback_fn,
    void *callback_arg, uint32_t flags)
{
	struct ioat_dma_hw_descriptor *hw_desc;
	struct ioat_descriptor *desc;
	struct ioat_softc *ioat;

	CTR0(KTR_IOAT, __func__);
	ioat = to_ioat_softc(dmaengine);

	desc = ioat_op_generic(ioat, IOAT_OP_COPY, 8, 0, 0, callback_fn,
	    callback_arg, flags);
	if (desc == NULL)
		return (NULL);

	hw_desc = desc->u.dma;
	hw_desc->u.control.null = 1;
	ioat_submit_single(ioat);
	return (&desc->bus_dmadesc);
}

struct bus_dmadesc *
ioat_copy(bus_dmaengine_t dmaengine, bus_addr_t dst,
    bus_addr_t src, bus_size_t len, bus_dmaengine_callback_t callback_fn,
    void *callback_arg, uint32_t flags)
{
	struct ioat_dma_hw_descriptor *hw_desc;
	struct ioat_descriptor *desc;
	struct ioat_softc *ioat;

	CTR0(KTR_IOAT, __func__);
	ioat = to_ioat_softc(dmaengine);

	if (((src | dst) & (0xffffull << 48)) != 0) {
		ioat_log_message(0, "%s: High 16 bits of src/dst invalid\n",
		    __func__);
		return (NULL);
	}

	desc = ioat_op_generic(ioat, IOAT_OP_COPY, len, src, dst, callback_fn,
	    callback_arg, flags);
	if (desc == NULL)
		return (NULL);

	hw_desc = desc->u.dma;
	if (g_ioat_debug_level >= 3)
		dump_descriptor(hw_desc);

	ioat_submit_single(ioat);
	return (&desc->bus_dmadesc);
}

struct bus_dmadesc *
ioat_copy_8k_aligned(bus_dmaengine_t dmaengine, bus_addr_t dst1,
    bus_addr_t dst2, bus_addr_t src1, bus_addr_t src2,
    bus_dmaengine_callback_t callback_fn, void *callback_arg, uint32_t flags)
{
	struct ioat_dma_hw_descriptor *hw_desc;
	struct ioat_descriptor *desc;
	struct ioat_softc *ioat;

	CTR0(KTR_IOAT, __func__);
	ioat = to_ioat_softc(dmaengine);

	if (((src1 | src2 | dst1 | dst2) & (0xffffull << 48)) != 0) {
		ioat_log_message(0, "%s: High 16 bits of src/dst invalid\n",
		    __func__);
		return (NULL);
	}
	if (((src1 | src2 | dst1 | dst2) & PAGE_MASK) != 0) {
		ioat_log_message(0, "%s: Addresses must be page-aligned\n",
		    __func__);
		return (NULL);
	}

	desc = ioat_op_generic(ioat, IOAT_OP_COPY, 2 * PAGE_SIZE, src1, dst1,
	    callback_fn, callback_arg, flags);
	if (desc == NULL)
		return (NULL);

	hw_desc = desc->u.dma;
	if (src2 != src1 + PAGE_SIZE) {
		hw_desc->u.control.src_page_break = 1;
		hw_desc->next_src_addr = src2;
	}
	if (dst2 != dst1 + PAGE_SIZE) {
		hw_desc->u.control.dest_page_break = 1;
		hw_desc->next_dest_addr = dst2;
	}

	if (g_ioat_debug_level >= 3)
		dump_descriptor(hw_desc);

	ioat_submit_single(ioat);
	return (&desc->bus_dmadesc);
}

struct bus_dmadesc *
ioat_copy_crc(bus_dmaengine_t dmaengine, bus_addr_t dst, bus_addr_t src,
    bus_size_t len, uint32_t *initialseed, bus_addr_t crcptr,
    bus_dmaengine_callback_t callback_fn, void *callback_arg, uint32_t flags)
{
	struct ioat_crc32_hw_descriptor *hw_desc;
	struct ioat_descriptor *desc;
	struct ioat_softc *ioat;
	uint32_t teststore;
	uint8_t op;

	CTR0(KTR_IOAT, __func__);
	ioat = to_ioat_softc(dmaengine);

	if ((ioat->capabilities & IOAT_DMACAP_MOVECRC) == 0) {
		ioat_log_message(0, "%s: Device lacks MOVECRC capability\n",
		    __func__);
		return (NULL);
	}
	if (((src | dst) & (0xffffffull << 40)) != 0) {
		ioat_log_message(0, "%s: High 24 bits of src/dst invalid\n",
		    __func__);
		return (NULL);
	}
	teststore = (flags & _DMA_CRC_TESTSTORE);
	if (teststore == _DMA_CRC_TESTSTORE) {
		ioat_log_message(0, "%s: TEST and STORE invalid\n", __func__);
		return (NULL);
	}
	if (teststore == 0 && (flags & DMA_CRC_INLINE) != 0) {
		ioat_log_message(0, "%s: INLINE invalid without TEST or STORE\n",
		    __func__);
		return (NULL);
	}

	switch (teststore) {
	case DMA_CRC_STORE:
		op = IOAT_OP_MOVECRC_STORE;
		break;
	case DMA_CRC_TEST:
		op = IOAT_OP_MOVECRC_TEST;
		break;
	default:
		KASSERT(teststore == 0, ("bogus"));
		op = IOAT_OP_MOVECRC;
		break;
	}

	if ((flags & DMA_CRC_INLINE) == 0 &&
	    (crcptr & (0xffffffull << 40)) != 0) {
		ioat_log_message(0,
		    "%s: High 24 bits of crcptr invalid\n", __func__);
		return (NULL);
	}

	desc = ioat_op_generic(ioat, op, len, src, dst, callback_fn,
	    callback_arg, flags & ~_DMA_CRC_FLAGS);
	if (desc == NULL)
		return (NULL);

	hw_desc = desc->u.crc32;

	if ((flags & DMA_CRC_INLINE) == 0)
		hw_desc->crc_address = crcptr;
	else
		hw_desc->u.control.crc_location = 1;

	if (initialseed != NULL) {
		hw_desc->u.control.use_seed = 1;
		hw_desc->seed = *initialseed;
	}

	if (g_ioat_debug_level >= 3)
		dump_descriptor(hw_desc);

	ioat_submit_single(ioat);
	return (&desc->bus_dmadesc);
}

struct bus_dmadesc *
ioat_crc(bus_dmaengine_t dmaengine, bus_addr_t src, bus_size_t len,
    uint32_t *initialseed, bus_addr_t crcptr,
    bus_dmaengine_callback_t callback_fn, void *callback_arg, uint32_t flags)
{
	struct ioat_crc32_hw_descriptor *hw_desc;
	struct ioat_descriptor *desc;
	struct ioat_softc *ioat;
	uint32_t teststore;
	uint8_t op;

	CTR0(KTR_IOAT, __func__);
	ioat = to_ioat_softc(dmaengine);

	if ((ioat->capabilities & IOAT_DMACAP_CRC) == 0) {
		ioat_log_message(0, "%s: Device lacks CRC capability\n",
		    __func__);
		return (NULL);
	}
	if ((src & (0xffffffull << 40)) != 0) {
		ioat_log_message(0, "%s: High 24 bits of src invalid\n",
		    __func__);
		return (NULL);
	}
	teststore = (flags & _DMA_CRC_TESTSTORE);
	if (teststore == _DMA_CRC_TESTSTORE) {
		ioat_log_message(0, "%s: TEST and STORE invalid\n", __func__);
		return (NULL);
	}
	if (teststore == 0 && (flags & DMA_CRC_INLINE) != 0) {
		ioat_log_message(0, "%s: INLINE invalid without TEST or STORE\n",
		    __func__);
		return (NULL);
	}

	switch (teststore) {
	case DMA_CRC_STORE:
		op = IOAT_OP_CRC_STORE;
		break;
	case DMA_CRC_TEST:
		op = IOAT_OP_CRC_TEST;
		break;
	default:
		KASSERT(teststore == 0, ("bogus"));
		op = IOAT_OP_CRC;
		break;
	}

	if ((flags & DMA_CRC_INLINE) == 0 &&
	    (crcptr & (0xffffffull << 40)) != 0) {
		ioat_log_message(0,
		    "%s: High 24 bits of crcptr invalid\n", __func__);
		return (NULL);
	}

	desc = ioat_op_generic(ioat, op, len, src, 0, callback_fn,
	    callback_arg, flags & ~_DMA_CRC_FLAGS);
	if (desc == NULL)
		return (NULL);

	hw_desc = desc->u.crc32;

	if ((flags & DMA_CRC_INLINE) == 0)
		hw_desc->crc_address = crcptr;
	else
		hw_desc->u.control.crc_location = 1;

	if (initialseed != NULL) {
		hw_desc->u.control.use_seed = 1;
		hw_desc->seed = *initialseed;
	}

	if (g_ioat_debug_level >= 3)
		dump_descriptor(hw_desc);

	ioat_submit_single(ioat);
	return (&desc->bus_dmadesc);
}

struct bus_dmadesc *
ioat_blockfill(bus_dmaengine_t dmaengine, bus_addr_t dst, uint64_t fillpattern,
    bus_size_t len, bus_dmaengine_callback_t callback_fn, void *callback_arg,
    uint32_t flags)
{
	struct ioat_fill_hw_descriptor *hw_desc;
	struct ioat_descriptor *desc;
	struct ioat_softc *ioat;

	CTR0(KTR_IOAT, __func__);
	ioat = to_ioat_softc(dmaengine);

	if ((ioat->capabilities & IOAT_DMACAP_BFILL) == 0) {
		ioat_log_message(0, "%s: Device lacks BFILL capability\n",
		    __func__);
		return (NULL);
	}

	if ((dst & (0xffffull << 48)) != 0) {
		ioat_log_message(0, "%s: High 16 bits of dst invalid\n",
		    __func__);
		return (NULL);
	}

	desc = ioat_op_generic(ioat, IOAT_OP_FILL, len, fillpattern, dst,
	    callback_fn, callback_arg, flags);
	if (desc == NULL)
		return (NULL);

	hw_desc = desc->u.fill;
	if (g_ioat_debug_level >= 3)
		dump_descriptor(hw_desc);

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
ioat_alloc_ring_entry(struct ioat_softc *ioat, int mflags)
{
	struct ioat_generic_hw_descriptor *hw_desc;
	struct ioat_descriptor *desc;
	int error, busdmaflag;

	error = ENOMEM;
	hw_desc = NULL;

	if ((mflags & M_WAITOK) != 0)
		busdmaflag = BUS_DMA_WAITOK;
	else
		busdmaflag = BUS_DMA_NOWAIT;

	desc = malloc(sizeof(*desc), M_IOAT, mflags);
	if (desc == NULL)
		goto out;

	bus_dmamem_alloc(ioat->hw_desc_tag, (void **)&hw_desc,
	    BUS_DMA_ZERO | busdmaflag, &ioat->hw_desc_map);
	if (hw_desc == NULL)
		goto out;

	memset(&desc->bus_dmadesc, 0, sizeof(desc->bus_dmadesc));
	desc->u.generic = hw_desc;

	error = bus_dmamap_load(ioat->hw_desc_tag, ioat->hw_desc_map, hw_desc,
	    sizeof(*hw_desc), ioat_dmamap_cb, &desc->hw_desc_bus_addr,
	    busdmaflag);
	if (error)
		goto out;

out:
	if (error) {
		ioat_free_ring_entry(ioat, desc);
		return (NULL);
	}
	return (desc);
}

static void
ioat_free_ring_entry(struct ioat_softc *ioat, struct ioat_descriptor *desc)
{

	if (desc == NULL)
		return;

	if (desc->u.generic)
		bus_dmamem_free(ioat->hw_desc_tag, desc->u.generic,
		    ioat->hw_desc_map);
	free(desc, M_IOAT);
}

/*
 * Reserves space in this IOAT descriptor ring by ensuring enough slots remain
 * for 'num_descs'.
 *
 * If mflags contains M_WAITOK, blocks until enough space is available.
 *
 * Returns zero on success, or an errno on error.  If num_descs is beyond the
 * maximum ring size, returns EINVAl; if allocation would block and mflags
 * contains M_NOWAIT, returns EAGAIN.
 *
 * Must be called with the submit_lock held; returns with the lock held.  The
 * lock may be dropped to allocate the ring.
 *
 * (The submit_lock is needed to add any entries to the ring, so callers are
 * assured enough room is available.)
 */
static int
ioat_reserve_space(struct ioat_softc *ioat, uint32_t num_descs, int mflags)
{
	struct ioat_descriptor **new_ring;
	uint32_t order;
	int error;

	mtx_assert(&ioat->submit_lock, MA_OWNED);
	error = 0;

	if (num_descs < 1 || num_descs > (1 << IOAT_MAX_ORDER)) {
		error = EINVAL;
		goto out;
	}
	if (ioat->quiescing) {
		error = ENXIO;
		goto out;
	}

	for (;;) {
		if (ioat_get_ring_space(ioat) >= num_descs)
			goto out;

		order = ioat->ring_size_order;
		if (ioat->is_resize_pending || order == IOAT_MAX_ORDER) {
			if ((mflags & M_WAITOK) != 0) {
				msleep(&ioat->tail, &ioat->submit_lock, 0,
				    "ioat_rsz", 0);
				continue;
			}

			error = EAGAIN;
			break;
		}

		ioat->is_resize_pending = TRUE;
		for (;;) {
			mtx_unlock(&ioat->submit_lock);

			new_ring = ioat_prealloc_ring(ioat, 1 << (order + 1),
			    TRUE, mflags);

			mtx_lock(&ioat->submit_lock);
			KASSERT(ioat->ring_size_order == order,
			    ("is_resize_pending should protect order"));

			if (new_ring == NULL) {
				KASSERT((mflags & M_WAITOK) == 0,
				    ("allocation failed"));
				error = EAGAIN;
				break;
			}

			error = ring_grow(ioat, order, new_ring);
			if (error == 0)
				break;
		}
		ioat->is_resize_pending = FALSE;
		wakeup(&ioat->tail);
		if (error)
			break;
	}

out:
	mtx_assert(&ioat->submit_lock, MA_OWNED);
	return (error);
}

static struct ioat_descriptor **
ioat_prealloc_ring(struct ioat_softc *ioat, uint32_t size, boolean_t need_dscr,
    int mflags)
{
	struct ioat_descriptor **ring;
	uint32_t i;
	int error;

	KASSERT(size > 0 && powerof2(size), ("bogus size"));

	ring = malloc(size * sizeof(*ring), M_IOAT, M_ZERO | mflags);
	if (ring == NULL)
		return (NULL);

	if (need_dscr) {
		error = ENOMEM;
		for (i = size / 2; i < size; i++) {
			ring[i] = ioat_alloc_ring_entry(ioat, mflags);
			if (ring[i] == NULL)
				goto out;
			ring[i]->id = i;
		}
	}
	error = 0;

out:
	if (error != 0 && ring != NULL) {
		ioat_free_ring(ioat, size, ring);
		ring = NULL;
	}
	return (ring);
}

static void
ioat_free_ring(struct ioat_softc *ioat, uint32_t size,
    struct ioat_descriptor **ring)
{
	uint32_t i;

	for (i = 0; i < size; i++) {
		if (ring[i] != NULL)
			ioat_free_ring_entry(ioat, ring[i]);
	}
	free(ring, M_IOAT);
}

static struct ioat_descriptor *
ioat_get_ring_entry(struct ioat_softc *ioat, uint32_t index)
{

	return (ioat->ring[index % (1 << ioat->ring_size_order)]);
}

static int
ring_grow(struct ioat_softc *ioat, uint32_t oldorder,
    struct ioat_descriptor **newring)
{
	struct ioat_descriptor *tmp, *next;
	struct ioat_dma_hw_descriptor *hw;
	uint32_t oldsize, newsize, head, tail, i, end;
	int error;

	CTR0(KTR_IOAT, __func__);

	mtx_assert(&ioat->submit_lock, MA_OWNED);

	if (oldorder != ioat->ring_size_order || oldorder >= IOAT_MAX_ORDER) {
		error = EINVAL;
		goto out;
	}

	oldsize = (1 << oldorder);
	newsize = (1 << (oldorder + 1));

	mtx_lock(&ioat->cleanup_lock);

	head = ioat->head & (oldsize - 1);
	tail = ioat->tail & (oldsize - 1);

	/* Copy old descriptors to new ring */
	for (i = 0; i < oldsize; i++)
		newring[i] = ioat->ring[i];

	/*
	 * If head has wrapped but tail hasn't, we must swap some descriptors
	 * around so that tail can increment directly to head.
	 */
	if (head < tail) {
		for (i = 0; i <= head; i++) {
			tmp = newring[oldsize + i];

			newring[oldsize + i] = newring[i];
			newring[oldsize + i]->id = oldsize + i;

			newring[i] = tmp;
			newring[i]->id = i;
		}
		head += oldsize;
	}

	KASSERT(head >= tail, ("invariants"));

	/* Head didn't wrap; we only need to link in oldsize..newsize */
	if (head < oldsize) {
		i = oldsize - 1;
		end = newsize;
	} else {
		/* Head did wrap; link newhead..newsize and 0..oldhead */
		i = head;
		end = newsize + (head - oldsize) + 1;
	}

	/*
	 * Fix up hardware ring, being careful not to trample the active
	 * section (tail -> head).
	 */
	for (; i < end; i++) {
		KASSERT((i & (newsize - 1)) < tail ||
		    (i & (newsize - 1)) >= head, ("trampling snake"));

		next = newring[(i + 1) & (newsize - 1)];
		hw = newring[i & (newsize - 1)]->u.dma;
		hw->next = next->hw_desc_bus_addr;
	}

	free(ioat->ring, M_IOAT);
	ioat->ring = newring;
	ioat->ring_size_order = oldorder + 1;
	ioat->tail = tail;
	ioat->head = head;
	error = 0;

	mtx_unlock(&ioat->cleanup_lock);
out:
	if (error)
		ioat_free_ring(ioat, (1 << (oldorder + 1)), newring);
	return (error);
}

static int
ring_shrink(struct ioat_softc *ioat, uint32_t oldorder,
    struct ioat_descriptor **newring)
{
	struct ioat_dma_hw_descriptor *hw;
	struct ioat_descriptor *ent, *next;
	uint32_t oldsize, newsize, current_idx, new_idx, i;
	int error;

	CTR0(KTR_IOAT, __func__);

	mtx_assert(&ioat->submit_lock, MA_OWNED);

	if (oldorder != ioat->ring_size_order || oldorder <= IOAT_MIN_ORDER) {
		error = EINVAL;
		goto out_unlocked;
	}

	oldsize = (1 << oldorder);
	newsize = (1 << (oldorder - 1));

	mtx_lock(&ioat->cleanup_lock);

	/* Can't shrink below current active set! */
	if (ioat_get_active(ioat) >= newsize) {
		error = ENOMEM;
		goto out;
	}

	/*
	 * Copy current descriptors to the new ring, dropping the removed
	 * descriptors.
	 */
	for (i = 0; i < newsize; i++) {
		current_idx = (ioat->tail + i) & (oldsize - 1);
		new_idx = (ioat->tail + i) & (newsize - 1);

		newring[new_idx] = ioat->ring[current_idx];
		newring[new_idx]->id = new_idx;
	}

	/* Free deleted descriptors */
	for (i = newsize; i < oldsize; i++) {
		ent = ioat_get_ring_entry(ioat, ioat->tail + i);
		ioat_free_ring_entry(ioat, ent);
	}

	/* Fix up hardware ring. */
	hw = newring[(ioat->tail + newsize - 1) & (newsize - 1)]->u.dma;
	next = newring[(ioat->tail + newsize) & (newsize - 1)];
	hw->next = next->hw_desc_bus_addr;

	free(ioat->ring, M_IOAT);
	ioat->ring = newring;
	ioat->ring_size_order = oldorder - 1;
	error = 0;

out:
	mtx_unlock(&ioat->cleanup_lock);
out_unlocked:
	if (error)
		ioat_free_ring(ioat, (1 << (oldorder - 1)), newring);
	return (error);
}

static void
ioat_halted_debug(struct ioat_softc *ioat, uint32_t chanerr)
{
	struct ioat_descriptor *desc;

	ioat_log_message(0, "Channel halted (%b)\n", (int)chanerr,
	    IOAT_CHANERR_STR);
	if (chanerr == 0)
		return;

	mtx_assert(&ioat->cleanup_lock, MA_OWNED);

	desc = ioat_get_ring_entry(ioat, ioat->tail + 0);
	dump_descriptor(desc->u.raw);

	desc = ioat_get_ring_entry(ioat, ioat->tail + 1);
	dump_descriptor(desc->u.raw);
}

static void
ioat_poll_timer_callback(void *arg)
{
	struct ioat_softc *ioat;

	ioat = arg;
	ioat_log_message(3, "%s\n", __func__);

	ioat_process_events(ioat);
}

static void
ioat_shrink_timer_callback(void *arg)
{
	struct ioat_descriptor **newring;
	struct ioat_softc *ioat;
	uint32_t order;

	ioat = arg;
	ioat_log_message(1, "%s\n", __func__);

	/* Slowly scale the ring down if idle. */
	mtx_lock(&ioat->submit_lock);

	/* Don't run while the hardware is being reset. */
	if (ioat->resetting) {
		mtx_unlock(&ioat->submit_lock);
		return;
	}

	order = ioat->ring_size_order;
	if (ioat->is_completion_pending || ioat->is_resize_pending ||
	    order == IOAT_MIN_ORDER) {
		mtx_unlock(&ioat->submit_lock);
		goto out;
	}
	ioat->is_resize_pending = TRUE;
	mtx_unlock(&ioat->submit_lock);

	newring = ioat_prealloc_ring(ioat, 1 << (order - 1), FALSE,
	    M_NOWAIT);

	mtx_lock(&ioat->submit_lock);
	KASSERT(ioat->ring_size_order == order,
	    ("resize_pending protects order"));

	if (newring != NULL && !ioat->is_completion_pending)
		ring_shrink(ioat, order, newring);
	else if (newring != NULL)
		ioat_free_ring(ioat, (1 << (order - 1)), newring);

	ioat->is_resize_pending = FALSE;
	mtx_unlock(&ioat->submit_lock);

out:
	if (ioat->ring_size_order > IOAT_MIN_ORDER)
		callout_reset(&ioat->shrink_timer, IOAT_SHRINK_PERIOD,
		    ioat_shrink_timer_callback, ioat);
}

/*
 * Support Functions
 */
static void
ioat_submit_single(struct ioat_softc *ioat)
{

	ioat_get(ioat, IOAT_ACTIVE_DESCR_REF);
	atomic_add_rel_int(&ioat->head, 1);
	atomic_add_rel_int(&ioat->hw_head, 1);

	if (!ioat->is_completion_pending) {
		ioat->is_completion_pending = TRUE;
		callout_reset(&ioat->poll_timer, 1, ioat_poll_timer_callback,
		    ioat);
		callout_stop(&ioat->shrink_timer);
	}

	ioat->stats.descriptors_submitted++;
}

static int
ioat_reset_hw(struct ioat_softc *ioat)
{
	uint64_t status;
	uint32_t chanerr;
	unsigned timeout;
	int error;

	mtx_lock(IOAT_REFLK);
	while (ioat->resetting && !ioat->destroying)
		msleep(&ioat->resetting, IOAT_REFLK, 0, "IRH_drain", 0);
	if (ioat->destroying) {
		mtx_unlock(IOAT_REFLK);
		return (ENXIO);
	}
	ioat->resetting = TRUE;

	ioat->quiescing = TRUE;
	ioat_drain_locked(ioat);
	mtx_unlock(IOAT_REFLK);

	/*
	 * Suspend ioat_process_events while the hardware and softc are in an
	 * indeterminate state.
	 */
	mtx_lock(&ioat->cleanup_lock);
	ioat->resetting_cleanup = TRUE;
	mtx_unlock(&ioat->cleanup_lock);

	status = ioat_get_chansts(ioat);
	if (is_ioat_active(status) || is_ioat_idle(status))
		ioat_suspend(ioat);

	/* Wait at most 20 ms */
	for (timeout = 0; (is_ioat_active(status) || is_ioat_idle(status)) &&
	    timeout < 20; timeout++) {
		DELAY(1000);
		status = ioat_get_chansts(ioat);
	}
	if (timeout == 20) {
		error = ETIMEDOUT;
		goto out;
	}

	KASSERT(ioat_get_active(ioat) == 0, ("active after quiesce"));

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
	if (ioat_model_resets_msix(ioat)) {
		ioat_log_message(1, "device resets MSI-X registers; saving\n");
		pci_save_state(ioat->device);
	}

	ioat_reset(ioat);

	/* Wait at most 20 ms */
	for (timeout = 0; ioat_reset_pending(ioat) && timeout < 20; timeout++)
		DELAY(1000);
	if (timeout == 20) {
		error = ETIMEDOUT;
		goto out;
	}

	if (ioat_model_resets_msix(ioat)) {
		ioat_log_message(1, "device resets registers; restored\n");
		pci_restore_state(ioat->device);
	}

	/* Reset attempts to return the hardware to "halted." */
	status = ioat_get_chansts(ioat);
	if (is_ioat_active(status) || is_ioat_idle(status)) {
		/* So this really shouldn't happen... */
		ioat_log_message(0, "Device is active after a reset?\n");
		ioat_write_chanctrl(ioat, IOAT_CHANCTRL_RUN);
		error = 0;
		goto out;
	}

	chanerr = ioat_read_4(ioat, IOAT_CHANERR_OFFSET);
	if (chanerr != 0) {
		mtx_lock(&ioat->cleanup_lock);
		ioat_halted_debug(ioat, chanerr);
		mtx_unlock(&ioat->cleanup_lock);
		error = EIO;
		goto out;
	}

	/*
	 * Bring device back online after reset.  Writing CHAINADDR brings the
	 * device back to active.
	 *
	 * The internal ring counter resets to zero, so we have to start over
	 * at zero as well.
	 */
	ioat->tail = ioat->head = ioat->hw_head = 0;
	ioat->last_seen = 0;
	*ioat->comp_update = 0;

	ioat_write_chanctrl(ioat, IOAT_CHANCTRL_RUN);
	ioat_write_chancmp(ioat, ioat->comp_update_bus_addr);
	ioat_write_chainaddr(ioat, ioat->ring[0]->hw_desc_bus_addr);
	error = 0;

out:
	/*
	 * Resume completions now that ring state is consistent.
	 * ioat_start_channel will add a pending completion and if we are still
	 * blocking completions, we may livelock.
	 */
	mtx_lock(&ioat->cleanup_lock);
	ioat->resetting_cleanup = FALSE;
	mtx_unlock(&ioat->cleanup_lock);

	/* Enqueues a null operation and ensures it completes. */
	if (error == 0)
		error = ioat_start_channel(ioat);

	/* Unblock submission of new work */
	mtx_lock(IOAT_REFLK);
	ioat->quiescing = FALSE;
	wakeup(&ioat->quiescing);

	ioat->resetting = FALSE;
	wakeup(&ioat->resetting);
	mtx_unlock(IOAT_REFLK);

	return (error);
}

static int
sysctl_handle_chansts(SYSCTL_HANDLER_ARGS)
{
	struct ioat_softc *ioat;
	struct sbuf sb;
	uint64_t status;
	int error;

	ioat = arg1;

	status = ioat_get_chansts(ioat) & IOAT_CHANSTS_STATUS;

	sbuf_new_for_sysctl(&sb, NULL, 256, req);
	switch (status) {
	case IOAT_CHANSTS_ACTIVE:
		sbuf_printf(&sb, "ACTIVE");
		break;
	case IOAT_CHANSTS_IDLE:
		sbuf_printf(&sb, "IDLE");
		break;
	case IOAT_CHANSTS_SUSPENDED:
		sbuf_printf(&sb, "SUSPENDED");
		break;
	case IOAT_CHANSTS_HALTED:
		sbuf_printf(&sb, "HALTED");
		break;
	case IOAT_CHANSTS_ARMED:
		sbuf_printf(&sb, "ARMED");
		break;
	default:
		sbuf_printf(&sb, "UNKNOWN");
		break;
	}
	error = sbuf_finish(&sb);
	sbuf_delete(&sb);

	if (error != 0 || req->newptr == NULL)
		return (error);
	return (EINVAL);
}

static int
sysctl_handle_dpi(SYSCTL_HANDLER_ARGS)
{
	struct ioat_softc *ioat;
	struct sbuf sb;
#define	PRECISION	"1"
	const uintmax_t factor = 10;
	uintmax_t rate;
	int error;

	ioat = arg1;
	sbuf_new_for_sysctl(&sb, NULL, 16, req);

	if (ioat->stats.interrupts == 0) {
		sbuf_printf(&sb, "NaN");
		goto out;
	}
	rate = ioat->stats.descriptors_processed * factor /
	    ioat->stats.interrupts;
	sbuf_printf(&sb, "%ju.%." PRECISION "ju", rate / factor,
	    rate % factor);
#undef	PRECISION
out:
	error = sbuf_finish(&sb);
	sbuf_delete(&sb);
	if (error != 0 || req->newptr == NULL)
		return (error);
	return (EINVAL);
}

static int
sysctl_handle_error(SYSCTL_HANDLER_ARGS)
{
	struct ioat_descriptor *desc;
	struct ioat_softc *ioat;
	int error, arg;

	ioat = arg1;

	arg = 0;
	error = SYSCTL_OUT(req, &arg, sizeof(arg));
	if (error != 0 || req->newptr == NULL)
		return (error);

	error = SYSCTL_IN(req, &arg, sizeof(arg));
	if (error != 0)
		return (error);

	if (arg != 0) {
		ioat_acquire(&ioat->dmaengine);
		desc = ioat_op_generic(ioat, IOAT_OP_COPY, 1,
		    0xffff000000000000ull, 0xffff000000000000ull, NULL, NULL,
		    0);
		if (desc == NULL)
			error = ENOMEM;
		else
			ioat_submit_single(ioat);
		ioat_release(&ioat->dmaengine);
	}
	return (error);
}

static int
sysctl_handle_reset(SYSCTL_HANDLER_ARGS)
{
	struct ioat_softc *ioat;
	int error, arg;

	ioat = arg1;

	arg = 0;
	error = SYSCTL_OUT(req, &arg, sizeof(arg));
	if (error != 0 || req->newptr == NULL)
		return (error);

	error = SYSCTL_IN(req, &arg, sizeof(arg));
	if (error != 0)
		return (error);

	if (arg != 0)
		error = ioat_reset_hw(ioat);

	return (error);
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
	struct sysctl_oid_list *par, *statpar, *state, *hammer;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree, *tmp;
	struct ioat_softc *ioat;

	ioat = DEVICE2SOFTC(device);
	ctx = device_get_sysctl_ctx(device);
	tree = device_get_sysctl_tree(device);
	par = SYSCTL_CHILDREN(tree);

	SYSCTL_ADD_INT(ctx, par, OID_AUTO, "version", CTLFLAG_RD,
	    &ioat->version, 0, "HW version (0xMM form)");
	SYSCTL_ADD_UINT(ctx, par, OID_AUTO, "max_xfer_size", CTLFLAG_RD,
	    &ioat->max_xfer_size, 0, "HW maximum transfer size");
	SYSCTL_ADD_INT(ctx, par, OID_AUTO, "intrdelay_supported", CTLFLAG_RD,
	    &ioat->intrdelay_supported, 0, "Is INTRDELAY supported");
	SYSCTL_ADD_U16(ctx, par, OID_AUTO, "intrdelay_max", CTLFLAG_RD,
	    &ioat->intrdelay_max, 0,
	    "Maximum configurable INTRDELAY on this channel (microseconds)");

	tmp = SYSCTL_ADD_NODE(ctx, par, OID_AUTO, "state", CTLFLAG_RD, NULL,
	    "IOAT channel internal state");
	state = SYSCTL_CHILDREN(tmp);

	SYSCTL_ADD_UINT(ctx, state, OID_AUTO, "ring_size_order", CTLFLAG_RD,
	    &ioat->ring_size_order, 0, "SW descriptor ring size order");
	SYSCTL_ADD_UINT(ctx, state, OID_AUTO, "head", CTLFLAG_RD, &ioat->head,
	    0, "SW descriptor head pointer index");
	SYSCTL_ADD_UINT(ctx, state, OID_AUTO, "tail", CTLFLAG_RD, &ioat->tail,
	    0, "SW descriptor tail pointer index");
	SYSCTL_ADD_UINT(ctx, state, OID_AUTO, "hw_head", CTLFLAG_RD,
	    &ioat->hw_head, 0, "HW DMACOUNT");

	SYSCTL_ADD_UQUAD(ctx, state, OID_AUTO, "last_completion", CTLFLAG_RD,
	    ioat->comp_update, "HW addr of last completion");

	SYSCTL_ADD_INT(ctx, state, OID_AUTO, "is_resize_pending", CTLFLAG_RD,
	    &ioat->is_resize_pending, 0, "resize pending");
	SYSCTL_ADD_INT(ctx, state, OID_AUTO, "is_completion_pending",
	    CTLFLAG_RD, &ioat->is_completion_pending, 0, "completion pending");
	SYSCTL_ADD_INT(ctx, state, OID_AUTO, "is_reset_pending", CTLFLAG_RD,
	    &ioat->is_reset_pending, 0, "reset pending");
	SYSCTL_ADD_INT(ctx, state, OID_AUTO, "is_channel_running", CTLFLAG_RD,
	    &ioat->is_channel_running, 0, "channel running");

	SYSCTL_ADD_PROC(ctx, state, OID_AUTO, "chansts",
	    CTLTYPE_STRING | CTLFLAG_RD, ioat, 0, sysctl_handle_chansts, "A",
	    "String of the channel status");

	SYSCTL_ADD_U16(ctx, state, OID_AUTO, "intrdelay", CTLFLAG_RD,
	    &ioat->cached_intrdelay, 0,
	    "Current INTRDELAY on this channel (cached, microseconds)");

	tmp = SYSCTL_ADD_NODE(ctx, par, OID_AUTO, "hammer", CTLFLAG_RD, NULL,
	    "Big hammers (mostly for testing)");
	hammer = SYSCTL_CHILDREN(tmp);

	SYSCTL_ADD_PROC(ctx, hammer, OID_AUTO, "force_hw_reset",
	    CTLTYPE_INT | CTLFLAG_RW, ioat, 0, sysctl_handle_reset, "I",
	    "Set to non-zero to reset the hardware");
	SYSCTL_ADD_PROC(ctx, hammer, OID_AUTO, "force_hw_error",
	    CTLTYPE_INT | CTLFLAG_RW, ioat, 0, sysctl_handle_error, "I",
	    "Set to non-zero to inject a recoverable hardware error");

	tmp = SYSCTL_ADD_NODE(ctx, par, OID_AUTO, "stats", CTLFLAG_RD, NULL,
	    "IOAT channel statistics");
	statpar = SYSCTL_CHILDREN(tmp);

	SYSCTL_ADD_UQUAD(ctx, statpar, OID_AUTO, "interrupts", CTLFLAG_RW,
	    &ioat->stats.interrupts,
	    "Number of interrupts processed on this channel");
	SYSCTL_ADD_UQUAD(ctx, statpar, OID_AUTO, "descriptors", CTLFLAG_RW,
	    &ioat->stats.descriptors_processed,
	    "Number of descriptors processed on this channel");
	SYSCTL_ADD_UQUAD(ctx, statpar, OID_AUTO, "submitted", CTLFLAG_RW,
	    &ioat->stats.descriptors_submitted,
	    "Number of descriptors submitted to this channel");
	SYSCTL_ADD_UQUAD(ctx, statpar, OID_AUTO, "errored", CTLFLAG_RW,
	    &ioat->stats.descriptors_error,
	    "Number of descriptors failed by channel errors");
	SYSCTL_ADD_U32(ctx, statpar, OID_AUTO, "halts", CTLFLAG_RW,
	    &ioat->stats.channel_halts, 0,
	    "Number of times the channel has halted");
	SYSCTL_ADD_U32(ctx, statpar, OID_AUTO, "last_halt_chanerr", CTLFLAG_RW,
	    &ioat->stats.last_halt_chanerr, 0,
	    "The raw CHANERR when the channel was last halted");

	SYSCTL_ADD_PROC(ctx, statpar, OID_AUTO, "desc_per_interrupt",
	    CTLTYPE_STRING | CTLFLAG_RD, ioat, 0, sysctl_handle_dpi, "A",
	    "Descriptors per interrupt");
}

static inline struct ioat_softc *
ioat_get(struct ioat_softc *ioat, enum ioat_ref_kind kind)
{
	uint32_t old;

	KASSERT(kind < IOAT_NUM_REF_KINDS, ("bogus"));

	old = atomic_fetchadd_32(&ioat->refcnt, 1);
	KASSERT(old < UINT32_MAX, ("refcnt overflow"));

#ifdef INVARIANTS
	old = atomic_fetchadd_32(&ioat->refkinds[kind], 1);
	KASSERT(old < UINT32_MAX, ("refcnt kind overflow"));
#endif

	return (ioat);
}

static inline void
ioat_putn(struct ioat_softc *ioat, uint32_t n, enum ioat_ref_kind kind)
{

	_ioat_putn(ioat, n, kind, FALSE);
}

static inline void
ioat_putn_locked(struct ioat_softc *ioat, uint32_t n, enum ioat_ref_kind kind)
{

	_ioat_putn(ioat, n, kind, TRUE);
}

static inline void
_ioat_putn(struct ioat_softc *ioat, uint32_t n, enum ioat_ref_kind kind,
    boolean_t locked)
{
	uint32_t old;

	KASSERT(kind < IOAT_NUM_REF_KINDS, ("bogus"));

	if (n == 0)
		return;

#ifdef INVARIANTS
	old = atomic_fetchadd_32(&ioat->refkinds[kind], -n);
	KASSERT(old >= n, ("refcnt kind underflow"));
#endif

	/* Skip acquiring the lock if resulting refcnt > 0. */
	for (;;) {
		old = ioat->refcnt;
		if (old <= n)
			break;
		if (atomic_cmpset_32(&ioat->refcnt, old, old - n))
			return;
	}

	if (locked)
		mtx_assert(IOAT_REFLK, MA_OWNED);
	else
		mtx_lock(IOAT_REFLK);

	old = atomic_fetchadd_32(&ioat->refcnt, -n);
	KASSERT(old >= n, ("refcnt error"));

	if (old == n)
		wakeup(IOAT_REFLK);
	if (!locked)
		mtx_unlock(IOAT_REFLK);
}

static inline void
ioat_put(struct ioat_softc *ioat, enum ioat_ref_kind kind)
{

	ioat_putn(ioat, 1, kind);
}

static void
ioat_drain_locked(struct ioat_softc *ioat)
{

	mtx_assert(IOAT_REFLK, MA_OWNED);
	while (ioat->refcnt > 0)
		msleep(IOAT_REFLK, IOAT_REFLK, 0, "ioat_drain", 0);
}

#ifdef DDB
#define	_db_show_lock(lo)	LOCK_CLASS(lo)->lc_ddb_show(lo)
#define	db_show_lock(lk)	_db_show_lock(&(lk)->lock_object)
DB_SHOW_COMMAND(ioat, db_show_ioat)
{
	struct ioat_softc *sc;
	unsigned idx;

	if (!have_addr)
		goto usage;
	idx = (unsigned)addr;
	if (idx >= ioat_channel_index)
		goto usage;

	sc = ioat_channel[idx];
	db_printf("ioat softc at %p\n", sc);
	if (sc == NULL)
		return;

	db_printf(" version: %d\n", sc->version);
	db_printf(" chan_idx: %u\n", sc->chan_idx);
	db_printf(" submit_lock: ");
	db_show_lock(&sc->submit_lock);

	db_printf(" capabilities: %b\n", (int)sc->capabilities,
	    IOAT_DMACAP_STR);
	db_printf(" cached_intrdelay: %u\n", sc->cached_intrdelay);
	db_printf(" *comp_update: 0x%jx\n", (uintmax_t)*sc->comp_update);

	db_printf(" poll_timer:\n");
	db_printf("  c_time: %ju\n", (uintmax_t)sc->poll_timer.c_time);
	db_printf("  c_arg: %p\n", sc->poll_timer.c_arg);
	db_printf("  c_func: %p\n", sc->poll_timer.c_func);
	db_printf("  c_lock: %p\n", sc->poll_timer.c_lock);
	db_printf("  c_flags: 0x%x\n", (unsigned)sc->poll_timer.c_flags);

	db_printf(" shrink_timer:\n");
	db_printf("  c_time: %ju\n", (uintmax_t)sc->shrink_timer.c_time);
	db_printf("  c_arg: %p\n", sc->shrink_timer.c_arg);
	db_printf("  c_func: %p\n", sc->shrink_timer.c_func);
	db_printf("  c_lock: %p\n", sc->shrink_timer.c_lock);
	db_printf("  c_flags: 0x%x\n", (unsigned)sc->shrink_timer.c_flags);

	db_printf(" quiescing: %d\n", (int)sc->quiescing);
	db_printf(" destroying: %d\n", (int)sc->destroying);
	db_printf(" is_resize_pending: %d\n", (int)sc->is_resize_pending);
	db_printf(" is_completion_pending: %d\n", (int)sc->is_completion_pending);
	db_printf(" is_reset_pending: %d\n", (int)sc->is_reset_pending);
	db_printf(" is_channel_running: %d\n", (int)sc->is_channel_running);
	db_printf(" intrdelay_supported: %d\n", (int)sc->intrdelay_supported);
	db_printf(" resetting: %d\n", (int)sc->resetting);

	db_printf(" head: %u\n", sc->head);
	db_printf(" tail: %u\n", sc->tail);
	db_printf(" hw_head: %u\n", sc->hw_head);
	db_printf(" ring_size_order: %u\n", sc->ring_size_order);
	db_printf(" last_seen: 0x%lx\n", sc->last_seen);
	db_printf(" ring: %p\n", sc->ring);

	db_printf("  ring[%u] (tail):\n", sc->tail %
	    (1 << sc->ring_size_order));
	db_printf("   id: %u\n", ioat_get_ring_entry(sc, sc->tail)->id);
	db_printf("   addr: 0x%lx\n",
	    ioat_get_ring_entry(sc, sc->tail)->hw_desc_bus_addr);
	db_printf("   next: 0x%lx\n",
	    ioat_get_ring_entry(sc, sc->tail)->u.generic->next);

	db_printf("  ring[%u] (head - 1):\n", (sc->head - 1) %
	    (1 << sc->ring_size_order));
	db_printf("   id: %u\n", ioat_get_ring_entry(sc, sc->head - 1)->id);
	db_printf("   addr: 0x%lx\n",
	    ioat_get_ring_entry(sc, sc->head - 1)->hw_desc_bus_addr);
	db_printf("   next: 0x%lx\n",
	    ioat_get_ring_entry(sc, sc->head - 1)->u.generic->next);

	db_printf("  ring[%u] (head):\n", (sc->head) %
	    (1 << sc->ring_size_order));
	db_printf("   id: %u\n", ioat_get_ring_entry(sc, sc->head)->id);
	db_printf("   addr: 0x%lx\n",
	    ioat_get_ring_entry(sc, sc->head)->hw_desc_bus_addr);
	db_printf("   next: 0x%lx\n",
	    ioat_get_ring_entry(sc, sc->head)->u.generic->next);

	for (idx = 0; idx < (1 << sc->ring_size_order); idx++)
		if ((*sc->comp_update & IOAT_CHANSTS_COMPLETED_DESCRIPTOR_MASK)
		    == ioat_get_ring_entry(sc, idx)->hw_desc_bus_addr)
			db_printf("  ring[%u] == hardware tail\n", idx);

	db_printf(" cleanup_lock: ");
	db_show_lock(&sc->cleanup_lock);

	db_printf(" refcnt: %u\n", sc->refcnt);
#ifdef INVARIANTS
	CTASSERT(IOAT_NUM_REF_KINDS == 2);
	db_printf(" refkinds: [ENG=%u, DESCR=%u]\n", sc->refkinds[0],
	    sc->refkinds[1]);
#endif
	db_printf(" stats:\n");
	db_printf("  interrupts: %lu\n", sc->stats.interrupts);
	db_printf("  descriptors_processed: %lu\n", sc->stats.descriptors_processed);
	db_printf("  descriptors_error: %lu\n", sc->stats.descriptors_error);
	db_printf("  descriptors_submitted: %lu\n", sc->stats.descriptors_submitted);

	db_printf("  channel_halts: %u\n", sc->stats.channel_halts);
	db_printf("  last_halt_chanerr: %u\n", sc->stats.last_halt_chanerr);

	if (db_pager_quit)
		return;

	db_printf(" hw status:\n");
	db_printf("  status: 0x%lx\n", ioat_get_chansts(sc));
	db_printf("  chanctrl: 0x%x\n",
	    (unsigned)ioat_read_2(sc, IOAT_CHANCTRL_OFFSET));
	db_printf("  chancmd: 0x%x\n",
	    (unsigned)ioat_read_1(sc, IOAT_CHANCMD_OFFSET));
	db_printf("  dmacount: 0x%x\n",
	    (unsigned)ioat_read_2(sc, IOAT_DMACOUNT_OFFSET));
	db_printf("  chainaddr: 0x%lx\n",
	    ioat_read_double_4(sc, IOAT_CHAINADDR_OFFSET_LOW));
	db_printf("  chancmp: 0x%lx\n",
	    ioat_read_double_4(sc, IOAT_CHANCMP_OFFSET_LOW));
	db_printf("  chanerr: %b\n",
	    (int)ioat_read_4(sc, IOAT_CHANERR_OFFSET), IOAT_CHANERR_STR);
	return;
usage:
	db_printf("usage: show ioat <0-%u>\n", ioat_channel_index);
	return;
}
#endif /* DDB */
