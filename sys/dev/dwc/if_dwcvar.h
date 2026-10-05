/*-
 * Copyright (c) 2014 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

/*
 * Ethernet media access controller (EMAC)
 * Chapter 17, Altera Cyclone V Device Handbook (CV-5V2 2014.07.22)
 *
 * EMAC is an instance of the Synopsys DesignWare 3504-0
 * Universal 10/100/1000 Ethernet MAC (DWC_gmac).
 */

#ifndef	__IF_DWCVAR_H__
#define	__IF_DWCVAR_H__

/*
 * Driver data and defines.
 */
#define	RX_DESC_COUNT	1024
#define	RX_MAP_COUNT	RX_DESC_COUNT
#define	TX_DESC_COUNT	1024
#define	TX_MAP_COUNT	TX_DESC_COUNT
#define	TX_MAP_MAX_SEGS	32

#define	DMA_DEFAULT_PBL	8

/* Synopsys Core versions IDs */
#define	DWMAC_CORE_3_40		0x34
#define	DWMAC_CORE_3_50		0x35
#define	DWMAC_CORE_3_70		0x37
#define	DWMAC_CORE_4_00		0x40
#define	DWMAC_CORE_4_10		0x41
#define	DWMAC_CORE_5_00		0x50
#define	DWMAC_CORE_5_10		0x51
#define	DWMAC_CORE_5_20		0x52
#define	DWMAC_CORE_5_40		0x54

struct dwc_bufmap {
	bus_dmamap_t		map;
	struct mbuf		*mbuf;
	/* Only used for TX descirptors */
	int			last_desc_idx;
};

struct dwc_softc;

struct dwc_ops {
	int (*dma_init)(struct dwc_softc *sc);
	void (*dma_free)(struct dwc_softc *sc);
	void (*dma_start)(struct dwc_softc *sc);
	void (*dma_stop)(struct dwc_softc *sc);
	int (*dma_reset)(struct dwc_softc *sc);
	void (*dma_txfinish_locked)(struct dwc_softc *sc);
	void (*dma_txstart)(struct dwc_softc *sc);
	int (*dma_intr)(struct dwc_softc *sc);

	int (*core_miibus_read_reg)(device_t dev, int phy, int reg);
	int (*core_miibus_write_reg)(device_t dev, int phy, int reg, int val);
	void (*core_miibus_statchg)(device_t dev);
	void (*core_setup)(struct dwc_softc *sc);
	void (*core_enable_mac)(struct dwc_softc *sc, bool enable);
	void (*core_enable_csum_offload)(struct dwc_softc *sc);
	void (*core_setup_rxfilter)(struct dwc_softc *sc);
	void (*core_get_hwaddr)(struct dwc_softc *sc, uint8_t *hwaddr);
	void (*core_harvest_stats)(struct dwc_softc *sc);
	void (*core_intr)(struct dwc_softc *softc);
	void (*core_intr_disable)(struct dwc_softc *sc);
};

struct dwc_softc {
	struct resource		*res[2];
	device_t		dev;
	phandle_t		node;
	int			mii_clk;
	device_t		miibus;
	struct mii_data *	mii_softc;
	if_t			ifp;
	int			if_flags;
	struct mtx		mtx;
	void *			intr_cookie;
	struct callout		dwc_callout;
	bool			link_is_up;
	bool			is_attached;
	bool			is_detaching;
	int			tx_watchdog_count;
	int			stats_harvest_count;
	int			phy_mode;

	/* clocks and reset */
	clk_t			clk_stmmaceth;
	clk_t			clk_pclk;
	hwreset_t		rst_stmmaceth;
	hwreset_t		rst_ahb;

	/* DMA config */
	uint32_t		txpbl;	/* TX Burst lenght */
	uint32_t		rxpbl;	/* RX Burst lenght */
	bool			nopblx8;
	bool			fixed_burst;
	bool			mixed_burst;
	bool			aal;
	bool			dma_ext_desc;

	/* RX */
	bus_dma_tag_t		rxdesc_tag;
	bus_dmamap_t		rxdesc_map;
	struct dwc_hwdesc	*rxdesc_ring;
	bus_addr_t		rxdesc_ring_paddr;
	bus_dma_tag_t		rxbuf_tag;
	struct dwc_bufmap	rxbuf_map[RX_DESC_COUNT];
	uint32_t		rx_idx;

	/* TX */
	bus_dma_tag_t		txdesc_tag;
	bus_dmamap_t		txdesc_map;
	struct dwc_hwdesc	*txdesc_ring;
	bus_addr_t		txdesc_ring_paddr;
	bus_dma_tag_t		txbuf_tag;
	struct dwc_bufmap	txbuf_map[TX_DESC_COUNT];
	uint32_t		tx_desc_head;
	uint32_t		tx_desc_tail;
	uint32_t		tx_map_head;
	uint32_t		tx_map_tail;
	int			tx_desccount;
	int			tx_mapcount;
	struct dwc_ops		*ops;

	int			flags;
#define	DWC_HAS_GMAC5		(1 << 2)
#define	DWC_HAS_GMAC4		(1 << 1)
#define	DWC_HAS_GMAC3		(1 << 0)
};

#define	READ4(_sc, _reg) \
	bus_read_4((_sc)->res[0], _reg)
#define	WRITE4(_sc, _reg, _val) \
	bus_write_4((_sc)->res[0], _reg, _val)

#define	DWC_LOCK(sc)			mtx_lock(&(sc)->mtx)
#define	DWC_UNLOCK(sc)			mtx_unlock(&(sc)->mtx)
#define	DWC_ASSERT_LOCKED(sc)		mtx_assert(&(sc)->mtx, MA_OWNED)
#define	DWC_ASSERT_UNLOCKED(sc)		mtx_assert(&(sc)->mtx, MA_NOTOWNED)

#endif	/* __IF_DWCVAR_H__ */
