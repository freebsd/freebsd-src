/* $FreeBSD$ */

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
 * FreeBSD version based on:
 *     NetBSD: dec_eb64plus.c,v 1.15 1998/11/19 02:20:07 ross Exp
 *
 * Some info on the Aspen Alpine which might be hard to come by:
 * - Hardware is close enough to the DEC EB64+ design to allow it to run
 *   the EB64+ SRM console firmware
 * - 3 PCI slots, closest to the SIMMs: Alpine calls this one slot C
 *                the middle one Alpine calls slot B
 *		  the 3rd one Alpine calls slot A
 *	(A, B, C are silkscreened on the PCB)
 * - embedded NCR810, located at PCI slot 5
 * - 3 ISA slots, hanging off an Intel 82378IB PCI-ISA bridge at PCI slot 8
 * - embedded floppy, PC keyboard interface, PS/2 mouse interface, 2 serial
 *   ports and a parallel port. All of this hanging off the ISA bridge
 */

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/systm.h>
#include <sys/termios.h>
#include <sys/bus.h>

#include <machine/rpb.h>
#include <machine/cpuconf.h>
#include <machine/clock.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <alpha/pci/apecsreg.h>
#include <alpha/pci/apecsvar.h>

#include "opt_dev_sc.h"

#ifndef	CONSPEED
#define	CONSPEED TTYDEF_SPEED
#endif
static int comcnrate = CONSPEED;

void dec_eb64plus_init(void);
static void dec_eb64plus_cons_init(void);
static void dec_eb64plus_intr_init(void);

extern void eb64plus_intr_enable(int irq); 	/* ../pci/pci_eb64plus_intr.s */
extern void eb64plus_intr_disable(int irq);	/* ../pci/pci_eb64plus_intr.s */

extern const char * bootdev_protocol(void);
extern int bootdev_boot_dev_type(void);

extern int siocnattach(int, int);
extern int sccnattach(void);

const struct alpha_variation_table dec_eb64plus_variations[] = {
	{ 0, "DEC EB64-plus" },
	{ 0, NULL },
};

void
dec_eb64plus_init()
{
	u_int64_t variation;

	platform.family = "EB64+";

	if ((platform.model = alpha_dsr_sysname()) == NULL) {
		variation = hwrpb->rpb_variation & SV_ST_MASK;
		if ((platform.model = alpha_variation_name(variation,
		    dec_eb64plus_variations)) == NULL)
			platform.model = alpha_unknown_sysname();
	}

	platform.iobus 			= "apecs";
	platform.cons_init 		= dec_eb64plus_cons_init;
	platform.pci_intr_init 		= dec_eb64plus_intr_init;
	/* SRM handles PCI interrupt mapping */
	platform.pci_intr_map  		= NULL;	
	/* see ../pci/pci_eb64plus_intr.s for intr. dis/enable */
	platform.pci_intr_disable 	= eb64plus_intr_disable;
	platform.pci_intr_enable 	= eb64plus_intr_enable;

}

/* XXX for forcing comconsole when srm serial console is used */
extern int comconsole;

/* init the console, serial or graphics */
static void
dec_eb64plus_cons_init()
{
	struct ctb *ctb;

	apecs_init();

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
			 * force a comconsole on com1 if the SRM has a serial
			 * console.
			 */
			comconsole = 0;
			if (siocnattach(0x3f8, comcnrate))
				panic("can't init serial console");

			boothowto |= RB_SERIAL;
			break;
		}

	case 3:
#ifdef DEV_SC
		/* graphics adapter console */
		sccnattach();
#else
		panic("not configured to use display && keyboard console");
#endif
		break;

	default:
		printf("ctb->ctb_term_type = 0x%lx\n", ctb->ctb_term_type);
		printf("ctb->ctb_turboslot = 0x%lx\n", ctb->ctb_turboslot);

		panic("consinit: unknown console type %d\n",
		    (int)ctb->ctb_term_type);
	}
}

/*
 * The SRM console may have left some some interrupts enabled.
 */
static void 	
dec_eb64plus_intr_init()
{
	int i;

	/* disable all PCI interrupts */
	for(i = 0; i <= 32; i++) 	/* 32 ?? NetBSD sez so */
		eb64plus_intr_disable(i);

	/* Enable ISA-PCI cascade interrupt */
	eb64plus_intr_enable(4);

}
