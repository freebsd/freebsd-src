/* $Id: tpam_main.c,v 1.1.2.2 2001/12/09 18:45:14 kai Exp $
 *
 * Turbo PAM ISDN driver for Linux. (Kernel Driver - main routines)
 *
 * Copyright 2001 Stelian Pop <stelian.pop@fr.alcove.com>, Alcôve
 *
 * This software may be used and distributed according to the terms
 * of the GNU General Public License, incorporated herein by reference.
 *
 * For all support questions please contact: <support@auvertech.fr>
 *
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/sched.h>
#include <linux/tqueue.h>
#include <linux/interrupt.h>
#include <linux/init.h>
#include <asm/io.h>

#include "tpam.h"

/* Local functions prototypes */
static int __devinit tpam_probe(struct pci_dev *, const struct pci_device_id *);
static void __devexit tpam_unregister_card(tpam_card *);
static void __devexit tpam_remove(struct pci_dev *);
static int __init tpam_init(void);
static void __exit tpam_exit(void);

/* List of boards */
static tpam_card *cards; /* = NULL; */
/* Number of cards */
static int cards_num;
/* Configurable id of the driver */
static char *id = "tpam\0\0\0\0\0\0\0\0\0\0\0\0";

MODULE_DESCRIPTION("ISDN4Linux: Driver for TurboPAM ISDN cards");
MODULE_AUTHOR("Stelian Pop");
MODULE_LICENSE("GPL");
MODULE_PARM_DESC(id,"ID-String of the driver");
MODULE_PARM(id,"s");

/*
 * Finds a board by its driver ID.
 *
 * 	driverId: driver ID (as referenced by the IDSN link layer)
 *
 * Return: the tpam_card structure if found, NULL on error.
 */
tpam_card *tpam_findcard(int driverid) {
	tpam_card *p = cards;

	while (p) {
		if (p->id == driverid)
			return p;
		p = p->next;
	}
	return NULL;
}

/*
 * Finds a channel number by its ncoid.
 *
 * 	card: the board
 * 	ncoid: the NCO id
 *
 * Return: the channel number if found, TPAM_CHANNEL_INVALID if not.
 */
u32 tpam_findchannel(tpam_card *card, u32 ncoid) {
	int i;

	for (i = 0; i < TPAM_NBCHANNEL; ++i)
		if (card->channels[i].ncoid == ncoid)
			return card->channels[i].num;
	return TPAM_CHANNEL_INVALID;
}

/*
 * Initializes and registers a new TurboPAM card.
 *
 * 	dev: the PCI device
 * 	num: the board number
 *
 * Return: 0 if OK, <0 if error
 */
static int __devinit tpam_probe(struct pci_dev *dev, const struct pci_device_id *pci_id) {
	tpam_card *card, *c;
	int i;

	/* allocate memory for the board structure */
	if (!(card = (tpam_card *)kmalloc(sizeof(tpam_card), GFP_KERNEL))) {
		printk(KERN_ERR "TurboPAM: tpam_register_card: "
		       "kmalloc failed!\n");
		return -ENOMEM;
	}

	memset((char *)card, 0, sizeof(tpam_card));

	card->irq = dev->irq;
	card->lock = SPIN_LOCK_UNLOCKED;
	sprintf(card->interface.id, "%s%d", id, cards_num);

	/* request interrupt */
	if (request_irq(card->irq, &tpam_irq, SA_INTERRUPT | SA_SHIRQ, 
			card->interface.id, card)) {
		printk(KERN_ERR "TurboPAM: tpam_register_card: "
		       "could not request irq %d\n", card->irq);
		kfree(card);
		return -EIO;
	}

	/* remap board memory */
	if (!(card->bar0 = (unsigned long) ioremap(pci_resource_start(dev, 0),
						   0x800000))) {
		printk(KERN_ERR "TurboPAM: tpam_register_card: "
		       "unable to remap bar0\n");
		free_irq(card->irq, card);
		kfree(card);
		return -EIO;
	}

	/* reset the board */
	readl(card->bar0 + TPAM_RESETPAM_REGISTER);

	/* initialisation magic :-( */
	copy_to_pam_dword(card, (void *)0x01800008, 0x00000030);
	copy_to_pam_dword(card, (void *)0x01800010, 0x00000030);
	copy_to_pam_dword(card, (void *)0x01800014, 0x42240822);
	copy_to_pam_dword(card, (void *)0x01800018, 0x07114000);
	copy_to_pam_dword(card, (void *)0x0180001c, 0x00000400);
	copy_to_pam_dword(card, (void *)0x01840070, 0x00000010);

	/* fill the ISDN link layer structure */
	card->interface.channels = TPAM_NBCHANNEL;
	card->interface.maxbufsize = TPAM_MAXBUFSIZE;
	card->interface.features = 
		ISDN_FEATURE_P_EURO |
		ISDN_FEATURE_L2_HDLC |
		ISDN_FEATURE_L2_MODEM |
		ISDN_FEATURE_L3_TRANS;
	card->interface.hl_hdrlen = 0;
	card->interface.command = tpam_command;
	card->interface.writebuf_skb = tpam_writebuf_skb;
	card->interface.writecmd = NULL;
	card->interface.readstat = NULL;

	/* register wrt the ISDN link layer */
	if (!register_isdn(&card->interface)) {
		printk(KERN_ERR "TurboPAM: tpam_register_card: "
		       "unable to register %s\n", card->interface.id);
		free_irq(card->irq, card);
		iounmap((void *)card->bar0);
		kfree(card);
		return -EIO;
	}
	card->id = card->interface.channels;

	/* initialize all channels */
	for (i = 0; i < TPAM_NBCHANNEL; ++i) {
		card->channels[i].num = i;
		card->channels[i].card = card;
		card->channels[i].ncoid = TPAM_NCOID_INVALID;
		card->channels[i].hdlc = 0;
		card->channels[i].realhdlc = 0;
		card->channels[i].hdlcshift = 0;
		skb_queue_head_init(&card->channels[i].sendq);
	}

	/* initialize the rest of board structure */
	card->channels_used = 0;
	card->channels_tested = 0;
	card->running = 0;
	card->busy = 0;
	card->roundrobin = 0;
	card->loopmode = 0;
	skb_queue_head_init(&card->sendq);
	skb_queue_head_init(&card->recvq);
	card->recv_tq.routine = (void *) (void *) tpam_recv_tq;
	card->recv_tq.data = card;
	card->send_tq.routine = (void *) (void *) tpam_send_tq;
	card->send_tq.data = card;

	/* add the board at the end of the list of boards */
	card->next = NULL;
	if (cards) {
		c = cards;
		while (c->next)
			c = c->next;
		c->next = card;
	}
	else
		cards = card;

	++cards_num;
	pci_set_drvdata(dev, card);

	return 0;
}

/*
 * Unregisters a TurboPAM board by releasing all its ressources (irq, mem etc).
 *
 * 	card: the board.
 */
static void __devexit tpam_unregister_card(tpam_card *card) {
	isdn_ctrl cmd;

	/* prevent the ISDN link layer that the driver will be unloaded */
	cmd.command = ISDN_STAT_UNLOAD;
	cmd.driver = card->id;
	(* card->interface.statcallb)(&cmd);

	/* release interrupt */
	free_irq(card->irq, card);

	/* release mapped memory */
	iounmap((void *)card->bar0);
}

/*
 * Stops the driver.
 */
static void __devexit tpam_remove(struct pci_dev *pcidev) {
	tpam_card *card = pci_get_drvdata(pcidev);
	tpam_card *c;

	/* remove from the list of cards */
	if (card == cards)
		cards = cards->next;
	else {
		c = cards;
		while (c->next != card) 
			c = c->next;
		c->next = c->next->next;
	}
	
	/* unregister each board */
	tpam_unregister_card(card);
	
	/* and free the board structure itself */
	kfree(card);
}

static struct pci_device_id tpam_pci_tbl[] __devinitdata = {
	{ PCI_VENDOR_ID_XILINX, PCI_DEVICE_ID_TURBOPAM,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ }
};

MODULE_DEVICE_TABLE(pci, tpam_pci_tbl);

static struct pci_driver tpam_driver = {
	name:		"tpam",
	id_table:	tpam_pci_tbl,
	probe:		tpam_probe,
	remove:		__devexit_p(tpam_remove),
};

static int __init tpam_init(void) {
	int ret;
	
	ret = pci_module_init(&tpam_driver);
	if (ret)
		return ret;
	printk(KERN_INFO "TurboPAM: %d card%s found, driver loaded.\n", 
	       cards_num, (cards_num > 1) ? "s" : "");
	return 0;
}

static void __exit tpam_exit(void) {
	pci_unregister_driver(&tpam_driver);
	printk(KERN_INFO "TurboPAM: driver unloaded\n");
}

/* Module entry points */
module_init(tpam_init);
module_exit(tpam_exit);

