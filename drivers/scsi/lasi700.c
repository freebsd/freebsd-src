/* -*- mode: c; c-basic-offset: 8 -*- */

/* PARISC LASI driver for the 53c700 chip
 *
 * Copyright (C) 2001 by James.Bottomley@HansenPartnership.com
**-----------------------------------------------------------------------------
**  
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
**-----------------------------------------------------------------------------
 */

/*
 * Many thanks to Richard Hirst <rhirst@linuxcare.com> for patiently
 * debugging this driver on the parisc architecture and suggesting
 * many improvements and bug fixes.
 *
 * Thanks also go to Linuxcare Inc. for providing several PARISC
 * machines for me to debug the driver on.
 */

#ifndef __hppa__
#error "lasi700 only compiles on hppa architecture"
#endif

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/stat.h>
#include <linux/mm.h>
#include <linux/blk.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/config.h>
#include <linux/ioport.h>

#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/irq.h>
#include <asm/hardware.h>
#include <asm/delay.h>
#include <asm/gsc.h>

#include <linux/module.h>

#include "scsi.h"
#include "hosts.h"
#include "constants.h"

#include "lasi700.h"
#include "53c700.h"

#ifdef MODULE

char *lasi700;			/* command line from insmod */

MODULE_AUTHOR("James Bottomley");
MODULE_DESCRIPTION("lasi700 SCSI Driver");
MODULE_LICENSE("GPL");
MODULE_PARM(lasi700, "s");

#endif

#ifdef MODULE
#define ARG_SEP ' '
#else
#define ARG_SEP ','
#endif

static unsigned long __initdata opt_base;
static int __initdata opt_irq;

static int __init
param_setup(char *string)
{
	char *pos = string, *next;

	while(pos != NULL && (next = strchr(pos, ':')) != NULL) {
		int val = (int)simple_strtoul(++next, NULL, 0);
		
		if(!strncmp(pos, "addr:", 5))
			opt_base = val;
		else if(!strncmp(pos, "irq:", 4))
			opt_irq = val;

		if((pos = strchr(pos, ARG_SEP)) != NULL)
			pos++;
	}
	return 1;
}

#ifndef MODULE
__setup("lasi700=", param_setup);
#endif

static Scsi_Host_Template __initdata *host_tpnt = NULL;
static int __initdata host_count = 0;
static struct parisc_device_id lasi700_scsi_tbl[] = {
	LASI700_ID_TABLE,
	LASI710_ID_TABLE,
	{ 0 }
};

MODULE_DEVICE_TABLE(parisc, lasi700_scsi_tbl);

static struct parisc_driver lasi700_driver = LASI700_DRIVER;

static int __init
lasi700_detect(Scsi_Host_Template *tpnt)
{
	host_tpnt = tpnt;

#ifdef MODULE
	if(lasi700)
		param_setup(lasi700);
#endif

	register_parisc_driver(&lasi700_driver);

	return (host_count != 0);
}

static int __init
lasi700_driver_callback(struct parisc_device *dev)
{
	unsigned long base = dev->hpa + LASI_SCSI_CORE_OFFSET;
	char *driver_name;
	struct Scsi_Host *host;
	struct NCR_700_Host_Parameters *hostdata =
		kmalloc(sizeof(struct NCR_700_Host_Parameters),
			GFP_KERNEL);
	if(dev->id.sversion == LASI_700_SVERSION) {
		driver_name = "lasi700";
	} else {
		driver_name = "lasi710";
	}
	if(hostdata == NULL) {
		printk(KERN_ERR "%s: Failed to allocate host data\n",
		       driver_name);
		return 1;
	}
	memset(hostdata, 0, sizeof(struct NCR_700_Host_Parameters));
	hostdata->base = base;
	hostdata->differential = 0;
	if(dev->id.sversion == LASI_700_SVERSION) {
		hostdata->clock = LASI700_CLOCK;
		hostdata->force_le_on_be = 1;
	} else {
		hostdata->clock = LASI710_CLOCK;
		hostdata->force_le_on_be = 0;
		hostdata->chip710 = 1;
		hostdata->dmode_extra = DMODE_FC2;
	}
	hostdata->pci_dev = ccio_get_fake(dev);
	if((host = NCR_700_detect(host_tpnt, hostdata)) == NULL) {
		kfree(hostdata);
		return 1;
	}
	host->irq = dev->irq;
	if(request_irq(dev->irq, NCR_700_intr, SA_SHIRQ, driver_name, host)) {
		printk(KERN_ERR "%s: irq problem, detaching\n",
		       driver_name);
		scsi_unregister(host);
		NCR_700_release(host);
		return 1;
	}
	host_count++;
	return 0;
}

static int
lasi700_release(struct Scsi_Host *host)
{
	struct D700_Host_Parameters *hostdata = 
		(struct D700_Host_Parameters *)host->hostdata[0];

	NCR_700_release(host);
	kfree(hostdata);
	free_irq(host->irq, host);
	unregister_parisc_driver(&lasi700_driver);
	return 1;
}

static Scsi_Host_Template driver_template = LASI700_SCSI;

#include "scsi_module.c"
