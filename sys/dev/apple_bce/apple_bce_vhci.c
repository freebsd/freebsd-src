/*-
 * Copyright (c) 2026 Abdelkader Boudih <freebsd@seuros.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Apple T2 BCE Virtual USB Host Controller Interface (VHCI).
 */

#ifdef USB_GLOBAL_INCLUDE_FILE
#include USB_GLOBAL_INCLUDE_FILE
#else
#include <sys/stdint.h>
#include <sys/stddef.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/sysctl.h>
#include <sys/sx.h>
#include <sys/unistd.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/priv.h>
#endif

#include <sys/sema.h>
#include <sys/taskqueue.h>
#include <sys/endian.h>
#include <machine/bus.h>
#include <machine/atomic.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usb_core.h>
#include <dev/usb/usb_busdma.h>
#include <dev/usb/usb_process.h>
#include <dev/usb/usb_transfer.h>
#include <dev/usb/usb_device.h>
#include <dev/usb/usb_hub.h>
#include <dev/usb/usb_util.h>
#include <dev/usb/usb_controller.h>
#include <dev/usb/usb_bus.h>

#include "apple_bce.h"
#include "apple_bce_queue.h"
#include "apple_bce_vhci.h"

/*
 * VHCI softc, defined here because it depends on USB headers.
 */
struct bce_vhci_softc {
	struct usb_bus		sc_bus;		/* Must be first */
	struct usb_device	*sc_devices[BCE_VHCI_MAX_DEVICES];
	struct apple_bce_softc	*sc_bce;
	device_t		sc_dev;

	/* Controller state */
	uint32_t		sc_port_mask;
	uint8_t			sc_port_count;
	int			sc_started;

	/* Port state */
	uint32_t		sc_port_status[BCE_VHCI_MAX_PORTS];
	uint32_t		sc_port_change[BCE_VHCI_MAX_PORTS];
	uint8_t			sc_port_power[BCE_VHCI_MAX_PORTS];

	/* Hub scratch buffer (for descriptor/status responses) */
	uint8_t			sc_hub_idata[32];

	/* Message queues (host -> device) */
	struct bce_vhci_msg_queue msg_commands;
	struct bce_vhci_msg_queue msg_system;
	struct bce_vhci_msg_queue msg_isochronous;
	struct bce_vhci_msg_queue msg_interrupt;
	struct bce_vhci_msg_queue msg_asynchronous;

	/* Event queues (device -> host), share a single CQ */
	struct bce_queue_cq	*ev_cq;
	struct bce_vhci_evt_queue ev_commands;
	struct bce_vhci_evt_queue ev_system;
	struct bce_vhci_evt_queue ev_isochronous;
	struct bce_vhci_evt_queue ev_interrupt;
	struct bce_vhci_evt_queue ev_asynchronous;

	/* Command execution (synchronous, wraps msg_commands) */
	struct bce_vhci_cmd_queue cmd;

	/* Queue ID bitmap (256 bits = BCE_MAX_QUEUE_COUNT) */
	uint32_t		sc_qid_bitmap[8];

	/* Per-device state (indexed by firmware device ID) */
	struct bce_vhci_device	sc_devs[BCE_VHCI_MAX_DEVICES];
	uint8_t			sc_port_to_dev[BCE_VHCI_MAX_PORTS];

	/* Deferred firmware event processing (from ev_commands) */
	struct task		sc_fwevt_task;
	volatile int		sc_detaching;	/* Teardown guard */

	/*
	 * Firmware event mailbox: ISR copies events here, task processes.
	 * Protected by sc_fwevt_lock.  Ring of BCE_VHCI_EVT_PENDING entries.
	 */
	struct mtx		sc_fwevt_lock;
#define	BCE_VHCI_FWEVT_RING	(BCE_VHCI_EVT_PENDING + 1)
	struct {
		struct bce_vhci_message	msg;
		int			needs_reply;
	}			sc_fwevt_ring[BCE_VHCI_FWEVT_RING];
	uint32_t		sc_fwevt_prod;
	uint32_t		sc_fwevt_cons;

	/* Spinlock for msg_asynchronous writes (ISR + taskqueue context) */
	struct mtx		sc_async_lock;

	/* Deferred endpoint reset (cannot sleep in pipe_start) */
	struct task		sc_reset_task;

	/* Deferred endpoint create (cannot sleep in pipe_start) */
	struct task		sc_create_task;

	/* Deferred port status change (ISR cannot call cmd_execute) */
	struct task		sc_port_chg_task;
	volatile uint32_t	sc_port_chg_mask;
};

/* Command timeout (ticks) */
#define BCE_VHCI_CMD_TIMEOUT_SHORT	(hz * 2)
#define BCE_VHCI_CMD_TIMEOUT_LONG	(hz * 30)

static usb_handle_req_t bce_vhci_roothub_exec;
static void bce_vhci_endpoint_init(struct usb_device *udev,
    struct usb_endpoint_descriptor *edesc, struct usb_endpoint *ep);
static void bce_vhci_xfer_setup(struct usb_setup_params *parm);
static void bce_vhci_xfer_unsetup(struct usb_xfer *xfer);
static void bce_vhci_get_dma_delay(struct usb_device *udev, uint32_t *pus);

static void bce_vhci_pipe_open(struct usb_xfer *xfer);
static void bce_vhci_pipe_close(struct usb_xfer *xfer);
static void bce_vhci_pipe_enter(struct usb_xfer *xfer);
static void bce_vhci_pipe_start(struct usb_xfer *xfer);

static int bce_vhci_probe(device_t dev);
static int bce_vhci_attach_dev(device_t dev);
static int bce_vhci_detach_dev(device_t dev);

static int bce_vhci_alloc_qid(struct bce_vhci_softc *vhci);
static void bce_vhci_free_qid(struct bce_vhci_softc *vhci, int qid);
static int bce_vhci_create_queues(struct bce_vhci_softc *vhci);
static void bce_vhci_destroy_queues(struct bce_vhci_softc *vhci);
static int bce_vhci_start_controller(struct bce_vhci_softc *vhci);
static void bce_vhci_msg_queue_completion(struct bce_queue_sq *sq);
static void bce_vhci_ev_cmd_completion(struct bce_queue_sq *sq);
static void bce_vhci_ev_system_completion(struct bce_queue_sq *sq);
static void bce_vhci_ev_generic_completion(struct bce_queue_sq *sq);
static void bce_vhci_cmd_deliver_completion(struct bce_vhci_softc *vhci,
    struct bce_vhci_message *msg);
static void bce_vhci_handle_port_status_change(struct bce_vhci_softc *vhci,
    struct bce_vhci_message *msg);
static void bce_vhci_evt_queue_submit_pending(struct bce_vhci_softc *vhci,
    struct bce_vhci_evt_queue *eq, uint32_t count);

static int bce_vhci_device_create(struct bce_vhci_softc *vhci, uint8_t port);
static void bce_vhci_device_destroy(struct bce_vhci_softc *vhci, uint8_t port);
static int bce_vhci_endpoint_create(struct bce_vhci_softc *vhci,
    struct bce_vhci_device *dev, uint8_t ep_addr,
    struct usb_endpoint_descriptor *edesc);
static void bce_vhci_endpoint_destroy(struct bce_vhci_softc *vhci,
    struct bce_vhci_device *dev, uint8_t ep_addr);
static void bce_vhci_handle_transfer_request(struct bce_vhci_softc *vhci,
    struct bce_vhci_message *msg);
static void bce_vhci_complete_ctrl_locked(struct bce_vhci_softc *vhci,
    struct bce_vhci_transfer_queue *tq, struct bce_vhci_message *msg);
static void bce_vhci_handle_ctrl_status(struct bce_vhci_softc *vhci,
    struct bce_vhci_message *msg);
static uint16_t bce_vhci_handle_endpoint_req_state(struct bce_vhci_softc *vhci,
    struct bce_vhci_message *msg);
static uint16_t bce_vhci_handle_endpoint_set_state(struct bce_vhci_softc *vhci,
    struct bce_vhci_message *msg);
static void bce_vhci_fwevt_task(void *arg, int pending);
static void bce_vhci_send_fw_event_reply(struct bce_vhci_softc *vhci,
    struct bce_vhci_message *req, uint16_t status);
static void bce_vhci_tq_completion(struct bce_queue_sq *sq);
static void bce_vhci_reset_task(void *arg, int pending);
static void bce_vhci_create_task(void *arg, int pending);
static void bce_vhci_port_chg_task(void *arg, int pending);
static int bce_vhci_cmd_execute(struct bce_vhci_softc *vhci,
    struct bce_vhci_message *req, struct bce_vhci_message *reply,
    int timeout_ticks);

/*
 * Convert USB endpoint address to tq[] index.
 * ep0 (0x00) maps to index 0.  For other endpoints, IN and OUT get
 * separate slots: OUT 0x01 -> 1, IN 0x81 -> 2, OUT 0x02 -> 3, etc.
 * Maximum index is 30 (ep 0x8F), fits in BCE_VHCI_MAX_ENDPOINTS (32).
 */
static inline uint8_t
bce_vhci_ep_index(uint8_t ep_addr)
{
	uint8_t num;

	num = ep_addr & 0x0F;
	if (num == 0)
		return (0);
	return (num * 2 - ((ep_addr & 0x80) ? 0 : 1));
}

static const struct usb_bus_methods bce_vhci_bus_methods = {
	.roothub_exec = bce_vhci_roothub_exec,
	.endpoint_init = bce_vhci_endpoint_init,
	.xfer_setup = bce_vhci_xfer_setup,
	.xfer_unsetup = bce_vhci_xfer_unsetup,
	.get_dma_delay = bce_vhci_get_dma_delay,
};

/*
 * Generic pipe methods (all transfer types for now).
 */
static const struct usb_pipe_methods bce_vhci_pipe_methods = {
	.open = bce_vhci_pipe_open,
	.close = bce_vhci_pipe_close,
	.enter = bce_vhci_pipe_enter,
	.start = bce_vhci_pipe_start,
};

/*
 * Device methods.
 */
static device_method_t bce_vhci_methods[] = {
	DEVMETHOD(device_probe,		bce_vhci_probe),
	DEVMETHOD(device_attach,	bce_vhci_attach_dev),
	DEVMETHOD(device_detach,	bce_vhci_detach_dev),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	/* Bus interface for usbus child */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD_END
};

static driver_t bce_vhci_driver = {
	.name = "bce_vhci",
	.methods = bce_vhci_methods,
	.size = sizeof(struct bce_vhci_softc),
};

DRIVER_MODULE(bce_vhci, apple_bce, bce_vhci_driver, 0, 0);
MODULE_DEPEND(bce_vhci, usb, 1, 1, 1);

/*
 * Hub descriptor (USB 2.0 hub with per-port power switching)
 */

/* Hub descriptor built dynamically in roothub_exec (port count varies) */

/* Device descriptor for the root hub */
static const struct usb_device_descriptor bce_vhci_devd = {
	.bLength = sizeof(struct usb_device_descriptor),
	.bDescriptorType = UDESC_DEVICE,
	.bcdUSB = { 0x00, 0x02 },	/* USB 2.0 */
	.bDeviceClass = UDCLASS_HUB,
	.bDeviceSubClass = UDSUBCLASS_HUB,
	.bDeviceProtocol = UDPROTO_HSHUBSTT,
	.bMaxPacketSize = 64,
	.idVendor = { 0x6b, 0x10 },	/* Apple 0x106b */
	.idProduct = { 0x01, 0x18 },	/* T2 BCE 0x1801 */
	.bcdDevice = { 0x00, 0x01 },	/* 1.00 */
	.iManufacturer = 1,
	.iProduct = 2,
	.bNumConfigurations = 1,
};

static const struct usb_device_qualifier bce_vhci_odevd = {
	.bLength = sizeof(struct usb_device_qualifier),
	.bDescriptorType = UDESC_DEVICE_QUALIFIER,
	.bcdUSB = { 0x00, 0x02 },
	.bDeviceClass = UDCLASS_HUB,
	.bDeviceSubClass = UDSUBCLASS_HUB,
	.bDeviceProtocol = UDPROTO_HSHUBSTT,
	.bMaxPacketSize0 = 0,
	.bNumConfigurations = 0,
};

/* Configuration descriptor + interface + endpoint */
/* 9 + 9 + 7 = 25 bytes */
static const uint8_t bce_vhci_confd[] = {
	/* Configuration descriptor */
	0x09, 0x02,			/* bLength, bDescriptorType */
	0x19, 0x00,			/* wTotalLength = 25 */
	0x01, 0x01, 0x00, 0xC0, 0x00,	/* nIntf, cfgVal, iCfg */
	/* Interface descriptor */
	0x09, 0x04,			/* bLength, bDescriptorType */
	0x00, 0x00, 0x01, 0x09, 0x00, 0x01, 0x00,
	/* Endpoint descriptor (interrupt IN ep1) */
	0x07, 0x05,			/* bLength, bDescriptorType */
	0x81, 0x03, 0x08, 0x00, 0xFF,	/* addr, attr, maxPkt, interval */
};

struct bce_vhci_dma_cb_arg {
	bus_addr_t	addr;
	int		error;
};

static void
bce_vhci_dma_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct bce_vhci_dma_cb_arg *cb = arg;

	cb->error = error;
	if (error == 0)
		cb->addr = segs[0].ds_addr;
}

/*
 * Allocate a CQ + SQ pair and DMA message buffer for a host->device queue.
 * Register it with firmware under the given name.
 */
static int
bce_vhci_msg_queue_create(struct bce_vhci_softc *vhci,
    struct bce_vhci_msg_queue *mq, const char *name, int cq_qid, int sq_qid,
    bce_sq_completion_fn compl_fn, void *compl_arg)
{
	struct apple_bce_softc *sc = vhci->sc_bce;
	struct bce_vhci_dma_cb_arg cb;
	struct bce_queue_memcfg cfg;
	uint32_t el_count = BCE_VHCI_MSG_QUEUE_EL;
	uint32_t status;
	int error, i;

	memset(mq, 0, sizeof(*mq));
	mq->el_count = el_count;

	/* Allocate CQ */
	mq->cq = bce_alloc_cq(sc, cq_qid, el_count);
	if (mq->cq == NULL)
		return (ENOMEM);

	/* Register CQ with firmware via command path */
	bce_get_cq_memcfg(mq->cq, &cfg);
	/* CQ interrupt vector = 4 (DMA MSI) */
	cfg.vector_or_cq = 4;
	status = bce_cmd_register_queue(sc->sc_cmd_cmdq, sc, &cfg, NULL, 0);
	if (status != 0) {
		device_printf(vhci->sc_dev,
		    "failed to register CQ %d for %s: %u\n",
		    cq_qid, name, status);
		error = EIO;
		goto fail_cq;
	}

	/* Register CQ in parent's dispatch tables */
	mtx_lock(&sc->sc_queues_lock);
	sc->sc_queues[cq_qid] = mq->cq;
	for (i = 0; i < BCE_MAX_CQ_COUNT; i++) {
		if (sc->sc_cq_list[i] == NULL) {
			sc->sc_cq_list[i] = mq->cq;
			break;
		}
	}
	if (i == BCE_MAX_CQ_COUNT) {
		sc->sc_queues[cq_qid] = NULL;
		mtx_unlock(&sc->sc_queues_lock);
		device_printf(vhci->sc_dev,
		    "CQ list full for %s\n", name);
		error = ENOSPC;
		goto fail_cq_reg;
	}
	mtx_unlock(&sc->sc_queues_lock);

	/* Allocate SQ (element size = bce_qe_submission = 32 bytes) */
	mq->sq = bce_alloc_sq(sc, sq_qid,
	    sizeof(struct bce_qe_submission), el_count,
	    compl_fn, compl_arg);
	if (mq->sq == NULL) {
		error = ENOMEM;
		goto fail_cq_reg;
	}

	/* Register SQ with firmware under the given name */
	bce_get_sq_memcfg(mq->sq, mq->cq, &cfg);
	status = bce_cmd_register_queue(sc->sc_cmd_cmdq, sc, &cfg, name, 1);
	if (status != 0) {
		device_printf(vhci->sc_dev,
		    "failed to register SQ %d (%s): %u\n",
		    sq_qid, name, status);
		error = EIO;
		goto fail_sq;
	}

	/* Register SQ in parent's dispatch tables */
	mtx_lock(&sc->sc_queues_lock);
	sc->sc_queues[sq_qid] = mq->sq;
	sc->sc_int_sq_list[sq_qid] = mq->sq;
	mtx_unlock(&sc->sc_queues_lock);

	/* Allocate DMA-coherent message buffer */
	error = bus_dma_tag_create(sc->sc_dma_tag,
	    4, 0,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR,
	    NULL, NULL,
	    el_count * sizeof(struct bce_vhci_message), 1,
	    el_count * sizeof(struct bce_vhci_message),
	    BUS_DMA_WAITOK,
	    NULL, NULL,
	    &mq->dma_tag);
	if (error != 0)
		goto fail_sq_reg;

	error = bus_dmamem_alloc(mq->dma_tag, (void **)&mq->data,
	    BUS_DMA_WAITOK | BUS_DMA_ZERO | BUS_DMA_COHERENT,
	    &mq->dma_map);
	if (error != 0)
		goto fail_dma_tag;

	error = bus_dmamap_load(mq->dma_tag, mq->dma_map, mq->data,
	    el_count * sizeof(struct bce_vhci_message),
	    bce_vhci_dma_cb, &cb, BUS_DMA_WAITOK);
	if (error != 0 || cb.error != 0) {
		error = error != 0 ? error : cb.error;
		goto fail_dma_mem;
	}
	mq->dma_addr = cb.addr;

	return (0);

fail_dma_mem:
	bus_dmamem_free(mq->dma_tag, mq->data, mq->dma_map);
fail_dma_tag:
	bus_dma_tag_destroy(mq->dma_tag);
fail_sq_reg:
	bce_cmd_unregister_queue(sc->sc_cmd_cmdq, sc, sq_qid);
	mtx_lock(&sc->sc_queues_lock);
	sc->sc_queues[sq_qid] = NULL;
	sc->sc_int_sq_list[sq_qid] = NULL;
	mtx_unlock(&sc->sc_queues_lock);
fail_sq:
	bce_free_sq(sc, mq->sq);
	mq->sq = NULL;
fail_cq_reg:
	bce_cmd_unregister_queue(sc->sc_cmd_cmdq, sc, cq_qid);
	mtx_lock(&sc->sc_queues_lock);
	sc->sc_queues[cq_qid] = NULL;
	for (i = 0; i < BCE_MAX_CQ_COUNT; i++) {
		if (sc->sc_cq_list[i] == mq->cq) {
			sc->sc_cq_list[i] = NULL;
			break;
		}
	}
	mtx_unlock(&sc->sc_queues_lock);
fail_cq:
	bce_free_cq(sc, mq->cq);
	mq->cq = NULL;
	return (error);
}

static void
bce_vhci_msg_queue_destroy(struct bce_vhci_softc *vhci,
    struct bce_vhci_msg_queue *mq)
{
	struct apple_bce_softc *sc = vhci->sc_bce;
	int i;

	if (mq->cq == NULL)
		return;

	/*
	 * Unregister and free SQ before releasing the DMA buffer it
	 * references
	 */
	if (mq->sq != NULL) {
		bce_cmd_unregister_queue(sc->sc_cmd_cmdq, sc, mq->sq->qid);
		mtx_lock(&sc->sc_queues_lock);
		sc->sc_queues[mq->sq->qid] = NULL;
		sc->sc_int_sq_list[mq->sq->qid] = NULL;
		mtx_unlock(&sc->sc_queues_lock);
		bce_free_sq(sc, mq->sq);
		mq->sq = NULL;
	}

	/* Free DMA message buffer */
	if (mq->data != NULL) {
		bus_dmamap_unload(mq->dma_tag, mq->dma_map);
		bus_dmamem_free(mq->dma_tag, mq->data, mq->dma_map);
		bus_dma_tag_destroy(mq->dma_tag);
		mq->data = NULL;
	}

	/* Unregister and free CQ */
	bce_cmd_unregister_queue(sc->sc_cmd_cmdq, sc, mq->cq->qid);
	mtx_lock(&sc->sc_queues_lock);
	sc->sc_queues[mq->cq->qid] = NULL;
	for (i = 0; i < BCE_MAX_CQ_COUNT; i++) {
		if (sc->sc_cq_list[i] == mq->cq) {
			sc->sc_cq_list[i] = NULL;
			break;
		}
	}
	mtx_unlock(&sc->sc_queues_lock);
	bce_free_cq(sc, mq->cq);
	mq->cq = NULL;
}

/*
 * Write a message to a host->device queue.
 * Caller must have reserved a submission slot.
 */
static void
bce_vhci_msg_queue_write(struct bce_vhci_softc *vhci,
    struct bce_vhci_msg_queue *mq, struct bce_vhci_message *msg)
{
	struct bce_qe_submission *s;
	uint32_t sidx;

	sidx = mq->sq->tail;
	s = bce_next_submission(mq->sq);

	/* Copy message into DMA buffer slot and sync for device access */
	mq->data[sidx] = *msg;
	bus_dmamap_sync(mq->dma_tag, mq->dma_map, BUS_DMASYNC_PREWRITE);

	/* Fill SQ entry pointing to the DMA buffer slot */
	s->length = sizeof(struct bce_vhci_message);
	s->addr = mq->dma_addr +
	    sidx * sizeof(struct bce_vhci_message);
	s->segl_addr = 0;
	s->segl_length = 0;

	bce_submit_to_device(vhci->sc_bce, mq->sq);
}

/*
 * Message queue completion: consume completions and free slots.
 */
static void
bce_vhci_msg_queue_completion(struct bce_queue_sq *sq)
{
	struct bce_vhci_msg_queue *mq = sq->userdata;

	while (sq->completion_cidx != sq->completion_tail) {
		sq->completion_cidx =
		    (sq->completion_cidx + 1) % sq->el_count;
		bce_notify_submission_complete(sq);
	}
	bus_dmamap_sync(mq->dma_tag, mq->dma_map, BUS_DMASYNC_POSTWRITE);
}

/*
 * Allocate an SQ (paired with the shared ev_cq) and DMA buffer for a
 * device->host event queue. Register with firmware and pre-submit
 * receive buffers.
 */
static int
bce_vhci_evt_queue_create(struct bce_vhci_softc *vhci,
    struct bce_vhci_evt_queue *eq, const char *name, int sq_qid,
    bce_sq_completion_fn compl_fn)
{
	struct apple_bce_softc *sc = vhci->sc_bce;
	struct bce_vhci_dma_cb_arg cb;
	struct bce_queue_memcfg cfg;
	uint32_t el_count = BCE_VHCI_EVT_QUEUE_EL;
	uint32_t status;
	int error;

	memset(eq, 0, sizeof(*eq));
	eq->el_count = el_count;
	eq->userdata = vhci;

	/* Allocate SQ (shared CQ = vhci->ev_cq) */
	eq->sq = bce_alloc_sq(sc, sq_qid,
	    sizeof(struct bce_qe_submission), el_count,
	    compl_fn, eq);
	if (eq->sq == NULL)
		return (ENOMEM);

	/* Register SQ with firmware (direction = from device = 0) */
	bce_get_sq_memcfg(eq->sq, vhci->ev_cq, &cfg);
	status = bce_cmd_register_queue(sc->sc_cmd_cmdq, sc, &cfg, name, 0);
	if (status != 0) {
		device_printf(vhci->sc_dev,
		    "failed to register event SQ %d (%s): %u\n",
		    sq_qid, name, status);
		error = EIO;
		goto fail_sq;
	}

	/* Register SQ in dispatch tables */
	mtx_lock(&sc->sc_queues_lock);
	sc->sc_queues[sq_qid] = eq->sq;
	sc->sc_int_sq_list[sq_qid] = eq->sq;
	mtx_unlock(&sc->sc_queues_lock);

	/* Allocate DMA-coherent receive buffer */
	error = bus_dma_tag_create(sc->sc_dma_tag,
	    4, 0,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR,
	    NULL, NULL,
	    el_count * sizeof(struct bce_vhci_message), 1,
	    el_count * sizeof(struct bce_vhci_message),
	    BUS_DMA_WAITOK,
	    NULL, NULL,
	    &eq->dma_tag);
	if (error != 0)
		goto fail_sq_reg;

	error = bus_dmamem_alloc(eq->dma_tag, (void **)&eq->data,
	    BUS_DMA_WAITOK | BUS_DMA_ZERO | BUS_DMA_COHERENT,
	    &eq->dma_map);
	if (error != 0)
		goto fail_dma_tag;

	error = bus_dmamap_load(eq->dma_tag, eq->dma_map, eq->data,
	    el_count * sizeof(struct bce_vhci_message),
	    bce_vhci_dma_cb, &cb, BUS_DMA_WAITOK);
	if (error != 0 || cb.error != 0) {
		error = error != 0 ? error : cb.error;
		goto fail_dma_mem;
	}
	eq->dma_addr = cb.addr;

	/* Pre-submit receive buffers */
	bce_vhci_evt_queue_submit_pending(vhci, eq, BCE_VHCI_EVT_PENDING);

	return (0);

fail_dma_mem:
	bus_dmamem_free(eq->dma_tag, eq->data, eq->dma_map);
fail_dma_tag:
	bus_dma_tag_destroy(eq->dma_tag);
fail_sq_reg:
	bce_cmd_unregister_queue(sc->sc_cmd_cmdq, sc, sq_qid);
	mtx_lock(&sc->sc_queues_lock);
	sc->sc_queues[sq_qid] = NULL;
	sc->sc_int_sq_list[sq_qid] = NULL;
	mtx_unlock(&sc->sc_queues_lock);
fail_sq:
	bce_free_sq(sc, eq->sq);
	eq->sq = NULL;
	return (error);
}

static void
bce_vhci_evt_queue_destroy(struct bce_vhci_softc *vhci,
    struct bce_vhci_evt_queue *eq)
{
	struct apple_bce_softc *sc = vhci->sc_bce;

	if (eq->sq == NULL)
		return;

	/* Unregister SQ from dispatch tables FIRST to stop IRQ callbacks */
	bce_cmd_flush_queue(sc->sc_cmd_cmdq, sc, eq->sq->qid);
	bce_cmd_unregister_queue(sc->sc_cmd_cmdq, sc, eq->sq->qid);
	mtx_lock(&sc->sc_queues_lock);
	sc->sc_queues[eq->sq->qid] = NULL;
	sc->sc_int_sq_list[eq->sq->qid] = NULL;
	mtx_unlock(&sc->sc_queues_lock);

	/* Now safe to free DMA buffer; no IRQ can reference it */
	if (eq->data != NULL) {
		bus_dmamap_unload(eq->dma_tag, eq->dma_map);
		bus_dmamem_free(eq->dma_tag, eq->data, eq->dma_map);
		bus_dma_tag_destroy(eq->dma_tag);
		eq->data = NULL;
	}
	bce_free_sq(sc, eq->sq);
	eq->sq = NULL;
}

/*
 * Submit empty receive buffers to an event queue so firmware can
 * write messages into them.
 */
static void
bce_vhci_evt_queue_submit_pending(struct bce_vhci_softc *vhci,
    struct bce_vhci_evt_queue *eq, uint32_t count)
{
	struct bce_qe_submission *s;
	uint32_t idx;

	bus_dmamap_sync(eq->dma_tag, eq->dma_map, BUS_DMASYNC_PREREAD);

	while (count-- > 0) {
		if (bce_reserve_submission(eq->sq) != 0) {
			device_printf(vhci->sc_dev,
			    "cannot reserve event submission\n");
			break;
		}
		idx = eq->sq->tail;
		s = bce_next_submission(eq->sq);
		s->length = sizeof(struct bce_vhci_message);
		s->addr = eq->dma_addr +
		    idx * sizeof(struct bce_vhci_message);
		s->segl_addr = 0;
		s->segl_length = 0;
	}
	bce_submit_to_device(vhci->sc_bce, eq->sq);
}

/*
 * Enqueue a firmware event into sc_fwevt_ring for deferred processing.
 * Called from ISR context; returns 0 on success, -1 if ring is full.
 */
static int
bce_vhci_fwevt_enqueue(struct bce_vhci_softc *vhci,
    struct bce_vhci_message *msg, int needs_reply)
{
	uint32_t next_prod;

	mtx_lock_spin(&vhci->sc_fwevt_lock);
	next_prod = (vhci->sc_fwevt_prod + 1) % BCE_VHCI_FWEVT_RING;
	if (next_prod == vhci->sc_fwevt_cons) {
		mtx_unlock_spin(&vhci->sc_fwevt_lock);
		device_printf(vhci->sc_dev,
		    "fwevt ring full, dropping 0x%04x\n", msg->cmd);
		return (-1);
	}
	vhci->sc_fwevt_ring[vhci->sc_fwevt_prod].msg = *msg;
	vhci->sc_fwevt_ring[vhci->sc_fwevt_prod].needs_reply = needs_reply;
	vhci->sc_fwevt_prod = next_prod;
	mtx_unlock_spin(&vhci->sc_fwevt_lock);

	if (vhci->sc_detaching == 0)
		taskqueue_enqueue(taskqueue_thread, &vhci->sc_fwevt_task);
	return (0);
}

/*
 * Generic event queue completion: read messages and resubmit buffers.
 * Used for system, isochronous, interrupt, and asynchronous event queues.
 */
static void
bce_vhci_ev_generic_completion(struct bce_queue_sq *sq)
{
	struct bce_vhci_evt_queue *eq = sq->userdata;
	struct bce_vhci_softc *vhci = eq->userdata;
	struct bce_vhci_message *msg;
	uint32_t cnt = 0;

	bus_dmamap_sync(eq->dma_tag, eq->dma_map, BUS_DMASYNC_POSTREAD);

	while (sq->completion_cidx != sq->completion_tail) {
		struct bce_sq_completion_data *cd;

		cd = &sq->completion_data[sq->completion_cidx];
		if (cd->status == BCE_COMP_ABORTED) {
			sq->completion_cidx =
			    (sq->completion_cidx + 1) % sq->el_count;
			bce_notify_submission_complete(sq);
			cnt++;
			continue;
		}

		msg = &eq->data[sq->head];
		/*
		 * Route events to appropriate handlers.
		 * Strip 0x4000 flag; firmware uses it as a
		 * variant marker.
		 */
		if (msg->cmd & BCE_VHCI_CMD_REPLY_FLAG)
			bce_vhci_cmd_deliver_completion(vhci, msg);
		else {
			uint16_t base_cmd = msg->cmd &
			    ~BCE_VHCI_CMD_CANCEL_FLAG;

			if (base_cmd == BCE_VHCI_CMD_PORT_STATUS_CHANGE)
				bce_vhci_handle_port_status_change(vhci,
				    msg);
			else if (base_cmd == BCE_VHCI_CMD_TRANSFER_REQUEST)
				bce_vhci_handle_transfer_request(vhci, msg);
			else if (base_cmd ==
			    BCE_VHCI_CMD_CTRL_TRANSFER_STATUS)
				bce_vhci_handle_ctrl_status(vhci, msg);
			else if (base_cmd ==
			    BCE_VHCI_CMD_ENDPOINT_REQ_STATE ||
			    base_cmd ==
			    BCE_VHCI_CMD_ENDPOINT_SET_STATE)
				bce_vhci_fwevt_enqueue(vhci, msg, 0);
		}

		sq->completion_cidx =
		    (sq->completion_cidx + 1) % sq->el_count;
		bce_notify_submission_complete(sq);
		cnt++;
	}

	if (cnt > 0)
		bce_vhci_evt_queue_submit_pending(vhci, eq, cnt);
}

/*
 * Event queue completion for the firmware command channel (ev_commands).
 *
 * This ISR callback is the sole consumer of the ev_commands SQ ring.
 * Command replies are delivered inline (semaphore post, ISR-safe).
 * Firmware events are copied into sc_fwevt_ring and deferred to
 * sc_fwevt_task which handles them in taskqueue_thread context
 * (needed because ENDP_PAUSED handling calls bce_cmd_flush_queue).
 */
static void
bce_vhci_ev_cmd_completion(struct bce_queue_sq *sq)
{
	struct bce_vhci_evt_queue *eq = sq->userdata;
	struct bce_vhci_softc *vhci = eq->userdata;
	struct bce_vhci_message *msg;
	uint32_t cnt = 0;

	bus_dmamap_sync(eq->dma_tag, eq->dma_map, BUS_DMASYNC_POSTREAD);

	while (sq->completion_cidx != sq->completion_tail) {
		struct bce_sq_completion_data *cd;

		cd = &sq->completion_data[sq->completion_cidx];
		if (cd->status == BCE_COMP_ABORTED) {
			sq->completion_cidx =
			    (sq->completion_cidx + 1) % sq->el_count;
			bce_notify_submission_complete(sq);
			cnt++;
			continue;
		}

		msg = &eq->data[sq->head];

		if (msg->cmd & BCE_VHCI_CMD_REPLY_FLAG) {
			/* Command reply: deliver inline (semaphore post) */
			bce_vhci_cmd_deliver_completion(vhci, msg);
		} else {
			/* Firmware event: defer to taskqueue */
			bce_vhci_fwevt_enqueue(vhci, msg, 1);
		}

		sq->completion_cidx =
		    (sq->completion_cidx + 1) % sq->el_count;
		bce_notify_submission_complete(sq);
		cnt++;
	}

	if (cnt > 0) {
		bus_dmamap_sync(eq->dma_tag, eq->dma_map, BUS_DMASYNC_PREREAD);
		bce_vhci_evt_queue_submit_pending(vhci, eq, cnt);
	}

}

/*
 * System event queue completion: handles command replies and
 * port status change notifications.
 */
static void
bce_vhci_ev_system_completion(struct bce_queue_sq *sq)
{

	/* Route through generic handler which checks for both */
	bce_vhci_ev_generic_completion(sq);
}

/*
 * Taskqueue handler for firmware events on ev_commands.
 *
 * Processes ENDPOINT_REQ_STATE / ENDPOINT_SET_STATE events from
 * process context (not ISR).  Implements cancel-pair detection:
 * if two consecutive events are cmd + cmd|0x4000 with same param1,
 * both are consumed with a single ABORT reply.
 *
 * Normal events are handled and replied with SUCCESS on msg_system.
 */
static void
bce_vhci_fwevt_task(void *arg, int pending __unused)
{
	struct bce_vhci_softc *vhci = arg;
	struct bce_vhci_message msg;

	if (vhci->sc_detaching)
		return;

	/*
	 * Process firmware events from the mailbox ring.
	 * The ISR is the sole consumer of the ev_commands SQ ring and
	 * copies events here; we process them in taskqueue context.
	 */
	for (;;) {
		uint16_t result;
		int needs_reply;

		mtx_lock_spin(&vhci->sc_fwevt_lock);
		if (vhci->sc_fwevt_cons == vhci->sc_fwevt_prod) {
			mtx_unlock_spin(&vhci->sc_fwevt_lock);
			break;
		}
		msg = vhci->sc_fwevt_ring[vhci->sc_fwevt_cons].msg;
		needs_reply =
		    vhci->sc_fwevt_ring[vhci->sc_fwevt_cons].needs_reply;
		vhci->sc_fwevt_cons = (vhci->sc_fwevt_cons + 1) %
		    BCE_VHCI_FWEVT_RING;
		mtx_unlock_spin(&vhci->sc_fwevt_lock);

		if (msg.cmd & BCE_VHCI_CMD_CANCEL_FLAG) {
			/* Firmware cancel; reply ABORT */
			result = BCE_VHCI_ABORT;
		} else if (msg.cmd == BCE_VHCI_CMD_ENDPOINT_REQ_STATE)
			result = bce_vhci_handle_endpoint_req_state(vhci, &msg);
		else if (msg.cmd == BCE_VHCI_CMD_ENDPOINT_SET_STATE)
			result = bce_vhci_handle_endpoint_set_state(vhci, &msg);
		else {
			device_printf(vhci->sc_dev,
			    "unhandled fw event: 0x%04x\n", msg.cmd);
			result = BCE_VHCI_BAD_ARGUMENT;
		}
		if (needs_reply)
			bce_vhci_send_fw_event_reply(vhci, &msg, result);
	}
}

/*
 * Deliver a firmware reply to the synchronous command waiter.
 */
static void
bce_vhci_cmd_deliver_completion(struct bce_vhci_softc *vhci,
    struct bce_vhci_message *msg)
{
	struct bce_vhci_cmd_queue *cq = &vhci->cmd;
	int do_post = 0;

	mtx_lock_spin(&cq->lock);
	if (cq->pending != 0) {
		uint16_t base_cmd;

		/*
		 * Accept only replies matching the expected command
		 * (with REPLY_FLAG and optionally CANCEL_FLAG).
		 * Drop stale replies from timed-out commands.
		 */
		base_cmd = msg->cmd & ~(BCE_VHCI_CMD_REPLY_FLAG |
		    BCE_VHCI_CMD_CANCEL_FLAG);
		if (base_cmd == cq->expected_cmd) {
			cq->response = *msg;
			cq->pending = 0;
			do_post = 1;
		}
	}
	mtx_unlock_spin(&cq->lock);

	/*
	 * sema_post uses MTX_DEF internally; must not be called under
	 * MTX_SPIN
	 */
	if (do_post)
		sema_post(&cq->completion);
}

/*
 * Handle port status change event from firmware (ISR context).
 * Cannot call cmd_execute here (sleeps), so defer to taskqueue.
 */
static void
bce_vhci_handle_port_status_change(struct bce_vhci_softc *vhci,
    struct bce_vhci_message *msg)
{
	uint32_t port;

	if (vhci->sc_detaching)
		return;

	port = msg->param1;
	if (port >= vhci->sc_port_count)
		return;

	atomic_set_int(&vhci->sc_port_chg_mask, 1U << port);
	taskqueue_enqueue(taskqueue_thread, &vhci->sc_port_chg_task);
}

/*
 * Deferred port status change handler (taskqueue context, can sleep).
 * Queries firmware for current port status and updates the cache.
 */
static void
bce_vhci_port_chg_task(void *arg, int pending __unused)
{
	struct bce_vhci_softc *vhci = arg;
	struct bce_vhci_message cmd, reply;
	uint32_t mask, port, port_status;
	int error;

	if (vhci->sc_detaching)
		return;

	mask = atomic_readandclear_int(&vhci->sc_port_chg_mask);

	for (port = 0; mask != 0; port++, mask >>= 1) {
		if ((mask & 1) == 0)
			continue;

		device_printf(vhci->sc_dev,
		    "port %u status change\n", port);

		memset(&cmd, 0, sizeof(cmd));
		cmd.cmd = BCE_VHCI_CMD_PORT_STATUS;
		cmd.param1 = port;

		error = bce_vhci_cmd_execute(vhci, &cmd, &reply,
		    BCE_VHCI_CMD_TIMEOUT_SHORT);

		USB_BUS_LOCK(&vhci->sc_bus);
		if (error == 0) {
			port_status = (uint32_t)reply.param2;

			vhci->sc_port_status[port] = 0;
			if (vhci->sc_port_power[port])
				vhci->sc_port_status[port] |=
				    UPS_PORT_POWER;
			if (port_status & BCE_VHCI_PORT_ENABLED)
				vhci->sc_port_status[port] |=
				    UPS_PORT_ENABLED | UPS_HIGH_SPEED;
			if (port_status & BCE_VHCI_PORT_CONNECTED)
				vhci->sc_port_status[port] |=
				    UPS_CURRENT_CONNECT_STATUS;
			if (port_status & BCE_VHCI_PORT_SUSPENDED)
				vhci->sc_port_status[port] |=
				    UPS_SUSPEND;
			if (port_status & BCE_VHCI_PORT_OVERCURRENT)
				vhci->sc_port_status[port] |=
				    UPS_OVERCURRENT_INDICATOR;
		}
		vhci->sc_port_change[port] |= UPS_C_CONNECT_STATUS;
		USB_BUS_UNLOCK(&vhci->sc_bus);
	}

	/* Wake the USB hub poll */
	usb_needs_explore(&vhci->sc_bus, 0);
}

/*
 * Deferred endpoint reset task.
 *
 * Called on taskqueue_thread (can sleep) after CTRL_TRANSFER_STATUS(STALL).
 * Flushes residual SQ entries and issues ENDPOINT_RESET (0x0044) to clear
 * firmware's stall state.
 *
 * After reset, clears tq->stalled so the USB stack's next retry succeeds.
 */
static void
bce_vhci_reset_one_tq(struct bce_vhci_softc *vhci,
    struct bce_vhci_transfer_queue *tq)
{
	struct apple_bce_softc *sc = vhci->sc_bce;
	struct bce_vhci_message cmd, reply;

	device_printf(vhci->sc_dev,
	    "reset_task: flushing + ENDPOINT_RESET dev=%d ep=0x%02x\n",
	    tq->dev_addr, tq->endp_addr);

	/* Flush residual SQ submissions */
	if (tq->sq_in != NULL)
		bce_cmd_flush_queue(sc->sc_cmd_cmdq, sc, tq->sq_in->qid);
	if (tq->sq_out != NULL)
		bce_cmd_flush_queue(sc->sc_cmd_cmdq, sc, tq->sq_out->qid);

	/* Issue ENDPOINT_RESET to clear firmware stall state */
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd = BCE_VHCI_CMD_ENDPOINT_RESET;
	cmd.param1 = tq->dev_addr | ((tq->endp_addr & 0x8F) << 8);
	bce_vhci_cmd_execute(vhci, &cmd, &reply, BCE_VHCI_CMD_TIMEOUT_SHORT);

	device_printf(vhci->sc_dev,
	    "reset_task: ENDPOINT_RESET done, clearing stall\n");

	USB_BUS_LOCK(&vhci->sc_bus);
	tq->stalled = 0;
	USB_BUS_UNLOCK(&vhci->sc_bus);
}

static void
bce_vhci_reset_task(void *arg, int pending __unused)
{
	struct bce_vhci_softc *vhci = arg;
	int i, j;

	for (i = 0; i < BCE_VHCI_MAX_DEVICES; i++) {
		struct bce_vhci_device *dev = &vhci->sc_devs[i];

		if (dev->allocated == 0)
			continue;
		for (j = 0; j < BCE_VHCI_MAX_ENDPOINTS; j++) {
			struct bce_vhci_transfer_queue *tq = &dev->tq[j];

			if (tq->active == 0 || tq->stalled == 0)
				continue;
			bce_vhci_reset_one_tq(vhci, tq);
		}
	}
}

/*
 * bce_vhci_create_task: deferred endpoint creation from taskqueue_thread.
 *
 * bce_vhci_pipe_start cannot call bce_vhci_endpoint_create directly because
 * it may be invoked from a USB callback (e.g. usbhid_intr_in_callback) that
 * holds a non-sleepable lock.  Instead, pipe_start sets create_pending on the
 * tq and schedules this task.  We scan all devices/endpoints, create any with
 * create_pending set, then return USB_ERR_STALLED from pipe_start so the USB
 * stack retries, at which point tq->active is set and we skip creation.
 */
static void
bce_vhci_create_task(void *arg, int pending __unused)
{
	struct bce_vhci_softc *vhci = arg;
	int i, j, ep_err;

	for (i = 0; i < BCE_VHCI_MAX_DEVICES; i++) {
		struct bce_vhci_device *dev = &vhci->sc_devs[i];

		if (dev->allocated == 0)
			continue;

		for (j = 0; j < BCE_VHCI_MAX_ENDPOINTS; j++) {
			struct bce_vhci_transfer_queue *tq = &dev->tq[j];
			struct usb_endpoint_descriptor *edesc;
			struct usb_xfer *xfer;
			uint8_t ep_addr;

			USB_BUS_LOCK(&vhci->sc_bus);
			if (tq->create_pending == 0 || tq->active) {
				tq->create_pending = 0;
				USB_BUS_UNLOCK(&vhci->sc_bus);
				continue;
			}
			tq->create_pending = 0;
			ep_addr = tq->endp_addr;
			edesc = tq->create_edesc;
			xfer = tq->create_xfer;
			/*
			 * Do NOT clear create_xfer yet --
			 * pipe_close may race
			 */
			USB_BUS_UNLOCK(&vhci->sc_bus);

			ep_err = bce_vhci_endpoint_create(vhci, dev,
			    ep_addr, edesc);

			/*
			 * Re-check create_xfer under lock.  Atomically
			 * clear it and either install active_xfer or
			 * complete with error; no gap for pipe_close.
			 */
			USB_BUS_LOCK(&vhci->sc_bus);
			if (tq->create_xfer != xfer) {
				/*
				 * Original xfer was closed; leave any
				 * newer create_xfer for pipe_start retry.
				 */
				USB_BUS_UNLOCK(&vhci->sc_bus);
				continue;
			}

			if (ep_err != 0) {
				tq->create_xfer = NULL;
				if (xfer != NULL)
					usbd_transfer_done(xfer,
					    USB_ERR_STALLED);
				USB_BUS_UNLOCK(&vhci->sc_bus);
				device_printf(vhci->sc_dev,
				    "create_task: ep create "
				    "failed: dev=%d ep=0x%02x "
				    "err=%d\n",
				    dev->fw_dev_id, ep_addr,
				    ep_err);
				continue;
			}

			if (xfer != NULL && (ep_addr & UE_DIR_IN)) {
				struct bce_vhci_message treq;
				struct bce_qe_submission *si;
				uint32_t len;

				len = xfer->frlengths[0];
				if (len > BCE_VHCI_XFER_BUFSZ)
					len = BCE_VHCI_XFER_BUFSZ;
				/*
				 * Handoff: clear create_xfer and
				 * set active_xfer atomically.
				 */
				tq->create_xfer = NULL;
				tq->active_xfer = xfer;
				tq->dma_inflight = 1;
				USB_BUS_UNLOCK(&vhci->sc_bus);

				bus_dmamap_sync(tq->dma_tag,
				    tq->dma_map,
				    BUS_DMASYNC_PREREAD);

				/* Reserve msg first, then SQ */
				memset(&treq, 0, sizeof(treq));
				treq.cmd =
				    BCE_VHCI_CMD_TRANSFER_REQUEST;
				treq.param1 =
				    ((uint32_t)ep_addr << 8) |
				    dev->fw_dev_id;
				treq.param2 = len;

				mtx_lock_spin(&vhci->sc_async_lock);
				if (bce_reserve_submission(
				    vhci->msg_asynchronous.sq) != 0) {
					mtx_unlock_spin(
					    &vhci->sc_async_lock);
					USB_BUS_LOCK(&vhci->sc_bus);
					if (tq->active_xfer == xfer) {
						tq->active_xfer = NULL;
						tq->dma_inflight = 0;
						usbd_transfer_done(xfer,
						    USB_ERR_IOERROR);
					}
					USB_BUS_UNLOCK(&vhci->sc_bus);
					continue;
				}
				mtx_unlock_spin(&vhci->sc_async_lock);
				/* active_xfer already set above */

				mtx_lock_spin(&tq->lock);
				if (bce_reserve_submission(
				    tq->sq_in) == 0) {
					si = bce_next_submission(
					    tq->sq_in);
					si->addr = tq->dma_addr;
					si->length = len;
					si->segl_addr = 0;
					si->segl_length = 0;
					bce_submit_to_device(
					    vhci->sc_bce,
					    tq->sq_in);
					mtx_unlock_spin(&tq->lock);

					mtx_lock_spin(
					    &vhci->sc_async_lock);
					bce_vhci_msg_queue_write(vhci,
					    &vhci->msg_asynchronous,
					    &treq);
					mtx_unlock_spin(
					    &vhci->sc_async_lock);
				} else {
					mtx_unlock_spin(&tq->lock);
					/* Return reserved msg slot */
					mtx_lock_spin(
					    &vhci->sc_async_lock);
					atomic_add_int(&vhci->
					    msg_asynchronous.sq->
					    available_commands, 1);
					mtx_unlock_spin(
					    &vhci->sc_async_lock);
					USB_BUS_LOCK(&vhci->sc_bus);
					if (tq->active_xfer == xfer) {
						tq->active_xfer = NULL;
						tq->dma_inflight = 0;
						usbd_transfer_done(xfer,
						    USB_ERR_IOERROR);
					}
					USB_BUS_UNLOCK(&vhci->sc_bus);
				}
			} else if (xfer != NULL &&
			    (ep_addr & UE_DIR_IN) == 0) {
				/*
				 * OUT endpoint: set active and submit.
				 */
				struct bce_vhci_message treq;
				struct bce_qe_submission *so;
				uint32_t len;

				len = xfer->frlengths[0];
				if (len > BCE_VHCI_XFER_BUFSZ)
					len = BCE_VHCI_XFER_BUFSZ;

				tq->create_xfer = NULL;
				tq->active_xfer = xfer;
				tq->dma_inflight = 1;

				if (len > 0) {
					usbd_copy_out(
					    &xfer->frbuffers[0], 0,
					    tq->dma_buf, len);
				}
				USB_BUS_UNLOCK(&vhci->sc_bus);

				if (len > 0) {
					bus_dmamap_sync(tq->dma_tag,
					    tq->dma_map,
					    BUS_DMASYNC_PREWRITE);
				}

				memset(&treq, 0, sizeof(treq));
				treq.cmd =
				    BCE_VHCI_CMD_TRANSFER_REQUEST;
				treq.param1 =
				    ((uint32_t)ep_addr << 8) |
				    dev->fw_dev_id;
				treq.param2 = len;

				mtx_lock_spin(&vhci->sc_async_lock);
				if (bce_reserve_submission(
				    vhci->msg_asynchronous.sq) != 0) {
					mtx_unlock_spin(
					    &vhci->sc_async_lock);
					USB_BUS_LOCK(&vhci->sc_bus);
					if (tq->active_xfer == xfer) {
						tq->active_xfer = NULL;
						tq->dma_inflight = 0;
						usbd_transfer_done(xfer,
						    USB_ERR_IOERROR);
					}
					USB_BUS_UNLOCK(&vhci->sc_bus);
					continue;
				}
				mtx_unlock_spin(&vhci->sc_async_lock);

				mtx_lock_spin(&tq->lock);
				if (bce_reserve_submission(
				    tq->sq_out) == 0) {
					so = bce_next_submission(
					    tq->sq_out);
					so->addr = tq->dma_addr;
					so->length = len;
					so->segl_addr = 0;
					so->segl_length = 0;
					bce_submit_to_device(
					    vhci->sc_bce,
					    tq->sq_out);
					mtx_unlock_spin(&tq->lock);

					mtx_lock_spin(
					    &vhci->sc_async_lock);
					bce_vhci_msg_queue_write(vhci,
					    &vhci->msg_asynchronous,
					    &treq);
					mtx_unlock_spin(
					    &vhci->sc_async_lock);
				} else {
					mtx_unlock_spin(&tq->lock);
					mtx_lock_spin(
					    &vhci->sc_async_lock);
					atomic_add_int(&vhci->
					    msg_asynchronous.sq->
					    available_commands, 1);
					mtx_unlock_spin(
					    &vhci->sc_async_lock);
					USB_BUS_LOCK(&vhci->sc_bus);
					if (tq->active_xfer == xfer) {
						tq->active_xfer = NULL;
						tq->dma_inflight = 0;
						usbd_transfer_done(xfer,
						    USB_ERR_IOERROR);
					}
					USB_BUS_UNLOCK(&vhci->sc_bus);
				}
			} else if (xfer != NULL) {
				tq->create_xfer = NULL;
				usbd_transfer_done(xfer,
				    USB_ERR_STALLED);
				USB_BUS_UNLOCK(&vhci->sc_bus);
			} else {
				tq->create_xfer = NULL;
				USB_BUS_UNLOCK(&vhci->sc_bus);
			}
		}
	}
}

/*
 * Execute a synchronous command: send on msg_commands, wait for reply
 * on ev_commands or ev_system.
 */
static int
bce_vhci_cmd_execute(struct bce_vhci_softc *vhci,
    struct bce_vhci_message *req, struct bce_vhci_message *reply,
    int timeout_ticks)
{
	struct bce_vhci_cmd_queue *cq = &vhci->cmd;
	struct bce_vhci_message cancel;
	int error;

	sx_xlock(&cq->exec_lock);
	mtx_lock_spin(&cq->lock);

	/* Reserve a submission slot */
	if (bce_reserve_submission(cq->msg->sq) != 0) {
		mtx_unlock_spin(&cq->lock);
		sx_xunlock(&cq->exec_lock);
		return (EAGAIN);
	}

	/* Setup completion state */
	cq->pending = 1;
	cq->expected_cmd = req->cmd;
	memset(&cq->response, 0, sizeof(cq->response));

	mtx_unlock_spin(&cq->lock);

	/* Send the command */
	bce_vhci_msg_queue_write(vhci, cq->msg, req);

	/* Wait for reply */
	error = sema_timedwait(&cq->completion, timeout_ticks);

	mtx_lock_spin(&cq->lock);

	if (error != 0) {
		/*
		 * Timeout: send cancellation and wait briefly.
		 */
		device_printf(vhci->sc_dev,
		    "cmd 0x%04x timeout, sending cancel\n", req->cmd);

		if (bce_reserve_submission(cq->msg->sq) == 0) {
			cancel = *req;
			cancel.cmd |= BCE_VHCI_CMD_CANCEL_FLAG;
			cq->pending = 1;
			mtx_unlock_spin(&cq->lock);

			bce_vhci_msg_queue_write(vhci, cq->msg, &cancel);

			error = sema_timedwait(&cq->completion, hz);

			mtx_lock_spin(&cq->lock);
			if (error != 0) {
				device_printf(vhci->sc_dev,
				    "cmd cancel timeout, possible desync\n");
				cq->pending = 0;
				mtx_unlock_spin(&cq->lock);
				sx_xunlock(&cq->exec_lock);
				return (ETIMEDOUT);
			}

			/*
			 * Check if we got the cancel ack or the
			 * original reply
			 */
			if ((cq->response.cmd & ~BCE_VHCI_CMD_REPLY_FLAG) ==
			    (req->cmd | BCE_VHCI_CMD_CANCEL_FLAG)) {
				cq->pending = 0;
				mtx_unlock_spin(&cq->lock);
				sx_xunlock(&cq->exec_lock);
				return (ETIMEDOUT);
			}
			/* Got original reply; fall through */
		} else {
			cq->pending = 0;
			mtx_unlock_spin(&cq->lock);
			sx_xunlock(&cq->exec_lock);
			return (ETIMEDOUT);
		}
	}

	/* Copy reply before releasing the lock */
	{
		struct bce_vhci_message resp;

		resp = cq->response;
		cq->pending = 0;
		mtx_unlock_spin(&cq->lock);
		sx_xunlock(&cq->exec_lock);

		if (reply != NULL)
			*reply = resp;

		/* Validate reply from local copy */
		if ((resp.cmd & ~BCE_VHCI_CMD_REPLY_FLAG) != req->cmd) {
			device_printf(vhci->sc_dev,
			    "cmd mismatch: sent 0x%04x, got 0x%04x\n",
			    req->cmd, resp.cmd);
			return (EIO);
		}

		if (resp.status != BCE_VHCI_SUCCESS)
			return (resp.status);
	}

	return (0);
}

/*
 * Submit a pending IN xfer after the previous one completed.
 * Called under USB_BUS_LOCK.  nxfer has been detached from
 * tq->pending_xfer by the caller.
 */
static void
bce_vhci_submit_pending_in(struct bce_vhci_softc *vhci,
    struct bce_vhci_transfer_queue *tq, struct usb_xfer *nxfer)
{
	struct bce_vhci_message treq;
	struct bce_qe_submission *si;
	uint32_t nlen;

	nlen = nxfer->frlengths[0];
	if (nlen > BCE_VHCI_XFER_BUFSZ)
		nlen = BCE_VHCI_XFER_BUFSZ;

	bus_dmamap_sync(tq->dma_tag, tq->dma_map,
	    BUS_DMASYNC_PREREAD);

	memset(&treq, 0, sizeof(treq));
	treq.cmd = BCE_VHCI_CMD_TRANSFER_REQUEST;
	treq.param1 =
	    ((uint32_t)tq->endp_addr << 8) | tq->dev_addr;
	treq.param2 = nlen;

	/* Reserve msg first, then SQ */
	mtx_lock_spin(&vhci->sc_async_lock);
	if (bce_reserve_submission(
	    vhci->msg_asynchronous.sq) != 0) {
		mtx_unlock_spin(&vhci->sc_async_lock);
		usbd_transfer_done(nxfer, USB_ERR_IOERROR);
		return;
	}
	mtx_unlock_spin(&vhci->sc_async_lock);

	/*
	 * Install active_xfer BEFORE ringing the doorbell.
	 * A fast completion could otherwise see NULL and
	 * discard the result.
	 */
	tq->active_xfer = nxfer;
	tq->dma_inflight = 1;

	mtx_lock_spin(&tq->lock);
	if (bce_reserve_submission(tq->sq_in) == 0) {
		si = bce_next_submission(tq->sq_in);
		si->addr = tq->dma_addr;
		si->length = nlen;
		si->segl_addr = 0;
		si->segl_length = 0;
		bce_submit_to_device(vhci->sc_bce, tq->sq_in);
		mtx_unlock_spin(&tq->lock);

		mtx_lock_spin(&vhci->sc_async_lock);
		bce_vhci_msg_queue_write(vhci,
		    &vhci->msg_asynchronous, &treq);
		mtx_unlock_spin(&vhci->sc_async_lock);
	} else {
		mtx_unlock_spin(&tq->lock);
		tq->active_xfer = NULL;
		tq->dma_inflight = 0;
		/* Return reserved msg slot */
		mtx_lock_spin(&vhci->sc_async_lock);
		atomic_add_int(
		    &vhci->msg_asynchronous.sq->
		    available_commands, 1);
		mtx_unlock_spin(&vhci->sc_async_lock);
		usbd_transfer_done(nxfer, USB_ERR_IOERROR);
	}
}

/*
 * Submit a pending OUT xfer after the previous one completed.
 * Called under USB_BUS_LOCK.  nxfer has been detached from
 * tq->pending_xfer by the caller.
 */
static void
bce_vhci_submit_pending_out(struct bce_vhci_softc *vhci,
    struct bce_vhci_transfer_queue *tq, struct usb_xfer *nxfer)
{
	struct bce_vhci_message treq;
	struct bce_qe_submission *so;
	uint32_t nlen;

	nlen = nxfer->frlengths[0];
	if (nlen > BCE_VHCI_XFER_BUFSZ)
		nlen = BCE_VHCI_XFER_BUFSZ;

	usbd_copy_out(&nxfer->frbuffers[0], 0,
	    tq->dma_buf, nlen);
	bus_dmamap_sync(tq->dma_tag, tq->dma_map,
	    BUS_DMASYNC_PREWRITE);

	memset(&treq, 0, sizeof(treq));
	treq.cmd = BCE_VHCI_CMD_TRANSFER_REQUEST;
	treq.param1 =
	    ((uint32_t)tq->endp_addr << 8) | tq->dev_addr;
	treq.param2 = nlen;

	/* Reserve msg first, then SQ */
	mtx_lock_spin(&vhci->sc_async_lock);
	if (bce_reserve_submission(
	    vhci->msg_asynchronous.sq) != 0) {
		mtx_unlock_spin(&vhci->sc_async_lock);
		usbd_transfer_done(nxfer, USB_ERR_IOERROR);
		return;
	}
	mtx_unlock_spin(&vhci->sc_async_lock);

	/*
	 * Install active_xfer BEFORE ringing the doorbell.
	 */
	tq->active_xfer = nxfer;
	tq->dma_inflight = 1;

	mtx_lock_spin(&tq->lock);
	if (bce_reserve_submission(tq->sq_out) == 0) {
		so = bce_next_submission(tq->sq_out);
		so->addr = tq->dma_addr;
		so->length = nlen;
		so->segl_addr = 0;
		so->segl_length = 0;
		bce_submit_to_device(vhci->sc_bce, tq->sq_out);
		mtx_unlock_spin(&tq->lock);

		mtx_lock_spin(&vhci->sc_async_lock);
		bce_vhci_msg_queue_write(vhci,
		    &vhci->msg_asynchronous, &treq);
		mtx_unlock_spin(&vhci->sc_async_lock);
	} else {
		mtx_unlock_spin(&tq->lock);
		tq->active_xfer = NULL;
		tq->dma_inflight = 0;
		/* Return reserved msg slot */
		mtx_lock_spin(&vhci->sc_async_lock);
		atomic_add_int(
		    &vhci->msg_asynchronous.sq->
		    available_commands, 1);
		mtx_unlock_spin(&vhci->sc_async_lock);
		usbd_transfer_done(nxfer, USB_ERR_IOERROR);
	}
}

/*
 * Transfer queue DMA completion callback.  Fires when the firmware
 * has consumed (OUT) or filled (IN) a DMA buffer we submitted.
 *
 * For IN transfers, record the actual byte count from the completion
 * so that handle_ctrl_status knows how much data was received.
 */
static void
bce_vhci_tq_completion(struct bce_queue_sq *sq)
{
	struct bce_vhci_transfer_queue *tq = sq->userdata;
	struct bce_vhci_softc *vhci = tq->vhci;

	while (sq->completion_cidx != sq->completion_tail) {
		struct bce_sq_completion_data *cd;

		cd = &sq->completion_data[sq->completion_cidx];

		/*
		 * For IN SQ completions (device -> host), handle data.
		 * BCE uses ithreaded MSI, so we can acquire USB_BUS_LOCK
		 * (MTX_DEF) here.  tq->lock (MTX_SPIN) nesting inside
		 * USB_BUS_LOCK is valid.
		 */
		if (sq == tq->sq_in && cd->status == BCE_COMP_SUCCESS) {
			if (tq->endp_addr == 0x00) {
				/*
				 * Control transfer: just record data length.
				 * Actual completion happens in
				 * handle_ctrl_status.  Clamp to DMA buffer.
				 * USB_BUS_LOCK protects ctrl_actual and
				 * ctrl_data_done against concurrent access
				 * from handle_ctrl_status.
				 *
				 * If CTRL_TRANSFER_STATUS arrived first
				 * (ctrl_status_pending), process it now
				 * that data is ready.
				 */
				uint32_t alen = (uint32_t)cd->data_size;
				if (alen > BCE_VHCI_XFER_BUFSZ)
					alen = BCE_VHCI_XFER_BUFSZ;
				USB_BUS_LOCK(&vhci->sc_bus);
				if (tq->active_xfer == NULL ||
				    (tq->ctrl_state != BCE_VHCI_CTRL_STATUS &&
				    tq->ctrl_state != BCE_VHCI_CTRL_DATA)) {
					tq->dma_inflight = 0;
					USB_BUS_UNLOCK(&vhci->sc_bus);
					goto next_compl;
				}
				if (alen > tq->ctrl_data_len)
					alen = tq->ctrl_data_len;
				tq->ctrl_actual = alen;
				tq->ctrl_data_done = 1;
				if (tq->ctrl_status_pending != 0) {
					tq->ctrl_status_pending = 0;
					bce_vhci_complete_ctrl_locked(
					    vhci, tq,
					    &tq->ctrl_status_msg);
				}
				USB_BUS_UNLOCK(&vhci->sc_bus);
			} else {
				/*
				 * Interrupt/bulk IN transfer: data is ready.
				 * Copy into xfer buffer and complete.
				 */
				struct usb_xfer *xfer;
				uint32_t len = (uint32_t)cd->data_size;

				USB_BUS_LOCK(&vhci->sc_bus);
				xfer = tq->active_xfer;
				tq->dma_inflight = 0;
				if (xfer != NULL) {
					bus_dmamap_sync(tq->dma_tag,
					    tq->dma_map,
					    BUS_DMASYNC_POSTREAD);

					if (len > BCE_VHCI_XFER_BUFSZ)
						len = BCE_VHCI_XFER_BUFSZ;
					if (len > xfer->frlengths[0])
						len = xfer->frlengths[0];

					usbd_copy_in(&xfer->frbuffers[0], 0,
					    tq->dma_buf, len);
					xfer->frlengths[0] = len;
					xfer->aframes = xfer->nframes;
					tq->active_xfer = NULL;

					/* Start next queued xfer if any */
					if (tq->pending_xfer != NULL) {
						struct usb_xfer *nxfer;

						nxfer = tq->pending_xfer;
						tq->pending_xfer = NULL;
						bce_vhci_submit_pending_in(
						    vhci, tq, nxfer);
					}

					usbd_transfer_done(xfer,
					    USB_ERR_NORMAL_COMPLETION);
				} else if (tq->pending_xfer != NULL) {
					/*
					 * Stale completion from cancelled xfer.
					 * DMA drained; start pending xfer now.
					 */
					struct usb_xfer *nxfer;

					nxfer = tq->pending_xfer;
					tq->pending_xfer = NULL;
					bce_vhci_submit_pending_in(
					    vhci, tq, nxfer);
				}
				USB_BUS_UNLOCK(&vhci->sc_bus);
			}
		}

		/*
		 * For ep0 OUT SQ completion in CTRL_SETUP state:
		 * setup packet DMA is done, start the data phase.
		 * USB_BUS_LOCK protects ctrl_state against concurrent
		 * access from pipe_start, pipe_close, and handle_ctrl_status.
		 */
		if (sq == tq->sq_out && tq->endp_addr == 0x00 &&
		    cd->status == BCE_COMP_SUCCESS) {
			bus_dmamap_sync(tq->dma_tag, tq->dma_map,
			    BUS_DMASYNC_POSTWRITE);
			USB_BUS_LOCK(&vhci->sc_bus);
			if (tq->active_xfer == NULL) {
				tq->dma_inflight = 0;
				USB_BUS_UNLOCK(&vhci->sc_bus);
				goto next_compl;
			}
			if (tq->ctrl_state == BCE_VHCI_CTRL_SETUP) {
				if (tq->ctrl_data_len > 0) {
					tq->ctrl_state = BCE_VHCI_CTRL_DATA;
				} else {
					tq->ctrl_state = BCE_VHCI_CTRL_STATUS;
				}
				/*
				 * CTRL_TRANSFER_STATUS may have arrived
				 * before setup DMA completed.  Process
				 * the deferred status now.
				 */
				if (tq->ctrl_status_pending != 0) {
					tq->ctrl_status_pending = 0;
					bce_vhci_complete_ctrl_locked(
					    vhci, tq,
					    &tq->ctrl_status_msg);
				}
			} else if (tq->ctrl_state ==
			    BCE_VHCI_CTRL_STATUS &&
			    tq->ctrl_dir == UE_DIR_OUT) {
				/*
				 * OUT data DMA done.  Allow
				 * CTRL_TRANSFER_STATUS to proceed.
				 */
				tq->ctrl_data_done = 1;
				if (tq->ctrl_status_pending != 0) {
					tq->ctrl_status_pending = 0;
					bce_vhci_complete_ctrl_locked(
					    vhci, tq,
					    &tq->ctrl_status_msg);
				}
			}
			USB_BUS_UNLOCK(&vhci->sc_bus);
			/*
			 * Data phase (both IN and OUT) is driven by firmware
			 * TRANSFER_REQUEST events handled in
			 * handle_transfer_request().
			 */
		}

		/*
		 * For OUT SQ completions on non-control endpoints,
		 * the firmware consumed our data; complete the xfer.
		 */
		if (sq == tq->sq_out && tq->endp_addr != 0x00 &&
		    cd->status == BCE_COMP_SUCCESS) {
			struct usb_xfer *xfer;

			/*
			 * POSTWRITE before CPU touches buffer again.
			 */
			bus_dmamap_sync(tq->dma_tag, tq->dma_map,
			    BUS_DMASYNC_POSTWRITE);

			USB_BUS_LOCK(&vhci->sc_bus);
			xfer = tq->active_xfer;
			tq->dma_inflight = 0;
			if (xfer != NULL) {
				tq->active_xfer = NULL;

				/* Start next queued OUT xfer if any */
				if (tq->pending_xfer != NULL) {
					struct usb_xfer *nxfer;

					nxfer = tq->pending_xfer;
					tq->pending_xfer = NULL;
					bce_vhci_submit_pending_out(
					    vhci, tq, nxfer);
				}

				xfer->aframes = xfer->nframes;
				usbd_transfer_done(xfer,
				    USB_ERR_NORMAL_COMPLETION);
			} else if (tq->pending_xfer != NULL) {
				struct usb_xfer *nxfer;

				nxfer = tq->pending_xfer;
				tq->pending_xfer = NULL;
				bce_vhci_submit_pending_out(
				    vhci, tq, nxfer);
			}
			USB_BUS_UNLOCK(&vhci->sc_bus);
		}

		/*
		 * Handle SQ error completions.  Clear dma_inflight
		 * and complete active xfer with error so the endpoint
		 * is not permanently stuck.
		 */
		if (cd->status != BCE_COMP_SUCCESS) {
			struct usb_xfer *xfer, *pxfer;

			USB_BUS_LOCK(&vhci->sc_bus);
			xfer = tq->active_xfer;
			pxfer = tq->pending_xfer;
			tq->dma_inflight = 0;
			if (xfer != NULL) {
				tq->active_xfer = NULL;
				tq->pending_xfer = NULL;
				tq->ctrl_state = BCE_VHCI_CTRL_IDLE;
				if (pxfer != NULL)
					usbd_transfer_done(pxfer,
					    USB_ERR_IOERROR);
				usbd_transfer_done(xfer,
				    USB_ERR_IOERROR);
			} else if (pxfer != NULL) {
				tq->pending_xfer = NULL;
				usbd_transfer_done(pxfer,
				    USB_ERR_IOERROR);
			}
			USB_BUS_UNLOCK(&vhci->sc_bus);
		}

next_compl:
		sq->completion_cidx =
		    (sq->completion_cidx + 1) % sq->el_count;
		bce_notify_submission_complete(sq);
	}
}

/*
 * Create per-endpoint DMA transfer queues and register with firmware.
 */
static int
bce_vhci_endpoint_create(struct bce_vhci_softc *vhci,
    struct bce_vhci_device *dev, uint8_t ep_addr,
    struct usb_endpoint_descriptor *edesc)
{
	struct apple_bce_softc *sc = vhci->sc_bce;
	struct bce_vhci_transfer_queue *tq;
	struct bce_queue_memcfg cfg;
	struct bce_vhci_dma_cb_arg cb;
	struct bce_vhci_message cmd, reply;
	char name[0x20];
	uint32_t status;
	int error, cq_qid, out_qid, in_qid, i;
	uint8_t ep_idx;

	ep_idx = bce_vhci_ep_index(ep_addr);
	if (ep_idx >= BCE_VHCI_MAX_ENDPOINTS)
		return (EINVAL);

	tq = &dev->tq[ep_idx];
	if (tq->active)
		return (EEXIST);

	/*
	 * Initialize runtime fields.  Do NOT zero the whole struct:
	 * create_xfer/create_pending are live state managed by
	 * create_task under USB_BUS_LOCK.
	 */
	tq->vhci = vhci;
	tq->dev_addr = dev->fw_dev_id;
	tq->endp_addr = ep_addr;
	tq->cq = NULL;
	tq->sq_in = NULL;
	tq->sq_out = NULL;
	tq->active_xfer = NULL;
	tq->pending_xfer = NULL;
	tq->paused_by = 0;
	tq->active = 0;
	tq->stalled = 0;
	tq->dma_inflight = 0;

	/* Free leftover DMA buffer from previous incarnation */
	if (tq->dma_tag != NULL) {
		bus_dmamap_unload(tq->dma_tag, tq->dma_map);
		bus_dmamem_free(tq->dma_tag, tq->dma_buf, tq->dma_map);
		bus_dma_tag_destroy(tq->dma_tag);
		tq->dma_tag = NULL;
	}
	tq->dma_map = NULL;
	tq->dma_addr = 0;
	tq->dma_buf = NULL;
	tq->ctrl_state = BCE_VHCI_CTRL_IDLE;
	tq->ctrl_dir = 0;
	tq->ctrl_data_len = 0;
	tq->ctrl_actual = 0;
	tq->ctrl_data_done = 0;
	tq->ctrl_status_pending = 0;
	tq->evt_pending = 0;
	/* tq->lock initialized in device_create, valid for device lifetime */

	/* Allocate DMA buffer for data transfers */
	error = bus_dma_tag_create(sc->sc_dma_tag,
	    4, 0,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR,
	    NULL, NULL,
	    BCE_VHCI_XFER_BUFSZ, 1, BCE_VHCI_XFER_BUFSZ,
	    BUS_DMA_WAITOK,
	    NULL, NULL,
	    &tq->dma_tag);
	if (error != 0)
		return (error);

	error = bus_dmamem_alloc(tq->dma_tag, &tq->dma_buf,
	    BUS_DMA_WAITOK | BUS_DMA_ZERO | BUS_DMA_COHERENT,
	    &tq->dma_map);
	if (error != 0)
		goto fail_tag;

	error = bus_dmamap_load(tq->dma_tag, tq->dma_map, tq->dma_buf,
	    BCE_VHCI_XFER_BUFSZ, bce_vhci_dma_cb, &cb, BUS_DMA_WAITOK);
	if (error != 0 || cb.error != 0) {
		error = error != 0 ? error : cb.error;
		goto fail_mem;
	}
	tq->dma_addr = cb.addr;

	/* Allocate CQ for this endpoint */
	cq_qid = bce_vhci_alloc_qid(vhci);
	if (cq_qid < 0) {
		error = ENOSPC;
		goto fail_dma;
	}
	tq->cq = bce_alloc_cq(sc, cq_qid, BCE_VHCI_TQ_EL);
	if (tq->cq == NULL) {
		error = ENOMEM;
		goto fail_cq_alloc;
	}

	bce_get_cq_memcfg(tq->cq, &cfg);
	cfg.vector_or_cq = 4;
	status = bce_cmd_register_queue(sc->sc_cmd_cmdq, sc, &cfg, NULL, 0);
	if (status != 0) {
		error = EIO;
		goto fail_cq;
	}

	mtx_lock(&sc->sc_queues_lock);
	sc->sc_queues[cq_qid] = tq->cq;
	{
		int inserted = 0;

		for (i = 0; i < BCE_MAX_CQ_COUNT; i++) {
			if (sc->sc_cq_list[i] == NULL) {
				sc->sc_cq_list[i] = tq->cq;
				inserted = 1;
				break;
			}
		}
		if (inserted == 0) {
			sc->sc_queues[cq_qid] = NULL;
			mtx_unlock(&sc->sc_queues_lock);
			device_printf(vhci->sc_dev,
			    "CQ list full, cannot add endpoint CQ\n");
			error = ENOSPC;
			goto fail_cq_reg;
		}
	}
	mtx_unlock(&sc->sc_queues_lock);

	/* Allocate OUT SQ (host -> device) */
	out_qid = bce_vhci_alloc_qid(vhci);
	if (out_qid < 0) {
		error = ENOSPC;
		goto fail_cq_reg;
	}
	tq->sq_out = bce_alloc_sq(sc, out_qid,
	    sizeof(struct bce_qe_submission), BCE_VHCI_TQ_EL,
	    bce_vhci_tq_completion, tq);
	if (tq->sq_out == NULL) {
		error = ENOMEM;
		goto fail_sq_out_alloc;
	}

	snprintf(name, sizeof(name), "VHC1-%d-%02x",
	    dev->fw_dev_id, ep_addr & 0x0F);
	bce_get_sq_memcfg(tq->sq_out, tq->cq, &cfg);
	status = bce_cmd_register_queue(sc->sc_cmd_cmdq, sc, &cfg, name, 1);
	if (status != 0) {
		device_printf(vhci->sc_dev,
		    "failed to register OUT SQ '%s': %u\n", name, status);
		error = EIO;
		goto fail_sq_out;
	}

	mtx_lock(&sc->sc_queues_lock);
	sc->sc_queues[out_qid] = tq->sq_out;
	sc->sc_int_sq_list[out_qid] = tq->sq_out;
	mtx_unlock(&sc->sc_queues_lock);

	/* Allocate IN SQ (device -> host) */
	in_qid = bce_vhci_alloc_qid(vhci);
	if (in_qid < 0) {
		error = ENOSPC;
		goto fail_sq_out_reg;
	}
	tq->sq_in = bce_alloc_sq(sc, in_qid,
	    sizeof(struct bce_qe_submission), BCE_VHCI_TQ_EL,
	    bce_vhci_tq_completion, tq);
	if (tq->sq_in == NULL) {
		error = ENOMEM;
		goto fail_sq_in_alloc;
	}

	snprintf(name, sizeof(name), "VHC1-%d-%02x",
	    dev->fw_dev_id, ep_addr | 0x80);
	bce_get_sq_memcfg(tq->sq_in, tq->cq, &cfg);
	status = bce_cmd_register_queue(sc->sc_cmd_cmdq, sc, &cfg, name, 0);
	if (status != 0) {
		device_printf(vhci->sc_dev,
		    "failed to register IN SQ '%s': %u\n", name, status);
		error = EIO;
		goto fail_sq_in;
	}

	mtx_lock(&sc->sc_queues_lock);
	sc->sc_queues[in_qid] = tq->sq_in;
	sc->sc_int_sq_list[in_qid] = tq->sq_in;
	mtx_unlock(&sc->sc_queues_lock);

	/* Tell firmware to create the endpoint */
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd = BCE_VHCI_CMD_ENDPOINT_CREATE;
	/*
	 * param1 = dev_id | ((ep_addr & 0x8F) << 8)
	 * param2 = type | (interval<<8) | (maxp<<16) | (maxp_burst<<32)
	 * Fields encode type, interval, maxpacket, and burst.
	 */
	cmd.param1 = dev->fw_dev_id |
	    ((uint32_t)(ep_addr & 0x8F) << 8);
	if (edesc != NULL) {
		uint8_t ep_type = UE_GET_XFERTYPE(edesc->bmAttributes);
		uint16_t maxp = UGETW(edesc->wMaxPacketSize) & 0x7FF;
		uint8_t mult = ((UGETW(edesc->wMaxPacketSize) >> 11) & 3) + 1;
		uint64_t maxp_burst = (uint64_t)mult * maxp;

		cmd.param2 = ep_type;
		if (ep_type == UE_INTERRUPT || ep_type == UE_ISOCHRONOUS)
			cmd.param2 |= (uint64_t)(edesc->bInterval - 1) << 8;
		cmd.param2 |= (uint64_t)maxp << 16;
		cmd.param2 |= maxp_burst << 32;
	}
	/* ep0: edesc=NULL -> param2=0, firmware uses defaults for control */

	error = bce_vhci_cmd_execute(vhci, &cmd, &reply,
	    BCE_VHCI_CMD_TIMEOUT_SHORT);
	if (error != 0) {
		device_printf(vhci->sc_dev,
		    "ENDPOINT_CREATE(dev=%d, ep=0x%02x) failed: %d\n",
		    dev->fw_dev_id, ep_addr, error);
		goto fail_sq_in_reg;
	}

	tq->active = 1;
	tq->ctrl_state = BCE_VHCI_CTRL_IDLE;

	device_printf(vhci->sc_dev,
	    "endpoint created: dev=%d ep=0x%02x\n",
	    dev->fw_dev_id, ep_addr);

	return (0);

fail_sq_in_reg:
	bce_cmd_unregister_queue(sc->sc_cmd_cmdq, sc, in_qid);
	mtx_lock(&sc->sc_queues_lock);
	sc->sc_queues[in_qid] = NULL;
	sc->sc_int_sq_list[in_qid] = NULL;
	mtx_unlock(&sc->sc_queues_lock);
fail_sq_in:
	bce_free_sq(sc, tq->sq_in);
	tq->sq_in = NULL;
fail_sq_in_alloc:
	bce_vhci_free_qid(vhci, in_qid);
fail_sq_out_reg:
	bce_cmd_unregister_queue(sc->sc_cmd_cmdq, sc, out_qid);
	mtx_lock(&sc->sc_queues_lock);
	sc->sc_queues[out_qid] = NULL;
	sc->sc_int_sq_list[out_qid] = NULL;
	mtx_unlock(&sc->sc_queues_lock);
fail_sq_out:
	bce_free_sq(sc, tq->sq_out);
	tq->sq_out = NULL;
fail_sq_out_alloc:
	bce_vhci_free_qid(vhci, out_qid);
fail_cq_reg:
	bce_cmd_unregister_queue(sc->sc_cmd_cmdq, sc, cq_qid);
	mtx_lock(&sc->sc_queues_lock);
	sc->sc_queues[cq_qid] = NULL;
	for (i = 0; i < BCE_MAX_CQ_COUNT; i++) {
		if (sc->sc_cq_list[i] == tq->cq) {
			sc->sc_cq_list[i] = NULL;
			break;
		}
	}
	mtx_unlock(&sc->sc_queues_lock);
fail_cq:
	bce_free_cq(sc, tq->cq);
	tq->cq = NULL;
fail_cq_alloc:
	bce_vhci_free_qid(vhci, cq_qid);
fail_dma:
	bus_dmamap_unload(tq->dma_tag, tq->dma_map);
fail_mem:
	bus_dmamem_free(tq->dma_tag, tq->dma_buf, tq->dma_map);
fail_tag:
	bus_dma_tag_destroy(tq->dma_tag);
	tq->dma_tag = NULL;
	return (error);
}

/*
 * Destroy a per-endpoint transfer queue.
 */
static void
bce_vhci_endpoint_destroy(struct bce_vhci_softc *vhci,
    struct bce_vhci_device *dev, uint8_t ep_addr)
{
	struct apple_bce_softc *sc = vhci->sc_bce;
	struct bce_vhci_transfer_queue *tq;
	struct bce_vhci_message cmd, reply;
	uint8_t ep_idx;
	int i;

	ep_idx = bce_vhci_ep_index(ep_addr);
	if (ep_idx >= BCE_VHCI_MAX_ENDPOINTS)
		return;

	tq = &dev->tq[ep_idx];
	if (tq->active == 0)
		return;

	/*
	 * Mark inactive and complete any orphaned transfers under USB_BUS_LOCK.
	 * IRQ event handlers (find_tq) check tq->active under USB_BUS_LOCK,
	 * so clearing it here prevents concurrent access during teardown.
	 */
	USB_BUS_LOCK(&vhci->sc_bus);
	tq->active = 0;
	tq->dma_inflight = 0;
	tq->ctrl_state = BCE_VHCI_CTRL_IDLE;
	{
		struct usb_xfer *ax, *px, *cx;

		ax = tq->active_xfer;
		px = tq->pending_xfer;
		cx = tq->create_xfer;
		tq->active_xfer = NULL;
		tq->pending_xfer = NULL;
		tq->create_xfer = NULL;
		tq->create_pending = 0;

		if (ax != NULL)
			usbd_transfer_done(ax, USB_ERR_CANCELLED);
		if (px != NULL)
			usbd_transfer_done(px, USB_ERR_CANCELLED);
		if (cx != NULL)
			usbd_transfer_done(cx, USB_ERR_CANCELLED);
	}
	USB_BUS_UNLOCK(&vhci->sc_bus);

	/*
	 * Drain the reset task to ensure it is not accessing this tq's
	 * queues concurrently.  Must be done without USB_BUS_LOCK held
	 * (taskqueue_drain may sleep).
	 */
	taskqueue_drain(taskqueue_thread, &vhci->sc_reset_task);

	/* Tell firmware to destroy the endpoint */
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd = BCE_VHCI_CMD_ENDPOINT_DESTROY;
	/* param1 = dev_id | ((ep_addr & 0x8F) << 8) */
	cmd.param1 = dev->fw_dev_id |
	    ((uint32_t)(ep_addr & 0x8F) << 8);
	bce_vhci_cmd_execute(vhci, &cmd, &reply, BCE_VHCI_CMD_TIMEOUT_SHORT);

	/* Tear down IN SQ */
	if (tq->sq_in != NULL) {
		int in_qid = tq->sq_in->qid;

		bce_cmd_flush_queue(sc->sc_cmd_cmdq, sc, in_qid);
		bce_cmd_unregister_queue(sc->sc_cmd_cmdq, sc, in_qid);
		mtx_lock(&sc->sc_queues_lock);
		sc->sc_queues[in_qid] = NULL;
		sc->sc_int_sq_list[in_qid] = NULL;
		mtx_unlock(&sc->sc_queues_lock);
		bce_free_sq(sc, tq->sq_in);
		tq->sq_in = NULL;
		bce_vhci_free_qid(vhci, in_qid);
	}

	/* Tear down OUT SQ */
	if (tq->sq_out != NULL) {
		int out_qid = tq->sq_out->qid;

		bce_cmd_flush_queue(sc->sc_cmd_cmdq, sc, out_qid);
		bce_cmd_unregister_queue(sc->sc_cmd_cmdq, sc, out_qid);
		mtx_lock(&sc->sc_queues_lock);
		sc->sc_queues[out_qid] = NULL;
		sc->sc_int_sq_list[out_qid] = NULL;
		mtx_unlock(&sc->sc_queues_lock);
		bce_free_sq(sc, tq->sq_out);
		tq->sq_out = NULL;
		bce_vhci_free_qid(vhci, out_qid);
	}

	/* Tear down CQ */
	if (tq->cq != NULL) {
		int cq_qid = tq->cq->qid;

		bce_cmd_unregister_queue(sc->sc_cmd_cmdq, sc, cq_qid);
		mtx_lock(&sc->sc_queues_lock);
		sc->sc_queues[cq_qid] = NULL;
		for (i = 0; i < BCE_MAX_CQ_COUNT; i++) {
			if (sc->sc_cq_list[i] == tq->cq) {
				sc->sc_cq_list[i] = NULL;
				break;
			}
		}
		mtx_unlock(&sc->sc_queues_lock);
		bce_free_cq(sc, tq->cq);
		tq->cq = NULL;
		bce_vhci_free_qid(vhci, cq_qid);
	}

	/*
	 * Keep DMA buffer alive: an ISR handler on a different event
	 * SQ may have passed find_tq before we unregistered the CQ
	 * and still references tq->dma_tag/dma_addr.  The buffer is
	 * freed in bce_vhci_tq_destroy (device_destroy / detach).
	 */

	/* tq->lock stays valid until device_destroy */

	device_printf(vhci->sc_dev,
	    "endpoint destroyed: dev=%d ep=0x%02x\n",
	    dev->fw_dev_id, ep_addr);
}

/*
 * Create a firmware device on a port and set up ep0 queues.
 * Called from the roothub SetPortFeature(PORT_RESET) path.
 *
 * NOTE: This runs from process context (USB explore thread) so it is
 * safe to sleep in bce_vhci_cmd_execute.
 */
static int
bce_vhci_device_create(struct bce_vhci_softc *vhci, uint8_t port)
{
	struct bce_vhci_message cmd, reply;
	struct bce_vhci_device *dev;
	uint8_t fw_dev_id;
	int error, i;

	/* Port reset */
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd = BCE_VHCI_CMD_PORT_RESET;
	cmd.param1 = port;
	cmd.param2 = 1000;	/* timeout ms */

	error = bce_vhci_cmd_execute(vhci, &cmd, &reply,
	    BCE_VHCI_CMD_TIMEOUT_LONG);
	if (error != 0) {
		device_printf(vhci->sc_dev,
		    "PORT_RESET(%d) failed: %d\n", port, error);
		return (error);
	}

	device_printf(vhci->sc_dev, "port %d reset complete\n", port);

	/* Create device */
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd = BCE_VHCI_CMD_DEVICE_CREATE;
	cmd.param1 = port;

	error = bce_vhci_cmd_execute(vhci, &cmd, &reply,
	    BCE_VHCI_CMD_TIMEOUT_SHORT);
	if (error != 0) {
		device_printf(vhci->sc_dev,
		    "DEVICE_CREATE(port=%d) failed: %d\n", port, error);
		return (error);
	}

	if (reply.param2 >= BCE_VHCI_MAX_DEVICES) {
		device_printf(vhci->sc_dev,
		    "firmware device ID %llu out of range\n",
		    (unsigned long long)reply.param2);
		/* Destroy the firmware device we just created */
		memset(&cmd, 0, sizeof(cmd));
		cmd.cmd = BCE_VHCI_CMD_DEVICE_DESTROY;
		cmd.param1 = (uint32_t)reply.param2;
		bce_vhci_cmd_execute(vhci, &cmd, &reply,
		    BCE_VHCI_CMD_TIMEOUT_SHORT);
		return (ERANGE);
	}

	fw_dev_id = (uint8_t)reply.param2;
	device_printf(vhci->sc_dev,
	    "device created: port=%d fw_dev_id=%d\n", port, fw_dev_id);

	dev = &vhci->sc_devs[fw_dev_id];
	memset(dev, 0, sizeof(*dev));
	dev->allocated = 1;
	dev->fw_dev_id = fw_dev_id;
	dev->port = port;
	vhci->sc_port_to_dev[port] = fw_dev_id;

	/* Initialize per-endpoint locks (valid for device lifetime) */
	{
		int i;

		for (i = 0; i < BCE_VHCI_MAX_ENDPOINTS; i++)
			mtx_init(&dev->tq[i].lock, "bce_vhci_tq",
			    NULL, MTX_SPIN);
	}

	/* Create ep0 (control endpoint, edesc=NULL -> firmware defaults) */
	error = bce_vhci_endpoint_create(vhci, dev, 0x00, NULL);
	if (error != 0) {
		device_printf(vhci->sc_dev,
		    "failed to create ep0 for dev %d: %d\n",
		    fw_dev_id, error);
		/* Destroy the device */
		memset(&cmd, 0, sizeof(cmd));
		cmd.cmd = BCE_VHCI_CMD_DEVICE_DESTROY;
		cmd.param1 = fw_dev_id;
		bce_vhci_cmd_execute(vhci, &cmd, &reply,
		    BCE_VHCI_CMD_TIMEOUT_SHORT);
		dev->allocated = 0;
		for (i = 0; i < BCE_VHCI_MAX_ENDPOINTS; i++)
			mtx_destroy(&dev->tq[i].lock);
		vhci->sc_port_to_dev[port] = 0xFF;
		return (error);
	}

	return (0);
}

/*
 * Destroy a firmware device and all its endpoints.
 */
static void
bce_vhci_device_destroy(struct bce_vhci_softc *vhci, uint8_t port)
{
	struct bce_vhci_device *dev;
	struct bce_vhci_message cmd, reply;
	uint8_t fw_dev_id;
	int i;

	if (port >= BCE_VHCI_MAX_PORTS)
		return;

	fw_dev_id = vhci->sc_port_to_dev[port];
	if (fw_dev_id >= BCE_VHCI_MAX_DEVICES)
		return;

	dev = &vhci->sc_devs[fw_dev_id];
	if (dev->allocated == 0)
		return;

	/*
	 * Drain the create task so it does not race endpoint creation
	 * against our teardown.  Must be done before destroying
	 * endpoints (taskqueue_drain may sleep).
	 */
	taskqueue_drain(taskqueue_thread, &vhci->sc_create_task);
	taskqueue_drain(taskqueue_thread, &vhci->sc_fwevt_task);

	/* Destroy all active endpoints */
	for (i = 0; i < BCE_VHCI_MAX_ENDPOINTS; i++) {
		if (dev->tq[i].active)
			bce_vhci_endpoint_destroy(vhci, dev,
			    dev->tq[i].endp_addr);
	}

	/* Destroy the firmware device */
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd = BCE_VHCI_CMD_DEVICE_DESTROY;
	cmd.param1 = fw_dev_id;
	bce_vhci_cmd_execute(vhci, &cmd, &reply,
	    BCE_VHCI_CMD_TIMEOUT_SHORT);

	/* Free deferred DMA buffers and destroy per-endpoint locks */
	for (i = 0; i < BCE_VHCI_MAX_ENDPOINTS; i++) {
		struct bce_vhci_transfer_queue *tq = &dev->tq[i];

		if (tq->dma_tag != NULL) {
			bus_dmamap_unload(tq->dma_tag, tq->dma_map);
			bus_dmamem_free(tq->dma_tag, tq->dma_buf,
			    tq->dma_map);
			bus_dma_tag_destroy(tq->dma_tag);
			tq->dma_tag = NULL;
		}
		mtx_destroy(&tq->lock);
	}

	dev->allocated = 0;
	vhci->sc_port_to_dev[port] = 0xFF;

	device_printf(vhci->sc_dev,
	    "device destroyed: port=%d fw_dev_id=%d\n",
	    port, fw_dev_id);
}

/*
 * Find the transfer queue for a given firmware device ID and endpoint.
 *
 * No USB_BUS_LOCK needed: endpoint_destroy clears tq->active under
 * USB_BUS_LOCK first (preventing new find_tq matches), then sends
 * ENDPOINT_DESTROY synchronously, then frees SQ/DMA resources.
 * Callers that drop USB_BUS_LOCK before SQ operations recheck
 * tq->active to handle the narrow window between active=0 and
 * resource free.  dev->allocated and tq->active are int-aligned;
 * reads are safe on x86 (aligned word reads are atomic).
 */
static struct bce_vhci_transfer_queue *
bce_vhci_find_tq(struct bce_vhci_softc *vhci, uint8_t dev_id, uint8_t ep_addr)
{
	struct bce_vhci_device *dev;
	uint8_t ep_idx;

	if (dev_id >= BCE_VHCI_MAX_DEVICES)
		return (NULL);

	dev = &vhci->sc_devs[dev_id];
	if (dev->allocated == 0)
		return (NULL);

	ep_idx = bce_vhci_ep_index(ep_addr);
	if (ep_idx >= BCE_VHCI_MAX_ENDPOINTS)
		return (NULL);

	if (dev->tq[ep_idx].active == 0)
		return (NULL);

	return (&dev->tq[ep_idx]);
}

/*
 * Handle TRANSFER_REQUEST from firmware.
 *
 * The firmware asks us for data by sending TRANSFER_REQUEST with:
 *   param1 = (ep_addr << 8) | dev_id
 *   param2 = requested byte count
 *
 * For control transfers this drives the setup/data phases.
 */
static void
bce_vhci_handle_transfer_request(struct bce_vhci_softc *vhci,
    struct bce_vhci_message *msg)
{
	struct bce_vhci_transfer_queue *tq;
	struct usb_xfer *xfer;
	struct bce_qe_submission *s;
	uint8_t dev_id, ep_addr;
	uint32_t req_len;
	int bus_locked;

	dev_id = msg->param1 & 0xFF;
	ep_addr = (msg->param1 >> 8) & 0xFF;
	req_len = (uint32_t)msg->param2;

	tq = bce_vhci_find_tq(vhci, dev_id, ep_addr);
	if (tq == NULL) {
		device_printf(vhci->sc_dev,
		    "TRANSFER_REQUEST for unknown dev=%d ep=0x%02x\n",
		    dev_id, ep_addr);
		return;
	}

	/*
	 * Read active_xfer under USB_BUS_LOCK to serialize with pipe_close.
	 * Called from ev_generic_completion (ithread) or from pipe_start
	 * (already under USB_BUS_LOCK) via evt_pending replay.
	 */
	bus_locked = mtx_owned(&vhci->sc_bus.bus_mtx);
	if (bus_locked == 0)
		USB_BUS_LOCK(&vhci->sc_bus);
	xfer = tq->active_xfer;
	if (xfer == NULL) {
		/*
		 * Firmware sends TRANSFER_REQUEST before the USB stack
		 * submits the xfer via pipe_start. Save the event and
		 * replay it when pipe_start fires.
		 */
		tq->evt_pending = 1;
		tq->evt_saved = *msg;
		if (bus_locked == 0)
			USB_BUS_UNLOCK(&vhci->sc_bus);
		return;
	}
	/*
	 * For non-control endpoints (interrupt/bulk), firmware is
	 * requesting or providing data.  Submit the appropriate buffer.
	 */
	if (tq->endp_addr != 0x00) {
		uint32_t len = req_len;

		if (bus_locked == 0)
			USB_BUS_UNLOCK(&vhci->sc_bus);

		if (len > BCE_VHCI_XFER_BUFSZ)
			len = BCE_VHCI_XFER_BUFSZ;

		/*
		 * Re-check tq->active after dropping USB_BUS_LOCK.
		 * endpoint_destroy sets active=0 under the lock before
		 * freeing SQ/DMA resources, so if it is clear, our
		 * SQ pointers may be stale.
		 */
		if (tq->active == 0)
			return;

		if (ep_addr & 0x80) {
			/* IN: firmware has data for us, submit receive buf */
			bus_dmamap_sync(tq->dma_tag, tq->dma_map,
			    BUS_DMASYNC_PREREAD);
			mtx_lock_spin(&tq->lock);
			if (bce_reserve_submission(tq->sq_in) == 0) {
				s = bce_next_submission(tq->sq_in);
				s->addr = tq->dma_addr;
				s->length = len;
				s->segl_addr = 0;
				s->segl_length = 0;
				bce_submit_to_device(vhci->sc_bce,
				    tq->sq_in);
				mtx_unlock_spin(&tq->lock);
			} else {
				mtx_unlock_spin(&tq->lock);
				/* SQ full; fail the transfer */
				if (bus_locked == 0)
					USB_BUS_LOCK(&vhci->sc_bus);
				if (tq->active_xfer == xfer) {
					tq->active_xfer = NULL;
					tq->dma_inflight = 0;
					usbd_transfer_done(xfer,
					    USB_ERR_IOERROR);
				}
				if (bus_locked == 0)
					USB_BUS_UNLOCK(&vhci->sc_bus);
			}
		} else {
			/* OUT: firmware wants data from us */
			if (bus_locked == 0)
				USB_BUS_LOCK(&vhci->sc_bus);
			if (xfer == tq->active_xfer &&
			    xfer->frlengths[0] > 0) {
				if (len > xfer->frlengths[0])
					len = xfer->frlengths[0];
				usbd_copy_out(&xfer->frbuffers[0], 0,
				    tq->dma_buf, len);
				if (bus_locked == 0)
					USB_BUS_UNLOCK(&vhci->sc_bus);
				bus_dmamap_sync(tq->dma_tag, tq->dma_map,
				    BUS_DMASYNC_PREWRITE);
				mtx_lock_spin(&tq->lock);
				if (bce_reserve_submission(tq->sq_out) == 0) {
					s = bce_next_submission(tq->sq_out);
					s->addr = tq->dma_addr;
					s->length = len;
					s->segl_addr = 0;
					s->segl_length = 0;
					bce_submit_to_device(vhci->sc_bce,
					    tq->sq_out);
					mtx_unlock_spin(&tq->lock);
				} else {
					mtx_unlock_spin(&tq->lock);
					/* SQ full; fail the transfer */
					if (bus_locked == 0)
						USB_BUS_LOCK(&vhci->sc_bus);
					if (tq->active_xfer == xfer) {
						tq->active_xfer = NULL;
						tq->dma_inflight = 0;
						usbd_transfer_done(xfer,
						    USB_ERR_IOERROR);
					}
					if (bus_locked == 0)
						USB_BUS_UNLOCK(&vhci->sc_bus);
				}
			} else {
				/*
				 * Zero-length OUT or cancelled xfer.
				 * Complete immediately, then start
				 * any pending transfer.
				 */
				if (xfer == tq->active_xfer) {
					tq->active_xfer = NULL;
					tq->dma_inflight = 0;
					xfer->aframes = xfer->nframes;

					if (tq->pending_xfer != NULL) {
						struct usb_xfer *nx;

						nx = tq->pending_xfer;
						tq->pending_xfer = NULL;
						bce_vhci_submit_pending_out(
						    vhci, tq, nx);
					}

					usbd_transfer_done(xfer,
					    USB_ERR_NORMAL_COMPLETION);
				}
				if (bus_locked == 0)
					USB_BUS_UNLOCK(&vhci->sc_bus);
			}
		}
		return;
	}

	/*
	 * Control endpoint (ep0) state machine.
	 * USB_BUS_LOCK is held here, protecting tq->active_xfer,
	 * tq->ctrl_state, and xfer validity.  We drop and re-validate
	 * only around spin-lock + DMA submission sections.
	 */
	if (tq->active_xfer != xfer) {
		/* Transfer was cancelled while we set up; bail */
		if (bus_locked == 0)
			USB_BUS_UNLOCK(&vhci->sc_bus);
		return;
	}

	tq->dma_inflight = 1;

	switch (tq->ctrl_state) {
	case BCE_VHCI_CTRL_SETUP:
	{
		/*
		 * Firmware wants the 8-byte setup packet.
		 * Copy from xfer frbuffers[0] into DMA buffer and submit
		 * on the OUT SQ.
		 */
		uint32_t len;

		len = req_len;
		if (len > 8)
			len = 8;
		if (len > BCE_VHCI_XFER_BUFSZ)
			len = BCE_VHCI_XFER_BUFSZ;

		usbd_copy_out(&xfer->frbuffers[0], 0, tq->dma_buf, len);
		if (bus_locked == 0)
			USB_BUS_UNLOCK(&vhci->sc_bus);

		if (tq->active == 0)
			return;

		bus_dmamap_sync(tq->dma_tag, tq->dma_map,
		    BUS_DMASYNC_PREWRITE);

		mtx_lock_spin(&tq->lock);
		if (bce_reserve_submission(tq->sq_out) != 0) {
			mtx_unlock_spin(&tq->lock);
			device_printf(vhci->sc_dev,
			    "no OUT SQ slot for setup\n");
			if (bus_locked == 0)
				USB_BUS_LOCK(&vhci->sc_bus);
			if (tq->active_xfer == xfer) {
				tq->active_xfer = NULL;
				tq->dma_inflight = 0;
				tq->ctrl_state = BCE_VHCI_CTRL_IDLE;
				usbd_transfer_done(xfer, USB_ERR_IOERROR);
			}
			if (bus_locked == 0)
				USB_BUS_UNLOCK(&vhci->sc_bus);
			return;
		}

		s = bce_next_submission(tq->sq_out);
		s->addr = tq->dma_addr;
		s->length = len;
		s->segl_addr = 0;
		s->segl_length = 0;
		bce_submit_to_device(vhci->sc_bce, tq->sq_out);
		mtx_unlock_spin(&tq->lock);

		/*
		 * Wait for OUT SQ completion (setup DMA done) before
		 * starting the data phase.  tq_completion will see
		 * CTRL_SETUP state on ep0 OUT completion and call
		 * data_start.  Stay in CTRL_SETUP until then.
		 */
		break;
	}

	case BCE_VHCI_CTRL_DATA:
	{
		/*
		 * Data phase.  Direction was determined from the setup
		 * packet bmRequestType bit 7.
		 * USB_BUS_LOCK is held on entry (protects xfer, ctrl_state).
		 */
		uint32_t len;

		len = req_len;
		if (len > BCE_VHCI_XFER_BUFSZ)
			len = BCE_VHCI_XFER_BUFSZ;
		if (len > tq->ctrl_data_len)
			len = tq->ctrl_data_len;

		/*
		 * Transition to STATUS before submitting DMA so that
		 * the SQ completion handler sees the correct state.
		 */
		tq->ctrl_state = BCE_VHCI_CTRL_STATUS;
		if (tq->ctrl_dir == UE_DIR_OUT)
			tq->ctrl_actual = len;

		if (tq->ctrl_dir == UE_DIR_IN) {
			/*
			 * Device -> host: reserve msg_asynchronous FIRST,
			 * then submit receive buffer on IN SQ.
			 * Correct ordering prevents an orphaned SQ entry
			 * if the msg slot is exhausted.
			 */
			struct bce_vhci_message treq;

			memset(&treq, 0, sizeof(treq));
			treq.cmd = BCE_VHCI_CMD_TRANSFER_REQUEST;
			treq.param1 =
			    ((uint32_t)tq->endp_addr << 8) | tq->dev_addr;
			treq.param2 = len;

			if (bus_locked == 0)
				USB_BUS_UNLOCK(&vhci->sc_bus);

			if (tq->active == 0)
				return;

			bus_dmamap_sync(tq->dma_tag, tq->dma_map,
			    BUS_DMASYNC_PREREAD);

			/* Reserve msg slot first */
			mtx_lock_spin(&vhci->sc_async_lock);
			if (bce_reserve_submission(
			    vhci->msg_asynchronous.sq) != 0) {
				mtx_unlock_spin(&vhci->sc_async_lock);
				device_printf(vhci->sc_dev,
				    "no msg_async slot for "
				    "ctrl data IN\n");
				if (bus_locked == 0)
					USB_BUS_LOCK(&vhci->sc_bus);
				if (tq->active_xfer == xfer) {
					tq->active_xfer = NULL;
					tq->dma_inflight = 0;
					tq->ctrl_state =
					    BCE_VHCI_CTRL_IDLE;
					usbd_transfer_done(xfer,
					    USB_ERR_IOERROR);
				}
				if (bus_locked == 0)
					USB_BUS_UNLOCK(&vhci->sc_bus);
				return;
			}
			mtx_unlock_spin(&vhci->sc_async_lock);

			/* Now reserve and submit IN SQ */
			mtx_lock_spin(&tq->lock);
			if (bce_reserve_submission(tq->sq_in) != 0) {
				mtx_unlock_spin(&tq->lock);
				/* Return the reserved msg slot */
				mtx_lock_spin(&vhci->sc_async_lock);
				atomic_add_int(&vhci->
				    msg_asynchronous.sq->
				    available_commands, 1);
				mtx_unlock_spin(&vhci->sc_async_lock);
				device_printf(vhci->sc_dev,
				    "no IN SQ slot for ctrl data\n");
				if (bus_locked == 0)
					USB_BUS_LOCK(&vhci->sc_bus);
				if (tq->active_xfer == xfer) {
					tq->active_xfer = NULL;
					tq->dma_inflight = 0;
					tq->ctrl_state =
					    BCE_VHCI_CTRL_IDLE;
					usbd_transfer_done(xfer,
					    USB_ERR_IOERROR);
				}
				if (bus_locked == 0)
					USB_BUS_UNLOCK(&vhci->sc_bus);
				return;
			}
			s = bce_next_submission(tq->sq_in);
			s->addr = tq->dma_addr;
			s->length = len;
			s->segl_addr = 0;
			s->segl_length = 0;
			bce_submit_to_device(vhci->sc_bce, tq->sq_in);
			mtx_unlock_spin(&tq->lock);

			mtx_lock_spin(&vhci->sc_async_lock);
			bce_vhci_msg_queue_write(vhci,
			    &vhci->msg_asynchronous, &treq);
			mtx_unlock_spin(&vhci->sc_async_lock);
		} else {
			/*
			 * Host -> device: copy data from xfer frbuffers[1]
			 * and submit on OUT SQ.  usbd_copy_out under
			 * USB_BUS_LOCK protects xfer validity.
			 */
			usbd_copy_out(&xfer->frbuffers[1], 0,
			    tq->dma_buf, len);
			if (bus_locked == 0)
				USB_BUS_UNLOCK(&vhci->sc_bus);

			if (tq->active == 0)
				return;

			bus_dmamap_sync(tq->dma_tag, tq->dma_map,
			    BUS_DMASYNC_PREWRITE);

			mtx_lock_spin(&tq->lock);
			if (bce_reserve_submission(tq->sq_out) != 0) {
				mtx_unlock_spin(&tq->lock);
				device_printf(vhci->sc_dev,
				    "no OUT SQ slot for data\n");
				if (bus_locked == 0)
					USB_BUS_LOCK(&vhci->sc_bus);
				if (tq->active_xfer == xfer) {
					tq->active_xfer = NULL;
					tq->dma_inflight = 0;
					tq->ctrl_state =
					    BCE_VHCI_CTRL_IDLE;
					usbd_transfer_done(xfer,
					    USB_ERR_IOERROR);
				}
				if (bus_locked == 0)
					USB_BUS_UNLOCK(&vhci->sc_bus);
				return;
			}

			s = bce_next_submission(tq->sq_out);
			s->addr = tq->dma_addr;
			s->length = len;
			s->segl_addr = 0;
			s->segl_length = 0;
			bce_submit_to_device(vhci->sc_bce, tq->sq_out);
			mtx_unlock_spin(&tq->lock);
		}

		break;
	}

	default:
		device_printf(vhci->sc_dev,
		    "unexpected TRANSFER_REQUEST in state %d\n",
		    tq->ctrl_state);
		tq->dma_inflight = 0;
		if (tq->active_xfer == xfer) {
			tq->active_xfer = NULL;
			tq->ctrl_state = BCE_VHCI_CTRL_IDLE;
			usbd_transfer_done(xfer, USB_ERR_IOERROR);
		}
		if (bus_locked == 0)
			USB_BUS_UNLOCK(&vhci->sc_bus);
		break;
	}
}

/*
 * Complete a control transfer.  Caller must hold USB_BUS_LOCK.
 * Maps firmware status to USB error and calls usbd_transfer_done.
 */
static void
bce_vhci_complete_ctrl_locked(struct bce_vhci_softc *vhci,
    struct bce_vhci_transfer_queue *tq, struct bce_vhci_message *msg)
{
	struct usb_xfer *xfer;
	usb_error_t usb_err;

	xfer = tq->active_xfer;
	if (xfer == NULL)
		return;

	/* Map firmware status to USB error */
	switch (msg->status) {
	case BCE_VHCI_SUCCESS:
		usb_err = USB_ERR_NORMAL_COMPLETION;

		/*
		 * Tell the USB stack all frames completed.
		 * usbd_transfer_done computes
		 * actlen = sum(frlengths[0..aframes-1]).
		 * If aframes stays 0, actlen=0 < sumlen -> USB_ERR_SHORT_XFER.
		 */
		xfer->aframes = xfer->nframes;

		/*
		 * If this was an IN data transfer, copy the received
		 * data back into the xfer buffer using usbd_copy_in
		 * (correct API for page-cache buffers).
		 */
		if (tq->ctrl_dir == UE_DIR_IN && tq->ctrl_actual > 0) {
			bus_dmamap_sync(tq->dma_tag, tq->dma_map,
			    BUS_DMASYNC_POSTREAD);

			usbd_copy_in(&xfer->frbuffers[1], 0,
			    tq->dma_buf, tq->ctrl_actual);
			xfer->frlengths[1] = tq->ctrl_actual;
		}
		break;
	case BCE_VHCI_PIPE_STALL:
		usb_err = USB_ERR_STALLED;
		/*
		 * Mark endpoint stalled so pipe_start will issue
		 * ENDPOINT_RESET (0x0044) before the next transfer.
		 */
		tq->stalled = 1;
		break;
	case BCE_VHCI_ABORT:
		usb_err = USB_ERR_CANCELLED;
		break;
	default:
		usb_err = USB_ERR_IOERROR;
		break;
	}

	tq->active_xfer = NULL;
	tq->dma_inflight = 0;
	tq->ctrl_state = BCE_VHCI_CTRL_IDLE;

	usbd_transfer_done(xfer, usb_err);
}

/*
 * Handle CTRL_TRANSFER_STATUS from firmware.
 *
 * This signals the end of a control transfer.
 *   param1 = (ep_addr << 8) | dev_id
 *   status = BCE_VHCI_SUCCESS(1) or error code
 *
 * For IN transfers, the IN DMA completion (tq_completion) must have
 * set ctrl_actual before we can copy data.  If the DMA completion
 * has not fired yet (ctrl_data_done == 0), defer this message and
 * let tq_completion process it when the data arrives.
 */
static void
bce_vhci_handle_ctrl_status(struct bce_vhci_softc *vhci,
    struct bce_vhci_message *msg)
{
	struct bce_vhci_transfer_queue *tq;
	uint8_t dev_id, ep_addr;

	dev_id = msg->param1 & 0xFF;
	ep_addr = (msg->param1 >> 8) & 0xFF;

	tq = bce_vhci_find_tq(vhci, dev_id, ep_addr);
	if (tq == NULL) {
		device_printf(vhci->sc_dev,
		    "CTRL_TRANSFER_STATUS for unknown dev=%d ep=0x%02x\n",
		    dev_id, ep_addr);
		return;
	}

	/*
	 * Acquire USB_BUS_LOCK before touching xfer state.
	 * This is called from ev_generic_completion (ithread context),
	 * so MTX_DEF is safe.  Serializes with pipe_close/pipe_start.
	 */
	USB_BUS_LOCK(&vhci->sc_bus);

	if (tq->active_xfer == NULL) {
		USB_BUS_UNLOCK(&vhci->sc_bus);
		device_printf(vhci->sc_dev,
		    "CTRL_TRANSFER_STATUS but no active xfer\n");
		return;
	}

	/*
	 * Defer successful completion until DMA completes.
	 * - ctrl_state SETUP: setup packet DMA still in flight
	 * - ctrl_data_len > 0 with !ctrl_data_done: data DMA pending
	 * Error statuses are never deferred to avoid permanent hangs
	 * if the DMA completion is lost due to the error.
	 */
	if (msg->status == BCE_VHCI_SUCCESS &&
	    (tq->ctrl_state == BCE_VHCI_CTRL_SETUP ||
	    (tq->ctrl_data_len > 0 && tq->ctrl_data_done == 0))) {
		tq->ctrl_status_msg = *msg;
		tq->ctrl_status_pending = 1;
		USB_BUS_UNLOCK(&vhci->sc_bus);
		return;
	}

	if (msg->status != BCE_VHCI_SUCCESS)
		device_printf(vhci->sc_dev,
		    "CTRL_TRANSFER_STATUS: dev=%d ep=0x%02x status=%u\n",
		    dev_id, ep_addr, msg->status);

	bce_vhci_complete_ctrl_locked(vhci, tq, msg);
	USB_BUS_UNLOCK(&vhci->sc_bus);
}

/*
 * Send a firmware event reply on msg_system.
 *
 * Firmware events on ev_commands are acknowledged by replying with
 * cmd | 0x8000 and a status code on msg_system (NOT msg_asynchronous,
 * NOT ENDPOINT_SET_STATE).
 */
static void
bce_vhci_send_fw_event_reply(struct bce_vhci_softc *vhci,
    struct bce_vhci_message *req, uint16_t status)
{
	struct bce_vhci_message resp;

	resp.cmd = req->cmd | BCE_VHCI_CMD_REPLY_FLAG;
	resp.status = status;
	resp.param1 = req->param1;
	resp.param2 = 0;

	if (bce_reserve_submission(vhci->msg_system.sq) == 0)
		bce_vhci_msg_queue_write(vhci, &vhci->msg_system, &resp);
	else
		device_printf(vhci->sc_dev,
		    "failed to send FW event reply for 0x%04x\n",
		    req->cmd);
}

/*
 * Handle ENDPOINT_REQ_STATE (0x0043) from firmware.
 *
 * Called from taskqueue context (sole consumer of ev_commands).
 * Updates internal pause/stall state only; no messages are sent
 * on msg_asynchronous from here (that queue is written from ISR
 * context only, avoiding multi-producer races).
 *
 * The reply (cmd | 0x8000) is sent by the caller on msg_system.
 */
static uint16_t
bce_vhci_handle_endpoint_req_state(struct bce_vhci_softc *vhci,
    struct bce_vhci_message *msg)
{
	struct bce_vhci_transfer_queue *tq;
	uint8_t dev_id, ep_addr;
	uint32_t req_state;

	dev_id = msg->param1 & 0xFF;
	ep_addr = (msg->param1 >> 8) & 0xFF;
	req_state = (uint32_t)msg->param2;

	tq = bce_vhci_find_tq(vhci, dev_id, ep_addr);
	if (tq == NULL)
		return (BCE_VHCI_BAD_ARGUMENT);

	/*
	 * USB_BUS_LOCK protects paused_by, stalled, ctrl_state, and active_xfer
	 * against concurrent access from pipe_start, pipe_close, and ISR paths.
	 * Nesting USB_BUS_LOCK (MTX_DEF) -> tq->lock / sc_async_lock (MTX_SPIN)
	 * is valid.
	 */
	USB_BUS_LOCK(&vhci->sc_bus);

	/* Revalidate after taking the lock; teardown may have started */
	if (tq->active == 0) {
		USB_BUS_UNLOCK(&vhci->sc_bus);
		return (BCE_VHCI_SUCCESS);
	}

	switch (req_state) {
	case BCE_VHCI_ENDP_ACTIVE:
	{
		int was_paused_by_fw;

		was_paused_by_fw =
		    (tq->paused_by & BCE_VHCI_PAUSE_FIRMWARE) != 0;
		tq->paused_by &= ~BCE_VHCI_PAUSE_FIRMWARE;
		tq->stalled = 0;
		/*
		 * Firmware flushes SQs during PAUSE, so after ACTIVE we must
		 * re-submit the IN buffer + TRANSFER_REQUEST, but ONLY if
		 * the endpoint was actually paused by firmware.  Firmware
		 * also sends ENDP_ACTIVE after a fresh ENDPOINT_CREATE; in
		 * that case the create_task has already sent the initial
		 * TRANSFER_REQUEST and a second submission here would confuse
		 * firmware state.
		 *
		 * NOTE: do NOT send ENDPOINT_SET_STATE here; firmware
		 * already knows the new state (it requested it).  The event
		 * reply from bce_vhci_send_fw_event_reply in fwevt_task is
		 * the ack.  Sending a command from within fwevt_task would
		 * deadlock because the reply comes back on ev_commands (same
		 * taskqueue).
		 */
		if (was_paused_by_fw &&
		    tq->ctrl_state == BCE_VHCI_CTRL_DATA &&
		    tq->ctrl_dir == UE_DIR_IN) {
			/*
			 * Control IN data phase: re-submit on
			 * msg_asynchronous
			 */
			struct bce_vhci_message treq;
			struct bce_qe_submission *si;
			uint32_t dlen;

			dlen = tq->ctrl_data_len;
			if (dlen > BCE_VHCI_XFER_BUFSZ)
				dlen = BCE_VHCI_XFER_BUFSZ;

			memset(&treq, 0, sizeof(treq));
			treq.cmd = BCE_VHCI_CMD_TRANSFER_REQUEST;
			treq.param1 =
			    ((uint32_t)tq->endp_addr << 8) | tq->dev_addr;
			treq.param2 = dlen;

			bus_dmamap_sync(tq->dma_tag, tq->dma_map,
			    BUS_DMASYNC_PREREAD);

			/*
			 * Reserve msg slot first, then SQ --
			 * avoids orphaned SQ entry
			 */
			mtx_lock_spin(&vhci->sc_async_lock);
			if (bce_reserve_submission(
			    vhci->msg_asynchronous.sq) == 0) {
				mtx_unlock_spin(&vhci->sc_async_lock);

				mtx_lock_spin(&tq->lock);
				if (bce_reserve_submission(tq->sq_in) == 0) {
					si = bce_next_submission(tq->sq_in);
					si->addr = tq->dma_addr;
					si->length = dlen;
					si->segl_addr = 0;
					si->segl_length = 0;
					bce_submit_to_device(vhci->sc_bce,
					    tq->sq_in);
					mtx_unlock_spin(&tq->lock);

					mtx_lock_spin(&vhci->sc_async_lock);
					bce_vhci_msg_queue_write(vhci,
					    &vhci->msg_asynchronous, &treq);
					mtx_unlock_spin(&vhci->sc_async_lock);
				} else {
					mtx_unlock_spin(&tq->lock);
					mtx_lock_spin(&vhci->sc_async_lock);
					atomic_add_int(&vhci->
					    msg_asynchronous.sq->
					    available_commands, 1);
					mtx_unlock_spin(&vhci->sc_async_lock);
					device_printf(vhci->sc_dev,
					    "ctrl resume: SQ full\n");
					if (tq->active_xfer != NULL) {
						struct usb_xfer *ax;
						ax = tq->active_xfer;
						tq->active_xfer = NULL;
						tq->dma_inflight = 0;
						tq->ctrl_state =
						    BCE_VHCI_CTRL_IDLE;
						usbd_transfer_done(ax,
						    USB_ERR_IOERROR);
					}
				}
			} else {
				mtx_unlock_spin(&vhci->sc_async_lock);
				device_printf(vhci->sc_dev,
				    "ctrl resume: msg full\n");
				if (tq->active_xfer != NULL) {
					struct usb_xfer *ax;
					ax = tq->active_xfer;
					tq->active_xfer = NULL;
					tq->dma_inflight = 0;
					tq->ctrl_state = BCE_VHCI_CTRL_IDLE;
					usbd_transfer_done(ax,
					    USB_ERR_IOERROR);
				}
			}
		} else if (was_paused_by_fw &&
		    tq->endp_addr != 0x00 && (ep_addr & UE_DIR_IN) &&
		    tq->active_xfer != NULL) {
			/*
			 * Interrupt/bulk IN: re-submit after firmware
			 * PAUSE/ACTIVE
			 */
			struct bce_vhci_message treq;
			struct bce_qe_submission *si;
			uint32_t len;

			len = tq->active_xfer->frlengths[0];
			if (len > BCE_VHCI_XFER_BUFSZ)
				len = BCE_VHCI_XFER_BUFSZ;

			memset(&treq, 0, sizeof(treq));
			treq.cmd = BCE_VHCI_CMD_TRANSFER_REQUEST;
			treq.param1 =
			    ((uint32_t)tq->endp_addr << 8) | tq->dev_addr;
			treq.param2 = len;

			bus_dmamap_sync(tq->dma_tag, tq->dma_map,
			    BUS_DMASYNC_PREREAD);

			/*
			 * Reserve msg slot first, then SQ --
			 * avoids orphaned SQ entry
			 */
			mtx_lock_spin(&vhci->sc_async_lock);
			if (bce_reserve_submission(
			    vhci->msg_asynchronous.sq) == 0) {
				mtx_unlock_spin(&vhci->sc_async_lock);

				mtx_lock_spin(&tq->lock);
				if (bce_reserve_submission(tq->sq_in) == 0) {
					si = bce_next_submission(tq->sq_in);
					si->addr = tq->dma_addr;
					si->length = len;
					si->segl_addr = 0;
					si->segl_length = 0;
					bce_submit_to_device(vhci->sc_bce,
					    tq->sq_in);
					mtx_unlock_spin(&tq->lock);

					mtx_lock_spin(&vhci->sc_async_lock);
					bce_vhci_msg_queue_write(vhci,
					    &vhci->msg_asynchronous, &treq);
					mtx_unlock_spin(&vhci->sc_async_lock);
				} else {
					mtx_unlock_spin(&tq->lock);
					mtx_lock_spin(&vhci->sc_async_lock);
					atomic_add_int(&vhci->
					    msg_asynchronous.sq->
					    available_commands, 1);
					mtx_unlock_spin(&vhci->sc_async_lock);
					device_printf(vhci->sc_dev,
					    "IN resume: SQ full\n");
					if (tq->active_xfer != NULL) {
						struct usb_xfer *ax;
						ax = tq->active_xfer;
						tq->active_xfer = NULL;
						tq->dma_inflight = 0;
						usbd_transfer_done(ax,
						    USB_ERR_IOERROR);
					}
				}
			} else {
				mtx_unlock_spin(&vhci->sc_async_lock);
				device_printf(vhci->sc_dev,
				    "IN resume: msg full\n");
				if (tq->active_xfer != NULL) {
					struct usb_xfer *ax;
					ax = tq->active_xfer;
					tq->active_xfer = NULL;
					tq->dma_inflight = 0;
					usbd_transfer_done(ax,
					    USB_ERR_IOERROR);
				}
			}
		}
	} /* end ENDP_ACTIVE scope */
		break;
	case BCE_VHCI_ENDP_PAUSED:
		tq->paused_by |= BCE_VHCI_PAUSE_FIRMWARE;
		/*
		 * Do NOT send ENDPOINT_SET_STATE; same deadlock
		 * reason as ACTIVE above.  The event reply is the ack.
		 */
		/*
		 * Flush pending SQ submissions after PAUSE.
		 * Without this, our pre-submitted IN buffer stays in
		 * firmware's view and confuses the re-submit after RESUME.
		 *
		 * bce_cmd_flush_queue sleeps (waits on semaphore), so we
		 * must drop USB_BUS_LOCK before calling it.  QIDs are
		 * snapshotted under the lock; endpoint_destroy also takes
		 * the lock before starting teardown, so the qids remain
		 * valid with firmware while we flush.  A double-flush
		 * (here + endpoint_destroy) is harmless.
		 */
		if (tq->active) {
			struct apple_bce_softc *sc = vhci->sc_bce;
			int sq_in_qid  = (tq->sq_in  != NULL) ?
			    tq->sq_in->qid  : -1;
			int sq_out_qid = (tq->sq_out != NULL) ?
			    tq->sq_out->qid : -1;

			USB_BUS_UNLOCK(&vhci->sc_bus);
			if (sq_in_qid >= 0)
				bce_cmd_flush_queue(sc->sc_cmd_cmdq,
				    sc, sq_in_qid);
			if (sq_out_qid >= 0)
				bce_cmd_flush_queue(sc->sc_cmd_cmdq,
				    sc, sq_out_qid);
			USB_BUS_LOCK(&vhci->sc_bus);
		}
		break;
	default:
		USB_BUS_UNLOCK(&vhci->sc_bus);
		return (BCE_VHCI_BAD_ARGUMENT);
	}

	USB_BUS_UNLOCK(&vhci->sc_bus);
	return (BCE_VHCI_SUCCESS);
}

/*
 * Handle unsolicited ENDPOINT_SET_STATE (0x0042) from firmware.
 *
 * Firmware notifies us of a state change it initiated (e.g., stall
 * after a protocol error).
 *
 *   param1 = (ep_addr << 8) | dev_id
 *   param2 = new_state
 */
static uint16_t
bce_vhci_handle_endpoint_set_state(struct bce_vhci_softc *vhci,
    struct bce_vhci_message *msg)
{
	struct bce_vhci_transfer_queue *tq;
	uint8_t dev_id, ep_addr;
	uint32_t new_state;

	dev_id = msg->param1 & 0xFF;
	ep_addr = (msg->param1 >> 8) & 0xFF;
	new_state = (uint32_t)msg->param2;

	device_printf(vhci->sc_dev,
	    "ENDPOINT_SET_STATE: dev=%d ep=0x%02x state=%u\n",
	    dev_id, ep_addr, new_state);

	tq = bce_vhci_find_tq(vhci, dev_id, ep_addr);
	if (tq == NULL)
		return (BCE_VHCI_BAD_ARGUMENT);

	switch (new_state) {
	case BCE_VHCI_ENDP_STALLED:
		/*
		 * USB_BUS_LOCK protects stalled against concurrent access
		 * from pipe_start, pipe_close, and the endpoint_req_state path.
		 */
		USB_BUS_LOCK(&vhci->sc_bus);
		tq->stalled = 1;
		USB_BUS_UNLOCK(&vhci->sc_bus);
		/*
		 * Do not touch active_xfer here; this runs from
		 * taskqueue while ISR may be using it.  The stall
		 * will be reported via CTRL_TRANSFER_STATUS(STALL)
		 * from firmware on the ISR path.
		 */
		return (BCE_VHCI_SUCCESS);
	default:
		return (BCE_VHCI_BAD_ARGUMENT);
	}
}

/*
 * QID bitmap allocator (internal, caller must hold sc_queues_lock).
 * BCE_MAX_QUEUE_COUNT = 256 = 8 * 32 bits.
 * Bit set means QID is in use.
 */
static int
bce_vhci_alloc_qid_locked(struct bce_vhci_softc *vhci)
{
	struct apple_bce_softc *sc __unused = vhci->sc_bce;
	int i, bit;

	mtx_assert(&sc->sc_queues_lock, MA_OWNED);
	for (i = 0; i < 8; i++) {
		if (vhci->sc_qid_bitmap[i] == 0xFFFFFFFF)
			continue;
		bit = ffs(~vhci->sc_qid_bitmap[i]) - 1;
		vhci->sc_qid_bitmap[i] |= (1u << bit);
		return (i * 32 + bit);
	}
	return (-1);
}

static int
bce_vhci_alloc_qid(struct bce_vhci_softc *vhci)
{
	struct apple_bce_softc *sc = vhci->sc_bce;
	int qid;

	mtx_lock(&sc->sc_queues_lock);
	qid = bce_vhci_alloc_qid_locked(vhci);
	mtx_unlock(&sc->sc_queues_lock);
	return (qid);
}

/*
 * Allocate next queue ID pair (CQ + SQ).
 * Returns the CQ qid; SQ qid = CQ qid + 1.
 * Finds two consecutive free bits in the bitmap.
 */
static int
bce_vhci_alloc_qid_pair(struct bce_vhci_softc *vhci)
{
	struct apple_bce_softc *sc = vhci->sc_bce;
	int qid, j;

	mtx_lock(&sc->sc_queues_lock);

	/* Find first free QID and check the next one is also free */
	qid = bce_vhci_alloc_qid_locked(vhci);
	if (qid < 0) {
		mtx_unlock(&sc->sc_queues_lock);
		return (-1);
	}
	if (qid + 1 >= BCE_MAX_QUEUE_COUNT) {
		/* Free the one we just allocated */
		vhci->sc_qid_bitmap[qid / 32] &= ~(1u << (qid % 32));
		mtx_unlock(&sc->sc_queues_lock);
		return (-1);
	}
	/* Check next QID is free */
	if (vhci->sc_qid_bitmap[(qid + 1) / 32] & (1u << ((qid + 1) % 32))) {
		/* Next is taken; free qid and search for consecutive pair */
		vhci->sc_qid_bitmap[qid / 32] &= ~(1u << (qid % 32));
		/* Brute-force search for consecutive pair */
		for (j = BCE_QUEUE_USER_MIN; j < BCE_MAX_QUEUE_COUNT - 1; j++) {
			uint32_t w0 = vhci->sc_qid_bitmap[j / 32];
			uint32_t w1 = vhci->sc_qid_bitmap[(j + 1) / 32];
			int b0 = j % 32;
			int b1 = (j + 1) % 32;

			if ((w0 & (1u << b0)) == 0 &&
			    (w1 & (1u << b1)) == 0) {
				vhci->sc_qid_bitmap[j / 32] |= (1u << b0);
				vhci->sc_qid_bitmap[(j + 1) / 32] |=
				    (1u << b1);
				mtx_unlock(&sc->sc_queues_lock);
				return (j);
			}
		}
		mtx_unlock(&sc->sc_queues_lock);
		return (-1);
	}
	/* Next QID is free; allocate it */
	vhci->sc_qid_bitmap[(qid + 1) / 32] |= (1u << ((qid + 1) % 32));
	mtx_unlock(&sc->sc_queues_lock);
	return (qid);
}

static void
bce_vhci_free_qid(struct bce_vhci_softc *vhci, int qid)
{
	struct apple_bce_softc *sc = vhci->sc_bce;

	if (qid < 0 || qid >= BCE_MAX_QUEUE_COUNT)
		return;
	mtx_lock(&sc->sc_queues_lock);
	vhci->sc_qid_bitmap[qid / 32] &= ~(1u << (qid % 32));
	mtx_unlock(&sc->sc_queues_lock);
}

static int
bce_vhci_create_queues(struct bce_vhci_softc *vhci)
{
	struct apple_bce_softc *sc = vhci->sc_bce;
	struct bce_queue_memcfg cfg;
	uint32_t status;
	int error, qid_pair, q, i;

	/* Initialize QID bitmap: mark QIDs 0..BCE_QUEUE_USER_MIN-1 as used */
	memset(vhci->sc_qid_bitmap, 0, sizeof(vhci->sc_qid_bitmap));
	for (q = 0; q < BCE_QUEUE_USER_MIN; q++)
		vhci->sc_qid_bitmap[q / 32] |= (1u << (q % 32));

	/* Initialize command queue locks early (destroy_queues expects them) */
	sx_init(&vhci->cmd.exec_lock, "bce_vhci_cmdex");
	mtx_init(&vhci->cmd.lock, "bce_vhci_cmd", NULL, MTX_SPIN);
	sema_init(&vhci->cmd.completion, 0, "bce_vhci_cmd");
	vhci->cmd.pending = 0;

	/*
	 * Create 5 message queues (host -> device).
	 * Each gets its own CQ + SQ pair.
	 */
#define CREATE_MSG_QUEUE(field, name)					\
	do {								\
		qid_pair = bce_vhci_alloc_qid_pair(vhci);		\
		if (qid_pair < 0) {					\
			error = ENOMEM;					\
			device_printf(vhci->sc_dev,			\
			    "queue IDs exhausted for %s\n", name);	\
			goto fail;					\
		}							\
		error = bce_vhci_msg_queue_create(vhci, &vhci->field,	\
		    name, qid_pair, qid_pair + 1,			\
		    bce_vhci_msg_queue_completion, &vhci->field);	\
		if (error != 0) {					\
			device_printf(vhci->sc_dev,			\
			    "failed to create %s: %d\n", name, error);	\
			goto fail;					\
		}							\
	} while (0)

	CREATE_MSG_QUEUE(msg_commands,    "VHC1HostCommands");
	CREATE_MSG_QUEUE(msg_system,      "VHC1HostSystemEvents");
	CREATE_MSG_QUEUE(msg_isochronous, "VHC1HostIsochronousEvents");
	CREATE_MSG_QUEUE(msg_interrupt,   "VHC1HostInterruptEvents");
	CREATE_MSG_QUEUE(msg_asynchronous, "VHC1HostAsynchronousEvents");
#undef CREATE_MSG_QUEUE

	/*
	 * Create shared event CQ (one CQ for all 5 event queues).
	 */
	{
		int ev_cq_qid = bce_vhci_alloc_qid(vhci);

		if (ev_cq_qid < 0) {
			device_printf(vhci->sc_dev,
			    "queue IDs exhausted for event CQ\n");
			error = ENOMEM;
			goto fail;
		}

		vhci->ev_cq = bce_alloc_cq(sc, ev_cq_qid,
		    BCE_VHCI_EVT_QUEUE_EL);
		if (vhci->ev_cq == NULL) {
			bce_vhci_free_qid(vhci, ev_cq_qid);
			error = ENOMEM;
			goto fail;
		}

		bce_get_cq_memcfg(vhci->ev_cq, &cfg);
		cfg.vector_or_cq = 4;
		status = bce_cmd_register_queue(sc->sc_cmd_cmdq, sc,
		    &cfg, NULL, 0);
		if (status != 0) {
			device_printf(vhci->sc_dev,
			    "failed to register event CQ: %u\n", status);
			bce_free_cq(sc, vhci->ev_cq);
			vhci->ev_cq = NULL;
			bce_vhci_free_qid(vhci, ev_cq_qid);
			error = EIO;
			goto fail;
		}

		mtx_lock(&sc->sc_queues_lock);
		sc->sc_queues[ev_cq_qid] = vhci->ev_cq;
		for (i = 0; i < BCE_MAX_CQ_COUNT; i++) {
			if (sc->sc_cq_list[i] == NULL) {
				sc->sc_cq_list[i] = vhci->ev_cq;
				break;
			}
		}
		if (i == BCE_MAX_CQ_COUNT) {
			sc->sc_queues[ev_cq_qid] = NULL;
			mtx_unlock(&sc->sc_queues_lock);
			device_printf(vhci->sc_dev,
			    "CQ list full for event CQ\n");
			bce_cmd_unregister_queue(sc->sc_cmd_cmdq, sc,
			    ev_cq_qid);
			bce_free_cq(sc, vhci->ev_cq);
			vhci->ev_cq = NULL;
			bce_vhci_free_qid(vhci, ev_cq_qid);
			error = ENOSPC;
			goto fail;
		}
		mtx_unlock(&sc->sc_queues_lock);
	}

	/*
	 * Create 5 event queues (device -> host), all sharing ev_cq.
	 */
#define CREATE_EVT_QUEUE(field, name, fn)				\
	do {								\
		int eq_qid = bce_vhci_alloc_qid(vhci);			\
		if (eq_qid < 0) {					\
			device_printf(vhci->sc_dev,			\
			    "queue IDs exhausted for %s\n", name);	\
			error = ENOMEM;					\
			goto fail;					\
		}							\
		error = bce_vhci_evt_queue_create(vhci, &vhci->field,	\
		    name, eq_qid, fn);					\
		if (error != 0) {					\
			device_printf(vhci->sc_dev,			\
			    "failed to create %s: %d\n", name, error);	\
			bce_vhci_free_qid(vhci, eq_qid);		\
			goto fail;					\
		}							\
	} while (0)

	CREATE_EVT_QUEUE(ev_commands,    "VHC1FirmwareCommands",
	    bce_vhci_ev_cmd_completion);
	CREATE_EVT_QUEUE(ev_system,      "VHC1FirmwareSystemEvents",
	    bce_vhci_ev_system_completion);
	CREATE_EVT_QUEUE(ev_isochronous, "VHC1FirmwareIsochronousEvents",
	    bce_vhci_ev_generic_completion);
	CREATE_EVT_QUEUE(ev_interrupt,   "VHC1FirmwareInterruptEvents",
	    bce_vhci_ev_generic_completion);
	CREATE_EVT_QUEUE(ev_asynchronous, "VHC1FirmwareAsynchronousEvents",
	    bce_vhci_ev_generic_completion);
#undef CREATE_EVT_QUEUE

	/* Wire command queue to its message queue */
	vhci->cmd.msg = &vhci->msg_commands;

	device_printf(vhci->sc_dev, "VHCI queues created\n");
	return (0);

fail:
	bce_vhci_destroy_queues(vhci);
	return (error);
}

static void
bce_vhci_destroy_queues(struct bce_vhci_softc *vhci)
{
	struct apple_bce_softc *sc = vhci->sc_bce;
	int i;

	/* Destroy event queues first (they may deliver cmd replies) */
	bce_vhci_evt_queue_destroy(vhci, &vhci->ev_asynchronous);
	bce_vhci_evt_queue_destroy(vhci, &vhci->ev_interrupt);
	bce_vhci_evt_queue_destroy(vhci, &vhci->ev_isochronous);
	bce_vhci_evt_queue_destroy(vhci, &vhci->ev_system);
	bce_vhci_evt_queue_destroy(vhci, &vhci->ev_commands);

	/* Destroy command queue state after events are drained */
	sema_destroy(&vhci->cmd.completion);
	if (mtx_initialized(&vhci->cmd.lock))
		mtx_destroy(&vhci->cmd.lock);
	sx_destroy(&vhci->cmd.exec_lock);

	/* Destroy shared event CQ */
	if (vhci->ev_cq != NULL) {
		bce_cmd_unregister_queue(sc->sc_cmd_cmdq, sc,
		    vhci->ev_cq->qid);
		mtx_lock(&sc->sc_queues_lock);
		sc->sc_queues[vhci->ev_cq->qid] = NULL;
		for (i = 0; i < BCE_MAX_CQ_COUNT; i++) {
			if (sc->sc_cq_list[i] == vhci->ev_cq) {
				sc->sc_cq_list[i] = NULL;
				break;
			}
		}
		mtx_unlock(&sc->sc_queues_lock);
		bce_free_cq(sc, vhci->ev_cq);
		vhci->ev_cq = NULL;
	}

	/* Destroy message queues */
	bce_vhci_msg_queue_destroy(vhci, &vhci->msg_asynchronous);
	bce_vhci_msg_queue_destroy(vhci, &vhci->msg_interrupt);
	bce_vhci_msg_queue_destroy(vhci, &vhci->msg_isochronous);
	bce_vhci_msg_queue_destroy(vhci, &vhci->msg_system);
	bce_vhci_msg_queue_destroy(vhci, &vhci->msg_commands);
}

static int
bce_vhci_start_controller(struct bce_vhci_softc *vhci)
{
	struct bce_vhci_message cmd, reply;
	uint16_t port_mask;
	uint8_t port_count;
	uint32_t port_status;
	int error;
	int i;

	/*
	 * CONTROLLER_ENABLE: param1 = 0x7100 | bus_number(1)
	 * Reply param2 = port bitmask
	 */
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd = BCE_VHCI_CMD_CONTROLLER_ENABLE;
	cmd.param1 = 0x7100 | 1;

	error = bce_vhci_cmd_execute(vhci, &cmd, &reply,
	    BCE_VHCI_CMD_TIMEOUT_LONG);
	if (error != 0) {
		device_printf(vhci->sc_dev,
		    "CONTROLLER_ENABLE failed: %d\n", error);
		return (error);
	}

	port_mask = (uint16_t)reply.param2;
	vhci->sc_port_mask = port_mask;

	/* Count ports from mask */
	port_count = 0;
	for (i = 0; i < BCE_VHCI_MAX_PORTS; i++) {
		if (port_mask & (1u << i))
			port_count = i + 1;
	}
	vhci->sc_port_count = port_count;

	device_printf(vhci->sc_dev,
	    "controller enabled: port_mask=0x%x, %d ports\n",
	    port_mask, port_count);

	/*
	 * CONTROLLER_START
	 */
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd = BCE_VHCI_CMD_CONTROLLER_START;

	error = bce_vhci_cmd_execute(vhci, &cmd, &reply,
	    BCE_VHCI_CMD_TIMEOUT_LONG);
	if (error != 0) {
		device_printf(vhci->sc_dev,
		    "CONTROLLER_START failed: %d\n", error);
		/* Disable the controller we just enabled */
		memset(&cmd, 0, sizeof(cmd));
		cmd.cmd = BCE_VHCI_CMD_CONTROLLER_DISABLE;
		bce_vhci_cmd_execute(vhci, &cmd, &reply,
		    BCE_VHCI_CMD_TIMEOUT_LONG);
		return (error);
	}

	vhci->sc_started = 1;

	/*
	 * Power on each port and read initial status.
	 */
	for (i = 0; i < port_count; i++) {
		if ((port_mask & (1u << i)) == 0)
			continue;

		memset(&cmd, 0, sizeof(cmd));
		cmd.cmd = BCE_VHCI_CMD_PORT_POWER_ON;
		cmd.param1 = i;

		error = bce_vhci_cmd_execute(vhci, &cmd, &reply,
		    BCE_VHCI_CMD_TIMEOUT_SHORT);
		if (error != 0) {
			device_printf(vhci->sc_dev,
			    "PORT_POWER_ON(%d) failed: %d\n", i, error);
			continue;
		}
		vhci->sc_port_power[i] = 1;

		/* Read initial port status */
		memset(&cmd, 0, sizeof(cmd));
		cmd.cmd = BCE_VHCI_CMD_PORT_STATUS;
		cmd.param1 = i;

		error = bce_vhci_cmd_execute(vhci, &cmd, &reply,
		    BCE_VHCI_CMD_TIMEOUT_SHORT);
		if (error != 0) {
			device_printf(vhci->sc_dev,
			    "PORT_STATUS(%d) failed: %d\n", i, error);
			continue;
		}

		port_status = (uint32_t)reply.param2;
		device_printf(vhci->sc_dev,
		    "port %d: raw_status=0x%x\n", i, port_status);

		/*
		 * Translate firmware port status to USB status bits.
		 */
		vhci->sc_port_status[i] = UPS_PORT_POWER;
		if (port_status & BCE_VHCI_PORT_ENABLED)
			vhci->sc_port_status[i] |=
			    UPS_PORT_ENABLED | UPS_HIGH_SPEED;
		if (port_status & BCE_VHCI_PORT_CONNECTED)
			vhci->sc_port_status[i] |=
			    UPS_CURRENT_CONNECT_STATUS;
		if (port_status & BCE_VHCI_PORT_SUSPENDED)
			vhci->sc_port_status[i] |= UPS_SUSPEND;
		if (port_status & BCE_VHCI_PORT_OVERCURRENT)
			vhci->sc_port_status[i] |=
			    UPS_OVERCURRENT_INDICATOR;

		if (vhci->sc_port_status[i] & UPS_CURRENT_CONNECT_STATUS)
			vhci->sc_port_change[i] |= UPS_C_CONNECT_STATUS;
	}

	return (0);
}

static usb_error_t
bce_vhci_roothub_exec(struct usb_device *udev,
    struct usb_device_request *req, const void **pptr, uint16_t *plength)
{
	struct bce_vhci_softc *vhci;
	const void *ptr;
	uint16_t len;
	uint16_t value, index;
	usb_error_t err;

	vhci = (struct bce_vhci_softc *)udev->bus;
	USB_BUS_LOCK_ASSERT(&vhci->sc_bus, MA_OWNED);

	ptr = NULL;
	len = 0;
	err = 0;

	value = UGETW(req->wValue);
	index = UGETW(req->wIndex);

	switch (req->bRequest) {
	case UR_CLEAR_FEATURE:
		switch (req->bmRequestType) {
		case UT_WRITE_CLASS_OTHER:
			/* ClearPortFeature */
			if (index < 1 || index > vhci->sc_port_count) {
				err = USB_ERR_IOERROR;
				break;
			}
			switch (value) {
			case UHF_C_PORT_CONNECTION:
				vhci->sc_port_change[index - 1] &=
				    ~UPS_C_CONNECT_STATUS;
				break;
			case UHF_C_PORT_ENABLE:
				vhci->sc_port_change[index - 1] &=
				    ~UPS_C_PORT_ENABLED;
				break;
			case UHF_C_PORT_RESET:
				vhci->sc_port_change[index - 1] &=
				    ~UPS_C_PORT_RESET;
				break;
			case UHF_C_PORT_OVER_CURRENT:
				vhci->sc_port_change[index - 1] &=
				    ~UPS_C_OVERCURRENT_INDICATOR;
				break;
			case UHF_C_PORT_SUSPEND:
				vhci->sc_port_change[index - 1] &=
				    ~UPS_C_SUSPEND;
				break;
			case UHF_PORT_ENABLE:
				vhci->sc_port_status[index - 1] &=
				    ~UPS_PORT_ENABLED;
				break;
			case UHF_PORT_SUSPEND:
				vhci->sc_port_status[index - 1] &=
				    ~UPS_SUSPEND;
				break;
			case UHF_PORT_POWER:
				if (vhci->sc_port_power[index - 1]) {
					struct bce_vhci_message cmd_pw;
					struct bce_vhci_message reply_pw;
					int pw_port = index - 1;

					memset(&cmd_pw, 0, sizeof(cmd_pw));
					cmd_pw.cmd =
					    BCE_VHCI_CMD_PORT_POWER_OFF;
					cmd_pw.param1 = pw_port;

					USB_BUS_UNLOCK(&vhci->sc_bus);
					if (bce_vhci_cmd_execute(vhci, &cmd_pw,
					    &reply_pw,
					    BCE_VHCI_CMD_TIMEOUT_SHORT) != 0) {
						device_printf(vhci->sc_dev,
						    "PORT_POWER_OFF(%d)"
						    " failed\n", pw_port);
						USB_BUS_LOCK(&vhci->sc_bus);
						err = USB_ERR_IOERROR;
						break;
					}
					USB_BUS_LOCK(&vhci->sc_bus);
					vhci->sc_port_power[pw_port] = 0;
					vhci->sc_port_status[pw_port] = 0;
				}
				break;
			default:
				err = USB_ERR_IOERROR;
				break;
			}
			break;
		default:
			err = USB_ERR_IOERROR;
			break;
		}
		break;

	case UR_GET_DESCRIPTOR:
		if (req->bmRequestType == UT_READ_CLASS_DEVICE) {
			/* Hub descriptor (USB 2.0) */
			struct usb_hub_descriptor hd;
			uint8_t nports = vhci->sc_port_count;
			uint8_t padsz = (nports + 7) / 8;

			memset(&hd, 0, sizeof(hd));
			hd.bDescLength = 7 + 2 * padsz;
			hd.bDescriptorType = UDESC_HUB;
			hd.bNbrPorts = nports;
			USETW(hd.wHubCharacteristics,
			    UHD_PWR_INDIVIDUAL);
			hd.bPwrOn2PwrGood = 50;
			len = hd.bDescLength;
			if (len > sizeof(vhci->sc_hub_idata))
				len = sizeof(vhci->sc_hub_idata);
			memcpy(vhci->sc_hub_idata, &hd, len);
			ptr = vhci->sc_hub_idata;
			break;
		}
		switch (value >> 8) {
		case UDESC_DEVICE:
			if ((value & 0xff) != 0) {
				err = USB_ERR_IOERROR;
				break;
			}
			len = sizeof(bce_vhci_devd);
			ptr = &bce_vhci_devd;
			break;
		case UDESC_DEVICE_QUALIFIER:
			if ((value & 0xff) != 0) {
				err = USB_ERR_IOERROR;
				break;
			}
			len = sizeof(bce_vhci_odevd);
			ptr = &bce_vhci_odevd;
			break;
		case UDESC_CONFIG:
			if ((value & 0xff) != 0) {
				err = USB_ERR_IOERROR;
				break;
			}
			len = sizeof(bce_vhci_confd);
			ptr = bce_vhci_confd;
			break;
		case UDESC_STRING:
			switch (value & 0xff) {
			case 0:	/* Language */
				ptr = "\x04\x03\x09\x04";
				len = 4;
				break;
			case 1:	/* Vendor */
				ptr = "\x0c\x03\x41\x00\x70\x00\x70\x00"
				      "\x6c\x00\x65\x00";
				len = 12;
				break;
			case 2:	/* Product */
				ptr = "\x1a\x03\x54\x00\x32\x00\x20\x00"
				      "\x42\x00\x43\x00\x45\x00\x20\x00"
				      "\x56\x00\x48\x00\x43\x00\x49\x00";
				len = 26;
				break;
			default:
				err = USB_ERR_IOERROR;
				break;
			}
			break;
		default:
			err = USB_ERR_IOERROR;
			break;
		}
		break;

	case UR_GET_INTERFACE:
		len = 1;
		ptr = "\x00";	/* alt setting 0 */
		break;

	case UR_GET_STATUS:
		switch (req->bmRequestType) {
		case UT_READ_CLASS_OTHER:
		{
			/* GetPortStatus */
			struct usb_port_status ps;
			uint16_t port;

			if (index < 1 || index > vhci->sc_port_count) {
				err = USB_ERR_IOERROR;
				break;
			}
			port = index - 1;

			USETW(ps.wPortStatus,
			    vhci->sc_port_status[port]);
			USETW(ps.wPortChange,
			    vhci->sc_port_change[port]);

			len = sizeof(ps);
			memcpy(&vhci->sc_hub_idata, &ps, sizeof(ps));
			ptr = &vhci->sc_hub_idata;
			break;
		}
		case UT_READ_CLASS_DEVICE:
		{
			/* GetHubStatus */
			len = 4;
			ptr = "\x00\x00\x00\x00";
			break;
		}
		case UT_READ_DEVICE:
		{
			len = 2;
			ptr = "\x01\x00";	/* self-powered */
			break;
		}
		default:
			err = USB_ERR_IOERROR;
			break;
		}
		break;

	case UR_SET_ADDRESS:
		if (value >= BCE_VHCI_MAX_DEVICES) {
			err = USB_ERR_IOERROR;
			break;
		}
		break;

	case UR_SET_CONFIG:
	case UR_SET_INTERFACE:
		break;

	case UR_SET_FEATURE:
		switch (req->bmRequestType) {
		case UT_WRITE_CLASS_OTHER:
			/* SetPortFeature */
			if (index < 1 || index > vhci->sc_port_count) {
				err = USB_ERR_IOERROR;
				break;
			}
			switch (value) {
			case UHF_PORT_POWER:
				if (vhci->sc_port_power[index - 1] == 0) {
					struct bce_vhci_message cmd_pw;
					struct bce_vhci_message reply_pw;
					int pw_port = index - 1;
					int pw_err;

					memset(&cmd_pw, 0, sizeof(cmd_pw));
					cmd_pw.cmd = BCE_VHCI_CMD_PORT_POWER_ON;
					cmd_pw.param1 = pw_port;

					USB_BUS_UNLOCK(&vhci->sc_bus);
					pw_err = bce_vhci_cmd_execute(vhci,
					    &cmd_pw, &reply_pw,
					    BCE_VHCI_CMD_TIMEOUT_SHORT);
					USB_BUS_LOCK(&vhci->sc_bus);
					if (pw_err != 0) {
						device_printf(vhci->sc_dev,
						    "PORT_POWER_ON(%d)"
						    " failed\n", pw_port);
						err = USB_ERR_IOERROR;
					} else {
						vhci->sc_port_power[pw_port] =
						    1;
						vhci->sc_port_status[pw_port] |=
						    UPS_PORT_POWER;
					}
				}
				break;
			case UHF_PORT_RESET:
			{
				int reset_err;

				vhci->sc_port_status[index - 1] |=
				    UPS_RESET;

				/*
				 * Drop bus lock for firmware I/O.
				 * Explore thread is single-threaded
				 * so this is safe.
				 */
				USB_BUS_UNLOCK(&vhci->sc_bus);

				/* Destroy existing device before re-creating */
				bce_vhci_device_destroy(vhci, index - 1);

				reset_err = bce_vhci_device_create(vhci,
				    index - 1);
				USB_BUS_LOCK(&vhci->sc_bus);

				vhci->sc_port_status[index - 1] &=
				    ~UPS_RESET;
				if (reset_err == 0) {
					vhci->sc_port_status[index - 1] |=
					    UPS_PORT_ENABLED |
					    UPS_HIGH_SPEED;
					vhci->sc_port_change[index - 1] |=
					    UPS_C_PORT_RESET;
				} else {
					device_printf(vhci->sc_dev,
					    "port %d reset failed: %d\n",
					    index, reset_err);
					err = USB_ERR_IOERROR;
				}
				break;
			}
			case UHF_PORT_ENABLE:
				vhci->sc_port_status[index - 1] |=
				    UPS_PORT_ENABLED;
				break;
			case UHF_PORT_SUSPEND:
				vhci->sc_port_status[index - 1] |=
				    UPS_SUSPEND;
				break;
			default:
				err = USB_ERR_IOERROR;
				break;
			}
			break;
		default:
			err = USB_ERR_IOERROR;
			break;
		}
		break;

	case UR_GET_CONFIG:
		len = 1;
		ptr = "\x01";	/* config 1 */
		break;

	default:
		err = USB_ERR_IOERROR;
		break;
	}

	if (err == 0) {
		if (pptr != NULL)
			*pptr = ptr;
		if (plength != NULL)
			*plength = len;
	}
	return (err);
}

static void
bce_vhci_endpoint_init(struct usb_device *udev,
    struct usb_endpoint_descriptor *edesc, struct usb_endpoint *ep)
{

	ep->methods = &bce_vhci_pipe_methods;
}

static void
bce_vhci_xfer_setup(struct usb_setup_params *parm)
{
	struct usb_xfer *xfer = parm->curr_xfer;

	parm->hc_max_packet_size = 0x400;	/* 1024 */
	parm->hc_max_packet_count = 1;
	parm->hc_max_frame_size = BCE_VHCI_XFER_BUFSZ;

	usbd_transfer_setup_sub(parm);

	if (parm->err)
		return;

	/* No HCD-specific TD/QH structures needed */
	xfer->flags_int.bdma_enable = 0;
}

static void
bce_vhci_xfer_unsetup(struct usb_xfer *xfer)
{
	/* Nothing to free */
}

static void
bce_vhci_get_dma_delay(struct usb_device *udev, uint32_t *pus)
{

	*pus = 0;	/* No hardware DMA delay */
}

static void
bce_vhci_pipe_open(struct usb_xfer *xfer)
{
	/* Nothing to do; endpoint resources managed elsewhere */
}

static void
bce_vhci_pipe_close(struct usb_xfer *xfer)
{
	struct bce_vhci_softc *vhci;
	int i;

	vhci = (struct bce_vhci_softc *)xfer->xroot->bus;

	/*
	 * If this xfer is the active transfer on any endpoint,
	 * clear it.  We do not flush the firmware SQ here because
	 * USB_BUS_LOCK is held (cannot sleep).  Stale SQ completions
	 * are discarded in tq_completion (active_xfer == NULL check).
	 */
	for (i = 0; i < BCE_VHCI_MAX_DEVICES; i++) {
		struct bce_vhci_device *dev = &vhci->sc_devs[i];
		int j;

		if (dev->allocated == 0)
			continue;
		for (j = 0; j < BCE_VHCI_MAX_ENDPOINTS; j++) {
			struct bce_vhci_transfer_queue *tq = &dev->tq[j];

			if (tq->active_xfer == xfer) {
				tq->active_xfer = NULL;
				tq->ctrl_state = BCE_VHCI_CTRL_IDLE;
				tq->ctrl_data_done = 0;
				tq->ctrl_status_pending = 0;
				tq->evt_pending = 0;
			}
			if (tq->pending_xfer == xfer)
				tq->pending_xfer = NULL;
			if (tq->create_xfer == xfer) {
				tq->create_xfer = NULL;
				tq->create_pending = 0;
			}
		}
	}

	/* Cancel any pending transfer */
	if (xfer->flags_int.transferring) {
		usbd_transfer_done(xfer, USB_ERR_CANCELLED);
	}
}

static void
bce_vhci_pipe_enter(struct usb_xfer *xfer)
{
	/* Called before start, can validate */
}

/*
 * Start a transfer.
 *
 * Called with USB_BUS_LOCK held.  For control transfers, we parse the
 * setup packet, record the direction and data length, set the
 * endpoint state machine to SETUP, and wait for firmware
 * TRANSFER_REQUEST events to drive the transfer forward.
 *
 * For interrupt/bulk, we STALL for now (not yet implemented).
 */
static void
bce_vhci_pipe_start(struct usb_xfer *xfer)
{
	struct bce_vhci_softc *vhci;
	struct bce_vhci_device *dev;
	struct bce_vhci_transfer_queue *tq;
	struct usb_device_request setup;
	uint8_t xfer_type;
	uint8_t ep_addr;

	vhci = (struct bce_vhci_softc *)xfer->xroot->bus;
	xfer_type = xfer->endpoint->edesc->bmAttributes & UE_XFERTYPE;
	ep_addr = xfer->endpointno;

	if (xfer_type == UE_INTERRUPT || xfer_type == UE_BULK) {
		struct usb_device *udev = xfer->xroot->udev;
		struct bce_vhci_message treq;
		uint8_t port, fw_dev_id, ep_idx;
		uint32_t len;

		port = udev->port_no;
		if (port < 1 || port > vhci->sc_port_count) {
			usbd_transfer_done(xfer, USB_ERR_STALLED);
			return;
		}

		fw_dev_id = vhci->sc_port_to_dev[port - 1];
		if (fw_dev_id >= BCE_VHCI_MAX_DEVICES) {
			usbd_transfer_done(xfer, USB_ERR_STALLED);
			return;
		}

		dev = &vhci->sc_devs[fw_dev_id];
		if (dev->allocated == 0) {
			usbd_transfer_done(xfer, USB_ERR_STALLED);
			return;
		}

		ep_idx = bce_vhci_ep_index(ep_addr);
		tq = &dev->tq[ep_idx];

		/* Create endpoint with firmware if not yet active */
		if (tq->active == 0) {
			/*
			 * Cannot sleep here: we may be in a USB callback
			 * (e.g. usbhid_intr_in_callback) holding a
			 * non-sleepable lock.  Defer to taskqueue_thread
			 * and return STALLED so the USB stack retries.
			 */
			if (vhci->sc_detaching == 0 &&
			    tq->create_xfer == NULL) {
				tq->endp_addr = ep_addr;
				tq->dev_addr = fw_dev_id;
				tq->create_pending = 1;
				tq->create_edesc = xfer->endpoint->edesc;
				/* held; task submits it */
				tq->create_xfer = xfer;
				taskqueue_enqueue(taskqueue_thread,
				    &vhci->sc_create_task);
			} else {
				usbd_transfer_done(xfer, USB_ERR_STALLED);
			}
			return;
		}

		if (tq->active_xfer != NULL || tq->dma_inflight != 0) {
			/*
			 * Pipeline: queue xfer for when active_xfer finishes.
			 * Also queue if old DMA is still inflight (pipe_close
			 * cleared active_xfer but completion not yet seen).
			 * Returning STALLED triggers stall recovery
			 * (CLEAR_FEATURE loop); hold the xfer instead.
			 */
			if (tq->pending_xfer == NULL)
				tq->pending_xfer = xfer;
			else
				usbd_transfer_done(xfer, USB_ERR_CANCELLED);
			return;
		}

		tq->active_xfer = xfer;
		tq->dma_inflight = 1;

		/*
		 * If firmware already sent a TRANSFER_REQUEST before
		 * the USB stack called pipe_start, replay it now
		 * instead of sending a duplicate host request.
		 */
		if (tq->evt_pending) {
			tq->evt_pending = 0;
			bce_vhci_handle_transfer_request(vhci,
			    &tq->evt_saved);
			return;
		}

		if (ep_addr & UE_DIR_IN) {
			/* IN transfer: reserve msg first, then SQ */
			struct bce_qe_submission *si;

			len = xfer->frlengths[0];
			if (len > BCE_VHCI_XFER_BUFSZ)
				len = BCE_VHCI_XFER_BUFSZ;

			bus_dmamap_sync(tq->dma_tag, tq->dma_map,
			    BUS_DMASYNC_PREREAD);

			memset(&treq, 0, sizeof(treq));
			treq.cmd = BCE_VHCI_CMD_TRANSFER_REQUEST;
			treq.param1 = (ep_addr << 8) | tq->dev_addr;
			treq.param2 = len;

			/* Reserve msg slot first */
			mtx_lock_spin(&vhci->sc_async_lock);
			if (bce_reserve_submission(
			    vhci->msg_asynchronous.sq) != 0) {
				mtx_unlock_spin(&vhci->sc_async_lock);
				tq->active_xfer = NULL;
				tq->dma_inflight = 0;
				usbd_transfer_done(xfer, USB_ERR_IOERROR);
				return;
			}
			mtx_unlock_spin(&vhci->sc_async_lock);

			/* Then reserve SQ slot */
			mtx_lock_spin(&tq->lock);
			if (bce_reserve_submission(tq->sq_in) != 0) {
				mtx_unlock_spin(&tq->lock);
				/* Restore msg slot reserved but won't use */
				mtx_lock_spin(&vhci->sc_async_lock);
				atomic_add_int(&vhci->
				    msg_asynchronous.sq->
				    available_commands, 1);
				mtx_unlock_spin(&vhci->sc_async_lock);
				tq->active_xfer = NULL;
				tq->dma_inflight = 0;
				usbd_transfer_done(xfer, USB_ERR_IOERROR);
				return;
			}

			si = bce_next_submission(tq->sq_in);
			si->addr = tq->dma_addr;
			si->length = len;
			si->segl_addr = 0;
			si->segl_length = 0;
			bce_submit_to_device(vhci->sc_bce, tq->sq_in);
			mtx_unlock_spin(&tq->lock);

			mtx_lock_spin(&vhci->sc_async_lock);
			bce_vhci_msg_queue_write(vhci,
			    &vhci->msg_asynchronous, &treq);
			mtx_unlock_spin(&vhci->sc_async_lock);
		} else {
			/* OUT transfer: reserve msg first, then SQ */
			struct bce_qe_submission *so;

			len = xfer->frlengths[0];
			if (len > BCE_VHCI_XFER_BUFSZ)
				len = BCE_VHCI_XFER_BUFSZ;

			usbd_copy_out(&xfer->frbuffers[0], 0,
			    tq->dma_buf, len);
			bus_dmamap_sync(tq->dma_tag, tq->dma_map,
			    BUS_DMASYNC_PREWRITE);

			memset(&treq, 0, sizeof(treq));
			treq.cmd = BCE_VHCI_CMD_TRANSFER_REQUEST;
			treq.param1 = (ep_addr << 8) | tq->dev_addr;
			treq.param2 = len;

			/* Reserve msg slot first */
			mtx_lock_spin(&vhci->sc_async_lock);
			if (bce_reserve_submission(
			    vhci->msg_asynchronous.sq) != 0) {
				mtx_unlock_spin(&vhci->sc_async_lock);
				tq->active_xfer = NULL;
				tq->dma_inflight = 0;
				usbd_transfer_done(xfer, USB_ERR_IOERROR);
				return;
			}
			mtx_unlock_spin(&vhci->sc_async_lock);

			/* Then reserve SQ slot */
			mtx_lock_spin(&tq->lock);
			if (bce_reserve_submission(tq->sq_out) == 0) {
				so = bce_next_submission(tq->sq_out);
				so->addr = tq->dma_addr;
				so->length = len;
				so->segl_addr = 0;
				so->segl_length = 0;
				bce_submit_to_device(vhci->sc_bce,
				    tq->sq_out);
			} else {
				mtx_unlock_spin(&tq->lock);
				/* Restore msg slot reserved but won't use */
				mtx_lock_spin(&vhci->sc_async_lock);
				atomic_add_int(&vhci->
				    msg_asynchronous.sq->
				    available_commands, 1);
				mtx_unlock_spin(&vhci->sc_async_lock);
				tq->active_xfer = NULL;
				tq->dma_inflight = 0;
				usbd_transfer_done(xfer, USB_ERR_IOERROR);
				return;
			}
			mtx_unlock_spin(&tq->lock);

			mtx_lock_spin(&vhci->sc_async_lock);
			bce_vhci_msg_queue_write(vhci,
			    &vhci->msg_asynchronous, &treq);
			mtx_unlock_spin(&vhci->sc_async_lock);
		}
		return;
	}

	if (xfer_type != UE_CONTROL) {
		device_printf(vhci->sc_dev,
		    "xfer start ep=0x%02x type=%d (not supported)\n",
		    ep_addr, xfer_type);
		usbd_transfer_done(xfer, USB_ERR_STALLED);
		return;
	}

	/*
	 * Control transfer on ep0.  Map the USB device's port number
	 * to the firmware device ID.  The USB stack's port_no comes
	 * from our root hub, so it maps directly to our port index.
	 *
	 * For the root hub itself, the USB stack handles it via
	 * roothub_exec, so we should never see it here.
	 */
	{
		struct usb_device *udev = xfer->xroot->udev;
		uint8_t port, fw_dev_id;

		port = udev->port_no;
		if (port < 1 || port > vhci->sc_port_count) {
			device_printf(vhci->sc_dev,
			    "control xfer: invalid port %d\n", port);
			usbd_transfer_done(xfer, USB_ERR_STALLED);
			return;
		}

		fw_dev_id = vhci->sc_port_to_dev[port - 1];
		if (fw_dev_id >= BCE_VHCI_MAX_DEVICES) {
			device_printf(vhci->sc_dev,
			    "control xfer: no firmware device for "
			    "port %d\n", port);
			usbd_transfer_done(xfer, USB_ERR_STALLED);
			return;
		}

		dev = &vhci->sc_devs[fw_dev_id];
		if (dev->allocated == 0 || dev->tq[0].active == 0) {
			device_printf(vhci->sc_dev,
			    "control xfer: device %d ep0 not ready\n",
			    fw_dev_id);
			usbd_transfer_done(xfer, USB_ERR_STALLED);
			return;
		}

		tq = &dev->tq[0];
	}

	/*
	 * If the endpoint is stalled from a previous transfer, we need
	 * ENDPOINT_RESET (0x0044) before the next transfer can succeed.
	 * We cannot sleep in pipe_start (USB device mutex held), so
	 * the reset runs asynchronously on taskqueue_thread.
	 *
	 * Return USB_ERR_STALLED to the USB stack so it retries; the
	 * retry will succeed once sc_reset_task clears tq->stalled.
	 */
	if (tq->stalled) {
		device_printf(vhci->sc_dev,
		    "control xfer: ep0 stalled, scheduling ENDPOINT_RESET "
		    "(dev=%d)\n", tq->dev_addr);
		if (vhci->sc_detaching == 0)
			taskqueue_enqueue(taskqueue_thread,
			    &vhci->sc_reset_task);
		usbd_transfer_done(xfer, USB_ERR_STALLED);
		return;
	}

	if (tq->active_xfer != NULL || tq->dma_inflight != 0) {
		device_printf(vhci->sc_dev,
		    "control xfer: ep0 busy (dev=%d)\n", tq->dev_addr);
		usbd_transfer_done(xfer, USB_ERR_STALLED);
		return;
	}

	/* Read the 8-byte setup packet from frbuffers[0] */
	usbd_copy_out(&xfer->frbuffers[0], 0, &setup, sizeof(setup));

	/* Determine data direction and length from setup packet */
	tq->ctrl_dir = (setup.bmRequestType & UT_READ) ?
	    UE_DIR_IN : UE_DIR_OUT;
	tq->ctrl_data_len = UGETW(setup.wLength);
	tq->ctrl_actual = 0;
	tq->ctrl_data_done = 0;
	tq->ctrl_status_pending = 0;
	tq->active_xfer = xfer;
	tq->ctrl_state = BCE_VHCI_CTRL_SETUP;


	/*
	 * SET_ADDRESS is handled by firmware via DEVICE_CREATE  --
	 * complete immediately without forwarding to firmware.
	 */
	if (setup.bRequest == UR_SET_ADDRESS) {
		tq->ctrl_state = BCE_VHCI_CTRL_IDLE;
		tq->active_xfer = NULL;
		xfer->aframes = xfer->nframes;
		usbd_transfer_done(xfer, USB_ERR_NORMAL_COMPLETION);
		return;
	}

	/*
	 * Check for a deferred TRANSFER_REQUEST that arrived before
	 * pipe_start.  If one is pending, replay it now.
	 */
	if (tq->evt_pending) {
		tq->evt_pending = 0;
		bce_vhci_handle_transfer_request(vhci, &tq->evt_saved);
	}
}

/*
 * DMA tag callback (required by usb_bus_mem_alloc_all)
 */

static void
bce_vhci_iterate_hw_softc(struct usb_bus *bus, usb_bus_mem_sub_cb_t *cb)
{
	/* No hardware-specific DMA pages needed */
}

static int
bce_vhci_probe(device_t dev)
{

	device_set_desc(dev, "Apple T2 BCE Virtual USB Host Controller");
	return (BUS_PROBE_DEFAULT);
}

static int
bce_vhci_attach_dev(device_t dev)
{
	struct bce_vhci_softc *vhci;
	struct apple_bce_softc *bce;
	int err;

	vhci = device_get_softc(dev);
	bce = device_get_softc(device_get_parent(dev));
	if (bce == NULL) {
		device_printf(dev, "no BCE parent\n");
		return (ENXIO);
	}

	vhci->sc_dev = dev;
	vhci->sc_bce = bce;

	/* Sanity check parent state */
	if (bce->sc_cmd_cmdq == NULL || bce->sc_dma_tag == NULL) {
		device_printf(dev,
		    "BCE parent not ready (cmdq=%p dma_tag=%p)\n",
		    bce->sc_cmd_cmdq, bce->sc_dma_tag);
		return (ENXIO);
	}

	mtx_init(&vhci->sc_async_lock, "bce_vhci_async", NULL, MTX_SPIN);
	mtx_init(&vhci->sc_fwevt_lock, "bce_vhci_fwevt", NULL, MTX_SPIN);

	/* Initialize device state */
	memset(vhci->sc_devs, 0, sizeof(vhci->sc_devs));
	memset(vhci->sc_port_to_dev, 0xFF, sizeof(vhci->sc_port_to_dev));

	TASK_INIT(&vhci->sc_fwevt_task, 0, bce_vhci_fwevt_task, vhci);
	TASK_INIT(&vhci->sc_reset_task, 0, bce_vhci_reset_task, vhci);
	TASK_INIT(&vhci->sc_create_task, 0, bce_vhci_create_task, vhci);
	TASK_INIT(&vhci->sc_port_chg_task, 0, bce_vhci_port_chg_task, vhci);

	/*
	 * Initialize USB bus early so bus_mtx is valid before
	 * firmware events can call USB_BUS_LOCK.
	 */
	vhci->sc_bus.parent = dev;
	vhci->sc_bus.devices = vhci->sc_devices;
	vhci->sc_bus.devices_max = BCE_VHCI_MAX_DEVICES;
	vhci->sc_bus.dma_bits = 32;
	vhci->sc_bus.usbrev = USB_REV_2_0;
	vhci->sc_bus.methods = &bce_vhci_bus_methods;

	err = usb_bus_mem_alloc_all(&vhci->sc_bus,
	    USB_GET_DMA_TAG(dev), &bce_vhci_iterate_hw_softc);
	if (err != 0) {
		device_printf(dev, "usb_bus_mem_alloc_all failed: %d\n", err);
		goto fail;
	}

	/*
	 * Create BCE message/event queues for VHCI communication.
	 */
	err = bce_vhci_create_queues(vhci);
	if (err != 0) {
		device_printf(dev, "failed to create VHCI queues: %d\n", err);
		goto fail_mem;
	}

	/*
	 * Start controller: ENABLE -> discover ports -> START -> power on.
	 */
	err = bce_vhci_start_controller(vhci);
	if (err != 0) {
		device_printf(dev,
		    "failed to start VHCI controller: %d\n", err);
		goto fail_queues;
	}

	/* Create usbus child */
	vhci->sc_bus.bdev = device_add_child(dev, "usbus", DEVICE_UNIT_ANY);
	if (vhci->sc_bus.bdev == NULL) {
		device_printf(dev, "failed to add usbus child\n");
		err = ENOMEM;
		goto fail_ctrl;
	}
	device_set_ivars(vhci->sc_bus.bdev, &vhci->sc_bus);

	err = device_probe_and_attach(vhci->sc_bus.bdev);
	if (err != 0) {
		device_printf(dev, "usbus attach failed: %d\n", err);
		goto fail_child;
	}

	device_printf(dev, "BCE VHCI attached, %d ports\n",
	    vhci->sc_port_count);
	return (0);

fail_child:
	device_delete_child(dev, vhci->sc_bus.bdev);
fail_ctrl:
	/* Disable controller before tearing down queues */
	if (vhci->sc_started && vhci->msg_commands.sq != NULL) {
		struct bce_vhci_message cmd_dis, reply_dis;

		memset(&cmd_dis, 0, sizeof(cmd_dis));
		cmd_dis.cmd = BCE_VHCI_CMD_CONTROLLER_DISABLE;
		bce_vhci_cmd_execute(vhci, &cmd_dis, &reply_dis,
		    BCE_VHCI_CMD_TIMEOUT_LONG);
		vhci->sc_started = 0;
	}
fail_queues:
	vhci->sc_detaching = 1;
	taskqueue_drain(taskqueue_thread, &vhci->sc_reset_task);
	taskqueue_drain(taskqueue_thread, &vhci->sc_create_task);
	taskqueue_drain(taskqueue_thread, &vhci->sc_port_chg_task);
	taskqueue_drain(taskqueue_thread, &vhci->sc_fwevt_task);
	bce_vhci_destroy_queues(vhci);
fail_mem:
	usb_bus_mem_free_all(&vhci->sc_bus, &bce_vhci_iterate_hw_softc);
fail:
	mtx_destroy(&vhci->sc_fwevt_lock);
	mtx_destroy(&vhci->sc_async_lock);
	return (err);
}

static int
bce_vhci_detach_dev(device_t dev)
{
	struct bce_vhci_softc *vhci;

	vhci = device_get_softc(dev);

	/* Stop deferred work before tearing down USB child */
	vhci->sc_detaching = 1;

	/* Detach usbus child */
	if (vhci->sc_bus.bdev != NULL) {
		int err;

		err = device_delete_children(dev);
		if (err != 0) {
			vhci->sc_detaching = 0;
			return (err);
		}
		vhci->sc_bus.bdev = NULL;
	}

	/*
	 * Drain tasks that may reference tq state before
	 * destroying endpoints.
	 */
	taskqueue_drain(taskqueue_thread, &vhci->sc_reset_task);
	taskqueue_drain(taskqueue_thread, &vhci->sc_create_task);
	taskqueue_drain(taskqueue_thread, &vhci->sc_port_chg_task);

	/* Destroy all firmware devices and their endpoints */
	{
		int i;

		for (i = 0; i < BCE_VHCI_MAX_PORTS; i++)
			bce_vhci_device_destroy(vhci, i);
	}

	/* Send CONTROLLER_DISABLE if we started */
	if (vhci->sc_started && vhci->msg_commands.sq != NULL) {
		struct bce_vhci_message cmd, reply;

		memset(&cmd, 0, sizeof(cmd));
		cmd.cmd = BCE_VHCI_CMD_CONTROLLER_DISABLE;
		bce_vhci_cmd_execute(vhci, &cmd, &reply,
		    BCE_VHCI_CMD_TIMEOUT_LONG);
		vhci->sc_started = 0;
	}

	/* Drain firmware event task after command completion */
	taskqueue_drain(taskqueue_thread, &vhci->sc_fwevt_task);

	/* Tear down VHCI queues (unregisters from IRQ dispatch first) */
	bce_vhci_destroy_queues(vhci);

	usb_bus_mem_free_all(&vhci->sc_bus, &bce_vhci_iterate_hw_softc);
	mtx_destroy(&vhci->sc_fwevt_lock);
	mtx_destroy(&vhci->sc_async_lock);

	return (0);
}

int
bce_vhci_attach(struct apple_bce_softc *sc)
{
	device_t vhci_dev;

	vhci_dev = device_add_child(sc->sc_dev, "bce_vhci", DEVICE_UNIT_ANY);
	if (vhci_dev == NULL) {
		device_printf(sc->sc_dev,
		    "failed to add bce_vhci child\n");
		return (ENOMEM);
	}
	sc->sc_vhci_dev = vhci_dev;
	device_set_ivars(vhci_dev, sc);

	if (device_probe_and_attach(vhci_dev) != 0) {
		device_delete_child(sc->sc_dev, vhci_dev);
		sc->sc_vhci_dev = NULL;
		return (ENXIO);
	}
	return (0);
}

int
bce_vhci_detach(struct apple_bce_softc *sc)
{
	device_t vhci_dev;
	int error;

	vhci_dev = sc->sc_vhci_dev;
	if (vhci_dev == NULL)
		return (0);

	error = device_delete_child(sc->sc_dev, vhci_dev);
	if (error == 0)
		sc->sc_vhci_dev = NULL;

	return (error);
}
