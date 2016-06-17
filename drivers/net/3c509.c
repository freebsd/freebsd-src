/* 3c509.c: A 3c509 EtherLink3 ethernet driver for linux. */
/*
	Written 1993-2000 by Donald Becker.

	Copyright 1994-2000 by Donald Becker.
	Copyright 1993 United States Government as represented by the
	Director, National Security Agency.	 This software may be used and
	distributed according to the terms of the GNU General Public License,
	incorporated herein by reference.

	This driver is for the 3Com EtherLinkIII series.

	The author may be reached as becker@scyld.com, or C/O
	Scyld Computing Corporation
	410 Severn Ave., Suite 210
	Annapolis MD 21403

	Known limitations:
	Because of the way 3c509 ISA detection works it's difficult to predict
	a priori which of several ISA-mode cards will be detected first.

	This driver does not use predictive interrupt mode, resulting in higher
	packet latency but lower overhead.  If interrupts are disabled for an
	unusually long time it could also result in missed packets, but in
	practice this rarely happens.


	FIXES:
		Alan Cox:       Removed the 'Unexpected interrupt' bug.
		Michael Meskes:	Upgraded to Donald Becker's version 1.07.
		Alan Cox:	Increased the eeprom delay. Regardless of 
				what the docs say some people definitely
				get problems with lower (but in card spec)
				delays
		v1.10 4/21/97 Fixed module code so that multiple cards may be detected,
				other cleanups.  -djb
		Andrea Arcangeli:	Upgraded to Donald Becker's version 1.12.
		Rick Payne:	Fixed SMP race condition
		v1.13 9/8/97 Made 'max_interrupt_work' an insmod-settable variable -djb
		v1.14 10/15/97 Avoided waiting..discard message for fast machines -djb
		v1.15 1/31/98 Faster recovery for Tx errors. -djb
		v1.16 2/3/98 Different ID port handling to avoid sound cards. -djb
		v1.18 12Mar2001 Andrew Morton <andrewm@uow.edu.au>
			- Avoid bogus detect of 3c590's (Andrzej Krzysztofowicz)
			- Reviewed against 1.18 from scyld.com
		v1.18a 17Nov2001 Jeff Garzik <jgarzik@pobox.com>
			- ethtool support
		v1.18b 1Mar2002 Zwane Mwaikambo <zwane@commfireservices.com>
			- Power Management support
                v1.18c 1Mar2002 David Ruggiero <jdr@farfalle.com>
                        - Full duplex support
		v1.19  16Oct2002 Zwane Mwaikambo <zwane@linuxpower.ca>
			- Additional ethtool features
  */

#define DRV_NAME	"3c509"
#define DRV_VERSION	"1.19"
#define DRV_RELDATE	"16Oct2002"

/* A few values that may be tweaked. */

/* Time in jiffies before concluding the transmitter is hung. */
#define TX_TIMEOUT  (400*HZ/1000)
/* Maximum events (Rx packets, etc.) to handle at each interrupt. */
static int max_interrupt_work = 10;

#include <linux/config.h>
#include <linux/module.h>

#include <linux/mca.h>
#include <linux/isapnp.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/errno.h>
#include <linux/in.h>
#include <linux/slab.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/delay.h>	/* for udelay() */
#include <linux/spinlock.h>
#include <linux/ethtool.h>

#include <asm/uaccess.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <linux/pm.h>

static char versionA[] __initdata = DRV_NAME ".c:" DRV_VERSION " " DRV_RELDATE " becker@scyld.com\n";
static char versionB[] __initdata = "http://www.scyld.com/network/3c509.html\n";

#ifdef EL3_DEBUG
static int el3_debug = EL3_DEBUG;
#else
static int el3_debug = 2;
#endif


/* To minimize the size of the driver source I only define operating
   constants if they are used several times.  You'll need the manual
   anyway if you want to understand driver details. */
/* Offsets from base I/O address. */
#define EL3_DATA 0x00
#define EL3_CMD 0x0e
#define EL3_STATUS 0x0e
#define	 EEPROM_READ 0x80

#define EL3_IO_EXTENT	16

#define EL3WINDOW(win_num) outw(SelectWindow + (win_num), ioaddr + EL3_CMD)


/* The top five bits written to EL3_CMD are a command, the lower
   11 bits are the parameter, if applicable. */
enum c509cmd {
	TotalReset = 0<<11, SelectWindow = 1<<11, StartCoax = 2<<11,
	RxDisable = 3<<11, RxEnable = 4<<11, RxReset = 5<<11, RxDiscard = 8<<11,
	TxEnable = 9<<11, TxDisable = 10<<11, TxReset = 11<<11,
	FakeIntr = 12<<11, AckIntr = 13<<11, SetIntrEnb = 14<<11,
	SetStatusEnb = 15<<11, SetRxFilter = 16<<11, SetRxThreshold = 17<<11,
	SetTxThreshold = 18<<11, SetTxStart = 19<<11, StatsEnable = 21<<11,
	StatsDisable = 22<<11, StopCoax = 23<<11, PowerUp = 27<<11,
	PowerDown = 28<<11, PowerAuto = 29<<11};

enum c509status {
	IntLatch = 0x0001, AdapterFailure = 0x0002, TxComplete = 0x0004,
	TxAvailable = 0x0008, RxComplete = 0x0010, RxEarly = 0x0020,
	IntReq = 0x0040, StatsFull = 0x0080, CmdBusy = 0x1000, };

/* The SetRxFilter command accepts the following classes: */
enum RxFilter {
	RxStation = 1, RxMulticast = 2, RxBroadcast = 4, RxProm = 8 };

/* Register window 1 offsets, the window used in normal operation. */
#define TX_FIFO		0x00
#define RX_FIFO		0x00
#define RX_STATUS 	0x08
#define TX_STATUS 	0x0B
#define TX_FREE		0x0C		/* Remaining free bytes in Tx buffer. */

#define WN0_CONF_CTRL	0x04		/* Window 0: Configuration control register */
#define WN0_ADDR_CONF	0x06		/* Window 0: Address configuration register */
#define WN0_IRQ		0x08		/* Window 0: Set IRQ line in bits 12-15. */
#define WN4_MEDIA	0x0A		/* Window 4: Various transcvr/media bits. */
#define	MEDIA_TP	0x00C0		/* Enable link beat and jabber for 10baseT. */
#define WN4_NETDIAG	0x06		/* Window 4: Net diagnostic */
#define FD_ENABLE	0x8000		/* Enable full-duplex ("external loopback") */  

/*
 * Must be a power of two (we use a binary and in the
 * circular queue)
 */
#define SKB_QUEUE_SIZE	64

struct el3_private {
	struct net_device_stats stats;
	struct net_device *next_dev;
	spinlock_t lock;
	/* skb send-queue */
	int head, size;
	struct sk_buff *queue[SKB_QUEUE_SIZE];
	char mca_slot;
#ifdef CONFIG_PM
	struct pm_dev *pmdev;
#endif
};
static int id_port __initdata = 0x110;	/* Start with 0x110 to avoid new sound cards.*/
static struct net_device *el3_root_dev;

static ushort id_read_eeprom(int index);
static ushort read_eeprom(int ioaddr, int index);
static int el3_open(struct net_device *dev);
static int el3_start_xmit(struct sk_buff *skb, struct net_device *dev);
static void el3_interrupt(int irq, void *dev_id, struct pt_regs *regs);
static void update_stats(struct net_device *dev);
static struct net_device_stats *el3_get_stats(struct net_device *dev);
static int el3_rx(struct net_device *dev);
static int el3_close(struct net_device *dev);
static void set_multicast_list(struct net_device *dev);
static void el3_tx_timeout (struct net_device *dev);
static int netdev_ioctl (struct net_device *dev, struct ifreq *rq, int cmd);
static void el3_down(struct net_device *dev);
static void el3_up(struct net_device *dev);
#ifdef CONFIG_PM
static int el3_suspend(struct pm_dev *pdev);
static int el3_resume(struct pm_dev *pdev);
static int el3_pm_callback(struct pm_dev *pdev, pm_request_t rqst, void *data);
#endif

#ifdef CONFIG_MCA
struct el3_mca_adapters_struct {
	char* name;
	int id;
};

static struct el3_mca_adapters_struct el3_mca_adapters[] __initdata = {
	{ "3Com 3c529 EtherLink III (10base2)", 0x627c },
	{ "3Com 3c529 EtherLink III (10baseT)", 0x627d },
	{ "3Com 3c529 EtherLink III (test mode)", 0x62db },
	{ "3Com 3c529 EtherLink III (TP or coax)", 0x62f6 },
	{ "3Com 3c529 EtherLink III (TP)", 0x62f7 },
	{ NULL, 0 },
};
#endif /* CONFIG_MCA */

#if defined(CONFIG_ISAPNP) || defined(CONFIG_ISAPNP_MODULE)
static struct isapnp_device_id el3_isapnp_adapters[] __initdata = {
	{	ISAPNP_ANY_ID, ISAPNP_ANY_ID,
		ISAPNP_VENDOR('T', 'C', 'M'), ISAPNP_FUNCTION(0x5090),
		(long) "3Com Etherlink III (TP)" },
	{	ISAPNP_ANY_ID, ISAPNP_ANY_ID,
		ISAPNP_VENDOR('T', 'C', 'M'), ISAPNP_FUNCTION(0x5091),
		(long) "3Com Etherlink III" },
	{	ISAPNP_ANY_ID, ISAPNP_ANY_ID,
		ISAPNP_VENDOR('T', 'C', 'M'), ISAPNP_FUNCTION(0x5094),
		(long) "3Com Etherlink III (combo)" },
	{	ISAPNP_ANY_ID, ISAPNP_ANY_ID,
		ISAPNP_VENDOR('T', 'C', 'M'), ISAPNP_FUNCTION(0x5095),
		(long) "3Com Etherlink III (TPO)" },
	{	ISAPNP_ANY_ID, ISAPNP_ANY_ID,
		ISAPNP_VENDOR('T', 'C', 'M'), ISAPNP_FUNCTION(0x5098),
		(long) "3Com Etherlink III (TPC)" },
	{	ISAPNP_ANY_ID, ISAPNP_ANY_ID,
		ISAPNP_VENDOR('P', 'N', 'P'), ISAPNP_FUNCTION(0x80f7),
		(long) "3Com Etherlink III compatible" },
	{	ISAPNP_ANY_ID, ISAPNP_ANY_ID,
		ISAPNP_VENDOR('P', 'N', 'P'), ISAPNP_FUNCTION(0x80f8),
		(long) "3Com Etherlink III compatible" },
	{ }	/* terminate list */
};

MODULE_DEVICE_TABLE(isapnp, el3_isapnp_adapters);

static u16 el3_isapnp_phys_addr[8][3];
static int nopnp;
#endif /* CONFIG_ISAPNP || CONFIG_ISAPNP_MODULE */

int __init el3_probe(struct net_device *dev, int card_idx)
{
	struct el3_private *lp;
	short lrs_state = 0xff, i;
	int ioaddr, irq, if_port;
	u16 phys_addr[3];
	static int current_tag;
	int mca_slot = -1;
#if defined(CONFIG_ISAPNP) || defined(CONFIG_ISAPNP_MODULE)
	static int pnp_cards;
#endif /* CONFIG_ISAPNP || CONFIG_ISAPNP_MODULE */

	if (dev) SET_MODULE_OWNER(dev);

	/* First check all slots of the EISA bus.  The next slot address to
	   probe is kept in 'eisa_addr' to support multiple probe() calls. */
	if (EISA_bus) {
		static int eisa_addr = 0x1000;
		while (eisa_addr < 0x9000) {
			int device_id;

			ioaddr = eisa_addr;
			eisa_addr += 0x1000;

			/* Check the standard EISA ID register for an encoded '3Com'. */
			if (inw(ioaddr + 0xC80) != 0x6d50)
				continue;

			/* Avoid conflict with 3c590, 3c592, 3c597, etc */
			device_id = (inb(ioaddr + 0xC82)<<8) + inb(ioaddr + 0xC83);
			if ((device_id & 0xFF00) == 0x5900) {
				continue;
			}

			/* Change the register set to the configuration window 0. */
			outw(SelectWindow | 0, ioaddr + 0xC80 + EL3_CMD);

			irq = inw(ioaddr + WN0_IRQ) >> 12;
			if_port = inw(ioaddr + 6)>>14;
			for (i = 0; i < 3; i++)
				phys_addr[i] = htons(read_eeprom(ioaddr, i));

			/* Restore the "Product ID" to the EEPROM read register. */
			read_eeprom(ioaddr, 3);

			/* Was the EISA code an add-on hack?  Nahhhhh... */
			goto found;
		}
	}

#ifdef CONFIG_MCA
	/* Based on Erik Nygren's (nygren@mit.edu) 3c529 patch, heavily
	 * modified by Chris Beauregard (cpbeaure@csclub.uwaterloo.ca)
	 * to support standard MCA probing.
	 *
	 * redone for multi-card detection by ZP Gu (zpg@castle.net)
	 * now works as a module
	 */

	if( MCA_bus ) {
		int slot, j;
		u_char pos4, pos5;

		for( j = 0; el3_mca_adapters[j].name != NULL; j ++ ) {
			slot = 0;
			while( slot != MCA_NOTFOUND ) {
				slot = mca_find_unused_adapter(
					el3_mca_adapters[j].id, slot );
				if( slot == MCA_NOTFOUND ) break;

				/* if we get this far, an adapter has been
				 * detected and is enabled
				 */

				pos4 = mca_read_stored_pos( slot, 4 );
				pos5 = mca_read_stored_pos( slot, 5 );

				ioaddr = ((short)((pos4&0xfc)|0x02)) << 8;
				irq = pos5 & 0x0f;

				/* probing for a card at a particular IO/IRQ */
				if(dev && ((dev->irq >= 1 && dev->irq != irq) ||
			   	(dev->base_addr >= 1 && dev->base_addr != ioaddr))) {
					slot++;         /* probing next slot */
					continue;
				}

				printk("3c509: found %s at slot %d\n",
					el3_mca_adapters[j].name, slot + 1 );

				/* claim the slot */
				mca_set_adapter_name(slot, el3_mca_adapters[j].name);
				mca_set_adapter_procfn(slot, NULL, NULL);
				mca_mark_as_used(slot);

				if_port = pos4 & 0x03;
				if (el3_debug > 2) {
					printk("3c529: irq %d  ioaddr 0x%x  ifport %d\n", irq, ioaddr, if_port);
				}
				EL3WINDOW(0);
				for (i = 0; i < 3; i++) {
					phys_addr[i] = htons(read_eeprom(ioaddr, i));
				}
				
				mca_slot = slot;

				goto found;
			}
		}
		/* if we get here, we didn't find an MCA adapter */
		return -ENODEV;
	}
#endif /* CONFIG_MCA */

#if defined(CONFIG_ISAPNP) || defined(CONFIG_ISAPNP_MODULE)
	if (nopnp == 1)
		goto no_pnp;

	for (i=0; el3_isapnp_adapters[i].vendor != 0; i++) {
		struct pci_dev *idev = NULL;
		int j;
		while ((idev = isapnp_find_dev(NULL,
						el3_isapnp_adapters[i].vendor,
						el3_isapnp_adapters[i].function,
						idev))) {
			idev->prepare(idev);
			/* Deactivation is needed if the driver was called
			   with "nopnp=1" before, does not harm if not. */
			idev->deactivate(idev);
			idev->activate(idev);
			if (!idev->resource[0].start || check_region(idev->resource[0].start, EL3_IO_EXTENT))
				continue;
			ioaddr = idev->resource[0].start;
			if (!request_region(ioaddr, EL3_IO_EXTENT, "3c509 PnP"))
				return -EBUSY;
			irq = idev->irq_resource[0].start;
			if (el3_debug > 3)
				printk ("ISAPnP reports %s at i/o 0x%x, irq %d\n",
					(char*) el3_isapnp_adapters[i].driver_data, ioaddr, irq);
			EL3WINDOW(0);
			for (j = 0; j < 3; j++)
				el3_isapnp_phys_addr[pnp_cards][j] =
					phys_addr[j] =
						htons(read_eeprom(ioaddr, j));
			if_port = read_eeprom(ioaddr, 8) >> 14;
			pnp_cards++;
			goto found;
		}
	}
no_pnp:
#endif /* CONFIG_ISAPNP || CONFIG_ISAPNP_MODULE */

	/* Select an open I/O location at 0x1*0 to do contention select. */
	for ( ; id_port < 0x200; id_port += 0x10) {
		if (check_region(id_port, 1))
			continue;
		outb(0x00, id_port);
		outb(0xff, id_port);
		if (inb(id_port) & 0x01)
			break;
	}
	if (id_port >= 0x200) {
		/* Rare -- do we really need a warning? */
		printk(" WARNING: No I/O port available for 3c509 activation.\n");
		return -ENODEV;
	}
	/* Next check for all ISA bus boards by sending the ID sequence to the
	   ID_PORT.  We find cards past the first by setting the 'current_tag'
	   on cards as they are found.  Cards with their tag set will not
	   respond to subsequent ID sequences. */

	outb(0x00, id_port);
	outb(0x00, id_port);
	for(i = 0; i < 255; i++) {
		outb(lrs_state, id_port);
		lrs_state <<= 1;
		lrs_state = lrs_state & 0x100 ? lrs_state ^ 0xcf : lrs_state;
	}

	/* For the first probe, clear all board's tag registers. */
	if (current_tag == 0)
		outb(0xd0, id_port);
	else				/* Otherwise kill off already-found boards. */
		outb(0xd8, id_port);

	if (id_read_eeprom(7) != 0x6d50) {
		return -ENODEV;
	}

	/* Read in EEPROM data, which does contention-select.
	   Only the lowest address board will stay "on-line".
	   3Com got the byte order backwards. */
	for (i = 0; i < 3; i++) {
		phys_addr[i] = htons(id_read_eeprom(i));
	}

#if defined(CONFIG_ISAPNP) || defined(CONFIG_ISAPNP_MODULE)
	if (nopnp == 0) {
		/* The ISA PnP 3c509 cards respond to the ID sequence.
		   This check is needed in order not to register them twice. */
		for (i = 0; i < pnp_cards; i++) {
			if (phys_addr[0] == el3_isapnp_phys_addr[i][0] &&
			    phys_addr[1] == el3_isapnp_phys_addr[i][1] &&
			    phys_addr[2] == el3_isapnp_phys_addr[i][2])
			{
				if (el3_debug > 3)
					printk("3c509 with address %02x %02x %02x %02x %02x %02x was found by ISAPnP\n",
						phys_addr[0] & 0xff, phys_addr[0] >> 8,
						phys_addr[1] & 0xff, phys_addr[1] >> 8,
						phys_addr[2] & 0xff, phys_addr[2] >> 8);
				/* Set the adaptor tag so that the next card can be found. */
				outb(0xd0 + ++current_tag, id_port);
				goto no_pnp;
			}
		}
	}
#endif /* CONFIG_ISAPNP || CONFIG_ISAPNP_MODULE */

	{
		unsigned int iobase = id_read_eeprom(8);
		if_port = iobase >> 14;
		ioaddr = 0x200 + ((iobase & 0x1f) << 4);
	}
	irq = id_read_eeprom(9) >> 12;

	if (dev) {					/* Set passed-in IRQ or I/O Addr. */
		if (dev->irq > 1  &&  dev->irq < 16)
			irq = dev->irq;

		if (dev->base_addr) {
			if (dev->mem_end == 0x3c509 			/* Magic key */
				&& dev->base_addr >= 0x200  &&  dev->base_addr <= 0x3e0)
				ioaddr = dev->base_addr & 0x3f0;
			else if (dev->base_addr != ioaddr)
				return -ENODEV;
		}
	}

	if (!request_region(ioaddr, EL3_IO_EXTENT, "3c509"))
		return -EBUSY;

	/* Set the adaptor tag so that the next card can be found. */
	outb(0xd0 + ++current_tag, id_port);

	/* Activate the adaptor at the EEPROM location. */
	outb((ioaddr >> 4) | 0xe0, id_port);

	EL3WINDOW(0);
	if (inw(ioaddr) != 0x6d50) {
		release_region(ioaddr, EL3_IO_EXTENT);
		return -ENODEV;
	}

	/* Free the interrupt so that some other card can use it. */
	outw(0x0f00, ioaddr + WN0_IRQ);
 found:
	if (dev == NULL) {
		dev = init_etherdev(dev, sizeof(struct el3_private));
		if (dev == NULL) {
			release_region(ioaddr, EL3_IO_EXTENT);
			return -ENOMEM;
		}
		SET_MODULE_OWNER(dev);
	}
	memcpy(dev->dev_addr, phys_addr, sizeof(phys_addr));
	dev->base_addr = ioaddr;
	dev->irq = irq;
        
	if (dev->mem_start & 0x05) { /* xcvr codes 1/3/4/12 */
		dev->if_port = (dev->mem_start & 0x0f);
        } else { /* xcvr codes 0/8 */
		/* use eeprom value, but save user's full-duplex selection */
		dev->if_port = (if_port | (dev->mem_start & 0x08) );
	}

	{
		const char *if_names[] = {"10baseT", "AUI", "undefined", "BNC"};
		printk("%s: 3c5x9 at %#3.3lx, %s port, address ",
			dev->name, dev->base_addr, if_names[(dev->if_port & 0x03)]);
	}

	/* Read in the station address. */
	for (i = 0; i < 6; i++)
		printk(" %2.2x", dev->dev_addr[i]);
	printk(", IRQ %d.\n", dev->irq);

	/* Make up a EL3-specific-data structure. */
	if (dev->priv == NULL)
		dev->priv = kmalloc(sizeof(struct el3_private), GFP_KERNEL);
	if (dev->priv == NULL)
		return -ENOMEM;
	memset(dev->priv, 0, sizeof(struct el3_private));
	
	lp = dev->priv;
	lp->mca_slot = mca_slot;
	lp->next_dev = el3_root_dev;
	spin_lock_init(&lp->lock);
	el3_root_dev = dev;

	if (el3_debug > 0)
		printk(KERN_INFO "%s" KERN_INFO "%s", versionA, versionB);

	/* The EL3-specific entries in the device structure. */
	dev->open = &el3_open;
	dev->hard_start_xmit = &el3_start_xmit;
	dev->stop = &el3_close;
	dev->get_stats = &el3_get_stats;
	dev->set_multicast_list = &set_multicast_list;
	dev->tx_timeout = el3_tx_timeout;
	dev->watchdog_timeo = TX_TIMEOUT;
	dev->do_ioctl = netdev_ioctl;

#ifdef CONFIG_PM
	/* register power management */
	lp->pmdev = pm_register(PM_ISA_DEV, card_idx, el3_pm_callback);
	if (lp->pmdev) {
		struct pm_dev *p;
		p = lp->pmdev;
		p->data = (struct net_device *)dev;
	}
#endif

	/* Fill in the generic fields of the device structure. */
	ether_setup(dev);
	return 0;
}

/* Read a word from the EEPROM using the regular EEPROM access register.
   Assume that we are in register window zero.
 */
static ushort read_eeprom(int ioaddr, int index)
{
	outw(EEPROM_READ + index, ioaddr + 10);
	/* Pause for at least 162 us. for the read to take place. */
	udelay (500);
	return inw(ioaddr + 12);
}

/* Read a word from the EEPROM when in the ISA ID probe state. */
static ushort __init id_read_eeprom(int index)
{
	int bit, word = 0;

	/* Issue read command, and pause for at least 162 us. for it to complete.
	   Assume extra-fast 16Mhz bus. */
	outb(EEPROM_READ + index, id_port);

	/* Pause for at least 162 us. for the read to take place. */
	udelay (500);
	
	for (bit = 15; bit >= 0; bit--)
		word = (word << 1) + (inb(id_port) & 0x01);

	if (el3_debug > 3)
		printk("  3c509 EEPROM word %d %#4.4x.\n", index, word);

	return word;
}


static int
el3_open(struct net_device *dev)
{
	int ioaddr = dev->base_addr;
	int i;

	outw(TxReset, ioaddr + EL3_CMD);
	outw(RxReset, ioaddr + EL3_CMD);
	outw(SetStatusEnb | 0x00, ioaddr + EL3_CMD);

	i = request_irq(dev->irq, &el3_interrupt, 0, dev->name, dev);
	if (i) return i;

	EL3WINDOW(0);
	if (el3_debug > 3)
		printk("%s: Opening, IRQ %d	 status@%x %4.4x.\n", dev->name,
			   dev->irq, ioaddr + EL3_STATUS, inw(ioaddr + EL3_STATUS));

	el3_up(dev);

	if (el3_debug > 3)
		printk("%s: Opened 3c509  IRQ %d  status %4.4x.\n",
			   dev->name, dev->irq, inw(ioaddr + EL3_STATUS));

	return 0;
}

static void
el3_tx_timeout (struct net_device *dev)
{
	struct el3_private *lp = (struct el3_private *)dev->priv;
	int ioaddr = dev->base_addr;

	/* Transmitter timeout, serious problems. */
	printk("%s: transmit timed out, Tx_status %2.2x status %4.4x "
		   "Tx FIFO room %d.\n",
		   dev->name, inb(ioaddr + TX_STATUS), inw(ioaddr + EL3_STATUS),
		   inw(ioaddr + TX_FREE));
	lp->stats.tx_errors++;
	dev->trans_start = jiffies;
	/* Issue TX_RESET and TX_START commands. */
	outw(TxReset, ioaddr + EL3_CMD);
	outw(TxEnable, ioaddr + EL3_CMD);
	netif_wake_queue(dev);
}


static int
el3_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct el3_private *lp = (struct el3_private *)dev->priv;
	int ioaddr = dev->base_addr;
	unsigned long flags;

	netif_stop_queue (dev);

	lp->stats.tx_bytes += skb->len;
	
	if (el3_debug > 4) {
		printk("%s: el3_start_xmit(length = %u) called, status %4.4x.\n",
			   dev->name, skb->len, inw(ioaddr + EL3_STATUS));
	}
#if 0
#ifndef final_version
	{	/* Error-checking code, delete someday. */
		ushort status = inw(ioaddr + EL3_STATUS);
		if (status & 0x0001 		/* IRQ line active, missed one. */
			&& inw(ioaddr + EL3_STATUS) & 1) { 			/* Make sure. */
			printk("%s: Missed interrupt, status then %04x now %04x"
				   "  Tx %2.2x Rx %4.4x.\n", dev->name, status,
				   inw(ioaddr + EL3_STATUS), inb(ioaddr + TX_STATUS),
				   inw(ioaddr + RX_STATUS));
			/* Fake interrupt trigger by masking, acknowledge interrupts. */
			outw(SetStatusEnb | 0x00, ioaddr + EL3_CMD);
			outw(AckIntr | IntLatch | TxAvailable | RxEarly | IntReq,
				 ioaddr + EL3_CMD);
			outw(SetStatusEnb | 0xff, ioaddr + EL3_CMD);
		}
	}
#endif
#endif
	/*
	 *	We lock the driver against other processors. Note
	 *	we don't need to lock versus the IRQ as we suspended
	 *	that. This means that we lose the ability to take
	 *	an RX during a TX upload. That sucks a bit with SMP
	 *	on an original 3c509 (2K buffer)
	 *
	 *	Using disable_irq stops us crapping on other
	 *	time sensitive devices.
	 */

    	spin_lock_irqsave(&lp->lock, flags);
	    
	/* Put out the doubleword header... */
	outw(skb->len, ioaddr + TX_FIFO);
	outw(0x00, ioaddr + TX_FIFO);
	/* ... and the packet rounded to a doubleword. */
#ifdef  __powerpc__
	outsl_ns(ioaddr + TX_FIFO, skb->data, (skb->len + 3) >> 2);
#else
	outsl(ioaddr + TX_FIFO, skb->data, (skb->len + 3) >> 2);
#endif

	dev->trans_start = jiffies;
	if (inw(ioaddr + TX_FREE) > 1536)
		netif_start_queue(dev);
	else
		/* Interrupt us when the FIFO has room for max-sized packet. */
		outw(SetTxThreshold + 1536, ioaddr + EL3_CMD);

	spin_unlock_irqrestore(&lp->lock, flags);

	dev_kfree_skb (skb);

	/* Clear the Tx status stack. */
	{
		short tx_status;
		int i = 4;

		while (--i > 0	&&	(tx_status = inb(ioaddr + TX_STATUS)) > 0) {
			if (tx_status & 0x38) lp->stats.tx_aborted_errors++;
			if (tx_status & 0x30) outw(TxReset, ioaddr + EL3_CMD);
			if (tx_status & 0x3C) outw(TxEnable, ioaddr + EL3_CMD);
			outb(0x00, ioaddr + TX_STATUS); /* Pop the status stack. */
		}
	}
	return 0;
}

/* The EL3 interrupt handler. */
static void
el3_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct net_device *dev = (struct net_device *)dev_id;
	struct el3_private *lp;
	int ioaddr, status;
	int i = max_interrupt_work;

	if (dev == NULL) {
		printk ("el3_interrupt(): irq %d for unknown device.\n", irq);
		return;
	}

	lp = (struct el3_private *)dev->priv;
	spin_lock(&lp->lock);

	ioaddr = dev->base_addr;

	if (el3_debug > 4) {
		status = inw(ioaddr + EL3_STATUS);
		printk("%s: interrupt, status %4.4x.\n", dev->name, status);
	}

	while ((status = inw(ioaddr + EL3_STATUS)) &
		   (IntLatch | RxComplete | StatsFull)) {

		if (status & RxComplete)
			el3_rx(dev);

		if (status & TxAvailable) {
			if (el3_debug > 5)
				printk("	TX room bit was handled.\n");
			/* There's room in the FIFO for a full-sized packet. */
			outw(AckIntr | TxAvailable, ioaddr + EL3_CMD);
			netif_wake_queue (dev);
		}
		if (status & (AdapterFailure | RxEarly | StatsFull | TxComplete)) {
			/* Handle all uncommon interrupts. */
			if (status & StatsFull)				/* Empty statistics. */
				update_stats(dev);
			if (status & RxEarly) {				/* Rx early is unused. */
				el3_rx(dev);
				outw(AckIntr | RxEarly, ioaddr + EL3_CMD);
			}
			if (status & TxComplete) {			/* Really Tx error. */
				struct el3_private *lp = (struct el3_private *)dev->priv;
				short tx_status;
				int i = 4;

				while (--i>0 && (tx_status = inb(ioaddr + TX_STATUS)) > 0) {
					if (tx_status & 0x38) lp->stats.tx_aborted_errors++;
					if (tx_status & 0x30) outw(TxReset, ioaddr + EL3_CMD);
					if (tx_status & 0x3C) outw(TxEnable, ioaddr + EL3_CMD);
					outb(0x00, ioaddr + TX_STATUS); /* Pop the status stack. */
				}
			}
			if (status & AdapterFailure) {
				/* Adapter failure requires Rx reset and reinit. */
				outw(RxReset, ioaddr + EL3_CMD);
				/* Set the Rx filter to the current state. */
				outw(SetRxFilter | RxStation | RxBroadcast
					 | (dev->flags & IFF_ALLMULTI ? RxMulticast : 0)
					 | (dev->flags & IFF_PROMISC ? RxProm : 0),
					 ioaddr + EL3_CMD);
				outw(RxEnable, ioaddr + EL3_CMD); /* Re-enable the receiver. */
				outw(AckIntr | AdapterFailure, ioaddr + EL3_CMD);
			}
		}

		if (--i < 0) {
			printk("%s: Infinite loop in interrupt, status %4.4x.\n",
				   dev->name, status);
			/* Clear all interrupts. */
			outw(AckIntr | 0xFF, ioaddr + EL3_CMD);
			break;
		}
		/* Acknowledge the IRQ. */
		outw(AckIntr | IntReq | IntLatch, ioaddr + EL3_CMD); /* Ack IRQ */
	}

	if (el3_debug > 4) {
		printk("%s: exiting interrupt, status %4.4x.\n", dev->name,
			   inw(ioaddr + EL3_STATUS));
	}
	spin_unlock(&lp->lock);
	return;
}


static struct net_device_stats *
el3_get_stats(struct net_device *dev)
{
	struct el3_private *lp = (struct el3_private *)dev->priv;
	unsigned long flags;

	/*
	 *	This is fast enough not to bother with disable IRQ
	 *	stuff.
	 */
	 
	spin_lock_irqsave(&lp->lock, flags);
	update_stats(dev);
	spin_unlock_irqrestore(&lp->lock, flags);
	return &lp->stats;
}

/*  Update statistics.  We change to register window 6, so this should be run
	single-threaded if the device is active. This is expected to be a rare
	operation, and it's simpler for the rest of the driver to assume that
	window 1 is always valid rather than use a special window-state variable.
	*/
static void update_stats(struct net_device *dev)
{
	struct el3_private *lp = (struct el3_private *)dev->priv;
	int ioaddr = dev->base_addr;

	if (el3_debug > 5)
		printk("   Updating the statistics.\n");
	/* Turn off statistics updates while reading. */
	outw(StatsDisable, ioaddr + EL3_CMD);
	/* Switch to the stats window, and read everything. */
	EL3WINDOW(6);
	lp->stats.tx_carrier_errors 	+= inb(ioaddr + 0);
	lp->stats.tx_heartbeat_errors	+= inb(ioaddr + 1);
	/* Multiple collisions. */	   inb(ioaddr + 2);
	lp->stats.collisions		+= inb(ioaddr + 3);
	lp->stats.tx_window_errors	+= inb(ioaddr + 4);
	lp->stats.rx_fifo_errors	+= inb(ioaddr + 5);
	lp->stats.tx_packets		+= inb(ioaddr + 6);
	/* Rx packets	*/		   inb(ioaddr + 7);
	/* Tx deferrals */		   inb(ioaddr + 8);
	inw(ioaddr + 10);	/* Total Rx and Tx octets. */
	inw(ioaddr + 12);

	/* Back to window 1, and turn statistics back on. */
	EL3WINDOW(1);
	outw(StatsEnable, ioaddr + EL3_CMD);
	return;
}

static int
el3_rx(struct net_device *dev)
{
	struct el3_private *lp = (struct el3_private *)dev->priv;
	int ioaddr = dev->base_addr;
	short rx_status;

	if (el3_debug > 5)
		printk("   In rx_packet(), status %4.4x, rx_status %4.4x.\n",
			   inw(ioaddr+EL3_STATUS), inw(ioaddr+RX_STATUS));
	while ((rx_status = inw(ioaddr + RX_STATUS)) > 0) {
		if (rx_status & 0x4000) { /* Error, update stats. */
			short error = rx_status & 0x3800;

			outw(RxDiscard, ioaddr + EL3_CMD);
			lp->stats.rx_errors++;
			switch (error) {
			case 0x0000:		lp->stats.rx_over_errors++; break;
			case 0x0800:		lp->stats.rx_length_errors++; break;
			case 0x1000:		lp->stats.rx_frame_errors++; break;
			case 0x1800:		lp->stats.rx_length_errors++; break;
			case 0x2000:		lp->stats.rx_frame_errors++; break;
			case 0x2800:		lp->stats.rx_crc_errors++; break;
			}
		} else {
			short pkt_len = rx_status & 0x7ff;
			struct sk_buff *skb;

			skb = dev_alloc_skb(pkt_len+5);
			lp->stats.rx_bytes += pkt_len;
			if (el3_debug > 4)
				printk("Receiving packet size %d status %4.4x.\n",
					   pkt_len, rx_status);
			if (skb != NULL) {
				skb->dev = dev;
				skb_reserve(skb, 2);     /* Align IP on 16 byte */

				/* 'skb->data' points to the start of sk_buff data area. */
#ifdef  __powerpc__
				insl_ns(ioaddr+RX_FIFO, skb_put(skb,pkt_len),
							   (pkt_len + 3) >> 2);
#else
				insl(ioaddr + RX_FIFO, skb_put(skb,pkt_len),
					 (pkt_len + 3) >> 2);
#endif

				outw(RxDiscard, ioaddr + EL3_CMD); /* Pop top Rx packet. */
				skb->protocol = eth_type_trans(skb,dev);
				netif_rx(skb);
				dev->last_rx = jiffies;
				lp->stats.rx_packets++;
				continue;
			}
			outw(RxDiscard, ioaddr + EL3_CMD);
			lp->stats.rx_dropped++;
			if (el3_debug)
				printk("%s: Couldn't allocate a sk_buff of size %d.\n",
					   dev->name, pkt_len);
		}
		inw(ioaddr + EL3_STATUS); 				/* Delay. */
		while (inw(ioaddr + EL3_STATUS) & 0x1000)
			printk(KERN_DEBUG "	Waiting for 3c509 to discard packet, status %x.\n",
				   inw(ioaddr + EL3_STATUS) );
	}

	return 0;
}

/*
 *     Set or clear the multicast filter for this adaptor.
 */
static void
set_multicast_list(struct net_device *dev)
{
	unsigned long flags;
	struct el3_private *lp = (struct el3_private *)dev->priv;
	int ioaddr = dev->base_addr;

	if (el3_debug > 1) {
		static int old;
		if (old != dev->mc_count) {
			old = dev->mc_count;
			printk("%s: Setting Rx mode to %d addresses.\n", dev->name, dev->mc_count);
		}
	}
	spin_lock_irqsave(&lp->lock, flags);
	if (dev->flags&IFF_PROMISC) {
		outw(SetRxFilter | RxStation | RxMulticast | RxBroadcast | RxProm,
			 ioaddr + EL3_CMD);
	}
	else if (dev->mc_count || (dev->flags&IFF_ALLMULTI)) {
		outw(SetRxFilter | RxStation | RxMulticast | RxBroadcast, ioaddr + EL3_CMD);
	}
	else
                outw(SetRxFilter | RxStation | RxBroadcast, ioaddr + EL3_CMD);
	spin_unlock_irqrestore(&lp->lock, flags);
}

static int
el3_close(struct net_device *dev)
{
	int ioaddr = dev->base_addr;

	if (el3_debug > 2)
		printk("%s: Shutting down ethercard.\n", dev->name);

	el3_down(dev);

	free_irq(dev->irq, dev);
	/* Switching back to window 0 disables the IRQ. */
	EL3WINDOW(0);
	/* But we explicitly zero the IRQ line select anyway. */
	outw(0x0f00, ioaddr + WN0_IRQ);

	return 0;
}

static int 
el3_link_ok(struct net_device *dev)
{
	int ioaddr = dev->base_addr;
	u16 tmp;

	EL3WINDOW(4);
	tmp = inw(ioaddr + WN4_MEDIA);
	EL3WINDOW(1);
	return tmp & (1<<11);
}

static int
el3_netdev_get_ecmd(struct net_device *dev, struct ethtool_cmd *ecmd)
{
	u16 tmp;
	int ioaddr = dev->base_addr;
	
	EL3WINDOW(0);
	/* obtain current tranceiver via WN4_MEDIA? */	
	tmp = inw(ioaddr + WN0_ADDR_CONF);
	ecmd->transceiver = XCVR_INTERNAL;
	switch (tmp >> 14) {
	case 0:
		ecmd->port = PORT_TP;
		break;
	case 1:
		ecmd->port = PORT_AUI;
		ecmd->transceiver = XCVR_EXTERNAL;
		break;
	case 3:
		ecmd->port = PORT_BNC;
	default:
		break;
	}

	ecmd->duplex = DUPLEX_HALF;
	ecmd->supported = 0;
	tmp = inw(ioaddr + WN0_CONF_CTRL);
	if (tmp & (1<<13))
		ecmd->supported |= SUPPORTED_AUI;
	if (tmp & (1<<12))
		ecmd->supported |= SUPPORTED_BNC;
	if (tmp & (1<<9)) {
		ecmd->supported |= SUPPORTED_TP | SUPPORTED_10baseT_Half |
				SUPPORTED_10baseT_Full;	/* hmm... */
		EL3WINDOW(4);
		tmp = inw(ioaddr + WN4_NETDIAG);
		if (tmp & FD_ENABLE)
			ecmd->duplex = DUPLEX_FULL;
	}

	ecmd->speed = SPEED_10;
	EL3WINDOW(1);
	return 0;
}

static int
el3_netdev_set_ecmd(struct net_device *dev, struct ethtool_cmd *ecmd)
{
	u16 tmp;
	int ioaddr = dev->base_addr;

	if (ecmd->speed != SPEED_10)
		return -EINVAL;
	if ((ecmd->duplex != DUPLEX_HALF) && (ecmd->duplex != DUPLEX_FULL))
		return -EINVAL;
	if ((ecmd->transceiver != XCVR_INTERNAL) && (ecmd->transceiver != XCVR_EXTERNAL))
		return -EINVAL;

	/* change XCVR type */
	EL3WINDOW(0);
	tmp = inw(ioaddr + WN0_ADDR_CONF);
	switch (ecmd->port) {
	case PORT_TP:
		tmp &= ~(3<<14);
		dev->if_port = 0;
		break;
	case PORT_AUI:
		tmp |= (1<<14);
		dev->if_port = 1;
		break;
	case PORT_BNC:
		tmp |= (3<<14);
		dev->if_port = 3;
		break;
	default:
		return -EINVAL;
	}

	outw(tmp, ioaddr + WN0_ADDR_CONF);
	if (dev->if_port == 3) {
		/* fire up the DC-DC convertor if BNC gets enabled */
		tmp = inw(ioaddr + WN0_ADDR_CONF);
		if (tmp & (3 << 14)) {
			outw(StartCoax, ioaddr + EL3_CMD);
			udelay(800);
		} else
			return -EIO;
	}

	EL3WINDOW(4);
	tmp = inw(ioaddr + WN4_NETDIAG);
	if (ecmd->duplex == DUPLEX_FULL)
		tmp |= FD_ENABLE;
	else
		tmp &= ~FD_ENABLE;
	outw(tmp, ioaddr + WN4_NETDIAG);
	EL3WINDOW(1);

	return 0;
}

/**
 * netdev_ethtool_ioctl: Handle network interface SIOCETHTOOL ioctls
 * @dev: network interface on which out-of-band action is to be performed
 * @useraddr: userspace address to which data is to be read and returned
 *
 * Process the various commands of the SIOCETHTOOL interface.
 */

static int
netdev_ethtool_ioctl (struct net_device *dev, void *useraddr)
{
	u32 ethcmd;
	struct el3_private *lp = dev->priv;

	/* dev_ioctl() in ../../net/core/dev.c has already checked
	   capable(CAP_NET_ADMIN), so don't bother with that here.  */

	if (get_user(ethcmd, (u32 *)useraddr))
		return -EFAULT;

	switch (ethcmd) {

	case ETHTOOL_GDRVINFO: {
		struct ethtool_drvinfo info = { ETHTOOL_GDRVINFO };
		strcpy (info.driver, DRV_NAME);
		strcpy (info.version, DRV_VERSION);
		if (copy_to_user (useraddr, &info, sizeof (info)))
			return -EFAULT;
		return 0;
	}

	/* get settings */
	case ETHTOOL_GSET: {
		int ret;
		struct ethtool_cmd ecmd = { ETHTOOL_GSET };
		spin_lock_irq(&lp->lock);
		ret = el3_netdev_get_ecmd(dev, &ecmd);
		spin_unlock_irq(&lp->lock);
		if (copy_to_user(useraddr, &ecmd, sizeof(ecmd)))
			return -EFAULT;
		return ret;
	}

	/* set settings */
	case ETHTOOL_SSET: {
		int ret;
		struct ethtool_cmd ecmd;
		if (copy_from_user(&ecmd, useraddr, sizeof(ecmd)))
			return -EFAULT;
		spin_lock_irq(&lp->lock);
		ret = el3_netdev_set_ecmd(dev, &ecmd);
		spin_unlock_irq(&lp->lock);
		return ret;
	}

	/* get link status */
	case ETHTOOL_GLINK: {
		struct ethtool_value edata = { ETHTOOL_GLINK };
		spin_lock_irq(&lp->lock);
		edata.data = el3_link_ok(dev);
		spin_unlock_irq(&lp->lock);
		if (copy_to_user(useraddr, &edata, sizeof(edata)))
			return -EFAULT;
		return 0;
	}

	/* get message-level */
	case ETHTOOL_GMSGLVL: {
		struct ethtool_value edata = {ETHTOOL_GMSGLVL};
		edata.data = el3_debug;
		if (copy_to_user(useraddr, &edata, sizeof(edata)))
			return -EFAULT;
		return 0;
	}
	/* set message-level */
	case ETHTOOL_SMSGLVL: {
		struct ethtool_value edata;
		if (copy_from_user(&edata, useraddr, sizeof(edata)))
			return -EFAULT;
		el3_debug = edata.data;
		return 0;
	}

	default:
		break;
	}

	return -EOPNOTSUPP;
}

/**
 * netdev_ioctl: Handle network interface ioctls
 * @dev: network interface on which out-of-band action is to be performed
 * @rq: user request data
 * @cmd: command issued by user
 *
 * Process the various out-of-band ioctls passed to this driver.
 */

static int
netdev_ioctl (struct net_device *dev, struct ifreq *rq, int cmd)
{
	int rc = 0;

	switch (cmd) {
	case SIOCETHTOOL:
		rc = netdev_ethtool_ioctl(dev, (void *) rq->ifr_data);
		break;

	default:
		rc = -EOPNOTSUPP;
		break;
	}

	return rc;
}

static void
el3_down(struct net_device *dev)
{
	int ioaddr = dev->base_addr;

	netif_stop_queue(dev);

	/* Turn off statistics ASAP.  We update lp->stats below. */
	outw(StatsDisable, ioaddr + EL3_CMD);

	/* Disable the receiver and transmitter. */
	outw(RxDisable, ioaddr + EL3_CMD);
	outw(TxDisable, ioaddr + EL3_CMD);

	if (dev->if_port == 3)
		/* Turn off thinnet power.  Green! */
		outw(StopCoax, ioaddr + EL3_CMD);
	else if (dev->if_port == 0) {
		/* Disable link beat and jabber, if_port may change here next open(). */
		EL3WINDOW(4);
		outw(inw(ioaddr + WN4_MEDIA) & ~MEDIA_TP, ioaddr + WN4_MEDIA);
	}

	outw(SetIntrEnb | 0x0000, ioaddr + EL3_CMD);

	update_stats(dev);
}

static void
el3_up(struct net_device *dev)
{
	int i, sw_info, net_diag;
	int ioaddr = dev->base_addr;
	
	/* Activating the board required and does no harm otherwise */
	outw(0x0001, ioaddr + 4);

	/* Set the IRQ line. */
	outw((dev->irq << 12) | 0x0f00, ioaddr + WN0_IRQ);

	/* Set the station address in window 2 each time opened. */
	EL3WINDOW(2);

	for (i = 0; i < 6; i++)
		outb(dev->dev_addr[i], ioaddr + i);

	if ((dev->if_port & 0x03) == 3) /* BNC interface */
		/* Start the thinnet transceiver. We should really wait 50ms...*/
		outw(StartCoax, ioaddr + EL3_CMD);
	else if ((dev->if_port & 0x03) == 0) { /* 10baseT interface */
		/* Combine secondary sw_info word (the adapter level) and primary
			sw_info word (duplex setting plus other useless bits) */
		EL3WINDOW(0);
		sw_info = (read_eeprom(ioaddr, 0x14) & 0x400f) | 
			(read_eeprom(ioaddr, 0x0d) & 0xBff0);

		EL3WINDOW(4);
		net_diag = inw(ioaddr + WN4_NETDIAG);
		net_diag = (net_diag | FD_ENABLE); /* temporarily assume full-duplex will be set */
		printk("%s: ", dev->name);
		switch (dev->if_port & 0x0c) {
			case 12:
				/* force full-duplex mode if 3c5x9b */
				if (sw_info & 0x000f) {
					printk("Forcing 3c5x9b full-duplex mode");
					break;
				}
			case 8:
				/* set full-duplex mode based on eeprom config setting */
				if ((sw_info & 0x000f) && (sw_info & 0x8000)) {
					printk("Setting 3c5x9b full-duplex mode (from EEPROM configuration bit)");
					break;
				}
			default:
				/* xcvr=(0 || 4) OR user has an old 3c5x9 non "B" model */
				printk("Setting 3c5x9/3c5x9B half-duplex mode");
				net_diag = (net_diag & ~FD_ENABLE); /* disable full duplex */
		}

		outw(net_diag, ioaddr + WN4_NETDIAG);
		printk(" if_port: %d, sw_info: %4.4x\n", dev->if_port, sw_info);
		if (el3_debug > 3)
			printk("%s: 3c5x9 net diag word is now: %4.4x.\n", dev->name, net_diag);
		/* Enable link beat and jabber check. */
		outw(inw(ioaddr + WN4_MEDIA) | MEDIA_TP, ioaddr + WN4_MEDIA);
	}

	/* Switch to the stats window, and clear all stats by reading. */
	outw(StatsDisable, ioaddr + EL3_CMD);
	EL3WINDOW(6);
	for (i = 0; i < 9; i++)
		inb(ioaddr + i);
	inw(ioaddr + 10);
	inw(ioaddr + 12);

	/* Switch to register set 1 for normal use. */
	EL3WINDOW(1);

	/* Accept b-case and phys addr only. */
	outw(SetRxFilter | RxStation | RxBroadcast, ioaddr + EL3_CMD);
	outw(StatsEnable, ioaddr + EL3_CMD); /* Turn on statistics. */

	outw(RxEnable, ioaddr + EL3_CMD); /* Enable the receiver. */
	outw(TxEnable, ioaddr + EL3_CMD); /* Enable transmitter. */
	/* Allow status bits to be seen. */
	outw(SetStatusEnb | 0xff, ioaddr + EL3_CMD);
	/* Ack all pending events, and set active indicator mask. */
	outw(AckIntr | IntLatch | TxAvailable | RxEarly | IntReq,
		 ioaddr + EL3_CMD);
	outw(SetIntrEnb | IntLatch|TxAvailable|TxComplete|RxComplete|StatsFull,
		 ioaddr + EL3_CMD);

	netif_start_queue(dev);
}

/* Power Management support functions */
#ifdef CONFIG_PM

static int
el3_suspend(struct pm_dev *pdev)
{
	unsigned long flags;
	struct net_device *dev;
	struct el3_private *lp;
	int ioaddr;
	
	if (!pdev && !pdev->data)
		return -EINVAL;

	dev = (struct net_device *)pdev->data;
	lp = (struct el3_private *)dev->priv;
	ioaddr = dev->base_addr;

	spin_lock_irqsave(&lp->lock, flags);

	if (netif_running(dev))
		netif_device_detach(dev);

	el3_down(dev);
	outw(PowerDown, ioaddr + EL3_CMD);

	spin_unlock_irqrestore(&lp->lock, flags);
	return 0;
}

static int
el3_resume(struct pm_dev *pdev)
{
	unsigned long flags;
	struct net_device *dev;
	struct el3_private *lp;
	int ioaddr;
	
	if (!pdev && !pdev->data)
		return -EINVAL;

	dev = (struct net_device *)pdev->data;
	lp = (struct el3_private *)dev->priv;
	ioaddr = dev->base_addr;

	spin_lock_irqsave(&lp->lock, flags);

	outw(PowerUp, ioaddr + EL3_CMD);
	el3_up(dev);

	if (netif_running(dev))
		netif_device_attach(dev);
		
	spin_unlock_irqrestore(&lp->lock, flags);
	return 0;
}

static int
el3_pm_callback(struct pm_dev *pdev, pm_request_t rqst, void *data)
{
	switch (rqst) {
		case PM_SUSPEND:
			return el3_suspend(pdev);

		case PM_RESUME:
			return el3_resume(pdev);
	}
	return 0;
}

#endif /* CONFIG_PM */

#ifdef MODULE
/* Parameters that may be passed into the module. */
static int debug = -1;
static int irq[] = {-1, -1, -1, -1, -1, -1, -1, -1};
static int xcvr[] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};

MODULE_PARM(debug,"i");
MODULE_PARM(irq,"1-8i");
MODULE_PARM(xcvr,"1-12i");
MODULE_PARM(max_interrupt_work, "i");
MODULE_PARM_DESC(debug, "debug level (0-6)");
MODULE_PARM_DESC(irq, "IRQ number(s) (assigned)");
MODULE_PARM_DESC(xcvr,"tranceiver(s) (0=internal, 1=external)");
MODULE_PARM_DESC(max_interrupt_work, "maximum events handled per interrupt");
#ifdef CONFIG_ISAPNP
MODULE_PARM(nopnp, "i");
MODULE_PARM_DESC(nopnp, "disable ISA PnP support (0-1)");
#endif	/* CONFIG_ISAPNP */
MODULE_DESCRIPTION("3Com Etherlink III (3c509, 3c509B) ISA/PnP ethernet driver");
MODULE_LICENSE("GPL");

int
init_module(void)
{
	int el3_cards = 0;

	if (debug >= 0)
		el3_debug = debug;

	el3_root_dev = NULL;
	while (el3_probe(0, el3_cards) == 0) {
		if (irq[el3_cards] > 1)
			el3_root_dev->irq = irq[el3_cards];
		if (xcvr[el3_cards] >= 0)
			el3_root_dev->if_port = xcvr[el3_cards];
		el3_cards++;
	}

	return el3_cards ? 0 : -ENODEV;
}

void
cleanup_module(void)
{
	struct net_device *next_dev;

	/* No need to check MOD_IN_USE, as sys_delete_module() checks. */
	while (el3_root_dev) {
		struct el3_private *lp = (struct el3_private *)el3_root_dev->priv;
#ifdef CONFIG_MCA		
		if(lp->mca_slot!=-1)
			mca_mark_as_unused(lp->mca_slot);
#endif

#ifdef CONFIG_PM
		if (lp->pmdev)
			pm_unregister(lp->pmdev);
#endif
		next_dev = lp->next_dev;
		unregister_netdev(el3_root_dev);
		release_region(el3_root_dev->base_addr, EL3_IO_EXTENT);
		kfree(el3_root_dev);
		el3_root_dev = next_dev;
	}
}
#endif /* MODULE */

/*
 * Local variables:
 *  compile-command: "gcc -DMODULE -D__KERNEL__ -Wall -Wstrict-prototypes -O6 -c 3c509.c"
 *  version-control: t
 *  kept-new-versions: 5
 *  tab-width: 4
 * End:
 */
