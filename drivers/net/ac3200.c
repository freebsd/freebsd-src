/* ac3200.c: A driver for the Ansel Communications EISA ethernet adaptor. */
/*
	Written 1993, 1994 by Donald Becker.
	Copyright 1993 United States Government as represented by the Director,
	National Security Agency.  This software may only be used and distributed
	according to the terms of the GNU General Public License as modified by SRC,
	incorporated herein by reference.

	The author may be reached as becker@scyld.com, or C/O
	Scyld Computing Corporation
	410 Severn Ave., Suite 210
	Annapolis MD 21403

	This is driver for the Ansel Communications Model 3200 EISA Ethernet LAN
	Adapter.  The programming information is from the users manual, as related
	by glee@ardnassak.math.clemson.edu.

	Changelog:

	Paul Gortmaker 05/98	: add support for shared mem above 1MB.

  */

static const char version[] =
	"ac3200.c:v1.01 7/1/94 Donald Becker (becker@cesdis.gsfc.nasa.gov)\n";

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/init.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>

#include "8390.h"

/* Offsets from the base address. */
#define AC_NIC_BASE	0x00
#define AC_SA_PROM	0x16			/* The station address PROM. */
#define AC_ADDR0	0x00			/* Prefix station address values. */
#define AC_ADDR1	0x40			
#define AC_ADDR2	0x90
#define AC_ID_PORT	0xC80
#define AC_EISA_ID	0x0110d305
#define AC_RESET_PORT	0xC84
#define AC_RESET	0x00
#define AC_ENABLE	0x01
#define AC_CONFIG	0xC90	/* The configuration port. */

#define AC_IO_EXTENT 0x20
                                /* Actually accessed is:
								 * AC_NIC_BASE (0-15)
								 * AC_SA_PROM (0-5)
								 * AC_ID_PORT (0-3)
								 * AC_RESET_PORT
								 * AC_CONFIG
								 */

/* Decoding of the configuration register. */
static unsigned char config2irqmap[8] __initdata = {15, 12, 11, 10, 9, 7, 5, 3};
static int addrmap[8] =
{0xFF0000, 0xFE0000, 0xFD0000, 0xFFF0000, 0xFFE0000, 0xFFC0000,  0xD0000, 0 };
static const char *port_name[4] = { "10baseT", "invalid", "AUI", "10base2"};

#define config2irq(configval)	config2irqmap[((configval) >> 3) & 7]
#define config2mem(configval)	addrmap[(configval) & 7]
#define config2name(configval)	port_name[((configval) >> 6) & 3]

/* First and last 8390 pages. */
#define AC_START_PG		0x00	/* First page of 8390 TX buffer */
#define AC_STOP_PG		0x80	/* Last page +1 of the 8390 RX ring */

int ac3200_probe(struct net_device *dev);
static int ac_probe1(int ioaddr, struct net_device *dev);

static int ac_open(struct net_device *dev);
static void ac_reset_8390(struct net_device *dev);
static void ac_block_input(struct net_device *dev, int count,
					struct sk_buff *skb, int ring_offset);
static void ac_block_output(struct net_device *dev, const int count,
							const unsigned char *buf, const int start_page);
static void ac_get_8390_hdr(struct net_device *dev, struct e8390_pkt_hdr *hdr,
					int ring_page);

static int ac_close_card(struct net_device *dev);


/*	Probe for the AC3200.

	The AC3200 can be identified by either the EISA configuration registers,
	or the unique value in the station address PROM.
	*/

int __init ac3200_probe(struct net_device *dev)
{
	unsigned short ioaddr = dev->base_addr;

	SET_MODULE_OWNER(dev);

	if (ioaddr > 0x1ff)		/* Check a single specified location. */
		return ac_probe1(ioaddr, dev);
	else if (ioaddr > 0)		/* Don't probe at all. */
		return -ENXIO;

	if ( ! EISA_bus)
		return -ENXIO;

	for (ioaddr = 0x1000; ioaddr < 0x9000; ioaddr += 0x1000)
		if (ac_probe1(ioaddr, dev) == 0)
			return 0;

	return -ENODEV;
}

static int __init ac_probe1(int ioaddr, struct net_device *dev)
{
	int i, retval;

	if (!request_region(ioaddr, AC_IO_EXTENT, dev->name))
		return -EBUSY;

	if (inb_p(ioaddr + AC_ID_PORT) == 0xff) {
		retval = -ENODEV;
		goto out;
	}

	if (inl(ioaddr + AC_ID_PORT) != AC_EISA_ID) {
		retval = -ENODEV;
		goto out;
	}

#ifndef final_version
	printk(KERN_DEBUG "AC3200 ethercard configuration register is %#02x,"
		   " EISA ID %02x %02x %02x %02x.\n", inb(ioaddr + AC_CONFIG),
		   inb(ioaddr + AC_ID_PORT + 0), inb(ioaddr + AC_ID_PORT + 1),
		   inb(ioaddr + AC_ID_PORT + 2), inb(ioaddr + AC_ID_PORT + 3));
#endif

	printk("AC3200 in EISA slot %d, node", ioaddr/0x1000);
	for(i = 0; i < 6; i++)
		printk(" %02x", dev->dev_addr[i] = inb(ioaddr + AC_SA_PROM + i));

#if 0
	/* Check the vendor ID/prefix. Redundant after checking the EISA ID */
	if (inb(ioaddr + AC_SA_PROM + 0) != AC_ADDR0
		|| inb(ioaddr + AC_SA_PROM + 1) != AC_ADDR1
		|| inb(ioaddr + AC_SA_PROM + 2) != AC_ADDR2 ) {
		printk(", not found (invalid prefix).\n");
		retval = -ENODEV;
		goto out;
	}
#endif

	/* Allocate dev->priv and fill in 8390 specific dev fields. */
	if (ethdev_init(dev)) {
		printk (", unable to allocate memory for dev->priv.\n");
		retval = -ENOMEM;
		goto out;
	}

	/* Assign and allocate the interrupt now. */
	if (dev->irq == 0) {
		dev->irq = config2irq(inb(ioaddr + AC_CONFIG));
		printk(", using");
	} else {
		dev->irq = irq_cannonicalize(dev->irq);
		printk(", assigning");
	}

	retval = request_irq(dev->irq, ei_interrupt, 0, dev->name, dev);
	if (retval) {
		printk (" nothing! Unable to get IRQ %d.\n", dev->irq);
		goto out1;
	}

	printk(" IRQ %d, %s port\n", dev->irq, port_name[dev->if_port]);

	dev->base_addr = ioaddr;

#ifdef notyet
	if (dev->mem_start)	{		/* Override the value from the board. */
		for (i = 0; i < 7; i++)
			if (addrmap[i] == dev->mem_start)
				break;
		if (i >= 7)
			i = 0;
		outb((inb(ioaddr + AC_CONFIG) & ~7) | i, ioaddr + AC_CONFIG);
	}
#endif

	dev->if_port = inb(ioaddr + AC_CONFIG) >> 6;
	dev->mem_start = config2mem(inb(ioaddr + AC_CONFIG));

	printk("%s: AC3200 at %#3x with %dkB memory at physical address %#lx.\n", 
			dev->name, ioaddr, AC_STOP_PG/4, dev->mem_start);

	/*
	 *  BEWARE!! Some dain-bramaged EISA SCUs will allow you to put
	 *  the card mem within the region covered by `normal' RAM  !!!
	 */
	if (dev->mem_start > 1024*1024) {	/* phys addr > 1MB */
		if (dev->mem_start < virt_to_bus(high_memory)) {
			printk(KERN_CRIT "ac3200.c: Card RAM overlaps with normal memory!!!\n");
			printk(KERN_CRIT "ac3200.c: Use EISA SCU to set card memory below 1MB,\n");
			printk(KERN_CRIT "ac3200.c: or to an address above 0x%lx.\n", virt_to_bus(high_memory));
			printk(KERN_CRIT "ac3200.c: Driver NOT installed.\n");
			retval = -EINVAL;
			goto out2;
		}
		dev->mem_start = (unsigned long)ioremap(dev->mem_start, AC_STOP_PG*0x100);
		if (dev->mem_start == 0) {
			printk(KERN_ERR "ac3200.c: Unable to remap card memory above 1MB !!\n");
			printk(KERN_ERR "ac3200.c: Try using EISA SCU to set memory below 1MB.\n");
			printk(KERN_ERR "ac3200.c: Driver NOT installed.\n");
			retval = -EINVAL;
			goto out2;
		}
		ei_status.reg0 = 1;	/* Use as remap flag */
		printk("ac3200.c: remapped %dkB card memory to virtual address %#lx\n",
				AC_STOP_PG/4, dev->mem_start);
	}

	dev->rmem_start = dev->mem_start + TX_PAGES*256;
	dev->mem_end = dev->rmem_end = dev->mem_start
		+ (AC_STOP_PG - AC_START_PG)*256;

	ei_status.name = "AC3200";
	ei_status.tx_start_page = AC_START_PG;
	ei_status.rx_start_page = AC_START_PG + TX_PAGES;
	ei_status.stop_page = AC_STOP_PG;
	ei_status.word16 = 1;

	if (ei_debug > 0)
		printk(version);

	ei_status.reset_8390 = &ac_reset_8390;
	ei_status.block_input = &ac_block_input;
	ei_status.block_output = &ac_block_output;
	ei_status.get_8390_hdr = &ac_get_8390_hdr;

	dev->open = &ac_open;
	dev->stop = &ac_close_card;
	NS8390_init(dev, 0);
	return 0;
out2:
	free_irq(dev->irq, dev);
out1:
	kfree(dev->priv);
	dev->priv = NULL;
out:
	release_region(ioaddr, AC_IO_EXTENT);
	return retval;
}

static int ac_open(struct net_device *dev)
{
#ifdef notyet
	/* Someday we may enable the IRQ and shared memory here. */
	int ioaddr = dev->base_addr;
#endif

	ei_open(dev);
	return 0;
}

static void ac_reset_8390(struct net_device *dev)
{
	ushort ioaddr = dev->base_addr;

	outb(AC_RESET, ioaddr + AC_RESET_PORT);
	if (ei_debug > 1) printk("resetting AC3200, t=%ld...", jiffies);

	ei_status.txing = 0;
	outb(AC_ENABLE, ioaddr + AC_RESET_PORT);
	if (ei_debug > 1) printk("reset done\n");

	return;
}

/* Grab the 8390 specific header. Similar to the block_input routine, but
   we don't need to be concerned with ring wrap as the header will be at
   the start of a page, so we optimize accordingly. */

static void
ac_get_8390_hdr(struct net_device *dev, struct e8390_pkt_hdr *hdr, int ring_page)
{
	unsigned long hdr_start = dev->mem_start + ((ring_page - AC_START_PG)<<8);
	isa_memcpy_fromio(hdr, hdr_start, sizeof(struct e8390_pkt_hdr));
}

/*  Block input and output are easy on shared memory ethercards, the only
	complication is when the ring buffer wraps. */

static void ac_block_input(struct net_device *dev, int count, struct sk_buff *skb,
						  int ring_offset)
{
	unsigned long xfer_start = dev->mem_start + ring_offset - (AC_START_PG<<8);

	if (xfer_start + count > dev->rmem_end) {
		/* We must wrap the input move. */
		int semi_count = dev->rmem_end - xfer_start;
		isa_memcpy_fromio(skb->data, xfer_start, semi_count);
		count -= semi_count;
		isa_memcpy_fromio(skb->data + semi_count, dev->rmem_start, count);
	} else {
		/* Packet is in one chunk -- we can copy + cksum. */
		isa_eth_io_copy_and_sum(skb, xfer_start, count, 0);
	}
}

static void ac_block_output(struct net_device *dev, int count,
							const unsigned char *buf, int start_page)
{
	unsigned long shmem = dev->mem_start + ((start_page - AC_START_PG)<<8);

	isa_memcpy_toio(shmem, buf, count);
}

static int ac_close_card(struct net_device *dev)
{
	if (ei_debug > 1)
		printk("%s: Shutting down ethercard.\n", dev->name);

#ifdef notyet
	/* We should someday disable shared memory and interrupts. */
	outb(0x00, ioaddr + 6);	/* Disable interrupts. */
	free_irq(dev->irq, dev);
#endif

	ei_close(dev);
	return 0;
}

#ifdef MODULE
#define MAX_AC32_CARDS	4	/* Max number of AC32 cards per module */
static struct net_device dev_ac32[MAX_AC32_CARDS];
static int io[MAX_AC32_CARDS];
static int irq[MAX_AC32_CARDS];
static int mem[MAX_AC32_CARDS];
MODULE_PARM(io, "1-" __MODULE_STRING(MAX_AC32_CARDS) "i");
MODULE_PARM(irq, "1-" __MODULE_STRING(MAX_AC32_CARDS) "i");
MODULE_PARM(mem, "1-" __MODULE_STRING(MAX_AC32_CARDS) "i");
MODULE_PARM_DESC(io, "I/O base adress(es)");
MODULE_PARM_DESC(irq, "IRQ number(s)");
MODULE_PARM_DESC(mem, "Memory base address(es)");
MODULE_DESCRIPTION("Ansel AC3200 EISA ethernet driver");
MODULE_LICENSE("GPL");

int
init_module(void)
{
	int this_dev, found = 0;

	for (this_dev = 0; this_dev < MAX_AC32_CARDS; this_dev++) {
		struct net_device *dev = &dev_ac32[this_dev];
		dev->irq = irq[this_dev];
		dev->base_addr = io[this_dev];
		dev->mem_start = mem[this_dev];		/* Currently ignored by driver */
		dev->init = ac3200_probe;
		/* Default is to only install one card. */
		if (io[this_dev] == 0 && this_dev != 0) break;
		if (register_netdev(dev) != 0) {
			printk(KERN_WARNING "ac3200.c: No ac3200 card found (i/o = 0x%x).\n", io[this_dev]);
			if (found != 0) {	/* Got at least one. */
				return 0;
			}
			return -ENXIO;
		}
		found++;
	}
	return 0;
}

void
cleanup_module(void)
{
	int this_dev;

	for (this_dev = 0; this_dev < MAX_AC32_CARDS; this_dev++) {
		struct net_device *dev = &dev_ac32[this_dev];
		if (dev->priv != NULL) {
			/* Someday free_irq may be in ac_close_card() */
			free_irq(dev->irq, dev);
			release_region(dev->base_addr, AC_IO_EXTENT);
			if (ei_status.reg0)
				iounmap((void *)dev->mem_start);
			unregister_netdev(dev);
			kfree(dev->priv);
			dev->priv = NULL;
		}
	}
}
#endif /* MODULE */


/*
 * Local variables:
 * compile-command: "gcc -D__KERNEL__ -I/usr/src/linux/net/inet -Wall -Wstrict-prototypes -O6 -m486 -c ac3200.c"
 *  version-control: t
 *  kept-new-versions: 5
 *  tab-width: 4
 * End:
 */
