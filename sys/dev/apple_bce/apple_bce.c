/*
 * Copyright (c) 2026 Abdelkader Boudih <freebsd@seuros.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Apple T2 Buffer Copy Engine (BCE) PCI driver.
 * Provides the transport layer for T2 coprocessor communication:
 * mailbox handshake, DMA queue setup, and firmware keepalive.
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/sema.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/rman.h>
#include <sys/time.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/atomic.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include "apple_bce.h"
#include "apple_bce_mailbox.h"
#include "apple_bce_queue.h"

static int	apple_bce_probe(device_t dev);
static int	apple_bce_attach(device_t dev);
static int	apple_bce_detach(device_t dev);
static void	apple_bce_timestamp_cb(void *arg);
static void	apple_bce_timestamp_init(struct apple_bce_softc *sc);
static void	apple_bce_timestamp_start(struct apple_bce_softc *sc,
		    int is_initial);
static void	apple_bce_timestamp_stop(struct apple_bce_softc *sc);

/*
 * Timestamp protocol -- T2 firmware expects periodic boottime updates.
 *
 * Register layout at BAR4 + 0xC000:
 *   regb + 0 (0xC000): high 32 bits / control
 *   regb + 8 (0xC008): low 32 bits / data
 *
 * Control opcodes (written to regb+8 with 0xFFFFFFFF to regb+0):
 *   -4 (0xFFFFFFFC): initial start
 *   -3 (0xFFFFFFFD): restart
 *   -2 (0xFFFFFFFE): stop
 */
static void
apple_bce_timestamp_init(struct apple_bce_softc *sc)
{
	/* Read control register and barrier to sync with firmware */
	bus_read_4(sc->sc_bar4, BCE_REG_TIMESTAMP);
	mb();
}

static void
apple_bce_timestamp_start(struct apple_bce_softc *sc, int is_initial)
{
	/* Send start opcode: -4 for initial, -3 for restart */
	bus_write_4(sc->sc_bar4, BCE_REG_TIMESTAMP + 8,
	    is_initial ? 0xFFFFFFFC : 0xFFFFFFFD);
	bus_write_4(sc->sc_bar4, BCE_REG_TIMESTAMP, 0xFFFFFFFF);

	mtx_lock_spin(&sc->sc_timestamp_lock);
	sc->sc_timestamp_stopped = 0;
	mtx_unlock_spin(&sc->sc_timestamp_lock);

	callout_reset(&sc->sc_timestamp_co, hz * BCE_TIMESTAMP_MS / 1000,
	    apple_bce_timestamp_cb, sc);
}

static void
apple_bce_timestamp_stop(struct apple_bce_softc *sc)
{
	mtx_lock_spin(&sc->sc_timestamp_lock);
	sc->sc_timestamp_stopped = 1;
	mtx_unlock_spin(&sc->sc_timestamp_lock);

	callout_drain(&sc->sc_timestamp_co);

	/* Send stop opcode */
	bus_write_4(sc->sc_bar4, BCE_REG_TIMESTAMP + 8, 0xFFFFFFFE);
	bus_write_4(sc->sc_bar4, BCE_REG_TIMESTAMP, 0xFFFFFFFF);
}

static void
apple_bce_timestamp_cb(void *arg)
{
	struct apple_bce_softc *sc = arg;
	struct bintime bt;
	uint64_t ns;

	/* Read to sync, then barrier */
	bus_read_4(sc->sc_bar4, BCE_REG_TIMESTAMP + 8);
	mb();

	/* Get boot time in nanoseconds */
	binuptime(&bt);
	ns = (uint64_t)bt.sec * 1000000000ULL +
	    (((uint64_t)(bt.frac >> 32) * 1000000000ULL) >> 32);

	/* Write: low 32 bits to regb+8, high 32 bits to regb+0 */
	bus_write_4(sc->sc_bar4, BCE_REG_TIMESTAMP + 8, (uint32_t)ns);
	bus_write_4(sc->sc_bar4, BCE_REG_TIMESTAMP, (uint32_t)(ns >> 32));

	mtx_lock_spin(&sc->sc_timestamp_lock);
	if (!sc->sc_timestamp_stopped)
		callout_schedule(&sc->sc_timestamp_co,
		    hz * BCE_TIMESTAMP_MS / 1000);
	mtx_unlock_spin(&sc->sc_timestamp_lock);
}

/*
 * IRQ handlers.
 */
static void
apple_bce_mbox_intr(void *arg)
{
	struct apple_bce_softc *sc = arg;

	bce_mailbox_handle_interrupt(&sc->sc_mbox);
}

static void
apple_bce_dma_intr(void *arg)
{
	struct apple_bce_softc *sc = arg;
	int i;

	/*
	 * Only process registered CQs. sc_cq_list[] contains only CQ
	 * pointers -- separate from sc_queues[] which mixes CQ and SQ.
	 */
	mtx_lock(&sc->sc_queues_lock);
	for (i = 0; i < BCE_MAX_CQ_COUNT; i++) {
		if (sc->sc_cq_list[i] != NULL)
			bce_handle_cq_completions(sc, sc->sc_cq_list[i]);
	}
	mtx_unlock(&sc->sc_queues_lock);
}

/*
 * Enable bus master on PCI function 0 (NVMe).
 * T2 requires function 0 to be bus master for DMA on function 1.
 */
static int
apple_bce_enable_pci0_busmaster(device_t dev)
{
	device_t pci0;
	device_t bus;

	bus = device_get_parent(dev);
	if (bus == NULL)
		return (ENXIO);

	/*
	 * Find function 0 on the same bus/slot.
	 * Our device is function 1; function 0 is NVMe.
	 */
	pci0 = pci_find_dbsf(pci_get_domain(dev), pci_get_bus(dev),
	    pci_get_slot(dev), 0);
	if (pci0 == NULL) {
		device_printf(dev, "cannot find PCI function 0\n");
		return (ENXIO);
	}

	pci_enable_busmaster(pci0);
	device_printf(dev, "enabled bus master on function 0 (%s)\n",
	    device_get_nameunit(pci0));
	return (0);
}

/*
 * Firmware handshake via mailbox.
 */
static int
apple_bce_fw_handshake(struct apple_bce_softc *sc)
{
	uint64_t reply;
	int error;

	error = bce_mailbox_send(&sc->sc_mbox,
	    BCE_MB_MSG(BCE_MB_SET_FW_PROTOCOL_VER, BCE_FW_PROTOCOL_VER),
	    &reply, BCE_MBOX_TIMEOUT_MS);
	if (error != 0) {
		device_printf(sc->sc_dev, "firmware handshake timeout\n");
		return (error);
	}

	if (BCE_MB_TYPE(reply) != BCE_MB_SET_FW_PROTOCOL_VER ||
	    BCE_MB_VALUE(reply) != BCE_FW_PROTOCOL_VER) {
		device_printf(sc->sc_dev,
		    "firmware version mismatch: got type=%u val=0x%llx\n",
		    BCE_MB_TYPE(reply),
		    (unsigned long long)BCE_MB_VALUE(reply));
		return (ENODEV);
	}

	device_printf(sc->sc_dev, "firmware handshake OK (protocol 0x%x)\n",
	    BCE_FW_PROTOCOL_VER);
	return (0);
}

/*
 * DMA callback for command queue registration.
 */
static void
bce_reg_dma_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct { bus_addr_t addr; int error; } *cb = arg;

	cb->error = error;
	if (error == 0)
		cb->addr = segs[0].ds_addr;
}

/*
 * Register a command queue (CQ or SQ) with firmware via mailbox.
 * The memcfg struct is DMA-mapped and its physical address sent
 * in the mailbox message -- firmware reads the config from DMA.
 */
static int
apple_bce_register_cmd_queue(struct apple_bce_softc *sc,
    struct bce_queue_memcfg *cfg, int is_sq)
{
	bus_dma_tag_t tag;
	bus_dmamap_t map;
	bus_addr_t paddr;
	struct bce_queue_memcfg *dma_cfg;
	struct {
		bus_addr_t addr;
		int error;
	} cb;
	uint64_t reply;
	int error, cmd_type;

	/* Allocate DMA-coherent buffer for memcfg (8-byte aligned) */
	error = bus_dma_tag_create(sc->sc_dma_tag,
	    8, 0, BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR,
	    NULL, NULL, sizeof(*cfg), 1, sizeof(*cfg),
	    BUS_DMA_WAITOK, NULL, NULL, &tag);
	if (error != 0)
		return (error);

	error = bus_dmamem_alloc(tag, (void **)&dma_cfg,
	    BUS_DMA_WAITOK | BUS_DMA_ZERO | BUS_DMA_COHERENT, &map);
	if (error != 0) {
		bus_dma_tag_destroy(tag);
		return (error);
	}

	/* Copy config and load DMA address */
	memcpy(dma_cfg, cfg, sizeof(*cfg));

	cb.error = 0;
	error = bus_dmamap_load(tag, map, dma_cfg, sizeof(*cfg),
	    bce_reg_dma_cb, &cb, BUS_DMA_WAITOK);
	if (error != 0 || cb.error != 0) {
		bus_dmamem_free(tag, dma_cfg, map);
		bus_dma_tag_destroy(tag);
		return (error != 0 ? error : cb.error);
	}
	paddr = cb.addr;

	/* Sync DMA buffer before device access */
	bus_dmamap_sync(tag, map, BUS_DMASYNC_PREWRITE);

	/* Send DMA address of memcfg to firmware */
	cmd_type = is_sq ? BCE_MB_REGISTER_CMD_SQ : BCE_MB_REGISTER_CMD_CQ;
	error = bce_mailbox_send(&sc->sc_mbox,
	    BCE_MB_MSG(cmd_type, paddr), &reply, BCE_MBOX_TIMEOUT_MS);

	bus_dmamap_sync(tag, map, BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(tag, map);
	bus_dmamem_free(tag, dma_cfg, map);
	bus_dma_tag_destroy(tag);

	if (error != 0)
		return (error);

	if (BCE_MB_TYPE(reply) != BCE_MB_REGISTER_QUEUE_REPLY) {
		device_printf(sc->sc_dev,
		    "unexpected queue registration reply: type=%u\n",
		    BCE_MB_TYPE(reply));
		return (EINVAL);
	}

	return (0);
}

/*
 * Setup command queues (CQ qid=0, SQ qid=1).
 */
static int
apple_bce_setup_cmd_queues(struct apple_bce_softc *sc)
{
	struct bce_queue_memcfg cfg;
	struct bce_queue_sq *sq;
	int error;

	/* Allocate command CQ (qid 0, 32 entries) */
	sc->sc_cmd_cq = bce_alloc_cq(sc, 0, 32);
	if (sc->sc_cmd_cq == NULL) {
		device_printf(sc->sc_dev, "failed to allocate command CQ\n");
		return (ENOMEM);
	}

	/* Register CQ with firmware via DMA-mapped memcfg */
	bce_get_cq_memcfg(sc->sc_cmd_cq, &cfg);
	error = apple_bce_register_cmd_queue(sc, &cfg, 0);
	if (error != 0) {
		device_printf(sc->sc_dev, "failed to register command CQ\n");
		goto fail_cq;
	}

	/* Store CQ in queue registries */
	mtx_lock(&sc->sc_queues_lock);
	sc->sc_queues[0] = sc->sc_cmd_cq;
	sc->sc_cq_list[0] = sc->sc_cmd_cq;
	mtx_unlock(&sc->sc_queues_lock);

	/* Allocate command SQ (qid 1, 64-byte elements, 32 entries) */
	sq = bce_alloc_sq(sc, 1, BCE_CMD_SIZE, 32, NULL, NULL);
	if (sq == NULL) {
		device_printf(sc->sc_dev, "failed to allocate command SQ\n");
		goto fail_cq;
	}

	/* Wrap SQ in command queue (sets completion callback internally) */
	sc->sc_cmd_cmdq = bce_alloc_cmdq(sc, sq);
	if (sc->sc_cmd_cmdq == NULL) {
		bce_free_sq(sc, sq);
		goto fail_cq;
	}

	/* Register SQ with firmware via DMA-mapped memcfg */
	bce_get_sq_memcfg(sq, sc->sc_cmd_cq, &cfg);
	error = apple_bce_register_cmd_queue(sc, &cfg, 1);
	if (error != 0) {
		device_printf(sc->sc_dev, "failed to register command SQ\n");
		goto fail_sq;
	}

	/* Store SQ in queue registry */
	mtx_lock(&sc->sc_queues_lock);
	sc->sc_queues[1] = sq;
	sc->sc_int_sq_list[0] = sq;
	mtx_unlock(&sc->sc_queues_lock);

	device_printf(sc->sc_dev, "command queues created (CQ=0, SQ=1)\n");
	return (0);

fail_sq:
	bce_free_cmdq(sc->sc_cmd_cmdq);
	sc->sc_cmd_cmdq = NULL;
	bce_free_sq(sc, sq);
fail_cq:
	/* Clear CQ from registries before freeing */
	mtx_lock(&sc->sc_queues_lock);
	sc->sc_queues[0] = NULL;
	sc->sc_cq_list[0] = NULL;
	mtx_unlock(&sc->sc_queues_lock);
	bce_free_cq(sc, sc->sc_cmd_cq);
	sc->sc_cmd_cq = NULL;
	return (error != 0 ? error : ENOMEM);
}

/*
 * PCI probe.
 */
static int
apple_bce_probe(device_t dev)
{
	if (pci_get_vendor(dev) != BCE_PCI_VENDOR_APPLE ||
	    pci_get_device(dev) != BCE_PCI_DEVICE_T2)
		return (ENXIO);

	/* Only attach to function 1 (BCE), not function 0 (NVMe) */
	if (pci_get_function(dev) != 1)
		return (ENXIO);

	device_set_desc(dev, "Apple T2 Buffer Copy Engine");
	return (BUS_PROBE_DEFAULT);
}

/*
 * PCI attach.
 */
static int
apple_bce_attach(device_t dev)
{
	struct apple_bce_softc *sc = device_get_softc(dev);
	int error;

	sc->sc_dev = dev;
	pci_enable_busmaster(dev);
	mtx_init(&sc->sc_queues_lock, "bce_queues", NULL, MTX_DEF);
	mtx_init(&sc->sc_timestamp_lock, "bce_ts", NULL, MTX_SPIN);
	callout_init(&sc->sc_timestamp_co, 1);

	/* Map BAR2 (DMA) and BAR4 (mailbox) */
	sc->sc_bar2_rid = PCIR_BAR(2);
	sc->sc_bar2 = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->sc_bar2_rid, RF_ACTIVE);
	if (sc->sc_bar2 == NULL) {
		device_printf(dev, "cannot map BAR2 (DMA)\n");
		error = ENXIO;
		goto fail;
	}

	sc->sc_bar4_rid = PCIR_BAR(4);
	sc->sc_bar4 = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->sc_bar4_rid, RF_ACTIVE);
	if (sc->sc_bar4 == NULL) {
		device_printf(dev, "cannot map BAR4 (mailbox)\n");
		error = ENXIO;
		goto fail;
	}

	/* Allocate MSI vectors (need at least 5: mbox=0, dma=4) */
	sc->sc_msi_count = 8;	/* Must be power of 2 for FreeBSD */
	error = pci_alloc_msi(dev, &sc->sc_msi_count);
	if (error != 0 || sc->sc_msi_count < 8) {
		device_printf(dev, "cannot allocate MSI vectors: "
		    "error=%d got=%d\n", error, sc->sc_msi_count);
		if (error == 0 && sc->sc_msi_count > 0)
			pci_release_msi(dev);
		sc->sc_msi_count = 0;
		error = ENXIO;
		goto fail;
	}

	/* Initialize mailbox before installing handlers */
	bce_mailbox_init(&sc->sc_mbox, sc->sc_bar4);

	/* Setup IRQ: vector 0 = mailbox, vector 4 = DMA */
	sc->sc_irq_rid_mbox = 1;	/* MSI vectors start at rid 1 */
	sc->sc_irq_mbox = bus_alloc_resource_any(dev, SYS_RES_IRQ,
	    &sc->sc_irq_rid_mbox, RF_ACTIVE);
	if (sc->sc_irq_mbox == NULL) {
		device_printf(dev, "cannot allocate mailbox IRQ\n");
		error = ENXIO;
		goto fail;
	}

	error = bus_setup_intr(dev, sc->sc_irq_mbox,
	    INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, apple_bce_mbox_intr, sc, &sc->sc_irq_mbox_cookie);
	if (error != 0) {
		device_printf(dev, "cannot setup mailbox IRQ\n");
		goto fail;
	}

	sc->sc_irq_rid_dma = 5;	/* Vector 4 = rid 5 */
	sc->sc_irq_dma = bus_alloc_resource_any(dev, SYS_RES_IRQ,
	    &sc->sc_irq_rid_dma, RF_ACTIVE);
	if (sc->sc_irq_dma == NULL) {
		device_printf(dev, "cannot allocate DMA IRQ\n");
		error = ENXIO;
		goto fail;
	}

	error = bus_setup_intr(dev, sc->sc_irq_dma,
	    INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, apple_bce_dma_intr, sc, &sc->sc_irq_dma_cookie);
	if (error != 0) {
		device_printf(dev, "cannot setup DMA IRQ\n");
		goto fail;
	}

	/* Create parent DMA tag with 37-bit addressing limit */
	error = bus_dma_tag_create(bus_get_dma_tag(dev),
	    1, 0,			/* alignment, boundary */
	    (1ULL << 37) - 1,		/* lowaddr: 37-bit limit */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter */
	    BUS_SPACE_MAXSIZE,		/* maxsize */
	    BUS_SPACE_UNRESTRICTED,	/* nsegments */
	    BUS_SPACE_MAXSIZE,		/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc */
	    &sc->sc_dma_tag);
	if (error != 0) {
		device_printf(dev, "cannot create DMA tag\n");
		goto fail;
	}

	/*
	 * Enable bus master on function 0 (NVMe) -- T2 quirk.
	 * Must be done before firmware handshake; T2 rejects DMA
	 * on function 1 unless function 0 is bus master.
	 */
	apple_bce_enable_pci0_busmaster(dev);

	/* Initialize and start timestamp keepalive */
	apple_bce_timestamp_init(sc);
	apple_bce_timestamp_start(sc, 1);

	/* Firmware handshake */
	error = apple_bce_fw_handshake(sc);
	if (error != 0)
		goto fail;

	/* Setup command queues */
	error = apple_bce_setup_cmd_queues(sc);
	if (error != 0)
		goto fail;

	device_printf(dev, "Apple T2 BCE initialized\n");
	return (0);

fail:
	apple_bce_detach(dev);
	return (error);
}

/*
 * PCI detach.
 */
static int
apple_bce_detach(device_t dev)
{
	struct apple_bce_softc *sc = device_get_softc(dev);

	/* 1. Stop timestamp */
	if (sc->sc_bar4 != NULL && mtx_initialized(&sc->sc_timestamp_lock))
		apple_bce_timestamp_stop(sc);
	else
		callout_drain(&sc->sc_timestamp_co);

	/* 2. Tear down IRQs first -- no more interrupts after this */
	if (sc->sc_irq_dma_cookie != NULL) {
		bus_teardown_intr(dev, sc->sc_irq_dma, sc->sc_irq_dma_cookie);
		sc->sc_irq_dma_cookie = NULL;
	}
	if (sc->sc_irq_mbox_cookie != NULL) {
		bus_teardown_intr(dev, sc->sc_irq_mbox,
		    sc->sc_irq_mbox_cookie);
		sc->sc_irq_mbox_cookie = NULL;
	}

	/* 3. Free command queues (safe now -- no IRQs can fire) */
	if (sc->sc_cmd_cmdq != NULL) {
		struct bce_queue_sq *cmd_sq;

		cmd_sq = sc->sc_cmd_cmdq->sq;
		bce_free_cmdq(sc->sc_cmd_cmdq);
		sc->sc_cmd_cmdq = NULL;
		bce_free_sq(sc, cmd_sq);
	}
	if (sc->sc_cmd_cq != NULL) {
		bce_free_cq(sc, sc->sc_cmd_cq);
		sc->sc_cmd_cq = NULL;
	}

	/* 4. Clear queue registries */
	if (mtx_initialized(&sc->sc_queues_lock)) {
		mtx_lock(&sc->sc_queues_lock);
		memset(sc->sc_queues, 0, sizeof(sc->sc_queues));
		memset(sc->sc_cq_list, 0, sizeof(sc->sc_cq_list));
		memset(sc->sc_int_sq_list, 0, sizeof(sc->sc_int_sq_list));
		mtx_unlock(&sc->sc_queues_lock);
	}

	/* 5. Destroy mailbox */
	bce_mailbox_destroy(&sc->sc_mbox);

	/* 6. Release DMA tag */
	if (sc->sc_dma_tag != NULL) {
		bus_dma_tag_destroy(sc->sc_dma_tag);
		sc->sc_dma_tag = NULL;
	}

	/* 7. Release IRQ resources */
	if (sc->sc_irq_dma != NULL) {
		bus_release_resource(dev, SYS_RES_IRQ, sc->sc_irq_rid_dma,
		    sc->sc_irq_dma);
		sc->sc_irq_dma = NULL;
	}
	if (sc->sc_irq_mbox != NULL) {
		bus_release_resource(dev, SYS_RES_IRQ, sc->sc_irq_rid_mbox,
		    sc->sc_irq_mbox);
		sc->sc_irq_mbox = NULL;
	}
	if (sc->sc_msi_count > 0) {
		pci_release_msi(dev);
		sc->sc_msi_count = 0;
	}

	/* 8. Release BARs */
	if (sc->sc_bar4 != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, sc->sc_bar4_rid,
		    sc->sc_bar4);
		sc->sc_bar4 = NULL;
	}
	if (sc->sc_bar2 != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, sc->sc_bar2_rid,
		    sc->sc_bar2);
		sc->sc_bar2 = NULL;
	}

	if (mtx_initialized(&sc->sc_timestamp_lock))
		mtx_destroy(&sc->sc_timestamp_lock);
	if (mtx_initialized(&sc->sc_queues_lock))
		mtx_destroy(&sc->sc_queues_lock);

	return (0);
}

static device_method_t apple_bce_methods[] = {
	DEVMETHOD(device_probe,		apple_bce_probe),
	DEVMETHOD(device_attach,	apple_bce_attach),
	DEVMETHOD(device_detach,	apple_bce_detach),
	DEVMETHOD_END
};

static driver_t apple_bce_driver = {
	"apple_bce",
	apple_bce_methods,
	sizeof(struct apple_bce_softc)
};

DRIVER_MODULE(apple_bce, pci, apple_bce_driver, NULL, NULL);
MODULE_DEPEND(apple_bce, pci, 1, 1, 1);
static const struct {
	uint16_t	vendor;
	uint16_t	device;
} apple_bce_pnp[] = {
	{ BCE_PCI_VENDOR_APPLE, BCE_PCI_DEVICE_T2 },
};

MODULE_PNP_INFO("U16:vendor;U16:device", pci, apple_bce, apple_bce_pnp,
    nitems(apple_bce_pnp));
