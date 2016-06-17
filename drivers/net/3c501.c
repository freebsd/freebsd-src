/* 3c501.c: A 3Com 3c501 Ethernet driver for Linux. */
/*
    Written 1992,1993,1994  Donald Becker

    Copyright 1993 United States Government as represented by the
    Director, National Security Agency.  This software may be used and
    distributed according to the terms of the GNU General Public License,
    incorporated herein by reference.

    This is a device driver for the 3Com Etherlink 3c501.
    Do not purchase this card, even as a joke.  It's performance is horrible,
    and it breaks in many ways.

    The author may be reached as becker@scyld.com, or C/O
	Scyld Computing Corporation
	410 Severn Ave., Suite 210
	Annapolis MD 21403


    Fixed (again!) the missing interrupt locking on TX/RX shifting.
    		Alan Cox <Alan.Cox@linux.org>

    Removed calls to init_etherdev since they are no longer needed, and
    cleaned up modularization just a bit. The driver still allows only
    the default address for cards when loaded as a module, but that's
    really less braindead than anyone using a 3c501 board. :)
		    19950208 (invid@msen.com)

    Added traps for interrupts hitting the window as we clear and TX load
    the board. Now getting 150K/second FTP with a 3c501 card. Still playing
    with a TX-TX optimisation to see if we can touch 180-200K/second as seems
    theoretically maximum.
    		19950402 Alan Cox <Alan.Cox@linux.org>
    		
    Cleaned up for 2.3.x because we broke SMP now. 
    		20000208 Alan Cox <alan@redhat.com>
    		
    Fixed zero fill corner case 
    		20030104 Alan Cox <alan@redhat.com>
    		
*/


/**
 * DOC: 3c501 Card Notes
 *
 *  Some notes on this thing if you have to hack it.  [Alan]
 *
 *  Some documentation is available from 3Com. Due to the boards age
 *  standard responses when you ask for this will range from 'be serious'
 *  to 'give it to a museum'. The documentation is incomplete and mostly
 *  of historical interest anyway. 
 *
 *  The basic system is a single buffer which can be used to receive or
 *  transmit a packet. A third command mode exists when you are setting
 *  things up.
 *
 *  If it's transmitting it's not receiving and vice versa. In fact the
 *  time to get the board back into useful state after an operation is
 *  quite large.
 *
 *  The driver works by keeping the board in receive mode waiting for a
 *  packet to arrive. When one arrives it is copied out of the buffer
 *  and delivered to the kernel. The card is reloaded and off we go.
 *
 *  When transmitting lp->txing is set and the card is reset (from
 *  receive mode) [possibly losing a packet just received] to command
 *  mode. A packet is loaded and transmit mode triggered. The interrupt
 *  handler runs different code for transmit interrupts and can handle
 *  returning to receive mode or retransmissions (yes you have to help
 *  out with those too).
 *
 * DOC: Problems
 *  
 *  There are a wide variety of undocumented error returns from the card
 *  and you basically have to kick the board and pray if they turn up. Most
 *  only occur under extreme load or if you do something the board doesn't
 *  like (eg touching a register at the wrong time).
 *
 *  The driver is less efficient than it could be. It switches through
 *  receive mode even if more transmits are queued. If this worries you buy
 *  a real Ethernet card.
 *
 *  The combination of slow receive restart and no real multicast
 *  filter makes the board unusable with a kernel compiled for IP
 *  multicasting in a real multicast environment. That's down to the board,
 *  but even with no multicast programs running a multicast IP kernel is
 *  in group 224.0.0.1 and you will therefore be listening to all multicasts.
 *  One nv conference running over that Ethernet and you can give up.
 *
 */

#define DRV_NAME	"3c501"
#define DRV_VERSION	"2001/11/17"


static const char version[] =
	DRV_NAME ".c: " DRV_VERSION " Alan Cox (alan@redhat.com).\n";

/*
 *	Braindamage remaining:
 *	The 3c501 board.
 */

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/ptrace.h>
#include <linux/fcntl.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/errno.h>
#include <linux/config.h>	/* for CONFIG_IP_MULTICAST */
#include <linux/spinlock.h>
#include <linux/ethtool.h>

#include <asm/uaccess.h>
#include <asm/bitops.h>
#include <asm/io.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/init.h>

/* A zero-terminated list of I/O addresses to be probed.
   The 3c501 can be at many locations, but here are the popular ones. */
static unsigned int netcard_portlist[] __initdata = { 
	0x280, 0x300, 0
};


/*
 *	Index to functions.
 */

int el1_probe(struct net_device *dev);
static int  el1_probe1(struct net_device *dev, int ioaddr);
static int  el_open(struct net_device *dev);
static void el_timeout(struct net_device *dev);
static int  el_start_xmit(struct sk_buff *skb, struct net_device *dev);
static void el_interrupt(int irq, void *dev_id, struct pt_regs *regs);
static void el_receive(struct net_device *dev);
static void el_reset(struct net_device *dev);
static int  el1_close(struct net_device *dev);
static struct net_device_stats *el1_get_stats(struct net_device *dev);
static void set_multicast_list(struct net_device *dev);
static struct ethtool_ops netdev_ethtool_ops;

#define EL1_IO_EXTENT	16

#ifndef EL_DEBUG
#define EL_DEBUG  0	/* use 0 for production, 1 for devel., >2 for debug */
#endif			/* Anything above 5 is wordy death! */
#define debug el_debug
static int el_debug = EL_DEBUG;

/*
 *	Board-specific info in dev->priv.
 */

struct net_local
{
	struct net_device_stats stats;
	int		tx_pkt_start;	/* The length of the current Tx packet. */
	int		collisions;	/* Tx collisions this packet */
	int		loading;	/* Spot buffer load collisions */
	int		txing;		/* True if card is in TX mode */
	spinlock_t	lock;		/* Serializing lock */
};


#define RX_STATUS (ioaddr + 0x06)
#define RX_CMD	  RX_STATUS
#define TX_STATUS (ioaddr + 0x07)
#define TX_CMD	  TX_STATUS
#define GP_LOW 	  (ioaddr + 0x08)
#define GP_HIGH   (ioaddr + 0x09)
#define RX_BUF_CLR (ioaddr + 0x0A)
#define RX_LOW	  (ioaddr + 0x0A)
#define RX_HIGH   (ioaddr + 0x0B)
#define SAPROM	  (ioaddr + 0x0C)
#define AX_STATUS (ioaddr + 0x0E)
#define AX_CMD	  AX_STATUS
#define DATAPORT  (ioaddr + 0x0F)
#define TX_RDY 0x08		/* In TX_STATUS */

#define EL1_DATAPTR	0x08
#define EL1_RXPTR	0x0A
#define EL1_SAPROM	0x0C
#define EL1_DATAPORT 	0x0f

/*
 *	Writes to the ax command register.
 */

#define AX_OFF	0x00			/* Irq off, buffer access on */
#define AX_SYS  0x40			/* Load the buffer */
#define AX_XMIT 0x44			/* Transmit a packet */
#define AX_RX	0x48			/* Receive a packet */
#define AX_LOOP	0x0C			/* Loopback mode */
#define AX_RESET 0x80

/*
 *	Normal receive mode written to RX_STATUS.  We must intr on short packets
 *	to avoid bogus rx lockups.
 */

#define RX_NORM 0xA8		/* 0x68 == all addrs, 0xA8 only to me. */
#define RX_PROM 0x68		/* Senior Prom, uhmm promiscuous mode. */
#define RX_MULT 0xE8		/* Accept multicast packets. */
#define TX_NORM 0x0A		/* Interrupt on everything that might hang the chip */

/*
 *	TX_STATUS register.
 */

#define TX_COLLISION 0x02
#define TX_16COLLISIONS 0x04
#define TX_READY 0x08

#define RX_RUNT 0x08
#define RX_MISSED 0x01		/* Missed a packet due to 3c501 braindamage. */
#define RX_GOOD	0x30		/* Good packet 0x20, or simple overflow 0x10. */


/*
 *	The boilerplate probe code.
 */

/**
 * el1_probe:
 * @dev: The device structure passed in to probe. 
 *
 * This can be called from two places. The network layer will probe using
 * a device structure passed in with the probe information completed. For a
 * modular driver we use #init_module to fill in our own structure and probe
 * for it.
 *
 * Returns 0 on success. ENXIO if asked not to probe and ENODEV if asked to
 * probe and failing to find anything.
 */
 
int __init el1_probe(struct net_device *dev)
{
	int i;
	int base_addr = dev->base_addr;

	SET_MODULE_OWNER(dev);

	if (base_addr > 0x1ff)	/* Check a single specified location. */
		return el1_probe1(dev, base_addr);
	else if (base_addr != 0)	/* Don't probe at all. */
		return -ENXIO;

	for (i = 0; netcard_portlist[i]; i++)
		if (el1_probe1(dev, netcard_portlist[i]) == 0)
			return 0;

	return -ENODEV;
}

/**
 *	el1_probe1: 
 *	@dev: The device structure to use
 *	@ioaddr: An I/O address to probe at.
 *
 *	The actual probe. This is iterated over by #el1_probe in order to
 *	check all the applicable device locations.
 *
 *	Returns 0 for a success, in which case the device is activated,
 *	EAGAIN if the IRQ is in use by another driver, and ENODEV if the
 *	board cannot be found.
 */

static int __init el1_probe1(struct net_device *dev, int ioaddr)
{
	struct net_local *lp;
	const char *mname;		/* Vendor name */
	unsigned char station_addr[6];
	int autoirq = 0;
	int i;

	/*
	 *	Reserve I/O resource for exclusive use by this driver
	 */

	if (!request_region(ioaddr, EL1_IO_EXTENT, dev->name))
		return -ENODEV;

	/*
	 *	Read the station address PROM data from the special port.
	 */

	for (i = 0; i < 6; i++)
	{
		outw(i, ioaddr + EL1_DATAPTR);
		station_addr[i] = inb(ioaddr + EL1_SAPROM);
	}
	/*
	 *	Check the first three octets of the S.A. for 3Com's prefix, or
	 *	for the Sager NP943 prefix.
	 */

	if (station_addr[0] == 0x02  &&  station_addr[1] == 0x60
		&& station_addr[2] == 0x8c)
	{
		mname = "3c501";
	} else if (station_addr[0] == 0x00  &&  station_addr[1] == 0x80
	&& station_addr[2] == 0xC8)
	{
		mname = "NP943";
    	}
    	else {
		release_region(ioaddr, EL1_IO_EXTENT);
		return -ENODEV;
	}

	/*
	 *	We auto-IRQ by shutting off the interrupt line and letting it float
	 *	high.
	 */

	if (dev->irq < 2)
	{
		autoirq_setup(2);
		inb(RX_STATUS);		/* Clear pending interrupts. */
		inb(TX_STATUS);
		outb(AX_LOOP + 1, AX_CMD);

		outb(0x00, AX_CMD);

		autoirq = autoirq_report(1);

		if (autoirq == 0)
		{
			printk(KERN_WARNING "%s probe at %#x failed to detect IRQ line.\n",
				mname, ioaddr);
			release_region(ioaddr, EL1_IO_EXTENT);
			return -EAGAIN;
		}
	}

	outb(AX_RESET+AX_LOOP, AX_CMD);			/* Loopback mode. */
	dev->base_addr = ioaddr;
	memcpy(dev->dev_addr, station_addr, ETH_ALEN);

	if (dev->mem_start & 0xf)
		el_debug = dev->mem_start & 0x7;
	if (autoirq)
		dev->irq = autoirq;

	printk(KERN_INFO "%s: %s EtherLink at %#lx, using %sIRQ %d.\n", dev->name, mname, dev->base_addr,
			autoirq ? "auto":"assigned ", dev->irq);

#ifdef CONFIG_IP_MULTICAST
	printk(KERN_WARNING "WARNING: Use of the 3c501 in a multicast kernel is NOT recommended.\n");
#endif

	if (el_debug)
		printk(KERN_DEBUG "%s", version);

	/*
	 *	Initialize the device structure.
	 */

	dev->priv = kmalloc(sizeof(struct net_local), GFP_KERNEL);
	if (dev->priv == NULL) {
		release_region(ioaddr, EL1_IO_EXTENT);
		return -ENOMEM;
	}
	memset(dev->priv, 0, sizeof(struct net_local));

	lp=dev->priv;
	spin_lock_init(&lp->lock);
	
	/*
	 *	The EL1-specific entries in the device structure.
	 */

	dev->open = &el_open;
	dev->hard_start_xmit = &el_start_xmit;
	dev->tx_timeout = &el_timeout;
	dev->watchdog_timeo = HZ;
	dev->stop = &el1_close;
	dev->get_stats = &el1_get_stats;
	dev->set_multicast_list = &set_multicast_list;
	dev->ethtool_ops = &netdev_ethtool_ops;

	/*
	 *	Setup the generic properties
	 */

	ether_setup(dev);

	return 0;
}

/**
 *	el1_open:
 *	@dev: device that is being opened
 *
 *	When an ifconfig is issued which changes the device flags to include
 *	IFF_UP this function is called. It is only called when the change 
 *	occurs, not when the interface remains up. #el1_close will be called
 *	when it goes down.
 *
 *	Returns 0 for a successful open, or -EAGAIN if someone has run off
 *	with our interrupt line.
 */

static int el_open(struct net_device *dev)
{
	int retval;
	int ioaddr = dev->base_addr;
	struct net_local *lp = (struct net_local *)dev->priv;
	unsigned long flags;

	if (el_debug > 2)
		printk(KERN_DEBUG "%s: Doing el_open()...", dev->name);

	if ((retval = request_irq(dev->irq, &el_interrupt, 0, dev->name, dev)))
		return retval;

	spin_lock_irqsave(&lp->lock, flags);
	el_reset(dev);
	spin_unlock_irqrestore(&lp->lock, flags);

	lp->txing = 0;		/* Board in RX mode */
	outb(AX_RX, AX_CMD);	/* Aux control, irq and receive enabled */
	netif_start_queue(dev);
	return 0;
}

/**
 * el_timeout:
 * @dev: The 3c501 card that has timed out
 *
 * Attempt to restart the board. This is basically a mixture of extreme
 * violence and prayer
 *
 */
 
static void el_timeout(struct net_device *dev)
{
	struct net_local *lp = (struct net_local *)dev->priv;
	int ioaddr = dev->base_addr;
 
	if (el_debug)
		printk (KERN_DEBUG "%s: transmit timed out, txsr %#2x axsr=%02x rxsr=%02x.\n",
			dev->name, inb(TX_STATUS), inb(AX_STATUS), inb(RX_STATUS));
	lp->stats.tx_errors++;
	outb(TX_NORM, TX_CMD);
	outb(RX_NORM, RX_CMD);
	outb(AX_OFF, AX_CMD);	/* Just trigger a false interrupt. */
	outb(AX_RX, AX_CMD);	/* Aux control, irq and receive enabled */
	lp->txing = 0;		/* Ripped back in to RX */
	netif_wake_queue(dev);
}

 
/**
 * el_start_xmit:
 * @skb: The packet that is queued to be sent
 * @dev: The 3c501 card we want to throw it down
 *
 * Attempt to send a packet to a 3c501 card. There are some interesting
 * catches here because the 3c501 is an extremely old and therefore
 * stupid piece of technology.
 *
 * If we are handling an interrupt on the other CPU we cannot load a packet
 * as we may still be attempting to retrieve the last RX packet buffer.
 *
 * When a transmit times out we dump the card into control mode and just
 * start again. It happens enough that it isnt worth logging.
 *
 * We avoid holding the spin locks when doing the packet load to the board.
 * The device is very slow, and its DMA mode is even slower. If we held the
 * lock while loading 1500 bytes onto the controller we would drop a lot of
 * serial port characters. This requires we do extra locking, but we have
 * no real choice.
 */

static int el_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct net_local *lp = (struct net_local *)dev->priv;
	int ioaddr = dev->base_addr;
	unsigned long flags;

	/*
	 *	Avoid incoming interrupts between us flipping txing and flipping
	 *	mode as the driver assumes txing is a faithful indicator of card
	 *	state
	 */

	spin_lock_irqsave(&lp->lock, flags);
	
	/*
	 *	Avoid timer-based retransmission conflicts.
	 */

	netif_stop_queue(dev);

	do
	{
		int len = skb->len;
		int pad = 0;
		int gp_start;
		unsigned char *buf = skb->data;
		
		if(len < ETH_ZLEN)
			pad = ETH_ZLEN - len;
			
		gp_start = 0x800 - ( len + pad );

		lp->tx_pkt_start = gp_start;
    		lp->collisions = 0;

    		lp->stats.tx_bytes += skb->len;

		/*
		 *	Command mode with status cleared should [in theory]
		 *	mean no more interrupts can be pending on the card.
		 */

		outb_p(AX_SYS, AX_CMD);
		inb_p(RX_STATUS);
		inb_p(TX_STATUS);

		lp->loading = 1;
		lp->txing = 1;

		/*
		 *	Turn interrupts back on while we spend a pleasant afternoon
		 *	loading bytes into the board
		 */

		spin_unlock_irqrestore(&lp->lock, flags);
		
		outw(0x00, RX_BUF_CLR);		/* Set rx packet area to 0. */
		outw(gp_start, GP_LOW);		/* aim - packet will be loaded into buffer start */
		outsb(DATAPORT,buf,len);	/* load buffer (usual thing each byte increments the pointer) */
		if(pad)
		{
			while(pad--)		/* Zero fill buffer tail */
				outb(0, DATAPORT);
		}
		outw(gp_start, GP_LOW);		/* the board reuses the same register */
	
		if(lp->loading != 2)
		{
			outb(AX_XMIT, AX_CMD);		/* fire ... Trigger xmit.  */
			lp->loading=0;
			dev->trans_start = jiffies;
			if (el_debug > 2)
				printk(KERN_DEBUG " queued xmit.\n");
			dev_kfree_skb (skb);
			return 0;
		}
		/* A receive upset our load, despite our best efforts */
		if(el_debug>2)
			printk(KERN_DEBUG "%s: burped during tx load.\n", dev->name);
		spin_lock_irqsave(&lp->lock, flags);
	}
	while(1);

}


/**
 * el_interrupt:
 * @irq: Interrupt number
 * @dev_id: The 3c501 that burped
 * @regs: Register data (surplus to our requirements)
 *
 * Handle the ether interface interrupts. The 3c501 needs a lot more 
 * hand holding than most cards. In paticular we get a transmit interrupt
 * with a collision error because the board firmware isnt capable of rewinding
 * its own transmit buffer pointers. It can however count to 16 for us.
 *
 * On the receive side the card is also very dumb. It has no buffering to
 * speak of. We simply pull the packet out of its PIO buffer (which is slow)
 * and queue it for the kernel. Then we reset the card for the next packet.
 *
 * We sometimes get suprise interrupts late both because the SMP IRQ delivery
 * is message passing and because the card sometimes seems to deliver late. I
 * think if it is part way through a receive and the mode is changed it carries
 * on receiving and sends us an interrupt. We have to band aid all these cases
 * to get a sensible 150kbytes/second performance. Even then you want a small
 * TCP window.
 */

static void el_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct net_device *dev = dev_id;
	struct net_local *lp;
	int ioaddr;
	int axsr;			/* Aux. status reg. */

	ioaddr = dev->base_addr;
	lp = (struct net_local *)dev->priv;

	spin_lock(&lp->lock);
	
	/*
	 *	What happened ?
	 */

	axsr = inb(AX_STATUS);

	/*
	 *	Log it
	 */

	if (el_debug > 3)
		printk(KERN_DEBUG "%s: el_interrupt() aux=%#02x", dev->name, axsr);

        if(lp->loading==1 && !lp->txing)
        	printk(KERN_WARNING "%s: Inconsistent state loading while not in tx\n",
        		dev->name);

	if (lp->txing)
	{

    		/*
    		 *	Board in transmit mode. May be loading. If we are
    		 *	loading we shouldn't have got this.
    		 */

		int txsr = inb(TX_STATUS);

		if(lp->loading==1)
		{
			if(el_debug > 2)
			{
				printk(KERN_DEBUG "%s: Interrupt while loading [", dev->name);
				printk(KERN_DEBUG " txsr=%02x gp=%04x rp=%04x]\n", txsr, inw(GP_LOW),inw(RX_LOW));
			}
			lp->loading=2;		/* Force a reload */
			spin_unlock(&lp->lock);
			return;
		}

		if (el_debug > 6)
			printk(KERN_DEBUG " txsr=%02x gp=%04x rp=%04x", txsr, inw(GP_LOW),inw(RX_LOW));

		if ((axsr & 0x80) && (txsr & TX_READY) == 0)
		{
			/*
			 *	FIXME: is there a logic to whether to keep on trying or
			 *	reset immediately ?
			 */
			if(el_debug>1)
				printk(KERN_DEBUG "%s: Unusual interrupt during Tx, txsr=%02x axsr=%02x"
			  		" gp=%03x rp=%03x.\n", dev->name, txsr, axsr,
			inw(ioaddr + EL1_DATAPTR), inw(ioaddr + EL1_RXPTR));
			lp->txing = 0;
			netif_wake_queue(dev);
		}
		else if (txsr & TX_16COLLISIONS)
		{
			/*
			 *	Timed out
			 */
			if (el_debug)
				printk (KERN_DEBUG "%s: Transmit failed 16 times, Ethernet jammed?\n",dev->name);
			outb(AX_SYS, AX_CMD);
			lp->txing = 0;
			lp->stats.tx_aborted_errors++;
			netif_wake_queue(dev);
		}
		else if (txsr & TX_COLLISION)
		{
			/*
			 *	Retrigger xmit.
			 */

			if (el_debug > 6)
				printk(KERN_DEBUG " retransmitting after a collision.\n");
			/*
			 *	Poor little chip can't reset its own start pointer
			 */

			outb(AX_SYS, AX_CMD);
			outw(lp->tx_pkt_start, GP_LOW);
			outb(AX_XMIT, AX_CMD);
			lp->stats.collisions++;
			spin_unlock(&lp->lock);
			return;
		}
		else
		{
			/*
			 *	It worked.. we will now fall through and receive
			 */
			lp->stats.tx_packets++;
			if (el_debug > 6)
				printk(KERN_DEBUG " Tx succeeded %s\n",
		       			(txsr & TX_RDY) ? "." : "but tx is busy!");
			/*
			 *	This is safe the interrupt is atomic WRT itself.
			 */

			lp->txing = 0;
			netif_wake_queue(dev);	/* In case more to transmit */
		}
	}
	else
	{
    		/*
    		 *	In receive mode.
    		 */

		int rxsr = inb(RX_STATUS);
		if (el_debug > 5)
			printk(KERN_DEBUG " rxsr=%02x txsr=%02x rp=%04x", rxsr, inb(TX_STATUS),inw(RX_LOW));
		/*
		 *	Just reading rx_status fixes most errors.
		 */
		if (rxsr & RX_MISSED)
			lp->stats.rx_missed_errors++;
		else if (rxsr & RX_RUNT)
		{	/* Handled to avoid board lock-up. */
			lp->stats.rx_length_errors++;
			if (el_debug > 5)
				printk(KERN_DEBUG " runt.\n");
		}
		else if (rxsr & RX_GOOD)
		{
			/*
			 *	Receive worked.
			 */
			el_receive(dev);
		}
		else
		{
			/*
			 *	Nothing?  Something is broken!
			 */
			if (el_debug > 2)
				printk(KERN_DEBUG "%s: No packet seen, rxsr=%02x **resetting 3c501***\n",
					dev->name, rxsr);
			el_reset(dev);
		}
		if (el_debug > 3)
			printk(KERN_DEBUG ".\n");
	}

	/*
	 *	Move into receive mode
	 */

	outb(AX_RX, AX_CMD);
	outw(0x00, RX_BUF_CLR);
	inb(RX_STATUS);		/* Be certain that interrupts are cleared. */
	inb(TX_STATUS);
	spin_unlock(&lp->lock);
	return;
}


/**
 * el_receive:
 * @dev: Device to pull the packets from
 *
 * We have a good packet. Well, not really "good", just mostly not broken.
 * We must check everything to see if it is good. In paticular we occasionally
 * get wild packet sizes from the card. If the packet seems sane we PIO it
 * off the card and queue it for the protocol layers.
 */

static void el_receive(struct net_device *dev)
{
	struct net_local *lp = (struct net_local *)dev->priv;
	int ioaddr = dev->base_addr;
	int pkt_len;
	struct sk_buff *skb;

	pkt_len = inw(RX_LOW);

	if (el_debug > 4)
		printk(KERN_DEBUG " el_receive %d.\n", pkt_len);

	if ((pkt_len < 60)  ||  (pkt_len > 1536))
	{
		if (el_debug)
			printk(KERN_DEBUG "%s: bogus packet, length=%d\n", dev->name, pkt_len);
		lp->stats.rx_over_errors++;
		return;
	}

	/*
	 *	Command mode so we can empty the buffer
	 */

	outb(AX_SYS, AX_CMD);
	skb = dev_alloc_skb(pkt_len+2);

	/*
	 *	Start of frame
	 */

	outw(0x00, GP_LOW);
	if (skb == NULL)
	{
		printk(KERN_INFO "%s: Memory squeeze, dropping packet.\n", dev->name);
		lp->stats.rx_dropped++;
		return;
	}
	else
	{
    		skb_reserve(skb,2);	/* Force 16 byte alignment */
		skb->dev = dev;
		/*
		 *	The read increments through the bytes. The interrupt
		 *	handler will fix the pointer when it returns to
		 *	receive mode.
		 */
		insb(DATAPORT, skb_put(skb,pkt_len), pkt_len);
		skb->protocol=eth_type_trans(skb,dev);
		netif_rx(skb);
		dev->last_rx = jiffies;
		lp->stats.rx_packets++;
		lp->stats.rx_bytes+=pkt_len;
	}
	return;
}

/**
 * el_reset: Reset a 3c501 card
 * @dev: The 3c501 card about to get zapped
 *
 * Even resetting a 3c501 isnt simple. When you activate reset it loses all
 * its configuration. You must hold the lock when doing this. The function
 * cannot take the lock itself as it is callable from the irq handler.
 */

static void  el_reset(struct net_device *dev)
{
	struct net_local *lp = (struct net_local *)dev->priv;
	int ioaddr = dev->base_addr;

	if (el_debug> 2)
		printk(KERN_INFO "3c501 reset...");
	outb(AX_RESET, AX_CMD);		/* Reset the chip */
	outb(AX_LOOP, AX_CMD);		/* Aux control, irq and loopback enabled */
	{
		int i;
		for (i = 0; i < 6; i++)	/* Set the station address. */
			outb(dev->dev_addr[i], ioaddr + i);
	}

	outw(0, RX_BUF_CLR);		/* Set rx packet area to 0. */
	outb(TX_NORM, TX_CMD);		/* tx irq on done, collision */
	outb(RX_NORM, RX_CMD);		/* Set Rx commands. */
	inb(RX_STATUS);			/* Clear status. */
	inb(TX_STATUS);
	lp->txing = 0;
}

/**
 * el1_close:
 * @dev: 3c501 card to shut down
 *
 * Close a 3c501 card. The IFF_UP flag has been cleared by the user via
 * the SIOCSIFFLAGS ioctl. We stop any further transmissions being queued,
 * and then disable the interrupts. Finally we reset the chip. The effects
 * of the rest will be cleaned up by #el1_open. Always returns 0 indicating
 * a success.
 */
 
static int el1_close(struct net_device *dev)
{
	int ioaddr = dev->base_addr;

	if (el_debug > 2)
		printk(KERN_INFO "%s: Shutting down Ethernet card at %#x.\n", dev->name, ioaddr);

	netif_stop_queue(dev);
	
	/*
	 *	Free and disable the IRQ.
	 */

	free_irq(dev->irq, dev);
	outb(AX_RESET, AX_CMD);		/* Reset the chip */

	return 0;
}

/**
 * el1_get_stats:
 * @dev: The card to get the statistics for
 *
 * In smarter devices this function is needed to pull statistics off the
 * board itself. The 3c501 has no hardware statistics. We maintain them all
 * so they are by definition always up to date.
 *
 * Returns the statistics for the card from the card private data
 */
 
static struct net_device_stats *el1_get_stats(struct net_device *dev)
{
	struct net_local *lp = (struct net_local *)dev->priv;
	return &lp->stats;
}

/**
 * set_multicast_list:
 * @dev: The device to adjust
 *
 * Set or clear the multicast filter for this adaptor to use the best-effort 
 * filtering supported. The 3c501 supports only three modes of filtering.
 * It always receives broadcasts and packets for itself. You can choose to
 * optionally receive all packets, or all multicast packets on top of this.
 */

static void set_multicast_list(struct net_device *dev)
{
	int ioaddr = dev->base_addr;

	if(dev->flags&IFF_PROMISC)
	{
		outb(RX_PROM, RX_CMD);
		inb(RX_STATUS);
	}
	else if (dev->mc_list || dev->flags&IFF_ALLMULTI)
	{
		outb(RX_MULT, RX_CMD);	/* Multicast or all multicast is the same */
		inb(RX_STATUS);		/* Clear status. */
	}
	else
	{
		outb(RX_NORM, RX_CMD);
		inb(RX_STATUS);
	}
}


static void netdev_get_drvinfo(struct net_device *dev,
			       struct ethtool_drvinfo *info)
{
	strcpy(info->driver, DRV_NAME);
	strcpy(info->version, DRV_VERSION);
	sprintf(info->bus_info, "ISA 0x%lx", dev->base_addr);
}

static u32 netdev_get_msglevel(struct net_device *dev)
{
	return debug;
}

static void netdev_set_msglevel(struct net_device *dev, u32 level)
{
	debug = level;
}

static struct ethtool_ops netdev_ethtool_ops = {
	.get_drvinfo		= netdev_get_drvinfo,
	.get_msglevel		= netdev_get_msglevel,
	.set_msglevel		= netdev_set_msglevel,
};

#ifdef MODULE

static struct net_device dev_3c501 = {
	init:		el1_probe,
	base_addr:	0x280,
	irq:		5,
};

static int io=0x280;
static int irq=5;
MODULE_PARM(io, "i");
MODULE_PARM(irq, "i");
MODULE_PARM_DESC(io, "EtherLink I/O base address");
MODULE_PARM_DESC(irq, "EtherLink IRQ number");

/**
 * init_module:
 *
 * When the driver is loaded as a module this function is called. We fake up
 * a device structure with the base I/O and interrupt set as if it were being
 * called from Space.c. This minimises the extra code that would otherwise
 * be required.
 *
 * Returns 0 for success or -EIO if a card is not found. Returning an error
 * here also causes the module to be unloaded
 */
 
int init_module(void)
{
	dev_3c501.irq=irq;
	dev_3c501.base_addr=io;
	if (register_netdev(&dev_3c501) != 0)
		return -EIO;
	return 0;
}

/**
 * cleanup_module:
 * 
 * The module is being unloaded. We unhook our network device from the system
 * and then free up the resources we took when the card was found.
 */
 
void cleanup_module(void)
{
	/*
	 *	No need to check MOD_IN_USE, as sys_delete_module() checks.
	 */

	unregister_netdev(&dev_3c501);

	/*
	 *	Free up the private structure, or leak memory :-)
	 */

	kfree(dev_3c501.priv);
	dev_3c501.priv = NULL;	/* gets re-allocated by el1_probe1 */

	/*
	 *	If we don't do this, we can't re-insmod it later.
	 */
	release_region(dev_3c501.base_addr, EL1_IO_EXTENT);
}

#endif /* MODULE */
MODULE_LICENSE("GPL");


/*
 * Local variables:
 *  compile-command: "gcc -D__KERNEL__ -Wall -Wstrict-prototypes -O6 -fomit-frame-pointer  -m486 -c -o 3c501.o 3c501.c"
 *  kept-new-versions: 5
 * End:
 */
