/*
 *
 * Copyright (c) 1996 Wolfgang Helbig <helbig@ba-stuttgart.de>
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
 * 3. Absolutely no warranty of function or purpose is made by the author.
 * 4. Modifications may be freely made to this file if the above conditions
 *    are met.
 *
 *	$Id: wdc_p.c,v 1.1 1997/03/11 23:17:26 se Exp $
 */

/*
 * The sole purpose of this code currently is to tell the ISA wdc driver,
 * whether there is a CMD640 IDE chip attached to the PCI bus.
 */

#include "pci.h"
#if NPCI > 0
#include "opt_wd.h"
#ifdef CMD640

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <pci/pcireg.h>
#include <pci/pcivar.h>
#include <i386/isa/wdc_p.h>

#include "wdc.h"

/*
 * PCI-ID's of IDE-Controller
 */

#define CMD640B_PCI_ID	0x06401095

static char* wdc_pci_probe __P((pcici_t tag, pcidi_t type));
static void wdc_pci_attach __P((pcici_t config_id, int unit));

static u_long wdc_pci_count = 0;

static struct pci_device wdc_pci_driver = {
	"wdc",
	wdc_pci_probe,
	wdc_pci_attach,
	&wdc_pci_count,
	NULL
};

DATA_SET (pcidevice_set, wdc_pci_driver);

static char*
wdc_pci_probe (pcici_t tag, pcidi_t type)
{
	if (type == CMD640B_PCI_ID)
		return "CMD 640B IDE";

	return NULL;
}

static void
wdc_pci_attach(pcici_t config_id, int unit)
{
	if (pci_conf_read(config_id, PCI_ID_REG) == CMD640B_PCI_ID)
		wdc_pci(Q_CMD640B);
}

#endif /* CMD640 */
#endif /* NPCI > 0 */
