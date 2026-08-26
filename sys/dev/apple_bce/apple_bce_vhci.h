/*-
 * Copyright (c) 2026 Abdelkader Boudih <freebsd@seuros.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Apple T2 BCE Virtual USB Host Controller Interface (VHCI).
 * Translates USB operations into BCE firmware messages over DMA queues.
 */

#ifndef _APPLE_BCE_VHCI_H_
#define _APPLE_BCE_VHCI_H_

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sx.h>
#include <sys/sema.h>
#include <sys/taskqueue.h>

#include "apple_bce.h"

/* Forward declaration  -- full USB headers included only in .c file */
struct usb_bus;
struct usb_device;
struct usb_xfer;

/*
 * VHCI limits.
 */
#define BCE_VHCI_MAX_PORTS	16
#define BCE_VHCI_MAX_DEVICES	32
#define BCE_VHCI_MAX_ENDPOINTS	32

/* Queue element counts */
#define BCE_VHCI_MSG_QUEUE_EL	32
#define BCE_VHCI_EVT_QUEUE_EL	256
#define BCE_VHCI_EVT_PENDING	32
#define BCE_VHCI_TQ_EL		32	/* Transfer queue elements */
#define BCE_VHCI_XFER_BUFSZ	4096	/* DMA buffer per transfer queue */

/*
 * VHCI message format (16 bytes, matches firmware protocol).
 */
struct bce_vhci_message {
	uint16_t	cmd;
	uint16_t	status;
	uint32_t	param1;
	uint64_t	param2;
} __packed;

/*
 * VHCI command IDs.
 */
enum bce_vhci_cmd_id {
	/* Controller commands */
	BCE_VHCI_CMD_CONTROLLER_ENABLE	= 0x0001,
	BCE_VHCI_CMD_CONTROLLER_DISABLE	= 0x0002,
	BCE_VHCI_CMD_CONTROLLER_START	= 0x0003,
	BCE_VHCI_CMD_CONTROLLER_PAUSE	= 0x0004,

	/* Port commands */
	BCE_VHCI_CMD_PORT_POWER_ON	= 0x0010,
	BCE_VHCI_CMD_PORT_POWER_OFF	= 0x0011,
	BCE_VHCI_CMD_PORT_RESUME	= 0x0012,
	BCE_VHCI_CMD_PORT_SUSPEND	= 0x0013,
	BCE_VHCI_CMD_PORT_RESET	= 0x0014,
	BCE_VHCI_CMD_PORT_DISABLE	= 0x0015,
	BCE_VHCI_CMD_PORT_STATUS	= 0x0016,
	BCE_VHCI_CMD_PORT_STATUS_CHANGE	= 0x0018,

	/* Device commands */
	BCE_VHCI_CMD_DEVICE_CREATE	= 0x0030,
	BCE_VHCI_CMD_DEVICE_DESTROY	= 0x0031,

	/* Endpoint commands */
	BCE_VHCI_CMD_ENDPOINT_CREATE	= 0x0040,
	BCE_VHCI_CMD_ENDPOINT_DESTROY	= 0x0041,
	BCE_VHCI_CMD_ENDPOINT_SET_STATE	= 0x0042,
	BCE_VHCI_CMD_ENDPOINT_REQ_STATE	= 0x0043,
	BCE_VHCI_CMD_ENDPOINT_RESET	= 0x0044,

	/* Transfer commands */
	BCE_VHCI_CMD_TRANSFER_REQUEST	= 0x1000,
	BCE_VHCI_CMD_CTRL_TRANSFER_STATUS = 0x1005,

	/* Reply flag  -- firmware replies have cmd | 0x8000 */
	BCE_VHCI_CMD_REPLY_FLAG		= 0x8000,

	/* Cancel flag  -- timeout sends cmd | 0x4000 */
	BCE_VHCI_CMD_CANCEL_FLAG	= 0x4000,
};

/*
 * VHCI message status codes.
 */
enum bce_vhci_msg_status {
	BCE_VHCI_SUCCESS	= 1,
	BCE_VHCI_ERROR		= 2,
	BCE_VHCI_PIPE_STALL	= 3,
	BCE_VHCI_ABORT		= 4,
	BCE_VHCI_BAD_ARGUMENT	= 5,
	BCE_VHCI_OVERRUN	= 6,
	BCE_VHCI_INTERNAL_ERROR	= 7,
	BCE_VHCI_NO_POWER	= 8,
	BCE_VHCI_UNSUPPORTED	= 9,
};

/*
 * Endpoint states.
 */
enum bce_vhci_endpoint_state {
	BCE_VHCI_ENDP_ACTIVE	= 0,
	BCE_VHCI_ENDP_PAUSED	= 1,
	BCE_VHCI_ENDP_STALLED	= 2,
};

/*
 * Control transfer state machine.
 */
enum bce_vhci_ctrl_state {
	BCE_VHCI_CTRL_IDLE	= 0,
	BCE_VHCI_CTRL_SETUP	= 1,	/* Awaiting setup XFER_REQ */
	BCE_VHCI_CTRL_DATA	= 2,	/* Awaiting data XFER_REQ */
	BCE_VHCI_CTRL_STATUS	= 3,	/* Awaiting CTRL_XFER_STATUS */
};

/*
 * Pause sources (bitmask).
 */
#define BCE_VHCI_PAUSE_INTERNAL		0x01
#define BCE_VHCI_PAUSE_FIRMWARE		0x02
#define BCE_VHCI_PAUSE_SUSPEND		0x04
#define BCE_VHCI_PAUSE_SHUTDOWN		0x08

/*
 * Port status bit mapping (firmware -> USB).
 * Firmware uses its own bit encoding; we translate in roothub_exec.
 */
#define BCE_VHCI_PORT_CONNECTED		0x0004
#define BCE_VHCI_PORT_ENABLED		0x0010
#define BCE_VHCI_PORT_SUSPENDED		0x0060
#define BCE_VHCI_PORT_OVERCURRENT	0x0002
#define BCE_VHCI_PORT_RESET		0x0008
#define BCE_VHCI_PORT_C_CONNECTION	0x40000

/*
 * VHCI message queue (host -> device).
 */
struct bce_vhci_msg_queue {
	struct bce_queue_cq	*cq;
	struct bce_queue_sq	*sq;
	bus_dma_tag_t		dma_tag;
	bus_dmamap_t		dma_map;
	bus_addr_t		dma_addr;
	struct bce_vhci_message	*data;
	uint32_t		el_count;
};

/*
 * VHCI event queue (device -> host).
 * Single contiguous DMA buffer for all receive slots.
 */
struct bce_vhci_evt_queue {
	struct bce_queue_sq	*sq;
	bus_dma_tag_t		dma_tag;
	bus_dmamap_t		dma_map;
	bus_addr_t		dma_addr;
	struct bce_vhci_message	*data;
	uint32_t		el_count;
	void			*userdata;
};

/*
 * VHCI command queue (synchronous command execution).
 */
struct bce_vhci_cmd_queue {
	struct bce_vhci_msg_queue *msg;
	struct sx		exec_lock;	/* Serialize callers */
	struct mtx		lock;
	struct sema		completion;
	struct bce_vhci_message	response;
	volatile int		pending;
	uint16_t		expected_cmd;	/* Filter late replies */
};

/*
 * VHCI transfer queue (per endpoint).
 *
 * Each endpoint gets a CQ + IN SQ + OUT SQ triplet registered with
 * firmware as named DMA queues.  The DMA buffer is used to shuttle
 * USB payloads between the USB stack's frame buffers and firmware.
 */
struct bce_vhci_transfer_queue {
	struct bce_vhci_softc	*vhci;
	uint8_t			dev_addr;	/* firmware device id */
	uint8_t			endp_addr;	/* USB endpoint address */
	struct mtx		lock;		/* Protects SQ submission */
	struct bce_queue_cq	*cq;
	struct bce_queue_sq	*sq_in;		/* Device -> host */
	struct bce_queue_sq	*sq_out;	/* Host -> device */
	struct usb_xfer		*active_xfer;
	uint32_t		paused_by;
	int			active;		/* Queues created with FW */
	int			stalled;
	int			dma_inflight;	/* DMA pending */
	int			create_pending;	/* Deferred ep create */
	struct usb_endpoint_descriptor *create_edesc;
	struct usb_xfer		*create_xfer;	/* Deferred xfer */

	/* DMA buffer for data transfer */
	bus_dma_tag_t		dma_tag;
	bus_dmamap_t		dma_map;
	bus_addr_t		dma_addr;
	void			*dma_buf;

	/* Control transfer state machine */
	enum bce_vhci_ctrl_state ctrl_state;
	uint8_t			ctrl_dir;	/* UE_DIR_IN or UE_DIR_OUT */
	uint32_t		ctrl_data_len;	/* Expected data phase length */
	uint32_t		ctrl_actual;	/* Actual bytes transferred */
	int			ctrl_data_done;	/* IN DMA completion seen */
	int			ctrl_status_pending; /* Deferred STATUS msg */
	struct bce_vhci_message	ctrl_status_msg; /* Saved STATUS for defer */

	/* Queued transfer waiting for active_xfer to finish */
	struct usb_xfer		*pending_xfer;

	/* Deferred firmware event (TRANSFER_REQUEST arrives before xfer) */
	int			evt_pending;
	struct bce_vhci_message	evt_saved;
};

/*
 * VHCI per-device state.
 */
struct bce_vhci_device {
	int			allocated;	/* Device created with FW */
	uint8_t			fw_dev_id;	/* Firmware device ID */
	uint8_t			port;		/* Port number */
	struct bce_vhci_transfer_queue tq[BCE_VHCI_MAX_ENDPOINTS];
};

/* VHCI softc is defined in apple_bce_vhci.c (depends on USB headers) */
struct bce_vhci_softc;

/* VHCI driver interface */
int	bce_vhci_attach(struct apple_bce_softc *sc);
int	bce_vhci_detach(struct apple_bce_softc *sc);

#endif /* _APPLE_BCE_VHCI_H_ */
