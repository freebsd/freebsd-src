/*
 * Copyright (C) 2001 Eduardo Horvath.
 * Copyright (c) 2001-2003 Thomas Moestl
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
 *	from: NetBSD: gem.c,v 1.21 2002/06/01 23:50:58 lukem Exp
 *
 * $FreeBSD$
 */

/*
 * Driver for Sun GEM ethernet controllers.
 */

#define	GEM_DEBUG

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/callout.h>
#include <sys/endian.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <machine/bus.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <gem/if_gemreg.h>
#include <gem/if_gemvar.h>

#define TRIES	10000

static void	gem_start(struct ifnet *);
static void	gem_stop(struct ifnet *, int);
static int	gem_ioctl(struct ifnet *, u_long, caddr_t);
static void	gem_cddma_callback(void *, bus_dma_segment_t *, int, int);
static void	gem_rxdma_callback(void *, bus_dma_segment_t *, int,
    bus_size_t, int);
static void	gem_txdma_callback(void *, bus_dma_segment_t *, int,
    bus_size_t, int);
static void	gem_tick(void *);
static void	gem_watchdog(struct ifnet *);
static void	gem_init(void *);
static void	gem_init_regs(struct gem_softc *sc);
static int	gem_ringsize(int sz);
static int	gem_meminit(struct gem_softc *);
static int	gem_load_txmbuf(struct gem_softc *, struct mbuf *);
static void	gem_mifinit(struct gem_softc *);
static int	gem_bitwait(struct gem_softc *sc, bus_addr_t r,
    u_int32_t clr, u_int32_t set);
static int	gem_reset_rx(struct gem_softc *);
static int	gem_reset_tx(struct gem_softc *);
static int	gem_disable_rx(struct gem_softc *);
static int	gem_disable_tx(struct gem_softc *);
static void	gem_rxdrain(struct gem_softc *);
static int	gem_add_rxbuf(struct gem_softc *, int);
static void	gem_setladrf(struct gem_softc *);

struct mbuf	*gem_get(struct gem_softc *, int, int);
static void	gem_eint(struct gem_softc *, u_int);
static void	gem_rint(struct gem_softc *);
#if 0
static void	gem_rint_timeout(void *);
#endif
static void	gem_tint(struct gem_softc *);
#ifdef notyet
static void	gem_power(int, void *);
#endif

devclass_t gem_devclass;
DRIVER_MODULE(miibus, gem, miibus_driver, miibus_devclass, 0, 0);
MODULE_DEPEND(gem, miibus, 1, 1, 1);

#ifdef GEM_DEBUG
#include <sys/ktr.h>
#define	KTR_GEM		KTR_CT2
#endif

#define	GEM_NSEGS GEM_NTXSEGS

/*
 * gem_attach:
 *
 *	Attach a Gem interface to the system.
 */
int
gem_attach(sc)
	struct gem_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct mii_softc *child;
	int i, error;
	u_int32_t v;

	/* Make sure the chip is stopped. */
	ifp->if_softc = sc;
	gem_reset(sc);

	error = bus_dma_tag_create(NULL, 1, 0, BUS_SPACE_MAXADDR_32BIT,
	    BUS_SPACE_MAXADDR, NULL, NULL, MCLBYTES, GEM_NSEGS,
	    BUS_SPACE_MAXSIZE_32BIT, 0, &sc->sc_pdmatag);
	if (error)
		return (error);

	error = bus_dma_tag_create(sc->sc_pdmatag, 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL, MAXBSIZE,
	    1, BUS_SPACE_MAXSIZE_32BIT, BUS_DMA_ALLOCNOW,
	    &sc->sc_rdmatag);
	if (error)
		goto fail_ptag;

	error = bus_dma_tag_create(sc->sc_pdmatag, 1, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    GEM_TD_BUFSIZE, GEM_NTXSEGS, BUS_SPACE_MAXSIZE_32BIT,
	    BUS_DMA_ALLOCNOW, &sc->sc_tdmatag);
	if (error)
		goto fail_rtag;

	error = bus_dma_tag_create(sc->sc_pdmatag, PAGE_SIZE, 0,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    sizeof(struct gem_control_data), 1,
	    sizeof(struct gem_control_data), BUS_DMA_ALLOCNOW,
	    &sc->sc_cdmatag);
	if (error)
		goto fail_ttag;

	/*
	 * Allocate the control data structures, and create and load the
	 * DMA map for it.
	 */
	if ((error = bus_dmamem_alloc(sc->sc_cdmatag,
	    (void **)&sc->sc_control_data, 0, &sc->sc_cddmamap))) {
		device_printf(sc->sc_dev, "unable to allocate control data,"
		    " error = %d\n", error);
		goto fail_ctag;
	}

	sc->sc_cddma = 0;
	if ((error = bus_dmamap_load(sc->sc_cdmatag, sc->sc_cddmamap,
	    sc->sc_control_data, sizeof(struct gem_control_data),
	    gem_cddma_callback, sc, 0)) != 0 || sc->sc_cddma == 0) {
		device_printf(sc->sc_dev, "unable to load control data DMA "
		    "map, error = %d\n", error);
		goto fail_cmem;
	}

	/*
	 * Initialize the transmit job descriptors.
	 */
	STAILQ_INIT(&sc->sc_txfreeq);
	STAILQ_INIT(&sc->sc_txdirtyq);

	/*
	 * Create the transmit buffer DMA maps.
	 */
	error = ENOMEM;
	for (i = 0; i < GEM_TXQUEUELEN; i++) {
		struct gem_txsoft *txs;

		txs = &sc->sc_txsoft[i];
		txs->txs_mbuf = NULL;
		txs->txs_ndescs = 0;
		if ((error = bus_dmamap_create(sc->sc_tdmatag, 0,
		    &txs->txs_dmamap)) != 0) {
			device_printf(sc->sc_dev, "unable to create tx DMA map "
			    "%d, error = %d\n", i, error);
			goto fail_txd;
		}
		STAILQ_INSERT_TAIL(&sc->sc_txfreeq, txs, txs_q);
	}

	/*
	 * Create the receive buffer DMA maps.
	 */
	for (i = 0; i < GEM_NRXDESC; i++) {
		if ((error = bus_dmamap_create(sc->sc_rdmatag, 0,
		    &sc->sc_rxsoft[i].rxs_dmamap)) != 0) {
			device_printf(sc->sc_dev, "unable to create rx DMA map "
			    "%d, error = %d\n", i, error);
			goto fail_rxd;
		}
		sc->sc_rxsoft[i].rxs_mbuf = NULL;
	}


	gem_mifinit(sc);

	if ((error = mii_phy_probe(sc->sc_dev, &sc->sc_miibus, gem_mediachange,
	    gem_mediastatus)) != 0) {
		device_printf(sc->sc_dev, "phy probe failed: %d\n", error);
		goto fail_rxd;
	}
	sc->sc_mii = device_get_softc(sc->sc_miibus);

	/*
	 * From this point forward, the attachment cannot fail.  A failure
	 * before this point releases all resources that may have been
	 * allocated.
	 */

	/* Announce ourselves. */
	device_printf(sc->sc_dev, "Ethernet address:");
	for (i = 0; i < 6; i++)
		printf("%c%02x", i > 0 ? ':' : ' ', sc->sc_arpcom.ac_enaddr[i]);

	/* Get RX FIFO size */
	sc->sc_rxfifosize = 64 *
	    bus_space_read_4(sc->sc_bustag, sc->sc_h, GEM_RX_FIFO_SIZE);
	printf(", %uKB RX fifo", sc->sc_rxfifosize / 1024);

	/* Get TX FIFO size */
	v = bus_space_read_4(sc->sc_bustag, sc->sc_h, GEM_TX_FIFO_SIZE);
	printf(", %uKB TX fifo\n", v / 16);

	/* Initialize ifnet structure. */
	ifp->if_softc = sc;
	ifp->if_unit = device_get_unit(sc->sc_dev);
	ifp->if_name = "gem";
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_start = gem_start;
	ifp->if_ioctl = gem_ioctl;
	ifp->if_watchdog = gem_watchdog;
	ifp->if_init = gem_init;
	ifp->if_output = ether_output;
	ifp->if_snd.ifq_maxlen = GEM_TXQUEUELEN;
	/*
	 * Walk along the list of attached MII devices and
	 * establish an `MII instance' to `phy number'
	 * mapping. We'll use this mapping in media change
	 * requests to determine which phy to use to program
	 * the MIF configuration register.
	 */
	for (child = LIST_FIRST(&sc->sc_mii->mii_phys); child != NULL;
	     child = LIST_NEXT(child, mii_list)) {
		/*
		 * Note: we support just two PHYs: the built-in
		 * internal device and an external on the MII
		 * connector.
		 */
		if (child->mii_phy > 1 || child->mii_inst > 1) {
			device_printf(sc->sc_dev, "cannot accomodate "
			    "MII device %s at phy %d, instance %d\n",
			    device_get_name(child->mii_dev),
			    child->mii_phy, child->mii_inst);
			continue;
		}

		sc->sc_phys[child->mii_inst] = child->mii_phy;
	}

	/*
	 * Now select and activate the PHY we will use.
	 *
	 * The order of preference is External (MDI1),
	 * Internal (MDI0), Serial Link (no MII).
	 */
	if (sc->sc_phys[1]) {
#ifdef GEM_DEBUG
		printf("using external phy\n");
#endif
		sc->sc_mif_config |= GEM_MIF_CONFIG_PHY_SEL;
	} else {
#ifdef GEM_DEBUG
		printf("using internal phy\n");
#endif
		sc->sc_mif_config &= ~GEM_MIF_CONFIG_PHY_SEL;
	}
	bus_space_write_4(sc->sc_bustag, sc->sc_h, GEM_MIF_CONFIG,
	    sc->sc_mif_config);
	/* Attach the interface. */
	ether_ifattach(ifp, sc->sc_arpcom.ac_enaddr);

#if notyet
	/*
	 * Add a suspend hook to make sure we come back up after a
	 * resume.
	 */
	sc->sc_powerhook = powerhook_establish(gem_power, sc);
	if (sc->sc_powerhook == NULL)
		device_printf(sc->sc_dev, "WARNING: unable to establish power "
		    "hook\n");
#endif

	callout_init(&sc->sc_tick_ch, 0);
	callout_init(&sc->sc_rx_ch, 0);
	return (0);

	/*
	 * Free any resources we've allocated during the failed attach
	 * attempt.  Do this in reverse order and fall through.
	 */
fail_rxd:
	for (i = 0; i < GEM_NRXDESC; i++) {
		if (sc->sc_rxsoft[i].rxs_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_rdmatag,
			    sc->sc_rxsoft[i].rxs_dmamap);
	}
fail_txd:
	for (i = 0; i < GEM_TXQUEUELEN; i++) {
		if (sc->sc_txsoft[i].txs_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_tdmatag,
			    sc->sc_txsoft[i].txs_dmamap);
	}
	bus_dmamap_unload(sc->sc_cdmatag, sc->sc_cddmamap);
fail_cmem:
	bus_dmamem_free(sc->sc_cdmatag, sc->sc_control_data,
	    sc->sc_cddmamap);
fail_ctag:
	bus_dma_tag_destroy(sc->sc_cdmatag);
fail_ttag:
	bus_dma_tag_destroy(sc->sc_tdmatag);
fail_rtag:
	bus_dma_tag_destroy(sc->sc_rdmatag);
fail_ptag:
	bus_dma_tag_destroy(sc->sc_pdmatag);
	return (error);
}

void
gem_detach(sc)
	struct gem_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int i;

	ether_ifdetach(ifp);
	gem_stop(ifp, 1);
	device_delete_child(sc->sc_dev, sc->sc_miibus);

	for (i = 0; i < GEM_NRXDESC; i++) {
		if (sc->sc_rxsoft[i].rxs_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_rdmatag,
			    sc->sc_rxsoft[i].rxs_dmamap);
	}
	for (i = 0; i < GEM_TXQUEUELEN; i++) {
		if (sc->sc_txsoft[i].txs_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_tdmatag,
			    sc->sc_txsoft[i].txs_dmamap);
	}
	GEM_CDSYNC(sc, BUS_DMASYNC_POSTREAD);
	GEM_CDSYNC(sc, BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_cdmatag, sc->sc_cddmamap);
	bus_dmamem_free(sc->sc_cdmatag, sc->sc_control_data,
	    sc->sc_cddmamap);
	bus_dma_tag_destroy(sc->sc_cdmatag);
	bus_dma_tag_destroy(sc->sc_tdmatag);
	bus_dma_tag_destroy(sc->sc_rdmatag);
	bus_dma_tag_destroy(sc->sc_pdmatag);
}

void
gem_suspend(sc)
	struct gem_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;

	gem_stop(ifp, 0);
}

void
gem_resume(sc)
	struct gem_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;

	if (ifp->if_flags & IFF_UP)
		gem_init(ifp);
}

static void
gem_cddma_callback(xsc, segs, nsegs, error)
	void *xsc;
	bus_dma_segment_t *segs;
	int nsegs;
	int error;
{
	struct gem_softc *sc = (struct gem_softc *)xsc;

	if (error != 0)
		return;
	if (nsegs != 1) {
		/* can't happen... */
		panic("gem_cddma_callback: bad control buffer segment count");
	}
	sc->sc_cddma = segs[0].ds_addr;
}

static void
gem_rxdma_callback(xsc, segs, nsegs, totsz, error)
	void *xsc;
	bus_dma_segment_t *segs;
	int nsegs;
	bus_size_t totsz;
	int error;
{
	struct gem_rxsoft *rxs = (struct gem_rxsoft *)xsc;

	if (error != 0)
		return;
	KASSERT(nsegs == 1, ("gem_rxdma_callback: bad dma segment count"));
	rxs->rxs_paddr = segs[0].ds_addr;
}

static void
gem_txdma_callback(xsc, segs, nsegs, totsz, error)
	void *xsc;
	bus_dma_segment_t *segs;
	int nsegs;
	bus_size_t totsz;
	int error;
{
	struct gem_txdma *txd = (struct gem_txdma *)xsc;
	struct gem_softc *sc = txd->txd_sc;
	struct gem_txsoft *txs = txd->txd_txs;
	bus_size_t len = 0;
	uint64_t flags = 0;
	int seg, nexttx;

	if (error != 0)
		return;
	/*
	 * Ensure we have enough descriptors free to describe
	 * the packet.  Note, we always reserve one descriptor
	 * at the end of the ring as a termination point, to
	 * prevent wrap-around.
	 */
	if (nsegs > sc->sc_txfree - 1) {
		txs->txs_ndescs = -1;
		return;
	}
	txs->txs_ndescs = nsegs;

	nexttx = txs->txs_firstdesc;
	/*
	 * Initialize the transmit descriptors.
	 */
	for (seg = 0; seg < nsegs;
	     seg++, nexttx = GEM_NEXTTX(nexttx)) {
		CTR5(KTR_GEM, "txdma_cb: mapping seg %d (txd %d), len "
		    "%lx, addr %#lx (%#lx)",  seg, nexttx,
		    segs[seg].ds_len, segs[seg].ds_addr,
		    GEM_DMA_WRITE(sc, segs[seg].ds_addr));

		if (segs[seg].ds_len == 0)
			continue;
		sc->sc_txdescs[nexttx].gd_addr =
		    GEM_DMA_WRITE(sc, segs[seg].ds_addr);
		KASSERT(segs[seg].ds_len < GEM_TD_BUFSIZE,
		    ("gem_txdma_callback: segment size too large!"));
		flags = segs[seg].ds_len & GEM_TD_BUFSIZE;
		if (len == 0) {
			CTR2(KTR_GEM, "txdma_cb: start of packet at seg %d, "
			    "tx %d", seg, nexttx);
			flags |= GEM_TD_START_OF_PACKET;
			if (++sc->sc_txwin > GEM_NTXSEGS * 2 / 3) {
				sc->sc_txwin = 0;
				flags |= GEM_TD_INTERRUPT_ME;
			}
		}
		if (len + segs[seg].ds_len == totsz) {
			CTR2(KTR_GEM, "txdma_cb: end of packet at seg %d, "
			    "tx %d", seg, nexttx);
			flags |= GEM_TD_END_OF_PACKET;
		}
		sc->sc_txdescs[nexttx].gd_flags = GEM_DMA_WRITE(sc, flags);
		txs->txs_lastdesc = nexttx;
		len += segs[seg].ds_len;
	}
	KASSERT((flags & GEM_TD_END_OF_PACKET) != 0,
	    ("gem_txdma_callback: missed end of packet!"));
}

static void
gem_tick(arg)
	void *arg;
{
	struct gem_softc *sc = arg;
	int s;

	s = splnet();
	mii_tick(sc->sc_mii);
	splx(s);

	callout_reset(&sc->sc_tick_ch, hz, gem_tick, sc);
}

static int
gem_bitwait(sc, r, clr, set)
	struct gem_softc *sc;
	bus_addr_t r;
	u_int32_t clr;
	u_int32_t set;
{
	int i;
	u_int32_t reg;

	for (i = TRIES; i--; DELAY(100)) {
		reg = bus_space_read_4(sc->sc_bustag, sc->sc_h, r);
		if ((r & clr) == 0 && (r & set) == set)
			return (1);
	}
	return (0);
}

void
gem_reset(sc)
	struct gem_softc *sc;
{
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t h = sc->sc_h;
	int s;

	s = splnet();
	CTR1(KTR_GEM, "%s: gem_reset", device_get_name(sc->sc_dev));
	gem_reset_rx(sc);
	gem_reset_tx(sc);

	/* Do a full reset */
	bus_space_write_4(t, h, GEM_RESET, GEM_RESET_RX | GEM_RESET_TX);
	if (!gem_bitwait(sc, GEM_RESET, GEM_RESET_RX | GEM_RESET_TX, 0))
		device_printf(sc->sc_dev, "cannot reset device\n");
	splx(s);
}


/*
 * gem_rxdrain:
 *
 *	Drain the receive queue.
 */
static void
gem_rxdrain(sc)
	struct gem_softc *sc;
{
	struct gem_rxsoft *rxs;
	int i;

	for (i = 0; i < GEM_NRXDESC; i++) {
		rxs = &sc->sc_rxsoft[i];
		if (rxs->rxs_mbuf != NULL) {
			bus_dmamap_sync(sc->sc_rdmatag, rxs->rxs_dmamap,
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->sc_rdmatag, rxs->rxs_dmamap);
			m_freem(rxs->rxs_mbuf);
			rxs->rxs_mbuf = NULL;
		}
	}
}

/*
 * Reset the whole thing.
 */
static void
gem_stop(ifp, disable)
	struct ifnet *ifp;
	int disable;
{
	struct gem_softc *sc = (struct gem_softc *)ifp->if_softc;
	struct gem_txsoft *txs;

	CTR1(KTR_GEM, "%s: gem_stop", device_get_name(sc->sc_dev));

	callout_stop(&sc->sc_tick_ch);

	/* XXX - Should we reset these instead? */
	gem_disable_tx(sc);
	gem_disable_rx(sc);

	/*
	 * Release any queued transmit buffers.
	 */
	while ((txs = STAILQ_FIRST(&sc->sc_txdirtyq)) != NULL) {
		STAILQ_REMOVE_HEAD(&sc->sc_txdirtyq, txs_q);
		if (txs->txs_ndescs != 0) {
			bus_dmamap_sync(sc->sc_tdmatag, txs->txs_dmamap,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_tdmatag, txs->txs_dmamap);
			if (txs->txs_mbuf != NULL) {
				m_freem(txs->txs_mbuf);
				txs->txs_mbuf = NULL;
			}
		}
		STAILQ_INSERT_TAIL(&sc->sc_txfreeq, txs, txs_q);
	}

	if (disable)
		gem_rxdrain(sc);

	/*
	 * Mark the interface down and cancel the watchdog timer.
	 */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;
}

/*
 * Reset the receiver
 */
int
gem_reset_rx(sc)
	struct gem_softc *sc;
{
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t h = sc->sc_h;

	/*
	 * Resetting while DMA is in progress can cause a bus hang, so we
	 * disable DMA first.
	 */
	gem_disable_rx(sc);
	bus_space_write_4(t, h, GEM_RX_CONFIG, 0);
	/* Wait till it finishes */
	if (!gem_bitwait(sc, GEM_RX_CONFIG, 1, 0))
		device_printf(sc->sc_dev, "cannot disable read dma\n");

	/* Wait 5ms extra. */
	DELAY(5000);

	/* Finally, reset the ERX */
	bus_space_write_4(t, h, GEM_RESET, GEM_RESET_RX);
	/* Wait till it finishes */
	if (!gem_bitwait(sc, GEM_RESET, GEM_RESET_TX, 0)) {
		device_printf(sc->sc_dev, "cannot reset receiver\n");
		return (1);
	}
	return (0);
}


/*
 * Reset the transmitter
 */
static int
gem_reset_tx(sc)
	struct gem_softc *sc;
{
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t h = sc->sc_h;
	int i;

	/*
	 * Resetting while DMA is in progress can cause a bus hang, so we
	 * disable DMA first.
	 */
	gem_disable_tx(sc);
	bus_space_write_4(t, h, GEM_TX_CONFIG, 0);
	/* Wait till it finishes */
	if (!gem_bitwait(sc, GEM_TX_CONFIG, 1, 0))
		device_printf(sc->sc_dev, "cannot disable read dma\n");

	/* Wait 5ms extra. */
	DELAY(5000);

	/* Finally, reset the ETX */
	bus_space_write_4(t, h, GEM_RESET, GEM_RESET_TX);
	/* Wait till it finishes */
	for (i = TRIES; i--; DELAY(100))
		if ((bus_space_read_4(t, h, GEM_RESET) & GEM_RESET_TX) == 0)
			break;
	if (!gem_bitwait(sc, GEM_RESET, GEM_RESET_TX, 0)) {
		device_printf(sc->sc_dev, "cannot reset receiver\n");
		return (1);
	}
	return (0);
}

/*
 * disable receiver.
 */
static int
gem_disable_rx(sc)
	struct gem_softc *sc;
{
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t h = sc->sc_h;
	u_int32_t cfg;

	/* Flip the enable bit */
	cfg = bus_space_read_4(t, h, GEM_MAC_RX_CONFIG);
	cfg &= ~GEM_MAC_RX_ENABLE;
	bus_space_write_4(t, h, GEM_MAC_RX_CONFIG, cfg);

	/* Wait for it to finish */
	return (gem_bitwait(sc, GEM_MAC_RX_CONFIG, GEM_MAC_RX_ENABLE, 0));
}

/*
 * disable transmitter.
 */
static int
gem_disable_tx(sc)
	struct gem_softc *sc;
{
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t h = sc->sc_h;
	u_int32_t cfg;

	/* Flip the enable bit */
	cfg = bus_space_read_4(t, h, GEM_MAC_TX_CONFIG);
	cfg &= ~GEM_MAC_TX_ENABLE;
	bus_space_write_4(t, h, GEM_MAC_TX_CONFIG, cfg);

	/* Wait for it to finish */
	return (gem_bitwait(sc, GEM_MAC_TX_CONFIG, GEM_MAC_TX_ENABLE, 0));
}

/*
 * Initialize interface.
 */
static int
gem_meminit(sc)
	struct gem_softc *sc;
{
	struct gem_rxsoft *rxs;
	int i, error;

	/*
	 * Initialize the transmit descriptor ring.
	 */
	memset((void *)sc->sc_txdescs, 0, sizeof(sc->sc_txdescs));
	for (i = 0; i < GEM_NTXDESC; i++) {
		sc->sc_txdescs[i].gd_flags = 0;
		sc->sc_txdescs[i].gd_addr = 0;
	}
	sc->sc_txfree = GEM_MAXTXFREE;
	sc->sc_txnext = 0;
	sc->sc_txwin = 0;

	/*
	 * Initialize the receive descriptor and receive job
	 * descriptor rings.
	 */
	for (i = 0; i < GEM_NRXDESC; i++) {
		rxs = &sc->sc_rxsoft[i];
		if (rxs->rxs_mbuf == NULL) {
			if ((error = gem_add_rxbuf(sc, i)) != 0) {
				device_printf(sc->sc_dev, "unable to "
				    "allocate or map rx buffer %d, error = "
				    "%d\n", i, error);
				/*
				 * XXX Should attempt to run with fewer receive
				 * XXX buffers instead of just failing.
				 */
				gem_rxdrain(sc);
				return (1);
			}
		} else
			GEM_INIT_RXDESC(sc, i);
	}
	sc->sc_rxptr = 0;
	GEM_CDSYNC(sc, BUS_DMASYNC_PREWRITE);
	GEM_CDSYNC(sc, BUS_DMASYNC_PREREAD);

	return (0);
}

static int
gem_ringsize(sz)
	int sz;
{
	int v = 0;

	switch (sz) {
	case 32:
		v = GEM_RING_SZ_32;
		break;
	case 64:
		v = GEM_RING_SZ_64;
		break;
	case 128:
		v = GEM_RING_SZ_128;
		break;
	case 256:
		v = GEM_RING_SZ_256;
		break;
	case 512:
		v = GEM_RING_SZ_512;
		break;
	case 1024:
		v = GEM_RING_SZ_1024;
		break;
	case 2048:
		v = GEM_RING_SZ_2048;
		break;
	case 4096:
		v = GEM_RING_SZ_4096;
		break;
	case 8192:
		v = GEM_RING_SZ_8192;
		break;
	default:
		printf("gem: invalid Receive Descriptor ring size\n");
		break;
	}
	return (v);
}

/*
 * Initialization of interface; set up initialization block
 * and transmit/receive descriptor rings.
 */
static void
gem_init(xsc)
	void *xsc;
{
	struct gem_softc *sc = (struct gem_softc *)xsc;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t h = sc->sc_h;
	int s;
	u_int32_t v;

	s = splnet();

	CTR1(KTR_GEM, "%s: gem_init: calling stop", device_get_name(sc->sc_dev));
	/*
	 * Initialization sequence. The numbered steps below correspond
	 * to the sequence outlined in section 6.3.5.1 in the Ethernet
	 * Channel Engine manual (part of the PCIO manual).
	 * See also the STP2002-STQ document from Sun Microsystems.
	 */

	/* step 1 & 2. Reset the Ethernet Channel */
	gem_stop(&sc->sc_arpcom.ac_if, 0);
	gem_reset(sc);
	CTR1(KTR_GEM, "%s: gem_init: restarting", device_get_name(sc->sc_dev));

	/* Re-initialize the MIF */
	gem_mifinit(sc);

	/* step 3. Setup data structures in host memory */
	gem_meminit(sc);

	/* step 4. TX MAC registers & counters */
	gem_init_regs(sc);
	/* XXX: VLAN code from NetBSD temporarily removed. */
	bus_space_write_4(t, h, GEM_MAC_MAC_MAX_FRAME,
            (ETHER_MAX_LEN + sizeof(struct ether_header)) | (0x2000<<16));

	/* step 5. RX MAC registers & counters */
	gem_setladrf(sc);

	/* step 6 & 7. Program Descriptor Ring Base Addresses */
	/* NOTE: we use only 32-bit DMA addresses here. */
	bus_space_write_4(t, h, GEM_TX_RING_PTR_HI, 0);
	bus_space_write_4(t, h, GEM_TX_RING_PTR_LO, GEM_CDTXADDR(sc, 0));

	bus_space_write_4(t, h, GEM_RX_RING_PTR_HI, 0);
	bus_space_write_4(t, h, GEM_RX_RING_PTR_LO, GEM_CDRXADDR(sc, 0));
	CTR3(KTR_GEM, "loading rx ring %lx, tx ring %lx, cddma %lx",
	    GEM_CDRXADDR(sc, 0), GEM_CDTXADDR(sc, 0), sc->sc_cddma);

	/* step 8. Global Configuration & Interrupt Mask */
	bus_space_write_4(t, h, GEM_INTMASK,
		      ~(GEM_INTR_TX_INTME|
			GEM_INTR_TX_EMPTY|
			GEM_INTR_RX_DONE|GEM_INTR_RX_NOBUF|
			GEM_INTR_RX_TAG_ERR|GEM_INTR_PCS|
			GEM_INTR_MAC_CONTROL|GEM_INTR_MIF|
			GEM_INTR_BERR));
	bus_space_write_4(t, h, GEM_MAC_RX_MASK,
			GEM_MAC_RX_DONE|GEM_MAC_RX_FRAME_CNT);
	bus_space_write_4(t, h, GEM_MAC_TX_MASK, 0xffff); /* XXXX */
	bus_space_write_4(t, h, GEM_MAC_CONTROL_MASK, 0); /* XXXX */

	/* step 9. ETX Configuration: use mostly default values */

	/* Enable DMA */
	v = gem_ringsize(GEM_NTXDESC /*XXX*/);
	bus_space_write_4(t, h, GEM_TX_CONFIG,
		v|GEM_TX_CONFIG_TXDMA_EN|
		((0x400<<10)&GEM_TX_CONFIG_TXFIFO_TH));

	/* step 10. ERX Configuration */

	/* Encode Receive Descriptor ring size: four possible values */
	v = gem_ringsize(GEM_NRXDESC /*XXX*/);

	/* Enable DMA */
	bus_space_write_4(t, h, GEM_RX_CONFIG,
		v|(GEM_THRSH_1024<<GEM_RX_CONFIG_FIFO_THRS_SHIFT)|
		(2<<GEM_RX_CONFIG_FBOFF_SHFT)|GEM_RX_CONFIG_RXDMA_EN|
		(0<<GEM_RX_CONFIG_CXM_START_SHFT));
	/*
	 * The following value is for an OFF Threshold of about 3/4 full
	 * and an ON Threshold of 1/4 full.
	 */
	bus_space_write_4(t, h, GEM_RX_PAUSE_THRESH,
	    (3 * sc->sc_rxfifosize / 256) |
	    (   (sc->sc_rxfifosize / 256) << 12));
	bus_space_write_4(t, h, GEM_RX_BLANKING, (6<<12)|6);

	/* step 11. Configure Media */
	mii_mediachg(sc->sc_mii);

	/* step 12. RX_MAC Configuration Register */
	v = bus_space_read_4(t, h, GEM_MAC_RX_CONFIG);
	v |= GEM_MAC_RX_ENABLE;
	bus_space_write_4(t, h, GEM_MAC_RX_CONFIG, v);

	/* step 14. Issue Transmit Pending command */

	/* step 15.  Give the reciever a swift kick */
	bus_space_write_4(t, h, GEM_RX_KICK, GEM_NRXDESC-4);

	/* Start the one second timer. */
	callout_reset(&sc->sc_tick_ch, hz, gem_tick, sc);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_timer = 0;
	sc->sc_ifflags = ifp->if_flags;
	splx(s);
}

static int
gem_load_txmbuf(sc, m0)
	struct gem_softc *sc;
	struct mbuf *m0;
{
	struct gem_txdma txd;
	struct gem_txsoft *txs;
	int error;

	/* Get a work queue entry. */
	if ((txs = STAILQ_FIRST(&sc->sc_txfreeq)) == NULL) {
		/* Ran out of descriptors. */
		return (-1);
	}
	txd.txd_sc = sc;
	txd.txd_txs = txs;
	txs->txs_mbuf = m0;
	txs->txs_firstdesc = sc->sc_txnext;
	error = bus_dmamap_load_mbuf(sc->sc_tdmatag, txs->txs_dmamap, m0,
	    gem_txdma_callback, &txd, BUS_DMA_NOWAIT);
	if (error != 0)
		goto fail;
	if (txs->txs_ndescs == -1) {
		error = -1;
		goto fail;
	}

	/* Sync the DMA map. */
	bus_dmamap_sync(sc->sc_tdmatag, txs->txs_dmamap,
	    BUS_DMASYNC_PREWRITE);

	CTR3(KTR_GEM, "load_mbuf: setting firstdesc=%d, lastdesc=%d, "
	    "ndescs=%d", txs->txs_firstdesc, txs->txs_lastdesc,
	    txs->txs_ndescs);
	STAILQ_REMOVE_HEAD(&sc->sc_txfreeq, txs_q);
	STAILQ_INSERT_TAIL(&sc->sc_txdirtyq, txs, txs_q);

	sc->sc_txnext = GEM_NEXTTX(txs->txs_lastdesc);
	sc->sc_txfree -= txs->txs_ndescs;
	return (0);

fail:
	CTR1(KTR_GEM, "gem_load_txmbuf failed (%d)", error);
	bus_dmamap_unload(sc->sc_tdmatag, txs->txs_dmamap);
	return (error);
}

static void
gem_init_regs(sc)
	struct gem_softc *sc;
{
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t h = sc->sc_h;
	const u_char *laddr = sc->sc_arpcom.ac_enaddr;
	u_int32_t v;

	/* These regs are not cleared on reset */
	if (!sc->sc_inited) {

		/* Wooo.  Magic values. */
		bus_space_write_4(t, h, GEM_MAC_IPG0, 0);
		bus_space_write_4(t, h, GEM_MAC_IPG1, 8);
		bus_space_write_4(t, h, GEM_MAC_IPG2, 4);

		bus_space_write_4(t, h, GEM_MAC_MAC_MIN_FRAME, ETHER_MIN_LEN);
		/* Max frame and max burst size */
		bus_space_write_4(t, h, GEM_MAC_MAC_MAX_FRAME,
		    ETHER_MAX_LEN | (0x2000<<16));

		bus_space_write_4(t, h, GEM_MAC_PREAMBLE_LEN, 0x7);
		bus_space_write_4(t, h, GEM_MAC_JAM_SIZE, 0x4);
		bus_space_write_4(t, h, GEM_MAC_ATTEMPT_LIMIT, 0x10);
		/* Dunno.... */
		bus_space_write_4(t, h, GEM_MAC_CONTROL_TYPE, 0x8088);
		bus_space_write_4(t, h, GEM_MAC_RANDOM_SEED,
		    ((laddr[5]<<8)|laddr[4])&0x3ff);

		/* Secondary MAC addr set to 0:0:0:0:0:0 */
		bus_space_write_4(t, h, GEM_MAC_ADDR3, 0);
		bus_space_write_4(t, h, GEM_MAC_ADDR4, 0);
		bus_space_write_4(t, h, GEM_MAC_ADDR5, 0);

		/* MAC control addr set to 01:80:c2:00:00:01 */
		bus_space_write_4(t, h, GEM_MAC_ADDR6, 0x0001);
		bus_space_write_4(t, h, GEM_MAC_ADDR7, 0xc200);
		bus_space_write_4(t, h, GEM_MAC_ADDR8, 0x0180);

		/* MAC filter addr set to 0:0:0:0:0:0 */
		bus_space_write_4(t, h, GEM_MAC_ADDR_FILTER0, 0);
		bus_space_write_4(t, h, GEM_MAC_ADDR_FILTER1, 0);
		bus_space_write_4(t, h, GEM_MAC_ADDR_FILTER2, 0);

		bus_space_write_4(t, h, GEM_MAC_ADR_FLT_MASK1_2, 0);
		bus_space_write_4(t, h, GEM_MAC_ADR_FLT_MASK0, 0);

		sc->sc_inited = 1;
	}

	/* Counters need to be zeroed */
	bus_space_write_4(t, h, GEM_MAC_NORM_COLL_CNT, 0);
	bus_space_write_4(t, h, GEM_MAC_FIRST_COLL_CNT, 0);
	bus_space_write_4(t, h, GEM_MAC_EXCESS_COLL_CNT, 0);
	bus_space_write_4(t, h, GEM_MAC_LATE_COLL_CNT, 0);
	bus_space_write_4(t, h, GEM_MAC_DEFER_TMR_CNT, 0);
	bus_space_write_4(t, h, GEM_MAC_PEAK_ATTEMPTS, 0);
	bus_space_write_4(t, h, GEM_MAC_RX_FRAME_COUNT, 0);
	bus_space_write_4(t, h, GEM_MAC_RX_LEN_ERR_CNT, 0);
	bus_space_write_4(t, h, GEM_MAC_RX_ALIGN_ERR, 0);
	bus_space_write_4(t, h, GEM_MAC_RX_CRC_ERR_CNT, 0);
	bus_space_write_4(t, h, GEM_MAC_RX_CODE_VIOL, 0);

	/* Un-pause stuff */
#if 0
	bus_space_write_4(t, h, GEM_MAC_SEND_PAUSE_CMD, 0x1BF0);
#else
	bus_space_write_4(t, h, GEM_MAC_SEND_PAUSE_CMD, 0);
#endif

	/*
	 * Set the station address.
	 */
	bus_space_write_4(t, h, GEM_MAC_ADDR0, (laddr[4]<<8)|laddr[5]);
	bus_space_write_4(t, h, GEM_MAC_ADDR1, (laddr[2]<<8)|laddr[3]);
	bus_space_write_4(t, h, GEM_MAC_ADDR2, (laddr[0]<<8)|laddr[1]);

	/*
	 * Enable MII outputs.  Enable GMII if there is a gigabit PHY.
	 */
	sc->sc_mif_config = bus_space_read_4(t, h, GEM_MIF_CONFIG);
	v = GEM_MAC_XIF_TX_MII_ENA;
	if (sc->sc_mif_config & GEM_MIF_CONFIG_MDI1) {
		v |= GEM_MAC_XIF_FDPLX_LED;
		if (sc->sc_flags & GEM_GIGABIT)
			v |= GEM_MAC_XIF_GMII_MODE;
	}
	bus_space_write_4(t, h, GEM_MAC_XIF_CONFIG, v);
}

static void
gem_start(ifp)
	struct ifnet *ifp;
{
	struct gem_softc *sc = (struct gem_softc *)ifp->if_softc;
	struct mbuf *m0 = NULL;
	int firsttx, ntx, ofree, txmfail;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	/*
	 * Remember the previous number of free descriptors and
	 * the first descriptor we'll use.
	 */
	ofree = sc->sc_txfree;
	firsttx = sc->sc_txnext;

	CTR3(KTR_GEM, "%s: gem_start: txfree %d, txnext %d",
	    device_get_name(sc->sc_dev), ofree, firsttx);

	/*
	 * Loop through the send queue, setting up transmit descriptors
	 * until we drain the queue, or use up all available transmit
	 * descriptors.
	 */
	txmfail = 0;
	for (ntx = 0;; ntx++) {
		/*
		 * Grab a packet off the queue.
		 */
		IF_DEQUEUE(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;

		txmfail = gem_load_txmbuf(sc, m0);
		if (txmfail > 0) {
			/* Drop the mbuf and complain. */
			printf("gem_start: error %d while loading mbuf dma "
			    "map\n", txmfail);
			continue;
		}
		/* Not enough descriptors. */
		if (txmfail == -1) {
			if (sc->sc_txfree == GEM_MAXTXFREE)
				panic("gem_start: mbuf chain too long!");
			IF_PREPEND(&ifp->if_snd, m0);
			break;
		}

		/* Kick the transmitter. */
		CTR2(KTR_GEM, "%s: gem_start: kicking tx %d",
		    device_get_name(sc->sc_dev), sc->sc_txnext);
		bus_space_write_4(sc->sc_bustag, sc->sc_h, GEM_TX_KICK,
			sc->sc_txnext);

		if (ifp->if_bpf != NULL)
			bpf_mtap(ifp->if_bpf, m0);
	}

	if (txmfail == -1 || sc->sc_txfree == 0) {
		/* No more slots left; notify upper layer. */
		ifp->if_flags |= IFF_OACTIVE;
	}

	if (ntx > 0) {
		GEM_CDSYNC(sc, BUS_DMASYNC_PREWRITE);

		CTR2(KTR_GEM, "%s: packets enqueued, OWN on %d",
		    device_get_name(sc->sc_dev), firsttx);

		/* Set a watchdog timer in case the chip flakes out. */
		ifp->if_timer = 5;
		CTR2(KTR_GEM, "%s: gem_start: watchdog %d",
			device_get_name(sc->sc_dev), ifp->if_timer);
	}
}

/*
 * Transmit interrupt.
 */
static void
gem_tint(sc)
	struct gem_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t mac = sc->sc_h;
	struct gem_txsoft *txs;
	int txlast;
	int progress = 0;


	CTR1(KTR_GEM, "%s: gem_tint", device_get_name(sc->sc_dev));

	/*
	 * Unload collision counters
	 */
	ifp->if_collisions +=
		bus_space_read_4(t, mac, GEM_MAC_NORM_COLL_CNT) +
		bus_space_read_4(t, mac, GEM_MAC_FIRST_COLL_CNT) +
		bus_space_read_4(t, mac, GEM_MAC_EXCESS_COLL_CNT) +
		bus_space_read_4(t, mac, GEM_MAC_LATE_COLL_CNT);

	/*
	 * then clear the hardware counters.
	 */
	bus_space_write_4(t, mac, GEM_MAC_NORM_COLL_CNT, 0);
	bus_space_write_4(t, mac, GEM_MAC_FIRST_COLL_CNT, 0);
	bus_space_write_4(t, mac, GEM_MAC_EXCESS_COLL_CNT, 0);
	bus_space_write_4(t, mac, GEM_MAC_LATE_COLL_CNT, 0);

	/*
	 * Go through our Tx list and free mbufs for those
	 * frames that have been transmitted.
	 */
	GEM_CDSYNC(sc, BUS_DMASYNC_POSTREAD);
	while ((txs = STAILQ_FIRST(&sc->sc_txdirtyq)) != NULL) {

#ifdef GEM_DEBUG
		if (ifp->if_flags & IFF_DEBUG) {
			int i;
			printf("    txsoft %p transmit chain:\n", txs);
			for (i = txs->txs_firstdesc;; i = GEM_NEXTTX(i)) {
				printf("descriptor %d: ", i);
				printf("gd_flags: 0x%016llx\t", (long long)
					GEM_DMA_READ(sc, sc->sc_txdescs[i].gd_flags));
				printf("gd_addr: 0x%016llx\n", (long long)
					GEM_DMA_READ(sc, sc->sc_txdescs[i].gd_addr));
				if (i == txs->txs_lastdesc)
					break;
			}
		}
#endif

		/*
		 * In theory, we could harveast some descriptors before
		 * the ring is empty, but that's a bit complicated.
		 *
		 * GEM_TX_COMPLETION points to the last descriptor
		 * processed +1.
		 */
		txlast = bus_space_read_4(t, mac, GEM_TX_COMPLETION);
		CTR3(KTR_GEM, "gem_tint: txs->txs_firstdesc = %d, "
		    "txs->txs_lastdesc = %d, txlast = %d",
		    txs->txs_firstdesc, txs->txs_lastdesc, txlast);
		if (txs->txs_firstdesc <= txs->txs_lastdesc) {
			if ((txlast >= txs->txs_firstdesc) &&
				(txlast <= txs->txs_lastdesc))
				break;
		} else {
			/* Ick -- this command wraps */
			if ((txlast >= txs->txs_firstdesc) ||
				(txlast <= txs->txs_lastdesc))
				break;
		}

		CTR0(KTR_GEM, "gem_tint: releasing a desc");
		STAILQ_REMOVE_HEAD(&sc->sc_txdirtyq, txs_q);

		sc->sc_txfree += txs->txs_ndescs;

		bus_dmamap_sync(sc->sc_tdmatag, txs->txs_dmamap,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_tdmatag, txs->txs_dmamap);
		if (txs->txs_mbuf != NULL) {
			m_freem(txs->txs_mbuf);
			txs->txs_mbuf = NULL;
		}

		STAILQ_INSERT_TAIL(&sc->sc_txfreeq, txs, txs_q);

		ifp->if_opackets++;
		progress = 1;
	}

	CTR3(KTR_GEM, "gem_tint: GEM_TX_STATE_MACHINE %x "
		"GEM_TX_DATA_PTR %llx "
		"GEM_TX_COMPLETION %x",
		bus_space_read_4(sc->sc_bustag, sc->sc_h, GEM_TX_STATE_MACHINE),
		((long long) bus_space_read_4(sc->sc_bustag, sc->sc_h,
			GEM_TX_DATA_PTR_HI) << 32) |
			     bus_space_read_4(sc->sc_bustag, sc->sc_h,
			GEM_TX_DATA_PTR_LO),
		bus_space_read_4(sc->sc_bustag, sc->sc_h, GEM_TX_COMPLETION));

	if (progress) {
		if (sc->sc_txfree == GEM_NTXDESC - 1)
			sc->sc_txwin = 0;

		/* Freed some descriptors, so reset IFF_OACTIVE and restart. */
		ifp->if_flags &= ~IFF_OACTIVE;
		gem_start(ifp);

		if (STAILQ_EMPTY(&sc->sc_txdirtyq))
			ifp->if_timer = 0;
	}

	CTR2(KTR_GEM, "%s: gem_tint: watchdog %d",
		device_get_name(sc->sc_dev), ifp->if_timer);
}

#if 0
static void
gem_rint_timeout(arg)
	void *arg;
{

	gem_rint((struct gem_softc *)arg);
}
#endif

/*
 * Receive interrupt.
 */
static void
gem_rint(sc)
	struct gem_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t h = sc->sc_h;
	struct gem_rxsoft *rxs;
	struct mbuf *m;
	u_int64_t rxstat;
	u_int32_t rxcomp;
	int i, len, progress = 0;

	callout_stop(&sc->sc_rx_ch);
	CTR1(KTR_GEM, "%s: gem_rint", device_get_name(sc->sc_dev));

	/*
	 * Read the completion register once.  This limits
	 * how long the following loop can execute.
	 */
	rxcomp = bus_space_read_4(t, h, GEM_RX_COMPLETION);

	CTR2(KTR_GEM, "gem_rint: sc->rxptr %d, complete %d",
	    sc->sc_rxptr, rxcomp);
	GEM_CDSYNC(sc, BUS_DMASYNC_POSTREAD);
	for (i = sc->sc_rxptr; i != rxcomp;
	     i = GEM_NEXTRX(i)) {
		rxs = &sc->sc_rxsoft[i];

		rxstat = GEM_DMA_READ(sc, sc->sc_rxdescs[i].gd_flags);

		if (rxstat & GEM_RD_OWN) {
#if 0 /* XXX: In case of emergency, re-enable this. */
			/*
			 * The descriptor is still marked as owned, although
			 * it is supposed to have completed. This has been
			 * observed on some machines. Just exiting here
			 * might leave the packet sitting around until another
			 * one arrives to trigger a new interrupt, which is
			 * generally undesirable, so set up a timeout.
			 */
			callout_reset(&sc->sc_rx_ch, GEM_RXOWN_TICKS,
			    gem_rint_timeout, sc);
#endif
			break;
		}

		progress++;
		ifp->if_ipackets++;

		if (rxstat & GEM_RD_BAD_CRC) {
			ifp->if_ierrors++;
			device_printf(sc->sc_dev, "receive error: CRC error\n");
			GEM_INIT_RXDESC(sc, i);
			continue;
		}

#ifdef GEM_DEBUG
		if (ifp->if_flags & IFF_DEBUG) {
			printf("    rxsoft %p descriptor %d: ", rxs, i);
			printf("gd_flags: 0x%016llx\t", (long long)
				GEM_DMA_READ(sc, sc->sc_rxdescs[i].gd_flags));
			printf("gd_addr: 0x%016llx\n", (long long)
				GEM_DMA_READ(sc, sc->sc_rxdescs[i].gd_addr));
		}
#endif

		/*
		 * No errors; receive the packet.  Note the Gem
		 * includes the CRC with every packet.
		 */
		len = GEM_RD_BUFLEN(rxstat);

		/*
		 * Allocate a new mbuf cluster.  If that fails, we are
		 * out of memory, and must drop the packet and recycle
		 * the buffer that's already attached to this descriptor.
		 */
		m = rxs->rxs_mbuf;
		if (gem_add_rxbuf(sc, i) != 0) {
			ifp->if_ierrors++;
			GEM_INIT_RXDESC(sc, i);
			continue;
		}
		m->m_data += 2; /* We're already off by two */

		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = len - ETHER_CRC_LEN;

		/* Pass it on. */
		(*ifp->if_input)(ifp, m);
	}

	if (progress) {
		GEM_CDSYNC(sc, BUS_DMASYNC_PREWRITE);
		/* Update the receive pointer. */
		if (i == sc->sc_rxptr) {
			device_printf(sc->sc_dev, "rint: ring wrap\n");
		}
		sc->sc_rxptr = i;
		bus_space_write_4(t, h, GEM_RX_KICK, GEM_PREVRX(i));
	}

	CTR2(KTR_GEM, "gem_rint: done sc->rxptr %d, complete %d",
		sc->sc_rxptr, bus_space_read_4(t, h, GEM_RX_COMPLETION));
}


/*
 * gem_add_rxbuf:
 *
 *	Add a receive buffer to the indicated descriptor.
 */
static int
gem_add_rxbuf(sc, idx)
	struct gem_softc *sc;
	int idx;
{
	struct gem_rxsoft *rxs = &sc->sc_rxsoft[idx];
	struct mbuf *m;
	int error;

	m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
	if (m == NULL)
		return (ENOBUFS);
	m->m_len = m->m_pkthdr.len = m->m_ext.ext_size;

#ifdef GEM_DEBUG
	/* bzero the packet to check dma */
	memset(m->m_ext.ext_buf, 0, m->m_ext.ext_size);
#endif

	if (rxs->rxs_mbuf != NULL) {
		bus_dmamap_sync(sc->sc_rdmatag, rxs->rxs_dmamap,
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_rdmatag, rxs->rxs_dmamap);
	}

	rxs->rxs_mbuf = m;

	error = bus_dmamap_load_mbuf(sc->sc_rdmatag, rxs->rxs_dmamap,
	    m, gem_rxdma_callback, rxs, BUS_DMA_NOWAIT);
	if (error != 0 || rxs->rxs_paddr == 0) {
		device_printf(sc->sc_dev, "can't load rx DMA map %d, error = "
		    "%d\n", idx, error);
		panic("gem_add_rxbuf");	/* XXX */
	}

	bus_dmamap_sync(sc->sc_rdmatag, rxs->rxs_dmamap, BUS_DMASYNC_PREREAD);

	GEM_INIT_RXDESC(sc, idx);

	return (0);
}


static void
gem_eint(sc, status)
	struct gem_softc *sc;
	u_int status;
{

	if ((status & GEM_INTR_MIF) != 0) {
		device_printf(sc->sc_dev, "XXXlink status changed\n");
		return;
	}

	device_printf(sc->sc_dev, "status=%x\n", status);
}


void
gem_intr(v)
	void *v;
{
	struct gem_softc *sc = (struct gem_softc *)v;
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t seb = sc->sc_h;
	u_int32_t status;

	status = bus_space_read_4(t, seb, GEM_STATUS);
	CTR3(KTR_GEM, "%s: gem_intr: cplt %x, status %x",
		device_get_name(sc->sc_dev), (status>>19),
		(u_int)status);

	if ((status & (GEM_INTR_RX_TAG_ERR | GEM_INTR_BERR)) != 0)
		gem_eint(sc, status);

	if ((status & (GEM_INTR_TX_EMPTY | GEM_INTR_TX_INTME)) != 0)
		gem_tint(sc);

	if ((status & (GEM_INTR_RX_DONE | GEM_INTR_RX_NOBUF)) != 0)
		gem_rint(sc);

	/* We should eventually do more than just print out error stats. */
	if (status & GEM_INTR_TX_MAC) {
		int txstat = bus_space_read_4(t, seb, GEM_MAC_TX_STATUS);
		if (txstat & ~GEM_MAC_TX_XMIT_DONE)
			device_printf(sc->sc_dev, "MAC tx fault, status %x\n",
			    txstat);
		if (txstat & (GEM_MAC_TX_UNDERRUN | GEM_MAC_TX_PKT_TOO_LONG))
			gem_init(sc);
	}
	if (status & GEM_INTR_RX_MAC) {
		int rxstat = bus_space_read_4(t, seb, GEM_MAC_RX_STATUS);
		if (rxstat & ~(GEM_MAC_RX_DONE | GEM_MAC_RX_FRAME_CNT))
			device_printf(sc->sc_dev, "MAC rx fault, status %x\n",
			    rxstat);
		if ((rxstat & GEM_MAC_RX_OVERFLOW) != 0)
			gem_init(sc);
	}
}


static void
gem_watchdog(ifp)
	struct ifnet *ifp;
{
	struct gem_softc *sc = ifp->if_softc;

	CTR3(KTR_GEM, "gem_watchdog: GEM_RX_CONFIG %x GEM_MAC_RX_STATUS %x "
		"GEM_MAC_RX_CONFIG %x",
		bus_space_read_4(sc->sc_bustag, sc->sc_h, GEM_RX_CONFIG),
		bus_space_read_4(sc->sc_bustag, sc->sc_h, GEM_MAC_RX_STATUS),
		bus_space_read_4(sc->sc_bustag, sc->sc_h, GEM_MAC_RX_CONFIG));
	CTR3(KTR_GEM, "gem_watchdog: GEM_TX_CONFIG %x GEM_MAC_TX_STATUS %x "
		"GEM_MAC_TX_CONFIG %x",
		bus_space_read_4(sc->sc_bustag, sc->sc_h, GEM_TX_CONFIG),
		bus_space_read_4(sc->sc_bustag, sc->sc_h, GEM_MAC_TX_STATUS),
		bus_space_read_4(sc->sc_bustag, sc->sc_h, GEM_MAC_TX_CONFIG));

	device_printf(sc->sc_dev, "device timeout\n");
	++ifp->if_oerrors;

	/* Try to get more packets going. */
	gem_start(ifp);
}

/*
 * Initialize the MII Management Interface
 */
static void
gem_mifinit(sc)
	struct gem_softc *sc;
{
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t mif = sc->sc_h;

	/* Configure the MIF in frame mode */
	sc->sc_mif_config = bus_space_read_4(t, mif, GEM_MIF_CONFIG);
	sc->sc_mif_config &= ~GEM_MIF_CONFIG_BB_ENA;
	bus_space_write_4(t, mif, GEM_MIF_CONFIG, sc->sc_mif_config);
}

/*
 * MII interface
 *
 * The GEM MII interface supports at least three different operating modes:
 *
 * Bitbang mode is implemented using data, clock and output enable registers.
 *
 * Frame mode is implemented by loading a complete frame into the frame
 * register and polling the valid bit for completion.
 *
 * Polling mode uses the frame register but completion is indicated by
 * an interrupt.
 *
 */
int
gem_mii_readreg(dev, phy, reg)
	device_t dev;
	int phy, reg;
{
	struct gem_softc *sc = device_get_softc(dev);
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t mif = sc->sc_h;
	int n;
	u_int32_t v;

#ifdef GEM_DEBUG_PHY
	printf("gem_mii_readreg: phy %d reg %d\n", phy, reg);
#endif

#if 0
	/* Select the desired PHY in the MIF configuration register */
	v = bus_space_read_4(t, mif, GEM_MIF_CONFIG);
	/* Clear PHY select bit */
	v &= ~GEM_MIF_CONFIG_PHY_SEL;
	if (phy == GEM_PHYAD_EXTERNAL)
		/* Set PHY select bit to get at external device */
		v |= GEM_MIF_CONFIG_PHY_SEL;
	bus_space_write_4(t, mif, GEM_MIF_CONFIG, v);
#endif

	/* Construct the frame command */
	v = (reg << GEM_MIF_REG_SHIFT)	| (phy << GEM_MIF_PHY_SHIFT) |
		GEM_MIF_FRAME_READ;

	bus_space_write_4(t, mif, GEM_MIF_FRAME, v);
	for (n = 0; n < 100; n++) {
		DELAY(1);
		v = bus_space_read_4(t, mif, GEM_MIF_FRAME);
		if (v & GEM_MIF_FRAME_TA0)
			return (v & GEM_MIF_FRAME_DATA);
	}

	device_printf(sc->sc_dev, "mii_read timeout\n");
	return (0);
}

int
gem_mii_writereg(dev, phy, reg, val)
	device_t dev;
	int phy, reg, val;
{
	struct gem_softc *sc = device_get_softc(dev);
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t mif = sc->sc_h;
	int n;
	u_int32_t v;

#ifdef GEM_DEBUG_PHY
	printf("gem_mii_writereg: phy %d reg %d val %x\n", phy, reg, val);
#endif

#if 0
	/* Select the desired PHY in the MIF configuration register */
	v = bus_space_read_4(t, mif, GEM_MIF_CONFIG);
	/* Clear PHY select bit */
	v &= ~GEM_MIF_CONFIG_PHY_SEL;
	if (phy == GEM_PHYAD_EXTERNAL)
		/* Set PHY select bit to get at external device */
		v |= GEM_MIF_CONFIG_PHY_SEL;
	bus_space_write_4(t, mif, GEM_MIF_CONFIG, v);
#endif
	/* Construct the frame command */
	v = GEM_MIF_FRAME_WRITE			|
	    (phy << GEM_MIF_PHY_SHIFT)		|
	    (reg << GEM_MIF_REG_SHIFT)		|
	    (val & GEM_MIF_FRAME_DATA);

	bus_space_write_4(t, mif, GEM_MIF_FRAME, v);
	for (n = 0; n < 100; n++) {
		DELAY(1);
		v = bus_space_read_4(t, mif, GEM_MIF_FRAME);
		if (v & GEM_MIF_FRAME_TA0)
			return (1);
	}

	device_printf(sc->sc_dev, "mii_write timeout\n");
	return (0);
}

void
gem_mii_statchg(dev)
	device_t dev;
{
	struct gem_softc *sc = device_get_softc(dev);
#ifdef GEM_DEBUG
	int instance = IFM_INST(sc->sc_mii->mii_media.ifm_cur->ifm_media);
#endif
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t mac = sc->sc_h;
	u_int32_t v;

#ifdef GEM_DEBUG
	if (sc->sc_debug)
		printf("gem_mii_statchg: status change: phy = %d\n",
			sc->sc_phys[instance]);
#endif

	/* Set tx full duplex options */
	bus_space_write_4(t, mac, GEM_MAC_TX_CONFIG, 0);
	DELAY(10000); /* reg must be cleared and delay before changing. */
	v = GEM_MAC_TX_ENA_IPG0|GEM_MAC_TX_NGU|GEM_MAC_TX_NGU_LIMIT|
		GEM_MAC_TX_ENABLE;
	if ((IFM_OPTIONS(sc->sc_mii->mii_media_active) & IFM_FDX) != 0) {
		v |= GEM_MAC_TX_IGN_CARRIER|GEM_MAC_TX_IGN_COLLIS;
	}
	bus_space_write_4(t, mac, GEM_MAC_TX_CONFIG, v);

	/* XIF Configuration */
 /* We should really calculate all this rather than rely on defaults */
	v = bus_space_read_4(t, mac, GEM_MAC_XIF_CONFIG);
	v = GEM_MAC_XIF_LINK_LED;
	v |= GEM_MAC_XIF_TX_MII_ENA;

	/* If an external transceiver is connected, enable its MII drivers */
	sc->sc_mif_config = bus_space_read_4(t, mac, GEM_MIF_CONFIG);
	if ((sc->sc_mif_config & GEM_MIF_CONFIG_MDI1) != 0) {
		/* External MII needs echo disable if half duplex. */
		if ((IFM_OPTIONS(sc->sc_mii->mii_media_active) & IFM_FDX) != 0)
			/* turn on full duplex LED */
			v |= GEM_MAC_XIF_FDPLX_LED;
		else
	 		/* half duplex -- disable echo */
	 		v |= GEM_MAC_XIF_ECHO_DISABL;

		if (IFM_SUBTYPE(sc->sc_mii->mii_media_active) == IFM_1000_T)
			v |= GEM_MAC_XIF_GMII_MODE;
		else
			v &= ~GEM_MAC_XIF_GMII_MODE;
	} else {
		/* Internal MII needs buf enable */
		v |= GEM_MAC_XIF_MII_BUF_ENA;
	}
	bus_space_write_4(t, mac, GEM_MAC_XIF_CONFIG, v);
}

int
gem_mediachange(ifp)
	struct ifnet *ifp;
{
	struct gem_softc *sc = ifp->if_softc;

	/* XXX Add support for serial media. */

	return (mii_mediachg(sc->sc_mii));
}

void
gem_mediastatus(ifp, ifmr)
	struct ifnet *ifp;
	struct ifmediareq *ifmr;
{
	struct gem_softc *sc = ifp->if_softc;

	if ((ifp->if_flags & IFF_UP) == 0)
		return;

	mii_pollstat(sc->sc_mii);
	ifmr->ifm_active = sc->sc_mii->mii_media_active;
	ifmr->ifm_status = sc->sc_mii->mii_media_status;
}

/*
 * Process an ioctl request.
 */
static int
gem_ioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct gem_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	switch (cmd) {
	case SIOCSIFADDR:
	case SIOCGIFADDR:
	case SIOCSIFMTU:
		error = ether_ioctl(ifp, cmd, data);
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if ((sc->sc_ifflags ^ ifp->if_flags) == IFF_PROMISC)
				gem_setladrf(sc);
			else
				gem_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				gem_stop(ifp, 0);
		}
		sc->sc_ifflags = ifp->if_flags;
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		gem_setladrf(sc);
		error = 0;
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_mii->mii_media, cmd);
		break;
	default:
		error = ENOTTY;
		break;
	}

	/* Try to get things going again */
	if (ifp->if_flags & IFF_UP)
		gem_start(ifp);
	splx(s);
	return (error);
}

/*
 * Set up the logical address filter.
 */
static void
gem_setladrf(sc)
	struct gem_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct ifmultiaddr *inm;
	struct sockaddr_dl *sdl;
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t h = sc->sc_h;
	u_char *cp;
	u_int32_t crc;
	u_int32_t hash[16];
	u_int32_t v;
	int len;
	int i;

	/* Get current RX configuration */
	v = bus_space_read_4(t, h, GEM_MAC_RX_CONFIG);

	/*
	 * Turn off promiscuous mode, promiscuous group mode (all multicast),
	 * and hash filter.  Depending on the case, the right bit will be
	 * enabled.
	 */
	v &= ~(GEM_MAC_RX_PROMISCUOUS|GEM_MAC_RX_HASH_FILTER|
	    GEM_MAC_RX_PROMISC_GRP);

	if ((ifp->if_flags & IFF_PROMISC) != 0) {
		/* Turn on promiscuous mode */
		v |= GEM_MAC_RX_PROMISCUOUS;
		goto chipit;
	}
	if ((ifp->if_flags & IFF_ALLMULTI) != 0) {
		hash[3] = hash[2] = hash[1] = hash[0] = 0xffff;
		ifp->if_flags |= IFF_ALLMULTI;
		v |= GEM_MAC_RX_PROMISC_GRP;
		goto chipit;
	}

	/*
	 * Set up multicast address filter by passing all multicast addresses
	 * through a crc generator, and then using the high order 8 bits as an
	 * index into the 256 bit logical address filter.  The high order 4
	 * bits selects the word, while the other 4 bits select the bit within
	 * the word (where bit 0 is the MSB).
	 */

	/* Clear hash table */
	memset(hash, 0, sizeof(hash));

	TAILQ_FOREACH(inm, &sc->sc_arpcom.ac_if.if_multiaddrs, ifma_link) {
		if (inm->ifma_addr->sa_family != AF_LINK)
			continue;
		sdl = (struct sockaddr_dl *)inm->ifma_addr;
		cp = LLADDR(sdl);
		crc = 0xffffffff;
		for (len = sdl->sdl_alen; --len >= 0;) {
			int octet = *cp++;
			int i;

#define MC_POLY_LE	0xedb88320UL	/* mcast crc, little endian */
			for (i = 0; i < 8; i++) {
				if ((crc & 1) ^ (octet & 1)) {
					crc >>= 1;
					crc ^= MC_POLY_LE;
				} else {
					crc >>= 1;
				}
				octet >>= 1;
			}
		}
		/* Just want the 8 most significant bits. */
		crc >>= 24;

		/* Set the corresponding bit in the filter. */
		hash[crc >> 4] |= 1 << (15 - (crc & 15));
	}

	v |= GEM_MAC_RX_HASH_FILTER;
	ifp->if_flags &= ~IFF_ALLMULTI;

	/* Now load the hash table into the chip (if we are using it) */
	for (i = 0; i < 16; i++) {
		bus_space_write_4(t, h,
		    GEM_MAC_HASH0 + i * (GEM_MAC_HASH1-GEM_MAC_HASH0),
		    hash[i]);
	}

chipit:
	bus_space_write_4(t, h, GEM_MAC_RX_CONFIG, v);
}
