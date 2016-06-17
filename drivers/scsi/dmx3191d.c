
/*
    dmx3191d.c - midlevel driver for the Domex DMX3191D SCSI card.
    Copyright (C) 2000 by Massimo Piccioni <dafastidio@libero.it>

    Based on the generic NCR5380 driver by Drew Eckhardt et al.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <asm/io.h>
#include <asm/system.h>
#include <linux/blk.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/stat.h>
#include <linux/version.h>

#include "scsi.h"
#include "hosts.h"
#include "constants.h"
#include "sd.h"

#include "dmx3191d.h"

/* play with these values to tune up your system performances */
/* default setting from g_NCR5380.c */
/*
#define USLEEP
#define USLEEP_POLL		1
#define USLEEP_SLEEP		20
#define USLEEP_WAITLONG		500
*/

#define AUTOSENSE
#include "NCR5380.h"
#include "NCR5380.c"


int __init dmx3191d_detect(Scsi_Host_Template *tmpl) {
	int boards = 0;
	struct Scsi_Host *instance = NULL;
	struct pci_dev *pdev = NULL;

	if (!pci_present()) {
		printk(KERN_WARNING "dmx3191: PCI support not enabled\n");
		return 0;
	}

	tmpl->proc_name = DMX3191D_DRIVER_NAME;

	while ((pdev = pci_find_device(PCI_VENDOR_ID_DOMEX,
			PCI_DEVICE_ID_DOMEX_DMX3191D, pdev))) {

		unsigned long port;
		if (pci_enable_device(pdev))
			continue;

		port = pci_resource_start (pdev, 0);
		
		if (!request_region(port, DMX3191D_REGION, DMX3191D_DRIVER_NAME)) {
			printk(KERN_ERR "dmx3191: region 0x%lx-0x%lx already reserved\n",
				port, port + DMX3191D_REGION);
			continue;
		}

		instance = scsi_register(tmpl, sizeof(struct NCR5380_hostdata));
		if(instance == NULL)
		{
			release_region(port, DMX3191D_REGION);
			continue;
		}
		scsi_set_pci_device(instance, pdev);
		instance->io_port = port;
		instance->irq = pdev->irq;
		NCR5380_init(instance, FLAG_NO_PSEUDO_DMA | FLAG_DTC3181E);

		if (request_irq(pdev->irq, dmx3191d_do_intr, SA_SHIRQ,
				DMX3191D_DRIVER_NAME, instance)) {
			printk(KERN_WARNING "dmx3191: IRQ %d not available - switching to polled mode.\n", pdev->irq);
			/* Steam powered scsi controllers run without an IRQ
			   anyway */
			instance->irq = SCSI_IRQ_NONE;
		}

		boards++;
	}
	return boards;
}

const char * dmx3191d_info(struct Scsi_Host *host) {
	static const char *info ="Domex DMX3191D";

	return info;
}

int dmx3191d_release_resources(struct Scsi_Host *instance)
{
	release_region(instance->io_port, DMX3191D_REGION);
	if(instance->irq != SCSI_IRQ_NONE)
		free_irq(instance->irq, instance);

	return 0;
}

MODULE_LICENSE("GPL");

static Scsi_Host_Template driver_template = DMX3191D;
#include "scsi_module.c"

