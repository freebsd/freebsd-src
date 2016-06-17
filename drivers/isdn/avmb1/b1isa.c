/* $Id: b1isa.c,v 1.1.4.1 2001/11/20 14:19:34 kai Exp $
 * 
 * Module for AVM B1 ISA-card.
 * 
 * Copyright 1999 by Carsten Paeth <calle@calle.de>
 * 
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/capi.h>
#include <linux/init.h>
#include <asm/io.h>
#include "capicmd.h"
#include "capiutil.h"
#include "capilli.h"
#include "avmcard.h"

static char *revision = "$Revision: 1.1.4.1 $";

/* ------------------------------------------------------------- */

MODULE_DESCRIPTION("CAPI4Linux: Driver for AVM B1 ISA card");
MODULE_AUTHOR("Carsten Paeth");
MODULE_LICENSE("GPL");

/* ------------------------------------------------------------- */

static void b1isa_interrupt(int interrupt, void *devptr, struct pt_regs *regs)
{
	avmcard *card;

	card = (avmcard *) devptr;

	if (!card) {
		printk(KERN_WARNING "b1_interrupt: wrong device\n");
		return;
	}
	if (card->interrupt) {
		printk(KERN_ERR "b1_interrupt: reentering interrupt hander (%s)\n", card->name);
		return;
	}

	card->interrupt = 1;

	b1_handle_interrupt(card);

	card->interrupt = 0;
}
/* ------------------------------------------------------------- */

static struct capi_driver_interface *di;

/* ------------------------------------------------------------- */

static void b1isa_remove_ctr(struct capi_ctr *ctrl)
{
	avmctrl_info *cinfo = (avmctrl_info *)(ctrl->driverdata);
	avmcard *card = cinfo->card;
	unsigned int port = card->port;

	b1_reset(port);
	b1_reset(port);

	di->detach_ctr(ctrl);
	free_irq(card->irq, card);
	release_region(card->port, AVMB1_PORTLEN);
	kfree(card->ctrlinfo);
	kfree(card);

	MOD_DEC_USE_COUNT;
}

/* ------------------------------------------------------------- */

static int b1isa_add_card(struct capi_driver *driver, struct capicardparams *p)
{
	avmctrl_info *cinfo;
	avmcard *card;
	int retval;

	MOD_INC_USE_COUNT;

	card = (avmcard *) kmalloc(sizeof(avmcard), GFP_ATOMIC);

	if (!card) {
		printk(KERN_WARNING "b1isa: no memory.\n");
	        MOD_DEC_USE_COUNT;
		return -ENOMEM;
	}
	memset(card, 0, sizeof(avmcard));
        cinfo = (avmctrl_info *) kmalloc(sizeof(avmctrl_info), GFP_ATOMIC);
	if (!cinfo) {
		printk(KERN_WARNING "b1isa: no memory.\n");
		kfree(card);
	        MOD_DEC_USE_COUNT;
		return -ENOMEM;
	}
	memset(cinfo, 0, sizeof(avmctrl_info));
	card->ctrlinfo = cinfo;
	cinfo->card = card;
	sprintf(card->name, "b1isa-%x", p->port);
	card->port = p->port;
	card->irq = p->irq;
	card->cardtype = avm_b1isa;

	if (check_region(card->port, AVMB1_PORTLEN)) {
		printk(KERN_WARNING
		       "b1isa: ports 0x%03x-0x%03x in use.\n",
		       card->port, card->port + AVMB1_PORTLEN);
	        kfree(card->ctrlinfo);
		kfree(card);
	        MOD_DEC_USE_COUNT;
		return -EBUSY;
	}
	if (b1_irq_table[card->irq & 0xf] == 0) {
		printk(KERN_WARNING "b1isa: irq %d not valid.\n", card->irq);
	        kfree(card->ctrlinfo);
		kfree(card);
	        MOD_DEC_USE_COUNT;
		return -EINVAL;
	}
	if (   card->port != 0x150 && card->port != 0x250
	    && card->port != 0x300 && card->port != 0x340) {
		printk(KERN_WARNING "b1isa: illegal port 0x%x.\n", card->port);
	        kfree(card->ctrlinfo);
		kfree(card);
	        MOD_DEC_USE_COUNT;
		return -EINVAL;
	}
	b1_reset(card->port);
	if ((retval = b1_detect(card->port, card->cardtype)) != 0) {
		printk(KERN_NOTICE "b1isa: NO card at 0x%x (%d)\n",
					card->port, retval);
	        kfree(card->ctrlinfo);
		kfree(card);
	        MOD_DEC_USE_COUNT;
		return -EIO;
	}
	b1_reset(card->port);
	b1_getrevision(card);

	request_region(p->port, AVMB1_PORTLEN, card->name);

	retval = request_irq(card->irq, b1isa_interrupt, 0, card->name, card);
	if (retval) {
		printk(KERN_ERR "b1isa: unable to get IRQ %d.\n", card->irq);
		release_region(card->port, AVMB1_PORTLEN);
	        kfree(card->ctrlinfo);
		kfree(card);
	        MOD_DEC_USE_COUNT;
		return -EBUSY;
	}

	cinfo->capi_ctrl = di->attach_ctr(driver, card->name, cinfo);
	if (!cinfo->capi_ctrl) {
		printk(KERN_ERR "b1isa: attach controller failed.\n");
		free_irq(card->irq, card);
		release_region(card->port, AVMB1_PORTLEN);
	        kfree(card->ctrlinfo);
		kfree(card);
	        MOD_DEC_USE_COUNT;
		return -EBUSY;
	}

	printk(KERN_INFO
		"%s: AVM B1 ISA at i/o %#x, irq %d, revision %d\n",
		driver->name, card->port, card->irq, card->revision);

	return 0;
}

static char *b1isa_procinfo(struct capi_ctr *ctrl)
{
	avmctrl_info *cinfo = (avmctrl_info *)(ctrl->driverdata);

	if (!cinfo)
		return "";
	sprintf(cinfo->infobuf, "%s %s 0x%x %d r%d",
		cinfo->cardname[0] ? cinfo->cardname : "-",
		cinfo->version[VER_DRIVER] ? cinfo->version[VER_DRIVER] : "-",
		cinfo->card ? cinfo->card->port : 0x0,
		cinfo->card ? cinfo->card->irq : 0,
		cinfo->card ? cinfo->card->revision : 0
		);
	return cinfo->infobuf;
}

/* ------------------------------------------------------------- */

static struct capi_driver b1isa_driver = {
    name: "b1isa",
    revision: "0.0",
    load_firmware: b1_load_firmware,
    reset_ctr: b1_reset_ctr,
    remove_ctr: b1isa_remove_ctr,
    register_appl: b1_register_appl,
    release_appl: b1_release_appl,
    send_message: b1_send_message,

    procinfo: b1isa_procinfo,
    ctr_read_proc: b1ctl_read_proc,
    driver_read_proc: 0,	/* use standard driver_read_proc */

    add_card: b1isa_add_card,
};

static int __init b1isa_init(void)
{
	struct capi_driver *driver = &b1isa_driver;
	char *p;
	int retval = 0;

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
		retval = -EIO;
	}
	MOD_DEC_USE_COUNT;
	return retval;
}

static void __exit b1isa_exit(void)
{
    detach_capi_driver(&b1isa_driver);
}

module_init(b1isa_init);
module_exit(b1isa_exit);
