/*
 * Product specific probe and attach routines for:
 *      3940, 2940, aic7870, and aic7850 SCSI controllers
 *
 * Copyright (c) 1995, 1996 Justin T. Gibbs
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
 * 3. Absolutely no warranty of function or purpose is made by the author
 *    Justin T. Gibbs.
 * 4. Modifications may be freely made to this file if the above conditions
 *    are met.
 *
 *	$Id: aic7870.c,v 1.27 1996/03/11 02:49:48 gibbs Exp $
 */

#include <pci.h>
#if NPCI > 0
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/queue.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <machine/clock.h>

#include <i386/scsi/aic7xxx.h>
#include <i386/scsi/93cx6.h>

#include <dev/aic7xxx/aic7xxx_reg.h>

#define PCI_BASEADR0	PCI_MAP_REG_START
#define PCI_DEVICE_ID_ADAPTEC_3940U	0x82789004ul
#define PCI_DEVICE_ID_ADAPTEC_2944U	0x84789004ul
#define PCI_DEVICE_ID_ADAPTEC_2940U	0x81789004ul
#define PCI_DEVICE_ID_ADAPTEC_3940	0x72789004ul
#define PCI_DEVICE_ID_ADAPTEC_2944	0x74789004ul
#define PCI_DEVICE_ID_ADAPTEC_2940	0x71789004ul
#define PCI_DEVICE_ID_ADAPTEC_AIC7880	0x80789004ul
#define PCI_DEVICE_ID_ADAPTEC_AIC7870	0x70789004ul
#define PCI_DEVICE_ID_ADAPTEC_AIC7850	0x50789004ul

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
 *
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
  unsigned short device_flags[16];	/* words 0-15 */

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
  unsigned short bios_control;		/* word 16 */

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
  unsigned short adapter_control;	/* word 17 */

/*
 * Bus Release, Host Adapter ID
 */
#define CFSCSIID	0x000f		/* host adapter SCSI ID */
/* UNUSED		0x00f0 */
#define CFBRTIME	0xff00		/* bus release time */
 unsigned short brtime_id;		/* word 18 */

/*
 * Maximum targets
 */
#define CFMAXTARG	0x00ff	/* maximum targets */
/* UNUSED		0xff00 */
  unsigned short max_targets;		/* word 19 */

  unsigned short res_1[11];		/* words 20-30 */
  unsigned short checksum;		/* word 31 */

};

static char* aic7870_probe __P((pcici_t tag, pcidi_t type));
static void aic7870_attach __P((pcici_t config_id, int unit));
static int load_seeprom __P((struct ahc_data *ahc));
static int acquire_seeprom __P((u_long offset, u_short CS, u_short CK,
				u_short DO, u_short DI, u_short RDY,  
				u_short MS));
static void release_seeprom __P((u_long offset, u_short CS, u_short CK,
				 u_short DO, u_short DI, u_short RDY,
				 u_short MS));

static u_char aic3940_count;

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
	switch(type) {
		case PCI_DEVICE_ID_ADAPTEC_3940U:
			return ("Adaptec 3940 Ultra SCSI host adapter");
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
		case PCI_DEVICE_ID_ADAPTEC_AIC7880:
			return ("Adaptec aic7880 Ultra SCSI host adapter");
			break;
		case PCI_DEVICE_ID_ADAPTEC_AIC7870:
			return ("Adaptec aic7870 SCSI host adapter");
			break;
		case PCI_DEVICE_ID_ADAPTEC_AIC7850:
			return ("Adaptec aic7850 SCSI host adapter");
			break;
		default:
			break;
	}
	return (0);

}

static void
aic7870_attach(config_id, unit)
	pcici_t config_id;
	int	unit;
{
	u_long io_port;
	u_long id;
	unsigned opri = 0;
	ahc_type ahc_t = AHC_NONE;
	ahc_flag ahc_f = AHC_FNONE;
	struct ahc_data *ahc;

        if(!(io_port = pci_conf_read(config_id, PCI_BASEADR0)))
		return;
	/*
	 * The first bit of PCI_BASEADR0 is always
	 * set hence we mask it off.
	 */
	io_port &= 0xfffffffe;

	switch ((id = pci_conf_read(config_id, PCI_ID_REG))) {
		case PCI_DEVICE_ID_ADAPTEC_3940U:
		case PCI_DEVICE_ID_ADAPTEC_3940:
			if (id == PCI_DEVICE_ID_ADAPTEC_3940U)
				ahc_t = AHC_394U;
			else
				ahc_t = AHC_394;
			aic3940_count++;
			if(!(aic3940_count & 0x01))
				/* Even count implies second channel */
				ahc_f |= AHC_CHNLB;
			break;
		case PCI_DEVICE_ID_ADAPTEC_2944U:
		case PCI_DEVICE_ID_ADAPTEC_2940U:
			ahc_t = AHC_294U;
			break;
		case PCI_DEVICE_ID_ADAPTEC_2944:
		case PCI_DEVICE_ID_ADAPTEC_2940:
			ahc_t = AHC_294;
			break;
		case PCI_DEVICE_ID_ADAPTEC_AIC7880:
			ahc_t = AHC_AIC7880;
			break;
		case PCI_DEVICE_ID_ADAPTEC_AIC7870:
			ahc_t = AHC_AIC7870;
			break;
		case PCI_DEVICE_ID_ADAPTEC_AIC7850:
			ahc_t = AHC_AIC7850;
			break;
		default:
			break;
	}

	ahc_reset(io_port);

	if(ahc_t & AHC_AIC7870){
		u_long devconfig = pci_conf_read(config_id, DEVCONFIG);
		if(devconfig & (RAMPSM)) {
			/*
			 * External SRAM present.  Have the probe walk
			 * the SCBs to see how much SRAM we have and set
			 * the number of SCBs accordingly.  We have to
			 * turn off SCBRAMSEL to access the external
			 * SCB SRAM.
			 *
			 * It seems that early versions of the aic7870
			 * didn't use these bits, hence the hack for the
			 * 3940 above.  I would guess that recent 3940s
			 * using later aic7870 or aic7880 chips do
			 * actually set RAMPSM.
			 *
			 * The documentation isn't clear, but it sounds
			 * like the value written to devconfig must not
			 * have RAMPSM set.  The second sixteen bits of
			 * the register are R/O anyway, so it shouldn't
			 * affect RAMPSM either way.
			 */
			devconfig &= ~(RAMPSM|SCBRAMSEL);
			pci_conf_write(config_id, DEVCONFIG, devconfig);
		}
	}

	/*
	 * Ensure that we are using good values for the PCI burst size
	 * and latency timer.
	 */
	{
		u_long csize_lattime = pci_conf_read(config_id, CSIZE_LATTIME);
		if((csize_lattime & CACHESIZE) == 0) {
			/* default to 8DWDs. What's the PCI define for this? */
			csize_lattime |= 8;
		}
		if((csize_lattime & LATTIME) == 0) {
			/* Default to 64 PCLKS (is this a good value?) */
			/* This may also be availble in the SEEPROM?? */
			csize_lattime |= (64 << 8);
		}
		if(bootverbose)
			printf("ahc%d: BurstLen = %dDWDs, "
			       "Latency Timer = %dPCLKS\n",
				unit,
				csize_lattime & CACHESIZE,
				(csize_lattime >> 8) & 0xff);
		pci_conf_write(config_id, CSIZE_LATTIME, csize_lattime);
		
	}

	if(!(ahc = ahc_alloc(unit, io_port, ahc_t, ahc_f)))
		return;  /* XXX PCI code should take return status */

	if(!(pci_map_int(config_id, ahc_intr, (void *)ahc, &bio_imask))) {
		ahc_free(ahc);
		return;
	}
	/*
	 * Protect ourself from spurrious interrupts during
	 * intialization.
	 */
	opri = splbio();

	/*
	 * Do aic7870/aic7880/aic7850 specific initialization
	 */
	{
		u_char	sblkctl;
		char	*id_string;
		u_long	iobase = ahc->baseport;

		switch(ahc->type) {
		   case AHC_394U:
		   case AHC_294U:
		   case AHC_AIC7880:
		   {
			id_string = "aic7880 ";
			load_seeprom(ahc);
			break;
		   }
		   case AHC_394:
		   case AHC_294:
		   case AHC_AIC7870:
		   {
			id_string = "aic7870 ";
			load_seeprom(ahc);
			break;
		   }
		   case AHC_AIC7850:
		   {
			id_string = "aic7850 ";
			/* Assume there is no BIOS for these cards? */
			ahc->flags |= AHC_USEDEFAULTS;
			break;
		   }
		   default:
		   {
			printf("ahc: Unknown controller type.  Ignoring.\n");
			return;
			break;
		   }
		}

		printf("ahc%d: %s", unit, id_string);

		/*
		 * Take the LED out of diagnostic mode
		 */
		sblkctl = inb(SBLKCTL + iobase);
		outb(SBLKCTL + iobase, (sblkctl & ~(DIAGLEDEN|DIAGLEDON)));

		/*
		 * I don't know where this is set in the SEEPROM or by the
		 * BIOS, so we default to 100%.
		 */
		outb(DSPCISTATUS + iobase, DFTHRSH_100);

		if(ahc->flags & AHC_USEDEFAULTS) {
			/*
			 * PCI Adapter default setup
			 * Should only be used if the adapter does not have
			 * an SEEPROM and we don't think a BIOS was installed.
			 */
			/* Set the host ID */
			outb(SCSICONF + iobase, 7);
			/* In case we are a wide card */
			outb(SCSICONF + 1 + iobase, 7);
		}
	}

	if(ahc_init(ahc)){
		ahc_free(ahc);
		splx(opri);
		return; /* XXX PCI code should take return status */
	}
	splx(opri);

	ahc_attach(ahc);
	return;
}

/*
 * Read the SEEPROM.  Return 0 on failure
 */
int
load_seeprom(ahc)
	struct	ahc_data *ahc;
{
	struct	seeprom_config sc;
	u_short *scarray = (u_short *)&sc;
	u_short	checksum = 0;
	u_long	iobase = ahc->baseport;
	u_char	scsi_conf;
	u_char	host_id;
	int	have_seeprom, retval;
                 
	if(bootverbose) 
		printf("ahc%d: Reading SEEPROM...", ahc->unit);
	have_seeprom = acquire_seeprom(iobase + SEECTL, SEECS,
				      SEECK, SEEDO, SEEDI, SEERDY, SEEMS);
	if (have_seeprom) {
		have_seeprom = read_seeprom(iobase + SEECTL,
					    (u_short *)&sc,
					    ahc->flags & AHC_CHNLB,
					    sizeof(sc)/2, SEECS, SEECK, SEEDO,
					    SEEDI, SEERDY, SEEMS);
		release_seeprom(iobase + SEECTL, SEECS, SEECK, SEEDO,
				SEEDI, SEERDY, SEEMS);
		if (have_seeprom) {
			/* Check checksum */
			int i;

			for (i = 0;i < (sizeof(sc)/2 - 1);i = i + 1)
				checksum = checksum + scarray[i];
			if (checksum != sc.checksum) {
				printf ("checksum error");
				have_seeprom = 0;
			}
			else if(bootverbose)
				printf("done.\n");
		}
	}
	if (!have_seeprom) {
		printf("\nahc%d: SEEPROM read failed, "
		       "using leftover BIOS values\n", ahc->unit);
		retval = 0;

		host_id = 0x7;
		scsi_conf = host_id | ENSPCHK; /* Assume a default */
		/*
		 * If we happen to be an ULTRA card,
		 * default to non-ultra mode.
		 */
		ahc->type &= ~AHC_ULTRA;
	}
	else {
		/*
		 * Put the data we've collected down into SRAM
		 * where ahc_init will find it.
		 */
		int i;
		int max_targ = sc.max_targets & CFMAXTARG;

	        for(i = 0; i <= max_targ; i++){
	                u_char target_settings;
			target_settings = (sc.device_flags[i] & CFXFER) << 4;
			if (sc.device_flags[i] & CFSYNCH)
				target_settings |= SOFS;
			if (sc.device_flags[i] & CFWIDEB)
				target_settings |= WIDEXFER;
			if (sc.device_flags[i] & CFDISC)
				ahc->discenable |= (0x01 << i);
			outb(TARG_SCRATCH+i+iobase, target_settings);
		}
		outb(DISC_DSB + iobase, ~(ahc->discenable & 0xff));
		outb(DISC_DSB + iobase + 1, ~((ahc->discenable >> 8) & 0xff));

		host_id = sc.brtime_id & CFSCSIID;

		scsi_conf = (host_id & 0x7);
		if(sc.adapter_control & CFSPARITY)
			scsi_conf |= ENSPCHK;

		if(ahc->type & AHC_ULTRA) {
			/* Should we enable Ultra mode? */
			if(!(sc.adapter_control & CFULTRAEN))
				/* Treat us as a non-ultra card */
				ahc->type &= ~AHC_ULTRA;
		}
		retval = 1;
	}
	/* Set the host ID */
	outb(SCSICONF + iobase, scsi_conf);
	/* In case we are a wide card */
	outb(SCSICONF + 1 + iobase, scsi_conf);

	return(retval);
}

static int
acquire_seeprom(offset, CS, CK, DO, DI, RDY, MS)
	u_long   offset;
	u_short  CS;   /* chip select */
	u_short  CK;   /* clock */
	u_short  DO;   /* data out */
	u_short  DI;   /* data in */
	u_short  RDY;  /* ready */
	u_short  MS;   /* mode select */
{
	int wait;
	/*
	 * Request access of the memory port.  When access is
	 * granted, SEERDY will go high.  We use a 1 second
	 * timeout which should be near 1 second more than
	 * is needed.  Reason: after the chip reset, there
	 * should be no contention.
	 */
	outb(offset, MS);
	wait = 1000;  /* 1 second timeout in msec */
	while (--wait && ((inb(offset) & RDY) == 0)) {
		DELAY (1000);  /* delay 1 msec */
        }
	if ((inb(offset) & RDY) == 0) {
		outb (offset, 0); 
		return (0);
	}         
	return(1);
}

static void
release_seeprom(offset, CS, CK, DO, DI, RDY, MS)
	u_long	 offset;
	u_short  CS;   /* chip select */
	u_short  CK;   /* clock */
	u_short  DO;   /* data out */
	u_short  DI;   /* data in */
	u_short  RDY;  /* ready */
	u_short  MS;   /* mode select */
{
	/* Release access to the memory port and the serial EEPROM. */
	outb(offset, 0);
}

#endif /* NPCI > 0 */
