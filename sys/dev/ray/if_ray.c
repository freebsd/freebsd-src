/*
 * Copyright (C) 2000
 * Dr. Duncan McLennan Barclay, dmlb@ragnet.demon.co.uk.
 *
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
 * THIS SOFTWARE IS PROVIDED BY DUNCAN BARCLAY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL DUNCAN BARCLAY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id$
 *
 */

#define XXX 0

/*
 * XXX build options - move to LINT
 */
#define RAY_NEED_STARTJOIN_TIMEO	/* Might be needed with build 4 */
#define RAY_DEBUG		100	/* Big numbers get more verbose */
#define RAY_CCS_TIMEOUT		(hz/2)	/* Timeout for CCS commands - only used for downloading startup parameters */
/*
 * XXX build options - move to LINT
 */

/*
 * Debugging odds and odds
 */
#ifndef RAY_DEBUG
#define RAY_DEBUG 0
#endif /* RAY_DEBUG */

#if RAY_DEBUG > 0
#define RAY_DHEX8(p, l) do { if (RAY_DEBUG > 10) {		\
    u_int8_t *i;						\
    for (i = p; i < (u_int8_t *)(p+l); i += 8)			\
    	printf("  0x%08lx %8D\n",				\
		(unsigned long)i, (unsigned char *)i, " ");	\
} } while (0)
#define RAY_DPRINTF(x)	do { if (RAY_DEBUG) {			\
    printf x ;							\
    } } while (0)
#else
#define RAY_HEX8(p, l)
#define RAY_HEX16(p, l)
#define RAY_DPRINTF(x)
#endif /* RAY_DEBUG */

#include "ray.h"
#include "card.h"
#include "apm.h"
#include "bpfilter.h"

#if NRAY > 0

#include <sys/param.h>
#include <sys/cdefs.h>
#include <sys/conf.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/systm.h>
#include <sys/uio.h>
#include <sys/proc.h>
#include <sys/ucred.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_mib.h>
#include <i386/isa/if_ieee80211.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif /* NBPFILTER */

#include <machine/clock.h>
#include <machine/md_var.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>

#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/if_rayreg.h>
#include <i386/isa/if_raymib.h>

#if NCARD > 0
#include <pccard/cardinfo.h>
#include <pccard/cis.h>
#include <pccard/driver.h>
#include <pccard/slot.h>
#endif /* NCARD */

#if NAPM > 0
#include <machine/apm_bios.h>
#endif /* NAPM */

/*
 * One of these structures per allocated device
 */
struct ray_softc {

    struct arpcom	arpcom;		/* Ethernet common */
    struct ifmedia	ifmedia;	/* Ifnet common */
    struct callout_handle \
    			timerh;		/* Handle for timer */
#ifdef RAY_NEED_STARTJOIN_TIMEO
    struct callout_handle \
    			sj_timerh;	/* Handle for start_join timer */
#endif /* RAY_NEED_STARTJOIN_TIMEO */

    char		*card_type;	/* Card model name */
    char		*vendor;	/* Card manufacturer */

    int			unit;		/* Unit number */
    u_char		gone;		/* 1 = Card bailed out */
    int			irq;		/* Assigned IRQ */
    caddr_t		maddr;		/* Shared RAM Address */
    int			msize;		/* Shared RAM Size */

    /* XXX these can go when attribute reading is fixed */
    int			slotnum;	/* Slot number */
    struct mem_desc	md;		/* Map info for common memory */

    struct ray_ecf_startup_v5	\
    			sc_ecf_startup; /* Startup info from card */

    u_int8_t		sc_ccsinuse[64];/* ccs in use -- not for tx */
    size_t		sc_ccs;		/* ccs used by non-scheduled,
    					 * non-overlapping procedures */

    u_int8_t		sc_bssid[ETHER_ADDR_LEN];	/* Current net values */
    u_int8_t		sc_cnwid[IEEE80211_NWID_LEN];	/* Last nwid */
    u_int8_t		sc_dnwid[IEEE80211_NWID_LEN];	/* Desired nwid */
    u_int8_t		sc_omode;	/* Old operating mode SC_MODE_xx */
    u_int8_t		sc_mode;	/* Current operating mode SC_MODE_xx */
    u_int8_t		sc_countrycode;	/* Current country code */
    u_int8_t		sc_dcountrycode;/* Desired country code */
    int			sc_havenet;	/* true if we have aquired a network */
};
static struct ray_softc ray_softc[NRAY];

#define	sc_station_addr	sc_ecf_startup.e_station_addr
#define	sc_version	sc_ecf_startup.e_fw_build_string
#define	sc_tibsize	sc_ecf_startup.e_tibsize

/* Modes of operation */
/*XXX must these be tied with defaults on the station type? or do they
 * decribe the network mode and not the station type? */
#define	SC_MODE_ADHOC	0	/* ad-hoc mode */
#define	SC_MODE_INFRA	1	/* infrastructure mode */

/* Commands -- priority given to LSB */
#define	SCP_FIRST		0x0001
#define	SCP_UPDATESUBCMD	0x0001
#define	SCP_STARTASSOC		0x0002
#define	SCP_REPORTPARAMS	0x0004
#define	SCP_IFSTART		0x0008

/* Update sub commands -- issues are serialized priority to LSB */
#define	SCP_UPD_FIRST		0x0100
#define	SCP_UPD_STARTUP		0x0100
#define	SCP_UPD_STARTJOIN	0x0200
#define	SCP_UPD_PROMISC		0x0400
#define	SCP_UPD_MCAST		0x0800
#define	SCP_UPD_UPDATEPARAMS	0x1000
#define	SCP_UPD_SHIFT		8
#define	SCP_UPD_MASK		0xff00

/* These command (a subset of the update set) require timeout checking */
#define	SCP_TIMOCHECK_CMD_MASK	\
	(SCP_UPD_UPDATEPARAMS | SCP_UPD_STARTUP | SCP_UPD_MCAST | \
	SCP_UPD_PROMISC)

/*
 * PCMCIA driver definition
 */
static int       ray_pccard_init	__P((struct pccard_devinfo *dev_p));
static void      ray_pccard_unload	__P((struct pccard_devinfo *dev_p));
static int       ray_pccard_intr	__P((struct pccard_devinfo *dev_p));

PCCARD_MODULE(ray, ray_pccard_init, ray_pccard_unload, ray_pccard_intr, 0, net_imask);

/*
 * ISA driver definition
 */
static int       ray_probe		__P((struct isa_device *dev));
static int       ray_attach		__P((struct isa_device *dev));
struct isa_driver raydriver = {
    ray_probe,
    ray_attach,
    "ray",
    1
};

/*
 * Network driver definition
 */
static void      ray_start		__P((struct ifnet *ifp));
static int       ray_ioctl		__P((struct ifnet *ifp, u_long command, caddr_t data));
static void      ray_watchdog		__P((struct ifnet *ifp));
static void      ray_init		__P((void *xsc));
static void      ray_stop		__P((struct ray_softc *sc));

/*
 * Internal utilites
 */
static int	ray_alloc_ccs		__P((struct ray_softc *sc, size_t *ccsp, u_int cmd, u_int track));
static void	ray_ccs_done		__P((struct ray_softc *sc, size_t ccs));
static void	ray_download_params	__P((struct ray_softc *sc));
static void	ray_download_timo	__P((void *xsc));
static u_int8_t	ray_free_ccs 		__P((struct ray_softc *sc, size_t ccs));
static int	ray_issue_cmd		__P((struct ray_softc *sc, size_t ccs, u_int track));
static void	ray_rcs_intr		__P((struct ray_softc *sc, size_t ccs));
static void	ray_start_join_timo	__P((void *xsc));
/*
 * Indirections for reading/writing shared memory - from NetBSD/if_ray.c
 */
#ifndef offsetof
#define offsetof(type, member) \
    ((size_t)(&((type *)0)->member))
#endif /* offsetof */

#define	SRAM_READ_1(sc, off) \
    (u_int8_t)*((sc)->maddr + (off))
/* ((u_int8_t)bus_space_read_1((sc)->sc_memt, (sc)->sc_memh, (off))) */

#define	SRAM_READ_FIELD_1(sc, off, s, f) \
    SRAM_READ_1(sc, (off) + offsetof(struct s, f))

#define	SRAM_READ_FIELD_2(sc, off, s, f)			\
    ((((u_int16_t)SRAM_READ_1(sc, (off) + offsetof(struct s, f)) << 8) \
    |(SRAM_READ_1(sc, (off) + 1 + offsetof(struct s, f)))))

#define	SRAM_READ_FIELD_N(sc, off, s, f, p, n)	\
    ray_read_region(sc, (off) + offsetof(struct s, f), (p), (n))

#define ray_read_region(sc, off, vp, n) \
    bcopy((sc)->maddr + (off), (vp), (n))

#define	SRAM_WRITE_1(sc, off, val)	\
    *((sc)->maddr + (off)) = (val)
/* bus_space_write_1((sc)->sc_memt, (sc)->sc_memh, (off), (val)) */

#define	SRAM_WRITE_FIELD_1(sc, off, s, f, v) 	\
    SRAM_WRITE_1(sc, (off) + offsetof(struct s, f), (v))

#define	SRAM_WRITE_FIELD_2(sc, off, s, f, v) do {	\
    SRAM_WRITE_1(sc, (off) + offsetof(struct s, f), (((v) >> 8 ) & 0xff)); \
    SRAM_WRITE_1(sc, (off) + 1 + offsetof(struct s, f), ((v) & 0xff)); \
} while (0)

#define	SRAM_WRITE_FIELD_N(sc, off, s, f, p, n)	\
    ray_write_region(sc, (off) + offsetof(struct s, f), (p), (n))

#define ray_write_region(sc, off, vp, n) \
    bcopy((vp), (sc)->maddr + (off), (n))

/*
 * Macro's
 */
#ifndef RAY_CCS_TIMEOUT
#define RAY_CCS_TIMEOUT		(hz / 2)
#endif
#define	RAY_ECF_READY(sc) 	ray_ecf_ready((sc))
#define	RAY_ECF_START_CMD(sc)	ray_attr_write((sc), RAY_ECFIR, RAY_ECFIR_IRQ)
#define	RAY_HCS_CLEAR_INTR(sc)	ray_attr_write((sc), RAY_HCSIR, 0)
#define RAY_HCS_INTR(sc)	ray_hcs_intr((sc))

/*
 * XXX
 * As described in if_xe.c...
 *
 * Horrid stuff for accessing CIS tuples and remapping common memory...
 * XXX
 */
#define CARD_MAJOR		50
static void	ray_attr_getmap	__P((struct ray_softc *sc));
static void	ray_attr_cm	__P((struct ray_softc *sc));
static int	ray_attr_write	__P((struct ray_softc *sc, off_t offset, u_int8_t byte));
static int	ray_attr_read	__P((struct ray_softc *sc, off_t offset, u_int8_t *buf, int size));
static int	ray_ecf_ready	__P((struct ray_softc *sc));
#define	RAY_MAP_CM(sc)		ray_attr_cm(sc)

/*
 * PCCard initialise.
 */
static int
ray_pccard_init (dev_p)
    struct pccard_devinfo   *dev_p;
{
    struct ray_softc	*sc;
    u_int32_t		irq;
    int			j;

    RAY_DPRINTF(("ray%d: PCCard probe\n", dev_p->isahd.id_unit));

    if (dev_p->isahd.id_unit >= NRAY)
	return(ENODEV);

    sc = &ray_softc[dev_p->isahd.id_unit];
    sc->gone = 0;
    sc->unit = dev_p->isahd.id_unit;
    sc->slotnum = dev_p->slt->slotnum;

    /* Get IRQ - encoded as a bitmask. */
    irq = dev_p->isahd.id_irq;
    for (j = 0; j < 32; j++) {
	if (irq & 0x1)
		break;
	irq >>= 1;
    }
    sc->irq = j;
    sc->maddr = dev_p->isahd.id_maddr;
    sc->msize = dev_p->isahd.id_msize;

    printf("ray%d: <Raylink/IEEE 802.11> maddr 0x%lx msize 0x%x irq %d on isa (PC-Card slot %d)\n",
	sc->unit, (unsigned long)sc->maddr, sc->msize, sc->irq, sc->slotnum);

    ray_attr_getmap(sc); /* XXX remove when attribute/common mapping fixed */

    if (ray_attach(&dev_p->isahd))
	return(ENXIO);

    return(0);
}

/*
 * PCCard unload.
 */
static void
ray_pccard_unload (dev_p)
    struct pccard_devinfo	*dev_p;
{
    struct ray_softc		*sc;
    struct ifnet		*ifp;

    RAY_DPRINTF(("ray%d: PCCard unload\n", dev_p->isahd.id_unit));

    sc = &ray_softc[dev_p->isahd.id_unit];

    if (sc->gone) {
	printf("ray%d: already unloaded\n", sc->unit);
	return;
    }

    /* Cleardown interface */
    ifp = &sc->arpcom.ac_if;
    ifp->if_flags &= ~(IFF_RUNNING|IFF_OACTIVE);
    if_down(ifp); /* XXX probably should be if_detach but I don't know if it works in 3.1 */

    /* Mark card as gone */
    sc->gone = 1;
    printf("ray%d: unloaded\n", sc->unit);

    return;
}

/*
 * PCCard interrupt.
 */
/* XXX return 1 if we take interrupt, 0 otherwise */
static int
ray_pccard_intr (dev_p)
    struct pccard_devinfo	*dev_p;
{
    struct ray_softc		*sc;
    int				ccsi, handled;

    RAY_DPRINTF(("ray%d: PCCard intr\n", dev_p->isahd.id_unit));

    sc = &ray_softc[dev_p->isahd.id_unit];
    RAY_MAP_CM(sc);

    if (sc->gone) {
	printf("ray%d: unloaded before interrupt!\n", sc->unit);
	return(0);
    }

    /*
     * Check that the interrupt was for us, if so get the rcs/ccs and vector
     * on the command contained within it.
     */
    if (!RAY_HCS_INTR(sc)) {

	handled = 0;

    } else {

	handled = 1;
	ccsi = SRAM_READ_1(sc, RAY_SCB_RCSI);
	if (ccsi <= RAY_CCS_LAST)
	    ray_ccs_done(sc, RAY_CCS_ADDRESS(ccsi));
	else if (ccsi <= RAY_RCS_LAST)
	    ray_rcs_intr(sc, RAY_CCS_ADDRESS(ccsi));
	else
	    printf("ray%d: ray_intr bad ccs index %d\n", sc->unit, ccsi);

#if XXX
	ccs_done and rcs_intr return function pointers - why dont
	they just do it themselves? its not as if each command only
	requires a single function call - things like start_join_net
	call a couple on the way...
	if (rcmd)
	    (*rcmd)(sc);
#endif

    }

    if (handled)
	RAY_HCS_CLEAR_INTR(sc);

    RAY_DPRINTF(("ray%d: interrupt %s handled\n",
    		sc->unit, handled?"was":"not"));

    return(handled);
}

/*
 * ISA probe routine.
 */
static int
ray_probe (dev_p)
    struct isa_device		*dev_p;
{

    RAY_DPRINTF(("ray%d: ISA probe\n", dev_p->id_unit));

    return(0);
}

/*
 * ISA/PCCard attach.
 */
static int
ray_attach (dev_p)
    struct isa_device		*dev_p;
{
    struct ray_softc		*sc;
    struct ray_ecf_startup_v5	*ep;
    struct ifnet		*ifp;
    char			ifname[IFNAMSIZ];

    RAY_DPRINTF(("ray%d: ISA/PCCard attach\n", dev_p->id_unit));

    sc = &ray_softc[dev_p->id_unit];
    RAY_MAP_CM(sc);

    if (sc->gone) {
	printf("ray%d: unloaded before attach!\n", sc->unit);
	return(0);
    }

    /*
     * Read startup results, check the card is okay and work out what
     * version we are using.
     */
    ep = &sc->sc_ecf_startup;
    ray_read_region(sc, RAY_ECF_TO_HOST_BASE, ep, sizeof(sc->sc_ecf_startup));
    if (ep->e_status != RAY_ECFS_CARD_OK) {
	printf("ray%d: card failed self test: status 0x%b\n", sc->unit,
	    ep->e_status,
	    "\020"			/* print in hex */
	    "\001RESERVED0"
	    "\002PROC_SELF_TEST"
	    "\003PROG_MEM_CHECKSUM"
	    "\004DATA_MEM_TEST"
	    "\005RX_CALIBRATION"
	    "\006FW_VERSION_COMPAT"
	    "\007RERSERVED1"
	    "\008TEST_COMPLETE"
	);
	return(0);
    }
    if (sc->sc_version != RAY_ECFS_BUILD_4 &&
        sc->sc_version != RAY_ECFS_BUILD_5
       ) {
	printf("ray%d: unsupported firmware version 0x%0x\n", sc->unit,
	    ep->e_fw_build_string);
	return(0);
    }

    if (bootverbose || RAY_DEBUG) {
	printf("ray%d: Start Up Results\n", sc->unit);
	if (RAY_DEBUG > 10)
	    RAY_DHEX8((u_int8_t *)sc->maddr + RAY_ECF_TO_HOST_BASE, 0x40);
	if (sc->sc_version == RAY_ECFS_BUILD_4)
	    printf("  Firmware version 4\n");
	else
	    printf("  Firmware version 5\n");
	printf("  Status 0x%x\n", ep->e_status);
	printf("  Ether address %6D\n", ep->e_station_addr, ":");
	if (sc->sc_version == RAY_ECFS_BUILD_4) {
	    printf("  Program checksum %0x\n", ep->e_resv0);
	    printf("  CIS checksum %0x\n", ep->e_rates[0]);
	} else {
	    printf("  (reserved word) %0x\n", ep->e_resv0);
	    printf("  Supported rates %8D\n", ep->e_rates, ":");
	}
	printf("  Japan call sign %12D\n", ep->e_japan_callsign, ":");
	if (sc->sc_version == RAY_ECFS_BUILD_5) {
	    printf("  Program checksum %0x\n", ep->e_prg_cksum);
	    printf("  CIS checksum %0x\n", ep->e_cis_cksum);
	    printf("  Firmware version %0x\n", ep->e_fw_build_string);
	    printf("  Firmware revision %0x\n", ep->e_fw_build);
	    printf("  (reserved word) %0x\n", ep->e_fw_resv);
	    printf("  ASIC version %0x\n", ep->e_asic_version);
	    printf("  TIB size %0x\n", ep->e_tibsize);
	}
    }

    /* Reset any pending interrupts */
    RAY_HCS_CLEAR_INTR(sc);

    /*
     * Set the parameters that will survive stop/init
     */
#if XXX
NetBSD
    bzero(sc->sc_cnwid, sizeof(sc->sc_cnwid));
    bzero(sc->sc_dnwid, sizeof(sc->sc_dnwid));
    strncpy(sc->sc_dnwid, RAY_DEF_NWID, sizeof(sc->sc_dnwid));
    strncpy(sc->sc_cnwid, RAY_DEF_NWID, sizeof(sc->sc_dnwid));
    sc->sc_omode = sc->sc_mode = RAY_MODE_DEFAULT;
    sc->sc_countrycode = sc->sc_dcountrycode = RAY_PID_COUNTRY_CODE_DEFAULT;
#endif

    /*
     * Initialise the network interface structure
     */
    bcopy((char *)&ep->e_station_addr,
	  (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);
    ifp = &sc->arpcom.ac_if;
    ifp->if_softc = sc;
    ifp->if_name = "ray";
    ifp->if_unit = sc->unit;
    ifp->if_timer = 0;
    ifp->if_flags = (IFF_BROADCAST | IFF_SIMPLEX ); /* XXX - IFF_MULTICAST */
#if XXX
    ifp->if_hdr = ...; make this big enough to hold the .11 and .3 headers
#endif
    ifp->if_baudrate = 1000000; /* XXX Is this baud or bps ;-) */

    ifp->if_output = ether_output;
    ifp->if_start = ray_start;
    ifp->if_ioctl = ray_ioctl;
    ifp->if_watchdog = ray_watchdog;
    ifp->if_init = ray_init;
    ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;

    /*
     * If this logical interface has already been attached,
     * don't attach it again or chaos will ensue.
     */
    sprintf(ifname, "ray%d", sc->unit);

    if (ifunit(ifname) == NULL) {
	callout_handle_init(&sc->timerh);
#ifdef RAY_NEED_STARTJOIN_TIMEO
	callout_handle_init(&sc->sj_timerh);
#endif /* RAY_NEED_STARTJOIN_TIMEO */
	if_attach(ifp);
	ether_ifattach(ifp);
#if NBPFILTER > 0
	bpfattach(ifp, DLT_EN10MB, sizeof(struct ether_header));
#endif /* NBFFILTER */

#if XXX
	this looks like a good idea
	at_shutdown(ray_shutdown, sc, SHUTDOWN_POST_SYNC);
#endif /* XXX */
    }

    return(0);
}

/*
 * Network start.
 *
 * Start output on interface.  We make two assumptions here:
 *  1) that the current priority is set to splimp _before_ this code
 *     is called *and* is returned to the appropriate priority after
 *     return
 *  2) that the IFF_OACTIVE flag is checked before this code is called
 *     (i.e. that the output part of the interface is idle)
 */
static void
ray_start (ifp)
    register struct ifnet	*ifp;
{
    struct ray_softc *sc;

    RAY_DPRINTF(("ray%d: Network start\n", ifp->if_unit));

    sc = ifp->if_softc;
    RAY_MAP_CM(sc);

    if (sc->gone) {
	printf("ray%d: unloaded before start!\n", sc->unit);
	return;
    }

/* XXX mark output queue full so the kernel waits */
ifp->if_flags |= IFF_OACTIVE;

/* XXX if_xe code is clean but if_ed does more checks at top */
 
    return;
}

/*
 * Network ioctl request.
 */
static int
ray_ioctl (ifp, command, data)
    register struct ifnet	*ifp;
    u_long			command;
    caddr_t			data;
{
    struct ray_softc *sc;
    int s, error = 0;

    RAY_DPRINTF(("ray%d: Network ioctl\n", ifp->if_unit));

    sc = ifp->if_softc;
    RAY_MAP_CM(sc);

    if (sc->gone) {
	printf("ray%d: unloaded before ioctl!\n", sc->unit);
	ifp->if_flags &= ~IFF_RUNNING;
	return ENXIO;
    }

    s = splimp();

    switch (command) {

	case SIOCSIFADDR:
	case SIOCGIFADDR:
	case SIOCSIFMTU:
	    RAY_DPRINTF(("ray%d: ioctl SIFADDR/GIFADDR/SIFMTU\n", sc->unit));
	    error = ether_ioctl(ifp, command, data);
	    break;

	case SIOCSIFFLAGS:
	    RAY_DPRINTF(("ray%d: for SIFFLAGS\n", sc->unit));
	    /*
	     * If the interface is marked up and stopped, then start
	     * it. If it is marked down and running, then stop it.
	     */
	    if (ifp->if_flags & IFF_UP) {
		if (!(ifp->if_flags & IFF_RUNNING))
		    ray_init(sc);
	    } else {
		if (ifp->if_flags & IFF_RUNNING)
		    ray_stop(sc);
	    }
	    /* DROP THROUGH */

#if XXX
	case SIOCADDMULTI:
	case SIOCDELMULTI:
	    RAY_DPRINTF(("ray%d: ioctl called for ADDMULTI/DELMULTI\n, sc->unit"));
	    /*
	     * Multicast list has (maybe) changed; set the hardware filter
	     * accordingly. This also serves to deal with promiscuous mode
	     * if we have a BPF listener active.
	     */
	    ray_setmulti(sc);
#endif /* XXX */

	    error = 0;
	    break;

case SIOCGIFFLAGS:
    RAY_DPRINTF(("ray%d: ioctl called for GIFFLAGS\n", sc->unit));
    error = EINVAL;
    break;
case SIOCGIFMETRIC:
    RAY_DPRINTF(("ray%d: ioctl called for GIFMETRIC\n", sc->unit));
    error = EINVAL;
    break;
case SIOCGIFMTU:
    RAY_DPRINTF(("ray%d: ioctl called for GIFMTU\n", sc->unit));
    error = EINVAL;
    break;
case SIOCGIFPHYS:
    RAY_DPRINTF(("ray%d: ioctl called for GIFPYHS\n", sc->unit));
    error = EINVAL;
    break;
case SIOCSIFMEDIA:
    RAY_DPRINTF(("ray%d: ioctl called for SIFMEDIA\n", sc->unit));
    error = EINVAL;
    break;
case SIOCGIFMEDIA:
    RAY_DPRINTF(("ray%d: ioctl called for GIFMEDIA\n", sc->unit));
    error = EINVAL;
    break;

	default:
	    error = EINVAL;
    }

    (void)splx(s);

    /* XXX This is here to avoid spl's */
if (command == SIOCGIFMEDIA) {
    RAY_DPRINTF(("ray%d: RAY_SCB\n", sc->unit));
    RAY_DHEX8((u_int8_t *)sc->maddr + RAY_SCB_BASE, 0x20);
    RAY_DPRINTF(("ray%d: RAY_STATUS\n", sc->unit));
    RAY_DHEX8((u_int8_t *)sc->maddr + RAY_STATUS_BASE, 0x20);
    RAY_DPRINTF(("ray%d: RAY_ECF_TO_HOST\n", sc->unit));
    RAY_DHEX8((u_int8_t *)sc->maddr + RAY_ECF_TO_HOST_BASE, 0x40);
    RAY_DPRINTF(("ray%d: RAY_HOST_TO_ECF\n", sc->unit));
    RAY_DHEX8((u_int8_t *)sc->maddr + RAY_HOST_TO_ECF_BASE, 0x50);
}

    return error;
}

static void
ray_watchdog (ifp)
    register struct ifnet	*ifp;
{
    struct ray_softc *sc;

    RAY_DPRINTF(("ray%d: Network watchdog\n", ifp->if_unit));

    sc = ifp->if_softc;
    RAY_MAP_CM(sc);

    if (sc->gone) {
	printf("ray%d: unloaded before watchdog!\n", sc->unit);
	return;
    }

    printf("ray%d: watchdog timeout\n", sc->unit);

/* XXX may need to have remedial action here
   for example
   	ray_reset - may be useful elsewhere
		ray_stop
		...
		ray_init
*/

    return;
}

/*
 * Network initialisation.
 */
static void
ray_init (xsc)
    void			*xsc;
{
    struct ray_softc		*sc = xsc;
    struct ray_ecf_startup_v5	*ep;
    struct ifnet		*ifp;
    size_t			ccs;
    int				i;

    RAY_DPRINTF(("ray%d: Network init\n", sc->unit));
    RAY_MAP_CM(sc);

    if (sc->gone) {
	printf("ray%d: unloaded before init!\n", sc->unit);
	return;
    }

#if XXX
NetBSD
    if ((sc->sc_if.if_flags & IFF_RUNNING))
	ray_stop(sc);
#endif

    ifp = &sc->arpcom.ac_if;

    /*
     * Reset instance variables
     */
#if XXX
NetBSD
    sc->sc_havenet = 0;
    bzero(sc->sc_bssid, sizeof(sc->sc_bssid));
    sc->sc_deftxrate = 0;
    sc->sc_encrypt = 0;
    sc->sc_promisc = 0;
    sc->sc_scheduled = 0;
    sc->sc_running = 0;
    sc->sc_txfree = RAY_CCS_NTX;
    sc->sc_checkcounters = 0;
#endif

    /* Set all ccs to be free */
    bzero(sc->sc_ccsinuse, sizeof(sc->sc_ccsinuse));
    sc->sc_ccs = RAY_CCS_LAST + 1;
    ccs = RAY_CCS_ADDRESS(0);
    for (i = 0; i < RAY_CCS_LAST; ccs += RAY_CCS_SIZE, i++)
	    SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd, c_status, RAY_CCS_STATUS_FREE);

    /* Clear any pending interrupts */
    RAY_HCS_CLEAR_INTR(sc);

    /*
     * Get startup results - the card may have been reset
     */
    ep = &sc->sc_ecf_startup;
    ray_read_region(sc, RAY_ECF_TO_HOST_BASE, ep, sizeof(sc->sc_ecf_startup));
    if (ep->e_status != RAY_ECFS_CARD_OK) {
	printf("ray%d: card failed self test: status 0x%b\n", sc->unit,
	    ep->e_status,
	    "\020"			/* print in hex */
	    "\001RESERVED0"
	    "\002PROC_SELF_TEST"
	    "\003PROG_MEM_CHECKSUM"
	    "\004DATA_MEM_TEST"
	    "\005RX_CALIBRATION"
	    "\006FW_VERSION_COMPAT"
	    "\007RERSERVED1"
	    "\008TEST_COMPLETE"
	);
#if XXX
	return; /* XXX This doesn't mark the interface as down */
#endif /* XXX */
    }

    /*
     * Fixup tib size to be correct - on build 4 it is garbage
     */
    if (sc->sc_version == RAY_ECFS_BUILD_4 && sc->sc_tibsize == 0x55)
	sc->sc_tibsize = 32;

    /*
     * We are now up and running. Next we have to download network
     * configuration into the card. We are busy until download is done.
     */
    ifp->if_flags |= IFF_RUNNING | IFF_OACTIVE;
#if XXX
    /* set this now so it gets set in the download */
    sc->sc_promisc = !!(sc->sc_if.if_flags & (IFF_PROMISC|IFF_ALLMULTI));
#endif /* XXX */
    ray_download_params(sc);

#if XXX
    need to understand how the doenload finishes first

    Start up flow is as follows.

    	The kernel calls ray_init when the interface is assigned an address.

	ray_init does a bit of house keeping before calling ray_download_params.

	ray_download_params fills the startup parameter structure out and
	sends it to the card. The download command simply completes so
	we use schedule a timeout function call to ray_download_timo.
	We pass the ccs in use via sc->sc_css.

	ray_download_timo checks the ccs for command completion/errors.
	Then it tells the card to start an adhoc or join a managed
	network. This should complete via the interrupt mechanism, but
	the NetBSD driver includes a timeout for some buggy stuff somewhere.
	I've left the hooks in but don't use them. The interrupt handler
	passes control to ray_start_join_done (again the ccs is in
	sc->sc_css XXX need to see if this is actually needed).

	XXX
    /*
     * Set running and clear output active, then attempt to start output
     */
    ifp->if_flags |= IFF_RUNNING;
/* XXX spl's needed higher up? but this is called by ioctl only?*/
    s = splimp();
    ray_start(ifp);
    (void) splx(s);
#endif

    return;
}

/*
 * Network stop.
 */
static void
ray_stop (sc)
    struct ray_softc	*sc;
{
    struct ifnet	*ifp;

    RAY_DPRINTF(("ray%d: Network stop\n", sc->unit));
    RAY_MAP_CM(sc);

    if (sc->gone) {
	printf("ray%d: unloaded before stop!\n", sc->unit);
	return;
    }

    ifp = &sc->arpcom.ac_if;

/* XXX stuff here please to kill activity on the card and drain down transmissons */

    /* Mark as not running */
    ifp->if_flags &= ~IFF_RUNNING;

    return;
}

/*
 * Process CCS command completion - called from ray_intr
 */
static void
ray_ccs_done (sc, ccs)
    struct ray_softc	*sc;
    size_t		ccs;
{
    struct ifnet	*ifp;
    u_int		cmd, status;
    
    RAY_DPRINTF(("ray%d: Processing ccs %d\n", sc->unit, RAY_CCS_INDEX(ccs)));
    RAY_MAP_CM(sc);

    ifp = &sc->arpcom.ac_if;

    cmd = SRAM_READ_FIELD_1(sc, ccs, ray_cmd, c_cmd);
    status = SRAM_READ_FIELD_1(sc, ccs, ray_cmd, c_status);
    RAY_DPRINTF(("ray%d: ccs idx %d ccs 0x%x cmd 0x%x stat %d\n",
    		sc->unit, RAY_CCS_INDEX(ccs), ccs, cmd, status));

    /* XXX should we panic on unrecognised commands or just ignore them?
     * maybe I'll macroize the printf's */
    switch (cmd) {
	case RAY_CMD_START_PARAMS:
	    printf("ray%d: ray_ccs_done got START_PARAMS - why?\n", sc->unit);
	    break;

	case RAY_CMD_UPDATE_PARAMS:
	    RAY_DPRINTF(("ray%d: ray_ccs_done got UPDATE_PARAMS\n", sc->unit));
	    XXX;
	    break;

	case RAY_CMD_REPORT_PARAMS:
	    RAY_DPRINTF(("ray%d: ray_ccs_done got REPORT_PARAMS\n", sc->unit));
	    XXX;
	    break;

	case RAY_CMD_UPDATE_MCAST:
	    RAY_DPRINTF(("ray%d: ray_ccs_done got UPDATE_MCAST\n", sc->unit));
	    XXX;
	    break;

	case RAY_CMD_UPDATE_APM:
	    RAY_DPRINTF(("ray%d: ray_ccs_done got UPDATE_APM\n", sc->unit));
	    XXX;
	    break;

	case RAY_CMD_START_NET:
	case RAY_CMD_JOIN_NET:
	    RAY_DPRINTF(("ray%d: ray_ccs_done got START|JOIN_NET\n", sc->unit));
#ifdef RAY_NEED_STARTJOIN_TIMEO
	    untimeout(ray_start_join_timo, sc, sc->sj_timerh);
#endif /* RAY_NEED_STARTJOIN_TIMEO */
	    XXX;
	    break;

	case RAY_CMD_START_ASSOC:
	    RAY_DPRINTF(("ray%d: ray_ccs_done got START_ASSOC\n", sc->unit));
	    XXX;
	    break;

	case RAY_CMD_TX_REQ:
	    RAY_DPRINTF(("ray%d: ray_ccs_done got TX_REQ\n", sc->unit));
	    XXX;
	    break;

	case RAY_CMD_TEST_MEM:
	    printf("ray%d: ray_ccs_done got TEST_MEM - why?\n", sc->unit);
	    break;

	case RAY_CMD_SHUTDOWN:
	    printf("ray%d: ray_ccs_done got SHUTDOWN - why?\n", sc->unit);
	    break;

	case RAY_CMD_DUMP_MEM:
	    printf("ray%d: ray_ccs_done got DUMP_MEM - why?\n", sc->unit);
	    break;

	case RAY_CMD_START_TIMER:
	    printf("ray%d: ray_ccs_done got START_TIMER - why?\n", sc->unit);
    	    break;

	default:
	    printf("ray%d: ray_ccs_done unknown command 0x%x\n", sc->unit, cmd);
	    break;
    }

    ray_free_ccs(sc, ccs);

    return;
}

/*
 * Process ECF command request - called from ray_intr
 */
static void
ray_rcs_intr (sc, rcs)
    struct ray_softc	*sc;
    size_t		rcs;
{
    struct ifnet	*ifp;
    u_int		cmd, status;
    
    RAY_DPRINTF(("ray%d: Processing rcs %d\n", sc->unit, RAY_CCS_INDEX(rcs)));
    RAY_MAP_CM(sc);

    ifp = &sc->arpcom.ac_if;

    cmd = SRAM_READ_FIELD_1(sc, rcs, ray_cmd, c_cmd);
    status = SRAM_READ_FIELD_1(sc, rcs, ray_cmd, c_status);
    RAY_DPRINTF(("ray%d: rcs idx %d rcs 0x%x cmd 0x%x stat %d\n",
    		sc->unit, RAY_CCS_INDEX(rcs), rcs, cmd, status));

    /* XXX should we panic on unrecognised commands or just ignore them?
     * maybe I'll macroize the printf's */
    switch (cmd) {
	case RAY_ECMD_RX_DONE:
	    printf("ray%d: ray_rcs_intr got RX_DONE\n", sc->unit);
	    XXX;
	    break;

	case RAY_ECMD_REJOIN_DONE:
	    RAY_DPRINTF(("ray%d: ray_rcs_intr got UPDATE_PARAMS\n", sc->unit));
	    XXX;
	    break;

	case RAY_ECMD_ROAM_START:
	    RAY_DPRINTF(("ray%d: ray_rcs_intr got ROAM_START\n", sc->unit));
	    XXX;
	    break;

	case RAY_ECMD_JAPAN_CALL_SIGNAL:
	    printf("ray%d: ray_rcs_intr got JAPAN_CALL_SIGNAL - why?\n",
	    		sc->unit);
	    break;

	default:
	    printf("ray%d: ray_rcs_intr unknown command 0x%x\n",
	    		sc->unit, cmd);
	    break;
    }

    SRAM_WRITE_FIELD_1(sc, rcs, ray_cmd, c_status, RAY_CCS_STATUS_FREE);

    return;
}

/*
 * Download start up structures to card.
 *
 * Part of ray_init, download, startjoin control flow.
 */
static void
ray_download_params (sc)
    struct ray_softc	*sc;
{
    struct ray_mib_4	ray_mib_4_default;
    struct ray_mib_5	ray_mib_5_default;

    RAY_DPRINTF(("ray%d: Downloading startup parameters\n", sc->unit));
    RAY_MAP_CM(sc);

#if XXX
netbsd
    ray_cmd_cancel(sc, SCP_UPD_STARTUP);
#endif /* XXX */

#define MIB4(m)		ray_mib_4_default.##m
#define MIB5(m)		ray_mib_5_default.##m
#define	PUT2(p, v) 	\
    do { (p)[0] = ((v >> 8) & 0xff); (p)[1] = (v & 0xff); } while(0)

     /*
      * Firmware version 4 defaults - see if_raymib.h for details
      */
     MIB4(mib_net_type)			= RAY_MIB_NET_TYPE_DEFAULT;
     MIB4(mib_ap_status)		= RAY_MIB_AP_STATUS_DEFAULT;
strncpy(MIB4(mib_ssid), 		  RAY_MIB_SSID_DEFAULT, RAY_MAXSSIDLEN);
     MIB4(mib_scan_mode)		= RAY_MIB_SCAN_MODE_DEFAULT;
     MIB4(mib_apm_mode)			= RAY_MIB_APM_MODE_DEFAULT;
     bcopy(sc->sc_station_addr, MIB4(mib_mac_addr), ETHER_ADDR_LEN);
PUT2(MIB4(mib_frag_thresh), 		  RAY_MIB_FRAG_THRESH_DEFAULT);
PUT2(MIB4(mib_dwell_time),		  RAY_MIB_DWELL_TIME_V4);
PUT2(MIB4(mib_beacon_period),		  RAY_MIB_BEACON_PERIOD_V4);
     MIB4(mib_dtim_interval)		= RAY_MIB_DTIM_INTERVAL_DEFAULT;
     MIB4(mib_max_retry)		= RAY_MIB_MAX_RETRY_DEFAULT;
     MIB4(mib_ack_timo)			= RAY_MIB_ACK_TIMO_DEFAULT;
     MIB4(mib_sifs)			= RAY_MIB_SIFS_DEFAULT;
     MIB4(mib_difs)			= RAY_MIB_DIFS_DEFAULT;
     MIB4(mib_pifs)			= RAY_MIB_PIFS_V4;
PUT2(MIB4(mib_rts_thresh),		  RAY_MIB_RTS_THRESH_DEFAULT);
PUT2(MIB4(mib_scan_dwell),		  RAY_MIB_SCAN_DWELL_V4);
PUT2(MIB4(mib_scan_max_dwell),		  RAY_MIB_SCAN_MAX_DWELL_V4);
     MIB4(mib_assoc_timo)		= RAY_MIB_ASSOC_TIMO_DEFAULT;
     MIB4(mib_adhoc_scan_cycle)		= RAY_MIB_ADHOC_SCAN_CYCLE_DEFAULT;
     MIB4(mib_infra_scan_cycle)		= RAY_MIB_INFRA_SCAN_CYCLE_DEFAULT;
     MIB4(mib_infra_super_scan_cycle)	= RAY_MIB_INFRA_SUPER_SCAN_CYCLE_DEFAULT;
     MIB4(mib_promisc)			= RAY_MIB_PROMISC_DEFAULT;
PUT2(MIB4(mib_uniq_word),		  RAY_MIB_UNIQ_WORD_DEFAULT);
     MIB4(mib_slot_time)		= RAY_MIB_SLOT_TIME_V4;
     MIB4(mib_roam_low_snr_thresh)	= RAY_MIB_ROAM_LOW_SNR_THRESH_DEFAULT;
     MIB4(mib_low_snr_count)		= RAY_MIB_LOW_SNR_COUNT_DEFAULT;
     MIB4(mib_infra_missed_beacon_count)= RAY_MIB_INFRA_MISSED_BEACON_COUNT_DEFAULT;
     MIB4(mib_adhoc_missed_beacon_count)= RAY_MIB_ADHOC_MISSED_BEACON_COUNT_DEFAULT;
     MIB4(mib_country_code)		= RAY_MIB_COUNTRY_CODE_DEFAULT;
     MIB4(mib_hop_seq)			= RAY_MIB_HOP_SEQ_DEFAULT;
     MIB4(mib_hop_seq_len)		= RAY_MIB_HOP_SEQ_LEN_V4;
     MIB4(mib_cw_max)			= RAY_MIB_CW_MAX_V4;
     MIB4(mib_cw_min)			= RAY_MIB_CW_MIN_V4;
     MIB4(mib_noise_filter_gain)	= RAY_MIB_NOISE_FILTER_GAIN_DEFAULT;
     MIB4(mib_noise_limit_offset)	= RAY_MIB_NOISE_LIMIT_OFFSET_DEFAULT;
     MIB4(mib_rssi_thresh_offset)	= RAY_MIB_RSSI_THRESH_OFFSET_DEFAULT;
     MIB4(mib_busy_thresh_offset)	= RAY_MIB_BUSY_THRESH_OFFSET_DEFAULT;
     MIB4(mib_sync_thresh)		= RAY_MIB_SYNC_THRESH_DEFAULT;
     MIB4(mib_test_mode)		= RAY_MIB_TEST_MODE_DEFAULT;
     MIB4(mib_test_min_chan)		= RAY_MIB_TEST_MIN_CHAN_DEFAULT;
     MIB4(mib_test_max_chan)		= RAY_MIB_TEST_MAX_CHAN_DEFAULT;

     /*
      * Firmware version 5 defaults - see if_raymib.h for details
      */
     MIB5(mib_net_type)			= RAY_MIB_NET_TYPE_DEFAULT;
     MIB5(mib_ap_status)		= RAY_MIB_AP_STATUS_DEFAULT;
strncpy(MIB5(mib_ssid), 		  RAY_MIB_SSID_DEFAULT, RAY_MAXSSIDLEN);
     MIB5(mib_scan_mode)		= RAY_MIB_SCAN_MODE_DEFAULT;
     MIB5(mib_apm_mode)			= RAY_MIB_APM_MODE_DEFAULT;
     bcopy(sc->sc_station_addr, MIB5(mib_mac_addr), ETHER_ADDR_LEN);
PUT2(MIB5(mib_frag_thresh), 		  RAY_MIB_FRAG_THRESH_DEFAULT);
PUT2(MIB5(mib_dwell_time),		  RAY_MIB_DWELL_TIME_V5);
PUT2(MIB5(mib_beacon_period),		  RAY_MIB_BEACON_PERIOD_V5);
     MIB5(mib_dtim_interval)		= RAY_MIB_DTIM_INTERVAL_DEFAULT;
     MIB5(mib_max_retry)		= RAY_MIB_MAX_RETRY_DEFAULT;
     MIB5(mib_ack_timo)			= RAY_MIB_ACK_TIMO_DEFAULT;
     MIB5(mib_sifs)			= RAY_MIB_SIFS_DEFAULT;
     MIB5(mib_difs)			= RAY_MIB_DIFS_DEFAULT;
     MIB5(mib_pifs)			= RAY_MIB_PIFS_V5;
PUT2(MIB5(mib_rts_thresh),		  RAY_MIB_RTS_THRESH_DEFAULT);
PUT2(MIB5(mib_scan_dwell),		  RAY_MIB_SCAN_DWELL_V5);
PUT2(MIB5(mib_scan_max_dwell),		  RAY_MIB_SCAN_MAX_DWELL_V5);
     MIB5(mib_assoc_timo)		= RAY_MIB_ASSOC_TIMO_DEFAULT;
     MIB5(mib_adhoc_scan_cycle)		= RAY_MIB_ADHOC_SCAN_CYCLE_DEFAULT;
     MIB5(mib_infra_scan_cycle)		= RAY_MIB_INFRA_SCAN_CYCLE_DEFAULT;
     MIB5(mib_infra_super_scan_cycle)	= RAY_MIB_INFRA_SUPER_SCAN_CYCLE_DEFAULT;
     MIB5(mib_promisc)			= RAY_MIB_PROMISC_DEFAULT;
PUT2(MIB5(mib_uniq_word),		  RAY_MIB_UNIQ_WORD_DEFAULT);
     MIB5(mib_slot_time)		= RAY_MIB_SLOT_TIME_V5;
     MIB5(mib_roam_low_snr_thresh)	= RAY_MIB_ROAM_LOW_SNR_THRESH_DEFAULT;
     MIB5(mib_low_snr_count)		= RAY_MIB_LOW_SNR_COUNT_DEFAULT;
     MIB5(mib_infra_missed_beacon_count)= RAY_MIB_INFRA_MISSED_BEACON_COUNT_DEFAULT;
     MIB5(mib_adhoc_missed_beacon_count)= RAY_MIB_ADHOC_MISSED_BEACON_COUNT_DEFAULT;
     MIB5(mib_country_code)		= RAY_MIB_COUNTRY_CODE_DEFAULT;
     MIB5(mib_hop_seq)			= RAY_MIB_HOP_SEQ_DEFAULT;
     MIB5(mib_hop_seq_len)		= RAY_MIB_HOP_SEQ_LEN_V5;
PUT2(MIB5(mib_cw_max),			  RAY_MIB_CW_MAX_V5);
PUT2(MIB5(mib_cw_min),			  RAY_MIB_CW_MIN_V5);
     MIB5(mib_noise_filter_gain)	= RAY_MIB_NOISE_FILTER_GAIN_DEFAULT;
     MIB5(mib_noise_limit_offset)	= RAY_MIB_NOISE_LIMIT_OFFSET_DEFAULT;
     MIB5(mib_rssi_thresh_offset)	= RAY_MIB_RSSI_THRESH_OFFSET_DEFAULT;
     MIB5(mib_busy_thresh_offset)	= RAY_MIB_BUSY_THRESH_OFFSET_DEFAULT;
     MIB5(mib_sync_thresh)		= RAY_MIB_SYNC_THRESH_DEFAULT;
     MIB5(mib_test_mode)		= RAY_MIB_TEST_MODE_DEFAULT;
     MIB5(mib_test_min_chan)		= RAY_MIB_TEST_MIN_CHAN_DEFAULT;
     MIB5(mib_test_max_chan)		= RAY_MIB_TEST_MAX_CHAN_DEFAULT;
     MIB5(mib_allow_probe_resp)		= RAY_MIB_ALLOW_PROBE_RESP_DEFAULT;
     MIB5(mib_privacy_must_start)	= RAY_MIB_PRIVACY_MUST_START_DEFAULT;
     MIB5(mib_privacy_can_join)		= RAY_MIB_PRIVACY_CAN_JOIN_DEFAULT;
     MIB5(mib_basic_rate_set[0])	= RAY_MIB_BASIC_RATE_SET_DEFAULT;

    if (!RAY_ECF_READY(sc))
    	panic("ray%d: ray_download_params something is already happening\n",
		sc->unit);

    if (sc->sc_version == RAY_ECFS_BUILD_4)
	ray_write_region(sc, RAY_HOST_TO_ECF_BASE,
			 &ray_mib_4_default, sizeof(ray_mib_4_default));
    else
	ray_write_region(sc, RAY_HOST_TO_ECF_BASE,
			 &ray_mib_5_default, sizeof(ray_mib_5_default));

/*
 * NetBSD
 * hand expanding ray_simple_cmd
 * we dont do any of the clever timeout stuff yet (i.e. ray_cmd_ran) just
 * simple check
 *
 * 	if (!ray_simple_cmd(sc, RAY_CMD_START_PARAMS, SCP_UPD_STARTUP))
 * 	    panic("ray_download_params issue");
 *
 * 	ray_simple_cmd ==
 * 	    ray_alloc_ccs(sc, &ccs, cmd, track) &&
 * 	    ray_issue_cmd(sc, ccs, track));
 *
 */
    /*
     * Get a free command ccs and issue the command - there is nothing
     * to fill in for a START_PARAMS command. The start parameters
     * command just gets serviced, so we use a timeout to complete the
     * sequence.
     */
    if (!ray_alloc_ccs(sc, &sc->sc_ccs, RAY_CMD_START_PARAMS, SCP_UPD_STARTUP))
    	panic("ray%d: ray_download_params can't get a CCS\n", sc->unit);

    if (!ray_issue_cmd(sc, sc->sc_ccs, SCP_UPD_STARTUP))
    	panic("ray%d: ray_download_params can't issue command\n", sc->unit);

    sc->timerh = timeout(ray_download_timo, sc, RAY_CCS_TIMEOUT);

    RAY_DPRINTF(("ray%d: Download now awaiting timeout\n", sc->unit));

    return;
}

/*
 * Download timeout routine.
 *
 * Part of ray_init, download, start_join control flow.
 */
static void
ray_download_timo (xsc)
    void		*xsc;
{
    struct ray_softc	*sc = xsc;
    u_int8_t		status, cmd;

    RAY_DPRINTF(("ray%d: ray_download_timo\n", sc->unit));
    RAY_MAP_CM(sc);

    status = SRAM_READ_FIELD_1(sc, sc->sc_ccs, ray_cmd, c_status);
    cmd = SRAM_READ_FIELD_1(sc, sc->sc_ccs, ray_cmd, c_cmd);
    RAY_DPRINTF(("ray%d: check rayidx %d ccs 0x%x cmd 0x%x stat %d\n",
    		sc->unit, RAY_CCS_INDEX(sc->sc_ccs), sc->sc_ccs, cmd, status));
    if ((cmd != RAY_CMD_START_PARAMS) || (status != RAY_CCS_STATUS_FREE))
    	printf("ray%d: Download ccs odd cmd = 0x%02x, status = 0x%02x",
		sc->unit, cmd, status);
	/*XXX so what do we do? reset or retry? */

    /*
     * If the card is still busy, re-schedule ourself
     */
    if (status == RAY_CCS_STATUS_BUSY) {
	RAY_DPRINTF(("ray%d: ray_download_timo - still busy, see you soon\n",
		sc->unit));
	sc->timerh = timeout(ray_download_timo, sc, RAY_CCS_TIMEOUT);
    }

    /* Clear the ccs */
    ray_free_ccs(sc, sc->sc_ccs);
    sc->sc_ccs = RAY_CCS_LAST + 1;

    /*
     * Grab a ccs and don't bother updating the network parameters.
     * Issue the start/join command and we get interrupted back.
     */
    if (sc->sc_mode == SC_MODE_ADHOC)
	    cmd = RAY_CMD_START_NET;
    else
	    cmd = RAY_CMD_JOIN_NET;

    if (!ray_alloc_ccs(sc, &sc->sc_ccs, cmd, SCP_UPD_STARTJOIN))
    	panic("ray%d: ray_download_timo can't get a CCS to start/join net\n",
		sc->unit);

    SRAM_WRITE_FIELD_1(sc, sc->sc_ccs, ray_cmd_net, c_upd_param, 0);

    if (!ray_issue_cmd(sc, sc->sc_ccs, SCP_UPD_STARTJOIN))
    	panic("ray%d: ray_download_timo can't issue start/join\n", sc->unit);

#ifdef RAY_NEED_STARTJOIN_TIMEO
    sc->sj_timerh = timeout(ray_start_join_timo, sc, RAY_CCS_TIMEOUT);
#endif /* RAY_NEED_STARTJOIN_TIMEO */

    RAY_DPRINTF(("ray%d: Start-join awaiting interrupt/timeout\n", sc->unit));

    return;
}

#ifdef RAY_NEED_STARTJOIN_TIMEO
/*
 * Back stop catcher for start_join command. The NetBSD driver
 * suggests that they need it to catch a bug in the firmware or the
 * parameters they use - they are not sure. I'll just panic as I seem
 * to get interrupts back fine and I have version 4 firmware.
 */
static void
ray_start_join_timo (xsc)
    void		*xsc;
{
    struct ray_softc	*sc = xsc;

    RAY_DPRINTF(("ray%d: ray_start_join_timo\n", sc->unit));
    RAY_MAP_CM(sc);

    panic("ray%d: ray-start_join_timo occured\n", sc->unit);

    return;
}
#endif /* RAY_NEED_STARTJOIN_TIMEO */

/*
 * Obtain a free ccs buffer.
 *
 * Returns 1 and in `ccsp' the bus offset of the free ccs 
 * or 0 if none are free
 *
 * If `track' is not zero, handles tracking this command
 * possibly indicating a callback is needed and setting a timeout
 * also if ECF isn't ready we terminate earlier to avoid overhead.
 *
 * This routine is only used for commands
 */
static int
ray_alloc_ccs (sc, ccsp, cmd, track)
    struct	ray_softc *sc;
    size_t	*ccsp;
    u_int	cmd, track;
{
    size_t	ccs;
    u_int	i;

    RAY_DPRINTF(("ray%d: ray_alloc_ccs for cmd %d\n", sc->unit, cmd));
    RAY_MAP_CM(sc);

#if XXX
	/* for tracked commands, if not ready just set pending */
	if (track && !RAY_ECF_READY(sc)) {
		ray_cmd_schedule(sc, track);
		return (0);
	}
#endif /* XXX */

    for (i = RAY_CCS_CMD_FIRST; i <= RAY_CCS_CMD_LAST; i++) {
	/* probe here to make the card go */
	(void)SRAM_READ_FIELD_1(sc, RAY_CCS_ADDRESS(i), ray_cmd, c_status);
	if (!sc->sc_ccsinuse[i])
	    break;
    }
    if (i > RAY_CCS_CMD_LAST) {
#if XXX
	if (track)
		ray_cmd_schedule(sc, track);
#endif /* XXX */
	return (0);
    }
    sc->sc_ccsinuse[i] = 1;
    ccs = RAY_CCS_ADDRESS(i);
    SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd, c_status, RAY_CCS_STATUS_BUSY);
    SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd, c_cmd, cmd);
    SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd, c_link, RAY_CCS_LINK_NULL);

    *ccsp = ccs;
    return (1);
}

/*
 * Free up a ccs/cmd and return the old status.
 * This routine is only used for commands.
 */
static u_int8_t
ray_free_ccs (sc, ccs)
    struct	ray_softc *sc;
    size_t	ccs;
{
    u_int8_t	stat;

    RAY_DPRINTF(("ray%d: free_ccs 0x%02x\n", sc->unit, RAY_CCS_INDEX(ccs)));
    RAY_MAP_CM(sc);

    stat = SRAM_READ_FIELD_1(sc, ccs, ray_cmd, c_status);
    SRAM_WRITE_FIELD_1(sc, ccs, ray_cmd, c_status, RAY_CCS_STATUS_FREE);
    if (ccs <= RAY_CCS_ADDRESS(RAY_CCS_LAST))
	    sc->sc_ccsinuse[RAY_CCS_INDEX(ccs)] = 0;

    return (stat);
}

/*
 * Issue a command by writing the mailbox and tickling the card.
 * Only used for commands not transmitted packets.
 */
static int
ray_issue_cmd(sc, ccs, track)
    struct	ray_softc *sc;
    size_t	ccs;
    u_int	track;
{
    u_int	i;

    RAY_DPRINTF(("ray%d: ray_cmd_issue, track = 0x%x\n", sc->unit, track));
    RAY_MAP_CM(sc);

    /*
     * XXX other drivers did this, but I think 
     * what we really want to do is just make sure we don't
     * get here or that spinning is ok
     */
    i = 0;
    while (!RAY_ECF_READY(sc))
	if (++i > 50) {
	    ray_free_ccs(sc, ccs);
#if XXX
	    if (track)
		ray_cmd_schedule(sc, track);
#endif /* XXX */
	    return (0);
	}

    SRAM_WRITE_1(sc, RAY_SCB_CCSI, RAY_CCS_INDEX(ccs));
    RAY_ECF_START_CMD(sc);
#if XXX
    ray_cmd_ran(sc, track);
#endif /* XXX */

    return (1);
}

/*
 * Two routines to read from/write to the attribute memory.
 *
 * Taken from if_xe.c.
 *
 * Until there is a real way of accessing the attribute memory from a driver
 * these have to stay.
 *
 * The hack to use the crdread/crdwrite device functions causes the attribute
 * memory to be remapped into the controller and looses the mapping of
 * the common memory.
 *
 * We cheat by using PIOCSMEM and assume that the common memory window
 * is in window 0 of the card structure.
 *
 * Also
 *	pccard/pcic.c/crdread does mark the unmapped window as inactive
 *	pccard/pccard.c/map_mem toggles the mapping of a window on
 *	successive calls
 *
 */
static void
ray_attr_getmap (struct ray_softc *sc)
{
    struct ucred uc;
    struct pcred pc;
    struct proc p;

    RAY_DPRINTF(("ray%d: attempting to get map for common memory\n", sc->unit));

    sc->md.window = 0;

    p.p_cred = &pc;
    p.p_cred->pc_ucred = &uc;
    p.p_cred->pc_ucred->cr_uid = 0;

    RAY_DPRINTF(("  ioctl returns 0x%0x\n", cdevsw[CARD_MAJOR]->d_ioctl(makedev(CARD_MAJOR, sc->slotnum), PIOCGMEM, (caddr_t)&sc->md, 0, &p)));

    RAY_DPRINTF(("  flags 0x%02x, start 0x%p, size 0x%08x, card address 0x%lx\n", sc->md.flags, sc->md.start, sc->md.size, sc->md.card));

    return;
}

static void
ray_attr_cm (struct ray_softc *sc)
{
    struct ucred uc;
    struct pcred pc;
    struct proc p;

    RAY_DPRINTF(("ray%d: attempting to remap common memory\n", sc->unit));

    p.p_cred = &pc;
    p.p_cred->pc_ucred = &uc;
    p.p_cred->pc_ucred->cr_uid = 0;

    cdevsw[CARD_MAJOR]->d_ioctl(makedev(CARD_MAJOR, sc->slotnum), PIOCSMEM, (caddr_t)&sc->md, 0, &p);

    return;
}

static int
ray_attr_write (struct ray_softc *sc, off_t offset, u_int8_t byte)
{
  struct iovec iov;
  struct uio uios;
  int err;

  iov.iov_base = &byte;
  iov.iov_len = sizeof(byte);

  uios.uio_iov = &iov;
  uios.uio_iovcnt = 1;
  uios.uio_offset = offset;
  uios.uio_resid = sizeof(byte);
  uios.uio_segflg = UIO_SYSSPACE;
  uios.uio_rw = UIO_WRITE;
  uios.uio_procp = 0;

  err = cdevsw[CARD_MAJOR]->d_write(makedev(CARD_MAJOR, sc->slotnum), &uios, 0);

  ray_attr_cm(sc);

  return(err);
}

static int
ray_attr_read (struct ray_softc *sc, off_t offset, u_int8_t *buf, int size)
{
  struct iovec iov;
  struct uio uios;
  int err;

  iov.iov_base = buf;
  iov.iov_len = size;

  uios.uio_iov = &iov;
  uios.uio_iovcnt = 1;
  uios.uio_offset = offset;
  uios.uio_resid = size;
  uios.uio_segflg = UIO_SYSSPACE;
  uios.uio_rw = UIO_READ;
  uios.uio_procp = 0;

  err =  cdevsw[CARD_MAJOR]->d_read(makedev(CARD_MAJOR, sc->slotnum), &uios, 0);

  ray_attr_cm(sc);

  return(err);
}

static u_int8_t
ray_read_reg (sc, off)
    struct ray_softc	*sc
    off_t		off
{
    u_int8_t		byte;

    ray_attr_read(sc, off, &byte, 1);

    return(byte);
}

#if XXX
/*
 * Could be replaced by the following macro
 * RAY_ECF_READY(sc)	(!(REG_READ(sc, RAY_ECFIR) & RAY_ECFIR_IRQ))
 * where reg_read is a suitable macro to read a byte in the attribute memory.
 */
static int
ray_ecf_ready(struct ray_softc *sc)
{
    u_int8_t	byte;

    ray_attr_read(sc, RAY_ECFIR, &byte, 1);

    return (!(byte & RAY_ECFIR_IRQ));
}

/*
 * Could be replaced by the following macro
 * RAY_HCS_INTR(sc)	(REG_READ(sc, RAY_HCSIR) & RAY_HCSIR_IRQ)
 * where reg_read is a suitable macro to read a byte in the attribute memory.
 */
static int
ray_hcs_intr(struct ray_softc *sc)
{
    u_int8_t	byte;

    ray_attr_read(sc, RAY_HCSIR, &byte, 1);

    return (byte & RAY_HCSIR_IRQ);
}
#endif

#endif /* NRAY */
