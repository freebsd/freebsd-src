/*
	ne3210.c

	Linux driver for Novell NE3210 EISA Network Adapter

	Copyright (C) 1998, Paul Gortmaker.

	This software may be used and distributed according to the terms
	of the GNU General Public License, incorporated herein by reference.

	Information and Code Sources:

	1) Based upon my other EISA 8390 drivers (lne390, es3210, smc-ultra32)
	2) The existing myriad of other Linux 8390 drivers by Donald Becker.
	3) Info for getting IRQ and sh-mem gleaned from the EISA cfg file

	The NE3210 is an EISA shared memory NS8390 implementation.  Shared 
	memory address > 1MB should work with this driver.

	Note that the .cfg file (3/11/93, v1.0) has AUI and BNC switched 
	around (or perhaps there are some defective/backwards cards ???)

	This driver WILL NOT WORK FOR THE NE3200 - it is completely different
	and does not use an 8390 at all.

*/

static const char *version =
	"ne3210.c: Driver revision v0.03, 30/09/98\n";

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/init.h>

#include <asm/io.h>
#include <asm/system.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include "8390.h"

int ne3210_probe(struct net_device *dev);
static int ne3210_probe1(struct net_device *dev, int ioaddr);

static int ne3210_open(struct net_device *dev);
static int ne3210_close(struct net_device *dev);

static void ne3210_reset_8390(struct net_device *dev);

static void ne3210_get_8390_hdr(struct net_device *dev, struct e8390_pkt_hdr *hdr, int ring_page);
static void ne3210_block_input(struct net_device *dev, int count, struct sk_buff *skb, int ring_offset);
static void ne3210_block_output(struct net_device *dev, int count, const unsigned char *buf, const int start_page);

#define NE3210_START_PG		0x00    /* First page of TX buffer	*/
#define NE3210_STOP_PG		0x80    /* Last page +1 of RX ring	*/

#define NE3210_ID_PORT		0xc80	/* Same for all EISA cards 	*/
#define NE3210_IO_EXTENT	0x20
#define NE3210_SA_PROM		0x16	/* Start of e'net addr.		*/
#define NE3210_RESET_PORT	0xc84
#define NE3210_NIC_OFFSET	0x00	/* Hello, the 8390 is *here*	*/

#define NE3210_ADDR0		0x00	/* 3 byte vendor prefix		*/
#define NE3210_ADDR1		0x00
#define NE3210_ADDR2		0x1b

#define NE3210_ID	0x0118cc3a	/* 0x3acc = 1110 10110 01100 =  nvl */

#define NE3210_CFG1		0xc84	/* NB: 0xc84 is also "reset" port. */
#define NE3210_CFG2		0xc90

/*
 *	You can OR any of the following bits together and assign it
 *	to NE3210_DEBUG to get verbose driver info during operation.
 *	Currently only the probe one is implemented.
 */

#define NE3210_D_PROBE	0x01
#define NE3210_D_RX_PKT	0x02
#define NE3210_D_TX_PKT	0x04
#define NE3210_D_IRQ	0x08

#define NE3210_DEBUG	0x0

static unsigned char irq_map[] __initdata = {15, 12, 11, 10, 9, 7, 5, 3};
static unsigned int shmem_map[] __initdata = {0xff0, 0xfe0, 0xfff0, 0xd8, 0xffe0, 0xffc0, 0xd0, 0x0};

/*
 *	Probe for the card. The best way is to read the EISA ID if it
 *	is known. Then we can check the prefix of the station address
 *	PROM for a match against the value assigned to Novell.
 */

int __init ne3210_probe(struct net_device *dev)
{
	unsigned short ioaddr = dev->base_addr;

	SET_MODULE_OWNER(dev);

	if (ioaddr > 0x1ff)		/* Check a single specified location. */
		return ne3210_probe1(dev, ioaddr);
	else if (ioaddr > 0)		/* Don't probe at all. */
		return -ENXIO;

	if (!EISA_bus) {
#if NE3210_DEBUG & NE3210_D_PROBE
		printk("ne3210-debug: Not an EISA bus. Not probing high ports.\n");
#endif
		return -ENXIO;
	}

	/* EISA spec allows for up to 16 slots, but 8 is typical. */
	for (ioaddr = 0x1000; ioaddr < 0x9000; ioaddr += 0x1000)
		if (ne3210_probe1(dev, ioaddr) == 0)
			return 0;

	return -ENODEV;
}

static int __init ne3210_probe1(struct net_device *dev, int ioaddr)
{
	int i, retval;
	unsigned long eisa_id;
	const char *ifmap[] = {"UTP", "?", "BNC", "AUI"};

	if (!request_region(dev->base_addr, NE3210_IO_EXTENT, dev->name))
		return -EBUSY;

	if (inb_p(ioaddr + NE3210_ID_PORT) == 0xff) {
		retval = -ENODEV;
		goto out;
	}

#if NE3210_DEBUG & NE3210_D_PROBE
	printk("ne3210-debug: probe at %#x, ID %#8x\n", ioaddr, inl(ioaddr + NE3210_ID_PORT));
	printk("ne3210-debug: config regs: %#x %#x\n",
		inb(ioaddr + NE3210_CFG1), inb(ioaddr + NE3210_CFG2));
#endif


/*	Check the EISA ID of the card. */
	eisa_id = inl(ioaddr + NE3210_ID_PORT);
	if (eisa_id != NE3210_ID) {
		retval = -ENODEV;
		goto out;
	}

	
#if 0
/*	Check the vendor ID as well. Not really required. */
	if (inb(ioaddr + NE3210_SA_PROM + 0) != NE3210_ADDR0
		|| inb(ioaddr + NE3210_SA_PROM + 1) != NE3210_ADDR1
		|| inb(ioaddr + NE3210_SA_PROM + 2) != NE3210_ADDR2 ) {
		printk("ne3210.c: card not found");
		for(i = 0; i < ETHER_ADDR_LEN; i++)
			printk(" %02x", inb(ioaddr + NE3210_SA_PROM + i));
		printk(" (invalid prefix).\n");
		retval = -ENODEV;
		goto out;
	}
#endif

	/* Allocate dev->priv and fill in 8390 specific dev fields. */
	if (ethdev_init(dev)) {
		printk ("ne3210.c: unable to allocate memory for dev->priv!\n");
		retval = -ENOMEM;
		goto out;
	}

	printk("ne3210.c: NE3210 in EISA slot %d, media: %s, addr:",
		ioaddr/0x1000, ifmap[inb(ioaddr + NE3210_CFG2) >> 6]);
	for(i = 0; i < ETHER_ADDR_LEN; i++)
		printk(" %02x", (dev->dev_addr[i] = inb(ioaddr + NE3210_SA_PROM + i)));
	printk(".\nne3210.c: ");

	/* Snarf the interrupt now. CFG file has them all listed as `edge' with share=NO */
	if (dev->irq == 0) {
		unsigned char irq_reg = inb(ioaddr + NE3210_CFG2) >> 3;
		dev->irq = irq_map[irq_reg & 0x07];
		printk("using");
	} else {
		/* This is useless unless we reprogram the card here too */
		if (dev->irq == 2) dev->irq = 9;	/* Doh! */
		printk("assigning");
	}
	printk(" IRQ %d,", dev->irq);

	retval = request_irq(dev->irq, ei_interrupt, 0, dev->name, dev);
	if (retval) {
		printk (" unable to get IRQ %d.\n", dev->irq);
		goto out1;
	}

	if (dev->mem_start == 0) {
		unsigned char mem_reg = inb(ioaddr + NE3210_CFG2) & 0x07;
		dev->mem_start = shmem_map[mem_reg] * 0x1000;
		printk(" using ");
	} else {
		/* Should check for value in shmem_map and reprogram the card to use it */
		dev->mem_start &= 0xfff8000;
		printk(" assigning ");
	}

	printk("%dkB memory at physical address %#lx\n",
			NE3210_STOP_PG/4, dev->mem_start);

	/*
	   BEWARE!! Some dain-bramaged EISA SCUs will allow you to put
	   the card mem within the region covered by `normal' RAM  !!!
	*/
	if (dev->mem_start > 1024*1024) {	/* phys addr > 1MB */
		if (dev->mem_start < virt_to_bus(high_memory)) {
			printk(KERN_CRIT "ne3210.c: Card RAM overlaps with normal memory!!!\n");
			printk(KERN_CRIT "ne3210.c: Use EISA SCU to set card memory below 1MB,\n");
			printk(KERN_CRIT "ne3210.c: or to an address above 0x%lx.\n", virt_to_bus(high_memory));
			printk(KERN_CRIT "ne3210.c: Driver NOT installed.\n");
			retval = -EINVAL;
			goto out2;
		}
		dev->mem_start = (unsigned long)ioremap(dev->mem_start, NE3210_STOP_PG*0x100);
		if (dev->mem_start == 0) {
			printk(KERN_ERR "ne3210.c: Unable to remap card memory above 1MB !!\n");
			printk(KERN_ERR "ne3210.c: Try using EISA SCU to set memory below 1MB.\n");
			printk(KERN_ERR "ne3210.c: Driver NOT installed.\n");
			retval = -EAGAIN;
			goto out2;
		}
		ei_status.reg0 = 1;	/* Use as remap flag */
		printk("ne3210.c: remapped %dkB card memory to virtual address %#lx\n",
				NE3210_STOP_PG/4, dev->mem_start);
	}

	dev->mem_end = dev->rmem_end = dev->mem_start
		+ (NE3210_STOP_PG - NE3210_START_PG)*256;
	dev->rmem_start = dev->mem_start + TX_PAGES*256;

	/* The 8390 offset is zero for the NE3210 */
	dev->base_addr = ioaddr;

	ei_status.name = "NE3210";
	ei_status.tx_start_page = NE3210_START_PG;
	ei_status.rx_start_page = NE3210_START_PG + TX_PAGES;
	ei_status.stop_page = NE3210_STOP_PG;
	ei_status.word16 = 1;

	if (ei_debug > 0)
		printk(version);

	ei_status.reset_8390 = &ne3210_reset_8390;
	ei_status.block_input = &ne3210_block_input;
	ei_status.block_output = &ne3210_block_output;
	ei_status.get_8390_hdr = &ne3210_get_8390_hdr;

	dev->open = &ne3210_open;
	dev->stop = &ne3210_close;
	NS8390_init(dev, 0);
	return 0;
out2:
	free_irq(dev->irq, dev);	
out1:
	kfree(dev->priv);
	dev->priv = NULL;
out:
	release_region(ioaddr, NE3210_IO_EXTENT);
	return retval;
}

/*
 *	Reset by toggling the "Board Enable" bits (bit 2 and 0).
 */

static void ne3210_reset_8390(struct net_device *dev)
{
	unsigned short ioaddr = dev->base_addr;

	outb(0x04, ioaddr + NE3210_RESET_PORT);
	if (ei_debug > 1) printk("%s: resetting the NE3210...", dev->name);

	mdelay(2);

	ei_status.txing = 0;
	outb(0x01, ioaddr + NE3210_RESET_PORT);
	if (ei_debug > 1) printk("reset done\n");

	return;
}

/*
 *	Note: In the following three functions is the implicit assumption
 *	that the associated memcpy will only use "rep; movsl" as long as
 *	we keep the counts as some multiple of doublewords. This is a
 *	requirement of the hardware, and also prevents us from using
 *	eth_io_copy_and_sum() since we can't guarantee it will limit
 *	itself to doubleword access.
 */

/*
 *	Grab the 8390 specific header. Similar to the block_input routine, but
 *	we don't need to be concerned with ring wrap as the header will be at
 *	the start of a page, so we optimize accordingly. (A single doubleword.)
 */

static void
ne3210_get_8390_hdr(struct net_device *dev, struct e8390_pkt_hdr *hdr, int ring_page)
{
	unsigned long hdr_start = dev->mem_start + ((ring_page - NE3210_START_PG)<<8);
	isa_memcpy_fromio(hdr, hdr_start, sizeof(struct e8390_pkt_hdr));
	hdr->count = (hdr->count + 3) & ~3;     /* Round up allocation. */
}

/*	
 *	Block input and output are easy on shared memory ethercards, the only
 *	complication is when the ring buffer wraps. The count will already
 *	be rounded up to a doubleword value via ne3210_get_8390_hdr() above.
 */

static void ne3210_block_input(struct net_device *dev, int count, struct sk_buff *skb,
						  int ring_offset)
{
	unsigned long xfer_start = dev->mem_start + ring_offset - (NE3210_START_PG<<8);

	if (xfer_start + count > dev->rmem_end) {
		/* Packet wraps over end of ring buffer. */
		int semi_count = dev->rmem_end - xfer_start;
		isa_memcpy_fromio(skb->data, xfer_start, semi_count);
		count -= semi_count;
		isa_memcpy_fromio(skb->data + semi_count, dev->rmem_start, count);
	} else {
		/* Packet is in one chunk. */
		isa_memcpy_fromio(skb->data, xfer_start, count);
	}
}

static void ne3210_block_output(struct net_device *dev, int count,
				const unsigned char *buf, int start_page)
{
	unsigned long shmem = dev->mem_start + ((start_page - NE3210_START_PG)<<8);

	count = (count + 3) & ~3;     /* Round up to doubleword */
	isa_memcpy_toio(shmem, buf, count);
}

static int ne3210_open(struct net_device *dev)
{
	ei_open(dev);
	return 0;
}

static int ne3210_close(struct net_device *dev)
{

	if (ei_debug > 1)
		printk("%s: Shutting down ethercard.\n", dev->name);

	ei_close(dev);
	return 0;
}

#ifdef MODULE
#define MAX_NE3210_CARDS	4	/* Max number of NE3210 cards per module */
static struct net_device dev_ne3210[MAX_NE3210_CARDS];
static int io[MAX_NE3210_CARDS];
static int irq[MAX_NE3210_CARDS];
static int mem[MAX_NE3210_CARDS];

MODULE_PARM(io, "1-" __MODULE_STRING(MAX_NE3210_CARDS) "i");
MODULE_PARM(irq, "1-" __MODULE_STRING(MAX_NE3210_CARDS) "i");
MODULE_PARM(mem, "1-" __MODULE_STRING(MAX_NE3210_CARDS) "i");
MODULE_PARM_DESC(io, "I/O base address(es)");
MODULE_PARM_DESC(irq, "IRQ number(s)");
MODULE_PARM_DESC(mem, "memory base address(es)");
MODULE_DESCRIPTION("NE3210 EISA Ethernet driver");
MODULE_LICENSE("GPL");

int init_module(void)
{
	int this_dev, found = 0;

	for (this_dev = 0; this_dev < MAX_NE3210_CARDS; this_dev++) {
		struct net_device *dev = &dev_ne3210[this_dev];
		dev->irq = irq[this_dev];
		dev->base_addr = io[this_dev];
		dev->mem_start = mem[this_dev];
		dev->init = ne3210_probe;
		/* Default is to only install one card. */
		if (io[this_dev] == 0 && this_dev != 0) break;
		if (register_netdev(dev) != 0) {
			printk(KERN_WARNING "ne3210.c: No NE3210 card found (i/o = 0x%x).\n", io[this_dev]);
			if (found != 0) {	/* Got at least one. */
				return 0;
			}
			return -ENXIO;
		}
		found++;
	}
	return 0;
}

void cleanup_module(void)
{
	int this_dev;

	for (this_dev = 0; this_dev < MAX_NE3210_CARDS; this_dev++) {
		struct net_device *dev = &dev_ne3210[this_dev];
		if (dev->priv != NULL) {
			free_irq(dev->irq, dev);
			release_region(dev->base_addr, NE3210_IO_EXTENT);
			if (ei_status.reg0)
				iounmap((void *)dev->mem_start);
			unregister_netdev(dev);
			kfree(dev->priv);
			dev->priv = NULL;
		}
	}
}

#endif /* MODULE */

