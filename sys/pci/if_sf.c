/*-
 * Copyright (c) 1997, 1998, 1999
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Adaptec AIC-6915 "Starfire" PCI fast ethernet driver for FreeBSD.
 * Programming manual is available from:
 * http://download.adaptec.com/pdfs/user_guides/aic6915_pg.pdf.
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Department of Electical Engineering
 * Columbia University, New York City
 */
/*
 * The Adaptec AIC-6915 "Starfire" is a 64-bit 10/100 PCI ethernet
 * controller designed with flexibility and reducing CPU load in mind.
 * The Starfire offers high and low priority buffer queues, a
 * producer/consumer index mechanism and several different buffer
 * queue and completion queue descriptor types. Any one of a number
 * of different driver designs can be used, depending on system and
 * OS requirements. This driver makes use of type0 transmit frame
 * descriptors (since BSD fragments packets across an mbuf chain)
 * and two RX buffer queues prioritized on size (one queue for small
 * frames that will fit into a single mbuf, another with full size
 * mbuf clusters for everything else). The producer/consumer indexes
 * and completion queues are also used.
 *
 * One downside to the Starfire has to do with alignment: buffer
 * queues must be aligned on 256-byte boundaries, and receive buffers
 * must be aligned on longword boundaries. The receive buffer alignment
 * causes problems on the Alpha platform, where the packet payload
 * should be longword aligned. There is no simple way around this.
 *
 * For receive filtering, the Starfire offers 16 perfect filter slots
 * and a 512-bit hash table.
 *
 * The Starfire has no internal transceiver, relying instead on an
 * external MII-based transceiver. Accessing registers on external
 * PHYs is done through a special register map rather than with the
 * usual bitbang MDIO method.
 *
 * Acesssing the registers on the Starfire is a little tricky. The
 * Starfire has a 512K internal register space. When programmed for
 * PCI memory mapped mode, the entire register space can be accessed
 * directly. However in I/O space mode, only 256 bytes are directly
 * mapped into PCI I/O space. The other registers can be accessed
 * indirectly using the SF_INDIRECTIO_ADDR and SF_INDIRECTIO_DATA
 * registers inside the 256-byte I/O window.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <net/bpf.h>

#include <vm/vm.h>              /* for vtophys */
#include <vm/pmap.h>            /* for vtophys */
#include <machine/bus_pio.h>
#include <machine/bus_memio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

/* "controller miibus0" required.  See GENERIC if you get errors here. */
#include "miibus_if.h"

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#define SF_USEIOSPACE

#include <pci/if_sfreg.h>

MODULE_DEPEND(sf, pci, 1, 1, 1);
MODULE_DEPEND(sf, ether, 1, 1, 1);
MODULE_DEPEND(sf, miibus, 1, 1, 1);

static struct sf_type sf_devs[] = {
	{ AD_VENDORID, AD_DEVICEID_STARFIRE,
		"Adaptec AIC-6915 10/100BaseTX" },
	{ 0, 0, NULL }
};

static int sf_probe		(device_t);
static int sf_attach		(device_t);
static int sf_detach		(device_t);
static void sf_intr		(void *);
static void sf_stats_update	(void *);
static void sf_rxeof		(struct sf_softc *);
static void sf_txeof		(struct sf_softc *);
static int sf_encap		(struct sf_softc *,
					struct sf_tx_bufdesc_type0 *,
					struct mbuf *);
static void sf_start		(struct ifnet *);
static int sf_ioctl		(struct ifnet *, u_long, caddr_t);
static void sf_init		(void *);
static void sf_stop		(struct sf_softc *);
static void sf_watchdog		(struct ifnet *);
static void sf_shutdown		(device_t);
static int sf_ifmedia_upd	(struct ifnet *);
static void sf_ifmedia_sts	(struct ifnet *, struct ifmediareq *);
static void sf_reset		(struct sf_softc *);
static int sf_init_rx_ring	(struct sf_softc *);
static void sf_init_tx_ring	(struct sf_softc *);
static int sf_newbuf		(struct sf_softc *,
					struct sf_rx_bufdesc_type0 *,
					struct mbuf *);
static void sf_setmulti		(struct sf_softc *);
static int sf_setperf		(struct sf_softc *, int, caddr_t);
static int sf_sethash		(struct sf_softc *, caddr_t, int);
#ifdef notdef
static int sf_setvlan		(struct sf_softc *, int, u_int32_t);
#endif

static u_int8_t sf_read_eeprom	(struct sf_softc *, int);

static int sf_miibus_readreg	(device_t, int, int);
static int sf_miibus_writereg	(device_t, int, int, int);
static void sf_miibus_statchg	(device_t);
#ifdef DEVICE_POLLING
static void sf_poll		(struct ifnet *ifp, enum poll_cmd cmd,
				 int count);
static void sf_poll_locked	(struct ifnet *ifp, enum poll_cmd cmd,
				 int count);
#endif /* DEVICE_POLLING */

static u_int32_t csr_read_4	(struct sf_softc *, int);
static void csr_write_4		(struct sf_softc *, int, u_int32_t);
static void sf_txthresh_adjust	(struct sf_softc *);

#ifdef SF_USEIOSPACE
#define SF_RES			SYS_RES_IOPORT
#define SF_RID			SF_PCI_LOIO
#else
#define SF_RES			SYS_RES_MEMORY
#define SF_RID			SF_PCI_LOMEM
#endif

static device_method_t sf_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		sf_probe),
	DEVMETHOD(device_attach,	sf_attach),
	DEVMETHOD(device_detach,	sf_detach),
	DEVMETHOD(device_shutdown,	sf_shutdown),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	sf_miibus_readreg),
	DEVMETHOD(miibus_writereg,	sf_miibus_writereg),
	DEVMETHOD(miibus_statchg,	sf_miibus_statchg),

	{ 0, 0 }
};

static driver_t sf_driver = {
	"sf",
	sf_methods,
	sizeof(struct sf_softc),
};

static devclass_t sf_devclass;

DRIVER_MODULE(sf, pci, sf_driver, sf_devclass, 0, 0);
DRIVER_MODULE(miibus, sf, miibus_driver, miibus_devclass, 0, 0);

#define SF_SETBIT(sc, reg, x)	\
	csr_write_4(sc, reg, csr_read_4(sc, reg) | (x))

#define SF_CLRBIT(sc, reg, x)				\
	csr_write_4(sc, reg, csr_read_4(sc, reg) & ~(x))

static u_int32_t
csr_read_4(sc, reg)
	struct sf_softc		*sc;
	int			reg;
{
	u_int32_t		val;

#ifdef SF_USEIOSPACE
	CSR_WRITE_4(sc, SF_INDIRECTIO_ADDR, reg + SF_RMAP_INTREG_BASE);
	val = CSR_READ_4(sc, SF_INDIRECTIO_DATA);
#else
	val = CSR_READ_4(sc, (reg + SF_RMAP_INTREG_BASE));
#endif

	return(val);
}

static u_int8_t
sf_read_eeprom(sc, reg)
	struct sf_softc		*sc;
	int			reg;
{
	u_int8_t		val;

	val = (csr_read_4(sc, SF_EEADDR_BASE +
	    (reg & 0xFFFFFFFC)) >> (8 * (reg & 3))) & 0xFF;

	return(val);
}

static void
csr_write_4(sc, reg, val)
	struct sf_softc		*sc;
	int			reg;
	u_int32_t		val;
{
#ifdef SF_USEIOSPACE
	CSR_WRITE_4(sc, SF_INDIRECTIO_ADDR, reg + SF_RMAP_INTREG_BASE);
	CSR_WRITE_4(sc, SF_INDIRECTIO_DATA, val);
#else
	CSR_WRITE_4(sc, (reg + SF_RMAP_INTREG_BASE), val);
#endif
}

/*
 * Copy the address 'mac' into the perfect RX filter entry at
 * offset 'idx.' The perfect filter only has 16 entries so do
 * some sanity tests.
 */
static int
sf_setperf(sc, idx, mac)
	struct sf_softc		*sc;
	int			idx;
	caddr_t			mac;
{
	u_int16_t		*p;

	if (idx < 0 || idx > SF_RXFILT_PERFECT_CNT)
		return(EINVAL);

	if (mac == NULL)
		return(EINVAL);

	p = (u_int16_t *)mac;

	csr_write_4(sc, SF_RXFILT_PERFECT_BASE +
	    (idx * SF_RXFILT_PERFECT_SKIP), htons(p[2]));
	csr_write_4(sc, SF_RXFILT_PERFECT_BASE +
	    (idx * SF_RXFILT_PERFECT_SKIP) + 4, htons(p[1]));
	csr_write_4(sc, SF_RXFILT_PERFECT_BASE +
	    (idx * SF_RXFILT_PERFECT_SKIP) + 8, htons(p[0]));

	return(0);
}

/*
 * Set the bit in the 512-bit hash table that corresponds to the
 * specified mac address 'mac.' If 'prio' is nonzero, update the
 * priority hash table instead of the filter hash table.
 */
static int
sf_sethash(sc, mac, prio)
	struct sf_softc		*sc;
	caddr_t			mac;
	int			prio;
{
	u_int32_t		h;

	if (mac == NULL)
		return(EINVAL);

	h = ether_crc32_be(mac, ETHER_ADDR_LEN) >> 23;

	if (prio) {
		SF_SETBIT(sc, SF_RXFILT_HASH_BASE + SF_RXFILT_HASH_PRIOOFF +
		    (SF_RXFILT_HASH_SKIP * (h >> 4)), (1 << (h & 0xF)));
	} else {
		SF_SETBIT(sc, SF_RXFILT_HASH_BASE + SF_RXFILT_HASH_ADDROFF +
		    (SF_RXFILT_HASH_SKIP * (h >> 4)), (1 << (h & 0xF)));
	}

	return(0);
}

#ifdef notdef
/*
 * Set a VLAN tag in the receive filter.
 */
static int
sf_setvlan(sc, idx, vlan)
	struct sf_softc		*sc;
	int			idx;
	u_int32_t		vlan;
{
	if (idx < 0 || idx >> SF_RXFILT_HASH_CNT)
		return(EINVAL);

	csr_write_4(sc, SF_RXFILT_HASH_BASE +
	    (idx * SF_RXFILT_HASH_SKIP) + SF_RXFILT_HASH_VLANOFF, vlan);

	return(0);
}
#endif

static int
sf_miibus_readreg(dev, phy, reg)
	device_t		dev;
	int			phy, reg;
{
	struct sf_softc		*sc;
	int			i;
	u_int32_t		val = 0;

	sc = device_get_softc(dev);

	for (i = 0; i < SF_TIMEOUT; i++) {
		val = csr_read_4(sc, SF_PHY_REG(phy, reg));
		if (val & SF_MII_DATAVALID)
			break;
	}

	if (i == SF_TIMEOUT)
		return(0);

	if ((val & 0x0000FFFF) == 0xFFFF)
		return(0);

	return(val & 0x0000FFFF);
}

static int
sf_miibus_writereg(dev, phy, reg, val)
	device_t		dev;
	int			phy, reg, val;
{
	struct sf_softc		*sc;
	int			i;
	int			busy;

	sc = device_get_softc(dev);

	csr_write_4(sc, SF_PHY_REG(phy, reg), val);

	for (i = 0; i < SF_TIMEOUT; i++) {
		busy = csr_read_4(sc, SF_PHY_REG(phy, reg));
		if (!(busy & SF_MII_BUSY))
			break;
	}

	return(0);
}

static void
sf_miibus_statchg(dev)
	device_t		dev;
{
	struct sf_softc		*sc;
	struct mii_data		*mii;

	sc = device_get_softc(dev);
	mii = device_get_softc(sc->sf_miibus);

	if ((mii->mii_media_active & IFM_GMASK) == IFM_FDX) {
		SF_SETBIT(sc, SF_MACCFG_1, SF_MACCFG1_FULLDUPLEX);
		csr_write_4(sc, SF_BKTOBKIPG, SF_IPGT_FDX);
	} else {
		SF_CLRBIT(sc, SF_MACCFG_1, SF_MACCFG1_FULLDUPLEX);
		csr_write_4(sc, SF_BKTOBKIPG, SF_IPGT_HDX);
	}
}

static void
sf_setmulti(sc)
	struct sf_softc		*sc;
{
	struct ifnet		*ifp;
	int			i;
	struct ifmultiaddr	*ifma;
	u_int8_t		dummy[] = { 0, 0, 0, 0, 0, 0 };

	ifp = &sc->arpcom.ac_if;

	/* First zot all the existing filters. */
	for (i = 1; i < SF_RXFILT_PERFECT_CNT; i++)
		sf_setperf(sc, i, (char *)&dummy);
	for (i = SF_RXFILT_HASH_BASE;
	    i < (SF_RXFILT_HASH_MAX + 1); i += 4)
		csr_write_4(sc, i, 0);
	SF_CLRBIT(sc, SF_RXFILT, SF_RXFILT_ALLMULTI);

	/* Now program new ones. */
	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		SF_SETBIT(sc, SF_RXFILT, SF_RXFILT_ALLMULTI);
	} else {
		i = 1;
		TAILQ_FOREACH_REVERSE(ifma, &ifp->if_multiaddrs, ifmultihead, ifma_link) {
			if (ifma->ifma_addr->sa_family != AF_LINK)
				continue;
			/*
			 * Program the first 15 multicast groups
			 * into the perfect filter. For all others,
			 * use the hash table.
			 */
			if (i < SF_RXFILT_PERFECT_CNT) {
				sf_setperf(sc, i,
			LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
				i++;
				continue;
			}

			sf_sethash(sc,
			    LLADDR((struct sockaddr_dl *)ifma->ifma_addr), 0);
		}
	}
}

/*
 * Set media options.
 */
static int
sf_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	struct sf_softc		*sc;
	struct mii_data		*mii;

	sc = ifp->if_softc;
	mii = device_get_softc(sc->sf_miibus);
	sc->sf_link = 0;
	if (mii->mii_instance) {
		struct mii_softc        *miisc;
		LIST_FOREACH(miisc, &mii->mii_phys, mii_list)
			mii_phy_reset(miisc);
	}
	mii_mediachg(mii);

	return(0);
}

/*
 * Report current media status.
 */
static void
sf_ifmedia_sts(ifp, ifmr)
	struct ifnet		*ifp;
	struct ifmediareq	*ifmr;
{
	struct sf_softc		*sc;
	struct mii_data		*mii;

	sc = ifp->if_softc;
	mii = device_get_softc(sc->sf_miibus);

	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
}

static int
sf_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct sf_softc		*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *) data;
	struct mii_data		*mii;
	int			error = 0;

	SF_LOCK(sc);

	switch(command) {
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->sf_if_flags & IFF_PROMISC)) {
				SF_SETBIT(sc, SF_RXFILT, SF_RXFILT_PROMISC);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->sf_if_flags & IFF_PROMISC) {
				SF_CLRBIT(sc, SF_RXFILT, SF_RXFILT_PROMISC);
			} else if (!(ifp->if_flags & IFF_RUNNING))
				sf_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				sf_stop(sc);
		}
		sc->sf_if_flags = ifp->if_flags;
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		sf_setmulti(sc);
		error = 0;
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		mii = device_get_softc(sc->sf_miibus);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
		break;
	case SIOCSIFCAP:
		ifp->if_capenable &= ~IFCAP_POLLING;
		ifp->if_capenable |= ifr->ifr_reqcap & IFCAP_POLLING;
		break;
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	SF_UNLOCK(sc);

	return(error);
}

static void
sf_reset(sc)
	struct sf_softc		*sc;
{
	register int		i;

	csr_write_4(sc, SF_GEN_ETH_CTL, 0);
	SF_SETBIT(sc, SF_MACCFG_1, SF_MACCFG1_SOFTRESET);
	DELAY(1000);
	SF_CLRBIT(sc, SF_MACCFG_1, SF_MACCFG1_SOFTRESET);

	SF_SETBIT(sc, SF_PCI_DEVCFG, SF_PCIDEVCFG_RESET);

	for (i = 0; i < SF_TIMEOUT; i++) {
		DELAY(10);
		if (!(csr_read_4(sc, SF_PCI_DEVCFG) & SF_PCIDEVCFG_RESET))
			break;
	}

	if (i == SF_TIMEOUT)
		printf("sf%d: reset never completed!\n", sc->sf_unit);

	/* Wait a little while for the chip to get its brains in order. */
	DELAY(1000);
}

/*
 * Probe for an Adaptec AIC-6915 chip. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 * We also check the subsystem ID so that we can identify exactly which
 * NIC has been found, if possible.
 */
static int
sf_probe(dev)
	device_t		dev;
{
	struct sf_type		*t;

	t = sf_devs;

	while(t->sf_name != NULL) {
		if ((pci_get_vendor(dev) == t->sf_vid) &&
		    (pci_get_device(dev) == t->sf_did)) {
			switch((pci_read_config(dev,
			    SF_PCI_SUBVEN_ID, 4) >> 16) & 0xFFFF) {
			case AD_SUBSYSID_62011_REV0:
			case AD_SUBSYSID_62011_REV1:
				device_set_desc(dev,
				    "Adaptec ANA-62011 10/100BaseTX");
				return(0);
			case AD_SUBSYSID_62022:
				device_set_desc(dev,
				    "Adaptec ANA-62022 10/100BaseTX");
				return(0);
			case AD_SUBSYSID_62044_REV0:
			case AD_SUBSYSID_62044_REV1:
				device_set_desc(dev,
				    "Adaptec ANA-62044 10/100BaseTX");
				return(0);
			case AD_SUBSYSID_62020:
				device_set_desc(dev,
				    "Adaptec ANA-62020 10/100BaseFX");
				return(0);
			case AD_SUBSYSID_69011:
				device_set_desc(dev,
				    "Adaptec ANA-69011 10/100BaseTX");
				return(0);
			default:
				device_set_desc(dev, t->sf_name);
				return(0);
				break;
			}
		}
		t++;
	}

	return(ENXIO);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
static int
sf_attach(dev)
	device_t		dev;
{
	int			i;
	struct sf_softc		*sc;
	struct ifnet		*ifp;
	int			unit, rid, error = 0;

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);

	mtx_init(&sc->sf_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF | MTX_RECURSE);
	/*
	 * Map control/status registers.
	 */
	pci_enable_busmaster(dev);

	rid = SF_RID;
	sc->sf_res = bus_alloc_resource_any(dev, SF_RES, &rid, RF_ACTIVE);

	if (sc->sf_res == NULL) {
		printf ("sf%d: couldn't map ports\n", unit);
		error = ENXIO;
		goto fail;
	}

	sc->sf_btag = rman_get_bustag(sc->sf_res);
	sc->sf_bhandle = rman_get_bushandle(sc->sf_res);

	/* Allocate interrupt */
	rid = 0;
	sc->sf_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_SHAREABLE | RF_ACTIVE);

	if (sc->sf_irq == NULL) {
		printf("sf%d: couldn't map interrupt\n", unit);
		error = ENXIO;
		goto fail;
	}

	callout_handle_init(&sc->sf_stat_ch);
	/* Reset the adapter. */
	sf_reset(sc);

	/*
	 * Get station address from the EEPROM.
	 */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		sc->arpcom.ac_enaddr[i] =
		    sf_read_eeprom(sc, SF_EE_NODEADDR + ETHER_ADDR_LEN - i);

	sc->sf_unit = unit;

	/* Allocate the descriptor queues. */
	sc->sf_ldata = contigmalloc(sizeof(struct sf_list_data), M_DEVBUF,
	    M_NOWAIT, 0, 0xffffffff, PAGE_SIZE, 0);

	if (sc->sf_ldata == NULL) {
		printf("sf%d: no memory for list buffers!\n", unit);
		error = ENXIO;
		goto fail;
	}

	bzero(sc->sf_ldata, sizeof(struct sf_list_data));

	/* Do MII setup. */
	if (mii_phy_probe(dev, &sc->sf_miibus,
	    sf_ifmedia_upd, sf_ifmedia_sts)) {
		printf("sf%d: MII without any phy!\n", sc->sf_unit);
		error = ENXIO;
		goto fail;
	}

	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST |
	    IFF_NEEDSGIANT;
	ifp->if_ioctl = sf_ioctl;
	ifp->if_start = sf_start;
	ifp->if_watchdog = sf_watchdog;
	ifp->if_init = sf_init;
	ifp->if_baudrate = 10000000;
	IFQ_SET_MAXLEN(&ifp->if_snd, SF_TX_DLIST_CNT - 1);
	ifp->if_snd.ifq_drv_maxlen = SF_TX_DLIST_CNT - 1;
	IFQ_SET_READY(&ifp->if_snd);
#ifdef DEVICE_POLLING
	ifp->if_capabilities |= IFCAP_POLLING;
#endif /* DEVICE_POLLING */
	ifp->if_capenable = ifp->if_capabilities;

	/*
	 * Call MI attach routine.
	 */
	ether_ifattach(ifp, sc->arpcom.ac_enaddr);

	/* Hook interrupt last to avoid having to lock softc */
	error = bus_setup_intr(dev, sc->sf_irq, INTR_TYPE_NET,
	    sf_intr, sc, &sc->sf_intrhand);

	if (error) {
		printf("sf%d: couldn't set up irq\n", unit);
		ether_ifdetach(ifp);
		goto fail;
	}

fail:
	if (error)
		sf_detach(dev);

	return(error);
}

/*
 * Shutdown hardware and free up resources. This can be called any
 * time after the mutex has been initialized. It is called in both
 * the error case in attach and the normal detach case so it needs
 * to be careful about only freeing resources that have actually been
 * allocated.
 */
static int
sf_detach(dev)
	device_t		dev;
{
	struct sf_softc		*sc;
	struct ifnet		*ifp;

	sc = device_get_softc(dev);
	KASSERT(mtx_initialized(&sc->sf_mtx), ("sf mutex not initialized"));
	SF_LOCK(sc);
	ifp = &sc->arpcom.ac_if;

	/* These should only be active if attach succeeded */
	if (device_is_attached(dev)) {
		sf_stop(sc);
		ether_ifdetach(ifp);
	}
	if (sc->sf_miibus)
		device_delete_child(dev, sc->sf_miibus);
	bus_generic_detach(dev);

	if (sc->sf_intrhand)
		bus_teardown_intr(dev, sc->sf_irq, sc->sf_intrhand);
	if (sc->sf_irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->sf_irq);
	if (sc->sf_res)
		bus_release_resource(dev, SF_RES, SF_RID, sc->sf_res);

	if (sc->sf_ldata)
		contigfree(sc->sf_ldata, sizeof(struct sf_list_data), M_DEVBUF);

	SF_UNLOCK(sc);
	mtx_destroy(&sc->sf_mtx);

	return(0);
}

static int
sf_init_rx_ring(sc)
	struct sf_softc		*sc;
{
	struct sf_list_data	*ld;
	int			i;

	ld = sc->sf_ldata;

	bzero((char *)ld->sf_rx_dlist_big,
	    sizeof(struct sf_rx_bufdesc_type0) * SF_RX_DLIST_CNT);
	bzero((char *)ld->sf_rx_clist,
	    sizeof(struct sf_rx_cmpdesc_type3) * SF_RX_CLIST_CNT);

	for (i = 0; i < SF_RX_DLIST_CNT; i++) {
		if (sf_newbuf(sc, &ld->sf_rx_dlist_big[i], NULL) == ENOBUFS)
			return(ENOBUFS);
	}

	return(0);
}

static void
sf_init_tx_ring(sc)
	struct sf_softc		*sc;
{
	struct sf_list_data	*ld;
	int			i;

	ld = sc->sf_ldata;

	bzero((char *)ld->sf_tx_dlist,
	    sizeof(struct sf_tx_bufdesc_type0) * SF_TX_DLIST_CNT);
	bzero((char *)ld->sf_tx_clist,
	    sizeof(struct sf_tx_cmpdesc_type0) * SF_TX_CLIST_CNT);

	for (i = 0; i < SF_TX_DLIST_CNT; i++)
		ld->sf_tx_dlist[i].sf_id = SF_TX_BUFDESC_ID;
	for (i = 0; i < SF_TX_CLIST_CNT; i++)
		ld->sf_tx_clist[i].sf_type = SF_TXCMPTYPE_TX;

	ld->sf_tx_dlist[SF_TX_DLIST_CNT - 1].sf_end = 1;
	sc->sf_tx_cnt = 0;
}

static int
sf_newbuf(sc, c, m)
	struct sf_softc		*sc;
	struct sf_rx_bufdesc_type0	*c;
	struct mbuf		*m;
{
	struct mbuf		*m_new = NULL;

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL)
			return(ENOBUFS);

		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			m_freem(m_new);
			return(ENOBUFS);
		}
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
	} else {
		m_new = m;
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
		m_new->m_data = m_new->m_ext.ext_buf;
	}

	m_adj(m_new, sizeof(u_int64_t));

	c->sf_mbuf = m_new;
	c->sf_addrlo = SF_RX_HOSTADDR(vtophys(mtod(m_new, caddr_t)));
	c->sf_valid = 1;

	return(0);
}

/*
 * The starfire is programmed to use 'normal' mode for packet reception,
 * which means we use the consumer/producer model for both the buffer
 * descriptor queue and the completion descriptor queue. The only problem
 * with this is that it involves a lot of register accesses: we have to
 * read the RX completion consumer and producer indexes and the RX buffer
 * producer index, plus the RX completion consumer and RX buffer producer
 * indexes have to be updated. It would have been easier if Adaptec had
 * put each index in a separate register, especially given that the damn
 * NIC has a 512K register space.
 *
 * In spite of all the lovely features that Adaptec crammed into the 6915,
 * it is marred by one truly stupid design flaw, which is that receive
 * buffer addresses must be aligned on a longword boundary. This forces
 * the packet payload to be unaligned, which is suboptimal on the x86 and
 * completely unuseable on the Alpha. Our only recourse is to copy received
 * packets into properly aligned buffers before handing them off.
 */

static void
sf_rxeof(sc)
	struct sf_softc		*sc;
{
	struct mbuf		*m;
	struct ifnet		*ifp;
	struct sf_rx_bufdesc_type0	*desc;
	struct sf_rx_cmpdesc_type3	*cur_rx;
	u_int32_t		rxcons, rxprod;
	int			cmpprodidx, cmpconsidx, bufprodidx;

	SF_LOCK_ASSERT(sc);

	ifp = &sc->arpcom.ac_if;

	rxcons = csr_read_4(sc, SF_CQ_CONSIDX);
	rxprod = csr_read_4(sc, SF_RXDQ_PTR_Q1);
	cmpprodidx = SF_IDX_LO(csr_read_4(sc, SF_CQ_PRODIDX));
	cmpconsidx = SF_IDX_LO(rxcons);
	bufprodidx = SF_IDX_LO(rxprod);

	while (cmpconsidx != cmpprodidx) {
		struct mbuf		*m0;

#ifdef DEVICE_POLLING
		if (ifp->if_flags & IFF_POLLING) {
			if (sc->rxcycles <= 0)
				break;
			sc->rxcycles--;
		}
#endif /* DEVICE_POLLING */

		cur_rx = &sc->sf_ldata->sf_rx_clist[cmpconsidx];
		desc = &sc->sf_ldata->sf_rx_dlist_big[cur_rx->sf_endidx];
		m = desc->sf_mbuf;
		SF_INC(cmpconsidx, SF_RX_CLIST_CNT);
		SF_INC(bufprodidx, SF_RX_DLIST_CNT);

		if (!(cur_rx->sf_status1 & SF_RXSTAT1_OK)) {
			ifp->if_ierrors++;
			sf_newbuf(sc, desc, m);
			continue;
		}

		m0 = m_devget(mtod(m, char *), cur_rx->sf_len, ETHER_ALIGN,
		    ifp, NULL);
		sf_newbuf(sc, desc, m);
		if (m0 == NULL) {
			ifp->if_ierrors++;
			continue;
		}
		m = m0;

		ifp->if_ipackets++;
		SF_UNLOCK(sc);
		(*ifp->if_input)(ifp, m);
		SF_LOCK(sc);
	}

	csr_write_4(sc, SF_CQ_CONSIDX,
	    (rxcons & ~SF_CQ_CONSIDX_RXQ1) | cmpconsidx);
	csr_write_4(sc, SF_RXDQ_PTR_Q1,
	    (rxprod & ~SF_RXDQ_PRODIDX) | bufprodidx);
}

/*
 * Read the transmit status from the completion queue and release
 * mbufs. Note that the buffer descriptor index in the completion
 * descriptor is an offset from the start of the transmit buffer
 * descriptor list in bytes. This is important because the manual
 * gives the impression that it should match the producer/consumer
 * index, which is the offset in 8 byte blocks.
 */
static void
sf_txeof(sc)
	struct sf_softc		*sc;
{
	int			txcons, cmpprodidx, cmpconsidx;
	struct sf_tx_cmpdesc_type1 *cur_cmp;
	struct sf_tx_bufdesc_type0 *cur_tx;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	txcons = csr_read_4(sc, SF_CQ_CONSIDX);
	cmpprodidx = SF_IDX_HI(csr_read_4(sc, SF_CQ_PRODIDX));
	cmpconsidx = SF_IDX_HI(txcons);

	while (cmpconsidx != cmpprodidx) {
		cur_cmp = &sc->sf_ldata->sf_tx_clist[cmpconsidx];
		cur_tx = &sc->sf_ldata->sf_tx_dlist[cur_cmp->sf_index >> 7];

		if (cur_cmp->sf_txstat & SF_TXSTAT_TX_OK)
			ifp->if_opackets++;
		else {
			if (cur_cmp->sf_txstat & SF_TXSTAT_TX_UNDERRUN)
				sf_txthresh_adjust(sc);
			ifp->if_oerrors++;
		}

		sc->sf_tx_cnt--;
		if (cur_tx->sf_mbuf != NULL) {
			m_freem(cur_tx->sf_mbuf);
			cur_tx->sf_mbuf = NULL;
		} else
			break;
		SF_INC(cmpconsidx, SF_TX_CLIST_CNT);
	}

	ifp->if_timer = 0;
	ifp->if_flags &= ~IFF_OACTIVE;

	csr_write_4(sc, SF_CQ_CONSIDX,
	    (txcons & ~SF_CQ_CONSIDX_TXQ) |
	    ((cmpconsidx << 16) & 0xFFFF0000));
}

static void
sf_txthresh_adjust(sc)
	struct sf_softc		*sc;
{
	u_int32_t		txfctl;
	u_int8_t		txthresh;

	txfctl = csr_read_4(sc, SF_TX_FRAMCTL);
	txthresh = txfctl & SF_TXFRMCTL_TXTHRESH;
	if (txthresh < 0xFF) {
		txthresh++;
		txfctl &= ~SF_TXFRMCTL_TXTHRESH;
		txfctl |= txthresh;
#ifdef DIAGNOSTIC
		printf("sf%d: tx underrun, increasing "
		    "tx threshold to %d bytes\n",
		    sc->sf_unit, txthresh * 4);
#endif
		csr_write_4(sc, SF_TX_FRAMCTL, txfctl);
	}
}

#ifdef DEVICE_POLLING
static void
sf_poll(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct sf_softc *sc = ifp->if_softc;

	SF_LOCK(sc);
	sf_poll_locked(ifp, cmd, count);
	SF_UNLOCK(sc);
}

static void
sf_poll_locked(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct sf_softc *sc = ifp->if_softc;

	SF_LOCK_ASSERT(sc);

	if (!(ifp->if_capenable & IFCAP_POLLING)) {
		ether_poll_deregister(ifp);
		cmd = POLL_DEREGISTER;
	}

	if (cmd == POLL_DEREGISTER) {
		/* Final call, enable interrupts. */
		csr_write_4(sc, SF_IMR, SF_INTRS);
		return;
	}

	sc->rxcycles = count;
	sf_rxeof(sc);
	sf_txeof(sc);
	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		sf_start(ifp);

	if (cmd == POLL_AND_CHECK_STATUS) {
		u_int32_t status;

		status = csr_read_4(sc, SF_ISR);
		if (status)
			csr_write_4(sc, SF_ISR, status);

		if (status & SF_ISR_TX_LOFIFO)
			sf_txthresh_adjust(sc);

		if (status & SF_ISR_ABNORMALINTR) {
			if (status & SF_ISR_STATSOFLOW) {
				untimeout(sf_stats_update, sc,
				    sc->sf_stat_ch);
				sf_stats_update(sc);
			} else
				sf_init(sc);
		}
	}
}
#endif /* DEVICE_POLLING */

static void
sf_intr(arg)
	void			*arg;
{
	struct sf_softc		*sc;
	struct ifnet		*ifp;
	u_int32_t		status;

	sc = arg;
	SF_LOCK(sc);

	ifp = &sc->arpcom.ac_if;

#ifdef DEVICE_POLLING
	if (ifp->if_flags & IFF_POLLING)
		goto done_locked;

	if ((ifp->if_capenable & IFCAP_POLLING) &&
	    ether_poll_register(sf_poll, ifp)) {
		/* OK, disable interrupts. */
		csr_write_4(sc, SF_IMR, 0x00000000);
		sf_poll_locked(ifp, 0, 1);
		goto done_locked;
	}
#endif /* DEVICE_POLLING */

	if (!(csr_read_4(sc, SF_ISR_SHADOW) & SF_ISR_PCIINT_ASSERTED)) {
		SF_UNLOCK(sc);
		return;
	}

	/* Disable interrupts. */
	csr_write_4(sc, SF_IMR, 0x00000000);

	for (;;) {
		status = csr_read_4(sc, SF_ISR);
		if (status)
			csr_write_4(sc, SF_ISR, status);

		if (!(status & SF_INTRS))
			break;

		if (status & SF_ISR_RXDQ1_DMADONE)
			sf_rxeof(sc);

		if (status & SF_ISR_TX_TXDONE ||
		    status & SF_ISR_TX_DMADONE ||
		    status & SF_ISR_TX_QUEUEDONE)
			sf_txeof(sc);

		if (status & SF_ISR_TX_LOFIFO)
			sf_txthresh_adjust(sc);

		if (status & SF_ISR_ABNORMALINTR) {
			if (status & SF_ISR_STATSOFLOW) {
				untimeout(sf_stats_update, sc,
				    sc->sf_stat_ch);
				sf_stats_update(sc);
			} else
				sf_init(sc);
		}
	}

	/* Re-enable interrupts. */
	csr_write_4(sc, SF_IMR, SF_INTRS);

	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		sf_start(ifp);

#ifdef DEVICE_POLLING
done_locked:
#endif /* DEVICE_POLLING */
	SF_UNLOCK(sc);
}

static void
sf_init(xsc)
	void			*xsc;
{
	struct sf_softc		*sc;
	struct ifnet		*ifp;
	struct mii_data		*mii;
	int			i;

	sc = xsc;
	SF_LOCK(sc);
	ifp = &sc->arpcom.ac_if;
	mii = device_get_softc(sc->sf_miibus);

	sf_stop(sc);
	sf_reset(sc);

	/* Init all the receive filter registers */
	for (i = SF_RXFILT_PERFECT_BASE;
	    i < (SF_RXFILT_HASH_MAX + 1); i += 4)
		csr_write_4(sc, i, 0);

	/* Empty stats counter registers. */
	for (i = 0; i < sizeof(struct sf_stats)/sizeof(u_int32_t); i++)
		csr_write_4(sc, SF_STATS_BASE +
		    (i + sizeof(u_int32_t)), 0);

	/* Init our MAC address */
	csr_write_4(sc, SF_PAR0, *(u_int32_t *)(&sc->arpcom.ac_enaddr[0]));
	csr_write_4(sc, SF_PAR1, *(u_int32_t *)(&sc->arpcom.ac_enaddr[4]));
	sf_setperf(sc, 0, (caddr_t)&sc->arpcom.ac_enaddr);

	if (sf_init_rx_ring(sc) == ENOBUFS) {
		printf("sf%d: initialization failed: no "
		    "memory for rx buffers\n", sc->sf_unit);
		SF_UNLOCK(sc);
		return;
	}

	sf_init_tx_ring(sc);

	csr_write_4(sc, SF_RXFILT, SF_PERFMODE_NORMAL|SF_HASHMODE_WITHVLAN);

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC) {
		SF_SETBIT(sc, SF_RXFILT, SF_RXFILT_PROMISC);
	} else {
		SF_CLRBIT(sc, SF_RXFILT, SF_RXFILT_PROMISC);
	}

	if (ifp->if_flags & IFF_BROADCAST) {
		SF_SETBIT(sc, SF_RXFILT, SF_RXFILT_BROAD);
	} else {
		SF_CLRBIT(sc, SF_RXFILT, SF_RXFILT_BROAD);
	}

	/*
	 * Load the multicast filter.
	 */
	sf_setmulti(sc);

	/* Init the completion queue indexes */
	csr_write_4(sc, SF_CQ_CONSIDX, 0);
	csr_write_4(sc, SF_CQ_PRODIDX, 0);

	/* Init the RX completion queue */
	csr_write_4(sc, SF_RXCQ_CTL_1,
	    vtophys(sc->sf_ldata->sf_rx_clist) & SF_RXCQ_ADDR);
	SF_SETBIT(sc, SF_RXCQ_CTL_1, SF_RXCQTYPE_3);

	/* Init RX DMA control. */
	SF_SETBIT(sc, SF_RXDMA_CTL, SF_RXDMA_REPORTBADPKTS);

	/* Init the RX buffer descriptor queue. */
	csr_write_4(sc, SF_RXDQ_ADDR_Q1,
	    vtophys(sc->sf_ldata->sf_rx_dlist_big));
	csr_write_4(sc, SF_RXDQ_CTL_1, (MCLBYTES << 16) | SF_DESCSPACE_16BYTES);
	csr_write_4(sc, SF_RXDQ_PTR_Q1, SF_RX_DLIST_CNT - 1);

	/* Init the TX completion queue */
	csr_write_4(sc, SF_TXCQ_CTL,
	    vtophys(sc->sf_ldata->sf_tx_clist) & SF_RXCQ_ADDR);

	/* Init the TX buffer descriptor queue. */
	csr_write_4(sc, SF_TXDQ_ADDR_HIPRIO,
		vtophys(sc->sf_ldata->sf_tx_dlist));
	SF_SETBIT(sc, SF_TX_FRAMCTL, SF_TXFRMCTL_CPLAFTERTX);
	csr_write_4(sc, SF_TXDQ_CTL,
	    SF_TXBUFDESC_TYPE0|SF_TXMINSPACE_128BYTES|SF_TXSKIPLEN_8BYTES);
	SF_SETBIT(sc, SF_TXDQ_CTL, SF_TXDQCTL_NODMACMP);

	/* Enable autopadding of short TX frames. */
	SF_SETBIT(sc, SF_MACCFG_1, SF_MACCFG1_AUTOPAD);

#ifdef DEVICE_POLLING
	/* Disable interrupts if we are polling. */
	if (ifp->if_flags & IFF_POLLING)
		csr_write_4(sc, SF_IMR, 0x00000000);
	else
#endif /* DEVICE_POLLING */

	/* Enable interrupts. */
	csr_write_4(sc, SF_IMR, SF_INTRS);
	SF_SETBIT(sc, SF_PCI_DEVCFG, SF_PCIDEVCFG_INTR_ENB);

	/* Enable the RX and TX engines. */
	SF_SETBIT(sc, SF_GEN_ETH_CTL, SF_ETHCTL_RX_ENB|SF_ETHCTL_RXDMA_ENB);
	SF_SETBIT(sc, SF_GEN_ETH_CTL, SF_ETHCTL_TX_ENB|SF_ETHCTL_TXDMA_ENB);

	/*mii_mediachg(mii);*/
	sf_ifmedia_upd(ifp);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	sc->sf_stat_ch = timeout(sf_stats_update, sc, hz);

	SF_UNLOCK(sc);
}

static int
sf_encap(sc, c, m_head)
	struct sf_softc		*sc;
	struct sf_tx_bufdesc_type0 *c;
	struct mbuf		*m_head;
{
	int			frag = 0;
	struct sf_frag		*f = NULL;
	struct mbuf		*m;

	m = m_head;

	for (m = m_head, frag = 0; m != NULL; m = m->m_next) {
		if (m->m_len != 0) {
			if (frag == SF_MAXFRAGS)
				break;
			f = &c->sf_frags[frag];
			if (frag == 0)
				f->sf_pktlen = m_head->m_pkthdr.len;
			f->sf_fraglen = m->m_len;
			f->sf_addr = vtophys(mtod(m, vm_offset_t));
			frag++;
		}
	}

	if (m != NULL) {
		struct mbuf		*m_new = NULL;

		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			printf("sf%d: no memory for tx list\n", sc->sf_unit);
			return(1);
		}

		if (m_head->m_pkthdr.len > MHLEN) {
			MCLGET(m_new, M_DONTWAIT);
			if (!(m_new->m_flags & M_EXT)) {
				m_freem(m_new);
				printf("sf%d: no memory for tx list\n",
				    sc->sf_unit);
				return(1);
			}
		}
		m_copydata(m_head, 0, m_head->m_pkthdr.len,
		    mtod(m_new, caddr_t));
		m_new->m_pkthdr.len = m_new->m_len = m_head->m_pkthdr.len;
		m_freem(m_head);
		m_head = m_new;
		f = &c->sf_frags[0];
		f->sf_fraglen = f->sf_pktlen = m_head->m_pkthdr.len;
		f->sf_addr = vtophys(mtod(m_head, caddr_t));
		frag = 1;
	}

	c->sf_mbuf = m_head;
	c->sf_id = SF_TX_BUFDESC_ID;
	c->sf_fragcnt = frag;
	c->sf_intr = 1;
	c->sf_caltcp = 0;
	c->sf_crcen = 1;

	return(0);
}

static void
sf_start(ifp)
	struct ifnet		*ifp;
{
	struct sf_softc		*sc;
	struct sf_tx_bufdesc_type0 *cur_tx = NULL;
	struct mbuf		*m_head = NULL;
	int			i, txprod;

	sc = ifp->if_softc;
	SF_LOCK(sc);

	if (!sc->sf_link && ifp->if_snd.ifq_len < 10) {
		SF_UNLOCK(sc);
		return;
	}

	if (ifp->if_flags & IFF_OACTIVE) {
		SF_UNLOCK(sc);
		return;
	}

	txprod = csr_read_4(sc, SF_TXDQ_PRODIDX);
	i = SF_IDX_HI(txprod) >> 4;

	if (sc->sf_ldata->sf_tx_dlist[i].sf_mbuf != NULL) {
		printf("sf%d: TX ring full, resetting\n", sc->sf_unit);
		sf_init(sc);
		txprod = csr_read_4(sc, SF_TXDQ_PRODIDX);
		i = SF_IDX_HI(txprod) >> 4;
	}

	while(sc->sf_ldata->sf_tx_dlist[i].sf_mbuf == NULL) {
		if (sc->sf_tx_cnt >= (SF_TX_DLIST_CNT - 5)) {
			ifp->if_flags |= IFF_OACTIVE;
			cur_tx = NULL;
			break;
		}
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		cur_tx = &sc->sf_ldata->sf_tx_dlist[i];
		if (sf_encap(sc, cur_tx, m_head)) {
			IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
			ifp->if_flags |= IFF_OACTIVE;
			cur_tx = NULL;
			break;
		}

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		BPF_MTAP(ifp, m_head);

		SF_INC(i, SF_TX_DLIST_CNT);
		sc->sf_tx_cnt++;
		/*
		 * Don't get the TX DMA queue get too full.
		 */
		if (sc->sf_tx_cnt > 64)
			break;
	}

	if (cur_tx == NULL) {
		SF_UNLOCK(sc);
		return;
	}

	/* Transmit */
	csr_write_4(sc, SF_TXDQ_PRODIDX,
	    (txprod & ~SF_TXDQ_PRODIDX_HIPRIO) |
	    ((i << 20) & 0xFFFF0000));

	ifp->if_timer = 5;

	SF_UNLOCK(sc);
}

static void
sf_stop(sc)
	struct sf_softc		*sc;
{
	int			i;
	struct ifnet		*ifp;

	SF_LOCK(sc);

	ifp = &sc->arpcom.ac_if;

	untimeout(sf_stats_update, sc, sc->sf_stat_ch);

#ifdef DEVICE_POLLING
	ether_poll_deregister(ifp);
#endif /* DEVICE_POLLING */
	
	csr_write_4(sc, SF_GEN_ETH_CTL, 0);
	csr_write_4(sc, SF_CQ_CONSIDX, 0);
	csr_write_4(sc, SF_CQ_PRODIDX, 0);
	csr_write_4(sc, SF_RXDQ_ADDR_Q1, 0);
	csr_write_4(sc, SF_RXDQ_CTL_1, 0);
	csr_write_4(sc, SF_RXDQ_PTR_Q1, 0);
	csr_write_4(sc, SF_TXCQ_CTL, 0);
	csr_write_4(sc, SF_TXDQ_ADDR_HIPRIO, 0);
	csr_write_4(sc, SF_TXDQ_CTL, 0);
	sf_reset(sc);

	sc->sf_link = 0;

	for (i = 0; i < SF_RX_DLIST_CNT; i++) {
		if (sc->sf_ldata->sf_rx_dlist_big[i].sf_mbuf != NULL) {
			m_freem(sc->sf_ldata->sf_rx_dlist_big[i].sf_mbuf);
			sc->sf_ldata->sf_rx_dlist_big[i].sf_mbuf = NULL;
		}
	}

	for (i = 0; i < SF_TX_DLIST_CNT; i++) {
		if (sc->sf_ldata->sf_tx_dlist[i].sf_mbuf != NULL) {
			m_freem(sc->sf_ldata->sf_tx_dlist[i].sf_mbuf);
			sc->sf_ldata->sf_tx_dlist[i].sf_mbuf = NULL;
		}
	}

	ifp->if_flags &= ~(IFF_RUNNING|IFF_OACTIVE);
	SF_UNLOCK(sc);
}

/*
 * Note: it is important that this function not be interrupted. We
 * use a two-stage register access scheme: if we are interrupted in
 * between setting the indirect address register and reading from the
 * indirect data register, the contents of the address register could
 * be changed out from under us.
 */
static void
sf_stats_update(xsc)
	void			*xsc;
{
	struct sf_softc		*sc;
	struct ifnet		*ifp;
	struct mii_data		*mii;
	struct sf_stats		stats;
	u_int32_t		*ptr;
	int			i;

	sc = xsc;
	SF_LOCK(sc);
	ifp = &sc->arpcom.ac_if;
	mii = device_get_softc(sc->sf_miibus);

	ptr = (u_int32_t *)&stats;
	for (i = 0; i < sizeof(stats)/sizeof(u_int32_t); i++)
		ptr[i] = csr_read_4(sc, SF_STATS_BASE +
		    (i + sizeof(u_int32_t)));

	for (i = 0; i < sizeof(stats)/sizeof(u_int32_t); i++)
		csr_write_4(sc, SF_STATS_BASE +
		    (i + sizeof(u_int32_t)), 0);

	ifp->if_collisions += stats.sf_tx_single_colls +
	    stats.sf_tx_multi_colls + stats.sf_tx_excess_colls;

	mii_tick(mii);

	if (!sc->sf_link && mii->mii_media_status & IFM_ACTIVE &&
	    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
		sc->sf_link++;
		if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
			sf_start(ifp);
	}

	sc->sf_stat_ch = timeout(sf_stats_update, sc, hz);

	SF_UNLOCK(sc);
}

static void
sf_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct sf_softc		*sc;

	sc = ifp->if_softc;

	SF_LOCK(sc);

	ifp->if_oerrors++;
	printf("sf%d: watchdog timeout\n", sc->sf_unit);

	sf_stop(sc);
	sf_reset(sc);
	sf_init(sc);

	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		sf_start(ifp);

	SF_UNLOCK(sc);
}

static void
sf_shutdown(dev)
	device_t		dev;
{
	struct sf_softc		*sc;

	sc = device_get_softc(dev);

	sf_stop(sc);
}
