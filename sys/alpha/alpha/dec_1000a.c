/* $NetBSD: dec_1000a.c,v 1.5 1999/04/15 22:06:47 thorpej Exp $ */

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is based on dec_kn20aa.c, written by Chris G. Demetriou at
 * Carnegie-Mellon University. Platform support for Noritake, Pintake, and
 * Corelle by Ross Harvey with copyright assignment by permission of Avalon
 * Computer Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the NetBSD
 *      Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

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
 * Additional Copyright (c) 1999 by Andrew Gallatin
 *
 * $FreeBSD$
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
#include <machine/bus.h>

#include <alpha/pci/apecsvar.h>
#include <alpha/pci/ciavar.h>

#include <pci/pcivar.h>

#include "opt_dev_sc.h"

#ifndef CONSPEED
#define CONSPEED TTYDEF_SPEED
#endif
static int comcnrate = CONSPEED;

void dec_1000a_init(int);
static void dec_1000a_cons_init(void);


static void dec_1000_intr_map(void *);
static void dec_1000_intr_disable(int);
static void dec_1000_intr_enable(int);
static void dec_1000_intr_init(void);

static void dec_1000a_intr_map(void *);
static void dec_1000a_intr_disable(int);
static void dec_1000a_intr_enable(int);
static void dec_1000a_intr_init(void);

extern int siocnattach(int, int);
extern int siogdbattach(int, int);
extern int sccnattach(void);


static const struct alpha_variation_table dec_1000_variations[] = {
	{ 0, "AlphaServer 1000" },
	{ 0, NULL },
};

static const struct alpha_variation_table dec_1000a_variations[] = {
	{ 0, "AlphaServer 1000A" },
	{ 0, NULL },
};

void
dec_1000a_init(int cputype)
{
	u_int64_t variation;

	platform.family = "AlphaServer 1000/1000A";

	if ((platform.model = alpha_dsr_sysname()) == NULL) {
		variation = hwrpb->rpb_variation & SV_ST_MASK;
		if ((platform.model = alpha_variation_name(variation,
		    cputype == ST_DEC_1000 ? dec_1000_variations
					   : dec_1000a_variations)) == NULL)
			platform.model = alpha_unknown_sysname();
	}

	switch(LOCATE_PCS(hwrpb, 0)->pcs_proc_type & PCS_PROC_MAJOR) {
	case PCS_PROC_EV4:
	case PCS_PROC_EV45:
		platform.iobus = "apecs";
		break;

	default:
		platform.iobus = "cia";
		break;
	}
	platform.cons_init = dec_1000a_cons_init;
	switch (cputype) {
	case ST_DEC_1000:
		platform.pci_intr_map = dec_1000_intr_map;
		platform.pci_intr_disable = dec_1000_intr_disable;
		platform.pci_intr_enable = dec_1000_intr_enable;
		platform.pci_intr_init = dec_1000_intr_init;
		break;

	default:
		platform.pci_intr_map = dec_1000a_intr_map;
		platform.pci_intr_disable = dec_1000a_intr_disable;
		platform.pci_intr_enable = dec_1000a_intr_enable;
		platform.pci_intr_init = dec_1000a_intr_init;
		break;
	}

}

/* XXX for forcing comconsole when srm serial console is used */
extern int comconsole;

static void
dec_1000a_cons_init()
{
	struct ctb *ctb;

        if(strcmp(platform.iobus, "cia") == 0) {
		cia_init();
	} else {
		apecs_init();
	}

#ifdef DDB
	siogdbattach(0x2f8, 57600);
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
                         * Force a comconsole on com1 if the SRM has a serial
			 * console.
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

		panic("consinit: unknown console type %d\n",
		    (int)ctb->ctb_term_type);
	}
}


static void
dec_1000_intr_map(arg)
	void *arg;
{
	pcicfgregs *cfg;

	cfg = (pcicfgregs *)arg;
	if (cfg->intpin == 0)	/* No IRQ used. */
		return;
	if (!(1 <= cfg->intpin && cfg->intpin <= 4))
		goto bad;

	switch(cfg->slot) {
	case 6:
		if(cfg->intpin != 1)
			break;
		cfg->intline = 0xc;		/* integrated ncr scsi */
		return;
		break;
	case 11:
	case 12:
	case 13:
		cfg->intline = (cfg->slot - 11) * 4 + cfg->intpin - 1;
		return;
		break;
	}
bad:	printf("dec_1000_intr_map: can't map dev %d pin %d\n",
	    cfg->slot, cfg->intpin);
}

/*
 * Read and write the mystery ICU IMR registers
 * on the AlphaServer 1000.
 */

#define IR() 	inw(0x536)
#define IW(x)	outw(0x536, (x))

/*
 * Enable and disable interrupts at the ICU level.
 */

static void
dec_1000_intr_enable(irq)
	int irq;
{

	IW(IR() | 1 << irq);
}

static void
dec_1000_intr_disable(irq)
	int irq;
{

	IW(IR() & ~(1 << irq));
}


static void
dec_1000_intr_init()
{
	/*
 	 * Initialize mystery ICU.
 	 */
	IW(0);					/* XXX ?? */	

	/*
 	 * Enable cascade interrupt.
 	 */
	dec_1000_intr_enable(2);
}

/*
 * Read and write the mystery ICU IMR registers
 * on the AlphaServer 1000a.
 */

#define IRA(o) 		inw(0x54a + 2*(o))
#define IWA(o, v)	outw(0x54a + 2*(o), (v))
#define IMR2IRQ(bn) 	((bn) - 1)
#define IRQ2IMR(irq) 	((irq) + 1)

static void
dec_1000a_intr_map(arg)
	void *arg;
{
	pcicfgregs *cfg;
	int device, imrbit;
	/*
	 * Get bit number in mystery ICU imr.
	 */
	static const signed char imrmap[][4] = {
#		define	IRQSPLIT(o) { (o), (o)+1, (o)+16, (o)+16+1 }
#		define	IRQNONE		 { 0, 0, 0, 0 }
		/*  0  */ { 1, 0, 0, 0 },	/* Noritake and Pintake */
		/*  1  */ IRQSPLIT(8),
		/*  2  */ IRQSPLIT(10),
		/*  3  */ IRQSPLIT(12),
		/*  4  */ IRQSPLIT(14),
		/*  5  */ { 1, 0, 0, 0 },	/* Corelle */
		/*  6  */ { 10, 0, 0, 0 },	/* Corelle */
		/*  7  */ IRQNONE,
		/*  8  */ { 1, 0, 0, 0 },	/* isp behind ppb */
		/*  9  */ IRQNONE,
		/* 10  */ IRQNONE,
		/* 11  */ IRQSPLIT(2),
		/* 12  */ IRQSPLIT(4),
		/* 13  */ IRQSPLIT(6),
		/* 14  */ IRQSPLIT(8)		/* Corelle */
	};

	cfg = (pcicfgregs *)arg;
	device = cfg->slot;

	if (cfg->intpin == 0)	/* No IRQ used. */
		return;
	if (!(1 <= cfg->intpin && cfg->intpin <= 4))
		goto bad;

	if (0 <= device && device < sizeof imrmap / sizeof imrmap[0]) {
		imrbit = imrmap[device][cfg->intpin - 1];
		if (imrbit) {
			cfg->intline = IMR2IRQ(imrbit);
			return;
		}
	}
bad:	printf("dec_1000a_intr_map: can't map dev %d pin %d\n",
	    device, cfg->intpin);
}


static void
dec_1000a_intr_enable(irq)
	int irq;
{
	int imrval, i;

	imrval = IRQ2IMR(irq);
	i = imrval >= 16;

	IWA(i, IRA(i) | 1 << (imrval & 0xf));
}



static void
dec_1000a_intr_disable(irq)
	int irq;
{
	int imrval, i;

	imrval = IRQ2IMR(irq);
	i = imrval >= 16;

	IWA(i, IRA(i) & ~(1 << (imrval & 0xf)));
}

static void
dec_1000a_intr_init()
{

/*
 * Initialize mystery ICU.
 */
	IWA(0, IRA(0) & 1);
	IWA(1, IRA(0) & 3);

/*
 * Enable cascade interrupt.
 */
	dec_1000a_intr_enable(2);
}
