/*	$OpenBSD: if_tx.c,v 1.3 1998/10/10 04:30:09 jason Exp $	*/
/*	$Id: if_tx.c,v 1.23 1999/03/31 13:50:52 nsayer Exp $ */

/*-
 * Copyright (c) 1997 Semen Ustimenko (semen@iclub.nsu.ru)
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
 *
 *
 */

/*
 * EtherPower II 10/100  Fast Ethernet (tx0)
 * (aka SMC9432TX based on SMC83c170 EPIC chip)
 * 
 * Thanks are going to Steve Bauer and Jason Wright.
 *
 * todo:
 *	Deal with bus mastering, i.e. i realy don't know what to do with
 *	    it and how it can improve performance.
 *	Implement FULL IFF_MULTICAST support.
 *	Test, test and test again:-(
 *	
 */

/* We should define compile time options before if_txvar.h included */
/*#define	EPIC_NOIFMEDIA	1*/
/*#define	EPIC_USEIOSPACE	1*/
#define	EARLY_RX	1
/*#define	EPIC_DEBUG	1*/

#if defined(EPIC_DEBUG)
#define dprintf(a) printf a
#else
#define dprintf(a)
#endif

/* Macro to get either mbuf cluster or nothing */
#define EPIC_MGETCLUSTER(m) \
	{ MGETHDR((m),M_DONTWAIT,MT_DATA); \
	  if (m) { \
	    MCLGET((m),M_DONTWAIT); \
	    if( NULL == ((m)->m_flags & M_EXT) ){ \
	      m_freem(m); \
	      (m) = NULL; \
	    } \
	  } \
	}

#include "bpfilter.h"
#include "pci.h"
#include "opt_bdg.h"

#if NPCI > 0
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#if !defined(SIOCSIFMEDIA) || defined(EPIC_NOIFMEDIA)
#define EPIC_NOIFMEDIA	1
#else
#include <net/if_media.h>
#endif

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#endif

#ifdef IPX
#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#if defined(__OpenBSD__)
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>

#include <netinet/if_ether.h>

#include <vm/vm.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/if_txvar.h>
#else /* __FreeBSD__ */
#include <net/if_mib.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/clock.h>

#include <pci/pcivar.h>
#include <pci/if_txvar.h>

#ifdef BRIDGE
#include <net/bridge.h>
#endif

#endif

#if defined(__FreeBSD__)
#if __FreeBSD_version >= 300000
#define EPIC_IFIOCTL_CMD_TYPE u_long
#else
#define EPIC_IFIOCTL_CMD_TYPE int
#endif
#define EPIC_INTR_RET_TYPE void
#else /* __OpenBSD__ */
#define EPIC_IFIOCTL_CMD_TYPE u_long
#define EPIC_INTR_RET_TYPE int
#endif

static int epic_ifioctl __P((register struct ifnet *, EPIC_IFIOCTL_CMD_TYPE, caddr_t));
static EPIC_INTR_RET_TYPE epic_intr __P((void *));
static int epic_common_attach __P((epic_softc_t *));
static void epic_ifstart __P((struct ifnet * const));
static void epic_ifwatchdog __P((struct ifnet *));
static int epic_init __P((epic_softc_t *));
static void epic_stop __P((epic_softc_t *));
static __inline void epic_rx_done __P((epic_softc_t *));
static __inline void epic_tx_done __P((epic_softc_t *));
static int epic_init_rings __P((epic_softc_t *));
static void epic_free_rings __P((epic_softc_t *));
static void epic_stop_activity __P((epic_softc_t *));
static void epic_start_activity __P((epic_softc_t *));
static void epic_set_rx_mode __P((epic_softc_t *));
static void epic_set_tx_mode __P((epic_softc_t *));
static void epic_set_mc_table __P((epic_softc_t *));
static void epic_set_media_speed __P((epic_softc_t *));
static void epic_init_phy __P((epic_softc_t *));
static void epic_dump_state __P((epic_softc_t *));
static int epic_autoneg __P((epic_softc_t *));
static int epic_read_eeprom __P((epic_softc_t *,u_int16_t));
static void epic_output_eepromw __P((epic_softc_t *, u_int16_t));
static u_int16_t epic_input_eepromw __P((epic_softc_t *));
static u_int8_t epic_eeprom_clock __P((epic_softc_t *,u_int8_t));
static void epic_write_eepromreg __P((epic_softc_t *,u_int8_t));
static u_int8_t epic_read_eepromreg __P((epic_softc_t *));
static u_int16_t epic_read_phy_register __P((epic_softc_t *, u_int16_t));
static void epic_write_phy_register __P((epic_softc_t *, u_int16_t, u_int16_t));

#if !defined(EPIC_NOIFMEDIA)
static int epic_ifmedia_change __P((struct ifnet *));
static void epic_ifmedia_status __P((struct ifnet *, struct ifmediareq *));
#endif

int epic_mtypes [] = {
	IFM_ETHER | IFM_10_T,
	IFM_ETHER | IFM_10_T | IFM_FDX,
	IFM_ETHER | IFM_100_TX,
	IFM_ETHER | IFM_100_TX | IFM_FDX,
	IFM_ETHER | IFM_10_T | IFM_LOOP,
	IFM_ETHER | IFM_10_T | IFM_FDX | IFM_LOOP,
	IFM_ETHER | IFM_10_T | IFM_LOOP | IFM_FLAG1,
	IFM_ETHER | IFM_100_TX | IFM_LOOP,
	IFM_ETHER | IFM_100_TX | IFM_LOOP | IFM_FLAG1,
	IFM_ETHER | IFM_100_TX | IFM_FDX | IFM_LOOP,
	IFM_ETHER | IFM_AUTO
};
#define	EPIC_MTYPESNUM (sizeof(epic_mtypes) / sizeof(epic_mtypes[0]))


/* -------------------------------------------------------------------------
   OS-specific part
   ------------------------------------------------------------------------- */

#if defined(__OpenBSD__)
/* -----------------------------OpenBSD------------------------------------- */

static int epic_openbsd_probe __P((struct device *,void *,void *));
static void epic_openbsd_attach __P((struct device *, struct device *, void *));
static void epic_shutdown __P((void *));

struct cfattach tx_ca = {
	sizeof(epic_softc_t), epic_openbsd_probe, epic_openbsd_attach 
};
struct cfdriver tx_cd = {
	NULL,"tx",DV_IFNET
};

/* Synopsis: Check if device id corresponds with SMC83C170 id. */
static int 
epic_openbsd_probe(
    struct device *parent,
    void *match,
    void *aux )
{
	struct pci_attach_args *pa = aux;
	if( PCI_VENDOR(pa->pa_id) != SMC_VENDORID )
		return 0;

	if( PCI_PRODUCT(pa->pa_id) == CHIPID_83C170 )
		return 1;

	return 0;
}

static void
epic_openbsd_attach(
    struct device *parent,
    struct device *self,
    void *aux )
{
	epic_softc_t *sc = (epic_softc_t*)self;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	struct ifnet *ifp;
	bus_space_tag_t iot = pa->pa_iot;
	bus_addr_t iobase;
	bus_size_t iosize; 
	int i;
#if !defined(EPIC_NOIFMEDIA)
	int tmp;
#endif

	if( pci_io_find(pc, pa->pa_tag, PCI_CBIO, &iobase, &iosize)) {
		printf(": can't find i/o space\n");
		return;
	}
	if( bus_space_map(iot, iobase, iosize, 0, &sc->sc_sh)) {
		printf(": can't map i/o space\n");
		return;
	}
	sc->sc_st = iot;

	ifp = &sc->sc_if;
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname,IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = epic_ifioctl;
	ifp->if_start = epic_ifstart;
	ifp->if_watchdog = epic_ifwatchdog;

	/* Do common attach procedure */
	if( epic_common_attach(sc) ) return;

	/* Map interrupt */
	if( pci_intr_map(pc, pa->pa_intrtag, pa->pa_intrpin,
	    pa->pa_intrline, &ih)) {
		printf(": can't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, epic_intr, sc, 
	    self->dv_xname);

	if( NULL == sc->sc_ih ) {
		printf(": can't establish interrupt");
		if( intrstr )printf(" at %s",intrstr);
		printf("\n");
		return;
	} 
	printf(": %s",intrstr);

	/* Display some info */
	printf(" address %s",ether_sprintf(sc->sc_macaddr));
	/* Read current media config and display it too */
	i = PHY_READ_2( sc, DP83840_BMCR );
#if !defined(EPIC_NOIFMEDIA)
	tmp = IFM_ETHER;
#endif
	if( i & BMCR_AUTONEGOTIATION ){
		printf(", Auto-Neg ");

		/* To avoid bug in QS6612 read LPAR enstead of BMSR */
		i = PHY_READ_2( sc, DP83840_LPAR );
		if( i & (ANAR_100_TX|ANAR_100_TX_FD) ) printf("100Mbps");
		else printf("10Mbps");
		if( i & (ANAR_10_FD|ANAR_100_TX_FD) ) printf(" FD");
#if !defined(EPIC_NOIFMEDIA)
		tmp |= IFM_AUTO;
#endif
	} else {
#if defined(EPIC_NOIFMEDIA)
		ifp->if_flags |= IFF_LINK0;
#endif
		if( i & BMCR_100MBPS ) {
			printf(", 100Mbps");
#if !defined(EPIC_NOIFMEDIA)
			tmp |= IFM_100_TX;
#else
			ifp->if_flags |= IFF_LINK2;
#endif
		} else {
			printf(", 10Mbps");
#if !defined(EPIC_NOIFMEDIA)
			tmp |= IFM_10_T;
#endif
		}
		if( i & BMCR_FULL_DUPLEX ) {
			printf(" FD");
#if !defined(EPIC_NOIFMEDIA)
			tmp |= IFM_FDX;
#else
			ifp->if_flags |= IFF_LINK1;
#endif
		}
	}

	/* Init ifmedia interface */
#if !defined(EPIC_NOIFMEDIA)
	ifmedia_init(&sc->ifmedia,0,epic_ifmedia_change,epic_ifmedia_status);

	for (i=0; i<EPIC_MTYPESNUM; i++)
		ifmedia_add(&sc->ifmedia,epic_mtypes[i],0,NULL);

	ifmedia_set(&sc->ifmedia, tmp);
#endif

	/* Attach os interface and bpf */
	if_attach(ifp);
	ether_ifattach(ifp);
#if NBPFILTER > 0
	bpfattach(&sc->sc_if.if_bpf, ifp, DLT_EN10MB,
	    sizeof(struct ether_header));
#endif

	/* Set shutdown routine to stop DMA process */ 
	shutdownhook_establish(epic_shutdown, sc);
	printf("\n");
}

/* Simple call epic_stop() */
static void
epic_shutdown(
    void *sc)
{
	epic_stop(sc);
}

#else /* __FreeBSD__ */
/* -----------------------------FreeBSD------------------------------------- */

static const char* epic_freebsd_probe __P((pcici_t, pcidi_t));
static void epic_freebsd_attach __P((pcici_t, int));
static void epic_shutdown __P((int, void *));

/* Global variables */
static u_long epic_pci_count;
static struct pci_device txdevice = { 
	"tx",
	epic_freebsd_probe,
	epic_freebsd_attach,
	&epic_pci_count,
	NULL };

/* Append this driver to pci drivers list */
DATA_SET ( pcidevice_set, txdevice );

/* Synopsis: Check if device id corresponds with SMC83C170 id.  */
static const char*
epic_freebsd_probe(
    pcici_t config_id,
    pcidi_t device_id)
{
	if( PCI_VENDORID(device_id) != SMC_VENDORID )
		return NULL;

	if( PCI_CHIPID(device_id) == CHIPID_83C170 )
		return "SMC 83c170";

	return NULL;
}

/*
 * Do FreeBSD-specific attach routine, like map registers, alloc softc
 * structure and etc.
 */
static void
epic_freebsd_attach(
    pcici_t config_id,
    int unit)
{
	struct ifnet *ifp;
	epic_softc_t *sc;
#if defined(EPIC_USEIOSPACE)
	u_int32_t iobase;
#else
	caddr_t	pmembase;
#endif
	int i,s,tmp;

	printf("tx%d",unit);

	/* Allocate memory for softc, hardware descriptors and frag lists */
	sc = (epic_softc_t *) malloc( sizeof(epic_softc_t), M_DEVBUF, M_NOWAIT);
	if (sc == NULL)	return;

	/* Preinitialize softc structure */
    	bzero(sc, sizeof(epic_softc_t));		
	sc->unit = unit;

	/* Fill ifnet structure */
	ifp = &sc->sc_if;
	ifp->if_unit = unit;
	ifp->if_name = "tx";
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST|IFF_SIMPLEX|IFF_MULTICAST;
	ifp->if_ioctl = epic_ifioctl;
	ifp->if_start = epic_ifstart;
	ifp->if_watchdog = epic_ifwatchdog;
	ifp->if_init = (if_init_f_t*)epic_init;
	ifp->if_timer = 0;
	ifp->if_output = ether_output;
	ifp->if_snd.ifq_maxlen = TX_RING_SIZE;

	/* Get iobase or membase */
#if defined(EPIC_USEIOSPACE)
	if (!pci_map_port(config_id, PCI_CBIO,(u_short *) &(sc->iobase))) {
		printf(": cannot map port\n");
		free(sc, M_DEVBUF);
		return;
	}
#else
	if (!pci_map_mem(config_id, PCI_CBMA,(vm_offset_t *) &(sc->csr),(vm_offset_t *) &pmembase)) {
		printf(": cannot map memory\n"); 
		free(sc, M_DEVBUF);
		return;
	}
#endif

	if( epic_common_attach(sc) ) return;

	/* Display ethernet address ,... */
	printf(": address %02x:%02x:%02x:%02x:%02x:%02x,",
		sc->sc_macaddr[0],sc->sc_macaddr[1],sc->sc_macaddr[2],
		sc->sc_macaddr[3],sc->sc_macaddr[4],sc->sc_macaddr[5]);

	/* board type and ... */
	printf(" type ");
	for(i=0x2c;i<0x32;i++) {
		tmp = epic_read_eeprom( sc, i );
		if( ' ' == (u_int8_t)tmp ) break;
		printf("%c",(u_int8_t)tmp);
		tmp >>= 8;
		if( ' ' == (u_int8_t)tmp ) break;
		printf("%c",(u_int8_t)tmp);
	}

	/* Read current media config and display it too */
	i = PHY_READ_2( sc, DP83840_BMCR );
#if !defined(EPIC_NOIFMEDIA)
	tmp = IFM_ETHER;
#endif
	if( i & BMCR_AUTONEGOTIATION ){
		printf(", Auto-Neg ");

		/* To avoid bug in QS6612 read LPAR enstead of BMSR */
		i = PHY_READ_2( sc, DP83840_LPAR );
		if( i & (ANAR_100_TX|ANAR_100_TX_FD) ) printf("100Mbps ");
		else printf("10Mbps ");
		if( i & (ANAR_10_FD|ANAR_100_TX_FD) ) printf("FD");
#if !defined(EPIC_NOIFMEDIA)
		tmp |= IFM_AUTO;
#endif
	} else {
#if defined(EPIC_NOIFMEDIA)
		ifp->if_flags |= IFF_LINK0;
#endif
		if( i & BMCR_100MBPS ) {
			printf(", 100Mbps ");
#if !defined(EPIC_NOIFMEDIA)
			tmp |= IFM_100_TX;
#else
			ifp->if_flags |= IFF_LINK2;
#endif
		} else {
			printf(", 10Mbps ");
#if !defined(EPIC_NOIFMEDIA)
			tmp |= IFM_10_T;
#endif
		}
		if( i & BMCR_FULL_DUPLEX ) {
			printf("FD");
#if !defined(EPIC_NOIFMEDIA)
			tmp |= IFM_FDX;
#else
			ifp->if_flags |= IFF_LINK1;
#endif
		}
	}

	/* Init ifmedia interface */
#if !defined(EPIC_NOIFMEDIA)
	ifmedia_init(&sc->ifmedia,0,epic_ifmedia_change,epic_ifmedia_status);

	for (i=0; i<EPIC_MTYPESNUM; i++)
		ifmedia_add(&sc->ifmedia,epic_mtypes[i],0,NULL);

	ifmedia_set(&sc->ifmedia, tmp);
#endif

	s = splimp();

	/* Map interrupt */
	if( !pci_map_int(config_id, epic_intr, (void*)sc, &net_imask) ) {
		printf(": couldn't map interrupt\n");
		free(sc, M_DEVBUF);
		return;
	}

	/* Set shut down routine to stop DMA processes on reboot */
	at_shutdown(epic_shutdown, sc, SHUTDOWN_POST_SYNC);

	/*  Attach to if manager */
	if_attach(ifp);
	ether_ifattach(ifp);

#if NBPFILTER > 0
	bpfattach(ifp,DLT_EN10MB, sizeof(struct ether_header));
#endif

	splx(s);

	printf("\n");

	return;
}

static void
epic_shutdown(
    int howto,
    void *sc)
{
	epic_stop(sc);
}

#endif /* __OpenBSD__ */

/* ------------------------------------------------------------------------
   OS-independing part
   ------------------------------------------------------------------------ */

/*
 * This is if_ioctl handler. 
 */
static int
epic_ifioctl __P((
    register struct ifnet * ifp,
    EPIC_IFIOCTL_CMD_TYPE command,
    caddr_t data))
{
	epic_softc_t *sc = ifp->if_softc;
	int x, error = 0;

	x = splimp();

	switch (command) {
#if defined(__FreeBSD__)
	case SIOCSIFADDR:
	case SIOCGIFADDR:
	case SIOCSIFMTU:
		error = ether_ioctl(ifp, command, data);
		break;
#else /* __OpenBSD__ */
	case SIOCSIFADDR: {
		struct ifaddr *ifa = (struct ifaddr *)data;
		
		ifp->if_flags |= IFF_UP;
		switch(ifa->ifa_addr->sa_family) {
#if INET
		case AF_INET:
			epic_stop(sc);
			epic_init(sc);
			arp_ifinit(&sc->arpcom,ifa);
			break;
#endif /* __FreeBSD__ */
#if NS
		case AF_NS: {
			register struct ns_addr * ina = &IA_SNS(ifa)->sns_addr;

			if( ns_nullhost(*ina) ) 
				ina->x_host = 
				    *(union ns_host *) LLADDR(ifp->if_sadl);
			else
				bcopy(ina->x_host.c_host, LLADDR(ifp->if_sadl),
				    ifp->if_addrlen);

			epic_stop(sc);
			epic_init(sc);
			break;
		}
#endif
		default:
			epic_stop(sc);
			epic_init(sc);		
			break;
		}
	}
#endif

	case SIOCSIFFLAGS:
		/*
		 * If the interface is marked up and stopped, then start it.
		 * If it is marked down and running, then stop it.
		 */
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_flags & IFF_RUNNING) == 0) {
				epic_init(sc);
				break;
			}
		} else {
			if (ifp->if_flags & IFF_RUNNING) {
				epic_stop(sc);
				break;
			}
		}

		epic_stop_activity(sc);	

		/* Handle IFF_PROMISC flag */
		epic_set_rx_mode(sc);

#if defined(EPIC_NOIFMEDIA)
		/* Handle IFF_LINKx flags */
		epic_set_media_speed(sc);
#endif
		epic_start_activity(sc);	
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		/* Update out multicast list */
#if defined(__FreeBSD__) && __FreeBSD_version >= 300000
		epic_set_mc_table(sc);
		error = 0;
#else
		error = (command == SIOCADDMULTI) ?
		    ether_addmulti((struct ifreq *)data, &sc->arpcom) :
		    ether_delmulti((struct ifreq *)data, &sc->arpcom);

		if (error == ENETRESET) {
			epic_set_mc_table(sc);
			error = 0;
		}
#endif
		break;

#if !defined(EPIC_NOIFMEDIA)
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, (struct ifreq *)data, 
		    &sc->ifmedia, command);
		break;
#endif

	default:
		error = EINVAL;
	}
	splx(x);

	return error;
}

/*
 * OS-independed part of attach process. allocate memory for descriptors
 * and frag lists, wake up chip, read MAC address and PHY identyfier.
 * Return -1 on failure.
 */
static int
epic_common_attach(
    epic_softc_t *sc)
{
	int i;
	caddr_t pool;

	i = sizeof(struct epic_frag_list)*TX_RING_SIZE +
	    sizeof(struct epic_rx_desc)*RX_RING_SIZE + 
	    sizeof(struct epic_tx_desc)*TX_RING_SIZE + PAGE_SIZE,
	sc->pool = (epic_softc_t *) malloc( i, M_DEVBUF, M_NOWAIT);

	if (sc->pool == NULL) {
		printf(": can't allocate memory for buffers\n");
		return -1;
	}
	bzero(sc->pool, i);

	/* Align pool on PAGE_SIZE */
	pool = (caddr_t)sc->pool;
	pool = (caddr_t)((u_int32_t)(pool + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1));

	/* Distribute memory */
	sc->tx_flist = (void *)pool;
	pool += sizeof(struct epic_frag_list)*TX_RING_SIZE;
	sc->rx_desc = (void *)pool;
	pool += sizeof(struct epic_rx_desc)*RX_RING_SIZE;
	sc->tx_desc = (void *)pool;

	/* Bring the chip out of low-power mode. */
	CSR_WRITE_4( sc, GENCTL, 0x0000 );

	/* Workaround for Application Note 7-15 */
	for (i=0; i<16; i++) CSR_WRITE_4(sc, TEST1, TEST1_CLOCK_TEST);

	/* Read mac address from EEPROM */
	for (i = 0; i < ETHER_ADDR_LEN / sizeof(u_int16_t); i++)
		((u_int16_t *)sc->sc_macaddr)[i] = epic_read_eeprom(sc,i);

	/* Identify PHY */
	sc->phyid = PHY_READ_2(sc, DP83840_PHYIDR1 )<<6;
	sc->phyid|= (PHY_READ_2( sc, DP83840_PHYIDR2 )>>10)&0x3F;
	if( QS6612_OUI != sc->phyid ) 
		printf(": WARNING! PHY unknown (0x%x)",sc->phyid);

	sc->tx_threshold = TRANSMIT_THRESHOLD;
	sc->txcon = TXCON_DEFAULT;

	return 0;
}

/*
 * This is if_start handler. It takes mbufs from if_snd queue
 * and quque them for transmit, one by one, until TX ring become full
 * or quque become empty.
 */
static void
epic_ifstart(struct ifnet * const ifp){
	epic_softc_t *sc = ifp->if_softc;
	struct epic_tx_buffer *buf;
	struct epic_tx_desc *desc;
	struct epic_frag_list *flist;
	struct mbuf *m0;
	register struct mbuf *m;
	register int i;

#if 0
	/* If no link is established, simply free all mbufs in queue */
	PHY_READ_2( sc, DP83840_BMSR );
	if( !(BMSR_LINK_STATUS & PHY_READ_2( sc, DP83840_BMSR )) ){
		IF_DEQUEUE( &ifp->if_snd, m0 );
		while( m0 ) {
			m_freem(m0);
			IF_DEQUEUE( &ifp->if_snd, m0 );
		}
		return;
	}
#endif

	/* Link is OK, queue packets to NIC */
	while( sc->pending_txs < TX_RING_SIZE  ){
		buf = sc->tx_buffer + sc->cur_tx;
		desc = sc->tx_desc + sc->cur_tx;
		flist = sc->tx_flist + sc->cur_tx;

		/* Get next packet to send */
		IF_DEQUEUE( &ifp->if_snd, m0 );

		/* If nothing to send, return */
		if( NULL == m0 ) return;

		/* If descriptor is busy, set IFF_OACTIVE and exit */
		if( desc->status & 0x8000 ) {
			dprintf((EPIC_FORMAT ": desc is busy in ifstart, up and down interface please\n",EPIC_ARGS(sc)));
			break;
		}

		if( buf->mbuf ) {
			dprintf((EPIC_FORMAT ": mbuf not freed in ifstart, up and down interface please\n",EPIC_ARGS(sc)));
			break;
		}

		/* Fill fragments list */
		for( m=m0, i=0;
		    (NULL != m) && (i < EPIC_MAX_FRAGS);
		    m = m->m_next, i++ ) {
			flist->frag[i].fraglen = m->m_len; 
			flist->frag[i].fragaddr = vtophys( mtod(m, caddr_t) );
		}
		flist->numfrags = i;

		/* If packet was more than EPIC_MAX_FRAGS parts, */
		/* recopy packet to new allocated mbuf cluster */
		if( NULL != m ){
			EPIC_MGETCLUSTER(m);
			if( NULL == m ){
				printf(EPIC_FORMAT ": cannot allocate mbuf cluster\n",EPIC_ARGS(sc));
				m_freem(m0);
				ifp->if_oerrors++;
				continue;
			}

			m_copydata( m0, 0, m0->m_pkthdr.len, mtod(m,caddr_t) );
			flist->frag[0].fraglen = 
			     m->m_pkthdr.len = m->m_len = m0->m_pkthdr.len;
			m->m_pkthdr.rcvif = ifp;

			flist->numfrags = 1;
			flist->frag[0].fragaddr = vtophys( mtod(m, caddr_t) );
			m_freem(m0);
			m0 = m;
		}

		buf->mbuf = m0;
		sc->pending_txs++;
		sc->cur_tx = ( sc->cur_tx + 1 ) & TX_RING_MASK;
		desc->control = 0x01;
		desc->txlength = 
		    max(m0->m_pkthdr.len,ETHER_MIN_LEN-ETHER_CRC_LEN);
		desc->status = 0x8000;
		CSR_WRITE_4( sc, COMMAND, COMMAND_TXQUEUED );

		/* Set watchdog timer */
		ifp->if_timer = 8;

#if NBPFILTER > 0
		if( ifp->if_bpf ) 
#if defined(__FreeBSD__)
			bpf_mtap( ifp, m0 );
#else /* __OpenBSD__ */
			bpf_mtap( ifp->if_bpf, m0 );
#endif /* __FreeBSD__ */
#endif
	}

	ifp->if_flags |= IFF_OACTIVE;

	return;
	
}

/*
 *
 * splimp() invoked before epic_intr_normal()
 */
static __inline void
epic_rx_done __P((
	epic_softc_t *sc ))
{
	u_int16_t len;
	struct epic_rx_buffer *buf;
	struct epic_rx_desc *desc;
	struct mbuf *m;
	struct ether_header *eh;

	while( !(sc->rx_desc[sc->cur_rx].status & 0x8000) ) { 
		buf = sc->rx_buffer + sc->cur_rx;
		desc = sc->rx_desc + sc->cur_rx;

		/* Switch to next descriptor */
		sc->cur_rx = (sc->cur_rx+1) & RX_RING_MASK;

		/* Check for errors, this should happend */
		/* only if SAVE_ERRORED_PACKETS is set, */
		/* normaly rx errors generate RXE interrupt */
		if( !(desc->status & 1) ) {
			dprintf((EPIC_FORMAT ": Rx error status: 0x%x\n",EPIC_ARGS(sc),desc->status));
			sc->sc_if.if_ierrors++;
			desc->status = 0x8000;
			continue;
		}

		/* Save packet length and mbuf contained packet */ 
		len = desc->rxlength - ETHER_CRC_LEN;
		m = buf->mbuf;

		/* Try to get mbuf cluster */
		EPIC_MGETCLUSTER( buf->mbuf );
		if( NULL == buf->mbuf ) { 
			printf(EPIC_FORMAT ": cannot allocate mbuf cluster\n",EPIC_ARGS(sc));
			buf->mbuf = m;
			desc->status = 0x8000;
			sc->sc_if.if_ierrors++;
			continue;
		}

		/* Point to new mbuf, and give descriptor to chip */
		desc->bufaddr = vtophys( mtod( buf->mbuf, caddr_t ) );
		desc->status = 0x8000;
		
		/* First mbuf in packet holds the ethernet and packet headers */
		eh = mtod( m, struct ether_header * );
		m->m_pkthdr.rcvif = &(sc->sc_if);
		m->m_pkthdr.len = m->m_len = len;

#if NBPFILTER > 0
		/* Give mbuf to BPFILTER */
		if( sc->sc_if.if_bpf ) 
#if defined(__FreeBSD__)
			bpf_mtap( &sc->sc_if, m );
#else /* __OpenBSD__ */
			bpf_mtap( sc->sc_if.if_bpf, m );
#endif /* __FreeBSD__ */
#endif /* NBPFILTER */

#ifdef BRIDGE
		if (do_bridge) {
			struct ifnet *bdg_ifp ;
			bdg_ifp = bridge_in(m);
			if (bdg_ifp == BDG_DROP) {
				if (m)
					m_free(m);
				continue; /* and drop */
			}
			if (bdg_ifp != BDG_LOCAL)
				bdg_forward(&m, bdg_ifp);
			if (bdg_ifp != BDG_LOCAL && bdg_ifp != BDG_BCAST &&
				bdg_ifp != BDG_MCAST) {
				if (m)
					m_free(m);
				continue; /* and drop */
			}
			/* all others accepted locally */
		}
#endif

#if NBPFILTER > 0
#ifdef BRIDGE
		/*
		 * This deserves explanation
		 * If the bridge is _on_, then the following check
		 * must not be done because occasionally the bridge
		 * gets packets that are local but have the ethernet
		 * address of one of the other interfaces.
		 *
		 * But if the bridge is off, then we have to drop
		 * stuff that came in just via bpfilter.
		 */
		if (!do_bridge)
#endif
		/* Accept only our packets, broadcasts and multicasts */
		if( (eh->ether_dhost[0] & 1) == 0 &&
		    bcmp(eh->ether_dhost,sc->sc_macaddr,ETHER_ADDR_LEN)){
			m_freem(m);
			continue;
		}
#endif

		/* Second mbuf holds packet ifself */
		m->m_pkthdr.len = m->m_len = len - sizeof(struct ether_header);
		m->m_data += sizeof( struct ether_header );

		/* Give mbuf to OS */
		ether_input(&sc->sc_if, eh, m);

		/* Successfuly received frame */
		sc->sc_if.if_ipackets++;
        }

	return;
}

/*
 * Synopsis: Do last phase of transmission. I.e. if desc is 
 * transmitted, decrease pending_txs counter, free mbuf contained
 * packet, switch to next descriptor and repeat until no packets
 * are pending or descriptor is not transmitted yet.
 */
static __inline void
epic_tx_done __P(( 
    register epic_softc_t *sc ))
{
	struct epic_tx_buffer *buf;
	struct epic_tx_desc *desc;
	u_int16_t status;

	while( sc->pending_txs > 0 ){
		buf = sc->tx_buffer + sc->dirty_tx;
		desc = sc->tx_desc + sc->dirty_tx;
		status = desc->status;

		/* If packet is not transmitted, thou followed */
		/* packets are not transmitted too */
		if( status & 0x8000 ) break;

		/* Packet is transmitted. Switch to next and */
		/* free mbuf */
		sc->pending_txs--;
		sc->dirty_tx = (sc->dirty_tx + 1) & TX_RING_MASK;
		m_freem( buf->mbuf );
		buf->mbuf = NULL;

		/* Check for errors and collisions */
		if( status & 0x0001 ) sc->sc_if.if_opackets++;
		else sc->sc_if.if_oerrors++;
		sc->sc_if.if_collisions += (status >> 8) & 0x1F;
#if defined(EPIC_DEBUG)
		if( (status & 0x1001) == 0x1001 ) 
			dprintf((EPIC_FORMAT ": frame not transmitted due collisions\n",EPIC_ARGS(sc)));
#endif
	}

	if( sc->pending_txs < TX_RING_SIZE ) 
		sc->sc_if.if_flags &= ~IFF_OACTIVE;
}

/*
 * Interrupt function
 *
 * splimp() assumed to be done 
 */
static EPIC_INTR_RET_TYPE
epic_intr (
    void *arg)
{
    epic_softc_t * sc = (epic_softc_t *) arg;
    int status,i=4;
#if defined(__OpenBSD__)
    int claimed = 0;
#endif


    while( i-- && ((status = CSR_READ_4(sc, INTSTAT)) & INTSTAT_INT_ACTV) ){
#if defined(__OpenBSD__)
	claimed = 1;
#endif
	CSR_WRITE_4( sc, INTSTAT, status );

	if( status & (INTSTAT_RQE|INTSTAT_RCC|INTSTAT_OVW) ) {
            epic_rx_done( sc );
            if( status & (INTSTAT_RQE|INTSTAT_OVW) ){
#if defined(EPIC_DEBUG)
                if( status & INTSTAT_OVW ) 
                    printf(EPIC_FORMAT ": RX buffer overflow\n",EPIC_ARGS(sc));
                if( status & INTSTAT_RQE ) 
                    printf(EPIC_FORMAT ": RX FIFO overflow\n",EPIC_ARGS(sc));
                if( sc->sc_if.if_flags & IFF_DEBUG ) 
                    epic_dump_state(sc);
#endif
                if( !(CSR_READ_4( sc, COMMAND ) & COMMAND_RXQUEUED) )
                    CSR_WRITE_4( sc, COMMAND, COMMAND_RXQUEUED );
                sc->sc_if.if_ierrors++;
            }
        }

        if( status & (INTSTAT_TXC|INTSTAT_TCC|INTSTAT_TQE) ) {
            epic_tx_done( sc );
	    if(!(sc->sc_if.if_flags & IFF_OACTIVE) &&
		sc->sc_if.if_snd.ifq_head )
		    epic_ifstart( &sc->sc_if );
	}

        if( (status & INTSTAT_GP2) && (QS6612_OUI == sc->phyid) ) {
	    u_int32_t phystatus = PHY_READ_2( sc, QS6612_INTSTAT );

	    if( phystatus & INTSTAT_AN_COMPLETE ) {
		u_int32_t bmcr;
		if( epic_autoneg(sc) == EPIC_FULL_DUPLEX ) {
		    dprintf((EPIC_FORMAT ": going fullduplex\n",EPIC_ARGS(sc)));
		    bmcr = BMCR_FULL_DUPLEX | PHY_READ_2( sc, DP83840_BMCR );
		    sc->txcon |= TXCON_FULL_DUPLEX;
		} else {
		    /* Default to half-duplex */
		    dprintf((EPIC_FORMAT ": going halfduplex\n",EPIC_ARGS(sc)));
		    bmcr = ~BMCR_FULL_DUPLEX & PHY_READ_2( sc, DP83840_BMCR );
		    sc->txcon &= ~TXCON_FULL_DUPLEX;
		}

		/* There is apparently QS6612 chip bug: */
		/* BMCR_FULL_DUPLEX flag is not updated by */
		/* autonegotiation process, so update it by hands */
		/* so we can rely on it in epic_ifmedia_status() */
		PHY_WRITE_2( sc, DP83840_BMCR, bmcr );

		epic_stop_activity(sc);
		epic_set_tx_mode(sc);
		epic_start_activity(sc);
	    }

	    PHY_READ_2(sc, DP83840_BMSR);
	    if( !(PHY_READ_2(sc, DP83840_BMSR) & BMSR_LINK_STATUS) ) {
		dprintf((EPIC_FORMAT ": WARNING! link down\n",EPIC_ARGS(sc)));
		sc->flags |= EPIC_LINK_DOWN;
	    } else {
    		dprintf((EPIC_FORMAT ": link up\n",EPIC_ARGS(sc)));
		sc->flags &= ~EPIC_LINK_DOWN;
	    }

	    /* We should clear GP2 int again after we clear it on PHY */
	    CSR_WRITE_4( sc, INTSTAT, INTSTAT_GP2 ); 
	}

	/* Check for errors */
	if( status & (INTSTAT_FATAL|INTSTAT_PMA|INTSTAT_PTA|
		      INTSTAT_APE|INTSTAT_DPE|INTSTAT_TXU|INTSTAT_RXE) ){
    	    if( status & (INTSTAT_FATAL|INTSTAT_PMA|INTSTAT_PTA|
			  INTSTAT_APE|INTSTAT_DPE) ){
		printf(EPIC_FORMAT ": PCI fatal error occured (%s%s%s%s)\n",
    		    EPIC_ARGS(sc),
		    (status&INTSTAT_PMA)?"PMA":"",
		    (status&INTSTAT_PTA)?" PTA":"",
		    (status&INTSTAT_APE)?" APE":"",
		    (status&INTSTAT_DPE)?" DPE":""
		);

		epic_dump_state(sc);

		epic_stop(sc);
		epic_init(sc);
		
	    	break;
	    }

	    if (status & INTSTAT_RXE) {
		dprintf((EPIC_FORMAT ": CRC/Alignment error\n",EPIC_ARGS(sc)));
		sc->sc_if.if_ierrors++;
	    }

	    /* Tx FIFO underflow. Increase tx threshold, */
	    /* if it grown above 2048, disable EARLY_TX */
	    if (status & INTSTAT_TXU) {
		if( sc->tx_threshold > 0x800 ) {
		    sc->txcon &= ~TXCON_EARLY_TRANSMIT_ENABLE;
    		    dprintf((EPIC_FORMAT ": TX underrun error, early tx disabled\n",EPIC_ARGS(sc)));
		} else {
		    sc->tx_threshold += 0x40;
    		    dprintf((EPIC_FORMAT ": TX underrun error, tx threshold increased to %d\n",EPIC_ARGS(sc),sc->tx_threshold));
		}

		CSR_WRITE_4(sc, COMMAND, COMMAND_TXUGO | COMMAND_TXQUEUED);
		epic_stop_activity(sc);
		epic_set_tx_mode(sc);
		epic_start_activity(sc);
		sc->sc_if.if_oerrors++;
	    }
	}
    }

    /* If no packets are pending, thus no timeouts */
    if( sc->pending_txs == 0 ) sc->sc_if.if_timer = 0;

#if defined(__OpenBSD__)
    return claimed;
#endif
}

/*
 * Synopsis: This one is called if packets wasn't transmitted
 * during timeout. Try to deallocate transmitted packets, and 
 * if success continue to work.
 *
 * splimp() invoked here
 */
static void
epic_ifwatchdog __P((
    struct ifnet *ifp))
{
	epic_softc_t *sc = ifp->if_softc;
	int x;

	x = splimp();

	printf(EPIC_FORMAT ": device timeout %d packets, ",
	    EPIC_ARGS(sc),sc->pending_txs);

	/* Try to finish queued packets */
	epic_tx_done( sc );

	/* If not successful */
	if( sc->pending_txs > 0 ){
#if defined(EPIC_DEBUG)
		if( ifp->if_flags & IFF_DEBUG ) epic_dump_state(sc);
#endif
		ifp->if_oerrors+=sc->pending_txs;

		/* Reinitialize board */
		printf("reinitialization\n");
		epic_stop(sc);
		epic_init(sc);

	} else 
		printf("seems we can continue normaly\n");

	/* Start output */
	if( ifp->if_snd.ifq_head ) epic_ifstart( ifp );

	splx(x);
}

#if defined(SIOCSIFMEDIA) && !defined(EPIC_NOIFMEDIA)
static int
epic_ifmedia_change __P((
    struct ifnet * ifp))
{
	epic_softc_t *sc = (epic_softc_t *)(ifp->if_softc);

	if (IFM_TYPE(sc->ifmedia.ifm_media) != IFM_ETHER)
        	return (EINVAL);

	if (!(ifp->if_flags & IFF_UP))
		return (0);

	epic_stop_activity(sc);
	epic_set_media_speed(sc);
	epic_start_activity(sc);

	return 0;
}

static void
epic_ifmedia_status __P((
    struct ifnet * ifp,
    struct ifmediareq *ifmr))
{
	epic_softc_t *sc = ifp->if_softc;
	u_int32_t bmcr;
	u_int32_t bmsr;

	if (!(ifp->if_flags & IFF_UP))
		return;

	bmcr = PHY_READ_2( sc, DP83840_BMCR );

	PHY_READ_2( sc, DP83840_BMSR );
	bmsr = PHY_READ_2( sc, DP83840_BMSR );

	ifmr->ifm_active = IFM_ETHER;
	ifmr->ifm_status = IFM_AVALID;

	if( !(bmsr & BMSR_LINK_STATUS) ) { 
		ifmr->ifm_active |= 
		    (bmcr&BMCR_AUTONEGOTIATION)?IFM_AUTO:IFM_NONE;
		return;
	}

	ifmr->ifm_status |= IFM_ACTIVE;
	ifmr->ifm_active |= (bmcr & BMCR_100MBPS) ? IFM_100_TX : IFM_10_T;
	ifmr->ifm_active |= (bmcr & BMCR_FULL_DUPLEX) ? IFM_FDX : 0;
	if ((sc->txcon & TXCON_LOOPBACK_MODE) == TXCON_LOOPBACK_MODE_INT)
		ifmr->ifm_active |= (IFM_LOOP | IFM_FLAG1);
	else if ((sc->txcon & TXCON_LOOPBACK_MODE) == TXCON_LOOPBACK_MODE_PHY)
		ifmr->ifm_active |= IFM_LOOP;

}
#endif

/*
 * Reset chip, PHY, allocate rings
 * 
 * splimp() invoked here
 */
static int 
epic_init __P((
    epic_softc_t * sc))
{       
	struct ifnet *ifp = &sc->sc_if;
	int s,i;
 
	s = splimp();

	/* Soft reset the chip (we have to power up card before) */
	CSR_WRITE_4( sc, GENCTL, 0 );
	CSR_WRITE_4( sc, GENCTL, GENCTL_SOFT_RESET );

	/*
	 * Reset takes 15 pci ticks which depends on PCI bus speed.
	 * Assuming it >= 33000000 hz, we have wait at least 495e-6 sec.
	 */
	DELAY(500);

	/* Wake up */
	CSR_WRITE_4( sc, GENCTL, 0 );

	/* Workaround for Application Note 7-15 */
	for (i=0; i<16; i++) CSR_WRITE_4(sc, TEST1, TEST1_CLOCK_TEST);

	/* Initialize rings */
	if( epic_init_rings( sc ) ) {
		printf(EPIC_FORMAT ": failed to init rings\n",EPIC_ARGS(sc));
		splx(s);
		return -1;
	}	

	/* Give rings to EPIC */
	CSR_WRITE_4( sc, PRCDAR, vtophys( sc->rx_desc ) );
	CSR_WRITE_4( sc, PTCDAR, vtophys( sc->tx_desc ) );

	/* Put node address to EPIC */
	CSR_WRITE_4( sc, LAN0, ((u_int16_t *)sc->sc_macaddr)[0] );
        CSR_WRITE_4( sc, LAN1, ((u_int16_t *)sc->sc_macaddr)[1] );
	CSR_WRITE_4( sc, LAN2, ((u_int16_t *)sc->sc_macaddr)[2] );

	/* Set tx mode, includeing transmit threshold */
	epic_set_tx_mode(sc);

	/* Compute and set RXCON. */
	epic_set_rx_mode( sc );

	/* Set multicast table */
	epic_set_mc_table( sc );

	/* Enable interrupts by setting the interrupt mask. */
	CSR_WRITE_4( sc, INTMASK,
		INTSTAT_RCC | INTSTAT_RQE | INTSTAT_OVW | INTSTAT_RXE |
		INTSTAT_TXC | INTSTAT_TCC | INTSTAT_TQE | INTSTAT_TXU |
		INTSTAT_FATAL |
		((QS6612_OUI == sc->phyid)?INTSTAT_GP2:0) );

	/* Enable interrupts,  set for PCI read multiple and etc */
	CSR_WRITE_4( sc, GENCTL,
		GENCTL_ENABLE_INTERRUPT | GENCTL_MEMORY_READ_MULTIPLE |
		GENCTL_ONECOPY | GENCTL_RECEIVE_FIFO_THRESHOLD64 );

	/* Set media speed mode */
	epic_set_media_speed( sc );

	/* Mark interface running ... */
	if( ifp->if_flags & IFF_UP ) ifp->if_flags |= IFF_RUNNING;
	else ifp->if_flags &= ~IFF_RUNNING;

	/* ... and free */
	ifp->if_flags &= ~IFF_OACTIVE;

	/* Start Rx process */
	epic_start_activity(sc);

	splx(s);
	return 0;
}

/*
 * Synopsis: calculate and set Rx mode. Chip must be in idle state to
 * access RXCON.
 */
static void
epic_set_rx_mode(
    epic_softc_t * sc)
{
	u_int32_t flags = sc->sc_if.if_flags;
        u_int32_t rxcon = RXCON_DEFAULT | RXCON_RECEIVE_MULTICAST_FRAMES | RXCON_RECEIVE_BROADCAST_FRAMES;

	rxcon |= (flags & IFF_PROMISC)?RXCON_PROMISCUOUS_MODE:0;

	CSR_WRITE_4( sc, RXCON, rxcon );

	return;
}

void
dump_phy_regs(epic_softc_t *sc) {

	printf("BMCR: 0x%04x\n", PHY_READ_2(sc, DP83840_BMCR));
	printf("BMSR: 0x%04x\n", PHY_READ_2(sc, DP83840_BMSR));
	printf("ANAR: 0x%04x\n", PHY_READ_2(sc, DP83840_ANAR));
	printf("LPAR: 0x%04x\n", PHY_READ_2(sc, DP83840_LPAR));
	printf("ANER: 0x%04x\n", PHY_READ_2(sc, DP83840_ANER));
	printf("MCTL: 0x%04x\n", PHY_READ_2(sc, QS6612_MCTL));
	printf("INTSTAT: 0x%04x\n", PHY_READ_2(sc, QS6612_INTSTAT));
	printf("INTMASK: 0x%04x\n", PHY_READ_2(sc, QS6612_INTMASK));
	printf("BPCR: 0x%04x\n", PHY_READ_2(sc, QS6612_BPCR));
}

/*
 * Synopsis: Reset PHY and do PHY-special initialization:
 */
static void
epic_init_phy __P((
    epic_softc_t * sc))
{
	u_int32_t i;

	/* Reset PHY (We have to take the delay from manual XXX) */
	PHY_WRITE_2(sc, DP83840_BMCR, BMCR_RESET);
	DELAY(10);
	for(i=0;i<0x1000;i++) {
		if( !(PHY_READ_2(sc, DP83840_BMCR) & BMCR_RESET) )
			break;
		DELAY(1);
	}

	if( PHY_READ_2(sc, DP83840_BMCR) & BMCR_RESET )
		printf(EPIC_FORMAT ": WARNING! cant reset PHY\n",EPIC_ARGS(sc));

	PHY_WRITE_2(sc, DP83840_BMCR, 0 );
	PHY_WRITE_2(sc, DP83840_BMCR, BMCR_LOOPBACK | BMCR_ISOLATE );

	switch( sc->phyid ){
	case QS6612_OUI: {
		/* Init QS6612 and EPIC to generate interrupt */
		CSR_WRITE_4(sc, NVCTL, NVCTL_GP1_OUTPUT_ENABLE | NVCTL_GP1);

		/* Mask interrupts sources */
		PHY_WRITE_2(sc, QS6612_INTMASK,
			PHY_READ_2(sc, QS6612_INTSTAT) |	
			INTMASK_THUNDERLAN | INTSTAT_AN_COMPLETE |
			INTSTAT_LINK_STATUS );

		/* Enable QS6612 extended cable length capabilites */
		/* PHY_WRITE_2(sc, QS6612_MCTL,			   */
		/*	PHY_READ_2(sc, QS6612_MCTL) | MCTL_BTEXT); */

		break;
	}
	default:
		break;
	}
}

/*
 * Synopsis: Set PHY to media type specified by IFF_LINK* flags or
 * ifmedia structure. Chip must be in idle state to access TXCON.
 */
static void
epic_set_media_speed __P((
    epic_softc_t * sc))
{
	u_int16_t media;
#if !defined(EPIC_NOIFMEDIA)
	u_int32_t tgtmedia = sc->ifmedia.ifm_cur->ifm_media;
#endif

	epic_init_phy(sc);

#if !defined(EPIC_NOIFMEDIA)
	if( IFM_SUBTYPE(tgtmedia) != IFM_AUTO ){
		/* Clean previous values */
		sc->txcon &= ~(TXCON_LOOPBACK_MODE | TXCON_FULL_DUPLEX);
		media = 0;

		/* Set mode */
		media |= (IFM_SUBTYPE(tgtmedia)==IFM_100_TX) ? BMCR_100MBPS : 0;
		if (tgtmedia & IFM_FDX) {
			media |= BMCR_FULL_DUPLEX;
 			sc->txcon |= TXCON_FULL_DUPLEX;
		}
		if (tgtmedia & IFM_LOOP) {
			if (tgtmedia & IFM_FLAG1)
				sc->txcon |= TXCON_LOOPBACK_MODE_INT;
			else {
				media |= BMCR_LOOPBACK | BMCR_ISOLATE;
				sc->txcon |= TXCON_LOOPBACK_MODE_PHY;
			}
		}

		sc->sc_if.if_baudrate = 
			(IFM_SUBTYPE(tgtmedia)==IFM_100_TX)?100000000:10000000;

		PHY_WRITE_2( sc, DP83840_BMCR, media );
	}
#else /* EPIC_NOIFMEDIA */
	struct ifnet *ifp = &sc->sc_if;

	if( ifp->if_flags & IFF_LINK0 ) {
		/* Set mode */
		media = 0;
		media|= (ifp->if_flags & IFF_LINK2) ? BMCR_100MBPS : 0;
		media|= (ifp->if_flags & IFF_LINK1) ? BMCR_FULL_DUPLEX : 0;

		sc->sc_if.if_baudrate = 
			(ifp->if_flags & IFF_LINK2)?100000000:10000000;

		PHY_WRITE_2( sc, DP83840_BMCR, media );

		if( ifp->if_flags & IFF_LINK2 ) sc->txcon |= TXCON_FULL_DUPLEX;
		else sc->txcon &= ~TXCON_FULL_DUPLEX; 
 
		CSR_WRITE_4( sc, TXCON, sc->txcon );
	}
#endif /* !EPIC_NOIFMEDIA */
	  else {
		sc->sc_if.if_baudrate = 100000000;

		sc->txcon &= ~TXCON_FULL_DUPLEX; 
		CSR_WRITE_4(sc, TXCON, sc->txcon);

		/* Set and restart autoneg */
		PHY_WRITE_2(sc, DP83840_BMCR, BMCR_AUTONEGOTIATION );
		PHY_WRITE_2(sc, DP83840_BMCR,
			BMCR_AUTONEGOTIATION | BMCR_RESTART_AUTONEG);

		/* If it is not QS6612 PHY, try to get result of autoneg. */
		if( QS6612_OUI != sc->phyid ) {
			/* Wait 3 seconds for the autoneg to finish
			 * This is the recommended time from the DP83840A data
			 * sheet Section 7.1
			 */
        		DELAY(3000000);
			
			if( epic_autoneg(sc) == EPIC_FULL_DUPLEX ) {
				sc->txcon |= TXCON_FULL_DUPLEX;
				CSR_WRITE_4(sc, TXCON, sc->txcon);
			}
		}
		/* Else it will be done when GP2 int occured */
	}

	epic_set_tx_mode(sc);

	return;
}

/*
 * This functions get results of the autoneg processes of the phy
 * It implements the workaround that is described in section 7.2 & 7.3 of the 
 * DP83840A data sheet
 * http://www.national.com/ds/DP/DP83840A.pdf
 */
static int 
epic_autoneg(
    epic_softc_t * sc)
{
	u_int16_t media;
	u_int16_t i;

        /* BMSR must be read twice to update the link status bit
	 * since that bit is a latch bit
         */
	PHY_READ_2( sc, DP83840_BMSR);
	i = PHY_READ_2( sc, DP83840_BMSR);
        
        if ((i & BMSR_LINK_STATUS) && (i & BMSR_AUTONEG_COMPLETE)){
		i = PHY_READ_2( sc, DP83840_LPAR );

		if ( i & (ANAR_100_TX_FD|ANAR_10_FD) )
			return 	EPIC_FULL_DUPLEX;
		else
			return EPIC_HALF_DUPLEX;
        } else {   
		/*Auto-negotiation or link status is not 1
		  Thus the auto-negotiation failed and one
		  must take other means to fix it.
		 */

		/* ANER must be read twice to get the correct reading for the 
		 * Multiple link fault bit -- it is a latched bit
	 	 */
 		PHY_READ_2( sc, DP83840_ANER );
		i = PHY_READ_2( sc, DP83840_ANER );
	
		if ( i & ANER_MULTIPLE_LINK_FAULT ) {
			/* it can be forced to 100Mb/s Half-Duplex */
	 		media = PHY_READ_2( sc, DP83840_BMCR );
			media &= ~(BMCR_AUTONEGOTIATION | BMCR_FULL_DUPLEX);
			media |= BMCR_100MBPS;
			PHY_WRITE_2( sc, DP83840_BMCR, media );
		
			/* read BMSR again to determine link status */
			PHY_READ_2( sc, DP83840_BMSR );
			i=PHY_READ_2( sc, DP83840_BMSR );
		
			if (i & BMSR_LINK_STATUS){
				/* port is linked to the non Auto-Negotiation
				 * 100Mbs partner.
			 	 */
				return EPIC_HALF_DUPLEX;
			}
			else {
				media = PHY_READ_2( sc, DP83840_BMCR);
				media &= ~(BMCR_AUTONEGOTIATION | BMCR_FULL_DUPLEX | BMCR_100MBPS);
				PHY_WRITE_2( sc, DP83840_BMCR, media);
				PHY_READ_2( sc, DP83840_BMSR );
				i = PHY_READ_2( sc, DP83840_BMSR );

				if (i & BMSR_LINK_STATUS) {
					/*port is linked to the non
					 * Auto-Negotiation10Mbs partner
			 	 	 */
					return EPIC_HALF_DUPLEX;
				}
			}
		}
		/* If we get here we are most likely not connected
		 * so lets default it to half duplex
		 */
		return EPIC_HALF_DUPLEX;
	}
	
}

/*
 */
static void
epic_set_tx_mode (
    epic_softc_t *sc )
{

    if( sc->txcon & TXCON_EARLY_TRANSMIT_ENABLE )
	CSR_WRITE_4( sc, ETXTHR, sc->tx_threshold );

    CSR_WRITE_4( sc, TXCON, sc->txcon );
}

/*
 * Synopsis: This function should update multicast hash table.
 * I suppose there is a bug in chips MC filter so this function
 * only set it to receive all MC packets. The second problem is
 * that we should wait for TX and RX processes to stop before
 * reprogramming MC filter. The epic_stop_activity() and 
 * epic_start_activity() should help to do this.
 */
static void
epic_set_mc_table (
    epic_softc_t * sc)
{
	struct ifnet *ifp = &sc->sc_if;

	if( ifp->if_flags & IFF_MULTICAST ){
		CSR_WRITE_4( sc, MC0, 0xFFFF );
		CSR_WRITE_4( sc, MC1, 0xFFFF );
		CSR_WRITE_4( sc, MC2, 0xFFFF );
		CSR_WRITE_4( sc, MC3, 0xFFFF );
	}

	return;
}


/* 
 * Synopsis: Start receive process and transmit one, if they need.
 */
static void
epic_start_activity __P((
    epic_softc_t * sc))
{
    /* Start rx process */
    CSR_WRITE_4(sc, COMMAND,
	COMMAND_RXQUEUED | COMMAND_START_RX |
	(sc->pending_txs?COMMAND_TXQUEUED:0));
    dprintf((EPIC_FORMAT ": activity started\n",EPIC_ARGS(sc)));
}

/*
 * Synopsis: Completely stop Rx and Tx processes. If TQE is set additional
 * packet needs to be queued to stop Tx DMA.
 */
static void
epic_stop_activity __P((
    epic_softc_t * sc))
{
    int i;

    /* Stop Tx and Rx DMA */
    CSR_WRITE_4(sc,COMMAND,COMMAND_STOP_RX|COMMAND_STOP_RDMA|COMMAND_STOP_TDMA);

    /* Wait Rx and Tx DMA to stop (why 1 ms ??? XXX) */
    dprintf((EPIC_FORMAT ": waiting Rx and Tx DMA to stop\n",EPIC_ARGS(sc)));
    for(i=0;i<0x1000;i++) {
	if((CSR_READ_4(sc,INTSTAT) & (INTSTAT_TXIDLE | INTSTAT_RXIDLE)) == 
	   (INTSTAT_TXIDLE | INTSTAT_RXIDLE) )
	    break;
	DELAY(1);
    }

    if( !(CSR_READ_4(sc,INTSTAT)&INTSTAT_RXIDLE) ) 
	printf(EPIC_FORMAT ": can't stop Rx DMA\n",EPIC_ARGS(sc));

    if( !(CSR_READ_4(sc,INTSTAT)&INTSTAT_TXIDLE) ) 
	printf(EPIC_FORMAT ": can't stop Tx DMA\n",EPIC_ARGS(sc));

    /* Catch all finished packets */
    epic_rx_done(sc);
    epic_tx_done(sc);

    /*
     * May need to queue one more packet if TQE, this is rare but existing
     * case.
     */
    if( (CSR_READ_4( sc, INTSTAT ) & INTSTAT_TQE) &&
       !(CSR_READ_4( sc, INTSTAT ) & INTSTAT_TXIDLE) ) {
	struct epic_tx_desc *desc;
	struct epic_frag_list *flist;
	struct epic_tx_buffer *buf;
	struct mbuf *m0;

	dprintf((EPIC_FORMAT ": queue last packet\n",EPIC_ARGS(sc)));

	desc = sc->tx_desc + sc->cur_tx;
	flist = sc->tx_flist + sc->cur_tx;
	buf = sc->tx_buffer + sc->cur_tx;

	if ((desc->status & 0x8000) || (buf->mbuf != NULL))
	    return;

	MGETHDR(m0,M_DONTWAIT,MT_DATA);
	if (NULL == m0)
	    return;

	/* Prepare mbuf */
	m0->m_len = min(MHLEN,ETHER_MIN_LEN-ETHER_CRC_LEN);
	flist->frag[0].fraglen = m0->m_len;
	m0->m_pkthdr.len = m0->m_len;
	m0->m_pkthdr.rcvif = &sc->sc_if;
	bzero(mtod(m0,caddr_t),m0->m_len);

	/* Fill fragments list */
	flist->frag[0].fraglen = m0->m_len; 
	flist->frag[0].fragaddr = vtophys( mtod(m0, caddr_t) );
	flist->numfrags = 1;

	/* Fill in descriptor */
	buf->mbuf = m0;
	sc->pending_txs++;
	sc->cur_tx = (sc->cur_tx + 1) & TX_RING_MASK;
	desc->control = 0x01;
	desc->txlength = max(m0->m_pkthdr.len,ETHER_MIN_LEN-ETHER_CRC_LEN);
	desc->status = 0x8000;

	/* Launch transmition */
	CSR_WRITE_4(sc, COMMAND, COMMAND_STOP_TDMA | COMMAND_TXQUEUED);

	/* Wait Tx DMA to stop (for how long??? XXX) */
	dprintf((EPIC_FORMAT ": waiting Tx DMA to stop\n",EPIC_ARGS(sc)));
	for(i=0;i<1000;i++) {
	    if( (CSR_READ_4(sc,INTSTAT)&INTSTAT_TXIDLE) == INTSTAT_TXIDLE )
		break;
	    DELAY(1);
	}

	if( !(CSR_READ_4(sc,INTSTAT)&INTSTAT_TXIDLE) )
	    printf(EPIC_FORMAT ": can't stop TX DMA\n",EPIC_ARGS(sc));
	else
	    epic_tx_done(sc);
    }

    dprintf((EPIC_FORMAT ": activity stoped\n",EPIC_ARGS(sc)));
}

/*
 *  Synopsis: Shut down board and deallocates rings.
 *
 *  splimp() invoked here
 */
static void
epic_stop __P((
    epic_softc_t * sc))
{
	int s;

	s = splimp();

	sc->sc_if.if_timer = 0;

	/* Disable interrupts */
	CSR_WRITE_4( sc, INTMASK, 0 );
	CSR_WRITE_4( sc, GENCTL, 0 );

	/* Try to stop Rx and TX processes */
	epic_stop_activity(sc);

	/* Reset chip */
	CSR_WRITE_4( sc, GENCTL, GENCTL_SOFT_RESET );
	DELAY(1000);

	/* Make chip go to bed */
	CSR_WRITE_4(sc, GENCTL, GENCTL_POWER_DOWN);

	/* Free memory allocated for rings */
	epic_free_rings(sc);

	/* Mark as stoped */
	sc->sc_if.if_flags &= ~IFF_RUNNING;

	splx(s);
	return;
}

/*
 * Synopsis: This function should free all memory allocated for rings.
 */ 
static void
epic_free_rings __P((
    epic_softc_t * sc))
{
	int i;

	for(i=0;i<RX_RING_SIZE;i++){
		struct epic_rx_buffer *buf = sc->rx_buffer + i;
		struct epic_rx_desc *desc = sc->rx_desc + i;
		
		desc->status = 0;
		desc->buflength = 0;
		desc->bufaddr = 0;

		if( buf->mbuf ) m_freem( buf->mbuf );
		buf->mbuf = NULL;
	}

	for(i=0;i<TX_RING_SIZE;i++){
		struct epic_tx_buffer *buf = sc->tx_buffer + i;
		struct epic_tx_desc *desc = sc->tx_desc + i;

		desc->status = 0;
		desc->buflength = 0;
		desc->bufaddr = 0;

		if( buf->mbuf ) m_freem( buf->mbuf );
		buf->mbuf = NULL;
	}
}

/*
 * Synopsis:  Allocates mbufs for Rx ring and point Rx descs to them.
 * Point Tx descs to fragment lists. Check that all descs and fraglists
 * are bounded and aligned properly.
 */
static int
epic_init_rings(epic_softc_t * sc){
	int i;

	sc->cur_rx = sc->cur_tx = sc->dirty_tx = sc->pending_txs = 0;

	for (i = 0; i < RX_RING_SIZE; i++) {
		struct epic_rx_buffer *buf = sc->rx_buffer + i;
		struct epic_rx_desc *desc = sc->rx_desc + i;

		desc->status = 0;		/* Owned by driver */
		desc->next = vtophys( sc->rx_desc + ((i+1) & RX_RING_MASK) );

		if( (desc->next & 3) || ((desc->next & 0xFFF) + sizeof(struct epic_rx_desc) > 0x1000 ) )
			printf(EPIC_FORMAT ": WARNING! rx_desc is misbound or misaligned\n",EPIC_ARGS(sc));

		EPIC_MGETCLUSTER( buf->mbuf );
		if( NULL == buf->mbuf ) {
			epic_free_rings(sc);
			return -1;
		}
		desc->bufaddr = vtophys( mtod(buf->mbuf,caddr_t) );

		desc->buflength = ETHER_MAX_FRAME_LEN;
		desc->status = 0x8000;			/* Give to EPIC */

	}

	for (i = 0; i < TX_RING_SIZE; i++) {
		struct epic_tx_buffer *buf = sc->tx_buffer + i;
		struct epic_tx_desc *desc = sc->tx_desc + i;

		desc->status = 0;
		desc->next = vtophys( sc->tx_desc + ( (i+1) & TX_RING_MASK ) );

		if( (desc->next & 3) || ((desc->next & 0xFFF) + sizeof(struct epic_tx_desc) > 0x1000 ) )
			printf(EPIC_FORMAT ": WARNING! tx_desc is misbound or misaligned\n",EPIC_ARGS(sc));

		buf->mbuf = NULL;
		desc->bufaddr = vtophys( sc->tx_flist + i );
		if( (desc->bufaddr & 3) || ((desc->bufaddr & 0xFFF) + sizeof(struct epic_frag_list) > 0x1000 ) )
			printf(EPIC_FORMAT ": WARNING! frag_list is misbound or misaligned\n",EPIC_ARGS(sc));
	}

	return 0;
}

/*
 * EEPROM operation functions
 */
static void epic_write_eepromreg __P((
    epic_softc_t *sc,
    u_int8_t val))
{
	u_int16_t i;

	CSR_WRITE_1( sc, EECTL, val );

	for (i=0; i<0xFF; i++)
		if( !(CSR_READ_1( sc, EECTL ) & 0x20) ) break;

	return;
}

static u_int8_t
epic_read_eepromreg __P((
    epic_softc_t *sc))
{
	return CSR_READ_1( sc,EECTL );
}  

static u_int8_t
epic_eeprom_clock __P((
    epic_softc_t *sc,
    u_int8_t val))
{
	epic_write_eepromreg( sc, val );
	epic_write_eepromreg( sc, (val | 0x4) );
	epic_write_eepromreg( sc, val );
	
	return epic_read_eepromreg( sc );
}

static void
epic_output_eepromw __P((
    epic_softc_t * sc,
    u_int16_t val))
{
	int i;          
	for( i = 0xF; i >= 0; i--){
		if( (val & (1 << i)) ) epic_eeprom_clock( sc, 0x0B );
		else epic_eeprom_clock( sc, 3);
	}
}

static u_int16_t
epic_input_eepromw __P((
    epic_softc_t *sc))
{
	int i;
	int tmp;
	u_int16_t retval = 0;

	for( i = 0xF; i >= 0; i--) {	
		tmp = epic_eeprom_clock( sc, 0x3 );
		if( tmp & 0x10 ){
			retval |= (1 << i);
		}
	}
	return retval;
}

static int
epic_read_eeprom __P((
    epic_softc_t *sc,
    u_int16_t loc))
{
	u_int16_t dataval;
	u_int16_t read_cmd;

	epic_write_eepromreg( sc , 3);

	if( epic_read_eepromreg( sc ) & 0x40 )
		read_cmd = ( loc & 0x3F ) | 0x180;
	else
		read_cmd = ( loc & 0xFF ) | 0x600;

	epic_output_eepromw( sc, read_cmd );

        dataval = epic_input_eepromw( sc );

	epic_write_eepromreg( sc, 1 );
	
	return dataval;
}

static u_int16_t
epic_read_phy_register __P((
    epic_softc_t *sc,
    u_int16_t loc))
{
	int i;

	CSR_WRITE_4( sc, MIICTL, ((loc << 4) | 0x0601) );

	for (i=0;i<0x100;i++) {
		if( !(CSR_READ_4( sc, MIICTL )&1) ) break;
		DELAY(1);
	}

	return CSR_READ_4( sc, MIIDATA );
}

static void
epic_write_phy_register __P((
    epic_softc_t * sc,
    u_int16_t loc,
    u_int16_t val))
{
	int i;

	CSR_WRITE_4( sc, MIIDATA, val );
	CSR_WRITE_4( sc, MIICTL, ((loc << 4) | 0x0602) );

	for( i=0;i<0x100;i++) {
		if( !(CSR_READ_4( sc, MIICTL )&2) ) break;
		DELAY(1);
	}

	return;
}

static void
epic_dump_state __P((
    epic_softc_t * sc))
{
	int j;
	struct epic_tx_desc *tdesc;
	struct epic_rx_desc *rdesc;
	printf(EPIC_FORMAT ": cur_rx: %d, pending_txs: %d, dirty_tx: %d, cur_tx: %d\n", EPIC_ARGS(sc),sc->cur_rx,sc->pending_txs,sc->dirty_tx,sc->cur_tx);
	printf(EPIC_FORMAT ": COMMAND: 0x%08x, INTSTAT: 0x%08x\n",EPIC_ARGS(sc),CSR_READ_4(sc,COMMAND),CSR_READ_4(sc,INTSTAT));
	printf(EPIC_FORMAT ": PRCDAR: 0x%08x, PTCDAR: 0x%08x\n",EPIC_ARGS(sc),CSR_READ_4(sc,PRCDAR),CSR_READ_4(sc,PTCDAR));
	printf(EPIC_FORMAT ": dumping rx descriptors\n",EPIC_ARGS(sc));
	for(j=0;j<RX_RING_SIZE;j++){
		rdesc = sc->rx_desc + j;
		printf("desc%d: %4d 0x%04x, 0x%08x, %4d, 0x%08x\n",
			j,
			rdesc->rxlength,rdesc->status,
			rdesc->bufaddr,
			rdesc->buflength,
			rdesc->next
		);
	}
	printf(EPIC_FORMAT ": dumping tx descriptors\n",EPIC_ARGS(sc));
	for(j=0;j<TX_RING_SIZE;j++){
		tdesc = sc->tx_desc + j;
		printf(
 		"desc%d: %4d 0x%04x, 0x%08lx, 0x%04x %4u, 0x%08lx, mbuf: %p\n",
			j,
			tdesc->txlength,tdesc->status,
			(u_long)tdesc->bufaddr,
			tdesc->control,tdesc->buflength,
			(u_long)tdesc->next,
			(void *)sc->tx_buffer[j].mbuf
		);
	}
}
#endif /* NPCI > 0 */
