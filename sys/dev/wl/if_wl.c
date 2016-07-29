/*-
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain all copyright 
 *    notices, this list of conditions and the following disclaimer.
 * 2. The names of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */
/*
 * if_wl.c - original MACH, then BSDI ISA wavelan driver
 *	ported to mach by Anders Klemets
 *	to BSDI by Robert Morris
 *	to FreeBSD by Jim Binkley
 *      to FreeBSD 2.2+ by Michael Smith
 *
 * 2.2 update:
 * Changed interface to match 2.1-2.2 differences.
 * Implement IRQ selection logic in wlprobe()
 * Implement PSA updating.
 * Pruned heading comments for relevance.
 * Ripped out all the 'interface counters' cruft.
 * Cut the missing-interrupt timer back to 100ms.
 * 2.2.1 update:
 * now supports all multicast mode (mrouted will work),
 *	but unfortunately must do that by going into promiscuous mode
 * NWID sysctl added so that normally promiscuous mode is NWID-specific
 *	but can be made NWID-inspecific
 *			7/14/97 jrb
 *
 * Work done:
 * Ported to FreeBSD, got promiscuous mode working with bpfs,
 * and rewired timer routine.  The i82586 will hang occasionally on output 
 * and the watchdog timer will kick it if so and log an entry.
 * 2 second timeout there.  Apparently the chip loses an interrupt.
 * Code borrowed from if_ie.c for watchdog timer.
 *
 * The wavelan card is a 2mbit radio modem that emulates ethernet;
 * i.e., it uses MAC addresses.  This should not be a surprise since
 * it uses an ethernet controller as a major hw item.
 * It can broadcast, unicast or apparently multicast in a base cell 
 * using an omni-directional antennae that is 
 * about 800 feet around the base cell barring walls and metal.  
 * With directional antennae, it can be used point to point over a mile
 * or so apparently (haven't tried that).
 *
 * There are ISA and pcmcia versions (not supported by this code).
 * The ISA card has an Intel 82586 lan controller on it.  It consists
 * of 2 pieces of hw, the lan controller (intel) and a radio-modem.
 * The latter has an extra set of controller registers that has nothing
 * to do with the i82586 and allows setting and monitoring of radio
 * signal strength, etc.  There is a nvram area called the PSA that
 * contains a number of setup variables including the IRQ and so-called
 * NWID or Network ID.  The NWID must be set the same for all radio
 * cards to communicate (unless you are using the ATT/NCR roaming feature
 * with their access points.  There is no support for that here. Roaming
 * involves a link-layer beacon sent out from the access points.  End
 * stations monitor the signal strength and only use the strongest
 * access point).  This driver assumes that the base ISA port, IRQ, 
 * and NWID are first set in nvram via the dos-side "instconf.exe" utility 
 * supplied with the card. This driver takes the ISA port from 
 * the kernel configuration setup, and then determines the IRQ either 
 * from the kernel config (if an explicit IRQ is set) or from the 
 * PSA on the card if not.
 * The hw also magically just uses the IRQ set in the nvram.
 * The NWID is used magically as well by the radio-modem
 * to determine which packets to keep or throw out.  
 *
 * sample config:
 *
 * device wl0 at isa? port 0x300 net irq ?
 *
 * Ifdefs:
 * 1. WLDEBUG. (off) - if turned on enables IFF_DEBUG set via ifconfig debug
 * 2. MULTICAST (on) - turned on and works up to and including mrouted
 * 3. WLCACHE (off) -  define to turn on a signal strength 
 * (and other metric) cache that is indexed by sender MAC address.  
 * Apps can read this out to learn the remote signal strength of a 
 * sender.  Note that it has a switch so that it only stores 
 * broadcast/multicast senders but it could be set to store unicast 
 * too only.  Size is hardwired in if_wl_wavelan.h
 *
 * one further note: promiscuous mode is a curious thing.  In this driver,
 * promiscuous mode apparently CAN catch ALL packets and ignore the NWID
 * setting.  This is probably more useful in a sense (for snoopers) if
 * you are interested in all traffic as opposed to if you are interested
 * in just your own.  There is a driver specific sysctl to turn promiscuous
 * from just promiscuous to wildly promiscuous...
 *
 * This driver also knows how to load the synthesizers in the 2.4 Gz
 * ISA Half-card, Product number 847647476 (USA/FCC IEEE Channel set).
 * This product consists of a "mothercard" that contains the 82586,
 * NVRAM that holds the PSA, and the ISA-buss interface custom ASIC. 
 * The radio transceiver is a "daughtercard" called the WaveMODEM which
 * connects to the mothercard through two single-inline connectors: a
 * 20-pin connector provides DC-power and modem signals, and a 3-pin
 * connector which exports the antenna connection. The code herein
 * loads the receive and transmit synthesizers and the corresponding
 * transmitter output power value from an EEPROM controlled through
 * additional registers via the MMC. The EEPROM address selected
 * are those whose values are preset by the DOS utility programs
 * provided with the product, and this provides compatible operation
 * with the DOS Packet Driver software. A future modification will
 * add the necessary functionality to this driver and to the wlconfig
 * utility to completely replace the DOS Configuration Utilities.
 * The 2.4 Gz WaveMODEM is described in document number 407-024692/E,
 * and is available through Lucent Technologies OEM supply channels.
 * --RAB 1997/06/08.
 */

#define MULTICAST  1

/* 
 *	Olivetti PC586 Mach Ethernet driver v1.0
 *	Copyright Ing. C. Olivetti & C. S.p.A. 1988, 1989
 *	All rights reserved.
 *
 */ 
/*
  Copyright 1988, 1989 by Olivetti Advanced Technology Center, Inc.,
Cupertino, California.

		All Rights Reserved

  Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Olivetti
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

  OLIVETTI DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL OLIVETTI BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUR OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
/*
  Copyright 1988, 1989 by Intel Corporation, Santa Clara, California.

		All Rights Reserved

Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appears in all
copies and that both the copyright notice and this permission notice
appear in supporting documentation, and that the name of Intel
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

INTEL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
IN NO EVENT SHALL INTEL BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * NOTE:
 *		by rvb:
 *  1.	The best book on the 82586 is:
 *		LAN Components User's Manual by Intel
 *	The copy I found was dated 1984.  This really tells you
 *	what the state machines are doing
 *  2.	In the current design, we only do one write at a time,
 *	though the hardware is capable of chaining and possibly
 *	even batching.  The problem is that we only make one
 *	transmit buffer available in sram space.
 */

#include "opt_wavelan.h"
#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/priv.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <sys/sysctl.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#include <net/bpf.h>
#include <isa/isavar.h>

/* was 1000 in original, fed to DELAY(x) */
#define DELAYCONST	1000
#include <dev/wl/if_wl_i82586.h>	/* Definitions for the Intel chip */
#include <dev/wl/if_wl.h>
#include <machine/if_wl_wavelan.h>

static char	t_packet[ETHERMTU + sizeof(struct ether_header) + sizeof(long)];

struct wl_softc {
    device_t	dev;
    struct	ifnet	*ifp;
    u_char	psa[0x40];
    u_char	nwid[2];	/* current radio modem nwid */
    int		flags;
    int		tbusy;		/* flag to determine if xmit is busy */
    u_short	begin_fd;
    u_short	end_fd;
    u_short	end_rbd;
    u_short	hacr;		/* latest host adapter CR command */
    short	mode;
    u_char      chan24;         /* 2.4 Gz: channel number/EEPROM Area # */
    u_short     freq24;         /* 2.4 Gz: resulting frequency  */
    int		rid_ioport;
    int		rid_irq;
    struct resource	*res_ioport;
    struct resource	*res_irq;
    void		*intr_cookie;
    struct mtx		wl_mtx;
    struct callout	watchdog_timer;
#ifdef WLCACHE
    int 	w_sigitems;     /* number of cached entries */
    /*  array of cache entries */
    struct w_sigcache w_sigcache[ MAXCACHEITEMS ];            
    int w_nextcache;            /* next free cache entry */    
    int w_wrapindex;   		/* next "free" cache entry */
#endif
};

#define WL_LOCK(_sc)	mtx_lock(&(_sc)->wl_mtx)
#define WL_LOCK_ASSERT(_sc)	mtx_assert(&(_sc)->wl_mtx, MA_OWNED)
#define WL_UNLOCK(_sc)	mtx_unlock(&(_sc)->wl_mtx)

static int	wlprobe(device_t);
static int	wlattach(device_t);
static int	wldetach(device_t);

static device_method_t wl_methods[] = {
	DEVMETHOD(device_probe,		wlprobe),
	DEVMETHOD(device_attach,	wlattach),
	DEVMETHOD(device_detach,	wldetach),
	{ 0, 0}
};

static driver_t wl_driver = {
	"wl",
	wl_methods,
	sizeof (struct wl_softc)
};

devclass_t wl_devclass;
DRIVER_MODULE(wl, isa, wl_driver, wl_devclass, 0, 0);
MODULE_DEPEND(wl, isa, 1, 1, 1);
MODULE_DEPEND(wl, ether, 1, 1, 1);

static struct isa_pnp_id wl_ids[] = {
	{0,		NULL}
};

/*
 * XXX  The Wavelan appears to be prone to dropping stuff if you talk to
 * it too fast.  This disgusting hack inserts a delay after each packet
 * is queued which helps avoid this behaviour on fast systems.
 */
static int	wl_xmit_delay = 250;
SYSCTL_INT(_machdep, OID_AUTO, wl_xmit_delay, CTLFLAG_RW, &wl_xmit_delay, 0, "");

/* 
 * not XXX, but ZZZ (bizarre).
 * promiscuous mode can be toggled to ignore NWIDs.  By default,
 * it does not.  Caution should be exercised about combining
 * this mode with IFF_ALLMULTI which puts this driver in
 * promiscuous mode.
 */
static int	wl_ignore_nwid = 0;
SYSCTL_INT(_machdep, OID_AUTO, wl_ignore_nwid, CTLFLAG_RW, &wl_ignore_nwid, 0, "");

/*
 * Emit diagnostics about transmission problems
 */
static int	xmt_watch = 0;
SYSCTL_INT(_machdep, OID_AUTO, wl_xmit_watch, CTLFLAG_RW, &xmt_watch, 0, "");

/*
 * Collect SNR statistics
 */
static int	gathersnr = 0;
SYSCTL_INT(_machdep, OID_AUTO, wl_gather_snr, CTLFLAG_RW, &gathersnr, 0, "");

static int	wl_allocate_resources(device_t device);
static int	wl_deallocate_resources(device_t device);
static void	wlstart(struct ifnet *ifp);
static void	wlstart_locked(struct ifnet *ifp);
static void	wlinit(void *xsc);
static void	wlinit_locked(struct wl_softc *sc);
static int	wlioctl(struct ifnet *ifp, u_long cmd, caddr_t data);
static void	wlwatchdog(void *arg);
static void	wlintr(void *arg);
static void	wlxmt(struct wl_softc *sc, struct mbuf *m);
static int	wldiag(struct wl_softc *sc); 
static int	wlconfig(struct wl_softc *sc); 
static int	wlcmd(struct wl_softc *sc, char *str);
static void	wlmmcstat(struct wl_softc *sc);
static u_short	wlbldru(struct wl_softc *sc);
static u_short	wlmmcread(struct wl_softc *sc, u_short reg);
static void	wlinitmmc(struct wl_softc *sc);
static int	wlhwrst(struct wl_softc *sc);
static void	wlrustrt(struct wl_softc *sc);
static void	wlbldcu(struct wl_softc *sc);
static int	wlack(struct wl_softc *sc);
static int	wlread(struct wl_softc *sc, u_short fd_p);
static void	getsnr(struct wl_softc *sc);
static void	wlrcv(struct wl_softc *sc);
static int	wlrequeue(struct wl_softc *sc, u_short fd_p);
static void	wlsftwsleaze(u_short *countp, u_char **mb_pp, struct mbuf **tm_pp, struct wl_softc *sc);
static void	wlhdwsleaze(u_short *countp, u_char **mb_pp, struct mbuf **tm_pp, struct wl_softc *sc);
#ifdef WLDEBUG
static void	wltbd(struct wl_softc *sc);
#endif
static void	wlgetpsa(struct wl_softc *sc, u_char *buf);
static void	wlsetpsa(struct wl_softc *sc);
static u_short	wlpsacrc(u_char *buf);
static void	wldump(struct wl_softc *sc);
#ifdef WLCACHE
static void	wl_cache_store(struct wl_softc *, struct ether_header *, struct mbuf *);
static void     wl_cache_zero(struct wl_softc *sc);
#endif

/* array for maping irq numbers to values for the irq parameter register */
static int irqvals[16] = { 
    0, 0, 0, 0x01, 0x02, 0x04, 0, 0x08, 0, 0, 0x10, 0x20, 0x40, 0, 0, 0x80 
};

/*
 * wlprobe:
 *
 *	This function "probes" or checks for the WaveLAN board on the bus to
 *	see if it is there.  As far as I can tell, the best break between this
 *	routine and the attach code is to simply determine whether the board
 *	is configured in properly.  Currently my approach to this is to write
 *	and read a word from the SRAM on the board being probed.  If the word
 *	comes back properly then we assume the board is there.  The config
 *	code expects to see a successful return from the probe routine before
 *	attach will be called.
 *
 * input	: address device is mapped to, and unit # being checked
 * output	: a '1' is returned if the board exists, and a 0 otherwise
 *
 */
static int
wlprobe(device_t device)
{
    struct wl_softc	*sc;
    char		*str = "wl%d: board out of range [0..%d]\n";
    u_char		inbuf[100];
    rman_res_t		junk, sirq;
    int			error, irq;

    error = ISA_PNP_PROBE(device_get_parent(device), device, wl_ids);
    if (error == ENXIO || error == 0)
	return (error);

    sc = device_get_softc(device);
    error = wl_allocate_resources(device);
    if (error)
	goto errexit;

    /* TBD. not true.
     * regular CMD() will not work, since no softc yet 
     */
#define PCMD(sc, hacr) WL_WRITE_2((sc), HACR, (hacr))

    PCMD(sc, HACR_RESET);			/* reset the board */
    DELAY(DELAYCONST);				/* >> 4 clocks at 6MHz */
    PCMD(sc, HACR_RESET);			/* reset the board */
    DELAY(DELAYCONST);	                	/* >> 4 clocks at 6MHz */

    /* clear reset command and set PIO#1 in autoincrement mode */
    PCMD(sc, HACR_DEFAULT);
    PCMD(sc, HACR_DEFAULT);
    WL_WRITE_2(sc, PIOR1, 0);			/* go to beginning of RAM */
    WL_WRITE_MULTI_2(sc, PIOP1, str, strlen(str)/2+1);	/* write string */
    
    WL_WRITE_2(sc, PIOR1, 0);			/* rewind */
    WL_READ_MULTI_2(sc, PIOP1, inbuf, strlen(str)/2+1);	/* read result */
    
    if (bcmp(str, inbuf, strlen(str))) {
	error = ENXIO;
	goto errexit;
    }

    sc->chan24 = 0;                             /* 2.4 Gz: config channel */
    sc->freq24 = 0;                             /* 2.4 Gz: frequency    */

    /* read the PSA from the board into temporary storage */
    wlgetpsa(sc, inbuf);
    
    /* We read the IRQ value from the PSA on the board. */
    for (irq = 15; irq >= 0; irq--)
	if (irqvals[irq] == inbuf[WLPSA_IRQNO])
	    break;
    if ((irq == 0) || (irqvals[irq] == 0)){
	device_printf(device, "PSA corrupt (invalid IRQ value)\n");
    } else {
	/*
	 * If the IRQ requested by the PSA is already claimed by another
	 * device, the board won't work, but the user can still access the
	 * driver to change the IRQ.
	 */
	if (bus_get_resource(device, SYS_RES_IRQ, 0, &sirq, &junk))
	    goto errexit;
	if (irq != (int)sirq)
	    device_printf(device, "board is configured for interrupt %d\n",
		irq);
    }
    wl_deallocate_resources(device);
    return (0);

errexit:
    wl_deallocate_resources(device);
    return (error);
}

/*
 * wlattach:
 *
 *	This function attaches a WaveLAN board to the "system".  The rest of
 *	runtime structures are initialized here (this routine is called after
 *	a successful probe of the board).  Once the ethernet address is read
 *	and stored, the board's ifnet structure is attached and readied.
 *
 * input	: isa_dev structure setup in autoconfig
 * output	: board structs and ifnet is setup
 *
 */
static int
wlattach(device_t device)
{
    struct wl_softc	*sc;
    int			error, i, j;
    struct ifnet	*ifp;
    u_char		eaddr[6];

    sc = device_get_softc(device);
    sc->dev = device;
    ifp = sc->ifp = if_alloc(IFT_ETHER);
    if (ifp == NULL) {
	device_printf(device, "can not if_alloc()\n");
	return (ENOSPC);
    }

    mtx_init(&sc->wl_mtx, device_get_nameunit(device), MTX_NETWORK_LOCK,
	MTX_DEF);
    callout_init_mtx(&sc->watchdog_timer, &sc->wl_mtx, 0);

    error = wl_allocate_resources(device);
    if (error) {
	wl_deallocate_resources(device);
	return (ENXIO);
    }

#ifdef WLDEBUG
    printf("wlattach: base %jx, unit %d\n", rman_get_start(sc->res_ioport),
	device_get_unit(device));
#endif

    sc->flags = 0;
    sc->mode = 0;
    sc->hacr = HACR_RESET;
    CMD(sc);				/* reset the board */
    DELAY(DELAYCONST);	                /* >> 4 clocks at 6MHz */
	
    /* clear reset command and set PIO#2 in parameter access mode */
    sc->hacr = (HACR_DEFAULT & ~HACR_16BITS);
    CMD(sc);

    /* Read the PSA from the board for our later reference */
    wlgetpsa(sc, sc->psa);

    /* fetch NWID */
    sc->nwid[0] = sc->psa[WLPSA_NWID];
    sc->nwid[1] = sc->psa[WLPSA_NWID+1];
    
    /* fetch MAC address - decide which one first */
    if (sc->psa[WLPSA_MACSEL] & 1)
	j = WLPSA_LOCALMAC;
    else
	j = WLPSA_UNIMAC;
    for (i=0; i < WAVELAN_ADDR_SIZE; ++i)
	eaddr[i] = sc->psa[j + i];

    /* enter normal 16 bit mode operation */
    sc->hacr = HACR_DEFAULT;
    CMD(sc);

    wlinitmmc(sc);
    WL_WRITE_2(sc, PIOR1, OFFSET_SCB + 8);	/* address of scb_crcerrs */
    WL_WRITE_2(sc, PIOP1, 0);			/* clear scb_crcerrs */
    WL_WRITE_2(sc, PIOP1, 0);			/* clear scb_alnerrs */
    WL_WRITE_2(sc, PIOP1, 0);			/* clear scb_rscerrs */
    WL_WRITE_2(sc, PIOP1, 0);			/* clear scb_ovrnerrs */

    ifp->if_softc = sc;
    ifp->if_mtu = WAVELAN_MTU;
    ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX;
#ifdef    WLDEBUG
    ifp->if_flags |= IFF_DEBUG;
#endif
#if	MULTICAST
    ifp->if_flags |= IFF_MULTICAST;
#endif	/* MULTICAST */
    if_initname(ifp, device_get_name(device), device_get_unit(device));
    ifp->if_init = wlinit;
    ifp->if_start = wlstart;
    ifp->if_ioctl = wlioctl;
    ifp->if_snd.ifq_maxlen = ifqmaxlen;
    /* no entries
       ifp->if_done
       ifp->if_reset
       */
    ether_ifattach(ifp, eaddr);

    if_printf(ifp, "NWID 0x%02x%02x", sc->nwid[0], sc->nwid[1]);
    if (sc->freq24) 
	printf(", Freq %d MHz",sc->freq24); 		/* 2.4 Gz       */
    printf("\n");                                       /* 2.4 Gz       */

    bus_setup_intr(device, sc->res_irq, INTR_TYPE_NET, NULL, wlintr, sc, &sc->intr_cookie);

    if (bootverbose)
	wldump(sc);
    return (0);
}

static int
wldetach(device_t device)
{
    struct wl_softc *sc = device_get_softc(device);
    struct ifnet *ifp;

    ifp = sc->ifp;
    ether_ifdetach(ifp);

    WL_LOCK(sc);

    /* reset the board */
    sc->hacr = HACR_RESET;
    CMD(sc);
    sc->hacr = HACR_DEFAULT;
    CMD(sc);
    callout_stop(&sc->watchdog_timer);
    WL_UNLOCK(sc);
    callout_drain(&sc->watchdog_timer);

    if (sc->intr_cookie != NULL) {
	bus_teardown_intr(device, sc->res_irq, sc->intr_cookie);
	sc->intr_cookie = NULL;
    }

    wl_deallocate_resources(device);
    if_free(ifp);
    mtx_destroy(&sc->wl_mtx);
    return (0);
}

static int
wl_allocate_resources(device_t device)
{
    struct wl_softc *sc = device_get_softc(device);
    int ports = 16;		/* Number of ports */

    sc->res_ioport = bus_alloc_resource_anywhere(device, SYS_RES_IOPORT,
	&sc->rid_ioport, ports, RF_ACTIVE);
    if (sc->res_ioport == NULL)
	goto errexit;

    sc->res_irq = bus_alloc_resource_any(device, SYS_RES_IRQ,
	&sc->rid_irq, RF_SHAREABLE|RF_ACTIVE);
    if (sc->res_irq == NULL)
	goto errexit;
    return (0);

errexit:
    wl_deallocate_resources(device);
    return (ENXIO);
}

static int
wl_deallocate_resources(device_t device)
{
    struct wl_softc *sc = device_get_softc(device);

    if (sc->res_irq != 0) {
	bus_release_resource(device, SYS_RES_IRQ,
	    sc->rid_irq, sc->res_irq);
	sc->res_irq = 0;
    }
    if (sc->res_ioport != 0) {
	bus_release_resource(device, SYS_RES_IOPORT,
	    sc->rid_ioport, sc->res_ioport);
	sc->res_ioport = 0;
    }
    return (0);
}

/*
 * Print out interesting information about the 82596.
 */
static void
wldump(struct wl_softc *sc)
{
    int		i;
	
    printf("hasr %04x\n", WL_READ_2(sc, HASR));
	
    printf("scb at %04x:\n ", OFFSET_SCB);
    WL_WRITE_2(sc, PIOR1, OFFSET_SCB);
    for (i = 0; i < 8; i++)
	printf("%04x ", WL_READ_2(sc, PIOP1));
    printf("\n");
	
    printf("cu at %04x:\n ", OFFSET_CU);
    WL_WRITE_2(sc, PIOR1, OFFSET_CU);
    for (i = 0; i < 8; i++)
	printf("%04x ", WL_READ_2(sc, PIOP1));
    printf("\n");
	
    printf("tbd at %04x:\n ", OFFSET_TBD);
    WL_WRITE_2(sc, PIOR1, OFFSET_TBD);
    for (i = 0; i < 4; i++)
	printf("%04x ", WL_READ_2(sc, PIOP1));
    printf("\n");
}

/* Initialize the Modem Management Controller */
static void
wlinitmmc(struct wl_softc *sc)
{
    int		configured;
    int		mode = sc->mode;
    int         i;                              /* 2.4 Gz               */
	
    /* enter 8 bit operation */
    sc->hacr = (HACR_DEFAULT & ~HACR_16BITS);
    CMD(sc);

    configured = sc->psa[WLPSA_CONFIGURED] & 1;
	
    /*
     * Set default modem control parameters.  Taken from NCR document
     *  407-0024326 Rev. A 
     */
    MMC_WRITE(MMC_JABBER_ENABLE, 0x01);
    MMC_WRITE(MMC_ANTEN_SEL, 0x02);
    MMC_WRITE(MMC_IFS, 0x20);
    MMC_WRITE(MMC_MOD_DELAY, 0x04);
    MMC_WRITE(MMC_JAM_TIME, 0x38);
    MMC_WRITE(MMC_DECAY_PRM, 0x00);		/* obsolete ? */
    MMC_WRITE(MMC_DECAY_UPDAT_PRM, 0x00);
    if (!configured) {
	MMC_WRITE(MMC_LOOPT_SEL, 0x00);
	if (sc->psa[WLPSA_COMPATNO] & 1) {
	    MMC_WRITE(MMC_THR_PRE_SET, 0x01);	/* 0x04 for AT and 0x01 for MCA */
	} else {
	    MMC_WRITE(MMC_THR_PRE_SET, 0x04);	/* 0x04 for AT and 0x01 for MCA */
	}
	MMC_WRITE(MMC_QUALITY_THR, 0x03);
    } else {
	/* use configuration defaults from parameter storage area */
	if (sc->psa[WLPSA_NWIDENABLE] & 1) {
	    if ((mode & (MOD_PROM | MOD_ENAL)) && wl_ignore_nwid) {
		MMC_WRITE(MMC_LOOPT_SEL, 0x40);
	    } else {
		MMC_WRITE(MMC_LOOPT_SEL, 0x00);
	    }
	} else {
	    MMC_WRITE(MMC_LOOPT_SEL, 0x40);	/* disable network id check */
	}
	MMC_WRITE(MMC_THR_PRE_SET, sc->psa[WLPSA_THRESH]);
	MMC_WRITE(MMC_QUALITY_THR, sc->psa[WLPSA_QUALTHRESH]);
    }
    MMC_WRITE(MMC_FREEZE, 0x00);
    MMC_WRITE(MMC_ENCR_ENABLE, 0x00);

    MMC_WRITE(MMC_NETW_ID_L,sc->nwid[1]);	/* set NWID */
    MMC_WRITE(MMC_NETW_ID_H,sc->nwid[0]);

    /* enter normal 16 bit mode operation */
    sc->hacr = HACR_DEFAULT;
    CMD(sc);
    CMD(sc);					/* virtualpc1 needs this! */

    if (sc->psa[WLPSA_COMPATNO]==		/* 2.4 Gz: half-card ver     */
		WLPSA_COMPATNO_WL24B) {		/* 2.4 Gz		     */
	i=sc->chan24<<4;			/* 2.4 Gz: position ch #     */
	MMC_WRITE(MMC_EEADDR,i+0x0f);		/* 2.4 Gz: named ch, wc=16   */
	MMC_WRITE(MMC_EECTRL,MMC_EECTRL_DWLD+	/* 2.4 Gz: Download Synths   */
			MMC_EECTRL_EEOP_READ);	/* 2.4 Gz: Read EEPROM	     */
	for (i=0; i<1000; ++i) {		/* 2.4 Gz: wait for download */
	    DELAY(40);				/* 2.4 Gz	      */
	    if ((wlmmcread(sc, MMC_EECTRLstat)	/* 2.4 Gz: check DWLD and    */
		&(MMC_EECTRLstat_DWLD		/* 2.4 Gz:	 EEBUSY	     */
		 +MMC_EECTRLstat_EEBUSY))==0)	/* 2.4 Gz:		     */
		break;				/* 2.4 Gz: download finished */
	}					/* 2.4 Gz		     */
	if (i==1000) printf("wl: synth load failed\n"); /* 2.4 Gz	*/
	MMC_WRITE(MMC_EEADDR,0x61);		/* 2.4 Gz: default pwr, wc=2 */
	MMC_WRITE(MMC_EECTRL,MMC_EECTRL_DWLD+	/* 2.4 Gz: Download Xmit Pwr */
			MMC_EECTRL_EEOP_READ);	/* 2.4 Gz: Read EEPROM	     */
	for (i=0; i<1000; ++i) {		/* 2.4 Gz: wait for download */
	    DELAY(40);				/* 2.4 Gz	      */
	    if ((wlmmcread(sc, MMC_EECTRLstat)	/* 2.4 Gz: check DWLD and    */
		&(MMC_EECTRLstat_DWLD		/* 2.4 Gz:	 EEBUSY	     */
		 +MMC_EECTRLstat_EEBUSY))==0)	/* 2.4 Gz:		     */
		break;				/* 2.4 Gz: download finished */
	}					/* 2.4 Gz		     */
	if (i==1000) printf("wl: xmit pwr load failed\n"); /* 2.4 Gz	     */
	MMC_WRITE(MMC_ANALCTRL,			/* 2.4 Gz: EXT ant+polarity  */
			MMC_ANALCTRL_ANTPOL +	/* 2.4 Gz:		     */
			MMC_ANALCTRL_EXTANT);	/* 2.4 Gz:		     */
	i=sc->chan24<<4;			/* 2.4 Gz: position ch #     */
	MMC_WRITE(MMC_EEADDR,i);		/* 2.4 Gz: get frequency     */
	MMC_WRITE(MMC_EECTRL,			/* 2.4 Gz: EEPROM read	    */
			MMC_EECTRL_EEOP_READ);	/* 2.4 Gz:		    */
	DELAY(40);				/* 2.4 Gz		     */
	i = wlmmcread(sc, MMC_EEDATALrv)	/* 2.4 Gz: freq val	     */
	  + (wlmmcread(sc, MMC_EEDATAHrv)<<8);	/* 2.4 Gz		     */
	sc->freq24 = (i>>6)+2400;		/* 2.4 Gz: save real freq    */
    }
}

/*
 * wlinit:
 *
 *	Another routine that interfaces the "if" layer to this driver.  
 *	Simply resets the structures that are used by "upper layers".  
 *	As well as calling wlhwrst that does reset the WaveLAN board.
 *
 * input	: softc pointer for this interface
 * output	: structures (if structs) and board are reset
 *
 */	
static void
wlinit(void *xsc)
{
    struct wl_softc	*sc = xsc;

    WL_LOCK(sc);
    wlinit_locked(sc);
    WL_UNLOCK(sc);
}

static void
wlinit_locked(struct wl_softc *sc)
{
    struct ifnet	*ifp = sc->ifp;
    int			stat;

#ifdef WLDEBUG
    if (sc->ifp->if_flags & IFF_DEBUG)
	if_printf(ifp, "entered wlinit()\n");
#endif
    WL_LOCK_ASSERT(sc);
    if ((stat = wlhwrst(sc)) == TRUE) {
	sc->ifp->if_drv_flags |= IFF_DRV_RUNNING;   /* same as DSF_RUNNING */
	/* 
	 * OACTIVE is used by upper-level routines
	 * and must be set
	 */
	sc->ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;  /* same as tbusy below */
		
	sc->flags |= DSF_RUNNING;
	sc->tbusy = 0;
	callout_stop(&sc->watchdog_timer);
		
	wlstart_locked(ifp);
    } else {
	if_printf(ifp, "init(): trouble resetting board.\n");
    }
}

/*
 * wlhwrst:
 *
 *	This routine resets the WaveLAN board that corresponds to the 
 *	board number passed in.
 *
 * input	: board number to do a hardware reset
 * output	: board is reset
 *
 */
static int
wlhwrst(struct wl_softc *sc)
{

#ifdef WLDEBUG
    if (sc->ifp->if_flags & IFF_DEBUG)
	if_printf(sc->ifp, "entered wlhwrst()\n");
#endif
    sc->hacr = HACR_RESET;
    CMD(sc);			/* reset the board */
	
    /* clear reset command and set PIO#1 in autoincrement mode */
    sc->hacr = HACR_DEFAULT;
    CMD(sc);

#ifdef	WLDEBUG
    if (sc->ifp->if_flags & IFF_DEBUG)
	wlmmcstat(sc);	/* Display MMC registers */
#endif	/* WLDEBUG */
    wlbldcu(sc);		/* set up command unit structures */
    
    if (wldiag(sc) == 0)
	return(0);
    
    if (wlconfig(sc) == 0)
	    return(0);
    /* 
     * insert code for loopback test here
     */
    wlrustrt(sc);		/* start receive unit */

    /* enable interrupts */
    sc->hacr = (HACR_DEFAULT | HACR_INTRON);
    CMD(sc);
    
    return(1);
}

/*
 * wlbldcu:
 *
 *	This function builds up the command unit structures.  It inits
 *	the scp, iscp, scb, cb, tbd, and tbuf.
 *
 */
static void
wlbldcu(struct wl_softc *sc)
{
    scp_t		scp;
    iscp_t		iscp;
    scb_t		scb;
    ac_t		cb;
    tbd_t		tbd;
    int		i;

    bzero(&scp, sizeof(scp));
    scp.scp_sysbus = 0;
    scp.scp_iscp = OFFSET_ISCP;
    scp.scp_iscp_base = 0;
    WL_WRITE_2(sc, PIOR1, OFFSET_SCP);
    WL_WRITE_MULTI_2(sc, PIOP1, &scp, sizeof(scp_t)/2);

    bzero(&iscp, sizeof(iscp));
    iscp.iscp_busy = 1;
    iscp.iscp_scb_offset = OFFSET_SCB;
    iscp.iscp_scb = 0;
    iscp.iscp_scb_base = 0;
    WL_WRITE_2(sc, PIOR1, OFFSET_ISCP);
    WL_WRITE_MULTI_2(sc, PIOP1, &iscp, sizeof(iscp_t)/2);

    scb.scb_status = 0;
    scb.scb_command = SCB_RESET;
    scb.scb_cbl_offset = OFFSET_CU;
    scb.scb_rfa_offset = OFFSET_RU;
    scb.scb_crcerrs = 0;
    scb.scb_alnerrs = 0;
    scb.scb_rscerrs = 0;
    scb.scb_ovrnerrs = 0;
    WL_WRITE_2(sc, PIOR1, OFFSET_SCB);
    WL_WRITE_MULTI_2(sc, PIOP1, &scb, sizeof(scb_t)/2);

    SET_CHAN_ATTN(sc);

    WL_WRITE_2(sc, PIOR0, OFFSET_ISCP + 0);	/* address of iscp_busy */
    for (i = 1000000; WL_READ_2(sc, PIOP0) && (i-- > 0); )
	continue;
    if (i <= 0)
	device_printf(sc->dev, "bldcu(): iscp_busy timeout.\n");
    WL_WRITE_2(sc, PIOR0, OFFSET_SCB + 0);	/* address of scb_status */
    for (i = STATUS_TRIES; i-- > 0; ) {
	if (WL_READ_2(sc, PIOP0) == (SCB_SW_CX|SCB_SW_CNA)) 
	    break;
    }
    if (i <= 0)
	device_printf(sc->dev, "bldcu(): not ready after reset.\n");
    wlack(sc);

    cb.ac_status = 0;
    cb.ac_command = AC_CW_EL;		/* NOP */
    cb.ac_link_offset = OFFSET_CU;
    WL_WRITE_2(sc, PIOR1, OFFSET_CU);
    WL_WRITE_MULTI_2(sc, PIOP1, &cb, 6/2);

    tbd.act_count = 0;
    tbd.next_tbd_offset = I82586NULL;
    tbd.buffer_addr = 0;
    tbd.buffer_base = 0;
    WL_WRITE_2(sc, PIOR1, OFFSET_TBD);
    WL_WRITE_MULTI_2(sc, PIOP1, &tbd, sizeof(tbd_t)/2);
}

/*
 * wlstart:
 *
 *	send a packet
 *
 * input	: board number
 * output	: stuff sent to board if any there
 *
 */
static void
wlstart(struct ifnet *ifp)
{
    struct wl_softc	*sc = ifp->if_softc;

    WL_LOCK(sc);
    wlstart_locked(ifp);
    WL_UNLOCK(sc);
}

static void
wlstart_locked(struct ifnet *ifp)
{
    struct mbuf		*m;
    struct wl_softc	*sc = ifp->if_softc;
    int			scb_status, cu_status, scb_command;

    WL_LOCK_ASSERT(sc);
#ifdef WLDEBUG
    if (sc->ifp->if_flags & IFF_DEBUG)
	if_printf(ifp, "entered wlstart()\n");
#endif

    WL_WRITE_2(sc, PIOR1, OFFSET_CU);
    cu_status = WL_READ_2(sc, PIOP1);
    WL_WRITE_2(sc, PIOR0,OFFSET_SCB + 0);	/* scb_status */
    scb_status = WL_READ_2(sc, PIOP0);
    WL_WRITE_2(sc, PIOR0, OFFSET_SCB + 2);
    scb_command = WL_READ_2(sc, PIOP0);

    /*
     * don't need OACTIVE check as tbusy here checks to see
     * if we are already busy 
     */
    if (sc->tbusy) {
	if ((scb_status & 0x0700) == SCB_CUS_IDLE &&
	   (cu_status & AC_SW_B) == 0){
	    sc->tbusy = 0;
	    callout_stop(&sc->watchdog_timer);
	    sc->ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	    /*
	     * This is probably just a race.  The xmt'r is just
	     * became idle but WE have masked interrupts so ...
	     */
#ifdef WLDEBUG
	    if_printf(ifp, "CU idle, scb %04x %04x cu %04x\n",
		   scb_status, scb_command, cu_status);
#endif 
	    if (xmt_watch) printf("!!");
	} else {
	    return;	/* genuinely still busy */
	}
    } else if ((scb_status & 0x0700) == SCB_CUS_ACTV ||
      (cu_status & AC_SW_B)){
#ifdef WLDEBUG
	if_printf(ifp, "CU unexpectedly busy; scb %04x cu %04x\n",
	       scb_status, cu_status);
#endif
	if (xmt_watch)
	    if_printf(ifp, "busy?!\n");
	return;		/* hey, why are we busy? */
    }

    /* get ourselves some data */
    IF_DEQUEUE(&ifp->if_snd, m);
    if (m != NULL) {
	/* let BPF see it before we commit it */
	BPF_MTAP(ifp, m);
	sc->tbusy++;
	/* set the watchdog timer so that if the board
	 * fails to interrupt we will restart
	 */
	/* try 10 ms, not very long */
	callout_reset(&sc->watchdog_timer, hz / 100, wlwatchdog, sc);
	sc->ifp->if_drv_flags |= IFF_DRV_OACTIVE;
	if_inc_counter(sc->ifp, IFCOUNTER_OPACKETS, 1);
	wlxmt(sc, m);
    } else {
	sc->ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
    }
    return;
}

/*
 * wlread:
 *
 *	This routine does the actual copy of data (including ethernet header
 *	structure) from the WaveLAN to an mbuf chain that will be passed up
 *	to the "if" (network interface) layer.  NOTE:  we currently
 *	don't handle trailer protocols, so if that is needed, it will
 *	(at least in part) be added here.  For simplicities sake, this
 *	routine copies the receive buffers from the board into a local (stack)
 *	buffer until the frame has been copied from the board.  Once in
 *	the local buffer, the contents are copied to an mbuf chain that
 *	is then enqueued onto the appropriate "if" queue.
 *
 * input	: board number, and a frame descriptor address
 * output	: the packet is put into an mbuf chain, and passed up
 * assumes	: if any errors occur, packet is "dropped on the floor"
 *
 */
static int
wlread(struct wl_softc *sc, u_short fd_p)
{
    struct ifnet	*ifp = sc->ifp;
    fd_t		fd;
    struct ether_header	*eh;
    struct mbuf		*m;
    rbd_t		rbd;
    u_char		*mb_p;
    u_short		mlen, len;
    u_short		bytes_in_msg, bytes_in_mbuf, bytes;

    WL_LOCK_ASSERT(sc);

#ifdef WLDEBUG
    if (sc->ifp->if_flags & IFF_DEBUG)
	if_printf(ifp, "entered wlread()\n");
#endif
    if (!((ifp->if_flags & IFF_UP) && (ifp->if_drv_flags & IFF_DRV_RUNNING))) {
	if_printf(ifp, "read(): board is not running.\n");
	sc->hacr &= ~HACR_INTRON;
	CMD(sc);		/* turn off interrupts */
    }

    /*
     * Collect message size.
     */
    WL_WRITE_2(sc, PIOR1, fd_p);
    WL_READ_MULTI_2(sc, PIOP1, &fd, sizeof(fd_t)/2);
    if (fd.rbd_offset == I82586NULL) {
	if (wlhwrst(sc) != TRUE) {
	    sc->hacr &= ~HACR_INTRON;
	    CMD(sc);		/* turn off interrupts */
	    if_printf(ifp, "read(): hwrst trouble.\n");
	}
	return 0;
    }

    WL_WRITE_2(sc, PIOR1, fd.rbd_offset);
    WL_READ_MULTI_2(sc, PIOP1, &rbd, sizeof(rbd_t)/2);
    bytes_in_msg = rbd.status & RBD_SW_COUNT;

    /*
     * Allocate a cluster'd mbuf to receive the packet.
     */
    m = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
    if (m == NULL) {
	if (wlhwrst(sc) != TRUE) {
	    sc->hacr &= ~HACR_INTRON;
	    CMD(sc);		/* turn off interrupts */
	    if_printf(ifp, "read(): hwrst trouble.\n");
	}
	return 0;
    }
    m->m_pkthdr.len = m->m_len = MCLBYTES;
    m_adj(m, ETHER_ALIGN);		/* align IP header */

    /*
     * Collect the message data.
     */
    mlen = 0;
    mb_p = mtod(m, u_char *);
    bytes_in_mbuf = m->m_len;

    /* Put the ethernet header inside the mbuf. */
    bcopy(&fd.destination[0], mb_p, 14);
    mb_p += 14;
    mlen += 14;
    bytes_in_mbuf -= 14;

    bytes = min(bytes_in_mbuf, bytes_in_msg);
    for (;;) {
	if (bytes & 1) {
	    len = bytes + 1;
	} else {
	    len = bytes;
	}
	WL_WRITE_2(sc, PIOR1, rbd.buffer_addr);
	WL_READ_MULTI_2(sc, PIOP1, mb_p, len/2);
	mlen += bytes;

	if (bytes > bytes_in_mbuf) {
	    /* XXX something wrong, a packet should fit in 1 cluster */
	    m_freem(m);
	    if_printf(ifp, "read(): packet too large (%u > %u)\n",
		   bytes, bytes_in_mbuf);
	    if (wlhwrst(sc) != TRUE) {
		sc->hacr &= ~HACR_INTRON;
		CMD(sc);  /* turn off interrupts */
		if_printf(ifp, "read(): hwrst trouble.\n");
	    }
	    return 0;
	}
	mb_p += bytes;
	bytes_in_mbuf -= bytes;
	bytes_in_msg -= bytes;
	if (bytes_in_msg == 0) {
	    if (rbd.status & RBD_SW_EOF || rbd.next_rbd_offset == I82586NULL) {
		break;
	    }
	    WL_WRITE_2(sc, PIOR1, rbd.next_rbd_offset);
	    WL_READ_MULTI_2(sc, PIOP1, &rbd, sizeof(rbd_t)/2);
	    bytes_in_msg = rbd.status & RBD_SW_COUNT;
	} else {
	    rbd.buffer_addr += bytes;
	}

	bytes = min(bytes_in_mbuf, bytes_in_msg);
    }

    m->m_pkthdr.len = m->m_len = mlen;
    m->m_pkthdr.rcvif = ifp;

    /*
     * If hw is in promiscuous mode (note that I said hardware, not if
     * IFF_PROMISC is set in ifnet flags), then if this is a unicast
     * packet and the MAC dst is not us, drop it.  This check in normally
     * inside ether_input(), but IFF_MULTI causes hw promisc without
     * a bpf listener, so this is wrong.
     *		Greg Troxel <gdt@ir.bbn.com>, 1998-08-07
     */
    /*
     * TBD: also discard packets where NWID does not match.
     * However, there does not appear to be a way to read the nwid
     * for a received packet.  -gdt 1998-08-07
     */
    /* XXX verify mbuf length */
    eh = mtod(m, struct ether_header *);
    if (
#ifdef WL_USE_IFNET_PROMISC_CHECK /* not defined */
	(sc->ifp->if_flags & (IFF_PROMISC|IFF_ALLMULTI))
#else
	/* hw is in promisc mode if this is true */
	(sc->mode & (MOD_PROM | MOD_ENAL))
#endif
	&&
	(eh->ether_dhost[0] & 1) == 0 && /* !mcast and !bcast */
	bcmp(eh->ether_dhost, IF_LLADDR(sc->ifp),
	     sizeof(eh->ether_dhost)) != 0 ) {
      m_freem(m);
      return 1;
    }

#ifdef WLDEBUG
    if (sc->ifp->if_flags & IFF_DEBUG)
	if_printf(ifp, "wlrecv %u bytes\n", mlen);
#endif

#ifdef WLCACHE
    wl_cache_store(sc, eh, m);
#endif

    /*
     * received packet is now in a chain of mbuf's.  next step is
     * to pass the packet upwards.
     */
    WL_UNLOCK(sc);
    (*ifp->if_input)(ifp, m);
    WL_LOCK(sc);
    return 1;
}

/*
 * wlioctl:
 *
 *	This routine processes an ioctl request from the "if" layer
 *	above.
 *
 * input	: pointer the appropriate "if" struct, command, and data
 * output	: based on command appropriate action is taken on the
 *	 	  WaveLAN board(s) or related structures
 * return	: error is returned containing exit conditions
 *
 */
static int
wlioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
    struct ifreq	*ifr = (struct ifreq *)data;
    struct wl_softc	*sc = ifp->if_softc;
    short		mode = 0;
    int			error = 0;
    struct thread	*td = curthread;	/* XXX */
    int			irq, irqval, i, isroot;
    char		psa_buf[0x40];
    char		eeprom_buf[0x80];
#ifdef WLCACHE
    size_t		size;
    char * 	        cpt;
#endif

#ifdef WLDEBUG
    if (sc->ifp->if_flags & IFF_DEBUG)
	if_printf(ifp, "entered wlioctl()\n");
#endif
    switch (cmd) {
    case SIOCSIFFLAGS:
	WL_LOCK(sc);
	if (ifp->if_flags & IFF_ALLMULTI) {
	    mode |= MOD_ENAL;
	}
	if (ifp->if_flags & IFF_PROMISC) {
	    mode |= MOD_PROM;
	}
	if (ifp->if_flags & IFF_LINK0) {
	    mode |= MOD_PROM;
	}
	/*
	 * force a complete reset if the receive multicast/
	 * promiscuous mode changes so that these take 
	 * effect immediately.
	 *
	 */
	if (sc->mode != mode) {
	    sc->mode = mode;
	    if (sc->flags & DSF_RUNNING) {
		sc->flags &= ~DSF_RUNNING;
		wlinit_locked(sc);
	    }
	}
	/* if interface is marked DOWN and still running then
	 * stop it.
	 */
	if ((ifp->if_flags & IFF_UP) == 0 && sc->flags & DSF_RUNNING) {
	    if_printf(ifp, "ioctl(): board is not running\n");
	    sc->flags &= ~DSF_RUNNING;
	    sc->hacr &= ~HACR_INTRON;
	    CMD(sc);		  /* turn off interrupts */
	} 
	/* else if interface is UP and RUNNING, start it
		*/
	else if (ifp->if_flags & IFF_UP && (sc->flags & DSF_RUNNING) == 0) {
	    wlinit_locked(sc);
	}
  
	/* if WLDEBUG set on interface, then printf rf-modem regs
	*/
	if (ifp->if_flags & IFF_DEBUG)
	    wlmmcstat(sc);
	WL_UNLOCK(sc);
	break;
#if	MULTICAST
    case SIOCADDMULTI:
    case SIOCDELMULTI:

	wlinit(sc);
	break;
#endif	/* MULTICAST */

    /* DEVICE SPECIFIC */


	/* copy the PSA out to the caller */
    case SIOCGWLPSA:
	/* work out if they're root */
	isroot = (priv_check(td, PRIV_NET80211_GETKEY) == 0);

	bzero(psa_buf, sizeof(psa_buf));
	WL_LOCK(sc);
	for (i = 0; i < 0x40; i++) {
	    /* don't hand the DES key out to non-root users */
	    if ((i > WLPSA_DESKEY) && (i < (WLPSA_DESKEY + 8)) && !isroot)
		continue;
	    psa_buf[i] = sc->psa[i];
	}
	WL_UNLOCK(sc);

	error = copyout(psa_buf, ifr->ifr_data, sizeof(psa_buf));
	break;


	/* copy the PSA in from the caller; we only copy _some_ values */
    case SIOCSWLPSA:
	/* root only */
	if ((error = priv_check(td, PRIV_DRIVER)))
	    break;

	error = copyin(ifr->ifr_data, psa_buf, sizeof(psa_buf));
	if (error)
	    break;
	
	/* check IRQ value */
	irqval = psa_buf[WLPSA_IRQNO];
	for (irq = 15; irq >= 0; irq--)
	    if (irqvals[irq] == irqval)
		break;
	if (irq == 0)			/* oops */
	    break;
	WL_LOCK(sc);
	/* new IRQ */
	sc->psa[WLPSA_IRQNO] = irqval;

	/* local MAC */
	for (i = 0; i < 6; i++)
	    sc->psa[WLPSA_LOCALMAC + i] = psa_buf[WLPSA_LOCALMAC + i];
		
	/* MAC select */	
	sc->psa[WLPSA_MACSEL] = psa_buf[WLPSA_MACSEL];
	
	/* default nwid */
	sc->psa[WLPSA_NWID] = psa_buf[WLPSA_NWID];
	sc->psa[WLPSA_NWID + 1] = psa_buf[WLPSA_NWID + 1];

	wlsetpsa(sc);		/* update the PSA */
	WL_UNLOCK(sc);
	break;


	/* get the current NWID out of the sc since we stored it there */
    case SIOCGWLCNWID:
	WL_LOCK(sc);
	ifr->ifr_data = (caddr_t) (sc->nwid[0] << 8 | sc->nwid[1]);
	WL_UNLOCK(sc);
	break;


	/*
	 * change the nwid dynamically.  This
	 * ONLY changes the radio modem and does not
	 * change the PSA.
	 *
	 * 2 steps:
	 *	1. save in softc "soft registers"
	 *	2. save in radio modem (MMC)
	 */
    case SIOCSWLCNWID:
	/* root only */
	if ((error = priv_check(td, PRIV_DRIVER)))
	    break;
	WL_LOCK(sc);
	if (!(ifp->if_flags & IFF_UP)) {
	    error = EIO;	/* only allowed while up */
	} else {
	    /* 
	     * soft c nwid shadows radio modem setting
	     */
	    sc->nwid[0] = (int)ifr->ifr_data >> 8;
	    sc->nwid[1] = (int)ifr->ifr_data & 0xff;
	    MMC_WRITE(MMC_NETW_ID_L,sc->nwid[1]);
	    MMC_WRITE(MMC_NETW_ID_H,sc->nwid[0]);
	}
	WL_UNLOCK(sc);
	break;

	/* copy the EEPROM in 2.4 Gz WaveMODEM  out to the caller */
    case SIOCGWLEEPROM:
	/* root only */
	if ((error = priv_check(td, PRIV_DRIVER)))
	    break;

	bzero(eeprom_buf, sizeof(eeprom_buf));
	WL_LOCK(sc);
	for (i=0x00; i<0x80; ++i) {		/* 2.4 Gz: size of EEPROM   */
	    MMC_WRITE(MMC_EEADDR,i);		/* 2.4 Gz: get frequency    */
	    MMC_WRITE(MMC_EECTRL,		/* 2.4 Gz: EEPROM read	    */
			MMC_EECTRL_EEOP_READ);	/* 2.4 Gz:		    */
	    DELAY(40);				/* 2.4 Gz		    */
	    eeprom_buf[2 * i] =			/* 2.4 Gz: pass low byte of */
		wlmmcread(sc, MMC_EEDATALrv);   /* 2.4 Gz: EEPROM word      */
	    eeprom_buf[2 * i + 1] =		/* 2.4 Gz: pass hi byte of  */
		wlmmcread(sc, MMC_EEDATALrv);	/* 2.4 Gz: EEPROM word      */
	}
	WL_UNLOCK(sc);
	error = copyout(ifr->ifr_data, eeprom_buf, sizeof(eeprom_buf));
	break;

#ifdef WLCACHE
	/* zero (Delete) the wl cache */
    case SIOCDWLCACHE:
	/* root only */
	if ((error = priv_check(td, PRIV_DRIVER)))
	    break;
	WL_LOCK(sc);
	wl_cache_zero(sc);
	WL_UNLOCK(sc);
	break;

	/* read out the number of used cache elements */
    case SIOCGWLCITEM:
	WL_LOCK(sc);
	ifr->ifr_data = (caddr_t) sc->w_sigitems;
	WL_UNLOCK(sc);
	break;

	/* read out the wl cache */
    case SIOCGWLCACHE:
	WL_LOCK(sc);
	size = sc->w_sigitems * sizeof(struct w_sigcache);
	cpt = malloc(size, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (cpt == NULL) {
	    WL_UNLOCK(sc);
	    return (ENOMEM);
	}

	bcopy(sc->w_sigcache, cpt, size);
	WL_UNLOCK(sc);

	error = copyout(cpt, ifr->ifr_data, size);
	free(cpt, M_DEVBUF);
	break;
#endif

    default:
        error = ether_ioctl(ifp, cmd, data);
	break;
    }
    return (error);
}

/*
 * wlwatchdog():
 *
 * Called if the timer set in wlstart expires before an interrupt is received
 * from the wavelan.   It seems to lose interrupts sometimes.
 * The watchdog routine gets called if the transmitter failed to interrupt
 *
 * input	: which board is timing out
 * output	: board reset 
 *
 */
static void
wlwatchdog(void *vsc)
{
    struct wl_softc *sc = vsc;

    log(LOG_ERR, "%s: wavelan device timeout on xmit\n", sc->ifp->if_xname);
    if_inc_counter(sc->ifp, IFCOUNTER_OERRORS, 1);
    wlinit_locked(sc);
}

/*
 * wlintr:
 *
 *	This function is the interrupt handler for the WaveLAN
 *	board.  This routine will be called whenever either a packet
 *	is received, or a packet has successfully been transferred and
 *	the unit is ready to transmit another packet.
 *
 * input	: board number that interrupted
 * output	: either a packet is received, or a packet is transferred
 *
 */
static void
wlintr(void *arg)
{
    struct wl_softc	*sc = (struct wl_softc *)arg;
    int			ac_status;
    u_short		int_type, int_type1;

    WL_LOCK(sc);
#ifdef WLDEBUG
    if (sc->ifp->if_flags & IFF_DEBUG)
	if_printf(sc->ifp, "wlintr() called\n");
#endif

    if ((int_type = WL_READ_2(sc, HASR)) & HASR_MMC_INTR) {
	/* handle interrupt from the modem management controller */
	/* This will clear the interrupt condition */ 
	(void) wlmmcread(sc, MMC_DCE_STATUS); /* ignored for now */
    }

    if (!(int_type & HASR_INTR)){	/* return if no interrupt from 82586 */
	/* commented out. jrb.  it happens when reinit occurs
	   printf("wlintr: int_type %x, dump follows\n", int_type);
	   wldump(sc);
	   */
	WL_UNLOCK(sc);
	return;
    }

    if (gathersnr)
	getsnr(sc);
    for (;;) {
	WL_WRITE_2(sc, PIOR0, OFFSET_SCB + 0);	/* get scb status */
	int_type = (WL_READ_2(sc, PIOP0) & SCB_SW_INT);
	if (int_type == 0)			/* no interrupts left */
	    break;

	int_type1 = wlack(sc);		/* acknowledge interrupt(s) */
	/* make sure no bits disappeared (others may appear) */
	if ((int_type & int_type1) != int_type)
	    printf("wlack() int bits disappeared : %04x != int_type %04x\n",
		   int_type1, int_type);
	int_type = int_type1;			/* go with the new status */
	/* 
	 * incoming packet
	 */
	if (int_type & SCB_SW_FR) {
	    if_inc_counter(sc->ifp, IFCOUNTER_IPACKETS, 1);
	    wlrcv(sc);
	}
	/*
	 * receiver not ready
	 */
	if (int_type & SCB_SW_RNR) {
	    if_inc_counter(sc->ifp, IFCOUNTER_IERRORS, 1);
#ifdef	WLDEBUG
	    if (sc->ifp->if_flags & IFF_DEBUG)
		if_printf(sc->ifp, "intr(): receiver overrun! begin_fd = %x\n",
		       sc->begin_fd);
#endif
	    wlrustrt(sc);
	}
	/*
	 * CU not ready
	 */
	if (int_type & SCB_SW_CNA) {
	    /*
	     * At present, we don't care about CNA's.  We
	     * believe they are a side effect of XMT.
	     */
	}
	if (int_type & SCB_SW_CX) {
	    /*
	     * At present, we only request Interrupt for
	     * XMT.
	     */
	    WL_WRITE_2(sc, PIOR1, OFFSET_CU);	/* get command status */
	    ac_status = WL_READ_2(sc, PIOP1);

	    if (xmt_watch) {			/* report some anomalies */

		if (sc->tbusy == 0) {
		    if_printf(sc->ifp, "xmt intr but not busy, CU %04x\n",
			   ac_status);
		}
		if (ac_status == 0) {
		    if_printf(sc->ifp, "xmt intr but ac_status == 0\n");
		}
		if (ac_status & AC_SW_A) {
		    if_printf(sc->ifp, "xmt aborted\n");
		}
#ifdef	notdef
		if (ac_status & TC_CARRIER) {
		    if_printf(sc->ifp, "no carrier\n");
		}
#endif	/* notdef */
		if (ac_status & TC_CLS) {
		    if_printf(sc->ifp, "no CTS\n");
		}
		if (ac_status & TC_DMA) {
		    if_printf(sc->ifp, "DMA underrun\n");
		}
		if (ac_status & TC_DEFER) {
		    if_printf(sc->ifp, "xmt deferred\n");
		}
		if (ac_status & TC_SQE) {
		    if_printf(sc->ifp, "heart beat\n");
		}
		if (ac_status & TC_COLLISION) {
		    if_printf(sc->ifp, "too many collisions\n");
		}
	    }
	    /* if the transmit actually failed, or returned some status */
	    if ((!(ac_status & AC_SW_OK)) || (ac_status & 0xfff)) {
		if (ac_status & (TC_COLLISION | TC_CLS | TC_DMA)) {
		    if_inc_counter(sc->ifp, IFCOUNTER_OERRORS, 1);
		}
		/* count collisions */
		if_inc_counter(sc->ifp, IFCOUNTER_COLLISIONS, (ac_status & 0xf));
		/* if TC_COLLISION set and collision count zero, 16 collisions */
		if ((ac_status & 0x20) == 0x20) {
		    if_inc_counter(sc->ifp, IFCOUNTER_COLLISIONS, 0x10);
		}
	    }
	    sc->tbusy = 0;
	    callout_stop(&sc->watchdog_timer);
	    sc->ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	    wlstart_locked(sc->ifp);
	}
    }
    WL_UNLOCK(sc);
    return;
}

/*
 * wlrcv:
 *
 *	This routine is called by the interrupt handler to initiate a
 *	packet transfer from the board to the "if" layer above this
 *	driver.  This routine checks if a buffer has been successfully
 *	received by the WaveLAN.  If so, the routine wlread is called
 *	to do the actual transfer of the board data (including the
 *	ethernet header) into a packet (consisting of an mbuf chain).
 *
 * input	: number of the board to check
 * output	: if a packet is available, it is "sent up"
 *
 */
static void
wlrcv(struct wl_softc *sc)
{
    u_short	fd_p, status, offset, link_offset;

#ifdef WLDEBUG
    if (sc->ifp->if_flags & IFF_DEBUG)
	if_printf(sc->ifp, "entered wlrcv()\n");
#endif
    for (fd_p = sc->begin_fd; fd_p != I82586NULL; fd_p = sc->begin_fd) {

	WL_WRITE_2(sc, PIOR0, fd_p + 0);	/* address of status */
	status = WL_READ_2(sc, PIOP0);
	WL_WRITE_2(sc, PIOR1, fd_p + 4);	/* address of link_offset */
	link_offset = WL_READ_2(sc, PIOP1);
	offset = WL_READ_2(sc, PIOP1);	/* rbd_offset */
	if (status == 0xffff || offset == 0xffff /*I82586NULL*/) {
	    if (wlhwrst(sc) != TRUE)
		if_printf(sc->ifp, "rcv(): hwrst ffff trouble.\n");
	    return;
	} else if (status & AC_SW_C) {
	    if (status == (RFD_DONE|RFD_RSC)) {
		/* lost one */
#ifdef	WLDEBUG
		if (sc->ifp->if_flags & IFF_DEBUG)
		    if_printf(sc->ifp, "RCV: RSC %x\n", status);
#endif
		if_inc_counter(sc->ifp, IFCOUNTER_IERRORS, 1);
	    } else if (!(status & RFD_OK)) {
		if_printf(sc->ifp, "RCV: !OK %x\n", status);
		if_inc_counter(sc->ifp, IFCOUNTER_IERRORS, 1);
	    } else if (status & 0xfff) {	/* can't happen */
		if_printf(sc->ifp, "RCV: ERRs %x\n", status);
		if_inc_counter(sc->ifp, IFCOUNTER_IERRORS, 1);
	    } else if (!wlread(sc, fd_p))
		return;

	    if (!wlrequeue(sc, fd_p)) {
		/* abort on chain error */
		if (wlhwrst(sc) != TRUE)
		    if_printf(sc->ifp, "rcv(): hwrst trouble.\n");
		return;
	    }
	    sc->begin_fd = link_offset;
	} else {
	    break;
	}
    }
    return;
}

/*
 * wlrequeue:
 *
 *	This routine puts rbd's used in the last receive back onto the
 *	free list for the next receive.
 *
 */
static int
wlrequeue(struct wl_softc *sc, u_short fd_p)
{
    fd_t		fd;
    u_short		l_rbdp, f_rbdp, rbd_offset;

    WL_WRITE_2(sc, PIOR0, fd_p + 6);
    rbd_offset = WL_READ_2(sc, PIOP0);
    if ((f_rbdp = rbd_offset) != I82586NULL) {
	l_rbdp = f_rbdp;
	for (;;) {
	    WL_WRITE_2(sc, PIOR0, l_rbdp + 0);	/* address of status */
	    if (WL_READ_2(sc, PIOP0) & RBD_SW_EOF)
		break;
	    WL_WRITE_2(sc, PIOP0, 0);
	    WL_WRITE_2(sc, PIOR0, l_rbdp + 2);	/* next_rbd_offset */
	    if ((l_rbdp = WL_READ_2(sc, PIOP0)) == I82586NULL)
		break;
	}
	WL_WRITE_2(sc, PIOP0, 0);
	WL_WRITE_2(sc, PIOR0, l_rbdp + 2);		/* next_rbd_offset */
	WL_WRITE_2(sc, PIOP0, I82586NULL);
	WL_WRITE_2(sc, PIOR0, l_rbdp + 8);		/* address of size */
	WL_WRITE_2(sc, PIOP0, WL_READ_2(sc, PIOP0) | AC_CW_EL);
	WL_WRITE_2(sc, PIOR0, sc->end_rbd + 2);
	WL_WRITE_2(sc, PIOP0, f_rbdp);		/* end_rbd->next_rbd_offset */
	WL_WRITE_2(sc, PIOR0, sc->end_rbd + 8);	/* size */
	WL_WRITE_2(sc, PIOP0, WL_READ_2(sc, PIOP0) & ~AC_CW_EL);
	sc->end_rbd = l_rbdp;
    }

    fd.status = 0;
    fd.command = AC_CW_EL;
    fd.link_offset = I82586NULL;
    fd.rbd_offset = I82586NULL;
    WL_WRITE_2(sc, PIOR1, fd_p);
    WL_WRITE_MULTI_2(sc, PIOP1, &fd, 8/2);
    
    WL_WRITE_2(sc, PIOR1, sc->end_fd + 2);	/* addr of command */
    WL_WRITE_2(sc, PIOP1, 0);		/* command = 0 */
    WL_WRITE_2(sc, PIOP1, fd_p);		/* end_fd->link_offset = fd_p */
    sc->end_fd = fd_p;

    return 1;
}

#ifdef	WLDEBUG
static int xmt_debug = 0;
#endif	/* WLDEBUG */

/*
 * wlxmt:
 *
 *	This routine fills in the appropriate registers and memory
 *	locations on the WaveLAN board and starts the board off on
 *	the transmit.
 *
 * input	: pointers to board of interest's softc and the mbuf
 * output	: board memory and registers are set for xfer and attention
 *
 */
static void
wlxmt(struct wl_softc *sc, struct mbuf *m)
{
    u_short		xmtdata_p = OFFSET_TBUF;
    u_short		xmtshort_p;
    struct mbuf		*tm_p = m;
    struct ether_header	*eh_p = mtod(m, struct ether_header *);
    u_char		*mb_p = mtod(m, u_char *) + sizeof(struct ether_header);
    u_short		count = m->m_len - sizeof(struct ether_header);
    ac_t		cb;
    u_short		tbd_p = OFFSET_TBD;
    u_short		len, clen = 0;
    int			spin;
	
#ifdef WLDEBUG
    if (sc->ifp->if_flags & IFF_DEBUG)
	if_printf(sc->ifp, "entered wlxmt()\n");
#endif

    cb.ac_status = 0;
    cb.ac_command = (AC_CW_EL|AC_TRANSMIT|AC_CW_I);
    cb.ac_link_offset = I82586NULL;
    WL_WRITE_2(sc, PIOR1, OFFSET_CU);
    WL_WRITE_MULTI_2(sc, PIOP1, &cb, 6/2);
    WL_WRITE_2(sc, PIOP1, OFFSET_TBD);	/* cb.cmd.transmit.tbd_offset */
    WL_WRITE_MULTI_2(sc, PIOP1, eh_p->ether_dhost, WAVELAN_ADDR_SIZE/2);
    WL_WRITE_2(sc, PIOP1, eh_p->ether_type);

#ifdef	WLDEBUG
    if (sc->ifp->if_flags & IFF_DEBUG) {
	if (xmt_debug) {
	    printf("XMT    mbuf: L%d @%p ", count, (void *)mb_p);
	    printf("ether type %x\n", eh_p->ether_type);
	}
    }
#endif	/* WLDEBUG */
    WL_WRITE_2(sc, PIOR0, OFFSET_TBD);
    WL_WRITE_2(sc, PIOP0, 0);		/* act_count */
    WL_WRITE_2(sc, PIOR1, OFFSET_TBD + 4);
    WL_WRITE_2(sc, PIOP1, xmtdata_p);	/* buffer_addr */
    WL_WRITE_2(sc, PIOP1, 0);		/* buffer_base */
    for (;;) {
	if (count) {
	    if (clen + count > WAVELAN_MTU)
		break;
	    if (count & 1)
		len = count + 1;
	    else
		len = count;
	    WL_WRITE_2(sc, PIOR1, xmtdata_p);
	    WL_WRITE_MULTI_2(sc, PIOP1, mb_p, len/2);
	    clen += count;
	    WL_WRITE_2(sc, PIOR0, tbd_p);  /* address of act_count */
	    WL_WRITE_2(sc, PIOP0, WL_READ_2(sc, PIOP0) + count);
	    xmtdata_p += len;
	    if ((tm_p = tm_p->m_next) == (struct mbuf *)0)
		break;
	    if (count & 1) {
		/* go to the next descriptor */
		WL_WRITE_2(sc, PIOR0, tbd_p + 2);
		tbd_p += sizeof (tbd_t);
		WL_WRITE_2(sc, PIOP0, tbd_p); /* next_tbd_offset */
		WL_WRITE_2(sc, PIOR0, tbd_p);
		WL_WRITE_2(sc, PIOP0, 0);	/* act_count */
		WL_WRITE_2(sc, PIOR1, tbd_p + 4);
		WL_WRITE_2(sc, PIOP1, xmtdata_p); /* buffer_addr */
		WL_WRITE_2(sc, PIOP1, 0);	      /* buffer_base */
		/* at the end -> coallesce remaining mbufs */
		if (tbd_p == OFFSET_TBD + (N_TBD-1) * sizeof (tbd_t)) {
		    wlsftwsleaze(&count, &mb_p, &tm_p, sc);
		    continue;
		}
		/* next mbuf short -> coallesce as needed */
		if ( (tm_p->m_next == (struct mbuf *) 0) ||
#define HDW_THRESHOLD 55
		     tm_p->m_len > HDW_THRESHOLD)
		    /* ok */;
		else {
		    wlhdwsleaze(&count, &mb_p, &tm_p, sc);
		    continue;
		}
	    }
	} else if ((tm_p = tm_p->m_next) == (struct mbuf *)0)
	    break;
	count = tm_p->m_len;
	mb_p = mtod(tm_p, u_char *);
#ifdef	WLDEBUG
	if (sc->ifp->if_flags & IFF_DEBUG)
	    if (xmt_debug)
		printf("mbuf+ L%d @%p ", count, (void *)mb_p);
#endif	/* WLDEBUG */
    }
#ifdef	WLDEBUG
    if (sc->ifp->if_flags & IFF_DEBUG)
	if (xmt_debug)
	    printf("CLEN = %d\n", clen);
#endif	/* WLDEBUG */
    WL_WRITE_2(sc, PIOR0, tbd_p);
    if (clen < ETHERMIN) {
	WL_WRITE_2(sc, PIOP0, WL_READ_2(sc, PIOP0) + ETHERMIN - clen);
	WL_WRITE_2(sc, PIOR1, xmtdata_p);
	for (xmtshort_p = xmtdata_p; clen < ETHERMIN; clen += 2)
	    WL_WRITE_2(sc, PIOP1, 0);
    }	
    WL_WRITE_2(sc, PIOP0, WL_READ_2(sc, PIOP0) | TBD_SW_EOF);
    WL_WRITE_2(sc, PIOR0, tbd_p + 2);
    WL_WRITE_2(sc, PIOP0, I82586NULL);
#ifdef	WLDEBUG
    if (sc->ifp->if_flags & IFF_DEBUG) {
	if (xmt_debug) {
	    wltbd(sc);
	    printf("\n");
	}
    }
#endif	/* WLDEBUG */

    WL_WRITE_2(sc, PIOR0, OFFSET_SCB + 2);	/* address of scb_command */
    /* 
     * wait for 586 to clear previous command, complain if it takes
     * too long
     */
    for (spin = 1;;spin = (spin + 1) % 10000) {
	if (WL_READ_2(sc, PIOP0) == 0) {		/* it's done, we can go */
	    break;
	}
	if ((spin == 0) && xmt_watch) {		/* not waking up, and we care */
	    if_printf(sc->ifp, "slow accepting xmit\n");
	}
    }
    WL_WRITE_2(sc, PIOP0, SCB_CU_STRT);		/* new command */
    SET_CHAN_ATTN(sc);
    
    m_freem(m);

    /* XXX 
     * Pause to avoid transmit overrun problems.
     * The required delay tends to vary with platform type, and may be
     * related to interrupt loss.
     */
    if (wl_xmit_delay) {
	DELAY(wl_xmit_delay);
    }
    return;
}

/*
 * wlbldru:
 *
 *	This function builds the linear linked lists of fd's and
 *	rbd's.  Based on page 4-32 of 1986 Intel microcom handbook.
 *
 */
static u_short
wlbldru(struct wl_softc *sc)
{
    fd_t	fd;
    rbd_t	rbd;
    u_short	fd_p = OFFSET_RU;
    u_short	rbd_p = OFFSET_RBD;
    int 	i;

    sc->begin_fd = fd_p;
    for (i = 0; i < N_FD; i++) {
	fd.status = 0;
	fd.command = 0;
	fd.link_offset = fd_p + sizeof(fd_t);
	fd.rbd_offset = I82586NULL;
	WL_WRITE_2(sc, PIOR1, fd_p);
	WL_WRITE_MULTI_2(sc, PIOP1, &fd, 8/2);
	fd_p = fd.link_offset;
    }
    fd_p -= sizeof(fd_t);
    sc->end_fd = fd_p;
    WL_WRITE_2(sc, PIOR1, fd_p + 2);
    WL_WRITE_2(sc, PIOP1, AC_CW_EL);	/* command */
    WL_WRITE_2(sc, PIOP1, I82586NULL);	/* link_offset */
    fd_p = OFFSET_RU;
    
    WL_WRITE_2(sc, PIOR0, fd_p + 6);	/* address of rbd_offset */
    WL_WRITE_2(sc, PIOP0, rbd_p);
    WL_WRITE_2(sc, PIOR1, rbd_p);
    for (i = 0; i < N_RBD; i++) {
	rbd.status = 0;
	rbd.buffer_addr = rbd_p + sizeof(rbd_t) + 2;
	rbd.buffer_base = 0;
	rbd.size = RCVBUFSIZE;
	if (i != N_RBD-1) {
	    rbd_p += sizeof(ru_t);
	    rbd.next_rbd_offset = rbd_p;
	} else {
	    rbd.next_rbd_offset = I82586NULL;
	    rbd.size |= AC_CW_EL;
	    sc->end_rbd = rbd_p;
	}
	WL_WRITE_MULTI_2(sc, PIOP1, &rbd, sizeof(rbd_t)/2);
	WL_WRITE_2(sc, PIOR1, rbd_p);
    }
    return sc->begin_fd;
}

/*
 * wlrustrt:
 *
 *	This routine starts the receive unit running.  First checks if the
 *	board is actually ready, then the board is instructed to receive
 *	packets again.
 *
 */
static void
wlrustrt(struct wl_softc *sc)
{
    u_short		rfa;

#ifdef WLDEBUG
    if (sc->ifp->if_flags & IFF_DEBUG)
	if_printf(sc->ifp, "entered wlrustrt()\n");
#endif
    WL_WRITE_2(sc, PIOR0, OFFSET_SCB);
    if (WL_READ_2(sc, PIOP0) & SCB_RUS_READY){
	printf("wlrustrt: RUS_READY\n");
	return;
    }

    WL_WRITE_2(sc, PIOR0, OFFSET_SCB + 2);
    WL_WRITE_2(sc, PIOP0, SCB_RU_STRT);		/* command */
    rfa = wlbldru(sc);
    WL_WRITE_2(sc, PIOR0, OFFSET_SCB + 6);	/* address of scb_rfa_offset */
    WL_WRITE_2(sc, PIOP0, rfa);

    SET_CHAN_ATTN(sc);
    return;
}

/*
 * wldiag:
 *
 *	This routine does a 586 op-code number 7, and obtains the
 *	diagnose status for the WaveLAN.
 *
 */
static int
wldiag(struct wl_softc *sc)
{
    short	status;

#ifdef WLDEBUG
    if (sc->ifp->if_flags & IFF_DEBUG)
	if_printf(sc->ifp, "entered wldiag()\n");
#endif
    WL_WRITE_2(sc, PIOR0, OFFSET_SCB);
    status = WL_READ_2(sc, PIOP0);
    if (status & SCB_SW_INT) {
		/* state is 2000 which seems ok
		   if_printf(sc->ifp, "diag(): unexpected initial state %\n",
		       WL_READ_2(sc, PIOP0));
		*/
	wlack(sc);
    }
    WL_WRITE_2(sc, PIOR1, OFFSET_CU);
    WL_WRITE_2(sc, PIOP1, 0);			/* ac_status */
    WL_WRITE_2(sc, PIOP1, AC_DIAGNOSE|AC_CW_EL);/* ac_command */
    if (wlcmd(sc, "diag()") == 0)
	return 0;
    WL_WRITE_2(sc, PIOR0, OFFSET_CU);
    if (WL_READ_2(sc, PIOP0) & 0x0800) {
	if_printf(sc->ifp, "i82586 Self Test failed!\n");
	return 0;
    }
    return TRUE;
}

/*
 * wlconfig:
 *
 *	This routine does a standard config of the WaveLAN board.
 *
 */
static int
wlconfig(struct wl_softc *sc)
{
    configure_t	configure;

#if	MULTICAST
    struct ifmultiaddr *ifma;
    u_char *addrp;
    int cnt = 0;
#endif	/* MULTICAST */

#ifdef WLDEBUG
    if (sc->ifp->if_flags & IFF_DEBUG)
	if_printf(sc->ifp, "entered wlconfig()\n");
#endif
    WL_WRITE_2(sc, PIOR0, OFFSET_SCB);
    if (WL_READ_2(sc, PIOP0) & SCB_SW_INT) {
	/*
	  if_printf(sc->ifp, "config(): unexpected initial state %x\n",
	      WL_READ_2(sc, PIOP0));
	  */
    }
    wlack(sc);

    WL_WRITE_2(sc, PIOR1, OFFSET_CU);
    WL_WRITE_2(sc, PIOP1, 0);				/* ac_status */
    WL_WRITE_2(sc, PIOP1, AC_CONFIGURE|AC_CW_EL);	/* ac_command */

/* jrb hack */
    configure.fifolim_bytecnt 	= 0x080c;
    configure.addrlen_mode  	= 0x0600;
    configure.linprio_interframe	= 0x2060;
    configure.slot_time      	= 0xf200;
    configure.hardware	     	= 0x0008;	/* tx even w/o CD */
    configure.min_frame_len   	= 0x0040;
#if 0
    /* This is the configuration block suggested by Marc Meertens
     * <mmeerten@obelix.utrecht.NCR.COM> in an e-mail message to John
     * Ioannidis on 10 Nov 92.
     */
    configure.fifolim_bytecnt 	= 0x040c;
    configure.addrlen_mode  	= 0x0600;
    configure.linprio_interframe	= 0x2060;
    configure.slot_time      	= 0xf000;
    configure.hardware	     	= 0x0008;	/* tx even w/o CD */
    configure.min_frame_len   	= 0x0040;
#else
    /*
     * below is the default board configuration from p2-28 from 586 book
     */
    configure.fifolim_bytecnt 	= 0x080c;
    configure.addrlen_mode  	= 0x2600;
    configure.linprio_interframe	= 0x7820;	/* IFS=120, ACS=2 */
    configure.slot_time      	= 0xf00c;	/* slottime=12    */
    configure.hardware	     	= 0x0008;	/* tx even w/o CD */
    configure.min_frame_len   	= 0x0040;
#endif
    if (sc->mode & (MOD_PROM | MOD_ENAL))
	configure.hardware |= 1;
    WL_WRITE_2(sc, PIOR1, OFFSET_CU + 6);
    WL_WRITE_MULTI_2(sc, PIOP1, &configure, sizeof(configure_t)/2);

    if (wlcmd(sc, "config()-configure") == 0)
	return 0;
#if	MULTICAST
    WL_WRITE_2(sc, PIOR1, OFFSET_CU);
    WL_WRITE_2(sc, PIOP1, 0);				/* ac_status */
    WL_WRITE_2(sc, PIOP1, AC_MCSETUP|AC_CW_EL);		/* ac_command */
    WL_WRITE_2(sc, PIOR1, OFFSET_CU + 8);
    if_maddr_rlock(sc->ifp);
    TAILQ_FOREACH(ifma, &sc->ifp->if_multiaddrs, ifma_link) {
	if (ifma->ifma_addr->sa_family != AF_LINK)
	    continue;
	
	addrp = LLADDR((struct sockaddr_dl *)ifma->ifma_addr);
        WL_WRITE_2(sc, PIOP1, addrp[0] + (addrp[1] << 8));
        WL_WRITE_2(sc, PIOP1, addrp[2] + (addrp[3] << 8));
        WL_WRITE_2(sc, PIOP1, addrp[4] + (addrp[5] << 8));
        ++cnt;
    }
    if_maddr_runlock(sc->ifp);
    WL_WRITE_2(sc, PIOR1, OFFSET_CU + 6);		/* mc-cnt */
    WL_WRITE_2(sc, PIOP1, cnt * WAVELAN_ADDR_SIZE);
    if (wlcmd(sc, "config()-mcaddress") == 0)
	return 0;
#endif	/* MULTICAST */

    WL_WRITE_2(sc, PIOR1, OFFSET_CU);
    WL_WRITE_2(sc, PIOP1, 0);				/* ac_status */
    WL_WRITE_2(sc, PIOP1, AC_IASETUP|AC_CW_EL);		/* ac_command */
    WL_WRITE_2(sc, PIOR1, OFFSET_CU + 6);
    WL_WRITE_MULTI_2(sc, PIOP1, IF_LLADDR(sc->ifp), WAVELAN_ADDR_SIZE/2);

    if (wlcmd(sc, "config()-address") == 0)
	return(0);

    wlinitmmc(sc);

    return(1);
}

/*
 * wlcmd:
 *
 * Set channel attention bit and busy wait until command has
 * completed. Then acknowledge the command completion.
 */
static int
wlcmd(struct wl_softc *sc, char *str)
{
    int i;
	
    WL_WRITE_2(sc, PIOR0, OFFSET_SCB + 2);	/* address of scb_command */
    WL_WRITE_2(sc, PIOP0, SCB_CU_STRT);
    
    SET_CHAN_ATTN(sc);
    
    WL_WRITE_2(sc, PIOR0, OFFSET_CU);
    for (i = 0; i < 0xffff; i++)
	if (WL_READ_2(sc, PIOP0) & AC_SW_C)
	    break;
    if (i == 0xffff || !(WL_READ_2(sc, PIOP0) & AC_SW_OK)) {
	if_printf(sc->ifp, "%s failed; status = %d, inw = %x, outw = %x\n",
	    str, WL_READ_2(sc, PIOP0) & AC_SW_OK, WL_READ_2(sc, PIOP0),
	    WL_READ_2(sc, PIOR0));
	WL_WRITE_2(sc, PIOR0, OFFSET_SCB);
	printf("scb_status %x\n", WL_READ_2(sc, PIOP0));
	WL_WRITE_2(sc, PIOR0, OFFSET_SCB+2);
	printf("scb_command %x\n", WL_READ_2(sc, PIOP0));
	WL_WRITE_2(sc, PIOR0, OFFSET_SCB+4);
	printf("scb_cbl %x\n", WL_READ_2(sc, PIOP0));
	WL_WRITE_2(sc, PIOR0, OFFSET_CU+2);
	printf("cu_cmd %x\n", WL_READ_2(sc, PIOP0));
	return(0);
    }

    WL_WRITE_2(sc, PIOR0, OFFSET_SCB);
    if ((WL_READ_2(sc, PIOP0) & SCB_SW_INT) &&
	(WL_READ_2(sc, PIOP0) != SCB_SW_CNA)) {
	/*
	  if_printf(sc->ifp, "%s: unexpected final state %x\n",
	      str, WL_READ_2(sc, PIOP0));
	  */
    }
    wlack(sc);
    return(TRUE);
}	

/*
 * wlack: if the 82596 wants attention because it has finished
 * sending or receiving a packet, acknowledge its desire and
 * return bits indicating the kind of attention. wlack() returns
 * these bits so that the caller can service exactly the
 * conditions that wlack() acknowledged.
 */
static int
wlack(struct wl_softc *sc)
{
    int i;
    u_short cmd;

    WL_WRITE_2(sc, PIOR1, OFFSET_SCB);
    if (!(cmd = (WL_READ_2(sc, PIOP1) & SCB_SW_INT)))
	return(0);
#ifdef WLDEBUG
    if (sc->ifp->if_flags & IFF_DEBUG)
	if_printf(sc->ifp, "doing a wlack()\n");
#endif
    WL_WRITE_2(sc, PIOP1, cmd);
    SET_CHAN_ATTN(sc);
    WL_WRITE_2(sc, PIOR0, OFFSET_SCB + 2);	/* address of scb_command */
    for (i = 1000000; WL_READ_2(sc, PIOP0) && (i-- > 0); )
	continue;
    if (i < 1)
	if_printf(sc->ifp, "wlack(): board not accepting command.\n");
    return(cmd);
}

#ifdef WLDEBUG
static void
wltbd(struct wl_softc *sc)
{
    u_short		tbd_p = OFFSET_TBD;
    tbd_t		tbd;
    int 		i = 0;
    int			sum = 0;

    for (;;) {
	WL_WRITE_2(sc, PIOR1, tbd_p);
	WL_READ_MULTI_2(sc, PIOP1, &tbd, sizeof(tbd_t)/2);
	sum += (tbd.act_count & ~TBD_SW_EOF);
	printf("%d: addr %x, count %d (%d), next %x, base %x\n",
	       i++, tbd.buffer_addr,
	       (tbd.act_count & ~TBD_SW_EOF), sum,
	       tbd.next_tbd_offset, tbd.buffer_base);
	if (tbd.act_count & TBD_SW_EOF)
	    break;
	tbd_p = tbd.next_tbd_offset;
    }
}
#endif

static void
wlhdwsleaze(u_short *countp, u_char **mb_pp, struct mbuf **tm_pp, struct wl_softc *sc)
{
    struct mbuf	*tm_p = *tm_pp;
    u_char		*mb_p = *mb_pp;
    u_short		count = 0;
    u_char		*cp;
    int		len;

    /*
     * can we get a run that will be coallesced or
     * that terminates before breaking
     */
    do {
	count += tm_p->m_len;
	if (tm_p->m_len & 1)
	    break;
    } while ((tm_p = tm_p->m_next) != (struct mbuf *)0);
    if ( (tm_p == (struct mbuf *)0) ||
	 count > HDW_THRESHOLD) {
	*countp = (*tm_pp)->m_len;
	*mb_pp = mtod((*tm_pp), u_char *);
	return;
    }

    /* we need to copy */
    tm_p = *tm_pp;
    mb_p = *mb_pp;
    count = 0;
    cp = (u_char *) t_packet;
    for (;;) {
	bcopy(mtod(tm_p, u_char *), cp, len = tm_p->m_len);
	count += len;
	if (count > HDW_THRESHOLD)
			break;
	cp += len;
	if (tm_p->m_next == (struct mbuf *)0)
	    break;
	tm_p = tm_p->m_next;
    }
    *countp = count;
    *mb_pp = (u_char *) t_packet;
    *tm_pp = tm_p;
    return;
}


static void
wlsftwsleaze(u_short *countp, u_char **mb_pp, struct mbuf **tm_pp, struct wl_softc *sc)
{
    struct mbuf	*tm_p = *tm_pp;
    u_short		count = 0;
    u_char		*cp = (u_char *) t_packet;
    int			len;

    /* we need to copy */
    for (;;) {
	bcopy(mtod(tm_p, u_char *), cp, len = tm_p->m_len);
	count += len;
	cp += len;
	if (tm_p->m_next == (struct mbuf *)0)
	    break;
	tm_p = tm_p->m_next;
    }

    *countp = count;
    *mb_pp = (u_char *) t_packet;
    *tm_pp = tm_p;
    return;
}

static void
wlmmcstat(struct wl_softc *sc)
{
    u_short tmp;

    device_printf(sc->dev, "DCE_STATUS: 0x%x, ",
	   wlmmcread(sc, MMC_DCE_STATUS) & 0x0f);
    tmp = wlmmcread(sc, MMC_CORRECT_NWID_H) << 8;
    tmp |= wlmmcread(sc, MMC_CORRECT_NWID_L);
    printf("Correct NWID's: %d, ", tmp);
    tmp = wlmmcread(sc, MMC_WRONG_NWID_H) << 8;
    tmp |= wlmmcread(sc, MMC_WRONG_NWID_L);
    printf("Wrong NWID's: %d\n", tmp);
    printf("THR_PRE_SET: 0x%x, ", wlmmcread(sc, MMC_THR_PRE_SET));
    printf("SIGNAL_LVL: %d, SILENCE_LVL: %d\n", 
	   wlmmcread(sc, MMC_SIGNAL_LVL),
	   wlmmcread(sc, MMC_SILENCE_LVL));
    printf("SIGN_QUAL: 0x%x, NETW_ID: %x:%x, DES: %d\n",
	   wlmmcread(sc, MMC_SIGN_QUAL),
	   wlmmcread(sc, MMC_NETW_ID_H),
	   wlmmcread(sc, MMC_NETW_ID_L),
	   wlmmcread(sc, MMC_DES_AVAIL));
}

static u_short
wlmmcread(struct wl_softc *sc, u_short reg)
{
    while (WL_READ_2(sc, HASR) & HASR_MMC_BUSY)
	continue;
    WL_WRITE_2(sc, MMCR,reg << 1);
    while (WL_READ_2(sc, HASR) & HASR_MMC_BUSY)
	continue;
    return (u_short)WL_READ_2(sc, MMCR) >> 8;
}

static void
getsnr(struct wl_softc *sc)
{
    MMC_WRITE(MMC_FREEZE,1);
    /* 
     * SNR retrieval procedure :
     *
     * read signal level : wlmmcread(sc, MMC_SIGNAL_LVL);
     * read silence level : wlmmcread(sc, MMC_SILENCE_LVL);
     */
    MMC_WRITE(MMC_FREEZE,0);
    /*
     * SNR is signal:silence ratio.
     */
}

/*
** wlgetpsa
**
** Reads the psa for the wavelan at (sc) into (buf)
*/
static void
wlgetpsa(struct wl_softc *sc, u_char *buf)
{
    int	i;

    PCMD(sc, HACR_DEFAULT & ~HACR_16BITS);
    PCMD(sc, HACR_DEFAULT & ~HACR_16BITS);

    for (i = 0; i < 0x40; i++) {
	WL_WRITE_2(sc, PIOR2, i);
	buf[i] = WL_READ_1(sc, PIOP2);
    }
    PCMD(sc, HACR_DEFAULT);
    PCMD(sc, HACR_DEFAULT);
}

/*
** wlsetpsa
**
** Writes the psa for wavelan (unit) from the softc back to the
** board.  Updates the CRC and sets the CRC OK flag.
**
** Do not call this when the board is operating, as it doesn't 
** preserve the hacr.
*/
static void
wlsetpsa(struct wl_softc *sc)
{
    int		i;
    u_short	crc;

    crc = wlpsacrc(sc->psa);	/* calculate CRC of PSA */
    sc->psa[WLPSA_CRCLOW] = crc & 0xff;
    sc->psa[WLPSA_CRCHIGH] = (crc >> 8) & 0xff;
    sc->psa[WLPSA_CRCOK] = 0x55;	/* default to 'bad' until programming complete */

    PCMD(sc, HACR_DEFAULT & ~HACR_16BITS);
    PCMD(sc, HACR_DEFAULT & ~HACR_16BITS);
    
    for (i = 0; i < 0x40; i++) {
	DELAY(DELAYCONST);
	WL_WRITE_2(sc, PIOR2, i);  /* write param memory */
	DELAY(DELAYCONST);
	WL_WRITE_1(sc, PIOP2, sc->psa[i]);
    }
    DELAY(DELAYCONST);
    WL_WRITE_2(sc, PIOR2, WLPSA_CRCOK);  /* update CRC flag*/
    DELAY(DELAYCONST);
    sc->psa[WLPSA_CRCOK] = 0xaa;	/* OK now */
    WL_WRITE_1(sc, PIOP2, 0xaa);	/* all OK */
    DELAY(DELAYCONST);
    
    PCMD(sc, HACR_DEFAULT);
    PCMD(sc, HACR_DEFAULT);
}

/* 
** CRC routine provided by Christopher Giordano <cgiordan@gdeb.com>,
** from original code by Tomi Mikkonen (tomitm@remedy.fi)
*/

static u_int crc16_table[16] = { 
    0x0000, 0xCC01, 0xD801, 0x1400,
    0xF001, 0x3C00, 0x2800, 0xE401,
    0xA001, 0x6C00, 0x7800, 0xB401,
    0x5000, 0x9C01, 0x8801, 0x4400 
};

static u_short
wlpsacrc(u_char *buf)
{
    u_short	crc = 0;
    int		i, r1;
    
    for (i = 0; i < 0x3d; i++, buf++) {
	/* lower 4 bits */
	r1 = crc16_table[crc & 0xF];
	crc = (crc >> 4) & 0x0FFF;
	crc = crc ^ r1 ^ crc16_table[*buf & 0xF];
	
	/* upper 4 bits */
	r1 = crc16_table[crc & 0xF];
	crc = (crc >> 4) & 0x0FFF;
	crc = crc ^ r1 ^ crc16_table[(*buf >> 4) & 0xF];
    }
    return(crc);
}
#ifdef WLCACHE

/*
 * wl_cache_store
 *
 * take input packet and cache various radio hw characteristics
 * indexed by MAC address.
 *
 * Some things to think about:
 *	note that no space is malloced. 
 *	We might hash the mac address if the cache were bigger.
 *	It is not clear that the cache is big enough.
 *		It is also not clear how big it should be.
 *	The cache is IP-specific.  We don't care about that as
 *		we want it to be IP-specific.
 *	The last N recv. packets are saved.  This will tend
 *		to reward agents and mobile hosts that beacon.
 *		That is probably fine for mobile ip.
 */

/* globals for wavelan signal strength cache */
/* this should go into softc structure above. 
*/

/* set true if you want to limit cache items to broadcast/mcast 
 * only packets (not unicast)
 */
static int wl_cache_mcastonly = 1;
SYSCTL_INT(_machdep, OID_AUTO, wl_cache_mcastonly, CTLFLAG_RW, 
	&wl_cache_mcastonly, 0, "");

/* set true if you want to limit cache items to IP packets only
*/
static int wl_cache_iponly = 1;
SYSCTL_INT(_machdep, OID_AUTO, wl_cache_iponly, CTLFLAG_RW, 
	&wl_cache_iponly, 0, "");

/* zero out the cache
*/
static void
wl_cache_zero(struct wl_softc *sc)
{

	bzero(&sc->w_sigcache[0], sizeof(struct w_sigcache) * MAXCACHEITEMS);
	sc->w_sigitems = 0;
	sc->w_nextcache = 0;
	sc->w_wrapindex = 0;
}

/* store hw signal info in cache.
 * index is MAC address, but an ip src gets stored too
 * There are two filters here controllable via sysctl:
 *	throw out unicast (on by default, but can be turned off)
 *	throw out non-ip (on by default, but can be turned off)
 */
static
void wl_cache_store (struct wl_softc *sc, struct ether_header *eh,
		     struct mbuf *m)
{
#ifdef INET
	struct ip *ip = NULL;	/* Avoid GCC warning */
	int i;
	int signal, silence;
	int w_insertcache;   /* computed index for cache entry storage */
	int ipflag = wl_cache_iponly;
#endif

	/* filters:
	 * 1. ip only
	 * 2. configurable filter to throw out unicast packets,
	 * keep multicast only.
	 */
 
#ifdef INET
	/* reject if not IP packet
	*/
	if ( wl_cache_iponly && (ntohs(eh->ether_type) != 0x800)) {
		return;
	}

	/* check if broadcast or multicast packet.  we toss
	 * unicast packets
	 */
	if (wl_cache_mcastonly && ((eh->ether_dhost[0] & 1) == 0)) {
		return;
	}

	/* find the ip header.  we want to store the ip_src
	 * address.  use the mtod macro(in mbuf.h) 
	 * to typecast m to struct ip *
	 */
	if (ipflag) {
		ip = mtod(m, struct ip *);
	}
        
	/* do a linear search for a matching MAC address 
	 * in the cache table
	 * . MAC address is 6 bytes,
	 * . var w_nextcache holds total number of entries already cached
	 */
	for (i = 0; i < sc->w_nextcache; i++) {
		if (! bcmp(eh->ether_shost, sc->w_sigcache[i].macsrc,  6 )) {
			/* Match!,
			 * so we already have this entry,
			 * update the data, and LRU age
			 */
			break;	
		}
	}

	/* did we find a matching mac address?
	 * if yes, then overwrite a previously existing cache entry
	 */
	if (i <  sc->w_nextcache )   {
		w_insertcache = i; 
	}
	/* else, have a new address entry,so
	 * add this new entry,
	 * if table full, then we need to replace entry
	 */
	else    {                          

		/* check for space in cache table 
		 * note: w_nextcache also holds number of entries
		 * added in the cache table 
		 */
		if ( sc->w_nextcache < MAXCACHEITEMS ) {
			w_insertcache = sc->w_nextcache;
			sc->w_nextcache++;                 
			sc->w_sigitems = sc->w_nextcache;
		}
        	/* no space found, so simply wrap with wrap index
		 * and "zap" the next entry
		 */
		else {
			if (sc->w_wrapindex == MAXCACHEITEMS) {
				sc->w_wrapindex = 0;
			}
			w_insertcache = sc->w_wrapindex++;
		}
	}

	/* invariant: w_insertcache now points at some slot
	 * in cache.
	 */
	if (w_insertcache < 0 || w_insertcache >= MAXCACHEITEMS) {
		log(LOG_ERR, 
			"wl_cache_store, bad index: %d of [0..%d], gross cache error\n",
			w_insertcache, MAXCACHEITEMS);
		return;
	}

	/*  store items in cache
	 *  .ipsrc
	 *  .macsrc
	 *  .signal (0..63) ,silence (0..63) ,quality (0..15)
	 */
	if (ipflag) {
		sc->w_sigcache[w_insertcache].ipsrc = ip->ip_src.s_addr;
	}
	bcopy( eh->ether_shost, sc->w_sigcache[w_insertcache].macsrc,  6);
	signal = sc->w_sigcache[w_insertcache].signal  = wlmmcread(sc, MMC_SIGNAL_LVL) & 0x3f;
	silence = sc->w_sigcache[w_insertcache].silence = wlmmcread(sc, MMC_SILENCE_LVL) & 0x3f;
	sc->w_sigcache[w_insertcache].quality = wlmmcread(sc, MMC_SIGN_QUAL) & 0x0f;
	if (signal > 0)
		sc->w_sigcache[w_insertcache].snr = 
			signal - silence;
	else
		sc->w_sigcache[w_insertcache].snr = 0;
#endif /* INET */

}
#endif /* WLCACHE */
