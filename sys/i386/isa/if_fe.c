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

#define FE_VERSION "if_fe.c ver. 0.8a"

/*
 * Device driver for Fujitsu MB86960A/MB86965A based Ethernet cards.
 * To be used with FreeBSD 2.0 RELEASE.
 * Contributed by M.S. <seki@sysrap.cs.fujitsu.co.jp>
 *
 * This version is intended to be a generic template for various
 * MB86960A/MB86965A based Ethernet cards.  It currently supports
 * Fujitsu FMV-180 series (i.e., FMV-181 and FMV-182) and Allied-
 * Telesis AT1700 series and RE2000 series.  There are some
 * unnecessary hooks embedded, which are primarily intended to support
 * other types of Ethernet cards, but the author is not sure whether
 * they are useful.
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

#include "fe.h"
#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/devconf.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#ifdef NS
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <machine/clock.h>

#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/icu.h>

#include <i386/isa/ic/mb86960.h>
#include <i386/isa/if_fereg.h>

#ifdef __GNUC__
#define INLINE inline
#else
#define INLINE
#endif

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
 * Delay padding of short transmission packets to minimum Ethernet size.
 * This may or may not gain performance.  An EXPERIMENTAL option.
 */
#ifndef FE_DELAYED_PADDING
#define FE_DELAYED_PADDING 0
#endif

/*
 * Transmit just one packet per a "send"command to 86960.
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

/* A cludge for PCMCIA support.  */
#define FE_FLAGS_PCMCIA		0x8000

/* Shouldn't this be defined somewhere else such as isa_device.h?  */
#define NO_IOADDR 0xFFFFFFFF

/* Identification of the driver version.  */
static char const fe_version [] = FE_VERSION " / " FE_REG_VERSION;

/*
 * Supported hardware (Ethernet card) types
 * This information is currently used only for debugging
 */
enum fe_type
{
	/* For cards which are successfully probed but not identified.  */
	FE_TYPE_UNKNOWN,

	/* Fujitsu FMV-180 series.  */
	FE_TYPE_FMV181,
	FE_TYPE_FMV182,

	/* Allied-Telesis AT1700 series and RE2000 series.  */
	FE_TYPE_AT1700,

	/* PCMCIA by Fujitsu.  */
	FE_TYPE_MBH10302,
	FE_TYPE_MBH10304,

	/* More can be here.  */
};

/*
 * Data type for a multicast address filter on 86960.
 */
struct fe_filter { u_char data [ FE_FILTER_LEN ]; };

/*
 * Special filter values.
 */
static struct fe_filter const fe_filter_nothing = { FE_FILTER_NOTHING };
static struct fe_filter const fe_filter_all     = { FE_FILTER_ALL };

/*
 * fe_softc: per line info and status
 */
struct fe_softc {

	/* Used by "common" codes.  */
	struct arpcom arpcom;	/* ethernet common */

	/* Used by config codes.  */
	struct kern_devconf kdc;/* Kernel configuration database info.  */

	/* Set by probe() and not modified in later phases.  */
	enum fe_type type;	/* interface type code */
	char * typestr;		/* printable name of the interface.  */
	u_short addr;		/* MB86960A I/O base address */
	u_short txb_size;	/* size of TX buffer, in bytes  */
	u_char proto_dlcr4;	/* DLCR4 prototype.  */
	u_char proto_dlcr5;	/* DLCR5 prototype.  */
	u_char proto_dlcr6;	/* DLCR6 prototype.  */
	u_char proto_dlcr7;	/* DLCR7 prototype.  */

	/* Vendor specific hooks.  */
	void ( * init )( struct fe_softc * ); /* Just before fe_init().  */
	void ( * stop )( struct fe_softc * ); /* Just after fe_stop().  */

	/* For BPF.  */
	caddr_t bpf;		/* BPF "magic cookie" */

	/* Transmission buffer management.  */
	u_short txb_free;	/* free bytes in TX buffer  */
	u_char txb_count;	/* number of packets in TX buffer  */
	u_char txb_sched;	/* number of scheduled packets  */
	u_char txb_padding;	/* number of delayed padding bytes  */

	/* Multicast address filter management.  */
	u_char filter_change;	/* MARs must be changed ASAP. */
	struct fe_filter filter;/* new filter value.  */

}       fe_softc[NFE];

/* Frequently accessed members in arpcom and kdc.  */
#define sc_if		arpcom.ac_if
#define sc_unit		arpcom.ac_if.if_unit
#define sc_enaddr	arpcom.ac_enaddr
#define sc_dcstate	kdc.kdc_state
#define sc_description	kdc.kdc_description

/*
 * Some entry functions receive a "struct ifnet *" typed pointer as an
 * argument.  It points to arpcom.ac_if of our softc.  Remember arpcom.ac_if
 * is located at very first of the fe_softc struct.  So, there is no
 * difference between "struct fe_softc *" and "struct ifnet *" at the machine
 * language level.  We just cast to turn a "struct ifnet *" value into "struct
 * fe_softc * value".  If this were C++, we would need no such cast at all.
 */
#define IFNET2SOFTC(P)	( ( struct fe_softc * )(P) )

/* Public entry point.  This is the only functoin which must be external.  */
void		feintr		( int );

/* Standard driver entry points.  These can be static.  */
int		fe_probe	( struct isa_device * );
int		fe_attach	( struct isa_device * );
void		fe_init		( int );
int		fe_ioctl	( struct ifnet *, int, caddr_t );
void		fe_start	( struct ifnet * );
void		fe_reset	( int );
void		fe_watchdog	( int );

/* Local functions.  Order of declaration is confused.  FIXME.  */
static int	fe_probe_fmv	( struct isa_device *, struct fe_softc * );
static int	fe_probe_ati	( struct isa_device *, struct fe_softc * );
static int	fe_probe_mbh	( struct isa_device *, struct fe_softc * );
static void	fe_init_mbh	( struct fe_softc * );
static int	fe_get_packet	( struct fe_softc *, u_short );
static void	fe_stop		( int );
static void	fe_tint		( struct fe_softc *, u_char );
static void	fe_rint		( struct fe_softc *, u_char );
static void	fe_xmit		( struct fe_softc * );
static void	fe_write_mbufs	( struct fe_softc *, struct mbuf * );
static struct fe_filter
		fe_mcaf		( struct fe_softc * );
static int	fe_hash		( u_char * );
static void	fe_setmode	( struct fe_softc * );
static void	fe_loadmar	( struct fe_softc * );
static void	fe_setlinkaddr	( struct fe_softc * );
#if FE_DEBUG >= 1
static void	fe_dump		( int, struct fe_softc *, char * );
#endif

/* Ethernet constants.  To be defined in if_ehter.h?  FIXME.  */
#define ETHER_MIN_LEN	60	/* with header, without CRC. */
#define ETHER_MAX_LEN	1514	/* with header, without CRC. */
#define ETHER_ADDR_LEN	6	/* number of bytes in an address.  */
#define ETHER_TYPE_LEN	2	/* number of bytes in a data type field.  */
#define ETHER_HDR_SIZE	14	/* src addr, dst addr, and data type.  */
#define ETHER_CRC_LEN	4	/* number of bytes in CRC field.  */

/* Driver struct used in the config code.  This must be public (external.)  */
struct isa_driver fedriver =
{
	fe_probe,
	fe_attach,
	"fe",
	0		/* Assume we are insensitive.  FIXME.  */
};

/* Initial value for a kdc struct.  */
static struct kern_devconf const fe_kdc_template =
{
	0, 0, 0,
	"fe", 0, { MDDT_ISA, 0, "net" },
	isa_generic_externalize, 0, 0, ISA_EXTERNALLEN,
	&kdc_isa0,		/* We are an ISA device.  */
	0,
	DC_UNCONFIGURED,	/* Not yet configured.  */
	"Ethernet (fe)",	/* Tentative description (filled in later.)  */
	DC_CLS_NETIF		/* We are a network interface.  */
};

/*
 * Fe driver specific constants which relate to 86960/86965.
 * They are here (not in if_fereg.h), since selection of those
 * values depend on driver design.  I want to keep definitions in
 * if_fereg.h "clean", so that if someone wrote another driver
 * for 86960/86965, if_fereg.h were usable unchanged.
 *
 * The above statement sounds somothing like it's better to name
 * it "ic/mb86960.h" but "if_fereg.h"...  Should I do so?  FIXME.
 */

/* Interrupt masks  */
#define FE_TMASK ( FE_D2_COLL16 | FE_D2_TXDONE )
#define FE_RMASK ( FE_D3_OVRFLO | FE_D3_CRCERR \
		 | FE_D3_ALGERR | FE_D3_SRTPKT | FE_D3_PKTRDY )

/* Maximum number of iterrations for a receive interrupt.  */
#define FE_MAX_RECV_COUNT ( ( 65536 - 2048 * 2 ) / 64 )
	/* Maximum size of SRAM is 65536,
	 * minimum size of transmission buffer in fe is 2x2KB,
	 * and minimum amount of received packet including headers
	 * added by the chip is 64 bytes.
	 * Hence FE_MAX_RECV_COUNT is the upper limit for number
	 * of packets in the receive buffer.  */

/*
 * Convenient routines to access contiguous I/O ports.
 */

static INLINE void
inblk ( u_short addr, u_char * mem, int len )
{
	while ( --len >= 0 ) {
		*mem++ = inb( addr++ );
	}
}

static INLINE void
outblk ( u_short addr, u_char const * mem, int len )
{
	while ( --len >= 0 ) {
		outb( addr++, *mem++ );
	}
}

/*
 * Hardware probe routines.
 */

/* How and where to probe; to support automatic I/O address detection.  */
struct fe_probe_list
{
	int ( * probe ) ( struct isa_device *, struct fe_softc * );
	u_short const * addresses;
};

/* Lists of possible addresses.  */
static u_short const fe_fmv_addr [] =
	{ 0x220, 0x240, 0x260, 0x280, 0x2A0, 0x2C0, 0x300, 0x340, 0 };
static u_short const fe_ati_addr [] =
	{ 0x240, 0x260, 0x280, 0x2A0, 0x300, 0x320, 0x340, 0x380, 0 };

static struct fe_probe_list const fe_probe_list [] =
{
	{ fe_probe_fmv, fe_fmv_addr },
	{ fe_probe_ati, fe_ati_addr },
	{ fe_probe_mbh, NULL },  /* PCMCIAs cannot be auto-detected.  */
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

int
fe_probe ( struct isa_device * isa_dev )
{
	struct fe_softc * sc, * u;
	int nports;
	struct fe_probe_list const * list;
	u_short const * addr;
	u_short single [ 2 ];

	/* Initialize "minimum" parts of our softc.  */
	sc = &fe_softc[ isa_dev->id_unit ];
	sc->sc_unit = isa_dev->id_unit;

#if FE_DEBUG >= 2
	log( LOG_INFO, "fe%d: %s\n", sc->sc_unit, fe_version );
#endif

#ifndef DEV_LKM
	/* Fill the device config data and register it.  */
	sc->kdc = fe_kdc_template;
	sc->kdc.kdc_unit = sc->sc_unit;
	sc->kdc.kdc_parentdata = isa_dev;
	dev_attach( &sc->kdc );
#endif

	/* Probe each possibility, one at a time.  */
	for ( list = fe_probe_list; list->probe != NULL; list++ ) {

		if ( isa_dev->id_iobase != NO_IOADDR ) {
			/* Probe one specific address.  */
			single[ 0 ] = isa_dev->id_iobase;
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

			/* Don't probe already used address.  */
			for ( u = &fe_softc[0]; u < &fe_softc[NFE]; u++ ) {
				if ( u->addr == *addr ) break;
			}
			if ( u < &fe_softc[NFE] ) continue;

			/* Probe an address.  */
			sc->addr = *addr;
			nports = list->probe( isa_dev, sc );
			if ( nports > 0 ) {
				/* Found.  */
				isa_dev->id_iobase = *addr;
				return ( nports );
			}

			/* Try next.  */
			sc->addr = 0;
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

static INLINE int
fe_simple_probe ( u_short addr, struct fe_simple_probe_struct const * sp )
{
	struct fe_simple_probe_struct const * p;

	for ( p = sp; p->mask != 0; p++ ) {
		if ( ( inb( addr + p->port ) & p->mask ) != p->bits ) {
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

static INLINE void
strobe ( u_short bmpr16 )
{
	/*
	 * Output same value twice.  To speed-down execution?
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
	u_short bmpr16 = sc->addr + FE_BMPR16;
	u_short bmpr17 = sc->addr + FE_BMPR17;
	u_char n, val, bit;
	u_char save16, save17;

	/* Save old values of the registers.  */
	save16 = inb( bmpr16 );
	save17 = inb( bmpr17 );

	/* Read bytes from EEPROM; two bytes per an iterration.  */
	for ( n = 0; n < FE_EEPROM_SIZE / 2; n++ ) {

		/* Reset the EEPROM interface.  */
		outb( bmpr16, 0x00 );
		outb( bmpr17, 0x00 );
		outb( bmpr16, FE_B16_SELECT );

		/* Start EEPROM access.  */
		outb( bmpr17, FE_B17_DATA );
		strobe( bmpr16 );

		/* Pass the iterration count to the chip.  */
		val = 0x80 | n;
		for ( bit = 0x80; bit != 0x00; bit >>= 1 ) {
			outb( bmpr17, ( val & bit ) ? FE_B17_DATA : 0 );
			strobe( bmpr16 );
		}
		outb( bmpr17, 0x00 );

		/* Read a byte.  */
		val = 0;
		for ( bit = 0x80; bit != 0x00; bit >>= 1 ) {
			strobe( bmpr16 );
			if ( inb( bmpr17 ) & FE_B17_DATA ) {
				val |= bit;
			}
		}
		*data++ = val;

		/* Read one more byte.  */
		val = 0;
		for ( bit = 0x80; bit != 0x00; bit >>= 1 ) {
			strobe( bmpr16 );
			if ( inb( bmpr17 ) & FE_B17_DATA ) {
				val |= bit;
			}
		}
		*data++ = val;
	}

	/* Restore register values, in the case we had no 86965.  */
	outb( bmpr16, save16 );
	outb( bmpr17, save17 );

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

/*
 * Probe and initialization for Fujitsu FMV-180 series boards
 */
static int
fe_probe_fmv ( struct isa_device *isa_dev, struct fe_softc * sc )
{
	int i, n;

	static u_short const ioaddr [ 8 ] =
		{ 0x220, 0x240, 0x260, 0x280, 0x2A0, 0x2C0, 0x300, 0x340 };
	static u_short const irqmap [ 4 ] =
		{ IRQ3,  IRQ7,  IRQ10, IRQ15 };

	static struct fe_simple_probe_struct const probe_table [] = {
		{ FE_DLCR2, 0x70, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
	    /*	{ FE_DLCR5, 0x80, 0x00 },	Doesn't work.  */

		{ FE_FMV0, FE_FMV0_MAGIC_MASK,  FE_FMV0_MAGIC_VALUE },
		{ FE_FMV1, FE_FMV1_CARDID_MASK, FE_FMV1_CARDID_ID   },
		{ FE_FMV3, FE_FMV3_EXTRA_MASK,  FE_FMV3_EXTRA_VALUE },
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
	 * We can always verify the *first* 2 bits (in Ehternet
	 * bit order) are "no multicast" and "no local" even for
	 * unknown vendors.
	 */
		{ FE_FMV4, 0x03, 0x00 },
#endif
		{ 0 }
	};

#if 0
	/*
	 * Dont probe at all if the config says we are PCMCIA...
	 */
	if ( isa_dev->id_flags & FE_FLAGS_PCMCIA ) return ( 0 );
#endif

	/*
	 * See if the sepcified address is possible for FMV-180 series.
	 */
	for ( i = 0; i < 8; i++ ) {
		if ( ioaddr[ i ] == sc->addr ) break;
	}
	if ( i == 8 ) return 0;

	/* Simple probe.  */
	if ( !fe_simple_probe( sc->addr, probe_table ) ) return 0;

	/* Check if our I/O address matches config info on EEPROM.  */
	n = ( inb( sc->addr + FE_FMV2 ) & FE_FMV2_ADDR ) >> FE_FMV2_ADDR_SHIFT;
	if ( ioaddr[ n ] != sc->addr ) return 0;

	/* Determine the card type.  */
	switch ( inb( sc->addr + FE_FMV0 ) & FE_FMV0_MODEL ) {
	  case FE_FMV0_MODEL_FMV181:
		sc->type = FE_TYPE_FMV181;
		sc->typestr = "FMV-181";
		sc->sc_description = "Ethernet adapter: FMV-181";
		break;
	  case FE_FMV0_MODEL_FMV182:
		sc->type = FE_TYPE_FMV182;
		sc->typestr = "FMV-182";
		sc->sc_description = "Ethernet adapter: FMV-182";
		break;
	  default:
	  	/* Unknown card type: maybe a new model, but...  */
		return 0;
	}

	/*
	 * An FMV-180 has successfully been proved.
	 * Determine which IRQ to be used.
	 *
	 * In this version, we always get an IRQ assignment from the
	 * FMV-180's configuration EEPROM, ignoring that specified in
	 * config file.
	 */
	n = ( inb( sc->addr + FE_FMV2 ) & FE_FMV2_IRQ ) >> FE_FMV2_IRQ_SHIFT;
	isa_dev->id_irq = irqmap[ n ];

	/*
	 * Initialize constants in the per-line structure.
	 */

	/* Get our station address from EEPROM.  */
	inblk( sc->addr + FE_FMV4, sc->sc_enaddr, ETHER_ADDR_LEN );

	/* Make sure we got a valid station address.  */
	if ( ( sc->sc_enaddr[ 0 ] & 0x03 ) != 0x00
	  || ( sc->sc_enaddr[ 0 ] == 0x00
	    && sc->sc_enaddr[ 1 ] == 0x00
	    && sc->sc_enaddr[ 2 ] == 0x00 ) ) return 0;

	/* Register values which depend on board design.  */
	sc->proto_dlcr4 = FE_D4_LBC_DISABLE | FE_D4_CNTRL;
	sc->proto_dlcr5 = 0;
	sc->proto_dlcr7 = FE_D7_BYTSWP_LH | FE_D7_IDENT_EC;

	/*
	 * Program the 86960 as follows:
	 *	SRAM: 32KB, 100ns, byte-wide access.
	 *	Transmission buffer: 4KB x 2.
	 *	System bus interface: 16 bits.
	 * We cannot change these values but TXBSIZE, because they
	 * are hard-wired on the board.  Modifying TXBSIZE will affect
	 * the driver performance.
	 */
	sc->proto_dlcr6 = FE_D6_BUFSIZ_32KB | FE_D6_TXBSIZ_2x4KB
		| FE_D6_BBW_BYTE | FE_D6_SBW_WORD | FE_D6_SRAM_100ns;

	/*
	 * Minimum initialization of the hardware.
	 * We write into registers; hope I/O ports have no
	 * overlap with other boards.
	 */

	/* Initialize ASIC.  */
	outb( sc->addr + FE_FMV3, 0 );
	outb( sc->addr + FE_FMV10, 0 );

	/* Wait for a while.  I'm not sure this is necessary.  FIXME.  */
	DELAY(200);

	/* Initialize 86960.  */
	outb( sc->addr + FE_DLCR6, sc->proto_dlcr6 | FE_D6_DLC_DISABLE );
	DELAY(200);

	/* Disable all interrupts.  */
	outb( sc->addr + FE_DLCR2, 0 );
	outb( sc->addr + FE_DLCR3, 0 );

	/* Turn the "master interrupt control" flag of ASIC on.  */
	outb( sc->addr + FE_FMV3, FE_FMV3_ENABLE_FLAG );

	/*
	 * That's all.  FMV-180 occupies 32 I/O addresses, by the way.
	 */
	return 32;
}

/*
 * Probe and initialization for Allied-Telesis AT1700/RE2000 series.
 */
static int
fe_probe_ati ( struct isa_device * isa_dev, struct fe_softc * sc )
{
	int i, n;
	u_char eeprom [ FE_EEPROM_SIZE ];

	static u_short const ioaddr [ 8 ] =
		{ 0x260, 0x280, 0x2A0, 0x240, 0x340, 0x320, 0x380, 0x300 };
	static u_short const irqmap_lo [ 4 ] =
		{ IRQ3,  IRQ4,  IRQ5,  IRQ9 };
	static u_short const irqmap_hi [ 4 ] =
		{ IRQ10, IRQ11, IRQ12, IRQ15 };
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

#if 0
	/*
	 * Don't probe at all if the config says we are PCMCIA...
	 */
	if ( isa_dev->id_flags & FE_FLAGS_PCMCIA ) return ( 0 );
#endif

#if FE_DEBUG >= 3
	log( LOG_INFO, "fe%d: probe (0x%x) for ATI\n", sc->sc_unit, sc->addr );
	fe_dump( LOG_INFO, sc, NULL );
#endif

	/*
	 * See if the sepcified address is possible for MB86965A JLI mode.
	 */
	for ( i = 0; i < 8; i++ ) {
		if ( ioaddr[ i ] == sc->addr ) break;
	}
	if ( i == 8 ) return 0;

	/*
	 * We should test if MB86965A is on the base address now.
	 * Unfortunately, it is very hard to probe it reliably, since
	 * we have no way to reset the chip under software control.
	 * On cold boot, we could check the "signature" bit patterns
	 * described in the Fujitsu document.  On warm boot, however,
	 * we can predict almost nothing about register values.
	 */
	if ( !fe_simple_probe( sc->addr, probe_table ) ) return 0;

	/* Check if our I/O address matches config info on 86965.  */
	n = ( inb( sc->addr + FE_BMPR19 ) & FE_B19_ADDR ) >> FE_B19_ADDR_SHIFT;
	if ( ioaddr[ n ] != sc->addr ) return 0;

	/*
	 * We are now almost sure we have an AT1700 at the given
	 * address.  So, read EEPROM through 86965.  We have to write
	 * into LSI registers to read from EEPROM.  I want to avoid it
	 * at this stage, but I cannot test the presense of the chip
	 * any further without reading EEPROM.  FIXME.
	 */
	fe_read_eeprom( sc, eeprom );

	/* Make sure that config info in EEPROM and 86965 agree.  */
	if ( eeprom[ FE_EEPROM_CONF ] != inb( sc->addr + FE_BMPR19 ) ) {
		return 0;
	}

	/*
	 * Determine the card type.
	 * There may be a way to identify various models.  FIXME.
	 */
	sc->type = FE_TYPE_AT1700;
	sc->typestr = "AT1700/RE2000";
	sc->sc_description = "Ethernet adapter: AT1700 or RE2000";

	/*
	 * I was told that RE2000 series has two variants on IRQ
	 * selection.  They are 3/4/5/9 and 10/11/12/15.  I don't know
	 * how we can distinguish which model is which.  For now, we
	 * just trust irq setting in config.  FIXME.
	 *
	 * I've heard that ATI puts an identification between these
	 * two models in the EEPROM.  Sounds reasonable.  I've also
	 * heard that Linux driver for AT1700 tests it.  O.K.  Let's
	 * try using it and see what happens.  Anyway, we will use an
	 * IRQ value passed by config (i.e., user), if one is
	 * available.  FIXME.
	 */
	n = ( inb( sc->addr + FE_BMPR19 ) & FE_B19_IRQ ) >> FE_B19_IRQ_SHIFT;
	if ( isa_dev->id_irq == 0 ) {
		/* Try to determine IRQ settings.  */
		if ( eeprom[ FE_EEP_ATI_TYPE ] & FE_EEP_ATI_TYPE_HIGHIRQ ) {
			isa_dev->id_irq = irqmap_hi[ n ];
		} else {
			isa_dev->id_irq = irqmap_lo[ n ];
		}
	}

	/*
	 * Initialize constants in the per-line structure.
	 */

	/* Get our station address from EEPROM.  */
	bcopy( eeprom + FE_EEP_ATI_ADDR, sc->sc_enaddr, ETHER_ADDR_LEN );

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

	/* Should find all register prototypes here.  FIXME.  */
	sc->proto_dlcr4 = FE_D4_LBC_DISABLE | FE_D4_CNTRL;  /* FIXME */
	sc->proto_dlcr5 = 0;
	sc->proto_dlcr7 = FE_D7_BYTSWP_LH | FE_D7_IDENT_EC;

	/*
	 * Program the 86960 as follows:
	 *	SRAM: 32KB, 100ns, byte-wide access.
	 *	Transmission buffer: 4KB x 2.
	 *	System bus interface: 16 bits.
	 * We cannot change these values but TXBSIZE, because they
	 * are hard-wired on the board.  Modifying TXBSIZE will affect
	 * the driver performance.
	 */
	sc->proto_dlcr6 = FE_D6_BUFSIZ_32KB | FE_D6_TXBSIZ_2x4KB
		| FE_D6_BBW_BYTE | FE_D6_SBW_WORD | FE_D6_SRAM_100ns;

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, "ATI found" );
#endif

	/* Initialize 86965.  */
	outb( sc->addr + FE_DLCR6, sc->proto_dlcr6 | FE_D6_DLC_DISABLE );
	DELAY(200);

	/* Disable all interrupts.  */
	outb( sc->addr + FE_DLCR2, 0 );
	outb( sc->addr + FE_DLCR3, 0 );

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, "end of fe_probe_ati()" );
#endif

	/*
	 * That's all.  AT1700 occupies 32 I/O addresses, by the way.
	 */
	return 32;
}

/*
 * Probe and initialization for Fujitsu MBH10302 PCMCIA Ethernet interface.
 */
static int
fe_probe_mbh ( struct isa_device * isa_dev, struct fe_softc * sc )
{
	static struct fe_simple_probe_struct probe_table [] = {
		{ FE_DLCR2, 0x70, 0x00 },
		{ FE_DLCR4, 0x08, 0x00 },
	    /*	{ FE_DLCR5, 0x80, 0x00 },	Does not work well.  */
#if 0
	/*
	 * Test *vendor* part of the address for Fujitsu.
	 * The test will gain reliability of probe process, but
	 * it rejects clones by other vendors, or OEM product
	 * supplied by resalers other than Fujitsu.
	 */
		{ FE_MBH10, 0xFF, 0x00 },
		{ FE_MBH11, 0xFF, 0x00 },
		{ FE_MBH12, 0xFF, 0x0E },
#else
	/*
	 * We can always verify the *first* 2 bits (in Ehternet
	 * bit order) are "global" and "unicast" even for
	 * unknown vendors.
	 */
		{ FE_MBH10, 0x03, 0x00 },
#endif
        /* Just a gap?  Seems reliable, anyway.  */
		{ 0x12, 0xFF, 0x00 },
		{ 0x13, 0xFF, 0x00 },
		{ 0x14, 0xFF, 0x00 },
		{ 0x15, 0xFF, 0x00 },
		{ 0x16, 0xFF, 0x00 },
		{ 0x17, 0xFF, 0x00 },
		{ 0x18, 0xFF, 0xFF },
		{ 0x19, 0xFF, 0xFF },

		{ 0 }
	};

#if 0
	/*
	 * We need a PCMCIA flag.
	 */
	if ( ( isa_dev->id_flags & FE_FLAGS_PCMCIA ) == 0 ) return ( 0 );
#endif

	/*
	 * We need explicit IRQ and supported address.
	 */
	if ( isa_dev->id_irq == 0 || ( sc->addr & ~0x3E0 ) != 0 ) return ( 0 );

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, "top of probe" );
#endif

	/*
	 * See if MBH10302 is on its address.
	 * I'm not sure the following probe code works.  FIXME.
	 */
	if ( !fe_simple_probe( sc->addr, probe_table ) ) return 0;

	/* Determine the card type.  */
	sc->type = FE_TYPE_MBH10302;
	sc->typestr = "MBH10302 (PCMCIA)";
	sc->sc_description = "Ethernet adapter: MBH10302 (PCMCIA)";

	/*
	 * Initialize constants in the per-line structure.
	 */

	/* Get our station address from EEPROM.  */
	inblk( sc->addr + FE_MBH10, sc->sc_enaddr, ETHER_ADDR_LEN );

	/* Make sure we got a valid station address.  */
	if ( ( sc->sc_enaddr[ 0 ] & 0x03 ) != 0x00
	  || ( sc->sc_enaddr[ 0 ] == 0x00
	    && sc->sc_enaddr[ 1 ] == 0x00
	    && sc->sc_enaddr[ 2 ] == 0x00 ) ) return 0;

	/* Should find all register prototypes here.  FIXME.  */
	sc->proto_dlcr4 = FE_D4_LBC_DISABLE | FE_D4_CNTRL;
	sc->proto_dlcr5 = 0;
	sc->proto_dlcr7 = FE_D7_BYTSWP_LH | FE_D7_IDENT_NICE;

	/*
	 * Program the 86960 as follows:
	 *	SRAM: 32KB, 100ns, byte-wide access.
	 *	Transmission buffer: 4KB x 2.
	 *	System bus interface: 16 bits.
	 * We cannot change these values but TXBSIZE, because they
	 * are hard-wired on the board.  Modifying TXBSIZE will affect
	 * the driver performance.
	 */
	sc->proto_dlcr6 = FE_D6_BUFSIZ_32KB | FE_D6_TXBSIZ_2x4KB
		| FE_D6_BBW_BYTE | FE_D6_SBW_WORD | FE_D6_SRAM_100ns;

	/* Setup hooks.  We need a special initialization procedure.  */
	sc->init = fe_init_mbh;

	/*
	 * Minimum initialization.
	 */

	/* Wait for a while.  I'm not sure this is necessary.  FIXME.  */
	DELAY(200);

	/* Minimul initialization of 86960.  */
	outb( sc->addr + FE_DLCR6, sc->proto_dlcr6 | FE_D6_DLC_DISABLE );
	DELAY( 200 );

	/* Disable all interrupts.  */
	outb( sc->addr + FE_DLCR2, 0 );
	outb( sc->addr + FE_DLCR3, 0 );

#if 1	/* FIXME.  */
	/* Initialize system bus interface and encoder/decoder operation.  */
	outb( sc->addr + FE_MBH0, FE_MBH0_MAGIC | FE_MBH0_INTR_DISABLE );
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
	/* Probably required after hot-insertion...  */

	/* Wait for a while.  I'm not sure this is necessary.  FIXME.  */
	DELAY(200);

	/* Minimul initialization of 86960.  */
	outb( sc->addr + FE_DLCR6, sc->proto_dlcr6 | FE_D6_DLC_DISABLE );
	DELAY( 200 );

	/* Disable all interrupts.  */
	outb( sc->addr + FE_DLCR2, 0 );
	outb( sc->addr + FE_DLCR3, 0 );

	/* Enable master interrupt flag.  */
	outb( sc->addr + FE_MBH0, FE_MBH0_MAGIC | FE_MBH0_INTR_ENABLE );
}

/*
 * Install interface into kernel networking data structures
 */
int
fe_attach ( struct isa_device *isa_dev )
{
	struct fe_softc *sc = &fe_softc[isa_dev->id_unit];

	/*
	 * Initialize ifnet structure
	 */
	sc->sc_if.if_unit     = sc->sc_unit;
	sc->sc_if.if_name     = "fe";
	sc->sc_if.if_init     = fe_init;
	sc->sc_if.if_output   = ether_output;
	sc->sc_if.if_start    = fe_start;
	sc->sc_if.if_ioctl    = fe_ioctl;
	sc->sc_if.if_reset    = fe_reset;
	sc->sc_if.if_watchdog = fe_watchdog;

	/*
	 * Set default interface flags.
	 */
	sc->sc_if.if_flags = IFF_BROADCAST | IFF_MULTICAST;

	/*
	 * Set maximum size of output queue, if it has not been set.
	 * It is done here as this driver may be started after the
	 * system intialization (i.e., the interface is PCMCIA.)
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
	if ( isa_dev->id_flags & FE_FLAGS_OVERRIDE_DLCR6 ) {
		sc->proto_dlcr6 = isa_dev->id_flags & FE_FLAGS_DLCR6_VALUE;
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

	/* Attach and stop the interface.  */
	if_attach( &sc->sc_if );
	fe_stop( sc->sc_unit );		/* This changes the state to IDLE.  */
	fe_setlinkaddr( sc );

	/* Print additional info when attached.  */
	printf( "fe%d: address %s, type %s\n", sc->sc_unit,
		ether_sprintf( sc->sc_enaddr ), sc->typestr );
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
	bpfattach(&sc->bpf, &sc->sc_if, DLT_EN10MB,
		  sizeof(struct ether_header));
#endif
	return 1;
}

/*
 * Reset interface.
 */
void
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
void
fe_stop ( int unit )
{
	struct fe_softc *sc = &fe_softc[unit];
	int s;

	s = splimp();

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, "stop()" );
#endif

	/* Disable interrupts.  */
	outb( sc->addr + FE_DLCR2, 0x00 );
	outb( sc->addr + FE_DLCR3, 0x00 );

	/* Stop interface hardware.  */
	DELAY( 200 );
	outb( sc->addr + FE_DLCR6, sc->proto_dlcr6 | FE_D6_DLC_DISABLE );
	DELAY( 200 );

	/* Clear all interrupt status.  */
	outb( sc->addr + FE_DLCR0, 0xFF );
	outb( sc->addr + FE_DLCR1, 0xFF );

	/* Put the chip in stand-by mode.  */
	DELAY( 200 );
	outb( sc->addr + FE_DLCR7, sc->proto_dlcr7 | FE_D7_POWER_DOWN );
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
	sc->sc_dcstate = DC_IDLE;

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
void
fe_watchdog ( int unit )
{
	struct fe_softc *sc = &fe_softc[unit];

#if FE_DEBUG >= 1
	log( LOG_ERR, "fe%d: transmission timeout (%d+%d)%s\n",
		unit, sc->txb_sched, sc->txb_count,
		( sc->sc_if.if_flags & IFF_UP )	? "" : " when down" );
#endif
#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, NULL );
#endif

	/* Record how many packets are lost by this accident.  */
	sc->sc_if.if_oerrors += sc->txb_sched + sc->txb_count;

	/* Put the interface into known initial state.  */
	if ( sc->sc_if.if_flags & IFF_UP ) {
		fe_reset( unit );
	} else {
		fe_stop( unit );
	}
}

/*
 * Initialize device.
 */
void
fe_init ( int unit )
{
	struct fe_softc *sc = &fe_softc[unit];
	int i, s;

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, "init()" );
#endif

	/* We need an address. */
	if (sc->sc_if.if_addrlist == 0) {
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
		log( LOG_ERR, "fe%d: invalid station address (%s)\n",
			sc->sc_unit, ether_sprintf( sc->sc_enaddr ) );
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
	outb( sc->addr + FE_DLCR6, sc->proto_dlcr6 | FE_D6_DLC_DISABLE );

	/* Power up the chip and select register bank for DLCRs.  */
	DELAY(200);
	outb( sc->addr + FE_DLCR7,
		sc->proto_dlcr7 | FE_D7_RBS_DLCR | FE_D7_POWER_UP );
	DELAY(200);

	/* Feed the station address.  */
	outblk( sc->addr + FE_DLCR8, sc->sc_enaddr, ETHER_ADDR_LEN );

	/* Clear multicast address filter to receive nothing.  */
	outb( sc->addr + FE_DLCR7,
		sc->proto_dlcr7 | FE_D7_RBS_MAR | FE_D7_POWER_UP );
	outblk( sc->addr + FE_MAR8, fe_filter_nothing.data, FE_FILTER_LEN );

	/* Select the BMPR bank for runtime register access.  */
	outb( sc->addr + FE_DLCR7,
		sc->proto_dlcr7 | FE_D7_RBS_BMPR | FE_D7_POWER_UP );

	/* Initialize registers.  */
	outb( sc->addr + FE_DLCR0, 0xFF );	/* Clear all bits.  */
	outb( sc->addr + FE_DLCR1, 0xFF );	/* ditto.  */
	outb( sc->addr + FE_DLCR2, 0x00 );
	outb( sc->addr + FE_DLCR3, 0x00 );
	outb( sc->addr + FE_DLCR4, sc->proto_dlcr4 );
	outb( sc->addr + FE_DLCR5, sc->proto_dlcr5 );
	outb( sc->addr + FE_BMPR10, 0x00 );
	outb( sc->addr + FE_BMPR11, FE_B11_CTRL_SKIP );
	outb( sc->addr + FE_BMPR12, 0x00 );
	outb( sc->addr + FE_BMPR13, FE_B13_TPTYPE_UTP | FE_B13_PORT_AUTO );
	outb( sc->addr + FE_BMPR14, 0x00 );
	outb( sc->addr + FE_BMPR15, 0x00 );

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, "just before enabling DLC" );
#endif

	/* Enable interrupts.  */
	outb( sc->addr + FE_DLCR2, FE_TMASK );
	outb( sc->addr + FE_DLCR3, FE_RMASK );

	/* Enable transmitter and receiver.  */
	DELAY(200);
	outb( sc->addr + FE_DLCR6, sc->proto_dlcr6 | FE_D6_DLC_ENABLE );
	DELAY(200);

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, "just after enabling DLC" );
#endif
	/*
	 * Make sure to empty the receive buffer.
	 *
	 * This may be redundant, but *if* the receive buffer were full
	 * at this point, the driver would hang.  I have experienced
	 * some strange hangups just after UP.  I hope the following
	 * code solve the problem.
	 *
	 * I have changed the order of hardware initialization.
	 * I think the receive buffer cannot have any packets at this
	 * point in this version.  The following code *must* be
	 * redundant now.  FIXME.
	 */
	for ( i = 0; i < FE_MAX_RECV_COUNT; i++ ) {
		if ( inb( sc->addr + FE_DLCR5 ) & FE_D5_BUFEMP ) break;
		outb( sc->addr + FE_BMPR14, FE_B14_SKIP );
	}
#if FE_DEBUG >= 1
	if ( i >= FE_MAX_RECV_COUNT ) {
		log( LOG_ERR, "fe%d: cannot empty receive buffer\n",
			sc->sc_unit );
	}
#endif
#if FE_DEBUG >= 3
	if ( i < FE_MAX_RECV_COUNT ) {
		log( LOG_INFO, "fe%d: receive buffer emptied (%d)\n",
			sc->sc_unit, i );
	}
#endif

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, "after ERB loop" );
#endif

	/* Do we need this here?  */
	outb( sc->addr + FE_DLCR0, 0xFF );	/* Clear all bits.  */
	outb( sc->addr + FE_DLCR1, 0xFF );	/* ditto.  */

#if FE_DEBUG >= 3
	fe_dump( LOG_INFO, sc, "after FIXME" );
#endif
	/* Set 'running' flag, because we are now running.   */
	sc->sc_if.if_flags |= IFF_RUNNING;

	/* Update device config status.  */
	sc->sc_dcstate = DC_BUSY;

	/*
	 * At this point, the interface is runnung properly,
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
static INLINE void
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

#if FE_DELAYED_PADDING
	/* Omit the postponed padding process.  */
	sc->txb_padding = 0;
#endif

	/* Start transmitter, passing packets in TX buffer.  */
	outb( sc->addr + FE_BMPR10, sc->txb_sched | FE_B10_START );
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
	struct fe_softc *sc = IFNET2SOFTC( ifp );
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
		 * If txb_count is incorrect, leaving it as is will cause
		 * sending of gabages after next interrupt.  We have to
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
	 * 86960 flushes the transmisstion buffer, so it is delayed
	 * until all buffered transmission packets have been sent
	 * out.
	 */
	if ( sc->filter_change ) {
		/*
		 * Filter change requst is delayed only when the DLC is
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
		if ( sc->txb_free < ETHER_MAX_LEN + FE_DATA_LEN_LEN ) {
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
		if ( sc->txb_sched == 0 ) fe_xmit( sc );

#if 0 /* Turned of, since our interface is now duplex.  */
		/*
		 * Tap off here if there is a bpf listener.
		 */
#if NBPFILTER > 0
		if ( sc->bpf ) bpf_mtap( sc->bpf, m );
#endif
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
static INLINE void
fe_droppacket ( struct fe_softc * sc )
{
	outb( sc->addr + FE_BMPR14, FE_B14_SKIP );
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
		left = inb( sc->addr + FE_BMPR10 );

#if FE_DEBUG >= 2
		log( LOG_WARNING, "fe%d: excessive collision (%d/%d)\n",
			sc->sc_unit, left, sc->txb_sched );
#endif
#if FE_DEBUG >= 3
		fe_dump( LOG_INFO, sc, NULL );
#endif

		/*
		 * Update statistics.
		 */
		sc->sc_if.if_collisions += 16;
		sc->sc_if.if_oerrors++;
		sc->sc_if.if_opackets += sc->txb_sched - left;

		/*
		 * Collision statistics has been updated.
		 * Clear the collision flag on 86960 now to avoid confusion.
		 */
		outb( sc->addr + FE_DLCR0, FE_D0_COLLID );

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
		outb( sc->addr + FE_BMPR11,
			FE_B11_CTRL_SKIP | FE_B11_MODE1 );
		sc->txb_sched = left - 1;
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
		 * transmission queue is clauded), 86960 informs us number
		 * of collisions occured on the last packet on the
		 * transmission only.  Number of collisions on previous
		 * packets are lost.  I have told that the fact is clearly
		 * stated in the Fujitsu document.
		 *
		 * I considered not to mind it seriously.  Collision
		 * count is not so important, anyway.  Any comments?  FIXME.
		 */

		if ( inb( sc->addr + FE_DLCR0 ) & FE_D0_COLLID ) {

			/* Clear collision flag.  */
			outb( sc->addr + FE_DLCR0, FE_D0_COLLID );

			/* Extract collision count from 86960.  */
			col = inb( sc->addr + FE_DLCR4 );
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
		 * Update total number of successfully
		 * transmitted packets.
		 */
		sc->sc_if.if_opackets += sc->txb_sched;
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
#if FE_DEBUG >= 3
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
	 * We just loop cheking the flag to pull out all received
	 * packets.
	 *
	 * We limit the number of iterrations to avoid inifnit-loop.
	 * It can be caused by a very slow CPU (some broken
	 * peripheral may insert incredible number of wait cycles)
	 * or, worse, by a broken MB86960 chip.
	 */
	for ( i = 0; i < FE_MAX_RECV_COUNT; i++ ) {

		/* Stop the iterration if 86960 indicates no packets.  */
		if ( inb( sc->addr + FE_DLCR5 ) & FE_D5_BUFEMP ) break;

		/*
		 * Extract A receive status byte.
		 * As our 86960 is in 16 bit bus access mode, we have to
		 * use inw() to get the status byte.  The significant
		 * value is returned in lower 8 bits.
		 */
		status = ( u_char )inw( sc->addr + FE_BMPR8 );
#if FE_DEBUG >= 4
		log( LOG_INFO, "fe%d: receive status = %04x\n",
			sc->sc_unit, status );
#endif

		/*
		 * If there was an error, update statistics and drop
		 * the packet, unless the interface is in promiscuous
		 * mode.
		 */
		if ( ( status & 0xF0 ) != 0x20 ) {
			if ( !( sc->sc_if.if_flags & IFF_PROMISC ) ) {
				sc->sc_if.if_ierrors++;
				fe_droppacket(sc);
				continue;
			}
		}

		/*
		 * Extract the packet length.
		 * It is a sum of a header (14 bytes) and a payload.
		 * CRC has been stripped off by the 86960.
		 */
		len = inw( sc->addr + FE_BMPR8 );

		/*
		 * MB86965 checks the packet length and drop big packet
		 * before passing it to us.  There are no chance we can
		 * get [crufty] packets.  Hence, if the length exceeds
		 * the specified limit, it means some serious failure,
		 * such as out-of-sync on receive buffer management.
		 *
		 * Is this statement true?  FIXME.
		 */
		if ( len > ETHER_MAX_LEN || len < ETHER_HDR_SIZE ) {
#if FE_DEBUG >= 2
			log( LOG_WARNING,
				"fe%d: received a %s packet? (%u bytes)\n",
				sc->sc_unit,
				len < ETHER_HDR_SIZE ? "partial" : "big",
				len );
#endif
			sc->sc_if.if_ierrors++;
			fe_droppacket( sc );
			continue;
		}

		/*
		 * Check for a short (RUNT) packet.  We *do* check
		 * but do nothing other than print a message.
		 * Short packets are illegal, but does nothing bad
		 * if it carries data for upper layer.
		 */
#if FE_DEBUG >= 2
		if ( len < ETHER_MIN_LEN ) {
			log( LOG_WARNING,
			     "fe%d: received a short packet? (%u bytes)\n",
			     sc->sc_unit, len );
		}
#endif

		/*
		 * Go get a packet.
		 */
		if ( fe_get_packet( sc, len ) < 0 ) {
			/* Skip a packet, updating statistics.  */
#if FE_DEBUG >= 2
			log( LOG_WARNING, "%s%d: no enough mbuf;"
			    " a packet (%u bytes) dropped\n",
			    sc->sc_unit, len );
#endif
			sc->sc_if.if_ierrors++;
			fe_droppacket( sc );

			/*
			 * We stop receiving packets, even if there are
			 * more in the buffer.  We hope we can get more
			 * mbuf next time.
			 */
			return;
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
		tstat = inb( sc->addr + FE_DLCR0 ) & FE_TMASK;
		rstat = inb( sc->addr + FE_DLCR1 ) & FE_RMASK;
		if ( tstat == 0 && rstat == 0 ) break;

		/*
		 * Reset the conditions we are acknowledging.
		 */
		outb( sc->addr + FE_DLCR0, tstat );
		outb( sc->addr + FE_DLCR1, rstat );

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
		 * packet lossage.
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
int
fe_ioctl ( struct ifnet *ifp, int command, caddr_t data )
{
	struct fe_softc *sc = IFNET2SOFTC( ifp );
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
			fe_init( sc->sc_unit );	/* before arpwhohas */
			arp_ifinit( &sc->arpcom, ifa );
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

#ifdef SIOCSIFPHYSADDR
	  case SIOCSIFPHYSADDR:
	    {
		/*
		 * Set the physical (Ehternet) address of the interface.
		 * When and by whom is this command used?  FIXME.
		 */
		struct ifreq * ifr = ( struct ifreq * )data;

		bcopy((caddr_t)&ifr->ifr_data,
		      (caddr_t)sc->sc_enaddr, ETHER_ADDR_LEN);
		fe_setlinkaddr( sc );
		break;
	    }
#endif

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
	    {
		/*
		 * Update out multicast list.
		 */
		struct ifreq * ifr = ( struct ifreq * )data;

		error = ( command == SIOCADDMULTI )
		      ? ether_addmulti( ifr, &sc->arpcom )
		      : ether_delmulti( ifr, &sc->arpcom );

		if ( error == ENETRESET ) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			fe_setmode( sc );
			error = 0;
		}

		break;
	    }
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
 * Retreive packet from receive buffer and send to the next level up via
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
#if ( MCLBYTES < ETHER_MAX_LEN + NFS_MAGIC_OFFSET )
#error "Too small MCLBYTES to use fe driver."
#endif

	/*
	 * Our strategy has one more problem.  There is a policy on
	 * mbuf cluster allocation.  It says that we must have at
	 * least MINCLSIZE (208 bytes on FreeBSD 2.0 for x86) to
	 * allocate a cluster.  For a packet of a size between
	 * (MHLEN - 2) to (MINCLSIZE - 2), our code violates the rule...
	 * On the other hand, the current code is short, simle,
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

	/* The following sillines is to make NFS happy */
	m->m_data += NFS_MAGIC_OFFSET;

	/* Get a packet.  */
	insw( sc->addr + FE_BMPR8, m->m_data, ( len + 1 ) >> 1 );

	/* Get (actually just point to) the header part.  */
	eh = mtod( m, struct ether_header *);

#define ETHER_ADDR_IS_MULTICAST(A) (*(char *)(A) & 1)

#if NBPFILTER > 0
	/*
	 * Check if there's a BPF listener on this interface.
	 * If it is, hand off the raw packet to bpf.
	 */
	if ( sc->bpf ) {
		bpf_mtap( sc->bpf, m );
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
			"fe%d: got an unwanted packet, dst = %s\n",
			sc->sc_unit,
			ether_sprintf( eh->ether_dhost ) );
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
 *
 * I wrote a code for an experimental "delayed padding" technique.
 * When employed, it postpones the padding process for short packets.
 * If xmit() occured at the moment, the padding process is omitted, and
 * garbages are sent as pad data.  If next packet is stored in the
 * transmission buffer before xmit(), write_mbuf() pads the previous
 * packet before transmitting new packet.  This *may* gain the
 * system performance (slightly).
 */
static void
fe_write_mbufs ( struct fe_softc *sc, struct mbuf *m )
{
	u_short addr_bmpr8 = sc->addr + FE_BMPR8;
	u_short length, len;
	short pad;
	struct mbuf *mp;
	u_char *data;
	u_short savebyte;	/* WARNING: Architecture dependent!  */
#define NO_PENDING_BYTE 0xFFFF

#if FE_DELAYED_PADDING
	/* Do the "delayed padding."  */
	pad = sc->txb_padding >> 1;
	if ( pad > 0 ) {
		while ( --pad >= 0 ) {
			outw( addr_bmpr8, 0 );
		}
		sc->txb_padding = 0;
	}
#endif

#if FE_DEBUG >= 2
	/* First, count up the total number of bytes to copy */
	length = 0;
	for ( mp = m; mp != NULL; mp = mp->m_next ) {
		length += mp->m_len;
	}
	/* Check if this matches the one in the packet header.  */
	if ( length != m->m_pkthdr.len ) {
		log( LOG_WARNING, "fe%d: packet length mismatch? (%d/%d)\n",
			sc->sc_unit, length, m->m_pkthdr.len );
	}
#else
	/* Just use the length value in the packet header.  */
	length = m->m_pkthdr.len;
#endif

#if FE_DEBUG >= 1
	/*
	 * Should never send big packets.  If such a packet is passed,
	 * it should be a bug of upper layer.  We just ignore it.
	 * ... Partial (too short) packets, neither.
	 */
	if ( length > ETHER_MAX_LEN || length < ETHER_HDR_SIZE ) {
		log( LOG_ERR,
			"fe%d: got a %s packet (%u bytes) to send\n",
			sc->sc_unit,
			length < ETHER_HDR_SIZE ? "partial" : "big", length );
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
	outw( addr_bmpr8, max( length, ETHER_MIN_LEN ) );

	/*
	 * Update buffer status now.
	 * Truncate the length up to an even number, since we use outw().
	 */
	length = ( length + 1 ) & ~1;
	sc->txb_free -= FE_DATA_LEN_LEN + max( length, ETHER_MIN_LEN );
	sc->txb_count++;

#if FE_DELAYED_PADDING
	/* Postpone the packet padding if necessary.  */
	if ( length < ETHER_MIN_LEN ) {
		sc->txb_padding = ETHER_MIN_LEN - length;
	}
#endif

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

#if ! FE_DELAYED_PADDING
	/*
	 * Pad the packet to the minimum length if necessary.
	 */
	pad = ( ETHER_MIN_LEN >> 1 ) - ( length >> 1 );
	while ( --pad >= 0 ) {
		outw( addr_bmpr8, 0 );
	}
#endif
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
	struct ether_multi *enm;
	struct ether_multistep step;

	filter = fe_filter_nothing;
	ETHER_FIRST_MULTI(step, &sc->arpcom, enm);
	while ( enm != NULL) {
		if ( bcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN) ) {
			return ( fe_filter_all );
		}
		index = fe_hash( enm->enm_addrlo );
#if FE_DEBUG >= 4
		log( LOG_INFO, "fe%d: hash(%s) == %d\n",
			sc->sc_unit, ether_sprintf( enm->enm_addrlo ), index );
#endif

		filter.data[index >> 3] |= 1 << (index & 7);
		ETHER_NEXT_MULTI(step, enm);
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
		 * So, we ignore errornous ones even in this mode.
		 * (Older versions of fe driver mistook the point.)
		 */
		outb( sc->addr + FE_DLCR5,
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
	outb( sc->addr + FE_DLCR5, sc->proto_dlcr5 | FE_D5_AFM1 );

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
	log( LOG_INFO, "fe%d: address filter:"
		" [%02x %02x %02x %02x %02x %02x %02x %02x]\n",
		sc->sc_unit,
		sc->filter.data[0], sc->filter.data[1],
		sc->filter.data[2], sc->filter.data[3],
		sc->filter.data[4], sc->filter.data[5],
		sc->filter.data[6], sc->filter.data[7] );
#endif

	/*
	 * We have to update the multicast filter in the 86960, A.S.A.P.
	 *
	 * Note that the DLC (Data Linc Control unit, i.e. transmitter
	 * and receiver) must be stopped when feeding the filter, and
	 * DLC trushes all packets in both transmission and receive
	 * buffers when stopped.
	 *
	 * ... Are the above sentenses correct?  I have to check the
	 *     manual of the MB86960A.  FIXME.
	 *
	 * To reduce the packet lossage, we delay the filter update
	 * process until buffers are empty.
	 */
	if ( sc->txb_sched == 0 && sc->txb_count == 0
          && !( inb( sc->addr + FE_DLCR1 ) & FE_D1_PKTRDY ) ) {
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
 * The caller must have splimp'ed befor fe_loadmar.
 * This function starts the DLC upon return.  So it can be called only
 * when the chip is working, i.e., from the driver's point of view, when
 * a device is RUNNING.  (I mistook the point in previous versions.)
 */
static void
fe_loadmar ( struct fe_softc * sc )
{
	/* Stop the DLC (transmitter and receiver).  */
	outb( sc->addr + FE_DLCR6, sc->proto_dlcr6 | FE_D6_DLC_DISABLE );

	/* Select register bank 1 for MARs.  */
	outb( sc->addr + FE_DLCR7,
		sc->proto_dlcr7 | FE_D7_RBS_MAR | FE_D7_POWER_UP );

	/* Copy filter value into the registers.  */
	outblk( sc->addr + FE_MAR8, sc->filter.data, FE_FILTER_LEN );

	/* Restore the bank selection for BMPRs (i.e., runtime registers).  */
	outb( sc->addr + FE_DLCR7,
		sc->proto_dlcr7 | FE_D7_RBS_BMPR | FE_D7_POWER_UP );

	/* Restart the DLC.  */
	outb( sc->addr + FE_DLCR6, sc->proto_dlcr6 | FE_D6_DLC_ENABLE );

	/* We have just updated the filter.  */
	sc->filter_change = 0;

#if FE_DEBUG >= 3
	log( LOG_INFO, "fe%d: address filter changed\n", sc->sc_unit );
#endif
}

/*
 * Copy the physical (Ethernet) address into the "data link" address
 * entry of the address list for an interface.
 * This is (said to be) useful for netstat(1) to keep track of which
 * interface is which.
 *
 * What I'm not sure on this function is, why this is a driver's function.
 * Probably this should be moved to somewhere independent to a specific
 * hardware, such as if_ehtersubr.c.  FIXME.
 */
static void
fe_setlinkaddr ( struct fe_softc * sc )
{
	struct ifaddr *ifa;
	struct sockaddr_dl * sdl;

	/*
	 * Search down the ifa address list looking for the AF_LINK type entry.
	 */
	for ( ifa = sc->sc_if.if_addrlist; ifa != NULL; ifa = ifa->ifa_next ) {
		if ( ifa->ifa_addr != NULL
		  && ifa->ifa_addr->sa_family == AF_LINK ) {

			/*
			 * We have found an AF_LINK type entry.
			 * Fill in the link-level address for this interface
			 */
			sdl = (struct sockaddr_dl *) ifa->ifa_addr;
			sdl->sdl_type = IFT_ETHER;
			sdl->sdl_alen = ETHER_ADDR_LEN;
			sdl->sdl_slen = 0;
			bcopy(sc->sc_enaddr, LLADDR(sdl), ETHER_ADDR_LEN);
#if FE_DEBUG >= 3
			log( LOG_INFO, "fe%d: link address set\n",
				sc->sc_unit );
#endif
			return;
		}
	}
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
	    inb( sc->addr + FE_DLCR0 ),  inb( sc->addr + FE_DLCR1 ),
	    inb( sc->addr + FE_DLCR2 ),  inb( sc->addr + FE_DLCR3 ),
	    inb( sc->addr + FE_DLCR4 ),  inb( sc->addr + FE_DLCR5 ),
	    inb( sc->addr + FE_DLCR6 ),  inb( sc->addr + FE_DLCR7 ),
	    inb( sc->addr + FE_BMPR10 ), inb( sc->addr + FE_BMPR11 ),
	    inb( sc->addr + FE_BMPR12 ), inb( sc->addr + FE_BMPR13 ),
	    inb( sc->addr + FE_BMPR14 ), inb( sc->addr + FE_BMPR15 ),
	    inb( sc->addr + 0x10 ), inb( sc->addr + 0x11 ),
	    inb( sc->addr + 0x12 ), inb( sc->addr + 0x13 ),
	    inb( sc->addr + 0x14 ), inb( sc->addr + 0x15 ),
	    inb( sc->addr + 0x16 ), inb( sc->addr + 0x17 ),
	    inb( sc->addr + 0x18 ), inb( sc->addr + 0x19 ),
	    inb( sc->addr + 0x1A ), inb( sc->addr + 0x1B ),
	    inb( sc->addr + 0x1C ), inb( sc->addr + 0x1D ),
	    inb( sc->addr + 0x1E ), inb( sc->addr + 0x1F ) );
}
#endif
