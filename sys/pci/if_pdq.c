/*-
 * Copyright (c) 1995 Matt Thomas (thomas@lkg.dec.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: if_pdq.c,v 1.7 1995/10/13 19:48:06 wollman Exp $
 *
 */

/*
 * DEC PDQ FDDI Controller; code for BSD derived operating systems
 *
 * Written by Matt Thomas
 *
 *   This driver supports the following FDDI controllers:
 *
 *	Device:			Config file entry:
 *	  DEC DEFPA (PCI)         device fpa0
 *	  DEC DEFEA (EISA)        device fea0 at isa0 net irq ? vector feaintr
 *
 *   Eventually, the following adapters will also be supported:
 *
 *	  DEC DEFTA (TC)	  device fta0 at tc? slot * vector ftaintr
 *	  DEC DEFQA (Q-Bus)	  device fta0 at uba? csr 0?? vector fqaintr
 *	  DEC DEFAA (FB+)	  device faa0 at fbus? slot * vector faaintr
 */


#include "fea.h"		/* DEFPA EISA FDDI */
#ifndef __bsdi__
#include "fpa.h"		/* DEFPA PCI FDDI */
#endif
#if NFPA > 0 || NFEA > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/route.h>

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif
#include <netinet/if_fddi.h>

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_param.h>

#if NFPA > 0
#include <pci/pcivar.h>
#include <i386/isa/icu.h>
#endif

#if NFEA > 0
#include <i386/isa/isa.h>
#include <i386/isa/icu.h>
#ifdef __FreeBSD__
#include <sys/devconf.h>
#include <i386/isa/isa_device.h>
#endif
#ifdef __bsdi__
#include <sys/device.h>
#include <i386/isa/isavar.h>
#include <i386/eisa/eisa.h>
#endif
#endif

#include "pdqreg.h"
#include "pdq_os.h"

typedef struct {
#ifdef __bsdi__
    struct device sc_dev;		/* base device */
    struct isadev sc_id;		/* ISA device */
    struct intrhand sc_ih;		/* intrrupt vectoring */
    struct atshutdown sc_ats;		/* shutdown routine */
#endif
    struct arpcom sc_ac;
    pdq_t *sc_pdq;
#if NBPFILTER > 0 && !defined(__FreeBSD__) && !defined(__bsdi__)
    caddr_t sc_bpf;
#endif
#if NFEA > 0
    unsigned sc_iobase;
#endif
} pdq_softc_t;

#define	sc_if		sc_ac.ac_if

#if defined(__FreeBSD__)
#define	sc_bpf		sc_if.if_bpf
typedef void ifnet_ret_t;
#elif defined(__bsdi__)
#define	sc_bpf		sc_if.if_bpf
typedef int ifnet_ret_t;
#endif


static void
pdq_ifreset(
    pdq_softc_t *sc)
{
    pdq_stop(sc->sc_pdq);
}

static void
pdq_ifinit(
    pdq_softc_t *sc)
{
    if (sc->sc_if.if_flags & IFF_UP) {
	sc->sc_if.if_flags |= IFF_RUNNING;
	if (sc->sc_if.if_flags & IFF_PROMISC) {
	    sc->sc_pdq->pdq_flags |= PDQ_PROMISC;
	} else {
	    sc->sc_pdq->pdq_flags &= ~PDQ_PROMISC;
	}
	if (sc->sc_if.if_flags & IFF_ALLMULTI) {
	    sc->sc_pdq->pdq_flags |= PDQ_ALLMULTI;
	} else {
	    sc->sc_pdq->pdq_flags &= ~PDQ_ALLMULTI;
	}
	if (sc->sc_if.if_flags & IFF_LINK1) {
	    sc->sc_pdq->pdq_flags |= PDQ_PASS_SMT;
	} else {
	    sc->sc_pdq->pdq_flags &= ~PDQ_PASS_SMT;
	}
	sc->sc_pdq->pdq_flags |= PDQ_RUNNING;
	pdq_run(sc->sc_pdq);
    } else {
	sc->sc_if.if_flags &= ~IFF_RUNNING;
	sc->sc_pdq->pdq_flags &= ~PDQ_RUNNING;
	pdq_stop(sc->sc_pdq);
    }
}

static void
pdq_ifwatchdog(
    pdq_softc_t *sc)
{
    struct mbuf *m;
    /*
     * No progress was made on the transmit queue for PDQ_OS_TX_TRANSMIT
     * seconds.  Remove all queued packets.
     */

    sc->sc_if.if_flags &= ~IFF_OACTIVE;
    sc->sc_if.if_timer = 0;
    for (;;) {
	IF_DEQUEUE(&sc->sc_if.if_snd, m);
	if (m == NULL)
	    return;
	m_freem(m);
    }
}

static ifnet_ret_t
pdq_ifstart(
    struct ifnet *ifp)
{
    pdq_softc_t *sc = (pdq_softc_t *) ((caddr_t) ifp - offsetof(pdq_softc_t, sc_ac.ac_if));
    struct ifqueue *ifq = &ifp->if_snd;
    struct mbuf *m;
    int tx = 0;

    if ((ifp->if_flags & IFF_RUNNING) == 0)
	return;

    if (sc->sc_if.if_timer == 0)
	sc->sc_if.if_timer = PDQ_OS_TX_TIMEOUT;

    if ((sc->sc_pdq->pdq_flags & PDQ_TXOK) == 0) {
	sc->sc_if.if_flags |= IFF_OACTIVE;
	return;
    }
    for (;; tx = 1) {
	IF_DEQUEUE(ifq, m);
	if (m == NULL)
	    break;

	if (pdq_queue_transmit_data(sc->sc_pdq, m) == PDQ_FALSE) {
	    ifp->if_flags |= IFF_OACTIVE;
	    IF_PREPEND(ifq, m);
	    break;
	}
    }
    if (tx)
	PDQ_DO_TYPE2_PRODUCER(sc->sc_pdq);
}

void
pdq_os_receive_pdu(
    pdq_t *pdq,
    struct mbuf *m,
    size_t pktlen)
{
    pdq_softc_t *sc = (pdq_softc_t *) pdq->pdq_os_ctx;
    struct fddi_header *fh = mtod(m, struct fddi_header *);

    sc->sc_if.if_ipackets++;
#if NBPFILTER > 0
    if (sc->sc_bpf != NULL)
	bpf_mtap(sc->sc_bpf, m);
    if ((fh->fddi_fc & (FDDIFC_L|FDDIFC_F)) != FDDIFC_LLC_ASYNC) {
	m_freem(m);
	return;
    }
#endif

    m->m_data += sizeof(struct fddi_header);
    m->m_len  -= sizeof(struct fddi_header);
    m->m_pkthdr.len = pktlen - sizeof(struct fddi_header);
    m->m_pkthdr.rcvif = &sc->sc_if;
    fddi_input(&sc->sc_if, fh, m);
}

void
pdq_os_restart_transmitter(
    pdq_t *pdq)
{
    pdq_softc_t *sc = (pdq_softc_t *) pdq->pdq_os_ctx;
    sc->sc_if.if_flags &= ~IFF_OACTIVE;
    if (sc->sc_if.if_snd.ifq_head != NULL) {
	sc->sc_if.if_timer = PDQ_OS_TX_TIMEOUT;
	pdq_ifstart(&sc->sc_if);
    } else {
	sc->sc_if.if_timer = 0;
    }
}

void
pdq_os_transmit_done(
    pdq_t *pdq,
    struct mbuf *m)
{
    pdq_softc_t *sc = (pdq_softc_t *) pdq->pdq_os_ctx;
#if NBPFILTER > 0
    if (sc->sc_bpf != NULL)
	bpf_mtap(sc->sc_bpf, m);
#endif
    m_freem(m);
    sc->sc_if.if_opackets++;
}

void
pdq_os_addr_fill(
    pdq_t *pdq,
    pdq_lanaddr_t *addr,
    size_t num_addrs)
{
    pdq_softc_t *sc = (pdq_softc_t *) pdq->pdq_os_ctx;
    struct ether_multistep step;
    struct ether_multi *enm;

    ETHER_FIRST_MULTI(step, &sc->sc_ac, enm);
    while (enm != NULL && num_addrs > 0) {
	((u_short *) addr->lanaddr_bytes)[0] = ((u_short *) enm->enm_addrlo)[0];
	((u_short *) addr->lanaddr_bytes)[1] = ((u_short *) enm->enm_addrlo)[1];
	((u_short *) addr->lanaddr_bytes)[2] = ((u_short *) enm->enm_addrlo)[2];
	ETHER_NEXT_MULTI(step, enm);
	addr++;
	num_addrs--;
    }
}

static int
pdq_ifioctl(
    struct ifnet *ifp,
    int cmd,
    caddr_t data)
{
    pdq_softc_t *sc = (pdq_softc_t *) ((caddr_t) ifp - offsetof(pdq_softc_t, sc_ac.ac_if));
    int s, error = 0;

    s = splimp();

    switch (cmd) {
	case SIOCSIFADDR: {
	    struct ifaddr *ifa = (struct ifaddr *)data;

	    ifp->if_flags |= IFF_UP;
	    switch(ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET: {
		    ((struct arpcom *)ifp)->ac_ipaddr = IA_SIN(ifa)->sin_addr;
		    (*ifp->if_init)(ifp->if_unit);
#ifdef __FreeBSD__
		    arp_ifinit((struct arpcom *)ifp, ifa);
#else
		    arpwhohas((struct arpcom *)ifp, &IA_SIN(ifa)->sin_addr);
#endif
		    break;
		}
#endif /* INET */

#ifdef NS
		/* This magic copied from if_is.c; I don't use XNS,
		 * so I have no way of telling if this actually
		 * works or not.
		 */
		case AF_NS: {
		    struct ns_addr *ina = &(IA_SNS(ifa)->sns_addr);
		    if (ns_nullhost(*ina)) {
			ina->x_host = *(union ns_host *)(sc->sc_ac.ac_enaddr);
		    } else {
			ifp->if_flags &= ~IFF_RUNNING;
			bcopy((caddr_t)ina->x_host.c_host,
			      (caddr_t)sc->sc_ac.ac_enaddr,
			      sizeof sc->sc_ac.ac_enaddr);
		    }

		    (*ifp->if_init)(ifp->if_unit);
		    break;
		}
#endif /* NS */

		default: {
		    (*ifp->if_init)(ifp->if_unit);
		    break;
		}
	    }
	    break;
	}

	case SIOCSIFFLAGS: {
	    (*ifp->if_init)(ifp->if_unit);
	    break;
	}

	case SIOCADDMULTI:
	case SIOCDELMULTI: {
	    /*
	     * Update multicast listeners
	     */
	    if (cmd == SIOCADDMULTI)
		error = ether_addmulti((struct ifreq *)data, &sc->sc_ac);
	    else
		error = ether_delmulti((struct ifreq *)data, &sc->sc_ac);

	    if (error == ENETRESET) {
		if (sc->sc_if.if_flags & IFF_RUNNING)
		    pdq_run(sc->sc_pdq);
		error = 0;
	    }
	    break;
	}

	default: {
	    error = EINVAL;
	    break;
	}
    }

    splx(s);
    return error;
}

static void
pdq_ifattach(
    pdq_softc_t *sc,
    ifnet_ret_t (*ifinit)(int unit),
    ifnet_ret_t (*ifreset)(int unit),
    ifnet_ret_t (*ifwatchdog)(int unit))
{
    struct ifnet *ifp = &sc->sc_if;

    ifp->if_flags = IFF_BROADCAST|IFF_SIMPLEX|IFF_MULTICAST;

    ifp->if_init = ifinit;
#ifndef __bsdi__
    ifp->if_reset = ifreset;
#endif
    ifp->if_watchdog = ifwatchdog;

    ifp->if_ioctl = pdq_ifioctl;
    ifp->if_output = fddi_output;
    ifp->if_start = pdq_ifstart;

    if_attach(ifp);
    fddi_ifattach(ifp);
#if NBPFILTER > 0
    bpfattach(&sc->sc_bpf, ifp, DLT_FDDI, sizeof(struct fddi_header));
#endif

}

#if NFPA > 0
/*
 * This is the PCI configuration support.  Since the PDQ is available
 * on both EISA and PCI boards, one must be careful in how defines the
 * PDQ in the config file.
 */
static char *pdq_pci_probe (pcici_t config_id, pcidi_t device_id);
static void pdq_pci_attach(pcici_t config_id, int unit);
static u_long pdq_pci_count;

static struct pci_device fpadevice = {
    "fpa",
    pdq_pci_probe,
    pdq_pci_attach,
    &pdq_pci_count,
    NULL
};

#if defined(__FreeBSD__)
static pdq_softc_t *pdqs_pci[NFPA];
#define	PDQ_PCI_UNIT_TO_SOFTC(unit)	(pdqs_pci[unit])

#ifdef DATA_SET
DATA_SET (pcidevice_set, fpadevice);
#endif
#endif

#define	PCI_CBMA	0x10	/* Configuration Base Memory Address */

static ifnet_ret_t
pdq_pci_ifreset(
    int unit)
{
    pdq_ifreset(PDQ_PCI_UNIT_TO_SOFTC(unit));
}

static ifnet_ret_t
pdq_pci_ifinit(
    int unit)
{
    pdq_ifinit(PDQ_PCI_UNIT_TO_SOFTC(unit));
}

static ifnet_ret_t
pdq_pci_ifwatchdog(
    int unit)
{
    pdq_ifwatchdog(PDQ_PCI_UNIT_TO_SOFTC(unit));
}

static int
pdq_pci_ifintr(
    pdq_softc_t *sc)
{
    return pdq_interrupt(sc->sc_pdq);
}

static char *
pdq_pci_probe(
    pcici_t config_id,
    pcidi_t device_id)
{
    if (device_id == 0x000f1011ul)
	return "Digital DEFPA PCI FDDI Controller";
    return NULL;
}

static void
pdq_pci_attach(
    pcici_t config_id,
    int unit)
{
    pdq_softc_t *sc;
    vm_offset_t va_csrs, pa_csrs;

    if (unit > NFPA) {
	printf("fpa%d: not configured; kernel is built for only %d device%s.\n",
	       unit, NFPA, NFPA == 1 ? "" : "s");
	return;
    }

    sc = (pdq_softc_t *) malloc(sizeof(*sc), M_DEVBUF, M_NOWAIT);
    if (sc == NULL)
	return;

    bzero(sc, sizeof(pdq_softc_t));	/* Zero out the softc*/
    if (!pci_map_mem(config_id, PCI_CBMA, &va_csrs, &pa_csrs)) {
	free((void *) sc, M_DEVBUF);
	return;
    }

    sc->sc_if.if_name = "fpa";
    sc->sc_if.if_unit = unit;
    sc->sc_pdq = pdq_initialize((void *) va_csrs, "fpa", unit,
				(void *) sc, PDQ_DEFPA);
    if (sc->sc_pdq == NULL) {
	free((void *) sc, M_DEVBUF);
	return;
    }
    bcopy((caddr_t) sc->sc_pdq->pdq_hwaddr.lanaddr_bytes, sc->sc_ac.ac_enaddr, 6);
    pdqs_pci[unit] = sc;
    pdq_ifattach(sc, pdq_pci_ifinit, pdq_pci_ifreset, pdq_pci_ifwatchdog);
    pci_map_int(config_id, pdq_pci_ifintr, (void*) sc, &net_imask);
}
#endif /* NFPA > 0 */

#if NFEA > 0
/*
 *
 */
static const int pdq_eisa_irqs[4] = { IRQ9, IRQ10, IRQ11, IRQ15 };

#ifdef __FreeBSD__
static pdq_softc_t *pdqs_eisa[NFEA];
#define	PDQ_EISA_UNIT_TO_SOFTC(unit)	(pdqs_eisa[unit])
#endif
#ifdef __bsdi__
extern struct cfdriver feacd;
#define	PDQ_EISA_UNIT_TO_SOFTC(unit)	((pdq_softc_t *)feacd.cd_devs[unit])
#endif

static ifnet_ret_t
pdq_eisa_ifreset(
    int unit)
{
    pdq_ifreset(PDQ_EISA_UNIT_TO_SOFTC(unit));
}

static ifnet_ret_t
pdq_eisa_ifinit(
    int unit)
{
    pdq_ifinit(PDQ_EISA_UNIT_TO_SOFTC(unit));
}

static ifnet_ret_t
pdq_eisa_ifwatchdog(
    int unit)
{
    pdq_ifwatchdog(PDQ_EISA_UNIT_TO_SOFTC(unit));
}

int
feaintr(
    int unit)
{
    pdq_interrupt(PDQ_EISA_UNIT_TO_SOFTC(unit)->sc_pdq);
    return unit;
}

static void
pdq_eisa_subprobe(
    pdq_uint32_t iobase,
    pdq_uint32_t *maddr,
    pdq_uint32_t *msize,
    pdq_uint32_t *irq)
{
    if (irq != NULL)
	*irq = pdq_eisa_irqs[PDQ_OS_IORD_8(iobase + PDQ_EISA_IO_CONFIG_STAT_0) & 3];
    *maddr = (PDQ_OS_IORD_8(iobase + PDQ_EISA_MEM_ADD_CMP_0) << 16)
	| (PDQ_OS_IORD_8(iobase + PDQ_EISA_MEM_ADD_CMP_1) << 8);
    *msize = (PDQ_OS_IORD_8(iobase + PDQ_EISA_MEM_ADD_MASK_0) + 4) << 8;
}

static void
pdq_eisa_devinit(
    pdq_softc_t *sc)
{
    pdq_uint8_t data;

    /*
     * Do the standard initialization for the DEFEA registers.
     */
    PDQ_OS_IOWR_8(sc->sc_iobase + PDQ_EISA_FUNCTION_CTRL, 0x23);
    PDQ_OS_IOWR_8(sc->sc_iobase + PDQ_EISA_IO_CMP_1_1, (sc->sc_iobase >> 8) & 0x0F);
    PDQ_OS_IOWR_8(sc->sc_iobase + PDQ_EISA_IO_CMP_1_0, (sc->sc_iobase >> 8) & 0x0F);
    PDQ_OS_IOWR_8(sc->sc_iobase + PDQ_EISA_SLOT_CTRL, 0x01);
    data = PDQ_OS_IORD_8(sc->sc_iobase + PDQ_EISA_BURST_HOLDOFF);
    PDQ_OS_IOWR_8(sc->sc_iobase + PDQ_EISA_BURST_HOLDOFF, data | 1);
    data = PDQ_OS_IORD_8(sc->sc_iobase + PDQ_EISA_IO_CONFIG_STAT_0);
    PDQ_OS_IOWR_8(sc->sc_iobase + PDQ_EISA_IO_CONFIG_STAT_0, data | 8);
}

#ifdef __FreeBSD__
static struct kern_devconf kdc_fea[NFEA] = { {
        0, 0, 0,                /* filled in by dev_attach */
        "fea", 0, { MDDT_ISA, 0, "net" },
        isa_generic_externalize, 0, 0, ISA_EXTERNALLEN,
        &kdc_isa0,              /* parent */
        0,                      /* parentdata */
        DC_BUSY,                /* host adapters are always ``in use'' */
        "DEC DEFEA EISA FDDI Controller"
} };

static inline void
pdq_eisa_registerdev(
    struct isa_device *id)
{
    if (id->id_unit)
	kdc_fea[id->id_unit] = kdc_fea[0];
    kdc_fea[id->id_unit].kdc_unit = id->id_unit;
    kdc_fea[id->id_unit].kdc_parentdata = id;
    dev_attach(&kdc_fea[id->id_unit]);
}

static int pdq_eisa_slots = ~1;

static int
pdq_eisa_probe(
    struct isa_device *id)
{
    pdq_softc_t *sc;
    int slot;
    pdq_uint32_t data, irq, maddr, msize;

    slot = 0x1000 * (ffs(pdq_eisa_slots) - 1);
    for (; slot <= 0xF000; slot++) {
	pdq_eisa_slots &= ~(1 << (slot >> 12));
	data = PDQ_OS_IORD_32(slot + PDQ_EISA_SLOT_ID);
	if ((data & 0xFFFFFF) != 0x30A310)
	    continue;
	id->id_iobase = slot;
	pdq_eisa_subprobe(slot, &maddr, &msize, &irq);
	if (id->id_irq != 0 && irq != id->id_irq) {
	    printf("fea%d: error: desired IRQ of %d does not match device's actual IRQ (%d),\n",
		   id->id_unit,
		   ffs(id->id_irq) - 1, ffs(irq) - 1);
	    return 0;
	}
	id->id_irq = irq;
	if (maddr == 0) {
	    printf("fea%d: error: memory not enabled! ECU reconfiguration required\n",
		   id->id_unit);
	    return 0;
	}
	id->id_maddr = (caddr_t) pmap_mapdev(maddr, msize);
	if (id->id_maddr == NULL)
	    return 0;
	id->id_msize = msize;
	if (PDQ_EISA_UNIT_TO_SOFTC(id->id_unit) == NULL) {
	    sc = (pdq_softc_t *) malloc(sizeof(pdq_softc_t), M_DEVBUF, M_WAITOK);
	    if (sc == NULL)
		return 0;
	    PDQ_EISA_UNIT_TO_SOFTC(id->id_unit) = sc;
	}
	return 0x1000;
    }
    return 0;
}

static int
pdq_eisa_attach(
    struct isa_device *id)
{
    pdq_softc_t *sc = PDQ_EISA_UNIT_TO_SOFTC(id->id_unit);

    bzero(sc, sizeof(pdq_softc_t));	/* Zero out the softc*/

    sc->sc_if.if_name = "fea";
    sc->sc_if.if_unit = id->id_unit;
    sc->sc_iobase = id->id_iobase;

    pdq_eisa_devinit(sc);
    sc->sc_pdq = pdq_initialize((void *) id->id_maddr, "fea", sc->sc_if.if_unit,
				(void *) sc, PDQ_DEFEA);
    if (sc->sc_pdq == NULL) {
	printf("fea%d: initialization failed\n", sc->sc_if.if_unit);
	return 0;
    }

    bcopy((caddr_t) sc->sc_pdq->pdq_hwaddr.lanaddr_bytes, sc->sc_ac.ac_enaddr, 6);
    pdq_ifattach(sc, pdq_eisa_ifinit, pdq_eisa_ifreset, pdq_eisa_ifwatchdog);
    pdq_eisa_registerdev(id);
    return 1;
}

/*
 *
 */
struct isa_driver feadriver = {
    pdq_eisa_probe,
    pdq_eisa_attach,
    "fea"
};
#endif /* __FreeBSD__ */

#ifdef __bsdi__
int
pdq_eisa_probe(
    struct device *parent,
    struct cfdata *cf,
    void *aux)
{
    struct isa_attach_args *ia = (struct isa_attach_args *) aux;
    int slot;
    pdq_uint32_t irq, maddr, msize;

    if (isa_bustype != BUS_EISA)
	return (0);

    if ((slot = eisa_match(cf, ia)) == 0)
	return (0);
    ia->ia_iobase = slot << 12;
    ia->ia_iosize = EISA_NPORT;
    eisa_slotalloc(slot);

    pdq_eisa_subprobe(ia->ia_iobase, &maddr, &msize, &irq);
    if (ia->ia_irq != IRQUNK && irq != ia->ia_irq) {
	printf("fea%d: error: desired IRQ of %d does not match device's actual IRQ (%d),\n",
	       cf->cf_unit,
	       ffs(ia->ia_irq) - 1, ffs(irq) - 1);
	return 0;
    }
    if (ia->ia_irq == IRQUNK) {
	if ((irq = isa_irqalloc(irq)) == 0)
	    return 0;
	ia->ia_irq = irq;
    }
    if (maddr == 0) {
	printf("fea%d: error: memory not enabled! ECU reconfiguration required\n",
	       cf->cf_unit);
	return 0;
    }

    /* EISA bus masters don't use host DMA channels */
    ia->ia_drq = 0;         /* XXX should be DRQUNK or DRQBUSMASTER? */

#if 0
    ia->ia_maddr = maddr;
    ia->ia_msize = msize;
#else
    ia->ia_maddr = 0;
    ia->ia_msize = 0;
#endif
    return 1;
}

void
pdq_eisa_attach(
    struct device *parent,
    struct device *self,
    void *aux)
{
    pdq_softc_t *sc = (pdq_softc_t *) self;
    register struct isa_attach_args *ia = (struct isa_attach_args *) aux;
    register struct ifnet *ifp = &sc->sc_if;
    int i;

    sc->sc_if.if_unit = sc->sc_dev.dv_unit;
    sc->sc_if.if_name = "fea";
    sc->sc_if.if_flags = 0;

    sc->sc_iobase = ia->ia_iobase;

    sc->sc_pdq = pdq_initialize((void *) ISA_HOLE_VADDR(ia->ia_maddr), "fea",
				sc->sc_if.if_unit, (void *) sc, PDQ_DEFEA);
    if (sc->sc_pdq == NULL) {
	printf("fea%d: initialization failed\n", sc->sc_if.if_unit);
	return;
    }

    bcopy((caddr_t) sc->sc_pdq->pdq_hwaddr.lanaddr_bytes, sc->sc_ac.ac_enaddr, 6);

    pdq_ifattach(sc, pdq_eisa_ifinit, pdq_eisa_ifreset, pdq_eisa_ifwatchdog);

    isa_establish(&sc->sc_id, &sc->sc_dev);

    sc->sc_ih.ih_fun = feaintr;
    sc->sc_ih.ih_arg = (void *)sc;
    intr_establish(ia->ia_irq, &sc->sc_ih, DV_NET);

    sc->sc_ats.func = (void (*)(void *)) pdq_stop;
    sc->sc_ats.arg = (void *) sc->sc_pdq;
    atshutdown(&sc->sc_ats, ATSH_ADD);
}

static char *pdq_eisa_ids[] = {
    "DEC3001",	/* 0x0130A310 */
    "DEC3002",	/* 0x0230A310 */
    "DEC3003",	/* 0x0330A310 */
    "DEC3004",	/* 0x0430A310 */
};

struct cfdriver feacd = {
    0, "fea", pdq_eisa_probe, pdq_eisa_attach, DV_IFNET, sizeof(pdq_softc_t),
    pdq_eisa_ids
};
#endif /* __bsdi__ */
#endif /* NFEA > 0 */
#endif /* NFPA > 0 || NFEA > 0 */
