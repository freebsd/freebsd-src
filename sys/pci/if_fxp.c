/*
 * Copyright (c) 1995, David Greenman
 * All rights reserved.
 *
 * Modifications to support NetBSD and media selection:
 * Copyright (c) 1997 Jason R. Thorpe.  All rights reserved.
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
 * $FreeBSD$
 */

/*
 * Intel EtherExpress Pro/100B PCI Fast Ethernet driver
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#include <net/bpf.h>

#if defined(__NetBSD__)

#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <net/if_dl.h>
#include <net/if_ether.h>

#include <netinet/if_inarp.h>

#include <vm/vm.h>

#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/pci/if_fxpreg.h>
#include <dev/pci/if_fxpvar.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>


#else /* __FreeBSD__ */

#include <sys/sockio.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <sys/proc.h>
#include <machine/mutex.h>

#include <net/ethernet.h>
#include <net/if_arp.h>

#include <vm/vm.h>		/* for vtophys */
#include <vm/pmap.h>		/* for vtophys */
#include <machine/clock.h>	/* for DELAY */

#include <pci/pcivar.h>
#include <pci/pcireg.h>		/* for PCIM_CMD_xxx */
#include <pci/if_fxpreg.h>
#include <pci/if_fxpvar.h>

#endif /* __NetBSD__ */

#ifdef __alpha__		/* XXX */
/* XXX XXX NEED REAL DMA MAPPING SUPPORT XXX XXX */
#undef vtophys
#define	vtophys(va)	alpha_XXX_dmamap((vm_offset_t)(va))
#endif /* __alpha__ */

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
 * Inline function to copy a 16-bit aligned 32-bit quantity.
 */
static __inline void fxp_lwcopy __P((volatile u_int32_t *,
	volatile u_int32_t *));
static __inline void
fxp_lwcopy(src, dst)
	volatile u_int32_t *src, *dst;
{
#ifdef __i386__
	*dst = *src;
#else
	volatile u_int16_t *a = (volatile u_int16_t *)src;
	volatile u_int16_t *b = (volatile u_int16_t *)dst;

	b[0] = a[0];
	b[1] = a[1];
#endif
}

/*
 * Template for default configuration parameters.
 * See struct fxp_cb_config for the bit definitions.
 */
static u_char fxp_cb_config_template[] = {
	0x0, 0x0,		/* cb_status */
	0x80, 0x2,		/* cb_command */
	0xff, 0xff, 0xff, 0xff,	/* link_addr */
	0x16,	/*  0 */
	0x8,	/*  1 */
	0x0,	/*  2 */
	0x0,	/*  3 */
	0x0,	/*  4 */
	0x80,	/*  5 */
	0xb2,	/*  6 */
	0x3,	/*  7 */
	0x1,	/*  8 */
	0x0,	/*  9 */
	0x26,	/* 10 */
	0x0,	/* 11 */
	0x60,	/* 12 */
	0x0,	/* 13 */
	0xf2,	/* 14 */
	0x48,	/* 15 */
	0x0,	/* 16 */
	0x40,	/* 17 */
	0xf3,	/* 18 */
	0x0,	/* 19 */
	0x3f,	/* 20 */
	0x5	/* 21 */
};

/* Supported media types. */
struct fxp_supported_media {
	const int	fsm_phy;	/* PHY type */
	const int	*fsm_media;	/* the media array */
	const int	fsm_nmedia;	/* the number of supported media */
	const int	fsm_defmedia;	/* default media for this PHY */
};

static const int fxp_media_standard[] = {
	IFM_ETHER|IFM_10_T,
	IFM_ETHER|IFM_10_T|IFM_FDX,
	IFM_ETHER|IFM_100_TX,
	IFM_ETHER|IFM_100_TX|IFM_FDX,
	IFM_ETHER|IFM_AUTO,
};
#define	FXP_MEDIA_STANDARD_DEFMEDIA	(IFM_ETHER|IFM_AUTO)

static const int fxp_media_default[] = {
	IFM_ETHER|IFM_MANUAL,		/* XXX IFM_AUTO ? */
};
#define	FXP_MEDIA_DEFAULT_DEFMEDIA	(IFM_ETHER|IFM_MANUAL)

static const struct fxp_supported_media fxp_media[] = {
	{ FXP_PHY_DP83840, fxp_media_standard,
	  sizeof(fxp_media_standard) / sizeof(fxp_media_standard[0]),
	  FXP_MEDIA_STANDARD_DEFMEDIA },
	{ FXP_PHY_DP83840A, fxp_media_standard,
	  sizeof(fxp_media_standard) / sizeof(fxp_media_standard[0]),
	  FXP_MEDIA_STANDARD_DEFMEDIA },
	{ FXP_PHY_82553A, fxp_media_standard,
	  sizeof(fxp_media_standard) / sizeof(fxp_media_standard[0]),
	  FXP_MEDIA_STANDARD_DEFMEDIA },
	{ FXP_PHY_82553C, fxp_media_standard,
	  sizeof(fxp_media_standard) / sizeof(fxp_media_standard[0]),
	  FXP_MEDIA_STANDARD_DEFMEDIA },
	{ FXP_PHY_82555, fxp_media_standard,
	  sizeof(fxp_media_standard) / sizeof(fxp_media_standard[0]),
	  FXP_MEDIA_STANDARD_DEFMEDIA },
	{ FXP_PHY_82555B, fxp_media_standard,
	  sizeof(fxp_media_standard) / sizeof(fxp_media_standard[0]),
	  FXP_MEDIA_STANDARD_DEFMEDIA },
	{ FXP_PHY_80C24, fxp_media_default,
	  sizeof(fxp_media_default) / sizeof(fxp_media_default[0]),
	  FXP_MEDIA_DEFAULT_DEFMEDIA },
};
#define	NFXPMEDIA (sizeof(fxp_media) / sizeof(fxp_media[0]))

static int fxp_mediachange	__P((struct ifnet *));
static void fxp_mediastatus	__P((struct ifnet *, struct ifmediareq *));
static void fxp_set_media	__P((struct fxp_softc *, int));
static __inline void fxp_scb_wait __P((struct fxp_softc *));
static __inline void fxp_dma_wait __P((volatile u_int16_t *, struct fxp_softc *sc));
static FXP_INTR_TYPE fxp_intr	__P((void *));
static void fxp_start		__P((struct ifnet *));
static int fxp_ioctl		__P((struct ifnet *,
				     FXP_IOCTLCMD_TYPE, caddr_t));
static void fxp_init		__P((void *));
static void fxp_stop		__P((struct fxp_softc *));
static void fxp_watchdog	__P((struct ifnet *));
static int fxp_add_rfabuf	__P((struct fxp_softc *, struct mbuf *));
static int fxp_mdi_read		__P((struct fxp_softc *, int, int));
static void fxp_mdi_write	__P((struct fxp_softc *, int, int, int));
static void fxp_autosize_eeprom __P((struct fxp_softc *));
static void fxp_read_eeprom	__P((struct fxp_softc *, u_int16_t *,
				     int, int));
static int fxp_attach_common	__P((struct fxp_softc *, u_int8_t *));
static void fxp_stats_update	__P((void *));
static void fxp_mc_setup	__P((struct fxp_softc *));

/*
 * Set initial transmit threshold at 64 (512 bytes). This is
 * increased by 64 (512 bytes) at a time, to maximum of 192
 * (1536 bytes), if an underrun occurs.
 */
static int tx_threshold = 64;

/*
 * Number of transmit control blocks. This determines the number
 * of transmit buffers that can be chained in the CB list.
 * This must be a power of two.
 */
#define FXP_NTXCB	128

/*
 * Number of completed TX commands at which point an interrupt
 * will be generated to garbage collect the attached buffers.
 * Must be at least one less than FXP_NTXCB, and should be
 * enough less so that the transmitter doesn't becomes idle
 * during the buffer rundown (which would reduce performance).
 */
#define FXP_CXINT_THRESH 120

/*
 * TxCB list index mask. This is used to do list wrap-around.
 */
#define FXP_TXCB_MASK	(FXP_NTXCB - 1)

/*
 * Number of receive frame area buffers. These are large so chose
 * wisely.
 */
#define FXP_NRFABUFS	64

/*
 * Maximum number of seconds that the receiver can be idle before we
 * assume it's dead and attempt to reset it by reprogramming the
 * multicast filter. This is part of a work-around for a bug in the
 * NIC. See fxp_stats_update().
 */
#define FXP_MAX_RX_IDLE	15

/*
 * Wait for the previous command to be accepted (but not necessarily
 * completed).
 */
static __inline void
fxp_scb_wait(sc)
	struct fxp_softc *sc;
{
	int i = 10000;

	while (CSR_READ_1(sc, FXP_CSR_SCB_COMMAND) && --i)
		DELAY(2);
	if (i == 0)
		printf(FXP_FORMAT ": SCB timeout\n", FXP_ARGS(sc));
}

static __inline void
fxp_dma_wait(status, sc)
	volatile u_int16_t *status;
	struct fxp_softc *sc;
{
	int i = 10000;

	while (!(*status & FXP_CB_STATUS_C) && --i)
		DELAY(2);
	if (i == 0)
		printf(FXP_FORMAT ": DMA timeout\n", FXP_ARGS(sc));
}

/*************************************************************
 * Operating system-specific autoconfiguration glue
 *************************************************************/

#if defined(__NetBSD__)

#ifdef __BROKEN_INDIRECT_CONFIG
static int fxp_match __P((struct device *, void *, void *));
#else
static int fxp_match __P((struct device *, struct cfdata *, void *));
#endif
static void fxp_attach __P((struct device *, struct device *, void *));

static void	fxp_shutdown __P((void *));

/* Compensate for lack of a generic ether_ioctl() */
static int	fxp_ether_ioctl __P((struct ifnet *,
				    FXP_IOCTLCMD_TYPE, caddr_t));
#define	ether_ioctl	fxp_ether_ioctl

struct cfattach fxp_ca = {
	sizeof(struct fxp_softc), fxp_match, fxp_attach
};

struct cfdriver fxp_cd = {
	NULL, "fxp", DV_IFNET
};

/*
 * Check if a device is an 82557.
 */
static int
fxp_match(parent, match, aux)
	struct device *parent;
#ifdef __BROKEN_INDIRECT_CONFIG
	void *match;
#else
	struct cfdata *match;
#endif
	void *aux;
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) != PCI_VENDOR_INTEL)
		return (0);

	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_INTEL_82557:
		return (1);
	}

	return (0);
}

static void
fxp_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct fxp_softc *sc = (struct fxp_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	u_int8_t enaddr[6];
	struct ifnet *ifp;

	/*
	 * Map control/status registers.
	 */
	if (pci_mapreg_map(pa, FXP_PCI_MMBA, PCI_MAPREG_TYPE_MEM, 0,
	    &sc->sc_st, &sc->sc_sh, NULL, NULL)) {
		printf(": can't map registers\n");
		return;
	}
	printf(": Intel EtherExpress Pro 10/100B Ethernet\n");

	/*
	 * Allocate our interrupt.
	 */
	if (pci_intr_map(pc, pa->pa_intrtag, pa->pa_intrpin,
	    pa->pa_intrline, &ih)) {
		printf("%s: couldn't map interrupt\n", sc->sc_dev.dv_xname);
		return;
	}
	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, fxp_intr, sc);
	if (sc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt",
		    sc->sc_dev.dv_xname);
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf("%s: interrupting at %s\n", sc->sc_dev.dv_xname, intrstr);

	/* Do generic parts of attach. */
	if (fxp_attach_common(sc, enaddr)) {
		/* Failed! */
		return;
	}

	printf("%s: Ethernet address %s%s\n", sc->sc_dev.dv_xname,
	    ether_sprintf(enaddr), sc->phy_10Mbps_only ? ", 10Mbps" : "");

	ifp = &sc->sc_ethercom.ec_if;
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = fxp_ioctl;
	ifp->if_start = fxp_start;
	ifp->if_watchdog = fxp_watchdog;

	/*
	 * Attach the interface.
	 */
	if_attach(ifp);
	/*
	 * Let the system queue as many packets as we have available
	 * TX descriptors.
	 */
	ifp->if_snd.ifq_maxlen = FXP_NTXCB - 1;
	ether_ifattach(ifp, enaddr);
	bpfattach(&sc->sc_ethercom.ec_if.if_bpf, ifp, DLT_EN10MB,
	    sizeof(struct ether_header));

	/*
	 * Add shutdown hook so that DMA is disabled prior to reboot. Not
	 * doing do could allow DMA to corrupt kernel memory during the
	 * reboot before the driver initializes.
	 */
	shutdownhook_establish(fxp_shutdown, sc);
}

/*
 * Device shutdown routine. Called at system shutdown after sync. The
 * main purpose of this routine is to shut off receiver DMA so that
 * kernel memory doesn't get clobbered during warmboot.
 */
static void
fxp_shutdown(sc)
	void *sc;
{
	fxp_stop((struct fxp_softc *) sc);
}

static int
fxp_ether_ioctl(ifp, cmd, data)
	struct ifnet *ifp;
	FXP_IOCTLCMD_TYPE cmd;
	caddr_t data;
{
	struct ifaddr *ifa = (struct ifaddr *) data;
	struct fxp_softc *sc = ifp->if_softc;

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			fxp_init(sc);
			arp_ifinit(ifp, ifa);
			break;
#endif
#ifdef NS
		case AF_NS:
		    {
			 register struct ns_addr *ina = &IA_SNS(ifa)->sns_addr;

			 if (ns_nullhost(*ina))
				ina->x_host = *(union ns_host *)
				    LLADDR(ifp->if_sadl);
			 else
				bcopy(ina->x_host.c_host, LLADDR(ifp->if_sadl),
				    ifp->if_addrlen);
			 /* Set new address. */
			 fxp_init(sc);
			 break;
		    }
#endif
		default:
			fxp_init(sc);
			break;
		}
		break;

	default:
		return (EINVAL);
	}

	return (0);
}

#else /* __FreeBSD__ */

/*
 * Return identification string if this is device is ours.
 */
static int
fxp_probe(device_t dev)
{
	if (pci_get_vendor(dev) == FXP_VENDORID_INTEL) {
		switch (pci_get_device(dev)) {

		case FXP_DEVICEID_i82557:
			device_set_desc(dev, "Intel Pro 10/100B/100+ Ethernet");
			return 0;
		case FXP_DEVICEID_i82559:
			device_set_desc(dev, "Intel InBusiness 10/100 Ethernet");
			return 0;
		case FXP_DEVICEID_i82559ER:
			device_set_desc(dev, "Intel Embedded 10/100 Ethernet");
			return 0;
		default:
			break;
		}
	}

	return ENXIO;
}

static int
fxp_attach(device_t dev)
{
	int error = 0;
	struct fxp_softc *sc = device_get_softc(dev);
	struct ifnet *ifp;
	FXP_SPLVAR(s)
	u_long val;
	int rid;

#if !defined(__NetBSD__)
	mtx_init(&sc->sc_mtx, "fxp", MTX_DEF);
#endif
	callout_handle_init(&sc->stat_ch);

	FXP_LOCK(sc, s);

	/*
	 * Enable bus mastering.
	 */
	val = pci_read_config(dev, PCIR_COMMAND, 2);
	val |= (PCIM_CMD_MEMEN|PCIM_CMD_BUSMASTEREN);
	pci_write_config(dev, PCIR_COMMAND, val, 2);

	/*
	 * Map control/status registers.
	 */
	rid = FXP_PCI_MMBA;
	sc->mem = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid,
				     0, ~0, 1, RF_ACTIVE);
	if (!sc->mem) {
		device_printf(dev, "could not map memory\n");
		error = ENXIO;
		goto fail;
        }

	sc->sc_st = rman_get_bustag(sc->mem);
	sc->sc_sh = rman_get_bushandle(sc->mem);

	/*
	 * Allocate our interrupt.
	 */
	rid = 0;
	sc->irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
				 RF_SHAREABLE | RF_ACTIVE);
	if (sc->irq == NULL) {
		device_printf(dev, "could not map interrupt\n");
		error = ENXIO;
		goto fail;
	}

	error = bus_setup_intr(dev, sc->irq, INTR_TYPE_NET,
			       fxp_intr, sc, &sc->ih);
	if (error) {
		device_printf(dev, "could not setup irq\n");
		goto fail;
	}

	/* Do generic parts of attach. */
	if (fxp_attach_common(sc, sc->arpcom.ac_enaddr)) {
		/* Failed! */
		bus_teardown_intr(dev, sc->irq, sc->ih);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irq);
		bus_release_resource(dev, SYS_RES_MEMORY, FXP_PCI_MMBA, sc->mem);
		error = ENXIO;
		goto fail;
	}

	device_printf(dev, "Ethernet address %6D%s\n",
	    sc->arpcom.ac_enaddr, ":", sc->phy_10Mbps_only ? ", 10Mbps" : "");

	ifp = &sc->arpcom.ac_if;
	ifp->if_unit = device_get_unit(dev);
	ifp->if_name = "fxp";
	ifp->if_output = ether_output;
	ifp->if_baudrate = 100000000;
	ifp->if_init = fxp_init;
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = fxp_ioctl;
	ifp->if_start = fxp_start;
	ifp->if_watchdog = fxp_watchdog;

	/*
	 * Attach the interface.
	 */
	ether_ifattach(ifp, ETHER_BPF_SUPPORTED);
	/*
	 * Let the system queue as many packets as we have available
	 * TX descriptors.
	 */
	ifp->if_snd.ifq_maxlen = FXP_NTXCB - 1;

	FXP_UNLOCK(sc, s);
	return 0;

 fail:
	FXP_UNLOCK(sc, s);
	mtx_destroy(&sc->sc_mtx);
	return error;
}

/*
 * Detach interface.
 */
static int
fxp_detach(device_t dev)
{
	struct fxp_softc *sc = device_get_softc(dev);
	FXP_SPLVAR(s)

	FXP_LOCK(sc, s);

	/*
	 * Close down routes etc.
	 */
	ether_ifdetach(&sc->arpcom.ac_if, ETHER_BPF_SUPPORTED);

	/*
	 * Stop DMA and drop transmit queue.
	 */
	fxp_stop(sc);

	/*
	 * Deallocate resources.
	 */
	bus_teardown_intr(dev, sc->irq, sc->ih);
	bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irq);
	bus_release_resource(dev, SYS_RES_MEMORY, FXP_PCI_MMBA, sc->mem);

	/*
	 * Free all the receive buffers.
	 */
	if (sc->rfa_headm != NULL)
		m_freem(sc->rfa_headm);

	/*
	 * Free all media structures.
	 */
	ifmedia_removeall(&sc->sc_media);

	/*
	 * Free anciliary structures.
	 */
	free(sc->cbl_base, M_DEVBUF);
	free(sc->fxp_stats, M_DEVBUF);
	free(sc->mcsp, M_DEVBUF);

	FXP_UNLOCK(sc, s);

	return 0;
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
	return 0;
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
	int i;
	FXP_SPLVAR(s)

	FXP_LOCK(sc, s);

	fxp_stop(sc);
	
	for (i=0; i<5; i++)
		sc->saved_maps[i] = pci_read_config(dev, PCIR_MAPS + i*4, 4);
	sc->saved_biosaddr = pci_read_config(dev, PCIR_BIOS, 4);
	sc->saved_intline = pci_read_config(dev, PCIR_INTLINE, 1);
	sc->saved_cachelnsz = pci_read_config(dev, PCIR_CACHELNSZ, 1);
	sc->saved_lattimer = pci_read_config(dev, PCIR_LATTIMER, 1);

	sc->suspended = 1;

	FXP_UNLOCK(sc, s);

	return 0;
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
	int i;
	FXP_SPLVAR(s)

	FXP_LOCK(sc, s);

	/* better way to do this? */
	for (i=0; i<5; i++)
		pci_write_config(dev, PCIR_MAPS + i*4, sc->saved_maps[i], 4);
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
		fxp_init(sc);

	sc->suspended = 0;

	FXP_UNLOCK(sc, s);

	return 0;
}

static device_method_t fxp_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		fxp_probe),
	DEVMETHOD(device_attach,	fxp_attach),
	DEVMETHOD(device_detach,	fxp_detach),
	DEVMETHOD(device_shutdown,	fxp_shutdown),
	DEVMETHOD(device_suspend,	fxp_suspend),
	DEVMETHOD(device_resume,	fxp_resume),

	{ 0, 0 }
};

static driver_t fxp_driver = {
	"fxp",
	fxp_methods,
	sizeof(struct fxp_softc),
};

static devclass_t fxp_devclass;

DRIVER_MODULE(if_fxp, pci, fxp_driver, fxp_devclass, 0, 0);

#endif /* __NetBSD__ */

/*************************************************************
 * End of operating system-specific autoconfiguration glue
 *************************************************************/

/*
 * Do generic parts of attach.
 */
static int
fxp_attach_common(sc, enaddr)
	struct fxp_softc *sc;
	u_int8_t *enaddr;
{
	u_int16_t data;
	int i, nmedia, defmedia;
	const int *media;

	/*
	 * Reset to a stable state.
	 */
	CSR_WRITE_4(sc, FXP_CSR_PORT, FXP_PORT_SELECTIVE_RESET);
	DELAY(10);

	sc->cbl_base = malloc(sizeof(struct fxp_cb_tx) * FXP_NTXCB,
	    M_DEVBUF, M_NOWAIT);
	if (sc->cbl_base == NULL)
		goto fail;
	bzero(sc->cbl_base, sizeof(struct fxp_cb_tx) * FXP_NTXCB);

	sc->fxp_stats = malloc(sizeof(struct fxp_stats), M_DEVBUF, M_NOWAIT);
	if (sc->fxp_stats == NULL)
		goto fail;
	bzero(sc->fxp_stats, sizeof(struct fxp_stats));

	sc->mcsp = malloc(sizeof(struct fxp_cb_mcs), M_DEVBUF, M_NOWAIT);
	if (sc->mcsp == NULL)
		goto fail;

	/*
	 * Pre-allocate our receive buffers.
	 */
	for (i = 0; i < FXP_NRFABUFS; i++) {
		if (fxp_add_rfabuf(sc, NULL) != 0) {
			goto fail;
		}
	}

	/*
	 * Find out how large of an SEEPROM we have.
	 */
	fxp_autosize_eeprom(sc);

	/*
	 * Get info about the primary PHY
	 */
	fxp_read_eeprom(sc, (u_int16_t *)&data, 6, 1);
	sc->phy_primary_addr = data & 0xff;
	sc->phy_primary_device = (data >> 8) & 0x3f;
	sc->phy_10Mbps_only = data >> 15;

	/*
	 * Read MAC address.
	 */
	fxp_read_eeprom(sc, (u_int16_t *)enaddr, 0, 3);

	/*
	 * Initialize the media structures.
	 */

	media = fxp_media_default;
	nmedia = sizeof(fxp_media_default) / sizeof(fxp_media_default[0]);
	defmedia = FXP_MEDIA_DEFAULT_DEFMEDIA;

	for (i = 0; i < NFXPMEDIA; i++) {
		if (sc->phy_primary_device == fxp_media[i].fsm_phy) {
			media = fxp_media[i].fsm_media;
			nmedia = fxp_media[i].fsm_nmedia;
			defmedia = fxp_media[i].fsm_defmedia;
		}
	}

	ifmedia_init(&sc->sc_media, 0, fxp_mediachange, fxp_mediastatus);
	for (i = 0; i < nmedia; i++) {
		if (IFM_SUBTYPE(media[i]) == IFM_100_TX && sc->phy_10Mbps_only)
			continue;
		ifmedia_add(&sc->sc_media, media[i], 0, NULL);
	}
	ifmedia_set(&sc->sc_media, defmedia);

	return (0);

 fail:
	printf(FXP_FORMAT ": Failed to malloc memory\n", FXP_ARGS(sc));
	if (sc->cbl_base)
		free(sc->cbl_base, M_DEVBUF);
	if (sc->fxp_stats)
		free(sc->fxp_stats, M_DEVBUF);
	if (sc->mcsp)
		free(sc->mcsp, M_DEVBUF);
	/* frees entire chain */
	if (sc->rfa_headm)
		m_freem(sc->rfa_headm);

	return (ENOMEM);
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
 *
 * Other ways to do this would be to try to read a register with known
 * contents with a varying number of address bits, but no such
 * register seem to be available. The high bits of register 10 are 01
 * on the 558 and 559, but apparently not on the 557.
 * 
 * The Linux driver computes a checksum on the EEPROM data, but the
 * value of this checksum is not very well documented.
 */
static void
fxp_autosize_eeprom(sc)
	struct fxp_softc *sc;
{
	u_int16_t reg;
	int x;

	CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, FXP_EEPROM_EECS);
	/*
	 * Shift in read opcode.
	 */
	for (x = 3; x > 0; x--) {
		if (FXP_EEPROM_OPC_READ & (1 << (x - 1))) {
			reg = FXP_EEPROM_EECS | FXP_EEPROM_EEDI;
		} else {
			reg = FXP_EEPROM_EECS;
		}
		CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, reg);
		CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL,
		    reg | FXP_EEPROM_EESK);
		DELAY(1);
		CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, reg);
		DELAY(1);
	}
	/*
	 * Shift in address.
	 * Wait for the dummy zero following a correct address shift.
	 */
	for (x = 1; x <= 8; x++) {
		CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, FXP_EEPROM_EECS);
		CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL,
			FXP_EEPROM_EECS | FXP_EEPROM_EESK);
		DELAY(1);
		if ((CSR_READ_2(sc, FXP_CSR_EEPROMCONTROL) & FXP_EEPROM_EEDO) == 0)
			break;
		CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, FXP_EEPROM_EECS);
		DELAY(1);
	}
	CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, 0);
	DELAY(1);
	sc->eeprom_size = x;
}
/*
 * Read from the serial EEPROM. Basically, you manually shift in
 * the read opcode (one bit at a time) and then shift in the address,
 * and then you shift out the data (all of this one bit at a time).
 * The word size is 16 bits, so you have to provide the address for
 * every 16 bits of data.
 */
static void
fxp_read_eeprom(sc, data, offset, words)
	struct fxp_softc *sc;
	u_short *data;
	int offset;
	int words;
{
	u_int16_t reg;
	int i, x;

	for (i = 0; i < words; i++) {
		CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, FXP_EEPROM_EECS);
		/*
		 * Shift in read opcode.
		 */
		for (x = 3; x > 0; x--) {
			if (FXP_EEPROM_OPC_READ & (1 << (x - 1))) {
				reg = FXP_EEPROM_EECS | FXP_EEPROM_EEDI;
			} else {
				reg = FXP_EEPROM_EECS;
			}
			CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, reg);
			CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL,
			    reg | FXP_EEPROM_EESK);
			DELAY(1);
			CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, reg);
			DELAY(1);
		}
		/*
		 * Shift in address.
		 */
		for (x = sc->eeprom_size; x > 0; x--) {
			if ((i + offset) & (1 << (x - 1))) {
				reg = FXP_EEPROM_EECS | FXP_EEPROM_EEDI;
			} else {
				reg = FXP_EEPROM_EECS;
			}
			CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, reg);
			CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL,
			    reg | FXP_EEPROM_EESK);
			DELAY(1);
			CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, reg);
			DELAY(1);
		}
		reg = FXP_EEPROM_EECS;
		data[i] = 0;
		/*
		 * Shift out data.
		 */
		for (x = 16; x > 0; x--) {
			CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL,
			    reg | FXP_EEPROM_EESK);
			DELAY(1);
			if (CSR_READ_2(sc, FXP_CSR_EEPROMCONTROL) &
			    FXP_EEPROM_EEDO)
				data[i] |= (1 << (x - 1));
			CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, reg);
			DELAY(1);
		}
		CSR_WRITE_2(sc, FXP_CSR_EEPROMCONTROL, 0);
		DELAY(1);
	}
}

/*
 * Start packet transmission on the interface.
 */
static void
fxp_start(ifp)
	struct ifnet *ifp;
{
	struct fxp_softc *sc = ifp->if_softc;
	struct fxp_cb_tx *txp;
	FXP_SPLVAR(s)

#if !defined(__NetBSD__)
	FXP_LOCK(sc, s);
#endif
	/*
	 * See if we need to suspend xmit until the multicast filter
	 * has been reprogrammed (which can only be done at the head
	 * of the command chain).
	 */
	if (sc->need_mcsetup) {
		FXP_UNLOCK(sc, s);
		return;
	}

	txp = NULL;

	/*
	 * We're finished if there is nothing more to add to the list or if
	 * we're all filled up with buffers to transmit.
	 * NOTE: One TxCB is reserved to guarantee that fxp_mc_setup() can add
	 *       a NOP command when needed.
	 */
	while (ifp->if_snd.ifq_head != NULL && sc->tx_queued < FXP_NTXCB - 1) {
		struct mbuf *m, *mb_head;
		int segment;

		/*
		 * Grab a packet to transmit.
		 */
		IF_DEQUEUE(&ifp->if_snd, mb_head);

		/*
		 * Get pointer to next available tx desc.
		 */
		txp = sc->cbl_last->next;

		/*
		 * Go through each of the mbufs in the chain and initialize
		 * the transmit buffer descriptors with the physical address
		 * and size of the mbuf.
		 */
tbdinit:
		for (m = mb_head, segment = 0; m != NULL; m = m->m_next) {
			if (m->m_len != 0) {
				if (segment == FXP_NTXSEG)
					break;
				txp->tbd[segment].tb_addr =
				    vtophys(mtod(m, vm_offset_t));
				txp->tbd[segment].tb_size = m->m_len;
				segment++;
			}
		}
		if (m != NULL) {
			struct mbuf *mn;

			/*
			 * We ran out of segments. We have to recopy this mbuf
			 * chain first. Bail out if we can't get the new buffers.
			 */
			MGETHDR(mn, M_DONTWAIT, MT_DATA);
			if (mn == NULL) {
				m_freem(mb_head);
				break;
			}
			if (mb_head->m_pkthdr.len > MHLEN) {
				MCLGET(mn, M_DONTWAIT);
				if ((mn->m_flags & M_EXT) == 0) {
					m_freem(mn);
					m_freem(mb_head);
					break;
				}
			}
			m_copydata(mb_head, 0, mb_head->m_pkthdr.len,
			    mtod(mn, caddr_t));
			mn->m_pkthdr.len = mn->m_len = mb_head->m_pkthdr.len;
			m_freem(mb_head);
			mb_head = mn;
			goto tbdinit;
		}

		txp->tbd_number = segment;
		txp->mb_head = mb_head;
		txp->cb_status = 0;
		if (sc->tx_queued != FXP_CXINT_THRESH - 1) {
			txp->cb_command =
			    FXP_CB_COMMAND_XMIT | FXP_CB_COMMAND_SF | FXP_CB_COMMAND_S;
		} else {
			txp->cb_command =
			    FXP_CB_COMMAND_XMIT | FXP_CB_COMMAND_SF | FXP_CB_COMMAND_S | FXP_CB_COMMAND_I;
			/*
			 * Set a 5 second timer just in case we don't hear from the
			 * card again.
			 */
			ifp->if_timer = 5;
		}
		txp->tx_threshold = tx_threshold;
	
		/*
		 * Advance the end of list forward.
		 */

#ifdef __alpha__
		/*
		 * On platforms which can't access memory in 16-bit
		 * granularities, we must prevent the card from DMA'ing
		 * up the status while we update the command field.
		 * This could cause us to overwrite the completion status.
		 */
		atomic_clear_short(&sc->cbl_last->cb_command,
		    FXP_CB_COMMAND_S);
#else
		sc->cbl_last->cb_command &= ~FXP_CB_COMMAND_S;
#endif /*__alpha__*/
		sc->cbl_last = txp;

		/*
		 * Advance the beginning of the list forward if there are
		 * no other packets queued (when nothing is queued, cbl_first
		 * sits on the last TxCB that was sent out).
		 */
		if (sc->tx_queued == 0)
			sc->cbl_first = txp;

		sc->tx_queued++;

		/*
		 * Pass packet to bpf if there is a listener.
		 */
		if (ifp->if_bpf)
			bpf_mtap(FXP_BPFTAP_ARG(ifp), mb_head);
	}

	/*
	 * We're finished. If we added to the list, issue a RESUME to get DMA
	 * going again if suspended.
	 */
	if (txp != NULL) {
		fxp_scb_wait(sc);
		CSR_WRITE_1(sc, FXP_CSR_SCB_COMMAND, FXP_SCB_COMMAND_CU_RESUME);
	}
#if !defined(__NetBSD__)
	FXP_UNLOCK(sc, s);
#endif
}

/*
 * Process interface interrupts.
 */
static FXP_INTR_TYPE
fxp_intr(arg)
	void *arg;
{
	struct fxp_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_if;
	u_int8_t statack;
	FXP_SPLVAR(s)
#if defined(__NetBSD__)
	int claimed = 0;
#endif

	FXP_LOCK(sc, s);

	if (sc->suspended) {
		FXP_UNLOCK(sc, s);
		return;
	}

	while ((statack = CSR_READ_1(sc, FXP_CSR_SCB_STATACK)) != 0) {
#if defined(__NetBSD__)
		claimed = 1;
#endif
		/*
		 * First ACK all the interrupts in this pass.
		 */
		CSR_WRITE_1(sc, FXP_CSR_SCB_STATACK, statack);

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
			struct fxp_cb_tx *txp;

			for (txp = sc->cbl_first; sc->tx_queued &&
			    (txp->cb_status & FXP_CB_STATUS_C) != 0;
			    txp = txp->next) {
				if (txp->mb_head != NULL) {
					m_freem(txp->mb_head);
					txp->mb_head = NULL;
				}
				sc->tx_queued--;
			}
			sc->cbl_first = txp;
			ifp->if_timer = 0;
			if (sc->tx_queued == 0) {
				if (sc->need_mcsetup)
					fxp_mc_setup(sc);
			}
			/*
			 * Try to start more packets transmitting.
			 */
			if (ifp->if_snd.ifq_head != NULL)
				fxp_start(ifp);
		}
		/*
		 * Process receiver interrupts. If a no-resource (RNR)
		 * condition exists, get whatever packets we can and
		 * re-start the receiver.
		 */
		if (statack & (FXP_SCB_STATACK_FR | FXP_SCB_STATACK_RNR)) {
			struct mbuf *m;
			struct fxp_rfa *rfa;
rcvloop:
			m = sc->rfa_headm;
			rfa = (struct fxp_rfa *)(m->m_ext.ext_buf +
			    RFA_ALIGNMENT_FUDGE);

			if (rfa->rfa_status & FXP_RFA_STATUS_C) {
				/*
				 * Remove first packet from the chain.
				 */
				sc->rfa_headm = m->m_next;
				m->m_next = NULL;

				/*
				 * Add a new buffer to the receive chain.
				 * If this fails, the old buffer is recycled
				 * instead.
				 */
				if (fxp_add_rfabuf(sc, m) == 0) {
					struct ether_header *eh;
					int total_len;

					total_len = rfa->actual_size &
					    (MCLBYTES - 1);
					if (total_len <
					    sizeof(struct ether_header)) {
						m_freem(m);
						goto rcvloop;
					}
					m->m_pkthdr.rcvif = ifp;
					m->m_pkthdr.len = m->m_len = total_len;
					eh = mtod(m, struct ether_header *);
					m->m_data +=
					    sizeof(struct ether_header);
					m->m_len -=
					    sizeof(struct ether_header);
					m->m_pkthdr.len = m->m_len;
					ether_input(ifp, eh, m);
				}
				goto rcvloop;
			}
			if (statack & FXP_SCB_STATACK_RNR) {
				fxp_scb_wait(sc);
				CSR_WRITE_4(sc, FXP_CSR_SCB_GENERAL,
				    vtophys(sc->rfa_headm->m_ext.ext_buf) +
					RFA_ALIGNMENT_FUDGE);
				CSR_WRITE_1(sc, FXP_CSR_SCB_COMMAND,
				    FXP_SCB_COMMAND_RU_START);
			}
		}
	}
#if defined(__NetBSD__)
	return (claimed);
#else
	FXP_UNLOCK(sc, s);
#endif
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
fxp_stats_update(arg)
	void *arg;
{
	struct fxp_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_if;
	struct fxp_stats *sp = sc->fxp_stats;
	struct fxp_cb_tx *txp;
	FXP_SPLVAR(s)

	ifp->if_opackets += sp->tx_good;
	ifp->if_collisions += sp->tx_total_collisions;
	if (sp->rx_good) {
		ifp->if_ipackets += sp->rx_good;
		sc->rx_idle_secs = 0;
	} else {
		/*
		 * Receiver's been idle for another second.
		 */
		sc->rx_idle_secs++;
	}
	ifp->if_ierrors +=
	    sp->rx_crc_errors +
	    sp->rx_alignment_errors +
	    sp->rx_rnr_errors +
	    sp->rx_overrun_errors;
	/*
	 * If any transmit underruns occured, bump up the transmit
	 * threshold by another 512 bytes (64 * 8).
	 */
	if (sp->tx_underruns) {
		ifp->if_oerrors += sp->tx_underruns;
		if (tx_threshold < 192)
			tx_threshold += 64;
	}
	FXP_LOCK(sc, s);
	/*
	 * Release any xmit buffers that have completed DMA. This isn't
	 * strictly necessary to do here, but it's advantagous for mbufs
	 * with external storage to be released in a timely manner rather
	 * than being defered for a potentially long time. This limits
	 * the delay to a maximum of one second.
	 */ 
	for (txp = sc->cbl_first; sc->tx_queued &&
	    (txp->cb_status & FXP_CB_STATUS_C) != 0;
	    txp = txp->next) {
		if (txp->mb_head != NULL) {
			m_freem(txp->mb_head);
			txp->mb_head = NULL;
		}
		sc->tx_queued--;
	}
	sc->cbl_first = txp;
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
		CSR_WRITE_1(sc, FXP_CSR_SCB_COMMAND,
		    FXP_SCB_COMMAND_CU_DUMPRESET);
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
	FXP_UNLOCK(sc, s);
	/*
	 * Schedule another timeout one second from now.
	 */
	sc->stat_ch = timeout(fxp_stats_update, sc, hz);
}

/*
 * Stop the interface. Cancels the statistics updater and resets
 * the interface.
 */
static void
fxp_stop(sc)
	struct fxp_softc *sc;
{
	struct ifnet *ifp = &sc->sc_if;
	struct fxp_cb_tx *txp;
	int i;
	FXP_SPLVAR(s)

#if !defined(__NetBSD__)
	FXP_LOCK(sc, s);
#endif

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;

	/*
	 * Cancel stats updater.
	 */
	untimeout(fxp_stats_update, sc, sc->stat_ch);

	/*
	 * Issue software reset
	 */
	CSR_WRITE_4(sc, FXP_CSR_PORT, FXP_PORT_SELECTIVE_RESET);
	DELAY(10);

	/*
	 * Release any xmit buffers.
	 */
	txp = sc->cbl_base;
	if (txp != NULL) {
		for (i = 0; i < FXP_NTXCB; i++) {
			if (txp[i].mb_head != NULL) {
				m_freem(txp[i].mb_head);
				txp[i].mb_head = NULL;
			}
		}
	}
	sc->tx_queued = 0;

	/*
	 * Free all the receive buffers then reallocate/reinitialize
	 */
	if (sc->rfa_headm != NULL)
		m_freem(sc->rfa_headm);
	sc->rfa_headm = NULL;
	sc->rfa_tailm = NULL;
	for (i = 0; i < FXP_NRFABUFS; i++) {
		if (fxp_add_rfabuf(sc, NULL) != 0) {
			/*
			 * This "can't happen" - we're at splimp()
			 * and we just freed all the buffers we need
			 * above.
			 */
			panic("fxp_stop: no buffers!");
		}
	}

#if !defined(__NetBSD__)
	FXP_UNLOCK(sc, s);
#endif
}

/*
 * Watchdog/transmission transmit timeout handler. Called when a
 * transmission is started on the interface, but no interrupt is
 * received before the timeout. This usually indicates that the
 * card has wedged for some reason.
 */
static void
fxp_watchdog(ifp)
	struct ifnet *ifp;
{
	struct fxp_softc *sc = ifp->if_softc;

	printf(FXP_FORMAT ": device timeout\n", FXP_ARGS(sc));
	ifp->if_oerrors++;

	fxp_init(sc);
}

static void
fxp_init(xsc)
	void *xsc;
{
	struct fxp_softc *sc = xsc;
	struct ifnet *ifp = &sc->sc_if;
	struct fxp_cb_config *cbp;
	struct fxp_cb_ias *cb_ias;
	struct fxp_cb_tx *txp;
	int i, prm;
	FXP_SPLVAR(s)

	FXP_LOCK(sc, s);
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
	CSR_WRITE_1(sc, FXP_CSR_SCB_COMMAND, FXP_SCB_COMMAND_CU_BASE);

	fxp_scb_wait(sc);
	CSR_WRITE_1(sc, FXP_CSR_SCB_COMMAND, FXP_SCB_COMMAND_RU_BASE);

	/*
	 * Initialize base of dump-stats buffer.
	 */
	fxp_scb_wait(sc);
	CSR_WRITE_4(sc, FXP_CSR_SCB_GENERAL, vtophys(sc->fxp_stats));
	CSR_WRITE_1(sc, FXP_CSR_SCB_COMMAND, FXP_SCB_COMMAND_CU_DUMP_ADR);

	/*
	 * We temporarily use memory that contains the TxCB list to
	 * construct the config CB. The TxCB list memory is rebuilt
	 * later.
	 */
	cbp = (struct fxp_cb_config *) sc->cbl_base;

	/*
	 * This bcopy is kind of disgusting, but there are a bunch of must be
	 * zero and must be one bits in this structure and this is the easiest
	 * way to initialize them all to proper values.
	 */
	bcopy(fxp_cb_config_template,
		(void *)(uintptr_t)(volatile void *)&cbp->cb_status,
		sizeof(fxp_cb_config_template));

	cbp->cb_status =	0;
	cbp->cb_command =	FXP_CB_COMMAND_CONFIG | FXP_CB_COMMAND_EL;
	cbp->link_addr =	-1;	/* (no) next command */
	cbp->byte_count =	22;	/* (22) bytes to config */
	cbp->rx_fifo_limit =	8;	/* rx fifo threshold (32 bytes) */
	cbp->tx_fifo_limit =	0;	/* tx fifo threshold (0 bytes) */
	cbp->adaptive_ifs =	0;	/* (no) adaptive interframe spacing */
	cbp->rx_dma_bytecount =	0;	/* (no) rx DMA max */
	cbp->tx_dma_bytecount =	0;	/* (no) tx DMA max */
	cbp->dma_bce =		0;	/* (disable) dma max counters */
	cbp->late_scb =		0;	/* (don't) defer SCB update */
	cbp->tno_int =		0;	/* (disable) tx not okay interrupt */
	cbp->ci_int =		1;	/* interrupt on CU idle */
	cbp->save_bf =		prm;	/* save bad frames */
	cbp->disc_short_rx =	!prm;	/* discard short packets */
	cbp->underrun_retry =	1;	/* retry mode (1) on DMA underrun */
	cbp->mediatype =	!sc->phy_10Mbps_only; /* interface mode */
	cbp->nsai =		1;	/* (don't) disable source addr insert */
	cbp->preamble_length =	2;	/* (7 byte) preamble */
	cbp->loopback =		0;	/* (don't) loopback */
	cbp->linear_priority =	0;	/* (normal CSMA/CD operation) */
	cbp->linear_pri_mode =	0;	/* (wait after xmit only) */
	cbp->interfrm_spacing =	6;	/* (96 bits of) interframe spacing */
	cbp->promiscuous =	prm;	/* promiscuous mode */
	cbp->bcast_disable =	0;	/* (don't) disable broadcasts */
	cbp->crscdt =		0;	/* (CRS only) */
	cbp->stripping =	!prm;	/* truncate rx packet to byte count */
	cbp->padding =		1;	/* (do) pad short tx packets */
	cbp->rcv_crc_xfer =	0;	/* (don't) xfer CRC to host */
	cbp->force_fdx =	0;	/* (don't) force full duplex */
	cbp->fdx_pin_en =	1;	/* (enable) FDX# pin */
	cbp->multi_ia =		0;	/* (don't) accept multiple IAs */
	cbp->mc_all =		sc->all_mcasts;/* accept all multicasts */

	/*
	 * Start the config command/DMA.
	 */
	fxp_scb_wait(sc);
	CSR_WRITE_4(sc, FXP_CSR_SCB_GENERAL, vtophys(&cbp->cb_status));
	CSR_WRITE_1(sc, FXP_CSR_SCB_COMMAND, FXP_SCB_COMMAND_CU_START);
	/* ...and wait for it to complete. */
	fxp_dma_wait(&cbp->cb_status, sc);

	/*
	 * Now initialize the station address. Temporarily use the TxCB
	 * memory area like we did above for the config CB.
	 */
	cb_ias = (struct fxp_cb_ias *) sc->cbl_base;
	cb_ias->cb_status = 0;
	cb_ias->cb_command = FXP_CB_COMMAND_IAS | FXP_CB_COMMAND_EL;
	cb_ias->link_addr = -1;
#if defined(__NetBSD__)
	bcopy(LLADDR(ifp->if_sadl), (void *)cb_ias->macaddr, 6);
#else
	bcopy(sc->arpcom.ac_enaddr,
	    (void *)(uintptr_t)(volatile void *)cb_ias->macaddr,
	    sizeof(sc->arpcom.ac_enaddr));
#endif /* __NetBSD__ */

	/*
	 * Start the IAS (Individual Address Setup) command/DMA.
	 */
	fxp_scb_wait(sc);
	CSR_WRITE_1(sc, FXP_CSR_SCB_COMMAND, FXP_SCB_COMMAND_CU_START);
	/* ...and wait for it to complete. */
	fxp_dma_wait(&cb_ias->cb_status, sc);

	/*
	 * Initialize transmit control block (TxCB) list.
	 */

	txp = sc->cbl_base;
	bzero(txp, sizeof(struct fxp_cb_tx) * FXP_NTXCB);
	for (i = 0; i < FXP_NTXCB; i++) {
		txp[i].cb_status = FXP_CB_STATUS_C | FXP_CB_STATUS_OK;
		txp[i].cb_command = FXP_CB_COMMAND_NOP;
		txp[i].link_addr = vtophys(&txp[(i + 1) & FXP_TXCB_MASK].cb_status);
		txp[i].tbd_array_addr = vtophys(&txp[i].tbd[0]);
		txp[i].next = &txp[(i + 1) & FXP_TXCB_MASK];
	}
	/*
	 * Set the suspend flag on the first TxCB and start the control
	 * unit. It will execute the NOP and then suspend.
	 */
	txp->cb_command = FXP_CB_COMMAND_NOP | FXP_CB_COMMAND_S;
	sc->cbl_first = sc->cbl_last = txp;
	sc->tx_queued = 1;

	fxp_scb_wait(sc);
	CSR_WRITE_1(sc, FXP_CSR_SCB_COMMAND, FXP_SCB_COMMAND_CU_START);

	/*
	 * Initialize receiver buffer area - RFA.
	 */
	fxp_scb_wait(sc);
	CSR_WRITE_4(sc, FXP_CSR_SCB_GENERAL,
	    vtophys(sc->rfa_headm->m_ext.ext_buf) + RFA_ALIGNMENT_FUDGE);
	CSR_WRITE_1(sc, FXP_CSR_SCB_COMMAND, FXP_SCB_COMMAND_RU_START);

	/*
	 * Set current media.
	 */
	fxp_set_media(sc, sc->sc_media.ifm_cur->ifm_media);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	FXP_UNLOCK(sc, s);

	/*
	 * Start stats updater.
	 */
	sc->stat_ch = timeout(fxp_stats_update, sc, hz);
}

static void
fxp_set_media(sc, media)
	struct fxp_softc *sc;
	int media;
{

	switch (sc->phy_primary_device) {
	case FXP_PHY_DP83840:
	case FXP_PHY_DP83840A:
		fxp_mdi_write(sc, sc->phy_primary_addr, FXP_DP83840_PCR,
		    fxp_mdi_read(sc, sc->phy_primary_addr, FXP_DP83840_PCR) |
		    FXP_DP83840_PCR_LED4_MODE |	/* LED4 always indicates duplex */
		    FXP_DP83840_PCR_F_CONNECT |	/* force link disconnect bypass */
		    FXP_DP83840_PCR_BIT10);	/* XXX I have no idea */
		/* fall through */
	case FXP_PHY_82553A:
	case FXP_PHY_82553C: /* untested */
	case FXP_PHY_82555:
	case FXP_PHY_82555B:
		if (IFM_SUBTYPE(media) != IFM_AUTO) {
			int flags;

			flags = (IFM_SUBTYPE(media) == IFM_100_TX) ?
			    FXP_PHY_BMCR_SPEED_100M : 0;
			flags |= (media & IFM_FDX) ?
			    FXP_PHY_BMCR_FULLDUPLEX : 0;
			fxp_mdi_write(sc, sc->phy_primary_addr,
			    FXP_PHY_BMCR,
			    (fxp_mdi_read(sc, sc->phy_primary_addr,
			    FXP_PHY_BMCR) &
			    ~(FXP_PHY_BMCR_AUTOEN | FXP_PHY_BMCR_SPEED_100M |
			     FXP_PHY_BMCR_FULLDUPLEX)) | flags);
		} else {
			fxp_mdi_write(sc, sc->phy_primary_addr,
			    FXP_PHY_BMCR,
			    (fxp_mdi_read(sc, sc->phy_primary_addr,
			    FXP_PHY_BMCR) | FXP_PHY_BMCR_AUTOEN));
		}
		break;
	/*
	 * The Seeq 80c24 doesn't have a PHY programming interface, so do
	 * nothing.
	 */
	case FXP_PHY_80C24:
		break;
	default:
		printf(FXP_FORMAT
		    ": warning: unsupported PHY, type = %d, addr = %d\n",
		     FXP_ARGS(sc), sc->phy_primary_device,
		     sc->phy_primary_addr);
	}
}

/*
 * Change media according to request.
 */
int
fxp_mediachange(ifp)
	struct ifnet *ifp;
{
	struct fxp_softc *sc = ifp->if_softc;
	struct ifmedia *ifm = &sc->sc_media;

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return (EINVAL);

	fxp_set_media(sc, ifm->ifm_media);
	return (0);
}

/*
 * Notify the world which media we're using.
 */
void
fxp_mediastatus(ifp, ifmr)
	struct ifnet *ifp;
	struct ifmediareq *ifmr;
{
	struct fxp_softc *sc = ifp->if_softc;
	int flags, stsflags;

	switch (sc->phy_primary_device) {
	case FXP_PHY_82555:
	case FXP_PHY_82555B:
	case FXP_PHY_DP83840:
	case FXP_PHY_DP83840A:
		ifmr->ifm_status = IFM_AVALID; /* IFM_ACTIVE will be valid */
		ifmr->ifm_active = IFM_ETHER;
		/*
		 * the following is not an error.
		 * You need to read this register twice to get current
		 * status. This is correct documented behaviour, the
		 * first read gets latched values.
		 */
		stsflags = fxp_mdi_read(sc, sc->phy_primary_addr, FXP_PHY_STS);
		stsflags = fxp_mdi_read(sc, sc->phy_primary_addr, FXP_PHY_STS);
		if (stsflags & FXP_PHY_STS_LINK_STS)
				ifmr->ifm_status |= IFM_ACTIVE;

		/* 
		 * If we are in auto mode, then try report the result.
		 */
		flags = fxp_mdi_read(sc, sc->phy_primary_addr, FXP_PHY_BMCR);
		if (flags & FXP_PHY_BMCR_AUTOEN) {
			ifmr->ifm_active |= IFM_AUTO; /* XXX presently 0 */
			if (stsflags & FXP_PHY_STS_AUTO_DONE) {
				/*
				 * Intel and National parts report
				 * differently on what they found.
				 */
				if ((sc->phy_primary_device == FXP_PHY_82555)
				|| (sc->phy_primary_device == FXP_PHY_82555B)) {
					flags = fxp_mdi_read(sc,
						sc->phy_primary_addr,
						FXP_PHY_USC);
	
					if (flags & FXP_PHY_USC_SPEED)
						ifmr->ifm_active |= IFM_100_TX;
					else
						ifmr->ifm_active |= IFM_10_T;
		
					if (flags & FXP_PHY_USC_DUPLEX)
						ifmr->ifm_active |= IFM_FDX;
				} else { /* it's National. only know speed  */
					flags = fxp_mdi_read(sc,
						sc->phy_primary_addr,
						FXP_DP83840_PAR);
	
					if (flags & FXP_DP83840_PAR_SPEED_10)
						ifmr->ifm_active |= IFM_10_T;
					else
						ifmr->ifm_active |= IFM_100_TX;
				}
			}
		} else { /* in manual mode.. just report what we were set to */
			if (flags & FXP_PHY_BMCR_SPEED_100M)
				ifmr->ifm_active |= IFM_100_TX;
			else
				ifmr->ifm_active |= IFM_10_T;

			if (flags & FXP_PHY_BMCR_FULLDUPLEX)
				ifmr->ifm_active |= IFM_FDX;
		}
		break;

	case FXP_PHY_80C24:
	default:
		ifmr->ifm_active = IFM_ETHER|IFM_MANUAL; /* XXX IFM_AUTO ? */
	}
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
fxp_add_rfabuf(sc, oldm)
	struct fxp_softc *sc;
	struct mbuf *oldm;
{
	u_int32_t v;
	struct mbuf *m;
	struct fxp_rfa *rfa, *p_rfa;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m != NULL) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0) {
			m_freem(m);
			if (oldm == NULL)
				return 1;
			m = oldm;
			m->m_data = m->m_ext.ext_buf;
		}
	} else {
		if (oldm == NULL)
			return 1;
		m = oldm;
		m->m_data = m->m_ext.ext_buf;
	}

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
	m->m_data += sizeof(struct fxp_rfa);
	rfa->size = (u_int16_t)(MCLBYTES - sizeof(struct fxp_rfa) - RFA_ALIGNMENT_FUDGE);

	/*
	 * Initialize the rest of the RFA.  Note that since the RFA
	 * is misaligned, we cannot store values directly.  Instead,
	 * we use an optimized, inline copy.
	 */

	rfa->rfa_status = 0;
	rfa->rfa_control = FXP_RFA_CONTROL_EL;
	rfa->actual_size = 0;

	v = -1;
	fxp_lwcopy(&v, (volatile u_int32_t *) rfa->link_addr);
	fxp_lwcopy(&v, (volatile u_int32_t *) rfa->rbd_addr);

	/*
	 * If there are other buffers already on the list, attach this
	 * one to the end by fixing up the tail to point to this one.
	 */
	if (sc->rfa_headm != NULL) {
		p_rfa = (struct fxp_rfa *) (sc->rfa_tailm->m_ext.ext_buf +
		    RFA_ALIGNMENT_FUDGE);
		sc->rfa_tailm->m_next = m;
		v = vtophys(rfa);
		fxp_lwcopy(&v, (volatile u_int32_t *) p_rfa->link_addr);
		p_rfa->rfa_control = 0;
	} else {
		sc->rfa_headm = m;
	}
	sc->rfa_tailm = m;

	return (m == oldm);
}

static volatile int
fxp_mdi_read(sc, phy, reg)
	struct fxp_softc *sc;
	int phy;
	int reg;
{
	int count = 10000;
	int value;

	CSR_WRITE_4(sc, FXP_CSR_MDICONTROL,
	    (FXP_MDI_READ << 26) | (reg << 16) | (phy << 21));

	while (((value = CSR_READ_4(sc, FXP_CSR_MDICONTROL)) & 0x10000000) == 0
	    && count--)
		DELAY(10);

	if (count <= 0)
		printf(FXP_FORMAT ": fxp_mdi_read: timed out\n",
		    FXP_ARGS(sc));

	return (value & 0xffff);
}

static void
fxp_mdi_write(sc, phy, reg, value)
	struct fxp_softc *sc;
	int phy;
	int reg;
	int value;
{
	int count = 10000;

	CSR_WRITE_4(sc, FXP_CSR_MDICONTROL,
	    (FXP_MDI_WRITE << 26) | (reg << 16) | (phy << 21) |
	    (value & 0xffff));

	while((CSR_READ_4(sc, FXP_CSR_MDICONTROL) & 0x10000000) == 0 &&
	    count--)
		DELAY(10);

	if (count <= 0)
		printf(FXP_FORMAT ": fxp_mdi_write: timed out\n",
		    FXP_ARGS(sc));
}

static int
fxp_ioctl(ifp, command, data)
	struct ifnet *ifp;
	FXP_IOCTLCMD_TYPE command;
	caddr_t data;
{
	struct fxp_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	FXP_SPLVAR(s)
	int error = 0;

	FXP_LOCK(sc, s);

	switch (command) {

	case SIOCSIFADDR:
#if !defined(__NetBSD__)
	case SIOCGIFADDR:
	case SIOCSIFMTU:
#endif
		error = ether_ioctl(ifp, command, data);
		break;

	case SIOCSIFFLAGS:
		sc->all_mcasts = (ifp->if_flags & IFF_ALLMULTI) ? 1 : 0;

		/*
		 * If interface is marked up and not running, then start it.
		 * If it is marked down and running, stop it.
		 * XXX If it's up then re-initialize it. This is so flags
		 * such as IFF_PROMISC are handled.
		 */
		if (ifp->if_flags & IFF_UP) {
			fxp_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				fxp_stop(sc);
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		sc->all_mcasts = (ifp->if_flags & IFF_ALLMULTI) ? 1 : 0;
#if defined(__NetBSD__)
		error = (command == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->sc_ethercom) :
		    ether_delmulti(ifr, &sc->sc_ethercom);

		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware
			 * filter accordingly.
			 */
			if (!sc->all_mcasts)
				fxp_mc_setup(sc);
			/*
			 * fxp_mc_setup() can turn on all_mcasts if we run
			 * out of space, so check it again rather than else {}.
			 */
			if (sc->all_mcasts)
				fxp_init(sc);
			error = 0;
		}
#else /* __FreeBSD__ */
		/*
		 * Multicast list has changed; set the hardware filter
		 * accordingly.
		 */
		if (!sc->all_mcasts)
			fxp_mc_setup(sc);
		/*
		 * fxp_mc_setup() can turn on sc->all_mcasts, so check it
		 * again rather than else {}.
		 */
		if (sc->all_mcasts)
			fxp_init(sc);
		error = 0;
#endif /* __NetBSD__ */
		break;

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, command);
		break;

	default:
		error = EINVAL;
	}
	FXP_UNLOCK(sc, s);
	return (error);
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
fxp_mc_setup(sc)
	struct fxp_softc *sc;
{
	struct fxp_cb_mcs *mcsp = sc->mcsp;
	struct ifnet *ifp = &sc->sc_if;
	struct ifmultiaddr *ifma;
	int nmcasts;
	int count;

	/*
	 * If there are queued commands, we must wait until they are all
	 * completed. If we are already waiting, then add a NOP command
	 * with interrupt option so that we're notified when all commands
	 * have been completed - fxp_start() ensures that no additional
	 * TX commands will be added when need_mcsetup is true.
	 */
	if (sc->tx_queued) {
		struct fxp_cb_tx *txp;

		/*
		 * need_mcsetup will be true if we are already waiting for the
		 * NOP command to be completed (see below). In this case, bail.
		 */
		if (sc->need_mcsetup)
			return;
		sc->need_mcsetup = 1;

		/*
		 * Add a NOP command with interrupt so that we are notified when all
		 * TX commands have been processed.
		 */
		txp = sc->cbl_last->next;
		txp->mb_head = NULL;
		txp->cb_status = 0;
		txp->cb_command = FXP_CB_COMMAND_NOP | FXP_CB_COMMAND_S | FXP_CB_COMMAND_I;
		/*
		 * Advance the end of list forward.
		 */
		sc->cbl_last->cb_command &= ~FXP_CB_COMMAND_S;
		sc->cbl_last = txp;
		sc->tx_queued++;
		/*
		 * Issue a resume in case the CU has just suspended.
		 */
		fxp_scb_wait(sc);
		CSR_WRITE_1(sc, FXP_CSR_SCB_COMMAND, FXP_SCB_COMMAND_CU_RESUME);
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
	mcsp->next = sc->cbl_base;
	mcsp->mb_head = NULL;
	mcsp->cb_status = 0;
	mcsp->cb_command = FXP_CB_COMMAND_MCAS | FXP_CB_COMMAND_S | FXP_CB_COMMAND_I;
	mcsp->link_addr = vtophys(&sc->cbl_base->cb_status);

	nmcasts = 0;
	if (!sc->all_mcasts) {
		for (ifma = ifp->if_multiaddrs.lh_first; ifma != NULL;
		    ifma = ifma->ifma_link.le_next) {
			if (ifma->ifma_addr->sa_family != AF_LINK)
				continue;
			if (nmcasts >= MAXMCADDR) {
				sc->all_mcasts = 1;
				nmcasts = 0;
				break;
			}
			bcopy(LLADDR((struct sockaddr_dl *)ifma->ifma_addr),
			    (void *)(uintptr_t)(volatile void *)
				&sc->mcsp->mc_addr[nmcasts][0], 6);
			nmcasts++;
		}
	}
	mcsp->mc_cnt = nmcasts * 6;
	sc->cbl_first = sc->cbl_last = (struct fxp_cb_tx *) mcsp;
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
		printf(FXP_FORMAT ": command queue timeout\n", FXP_ARGS(sc));
		return;
	}

	/*
	 * Start the multicast setup command.
	 */
	fxp_scb_wait(sc);
	CSR_WRITE_4(sc, FXP_CSR_SCB_GENERAL, vtophys(&mcsp->cb_status));
	CSR_WRITE_1(sc, FXP_CSR_SCB_COMMAND, FXP_SCB_COMMAND_CU_START);

	ifp->if_timer = 2;
	return;
}
