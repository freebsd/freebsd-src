/* $NetBSD: dec_2100_a50.c,v 1.39 1998/04/17 02:45:19 mjacob Exp $ */
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
 * Additional Copyright (c) 1998 by Andrew Gallatin for Duke University.
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

#include "sc.h"
#ifndef	CONSPEED
#define	CONSPEED TTYDEF_SPEED
#endif
static int comcnrate = CONSPEED;

void dec_2100_a50_init(void);
static void dec_2100_a50_cons_init(void);
static void dec_2100_a50_intr_map(void *);
void sio_intr_establish(int);
void sio_intr_disestablish(int);
void sio_intr_setup(void);

extern int siocnattach(int, int);
extern int siogdbattach(int, int);
extern int sccnattach(void);

const struct alpha_variation_table dec_2100_a50_variations[] = {
	{ SV_ST_AVANTI,	"AlphaStation 400 4/233 (\"Avanti\")" },
	{ SV_ST_MUSTANG2_4_166, "AlphaStation 200 4/166 (\"Mustang II\")" },
	{ SV_ST_MUSTANG2_4_233, "AlphaStation 200 4/233 (\"Mustang II\")" },
	{ SV_ST_AVANTI_4_266, "AlphaStation 250 4/266" },
	{ SV_ST_MUSTANG2_4_100, "AlphaStation 200 4/100 (\"Mustang II\")" },
	{ SV_ST_AVANTI_4_233, "AlphaStation 255/233" },
	{ 0, NULL },
};

void
dec_2100_a50_init()
{
	u_int64_t variation;

	platform.family = "AlphaStation 200/400 (\"Avanti\")";

	if ((platform.model = alpha_dsr_sysname()) == NULL) {
		variation = hwrpb->rpb_variation & SV_ST_MASK;
		if (variation == SV_ST_AVANTI_XXX) {
			/* XXX apparently the same? */
			variation = SV_ST_AVANTI;
		}
		if ((platform.model = alpha_variation_name(variation,
		    dec_2100_a50_variations)) == NULL)
			platform.model = alpha_unknown_sysname();
	}

	platform.iobus = "apecs";
	platform.cons_init = dec_2100_a50_cons_init;
	platform.pci_intr_map  = dec_2100_a50_intr_map;
}

/* XXX for forcing comconsole when srm serial console is used */
extern int comconsole;

static void
dec_2100_a50_cons_init()
{
	struct ctb *ctb;

	apecs_init();

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
			 * character time = (1000000 / (defaultrate / 10)).
			 */
			DELAY(160000000 / comcnrate);
			/*
			 * Force a comconsole on com1 if the SRM has a serial console.
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
#if NSC > 0
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

#define	SIO_PCIREG_PIRQ_RTCTRL	0x60    /* PIRQ0 Route Control */

void
dec_2100_a50_intr_map(void *arg)
{
	u_int8_t pirqline;
	u_int32_t pirqreg;
	int pirq;
	pcicfgregs *cfg;

	pirq = 0;  /* gcc -Wuninitialized XXX */
	cfg = (pcicfgregs *)arg;

	/*
	 * Slot->interrupt translation.  Taken from NetBSD.
	 */

	if(cfg->intpin == 0)
		return;

	if(cfg->intpin > 4)
		panic("dec_2100_a50_intr_map: bad intpin %d",cfg->intpin);

	switch (cfg->slot) {
	case 6:					/*  NCR SCSI */
		pirq = 3;
		break;

	case 11:				/* slot 1 */
	case 14:				/* slot 3 */
		switch(cfg->intpin) {
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
		default:
			panic("dec_2100_a50_intr_map bogus PCI pin %d\n",
			    cfg->intpin);

		}
		break;
	case 12:				/* slot 2 */
		switch (cfg->intpin) {
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
		default:
			panic("dec_2100_a50_intr_map bogus PCI pin %d\n",
			    cfg->intpin);

		};
		break;

	case 13:				/* slot 3 */
		switch (cfg->intpin) {
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
		};
		break;
default:
		printf("dec_2100_a50_intr_map: weird slot %d\n",
		    cfg->slot);

		/* return; */
        }

	/*
	 *  Read the SIO IRQ routing register to determine where the
	 *  interrupt will actually be routed.  Thank you, NetBSD.
	 */
	   
	pirqreg = apecs_pcib_read_config(0, 0, 7, 0,
					 SIO_PCIREG_PIRQ_RTCTRL, 4);
	pirqline = (pirqreg >> (pirq * 8)) & 0xff;
	if ((pirqline & 0x80) != 0)
		panic("bad pirqline %d",pirqline);
	pirqline &= 0xf;
	cfg->intline = pirqline;
}
