/*	$NetBSD: if_cnw.c,v 1.15 2000/10/16 10:26:41 itojun Exp $	*/


#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Michael Eriksson.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1996, 1997 Berkeley Software Design, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that this notice is retained,
 * the conditions in the following notices are met, and terms applying
 * to contributors in the following notices also apply to Berkeley
 * Software Design, Inc.
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by
 *	Berkeley Software Design, Inc.
 * 4. Neither the name of the Berkeley Software Design, Inc. nor the names
 *    of its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BERKELEY SOFTWARE DESIGN, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BERKELEY SOFTWARE DESIGN, INC. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Paul Borman, December 1996
 *
 * This driver is derived from a generic frame work which is
 * Copyright(c) 1994,1995,1996
 * Yoichi Shinoda, Yoshitaka Tokugawa, WIDE Project, Wildboar Project
 * and Foretune.  All rights reserved.
 *
 * A linux driver was used as the "hardware reference manual" (i.e.,
 * to determine registers and a general outline of how the card works)
 * That driver is publically available and copyright
 *
 * John Markus Bjørndalen
 * Department of Computer Science
 * University of Tromsø
 * Norway             
 * johnm@staff.cs.uit.no, http://www.cs.uit.no/~johnm/
 */

/*
 * This is a driver for the Xircom CreditCard Netwave (also known as
 * the Netwave Airsurfer) wireless LAN PCMCIA adapter.
 *
 * When this driver was developed, the Linux Netwave driver was used
 * as a hardware manual. That driver is Copyright (c) 1997 University
 * of Tromsø, Norway. It is part of the Linix pcmcia-cs package that
 * can be found at
 * http://hyper.stanford.edu/HyperNews/get/pcmcia/home.html. The most
 * recent version of the pcmcia-cs package when this driver was
 * written was 3.0.6.
 *
 * Unfortunately, a lot of explicit numeric constants were used in the
 * Linux driver. I have tried to use symbolic names whenever possible,
 * but since I don't have any real hardware documentation, there's
 * still one or two "magic numbers" :-(.
 *
 * Driver limitations: This driver doesn't do multicasting or receiver
 * promiscuity, because of missing hardware documentation. I couldn't
 * get receiver promiscuity to work, and I haven't even tried
 * multicast. Volunteers are welcome, of course :-).
 */

#if !defined(__FreeBSD__)
#include "opt_inet.h"
#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/socket.h>
#include <sys/mbuf.h>
#include <sys/ioctl.h>
#include <sys/proc.h>

#include <net/if.h>

#include <dev/pcmcia/if_cnwreg.h>
#include <dev/pcmcia/if_cnwioctl.h>

#include <dev/pcmcia/pcmciareg.h>
#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciadevs.h>

#include <net/if_dl.h>
#include <net/if_ether.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_inarp.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

/*
 * Let these be patchable variables, initialized from macros that can
 * be set in the kernel config file. Someone with lots of spare time
 * could probably write a nice Netwave configuration program to do
 * this a little bit more elegantly :-).
 */
#ifndef CNW_DOMAIN
#define CNW_DOMAIN	0x100
#endif
int cnw_domain = CNW_DOMAIN;		/* Domain */
#ifndef CNW_SCRAMBLEKEY
#define CNW_SCRAMBLEKEY 0
#endif
int cnw_skey = CNW_SCRAMBLEKEY;		/* Scramble key */

/*
 * The card appears to work much better when we only allow one packet
 * "in the air" at a time.  This is done by not allowing another packet
 * on the card, even if there is room.  Turning this off will allow the
 * driver to stuff packets on the card as soon as a transmit buffer is
 * available.  This does increase the number of collisions, though.
 * We can que a second packet if there are transmit buffers available,
 * but we do not actually send the packet until the last packet has
 * been written.
 */
#define	ONE_AT_A_TIME

/*
 * Netwave cards choke if we try to use io memory address >= 0x400.
 * Even though, CIS tuple does not talk about this.
 * Use memory mapped access.
 */
#define MEMORY_MAPPED

int	cnw_match(struct device *, struct cfdata *, void *);
void	cnw_attach(struct device *, struct device *, void *);
int	cnw_detach(struct device *, int);

int	cnw_activate(struct device *, enum devact);

struct cnw_softc {
	struct device sc_dev;		    /* Device glue (must be first) */
	struct ethercom sc_ethercom;	    /* Ethernet common part */
	int sc_domain;			    /* Netwave domain */
	int sc_skey;			    /* Netwave scramble key */
	struct cnwstats sc_stats;

	/* PCMCIA-specific stuff */
	struct pcmcia_function *sc_pf;	    /* PCMCIA function */
#ifndef MEMORY_MAPPED
	struct pcmcia_io_handle sc_pcioh;   /* PCMCIA I/O space handle */
	int sc_iowin;			    /*   ...window */
	bus_space_tag_t sc_iot;		    /*   ...bus_space tag */
	bus_space_handle_t sc_ioh;	    /*   ...bus_space handle */
#endif
	struct pcmcia_mem_handle sc_pcmemh; /* PCMCIA memory handle */
	bus_addr_t sc_memoff;		    /*   ...offset */
	int sc_memwin;			    /*   ...window */
	bus_space_tag_t sc_memt;	    /*   ...bus_space tag */
	bus_space_handle_t sc_memh;	    /*   ...bus_space handle */
	void *sc_ih;			    /* Interrupt cookie */
	struct timeval sc_txlast;	    /* When the last xmit was made */
	int sc_active;			    /* Currently xmitting a packet */

	int sc_resource;		    /* Resources alloc'ed on attach */
#define CNW_RES_PCIC	1
#define CNW_RES_IO	2
#define CNW_RES_MEM	4
#define CNW_RES_NET	8
};

struct cfattach cnw_ca = {
	sizeof(struct cnw_softc), cnw_match, cnw_attach, cnw_detach,
		cnw_activate
};

#else /* FreeBSD */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/ucred.h>
#include <sys/socket.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/syslog.h>
#include <sys/sysctl.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/clock.h>
#include <machine/md_var.h>
#include <sys/rman.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>

#include <net/bpf.h>

#include <dev/pccard/pccardvar.h>
#include "card_if.h"

#include <dev/cnw/if_cnwioctl.h>
#include <dev/cnw/if_cnwreg.h>

#ifndef CNW_DOMAIN
#define CNW_DOMAIN	0x100
#endif
int cnw_domain = (int)CNW_DOMAIN;		/* Domain */

#ifndef CNW_SCRAMBLEKEY
#define CNW_SCRAMBLEKEY 0
#endif
int cnw_skey = CNW_SCRAMBLEKEY;			/* Scramble key */

#define ONE_AT_A_TIME

#ifndef MEMORY_MAPPED
#define MEMORY_MAPPED
#endif

struct cnw_softc {
	struct ifnet	*sc_ifp;
	struct ifmedia	ifmedia;
	device_t	dev;
	struct cnwstats sc_stats;
	int sc_domain;                      /* Netwave domain */
	int sc_skey;				/* Netwave scramble key */

	struct resource *	mem_res;
	struct resource *	irq;

	bus_addr_t		sc_memoff;	/*   ...offset */
	bus_space_tag_t		sc_memt;	/*   ...bus_space tag */
	bus_space_handle_t	sc_memh;	/*   ...bus_space handle */

	void *			cnw_intrhand;
	int			cnw_gone;

	struct timeval sc_txlast;           /* When the last xmit was made */
	int sc_active;                      /* Currently xmitting a packet */
};


static void cnw_freebsd_init	(void *);
static void cnw_stop		(struct cnw_softc *);

static int cnw_pccard_probe	(device_t);
static int cnw_pccard_attach	(device_t);
static int cnw_pccard_detach	(device_t);
static void cnw_shutdown	(device_t);
static int cnw_alloc		(device_t);
static void cnw_free		(device_t);

static device_method_t cnw_pccard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		cnw_pccard_probe),
	DEVMETHOD(device_attach,	cnw_pccard_attach),
	DEVMETHOD(device_detach,	cnw_pccard_detach),
	DEVMETHOD(device_shutdown,	cnw_shutdown),

	{ 0, 0 }
};

static driver_t cnw_pccard_driver = {
	"cnw",
	cnw_pccard_methods,
	sizeof(struct cnw_softc)
};

static devclass_t cnw_pccard_devclass;

DRIVER_MODULE(cnw, pccard, cnw_pccard_driver, cnw_pccard_devclass, 0, 0);
MODULE_DEPEND(cnw, ether, 1, 1, 1);

#endif /* !defined(__FreeBSD__) */

void cnw_reset(struct cnw_softc *);
void cnw_init(struct cnw_softc *);
#if !defined(__FreeBSD__)
int cnw_enable(struct cnw_softc *sc);
void cnw_disable(struct cnw_softc *sc);
void cnw_config(struct cnw_softc *sc, u_int8_t *);
#endif
void cnw_start(struct ifnet *);
void cnw_transmit(struct cnw_softc *, struct mbuf *);
struct mbuf *cnw_read(struct cnw_softc *);
void cnw_recv(struct cnw_softc *);
#if !defined(__FreeBSD__)
int cnw_intr(void *arg);
#else
void cnw_intr(void *arg);
#endif
int cnw_ioctl(struct ifnet *, u_long, caddr_t);
void cnw_watchdog(struct ifnet *);
static int cnw_setdomain(struct cnw_softc *, int);
static int cnw_setkey(struct cnw_softc *, int);

/* ---------------------------------------------------------------- */

/* Help routines */
static int wait_WOC(struct cnw_softc *, int);
static int read16(struct cnw_softc *, int);
static int cnw_cmd(struct cnw_softc *, int, int, int, int);

/* 
 * Wait until the WOC (Write Operation Complete) bit in the 
 * ASR (Adapter Status Register) is asserted. 
 */
static int
wait_WOC(sc, line)
	struct cnw_softc *sc;
	int line;
{
	int i, asr;

	for (i = 0; i < 5000; i++) {
#ifndef MEMORY_MAPPED
		asr = bus_space_read_1(sc->sc_iot, sc->sc_ioh, CNW_REG_ASR);
#else
		asr = bus_space_read_1(sc->sc_memt, sc->sc_memh,
		    sc->sc_memoff + CNW_IOM_OFF + CNW_REG_ASR);
#endif
		if (asr & CNW_ASR_WOC)
			return (0);
		DELAY(100);
	}
	if (line > 0)
#if !defined(__FreeBSD__)
		printf("%s: wedged at line %d\n", sc->sc_dev.dv_xname, line);
#else
		device_printf(sc->dev, "wedged at line %d\n", line);
#endif
	return (1);
}
#define WAIT_WOC(sc) wait_WOC(sc, __LINE__)


/*
 * Read a 16 bit value from the card. 
 */
static int
read16(sc, offset)
	struct cnw_softc *sc;
	int offset;
{
	int hi, lo;
	int offs = sc->sc_memoff + offset;

	/* This could presumably be done more efficient with
	 * bus_space_read_2(), but I don't know anything about the
	 * byte sex guarantees... Besides, this is pretty cheap as
	 * well :-)
	 */
	lo = bus_space_read_1(sc->sc_memt, sc->sc_memh, offs);
	hi = bus_space_read_1(sc->sc_memt, sc->sc_memh, offs + 1);
	return ((hi << 8) | lo);
}


/*
 * Send a command to the card by writing it to the command buffer.
 */
int
cnw_cmd(sc, cmd, count, arg1, arg2)
	struct cnw_softc *sc;
	int cmd, count, arg1, arg2;
{
	int ptr = sc->sc_memoff + CNW_EREG_CB;

	if (wait_WOC(sc, 0)) {
#if !defined(__FreeBSD__)
		printf("%s: wedged when issuing cmd 0x%x\n",
		    sc->sc_dev.dv_xname, cmd);
#else
		device_printf(sc->dev, "wedged when issuing cmd 0x%x\n", cmd);
#endif
		/*
		 * We'll continue anyway, as that's probably the best
		 * thing we can do; at least the user knows there's a
		 * problem, and can reset the interface with ifconfig
		 * down/up.
		 */
	}

	bus_space_write_1(sc->sc_memt, sc->sc_memh, ptr, cmd);
	if (count > 0) {
		bus_space_write_1(sc->sc_memt, sc->sc_memh, ptr + 1, arg1);
		if (count > 1)
			bus_space_write_1(sc->sc_memt, sc->sc_memh,
			    ptr + 2, arg2);
	}
	bus_space_write_1(sc->sc_memt, sc->sc_memh,
	    ptr + count + 1, CNW_CMD_EOC);
	return (0);
}
#define CNW_CMD0(sc, cmd) \
    do { cnw_cmd(sc, cmd, 0, 0, 0); } while (0)
#define CNW_CMD1(sc, cmd, arg1)	\
    do { cnw_cmd(sc, cmd, 1, arg1 , 0); } while (0)
#define CNW_CMD2(sc, cmd, arg1, arg2) \
    do { cnw_cmd(sc, cmd, 2, arg1, arg2); } while (0)

/* ---------------------------------------------------------------- */

/*
 * Reset the hardware.
 */
void
cnw_reset(sc)
	struct cnw_softc *sc;
{
#ifdef CNW_DEBUG
	if (sc->sc_ethercom.ec_if.if_flags & IFF_DEBUG)
		printf("%s: resetting\n", sc->sc_dev.dv_xname);
#endif
	wait_WOC(sc, 0);
#ifndef MEMORY_MAPPED
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, CNW_REG_PMR, CNW_PMR_RESET);
#else
	bus_space_write_1(sc->sc_memt, sc->sc_memh,
	    sc->sc_memoff + CNW_IOM_OFF + CNW_REG_PMR, CNW_PMR_RESET);
#endif
	bus_space_write_1(sc->sc_memt, sc->sc_memh,
	    sc->sc_memoff + CNW_EREG_ASCC, CNW_ASR_WOC);
#ifndef MEMORY_MAPPED
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, CNW_REG_PMR, 0);
#else
	bus_space_write_1(sc->sc_memt, sc->sc_memh,
	    sc->sc_memoff + CNW_IOM_OFF + CNW_REG_PMR, 0);
#endif
}


/*
 * Initialize the card.
 */
void
cnw_init(sc)
	struct cnw_softc *sc;
{
#if !defined(__FreeBSD__)
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
#else	/* FreeBSD */
	struct ifnet *ifp = sc->sc_ifp;
#endif
	const u_int8_t rxmode =
	    CNW_RXCONF_RXENA | CNW_RXCONF_BCAST | CNW_RXCONF_AMP;

	/* Reset the card */
	cnw_reset(sc);

	/* Issue a NOP to check the card */
	CNW_CMD0(sc, CNW_CMD_NOP);

	/* Set up receive configuration */
	CNW_CMD1(sc, CNW_CMD_SRC,
	    rxmode | ((ifp->if_flags & IFF_PROMISC) ? CNW_RXCONF_PRO : 0));

	/* Set up transmit configuration */
	CNW_CMD1(sc, CNW_CMD_STC, CNW_TXCONF_TXENA);

	/* Set domain */
	CNW_CMD2(sc, CNW_CMD_SMD, sc->sc_domain, sc->sc_domain >> 8);

	/* Set scramble key */
	CNW_CMD2(sc, CNW_CMD_SSK, sc->sc_skey, sc->sc_skey >> 8);

	/* Enable interrupts */
	WAIT_WOC(sc);
#ifndef MEMORY_MAPPED
	bus_space_write_1(sc->sc_iot, sc->sc_ioh,
	    CNW_REG_IMR, CNW_IMR_IENA | CNW_IMR_RFU1);
#else
	bus_space_write_1(sc->sc_memt, sc->sc_memh,
	    sc->sc_memoff + CNW_IOM_OFF + CNW_REG_IMR,
	    CNW_IMR_IENA | CNW_IMR_RFU1);
#endif

	/* Enable receiver */
	CNW_CMD0(sc, CNW_CMD_ER);

	/* "Set the IENA bit in COR" */
	WAIT_WOC(sc);
#ifndef MEMORY_MAPPED
	bus_space_write_1(sc->sc_iot, sc->sc_ioh, CNW_REG_COR,
	    CNW_COR_IENA | CNW_COR_LVLREQ);
#else
	bus_space_write_1(sc->sc_memt, sc->sc_memh,
	    sc->sc_memoff + CNW_IOM_OFF + CNW_REG_COR,
	    CNW_COR_IENA | CNW_COR_LVLREQ);
#endif
}


#if !defined(__FreeBSD__)
/*
 * Enable and initialize the card.
 */
int
cnw_enable(sc)
	struct cnw_softc *sc;
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;

	if ((ifp->if_flags & IFF_RUNNING) != 0)
		return (0);

	sc->sc_ih = pcmcia_intr_establish(sc->sc_pf, IPL_NET, cnw_intr, sc);
	if (sc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt handler\n",
		    sc->sc_dev.dv_xname);
		return (EIO);
	}
	if (pcmcia_function_enable(sc->sc_pf) != 0) {
		printf("%s: couldn't enable card\n", sc->sc_dev.dv_xname);
		return (EIO);
	}
	sc->sc_resource |= CNW_RES_PCIC;
	cnw_init(sc);
	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_flags |= IFF_RUNNING;
	return (0);
}


/*
 * Stop and disable the card.
 */
void
cnw_disable(sc)
	struct cnw_softc *sc;
{
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;

	if ((ifp->if_flags & IFF_RUNNING) == 0)
		return;

	pcmcia_function_disable(sc->sc_pf);
	sc->sc_resource &= ~CNW_RES_PCIC;
	pcmcia_intr_disestablish(sc->sc_pf, sc->sc_ih);
	ifp->if_flags &= ~IFF_RUNNING;
	ifp->if_timer = 0;
}

/*
 * Match the hardware we handle.
 */
int
cnw_match(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct pcmcia_attach_args *pa = aux;

	if (pa->manufacturer == PCMCIA_VENDOR_XIRCOM &&
	    pa->product == PCMCIA_PRODUCT_XIRCOM_CNW_801)
		return 1;
	if (pa->manufacturer == PCMCIA_VENDOR_XIRCOM &&
	    pa->product == PCMCIA_PRODUCT_XIRCOM_CNW_802)
		return 1;
	return 0;
}


/*
 * Attach the card.
 */
void
cnw_attach(parent, self, aux)
	struct device  *parent, *self;
	void           *aux;
{
	struct cnw_softc *sc = (void *) self;
	struct pcmcia_attach_args *pa = aux;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
	u_int8_t macaddr[ETHER_ADDR_LEN];
	int i;
	bus_size_t memsize;

	sc->sc_resource = 0;

	/* Enable the card */
	sc->sc_pf = pa->pf;
	pcmcia_function_init(sc->sc_pf, sc->sc_pf->cfe_head.sqh_first);
	if (pcmcia_function_enable(sc->sc_pf)) {
		printf(": function enable failed\n");
		return;
	}
	sc->sc_resource |= CNW_RES_PCIC;

	/* Map I/O register and "memory" */
#ifndef MEMORY_MAPPED
	if (pcmcia_io_alloc(sc->sc_pf, 0, CNW_IO_SIZE, CNW_IO_SIZE,
	    &sc->sc_pcioh) != 0) {
		printf(": can't allocate i/o space\n");
		goto fail;
	}
	if (pcmcia_io_map(sc->sc_pf, PCMCIA_WIDTH_IO16, 0,
	    CNW_IO_SIZE, &sc->sc_pcioh, &sc->sc_iowin) != 0) {
		printf(": can't map i/o space\n");
		pcmcia_io_free(sc->sc_pf, &sc->sc_pcioh);
		goto fail;
	}
	sc->sc_iot = sc->sc_pcioh.iot;
	sc->sc_ioh = sc->sc_pcioh.ioh;
	sc->sc_resource |= CNW_RES_IO;
#endif
#ifndef MEMORY_MAPPED
	memsize = CNW_MEM_SIZE;
#else
	memsize = CNW_MEM_SIZE + CNW_IOM_SIZE;
#endif
	if (pcmcia_mem_alloc(sc->sc_pf, memsize, &sc->sc_pcmemh) != 0) {
		printf(": can't allocate memory\n");
		goto fail;
	}
	if (pcmcia_mem_map(sc->sc_pf, PCMCIA_WIDTH_MEM8|PCMCIA_MEM_COMMON,
	    CNW_MEM_ADDR, memsize, &sc->sc_pcmemh, &sc->sc_memoff,
	    &sc->sc_memwin) != 0) {
		printf(": can't map memory\n");
		pcmcia_mem_free(sc->sc_pf, &sc->sc_pcmemh);
		goto fail;
	}
	sc->sc_memt = sc->sc_pcmemh.memt;
	sc->sc_memh = sc->sc_pcmemh.memh;
	sc->sc_resource |= CNW_RES_MEM;
	switch (pa->product) {
	case PCMCIA_PRODUCT_XIRCOM_CNW_801:
		printf(": %s\n", PCMCIA_STR_XIRCOM_CNW_801);
		break;
	case PCMCIA_PRODUCT_XIRCOM_CNW_802:
		printf(": %s\n", PCMCIA_STR_XIRCOM_CNW_802);
		break;
	}

	/* Finish setup of softc */
	sc->sc_domain = cnw_domain;
	sc->sc_skey = cnw_skey;

	/* Get MAC address */
	cnw_reset(sc);
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		macaddr[i] = bus_space_read_1(sc->sc_memt, sc->sc_memh,
		    sc->sc_memoff + CNW_EREG_PA + i);
	printf("%s: address %s\n", sc->sc_dev.dv_xname,
	    ether_sprintf(macaddr));

	/* Set up ifnet structure */
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_start = cnw_start;
	ifp->if_ioctl = cnw_ioctl;
	ifp->if_watchdog = cnw_watchdog;
	ifp->if_flags = IFF_BROADCAST | IFF_MULTICAST | IFF_SIMPLEX |
	    IFF_NOTRAILERS;

	/* Attach the interface */
	if_attach(ifp);
	ether_ifattach(ifp, macaddr);
#if NBPFILTER > 0
	bpfattach(&sc->sc_ethercom.ec_if.if_bpf, ifp, DLT_EN10MB,
	    sizeof(struct ether_header));
#endif
	sc->sc_resource |= CNW_RES_NET;

	ifp->if_baudrate = IF_Mbps(1);

	/* Disable the card now, and turn it on when the interface goes up */
	pcmcia_function_disable(sc->sc_pf);
	sc->sc_resource &= ~CNW_RES_PCIC;
	return;

fail:
#ifndef MEMORY_MAPPED
	if ((sc->sc_resource & CNW_RES_IO) != 0) {
		pcmcia_io_unmap(sc->sc_pf, sc->sc_iowin);
		pcmcia_io_free(sc->sc_pf, &sc->sc_pcioh);
		sc->sc_resource &= ~CNW_RES_IO;
	}
#endif
	if ((sc->sc_resource & CNW_RES_PCIC) != 0) {
		pcmcia_function_disable(sc->sc_pf);
		sc->sc_resource &= ~CNW_RES_PCIC;
	}
}
#endif /* !defined(__FreeBSD__) */

/*
 * Start outputting on the interface.
 */
void
cnw_start(ifp)
	struct ifnet *ifp;
{
	struct cnw_softc *sc = ifp->if_softc;
	struct mbuf *m0;
	int lif;
	int asr;
#ifdef ONE_AT_A_TIME
	struct timeval now;
#endif

#ifdef CNW_DEBUG
	if (sc->sc_ethercom.ec_if.if_flags & IFF_DEBUG)
		printf("%s: cnw_start\n", ifp->if_xname);
#if defined(__FreeBSD__)
	if (ifp->if_drv_flags & IFF_DRV_OACTIVE)
#else
	if (ifp->if_flags & IFF_OACTIVE)
#endif
		printf("%s: cnw_start reentered\n", ifp->if_xname);
#endif

#if defined(__FreeBSD__)
	if (sc->cnw_gone)
		return;

	ifp->if_drv_flags |= IFF_DRV_OACTIVE;
#else
	ifp->if_flags |= IFF_OACTIVE;
#endif

	for (;;) {
#ifdef ONE_AT_A_TIME
		microtime(&now);
		now.tv_sec -= sc->sc_txlast.tv_sec;
		now.tv_usec -= sc->sc_txlast.tv_usec;
		if (now.tv_usec < 0) {
			now.tv_usec += 1000000;
			now.tv_sec--;
		}

		/*
		 * Don't ship this packet out until the last
		 * packet has left the building.
		 * If we have not tried to send a packet for 1/5
		 * a second then we assume we lost an interrupt,
		 * lets go on and send the next packet anyhow.
		 *
		 * I suppose we could check to see if it is okay
		 * to put additional packets on the card (beyond
		 * the one already waiting to be sent) but I don't
		 * think we would get any improvement in speed as
		 * we should have ample time to put the next packet
		 * on while this one is going out.
		 */
		if (sc->sc_active && now.tv_sec == 0 && now.tv_usec < 200000)
			break;
#endif

		/* Make sure the link integrity field is on */
		WAIT_WOC(sc);
		lif = bus_space_read_1(sc->sc_memt, sc->sc_memh,
		    sc->sc_memoff + CNW_EREG_LIF);
		if (lif == 0) {
#ifdef CNW_DEBUG
			if (sc->sc_ethercom.ec_if.if_flags & IFF_DEBUG)
				printf("%s: link integrity %d\n", lif);
#endif
			break;
		}

		/* Is there any buffer space available on the card? */
		WAIT_WOC(sc);
#ifndef MEMORY_MAPPED
		asr = bus_space_read_1(sc->sc_iot, sc->sc_ioh, CNW_REG_ASR);
#else
		asr = bus_space_read_1(sc->sc_memt, sc->sc_memh,
		    sc->sc_memoff + CNW_IOM_OFF + CNW_REG_ASR);
#endif
		if (!(asr & CNW_ASR_TXBA)) {
#ifdef CNW_DEBUG
			if (sc->sc_ethercom.ec_if.if_flags & IFF_DEBUG)
				printf("%s: no buffer space\n", ifp->if_xname);
#endif
			break;
		}

		sc->sc_stats.nws_tx++;

		IF_DEQUEUE(&ifp->if_snd, m0);
		if (m0 == 0)
			break;

#if !defined(__FreeBSD__)
#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m0);
#endif
#else	/* FreeBSD */
		BPF_MTAP(ifp, m0);
#endif
		
		cnw_transmit(sc, m0);
		++ifp->if_opackets;
		ifp->if_timer = 3; /* start watchdog timer */

		microtime(&sc->sc_txlast);
		sc->sc_active = 1;
	}

#if defined(__FreeBSD__)
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
#else
	ifp->if_flags &= ~IFF_OACTIVE;
#endif
}

/*
 * Transmit a packet.
 */
void
cnw_transmit(sc, m)
	struct cnw_softc *sc;
	struct mbuf *m;
{
	int buffer, bufsize, bufoffset, bufptr, bufspace, len, mbytes, n;
	u_int8_t *mptr;

	/* Get buffer info from card */
	buffer = read16(sc, CNW_EREG_TDP);
	bufsize = read16(sc, CNW_EREG_TDP + 2);
	bufoffset = read16(sc, CNW_EREG_TDP + 4);
#ifdef CNW_DEBUG
	if (sc->sc_ethercom.ec_if.if_flags & IFF_DEBUG)
		printf("%s: cnw_transmit b=0x%x s=%d o=0x%x\n",
		    sc->sc_dev.dv_xname, buffer, bufsize, bufoffset);
#endif

	/* Copy data from mbuf chain to card buffers */
	bufptr = sc->sc_memoff + buffer + bufoffset;
	bufspace = bufsize;
	len = 0;
	while (m) {
		mptr = mtod(m, u_int8_t *);
		mbytes = m->m_len;
		len += mbytes;
		while (mbytes > 0) {
			if (bufspace == 0) {
				buffer = read16(sc, buffer);
				bufptr = sc->sc_memoff + buffer + bufoffset;
				bufspace = bufsize;
#ifdef CNW_DEBUG
				if (sc->sc_ethercom.ec_if.if_flags & IFF_DEBUG)
					printf("%s:   next buffer @0x%x\n",
					    sc->sc_dev.dv_xname, buffer);
#endif
			}
			n = mbytes <= bufspace ? mbytes : bufspace;
			bus_space_write_region_1(sc->sc_memt, sc->sc_memh,
			    bufptr, mptr, n);
			bufptr += n;
			bufspace -= n;
			mptr += n;
			mbytes -= n;
		}
		m = m_free(m);
	}

	/* Issue transmit command */
	CNW_CMD2(sc, CNW_CMD_TL, len, len >> 8);
}


/*
 * Pull a packet from the card into an mbuf chain.
 */
struct mbuf *
cnw_read(sc)
	struct cnw_softc *sc;
{
	struct mbuf *m, *top, **mp;
	int totbytes, buffer, bufbytes, bufptr, mbytes, n;
	u_int8_t *mptr;

	WAIT_WOC(sc);
	totbytes = read16(sc, CNW_EREG_RDP);
#ifdef CNW_DEBUG
	if (sc->sc_ethercom.ec_if.if_flags & IFF_DEBUG)
		printf("%s: recv %d bytes\n", sc->sc_dev.dv_xname, totbytes);
#endif
	buffer = CNW_EREG_RDP + 2;
	bufbytes = 0;
	bufptr = 0; /* XXX make gcc happy */

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == 0)
		return (0);
#if !defined(__FreeBSD__)
	m->m_pkthdr.rcvif = &sc->sc_ethercom.ec_if;
#else	/* FreeBSD */
	m->m_pkthdr.rcvif = sc->sc_ifp;
#endif
	m->m_pkthdr.len = totbytes;
	mbytes = MHLEN;
	top = 0;
	mp = &top;

	while (totbytes > 0) {
		if (top) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == 0) {
				m_freem(top);
				return (0);
			}
			mbytes = MLEN;
		}
		if (totbytes >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				m_free(m);
				m_freem(top);
				return (0);
			}
			mbytes = MCLBYTES;
		}
		if (!top) {
			int pad = ALIGN(sizeof(struct ether_header)) -
			    sizeof(struct ether_header);
			m->m_data += pad;
			mbytes -= pad;
		}
		mptr = mtod(m, u_int8_t *);
		mbytes = m->m_len = min(totbytes, mbytes);
		totbytes -= mbytes;
		while (mbytes > 0) {
			if (bufbytes == 0) {
				buffer = read16(sc, buffer);
				bufbytes = read16(sc, buffer + 2);
				bufptr = sc->sc_memoff + buffer +
				    read16(sc, buffer + 4);
#ifdef CNW_DEBUG
				if (sc->sc_ethercom.ec_if.if_flags & IFF_DEBUG)
					printf("%s:   %d bytes @0x%x+0x%x\n",
					    sc->sc_dev.dv_xname, bufbytes,
					    buffer, bufptr - buffer -
					    sc->sc_memoff);
#endif
			}
			n = mbytes <= bufbytes ? mbytes : bufbytes;
			bus_space_read_region_1(sc->sc_memt, sc->sc_memh,
			    bufptr, mptr, n);
			bufbytes -= n;
			bufptr += n;
			mbytes -= n;
			mptr += n;
		}
		*mp = m;
		mp = &m->m_next;
	}

	return (top);
}


/*
 * Handle received packets.
 */
void
cnw_recv(sc)
	struct cnw_softc *sc;
{
	int rser;
#if !defined(__FreeBSD__)
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
#else
	struct ifnet *ifp = sc->sc_ifp;
#endif
	struct mbuf *m;

	for (;;) {
		WAIT_WOC(sc);
		rser = bus_space_read_1(sc->sc_memt, sc->sc_memh,
		    sc->sc_memoff + CNW_EREG_RSER);
		if (!(rser & CNW_RSER_RXAVAIL))
			return;

		/* Pull packet off card */
		m = cnw_read(sc);

		/* Acknowledge packet */
		CNW_CMD0(sc, CNW_CMD_SRP);

		/* Did we manage to get the packet from the interface? */
		if (m == 0) {
			++ifp->if_ierrors;
			return;
		}
		++ifp->if_ipackets;

#if !defined(__FreeBSD__)
#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif
#endif

		/* Pass the packet up. */
		(*ifp->if_input)(ifp, m);
	}
}


/*
 * Interrupt handler.
 */
#if !defined(__FreeBSD__)
int
#else
void
#endif
cnw_intr(arg)
	void *arg;
{
	struct cnw_softc *sc = arg;
#if !defined(__FreeBSD__)
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;
#else
	struct ifnet *ifp = sc->sc_ifp;
#endif
	int ret, status, rser, tser;


#if !defined(__FreeBSD__)
	if ((sc->sc_ethercom.ec_if.if_flags & IFF_RUNNING) == 0 ||
	    (sc->sc_dev.dv_flags & DVF_ACTIVE) == 0)
		return (0);
#else
	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return;
#endif
	ifp->if_timer = 0;	/* stop watchdog timer */

	ret = 0;
	for (;;) {
		WAIT_WOC(sc);
#ifndef MEMORY_MAPPED
		status = bus_space_read_1(sc->sc_iot, sc->sc_ioh,
		    CNW_REG_CCSR);
#else
		status = bus_space_read_1(sc->sc_memt, sc->sc_memh,
		    sc->sc_memoff + CNW_IOM_OFF + CNW_REG_CCSR);
#endif

#if !defined(__FreeBSD__)
		if (!(status & 0x02)) {
			if (ret == 0)
				printf("%s: spurious interrupt\n",
				    sc->sc_dev.dv_xname);
			return (ret);
		}
#else
		if (!(status & 0x02)) {
			if (ret == 0)
				device_printf(sc->dev, "spurious interrupt\n");
			return;
		}
#endif
		ret = 1;

#ifndef MEMORY_MAPPED
		status = bus_space_read_1(sc->sc_iot, sc->sc_ioh, CNW_REG_ASR);
#else
		status = bus_space_read_1(sc->sc_memt, sc->sc_memh,
		    sc->sc_memoff + CNW_IOM_OFF + CNW_REG_ASR);
#endif

		/* Anything to receive? */
		if (status & CNW_ASR_RXRDY) {
			sc->sc_stats.nws_rx++;
			cnw_recv(sc);
		}

		/* Receive error */
		if (status & CNW_ASR_RXERR) {
			/*
			 * I get a *lot* of spurious receive errors
			 * (many per second), even when the interface
			 * is quiescent, so we don't increment
			 * if_ierrors here.
			 */
			rser = bus_space_read_1(sc->sc_memt, sc->sc_memh,
			    sc->sc_memoff + CNW_EREG_RSER);

			/* RX statistics */
			sc->sc_stats.nws_rxerr++;
			if (rser & CNW_RSER_RXBIG)
				sc->sc_stats.nws_rxframe++;
			if (rser & CNW_RSER_RXCRC)
				sc->sc_stats.nws_rxcrcerror++;
			if (rser & CNW_RSER_RXOVERRUN)
				sc->sc_stats.nws_rxoverrun++;
			if (rser & CNW_RSER_RXOVERFLOW)
				sc->sc_stats.nws_rxoverflow++;
			if (rser & CNW_RSER_RXERR)
				sc->sc_stats.nws_rxerrors++;
			if (rser & CNW_RSER_RXAVAIL)
				sc->sc_stats.nws_rxavail++;

			/* Clear error bits in RSER */
			WAIT_WOC(sc);
			bus_space_write_1(sc->sc_memt, sc->sc_memh,
			    sc->sc_memoff + CNW_EREG_RSERW,
			    CNW_RSER_RXERR |
			    (rser & (CNW_RSER_RXCRC | CNW_RSER_RXBIG)));
			/* Clear RXERR in ASR */
			WAIT_WOC(sc);
			bus_space_write_1(sc->sc_memt, sc->sc_memh,
			    sc->sc_memoff + CNW_EREG_ASCC, CNW_ASR_RXERR);
		}

		/* Transmit done */
		if (status & CNW_ASR_TXDN) {
			tser = bus_space_read_1(sc->sc_memt, sc->sc_memh,
						CNW_EREG_TSER);

			/* TX statistics */
			if (tser & CNW_TSER_TXERR)
				sc->sc_stats.nws_txerrors++;
			if (tser & CNW_TSER_TXNOAP)
				sc->sc_stats.nws_txlostcd++;
			if (tser & CNW_TSER_TXGU)
				sc->sc_stats.nws_txabort++;

			if (tser & CNW_TSER_TXOK) {
				sc->sc_stats.nws_txokay++;
				sc->sc_stats.nws_txretries[status & 0xf]++;
				WAIT_WOC(sc);
				bus_space_write_1(sc->sc_memt, sc->sc_memh,
				    sc->sc_memoff + CNW_EREG_TSERW,
				    CNW_TSER_TXOK | CNW_TSER_RTRY);
			}

			if (tser & CNW_TSER_ERROR) {
				++ifp->if_oerrors;
				WAIT_WOC(sc);
				bus_space_write_1(sc->sc_memt, sc->sc_memh,
				    sc->sc_memoff + CNW_EREG_TSERW,
				    (tser & CNW_TSER_ERROR) |
				    CNW_TSER_RTRY);
			}

			sc->sc_active = 0;
#if defined(__FreeBSD__)
			ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
#else
			ifp->if_flags &= ~IFF_OACTIVE;
#endif

			/* Continue to send packets from the queue */
#if !defined(__FreeBSD__)
			cnw_start(&sc->sc_ethercom.ec_if);
#else
			cnw_start(ifp);
#endif
		}
				
	}
}


/*
 * Handle device ioctls.
 */
int
cnw_ioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct cnw_softc *sc = ifp->if_softc;
#if !defined(__FreeBSD__)
	struct ifaddr *ifa = (struct ifaddr *)data;
#endif
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;
#if __FreeBSD__ >= 5
	struct thread *td = curthread;	/* XXX */
#else
	struct proc *td = curproc;	/*XXX*/
#endif

	s = splnet();

#if defined(__FreeBSD__)
	if (sc->cnw_gone) {
		splx(s);
		return(ENODEV);
	}
#endif

	switch (cmd) {

	case SIOCSIFADDR:
#if !defined(__FreeBSD__)
		if (!(ifp->if_flags & IFF_RUNNING) &&
		    (error = cnw_enable(sc)) != 0)
			break;
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			cnw_init(sc);
			arp_ifinit(&sc->sc_ethercom.ec_if, ifa);
			break;
#endif
		default:
			cnw_init(sc);
			break;
		}
#else
		error = ether_ioctl(ifp, cmd, data);
#endif
		break;

	case SIOCSIFFLAGS:
#if !defined(__FreeBSD__)
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) == IFF_RUNNING) {
			/*
			 * The interface is marked down and it is running, so
			 * stop it.
			 */
			cnw_disable(sc);
		} else if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) == IFF_UP){
			/*
			 * The interface is marked up and it is stopped, so
			 * start it.
			 */
			error = cnw_enable(sc);
		} else {
			/* IFF_PROMISC may be changed */
			cnw_init(sc);
		}
#else
		if (ifp->if_flags & IFF_UP) {
				cnw_freebsd_init(sc);
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				cnw_stop(sc);
			} else {
				cnw_freebsd_init(sc);
			}
		}
		
#endif
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
#if !defined(__FreeBSD__)
		/* Update our multicast list. */
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->sc_ethercom) :
		    ether_delmulti(ifr, &sc->sc_ethercom);
		if (error == ENETRESET || error == 0) {
			cnw_init(sc);
			error = 0;
		}
#else
		/* XXX */
		error = 0;
#endif
		break;

	case SIOCGCNWDOMAIN:
		((struct ifreq *)data)->ifr_domain = sc->sc_domain;
		break;

	case SIOCSCNWDOMAIN:
#if !defined(__FreeBSD__)
		error = suser(p->p_ucred, &p->p_acflag);
#else
		error = suser(td);
#endif
		if (error)
			break;
		error = cnw_setdomain(sc, ifr->ifr_domain);
		break;

	case SIOCSCNWKEY:
#if !defined(__FreeBSD__)
		error = suser(p->p_ucred, &p->p_acflag);
#else
		error = suser(td);
#endif
		if (error)
			break;
		error = cnw_setkey(sc, (int)ifr->ifr_key);
		break;

	case SIOCGCNWSTATUS:
#if !defined(__FreeBSD__)
		error = suser(p->p_ucred, &p->p_acflag);
#else
		error = suser(td);
#endif
		if (error)
			break;
#if !defined(__FreeBSD__)
		if ((ifp->if_flags & IFF_RUNNING) == 0)
#else
		if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
#endif
			break;
		bus_space_read_region_1(sc->sc_memt, sc->sc_memh,
		    sc->sc_memoff + CNW_EREG_CB,
		    ((struct cnwstatus *)data)->data,
		    sizeof(((struct cnwstatus *)data)->data));
		break;

	case SIOCGCNWSTATS:
		bcopy((void *)&sc->sc_stats,
		    (void *)&(((struct cnwistats *)data)->stats),
		    sizeof(struct cnwstats));
			break;

	default:
		error = EINVAL;
		break;
	}

	splx(s);
	return (error);
}


/*
 * Device timeout/watchdog routine. Entered if the device neglects to
 * generate an interrupt after a transmit has been started on it.
 */
void
cnw_watchdog(ifp)
	struct ifnet *ifp;
{
	struct cnw_softc *sc = ifp->if_softc;

#if !defined(__FreeBSD__)
	printf("%s: device timeout; card reset\n", sc->sc_dev.dv_xname);
	++ifp->if_oerrors;
	cnw_init(sc);
#else
	device_printf(sc->dev, "device timeout; card reset\n");
	++ifp->if_oerrors;
	cnw_freebsd_init(sc);
#endif
}

int
cnw_setdomain(sc, domain)
	struct cnw_softc *sc;
	int domain;
{
	int s;

	if (domain & ~0x1ff)
		return EINVAL;

	s = splnet();
	CNW_CMD2(sc, CNW_CMD_SMD, domain, domain >> 8);
	splx(s);

	sc->sc_domain = domain;
	return 0;
}

int
cnw_setkey(sc, key)
	struct cnw_softc *sc;
	int key;
{
	int s;

	if (key & ~0xffff)
		return EINVAL;

	s = splnet();
	CNW_CMD2(sc, CNW_CMD_SSK, key, key >> 8);
	splx(s);

	sc->sc_skey = key;
	return 0;
}

#if !defined(__FreeBSD__)
int
cnw_activate(self, act)
	struct device *self;
	enum devact act;
{
	struct cnw_softc *sc = (struct cnw_softc *)self;
	int rv = 0, s;

	s = splnet();
	switch (act) {
	case DVACT_ACTIVATE:
		rv = EOPNOTSUPP;
		break;

	case DVACT_DEACTIVATE:
		if_deactivate(&sc->sc_ethercom.ec_if);
		break;
	}
	splx(s);
	return (rv);
}

int
cnw_detach(self, flags)
	struct device *self;
	int flags;
{
	struct cnw_softc *sc = (struct cnw_softc *)self;
	struct ifnet *ifp = &sc->sc_ethercom.ec_if;

	/* cnw_disable() checks IFF_DRV_RUNNING */
	cnw_disable(sc);

	if ((sc->sc_resource & CNW_RES_NET) != 0) {
#if NBPFILTER > 0
		bpfdetach(ifp);
#endif
		ether_ifdetach(ifp);
		if_detach(ifp);
	}

#ifndef MEMORY_MAPPED
	/* unmap and free our i/o windows */
	if ((sc->sc_resource & CNW_RES_IO) != 0) {
		pcmcia_io_unmap(sc->sc_pf, sc->sc_iowin);
		pcmcia_io_free(sc->sc_pf, &sc->sc_pcioh);
	}
#endif

	/* unmap and free our memory windows */
	if ((sc->sc_resource & CNW_RES_MEM) != 0) {
		pcmcia_mem_unmap(sc->sc_pf, sc->sc_memwin);
		pcmcia_mem_free(sc->sc_pf, &sc->sc_pcmemh);
	}

	return (0);
}
#endif

#if defined(__FreeBSD__)
static void cnw_freebsd_init(xsc)
	void	*xsc;
{
	struct cnw_softc	*sc = xsc;
	struct ifnet *ifp = sc->sc_ifp;
	int s;

	if (sc->cnw_gone)
		return;

	s = splimp();
	cnw_init(sc);

#if 0
	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		cnw_stop(sc);
#endif

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

/*	sc->cnw_stat_ch = timeout(cnw_inquire, sc, hz * 60); */

	cnw_start(ifp);

	splx(s);

	return;
}

static void cnw_stop(sc)
	struct cnw_softc	*sc;
{
	struct ifnet		*ifp;

	if (sc->cnw_gone)
		return;

	cnw_reset(sc);

	ifp = sc->sc_ifp;
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	return;
}

static int cnw_pccard_probe(dev)
	device_t	dev;
{
	struct cnw_softc	*sc;
	int		error;

	sc = device_get_softc(dev);
	sc->cnw_gone = 0;

	error = cnw_alloc(dev);
	if (error)
		return (error);

	device_set_desc(dev, "Netwave AirSurfer Wireless LAN");
	cnw_free(dev);

	return (0);
}

static int cnw_pccard_detach(dev)
	device_t		dev;
{
	struct cnw_softc	*sc;
	struct ifnet		*ifp;
#if 0
	int			s;

	s = splimp();
#endif

	sc = device_get_softc(dev);
	ifp = sc->sc_ifp;

	if (sc->cnw_gone) {
		device_printf(dev, "already unloaded\n");
		return(ENODEV);
	}

	cnw_stop(sc);

	ether_ifdetach(ifp);
	cnw_free(dev);
	if_free(ifp);
	sc->cnw_gone = 1;

#if 0
	splx(s);
#endif
	return(0);
}

static int cnw_pccard_attach(device_t dev)
{
	struct cnw_softc		*sc;
	struct ifnet		*ifp;
	int			i, error;
	u_char			eaddr[6];

	sc = device_get_softc(dev);
	ifp = sc->sc_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "if_alloc() failed\n");
		return (ENOSPC);
	}


	error = cnw_alloc(dev);
	if (error) {
		device_printf(dev, "cnw_alloc() failed! (%d)\n", error);
		if_free(ifp);
		return (error);
	}

	error = bus_setup_intr(dev, sc->irq, INTR_TYPE_NET,
			       cnw_intr, sc, &sc->cnw_intrhand);

	if (error) {
		device_printf(dev, "bus_setup_intr() failed! (%d)\n", error);
		cnw_free(dev);
		if_free(ifp);
		return (error);
	}

	/* Set initial values */
	sc->sc_domain = cnw_domain;
	sc->sc_skey = cnw_skey;

	/* Reset the NIC. */
	cnw_reset(sc);

	/* Get MAC address */
	for (i=0; i< ETHER_ADDR_LEN; i++) {
		eaddr[i] = bus_space_read_1(sc->sc_memt, sc->sc_memh,
				sc->sc_memoff + CNW_EREG_PA + i);
	}

	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_timer = 0;
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = (IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST
	    | IFF_NEEDSGIANT);
	ifp->if_ioctl = cnw_ioctl;
	ifp->if_start = cnw_start;
/*	ifp->if_watchdog = 0; */
	ifp->if_watchdog = cnw_watchdog;
	ifp->if_init = cnw_freebsd_init;
	ifp->if_baudrate = 1 * 1000* 1000;
	ifp->if_snd.ifq_maxlen = IFQ_MAXLEN;

	cnw_freebsd_init(sc);
	cnw_stop(sc);

	/*
	 * Call MI attach routine.
	 */
	ether_ifattach(ifp, eaddr);
/*	callout_handle_init(&sc->cnw_stat_ch); */

	return(0);
}

static void cnw_shutdown(dev)
	device_t		dev;
{
	struct cnw_softc	*sc;

	sc = device_get_softc(dev);
	cnw_stop(sc);

	return;
}

static int cnw_alloc(dev)
	device_t		dev;
{
	struct cnw_softc	*sc = device_get_softc(dev);
	int			rid;
	int error;

	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
					     RF_ACTIVE);
	if (!sc->mem_res) {
		device_printf(dev, "Cannot allocate attribute memory\n");
		return (ENOMEM);
	}
	sc->sc_memt = rman_get_bustag(sc->mem_res);
	sc->sc_memh = rman_get_bushandle(sc->mem_res);


	error = CARD_SET_MEMORY_OFFSET(device_get_parent(dev),
	    dev, rid, CNW_MEM_ADDR, NULL);
	if (error) {
		device_printf(dev,
			"CARD_SET_MEMORY_OFFSET returned 0x%0x", error);
		return(error);
	}

	error = CARD_SET_RES_FLAGS(device_get_parent(dev), dev,
			SYS_RES_MEMORY, rid, PCCARD_A_MEM_8BIT);
	if (error) {
		device_printf(dev,
			"CARD_SET_RES_FLAGS returned 0x%0x\n", error);
		return (error);
	}

	rid = 0;
	sc->irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid, RF_ACTIVE);
	if (!sc->irq) {
		device_printf(dev, "No irq?!\n");
		return (ENXIO);
	}

	sc->dev = dev;
	sc->sc_memoff = 0;
	
	return (0);
}

static void cnw_free(dev)
	device_t		dev;
{
	struct cnw_softc	*sc = device_get_softc(dev);

	if (sc->mem_res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->mem_res);
		sc->mem_res = 0;
	}
	if (sc->irq != NULL) {
		bus_teardown_intr(dev, sc->irq, sc->cnw_intrhand);
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->irq);
		sc->irq = 0;
	}

	return;
}

#endif /* FreeBSD */
