/*
 * Device driver for Specialix range (SI/XIO) of serial line multiplexors.
 *
 * Copyright (C) 1990, 1992, 1998 Specialix International,
 * Copyright (C) 1993, Andy Rutter <andy@acronym.co.uk>
 * Copyright (C) 1995, Peter Wemm <peter@netplex.com.au>
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
 *	$Id: si.c,v 1.69 1998/03/23 16:27:37 peter Exp $
 */

#ifndef lint
static const char si_copyright1[] =  "@(#) Copyright (C) Specialix International, 1990,1992,1998",
		  si_copyright2[] =  "@(#) Copyright (C) Andy Rutter 1993",
		  si_copyright3[] =  "@(#) Copyright (C) Peter Wemm 1995";
#endif	/* not lint */

#include "opt_compat.h"
#include "opt_debug_si.h"
#include "opt_devfs.h"

#include <sys/param.h>
#include <sys/systm.h>
#if defined(COMPAT_43) || defined(COMPAT_SUNOS)
#include <sys/ioctl_compat.h>
#endif
#include <sys/tty.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/dkstat.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/

#include <machine/clock.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <i386/isa/icu.h>
#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>

#include <i386/isa/sireg.h>
#include <machine/si.h>
#include <machine/stdarg.h>

#include "pci.h"
#if NPCI > 0
#include <pci/pcivar.h>
#endif

#include "eisa.h"
#if NEISA > 0
#include <i386/eisa/eisaconf.h>
#include <i386/isa/icu.h>
#endif

#include "si.h"

/*
 * This device driver is designed to interface the Specialix International
 * SI, XIO and SX range of serial multiplexor cards to FreeBSD on an ISA,
 * EISA or PCI bus machine.
 *
 * The controller is interfaced to the host via dual port RAM
 * and an interrupt.
 *
 * The code for the Host 1 (very old ISA cards) has not been tested.
 */

#define	POLL		/* turn on poller to scan for lost interrupts */
#define REALPOLL	/* on each poll, scan for work regardless */
#define POLLHZ	(hz/10)	/* 10 times per second */
#define SI_I_HIGH_WATER	(TTYHOG - 2 * SI_BUFFERSIZE)
#define INT_COUNT 25000		/* max of 125 ints per second */
#define JET_INT_COUNT 100	/* max of 100 ints per second */
#define RXINT_COUNT 1	/* one rxint per 10 milliseconds */

enum si_mctl { GET, SET, BIS, BIC };

static void si_command __P((struct si_port *, int, int));
static int si_modem __P((struct si_port *, enum si_mctl, int));
static void si_write_enable __P((struct si_port *, int));
static int si_Sioctl __P((dev_t, int, caddr_t, int, struct proc *));
static void si_start __P((struct tty *));
static timeout_t si_lstart;
static void si_disc_optim __P((struct tty *tp, struct termios *t,
					struct si_port *pp));
static void sihardclose __P((struct si_port *pp));
static void sidtrwakeup __P((void *chan));

static int	siparam __P((struct tty *, struct termios *));

static	int	siprobe __P((struct isa_device *id));
static	int	siattach __P((struct isa_device *id));
static	void	si_modem_state __P((struct si_port *pp, struct tty *tp, int hi_ip));
static void	si_intr __P((int unit));
static char *	si_modulename __P((int host_type, int uart_type));

struct isa_driver sidriver =
	{ siprobe, siattach, "si" };

static u_long sipcieisacount = 0;

#if NPCI > 0

static char *sipciprobe __P((pcici_t, pcidi_t));
static void sipciattach __P((pcici_t, int));

static struct pci_device sipcidev = {
	"si",
	sipciprobe,
	sipciattach,
	&sipcieisacount,
	NULL,
};

DATA_SET (pcidevice_set, sipcidev);

#endif

#if NEISA > 0

static int si_eisa_probe __P((void));
static int si_eisa_attach __P((struct eisa_device *ed));

static struct eisa_driver si_eisa_driver = {
	"si",
	si_eisa_probe,
	si_eisa_attach,
	NULL,
	&sipcieisacount,
};

DATA_SET(eisadriver_set, si_eisa_driver);

#endif

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
	  ttpoll,	nommap,		NULL,	"si",	NULL,	-1 };


#ifdef SI_DEBUG		/* use: ``options "SI_DEBUG"'' in your config file */

static	void	si_dprintf __P((struct si_port *pp, int flags, const char *fmt,
				...));
static	char	*si_mctl2str __P((enum si_mctl cmd));

#define	DPRINT(x)	si_dprintf x

#else
#define	DPRINT(x)	/* void */
#endif

static int si_Nports;
static int si_Nmodules;
static int si_debug = 0;	/* data, not bss, so it's patchable */

SYSCTL_INT(_machdep, OID_AUTO, si_debug, CTLFLAG_RW, &si_debug, 0, "");

static struct tty *si_tty;

/* where the firmware lives; defined in si2_z280.c and si3_t225.c */
/* old: si2_z280.c */
extern unsigned char si2_z280_download[];
extern unsigned short si2_z280_downloadaddr;
extern int si2_z280_dsize;
/* new: si3_t225.c */
extern unsigned char si3_t225_download[];
extern unsigned short si3_t225_downloadaddr;
extern int si3_t225_dsize;
extern unsigned char si3_t225_bootstrap[];
extern unsigned short si3_t225_bootloadaddr;
extern int si3_t225_bsize;


struct si_softc {
	int 		sc_type;	/* adapter type */
	char 		*sc_typename;	/* adapter type string */

	struct si_port	*sc_ports;	/* port structures for this card */

	caddr_t		sc_paddr;	/* physical addr of iomem */
	caddr_t		sc_maddr;	/* kvaddr of iomem */
	int		sc_nport;	/* # ports on this card */
	int		sc_irq;		/* copy of attach irq */
#if NEISA > 0
	int		sc_eisa_iobase;	/* EISA io port address */
	int		sc_eisa_irq;	/* EISA irq number */
#endif
#ifdef	DEVFS
	struct {
		void	*ttya;
		void	*cuaa;
		void	*ttyl;
		void	*cual;
		void	*ttyi;
		void	*cuai;
	} devfs_token[32]; /* what is the max per card? */
	void	*control_token;
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

#ifdef POLL
static int si_pollrate;			/* in addition to irq */
static int si_realpoll;			/* poll HW on timer */

SYSCTL_INT(_machdep, OID_AUTO, si_pollrate, CTLFLAG_RW, &si_pollrate, 0, "");
SYSCTL_INT(_machdep, OID_AUTO, si_realpoll, CTLFLAG_RW, &si_realpoll, 0, "");
		 
static int init_finished = 0;
static void si_poll __P((void *));
#endif

/*
 * Array of adapter types and the corresponding RAM size. The order of
 * entries here MUST match the ordinal of the adapter type.
 */
static char *si_type[] = {
	"EMPTY",
	"SIHOST",
	"SIMCA",		/* FreeBSD does not support Microchannel */
	"SIHOST2",
	"SIEISA",
	"SIPCI",
	"SXPCI",
	"SXISA",
};

#if NPCI > 0

static char *
sipciprobe(configid, deviceid)
pcici_t configid;
pcidi_t deviceid;
{
	switch (deviceid)
	{
		case 0x400011cb:
			return("Specialix SI/XIO PCI host card");
			break;
		case 0x200011cb:
			if (pci_conf_read(configid, SIJETSSIDREG) == 0x020011cb)
				return("Specialix SX PCI host card");
			else
				return NULL;
			break;
		default:
			return NULL;
	}
	/*NOTREACHED*/
}

void
sipciattach(configid, unit)
pcici_t configid;
int unit;
{
	struct isa_device id;
	vm_offset_t vaddr,paddr;
	u_long mapval = 0;	/* shut up gcc, should not be needed */

	switch ( pci_conf_read(configid, 0) >> 16 )
	{
		case 0x4000:
			si_softc[unit].sc_type = SIPCI;
			mapval = SIPCIBADR;
			break;
		case 0x2000:
			si_softc[unit].sc_type = SIJETPCI;
			mapval = SIJETBADR;
			break;
	}
	if (!pci_map_mem(configid, mapval, &vaddr, &paddr))
	{
		printf("si%d: couldn't map memory\n", unit);
	}

	/*
	 * We're cheating here a little bit. The argument to an ISA
	 * interrupt routine is the unit number. The argument to a
	 * PCI interrupt handler is a void *, but we're simply going
	 * to be lazy and hand it the unit number.
	 */
	if (!pci_map_int(configid, (pci_inthand_t *) si_intr, (void *)unit, &tty_imask)) {
		printf("si%d: couldn't map interrupt\n", unit);
	}
	si_softc[unit].sc_typename = si_type[si_softc[unit].sc_type];

	/*
	 * More cheating: We're going to dummy up a struct isa_device
	 * and call the other attach routine. We don't really have to
	 * fill in very much of the structure, since we filled in a
	 * little of the soft state already.
	 */
	id.id_unit = unit;
	id.id_maddr = (caddr_t) vaddr;
	siattach(&id);
}

#endif

#if NEISA > 0

static const char *si_eisa_match __P((eisa_id_t id));

static const char *
si_eisa_match(id)
	eisa_id_t id;
{
	if (id == SIEISADEVID)
		return ("Specialix SI/XIO EISA host card");
	return (NULL);
}

static int
si_eisa_probe(void)
{
	struct eisa_device *ed = NULL;
	int count, irq;

	for (count=0; (ed = eisa_match_dev(ed, si_eisa_match)) != NULL; count++)
	{
		u_long port,maddr;

		port = (ed->ioconf.slot * EISA_SLOT_SIZE) + SIEISABASE;
		eisa_add_iospace(ed, port, SIEISAIOSIZE, RESVADDR_NONE);
		maddr = (inb(port+1) << 24) | (inb(port) << 16);
		irq  = ((inb(port+2) >> 4) & 0xf);
		eisa_add_mspace(ed, maddr, SIEISA_MEMSIZE, RESVADDR_NONE);
		eisa_add_intr(ed, irq);
		eisa_registerdev(ed, &si_eisa_driver);
		count++;
	}
	return count;
}

static int
si_eisa_attach(ed)
	struct eisa_device *ed;
{
	struct isa_device id;
	resvaddr_t *maddr,*iospace;
	u_int irq;
	struct si_softc *sc;

	sc = &si_softc[ed->unit];

	sc->sc_type = SIEISA;
	sc->sc_typename = si_type[sc->sc_type];

	if ((iospace = ed->ioconf.ioaddrs.lh_first) == NULL) {
		printf("si%d: no iospace??\n", ed->unit);
		return -1;
	}
	sc->sc_eisa_iobase = iospace->addr;

	irq  = ((inb(iospace->addr + 2) >> 4) & 0xf);
	sc->sc_eisa_irq = irq;

	if ((maddr = ed->ioconf.maddrs.lh_first) == NULL) {
		printf("si%d: where am I??\n", ed->unit);
		return -1;
	}
	eisa_reg_start(ed);
	if (eisa_reg_iospace(ed, iospace)) {
		printf("si%d: failed to register iospace 0x%x\n",
			ed->unit, iospace);
		return -1;
	}
	if (eisa_reg_mspace(ed, maddr)) {
		printf("si%d: failed to register memspace 0x%x\n",
			ed->unit, maddr);
		return -1;
	}
	/*
	 * We're cheating here a little bit. The argument to an ISA
	 * interrupt routine is the unit number. The argument to a
	 * EISA interrupt handler is a void *, but we're simply going
	 * to be lazy and hand it the unit number.
	 */
	if (eisa_reg_intr(ed, irq, (void (*)(void *)) si_intr,
		(void *)(ed->unit), &tty_imask, 1)) {
		printf("si%d: failed to register interrupt %d\n",
			ed->unit, irq);
		return -1;
	}
	eisa_reg_end(ed);
	if (eisa_enable_intr(ed, irq)) {
		return -1;
	}

	/*
	 * More cheating: We're going to dummy up a struct isa_device
	 * and call the other attach routine. We don't really have to
	 * fill in very much of the structure, since we filled in a
	 * little of the soft state already.
	 */
	id.id_unit = ed->unit;
	id.id_maddr = (caddr_t) pmap_mapdev(maddr->addr, SIEISA_MEMSIZE);
	return (siattach(&id));
}

#endif


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

	si_pollrate = POLLHZ;		/* default 10 per second */
#ifdef REALPOLL
	si_realpoll = 1;		/* scan always */
#endif
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

	if (si_softc[id->id_unit].sc_typename) {
		/* EISA or PCI has taken this unit, choose another */
		for (i=0; i < NSI; i++) {
			if (si_softc[i].sc_typename == NULL) {
				id->id_unit = i;
				break;
			}
		}
		if (i >= NSI) {
			DPRINT((0, DBG_AUTOBOOT|DBG_FAIL,
				"si%d: cannot realloc unit\n", id->id_unit));
			return (0);
		}
	}

	for (i=0; i < NSI; i++) {
		sc = &si_softc[i];
		if ((caddr_t)sc->sc_paddr == (caddr_t)paddr) {
			DPRINT((0, DBG_AUTOBOOT|DBG_FAIL,
				"si%d: iomem (%x) already configured to si%d\n",
				id->id_unit, sc->sc_paddr, i));
			return(0);
		}
	}

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
	 * Let's look first for a JET ISA card, since that's pretty easy
	 *
	 * All jet hosts are supposed to have this string in the IDROM,
	 * but it's not worth checking on self-IDing busses like PCI.
	 */
	{
		unsigned char *jet_chk_str = "JET HOST BY KEV#";

		for (i = 0; i < strlen(jet_chk_str); i++)
			if (jet_chk_str[i] != *(maddr + SIJETIDSTR + 2 * i))
				goto try_mk2;
	}
	DPRINT((0, DBG_AUTOBOOT|DBG_FAIL,
		"si%d: JET first check - 0x%x\n",
		id->id_unit, (*(maddr+SIJETIDBASE))));
	if (*(maddr+SIJETIDBASE) != (SISPLXID&0xff))
		goto try_mk2;
	DPRINT((0, DBG_AUTOBOOT|DBG_FAIL,
		"si%d: JET second check - 0x%x\n",
		id->id_unit, (*(maddr+SIJETIDBASE+2))));
	if (*(maddr+SIJETIDBASE+2) != ((SISPLXID&0xff00)>>8))
		goto try_mk2;
	/* It must be a Jet ISA or RIO card */
	DPRINT((0, DBG_AUTOBOOT|DBG_FAIL,
		"si%d: JET id check - 0x%x\n",
		id->id_unit, (*(maddr+SIUNIQID))));
	if ((*(maddr+SIUNIQID) & 0xf0) !=0x20)
		goto try_mk2;
	/* It must be a Jet ISA SI/XIO card */
	*(maddr + SIJETCONFIG) = 0;
	type = SIJETISA;
	ramsize = SIJET_RAMSIZE;
	goto got_card;
	/*
	 * OK, now to see if whatever responded is really an SI card.
	 * Try for a MK II next (SIHOST2)
	 */
try_mk2:
	for (i = SIPLSIG; i < SIPLSIG + 8; i++)
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

	/* It must be an SIHOST (maybe?) - there must be a better way XXX */
	type = SIHOST;
	ramsize = SIHOST_RAMSIZE;

got_card:
	DPRINT((0, DBG_AUTOBOOT, "si%d: found type %d card, try memory test\n",
		id->id_unit, type));
	/* Try the acid test */
	ux = maddr + SIRAM;
	for (i = 0; i < ramsize; i++, ux++)
		*ux = (BYTE)(i&0xff);
	ux = maddr + SIRAM;
	for (i = 0; i < ramsize; i++, ux++) {
		if ((was = *ux) != (BYTE)(i&0xff)) {
			DPRINT((0, DBG_AUTOBOOT|DBG_FAIL,
				"si%d: match fail at phys 0x%x, was %x should be %x\n",
				id->id_unit, paddr + i, was, i&0xff));
			goto fail;
		}
	}

	/* clear out the RAM */
	ux = maddr + SIRAM;
	for (i = 0; i < ramsize; i++)
		*ux++ = 0;
	ux = maddr + SIRAM;
	for (i = 0; i < ramsize; i++) {
		if ((was = *ux++) != 0) {
			DPRINT((0, DBG_AUTOBOOT|DBG_FAIL,
				"si%d: clear fail at phys 0x%x, was %x\n",
				id->id_unit, paddr + i, was));
			goto fail;
		}
	}

	/*
	 * Success, we've found a valid board, now fill in
	 * the adapter structure.
	 */
	switch (type) {
	case SIHOST2:
		if ((id->id_irq & (IRQ11|IRQ12|IRQ15)) == 0) {
bad_irq:
			DPRINT((0, DBG_AUTOBOOT|DBG_FAIL,
				"si%d: bad IRQ value - %d\n",
				id->id_unit, id->id_irq));
			return(0);
		}
		id->id_msize = SIHOST2_MEMSIZE;
		break;
	case SIHOST:
		if ((id->id_irq & (IRQ11|IRQ12|IRQ15)) == 0) {
			goto bad_irq;
		}
		id->id_msize = SIHOST_MEMSIZE;
		break;
	case SIJETISA:
		if ((id->id_irq & (IRQ9|IRQ10|IRQ11|IRQ12|IRQ15)) == 0) {
			goto bad_irq;
		}
		id->id_msize = SIJETISA_MEMSIZE;
		break;
	case SIMCA:		/* MCA */
	default:
		printf("si%d: %s not supported\n", id->id_unit, si_type[type]);
		return(0);
	}
	id->id_intr = (inthand2_t *)si_intr; /* set here instead of config */
	si_softc[id->id_unit].sc_type = type;
	si_softc[id->id_unit].sc_typename = si_type[type];
	return(-1);	/* -1 == found */
}

/*
 * We have to make an 8 bit version of bcopy, since some cards can't
 * deal with 32 bit I/O
 */
#if 1
static void
si_bcopy(const void *src, void *dst, size_t len)
{
	while (len--)
		*(((u_char *)dst)++) = *(((u_char *)src)++);
}
#else
#define si_bcopy bcopy
#endif


/*
 * Attach the device.  Initialize the card.
 *
 * This routine also gets called by the EISA and PCI attach routines.
 * It presumes that the softstate for the unit has had had its type field
 * and the EISA specific stuff filled in, as well as the kernel virtual
 * base address and the unit number of the isa_device struct.
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

	DPRINT((0, DBG_AUTOBOOT, "si%d: siattach\n", id->id_unit));

	sc->sc_paddr = (caddr_t)vtophys(id->id_maddr);
	sc->sc_maddr = id->id_maddr;
	sc->sc_irq = id->id_irq;

	DPRINT((0, DBG_AUTOBOOT, "si%d: type: %s paddr: %x maddr: %x\n", unit,
		sc->sc_typename, sc->sc_paddr, sc->sc_maddr));

	sc->sc_ports = NULL;			/* mark as uninitialised */

	maddr = sc->sc_maddr;

	/* Stop the CPU first so it won't stomp around while we load */

	switch (sc->sc_type) {
#if NEISA > 0
		case SIEISA:
			outb(sc->sc_eisa_iobase + 2, sc->sc_eisa_irq << 4);
		break;
#endif
#if NPCI > 0
		case SIPCI:
			*(maddr+SIPCIRESET) = 0;
		break;
		case SIJETPCI: /* fall through to JET ISA */
#endif
		case SIJETISA:
			*(maddr+SIJETCONFIG) = 0;
		break;
		case SIHOST2:
			*(maddr+SIPLRESET) = 0;
		break;
		case SIHOST:
			*(maddr+SIRESET) = 0;
		break;
		default: /* this should never happen */
			printf("si%d: unsupported configuration\n", unit);
			return 0;
		break;
	}

	/* OK, now lets download the download code */

	if ((sc->sc_type == SIJETISA) || (sc->sc_type == SIJETPCI)) {
		DPRINT((0, DBG_DOWNLOAD, "si%d: jet_download: nbytes %d\n",
			id->id_unit, si3_t225_dsize));
		si_bcopy(si3_t225_download, maddr + si3_t225_downloadaddr,
			si3_t225_dsize);
		DPRINT((0, DBG_DOWNLOAD,
			"si%d: jet_bootstrap: nbytes %d -> %x\n",
			id->id_unit, si3_t225_bsize, si3_t225_bootloadaddr));
		si_bcopy(si3_t225_bootstrap, maddr + si3_t225_bootloadaddr,
			si3_t225_bsize);
	} else {
		DPRINT((0, DBG_DOWNLOAD, "si%d: si_download: nbytes %d\n",
			id->id_unit, si2_z280_dsize));
		si_bcopy(si2_z280_download, maddr + si2_z280_downloadaddr,
			si2_z280_dsize);
	}

	/* Now start the CPU */

	switch (sc->sc_type) {
#if NEISA > 0
	case SIEISA:
		/* modify the download code to tell it that it's on an EISA */
		*(maddr + 0x42) = 1;
		outb(sc->sc_eisa_iobase + 2, (sc->sc_eisa_irq << 4) | 4);
		(void)inb(sc->sc_eisa_iobase + 3); /* reset interrupt */
		break;
#endif
	case SIPCI:
		/* modify the download code to tell it that it's on a PCI */
		*(maddr+0x42) = 1;
		*(maddr+SIPCIRESET) = 1;
		*(maddr+SIPCIINTCL) = 0;
		break;
	case SIJETPCI:
		*(maddr+SIJETRESET) = 0;
		*(maddr+SIJETCONFIG) = SIJETBUSEN|SIJETIRQEN;
		break;
	case SIJETISA:
		*(maddr+SIJETRESET) = 0;
		switch (sc->sc_irq) {
		case IRQ9:
			*(maddr+SIJETCONFIG) = SIJETBUSEN|SIJETIRQEN|0x90;
			break;
		case IRQ10:
			*(maddr+SIJETCONFIG) = SIJETBUSEN|SIJETIRQEN|0xa0;
			break;
		case IRQ11:
			*(maddr+SIJETCONFIG) = SIJETBUSEN|SIJETIRQEN|0xb0;
			break;
		case IRQ12:
			*(maddr+SIJETCONFIG) = SIJETBUSEN|SIJETIRQEN|0xc0;
			break;
		case IRQ15:
			*(maddr+SIJETCONFIG) = SIJETBUSEN|SIJETIRQEN|0xf0;
			break;
		}
		break;
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
	default: /* this should _REALLY_ never happen */
		printf("si%d: Uh, it was supported a second ago...\n", unit);
		return 0;
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
		if ((sc->sc_type == SIJETISA) || (sc->sc_type == SIJETPCI)) {
			/* set throttle to 100 times per second */
			regp->int_count = JET_INT_COUNT;
			/* rx_intr_count is a NOP in Jet */
		} else {
			/* set throttle to 125 times per second */
			regp->int_count = INT_COUNT;
			/* rx intr max of 25 times per second */
			regp->rx_int_count = RXINT_COUNT;
		}
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
		printf("si%d: download code version error - initstat %x\n",
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
		switch (modp->sm_type) {
		case TA4:
			DPRINT((0, DBG_DOWNLOAD,
				"si%d: Found old TA4 module, 4 ports\n",
				unit));
			x = 4;
			break;
		case TA8:
			DPRINT((0, DBG_DOWNLOAD,
				"si%d: Found old TA8 module, 8 ports\n",
				unit));
			x = 8;
			break;
		case TA4_ASIC:
			DPRINT((0, DBG_DOWNLOAD,
				"si%d: Found ASIC TA4 module, 4 ports\n",
				unit));
			x = 4;
			break;
		case TA8_ASIC:
			DPRINT((0, DBG_DOWNLOAD,
				"si%d: Found ASIC TA8 module, 8 ports\n",
				unit));
			x = 8;
			break;
		case MTA:
			DPRINT((0, DBG_DOWNLOAD,
				"si%d: Found CD1400 module, 8 ports\n",
				unit));
			x = 8;
			break;
		case SXDC:
			DPRINT((0, DBG_DOWNLOAD,
				"si%d: Found SXDC module, 8 ports\n",
				unit));
			x = 8;
			break;
		default:
			printf("si%d: unknown module type %d\n",
				unit, modp->sm_type);
			goto try_next;
		}

		/* this was limited in firmware and is also a driver issue */
		if ((nport + x) > SI_MAXPORTPERCARD) {
			printf("si%d: extra ports ignored\n", unit);
			goto try_next;
		}

		nport += x;
		si_Nports += x;
		si_Nmodules++;

try_next:
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

	/*
	 * Scan round the ports again, this time initialising.
	 */
	pp = sc->sc_ports;
	nmodule = 0;
	modp = (struct si_module *)(maddr + 0x80);
	uart_type = 1000;	/* arbitary, > uchar_max */
	for (;;) {
		switch (modp->sm_type) {
		case TA4:
			nport = 4;
			break;
		case TA8:
			nport = 8;
			break;
		case TA4_ASIC:
			nport = 4;
			break;
		case TA8_ASIC:
			nport = 8;
			break;
		case MTA:
			nport = 8;
			break;
		case SXDC:
			nport = 8;
			break;
		default:
			goto try_next2;
		}
		nmodule++;
		ccbp = (struct si_channel *)((char *)modp + 0x100);
		if (uart_type == 1000)
			uart_type = ccbp->type;
		else if (uart_type != ccbp->type)
			printf("si%d: Warning: module %d mismatch! (%d%s != %d%s)\n",
			    unit, nmodule,
			    ccbp->type, si_modulename(sc->sc_type, ccbp->type),
			    uart_type, si_modulename(sc->sc_type, uart_type));

		for (x = 0; x < nport; x++, pp++, ccbp++) {
			pp->sp_ccb = ccbp;	/* save the address */
			pp->sp_tty = tp++;
			pp->sp_pend = IDLE_CLOSE;
			pp->sp_state = 0;	/* internal flag */
			pp->sp_dtr_wait = 3 * hz;
			pp->sp_iin.c_iflag = TTYDEF_IFLAG;
			pp->sp_iin.c_oflag = TTYDEF_OFLAG;
			pp->sp_iin.c_cflag = TTYDEF_CFLAG;
			pp->sp_iin.c_lflag = TTYDEF_LFLAG;
			termioschars(&pp->sp_iin);
			pp->sp_iin.c_ispeed = pp->sp_iin.c_ospeed =
				TTYDEF_SPEED;;
			pp->sp_iout = pp->sp_iin;
		}
try_next2:
		if (modp->sm_next == 0) {
			printf("si%d: card: %s, ports: %d, modules: %d, type: %d%s\n",
				unit,
				sc->sc_typename,
				sc->sc_nport,
				nmodule,
				uart_type,
				si_modulename(sc->sc_type, uart_type));
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
	for ( x = 0; x < sc->sc_nport; x++ ) {
		/* sync with the manuals that start at 1 */
		y = x + 1 + id->id_unit * (1 << SI_CARDSHIFT);
		sc->devfs_token[x].ttya = devfs_add_devswf(
			&si_cdevsw, x,
			DV_CHR, 0, 0, 0600, "ttyA%02d", y);
		sc->devfs_token[x].cuaa = devfs_add_devswf(
			&si_cdevsw, x + 0x00080,
			DV_CHR, 0, 0, 0600, "cuaA%02d", y);
		sc->devfs_token[x].ttyi = devfs_add_devswf(
			&si_cdevsw, x + 0x10000,
			DV_CHR, 0, 0, 0600, "ttyiA%02d", y);
		sc->devfs_token[x].cuai = devfs_add_devswf(
			&si_cdevsw, x + 0x10080,
			DV_CHR, 0, 0, 0600, "cuaiA%02d", y);
		sc->devfs_token[x].ttyl = devfs_add_devswf(
			&si_cdevsw, x + 0x20000,
			DV_CHR, 0, 0, 0600, "ttylA%02d", y);
		sc->devfs_token[x].cual = devfs_add_devswf(
			&si_cdevsw, x + 0x20080,
			DV_CHR, 0, 0, 0600, "cualA%02d", y);
	}
	sc->control_token = 
		devfs_add_devswf(&si_cdevsw, 0x40000, DV_CHR, 0, 0, 0600, 
				 "si_control");
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
		if ((error = suser(p->p_ucred, &p->p_acflag)))
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
		timeout(si_poll, (caddr_t)0L, si_pollrate);
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
		untimeout(si_lstart, (caddr_t)pp, pp->lstart_ch);
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
	    || (!pp->sp_active_out
		&& !(ccbp->hi_ip & IP_DCD)
		&& !(pp->sp_iin.c_cflag && CLOCAL))
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
		if ((error = ttysleep(tp, (caddr_t)pp, TTOPRI|PCATCH,
				     "siwrite", tp->t_timeout))) {
			if (error == EWOULDBLOCK)
				error = EIO;
			goto out;
		}
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
			ct = IS_CALLOUT(mynor) ? &pp->sp_lout : &pp->sp_lin;
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
	case TIOCSETAW:
	case TIOCSETAF:
	case TIOCDRAIN:
#ifdef COMPAT_43
	case TIOCSETP:
#endif
		blocked++;	/* block writes for ttywait() and siparam() */
		si_write_enable(pp, 0);
	}

	error = (*linesw[tp->t_line].l_ioctl)(tp, cmd, data, flag, p);
	if (error != ENOIOCTL)
		goto out;

	oldspl = spltty();

	error = ttioctl(tp, cmd, data, flag);
	si_disc_optim(tp, &tp->t_termios, pp);
	if (error != ENOIOCTL)
		goto outspl;

	switch (cmd) {
	case TIOCSBRK:
		si_command(pp, SBREAK, SI_WAIT);
		break;
	case TIOCCBRK:
		si_command(pp, EBREAK, SI_WAIT);
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

#define SUCHECK if ((error = suser(p->p_ucred, &p->p_acflag))) goto out

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
		si_bcopy(xpp, &sps->tc_siport, sizeof(sps->tc_siport));
		break;
	case TCSI_CCB:
		SUCHECK;
		si_bcopy((char *)xpp->sp_ccb, &sps->tc_ccb, sizeof(sps->tc_ccb));
		break;
	case TCSI_TTY:
		SUCHECK;
		si_bcopy(xpp->sp_tty, &sps->tc_tty, sizeof(sps->tc_tty));
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

	/* XXX - if Jet host and SXDC module, use extended baud rates */

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

	ccbp->hi_mr2 = val;

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
		 * or so. The test (int_scounter >= 200) won't correspond
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
	if (lost || si_realpoll)
		si_intr(-1);	/* call intr with fake vector */
out:
	splx(oldspl);

	timeout(si_poll, (caddr_t)0L, si_pollrate);
}
#endif	/* ifdef POLL */

/*
 * The interrupt handler polls ALL ports on ALL adapters each time
 * it is called.
 */

static BYTE si_rxbuf[SI_BUFFERSIZE];	/* input staging area */
static BYTE si_txbuf[SI_BUFFERSIZE];	/* output staging area */

static void
si_intr(int unit)
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

	DPRINT((0, (unit < 0) ? DBG_POLL:DBG_INTR, "si_intr(%d)\n", unit));
	if (in_intr) {
		if (unit < 0)	/* should never happen */
			printf("si%d: Warning poll entered during interrupt\n",
				unit);
		else
			printf("si%d: Warning interrupt handler re-entered\n",
				unit);
		return;
	}
	in_intr = 1;

	/*
	 * When we get an int we poll all the channels and do ALL pending
	 * work, not just the first one we find. This allows all cards to
	 * share the same vector.
	 *
	 * XXX - But if we're sharing the vector with something that's NOT
	 * a SI/XIO/SX card, we may be making more work for ourselves.
	 */
	for (card = 0; card < NSI; card++) {
		sc = &si_softc[card];
		if (sc->sc_type == SIEMPTY)
			continue;

		/*
		 * First, clear the interrupt
		 */
		switch(sc->sc_type) {
		case SIHOST:
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
#if NPCI > 0
		case SIPCI:
			maddr = sc->sc_maddr;
			((volatile struct si_reg *)maddr)->int_pending = 0;
			*(maddr+SIPCIINTCL) = 0x0;
			break;
		case SIJETPCI:	/* fall through to JETISA case */
#endif
		case SIJETISA:
			maddr = sc->sc_maddr;
			((volatile struct si_reg *)maddr)->int_pending = 0;
			*(maddr+SIJETINTCL) = 0x0;
			break;
#if NEISA > 0
		case SIEISA:
			maddr = sc->sc_maddr;
			((volatile struct si_reg *)maddr)->int_pending = 0;
			(void)inb(sc->sc_eisa_iobase + 3);
			break;
#endif
		case SIEMPTY:
		default:
			continue;
		}
		((volatile struct si_reg *)maddr)->int_scounter = 0;

		/*
		 * check each port
		 */
		for (pp = sc->sc_ports, port=0; port < sc->sc_nport;
		     pp++, port++) {
			ccbp = pp->sp_ccb;
			tp = pp->sp_tty;

			/*
			 * See if a command has completed ?
			 */
			if (ccbp->hi_stat != pp->sp_pend) {
				DPRINT((pp, DBG_INTR,
					"si_intr hi_stat = 0x%x, pend = %d\n",
					ccbp->hi_stat, pp->sp_pend));
				switch(pp->sp_pend) {
				case LOPEN:
				case MPEND:
				case MOPEN:
				case CONFIG:
				case SBREAK:
				case EBREAK:
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
			 * Check to see if we should 'receive' characters.
			 */
			if (tp->t_state & TS_CONNECTED &&
			    tp->t_state & TS_ISOPEN)
				isopen = 1;
			else
				isopen = 0;

			/*
			 * Do input break processing
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
			 * If the tty input buffers are blocked, stop emptying
			 * the incoming buffers and let the auto flow control
			 * assert..
			 */
			if (tp->t_state & TS_TBLOCK) {
				goto end_rx;
			}

			/*
			 * Process read characters if not skipped above
			 */
			op = ccbp->hi_rxopos;
			ip = ccbp->hi_rxipos;
			c = ip - op;
			if (c == 0) {
				goto end_rx;
			}

			n = c & 0xff;
			if (n > 250)
				n = 250;

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
				si_bcopy((caddr_t)z, si_rxbuf, n);

				op += n;
			} else {
				x = SI_BUFFERSIZE - op;

				DPRINT((pp, DBG_INTR, "\tdouble part 1 %d\n", x));
				z = ccbp->hi_rxbuf + op;
				si_bcopy((caddr_t)z, si_rxbuf, x);

				DPRINT((pp, DBG_INTR, "\tdouble part 2 %d\n",
					n - x));
				z = ccbp->hi_rxbuf;
				si_bcopy((caddr_t)z, si_rxbuf + x, n - x);

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
	DPRINT((0, (unit < 0) ? DBG_POLL:DBG_INTR, "end si_intr(%d)\n", unit));
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
	BYTE ipos;
	int nchar;
	int oldspl, count, n, amount, buffer_full;

	oldspl = spltty();

	qp = &tp->t_outq;
	pp = TP2PP(tp);

	DPRINT((pp, DBG_ENTRY|DBG_START,
		"si_start(%x) t_state %x sp_state %x t_outq.c_cc %d\n",
		tp, tp->t_state, pp->sp_state, qp->c_cc));

	if (tp->t_state & (TS_TIMEOUT|TS_TTSTOP))
		goto out;

	buffer_full = 0;
	ccbp = pp->sp_ccb;

	count = (int)ccbp->hi_txipos - (int)ccbp->hi_txopos;
	DPRINT((pp, DBG_START, "count %d\n", (BYTE)count));

	while ((nchar = qp->c_cc) > 0) {
		if ((BYTE)count >= 255) {
			buffer_full++;
			break;
		}
		amount = min(nchar, (255 - (BYTE)count));
		ipos = (unsigned int)ccbp->hi_txipos;
		n = q_to_b(&tp->t_outq, si_txbuf, amount);
		/* will it fit in one lump? */
		if ((SI_BUFFERSIZE - ipos) >= n) {
			si_bcopy(si_txbuf, (char *)&ccbp->hi_txbuf[ipos], n);
		} else {
			si_bcopy(si_txbuf, (char *)&ccbp->hi_txbuf[ipos],
				SI_BUFFERSIZE - ipos);
			si_bcopy(si_txbuf + (SI_BUFFERSIZE - ipos),
				(char *)&ccbp->hi_txbuf[0],
				n - (SI_BUFFERSIZE - ipos));
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

	if (tp->t_state & TS_BUSY)
	{
		int time;

		time = ttspeedtab(tp->t_ospeed, chartimes);

		if (time > 0) {
			if (time < nchar)
				time = nchar / time;
			else
				time = 2;
		} else {
			DPRINT((pp, DBG_START,
				"bad char time value! %d\n", time));
			time = hz/10;
		}

		if ((pp->sp_state & (SS_LSTART|SS_INLSTART)) == SS_LSTART) {
			untimeout(si_lstart, (caddr_t)pp, pp->lstart_ch);
		} else {
			pp->sp_state |= SS_LSTART;
		}
		DPRINT((pp, DBG_START, "arming lstart, time=%d\n", time));
		pp->lstart_ch = timeout(si_lstart, (caddr_t)pp, time);
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
si_lstart(void *arg)
{
	register struct si_port *pp = arg;
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
 * Issue a command to the host card CPU.
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
	/* XXX: sits in IDLE_BREAK until something disturbs it or break
	 * is turned off. */
	while((x = ccbp->hi_stat) != IDLE_OPEN &&
			x != IDLE_CLOSE &&
			x != IDLE_BREAK &&
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
	/* it should now be in IDLE_{OPEN|CLOSE|BREAK}, or "cmd" */

	/* if there was a pending command, cause a state-change wakeup */
	switch(pp->sp_pend) {
	case LOPEN:
	case MPEND:
	case MOPEN:
	case CONFIG:
	case SBREAK:
	case EBREAK:
		wakeup(&pp->sp_state);
		break;
	default:
		break;
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
		} else while(ccbp->hi_stat != IDLE_OPEN &&
			     ccbp->hi_stat != IDLE_BREAK) {
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
	pp->sp_hotchar = linesw[tp->t_line].l_hotchar;
	DPRINT((pp, DBG_OPTIM, "bypass: %s, hotchar: %x\n", 
		(tp->t_state & TS_CAN_BYPASS_L_RINT) ? "on" : "off",
		pp->sp_hotchar));
}


#ifdef	SI_DEBUG

static void
#ifdef __STDC__
si_dprintf(struct si_port *pp, int flags, const char *fmt, ...)
#else
si_dprintf(pp, flags, fmt, va_alist)
	struct si_port *pp;
	int flags;
	char *fmt;
#endif
{
	va_list ap;

	if ((pp == NULL && (si_debug&flags)) ||
	    (pp != NULL && ((pp->sp_debug&flags) || (si_debug&flags)))) {
		if (pp != NULL)
			printf("%ci%d(%d): ", 's',
				(int)SI_CARD(pp->sp_tty->t_dev),
				(int)SI_PORT(pp->sp_tty->t_dev));
		va_start(ap, fmt);
		vprintf(fmt, ap);
		va_end(ap);
	}
}

static char *
si_mctl2str(cmd)
	enum si_mctl cmd;
{
	switch (cmd) {
	case GET:
		return("GET");
	case SET:
		return("SET");
	case BIS:
		return("BIS");
	case BIC:
		return("BIC");
	}
	return("BAD");
}

#endif	/* DEBUG */

static char *
si_modulename(host_type, uart_type)
	int host_type, uart_type;
{
	switch (host_type) {
	/* Z280 based cards */
#if NEISA > 0
	case SIEISA: 
#endif
	case SIHOST2:  
	case SIHOST:
#if NPCI > 0
	case SIPCI:
#endif
		switch (uart_type) {
		case 0:
			return(" (XIO)");
		case 1:
			return(" (SI)");
		}
		break;
	/* T225 based hosts */
#if NPCI > 0
	case SIJETPCI:
#endif
	case SIJETISA:
		switch (uart_type) {
		case 0:
			return(" (SI)");
		case 40:
			return(" (XIO)");
		case 80:
			return(" (SX)");
		}
		break;
	}
	return("");
}

static si_devsw_installed = 0;

static void 
si_drvinit(void *unused)
{
	dev_t dev;

	if (!si_devsw_installed) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev,&si_cdevsw, NULL);
		si_devsw_installed = 1;
	}
}

SYSINIT(sidev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,si_drvinit,NULL)
