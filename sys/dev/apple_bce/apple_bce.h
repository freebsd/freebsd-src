/*
 * Copyright (c) 2026 Abdelkader Boudih <freebsd@seuros.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Apple T2 Buffer Copy Engine (BCE) driver.
 * PCI transport layer for T2 coprocessor communication.
 */

#ifndef _APPLE_BCE_H_
#define _APPLE_BCE_H_

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sema.h>
#include <sys/callout.h>
#include <sys/rman.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#define BCE_PCI_VENDOR_APPLE	0x106b
#define BCE_PCI_DEVICE_T2	0x1801

#define BCE_MAX_QUEUE_COUNT	0x100
#define BCE_MAX_CQ_COUNT	64	/* Max completion queues tracked */
#define BCE_QUEUE_USER_MIN	2
#define BCE_QUEUE_USER_MAX	(BCE_MAX_QUEUE_COUNT - 1)
#define BCE_CMD_SIZE		0x40

#define BCE_FW_PROTOCOL_VER	0x20001
#define BCE_MBOX_TIMEOUT_MS	1000
#define BCE_TIMESTAMP_MS	150	/* Firmware keepalive interval */

/*
 * Mailbox register offsets (BAR4).
 */
#define BCE_REG_MBOX_OUT	0x820	/* 4x u32 to-device */
#define BCE_REG_MBOX_REPLY	0x810	/* 4x u32 from-device */
#define BCE_REG_MBOX_REPLY_CTR	0x108	/* Reply counter (bits 23:20 = count) */
#define BCE_REG_TIMESTAMP	0xC000	/* Timestamp sync */

/*
 * DMA register offsets (BAR2).
 */
#define BCE_REG_DOORBELL_BASE	0x44000

/*
 * Mailbox message format: 6-bit type (63:58) + 58-bit value.
 */
#define BCE_MB_MSG(type, val)	(((uint64_t)(type) << 58) | \
				 ((val) & 0x3FFFFFFFFFFFFFFULL))
#define BCE_MB_TYPE(v)		((uint32_t)((v) >> 58))
#define BCE_MB_VALUE(v)		((v) & 0x3FFFFFFFFFFFFFFULL)

/*
 * Mailbox message types.
 */
enum bce_mb_type {
	BCE_MB_REGISTER_CMD_SQ		= 0x07,
	BCE_MB_REGISTER_CMD_CQ		= 0x08,
	BCE_MB_REGISTER_QUEUE_REPLY	= 0x0B,
	BCE_MB_SET_FW_PROTOCOL_VER	= 0x0C,
	BCE_MB_SLEEP_NO_STATE		= 0x14,
	BCE_MB_RESTORE_NO_STATE		= 0x15,
	BCE_MB_SAVE_AND_SLEEP		= 0x17,
	BCE_MB_RESTORE_AND_WAKE		= 0x18,
	BCE_MB_SAVE_FAILURE		= 0x19,
	BCE_MB_SAVE_RESTORE_COMPLETE	= 0x1A,
};

/*
 * Queue types.
 */
enum bce_queue_type {
	BCE_QUEUE_CQ = 0,
	BCE_QUEUE_SQ = 1,
};

/*
 * Completion queue entry.
 */
struct bce_qe_completion {
	uint64_t	result;
	uint64_t	data_size;
	uint16_t	qid;		/* Source SQ */
	uint16_t	completion_index;
	uint16_t	status;
	uint16_t	flags;
#define	BCE_CQ_FLAG_PENDING	0x8000
} __packed;

/*
 * Completion status codes.
 */
enum bce_completion_status {
	BCE_COMP_SUCCESS	= 0,
	BCE_COMP_ERROR		= 1,
	BCE_COMP_ABORTED	= 2,
	BCE_COMP_NO_SPACE	= 3,
	BCE_COMP_OVERRUN	= 4,
};

/*
 * Submission queue entry.
 */
struct bce_qe_submission {
	uint64_t	length;
	uint64_t	addr;
	uint64_t	segl_addr;	/* Scatter-gather list */
	uint64_t	segl_length;
} __packed;

/*
 * Queue memory configuration (for mailbox registration).
 */
struct bce_queue_memcfg {
	uint16_t	qid;
	uint16_t	el_count;
	uint16_t	vector_or_cq;
	uint16_t	_pad;
	uint64_t	addr;
	uint64_t	length;
} __packed;

/*
 * Completion data cached per SQ slot.
 */
struct bce_sq_completion_data {
	uint32_t	status;
	uint64_t	data_size;
	uint64_t	result;
};

/*
 * Completion callback.
 */
struct bce_queue_sq;
typedef void (*bce_sq_completion_fn)(struct bce_queue_sq *sq);

/*
 * Completion queue.
 */
struct bce_queue_cq {
	int		qid;
	uint32_t	el_count;
	bus_dma_tag_t	dma_tag;
	bus_dmamap_t	dma_map;
	bus_addr_t	dma_addr;
	struct bce_qe_completion *data;
	uint32_t	index;		/* Consumer index */
};

/*
 * Submission queue.
 */
struct bce_queue_sq {
	int		qid;
	uint32_t	el_size;
	uint32_t	el_count;
	bus_dma_tag_t	dma_tag;
	bus_dmamap_t	dma_map;
	bus_addr_t	dma_addr;
	void		*data;
	void		*userdata;

	/* Submission tracking */
	volatile int	available_commands;
	uint32_t	head;
	uint32_t	tail;

	/* Completion tracking */
	uint32_t	completion_cidx;
	uint32_t	completion_tail;
	struct bce_sq_completion_data *completion_data;
	int		has_pending;
	bce_sq_completion_fn completion;
};

/*
 * Command queue (wraps SQ for synchronous ops).
 */
struct bce_queue_cmdq_result {
	struct sema	cmpl;
	uint32_t	status;
	uint64_t	result;
};

struct bce_queue_cmdq {
	struct bce_queue_sq	*sq;
	struct mtx		lck;
	struct bce_queue_cmdq_result **tres;	/* Pending results per slot */
};

/*
 * Command queue command IDs.
 */
enum bce_cmdq_cmd {
	BCE_CMD_REGISTER_QUEUE		= 0x20,
	BCE_CMD_UNREGISTER_QUEUE	= 0x30,
	BCE_CMD_FLUSH_QUEUE		= 0x40,
	BCE_CMD_SET_QUEUE_PROPERTY	= 0x50,
};

/*
 * Register queue command payload (fits in BCE_CMD_SIZE).
 */
struct bce_cmdq_reg_cmd {
	uint16_t	cmd;
	uint16_t	flags;
#define	BCE_CMDQ_FLAG_NAMED	0x02
#define	BCE_CMDQ_FLAG_OUT	0x01
	uint16_t	qid;
	uint16_t	_pad;
	uint16_t	el_count;
	uint16_t	vector_or_cq;
	uint16_t	_pad2;
	uint16_t	name_len;
	char		name[0x20];
	uint64_t	addr;
	uint64_t	length;
} __packed;

/*
 * Simple queue command (unregister/flush).
 */
struct bce_cmdq_simple_cmd {
	uint16_t	cmd;
	uint16_t	flags;
	uint16_t	qid;
} __packed;

/*
 * Mailbox state.
 */
struct bce_mailbox {
	struct resource	*reg;		/* BAR4 resource */
	volatile int	status;		/* 0=idle, 1=pending, 2=ready */
	int		initialized;
	struct sema	mb_cmpl;
	uint64_t	mb_result;
};

/*
 * Main device softc.
 */
struct apple_bce_softc {
	device_t		sc_dev;

	/* PCI resources */
	int			sc_bar2_rid;
	struct resource		*sc_bar2;	/* DMA registers */
	int			sc_bar4_rid;
	struct resource		*sc_bar4;	/* Mailbox registers */

	/* MSI */
	int			sc_msi_count;
	int			sc_irq_rid_mbox;
	struct resource		*sc_irq_mbox;
	void			*sc_irq_mbox_cookie;
	int			sc_irq_rid_dma;
	struct resource		*sc_irq_dma;
	void			*sc_irq_dma_cookie;

	/* DMA */
	bus_dma_tag_t		sc_dma_tag;

	/* Mailbox */
	struct bce_mailbox	sc_mbox;

	/* Timestamp keepalive */
	struct callout		sc_timestamp_co;
	struct mtx		sc_timestamp_lock;
	int			sc_timestamp_stopped;

	/* Command path */
	struct bce_queue_cq	*sc_cmd_cq;
	struct bce_queue_cmdq	*sc_cmd_cmdq;

	/* Queue registry */
	void			*sc_queues[BCE_MAX_QUEUE_COUNT];
	struct mtx		sc_queues_lock;
	struct bce_queue_cq	*sc_cq_list[BCE_MAX_CQ_COUNT];
	struct bce_queue_sq	*sc_int_sq_list[BCE_MAX_QUEUE_COUNT];
	device_t		sc_vhci_dev;
};

/* Inline helpers */
static __inline void *
bce_sq_element(struct bce_queue_sq *sq, uint32_t idx)
{
	return ((char *)sq->data + (idx * sq->el_size));
}

static __inline struct bce_qe_completion *
bce_cq_element(struct bce_queue_cq *cq, uint32_t idx)
{
	return (&cq->data[idx]);
}

#endif /* _APPLE_BCE_H_ */
