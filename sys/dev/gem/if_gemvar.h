/*-
 * Copyright (C) 2001 Eduardo Horvath.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR  ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR  BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: NetBSD: gemvar.h,v 1.8 2002/05/15 02:36:12 matt Exp
 *
 * $FreeBSD$
 */

#ifndef	_IF_GEMVAR_H
#define	_IF_GEMVAR_H


#include <sys/queue.h>
#include <sys/callout.h>

/*
 * Misc. definitions for the Sun ``Gem'' Ethernet controller family driver.
 */

/*
 * Transmit descriptor list size.  This is arbitrary, but allocate
 * enough descriptors for 64 pending transmissions and 16 segments
 * per packet. This limit is not actually enforced (packets with more segments
 * can be sent, depending on the busdma backend); it is however used as an
 * estimate for the tx window size.
 */
#define	GEM_NTXSEGS		16

#define	GEM_TXQUEUELEN		64
#define	GEM_NTXDESC		(GEM_TXQUEUELEN * GEM_NTXSEGS)
#define	GEM_MAXTXFREE		(GEM_NTXDESC - 1)
#define	GEM_NTXDESC_MASK	(GEM_NTXDESC - 1)
#define	GEM_NEXTTX(x)		((x + 1) & GEM_NTXDESC_MASK)

/*
 * Receive descriptor list size.  We have one Rx buffer per incoming
 * packet, so this logic is a little simpler.
 */
#define	GEM_NRXDESC		128
#define	GEM_NRXDESC_MASK	(GEM_NRXDESC - 1)
#define	GEM_PREVRX(x)		((x - 1) & GEM_NRXDESC_MASK)
#define	GEM_NEXTRX(x)		((x + 1) & GEM_NRXDESC_MASK)

/*
 * How many ticks to wait until to retry on a RX descriptor that is still owned
 * by the hardware.
 */
#define	GEM_RXOWN_TICKS		(hz / 50)

/*
 * Control structures are DMA'd to the GEM chip.  We allocate them in
 * a single clump that maps to a single DMA segment to make several things
 * easier.
 */
struct gem_control_data {
	/*
	 * The transmit descriptors.
	 */
	struct gem_desc gcd_txdescs[GEM_NTXDESC];

	/*
	 * The receive descriptors.
	 */
	struct gem_desc gcd_rxdescs[GEM_NRXDESC];
};

#define	GEM_CDOFF(x)		offsetof(struct gem_control_data, x)
#define	GEM_CDTXOFF(x)		GEM_CDOFF(gcd_txdescs[(x)])
#define	GEM_CDRXOFF(x)		GEM_CDOFF(gcd_rxdescs[(x)])

/*
 * Software state for transmit job mbufs (may be elements of mbuf chains).
 */
struct gem_txsoft {
	struct mbuf *txs_mbuf;		/* head of our mbuf chain */
	bus_dmamap_t txs_dmamap;	/* our DMA map */
	int txs_firstdesc;		/* first descriptor in packet */
	int txs_lastdesc;		/* last descriptor in packet */
	int txs_ndescs;			/* number of descriptors */
	STAILQ_ENTRY(gem_txsoft) txs_q;
};

STAILQ_HEAD(gem_txsq, gem_txsoft);

/* Argument structure for busdma callback */
struct gem_txdma {
	struct gem_softc *txd_sc;
	struct gem_txsoft	*txd_txs;
};

/*
 * Software state for receive jobs.
 */
struct gem_rxsoft {
	struct mbuf *rxs_mbuf;		/* head of our mbuf chain */
	bus_dmamap_t rxs_dmamap;	/* our DMA map */
	bus_addr_t rxs_paddr;		/* physical address of the segment */
};

/*
 * Software state per device.
 */
struct gem_softc {
	struct ifnet	*sc_ifp;
	device_t	sc_miibus;
	struct mii_data	*sc_mii;	/* MII media control */
	device_t	sc_dev;		/* generic device information */
	u_char		sc_enaddr[6];
	struct callout	sc_tick_ch;	/* tick callout */
	struct callout	sc_rx_ch;	/* delayed rx callout */
	int		sc_wdog_timer;	/* watchdog timer */

	/* The following bus handles are to be provided by the bus front-end */
	bus_space_tag_t	sc_bustag;	/* bus tag */
	bus_dma_tag_t	sc_pdmatag;	/* parent bus dma tag */
	bus_dma_tag_t	sc_rdmatag;	/* RX bus dma tag */
	bus_dma_tag_t	sc_tdmatag;	/* TX bus dma tag */
	bus_dma_tag_t	sc_cdmatag;	/* control data bus dma tag */
	bus_dmamap_t	sc_dmamap;	/* bus dma handle */
	bus_space_handle_t sc_h;	/* bus space handle for all regs */

	int		sc_phys[2];	/* MII instance -> PHY map */

	int		sc_mif_config;	/* Selected MII reg setting */

	int		sc_pci;		/* XXXXX -- PCI buses are LE. */
	u_int		sc_variant;	/* which GEM are we dealing with? */
#define	GEM_UNKNOWN		0	/* don't know */
#define	GEM_SUN_GEM		1	/* Sun GEM variant */
#define	GEM_APPLE_GMAC		2	/* Apple GMAC variant */

	u_int		sc_flags;	/* */
#define	GEM_GIGABIT		0x0001	/* has a gigabit PHY */

	/*
	 * Ring buffer DMA stuff.
	 */
	bus_dma_segment_t sc_cdseg;	/* control data memory */
	int		sc_cdnseg;	/* number of segments */
	bus_dmamap_t	sc_cddmamap;	/* control data DMA map */
	bus_addr_t	sc_cddma;

	/*
	 * Software state for transmit and receive descriptors.
	 */
	struct gem_txsoft sc_txsoft[GEM_TXQUEUELEN];
	struct gem_rxsoft sc_rxsoft[GEM_NRXDESC];

	/*
	 * Control data structures.
	 */
	struct gem_control_data *sc_control_data;
#define	sc_txdescs	sc_control_data->gcd_txdescs
#define	sc_rxdescs	sc_control_data->gcd_rxdescs

	int		sc_txfree;		/* number of free Tx descriptors */
	int		sc_txnext;		/* next ready Tx descriptor */
	int		sc_txwin;		/* Tx descriptors since last Tx int */

	struct gem_txsq	sc_txfreeq;	/* free Tx descsofts */
	struct gem_txsq	sc_txdirtyq;	/* dirty Tx descsofts */

	int		sc_rxptr;		/* next ready RX descriptor/descsoft */
	int		sc_rxfifosize;		/* Rx FIFO size (bytes) */

	/* ========== */
	int		sc_inited;
	int		sc_debug;
	int		sc_ifflags;

	struct mtx	sc_mtx;
};

#define	GEM_DMA_READ(sc, v)	(((sc)->sc_pci) ? le64toh(v) : be64toh(v))
#define	GEM_DMA_WRITE(sc, v)	(((sc)->sc_pci) ? htole64(v) : htobe64(v))

#define	GEM_CDTXADDR(sc, x)	((sc)->sc_cddma + GEM_CDTXOFF((x)))
#define	GEM_CDRXADDR(sc, x)	((sc)->sc_cddma + GEM_CDRXOFF((x)))

#define	GEM_CDSYNC(sc, ops)						\
	bus_dmamap_sync((sc)->sc_cdmatag, (sc)->sc_cddmamap, (ops));	\

#define	GEM_INIT_RXDESC(sc, x)						\
do {									\
	struct gem_rxsoft *__rxs = &sc->sc_rxsoft[(x)];			\
	struct gem_desc *__rxd = &sc->sc_rxdescs[(x)];			\
	struct mbuf *__m = __rxs->rxs_mbuf;				\
									\
	__m->m_data = __m->m_ext.ext_buf;				\
	__rxd->gd_addr =						\
	    GEM_DMA_WRITE((sc), __rxs->rxs_paddr);			\
	__rxd->gd_flags =						\
	    GEM_DMA_WRITE((sc),						\
			(((__m->m_ext.ext_size)<<GEM_RD_BUFSHIFT)	\
				& GEM_RD_BUFSIZE) | GEM_RD_OWN);	\
} while (0)

#define	GEM_LOCK_INIT(_sc, _name)					\
	mtx_init(&(_sc)->sc_mtx, _name, MTX_NETWORK_LOCK, MTX_DEF)
#define	GEM_LOCK(_sc)			mtx_lock(&(_sc)->sc_mtx)
#define	GEM_UNLOCK(_sc)			mtx_unlock(&(_sc)->sc_mtx)
#define	GEM_LOCK_ASSERT(_sc, _what)	mtx_assert(&(_sc)->sc_mtx, (_what))
#define	GEM_LOCK_DESTROY(_sc)		mtx_destroy(&(_sc)->sc_mtx)

#ifdef _KERNEL
extern devclass_t gem_devclass;

int	gem_attach(struct gem_softc *);
void	gem_detach(struct gem_softc *);
void	gem_suspend(struct gem_softc *);
void	gem_resume(struct gem_softc *);
void	gem_intr(void *);

int	gem_mediachange(struct ifnet *);
void	gem_mediastatus(struct ifnet *, struct ifmediareq *);

void	gem_reset(struct gem_softc *);

/* MII methods & callbacks */
int	gem_mii_readreg(device_t, int, int);
int	gem_mii_writereg(device_t, int, int, int);
void	gem_mii_statchg(device_t);

#endif /* _KERNEL */


#endif
