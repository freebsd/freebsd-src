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

#ifndef RAY_DEBUG
#define RAY_DEBUG 100
#endif /* RAY_DEBUG */
#define HEXDUMP_8(p, l) do {					\
    u_int8_t *i;						\
    int j;							\
    for (i = p; i < (u_int8_t *)(p+l); i += 8) {		\
	printf("  0x%08lx %02x", (unsigned long)i, *i);		\
	for (j = 1; j < 8; j++)					\
	    printf(" %02x", *(i+j));				\
	printf("\n");						\
    }								\
} while (0)
#define HEXDUMP_16(p, l) do {					\
    u_int16_t *i;						\
    int j;							\
    for (i = p; i < (u_int16_t *)(p+l); i += 8) {		\
	printf("  0x%08lx %02x", (unsigned long)i, *i);		\
	for (j = 1; j < 8; j++)					\
	    printf(" %02x", *(i+j));				\
	printf("\n");						\
    }								\
} while (0)

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

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>

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

    struct arpcom		arpcom;		/* Ethernet common */
    struct ifmedia		ifmedia;	/* Ifnet common */

    char			*card_type;	/* Card model name */
    char			*vendor;	/* Card manufacturer */

    int				unit;		/* Unit number */
    u_char			gone;		/* 1 = Card bailed out */
    int				slotnum;	/* Slot number XXX only for attr read/write */
    int				irq;		/* Assigned IRQ */
    caddr_t			maddr;		/* Shared RAM Address */
    int				msize;		/* Shared RAM Size */

    struct ray_ecf_startup_v5	sc_ecf_startup; /* Startup info from card */
};
static struct ray_softc ray_softc[NRAY];

#define	sc_version	sc_ecf_startup.e_fw_build_string

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
 * Misc. internal utilites
 */

/*
 * As described in if_xe.c...
 *
 * Horrid stuff for accessing CIS tuples
 */
#define CARD_MAJOR		50
static int	ray_attr_write		__P((struct ray_softc *sc, off_t offset, u_char byte));
static int	ray_attr_read		__P((struct ray_softc *sc, off_t offset, u_char *buf, int size));

/*
 * PCCard initialise.
 */
static int
ray_pccard_init(dev_p)
    struct pccard_devinfo   *dev_p;
{
    struct ray_softc	*sc;
    u_int32_t		irq;
    int			j;

#if RAY_DEBUG > 0
    printf("ray%d: PCCard probe\n", dev_p->isahd.id_unit);
#endif

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

    if (ray_attach(&dev_p->isahd))
	return(ENXIO);

    return(0);
}

/*
 * PCCard unload.
 */
static void
ray_pccard_unload(dev_p)
    struct pccard_devinfo	*dev_p;
{
    struct ray_softc		*sc;
    struct ifnet		*ifp;

#if RAY_DEBUG > 0
    printf("ray%d: PCCard unload\n", dev_p->isahd.id_unit);
#endif

    sc = &ray_softc[dev_p->isahd.id_unit];

    if (sc->gone) {
	printf("ray%d: already unloaded\n", sc->unit);
	return;
    }

    /*
     * Cleardown interface
     */
    ifp = &sc->arpcom.ac_if;
    ifp->if_flags &= ~(IFF_RUNNING|IFF_OACTIVE);
    if_down(ifp);

    /*
     * Mark card as gone
     */
    sc->gone = 1;
    printf("ray%d: unloaded\n", sc->unit);

    return;
}

/*
 * PCCard interrupt.
 */
/* XXX return 1 if we take interrupt, 0 otherwise */
static int
ray_pccard_intr(dev_p)
    struct pccard_devinfo	*dev_p;
{
    struct ray_softc		*sc;

#if RAY_DEBUG > 0
    printf("ray%d: PCCard intr\n", dev_p->isahd.id_unit);
#endif

    sc = &ray_softc[dev_p->isahd.id_unit];

    if (sc->gone) {
	printf("ray%d: unloaded before interrupt!\n", sc->unit);
	return(0);
    }

    return(1);
}

/*
 * ISA probe routine.
 */
static int
ray_probe(dev_p)
    struct isa_device		*dev_p;
{

#if RAY_DEBUG > 0
    printf("ray%d: ISA probe\n", dev_p->id_unit);
#endif

    return(0);
}

/*
 * ISA/PCCard attach.
 */
static int
ray_attach(dev_p)
    struct isa_device		*dev_p;
{
    struct ray_softc		*sc;
    struct ray_ecf_startup_v5	*ep;
    struct ifnet		*ifp;

#if RAY_DEBUG > 0
    printf("ray%d: ISA/PCCard attach\n", dev_p->id_unit);
#endif

    sc = &ray_softc[dev_p->id_unit];

    if (sc->gone) {
	printf("ray%d: unloaded before attach!\n", sc->unit);
	return(0);
    }

    /*
     * Read startup results, check the card is okay and work out what
     * version we are using.
     */
    ep = &sc->sc_ecf_startup;
    bcopy(sc->maddr + RAY_ECF_TO_HOST_BASE, ep, sizeof(sc->sc_ecf_startup));
    if (ep->e_status != RAY_ECFS_CARD_OK) {
/* XXX freebsd has a nice bit mask print thingy - use it here */
	printf("ray%d: card failed self test: status 0x%0x\n", sc->unit,
	    ep->e_status);

	return(0);
    }
    if (sc->sc_version != RAY_ECFS_BUILD_4 && sc->sc_version != RAY_ECFS_BUILD_5) {
	printf("ray%d: unsupported firmware version 0x%0x\n", sc->unit,
	    ep->e_fw_build_string);
	return(0);
    }

#if RAY_DEBUG > 1
    printf("ray%d: Start Up Results\n", sc->unit);
#if RAY_DEBUG > 10
    HEXDUMP_8((u_int8_t *)sc->maddr + RAY_ECF_TO_HOST_BASE, 0x40);
#endif
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
	printf("  TIB size %0x\n", ep->e_tib_size);
    }
#endif

    /*
     * Reset any pending interrupts
     */
#if 0
{u_int8_t p[16]; printf("Status pre interrupt clearing\n"); ray_attr_read(sc, RAY_CCR, p, sizeof(p)); HEXDUMP_8(p, sizeof(p)); HEXDUMP_16((u_int16_t *)p, sizeof(p) / 2);}
#endif
    ray_attr_write(sc, RAY_HCSIR, 0);
#if 0
{u_int8_t p[16]; printf("Status post interrupt clearing\n"); ray_attr_read(sc, RAY_CCR, p, sizeof(p)); HEXDUMP_8(p, sizeof(p)); HEXDUMP_16((u_int16_t *)p, sizeof(p) / 2);}
#endif

    /*
     * Initialise the network interface structure
     */
#if XXX
sc->arpcom.ac_enaddr = 
#endif /* XXX */
    ifp = &sc->arpcom.ac_if;
    ifp->if_softc = sc;
    ifp->if_name = "ray";
    ifp->if_unit = sc->unit;
    ifp->if_timer = 0;
    ifp->if_flags = (IFF_BROADCAST | IFF_SIMPLEX ); /* XXX - IFF_MULTICAST */


#if XXX
ifp->if_linkmib = &scp->mibdata;
ifp->if_linkmiblen = sizeof scp->mibdata;
#endif /* XXX */

printf("type 0x%x\n", ifp->if_type);
printf("addrlen 0x%x\n", ifp->if_addrlen);
printf("physical 0x%x\n", ifp->if_physical);
printf("hdrlen 0x%x\n", ifp->if_hdrlen);
printf("mtu 0x%lx\n", ifp->if_mtu);
printf("metic 0x%lx\n", ifp->if_metric);
printf("baudrate 0x%lx\n", ifp->if_baudrate);
#if XXX
if_mtu
...
if_rawoutput

    ifp->if_output = ether_output;
    ifp->if_start = ray_start;
    ifp->if_ioctl = ray_ioctl;
    ifp->if_watchdog = ray_watchdog;
    ifp->if_init = ray_init;

    ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;

xe
  ifmedia_init(scp->ifm, 0, xe_media_change, xe_media_status);
  callout_handle_init(&scp->chand);

  ifmedia_add(scp->ifm, IFM_ETHER|IFM_AUTO, 0, NULL);

  if_attach(scp->ifp);
  ether_ifattach(scp->ifp);

#endif /* XXX */

#if XXX
    return(1);
#else
    return(0);
#endif
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

#if RAY_DEBUG > 0
    printf("ray%d: Network start\n", ifp->if_unit);
#endif

    sc = ifp->if_softc;

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

#if RAY_DEBUG > 0
    printf("ray%d: Network ioctl\n", ifp->if_unit);
#endif

    sc = ifp->if_softc;

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
	    error = ether_ioctl(ifp, command, data);
	    break;

	case SIOCSIFFLAGS:
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
	    /*
	     * Multicast list has (maybe) changed; set the hardware filter
	     * accordingly. This also serves to deal with promiscuous mode
	     * if we have a BPF listener active.
	     */
	    ray_setmulti(sc);
#endif /* XXX */

	    error = 0;
	    break;

	default:
	    error = EINVAL;
    }

    (void)splx(s);

    return error;
}

static void
ray_watchdog (ifp)
    register struct ifnet	*ifp;
{
    struct ray_softc *sc;

#if RAY_DEBUG > 0
    printf("ray%d: Network watchdog\n", ifp->if_unit);
#endif

    sc = ifp->if_softc;

    if (sc->gone) {
	printf("ray%d: unloaded before watchdog!\n", sc->unit);
	return;
    }

    printf("ray%d: watchdog timeout\n", sc->unit);

/* XXX may need to have remedial action here
   for example
   	ray_reset
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
    void *xsc;
{
    struct ray_softc	*sc = xsc;
    struct ifnet	*ifp;
    int			s;

#if RAY_DEBUG > 0
    printf("ray%d: Network init\n", sc->unit);
#endif

    if (sc->gone) {
	printf("ray%d: unloaded before init!\n", sc->unit);
	return;
    }

    ifp = &sc->arpcom.ac_if;

/* XXX stuff here please */

    /*
     * Set running and clear output active, then attempt to start output
     */
    ifp->if_flags |= IFF_RUNNING;
    ifp->if_flags &= ~IFF_OACTIVE;
/* XXX spl's needed higher up? */
    s = splimp();
    ray_start(ifp);
    (void) splx(s);

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

#if RAY_DEBUG > 0
    printf("ray%d: Network stop\n", sc->unit);
#endif

    if (sc->gone) {
	printf("ray%d: unloaded before stop!\n", sc->unit);
	return;
    }

    ifp = &sc->arpcom.ac_if;

/* XXX stuff here please to kill activity on the card and drain down transmissons */

    /*
     * Mark as not running
     */
    ifp->if_flags &= ~IFF_RUNNING;

    return;
}

/*
 * Two routines to read from/write to the attribute memory.
 *
 * Taken from if_xe.c.
 *
 * Until there is a real way of accessing the attribute memory from a driver
 * these have to stay.
 *
 */
static int
ray_attr_write(struct ray_softc *sc, off_t offset, u_char byte)
{
  struct iovec iov;
  struct uio uios;

  iov.iov_base = &byte;
  iov.iov_len = sizeof(byte);

  uios.uio_iov = &iov;
  uios.uio_iovcnt = 1;
  uios.uio_offset = offset;
  uios.uio_resid = sizeof(byte);
  uios.uio_segflg = UIO_SYSSPACE;
  uios.uio_rw = UIO_WRITE;
  uios.uio_procp = 0;

  return cdevsw[CARD_MAJOR]->d_write(makedev(CARD_MAJOR, sc->slotnum), &uios, 0);
}

static int
ray_attr_read(struct ray_softc *sc, off_t offset, u_char *buf, int size)
{
  struct iovec iov;
  struct uio uios;

  iov.iov_base = buf;
  iov.iov_len = size;

  uios.uio_iov = &iov;
  uios.uio_iovcnt = 1;
  uios.uio_offset = offset;
  uios.uio_resid = size;
  uios.uio_segflg = UIO_SYSSPACE;
  uios.uio_rw = UIO_READ;
  uios.uio_procp = 0;

  return cdevsw[CARD_MAJOR]->d_read(makedev(CARD_MAJOR, sc->slotnum), &uios, 0);
}

#endif /* NRAY */
