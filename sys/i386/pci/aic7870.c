/*
 * Product specific probe and attach routines for:
 *      294X and aic7870 motherboard SCSI controllers
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
 *	$Id$
 */

#include <pci.h>
#if NPCI > 0
#include <sys/param.h>
#include <sys/systm.h>
#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <i386/pci/pcireg.h>
#include <i386/scsi/aic7xxx.h>

#define PCI_BASEADR0	PCI_MAP_REG_START
#define PCI_DEVICE_ID_ADAPTEC_2940	0x71789004ul
#define PCI_DEVICE_ID_ADAPTEC_2940_MB	0x70789004ul

static char* aic7870_probe __P((pcici_t tag, pcidi_t type));
void aic7870_attach __P((pcici_t config_id, int unit));

static u_long aic7870_count;

struct  pci_driver ahc_device = {
        aic7870_probe,
        aic7870_attach,
        &aic7870_count
};

static  char* 
aic7870_probe (pcici_t tag, pcidi_t type)
{   
	switch(type) {
		case PCI_DEVICE_ID_ADAPTEC_2940:
				return ("Adaptec 294X SCSI host adapter");
				break;
		case PCI_DEVICE_ID_ADAPTEC_2940_MB:
				return ("Adaptec aic7870 SCSI host adapter"
					": on Motherboard");
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
	if(!(io_port = pci_conf_read(config_id, PCI_BASEADR0)))
		return;
	io_port -= 0xc01ul;	/*
	printf("io_port = 0x%lx\n", io_port);
				 * Make the offsets the same as for EISA 
				 * The first bit of PCI_BASEADR0 is always
				 * set hence we subtract 0xc01 instead of the
				 * 0xc00 that you would expect.
				 */
	if(ahcprobe(unit, io_port, AHC_294)){
		ahc_unit++;
		if(ahc_attach(unit))
			/*
			 * To be compatible with the isa style of 
			 * interrupt handler, we pass the unit number
			 * not a pointer to our per device structure.
			 */ 
			pci_map_int (config_id, ahcintr, (void *)unit, 
				     &bio_imask);
	}
	return;
}       

#endif /* NPCI > 0 */
