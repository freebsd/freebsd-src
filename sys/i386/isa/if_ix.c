/*
 * Copyright (c) 1993, 1994, 1995
 *	Rodney W. Grimes, Milwaukie, Oregon  97222.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Rodney W. Grimes.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY RODNEY W. GRIMES ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL RODNEY W. GRIMES BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	$Id: if_ix.c,v 1.20 1996/06/12 05:03:41 gpalmer Exp $
 */

#include "ix.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/syslog.h>
#include <sys/devconf.h>

#include <machine/clock.h>
#include <machine/md_var.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif /* INET */

#ifdef IPX /*ZZZ no work done on this, this is just here to remind me*/
#include <netipx/ipx.h>
#include <netipx/ipx_if.h>
#endif /* IPX */

#ifdef NS /*ZZZ no work done on this, this is just here to remind me*/
#include <netns/ns.h>
#include <netns/ns_if.h>
#endif /* NS */

#ifdef ISO /*ZZZ no work done on this, this is just here to remind me*/
#include <netiso/iso.h>
#include <netiso/iso_var.h>
extern char all_es_snpa[], all_is_snpa[], all_l1is_snpa[], all_l2is_snpa[];
#endif /* ISO */

/*ZZZ no work done on this, this is just here to remind me*/
#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif /* NBPFILTER > 0 */

#include <i386/isa/isa_device.h>
#include <i386/isa/icu.h>
#include <i386/isa/if_ixreg.h>

static ix_softc_t	ix_softc[NIX];

#define	DEBUGNONE	0x0000
#define	DEBUGPROBE	0x0001
#define	DEBUGATTACH	(DEBUGPROBE << 1)
#define DEBUGINIT	(DEBUGATTACH << 1)
#define	DEBUGINIT_RFA	(DEBUGINIT << 1)
#define	DEBUGINIT_TFA	(DEBUGINIT_RFA << 1)
#define DEBUGINTR	(DEBUGINIT_TFA << 1)
#define	DEBUGINTR_FR	(DEBUGINTR << 1)
#define	DEBUGINTR_CX	(DEBUGINTR_FR << 1)
#define DEBUGSTART	(DEBUGINTR_CX << 1)
#define	DEBUGSTOP	(DEBUGSTART << 1)
#define	DEBUGRESET	(DEBUGSTOP << 1)
#define DEBUGDONE	(DEBUGRESET << 1)
#define	DEBUGIOCTL	(DEBUGDONE << 1)
#define	DEBUGACK	(DEBUGIOCTL << 1)
#define	DEBUGCA		(DEBUGACK << 1)
#define	DEBUGCB_WAIT	(DEBUGCA << 1)
#define DEBUGALL	0xFFFFFFFF

/*
#define IXDEBUG	(DEBUGPROBE | DEBUGINIT | DEBUGSTOP | DEBUGCB_WAIT)
*/

#ifdef IXDEBUG
int	ixdebug=IXDEBUG;
#define	DEBUGBEGIN(flag) \
	{\
	if (ixdebug & flag)\
		{
#define	DEBUGEND \
		}\
	}
#define DEBUGDO(x)	x

#else /* IXDEBUG */

#define DEBUGBEGIN(flag)
#define DEBUGEND
#define	DEBUGDO(x)
#endif /* IXDEBUG */

/*
 * Enable the exteneded ixcounters by using #define IXCOUNTERS, note that
 * this requires a modification to the ifnet structure to add the counters
 * in.  Some day a standard extended ifnet structure will be done so that
 * additional statistics can be gathered for any board that supports these
 * counters.
 */
#ifdef IXCOUNTERS
#define IXCOUNTER(x)	x
#else /* IXCOUNTERS */
#define IXCOUNTER(x)
#endif /* IXCOUNTERS */

/*
 * Function Prototypes
 */
static inline void ixinterrupt_enable(int);
static inline void ixinterrupt_disable(int);
static inline void ixchannel_attention(int);
static u_short ixacknowledge(int);
static int ix_cb_wait(cb_t *, char *);
static int ix_scb_wait(scb_t *, u_short, char *);
static int ixprobe(struct isa_device *);
static int ixattach(struct isa_device *);
static void ixinit(int);
static void ixinit_rfa(int);
static void ixinit_tfa(int);
static inline void ixintr_cx(int);
static inline void ixintr_cx_free(int, cb_t *);
static inline void ixintr_fr(int);
static inline void ixintr_fr_copy(int, rfd_t *);
static inline void ixintr_fr_free(int, rfd_t *);
static void ixstart(struct ifnet *);
static int ixstop(struct ifnet *);
static int ixdone(struct ifnet *);
static int ixioctl(struct ifnet *, int, caddr_t);
static void ixreset(int);
static void ixwatchdog(struct ifnet *);
static u_short ixeeprom_read(int, int);
static void ixeeprom_outbits(int, int, int);
static int ixeeprom_inbits(int);
static void ixeeprom_clock(int, int);
/*
RRR */

struct	isa_driver ixdriver = {ixprobe, ixattach, "ix"};

static struct kern_devconf kdc_ix_template = {
	0, 0, 0,                /* filled in by dev_attach */
	"ix", 0, { MDDT_ISA, 0, "net" },
	isa_generic_externalize, 0, 0, ISA_EXTERNALLEN,
	&kdc_isa0,              /* parent */
	0,                      /* parentdata */
	DC_UNCONFIGURED,        /* state */
	"",                     /* description */
	DC_CLS_NETIF            /* class */
};

static inline void
ix_registerdev(struct isa_device *id, const char *descr)
{
	struct kern_devconf *kdc = &ix_softc[id->id_unit].kdc;
	*kdc = kdc_ix_template;
	kdc->kdc_unit = id->id_unit;
	kdc->kdc_parentdata = id;
	kdc->kdc_description = descr;
	dev_attach(kdc);
}

/*
 * Enable the interrupt signal on the board so that it may interrupt
 * the host.
 */
static inline void
ixinterrupt_enable(int unit) {
	ix_softc_t	*sc = &ix_softc[unit];

	outb(sc->iobase + sel_irq, sc->irq_encoded | IRQ_ENABLE);
}

/*
 * Disable the interrupt signal on the board so that it will not interrupt
 * the host.
 */
static inline void
ixinterrupt_disable(int unit) {
	ix_softc_t	*sc = &ix_softc[unit];

	outb(sc->iobase + sel_irq, sc->irq_encoded);
}

/*
 * Send a channel attention to the 82586 chip
 */
static inline void
ixchannel_attention(int unit) {
	DEBUGBEGIN(DEBUGCA)
	DEBUGDO(printf("ca");)
	DEBUGEND
	outb(ix_softc[unit].iobase + ca_ctrl, 0);
}

u_short
ixacknowledge(int unit) {
	ix_softc_t	*sc = &ix_softc[unit];
	scb_t		*scb = (scb_t *)(sc->maddr + SCB_ADDR);
	u_short		status;
	int		i;

	DEBUGBEGIN(DEBUGACK)
	DEBUGDO(printf("ack:");)
	DEBUGEND
	status = scb->status;
	scb->command = status & SCB_ACK_MASK;
	if ((status & SCB_ACK_MASK) != 0) {
		ixchannel_attention(unit);
		for (i = 1000000; scb->command && (i > 0); i--); /*ZZZ timeout*/
		if (i == 0) {
			printf(".TO=%x:", scb->command);
			printf("\nshutting down\n");
			ixinterrupt_disable(unit);
			sc->flags = IXF_NONE;
			status = 0;
		} else {
			DEBUGBEGIN(DEBUGACK)
			DEBUGDO(printf(".ok:");)
			DEBUGEND
		}
	 } else {
		/* nothing to acknowledge */
		DEBUGBEGIN(DEBUGACK)
		DEBUGDO(printf("NONE:");)
		DEBUGEND
	}
	DEBUGBEGIN(DEBUGACK)
	DEBUGDO(printf("%x ", status);)
	DEBUGEND
	return(status);
}

int
ix_cb_wait(cb_t *cb, char *message) {
	int		i;
	int		status;

	for (i=1000000; i>0; i--) {
		if (cb->status & CB_COMPLETE) break; /* Wait for done */
	}
	if (i == 0) {
		printf("%s timeout cb->status = %x\n", message, cb->status);
		status = 1;
	} else {
		DEBUGBEGIN(DEBUGCB_WAIT)
		DEBUGDO(printf("%s cb ok count = %d\n", message, i);)
		DEBUGEND
		status = 0;
	}
	return (status);
}

int
ix_scb_wait(scb_t *scb, u_short expect, char *message) {
	int		i;
	int		status;

	for (i=1000000; i>0; i--) {
		if (scb->status == expect) break; /* Wait for done */
	}
	if (i == 0) {
		printf("%s timeout scb->status = %x\n", message, scb->status);
		status = 1;
	} else {
		DEBUGBEGIN(DEBUGINIT)
		DEBUGDO(printf("%s scb ok count = %d\n", message, i);)
		DEBUGEND
		status = 0;
	}
	return (status);
}

int
ixprobe(struct isa_device *dvp) {
	int	unit = dvp->id_unit;
	ix_softc_t	*sc = &ix_softc[unit];
	char	tempid, idstate;
	int	i;
	int	status = 0;
	u_short	boardid,
		checksum,
		connector,
		eaddrtemp,
		irq;
	/* ZZZ irq_translate should really be unsigned, but until
	 * isa_device.h and all uses are fixed we have to live with it */
	short	irq_translate[] = {0, IRQ9, IRQ3, IRQ4, IRQ5, IRQ10, IRQ11, 0};
	char	irq_encode[] = { 0, 0, 0, 2, 3, 4, 0, 0, 0, 1, 5, 6, 0, 0, 0, 0 };

	DEBUGBEGIN(DEBUGPROBE)
	DEBUGDO(printf ("ixprobe:");)
	DEBUGEND

	/*
	 * Since Intel gives us such a nice way to ID this board lets
	 * see if we really have one at this I/O address
	 */
	idstate = inb(dvp->id_iobase + autoid) & 0x03;
	for (i=0, boardid=0; i < 4; i++) {
		tempid = inb(dvp->id_iobase + autoid);
		boardid |= ((tempid & 0xF0) >> 4) << ((tempid & 0x03) << 2);
		if ((tempid & 0x03) != (++idstate & 0x03)) {
			/* out of sequence, destroy boardid and bail out */
			boardid = 0;
			break;
		}
	}
	DEBUGBEGIN(DEBUGPROBE)
	DEBUGDO(printf("boardid = %x\n", boardid);)
	DEBUGEND
	if (boardid != BOARDID) {
		goto ixprobe_exit;
	}

	/*
	 * We now know that we have a board, so save the I/O base
	 * address in the softc and use the softc from here on out
	 */
	sc->iobase = dvp->id_iobase;

	/*
	 * Reset the Bart ASIC by pulsing the reset bit and waiting
	 * the required 240 uSecounds.  Also place the 82856 in the reset
	 * mode so that we can access the EEPROM
	 */
	outb(sc->iobase + ee_ctrl, GA_RESET);
	outb(sc->iobase + ee_ctrl, 0);
	DELAY(240);
	outb(sc->iobase + ee_ctrl, I586_RESET);

	/*
	 * Checksum the EEPROM, should be equal to BOARDID
	 */
	for (i=0, checksum=0; i<64; i++) {
		checksum += ixeeprom_read(unit, i);
	}
	DEBUGBEGIN(DEBUGPROBE)
	DEBUGDO(printf ("checksum = %x\n", checksum);)
	DEBUGEND
	if (checksum != BOARDID) {
		goto ixprobe_exit;
	}

	/*
	 * Do the I/O channel ready test
	 */
	{
	u_char		lock_bit;

	lock_bit = ixeeprom_read(unit, eeprom_lock_address);
	if (lock_bit & EEPROM_LOCKED) {
		DEBUGBEGIN(DEBUGPROBE)
		DEBUGDO(printf ("lockbit set, no doing io channel ready test\n");)
		DEBUGEND
	} else {
		u_char	bart_config,
			junk;

		bart_config = inb(sc->iobase + config);
		bart_config |= BART_IOCHRDY_LATE | BART_IO_TEST_EN;
		outb(sc->iobase + config, bart_config);
		junk = inb(sc->iobase + 0x4000);	 /*XXX read junk */
		bart_config = inb(sc->iobase + config);
		outb(sc->iobase + config, bart_config & ~(BART_IO_TEST_EN));
		if (bart_config & BART_IO_RESULT) {
			printf ("iochannel ready test failed!!\n");
		} else {
			DEBUGBEGIN(DEBUGPROBE)
			DEBUGDO(printf ("iochannel ready test passed\n");)
			DEBUGEND
		}
	}
	}

	/*
	 * Size and test the memory on the board.  The size of the memory
	 * can be one of 16k, 32k, 48k or 64k.  It can be located in the
	 * address range 0xC0000 to 0xEFFFF on 16k boundaries.  Although
	 * the board can be configured for 0xEC0000 to 0xEEFFFF,
	 * or 0xFC0000 to 0xFFFFFF these ranges are not supported by 386bsd.
	 *
	 * If the size does not match the passed in memory allocation size
	 * issue a warning, but continue with the minimum of the two sizes.
	 */
	{
	u_short	memory_page;
	u_short	memory_adjust;
	u_short	memory_decode;
	u_short	memory_edecode;

	switch (dvp->id_msize) {
		case 65536:
		case 32768: /* XXX Only support 32k and 64k right now */
			{ break; }
		case 16384:
		case 49512:
		default: {
			printf("ixprobe mapped memory size out of range\n");
			goto ixprobe_exit;
		}
	}

	if ((kvtop(dvp->id_maddr) < 0xC0000) ||
	    (kvtop(dvp->id_maddr) + dvp->id_msize > 0xF0000)) {
		printf("ixprobe mapped memory address out of range\n");
		goto ixprobe_exit;
	}

	memory_page = (kvtop(dvp->id_maddr) & 0x3C000) >> 14;
	memory_adjust = MEMCTRL_FMCS16 | (memory_page & 0x3) << 2;
	memory_decode = ((1 << (dvp->id_msize / 16384)) - 1) << memory_page;
	memory_edecode = ((~memory_decode >> 4) & 0xF0) | (memory_decode >> 8);

	/* ZZZ This should be checked against eeprom location 6, low byte */
	outb(sc->iobase + memdec, memory_decode & 0xFF);
	/* ZZZ This should be checked against eeprom location 1, low byte */
	outb(sc->iobase + memctrl, memory_adjust);
	/* ZZZ Now if I could find this one I would have it made */
	outb(sc->iobase + mempc, (~memory_decode & 0xFF));
	/* ZZZ I think this is location 6, high byte */
	outb(sc->iobase + memectrl, memory_edecode); /*XXX disable Exxx */

	sc->maddr = dvp->id_maddr;
	sc->msize = dvp->id_msize;

	DEBUGBEGIN(DEBUGPROBE)
	DEBUGDO(printf("Physical address = %lx\n", kvtop(sc->maddr));)
	DEBUGEND
	}

	/*
	 * first prime the stupid bart DRAM controller so that it
	 * works, then zero out all or memory.
	 */
	bzero(sc->maddr, 32);
	bzero(sc->maddr, sc->msize);

	/*
	 * Get the type of connector used, either AUI, BNC or TPE.
	 */
	connector = ixeeprom_read(unit, eeprom_config1);
	if (connector & CONNECT_BNCTPE) {
		connector = ixeeprom_read(unit, eeprom_config2);
		if (connector & CONNECT_TPE) {
			sc->connector = TPE;
			DEBUGBEGIN(DEBUGPROBE)
			DEBUGDO(printf ("Using TPE connector\n");)
			DEBUGEND
		} else {
			sc->connector = BNC;
			DEBUGBEGIN(DEBUGPROBE)
			DEBUGDO(printf ("Using BNC connector\n");)
			DEBUGEND
		}
	} else {
		sc->connector = AUI;
		DEBUGBEGIN(DEBUGPROBE)
		DEBUGDO(printf ("Using AUI connector\n");)
		DEBUGEND
	}

	/*
	 * Get the encoded interrupt number from the EEPROM, check it
	 * against the passed in IRQ.  Issue a warning if they do not
	 * match.  Always use the passed in IRQ, not the one in the EEPROM.
	 */
	irq = ixeeprom_read(unit, eeprom_config1);
	irq = (irq & IRQ) >> IRQ_SHIFT;
	irq = irq_translate[irq];
	if (dvp->id_irq > 0) {
		if (irq != dvp->id_irq) {
			printf("ix%d: WARNING: board is configured for IRQ %d, using %d\n",
				unit, ffs(irq) - 1, ffs(dvp->id_irq) - 1);
			irq = dvp->id_irq;
		}
	} else {
		dvp->id_irq = irq;
	}
	sc->irq_encoded = irq_encode[ffs(irq) - 1];
	if (sc->irq_encoded == 0) {
		printf("ix%d: invalid irq (%d)\n", unit, ffs(irq) - 1);
		goto ixprobe_exit;
	}

	/*
	 * Get the slot width, either 8 bit or 16 bit.
	 */
	if (inb(sc->iobase + config) & SLOT_WIDTH) {
		sc->width = WIDTH_16;
		DEBUGBEGIN(DEBUGPROBE)
		DEBUGDO(printf("Using 16-bit slot\n");)
		DEBUGEND
	} else {
		sc->width = WIDTH_8;
		DEBUGBEGIN(DEBUGPROBE)
		DEBUGDO(printf("Using 8-bit slot\n");)
		DEBUGEND
	}

	/*
	 * Get the hardware ethernet address from the EEPROM and
	 * save it in the softc for use by the 586 setup code.
	 */
	eaddrtemp = ixeeprom_read(unit, eeprom_enetaddr_high);
	sc->arpcom.ac_enaddr[1] = eaddrtemp & 0xFF;
	sc->arpcom.ac_enaddr[0] = eaddrtemp >> 8;
	eaddrtemp = ixeeprom_read(unit, eeprom_enetaddr_mid);
	sc->arpcom.ac_enaddr[3] = eaddrtemp & 0xFF;
	sc->arpcom.ac_enaddr[2] = eaddrtemp >> 8;
	eaddrtemp = ixeeprom_read(unit, eeprom_enetaddr_low);
	sc->arpcom.ac_enaddr[5] = eaddrtemp & 0xFF;
	sc->arpcom.ac_enaddr[4] = eaddrtemp >> 8;

	sc->flags = IXF_NONE;	/* make sure the flag word is NONE */
	status = IX_IO_PORTS;

#ifndef DEV_LKM
	ix_registerdev(dvp, "Ethernet adapter: Intel EtherExpress16");
#endif /* not DEV_LKM */

ixprobe_exit:
	DEBUGBEGIN(DEBUGPROBE)
	DEBUGDO(printf ("ixprobe exited\n");)
	DEBUGEND
	return(status);
}

int
ixattach(struct isa_device *dvp) {
	int 			unit = dvp->id_unit;
	ix_softc_t		*sc = &ix_softc[unit];
	struct ifnet		*ifp = &sc->arpcom.ac_if;

	DEBUGBEGIN(DEBUGATTACH)
	DEBUGDO(printf("ixattach:");)
	DEBUGEND

	/*
	 * Fill in the interface parameters for if_attach
	 * Note:  We could save some code here by first using a
	 *        bzero(ifp, sizeof(ifp)); and then not doing all
	 *        the = 0;'s
	 *        Infact we should bzero this just to make sure
	 *        that something does not get missed.
	 * Further note by GW:
	 * 	Actually, it's a complete waste of time to zero any of
	 *	this stuff because the C language guarantees that it's
	 *	already zeroed.  If this code is changed to do dynamic
	 *	allocation, this will have to get revisited.
	 */
	bzero(ifp, sizeof(ifp));
	ifp->if_softc = sc;
	ifp->if_name = ixdriver.name;
	ifp->if_unit = unit;
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_SIMPLEX | IFF_BROADCAST;
	ifp->if_output = ether_output;
	ifp->if_start = ixstart;
	ifp->if_done = ixdone;
	ifp->if_ioctl = ixioctl;
	ifp->if_watchdog = ixwatchdog;
	ifp->if_type = IFT_ETHER;
	ifp->if_addrlen = ETHER_ADDRESS_LENGTH;
	ifp->if_hdrlen = ETHER_HEADER_LENGTH;
#ifdef IXCOUNTERS
	 /*
	  * ZZZ more counters added, but bzero gets them
	  */
#endif /* IXCOUNTERS */

	if_attach(ifp);
	ether_ifattach(ifp);
	sc->kdc.kdc_state = DC_IDLE;

	printf("ix%d: address %6D\n", unit, sc->arpcom.ac_enaddr, ":");
	return(0);
}

void
ixinit(int unit) {
	ix_softc_t	*sc = &ix_softc[unit];
	struct		ifnet *ifp = &sc->arpcom.ac_if;
	scp_t		*scp = (scp_t *)(sc->maddr + SCP_ADDR);
	iscp_t		*iscp = (iscp_t *)(sc->maddr + ISCP_ADDR);
	scb_t		*scb = (scb_t *)(sc->maddr + SCB_ADDR);
	cb_t		*cb;
	tbd_t		*tbd;
	int		i;
	u_char		bart_config;	/* bart config byte */
	int		status = 0;

	DEBUGBEGIN(DEBUGINIT)
	DEBUGDO(printf("ixinit:");)
	DEBUGEND

	sc->kdc.kdc_state = DC_BUSY;

	/* Put bart into loopback until we are done intializing to
	 * make sure that packets don't hit the wire */
	bart_config = inb(sc->iobase + config);
	bart_config |= BART_LOOPBACK;
	bart_config |= BART_MCS16_TEST;	/* inb does not get this bit! */
	outb(sc->iobase + config, bart_config);
	bart_config = inb(sc->iobase + config);

	scp->unused1 = 0;		/* Intel says to put zeros in it */
	scp->sysbus = sc->width;	/* ZZZ need to fix for 596 */
	scp->unused2 = 0;		/* Intel says to put zeros in it */
	scp->unused3 = 0;		/* Intel says to put zeros in it */
	scp->iscp = ISCP_ADDR;

	iscp->busy = ISCP_BUSY;
	iscp->scb_offset = SCB_ADDR;
	iscp->scb_base = TFA_START;

	scb->status = SCB_STAT_NULL;
	scb->command = SCB_RESET;
	scb->cbl_offset = TFA_START;
	scb->rfa_offset = RFA_START;
	scb->crc_errors = 0;
	scb->aln_errors = 0;
	scb->rsc_errors = 0;
	scb->ovr_errors = 0;

	ixinit_tfa(unit);
	ixinit_rfa(unit);
	cb = sc->cb_head;
	tbd = sc->tbd_head;

	/*
	 * remove the reset signal and start the 586 up, the 586 well read
         * the SCP, ISCP and the reset CB.  This should put it into a
	 * known state: RESET!
	 */
	outb(sc->iobase + ee_ctrl, EENORMAL);
	ixchannel_attention(unit);

	for (i=100000; iscp->busy && (i>0); i--);	/* Wait for done */
	if (i == 0) {
		printf("iscp->busy time out\n");
		status = 1;
	} else {
		DEBUGBEGIN(DEBUGINIT)
		DEBUGDO(printf ("iscp->busy did not timeout = %d\n", i);)
		DEBUGEND
	}

	status |= ix_scb_wait(scb, (u_short)(SCB_STAT_CX | SCB_STAT_CNA),
	                      "Reset");
	ixacknowledge(unit);

/* XXX this belongs some place else, run diagnostics on the 586 */
	{
	cb_diagnose_t	*cb_diag = (cb_diagnose_t *)(cb);

	cb_diag->common.status = 0;
	cb_diag->common.command = CB_CMD_EL | CB_CMD_DIAGNOSE;
	scb->command = SCB_CUC_START;
	ixchannel_attention(unit);
	status |= ix_cb_wait((cb_t *)cb_diag, "Diagnose");
	status |= ix_scb_wait(scb, (u_short)SCB_STAT_CNA, "Diagnose");
	ixacknowledge(unit);
	}
/* XXX end this belongs some place else, run diagnostics on the 586 */

/* XXX this belongs some place else, run configure on the 586 */
	{
	cb_configure_t	*cb_conf = (cb_configure_t *)(cb);

	cb_conf->common.status = 0;
	cb_conf->common.command = CB_CMD_EL | CB_CMD_CONF;
	cb_conf->byte[0] = 12;		/* 12 byte configure block */
	cb_conf->byte[1] = 8;		/* fifo limit at 8 bytes */
	cb_conf->byte[2] = 0x40;	/* don't save bad frames,
					   srdy/ardy is srdy */
	cb_conf->byte[3] = 0x2E;	/* address length is 6 bytes,
					   address and length are in tb,
					   preamble length is 8 bytes,
					   internal loopback off,
					   external loopback off */
	cb_conf->byte[4] = 0;		/* linear priority is 0,
					   ACR (Exponential priorty) is 0,
					   exponential backoff is 802.3 */
	cb_conf->byte[5] = 96;		/* interframe spacing in TxC clocks */
	cb_conf->byte[6] = 0;		/* lower 8 bits of slot time */
	cb_conf->byte[7] = 0xf2;	/* upper slot time (512 bits),
					   15 transmision retries */
	cb_conf->byte[8] = 0;		/* promiscuous mode off,
					   broadcast enabled,
					   nrz encodeing,
					   cease transmission if ^CRS,
					   insert crc,
					   end of carrier mode bit stuffing,
					   no padding */
	cb_conf->byte[9] = 0;		/* carrier sense filter = 0 bits,
					   carrier sense source external,
					   collision detect filter = 0 bits,
					   collision detect source external */
	cb_conf->byte[10] = 60;		/* minimum number of bytes is a frame */
	cb_conf->byte[11] = 0;		/* unused */

	scb->command = SCB_CUC_START;
	ixchannel_attention(unit);
	status |= ix_cb_wait((cb_t *)cb_conf, "Configure");
	status |= ix_scb_wait(scb, (u_short)SCB_STAT_CNA, "Configure");
	ixacknowledge(unit);
	}
/* XXX end this belongs some place else, run configure on the 586 */

/* XXX this belongs some place else, run ias on the 586 */
	{
	cb_ias_t	*cb_ias = (cb_ias_t *)(cb);

	cb_ias->common.status = 0;
	cb_ias->common.command = CB_CMD_EL | CB_CMD_IAS;
	bcopy(sc->arpcom.ac_enaddr, cb_ias->source, ETHER_ADDRESS_LENGTH);
	scb->command = SCB_CUC_START;
	ixchannel_attention(unit);
	status |= ix_cb_wait((cb_t *)cb_ias, "IAS");
	status |= ix_scb_wait(scb, (u_short)SCB_STAT_CNA, "IAS");
	ixacknowledge(unit);
	}
/* XXX end this belongs some place else, run ias on the 586 */

	if (status == 0) {
		/* Take bart out of loopback as we are done intializing */
		bart_config = inb(sc->iobase + config);
		bart_config &= ~BART_LOOPBACK;
		bart_config |= BART_MCS16_TEST; /* inb does not get this bit! */
		outb(sc->iobase + config, bart_config);

		/* The above code screwed with the tfa, reinit it! */
		ixinit_tfa(unit);
		scb->command = SCB_RUC_START;	/* start up the receive unit */
		ixchannel_attention(unit);
		sc->flags |= IXF_INITED;	/* we have been initialized */
		ifp->if_flags |= IFF_RUNNING;
		ifp->if_flags &= ~IFF_OACTIVE;
		ixinterrupt_enable(unit);	/* Let err fly!!! */
	}

	DEBUGBEGIN(DEBUGINIT)
	DEBUGDO(printf("ixinit exited\n");)
	DEBUGEND
	return;
}

/*
 * ixinit_rfa(int unit)
 *
 *	This routine initializes the Receive Frame Area for the 82586
 *
 * input	the unit number to build the RFA for
 * access	the softc for memory address
 * output	an initialized RFA, ready for packet receiption
 *		the following queue pointers in the softc structure are
 *		also initialize
 *		sc->rfd_head		sc->rfd_tail
 *		sc->rbd_head		sc->rbd_tail
 * defines	RFA_START	the starting offset of the RFA
 *		RFA_SIZE	size of the RFA area
 *		RB_SIZE		size of the receive buffer, this must
 *				be even and should be greater than the
 *				minumum packet size and less than the
 *				maximum packet size
 */

void
ixinit_rfa(int unit) {
	ix_softc_t	*sc = &ix_softc[unit];
	rfd_t		*rfd;
	rbd_t		*rbd;
	caddr_t		rb;
	int		i,
			complete_frame_size,
			how_many_frames;

	DEBUGBEGIN(DEBUGINIT_RFA)
	DEBUGDO(printf("\nix%d: ixinit_rfa\n", unit);)
	DEBUGEND

	complete_frame_size = sizeof(rfd_t) + sizeof(rbd_t) + RB_SIZE;
	how_many_frames = RFA_SIZE / complete_frame_size;

	/* build the list of rfd's, rbd's and rb's */
	rfd = (rfd_t *)(sc->maddr + RFA_START);
	rbd = (rbd_t *)(sc->maddr +
	                RFA_START +
                        (how_many_frames * sizeof(rfd_t)));
	rb = sc->maddr + RFA_START +
	     (how_many_frames * (sizeof(rfd_t) + sizeof(rbd_t)));
	sc->rfd_head = rfd;
	sc->rbd_head = rbd;
	for (i = 0; i < how_many_frames; i++, rfd++, rbd++, rb += RB_SIZE) {
		rfd->status = 0;
		rfd->command = 0;
		rfd->next = KVTOBOARD(rfd) + sizeof(rfd_t);
		rfd->rbd_offset = INTEL586NULL;
		/* ZZZ could bzero this, but just leave a note for now */
		/* ZZZ bzero(rfd->destination); */
		/* ZZZ bzero(rfd->source); */
		rfd->length = 0;

		rbd->act_count = 0;
		rbd->next = KVTOBOARD(rbd) + sizeof(rbd_t);
		rbd->buffer = KVTOBOARD(rb);
		rbd->size = RB_SIZE;

		/*
		 * handle the boundary conditions here.  for the zeroth
		 * rfd we must set the rbd_offset to point at the zeroth
		 * rbd.  for the last rfd and rbd we need to close the
		 * list into a ring and set the end of list bits.
		 */

		if (i == 0) {
			rfd->rbd_offset = KVTOBOARD(rbd);
		}
		if (i == how_many_frames - 1) {
			rfd->command = RFD_CMD_EL | RFD_CMD_SUSP;
			rfd->next = KVTOBOARD(sc->rfd_head);

			rbd->next = KVTOBOARD(sc->rbd_head);
			rbd->size = RBD_SIZE_EL | RB_SIZE;
		}

	}
	sc->rfd_tail = (--rfd);
	sc->rbd_tail = (--rbd);

#ifdef IXDEBUG
	DEBUGBEGIN(DEBUGINIT_RFA)
		rfd = (rfd_t *)(sc->maddr + RFA_START);
		rbd = (rbd_t *)(sc->maddr +
		               RFA_START +
		               (how_many_frames * sizeof(rfd_t)));
		rb = sc->maddr + RFA_START +
		     (how_many_frames * (sizeof(rfd_t) + sizeof(rbd_t)));
		printf("  complete_frame_size = %d\n", complete_frame_size);
		printf("  how_many_frames = %d\n", how_many_frames);
		printf("  rfd_head = %lx\t\trfd_tail = %lx\n",
		       kvtop(sc->rfd_head), kvtop(sc->rfd_tail));
		printf("  rbd_head = %lx\t\trbd_tail = %lx\n",
		       kvtop(sc->rbd_head), kvtop(sc->rbd_tail));
		for (i = 0; i < how_many_frames; i++, rfd++, rbd++, rb += RB_SIZE) {
			printf("  %d:\trfd = %lx\t\trbd = %lx\t\trb = %lx\n",
			       i, kvtop(rfd), kvtop(rbd), kvtop(rb));
			printf("\trfd->command = %x\n", rfd->command);
			printf("\trfd->next = %x\trfd->rbd_offset = %x\n",
			          rfd->next,      rfd->rbd_offset);
			printf("\trbd->next = %x\trbd->size = %x",
			          rbd->next,      rbd->size);
			printf("\trbd->buffer = %lx\n\n",
			          rbd->buffer);
		}
	DEBUGEND
#endif /* IXDEBUG */

	/*
	 * ZZZ need to add sanity check to see if last rb runs into
	 * the stuff after it in memory, this should not be possible
	 * but if someone screws up with the defines it can happen
	 */
	DEBUGBEGIN(DEBUGINIT_RFA)
	DEBUGDO(printf ("  next rb would be at %lx\n", kvtop(rb));)
	DEBUGDO(printf("ix%d: ixinit_rfa exit\n", unit);)
	DEBUGEND
}

/*
 * ixinit_tfa(int unit)
 *
 *	This routine initializes the Transmit Frame Area for the 82586
 *
 * input	the unit number to build the TFA for
 * access	the softc for memory address
 * output	an initialized TFA, ready for packet transmission
 *		the following queue pointers in the softc structure are
 *		also initialize
 *		sc->cb_head		sc->cb_tail
 *		sc->tbd_head		sc->tbd_tail
 * defines	TB_COUNT	home many transmit buffers to create
 *		TB_SIZE		size of the tranmit buffer, this must
 *				be even and should be greater than the
 *				minumum packet size and less than the
 *				maximum packet size
 *		TFA_START	the starting offset of the TFA
 */

void
ixinit_tfa(int unit) {
	ix_softc_t	*sc = &ix_softc[unit];
	cb_transmit_t	*cb;
	tbd_t		*tbd;
	caddr_t		tb;
	int		i;

	DEBUGBEGIN(DEBUGINIT_TFA)
	DEBUGDO(printf("\nix%d: ixinit_tfa\n", unit);)
	DEBUGEND

	/* build the list of cb's, tbd's and tb's */
	cb = (cb_transmit_t *)(sc->maddr + TFA_START);
	tbd = (tbd_t *)(sc->maddr +
	                TFA_START +
                        (TB_COUNT * sizeof(cb_transmit_t)));
	tb = sc->maddr + TFA_START +
	     (TB_COUNT * (sizeof(cb_transmit_t) + sizeof(tbd_t)));
	sc->cb_head = (cb_t *)cb;
	sc->tbd_head = tbd;
	for (i = 0; i < TB_COUNT; i++, cb++, tbd++, tb += TB_SIZE) {
		cb->common.status = 0;
		cb->common.command = CB_CMD_NOP;
		cb->common.next = KVTOBOARD(cb) + sizeof(cb_transmit_t);
		cb->tbd_offset = KVTOBOARD(tbd);
		/* ZZZ could bzero this, but just leave a note for now */
		/* ZZZ bzero(cb->destination); */
		cb->length = 0;

		tbd->act_count = 0;
		tbd->act_count = TBD_STAT_EOF;
		tbd->next = KVTOBOARD(tbd) + sizeof(tbd_t);
		tbd->buffer = KVTOBOARD(tb);

		/*
		 * handle the boundary conditions here.
		 */

		if (i == TB_COUNT - 1) {
			cb->common.command = CB_CMD_EL | CB_CMD_NOP;
			cb->common.next = INTEL586NULL; /*RRR KVTOBOARD(sc->cb_head);*/
			tbd->next = INTEL586NULL; /*RRR KVTOBOARD(sc->tbd_head);*/
		}
	}
	sc->cb_tail = (cb_t *)(--cb);
	sc->tbd_tail = (--tbd);

#ifdef IXDEBUG
	DEBUGBEGIN(DEBUGINIT_TFA)
		cb = (cb_transmit_t *)(sc->maddr + TFA_START);
		tbd = (tbd_t *)(sc->maddr +
		               TFA_START +
		               (TB_COUNT * sizeof(cb_transmit_t)));
		tb = sc->maddr + TFA_START +
		     (TB_COUNT * (sizeof(cb_transmit_t) + sizeof(tbd_t)));
		printf("  TB_COUNT = %d\n", TB_COUNT);
		printf("  cb_head = %lx\t\tcb_tail = %lx\n",
		       kvtop(sc->cb_head), kvtop(sc->cb_tail));
		printf("  tbd_head = %lx\t\ttbd_tail = %lx\n",
		       kvtop(sc->tbd_head), kvtop(sc->tbd_tail));
		for (i = 0; i < TB_COUNT; i++, cb++, tbd++, tb += TB_SIZE) {
			printf("  %d:\tcb = %lx\t\ttbd = %lx\t\ttb = %lx\n",
			       i, kvtop(cb), kvtop(tbd), kvtop(tb));
			printf("\tcb->common.command = %x\n", cb->common.command);
			printf("\tcb->common.next = %x\tcb->tbd_offset = %x\n",
			          cb->common.next,      cb->tbd_offset);
			printf("\ttbd->act_count = %x", tbd->act_count);
			printf("\ttbd->next = %x", tbd->next);
			printf("\ttbd->buffer = %lx\n\n",
			          tbd->buffer);
		}
	DEBUGEND
#endif /* IXDEBUG */

	/*
	 * ZZZ need to add sanity check to see if last tb runs into
	 * the stuff after it in memory, this should not be possible
	 * but if someone screws up with the defines it can happen
	 */
	DEBUGBEGIN(DEBUGINIT_TFA)
	DEBUGDO(printf ("  next tb would be at %lx\n", kvtop(tb));)
	DEBUGDO(printf("ix%d: ixinit_tfa exit\n", unit);)
	DEBUGEND
}

void
ixintr(int unit) {
	ix_softc_t	*sc = &ix_softc[unit];
	struct	ifnet	*ifp = &sc->arpcom.ac_if;
	scb_t		*scb = (scb_t *)(sc->maddr + SCB_ADDR);
	int		check_queue;	/* flag to tell us to check the queue */
	u_short		status;

	DEBUGBEGIN(DEBUGINTR)
	DEBUGDO(printf("ixintr: ");)
	DEBUGEND

	if ((sc->flags & IXF_INITED) == 0) {
		printf ("\n ixintr without being inited!!\n"); /* ZZZ */
		ixinterrupt_disable(unit);
		goto ixintr_exit;
	}
	if (ifp->if_flags & IFF_RUNNING == 0) {
		printf("\n  ixintr when device not running!!\n"); /* ZZZ */
		ixinterrupt_disable(unit);
		goto ixintr_exit;
	}

	/* The sequence, disable ints, status=ack must be done
	 * as quick as possible to avoid missing things */
	ixinterrupt_disable(unit);
	status = ixacknowledge(unit);
	check_queue = 0;
	while ((status & SCB_STAT_MASK) != 0) {
		if (status & SCB_STAT_FR) {
			ixintr_fr(unit);
		}
		if (status & SCB_STAT_CX) {
			ixintr_cx(unit);
			check_queue++;
		}
		if (status & SCB_STAT_CNA) {
			DEBUGBEGIN(DEBUGINTR)
			DEBUGDO(printf("cna:");)
			DEBUGEND
		}
		if ((status & SCB_STAT_RNR) ||
		    ((status & SCB_RUS_MASK) == SCB_RUS_NRSC)) {
			DEBUGBEGIN(DEBUGINTR)
			printf("RNR:");		/* ZZZ this means trouble */
			DEBUGEND
			IXCOUNTER(ifp->if_rnr++;)
			ixinit_rfa(unit);
			scb->status = SCB_STAT_NULL;
			scb->command = SCB_RUC_START;
			scb->rfa_offset = RFA_START;
			ixchannel_attention(unit);
		}
		if (scb->status & SCB_STAT_MASK) {
			status = ixacknowledge(unit);
		} else {
			status = 0;
		}
	}

	ixinterrupt_enable(unit);
	if (check_queue && ifp->if_snd.ifq_head != 0) {
		ixstart(ifp);	/* we have stuff on the queue */
	}

ixintr_exit:
	DEBUGBEGIN(DEBUGINTR)
	DEBUGDO(printf(" ixintr exited\n");)
	DEBUGEND
}

static inline void
ixintr_cx(int unit) {
	ix_softc_t	*sc = &ix_softc[unit];
	struct ifnet	*ifp = &sc->arpcom.ac_if;
	cb_t		*cb;

	DEBUGBEGIN(DEBUGINTR_CX)
	DEBUGDO(printf("cx:");)
	DEBUGEND
	cb = sc->cb_head;
	do {
		if (cb->status & CB_BUSY) {
			IXCOUNTER(ifp->if_busy++;)
			printf("ix.cx.busy");	/* This should never occur */
		}
		if (cb->status & CB_COMPLETE) {
			IXCOUNTER(ifp->if_complete++;)
			switch(cb->command & CB_CMD_MASK) {
				case CB_CMD_NOP: { break; }
				case CB_CMD_IAS: { break; }
				case CB_CMD_CONF: { break; }
				case CB_CMD_MCAS: { break; }
				case CB_CMD_TRANSMIT: {
					if (cb->status & CB_OK) {
						ifp->if_opackets++;
						IXCOUNTER(ifp->if_ok++;)
					} else {
						if (cb->status & CB_ABORT) {
							IXCOUNTER(ifp->if_abort++;)
							printf("ix.cx.abort");
						}
						if (cb->status & CB_LATECOLL) {
							IXCOUNTER(ifp->if_latecoll++;)
							printf("ix.cx.latecoll");
						}
						if (cb->status & CB_NOCS) {
							IXCOUNTER(ifp->if_nocs++;)
							printf("ix.cx.nocs");
						}
						if (cb->status & CB_NOCTS) {
							IXCOUNTER(ifp->if_nocts++;)
							printf("ix.cx.nocts");
						}
						if (cb->status & CB_DMAUNDER) {
							IXCOUNTER(ifp->if_dmaunder++;)
							printf("ix.cx.dmaunder");
						}
						if (cb->status & CB_DEFER) {
							IXCOUNTER(ifp->if_defer++;)
							printf("ix.cx.defer");
						}
						if (cb->status & CB_HEARTBEAT) {
							IXCOUNTER(ifp->if_heartbeat++;)
							printf("ix.cx.heartbeat");
						}
						if (cb->status & CB_EXCESSCOLL) {
							IXCOUNTER(ifp->if_excesscoll++;)
							printf("ix.cx.excesscoll");
						}
						ifp->if_oerrors++;
					}
					ifp->if_collisions += cb->status & CB_COLLISIONS;
					ifp->if_timer = 0;	/* clear watchdog timeout */
					break;
				}
				case CB_CMD_TDR: { break; }
				case CB_CMD_DUMP: { break; }
				case CB_CMD_DIAGNOSE: { break; }
				default: { break; }
			}
			ixintr_cx_free(unit, cb);
		} else {
		}
		if (cb->next == INTEL586NULL) {
			break;
		} else {
			cb = (cb_t *)BOARDTOKV(cb->next);
		}
	}
	while (1);
	/*
	 * clear the IFF_OACTIVE flag because the CU should now be
	 * idle, this only holds true as long as the last CB is the
	 * only one with the CB_CMD_INT bit set.  If the start routine
	 * violates this rule this code well have to change.
	 */
	ifp->if_flags &= ~IFF_OACTIVE;
	}

static inline void
ixintr_cx_free(int unit, cb_t *cb) {

	DEBUGBEGIN(DEBUGINTR_CX)
	DEBUGDO(ix_softc_t	*sc = &ix_softc[unit];)
	DEBUGDO(printf("cb=%x:cb->status=%x:", KVTOBOARD(cb), cb->status);)
	DEBUGEND
/*1*/	cb->command = CB_CMD_EL | CB_CMD_NOP;
/*2*/	cb->status = 0;
}

static inline void
ixintr_fr(int unit) {
	ix_softc_t	*sc = &ix_softc[unit];

	DEBUGBEGIN(DEBUGINTR_FR)
	DEBUGDO(printf("fr:");)
	DEBUGEND
	/* find each frame in the rfa and copy it up, then free it */
	while ((sc->rfd_head->status & (RFD_COMPLETE | RFD_BUSY)) == RFD_COMPLETE) {
		ixintr_fr_copy(unit, sc->rfd_head);
		ixintr_fr_free(unit, sc->rfd_head);
	}
}

static inline void
ixintr_fr_copy(int unit, rfd_t *rfd) {
	ix_softc_t	*sc = &ix_softc[unit];
	struct		ifnet *ifp = &sc->arpcom.ac_if;
	rbd_t		*rbd;
	caddr_t		rb;
	struct mbuf	*m0, *m;
	struct ether_header	*eh;
	int		length,
			bytesleft;

	rbd = (rbd_t *)(sc->maddr + rfd->rbd_offset);
	rb = (caddr_t)(sc->maddr + rbd->buffer);
	DEBUGBEGIN(DEBUGINTR_FR)
	DEBUGDO(int	i;)
	DEBUGDO(printf("rfd=%x:", KVTOBOARD(rfd));)
	DEBUGDO(printf("rfd->status=%x:", rfd->status);)
	DEBUGDO(printf("rbd->act_count=%x:", rbd->act_count);)
	DEBUGDO(printf("data=");)
	DEBUGDO(for (i = 0; i < 16; i ++) printf ("%02x", rb[i] & 0xFF);)
	DEBUGDO(printf(":");)
	DEBUGEND
	/* trickery here, eh points right at memory on
	 * the board.  eh is only used by ether_input,
	 * it is not passed to the upper layer */
	eh = (struct ether_header *)rb;

	/* here we go, lets build an mbuf chain up to hold all this */
	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == 0) {
		printf ("MGETHDR:"); /* ZZZ need to add drop counters */
		return;
	}
	m0 = m;
	length = rbd->act_count & RBD_STAT_SIZE;
	bytesleft = length - sizeof(struct ether_header);
	rb += sizeof(struct ether_header);
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = bytesleft;
	m->m_len = MHLEN;
	while (bytesleft > 0) {
		if (bytesleft > MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if (m->m_flags & M_EXT) {
				m->m_len = MCLBYTES;
			} else {
				printf ("MCLGET:");
				m_freem(m0); /* ZZZ need to add drop counters */
				return;
			}
		}
		m->m_len = min(m->m_len, bytesleft);
		bcopy(rb, mtod(m, caddr_t), m->m_len);
		rb += m->m_len;
		bytesleft -= m->m_len;
		if (bytesleft > 0) {
			MGET(m->m_next, M_DONTWAIT, MT_DATA);
			if (m->m_next == 0) {
				printf ("MGET");
				m_freem(m0); /* ZZZ need to add drop counters */
				return;
			}
			m = m->m_next;
			m->m_len = MLEN;
		}
	}

	ether_input(ifp, eh, m0);
	ifp->if_ipackets++;
	return;
}

static inline void
ixintr_fr_free(int unit, rfd_t *rfd) {
	ix_softc_t	*sc = &ix_softc[unit];
	rbd_t		*rbd;

	rbd = (rbd_t *)(sc->maddr + rfd->rbd_offset);

	/* XXX this still needs work, does not handle chained rbd's */
/*1*/	rbd->act_count = 0;
/*2*/	rbd->size = RBD_SIZE_EL | RB_SIZE;
/*3*/	sc->rbd_tail->size = RB_SIZE;
/*4*/	sc->rbd_tail = rbd;

	/* Free the rfd buy putting it back on the rfd queue */
/*1*/	rfd->command = RFD_CMD_EL | RFD_CMD_SUSP;
/*2*/	rfd->status = 0;
/*3*/	rfd->rbd_offset = INTEL586NULL;
/*4*/	sc->rfd_head = (rfd_t *)BOARDTOKV(rfd->next);
/*5*/	sc->rfd_tail->command &= ~(RFD_CMD_EL | RFD_CMD_SUSP);
/*6*/	sc->rfd_tail = rfd;
}

/* Psuedo code:
 * 	Do consistency check:
 *	  IFF_UP should be set.
 *	  IFF_RUNNING should be set.
 *	  IFF_OACTIVE should be clear.
 *	  ifp->snd.ifq_head should point to an MBUF
 *	  I82586 CU should be in the idle state.
 *	  All cb's should have CUC = NOP.
 *	The real work:
 *	  while there are packets to send & free cb's do:
 *		build a cb, tbd, and tb
 *		copy the MBUF chain to a tb
 *	  setup the scb for a start CU
 *	  start the CU
 *	  set IFF_OACTIVE
 *	  set ifp->if_timer for watchdog timeout
 *	Exit:
 */
void
ixstart(struct ifnet *ifp) {
	int 		unit = ifp->if_unit;
	ix_softc_t	*sc = ifp->if_softc;
	scb_t 		*scb = (scb_t *)BOARDTOKV(SCB_ADDR);
	cb_t		*cb = sc->cb_head;
	tbd_t		*tbd;
	caddr_t		tb;
	struct mbuf	*m, *m_temp;
	u_short		length;
	IXCOUNTER(int		queued;)

	DEBUGBEGIN(DEBUGSTART)
	DEBUGDO(printf("ixstart:");)
	DEBUGEND

	/* check that if is up and running */
	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) != (IFF_UP | IFF_RUNNING)) {
		goto ixstart_exit;
	}

	/* check to see that we are not already active */
	if ((ifp->if_flags & IFF_OACTIVE) == IFF_OACTIVE) {
		goto ixstart_exit;
	}

	/* check that there are packets to send */
	if (ifp->if_snd.ifq_head == 0) {
		goto ixstart_exit;
	}

	/* check that the command unit is idle */
	if ((scb->status & SCB_CUS_MASK) != SCB_CUS_IDLE) {
		goto ixstart_exit;
	}

	/* check that all cb's on the list are free */
#ifdef THISDONTDONOTHING
	cb = sc->cb_head;
	IXCOUNTER(ifp->if_could_queue = 0;)
	do {
		/* XXX this does nothing right now! */
		DEBUGBEGIN(DEBUGSTART)
		DEBUGDO(printf("chk_cb=%x:", KVTOBOARD(cb));)
		DEBUGEND
		IXCOUNTER(ifp->if_could_queue++;)
		if (cb->next == INTEL586NULL) {
			break;
		} else {
			cb = (cb_t *)BOARDTOKV(cb->next);
		}
	}
	while (1);
#endif /* THISDONTDONOTHING */

	/* build as many cb's as we can */
	IXCOUNTER(queued = 0;)
	cb = sc->cb_head;
	do {
		cb->status = 0;
		cb->command = CB_CMD_TRANSMIT;
		tbd = (tbd_t *)BOARDTOKV(((cb_transmit_t *)cb)->tbd_offset);
		tb = (caddr_t)BOARDTOKV(tbd->buffer);
		DEBUGBEGIN(DEBUGSTART)
		DEBUGDO(printf("cb=%x:", KVTOBOARD(cb));)
		DEBUGDO(printf("tbd=%x:", KVTOBOARD(tbd));)
		DEBUGDO(printf("tb=%x:", KVTOBOARD(tb));)
		DEBUGEND

		IF_DEQUEUE(&ifp->if_snd, m);
		length = 0;
		for (m_temp = m; m_temp != 0; m_temp = m_temp->m_next) {
			bcopy(mtod(m_temp, caddr_t), tb, m_temp->m_len);
			tb += m_temp->m_len;
			length += m_temp->m_len;
		}
		m_freem(m);
		if (length < ETHER_MIN_LENGTH) length = ETHER_MIN_LENGTH;
#ifdef DIAGNOSTIC
		if (length > ETHER_MAX_LENGTH) {
			/* XXX
 			 * This should never ever happen, if it does
			 * we probable screwed up all sorts of board data
			 * in the above bcopy's and should probably shut
			 * down, but for now just issue a warning that
			 * something is real wrong
			 */
			printf("ix%d: ixstart: Packet length=%d > MTU=%d\n",
			        unit, length, ETHER_MAX_LENGTH);
		}
#endif /* DIAGNOSTIC */
		tbd->act_count = TBD_STAT_EOF | length;
		IXCOUNTER(queued++;)
		/* check to see if we have used the last cb */
		if (cb->next == INTEL586NULL) {
			IXCOUNTER(ifp->if_filled_queue++;)
			break;
		} else {
			cb = (cb_t *)BOARDTOKV(cb->next);
		}
	} while (ifp->if_snd.ifq_head != 0);
	IXCOUNTER(ifp->if_high_queue = max(ifp->if_high_queue, queued);)

	/* set the end of list and interrupt bits in the last cb */
	cb->command |= (CB_CMD_EL | CB_CMD_INT);

	/* build the scb */
	scb->status = SCB_STAT_NULL;
	scb->command = SCB_CUC_START;
	scb->cbl_offset = KVTOBOARD(sc->cb_head);/* This should not be needed */

	/* start the cu */
	ixchannel_attention(unit);

	/* mark the interface as having output active */
	ifp->if_flags |= IFF_OACTIVE;

	/*
	 * set the watchdog timer so that if the board fails to interrupt
	 * we will go clean up
	 */
	ifp->if_timer = 2;

ixstart_exit:
	DEBUGBEGIN(DEBUGSTART)
	DEBUGDO(printf("ixstart exited\n");)
	DEBUGEND
	return;
}

int
ixstop(struct ifnet *ifp) {
	int		unit = ifp->if_unit;
	ix_softc_t	*sc = ifp->if_softc;

	DEBUGBEGIN(DEBUGSTOP)
	DEBUGDO(printf("ixstop:");)
	DEBUGEND

	/* XXX Need to find out what spl we are at, and maybe add splx */

	ifp->if_flags &= ~IFF_RUNNING;
	ixinterrupt_disable(unit);

	/* force the 82586 reset pin high */
	outb(sc->iobase + ee_ctrl, I586_RESET);

	sc->kdc.kdc_state = DC_IDLE;

	DEBUGBEGIN(DEBUGSTOP)
	DEBUGDO(printf("ixstop exiting\n");)
	DEBUGEND
	return(0);
}

/*
 * I can't find any calls to if_done, it may be deprecated, but I left
 * it here until I find out.  rwgrimes 1993/01/15
 */
int
ixdone(struct ifnet *ifp) {
	DEBUGBEGIN(DEBUGDONE)
	DEBUGDO(printf("ixdone:");)
	DEBUGEND
	DEBUGBEGIN(DEBUGDONE)
	DEBUGDO(printf("ixdone exited\n");)
	DEBUGEND
	return(0);
}

int
ixioctl(struct ifnet *ifp, int cmd, caddr_t data) {
	int	unit = ifp->if_unit;
	int	status = 0;
	int	s;

	DEBUGBEGIN(DEBUGIOCTL)
	DEBUGDO(printf("ixioctl:");)
	DEBUGEND

	s = splimp();

	switch(cmd) {
	case SIOCSIFADDR: {
		struct ifaddr *ifa = (struct ifaddr *)data;

		if (ifp->if_flags & IFF_RUNNING) ixstop(ifp);
		ifp->if_flags |= IFF_UP;
		ixinit(unit);

		switch(ifa->ifa_addr->sa_family) {
#ifdef INET
			case AF_INET: {
				arp_ifinit((struct arpcom *)ifp, ifa);
				break;
			}
#endif /* INET */
#ifdef IPX
			case AF_IPX: {
				/*ZZZ*/printf("Address family IPX not supported by ixioctl\n");
				break;
			}
#endif /* IPX */
#ifdef NS
			case AF_NS: {
				/*ZZZ*/printf("Address family NS not supported by ixioctl\n");
				break;
			}
#endif /* NS */
			default: {
				DEBUGBEGIN(DEBUGIOCTL)
				DEBUGDO(printf("Unknow Address Family in ixioctl\n");)
				DEBUGEND
				status = EINVAL;
				break;
			}
		}
		break;
	}

	case SIOCSIFFLAGS: {
		if (((ifp->if_flags & IFF_UP) == 0) &&
		     (ifp->if_flags & IFF_RUNNING)) {
			ixstop(ifp);
		}
		else if ((ifp->if_flags & IFF_UP) &&
		        ((ifp->if_flags & IFF_RUNNING) == 0)) {
			ixinit(unit);
		}
		break;
	}

	default: {
		DEBUGBEGIN(DEBUGIOCTL)
		DEBUGDO(printf("Unknown cmd in ixioctl\n");)
		DEBUGEND
		status = EINVAL;
		break;
	}
	}
	splx(s);

	DEBUGBEGIN(DEBUGIOCTL)
	DEBUGDO(printf("ixioctl exit\n");)
	DEBUGEND
	return(status);
}

void
ixreset(int unit) {
	ix_softc_t	*sc = &ix_softc[unit];
	struct	ifnet	*ifp = &sc->arpcom.ac_if;
	int		s;

	s = splimp();
	DEBUGBEGIN(DEBUGRESET)
	DEBUGDO(printf("ixreset:");)
	DEBUGEND

	ixstop(ifp);
	ixinit(unit);

	DEBUGBEGIN(DEBUGRESET)
	DEBUGDO(printf("ixreset exit\n");)
	DEBUGEND
	(void) splx(s);
	return;
}
/*
 * The ixwatchdog routine gets called if the transmitter failed to interrupt
 * within ifp->if_timer XXXseconds.  The interrupt service routine must reset
 * ifp->if_timer to 0 after an transmitter interrupt occurs to stop the
 * watchdog from happening.
 */
void
ixwatchdog(struct ifnet *ifp) {
	log(LOG_ERR, "ix%d: device timeout\n", ifp->if_unit);
	ifp->if_oerrors++;
	ixreset(ifp->if_unit);
	return;
}

u_short
ixeeprom_read(int unit, int location) {
	int	eeprom_control,
		data;

	eeprom_control = inb(ix_softc[unit].iobase + ee_ctrl);
	eeprom_control &= 0xB2; /* XXX fix 0xB2 */
	eeprom_control |= EECS;
	outb(ix_softc[unit].iobase + ee_ctrl, eeprom_control);
	ixeeprom_outbits(unit, eeprom_read_op, eeprom_opsize1);
	ixeeprom_outbits(unit, location, eeprom_addr_size);
	data = ixeeprom_inbits(unit);
	eeprom_control = inb(ix_softc[unit].iobase + ee_ctrl);
	eeprom_control &= ~(GA_RESET | EEDI | EECS);
	outb(ix_softc[unit].iobase + ee_ctrl, eeprom_control);
	ixeeprom_clock(unit, 1);
	ixeeprom_clock(unit, 0);
	return(data);
}

void
ixeeprom_outbits(int unit, int data, int count) {
	int	eeprom_control,
		i;

	eeprom_control = inb(ix_softc[unit].iobase + ee_ctrl);
	eeprom_control &= ~GA_RESET;
	for(i=count-1; i>=0; i--) {
		eeprom_control &= ~EEDI;
		if (data & (1 << i)) {
			eeprom_control |= EEDI;
		}
		outb(ix_softc[unit].iobase + ee_ctrl, eeprom_control);
		DELAY(1); /* eeprom data must be setup for 0.4 uSec */
		ixeeprom_clock(unit, 1);
		ixeeprom_clock(unit, 0);
	}
	eeprom_control &= ~EEDI;
	outb(ix_softc[unit].iobase + ee_ctrl, eeprom_control);
	DELAY(1); /* eeprom data must be held for 0.4 uSec */
}

int
ixeeprom_inbits(int unit) {
	int	eeprom_control,
		data,
		i;

	eeprom_control = inb(ix_softc[unit].iobase + ee_ctrl);
	eeprom_control &= ~GA_RESET;
	for(data=0, i=0; i<16; i++) {
		data = data << 1;
		ixeeprom_clock(unit, 1);
		eeprom_control = inb(ix_softc[unit].iobase + ee_ctrl);
		if (eeprom_control & EEDO) {
			data |= 1;
		}
		ixeeprom_clock(unit, 0);
	}
	return(data);
}

void
ixeeprom_clock(int unit, int state) {
	int	eeprom_control;

	eeprom_control = inb(ix_softc[unit].iobase + ee_ctrl);
	eeprom_control &= ~(GA_RESET | EESK);
	if (state) {
		eeprom_control = eeprom_control | EESK;
	}
	outb(ix_softc[unit].iobase + ee_ctrl, eeprom_control);
	DELAY(9); /* EESK must be stable for 8.38 uSec */
}
