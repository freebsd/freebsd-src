/*
 * Product specific probe and attach routines for:
 *      3940, 2940, aic7880, aic7870, aic7860 and aic7850 SCSI controllers
 *
 * Copyright (c) 1995, 1996 Justin T. Gibbs.
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
 *	$FreeBSD$
 */

#if defined(__FreeBSD__)
#include <pci.h>
#endif
#if NPCI > 0 || defined(__NetBSD__)
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#if defined(__NetBSD__)
#include <sys/device.h>
#include <machine/bus.h>
#include <machine/intr.h>
#endif /* defined(__NetBSD__) */

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#if defined(__FreeBSD__)

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <machine/clock.h>

#include <i386/scsi/aic7xxx.h>
#include <i386/scsi/93cx6.h>

#include <dev/aic7xxx/aic7xxx_reg.h>

#define PCI_BASEADR0	PCI_MAP_REG_START	/* I/O Address */
#define PCI_BASEADR1	PCI_MAP_REG_START + 4	/* Mem I/O Address */

#elif defined(__NetBSD__)

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/ic/aic7xxxreg.h>
#include <dev/ic/aic7xxxvar.h>
#include <dev/ic/smc93cx6var.h>

#define bootverbose	1
#define PCI_BASEADR0	PCI_MAPREG_START	/* I/O Address */
#define PCI_BASEADR1	PCI_MAPREG_START + 4	/* Mem I/O Address */

#endif /* defined(__NetBSD__) */

#define PCI_DEVICE_ID_ADAPTEC_398XU	0x83789004ul
#define PCI_DEVICE_ID_ADAPTEC_3940U	0x82789004ul
#define PCI_DEVICE_ID_ADAPTEC_2944U	0x84789004ul
#define PCI_DEVICE_ID_ADAPTEC_2940U	0x81789004ul
#define PCI_DEVICE_ID_ADAPTEC_2940AU	0x61789004ul
#define PCI_DEVICE_ID_ADAPTEC_398X	0x73789004ul
#define PCI_DEVICE_ID_ADAPTEC_3940	0x72789004ul
#define PCI_DEVICE_ID_ADAPTEC_2944	0x74789004ul
#define PCI_DEVICE_ID_ADAPTEC_2940	0x71789004ul
#define PCI_DEVICE_ID_ADAPTEC_AIC7880	0x80789004ul
#define PCI_DEVICE_ID_ADAPTEC_AIC7870	0x70789004ul
#define PCI_DEVICE_ID_ADAPTEC_AIC7860	0x60789004ul
#define PCI_DEVICE_ID_ADAPTEC_AIC7855	0x55789004ul
#define PCI_DEVICE_ID_ADAPTEC_AIC7850	0x50789004ul
#define PCI_DEVICE_ID_ADAPTEC_AIC7810	0x10789004ul

#define	DEVCONFIG		0x40
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

/*
 * Define the format of the aic78X0 SEEPROM registers (16 bits).
 */

struct seeprom_config {

/*
 * SCSI ID Configuration Flags
 */
#define CFXFER		0x0007		/* synchronous transfer rate */
#define CFSYNCH		0x0008		/* enable synchronous transfer */
#define CFDISC		0x0010		/* enable disconnection */
#define CFWIDEB		0x0020		/* wide bus device */
/* UNUSED		0x00C0 */
#define CFSTART		0x0100		/* send start unit SCSI command */
#define CFINCBIOS	0x0200		/* include in BIOS scan */
#define CFRNFOUND	0x0400		/* report even if not found */
/* UNUSED		0xf800 */
  u_int16_t device_flags[16];	/* words 0-15 */

/*
 * BIOS Control Bits
 */
#define CFSUPREM	0x0001		/* support all removeable drives */
#define CFSUPREMB	0x0002		/* support removeable drives for boot only */
#define CFBIOSEN	0x0004		/* BIOS enabled */
/* UNUSED		0x0008 */
#define CFSM2DRV	0x0010		/* support more than two drives */
/* UNUSED		0x0060 */
#define CFEXTEND	0x0080		/* extended translation enabled */
/* UNUSED		0xff00 */
  u_int16_t bios_control;		/* word 16 */

/*
 * Host Adapter Control Bits
 */
/* UNUSED		0x0001 */
#define CFULTRAEN       0x0002          /* Ultra SCSI speed enable (Ultra cards) */
#define CFSTERM		0x0004		/* SCSI low byte termination (non-wide cards) */
#define CFWSTERM	0x0008		/* SCSI high byte termination (wide card) */
#define CFSPARITY	0x0010		/* SCSI parity */
/* UNUSED		0x0020 */
#define CFRESETB	0x0040		/* reset SCSI bus at IC initialization */
/* UNUSED		0xff80 */
  u_int16_t adapter_control;	/* word 17 */

/*
 * Bus Release, Host Adapter ID
 */
#define CFSCSIID	0x000f		/* host adapter SCSI ID */
/* UNUSED		0x00f0 */
#define CFBRTIME	0xff00		/* bus release time */
 u_int16_t brtime_id;		/* word 18 */

/*
 * Maximum targets
 */
#define CFMAXTARG	0x00ff	/* maximum targets */
/* UNUSED		0xff00 */
  u_int16_t max_targets;		/* word 19 */

  u_int16_t res_1[11];		/* words 20-30 */
  u_int16_t checksum;		/* word 31 */
};

static void load_seeprom __P((struct ahc_softc *ahc, u_int8_t *sxfrctl1));
static int acquire_seeprom __P((struct seeprom_descriptor *sd));
static void release_seeprom __P((struct seeprom_descriptor *sd));

static int aic3940_count;
static int aic398X_count;
static struct ahc_softc *first_398X;

#if defined(__FreeBSD__)

static char* aic7870_probe __P((pcici_t tag, pcidi_t type));
static void aic7870_attach __P((pcici_t config_id, int unit));

static struct  pci_device ahc_pci_driver = {
	"ahc",
        aic7870_probe,
        aic7870_attach,
        &ahc_unit,
	NULL
};

DATA_SET (pcidevice_set, ahc_pci_driver);

static  char*
aic7870_probe (pcici_t tag, pcidi_t type)
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
		return ("Adaptec 3940 SCSI host adapter");
		break;
	case PCI_DEVICE_ID_ADAPTEC_2944U:
		return ("Adaptec 2944 Ultra SCSI host adapter");
		break;
	case PCI_DEVICE_ID_ADAPTEC_2940U:
		return ("Adaptec 2940 Ultra SCSI host adapter");
		break;
	case PCI_DEVICE_ID_ADAPTEC_2944:
		return ("Adaptec 2944 SCSI host adapter");
		break;
	case PCI_DEVICE_ID_ADAPTEC_2940:
		return ("Adaptec 2940 SCSI host adapter");
		break;
	case PCI_DEVICE_ID_ADAPTEC_2940AU:
		return ("Adaptec 2940A Ultra SCSI host adapter");
		break;
	case PCI_DEVICE_ID_ADAPTEC_AIC7880:
		return ("Adaptec aic7880 Ultra SCSI host adapter");
		break;
	case PCI_DEVICE_ID_ADAPTEC_AIC7870:
		return ("Adaptec aic7870 SCSI host adapter");
		break;
	case PCI_DEVICE_ID_ADAPTEC_AIC7860:
		return ("Adaptec aic7860 SCSI host adapter");
		break;
	case PCI_DEVICE_ID_ADAPTEC_AIC7855:
		return ("Adaptec aic7855 SCSI host adapter");
		break;
	case PCI_DEVICE_ID_ADAPTEC_AIC7850:
		return ("Adaptec aic7850 SCSI host adapter");
		break;
	case PCI_DEVICE_ID_ADAPTEC_AIC7810:
		return ("Adaptec aic7810 RAID memory controller");
		break;
	default:
		break;
	}
	return (0);

}

#elif defined(__NetBSD__)

int ahc_pci_probe __P((struct device *, void *, void *));
void ahc_pci_attach __P((struct device *, struct device *, void *));

struct cfattach ahc_pci_ca = {
	sizeof(struct ahc_softc), ahc_pci_probe, ahc_pci_attach
};

int
ahc_pci_probe(parent, match, aux)
        struct device *parent;
        void *match, *aux; 
{       
        struct pci_attach_args *pa = aux;

	switch (pa->pa_id) {
	case PCI_DEVICE_ID_ADAPTEC_398XU:
	case PCI_DEVICE_ID_ADAPTEC_3940U:
	case PCI_DEVICE_ID_ADAPTEC_2944U:
	case PCI_DEVICE_ID_ADAPTEC_2940U:
	case PCI_DEVICE_ID_ADAPTEC_2940AU:
	case PCI_DEVICE_ID_ADAPTEC_398X:
	case PCI_DEVICE_ID_ADAPTEC_3940:
	case PCI_DEVICE_ID_ADAPTEC_2944:
	case PCI_DEVICE_ID_ADAPTEC_2940:
	case PCI_DEVICE_ID_ADAPTEC_AIC7880:
	case PCI_DEVICE_ID_ADAPTEC_AIC7870:
	case PCI_DEVICE_ID_ADAPTEC_AIC7860:
	case PCI_DEVICE_ID_ADAPTEC_AIC7855:
	case PCI_DEVICE_ID_ADAPTEC_AIC7850:
	case PCI_DEVICE_ID_ADAPTEC_AIC7810:
		return 1;
	}
	return 0;
}
#endif /* defined(__NetBSD__) */

#if defined(__FreeBSD__)
static void
aic7870_attach(config_id, unit)
	pcici_t config_id;
	int	unit;
#elif defined(__NetBSD__)
void    
ahc_pci_attach(parent, self, aux)
        struct device *parent, *self;
        void *aux;
#endif
{
#if defined(__FreeBSD__)
	u_int16_t io_port;
	struct	  ahc_softc *ahc;
#elif defined(__NetBSD__)
	struct pci_attach_args *pa = aux;
	struct ahc_softc *ahc = (void *)self;
	int unit = ahc->sc_dev.dv_unit;
	bus_io_addr_t iobase;
	bus_io_size_t iosize;
	bus_io_handle_t ioh;
	pci_intr_handle_t ih;
	const char *intrstr;
#endif
	u_int32_t id;
	struct scb_data *shared_scb_data;
	int opri;
	ahc_type    ahc_t = AHC_NONE;
	ahc_flag    ahc_f = AHC_FNONE;
	vm_offset_t vaddr;
	vm_offset_t paddr;
	u_int8_t    ultra_enb = 0;
	u_int8_t    our_id = 0;
	u_int8_t    sxfrctl1;

	shared_scb_data = NULL;
	vaddr = NULL;
	paddr = NULL;
#if defined(__FreeBSD__)
	io_port = 0;
#ifdef AHC_ALLOW_MEMIO
	if (pci_map_mem(config_id, PCI_BASEADR1, &vaddr, &paddr) == 0)
#endif
		if (pci_map_port(config_id, PCI_BASEADR0, &io_port) == 0)
			return;
		
#elif defined(__NetBSD__)
	/* XXX Memory mapped I/O?? */
	if (bus_io_map(pa->pa_bc, iobase, iosize, &ioh))
		if (pci_io_find(pa->pa_pc, pa->pa_tag, PCI_BASEADR0, &iobase,
				&iosize))
			return;
#endif

#if defined(__FreeBSD__)
	switch ((id = pci_conf_read(config_id, PCI_ID_REG))) {
#elif defined(__NetBSD__)
	switch (id = pa->pa_id) {
#endif
		case PCI_DEVICE_ID_ADAPTEC_398XU:
		case PCI_DEVICE_ID_ADAPTEC_398X:
			if (id == PCI_DEVICE_ID_ADAPTEC_398XU)
				ahc_t = AHC_398U;
			else
				ahc_t = AHC_398;
			switch (aic398X_count) {
			case 0:
				break;
			case 1:
				ahc_f |= AHC_CHNLB;
				break;
			case 2:
				ahc_f |= AHC_CHNLC;
				break;
			default:
				break;
			}
			aic398X_count++; 
			if (first_398X != NULL)
#ifdef AHC_SHARE_SCBS
				shared_scb_data = first_398X->scb_data;
#endif
			if (aic398X_count == 3) {
				/*
				 * This is the last device on this RAID
				 * controller, so reset our counts.
				 * XXX This won't work for the multiple 3980
				 * controllers since they have only 2 channels,
				 * but I'm not even sure if Adaptec actually
				 * went through with their plans to produce
				 * this controller.
				 */
				aic398X_count = 0;
				first_398X = NULL;
			}
			break;
	case PCI_DEVICE_ID_ADAPTEC_3940U:
	case PCI_DEVICE_ID_ADAPTEC_3940:
		if (id == PCI_DEVICE_ID_ADAPTEC_3940U)
			ahc_t = AHC_394U;
		else
			ahc_t = AHC_394;
		if ((aic3940_count & 0x01) != 0)
			/* Odd count implies second channel */
			ahc_f |= AHC_CHNLB;
		aic3940_count++;
		break;
	case PCI_DEVICE_ID_ADAPTEC_2944U:
	case PCI_DEVICE_ID_ADAPTEC_2940U:
		ahc_t = AHC_294U;
		break;
	case PCI_DEVICE_ID_ADAPTEC_2944:
	case PCI_DEVICE_ID_ADAPTEC_2940:
		ahc_t = AHC_294;
		break;
	case PCI_DEVICE_ID_ADAPTEC_2940AU:
		ahc_t = AHC_294AU;
		break;
	case PCI_DEVICE_ID_ADAPTEC_AIC7880:
		ahc_t = AHC_AIC7880;
		break;
	case PCI_DEVICE_ID_ADAPTEC_AIC7870:
		ahc_t = AHC_AIC7870;
		break;
	case PCI_DEVICE_ID_ADAPTEC_AIC7860:
		ahc_t = AHC_AIC7860;
		break;
	case PCI_DEVICE_ID_ADAPTEC_AIC7855:
	case PCI_DEVICE_ID_ADAPTEC_AIC7850:
		ahc_t = AHC_AIC7850;
		break;
	case PCI_DEVICE_ID_ADAPTEC_AIC7810:
		printf("RAID functionality unsupported\n");
		return;
	default:
		break;
	}

	/* On all PCI adapters, we allow SCB paging */
	ahc_f |= AHC_PAGESCBS;
#if defined(__FreeBSD__)
	if ((ahc = ahc_alloc(unit, io_port, vaddr, ahc_t, ahc_f,
	     shared_scb_data)) == NULL)
		return;  /* XXX PCI code should take return status */
#else
	ahc_construct(ahc, pa->pa_bc, ioh, ahc_t, ahc_f);
#endif

	/* Remeber how the card was setup in case there is no SEEPROM */
	our_id = ahc_inb(ahc, SCSIID) & OID;
	if (ahc_t & AHC_ULTRA)
		ultra_enb = ahc_inb(ahc, SXFRCTL0) & ULTRAEN;
	sxfrctl1 = ahc_inb(ahc, SXFRCTL1) & STPWEN;

#if defined(__NetBSD__)
	printf("\n");
#endif
	ahc_reset(ahc);

#ifdef AHC_SHARE_SCBS
	if (ahc_t & AHC_AIC7870) {
#if defined(__FreeBSD__)
		u_int32_t devconfig = pci_conf_read(config_id, DEVCONFIG);
#elif defined(__NetBSD__)
		u_int32_t devconfig =
			pci_conf_read(pa->pa_pc, pa->pa_tag, DEVCONFIG);
#endif
		if (devconfig & (RAMPSM)) {
			/* XXX Assume 9bit SRAM and enable parity checking */
			devconfig |= EXTSCBPEN;

			/* XXX Assume fast SRAM and only enable 2 cycle
			 * access if we are sharing the SRAM across mutiple
			 * adapters (398X adapter).
			 */
			if ((devconfig & MPORTMODE) == 0)
				/* Multi-user mode */
				devconfig |= EXTSCBTIME;

			devconfig &= ~SCBRAMSEL;
#if defined(__FreeBSD__)
			pci_conf_write(config_id, DEVCONFIG, devconfig);
#elif defined(__NetBSD__)
			pci_conf_write(pa->pa_pc, pa->pa_tag,
				       DEVCONFIG, devconfig);
#endif
		}
	}
#endif

#if defined(__FreeBSD__)
	if (!(pci_map_int(config_id, ahc_intr, (void *)ahc, &bio_imask))) {
		ahc_free(ahc);
		return;
	}
#elif defined(__NetBSD__)

	if (pci_intr_map(pa->pa_pc, pa->pa_intrtag, pa->pa_intrpin,
			 pa->pa_intrline, &ih)) {
		printf("%s: couldn't map interrupt\n", ahc->sc_dev.dv_xname);
		ahc_free(ahc);
		return;
	}
	intrstr = pci_intr_string(pa->pa_pc, ih);
#ifdef __OpenBSD__
	ahc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_BIO, ahc_intr, ahc,
					ahc->sc_dev.dv_xname);
#else
	ahc->sc_ih = pci_intr_establish(pa->pa_pc, ih, IPL_BIO, ahc_intr, ahc);
#endif
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
#endif
	/*
	 * Protect ourself from spurrious interrupts during
	 * intialization.
	 */
	opri = splbio();

	/*
	 * Do aic7880/aic7870/aic7860/aic7850 specific initialization
	 */
	{
		u_int8_t sblkctl;
		char	 *id_string;

		switch(ahc->type) {
		case AHC_398U:
		case AHC_394U:
		case AHC_294U:
		case AHC_AIC7880:
			id_string = "aic7880 ";
			load_seeprom(ahc, &sxfrctl1);
			break;
		case AHC_398:
		case AHC_394:
		case AHC_294:
		case AHC_AIC7870:
			id_string = "aic7870 ";
			load_seeprom(ahc, &sxfrctl1);
			break;
		case AHC_294AU:
		case AHC_AIC7860:
			id_string = "aic7860 ";
			load_seeprom(ahc, &sxfrctl1);
			break;
		case AHC_AIC7850:
			id_string = "aic7850 ";
			/*
			 * Use defaults, if the chip wasn't initialized by
			 * a BIOS.
			 */
			ahc->flags |= AHC_USEDEFAULTS;
			break;
		default:
			printf("ahc: Unknown controller type.  Ignoring.\n");
			ahc_free(ahc);
			splx(opri);
			return;
		}

		/*
		 * Take the LED out of diagnostic mode
		 */
		sblkctl = ahc_inb(ahc, SBLKCTL);
		ahc_outb(ahc, SBLKCTL, (sblkctl & ~(DIAGLEDEN|DIAGLEDON)));

		/*
		 * I don't know where this is set in the SEEPROM or by the
		 * BIOS, so we default to 100%.
		 */
		ahc_outb(ahc, DSPCISTATUS, DFTHRSH_100);

		if (ahc->flags & AHC_USEDEFAULTS) {
			/*
			 * PCI Adapter default setup
			 * Should only be used if the adapter does not have
			 * an SEEPROM.
			 */
			/* See if someone else set us up already */
			u_int32_t i;
		        for (i = TARG_SCRATCH; i < 0x60; i++) {
                        	if (ahc_inb(ahc, i) != 0x00)
					break;
			}
			if (i == TARG_SCRATCH) {
				/*
				 * Try looking for all ones.  You can get
				 * either.
				 */
				for (i = TARG_SCRATCH; i < 0x60; i++) {
                        		if (ahc_inb(ahc, i) != 0xff)
						break;
				}
			}
			if ((i != 0x60) && (our_id != 0)) {
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
			/* In case we are a wide card */
			ahc_outb(ahc, SCSICONF + 1, our_id);

			if (!ultra_enb || (ahc->flags & AHC_USEDEFAULTS)) {
				/*
				 * If there wasn't a BIOS or the board
				 * wasn't in this mode to begin with, 
				 * turn off ultra.
				 */
				ahc->type &= ~AHC_ULTRA;
			}
		}

		printf("%s: %s", ahc_name(ahc), id_string);
	}

	if (ahc_init(ahc)){
		ahc_free(ahc);
		splx(opri);
		return; /* XXX PCI code should take return status */
	}

	/*
	 * Put our termination setting into sxfrctl1 now that the
	 * generic initialization is complete.
	 */
	sxfrctl1 |= ahc_inb(ahc, SXFRCTL1);
	ahc_outb(ahc, SXFRCTL1, sxfrctl1);

	if ((ahc->type & AHC_398) == AHC_398) {
		/* Only set this once we've successfully probed */
		if (shared_scb_data == NULL)
			first_398X = ahc;
	}
	splx(opri);

	ahc_attach(ahc);
}

/*
 * Read the SEEPROM.  Return 0 on failure
 */
void
load_seeprom(ahc, sxfrctl1)
	struct	 ahc_softc *ahc;
	u_int8_t *sxfrctl1;
{
	struct	  seeprom_descriptor sd;
	struct	  seeprom_config sc;
	u_int16_t *scarray = (u_int16_t *)&sc;
	u_int16_t checksum = 0;
	u_int8_t  scsi_conf;
	u_int8_t  host_id;
	int	  have_seeprom;
                 
#if defined(__FreeBSD__)
	sd.sd_maddr = ahc->maddr;
	if (sd.sd_maddr != NULL)
		sd.sd_maddr += SEECTL;
	sd.sd_iobase = ahc->baseport;
	if (sd.sd_iobase != 0)
		sd.sd_iobase += SEECTL;
#elif defined(__NetBSD__)
	sd.sd_bc = ahc->sc_bc;
	sd.sd_ioh = ahc->sc_ioh;
	sd.sd_offset = SEECTL;
#endif
	if ((ahc->type & AHC_398) == AHC_398)
		sd.sd_chip = C56_66;
	else
		sd.sd_chip = C46;
	sd.sd_MS = SEEMS;
	sd.sd_RDY = SEERDY;
	sd.sd_CS = SEECS;
	sd.sd_CK = SEECK;
	sd.sd_DO = SEEDO;
	sd.sd_DI = SEEDI;

	if (bootverbose) 
		printf("%s: Reading SEEPROM...", ahc_name(ahc));
	have_seeprom = acquire_seeprom(&sd);
	if (have_seeprom) {
		have_seeprom = read_seeprom(&sd,
					    (u_int16_t *)&sc,
					    ahc->flags & (AHC_CHNLB|AHC_CHNLC),
					    sizeof(sc)/2);
		release_seeprom(&sd);
		if (have_seeprom) {
			/* Check checksum */
			int i;
			int maxaddr = (sizeof(sc)/2) - 1;

			for (i = 0; i < maxaddr; i++)
				checksum = checksum + scarray[i];
			if (checksum != sc.checksum) {
				if(bootverbose)
					printf ("checksum error");
				have_seeprom = 0;
			} else if (bootverbose)
				printf("done.\n");
		}
	}
	if (!have_seeprom) {
		if (bootverbose)
			printf("\n%s: No SEEPROM availible\n", ahc_name(ahc));
		ahc->flags |= AHC_USEDEFAULTS;
	} else {
		/*
		 * Put the data we've collected down into SRAM
		 * where ahc_init will find it.
		 */
		int i;
		int max_targ = sc.max_targets & CFMAXTARG;

		for (i = 0; i < max_targ; i++){
	                u_char target_settings;
			target_settings = (sc.device_flags[i] & CFXFER) << 4;
			if (sc.device_flags[i] & CFSYNCH)
				target_settings |= SOFS;
			if (sc.device_flags[i] & CFWIDEB)
				target_settings |= WIDEXFER;
			if (sc.device_flags[i] & CFDISC)
				ahc->discenable |= (0x01 << i);
			ahc_outb(ahc, TARG_SCRATCH+i, target_settings);
		}
		ahc_outb(ahc, DISC_DSB, ~(ahc->discenable & 0xff));
		ahc_outb(ahc, DISC_DSB + 1, ~((ahc->discenable >> 8) & 0xff));

		host_id = sc.brtime_id & CFSCSIID;

		scsi_conf = (host_id & 0x7);
		if (sc.adapter_control & CFSPARITY)
			scsi_conf |= ENSPCHK;
		if (sc.adapter_control & CFRESETB)
			scsi_conf |= RESET_SCSI;

		/*
		 * Update the settings in sxfrctl1 to match the
		 *termination settings
		 */
		*sxfrctl1 = 0;
		if (sc.adapter_control & CFSTERM)
			*sxfrctl1 |= STPWEN;

		if (ahc->type & AHC_ULTRA) {
			/* Should we enable Ultra mode? */
			if (!(sc.adapter_control & CFULTRAEN))
				/* Treat us as a non-ultra card */
				ahc->type &= ~AHC_ULTRA;
		}
		/* Set the host ID */
		ahc_outb(ahc, SCSICONF, scsi_conf);
		/* In case we are a wide card */
		ahc_outb(ahc, SCSICONF + 1, host_id);
	}
}

static int
acquire_seeprom(sd)
	struct seeprom_descriptor *sd;
{
	int wait;

	/*
	 * Request access of the memory port.  When access is
	 * granted, SEERDY will go high.  We use a 1 second
	 * timeout which should be near 1 second more than
	 * is needed.  Reason: after the chip reset, there
	 * should be no contention.
	 */
	SEEPROM_OUTB(sd, sd->sd_MS);
	wait = 1000;  /* 1 second timeout in msec */
	while (--wait && ((SEEPROM_INB(sd) & sd->sd_RDY) == 0)) {
		DELAY (1000);  /* delay 1 msec */
        }
	if ((SEEPROM_INB(sd) & sd->sd_RDY) == 0) {
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

#endif /* NPCI > 0 */
