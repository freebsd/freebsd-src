/*
 * Device driver for Specialix range (SI/XIO) of serial line multiplexors.
 *
 * Copyright (C) 1990, 1992 Specialix International,
 * Copyright (C) 1993, Andy Rutter <andy@acronym.co.uk>
 * Copyright (C) 1995, Peter Wemm <peter@haywire.dialix.com>
 *
 * Originally derived from:	SunOS 4.x version
 * Ported from BSDI version to FreeBSD by Peter Wemm.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notices, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notices, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Andy Rutter of
 *	Advanced Methods and Tools Ltd. based on original information
 *	from Specialix International.
 * 4. Neither the name of Advanced Methods and Tools, nor Specialix
 *    International may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE.
 *
 *	$Id: si.c,v 1.29 1995/12/14 14:29:10 peter Exp $
 */

#ifndef lint
static char si_copyright1[] =  "@(#) (C) Specialix International, 1990,1992",
            si_copyright2[] =  "@(#) (C) Andy Rutter 1993",
            si_copyright3[] =  "@(#) (C) Peter Wemm 1995";
#endif	/* not lint */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/ttydefaults.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <sys/dkstat.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/malloc.h>
#include <sys/devconf.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/

#include <machine/clock.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <i386/isa/icu.h>
#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>

#include <i386/isa/sireg.h>
#include <machine/si.h>

#include "si.h"

/*
 * This device driver is designed to interface the Specialix International
 * range of serial multiplexor cards (SI/XIO) to BSDI/386 on an ISA bus machine.
 *
 * The controller is interfaced to the host via dual port ram
 * and a (programmable - SIHOST2) interrupt at IRQ 11,12 or 15.
 */

#define	POLL		/* turn on poller to generate buffer empty interrupt */
#undef	FASTPOLL	/* turn on 100Hz poller, (XXX: NOTYET!) */
#define SI_DEF_HWFLOW	/* turn on default CRTSCTS flow control */
#define SI_I_HIGH_WATER	(TTYHOG - 2 * SI_BUFFERSIZE)

enum si_mctl { GET, SET, BIS, BIC };

static	const char devchar[] = "ABCDEFGHIJK";
static	const char portchar[] = "0123456789abcdefghijklmnopqrstuvwxyz";


static void si_command __P((struct si_port *, int, int));
static int si_modem __P((struct si_port *, enum si_mctl, int));
static void si_write_enable __P((struct si_port *, int));
static int si_Sioctl __P((dev_t, int, caddr_t, int, struct proc *));
static void si_start __P((struct tty *));
static void si_lstart __P((struct si_port *));
static void si_disc_optim __P((struct tty *tp, struct termios *t,
					struct si_port *pp));
static void sihardclose __P((struct si_port *pp));
static void sidtrwakeup __P((void *chan));

static int	siparam __P((struct tty *, struct termios *));

static	void	si_registerdev __P((struct isa_device *id));
static	int	siprobe __P((struct isa_device *id));
static	int	siattach __P((struct isa_device *id));
static	void	si_modem_state __P((struct si_port *pp, struct tty *tp, int hi_ip));

struct isa_driver sidriver =
	{ siprobe, siattach, "si" };


static	d_open_t	siopen;
static	d_close_t	siclose;
static	d_read_t	siread;
static	d_write_t	siwrite;
static	d_ioctl_t	siioctl;
static	d_stop_t	sistop;
static	d_devtotty_t	sidevtotty;

#define CDEV_MAJOR 68
static struct cdevsw si_cdevsw = 
	{ siopen,	siclose,	siread,		siwrite,	/*68*/
	  siioctl,	sistop,		noreset,	sidevtotty,/* si */
	  ttselect,	nommap,		NULL,	"si",	NULL,	-1 };


#ifdef SI_DEBUG		/* use: ``options "SI_DEBUG"'' in your config file */
/* XXX: should be varargs, I know.. but where's vprintf()? */
static	void	si_dprintf __P((/* struct si_port *pp, int flags, char *str, int a1, int a2, int a3, int a4, int a5, int a6 */));
static	char	*si_mctl2str __P((enum si_mctl cmd));
#define	DPRINT(x)	si_dprintf x
#else
#define	DPRINT(x)	/* void */
#endif

static int si_Nports;
static int si_Nmodules;
static int si_debug = 0;	/* data, not bss, so it's patchable */

static struct tty *si_tty;

/* where the firmware lives; defined in si_code.c */
extern int si_dsize;
extern unsigned char si_download[];

#ifdef DEVFS
static char chardev[] = "0123456789abcdef";
#endif

struct si_softc {
	int 		sc_type;	/* adapter type */
	char 		*sc_typename;	/* adapter type string */

	struct si_port	*sc_ports;	/* port structures for this card */

	caddr_t		sc_paddr;	/* physical addr of iomem */
	caddr_t		sc_maddr;	/* kvaddr of iomem */
	int		sc_nport;	/* # ports on this card */
	int		sc_irq;		/* copy of attach irq */
	int		sc_eisa_iobase;	/* EISA io port address */
	int		sc_eisa_irqbits;
	struct kern_devconf sc_kdc;
#ifdef	DEVFS
	struct {
		void	*ttyd;
		void	*cuaa;
		void	*ttyl;
		void	*ttyi;
	} devfs_token[32]; /* what is the max per card? */
#endif
};
static struct si_softc si_softc[NSI];		/* up to 4 elements */

#ifndef B2000	/* not standard, but the hardware knows it. */
# define B2000 2000
#endif
static struct speedtab bdrates[] = {
	B75,	CLK75,		/* 0x0 */
	B110,	CLK110,		/* 0x1 */
	B150,	CLK150,		/* 0x3 */
	B300,	CLK300,		/* 0x4 */
	B600,	CLK600,		/* 0x5 */
	B1200,	CLK1200,	/* 0x6 */
	B2000,	CLK2000,	/* 0x7 */
	B2400,	CLK2400,	/* 0x8 */
	B4800,	CLK4800,	/* 0x9 */
	B9600,	CLK9600,	/* 0xb */
	B19200,	CLK19200,	/* 0xc */
	B38400, CLK38400,	/* 0x2 (out of order!) */
	B57600, CLK57600,	/* 0xd */
	B115200, CLK110,	/* 0x1 (dupe!, 110 baud on "si") */
	-1,	-1
};


/* populated with approx character/sec rates - translated at card
 * initialisation time to chars per tick of the clock */
static int done_chartimes = 0;
static struct speedtab chartimes[] = {
	B75,	8,
	B110,	11,
	B150,	15,
	B300,	30,
	B600,	60,
	B1200,	120,
	B2000,	200,
	B2400,	240,
	B4800,	480,
	B9600,	960,
	B19200,	1920,
	B38400, 3840,
	B57600, 5760,
	B115200, 11520,
	-1,	-1
};
static volatile int in_intr = 0;	/* Inside interrupt handler? */

static int si_default_rate =	TTYDEF_SPEED;
static int si_default_iflag =	0;
static int si_default_oflag =	0;
static int si_default_lflag =	0;
#ifdef SI_DEF_HWFLOW
static int si_default_cflag =	TTYDEF_CFLAG | CRTSCTS;
#else
static int si_default_cflag =	TTYDEF_CFLAG;
#endif

#ifdef POLL
#define	POLL_INTERVAL	(hz/2)
static int init_finished = 0;
static int fastpoll = 0;
static void si_poll __P((void *));
#endif

/*
 * Array of adapter types and the corresponding RAM size. The order of
 * entries here MUST match the ordinal of the adapter type.
 */
static char *si_type[] = {
	"EMPTY",
	"SIHOST",
	"SI2",				/* MCA */
	"SIHOST2",
	"SIEISA",
};


static struct kern_devconf si_kdc[NSI] = { {
	0, 0, 0,		/* filled in by dev_attach */
	"si", 0, { MDDT_ISA, 0, "tty" },
	isa_generic_externalize, 0, 0, ISA_EXTERNALLEN,
	&kdc_isa0,		/* parent */
	0,			/* parent data */
	DC_UNCONFIGURED,	/* state */
	"Specialix SI/XIO Host adapter",
	DC_CLS_SERIAL,		/* class */
} };

static void
si_registerdev(id)
	struct isa_device *id;
{
	if (id->id_unit != 0) {
		si_kdc[id->id_unit] = si_kdc[0];	/* struct copy */
	}
	si_kdc[id->id_unit].kdc_unit = id->id_unit;
	si_kdc[id->id_unit].kdc_isa = id;
	si_kdc[id->id_unit].kdc_state = DC_UNCONFIGURED;
	dev_attach(&si_kdc[id->id_unit]);
}

/* Look for a valid board at the given mem addr */
static int
siprobe(id)
	struct isa_device *id;
{
	struct si_softc *sc;
	int type;
	u_int i, ramsize;
	volatile BYTE was, *ux;
	volatile unsigned char *maddr;
	unsigned char *paddr;

	si_registerdev(id);

	maddr = id->id_maddr;		/* virtual address... */
	paddr = (caddr_t)vtophys(id->id_maddr);	/* physical address... */

	DPRINT((0, DBG_AUTOBOOT, "si%d: probe at virtual=0x%x physical=0x%x\n",
		id->id_unit, id->id_maddr, paddr));

	/*
	 * this is a lie, but it's easier than trying to handle caching
	 * and ram conflicts in the >1M and <16M region.
	 */
	if ((caddr_t)paddr < (caddr_t)IOM_BEGIN ||
	    (caddr_t)paddr >= (caddr_t)IOM_END) {
		printf("si%d: iomem (%lx) out of range\n",
			id->id_unit, (long)paddr);
		return(0);
	}

	if (id->id_unit >= NSI) {
		/* THIS IS IMPOSSIBLE */
		return(0);
	}

	if (((u_int)paddr & 0x7fff) != 0) {
		DPRINT((0, DBG_AUTOBOOT|DBG_FAIL,
			"si%d: iomem (%x) not on 32k boundary\n",
			id->id_unit, paddr));
		return(0);
	}


	for (i=0; i < NSI; i++) {
		if ((sc = &si_softc[i]) == NULL)
			continue;
		if ((caddr_t)sc->sc_paddr == (caddr_t)paddr) {
			DPRINT((0, DBG_AUTOBOOT|DBG_FAIL,
				"si%d: iomem (%x) already configured to si%d\n",
				id->id_unit, sc->sc_paddr, i));
			return(0);
		}
	}

#if NEISA > 0
	if (id->id_iobase > 0x0fff) {	/* EISA card */
		int irq, port;
		unsigned long base;
		int eisa_irqs[] = { 0,IRQ1,IRQ2,IRQ3,IRQ4,IRQ5,IRQ6,IRQ7,
			IRQ8,IRQ9,IRQ10,IRQ11,IRQ12,IRQ13,IRQ14,IRQ15 };

		port = id->id_iobase;
		base = (inb(port+1) << 24) | (inb(port) << 16);
		irq  = ((inb(port+2) >> 4) & 0xf);

		id->id_irq = eisa_irqs[irq];

		DPRINT((0, DBG_AUTOBOOT,
		    "si%d: EISA base %x, irq %x, id_irq %x, port %x\n",
		    id->id_unit, base, irq, id->id_irq, port));

		if ((id->id_irq&(IRQ1|IRQ2|IRQ8|IRQ13)) != 0)
			goto bad_irq;

		id->id_iobase &= 0xf000;
		id->id_iosize  = 0x0fff;

		type = EISA;
		outb(p+2, (BYTE)irq << 4);

		sc->sc_eisa_iobase = p;
		sc->sc_eisa_irqbits = irq << 4;
		ramsize = SIEISA_RAMSIZE;
		goto got_card;
	}
#endif

	/* Is there anything out there? (0x17 is just an arbitrary number) */
	*maddr = 0x17;
	if (*maddr != 0x17) {
		DPRINT((0, DBG_AUTOBOOT|DBG_FAIL,
			"si%d: 0x17 check fail at phys 0x%x\n",
			id->id_unit, paddr));
fail:
		return(0);
	}
	/*
	 * OK, now to see if whatever responded is really an SI card.
	 * Try for a MK II first (SIHOST2)
	 */
	for (i=SIPLSIG; i<SIPLSIG+8; i++)
		if ((*(maddr+i) & 7) != (~(BYTE)i & 7))
			goto try_mk1;

	/* It must be an SIHOST2 */
	*(maddr + SIPLRESET) = 0;
	*(maddr + SIPLIRQCLR) = 0;
	*(maddr + SIPLIRQSET) = 0x10;
	type = SIHOST2;
	ramsize = SIHOST2_RAMSIZE;
	goto got_card;

	/*
	 * Its not a MK II, so try for a MK I (SIHOST)
	 */
try_mk1:
	*(maddr+SIRESET) = 0x0;		/* reset the card */
	*(maddr+SIINTCL) = 0x0;		/* clear int */
	*(maddr+SIRAM) = 0x17;
	if (*(maddr+SIRAM) != (BYTE)0x17)
		goto fail;
	*(maddr+0x7ff8) = 0x17;
	if (*(maddr+0x7ff8) != (BYTE)0x17) {
		DPRINT((0, DBG_AUTOBOOT|DBG_FAIL,
			"si%d: 0x17 check fail at phys 0x%x = 0x%x\n",
			id->id_unit, paddr+0x77f8, *(maddr+0x77f8)));
		goto fail;
	}

	/* It must be an SIHOST (maybe?) - there must be a better way XXXX */
	type = SIHOST;
	ramsize = SIHOST_RAMSIZE;

got_card:
	DPRINT((0, DBG_AUTOBOOT, "si%d: found type %d card, try memory test\n",
		id->id_unit, type));
	/* Try the acid test */
	ux = (BYTE *)(maddr + SIRAM);
	for (i=0; i<ramsize; i++, ux++)
		*ux = (BYTE)(i&0xff);
	ux = (BYTE *)(maddr + SIRAM);
	for (i=0; i<ramsize; i++, ux++) {
		if ((was = *ux) != (BYTE)(i&0xff)) {
			DPRINT((0, DBG_AUTOBOOT|DBG_FAIL,
				"si%d: match fail at phys 0x%x, was %x should be %x\n",
				id->id_unit, paddr+i, was, i&0xff));
			goto fail;
		}
	}

	/* clear out the RAM */
	ux = (BYTE *)(maddr + SIRAM);
	for (i=0; i<ramsize; i++)
		*ux++ = 0;
	ux = (BYTE *)(maddr + SIRAM);
	for (i=0; i<ramsize; i++) {
		if ((was = *ux++) != 0) {
			DPRINT((0, DBG_AUTOBOOT|DBG_FAIL,
				"si%d: clear fail at phys 0x%x, was %x\n",
				id->id_unit, paddr+i, was));
			goto fail;
		}
	}

	/*
	 * Success, we've found a valid board, now fill in
	 * the adapter structure.
	 */
	switch (type) {
	case SIHOST2:
		if ((id->id_irq&(IRQ11|IRQ12|IRQ15)) == 0) {
bad_irq:
			DPRINT((0, DBG_AUTOBOOT|DBG_FAIL,
				"si%d: bad IRQ value - %d\n",
				id->id_unit, id->id_irq));
			return(0);
		}
		id->id_msize = SIHOST2_MEMSIZE;
		break;
	case SIHOST:
		if ((id->id_irq&(IRQ11|IRQ12|IRQ15)) == 0) {
			goto bad_irq;
		}
		id->id_msize = SIHOST_MEMSIZE;
		break;
	case SIEISA:
		id->id_msize = SIEISA_MEMSIZE;
		break;
	case SI2:		/* MCA */
	default:
		printf("si%d: %s not supported\n", id->id_unit, si_type[type]);
		return(0);
	}
	si_softc[id->id_unit].sc_type = type;
	si_softc[id->id_unit].sc_typename = si_type[type];
	return(-1);	/* -1 == found */
}

/*
 * Attach the device.  Initialize the card.
 */
static int
siattach(id)
	struct isa_device *id;
{
	int unit = id->id_unit;
	struct si_softc *sc = &si_softc[unit];
	struct si_port *pp;
	volatile struct si_channel *ccbp;
	volatile struct si_reg *regp;
	volatile caddr_t maddr;
	struct si_module *modp;
	struct tty *tp;
	struct speedtab *spt;
	int nmodule, nport, x, y;
	int uart_type;
#ifdef DEVFS
	char	name[32];
#endif

	DPRINT((0, DBG_AUTOBOOT, "si%d: siattach\n", id->id_unit));

	sc->sc_paddr = (caddr_t)vtophys(id->id_maddr);
	sc->sc_maddr = id->id_maddr;
	sc->sc_irq = id->id_irq;

	sc->sc_ports = NULL;			/* mark as uninitialised */

	maddr = sc->sc_maddr;

	/*
	 * OK, now lets download the firmware and try and boot the CPU..
	 */

	DPRINT((0, DBG_DOWNLOAD, "si%d: si_download: nbytes %d\n",
		id->id_unit, si_dsize));
	bcopy(si_download, maddr, si_dsize);

	switch (sc->sc_type) {
	case SIEISA:
#if NEISA > 0
		/* modify the Z280 firmware to tell it that it's on an EISA */
		*(maddr+0x42) = 1;
		outb(sc->sc_eisa_iobase+2, sc->sc_eisa_irqbits | 4);
		(void)inb(sc->sc_eisa_iobase+3); /* reset interrupt */
		break;
#endif	/* fall-through if not EISA */
	case SI2:
		/*
		 * must get around to converting the code for
		 * these one day, if FreeBSD ever supports it.
		 */
		return 0;
	case SIHOST:
		*(maddr+SIRESET_CL) = 0;
		*(maddr+SIINTCL_CL) = 0;
		break;
	case SIHOST2:
		*(maddr+SIPLRESET) = 0x10;
		switch (sc->sc_irq) {
		case IRQ11:
			*(maddr+SIPLIRQ11) = 0x10;
			break;
		case IRQ12:
			*(maddr+SIPLIRQ12) = 0x10;
			break;
		case IRQ15:
			*(maddr+SIPLIRQ15) = 0x10;
			break;
		}
		*(maddr+SIPLIRQCLR) = 0x10;
		break;
	}

	DELAY(1000000);			/* wait around for a second */

	regp = (struct si_reg *)maddr;
	y = 0;
					/* wait max of 5 sec for init OK */
	while (regp->initstat == 0 && y++ < 10) {
		DELAY(500000);
	}
	switch (regp->initstat) {
	case 0:
		printf("si%d: startup timeout - aborting\n", unit);
		sc->sc_type = SIEMPTY;
		return 0;
	case 1:
			/* set throttle to 125 intr per second */
		regp->int_count = 25000;
			/* rx intr max of 25 timer per second */
		regp->rx_int_count = 4;
		regp->int_pending = 0;		/* no intr pending */
		regp->int_scounter = 0;	/* reset counter */
		break;
	case 0xff:
		/*
		 * No modules found, so give up on this one.
		 */
		printf("si%d: %s - no ports found\n", unit,
			si_type[sc->sc_type]);
		return 0;
	default:
		printf("si%d: Z280 version error - initstat %x\n",
			unit, regp->initstat);
		return 0;
	}

	/*
	 * First time around the ports just count them in order
	 * to allocate some memory.
	 */
	nport = 0;
	modp = (struct si_module *)(maddr + 0x80);
	for (;;) {
		DPRINT((0, DBG_DOWNLOAD, "si%d: ccb addr 0x%x\n", unit, modp));
		switch (modp->sm_type & (~MMASK)) {
		case M232:
		case M422:
			DPRINT((0, DBG_DOWNLOAD,
				"si%d: Found 232/422 module, %d ports\n",
				unit, (int)(modp->sm_type & MMASK)));

			/* this is a firmware issue */
			if (si_Nports == SI_MAXPORTPERCARD) {
				printf("si%d: extra ports ignored\n", unit);
				continue;
			}

			x = modp->sm_type & MMASK;
			nport += x;
			si_Nports += x;
			si_Nmodules++;
			break;
		default:
			printf("si%d: unknown module type %d\n",
				unit, modp->sm_type);
			break;
		}
		if (modp->sm_next == 0)
			break;
		modp = (struct si_module *)
			(maddr + (unsigned)(modp->sm_next & 0x7fff));
	}
	sc->sc_ports = (struct si_port *)malloc(sizeof(struct si_port) * nport,
		M_DEVBUF, M_NOWAIT);
	if (sc->sc_ports == 0) {
mem_fail:
		printf("si%d: fail to malloc memory for port structs\n",
			unit);
		return 0;
	}
	bzero(sc->sc_ports, sizeof(struct si_port) * nport);
	sc->sc_nport = nport;

	/*
	 * allocate tty structures for ports
	 */
	tp = (struct tty *)malloc(sizeof(*tp) * nport, M_DEVBUF, M_NOWAIT);
	if (tp == 0)
		goto mem_fail;
	bzero(tp, sizeof(*tp) * nport);
	si_tty = tp;

	/* mark the device state as attached */
	si_kdc[unit].kdc_state = DC_BUSY;

	/*
	 * Scan round the ports again, this time initialising.
	 */
	pp = sc->sc_ports;
	nmodule = 0;
	modp = (struct si_module *)(maddr + 0x80);
	uart_type = 0;
	for (;;) {
		switch (modp->sm_type & (~MMASK)) {
		case M232:
		case M422:
			nmodule++;
			nport = (modp->sm_type & MMASK);
			ccbp = (struct si_channel *)((char *)modp+0x100);
			if (uart_type == 0)
				uart_type = ccbp->type;
			for (x = 0; x < nport; x++, pp++, ccbp++) {
				pp->sp_ccb = ccbp;	/* save the address */
				pp->sp_tty = tp++;
				pp->sp_pend = IDLE_CLOSE;
				pp->sp_state = 0;	/* internal flag */
				pp->sp_dtr_wait = 3 * hz;
				pp->sp_iin.c_iflag = si_default_iflag;
				pp->sp_iin.c_oflag = si_default_oflag;
				pp->sp_iin.c_cflag = si_default_cflag;
				pp->sp_iin.c_lflag = si_default_lflag;
				termioschars(&pp->sp_iin);
				pp->sp_iin.c_ispeed = pp->sp_iin.c_ospeed =
					si_default_rate;
				pp->sp_iout = pp->sp_iin;
			}
			break;
		default:
			break;
		}
		if (modp->sm_next == 0) {
			printf("si%d: card: %s, ports: %d, modules: %d (type: %d)\n",
				unit,
				sc->sc_typename,
				sc->sc_nport,
				nmodule,
				uart_type);
			break;
		}
		modp = (struct si_module *)
			(maddr + (unsigned)(modp->sm_next & 0x7fff));
	}
	if (done_chartimes == 0) {
		for (spt = chartimes ; spt->sp_speed != -1; spt++) {
			if ((spt->sp_code /= hz) == 0)
				spt->sp_code = 1;
		}
		done_chartimes = 1;
	}

#ifdef DEVFS
/*	path	name	devsw		minor	type   uid gid perm*/
	for ( x = 1; x <= nport; x++ ) {
		sprintf(name,"ttyA%c%c", chardev[x / 10], chardev[x % 10]);
		sc->devfs_token[x].ttyd = devfs_add_devsw(
			"/", name, &si_cdevsw, x,
			DV_CHR, 0, 0, 0600);
		sprintf(name,"cuaA%c%c", chardev[x / 10], chardev[x % 10]);
		sc->devfs_token[x].cuaa = devfs_add_devsw(
			"/", name, &si_cdevsw, x + 128,
			DV_CHR, 0, 0, 0600);
		sprintf(name,"ttyiA%c%c", chardev[x / 10], chardev[x % 10]);
		sc->devfs_token[x].ttyi = devfs_add_devsw(
			"/", name, &si_cdevsw, x + 0x10000,
			DV_CHR, 0, 0, 0600);
		sprintf(name,"ttylA%c%c", chardev[x / 10], chardev[x % 10]);
		sc->devfs_token[x].ttyl = devfs_add_devsw(
			"/", name, &si_cdevsw, x + 0x20000,
			DV_CHR, 0, 0, 0600);
	}
	/* XXX: no global yet */
#endif
	return (1);
}

static	int
siopen(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int oldspl, error;
	int card, port;
	register struct si_softc *sc;
	register struct tty *tp;
	volatile struct si_channel *ccbp;
	struct si_port *pp;
	int mynor = minor(dev);

	/* quickly let in /dev/si_control */
	if (IS_CONTROLDEV(mynor)) {
		if (error = suser(p->p_ucred, &p->p_acflag))
			return(error);
		return(0);
	}

	card = SI_CARD(mynor);
	if (card >= NSI)
		return (ENXIO);
	sc = &si_softc[card];

	if (sc->sc_type == SIEMPTY) {
		DPRINT((0, DBG_OPEN|DBG_FAIL, "si%d: type %s??\n",
			card, sc->sc_typename));
		return(ENXIO);
	}

	port = SI_PORT(mynor);
	if (port >= sc->sc_nport) {
		DPRINT((0, DBG_OPEN|DBG_FAIL, "si%d: nports %d\n",
			card, sc->sc_nport));
		return(ENXIO);
	}

#ifdef	POLL
	/*
	 * We've now got a device, so start the poller.
	 */
	if (init_finished == 0) {
		timeout(si_poll, (caddr_t)0L, POLL_INTERVAL);
		init_finished = 1;
	}
#endif

	/* initial/lock device */
	if (IS_STATE(mynor)) {
		return(0);
	}

	pp = sc->sc_ports + port;
	tp = pp->sp_tty;			/* the "real" tty */
	ccbp = pp->sp_ccb;			/* Find control block */
	DPRINT((pp, DBG_ENTRY|DBG_OPEN, "siopen(%x,%x,%x,%x)\n",
		dev, flag, mode, p));

	oldspl = spltty();			/* Keep others out */
	error = 0;

open_top:
	while (pp->sp_state & SS_DTR_OFF) {
		error = tsleep(&pp->sp_dtr_wait, TTIPRI|PCATCH, "sidtr", 0);
		if (error != 0)
			goto out;
	}

	if (tp->t_state & TS_ISOPEN) {
		/*
		 * The device is open, so everything has been initialised.
		 * handle conflicts.
		 */
		if (IS_CALLOUT(mynor)) {
			if (!pp->sp_active_out) {
				error = EBUSY;
				goto out;
			}
		} else {
			if (pp->sp_active_out) {
				if (flag & O_NONBLOCK) {
					error = EBUSY;
					goto out;
				}
				error = tsleep(&pp->sp_active_out,
						TTIPRI|PCATCH, "sibi", 0);
				if (error != 0)
					goto out;
				goto open_top;
			}
		}
		if (tp->t_state & TS_XCLUDE && p->p_ucred->cr_uid != 0) {
			DPRINT((pp, DBG_OPEN|DBG_FAIL,
				"already open and EXCLUSIVE set\n"));
			error = EBUSY;
			goto out;
		}
	} else {
		/*
		 * The device isn't open, so there are no conflicts.
		 * Initialize it. Avoid sleep... :-)
		 */
		DPRINT((pp, DBG_OPEN, "first open\n"));
		tp->t_oproc = si_start;
		tp->t_param = siparam;
		tp->t_dev = dev;
		tp->t_termios = mynor & SI_CALLOUT_MASK
				? pp->sp_iout : pp->sp_iin;

		(void) si_modem(pp, SET, TIOCM_DTR|TIOCM_RTS);

		++pp->sp_wopeners;	/* in case of sleep in siparam */

		error = siparam(tp, &tp->t_termios);

		--pp->sp_wopeners;
		if (error != 0)
			goto out;
		/* XXX: we should goto_top if siparam slept */

		ttsetwater(tp);

		/* set initial DCD state */
		pp->sp_last_hi_ip = ccbp->hi_ip;
		if ((pp->sp_last_hi_ip & IP_DCD) || IS_CALLOUT(mynor)) {
			(*linesw[tp->t_line].l_modem)(tp, 1);
		}
	}

	/* whoops! we beat the close! */
	if (pp->sp_state & SS_CLOSING) {
		/* try and stop it from proceeding to bash the hardware */
		pp->sp_state &= ~SS_CLOSING;
	}

	/*
	 * Wait for DCD if necessary
	 */
	if (!(tp->t_state & TS_CARR_ON)
	    && !IS_CALLOUT(mynor)
	    && !(tp->t_cflag & CLOCAL)
	    && !(flag & O_NONBLOCK)) {
		++pp->sp_wopeners;
		DPRINT((pp, DBG_OPEN, "sleeping for carrier\n"));
		error = tsleep(TSA_CARR_ON(tp), TTIPRI|PCATCH, "sidcd", 0);
		--pp->sp_wopeners;
		if (error != 0)
			goto out;
		goto open_top;
	}

	error = (*linesw[tp->t_line].l_open)(dev, tp);
	si_disc_optim(tp, &tp->t_termios, pp);
	if (tp->t_state & TS_ISOPEN && IS_CALLOUT(mynor))
		pp->sp_active_out = TRUE;

	pp->sp_state |= SS_OPEN;	/* made it! */

out:
	splx(oldspl);

	DPRINT((pp, DBG_OPEN, "leaving siopen\n"));

	if (!(tp->t_state & TS_ISOPEN) && pp->sp_wopeners == 0)
		sihardclose(pp);

	return(error);
}

static	int
siclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	register struct si_port *pp;
	register struct tty *tp;
	int oldspl;
	int error = 0;
	int mynor = minor(dev);

	if (IS_SPECIAL(mynor))
		return(0);

	oldspl = spltty();

	pp = MINOR2PP(mynor);
	tp = pp->sp_tty;

	DPRINT((pp, DBG_ENTRY|DBG_CLOSE, "siclose(%x,%x,%x,%x) sp_state:%x\n",
		dev, flag, mode, p, pp->sp_state));

	/* did we sleep and loose a race? */
	if (pp->sp_state & SS_CLOSING) {
		/* error = ESOMETING? */
		goto out;
	}

	/* begin race detection.. */
	pp->sp_state |= SS_CLOSING;

	si_write_enable(pp, 0);		/* block writes for ttywait() */

	/* THIS MAY SLEEP IN TTYWAIT!!! */
	(*linesw[tp->t_line].l_close)(tp, flag);

	si_write_enable(pp, 1);

	/* did we sleep and somebody started another open? */
	if (!(pp->sp_state & SS_CLOSING)) {
		/* error = ESOMETING? */
		goto out;
	}
	/* ok. we are now still on the right track.. nuke the hardware */

	if (pp->sp_state & SS_LSTART) {
		untimeout((timeout_func_t)si_lstart, (caddr_t)pp);
		pp->sp_state &= ~SS_LSTART;
	}

	sistop(tp, FREAD | FWRITE);

	sihardclose(pp);
	ttyclose(tp);
	pp->sp_state &= ~SS_OPEN;

out:
	DPRINT((pp, DBG_CLOSE|DBG_EXIT, "close done, returning\n"));
	splx(oldspl);
	return(error);
}

static void
sihardclose(pp)
	struct si_port *pp;
{
	int oldspl;
	struct tty *tp;
	volatile struct si_channel *ccbp;

	oldspl = spltty();

	tp = pp->sp_tty;
	ccbp = pp->sp_ccb;			/* Find control block */
	if (tp->t_cflag & HUPCL
	    || !pp->sp_active_out
	       && !(ccbp->hi_ip & IP_DCD)
	       && !(pp->sp_iin.c_cflag && CLOCAL)
	    || !(tp->t_state & TS_ISOPEN)) {

		(void) si_modem(pp, BIC, TIOCM_DTR|TIOCM_RTS);
		(void) si_command(pp, FCLOSE, SI_NOWAIT);

		if (pp->sp_dtr_wait != 0) {
			timeout(sidtrwakeup, pp, pp->sp_dtr_wait);
			pp->sp_state |= SS_DTR_OFF;
		}

	}
	pp->sp_active_out = FALSE;
	wakeup((caddr_t)&pp->sp_active_out);
	wakeup(TSA_CARR_ON(tp));

	splx(oldspl);
}


/*
 * called at splsoftclock()...
 */
static void
sidtrwakeup(chan)
	void *chan;
{
	struct si_port *pp;
	int oldspl;

	oldspl = spltty();

	pp = (struct si_port *)chan;
	pp->sp_state &= ~SS_DTR_OFF;
	wakeup(&pp->sp_dtr_wait);

	splx(oldspl);
}

/*
 * User level stuff - read and write
 */
static	int
siread(dev, uio, flag)
	register dev_t dev;
	struct uio *uio;
	int flag;
{
	register struct tty *tp;
	int mynor = minor(dev);

	if (IS_SPECIAL(mynor)) {
		DPRINT((0, DBG_ENTRY|DBG_FAIL|DBG_READ, "siread(CONTROLDEV!!)\n"));
		return(ENODEV);
	}
	tp = MINOR2TP(mynor);
	DPRINT((TP2PP(tp), DBG_ENTRY|DBG_READ,
		"siread(%x,%x,%x)\n", dev, uio, flag));
	return ((*linesw[tp->t_line].l_read)(tp, uio, flag));
}


static	int
siwrite(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	register struct si_port *pp;
	register struct tty *tp;
	int error = 0;
	int mynor = minor(dev);
	int oldspl;

	if (IS_SPECIAL(mynor)) {
		DPRINT((0, DBG_ENTRY|DBG_FAIL|DBG_WRITE, "siwrite(CONTROLDEV!!)\n"));
		return(ENODEV);
	}
	pp = MINOR2PP(mynor);
	tp = pp->sp_tty;
	DPRINT((pp, DBG_WRITE, "siwrite(%x,%x,%x)\n", dev, uio, flag));

	oldspl = spltty();
	/*
	 * If writes are currently blocked, wait on the "real" tty
	 */
	while (pp->sp_state & SS_BLOCKWRITE) {
		pp->sp_state |= SS_WAITWRITE;
		DPRINT((pp, DBG_WRITE, "in siwrite, wait for SS_BLOCKWRITE to clear\n"));
		if (error = ttysleep(tp, (caddr_t)pp, TTOPRI|PCATCH,
				     "siwrite", 0))
			goto out;
	}

	error = (*linesw[tp->t_line].l_write)(tp, uio, flag);
out:
	splx(oldspl);
	return (error);
}


static	struct tty *
sidevtotty(dev_t dev)
{
	struct si_port *pp;
	int mynor = minor(dev);
	struct si_softc *sc = &si_softc[SI_CARD(mynor)];

	if (IS_SPECIAL(mynor))
		return(NULL);
	if (SI_PORT(mynor) >= sc->sc_nport)
		return(NULL);
	pp = MINOR2PP(mynor);
	return (pp->sp_tty);
}

static	int
siioctl(dev, cmd, data, flag, p)
	dev_t dev;
	int cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	struct si_port *pp;
	register struct tty *tp;
	int error;
	int mynor = minor(dev);
	int oldspl;
	int blocked = 0;
#if defined(COMPAT_43)
	int oldcmd;
	struct termios term;
#endif

	if (IS_SI_IOCTL(cmd))
		return(si_Sioctl(dev, cmd, data, flag, p));

	pp = MINOR2PP(mynor);
	tp = pp->sp_tty;

	DPRINT((pp, DBG_ENTRY|DBG_IOCTL, "siioctl(%x,%x,%x,%x)\n",
		dev, cmd, data, flag));
	if (IS_STATE(mynor)) {
		struct termios *ct;

		switch (mynor & SI_STATE_MASK) {
		case SI_INIT_STATE_MASK:
			ct = IS_CALLOUT(mynor) ? &pp->sp_iout : &pp->sp_iin;
			break;
		case SI_LOCK_STATE_MASK:
			ct = IS_CALLOUT(mynor) ? &pp->sp_iout : &pp->sp_iin;
			break;
		default:
			return (ENODEV);
		}
		switch (cmd) {
		case TIOCSETA:
			error = suser(p->p_ucred, &p->p_acflag);
			if (error != 0)
				return (error);
			*ct = *(struct termios *)data;
			return (0);
		case TIOCGETA:
			*(struct termios *)data = *ct;
			return (0);
		case TIOCGETD:
			*(int *)data = TTYDISC;
			return (0);
		case TIOCGWINSZ:
			bzero(data, sizeof(struct winsize));
			return (0);
		default:
			return (ENOTTY);
		}
	}
	/*
	 * Do the old-style ioctl compat routines...
	 */
#if defined(COMPAT_43)
	term = tp->t_termios;
	oldcmd = cmd;
	error = ttsetcompat(tp, &cmd, data, &term);
	if (error != 0)
		return (error);
	if (cmd != oldcmd)
		data = (caddr_t)&term;
#endif
	/*
	 * Do the initial / lock state business
	 */
	if (cmd == TIOCSETA || cmd == TIOCSETAW || cmd == TIOCSETAF) {
		int     cc;
		struct termios *dt = (struct termios *)data;
		struct termios *lt = mynor & SI_CALLOUT_MASK
				     ? &pp->sp_lout : &pp->sp_lin;

		dt->c_iflag = (tp->t_iflag & lt->c_iflag)
			| (dt->c_iflag & ~lt->c_iflag);
		dt->c_oflag = (tp->t_oflag & lt->c_oflag)
			| (dt->c_oflag & ~lt->c_oflag);
		dt->c_cflag = (tp->t_cflag & lt->c_cflag)
			| (dt->c_cflag & ~lt->c_cflag);
		dt->c_lflag = (tp->t_lflag & lt->c_lflag)
			| (dt->c_lflag & ~lt->c_lflag);
		for (cc = 0; cc < NCCS; ++cc)
			if (lt->c_cc[cc] != 0)
				dt->c_cc[cc] = tp->t_cc[cc];
		if (lt->c_ispeed != 0)
			dt->c_ispeed = tp->t_ispeed;
		if (lt->c_ospeed != 0)
			dt->c_ospeed = tp->t_ospeed;
	}

	/*
	 * Block user-level writes to give the ttywait()
	 * a chance to completely drain for commands
	 * that require the port to be in a quiescent state.
	 */
	switch (cmd) {
	case TIOCSETAW: case TIOCSETAF:
	case TIOCDRAIN: case TIOCSETP:
		blocked++;	/* block writes for ttywait() and siparam() */
		si_write_enable(pp, 0);
	}

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error >= 0)
		goto out;

	oldspl = spltty();

	error = ttioctl(tp, cmd, data, flag);
	si_disc_optim(tp, &tp->t_termios, pp);
	if (error >= 0)
		goto outspl;

	switch (cmd) {
	case TIOCSBRK:
		si_command(pp, SBREAK, SI_NOWAIT);
		break;
	case TIOCCBRK:
		si_command(pp, EBREAK, SI_NOWAIT);
		break;
	case TIOCSDTR:
		(void) si_modem(pp, SET, TIOCM_DTR|TIOCM_RTS);
		break;
	case TIOCCDTR:
		(void) si_modem(pp, SET, 0);
		break;
	case TIOCMSET:
		(void) si_modem(pp, SET, *(int *)data);
		break;
	case TIOCMBIS:
		(void) si_modem(pp, BIS, *(int *)data);
		break;
	case TIOCMBIC:
		(void) si_modem(pp, BIC, *(int *)data);
		break;
	case TIOCMGET:
		*(int *)data = si_modem(pp, GET, 0);
		break;
	case TIOCMSDTRWAIT:
		/* must be root since the wait applies to following logins */
		error = suser(p->p_ucred, &p->p_acflag);
		if (error != 0) {
			goto outspl;
		}
		pp->sp_dtr_wait = *(int *)data * hz / 100;
		break;
	case TIOCMGDTRWAIT:
		*(int *)data = pp->sp_dtr_wait * 100 / hz;
		break;

	default:
		error = ENOTTY;
	}
	error = 0;
outspl:
	splx(oldspl);
out:
	DPRINT((pp, DBG_IOCTL|DBG_EXIT, "siioctl ret %d\n", error));
	if (blocked)
		si_write_enable(pp, 1);
	return(error);
}

/*
 * Handle the Specialix ioctls. All MUST be called via the CONTROL device
 */
static int
si_Sioctl(dev_t dev, int cmd, caddr_t data, int flag, struct proc *p)
{
	struct si_softc *xsc;
	register struct si_port *xpp;
	volatile struct si_reg *regp;
	struct si_tcsi *dp;
	struct si_pstat *sps;
	int *ip, error = 0;
	int oldspl;
	int card, port;
	int mynor = minor(dev);

	DPRINT((0, DBG_ENTRY|DBG_IOCTL, "si_Sioctl(%x,%x,%x,%x)\n",
		dev, cmd, data, flag));

#if 1
	DPRINT((0, DBG_IOCTL, "TCSI_PORT=%x\n", TCSI_PORT));
	DPRINT((0, DBG_IOCTL, "TCSI_CCB=%x\n", TCSI_CCB));
	DPRINT((0, DBG_IOCTL, "TCSI_TTY=%x\n", TCSI_TTY));
#endif

	if (!IS_CONTROLDEV(mynor)) {
		DPRINT((0, DBG_IOCTL|DBG_FAIL, "not called from control device!\n"));
		return(ENODEV);
	}

	oldspl = spltty();	/* better safe than sorry */

	ip = (int *)data;

#define SUCHECK if (error = suser(p->p_ucred, &p->p_acflag)) goto out

	switch (cmd) {
	case TCSIPORTS:
		*ip = si_Nports;
		goto out;
	case TCSIMODULES:
		*ip = si_Nmodules;
		goto out;
	case TCSISDBG_ALL:
		SUCHECK;
		si_debug = *ip;
		goto out;
	case TCSIGDBG_ALL:
		*ip = si_debug;
		goto out;
	default:
		/*
		 * Check that a controller for this port exists
		 */

		/* may also be a struct si_pstat, a superset of si_tcsi */

		dp = (struct si_tcsi *)data;
		sps = (struct si_pstat *)data;
		card = dp->tc_card;
		xsc = &si_softc[card];	/* check.. */
		if (card < 0 || card >= NSI || xsc->sc_type == SIEMPTY) {
			error = ENOENT;
			goto out;
		}
		/*
		 * And check that a port exists
		 */
		port = dp->tc_port;
		if (port < 0 || port >= xsc->sc_nport) {
			error = ENOENT;
			goto out;
		}
		xpp = xsc->sc_ports + port;
		regp = (struct si_reg *)xsc->sc_maddr;
	}

	switch (cmd) {
	case TCSIDEBUG:
#ifdef	SI_DEBUG
		SUCHECK;
		if (xpp->sp_debug)
			xpp->sp_debug = 0;
		else {
			xpp->sp_debug = DBG_ALL;
			DPRINT((xpp, DBG_IOCTL, "debug toggled %s\n",
				(xpp->sp_debug&DBG_ALL)?"ON":"OFF"));
		}
		break;
#else
		error = ENODEV;
		goto out;
#endif
	case TCSISDBG_LEVEL:
	case TCSIGDBG_LEVEL:
#ifdef	SI_DEBUG
		if (cmd == TCSIGDBG_LEVEL) {
			dp->tc_dbglvl = xpp->sp_debug;
		} else {
			SUCHECK;
			xpp->sp_debug = dp->tc_dbglvl;
		}
		break;
#else
		error = ENODEV;
		goto out;
#endif
	case TCSIGRXIT:
		dp->tc_int = regp->rx_int_count;
		break;
	case TCSIRXIT:
		SUCHECK;
		regp->rx_int_count = dp->tc_int;
		break;
	case TCSIGIT:
		dp->tc_int = regp->int_count;
		break;
	case TCSIIT:
		SUCHECK;
		regp->int_count = dp->tc_int;
		break;
	case TCSISTATE:
		dp->tc_int = xpp->sp_ccb->hi_ip;
		break;
	/* these next three use a different structure */
	case TCSI_PORT:
		SUCHECK;
		sps->tc_siport = *xpp;
		break;
	case TCSI_CCB:
		SUCHECK;
		sps->tc_ccb = *xpp->sp_ccb;
		break;
	case TCSI_TTY:
		SUCHECK;
		sps->tc_tty = *xpp->sp_tty;
		break;
	default:
		error = EINVAL;
		goto out;
	}
out:
	splx(oldspl);
	return(error);		/* success */
}

/*
 *	siparam()	: Configure line params
 *	called at spltty();
 *	this may sleep, does not flush, nor wait for drain, nor block writes
 *	caller must arrange this if it's important..
 */
static int
siparam(tp, t)
	register struct tty *tp;
	register struct termios *t;
{
	register struct si_port *pp = TP2PP(tp);
	volatile struct si_channel *ccbp;
	int oldspl, cflag, iflag, oflag, lflag;
	int error = 0;		/* shutup gcc */
	int ispeed = 0;		/* shutup gcc */
	int ospeed = 0;		/* shutup gcc */
	BYTE val;

	DPRINT((pp, DBG_ENTRY|DBG_PARAM, "siparam(%x,%x)\n", tp, t));
	cflag = t->c_cflag;
	iflag = t->c_iflag;
	oflag = t->c_oflag;
	lflag = t->c_lflag;
	DPRINT((pp, DBG_PARAM, "OFLAG 0x%x CFLAG 0x%x IFLAG 0x%x LFLAG 0x%x\n",
		oflag, cflag, iflag, lflag));


	/* if not hung up.. */
	if (t->c_ospeed != 0) {
		/* translate baud rate to firmware values */
		ospeed = ttspeedtab(t->c_ospeed, bdrates);
		ispeed = t->c_ispeed ?
			 ttspeedtab(t->c_ispeed, bdrates) : ospeed;

		/* enforce legit baud rate */
		if (ospeed < 0 || ispeed < 0)
			return (EINVAL);
	}


	oldspl = spltty();

	ccbp = pp->sp_ccb;

	/* ========== set hi_break ========== */
	val = 0;
	if (iflag & IGNBRK)		/* Breaks */
		val |= BR_IGN;
	if (iflag & BRKINT)		/* Interrupt on break? */
		val |= BR_INT;
	if (iflag & PARMRK)		/* Parity mark? */
		val |= BR_PARMRK;
	if (iflag & IGNPAR)		/* Ignore chars with parity errors? */
		val |= BR_PARIGN;
	ccbp->hi_break = val;

	/* ========== set hi_csr ========== */
	/* if not hung up.. */
	if (t->c_ospeed != 0) {
		/* Set I/O speeds */
		 val = (ispeed << 4) | ospeed;
	}
	ccbp->hi_csr = val;

	/* ========== set hi_mr2 ========== */
	val = 0;
	if (cflag & CSTOPB)				/* Stop bits */
		val |= MR2_2_STOP;
	else
		val |= MR2_1_STOP;
	/*
	 * Enable H/W RTS/CTS handshaking. The default TA/MTA is
	 * a DCE, hence the reverse sense of RTS and CTS
	 */
	/* Output Flow - RTS must be raised before data can be sent */
	if (cflag & CCTS_OFLOW)
		val |= MR2_RTSCONT;

	ccbp->hi_mr1 = val;

	/* ========== set hi_mr1 ========== */
	val = 0;
	if (!(cflag & PARENB))				/* Parity */
		val |= MR1_NONE;
	else
		val |= MR1_WITH;
	if (cflag & PARODD)
		val |= MR1_ODD;

	if ((cflag & CS8) == CS8) {			/* 8 data bits? */
		val |= MR1_8_BITS;
	} else if ((cflag & CS7) == CS7) {		/* 7 data bits? */
		val |= MR1_7_BITS;
	} else if ((cflag & CS6) == CS6) {		/* 6 data bits? */
		val |= MR1_6_BITS;
	} else {					/* Must be 5 */
		val |= MR1_5_BITS;
	}
	/*
	 * Enable H/W RTS/CTS handshaking. The default TA/MTA is
	 * a DCE, hence the reverse sense of RTS and CTS
	 */
	/* Input Flow - CTS is raised when port is ready to receive data */
	if (cflag & CRTS_IFLOW)
		val |= MR1_CTSCONT;

	ccbp->hi_mr1 = val;

	/* ========== set hi_mask ========== */
	val = 0xff;
	if ((cflag & CS8) == CS8) {			/* 8 data bits? */
		val &= 0xFF;
	} else if ((cflag & CS7) == CS7) {		/* 7 data bits? */
		val &= 0x7F;
	} else if ((cflag & CS6) == CS6) {		/* 6 data bits? */
		val &= 0x3F;
	} else {					/* Must be 5 */
		val &= 0x1F;
	}
	if (iflag & ISTRIP)
		val &= 0x7F;

	ccbp->hi_mask = val;

	/* ========== set hi_prtcl ========== */
	val = 0;
				/* Monitor DCD etc. if a modem */
	if (!(cflag & CLOCAL))
		val |= SP_DCEN;
	if (iflag & IXANY)
		val |= SP_TANY;
	if (iflag & IXON)
		val |= SP_TXEN;
	if (iflag & IXOFF)
		val |= SP_RXEN;
	if (iflag & INPCK)
		val |= SP_PAEN;

	ccbp->hi_prtcl = val;


	/* ========== set hi_{rx|tx}{on|off} ========== */
	/* XXX: the card TOTALLY shields us from the flow control... */
	ccbp->hi_txon = t->c_cc[VSTART];
	ccbp->hi_txoff = t->c_cc[VSTOP];

	ccbp->hi_rxon = t->c_cc[VSTART];
	ccbp->hi_rxoff = t->c_cc[VSTOP];

	/* ========== send settings to the card ========== */
	/* potential sleep here */
	if (ccbp->hi_stat == IDLE_CLOSE)		/* Not yet open */
		si_command(pp, LOPEN, SI_WAIT);		/* open it */
	else
		si_command(pp, CONFIG, SI_WAIT);	/* change params */

	/* ========== set DTR etc ========== */
	/* Hangup if ospeed == 0 */
	if (t->c_ospeed == 0) {
		(void) si_modem(pp, BIC, TIOCM_DTR|TIOCM_RTS);
	} else {
		/*
		 * If the previous speed was 0, may need to re-enable
	 	 * the modem signals
	 	 */
		(void) si_modem(pp, SET, TIOCM_DTR|TIOCM_RTS);
	}

	DPRINT((pp, DBG_PARAM, "siparam, complete: MR1 %x MR2 %x HI_MASK %x PRTCL %x HI_BREAK %x\n",
		ccbp->hi_mr1, ccbp->hi_mr2, ccbp->hi_mask, ccbp->hi_prtcl, ccbp->hi_break));

	splx(oldspl);
	return(error);
}

/*
 * Enable or Disable the writes to this channel...
 * "state" ->  enabled = 1; disabled = 0;
 */
static void
si_write_enable(pp, state)
	register struct si_port *pp;
	int state;
{
	int oldspl;

	oldspl = spltty();

	if (state) {
		pp->sp_state &= ~SS_BLOCKWRITE;
		if (pp->sp_state & SS_WAITWRITE) {
			pp->sp_state &= ~SS_WAITWRITE;
			/* thunder away! */
			wakeup((caddr_t)pp);
		}
	} else {
		pp->sp_state |= SS_BLOCKWRITE;
	}

	splx(oldspl);
}

/*
 * Set/Get state of modem control lines.
 * Due to DCE-like behaviour of the adapter, some signals need translation:
 *	TIOCM_DTR	DSR
 *	TIOCM_RTS	CTS
 */
static int
si_modem(pp, cmd, bits)
	struct si_port *pp;
	enum si_mctl cmd;
	int bits;
{
	volatile struct si_channel *ccbp;
	int x;

	DPRINT((pp, DBG_ENTRY|DBG_MODEM, "si_modem(%x,%s,%x)\n", pp, si_mctl2str(cmd), bits));
	ccbp = pp->sp_ccb;		/* Find channel address */
	switch (cmd) {
	case GET:
		x = ccbp->hi_ip;
		bits = TIOCM_LE;
		if (x & IP_DCD)		bits |= TIOCM_CAR;
		if (x & IP_DTR)		bits |= TIOCM_DTR;
		if (x & IP_RTS)		bits |= TIOCM_RTS;
		if (x & IP_RI)		bits |= TIOCM_RI;
		return(bits);
	case SET:
		ccbp->hi_op &= ~(OP_DSR|OP_CTS);
		/* fall through */
	case BIS:
		x = 0;
		if (bits & TIOCM_DTR)
			x |= OP_DSR;
		if (bits & TIOCM_RTS)
			x |= OP_CTS;
		ccbp->hi_op |= x;
		break;
	case BIC:
		if (bits & TIOCM_DTR)
			ccbp->hi_op &= ~OP_DSR;
		if (bits & TIOCM_RTS)
			ccbp->hi_op &= ~OP_CTS;
	}
	return 0;
}

/*
 * Handle change of modem state
 */
static void
si_modem_state(pp, tp, hi_ip)
	register struct si_port *pp;
	register struct tty *tp;
	register int hi_ip;
{
							/* if a modem dev */
	if (hi_ip & IP_DCD) {
		if ( !(pp->sp_last_hi_ip & IP_DCD)) {
			DPRINT((pp, DBG_INTR, "modem carr on t_line %d\n",
				tp->t_line));
			(void)(*linesw[tp->t_line].l_modem)(tp, 1);
		}
	} else {
		if (pp->sp_last_hi_ip & IP_DCD) {
			DPRINT((pp, DBG_INTR, "modem carr off\n"));
			if ((*linesw[tp->t_line].l_modem)(tp, 0))
				(void) si_modem(pp, SET, 0);
		}
	}
	pp->sp_last_hi_ip = hi_ip;

}

/*
 * Poller to catch missed interrupts.
 *
 * Note that the SYSV Specialix drivers poll at 100 times per second to get
 * better response.  We could really use a "periodic" version timeout(). :-)
 */
#ifdef POLL
static void
si_poll(void *nothing)
{
	register struct si_softc *sc;
	register int i;
	volatile struct si_reg *regp;
	register struct si_port *pp;
	int lost, oldspl, port;

	DPRINT((0, DBG_POLL, "si_poll()\n"));
	oldspl = spltty();
	if (in_intr)
		goto out;
	lost = 0;
	for (i=0; i<NSI; i++) {
		sc = &si_softc[i];
		if (sc->sc_type == SIEMPTY)
			continue;
		regp = (struct si_reg *)sc->sc_maddr;
		/*
		 * See if there has been a pending interrupt for 2 seconds
		 * or so. The test <int_scounter >= 200) won't correspond
		 * to 2 seconds if int_count gets changed.
		 */
		if (regp->int_pending != 0) {
			if (regp->int_scounter >= 200 &&
			    regp->initstat == 1) {
				printf("si%d: lost intr\n", i);
				lost++;
			}
		} else {
			regp->int_scounter = 0;
		}

		/*
		 * gripe about no input flow control..
		 */
		pp = sc->sc_ports;
		for (port = 0; port < sc->sc_nport; pp++, port++) {
			if (pp->sp_delta_overflows > 0) {
				printf("si%d: %d tty level buffer overflows\n",
					i, pp->sp_delta_overflows);
				pp->sp_delta_overflows = 0;
			}
		}
	}
	if (lost)
		siintr(-1);	/* call intr with fake vector */
out:
	splx(oldspl);

	timeout(si_poll, (caddr_t)0L, POLL_INTERVAL);
}
#endif	/* ifdef POLL */

/*
 * The interrupt handler polls ALL ports on ALL adapters each time
 * it is called.
 */

static BYTE si_rxbuf[SI_BUFFERSIZE];	/* input staging area */

void
siintr(int unit)
{
	register struct si_softc *sc;

	register struct si_port *pp;
	volatile struct si_channel *ccbp;
	register struct tty *tp;
	volatile caddr_t maddr;
	BYTE op, ip;
	int x, card, port, n, i, isopen;
	volatile BYTE *z;
	BYTE c;

	DPRINT((0, (unit < 0) ? DBG_POLL:DBG_INTR, "siintr(%d)\n", unit));
	if (in_intr) {
		if (unit < 0)	/* should never happen */
			return;
		printf("si%d: Warning interrupt handler re-entered\n",
			unit);
		return;
	}
	in_intr = 1;

	/*
	 * When we get an int we poll all the channels and do ALL pending
	 * work, not just the first one we find. This allows all cards to
	 * share the same vector.
	 */
	for (card=0; card < NSI; card++) {
		sc = &si_softc[card];
		if (sc->sc_type == SIEMPTY)
			continue;

		/*
		 * First, clear the interrupt
		 */
		switch(sc->sc_type) {
		case SIHOST :
			maddr = sc->sc_maddr;
			((volatile struct si_reg *)maddr)->int_pending = 0;
							/* flag nothing pending */
			*(maddr+SIINTCL) = 0x00;	/* Set IRQ clear */
			*(maddr+SIINTCL_CL) = 0x00;	/* Clear IRQ clear */
			break;
		case SIHOST2:
			maddr = sc->sc_maddr;
			((volatile struct si_reg *)maddr)->int_pending = 0;
			*(maddr+SIPLIRQCLR) = 0x00;
			*(maddr+SIPLIRQCLR) = 0x10;
			break;
		case SIEISA:
#if NEISA > 0
			maddr = sc->sc_maddr;
			((volatile struct si_reg *)maddr)->int_pending = 0;
			(void)inb(sc->sc_eisa_iobase+3);
			break;
#endif	/* fall through if not EISA kernel */
		case SIEMPTY:
		default:
			continue;
		}
		((volatile struct si_reg *)maddr)->int_scounter = 0;

		/*
		 * check each port
		 */
		for (pp=sc->sc_ports,port=0; port < sc->sc_nport; pp++,port++) {
			ccbp = pp->sp_ccb;
			tp = pp->sp_tty;


			/*
			 * See if a command has completed ?
			 */
			if (ccbp->hi_stat != pp->sp_pend) {
				DPRINT((pp, DBG_INTR,
					"siintr hi_stat = 0x%x, pend = %d\n",
					ccbp->hi_stat, pp->sp_pend));
				switch(pp->sp_pend) {
				case LOPEN:
				case MPEND:
				case MOPEN:
				case CONFIG:
					pp->sp_pend = ccbp->hi_stat;
						/* sleeping in si_command */
					wakeup(&pp->sp_state);
					break;
				default:
					pp->sp_pend = ccbp->hi_stat;
				}
	 		}

			/*
			 * Continue on if it's closed
			 */
			if (ccbp->hi_stat == IDLE_CLOSE) {
				continue;
			}

			/*
			 * Do modem state change if not a local device
			 */
			si_modem_state(pp, tp, ccbp->hi_ip);

			/*
			 * Check to see if there's we should 'receive'
			 * characters.
			 */
			if (tp->t_state & TS_CONNECTED &&
			    tp->t_state & TS_ISOPEN)
				isopen = 1;
			else
				isopen = 0;

			/*
			 * Do break processing
			 */
			if (ccbp->hi_state & ST_BREAK) {
				if (isopen) {
				    (*linesw[tp->t_line].l_rint)(TTY_BI, tp);
				}
				ccbp->hi_state &= ~ST_BREAK;   /* A Bit iffy this */
				DPRINT((pp, DBG_INTR, "si_intr break\n"));
			}

			/*
			 * Do RX stuff - if not open then dump any characters.
			 * XXX: This is VERY messy and needs to be cleaned up.
			 *
			 * XXX: can we leave data in the host adapter buffer
			 * when the clists are full?  That may be dangerous
			 * if the user cannot get an interrupt signal through.
			 */

	more_rx:	/* XXX Sorry. the nesting was driving me bats! :-( */

			if (!isopen) {
				ccbp->hi_rxopos = ccbp->hi_rxipos;
				goto end_rx;
			}

			/*
			 * Process read characters if not skipped above
			 */
			c = ccbp->hi_rxipos - ccbp->hi_rxopos;
			if (c == 0) {
				goto end_rx;
			}

			op = ccbp->hi_rxopos;
			ip = ccbp->hi_rxipos;
			n = c & 0xff;

			DPRINT((pp, DBG_INTR, "n = %d, op = %d, ip = %d\n",
						n, op, ip));

			/*
			 * Suck characters out of host card buffer into the
			 * "input staging buffer" - so that we dont leave the
			 * host card in limbo while we're possibly echoing
			 * characters and possibly flushing input inside the
			 * ldisc l_rint() routine.
			 */
			if (n <= SI_BUFFERSIZE - op) {

				DPRINT((pp, DBG_INTR, "\tsingle copy\n"));
				z = ccbp->hi_rxbuf + op;
				bcopy((caddr_t)z, si_rxbuf, n);

				op += n;
			} else {
				x = SI_BUFFERSIZE - op;

				DPRINT((pp, DBG_INTR, "\tdouble part 1 %d\n", x));
				z = ccbp->hi_rxbuf + op;
				bcopy((caddr_t)z, si_rxbuf, x);

				DPRINT((pp, DBG_INTR, "\tdouble part 2 %d\n", n-x));
				z = ccbp->hi_rxbuf;
				bcopy((caddr_t)z, si_rxbuf+x, n-x);

				op += n;
			}

			/* clear collected characters from buffer */
			ccbp->hi_rxopos = op;

			DPRINT((pp, DBG_INTR, "n = %d, op = %d, ip = %d\n",
						n, op, ip));

			/*
			 * at this point...
			 * n = number of chars placed in si_rxbuf
			 */

			/*
			 * Avoid the grotesquely inefficient lineswitch
			 * routine (ttyinput) in "raw" mode. It usually
			 * takes about 450 instructions (that's without
			 * canonical processing or echo!). slinput is
			 * reasonably fast (usually 40 instructions
			 * plus call overhead).
			 */
			if (tp->t_state & TS_CAN_BYPASS_L_RINT) {

				/* block if the driver supports it */
				if (tp->t_rawq.c_cc + n >= SI_I_HIGH_WATER
				    && (tp->t_cflag & CRTS_IFLOW
					|| tp->t_iflag & IXOFF)
				    && !(tp->t_state & TS_TBLOCK))
					ttyblock(tp);

				tk_nin += n;
				tk_rawcc += n;
				tp->t_rawcc += n;

				pp->sp_delta_overflows +=
				    b_to_q((char *)si_rxbuf, n, &tp->t_rawq);

				ttwakeup(tp);
				if (tp->t_state & TS_TTSTOP
				    && (tp->t_iflag & IXANY
					|| tp->t_cc[VSTART] == tp->t_cc[VSTOP])) {
					tp->t_state &= ~TS_TTSTOP;
					tp->t_lflag &= ~FLUSHO;
					si_start(tp);
				}
			} else {
				/*
				 * It'd be nice to not have to go through the
				 * function call overhead for each char here.
				 * It'd be nice to block input it, saving a
				 * loop here and the call/return overhead.
				 */
				for(x = 0; x < n; x++) {
					i = si_rxbuf[x];
					if ((*linesw[tp->t_line].l_rint)(i, tp)
					     == -1) {
						pp->sp_delta_overflows++;
					}
					/*
					 * doesn't seem to be much point doing
					 * this here.. this driver has no
					 * softtty processing! ??
					 */
					if (pp->sp_hotchar && i == pp->sp_hotchar) {
						setsofttty();
					}
				}
			}
			goto more_rx;	/* try for more until RXbuf is empty */

	end_rx:		/* XXX: Again, sorry about the gotos.. :-) */

			/*
			 * Do TX stuff
			 */
			(*linesw[tp->t_line].l_start)(tp);

		} /* end of for (all ports on this controller) */
	} /* end of for (all controllers) */

	in_intr = 0;
	DPRINT((0, (unit < 0) ? DBG_POLL:DBG_INTR, "end siintr(%d)\n", unit));
}

/*
 * Nudge the transmitter...
 *
 * XXX: I inherited some funny code here.  It implies the host card only
 * interrupts when the transmit buffer reaches the low-water-mark, and does
 * not interrupt when it's actually hits empty.  In some cases, we have
 * processes waiting for complete drain, and we need to simulate an interrupt
 * about when we think the buffer is going to be empty (and retry if not).
 * I really am not certain about this...  I *need* the hardware manuals.
 */
static void
si_start(tp)
	register struct tty *tp;
{
	struct si_port *pp;
	volatile struct si_channel *ccbp;
	register struct clist *qp;
	register char *dptr;
	BYTE ipos;
	int nchar;
	int oldspl, count, n, amount, buffer_full;
	int do_exitproc;

	oldspl = spltty();

	qp = &tp->t_outq;
	pp = TP2PP(tp);

	DPRINT((pp, DBG_ENTRY|DBG_START,
		"si_start(%x) t_state %x sp_state %x t_outq.c_cc %d\n",
		tp, tp->t_state, pp->sp_state, qp->c_cc));

	if (tp->t_state & (TS_TIMEOUT|TS_TTSTOP))
		goto out;

	do_exitproc = 0;
	buffer_full = 0;
	ccbp = pp->sp_ccb;

	/*
	 * Handle the case where ttywait() is called on process exit
	 * this may be BSDI specific, I dont know...
	 */
	if (tp->t_session != NULL && tp->t_session->s_leader != NULL &&
	    (tp->t_session->s_leader->p_flag & P_WEXIT)) {
		do_exitproc++;
	}

	count = (int)ccbp->hi_txipos - (int)ccbp->hi_txopos;
	DPRINT((pp, DBG_START, "count %d\n", (BYTE)count));

	dptr = (char *)ccbp->hi_txbuf;	/* data buffer */

	while ((nchar = qp->c_cc) > 0) {
		if ((BYTE)count >= 255) {
			buffer_full++;
			break;
		}
		amount = min(nchar, (255 - (BYTE)count));
		ipos = (unsigned int)ccbp->hi_txipos;
		/* will it fit in one lump? */
		if ((SI_BUFFERSIZE - ipos) >= amount) {
			n = q_to_b(&tp->t_outq,
				(char *)&ccbp->hi_txbuf[ipos], amount);
		} else {
			n = q_to_b(&tp->t_outq,
				(char *)&ccbp->hi_txbuf[ipos],
				SI_BUFFERSIZE-ipos);
			if (n == SI_BUFFERSIZE-ipos) {
				n += q_to_b(&tp->t_outq,
					(char *)&ccbp->hi_txbuf[0],
					amount - (SI_BUFFERSIZE-ipos));
			}
		}
		ccbp->hi_txipos += n;
		count = (int)ccbp->hi_txipos - (int)ccbp->hi_txopos;
	}

	if (count != 0 && nchar == 0) {
		tp->t_state |= TS_BUSY;
	} else {
		tp->t_state &= ~TS_BUSY;
	}

	/* wakeup time? */
	ttwwakeup(tp);

	DPRINT((pp, DBG_START, "count %d, nchar %d, tp->t_state 0x%x\n",
		(BYTE)count, nchar, tp->t_state));

	if ((tp->t_state & TS_BUSY) || do_exitproc)
	{
		int time;

		if (do_exitproc != 0) {
			time = hz / 10;
		} else {
			time = ttspeedtab(tp->t_ospeed, chartimes);

			if (time > 0) {
				if (time < nchar)
					time = nchar / time;
				else
					time = 2;
			} else {
				printf("si%d: bad char time value!!\n",
					(int)SI_CARD(tp->t_dev));
				goto out;
			}
		}

		if ((pp->sp_state & (SS_LSTART|SS_INLSTART)) == SS_LSTART) {
			untimeout((timeout_func_t)si_lstart, (caddr_t)pp);
		} else {
			pp->sp_state |= SS_LSTART;
		}
		DPRINT((pp, DBG_START, "arming lstart, time=%d\n", time));
		timeout((timeout_func_t)si_lstart, (caddr_t)pp, time);
	}

out:
	splx(oldspl);
	DPRINT((pp, DBG_EXIT|DBG_START, "leave si_start()\n"));
}

/*
 * Note: called at splsoftclock from the timeout code
 * This has to deal with two things...  cause wakeups while waiting for
 * tty drains on last process exit, and call l_start at about the right
 * time for protocols like ppp.
 */
static void
si_lstart(pp)
	register struct si_port *pp;
{
	register struct tty *tp;
	int oldspl;

	DPRINT((pp, DBG_ENTRY|DBG_LSTART, "si_lstart(%x) sp_state %x\n",
		pp, pp->sp_state));

	oldspl = spltty();

	if ((pp->sp_state & SS_OPEN) == 0 || (pp->sp_state & SS_LSTART) == 0) {
		splx(oldspl);
		return;
	}
	pp->sp_state &= ~SS_LSTART;
	pp->sp_state |= SS_INLSTART;

	tp = pp->sp_tty;

	/* deal with the process exit case */
	ttwwakeup(tp);

	/* nudge protocols - eg: ppp */
	(*linesw[tp->t_line].l_start)(tp);

	pp->sp_state &= ~SS_INLSTART;
	splx(oldspl);
}

/*
 * Stop output on a line. called at spltty();
 */
void
sistop(tp, rw)
	register struct tty *tp;
	int rw;
{
	volatile struct si_channel *ccbp;
	struct si_port *pp;

	pp = TP2PP(tp);
	ccbp = pp->sp_ccb;

	DPRINT((TP2PP(tp), DBG_ENTRY|DBG_STOP, "sistop(%x,%x)\n", tp, rw));

	/* XXX: must check (rw & FWRITE | FREAD) etc flushing... */
	if (rw & FWRITE) {
		/* what level are we meant to be flushing anyway? */
		if (tp->t_state & TS_BUSY) {
			si_command(TP2PP(tp), WFLUSH, SI_NOWAIT);
			tp->t_state &= ~TS_BUSY;
			ttwwakeup(tp);	/* Bruce???? */
		}
	}
#if 1	/* XXX: this doesn't work right yet.. */
	/* XXX: this may have been failing because we used to call l_rint()
	 * while we were looping based on these two counters. Now, we collect
	 * the data and then loop stuffing it into l_rint(), making this
	 * useless.  Should we cause this to blow away the staging buffer?
	 */
	if (rw & FREAD) {
		ccbp->hi_rxopos = ccbp->hi_rxipos;
	}
#endif
}

/*
 * Issue a command to the Z280 host card CPU.
 */

static void
si_command(pp, cmd, waitflag)
	struct si_port *pp;		/* port control block (local) */
	int cmd;
	int waitflag;
{
	int oldspl;
	volatile struct si_channel *ccbp = pp->sp_ccb;
	int x;

	DPRINT((pp, DBG_ENTRY|DBG_PARAM, "si_command(%x,%x,%d): hi_stat 0x%x\n",
		pp, cmd, waitflag, ccbp->hi_stat));

	oldspl = spltty();		/* Keep others out */

	/* wait until it's finished what it was doing.. */
	while((x = ccbp->hi_stat) != IDLE_OPEN &&
			x != IDLE_CLOSE &&
			x != cmd) {
		if (in_intr) {			/* Prevent sleep in intr */
			DPRINT((pp, DBG_PARAM,
				"cmd intr collision - completing %d\trequested %d\n",
				x, cmd));
			splx(oldspl);
			return;
		} else if (ttysleep(pp->sp_tty, (caddr_t)&pp->sp_state, TTIPRI|PCATCH,
				"sicmd1", 1)) {
			splx(oldspl);
			return;
		}
	}
	/* it should now be in IDLE_OPEN, IDLE_CLOSE, or "cmd" */

	/* if there was a pending command, cause a state-change wakeup */
	if (pp->sp_pend != IDLE_OPEN) {
		switch(pp->sp_pend) {
		case LOPEN:
		case MPEND:
		case MOPEN:
		case CONFIG:
			wakeup(&pp->sp_state);
			break;
		default:
			break;
		}
	}

	pp->sp_pend = cmd;		/* New command pending */
	ccbp->hi_stat = cmd;		/* Post it */

	if (waitflag) {
		if (in_intr) {		/* If in interrupt handler */
			DPRINT((pp, DBG_PARAM,
				"attempt to sleep in si_intr - cmd req %d\n",
				cmd));
			splx(oldspl);
			return;
		} else while(ccbp->hi_stat != IDLE_OPEN) {
			if (ttysleep(pp->sp_tty, (caddr_t)&pp->sp_state, TTIPRI|PCATCH,
			    "sicmd2", 0))
				break;
		}
	}
	splx(oldspl);
}

static void
si_disc_optim(tp, t, pp)
	struct tty	*tp;
	struct termios	*t;
	struct si_port	*pp;
{
	/*
	 * XXX can skip a lot more cases if Smarts.  Maybe
	 * (IGNCR | ISTRIP | IXON) in c_iflag.  But perhaps we
	 * shouldn't skip if (TS_CNTTB | TS_LNCH) is set in t_state.
	 */
	if (!(t->c_iflag & (ICRNL | IGNCR | IMAXBEL | INLCR | ISTRIP | IXON))
	    && (!(t->c_iflag & BRKINT) || (t->c_iflag & IGNBRK))
	    && (!(t->c_iflag & PARMRK)
		|| (t->c_iflag & (IGNPAR | IGNBRK)) == (IGNPAR | IGNBRK))
	    && !(t->c_lflag & (ECHO | ICANON | IEXTEN | ISIG | PENDIN))
	    && linesw[tp->t_line].l_rint == ttyinput)
		tp->t_state |= TS_CAN_BYPASS_L_RINT;
	else
		tp->t_state &= ~TS_CAN_BYPASS_L_RINT;

	/*
	 * Prepare to reduce input latency for packet
	 * discplines with a end of packet character.
	 */
	if (tp->t_line == SLIPDISC)
		pp->sp_hotchar = 0xc0;
	else if (tp->t_line == PPPDISC)
		pp->sp_hotchar = 0x7e;
	else
		pp->sp_hotchar = 0;

	DPRINT((pp, DBG_OPTIM, "bypass: %s, hotchar: %x\n", 
		(tp->t_state & TS_CAN_BYPASS_L_RINT) ? "on" : "off",
		pp->sp_hotchar));
}


#ifdef	SI_DEBUG
static void
si_dprintf(pp, flags, str, a1, a2, a3, a4, a5, a6)
	struct si_port *pp;
	int flags;
	char *str;
	int a1, a2, a3, a4, a5, a6;
{
	if ((pp == NULL && (si_debug&flags)) ||
	    (pp != NULL && ((pp->sp_debug&flags) || (si_debug&flags)))) {
	    	if (pp != NULL)
	    		printf("%ci%d(%d): ", 's',
	    			(int)SI_CARD(pp->sp_tty->t_dev),
	    			(int)SI_PORT(pp->sp_tty->t_dev));
		printf(str, a1, a2, a3, a4, a5, a6);
	}
}

static char *
si_mctl2str(cmd)
	enum si_mctl cmd;
{
	switch (cmd) {
	case GET:	return("GET");
	case SET:	return("SET");
	case BIS:	return("BIS");
	case BIC:	return("BIC");
	}
	return("BAD");
}

#endif	/* DEBUG */



static si_devsw_installed = 0;

static void 	si_drvinit(void *unused)
{
	dev_t dev;

	if( ! si_devsw_installed ) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev,&si_cdevsw, NULL);
		si_devsw_installed = 1;
    	}
}

SYSINIT(sidev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,si_drvinit,NULL)

