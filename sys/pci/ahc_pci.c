/*
 * Product specific probe and attach routines for:
 *      3940, 2940, aic7895, aic7890, aic7880,
 *	aic7870, aic7860 and aic7850 SCSI controllers
 *
 * Copyright (c) 1995, 1996, 1997, 1998 Justin T. Gibbs
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Where this Software is combined with software released under the terms of 
 * the GNU Public License ("GPL") and the terms of the GPL would require the 
 * combined work to also be released under the terms of the GPL, the terms
 * and conditions of this License will apply in addition to those of the
 * GPL with the exception of any terms or conditions of this License that
 * conflict with, or are expressly prohibited by, the GPL.
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
 *	$Id: ahc_pci.c,v 1.7 1999/03/05 23:28:36 gibbs Exp $
 */

#include <pci.h>
#if NPCI > 0
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/queue.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <machine/bus_memio.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/clock.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>

#include <cam/scsi/scsi_all.h>

#include <dev/aic7xxx/aic7xxx.h>
#include <dev/aic7xxx/93cx6.h>

#include <aic7xxx_reg.h>

#define PCI_BASEADR0	PCI_MAP_REG_START	/* I/O Address */
#define PCI_BASEADR1	PCI_MAP_REG_START + 4	/* Mem I/O Address */

#define PCI_DEVICE_ID_ADAPTEC_398XU	0x83789004ul
#define PCI_DEVICE_ID_ADAPTEC_3940U	0x82789004ul
#define PCI_DEVICE_ID_ADAPTEC_3950U2	0x00509005ul
#define PCI_DEVICE_ID_ADAPTEC_2944U	0x84789004ul
#define PCI_DEVICE_ID_ADAPTEC_2940U	0x81789004ul
#define PCI_DEVICE_ID_ADAPTEC_2940AU	0x61789004ul
#define PCI_DEVICE_ID_ADAPTEC_2940U2	0x00109005ul
#define PCI_DEVICE_ID_ADAPTEC_2930U2	0x00119005ul
#define PCI_DEVICE_ID_ADAPTEC_398X	0x73789004ul
#define PCI_DEVICE_ID_ADAPTEC_3940	0x72789004ul
#define PCI_DEVICE_ID_ADAPTEC_2944	0x74789004ul
#define PCI_DEVICE_ID_ADAPTEC_2940	0x71789004ul
#define PCI_DEVICE_ID_ADAPTEC_AIC7890	0x001F9005ul
#define PCI_DEVICE_ID_ADAPTEC_AIC7895C	0x78939004ul /* RAID Port */
#define PCI_DEVICE_ID_ADAPTEC_AIC7895	0x78959004ul
#define PCI_DEVICE_ID_ADAPTEC_AIC7896	0x005F9005ul
#define PCI_DEVICE_ID_ADAPTEC_AIC7880	0x80789004ul
#define PCI_DEVICE_ID_ADAPTEC_AIC7870	0x70789004ul
#define PCI_DEVICE_ID_ADAPTEC_AIC7860	0x60789004ul
#define PCI_DEVICE_ID_ADAPTEC_AIC7855	0x55789004ul
#define PCI_DEVICE_ID_ADAPTEC_AIC7850	0x50789004ul
#define PCI_DEVICE_ID_ADAPTEC_AIC7810	0x10789004ul
#define PCI_DEVICE_ID_ADAPTEC_AIC7815	0x15789004ul

#define AHC_394X_SLOT_CHANNEL_A	4
#define AHC_394X_SLOT_CHANNEL_B	5

#define AHC_398X_SLOT_CHANNEL_A	4
#define AHC_398X_SLOT_CHANNEL_B	8
#define AHC_398X_SLOT_CHANNEL_C	12

#define	DEVCONFIG		0x40
#define		SCBSIZE32	0x00010000ul	/* aic789X only */
#define		MPORTMODE	0x00000400ul	/* aic7870 only */
#define		RAMPSM		0x00000200ul	/* aic7870 only */
#define		VOLSENSE	0x00000100ul
#define		SCBRAMSEL	0x00000080ul
#define		MRDCEN		0x00000040ul
#define		EXTSCBTIME	0x00000020ul	/* aic7870 only */
#define		EXTSCBPEN	0x00000010ul	/* aic7870 only */
#define		BERREN		0x00000008ul
#define		DACEN		0x00000004ul
#define		STPWLEVEL	0x00000002ul
#define		DIFACTNEGEN	0x00000001ul	/* aic7870 only */

#define	CSIZE_LATTIME		0x0c
#define		CACHESIZE	0x0000003ful	/* only 5 bits */
#define		LATTIME		0x0000ff00ul

static void check_extport(struct ahc_softc *ahc, u_int *sxfrctl1);
static void configure_termination(struct ahc_softc *ahc,
				  struct seeprom_config *sc,
				  struct seeprom_descriptor *sd,
	 			  u_int *sxfrctl1);

static void ahc_ultra2_term_detect(struct ahc_softc *ahc,
				   int *enableSEC_low,
				   int *enableSEC_high,
				   int *enablePRI_low,
				   int *enablePRI_high,
				   int *eeprom_present);
static void aic787X_cable_detect(struct ahc_softc *ahc, int *internal50_present,
				 int *internal68_present,
				 int *externalcable_present,
				 int *eeprom_present);
static void aic785X_cable_detect(struct ahc_softc *ahc, int *internal50_present,
				 int *externalcable_present,
				 int *eeprom_present);
static int acquire_seeprom(struct ahc_softc *ahc,
			   struct seeprom_descriptor *sd);
static void release_seeprom(struct seeprom_descriptor *sd);
static void write_brdctl(struct ahc_softc *ahc, u_int8_t value);
static u_int8_t read_brdctl(struct ahc_softc *ahc);

static struct ahc_softc *first_398X;

static const char* ahc_pci_probe(pcici_t tag, pcidi_t type);
static void ahc_pci_attach(pcici_t config_id, int unit);

/* Exported for use in the ahc_intr routine */
void ahc_pci_intr(struct ahc_softc *ahc);

static struct  pci_device ahc_pci_driver = {
	"ahc",
        ahc_pci_probe,
        ahc_pci_attach,
        &ahc_unit,
	NULL
};

DATA_SET (pcidevice_set, ahc_pci_driver);

static const char*
ahc_pci_probe (pcici_t tag, pcidi_t type)
{
	switch (type) {
	case PCI_DEVICE_ID_ADAPTEC_398XU:
		return ("Adaptec 398X Ultra SCSI RAID adapter");
		break;
	case PCI_DEVICE_ID_ADAPTEC_3940U:
		return ("Adaptec 3940 Ultra SCSI host adapter");
		break;
	case PCI_DEVICE_ID_ADAPTEC_398X:
		return ("Adaptec 398X SCSI RAID adapter");
		break;
	case PCI_DEVICE_ID_ADAPTEC_3940:
		return ("Adaptec 3940 SCSI adapter");
		break;
	case PCI_DEVICE_ID_ADAPTEC_3950U2:
		return ("Adaptec 3950 Ultra2 SCSI adapter");
		break;
	case PCI_DEVICE_ID_ADAPTEC_2930U2:
		return ("Adaptec 2930 Ultra2 SCSI adapter");
		break;
	case PCI_DEVICE_ID_ADAPTEC_2944U:
		return ("Adaptec 2944 Ultra SCSI adapter");
		break;
	case PCI_DEVICE_ID_ADAPTEC_2940U:
		return ("Adaptec 2940 Ultra SCSI adapter");
		break;
	case PCI_DEVICE_ID_ADAPTEC_2940U2:
		return ("Adaptec 2940 Ultra2 SCSI adapter");
		break;
	case PCI_DEVICE_ID_ADAPTEC_2944:
		return ("Adaptec 2944 SCSI adapter");
		break;
	case PCI_DEVICE_ID_ADAPTEC_2940:
		return ("Adaptec 2940 SCSI adapter");
		break;
	case PCI_DEVICE_ID_ADAPTEC_2940AU:
		return ("Adaptec 2940A Ultra SCSI adapter");
		break;
	case PCI_DEVICE_ID_ADAPTEC_AIC7895:
		return ("Adaptec aic7895 Ultra SCSI adapter");
		break;
	case PCI_DEVICE_ID_ADAPTEC_AIC7895C:
		return ("Adaptec aic7895 `Raid Port' Ultra SCSI adapter");
		break;
	case PCI_DEVICE_ID_ADAPTEC_AIC7890:
		return ("Adaptec aic7890/91 Ultra2 SCSI adapter");
		break;
	case PCI_DEVICE_ID_ADAPTEC_AIC7896:
		return ("Adaptec aic7896/97 Ultra2 SCSI adapter");
		break;
	case PCI_DEVICE_ID_ADAPTEC_AIC7880:
		return ("Adaptec aic7880 Ultra SCSI adapter");
		break;
	case PCI_DEVICE_ID_ADAPTEC_AIC7870:
		return ("Adaptec aic7870 SCSI adapter");
		break;
	case PCI_DEVICE_ID_ADAPTEC_AIC7860:
		return ("Adaptec aic7860 SCSI adapter");
		break;
	case PCI_DEVICE_ID_ADAPTEC_AIC7855:
		return ("Adaptec aic7855 SCSI adapter");
		break;
	case PCI_DEVICE_ID_ADAPTEC_AIC7850:
		return ("Adaptec aic7850 SCSI adapter");
		break;
	case PCI_DEVICE_ID_ADAPTEC_AIC7810:
		return ("Adaptec aic7810 RAID memory controller");
		break;
	case PCI_DEVICE_ID_ADAPTEC_AIC7815:
		return ("Adaptec aic7815 RAID memory controller");
		break;
	default:
		break;
	}
	return (0);

}

static void
ahc_pci_attach(pcici_t config_id, int unit)
{
	pci_port_t	 io_port;
	struct		 ahc_softc *ahc;
	u_int32_t	 id;
	u_int32_t	 command;
	struct scb_data *shared_scb_data;
	int opri;
	ahc_chip	 ahc_t = AHC_NONE;
	ahc_feature	 ahc_fe = AHC_FENONE;
	ahc_flag	 ahc_f = AHC_FNONE;
	vm_offset_t	 vaddr;
	vm_offset_t	 paddr;
	u_int		 our_id = 0;
	u_int		 sxfrctl1;
	u_int		 scsiseq;
	int		 error;
	char		 channel;

	if (config_id->func == 1)
		channel = 'B';
	else
		channel = 'A';
	shared_scb_data = NULL;
	vaddr = NULL;
	paddr = NULL;
	io_port = 0;
	command = pci_conf_read(config_id, PCI_COMMAND_STATUS_REG);
#ifdef AHC_ALLOW_MEMIO
	if ((command & PCI_COMMAND_MEM_ENABLE) == 0
	 || (pci_map_mem(config_id, PCI_BASEADR1, &vaddr, &paddr)) == 0)
#endif
		if ((command & PCI_COMMAND_IO_ENABLE) == 0
		 || (pci_map_port(config_id, PCI_BASEADR0, &io_port)) == 0)
			return;

	switch ((id = pci_conf_read(config_id, PCI_ID_REG))) {
	case PCI_DEVICE_ID_ADAPTEC_398XU:
	case PCI_DEVICE_ID_ADAPTEC_398X:
		if (id == PCI_DEVICE_ID_ADAPTEC_398XU) {
			ahc_t = AHC_AIC7880;
			ahc_fe = AHC_AIC7880_FE;
		} else {
			ahc_t = AHC_AIC7870;
			ahc_fe = AHC_AIC7870_FE;
		}

		switch (config_id->slot) {
		case AHC_398X_SLOT_CHANNEL_A:
			break;
		case AHC_398X_SLOT_CHANNEL_B:
			channel = 'B';
			break;
		case AHC_398X_SLOT_CHANNEL_C:
			channel = 'C';
			break;
		default:
			printf("adapter at unexpected slot %d\n"
			       "unable to map to a channel\n",
			       config_id->slot);
		}
		ahc_f |= AHC_LARGE_SEEPROM;
		break;
	case PCI_DEVICE_ID_ADAPTEC_3940U:
	case PCI_DEVICE_ID_ADAPTEC_3940:
		if (id == PCI_DEVICE_ID_ADAPTEC_3940U) {
			ahc_t = AHC_AIC7880;
			ahc_fe = AHC_AIC7880_FE;
		} else {
			ahc_t = AHC_AIC7870;
			ahc_fe = AHC_AIC7870_FE;
		}

		switch (config_id->slot) {
		case AHC_394X_SLOT_CHANNEL_A:
			break;
		case AHC_394X_SLOT_CHANNEL_B:
			channel = 'B';
			break;
		default:
			printf("adapter at unexpected slot %d\n"
			       "unable to map to a channel\n",
			       config_id->slot);
		}
		break;
	case PCI_DEVICE_ID_ADAPTEC_AIC7890:
	case PCI_DEVICE_ID_ADAPTEC_2940U2:
	case PCI_DEVICE_ID_ADAPTEC_2930U2:
	{
		ahc_t = AHC_AIC7890;
		ahc_fe = AHC_AIC7890_FE;
		break;
	}
	case PCI_DEVICE_ID_ADAPTEC_AIC7896:
	case PCI_DEVICE_ID_ADAPTEC_3950U2:
	{
		ahc_t = AHC_AIC7896;
		ahc_fe = AHC_AIC7896_FE;
		break;
	}
	case PCI_DEVICE_ID_ADAPTEC_2944U:
	case PCI_DEVICE_ID_ADAPTEC_2940U:
	case PCI_DEVICE_ID_ADAPTEC_AIC7880:
		ahc_t = AHC_AIC7880;
		ahc_fe = AHC_AIC7880_FE;
		break;
	case PCI_DEVICE_ID_ADAPTEC_2944:
	case PCI_DEVICE_ID_ADAPTEC_2940:
	case PCI_DEVICE_ID_ADAPTEC_AIC7870:
		ahc_t = AHC_AIC7870;
		ahc_fe = AHC_AIC7870_FE;
		break;
	case PCI_DEVICE_ID_ADAPTEC_2940AU:
	case PCI_DEVICE_ID_ADAPTEC_AIC7860:
		ahc_fe = AHC_AIC7860_FE;
		ahc_t = AHC_AIC7860;
		break;
	case PCI_DEVICE_ID_ADAPTEC_AIC7895:
	case PCI_DEVICE_ID_ADAPTEC_AIC7895C:
	{
		u_int32_t devconfig;

		ahc_t = AHC_AIC7895;
		ahc_fe = AHC_AIC7895_FE;
		devconfig = pci_conf_read(config_id, DEVCONFIG);
		devconfig &= ~SCBSIZE32;
		pci_conf_write(config_id, DEVCONFIG, devconfig);
		break;
	}
	case PCI_DEVICE_ID_ADAPTEC_AIC7855:
	case PCI_DEVICE_ID_ADAPTEC_AIC7850:
		ahc_t = AHC_AIC7850;
		ahc_fe = AHC_AIC7850_FE;
		break;
	case PCI_DEVICE_ID_ADAPTEC_AIC7810:
	case PCI_DEVICE_ID_ADAPTEC_AIC7815:
		printf("RAID functionality unsupported\n");
		return;
	default:
		break;
	}

	/* On all PCI adapters, we allow SCB paging */
	ahc_f |= AHC_PAGESCBS;
	if ((ahc = ahc_alloc(unit, io_port, vaddr, ahc_t|AHC_PCI, ahc_fe, ahc_f,
	     shared_scb_data)) == NULL)
		return;  /* XXX PCI code should take return status */

	ahc->channel = channel;

	/* Allocate a dmatag for our SCB DMA maps */
	/* XXX Should be a child of the PCI bus dma tag */
	error = bus_dma_tag_create(/*parent*/NULL, /*alignment*/0,
				   /*boundary*/0,
				   /*lowaddr*/BUS_SPACE_MAXADDR_32BIT,
				   /*highaddr*/BUS_SPACE_MAXADDR,
				   /*filter*/NULL, /*filterarg*/NULL,
				   /*maxsize*/MAXBSIZE, /*nsegments*/AHC_NSEG,
				   /*maxsegsz*/AHC_MAXTRANSFER_SIZE,
				    /*flags*/BUS_DMA_ALLOCNOW, &ahc->dmat);

	if (error != 0) {
		printf("%s: Could not allocate DMA tag - error %d\n",
		       ahc_name(ahc), error);
		ahc_free(ahc);
		return;
	}


	/* Store our PCI bus information for use in our PCI error handler */
	ahc->pci_config_id = config_id;
	
	/* Remeber how the card was setup in case there is no SEEPROM */
	ahc_outb(ahc, HCNTRL, ahc->pause);
	if ((ahc->features & AHC_ULTRA2) != 0)
		our_id = ahc_inb(ahc, SCSIID_ULTRA2) & OID;
	else
		our_id = ahc_inb(ahc, SCSIID) & OID;
	sxfrctl1 = ahc_inb(ahc, SXFRCTL1) & STPWEN;
	scsiseq = ahc_inb(ahc, SCSISEQ);

	if (ahc_reset(ahc) != 0) {
		/* Failed */
		ahc_free(ahc);
		return;
	}

	/*
	 * Take a look to see if we have external SRAM.
	 * We currently do not attempt to use SRAM that is
	 * shared among multiple controllers.
	 */
	if ((ahc->features & AHC_ULTRA2) != 0) {
		u_int dscommand0;

		dscommand0 = ahc_inb(ahc, DSCOMMAND0);
		if ((dscommand0 & RAMPS) != 0) {
			u_int32_t devconfig;

			devconfig = pci_conf_read(config_id, DEVCONFIG);
			if ((devconfig & MPORTMODE) != 0) {
				/* Single user mode */

				/*
				 * XXX Assume 9bit SRAM and enable
				 * parity checking
				 */
				devconfig |= EXTSCBPEN;
				pci_conf_write(config_id, DEVCONFIG, devconfig);

				/*
				 * Set the bank select apropriately.
				 */
				if (ahc->channel == 'B')
					ahc_outb(ahc, SCBBADDR, 1);
				else
					ahc_outb(ahc, SCBBADDR, 0);
				
				/* Select external SCB SRAM */
				dscommand0 &= ~INTSCBRAMSEL;
				ahc_outb(ahc, DSCOMMAND0, dscommand0);

				if (ahc_probe_scbs(ahc) == 0) {
					/* External ram isn't really there */
					dscommand0 |= INTSCBRAMSEL;
					ahc_outb(ahc, DSCOMMAND0, dscommand0);
				} else if (bootverbose)
					printf("%s: External SRAM bank%d\n",
					       ahc_name(ahc),
					       ahc->channel == 'B' ? 1 : 0);
			}

		}
	} else if ((ahc->chip & AHC_CHIPID_MASK) >= AHC_AIC7870) {
		u_int32_t devconfig = pci_conf_read(config_id, DEVCONFIG);
		if ((devconfig & RAMPSM) != 0
		 && (devconfig & MPORTMODE) != 0) {

			/* XXX Assume 9bit SRAM and enable parity checking */
			devconfig |= EXTSCBPEN;

			/* XXX Assume fast SRAM */
			devconfig &= ~EXTSCBTIME;

			/*
			 * Set the bank select apropriately.
			 */
			if ((ahc->chip & AHC_CHIPID_MASK) == AHC_AIC7895) {
				if (ahc->channel == 'B')
					ahc_outb(ahc, SCBBADDR, 1);
				else
					ahc_outb(ahc, SCBBADDR, 0);
			}

			/* Select external SRAM */
			devconfig &= ~SCBRAMSEL;
			pci_conf_write(config_id, DEVCONFIG, devconfig);

			if (ahc_probe_scbs(ahc) == 0) {
				/* External ram isn't really there */
				devconfig |= SCBRAMSEL;
				pci_conf_write(config_id, DEVCONFIG, devconfig);
			} else if (bootverbose)
				printf("%s: External SRAM bank%d\n",
				       ahc_name(ahc),
				       ahc->channel == 'B' ? 1 : 0);
		}
	}

	if (!(pci_map_int(config_id, ahc_intr, (void *)ahc, &cam_imask))) {
		ahc_free(ahc);
		return;
	}
	/*
	 * Protect ourself from spurrious interrupts during
	 * intialization.
	 */
	opri = splcam();

	/*
	 * Do aic7880/aic7870/aic7860/aic7850 specific initialization
	 */
	{
		u_int8_t sblkctl;
		char	 *id_string;

		switch(ahc_t) {
		case AHC_AIC7896:
		{
			u_int dscommand0;

			/*
			 * DPARCKEN doesn't work correctly on
			 * some MBs so don't use it.
			 */
			id_string = "aic7896/97 ";
			dscommand0 = ahc_inb(ahc, DSCOMMAND0);
			dscommand0 &= ~(USCBSIZE32|DPARCKEN);
			dscommand0 |= CACHETHEN|MPARCKEN;
			ahc_outb(ahc, DSCOMMAND0, dscommand0);
			break;
		}
		case AHC_AIC7890:
		{
			u_int dscommand0;

			/*
			 * DPARCKEN doesn't work correctly on
			 * some MBs so don't use it.
			 */
			id_string = "aic7890/91 ";
			dscommand0 = ahc_inb(ahc, DSCOMMAND0);
			dscommand0 &= ~(USCBSIZE32|DPARCKEN);
			dscommand0 |= CACHETHEN|MPARCKEN;
			ahc_outb(ahc, DSCOMMAND0, dscommand0);
			break;
		}
		case AHC_AIC7895:
			id_string = "aic7895 ";
			break;
		case AHC_AIC7880:
			id_string = "aic7880 ";
			break;
		case AHC_AIC7870:
			id_string = "aic7870 ";
			break;
		case AHC_AIC7860:
			id_string = "aic7860 ";
			break;
		case AHC_AIC7850:
			id_string = "aic7850 ";
			break;
		default:
			printf("ahc: Unknown controller type.  Ignoring.\n");
			ahc_free(ahc);
			splx(opri);
			return;
		}

		/* See if we have an SEEPROM and perform auto-term */
		check_extport(ahc, &sxfrctl1);

		/*
		 * Take the LED out of diagnostic mode
		 */
		sblkctl = ahc_inb(ahc, SBLKCTL);
		ahc_outb(ahc, SBLKCTL, (sblkctl & ~(DIAGLEDEN|DIAGLEDON)));

		/*
		 * I don't know where this is set in the SEEPROM or by the
		 * BIOS, so we default to 100% on Ultra or slower controllers
		 * and 75% on ULTRA2 controllers.
		 */
		if ((ahc->features & AHC_ULTRA2) != 0) {
			ahc_outb(ahc, DFF_THRSH, RD_DFTHRSH_75|WR_DFTHRSH_75);
		} else {
			ahc_outb(ahc, DSPCISTATUS, DFTHRSH_100);
		}

		if (ahc->flags & AHC_USEDEFAULTS) {
			/*
			 * PCI Adapter default setup
			 * Should only be used if the adapter does not have
			 * an SEEPROM.
			 */
			/* See if someone else set us up already */
			if (scsiseq != 0) {
				printf("%s: Using left over BIOS settings\n",
					ahc_name(ahc));
				ahc->flags &= ~AHC_USEDEFAULTS;
			} else {
				/*
				 * Assume only one connector and always turn
				 * on termination.
				 */
 				our_id = 0x07;
				sxfrctl1 = STPWEN;
			}
			ahc_outb(ahc, SCSICONF,
				 (our_id & 0x07)|ENSPCHK|RESET_SCSI);

			ahc->our_id = our_id;
		}

		printf("%s: %s", ahc_name(ahc), id_string);
	}

	/*
	 * Record our termination setting for the
	 * generic initialization routine.
	 */
	if ((sxfrctl1 & STPWEN) != 0)
		ahc->flags |= AHC_TERM_ENB_A;

	if (ahc_init(ahc)) {
		ahc_free(ahc);
		splx(opri);
		return; /* XXX PCI code should take return status */
	}

	/* XXX Crude hack - fix sometime */
	if (ahc->flags & AHC_SHARED_SRAM) {
		/* Only set this once we've successfully probed */
		if (shared_scb_data == NULL)
			first_398X = ahc;
	}

	splx(opri);

	ahc_attach(ahc);
}

/*
 * Check the external port logic for a serial eeprom
 * and termination/cable detection contrls.
 */
static void
check_extport(struct ahc_softc *ahc, u_int *sxfrctl1)
{
	struct	  seeprom_descriptor sd;
	struct	  seeprom_config sc;
	u_int8_t  scsi_conf;
	int	  have_seeprom;

	sd.sd_tag = ahc->tag;
	sd.sd_bsh = ahc->bsh;
	sd.sd_control_offset = SEECTL;		
	sd.sd_status_offset = SEECTL;		
	sd.sd_dataout_offset = SEECTL;		

	/*
	 * For some multi-channel devices, the c46 is simply too
	 * small to work.  For the other controller types, we can
	 * get our information from either SEEPROM type.  Set the
	 * type to start our probe with accordingly.
	 */
	if (ahc->flags & AHC_LARGE_SEEPROM)
		sd.sd_chip = C56_66;
	else
		sd.sd_chip = C46;

	sd.sd_MS = SEEMS;
	sd.sd_RDY = SEERDY;
	sd.sd_CS = SEECS;
	sd.sd_CK = SEECK;
	sd.sd_DO = SEEDO;
	sd.sd_DI = SEEDI;

	have_seeprom = acquire_seeprom(ahc, &sd);

	if (have_seeprom) {

		if (bootverbose) 
			printf("%s: Reading SEEPROM...", ahc_name(ahc));

		for (;;) {
			bus_size_t start_addr;

			start_addr = 32 * (ahc->channel - 'A');

			have_seeprom = read_seeprom(&sd, (u_int16_t *)&sc,
						    start_addr, sizeof(sc)/2);

			if (have_seeprom) {
				/* Check checksum */
				int i;
				int maxaddr;
				u_int16_t *scarray;
				u_int16_t checksum;

				maxaddr = (sizeof(sc)/2) - 1;
				checksum = 0;
				scarray = (u_int16_t *)&sc;

				for (i = 0; i < maxaddr; i++)
					checksum = checksum + scarray[i];
				if (checksum == 0 || checksum != sc.checksum) {
					if (bootverbose && sd.sd_chip == C56_66)
						printf ("checksum error\n");
					have_seeprom = 0;
				} else {
					if (bootverbose)
						printf("done.\n");
					break;
				}
			}

			if (sd.sd_chip == C56_66)
				break;
			sd.sd_chip = C56_66;
		}
	}

	if (!have_seeprom) {
		if (bootverbose)
			printf("%s: No SEEPROM available.\n", ahc_name(ahc));
		ahc->flags |= AHC_USEDEFAULTS;
	} else {
		/*
		 * Put the data we've collected down into SRAM
		 * where ahc_init will find it.
		 */
		int i;
		int max_targ = sc.max_targets & CFMAXTARG;
		u_int16_t discenable;
		u_int16_t ultraenb;

		discenable = 0;
		ultraenb = 0;
		if ((sc.adapter_control & CFULTRAEN) != 0) {
			/*
			 * Determine if this adapter has a "newstyle"
			 * SEEPROM format.
			 */
			for (i = 0; i < max_targ; i++) {
				if ((sc.device_flags[i] & CFSYNCHISULTRA) != 0){
					ahc->flags |= AHC_NEWEEPROM_FMT;
					break;
				}
			}
		}

		for (i = 0; i < max_targ; i++) {
			u_int     scsirate;
			u_int16_t target_mask;

			target_mask = 0x01 << i;
			if (sc.device_flags[i] & CFDISC)
				discenable |= target_mask;
			if ((ahc->flags & AHC_NEWEEPROM_FMT) != 0) {
				if ((sc.device_flags[i] & CFSYNCHISULTRA) != 0)
					ultraenb |= target_mask;
			} else if ((sc.adapter_control & CFULTRAEN) != 0) {
				ultraenb |= target_mask;
			}
			if ((sc.device_flags[i] & CFXFER) == 0x04
			 && (ultraenb & target_mask) != 0) {
				/* Treat 10MHz as a non-ultra speed */
				sc.device_flags[i] &= ~CFXFER;
			 	ultraenb &= ~target_mask;
			}
			if ((ahc->features & AHC_ULTRA2) != 0) {
				u_int offset;

				if (sc.device_flags[i] & CFSYNCH)
					offset = MAX_OFFSET_ULTRA2;
				else 
					offset = 0;
				ahc_outb(ahc, TARG_OFFSET + i, offset);

				scsirate = (sc.device_flags[i] & CFXFER)
					 | ((ultraenb & target_mask)
					    ? 0x18 : 0x10);
				if (sc.device_flags[i] & CFWIDEB)
					scsirate |= WIDEXFER;
			} else {
				scsirate = (sc.device_flags[i] & CFXFER) << 4;
				if (sc.device_flags[i] & CFSYNCH)
					scsirate |= SOFS;
				if (sc.device_flags[i] & CFWIDEB)
					scsirate |= WIDEXFER;
			}
			ahc_outb(ahc, TARG_SCSIRATE + i, scsirate);
		}
		ahc->our_id = sc.brtime_id & CFSCSIID;

		scsi_conf = (ahc->our_id & 0x7);
		if (sc.adapter_control & CFSPARITY)
			scsi_conf |= ENSPCHK;
		if (sc.adapter_control & CFRESETB)
			scsi_conf |= RESET_SCSI;

		if (sc.bios_control & CFEXTEND)
			ahc->flags |= AHC_EXTENDED_TRANS_A;
		if (ahc->features & AHC_ULTRA
		 && (ahc->flags & AHC_NEWEEPROM_FMT) == 0) {
			/* Should we enable Ultra mode? */
			if (!(sc.adapter_control & CFULTRAEN))
				/* Treat us as a non-ultra card */
				ultraenb = 0;
		}
		/* Set SCSICONF info */
		ahc_outb(ahc, SCSICONF, scsi_conf);
		ahc_outb(ahc, DISC_DSB, ~(discenable & 0xff));
		ahc_outb(ahc, DISC_DSB + 1, ~((discenable >> 8) & 0xff));
		ahc_outb(ahc, ULTRA_ENB, ultraenb & 0xff);
		ahc_outb(ahc, ULTRA_ENB + 1, (ultraenb >> 8) & 0xff);
	}

	if ((ahc->features & AHC_SPIOCAP) != 0) {
		if ((ahc_inb(ahc, SPIOCAP) & SSPIOCPS) != 0) {
			configure_termination(ahc, &sc, &sd, sxfrctl1);
		}
	} else if (have_seeprom) {
		configure_termination(ahc, &sc, &sd, sxfrctl1);
	}

	release_seeprom(&sd);
}

static void
configure_termination(struct ahc_softc *ahc,
		      struct seeprom_config *sc,
		      struct seeprom_descriptor *sd,
		      u_int *sxfrctl1)
{
	int max_targ = sc->max_targets & CFMAXTARG;
	u_int8_t brddat;
	
	brddat = 0;

	/*
	 * Update the settings in sxfrctl1 to match the
	 *termination settings 
	 */
	*sxfrctl1 = 0;
	
	/*
	 * SEECS must be on for the GALS to latch
	 * the data properly.  Be sure to leave MS
	 * on or we will release the seeprom.
	 */
	SEEPROM_OUTB(sd, sd->sd_MS | sd->sd_CS);
	if ((sc->adapter_control & CFAUTOTERM) != 0
	 || (ahc->features & AHC_ULTRA2) != 0) {
		int internal50_present;
		int internal68_present;
		int externalcable_present;
		int eeprom_present;
		int enableSEC_low;
		int enableSEC_high;
		int enablePRI_low;
		int enablePRI_high;

		enableSEC_low = 0;
		enableSEC_high = 0;
		enablePRI_low = 0;
		enablePRI_high = 0;
		if (ahc->features & AHC_ULTRA2) {
			ahc_ultra2_term_detect(ahc, &enableSEC_low,
					       &enableSEC_high,
					       &enablePRI_low,
					       &enablePRI_high,
					       &eeprom_present);
			if ((sc->adapter_control & CFSEAUTOTERM) == 0) {
				enableSEC_low = (sc->adapter_control & CFSTERM);
				enableSEC_high =
				    (sc->adapter_control & CFWSTERM);
			}
			if ((sc->adapter_control & CFAUTOTERM) == 0) {
				enablePRI_low = enablePRI_high =
				    (sc->adapter_control & CFLVDSTERM);
			}
			/* Make the table calculations below happy */
			internal50_present = 0;
			internal68_present = 1;
			externalcable_present = 1;
		} else if ((ahc->features & AHC_SPIOCAP) != 0) {
			aic785X_cable_detect(ahc, &internal50_present,
					     &externalcable_present,
					     &eeprom_present);
		} else {
			aic787X_cable_detect(ahc, &internal50_present,
					     &internal68_present,
					     &externalcable_present,
					     &eeprom_present);
		}

		if (max_targ <= 8) {
			internal68_present = 0;
		}

		if (bootverbose) {
			if ((ahc->features & AHC_ULTRA2) == 0) {
				printf("%s: internal 50 cable %s present, "
				       "internal 68 cable %s present\n",
				       ahc_name(ahc),
				       internal50_present ? "is":"not",
				       internal68_present ? "is":"not");

				printf("%s: external cable %s present\n",
				       ahc_name(ahc),
				       externalcable_present ? "is":"not");
			}
			printf("%s: BIOS eeprom %s present\n",
			       ahc_name(ahc), eeprom_present ? "is" : "not");

		}

		/*
		 * Now set the termination based on what
		 * we found.
		 * Flash Enable = BRDDAT7
		 * Secondary High Term Enable = BRDDAT6
		 * Secondary Low Term Enable = BRDDAT5 (7890)
		 * Primary High Term Enable = BRDDAT4 (7890)
		 */
		if ((ahc->features & AHC_ULTRA2) == 0
		    && (internal50_present != 0)
		    && (internal68_present != 0)
		    && (externalcable_present != 0)) {
			printf("%s: Illegal cable configuration!!. "
			       "Only two connectors on the "
			       "adapter may be used at a "
			       "time!\n", ahc_name(ahc));
		}

		if ((max_targ > 8)
		 && ((externalcable_present == 0)
		  || (internal68_present == 0)
		  || (enableSEC_high != 0))) {
			brddat |= BRDDAT6;
			if (bootverbose)
				printf("%s: %sHigh byte termination Enabled\n",
				       ahc_name(ahc),
				       enableSEC_high ? "Secondary " : "");
		}

		if (((internal50_present ? 1 : 0)
		   + (internal68_present ? 1 : 0)
		   + (externalcable_present ? 1 : 0)) <= 1
		 || (enableSEC_low != 0)) {
			if ((ahc->features & AHC_ULTRA2) != 0)
				brddat |= BRDDAT5;
			else
				*sxfrctl1 |= STPWEN;
			if (bootverbose)
				printf("%s: %sLow byte termination Enabled\n",
				       ahc_name(ahc),
				       enableSEC_low ? "Secondary " : "");
		}

		if (enablePRI_low != 0) {
			*sxfrctl1 |= STPWEN;
			if (bootverbose)
				printf("%s: Primary Low Byte termination "
				       "Enabled\n", ahc_name(ahc));
		}

		if (enablePRI_high != 0) {
			brddat |= BRDDAT4;
			if (bootverbose)
				printf("%s: Primary High Byte "
				       "termination Enabled\n",
				       ahc_name(ahc));
		}
		
		write_brdctl(ahc, brddat);

	} else {
		if (sc->adapter_control & CFSTERM) {
			if ((ahc->features & AHC_ULTRA2) != 0)
				brddat |= BRDDAT5;
			else
				*sxfrctl1 |= STPWEN;

			if (bootverbose)
				printf("%s: %sLow byte termination Enabled\n",
				       ahc_name(ahc),
				       (ahc->features & AHC_ULTRA2) ? "Primary "
								    : "");
		}

		if (sc->adapter_control & CFWSTERM) {
			brddat |= BRDDAT6;
			if (bootverbose)
				printf("%s: %sHigh byte termination Enabled\n",
				       ahc_name(ahc),
				       (ahc->features & AHC_ULTRA2)
				     ? "Secondary " : "");
		}

		write_brdctl(ahc, brddat);
	}
	SEEPROM_OUTB(sd, sd->sd_MS); /* Clear CS */
}

static void
ahc_ultra2_term_detect(struct ahc_softc *ahc, int *enableSEC_low,
		      int *enableSEC_high, int *enablePRI_low,
		      int *enablePRI_high, int *eeprom_present)
{
	u_int8_t brdctl;

	/*
	 * BRDDAT7 = Eeprom
	 * BRDDAT6 = Enable Secondary High Byte termination
	 * BRDDAT5 = Enable Secondary Low Byte termination
	 * BRDDAT4 = Enable Primary low byte termination
	 * BRDDAT3 = Enable Primary high byte termination
	 */
	brdctl = read_brdctl(ahc);

	*eeprom_present = brdctl & BRDDAT7;
	*enableSEC_high = (brdctl & BRDDAT6);
	*enableSEC_low = (brdctl & BRDDAT5);
	*enablePRI_low = (brdctl & BRDDAT4);
	*enablePRI_high = (brdctl & BRDDAT3);
}

static void
aic787X_cable_detect(struct ahc_softc *ahc, int *internal50_present,
		     int *internal68_present, int *externalcable_present,
		     int *eeprom_present)
{
	u_int8_t brdctl;

	/*
	 * First read the status of our cables.
	 * Set the rom bank to 0 since the
	 * bank setting serves as a multiplexor
	 * for the cable detection logic.
	 * BRDDAT5 controls the bank switch.
	 */
	write_brdctl(ahc, 0);

	/*
	 * Now read the state of the internal
	 * connectors.  BRDDAT6 is INT50 and
	 * BRDDAT7 is INT68.
	 */
	brdctl = read_brdctl(ahc);
	*internal50_present = !(brdctl & BRDDAT6);
	*internal68_present = !(brdctl & BRDDAT7);

	/*
	 * Set the rom bank to 1 and determine
	 * the other signals.
	 */
	write_brdctl(ahc, BRDDAT5);

	/*
	 * Now read the state of the external
	 * connectors.  BRDDAT6 is EXT68 and
	 * BRDDAT7 is EPROMPS.
	 */
	brdctl = read_brdctl(ahc);
	*externalcable_present = !(brdctl & BRDDAT6);
	*eeprom_present = brdctl & BRDDAT7;
}

static void
aic785X_cable_detect(struct ahc_softc *ahc, int *internal50_present,
		     int *externalcable_present, int *eeprom_present)
{
	u_int8_t brdctl;

	ahc_outb(ahc, BRDCTL, BRDRW|BRDCS);
	ahc_outb(ahc, BRDCTL, 0);
	brdctl = ahc_inb(ahc, BRDCTL);
	*internal50_present = !(brdctl & BRDDAT5);
	*externalcable_present = !(brdctl & BRDDAT6);

	*eeprom_present = (ahc_inb(ahc, SPIOCAP) & EEPROM) != 0;
}
	
static int
acquire_seeprom(struct ahc_softc *ahc, struct seeprom_descriptor *sd)
{
	int wait;

	if ((ahc->features & AHC_SPIOCAP) != 0
	 && (ahc_inb(ahc, SPIOCAP) & SEEPROM) == 0)
			return (0);

	/*
	 * Request access of the memory port.  When access is
	 * granted, SEERDY will go high.  We use a 1 second
	 * timeout which should be near 1 second more than
	 * is needed.  Reason: after the chip reset, there
	 * should be no contention.
	 */
	SEEPROM_OUTB(sd, sd->sd_MS);
	wait = 1000;  /* 1 second timeout in msec */
	while (--wait && ((SEEPROM_STATUS_INB(sd) & sd->sd_RDY) == 0)) {
		DELAY(1000);  /* delay 1 msec */
	}
	if ((SEEPROM_STATUS_INB(sd) & sd->sd_RDY) == 0) {
		SEEPROM_OUTB(sd, 0); 
		return (0);
	}
	return(1);
}

static void
release_seeprom(sd)
	struct seeprom_descriptor *sd;
{
	/* Release access to the memory port and the serial EEPROM. */
	SEEPROM_OUTB(sd, 0);
}

static void
write_brdctl(ahc, value)
	struct 	ahc_softc *ahc;
	u_int8_t value;
{
	u_int8_t brdctl;

	if ((ahc->chip & AHC_CHIPID_MASK) == AHC_AIC7895) {
		brdctl = BRDSTB;
	 	if (ahc->channel == 'B')
			brdctl |= BRDCS;
	} else if ((ahc->chip & AHC_CHIPID_MASK) == AHC_AIC7890) {
		brdctl = 0;
	} else {
		brdctl = BRDSTB|BRDCS;
	}
	ahc_outb(ahc, BRDCTL, brdctl);
	brdctl |= value;
	ahc_outb(ahc, BRDCTL, brdctl);
	if ((ahc->chip & AHC_CHIPID_MASK) == AHC_AIC7890)
		brdctl |= BRDSTB_ULTRA2;
	else
		brdctl &= ~BRDSTB;
	ahc_outb(ahc, BRDCTL, brdctl);
	if ((ahc->chip & AHC_CHIPID_MASK) == AHC_AIC7890)
		brdctl = 0;
	else
		brdctl &= ~BRDCS;
	ahc_outb(ahc, BRDCTL, brdctl);
}

static u_int8_t
read_brdctl(ahc)
	struct 	ahc_softc *ahc;
{
	u_int8_t brdctl;
	u_int8_t value;

	if ((ahc->chip & AHC_CHIPID_MASK) == AHC_AIC7895) {
		brdctl = BRDRW;
	 	if (ahc->channel == 'B')
			brdctl |= BRDCS;
	} else if ((ahc->chip & AHC_CHIPID_MASK) == AHC_AIC7890) {
		brdctl = BRDRW_ULTRA2;
	} else {
		brdctl = BRDRW|BRDCS;
	}
	ahc_outb(ahc, BRDCTL, brdctl);
	value = ahc_inb(ahc, BRDCTL);
	ahc_outb(ahc, BRDCTL, 0);
	return (value);
}

#define	DPE	0x80
#define SSE	0x40
#define	RMA	0x20
#define	RTA	0x10
#define STA	0x08
#define DPR	0x01

void
ahc_pci_intr(struct ahc_softc *ahc)
{
	u_int8_t status1;

	status1 = pci_cfgread(ahc->pci_config_id, PCIR_STATUS + 1, /*bytes*/1);

	if (status1 & DPE) {
		printf("%s: Data Parity Error Detected during address "
		       "or write data phase\n", ahc_name(ahc));
	}
	if (status1 & SSE) {
		printf("%s: Signal System Error Detected\n", ahc_name(ahc));
	}
	if (status1 & RMA) {
		printf("%s: Received a Master Abort\n", ahc_name(ahc));
	}
	if (status1 & RTA) {
		printf("%s: Received a Target Abort\n", ahc_name(ahc));
	}
	if (status1 & STA) {
		printf("%s: Signaled a Target Abort\n", ahc_name(ahc));
	}
	if (status1 & DPR) {
		printf("%s: Data Parity Error has been reported via PERR#\n",
		       ahc_name(ahc));
	}
	if ((status1 & (DPE|SSE|RMA|RTA|STA|DPR)) == 0) {
		printf("%s: Latched PCIERR interrupt with "
		       "no status bits set\n", ahc_name(ahc)); 
	}
	pci_cfgwrite(ahc->pci_config_id, PCIR_STATUS + 1, status1, /*bytes*/1);

	if (status1 & (DPR|RMA|RTA)) {
		ahc_outb(ahc, CLRINT, CLRPARERR);
	}
}

#endif /* NPCI > 0 */
