/*-
 * Copyright (c) 2012 Damjan Marion <dmarion@Freebsd.org>
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

#ifndef	_IF_CPSWVAR_H
#define	_IF_CPSWVAR_H

#define CPSW_INTR_COUNT		4

/* MII BUS  */
#define CPSW_MIIBUS_RETRIES	5
#define CPSW_MIIBUS_DELAY	1000

#define CPSW_MAX_TX_BUFFERS	128
#define CPSW_MAX_RX_BUFFERS	128
#define CPSW_MAX_ALE_ENTRIES	1024

struct cpsw_softc {
	struct ifnet	*ifp;
	phandle_t	node;
	device_t	dev;
	uint8_t		mac_addr[ETHER_ADDR_LEN];
	device_t	miibus;
	struct mii_data	*mii;
	struct mtx	tx_lock;			/* transmitter lock */
	struct mtx	rx_lock;			/* receiver lock */
	struct resource	*res[1 + CPSW_INTR_COUNT];	/* resources */
	void		*ih_cookie[CPSW_INTR_COUNT];	/* interrupt handlers cookies */

	uint32_t	cpsw_if_flags;
	uint32_t	cpsw_media_status;

	struct callout	wd_callout;
	int		wd_timer;

	/* buffers */
	bus_dma_tag_t	mbuf_dtag;
	bus_dmamap_t	tx_dmamap[CPSW_MAX_TX_BUFFERS];
	bus_dmamap_t	rx_dmamap[CPSW_MAX_RX_BUFFERS];
	struct mbuf	*tx_mbuf[CPSW_MAX_TX_BUFFERS];
	struct mbuf	*rx_mbuf[CPSW_MAX_RX_BUFFERS];
	int		txbd_head;
	int		txbd_queue_size;
	int		rxbd_head;
	int		rxbd_tail;

	int		tmp;
	int		eoq;
	int		tc[CPSW_MAX_TX_BUFFERS];
	int		tc_unload[CPSW_MAX_TX_BUFFERS];
	
	struct cpsw_softc *phy_sc;
};

#define CPDMA_BD_SOP		(1<<15)
#define CPDMA_BD_EOP		(1<<14)
#define CPDMA_BD_OWNER		(1<<13)
#define CPDMA_BD_EOQ		(1<<12)
#define CPDMA_BD_PKT_ERR_MASK	(3<< 4)

struct cpsw_cpdma_bd {
	volatile uint32_t next;
	volatile uint32_t bufptr;
	volatile uint16_t buflen;
	volatile uint16_t bufoff;
	volatile uint16_t pktlen;
	volatile uint16_t flags;
};

/* Read/Write macros */
#define cpsw_read_4(reg)		bus_read_4(sc->res[0], reg)
#define cpsw_write_4(reg, val)		bus_write_4(sc->res[0], reg, val)

#define cpsw_cpdma_txbd_offset(i)	\
	(CPSW_CPPI_RAM_OFFSET + ((i)*16))
#define cpsw_cpdma_txbd_paddr(i)	(cpsw_cpdma_txbd_offset(i) + \
	vtophys(rman_get_start(sc->res[0])))
#define cpsw_cpdma_read_txbd(i, val)	\
	bus_read_region_4(sc->res[0], cpsw_cpdma_txbd_offset(i), (uint32_t *) val, 4)
#define cpsw_cpdma_write_txbd(i, val)	\
	bus_write_region_4(sc->res[0], cpsw_cpdma_txbd_offset(i), (uint32_t *) val, 4)
#define cpsw_cpdma_write_txbd_next(i, val) \
	bus_write_4(sc->res[0], cpsw_cpdma_txbd_offset(i), val)
#define cpsw_cpdma_read_txbd_flags(i) \
	bus_read_2(sc->res[0], cpsw_cpdma_txbd_offset(i)+14)

#define cpsw_cpdma_rxbd_offset(i)	\
	(CPSW_CPPI_RAM_OFFSET + ((CPSW_MAX_TX_BUFFERS + (i))*16))
#define cpsw_cpdma_rxbd_paddr(i)	(cpsw_cpdma_rxbd_offset(i) + \
	vtophys(rman_get_start(sc->res[0])))
#define cpsw_cpdma_read_rxbd(i, val)	\
	bus_read_region_4(sc->res[0], cpsw_cpdma_rxbd_offset(i), (uint32_t *) val, 4)
#define cpsw_cpdma_write_rxbd(i, val)	\
	bus_write_region_4(sc->res[0], cpsw_cpdma_rxbd_offset(i), (uint32_t *) val, 4)
#define cpsw_cpdma_write_rxbd_next(i, val) \
	bus_write_4(sc->res[0], cpsw_cpdma_rxbd_offset(i), val)
#define cpsw_cpdma_read_rxbd_flags(i) \
	bus_read_2(sc->res[0], cpsw_cpdma_rxbd_offset(i)+14)

#endif /*_IF_CPSWVAR_H */
