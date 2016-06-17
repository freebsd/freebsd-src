/*
 *  linux/drivers/acorn/net/etherh.c
 *
 *  Copyright (C) 2000-2002 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * NS8390 I-cubed EtherH and ANT EtherM specific driver
 * Thanks to I-Cubed for information on their cards.
 * EtherM conversion (C) 1999 Chris Kemp and Tim Watterton
 * EtherM integration (C) 2000 Aleph One Ltd (Tak-Shing Chan)
 * EtherM integration re-engineered by Russell King.
 *
 * Changelog:
 *  08-12-1996	RMK	1.00	Created
 *		RMK	1.03	Added support for EtherLan500 cards
 *  23-11-1997	RMK	1.04	Added media autodetection
 *  16-04-1998	RMK	1.05	Improved media autodetection
 *  10-02-2000	RMK	1.06	Updated for 2.3.43
 *  13-05-2000	RMK	1.07	Updated for 2.3.99-pre8
 *  12-10-1999  CK/TEW		EtherM driver first release
 *  21-12-2000	TTC		EtherH/EtherM integration
 *  25-12-2000	RMK	1.08	Clean integration of EtherM into this driver.
 *  03-01-2002	RMK	1.09	Always enable IRQs if we're in the nic slot.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/in.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/init.h>

#include <asm/system.h>
#include <asm/bitops.h>
#include <asm/ecard.h>
#include <asm/io.h>
#include <asm/irq.h>

#include "../../net/8390.h"

#define NET_DEBUG  0
#define DEBUG_INIT 2

static unsigned int net_debug = NET_DEBUG;

static const card_ids __init etherh_cids[] = {
	{ MANU_ANT, PROD_ANT_ETHERM      },
	{ MANU_I3,  PROD_I3_ETHERLAN500  },
	{ MANU_I3,  PROD_I3_ETHERLAN600  },
	{ MANU_I3,  PROD_I3_ETHERLAN600A },
	{ 0xffff,   0xffff }
};

struct etherh_priv {
	unsigned int	id;
	unsigned int	ctrl_port;
	unsigned int	ctrl;
};

MODULE_AUTHOR("Russell King");
MODULE_DESCRIPTION("EtherH/EtherM driver");
MODULE_LICENSE("GPL");

static char version[] __initdata =
	"EtherH/EtherM Driver (c) 2002 Russell King v1.09\n";

#define ETHERH500_DATAPORT	0x200	/* MEMC */
#define ETHERH500_NS8390	0x000	/* MEMC */
#define ETHERH500_CTRLPORT	0x200	/* IOC  */

#define ETHERH600_DATAPORT	16	/* MEMC */
#define ETHERH600_NS8390	0x200	/* MEMC */
#define ETHERH600_CTRLPORT	0x080	/* MEMC */

#define ETHERH_CP_IE		1
#define ETHERH_CP_IF		2
#define ETHERH_CP_HEARTBEAT	2

#define ETHERH_TX_START_PAGE	1
#define ETHERH_STOP_PAGE	127

/*
 * These came from CK/TEW
 */
#define ETHERM_DATAPORT		0x080	/* MEMC */
#define ETHERM_NS8390		0x200	/* MEMC */
#define ETHERM_CTRLPORT		0x08f	/* MEMC */

#define ETHERM_TX_START_PAGE	64
#define ETHERM_STOP_PAGE	127

/* ------------------------------------------------------------------------ */

static inline void etherh_set_ctrl(struct etherh_priv *eh, unsigned int mask)
{
	eh->ctrl |= mask;
	outb(eh->ctrl, eh->ctrl_port);
}

static inline void etherh_clr_ctrl(struct etherh_priv *eh, unsigned int mask)
{
	eh->ctrl &= ~mask;
	outb(eh->ctrl, eh->ctrl_port);
}

static inline unsigned int etherh_get_stat(struct etherh_priv *eh)
{
	return inb(eh->ctrl_port);
}




static void etherh_irq_enable(ecard_t *ec, int irqnr)
{
	struct etherh_priv *eh = ec->irq_data;

	etherh_set_ctrl(eh, ETHERH_CP_IE);
}

static void etherh_irq_disable(ecard_t *ec, int irqnr)
{
	struct etherh_priv *eh = ec->irq_data;

	etherh_clr_ctrl(eh, ETHERH_CP_IE);
}

static expansioncard_ops_t etherh_ops = {
	irqenable:	etherh_irq_enable,
	irqdisable:	etherh_irq_disable,
};




static void
etherh_setif(struct net_device *dev)
{
	struct ei_device *ei_local = (struct ei_device *) dev->priv;
	struct etherh_priv *eh = (struct etherh_priv *)dev->rmem_start;
	unsigned long addr, flags;

	save_flags_cli(flags);

	/* set the interface type */
	switch (eh->id) {
	case PROD_I3_ETHERLAN600:
	case PROD_I3_ETHERLAN600A:
		addr = dev->base_addr + EN0_RCNTHI;

		switch (dev->if_port) {
		case IF_PORT_10BASE2:
			outb((inb(addr) & 0xf8) | 1, addr);
			break;
		case IF_PORT_10BASET:
			outb((inb(addr) & 0xf8), addr);
			break;
		}
		break;

	case PROD_I3_ETHERLAN500:
		switch (dev->if_port) {
		case IF_PORT_10BASE2:
			etherh_clr_ctrl(eh, ETHERH_CP_IF);
			break;

		case IF_PORT_10BASET:
			etherh_set_ctrl(eh, ETHERH_CP_IF);
			break;
		}
		break;

	default:
		break;
	}

	restore_flags(flags);
}

static int
etherh_getifstat(struct net_device *dev)
{
	struct ei_device *ei_local = (struct ei_device *) dev->priv;
	struct etherh_priv *eh = (struct etherh_priv *)dev->rmem_start;
	int stat = 0;

	switch (eh->id) {
	case PROD_I3_ETHERLAN600:
	case PROD_I3_ETHERLAN600A:
		switch (dev->if_port) {
		case IF_PORT_10BASE2:
			stat = 1;
			break;
		case IF_PORT_10BASET:
			stat = inb(dev->base_addr+EN0_RCNTHI) & 4;
			break;
		}
		break;

	case PROD_I3_ETHERLAN500:
		switch (dev->if_port) {
		case IF_PORT_10BASE2:
			stat = 1;
			break;
		case IF_PORT_10BASET:
			stat = etherh_get_stat(eh) & ETHERH_CP_HEARTBEAT;
			break;
		}
		break;

	default:
		stat = 0;
		break;
	}

	return stat != 0;
}

/*
 * Configure the interface.  Note that we ignore the other
 * parts of ifmap, since its mostly meaningless for this driver.
 */
static int etherh_set_config(struct net_device *dev, struct ifmap *map)
{
	switch (map->port) {
	case IF_PORT_10BASE2:
	case IF_PORT_10BASET:
		/*
		 * If the user explicitly sets the interface
		 * media type, turn off automedia detection.
		 */
		dev->flags &= ~IFF_AUTOMEDIA;
		dev->if_port = map->port;
		break;

	default:
		return -EINVAL;
	}

	etherh_setif(dev);

	return 0;
}

/*
 * Reset the 8390 (hard reset).  Note that we can't actually do this.
 */
static void
etherh_reset(struct net_device *dev)
{
	struct ei_device *ei_local = (struct ei_device *) dev->priv;

	outb_p(E8390_NODMA+E8390_PAGE0+E8390_STOP, dev->base_addr);

	/*
	 * See if we need to change the interface type.
	 * Note that we use 'interface_num' as a flag
	 * to indicate that we need to change the media.
	 */
	if (dev->flags & IFF_AUTOMEDIA && ei_local->interface_num) {
		ei_local->interface_num = 0;

		if (dev->if_port == IF_PORT_10BASET)
			dev->if_port = IF_PORT_10BASE2;
		else
			dev->if_port = IF_PORT_10BASET;

		etherh_setif(dev);
	}
}

/*
 * Write a block of data out to the 8390
 */
static void
etherh_block_output (struct net_device *dev, int count, const unsigned char *buf, int start_page)
{
	struct ei_device *ei_local = (struct ei_device *) dev->priv;
	unsigned int addr, dma_addr;
	unsigned long dma_start;

	if (ei_local->dmaing) {
		printk(KERN_ERR "%s: DMAing conflict in etherh_block_input: "
			" DMAstat %d irqlock %d\n", dev->name,
			ei_local->dmaing, ei_local->irqlock);
		return;
	}

	/*
	 * Make sure we have a round number of bytes if we're in word mode.
	 */
	if (count & 1 && ei_local->word16)
		count++;

	ei_local->dmaing = 1;

	addr = dev->base_addr;
	dma_addr = dev->mem_start;

	count = (count + 1) & ~1;
	outb (E8390_NODMA | E8390_PAGE0 | E8390_START, addr + E8390_CMD);

	outb (0x42, addr + EN0_RCNTLO);
	outb (0x00, addr + EN0_RCNTHI);
	outb (0x42, addr + EN0_RSARLO);
	outb (0x00, addr + EN0_RSARHI);
	outb (E8390_RREAD | E8390_START, addr + E8390_CMD);

	udelay (1);

	outb (ENISR_RDC, addr + EN0_ISR);
	outb (count, addr + EN0_RCNTLO);
	outb (count >> 8, addr + EN0_RCNTHI);
	outb (0, addr + EN0_RSARLO);
	outb (start_page, addr + EN0_RSARHI);
	outb (E8390_RWRITE | E8390_START, addr + E8390_CMD);

	if (ei_local->word16)
		outsw (dma_addr, buf, count >> 1);
	else
		outsb (dma_addr, buf, count);

	dma_start = jiffies;

	while ((inb (addr + EN0_ISR) & ENISR_RDC) == 0)
		if (jiffies - dma_start > 2*HZ/100) { /* 20ms */
			printk(KERN_ERR "%s: timeout waiting for TX RDC\n",
				dev->name);
			etherh_reset (dev);
			NS8390_init (dev, 1);
			break;
		}

	outb (ENISR_RDC, addr + EN0_ISR);
	ei_local->dmaing = 0;
}

/*
 * Read a block of data from the 8390
 */
static void
etherh_block_input (struct net_device *dev, int count, struct sk_buff *skb, int ring_offset)
{
	struct ei_device *ei_local = (struct ei_device *) dev->priv;
	unsigned int addr, dma_addr;
	unsigned char *buf;

	if (ei_local->dmaing) {
		printk(KERN_ERR "%s: DMAing conflict in etherh_block_input: "
			" DMAstat %d irqlock %d\n", dev->name,
			ei_local->dmaing, ei_local->irqlock);
		return;
	}

	ei_local->dmaing = 1;

	addr = dev->base_addr;
	dma_addr = dev->mem_start;

	buf = skb->data;
	outb (E8390_NODMA | E8390_PAGE0 | E8390_START, addr + E8390_CMD);
	outb (count, addr + EN0_RCNTLO);
	outb (count >> 8, addr + EN0_RCNTHI);
	outb (ring_offset, addr + EN0_RSARLO);
	outb (ring_offset >> 8, addr + EN0_RSARHI);
	outb (E8390_RREAD | E8390_START, addr + E8390_CMD);

	if (ei_local->word16) {
		insw (dma_addr, buf, count >> 1);
		if (count & 1)
			buf[count - 1] = inb (dma_addr);
	} else
		insb (dma_addr, buf, count);

	outb (ENISR_RDC, addr + EN0_ISR);
	ei_local->dmaing = 0;
}

/*
 * Read a header from the 8390
 */
static void
etherh_get_header (struct net_device *dev, struct e8390_pkt_hdr *hdr, int ring_page)
{
	struct ei_device *ei_local = (struct ei_device *) dev->priv;
	unsigned int addr, dma_addr;

	if (ei_local->dmaing) {
		printk(KERN_ERR "%s: DMAing conflict in etherh_get_header: "
			" DMAstat %d irqlock %d\n", dev->name,
			ei_local->dmaing, ei_local->irqlock);
		return;
	}

	ei_local->dmaing = 1;

	addr = dev->base_addr;
	dma_addr = dev->mem_start;

	outb (E8390_NODMA | E8390_PAGE0 | E8390_START, addr + E8390_CMD);
	outb (sizeof (*hdr), addr + EN0_RCNTLO);
	outb (0, addr + EN0_RCNTHI);
	outb (0, addr + EN0_RSARLO);
	outb (ring_page, addr + EN0_RSARHI);
	outb (E8390_RREAD | E8390_START, addr + E8390_CMD);

	if (ei_local->word16)
		insw (dma_addr, hdr, sizeof (*hdr) >> 1);
	else
		insb (dma_addr, hdr, sizeof (*hdr));

	outb (ENISR_RDC, addr + EN0_ISR);
	ei_local->dmaing = 0;
}

/*
 * Open/initialize the board.  This is called (in the current kernel)
 * sometime after booting when the 'ifconfig' program is run.
 *
 * This routine should set everything up anew at each open, even
 * registers that "should" only need to be set once at boot, so that
 * there is non-reboot way to recover if something goes wrong.
 */
static int
etherh_open(struct net_device *dev)
{
	struct ei_device *ei_local = (struct ei_device *) dev->priv;

	if (!is_valid_ether_addr(dev->dev_addr)) {
		printk(KERN_WARNING "%s: invalid ethernet address\n",
			dev->name);
		return -EINVAL;
	}

	if (request_irq(dev->irq, ei_interrupt, 0, dev->name, dev))
		return -EAGAIN;

	/*
	 * Make sure that we aren't going to change the
	 * media type on the next reset - we are about to
	 * do automedia manually now.
	 */
	ei_local->interface_num = 0;

	/*
	 * If we are doing automedia detection, do it now.
	 * This is more reliable than the 8390's detection.
	 */
	if (dev->flags & IFF_AUTOMEDIA) {
		dev->if_port = IF_PORT_10BASET;
		etherh_setif(dev);
		mdelay(1);
		if (!etherh_getifstat(dev)) {
			dev->if_port = IF_PORT_10BASE2;
			etherh_setif(dev);
		}
	} else
		etherh_setif(dev);

	etherh_reset(dev);
	ei_open(dev);

	return 0;
}

/*
 * The inverse routine to etherh_open().
 */
static int
etherh_close(struct net_device *dev)
{
	ei_close (dev);
	free_irq (dev->irq, dev);
	return 0;
}

static int
etherh_set_mac_address(struct net_device *dev, void *p)
{
	struct sockaddr *addr = p;

	if (netif_running(dev))
		return -EBUSY;

	memcpy(dev->dev_addr, addr->sa_data, dev->addr_len);

	/*
	 * We'll set the MAC address on the chip when we open it.
	 */

	return 0;
}

/*
 * Initialisation
 */

static void __init etherh_banner(void)
{
	static int version_printed;

	if (net_debug && version_printed++ == 0)
		printk(KERN_INFO "%s", version);
}

/*
 * Read the ethernet address string from the on board rom.
 * This is an ascii string...
 */
static void __init etherh_addr(char *addr, struct expansion_card *ec)
{
	struct in_chunk_dir cd;
	char *s;
	
	memset(addr, 0, 6);

	if (ecard_readchunk(&cd, ec, 0xf5, 0) && (s = strchr(cd.d.string, '('))) {
		int i;
		for (i = 0; i < 6; i++) {
			addr[i] = simple_strtoul(s + 1, &s, 0x10);
			if (*s != (i == 5? ')' : ':'))
				break;
		}
	}
}

/*
 * Create an ethernet address from the system serial number.
 */
static void __init etherm_addr(char *addr)
{
	unsigned int serial;

	serial = system_serial_low | system_serial_high;
	if (serial != 0) {
		addr[0] = 0;
		addr[1] = 0;
		addr[2] = 0xa4;
		addr[3] = 0x10 + (serial >> 24);
		addr[4] = serial >> 16;
		addr[5] = serial >> 8;
	}
}

static u32 etherh_regoffsets[16];
static u32 etherm_regoffsets[16];

static struct net_device * __init etherh_init_one(struct expansion_card *ec)
{
	struct ei_device *ei_local;
	struct net_device *dev;
	struct etherh_priv *eh;
	const char *dev_type;
	int i, size;

	etherh_banner();

	ecard_claim(ec);
	
	dev = init_etherdev(NULL, 0);
	if (!dev)
		goto out;

	eh = kmalloc(sizeof(struct etherh_priv), GFP_KERNEL);
	if (!eh)
		goto out_nopriv;

	SET_MODULE_OWNER(dev);

	dev->open		= etherh_open;
	dev->stop		= etherh_close;
	dev->set_mac_address	= etherh_set_mac_address;
	dev->set_config		= etherh_set_config;
	dev->irq		= ec->irq;
	dev->base_addr		= ecard_address(ec, ECARD_MEMC, 0);
	dev->rmem_start		= (unsigned long)eh;

	/*
	 * IRQ and control port handling
	 */
	ec->ops		= &etherh_ops;
	ec->irq_data	= eh;
	eh->ctrl	= 0;
	eh->id		= ec->cid.product;

	switch (ec->cid.product) {
	case PROD_ANT_ETHERM:
		etherm_addr(dev->dev_addr);
		dev->base_addr += ETHERM_NS8390;
		dev->mem_start  = dev->base_addr + ETHERM_DATAPORT;
		eh->ctrl_port   = dev->base_addr + ETHERM_CTRLPORT;
		break;

	case PROD_I3_ETHERLAN500:
		etherh_addr(dev->dev_addr, ec);
		dev->base_addr += ETHERH500_NS8390;
		dev->mem_start  = dev->base_addr + ETHERH500_DATAPORT;
		eh->ctrl_port   = ecard_address (ec, ECARD_IOC, ECARD_FAST)
				  + ETHERH500_CTRLPORT;
		break;

	case PROD_I3_ETHERLAN600:
	case PROD_I3_ETHERLAN600A:
		etherh_addr(dev->dev_addr, ec);
		dev->base_addr += ETHERH600_NS8390;
		dev->mem_start  = dev->base_addr + ETHERH600_DATAPORT;
		eh->ctrl_port   = dev->base_addr + ETHERH600_CTRLPORT;
		break;

	default:
		printk(KERN_ERR "%s: unknown card type %x\n",
		       dev->name, ec->cid.product);
		goto free;
	}

	size = 16;
	if (ec->cid.product == PROD_ANT_ETHERM)
		size <<= 3;

	if (!request_region(dev->base_addr, size, dev->name))
		goto free;

	if (ethdev_init(dev))
		goto release;

	/*
	 * If we're in the NIC slot, make sure the IRQ is enabled
	 */
	if (dev->irq == 11)
		etherh_set_ctrl(eh, ETHERH_CP_IE);

	/*
	 * Unfortunately, ethdev_init eventually calls
	 * ether_setup, which re-writes dev->flags.
	 */
	switch (ec->cid.product) {
	case PROD_ANT_ETHERM:
		dev_type = "ANT EtherM";
		dev->if_port = IF_PORT_UNKNOWN;
		break;

	case PROD_I3_ETHERLAN500:
		dev_type = "i3 EtherH 500";
		dev->if_port = IF_PORT_UNKNOWN;
		break;

	case PROD_I3_ETHERLAN600:
		dev_type = "i3 EtherH 600";
		dev->flags  |= IFF_PORTSEL | IFF_AUTOMEDIA;
		dev->if_port = IF_PORT_10BASET;
		break;

	case PROD_I3_ETHERLAN600A:
		dev_type = "i3 EtherH 600A";
		dev->flags  |= IFF_PORTSEL | IFF_AUTOMEDIA;
		dev->if_port = IF_PORT_10BASET;
		break;

	default:
		dev_type = "unknown";
		break;
	}

	printk(KERN_INFO "%s: %s in slot %d, ",
		dev->name, dev_type, ec->slot_no);

	for (i = 0; i < 6; i++)
		printk("%2.2x%c", dev->dev_addr[i], i == 5 ? '\n' : ':');

	ei_local = (struct ei_device *) dev->priv;
	if (ec->cid.product == PROD_ANT_ETHERM) {
		ei_local->tx_start_page = ETHERM_TX_START_PAGE;
		ei_local->stop_page     = ETHERM_STOP_PAGE;
		ei_local->reg_offset    = etherm_regoffsets;
	} else {
		ei_local->tx_start_page = ETHERH_TX_START_PAGE;
		ei_local->stop_page     = ETHERH_STOP_PAGE;
		ei_local->reg_offset    = etherh_regoffsets;
	}

	ei_local->name          = dev->name;
	ei_local->word16        = 1;
	ei_local->rx_start_page = ei_local->tx_start_page + TX_PAGES;
	ei_local->reset_8390    = etherh_reset;
	ei_local->block_input   = etherh_block_input;
	ei_local->block_output  = etherh_block_output;
	ei_local->get_8390_hdr  = etherh_get_header;
	ei_local->interface_num = 0;

	etherh_reset(dev);
	NS8390_init(dev, 0);
	return dev;

release:
	release_region(dev->base_addr, 16);
free:
	kfree(eh);
out_nopriv:
	unregister_netdev(dev);
	kfree(dev);
out:
	ecard_release(ec);
	return NULL;
}

#define MAX_ETHERH_CARDS 2

static struct net_device *e_dev[MAX_ETHERH_CARDS];
static struct expansion_card *e_card[MAX_ETHERH_CARDS];

static int __init etherh_init(void)
{
	int i, ret = -ENODEV;

	for (i = 0; i < 16; i++) {
		etherh_regoffsets[i] = i;
		etherm_regoffsets[i] = i << 3;
	}

	ecard_startfind();

	for (i = 0; i < MAX_ECARDS; i++) {
		struct expansion_card *ec;
		struct net_device *dev;

		ec = ecard_find(0, etherh_cids);
		if (!ec)
			break;

		dev = etherh_init_one(ec);
		if (!dev)
			break;

		e_card[i] = ec;
		e_dev[i]  = dev;
		ret = 0;
	}

	return ret;
}

static void __exit etherh_exit(void)
{
	int i;

	for (i = 0; i < MAX_ETHERH_CARDS; i++) {
		if (e_dev[i]) {
			int size;
			unregister_netdev(e_dev[i]);
			size = 16;
			if (e_card[i]->cid.product == PROD_ANT_ETHERM)
				size <<= 3;
			release_region(e_dev[i]->base_addr, size);
			kfree(e_dev[i]);
			e_dev[i] = NULL;
		}
		if (e_card[i]) {
			e_card[i]->ops = NULL;
			kfree(e_card[i]->irq_data);
			ecard_release(e_card[i]);
			e_card[i] = NULL;
		}
	}
}

module_init(etherh_init);
module_exit(etherh_exit);
