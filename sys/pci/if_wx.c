/* $FreeBSD$ */
/*
 * Principal Author: Matthew Jacob <mjacob@feral.com>
 * Copyright (c) 1999, 2001 by Traakan Software
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 *
 * Additional Copyright (c) 2001 by Parag Patel
 * under same licence for MII PHY code.
 */

/*
 * Intel Gigabit Ethernet (82452/82453) Driver.
 * Inspired by fxp driver by David Greenman for FreeBSD, and by
 * Bill Paul's work in other FreeBSD network drivers.
 */

/*
 * Many bug fixes gratefully acknowledged from:
 *
 *	The folks at Sitara Networks
 */

/*
 * Options
 */

/*
 * Use only every other 16 byte receive descriptor, leaving the ones
 * in between empty. This card is most efficient at reading/writing
 * 32 byte cache lines, so avoid all the (not working for early rev
 * cards) MWI and/or READ/MODIFY/WRITE cycles updating one descriptor
 * would have you do.
 *
 * This isn't debugged yet.
 */
/* #define	PADDED_CELL	1 */

/*
 * Since the includes are a mess, they'll all be in if_wxvar.h
 */

#include <pci/if_wxvar.h>

#ifdef __alpha__
#undef vtophys
#define	vtophys(va)	alpha_XXX_dmamap((vm_offset_t)(va))
#endif /* __alpha__ */

/*
 * Function Prototpes, yadda yadda...
 */

static int wx_intr(void *);
static void wx_handle_link_intr(wx_softc_t *);
static void wx_check_link(wx_softc_t *);
static void wx_handle_rxint(wx_softc_t *);
static void wx_gc(wx_softc_t *);
static void wx_start(struct ifnet *);
static int wx_ioctl(struct ifnet *, IOCTL_CMD_TYPE, caddr_t);
static int wx_ifmedia_upd(struct ifnet *);
static void wx_ifmedia_sts(struct ifnet *, struct ifmediareq *);
static int wx_init(void *);
static void wx_hw_stop(wx_softc_t *);
static void wx_set_addr(wx_softc_t *, int, u_int8_t *);
static int wx_hw_initialize(wx_softc_t *);
static void wx_stop(wx_softc_t *);
static void wx_txwatchdog(struct ifnet *);
static int wx_get_rbuf(wx_softc_t *, rxpkt_t *);
static void wx_rxdma_map(wx_softc_t *, rxpkt_t *, struct mbuf *);

static INLINE void wx_eeprom_raise_clk(wx_softc_t *, u_int32_t);
static INLINE void wx_eeprom_lower_clk(wx_softc_t *, u_int32_t);
static INLINE void wx_eeprom_sobits(wx_softc_t *, u_int16_t, u_int16_t);
static INLINE u_int16_t wx_eeprom_sibits(wx_softc_t *);
static INLINE void wx_eeprom_cleanup(wx_softc_t *);
static INLINE u_int16_t wx_read_eeprom_word(wx_softc_t *, int);
static void wx_read_eeprom(wx_softc_t *, u_int16_t *, int, int);

static int wx_attach_common(wx_softc_t *);
static void wx_watchdog(void *);

static INLINE void wx_mwi_whackon(wx_softc_t *);
static INLINE void wx_mwi_unwhack(wx_softc_t *);
static int wx_dring_setup(wx_softc_t *);
static void wx_dring_teardown(wx_softc_t *);

static int wx_attach_phy(wx_softc_t *);
static int wx_miibus_readreg(void *, int, int);
static int wx_miibus_writereg(void *, int, int, int);
static void wx_miibus_statchg(void *);
static void wx_miibus_mediainit(void *);

static u_int32_t wx_mii_shift_in(wx_softc_t *);
static void wx_mii_shift_out(wx_softc_t *, u_int32_t, u_int32_t);

#define	WX_DISABLE_INT(sc)	WRITE_CSR(sc, WXREG_IMCLR, WXDISABLE)
#define	WX_ENABLE_INT(sc)	WRITE_CSR(sc, WXREG_IMASK, sc->wx_ienable)

/*
 * Until we do a bit more work, we can get no bigger than MCLBYTES
 */
#if	0
#define	WX_MAXMTU	(WX_MAX_PKT_SIZE_JUMBO - sizeof (struct ether_header))
#else
#define	WX_MAXMTU	(MCLBYTES - sizeof (struct ether_header))
#endif

#define	DPRINTF(sc, x)	if (sc->wx_debug) printf x
#define	IPRINTF(sc, x)	if (sc->wx_verbose) printf x

static const char ldn[] = "%s: link down\n";
static const char lup[] = "%s: link up\n";
static const char sqe[] = "%s: receive sequence error\n";
static const char ane[] = "%s: /C/ ordered sets seen- enabling ANE\n";
static const char inane[] = "%s: no /C/ ordered sets seen- disabling ANE\n";


/*
 * Program multicast addresses.
 *
 * This function must be called at splimp, but it may sleep.
 */
static int
wx_mc_setup(wx_softc_t *sc)
{
	struct ifnet *ifp = &sc->wx_if;
	struct ifmultiaddr *ifma;

	/*
	 * XXX: drain TX queue
	 */
	if (sc->tactive) {
		return (EBUSY);
	}

	wx_stop(sc);

	if ((ifp->if_flags & IFF_ALLMULTI) || (ifp->if_flags & IFF_PROMISC)) {
		sc->all_mcasts = 1;
		return (wx_init(sc));
	}

	sc->wx_nmca = 0;
	for (ifma = ifp->if_multiaddrs.lh_first, sc->wx_nmca = 0;
	    ifma != NULL; ifma = ifma->ifma_link.le_next) {

		if (ifma->ifma_addr->sa_family != AF_LINK) {
			continue;
		}
		if (sc->wx_nmca >= WX_RAL_TAB_SIZE-1) {
			sc->wx_nmca = 0;
			sc->all_mcasts = 1;
			break;
		}
		bcopy(LLADDR((struct sockaddr_dl *)ifma->ifma_addr),
		    (void *) &sc->wx_mcaddr[sc->wx_nmca++][0], 6);
	}
	return (wx_init(sc));
}

/*
 * Return identification string if this is device is ours.
 */
static int
wx_probe(device_t dev)
{
	if (pci_get_vendor(dev) != WX_VENDOR_INTEL) {
		return (ENXIO);
	}
	switch (pci_get_device(dev)) {
	case WX_PRODUCT_82452:
		device_set_desc(dev, "Intel PRO/1000 Gigabit (WISEMAN)");
		break;
	case WX_PRODUCT_LIVENGOOD:
		device_set_desc(dev, "Intel PRO/1000 (LIVENGOOD)");
		break;
	case WX_PRODUCT_82452_SC:
		device_set_desc(dev, "Intel PRO/1000 F Gigabit Ethernet");
		break;
	case WX_PRODUCT_82543:
		device_set_desc(dev, "Intel PRO/1000 T Gigabit Ethernet");
		break;
	default:
		return (ENXIO);
	}
	return (0);
}

static int
wx_attach(device_t dev)
{
	int error = 0;
	wx_softc_t *sc = device_get_softc(dev);
	struct ifnet *ifp;
	u_int32_t val;
	int rid;

	bzero(sc, sizeof (wx_softc_t));

	callout_handle_init(&sc->w.sch);
	sc->w.dev = dev;

	if (bootverbose)
		sc->wx_verbose = 1;

	if (getenv_int ("wx_debug", &rid)) {
		if (rid & (1 << device_get_unit(dev))) {
			sc->wx_debug = 1;
		}
	}

	if (getenv_int("wx_no_ilos", &rid)) {
		if (rid & (1 << device_get_unit(dev))) {
			sc->wx_no_ilos = 1;
		}
	}

	if (getenv_int("wx_ilos", &rid)) {
		if (rid & (1 << device_get_unit(dev))) {
			sc->wx_ilos = 1;
		}
	}

	if (getenv_int("wx_no_flow", &rid)) {
		if (rid & (1 << device_get_unit(dev))) {
			sc->wx_no_flow = 1;
		}
	}

#ifdef	SMPNG
	mtx_init(&sc->wx_mtx, device_get_nameunit(dev), MTX_DEF | MTX_RECURSE);
#endif
	WX_LOCK(sc);
	/*
 	 * get revision && id...
	 */
	sc->wx_idnrev = (pci_get_device(dev) << 16) | (pci_get_revid(dev));

	/*
	 * Enable bus mastering, make sure that the cache line size is right.
	 */
	pci_enable_busmaster(dev);
	pci_enable_io(dev, SYS_RES_MEMORY);
	val = pci_read_config(dev, PCIR_COMMAND, 4);
	if ((val & PCIM_CMD_MEMEN) == 0) {
		device_printf(dev, "failed to enable memory mapping\n");
		error = ENXIO;
		goto out;
	}

	/*
	 * Let the BIOS do it's job- but check for sanity.
	 */
	val = pci_read_config(dev, PCIR_CACHELNSZ, 1);
	if (val < 4 || val > 32) {
		pci_write_config(dev, PCIR_CACHELNSZ, 8, 1);
	}

	/*
	 * Map control/status registers.
	 */
	rid = WX_MMBA;
	sc->w.mem = bus_alloc_resource(dev, SYS_RES_MEMORY,
	    &rid, 0, ~0, 1, RF_ACTIVE);
	if (!sc->w.mem) {
		device_printf(dev, "could not map memory\n");
		error = ENXIO;
		goto out;
        }
	sc->w.st = rman_get_bustag(sc->w.mem);
	sc->w.sh = rman_get_bushandle(sc->w.mem);

	rid = 0;
	sc->w.irq = bus_alloc_resource(dev, SYS_RES_IRQ,
	    &rid, 0, ~0, 1, RF_SHAREABLE | RF_ACTIVE);
	if (sc->w.irq == NULL) {
		device_printf(dev, "could not map interrupt\n");
		error = ENXIO;
		goto out;
	}
	error = bus_setup_intr(dev, sc->w.irq, INTR_TYPE_NET,
	    (void (*)(void *))wx_intr, sc, &sc->w.ih);
	if (error) {
		device_printf(dev, "could not setup irq\n");
		goto out;
	}
	(void) snprintf(sc->wx_name, sizeof (sc->wx_name) - 1, "wx%d",
	    device_get_unit(dev));
	if (wx_attach_common(sc)) {
		bus_teardown_intr(dev, sc->w.irq, sc->w.ih);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->w.irq);
		bus_release_resource(dev, SYS_RES_MEMORY, WX_MMBA, sc->w.mem);
		error = ENXIO;
		goto out;
	}
	device_printf(dev, "Ethernet address %02x:%02x:%02x:%02x:%02x:%02x\n",
	    sc->w.arpcom.ac_enaddr[0], sc->w.arpcom.ac_enaddr[1],
	    sc->w.arpcom.ac_enaddr[2], sc->w.arpcom.ac_enaddr[3],
	    sc->w.arpcom.ac_enaddr[4], sc->w.arpcom.ac_enaddr[5]);

	ifp = &sc->w.arpcom.ac_if;
	ifp->if_unit = device_get_unit(dev);
	ifp->if_name = "wx";
	ifp->if_mtu = ETHERMTU; /* we always start at ETHERMTU size */
	ifp->if_output = ether_output;
	ifp->if_baudrate = 1000000000;
	ifp->if_init = (void (*)(void *))wx_init;
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = wx_ioctl;
	ifp->if_start = wx_start;
	ifp->if_watchdog = wx_txwatchdog;
	ifp->if_snd.ifq_maxlen = WX_MAX_TDESC - 1;
	ether_ifattach(ifp, ETHER_BPF_SUPPORTED);
out:
	WX_UNLOCK(sc);
	return (error);
}

static int
wx_attach_phy(wx_softc_t *sc)
{
	if (mii_phy_probe(sc->w.dev, &sc->w.miibus, wx_ifmedia_upd,
	    wx_ifmedia_sts)) {
		printf("%s: no PHY probed!\n", sc->wx_name);
		return (-1);
	}
	sc->wx_mii = 1;
	return 0;
}

static int
wx_detach(device_t dev)
{
	wx_softc_t *sc = device_get_softc(dev);

	WX_LOCK(sc);
	ether_ifdetach(&sc->w.arpcom.ac_if, ETHER_BPF_SUPPORTED);
	wx_stop(sc);

	bus_generic_detach(dev);
	if (sc->w.miibus) {
		bus_generic_detach(dev);
		device_delete_child(dev, sc->w.miibus);
	} else {
		ifmedia_removeall(&sc->wx_media);
	}

	bus_teardown_intr(dev, sc->w.irq, sc->w.ih);
	bus_release_resource(dev, SYS_RES_IRQ, 0, sc->w.irq);
	bus_release_resource(dev, SYS_RES_MEMORY, WX_MMBA, sc->w.mem);
	WX_UNLOCK(sc);
	return (0);
}

static int
wx_shutdown(device_t dev)
{
	wx_hw_stop((wx_softc_t *) device_get_softc(dev));
	return (0);
}

static INLINE void
wx_mwi_whackon(wx_softc_t *sc)
{
	sc->wx_cmdw = pci_read_config(sc->w.dev, PCIR_COMMAND, 2);
	pci_write_config(sc->w.dev, PCIR_COMMAND, sc->wx_cmdw & ~MWI, 2);
}

static INLINE void
wx_mwi_unwhack(wx_softc_t *sc)
{
	if (sc->wx_cmdw & MWI) {
		pci_write_config(sc->w.dev, PCIR_COMMAND, sc->wx_cmdw, 2);
	}
}

static int
wx_dring_setup(wx_softc_t *sc)
{
	size_t len;

	len = sizeof (wxrd_t) * WX_MAX_RDESC;
	sc->rdescriptors = (wxrd_t *)
	    contigmalloc(len, M_DEVBUF, M_NOWAIT, 0, ~0, 4096, 0);
	if (sc->rdescriptors == NULL) {
		printf("%s: could not allocate rcv descriptors\n", sc->wx_name);
		return (-1);
	}
	if (((intptr_t)sc->rdescriptors) & 0xfff) {
		contigfree(sc->rdescriptors, len, M_DEVBUF);
		sc->rdescriptors = NULL;
		printf("%s: rcv descriptors not 4KB aligned\n", sc->wx_name);
		return (-1);
	}
        bzero(sc->rdescriptors, len);

	len = sizeof (wxtd_t) * WX_MAX_TDESC;
	sc->tdescriptors = (wxtd_t *)
	    contigmalloc(len, M_DEVBUF, M_NOWAIT, 0, ~0, 4096, 0);
	if (sc->tdescriptors == NULL) {
		contigfree(sc->rdescriptors,
		    sizeof (wxrd_t) * WX_MAX_RDESC, M_DEVBUF);
		sc->rdescriptors = NULL;
		printf("%s: could not allocate xmt descriptors\n", sc->wx_name);
		return (-1);
	}
	if (((intptr_t)sc->tdescriptors) & 0xfff) {
		contigfree(sc->rdescriptors,
		    sizeof (wxrd_t) * WX_MAX_RDESC, M_DEVBUF);
		sc->rdescriptors = NULL;
		printf("%s: xmt descriptors not 4KB aligned\n", sc->wx_name);
		return (-1);
	}
        bzero(sc->tdescriptors, len);
	return (0);
}

static void
wx_dring_teardown(wx_softc_t *sc)
{
	if (sc->rdescriptors) {
		contigfree(sc->rdescriptors,
		    sizeof (wxrd_t) * WX_MAX_RDESC, M_DEVBUF);
		sc->rdescriptors = NULL;
	}
	if (sc->tdescriptors) {
		contigfree(sc->tdescriptors,
		    sizeof (wxtd_t) * WX_MAX_TDESC, M_DEVBUF);
		sc->tdescriptors = NULL;
	}
}

static device_method_t wx_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		wx_probe),
	DEVMETHOD(device_attach,	wx_attach),
	DEVMETHOD(device_detach,	wx_detach),
	DEVMETHOD(device_shutdown,	wx_shutdown),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	wx_miibus_readreg),
	DEVMETHOD(miibus_writereg,	wx_miibus_writereg),
	DEVMETHOD(miibus_statchg,	wx_miibus_statchg),
	DEVMETHOD(miibus_mediainit,	wx_miibus_mediainit),

	{ 0, 0 }
};

static driver_t wx_driver = {
	"wx", wx_methods, sizeof(wx_softc_t),
};
static devclass_t wx_devclass;
DRIVER_MODULE(if_wx, pci, wx_driver, wx_devclass, 0, 0);
DRIVER_MODULE(miibus, wx, miibus_driver, miibus_devclass, 0, 0);

/*
 * Do generic parts of attach. Our registers have been mapped
 * and our interrupt registered.
 */
static int
wx_attach_common(wx_softc_t *sc)
{
	size_t len;
	u_int32_t tmp;
	int ll = 0;

	/*
	 * First, check for revision support.
	 */
	if (sc->wx_idnrev < WX_WISEMAN_2_0) {
		printf("%s: cannot support ID 0x%x, revision %d chips\n",
		    sc->wx_name, sc->wx_idnrev >> 16, sc->wx_idnrev & 0xffff);
		return (ENXIO);
	}

	/*
	 * Second, reset the chip.
	 */
	wx_hw_stop(sc);

	/*
	 * Third, validate our EEPROM.
	 */

	/* TBD */

	/*
	 * Fourth, read eeprom for our MAC address and other things.
	 */
	wx_read_eeprom(sc, (u_int16_t *)sc->wx_enaddr, WX_EEPROM_MAC_OFF, 3);

	/*
	 * Fifth, establish some adapter parameters.
	 */
	sc->wx_txint_delay = 128;
	sc->wx_dcr = 0;

	if (IS_LIVENGOOD_CU(sc)) {

		/* settings to talk to PHY */
		sc->wx_dcr |= WXDCR_FRCSPD | WXDCR_FRCDPX | WXDCR_SLU;
		WRITE_CSR(sc, WXREG_DCR, sc->wx_dcr);

		/*
		 * Raise the PHY's reset line to make it operational.
		 */
		tmp = READ_CSR(sc, WXREG_EXCT);
		tmp |= WXPHY_RESET_DIR4;
		WRITE_CSR(sc, WXREG_EXCT, tmp);
		DELAY(20*1000);

		tmp = READ_CSR(sc, WXREG_EXCT);
		tmp &= ~WXPHY_RESET4;
		WRITE_CSR(sc, WXREG_EXCT, tmp);
		DELAY(20*1000);

		tmp = READ_CSR(sc, WXREG_EXCT);
		tmp |= WXPHY_RESET4;
		WRITE_CSR(sc, WXREG_EXCT, tmp);
		DELAY(20*1000);

		if (wx_attach_phy(sc)) {
			goto fail;
		}
	} else {
		ifmedia_init(&sc->wx_media, IFM_IMASK,
		    wx_ifmedia_upd, wx_ifmedia_sts);

		ifmedia_add(&sc->wx_media, IFM_ETHER|IFM_1000_SX, 0, NULL);
		ifmedia_add(&sc->wx_media,
		    IFM_ETHER|IFM_1000_SX|IFM_FDX, 0, NULL);
		ifmedia_set(&sc->wx_media, IFM_ETHER|IFM_1000_SX|IFM_FDX);

		sc->wx_media.ifm_media = sc->wx_media.ifm_cur->ifm_media;
	}

	/*
	 * Sixth, establish a default device control register word.
	 */
	ll += 1;
	if (sc->wx_cfg1 & WX_EEPROM_CTLR1_FD)
		sc->wx_dcr |= WXDCR_FD;
	if (sc->wx_cfg1 & WX_EEPROM_CTLR1_ILOS)
		sc->wx_dcr |= WXDCR_ILOS;

	tmp = (sc->wx_cfg1 >> WX_EEPROM_CTLR1_SWDPIO_SHIFT) & WXDCR_SWDPIO_MASK;
	sc->wx_dcr |= (tmp << WXDCR_SWDPIO_SHIFT);

	if (sc->wx_no_ilos)
		sc->wx_dcr &= ~WXDCR_ILOS;
	if (sc->wx_ilos)
		sc->wx_dcr |= WXDCR_ILOS;
	if (sc->wx_no_flow == 0)
		sc->wx_dcr |= WXDCR_RFCE | WXDCR_TFCE;

	/*
	 * Seventh, allocate various sw structures...
	 */
	len = sizeof (rxpkt_t) * WX_MAX_RDESC;
	sc->rbase = (rxpkt_t *) WXMALLOC(len);
	if (sc->rbase == NULL) {
                goto fail;
	}
        bzero(sc->rbase, len);
	ll += 1;

	len = sizeof (txpkt_t) * WX_MAX_TDESC;
	sc->tbase = (txpkt_t *) WXMALLOC(len);
        if (sc->tbase == NULL) {
                goto fail;
	}
        bzero(sc->tbase, len);
	ll += 1;

	/*
	 * Eighth, allocate and dma map (platform dependent) descriptor rings.
	 * They have to be aligned on a 4KB boundary.
	 */
	if (wx_dring_setup(sc) == 0) {
		return (0);
	}

fail:
	printf("%s: failed to do common attach (%d)\n", sc->wx_name, ll);
	wx_dring_teardown(sc);
	if (sc->rbase) {
		WXFREE(sc->rbase);
		sc->rbase = NULL;
	}
	if (sc->tbase) {
		WXFREE(sc->tbase);
		sc->tbase = NULL;
	}
	return (ENOMEM);
}

/*
 * EEPROM functions.
 */

static INLINE void
wx_eeprom_raise_clk(wx_softc_t *sc, u_int32_t regval)
{
	WRITE_CSR(sc, WXREG_EECDR, regval | WXEECD_SK);
	DELAY(50);
}

static INLINE void
wx_eeprom_lower_clk(wx_softc_t *sc, u_int32_t regval)
{
	WRITE_CSR(sc, WXREG_EECDR, regval & ~WXEECD_SK);
	DELAY(50);
}

static INLINE void
wx_eeprom_sobits(wx_softc_t *sc, u_int16_t data, u_int16_t count)
{
	u_int32_t regval, mask;

	mask = 1 << (count - 1);
	regval = READ_CSR(sc, WXREG_EECDR) & ~(WXEECD_DI|WXEECD_DO);

	do {
		if (data & mask)
			regval |= WXEECD_DI;
		else
			regval &= ~WXEECD_DI;
		WRITE_CSR(sc, WXREG_EECDR, regval); DELAY(50);
		wx_eeprom_raise_clk(sc, regval);
		wx_eeprom_lower_clk(sc, regval);
		mask >>= 1;
	} while (mask != 0);
	WRITE_CSR(sc, WXREG_EECDR, regval & ~WXEECD_DI);
}

static INLINE u_int16_t
wx_eeprom_sibits(wx_softc_t *sc)
{
	unsigned int regval, i;
	u_int16_t data;

	data = 0;
	regval = READ_CSR(sc, WXREG_EECDR) & ~(WXEECD_DI|WXEECD_DO);
	for (i = 0; i != 16; i++) {
		data <<= 1;
		wx_eeprom_raise_clk(sc, regval);
		regval = READ_CSR(sc, WXREG_EECDR) & ~WXEECD_DI;
		if (regval & WXEECD_DO) {
			data |= 1;
		}
		wx_eeprom_lower_clk(sc, regval);
	}
	return (data);
}

static INLINE void
wx_eeprom_cleanup(wx_softc_t *sc)
{
	u_int32_t regval;
	regval = READ_CSR(sc, WXREG_EECDR) & ~(WXEECD_DI|WXEECD_CS);
	WRITE_CSR(sc, WXREG_EECDR, regval); DELAY(50);
	wx_eeprom_raise_clk(sc, regval);
	wx_eeprom_lower_clk(sc, regval);
}

static u_int16_t INLINE 
wx_read_eeprom_word(wx_softc_t *sc, int offset)
{
	u_int16_t       data;
	WRITE_CSR(sc, WXREG_EECDR, WXEECD_CS);
	wx_eeprom_sobits(sc, EEPROM_READ_OPCODE, 3);
	wx_eeprom_sobits(sc, offset, 6);
	data = wx_eeprom_sibits(sc);
	wx_eeprom_cleanup(sc);
	return (data);
}

static void
wx_read_eeprom(wx_softc_t *sc, u_int16_t *data, int offset, int words)
{
	int i;
	for (i = 0; i < words; i++) {
		*data++ = wx_read_eeprom_word(sc, offset++);
	}
	sc->wx_cfg1 = wx_read_eeprom_word(sc, WX_EEPROM_CTLR1_OFF);
}

/*
 * Start packet transmission on the interface.
 */

static void
wx_start(struct ifnet *ifp)
{
	wx_softc_t *sc = SOFTC_IFP(ifp);
	u_int16_t widx = WX_MAX_TDESC, cidx, nactv;

	WX_LOCK(sc);
	DPRINTF(sc, ("%s: wx_start\n", sc->wx_name));
	nactv = sc->tactive;
	while (nactv < WX_MAX_TDESC - 1) {
		int ndesc;
		int gctried = 0;
		struct mbuf *m, *mb_head;

		IF_DEQUEUE(&ifp->if_snd, mb_head);
		if (mb_head == NULL) {
			break;
		}
		sc->wx_xmitwanted++;

		/*
		 * If we have a packet less than ethermin, pad it out.
		 */
		if (mb_head->m_pkthdr.len < WX_MIN_RPKT_SIZE) {
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (m == NULL) {
				m_freem(mb_head);
				break;
			}
			m_copydata(mb_head, 0, mb_head->m_pkthdr.len,
			    mtod(m, caddr_t));
			m->m_pkthdr.len = m->m_len = WX_MIN_RPKT_SIZE;
			bzero(mtod(m, char *) + mb_head->m_pkthdr.len,
			     WX_MIN_RPKT_SIZE - mb_head->m_pkthdr.len);
			sc->wx_xmitpullup++;
			m_freem(mb_head);
			mb_head = m;
		}
	again:
		cidx = sc->tnxtfree;
		nactv = sc->tactive;


		/*
		 * Go through each of the mbufs in the chain and initialize
		 * the transmit buffer descriptors with the physical address
		 * and size of that mbuf. If we have a length less than our
		 * minimum transmit size, we bail (to do a pullup). If we run
		 * out of descriptors, we also bail and try and do a pullup.
		 */
		for (ndesc = 0, m = mb_head; m != NULL; m = m->m_next) {
			vm_offset_t vptr;
			wxtd_t *td;

			/*
			 * If this mbuf has no data, skip it.
			 */
			if (m->m_len == 0) {
				continue;
			}

			/*
			 * If this packet is too small for the chip's minimum,
			 * break out to cluster it.
			 */
			if (m->m_len < WX_MIN_RPKT_SIZE) {
				sc->wx_xmitrunt++;
				break;
			}

			/*
			 * Do we have a descriptor available for this mbuf?
			 */
			if (++nactv == WX_MAX_TDESC) {
				if (gctried++ == 0) {
					sc->wx_xmitgc++;
					wx_gc(sc);
					goto again;
				}
				break;
			}
			sc->tbase[cidx].dptr = m;
			td = &sc->tdescriptors[cidx];
			td->length = m->m_len;

			vptr = mtod(m, vm_offset_t);
			td->address.highpart = 0;
			td->address.lowpart = vtophys(vptr);

			td->cso = 0;
			td->status = 0;
			td->special = 0;
			td->cmd = 0;
			td->css = 0;

			if (sc->wx_debug) {
				printf("%s: XMIT[%d] %p vptr %lx (length %d "
				    "DMA addr %x) idx %d\n", sc->wx_name,
				    ndesc, m, (long) vptr, td->length,
				    td->address.lowpart, cidx);
			}
			ndesc++;
			cidx = T_NXT_IDX(cidx);
		}

		/*
		 * If we get here and m is NULL, we can send
		 * the the packet chain described by mb_head.
		 */
		if (m == NULL) {
			/*
			 * Mark the last descriptor with EOP and tell the
			 * chip to insert a final checksum.
			 */
			wxtd_t *td = &sc->tdescriptors[T_PREV_IDX(cidx)];
			td->cmd = TXCMD_EOP|TXCMD_IFCS;

			sc->tbase[sc->tnxtfree].sidx = sc->tnxtfree;
			sc->tbase[sc->tnxtfree].eidx = cidx;
			sc->tbase[sc->tnxtfree].next = NULL;
			if (sc->tbsyf) {
				sc->tbsyl->next = &sc->tbase[sc->tnxtfree];
			} else {
				sc->tbsyf = &sc->tbase[sc->tnxtfree];
			}
			sc->tbsyl = &sc->tbase[sc->tnxtfree];
			sc->tnxtfree = cidx;
			sc->tactive = nactv;
			ifp->if_timer = 10;
			if (ifp->if_bpf)
				bpf_mtap(WX_BPFTAP_ARG(ifp), mb_head);
			/* defer xmit until we've got them all */
			widx = cidx;
			continue;
		}

		/*
		 * Otherwise, we couldn't send this packet for some reason.
		 *
		 * If don't have a descriptor available, and this is a
		 * single mbuf packet, freeze output so that later we
		 * can restart when we have more room. Otherwise, we'll
		 * try and cluster the request. We've already tried to
		 * garbage collect completed descriptors.
		 */
		if (nactv == WX_MAX_TDESC && mb_head->m_next == NULL) {
			sc->wx_xmitputback++;
			ifp->if_flags |= IFF_OACTIVE;
			IF_PREPEND(&ifp->if_snd, mb_head);
			break;
		}

		/*
		 * Otherwise, it's either a fragment length somewhere in the
		 * chain that isn't at least WX_MIN_XPKT_SIZE in length or
		 * the number of fragments exceeds the number of descriptors
		 * available.
		 *
		 * We could try a variety of strategies here- if this is
		 * a length problem for single mbuf packet or a length problem
		 * for the last mbuf in a chain (we could just try and adjust
		 * it), but it's just simpler to try and cluster it.
		 */
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL) {
			m_freem(mb_head);
			break;
		}
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_freem(m);
			m_freem(mb_head);
			break;
		}
		m_copydata(mb_head, 0, mb_head->m_pkthdr.len, mtod(m, caddr_t));
		m->m_pkthdr.len = m->m_len = mb_head->m_pkthdr.len;
		m_freem(mb_head);
		mb_head = m;
		sc->wx_xmitcluster++;
		goto again;
	}

	if (widx < WX_MAX_TDESC) {
		if (IS_WISEMAN(sc)) {
			WRITE_CSR(sc, WXREG_TDT, widx);
		} else {
			WRITE_CSR(sc, WXREG_TDT_LIVENGOOD, widx);
		}
	}

	if (sc->tactive == WX_MAX_TDESC - 1) {
		sc->wx_xmitblocked++;
		ifp->if_flags |= IFF_OACTIVE;
	}

	/* used SW LED to indicate transmission active */
	if (sc->tactive > 0 && sc->wx_mii) {
		WRITE_CSR(sc, WXREG_DCR,
		    READ_CSR(sc, WXREG_DCR) | (WXDCR_SWDPIO0|WXDCR_SWDPIN0));
	}
	WX_UNLOCK(sc);
}

/*
 * Process interface interrupts.
 */
static int
wx_intr(void *arg)
{
	wx_softc_t *sc = arg;
	int claimed = 0;

	WX_ILOCK(sc);
	/*
	 * Read interrupt cause register. Reading it clears bits.
	 */
	sc->wx_icr = READ_CSR(sc, WXREG_ICR);
	if (sc->wx_icr) {
		claimed++;
		WX_DISABLE_INT(sc);
		sc->wx_intr++;
		if (sc->wx_icr & (WXISR_LSC|WXISR_RXSEQ|WXISR_GPI_EN1)) {
			wx_handle_link_intr(sc);
		}
		wx_handle_rxint(sc);
		if (sc->wx_icr & WXISR_TXQE) {
			wx_gc(sc);
		}
		if (sc->wx_if.if_snd.ifq_head != NULL) {
			wx_start(&sc->wx_if);
		}
		WX_ENABLE_INT(sc);
	}
	WX_IUNLK(sc);
	return (claimed);
}

static void
wx_handle_link_intr(wx_softc_t *sc)
{
	u_int32_t txcw, rxcw, dcr, dsr;

	sc->wx_linkintr++;

	dcr = READ_CSR(sc, WXREG_DCR);
	DPRINTF(sc, ("%s: handle_link_intr: icr=%#x dcr=%#x\n",
	    sc->wx_name, sc->wx_icr, dcr));
	if (sc->wx_mii) {
		mii_data_t *mii = WX_MII_FROM_SOFTC(sc);
		mii_pollstat(mii);
		if (mii->mii_media_status & IFM_ACTIVE) {
			if (IFM_SUBTYPE(mii->mii_media_active) == IFM_NONE) {
				IPRINTF(sc, (ldn, sc->wx_name));
				sc->linkup = 0;
			} else {
				IPRINTF(sc, (lup, sc->wx_name));
				sc->linkup = 1;
			}
			WRITE_CSR(sc, WXREG_DCR, sc->wx_dcr);
		} else if (sc->wx_icr & WXISR_RXSEQ) {
			DPRINTF(sc, (sqe, sc->wx_name));
		}
		return;
	}

	txcw = READ_CSR(sc, WXREG_XMIT_CFGW);
	rxcw = READ_CSR(sc, WXREG_RECV_CFGW);
	dsr = READ_CSR(sc, WXREG_DSR);

	/*
	 * If we have LOS or are now receiving Ordered Sets and are not
	 * doing auto-negotiation, restore autonegotiation.
	 */

	if (((dcr & WXDCR_SWDPIN1) || (rxcw & WXRXCW_C)) &&
	    ((txcw & WXTXCW_ANE) == 0)) {
		DPRINTF(sc, (ane, sc->wx_name));
		WRITE_CSR(sc, WXREG_XMIT_CFGW, WXTXCW_DEFAULT);
		sc->wx_dcr &= ~WXDCR_SLU;
		WRITE_CSR(sc, WXREG_DCR, sc->wx_dcr);
		sc->ane_failed = 0;
	}

	if (sc->wx_icr & WXISR_LSC) {
		if (READ_CSR(sc, WXREG_DSR) & WXDSR_LU) {
			IPRINTF(sc, (lup, sc->wx_name));
			sc->linkup = 1;
			sc->wx_dcr |= (WXDCR_SWDPIO0|WXDCR_SWDPIN0);
		} else {
			IPRINTF(sc, (ldn, sc->wx_name));
			sc->linkup = 0;
			sc->wx_dcr &= ~(WXDCR_SWDPIO0|WXDCR_SWDPIN0);
		}
		WRITE_CSR(sc, WXREG_DCR, sc->wx_dcr);
	} else {
		DPRINTF(sc, (sqe, sc->wx_name));
	}
}

static void
wx_check_link(wx_softc_t *sc)
{
	u_int32_t rxcw, dcr, dsr;

	if (sc->wx_mii) {
		mii_pollstat(WX_MII_FROM_SOFTC(sc));
		return;
	}

	rxcw = READ_CSR(sc, WXREG_RECV_CFGW);
	dcr = READ_CSR(sc, WXREG_DCR);
	dsr = READ_CSR(sc, WXREG_DSR);

	if ((dsr & WXDSR_LU) == 0 && (dcr & WXDCR_SWDPIN1) == 0 &&
	    (rxcw & WXRXCW_C) == 0) {
		if (sc->ane_failed == 0) {
			sc->ane_failed = 1;
			return;
		}
		DPRINTF(sc, (inane, sc->wx_name));
		WRITE_CSR(sc, WXREG_XMIT_CFGW, WXTXCW_DEFAULT & ~WXTXCW_ANE);
		if (sc->wx_idnrev < WX_WISEMAN_2_1)
			sc->wx_dcr &= ~WXDCR_TFCE;
		sc->wx_dcr |= WXDCR_SLU;
     		WRITE_CSR(sc, WXREG_DCR, sc->wx_dcr);
	} else if ((rxcw & WXRXCW_C) != 0 && (dcr & WXDCR_SLU) != 0) {
		DPRINTF(sc, (ane, sc->wx_name));
		WRITE_CSR(sc, WXREG_XMIT_CFGW, WXTXCW_DEFAULT);
		sc->wx_dcr &= ~WXDCR_SLU;
		WRITE_CSR(sc, WXREG_DCR, sc->wx_dcr);
	}
}

static void
wx_handle_rxint(wx_softc_t *sc)
{
	struct ether_header *eh;
	struct mbuf *m0, *mb, *pending[WX_MAX_RDESC];
	struct ifnet *ifp = &sc->wx_if;
	int npkts, ndesc, lidx, idx, tlen;

	DPRINTF(sc, ("%s: wx_handle_rxint\n", sc->wx_name));

	for (m0 = sc->rpending, tlen = ndesc = npkts = 0, idx = sc->rnxt,
	    lidx = R_PREV_IDX(idx); ndesc < WX_MAX_RDESC;
	    ndesc++, lidx = idx, idx = R_NXT_IDX(idx)) {
		wxrd_t *rd;
		rxpkt_t *rxpkt;
		int length, offset, lastframe;

		rd = &sc->rdescriptors[idx];
		/*
		 * XXX: DMA Flush descriptor
		 */
		if ((rd->status & RDSTAT_DD) == 0) {
			if (m0) {
				if (sc->rpending == NULL) {
					m0->m_pkthdr.len = tlen;
					sc->rpending = m0;
				} else {
					m_freem(m0);
				}
				m0 = NULL;
			}
			DPRINTF(sc, ("%s: WXRX: ndesc %d idx %d lidx %d\n",
			    sc->wx_name, ndesc, idx, lidx));
			break;
		}

		if (rd->errors != 0) {
			printf("%s: packet with errors (%x)\n",
			    sc->wx_name, rd->errors);
			rd->status = 0;
			ifp->if_ierrors++;
			if (m0) {
				m_freem(m0);
				m0 = NULL;
				if (sc->rpending) {
					m_freem(sc->rpending);
					sc->rpending = NULL;
				}
			}
			continue;
		}


		rxpkt = &sc->rbase[idx];
		mb = rxpkt->dptr;
		if (mb == NULL) {
			printf("%s: receive descriptor with no mbuf\n",
			    sc->wx_name);
			(void) wx_get_rbuf(sc, rxpkt);
			rd->status = 0;
			ifp->if_ierrors++;
			if (m0) {
				m_freem(m0);
				m0 = NULL;
				if (sc->rpending) {
					m_freem(sc->rpending);
					sc->rpending = NULL;
				}
			}
			continue;
		}

		/* XXX: Flush DMA for rxpkt */

		if (wx_get_rbuf(sc, rxpkt)) {
			sc->wx_rxnobuf++;
			wx_rxdma_map(sc, rxpkt, mb);
			ifp->if_ierrors++;
			rd->status = 0;
			if (m0) {
				m_freem(m0);
				m0 = NULL;
				if (sc->rpending) {
					m_freem(sc->rpending);
					sc->rpending = NULL;
				}
			}
			continue;
		}

		/*
		 * Save the completing packet's offset value and length
		 * and install the new one into the descriptor.
		 */
		lastframe = (rd->status & RDSTAT_EOP) != 0;
		length = rd->length;
		offset = rd->address.lowpart & 0xff;
		bzero (rd, sizeof (*rd));
		rd->address.lowpart = rxpkt->dma_addr + WX_RX_OFFSET_VALUE;

		mb->m_len = length;
		mb->m_data += offset;
		mb->m_next = NULL;
		if (m0 == NULL) {
			m0 = mb;
			tlen = length;
		} else if (m0 == sc->rpending) {
			/*
			 * Pick up where we left off before. If
			 * we have an offset (we're assuming the
			 * first frame has an offset), then we've
			 * lost sync somewhere along the line.
			 */
			if (offset) {
				printf("%s: lost sync with partial packet\n",
				    sc->wx_name);
				m_freem(sc->rpending);
				sc->rpending = NULL;
				m0 = mb;
				tlen = length;
			} else {
				sc->rpending = NULL;
				tlen = m0->m_pkthdr.len;
			}
		} else {
			tlen += length;
		}

		DPRINTF(sc, ("%s: RDESC[%d] len %d off %d lastframe %d\n",
		    sc->wx_name, idx, mb->m_len, offset, lastframe));
		if (m0 != mb)
			m_cat(m0, mb);
		if (lastframe == 0) {
			continue;
		}
		m0->m_pkthdr.rcvif = ifp;
		m0->m_pkthdr.len = tlen - WX_CRC_LENGTH;
		mb->m_len -= WX_CRC_LENGTH;

		eh = mtod(m0, struct ether_header *);
		/*
		 * No need to check for promiscous mode since 
		 * the decision to keep or drop the packet is
		 * handled by ether_input()
		 */
		pending[npkts++] = m0;
		m0 = NULL;
		tlen = 0;
	}

	if (ndesc) {
		if (IS_WISEMAN(sc)) {
			WRITE_CSR(sc, WXREG_RDT0, lidx);
		} else {
			WRITE_CSR(sc, WXREG_RDT0_LIVENGOOD, lidx);
		}
		sc->rnxt = idx;
	}

	if (npkts) {
		sc->wx_rxintr++;
	}

	for (idx = 0; idx < npkts; idx++) {
		mb = pending[idx];
                if (ifp->if_bpf) {
                        bpf_mtap(WX_BPFTAP_ARG(ifp), mb);
		}
		ifp->if_ipackets++;
		DPRINTF(sc, ("%s: RECV packet length %d\n",
		    sc->wx_name, mb->m_pkthdr.len));
		eh = mtod(mb, struct ether_header *);
		m_adj(mb, sizeof (struct ether_header));
		ether_input(ifp, eh, mb);
	}
}

static void
wx_gc(wx_softc_t *sc)
{
	struct ifnet *ifp = &sc->wx_if;
	txpkt_t *txpkt;
	u_int32_t tdh;

	WX_LOCK(sc);
	txpkt = sc->tbsyf;
	if (IS_WISEMAN(sc)) {
		tdh = READ_CSR(sc, WXREG_TDH);
	} else {
		tdh = READ_CSR(sc, WXREG_TDH_LIVENGOOD);
	}
	while (txpkt != NULL) {
		u_int32_t end = txpkt->eidx, cidx = tdh;

		/*
		 * Normalize start..end indices to 2 *
	 	 * WX_MAX_TDESC range to eliminate wrap.
		 */
		if (txpkt->eidx < txpkt->sidx) {
			end += WX_MAX_TDESC;
		}

		/*
		 * Normalize current chip index to 2 *
	 	 * WX_MAX_TDESC range to eliminate wrap.
		 */
		if (cidx < txpkt->sidx) {
			cidx += WX_MAX_TDESC;
		}

		/*
		 * If the current chip index is between low and
		 * high indices for this packet, it's not finished
		 * transmitting yet. Because transmits are done FIFO,
		 * this means we're done garbage collecting too.
		 */

		if (txpkt->sidx <= cidx && cidx < txpkt->eidx) {
			DPRINTF(sc, ("%s: TXGC %d..%d TDH %d\n", sc->wx_name,
			    txpkt->sidx, txpkt->eidx, tdh));
			break;
		}
		ifp->if_opackets++;

		if (txpkt->dptr) {
			(void) m_freem(txpkt->dptr);
		} else {
			printf("%s: null mbuf in gc\n", sc->wx_name);
		}

		for (cidx = txpkt->sidx; cidx != txpkt->eidx;
		    cidx = T_NXT_IDX(cidx)) {
			txpkt_t *tmp;
			wxtd_t *td;

			td = &sc->tdescriptors[cidx];
			if (td->status & TXSTS_EC) {
				IPRINTF(sc, ("%s: excess collisions\n",
				    sc->wx_name));
				ifp->if_collisions++;
				ifp->if_oerrors++;
			}
			if (td->status & TXSTS_LC) {
				IPRINTF(sc,
				    ("%s: lost carrier\n", sc->wx_name));
				ifp->if_oerrors++;
			}
			tmp = &sc->tbase[cidx];
			DPRINTF(sc, ("%s: TXGC[%d] %p %d..%d done nact %d "
			    "TDH %d\n", sc->wx_name, cidx, tmp->dptr,
			    txpkt->sidx, txpkt->eidx, sc->tactive, tdh));
			tmp->dptr = NULL;
			if (sc->tactive == 0) {
				printf("%s: nactive < 0?\n", sc->wx_name);
			} else {
				sc->tactive -= 1;
			}
			bzero(td, sizeof (*td));
		}
		sc->tbsyf = txpkt->next;
		txpkt = sc->tbsyf;
	}
	if (sc->tactive < WX_MAX_TDESC - 1) {
		ifp->if_timer = 0;
		ifp->if_flags &= ~IFF_OACTIVE;
	}

	/* used SW LED to indicate transmission not active */
	if (sc->tactive == 0 && sc->wx_mii) {
		WRITE_CSR(sc, WXREG_DCR,
		    READ_CSR(sc, WXREG_DCR) & ~(WXDCR_SWDPIO0|WXDCR_SWDPIN0));
	}
	WX_UNLOCK(sc);
}

/*
 * Periodic timer to update packet in/out/collision statistics,
 * and, more importantly, garbage collect completed transmissions
 * and to handle link status changes.
 */
static void
wx_watchdog(void *arg)
{
	wx_softc_t *sc = arg;

	WX_LOCK(sc);
	wx_gc(sc);
	wx_check_link(sc);
	WX_UNLOCK(sc);

	/*
	 * Schedule another timeout one second from now.
	 */
	VTIMEOUT(sc, wx_watchdog, sc, hz);
}

/*
 * Stop and reinitialize the hardware
 */
static void
wx_hw_stop(wx_softc_t *sc)
{
	u_int32_t icr;
	DPRINTF(sc, ("%s: wx_hw_stop\n", sc->wx_name));
	WX_DISABLE_INT(sc);
	if (sc->wx_idnrev < WX_WISEMAN_2_1) {
		wx_mwi_whackon(sc);
	}
	WRITE_CSR(sc, WXREG_DCR, WXDCR_RST);
	DELAY(20 * 1000);
	icr = READ_CSR(sc, WXREG_ICR);
	if (sc->wx_idnrev < WX_WISEMAN_2_1) {
		wx_mwi_unwhack(sc);
	}
}

static void
wx_set_addr(wx_softc_t *sc, int idx, u_int8_t *mac)
{
	u_int32_t t0, t1;
	DPRINTF(sc, ("%s: wx_set_addr\n", sc->wx_name));
	t0 = (mac[0]) | (mac[1] << 8) | (mac[2] << 16) | (mac[3] << 24);
	t1 = (mac[4] << 0) | (mac[5] << 8);
	t1 |= WX_RAL_AV;
	WRITE_CSR(sc, WXREG_RAL_LO(idx), t0);
	WRITE_CSR(sc, WXREG_RAL_HI(idx), t1);
}

static int
wx_hw_initialize(wx_softc_t *sc)
{
	int i;

	DPRINTF(sc, ("%s: wx_hw_initialize\n", sc->wx_name));

	WRITE_CSR(sc, WXREG_VET, 0);
	for (i = 0; i < (WX_VLAN_TAB_SIZE << 2); i += 4) {
		WRITE_CSR(sc, (WXREG_VFTA + i), 0);
	}
	if (sc->wx_idnrev < WX_WISEMAN_2_1) {
		wx_mwi_whackon(sc);
		WRITE_CSR(sc, WXREG_RCTL, WXRCTL_RST);
		DELAY(5 * 1000);
	}
	/*
	 * Load the first receiver address with our MAC address,
	 * and load as many multicast addresses as can fit into
	 * the receive address array.
	 */
	wx_set_addr(sc, 0, sc->wx_enaddr);
	for (i = 1; i <= sc->wx_nmca; i++) {
		if (i >= WX_RAL_TAB_SIZE) {
			break;
		} else {
			wx_set_addr(sc, i, sc->wx_mcaddr[i-1]);
		}
	}

	while (i < WX_RAL_TAB_SIZE) {
		WRITE_CSR(sc, WXREG_RAL_LO(i), 0);
		WRITE_CSR(sc, WXREG_RAL_HI(i), 0);
		i++;
	}

	if (sc->wx_idnrev < WX_WISEMAN_2_1) {
		WRITE_CSR(sc, WXREG_RCTL, 0);
		DELAY(1 * 1000);
		wx_mwi_unwhack(sc);
	}

	/*
	 * Clear out the hashed multicast table array.
	 */
	for (i = 0; i < WX_MC_TAB_SIZE; i++) {
		WRITE_CSR(sc, WXREG_MTA + (sizeof (u_int32_t) * 4), 0);
	}

	if (IS_LIVENGOOD_CU(sc)) {
		/*
		 * has a PHY - raise its reset line to make it operational
		 */
		u_int32_t tmp = READ_CSR(sc, WXREG_EXCT);
		tmp |= WXPHY_RESET_DIR4;
		WRITE_CSR(sc, WXREG_EXCT, tmp);
		DELAY(20*1000);

		tmp = READ_CSR(sc, WXREG_EXCT);
		tmp &= ~WXPHY_RESET4;
		WRITE_CSR(sc, WXREG_EXCT, tmp);
		DELAY(20*1000);

		tmp = READ_CSR(sc, WXREG_EXCT);
		tmp |= WXPHY_RESET4;
		WRITE_CSR(sc, WXREG_EXCT, tmp);
		DELAY(20*1000);
	} else if (IS_LIVENGOOD(sc)) {
		u_int16_t tew;

		/*
		 * Handle link control
		 */
		WRITE_CSR(sc, WXREG_DCR, sc->wx_dcr | WXDCR_LRST);
		DELAY(50 * 1000);

		wx_read_eeprom(sc, &tew, WX_EEPROM_CTLR2_OFF, 1);
		tew = (tew & WX_EEPROM_CTLR2_SWDPIO) << WX_EEPROM_EXT_SHIFT;
		WRITE_CSR(sc, WXREG_EXCT, (u_int32_t)tew);
	}

	if (sc->wx_dcr & (WXDCR_RFCE|WXDCR_TFCE)) {
		WRITE_CSR(sc, WXREG_FCAL, FC_FRM_CONST_LO);
		WRITE_CSR(sc, WXREG_FCAH, FC_FRM_CONST_HI);
		WRITE_CSR(sc, WXREG_FCT, FC_TYP_CONST);
	} else {
		WRITE_CSR(sc, WXREG_FCAL, 0);
		WRITE_CSR(sc, WXREG_FCAH, 0);
		WRITE_CSR(sc, WXREG_FCT, 0);
	}
	WRITE_CSR(sc, WXREG_FLOW_XTIMER, WX_XTIMER_DFLT);

	if (IS_WISEMAN(sc)) {
		if (sc->wx_idnrev < WX_WISEMAN_2_1) {
			WRITE_CSR(sc, WXREG_FLOW_RCV_HI, 0);
			WRITE_CSR(sc, WXREG_FLOW_RCV_LO, 0);
			sc->wx_dcr &= ~(WXDCR_RFCE|WXDCR_TFCE);
		} else {
			WRITE_CSR(sc, WXREG_FLOW_RCV_HI, WX_RCV_FLOW_HI_DFLT);
			WRITE_CSR(sc, WXREG_FLOW_RCV_LO, WX_RCV_FLOW_LO_DFLT);
		}
	} else {
		WRITE_CSR(sc, WXREG_FLOW_RCV_HI_LIVENGOOD, WX_RCV_FLOW_HI_DFLT);
		WRITE_CSR(sc, WXREG_FLOW_RCV_LO_LIVENGOOD, WX_RCV_FLOW_LO_DFLT);
	}

	if (!IS_LIVENGOOD_CU(sc))
		WRITE_CSR(sc, WXREG_XMIT_CFGW, WXTXCW_DEFAULT);

	WRITE_CSR(sc, WXREG_DCR, sc->wx_dcr);
	DELAY(50 * 1000);

	if (!IS_LIVENGOOD_CU(sc)) {
		/*
		 * The pin stuff is all FM from the Linux driver.
		 */
		if ((READ_CSR(sc, WXREG_DCR) & WXDCR_SWDPIN1) == 0) {
			for (i = 0; i < (WX_LINK_UP_TIMEOUT/10); i++) {
				DELAY(10 * 1000);
				if (READ_CSR(sc, WXREG_DSR) & WXDSR_LU) {
					sc->linkup = 1;
					break;
				}
			}
			if (sc->linkup == 0) {
				sc->ane_failed = 1;
				wx_check_link(sc);
			}
			sc->ane_failed = 0;
		} else {
			printf("%s: SWDPIO1 did not clear- check for reversed "
				"or disconnected cable\n", sc->wx_name);
			/* but return okay anyway */
		}
	}

	sc->wx_ienable = WXIENABLE_DEFAULT;
	return (0);
}

/*
 * Stop the interface. Cancels the statistics updater and resets the interface.
 */
static void
wx_stop(wx_softc_t *sc)
{
	txpkt_t *txp;
	rxpkt_t *rxp;
	struct ifnet *ifp = &sc->wx_if;

	DPRINTF(sc, ("%s: wx_stop\n", sc->wx_name));
	/*
	 * Cancel stats updater.
	 */
	UNTIMEOUT(wx_watchdog, sc, sc);

	/*
	 * Reset the chip
	 */
	wx_hw_stop(sc);

	/*
	 * Release any xmit buffers.
	 */
	for (txp = sc->tbase; txp && txp < &sc->tbase[WX_MAX_TDESC]; txp++) {
		if (txp->dptr) {
			m_free(txp->dptr);
			txp->dptr = NULL;
		}
	}

	/*
	 * Free all the receive buffers.
	 */
	for (rxp = sc->rbase; rxp && rxp < &sc->rbase[WX_MAX_RDESC]; rxp++) {
		if (rxp->dptr) {
			m_free(rxp->dptr);
			rxp->dptr = NULL;
		}
	}

	if (sc->rpending) {
		m_freem(sc->rpending);
		sc->rpending = NULL;
	}

	/*
	 * And we're outta here...
	 */

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;
}

/*
 * Transmit Watchdog
 */
static void
wx_txwatchdog(struct ifnet *ifp)
{
	wx_softc_t *sc = SOFTC_IFP(ifp);
	printf("%s: device timeout\n", sc->wx_name);
	ifp->if_oerrors++;
	if (wx_init(sc)) {
		printf("%s: could not re-init device\n", sc->wx_name);
		VTIMEOUT(sc, (void (*)(void *))wx_init, sc, hz);
	}
}

static int
wx_init(void *xsc)
{
	struct ifmedia *ifm;
	wx_softc_t *sc = xsc;
	struct ifnet *ifp = &sc->wx_if;
	rxpkt_t *rxpkt;
	wxrd_t *rd;
	size_t len;
	int i, bflags;

	DPRINTF(sc, ("%s: wx_init\n", sc->wx_name));
	WX_LOCK(sc);

	/*
	 * Cancel any pending I/O by resetting things.
	 * wx_stop will free any allocated mbufs.
	 */
	wx_stop(sc);

	/*
	 * Reset the hardware. All network addresses loaded here, but
	 * neither the receiver nor the transmitter are enabled.
	 */

	if (wx_hw_initialize(sc)) {
		DPRINTF(sc, ("%s: wx_hw_initialize failed\n", sc->wx_name));
		WX_UNLOCK(sc);
		return (EIO);
	}

	/*
	 * Set up the receive ring stuff.
	 */
	len = sizeof (wxrd_t) * WX_MAX_RDESC;
	bzero(sc->rdescriptors, len);
	for (rxpkt = sc->rbase, i = 0; rxpkt != NULL && i < WX_MAX_RDESC;
	    i += RXINCR, rxpkt++) {
		rd = &sc->rdescriptors[i];
		if (wx_get_rbuf(sc, rxpkt)) {
			break;
		}
		rd->address.lowpart = rxpkt->dma_addr + WX_RX_OFFSET_VALUE;
	}
	if (i != WX_MAX_RDESC) {
		printf("%s: could not set up rbufs\n", sc->wx_name);
		wx_stop(sc);
		WX_UNLOCK(sc);
		return (ENOMEM);
	}

	/*
	 * Set up transmit parameters and enable the transmitter.
	 */
	sc->tnxtfree = sc->tactive = 0;
	sc->tbsyf = sc->tbsyl = NULL;
	WRITE_CSR(sc, WXREG_TCTL, 0);
	DELAY(5 * 1000);
	if (IS_WISEMAN(sc)) {
		WRITE_CSR(sc, WXREG_TDBA_LO,
			vtophys((vm_offset_t)&sc->tdescriptors[0]));
		WRITE_CSR(sc, WXREG_TDBA_HI, 0);
		WRITE_CSR(sc, WXREG_TDLEN, WX_MAX_TDESC * sizeof (wxtd_t));
		WRITE_CSR(sc, WXREG_TDH, 0);
		WRITE_CSR(sc, WXREG_TDT, 0);
		WRITE_CSR(sc, WXREG_TQSA_HI, 0);
		WRITE_CSR(sc, WXREG_TQSA_LO, 0);
		WRITE_CSR(sc, WXREG_TIPG, WX_WISEMAN_TIPG_DFLT);
		WRITE_CSR(sc, WXREG_TIDV, sc->wx_txint_delay);
	} else {
		WRITE_CSR(sc, WXREG_TDBA_LO_LIVENGOOD,
			vtophys((vm_offset_t)&sc->tdescriptors[0]));
		WRITE_CSR(sc, WXREG_TDBA_HI_LIVENGOOD, 0);
		WRITE_CSR(sc, WXREG_TDLEN_LIVENGOOD,
			WX_MAX_TDESC * sizeof (wxtd_t));
		WRITE_CSR(sc, WXREG_TDH_LIVENGOOD, 0);
		WRITE_CSR(sc, WXREG_TDT_LIVENGOOD, 0);
		WRITE_CSR(sc, WXREG_TQSA_HI, 0);
		WRITE_CSR(sc, WXREG_TQSA_LO, 0);
		WRITE_CSR(sc, WXREG_TIPG, WX_LIVENGOOD_TIPG_DFLT);
		WRITE_CSR(sc, WXREG_TIDV_LIVENGOOD, sc->wx_txint_delay);
	}
	WRITE_CSR(sc, WXREG_TCTL, (WXTCTL_CT(WX_COLLISION_THRESHOLD) |
	    WXTCTL_COLD(WX_FDX_COLLISION_DX) | WXTCTL_EN));
	/*
	 * Set up receive parameters and enable the receiver.
	 */

	sc->rnxt = 0;
	WRITE_CSR(sc, WXREG_RCTL, 0);
	DELAY(5 * 1000);
	if (IS_WISEMAN(sc)) {
		WRITE_CSR(sc, WXREG_RDTR0, WXRDTR_FPD);
		WRITE_CSR(sc, WXREG_RDBA0_LO,
		    vtophys((vm_offset_t)&sc->rdescriptors[0]));
		WRITE_CSR(sc, WXREG_RDBA0_HI, 0);
		WRITE_CSR(sc, WXREG_RDLEN0, WX_MAX_RDESC * sizeof (wxrd_t));
		WRITE_CSR(sc, WXREG_RDH0, 0);
		WRITE_CSR(sc, WXREG_RDT0, (WX_MAX_RDESC - RXINCR));
	} else {
		/*
		 * The delay should yield ~10us receive interrupt delay 
		 */
		WRITE_CSR(sc, WXREG_RDTR0_LIVENGOOD, WXRDTR_FPD | 0x40);
		WRITE_CSR(sc, WXREG_RDBA0_LO_LIVENGOOD,
		    vtophys((vm_offset_t)&sc->rdescriptors[0]));
		WRITE_CSR(sc, WXREG_RDBA0_HI_LIVENGOOD, 0);
		WRITE_CSR(sc, WXREG_RDLEN0_LIVENGOOD,
		    WX_MAX_RDESC * sizeof (wxrd_t));
		WRITE_CSR(sc, WXREG_RDH0_LIVENGOOD, 0);
		WRITE_CSR(sc, WXREG_RDT0_LIVENGOOD, (WX_MAX_RDESC - RXINCR));
	}
	WRITE_CSR(sc, WXREG_RDTR1, 0);
	WRITE_CSR(sc, WXREG_RDBA1_LO, 0);
	WRITE_CSR(sc, WXREG_RDBA1_HI, 0);
	WRITE_CSR(sc, WXREG_RDLEN1, 0);
	WRITE_CSR(sc, WXREG_RDH1, 0);
	WRITE_CSR(sc, WXREG_RDT1, 0);

	if (ifp->if_mtu > ETHERMTU) {
		bflags = WXRCTL_EN | WXRCTL_LPE | WXRCTL_2KRBUF;
	} else {
		bflags = WXRCTL_EN | WXRCTL_2KRBUF;
	}

	WRITE_CSR(sc, WXREG_RCTL, bflags |
	    ((ifp->if_flags & IFF_BROADCAST) ? WXRCTL_BAM : 0) |
	    ((ifp->if_flags & IFF_PROMISC) ? WXRCTL_UPE : 0) |
	    ((sc->all_mcasts) ? WXRCTL_MPE : 0));

	/*
	 * Enable Interrupts
	 */
	WX_ENABLE_INT(sc);

	if (sc->wx_mii) {
		mii_mediachg(WX_MII_FROM_SOFTC(sc));
	} else {
		ifm = &sc->wx_media;
		i = ifm->ifm_media;
		ifm->ifm_media = ifm->ifm_cur->ifm_media;
		wx_ifmedia_upd(ifp);
		ifm->ifm_media = i;
	}

	/*
	 * Mark that we're up and running...
	 */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;


	/*
	 * Start stats updater.
	 */
	TIMEOUT(sc, wx_watchdog, sc, hz);

	WX_UNLOCK(sc);
	/*
	 * And we're outta here...
	 */
	return (0);
}

/*
 * Get a receive buffer for our use (and dma map the data area).
 * 
 * The Wiseman chip can have buffers be 256, 512, 1024 or 2048 bytes in size.
 * The LIVENGOOD chip can go higher (up to 16K), but what's the point as
 * we aren't doing non-MCLGET memory management.
 *
 * It wants them aligned on 256 byte boundaries, but can actually cope
 * with an offset in the first 255 bytes of the head of a receive frame.
 *
 * We'll allocate a MCLBYTE sized cluster but *not* adjust the data pointer
 * by any alignment value. Instead, we'll tell the chip to offset by any
 * alignment and we'll catch the alignment on the backend at interrupt time.
 */
static void
wx_rxdma_map(wx_softc_t *sc, rxpkt_t *rxpkt, struct mbuf *mb)
{
	rxpkt->dptr = mb;
	rxpkt->dma_addr = vtophys(mtod(mb, vm_offset_t));
}

static int
wx_get_rbuf(wx_softc_t *sc, rxpkt_t *rxpkt)
{
	struct mbuf *mb;
	MGETHDR(mb, M_DONTWAIT, MT_DATA);
	if (mb == NULL) {
		rxpkt->dptr = NULL;
		return (-1);
	}
	MCLGET(mb, M_DONTWAIT);
	if ((mb->m_flags & M_EXT) == 0) {
		m_freem(mb);
		rxpkt->dptr = NULL;
		return (-1);
	}
	wx_rxdma_map(sc, rxpkt, mb);
	return (0);
}

static int
wx_ioctl(struct ifnet *ifp, IOCTL_CMD_TYPE command, caddr_t data)
{
	wx_softc_t *sc = SOFTC_IFP(ifp);
	struct ifreq *ifr = (struct ifreq *) data;
	int error = 0;

	WX_LOCK(sc);
	switch (command) {
	case SIOCSIFADDR:
	case SIOCGIFADDR:
		error = ether_ioctl(ifp, command, data);
		break;
	case SIOCSIFMTU:
		if (ifr->ifr_mtu > WX_MAXMTU || ifr->ifr_mtu < ETHERMIN) {
			error = EINVAL;
                } else if (ifp->if_mtu != ifr->ifr_mtu) {
			ifp->if_mtu = ifr->ifr_mtu;
			error = wx_init(sc);
                }
                break;
	case SIOCSIFFLAGS:
		sc->all_mcasts = (ifp->if_flags & IFF_ALLMULTI) ? 1 : 0;

		/*
		 * If interface is marked up and not running, then start it.
		 * If it is marked down and running, stop it.
		 * If it's up then re-initialize it. This is so flags
		 * such as IFF_PROMISC are handled.
		 */
		if (ifp->if_flags & IFF_UP) {
			error = wx_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING) {
				wx_stop(sc);
			}
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		sc->all_mcasts = (ifp->if_flags & IFF_ALLMULTI) ? 1 : 0;
		error = wx_mc_setup(sc);
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		DPRINTF(sc, ("%s: ioctl SIOC[GS]IFMEDIA: command=%#lx\n",
		    sc->wx_name, command));
		if (sc->wx_mii) {
			mii_data_t *mii = WX_MII_FROM_SOFTC(sc);
			error = ifmedia_ioctl(ifp, ifr,
			    &mii->mii_media, command);
		} else {
			error = ifmedia_ioctl(ifp, ifr, &sc->wx_media, command);
		}

		break;
	default:
		error = EINVAL;
	}

	WX_UNLOCK(sc);
	return (error);
}

static int
wx_ifmedia_upd(struct ifnet *ifp)
{
	struct wx_softc *sc = SOFTC_IFP(ifp);
	struct ifmedia *ifm;

	DPRINTF(sc, ("%s: ifmedia_upd\n", sc->wx_name));

	if (sc->wx_mii) {
		mii_mediachg(WX_MII_FROM_SOFTC(sc));
		return 0;
	}

	ifm = &sc->wx_media;

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER) {
		return (EINVAL);
	}

	return (0);
}

static void
wx_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	u_int32_t dsr;
	struct wx_softc *sc = SOFTC_IFP(ifp);

	DPRINTF(sc, ("%s: ifmedia_sts: ", sc->wx_name));

	if (sc->wx_mii) {
		mii_data_t *mii = WX_MII_FROM_SOFTC(sc);
		mii_pollstat(mii);
		ifmr->ifm_active = mii->mii_media_active;
		ifmr->ifm_status = mii->mii_media_status;
		DPRINTF(sc, ("active=%#x status=%#x\n",
		    ifmr->ifm_active, ifmr->ifm_status));
		return;
	}

	DPRINTF(sc, ("\n"));
	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (sc->linkup == 0)
		return;

	ifmr->ifm_status |= IFM_ACTIVE;
	dsr = READ_CSR(sc, WXREG_DSR);
	if (IS_LIVENGOOD(sc)) {
		if (dsr &  WXDSR_1000BT) {
			if (IS_LIVENGOOD_CU(sc)) {
				ifmr->ifm_status |= IFM_1000_TX;
			}
			else {
				ifmr->ifm_status |= IFM_1000_SX;
			}
		} else if (dsr & WXDSR_100BT) {
			ifmr->ifm_status |= IFM_100_FX;	/* ?? */
		} else {
			ifmr->ifm_status |= IFM_10_T;	/* ?? */
		}
	} else {
		ifmr->ifm_status |= IFM_1000_SX;
	}
	if (dsr & WXDSR_FD) {
		ifmr->ifm_active |= IFM_FDX;
	}
}


#define RAISE_CLOCK(sc, dcr)	\
		WRITE_CSR(sc, WXREG_DCR, (dcr) | WXPHY_MDC), DELAY(2)

#define LOWER_CLOCK(sc, dcr)	\
		WRITE_CSR(sc, WXREG_DCR, (dcr) & ~WXPHY_MDC), DELAY(2)

static u_int32_t
wx_mii_shift_in(wx_softc_t *sc)
{
	u_int32_t dcr, i;
	u_int32_t data = 0;

	dcr = READ_CSR(sc, WXREG_DCR);
	dcr &= ~(WXPHY_MDIO_DIR | WXPHY_MDIO);
	WRITE_CSR(sc, WXREG_DCR, dcr);
	RAISE_CLOCK(sc, dcr);
	LOWER_CLOCK(sc, dcr);

	for (i = 0; i < 16; i++) {
		data <<= 1;
		RAISE_CLOCK(sc, dcr);
		dcr = READ_CSR(sc, WXREG_DCR);

		if (dcr & WXPHY_MDIO)
			data |= 1;
		
		LOWER_CLOCK(sc, dcr);
	}

	RAISE_CLOCK(sc, dcr);
	LOWER_CLOCK(sc, dcr);
	return (data);
}

static void
wx_mii_shift_out(wx_softc_t *sc, u_int32_t data, u_int32_t count)
{
	u_int32_t dcr, mask;

	dcr = READ_CSR(sc, WXREG_DCR);
	dcr |= WXPHY_MDIO_DIR | WXPHY_MDC_DIR;

	for (mask = (1 << (count - 1)); mask; mask >>= 1) {
		if (data & mask)
			dcr |= WXPHY_MDIO;
		else
			dcr &= ~WXPHY_MDIO;

		WRITE_CSR(sc, WXREG_DCR, dcr);
		DELAY(2);
		RAISE_CLOCK(sc, dcr);
		LOWER_CLOCK(sc, dcr);
	}
}

static int
wx_miibus_readreg(void *arg, int phy, int reg)
{
	wx_softc_t *sc = WX_SOFTC_FROM_MII_ARG(arg);
	unsigned int data = 0;

	if (!IS_LIVENGOOD_CU(sc)) {
		return 0;
	}
	wx_mii_shift_out(sc, WXPHYC_PREAMBLE, WXPHYC_PREAMBLE_LEN);
	wx_mii_shift_out(sc, reg | (phy << 5) | (WXPHYC_READ << 10) |
	    (WXPHYC_SOF << 12), 14);
	data = wx_mii_shift_in(sc);
	return (data & WXMDIC_DATA_MASK);
}

static int
wx_miibus_writereg(void *arg, int phy, int reg, int data)
{
	wx_softc_t *sc = WX_SOFTC_FROM_MII_ARG(arg);
	if (!IS_LIVENGOOD_CU(sc)) {
		return 0;
	}
	wx_mii_shift_out(sc, WXPHYC_PREAMBLE, WXPHYC_PREAMBLE_LEN);
	wx_mii_shift_out(sc, (u_int32_t)data | (WXPHYC_TURNAROUND << 16) |
	    (reg << 18) | (phy << 23) | (WXPHYC_WRITE << 28) |
	    (WXPHYC_SOF << 30), 32);
	return (0);
}

static void
wx_miibus_statchg(void *arg)
{
	wx_softc_t *sc = WX_SOFTC_FROM_MII_ARG(arg);
	mii_data_t *mii = WX_MII_FROM_SOFTC(sc);
	u_int32_t dcr, tctl;

	if (mii == NULL)
		return;

	dcr = sc->wx_dcr;
	tctl = READ_CSR(sc, WXREG_TCTL);
	DPRINTF(sc, ("%s: statchg dcr=%#x tctl=%#x", sc->wx_name, dcr, tctl));

	dcr |= WXDCR_FRCSPD | WXDCR_FRCDPX | WXDCR_SLU;
	dcr &= ~(WXDCR_SPEED_MASK | WXDCR_ASDE /* | WXDCR_ILOS */);

	if (mii->mii_media_status & IFM_ACTIVE) {
		if (IFM_SUBTYPE(mii->mii_media_active) == IFM_NONE) {
			DPRINTF(sc, (" link-down\n"));
			sc->linkup = 0;
			return;
		}

		sc->linkup = 1;
	}

	if (IFM_SUBTYPE(mii->mii_media_active) == IFM_1000_TX) {
		DPRINTF(sc, (" 1000TX"));
		dcr |= WXDCR_1000BT;
	} else if (IFM_SUBTYPE(mii->mii_media_active) == IFM_100_TX) {
		DPRINTF(sc, (" 100TX"));
		dcr |= WXDCR_100BT;
	} else	/* assume IFM_10_TX */ {
		DPRINTF(sc, (" 10TX"));
		dcr |= WXDCR_10BT;
	}

	if (mii->mii_media_active & IFM_FDX) {
		DPRINTF(sc, ("-FD"));
		tctl = WXTCTL_CT(WX_COLLISION_THRESHOLD) |
		    WXTCTL_COLD(WX_FDX_COLLISION_DX) | WXTCTL_EN;
		dcr |= WXDCR_FD;
	} else {
		DPRINTF(sc, ("-HD"));
		tctl = WXTCTL_CT(WX_COLLISION_THRESHOLD) |
		    WXTCTL_COLD(WX_HDX_COLLISION_DX) | WXTCTL_EN;
		dcr &= ~WXDCR_FD;
	}

	/* FLAG0==rx-flow-control FLAG1==tx-flow-control */
	if (mii->mii_media_active & IFM_FLAG0) {
		dcr |= WXDCR_RFCE;
	} else {
		dcr &= ~WXDCR_RFCE;
	}

	if (mii->mii_media_active & IFM_FLAG1) {
		dcr |= WXDCR_TFCE;
	} else {
		dcr &= ~WXDCR_TFCE;
	}

	if (dcr & (WXDCR_RFCE|WXDCR_TFCE)) {
		WRITE_CSR(sc, WXREG_FCAL, FC_FRM_CONST_LO);
		WRITE_CSR(sc, WXREG_FCAH, FC_FRM_CONST_HI);
		WRITE_CSR(sc, WXREG_FCT, FC_TYP_CONST);
	} else {
		WRITE_CSR(sc, WXREG_FCAL, 0);
		WRITE_CSR(sc, WXREG_FCAH, 0);
		WRITE_CSR(sc, WXREG_FCT, 0);
	}

	DPRINTF(sc, (" dcr=%#x tctl=%#x\n", dcr, tctl));
	WRITE_CSR(sc, WXREG_TCTL, tctl);
	sc->wx_dcr = dcr;
	WRITE_CSR(sc, WXREG_DCR, dcr);
}

static void
wx_miibus_mediainit(void *arg)
{
}
