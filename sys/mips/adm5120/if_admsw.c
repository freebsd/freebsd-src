/* $NetBSD: if_admsw.c,v 1.3 2007/04/22 19:26:25 dyoung Exp $ */

/*-
 * Copyright (c) 2007 Ruslan Ermilov and Vsevolod Lobko.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
 * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */
/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Device driver for Alchemy Semiconductor Au1x00 Ethernet Media
 * Access Controller.
 *
 * TODO:
 *
 *	Better Rx buffer management; we want to get new Rx buffers
 *	to the chip more quickly than we currently do.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <machine/bus.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_mib.h>
#include <net/if_types.h>
#include <net/if_var.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

#include <net/bpf.h>
#include <net/bpfdesc.h>

#include <mips/adm5120/adm5120reg.h>
#include <mips/adm5120/if_admswreg.h>
#include <mips/adm5120/if_admswvar.h>

/* TODO: add locking */
#define ADMSW_LOCK(sc) do {} while(0);
#define ADMSW_UNLOCK(sc) do {} while(0);

static uint8_t vlan_matrix[SW_DEVS] = {
	(1 << 6) | (1 << 0),		/* CPU + port0 */
	(1 << 6) | (1 << 1),		/* CPU + port1 */
	(1 << 6) | (1 << 2),		/* CPU + port2 */
	(1 << 6) | (1 << 3),		/* CPU + port3 */
	(1 << 6) | (1 << 4),		/* CPU + port4 */
	(1 << 6) | (1 << 5),		/* CPU + port5 */
};

/* ifnet entry points */
static void	admsw_start(struct ifnet *);
static void	admsw_watchdog(void *);
static int	admsw_ioctl(struct ifnet *, u_long, caddr_t);
static void	admsw_init(void *);
static void	admsw_stop(struct ifnet *, int);

static void	admsw_reset(struct admsw_softc *);
static void	admsw_set_filter(struct admsw_softc *);

static void	admsw_txintr(struct admsw_softc *, int);
static void	admsw_rxintr(struct admsw_softc *, int);
static int	admsw_add_rxbuf(struct admsw_softc *, int, int);
#define	admsw_add_rxhbuf(sc, idx)	admsw_add_rxbuf(sc, idx, 1)
#define	admsw_add_rxlbuf(sc, idx)	admsw_add_rxbuf(sc, idx, 0)

static int	admsw_mediachange(struct ifnet *);
static void	admsw_mediastatus(struct ifnet *, struct ifmediareq *);

static int	admsw_intr(void *);

/* bus entry points */
static int	admsw_probe(device_t dev);
static int	admsw_attach(device_t dev);
static int	admsw_detach(device_t dev);
static int	admsw_shutdown(device_t dev);

static void
admsw_dma_map_addr(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	uint32_t *addr;

	if (error)
		return;

	KASSERT(nseg == 1, ("too many DMA segments, %d should be 1", nseg));
	addr = arg;
	*addr = segs->ds_addr;
}

static void
admsw_rxbuf_map_addr(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct admsw_descsoft *ds;

	if (error)
		return;

	KASSERT(nseg == 1, ("too many DMA segments, %d should be 1", nseg));

	ds = arg;
	ds->ds_nsegs = nseg;
	ds->ds_addr[0] = segs[0].ds_addr;
	ds->ds_len[0] = segs[0].ds_len;

}

static void
admsw_mbuf_map_addr(void *arg, bus_dma_segment_t *segs, int nseg, 
    bus_size_t mapsize, int error)
{
	struct admsw_descsoft *ds;

	if (error)
		return;

	ds = arg;

	if((nseg != 1) && (nseg != 2))
		panic("%s: nseg == %d\n", __func__, nseg);

	ds->ds_nsegs = nseg;
	ds->ds_addr[0] = segs[0].ds_addr;
	ds->ds_len[0] = segs[0].ds_len;

	if(nseg > 1) {
		ds->ds_addr[1] = segs[1].ds_addr;
		ds->ds_len[1] = segs[1].ds_len;
	}
}



static int
admsw_probe(device_t dev)
{

	device_set_desc(dev, "ADM5120 Switch Engine");
	return (0);
}

#define	REG_READ(o)	bus_read_4((sc)->mem_res, (o))
#define	REG_WRITE(o,v)	bus_write_4((sc)->mem_res, (o),(v))

static void
admsw_init_bufs(struct admsw_softc *sc)
{
	int i;
	struct admsw_desc *desc;

	for (i = 0; i < ADMSW_NTXHDESC; i++) {
		if (sc->sc_txhsoft[i].ds_mbuf != NULL) {
			m_freem(sc->sc_txhsoft[i].ds_mbuf);
			sc->sc_txhsoft[i].ds_mbuf = NULL;
		}
		desc = &sc->sc_txhdescs[i];
		desc->data = 0;
		desc->cntl = 0;
		desc->len = MAC_BUFLEN;
		desc->status = 0;
		ADMSW_CDTXHSYNC(sc, i,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	}
	sc->sc_txhdescs[ADMSW_NTXHDESC - 1].data |= ADM5120_DMA_RINGEND;
	ADMSW_CDTXHSYNC(sc, ADMSW_NTXHDESC - 1,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	for (i = 0; i < ADMSW_NRXHDESC; i++) {
		if (sc->sc_rxhsoft[i].ds_mbuf == NULL) {
			if (admsw_add_rxhbuf(sc, i) != 0)
				panic("admsw_init_bufs\n");
		} else
			ADMSW_INIT_RXHDESC(sc, i);
	}

	for (i = 0; i < ADMSW_NTXLDESC; i++) {
		if (sc->sc_txlsoft[i].ds_mbuf != NULL) {
			m_freem(sc->sc_txlsoft[i].ds_mbuf);
			sc->sc_txlsoft[i].ds_mbuf = NULL;
		}
		desc = &sc->sc_txldescs[i];
		desc->data = 0;
		desc->cntl = 0;
		desc->len = MAC_BUFLEN;
		desc->status = 0;
		ADMSW_CDTXLSYNC(sc, i,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	}
	sc->sc_txldescs[ADMSW_NTXLDESC - 1].data |= ADM5120_DMA_RINGEND;
	ADMSW_CDTXLSYNC(sc, ADMSW_NTXLDESC - 1,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	for (i = 0; i < ADMSW_NRXLDESC; i++) {
		if (sc->sc_rxlsoft[i].ds_mbuf == NULL) {
			if (admsw_add_rxlbuf(sc, i) != 0)
				panic("admsw_init_bufs\n");
		} else
			ADMSW_INIT_RXLDESC(sc, i);
	}

	REG_WRITE(SEND_HBADDR_REG, ADMSW_CDTXHADDR(sc, 0));
	REG_WRITE(SEND_LBADDR_REG, ADMSW_CDTXLADDR(sc, 0));
	REG_WRITE(RECV_HBADDR_REG, ADMSW_CDRXHADDR(sc, 0));
	REG_WRITE(RECV_LBADDR_REG, ADMSW_CDRXLADDR(sc, 0));

	sc->sc_txfree = ADMSW_NTXLDESC;
	sc->sc_txnext = 0;
	sc->sc_txdirty = 0;
	sc->sc_rxptr = 0;
}

static void
admsw_setvlan(struct admsw_softc *sc, char matrix[6])
{
	uint32_t i;

	i = matrix[0] + (matrix[1] << 8) + (matrix[2] << 16) + (matrix[3] << 24);
	REG_WRITE(VLAN_G1_REG, i);
	i = matrix[4] + (matrix[5] << 8);
	REG_WRITE(VLAN_G2_REG, i);
}

static void
admsw_reset(struct admsw_softc *sc)
{
	uint32_t wdog1;
	int i;

	REG_WRITE(PORT_CONF0_REG,
	    REG_READ(PORT_CONF0_REG) | PORT_CONF0_DP_MASK);
	REG_WRITE(CPUP_CONF_REG,
	    REG_READ(CPUP_CONF_REG) | CPUP_CONF_DCPUP);

	/* Wait for DMA to complete.  Overkill.  In 3ms, we can
	 * send at least two entire 1500-byte packets at 10 Mb/s.
	 */
	DELAY(3000);

	/* The datasheet recommends that we move all PHYs to reset
	 * state prior to software reset.
	 */
	REG_WRITE(PHY_CNTL2_REG,
	    REG_READ(PHY_CNTL2_REG) & ~PHY_CNTL2_PHYR_MASK);

	/* Reset the switch. */
	REG_WRITE(ADMSW_SW_RES, 0x1);

	DELAY(100 * 1000);

	REG_WRITE(ADMSW_BOOT_DONE, ADMSW_BOOT_DONE_BO);

	/* begin old code */
	REG_WRITE(CPUP_CONF_REG,
	    CPUP_CONF_DCPUP | CPUP_CONF_CRCP | CPUP_CONF_DUNP_MASK |
	    CPUP_CONF_DMCP_MASK);

	REG_WRITE(PORT_CONF0_REG, PORT_CONF0_EMCP_MASK | PORT_CONF0_EMBP_MASK);

	REG_WRITE(PHY_CNTL2_REG,
	    REG_READ(PHY_CNTL2_REG) | PHY_CNTL2_ANE_MASK | PHY_CNTL2_PHYR_MASK |
	    PHY_CNTL2_AMDIX_MASK);

	REG_WRITE(PHY_CNTL3_REG, REG_READ(PHY_CNTL3_REG) | PHY_CNTL3_RNT);

	REG_WRITE(ADMSW_INT_MASK, INT_MASK);
	REG_WRITE(ADMSW_INT_ST, INT_MASK);

	/*
	 * While in DDB, we stop servicing interrupts, RX ring
	 * fills up and when free block counter falls behind FC
	 * threshold, the switch starts to emit 802.3x PAUSE
	 * frames.  This can upset peer switches.
	 *
	 * Stop this from happening by disabling FC and D2
	 * thresholds.
	 */
	REG_WRITE(FC_TH_REG,
	    REG_READ(FC_TH_REG) & ~(FC_TH_FCS_MASK | FC_TH_D2S_MASK));

	admsw_setvlan(sc, vlan_matrix);

	for (i = 0; i < SW_DEVS; i++) {
		REG_WRITE(MAC_WT1_REG,
		    sc->sc_enaddr[2] |
		    (sc->sc_enaddr[3]<<8) |
		    (sc->sc_enaddr[4]<<16) |
		    ((sc->sc_enaddr[5]+i)<<24));
		REG_WRITE(MAC_WT0_REG, (i<<MAC_WT0_VLANID_SHIFT) |
		    (sc->sc_enaddr[0]<<16) | (sc->sc_enaddr[1]<<24) |
		    MAC_WT0_WRITE | MAC_WT0_VLANID_EN);

		while (!(REG_READ(MAC_WT0_REG) & MAC_WT0_WRITE_DONE));
	}

	wdog1 = REG_READ(ADM5120_WDOG1);
	REG_WRITE(ADM5120_WDOG1, wdog1 & ~ADM5120_WDOG1_WDE);
}

static int
admsw_attach(device_t dev)
{
	uint8_t enaddr[ETHER_ADDR_LEN];
	struct admsw_softc *sc = (struct admsw_softc *) device_get_softc(dev);
	struct ifnet *ifp;
	int error, i, rid;

	sc->sc_dev = dev;
	device_printf(dev, "ADM5120 Switch Engine, %d ports\n", SW_DEVS);
	sc->ndevs = 0;

	/* XXXMIPS: fix it */
	enaddr[0] = 0x00;
	enaddr[1] = 0x0C;
	enaddr[2] = 0x42;
	enaddr[3] = 0x07;
	enaddr[4] = 0xB2;
	enaddr[5] = 0x4E;

	memcpy(sc->sc_enaddr, enaddr, sizeof(sc->sc_enaddr));

	device_printf(sc->sc_dev, "base Ethernet address %s\n",
	    ether_sprintf(enaddr));
	callout_init(&sc->sc_watchdog, 1);

	rid = 0;
	if ((sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, 
	    RF_ACTIVE)) == NULL) {
                device_printf(dev, "unable to allocate memory resource\n");
                return (ENXIO);
        }

	/* Hook up the interrupt handler. */
	rid = 0;
	if ((sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, 
	    RF_SHAREABLE | RF_ACTIVE)) == NULL) {
                device_printf(dev, "unable to allocate IRQ resource\n");
                return (ENXIO);
        }

	if ((error = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_NET, 
	    admsw_intr, NULL, sc, &sc->sc_ih)) != 0) {
                device_printf(dev, 
                    "WARNING: unable to register interrupt handler\n");
                return (error);
        }

	/*
	 * Allocate the control data structures, and create and load the
	 * DMA map for it.
	 */
	if ((error = bus_dma_tag_create(NULL, 4, 0, 
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR,
	    NULL, NULL, sizeof(struct admsw_control_data), 1,
	    sizeof(struct admsw_control_data), 0, NULL, NULL, 
	    &sc->sc_control_dmat)) != 0) {
		device_printf(sc->sc_dev, 
		    "unable to create control data DMA map, error = %d\n", 
		    error);
		return (error);
	}

	if ((error = bus_dmamem_alloc(sc->sc_control_dmat,
	    (void **)&sc->sc_control_data, BUS_DMA_NOWAIT, 
	    &sc->sc_cddmamap)) != 0) {
		device_printf(sc->sc_dev, 
		    "unable to allocate control data, error = %d\n", error);
		return (error);
	}

	if ((error = bus_dmamap_load(sc->sc_control_dmat, sc->sc_cddmamap,
	    sc->sc_control_data, sizeof(struct admsw_control_data), 
	    admsw_dma_map_addr, &sc->sc_cddma, 0)) != 0) {
		device_printf(sc->sc_dev, 
		    "unable to load control data DMA map, error = %d\n", error);
		return (error);
	}

	/*
	 * Create the transmit buffer DMA maps.
	 */
	if ((error = bus_dma_tag_create(NULL, 1, 0, 
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR,
	    NULL, NULL, MCLBYTES, 1, MCLBYTES, 0, NULL, NULL, 
	    &sc->sc_bufs_dmat)) != 0) {
		device_printf(sc->sc_dev, 
		    "unable to create control data DMA map, error = %d\n", 
		    error);
		return (error);
	}

	for (i = 0; i < ADMSW_NTXHDESC; i++) {
		if ((error = bus_dmamap_create(sc->sc_bufs_dmat, 0,
		    &sc->sc_txhsoft[i].ds_dmamap)) != 0) {
			device_printf(sc->sc_dev, 
			    "unable to create txh DMA map %d, error = %d\n", 
			    i, error);
			return (error);
		}
		sc->sc_txhsoft[i].ds_mbuf = NULL;
	}

	for (i = 0; i < ADMSW_NTXLDESC; i++) {
		if ((error = bus_dmamap_create(sc->sc_bufs_dmat, 0,
		    &sc->sc_txlsoft[i].ds_dmamap)) != 0) {
			device_printf(sc->sc_dev, 
			    "unable to create txl DMA map %d, error = %d\n", 
			    i, error);
			return (error);
		}
		sc->sc_txlsoft[i].ds_mbuf = NULL;
	}

	/*
	 * Create the receive buffer DMA maps.
	 */
	for (i = 0; i < ADMSW_NRXHDESC; i++) {
		if ((error = bus_dmamap_create(sc->sc_bufs_dmat, 0, 
		     &sc->sc_rxhsoft[i].ds_dmamap)) != 0) {
			device_printf(sc->sc_dev, 
			    "unable to create rxh DMA map %d, error = %d\n", 
			    i, error);
			return (error);
		}
		sc->sc_rxhsoft[i].ds_mbuf = NULL;
	}

	for (i = 0; i < ADMSW_NRXLDESC; i++) {
		if ((error = bus_dmamap_create(sc->sc_bufs_dmat, 0,
		    &sc->sc_rxlsoft[i].ds_dmamap)) != 0) {
			device_printf(sc->sc_dev, 
			    "unable to create rxl DMA map %d, error = %d\n",
			    i, error);
			return (error);
		}
		sc->sc_rxlsoft[i].ds_mbuf = NULL;
	}

	admsw_init_bufs(sc);
	admsw_reset(sc);

	for (i = 0; i < SW_DEVS; i++) {
		ifmedia_init(&sc->sc_ifmedia[i], 0, admsw_mediachange, 
		    admsw_mediastatus);
		ifmedia_add(&sc->sc_ifmedia[i], IFM_ETHER|IFM_10_T, 0, NULL);
		ifmedia_add(&sc->sc_ifmedia[i], 
		    IFM_ETHER|IFM_10_T|IFM_FDX, 0, NULL);
		ifmedia_add(&sc->sc_ifmedia[i], IFM_ETHER|IFM_100_TX, 0, NULL);
		ifmedia_add(&sc->sc_ifmedia[i], 
		    IFM_ETHER|IFM_100_TX|IFM_FDX, 0, NULL);
		ifmedia_add(&sc->sc_ifmedia[i], IFM_ETHER|IFM_AUTO, 0, NULL);
		ifmedia_set(&sc->sc_ifmedia[i], IFM_ETHER|IFM_AUTO);

		ifp = sc->sc_ifnet[i] = if_alloc(IFT_ETHER);

		/* Setup interface parameters */
		ifp->if_softc = sc;
		if_initname(ifp, device_get_name(dev), i);
		ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
		ifp->if_ioctl = admsw_ioctl;
		ifp->if_output = ether_output;
		ifp->if_start = admsw_start;
		ifp->if_init = admsw_init;
		ifp->if_mtu = ETHERMTU;
		ifp->if_baudrate = IF_Mbps(100);
		IFQ_SET_MAXLEN(&ifp->if_snd, max(ADMSW_NTXLDESC, ifqmaxlen));
		ifp->if_snd.ifq_drv_maxlen = max(ADMSW_NTXLDESC, ifqmaxlen);
		IFQ_SET_READY(&ifp->if_snd);
		ifp->if_capabilities |= IFCAP_VLAN_MTU;

		/* Attach the interface. */
		ether_ifattach(ifp, enaddr);
		enaddr[5]++;
	}

	/* XXX: admwdog_attach(sc); */

	/* leave interrupts and cpu port disabled */
	return (0);
}

static int
admsw_detach(device_t dev)
{

	printf("TODO: DETACH\n");
	return (0);
}

/*
 * admsw_shutdown:
 *
 *	Make sure the interface is stopped at reboot time.
 */
static int
admsw_shutdown(device_t dev)
{
	struct admsw_softc *sc;
	int i;

	sc = device_get_softc(dev);
	for (i = 0; i < SW_DEVS; i++)
		admsw_stop(sc->sc_ifnet[i], 1);

	return (0);
}

/*
 * admsw_start:		[ifnet interface function]
 *
 *	Start packet transmission on the interface.
 */
static void
admsw_start(struct ifnet *ifp)
{
	struct admsw_softc *sc = ifp->if_softc;
	struct mbuf *m0, *m;
	struct admsw_descsoft *ds;
	struct admsw_desc *desc;
	bus_dmamap_t dmamap;
	struct ether_header *eh;
	int error, nexttx, len, i;
	static int vlan = 0;

	/*
	 * Loop through the send queues, setting up transmit descriptors
	 * unitl we drain the queues, or use up all available transmit
	 * descriptors.
	 */
	for (;;) {
		vlan++;
		if (vlan == SW_DEVS)
			vlan = 0;
		i = vlan;
		for (;;) {
			ifp = sc->sc_ifnet[i];
			if ((ifp->if_drv_flags & (IFF_DRV_RUNNING|IFF_DRV_OACTIVE)) 
			    == IFF_DRV_RUNNING) {
				/* Grab a packet off the queue. */
				IF_DEQUEUE(&ifp->if_snd, m0);
				if (m0 != NULL)
					break;
			}
			i++;
			if (i == SW_DEVS)
				i = 0;
			if (i == vlan)
				return;
		}
		vlan = i;
		m = NULL;

		/* Get a spare descriptor. */
		if (sc->sc_txfree == 0) {
			/* No more slots left; notify upper layer. */
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			break;
		}
		nexttx = sc->sc_txnext;
		desc = &sc->sc_txldescs[nexttx];
		ds = &sc->sc_txlsoft[nexttx];
		dmamap = ds->ds_dmamap;

		/*
		 * Load the DMA map.  If this fails, the packet either
		 * didn't fit in the alloted number of segments, or we
		 * were short on resources.  In this case, we'll copy
		 * and try again.
		 */
		if (m0->m_pkthdr.len < ETHER_MIN_LEN ||
		    bus_dmamap_load_mbuf(sc->sc_bufs_dmat, dmamap, m0,
		    admsw_mbuf_map_addr, ds, BUS_DMA_NOWAIT) != 0) {
			MGETHDR(m, M_NOWAIT, MT_DATA);
			if (m == NULL) {
				device_printf(sc->sc_dev, 
				    "unable to allocate Tx mbuf\n");
				break;
			}
			if (m0->m_pkthdr.len > MHLEN) {
				if (!(MCLGET(m, M_NOWAIT))) {
					device_printf(sc->sc_dev, 
					    "unable to allocate Tx cluster\n");
					m_freem(m);
					break;
				}
			}
			m->m_pkthdr.csum_flags = m0->m_pkthdr.csum_flags;
			m_copydata(m0, 0, m0->m_pkthdr.len, mtod(m, void *));
			m->m_pkthdr.len = m->m_len = m0->m_pkthdr.len;
			if (m->m_pkthdr.len < ETHER_MIN_LEN) {
				if (M_TRAILINGSPACE(m) < ETHER_MIN_LEN - m->m_pkthdr.len)
					panic("admsw_start: M_TRAILINGSPACE\n");
				memset(mtod(m, uint8_t *) + m->m_pkthdr.len, 0,
				    ETHER_MIN_LEN - ETHER_CRC_LEN - m->m_pkthdr.len);
				m->m_pkthdr.len = m->m_len = ETHER_MIN_LEN;
			}
			error = bus_dmamap_load_mbuf(sc->sc_bufs_dmat, 
			    dmamap, m, admsw_mbuf_map_addr, ds, BUS_DMA_NOWAIT);
			if (error) {
				device_printf(sc->sc_dev, 
				    "unable to load Tx buffer, error = %d\n", 
				    error);
				break;
			}
		}

		if (m != NULL) {
			m_freem(m0);
			m0 = m;
		}

		/*
		 * WE ARE NOW COMMITTED TO TRANSMITTING THE PACKET.
		 */

		/* Sync the DMA map. */
		bus_dmamap_sync(sc->sc_bufs_dmat, dmamap, BUS_DMASYNC_PREWRITE);

		if (ds->ds_nsegs != 1 && ds->ds_nsegs != 2)
			panic("admsw_start: nsegs == %d\n", ds->ds_nsegs);
		desc->data = ds->ds_addr[0];
		desc->len = len = ds->ds_len[0];
		if (ds->ds_nsegs > 1) {
			len += ds->ds_len[1];
			desc->cntl = ds->ds_addr[1] | ADM5120_DMA_BUF2ENABLE;
		} else
			desc->cntl = 0;
		desc->status = (len << ADM5120_DMA_LENSHIFT) | (1 << vlan);
		eh = mtod(m0, struct ether_header *);
		if (ntohs(eh->ether_type) == ETHERTYPE_IP &&
		    m0->m_pkthdr.csum_flags & CSUM_IP)
			desc->status |= ADM5120_DMA_CSUM;
		if (nexttx == ADMSW_NTXLDESC - 1)
			desc->data |= ADM5120_DMA_RINGEND;
		desc->data |= ADM5120_DMA_OWN;

		/* Sync the descriptor. */
		ADMSW_CDTXLSYNC(sc, nexttx,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		REG_WRITE(SEND_TRIG_REG, 1);
		/* printf("send slot %d\n",nexttx); */

		/*
		 * Store a pointer to the packet so we can free it later.
		 */
		ds->ds_mbuf = m0;

		/* Advance the Tx pointer. */
		sc->sc_txfree--;
		sc->sc_txnext = ADMSW_NEXTTXL(nexttx);

		/* Pass the packet to any BPF listeners. */
		BPF_MTAP(ifp, m0);

		/* Set a watchdog timer in case the chip flakes out. */
		sc->sc_timer = 5;
	}
}

/*
 * admsw_watchdog:	[ifnet interface function]
 *
 *	Watchdog timer handler.
 */
static void
admsw_watchdog(void *arg)
{
	struct admsw_softc *sc = arg;
	struct ifnet *ifp;
	int vlan;

	callout_reset(&sc->sc_watchdog, hz, admsw_watchdog, sc);
	if (sc->sc_timer == 0 || --sc->sc_timer > 0)
		return;

	/* Check if an interrupt was lost. */
	if (sc->sc_txfree == ADMSW_NTXLDESC) {
		device_printf(sc->sc_dev, "watchdog false alarm\n");
		return;
	}
	if (sc->sc_timer != 0)
		device_printf(sc->sc_dev, "watchdog timer is %d!\n",  
		    sc->sc_timer);
	admsw_txintr(sc, 0);
	if (sc->sc_txfree == ADMSW_NTXLDESC) {
		device_printf(sc->sc_dev, "tx IRQ lost (queue empty)\n");
		return;
	}
	if (sc->sc_timer != 0) {
		device_printf(sc->sc_dev, "tx IRQ lost (timer recharged)\n");
		return;
	}

	device_printf(sc->sc_dev, "device timeout, txfree = %d\n",  
	    sc->sc_txfree);
	for (vlan = 0; vlan < SW_DEVS; vlan++)
		admsw_stop(sc->sc_ifnet[vlan], 0);
	admsw_init(sc);

	ifp = sc->sc_ifnet[0];

	/* Try to get more packets going. */
	admsw_start(ifp);
}

/*
 * admsw_ioctl:		[ifnet interface function]
 *
 *	Handle control requests from the operator.
 */
static int
admsw_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct admsw_softc *sc = ifp->if_softc;
	struct ifdrv *ifd;
	int error, port;

	ADMSW_LOCK(sc);

	switch (cmd) {
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		port = 0;
		while(port < SW_DEVS)
			if(ifp == sc->sc_ifnet[port])
				 break;
			else 
				port++;
		if (port >= SW_DEVS)
			error = EOPNOTSUPP;
		else
			error = ifmedia_ioctl(ifp, (struct ifreq *)data,
			    &sc->sc_ifmedia[port], cmd);
		break;

	case SIOCGDRVSPEC:
	case SIOCSDRVSPEC:
		ifd = (struct ifdrv *) data;
		if (ifd->ifd_cmd != 0 || ifd->ifd_len != sizeof(vlan_matrix)) {
			error = EINVAL;
			break;
		}
		if (cmd == SIOCGDRVSPEC) {
			error = copyout(vlan_matrix, ifd->ifd_data,
			    sizeof(vlan_matrix));
		} else {
			error = copyin(ifd->ifd_data, vlan_matrix,
			    sizeof(vlan_matrix));
			admsw_setvlan(sc, vlan_matrix);
		}
		break;

	default:
		error = ether_ioctl(ifp, cmd, data);
		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			admsw_set_filter(sc);
			error = 0;
		}
		break;
	}

	/* Try to get more packets going. */
	admsw_start(ifp);

	ADMSW_UNLOCK(sc);
	return (error);
}


/*
 * admsw_intr:
 *
 *	Interrupt service routine.
 */
static int
admsw_intr(void *arg)
{
	struct admsw_softc *sc = arg;
	uint32_t pending;

	pending = REG_READ(ADMSW_INT_ST);
	REG_WRITE(ADMSW_INT_ST, pending);

	if (sc->ndevs == 0)
		return (FILTER_STRAY);

	if ((pending & ADMSW_INTR_RHD) != 0)
		admsw_rxintr(sc, 1);

	if ((pending & ADMSW_INTR_RLD) != 0)
		admsw_rxintr(sc, 0);

	if ((pending & ADMSW_INTR_SHD) != 0)
		admsw_txintr(sc, 1);

	if ((pending & ADMSW_INTR_SLD) != 0)
		admsw_txintr(sc, 0);

	return (FILTER_HANDLED);
}

/*
 * admsw_txintr:
 *
 *	Helper; handle transmit interrupts.
 */
static void
admsw_txintr(struct admsw_softc *sc, int prio)
{
	struct ifnet *ifp;
	struct admsw_desc *desc;
	struct admsw_descsoft *ds;
	int i, vlan;
	int gotone = 0;

	/* printf("txintr: txdirty: %d, txfree: %d\n",sc->sc_txdirty, sc->sc_txfree); */
	for (i = sc->sc_txdirty; sc->sc_txfree != ADMSW_NTXLDESC;
	    i = ADMSW_NEXTTXL(i)) {

		ADMSW_CDTXLSYNC(sc, i,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		desc = &sc->sc_txldescs[i];
		ds = &sc->sc_txlsoft[i];
		if (desc->data & ADM5120_DMA_OWN) {
			ADMSW_CDTXLSYNC(sc, i,
			    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
			break;
		}

		bus_dmamap_sync(sc->sc_bufs_dmat, ds->ds_dmamap, 
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_bufs_dmat, ds->ds_dmamap);
		m_freem(ds->ds_mbuf);
		ds->ds_mbuf = NULL;

		vlan = ffs(desc->status & 0x3f) - 1;
		if (vlan < 0 || vlan >= SW_DEVS)
			panic("admsw_txintr: bad vlan\n");
		ifp = sc->sc_ifnet[vlan];
		gotone = 1;
		/* printf("clear tx slot %d\n",i); */

		if_inc_counter(ifp, IFCOUNTER_OPACKETS, 1);

		sc->sc_txfree++;
	}

	if (gotone) {
		sc->sc_txdirty = i;
		for (vlan = 0; vlan < SW_DEVS; vlan++)
			sc->sc_ifnet[vlan]->if_drv_flags &= ~IFF_DRV_OACTIVE;

		ifp = sc->sc_ifnet[0];

		/* Try to queue more packets. */
		admsw_start(ifp);

		/*
		 * If there are no more pending transmissions,
		 * cancel the watchdog timer.
		 */
		if (sc->sc_txfree == ADMSW_NTXLDESC)
			sc->sc_timer = 0;

	}

	/* printf("txintr end: txdirty: %d, txfree: %d\n",sc->sc_txdirty, sc->sc_txfree); */
}

/*
 * admsw_rxintr:
 *
 *	Helper; handle receive interrupts.
 */
static void
admsw_rxintr(struct admsw_softc *sc, int high)
{
	struct ifnet *ifp;
	struct admsw_descsoft *ds;
	struct mbuf *m;
	uint32_t stat;
	int i, len, port, vlan;

	/* printf("rxintr\n"); */

	if (high)
		panic("admsw_rxintr: high priority packet\n");

#if 1
	ADMSW_CDRXLSYNC(sc, sc->sc_rxptr, 
	    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
	if ((sc->sc_rxldescs[sc->sc_rxptr].data & ADM5120_DMA_OWN) == 0)
		ADMSW_CDRXLSYNC(sc, sc->sc_rxptr, 
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	else {
		i = sc->sc_rxptr;
		do {
			ADMSW_CDRXLSYNC(sc, i, 
			    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
			i = ADMSW_NEXTRXL(i);
			/* the ring is empty, just return. */
			if (i == sc->sc_rxptr)
				return;
			ADMSW_CDRXLSYNC(sc, i, 
			    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
		} while (sc->sc_rxldescs[i].data & ADM5120_DMA_OWN);

		ADMSW_CDRXLSYNC(sc, i, 
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		ADMSW_CDRXLSYNC(sc, sc->sc_rxptr, 
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		if ((sc->sc_rxldescs[sc->sc_rxptr].data & ADM5120_DMA_OWN) == 0)
			ADMSW_CDRXLSYNC(sc, sc->sc_rxptr, 
			    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
		else {
			ADMSW_CDRXLSYNC(sc, sc->sc_rxptr, 
			    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
			/* We've fallen behind the chip: catch it. */
#if 0
			device_printf(sc->sc_dev, 
			   "RX ring resync, base=%x, work=%x, %d -> %d\n",
			    REG_READ(RECV_LBADDR_REG),
			    REG_READ(RECV_LWADDR_REG), sc->sc_rxptr, i);
#endif
			sc->sc_rxptr = i;
			/* ADMSW_EVCNT_INCR(&sc->sc_ev_rxsync); */
		}
	}
#endif
	for (i = sc->sc_rxptr;; i = ADMSW_NEXTRXL(i)) {
		ds = &sc->sc_rxlsoft[i];

		ADMSW_CDRXLSYNC(sc, i, 
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		if (sc->sc_rxldescs[i].data & ADM5120_DMA_OWN) {
			ADMSW_CDRXLSYNC(sc, i, 
			    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
			break;
		}

		/* printf("process slot %d\n",i); */

		bus_dmamap_sync(sc->sc_bufs_dmat, ds->ds_dmamap,
		    BUS_DMASYNC_POSTREAD);

		stat = sc->sc_rxldescs[i].status;
		len = (stat & ADM5120_DMA_LEN) >> ADM5120_DMA_LENSHIFT;
		len -= ETHER_CRC_LEN;
		port = (stat & ADM5120_DMA_PORTID) >> ADM5120_DMA_PORTSHIFT;

		for (vlan = 0; vlan < SW_DEVS; vlan++)
			if ((1 << port) & vlan_matrix[vlan])
				break;

		if (vlan == SW_DEVS)
			vlan = 0;

		ifp = sc->sc_ifnet[vlan];

		m = ds->ds_mbuf;
		if (admsw_add_rxlbuf(sc, i) != 0) {
			if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);
			ADMSW_INIT_RXLDESC(sc, i);
			bus_dmamap_sync(sc->sc_bufs_dmat, ds->ds_dmamap,
			    BUS_DMASYNC_PREREAD);
			continue;
		}

		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = len;
		if ((stat & ADM5120_DMA_TYPE) == ADM5120_DMA_TYPE_IP) {
			m->m_pkthdr.csum_flags |= CSUM_IP_CHECKED;
			if (!(stat & ADM5120_DMA_CSUMFAIL))
				m->m_pkthdr.csum_flags |= CSUM_IP_VALID;
		}

		BPF_MTAP(ifp, m);

		/* Pass it on. */
		(*ifp->if_input)(ifp, m);
		if_inc_counter(ifp, IFCOUNTER_IPACKETS, 1);
	}

	/* Update the receive pointer. */
	sc->sc_rxptr = i;
}

/*
 * admsw_init:		[ifnet interface function]
 *
 *	Initialize the interface.
 */
static void
admsw_init(void *xsc)
{
	struct admsw_softc *sc = xsc;
	struct ifnet *ifp;
	int i;

	for (i = 0; i < SW_DEVS; i++) {
		ifp = sc->sc_ifnet[i];
		if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
			if (sc->ndevs == 0) {
				admsw_init_bufs(sc);
				admsw_reset(sc);
				REG_WRITE(CPUP_CONF_REG,
				    CPUP_CONF_CRCP | CPUP_CONF_DUNP_MASK |
				    CPUP_CONF_DMCP_MASK);
				/* clear all pending interrupts */
				REG_WRITE(ADMSW_INT_ST, INT_MASK);

				/* enable needed interrupts */
				REG_WRITE(ADMSW_INT_MASK, 
				    REG_READ(ADMSW_INT_MASK) & 
				    ~(ADMSW_INTR_SHD | ADMSW_INTR_SLD | 
					ADMSW_INTR_RHD | ADMSW_INTR_RLD | 
					ADMSW_INTR_HDF | ADMSW_INTR_LDF));

				callout_reset(&sc->sc_watchdog, hz,
				    admsw_watchdog, sc);
			}
			sc->ndevs++;
		}


		/* mark iface as running */
		ifp->if_drv_flags |= IFF_DRV_RUNNING;
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	}

	/* Set the receive filter. */
	admsw_set_filter(sc);
}

/*
 * admsw_stop:		[ifnet interface function]
 *
 *	Stop transmission on the interface.
 */
static void
admsw_stop(struct ifnet *ifp, int disable)
{
	struct admsw_softc *sc = ifp->if_softc;

	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
		return;

	if (--sc->ndevs == 0) {
		/* printf("debug: de-initializing hardware\n"); */

		/* disable cpu port */
		REG_WRITE(CPUP_CONF_REG,
				CPUP_CONF_DCPUP | CPUP_CONF_CRCP |
				CPUP_CONF_DUNP_MASK | CPUP_CONF_DMCP_MASK);

		/* XXX We should disable, then clear? --dyoung */
		/* clear all pending interrupts */
		REG_WRITE(ADMSW_INT_ST, INT_MASK);

		/* disable interrupts */
		REG_WRITE(ADMSW_INT_MASK, INT_MASK);

		/* Cancel the watchdog timer. */
		sc->sc_timer = 0;
		callout_stop(&sc->sc_watchdog);
	}

	/* Mark the interface as down. */
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	return;
}

/*
 * admsw_set_filter:
 *
 *	Set up the receive filter.
 */
static void
admsw_set_filter(struct admsw_softc *sc)
{
	int i;
	uint32_t allmc, anymc, conf, promisc;
	struct ifnet *ifp;
	struct ifmultiaddr *ifma;

	/* Find which ports should be operated in promisc mode. */
	allmc = anymc = promisc = 0;
	for (i = 0; i < SW_DEVS; i++) {
		ifp = sc->sc_ifnet[i];
		if (ifp->if_flags & IFF_PROMISC)
			promisc |= vlan_matrix[i];

		ifp->if_flags &= ~IFF_ALLMULTI;

		if_maddr_rlock(ifp);
		TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link)
		{
			if (ifma->ifma_addr->sa_family != AF_LINK)
				continue;

			anymc |= vlan_matrix[i];
		}
		if_maddr_runlock(ifp);
	}

	conf = REG_READ(CPUP_CONF_REG);
	/* 1 Disable forwarding of unknown & multicast packets to
	 *   CPU on all ports.
	 * 2 Enable forwarding of unknown & multicast packets to
	 *   CPU on ports where IFF_PROMISC or IFF_ALLMULTI is set.
	 */
	conf |= CPUP_CONF_DUNP_MASK | CPUP_CONF_DMCP_MASK;
	/* Enable forwarding of unknown packets to CPU on selected ports. */
	conf ^= ((promisc << CPUP_CONF_DUNP_SHIFT) & CPUP_CONF_DUNP_MASK);
	conf ^= ((allmc << CPUP_CONF_DMCP_SHIFT) & CPUP_CONF_DMCP_MASK);
	conf ^= ((anymc << CPUP_CONF_DMCP_SHIFT) & CPUP_CONF_DMCP_MASK);
	REG_WRITE(CPUP_CONF_REG, conf);
}

/*
 * admsw_add_rxbuf:
 *
 *	Add a receive buffer to the indicated descriptor.
 */
int
admsw_add_rxbuf(struct admsw_softc *sc, int idx, int high)
{
	struct admsw_descsoft *ds;
	struct mbuf *m;
	int error;

	if (high)
		ds = &sc->sc_rxhsoft[idx];
	else
		ds = &sc->sc_rxlsoft[idx];

	MGETHDR(m, M_NOWAIT, MT_DATA);
	if (m == NULL)
		return (ENOBUFS);

	if (!(MCLGET(m, M_NOWAIT))) {
		m_freem(m);
		return (ENOBUFS);
	}

	if (ds->ds_mbuf != NULL)
		bus_dmamap_unload(sc->sc_bufs_dmat, ds->ds_dmamap);

	ds->ds_mbuf = m;

	error = bus_dmamap_load(sc->sc_bufs_dmat, ds->ds_dmamap,
	    m->m_ext.ext_buf, m->m_ext.ext_size, admsw_rxbuf_map_addr, 
	    ds, BUS_DMA_NOWAIT);
	if (error) {
		device_printf(sc->sc_dev, 
		    "can't load rx DMA map %d, error = %d\n", idx, error);
		panic("admsw_add_rxbuf");	/* XXX */
	}

	bus_dmamap_sync(sc->sc_bufs_dmat, ds->ds_dmamap, BUS_DMASYNC_PREREAD);

	if (high)
		ADMSW_INIT_RXHDESC(sc, idx);
	else
		ADMSW_INIT_RXLDESC(sc, idx);

	return (0);
}

int
admsw_mediachange(struct ifnet *ifp)
{
	struct admsw_softc *sc = ifp->if_softc;
	int port = 0;
	struct ifmedia *ifm;
	int old, new, val;

	while(port < SW_DEVS) {
		if(ifp == sc->sc_ifnet[port])
			break;
		else
			port++;
	}

	ifm = &sc->sc_ifmedia[port];

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return (EINVAL);

	if (IFM_SUBTYPE(ifm->ifm_media) == IFM_AUTO) {
		val = PHY_CNTL2_AUTONEG|PHY_CNTL2_100M|PHY_CNTL2_FDX;
	} else if (IFM_SUBTYPE(ifm->ifm_media) == IFM_100_TX) {
		if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX)
			val = PHY_CNTL2_100M|PHY_CNTL2_FDX;
		else
			val = PHY_CNTL2_100M;
	} else if (IFM_SUBTYPE(ifm->ifm_media) == IFM_10_T) {
		if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX)
			val = PHY_CNTL2_FDX;
		else
			val = 0;
	} else
		return (EINVAL);

	old = REG_READ(PHY_CNTL2_REG);
	new = old & ~((PHY_CNTL2_AUTONEG|PHY_CNTL2_100M|PHY_CNTL2_FDX) << port);
	new |= (val << port);

	if (new != old)
		REG_WRITE(PHY_CNTL2_REG, new);

	return (0);
}

void
admsw_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct admsw_softc *sc = ifp->if_softc;
	int port = 0;
	int status;

	while(port < SW_DEVS) {
		if(ifp == sc->sc_ifnet[port])
			break;
		else
			port++;
	}

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	status = REG_READ(PHY_ST_REG) >> port;

	if ((status & PHY_ST_LINKUP) == 0) {
		ifmr->ifm_active |= IFM_NONE;
		return;
	}

	ifmr->ifm_status |= IFM_ACTIVE;
	ifmr->ifm_active |= (status & PHY_ST_100M) ? IFM_100_TX : IFM_10_T;
	if (status & PHY_ST_FDX)
		ifmr->ifm_active |= IFM_FDX;
}

static device_method_t admsw_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		admsw_probe),
	DEVMETHOD(device_attach,	admsw_attach),
	DEVMETHOD(device_detach,	admsw_detach),
	DEVMETHOD(device_shutdown,	admsw_shutdown),

	{ 0, 0 }
};

static devclass_t admsw_devclass;

static driver_t admsw_driver = {
	"admsw",
	admsw_methods,
	sizeof(struct admsw_softc),
};

DRIVER_MODULE(admsw, obio, admsw_driver, admsw_devclass, 0, 0);
MODULE_DEPEND(admsw, ether, 1, 1, 1);
