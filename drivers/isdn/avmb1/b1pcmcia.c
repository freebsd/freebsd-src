/* $Id: b1pcmcia.c,v 1.1.4.1 2001/11/20 14:19:34 kai Exp $
 * 
 * Module for AVM B1/M1/M2 PCMCIA-card.
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
#include <linux/init.h>
#include <asm/io.h>
#include <linux/capi.h>
#include <linux/b1pcmcia.h>
#include "capicmd.h"
#include "capiutil.h"
#include "capilli.h"
#include "avmcard.h"

static char *revision = "$Revision: 1.1.4.1 $";

/* ------------------------------------------------------------- */

MODULE_DESCRIPTION("CAPI4Linux: Driver for AVM PCMCIA cards");
MODULE_AUTHOR("Carsten Paeth");
MODULE_LICENSE("GPL");

/* ------------------------------------------------------------- */

static struct capi_driver_interface *di;

/* ------------------------------------------------------------- */

static void b1pcmcia_interrupt(int interrupt, void *devptr, struct pt_regs *regs)
{
	avmcard *card;

	card = (avmcard *) devptr;

	if (!card) {
		printk(KERN_WARNING "b1pcmcia: interrupt: wrong device\n");
		return;
	}
	if (card->interrupt) {
		printk(KERN_ERR "%s: reentering interrupt hander.\n",
			card->name);
		return;
	}

	card->interrupt = 1;

	b1_handle_interrupt(card);

	card->interrupt = 0;
}
/* ------------------------------------------------------------- */

static void b1pcmcia_remove_ctr(struct capi_ctr *ctrl)
{
	avmctrl_info *cinfo = (avmctrl_info *)(ctrl->driverdata);
	avmcard *card = cinfo->card;
	unsigned int port = card->port;

	b1_reset(port);
	b1_reset(port);

	di->detach_ctr(ctrl);
	free_irq(card->irq, card);
	/* io addrsses managent by CardServices 
	 * release_region(card->port, AVMB1_PORTLEN);
	 */
	kfree(card);

	MOD_DEC_USE_COUNT;
}

/* ------------------------------------------------------------- */

static int b1pcmcia_add_card(struct capi_driver *driver,
				unsigned int port,
				unsigned irq,
				enum avmcardtype cardtype)
{
	avmctrl_info *cinfo;
	avmcard *card;
	char *cardname;
	int retval;

	MOD_INC_USE_COUNT;

	card = (avmcard *) kmalloc(sizeof(avmcard), GFP_ATOMIC);

	if (!card) {
		printk(KERN_WARNING "%s: no memory.\n", driver->name);
	        MOD_DEC_USE_COUNT;
		return -ENOMEM;
	}
	memset(card, 0, sizeof(avmcard));
        cinfo = (avmctrl_info *) kmalloc(sizeof(avmctrl_info), GFP_ATOMIC);
	if (!cinfo) {
		printk(KERN_WARNING "%s: no memory.\n", driver->name);
		kfree(card);
	        MOD_DEC_USE_COUNT;
		return -ENOMEM;
	}
	memset(cinfo, 0, sizeof(avmctrl_info));
	card->ctrlinfo = cinfo;
	cinfo->card = card;
	switch (cardtype) {
		case avm_m1: sprintf(card->name, "m1-%x", port); break;
		case avm_m2: sprintf(card->name, "m2-%x", port); break;
		default: sprintf(card->name, "b1pcmcia-%x", port); break;
	}
	card->port = port;
	card->irq = irq;
	card->cardtype = cardtype;

	b1_reset(card->port);
	if ((retval = b1_detect(card->port, card->cardtype)) != 0) {
		printk(KERN_NOTICE "%s: NO card at 0x%x (%d)\n",
					driver->name, card->port, retval);
	        kfree(card->ctrlinfo);
		kfree(card);
	        MOD_DEC_USE_COUNT;
		return -EIO;
	}
	b1_reset(card->port);
	b1_getrevision(card);

	retval = request_irq(card->irq, b1pcmcia_interrupt, 0, card->name, card);
	if (retval) {
		printk(KERN_ERR "%s: unable to get IRQ %d.\n",
				driver->name, card->irq);
	        kfree(card->ctrlinfo);
		kfree(card);
	        MOD_DEC_USE_COUNT;
		return -EBUSY;
	}

	cinfo->capi_ctrl = di->attach_ctr(driver, card->name, cinfo);
	if (!cinfo->capi_ctrl) {
		printk(KERN_ERR "%s: attach controller failed.\n",
				driver->name);
		free_irq(card->irq, card);
	        kfree(card->ctrlinfo);
		kfree(card);
	        MOD_DEC_USE_COUNT;
		return -EBUSY;
	}
	switch (cardtype) {
		case avm_m1: cardname = "M1"; break;
		case avm_m2: cardname = "M2"; break;
		default    : cardname = "B1 PCMCIA"; break;
	}

	printk(KERN_INFO
		"%s: AVM %s at i/o %#x, irq %d, revision %d\n",
		driver->name, cardname, card->port, card->irq, card->revision);

	return cinfo->capi_ctrl->cnr;
}

/* ------------------------------------------------------------- */

static char *b1pcmcia_procinfo(struct capi_ctr *ctrl)
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

static struct capi_driver b1pcmcia_driver = {
    name: "b1pcmcia",
    revision: "0.0",
    load_firmware: b1_load_firmware,
    reset_ctr: b1_reset_ctr,
    remove_ctr: b1pcmcia_remove_ctr,
    register_appl: b1_register_appl,
    release_appl: b1_release_appl,
    send_message: b1_send_message,

    procinfo: b1pcmcia_procinfo,
    ctr_read_proc: b1ctl_read_proc,
    driver_read_proc: 0,	/* use standard driver_read_proc */

    add_card: 0,
};

/* ------------------------------------------------------------- */

int b1pcmcia_addcard_b1(unsigned int port, unsigned irq)
{
	return b1pcmcia_add_card(&b1pcmcia_driver, port, irq, avm_b1pcmcia);
}

int b1pcmcia_addcard_m1(unsigned int port, unsigned irq)
{
	return b1pcmcia_add_card(&b1pcmcia_driver, port, irq, avm_m1);
}

int b1pcmcia_addcard_m2(unsigned int port, unsigned irq)
{
	return b1pcmcia_add_card(&b1pcmcia_driver, port, irq, avm_m2);
}

int b1pcmcia_delcard(unsigned int port, unsigned irq)
{
	struct capi_ctr *ctrl;
	avmcard *card;

	for (ctrl = b1pcmcia_driver.controller; ctrl; ctrl = ctrl->next) {
		card = ((avmctrl_info *)(ctrl->driverdata))->card;
		if (card->port == port && card->irq == irq) {
			b1pcmcia_remove_ctr(ctrl);
			return 0;
		}
	}
	return -ESRCH;
}

EXPORT_SYMBOL(b1pcmcia_addcard_b1);
EXPORT_SYMBOL(b1pcmcia_addcard_m1);
EXPORT_SYMBOL(b1pcmcia_addcard_m2);
EXPORT_SYMBOL(b1pcmcia_delcard);

/* ------------------------------------------------------------- */

static int __init b1pcmcia_init(void)
{
	struct capi_driver *driver = &b1pcmcia_driver;
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

static void __exit b1pcmcia_exit(void)
{
    detach_capi_driver(&b1pcmcia_driver);
}

module_init(b1pcmcia_init);
module_exit(b1pcmcia_exit);
