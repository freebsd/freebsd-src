/*
 * Product specific probe and attach routines for:
 *      Buslogic BT946 and BT956 SCSI controllers
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
 *	$Id: bt9xx.c,v 1.3 1995/12/14 14:19:19 peter Exp $
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
#include <i386/scsi/btreg.h>

/* XXX Need more device IDs */
#define PCI_BASEADR0	PCI_MAP_REG_START
#define PCI_DEVICE_ID_BUSLOGIC_946	0x104B1040ul

static char* bt_pci_probe __P((pcici_t tag, pcidi_t type));
static void bt_pci_attach __P((pcici_t config_id, int unit));

static struct  pci_device bt_pci_driver = {
	"bt",
        bt_pci_probe,
        bt_pci_attach,
        &bt_unit,
	NULL
};

DATA_SET (pcidevice_set, bt_pci_driver);

static  char*
bt_pci_probe (pcici_t tag, pcidi_t type)
{
	switch(type) {
		case PCI_DEVICE_ID_BUSLOGIC_946:
			return ("Buslogic 946 SCSI host adapter");
			break;
		default:
			break;
	}
	return (0);

}

static void
bt_pci_attach(config_id, unit)
	pcici_t config_id;
	int	unit;
{
	u_long io_port;
	unsigned opri = 0;
	struct bt_data *bt;

        if(!(io_port = pci_conf_read(config_id, PCI_BASEADR0)))
		return;
	/*
	 * The first bit of PCI_BASEADR0 is always
	 * set hence we mask it off.
	 */
	io_port &= 0xfffffffe;

	if(!(bt = bt_alloc(unit, io_port)))
		return;  /* XXX PCI code should take return status */

	if(!(pci_map_int(config_id, bt_intr, (void *)bt, &bio_imask))) {
		bt_free(bt);
		return;
	}
	/*
	 * Protect ourself from spurrious interrupts during
	 * intialization and attach.  We should really rely
	 * on interrupts during attach, but we don't have
	 * access to our interrupts during ISA probes, so until
	 * that changes, we mask our interrupts during attach
	 * too.
	 */
	opri = splbio();

	if(bt_init(bt)){
		bt_free(bt);
		splx(opri);
		return; /* XXX PCI code should take return status */
	}

	bt_attach(bt);

	splx(opri);
	return;
}

#endif /* NPCI > 0 */
