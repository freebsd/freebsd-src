/*
 * Product specific probe and attach routines for:
 *      3940, 2940, aic7870, and aic7850 SCSI controllers
 *
 * Copyright (c) 1995 Justin T. Gibbs
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
 *	$Id: aic7870.c,v 1.17 1995/11/04 14:43:20 bde Exp $
 */

#include <pci.h>
#if NPCI > 0
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <pci/pcireg.h>
#include <pci/pcivar.h>
#include <i386/scsi/aic7xxx.h>

#define PCI_BASEADR0	PCI_MAP_REG_START
#define PCI_DEVICE_ID_ADAPTEC_3940U	0x82789004ul
#define PCI_DEVICE_ID_ADAPTEC_2940U	0x81789004ul
#define PCI_DEVICE_ID_ADAPTEC_3940	0x72789004ul
#define PCI_DEVICE_ID_ADAPTEC_2940	0x71789004ul
#define PCI_DEVICE_ID_ADAPTEC_AIC7880	0x80789004ul
#define PCI_DEVICE_ID_ADAPTEC_AIC7870	0x70789004ul
#define PCI_DEVICE_ID_ADAPTEC_AIC7850	0x50789004ul

#define	DEVCONFIG		0x40
#define		MPORTMODE	0x00000400ul	/* aic7870 only */
#define		RAMPSM		0x00000200ul	/* aic7870 only */
#define		VOLSENSE	0x00000100ul
#define		RAMENB		0x00000080ul
#define		MRDCEN		0x00000040ul
#define		EXTSCBTIME	0x00000020ul	/* aic7870 only */
#define		EXTSCBPEN	0x00000010ul	/* aic7870 only */
#define		BERREN		0x00000008ul
#define		DACEN		0x00000004ul
#define		STPWLEVEL	0x00000002ul
#define		DIFACTNEGEN	0x00000001ul	/* aic7870 only */

static char* aic7870_probe __P((pcici_t tag, pcidi_t type));
void aic7870_attach __P((pcici_t config_id, int unit));

static u_char aic3940_count;

struct  pci_device ahc_pci_driver = {
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
		case PCI_DEVICE_ID_ADAPTEC_2940U:
			return ("Adaptec 2940 Ultra SCSI host adapter");
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

void
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
			/* Even though it doesn't turn on RAMPS, it has them */
			ahc_f |= AHC_EXTSCB;
			break;
		case PCI_DEVICE_ID_ADAPTEC_2940U:
			ahc_t = AHC_294U;
			break;
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

	if(ahc_t & AHC_AIC7870){
		u_long devconfig = pci_conf_read(config_id, DEVCONFIG);
		if(devconfig & (RAMPSM|RAMENB)) {
			/*
			 * External SRAM present.  Have the probe walk
			 * the SCBs to see how much SRAM we have and set
			 * the number of SCBs accordingly.
			 */
			ahc_f |= AHC_EXTSCB;
		}
	}

	ahc_reset(io_port);

	if(!(ahc = ahc_alloc(unit, io_port, ahc_t, ahc_f)))
		return;  /* XXX PCI code should take return status */

	if(!(pci_map_int(config_id, ahcintr, (void *)ahc, &bio_imask))) {
		ahc_free(ahc);
		return;
	}
	/*
	 * Protect ourself from spurrious interrupts during
	 * intialization.
	 */
	opri = splbio();

	if(ahc_init(unit)){
		ahc_free(ahc);
		splx(opri);
		return; /* XXX PCI code should take return status */
	}
	ahc_unit++;

	ahc_attach(unit);
	splx(opri);
	return;
}

#endif /* NPCI > 0 */
