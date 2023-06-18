/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright © 2021-2023 Dmitry Salychev
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

#ifndef	_DPAA2_TYPES_H
#define	_DPAA2_TYPES_H

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <machine/atomic.h>
#include <machine/bus.h>

#define DPAA2_MAGIC	((uint32_t) 0xD4AA2C0Du)

#define DPAA2_MAX_CHANNELS	 16 /* CPU cores */
#define DPAA2_MAX_TCS		 8  /* Traffic classes */

/**
 * @brief Types of the DPAA2 devices.
 */
enum dpaa2_dev_type {
	DPAA2_DEV_MC = 7500,	/* Management Complex (firmware bus) */
	DPAA2_DEV_RC,		/* Resource Container (firmware bus) */
	DPAA2_DEV_IO,		/* I/O object (to work with QBMan portal) */
	DPAA2_DEV_NI,		/* Network Interface */
	DPAA2_DEV_MCP,		/* MC portal */
	DPAA2_DEV_BP,		/* Buffer Pool */
	DPAA2_DEV_CON,		/* Concentrator */
	DPAA2_DEV_MAC,		/* MAC object */
	DPAA2_DEV_MUX,		/* MUX (Datacenter bridge) object */
	DPAA2_DEV_SW,		/* Ethernet Switch */

	DPAA2_DEV_NOTYPE	/* Shouldn't be assigned to any DPAA2 device. */
};

/**
 * @brief Types of the DPNI queues.
 */
enum dpaa2_ni_queue_type {
	DPAA2_NI_QUEUE_RX = 0,
	DPAA2_NI_QUEUE_TX,
	DPAA2_NI_QUEUE_TX_CONF,
	DPAA2_NI_QUEUE_RX_ERR
};

struct dpaa2_atomic {
	volatile int counter;
};

/**
 * @brief Tx ring.
 *
 * fq:		Parent (TxConf) frame queue.
 * fqid:	ID of the logical Tx queue.
 * br:		Ring buffer for mbufs to transmit.
 * lock:	Lock for the ring buffer.
 */
struct dpaa2_ni_tx_ring {
	struct dpaa2_ni_fq	*fq;
	uint32_t		 fqid;
	uint32_t		 txid; /* Tx ring index */

	struct buf_ring		*br;
	struct mtx		 lock;
} __aligned(CACHE_LINE_SIZE);

/**
 * @brief Frame Queue is the basic queuing structure used by the QMan.
 *
 * It comprises a list of frame descriptors (FDs), so it can be thought of
 * as a queue of frames.
 *
 * NOTE: When frames on a FQ are ready to be processed, the FQ is enqueued
 *	 onto a work queue (WQ).
 *
 * fqid:	Frame queue ID, can be used to enqueue/dequeue or execute other
 *		commands on the queue through DPIO.
 * txq_n:	Number of configured Tx queues.
 * tx_fqid:	Frame queue IDs of the Tx queues which belong to the same flowid.
 *		Note that Tx queues are logical queues and not all management
 *		commands are available on these queue types.
 * qdbin:	Queue destination bin. Can be used with the DPIO enqueue
 *		operation based on QDID, QDBIN and QPRI. Note that all Tx queues
 *		with the same flowid have the same destination bin.
 */
struct dpaa2_ni_fq {
	struct dpaa2_channel	*chan;
	uint32_t		 fqid;
	uint16_t		 flowid;
	uint8_t			 tc;
	enum dpaa2_ni_queue_type type;

	/* Optional fields (for TxConf queue). */
	struct dpaa2_ni_tx_ring	 tx_rings[DPAA2_MAX_TCS];
	uint32_t		 tx_qdbin;
} __aligned(CACHE_LINE_SIZE);

/* Handy wrappers over atomic operations. */
#define DPAA2_ATOMIC_XCHG(a, val) \
	(atomic_swap_int(&(a)->counter, (val)))
#define DPAA2_ATOMIC_READ(a) \
	(atomic_load_acq_int(&(a)->counter))
#define DPAA2_ATOMIC_ADD(a, val) \
	(atomic_add_acq_int(&(a)->counter, (val)))

const char *dpaa2_ttos(enum dpaa2_dev_type);
enum dpaa2_dev_type dpaa2_stot(const char *);
void dpaa2_dmamap_oneseg_cb(void *, bus_dma_segment_t *, int, int);

#endif /* _DPAA2_TYPES_H */
