/* $NetBSD: dec_axppci_33.c,v 1.38 1998/07/07 08:49:12 ross Exp $ */
/*
 * Copyright (c) 1995, 1996, 1997 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */
/*
 * Additional Copyright (c) 1997 by Matthew Jacob for NASA/Ames Research Center
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"
#include "opt_dev_sc.h"

#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/systm.h>
#include <sys/termios.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/clock.h>
#include <machine/cpuconf.h>
#include <machine/md_var.h>
#include <machine/rpb.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <alpha/pci/lcavar.h>

#ifndef	CONSPEED
#define	CONSPEED TTYDEF_SPEED
#endif
static int comcnrate = CONSPEED;

void dec_axppci_33_init(void);
static void dec_axppci_33_cons_init(void);
static int dec_axppci_33_intr_route (device_t, device_t, int);

extern int siocnattach(int, int);
extern int siogdbattach(int, int);
extern int sccnattach(void);

const struct alpha_variation_table dec_axppci_33_variations[] = {
	{ 0, "Alpha PC AXPpci33 (\"NoName\")" },
	{ 0, NULL },
};

#define	NSIO_PORT	0x26e	/* Hardware enabled option: 0x398 */
#define	NSIO_BASE	0
#define	NSIO_INDEX	NSIO_BASE
#define	NSIO_DATA	1
#define	NSIO_SIZE	2
#define	NSIO_CFG0	0
#define	NSIO_CFG1	1
#define	NSIO_CFG2	2
#define	NSIO_IDE_ENABLE	0x40

void
dec_axppci_33_init()
{
	int cfg0val;
	u_int64_t variation;

	platform.family = "DEC AXPpci";

	if ((platform.model = alpha_dsr_sysname()) == NULL) {
		variation = hwrpb->rpb_variation & SV_ST_MASK;
		if ((platform.model = alpha_variation_name(variation,
		    dec_axppci_33_variations)) == NULL)
			platform.model = alpha_unknown_sysname();
	}

	platform.iobus = "lca";
	platform.cons_init = dec_axppci_33_cons_init;
	platform.pci_intr_route = dec_axppci_33_intr_route;
	platform.pci_intr_map = NULL;

	lca_init();

	outb(NSIO_PORT + NSIO_INDEX, NSIO_CFG0);
	alpha_mb();
	cfg0val = inb(NSIO_PORT + NSIO_DATA);

	cfg0val |= NSIO_IDE_ENABLE;

	outb(NSIO_PORT + NSIO_INDEX, NSIO_CFG0);
	alpha_mb();
	outb(NSIO_PORT + NSIO_DATA, cfg0val);
	alpha_mb();
	outb(NSIO_PORT + NSIO_DATA, cfg0val);
}

/* XXX for forcing comconsole when srm serial console is used */
extern int comconsole;

static void
dec_axppci_33_cons_init()
{
	struct ctb *ctb;

	lca_init();

#ifdef DDB
	siogdbattach(0x2f8, 9600);
#endif
	ctb = (struct ctb *)(((caddr_t)hwrpb) + hwrpb->rpb_ctb_off);

	switch (ctb->ctb_term_type) {
	case 2:
		/* serial console ... */
		/* XXX */
		{
			/*
			 * Delay to allow PROM putchars to complete.
			 * FIFO depth * character time,
			 * character time = (1000000 / (defaultrate / 10))
			 */
			DELAY(160000000 / comcnrate);
			/*
			 * force a comconsole on com1 if the SRM has a serial console
			 */
			comconsole = 0;
			if (siocnattach(0x3f8, comcnrate))
				panic("can't init serial console");

			boothowto |= RB_SERIAL;
			break;
		}

	case 3:
		/* display console ... */
		/* XXX */
#ifdef DEV_SC
		sccnattach();
#else
		panic("not configured to use display && keyboard console");
#endif
		break;

	default:
		printf("ctb->ctb_term_type = 0x%lx\n", ctb->ctb_term_type);
		printf("ctb->ctb_turboslot = 0x%lx\n", ctb->ctb_turboslot);

		panic("consinit: unknown console type");
	}
}

#define	SIO_PCIREG_PIRQ_RTCTRL	0x60	/* PIRQ0 Route Control */

static int
dec_axppci_33_intr_route(device_t pcib, device_t dev, int pin)
{
	int pirq;
	u_int32_t pirqreg;
	u_int8_t pirqline;

#ifndef DIAGNOSTIC
	pirq = 0;				/* XXX gcc -Wuninitialized */
#endif

	/*
	 * Slot->interrupt translation.  Taken from NetBSD.
	 */

	if (pin == 0) {
		/* No IRQ used. */
		return -1;
	}
	if (pin > 4) {
		printf("dec_axppci_33_intr_route: bad interrupt pin %d\n", pin);
		return -1;
	}

	switch (pci_get_slot(dev)) {
	case 6:					/* NCR SCSI */
		pirq = 3;
		break;

	case 11:				/* slot 1 */
		switch (pin) {
		case 1:
		case 4:
			pirq = 0;
			break;
		case 2:
			pirq = 2;
			break;
		case 3:
			pirq = 1;
			break;
#ifdef DIAGNOSTIC
		default:			/* XXX gcc -Wuninitialized */
			panic("dec_axppci_33_intr_route: bogus PCI pin %d\n",
			    pin);
#endif
		};
		break;

	case 12:				/* slot 2 */
		switch (pin) {
		case 1:
		case 4:
			pirq = 1;
			break;
		case 2:
			pirq = 0;
			break;
		case 3:
			pirq = 2;
			break;
#ifdef DIAGNOSTIC
		default:			/* XXX gcc -Wuninitialized */
			panic("dec_axppci_33_intr_route: bogus PCI pin %d\n",
			    pin);
#endif
		};
		break;

	case 8:				/* slot 3 */
		switch (pin) {
		case 1:
		case 4:
			pirq = 2;
			break;
		case 2:
			pirq = 1;
			break;
		case 3:
			pirq = 0;
			break;
#ifdef DIAGNOSTIC
		default:			/* XXX gcc -Wuninitialized */
			panic("dec_axppci_33_intr_route: bogus PCI pin %d\n",
			    pin);
#endif
		};
		break;

	default:
		printf("dec_axppci_33_intr_route: weird device number %d\n",
		    pci_get_slot(dev));
		return -1;
	}

	pirqreg = lca_pcib_read_config(0, 0, 7, 0,
				       SIO_PCIREG_PIRQ_RTCTRL, 4);
	pirqline = (pirqreg >> (pirq * 8)) & 0xff;
	if ((pirqline & 0x80) != 0)
		panic("bad pirqline %d",pirqline);
	pirqline &= 0xf;

	return(pirqline);
}
