/*-
 * Copyright (c) 1995, David Greenman
 * Copyright (c) 2001 Jonathan Lemon <jlemon@freebsd.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Intel EtherExpress Pro/100B PCI Fast Ethernet driver
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/endian.h>
#include <sys/mbuf.h>
		/* #include <sys/mutex.h> */
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <net/bpf.h>
#include <sys/sockio.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <net/ethernet.h>
#include <net/if_arp.h>

#include <machine/clock.h>	/* for DELAY */

#include <net/if_types.h>
#include <net/if_vlan_var.h>

#ifdef FXP_IP_CSUM_WAR
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <machine/in_cksum.h>
#endif

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>		/* for PCIM_CMD_xxx */

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/fxp/if_fxpreg.h>
#include <dev/fxp/if_fxpvar.h>
#include <dev/fxp/rcvbundl.h>

MODULE_DEPEND(fxp, pci, 1, 1, 1);
MODULE_DEPEND(fxp, ether, 1, 1, 1);
MODULE_DEPEND(fxp, miibus, 1, 1, 1);
#include "miibus_if.h"

/*
 * NOTE!  On the Alpha, we have an alignment constraint.  The
 * card DMAs the packet immediately following the RFA.  However,
 * the first thing in the packet is a 14-byte Ethernet header.
 * This means that the packet is misaligned.  To compensate,
 * we actually offset the RFA 2 bytes into the cluster.  This
 * alignes the packet after the Ethernet header at a 32-bit
 * boundary.  HOWEVER!  This means that the RFA is misaligned!
 */
#define	RFA_ALIGNMENT_FUDGE	2

/*
 * Set initial transmit threshold at 64 (512 bytes). This is
 * increased by 64 (512 bytes) at a time, to maximum of 192
 * (1536 bytes), if an underrun occurs.
 */
static int tx_threshold = 64;

/*
 * The configuration byte map has several undefined fields which
 * must be one or must be zero.  Set up a template for these bits
 * only, (assuming a 82557 chip) leaving the actual configuration
 * to fxp_init.
 *
 * See struct fxp_cb_config for the bit definitions.
 */
static u_char fxp_cb_config_template[] = {
	0x0, 0x0,		/* cb_status */
	0x0, 0x0,		/* cb_command */
	0x0, 0x0, 0x0, 0x0,	/* link_addr */
	0x0,	/*  0 */
	0x0,	/*  1 */
	0x0,	/*  2 */
	0x0,	/*  3 */
	0x0,	/*  4 */
	0x0,	/*  5 */
	0x32,	/*  6 */
	0x0,	/*  7 */
	0x0,	/*  8 */
	0x0,	/*  9 */
	0x6,	/* 10 */
	0x0,	/* 11 */
	0x0,	/* 12 */
	0x0,	/* 13 */
	0xf2,	/* 14 */
	0x48,	/* 15 */
	0x0,	/* 16 */
	0x40,	/* 17 */
	0xf0,	/* 18 */
	0x0,	/* 19 */
	0x3f,	/* 20 */
	0x5	/* 21 */
};

struct fxp_ident {
	u_int16_t	devid;
	int16_t		revid;		/* -1 matches anything */
	char 		*name;
};

/*
 * Claim various Intel PCI device identifiers for this driver.  The
 * sub-vendor and sub-device field are extensively used to identify
 * particular variants, but we don't currently differentiate between
 * them.
 */
static struct fxp_ident fxp_ident_table[] = {
    { 0x1029,	-1,	"Intel 82559 PCI/CardBus Pro/100" },
    { 0x1030,	-1,	"Intel 82559 Pro/100 Ethernet" },
    { 0x1031,	-1,	"Intel 82801CAM (ICH3) Pro/100 VE Ethernet" },
    { 0x1032,	-1,	"Intel 82801CAM (ICH3) Pro/100 VE Ethernet" },
    { 0x1033,	-1,	"Intel 82801CAM (ICH3) Pro/100 VM Ethernet" },
    { 0x1034,	-1,	"Intel 82801CAM (ICH3) Pro/100 VM Ethernet" },
    { 0x1035,	-1,	"Intel 82801CAM (ICH3) Pro/100 Ethernet" },
    { 0x1036,	-1,	"Intel 82801CAM (ICH3) Pro/100 Ethernet" },
    { 0x1037,	-1,	"Intel 82801CAM (ICH3) Pro/100 Ethernet" },
    { 0x1038,	-1,	"Intel 82801CAM (ICH3) Pro/100 VM Ethernet" },
    { 0x1039,	-1,	"Intel 82801DB (ICH4) Pro/100 VE Ethernet" },
    { 0x103A,	-1,	"Intel 82801DB (ICH4) Pro/100 Ethernet" },
    { 0x103B,	-1,	"Intel 82801DB (ICH4) Pro/100 VM Ethernet" },
    { 0x103C,	-1,	"Intel 82801DB (ICH4) Pro/100 Ethernet" },
    { 0x103D,	-1,	"Intel 82801DB (ICH4) Pro/100 VE Ethernet" },
    { 0x103E,	-1,	"Intel 82801DB (ICH4) Pro/100 VM Ethernet" },
    { 0x1050,	-1,	"Intel 82801BA (D865) Pro/100 VE Ethernet" },
    { 0x1051,	-1,	"Intel 82562ET (ICH5/ICH5R) Pro/100 VE Ethernet" },
    { 0x1059,	-1,	"Intel 82551QM Pro/100 M Mobile Connection" },
    { 0x1064,	-1,	"Intel 82562EZ (ICH6)" },
    { 0x1209,	-1,	"Intel 82559ER Embedded 10/100 Ethernet" },
    { 0x1229,	0x01,	"Intel 82557 Pro/100 Ethernet" },
    { 0x1229,	0x02,	"Intel 82557 Pro/100 Ethernet" },
    { 0x1229,	0x03,	"Intel 82557 Pro/100 Ethernet" },
    { 0x1229,	0x04,	"Intel 82558 Pro/100 Ethernet" },
    { 0x1229,	0x05,	"Intel 82558 Pro/100 Ethernet" },
    { 0x1229,	0x06,	"Intel 82559 Pro/100 Ethernet" },
    { 0x1229,	0x07,	"Intel 82559 Pro/100 Ethernet" },
    { 0x1229,	0x08,	"Intel 82559 Pro/100 Ethernet" },
    { 0x1229,	0x09,	"Intel 82559ER Pro/100 Ethernet" },
    { 0x1229,	0x0c,	"Intel 82550 Pro/100 Ethernet" },
    { 0x1229,	0x0d,	"Intel 82550 Pro/100 Ethernet" },
    { 0x1229,	0x0e,	"Intel 82550 Pro/100 Ethernet" },
    { 0x1229,	0x0f,	"Intel 82551 Pro/100 Ethernet" },
    { 0x1229,	0x10,	"Intel 82551 Pro/100 Ethernet" },
    { 0x1229,	-1,	"Intel 82557/8/9 Pro/100 Ethernet" },
    { 0x2449,	-1,	"Intel 82801BA/CAM (ICH2/3) Pro/100 Ethernet" },
    { 0,	-1,	NULL },
};

#ifdef FXP_IP_CSUM_WAR
#define FXP_CSUM_FEATURES    (CSUM_IP | CSUM_TCP | CSUM_UDP)
#else
#define FXP_CSUM_FEATURES    (CSUM_TCP | CSUM_UDP)
#endif

static int		fxp_probe(device_t dev);
static int		fxp_attach(device_t dev);
static int		fxp_detach(device_t dev);
static int		fxp_shutdown(device_t dev);
static int		fxp_suspend(device_t dev);
static int		fxp_resume(device_t dev);

static void		fxp_intr(void *xsc);
static void		fxp_intr_body(struct fxp_softc *sc, struct ifnet *ifp,
			    u_int8_t statack, int count);
static void 		fxp_init(void *xsc);
static void 		fxp_init_body(struct fxp_softc *sc);
static void 		fxp_tick(void *xsc);
static void 		fxp_start(struct ifnet *ifp);
static void 		fxp_start_body(struct ifnet *ifp);
static void		fxp_stop(struct fxp_softc *sc);
static void 		fxp_release(struct fxp_softc *sc);
static int		fxp_ioctl(struct ifnet *ifp, u_long command,
			    caddr_t data);
static void 		fxp_watchdog(struct ifnet *ifp);
static int		fxp_add_rfabuf(struct fxp_softc *sc,
    			    struct fxp_rx *rxp);
static int		fxp_mc_addrs(struct fxp_softc *sc);
static void		fxp_mc_setup(struct fxp_softc *sc);
static u_int16_t	fxp_eeprom_getword(struct fxp_softc *sc, int offset,
			    int autosize);
static void 		fxp_eeprom_putword(struct fxp_softc *sc, int offset,
			    u_int16_t data);
static void		fxp_autosize_eeprom(struct fxp_softc *sc);
static void		fxp_read_eeprom(struct fxp_softc *sc, u_short *data,
			    int offset, int words);
static void		fxp_write_eeprom(struct fxp_softc *sc, u_short *data,
			    int offset, int words);
static int		fxp_ifmedia_upd(struct ifnet *ifp);
static void		fxp_ifmedia_sts(struct ifnet *ifp,
			    struct ifmediareq *ifmr);
static int		fxp_serial_ifmedia_upd(struct ifnet *ifp);
static void		fxp_serial_ifmedia_sts(struct ifnet *ifp,
			    struct ifmediareq *ifmr);
static volatile int	fxp_miibus_readreg(device_t dev, int phy, int reg);
static void		fxp_miibus_writereg(device_t dev, int phy, int reg,
			    int value);
static void		fxp_load_ucode(struct fxp_softc *sc);
static int		sysctl_int_range(SYSCTL_HANDLER_ARGS,
			    int low, int high);
static int		sysctl_hw_fxp_bundle_max(SYSCTL_HANDLER_ARGS);
static int		sysctl_hw_fxp_int_delay(SYSCTL_HANDLER_ARGS);
static void 		fxp_scb_wait(struct fxp_softc *sc);
static void		fxp_scb_cmd(struct fxp_softc *sc, int cmd);
static void		fxp_dma_wait(struct fxp_softc *sc,
    			    volatile u_int16_t *status, bus_dma_tag_t dmat,
			    bus_dmamap_t map);

static device_method_t fxp_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		fxp_probe),
	DEVMETHOD(device_attach,	fxp_attach),
	DEVMETHOD(device_detach,	fxp_detach),
	DEVMETHOD(device_shutdown,	fxp_shutdown),
	DEVMETHOD(device_suspend,	fxp_suspend),
	DEVMETHOD(device_resume,	fxp_resume),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	fxp_miibus_readreg),
	DEVMETHOD(miibus_writereg,	fxp_miibus_writereg),

	{ 0, 0 }
};

static driver_t fxp_driver = {
	"fxp",
	fxp_methods,
	sizeof(struct fxp_softc),
};

static devclass_t fxp_devclass;

DRIVER_MODULE(fxp, pci, fxp_driver, fxp_devclass, 0, 0);
DRIVER_MODULE(fxp, cardbus, fxp_driver, fxp_devclass, 0, 0);
DRIVER_MODULE(miibus, fxp, miibus_driver, miibus_devclass, 0, 0);

/*
 * Wait for the previous command to be accepted (but not necessarily
 * completed).
 */
static void
fxp_scb_wait(struct fxp_softc *sc)
{
	int i = 10000;

	while (CSR_READ_1(sc, FXP_CSR_SCB_COMMAND) && --i)
		DELAY(2);
	if (i == 0)
		device_printf(sc->dev, "SCB timeout: 0x%x 0x%x 0x%x 0x%x\n",
		    CSR_READ_1(sc, FXP_CSR_SCB_COMMAND),
		    CSR_READ_1(sc, FXP_CSR_SCB_STATACK),
		    CSR_READ_1(sc, FXP_CSR_SCB_RUSCUS),
		    CSR_READ_2(sc, FXP_CSR_FLOWCONTROL));
}

static void
fxp_scb_cmd(struct fxp_softc *sc, int cmd)
{

	if (cmd == FXP_SCB_COMMAND_CU_RESUME && sc->cu_resume_bug) {
		CSR_WRITE_1(sc, FXP_CSR_SCB_COMMAND, FXP_CB_COMMAND_NOP);
		fxp_scb_wait(sc);
	}
	CSR_WRITE_1(sc, FXP_CSR_SCB_COMMAND, cmd);
}

static void
fxp_dma_wait(struct fxp_softc *sc, volatile u_int16_t *status,
    bus_dma_tag_t dmat, bus_dmamap_t map)
{
	int i = 10000;

	bus_dmamap_sync(dmat, map, BUS_DMASYNC_POSTREAD);
	while (!(le16toh(*status) & FXP_CB_STATUS_C) && --i) {
		DELAY(2);
		bus_dmamap_sync(dmat, map, BUS_DMASYNC_POSTREAD);
	}
	if (i == 0)
		device_printf(sc->dev, "DMA timeout\n");
}

/*
 * Return identification string if this device is ours.
 */
static int
fxp_probe(device_t dev)
{
	u_int16_t devid;
	u_int8_t revid;
	struct fxp_ident *ident;

	if (pci_get_vendor(dev) == FXP_VENDORID_INTEL) {
		devid = pci_get_device(dev);
		revid = pci_get_revid(dev);
		for (ident = fxp_ident_table; ident->name != NULL; ident++) {
			if (ident->devid == devid &&
			    (ident->revid == revid || ident->revid == -1)) {
				device_set_desc(dev, ident->name);
				return (0);
			}
		}
	}
	return (ENXIO);
}

static void
fxp_dma_map_addr(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	u_int32_t *addr;

	if (error)
		return;

	KASSERT(nseg == 1, ("too many DMA segments, %d should be 1", nseg));
	addr = arg;
	*addr = segs->ds_addr;
}

static int
fxp_attach(device_t dev)
{
	int error = 0;
	struct fxp_softc *sc = device_get_softc(dev);
	struct ifnet *ifp;
	struct fxp_rx *rxp;
	u_int32_t val;
	u_int16_t data, myea[ETHER_ADDR_LEN / 2];
	int i, rid, m1, m2, prefer_iomap, maxtxseg;
	int s;

	sc->dev = dev;
	callout_init(&sc->stat_ch, CALLOUT_MPSAFE);
	mtx_init(&sc->sc_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF);
	ifmedia_init(&sc->sc_media, 0, fxp_serial_ifmedia_upd,
	    fxp_serial_ifmedia_sts);

	s = splimp();

	/*
	 * Enable bus mastering.
	 */
	pci_enable_busmaster(dev);
	val = pci_read_config(dev, PCIR_COMMAND, 2);

	/*
	 * Figure out which we should try first - memory mapping or i/o mapping?
	 * We default to memory mapping. Then we accept an override from the
	 * command line. Then we check to see which one is enabled.
	 */
	m1 = PCIM_CMD_MEMEN;
	m2 = PCIM_CMD_PORTEN;
	prefer_iomap = 0;
	if (resource_int_value(device_get_name(dev), device_get_unit(dev),
	    "prefer_iomap", &prefer_iomap) == 0 && prefer_iomap != 0) {
		m1 = PCIM_CMD_PORTEN;
		m2 = PCIM_CMD_MEMEN;
	}

	sc->rtp = (m1 == PCIM_CMD_MEMEN)? SYS_RES_MEMORY : SYS_RES_IOPORT;
	sc->rgd = (m1 == PCIM_CMD_MEMEN)? FXP_PCI_MMBA : FXP_PCI_IOBA;
	sc->mem = bus_alloc_resource_any(dev, sc->rtp, &sc->rgd, RF_ACTIVE);
	if (sc->mem == NULL) {
		sc->rtp =
		    (m2 == PCIM_CMD_MEMEN)? SYS_RES_MEMORY : SYS_RES_IOPORT;
		sc->rgd = (m2 == PCIM_CMD_MEMEN)? FXP_PCI_MMBA : FXP_PCI_IOBA;
		sc->mem = bus_alloc_resource_any(dev, sc->rtp, &sc->rgd,
                                            RF_ACTIVE);
	}

	if (!sc->mem) {
		error = ENXIO;
		goto fail;
        }
	if (bootverbose) {
		device_printf(dev, "using %s space register mapping\n",
		   sc->rtp == SYS_RES_MEMORY? "memory" : "I/O");
	}

	sc->sc_st = rman_get_bustag(sc->mem);
	sc->sc_sh = rman_get_bushandle(sc->mem);

	/*
	 * Allocate our interrupt.
	 */
	rid = 0;
	sc->irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
				 RF_SHAREABLE | RF_ACTIVE);
	if (sc->irq == NULL) {
		device_printf(dev, "could not map interrupt\n");
		error = ENXIO;
		goto fail;
	}

	/*
	 * Reset to a stable state.
	 */
	CSR_WRITE_4(sc, FXP_CSR_PORT, FXP_PORT_SELECTIVE_RESET);
	DELAY(10);

	/*
	 * Find out how large of an SEEPROM we have.
	 */
	fxp_autosize_eeprom(sc);

	/*
	 * Determine whether we must use the 503 serial interface.
	 */
	fxp_read_eeprom(sc, &data, 6, 1);
	if (sc->revision <= FXP_REV_82557 && (data & FXP_PHY_DEVICE_MASK) != 0
	    && (data & FXP_PHY_SERIAL_ONLY))
		sc->flags |= FXP_FLAG_SERIAL_MEDIA;

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "int_delay", CTLTYPE_INT | CTLFLAG_RW,
	    &sc->tunable_int_delay, 0, sysctl_hw_fxp_int_delay, "I",
	    "FXP driver receive interrupt microcode bundling delay");
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "bundle_max", CTLTYPE_INT | CTLFLAG_RW,
	    &sc->tunable_bundle_max, 0, sysctl_hw_fxp_bundle_max, "I",
	    "FXP driver receive interrupt microcode bundle size limit");
	SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "rnr", CTLFLAG_RD, &sc->rnr, 0,
	    "FXP RNR events");
	SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "noflow", CTLFLAG_RW, &sc->tunable_noflow, 0,
	    "FXP flow control disabled");

	/*
	 * Pull in device tunables.
	 */
	sc->tunable_int_delay = TUNABLE_INT_DELAY;
	sc->tunable_bundle_max = TUNABLE_BUNDLE_MAX;
	sc->tunable_noflow = 1;
	(void) resource_int_value(device_get_name(dev), device_get_unit(dev),
	    "int_delay", &sc->tunable_int_delay);
	(void) resource_int_value(device_get_name(dev), device_get_unit(dev),
	    "bundle_max", &sc->tunable_bundle_max);
	(void) resource_int_value(device_get_name(dev), device_get_unit(dev),
	    "noflow", &sc->tunable_noflow);
	sc->rnr = 0;

	/*
	 * Find out the chip revision; lump all 82557 revs together.
	 */
	fxp_read_eeprom(sc, &data, 5, 1);
	if ((data >> 8) == 1)
		sc->revision = FXP_REV_82557;
	else
		sc->revision = pci_get_revid(dev);

	/*
	 * Enable workarounds for certain chip revision deficiencies.
	 *
	 * Systems based on the ICH2/ICH2-M chip from Intel, and possibly
	 * some systems based a normal 82559 design, have a defect where
	 * the chip can cause a PCI protocol violation if it receives
	 * a CU_RESUME command when it is entering the IDLE state.  The 
	 * workaround is to disable Dynamic Standby Mode, so the chip never
	 * deasserts CLKRUN#, and always remains in an active state.
	 *
	 * See Intel 82801BA/82801BAM Specification Update, Errata #30.
	 */
	i = pci_get_device(dev);
	if (i == 0x2449 || (i > 0x1030 && i < 0x1039) ||
	    sc->revision >= FXP_REV_82559_A0) {
		fxp_read_eeprom(sc, &data, 10, 1);
		if (data & 0x02) {			/* STB enable */
			u_int16_t cksum;
			int i;

			device_printf(dev,
			    "Disabling dynamic standby mode in EEPROM\n");
			data &= ~0x02;
			fxp_write_eeprom(sc, &data, 10, 1);
			device_printf(dev, "New EEPROM ID: 0x%x\n", data);
			cksum = 0;
			for (i = 0; i < (1 << sc->eeprom_size) - 1; i++) {
				fxp_read_eeprom(sc, &data, i, 1);
				cksum += data;
			}
			i = (1 << sc->eeprom_size) - 1;
			cksum = 0xBABA - cksum;
			fxp_read_eeprom(sc, &data, i, 1);
			fxp_write_eeprom(sc, &cksum, i, 1);
			device_printf(dev,
			    "EEPROM checksum @ 0x%x: 0x%x -> 0x%x\n",
			    i, data, cksum);
#if 1
			/*
			 * If the user elects to continue, try the software
			 * workaround, as it is better than nothing.
			 */
			sc->flags |= FXP_FLAG_CU_RESUME_BUG;
#endif
		}
	}

	/*
	 * If we are not a 82557 chip, we can enable extended features.
	 */
	if (sc->revision != FXP_REV_82557) {
		/*
		 * If MWI is enabled in the PCI configuration, and there
		 * is a valid cacheline size (8 or 16 dwords), then tell
		 * the board to turn on MWI.
		 */
		if (val & PCIM_CMD_MWRICEN &&
		    pci_read_config(dev, PCIR_CACHELNSZ, 1) != 0)
			sc->flags |= FXP_FLAG_MWI_ENABLE;

		/* turn on the extended TxCB feature */
		sc->flags |= FXP_FLAG_EXT_TXCB;

		/* enable reception of long frames for VLAN */
		sc->flags |= FXP_FLAG_LONG_PKT_EN;
	} else {
		/* a hack to get long VLAN frames on a 82557 */
		sc->flags |= FXP_FLAG_SAVE_BAD;
	}

	/*
	 * Enable use of extended RFDs and TCBs for 82550
	 * and later chips. Note: we need extended TXCB support
	 * too, but that's already enabled by the code above.
	 * Be careful to do this only on the right devices.
	 */

	if (sc->revision == FXP_REV_82550 || sc->revision == FXP_REV_82550_C) {
		sc->rfa_size = sizeof (struct fxp_rfa);
		sc->tx_cmd = FXP_CB_COMMAND_IPCBXMIT;
		sc->flags |= FXP_FLAG_EXT_RFA;
	} else {
		sc->rfa_size = sizeof (struct fxp_rfa) - FXP_RFAX_LEN;
		sc->tx_cmd = FXP_CB_COMMAND_XMIT;
	}

	/*
	 * Allocate DMA tags and DMA safe memory.
	 */
	maxtxseg = sc->flags & FXP_FLAG_EXT_RFA ? FXP_NTXSEG - 1 : FXP_NTXSEG;
	error = bus_dma_tag_create(NULL, 2, 0, BUS_SPACE_MAXADDR_32BIT,
	    BUS_SPACE_MAXADDR, NULL, NULL, MCLBYTES * maxtxseg,
	    maxtxseg, MCLBYTES, 0, busdma_lock_mutex, &Giant, &sc->fxp_mtag);
	if (error) {
		device_printf(dev, "could not allocate dma tag\n");
		goto fail;
	}

	error = bus_dma_tag_create(NULL, 4, 0, BUS_SPACE_MAXADDR_32BIT,
	    BUS_SPACE_MAXADDR, NULL, NULL, sizeof(struct fxp_stats), 1,
	    sizeof(struct fxp_stats), 0, busdma_lock_mutex, &Giant,
	    &sc->fxp_stag);
	if (error) {
		device_printf(dev, "could not allocate dma tag\n");
		goto fail;
	}

	error = bus_dmamem_alloc(sc->fxp_stag, (void **)&sc->fxp_stats,
	    BUS_DMA_NOWAIT | BUS_DMA_ZERO, &sc->fxp_smap);
	if (error)
		goto fail;
	error = bus_dmamap_load(sc->fxp_stag, sc->fxp_smap, sc->fxp_stats,
	    sizeof(struct fxp_stats), fxp_dma_map_addr, &sc->stats_addr, 0);
	if (error) {
		device_printf(dev, "could not map the stats buffer\n");
		goto fail;
	}

	error = bus_dma_tag_create(NULL, 4, 0, BUS_SPACE_MAXADDR_32BIT,
	    BUS_SPACE_MAXADDR, NULL, NULL, FXP_TXCB_SZ, 1,
	    FXP_TXCB_SZ, 0, busdma_lock_mutex, &Giant, &sc->cbl_tag);
	if (error) {
		device_printf(dev, "could not allocate dma tag\n");
		goto fail;
	}

	error = bus_dmamem_alloc(sc->cbl_tag, (void **)&sc->fxp_desc.cbl_list,
	    BUS_DMA_NOWAIT | BUS_DMA_ZERO, &sc->cbl_map);
	if (error)
		goto fail;

	error = bus_dmamap_load(sc->cbl_tag, sc->cbl_map,
	    sc->fxp_desc.cbl_list, FXP_TXCB_SZ, fxp_dma_map_addr,
	    &sc->fxp_desc.cbl_addr, 0);
	if (error) {
		device_printf(dev, "could not map DMA memory\n");
		goto fail;
	}

	error = bus_dma_tag_create(NULL, 4, 0, BUS_SPACE_MAXADDR_32BIT,
	    BUS_SPACE_MAXADDR, NULL, NULL, sizeof(struct fxp_cb_mcs), 1,
	    sizeof(struct fxp_cb_mcs), 0, busdma_lock_mutex, &Giant,
	    &sc->mcs_tag);
	if (error) {
		device_printf(dev, "could not allocate dma tag\n");
		goto fail;
	}

	error = bus_dmamem_alloc(sc->mcs_tag, (void **)&sc->mcsp,
	    BUS_DMA_NOWAIT, &sc->mcs_map);
	if (error)
		goto fail;
	error = bus_dmamap_load(sc->mcs_tag, sc->mcs_map, sc->mcsp,
	    sizeof(struct fxp_cb_mcs), fxp_dma_map_addr, &sc->mcs_addr, 0);
	if (error) {
		device_printf(dev, "can't map the multicast setup command\n");
		goto fail;
	}

	/*
	 * Pre-allocate the TX DMA maps.
	 */
	for (i = 0; i < FXP_NTXCB; i++) {
		error = bus_dmamap_create(sc->fxp_mtag, 0,
		    &sc->fxp_desc.tx_list[i].tx_map);
		if (error) {
			device_printf(dev, "can't create DMA map for TX\n");
			goto fail;
		}
	}
	error = bus_dmamap_create(sc->fxp_mtag, 0, &sc->spare_map);
	if (error) {
		device_printf(dev, "can't create spare DMA map\n");
		goto fail;
	}

	/*
	 * Pre-allocate our receive buffers.
	 */
	sc->fxp_desc.rx_head = sc->fxp_desc.rx_tail = NULL;
	for (i = 0; i < FXP_NRFABUFS; i++) {
		rxp = &sc->fxp_desc.rx_list[i];
		error = bus_dmamap_create(sc->fxp_mtag, 0, &rxp->rx_map);
		if (error) {
			device_printf(dev, "can't create DMA map for RX\n");
			goto fail;
		}
		if (fxp_add_rfabuf(sc, rxp) != 0) {
			error = ENOMEM;
			goto fail;
		}
	}

	/*
	 * Read MAC address.
	 */
	fxp_read_eeprom(sc, myea, 0, 3);
	sc->arpcom.ac_enaddr[0] = myea[0] & 0xff;
	sc->arpcom.ac_enaddr[1] = myea[0] >> 8;
	sc->arpcom.ac_enaddr[2] = myea[1] & 0xff;
	sc->arpcom.ac_enaddr[3] = myea[1] >> 8;
	sc->arpcom.ac_enaddr[4] = myea[2] & 0xff;
	sc->arpcom.ac_enaddr[5] = myea[2] >> 8;
	if (bootverbose) {
		device_printf(dev, "PCI IDs: %04x %04x %04x %04x %04x\n",
		    pci_get_vendor(dev), pci_get_device(dev),
		    pci_get_subvendor(dev), pci_get_subdevice(dev),
		    pci_get_revid(dev));
		fxp_read_eeprom(sc, &data, 10, 1);
		device_printf(dev, "Dynamic Standby mode is %s\n",
		    data & 0x02 ? "enabled" : "disabled");
	}

	/*
	 * If this is only a 10Mbps device, then there is no MII, and
	 * the PHY will use a serial interface instead.
	 *
	 * The Seeq 80c24 AutoDUPLEX(tm) Ethernet Interface Adapter
	 * doesn't have a programming interface of any sort.  The
	 * media is sensed automatically based on how the link partner
	 * is configured.  This is, in essence, manual configuration.
	 */
	if (sc->flags & FXP_FLAG_SERIAL_MEDIA) {
		ifmedia_add(&sc->sc_media, IFM_ETHER|IFM_MANUAL, 0, NULL);
		ifmedia_set(&sc->sc_media, IFM_ETHER|IFM_MANUAL);
	} else {
		if (mii_phy_probe(dev, &sc->miibus, fxp_ifmedia_upd,
		    fxp_ifmedia_sts)) {
	                device_printf(dev, "MII without any PHY!\n");
			error = ENXIO;
			goto fail;
		}
	}

	ifp = &sc->arpcom.ac_if;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_baudrate = 100000000;
	ifp->if_init = fxp_init;
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = fxp_ioctl;
	ifp->if_start = fxp_start;
	ifp->if_watchdog = fxp_watchdog;

	ifp->if_capabilities = ifp->if_capenable = 0;

	/* Enable checksum offload for 82550 or better chips */
	if (sc->flags & FXP_FLAG_EXT_RFA) {
		ifp->if_hwassist = FXP_CSUM_FEATURES;
		ifp->if_capabilities |= IFCAP_HWCSUM;
		ifp->if_capenable |= IFCAP_HWCSUM;
	}

#ifdef DEVICE_POLLING
	/* Inform the world we support polling. */
	ifp->if_capabilities |= IFCAP_POLLING;
	ifp->if_capenable |= IFCAP_POLLING;
#endif

	/*
	 * Attach the interface.
	 */
	ether_ifattach(ifp, sc->arpcom.ac_enaddr);

	/*
	 * Tell the upper layer(s) we support long frames.
	 * Must appear after the call to ether_ifattach() because
	 * ether_ifattach() sets ifi_hdrlen to the default value.
	 */
	ifp->if_data.ifi_hdrlen = sizeof(struct ether_vlan_header);
	ifp->if_capabilities |= IFCAP_VLAN_MTU;
	ifp->if_capenable |= IFCAP_VLAN_MTU; /* the hw bits already set */

	/*
	 * Let the system queue as many packets as we have available
	 * TX descriptors.
	 */
	IFQ_SET_MAXLEN(&ifp->if_snd, FXP_NTXCB - 1);
	ifp->if_snd.ifq_drv_maxlen = FXP_NTXCB - 1;
	IFQ_SET_READY(&ifp->if_snd);

	/* 
	 * Hook our interrupt after all initialization is complete.
	 */
	error = bus_setup_intr(dev, sc->irq, INTR_TYPE_NET | INTR_MPSAFE,
			       fxp_intr, sc, &sc->ih);
	if (error) {
		device_printf(dev, "could not setup irq\n");
		ether_ifdetach(&sc->arpcom.ac_if);
		goto fail;
	}

fail:
	splx(s);
	if (error)
		fxp_release(sc);
	return (error);
}

/*
 * Release all resources.  The softc lock should not be held and the
 * interrupt should already be torn down.
 */
static void
fxp_release(struct fxp_softc *sc)
{
	struct fxp_rx *rxp;
	struct fxp_tx *txp;
	int i;

	FXP_LOCK_ASSERT(sc, MA_NOTOWNED);
	KASSERT(sc->ih == NULL,
	    ("fxp_release() called with intr handle still active"));
	if (sc->miibus)
		device_delete_child(sc->dev, sc->miibus);
	bus_generic_detach(sc->dev);
	ifmedia_removeall(&sc->sc_media);
	if (sc->fxp_desc.cbl_list) {
		bus_dmamap_unload(sc->cbl_tag, sc->cbl_map);
		bus_dmamem_free(sc->cbl_tag, sc->fxp_desc.cbl_list,
		    sc->cbl_map);
	}
	if (sc->fxp_stats) {
		bus_dmamap_unload(sc->fxp_stag, sc->fxp_smap);
		bus_dmamem_free(sc->fxp_stag, sc->fxp_stats, sc->fxp_smap);
	}
	if (sc->mcsp) {
		bus_dmamap_unload(sc->mcs_tag, sc->mcs_map);
		bus_dmamem_free(sc->mcs_tag, sc->mcsp, sc->mcs_map);
	}
	if (sc->irq)
		bus_release_resource(sc->dev, SYS_RES_IRQ, 0, sc->irq);
	if (sc->mem)
		bus_release_resource(sc->dev, sc->rtp, sc->rgd, sc->mem);
	if (sc->fxp_mtag) {
		for (i = 0; i < FXP_NRFABUFS; i++) {
			rxp = &sc->fxp_desc.rx_list[i];
			if (rxp->rx_mbuf != NULL) {
				bus_dmamap_sync(sc->fxp_mtag, rxp->rx_map,
				    BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(sc->fxp_mtag, rxp->rx_map);
				m_freem(rxp->rx_mbuf);
			}
			bus_dmamap_destroy(sc->fxp_mtag, rxp->rx_map);
		}
		bus_dmamap_destroy(sc->fxp_mtag, sc->spare_map);
		bus_dma_tag_destroy(sc->fxp_mtag);
	}
	if (sc->fxp_stag) {
		for (i = 0; i < FXP_NTXCB; i++) {
			txp = &sc->fxp_desc.tx_list[i];
			if (txp->tx_mbuf != NULL) {
				bus_dmamap_sync(sc->fxp_mtag, txp->tx_map,
				    BUS_DMASYNC_POSTWRITE);
				bus_dmamap_unload(sc->fxp_mtag, txp->tx_map);
				m_freem(txp->tx_mbuf);
			}
			bus_dmamap_destroy(sc->fxp_mtag, txp->tx_map);
		}
		bus_dma_tag_destroy(sc->fxp_stag);
	}
	if (sc->cbl_tag)
		bus_dma_tag_destroy(sc->cbl_tag);
	if (sc->mcs_tag)
		bus_dma_tag_destroy(sc->mcs_tag);

	mtx_destroy(&sc->sc_mtx);
}

/*
 * Detach interface.
 */
static int
fxp_detach(device_t dev)
{
	struct fxp_softc *sc = device_get_softc(dev);
	int s;

	FXP_LOCK(sc);
	s = splimp();

	sc->suspended = 1;	/* Do same thing as we do for suspend */
	/*
	 * Close down routes etc.
	 */
	ether_ifdetach(&sc->arpcom.ac_if);

	/*
	 * Stop DMA and drop transmit queue, but disable interrupts first.
	 */
	CSR_WRITE_1(sc, FXP_CSR_SCB_INTRCNTL, FXP_SCB_INTR_DISABLE);
	fxp_stop(sc);
	FXP_UNLOCK(sc);

	/*
	 * Unhook interrupt before dropping lock. This is to prevent
	 * races with fxp_intr().
	 */
	bus_teardown_intr(sc->dev, sc->irq, sc->ih);
	sc->ih = NULL;

	splx(s);

	/* Release our allocated resources. */
	fxp_release(sc);
	return (0);
}

/*
 * Device shutdown routine. Called at system shutdown after sync. The
 * main purpose of this routine is to shut off receiver DMA so that
 * kernel memory doesn't get clobbered during warmboot.
 */
static int
fxp_shutdown(device_t dev)
{
	/*
	 * Make sure that DMA is disabled prior to reboot. Not doing
	 * do could allow DMA to corrupt kernel memory during the
	 * reboot before the driver initializes.
	 */
	fxp_stop((struct fxp_softc *) device_get_softc(dev));
	return (0);
}

/*
 * Device suspend routine.  Stop the interface and save some PCI
 * settings in case the BIOS doesn't restore them properly on
 * resume.
 */
static int
fxp_suspend(device_t dev)
{
	struct fxp_softc *sc = device_get_softc(dev);
	int i, s;

	FXP_LOCK(sc);
	s = splimp();

	fxp_stop(sc);
	
	for (i = 0; i < 5; i++)
		sc->saved_maps[i] = pci_read_config(dev, PCIR_BAR(i), 4);
	sc->saved_biosaddr = pci_read_config(dev, PCIR_BIOS, 4);
	sc->saved_intline = pci_read_config(dev, PCIR_INTLINE, 1);
	sc->saved_cachelnsz = pci_read_config(dev, PCIR_CACHELNSZ, 1);
	sc->saved_lattimer = pci_read_config(dev, PCIR_LATTIMER, 1);

	sc->suspended = 1;

	FXP_UNLOCK(sc);
	splx(s);
	return (0);
}

/*
 * Device resume routine.  Restore some PCI settings in case the BIOS
 * doesn't, re-enable busmastering, and restart the interface if
 * appropriate.
 */
static int
fxp_resume(device_t dev)
{
	struct fxp_softc *sc = device_get_softc(dev);
	struct ifnet *ifp = &sc->sc_if;
	u_int16_t pci_command;
	int i, s;

	FXP_LOCK(sc);
	s = splimp();

	/* better way to do this? */
	for (i = 0; i < 5; i++)
		pci_write_config(dev, PCIR_BAR(i), sc->saved_maps[i], 4);
	pci_write_config(dev, PCIR_BIOS, sc->saved_biosaddr, 4);
	pci_write_config(dev, PCIR_INTLINE, sc->saved_intline, 1);
	pci_write_config(dev, PCIR_CACHELNSZ, sc->saved_cachelnsz, 1);
	pci_write_config(dev, PCIR_LATTIMER, sc->saved_lattimer, 1);

	/* reenable busmastering */
	pci_command = pci_read_config(dev, PCIR_COMMAND, 2);
	pci_command |= (PCIM_CMD_MEMEN|PCIM_CMD_BUSMASTEREN);
	pci_write_config(dev, PCIR_COMMAND, pci_command, 2);

	CSR_WRITE_4(sc, FXP_CSR_PORT, FXP_PORT_SELECTIVE_RESET);
	DELAY(10);

	/* reinitialize interface if necessary */
	if (ifp->if_flags & IFF_UP)
		fxp_init_body(sc);

	sc->suspended = 0;

	FXP_UNLOCK(sc);
	splx(s);
	return (0);
}

static void 
fxp_eeprom_shiftin(struct fxp_softc *sc, int data, int length)
{
	u_int16_t reg;
	int x;

	/*
	 * Shift in data.
	 */
	for (x = 1 << (length - 1); x; x >>= 1) {
		if (data & x)
			reg = FXP_EEPROM_EECS | FXP_EEPROM_EEDI;
		else
			reg = FXP_EEPROM_EECS;
		CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, reg);
		DELAY(1);
		CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, reg | FXP_EEPROM_EESK);
		DELAY(1);
		CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, reg);
		DELAY(1);
	}
}

/*
 * Read from the serial EEPROM. Basically, you manually shift in
 * the read opcode (one bit at a time) and then shift in the address,
 * and then you shift out the data (all of this one bit at a time).
 * The word size is 16 bits, so you have to provide the address for
 * every 16 bits of data.
 */
static u_int16_t
fxp_eeprom_getword(struct fxp_softc *sc, int offset, int autosize)
{
	u_int16_t reg, data;
	int x;

	CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, FXP_EEPROM_EECS);
	/*
	 * Shift in read opcode.
	 */
	fxp_eeprom_shiftin(sc, FXP_EEPROM_OPC_READ, 3);
	/*
	 * Shift in address.
	 */
	data = 0;
	for (x = 1 << (sc->eeprom_size - 1); x; x >>= 1) {
		if (offset & x)
			reg = FXP_EEPROM_EECS | FXP_EEPROM_EEDI;
		else
			reg = FXP_EEPROM_EECS;
		CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, reg);
		DELAY(1);
		CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, reg | FXP_EEPROM_EESK);
		DELAY(1);
		CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, reg);
		DELAY(1);
		reg = CSR_READ_2(sc, FXP_CSR_EEPROMCONTROL) & FXP_EEPROM_EEDO;
		data++;
		if (autosize && reg == 0) {
			sc->eeprom_size = data;
			break;
		}
	}
	/*
	 * Shift out data.
	 */
	data = 0;
	reg = FXP_EEPROM_EECS;
	for (x = 1 << 15; x; x >>= 1) {
		CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, reg | FXP_EEPROM_EESK);
		DELAY(1);
		if (CSR_READ_2(sc, FXP_CSR_EEPROMCONTROL) & FXP_EEPROM_EEDO)
			data |= x;
		CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, reg);
		DELAY(1);
	}
	CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, 0);
	DELAY(1);

	return (data);
}

static void
fxp_eeprom_putword(struct fxp_softc *sc, int offset, u_int16_t data)
{
	int i;

	/*
	 * Erase/write enable.
	 */
	CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, FXP_EEPROM_EECS);
	fxp_eeprom_shiftin(sc, 0x4, 3);
	fxp_eeprom_shiftin(sc, 0x03 << (sc->eeprom_size - 2), sc->eeprom_size);
	CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, 0);
	DELAY(1);
	/*
	 * Shift in write opcode, address, data.
	 */
	CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, FXP_EEPROM_EECS);
	fxp_eeprom_shiftin(sc, FXP_EEPROM_OPC_WRITE, 3);
	fxp_eeprom_shiftin(sc, offset, sc->eeprom_size);
	fxp_eeprom_shiftin(sc, data, 16);
	CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, 0);
	DELAY(1);
	/*
	 * Wait for EEPROM to finish up.
	 */
	CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, FXP_EEPROM_EECS);
	DELAY(1);
	for (i = 0; i < 1000; i++) {
		if (CSR_READ_2(sc, FXP_CSR_EEPROMCONTROL) & FXP_EEPROM_EEDO)
			break;
		DELAY(50);
	}
	CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, 0);
	DELAY(1);
	/*
	 * Erase/write disable.
	 */
	CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, FXP_EEPROM_EECS);
	fxp_eeprom_shiftin(sc, 0x4, 3);
	fxp_eeprom_shiftin(sc, 0, sc->eeprom_size);
	CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, 0);
	DELAY(1);
}

/*
 * From NetBSD:
 *
 * Figure out EEPROM size.
 *
 * 559's can have either 64-word or 256-word EEPROMs, the 558
 * datasheet only talks about 64-word EEPROMs, and the 557 datasheet
 * talks about the existance of 16 to 256 word EEPROMs.
 *
 * The only known sizes are 64 and 256, where the 256 version is used
 * by CardBus cards to store CIS information.
 *
 * The address is shifted in msb-to-lsb, and after the last
 * address-bit the EEPROM is supposed to output a `dummy zero' bit,
 * after which follows the actual data. We try to detect this zero, by
 * probing the data-out bit in the EEPROM control register just after
 * having shifted in a bit. If the bit is zero, we assume we've
 * shifted enough address bits. The data-out should be tri-state,
 * before this, which should translate to a logical one.
 */
static void
fxp_autosize_eeprom(struct fxp_softc *sc)
{

	/* guess maximum size of 256 words */
	sc->eeprom_size = 8;

	/* autosize */
	(void) fxp_eeprom_getword(sc, 0, 1);
}

static void
fxp_read_eeprom(struct fxp_softc *sc, u_short *data, int offset, int words)
{
	int i;

	for (i = 0; i < words; i++)
		data[i] = fxp_eeprom_getword(sc, offset + i, 0);
}

static void
fxp_write_eeprom(struct fxp_softc *sc, u_short *data, int offset, int words)
{
	int i;

	for (i = 0; i < words; i++)
		fxp_eeprom_putword(sc, offset + i, data[i]);
}

static void
fxp_dma_map_txbuf(void *arg, bus_dma_segment_t *segs, int nseg,
    bus_size_t mapsize, int error)
{
	struct fxp_softc *sc;
	struct fxp_cb_tx *txp;
	int i;

	if (error)
		return;

	KASSERT(nseg <= FXP_NTXSEG, ("too many DMA segments"));

	sc = arg;
	txp = sc->fxp_desc.tx_last->tx_next->tx_cb;
	for (i = 0; i < nseg; i++) {
		KASSERT(segs[i].ds_len <= MCLBYTES, ("segment size too large"));
		/*
		 * If this is an 82550/82551, then we're using extended
		 * TxCBs _and_ we're using checksum offload. This means
		 * that the TxCB is really an IPCB. One major difference
		 * between the two is that with plain extended TxCBs,
		 * the bottom half of the TxCB contains two entries from
		 * the TBD array, whereas IPCBs contain just one entry:
		 * one entry (8 bytes) has been sacrificed for the TCP/IP
		 * checksum offload control bits. So to make things work
		 * right, we have to start filling in the TBD array
		 * starting from a different place depending on whether
		 * the chip is an 82550/82551 or not.
		 */
		if (sc->flags & FXP_FLAG_EXT_RFA) {
			txp->tbd[i + 1].tb_addr = htole32(segs[i].ds_addr);
			txp->tbd[i + 1].tb_size = htole32(segs[i].ds_len);
		} else {
			txp->tbd[i].tb_addr = htole32(segs[i].ds_addr);
			txp->tbd[i].tb_size = htole32(segs[i].ds_len);
		}
	}
	txp->tbd_number = nseg;
}

/*
 * Grab the softc lock and call the real fxp_start_body() routine
 */
static void
fxp_start(struct ifnet *ifp)
{
	struct fxp_softc *sc = ifp->if_softc;

	FXP_LOCK(sc);
	fxp_start_body(ifp);
	FXP_UNLOCK(sc);
}

/*
 * Start packet transmission on the interface.  
 * This routine must be called with the softc lock held, and is an
 * internal entry point only.
 */
static void
fxp_start_body(struct ifnet *ifp)
{
	struct fxp_softc *sc = ifp->if_softc;
	struct fxp_tx *txp;
	struct mbuf *mb_head;
	int error;

	FXP_LOCK_ASSERT(sc, MA_OWNED);
	/*
	 * See if we need to suspend xmit until the multicast filter
	 * has been reprogrammed (which can only be done at the head
	 * of the command chain).
	 */
	if (sc->need_mcsetup) {
		return;
	}

	txp = NULL;

	/*
	 * We're finished if there is nothing more to add to the list or if
	 * we're all filled up with buffers to transmit.
	 * NOTE: One TxCB is reserved to guarantee that fxp_mc_setup() can add
	 *       a NOP command when needed.
	 */
	while (!IFQ_DRV_IS_EMPTY(&ifp->if_snd) &&
	    sc->tx_queued < FXP_NTXCB - 1) {

		/*
		 * Grab a packet to transmit.
		 */
		IFQ_DRV_DEQUEUE(&ifp->if_snd, mb_head);
		if (mb_head == NULL)
			break;

		/*
		 * Get pointer to next available tx desc.
		 */
		txp = sc->fxp_desc.tx_last->tx_next;

		/*
		 * A note in Appendix B of the Intel 8255x 10/100 Mbps
		 * Ethernet Controller Family Open Source Software
		 * Developer Manual says:
		 *   Using software parsing is only allowed with legal
		 *   TCP/IP or UDP/IP packets.
		 *   ...
		 *   For all other datagrams, hardware parsing must
		 *   be used.
		 * Software parsing appears to truncate ICMP and
		 * fragmented UDP packets that contain one to three
		 * bytes in the second (and final) mbuf of the packet.
		 */
		if (sc->flags & FXP_FLAG_EXT_RFA)
			txp->tx_cb->ipcb_ip_activation_high =
			    FXP_IPCB_HARDWAREPARSING_ENABLE;

		/*
		 * Deal with TCP/IP checksum offload. Note that
		 * in order for TCP checksum offload to work,
		 * the pseudo header checksum must have already
		 * been computed and stored in the checksum field
		 * in the TCP header. The stack should have
		 * already done this for us.
		 */

		if (mb_head->m_pkthdr.csum_flags) {
			if (mb_head->m_pkthdr.csum_flags & CSUM_DELAY_DATA) {
				txp->tx_cb->ipcb_ip_schedule =
				    FXP_IPCB_TCPUDP_CHECKSUM_ENABLE;
				if (mb_head->m_pkthdr.csum_flags & CSUM_TCP)
					txp->tx_cb->ipcb_ip_schedule |=
					    FXP_IPCB_TCP_PACKET;
			}
#ifdef FXP_IP_CSUM_WAR
		/*
		 * XXX The 82550 chip appears to have trouble
		 * dealing with IP header checksums in very small
		 * datagrams, namely fragments from 1 to 3 bytes
		 * in size. For example, say you want to transmit
		 * a UDP packet of 1473 bytes. The packet will be
		 * fragmented over two IP datagrams, the latter
		 * containing only one byte of data. The 82550 will
		 * botch the header checksum on the 1-byte fragment.
		 * As long as the datagram contains 4 or more bytes
		 * of data, you're ok.
		 *
                 * The following code attempts to work around this
		 * problem: if the datagram is less than 38 bytes
		 * in size (14 bytes ether header, 20 bytes IP header,
		 * plus 4 bytes of data), we punt and compute the IP
		 * header checksum by hand. This workaround doesn't
		 * work very well, however, since it can be fooled
		 * by things like VLAN tags and IP options that make
		 * the header sizes/offsets vary.
		 */

			if (mb_head->m_pkthdr.csum_flags & CSUM_IP) {
				if (mb_head->m_pkthdr.len < 38) {
					struct ip *ip;
					mb_head->m_data += ETHER_HDR_LEN;
					ip = mtod(mb_head, struct ip *);
					ip->ip_sum = in_cksum(mb_head,
					    ip->ip_hl << 2);
					mb_head->m_data -= ETHER_HDR_LEN;
				} else {
					txp->tx_cb->ipcb_ip_activation_high =
					    FXP_IPCB_HARDWAREPARSING_ENABLE;
					txp->tx_cb->ipcb_ip_schedule |=
					    FXP_IPCB_IP_CHECKSUM_ENABLE;
				}
			}
#endif
		}

		/*
		 * Go through each of the mbufs in the chain and initialize
		 * the transmit buffer descriptors with the physical address
		 * and size of the mbuf.
		 */
		error = bus_dmamap_load_mbuf(sc->fxp_mtag, txp->tx_map,
		    mb_head, fxp_dma_map_txbuf, sc, 0);

		if (error && error != EFBIG) {
			device_printf(sc->dev, "can't map mbuf (error %d)\n",
			    error);
			m_freem(mb_head);
			break;
		}

		if (error) {
			struct mbuf *mn;

			/*
			 * We ran out of segments. We have to recopy this
			 * mbuf chain first. Bail out if we can't get the
			 * new buffers.
			 */
			mn = m_defrag(mb_head, M_DONTWAIT);
			if (mn == NULL) {
				m_freem(mb_head);
				break;
			} else {
				mb_head = mn;
			}
			error = bus_dmamap_load_mbuf(sc->fxp_mtag, txp->tx_map,
			    mb_head, fxp_dma_map_txbuf, sc, 0);
			if (error) {
				device_printf(sc->dev,
				    "can't map mbuf (error %d)\n", error);
				m_freem(mb_head);
				break;
			}
		}

		bus_dmamap_sync(sc->fxp_mtag, txp->tx_map,
		    BUS_DMASYNC_PREWRITE);

		txp->tx_mbuf = mb_head;
		txp->tx_cb->cb_status = 0;
		txp->tx_cb->byte_count = 0;
		if (sc->tx_queued != FXP_CXINT_THRESH - 1) {
			txp->tx_cb->cb_command =
			    htole16(sc->tx_cmd | FXP_CB_COMMAND_SF |
			    FXP_CB_COMMAND_S);
		} else {
			txp->tx_cb->cb_command =
			    htole16(sc->tx_cmd | FXP_CB_COMMAND_SF |
			    FXP_CB_COMMAND_S | FXP_CB_COMMAND_I);
			/*
			 * Set a 5 second timer just in case we don't hear
			 * from the card again.
			 */
			ifp->if_timer = 5;
		}
		txp->tx_cb->tx_threshold = tx_threshold;

		/*
		 * Advance the end of list forward.
		 */

#ifdef __alpha__
		/*
		 * On platforms which can't access memory in 16-bit
		 * granularities, we must prevent the card from DMA'ing
		 * up the status while we update the command field.
		 * This could cause us to overwrite the completion status.
		 * XXX This is probably bogus and we're _not_ looking
		 * for atomicity here.
		 */
		atomic_clear_16(&sc->fxp_desc.tx_last->tx_cb->cb_command,
		    htole16(FXP_CB_COMMAND_S));
#else
		sc->fxp_desc.tx_last->tx_cb->cb_command &=
		    htole16(~FXP_CB_COMMAND_S);
#endif /*__alpha__*/
		sc->fxp_desc.tx_last = txp;

		/*
		 * Advance the beginning of the list forward if there are
		 * no other packets queued (when nothing is queued, tx_first
		 * sits on the last TxCB that was sent out).
		 */
		if (sc->tx_queued == 0)
			sc->fxp_desc.tx_first = txp;

		sc->tx_queued++;

		/*
		 * Pass packet to bpf if there is a listener.
		 */
		BPF_MTAP(ifp, mb_head);
	}
	bus_dmamap_sync(sc->cbl_tag, sc->cbl_map, BUS_DMASYNC_PREWRITE);

	/*
	 * We're finished. If we added to the list, issue a RESUME to get DMA
	 * going again if suspended.
	 */
	if (txp != NULL) {
		fxp_scb_wait(sc);
		fxp_scb_cmd(sc, FXP_SCB_COMMAND_CU_RESUME);
	}
}

#ifdef DEVICE_POLLING
static poll_handler_t fxp_poll;

static void
fxp_poll(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct fxp_softc *sc = ifp->if_softc;
	u_int8_t statack;

	FXP_LOCK(sc);
	if (!(ifp->if_capenable & IFCAP_POLLING)) {
		ether_poll_deregister(ifp);
		cmd = POLL_DEREGISTER;
	}
	if (cmd == POLL_DEREGISTER) {	/* final call, enable interrupts */
		CSR_WRITE_1(sc, FXP_CSR_SCB_INTRCNTL, 0);
		FXP_UNLOCK(sc);
		return;
	}
	statack = FXP_SCB_STATACK_CXTNO | FXP_SCB_STATACK_CNA |
	    FXP_SCB_STATACK_FR;
	if (cmd == POLL_AND_CHECK_STATUS) {
		u_int8_t tmp;

		tmp = CSR_READ_1(sc, FXP_CSR_SCB_STATACK);
		if (tmp == 0xff || tmp == 0) {
			FXP_UNLOCK(sc);
			return; /* nothing to do */
		}
		tmp &= ~statack;
		/* ack what we can */
		if (tmp != 0)
			CSR_WRITE_1(sc, FXP_CSR_SCB_STATACK, tmp);
		statack |= tmp;
	}
	fxp_intr_body(sc, ifp, statack, count);
	FXP_UNLOCK(sc);
}
#endif /* DEVICE_POLLING */

/*
 * Process interface interrupts.
 */
static void
fxp_intr(void *xsc)
{
	struct fxp_softc *sc = xsc;
	struct ifnet *ifp = &sc->sc_if;
	u_int8_t statack;

	FXP_LOCK(sc);
	if (sc->suspended) {
		FXP_UNLOCK(sc);
		return;
	}

#ifdef DEVICE_POLLING
	if (ifp->if_flags & IFF_POLLING) {
		FXP_UNLOCK(sc);
		return;
	}
	if ((ifp->if_capenable & IFCAP_POLLING) &&
	    ether_poll_register(fxp_poll, ifp)) {
		/* disable interrupts */
		CSR_WRITE_1(sc, FXP_CSR_SCB_INTRCNTL, FXP_SCB_INTR_DISABLE);
		FXP_UNLOCK(sc);
		fxp_poll(ifp, 0, 1);
		return;
	}
#endif
	while ((statack = CSR_READ_1(sc, FXP_CSR_SCB_STATACK)) != 0) {
		/*
		 * It should not be possible to have all bits set; the
		 * FXP_SCB_INTR_SWI bit always returns 0 on a read.  If 
		 * all bits are set, this may indicate that the card has
		 * been physically ejected, so ignore it.
		 */  
		if (statack == 0xff) {
			FXP_UNLOCK(sc);
			return;
		}

		/*
		 * First ACK all the interrupts in this pass.
		 */
		CSR_WRITE_1(sc, FXP_CSR_SCB_STATACK, statack);
		fxp_intr_body(sc, ifp, statack, -1);
	}
	FXP_UNLOCK(sc);
}

static void
fxp_txeof(struct fxp_softc *sc)
{
	struct fxp_tx *txp;

	bus_dmamap_sync(sc->cbl_tag, sc->cbl_map, BUS_DMASYNC_PREREAD);
	for (txp = sc->fxp_desc.tx_first; sc->tx_queued &&
	    (le16toh(txp->tx_cb->cb_status) & FXP_CB_STATUS_C) != 0;
	    txp = txp->tx_next) {
		if (txp->tx_mbuf != NULL) {
			bus_dmamap_sync(sc->fxp_mtag, txp->tx_map,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->fxp_mtag, txp->tx_map);
			m_freem(txp->tx_mbuf);
			txp->tx_mbuf = NULL;
			/* clear this to reset csum offload bits */
			txp->tx_cb->tbd[0].tb_addr = 0;
		}
		sc->tx_queued--;
	}
	sc->fxp_desc.tx_first = txp;
	bus_dmamap_sync(sc->cbl_tag, sc->cbl_map, BUS_DMASYNC_PREWRITE);
}

static void
fxp_intr_body(struct fxp_softc *sc, struct ifnet *ifp, u_int8_t statack,
    int count)
{
	struct mbuf *m;
	struct fxp_rx *rxp;
	struct fxp_rfa *rfa;
	int rnr = (statack & FXP_SCB_STATACK_RNR) ? 1 : 0;

	FXP_LOCK_ASSERT(sc, MA_OWNED);
	if (rnr)
		sc->rnr++;
#ifdef DEVICE_POLLING
	/* Pick up a deferred RNR condition if `count' ran out last time. */
	if (sc->flags & FXP_FLAG_DEFERRED_RNR) {
		sc->flags &= ~FXP_FLAG_DEFERRED_RNR;
		rnr = 1;
	}
#endif

	/*
	 * Free any finished transmit mbuf chains.
	 *
	 * Handle the CNA event likt a CXTNO event. It used to
	 * be that this event (control unit not ready) was not
	 * encountered, but it is now with the SMPng modifications.
	 * The exact sequence of events that occur when the interface
	 * is brought up are different now, and if this event
	 * goes unhandled, the configuration/rxfilter setup sequence
	 * can stall for several seconds. The result is that no
	 * packets go out onto the wire for about 5 to 10 seconds
	 * after the interface is ifconfig'ed for the first time.
	 */
	if (statack & (FXP_SCB_STATACK_CXTNO | FXP_SCB_STATACK_CNA)) {
		fxp_txeof(sc);

		ifp->if_timer = 0;
		if (sc->tx_queued == 0) {
			if (sc->need_mcsetup)
				fxp_mc_setup(sc);
		}
		/*
		 * Try to start more packets transmitting.
		 */
		if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
			fxp_start_body(ifp);
	}

	/*
	 * Just return if nothing happened on the receive side.
	 */
	if (!rnr && (statack & FXP_SCB_STATACK_FR) == 0)
		return;

	/*
	 * Process receiver interrupts. If a no-resource (RNR)
	 * condition exists, get whatever packets we can and
	 * re-start the receiver.
	 *
	 * When using polling, we do not process the list to completion,
	 * so when we get an RNR interrupt we must defer the restart
	 * until we hit the last buffer with the C bit set.
	 * If we run out of cycles and rfa_headm has the C bit set,
	 * record the pending RNR in the FXP_FLAG_DEFERRED_RNR flag so
	 * that the info will be used in the subsequent polling cycle.
	 */
	for (;;) {
		rxp = sc->fxp_desc.rx_head;
		m = rxp->rx_mbuf;
		rfa = (struct fxp_rfa *)(m->m_ext.ext_buf +
		    RFA_ALIGNMENT_FUDGE);
		bus_dmamap_sync(sc->fxp_mtag, rxp->rx_map,
		    BUS_DMASYNC_POSTREAD);

#ifdef DEVICE_POLLING /* loop at most count times if count >=0 */
		if (count >= 0 && count-- == 0) {
			if (rnr) {
				/* Defer RNR processing until the next time. */
				sc->flags |= FXP_FLAG_DEFERRED_RNR;
				rnr = 0;
			}
			break;
		}
#endif /* DEVICE_POLLING */

		if ((le16toh(rfa->rfa_status) & FXP_RFA_STATUS_C) == 0)
			break;

		/*
		 * Advance head forward.
		 */
		sc->fxp_desc.rx_head = rxp->rx_next;

		/*
		 * Add a new buffer to the receive chain.
		 * If this fails, the old buffer is recycled
		 * instead.
		 */
		if (fxp_add_rfabuf(sc, rxp) == 0) {
			int total_len;

			/*
			 * Fetch packet length (the top 2 bits of
			 * actual_size are flags set by the controller
			 * upon completion), and drop the packet in case
			 * of bogus length or CRC errors.
			 */
			total_len = le16toh(rfa->actual_size) & 0x3fff;
			if (total_len < sizeof(struct ether_header) ||
			    total_len > MCLBYTES - RFA_ALIGNMENT_FUDGE -
				sc->rfa_size ||
			    le16toh(rfa->rfa_status) & FXP_RFA_STATUS_CRC) {
				m_freem(m);
				continue;
			}

                        /* Do IP checksum checking. */
			if (le16toh(rfa->rfa_status) & FXP_RFA_STATUS_PARSE) {
				if (rfa->rfax_csum_sts &
				    FXP_RFDX_CS_IP_CSUM_BIT_VALID)
					m->m_pkthdr.csum_flags |=
					    CSUM_IP_CHECKED;
				if (rfa->rfax_csum_sts &
				    FXP_RFDX_CS_IP_CSUM_VALID)
					m->m_pkthdr.csum_flags |=
					    CSUM_IP_VALID;
				if ((rfa->rfax_csum_sts &
				    FXP_RFDX_CS_TCPUDP_CSUM_BIT_VALID) &&
				    (rfa->rfax_csum_sts &
				    FXP_RFDX_CS_TCPUDP_CSUM_VALID)) {
					m->m_pkthdr.csum_flags |=
					    CSUM_DATA_VALID|CSUM_PSEUDO_HDR;
					m->m_pkthdr.csum_data = 0xffff;
				}
			}

			m->m_pkthdr.len = m->m_len = total_len;
			m->m_pkthdr.rcvif = ifp;

			/*
			 * Drop locks before calling if_input() since it
			 * may re-enter fxp_start() in the netisr case.
			 * This would result in a lock reversal.  Better
			 * performance might be obtained by chaining all
			 * packets received, dropping the lock, and then
			 * calling if_input() on each one.
			 */
			FXP_UNLOCK(sc);
			(*ifp->if_input)(ifp, m);
			FXP_LOCK(sc);
		}
	}
	if (rnr) {
		fxp_scb_wait(sc);
		CSR_WRITE_4(sc, FXP_CSR_SCB_GENERAL,
		    sc->fxp_desc.rx_head->rx_addr);
		fxp_scb_cmd(sc, FXP_SCB_COMMAND_RU_START);
	}
}

/*
 * Update packet in/out/collision statistics. The i82557 doesn't
 * allow you to access these counters without doing a fairly
 * expensive DMA to get _all_ of the statistics it maintains, so
 * we do this operation here only once per second. The statistics
 * counters in the kernel are updated from the previous dump-stats
 * DMA and then a new dump-stats DMA is started. The on-chip
 * counters are zeroed when the DMA completes. If we can't start
 * the DMA immediately, we don't wait - we just prepare to read
 * them again next time.
 */
static void
fxp_tick(void *xsc)
{
	struct fxp_softc *sc = xsc;
	struct ifnet *ifp = &sc->sc_if;
	struct fxp_stats *sp = sc->fxp_stats;
	int s;

	FXP_LOCK(sc);
	s = splimp();
	bus_dmamap_sync(sc->fxp_stag, sc->fxp_smap, BUS_DMASYNC_POSTREAD);
	ifp->if_opackets += le32toh(sp->tx_good);
	ifp->if_collisions += le32toh(sp->tx_total_collisions);
	if (sp->rx_good) {
		ifp->if_ipackets += le32toh(sp->rx_good);
		sc->rx_idle_secs = 0;
	} else {
		/*
		 * Receiver's been idle for another second.
		 */
		sc->rx_idle_secs++;
	}
	ifp->if_ierrors +=
	    le32toh(sp->rx_crc_errors) +
	    le32toh(sp->rx_alignment_errors) +
	    le32toh(sp->rx_rnr_errors) +
	    le32toh(sp->rx_overrun_errors);
	/*
	 * If any transmit underruns occured, bump up the transmit
	 * threshold by another 512 bytes (64 * 8).
	 */
	if (sp->tx_underruns) {
		ifp->if_oerrors += le32toh(sp->tx_underruns);
		if (tx_threshold < 192)
			tx_threshold += 64;
	}

	/*
	 * Release any xmit buffers that have completed DMA. This isn't
	 * strictly necessary to do here, but it's advantagous for mbufs
	 * with external storage to be released in a timely manner rather
	 * than being defered for a potentially long time. This limits
	 * the delay to a maximum of one second.
	 */ 
	fxp_txeof(sc);

	/*
	 * If we haven't received any packets in FXP_MAC_RX_IDLE seconds,
	 * then assume the receiver has locked up and attempt to clear
	 * the condition by reprogramming the multicast filter. This is
	 * a work-around for a bug in the 82557 where the receiver locks
	 * up if it gets certain types of garbage in the syncronization
	 * bits prior to the packet header. This bug is supposed to only
	 * occur in 10Mbps mode, but has been seen to occur in 100Mbps
	 * mode as well (perhaps due to a 10/100 speed transition).
	 */
	if (sc->rx_idle_secs > FXP_MAX_RX_IDLE) {
		sc->rx_idle_secs = 0;
		fxp_mc_setup(sc);
	}
	/*
	 * If there is no pending command, start another stats
	 * dump. Otherwise punt for now.
	 */
	if (CSR_READ_1(sc, FXP_CSR_SCB_COMMAND) == 0) {
		/*
		 * Start another stats dump.
		 */
		bus_dmamap_sync(sc->fxp_stag, sc->fxp_smap,
		    BUS_DMASYNC_PREREAD);
		fxp_scb_cmd(sc, FXP_SCB_COMMAND_CU_DUMPRESET);
	} else {
		/*
		 * A previous command is still waiting to be accepted.
		 * Just zero our copy of the stats and wait for the
		 * next timer event to update them.
		 */
		sp->tx_good = 0;
		sp->tx_underruns = 0;
		sp->tx_total_collisions = 0;

		sp->rx_good = 0;
		sp->rx_crc_errors = 0;
		sp->rx_alignment_errors = 0;
		sp->rx_rnr_errors = 0;
		sp->rx_overrun_errors = 0;
	}
	if (sc->miibus != NULL)
		mii_tick(device_get_softc(sc->miibus));

	/*
	 * Schedule another timeout one second from now.
	 */
	callout_reset(&sc->stat_ch, hz, fxp_tick, sc);
	FXP_UNLOCK(sc);
	splx(s);
}

/*
 * Stop the interface. Cancels the statistics updater and resets
 * the interface.
 */
static void
fxp_stop(struct fxp_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;
	struct fxp_tx *txp;
	int i;

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;

#ifdef DEVICE_POLLING
	ether_poll_deregister(ifp);
#endif
	/*
	 * Cancel stats updater.
	 */
	callout_stop(&sc->stat_ch);

	/*
	 * Issue software reset, which also unloads the microcode.
	 */
	sc->flags &= ~FXP_FLAG_UCODE;
	CSR_WRITE_4(sc, FXP_CSR_PORT, FXP_PORT_SOFTWARE_RESET);
	DELAY(50);

	/*
	 * Release any xmit buffers.
	 */
	txp = sc->fxp_desc.tx_list;
	if (txp != NULL) {
		for (i = 0; i < FXP_NTXCB; i++) {
 			if (txp[i].tx_mbuf != NULL) {
				bus_dmamap_sync(sc->fxp_mtag, txp[i].tx_map,
				    BUS_DMASYNC_POSTWRITE);
				bus_dmamap_unload(sc->fxp_mtag, txp[i].tx_map);
				m_freem(txp[i].tx_mbuf);
				txp[i].tx_mbuf = NULL;
				/* clear this to reset csum offload bits */
				txp[i].tx_cb->tbd[0].tb_addr = 0;
			}
		}
	}
	bus_dmamap_sync(sc->cbl_tag, sc->cbl_map, BUS_DMASYNC_PREWRITE);
	sc->tx_queued = 0;
}

/*
 * Watchdog/transmission transmit timeout handler. Called when a
 * transmission is started on the interface, but no interrupt is
 * received before the timeout. This usually indicates that the
 * card has wedged for some reason.
 */
static void
fxp_watchdog(struct ifnet *ifp)
{
	struct fxp_softc *sc = ifp->if_softc;

	FXP_LOCK(sc);
	device_printf(sc->dev, "device timeout\n");
	ifp->if_oerrors++;

	fxp_init_body(sc);
	FXP_UNLOCK(sc);
}

/*
 * Acquire locks and then call the real initialization function.  This
 * is necessary because ether_ioctl() calls if_init() and this would
 * result in mutex recursion if the mutex was held.
 */
static void
fxp_init(void *xsc)
{
	struct fxp_softc *sc = xsc;

	FXP_LOCK(sc);
	fxp_init_body(sc);
	FXP_UNLOCK(sc);
}

/*
 * Perform device initialization. This routine must be called with the
 * softc lock held.
 */
static void
fxp_init_body(struct fxp_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;
	struct fxp_cb_config *cbp;
	struct fxp_cb_ias *cb_ias;
	struct fxp_cb_tx *tcbp;
	struct fxp_tx *txp;
	struct fxp_cb_mcs *mcsp;
	int i, prm, s;

	FXP_LOCK_ASSERT(sc, MA_OWNED);
	s = splimp();
	/*
	 * Cancel any pending I/O
	 */
	fxp_stop(sc);

	prm = (ifp->if_flags & IFF_PROMISC) ? 1 : 0;

	/*
	 * Initialize base of CBL and RFA memory. Loading with zero
	 * sets it up for regular linear addressing.
	 */
	CSR_WRITE_4(sc, FXP_CSR_SCB_GENERAL, 0);
	fxp_scb_cmd(sc, FXP_SCB_COMMAND_CU_BASE);

	fxp_scb_wait(sc);
	fxp_scb_cmd(sc, FXP_SCB_COMMAND_RU_BASE);

	/*
	 * Initialize base of dump-stats buffer.
	 */
	fxp_scb_wait(sc);
	bus_dmamap_sync(sc->fxp_stag, sc->fxp_smap, BUS_DMASYNC_PREREAD);
	CSR_WRITE_4(sc, FXP_CSR_SCB_GENERAL, sc->stats_addr);
	fxp_scb_cmd(sc, FXP_SCB_COMMAND_CU_DUMP_ADR);

	/*
	 * Attempt to load microcode if requested.
	 */
	if (ifp->if_flags & IFF_LINK0 && (sc->flags & FXP_FLAG_UCODE) == 0)
		fxp_load_ucode(sc);

	/*
	 * Initialize the multicast address list.
	 */
	if (fxp_mc_addrs(sc)) {
		mcsp = sc->mcsp;
		mcsp->cb_status = 0;
		mcsp->cb_command =
		    htole16(FXP_CB_COMMAND_MCAS | FXP_CB_COMMAND_EL);
		mcsp->link_addr = 0xffffffff;
		/*
	 	 * Start the multicast setup command.
		 */
		fxp_scb_wait(sc);
		bus_dmamap_sync(sc->mcs_tag, sc->mcs_map, BUS_DMASYNC_PREWRITE);
		CSR_WRITE_4(sc, FXP_CSR_SCB_GENERAL, sc->mcs_addr);
		fxp_scb_cmd(sc, FXP_SCB_COMMAND_CU_START);
		/* ...and wait for it to complete. */
		fxp_dma_wait(sc, &mcsp->cb_status, sc->mcs_tag, sc->mcs_map);
		bus_dmamap_sync(sc->mcs_tag, sc->mcs_map,
		    BUS_DMASYNC_POSTWRITE);
	}

	/*
	 * We temporarily use memory that contains the TxCB list to
	 * construct the config CB. The TxCB list memory is rebuilt
	 * later.
	 */
	cbp = (struct fxp_cb_config *)sc->fxp_desc.cbl_list;

	/*
	 * This bcopy is kind of disgusting, but there are a bunch of must be
	 * zero and must be one bits in this structure and this is the easiest
	 * way to initialize them all to proper values.
	 */
	bcopy(fxp_cb_config_template, cbp, sizeof(fxp_cb_config_template));

	cbp->cb_status =	0;
	cbp->cb_command =	htole16(FXP_CB_COMMAND_CONFIG |
	    FXP_CB_COMMAND_EL);
	cbp->link_addr =	0xffffffff;	/* (no) next command */
	cbp->byte_count =	sc->flags & FXP_FLAG_EXT_RFA ? 32 : 22;
	cbp->rx_fifo_limit =	8;	/* rx fifo threshold (32 bytes) */
	cbp->tx_fifo_limit =	0;	/* tx fifo threshold (0 bytes) */
	cbp->adaptive_ifs =	0;	/* (no) adaptive interframe spacing */
	cbp->mwi_enable =	sc->flags & FXP_FLAG_MWI_ENABLE ? 1 : 0;
	cbp->type_enable =	0;	/* actually reserved */
	cbp->read_align_en =	sc->flags & FXP_FLAG_READ_ALIGN ? 1 : 0;
	cbp->end_wr_on_cl =	sc->flags & FXP_FLAG_WRITE_ALIGN ? 1 : 0;
	cbp->rx_dma_bytecount =	0;	/* (no) rx DMA max */
	cbp->tx_dma_bytecount =	0;	/* (no) tx DMA max */
	cbp->dma_mbce =		0;	/* (disable) dma max counters */
	cbp->late_scb =		0;	/* (don't) defer SCB update */
	cbp->direct_dma_dis =	1;	/* disable direct rcv dma mode */
	cbp->tno_int_or_tco_en =0;	/* (disable) tx not okay interrupt */
	cbp->ci_int =		1;	/* interrupt on CU idle */
	cbp->ext_txcb_dis = 	sc->flags & FXP_FLAG_EXT_TXCB ? 0 : 1;
	cbp->ext_stats_dis = 	1;	/* disable extended counters */
	cbp->keep_overrun_rx = 	0;	/* don't pass overrun frames to host */
	cbp->save_bf =		sc->flags & FXP_FLAG_SAVE_BAD ? 1 : prm;
	cbp->disc_short_rx =	!prm;	/* discard short packets */
	cbp->underrun_retry =	1;	/* retry mode (once) on DMA underrun */
	cbp->two_frames =	0;	/* do not limit FIFO to 2 frames */
	cbp->dyn_tbd =		0;	/* (no) dynamic TBD mode */
	cbp->ext_rfa =		sc->flags & FXP_FLAG_EXT_RFA ? 1 : 0;
	cbp->mediatype =	sc->flags & FXP_FLAG_SERIAL_MEDIA ? 0 : 1;
	cbp->csma_dis =		0;	/* (don't) disable link */
	cbp->tcp_udp_cksum =	0;	/* (don't) enable checksum */
	cbp->vlan_tco =		0;	/* (don't) enable vlan wakeup */
	cbp->link_wake_en =	0;	/* (don't) assert PME# on link change */
	cbp->arp_wake_en =	0;	/* (don't) assert PME# on arp */
	cbp->mc_wake_en =	0;	/* (don't) enable PME# on mcmatch */
	cbp->nsai =		1;	/* (don't) disable source addr insert */
	cbp->preamble_length =	2;	/* (7 byte) preamble */
	cbp->loopback =		0;	/* (don't) loopback */
	cbp->linear_priority =	0;	/* (normal CSMA/CD operation) */
	cbp->linear_pri_mode =	0;	/* (wait after xmit only) */
	cbp->interfrm_spacing =	6;	/* (96 bits of) interframe spacing */
	cbp->promiscuous =	prm;	/* promiscuous mode */
	cbp->bcast_disable =	0;	/* (don't) disable broadcasts */
	cbp->wait_after_win =	0;	/* (don't) enable modified backoff alg*/
	cbp->ignore_ul =	0;	/* consider U/L bit in IA matching */
	cbp->crc16_en =		0;	/* (don't) enable crc-16 algorithm */
	cbp->crscdt =		sc->flags & FXP_FLAG_SERIAL_MEDIA ? 1 : 0;

	cbp->stripping =	!prm;	/* truncate rx packet to byte count */
	cbp->padding =		1;	/* (do) pad short tx packets */
	cbp->rcv_crc_xfer =	0;	/* (don't) xfer CRC to host */
	cbp->long_rx_en =	sc->flags & FXP_FLAG_LONG_PKT_EN ? 1 : 0;
	cbp->ia_wake_en =	0;	/* (don't) wake up on address match */
	cbp->magic_pkt_dis =	0;	/* (don't) disable magic packet */
					/* must set wake_en in PMCSR also */
	cbp->force_fdx =	0;	/* (don't) force full duplex */
	cbp->fdx_pin_en =	1;	/* (enable) FDX# pin */
	cbp->multi_ia =		0;	/* (don't) accept multiple IAs */
	cbp->mc_all =		sc->flags & FXP_FLAG_ALL_MCAST ? 1 : 0;
	cbp->gamla_rx =		sc->flags & FXP_FLAG_EXT_RFA ? 1 : 0;

	if (sc->tunable_noflow || sc->revision == FXP_REV_82557) {
		/*
		 * The 82557 has no hardware flow control, the values
		 * below are the defaults for the chip.
		 */
		cbp->fc_delay_lsb =	0;
		cbp->fc_delay_msb =	0x40;
		cbp->pri_fc_thresh =	3;
		cbp->tx_fc_dis =	0;
		cbp->rx_fc_restop =	0;
		cbp->rx_fc_restart =	0;
		cbp->fc_filter =	0;
		cbp->pri_fc_loc =	1;
	} else {
		cbp->fc_delay_lsb =	0x1f;
		cbp->fc_delay_msb =	0x01;
		cbp->pri_fc_thresh =	3;
		cbp->tx_fc_dis =	0;	/* enable transmit FC */
		cbp->rx_fc_restop =	1;	/* enable FC restop frames */
		cbp->rx_fc_restart =	1;	/* enable FC restart frames */
		cbp->fc_filter =	!prm;	/* drop FC frames to host */
		cbp->pri_fc_loc =	1;	/* FC pri location (byte31) */
	}

	/*
	 * Start the config command/DMA.
	 */
	fxp_scb_wait(sc);
	bus_dmamap_sync(sc->cbl_tag, sc->cbl_map, BUS_DMASYNC_PREWRITE);
	CSR_WRITE_4(sc, FXP_CSR_SCB_GENERAL, sc->fxp_desc.cbl_addr);
	fxp_scb_cmd(sc, FXP_SCB_COMMAND_CU_START);
	/* ...and wait for it to complete. */
	fxp_dma_wait(sc, &cbp->cb_status, sc->cbl_tag, sc->cbl_map);
	bus_dmamap_sync(sc->cbl_tag, sc->cbl_map, BUS_DMASYNC_POSTWRITE);

	/*
	 * Now initialize the station address. Temporarily use the TxCB
	 * memory area like we did above for the config CB.
	 */
	cb_ias = (struct fxp_cb_ias *)sc->fxp_desc.cbl_list;
	cb_ias->cb_status = 0;
	cb_ias->cb_command = htole16(FXP_CB_COMMAND_IAS | FXP_CB_COMMAND_EL);
	cb_ias->link_addr = 0xffffffff;
	bcopy(sc->arpcom.ac_enaddr, cb_ias->macaddr,
	    sizeof(sc->arpcom.ac_enaddr));

	/*
	 * Start the IAS (Individual Address Setup) command/DMA.
	 */
	fxp_scb_wait(sc);
	bus_dmamap_sync(sc->cbl_tag, sc->cbl_map, BUS_DMASYNC_PREWRITE);
	fxp_scb_cmd(sc, FXP_SCB_COMMAND_CU_START);
	/* ...and wait for it to complete. */
	fxp_dma_wait(sc, &cb_ias->cb_status, sc->cbl_tag, sc->cbl_map);
	bus_dmamap_sync(sc->cbl_tag, sc->cbl_map, BUS_DMASYNC_POSTWRITE);

	/*
	 * Initialize transmit control block (TxCB) list.
	 */
	txp = sc->fxp_desc.tx_list;
	tcbp = sc->fxp_desc.cbl_list;
	bzero(tcbp, FXP_TXCB_SZ);
	for (i = 0; i < FXP_NTXCB; i++) {
		txp[i].tx_cb = tcbp + i;
		txp[i].tx_mbuf = NULL;
		tcbp[i].cb_status = htole16(FXP_CB_STATUS_C | FXP_CB_STATUS_OK);
		tcbp[i].cb_command = htole16(FXP_CB_COMMAND_NOP);
		tcbp[i].link_addr = htole32(sc->fxp_desc.cbl_addr +
		    (((i + 1) & FXP_TXCB_MASK) * sizeof(struct fxp_cb_tx)));
		if (sc->flags & FXP_FLAG_EXT_TXCB)
			tcbp[i].tbd_array_addr =
			    htole32(FXP_TXCB_DMA_ADDR(sc, &tcbp[i].tbd[2]));
		else
			tcbp[i].tbd_array_addr =
			    htole32(FXP_TXCB_DMA_ADDR(sc, &tcbp[i].tbd[0]));
		txp[i].tx_next = &txp[(i + 1) & FXP_TXCB_MASK];
	}
	/*
	 * Set the suspend flag on the first TxCB and start the control
	 * unit. It will execute the NOP and then suspend.
	 */
	tcbp->cb_command = htole16(FXP_CB_COMMAND_NOP | FXP_CB_COMMAND_S);
	bus_dmamap_sync(sc->cbl_tag, sc->cbl_map, BUS_DMASYNC_PREWRITE);
	sc->fxp_desc.tx_first = sc->fxp_desc.tx_last = txp;
	sc->tx_queued = 1;

	fxp_scb_wait(sc);
	fxp_scb_cmd(sc, FXP_SCB_COMMAND_CU_START);

	/*
	 * Initialize receiver buffer area - RFA.
	 */
	fxp_scb_wait(sc);
	CSR_WRITE_4(sc, FXP_CSR_SCB_GENERAL, sc->fxp_desc.rx_head->rx_addr);
	fxp_scb_cmd(sc, FXP_SCB_COMMAND_RU_START);

	/*
	 * Set current media.
	 */
	if (sc->miibus != NULL)
		mii_mediachg(device_get_softc(sc->miibus));

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	/*
	 * Enable interrupts.
	 */
#ifdef DEVICE_POLLING
	/*
	 * ... but only do that if we are not polling. And because (presumably)
	 * the default is interrupts on, we need to disable them explicitly!
	 */
	if ( ifp->if_flags & IFF_POLLING )
		CSR_WRITE_1(sc, FXP_CSR_SCB_INTRCNTL, FXP_SCB_INTR_DISABLE);
	else
#endif /* DEVICE_POLLING */
	CSR_WRITE_1(sc, FXP_CSR_SCB_INTRCNTL, 0);

	/*
	 * Start stats updater.
	 */
	callout_reset(&sc->stat_ch, hz, fxp_tick, sc);
	splx(s);
}

static int
fxp_serial_ifmedia_upd(struct ifnet *ifp)
{

	return (0);
}

static void
fxp_serial_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{

	ifmr->ifm_active = IFM_ETHER|IFM_MANUAL;
}

/*
 * Change media according to request.
 */
static int
fxp_ifmedia_upd(struct ifnet *ifp)
{
	struct fxp_softc *sc = ifp->if_softc;
	struct mii_data *mii;

	mii = device_get_softc(sc->miibus);
	mii_mediachg(mii);
	return (0);
}

/*
 * Notify the world which media we're using.
 */
static void
fxp_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct fxp_softc *sc = ifp->if_softc;
	struct mii_data *mii;

	mii = device_get_softc(sc->miibus);
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;

	if (ifmr->ifm_status & IFM_10_T && sc->flags & FXP_FLAG_CU_RESUME_BUG)
		sc->cu_resume_bug = 1;
	else
		sc->cu_resume_bug = 0;
}

/*
 * Add a buffer to the end of the RFA buffer list.
 * Return 0 if successful, 1 for failure. A failure results in
 * adding the 'oldm' (if non-NULL) on to the end of the list -
 * tossing out its old contents and recycling it.
 * The RFA struct is stuck at the beginning of mbuf cluster and the
 * data pointer is fixed up to point just past it.
 */
static int
fxp_add_rfabuf(struct fxp_softc *sc, struct fxp_rx *rxp)
{
	struct mbuf *m;
	struct fxp_rfa *rfa, *p_rfa;
	struct fxp_rx *p_rx;
	bus_dmamap_t tmp_map;
	int error;

	m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
	if (m == NULL)
		return (ENOBUFS);

	/*
	 * Move the data pointer up so that the incoming data packet
	 * will be 32-bit aligned.
	 */
	m->m_data += RFA_ALIGNMENT_FUDGE;

	/*
	 * Get a pointer to the base of the mbuf cluster and move
	 * data start past it.
	 */
	rfa = mtod(m, struct fxp_rfa *);
	m->m_data += sc->rfa_size;
	rfa->size = htole16(MCLBYTES - sc->rfa_size - RFA_ALIGNMENT_FUDGE);

	rfa->rfa_status = 0;
	rfa->rfa_control = htole16(FXP_RFA_CONTROL_EL);
	rfa->actual_size = 0;

	/*
	 * Initialize the rest of the RFA.  Note that since the RFA
	 * is misaligned, we cannot store values directly.  We're thus
	 * using the le32enc() function which handles endianness and
	 * is also alignment-safe.
	 */
	le32enc(&rfa->link_addr, 0xffffffff);
	le32enc(&rfa->rbd_addr, 0xffffffff);

	/* Map the RFA into DMA memory. */
	error = bus_dmamap_load(sc->fxp_mtag, sc->spare_map, rfa,
	    MCLBYTES - RFA_ALIGNMENT_FUDGE, fxp_dma_map_addr,
	    &rxp->rx_addr, 0);
	if (error) {
		m_freem(m);
		return (error);
	}

	bus_dmamap_unload(sc->fxp_mtag, rxp->rx_map);
	tmp_map = sc->spare_map;
	sc->spare_map = rxp->rx_map;
	rxp->rx_map = tmp_map;
	rxp->rx_mbuf = m;

	bus_dmamap_sync(sc->fxp_mtag, rxp->rx_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	/*
	 * If there are other buffers already on the list, attach this
	 * one to the end by fixing up the tail to point to this one.
	 */
	if (sc->fxp_desc.rx_head != NULL) {
		p_rx = sc->fxp_desc.rx_tail;
		p_rfa = (struct fxp_rfa *)
		    (p_rx->rx_mbuf->m_ext.ext_buf + RFA_ALIGNMENT_FUDGE);
		p_rx->rx_next = rxp;
		le32enc(&p_rfa->link_addr, rxp->rx_addr);
		p_rfa->rfa_control = 0;
		bus_dmamap_sync(sc->fxp_mtag, p_rx->rx_map,
		    BUS_DMASYNC_PREWRITE);
	} else {
		rxp->rx_next = NULL;
		sc->fxp_desc.rx_head = rxp;
	}
	sc->fxp_desc.rx_tail = rxp;
	return (0);
}

static volatile int
fxp_miibus_readreg(device_t dev, int phy, int reg)
{
	struct fxp_softc *sc = device_get_softc(dev);
	int count = 10000;
	int value;

	CSR_WRITE_4(sc, FXP_CSR_MDICONTROL,
	    (FXP_MDI_READ << 26) | (reg << 16) | (phy << 21));

	while (((value = CSR_READ_4(sc, FXP_CSR_MDICONTROL)) & 0x10000000) == 0
	    && count--)
		DELAY(10);

	if (count <= 0)
		device_printf(dev, "fxp_miibus_readreg: timed out\n");

	return (value & 0xffff);
}

static void
fxp_miibus_writereg(device_t dev, int phy, int reg, int value)
{
	struct fxp_softc *sc = device_get_softc(dev);
	int count = 10000;

	CSR_WRITE_4(sc, FXP_CSR_MDICONTROL,
	    (FXP_MDI_WRITE << 26) | (reg << 16) | (phy << 21) |
	    (value & 0xffff));

	while ((CSR_READ_4(sc, FXP_CSR_MDICONTROL) & 0x10000000) == 0 &&
	    count--)
		DELAY(10);

	if (count <= 0)
		device_printf(dev, "fxp_miibus_writereg: timed out\n");
}

static int
fxp_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct fxp_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	struct mii_data *mii;
	int flag, mask, s, error = 0;

	/*
	 * Detaching causes us to call ioctl with the mutex owned.  Preclude
	 * that by saying we're busy if the lock is already held.
	 */
	if (FXP_LOCKED(sc))
		return (EBUSY);

	FXP_LOCK(sc);
	s = splimp();

	switch (command) {
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_ALLMULTI)
			sc->flags |= FXP_FLAG_ALL_MCAST;
		else
			sc->flags &= ~FXP_FLAG_ALL_MCAST;

		/*
		 * If interface is marked up and not running, then start it.
		 * If it is marked down and running, stop it.
		 * XXX If it's up then re-initialize it. This is so flags
		 * such as IFF_PROMISC are handled.
		 */
		if (ifp->if_flags & IFF_UP) {
			fxp_init_body(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				fxp_stop(sc);
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifp->if_flags & IFF_ALLMULTI)
			sc->flags |= FXP_FLAG_ALL_MCAST;
		else
			sc->flags &= ~FXP_FLAG_ALL_MCAST;
		/*
		 * Multicast list has changed; set the hardware filter
		 * accordingly.
		 */
		if ((sc->flags & FXP_FLAG_ALL_MCAST) == 0)
			fxp_mc_setup(sc);
		/*
		 * fxp_mc_setup() can set FXP_FLAG_ALL_MCAST, so check it
		 * again rather than else {}.
		 */
		if (sc->flags & FXP_FLAG_ALL_MCAST)
			fxp_init_body(sc);
		error = 0;
		break;

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		if (sc->miibus != NULL) {
			mii = device_get_softc(sc->miibus);
                        error = ifmedia_ioctl(ifp, ifr,
                            &mii->mii_media, command);
		} else {
                        error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, command);
		}
		break;

	case SIOCSIFCAP:
		mask = ifp->if_capenable ^ ifr->ifr_reqcap;
		if (mask & IFCAP_POLLING)
			ifp->if_capenable ^= IFCAP_POLLING;
		if (mask & IFCAP_VLAN_MTU) {
			ifp->if_capenable ^= IFCAP_VLAN_MTU;
			if (sc->revision != FXP_REV_82557)
				flag = FXP_FLAG_LONG_PKT_EN;
			else /* a hack to get long frames on the old chip */
				flag = FXP_FLAG_SAVE_BAD;
			sc->flags ^= flag;
			if (ifp->if_flags & IFF_UP)
				fxp_init_body(sc);
		}
		break;

	default:
		/* 
		 * ether_ioctl() will eventually call fxp_start() which
		 * will result in mutex recursion so drop it first.
		 */
		FXP_UNLOCK(sc);
		error = ether_ioctl(ifp, command, data);
	}
	if (FXP_LOCKED(sc))
		FXP_UNLOCK(sc);
	splx(s);
	return (error);
}

/*
 * Fill in the multicast address list and return number of entries.
 */
static int
fxp_mc_addrs(struct fxp_softc *sc)
{
	struct fxp_cb_mcs *mcsp = sc->mcsp;
	struct ifnet *ifp = &sc->sc_if;
	struct ifmultiaddr *ifma;
	int nmcasts;

	nmcasts = 0;
	if ((sc->flags & FXP_FLAG_ALL_MCAST) == 0) {
#if __FreeBSD_version < 500000
		LIST_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
#else
		TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
#endif
			if (ifma->ifma_addr->sa_family != AF_LINK)
				continue;
			if (nmcasts >= MAXMCADDR) {
				sc->flags |= FXP_FLAG_ALL_MCAST;
				nmcasts = 0;
				break;
			}
			bcopy(LLADDR((struct sockaddr_dl *)ifma->ifma_addr),
			    &sc->mcsp->mc_addr[nmcasts][0], ETHER_ADDR_LEN);
			nmcasts++;
		}
	}
	mcsp->mc_cnt = htole16(nmcasts * ETHER_ADDR_LEN);
	return (nmcasts);
}

/*
 * Program the multicast filter.
 *
 * We have an artificial restriction that the multicast setup command
 * must be the first command in the chain, so we take steps to ensure
 * this. By requiring this, it allows us to keep up the performance of
 * the pre-initialized command ring (esp. link pointers) by not actually
 * inserting the mcsetup command in the ring - i.e. its link pointer
 * points to the TxCB ring, but the mcsetup descriptor itself is not part
 * of it. We then can do 'CU_START' on the mcsetup descriptor and have it
 * lead into the regular TxCB ring when it completes.
 *
 * This function must be called at splimp.
 */
static void
fxp_mc_setup(struct fxp_softc *sc)
{
	struct fxp_cb_mcs *mcsp = sc->mcsp;
	struct ifnet *ifp = &sc->sc_if;
	struct fxp_tx *txp;
	int count;

	FXP_LOCK_ASSERT(sc, MA_OWNED);
	/*
	 * If there are queued commands, we must wait until they are all
	 * completed. If we are already waiting, then add a NOP command
	 * with interrupt option so that we're notified when all commands
	 * have been completed - fxp_start() ensures that no additional
	 * TX commands will be added when need_mcsetup is true.
	 */
	if (sc->tx_queued) {
		/*
		 * need_mcsetup will be true if we are already waiting for the
		 * NOP command to be completed (see below). In this case, bail.
		 */
		if (sc->need_mcsetup)
			return;
		sc->need_mcsetup = 1;

		/*
		 * Add a NOP command with interrupt so that we are notified
		 * when all TX commands have been processed.
		 */
		txp = sc->fxp_desc.tx_last->tx_next;
		txp->tx_mbuf = NULL;
		txp->tx_cb->cb_status = 0;
		txp->tx_cb->cb_command = htole16(FXP_CB_COMMAND_NOP |
		    FXP_CB_COMMAND_S | FXP_CB_COMMAND_I);
		/*
		 * Advance the end of list forward.
		 */
		sc->fxp_desc.tx_last->tx_cb->cb_command &=
		    htole16(~FXP_CB_COMMAND_S);
		bus_dmamap_sync(sc->cbl_tag, sc->cbl_map, BUS_DMASYNC_PREWRITE);
		sc->fxp_desc.tx_last = txp;
		sc->tx_queued++;
		/*
		 * Issue a resume in case the CU has just suspended.
		 */
		fxp_scb_wait(sc);
		fxp_scb_cmd(sc, FXP_SCB_COMMAND_CU_RESUME);
		/*
		 * Set a 5 second timer just in case we don't hear from the
		 * card again.
		 */
		ifp->if_timer = 5;

		return;
	}
	sc->need_mcsetup = 0;

	/*
	 * Initialize multicast setup descriptor.
	 */
	mcsp->cb_status = 0;
	mcsp->cb_command = htole16(FXP_CB_COMMAND_MCAS |
	    FXP_CB_COMMAND_S | FXP_CB_COMMAND_I);
	mcsp->link_addr = htole32(sc->fxp_desc.cbl_addr);
	txp = &sc->fxp_desc.mcs_tx;
	txp->tx_mbuf = NULL;
	txp->tx_cb = (struct fxp_cb_tx *)sc->mcsp;
	txp->tx_next = sc->fxp_desc.tx_list;
	(void) fxp_mc_addrs(sc);
	sc->fxp_desc.tx_first = sc->fxp_desc.tx_last = txp;
	sc->tx_queued = 1;

	/*
	 * Wait until command unit is not active. This should never
	 * be the case when nothing is queued, but make sure anyway.
	 */
	count = 100;
	while ((CSR_READ_1(sc, FXP_CSR_SCB_RUSCUS) >> 6) ==
	    FXP_SCB_CUS_ACTIVE && --count)
		DELAY(10);
	if (count == 0) {
		device_printf(sc->dev, "command queue timeout\n");
		return;
	}

	/*
	 * Start the multicast setup command.
	 */
	fxp_scb_wait(sc);
	bus_dmamap_sync(sc->mcs_tag, sc->mcs_map, BUS_DMASYNC_PREWRITE);
	CSR_WRITE_4(sc, FXP_CSR_SCB_GENERAL, sc->mcs_addr);
	fxp_scb_cmd(sc, FXP_SCB_COMMAND_CU_START);

	ifp->if_timer = 2;
	return;
}

static u_int32_t fxp_ucode_d101a[] = D101_A_RCVBUNDLE_UCODE;
static u_int32_t fxp_ucode_d101b0[] = D101_B0_RCVBUNDLE_UCODE;
static u_int32_t fxp_ucode_d101ma[] = D101M_B_RCVBUNDLE_UCODE;
static u_int32_t fxp_ucode_d101s[] = D101S_RCVBUNDLE_UCODE;
static u_int32_t fxp_ucode_d102[] = D102_B_RCVBUNDLE_UCODE;
static u_int32_t fxp_ucode_d102c[] = D102_C_RCVBUNDLE_UCODE;

#define UCODE(x)	x, sizeof(x)/sizeof(u_int32_t)

struct ucode {
	u_int32_t	revision;
	u_int32_t	*ucode;
	int		length;
	u_short		int_delay_offset;
	u_short		bundle_max_offset;
} ucode_table[] = {
	{ FXP_REV_82558_A4, UCODE(fxp_ucode_d101a), D101_CPUSAVER_DWORD, 0 },
	{ FXP_REV_82558_B0, UCODE(fxp_ucode_d101b0), D101_CPUSAVER_DWORD, 0 },
	{ FXP_REV_82559_A0, UCODE(fxp_ucode_d101ma),
	    D101M_CPUSAVER_DWORD, D101M_CPUSAVER_BUNDLE_MAX_DWORD },
	{ FXP_REV_82559S_A, UCODE(fxp_ucode_d101s),
	    D101S_CPUSAVER_DWORD, D101S_CPUSAVER_BUNDLE_MAX_DWORD },
	{ FXP_REV_82550, UCODE(fxp_ucode_d102),
	    D102_B_CPUSAVER_DWORD, D102_B_CPUSAVER_BUNDLE_MAX_DWORD },
	{ FXP_REV_82550_C, UCODE(fxp_ucode_d102c),
	    D102_C_CPUSAVER_DWORD, D102_C_CPUSAVER_BUNDLE_MAX_DWORD },
	{ 0, NULL, 0, 0, 0 }
};

static void
fxp_load_ucode(struct fxp_softc *sc)
{
	struct ucode *uc;
	struct fxp_cb_ucode *cbp;
	int i;

	for (uc = ucode_table; uc->ucode != NULL; uc++)
		if (sc->revision == uc->revision)
			break;
	if (uc->ucode == NULL)
		return;
	cbp = (struct fxp_cb_ucode *)sc->fxp_desc.cbl_list;
	cbp->cb_status = 0;
	cbp->cb_command = htole16(FXP_CB_COMMAND_UCODE | FXP_CB_COMMAND_EL);
	cbp->link_addr = 0xffffffff;    	/* (no) next command */
	for (i = 0; i < uc->length; i++)
		cbp->ucode[i] = htole32(uc->ucode[i]);
	if (uc->int_delay_offset)
		*(u_int16_t *)&cbp->ucode[uc->int_delay_offset] =
		    htole16(sc->tunable_int_delay + sc->tunable_int_delay / 2);
	if (uc->bundle_max_offset)
		*(u_int16_t *)&cbp->ucode[uc->bundle_max_offset] =
		    htole16(sc->tunable_bundle_max);
	/*
	 * Download the ucode to the chip.
	 */
	fxp_scb_wait(sc);
	bus_dmamap_sync(sc->cbl_tag, sc->cbl_map, BUS_DMASYNC_PREWRITE);
	CSR_WRITE_4(sc, FXP_CSR_SCB_GENERAL, sc->fxp_desc.cbl_addr);
	fxp_scb_cmd(sc, FXP_SCB_COMMAND_CU_START);
	/* ...and wait for it to complete. */
	fxp_dma_wait(sc, &cbp->cb_status, sc->cbl_tag, sc->cbl_map);
	bus_dmamap_sync(sc->cbl_tag, sc->cbl_map, BUS_DMASYNC_POSTWRITE);
	device_printf(sc->dev,
	    "Microcode loaded, int_delay: %d usec  bundle_max: %d\n",
	    sc->tunable_int_delay, 
	    uc->bundle_max_offset == 0 ? 0 : sc->tunable_bundle_max);
	sc->flags |= FXP_FLAG_UCODE;
}

static int
sysctl_int_range(SYSCTL_HANDLER_ARGS, int low, int high)
{
	int error, value;

	value = *(int *)arg1;
	error = sysctl_handle_int(oidp, &value, 0, req);
	if (error || !req->newptr)
		return (error);
	if (value < low || value > high)
		return (EINVAL);
	*(int *)arg1 = value;
	return (0);
}

/*
 * Interrupt delay is expressed in microseconds, a multiplier is used
 * to convert this to the appropriate clock ticks before using. 
 */
static int
sysctl_hw_fxp_int_delay(SYSCTL_HANDLER_ARGS)
{
	return (sysctl_int_range(oidp, arg1, arg2, req, 300, 3000));
}

static int
sysctl_hw_fxp_bundle_max(SYSCTL_HANDLER_ARGS)
{
	return (sysctl_int_range(oidp, arg1, arg2, req, 1, 0xffff));
}
