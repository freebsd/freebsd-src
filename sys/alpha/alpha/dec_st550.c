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
/*
 * Additional Copyright (c) 1998 by Andrew Gallatin for Duke University
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ddb.h"
#include "opt_dev_sc.h"

#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/termios.h>

#include <machine/clock.h>
#include <machine/cpuconf.h>
#include <machine/intr.h>
#include <machine/md_var.h>
#include <machine/rpb.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <alpha/pci/ciareg.h>
#include <alpha/pci/ciavar.h>

#ifndef	CONSPEED
#define	CONSPEED TTYDEF_SPEED
#endif
static int comcnrate = CONSPEED;

void st550_init(void);
static void st550_cons_init(void);
static void st550_intr_init(void);
static void pyxis_intr_enable(int);
static void pyxis_intr_disable(int);
static void st550_intr_enable(int);
static void st550_intr_disable(int);
static void st550_intr_map(void *);
#define ST550_PCI_IRQ_BEGIN 8
#define ST550_PCI_MAX_IRQ  47

extern int siocnattach(int, int);
extern int siogdbattach(int, int);
extern int sccnattach(void);

void
st550_init()
{

	platform.family = "Digital Personal Workstation (Miata)";

	if ((platform.model = alpha_dsr_sysname()) == NULL) {
		/* XXX Don't know the system variations, yet. */
		platform.model = alpha_unknown_sysname();
	}

	platform.iobus = "cia";
	platform.cons_init = st550_cons_init;
	platform.pci_intr_init = st550_intr_init;
	platform.pci_intr_map = st550_intr_map;
	platform.pci_intr_disable = st550_intr_disable;
	platform.pci_intr_enable = st550_intr_enable;
}

extern int comconsole;

static void
st550_cons_init()
{
	struct ctb *ctb;

	cia_init();

#ifdef DDB
	siogdbattach(0x2f8, 57600);
#endif

	ctb = (struct ctb *)(((caddr_t)hwrpb) + hwrpb->rpb_ctb_off);

	switch (ctb->ctb_term_type) {
	case 2:
		/* serial console ... */
		/* XXX */
		/*
		 * Delay to allow PROM putchars to complete.
		 * FIFO depth * character time,
		 * character time = (1000000 / (defaultrate / 10))
		 */
		DELAY(160000000 / comcnrate);
		comconsole = 0;
		if (siocnattach(0x3f8, comcnrate))
			panic("can't init serial console");

		boothowto |= RB_SERIAL;
		break;

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

		panic("consinit: unknown console type %ld\n",
		    ctb->ctb_term_type);
	}
}

static void
st550_intr_init()
{

	/* This is here because we need to disable extraneous pci interrupts. */
	int i;
	for(i = ST550_PCI_IRQ_BEGIN; i <= ST550_PCI_MAX_IRQ; i++)
		pyxis_intr_disable(i);
	/* From Linux... */
	pyxis_intr_enable(2);	/* enable HALT switch */
	pyxis_intr_enable(6);	/* enable timer */
	pyxis_intr_enable(7);	/* enable ISA PIC cascade */
}

static void
st550_intr_map(void *arg)
{
	pcicfgregs *cfg;

	cfg = (pcicfgregs *)arg;

	/* There are two main variants of Miata: Miata 1 (Intel SIO)
	 * and Miata {1.5,2} (Cypress).
	 *
	 * The Miata 1 has a CMD PCI IDE wired to compatibility mode at
	 * slot 4 of bus 0.  This variant  has the Pyxis DMA bug.
	 *
	 * On the Miata 1.5 and Miata 2, the Cypress PCI-ISA bridge lives
	 * on device 7 of bus 0.  This device has PCI IDE wired to
	 * compatibility mode on functions 1 and 2.
	 *
	 * There will be no interrupt mapping for these devices, so just
	 * bail out now.
	 */
	if(cfg->bus == 0) {
		if ((hwrpb->rpb_variation & SV_ST_MASK) < SV_ST_MIATA_1_5) {
			/* Miata 1 */
			if (cfg->slot == 7)
				return;
			else if (cfg->func == 4)
				return;
		} else {
			/* Miata 1.5 or Miata 2 */
			if (cfg->slot == 7) {
				if (cfg->func == 0)
					return;
				return;
			}
		}
	}
	/* Account for the PCI interrupt offset. */
	/* cfg->intline += ST550_PCI_IRQ_BEGIN; */
	return;
}

/*
 * The functions below were written based on a draft copy of the
 * 21174 TRM.
 */
static void
pyxis_intr_enable(irq)
	int irq;
{
	volatile u_int64_t temp;

	alpha_mb();
	temp = REGVAL64(PYXIS_INT_MASK);
	alpha_mb();

	temp |= ( 1L << irq );
	REGVAL64(PYXIS_INT_MASK) = temp;
	alpha_mb();
	temp = REGVAL64(PYXIS_INT_MASK);
#if 0
	printf("pyxis_intr_enable: enabling %d, current mask= ", irq);
	{
		int i;
		for ( i = 0; i < 61; i++)
			if (temp & (1 << i)) {
				printf("%d " , i);
			}
		printf("\n");
	}
#endif

}

static void
pyxis_intr_disable(irq)
        int irq;
{
        volatile u_int64_t temp;

        alpha_mb();
        temp =  REGVAL64(PYXIS_INT_MASK);
        temp &= ~(1L << irq );
        REGVAL64(PYXIS_INT_MASK) = temp;
        alpha_mb();
        temp = REGVAL64(PYXIS_INT_MASK);
#if 0
	printf("pyxis_intr_disable: disabled %d, current mask ", irq);
	{
		int i;
		for ( i = 0; i < 61; i++)
			if (temp & (1 << i)) {
				printf("%d ", i);
			}
	printf("\n");
	}
#endif

}

static void
st550_intr_enable(irq)
	int irq;
{

	pyxis_intr_enable(irq + ST550_PCI_IRQ_BEGIN);
}

static void
st550_intr_disable(irq)
	int irq;
{

	pyxis_intr_disable(irq + ST550_PCI_IRQ_BEGIN);
}
