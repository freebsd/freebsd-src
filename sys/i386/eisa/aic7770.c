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
 *	$Id: aic7770.c,v 1.27 1996/04/20 21:21:47 gibbs Exp $
 */

#include "eisa.h"
#if NEISA > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/devconf.h>
#include <sys/kernel.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <machine/clock.h>

#include <i386/eisa/eisaconf.h>
#include <i386/scsi/aic7xxx.h>
#include <dev/aic7xxx/aic7xxx_reg.h>

#define EISA_DEVICE_ID_ADAPTEC_AIC7770	0x04907770
#define EISA_DEVICE_ID_ADAPTEC_274x	0x04907771
#define EISA_DEVICE_ID_ADAPTEC_284xB	0x04907756 /* BIOS enabled */
#define EISA_DEVICE_ID_ADAPTEC_284x	0x04907757 /* BIOS disabled*/

#define AHC_EISA_SLOT_OFFSET	0xc00
#define AHC_EISA_IOSIZE		0x100
#define INTDEF			0x5cul	/* Interrupt Definition Register */

static int	aic7770probe __P((void));
static int	aic7770_attach __P((struct eisa_device *e_dev));

static struct eisa_driver ahc_eisa_driver = {
					"ahc",
					aic7770probe,
					aic7770_attach,
					/*shutdown*/NULL,
					&ahc_unit
				      };

DATA_SET (eisadriver_set, ahc_eisa_driver);

static struct kern_devconf kdc_aic7770 = {
	0, 0, 0,                /* filled in by dev_attach */
	"ahc", 0, { MDDT_EISA, 0, "bio" },
	eisa_generic_externalize, 0, 0, EISA_EXTERNALLEN,
	&kdc_eisa0,		/* parent */
	0,			/* parentdata */
	DC_UNCONFIGURED,	/* always start out here */
	NULL,
	DC_CLS_MISC		/* host adapters aren't special */
};


static char	*aic7770_match __P((eisa_id_t type));

static  char*
aic7770_match(type)
	eisa_id_t type;
{
	switch(type) {
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
	u_long iobase;
	char intdef;
	u_long irq;
	struct eisa_device *e_dev = NULL;
	int count;

	count = 0;
	while ((e_dev = eisa_match_dev(e_dev, aic7770_match))) {
		iobase = (e_dev->ioconf.slot * EISA_SLOT_SIZE)
			 + AHC_EISA_SLOT_OFFSET;
		ahc_reset(iobase);

		eisa_add_iospace(e_dev, iobase, AHC_EISA_IOSIZE, RESVADDR_NONE);
		intdef = inb(INTDEF + iobase);
		switch (intdef & 0xf) {
			case 9: 
				irq = 9;
				break;
			case 10:
				irq = 10;
				break;
			case 11:
				irq = 11;
				break;  
			case 12:
				irq = 12;
				break;
			case 14:
				irq = 14;
				break;
			case 15:
				irq = 15;
				break;
			default:
				printf("aic7770 at slot %d: illegal "
				       "irq setting %d\n", e_dev->ioconf.slot,
					intdef);
				continue;
		}
		eisa_add_intr(e_dev, irq);
		eisa_registerdev(e_dev, &ahc_eisa_driver, &kdc_aic7770);
		if(e_dev->id == EISA_DEVICE_ID_ADAPTEC_284xB
		   || e_dev->id == EISA_DEVICE_ID_ADAPTEC_284x) {
			/* Our real parent is the isa bus.  Say so. */
			e_dev->kdc->kdc_parent = &kdc_isa0;
		}
		count++;
	}
	return count;
}

static int
aic7770_attach(e_dev)
	struct eisa_device *e_dev;
{
	ahc_type type;
	struct ahc_data *ahc;
	resvaddr_t *iospace;
	u_long	iobase;
	int unit = e_dev->unit;
	int irq = ffs(e_dev->ioconf.irq) - 1;

	iospace = e_dev->ioconf.ioaddrs.lh_first;

	if(!iospace)
		return -1;

	iobase = iospace->addr;

	switch(e_dev->id) {
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

	if(!(ahc = ahc_alloc(unit, iospace->addr, type, AHC_FNONE)))
		return -1;

	eisa_reg_start(e_dev);
	if(eisa_reg_iospace(e_dev, iospace)) {
		ahc_free(ahc);
		return -1;
	}

	/*
	 * The IRQMS bit enables level sensitive interrupts. Only allow
	 * IRQ sharing if it's set.
	 */
	if(eisa_reg_intr(e_dev, irq, ahc_intr, (void *)ahc, &bio_imask,
			 /*shared ==*/ahc->pause & IRQMS)) {
		ahc_free(ahc);
		return -1;
	}
	eisa_reg_end(e_dev);

	/*
	 * Tell the user what type of interrupts we're using.
	 * usefull for debugging irq problems
	 */
	if(bootverbose) {
		if(ahc->pause & IRQMS)
			printf("ahc%d: Using Level Sensitive "
			       "Interrupts\n", unit);
		else
			printf("ahc%d: Using Edge Triggered "
			       "Interrupts\n", unit); 
	}

	/*
	 * Now that we know we own the resources we need, do the 
	 * card initialization.
	 *
	 * First, the aic7770 card specific setup.
	 */
	switch( ahc->type ) {
	    case AHC_AIC7770:
	    case AHC_274:
	    {
		u_char biosctrl = inb(HA_274_BIOSCTRL + iobase);

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
	 * It's still not clear exactly what is differenent about the Rev E.
	 * We think it allows 8 bit entries in the QOUTFIFO to support
	 * "paging" SCBs so you can have more than 4 commands active at
	 * once.
	 */     
	{
		char *id_string;
		u_char sblkctl;
		u_char sblkctl_orig;

		sblkctl_orig = inb(SBLKCTL + iobase);
		sblkctl = sblkctl_orig ^ AUTOFLUSHDIS;
		outb(SBLKCTL + iobase, sblkctl);
		sblkctl = inb(SBLKCTL + iobase);
		if(sblkctl != sblkctl_orig)
		{
			id_string = "aic7770 >= Rev E, ";
			/*
			 * Ensure autoflush is enabled
			 */
			sblkctl &= ~AUTOFLUSHDIS;
			outb(SBLKCTL + iobase, sblkctl);

			/* Allow paging on this adapter */
			ahc->flags |= AHC_PAGESCBS;
		}
		else
			id_string = "aic7770 <= Rev C, ";

		printf("ahc%d: %s", unit, id_string);
	}

	/* Setup the FIFO threshold and the bus off time */
	{
		u_char hostconf = inb(HOSTCONF + iobase);
		outb(BUSSPD + iobase, hostconf & DFTHRSH);
		outb(BUSTIME + iobase, (hostconf << 2) & BOFF);
	}

	/*
	 * Generic aic7xxx initialization.
	 */
	if(ahc_init(ahc)){
		ahc_free(ahc);
		/*
		 * The board's IRQ line is not yet enabled so it's safe
		 * to release the irq.
		 */
		eisa_release_intr(e_dev, irq, ahc_intr);
		return -1;
	}

	/*
	 * Enable the board's BUS drivers
	 */
	outb(BCTL + iobase, ENABLE);

	/*
	 * Enable our interrupt handler.
	 */
	if(eisa_enable_intr(e_dev, irq)) {
		ahc_free(ahc);
		eisa_release_intr(e_dev, irq, ahc_intr);
		return -1;
	}

	e_dev->kdc->kdc_state = DC_BUSY; /* host adapters always busy */

	/* Attach sub-devices - always succeeds */
	ahc_attach(ahc);

	return 0;
}

#endif /* NEISA > 0 */
