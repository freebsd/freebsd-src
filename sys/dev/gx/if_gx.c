/*-
 * Copyright (c) 1999,2000,2001 Jonathan Lemon
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 *	$FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <net/bpf.h>
#include <net/if_types.h>
#include <net/if_vlan_var.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <vm/vm.h>              /* for vtophys */
#include <vm/pmap.h>            /* for vtophys */
#include <machine/clock.h>      /* for DELAY */
#include <machine/bus_memio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/gx/if_gxreg.h>
#include <dev/gx/if_gxvar.h>

MODULE_DEPEND(gx, miibus, 1, 1, 1);
#include "miibus_if.h"

#define TUNABLE_TX_INTR_DELAY	100
#define TUNABLE_RX_INTR_DELAY	100

#define GX_CSUM_FEATURES	(CSUM_IP | CSUM_TCP | CSUM_UDP | CSUM_IP_FRAGS)

/*
 * Various supported device vendors/types and their names.
 */
struct gx_device {
	u_int16_t	vendor;
	u_int16_t	device;
	int		version_flags;
	u_int32_t	version_ipg;
	char		*name;
};

static struct gx_device gx_devs[] = {
	{ INTEL_VENDORID, DEVICEID_WISEMAN,
	    GXF_FORCE_TBI | GXF_OLD_REGS,
	    10 | 2 << 10 | 10 << 20,
	    "Intel Gigabit Ethernet (82542)" },
	{ INTEL_VENDORID, DEVICEID_LIVINGOOD_FIBER,
	    GXF_DMA | GXF_ENABLE_MWI | GXF_CSUM,
	    6 | 8 << 10 | 6 << 20,
	    "Intel Gigabit Ethernet (82543GC-F)" },
	{ INTEL_VENDORID, DEVICEID_LIVINGOOD_COPPER,
	    GXF_DMA | GXF_ENABLE_MWI | GXF_CSUM,
	    8 | 8 << 10 | 6 << 20,
	    "Intel Gigabit Ethernet (82543GC-T)" },
#if 0
/* notyet.. */
	{ INTEL_VENDORID, DEVICEID_CORDOVA_FIBER,
	    GXF_DMA | GXF_ENABLE_MWI | GXF_CSUM,
	    6 | 8 << 10 | 6 << 20,
	    "Intel Gigabit Ethernet (82544EI-F)" },
	{ INTEL_VENDORID, DEVICEID_CORDOVA_COPPER,
	    GXF_DMA | GXF_ENABLE_MWI | GXF_CSUM,
	    8 | 8 << 10 | 6 << 20,
	    "Intel Gigabit Ethernet (82544EI-T)" },
	{ INTEL_VENDORID, DEVICEID_CORDOVA2_COPPER,
	    GXF_DMA | GXF_ENABLE_MWI | GXF_CSUM,
	    8 | 8 << 10 | 6 << 20,
	    "Intel Gigabit Ethernet (82544GC-T)" },
#endif
	{ 0, 0, 0, 0, NULL }
};

static struct gx_regs new_regs = {
	GX_RX_RING_BASE, GX_RX_RING_LEN,
	GX_RX_RING_HEAD, GX_RX_RING_TAIL,
	GX_RX_INTR_DELAY, GX_RX_DMA_CTRL,

	GX_TX_RING_BASE, GX_TX_RING_LEN,
	GX_TX_RING_HEAD, GX_TX_RING_TAIL,
	GX_TX_INTR_DELAY, GX_TX_DMA_CTRL,
};
static struct gx_regs old_regs = {
	GX_RX_OLD_RING_BASE, GX_RX_OLD_RING_LEN,
	GX_RX_OLD_RING_HEAD, GX_RX_OLD_RING_TAIL,
	GX_RX_OLD_INTR_DELAY, GX_RX_OLD_DMA_CTRL,

	GX_TX_OLD_RING_BASE, GX_TX_OLD_RING_LEN,
	GX_TX_OLD_RING_HEAD, GX_TX_OLD_RING_TAIL,
	GX_TX_OLD_INTR_DELAY, GX_TX_OLD_DMA_CTRL,
};

static int 	gx_probe(device_t dev);
static int 	gx_attach(device_t dev);
static int 	gx_detach(device_t dev);
static void 	gx_shutdown(device_t dev);

static void 	gx_intr(void *xsc);
static void	gx_init(void *xsc);

static struct 	gx_device *gx_match(device_t dev);
static void	gx_eeprom_getword(struct gx_softc *gx, int addr,
		    u_int16_t *dest);
static int	gx_read_eeprom(struct gx_softc *gx, caddr_t dest, int off,
		    int cnt);
static int	gx_ifmedia_upd(struct ifnet *ifp);
static void	gx_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr);
static int 	gx_miibus_readreg(device_t dev, int phy, int reg);
static void 	gx_miibus_writereg(device_t dev, int phy, int reg, int value);
static void 	gx_miibus_statchg(device_t dev);
static int	gx_ioctl(struct ifnet *ifp, u_long command, caddr_t data);
static void	gx_setmulti(struct gx_softc *gx);
static void	gx_reset(struct gx_softc *gx);
static void 	gx_phy_reset(struct gx_softc *gx);
static void 	gx_release(struct gx_softc *gx);
static void	gx_stop(struct gx_softc *gx);
static void	gx_watchdog(struct ifnet *ifp);
static void	gx_start(struct ifnet *ifp);

static int	gx_init_rx_ring(struct gx_softc *gx);
static void	gx_free_rx_ring(struct gx_softc *gx);
static int	gx_init_tx_ring(struct gx_softc *gx);
static void	gx_free_tx_ring(struct gx_softc *gx);

static device_method_t gx_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		gx_probe),
	DEVMETHOD(device_attach,	gx_attach),
	DEVMETHOD(device_detach,	gx_detach),
	DEVMETHOD(device_shutdown,	gx_shutdown),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	gx_miibus_readreg),
	DEVMETHOD(miibus_writereg,	gx_miibus_writereg),
	DEVMETHOD(miibus_statchg,	gx_miibus_statchg),

	{ 0, 0 }
};

static driver_t gx_driver = {
	"gx",
	gx_methods,
	sizeof(struct gx_softc)
};

static devclass_t gx_devclass;

DRIVER_MODULE(if_gx, pci, gx_driver, gx_devclass, 0, 0);
DRIVER_MODULE(miibus, gx, miibus_driver, miibus_devclass, 0, 0);

static struct gx_device *
gx_match(device_t dev)
{
	int i;

	for (i = 0; gx_devs[i].name != NULL; i++) {
		if ((pci_get_vendor(dev) == gx_devs[i].vendor) &&
		    (pci_get_device(dev) == gx_devs[i].device))
			return (&gx_devs[i]);
	}
	return (NULL);
}

static int
gx_probe(device_t dev)
{
	struct gx_device *gx_dev;

	gx_dev = gx_match(dev);
	if (gx_dev == NULL)
		return (ENXIO);

	device_set_desc(dev, gx_dev->name);
	return (0);
}

static int
gx_attach(device_t dev)
{
	struct gx_softc	*gx;
	struct gx_device *gx_dev;
	struct ifnet *ifp;
	u_int32_t command;
	int rid, s;
	int error = 0;

	s = splimp();

	gx = device_get_softc(dev);
	bzero(gx, sizeof(struct gx_softc));
	gx->gx_dev = dev;

	gx_dev = gx_match(dev);
	gx->gx_vflags = gx_dev->version_flags;
	gx->gx_ipg = gx_dev->version_ipg;

	mtx_init(&gx->gx_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF | MTX_RECURSE);

	GX_LOCK(gx);

	/*
	 * Map control/status registers.
	 */
	command = pci_read_config(dev, PCIR_COMMAND, 4);
	command |= PCIM_CMD_MEMEN | PCIM_CMD_BUSMASTEREN;
	if (gx->gx_vflags & GXF_ENABLE_MWI)
		command |= PCIM_CMD_MWIEN;
	pci_write_config(dev, PCIR_COMMAND, command, 4);
	command = pci_read_config(dev, PCIR_COMMAND, 4);

/* XXX check cache line size? */

	if ((command & PCIM_CMD_MEMEN) == 0) {
		device_printf(dev, "failed to enable memory mapping!\n");
		error = ENXIO;
		goto fail;
	}

	rid = GX_PCI_LOMEM;
	gx->gx_res = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid,
	    0, ~0, 1, RF_ACTIVE);
#if 0
/* support PIO mode */
	rid = PCI_LOIO;
	gx->gx_res = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
	    0, ~0, 1, RF_ACTIVE);
#endif

	if (gx->gx_res == NULL) {
		device_printf(dev, "couldn't map memory\n");
		error = ENXIO;
		goto fail;
	}

	gx->gx_btag = rman_get_bustag(gx->gx_res);
	gx->gx_bhandle = rman_get_bushandle(gx->gx_res);

	/* Allocate interrupt */
	rid = 0;
	gx->gx_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
	    RF_SHAREABLE | RF_ACTIVE);

	if (gx->gx_irq == NULL) {
		device_printf(dev, "couldn't map interrupt\n");
		error = ENXIO;
		goto fail;
	}

	error = bus_setup_intr(dev, gx->gx_irq, INTR_TYPE_NET,
	   gx_intr, gx, &gx->gx_intrhand);
	if (error) {
		device_printf(dev, "couldn't setup irq\n");
		goto fail;
	}

	/* compensate for different register mappings */
	if (gx->gx_vflags & GXF_OLD_REGS)
		gx->gx_reg = old_regs;
	else
		gx->gx_reg = new_regs;

	if (gx_read_eeprom(gx, (caddr_t)&gx->arpcom.ac_enaddr,
	    GX_EEMAP_MAC, 3)) {
		device_printf(dev, "failed to read station address\n");
		error = ENXIO;
		goto fail;
	}
	device_printf(dev, "Ethernet address: %6D\n",
	    gx->arpcom.ac_enaddr, ":");

	/* Allocate the ring buffers. */
	gx->gx_rdata = contigmalloc(sizeof(struct gx_ring_data), M_DEVBUF,
	    M_NOWAIT, 0, 0xffffffff, PAGE_SIZE, 0);

	if (gx->gx_rdata == NULL) {
		device_printf(dev, "no memory for list buffers!\n");
		error = ENXIO;
		goto fail;
	}
	bzero(gx->gx_rdata, sizeof(struct gx_ring_data));

	/* Set default tuneable values. */
	gx->gx_tx_intr_delay = TUNABLE_TX_INTR_DELAY;
	gx->gx_rx_intr_delay = TUNABLE_RX_INTR_DELAY;

	/* Set up ifnet structure */
	ifp = &gx->arpcom.ac_if;
	ifp->if_softc = gx;
	ifp->if_unit = device_get_unit(dev);
	ifp->if_name = "gx";
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = gx_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = gx_start;
	ifp->if_watchdog = gx_watchdog;
	ifp->if_init = gx_init;
	ifp->if_mtu = ETHERMTU;
	ifp->if_snd.ifq_maxlen = GX_TX_RING_CNT - 1;
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING;

	/* see if we can enable hardware checksumming */
	if (gx->gx_vflags & GXF_CSUM) {
		ifp->if_capabilities = IFCAP_HWCSUM;
		ifp->if_capenable = ifp->if_capabilities;
	}

	/* figure out transciever type */
	if (gx->gx_vflags & GXF_FORCE_TBI ||
	    CSR_READ_4(gx, GX_STATUS) & GX_STAT_TBIMODE)
		gx->gx_tbimode = 1;

	if (gx->gx_tbimode) {
		/* SERDES transceiver */
		ifmedia_init(&gx->gx_media, IFM_IMASK, gx_ifmedia_upd,
		    gx_ifmedia_sts);
		ifmedia_add(&gx->gx_media,
		    IFM_ETHER|IFM_1000_SX|IFM_FDX, 0, NULL);
		ifmedia_add(&gx->gx_media, IFM_ETHER|IFM_AUTO, 0, NULL);
		ifmedia_set(&gx->gx_media, IFM_ETHER|IFM_AUTO);
	} else {
		/* GMII/MII transceiver */
		gx_phy_reset(gx);
		if (mii_phy_probe(dev, &gx->gx_miibus, gx_ifmedia_upd,
		    gx_ifmedia_sts)) {
			device_printf(dev, "GMII/MII, PHY not detected\n");
			error = ENXIO;
			goto fail;
		}
	}

	/*
	 * Call MI attach routines.
	 */
	ether_ifattach(ifp, gx->arpcom.ac_enaddr);

	GX_UNLOCK(gx);
	splx(s);
	return (0);

fail:
	GX_UNLOCK(gx);
	gx_release(gx);
	splx(s);
	return (error);
}

static void
gx_release(struct gx_softc *gx)
{

	bus_generic_detach(gx->gx_dev);
	if (gx->gx_miibus)
		device_delete_child(gx->gx_dev, gx->gx_miibus);

	if (gx->gx_intrhand)
		bus_teardown_intr(gx->gx_dev, gx->gx_irq, gx->gx_intrhand);
	if (gx->gx_irq)
		bus_release_resource(gx->gx_dev, SYS_RES_IRQ, 0, gx->gx_irq);
	if (gx->gx_res)
		bus_release_resource(gx->gx_dev, SYS_RES_MEMORY,
		    GX_PCI_LOMEM, gx->gx_res);
}

static void
gx_init(void *xsc)
{
	struct gx_softc *gx = (struct gx_softc *)xsc;
	struct ifmedia *ifm;
	struct ifnet *ifp;
	device_t dev;
	u_int16_t *m;
	u_int32_t ctrl;
	int s, i, tmp;

	dev = gx->gx_dev;
	ifp = &gx->arpcom.ac_if;

	s = splimp();
	GX_LOCK(gx);

	/* Disable host interrupts, halt chip. */
	gx_reset(gx);

	/* disable I/O, flush RX/TX FIFOs, and free RX/TX buffers */
	gx_stop(gx);

	/* Load our MAC address, invalidate other 15 RX addresses. */
	m = (u_int16_t *)&gx->arpcom.ac_enaddr[0];
	CSR_WRITE_4(gx, GX_RX_ADDR_BASE, (m[1] << 16) | m[0]);
	CSR_WRITE_4(gx, GX_RX_ADDR_BASE + 4, m[2] | GX_RA_VALID);
	for (i = 1; i < 16; i++)
		CSR_WRITE_8(gx, GX_RX_ADDR_BASE + i * 8, (u_quad_t)0);

	/* Program multicast filter. */
	gx_setmulti(gx);

	/* Init RX ring. */
	gx_init_rx_ring(gx);

	/* Init TX ring. */
	gx_init_tx_ring(gx);

	if (gx->gx_vflags & GXF_DMA) {
		/* set up DMA control */	
		CSR_WRITE_4(gx, gx->gx_reg.r_rx_dma_ctrl, 0x00010000);
		CSR_WRITE_4(gx, gx->gx_reg.r_tx_dma_ctrl, 0x00000000);
	}

	/* enable receiver */
	ctrl = GX_RXC_ENABLE | GX_RXC_RX_THOLD_EIGHTH | GX_RXC_RX_BSIZE_2K;
	ctrl |= GX_RXC_BCAST_ACCEPT;

	/* Enable or disable promiscuous mode as needed. */
	if (ifp->if_flags & IFF_PROMISC)
		ctrl |= GX_RXC_UNI_PROMISC;

	/* This is required if we want to accept jumbo frames */
	if (ifp->if_mtu > ETHERMTU)
		ctrl |= GX_RXC_LONG_PKT_ENABLE;

	/* setup receive checksum control */
	if (ifp->if_capenable & IFCAP_RXCSUM)
		CSR_WRITE_4(gx, GX_RX_CSUM_CONTROL,
		    GX_CSUM_TCP/* | GX_CSUM_IP*/);

	/* setup transmit checksum control */
	if (ifp->if_capenable & IFCAP_TXCSUM)
	        ifp->if_hwassist = GX_CSUM_FEATURES;

	ctrl |= GX_RXC_STRIP_ETHERCRC;		/* not on 82542? */
	CSR_WRITE_4(gx, GX_RX_CONTROL, ctrl);

	/* enable transmitter */
	ctrl = GX_TXC_ENABLE | GX_TXC_PAD_SHORT_PKTS | GX_TXC_COLL_RETRY_16;

	/* XXX we should support half-duplex here too... */
	ctrl |= GX_TXC_COLL_TIME_FDX;

	CSR_WRITE_4(gx, GX_TX_CONTROL, ctrl);

	/*
	 * set up recommended IPG times, which vary depending on chip type:
	 * 	IPG transmit time:  80ns
	 *	IPG receive time 1: 20ns
	 *	IPG receive time 2: 80ns
	 */
	CSR_WRITE_4(gx, GX_TX_IPG, gx->gx_ipg);

	/* set up 802.3x MAC flow control address -- 01:80:c2:00:00:01 */
	CSR_WRITE_4(gx, GX_FLOW_CTRL_BASE, 0x00C28001);
	CSR_WRITE_4(gx, GX_FLOW_CTRL_BASE+4, 0x00000100);

	/* set up 802.3x MAC flow control type -- 88:08 */
	CSR_WRITE_4(gx, GX_FLOW_CTRL_TYPE, 0x8808);

	/* Set up tuneables */
	CSR_WRITE_4(gx, gx->gx_reg.r_rx_delay, gx->gx_rx_intr_delay);
	CSR_WRITE_4(gx, gx->gx_reg.r_tx_delay, gx->gx_tx_intr_delay);

	/*
	 * Configure chip for correct operation.
	 */
	ctrl = GX_CTRL_DUPLEX;
#if BYTE_ORDER == BIG_ENDIAN
	ctrl |= GX_CTRL_BIGENDIAN;
#endif
	ctrl |= GX_CTRL_VLAN_ENABLE;

	if (gx->gx_tbimode) {
		/*
		 * It seems that TXCW must be initialized from the EEPROM
		 * manually.
		 *
		 * XXX
		 * should probably read the eeprom and re-insert the
		 * values here.
		 */
#define TXCONFIG_WORD	0x000001A0
		CSR_WRITE_4(gx, GX_TX_CONFIG, TXCONFIG_WORD);

		/* turn on hardware autonegotiate */
		GX_SETBIT(gx, GX_TX_CONFIG, GX_TXCFG_AUTONEG);
	} else {
		/*
		 * Auto-detect speed from PHY, instead of using direct
		 * indication.  The SLU bit doesn't force the link, but
		 * must be present for ASDE to work.
		 */
		gx_phy_reset(gx);
		ctrl |= GX_CTRL_SET_LINK_UP | GX_CTRL_AUTOSPEED;
	}

	/*
	 * Take chip out of reset and start it running.
	 */
	CSR_WRITE_4(gx, GX_CTRL, ctrl);

	/* Turn interrupts on. */
	CSR_WRITE_4(gx, GX_INT_MASK_SET, GX_INT_WANTED);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	/*
	 * Set the current media.
	 */
	if (gx->gx_miibus != NULL) {
		mii_mediachg(device_get_softc(gx->gx_miibus));
	} else {
		ifm = &gx->gx_media;
		tmp = ifm->ifm_media;
		ifm->ifm_media = ifm->ifm_cur->ifm_media;
		gx_ifmedia_upd(ifp);
		ifm->ifm_media = tmp;
	}

	/*
	 * XXX
	 * Have the LINK0 flag force the link in TBI mode.
	 */
	if (gx->gx_tbimode && ifp->if_flags & IFF_LINK0) {
		GX_CLRBIT(gx, GX_TX_CONFIG, GX_TXCFG_AUTONEG);
		GX_SETBIT(gx, GX_CTRL, GX_CTRL_SET_LINK_UP);
	}

#if 0
printf("66mhz: %s  64bit: %s\n",
	CSR_READ_4(gx, GX_STATUS) & GX_STAT_PCI66 ? "yes" : "no",
	CSR_READ_4(gx, GX_STATUS) & GX_STAT_BUS64 ? "yes" : "no");
#endif

	GX_UNLOCK(gx);
	splx(s);
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
static void
gx_shutdown(device_t dev)
{
	struct gx_softc *gx;

	gx = device_get_softc(dev);
	gx_reset(gx);
	gx_stop(gx);
}

static int
gx_detach(device_t dev)
{
	struct gx_softc *gx;
	struct ifnet *ifp;
	int s;

	s = splimp();

	gx = device_get_softc(dev);
	ifp = &gx->arpcom.ac_if;
	GX_LOCK(gx);

	ether_ifdetach(ifp);
	gx_reset(gx);
	gx_stop(gx);
	ifmedia_removeall(&gx->gx_media);
	gx_release(gx);

	contigfree(gx->gx_rdata, sizeof(struct gx_ring_data), M_DEVBUF);
		
	GX_UNLOCK(gx);
	mtx_destroy(&gx->gx_mtx);
	splx(s);

	return (0);
}

static void
gx_eeprom_getword(struct gx_softc *gx, int addr, u_int16_t *dest)
{
	u_int16_t word = 0;
	u_int32_t base, reg;
	int x;

	addr = (GX_EE_OPC_READ << GX_EE_ADDR_SIZE) |
	    (addr & ((1 << GX_EE_ADDR_SIZE) - 1));

	base = CSR_READ_4(gx, GX_EEPROM_CTRL);
	base &= ~(GX_EE_DATA_OUT | GX_EE_DATA_IN | GX_EE_CLOCK);
	base |= GX_EE_SELECT;

	CSR_WRITE_4(gx, GX_EEPROM_CTRL, base);

	for (x = 1 << ((GX_EE_OPC_SIZE + GX_EE_ADDR_SIZE) - 1); x; x >>= 1) {
		reg = base | (addr & x ? GX_EE_DATA_IN : 0);
		CSR_WRITE_4(gx, GX_EEPROM_CTRL, reg);
		DELAY(10);
		CSR_WRITE_4(gx, GX_EEPROM_CTRL, reg | GX_EE_CLOCK);
		DELAY(10);
		CSR_WRITE_4(gx, GX_EEPROM_CTRL, reg);
		DELAY(10);
	}

	for (x = 1 << 15; x; x >>= 1) {
		CSR_WRITE_4(gx, GX_EEPROM_CTRL, base | GX_EE_CLOCK);
		DELAY(10);
		reg = CSR_READ_4(gx, GX_EEPROM_CTRL);
		if (reg & GX_EE_DATA_OUT)
			word |= x;
		CSR_WRITE_4(gx, GX_EEPROM_CTRL, base);
		DELAY(10);
	}

	CSR_WRITE_4(gx, GX_EEPROM_CTRL, base & ~GX_EE_SELECT);
	DELAY(10);

	*dest = word;
}
	
static int
gx_read_eeprom(struct gx_softc *gx, caddr_t dest, int off, int cnt)
{
	u_int16_t *word;
	int i;

	word = (u_int16_t *)dest;
	for (i = 0; i < cnt; i ++) {
		gx_eeprom_getword(gx, off + i, word);
		word++;
	}
	return (0);
}

/*
 * Set media options.
 */
static int
gx_ifmedia_upd(struct ifnet *ifp)
{
	struct gx_softc	*gx;
	struct ifmedia *ifm;
	struct mii_data *mii;

	gx = ifp->if_softc;

	if (gx->gx_tbimode) {
		ifm = &gx->gx_media;
		if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
			return (EINVAL);
		switch (IFM_SUBTYPE(ifm->ifm_media)) {
		case IFM_AUTO:
			GX_SETBIT(gx, GX_CTRL, GX_CTRL_LINK_RESET);
			GX_SETBIT(gx, GX_TX_CONFIG, GX_TXCFG_AUTONEG);
			GX_CLRBIT(gx, GX_CTRL, GX_CTRL_LINK_RESET);
			break;
		case IFM_1000_SX:
			device_printf(gx->gx_dev,
			    "manual config not supported yet.\n");
#if 0
			GX_CLRBIT(gx, GX_TX_CONFIG, GX_TXCFG_AUTONEG);
			config = /* bit symbols for 802.3z */0;
			ctrl |= GX_CTRL_SET_LINK_UP;
			if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX)
				ctrl |= GX_CTRL_DUPLEX;
#endif
			break;
		default:
			return (EINVAL);
		}
	} else {
		ifm = &gx->gx_media;

		/*
		 * 1000TX half duplex does not work.
		 */
		if (IFM_TYPE(ifm->ifm_media) == IFM_ETHER &&
		    IFM_SUBTYPE(ifm->ifm_media) == IFM_1000_T &&
		    (IFM_OPTIONS(ifm->ifm_media) & IFM_FDX) == 0)
			return (EINVAL);
		mii = device_get_softc(gx->gx_miibus);
		mii_mediachg(mii);
	}
	return (0);
}

/*
 * Report current media status.
 */
static void
gx_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct gx_softc	*gx;
	struct mii_data *mii;
	u_int32_t status;

	gx = ifp->if_softc;

	if (gx->gx_tbimode) {
		ifmr->ifm_status = IFM_AVALID;
		ifmr->ifm_active = IFM_ETHER;

		status = CSR_READ_4(gx, GX_STATUS);
		if ((status & GX_STAT_LINKUP) == 0)
			return;

		ifmr->ifm_status |= IFM_ACTIVE;
		ifmr->ifm_active |= IFM_1000_SX | IFM_FDX;
	} else {
		mii = device_get_softc(gx->gx_miibus);
		mii_pollstat(mii);
		if ((mii->mii_media_active & (IFM_1000_T | IFM_HDX)) ==
		    (IFM_1000_T | IFM_HDX))
			mii->mii_media_active = IFM_ETHER | IFM_NONE;
		ifmr->ifm_active = mii->mii_media_active;
		ifmr->ifm_status = mii->mii_media_status;
	}
}

static void 
gx_mii_shiftin(struct gx_softc *gx, int data, int length)
{
        u_int32_t reg, x;

	/*
	 * Set up default GPIO direction + PHY data out.
	 */
	reg = CSR_READ_4(gx, GX_CTRL);
	reg &= ~(GX_CTRL_GPIO_DIR_MASK | GX_CTRL_PHY_IO | GX_CTRL_PHY_CLK);
	reg |= GX_CTRL_GPIO_DIR | GX_CTRL_PHY_IO_DIR;

        /*
         * Shift in data to PHY.
         */
	for (x = 1 << (length - 1); x; x >>= 1) {
                if (data & x)
                        reg |= GX_CTRL_PHY_IO;
                else
                        reg &= ~GX_CTRL_PHY_IO;
                CSR_WRITE_4(gx, GX_CTRL, reg);
                DELAY(10);
                CSR_WRITE_4(gx, GX_CTRL, reg | GX_CTRL_PHY_CLK);
                DELAY(10);
                CSR_WRITE_4(gx, GX_CTRL, reg);
                DELAY(10);
        }
}

static u_int16_t 
gx_mii_shiftout(struct gx_softc *gx)
{
        u_int32_t reg;
	u_int16_t data;
	int x;

	/*
	 * Set up default GPIO direction + PHY data in.
	 */
	reg = CSR_READ_4(gx, GX_CTRL);
	reg &= ~(GX_CTRL_GPIO_DIR_MASK | GX_CTRL_PHY_IO | GX_CTRL_PHY_CLK);
	reg |= GX_CTRL_GPIO_DIR;

	CSR_WRITE_4(gx, GX_CTRL, reg);
	DELAY(10);
	CSR_WRITE_4(gx, GX_CTRL, reg | GX_CTRL_PHY_CLK);
	DELAY(10);
	CSR_WRITE_4(gx, GX_CTRL, reg);
	DELAY(10);
	/*
	 * Shift out data from PHY.
	 */
	data = 0;
	for (x = 1 << 15; x; x >>= 1) {
		CSR_WRITE_4(gx, GX_CTRL, reg | GX_CTRL_PHY_CLK);
		DELAY(10);
		if (CSR_READ_4(gx, GX_CTRL) & GX_CTRL_PHY_IO)
			data |= x;
		CSR_WRITE_4(gx, GX_CTRL, reg);
		DELAY(10);
	}
	CSR_WRITE_4(gx, GX_CTRL, reg | GX_CTRL_PHY_CLK);
	DELAY(10);
	CSR_WRITE_4(gx, GX_CTRL, reg);
	DELAY(10);

	return (data);
}

static int
gx_miibus_readreg(device_t dev, int phy, int reg)
{
	struct gx_softc *gx;

	gx = device_get_softc(dev);
	if (gx->gx_tbimode)
		return (0);

	/*
	 * XXX
	 * Note: Cordova has a MDIC register. livingood and < have mii bits
	 */ 

	gx_mii_shiftin(gx, GX_PHY_PREAMBLE, GX_PHY_PREAMBLE_LEN);
	gx_mii_shiftin(gx, (GX_PHY_SOF << 12) | (GX_PHY_OP_READ << 10) |
	    (phy << 5) | reg, GX_PHY_READ_LEN);
	return (gx_mii_shiftout(gx));
}

static void
gx_miibus_writereg(device_t dev, int phy, int reg, int value)
{
	struct gx_softc *gx;

	gx = device_get_softc(dev);
	if (gx->gx_tbimode)
		return;

	gx_mii_shiftin(gx, GX_PHY_PREAMBLE, GX_PHY_PREAMBLE_LEN);
	gx_mii_shiftin(gx, (GX_PHY_SOF << 30) | (GX_PHY_OP_WRITE << 28) |
	    (phy << 23) | (reg << 18) | (GX_PHY_TURNAROUND << 16) |
	    (value & 0xffff), GX_PHY_WRITE_LEN);
}

static void
gx_miibus_statchg(device_t dev)
{
	struct gx_softc *gx;
	struct mii_data *mii;
	int reg, s;

	gx = device_get_softc(dev);
	if (gx->gx_tbimode)
		return;

	/*
	 * Set flow control behavior to mirror what PHY negotiated.
	 */
	mii = device_get_softc(gx->gx_miibus);

	s = splimp();
	GX_LOCK(gx);

	reg = CSR_READ_4(gx, GX_CTRL);
	if (mii->mii_media_active & IFM_FLAG0)
		reg |= GX_CTRL_RX_FLOWCTRL;
	else
		reg &= ~GX_CTRL_RX_FLOWCTRL;
	if (mii->mii_media_active & IFM_FLAG1)
		reg |= GX_CTRL_TX_FLOWCTRL;
	else
		reg &= ~GX_CTRL_TX_FLOWCTRL;
	CSR_WRITE_4(gx, GX_CTRL, reg);

	GX_UNLOCK(gx);
	splx(s);
}

static int
gx_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct gx_softc	*gx = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	struct mii_data *mii;
	int s, mask, error = 0;

	s = splimp();
	GX_LOCK(gx);

	switch (command) {
	case SIOCSIFMTU:
		if (ifr->ifr_mtu > GX_MAX_MTU) {
			error = EINVAL;
		} else {
			ifp->if_mtu = ifr->ifr_mtu;
			gx_init(gx);
		}
		break;
	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) == 0) {
			gx_stop(gx);
		} else if (ifp->if_flags & IFF_RUNNING &&
		    ((ifp->if_flags & IFF_PROMISC) != 
		    (gx->gx_if_flags & IFF_PROMISC))) {
			if (ifp->if_flags & IFF_PROMISC)
				GX_SETBIT(gx, GX_RX_CONTROL, GX_RXC_UNI_PROMISC);
			else 
				GX_CLRBIT(gx, GX_RX_CONTROL, GX_RXC_UNI_PROMISC);
		} else {
			gx_init(gx);
		}
		gx->gx_if_flags = ifp->if_flags;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifp->if_flags & IFF_RUNNING)
			gx_setmulti(gx);
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		if (gx->gx_miibus != NULL) {
			mii = device_get_softc(gx->gx_miibus);
			error = ifmedia_ioctl(ifp, ifr,
			    &mii->mii_media, command);
		} else {
			error = ifmedia_ioctl(ifp, ifr, &gx->gx_media, command);
		}
		break;
	case SIOCSIFCAP:
		mask = ifr->ifr_reqcap ^ ifp->if_capenable;
		if (mask & IFCAP_HWCSUM) {
			if (IFCAP_HWCSUM & ifp->if_capenable)
				ifp->if_capenable &= ~IFCAP_HWCSUM;
			else
				ifp->if_capenable |= IFCAP_HWCSUM;
			if (ifp->if_flags & IFF_RUNNING)
				gx_init(gx);
		}
		break;
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	GX_UNLOCK(gx);
	splx(s);
	return (error);
}

static void
gx_phy_reset(struct gx_softc *gx)
{
	int reg;

	GX_SETBIT(gx, GX_CTRL, GX_CTRL_SET_LINK_UP);

	/*
	 * PHY reset is active low.
	 */
	reg = CSR_READ_4(gx, GX_CTRL_EXT);
	reg &= ~(GX_CTRLX_GPIO_DIR_MASK | GX_CTRLX_PHY_RESET);
	reg |= GX_CTRLX_GPIO_DIR;

	CSR_WRITE_4(gx, GX_CTRL_EXT, reg | GX_CTRLX_PHY_RESET);
	DELAY(10);
	CSR_WRITE_4(gx, GX_CTRL_EXT, reg);
	DELAY(10);
	CSR_WRITE_4(gx, GX_CTRL_EXT, reg | GX_CTRLX_PHY_RESET);
	DELAY(10);

#if 0
	/* post-livingood (cordova) only */
		GX_SETBIT(gx, GX_CTRL, 0x80000000);
		DELAY(1000);
		GX_CLRBIT(gx, GX_CTRL, 0x80000000);
#endif
}

static void
gx_reset(struct gx_softc *gx)
{

	/* Disable host interrupts. */
	CSR_WRITE_4(gx, GX_INT_MASK_CLR, GX_INT_ALL);

	/* reset chip (THWAP!) */
	GX_SETBIT(gx, GX_CTRL, GX_CTRL_DEVICE_RESET);
	DELAY(10);
}

static void
gx_stop(struct gx_softc *gx)
{
	struct ifnet *ifp;

	ifp = &gx->arpcom.ac_if;

	/* reset and flush transmitter */
	CSR_WRITE_4(gx, GX_TX_CONTROL, GX_TXC_RESET);

	/* reset and flush receiver */
	CSR_WRITE_4(gx, GX_RX_CONTROL, GX_RXC_RESET);

	/* reset link */
	if (gx->gx_tbimode)
		GX_SETBIT(gx, GX_CTRL, GX_CTRL_LINK_RESET);

	/* Free the RX lists. */
	gx_free_rx_ring(gx);

	/* Free TX buffers. */
	gx_free_tx_ring(gx);

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
}

static void
gx_watchdog(struct ifnet *ifp)
{
	struct gx_softc	*gx;

	gx = ifp->if_softc;

	device_printf(gx->gx_dev, "watchdog timeout -- resetting\n");
	gx_reset(gx);
	gx_init(gx);

	ifp->if_oerrors++;
}

/*
 * Intialize a receive ring descriptor.
 */
static int
gx_newbuf(struct gx_softc *gx, int idx, struct mbuf *m)
{
	struct mbuf *m_new = NULL;
	struct gx_rx_desc *r;

	if (m == NULL) {
		MGETHDR(m_new, M_NOWAIT, MT_DATA);
		if (m_new == NULL) {
			device_printf(gx->gx_dev, 
			    "mbuf allocation failed -- packet dropped\n");
			return (ENOBUFS);
		}
		MCLGET(m_new, M_NOWAIT);
		if ((m_new->m_flags & M_EXT) == 0) {
			device_printf(gx->gx_dev, 
			    "cluster allocation failed -- packet dropped\n");
			m_freem(m_new);
			return (ENOBUFS);
		}
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
	} else {
		m->m_len = m->m_pkthdr.len = MCLBYTES;
		m->m_data = m->m_ext.ext_buf;
		m->m_next = NULL;
		m_new = m;
	}

	/*
	 * XXX
	 * this will _NOT_ work for large MTU's; it will overwrite
	 * the end of the buffer.  E.g.: take this out for jumbograms,
	 * but then that breaks alignment.
	 */
	if (gx->arpcom.ac_if.if_mtu <= ETHERMTU)
		m_adj(m_new, ETHER_ALIGN);

	gx->gx_cdata.gx_rx_chain[idx] = m_new;
	r = &gx->gx_rdata->gx_rx_ring[idx];
	r->rx_addr = vtophys(mtod(m_new, caddr_t));
	r->rx_staterr = 0;

	return (0);
}

/*
 * The receive ring can have up to 64K descriptors, which at 2K per mbuf
 * cluster, could add up to 128M of memory.  Due to alignment constraints,
 * the number of descriptors must be a multiple of 8.  For now, we
 * allocate 256 entries and hope that our CPU is fast enough to keep up
 * with the NIC.
 */
static int
gx_init_rx_ring(struct gx_softc *gx)
{
	int i, error;

	for (i = 0; i < GX_RX_RING_CNT; i++) {
		error = gx_newbuf(gx, i, NULL);
		if (error)
			return (error);
	}

	/* bring receiver out of reset state, leave disabled */
	CSR_WRITE_4(gx, GX_RX_CONTROL, 0);

	/* set up ring registers */
	CSR_WRITE_8(gx, gx->gx_reg.r_rx_base,
	    (u_quad_t)vtophys(gx->gx_rdata->gx_rx_ring));

	CSR_WRITE_4(gx, gx->gx_reg.r_rx_length,
	    GX_RX_RING_CNT * sizeof(struct gx_rx_desc));
	CSR_WRITE_4(gx, gx->gx_reg.r_rx_head, 0);
	CSR_WRITE_4(gx, gx->gx_reg.r_rx_tail, GX_RX_RING_CNT - 1);
	gx->gx_rx_tail_idx = 0;

	return (0);
}

static void
gx_free_rx_ring(struct gx_softc *gx)
{
	struct mbuf **mp;
	int i;

	mp = gx->gx_cdata.gx_rx_chain;
	for (i = 0; i < GX_RX_RING_CNT; i++, mp++) {
		if (*mp != NULL) {
			m_freem(*mp);
			*mp = NULL;
		}
	}
	bzero((void *)gx->gx_rdata->gx_rx_ring,
	    GX_RX_RING_CNT * sizeof(struct gx_rx_desc));

	/* release any partially-received packet chain */
	if (gx->gx_pkthdr != NULL) {
		m_freem(gx->gx_pkthdr);
		gx->gx_pkthdr = NULL;
	}
}

static int
gx_init_tx_ring(struct gx_softc *gx)
{

	/* bring transmitter out of reset state, leave disabled */
	CSR_WRITE_4(gx, GX_TX_CONTROL, 0);

	/* set up ring registers */
	CSR_WRITE_8(gx, gx->gx_reg.r_tx_base,
	    (u_quad_t)vtophys(gx->gx_rdata->gx_tx_ring));
	CSR_WRITE_4(gx, gx->gx_reg.r_tx_length,
	    GX_TX_RING_CNT * sizeof(struct gx_tx_desc));
	CSR_WRITE_4(gx, gx->gx_reg.r_tx_head, 0);
	CSR_WRITE_4(gx, gx->gx_reg.r_tx_tail, 0);
	gx->gx_tx_head_idx = 0;
	gx->gx_tx_tail_idx = 0;
	gx->gx_txcnt = 0;

	/* set up initial TX context */
	gx->gx_txcontext = GX_TXCONTEXT_NONE;

	return (0);
}

static void
gx_free_tx_ring(struct gx_softc *gx)
{
	struct mbuf **mp;
	int i;

	mp = gx->gx_cdata.gx_tx_chain;
	for (i = 0; i < GX_TX_RING_CNT; i++, mp++) {
		if (*mp != NULL) {
			m_freem(*mp);
			*mp = NULL;
		}
	}
	bzero((void *)&gx->gx_rdata->gx_tx_ring,
	    GX_TX_RING_CNT * sizeof(struct gx_tx_desc));
}

static void
gx_setmulti(struct gx_softc *gx)
{
	int i;

	/* wipe out the multicast table */
	for (i = 1; i < 128; i++)
		CSR_WRITE_4(gx, GX_MULTICAST_BASE + i * 4, 0);
}

static void
gx_rxeof(struct gx_softc *gx)
{
	struct gx_rx_desc *rx;
	struct ifnet *ifp;
	int idx, staterr, len;
	struct mbuf *m;

	gx->gx_rx_interrupts++;

	ifp = &gx->arpcom.ac_if;
	idx = gx->gx_rx_tail_idx;

	while (gx->gx_rdata->gx_rx_ring[idx].rx_staterr & GX_RXSTAT_COMPLETED) {

		rx = &gx->gx_rdata->gx_rx_ring[idx];
		m = gx->gx_cdata.gx_rx_chain[idx];
		/*
		 * gx_newbuf overwrites status and length bits, so we 
		 * make a copy of them here.
		 */
		len = rx->rx_len;
		staterr = rx->rx_staterr;

		if (staterr & GX_INPUT_ERROR)
			goto ierror;

		if (gx_newbuf(gx, idx, NULL) == ENOBUFS)
			goto ierror;

		GX_INC(idx, GX_RX_RING_CNT);

		if (staterr & GX_RXSTAT_INEXACT_MATCH) {
			/*
			 * multicast packet, must verify against
			 * multicast address.
			 */
		}

		if ((staterr & GX_RXSTAT_END_OF_PACKET) == 0) {
			if (gx->gx_pkthdr == NULL) {
				m->m_len = len;
				m->m_pkthdr.len = len;
				gx->gx_pkthdr = m;
				gx->gx_pktnextp = &m->m_next;
			} else {
				m->m_len = len;
				m->m_flags &= ~M_PKTHDR;
				gx->gx_pkthdr->m_pkthdr.len += len;
				*(gx->gx_pktnextp) = m;
				gx->gx_pktnextp = &m->m_next;
			}
			continue;
		}

		if (gx->gx_pkthdr == NULL) {
			m->m_len = len;
			m->m_pkthdr.len = len;
		} else {
			m->m_len = len;
			m->m_flags &= ~M_PKTHDR;
			gx->gx_pkthdr->m_pkthdr.len += len;
			*(gx->gx_pktnextp) = m;
			m = gx->gx_pkthdr;
			gx->gx_pkthdr = NULL;
		}

		ifp->if_ipackets++;
		m->m_pkthdr.rcvif = ifp;

#define IP_CSMASK 	(GX_RXSTAT_IGNORE_CSUM | GX_RXSTAT_HAS_IP_CSUM)
#define TCP_CSMASK \
    (GX_RXSTAT_IGNORE_CSUM | GX_RXSTAT_HAS_TCP_CSUM | GX_RXERR_TCP_CSUM)
		if (ifp->if_capenable & IFCAP_RXCSUM) {
#if 0
			/*
			 * Intel Erratum #23 indicates that the Receive IP
			 * Checksum offload feature has been completely
			 * disabled.
			 */
			if ((staterr & IP_CSUM_MASK) == GX_RXSTAT_HAS_IP_CSUM) {
				m->m_pkthdr.csum_flags |= CSUM_IP_CHECKED;
				if ((staterr & GX_RXERR_IP_CSUM) == 0)
					m->m_pkthdr.csum_flags |= CSUM_IP_VALID;
			}
#endif
			if ((staterr & TCP_CSMASK) == GX_RXSTAT_HAS_TCP_CSUM) {
				m->m_pkthdr.csum_flags |=
				    CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
				m->m_pkthdr.csum_data = 0xffff;
			}
		}
		/*
		 * If we received a packet with a vlan tag,
		 * mark the packet before it's passed up.
		 */
		if (staterr & GX_RXSTAT_VLAN_PKT) {
			VLAN_INPUT_TAG(ifp, m, rx->rx_special, continue);
		}
		(*ifp->if_input)(ifp, m);
		continue;

  ierror:
		ifp->if_ierrors++;
		gx_newbuf(gx, idx, m);

		/* 
		 * XXX
		 * this isn't quite right.  Suppose we have a packet that
		 * spans 5 descriptors (9K split into 2K buffers).  If
		 * the 3rd descriptor sets an error, we need to ignore
		 * the last two.  The way things stand now, the last two
		 * will be accepted as a single packet.
		 *
		 * we don't worry about this -- the chip may not set an
		 * error in this case, and the checksum of the upper layers
		 * will catch the error.
		 */
		if (gx->gx_pkthdr != NULL) {
			m_freem(gx->gx_pkthdr);
			gx->gx_pkthdr = NULL;
		}
		GX_INC(idx, GX_RX_RING_CNT);
	}

	gx->gx_rx_tail_idx = idx;
	if (--idx < 0)
		idx = GX_RX_RING_CNT - 1;
	CSR_WRITE_4(gx, gx->gx_reg.r_rx_tail, idx);
}

static void
gx_txeof(struct gx_softc *gx)
{
	struct ifnet *ifp;
	int idx, cnt;

	gx->gx_tx_interrupts++;

	ifp = &gx->arpcom.ac_if;
	idx = gx->gx_tx_head_idx;
	cnt = gx->gx_txcnt;

	/*
	 * If the system chipset performs I/O write buffering, it is 
	 * possible for the PIO read of the head descriptor to bypass the
	 * memory write of the descriptor, resulting in reading a descriptor
	 * which has not been updated yet.
	 */
	while (cnt) {
		struct gx_tx_desc_old *tx;

		tx = (struct gx_tx_desc_old *)&gx->gx_rdata->gx_tx_ring[idx];
		cnt--;

		if ((tx->tx_command & GX_TXOLD_END_OF_PKT) == 0) {
			GX_INC(idx, GX_TX_RING_CNT);
			continue;
		}

		if ((tx->tx_status & GX_TXSTAT_DONE) == 0)
			break;

		ifp->if_opackets++;

		m_freem(gx->gx_cdata.gx_tx_chain[idx]);
		gx->gx_cdata.gx_tx_chain[idx] = NULL;
		gx->gx_txcnt = cnt;
		ifp->if_timer = 0;

		GX_INC(idx, GX_TX_RING_CNT);
		gx->gx_tx_head_idx = idx;
	}

	if (gx->gx_txcnt == 0)
		ifp->if_flags &= ~IFF_OACTIVE;
}

static void
gx_intr(void *xsc)
{
	struct gx_softc	*gx;
	struct ifnet *ifp;
	u_int32_t intr;
	int s;

	gx = xsc;
	ifp = &gx->arpcom.ac_if;

	s = splimp();

	gx->gx_interrupts++;

	/* Disable host interrupts. */
	CSR_WRITE_4(gx, GX_INT_MASK_CLR, GX_INT_ALL);

	/*
	 * find out why we're being bothered.
	 * reading this register automatically clears all bits.
	 */
	intr = CSR_READ_4(gx, GX_INT_READ);

	/* Check RX return ring producer/consumer */
	if (intr & (GX_INT_RCV_TIMER | GX_INT_RCV_THOLD | GX_INT_RCV_OVERRUN))
		gx_rxeof(gx);

	/* Check TX ring producer/consumer */
	if (intr & (GX_INT_XMIT_DONE | GX_INT_XMIT_EMPTY))
		gx_txeof(gx);

	/*
	 * handle other interrupts here.
	 */

	/*
	 * Link change interrupts are not reliable; the interrupt may
	 * not be generated if the link is lost.  However, the register
	 * read is reliable, so check that.  Use SEQ errors to possibly
	 * indicate that the link has changed.
	 */
	if (intr & GX_INT_LINK_CHANGE) {
		if ((CSR_READ_4(gx, GX_STATUS) & GX_STAT_LINKUP) == 0) {
			device_printf(gx->gx_dev, "link down\n");
		} else {
			device_printf(gx->gx_dev, "link up\n");
		}
	}

	/* Turn interrupts on. */
	CSR_WRITE_4(gx, GX_INT_MASK_SET, GX_INT_WANTED);

	if (ifp->if_flags & IFF_RUNNING && ifp->if_snd.ifq_head != NULL)
		gx_start(ifp);

	splx(s);
}

/*
 * Encapsulate an mbuf chain in the tx ring by coupling the mbuf data
 * pointers to descriptors.
 */
static int
gx_encap(struct gx_softc *gx, struct mbuf *m_head)
{
	struct gx_tx_desc_data *tx = NULL;
	struct gx_tx_desc_ctx *tctx;
	struct mbuf *m;
	int idx, cnt, csumopts, txcontext;
	struct m_tag *mtag;

	cnt = gx->gx_txcnt;
	idx = gx->gx_tx_tail_idx;
	txcontext = gx->gx_txcontext;

	/*
	 * Insure we have at least 4 descriptors pre-allocated.
	 */
	if (cnt >= GX_TX_RING_CNT - 4)
		return (ENOBUFS);

	/*
	 * Set up the appropriate offload context if necessary.
	 */
	csumopts = 0;
	if (m_head->m_pkthdr.csum_flags) {
		if (m_head->m_pkthdr.csum_flags & CSUM_IP)
			csumopts |= GX_TXTCP_OPT_IP_CSUM;
		if (m_head->m_pkthdr.csum_flags & CSUM_TCP) {
			csumopts |= GX_TXTCP_OPT_TCP_CSUM;
			txcontext = GX_TXCONTEXT_TCPIP;
		} else if (m_head->m_pkthdr.csum_flags & CSUM_UDP) {
			csumopts |= GX_TXTCP_OPT_TCP_CSUM;
			txcontext = GX_TXCONTEXT_UDPIP;
		} else if (txcontext == GX_TXCONTEXT_NONE)
			txcontext = GX_TXCONTEXT_TCPIP;
		if (txcontext == gx->gx_txcontext)
			goto context_done;

		tctx = (struct gx_tx_desc_ctx *)&gx->gx_rdata->gx_tx_ring[idx];
		tctx->tx_ip_csum_start = ETHER_HDR_LEN;
		tctx->tx_ip_csum_end = ETHER_HDR_LEN + sizeof(struct ip) - 1;
		tctx->tx_ip_csum_offset = 
		    ETHER_HDR_LEN + offsetof(struct ip, ip_sum);
		tctx->tx_tcp_csum_start = ETHER_HDR_LEN + sizeof(struct ip);
		tctx->tx_tcp_csum_end = 0;
		if (txcontext == GX_TXCONTEXT_TCPIP)
			tctx->tx_tcp_csum_offset = ETHER_HDR_LEN +
			    sizeof(struct ip) + offsetof(struct tcphdr, th_sum);
		else
			tctx->tx_tcp_csum_offset = ETHER_HDR_LEN +
			    sizeof(struct ip) + offsetof(struct udphdr, uh_sum);
		tctx->tx_command = GX_TXCTX_EXTENSION | GX_TXCTX_INT_DELAY;
		tctx->tx_type = 0;
		tctx->tx_status = 0;
		GX_INC(idx, GX_TX_RING_CNT);
		cnt++;
	}
context_done:
	/*
 	 * Start packing the mbufs in this chain into the transmit
	 * descriptors.  Stop when we run out of descriptors or hit
	 * the end of the mbuf chain.
	 */
	for (m = m_head; m != NULL; m = m->m_next) {
		if (m->m_len == 0)
			continue;

		if (cnt == GX_TX_RING_CNT) {
printf("overflow(2): %d, %d\n", cnt, GX_TX_RING_CNT);
			return (ENOBUFS);
}

		tx = (struct gx_tx_desc_data *)&gx->gx_rdata->gx_tx_ring[idx];
		tx->tx_addr = vtophys(mtod(m, vm_offset_t));
		tx->tx_status = 0;
		tx->tx_len = m->m_len;
		if (gx->arpcom.ac_if.if_hwassist) {
			tx->tx_type = 1;
			tx->tx_command = GX_TXTCP_EXTENSION;
			tx->tx_options = csumopts;
		} else {
			/*
			 * This is really a struct gx_tx_desc_old.
			 */
			tx->tx_command = 0;
		}
		GX_INC(idx, GX_TX_RING_CNT);
		cnt++;
	}

	if (tx != NULL) {
		tx->tx_command |= GX_TXTCP_REPORT_STATUS | GX_TXTCP_INT_DELAY |
		    GX_TXTCP_ETHER_CRC | GX_TXTCP_END_OF_PKT;
		mtag = VLAN_OUTPUT_TAG(&gx->arpcom.ac_if, m);
		if (mtag != NULL) {
			tx->tx_command |= GX_TXTCP_VLAN_ENABLE;
			tx->tx_vlan = VLAN_TAG_VALUE(mtag);
		}
		gx->gx_txcnt = cnt;
		gx->gx_tx_tail_idx = idx;
		gx->gx_txcontext = txcontext;
		idx = GX_PREV(idx, GX_TX_RING_CNT);
		gx->gx_cdata.gx_tx_chain[idx] = m_head;

		CSR_WRITE_4(gx, gx->gx_reg.r_tx_tail, gx->gx_tx_tail_idx);
	}
	
	return (0);
}
 
/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit descriptors.
 */
static void
gx_start(struct ifnet *ifp)
{
	struct gx_softc	*gx;
	struct mbuf *m_head;
	int s;

	s = splimp();

	gx = ifp->if_softc;

	for (;;) {
		IF_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		/*
		 * Pack the data into the transmit ring. If we
		 * don't have room, set the OACTIVE flag and wait
		 * for the NIC to drain the ring.
		 */
		if (gx_encap(gx, m_head) != 0) {
			IF_PREPEND(&ifp->if_snd, m_head);
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		BPF_MTAP(ifp, m_head);

		/*
		 * Set a timeout in case the chip goes out to lunch.
		 */
		ifp->if_timer = 5;
	}

	splx(s);
}
