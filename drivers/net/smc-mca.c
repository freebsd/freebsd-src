/* smc-mca.c: A SMC Ultra ethernet driver for linux. */
/*
    Most of this driver, except for ultramca_probe is nearly
    verbatim from smc-ultra.c by Donald Becker. The rest is
    written and copyright 1996 by David Weis, weisd3458@uni.edu

    This is a driver for the SMC Ultra and SMC EtherEZ ethercards.

    This driver uses the cards in the 8390-compatible, shared memory mode.
    Most of the run-time complexity is handled by the generic code in
    8390.c.

    This driver enables the shared memory only when doing the actual data
    transfers to avoid a bug in early version of the card that corrupted
    data transferred by a AHA1542.

    This driver does not support the programmed-I/O data transfer mode of
    the EtherEZ.  That support (if available) is smc-ez.c.  Nor does it
    use the non-8390-compatible "Altego" mode. (No support currently planned.)

    Changelog:

    Paul Gortmaker	 : multiple card support for module users.
    David Weis		 : Micro Channel-ized it.
    Tom Sightler	 : Added support for IBM PS/2 Ethernet Adapter/A
    Christopher Turcksin : Changed MCA-probe so that multiple adapters are
			   found correctly (Jul 16, 1997)
    Chris Beauregard	 : Tried to merge the two changes above (Dec 15, 1997)
    Tom Sightler	 : Fixed minor detection bug caused by above merge
    Tom Sightler	 : Added support for three more Western Digital
			   MCA-adapters
    Tom Sightler	 : Added support for 2.2.x mca_find_unused_adapter
    Hartmut Schmidt	 : - Modified parameter detection to handle each
			     card differently depending on a switch-list
			   - 'card_ver' removed from the adapter list
			   - Some minor bug fixes
*/


#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/init.h>
#include <asm/io.h>
#include <asm/system.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include "8390.h"
#include "smc-mca.h"
#include <linux/mca.h>

int ultramca_probe(struct net_device *dev);

static int ultramca_open(struct net_device *dev);
static void ultramca_reset_8390(struct net_device *dev);
static void ultramca_get_8390_hdr(struct net_device *dev,
                                  struct e8390_pkt_hdr *hdr,
                                  int ring_page);
static void ultramca_block_input(struct net_device *dev, int count,
                                 struct sk_buff *skb,
                                 int ring_offset);
static void ultramca_block_output(struct net_device *dev, int count,
                                  const unsigned char *buf,
                                  const int start_page);
static int ultramca_close_card(struct net_device *dev);

#define START_PG        0x00    /* First page of TX buffer */

#define ULTRA_CMDREG 0      /* Offset to ASIC command register. */
#define ULTRA_RESET  0x80   /* Board reset, in ULTRA_CMDREG. */
#define ULTRA_MEMENB 0x40   /* Enable the shared memory. */
#define ULTRA_NIC_OFFSET 16 /* NIC register offset from the base_addr. */
#define ULTRA_IO_EXTENT 32
#define EN0_ERWCNT      0x08  /* Early receive warning count. */

#define _61c8_SMC_Ethercard_PLUS_Elite_A_BNC_AUI_WD8013EP_A            0
#define _61c9_SMC_Ethercard_PLUS_Elite_A_UTP_AUI_WD8013EP_A            1
#define _6fc0_WD_Ethercard_PLUS_A_WD8003E_A_OR_WD8003ET_A              2
#define _6fc1_WD_Starcard_PLUS_A_WD8003ST_A                            3
#define _6fc2_WD_Ethercard_PLUS_10T_A_WD8003W_A                        4
#define _efd4_IBM_PS2_Adapter_A_for_Ethernet_UTP_AUI_WD8013WP_A        5
#define _efd5_IBM_PS2_Adapter_A_for_Ethernet_BNC_AUI_WD8013WP_A        6
#define _efe5_IBM_PS2_Adapter_A_for_Ethernet                           7

struct smc_mca_adapters_t {
	unsigned int id;
	char *name;
};

static struct smc_mca_adapters_t smc_mca_adapters[] __initdata = {
    { 0x61c8, "SMC Ethercard PLUS Elite/A BNC/AUI (WD8013EP/A)" },
    { 0x61c9, "SMC Ethercard PLUS Elite/A UTP/AUI (WD8013WP/A)" },
    { 0x6fc0, "WD Ethercard PLUS/A (WD8003E/A or WD8003ET/A)" },
    { 0x6fc1, "WD Starcard PLUS/A (WD8003ST/A)" },
    { 0x6fc2, "WD Ethercard PLUS 10T/A (WD8003W/A)" },
    { 0xefd4, "IBM PS/2 Adapter/A for Ethernet UTP/AUI (WD8013WP/A)" },
    { 0xefd5, "IBM PS/2 Adapter/A for Ethernet BNC/AUI (WD8013EP/A)" },
    { 0xefe5, "IBM PS/2 Adapter/A for Ethernet" },
    { 0x0000, NULL }
};

int __init ultramca_probe(struct net_device *dev)
{
	unsigned short ioaddr;
	unsigned char reg4, num_pages;
	char slot = -1;
	unsigned char pos2 = 0xff, pos3 = 0xff, pos4 = 0xff, pos5 = 0xff;
	int i, j;
	int adapter_found = 0;
	int adapter = 0;
	int tbase = 0;
	int tirq = 0;
	int base_addr = dev->base_addr;
	int irq = dev->irq;

	if (!MCA_bus) {
		return -ENODEV;
	}

	SET_MODULE_OWNER(dev);

	if (base_addr || irq) {
		printk(KERN_INFO "Probing for SMC MCA adapter");
		if (base_addr) {
			printk(KERN_INFO " at I/O address 0x%04x%c",
			       base_addr, irq ? ' ' : '\n');
		}
		if (irq) {
			printk(KERN_INFO "using irq %d\n", irq);
		}
	}

        /* proper multicard detection by ZP Gu (zpg@castle.net) */

	for (j = 0; (smc_mca_adapters[j].name != NULL) && !adapter_found; j++) {
		slot = mca_find_unused_adapter(smc_mca_adapters[j].id, 0);

		while((slot != MCA_NOTFOUND) && !adapter_found) {
			tirq = 0;
			tbase = 0;

                        /* If we're trying to match a specificied irq or
			 * io address, we'll reject the adapter
			 * found unless it's the one we're looking for
			 */

			pos2 = mca_read_stored_pos(slot, 2); /* io_addr */
			pos3 = mca_read_stored_pos(slot, 3); /* shared mem */
			pos4 = mca_read_stored_pos(slot, 4); /* ROM bios addr
							      * range */
			pos5 = mca_read_stored_pos(slot, 5); /* irq, media
							      * and RIPL */

			/* Test the following conditions:
			 * - If an irq parameter is supplied, compare it
			 *   with the irq of the adapter we found
			 * - If a base_addr paramater is given, compare it
			 *   with the base_addr of the adapter we found
			 * - Check that the irq and the base_addr of the
			 *   adapter we found is not already in use by
			 *   this driver
			 */

			switch (j) { /* j = card-idx (card array above) [hs] */
				case _61c8_SMC_Ethercard_PLUS_Elite_A_BNC_AUI_WD8013EP_A:
		                case _61c9_SMC_Ethercard_PLUS_Elite_A_UTP_AUI_WD8013EP_A:
				case _efd4_IBM_PS2_Adapter_A_for_Ethernet_UTP_AUI_WD8013WP_A:
				case _efd5_IBM_PS2_Adapter_A_for_Ethernet_BNC_AUI_WD8013WP_A:
				{
					tbase = addr_table[(pos2 & 0xf0) >> 4].base_addr;
					tirq  = irq_table[(pos5 & 0xc) >> 2].new_irq;
					break;
				}
				case _6fc0_WD_Ethercard_PLUS_A_WD8003E_A_OR_WD8003ET_A:
				case _6fc1_WD_Starcard_PLUS_A_WD8003ST_A:
				case _6fc2_WD_Ethercard_PLUS_10T_A_WD8003W_A:
				case _efe5_IBM_PS2_Adapter_A_for_Ethernet:
				{
					tbase = ((pos2 & 0x0fe) * 0x10);
					tirq  = irq_table[(pos5 & 3)].old_irq;
					break;
				}
			}

			if(!tirq || !tbase || (irq && irq != tirq) || (base_addr && tbase != base_addr)) {
				slot = mca_find_unused_adapter(smc_mca_adapters[j].id, slot + 1);
			} else {
				adapter_found = 1;
				adapter = j;
			}
		}
	}

	if(!adapter_found) {
		return ((base_addr || irq) ? -ENXIO : -ENODEV);
	}

        /* Adapter found. */

	printk(KERN_INFO "%s: %s found in slot %d\n",
	       dev->name, smc_mca_adapters[adapter].name, slot + 1);

	mca_set_adapter_name(slot, smc_mca_adapters[adapter].name);
	mca_mark_as_used(slot);


	dev->base_addr = ioaddr = tbase;
	dev->irq       = tirq;
	dev->mem_start = 0;
	num_pages      = 40;

	switch (adapter) {	/* card-# in const array above [hs] */
		case _61c8_SMC_Ethercard_PLUS_Elite_A_BNC_AUI_WD8013EP_A:
		case _61c9_SMC_Ethercard_PLUS_Elite_A_UTP_AUI_WD8013EP_A:
		{
			for (i = 0; i < 16; i++) { /* taking 16 counts
						    * up to 15 [hs] */
				if (mem_table[i].mem_index == (pos3 & ~MEM_MASK)) {
					dev->mem_start = mem_table[i].mem_start;
					num_pages = mem_table[i].num_pages;
				}
			}
			break;
		}
		case _6fc0_WD_Ethercard_PLUS_A_WD8003E_A_OR_WD8003ET_A:
		case _6fc1_WD_Starcard_PLUS_A_WD8003ST_A:
		case _6fc2_WD_Ethercard_PLUS_10T_A_WD8003W_A:
		case _efe5_IBM_PS2_Adapter_A_for_Ethernet:
		{
			dev->mem_start = ((pos3 & 0xfc) * 0x1000);
			num_pages = 0x40;
			break;
		}
		case _efd4_IBM_PS2_Adapter_A_for_Ethernet_UTP_AUI_WD8013WP_A:
		case _efd5_IBM_PS2_Adapter_A_for_Ethernet_BNC_AUI_WD8013WP_A:
		{
			/* courtesy of gamera@quartz.ocn.ne.jp, pos3 indicates
			 * the index of the 0x2000 step.
			 * beware different number of pages [hs]
			 */
			dev->mem_start = 0xc0000 + (0x2000 * (pos3 & 0xf));
			num_pages = 0x20 + (2 * (pos3 & 0x10));
			break;
		}
	}

	if (dev->mem_start == 0) /* sanity check, shouldn't happen */
		return -ENODEV;

	if (!request_region(ioaddr, ULTRA_IO_EXTENT, dev->name))
		return -EBUSY;

	reg4 = inb(ioaddr + 4) & 0x7f;
	outb(reg4, ioaddr + 4);

	printk(KERN_INFO "%s: Parameters: %#3x,", dev->name, ioaddr);

	for (i = 0; i < 6; i++)
		printk(KERN_INFO " %2.2X", dev->dev_addr[i] = inb(ioaddr + 8 + i));

	/* Switch from the station address to the alternate register set
	 * and read the useful registers there.
	 */

	outb(0x80 | reg4, ioaddr + 4);

	/* Enable FINE16 mode to avoid BIOS ROM width mismatches @ reboot.
	 */

	outb(0x80 | inb(ioaddr + 0x0c), ioaddr + 0x0c);

	/* Switch back to the station address register set so that
	 * the MS-DOS driver can find the card after a warm boot.
	 */

	outb(reg4, ioaddr + 4);

	/* Allocate dev->priv and fill in 8390 specific dev fields.
	 */

	if (ethdev_init(dev)) {
		printk (KERN_INFO ", no memory for dev->priv.\n");
		release_region(ioaddr, ULTRA_IO_EXTENT);
		return -ENOMEM;
	}

	/* The 8390 isn't at the base address, so fake the offset
	 */

	dev->base_addr = ioaddr + ULTRA_NIC_OFFSET;

	ei_status.name = "SMC Ultra MCA";
	ei_status.word16 = 1;
	ei_status.tx_start_page = START_PG;
	ei_status.rx_start_page = START_PG + TX_PAGES;
	ei_status.stop_page = num_pages;

	dev->rmem_start = dev->mem_start + TX_PAGES * 256;
	dev->mem_end = dev->rmem_end =
	dev->mem_start + (ei_status.stop_page - START_PG) * 256;

	printk(KERN_INFO ", IRQ %d memory %#lx-%#lx.\n",
	dev->irq, dev->mem_start, dev->mem_end - 1);

	ei_status.reset_8390 = &ultramca_reset_8390;
	ei_status.block_input = &ultramca_block_input;
	ei_status.block_output = &ultramca_block_output;
	ei_status.get_8390_hdr = &ultramca_get_8390_hdr;

	ei_status.priv = slot;

	dev->open = &ultramca_open;
	dev->stop = &ultramca_close_card;
	NS8390_init(dev, 0);

	return 0;
}

static int ultramca_open(struct net_device *dev)
{
	int ioaddr = dev->base_addr - ULTRA_NIC_OFFSET; /* ASIC addr */
	int retval;

	if ((retval = request_irq(dev->irq, ei_interrupt, 0, dev->name, dev)))
		return retval;

	outb(ULTRA_MEMENB, ioaddr); /* Enable memory */
	outb(0x80, ioaddr + 5);     /* ??? */
	outb(0x01, ioaddr + 6);     /* Enable interrupts and memory. */
	outb(0x04, ioaddr + 5);     /* ??? */

	/* Set the early receive warning level in window 0 high enough not
	 * to receive ERW interrupts.
	 */

	/* outb_p(E8390_NODMA + E8390_PAGE0, dev->base_addr);
	 * outb(0xff, dev->base_addr + EN0_ERWCNT);
	 */

	ei_open(dev);
	return 0;
}

static void ultramca_reset_8390(struct net_device *dev)
{
	int ioaddr = dev->base_addr - ULTRA_NIC_OFFSET; /* ASIC addr */

	outb(ULTRA_RESET, ioaddr);
	if (ei_debug > 1)
		printk("resetting Ultra, t=%ld...", jiffies);
	ei_status.txing = 0;

	outb(0x80, ioaddr + 5);     /* ??? */
	outb(0x01, ioaddr + 6);     /* Enable interrupts and memory. */

	if (ei_debug > 1)
		printk("reset done\n");
	return;
}

/* Grab the 8390 specific header. Similar to the block_input routine, but
 * we don't need to be concerned with ring wrap as the header will be at
 * the start of a page, so we optimize accordingly.
 */

static void ultramca_get_8390_hdr(struct net_device *dev, struct e8390_pkt_hdr *hdr, int ring_page)
{
	unsigned long hdr_start = dev->mem_start + ((ring_page - START_PG) << 8);

#ifdef notdef
	/* Officially this is what we are doing, but the readl() is faster */
	isa_memcpy_fromio(hdr, hdr_start, sizeof(struct e8390_pkt_hdr));
#else
	((unsigned int*)hdr)[0] = isa_readl(hdr_start);
#endif
}

/* Block input and output are easy on shared memory ethercards, the only
 * complication is when the ring buffer wraps.
 */

static void ultramca_block_input(struct net_device *dev, int count, struct sk_buff *skb, int ring_offset)
{
	unsigned long xfer_start = dev->mem_start + ring_offset - (START_PG << 8);

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

static void ultramca_block_output(struct net_device *dev, int count, const unsigned char *buf,
                int start_page)
{
	unsigned long shmem = dev->mem_start + ((start_page - START_PG) << 8);

	isa_memcpy_toio(shmem, buf, count);
}

static int ultramca_close_card(struct net_device *dev)
{
	int ioaddr = dev->base_addr - ULTRA_NIC_OFFSET; /* ASIC addr */

	netif_stop_queue(dev);
	
	if (ei_debug > 1)
		printk("%s: Shutting down ethercard.\n", dev->name);

	outb(0x00, ioaddr + 6);     /* Disable interrupts. */
	free_irq(dev->irq, dev);

	NS8390_init(dev, 0);
	/* We should someday disable shared memory and change to 8-bit mode
         * "just in case"...
	 */

	return 0;
}


#ifdef MODULE
#undef MODULE        /* don't want to bother now! */

#define MAX_ULTRAMCA_CARDS 4	/* Max number of Ultra cards per module */

static struct net_device dev_ultra[MAX_ULTRAMCA_CARDS];
static int io[MAX_ULTRAMCA_CARDS];
static int irq[MAX_ULTRAMCA_CARDS];
MODULE_LICENSE("GPL");

MODULE_PARM(io, "1-" __MODULE_STRING(MAX_ULTRAMCA_CARDS) "i");
MODULE_PARM(irq, "1-" __MODULE_STRING(MAX_ULTRAMCA_CARDS) "i");
MODULE_PARM_DESC(io, "SMC Ultra/EtherEZ MCA I/O base address(es)");
MODULE_PARM_DESC(irq, "SMC Ultra/EtherEZ MCA IRQ number(s)");

int init_module(void)
{
	int this_dev, found = 0;

	for (this_dev = 0; this_dev < MAX_ULTRAMCA_CARDS; this_dev++) {
		struct net_device *dev = &dev_ultra[this_dev];
		dev->irq = irq[this_dev];
		dev->base_addr = io[this_dev];
		dev->init = ultramca_probe;

		if (register_netdev(dev) != 0) {
			if (found != 0) {	/* Got at least one. */
				return 0;
			}
			printk(KERN_NOTICE "smc-mca.c: No SMC Ultra card found (i/o = 0x%x).\n", io[this_dev]);
			return -ENXIO;
		}
		found++;
	}
	return 0;
}

void cleanup_module(void)
{
	int this_dev;

	for (this_dev = 0; this_dev < MAX_ULTRAMCA_CARDS; this_dev++) {
		struct net_device *dev = &dev_ultra[this_dev];
		if (dev->priv != NULL) {
			void *priv = dev->priv;
			/* NB: ultra_close_card() does free_irq */
			int ioaddr = dev->base_addr - ULTRA_NIC_OFFSET;
			mca_mark_as_unused(ei_status.priv);
			release_region(ioaddr, ULTRA_IO_EXTENT);
			unregister_netdev(dev);
			kfree(priv);
		}
	}
}
#endif /* MODULE */

/*
 * Local variables:
 *  compile-command: "gcc -D__KERNEL__ -Wall -O6 -I/usr/src/linux/net/inet -c smc-mca.c"
 *  version-control: t
 *  kept-new-versions: 5
 *  c-indent-level: 8
 *  tab-width: 8
 * End:
 */
