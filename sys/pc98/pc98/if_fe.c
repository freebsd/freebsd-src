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
 * $Id: if_fe.c,v 1.26 1997/11/07 12:53:59 kato Exp $
 *
 * Device driver for Fujitsu MB86960A/MB86965A based Ethernet cards.
 * To be used with FreeBSD 2.x
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
 * This version also includes some alignments for
 * RE1000/RE1000+/ME1500 support.  It is incomplete, however, since the
 * cards are not for AT-compatibles.  (They are for PC98 bus -- a
 * proprietary bus architecture available only in Japan.)  Further
 * work for PC98 version will be available as a part of FreeBSD(98)
 * project.
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
 * Modified for Allied-Telesis RE1000 series.
 */


/*
 * TODO:
 *  o   To support MBH10304 PC card.  It is another MB8696x based
 *      PCMCIA Ethernet card by Fujitsu, which is not compatible with
 *      MBH10302.
 *  o   To merge FreeBSD(98) efforts into a single source file.
 *  o   To support ISA PnP auto configuration for FMV-183/184.
 *  o   To reconsider mbuf usage.
 *  o   To reconsider transmission buffer usage, including
 *      transmission buffer size (currently 4KB x 2) and pros-and-
 *      cons of multiple frame transmission.
 *  o   To test IPX codes.
 */

#include "fe.h"
#include "bpfilter.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>

#include <sys/conf.h>

#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_dl.h>

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

#include <machine/clock.h>

#include <i386/isa/isa_device.h>
#include <i386/isa/icu.h>

/* PCCARD suport */
#include "card.h"
#if NCARD > 0
#include <sys/select.h>
#include <pccard/cardinfo.h>
#include <pccard/slot.h>
#include <pccard/driver.h>
#endif

#include <i386/isa/ic/mb86960.h>
#include <i386/isa/if_fereg.h>

/*
 * This version of fe is an ISA device driver.
 * Override the following macro to adapt it to another bus.
 * (E.g., PC98.)
 */
#define DEVICE	struct isa_device

/*
 * Default settings for fe driver specific options.
 * They can be set in config file by "options" statements.
 */

/*
 * Debug control.
 * 0: No debug at all.  All debug specific codes are stripped off.
 * 1: Silent.  No debug messages are logged except emergent ones.
 * 2: Brief.  Lair events and/or important information are logged.
 * 3: Detailed.  Logs all information which *may* be useful for debugging.
 * 4: Trace.  All actions in the driver is logged.  Super verbose.
 */
#ifndef FE_DEBUG
#define FE_DEBUG	1
#endif

/*
 * Transmit just one packet per a "send" command to 86960.
 * This option is intended for performance test.  An EXPERIMENTAL option.
 */
#ifndef FE_SINGLE_TRANSMISSION
#define FE_SINGLE_TRANSMISSION 0
#endif

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
	char * typestr;		/* printable name of the interface.  */
	u_short iobase;		/* base I/O address of the adapter.  */
	u_short ioaddr [ MAXREGISTERS ]; /* I/O addresses of register.  */
	u_short txb_size;	/* size of TX buffer, in bytes  */
	u_char proto_dlcr4;	/* DLCR4 prototype.  */
	u_char proto_dlcr5;	/* DLCR5 prototype.  */
	u_char proto_dlcr6;	/* DLCR6 prototype.  */
	u_char proto_dlcr7;	/* DLCR7 prototype.  */
	u_char proto_bmpr13;	/* BMPR13 prototype.  */
	u_char proto_bmpr14;	/* BMPR14 prototype.  */

	/* Vendor specific hooks.  */
	void ( * init )( struct fe_softc * ); /* Just before fe_init().  */
	void ( * stop )( struct fe_softc * ); /* Just after fe_stop().  */

	/* Transmission buffer management.  */
	u_short txb_free;	/* free bytes in TX buffer  */
	u_char txb_count;	/* number of packets in TX buffer  */
	u_char txb_sched;	/* number of scheduled packets  */

	/* Excessive collision counter (see fe_tint() for details.  */
	u_char tx_excolls;	/* # of excessive collisions.  */

	/* Multicast address filter management.  */
	u_char filter_change;	/* MARs must be changed ASAP. */
	struct fe_filter filter;/* new filter value.  */

}       fe_softc[NFE];

#define sc_if		arpcom.ac_if
#define sc_unit		arpcom.ac_if.if_unit
#define sc_enaddr	arpcom.ac_enaddr

/* Standard driver entry points.  These can be static.  */
static int		fe_probe	( struct isa_device * );
static int		fe_attach	( struct isa_device * );
static void		fe_init		( int );
static int		fe_ioctl	( struct ifnet *, int, caddr_t );
static void		fe_start	( struct ifnet * );
static void		fe_reset	( int );
static void		fe_watchdog	( struct ifnet * );

/* Local functions.  Order of declaration is confused.  FIXME.  */
#ifdef PC98
static int	fe_probe_re1000	( DEVICE *, struct fe_softc * );
static int	fe_probe_re1000p( DEVICE *, struct fe_softc * );
static int	fe_probe_cnet9ne ( DEVICE *, struct fe_softc * );
static int	fe_probe_cnet98p2( DEVICE *, struct fe_softc * );
#else
static int	fe_probe_fmv	( DEVICE *, struct fe_softc * );
static int	fe_probe_ati	( DEVICE *, struct fe_softc * );
static void	fe_init_ati	( struct fe_softc * );
#endif /* PC98 */
static int	fe_probe_gwy	( DEVICE *, struct fe_softc * );
#if NCARD > 0
static int	fe_probe_mbh	( DEVICE *, struct fe_softc * );
static void	fe_init_mbh	( struct fe_softc * );
static int	fe_probe_tdk	( DEVICE *, struct fe_softc * );
#endif
static int	fe_get_packet	( struct fe_softc *, u_short );
static void	fe_stop		( int );
static void	fe_tint		( struct fe_softc *, u_char );
static void	fe_rint		( struct fe_softc *, u_char );
static void	fe_xmit		( struct fe_softc * );
static void	fe_emptybuffer	( struct fe_softc * );
static void	fe_write_mbufs	( struct fe_softc *, struct mbuf * );
static struct fe_filter
		fe_mcaf		( struct fe_softc * );
static int	fe_hash		( u_char * );
static void	fe_setmode	( struct fe_softc * );
static void	fe_loadmar	( struct fe_softc * );
#if FE_DEBUG >= 1
static void	fe_dump		( int, struct fe_softc *, char * );
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
static int feinit(struct pccard_devinfo *);		/* init device */
static void feunload(struct pccard_devinfo *);		/* Disable driver */
static int fe_card_intr(struct pccard_devinfo *);	/* Interrupt handler */

static struct pccard_device fe_info = {
	"fe",
	feinit,
	feunload,
	fe_card_intr,
	0,			/* Attributes - presently unused */
	&net_imask		/* Interrupt mask for device */
				/* XXX - Should this also include net_imask? */
};

DATA_SET(pccarddrv_set, fe_info);

/*
 *	Initialize the device - called from Slot manager.
 */
static int
feinit(struct pccard_devinfo *devi)
{
        struct fe_softc *sc;

	/* validate unit number. */
	if (devi->isahd.id_unit >= NFE)
		return (ENODEV);
	/*
	 * Probe the device. If a value is returned,
	 * the device was found at the location.
	 */
#if FE_DEBUG >= 2
	printf("Start Probe\n");
#endif
	/* Initialize "minimum" parts of our softc.  */
	sc = &fe_softc[devi->isahd.id_unit];
	sc->sc_unit = devi->isahd.id_unit;
	sc->iobase = devi->isahd.id_iobase;

	/* Use Ethernet address got from CIS, if one is available.  */
	if ((devi->misc[0] & 0x03) == 0x00
	    && (devi->misc[0] | devi->misc[1] | devi->misc[2]) != 0) {
	       /* Yes, it looks like a valid Ether address.  */
		bcopy(devi->misc, sc->sc_enaddr, ETHER_ADDR_LEN);
	} else {
		/* Indicate we have no Ether address in CIS.  */
		bzero(sc->sc_enaddr, ETHER_ADDR_LEN);
	}

	/* Probe supported PC card models.  */
	if (fe_probe_tdk(&devi->isahd, sc) == 0 &&
	    fe_probe_mbh(&devi->isahd, sc) == 0)
		return (ENXIO);
#if FE_DEBUG >= 2
	printf("Start attach\n");
#endif
	if (fe_attach(&devi->isahd) == 0)
		return (ENXIO);

	return (0);
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
	printf("fe%d: unload\n", devi->isahd.id_unit);
	fe_stop(devi->isahd.id_unit);
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
 */

/* How and where to probe; to support automatic I/O address detection.  */
struct fe_probe_list
{
	int ( * probe ) ( DEVICE *, struct fe_softc * );
	u_short const * addresses;
};

/* Lists of possible addresses.  */
#ifdef PC98
static u_short const fe_re1000_addr [] =
	{ 0x0D0, 0x0D2, 0x0D4, 0x0D6, 0x0D8, 0x0DA, 0x0DC, 0x0DE,
	  0x1D0, 0x1D2, 0x1D4, 0x1D6, 0x1D8, 0x1DA, 0x1DC, 0x1DE, 0 };
static u_short const fe_re1000p_addr [] =
	{ 0x0D0, 0x0D2, 0x0D4, 0x0D8, 0x1D4, 0x1D6, 0x1D8, 0x1DA, 0 };
static u_short const fe_cnet9ne_addr [] =
	{ 0x73D0, 0 };
static u_short const fe_cnet98p2_addr [] =
	{ 0x03D0, 0x13D0, 0x23D0, 0x33D0, 0x43D0, 0x53D0, 0x63D0,
	  0x73D0, 0x83D0, 0x93D0, 0xA3D0, 0xB3D0, 0xC3D0, 0xD3D0, 0 };
#else
static u_short const fe_fmv_addr [] =
	{ 0x220, 0x240, 0x260, 0x280, 0x2A0, 0x2C0, 0x300, 0x340, 0 };
static u_short const fe_ati_addr [] =
	{ 0x240, 0x260, 0x280, 0x2A0, 0x300, 0x320, 0x340, 0x380, 0 };
#endif

static struct fe_probe_list const fe_probe_list [] =
{
#ifdef PC98
	{ fe_probe_re1000, fe_re1000_addr },
	{ fe_probe_re1000p, fe_re1000p_addr },
	/* XXX: We must probe C-NET(98)P2 after C-NET(9N)E. */
	{ fe_probe_cnet9ne, fe_cnet9ne_addr },
	{ fe_probe_cnet98p2, fe_cnet98p2_addr },
#else
	{ fe_probe_fmv, fe_fmv_addr },
	{ fe_probe_ati, fe_ati_addr },
#endif
	{ fe_probe_gwy, NULL },		/* GWYs cannot be auto detected.  */
	{ NULL, NULL }
};


/*
 * Determine if the device is present
 *
 *   on entry:
 * 	a pointer to an isa_device struct
 *   on exit:
 *	zero if device not found
 *	or number of i/o addresses used (if found)
 */

static int
fe_probe ( DEVICE * dev )
{
	struct fe_softc * sc;
	int u;
	int nports;
	struct fe_probe_list const * list;
	u_short const * addr;
	u_short single [ 2 ];

	/* Initialize "minimum" parts of our softc.  */
	sc = &fe_softc[ dev->id_unit ];
	sc->sc_unit = dev->id_unit;

	/* TODO: Should be in each probe routines */
	sc->proto_bmpr14 = 0;

	/* Probe each possibility, one at a time.  */
	for ( list = fe_probe_list; list->probe != NULL; list++ ) {

		if ( dev->id_iobase != NO_IOADDR ) {
			/* Probe one specific address.  */
			single[ 0 ] = dev->id_iobase;
			single[ 1 ] = 0;
			addr = single;
		} else if ( list->addresses != NULL ) {
			/* Auto detect.  */
			addr = list->addresses;
		} else {
			/* We need a list of addresses to do auto detect.  */
			continue;
		}

		/* Probe all possible addresses for the board.  */
		while ( *addr != 0 ) {

			/* See if the address is already in use.  */
			for ( u = 0; u < NFE; u++ ) {
				if ( fe_softc[u].iobase == *addr ) break;
			}

#if FE_DEBUG >= 3
			if ( u == NFE ) {
			    log( LOG_INFO, "fe%d: probing %d at 0x%x\n",
				sc->sc_unit, list - fe_probe_list, *addr );
			} else if ( u == sc->sc_unit ) {
			    log( LOG_INFO, "fe%d: re-probing %d at 0x%x?\n",
				sc->sc_unit, list - fe_probe_list, *addr );
			} else {
			    log( LOG_INFO, "fe%d: skipping %d at 0x%x\n",
				sc->sc_unit, list - fe_probe_list, *addr );
			}
#endif

			/* Probe the address if it is free.  */
			if ( u == NFE || u == sc->sc_unit ) {

				/* Probe an address.  */
				sc->iobase = *addr;
				nports = list->probe( dev, sc );
				if ( nports > 0 ) {
				    /* Found.  */
				    dev->id_iobase = *addr;
				    return ( nports );
				}
				sc->iobase = 0;
			}

			/* Try next.  */
			addr++;
		}
	}

	/* Probe failed.  */
	return ( 0 );
}

/*
 * Check for specific bits in specific registers have specific values.
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
#if FE_DEBUG >=2
		printf("Probe Port:%x,Value:%x,Mask:%x.Bits:%x\n",
			p->port,inb(sc->ioaddr[ p->port]),p->mask,p->bits);
#endif
		if ( ( inb( sc->ioaddr[ p->port ] ) & p->mask ) != p->bits ) 
		{
			return ( 0 );
		}
	}
	return ( 1 );
}

/*
 * Routines to read all bytes from the config EEPROM through MB86965A.
 * I'm not sure what exactly I'm doing here...  I was told just to follow
 * the steps, and it worked.  Could someone tell me why the following
 * code works?  (Or, why all similar codes I tried previously doesn't
 * work.)  FIXME.
 */

static void
fe_strobe_eeprom ( u_short bmpr16 )
{
	/*
	 * We must guarantee 800ns (or more) interval to access slow
	 * EEPROMs.  The following redundant code provides enough
	 * delay with ISA timing.  (Even if the bus clock is "tuned.")
	 * Some modification will be needed on faster busses.
	 */
	outb( bmpr16, FE_B16_SELECT );
	outb( bmpr16, FE_B16_SELECT );
	outb( bmpr16, FE_B16_SELECT | FE_B16_CLOCK );
	outb( bmpr16, FE_B16_SELECT | FE_B16_CLOCK );
	outb( bmpr16, FE_B16_SELECT );
	outb( bmpr16, FE_B16_SELECT );
}

static void
fe_read_eeprom ( struct fe_softc * sc, u_char * data )
{
	u_short bmpr16 = sc->ioaddr[ FE_BMPR16 ];
	u_short bmpr17 = sc->ioaddr[ FE_BMPR17 ];
	u_char n, val, bit;

	/* Read bytes from EEPROM; two bytes per an iteration.  */
	for ( n = 0; n < FE_EEPROM_SIZE / 2; n++ ) {

		/* Reset the EEPROM interface.  */
		outb( bmpr16, 0x00 );
		outb( bmpr17, 0x00 );

		/* Start EEPROM access.  */
		outb( bmpr16, FE_B16_SELECT );
		outb( bmpr17, FE_B17_DATA );
		fe_strobe_eeprom( bmpr16 );

		/* Pass the iteration count to the chip.  */
		val = 0x80 | n;
		for ( bit = 0x80; bit != 0x00; bit >>= 1 ) {
			outb( bmpr17, ( val & bit ) ? FE_B17_DATA : 0 );
			fe_strobe_eeprom( bmpr16 );
		}
		outb( bmpr17, 0x00 );

		/* Read a byte.  */
		val = 0;
		for ( bit = 0x80; bit != 0x00; bit >>= 1 ) {
			fe_strobe_eeprom( bmpr16 );
			if ( inb( bmpr17 ) & FE_B17_DATA ) {
				val |= bit;
			}
		}
		*data++ = val;

		/* Read one more byte.  */
		val = 0;
		for ( bit = 0x80; bit != 0x00; bit >>= 1 ) {
			fe_strobe_eeprom( bmpr16 );
			if ( inb( bmpr17 ) & FE_B17_DATA ) {
				val |= bit;
			}
		}
		*data++ = val;
	}

	/* Reset the EEPROM interface, again.  */
	outb( bmpr16, 0x00 );
	outb( bmpr17, 0x00 );

#if FE_DEBUG >= 3
	/* Report what we got.  */
	data -= FE_EEPROM_SIZE;
	log( LOG_INFO, "fe%d: EEPROM:"
		" %02x%02x%02x%02x %02x%02x%02x%02x -"
		" %02x%02x%02x%02x %02x%02x%02x%02x -"
		" %02x%02x%02x%02x %02x%02x%02x%02x -"
		" %02x%02x%02x%02x %02x%02x%02x%02x\n",
		sc->sc_unit,
		data[ 0], data[ 1], data[ 2], data[ 3],
		data[ 4], data[ 5], data[ 6], data[ 7],
		data[ 8], data[ 9], data[10], data[11],
		data[12], data[13], data[14], data[15],
		data[16], data[17], data[18], data[19],
		data[20], data[21], data[22], data[23],
		data[24], data[25], data[26], data[27],
		data[28], data[29], data[30], data[31] );
#endif
}

/*
 * Hardware (vendor) specific probe routines.
 */

#ifdef PC98
/*
 * Probe and initialization for Allied-Telesis RE1000 series.
 */
static int
fe_probe_re1000 ( DEVICE * isa_dev, struct fe_softc * sc )
{
	int i, n;
	int dlcr6, dlcr7;
	u_char c = 0;

	static u_short const irqmap [ 4 ] =
		{ IRQ3,  IRQ5,  IRQ6,  IRQ12 };

#if FE_DEBUG >= 3
	log( LOG_INFO, "fe%d: probe (0x%x) for RE1000\n", sc->sc_unit, sc->iobase );
	fe_dump( LOG_INFO, sc, NULL );
#endif

	/* Setup an I/O address mapping table.  */
	for ( i = 0; i < MAXREGISTERS; i++ ) {
		sc->ioaddr[ i ] = sc->iobase + (i/2)*0x200 + (i%2);
	}

	/*
	 * RE1000 does not use 86965 EEPROM interface.
	 */
	c ^= sc->sc_enaddr[0] = inb(sc->ioaddr[FE_RE1000_MAC0]);
	c ^= sc->sc_enaddr[1] = inb(sc->ioaddr[FE_RE1000_MAC1]);
	c ^= sc->sc_enaddr[2] = inb(sc->ioaddr[FE_RE1000_MAC2]);
	c ^= sc->sc_enaddr[3] = inb(sc->ioaddr[FE_RE1000_MAC3]);
	c ^= sc->sc_enaddr[4] = inb(sc->ioaddr[FE_RE1000_MAC4]);
	c ^= sc->sc_enaddr[5] = inb(sc->ioaddr[FE_RE1000_MAC5]);
	c ^= inb(sc->ioaddr[FE_RE1000_MACCHK]);
	if (c != 0) return 0;

	if ( sc->sc_enaddr[ 0 ] != 0x00
		|| sc->sc_enaddr[ 1 ] != 0x00
		|| sc->sc_enaddr[ 2 ] != 0xF4 ) return 0;

	/*
	 * check interrupt configure
	 */
	for (n=0; n<4; n++) {
		if (isa_dev->id_irq == irqmap[n]) break;
	}
	if (n == 4) return 0;

	/*
	 * set irq
	 */
	c = inb(sc->ioaddr[FE_RE1000_IRQCONF]);
	c &= (~ FE_RE1000_IRQCONF_IRQ);
	c |= (1 << (n + FE_RE1000_IRQCONF_IRQSHIFT));
	outb(sc->ioaddr[FE_RE1000_IRQCONF], c);

	sc->typestr = "RE1000";

	/*
	 * Program the 86965 as follows:
	 *	SRAM: 32KB, 100ns, byte-wide access.
	 *	Transmission buffer: 4KB x 2.
	 *	System bus interface: 16 bits.
	 */
	sc->proto_dlcr4 = FE_D4_LBC_DISABLE | FE_D4_CNTRL;  /* FIXME */
	sc->proto_dlcr5 = 0;
	sc->proto_dlcr6 = FE_D6_BUFSIZ_32KB | FE_D6_TXBSIZ_2x4KB
		| FE_D6_BBW_BYTE | FE_D6_SBW_WORD | FE_D6_SRAM_100ns;
	sc->proto_dlcr7 = FE_D7_BYTSWP_LH | FE_D7_IDENT_EC;
	sc->proto_bmpr13 = FE_B13_TPTYPE_UTP | FE_B13_PORT_AUTO;

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, "RE1000 found" );
#endif

	/* Initialize 86965.  */
	outb( sc->ioaddr[FE_DLCR6], sc->proto_dlcr6 | FE_D6_DLC_DISABLE );
	DELAY(200);

	/* Disable all interrupts.  */
	outb( sc->ioaddr[FE_DLCR2], 0 );
	outb( sc->ioaddr[FE_DLCR3], 0 );

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, "end of fe_probe_re1000()" );
#endif

	/*
	 * That's all.  RE1000 occupies 2*16 I/O addresses, by the way.
	 */
	return 2;	/* ??? */
}

/*
 * Probe and initialization for Allied-Telesis RE1000Plus/ME1500 series.
 */
static int
fe_probe_re1000p ( DEVICE * isa_dev, struct fe_softc * sc )
{
	int i, n, signature;
	int dlcr6, dlcr7;
	u_char eeprom [ FE_EEPROM_SIZE ];

	static u_short const irqmap [ 4 ] =
		{ IRQ3,  IRQ5,  IRQ6,  IRQ12 };
	static struct fe_simple_probe_struct const probe_signature1 [] = {
		{ FE_DLCR0,  0xBF, 0x00 },
		{ FE_DLCR2,  0xFF, 0x00 },
		{ FE_DLCR4,  0x0F, 0x06 },
		{ FE_DLCR6,  0x0F, 0x06 },
		{ 0 }
	};
	static struct fe_simple_probe_struct const probe_signature2 [] = {
		{ FE_DLCR1,  0xFF, 0x00 },
		{ FE_DLCR3,  0xFF, 0x00 },
		{ FE_DLCR5,  0xFF, 0x41 },
		{ 0 }
	};
	static struct fe_simple_probe_struct const probe_table [] = {
		{ FE_DLCR2,  0x71, 0x00 },
		{ FE_DLCR4,  0x08, 0x00 },
		{ FE_DLCR5,  0x80, 0x00 },
		{ 0 }
	};
	static struct fe_simple_probe_struct const vendor_code [] = {
		{ FE_DLCR8,  0xFF, 0x00 },
		{ FE_DLCR9,  0xFF, 0x00 },
		{ FE_DLCR10,  0xFF, 0xF4 },
		{ 0 }
	};

#if FE_DEBUG >= 3
	log( LOG_INFO, "fe%d: probe (0x%x) for RE1000Plus/ME1500\n", sc->sc_unit, sc->iobase );
	fe_dump( LOG_INFO, sc, NULL );
#endif

	/* Setup an I/O address mapping table.  */
	for ( i = 0; i < 16; i++ ) {
		sc->ioaddr[ i ] = sc->iobase + (i/2)*0x200 + (i%2);
	}
	for ( i = 16; i < MAXREGISTERS; i++ ) {
		sc->ioaddr[ i ] = sc->iobase + i*0x200 - 0x1000;
	}

	/* First, check the "signature" */
	signature = 0;
	if (fe_simple_probe(sc, probe_signature1)) {
		outb(sc->ioaddr[FE_DLCR6], (inb(sc->ioaddr[FE_DLCR6]) & 0xCF) | 0x16);
		if (fe_simple_probe(sc, probe_signature2))
			signature = 1;
	}

	/*
	 * If the "signature" not detected, 86965 *might* be previously
	 * initialized. So, check the Ethernet address here.
	 *
	 * Allied-Telesis uses 00 00 F4 ?? ?? ??.
	 */
	if (signature == 0) {
		/* Simple check */
		if (!fe_simple_probe(sc, probe_table)) return 0;

		/* Disable DLC */
		dlcr6 = inb(sc->ioaddr[FE_DLCR6]);
		outb(sc->ioaddr[FE_DLCR6], dlcr6 | FE_D6_DLC_DISABLE);
		/* Select register bank for DLCR */
		dlcr7 = inb(sc->ioaddr[FE_DLCR7]);
		outb(sc->ioaddr[FE_DLCR7], dlcr7 & 0xF3 | FE_D7_RBS_DLCR);

		/* Check the Ethernet address */
		if (!fe_simple_probe(sc, vendor_code)) return 0;

		/* Restore configuration registers */
		DELAY(200);
		outb(sc->ioaddr[FE_DLCR6], dlcr6);
		outb(sc->ioaddr[FE_DLCR7], dlcr7);
	}

	/*
	 * We are now almost sure we have an 86965 at the given
	 * address.  So, read EEPROM through 86965.  We have to write
	 * into LSI registers to read from EEPROM.  I want to avoid it
	 * at this stage, but I cannot test the presense of the chip
	 * any further without reading EEPROM.  FIXME.
	 */
	fe_read_eeprom( sc, eeprom );

	/* Make sure that config info in EEPROM and 86965 agree.  */
	if ( eeprom[ FE_EEPROM_CONF ] != inb( sc->ioaddr[FE_BMPR19] ) ) {
		return 0;
	}

	/*
	 * Initialize constants in the per-line structure.
	 */
	    
	/* Get our station address from EEPROM.  */
	bcopy( eeprom + FE_ATI_EEP_ADDR, sc->sc_enaddr, ETHER_ADDR_LEN );

	sc->typestr = "RE1000Plus/ME1500";

	/*
	 * Read IRQ configuration.
	 */
	n = (inb(sc->ioaddr[FE_BMPR19]) & FE_B19_IRQ ) >> FE_B19_IRQ_SHIFT;
	isa_dev->id_irq = irqmap[n];

	/*
	 * Program the 86965 as follows:
	 *	SRAM: 32KB, 100ns, byte-wide access.
	 *	Transmission buffer: 4KB x 2.
	 *	System bus interface: 16 bits.
	 */
	sc->proto_dlcr4 = FE_D4_LBC_DISABLE | FE_D4_CNTRL;  /* FIXME */
	sc->proto_dlcr5 = 0;
	sc->proto_dlcr6 = FE_D6_BUFSIZ_32KB | FE_D6_TXBSIZ_2x4KB
		| FE_D6_BBW_BYTE | FE_D6_SBW_WORD | FE_D6_SRAM_100ns;
	sc->proto_dlcr7 = FE_D7_BYTSWP_LH | FE_D7_IDENT_EC;
	sc->proto_bmpr13 = FE_B13_TPTYPE_UTP | FE_B13_PORT_AUTO;

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, "RE1000Plus/ME1500 found" );
#endif

	/* Initialize 86965.  */
	outb( sc->ioaddr[FE_DLCR6], sc->proto_dlcr6 | FE_D6_DLC_DISABLE );
	DELAY(200);

	/* Disable all interrupts.  */
	outb( sc->ioaddr[FE_DLCR2], 0 );
	outb( sc->ioaddr[FE_DLCR3], 0 );

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, "end of fe_probe_re1000p()" );
#endif

	/*
	 * That's all.  RE1000Plus/ME1500 occupies 2*16 I/O addresses, by the way.
	 */
	return 2;	/* ??? */
}

/*
 * Probe and initialization for Contec C-NET(9N)E series.
 */

/* TODO: Should be in "if_fereg.h" */
#define	FE_CNET9NE_INTR		0x10		/* Interrupt Mask? */
#define	FE_CNET9NE_MAC0		0x11		/* Station(MAC) address */
#define	FE_CNET9NE_MAC1		0x13
#define	FE_CNET9NE_MAC2		0x15
#define	FE_CNET9NE_MAC3		0x17
#define	FE_CNET9NE_MAC4		0x19
#define	FE_CNET9NE_MAC5		0x1B

/* TODO: Should be in "ic/mb86960.h" */
#define	FE_D7_ENDEC	0xC0	/* Encoder/Decoder mode(86960 only)	*/
#define	FE_D7_ENDEC_NORMAL_NICE		0x00	/* Normal NICE		*/
#define	FE_D7_ENDEC_NICE_MONITOR	0x40	/* NICE + Monitor	*/
#define	FE_D7_ENDEC_BYPASS		0x80	/* Encoder/Decoder Bypass */
#define	FE_D7_ENDEC_TEST		0xC0	/* Encoder/Decoder Test	*/

static int
fe_probe_cnet9ne ( DEVICE * isa_dev, struct fe_softc * sc )
{
	int	i;
	u_char	c;

#if FE_DEBUG >= 3
	log( LOG_INFO, "fe%d: probe (0x%x) for C-NET(9N)E\n", sc->sc_unit, sc->iobase );
#endif

	/* Setup an I/O address mapping table.  */
	for ( i = 0; i < 16; i++ ) {
		sc->ioaddr[i] = sc->iobase + i;
	}
	for ( ; i < MAXREGISTERS; i++ ) {
		sc->ioaddr[i] = sc->iobase + 0x400 - 16 + i;
	}

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, NULL );
#endif

	/* Get our station address from EEPROM. */
	sc->sc_enaddr[0] = inb( sc->ioaddr[FE_CNET9NE_MAC0] );
	sc->sc_enaddr[1] = inb( sc->ioaddr[FE_CNET9NE_MAC1] );
	sc->sc_enaddr[2] = inb( sc->ioaddr[FE_CNET9NE_MAC2] );
	sc->sc_enaddr[3] = inb( sc->ioaddr[FE_CNET9NE_MAC3] );
	sc->sc_enaddr[4] = inb( sc->ioaddr[FE_CNET9NE_MAC4] );
	sc->sc_enaddr[5] = inb( sc->ioaddr[FE_CNET9NE_MAC5] );

#if 1
	/*
	 * Check the Ethernet address here.
	 *
	 * Contec uses 00 80 4C ?? ?? ??.
	 */
	if ( sc->sc_enaddr[0] != (u_char)0x00
	||   sc->sc_enaddr[1] != (u_char)0x80
	||   sc->sc_enaddr[2] != (u_char)0x4C ) {
#else
	/*
	 * Make sure we got a valid Ethernet address.
	 */
	if ( ( sc->sc_enaddr[0] & 0x03 ) != 0x00	/* Multicast or Local address. */
	||   ( sc->sc_enaddr[0] | sc->sc_enaddr[1] | sc->sc_enaddr[2] ) == 0x00 ) {
#endif
#if FE_DEBUG >= 3
		log( LOG_INFO, "fe%d: invalid MAC adrs(%x:%x:%x:%x:%x:%x)\n"
		, sc->sc_unit
		, (u_char)sc->sc_enaddr[0], (u_char)sc->sc_enaddr[1]
		, (u_char)sc->sc_enaddr[2], (u_char)sc->sc_enaddr[3]
		, (u_char)sc->sc_enaddr[4], (u_char)sc->sc_enaddr[5] );
#endif
		return 0;
	}

	/* See if C-NET(9N)E is on its address. */
	if ( inb( sc->ioaddr[FE_DLCR6] ) == (u_char)0xff ) {
#if FE_DEBUG >= 3
		log( LOG_INFO, "fe%d: inb(%x) returns 0xff\n"
		, sc->sc_unit, sc->ioaddr[FE_DLCR6] );
#endif
		return 0;
	}

	sc->typestr = "C-NET9NE";

	/*
	 * Program the 86960 as follows:
	 *	SRAM: 64KB, word-wide access.
	 *	Transmission buffer: 4KB x 2.
	 *	System bus interface: 16 bits.
	 *	Encoder/Decoder mode: Normal NICE.
	 *
	 * 86960 manual says that SRAM access-time can't be configured.
	 * (must be 1)
	 */
	sc->proto_dlcr4 = FE_D4_LBC_DISABLE | FE_D4_CNTRL;
	sc->proto_dlcr5 = FE_D5_RMTRST;	/* reserved bit(must be 1) */
	sc->proto_dlcr6 = FE_D6_BUFSIZ_64KB | FE_D6_TXBSIZ_2x4KB
		| FE_D6_BBW_WORD | FE_D6_SBW_WORD | FE_D6_SRAM;
#ifndef	CNET9NC
	sc->proto_dlcr7 = FE_D7_BYTSWP_LH | FE_D7_ENDEC_NORMAL_NICE;
#else
	sc->proto_dlcr7 = FE_D7_BYTSWP_LH | FE_D7_ENDEC_BYPASS;
#endif
	sc->proto_bmpr13 = FE_B13_TPTYPE_UTP | FE_B13_PORT_AUTO;
	sc->proto_bmpr14 = 0;

	sc->stop = sc->init = NULL;

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, "C-NET(9N)E found" );
#endif

	/* Initialize 86960.  */
	outb( sc->ioaddr[FE_DLCR6], sc->proto_dlcr6 | FE_D6_DLC_DISABLE );
	DELAY( 200 );

#if 1	/* XXX: Is this really necessary?  FIXME. */
	c = inb( sc->ioaddr[FE_DLCR1] );
	if ( c == (u_char)0xff ) {
#if FE_DEBUG >= 3
		log( LOG_INFO, "fe%d: inb(%x) returns 0xff\n"
		, sc->sc_unit, sc->ioaddr[FE_DLCR1] );
#endif
		return 0;
	}
	if ( ( c & FE_D1_PKTRDY ) == 0 ) {
		outb( sc->ioaddr[FE_DLCR1], FE_D1_PKTRDY );
	}
#endif

	/* Disable all interrupts.  */
	outb( sc->ioaddr[FE_DLCR2], 0 );
	outb( sc->ioaddr[FE_DLCR3], 0 );

#ifndef	CNET9NC
	/* Enable interrupt?  FIXME. */
	outb( sc->ioaddr[FE_CNET9NE_INTR], 0x10 );
#endif

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, "end of fe_probe_cnet9ne()" );
#endif

	/*
	 * XXX: The I/O address range is fragmented in the CNET(9N)E.
	 *      "16" is the number of regs at iobase.
	 */
	return 16;
}

/*
 * Probe and initialization for Contec C-NET(98)P2 series.
 */

/*
 * Routines to read all bytes from the config EEPROM through TDK 78Q8377A.
 * I'm not sure what exactly I'm doing here...  I was told just to follow
 * the steps, and it worked.  Could someone tell me why the following
 * code works?  FIXME.
 */

static void
fe_strobe_eeprom_tdk ( u_short bmpr12 )
{
	outb( bmpr12, 0x10 );
	outb( bmpr12, 0x12 );
	outb( bmpr12, 0x12 );
	outb( bmpr12, 0x16 );
	outb( bmpr12, 0x12 | 0x01 );
	outb( bmpr12, 0x16 | 0x01 );
	outb( bmpr12, 0x12 | 0x01 );
	outb( bmpr12, 0x16 | 0x01 );
	outb( bmpr12, 0x12 );
	outb( bmpr12, 0x16 );
}

static void
fe_read_eeprom_tdk ( struct fe_softc * sc, u_char * data )
{
	u_short	bmpr12 = sc->ioaddr[FE_DLCR12];
	u_char	n, val, bit;

	outb( sc->ioaddr[FE_DLCR6], FE_D6_BBW_WORD | FE_D6_SBW_WORD
		| FE_D6_DLC_DISABLE );
	outb( sc->ioaddr[FE_DLCR7], FE_D7_BYTSWP_LH | FE_D7_RBS_BMPR
		| FE_D7_RDYPNS | FE_D7_POWER_UP );

	/* Read bytes from EEPROM; two bytes per an iteration.  */
	for ( n = 0; n < FE_EEPROM_SIZE / 2; n++ ) {

		/* Start EEPROM access.  */
		fe_strobe_eeprom_tdk( bmpr12 );

		/* Pass the iteration count to the chip.  */
		for ( bit = 0x80; bit != 0x00; bit >>= 1 ) {
			val = ( n & bit ) ? 0x01 : 0x00;
			outb( bmpr12, 0x12 | val );
			outb( bmpr12, 0x16 | val );
		}

		/* Read a byte.  */
		val = 0;
		for ( bit = 0x80; bit != 0x00; bit >>= 1 ) {
			outb( bmpr12, 0x12 );
			outb( bmpr12, 0x16 );
			if ( inb( bmpr12 ) & 0x01 ) {
				val |= bit;
			}
		}
		*data++ = val;

		/* Read one more byte.  */
		val = 0;
		for ( bit = 0x80; bit != 0x00; bit >>= 1 ) {
			outb( bmpr12, 0x12 );
			outb( bmpr12, 0x16 );
			if ( inb( bmpr12 ) & 0x01 ) {
				val |= bit;
			}
		}
		*data++ = val;

		outb( bmpr12, 0x10 );
	}

	/* Reset the EEPROM interface.  */
	outb( bmpr12, 0x00 );

#if FE_DEBUG >= 3
	/* Report what we got.  */
	data -= FE_EEPROM_SIZE;
	log( LOG_INFO, "fe%d: EEPROM:"
		" %02x%02x%02x%02x %02x%02x%02x%02x -"
		" %02x%02x%02x%02x %02x%02x%02x%02x -"
		" %02x%02x%02x%02x %02x%02x%02x%02x -"
		" %02x%02x%02x%02x %02x%02x%02x%02x\n",
		sc->sc_unit,
		data[ 0], data[ 1], data[ 2], data[ 3],
		data[ 4], data[ 5], data[ 6], data[ 7],
		data[ 8], data[ 9], data[10], data[11],
		data[12], data[13], data[14], data[15],
		data[16], data[17], data[18], data[19],
		data[20], data[21], data[22], data[23],
		data[24], data[25], data[26], data[27],
		data[28], data[29], data[30], data[31] );
#endif
}

/* TODO: Should be in "if_fereg.h" */
#define	FE_CNET98P2_EEP_IRQ	(0x04 * 2 + 1)	/* Irq			*/
#define	FE_CNET98P2_EEP_ADDR	(0x08 * 2)	/* Station(MAC) address	*/
#define	FE_CNET98P2_EEP_DUPLEX	(0x0c * 2 + 1)	/* Duplex mode		*/

static int
fe_probe_cnet98p2 ( DEVICE * isa_dev, struct fe_softc * sc )
{
	int	i;
	u_char	duplex;
	u_char	eeprom[FE_EEPROM_SIZE];
	static u_short const irqmap [] =
		/*                        INT0          INT1  INT2	*/
		{ NO_IRQ, NO_IRQ, NO_IRQ, IRQ3, NO_IRQ, IRQ5, IRQ6, NO_IRQ,
		  NO_IRQ, IRQ9, IRQ10, NO_IRQ, IRQ12, IRQ13, NO_IRQ, NO_IRQ };
		/*        INT3  INT4           INT5   INT6		*/

#if FE_DEBUG >= 3
	log( LOG_INFO, "fe%d: probe (0x%x) for C-NET(98)P2\n", sc->sc_unit, sc->iobase );
#endif

	/* Setup an I/O address mapping table.  */
	for ( i = 0; i < 16; i++ ) {
		sc->ioaddr[i] = sc->iobase + i;
	}
	/* Full unused slots with a safe address. */
	for ( ; i < MAXREGISTERS; i++ ) {
		sc->ioaddr[i] = sc->iobase;
	}

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, NULL );
#endif

	/* See if C-NET(98)P2 is on its address. */
	if ( inb( sc->ioaddr[FE_DLCR0] ) == (u_char)0xff ) {
#if FE_DEBUG >= 3
		log( LOG_INFO, "fe%d: inb(%x) returns 0xff\n"
		, sc->sc_unit, sc->ioaddr[FE_DLCR0] );
#endif
		return 0;
	}
	if ( inb( sc->ioaddr[FE_DLCR6] ) == (u_char)0xff ) {
#if FE_DEBUG >= 3
		log( LOG_INFO, "fe%d: inb(%x) returns 0xff\n"
		, sc->sc_unit, sc->ioaddr[FE_DLCR6] );
#endif
		return 0;
	}

	/*
	 * We are now almost sure we have a 78Q8377 at the given
	 * address.  So, read EEPROM through 78Q8377.  We have to write
	 * into LSI registers to read from EEPROM.  FIXME.
	 */
	fe_read_eeprom_tdk( sc, eeprom );

	/*
	 * Initialize constants in the per-line structure.
	 */

	/* Get our station address from EEPROM.  */
	bcopy( eeprom + FE_CNET98P2_EEP_ADDR, sc->sc_enaddr, ETHER_ADDR_LEN );

#if 1
	/*
	 * Check the Ethernet address here.
	 *
	 * Contec uses 00 80 4C ?? ?? ??.
	 */
	if ( sc->sc_enaddr[0] != (u_char)0x00
	||   sc->sc_enaddr[1] != (u_char)0x80
	||   sc->sc_enaddr[2] != (u_char)0x4C ) {
#else
	/*
	 * Make sure we got a valid Ethernet address.
	 */
	if ( ( sc->sc_enaddr[0] & 0x03 ) != 0x00	/* Multicast or Local address. */
	||   ( sc->sc_enaddr[0] | sc->sc_enaddr[1] | sc->sc_enaddr[2] ) == 0x00 ) {
#endif
#if FE_DEBUG >= 3
		log( LOG_INFO, "fe%d: invalid MAC adrs(%x:%x:%x:%x:%x:%x)\n"
		, sc->sc_unit
		, (u_char)sc->sc_enaddr[0], (u_char)sc->sc_enaddr[1]
		, (u_char)sc->sc_enaddr[2], (u_char)sc->sc_enaddr[3]
		, (u_char)sc->sc_enaddr[4], (u_char)sc->sc_enaddr[5] );
#endif
		return 0;
	}

	/*
	 * Get IRQ configuration from EEPROM.
	 */
	isa_dev->id_irq = irqmap[ eeprom[FE_CNET98P2_EEP_IRQ] ];
	if ( isa_dev->id_irq == NO_IRQ ) {
#if FE_DEBUG >= 3
		log( LOG_INFO, "fe%d: invalid irq configuration(%d)\n"
		, sc->sc_unit, eeprom[FE_CNET98P2_EEP_IRQ] );
#endif
		return 0;
	}

	/*
	 * Get Duplex-mode configuration from EEPROM.
	 */
	duplex = eeprom[FE_CNET98P2_EEP_DUPLEX] & FE_D4_DSC;
	sc->typestr = ( duplex ? "CNET98P2(Full duplex)"
		: "CNET98P2(Half duplex)" );

	/*
	 * Program the 78Q8377 as follows:
	 *      SRAM: 32KB, 100ns, byte-wide access.
	 *      Transmission buffer: 4KB x 2.
	 *      System bus interface: 16 bits.
	 * XXX: Should we add IDENT_NICE or IDENT_EC to DLCR7?  FIXME.
	 */
	sc->proto_dlcr4 = FE_D4_LBC_DISABLE | FE_D4_CNTRL | duplex;
	sc->proto_dlcr5 = 0;
	sc->proto_dlcr6 = FE_D6_BUFSIZ_32KB | FE_D6_TXBSIZ_2x4KB
		| FE_D6_BBW_BYTE | FE_D6_SBW_WORD | FE_D6_SRAM_100ns;
	sc->proto_dlcr7 = FE_D7_BYTSWP_LH;
	sc->proto_bmpr13 = FE_B13_TPTYPE_UTP | FE_B13_PORT_AUTO;
	sc->proto_bmpr14 = FE_B14_FILTER;

	sc->stop = sc->init = NULL;

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, "C-NET(98)P2 found" );
#endif

	/* Initialize 78Q8377.  */
	outb( sc->ioaddr[FE_DLCR6], sc->proto_dlcr6 | FE_D6_DLC_DISABLE );
	DELAY( 200 );

	/* Disable all interrupts.  */
	outb( sc->ioaddr[FE_DLCR2], 0 );
	outb( sc->ioaddr[FE_DLCR3], 0 );

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, "end of fe_probe_cnet98p2()" );
#endif

	/*
	 * That's all.  C-NET(98)P2 occupies 16 I/O addresses, as always.
	 */
	return 16;
}
#else
/*
 * Probe and initialization for Fujitsu FMV-180 series boards
 */
static int
fe_probe_fmv ( DEVICE * dev, struct fe_softc * sc )
{
	int i, n;

	static u_short const baseaddr [ 8 ] =
		{ 0x220, 0x240, 0x260, 0x280, 0x2A0, 0x2C0, 0x300, 0x340 };
	static u_short const irqmap [ 4 ] =
		{ IRQ3,  IRQ7,  IRQ10, IRQ15 };

	static struct fe_simple_probe_struct const probe_table [] = {
		{ FE_DLCR2, 0x70, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
	    /*	{ FE_DLCR5, 0x80, 0x00 },	Doesn't work.  */

		{ FE_FMV0,  0x78, 0x50 },	/* ERRDY+PRRDY */
		{ FE_FMV1,  0xB0, 0x00 },       /* FMV-183/184 has 0x48 bits. */
		{ FE_FMV3,  0x7F, 0x00 },
#if 1
	/*
	 * Test *vendor* part of the station address for Fujitsu.
	 * The test will gain reliability of probe process, but
	 * it rejects FMV-180 clone boards manufactured by other vendors.
	 * We have to turn the test off when such cards are made available.
	 */
		{ FE_FMV4, 0xFF, 0x00 },
		{ FE_FMV5, 0xFF, 0x00 },
		{ FE_FMV6, 0xFF, 0x0E },
#else
	/*
	 * We can always verify the *first* 2 bits (in Ethernet
	 * bit order) are "no multicast" and "no local" even for
	 * unknown vendors.
	 */
		{ FE_FMV4, 0x03, 0x00 },
#endif
		{ 0 }
	};

	/* "Hardware revision ID"  */
	int revision;

	/*
	 * See if the specified address is possible for FMV-180 series.
	 */
	for ( i = 0; i < 8; i++ ) {
		if ( baseaddr[ i ] == sc->iobase ) break;
	}
	if ( i == 8 ) return 0;

	/* Setup an I/O address mapping table.  */
	for ( i = 0; i < MAXREGISTERS; i++ ) {
		sc->ioaddr[ i ] = sc->iobase + i;
	}

	/* Simple probe.  */
	if ( !fe_simple_probe( sc, probe_table ) ) return 0;

	/* Check if our I/O address matches config info. on EEPROM.  */
	n = ( inb( sc->ioaddr[ FE_FMV2 ] ) & FE_FMV2_IOS )
	    >> FE_FMV2_IOS_SHIFT;
	if ( baseaddr[ n ] != sc->iobase ) {
#if 0
	    /* May not work on some revisions of the cards... FIXME.  */
	    return 0;
#else
	    /* Just log the fact and see what happens... FIXME.  */
	    log( LOG_WARNING, "fe%d: strange I/O config?\n", sc->sc_unit );
#endif
	}

	/* Find the "hardware revision."  */
	revision = inb( sc->ioaddr[ FE_FMV1 ] ) & FE_FMV1_REV;

	/* Determine the card type.  */
	sc->typestr = NULL;
	switch ( inb( sc->ioaddr[ FE_FMV0 ] ) & FE_FMV0_MEDIA ) {
	  case 0:
		/* No interface?  This doesn't seem to be an FMV-180...  */
		return 0;
	  case FE_FMV0_MEDIUM_T:
		switch ( revision ) {
		  case 8:
		    sc->typestr = "FMV-183";
		    break;
		  case 12:
		    sc->typestr = "FMV-183 (on-board)";
		    break;
		}
		break;
	  case FE_FMV0_MEDIUM_T | FE_FMV0_MEDIUM_5:
		switch ( revision ) {
		  case 0:
		    sc->typestr = "FMV-181";
		    break;
		  case 1:
		    sc->typestr = "FMV-181A";
		    break;
		}
		break;
	  case FE_FMV0_MEDIUM_2:
		switch ( revision ) {
		  case 8:
		    sc->typestr = "FMV-184 (CSR = 2)";
		    break;
		}
		break;
	  case FE_FMV0_MEDIUM_5:
		switch ( revision ) {
		  case 8:
		    sc->typestr = "FMV-184 (CSR = 1)";
		    break;
		}
		break;
	  case FE_FMV0_MEDIUM_2 | FE_FMV0_MEDIUM_5:
		switch ( revision ) {
		  case 0:
		    sc->typestr = "FMV-182";
		    break;
		  case 1:
		    sc->typestr = "FMV-182A";
		    break;
		  case 8:
		    sc->typestr = "FMV-184 (CSR = 3)";
		    break;
		}
		break;
	}
	if ( sc->typestr == NULL ) {
	  	/* Unknown card type...  Hope the driver works.  */
		sc->typestr = "unknown FMV-180 version";
		log( LOG_WARNING, "fe%d: %s: %x-%x-%x-%x\n",
			sc->sc_unit, sc->typestr,
			inb( sc->ioaddr[ FE_FMV0 ] ),
			inb( sc->ioaddr[ FE_FMV1 ] ),
			inb( sc->ioaddr[ FE_FMV2 ] ),
			inb( sc->ioaddr[ FE_FMV3 ] ) );
	}

	/*
	 * An FMV-180 has been proved.
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
		log( LOG_WARNING,
		    "fe%d: check IRQ in config; it may be incorrect\n",
		    sc->sc_unit );
	}

	/*
	 * Initialize constants in the per-line structure.
	 */

	/* Get our station address from EEPROM.  */
	inblk( sc, FE_FMV4, sc->sc_enaddr, ETHER_ADDR_LEN );

	/* Make sure we got a valid station address.  */
	if ( ( sc->sc_enaddr[ 0 ] & 0x03 ) != 0x00
	  || ( sc->sc_enaddr[ 0 ] == 0x00
	    && sc->sc_enaddr[ 1 ] == 0x00
	    && sc->sc_enaddr[ 2 ] == 0x00 ) ) return 0;

	/*
	 * Register values which (may) depend on board design.
	 *
	 * Program the 86960 as follows:
	 *	SRAM: 32KB, 100ns, byte-wide access.
	 *	Transmission buffer: 4KB x 2.
	 *	System bus interface: 16 bits.
	 */
	sc->proto_dlcr4 = FE_D4_LBC_DISABLE | FE_D4_CNTRL;
	sc->proto_dlcr5 = 0;
	sc->proto_dlcr6 = FE_D6_BUFSIZ_32KB | FE_D6_TXBSIZ_2x4KB
		| FE_D6_BBW_BYTE | FE_D6_SBW_WORD | FE_D6_SRAM_100ns;
	sc->proto_dlcr7 = FE_D7_BYTSWP_LH | FE_D7_IDENT_EC;
	sc->proto_bmpr13 = FE_B13_TPTYPE_UTP | FE_B13_PORT_AUTO;

	/*
	 * Minimum initialization of the hardware.
	 * We write into registers; hope I/O ports have no
	 * overlap with other boards.
	 */

	/* Initialize ASIC.  */
	outb( sc->ioaddr[ FE_FMV3 ], 0 );
	outb( sc->ioaddr[ FE_FMV10 ], 0 );

	/* Initialize 86960.  */
	DELAY( 200 );
	outb( sc->ioaddr[ FE_DLCR6 ], sc->proto_dlcr6 | FE_D6_DLC_DISABLE );
	DELAY( 200 );

	/* Disable all interrupts.  */
	outb( sc->ioaddr[ FE_DLCR2 ], 0 );
	outb( sc->ioaddr[ FE_DLCR3 ], 0 );

	/* "Refresh" hardware configuration.  FIXME.  */
	outb( sc->ioaddr[ FE_FMV2 ], inb( sc->ioaddr[ FE_FMV2 ] ) );

	/* Turn the "master interrupt control" flag of ASIC on.  */
	outb( sc->ioaddr[ FE_FMV3 ], FE_FMV3_IRQENB );

	/*
	 * That's all.  FMV-180 occupies 32 I/O addresses, by the way.
	 */
	return 32;
}

/*
 * Probe and initialization for Allied-Telesis AT1700/RE2000 series.
 */
static int
fe_probe_ati ( DEVICE * dev, struct fe_softc * sc )
{
	int i, n;
	u_char eeprom [ FE_EEPROM_SIZE ];
	u_char save16, save17;

	static u_short const baseaddr [ 8 ] =
		{ 0x260, 0x280, 0x2A0, 0x240, 0x340, 0x320, 0x380, 0x300 };
	static u_short const irqmaps [ 4 ][ 4 ] =
	{
		{ IRQ3,  IRQ4,  IRQ5,  IRQ9  },
		{ IRQ10, IRQ11, IRQ12, IRQ15 },
		{ IRQ3,  IRQ11, IRQ5,  IRQ15 },
		{ IRQ10, IRQ11, IRQ14, IRQ15 },
	};
	static struct fe_simple_probe_struct const probe_table [] = {
		{ FE_DLCR2,  0x70, 0x00 },
		{ FE_DLCR4,  0x08, 0x00 },
		{ FE_DLCR5,  0x80, 0x00 },
#if 0
		{ FE_BMPR16, 0x1B, 0x00 },
		{ FE_BMPR17, 0x7F, 0x00 },
#endif
		{ 0 }
	};

	/* Assume we have 86965 and no need to restore these.  */
	save16 = 0;
	save17 = 0;

#if FE_DEBUG >= 3
	log( LOG_INFO, "fe%d: probe (0x%x) for ATI\n",
	     sc->sc_unit, sc->iobase );
	fe_dump( LOG_INFO, sc, NULL );
#endif

	/*
	 * See if the specified address is possible for MB86965A JLI mode.
	 */
	for ( i = 0; i < 8; i++ ) {
		if ( baseaddr[ i ] == sc->iobase ) break;
	}
	if ( i == 8 ) goto NOTFOUND;

	/* Setup an I/O address mapping table.  */
	for ( i = 0; i < MAXREGISTERS; i++ ) {
		sc->ioaddr[ i ] = sc->iobase + i;
	}

	/*
	 * We should test if MB86965A is on the base address now.
	 * Unfortunately, it is very hard to probe it reliably, since
	 * we have no way to reset the chip under software control.
	 * On cold boot, we could check the "signature" bit patterns
	 * described in the Fujitsu document.  On warm boot, however,
	 * we can predict almost nothing about register values.
	 */
	if ( !fe_simple_probe( sc, probe_table ) ) goto NOTFOUND;

	/* Check if our I/O address matches config info on 86965.  */
	n = ( inb( sc->ioaddr[ FE_BMPR19 ] ) & FE_B19_ADDR )
	    >> FE_B19_ADDR_SHIFT;
	if ( baseaddr[ n ] != sc->iobase ) goto NOTFOUND;

	/*
	 * We are now almost sure we have an AT1700 at the given
	 * address.  So, read EEPROM through 86965.  We have to write
	 * into LSI registers to read from EEPROM.  I want to avoid it
	 * at this stage, but I cannot test the presence of the chip
	 * any further without reading EEPROM.  FIXME.
	 */
	save16 = inb( sc->ioaddr[ FE_BMPR16 ] );
	save17 = inb( sc->ioaddr[ FE_BMPR17 ] );
	fe_read_eeprom( sc, eeprom );

	/* Make sure the EEPROM is turned off.  */
	outb( sc->ioaddr[ FE_BMPR16 ], 0 );
	outb( sc->ioaddr[ FE_BMPR17 ], 0 );

	/* Make sure that config info in EEPROM and 86965 agree.  */
	if ( eeprom[ FE_EEPROM_CONF ] != inb( sc->ioaddr[ FE_BMPR19 ] ) ) {
		goto NOTFOUND;
	}

	/*
	 * The following model identification codes are stolen from
	 * from the NetBSD port of the fe driver.  My reviewers
	 * suggested minor revision.
	 */

	/* Determine the card type.  */
	switch (eeprom[FE_ATI_EEP_MODEL]) {
	  case FE_ATI_MODEL_AT1700T:
		sc->typestr = "AT-1700T/RE2001";
		break;
	  case FE_ATI_MODEL_AT1700BT:
		sc->typestr = "AT-1700BT/RE2003";
		break;
	  case FE_ATI_MODEL_AT1700FT:
		sc->typestr = "AT-1700FT/RE2009";
		break;
	  case FE_ATI_MODEL_AT1700AT:
		sc->typestr = "AT-1700AT/RE2005";
		break;
	  default:
		sc->typestr = "unknown AT-1700/RE2000 ?";
		break;
	}

	/*
	 * Try to determine IRQ settings.
	 * Different models use different ranges of IRQs.
	 */
	if ( dev->id_irq == NO_IRQ ) {
		n = ( inb( sc->ioaddr[ FE_BMPR19 ] ) & FE_B19_IRQ )
		    >> FE_B19_IRQ_SHIFT;
		switch ( eeprom[ FE_ATI_EEP_REVISION ] & 0xf0 ) {
		  case 0x30:
			dev->id_irq = irqmaps[ 3 ][ n ];
			break;
		  case 0x10:
		  case 0x50:
			dev->id_irq = irqmaps[ 2 ][ n ];
			break;
		  case 0x40:
		  case 0x60:
			if ( eeprom[ FE_ATI_EEP_MAGIC ] & 0x04 ) {
				dev->id_irq = irqmaps[ 1 ][ n ];
			} else {
				dev->id_irq = irqmaps[ 0 ][ n ];
			}
			break;
		  default:
			dev->id_irq = irqmaps[ 0 ][ n ];
			break;
		}
	}


	/*
	 * Initialize constants in the per-line structure.
	 */

	/* Get our station address from EEPROM.  */
	bcopy( eeprom + FE_ATI_EEP_ADDR, sc->sc_enaddr, ETHER_ADDR_LEN );

#if 1
	/*
	 * This test doesn't work well for AT1700 look-alike by
	 * other vendors.
	 */
	/* Make sure the vendor part is for Allied-Telesis.  */
	if ( sc->sc_enaddr[ 0 ] != 0x00
	  || sc->sc_enaddr[ 1 ] != 0x00
	  || sc->sc_enaddr[ 2 ] != 0xF4 ) return 0;

#else
	/* Make sure we got a valid station address.  */
	if ( ( sc->sc_enaddr[ 0 ] & 0x03 ) != 0x00
	  || ( sc->sc_enaddr[ 0 ] == 0x00
	    && sc->sc_enaddr[ 1 ] == 0x00
	    && sc->sc_enaddr[ 2 ] == 0x00 ) ) return 0;
#endif

	/*
	 * Program the 86960 as follows:
	 *	SRAM: 32KB, 100ns, byte-wide access.
	 *	Transmission buffer: 4KB x 2.
	 *	System bus interface: 16 bits.
	 */
	sc->proto_dlcr4 = FE_D4_LBC_DISABLE | FE_D4_CNTRL;  /* FIXME */
	sc->proto_dlcr5 = 0;
	sc->proto_dlcr6 = FE_D6_BUFSIZ_32KB | FE_D6_TXBSIZ_2x4KB
		| FE_D6_BBW_BYTE | FE_D6_SBW_WORD | FE_D6_SRAM_100ns;
	sc->proto_dlcr7 = FE_D7_BYTSWP_LH | FE_D7_IDENT_EC;
#if 0	/* XXXX Should we use this?  FIXME.  */
	sc->proto_bmpr13 = eeprom[ FE_ATI_EEP_MEDIA ];
#else
	sc->proto_bmpr13 = FE_B13_TPTYPE_UTP | FE_B13_PORT_AUTO;
#endif

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, "ATI found" );
#endif

	/* Setup hooks.  This may solves a nasty bug.  FIXME.  */
	sc->init = fe_init_ati;

	/* Initialize 86965.  */
	DELAY( 200 );
	outb( sc->ioaddr[ FE_DLCR6 ], sc->proto_dlcr6 | FE_D6_DLC_DISABLE );
	DELAY( 200 );

	/* Disable all interrupts.  */
	outb( sc->ioaddr[ FE_DLCR2 ], 0 );
	outb( sc->ioaddr[ FE_DLCR3 ], 0 );

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, "end of fe_probe_ati()" );
#endif

	/*
	 * That's all.  AT1700 occupies 32 I/O addresses, by the way.
	 */
	return 32;

      NOTFOUND:
	/*
	 * We have no AT1700 at a given address.
	 * Restore BMPR16 and BMPR17 if we have destroyed them,
	 * hoping that the hardware on the address didn't get
	 * bad side effect.
	 */
	if ( save16 != 0 | save17 != 0 ) {
		outb( sc->ioaddr[ FE_BMPR16 ], save16 );
		outb( sc->ioaddr[ FE_BMPR17 ], save17 );
	}
	return ( 0 );
}

/* ATI specific initialization routine.  */
static void
fe_init_ati ( struct fe_softc * sc )
{
/*
	 * I've told that the following operation "Resets" the chip.
	 * Hope this solve a bug which hangs up the driver under
	 * heavy load...  FIXME.
	 */

	/* Minimal initialization of 86965.  */
	DELAY( 200 );
	outb( sc->ioaddr[ FE_DLCR6 ], sc->proto_dlcr6 | FE_D6_DLC_DISABLE );
	DELAY( 200 );

	/* "Reset" by wrting into an undocument register location.  */
	outb( sc->ioaddr[ 0x1F ], 0 );

	/* How long do we have to wait after the reset?  FIXME.  */
	DELAY( 300 );
}
#endif	/* PC98 */

/*
 * Probe and initialization for Gateway Communications' old cards.
 */
static int
fe_probe_gwy ( DEVICE * dev, struct fe_softc * sc )
{
	int i;

	static struct fe_simple_probe_struct probe_table [] = {
		{ FE_DLCR2, 0x70, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
		{ FE_DLCR7, 0xC0, 0x00 },
		/*
		 * Test *vendor* part of the address for Gateway.
		 * This test is essential to identify Gateway's cards.
		 * We shuld define some symbolic names for the
		 * following offsets.  FIXME.
		 */
		{ 0x18, 0xFF, 0x00 },
		{ 0x19, 0xFF, 0x00 },
		{ 0x1A, 0xFF, 0x61 },
		{ 0 }
	};

	/*
	 * We need explicit IRQ and supported address.
	 * I'm not sure which address and IRQ is possible for Gateway
	 * Ethernet family.  The following accepts everything.  FIXME.
	 */
	if ( dev->id_irq == NO_IRQ || ( sc->iobase & ~0x3E0 ) != 0 ) {
		return ( 0 );
	}

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, "top of probe" );
#endif

	/* Setup an I/O address mapping table.  */
	for ( i = 0; i < MAXREGISTERS; i++ ) {
		sc->ioaddr[ i ] = sc->iobase + i;
	}

	/* See if the card is on its address.  */
	if ( !fe_simple_probe( sc, probe_table ) ) {
		return 0;
	}

	/* Determine the card type.  */
	sc->typestr = "Gateway Ethernet w/ Fujitsu chipset";

	/* Get our station address from EEPROM. */
	inblk( sc, 0x18, sc->sc_enaddr, ETHER_ADDR_LEN );

	/*
	 * Program the 86960 as follows:
	 *	SRAM: 16KB, 100ns, byte-wide access.
	 *	Transmission buffer: 2KB x 2.
	 *	System bus interface: 16 bits.
	 * Make sure to clear out ID bits in DLCR7
	 * (They actually are Encoder/Decoder control in NICE.)
	 */
	sc->proto_dlcr4 = FE_D4_LBC_DISABLE | FE_D4_CNTRL;
	sc->proto_dlcr5 = 0;
	sc->proto_dlcr6 = FE_D6_BUFSIZ_16KB | FE_D6_TXBSIZ_2x2KB
		| FE_D6_BBW_BYTE | FE_D6_SBW_WORD | FE_D6_SRAM_100ns;
	sc->proto_dlcr7 = FE_D7_BYTSWP_LH;
	sc->proto_bmpr13 = 0;

	/* Minimal initialization of 86960.  */
	DELAY( 200 );
	outb( sc->ioaddr[ FE_DLCR6 ], sc->proto_dlcr6 | FE_D6_DLC_DISABLE );
	DELAY( 200 );

	/* Disable all interrupts.  */
	outb( sc->ioaddr[ FE_DLCR2 ], 0 );
	outb( sc->ioaddr[ FE_DLCR3 ], 0 );

	/* That's all.  The card occupies 32 I/O addresses, as always.  */
	return 32;
}

#if NCARD > 0
/*
 * Probe and initialization for Fujitsu MBH10302 PCMCIA Ethernet interface.
 * Note that this is for 10302 only; MBH10304 is handled by fe_probe_tdk().
 */
static int
fe_probe_mbh ( DEVICE * dev, struct fe_softc * sc )
{
	int i,type;

	static struct fe_simple_probe_struct probe_table [] = {
		{ FE_DLCR0, 0x09, 0x00 },
		{ FE_DLCR2, 0x79, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
		{ FE_DLCR6, 0xFF, 0xB6 },
	/*
	 * The following location has the first byte of the card's
	 * Ethernet (MAC) address.
	 * We can always verify the *first* 2 bits (in Ethernet
	 * bit order) are "global" and "unicast" for any vendors'.
	 */
		{ FE_MBH10, 0x03, 0x00 },

        /* Just a gap?  Seems reliable, anyway.  */
		{ 0x12, 0xFF, 0x00 },
		{ 0x13, 0xFF, 0x00 },
		{ 0x14, 0xFF, 0x00 },
		{ 0x15, 0xFF, 0x00 },
		{ 0x16, 0xFF, 0x00 },
		{ 0x17, 0xFF, 0x00 },
#if 0
		{ 0x18, 0xFF, 0xFF },
		{ 0x19, 0xFF, 0xFF },
#endif

		{ 0 }
	};

	/*
	 * We need explicit IRQ and supported address.
	 */
	if ( dev->id_irq == NO_IRQ || ( sc->iobase & ~0x3E0 ) != 0 ) {
		return ( 0 );
	}

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, "top of probe" );
#endif

	/* Setup an I/O address mapping table.  */
	for ( i = 0; i < MAXREGISTERS; i++ ) {
		sc->ioaddr[ i ] = sc->iobase + i;
	}

	/*
	 * See if MBH10302 is on its address.
	 * I'm not sure the following probe code works.  FIXME.
	 */
	if ( !fe_simple_probe( sc, probe_table ) ) return 0;

	/* Determine the card type.  */
	sc->typestr = "MBH10302 (PCMCIA)";

	/*
	 * Initialize constants in the per-line structure.
	 */

	/* Get our station address from EEPROM.  */
	inblk( sc, FE_MBH10, sc->sc_enaddr, ETHER_ADDR_LEN );

	/* Make sure we got a valid station address.  */
	if ( sc->sc_enaddr[ 0 ] == 0x00
	    && sc->sc_enaddr[ 1 ] == 0x00
	  && sc->sc_enaddr[ 2 ] == 0x00 ) return 0;

	/*
	 * Program the 86960 as follows:
	 *	SRAM: 32KB, 100ns, byte-wide access.
	 *	Transmission buffer: 4KB x 2.
	 *	System bus interface: 16 bits.
	 */
	sc->proto_dlcr4 = FE_D4_LBC_DISABLE | FE_D4_CNTRL;
	sc->proto_dlcr5 = 0;
	sc->proto_dlcr6 = FE_D6_BUFSIZ_32KB | FE_D6_TXBSIZ_2x4KB
		| FE_D6_BBW_BYTE | FE_D6_SBW_WORD | FE_D6_SRAM_100ns;
	sc->proto_dlcr7 = FE_D7_BYTSWP_LH | FE_D7_IDENT_NICE;
	sc->proto_bmpr13 = FE_B13_TPTYPE_UTP | FE_B13_PORT_AUTO;

	/* Setup hooks.  We need a special initialization procedure.  */
	sc->init = fe_init_mbh;

	/*
	 * Minimum initialization.
	 */

	/* Minimal initialization of 86960.  */
	DELAY( 200 );
	outb( sc->ioaddr[ FE_DLCR6 ], sc->proto_dlcr6 | FE_D6_DLC_DISABLE );
	DELAY( 200 );

	/* Disable all interrupts.  */
	outb( sc->ioaddr[ FE_DLCR2 ], 0 );
	outb( sc->ioaddr[ FE_DLCR3 ], 0 );

#if 1	/* FIXME.  */
	/* Initialize system bus interface and encoder/decoder operation.  */
	outb( sc->ioaddr[ FE_MBH0 ], FE_MBH0_MAGIC | FE_MBH0_INTR_DISABLE );
#endif

	/*
	 * That's all.  MBH10302 occupies 32 I/O addresses, by the way.
	 */
	return 32;
}

/* MBH specific initialization routine.  */
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

/*
 * Probe and initialization for TDK/CONTEC PCMCIA Ethernet interface.
 * by MASUI Kenji <masui@cs.titech.ac.jp>
 *
 * (Contec uses TDK Ethenet chip -- hosokawa)
 *
 * This version of fe_probe_tdk has been rewrote to handle
 * *generic* PC card implementation of Fujitsu MB8696x and compatibles.
 * The name _tdk is just for a historical reason.  <seki> :-)
 */
static int
fe_probe_tdk ( DEVICE * dev, struct fe_softc * sc )
{
        int i;

        static struct fe_simple_probe_struct probe_table [] = {
                { FE_DLCR2, 0x70, 0x00 },
                { FE_DLCR4, 0x08, 0x00 },
            /*  { FE_DLCR5, 0x80, 0x00 },       Does not work well.  */
                { 0 }
        };

	/* We need an IRQ.  */
        if ( dev->id_irq == NO_IRQ ) {
                return ( 0 );
        }

	/* Generic driver needs Ethernet address taken from CIS.  */
	if (sc->arpcom.ac_enaddr[0] == 0
	 && sc->arpcom.ac_enaddr[1] == 0
	 && sc->arpcom.ac_enaddr[2] == 0) {
		return 0;
	}

        /* Setup an I/O address mapping table; we need only 16 ports.  */
        for (i = 0; i < 16; i++) {
                sc->ioaddr[i] = sc->iobase + i;
        }
	/* Fill unused slots with a safe address.  */
        for (i = 16; i < MAXREGISTERS; i++) {
                sc->ioaddr[i] = sc->iobase;
        }

        /*
         * See if C-NET(PC)C is on its address.
         */

        if ( !fe_simple_probe( sc, probe_table ) ) return 0;

        /* Determine the card type.  */
        sc->typestr = "Generic MB8696x Ethernet (PCMCIA)";

        /*
         * Initialize constants in the per-line structure.
         */

        /* The station address *must*be* already in sc_enaddr;
           Make sure we got a valid station address.  */
        if ( ( sc->sc_enaddr[ 0 ] & 0x03 ) != 0x00
          || ( sc->sc_enaddr[ 0 ] == 0x00
            && sc->sc_enaddr[ 1 ] == 0x00
            && sc->sc_enaddr[ 2 ] == 0x00 ) ) return 0;

        /*
         * Program the 86965 as follows:
         *      SRAM: 32KB, 100ns, byte-wide access.
         *      Transmission buffer: 4KB x 2.
         *      System bus interface: 16 bits.
	 * XXX: Should we remove IDENT_NICE from DLCR7?  Or,
	 *	even add IDENT_EC instead?  FIXME.
         */
        sc->proto_dlcr4 = FE_D4_LBC_DISABLE | FE_D4_CNTRL;
        sc->proto_dlcr5 = 0;
        sc->proto_dlcr6 = FE_D6_BUFSIZ_32KB | FE_D6_TXBSIZ_2x4KB
                | FE_D6_BBW_BYTE | FE_D6_SBW_WORD | FE_D6_SRAM_100ns;
        sc->proto_dlcr7 = FE_D7_BYTSWP_LH | FE_D7_IDENT_NICE;
        sc->proto_bmpr13 = FE_B13_TPTYPE_UTP | FE_B13_PORT_AUTO;

        /* Minimul initialization of 86960.  */
        DELAY( 200 );
        outb( sc->ioaddr[ FE_DLCR6 ], sc->proto_dlcr6 | FE_D6_DLC_DISABLE );
        DELAY( 200 );

        /* Disable all interrupts.  */
        outb( sc->ioaddr[ FE_DLCR2 ], 0 );
        outb( sc->ioaddr[ FE_DLCR3 ], 0 );

        /*
         * That's all.  C-NET(PC)C occupies 16 I/O addresses.
	 *
	 * Some PC cards (e.g., TDK and Contec) have 16 I/O addresses,
	 * while some others (e.g., Fujitsu) have 32.  Fortunately,
	 * this generic driver never accesses latter 16 ports in 32
	 * ports cards.  So, we can assume the *generic* PC cards
	 * always have 16 ports.
	 *
	 * Moreover, PC card probe is isolated from ISA probe, and PC
	 * card probe routine doesn't use "# of ports" returned by this
	 * function.  16 v.s. 32 is not important now.
	 */
        return 16;
}
#endif /* NCARD > 0 */

/*
 * Install interface into kernel networking data structures
 */
static int
fe_attach ( DEVICE * dev )
{
#if NCARD > 0
	static	int	already_ifattach[NFE];
#endif
	struct fe_softc *sc = &fe_softc[dev->id_unit];

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

	/*
	 * Set default interface flags.
	 */
 	sc->sc_if.if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;

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

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, "attach()" );
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
#if FE_DEBUG >= 2
		log( LOG_WARNING, "fe%d: strange TXBSIZ config; fixing\n",
			sc->sc_unit );
#endif
		sc->proto_dlcr6 &= ~FE_D6_TXBSIZ;
		sc->proto_dlcr6 |=  FE_D6_TXBSIZ_2x2KB;
		sc->txb_size = 2048;
		break;
	}

	/* Attach and stop the interface. */
#if NCARD > 0
	if (already_ifattach[dev->id_unit] != 1) {
		if_attach(&sc->sc_if);
		already_ifattach[dev->id_unit] = 1;
	}
#else
	if_attach(&sc->sc_if);
#endif
	fe_stop(sc->sc_unit);		/* This changes the state to IDLE.  */
 	ether_ifattach(&sc->sc_if);
  
  	/* Print additional info when attached.  */
 	printf( "fe%d: address %6D, type %s\n", sc->sc_unit,
 		sc->sc_enaddr, ":" , sc->typestr );
#if FE_DEBUG >= 3
	{
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
		printf( "fe%d: SRAM %dKB %dbit %dns, TXB %dKBx2, %dbit I/O\n",
			sc->sc_unit, buf, bbw, ram, txb, sbw );
	}
#endif

#if NBPFILTER > 0
	/* If BPF is in the kernel, call the attach for it.  */
 	bpfattach( &sc->sc_if, DLT_EN10MB, sizeof(struct ether_header));
#endif
	return 1;
}

/*
 * Reset interface.
 */
static void
fe_reset ( int unit )
{
	/*
	 * Stop interface and re-initialize.
	 */
	fe_stop(unit);
	fe_init(unit);
}

/*
 * Stop everything on the interface.
 *
 * All buffered packets, both transmitting and receiving,
 * if any, will be lost by stopping the interface.
 */
static void
fe_stop ( int unit )
{
	struct fe_softc *sc = &fe_softc[unit];
	int s;

	s = splimp();

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, "stop()" );
#endif

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

	/* Update config status also.  */

	/* Call a hook.  */
	if ( sc->stop ) sc->stop( sc );

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, "end of stop()" );
#endif

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

#if FE_DEBUG >= 1
	/* A "debug" message.  */
	log( LOG_ERR, "fe%d: transmission timeout (%d+%d)%s\n",
		ifp->if_unit, sc->txb_sched, sc->txb_count,
		( ifp->if_flags & IFF_UP ) ? "" : " when down" );
	if ( sc->sc_if.if_opackets == 0 && sc->sc_if.if_ipackets == 0 ) {
		log( LOG_WARNING, "fe%d: wrong IRQ setting in config?\n",
		    ifp->if_unit );
	}
#endif

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, NULL );
#endif

	/* Record how many packets are lost by this accident.  */
	ifp->if_oerrors += sc->txb_sched + sc->txb_count;

	/* Put the interface into known initial state.  */
	if ( ifp->if_flags & IFF_UP ) {
		fe_reset( ifp->if_unit );
	} else {
		fe_stop( ifp->if_unit );
	}
}

/*
 * Initialize device.
 */
static void
fe_init ( int unit )
{
	struct fe_softc *sc = &fe_softc[unit];
	int s;

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, "init()" );
#endif

	/* We need an address. */
	if (TAILQ_EMPTY(&sc->sc_if.if_addrhead)) { /* XXX unlikely */
#if FE_DEBUG >= 1
		log( LOG_ERR, "fe%d: init() without any address\n",
			sc->sc_unit );
#endif
		return;
	}

#if FE_DEBUG >= 1
	/*
	 * Make sure we have a valid station address.
	 * The following test is applicable for any Ethernet interfaces.
	 * It can be done in somewhere common to all of them.  FIXME.
	 */
	if ( ( sc->sc_enaddr[ 0 ] & 0x01 ) != 0
	  || ( sc->sc_enaddr[ 0 ] == 0x00
	    && sc->sc_enaddr[ 1 ] == 0x00
	    && sc->sc_enaddr[ 2 ] == 0x00 ) ) {
		log( LOG_ERR, "fe%d: invalid station address (%6D)\n",
			sc->sc_unit, sc->sc_enaddr, ":" );
		return;
	}
#endif

	/* Start initializing 86960.  */
	s = splimp();

	/* Call a hook.  */
	if ( sc->init ) sc->init( sc );

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, "after init hook" );
#endif

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

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, "just before enabling DLC" );
#endif

	/* Enable interrupts.  */
	outb( sc->ioaddr[ FE_DLCR2 ], FE_TMASK );
	outb( sc->ioaddr[ FE_DLCR3 ], FE_RMASK );

	/* Enable transmitter and receiver.  */
	DELAY( 200 );
	outb( sc->ioaddr[ FE_DLCR6 ], sc->proto_dlcr6 | FE_D6_DLC_ENABLE );
	DELAY( 200 );

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, "just after enabling DLC" );
#endif

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
		log( LOG_WARNING,
			"fe%d: receive buffer has some data after reset\n",
			sc->sc_unit );
		
		fe_emptybuffer( sc );
	}

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, "after ERB loop" );
#endif

	/* Do we need this here?  Actually, no.  I must be paranoia.  */
	outb( sc->ioaddr[ FE_DLCR0 ], 0xFF );	/* Clear all bits.  */
	outb( sc->ioaddr[ FE_DLCR1 ], 0xFF );	/* ditto.  */

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, "after FIXME" );
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

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, "after setmode" );
#endif

	/* ...and attempt to start output queued packets.  */
	fe_start( &sc->sc_if );

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, "init() done" );
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

#if FE_DEBUG >= 1
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
		log( LOG_ERR, "fe%d: inconsistent txb variables (%d, %d)\n",
			sc->sc_unit, sc->txb_count, sc->txb_free );
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

#if FE_DEBUG >= 1
	/*
	 * First, see if there are buffered packets and an idle
	 * transmitter - should never happen at this point.
	 */
	if ( ( sc->txb_count > 0 ) && ( sc->txb_sched == 0 ) ) {
		log( LOG_ERR,
			"fe%d: transmitter idle with %d buffered packets\n",
			sc->sc_unit, sc->txb_count );
		fe_xmit( sc );
	}
#endif

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
		( void )inw( sc->ioaddr[ FE_BMPR8 ] );
		( void )inw( sc->ioaddr[ FE_BMPR8 ] );
		outb( sc->ioaddr[ FE_BMPR14 ], sc->proto_bmpr14 | FE_B14_SKIP );
	} else {
		/* We should not come here unless receiving RUNTs.  */
		for ( i = 0; i < len; i += 2 ) {
			( void )inw( sc->ioaddr[ FE_BMPR8 ] );
		}
	}
}

/*
 * Empty receiving buffer.
 */
static void
fe_emptybuffer ( struct fe_softc * sc )
{
	int i;
	u_char saved_dlcr5;

#if FE_DEBUG >= 2
	log( LOG_WARNING, "fe%d: emptying receive buffer\n", sc->sc_unit );
#endif
	/*
	 * Stop receiving packets, temporarily.
	 */
	saved_dlcr5 = inb( sc->ioaddr[ FE_DLCR5 ] );
	outb( sc->ioaddr[ FE_DLCR5 ], sc->proto_dlcr5 );
	DELAY(1300);

	/*
	 * When we come here, the receive buffer management should
	 * have been broken.  So, we cannot use skip operation.
	 * Just discard everything in the buffer.
	 */
	for (i = 0; i < 32768; i++) {
		if ( inb( sc->ioaddr[ FE_DLCR5 ] ) & FE_D5_BUFEMP ) break;
		( void )inw( sc->ioaddr[ FE_BMPR8 ] );
	}

	/*
	 * Double check.
	 */
	if ( inb( sc->ioaddr[ FE_DLCR5 ] ) & FE_D5_BUFEMP ) {
		log( LOG_ERR, "fe%d: could not empty receive buffer\n",
			sc->sc_unit );
		/* Hmm.  What should I do if this happens?  FIXME.  */
	}

	/*
	 * Restart receiving packets.
	 */
	outb( sc->ioaddr[ FE_DLCR5 ], saved_dlcr5 );
}

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

#if FE_DEBUG >= 2
		log( LOG_WARNING, "fe%d: excessive collision (%d/%d)\n",
			sc->sc_unit, left, sc->txb_sched );
#endif
#if FE_DEBUG >= 3
		fe_dump( LOG_INFO, sc, NULL );
#endif

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
#if FE_DEBUG >= 3
			log( LOG_WARNING, "fe%d: %d collision(s) (%d)\n",
				sc->sc_unit, col, sc->txb_sched );
#endif
		}

		/*
		 * Update transmission statistics.
		 * Be sure to reflect number of excessive collisions.
		 */
		sc->sc_if.if_opackets += sc->txb_sched - sc->tx_excolls;
		sc->sc_if.if_oerrors += sc->tx_excolls;
		sc->sc_if.if_collisions += sc->tx_excolls * 16;
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
	 */
	if ( rstat & ( FE_D1_OVRFLO | FE_D1_CRCERR
		     | FE_D1_ALGERR | FE_D1_SRTPKT ) ) {
#if FE_DEBUG >= 2
		log( LOG_WARNING,
			"fe%d: receive error: %s%s%s%s(%02x)\n",
			sc->sc_unit,
			rstat & FE_D1_OVRFLO ? "OVR " : "",
			rstat & FE_D1_CRCERR ? "CRC " : "",
			rstat & FE_D1_ALGERR ? "ALG " : "",
			rstat & FE_D1_SRTPKT ? "LEN " : "",
			rstat );
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
	for (i = 0; i < FE_MAX_RECV_COUNT * 2; i++) {

		/* Stop the iteration if 86960 indicates no packets.  */
		if ( inb( sc->ioaddr[ FE_DLCR5 ] ) & FE_D5_BUFEMP ) break;

		/*
		 * Extract A receive status byte.
		 * As our 86960 is in 16 bit bus access mode, we have to
		 * use inw() to get the status byte.  The significant
		 * value is returned in lower 8 bits.
		 */
		status = ( u_char )inw( sc->ioaddr[ FE_BMPR8 ] );
#if FE_DEBUG >= 4
		log( LOG_INFO, "fe%d: receive status = %04x\n",
			sc->sc_unit, status );
#endif

		/*
		 * Extract the packet length.
		 * It is a sum of a header (14 bytes) and a payload.
		 * CRC has been stripped off by the 86960.
		 */
		len = inw( sc->ioaddr[ FE_BMPR8 ] );

#if FE_DEBUG >= 1
		/*
		 * If there was an error with the received packet, it
		 * must be an indication of out-of-sync on receive
		 * buffer, because we have programmed the 8696x to
		 * to discard errored packets, even when the interface
		 * is in promiscuous mode.  We have to re-synchronize.
		 */
		if (!(status & FE_RPH_GOOD)) {
			log(LOG_ERR,
			    "fe%d: corrupted receive status byte (%02x)\n",
			    sc->arpcom.ac_if.if_unit, status);
			sc->arpcom.ac_if.if_ierrors++;
			fe_emptybuffer( sc );
			break;
		}
#endif

#if FE_DEBUG >= 1
		/*
		 * MB86960 checks the packet length and drop big packet
		 * before passing it to us.  There are no chance we can
		 * get big packets through it, even if they are actually
		 * sent over a line.  Hence, if the length exceeds
		 * the specified limit, it means some serious failure,
		 * such as out-of-sync on receive buffer management.
		 *
		 * Same for short packets, since we have programmed
		 * 86960 to drop short packets.
		 */
		if ( len > ETHER_MAX_LEN - ETHER_CRC_LEN
		  || len < ETHER_MIN_LEN - ETHER_CRC_LEN ) {
			log( LOG_WARNING,
				"fe%d: received a %s packet? (%u bytes)\n",
				sc->sc_unit,
				len < ETHER_MIN_LEN - ETHER_CRC_LEN
					? "partial" : "big",
				len );
			sc->sc_if.if_ierrors++;
			fe_emptybuffer( sc );
			break;
		}
#endif

		/*
		 * Go get a packet.
		 */
		if ( fe_get_packet( sc, len ) < 0 ) {

#if FE_DEBUG >= 2
			log( LOG_WARNING, "%s%d: out of mbuf;"
			    " dropping a packet (%u bytes)\n",
			    sc->sc_unit, len );
#endif

			/* Skip a packet, updating statistics.  */
			sc->sc_if.if_ierrors++;
			fe_droppacket( sc, len );

			/*
			 * Try extracting other packets, although they will
			 * cause out-of-mbuf error again.  This is required
			 * to keep receiver interrupt comming.
			 * (Earlier versions had a bug on this point.)
			 */
			continue;
		}

		/* Successfully received a packet.  Update stat.  */
		sc->sc_if.if_ipackets++;
	}
}

/*
 * Ethernet interface interrupt processor
 */
void
feintr ( int unit )
{
	struct fe_softc *sc = &fe_softc[unit];
	u_char tstat, rstat;

	/*
	 * Loop until there are no more new interrupt conditions.
	 */
	for (;;) {

#if FE_DEBUG >= 4
		fe_dump( LOG_INFO, sc, "intr()" );
#endif

		/*
		 * Get interrupt conditions, masking unneeded flags.
		 */
		tstat = inb( sc->ioaddr[ FE_DLCR0 ] ) & FE_TMASK;
		rstat = inb( sc->ioaddr[ FE_DLCR1 ] ) & FE_RMASK;

#if FE_DEBUG >= 1
		/* Test for a "dead-lock" condition.  */
		if ((rstat & FE_D1_PKTRDY) == 0
		    && (inb(sc->ioaddr[FE_DLCR5]) & FE_D5_BUFEMP) == 0
		    && (inb(sc->ioaddr[FE_DLCR1]) & FE_D1_PKTRDY) == 0) {
			/*
			 * PKTRDY is off, while receive buffer is not empty.
			 * We did a double check to avoid a race condition...
			 * So, we should have missed an interrupt.
			 */
			log(LOG_WARNING,
			    "fe%d: missed a receiver interrupt?\n",
			    sc->arpcom.ac_if.if_unit);
			/* Simulate the missed interrupt condition.  */
			rstat |= FE_D1_PKTRDY;
		}
#endif

		/* Stop processing if there are no interrupts to handle.  */
		if ( tstat == 0 && rstat == 0 ) break;

		/*
		 * Reset the conditions we are acknowledging.
		 */
		outb( sc->ioaddr[ FE_DLCR0 ], tstat );
		outb( sc->ioaddr[ FE_DLCR1 ], rstat );

		/*
		 * Handle transmitter interrupts. Handle these first because
		 * the receiver will reset the board under some conditions.
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
}

/*
 * Process an ioctl request. This code needs some work - it looks
 * pretty ugly.
 */
static int
fe_ioctl ( struct ifnet * ifp, int command, caddr_t data )
{
	struct fe_softc *sc = ifp->if_softc;
	int s, error = 0;

#if FE_DEBUG >= 3
	log( LOG_INFO, "fe%d: ioctl(%x)\n", sc->sc_unit, command );
#endif

	s = splimp();

	switch (command) {

	  case SIOCSIFADDR:
	    {
		struct ifaddr * ifa = ( struct ifaddr * )data;

		sc->sc_if.if_flags |= IFF_UP;

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		  case AF_INET:
			fe_init( sc->sc_unit );	/* before arp_ifinit */
			arp_ifinit( &sc->arpcom, ifa );
			break;
#endif
#ifdef IPX
			/*
			 * XXX - This code is probably wrong
			 */
		  case AF_IPX:
			{
				register struct ipx_addr *ina
				    = &(IA_SIPX(ifa)->sipx_addr);

				if (ipx_nullhost(*ina))
					ina->x_host =
					    *(union ipx_host *) (sc->sc_enaddr); 				else {
					bcopy((caddr_t) ina->x_host.c_host,
					      (caddr_t) sc->sc_enaddr,
					      sizeof(sc->sc_enaddr));
				}

				/*
				 * Set new address
				 */
				fe_init(sc->sc_unit);
				break;
			}
#endif
#ifdef INET6
		  case AF_INET6:
			/* IPV6 added by shin 96.2.6 */
			fe_init(sc->sc_unit);
			ndp6_ifinit(&sc->arpcom, ifa);
			break;
#endif
#ifdef NS

			/*
			 * XXX - This code is probably wrong
			 */
		  case AF_NS:
			{
				register struct ns_addr *ina
				    = &(IA_SNS(ifa)->sns_addr);

				if (ns_nullhost(*ina))
					ina->x_host =
					    *(union ns_host *) (sc->sc_enaddr);
				else {
					bcopy((caddr_t) ina->x_host.c_host,
					      (caddr_t) sc->sc_enaddr,
					      sizeof(sc->sc_enaddr));
				}

				/*
				 * Set new address
				 */
				fe_init(sc->sc_unit);
				break;
			}
#endif
		  default:
			fe_init( sc->sc_unit );
			break;
		}
		break;
	    }

#ifdef SIOCGIFADDR
	  case SIOCGIFADDR:
	    {
		struct ifreq * ifr = ( struct ifreq * )data;
		struct sockaddr * sa = ( struct sockaddr * )&ifr->ifr_data;

		bcopy((caddr_t)sc->sc_enaddr,
		      (caddr_t)sa->sa_data, ETHER_ADDR_LEN);
		break;
	    }
#endif

#ifdef SIOCGIFPHYSADDR
	  case SIOCGIFPHYSADDR:
	    {
		struct ifreq * ifr = ( struct ifreq * )data;

		bcopy((caddr_t)sc->sc_enaddr,
		      (caddr_t)&ifr->ifr_data, ETHER_ADDR_LEN);
		break;
	    }
#endif

#ifdef notdef
#ifdef SIOCSIFPHYSADDR
	  case SIOCSIFPHYSADDR:
	    {
		/*
		 * Set the physical (Ethernet) address of the interface.
		 * When and by whom is this command used?  FIXME.
		 */
		struct ifreq * ifr = ( struct ifreq * )data;

		bcopy((caddr_t)&ifr->ifr_data,
		      (caddr_t)sc->sc_enaddr, ETHER_ADDR_LEN);
		fe_setlinkaddr( sc );
		break;
	    }
#endif
#endif /* notdef */

#ifdef SIOCSIFFLAGS
	  case SIOCSIFFLAGS:
	    {
		/*
		 * Switch interface state between "running" and
		 * "stopped", reflecting the UP flag.
		 */
		if ( sc->sc_if.if_flags & IFF_UP ) {
			if ( ( sc->sc_if.if_flags & IFF_RUNNING ) == 0 ) {
				fe_init( sc->sc_unit );
			}
		} else {
			if ( ( sc->sc_if.if_flags & IFF_RUNNING ) != 0 ) {
				fe_stop( sc->sc_unit );
			}
		}

		/*
		 * Promiscuous and/or multicast flags may have changed,
		 * so reprogram the multicast filter and/or receive mode.
		 */
		fe_setmode( sc );

#if FE_DEBUG >= 1
		/* "ifconfig fe0 debug" to print register dump.  */
		if ( sc->sc_if.if_flags & IFF_DEBUG ) {
			fe_dump( LOG_DEBUG, sc, "SIOCSIFFLAGS(DEBUG)" );
		}
#endif
		break;
	    }
#endif

#ifdef SIOCADDMULTI
	  case SIOCADDMULTI:
	  case SIOCDELMULTI:
	    /*
	     * Multicast list has changed; set the hardware filter
	     * accordingly.
	     */
	    fe_setmode( sc );
	    error = 0;
	    break;
#endif

#ifdef SIOCSIFMTU
	  case SIOCSIFMTU:
	    {
		/*
		 * Set the interface MTU.
		 */
		struct ifreq * ifr = ( struct ifreq * )data;

		if ( ifr->ifr_mtu > ETHERMTU ) {
			error = EINVAL;
		} else {
			sc->sc_if.if_mtu = ifr->ifr_mtu;
		}
		break;
	    }
#endif

	  default:
		error = EINVAL;
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

	/* Get a packet.  */
	insw( sc->ioaddr[ FE_BMPR8 ], m->m_data, ( len + 1 ) >> 1 );

	/* Get (actually just point to) the header part.  */
	eh = mtod( m, struct ether_header *);

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

#if FE_DEBUG >= 3
	if ( !ETHER_ADDR_IS_MULTICAST( eh->ether_dhost )
	  && bcmp( eh->ether_dhost, sc->sc_enaddr, ETHER_ADDR_LEN ) != 0 ) {
		/*
		 * This packet was not for us.  We can't be in promiscuous
		 * mode since the case was handled by above test.
		 * We found an error (of this driver.)
		 */
		log( LOG_WARNING,
			"fe%d: got an unwanted packet, dst = %6D\n",
			sc->sc_unit, eh->ether_dhost , ":" );
		m_freem( m );
		return 0;
	}
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

#if FE_DEBUG >= 1
	/* First, count up the total number of bytes to copy */
	length = 0;
	for ( mp = m; mp != NULL; mp = mp->m_next ) {
		length += mp->m_len;
	}
#else
	/* Just use the length value in the packet header.  */
	length = m->m_pkthdr.len;
#endif

#if FE_DEBUG >= 2
	/* Check if this matches the one in the packet header.  */
	if ( length != m->m_pkthdr.len ) {
		log( LOG_WARNING, "fe%d: packet length mismatch? (%d/%d)\n",
			sc->sc_unit, length, m->m_pkthdr.len );
	}
#endif

#if FE_DEBUG >= 1
	/*
	 * Should never send big packets.  If such a packet is passed,
	 * it should be a bug of upper layer.  We just ignore it.
	 * ... Partial (too short) packets, neither.
	 */
	if ( length < ETHER_HDR_LEN
	  || length > ETHER_MAX_LEN - ETHER_CRC_LEN ) {
		log( LOG_ERR,
			"fe%d: got an out-of-spec packet (%u bytes) to send\n",
			sc->sc_unit, length );
		sc->sc_if.if_oerrors++;
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
	outw( addr_bmpr8, max( length, ETHER_MIN_LEN - ETHER_CRC_LEN ) );

	/*
	 * Update buffer status now.
	 * Truncate the length up to an even number, since we use outw().
	 */
	length = ( length + 1 ) & ~1;
	sc->txb_free -= FE_DATA_LEN_LEN + max( length, ETHER_MIN_LEN - ETHER_CRC_LEN);
	sc->txb_count++;

	/*
	 * Transfer the data from mbuf chain to the transmission buffer.
	 * MB86960 seems to require that data be transferred as words, and
	 * only words.  So that we require some extra code to patch
	 * over odd-length mbufs.
	 */
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

	/* Pad to the Ethernet minimum length, if the packet is too short.  */
	if ( length < ETHER_MIN_LEN - ETHER_CRC_LEN ) {
		outsw( addr_bmpr8, padding, ( ETHER_MIN_LEN - ETHER_CRC_LEN - length ) >> 1);
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
#if FE_DEBUG >= 4
		log( LOG_INFO, "fe%d: hash(%6D) == %d\n",
			sc->sc_unit, enm->enm_addrlo , ":", index );
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

#if FE_DEBUG >= 3
		log( LOG_INFO, "fe%d: promiscuous mode\n", sc->sc_unit );
#endif
		return;
	}

	/*
	 * Turn the chip to the normal (non-promiscuous) mode.
	 */
	outb( sc->ioaddr[ FE_DLCR5 ], sc->proto_dlcr5 | FE_D5_AFM1 );

	/*
	 * Find the new multicast filter value.
	 * I'm not sure we have to handle modes other than MULTICAST.
	 * Who sets ALLMULTI?  Who turns MULTICAST off?  FIXME.
	 */
	if ( flags & IFF_ALLMULTI ) {
		sc->filter = fe_filter_all;
	} else if ( flags & IFF_MULTICAST ) {
		sc->filter = fe_mcaf( sc );
	} else {
		sc->filter = fe_filter_nothing;
	}
	sc->filter_change = 1;

#if FE_DEBUG >= 3
	log( LOG_INFO, "fe%d: address filter: [%8D]\n",
	    sc->sc_unit, sc->filter.data, " " );
#endif

	/*
	 * We have to update the multicast filter in the 86960, A.S.A.P.
	 *
	 * Note that the DLC (Data Link Control unit, i.e. transmitter
	 * and receiver) must be stopped when feeding the filter, and
	 * DLC trashes all packets in both transmission and receive
	 * buffers when stopped.
	 *
	 * ... Are the above sentences correct?  I have to check the
	 *     manual of the MB86960A.  FIXME.
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
#if FE_DEBUG >= 4
		log( LOG_INFO, "fe%d: filter change delayed\n", sc->sc_unit );
#endif
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

#if FE_DEBUG >= 3
	log( LOG_INFO, "fe%d: address filter changed\n", sc->sc_unit );
#endif
}

#if FE_DEBUG >= 1
static void
fe_dump ( int level, struct fe_softc * sc, char * message )
{
	log( level, "fe%d: %s,"
	    " DLCR = %02x %02x %02x %02x %02x %02x %02x %02x,"
	    " BMPR = xx xx %02x %02x %02x %02x %02x %02x,"
	    " asic = %02x %02x %02x %02x %02x %02x %02x %02x"
	    " + %02x %02x %02x %02x %02x %02x %02x %02x\n",
	    sc->sc_unit, message ? message : "registers",
	    inb( sc->ioaddr[ FE_DLCR0 ] ),  inb( sc->ioaddr[ FE_DLCR1 ] ),
	    inb( sc->ioaddr[ FE_DLCR2 ] ),  inb( sc->ioaddr[ FE_DLCR3 ] ),
	    inb( sc->ioaddr[ FE_DLCR4 ] ),  inb( sc->ioaddr[ FE_DLCR5 ] ),
	    inb( sc->ioaddr[ FE_DLCR6 ] ),  inb( sc->ioaddr[ FE_DLCR7 ] ),
	    inb( sc->ioaddr[ FE_BMPR10 ] ), inb( sc->ioaddr[ FE_BMPR11 ] ),
	    inb( sc->ioaddr[ FE_BMPR12 ] ), inb( sc->ioaddr[ FE_BMPR13 ] ),
	    inb( sc->ioaddr[ FE_BMPR14 ] ), inb( sc->ioaddr[ FE_BMPR15 ] ),
	    inb( sc->ioaddr[ 0x10 ] ),      inb( sc->ioaddr[ 0x11 ] ),
	    inb( sc->ioaddr[ 0x12 ] ),      inb( sc->ioaddr[ 0x13 ] ),
	    inb( sc->ioaddr[ 0x14 ] ),      inb( sc->ioaddr[ 0x15 ] ),
	    inb( sc->ioaddr[ 0x16 ] ),      inb( sc->ioaddr[ 0x17 ] ),
	    inb( sc->ioaddr[ 0x18 ] ),      inb( sc->ioaddr[ 0x19 ] ),
	    inb( sc->ioaddr[ 0x1A ] ),      inb( sc->ioaddr[ 0x1B ] ),
	    inb( sc->ioaddr[ 0x1C ] ),      inb( sc->ioaddr[ 0x1D ] ),
	    inb( sc->ioaddr[ 0x1E ] ),      inb( sc->ioaddr[ 0x1F ] ) );
}
#endif
