/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019-2025 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
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

#ifndef	_DEV_XILINX_IF_XAEVAR_H_
#define	_DEV_XILINX_IF_XAEVAR_H_

struct xae_bufmap {
	struct mbuf	*mbuf;
	bus_dmamap_t	map;
};

/*
 * Driver data and defines.
 */
#define	RX_DESC_COUNT	64
#define	RX_DESC_SIZE	(sizeof(struct axidma_desc) * RX_DESC_COUNT)
#define	TX_DESC_COUNT	64
#define	TX_DESC_SIZE	(sizeof(struct axidma_desc) * TX_DESC_COUNT)

#define	AXIDMA_TX_CHAN		0
#define	AXIDMA_RX_CHAN		1

struct xae_softc {
	struct resource		*res[2];
	bus_space_tag_t		bst;
	bus_space_handle_t	bsh;
	device_t		dev;
	uint8_t			macaddr[ETHER_ADDR_LEN];
	device_t		miibus;
	struct mii_data *	mii_softc;
	if_t			ifp;
	int			if_flags;
	struct mtx		mtx;
	void *			intr_cookie;
	struct callout		xae_callout;
	boolean_t		link_is_up;
	boolean_t		is_attached;
	boolean_t		is_detaching;
	int			phy_addr;

	/* Counters */
	uint64_t		counters[XAE_MAX_COUNTERS];

	/* Axistream-connected. */
	device_t		dma_dev;
	struct resource		*dma_res;

	int			rxbuf_align;
	int			txbuf_align;

	bus_dma_tag_t		rxdesc_tag;
	bus_dmamap_t		rxdesc_map;
	struct axidma_desc	*rxdesc_ring;
	bus_addr_t		rxdesc_ring_paddr;
	bus_dma_tag_t		rxbuf_tag;
	struct xae_bufmap	rxbuf_map[RX_DESC_COUNT];
	uint32_t		rx_idx;

	bus_dma_tag_t		txdesc_tag;
	bus_dmamap_t		txdesc_map;
	struct axidma_desc	*txdesc_ring;
	bus_addr_t		txdesc_ring_paddr;
	bus_dma_tag_t		txbuf_tag;
	struct xae_bufmap	txbuf_map[TX_DESC_COUNT];
	uint32_t		tx_idx_head;
	uint32_t		tx_idx_tail;
	int			txcount;
};

#endif	/* _DEV_XILINX_IF_XAEVAR_H_ */
