/*
 * Product specific probe and attach routines for:
 * 	27/284X and aic7770 motherboard SCSI controllers
 *
 * Copyright (c) 1994, 1995, 1996, 1997, 1998 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
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
 * $FreeBSD$
 */

#include "eisa.h"
#if NEISA > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <machine/bus_pio.h>
#include <machine/bus.h>

#include <i386/eisa/eisaconf.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/scsi/scsi_all.h>

#include <dev/aic7xxx/aic7xxx.h>
#include <dev/aic7xxx/93cx6.h>

#include <aic7xxx_reg.h>

#define EISA_DEVICE_ID_ADAPTEC_AIC7770	0x04907770
#define EISA_DEVICE_ID_ADAPTEC_274x	0x04907771
#define EISA_DEVICE_ID_ADAPTEC_284xB	0x04907756 /* BIOS enabled */
#define EISA_DEVICE_ID_ADAPTEC_284x	0x04907757 /* BIOS disabled*/

#define AHC_EISA_SLOT_OFFSET	0xc00
#define AHC_EISA_IOSIZE		0x100
#define INTDEF			0x5cul	/* Interrupt Definition Register */

static int	aic7770_probe(void);
static int	aic7770_attach(struct eisa_device *e_dev); 
static void	aha2840_load_seeprom(struct ahc_softc *ahc);

static struct eisa_driver ahc_eisa_driver =
{
	"ahc",
	aic7770_probe,
	aic7770_attach,
	/*shutdown*/NULL,
	&ahc_unit
};

DATA_SET (eisadriver_set, ahc_eisa_driver);

static const char *aic7770_match(eisa_id_t type);

static const char*
aic7770_match(eisa_id_t type)
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
aic7770_probe()
{
	u_int32_t iobase;
	u_int32_t irq;
	u_int8_t intdef;
	u_int8_t hcntrl;
	struct eisa_device *e_dev;
	int count; 

	e_dev = NULL;
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
				irq = 0;
				break;
		}
		if (irq == 0)
			continue;
		eisa_add_intr(e_dev, irq);
		eisa_registerdev(e_dev, &ahc_eisa_driver);
		count++;
	}
	return count;
}

static int
aic7770_attach(struct eisa_device *e_dev)
{
	ahc_chip chip;
	bus_dma_tag_t parent_dmat;
	struct ahc_softc *ahc;
	resvaddr_t *iospace;
	int unit = e_dev->unit;
	int irq;
	int error;

	if (TAILQ_FIRST(&e_dev->ioconf.irqs) == NULL)
		return (-1);

	irq = TAILQ_FIRST(&e_dev->ioconf.irqs)->irq_no;

	iospace = e_dev->ioconf.ioaddrs.lh_first;

	if (!iospace)
		return -1;

	switch (e_dev->id) {
	case EISA_DEVICE_ID_ADAPTEC_274x:
	case EISA_DEVICE_ID_ADAPTEC_AIC7770:
		chip = AHC_AIC7770|AHC_EISA;
		break;
	case EISA_DEVICE_ID_ADAPTEC_284xB:
	case EISA_DEVICE_ID_ADAPTEC_284x:
		chip = AHC_AIC7770|AHC_VL;
		break;
	default: 
		printf("aic7770_attach: Unknown device type!\n");
		return -1;
	}

	/* XXX Should be a child of the EISA bus dma tag */
	error = bus_dma_tag_create(/*parent*/NULL, /*alignment*/1,
				   /*boundary*/0,
				   /*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
				   /*highaddr*/BUS_SPACE_MAXADDR,
				   /*filter*/NULL, /*filterarg*/NULL,
				   /*maxsize*/MAXBSIZE,
				   /*nsegments*/AHC_NSEG,
				   /*maxsegsz*/AHC_MAXTRANSFER_SIZE,
				   /*flags*/BUS_DMA_ALLOCNOW, &parent_dmat);

	if (error != 0) {
		printf("ahc_eisa_attach: Could not allocate DMA tag "
		       "- error %d\n", error);
		return -1;
	}

	if (!(ahc = ahc_alloc(unit, iospace->addr, NULL, parent_dmat,
			      chip, AHC_AIC7770_FE, AHC_FNONE, NULL)))
		return -1;

	ahc->channel = 'A';
	ahc->channel_b = 'B';

	eisa_reg_start(e_dev);
	if (eisa_reg_iospace(e_dev, iospace)) {
		ahc_free(ahc);
		return -1;
	}

	if (ahc_reset(ahc) != 0) {
		ahc_free(ahc);
		return -1;
	}

	/*
	 * The IRQMS bit enables level sensitive interrupts. Only allow
	 * IRQ sharing if it's set.
	 */
	if (eisa_reg_intr(e_dev, irq, ahc_intr, (void *)ahc, &cam_imask,
			  /*shared ==*/ahc->pause & IRQMS)) {
		ahc_free(ahc);
		return -1;
	}
	eisa_reg_end(e_dev);

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
	switch (chip & (AHC_EISA|AHC_VL)) {
	case AHC_EISA:
	{
		u_int biosctrl;
		u_int scsiconf;
		u_int scsiconf1;
#if DEBUG
		int i;
#endif

		biosctrl = ahc_inb(ahc, HA_274_BIOSCTRL);
		scsiconf = ahc_inb(ahc, SCSICONF);
		scsiconf1 = ahc_inb(ahc, SCSICONF + 1);

#if DEBUG
		for (i = TARG_SCSIRATE; i <= HA_274_BIOSCTRL; i+=8) {
			printf("0x%x, 0x%x, 0x%x, 0x%x, "
			       "0x%x, 0x%x, 0x%x, 0x%x\n",
				ahc_inb(ahc, i),
				ahc_inb(ahc, i+1),
				ahc_inb(ahc, i+2),
				ahc_inb(ahc, i+3),
				ahc_inb(ahc, i+4),
				ahc_inb(ahc, i+5),
				ahc_inb(ahc, i+6),
				ahc_inb(ahc, i+7));
		}
#endif

		/* Get the primary channel information */
		if ((biosctrl & CHANNEL_B_PRIMARY) != 0)
			ahc->flags |= AHC_CHANNEL_B_PRIMARY;

		if ((biosctrl & BIOSMODE) == BIOSDISABLED) {
			ahc->flags |= AHC_USEDEFAULTS;
		} else {
			if ((ahc->features & AHC_WIDE) != 0) {
				ahc->our_id = scsiconf1 & HWSCSIID;
				if (scsiconf & TERM_ENB)
					ahc->flags |= AHC_TERM_ENB_A;
			} else {
				ahc->our_id = scsiconf & HSCSIID;
				ahc->our_id_b = scsiconf1 & HSCSIID;
				if (scsiconf & TERM_ENB)
					ahc->flags |= AHC_TERM_ENB_A;
				if (scsiconf1 & TERM_ENB)
					ahc->flags |= AHC_TERM_ENB_B;
			}
		}
		/*
		 * We have no way to tell, so assume extended
		 * translation is enabled.
		 */
		ahc->flags |= AHC_EXTENDED_TRANS_A|AHC_EXTENDED_TRANS_B;
		break;
	}
	case AHC_VL:
	{
		aha2840_load_seeprom(ahc);
		break;
	}
	default:
		break;
	}

	/*
	 * See if we have a Rev E or higher aic7770. Anything below a
	 * Rev E will have a R/O autoflush disable configuration bit.
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
	ahc_outb(ahc, BCTL, ENABLE);

	/*
	 * Enable our interrupt handler.
	 */
	if (eisa_enable_intr(e_dev, irq)) {
		ahc_free(ahc);
		eisa_release_intr(e_dev, irq, ahc_intr);
		return -1;
	}

	/* Attach sub-devices - always succeeds */
	ahc_attach(ahc);

	return 0;
}

/*
 * Read the 284x SEEPROM.
 */
static void
aha2840_load_seeprom(struct ahc_softc *ahc)
{
	struct	  seeprom_descriptor sd;
	struct	  seeprom_config sc;
	u_int16_t checksum = 0;
	u_int8_t  scsi_conf;
	int	  have_seeprom;

	sd.sd_tag = ahc->tag;
	sd.sd_bsh = ahc->bsh;
	sd.sd_control_offset = SEECTL_2840;
	sd.sd_status_offset = STATUS_2840;
	sd.sd_dataout_offset = STATUS_2840;		
	sd.sd_chip = C46;
	sd.sd_MS = 0;
	sd.sd_RDY = EEPROM_TF;
	sd.sd_CS = CS_2840;
	sd.sd_CK = CK_2840;
	sd.sd_DO = DO_2840;
	sd.sd_DI = DI_2840;

	if (bootverbose)
		printf("%s: Reading SEEPROM...", ahc_name(ahc));
	have_seeprom = read_seeprom(&sd,
				    (u_int16_t *)&sc,
				    /*start_addr*/0,
				    sizeof(sc)/2);

	if (have_seeprom) {
		/* Check checksum */
		int i;
		int maxaddr = (sizeof(sc)/2) - 1;
		u_int16_t *scarray = (u_int16_t *)&sc;

		for (i = 0; i < maxaddr; i++)
			checksum = checksum + scarray[i];
		if (checksum != sc.checksum) {
			if(bootverbose)
				printf ("checksum error\n");
			have_seeprom = 0;
		} else if (bootverbose) {
			printf("done.\n");
		}
	}

	if (!have_seeprom) {
		if (bootverbose)
			printf("%s: No SEEPROM available\n", ahc_name(ahc));
		ahc->flags |= AHC_USEDEFAULTS;
	} else {
		/*
		 * Put the data we've collected down into SRAM
		 * where ahc_init will find it.
		 */
		int i;
		int max_targ = (ahc->features & AHC_WIDE) != 0 ? 16 : 8;
		u_int16_t discenable;

		discenable = 0;
		for (i = 0; i < max_targ; i++){
	                u_int8_t target_settings;
			target_settings = (sc.device_flags[i] & CFXFER) << 4;
			if (sc.device_flags[i] & CFSYNCH)
				target_settings |= SOFS;
			if (sc.device_flags[i] & CFWIDEB)
				target_settings |= WIDEXFER;
			if (sc.device_flags[i] & CFDISC)
				discenable |= (0x01 << i);
			ahc_outb(ahc, TARG_SCSIRATE + i, target_settings);
		}
		ahc_outb(ahc, DISC_DSB, ~(discenable & 0xff));
		ahc_outb(ahc, DISC_DSB + 1, ~((discenable >> 8) & 0xff));

		ahc->our_id = sc.brtime_id & CFSCSIID;

		scsi_conf = (ahc->our_id & 0x7);
		if (sc.adapter_control & CFSPARITY)
			scsi_conf |= ENSPCHK;
		if (sc.adapter_control & CFRESETB)
			scsi_conf |= RESET_SCSI;

		if (sc.bios_control & CF284XEXTEND)		
			ahc->flags |= AHC_EXTENDED_TRANS_A;
		/* Set SCSICONF info */
		ahc_outb(ahc, SCSICONF, scsi_conf);

		if (sc.adapter_control & CF284XSTERM)
			ahc->flags |= AHC_TERM_ENB_A;
	}
}
#endif /* NEISA > 0 */
