/*
 * All Rights Reserved, Copyright (C) Fujitsu Limited 1995
 *
 * This software may be used, modified, copied, distributed, and sold, in
 * both source and binary form provided that the above copyright, these
 * terms and the following disclaimer are retained.  The name of the author
 * and/or the contributor may not be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND THE CONTRIBUTOR ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR THE CONTRIBUTOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION.
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $Id: if_fe.c,v 1.49 1999/03/03 10:40:26 kato Exp $
 *
 * Device driver for Fujitsu MB86960A/MB86965A based Ethernet cards.
 * To be used with FreeBSD 3.x
 * Contributed by M. Sekiguchi. <seki@sysrap.cs.fujitsu.co.jp>
 *
 * This version is intended to be a generic template for various
 * MB86960A/MB86965A based Ethernet cards.  It currently supports
 * Fujitsu FMV-180 series for ISA and Allied-Telesis AT1700/RE2000
 * series for ISA, as well as Fujitsu MBH10302 PC card.
 * There are some currently-
 * unused hooks embedded, which are primarily intended to support
 * other types of Ethernet cards, but the author is not sure whether
 * they are useful.
 *
 * This version also includes some alignments to support RE1000,
 * C-NET(98)P2 and so on. These cards are not for AT-compatibles,
 * but for NEC PC-98 bus -- a proprietary bus architecture available
 * only in Japan. Confusingly, it is different from the Microsoft's
 * PC98 architecture. :-{
 * Further work for PC-98 version will be available as a part of
 * FreeBSD(98) project.
 *
 * This software is a derivative work of if_ed.c version 1.56 by David
 * Greenman available as a part of FreeBSD 2.0 RELEASE source distribution.
 *
 * The following lines are retained from the original if_ed.c:
 *
 * Copyright (C) 1993, David Greenman. This software may be used, modified,
 *   copied, distributed, and sold, in both source and binary form provided
 *   that the above copyright and these terms are retained. Under no
 *   circumstances is the author responsible for the proper functioning
 *   of this software, nor does the author assume any responsibility
 *   for damages incurred with its use.
 */

/*
 * TODO:
 *  o   To support ISA PnP auto configuration for FMV-183/184.
 *  o   To reconsider mbuf usage.
 *  o   To reconsider transmission buffer usage, including
 *      transmission buffer size (currently 4KB x 2) and pros-and-
 *      cons of multiple frame transmission.
 *  o   To test IPX codes.
 *  o   To test FreeBSD3.0-current.
 *  o   To test BRIDGE codes.
 */

#include "fe.h"
#include "bpfilter.h"
#include "opt_fe.h"
#include "opt_inet.h"
#include "opt_ipx.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_mib.h>
#include <net/if_media.h>
#include <net/if_types.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

/* IPX code is not tested.  FIXME.  */
#ifdef IPX
#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#endif

/* To be used with IPv6 package of INRIA.  */
#ifdef INET6
/* IPv6 added by shin 96.2.6 */
#include <netinet/if_ether6.h>
#endif

/* XNS code is not tested.  FIXME.  */
#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#ifdef BRIDGE
#include <net/bridge.h>
#endif

#include <machine/clock.h>

#include <i386/isa/isa_device.h>
#include <i386/isa/icu.h>

/* PCCARD suport */
#include "card.h"
#if NCARD > 0
#include <sys/kernel.h>
#include <sys/select.h>
#include <sys/module.h>
#include <pccard/cardinfo.h>
#include <pccard/slot.h>
#endif

#include <i386/isa/ic/mb86960.h>
#include <i386/isa/if_fereg.h>

/*
 * Default settings for fe driver specific options.
 * They can be set in config file by "options" statements.
 */

/*
 * Transmit just one packet per a "send" command to 86960.
 * This option is intended for performance test.  An EXPERIMENTAL option.
 */
#ifndef FE_SINGLE_TRANSMISSION
#define FE_SINGLE_TRANSMISSION 0
#endif

/*
 * Maximum loops when interrupt.
 * This option prevents an infinite loop due to hardware failure.
 * (Some laptops make an infinite loop after PC-Card is ejected.)
 */
#ifndef FE_MAX_LOOP
#define FE_MAX_LOOP 0x800
#endif

/*
 * If you define this option, 8-bit cards are also supported.
 */
/*#define FE_8BIT_SUPPORT*/

/*
 * Device configuration flags.
 */

/* DLCR6 settings.  */
#define FE_FLAGS_DLCR6_VALUE	0x007F

/* Force DLCR6 override.  */
#define FE_FLAGS_OVERRIDE_DLCR6	0x0080

/* Shouldn't these be defined somewhere else such as isa_device.h?  */
#define NO_IOADDR	(-1)
#define NO_IRQ		0

/*
 * Data type for a multicast address filter on 8696x.
 */
struct fe_filter { u_char data [ FE_FILTER_LEN ]; };

/*
 * Special filter values.
 */
static struct fe_filter const fe_filter_nothing = { FE_FILTER_NOTHING };
static struct fe_filter const fe_filter_all     = { FE_FILTER_ALL };

/* How many registers does an fe-supported adapter have at maximum?  */
#define MAXREGISTERS 32

/*
 * fe_softc: per line info and status
 */
static struct fe_softc {

	/* Used by "common" codes.  */
	struct arpcom arpcom;	/* Ethernet common */

	/* Used by config codes.  */

	/* Set by probe() and not modified in later phases.  */
	char const * typestr;	/* printable name of the interface.  */
	u_short iobase;		/* base I/O address of the adapter.  */
	u_short ioaddr [ MAXREGISTERS ]; /* I/O addresses of registers.  */
	u_short txb_size;	/* size of TX buffer, in bytes  */
	u_char proto_dlcr4;	/* DLCR4 prototype.  */
	u_char proto_dlcr5;	/* DLCR5 prototype.  */
	u_char proto_dlcr6;	/* DLCR6 prototype.  */
	u_char proto_dlcr7;	/* DLCR7 prototype.  */
	u_char proto_bmpr13;	/* BMPR13 prototype.  */
	u_char stability;	/* How stable is this?  */ 
	u_short priv_info;	/* info specific to a vendor/model.  */

	/* Vendor/model specific hooks.  */
	void (*init)(struct fe_softc *); /* Just before fe_init().  */
	void (*stop)(struct fe_softc *); /* Just after fe_stop().  */

	/* Transmission buffer management.  */
	u_short txb_free;	/* free bytes in TX buffer  */
	u_char txb_count;	/* number of packets in TX buffer  */
	u_char txb_sched;	/* number of scheduled packets  */

	/* Excessive collision counter (see fe_tint() for details.)  */
	u_char tx_excolls;	/* # of excessive collisions.  */

	/* Multicast address filter management.  */
	u_char filter_change;	/* MARs must be changed ASAP. */
	struct fe_filter filter;/* new filter value.  */

	/* Network management.  */
	struct ifmib_iso_8802_3 mibdata;

	/* Media information.  */
	struct ifmedia media;	/* used by if_media.  */
	u_short mbitmap;	/* bitmap for supported media; see bit2media */
	int defmedia;		/* default media  */
	void (* msel)(struct fe_softc *); /* media selector.  */

}       fe_softc[NFE];

#define sc_if		arpcom.ac_if
#define sc_unit		arpcom.ac_if.if_unit
#define sc_enaddr	arpcom.ac_enaddr

/* Standard driver entry points.  These can be static.  */
static int		fe_probe	( struct isa_device * );
static int		fe_attach	( struct isa_device * );
static void		fe_init		( void * );
static ointhand2_t	feintr;
static int		fe_ioctl	( struct ifnet *, u_long, caddr_t );
static void		fe_start	( struct ifnet * );
static void		fe_watchdog	( struct ifnet * );
static int		fe_medchange	( struct ifnet * );
static void		fe_medstat	( struct ifnet *, struct ifmediareq * );

/* Local functions.  Order of declaration is confused.  FIXME.  */
static int	fe_probe_ssi	( struct isa_device *, struct fe_softc * );
static int	fe_probe_jli	( struct isa_device *, struct fe_softc * );
static int	fe_probe_fmv	( struct isa_device *, struct fe_softc * );
static int	fe_probe_lnx	( struct isa_device *, struct fe_softc * );
static int	fe_probe_gwy	( struct isa_device *, struct fe_softc * );
static int	fe_probe_ubn	( struct isa_device *, struct fe_softc * );
#ifdef PC98
static int	fe_probe_re1000	( struct isa_device *, struct fe_softc * );
static int	fe_probe_cnet9ne( struct isa_device *, struct fe_softc * );
#endif
#if NCARD > 0
static int	fe_probe_mbh	( struct isa_device *, struct fe_softc * );
static int	fe_probe_tdk	( struct isa_device *, struct fe_softc * );
#endif
static int	fe_get_packet	( struct fe_softc *, u_short );
static void	fe_stop		( struct fe_softc * );
static void	fe_tint		( struct fe_softc *, u_char );
static void	fe_rint		( struct fe_softc *, u_char );
static void	fe_xmit		( struct fe_softc * );
static void	fe_write_mbufs	( struct fe_softc *, struct mbuf * );
static void	fe_setmode	( struct fe_softc * );
static void	fe_loadmar	( struct fe_softc * );

#ifdef DIAGNOSTIC
static void	fe_emptybuffer	( struct fe_softc * );
#endif

/* Driver struct used in the config code.  This must be public (external.)  */
struct isa_driver fedriver =
{
	fe_probe,
	fe_attach,
	"fe",
	1			/* It's safe to mark as "sensitive"  */
};

/*
 * Fe driver specific constants which relate to 86960/86965.
 */

/* Interrupt masks  */
#define FE_TMASK ( FE_D2_COLL16 | FE_D2_TXDONE )
#define FE_RMASK ( FE_D3_OVRFLO | FE_D3_CRCERR \
		 | FE_D3_ALGERR | FE_D3_SRTPKT | FE_D3_PKTRDY )

/* Maximum number of iterations for a receive interrupt.  */
#define FE_MAX_RECV_COUNT ( ( 65536 - 2048 * 2 ) / 64 )
	/*
	 * Maximum size of SRAM is 65536,
	 * minimum size of transmission buffer in fe is 2x2KB,
	 * and minimum amount of received packet including headers
	 * added by the chip is 64 bytes.
	 * Hence FE_MAX_RECV_COUNT is the upper limit for number
	 * of packets in the receive buffer.
	 */

/*
 * Miscellaneous definitions not directly related to hardware.
 */

/* Flags for stability.  */
#define UNSTABLE_IRQ	0x01	/* IRQ setting may be incorrect.  */
#define UNSTABLE_MAC	0x02	/* Probed MAC address may be incorrect.  */
#define UNSTABLE_TYPE	0x04	/* Probed vendor/model may be incorrect.  */

/* The following line must be delete when "net/if_media.h" support it.  */
#ifndef IFM_10_FL
#define IFM_10_FL	/* 13 */ IFM_10_5
#endif

#if 0
/* Mapping between media bitmap (in fe_softc.mbitmap) and ifm_media.  */
static int const bit2media [] = {
#define MB_HA	0x0001
			IFM_HDX | IFM_ETHER | IFM_AUTO,
#define MB_HM	0x0002
			IFM_HDX | IFM_ETHER | IFM_MANUAL,
#define MB_HT	0x0004
			IFM_HDX | IFM_ETHER | IFM_10_T,
#define MB_H2	0x0008
			IFM_HDX | IFM_ETHER | IFM_10_2,
#define MB_H5	0x0010
			IFM_HDX | IFM_ETHER | IFM_10_5,
#define MB_HF	0x0020
			IFM_HDX | IFM_ETHER | IFM_10_FL,
#define MB_FT	0x0040
			IFM_FDX | IFM_ETHER | IFM_10_T,
	/* More can be come here... */
			0
};
#else
/* Mapping between media bitmap (in fe_softc.mbitmap) and ifm_media.  */
static int const bit2media [] = {
#define MB_HA	0x0001
			IFM_ETHER | IFM_AUTO,
#define MB_HM	0x0002
			IFM_ETHER | IFM_MANUAL,
#define MB_HT	0x0004
			IFM_ETHER | IFM_10_T,
#define MB_H2	0x0008
			IFM_ETHER | IFM_10_2,
#define MB_H5	0x0010
			IFM_ETHER | IFM_10_5,
#define MB_HF	0x0020
			IFM_ETHER | IFM_10_FL,
#define MB_FT	0x0040
			IFM_ETHER | IFM_10_T,
	/* More can be come here... */
			0
};
#endif

/*
 * Routines to access contiguous I/O ports.
 */

static void
inblk ( struct fe_softc * sc, int offs, u_char * mem, int len )
{
	while ( --len >= 0 ) {
		*mem++ = inb( sc->ioaddr[ offs++ ] );
	}
}

static void
outblk ( struct fe_softc * sc, int offs, u_char const * mem, int len )
{
	while ( --len >= 0 ) {
		outb( sc->ioaddr[ offs++ ], *mem++ );
	}
}

/* PCCARD Support */
#if NCARD > 0
/*
 *      PC-Card (PCMCIA) specific code.
 */
static int	feinit		(struct pccard_devinfo *);
static void	feunload	(struct pccard_devinfo *);
static int	fe_card_intr	(struct pccard_devinfo *);

PCCARD_MODULE(fe, feinit, feunload, fe_card_intr, 0, net_imask);

/*
 *      Initialize the device - called from Slot manager.
 */
static int
feinit(struct pccard_devinfo *devi)
{
        struct fe_softc *sc;

	/* validate unit number.  */
	if (devi->isahd.id_unit >= NFE) return ENODEV;

	/* Prepare for the device probe process.  */
	sc = &fe_softc[devi->isahd.id_unit];
	sc->sc_unit = devi->isahd.id_unit;
	sc->iobase = devi->isahd.id_iobase;

	/*
	 * When the feinit() is called, the devi->misc holds a
	 * six-byte value set by the pccard daemon.  If the
	 * corresponding entry in /etc/pccard.conf has an "ether"
	 * keyword, the value is the Ethernet MAC address extracted
	 * from CIS area of the card.  If the entry has no "ether"
	 * keyword, the daemon fills the field with binary zero,
	 * instead.  We passes the value (either MAC address or zero)
	 * to model-specific sub-probe routines through sc->sc_enaddr
	 * (it actually is sc->sc_arpcom.ar_enaddr, BTW) so that the
	 * sub-probe routies can use that info.
	 */
	bcopy(devi->misc, sc->sc_enaddr, ETHER_ADDR_LEN);

	/* Probe for supported cards.  */
	if (fe_probe_mbh(&devi->isahd, sc) == 0
	 && fe_probe_tdk(&devi->isahd, sc) == 0) return ENXIO;

	/* We've got a supported card.  Attach it, then.  */
	if (fe_attach(&devi->isahd) == 0) return ENXIO;

	return 0;
}

/*
 *	feunload - unload the driver and clear the table.
 *	XXX TODO:
 *	This is usually called when the card is ejected, but
 *	can be caused by a modunload of a controller driver.
 *	The idea is to reset the driver's view of the device
 *	and ensure that any driver entry points such as
 *	read and write do not hang.
 */
static void
feunload(struct pccard_devinfo *devi)
{
	struct fe_softc *sc = &fe_softc[devi->isahd.id_unit];
	printf("fe%d: unload\n", sc->sc_unit);
	fe_stop(sc);
	if_down(&sc->arpcom.ac_if);
}

/*
 *	fe_card_intr - Shared interrupt called from
 *	 front end of PC-Card handler.
 */
static int
fe_card_intr(struct pccard_devinfo *devi)
{
	feintr(devi->isahd.id_unit);
	return (1);
}
#endif /* NCARD > 0 */


/*
 * Hardware probe routines.
 *
 * In older versions of this driver, we provided an automatic I/O
 * address detection.  The features is, however, removed from this
 * version, for simplicity.  Any comments?
 */

/*
 * Determine if the device is present at a specified I/O address.  The
 * main entry to the driver.
 */

static int
fe_probe (struct isa_device * dev)
{
	struct fe_softc * sc;
	int nports;

#ifdef DIAGNOSTIC
	if (dev->id_unit >= NFE) {
		printf("fe%d: too large unit number for the current config\n",
		       dev->id_unit);
		return 0;
	}
#endif

	/* Prepare for the softc struct.  */
	sc = &fe_softc[dev->id_unit];
	sc->sc_unit = dev->id_unit;
	sc->iobase = dev->id_iobase;

	/* Probe for supported boards.  */
	nports = 0;
#ifdef PC98
	if (!nports) nports = fe_probe_re1000(dev, sc);
	if (!nports) nports = fe_probe_cnet9ne(dev, sc);
#endif
	if (!nports) nports = fe_probe_ssi(dev, sc);
	if (!nports) nports = fe_probe_jli(dev, sc);
	if (!nports) nports = fe_probe_fmv(dev, sc);
	if (!nports) nports = fe_probe_lnx(dev, sc);
	if (!nports) nports = fe_probe_ubn(dev, sc);
	if (!nports) nports = fe_probe_gwy(dev, sc);

	/* We found supported board.  */
	return nports;
}

/*
 * Check for specific bits in specific registers have specific values.
 * A common utility function called from various sub-probe routines.
 */

struct fe_simple_probe_struct
{
	u_char port;	/* Offset from the base I/O address.  */
	u_char mask;	/* Bits to be checked.  */
	u_char bits;	/* Values to be compared against.  */
};

static int
fe_simple_probe ( struct fe_softc const * sc,
		  struct fe_simple_probe_struct const * sp )
{
	struct fe_simple_probe_struct const * p;

	for ( p = sp; p->mask != 0; p++ ) {
#ifdef FE_DEBUG
	        unsigned a = sc->ioaddr[p->port];
		printf("fe%d: Probing %02x (%04x): %02x (%02x, %02x): %s\n",
		       sc->sc_unit, p->port, a, inb(a), p->mask, p->bits,
		       (inb(a) & p->mask) == p->bits ? "OK" : "NG");
#endif
		if ( ( inb( sc->ioaddr[ p->port ] ) & p->mask ) != p->bits ) 
		{
			return ( 0 );
		}
	}
	return ( 1 );
}

/* Test if a given 6 byte value is a valid Ethernet station (MAC)
   address.  "Vendor" is an expected vendor code (first three bytes,)
   or a zero when nothing expected.  */
static int
valid_Ether_p (u_char const * addr, unsigned vendor)
{
#ifdef FE_DEBUG
	printf("fe?: validating %6D against %06x\n", addr, ":", vendor);
#endif

	/* All zero is not allowed as a vendor code.  */
	if (addr[0] == 0 && addr[1] == 0 && addr[2] == 0) return 0;

	switch (vendor) {
	    case 0x000000:
		/* Legal Ethernet address (stored in ROM) must have
		   its Group and Local bits cleared.  */
		if ((addr[0] & 0x03) != 0) return 0;
		break;
	    case 0x020000:
		/* Same as above, but a local address is allowed in
                   this context.  */
		if ((addr[0] & 0x01) != 0) return 0;
		break;
	    default:
		/* Make sure the vendor part matches if one is given.  */
		if (   addr[0] != ((vendor >> 16) & 0xFF)
		    || addr[1] != ((vendor >>  8) & 0xFF)
		    || addr[2] != ((vendor      ) & 0xFF)) return 0;
		break;
	}

	/* Host part must not be all-zeros nor all-ones.  */
	if (addr[3] == 0xFF && addr[4] == 0xFF && addr[5] == 0xFF) return 0;
	if (addr[3] == 0x00 && addr[4] == 0x00 && addr[5] == 0x00) return 0;

	/* Given addr looks like an Ethernet address.  */
	return 1;
}

/* Fill our softc struct with default value.  */
static void
fe_softc_defaults (struct fe_softc *sc)
{
	int i;

	/* Initialize I/O address re-mapping table for the standard
	   (contiguous) register layout.  This routine doesn't use
	   ioaddr[], so the caller can safely override it after
	   calling fe_softc_defaults, if needed.  */
	for (i = 0; i < MAXREGISTERS; i++) sc->ioaddr[i] = sc->iobase + i;

	/* Prepare for typical register prototypes.  We assume a
           "typical" board has <32KB> of <fast> SRAM connected with a
           <byte-wide> data lines.  */
	sc->proto_dlcr4 = FE_D4_LBC_DISABLE | FE_D4_CNTRL;
	sc->proto_dlcr5 = 0;
	sc->proto_dlcr6 = FE_D6_BUFSIZ_32KB | FE_D6_TXBSIZ_2x4KB
		| FE_D6_BBW_BYTE | FE_D6_SBW_WORD | FE_D6_SRAM_100ns;
	sc->proto_dlcr7 = FE_D7_BYTSWP_LH;
	sc->proto_bmpr13 = 0;

	/* Assume the probe process (to be done later) is stable.  */
	sc->stability = 0;

	/* A typical board needs no hooks.  */
	sc->init = NULL;
	sc->stop = NULL;

	/* Assume the board has no software-controllable media selection.  */
	sc->mbitmap = MB_HM;
	sc->defmedia = MB_HM;
	sc->msel = NULL;
}

/* Common error reporting routine used in probe routines for
   "soft configured IRQ"-type boards.  */
static void
fe_irq_failure (char const *name, int unit, int irq, char const *list)
{
	printf("fe%d: %s board is detected, but %s IRQ was given\n",
	       unit, name, (irq == NO_IRQ ? "no" : "invalid"));
	if (list != NULL) {
		printf("fe%d: specify an IRQ from %s in kernel config\n",
		       unit, list);
	}
}

/*
 * Hardware (vendor) specific probe routines and hooks.
 */

/*
 * Machine independent routines.
 */

/*
 * Generic media selection scheme for MB86965 based boards.
 */
static void
fe_msel_965 (struct fe_softc *sc)
{
	u_char b13;

	/* Find the appropriate bits for BMPR13 tranceiver control.  */
	switch (IFM_SUBTYPE(sc->media.ifm_media)) {
	    case IFM_AUTO: b13 = FE_B13_PORT_AUTO | FE_B13_TPTYPE_UTP; break;
	    case IFM_10_T: b13 = FE_B13_PORT_TP   | FE_B13_TPTYPE_UTP; break;
	    default:       b13 = FE_B13_PORT_AUI;  break;
	}

	/* Write it into the register.  It takes effect immediately.  */
	outb(sc->ioaddr[FE_BMPR13], sc->proto_bmpr13 | b13);
}

/*
 * Fujitsu MB86965 JLI mode support routines.
 */

/* Datasheet for 86965 explicitly states that it only supports serial
 * EEPROM with 16 words (32 bytes) capacity.  (I.e., 93C06.)  However,
 * ones with 64 words (128 bytes) are available in the marked, namely
 * 93C46, and are also fully compatible with 86965.  It is known that
 * some boards (e.g., ICL) actually have 93C46 on them and use extra
 * storage to keep various config info.  */
#define JLI_EEPROM_SIZE	128

/*
 * Routines to read all bytes from the config EEPROM through MB86965A.
 * It is a MicroWire (3-wire) serial EEPROM with 6-bit address.
 * (93C06 or 93C46.)
 */
static void
fe_strobe_eeprom_jli ( u_short bmpr16 )
{
	/*
	 * We must guarantee 1us (or more) interval to access slow
	 * EEPROMs.  The following redundant code provides enough
	 * delay with ISA timing.  (Even if the bus clock is "tuned.")
	 * Some modification will be needed on faster busses.
	 */
	outb( bmpr16, FE_B16_SELECT );
	outb( bmpr16, FE_B16_SELECT | FE_B16_CLOCK );
	outb( bmpr16, FE_B16_SELECT | FE_B16_CLOCK );
	outb( bmpr16, FE_B16_SELECT );
}

static void
fe_read_eeprom_jli ( struct fe_softc * sc, u_char * data )
{
	u_short bmpr16 = sc->ioaddr[ FE_BMPR16 ];
	u_short bmpr17 = sc->ioaddr[ FE_BMPR17 ];
	u_char n, val, bit;
	u_char save16, save17;

	/* Save the current value of the EEPROM interface registers.  */
	save16 = inb(bmpr16);
	save17 = inb(bmpr17);

	/* Read bytes from EEPROM; two bytes per an iteration.  */
	for ( n = 0; n < JLI_EEPROM_SIZE / 2; n++ ) {

		/* Reset the EEPROM interface.  */
		outb( bmpr16, 0x00 );
		outb( bmpr17, 0x00 );

		/* Start EEPROM access.  */
		outb( bmpr16, FE_B16_SELECT );
		outb( bmpr17, FE_B17_DATA );
		fe_strobe_eeprom_jli( bmpr16 );

		/* Pass the iteration count as well as a READ command.  */
		val = 0x80 | n;
		for ( bit = 0x80; bit != 0x00; bit >>= 1 ) {
			outb( bmpr17, ( val & bit ) ? FE_B17_DATA : 0 );
			fe_strobe_eeprom_jli( bmpr16 );
		}
		outb( bmpr17, 0x00 );

		/* Read a byte.  */
		val = 0;
		for ( bit = 0x80; bit != 0x00; bit >>= 1 ) {
			fe_strobe_eeprom_jli( bmpr16 );
			if ( inb( bmpr17 ) & FE_B17_DATA ) {
				val |= bit;
			}
		}
		*data++ = val;

		/* Read one more byte.  */
		val = 0;
		for ( bit = 0x80; bit != 0x00; bit >>= 1 ) {
			fe_strobe_eeprom_jli( bmpr16 );
			if ( inb( bmpr17 ) & FE_B17_DATA ) {
				val |= bit;
			}
		}
		*data++ = val;
	}

#if 0
	/* Reset the EEPROM interface, again.  */
	outb( bmpr16, 0x00 );
	outb( bmpr17, 0x00 );
#else
	/* Make sure to restore the original value of EEPROM interface
           registers, since we are not yet sure we have MB86965A on
           the address.  */
	outb(bmpr17, save17);
	outb(bmpr16, save16);
#endif

#if 1
	/* Report what we got.  */
	if (bootverbose) {
		int i;
		data -= JLI_EEPROM_SIZE;
		for (i = 0; i < JLI_EEPROM_SIZE; i += 16) {
			printf("fe%d: EEPROM(JLI):%3x: %16D\n",
			       sc->sc_unit, i, data + i, " ");
		}
	}
#endif
}

static void
fe_init_jli (struct fe_softc * sc)
{
	/* "Reset" by writing into a magic location.  */
	DELAY(200);
	outb(sc->ioaddr[0x1E], inb(sc->ioaddr[0x1E]));
	DELAY(300);
}

/*
 * SSi 78Q8377A support routines.
 */

#define SSI_EEPROM_SIZE	512
#define SSI_DIN	0x01
#define SSI_DAT	0x01
#define SSI_CSL	0x02
#define SSI_CLK	0x04
#define SSI_EEP	0x10

/*
 * Routines to read all bytes from the config EEPROM through 78Q8377A.
 * It is a MicroWire (3-wire) serial EEPROM with 8-bit address.  (I.e.,
 * 93C56 or 93C66.)
 *
 * As I don't have SSi manuals, (hmm, an old song again!) I'm not exactly
 * sure the following code is correct...  It is just stolen from the
 * C-NET(98)P2 support routine in FreeBSD(98).
 */

static void
fe_read_eeprom_ssi (struct fe_softc *sc, u_char *data)
{
	u_short	bmpr12 = sc->ioaddr[FE_DLCR12];
	u_char val, bit;
	int n;
	u_char save6, save7, save12;

	/* Save the current value for the DLCR registers we are about
           to destroy.  */
	save6 = inb(sc->ioaddr[FE_DLCR6]);
	save7 = inb(sc->ioaddr[FE_DLCR7]);

	/* Put the 78Q8377A into a state that we can access the EEPROM.  */
	outb(sc->ioaddr[FE_DLCR6],
	     FE_D6_BBW_WORD | FE_D6_SBW_WORD | FE_D6_DLC_DISABLE);
	outb(sc->ioaddr[FE_DLCR7],
	     FE_D7_BYTSWP_LH | FE_D7_RBS_BMPR | FE_D7_RDYPNS | FE_D7_POWER_UP);

	/* Save the current value for the BMPR12 register, too.  */
	save12 = inb(bmpr12);

	/* Read bytes from EEPROM; two bytes per an iteration.  */
	for ( n = 0; n < SSI_EEPROM_SIZE / 2; n++ ) {

		/* Start EEPROM access  */
		outb(bmpr12, SSI_EEP);
		outb(bmpr12, SSI_EEP | SSI_CSL);

		/* Send the following four bits to the EEPROM in the
                   specified order: a dummy bit, a start bit, and
                   command bits (10) for READ.  */
		outb(bmpr12, SSI_EEP | SSI_CSL                    );
		outb(bmpr12, SSI_EEP | SSI_CSL | SSI_CLK          );	/* 0 */
		outb(bmpr12, SSI_EEP | SSI_CSL           | SSI_DAT);
		outb(bmpr12, SSI_EEP | SSI_CSL | SSI_CLK | SSI_DAT);	/* 1 */
		outb(bmpr12, SSI_EEP | SSI_CSL           | SSI_DAT);
		outb(bmpr12, SSI_EEP | SSI_CSL | SSI_CLK | SSI_DAT);	/* 1 */
		outb(bmpr12, SSI_EEP | SSI_CSL                    );
		outb(bmpr12, SSI_EEP | SSI_CSL | SSI_CLK          );	/* 0 */

		/* Pass the iteration count to the chip.  */
		for ( bit = 0x80; bit != 0x00; bit >>= 1 ) {
			val = ( n & bit ) ? SSI_DAT : 0;
			outb(bmpr12, SSI_EEP | SSI_CSL           | val);
			outb(bmpr12, SSI_EEP | SSI_CSL | SSI_CLK | val);
		}

		/* Read a byte.  */
		val = 0;
		for ( bit = 0x80; bit != 0x00; bit >>= 1 ) {
			outb(bmpr12, SSI_EEP | SSI_CSL);
			outb(bmpr12, SSI_EEP | SSI_CSL | SSI_CLK);
			if (inb(bmpr12) & SSI_DIN) val |= bit;
		}
		*data++ = val;

		/* Read one more byte.  */
		val = 0;
		for ( bit = 0x80; bit != 0x00; bit >>= 1 ) {
			outb(bmpr12, SSI_EEP | SSI_CSL);
			outb(bmpr12, SSI_EEP | SSI_CSL | SSI_CLK);
			if (inb(bmpr12) & SSI_DIN) val |= bit;
		}
		*data++ = val;

		outb(bmpr12, SSI_EEP);
	}

	/* Reset the EEPROM interface.  (For now.)  */
	outb( bmpr12, 0x00 );

	/* Restore the saved register values, for the case that we
           didn't have 78Q8377A at the given address.  */
	outb(sc->ioaddr[FE_BMPR12], save12);
	outb(sc->ioaddr[FE_DLCR7], save7);
	outb(sc->ioaddr[FE_DLCR6], save6);

#if 1
	/* Report what we got.  */
	if (bootverbose) {
		int i;
		data -= SSI_EEPROM_SIZE;
		for (i = 0; i < SSI_EEPROM_SIZE; i += 16) {
			printf("fe%d: EEPROM(SSI):%3x: %16D\n",
			       sc->sc_unit, i, data + i, " ");
		}
	}
#endif
}

#define	FE_SSI_EEP_IRQ		9	/* Irq ???		*/
#define	FE_SSI_EEP_ADDR		16	/* Station(MAC) address	*/
#define	FE_SSI_EEP_DUPLEX	25	/* Duplex mode ???	*/

/*
 * TDK/LANX boards support routines.
 */

/* AX012/AX013 equips an X24C01 chip, which has 128 bytes of memory cells.  */
#define LNX_EEPROM_SIZE	128

/* Bit assignments and command definitions for the serial EEPROM
   interface register in LANX ASIC.  */
#define LNX_SDA_HI	0x08	/* Drive SDA line high (logical 1.)	*/
#define LNX_SDA_LO	0x00	/* Drive SDA line low (logical 0.)	*/
#define LNX_SDA_FL	0x08	/* Float (don't drive) SDA line.	*/
#define LNX_SDA_IN	0x01	/* Mask for reading SDA line.		*/
#define LNX_CLK_HI	0x04	/* Drive clock line high (active.)	*/
#define LNX_CLK_LO	0x00	/* Drive clock line low (inactive.)	*/

/* It is assumed that the CLK line is low and SDA is high (float) upon entry.  */
#define LNX_PH(D,K,N) \
	((LNX_SDA_##D | LNX_CLK_##K) << N)
#define LNX_CYCLE(D1,D2,D3,D4,K1,K2,K3,K4) \
	(LNX_PH(D1,K1,0)|LNX_PH(D2,K2,8)|LNX_PH(D3,K3,16)|LNX_PH(D4,K4,24))

#define LNX_CYCLE_START	LNX_CYCLE(HI,LO,LO,HI, HI,HI,LO,LO)
#define LNX_CYCLE_STOP	LNX_CYCLE(LO,LO,HI,HI, LO,HI,HI,LO)
#define LNX_CYCLE_HI	LNX_CYCLE(HI,HI,HI,HI, LO,HI,LO,LO)
#define LNX_CYCLE_LO	LNX_CYCLE(LO,LO,LO,HI, LO,HI,LO,LO)
#define LNX_CYCLE_INIT	LNX_CYCLE(LO,HI,HI,HI, LO,LO,LO,LO)

static void
fe_eeprom_cycle_lnx (u_short reg20, u_long cycle)
{
	outb(reg20, (cycle      ) & 0xFF);
	DELAY(15);
	outb(reg20, (cycle >>  8) & 0xFF);
	DELAY(15);
	outb(reg20, (cycle >> 16) & 0xFF);
	DELAY(15);
	outb(reg20, (cycle >> 24) & 0xFF);
	DELAY(15);
}

static u_char
fe_eeprom_receive_lnx (u_short reg20)
{
	u_char dat;

	outb(reg20, LNX_CLK_HI | LNX_SDA_FL);
	DELAY(15);
	dat = inb(reg20);
	outb(reg20, LNX_CLK_LO | LNX_SDA_FL);
	DELAY(15);
	return (dat & LNX_SDA_IN);
}

static void
fe_read_eeprom_lnx (struct fe_softc *sc, u_char *data)
{
	int i;
	u_char n, bit, val;
	u_char save20;
	u_short reg20 = sc->ioaddr[0x14];

	save20 = inb(sc->ioaddr[0x14]);

	/* NOTE: DELAY() timing constants are approximately three
           times longer (slower) than the required minimum.  This is
           to guarantee a reliable operation under some tough
           conditions...  Fortunately, this routine is only called
           during the boot phase, so the speed is less important than
           stability.  */

#if 1
	/* Reset the X24C01's internal state machine and put it into
	   the IDLE state.  We usually don't need this, but *if*
	   someone (e.g., probe routine of other driver) write some
	   garbage into the register at 0x14, synchronization will be
	   lost, and the normal EEPROM access protocol won't work.
	   Moreover, as there are no easy way to reset, we need a
	   _manoeuvre_ here.  (It even lacks a reset pin, so pushing
	   the RESET button on the PC doesn't help!)  */
	fe_eeprom_cycle_lnx(reg20, LNX_CYCLE_INIT);
	for (i = 0; i < 10; i++) {
		fe_eeprom_cycle_lnx(reg20, LNX_CYCLE_START);
	}
	fe_eeprom_cycle_lnx(reg20, LNX_CYCLE_STOP);
	DELAY(10000);
#endif

	/* Issue a start condition.  */
	fe_eeprom_cycle_lnx(reg20, LNX_CYCLE_START);

	/* Send seven bits of the starting address (zero, in this
	   case) and a command bit for READ.  */
	val = 0x01;
	for (bit = 0x80; bit != 0x00; bit >>= 1) {
		if (val & bit) {
			fe_eeprom_cycle_lnx(reg20, LNX_CYCLE_HI);
		} else {
			fe_eeprom_cycle_lnx(reg20, LNX_CYCLE_LO);
		}
	}

	/* Receive an ACK bit.  */
	if (fe_eeprom_receive_lnx(reg20)) {
		/* ACK was not received.  EEPROM is not present (i.e.,
		   this board was not a TDK/LANX) or not working
		   properly.  */
		if (bootverbose) {
			printf("fe%d: no ACK received from EEPROM(LNX)\n",
			       sc->sc_unit);
		}
		/* Clear the given buffer to indicate we could not get
                   any info. and return.  */
		bzero(data, LNX_EEPROM_SIZE);
		goto RET;
	}

	/* Read bytes from EEPROM.  */
	for (n = 0; n < LNX_EEPROM_SIZE; n++) {

		/* Read a byte and store it into the buffer.  */
		val = 0x00;
		for (bit = 0x80; bit != 0x00; bit >>= 1) {
			if (fe_eeprom_receive_lnx(reg20)) val |= bit;
		}
		*data++ = val;

		/* Acknowledge if we have to read more.  */
		if (n < LNX_EEPROM_SIZE - 1) {
			fe_eeprom_cycle_lnx(reg20, LNX_CYCLE_LO);
		}
	}

	/* Issue a STOP condition, de-activating the clock line.
	   It will be safer to keep the clock line low than to leave
	   it high.  */
	fe_eeprom_cycle_lnx(reg20, LNX_CYCLE_STOP);

    RET:
	outb(sc->ioaddr[0x14], save20);
	
#if 1
	/* Report what we got.  */
	data -= LNX_EEPROM_SIZE;
	if (bootverbose) {
		for (i = 0; i < JLI_EEPROM_SIZE; i += 16) {
			printf("fe%d: EEPROM(LNX):%3x: %16D\n",
			       sc->sc_unit, i, data + i, " ");
		}
	}
#endif
}

static void
fe_init_lnx ( struct fe_softc * sc )
{
	/* Reset the 86960.  Do we need this?  FIXME.  */
	outb(sc->ioaddr[0x12], 0x06);
	DELAY(100);
	outb(sc->ioaddr[0x12], 0x07);
	DELAY(100);

	/* Setup IRQ control register on the ASIC.  */
	outb(sc->ioaddr[0x14], sc->priv_info);
}

/*
 * Ungermann-Bass boards support routine.
 */
static void
fe_init_ubn ( struct fe_softc * sc )
{
 	/* Do we need this?  FIXME.  */
	outb(sc->ioaddr[FE_DLCR7],
		sc->proto_dlcr7 | FE_D7_RBS_BMPR | FE_D7_POWER_UP);
 	outb(sc->ioaddr[0x18], 0x00);
 	DELAY( 200 );
 
	/* Setup IRQ control register on the ASIC.  */
	outb(sc->ioaddr[0x14], sc->priv_info);
}

/*
 * Machine dependent probe routines.
 */

#ifdef PC98
static int
fe_probe_fmv ( struct isa_device * dev, struct fe_softc * sc )
{
	/* PC-98 has no board of this architechture.  */
	return 0;
}

/* ioaddr for RE1000/1000Plus - Very dirty!  */
static u_short ioaddr_re1000[MAXREGISTERS] = {
	0x0000, 0x0001, 0x0200, 0x0201, 0x0400, 0x0401, 0x0600, 0x0601,
	0x0800, 0x0801, 0x0a00, 0x0a01, 0x0c00, 0x0c01, 0x0e00, 0x0e01,
	0x1000, 0x1200, 0x1400, 0x1600, 0x1800, 0x1a00, 0x1c00, 0x1e00,
	0x1001, 0x1201, 0x1401, 0x1601, 0x1801, 0x1a01, 0x1c01, 0x1e01,
};

/*
 * Probe and initialization for Allied-Telesis RE1000 series.
 */
static void
fe_init_re1000 ( struct fe_softc * sc )
{
	/* Setup IRQ control register on the ASIC.  */
	outb(sc->ioaddr[FE_RE1000_IRQCONF], sc->priv_info);
}

static int
fe_probe_re1000 ( struct isa_device * dev, struct fe_softc * sc )
{
	int i, n;
	u_char sum;

	static struct fe_simple_probe_struct probe_table [] = {
		{ FE_DLCR2, 0x58, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
		{ 0 }
	};

	/* See if the specified I/O address is possible for RE1000.  */
	/* [01]D[02468ACE] are allowed.  */ 
	if ((sc->iobase & ~0x10E) != 0xD0) return 0;

	/* Setup an I/O address mapping table and some others.  */
	fe_softc_defaults(sc);

	/* Re-map ioaddr for RE1000.  */
	for (i = 0; i < MAXREGISTERS; i++)
		sc->ioaddr[i] = sc->iobase + ioaddr_re1000[i];

	/* See if the card is on its address.  */
	if (!fe_simple_probe(sc, probe_table)) return 0;

	/* Get our station address from EEPROM.  */
	inblk(sc, 0x18, sc->sc_enaddr, ETHER_ADDR_LEN);

	/* Make sure it is Allied-Telesis's.  */
	if (!valid_Ether_p(sc->sc_enaddr, 0x0000F4)) return 0;
#if 1
	/* Calculate checksum.  */
	sum = inb(sc->ioaddr[0x1e]);
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		sum ^= sc->sc_enaddr[i];
	}
	if (sum != 0) return 0;
#endif
	/* Setup the board type.  */
	sc->typestr = "RE1000";

	/* This looks like an RE1000 board.  It requires an
	   explicit IRQ setting in config.  Make sure we have one,
	   determining an appropriate value for the IRQ control
	   register.  */
	switch (dev->id_irq) {
	  case IRQ3:  n = 0x10; break;
	  case IRQ5:  n = 0x20; break;
	  case IRQ6:  n = 0x40; break;
	  case IRQ12: n = 0x80; break;
	  default:
		fe_irq_failure(sc->typestr,
				sc->sc_unit, dev->id_irq, "3/5/6/12");
		return 0;
	}
	sc->priv_info = inb(sc->ioaddr[FE_RE1000_IRQCONF]) & 0x0f | n;

	/* Setup hooks.  We need a special initialization procedure.  */
	sc->init = fe_init_re1000;

	/* The I/O address range is fragmented in the RE1000.
	   It occupies 2*16 I/O addresses, by the way.  */
	return 2;
}

/* JLI sub-probe for Allied-Telesis RE1000Plus/ME1500 series.  */
static u_short const *
fe_probe_jli_re1000p (struct fe_softc * sc, u_char const * eeprom)
{
	int i;
	static u_short const irqmaps_re1000p [4] = { IRQ3, IRQ5, IRQ6, IRQ12 };

	/* Make sure the EEPROM contains Allied-Telesis bit pattern.  */
	if (eeprom[1] != 0xFF) return NULL;
	for (i =  2; i <  8; i++) if (eeprom[i] != 0xFF) return NULL;
	for (i = 14; i < 24; i++) if (eeprom[i] != 0xFF) return NULL;

	/* Get our station address from EEPROM, and make sure the
           EEPROM contains Allied-Telesis's address.  */
	bcopy(eeprom+8, sc->sc_enaddr, ETHER_ADDR_LEN);
	if (!valid_Ether_p(sc->sc_enaddr, 0x0000F4)) return NULL;

	/* I don't know any sub-model identification.  */
	sc->typestr = "RE1000Plus/ME1500";

	/* Returns the IRQ table for the RE1000Plus.  */
	return irqmaps_re1000p;
}

/*
 * Probe for Allied-Telesis RE1000Plus/ME1500 series.
 */
static int
fe_probe_jli (struct isa_device * dev, struct fe_softc * sc)
{
	int i, n;
	int irq;
	u_char eeprom [JLI_EEPROM_SIZE];
	u_short const * irqmap;

	static u_short const baseaddr [8] =
		{ 0x1D6, 0x1D8, 0x1DA, 0x1D4, 0x0D4, 0x0D2, 0x0D8, 0x0D0 };
	static struct fe_simple_probe_struct const probe_table [] = {
	/*	{ FE_DLCR1,  0x20, 0x00 },	Doesn't work. */
		{ FE_DLCR2,  0x50, 0x00 },
		{ FE_DLCR4,  0x08, 0x00 },
	/*	{ FE_DLCR5,  0x80, 0x00 },	Doesn't work. */
#if 0
		{ FE_BMPR16, 0x1B, 0x00 },
		{ FE_BMPR17, 0x7F, 0x00 },
#endif
		{ 0 }
	};

	/*
	 * See if the specified address is possible for MB86965A JLI mode.
	 */
	for (i = 0; i < 8; i++) {
		if (baseaddr[i] == sc->iobase) break;
	}
	if (i == 8) return 0;

	/* Fill the softc struct with reasonable default.  */
	fe_softc_defaults(sc);

	/* Re-map ioaddr for RE1000Plus.  */
	for (i = 0; i < MAXREGISTERS; i++)
		sc->ioaddr[i] = sc->iobase + ioaddr_re1000[i];

	/*
	 * We should test if MB86965A is on the base address now.
	 * Unfortunately, it is very hard to probe it reliably, since
	 * we have no way to reset the chip under software control.
	 * On cold boot, we could check the "signature" bit patterns
	 * described in the Fujitsu document.  On warm boot, however,
	 * we can predict almost nothing about register values.
	 */
	if (!fe_simple_probe(sc, probe_table)) return 0;

	/* Check if our I/O address matches config info on 86965.  */
	n = (inb(sc->ioaddr[FE_BMPR19]) & FE_B19_ADDR) >> FE_B19_ADDR_SHIFT;
	if (baseaddr[n] != sc->iobase) return 0;

	/*
	 * We are now almost sure we have an MB86965 at the given
	 * address.  So, read EEPROM through it.  We have to write
	 * into LSI registers to read from EEPROM.  I want to avoid it
	 * at this stage, but I cannot test the presence of the chip
	 * any further without reading EEPROM.  FIXME.
	 */
	fe_read_eeprom_jli(sc, eeprom);

	/* Make sure that config info in EEPROM and 86965 agree.  */
	if (eeprom[FE_EEPROM_CONF] != inb(sc->ioaddr[FE_BMPR19])) {
		return 0;
	}

	/* Use 86965 media selection scheme, unless othewise
           specified.  It is "AUTO always" and "select with BMPR13".
           This behaviour covers most of the 86965 based board (as
           minimum requirements.)  It is backward compatible with
           previous versions, also.  */
	sc->mbitmap = MB_HA;
	sc->defmedia = MB_HA;
	sc->msel = fe_msel_965;

	/* Perform board-specific probe.  */
	if ((irqmap = fe_probe_jli_re1000p(sc, eeprom)) == NULL) return 0;

	/* Find the IRQ read from EEPROM.  */
	n = (inb(sc->ioaddr[FE_BMPR19]) & FE_B19_IRQ) >> FE_B19_IRQ_SHIFT;
	irq = irqmap[n];

	/* Try to determine IRQ setting.  */
	if (dev->id_irq == NO_IRQ && irq == NO_IRQ) {
		/* The device must be configured with an explicit IRQ.  */
		printf("fe%d: IRQ auto-detection does not work\n",
		       sc->sc_unit);
		return 0;
	} else if (dev->id_irq == NO_IRQ && irq != NO_IRQ) {
		/* Just use the probed IRQ value.  */
		dev->id_irq = irq;
	} else if (dev->id_irq != NO_IRQ && irq == NO_IRQ) {
		/* No problem.  Go ahead.  */
	} else if (dev->id_irq == irq) {
		/* Good.  Go ahead.  */
	} else {
		/* User must be warned in this case.  */
		sc->stability |= UNSTABLE_IRQ;
	}

	/* Setup a hook, which resets te 86965 when the driver is being
           initialized.  This may solve a nasty bug.  FIXME.  */
	sc->init = fe_init_jli;

	/* The I/O address range is fragmented in the RE1000Plus.
	   It occupies 2*16 I/O addresses, by the way.  */
	return 2;
}

/*
 * Probe and initialization for Contec C-NET(9N)E series.
 */

/* TODO: Should be in "if_fereg.h" */
#define FE_CNET9NE_INTR		0x10		/* Interrupt Mask? */

static void
fe_init_cnet9ne ( struct fe_softc * sc )
{
	/* Enable interrupt?  FIXME.  */
	outb(sc->ioaddr[FE_CNET9NE_INTR], 0x10);
}

static int
fe_probe_cnet9ne ( struct isa_device * dev, struct fe_softc * sc )
{
	int i;

	static struct fe_simple_probe_struct probe_table [] = {
		{ FE_DLCR2, 0x58, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
		{ 0 }
	};
	static u_short ioaddr[MAXREGISTERS - 16] = {
	/*	0x000, 0x001, 0x002, 0x003, 0x004, 0x005, 0x006, 0x007,	*/
	/*	0x008, 0x009, 0x00a, 0x00b, 0x00c, 0x00d, 0x00e, 0x00f,	*/
		0x400, 0x402, 0x404, 0x406, 0x408, 0x40a, 0x40c, 0x40e,
		0x401, 0x403, 0x405, 0x407, 0x409, 0x40b, 0x40d, 0x40f,
	};

	/* See if the specified I/O address is possible for C-NET(9N)E.  */
	if (sc->iobase != 0x73D0) return 0;

	/* Setup an I/O address mapping table and some others.  */
	fe_softc_defaults(sc);

	/* Re-map ioaddr for C-NET(9N)E.  */
	for (i = 16; i < MAXREGISTERS; i++)
		sc->ioaddr[i] = sc->iobase + ioaddr[i - 16];

	/* See if the card is on its address.  */
	if (!fe_simple_probe(sc, probe_table)) return 0;

	/* Get our station address from EEPROM.  */
	inblk(sc, 0x18, sc->sc_enaddr, ETHER_ADDR_LEN);

	/* Make sure it is Contec's.  */
	if (!valid_Ether_p(sc->sc_enaddr, 0x00804C)) return 0;

	/* Setup the board type.  */
	sc->typestr = "C-NET(9N)E";

	/* C-NET(9N)E seems to work only IRQ5.  FIXME.  */
	if (dev->id_irq != IRQ5) {
		fe_irq_failure(sc->typestr, sc->sc_unit, dev->id_irq, "5");
		return 0;
	}

	/* We need an init hook to initialize ASIC before we start.  */
	sc->init = fe_init_cnet9ne;

	/* C-NET(9N)E has 64KB SRAM.  */
	sc->proto_dlcr6 = FE_D6_BUFSIZ_64KB | FE_D6_TXBSIZ_2x4KB
			| FE_D6_BBW_WORD | FE_D6_SBW_WORD | FE_D6_SRAM;

	/* The I/O address range is fragmented in the C-NET(9N)E.
	   This is the number of regs at iobase.  */
	return 16;
}

/*
 * Probe for Contec C-NET(98)P2 series.
 * (Logitec LAN-98TP/LAN-98T25P - parhaps)
 */
static int
fe_probe_ssi (struct isa_device *dev, struct fe_softc *sc)
{
	u_char eeprom [SSI_EEPROM_SIZE];

	static struct fe_simple_probe_struct probe_table [] = {
		{ FE_DLCR2, 0x08, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
		{ 0 }
	};
	static u_short const irqmap[] = {
		/*                      INT0            INT1    INT2        */
		NO_IRQ, NO_IRQ, NO_IRQ, IRQ3  , NO_IRQ, IRQ5  , IRQ6  , NO_IRQ,
		NO_IRQ, IRQ9  , IRQ10 , NO_IRQ, IRQ12 , IRQ13 , NO_IRQ, NO_IRQ,
		/*      INT3    INT41           INT5    INT6                */
	};

	/* See if the specified I/O address is possible for 78Q8377A.  */
	/* [0-D]3D0 are allowed.  */
	if ((sc->iobase & 0xFFF) != 0x3D0) return 0;	/* XXX */

	/* Fill the softc struct with default values.  */
	fe_softc_defaults(sc);

	/* See if the card is on its address.  */
	if (!fe_simple_probe(sc, probe_table)) return 0;

	/* We now have to read the config EEPROM.  We should be very
           careful, since doing so destroys a register.  (Remember, we
           are not yet sure we have a C-NET(98)P2 board here.)  Don't
           remember to select BMPRs bofore reading EEPROM, since other
           register bank may be selected before the probe() is called.  */
	fe_read_eeprom_ssi(sc, eeprom);

	/* Make sure the Ethernet (MAC) station address is of Contec's.  */
	if (!valid_Ether_p(eeprom+FE_SSI_EEP_ADDR, 0x00804C)) return 0;
	bcopy(eeprom+FE_SSI_EEP_ADDR, sc->sc_enaddr, ETHER_ADDR_LEN);

	/* Setup the board type.  */
        sc->typestr = "C-NET(98)P2";

	/* Get IRQ configuration from EEPROM.  */
	dev->id_irq = irqmap[eeprom[FE_SSI_EEP_IRQ]];
	if (dev->id_irq == NO_IRQ) {
		fe_irq_failure(sc->typestr,
				sc->sc_unit, dev->id_irq, "3/5/6/9/10/12/13");
		return 0;
	}

	/* Get Duplex-mode configuration from EEPROM.  */
	sc->proto_dlcr4 |= (eeprom[FE_SSI_EEP_DUPLEX] & FE_D4_DSC);

	/* Fill softc struct accordingly.  */
	sc->mbitmap = MB_HT;
	sc->defmedia = MB_HT;

	/* We have 16 registers.  */
	return 16;
}

/*
 * Probe for TDK LAC-98012/013/025/9N011 - parhaps.
 */
static int
fe_probe_lnx (struct isa_device *dev, struct fe_softc *sc)
{
#ifndef FE_8BIT_SUPPORT
	printf("fe%d: skip LAC-98012/013(only 16-bit cards are supported)\n",
	       sc->sc_unit);
	return 0;
#else
	int i;
	u_char eeprom [LNX_EEPROM_SIZE];

	static struct fe_simple_probe_struct probe_table [] = {
		{ FE_DLCR2, 0x58, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
		{ 0 }
	};

	/* See if the specified I/O address is possible for TDK/LANX boards.  */
	/* 0D0, 4D0, 8D0, and CD0 are allowed.  */
	if ((sc->iobase & ~0xC00) != 0xD0) return 0;

	/* Fill the softc struct with default values.  */
	fe_softc_defaults(sc);

	/* Re-map ioaddr for LAC-98.
	 *	0x000, 0x002, 0x004, 0x006, 0x008, 0x00a, 0x00c, 0x00e,
	 *	0x100, 0x102, 0x104, 0x106, 0x108, 0x10a, 0x10c, 0x10e,
	 *	0x200, 0x202, 0x204, 0x206, 0x208, 0x20a, 0x20c, 0x20e,
	 *	0x300, 0x302, 0x304, 0x306, 0x308, 0x30a, 0x30c, 0x30e,
	 */
	for (i = 0; i < MAXREGISTERS; i++)
		sc->ioaddr[i] = sc->iobase + ((i & 7) << 1) + ((i & 0x18) << 5);

	/* See if the card is on its address.  */
	if (!fe_simple_probe(sc, probe_table)) return 0;

	/* We now have to read the config EEPROM.  We should be very
           careful, since doing so destroys a register.  (Remember, we
           are not yet sure we have a LAC-98012/98013 board here.)  */
	fe_read_eeprom_lnx(sc, eeprom);

	/* Make sure the Ethernet (MAC) station address is of TDK/LANX's.  */
	if (!valid_Ether_p(eeprom, 0x008098)) return 0;
	bcopy(eeprom, sc->sc_enaddr, ETHER_ADDR_LEN);

	/* Setup the board type.  */
	sc->typestr = "LAC-98012/98013";

	/* This looks like a TDK/LANX board.  It requires an
	   explicit IRQ setting in config.  Make sure we have one,
	   determining an appropriate value for the IRQ control
	   register.  */
	switch (dev->id_irq) {
	  case IRQ3 : sc->priv_info = 0x10 | LNX_CLK_LO | LNX_SDA_HI; break;
	  case IRQ5 : sc->priv_info = 0x20 | LNX_CLK_LO | LNX_SDA_HI; break;
	  case IRQ6 : sc->priv_info = 0x40 | LNX_CLK_LO | LNX_SDA_HI; break;
	  case IRQ12: sc->priv_info = 0x80 | LNX_CLK_LO | LNX_SDA_HI; break;
	  default:
		fe_irq_failure(sc->typestr,
				sc->sc_unit, dev->id_irq, "3/5/6/12");
		return 0;
	}

	/* LAC-98's system bus width is 8-bit.  */ 
	sc->proto_dlcr6 = FE_D6_BUFSIZ_32KB | FE_D6_TXBSIZ_2x2KB
			| FE_D6_BBW_BYTE | FE_D6_SBW_BYTE | FE_D6_SRAM_150ns;

	/* Setup hooks.  We need a special initialization procedure.  */
	sc->init = fe_init_lnx;

	/* The I/O address range is fragmented in the LAC-98.
	   It occupies 16*4 I/O addresses, by the way.  */
	return 16;
#endif /* FE_8BIT_SUPPORT */
}

/*
 * Probe for Gateway Communications' old cards.
 * (both as Generic MB86960 probe routine)
 */
static int
fe_probe_gwy ( struct isa_device * dev, struct fe_softc * sc )
{
	static struct fe_simple_probe_struct probe_table [] = {
	    /*	{ FE_DLCR2, 0x70, 0x00 }, */
		{ FE_DLCR2, 0x58, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
		{ 0 }
	};

	/* I'm not sure which address is possible, so accepts any.  FIXME.  */

	/* Setup an I/O address mapping table and some others.  */
	fe_softc_defaults(sc);

	/* Does we need to re-map ioaddr?  FIXME.  */

	/* See if the card is on its address.  */
	if ( !fe_simple_probe( sc, probe_table ) ) return 0;

	/* Get our station address from EEPROM. */
	inblk( sc, 0x18, sc->sc_enaddr, ETHER_ADDR_LEN );
	if (!valid_Ether_p(sc->sc_enaddr, 0x000000)) return 0;

	/* Determine the card type.  */
	sc->typestr = "Generic MB86960 Ethernet";
	if (valid_Ether_p(sc->sc_enaddr, 0x000061))
		sc->typestr = "Gateway Ethernet (Fujitsu chipset)";

	/* Gateway's board requires an explicit IRQ to work, since it
	   is not possible to probe the setting of jumpers.  */
	if (dev->id_irq == NO_IRQ) {
		fe_irq_failure(sc->typestr, sc->sc_unit, NO_IRQ, NULL);
		return 0;
	}

	/* We should change return value when re-mapping ioaddr.  FIXME. */
	return 32;
}

/*
 * Probe for Ungermann-Bass Access/PC N98C+(Model 85152).
 */
static int
fe_probe_ubn (struct isa_device * dev, struct fe_softc * sc)
{
	u_char sum;
	int i;
	static struct fe_simple_probe_struct const probe_table [] = {
		{ FE_DLCR2, 0x58, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
		{ 0 }
	};

	/* See if the specified I/O address is possible for Access/PC.  */
	/* [01][048C]D0 are allowed.  */ 
	if ((sc->iobase & ~0x1C00) != 0xD0) return 0;

	/* Setup an I/O address mapping table and some others.  */
	fe_softc_defaults(sc);

	/* Re-map ioaddr for Access/PC N98C+.
	 *	0x000, 0x001, 0x002, 0x003, 0x004, 0x005, 0x006, 0x007,
	 *	0x008, 0x009, 0x00a, 0x00b, 0x00c, 0x00d, 0x00e, 0x00f,
	 *	0x200, 0x201, 0x202, 0x203, 0x204, 0x205, 0x206, 0x207,
	 *	0x208, 0x209, 0x20a, 0x20b, 0x20c, 0x20d, 0x20e, 0x20f,
	 */
	for (i = 16; i < MAXREGISTERS; i++)
		sc->ioaddr[i] = sc->iobase + 0x200 - 16 + i;

	/* Simple probe.  */
	if (!fe_simple_probe(sc, probe_table)) return 0;

	/* Get our station address form ID ROM and make sure it is UBN's.  */
	inblk(sc, 0x18, sc->sc_enaddr, ETHER_ADDR_LEN);
	if (!valid_Ether_p(sc->sc_enaddr, 0x00DD01)) return 0;
#if 1
	/* Calculate checksum.  */
	sum = inb(sc->ioaddr[0x1e]);
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		sum ^= sc->sc_enaddr[i];
	}
	if (sum != 0) return 0;
#endif
	/* Setup the board type.  */
	sc->typestr = "Access/PC";

	/* This looks like an AccessPC/N98C+ board.  It requires an
	   explicit IRQ setting in config.  Make sure we have one,
	   determining an appropriate value for the IRQ control
	   register.  */
	switch (dev->id_irq) {
	  case IRQ3:  sc->priv_info = 0x01; break;
	  case IRQ5:  sc->priv_info = 0x02; break;
	  case IRQ6:  sc->priv_info = 0x04; break;
	  case IRQ12: sc->priv_info = 0x08; break;
	  default:
		fe_irq_failure(sc->typestr,
				sc->sc_unit, dev->id_irq, "3/5/6/12");
		return 0;
	}

	/* Setup hooks.  We need a special initialization procedure.  */
	sc->init = fe_init_ubn;

	/* The I/O address range is fragmented in the Access/PC N98C+.
	   This is the number of regs at iobase.  */
	return 16;
}

#else	/* !PC98 */
/*
 * Probe and initialization for Fujitsu FMV-180 series boards
 */

static void
fe_init_fmv (struct fe_softc *sc)
{
	/* Initialize ASIC.  */
	outb( sc->ioaddr[ FE_FMV3 ], 0 );
	outb( sc->ioaddr[ FE_FMV10 ], 0 );

#if 0
	/* "Refresh" hardware configuration.  FIXME.  */
	outb( sc->ioaddr[ FE_FMV2 ], inb( sc->ioaddr[ FE_FMV2 ] ) );
#endif

	/* Turn the "master interrupt control" flag of ASIC on.  */
	outb( sc->ioaddr[ FE_FMV3 ], FE_FMV3_IRQENB );
}

static void
fe_msel_fmv184 (struct fe_softc *sc)
{
	u_char port;

	/* FMV-184 has a special "register" to switch between AUI/BNC.
	   Determine the value to write into the register, based on the
	   user-specified media selection.  */
	port = (IFM_SUBTYPE(sc->media.ifm_media) == IFM_10_2) ? 0x00 : 0x01;

	/* The register is #5 on exntesion register bank...
	   (Details of the register layout is not yet discovered.)  */
	outb(sc->ioaddr[0x1B], 0x46);	/* ??? */
	outb(sc->ioaddr[0x1E], 0x04);	/* select ex-reg #4.  */
	outb(sc->ioaddr[0x1F], 0xC8);	/* ??? */
	outb(sc->ioaddr[0x1E], 0x05);	/* select ex-reg #5.  */
	outb(sc->ioaddr[0x1F], port);	/* Switch the media.  */
	outb(sc->ioaddr[0x1E], 0x04);	/* select ex-reg #4.  */
	outb(sc->ioaddr[0x1F], 0x00);	/* ??? */
	outb(sc->ioaddr[0x1B], 0x00);	/* ??? */

	/* Make sure to select "external tranceiver" on MB86964.  */
	outb(sc->ioaddr[FE_BMPR13], sc->proto_bmpr13 | FE_B13_PORT_AUI);
}

static int
fe_probe_fmv ( struct isa_device * dev, struct fe_softc * sc )
{
	int n;

	static u_short const irqmap [ 4 ] =
		{ IRQ3,  IRQ7,  IRQ10, IRQ15 };

	static struct fe_simple_probe_struct const probe_table [] = {
		{ FE_DLCR2, 0x71, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },

		{ FE_FMV0, 0x78, 0x50 },	/* ERRDY+PRRDY */
		{ FE_FMV1, 0xB0, 0x00 },	/* FMV-183/4 has 0x48 bits. */
		{ FE_FMV3, 0x7F, 0x00 },

		{ 0 }
	};

	/* Board subtypes; it lists known FMV-180 variants.  */
	struct subtype {
		u_short mcode;
		u_short mbitmap;
		u_short defmedia;
		char const * str;
	};
	static struct subtype const typelist [] = {
	    { 0x0005, MB_HA|MB_HT|MB_H5, MB_HA, "FMV-181"		},
	    { 0x0105, MB_HA|MB_HT|MB_H5, MB_HA, "FMV-181A"		},
	    { 0x0003, MB_HM,             MB_HM, "FMV-182"		},
	    { 0x0103, MB_HM,             MB_HM, "FMV-182A"		},
	    { 0x0804, MB_HT,             MB_HT, "FMV-183"		},
	    { 0x0C04, MB_HT,             MB_HT, "FMV-183 (on-board)"	},
	    { 0x0803, MB_H2|MB_H5,       MB_H2, "FMV-184"		},
	    { 0,      MB_HA,             MB_HA, "unknown FMV-180 (?)"	},
	};
	struct subtype const * type;

	/* Media indicator and "Hardware revision ID"  */
	u_short mcode;

	/* See if the specified address is possible for FMV-180
           series.  220, 240, 260, 280, 2A0, 2C0, 300, and 340 are
           allowed for all boards, and 200, 2E0, 320, 360, 380, 3A0,
           3C0, and 3E0 for PnP boards.  */
	if ((sc->iobase & ~0x1E0) != 0x200) return 0;

	/* Setup an I/O address mapping table and some others.  */
	fe_softc_defaults(sc);

	/* Simple probe.  */
	if (!fe_simple_probe(sc, probe_table)) return 0;

	/* Get our station address from EEPROM, and make sure it is
           Fujitsu's.  */
	inblk(sc, FE_FMV4, sc->sc_enaddr, ETHER_ADDR_LEN);
	if (!valid_Ether_p(sc->sc_enaddr, 0x00000E)) return 0;

	/* Find the supported media and "hardware revision" to know
           the model identification.  */
	mcode = (inb(sc->ioaddr[FE_FMV0]) & FE_FMV0_MEDIA)
	     | ((inb(sc->ioaddr[FE_FMV1]) & FE_FMV1_REV) << 8);

	/* Determine the card type.  */
	for (type = typelist; type->mcode != 0; type++) {
		if (type->mcode == mcode) break;
	}
	if (type->mcode == 0) {
	  	/* Unknown card type...  Hope the driver works.  */
		sc->stability |= UNSTABLE_TYPE;
		if (bootverbose) {
			printf("fe%d: unknown config: %x-%x-%x-%x\n",
			       sc->sc_unit,
			       inb(sc->ioaddr[FE_FMV0]),
			       inb(sc->ioaddr[FE_FMV1]),
			       inb(sc->ioaddr[FE_FMV2]),
			       inb(sc->ioaddr[FE_FMV3]));
		}
	}

	/* Setup the board type and media information.  */
	sc->typestr = type->str;
	sc->mbitmap = type->mbitmap;
	sc->defmedia = type->defmedia;
	sc->msel = fe_msel_965;

	if (type->mbitmap == (MB_H2 | MB_H5)) {
		/* FMV184 requires a special media selection procedure.  */
		sc->msel = fe_msel_fmv184;
	}

	/*
	 * An FMV-180 has been probed.
	 * Determine which IRQ to be used.
	 *
	 * In this version, we give a priority to the kernel config file.
	 * If the EEPROM and config don't match, say it to the user for
	 * an attention.
	 */
	n = ( inb( sc->ioaddr[ FE_FMV2 ] ) & FE_FMV2_IRS )
		>> FE_FMV2_IRS_SHIFT;
	if ( dev->id_irq == NO_IRQ ) {
		/* Just use the probed value.  */
		dev->id_irq = irqmap[ n ];
	} else if ( dev->id_irq != irqmap[ n ] ) {
		/* Don't match.  */
		sc->stability |= UNSTABLE_IRQ;
	}

	/* We need an init hook to initialize ASIC before we start.  */
	sc->init = fe_init_fmv;

	/*
	 * That's all.  FMV-180 occupies 32 I/O addresses, by the way.
	 */
	return 32;
}

/*
 * Fujitsu MB86965 JLI mode probe routines.
 *
 * 86965 has a special operating mode called JLI (mode 0), under which
 * the chip interfaces with ISA bus with a software-programmable
 * configuration.  (The Fujitsu document calls the feature "Plug and
 * play," but it is not compatible with the ISA-PnP spec. designed by
 * Intel and Microsoft.)  Ethernet cards designed to use JLI are
 * almost same, but there are two things which require board-specific
 * probe routines: EEPROM layout and IRQ pin connection.
 *
 * JLI provides a handy way to access EEPROM which should contains the
 * chip configuration information (such as I/O port address) as well
 * as Ethernet station (MAC) address.  The chip configuration info. is
 * stored on a fixed location.  However, the station address can be
 * located anywhere in the EEPROM; it is up to the board designer to
 * determine the location.  (The manual just says "somewhere in the
 * EEPROM.")  The fe driver must somehow find out the correct
 * location.
 *
 * Another problem resides in the IRQ pin connection.  JLI provides a
 * user to choose an IRQ from up to four predefined IRQs.  The 86965
 * chip has a register to select one out of the four possibilities.
 * However, the selection is against the four IRQ pins on the chip.
 * (So-called IRQ-A, -B, -C and -D.)  It is (again) up to the board
 * designer to determine which pin to connect which IRQ line on the
 * ISA bus.  We need a vendor (or model, for some vendor) specific IRQ
 * mapping table.
 * 
 * The routine fe_probe_jli() provides all probe and initialization
 * processes which are common to all JLI implementation, and sub-probe
 * routines supply board-specific actions.
 *
 * JLI sub-probe routine has the following template:
 *
 *	u_short const * func (struct fe_softc * sc, u_char const * eeprom);
 *
 * where eeprom is a pointer to an array of 32 byte data read from the
 * config EEPROM on the board.  It retuns an IRQ mapping table for the
 * board, when the corresponding implementation is detected.  It
 * returns a NULL otherwise.
 * 
 * Primary purpose of the functin is to analize the config EEPROM,
 * determine if it matches with the pattern of that of supported card,
 * and extract necessary information from it.  One of the information
 * expected to be extracted from EEPROM is the Ethernet station (MAC)
 * address, which must be set to the softc table of the interface by
 * the board-specific routine.
 */

/* JLI sub-probe for Allied-Telesyn/Allied-Telesis AT1700/RE2000 series.  */
static u_short const *
fe_probe_jli_ati (struct fe_softc * sc, u_char const * eeprom)
{
	int i;
	static u_short const irqmaps_ati [4][4] =
	{
		{ IRQ3,  IRQ4,  IRQ5,  IRQ9  },
		{ IRQ10, IRQ11, IRQ12, IRQ15 },
		{ IRQ3,  IRQ11, IRQ5,  IRQ15 },
		{ IRQ10, IRQ11, IRQ14, IRQ15 },
	};

	/* Make sure the EEPROM contains Allied-Telesis/Allied-Telesyn
	   bit pattern.  */
	if (eeprom[1] != 0x00) return NULL;
	for (i =  2; i <  8; i++) if (eeprom[i] != 0xFF) return NULL;
	for (i = 14; i < 24; i++) if (eeprom[i] != 0xFF) return NULL;

	/* Get our station address from EEPROM, and make sure the
           EEPROM contains ATI's address.  */
	bcopy(eeprom+8, sc->sc_enaddr, ETHER_ADDR_LEN);
	if (!valid_Ether_p(sc->sc_enaddr, 0x0000F4)) return NULL;

	/*
	 * The following model identification codes are stolen
	 * from the NetBSD port of the fe driver.  My reviewers
	 * suggested minor revision.
	 */

	/* Determine the card type.  */
	switch (eeprom[FE_ATI_EEP_MODEL]) {
	  case FE_ATI_MODEL_AT1700T:
		sc->typestr = "AT-1700T/RE2001";
		sc->mbitmap = MB_HT;
		sc->defmedia = MB_HT;
		break;
	  case FE_ATI_MODEL_AT1700BT:
		sc->typestr = "AT-1700BT/RE2003";
		sc->mbitmap = MB_HA | MB_HT | MB_H2;
		break;
	  case FE_ATI_MODEL_AT1700FT:
		sc->typestr = "AT-1700FT/RE2009";
		sc->mbitmap = MB_HA | MB_HT | MB_HF;
		break;
	  case FE_ATI_MODEL_AT1700AT:
		sc->typestr = "AT-1700AT/RE2005";
		sc->mbitmap = MB_HA | MB_HT | MB_H5;
		break;
	  default:
		sc->typestr = "unknown AT-1700/RE2000";
		sc->stability |= UNSTABLE_TYPE | UNSTABLE_IRQ;
		break;
	}

#if 0
	/* Should we extract default media from eeprom?  Linux driver
	   for AT1700 does it, although previous releases of FreeBSD
	   don't.  FIXME.  */
	/* Determine the default media selection from the config
           EEPROM.  The byte at offset EEP_MEDIA is believed to
           contain BMPR13 value to be set.  We just ignore STP bit or
           squelch bit, since we don't support those.  (It is
           intentional.)  */
	switch (eeprom[FE_ATI_EEP_MEDIA] & FE_B13_PORT) {
	    case FE_B13_AUTO:
		sc->defmedia = MB_HA;
		break;
	    case FE_B13_TP:
		sc->defmedia = MB_HT;
		break;
	    case FE_B13_AUI:
		sc->defmedia = sc->mbitmap & (MB_H2|MB_H5|MB_H5); /*XXX*/
		break;
	    default:	    
		sc->defmedia = MB_HA;
		break;
	}

	/* Make sure the default media is compatible with the supported
	   ones.  */
	if ((sc->defmedia & sc->mbitmap) == 0) {
		if (sc->defmedia == MB_HA) {
			sc->defmedia = MB_HT;
		} else {
			sc->defmedia = MB_HA;
		}
	}
#endif	

	/*
	 * Try to determine IRQ settings.
	 * Different models use different ranges of IRQs.
	 */
	switch ((eeprom[FE_ATI_EEP_REVISION] & 0xf0)
	       |(eeprom[FE_ATI_EEP_MAGIC]    & 0x04)) {
	    case 0x30: case 0x34: return irqmaps_ati[3];
	    case 0x10: case 0x14:
	    case 0x50: case 0x54: return irqmaps_ati[2];
	    case 0x44: case 0x64: return irqmaps_ati[1];
	    default:		  return irqmaps_ati[0];
	}
}

/* JLI sub-probe and msel hook for ICL Ethernet.  */

static void
fe_msel_icl (struct fe_softc *sc)
{
	u_char d4;

	/* Switch between UTP and "external tranceiver" as always.  */    
	fe_msel_965(sc);

	/* The board needs one more bit (on DLCR4) be set appropriately.  */
	if (IFM_SUBTYPE(sc->media.ifm_media) == IFM_10_5) {
		d4 = sc->proto_dlcr4 | FE_D4_CNTRL;
	} else {
		d4 = sc->proto_dlcr4 & ~FE_D4_CNTRL;
	}
	outb(sc->ioaddr[FE_DLCR4], d4);
}

static u_short const *
fe_probe_jli_icl (struct fe_softc * sc, u_char const * eeprom)
{
	int i;
	u_short defmedia;
	u_char d6;
	static u_short const irqmap_icl [4] = { IRQ9, IRQ10, IRQ5, IRQ15 };

	/* Make sure the EEPROM contains ICL bit pattern.  */
	for (i = 24; i < 39; i++) {
	    if (eeprom[i] != 0x20 && (eeprom[i] & 0xF0) != 0x30) return NULL;
	}
	for (i = 112; i < 122; i++) {
	    if (eeprom[i] != 0x20 && (eeprom[i] & 0xF0) != 0x30) return NULL;
	}

	/* Make sure the EEPROM contains ICL's permanent station
           address.  If it isn't, probably this board is not an
           ICL's.  */
	if (!valid_Ether_p(eeprom+122, 0x00004B)) return NULL;

	/* Check if the "configured" Ethernet address in the EEPROM is
	   valid.  Use it if it is, or use the "permanent" address instead.  */
	if (valid_Ether_p(eeprom+4, 0x020000)) {
		/* The configured address is valid.  Use it.  */
		bcopy(eeprom+4, sc->sc_enaddr, ETHER_ADDR_LEN);
	} else {
		/* The configured address is invalid.  Use permanent.  */
		bcopy(eeprom+122, sc->sc_enaddr, ETHER_ADDR_LEN);
	}

	/* Determine model and supported media.  */
	switch (eeprom[0x5E]) {
	    case 0:
	        sc->typestr = "EtherTeam16i/COMBO";
	        sc->mbitmap = MB_HA | MB_HT | MB_H5 | MB_H2;
		break;
	    case 1:
		sc->typestr = "EtherTeam16i/TP";
	        sc->mbitmap = MB_HT;
		break;
	    case 2:
		sc->typestr = "EtherTeam16i/ErgoPro";
		sc->mbitmap = MB_HA | MB_HT | MB_H5;
		break;
	    case 4:
		sc->typestr = "EtherTeam16i/DUO";
		sc->mbitmap = MB_HA | MB_HT | MB_H2;
		break;
	    default:
		sc->typestr = "EtherTeam16i";
		sc->stability |= UNSTABLE_TYPE;
		if (bootverbose) {
		    printf("fe%d: unknown model code %02x for EtherTeam16i\n",
			   sc->sc_unit, eeprom[0x5E]);
		}
		break;
	}

	/* I'm not sure the following msel hook is required by all
           models or COMBO only...  FIXME.  */
	sc->msel = fe_msel_icl;

	/* Make the configured media selection the default media.  */
	switch (eeprom[0x28]) {
	    case 0: defmedia = MB_HA; break;
	    case 1: defmedia = MB_H5; break;
	    case 2: defmedia = MB_HT; break;
	    case 3: defmedia = MB_H2; break;
	    default: 
		if (bootverbose) {
			printf("fe%d: unknown default media: %02x\n",
			       sc->sc_unit, eeprom[0x28]);
		}
		defmedia = MB_HA;
		break;
	}

	/* Make sure the default media is compatible with the
	   supported media.  */
	if ((defmedia & sc->mbitmap) == 0) {
		if (bootverbose) {
			printf("fe%d: default media adjusted\n", sc->sc_unit);
		}
		defmedia = sc->mbitmap;
	}

	/* Keep the determined default media.  */
	sc->defmedia = defmedia;

	/* ICL has "fat" models.  We have to program 86965 to properly
	   reflect the hardware.  */
	d6 = sc->proto_dlcr6 & ~(FE_D6_BUFSIZ | FE_D6_BBW);
	switch ((eeprom[0x61] << 8) | eeprom[0x60]) {
	    case 0x2008: d6 |= FE_D6_BUFSIZ_32KB | FE_D6_BBW_BYTE; break;
	    case 0x4010: d6 |= FE_D6_BUFSIZ_64KB | FE_D6_BBW_WORD; break;
	    default:
		/* We can't support it, since we don't know which bits
		   to set in DLCR6.  */
		printf("fe%d: unknown SRAM config for ICL\n", sc->sc_unit);
		return NULL;
	}
	sc->proto_dlcr6 = d6;

	/* Returns the IRQ table for the ICL board.  */
	return irqmap_icl;
}

/* JLI sub-probe for RATOC REX-5586/5587.  */
static u_short const *
fe_probe_jli_rex (struct fe_softc * sc, u_char const * eeprom)
{
	int i;
	static u_short const irqmap_rex [4] = { IRQ3, IRQ4, IRQ5, NO_IRQ };

	/* Make sure the EEPROM contains RATOC's config pattern.  */
	if (eeprom[1] != eeprom[0]) return NULL;
	for (i = 8; i < 32; i++) if (eeprom[i] != 0xFF) return NULL;

	/* Get our station address from EEPROM.  Note that RATOC
	   stores it "byte-swapped" in each word.  (I don't know why.)
	   So, we just can't use bcopy().*/
	sc->sc_enaddr[0] = eeprom[3];
	sc->sc_enaddr[1] = eeprom[2];
	sc->sc_enaddr[2] = eeprom[5];
	sc->sc_enaddr[3] = eeprom[4];
	sc->sc_enaddr[4] = eeprom[7];
	sc->sc_enaddr[5] = eeprom[6];

	/* Make sure the EEPROM contains RATOC's station address.  */
	if (!valid_Ether_p(sc->sc_enaddr, 0x00C0D0)) return NULL;

	/* I don't know any sub-model identification.  */
	sc->typestr = "REX-5586/5587";

	/* Returns the IRQ for the RATOC board.  */
	return irqmap_rex;
}

/* JLI sub-probe for Unknown board.  */
static u_short const *
fe_probe_jli_unk (struct fe_softc * sc, u_char const * eeprom)
{
	int i, n, romsize;
	static u_short const irqmap [4] = { NO_IRQ, NO_IRQ, NO_IRQ, NO_IRQ };

	/* The generic JLI probe considered this board has an 86965
	   in JLI mode, but any other board-specific routines could
	   not find the matching implementation.  So, we "guess" the
	   location by looking for a bit pattern which looks like a
	   MAC address.  */

	/* Determine how large the EEPROM is.  */
	for (romsize = JLI_EEPROM_SIZE/2; romsize > 16; romsize >>= 1) {
		for (i = 0; i < romsize; i++) {
			if (eeprom[i] != eeprom[i+romsize]) break;
		}
		if (i < romsize) break;
	}
	romsize <<= 1;

	/* Look for a bit pattern which looks like a MAC address.  */
	for (n = 2; n <= romsize - ETHER_ADDR_LEN; n += 2) {
		if (!valid_Ether_p(eeprom + n, 0x000000)) continue;
	}

	/* If no reasonable address was found, we can't go further.  */
	if (n > romsize - ETHER_ADDR_LEN) return NULL;

	/* Extract our (guessed) station address.  */
	bcopy(eeprom+n, sc->sc_enaddr, ETHER_ADDR_LEN);

	/* We are not sure what type of board it is... */
	sc->typestr = "(unknown JLI)";
	sc->stability |= UNSTABLE_TYPE | UNSTABLE_MAC;

	/* Returns the totally unknown IRQ mapping table.  */
	return irqmap;
}

/*
 * Probe and initialization for all JLI implementations.
 */

static int
fe_probe_jli (struct isa_device * dev, struct fe_softc * sc)
{
	int i, n;
	int irq;
	u_char eeprom [JLI_EEPROM_SIZE];
	u_short const * irqmap;

	static u_short const baseaddr [8] =
		{ 0x260, 0x280, 0x2A0, 0x240, 0x340, 0x320, 0x380, 0x300 };
	static struct fe_simple_probe_struct const probe_table [] = {
		{ FE_DLCR1,  0x20, 0x00 },
		{ FE_DLCR2,  0x50, 0x00 },
		{ FE_DLCR4,  0x08, 0x00 },
		{ FE_DLCR5,  0x80, 0x00 },
#if 0
		{ FE_BMPR16, 0x1B, 0x00 },
		{ FE_BMPR17, 0x7F, 0x00 },
#endif
		{ 0 }
	};

	/*
	 * See if the specified address is possible for MB86965A JLI mode.
	 */
	for (i = 0; i < 8; i++) {
		if (baseaddr[i] == sc->iobase) break;
	}
	if (i == 8) return 0;

	/* Fill the softc struct with reasonable default.  */
	fe_softc_defaults(sc);

	/*
	 * We should test if MB86965A is on the base address now.
	 * Unfortunately, it is very hard to probe it reliably, since
	 * we have no way to reset the chip under software control.
	 * On cold boot, we could check the "signature" bit patterns
	 * described in the Fujitsu document.  On warm boot, however,
	 * we can predict almost nothing about register values.
	 */
	if (!fe_simple_probe(sc, probe_table)) return 0;

	/* Check if our I/O address matches config info on 86965.  */
	n = (inb(sc->ioaddr[FE_BMPR19]) & FE_B19_ADDR) >> FE_B19_ADDR_SHIFT;
	if (baseaddr[n] != sc->iobase) return 0;

	/*
	 * We are now almost sure we have an MB86965 at the given
	 * address.  So, read EEPROM through it.  We have to write
	 * into LSI registers to read from EEPROM.  I want to avoid it
	 * at this stage, but I cannot test the presence of the chip
	 * any further without reading EEPROM.  FIXME.
	 */
	fe_read_eeprom_jli(sc, eeprom);

	/* Make sure that config info in EEPROM and 86965 agree.  */
	if (eeprom[FE_EEPROM_CONF] != inb(sc->ioaddr[FE_BMPR19])) {
		return 0;
	}

	/* Use 86965 media selection scheme, unless othewise
           specified.  It is "AUTO always" and "select with BMPR13."
           This behaviour covers most of the 86965 based board (as
           minimum requirements.)  It is backward compatible with
           previous versions, also.  */
	sc->mbitmap = MB_HA;
	sc->defmedia = MB_HA;
	sc->msel = fe_msel_965;

	/* Perform board-specific probe, one by one.  Note that the
           order of probe is important and should not be changed
           arbitrarily.  */
	if ((irqmap = fe_probe_jli_ati(sc, eeprom)) == NULL
	 && (irqmap = fe_probe_jli_rex(sc, eeprom)) == NULL
	 && (irqmap = fe_probe_jli_icl(sc, eeprom)) == NULL
	 && (irqmap = fe_probe_jli_unk(sc, eeprom)) == NULL) return 0;

	/* Find the IRQ read from EEPROM.  */
	n = (inb(sc->ioaddr[FE_BMPR19]) & FE_B19_IRQ) >> FE_B19_IRQ_SHIFT;
	irq = irqmap[n];

	/* Try to determine IRQ setting.  */
	if (dev->id_irq == NO_IRQ && irq == NO_IRQ) {
		/* The device must be configured with an explicit IRQ.  */
		printf("fe%d: IRQ auto-detection does not work\n",
		       sc->sc_unit);
		return 0;
	} else if (dev->id_irq == NO_IRQ && irq != NO_IRQ) {
		/* Just use the probed IRQ value.  */
		dev->id_irq = irq;
	} else if (dev->id_irq != NO_IRQ && irq == NO_IRQ) {
		/* No problem.  Go ahead.  */
	} else if (dev->id_irq == irq) {
		/* Good.  Go ahead.  */
	} else {
		/* User must be warned in this case.  */
		sc->stability |= UNSTABLE_IRQ;
	}

	/* Setup a hook, which resets te 86965 when the driver is being
           initialized.  This may solve a nasty bug.  FIXME.  */
	sc->init = fe_init_jli;

	/*
	 * That's all.  86965 JLI occupies 32 I/O addresses, by the way.
	 */
	return 32;
}

/* Probe for TDK LAK-AX031, which is an SSi 78Q8377A based board.  */

static int
fe_probe_ssi (struct isa_device *dev, struct fe_softc *sc)
{
	u_char eeprom [SSI_EEPROM_SIZE];

	static struct fe_simple_probe_struct probe_table [] = {
		{ FE_DLCR2, 0x08, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
		{ 0 }
	};

	/* See if the specified I/O address is possible for 78Q8377A.  */
	if ((sc->iobase & ~0x3F0) != 0x000) return 0;

	/* Fill the softc struct with default values.  */
	fe_softc_defaults(sc);

	/* See if the card is on its address.  */
	if (!fe_simple_probe(sc, probe_table)) return 0;

	/* We now have to read the config EEPROM.  We should be very
           careful, since doing so destroys a register.  (Remember, we
           are not yet sure we have a LAK-AX031 board here.)  Don't
           remember to select BMPRs bofore reading EEPROM, since other
           register bank may be selected before the probe() is called.  */
	fe_read_eeprom_ssi(sc, eeprom);

	/* Make sure the Ethernet (MAC) station address is of TDK's.  */
	if (!valid_Ether_p(eeprom+FE_SSI_EEP_ADDR, 0x008098)) return 0;
	bcopy(eeprom+FE_SSI_EEP_ADDR, sc->sc_enaddr, ETHER_ADDR_LEN);

	/* This looks like a TDK-AX031 board.  It requires an explicit
	   IRQ setting in config, since we currently don't know how we
	   can find the IRQ value assigned by ISA PnP manager.  */
	if (dev->id_irq == NO_IRQ) {
		fe_irq_failure("LAK-AX031", sc->sc_unit, dev->id_irq, NULL);
		return 0;
	}

	/* Fill softc struct accordingly.  */
	sc->typestr = "LAK-AX031";
	sc->mbitmap = MB_HT;
	sc->defmedia = MB_HT;

	/* We have 16 registers.  */
	return 16;
}

/*
 * Probe and initialization for TDK/LANX LAC-AX012/013 boards.
 */
static int
fe_probe_lnx (struct isa_device *dev, struct fe_softc *sc)
{
	u_char eeprom [LNX_EEPROM_SIZE];

	static struct fe_simple_probe_struct probe_table [] = {
		{ FE_DLCR2, 0x58, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
		{ 0 }
	};

	/* See if the specified I/O address is possible for TDK/LANX boards.  */
	/* 300, 320, 340, and 360 are allowed.  */
	if ((sc->iobase & ~0x060) != 0x300) return 0;

	/* Fill the softc struct with default values.  */
	fe_softc_defaults(sc);

	/* See if the card is on its address.  */
	if (!fe_simple_probe(sc, probe_table)) return 0;

	/* We now have to read the config EEPROM.  We should be very
           careful, since doing so destroys a register.  (Remember, we
           are not yet sure we have a LAC-AX012/AX013 board here.)  */
	fe_read_eeprom_lnx(sc, eeprom);

	/* Make sure the Ethernet (MAC) station address is of TDK/LANX's.  */
	if (!valid_Ether_p(eeprom, 0x008098)) return 0;
	bcopy(eeprom, sc->sc_enaddr, ETHER_ADDR_LEN);

	/* This looks like a TDK/LANX board.  It requires an
	   explicit IRQ setting in config.  Make sure we have one,
	   determining an appropriate value for the IRQ control
	   register.  */
	switch (dev->id_irq) {
	  case IRQ3: sc->priv_info = 0x40 | LNX_CLK_LO | LNX_SDA_HI; break;
	  case IRQ4: sc->priv_info = 0x20 | LNX_CLK_LO | LNX_SDA_HI; break;
	  case IRQ5: sc->priv_info = 0x10 | LNX_CLK_LO | LNX_SDA_HI; break;
	  case IRQ9: sc->priv_info = 0x80 | LNX_CLK_LO | LNX_SDA_HI; break;
	  default:
		fe_irq_failure("LAC-AX012/AX013",
			       sc->sc_unit, dev->id_irq, "3/4/5/9");
		return 0;
	}

	/* Fill softc struct accordingly.  */
	sc->typestr = "LAC-AX012/AX013";
	sc->init = fe_init_lnx;

	/* We have 32 registers.  */
	return 32;
}

/*
 * Probe and initialization for Gateway Communications' old cards.
 */
static int
fe_probe_gwy ( struct isa_device * dev, struct fe_softc * sc )
{
	static struct fe_simple_probe_struct probe_table [] = {
	    /*	{ FE_DLCR2, 0x70, 0x00 }, */
		{ FE_DLCR2, 0x58, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
		{ 0 }
	};

	/* See if the specified I/O address is possible for Gateway boards.  */
	if ((sc->iobase & ~0x1E0) != 0x200) return 0;

	/* Setup an I/O address mapping table and some others.  */
	fe_softc_defaults(sc);

	/* See if the card is on its address.  */
	if ( !fe_simple_probe( sc, probe_table ) ) return 0;

	/* Get our station address from EEPROM. */
	inblk( sc, 0x18, sc->sc_enaddr, ETHER_ADDR_LEN );

	/* Make sure it is Gateway Communication's.  */
	if (!valid_Ether_p(sc->sc_enaddr, 0x000061)) return 0;

	/* Gateway's board requires an explicit IRQ to work, since it
	   is not possible to probe the setting of jumpers.  */
	if (dev->id_irq == NO_IRQ) {
		fe_irq_failure("Gateway Ethernet", sc->sc_unit, NO_IRQ, NULL);
		return 0;
	}

	/* Fill softc struct accordingly.  */
	sc->typestr = "Gateway Ethernet (Fujitsu chipset)";

	/* That's all.  The card occupies 32 I/O addresses, as always.  */
	return 32;
}

/* Probe and initialization for Ungermann-Bass Network
   K.K. "Access/PC" boards.  */
static int
fe_probe_ubn (struct isa_device * dev, struct fe_softc * sc)
{
#if 0
	u_char sum;
#endif
	static struct fe_simple_probe_struct const probe_table [] = {
		{ FE_DLCR2, 0x58, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
		{ 0 }
	};

	/* See if the specified I/O address is possible for AccessPC/ISA.  */
	if ((sc->iobase & ~0x0E0) != 0x300) return 0;

	/* Setup an I/O address mapping table and some others.  */
	fe_softc_defaults(sc);

	/* Simple probe.  */
	if (!fe_simple_probe(sc, probe_table)) return 0;

	/* Get our station address form ID ROM and make sure it is UBN's.  */
	inblk(sc, 0x18, sc->sc_enaddr, ETHER_ADDR_LEN);
	if (!valid_Ether_p(sc->sc_enaddr, 0x00DD01)) return 0;
#if 0
	/* Calculate checksum.  */
	sum = inb(sc->ioaddr[0x1e]);
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		sum ^= sc->sc_enaddr[i];
	}
	if (sum != 0) return 0;
#endif
	/* This looks like an AccessPC/ISA board.  It requires an
	   explicit IRQ setting in config.  Make sure we have one,
	   determining an appropriate value for the IRQ control
	   register.  */
	switch (dev->id_irq) {
	  case IRQ3:  sc->priv_info = 0x02; break;
	  case IRQ4:  sc->priv_info = 0x04; break;
	  case IRQ5:  sc->priv_info = 0x08; break;
	  case IRQ10: sc->priv_info = 0x10; break;
	  default:
		fe_irq_failure("Access/PC",
			       sc->sc_unit, dev->id_irq, "3/4/5/10");
		return 0;
	}

	/* Fill softc struct accordingly.  */
	sc->typestr = "Access/PC";
	sc->init = fe_init_ubn;

	/* We have 32 registers.  */
	return 32;
}
#endif	/* PC98 */

#if NCARD > 0
/*
 * Probe and initialization for Fujitsu MBH10302 PCMCIA Ethernet interface.
 * Note that this is for 10302 only; MBH10304 is handled by fe_probe_tdk().
 */

static void
fe_init_mbh ( struct fe_softc * sc )
{
	/* Minimal initialization of 86960.  */
	DELAY( 200 );
	outb( sc->ioaddr[ FE_DLCR6 ], sc->proto_dlcr6 | FE_D6_DLC_DISABLE );
	DELAY( 200 );

	/* Disable all interrupts.  */
	outb( sc->ioaddr[ FE_DLCR2 ], 0 );
	outb( sc->ioaddr[ FE_DLCR3 ], 0 );

	/* Enable master interrupt flag.  */
	outb( sc->ioaddr[ FE_MBH0 ], FE_MBH0_MAGIC | FE_MBH0_INTR_ENABLE );
}

static int
fe_probe_mbh ( struct isa_device * dev, struct fe_softc * sc )
{
	static struct fe_simple_probe_struct probe_table [] = {
		{ FE_DLCR2, 0x58, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
		{ FE_DLCR6, 0xFF, 0xB6 },
		{ 0 }
	};

#ifdef DIAGNOSTIC
	/* We need an explicit IRQ.  */
	if (dev->id_irq == NO_IRQ) return 0;
#endif

	/* Ethernet MAC address should *NOT* have been given by pccardd,
	   if this is a true MBH10302; i.e., Ethernet address must be
	   "all-zero" upon entry.  */
	if (sc->sc_enaddr[0] || sc->sc_enaddr[1] || sc->sc_enaddr[2] ||
	    sc->sc_enaddr[3] || sc->sc_enaddr[4] || sc->sc_enaddr[5]) {
		return 0;
	}

	/* Fill the softc struct with default values.  */
	fe_softc_defaults(sc);

	/*
	 * See if MBH10302 is on its address.
	 * I'm not sure the following probe code works.  FIXME.
	 */
	if ( !fe_simple_probe( sc, probe_table ) ) return 0;

	/* Get our station address from EEPROM.  */
	inblk( sc, FE_MBH10, sc->sc_enaddr, ETHER_ADDR_LEN );

	/* Make sure we got a valid station address.  */
	if (!valid_Ether_p(sc->sc_enaddr, 0)) return 0;

	/* Determine the card type.  */
	sc->typestr = "MBH10302 (PCMCIA)";

	/* We seems to need our own IDENT bits...  FIXME.  */
	sc->proto_dlcr7 = FE_D7_BYTSWP_LH | FE_D7_IDENT_NICE;

	/* Setup hooks.  We need a special initialization procedure.  */
	sc->init = fe_init_mbh;

	/*
	 * That's all.  MBH10302 occupies 32 I/O addresses, by the way.
	 */
	return 32;
}

/*
 * Probe and initialization for TDK/CONTEC PCMCIA Ethernet interface.
 * by MASUI Kenji <masui@cs.titech.ac.jp>
 *
 * (Contec uses TDK Ethenet chip -- hosokawa)
 *
 * This version of fe_probe_tdk has been rewrote to handle
 * *generic* PC card implementation of Fujitsu MB8696x family.  The
 * name _tdk is just for a historical reason. :-)
 */
static int
fe_probe_tdk ( struct isa_device * dev, struct fe_softc * sc )
{
        static struct fe_simple_probe_struct probe_table [] = {
                { FE_DLCR2, 0x50, 0x00 },
                { FE_DLCR4, 0x08, 0x00 },
            /*  { FE_DLCR5, 0x80, 0x00 },       Does not work well.  */
                { 0 }
        };

        if ( dev->id_irq == NO_IRQ ) {
                return ( 0 );
        }

	fe_softc_defaults(sc);

        /*
         * See if C-NET(PC)C is on its address.
         */

        if ( !fe_simple_probe( sc, probe_table ) ) return 0;

        /* Determine the card type.  */
        sc->typestr = "Generic MB8696x/78Q837x Ethernet (PCMCIA)";

        /*
         * Initialize constants in the per-line structure.
         */

        /* Make sure we got a valid station address.  */
        if (!valid_Ether_p(sc->sc_enaddr, 0)) return 0;

        /*
         * That's all.  C-NET(PC)C occupies 16 I/O addresses.
	 * XXX: Are there any card with 32 I/O addresses?  FIXME.
         */
        return 16;
}
#endif /* NCARD > 0 */

/*
 * Install interface into kernel networking data structures
 */
static int
fe_attach ( struct isa_device * dev )
{
#if NCARD > 0
	static	int	already_ifattach[NFE];
#endif
	struct fe_softc *sc = &fe_softc[dev->id_unit];
	int b;

	dev->id_ointr = feintr;

	/*
	 * Initialize ifnet structure
	 */
 	sc->sc_if.if_softc    = sc;
	sc->sc_if.if_unit     = sc->sc_unit;
	sc->sc_if.if_name     = "fe";
	sc->sc_if.if_output   = ether_output;
	sc->sc_if.if_start    = fe_start;
	sc->sc_if.if_ioctl    = fe_ioctl;
	sc->sc_if.if_watchdog = fe_watchdog;
	sc->sc_if.if_init     = fe_init;
	sc->sc_if.if_linkmib  = &sc->mibdata;
	sc->sc_if.if_linkmiblen = sizeof (sc->mibdata);

#if 0 /* I'm not sure... */
	sc->mibdata.dot3Compliance = DOT3COMPLIANCE_COLLS;
#endif

	/*
	 * Set fixed interface flags.
	 */
 	sc->sc_if.if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;

#if 1
	/*
	 * Set maximum size of output queue, if it has not been set.
	 * It is done here as this driver may be started after the
	 * system initialization (i.e., the interface is PCMCIA.)
	 *
	 * I'm not sure this is really necessary, but, even if it is,
	 * it should be done somewhere else, e.g., in if_attach(),
	 * since it must be a common workaround for all network drivers.
	 * FIXME.
	 */
	if ( sc->sc_if.if_snd.ifq_maxlen == 0 ) {
		sc->sc_if.if_snd.ifq_maxlen = ifqmaxlen;
	}
#endif

#if FE_SINGLE_TRANSMISSION
	/* Override txb config to allocate minimum.  */
	sc->proto_dlcr6 &= ~FE_D6_TXBSIZ
	sc->proto_dlcr6 |=  FE_D6_TXBSIZ_2x2KB;
#endif

	/* Modify hardware config if it is requested.  */
	if ( dev->id_flags & FE_FLAGS_OVERRIDE_DLCR6 ) {
		sc->proto_dlcr6 = dev->id_flags & FE_FLAGS_DLCR6_VALUE;
	}

	/* Find TX buffer size, based on the hardware dependent proto.  */
	switch ( sc->proto_dlcr6 & FE_D6_TXBSIZ ) {
	  case FE_D6_TXBSIZ_2x2KB: sc->txb_size = 2048; break;
	  case FE_D6_TXBSIZ_2x4KB: sc->txb_size = 4096; break;
	  case FE_D6_TXBSIZ_2x8KB: sc->txb_size = 8192; break;
	  default:
		/* Oops, we can't work with single buffer configuration.  */
		if (bootverbose) {
			printf("fe%d: strange TXBSIZ config; fixing\n",
			       sc->sc_unit);
		}
		sc->proto_dlcr6 &= ~FE_D6_TXBSIZ;
		sc->proto_dlcr6 |=  FE_D6_TXBSIZ_2x2KB;
		sc->txb_size = 2048;
		break;
	}

	/* Initialize the if_media interface.  */
	ifmedia_init(&sc->media, 0, fe_medchange, fe_medstat );
	for (b = 0; bit2media[b] != 0; b++) {
		if (sc->mbitmap & (1 << b)) {
			ifmedia_add(&sc->media, bit2media[b], 0, NULL);
		}
	}
	for (b = 0; bit2media[b] != 0; b++) {
		if (sc->defmedia & (1 << b)) {
			ifmedia_set(&sc->media, bit2media[b]);
			break;
		}
	}
#if 0	/* Turned off; this is called later, when the interface UPs.  */
	fe_medchange(sc);
#endif

	/* Attach and stop the interface. */
#if NCARD > 0
	if (already_ifattach[dev->id_unit] != 1) {
		if_attach(&sc->sc_if);
		already_ifattach[dev->id_unit] = 1;
	}
#else
	if_attach(&sc->sc_if);
#endif
	fe_stop(sc);
 	ether_ifattach(&sc->sc_if);
  
  	/* Print additional info when attached.  */
 	printf("fe%d: address %6D, type %s%s\n", sc->sc_unit,
	       sc->sc_enaddr, ":" , sc->typestr,
	       (sc->proto_dlcr4 & FE_D4_DSC) ? ", full duplex" : "");
	if (bootverbose) {
		int buf, txb, bbw, sbw, ram;

		buf = txb = bbw = sbw = ram = -1;
		switch ( sc->proto_dlcr6 & FE_D6_BUFSIZ ) {
		  case FE_D6_BUFSIZ_8KB:  buf =  8; break;
		  case FE_D6_BUFSIZ_16KB: buf = 16; break;
		  case FE_D6_BUFSIZ_32KB: buf = 32; break;
		  case FE_D6_BUFSIZ_64KB: buf = 64; break;
		}
		switch ( sc->proto_dlcr6 & FE_D6_TXBSIZ ) {
		  case FE_D6_TXBSIZ_2x2KB: txb = 2; break;
		  case FE_D6_TXBSIZ_2x4KB: txb = 4; break;
		  case FE_D6_TXBSIZ_2x8KB: txb = 8; break;
		}
		switch ( sc->proto_dlcr6 & FE_D6_BBW ) {
		  case FE_D6_BBW_BYTE: bbw =  8; break;
		  case FE_D6_BBW_WORD: bbw = 16; break;
		}
		switch ( sc->proto_dlcr6 & FE_D6_SBW ) {
		  case FE_D6_SBW_BYTE: sbw =  8; break;
		  case FE_D6_SBW_WORD: sbw = 16; break;
		}
		switch ( sc->proto_dlcr6 & FE_D6_SRAM ) {
		  case FE_D6_SRAM_100ns: ram = 100; break;
		  case FE_D6_SRAM_150ns: ram = 150; break;
		}
		printf("fe%d: SRAM %dKB %dbit %dns, TXB %dKBx2, %dbit I/O\n",
			sc->sc_unit, buf, bbw, ram, txb, sbw);
	}
	if (sc->stability & UNSTABLE_IRQ) {
		printf("fe%d: warning: IRQ number may be incorrect\n",
		       sc->sc_unit);
	}
	if (sc->stability & UNSTABLE_MAC) {
		printf("fe%d: warning: above MAC address may be incorrect\n",
		       sc->sc_unit);
	}
	if (sc->stability & UNSTABLE_TYPE) {
		printf("fe%d: warning: hardware type was not validated\n",
		       sc->sc_unit);
	}

#if NBPFILTER > 0
	/* If BPF is in the kernel, call the attach for it.  */
 	bpfattach(&sc->sc_if, DLT_EN10MB, sizeof(struct ether_header));
#endif
	return 1;
}

/*
 * Reset interface, after some (hardware) trouble is deteced.
 */
static void
fe_reset (struct fe_softc *sc)
{
	/* Record how many packets are lost by this accident.  */
	sc->sc_if.if_oerrors += sc->txb_sched + sc->txb_count;
	sc->mibdata.dot3StatsInternalMacTransmitErrors++;

	/* Put the interface into known initial state.  */
	fe_stop(sc);
	if (sc->sc_if.if_flags & IFF_UP) fe_init(sc);
}

/*
 * Stop everything on the interface.
 *
 * All buffered packets, both transmitting and receiving,
 * if any, will be lost by stopping the interface.
 */
static void
fe_stop (struct fe_softc *sc)
{
	int s;

	s = splimp();

	/* Disable interrupts.  */
	outb( sc->ioaddr[ FE_DLCR2 ], 0x00 );
	outb( sc->ioaddr[ FE_DLCR3 ], 0x00 );

	/* Stop interface hardware.  */
	DELAY( 200 );
	outb( sc->ioaddr[ FE_DLCR6 ], sc->proto_dlcr6 | FE_D6_DLC_DISABLE );
	DELAY( 200 );

	/* Clear all interrupt status.  */
	outb( sc->ioaddr[ FE_DLCR0 ], 0xFF );
	outb( sc->ioaddr[ FE_DLCR1 ], 0xFF );

	/* Put the chip in stand-by mode.  */
	DELAY( 200 );
	outb( sc->ioaddr[ FE_DLCR7 ], sc->proto_dlcr7 | FE_D7_POWER_DOWN );
	DELAY( 200 );

	/* Reset transmitter variables and interface flags.  */
	sc->sc_if.if_flags &= ~( IFF_OACTIVE | IFF_RUNNING );
	sc->sc_if.if_timer = 0;
	sc->txb_free = sc->txb_size;
	sc->txb_count = 0;
	sc->txb_sched = 0;

	/* MAR loading can be delayed.  */
	sc->filter_change = 0;

	/* Call a device-specific hook.  */
	if ( sc->stop ) sc->stop( sc );

	(void) splx(s);
}

/*
 * Device timeout/watchdog routine. Entered if the device neglects to
 * generate an interrupt after a transmit has been started on it.
 */
static void
fe_watchdog ( struct ifnet *ifp )
{
	struct fe_softc *sc = (struct fe_softc *)ifp;

	/* A "debug" message.  */
	printf("fe%d: transmission timeout (%d+%d)%s\n",
	       ifp->if_unit, sc->txb_sched, sc->txb_count,
	       (ifp->if_flags & IFF_UP) ? "" : " when down");
	if ( sc->sc_if.if_opackets == 0 && sc->sc_if.if_ipackets == 0 ) {
		printf("fe%d: wrong IRQ setting in config?\n", ifp->if_unit);
	}
	fe_reset( sc );
}

/*
 * Initialize device.
 */
static void
fe_init (void * xsc)
{
	struct fe_softc *sc = xsc;
	int s;

	/* We need an address. */
	if (TAILQ_EMPTY(&sc->sc_if.if_addrhead)) { /* XXX unlikely */
#ifdef DIAGNOSTIC
		printf("fe%d: init() without any address\n", sc->sc_unit);
#endif
		return;
	}

	/* Start initializing 86960.  */
	s = splimp();

	/* Call a hook before we start initializing the chip.  */
	if ( sc->init ) sc->init( sc );

	/*
	 * Make sure to disable the chip, also.
	 * This may also help re-programming the chip after
	 * hot insertion of PCMCIAs.
	 */
	DELAY( 200 );
	outb( sc->ioaddr[ FE_DLCR6 ], sc->proto_dlcr6 | FE_D6_DLC_DISABLE );
	DELAY( 200 );

	/* Power up the chip and select register bank for DLCRs.  */
	DELAY( 200 );
	outb( sc->ioaddr[ FE_DLCR7 ],
		sc->proto_dlcr7 | FE_D7_RBS_DLCR | FE_D7_POWER_UP );
	DELAY( 200 );

	/* Feed the station address.  */
	outblk( sc, FE_DLCR8, sc->sc_enaddr, ETHER_ADDR_LEN );

	/* Clear multicast address filter to receive nothing.  */
	outb( sc->ioaddr[ FE_DLCR7 ],
		sc->proto_dlcr7 | FE_D7_RBS_MAR | FE_D7_POWER_UP );
	outblk( sc, FE_MAR8, fe_filter_nothing.data, FE_FILTER_LEN );

	/* Select the BMPR bank for runtime register access.  */
	outb( sc->ioaddr[ FE_DLCR7 ],
		sc->proto_dlcr7 | FE_D7_RBS_BMPR | FE_D7_POWER_UP );

	/* Initialize registers.  */
	outb( sc->ioaddr[ FE_DLCR0 ], 0xFF );	/* Clear all bits.  */
	outb( sc->ioaddr[ FE_DLCR1 ], 0xFF );	/* ditto.  */
	outb( sc->ioaddr[ FE_DLCR2 ], 0x00 );
	outb( sc->ioaddr[ FE_DLCR3 ], 0x00 );
	outb( sc->ioaddr[ FE_DLCR4 ], sc->proto_dlcr4 );
	outb( sc->ioaddr[ FE_DLCR5 ], sc->proto_dlcr5 );
	outb( sc->ioaddr[ FE_BMPR10 ], 0x00 );
	outb( sc->ioaddr[ FE_BMPR11 ], FE_B11_CTRL_SKIP | FE_B11_MODE1 );
	outb( sc->ioaddr[ FE_BMPR12 ], 0x00 );
	outb( sc->ioaddr[ FE_BMPR13 ], sc->proto_bmpr13 );
	outb( sc->ioaddr[ FE_BMPR14 ], 0x00 );
	outb( sc->ioaddr[ FE_BMPR15 ], 0x00 );

	/* Enable interrupts.  */
	outb( sc->ioaddr[ FE_DLCR2 ], FE_TMASK );
	outb( sc->ioaddr[ FE_DLCR3 ], FE_RMASK );

	/* Select requested media, just before enabling DLC.  */
	if (sc->msel) sc->msel(sc);

	/* Enable transmitter and receiver.  */
	DELAY( 200 );
	outb( sc->ioaddr[ FE_DLCR6 ], sc->proto_dlcr6 | FE_D6_DLC_ENABLE );
	DELAY( 200 );

#ifdef DIAGNOSTIC
	/*
	 * Make sure to empty the receive buffer.
	 *
	 * This may be redundant, but *if* the receive buffer were full
	 * at this point, then the driver would hang.  I have experienced
	 * some strange hang-up just after UP.  I hope the following
	 * code solve the problem.
	 *
	 * I have changed the order of hardware initialization.
	 * I think the receive buffer cannot have any packets at this
	 * point in this version.  The following code *must* be
	 * redundant now.  FIXME.
	 *
	 * I've heard a rumore that on some PC card implementation of
	 * 8696x, the receive buffer can have some data at this point.
	 * The following message helps discovering the fact.  FIXME.
	 */
	if ( !( inb( sc->ioaddr[ FE_DLCR5 ] ) & FE_D5_BUFEMP ) ) {
		printf("fe%d: receive buffer has some data after reset\n",
		       sc->sc_unit);
		fe_emptybuffer( sc );
	}

	/* Do we need this here?  Actually, no.  I must be paranoia.  */
	outb( sc->ioaddr[ FE_DLCR0 ], 0xFF );	/* Clear all bits.  */
	outb( sc->ioaddr[ FE_DLCR1 ], 0xFF );	/* ditto.  */
#endif

	/* Set 'running' flag, because we are now running.   */
	sc->sc_if.if_flags |= IFF_RUNNING;

	/*
	 * At this point, the interface is running properly,
	 * except that it receives *no* packets.  we then call
	 * fe_setmode() to tell the chip what packets to be
	 * received, based on the if_flags and multicast group
	 * list.  It completes the initialization process.
	 */
	fe_setmode( sc );

#if 0
	/* ...and attempt to start output queued packets.  */
	/* TURNED OFF, because the semi-auto media prober wants to UP
           the interface keeping it idle.  The upper layer will soon
           start the interface anyway, and there are no significant
           delay.  */
	fe_start( &sc->sc_if );
#endif

	(void) splx(s);
}

/*
 * This routine actually starts the transmission on the interface
 */
static void
fe_xmit ( struct fe_softc * sc )
{
	/*
	 * Set a timer just in case we never hear from the board again.
	 * We use longer timeout for multiple packet transmission.
	 * I'm not sure this timer value is appropriate.  FIXME.
	 */
	sc->sc_if.if_timer = 1 + sc->txb_count;

	/* Update txb variables.  */
	sc->txb_sched = sc->txb_count;
	sc->txb_count = 0;
	sc->txb_free = sc->txb_size;
	sc->tx_excolls = 0;

	/* Start transmitter, passing packets in TX buffer.  */
	outb( sc->ioaddr[ FE_BMPR10 ], sc->txb_sched | FE_B10_START );
}

/*
 * Start output on interface.
 * We make two assumptions here:
 *  1) that the current priority is set to splimp _before_ this code
 *     is called *and* is returned to the appropriate priority after
 *     return
 *  2) that the IFF_OACTIVE flag is checked before this code is called
 *     (i.e. that the output part of the interface is idle)
 */
void
fe_start ( struct ifnet *ifp )
{
	struct fe_softc *sc = ifp->if_softc;
	struct mbuf *m;

#ifdef DIAGNOSTIC
	/* Just a sanity check.  */
	if ( ( sc->txb_count == 0 ) != ( sc->txb_free == sc->txb_size ) ) {
		/*
		 * Txb_count and txb_free co-works to manage the
		 * transmission buffer.  Txb_count keeps track of the
		 * used potion of the buffer, while txb_free does unused
		 * potion.  So, as long as the driver runs properly,
		 * txb_count is zero if and only if txb_free is same
		 * as txb_size (which represents whole buffer.)
		 */
		printf("fe%d: inconsistent txb variables (%d, %d)\n",
			sc->sc_unit, sc->txb_count, sc->txb_free);
		/*
		 * So, what should I do, then?
		 *
		 * We now know txb_count and txb_free contradicts.  We
		 * cannot, however, tell which is wrong.  More
		 * over, we cannot peek 86960 transmission buffer or
		 * reset the transmission buffer.  (In fact, we can
		 * reset the entire interface.  I don't want to do it.)
		 *
		 * If txb_count is incorrect, leaving it as-is will cause
		 * sending of garbage after next interrupt.  We have to
		 * avoid it.  Hence, we reset the txb_count here.  If
		 * txb_free was incorrect, resetting txb_count just loose
		 * some packets.  We can live with it.
		 */
		sc->txb_count = 0;
	}
#endif

	/*
	 * First, see if there are buffered packets and an idle
	 * transmitter - should never happen at this point.
	 */
	if ( ( sc->txb_count > 0 ) && ( sc->txb_sched == 0 ) ) {
		printf("fe%d: transmitter idle with %d buffered packets\n",
		       sc->sc_unit, sc->txb_count);
		fe_xmit( sc );
	}

	/*
	 * Stop accepting more transmission packets temporarily, when
	 * a filter change request is delayed.  Updating the MARs on
	 * 86960 flushes the transmission buffer, so it is delayed
	 * until all buffered transmission packets have been sent
	 * out.
	 */
	if ( sc->filter_change ) {
		/*
		 * Filter change request is delayed only when the DLC is
		 * working.  DLC soon raise an interrupt after finishing
		 * the work.
		 */
		goto indicate_active;
	}

	for (;;) {

		/*
		 * See if there is room to put another packet in the buffer.
		 * We *could* do better job by peeking the send queue to
		 * know the length of the next packet.  Current version just
		 * tests against the worst case (i.e., longest packet).  FIXME.
		 *
		 * When adding the packet-peek feature, don't forget adding a
		 * test on txb_count against QUEUEING_MAX.
		 * There is a little chance the packet count exceeds
		 * the limit.  Assume transmission buffer is 8KB (2x8KB
		 * configuration) and an application sends a bunch of small
		 * (i.e., minimum packet sized) packets rapidly.  An 8KB
		 * buffer can hold 130 blocks of 62 bytes long...
		 */
		if ( sc->txb_free
		    < ETHER_MAX_LEN - ETHER_CRC_LEN + FE_DATA_LEN_LEN ) {
			/* No room.  */
			goto indicate_active;
		}

#if FE_SINGLE_TRANSMISSION
		if ( sc->txb_count > 0 ) {
			/* Just one packet per a transmission buffer.  */
			goto indicate_active;
		}
#endif

		/*
		 * Get the next mbuf chain for a packet to send.
		 */
		IF_DEQUEUE( &sc->sc_if.if_snd, m );
		if ( m == NULL ) {
			/* No more packets to send.  */
			goto indicate_inactive;
		}

		/*
		 * Copy the mbuf chain into the transmission buffer.
		 * txb_* variables are updated as necessary.
		 */
		fe_write_mbufs( sc, m );

		/* Start transmitter if it's idle.  */
		if ( ( sc->txb_count > 0 ) && ( sc->txb_sched == 0 ) ) {
			fe_xmit( sc );
		}

		/*
		 * Tap off here if there is a bpf listener,
		 * and the device is *not* in promiscuous mode.
		 * (86960 receives self-generated packets if 
		 * and only if it is in "receive everything"
		 * mode.)
		 */
#if NBPFILTER > 0
		if ( sc->sc_if.if_bpf
		  && !( sc->sc_if.if_flags & IFF_PROMISC ) ) {
			bpf_mtap( &sc->sc_if, m );
		}
#endif

		m_freem( m );
	}

  indicate_inactive:
	/*
	 * We are using the !OACTIVE flag to indicate to
	 * the outside world that we can accept an
	 * additional packet rather than that the
	 * transmitter is _actually_ active.  Indeed, the
	 * transmitter may be active, but if we haven't
	 * filled all the buffers with data then we still
	 * want to accept more.
	 */
	sc->sc_if.if_flags &= ~IFF_OACTIVE;
	return;

  indicate_active:
	/*
	 * The transmitter is active, and there are no room for
	 * more outgoing packets in the transmission buffer.
	 */
	sc->sc_if.if_flags |= IFF_OACTIVE;
	return;
}

/*
 * Drop (skip) a packet from receive buffer in 86960 memory.
 */
static void
fe_droppacket ( struct fe_softc * sc, int len )
{
	int i;

	/*
	 * 86960 manual says that we have to read 8 bytes from the buffer
	 * before skip the packets and that there must be more than 8 bytes
	 * remaining in the buffer when issue a skip command.
	 * Remember, we have already read 4 bytes before come here.
	 */
	if ( len > 12 ) {
		/* Read 4 more bytes, and skip the rest of the packet.  */
#ifdef FE_8BIT_SUPPORT
		if ((sc->proto_dlcr6 & FE_D6_SBW) == FE_D6_SBW_BYTE)
		{
			( void )inb( sc->ioaddr[ FE_BMPR8 ] );
			( void )inb( sc->ioaddr[ FE_BMPR8 ] );
			( void )inb( sc->ioaddr[ FE_BMPR8 ] );
			( void )inb( sc->ioaddr[ FE_BMPR8 ] );
		}
		else
#endif
		{
			( void )inw( sc->ioaddr[ FE_BMPR8 ] );
			( void )inw( sc->ioaddr[ FE_BMPR8 ] );
		}
		outb( sc->ioaddr[ FE_BMPR14 ], FE_B14_SKIP );
	} else {
		/* We should not come here unless receiving RUNTs.  */
#ifdef FE_8BIT_SUPPORT
		if ((sc->proto_dlcr6 & FE_D6_SBW) == FE_D6_SBW_BYTE)
		{
			for ( i = 0; i < len; i++ ) {
				( void )inb( sc->ioaddr[ FE_BMPR8 ] );
			}
		}
		else
#endif
		{
			for ( i = 0; i < len; i += 2 ) {
				( void )inw( sc->ioaddr[ FE_BMPR8 ] );
			}
		}
	}
}

#ifdef DIAGNOSTIC
/*
 * Empty receiving buffer.
 */
static void
fe_emptybuffer ( struct fe_softc * sc )
{
	int i;
	u_char saved_dlcr5;

#ifdef FE_DEBUG
	printf("fe%d: emptying receive buffer\n", sc->sc_unit);
#endif

	/*
	 * Stop receiving packets, temporarily.
	 */
	saved_dlcr5 = inb( sc->ioaddr[ FE_DLCR5 ] );
	outb( sc->ioaddr[ FE_DLCR5 ], sc->proto_dlcr5 );
	DELAY(1300);

	/*
	 * When we come here, the receive buffer management may
	 * have been broken.  So, we cannot use skip operation.
	 * Just discard everything in the buffer.
	 */
#ifdef FE_8BIT_SUPPORT
	if ((sc->proto_dlcr6 & FE_D6_SBW) == FE_D6_SBW_BYTE)
	{
		for ( i = 0; i < 65536; i++ ) {
			if ( inb( sc->ioaddr[ FE_DLCR5 ] ) & FE_D5_BUFEMP ) break;
			( void )inb( sc->ioaddr[ FE_BMPR8 ] );
		}
	}
	else
#endif
	{
		for ( i = 0; i < 65536; i += 2 ) {
			if ( inb( sc->ioaddr[ FE_DLCR5 ] ) & FE_D5_BUFEMP ) break;
			( void )inw( sc->ioaddr[ FE_BMPR8 ] );
		}
	}

	/*
	 * Double check.
	 */
	if ( inb( sc->ioaddr[ FE_DLCR5 ] ) & FE_D5_BUFEMP ) {
		printf("fe%d: could not empty receive buffer\n", sc->sc_unit);
		/* Hmm.  What should I do if this happens?  FIXME.  */
	}

	/*
	 * Restart receiving packets.
	 */
	outb( sc->ioaddr[ FE_DLCR5 ], saved_dlcr5 );
}
#endif

/*
 * Transmission interrupt handler
 * The control flow of this function looks silly.  FIXME.
 */
static void
fe_tint ( struct fe_softc * sc, u_char tstat )
{
	int left;
	int col;

	/*
	 * Handle "excessive collision" interrupt.
	 */
	if ( tstat & FE_D0_COLL16 ) {

		/*
		 * Find how many packets (including this collided one)
		 * are left unsent in transmission buffer.
		 */
		left = inb( sc->ioaddr[ FE_BMPR10 ] );
		printf("fe%d: excessive collision (%d/%d)\n",
		       sc->sc_unit, left, sc->txb_sched);

		/*
		 * Clear the collision flag (in 86960) here
		 * to avoid confusing statistics.
		 */
		outb( sc->ioaddr[ FE_DLCR0 ], FE_D0_COLLID );

		/*
		 * Restart transmitter, skipping the
		 * collided packet.
		 *
		 * We *must* skip the packet to keep network running
		 * properly.  Excessive collision error is an
		 * indication of the network overload.  If we
		 * tried sending the same packet after excessive
		 * collision, the network would be filled with
		 * out-of-time packets.  Packets belonging
		 * to reliable transport (such as TCP) are resent
		 * by some upper layer.
		 */
		outb( sc->ioaddr[ FE_BMPR11 ],
			FE_B11_CTRL_SKIP | FE_B11_MODE1 );

		/* Update statistics.  */
		sc->tx_excolls++;
	}

	/*
	 * Handle "transmission complete" interrupt.
	 */
	if ( tstat & FE_D0_TXDONE ) {

		/*
		 * Add in total number of collisions on last
		 * transmission.  We also clear "collision occurred" flag
		 * here.
		 *
		 * 86960 has a design flaw on collision count on multiple
		 * packet transmission.  When we send two or more packets
		 * with one start command (that's what we do when the
		 * transmission queue is crowded), 86960 informs us number
		 * of collisions occurred on the last packet on the
		 * transmission only.  Number of collisions on previous
		 * packets are lost.  I have told that the fact is clearly
		 * stated in the Fujitsu document.
		 *
		 * I considered not to mind it seriously.  Collision
		 * count is not so important, anyway.  Any comments?  FIXME.
		 */

		if ( inb( sc->ioaddr[ FE_DLCR0 ] ) & FE_D0_COLLID ) {

			/* Clear collision flag.  */
			outb( sc->ioaddr[ FE_DLCR0 ], FE_D0_COLLID );

			/* Extract collision count from 86960.  */
			col = inb( sc->ioaddr[ FE_DLCR4 ] );
			col = ( col & FE_D4_COL ) >> FE_D4_COL_SHIFT;
			if ( col == 0 ) {
				/*
				 * Status register indicates collisions,
				 * while the collision count is zero.
				 * This can happen after multiple packet
				 * transmission, indicating that one or more
				 * previous packet(s) had been collided.
				 *
				 * Since the accurate number of collisions
				 * has been lost, we just guess it as 1;
				 * Am I too optimistic?  FIXME.
				 */
				col = 1;
			}
			sc->sc_if.if_collisions += col;
			if ( col == 1 ) {
				sc->mibdata.dot3StatsSingleCollisionFrames++;
			} else {
				sc->mibdata.dot3StatsMultipleCollisionFrames++;
			}
			sc->mibdata.dot3StatsCollFrequencies[col-1]++;
		}

		/*
		 * Update transmission statistics.
		 * Be sure to reflect number of excessive collisions.
		 */
		col = sc->tx_excolls;
		sc->sc_if.if_opackets += sc->txb_sched - col;
		sc->sc_if.if_oerrors += col;
		sc->sc_if.if_collisions += col * 16;
		sc->mibdata.dot3StatsExcessiveCollisions += col;
		sc->mibdata.dot3StatsCollFrequencies[15] += col;
		sc->txb_sched = 0;

		/*
		 * The transmitter is no more active.
		 * Reset output active flag and watchdog timer.
		 */
		sc->sc_if.if_flags &= ~IFF_OACTIVE;
		sc->sc_if.if_timer = 0;

		/*
		 * If more data is ready to transmit in the buffer, start
		 * transmitting them.  Otherwise keep transmitter idle,
		 * even if more data is queued.  This gives receive
		 * process a slight priority.
		 */
		if ( sc->txb_count > 0 ) fe_xmit( sc );
	}
}

/*
 * Ethernet interface receiver interrupt.
 */
static void
fe_rint ( struct fe_softc * sc, u_char rstat )
{
	u_short len;
	u_char status;
	int i;

	/*
	 * Update statistics if this interrupt is caused by an error.
	 * Note that, when the system was not sufficiently fast, the
	 * receive interrupt might not be acknowledged immediately.  If
	 * one or more errornous frames were received before this routine
	 * was scheduled, they are ignored, and the following error stats
	 * give less than real values.
	 */
	if ( rstat & ( FE_D1_OVRFLO | FE_D1_CRCERR
		     | FE_D1_ALGERR | FE_D1_SRTPKT ) ) {
		if ( rstat & FE_D1_OVRFLO )
			sc->mibdata.dot3StatsInternalMacReceiveErrors++;
		if ( rstat & FE_D1_CRCERR )
			sc->mibdata.dot3StatsFCSErrors++;
		if ( rstat & FE_D1_ALGERR )
			sc->mibdata.dot3StatsAlignmentErrors++;
#if 0
		/* The reference MAC receiver defined in 802.3
		   silently ignores short frames (RUNTs) without
		   notifying upper layer.  RFC 1650 (dot3 MIB) is
		   based on the 802.3, and it has no stats entry for
		   RUNTs...  */
		if ( rstat & FE_D1_SRTPKT )
			sc->mibdata.dot3StatsFrameTooShorts++; /* :-) */
#endif
		sc->sc_if.if_ierrors++;
	}

	/*
	 * MB86960 has a flag indicating "receive queue empty."
	 * We just loop, checking the flag, to pull out all received
	 * packets.
	 *
	 * We limit the number of iterations to avoid infinite-loop.
	 * The upper bound is set to unrealistic high value.
	 */
	for ( i = 0; i < FE_MAX_RECV_COUNT * 2; i++ ) {

		/* Stop the iteration if 86960 indicates no packets.  */
		if ( inb( sc->ioaddr[ FE_DLCR5 ] ) & FE_D5_BUFEMP ) return;

		/*
		 * Extract a receive status byte.
		 * As our 86960 is in 16 bit bus access mode, we have to
		 * use inw() to get the status byte.  The significant
		 * value is returned in lower 8 bits.
		 */
#ifdef FE_8BIT_SUPPORT
		if ((sc->proto_dlcr6 & FE_D6_SBW) == FE_D6_SBW_BYTE)
		{
			status = inb( sc->ioaddr[ FE_BMPR8 ] );
			( void ) inb( sc->ioaddr[ FE_BMPR8 ] );
		}
		else
#endif
		{
			status = ( u_char )inw( sc->ioaddr[ FE_BMPR8 ] );
		}	

		/*
		 * Extract the packet length.
		 * It is a sum of a header (14 bytes) and a payload.
		 * CRC has been stripped off by the 86960.
		 */
#ifdef FE_8BIT_SUPPORT
		if ((sc->proto_dlcr6 & FE_D6_SBW) == FE_D6_SBW_BYTE)
		{
			len  =   inb( sc->ioaddr[ FE_BMPR8 ] );
			len |= ( inb( sc->ioaddr[ FE_BMPR8 ] ) << 8 );
		}
		else
#endif
		{
			len = inw( sc->ioaddr[ FE_BMPR8 ] );
		}

		/*
		 * AS our 86960 is programed to ignore errored frame,
		 * we must not see any error indication in the
		 * receive buffer.  So, any error condition is a
		 * serious error, e.g., out-of-sync of the receive
		 * buffer pointers.
		 */
		if ( ( status & 0xF0 ) != 0x20
		     || len > ETHER_MAX_LEN - ETHER_CRC_LEN
		     || len < ETHER_MIN_LEN - ETHER_CRC_LEN ) {
			printf("fe%d: RX buffer out-of-sync\n", sc->sc_unit);
			sc->sc_if.if_ierrors++;
			sc->mibdata.dot3StatsInternalMacReceiveErrors++;
			fe_reset(sc);
			return;
		}

		/*
		 * Go get a packet.
		 */
		if ( fe_get_packet( sc, len ) < 0 ) {
			/*
			 * Negative return from fe_get_packet()
			 * indicates no available mbuf.  We stop
			 * receiving packets, even if there are more
			 * in the buffer.  We hope we can get more
			 * mbuf next time.
			 */
			sc->sc_if.if_ierrors++;
			sc->mibdata.dot3StatsMissedFrames++;
			fe_droppacket( sc, len );
			return;
		}

		/* Successfully received a packet.  Update stat.  */
		sc->sc_if.if_ipackets++;
	}

	/* Maximum number of frames has been received.  Something
           strange is happening here... */
	printf("fe%d: unusual receive flood\n", sc->sc_unit);
	sc->mibdata.dot3StatsInternalMacReceiveErrors++;
	fe_reset(sc);
}

/*
 * Ethernet interface interrupt processor
 */
static void
feintr ( int unit )
{
	struct fe_softc *sc = &fe_softc[unit];
	u_char tstat, rstat;
	int loop_count = FE_MAX_LOOP;

	/* Loop until there are no more new interrupt conditions.  */
	while (loop_count-- > 0) {
		/*
		 * Get interrupt conditions, masking unneeded flags.
		 */
		tstat = inb( sc->ioaddr[ FE_DLCR0 ] ) & FE_TMASK;
		rstat = inb( sc->ioaddr[ FE_DLCR1 ] ) & FE_RMASK;
		if ( tstat == 0 && rstat == 0 ) return;

		/*
		 * Reset the conditions we are acknowledging.
		 */
		outb( sc->ioaddr[ FE_DLCR0 ], tstat );
		outb( sc->ioaddr[ FE_DLCR1 ], rstat );

		/*
		 * Handle transmitter interrupts.
		 */
		if ( tstat ) {
			fe_tint( sc, tstat );
		}

		/*
		 * Handle receiver interrupts
		 */
		if ( rstat ) {
			fe_rint( sc, rstat );
		}

		/*
		 * Update the multicast address filter if it is
		 * needed and possible.  We do it now, because
		 * we can make sure the transmission buffer is empty,
		 * and there is a good chance that the receive queue
		 * is empty.  It will minimize the possibility of
		 * packet loss.
		 */
		if ( sc->filter_change
		  && sc->txb_count == 0 && sc->txb_sched == 0 ) {
			fe_loadmar(sc);
			sc->sc_if.if_flags &= ~IFF_OACTIVE;
		}

		/*
		 * If it looks like the transmitter can take more data,
		 * attempt to start output on the interface. This is done
		 * after handling the receiver interrupt to give the
		 * receive operation priority.
		 *
		 * BTW, I'm not sure in what case the OACTIVE is on at
		 * this point.  Is the following test redundant?
		 *
		 * No.  This routine polls for both transmitter and
		 * receiver interrupts.  86960 can raise a receiver
		 * interrupt when the transmission buffer is full.
		 */
		if ( ( sc->sc_if.if_flags & IFF_OACTIVE ) == 0 ) {
			fe_start( &sc->sc_if );
		}

	}

	printf("fe%d: too many loops\n", sc->sc_unit);
	return;
}

/*
 * Process an ioctl request. This code needs some work - it looks
 * pretty ugly.
 */
static int
fe_ioctl ( struct ifnet * ifp, u_long command, caddr_t data )
{
	struct fe_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	s = splimp();

	switch (command) {

	  case SIOCSIFADDR:
	  case SIOCGIFADDR:
	  case SIOCSIFMTU:
		/* Just an ordinary action.  */
		error = ether_ioctl(ifp, command, data);
		break;

	  case SIOCSIFFLAGS:
		/*
		 * Switch interface state between "running" and
		 * "stopped", reflecting the UP flag.
		 */
		if ( sc->sc_if.if_flags & IFF_UP ) {
			if ( ( sc->sc_if.if_flags & IFF_RUNNING ) == 0 ) {
				fe_init(sc);
			}
		} else {
			if ( ( sc->sc_if.if_flags & IFF_RUNNING ) != 0 ) {
				fe_stop(sc);
			}
		}

		/*
		 * Promiscuous and/or multicast flags may have changed,
		 * so reprogram the multicast filter and/or receive mode.
		 */
		fe_setmode( sc );

		/* Done.  */
		break;

	  case SIOCADDMULTI:
	  case SIOCDELMULTI:
		/*
		 * Multicast list has changed; set the hardware filter
		 * accordingly.
		 */
		fe_setmode( sc );
		break;

	  case SIOCSIFMEDIA:
	  case SIOCGIFMEDIA:
		/* Let if_media to handle these commands and to call
		   us back.  */
		error = ifmedia_ioctl(ifp, ifr, &sc->media, command);
		break;

	  default:
		error = EINVAL;
		break;
	}

	(void) splx(s);
	return (error);
}

/*
 * Retrieve packet from receive buffer and send to the next level up via
 * ether_input(). If there is a BPF listener, give a copy to BPF, too.
 * Returns 0 if success, -1 if error (i.e., mbuf allocation failure).
 */
static int
fe_get_packet ( struct fe_softc * sc, u_short len )
{
	struct ether_header *eh;
	struct mbuf *m;

	/*
	 * NFS wants the data be aligned to the word (4 byte)
	 * boundary.  Ethernet header has 14 bytes.  There is a
	 * 2-byte gap.
	 */
#define NFS_MAGIC_OFFSET 2

	/*
	 * This function assumes that an Ethernet packet fits in an
	 * mbuf (with a cluster attached when necessary.)  On FreeBSD
	 * 2.0 for x86, which is the primary target of this driver, an
	 * mbuf cluster has 4096 bytes, and we are happy.  On ancient
	 * BSDs, such as vanilla 4.3 for 386, a cluster size was 1024,
	 * however.  If the following #error message were printed upon
	 * compile, you need to rewrite this function.
	 */
#if ( MCLBYTES < ETHER_MAX_LEN - ETHER_CRC_LEN + NFS_MAGIC_OFFSET )
#error "Too small MCLBYTES to use fe driver."
#endif

	/*
	 * Our strategy has one more problem.  There is a policy on
	 * mbuf cluster allocation.  It says that we must have at
	 * least MINCLSIZE (208 bytes on FreeBSD 2.0 for x86) to
	 * allocate a cluster.  For a packet of a size between
	 * (MHLEN - 2) to (MINCLSIZE - 2), our code violates the rule...
	 * On the other hand, the current code is short, simple,
	 * and fast, however.  It does no harmful thing, just waists
	 * some memory.  Any comments?  FIXME.
	 */

	/* Allocate an mbuf with packet header info.  */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if ( m == NULL ) return -1;

	/* Attach a cluster if this packet doesn't fit in a normal mbuf.  */
	if ( len > MHLEN - NFS_MAGIC_OFFSET ) {
		MCLGET( m, M_DONTWAIT );
		if ( !( m->m_flags & M_EXT ) ) {
			m_freem( m );
			return -1;
		}
	}

	/* Initialize packet header info.  */
	m->m_pkthdr.rcvif = &sc->sc_if;
	m->m_pkthdr.len = len;

	/* Set the length of this packet.  */
	m->m_len = len;

	/* The following silliness is to make NFS happy */
	m->m_data += NFS_MAGIC_OFFSET;

	/* Get (actually just point to) the header part.  */
	eh = mtod(m, struct ether_header *);

	/* Get a packet.  */
#ifdef FE_8BIT_SUPPORT
	if ((sc->proto_dlcr6 & FE_D6_SBW) == FE_D6_SBW_BYTE)
	{
		insb( sc->ioaddr[ FE_BMPR8 ], eh,   len );
	}
	else
#endif
	{
		insw( sc->ioaddr[ FE_BMPR8 ], eh, ( len + 1 ) >> 1 );
	}

#define ETHER_ADDR_IS_MULTICAST(A) (*(char *)(A) & 1)

#if NBPFILTER > 0
	/*
	 * Check if there's a BPF listener on this interface.
	 * If it is, hand off the raw packet to bpf.
	 */
	if ( sc->sc_if.if_bpf ) {
		bpf_mtap( &sc->sc_if, m );
	}
#endif

#ifdef BRIDGE
	if (do_bridge) {
		struct ifnet *ifp;

		ifp = bridge_in(m);
		if (ifp == BDG_DROP) {
			m_freem(m);
			return 0;
		}
		if (ifp != BDG_LOCAL)
			bdg_forward(&m, ifp); /* not local, need forwarding */
		if (ifp == BDG_LOCAL || ifp == BDG_BCAST || ifp == BDG_MCAST)
			goto getit;
		/* not local and not multicast, just drop it */
		if (m)
			m_freem(m);
		return 0;
	}
#endif

	/*
	 * Make sure this packet is (or may be) directed to us.
	 * That is, the packet is either unicasted to our address,
	 * or broad/multi-casted.  If any other packets are
	 * received, it is an indication of an error -- probably
	 * 86960 is in a wrong operation mode.
	 * Promiscuous mode is an exception.  Under the mode, all
	 * packets on the media must be received.  (We must have
	 * programmed the 86960 so.)
	 */

	if ( ( sc->sc_if.if_flags & IFF_PROMISC )
	  && !ETHER_ADDR_IS_MULTICAST( eh->ether_dhost )
	  && bcmp( eh->ether_dhost, sc->sc_enaddr, ETHER_ADDR_LEN ) != 0 ) {
		/*
		 * The packet was not for us.  This is normal since
		 * we are now in promiscuous mode.  Just drop the packet.
		 */
		m_freem( m );
		return 0;
	}

#ifdef BRIDGE
getit:
#endif
	/* Strip off the Ethernet header.  */
	m->m_pkthdr.len -= sizeof ( struct ether_header );
	m->m_len -= sizeof ( struct ether_header );
	m->m_data += sizeof ( struct ether_header );

	/* Feed the packet to upper layer.  */
	ether_input( &sc->sc_if, eh, m );
	return 0;
}

/*
 * Write an mbuf chain to the transmission buffer memory using 16 bit PIO.
 * Returns number of bytes actually written, including length word.
 *
 * If an mbuf chain is too long for an Ethernet frame, it is not sent.
 * Packets shorter than Ethernet minimum are legal, and we pad them
 * before sending out.  An exception is "partial" packets which are
 * shorter than mandatory Ethernet header.
 */
static void
fe_write_mbufs ( struct fe_softc *sc, struct mbuf *m )
{
	u_short addr_bmpr8 = sc->ioaddr[ FE_BMPR8 ];
	u_short length, len;
	struct mbuf *mp;
	u_char *data;
	u_short savebyte;	/* WARNING: Architecture dependent!  */
#define NO_PENDING_BYTE 0xFFFF

	static u_char padding [ ETHER_MIN_LEN - ETHER_CRC_LEN - ETHER_HDR_LEN ];

#ifdef DIAGNOSTIC
	/* First, count up the total number of bytes to copy */
	length = 0;
	for ( mp = m; mp != NULL; mp = mp->m_next ) {
		length += mp->m_len;
	}
	/* Check if this matches the one in the packet header.  */
	if ( length != m->m_pkthdr.len ) {
		printf("fe%d: packet length mismatch? (%d/%d)\n", sc->sc_unit,
		       length, m->m_pkthdr.len);
	}
#else
	/* Just use the length value in the packet header.  */
	length = m->m_pkthdr.len;
#endif

#ifdef DIAGNOSTIC
	/*
	 * Should never send big packets.  If such a packet is passed,
	 * it should be a bug of upper layer.  We just ignore it.
	 * ... Partial (too short) packets, neither.
	 */
	if ( length < ETHER_HDR_LEN
	  || length > ETHER_MAX_LEN - ETHER_CRC_LEN ) {
		printf("fe%d: got an out-of-spec packet (%u bytes) to send\n",
			sc->sc_unit, length);
		sc->sc_if.if_oerrors++;
		sc->mibdata.dot3StatsInternalMacTransmitErrors++;
		return;
	}
#endif

	/*
	 * Put the length word for this frame.
	 * Does 86960 accept odd length?  -- Yes.
	 * Do we need to pad the length to minimum size by ourselves?
	 * -- Generally yes.  But for (or will be) the last
	 * packet in the transmission buffer, we can skip the
	 * padding process.  It may gain performance slightly.  FIXME.
	 */
#ifdef FE_8BIT_SUPPORT
	if ((sc->proto_dlcr6 & FE_D6_SBW) == FE_D6_SBW_BYTE)
	{
		len = max( length, ETHER_MIN_LEN - ETHER_CRC_LEN );
		outb( addr_bmpr8,   len & 0x00ff );
		outb( addr_bmpr8, ( len & 0xff00 ) >> 8 );
	}
	else
#endif
	{
		outw( addr_bmpr8, max( length, ETHER_MIN_LEN - ETHER_CRC_LEN ) );
	}

	/*
	 * Update buffer status now.
	 * Truncate the length up to an even number, since we use outw().
	 */
#ifdef FE_8BIT_SUPPORT
	if ((sc->proto_dlcr6 & FE_D6_SBW) != FE_D6_SBW_BYTE)
#endif
	{
		length = ( length + 1 ) & ~1;
	}
	sc->txb_free -= FE_DATA_LEN_LEN + max( length, ETHER_MIN_LEN - ETHER_CRC_LEN);
	sc->txb_count++;

	/*
	 * Transfer the data from mbuf chain to the transmission buffer.
	 * MB86960 seems to require that data be transferred as words, and
	 * only words.  So that we require some extra code to patch
	 * over odd-length mbufs.
	 */
#ifdef FE_8BIT_SUPPORT
	if ((sc->proto_dlcr6 & FE_D6_SBW) == FE_D6_SBW_BYTE)
	{
		/* 8-bit cards are easy.  */
		for ( mp = m; mp != 0; mp = mp->m_next ) {
			if ( mp->m_len ) {
				outsb( addr_bmpr8, mtod(mp, caddr_t), mp->m_len );
			}
		}
	}
	else
#endif
	{
		/* 16-bit cards are a pain.  */
		savebyte = NO_PENDING_BYTE;
		for ( mp = m; mp != 0; mp = mp->m_next ) {

			/* Ignore empty mbuf.  */
			len = mp->m_len;
			if ( len == 0 ) continue;

			/* Find the actual data to send.  */
			data = mtod(mp, caddr_t);

			/* Finish the last byte.  */
			if ( savebyte != NO_PENDING_BYTE ) {
				outw( addr_bmpr8, savebyte | ( *data << 8 ) );
				data++;
				len--;
				savebyte = NO_PENDING_BYTE;
			}

			/* output contiguous words */
			if (len > 1) {
				outsw( addr_bmpr8, data, len >> 1);
				data += len & ~1;
				len &= 1;
			}

			/* Save a remaining byte, if there is one.  */
			if ( len > 0 ) {
				savebyte = *data;
			}
		}

		/* Spit the last byte, if the length is odd.  */
		if ( savebyte != NO_PENDING_BYTE ) {
			outw( addr_bmpr8, savebyte );
		}
	}

	/* Pad to the Ethernet minimum length, if the packet is too short.  */
	if ( length < ETHER_MIN_LEN - ETHER_CRC_LEN ) {
#ifdef FE_8BIT_SUPPORT
		if ((sc->proto_dlcr6 & FE_D6_SBW) == FE_D6_SBW_BYTE)
		{
			outsb( addr_bmpr8, padding,   ETHER_MIN_LEN - ETHER_CRC_LEN - length );
		}
		else
#endif
		{
			outsw( addr_bmpr8, padding, ( ETHER_MIN_LEN - ETHER_CRC_LEN - length ) >> 1);
		}
	}
}

/*
 * Compute hash value for an Ethernet address
 */
static int
fe_hash ( u_char * ep )
{
#define FE_HASH_MAGIC_NUMBER 0xEDB88320L

	u_long hash = 0xFFFFFFFFL;
	int i, j;
	u_char b;
	u_long m;

	for ( i = ETHER_ADDR_LEN; --i >= 0; ) {
		b = *ep++;
		for ( j = 8; --j >= 0; ) {
			m = hash;
			hash >>= 1;
			if ( ( m ^ b ) & 1 ) hash ^= FE_HASH_MAGIC_NUMBER;
			b >>= 1;
		}
	}
	return ( ( int )( hash >> 26 ) );
}

/*
 * Compute the multicast address filter from the
 * list of multicast addresses we need to listen to.
 */
static struct fe_filter
fe_mcaf ( struct fe_softc *sc )
{
	int index;
	struct fe_filter filter;
	struct ifmultiaddr *ifma;

	filter = fe_filter_nothing;
	for (ifma = sc->arpcom.ac_if.if_multiaddrs.lh_first; ifma;
	     ifma = ifma->ifma_link.le_next) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		index = fe_hash(LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
#ifdef FE_DEBUG
		printf("fe%d: hash(%6D) == %d\n",
			sc->sc_unit, enm->enm_addrlo , ":", index);
#endif

		filter.data[index >> 3] |= 1 << (index & 7);
	}
	return ( filter );
}

/*
 * Calculate a new "multicast packet filter" and put the 86960
 * receiver in appropriate mode.
 */
static void
fe_setmode ( struct fe_softc *sc )
{
	int flags = sc->sc_if.if_flags;

	/*
	 * If the interface is not running, we postpone the update
	 * process for receive modes and multicast address filter
	 * until the interface is restarted.  It reduces some
	 * complicated job on maintaining chip states.  (Earlier versions
	 * of this driver had a bug on that point...)
	 *
	 * To complete the trick, fe_init() calls fe_setmode() after
	 * restarting the interface.
	 */
	if ( !( flags & IFF_RUNNING ) ) return;

	/*
	 * Promiscuous mode is handled separately.
	 */
	if ( flags & IFF_PROMISC ) {
		/*
		 * Program 86960 to receive all packets on the segment
		 * including those directed to other stations.
		 * Multicast filter stored in MARs are ignored
		 * under this setting, so we don't need to update it.
		 *
		 * Promiscuous mode in FreeBSD 2 is used solely by
		 * BPF, and BPF only listens to valid (no error) packets.
		 * So, we ignore erroneous ones even in this mode.
		 * (Older versions of fe driver mistook the point.)
		 */
		outb( sc->ioaddr[ FE_DLCR5 ],
			sc->proto_dlcr5 | FE_D5_AFM0 | FE_D5_AFM1 );
		sc->filter_change = 0;
		return;
	}

	/*
	 * Turn the chip to the normal (non-promiscuous) mode.
	 */
	outb( sc->ioaddr[ FE_DLCR5 ], sc->proto_dlcr5 | FE_D5_AFM1 );

	/*
	 * Find the new multicast filter value.
	 */
	if ( flags & IFF_ALLMULTI ) {
		sc->filter = fe_filter_all;
	} else {
		sc->filter = fe_mcaf( sc );
	}
	sc->filter_change = 1;

	/*
	 * We have to update the multicast filter in the 86960, A.S.A.P.
	 *
	 * Note that the DLC (Data Link Control unit, i.e. transmitter
	 * and receiver) must be stopped when feeding the filter, and
	 * DLC trashes all packets in both transmission and receive
	 * buffers when stopped.
	 *
	 * To reduce the packet loss, we delay the filter update
	 * process until buffers are empty.
	 */
	if ( sc->txb_sched == 0 && sc->txb_count == 0
          && !( inb( sc->ioaddr[ FE_DLCR1 ] ) & FE_D1_PKTRDY ) ) {
		/*
		 * Buffers are (apparently) empty.  Load
		 * the new filter value into MARs now.
		 */
		fe_loadmar(sc);
	} else {
		/*
		 * Buffers are not empty.  Mark that we have to update
		 * the MARs.  The new filter will be loaded by feintr()
		 * later.
		 */
	}
}

/*
 * Load a new multicast address filter into MARs.
 *
 * The caller must have splimp'ed before fe_loadmar.
 * This function starts the DLC upon return.  So it can be called only
 * when the chip is working, i.e., from the driver's point of view, when
 * a device is RUNNING.  (I mistook the point in previous versions.)
 */
static void
fe_loadmar ( struct fe_softc * sc )
{
	/* Stop the DLC (transmitter and receiver).  */
	DELAY( 200 );
	outb( sc->ioaddr[ FE_DLCR6 ], sc->proto_dlcr6 | FE_D6_DLC_DISABLE );
	DELAY( 200 );

	/* Select register bank 1 for MARs.  */
	outb( sc->ioaddr[ FE_DLCR7 ],
		sc->proto_dlcr7 | FE_D7_RBS_MAR | FE_D7_POWER_UP );

	/* Copy filter value into the registers.  */
	outblk( sc, FE_MAR8, sc->filter.data, FE_FILTER_LEN );

	/* Restore the bank selection for BMPRs (i.e., runtime registers).  */
	outb( sc->ioaddr[ FE_DLCR7 ],
		sc->proto_dlcr7 | FE_D7_RBS_BMPR | FE_D7_POWER_UP );

	/* Restart the DLC.  */
	DELAY( 200 );
	outb( sc->ioaddr[ FE_DLCR6 ], sc->proto_dlcr6 | FE_D6_DLC_ENABLE );
	DELAY( 200 );

	/* We have just updated the filter.  */
	sc->filter_change = 0;
}

/* Change the media selection.  */
static int
fe_medchange (struct ifnet *ifp)
{
	struct fe_softc *sc = (struct fe_softc *)ifp->if_softc;

#ifdef DIAGNOSTIC
	/* If_media should not pass any request for a media which this
	   interface doesn't support.  */
	int b;

	for (b = 0; bit2media[b] != 0; b++) {
		if (bit2media[b] == sc->media.ifm_media) break;
	}
	if (((1 << b) & sc->mbitmap) == 0) {
		printf("fe%d: got an unsupported media request (0x%x)\n",
		       sc->sc_unit, sc->media.ifm_media);
		return EINVAL;
	}
#endif

	/* We don't actually change media when the interface is down.
	   fe_init() will do the job, instead.  Should we also wait
	   until the transmission buffer being empty?  Changing the
	   media when we are sending a frame will cause two garbages
	   on wires, one on old media and another on new.  FIXME */
	if (sc->sc_if.if_flags & IFF_UP) {
		if (sc->msel) sc->msel(sc);
	}

	return 0;
}

/* I don't know how I can support media status callback... FIXME.  */
static void
fe_medstat (struct ifnet *ifp, struct ifmediareq *ifmr)
{
	(void)ifp;
	(void)ifmr;
}
