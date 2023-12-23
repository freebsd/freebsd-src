/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Alstom Group.
 * Copyright (c) 2021 Semihalf.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef _ENETC_H_
#define _ENETC_H_

#include <sys/param.h>

#include <dev/enetc/enetc_hw.h>

struct enetc_softc;
struct enetc_rx_queue {
	struct enetc_softc	*sc;
	uint16_t		qid;

	union enetc_rx_bd	*ring;
	uint64_t		ring_paddr;

	struct if_irq		irq;
	bool			enabled;
};

struct enetc_tx_queue {
	struct enetc_softc	*sc;

	union enetc_tx_bd	*ring;
	uint64_t		ring_paddr;

	qidx_t			cidx;

	struct if_irq		irq;
};

struct enetc_ctrl_queue {
	qidx_t			pidx;

	struct iflib_dma_info	dma;
	struct enetc_cbd	*ring;

	struct if_irq		irq;
};

struct enetc_softc {
	device_t	dev;

	struct mtx	mii_lock;

	if_ctx_t	ctx;
	if_softc_ctx_t	shared;
#define tx_num_queues	shared->isc_ntxqsets
#define rx_num_queues	shared->isc_nrxqsets
#define tx_queue_size	shared->isc_ntxd[0]
#define rx_queue_size	shared->isc_nrxd[0]

	struct resource	*regs;

	device_t	miibus;

	struct enetc_tx_queue	*tx_queues;
	struct enetc_rx_queue	*rx_queues;
	struct enetc_ctrl_queue	ctrl_queue;

	/* Default RX queue configuration. */
	uint32_t		rbmr;
	/*
	 * Hardware VLAN hash based filtering uses a 64bit bitmap.
	 * We need to know how many vids are in given position to
	 * know when to remove the bit from the bitmap.
	 */
#define	VLAN_BITMAP_SIZE	64
	uint8_t			vlan_bitmap[64];

	struct if_irq		admin_irq;
	int			phy_addr;

	struct ifmedia		fixed_ifmedia;
	bool			fixed_link;
};

#define ENETC_RD4(sc, reg)	\
	bus_read_4((sc)->regs, reg)
#define ENETC_WR4(sc, reg, value)	\
	bus_write_4((sc)->regs, reg, value)

#define ENETC_PORT_RD8(sc, reg) \
	bus_read_8((sc)->regs, ENETC_PORT_BASE + (reg))
#define ENETC_PORT_RD4(sc, reg) \
	bus_read_4((sc)->regs, ENETC_PORT_BASE + (reg))
#define ENETC_PORT_WR4(sc, reg, value) \
	bus_write_4((sc)->regs, ENETC_PORT_BASE + (reg), value)
#define ENETC_PORT_RD2(sc, reg) \
	bus_read_2((sc)->regs, ENETC_PORT_BASE + (reg))
#define ENETC_PORT_WR2(sc, reg, value) \
	bus_write_2((sc)->regs, ENETC_PORT_BASE + (reg), value)

#define ENETC_TXQ_RD4(sc, q, reg) \
	ENETC_RD4((sc), ENETC_BDR(TX, q, reg))
#define ENETC_TXQ_WR4(sc, q, reg, value) \
	ENETC_WR4((sc), ENETC_BDR(TX, q, reg), value)
#define ENETC_RXQ_RD4(sc, q, reg) \
	ENETC_RD4((sc), ENETC_BDR(RX, q, reg))
#define ENETC_RXQ_WR4(sc, q, reg, value) \
	ENETC_WR4((sc), ENETC_BDR(RX, q, reg), value)

/* Device constants */

#define ENETC_MAX_FRAME_LEN	9600

#define ENETC_MAX_QUEUES	4

/* Max supported nr of descriptors per frame. */
#define ENETC_MAX_SCATTER	15

/*
 * Up to 4096 transmit/receive descriptors are supported,
 * their number has to be a multiple of 64.
 */
#define ENETC_MIN_DESC		64
#define ENETC_MAX_DESC		4096
#define ENETC_DEFAULT_DESC	512
#define ENETC_DESC_ALIGN	64

/* Rings have to be 128B aligned. */
#define ENETC_RING_ALIGN	128

#define ENETC_MSIX_COUNT	32

#define ENETC_RX_INTR_PKT_THR	16

/* Rx threshold irq timeout, 100us */
#define ENETC_RX_INTR_TIME_THR	((100ULL * ENETC_CLK) / 1000000ULL)

#define ENETC_RX_IP_ALIGN	2

#endif
