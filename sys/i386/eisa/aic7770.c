/*
 * Product specific probe and attach routines for:
 * 	27/284X and aic7770 motherboard SCSI controllers
 *
 * Copyright (c) 1994, 1995, 1996 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id$
 */

#if defined(__FreeBSD__)
#include "eisa.h"
#endif
#if NEISA > 0 || defined(__NetBSD__)

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#if defined(__NetBSD__)
#include <sys/device.h>
#include <machine/bus.h>
#include <machine/intr.h>
#endif /* defined(__NetBSD__) */

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#if defined(__FreeBSD__)

#include <machine/clock.h>

#include <i386/eisa/eisaconf.h>
#include <i386/scsi/aic7xxx.h>
#include <dev/aic7xxx/aic7xxx_reg.h>

#define EISA_DEVICE_ID_ADAPTEC_AIC7770	0x04907770
#define EISA_DEVICE_ID_ADAPTEC_274x	0x04907771
#define EISA_DEVICE_ID_ADAPTEC_284xB	0x04907756 /* BIOS enabled */
#define EISA_DEVICE_ID_ADAPTEC_284x	0x04907757 /* BIOS disabled*/

#elif defined(__NetBSD__)

#include <dev/eisa/eisareg.h>
#include <dev/eisa/eisavar.h>
#include <dev/eisa/eisadevs.h>

#include <dev/ic/aic7xxxreg.h>
#include <dev/ic/aic7xxxvar.h>

#endif /* defined(__NetBSD__) */

#define AHC_EISA_SLOT_OFFSET	0xc00
#define AHC_EISA_IOSIZE		0x100
#define INTDEF			0x5cul	/* Interrupt Definition Register */

#if defined(__FreeBSD__)

static int	aic7770probe __P((void));
static int	aic7770_attach __P((struct eisa_device *e_dev));

static struct eisa_driver ahc_eisa_driver =
{
	"ahc",
	aic7770probe,
	aic7770_attach,
	/*shutdown*/NULL,
	&ahc_unit
};

DATA_SET (eisadriver_set, ahc_eisa_driver);

static char	*aic7770_match __P((eisa_id_t type));

static  char*
aic7770_match(type)
	eisa_id_t type;
{
	switch (type) {
	case EISA_DEVICE_ID_ADAPTEC_AIC7770:
		return ("Adaptec aic7770 SCSI host adapter");
		break;
	case EISA_DEVICE_ID_ADAPTEC_274x:
		return ("Adaptec 274X SCSI host adapter");
		break;
	case EISA_DEVICE_ID_ADAPTEC_284xB:
	case EISA_DEVICE_ID_ADAPTEC_284x:
		return ("Adaptec 284X SCSI host adapter");
		break;
	default:
		break;
	}
	return (NULL);
}

static int
aic7770probe(void)
{
	u_int32_t iobase;
	u_int32_t irq;
	u_int8_t intdef;
	u_int8_t hcntrl;
	struct eisa_device *e_dev = NULL;
	int count;

	count = 0;
	while ((e_dev = eisa_match_dev(e_dev, aic7770_match))) {
		iobase = (e_dev->ioconf.slot * EISA_SLOT_SIZE)
			 + AHC_EISA_SLOT_OFFSET;

		/* Pause the card preseving the IRQ type */
		hcntrl = inb(iobase + HCNTRL) & IRQMS;

		outb(iobase + HCNTRL, hcntrl | PAUSE);

		eisa_add_iospace(e_dev, iobase, AHC_EISA_IOSIZE, RESVADDR_NONE);
		intdef = inb(INTDEF + iobase);
		irq = intdef & 0xf;
		switch (irq) {
			case 9: 
			case 10:
			case 11:
			case 12:
			case 14:
			case 15:
				break;
			default:
				printf("aic7770 at slot %d: illegal "
				       "irq setting %d\n", e_dev->ioconf.slot,
					intdef);
				continue;
		}
		eisa_add_intr(e_dev, irq);
		eisa_registerdev(e_dev, &ahc_eisa_driver);
		count++;
	}
	return count;
}

#elif defined(__NetBSD__)

#define bootverbose	1

int	ahc_eisa_match __P((struct device *, void *, void *));
void	ahc_eisa_attach __P((struct device *, struct device *, void *));


struct cfattach ahc_eisa_ca =
{
	sizeof(struct ahc_softc), ahc_eisa_match, ahc_eisa_attach
};

/*
 * Return irq setting of the board, otherwise -1.
 */
int
ahc_eisa_irq(bc, ioh)
	bus_chipset_tag_t bc;
	bus_io_handle_t ioh;
{
	int irq;
	u_int8_t intdef;

	ahc_reset("ahc_eisa", bc, ioh);
	intdef = bus_io_read_1(bc, ioh, INTDEF);
	switch (irq = (intdef & 0xf)) {
	case 9:
	case 10:
	case 11:
	case 12:
	case 14:
	case 15:
		break;
	default:
		printf("ahc_eisa_irq: illegal irq setting %d\n", intdef);
		return -1;
	}

	/* Note that we are going and return (to probe) */
	return irq;
}

/*
 * Check the slots looking for a board we recognise
 * If we find one, note it's address (slot) and call
 * the actual probe routine to check it out.
 */
int
ahc_eisa_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct eisa_attach_args *ea = aux;
	bus_chipset_tag_t bc = ea->ea_bc;
	bus_io_handle_t ioh;
	int irq;

	/* must match one of our known ID strings */
	if (strcmp(ea->ea_idstring, "ADP7770") &&
	    strcmp(ea->ea_idstring, "ADP7771") &&
	    strcmp(ea->ea_idstring, "ADP7756") && /* XXX - not EISA, but VL */
	    strcmp(ea->ea_idstring, "ADP7757"))	  /* XXX - not EISA, but VL */
		return (0);

	if (bus_io_map(bc, EISA_SLOT_ADDR(ea->ea_slot) + AHC_EISA_SLOT_OFFSET, 
	    AHC_EISA_IOSIZE, &ioh))
		return (0);

	irq = ahc_eisa_irq(bc, ioh);

	bus_io_unmap(bc, ioh, AHC_EISA_IOSIZE);

	return (irq >= 0);
}

#endif /* defined(__NetBSD__) */

#if defined(__FreeBSD__)
static int
aic7770_attach(e_dev)
	struct eisa_device *e_dev;
#elif defined(__NetBSD__)
void
ahc_eisa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
#endif
{
	ahc_type type;

#if defined(__FreeBSD__)
	struct ahc_softc *ahc;
	resvaddr_t *iospace;
	int unit = e_dev->unit;
	int irq = ffs(e_dev->ioconf.irq) - 1;

	iospace = e_dev->ioconf.ioaddrs.lh_first;

	if (!iospace)
		return -1;

	switch (e_dev->id) {
	case EISA_DEVICE_ID_ADAPTEC_AIC7770:
		type = AHC_AIC7770;
		break;
	case EISA_DEVICE_ID_ADAPTEC_274x:
		type = AHC_274;
		break;          
	case EISA_DEVICE_ID_ADAPTEC_284xB:
	case EISA_DEVICE_ID_ADAPTEC_284x:
		type = AHC_284;
		break;
	default: 
		printf("aic7770_attach: Unknown device type!\n");
		return -1;
		break;
	}

	if (!(ahc = ahc_alloc(unit, iospace->addr, NULL,
			      type, AHC_FNONE, NULL)))
		return -1;

	eisa_reg_start(e_dev);
	if (eisa_reg_iospace(e_dev, iospace)) {
		ahc_free(ahc);
		return -1;
	}

	ahc_reset(ahc);

	/*
	 * The IRQMS bit enables level sensitive interrupts. Only allow
	 * IRQ sharing if it's set.
	 */
	if (eisa_reg_intr(e_dev, irq, ahc_intr, (void *)ahc, &bio_imask,
			 /*shared ==*/ahc->pause & IRQMS)) {
		ahc_free(ahc);
		return -1;
	}
	eisa_reg_end(e_dev);

#elif defined(__NetBSD__)

	struct ahc_softc *ahc = (void *)self;
	struct eisa_attach_args *ea = aux;
	bus_chipset_tag_t bc = ea->ea_bc;
	bus_io_handle_t ioh;
	int irq;
	eisa_chipset_tag_t ec = ea->ea_ec;
	eisa_intr_handle_t ih;
	const char *model, *intrstr;

	if (bus_io_map(bc, EISA_SLOT_ADDR(ea->ea_slot) + AHC_EISA_SLOT_OFFSET, 
		       AHC_EISA_IOSIZE, &ioh))
		panic("ahc_eisa_attach: could not map I/O addresses");
	if ((irq = ahc_eisa_irq(bc, ioh)) < 0)
		panic("ahc_eisa_attach: ahc_eisa_irq failed!");

	if (strcmp(ea->ea_idstring, "ADP7770") == 0) {
		model = EISA_PRODUCT_ADP7770;
		type = AHC_AIC7770;
	} else if (strcmp(ea->ea_idstring, "ADP7771") == 0) {
		model = EISA_PRODUCT_ADP7771;
		type = AHC_274;
	} else if (strcmp(ea->ea_idstring, "ADP7756") == 0) {
		model = EISA_PRODUCT_ADP7756;
		type = AHC_284;
	} else if (strcmp(ea->ea_idstring, "ADP7757") == 0) {
		model = EISA_PRODUCT_ADP7757;
		type = AHC_284;
	} else {
		panic("ahc_eisa_attach: Unknown device type %s\n",
		      ea->ea_idstring);
	}
	printf(": %s\n", model);

	ahc_construct(ahc, bc, ioh, type, AHC_FNONE);
	if (eisa_intr_map(ec, irq, &ih)) {
		printf("%s: couldn't map interrupt (%d)\n",
		       ahc->sc_dev.dv_xname, irq);
		return;
	}
#endif /* defined(__NetBSD__) */

	/*
	 * Tell the user what type of interrupts we're using.
	 * usefull for debugging irq problems
	 */
	if (bootverbose) {
		printf("%s: Using %s Interrupts\n",
		       ahc_name(ahc),
		       ahc->pause & IRQMS ?
				"Level Sensitive" : "Edge Triggered");
	}

	/*
	 * Now that we know we own the resources we need, do the 
	 * card initialization.
	 *
	 * First, the aic7770 card specific setup.
	 */
	switch (ahc->type) {
	case AHC_AIC7770:
	case AHC_274:
	{
		u_int8_t biosctrl = ahc_inb(ahc, HA_274_BIOSCTRL);

		/* Get the primary channel information */
		ahc->flags |= (biosctrl & CHANNEL_B_PRIMARY);

		if((biosctrl & BIOSMODE) == BIOSDISABLED)
			ahc->flags |= AHC_USEDEFAULTS;
		break;
	}
	case AHC_284:
	{
		/* XXX
		 * All values are automagically intialized at
		 * POST for these cards, so we can always rely
		 * on the Scratch Ram values.  However, we should
		 * read the SEEPROM here (Dan has the code to do
		 * it) so we can say what kind of translation the
		 * BIOS is using.  Printing out the geometry could
		 * save a lot of users the grief of failed installs.
		 */
		break;
	}
	default:
		break;
	}

	/*
	 * See if we have a Rev E or higher aic7770. Anything below a
	 * Rev E will have a R/O autoflush disable configuration bit.
	 * The Rev E. cards have some changes to support Adaptec's SCB
	 * paging scheme, but I don't know what that is yet.
	 */
	{
		char *id_string;
		u_int8_t sblkctl;
		u_int8_t sblkctl_orig;

		sblkctl_orig = ahc_inb(ahc, SBLKCTL);
		sblkctl = sblkctl_orig ^ AUTOFLUSHDIS;
		ahc_outb(ahc, SBLKCTL, sblkctl);
		sblkctl = ahc_inb(ahc, SBLKCTL);
		if (sblkctl != sblkctl_orig) {
			id_string = "aic7770 >= Rev E, ";
			/*
			 * Ensure autoflush is enabled
			 */
			sblkctl &= ~AUTOFLUSHDIS;
			ahc_outb(ahc, SBLKCTL, sblkctl);

		} else
			id_string = "aic7770 <= Rev C, ";

		printf("%s: %s", ahc_name(ahc), id_string);
	}

	/* Setup the FIFO threshold and the bus off time */
	{
		u_int8_t hostconf = ahc_inb(ahc, HOSTCONF);
		ahc_outb(ahc, BUSSPD, hostconf & DFTHRSH);
		ahc_outb(ahc, BUSTIME, (hostconf << 2) & BOFF);
	}

	/*
	 * Generic aic7xxx initialization.
	 */
	if (ahc_init(ahc)) {
#if defined(__FreeBSD__)
		ahc_free(ahc);
		/*
		 * The board's IRQ line is not yet enabled so it's safe
		 * to release the irq.
		 */
		eisa_release_intr(e_dev, irq, ahc_intr);
		return -1;
#elif defined(__NetBSD__)
		ahc_free(ahc);
		return;
#endif
	}

	/*
	 * Enable the board's BUS drivers
	 */
	ahc_outb(ahc, BCTL, ENABLE);

#if defined(__FreeBSD__)
	/*
	 * Enable our interrupt handler.
	 */
	if (eisa_enable_intr(e_dev, irq)) {
		ahc_free(ahc);
		eisa_release_intr(e_dev, irq, ahc_intr);
		return -1;
	}

#elif defined(__NetBSD__)
	intrstr = eisa_intr_string(ec, ih);
	/*
	 * The IRQMS bit enables level sensitive interrupts only allow
	 * IRQ sharing if its set.
	 */
	ahc->sc_ih = eisa_intr_establish(ec, ih,
	    ahc->pause & IRQMS ? IST_LEVEL : IST_EDGE, IPL_BIO, ahc_intr, ahc
#if defined(__OpenBSD__)
	    , ahc->sc_dev.dv_xname
#endif
	    );
	if (ahc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt",
		       ahc->sc_dev.dv_xname);
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		ahc_free(ahc);
		return;
	}
	if (intrstr != NULL)
		printf("%s: interrupting at %s\n", ahc->sc_dev.dv_xname,
		       intrstr);
#endif /* defined(__NetBSD__) */

	/* Attach sub-devices - always succeeds */
	ahc_attach(ahc);

#if defined(__FreeBSD__)
	return 0;
#endif
}

#endif /* NEISA > 0 */
