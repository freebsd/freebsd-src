/* $Id: t1pci.c,v 1.1.4.1 2001/11/20 14:19:34 kai Exp $
 * 
 * Module for AVM T1 PCI-card.
 * 
 * Copyright 1999 by Carsten Paeth <calle@calle.de>
 * 
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/pci.h>
#include <linux/capi.h>
#include <linux/init.h>
#include <asm/io.h>
#include "capicmd.h"
#include "capiutil.h"
#include "capilli.h"
#include "avmcard.h"

static char *revision = "$Revision: 1.1.4.1 $";

#undef CONFIG_T1PCI_DEBUG
#undef CONFIG_T1PCI_POLLDEBUG

/* ------------------------------------------------------------- */

static struct pci_device_id t1pci_pci_tbl[] __initdata = {
	{ PCI_VENDOR_ID_AVM, PCI_DEVICE_ID_AVM_T1, PCI_ANY_ID, PCI_ANY_ID },
	{ }				/* Terminating entry */
};

MODULE_DEVICE_TABLE(pci, t1pci_pci_tbl);
MODULE_DESCRIPTION("CAPI4Linux: Driver for AVM T1 PCI card");
MODULE_AUTHOR("Carsten Paeth");
MODULE_LICENSE("GPL");

/* ------------------------------------------------------------- */

static struct capi_driver_interface *di;

/* ------------------------------------------------------------- */

static void t1pci_remove_ctr(struct capi_ctr *ctrl)
{
	avmctrl_info *cinfo = (avmctrl_info *)(ctrl->driverdata);
	avmcard *card = cinfo->card;

 	b1dma_reset(card);

	di->detach_ctr(ctrl);
	free_irq(card->irq, card);
	iounmap(card->mbase);
	release_region(card->port, AVMB1_PORTLEN);
	ctrl->driverdata = 0;
	kfree(card->ctrlinfo);
	kfree(card->dma);
	kfree(card);

	MOD_DEC_USE_COUNT;
}

/* ------------------------------------------------------------- */

static int t1pci_add_card(struct capi_driver *driver, struct capicardparams *p)
{
	avmcard *card;
	avmctrl_info *cinfo;
	int retval;

	MOD_INC_USE_COUNT;

	card = (avmcard *) kmalloc(sizeof(avmcard), GFP_ATOMIC);

	if (!card) {
		printk(KERN_WARNING "%s: no memory.\n", driver->name);
	        MOD_DEC_USE_COUNT;
		return -ENOMEM;
	}
	memset(card, 0, sizeof(avmcard));
	card->dma = (avmcard_dmainfo *) kmalloc(sizeof(avmcard_dmainfo), GFP_ATOMIC);
	if (!card->dma) {
		printk(KERN_WARNING "%s: no memory.\n", driver->name);
		kfree(card);
	        MOD_DEC_USE_COUNT;
		return -ENOMEM;
	}
	memset(card->dma, 0, sizeof(avmcard_dmainfo));
        cinfo = (avmctrl_info *) kmalloc(sizeof(avmctrl_info), GFP_ATOMIC);
	if (!cinfo) {
		printk(KERN_WARNING "%s: no memory.\n", driver->name);
		kfree(card->dma);
		kfree(card);
	        MOD_DEC_USE_COUNT;
		return -ENOMEM;
	}
	memset(cinfo, 0, sizeof(avmctrl_info));
	card->ctrlinfo = cinfo;
	cinfo->card = card;
	sprintf(card->name, "t1pci-%x", p->port);
	card->port = p->port;
	card->irq = p->irq;
	card->membase = p->membase;
	card->cardtype = avm_t1pci;

	if (check_region(card->port, AVMB1_PORTLEN)) {
		printk(KERN_WARNING
		       "%s: ports 0x%03x-0x%03x in use.\n",
		       driver->name, card->port, card->port + AVMB1_PORTLEN);
	        kfree(card->ctrlinfo);
		kfree(card->dma);
		kfree(card);
	        MOD_DEC_USE_COUNT;
		return -EBUSY;
	}

	card->mbase = ioremap_nocache(card->membase, 64);
	if (!card->mbase) {
		printk(KERN_NOTICE "%s: can't remap memory at 0x%lx\n",
					driver->name, card->membase);
	        kfree(card->ctrlinfo);
		kfree(card->dma);
		kfree(card);
	        MOD_DEC_USE_COUNT;
		return -EIO;
	}

	b1dma_reset(card);

	if ((retval = t1pci_detect(card)) != 0) {
		if (retval < 6)
			printk(KERN_NOTICE "%s: NO card at 0x%x (%d)\n",
					driver->name, card->port, retval);
		else
			printk(KERN_NOTICE "%s: card at 0x%x, but cabel not connected or T1 has no power (%d)\n",
					driver->name, card->port, retval);
                iounmap(card->mbase);
	        kfree(card->ctrlinfo);
		kfree(card->dma);
		kfree(card);
	        MOD_DEC_USE_COUNT;
		return -EIO;
	}
	b1dma_reset(card);

	request_region(p->port, AVMB1_PORTLEN, card->name);

	retval = request_irq(card->irq, b1dma_interrupt, SA_SHIRQ, card->name, card);
	if (retval) {
		printk(KERN_ERR "%s: unable to get IRQ %d.\n",
				driver->name, card->irq);
                iounmap(card->mbase);
		release_region(card->port, AVMB1_PORTLEN);
	        kfree(card->ctrlinfo);
		kfree(card->dma);
		kfree(card);
	        MOD_DEC_USE_COUNT;
		return -EBUSY;
	}

	cinfo->capi_ctrl = di->attach_ctr(driver, card->name, cinfo);
	if (!cinfo->capi_ctrl) {
		printk(KERN_ERR "%s: attach controller failed.\n", driver->name);
                iounmap(card->mbase);
		free_irq(card->irq, card);
		release_region(card->port, AVMB1_PORTLEN);
	        kfree(card->ctrlinfo);
		kfree(card->dma);
		kfree(card);
	        MOD_DEC_USE_COUNT;
		return -EBUSY;
	}
	card->cardnr = cinfo->capi_ctrl->cnr;

	skb_queue_head_init(&card->dma->send_queue);

	printk(KERN_INFO
		"%s: AVM T1 PCI at i/o %#x, irq %d, mem %#lx\n",
		driver->name, card->port, card->irq, card->membase);

	return 0;
}

/* ------------------------------------------------------------- */

static char *t1pci_procinfo(struct capi_ctr *ctrl)
{
	avmctrl_info *cinfo = (avmctrl_info *)(ctrl->driverdata);

	if (!cinfo)
		return "";
	sprintf(cinfo->infobuf, "%s %s 0x%x %d 0x%lx",
		cinfo->cardname[0] ? cinfo->cardname : "-",
		cinfo->version[VER_DRIVER] ? cinfo->version[VER_DRIVER] : "-",
		cinfo->card ? cinfo->card->port : 0x0,
		cinfo->card ? cinfo->card->irq : 0,
		cinfo->card ? cinfo->card->membase : 0
		);
	return cinfo->infobuf;
}

/* ------------------------------------------------------------- */

static struct capi_driver t1pci_driver = {
    name: "t1pci",
    revision: "0.0",
    load_firmware: b1dma_load_firmware,
    reset_ctr: b1dma_reset_ctr,
    remove_ctr: t1pci_remove_ctr,
    register_appl: b1dma_register_appl,
    release_appl: b1dma_release_appl,
    send_message: b1dma_send_message,

    procinfo: t1pci_procinfo,
    ctr_read_proc: b1dmactl_read_proc,
    driver_read_proc: 0,	/* use standard driver_read_proc */

    add_card: 0, /* no add_card function */
};

static int ncards = 0;

static int __init t1pci_init(void)
{
	struct capi_driver *driver = &t1pci_driver;
	struct pci_dev *dev = NULL;
	char *p;
	int retval;

	MOD_INC_USE_COUNT;

	if ((p = strchr(revision, ':')) != 0 && p[1]) {
		strncpy(driver->revision, p + 2, sizeof(driver->revision));
		driver->revision[sizeof(driver->revision)-1] = 0;
		if ((p = strchr(driver->revision, '$')) != 0 && p > driver->revision)
			*(p-1) = 0;
	}

	printk(KERN_INFO "%s: revision %s\n", driver->name, driver->revision);

        di = attach_capi_driver(driver);
	if (!di) {
		printk(KERN_ERR "%s: failed to attach capi_driver\n",
				driver->name);
		MOD_DEC_USE_COUNT;
		return -EIO;
	}

	while ((dev = pci_find_device(PCI_VENDOR_ID_AVM, PCI_DEVICE_ID_AVM_T1, dev))) {
		struct capicardparams param;

		if (pci_enable_device(dev) < 0) {
		        printk(KERN_ERR	"%s: failed to enable AVM-T1-PCI\n",
			       driver->name);
			continue;
		}
		pci_set_master(dev);

		param.port = pci_resource_start(dev, 1);
 		param.irq = dev->irq;
		param.membase = pci_resource_start(dev, 0);

		printk(KERN_INFO
			"%s: PCI BIOS reports AVM-T1-PCI at i/o %#x, irq %d, mem %#x\n",
			driver->name, param.port, param.irq, param.membase);
		retval = t1pci_add_card(driver, &param);
		if (retval != 0) {
		        printk(KERN_ERR
			"%s: no AVM-T1-PCI at i/o %#x, irq %d detected, mem %#x\n",
			driver->name, param.port, param.irq, param.membase);
			continue;
		}
		ncards++;
	}
	if (ncards) {
		printk(KERN_INFO "%s: %d T1-PCI card(s) detected\n",
				driver->name, ncards);
		MOD_DEC_USE_COUNT;
		return 0;
	}
	printk(KERN_ERR "%s: NO T1-PCI card detected\n", driver->name);
	detach_capi_driver(&t1pci_driver);
	MOD_DEC_USE_COUNT;
	return -ENODEV;
}

static void __exit t1pci_exit(void)
{
    detach_capi_driver(&t1pci_driver);
}

module_init(t1pci_init);
module_exit(t1pci_exit);
