/* $NetBSD: dec_kn20aa.c,v 1.38 1998/04/17 02:45:19 mjacob Exp $ */
/*-
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
/*-
 * Additional Copyright (c) 1997 by Matthew Jacob for NASA/Ames Research Center
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/systm.h>
#include <sys/termios.h>
#include <sys/bus.h>

#include <machine/clock.h>
#include <machine/cpuconf.h>
#include <machine/md_var.h>
#include <machine/rpb.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <alpha/pci/ciareg.h>
#include <alpha/pci/ciavar.h>

void dec_kn20aa_init(void);
static void dec_kn20aa_cons_init(void);
static void dec_kn20aa_intr_init(void);
static void dec_kn20aa_intr_map(void *);
static void dec_kn20aa_intr_disable(int);
static void dec_kn20aa_intr_enable(int);

#if 0
static void dec_kn20aa_device_register(struct device *, void *);
#endif

const struct alpha_variation_table dec_kn20aa_variations[] = {
	{ 0, "AlphaStation 500 or 600 (KN20AA)" },
	{ 0, NULL },
};

void
dec_kn20aa_init()
{
	u_int64_t variation;

	platform.family = "AlphaStation 500 or 600 (KN20AA)";

	if ((platform.model = alpha_dsr_sysname()) == NULL) {
		variation = hwrpb->rpb_variation & SV_ST_MASK;
		if ((platform.model = alpha_variation_name(variation,
		    dec_kn20aa_variations)) == NULL)
			platform.model = alpha_unknown_sysname();
	}

	platform.iobus = "cia";
	platform.cons_init = dec_kn20aa_cons_init;
	platform.pci_intr_init  = dec_kn20aa_intr_init;
	platform.pci_intr_map  = dec_kn20aa_intr_map;
	platform.pci_intr_disable = dec_kn20aa_intr_disable;
	platform.pci_intr_enable = dec_kn20aa_intr_enable;
}

static void
dec_kn20aa_cons_init()
{
	struct ctb *ctb;

	cia_init();

	ctb = (struct ctb *)(((caddr_t)hwrpb) + hwrpb->rpb_ctb_off);

	switch (ctb->ctb_term_type) {
	case 2:
		boothowto |= RB_SERIAL;
		break;

	case 3:
		boothowto &= ~RB_SERIAL;
		break;

	default:
		printf("ctb->ctb_term_type = 0x%lx\n", ctb->ctb_term_type);
		printf("ctb->ctb_turboslot = 0x%lx\n", ctb->ctb_turboslot);

		panic("consinit: unknown console type %d\n",
		    (int)ctb->ctb_term_type);
	}
}
#if 0
static void
dec_kn20aa_device_register(dev, aux)
	struct device *dev;
	void *aux;
{
	static int found, initted, scsiboot, netboot;
	static struct device *pcidev, *scsidev;
	struct bootdev_data *b = bootdev_data;
	struct device *parent = dev->dv_parent;
	struct cfdata *cf = dev->dv_cfdata;
	struct cfdriver *cd = cf->cf_driver;

	if (found)
		return;

	if (!initted) {
		scsiboot = (strcmp(b->protocol, "SCSI") == 0);
		netboot = (strcmp(b->protocol, "BOOTP") == 0);
#if 0
		printf("scsiboot = %d, netboot = %d\n", scsiboot, netboot);
#endif
		initted =1;
	}

	if (pcidev == NULL) {
		if (strcmp(cd->cd_name, "pci"))
			return;
		else {
			struct pcibus_attach_args *pba = aux;

			if ((b->slot / 1000) != pba->pba_bus)
				return;
	
			pcidev = dev;
#if 0
			printf("\npcidev = %s\n", pcidev->dv_xname);
#endif
			return;
		}
	}

	if (scsiboot && (scsidev == NULL)) {
		if (parent != pcidev)
			return;
		else {
			struct pci_attach_args *pa = aux;

			if ((b->slot % 1000) != pa->pa_device)
				return;

			/* XXX function? */
	
			scsidev = dev;
#if 0
			printf("\nscsidev = %s\n", scsidev->dv_xname);
#endif
			return;
		}
	}

	if (scsiboot &&
	    (!strcmp(cd->cd_name, "sd") ||
	     !strcmp(cd->cd_name, "st") ||
	     !strcmp(cd->cd_name, "cd"))) {
		struct scsipibus_attach_args *sa = aux;

		if (parent->dv_parent != scsidev)
			return;

		if (b->unit / 100 != sa->sa_sc_link->scsipi_scsi.target)
			return;

		/* XXX LUN! */

		switch (b->boot_dev_type) {
		case 0:
			if (strcmp(cd->cd_name, "sd") &&
			    strcmp(cd->cd_name, "cd"))
				return;
			break;
		case 1:
			if (strcmp(cd->cd_name, "st"))
				return;
			break;
		default:
			return;
		}

		/* we've found it! */
		booted_device = dev;
#if 0
		printf("\nbooted_device = %s\n", booted_device->dv_xname);
#endif
		found = 1;
	}

	if (netboot) {
		if (parent != pcidev)
			return;
		else {
			struct pci_attach_args *pa = aux;

			if ((b->slot % 1000) != pa->pa_device)
				return;

			/* XXX function? */
	
			booted_device = dev;
#if 0
			printf("\nbooted_device = %s\n", booted_device->dv_xname);
#endif
			found = 1;
			return;
		}
	}
}
#endif

#define KN20AA_MAX_IRQ  32
void
dec_kn20aa_intr_init()
{

	/*
	 * Enable ISA-PCI cascade interrupt.
	 */
	dec_kn20aa_intr_enable(31);
}

void
dec_kn20aa_intr_map(void *arg)
{
	pcicfgregs *cfg;

	cfg = (pcicfgregs *)arg;
	/*
	 * Slot->interrupt translation.  Appears to work, though it
	 * may not hold up forever.
	 *
	 * The DEC engineers who did this hardware obviously engaged
	 * in random drug testing.
	 */
	switch (cfg->slot) {
	case 11:
	case 12:
		cfg->intline = ((cfg->slot - 11) + 0) * 4;
		break;

	case 7:
		cfg->intline = 8;
		break;

	case 9:
		cfg->intline = 12;
		break;

	case 6:				/* 21040 on AlphaStation 500 */
		cfg->intline = 13;
		break;

	case 8:
		cfg->intline = 16;
		break;

	case 10:			/* 8275EB on AlphaStation 500 */
		return;

	default:
		if(!cfg->bus){
			printf("dec_kn20aa_intr_map: weird slot %d\n",
			    cfg->slot);
			return;
		} else {
			cfg->intline = cfg->slot;
		}
	}

	cfg->intline += cfg->bus*16;
	if (cfg->intline > KN20AA_MAX_IRQ)
		panic("dec_kn20aa_intr_map: cfg->intline too large (%d)\n",
		    cfg->intline);
}

void
dec_kn20aa_intr_enable(irq)
	int irq;
{

	/*
	 * From disassembling small bits of the OSF/1 kernel:
	 * the following appears to enable a given interrupt request.
	 * "blech."  I'd give valuable body parts for better docs or
	 * for a good decompiler.
	 */
	alpha_mb();
	REGVAL(0x8780000000L + 0x40L) |= (1 << irq);    /* XXX */
	alpha_mb();
}

void
dec_kn20aa_intr_disable(irq)
	int irq;
{

	alpha_mb();
	REGVAL(0x8780000000L + 0x40L) &= ~(1 << irq);   /* XXX */
	alpha_mb();
}
