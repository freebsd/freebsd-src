/* mvme147.c  : the  Linux/mvme147/lance ethernet driver
 *
 * Copyright (C) 05/1998 Peter Maydell <pmaydell@chiark.greenend.org.uk>
 * Based on the Sun Lance driver and the NetBSD HP Lance driver
 * Uses the generic 7990.c LANCE code.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/ptrace.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/pgtable.h>

/* Used for the temporal inet entries and routing */
#include <linux/socket.h>
#include <linux/route.h>

#include <linux/dio.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#include <asm/mvme147hw.h>

/* We have 16834 bytes of RAM for the init block and buffers. This places
 * an upper limit on the number of buffers we can use. NetBSD uses 8 Rx
 * buffers and 2 Tx buffers.
 */
#define LANCE_LOG_TX_BUFFERS 1
#define LANCE_LOG_RX_BUFFERS 3

#include "7990.h"                                 /* use generic LANCE code */

/* Our private data structure */
struct m147lance_private {
	struct lance_private lance;
	void *base;
	void *ram;
};

/* function prototypes... This is easy because all the grot is in the
 * generic LANCE support. All we have to support is probing for boards,
 * plus board-specific init, open and close actions.
 * Oh, and we need to tell the generic code how to read and write LANCE registers...
 */
int mvme147lance_probe(struct net_device *dev);
static int m147lance_open(struct net_device *dev);
static int m147lance_close(struct net_device *dev);
static void m147lance_writerap(struct m147lance_private *lp, unsigned short value);
static void m147lance_writerdp(struct m147lance_private *lp, unsigned short value);
static unsigned short m147lance_readrdp(struct m147lance_private *lp);

typedef void (*writerap_t)(void *, unsigned short);
typedef void (*writerdp_t)(void *, unsigned short);
typedef unsigned short (*readrdp_t)(void *);

#ifdef MODULE
static struct m147lance_private *root_m147lance_dev;
#endif

/* Initialise the one and only on-board 7990 */
int __init mvme147lance_probe(struct net_device *dev)
{
	static int called;
	static const char name[] = "MVME147 LANCE";
	struct m147lance_private *lp;
	u_long *addr;
	u_long address;

	if (!MACH_IS_MVME147 || called)
		return -ENODEV;
	called++;

	SET_MODULE_OWNER(dev);

	dev->priv = kmalloc(sizeof(struct m147lance_private), GFP_KERNEL);
	if (dev->priv == NULL)
		return -ENOMEM;
	memset(dev->priv, 0, sizeof(struct m147lance_private));

	/* Fill the dev fields */
	dev->base_addr = (unsigned long)MVME147_LANCE_BASE;
	dev->open = &m147lance_open;
	dev->stop = &m147lance_close;
	dev->hard_start_xmit = &lance_start_xmit;
	dev->get_stats = &lance_get_stats;
	dev->set_multicast_list = &lance_set_multicast;
	dev->tx_timeout = &lance_tx_timeout;
	dev->dma = 0;

	addr=(u_long *)ETHERNET_ADDRESS;
	address = *addr;
	dev->dev_addr[0]=0x08;
	dev->dev_addr[1]=0x00;
	dev->dev_addr[2]=0x3e;
	address=address>>8;
	dev->dev_addr[5]=address&0xff;
	address=address>>8;
	dev->dev_addr[4]=address&0xff;
	address=address>>8;
	dev->dev_addr[3]=address&0xff;

	printk("%s: MVME147 at 0x%08lx, irq %d, Hardware Address %02x:%02x:%02x:%02x:%02x:%02x\n",
		dev->name, dev->base_addr, MVME147_LANCE_IRQ,
		dev->dev_addr[0],
		dev->dev_addr[1], dev->dev_addr[2],
		dev->dev_addr[3], dev->dev_addr[4],
		dev->dev_addr[5]);

	lp = (struct m147lance_private *)dev->priv;
	lp->ram = (void *)__get_dma_pages(GFP_ATOMIC, 3);	/* 16K */
	if (!lp->ram)
	{
		printk("%s: No memory for LANCE buffers\n", dev->name);
		return -ENODEV;
	}

	lp->lance.name = (char*)name;                   /* discards const, shut up gcc */
	lp->lance.ll = (struct lance_regs *)(dev->base_addr);
	lp->lance.init_block = (struct lance_init_block *)(lp->ram); /* CPU addr */
	lp->lance.lance_init_block = (struct lance_init_block *)(lp->ram);                 /* LANCE addr of same RAM */
	lp->lance.busmaster_regval = LE_C3_BSWP;        /* we're bigendian */
	lp->lance.irq = MVME147_LANCE_IRQ;
	lp->lance.writerap = (writerap_t)m147lance_writerap;
	lp->lance.writerdp = (writerdp_t)m147lance_writerdp;
	lp->lance.readrdp = (readrdp_t)m147lance_readrdp;
	lp->lance.lance_log_rx_bufs = LANCE_LOG_RX_BUFFERS;
	lp->lance.lance_log_tx_bufs = LANCE_LOG_TX_BUFFERS;
	lp->lance.rx_ring_mod_mask = RX_RING_MOD_MASK;
	lp->lance.tx_ring_mod_mask = TX_RING_MOD_MASK;
	ether_setup(dev);

#ifdef MODULE
	dev->ifindex = dev_new_index();
	lp->next_module = root_m147lance_dev;
	root_m147lance_dev = lp;
#endif /* MODULE */

	return 0;
}

static void m147lance_writerap(struct m147lance_private *lp, unsigned short value)
{
	lp->lance.ll->rap = value;
}

static void m147lance_writerdp(struct m147lance_private *lp, unsigned short value)
{
	lp->lance.ll->rdp = value;
}

static unsigned short m147lance_readrdp(struct m147lance_private *lp)
{
	return lp->lance.ll->rdp;
}

static int m147lance_open(struct net_device *dev)
{
	int status;

	status = lance_open(dev);                 /* call generic lance open code */
	if (status)
		return status;
	/* enable interrupts at board level. */
	m147_pcc->lan_cntrl=0;       /* clear the interrupts (if any) */
	m147_pcc->lan_cntrl=0x08 | 0x04;     /* Enable irq 4 */

	return 0;
}

static int m147lance_close(struct net_device *dev)
{
	/* disable interrupts at boardlevel */
	m147_pcc->lan_cntrl=0x0; /* disable interrupts */
	lance_close(dev);
	return 0;
}

#ifdef MODULE
MODULE_LICENSE("GPL");

int init_module(void)
{
	root_lance_dev = NULL;
	return mvme147lance_probe(NULL);
}

void cleanup_module(void)
{
	/* Walk the chain of devices, unregistering them */
	struct m147lance_private *lp;
	while (root_m147lance_dev) {
		lp = root_m147lance_dev->next_module;
		unregister_netdev(root_lance_dev->dev);
		free_pages(lp->ram, 3);
		kfree(root_lance_dev->dev);
		root_lance_dev = lp;
	}
}

#endif /* MODULE */
