/*-
 * Copyright (c) 2010 Yohanes Nugroho <yohanes@gmail.com>
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_vlan_var.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

#include <net/bpf.h>
#include <net/bpfdesc.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <arm/at91/at91_pmcvar.h>
#include <arm/at91/if_macbreg.h>
#include <arm/at91/if_macbvar.h>
#include <arm/at91/at91_piovar.h>

#include <arm/at91/at91sam9g20reg.h>

#include <machine/bus.h>
#include <machine/intr.h>

/* "device miibus" required.  See GENERIC if you get errors here. */
#include "miibus_if.h"


#define	MACB_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	MACB_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define	MACB_LOCK_INIT(_sc)					\
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->dev),	\
	    MTX_NETWORK_LOCK, MTX_DEF)
#define	MACB_LOCK_DESTROY(_sc)	mtx_destroy(&_sc->sc_mtx);
#define	MACB_LOCK_ASSERT(_sc)	mtx_assert(&_sc->sc_mtx, MA_OWNED);
#define	MACB_ASSERT_UNLOCKED(_sc) mtx_assert(&_sc->sc_mtx, MA_NOTOWNED);


static inline uint32_t
read_4(struct macb_softc *sc, bus_size_t off)
{

	return (bus_read_4(sc->mem_res, off));
}

static inline void
write_4(struct macb_softc *sc, bus_size_t off, uint32_t val)
{

	bus_write_4(sc->mem_res, off, val);
}


static devclass_t macb_devclass;

/* ifnet entry points */

static void	macbinit_locked(void *);
static void	macbstart_locked(struct ifnet *);

static void	macbinit(void *);
static void	macbstart(struct ifnet *);
static void	macbstop(struct macb_softc *);
static int	macbioctl(struct ifnet * ifp, u_long, caddr_t);

/* bus entry points */

static int	macb_probe(device_t dev);
static int	macb_attach(device_t dev);
static int	macb_detach(device_t dev);

/* helper functions */
static int
macb_new_rxbuf(struct macb_softc *sc, int index);

static void
macb_free_desc_dma_tx(struct macb_softc *sc);

static void
macb_free_desc_dma_rx(struct macb_softc *sc);

static void
macb_init_desc_dma_tx(struct macb_softc *sc);

static void
macb_watchdog(struct macb_softc *sc);

static int macb_intr_rx_locked(struct macb_softc *sc, int count);
static void macb_intr_task(void *arg, int pending __unused);
static void macb_intr(void *xsc);

static void
macb_tx_cleanup(struct macb_softc *sc);

static inline int
phy_write(struct macb_softc *sc, int phy, int reg, int data);

static void	macb_reset(struct macb_softc *sc);

static void
macb_deactivate(device_t dev)
{
	struct macb_softc *sc;

	sc = device_get_softc(dev);

	macb_free_desc_dma_tx(sc);
	macb_free_desc_dma_rx(sc);

}

static void
macb_getaddr(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	bus_addr_t *paddr;

	KASSERT(nsegs == 1, ("wrong number of segments, should be 1"));
	paddr = arg;
	*paddr = segs->ds_addr;
}

static int
macb_alloc_desc_dma_tx(struct macb_softc *sc)
{
	int error, i;

	/* Allocate a busdma tag and DMA safe memory for TX/RX descriptors. */
	error = bus_dma_tag_create(sc->sc_parent_tag,	/* parent */
	    16, 0,			/* alignment, boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filtfunc, filtfuncarg */
	    sizeof(struct eth_tx_desc) * MACB_MAX_TX_BUFFERS, /* max size */
	    1,				/* nsegments */
	    sizeof(struct eth_tx_desc) * MACB_MAX_TX_BUFFERS,
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockfuncarg */
	    &sc->dmatag_data_tx);	/* dmat */
	if (error != 0) {
		device_printf(sc->dev,
		    "Couldn't create TX descriptor dma tag\n");
		return (error);
	}
	/* Allocate memory for TX ring. */
	error = bus_dmamem_alloc(sc->dmatag_data_tx,
	    (void**)&(sc->desc_tx), BUS_DMA_NOWAIT | BUS_DMA_ZERO |
	    BUS_DMA_COHERENT, &sc->dmamap_ring_tx);
	if (error != 0) {
		device_printf(sc->dev, "failed to allocate TX dma memory\n");
		return (error);
	}
	/* Load Ring DMA. */
	error = bus_dmamap_load(sc->dmatag_data_tx, sc->dmamap_ring_tx,
	    sc->desc_tx, sizeof(struct eth_tx_desc) * MACB_MAX_TX_BUFFERS,
	    macb_getaddr, &sc->ring_paddr_tx, BUS_DMA_NOWAIT);
	if (error != 0) {
		device_printf(sc->dev, "can't load TX descriptor dma map\n");
		return (error);
	}
	/* Allocate a busdma tag for mbufs. No alignment restriction applys. */
	error = bus_dma_tag_create(sc->sc_parent_tag,	/* parent */
	    1, 0,			/* alignment, boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filtfunc, filtfuncarg */
	    MCLBYTES * MAX_FRAGMENT,	/* maxsize */
	    MAX_FRAGMENT,		/* nsegments */
	    MCLBYTES, 0,		/* maxsegsz, flags */
	    NULL, NULL,			/* lockfunc, lockfuncarg */
	    &sc->dmatag_ring_tx);	/* dmat */
	if (error != 0) {
		device_printf(sc->dev, "failed to create TX mbuf dma tag\n");
		return (error);
	}

	for (i = 0; i < MACB_MAX_TX_BUFFERS; i++) {
		/* Create dma map for each descriptor. */
		error = bus_dmamap_create(sc->dmatag_ring_tx, 0,
		    &sc->tx_desc[i].dmamap);
		if (error != 0) {
			device_printf(sc->dev,
			    "failed to create TX mbuf dma map\n");
			return (error);
		}
	}
	return (0);
}

static void
macb_free_desc_dma_tx(struct macb_softc *sc)
{
	struct tx_desc_info *td;
	int i;

	/* TX buffers. */
	if (sc->dmatag_ring_tx != NULL) {
		for (i = 0; i < MACB_MAX_TX_BUFFERS; i++) {
			td = &sc->tx_desc[i];
			if (td->dmamap != NULL) {
				bus_dmamap_destroy(sc->dmatag_ring_tx,
				    td->dmamap);
				td->dmamap = NULL;
			}
		}
		bus_dma_tag_destroy(sc->dmatag_ring_tx);
		sc->dmatag_ring_tx = NULL;
	}

	/* TX descriptor ring. */
	if (sc->dmatag_data_tx != NULL) {
		if (sc->ring_paddr_tx != 0)
			bus_dmamap_unload(sc->dmatag_data_tx,
			    sc->dmamap_ring_tx);
		if (sc->desc_tx != NULL)
			bus_dmamem_free(sc->dmatag_data_tx, sc->desc_tx,
			    sc->dmamap_ring_tx);
		sc->ring_paddr_tx = 0;
		sc->desc_tx = NULL;
		bus_dma_tag_destroy(sc->dmatag_data_tx);
		sc->dmatag_data_tx = NULL;
	}
}

static void
macb_init_desc_dma_tx(struct macb_softc *sc)
{
	struct eth_tx_desc *desc;
	int i;

	MACB_LOCK_ASSERT(sc);

	sc->tx_prod = 0;
	sc->tx_cons = 0;
	sc->tx_cnt = 0;

	desc = &sc->desc_tx[0];
	bzero(desc, sizeof(struct eth_tx_desc) * MACB_MAX_TX_BUFFERS);

	for (i = 0; i < MACB_MAX_TX_BUFFERS; i++) {
		desc = &sc->desc_tx[i];
		if (i == MACB_MAX_TX_BUFFERS - 1)
			desc->flags = TD_OWN | TD_WRAP_MASK;
		else
			desc->flags = TD_OWN;
	}

	bus_dmamap_sync(sc->dmatag_data_tx, sc->dmamap_ring_tx,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
}

static int
macb_alloc_desc_dma_rx(struct macb_softc *sc)
{
	int error, i;

	/* Allocate a busdma tag and DMA safe memory for RX descriptors. */
	error = bus_dma_tag_create(sc->sc_parent_tag,	/* parent */
	    16, 0,			/* alignment, boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filtfunc, filtfuncarg */
	    /* maxsize, nsegments */
	    sizeof(struct eth_rx_desc) * MACB_MAX_RX_BUFFERS, 1,
	    /* maxsegsz, flags */
	    sizeof(struct eth_rx_desc) * MACB_MAX_RX_BUFFERS, 0,
	    NULL, NULL,			/* lockfunc, lockfuncarg */
	    &sc->dmatag_data_rx);	/* dmat */
	if (error != 0) {
		device_printf(sc->dev,
		    "Couldn't create RX descriptor dma tag\n");
		return (error);
	}
	/* Allocate RX ring. */
	error = bus_dmamem_alloc(sc->dmatag_data_rx, (void**)&(sc->desc_rx),
	    BUS_DMA_NOWAIT | BUS_DMA_ZERO | BUS_DMA_COHERENT,
	    &sc->dmamap_ring_rx);
	if (error != 0) {
		device_printf(sc->dev,
		    "failed to allocate RX descriptor dma memory\n");
		return (error);
	}

	/* Load dmamap. */
	error = bus_dmamap_load(sc->dmatag_data_rx, sc->dmamap_ring_rx,
	    sc->desc_rx, sizeof(struct eth_rx_desc) * MACB_MAX_RX_BUFFERS,
	    macb_getaddr, &sc->ring_paddr_rx, BUS_DMA_NOWAIT);
	if (error != 0) {
		device_printf(sc->dev, "can't load RX descriptor dma map\n");
		return (error);
	}

	/* Allocate a busdma tag for mbufs. */
	error = bus_dma_tag_create(sc->sc_parent_tag,/* parent */
	    16, 0,			/* alignment, boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filtfunc, filtfuncarg */
	    MCLBYTES, 1,		/* maxsize, nsegments */
	    MCLBYTES, 0,		/* maxsegsz, flags */
	    NULL, NULL,			/* lockfunc, lockfuncarg */
	    &sc->dmatag_ring_rx);	/* dmat */

	if (error != 0) {
		device_printf(sc->dev, "failed to create RX mbuf dma tag\n");
		return (error);
	}

	for (i = 0; i < MACB_MAX_RX_BUFFERS; i++) {
		error = bus_dmamap_create(sc->dmatag_ring_rx, 0,
		    &sc->rx_desc[i].dmamap);
		if (error != 0) {
			device_printf(sc->dev,
			    "failed to create RX mbuf dmamap\n");
			return (error);
		}
	}

	return (0);
}

static void
macb_free_desc_dma_rx(struct macb_softc *sc)
{
	struct rx_desc_info *rd;
	int i;

	/* RX buffers. */
	if (sc->dmatag_ring_rx != NULL) {
		for (i = 0; i < MACB_MAX_RX_BUFFERS; i++) {
			rd = &sc->rx_desc[i];
			if (rd->dmamap != NULL) {
				bus_dmamap_destroy(sc->dmatag_ring_rx,
				    rd->dmamap);
				rd->dmamap = NULL;
			}
		}
		bus_dma_tag_destroy(sc->dmatag_ring_rx);
		sc->dmatag_ring_rx = NULL;
	}
	/* RX descriptor ring. */
	if (sc->dmatag_data_rx != NULL) {
		if (sc->ring_paddr_rx != 0)
			bus_dmamap_unload(sc->dmatag_data_rx,
			    sc->dmamap_ring_rx);
		if (sc->desc_rx != NULL)
			bus_dmamem_free(sc->dmatag_data_rx, sc->desc_rx,
			    sc->dmamap_ring_rx);
		sc->ring_paddr_rx = 0;
		sc->desc_rx = NULL;
		bus_dma_tag_destroy(sc->dmatag_data_rx);
		sc->dmatag_data_rx = NULL;
	}
}

static int
macb_init_desc_dma_rx(struct macb_softc *sc)
{
	struct eth_rx_desc *desc;
	struct rx_desc_info *rd;
	int i;

	MACB_LOCK_ASSERT(sc);

	sc->rx_cons = 0;
	desc = &sc->desc_rx[0];
	bzero(desc, sizeof(struct eth_rx_desc) * MACB_MAX_RX_BUFFERS);
	for (i = 0; i < MACB_MAX_RX_BUFFERS; i++) {
		rd = &sc->rx_desc[i];
		rd->buff = NULL;
		if (macb_new_rxbuf(sc, i) != 0)
			return (ENOBUFS);
	}
	bus_dmamap_sync(sc->dmatag_ring_rx, sc->dmamap_ring_rx,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	return (0);
}

static int
macb_new_rxbuf(struct macb_softc *sc, int index)
{
	struct rx_desc_info *rd;
	struct eth_rx_desc *desc;
	struct mbuf *m;
	bus_dma_segment_t seg[1];
	int error, nsegs;

	m = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
	if (m == NULL)
		return (ENOBUFS);
	m->m_len = m->m_pkthdr.len = MCLBYTES - ETHER_ALIGN;
	rd = &sc->rx_desc[index];
	bus_dmamap_unload(sc->dmatag_ring_rx, rd->dmamap);
	error = bus_dmamap_load_mbuf_sg(sc->dmatag_ring_rx, rd->dmamap, m,
	    seg, &nsegs, 0);
	KASSERT(nsegs == 1, ("Too many segments returned!"));
	if (error != 0) {
		m_free(m);
		return (error);
	}

	bus_dmamap_sync(sc->dmatag_ring_rx, rd->dmamap, BUS_DMASYNC_PREREAD);
	rd->buff = m;

	desc = &sc->desc_rx[index];
	desc->addr = seg[0].ds_addr;

	desc->flags = DATA_SIZE;

	if (index == MACB_MAX_RX_BUFFERS - 1)
		desc->addr |= RD_WRAP_MASK;

	return (0);
}

static int
macb_allocate_dma(struct macb_softc *sc)
{
	int error;

	/* Create parent tag for tx and rx */
	error = bus_dma_tag_create(
	    bus_get_dma_tag(sc->dev),	/* parent */
	    1, 0,			/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    BUS_SPACE_MAXSIZE_32BIT, 0,	/* maxsize, nsegments */
	    BUS_SPACE_MAXSIZE_32BIT,	/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,		/* lockfunc, lockarg */
	    &sc->sc_parent_tag);
	if (error != 0) {
		device_printf(sc->dev, "Couldn't create parent DMA tag\n");
		return (error);
	}

	if ((error = macb_alloc_desc_dma_tx(sc)) != 0)
		return (error);
	if ((error = macb_alloc_desc_dma_rx(sc)) != 0)
		return (error);
	return (0);
}


static void
macb_tick(void *xsc)
{
	struct macb_softc *sc;
	struct mii_data *mii;

	sc = xsc;
	mii = device_get_softc(sc->miibus);
	mii_tick(mii);
	macb_watchdog(sc);
	/*
	 * Schedule another timeout one second from now.
	 */
	callout_reset(&sc->tick_ch, hz, macb_tick, sc);
}


static void
macb_watchdog(struct macb_softc *sc)
{
	struct ifnet *ifp;

	MACB_LOCK_ASSERT(sc);

	if (sc->macb_watchdog_timer == 0 || --sc->macb_watchdog_timer)
		return;

	ifp = sc->ifp;
	if ((sc->flags & MACB_FLAG_LINK) == 0) {
		if_printf(ifp, "watchdog timeout (missed link)\n");
		if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
		return;
	}

	if_printf(ifp, "watchdog timeout\n");
	if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	macbinit_locked(sc);
	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		macbstart_locked(ifp);
}



static void
macbinit_locked(void *xsc)
{
	struct macb_softc *sc;
	struct ifnet *ifp;
	int err;
	uint32_t config;
	struct mii_data *mii;

	sc = xsc;
	ifp = sc->ifp;

	MACB_LOCK_ASSERT(sc);

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) != 0)
		return;

	if ((err = macb_init_desc_dma_rx(sc)) != 0) {
		device_printf(sc->dev, "no memory for RX buffers\n");
		//ecestop(sc);
		return;
	}
	macb_init_desc_dma_tx(sc);

	config = read_4(sc, EMAC_NCFGR) | (sc->clock << 10); /*set clock*/
	config |= CFG_PAE;		/* PAuse Enable */
	config |= CFG_DRFCS;		/* Discard Rx FCS */
	config |= CFG_SPD;		/* 100 mbps*/
	//config |= CFG_CAF;
	config |= CFG_FD;

	config |= CFG_RBOF_2; /*offset +2*/

	write_4(sc, EMAC_NCFGR, config);

	/* Initialize TX and RX buffers */
	write_4(sc, EMAC_RBQP, sc->ring_paddr_rx);
	write_4(sc, EMAC_TBQP, sc->ring_paddr_tx);

	/* Enable TX and RX */
	write_4(sc, EMAC_NCR, RX_ENABLE | TX_ENABLE | MPE_ENABLE);


	/* Enable interrupts */
	write_4(sc, EMAC_IER, (RCOMP_INTERRUPT |
			       RXUBR_INTERRUPT |
			       TUND_INTERRUPT |
			       RLE_INTERRUPT |
			       TXERR_INTERRUPT |
			       ROVR_INTERRUPT |
			       HRESP_INTERRUPT|
			       TCOMP_INTERRUPT
			));

	/*
	 * Set 'running' flag, and clear output active flag
	 * and attempt to start the output
	 */
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	mii = device_get_softc(sc->miibus);

	sc->flags |= MACB_FLAG_LINK;

	mii_mediachg(mii);

	callout_reset(&sc->tick_ch, hz, macb_tick, sc);
}


static void
macb_tx_cleanup(struct macb_softc *sc)
{
	struct ifnet *ifp;
	struct eth_tx_desc *desc;
	struct tx_desc_info *td;
	int flags;
	int status;
	int i;

	MACB_LOCK_ASSERT(sc);

	status = read_4(sc, EMAC_TSR);

	write_4(sc, EMAC_TSR, status);

	/*buffer underrun*/
	if ((status & TSR_UND) != 0) {
		/*reset buffers*/
		printf("underrun\n");
		bus_dmamap_sync(sc->dmatag_data_tx, sc->dmamap_ring_tx,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		sc->tx_cons = sc->tx_prod = 0;
		for (i = 0; i < MACB_MAX_TX_BUFFERS; i++) {
			desc = &sc->desc_tx[i];
			desc->flags = TD_OWN;
		}

		for (i = 0; i < MACB_MAX_TX_BUFFERS; i++) {
			td = &sc->tx_desc[i];
			if (td->buff != NULL) {
				/* We are finished with this descriptor. */
				bus_dmamap_sync(sc->dmatag_ring_tx, td->dmamap,
						BUS_DMASYNC_POSTWRITE);
				/* ... and unload, so we can reuse. */
				bus_dmamap_unload(sc->dmatag_data_tx,
						  td->dmamap);
				m_freem(td->buff);
				td->buff = NULL;
			}
		}
	}

	if ((status & TSR_COMP) == 0)
		return;


	if (sc->tx_cons == sc->tx_prod)
		return;

	ifp = sc->ifp;

	/* Prepare to read the ring (owner bit). */
	bus_dmamap_sync(sc->dmatag_data_tx, sc->dmamap_ring_tx,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	while (sc->tx_cons != sc->tx_prod) {
		desc = &sc->desc_tx[sc->tx_cons];
		if ((desc->flags & TD_OWN) == 0)
			break;

		td = &sc->tx_desc[sc->tx_cons];
		if (td->buff != NULL) {
			/* We are finished with this descriptor. */
			bus_dmamap_sync(sc->dmatag_ring_tx, td->dmamap,
					BUS_DMASYNC_POSTWRITE);
			/* ... and unload, so we can reuse. */
			bus_dmamap_unload(sc->dmatag_data_tx,
					  td->dmamap);
			m_freem(td->buff);
			td->buff = NULL;
			if_inc_counter(ifp, IFCOUNTER_OPACKETS, 1);
		}

		do {
			sc->tx_cnt--;
			MACB_DESC_INC(sc->tx_cons, MACB_MAX_TX_BUFFERS);
			ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
			flags = desc->flags;
			desc->flags = TD_OWN;
			desc = &sc->desc_tx[sc->tx_cons];
			if (flags & TD_LAST) {
				break;
			}
		} while (sc->tx_cons != sc->tx_prod);
	}

	/* Unarm watchog timer when there is no pending descriptors in queue. */
	if (sc->tx_cnt == 0)
		sc->macb_watchdog_timer = 0;
}

static void
macb_rx(struct macb_softc *sc)
{
	struct eth_rx_desc	*rxdesc;
	struct ifnet *ifp;
	struct mbuf *m;
	int rxbytes;
	int flags;
	int nsegs;
	int first;

	rxdesc = &(sc->desc_rx[sc->rx_cons]);

	ifp = sc->ifp;

	bus_dmamap_sync(sc->dmatag_ring_rx, sc->dmamap_ring_rx,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);


	nsegs = 0;
	while (rxdesc->addr & RD_OWN) {

		if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
			break;

		flags = rxdesc->flags;

		rxbytes = flags & RD_LEN_MASK;

		m = sc->rx_desc[sc->rx_cons].buff;

		bus_dmamap_sync(sc->dmatag_ring_rx,
		    sc->rx_desc[sc->rx_cons].dmamap, BUS_DMASYNC_POSTREAD);
		if (macb_new_rxbuf(sc, sc->rx_cons) != 0) {
			if_inc_counter(ifp, IFCOUNTER_IQDROPS, 1);
			first = sc->rx_cons;
			
			do  {
				rxdesc->flags = DATA_SIZE;
				MACB_DESC_INC(sc->rx_cons, MACB_MAX_RX_BUFFERS);
				if ((rxdesc->flags & RD_EOF) != 0)
					break;
				rxdesc = &(sc->desc_rx[sc->rx_cons]);
			} while (sc->rx_cons != first);

			if (sc->macb_cdata.rxhead != NULL) {
				m_freem(sc->macb_cdata.rxhead);
				sc->macb_cdata.rxhead = NULL;
				sc->macb_cdata.rxtail = NULL;				
			}
			
			break;
		}

		nsegs++;

		/* Chain received mbufs. */
		if (sc->macb_cdata.rxhead == NULL) {
			m->m_data += 2;
			sc->macb_cdata.rxhead = m;
			sc->macb_cdata.rxtail = m;
			if (flags & RD_EOF)
				m->m_len = rxbytes;
			else
				m->m_len = DATA_SIZE - 2;
		} else {
			m->m_flags &= ~M_PKTHDR;
			m->m_len = DATA_SIZE;
			sc->macb_cdata.rxtail->m_next = m;
			sc->macb_cdata.rxtail = m;
		}

		if (flags & RD_EOF) {

			if (nsegs > 1) {
				sc->macb_cdata.rxtail->m_len = (rxbytes -
				    ((nsegs - 1) * DATA_SIZE)) + 2;
			}

			m = sc->macb_cdata.rxhead;
			m->m_flags |= M_PKTHDR;
			m->m_pkthdr.len = rxbytes;
			m->m_pkthdr.rcvif = ifp;
			if_inc_counter(ifp, IFCOUNTER_IPACKETS, 1);

			nsegs = 0;
			MACB_UNLOCK(sc);
			(*ifp->if_input)(ifp, m);
			MACB_LOCK(sc);
			sc->macb_cdata.rxhead = NULL;
			sc->macb_cdata.rxtail = NULL;

		}
		
		rxdesc->addr &= ~RD_OWN;

		MACB_DESC_INC(sc->rx_cons, MACB_MAX_RX_BUFFERS);

		rxdesc = &(sc->desc_rx[sc->rx_cons]);
	}

	write_4(sc, EMAC_IER, (RCOMP_INTERRUPT|RXUBR_INTERRUPT));

}

static int
macb_intr_rx_locked(struct macb_softc *sc, int count)
{
	macb_rx(sc);
	return (0);
}

static void
macb_intr_task(void *arg, int pending __unused)
{
	struct macb_softc *sc;

	sc = arg;
	MACB_LOCK(sc);
	macb_intr_rx_locked(sc, -1);
	MACB_UNLOCK(sc);
}

static void
macb_intr(void *xsc)
{
	struct macb_softc *sc;
	struct ifnet *ifp;
	uint32_t status;

	sc = xsc;
	ifp = sc->ifp;
	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
		printf("not running\n");
		return;
	}

	MACB_LOCK(sc);
	status = read_4(sc, EMAC_ISR);

	while (status) {
		if (status & RCOMP_INTERRUPT) {
			write_4(sc, EMAC_IDR, (RCOMP_INTERRUPT|RXUBR_INTERRUPT));
			taskqueue_enqueue(sc->sc_tq, &sc->sc_intr_task);
		}

		if (status & TCOMP_INTERRUPT) {
			macb_tx_cleanup(sc);
		}

		status = read_4(sc, EMAC_ISR);
	}

	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		macbstart_locked(ifp);
	MACB_UNLOCK(sc);
}

static inline int
macb_encap(struct macb_softc *sc, struct mbuf **m_head)
{
	struct eth_tx_desc *desc;
	struct tx_desc_info *txd, *txd_last;
	struct mbuf *m;
	bus_dma_segment_t segs[MAX_FRAGMENT];
	bus_dmamap_t map;
	uint32_t csum_flags;
	int error, i, nsegs, prod, si;

	M_ASSERTPKTHDR((*m_head));

	prod = sc->tx_prod;

	m = *m_head;

	txd = txd_last = &sc->tx_desc[prod];
	error = bus_dmamap_load_mbuf_sg(sc->dmatag_ring_tx, txd->dmamap,
	    *m_head, segs, &nsegs, 0);
	if (error == EFBIG) {
		m = m_collapse(*m_head, M_NOWAIT, MAX_FRAGMENT);
		if (m == NULL) {
			m_freem(*m_head);
			*m_head = NULL;
			return (ENOMEM);
		}
		*m_head = m;
		error = bus_dmamap_load_mbuf_sg(sc->dmatag_ring_tx, txd->dmamap,
		    *m_head, segs, &nsegs, 0);
		if (error != 0) {
			m_freem(*m_head);
			*m_head = NULL;
			return (error);
		}
	} else if (error != 0) {
		return (error);
	}
	/* Check for TX descriptor overruns. */
	if (sc->tx_cnt + nsegs > MACB_MAX_TX_BUFFERS - 1) {
		bus_dmamap_unload(sc->dmatag_ring_tx, txd->dmamap);
		return (ENOBUFS);
	}
	bus_dmamap_sync(sc->dmatag_ring_tx, txd->dmamap, BUS_DMASYNC_PREWRITE);
	m = *m_head;

	/* TODO: VLAN hardware tag insertion. */

	csum_flags = 0;
	si = prod;
	desc = NULL;

	for (i = 0; i < nsegs; i++) {
		desc = &sc->desc_tx[prod];
		desc->addr = segs[i].ds_addr;

		if (i == 0 ) {
			desc->flags = segs[i].ds_len | TD_OWN;
		} else {
			desc->flags = segs[i].ds_len;
		}

		if (prod == MACB_MAX_TX_BUFFERS - 1)
			desc->flags |= TD_WRAP_MASK;

		sc->tx_cnt++;
		MACB_DESC_INC(prod, MACB_MAX_TX_BUFFERS);
	}
	/*
	 * Set EOP on the last fragment.
	 */

	desc->flags |= TD_LAST;
	desc = &sc->desc_tx[si];
	desc->flags &= ~TD_OWN;

	sc->tx_prod = prod;

	/* Swap the first dma map and the last. */
	map = txd_last->dmamap;
	txd_last->dmamap = txd->dmamap;
	txd->dmamap = map;
	txd->buff = m;

	return (0);
}


static void
macbstart_locked(struct ifnet *ifp)
{



	struct macb_softc *sc;
	struct mbuf *m0;
#if 0
	struct mbuf *m_new;
#endif
	int queued = 0;

	sc = ifp->if_softc;

	if ((ifp->if_drv_flags & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING || (sc->flags & MACB_FLAG_LINK) == 0) {
		return;
	}

	while (!IFQ_DRV_IS_EMPTY(&ifp->if_snd)) {
		/* Get packet from the queue */
		IF_DEQUEUE(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;
#if 0
		if (m0->m_next != NULL) {
			/* Fragmented mbuf chain, collapse it. */
			m_new = m_defrag(m0, M_NOWAIT);
			if (m_new != NULL) {
				/* Original frame freed. */
				m0 = m_new;
			} else {
				/* Defragmentation failed, just use the chain. */
			}
		}
#endif
		if (macb_encap(sc, &m0)) {
			if (m0 == NULL)
				break;
			IF_PREPEND(&ifp->if_snd, m0);
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			break;
		}
		queued++;
		BPF_MTAP(ifp, m0);
	}
	if (IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	if (queued) {
		bus_dmamap_sync(sc->dmatag_data_tx, sc->dmamap_ring_tx,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		write_4(sc, EMAC_NCR, read_4(sc, EMAC_NCR) | TRANSMIT_START);
		sc->macb_watchdog_timer = MACB_TIMEOUT;
	}
}

static void
macbinit(void *xsc)
{
	struct macb_softc *sc = xsc;

	MACB_LOCK(sc);
	macbinit_locked(sc);
	MACB_UNLOCK(sc);
}

static void
macbstart(struct ifnet *ifp)
{
	struct macb_softc *sc = ifp->if_softc;
	MACB_ASSERT_UNLOCKED(sc);
	MACB_LOCK(sc);
	macbstart_locked(ifp);
	MACB_UNLOCK(sc);

}


static void
macbstop(struct macb_softc *sc)
{
	struct ifnet *ifp = sc->ifp;
	struct rx_desc_info *rd;
	struct tx_desc_info *td;
	int i;

	ifp = sc->ifp;

	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	macb_reset(sc);

	sc->flags &= ~MACB_FLAG_LINK;
	callout_stop(&sc->tick_ch);
	sc->macb_watchdog_timer = 0;

	/* Free TX/RX mbufs still in the queues. */
	for (i = 0; i < MACB_MAX_TX_BUFFERS; i++) {
		td = &sc->tx_desc[i];
		if (td->buff != NULL) {
			bus_dmamap_sync(sc->dmatag_ring_tx, td->dmamap,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->dmatag_data_tx, td->dmamap);
			m_freem(td->buff);
			td->buff = NULL;
		}
	}
	for (i = 0; i < MACB_MAX_RX_BUFFERS; i++) {
		rd = &sc->rx_desc[i];
		if (rd->buff != NULL) {
			bus_dmamap_sync(sc->dmatag_ring_rx, rd->dmamap,
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->dmatag_data_rx, rd->dmamap);
			m_freem(rd->buff);
			rd->buff = NULL;
		}
	}	
}

static int
get_hash_index(uint8_t *mac)
{
	int i, j, k;
	int result;
	int bit;

	result = 0;
	for (i = 0; i < 6; i++) {
		bit = 0;
		for (j = 0; j < 8;  j++) {
			k = j * 6 + i;
			bit ^= (mac[k/8] & (1 << (k % 8)) ) != 0;
		}
		result |= bit;
	}
	return result;
}

static void
set_mac_filter(uint32_t *filter, uint8_t *mac)
{
	int bits;

	bits = get_hash_index(mac);
	filter[bits >> 5] |= 1 << (bits & 31);
}

static void
set_filter(struct macb_softc *sc)
{
	struct ifnet *ifp;
	struct ifmultiaddr *ifma;
	int config;
	int count;
	uint32_t multicast_filter[2];

	ifp = sc->ifp;

	config = read_4(sc, EMAC_NCFGR);
	
	config &= ~(CFG_CAF | CFG_MTI);
	write_4(sc, EMAC_HRB, 0);
	write_4(sc, EMAC_HRT, 0);

	if ((ifp->if_flags & (IFF_ALLMULTI |IFF_PROMISC)) != 0){
		if ((ifp->if_flags & IFF_ALLMULTI) != 0) {
			write_4(sc, EMAC_HRB, ~0);
			write_4(sc, EMAC_HRT, ~0);
			config |= CFG_MTI;
		}
		if ((ifp->if_flags & IFF_PROMISC) != 0) {
			config |= CFG_CAF;
		}
		write_4(sc, EMAC_NCFGR, config);
		return;
	}

	if_maddr_rlock(ifp);
	count = 0;
	multicast_filter[0] = 0;
	multicast_filter[1] = 0;

	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		count++;
		set_mac_filter(multicast_filter,
			   LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
	}
	if (count) {
		write_4(sc, EMAC_HRB, multicast_filter[0]);
		write_4(sc, EMAC_HRT, multicast_filter[1]);
		write_4(sc, EMAC_NCFGR, config|CFG_MTI);
	}
	if_maddr_runlock(ifp);
}

static int
macbioctl(struct ifnet * ifp, u_long cmd, caddr_t data)
{

	struct macb_softc *sc = ifp->if_softc;
	struct mii_data *mii;
	struct ifreq *ifr = (struct ifreq *)data;

	int error = 0;

	switch (cmd) {
	case SIOCSIFFLAGS:
		MACB_LOCK(sc);

		if ((ifp->if_flags & IFF_UP) != 0) {
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING) != 0) {
				if (((ifp->if_flags ^ sc->if_flags)
				    & (IFF_PROMISC | IFF_ALLMULTI)) != 0)
					set_filter(sc);
			} else {
				macbinit_locked(sc);
			}
		} else if ((ifp->if_drv_flags & IFF_DRV_RUNNING) != 0) {
			macbstop(sc);
		}
		sc->if_flags = ifp->if_flags;
		MACB_UNLOCK(sc);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		MACB_LOCK(sc);
		if ((ifp->if_drv_flags & IFF_DRV_RUNNING) != 0)
			set_filter(sc);

		MACB_UNLOCK(sc);
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		mii = device_get_softc(sc->miibus);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, cmd);
		break;
	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}
	return (error);

}

/* bus entry points */

static int
macb_probe(device_t dev)
{
	device_set_desc(dev, "macb");
	return (0);
}

/*
 * Change media according to request.
 */
static int
macb_ifmedia_upd(struct ifnet *ifp)
{
	struct macb_softc *sc = ifp->if_softc;
	struct mii_data *mii;

	mii = device_get_softc(sc->miibus);
	MACB_LOCK(sc);
	mii_mediachg(mii);
	MACB_UNLOCK(sc);
	return (0);
}

/*
 * Notify the world which media we're using.
 */
static void
macb_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct macb_softc *sc = ifp->if_softc;
	struct mii_data *mii;

	mii = device_get_softc(sc->miibus);

	MACB_LOCK(sc);
	/* Don't report link state if driver is not running. */
	if ((ifp->if_flags & IFF_UP) == 0) {
		MACB_UNLOCK(sc);
		return;
	}
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
	MACB_UNLOCK(sc);
}

static void
macb_reset(struct macb_softc *sc)
{
	/*
	 * Disable RX and TX
	 */
	write_4(sc, EMAC_NCR, 0);

	write_4(sc, EMAC_NCR, CLEAR_STAT);

	/* Clear all status flags */
	write_4(sc, EMAC_TSR, ~0UL);
	write_4(sc, EMAC_RSR, ~0UL);

	/* Disable all interrupts */
	write_4(sc, EMAC_IDR, ~0UL);
	read_4(sc, EMAC_ISR);

}


static int
macb_get_mac(struct macb_softc *sc, u_char *eaddr)
{
	uint32_t bottom;
	uint16_t top;

	bottom = read_4(sc, EMAC_SA1B);
	top = read_4(sc, EMAC_SA1T);

	eaddr[0] = bottom & 0xff;
	eaddr[1] = (bottom >> 8) & 0xff;
	eaddr[2] = (bottom >> 16) & 0xff;
	eaddr[3] = (bottom >> 24) & 0xff;
	eaddr[4] = top & 0xff;
	eaddr[5] = (top >> 8) & 0xff;

	return (0);
}


static int
macb_attach(device_t dev)
{
	struct macb_softc *sc;
	struct ifnet *ifp = NULL;
	struct sysctl_ctx_list *sctx;
	struct sysctl_oid *soid;
	int pclk_hz;
	u_char eaddr[ETHER_ADDR_LEN];
	int rid;
	int err;
	struct at91_pmc_clock *master;


	err = 0;

	sc = device_get_softc(dev);
	sc->dev = dev;

	MACB_LOCK_INIT(sc);

	callout_init_mtx(&sc->tick_ch, &sc->sc_mtx, 0);

	/*
	 * Allocate resources.
	 */
	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev, "could not allocate memory resources.\n");
		err = ENOMEM;
		goto out;
	}
	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (sc->irq_res == NULL) {
		device_printf(dev, "could not allocate interrupt resources.\n");
		err = ENOMEM;
		goto out;
	}

	/*setup clock*/
	sc->clk = at91_pmc_clock_ref(device_get_nameunit(sc->dev));
	at91_pmc_clock_enable(sc->clk);

	macb_reset(sc);
	macb_get_mac(sc, eaddr);

	master = at91_pmc_clock_ref("mck");

	pclk_hz = master->hz;

	sc->clock = CFG_CLK_8;
	if (pclk_hz <= 20000000)
		sc->clock = CFG_CLK_8;
	else if (pclk_hz <= 40000000)
		sc->clock = CFG_CLK_16;
	else if (pclk_hz <= 80000000)
		sc->clock = CFG_CLK_32;
	else
		sc->clock = CFG_CLK_64;

	sc->clock = sc->clock << 10;

	write_4(sc, EMAC_NCFGR, sc->clock);
	write_4(sc, EMAC_USRIO, USRIO_CLOCK);       //enable clock

	write_4(sc, EMAC_NCR, MPE_ENABLE); //enable MPE

	sc->ifp = ifp = if_alloc(IFT_ETHER);
	err = mii_attach(dev, &sc->miibus, ifp, macb_ifmedia_upd,
	    macb_ifmedia_sts, BMSR_DEFCAPMASK, MII_PHY_ANY, MII_OFFSET_ANY, 0);
	if (err != 0) {
		device_printf(dev, "attaching PHYs failed\n");
		goto out;
	}

	if (macb_allocate_dma(sc) != 0)
		goto out;

	/* Sysctls */
	sctx = device_get_sysctl_ctx(dev);
	soid = device_get_sysctl_tree(dev);

	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_capabilities |= IFCAP_VLAN_MTU;
	ifp->if_capenable |= IFCAP_VLAN_MTU;	/* The hw bits already set. */
	ifp->if_start = macbstart;
	ifp->if_ioctl = macbioctl;
	ifp->if_init = macbinit;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ifp->if_snd.ifq_drv_maxlen = IFQ_MAXLEN;
	IFQ_SET_READY(&ifp->if_snd);
	sc->if_flags = ifp->if_flags;

	TASK_INIT(&sc->sc_intr_task, 0, macb_intr_task, sc);

	sc->sc_tq = taskqueue_create_fast("macb_taskq", M_WAITOK,
	    taskqueue_thread_enqueue, &sc->sc_tq);
	if (sc->sc_tq == NULL) {
		device_printf(sc->dev, "could not create taskqueue\n");
		goto out;
	}

	ether_ifattach(ifp, eaddr);

	/*
	 * Activate the interrupt.
	 */
	err = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, macb_intr, sc, &sc->intrhand);
	if (err) {
		device_printf(dev, "could not establish interrupt handler.\n");
		ether_ifdetach(ifp);
		goto out;
	}

	taskqueue_start_threads(&sc->sc_tq, 1, PI_NET, "%s taskq",
	    device_get_nameunit(sc->dev));

	sc->macb_cdata.rxhead = 0;
	sc->macb_cdata.rxtail = 0;

	phy_write(sc, 0, 0, 0x3300); //force autoneg

	return (0);
out:

	return (err);
}

static int
macb_detach(device_t dev)
{
	struct macb_softc *sc;

	sc = device_get_softc(dev);
	ether_ifdetach(sc->ifp);
	MACB_LOCK(sc);
	macbstop(sc);
	MACB_UNLOCK(sc);
	callout_drain(&sc->tick_ch);
	bus_teardown_intr(dev, sc->irq_res, sc->intrhand);
	taskqueue_drain(sc->sc_tq, &sc->sc_intr_task);
	taskqueue_free(sc->sc_tq);
	macb_deactivate(dev);
	bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irq_res);
	bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->mem_res);
	MACB_LOCK_DESTROY(sc);

	return (0);
}

/*PHY related functions*/
static inline int
phy_read(struct macb_softc *sc, int phy, int reg)
{
	int val;

	write_4(sc, EMAC_MAN, EMAC_MAN_REG_RD(phy, reg));
	while ((read_4(sc, EMAC_SR) & EMAC_SR_IDLE) == 0)
		continue;
	val = read_4(sc, EMAC_MAN) & EMAC_MAN_VALUE_MASK;

	return (val);
}

static inline int
phy_write(struct macb_softc *sc, int phy, int reg, int data)
{

	write_4(sc, EMAC_MAN, EMAC_MAN_REG_WR(phy, reg, data));
	while ((read_4(sc, EMAC_SR) & EMAC_SR_IDLE) == 0)
		continue;

	return (0);
}

/*
 * MII bus support routines.
 */
static int
macb_miibus_readreg(device_t dev, int phy, int reg)
{
	struct macb_softc *sc;
	sc = device_get_softc(dev);
	return (phy_read(sc, phy, reg));
}

static int
macb_miibus_writereg(device_t dev, int phy, int reg, int data)
{
	struct macb_softc *sc;
	sc = device_get_softc(dev);
	return (phy_write(sc, phy, reg, data));
}

static void
macb_child_detached(device_t dev, device_t child)
{
	struct macb_softc *sc;
	sc = device_get_softc(dev);

}

static void
macb_miibus_statchg(device_t dev)
{
	struct macb_softc *sc;
	struct mii_data *mii;
	int config;

	sc = device_get_softc(dev);

	mii = device_get_softc(sc->miibus);

	sc->flags &= ~MACB_FLAG_LINK;

	config = read_4(sc, EMAC_NCFGR);

	if ((mii->mii_media_status & IFM_AVALID) != 0) {
		switch (IFM_SUBTYPE(mii->mii_media_active)) {
		case IFM_10_T:
			config &= ~(CFG_SPD);
			sc->flags |= MACB_FLAG_LINK;
			break;
		case IFM_100_TX:
			config |= CFG_SPD;
			sc->flags |= MACB_FLAG_LINK;
			break;
		default:
			break;
		}
	}

	config |= CFG_FD;
	write_4(sc, EMAC_NCFGR, config);
}

static device_method_t macb_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,	macb_probe),
	DEVMETHOD(device_attach,	macb_attach),
	DEVMETHOD(device_detach,	macb_detach),

	/* Bus interface */
	DEVMETHOD(bus_child_detached,	macb_child_detached),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	macb_miibus_readreg),
	DEVMETHOD(miibus_writereg,	macb_miibus_writereg),
	DEVMETHOD(miibus_statchg,	macb_miibus_statchg),
	{ 0, 0 }
};

static driver_t macb_driver = {
	"macb",
	macb_methods,
	sizeof(struct macb_softc),
};


DRIVER_MODULE(macb, atmelarm, macb_driver, macb_devclass, 0, 0);
DRIVER_MODULE(miibus, macb, miibus_driver, miibus_devclass, 0, 0);
MODULE_DEPEND(macb, miibus, 1, 1, 1);
MODULE_DEPEND(macb, ether, 1, 1, 1);
