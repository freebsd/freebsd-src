static const char version[] =
	"de600.c: $Revision: 1.40 $,  Bjorn Ekwall (bj0rn@blox.se)\n";
/*
 *	de600.c
 *
 *	Linux driver for the D-Link DE-600 Ethernet pocket adapter.
 *
 *	Portions (C) Copyright 1993, 1994 by Bjorn Ekwall
 *	The Author may be reached as bj0rn@blox.se
 *
 *	Based on adapter information gathered from DE600.ASM by D-Link Inc.,
 *	as included on disk C in the v.2.11 of PC/TCP from FTP Software.
 *	For DE600.asm:
 *		Portions (C) Copyright 1990 D-Link, Inc.
 *		Copyright, 1988-1992, Russell Nelson, Crynwr Software
 *
 *	Adapted to the sample network driver core for linux,
 *	written by: Donald Becker <becker@super.org>
 *		(Now at <becker@scyld.com>)
 *
 *	compile-command:
 *	"gcc -D__KERNEL__  -Wall -Wstrict-prototypes -O6 -fomit-frame-pointer \
 *	 -m486 -c de600.c
 *
 **************************************************************/
/*
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2, or (at your option)
 *	any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 **************************************************************/
/* Add more time here if your adapter won't work OK: */
#define DE600_SLOW_DOWN udelay(delay_time)

 /*
 * If you still have trouble reading/writing to the adapter,
 * modify the following "#define": (see <asm/io.h> for more info)
#define REALLY_SLOW_IO
 */
#define SLOW_IO_BY_JUMPING /* Looks "better" than dummy write to port 0x80 :-) */

/*
 * If you want to enable automatic continuous checking for the DE600,
 * keep this #define enabled.
 * It doesn't cost much per packet, so I think it is worth it!
 * If you disagree, comment away the #define, and live with it...
 *
 */
#define CHECK_LOST_DE600

/*
 * Enable this #define if you want the adapter to do a "ifconfig down" on
 * itself when we have detected that something is possibly wrong with it.
 * The default behaviour is to retry with "adapter_init()" until success.
 * This should be used for debugging purposes only.
 * (Depends on the CHECK_LOST_DE600 above)
 *
 */
#define SHUTDOWN_WHEN_LOST

/*
 * See comment at "de600_rspace()"!
 * This is an *ugly* hack, but for now it achieves its goal of
 * faking a TCP flow-control that will not flood the poor DE600.
 *
 * Tricks TCP to announce a small max window (max 2 fast packets please :-)
 *
 * Comment away at your own risk!
 *
 * Update: Use the more general per-device maxwindow parameter instead.
 */
#undef FAKE_SMALL_MAX

/* use 0 for production, 1 for verification, >2 for debug */
#ifdef DE600_DEBUG
#define PRINTK(x) if (de600_debug >= 2) printk x
#else
#define DE600_DEBUG 0
#define PRINTK(x) /**/
#endif

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <asm/io.h>
#include <linux/in.h>
#include <linux/ptrace.h>
#include <asm/system.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/delay.h>

#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

static unsigned int de600_debug = DE600_DEBUG;
MODULE_PARM(de600_debug, "i");
MODULE_PARM_DESC(de600_debug, "DE-600 debug level (0-2)");

static unsigned int delay_time = 10;
MODULE_PARM(delay_time, "i");
MODULE_PARM_DESC(delay_time, "DE-600 deley on I/O in microseconds");

#ifdef FAKE_SMALL_MAX
static unsigned long de600_rspace(struct sock *sk);
#include <net/sock.h>
#endif

typedef unsigned char byte;

/**************************************************
 *                                                *
 * Definition of D-Link Ethernet Pocket adapter   *
 *                                                *
 **************************************************/
/*
 * D-Link Ethernet pocket adapter ports
 */
/*
 * OK, so I'm cheating, but there are an awful lot of
 * reads and writes in order to get anything in and out
 * of the DE-600 with 4 bits at a time in the parallel port,
 * so every saved instruction really helps :-)
 *
 * That is, I don't care what the device struct says
 * but hope that Space.c will keep the rest of the drivers happy.
 */
#ifndef DE600_IO
#define DE600_IO 0x378
#endif

#define DATA_PORT	(DE600_IO)
#define STATUS_PORT	(DE600_IO + 1)
#define COMMAND_PORT	(DE600_IO + 2)

#ifndef DE600_IRQ
#define DE600_IRQ	7
#endif
/*
 * It really should look like this, and autoprobing as well...
 *
#define DATA_PORT	(dev->base_addr + 0)
#define STATUS_PORT	(dev->base_addr + 1)
#define COMMAND_PORT	(dev->base_addr + 2)
#define DE600_IRQ	dev->irq
 */

/*
 * D-Link COMMAND_PORT commands
 */
#define SELECT_NIC	0x04 /* select Network Interface Card */
#define SELECT_PRN	0x1c /* select Printer */
#define NML_PRN		0xec /* normal Printer situation */
#define IRQEN		0x10 /* enable IRQ line */

/*
 * D-Link STATUS_PORT
 */
#define RX_BUSY		0x80
#define RX_GOOD		0x40
#define TX_FAILED16	0x10
#define TX_BUSY		0x08

/*
 * D-Link DATA_PORT commands
 * command in low 4 bits
 * data in high 4 bits
 * select current data nibble with HI_NIBBLE bit
 */
#define WRITE_DATA	0x00 /* write memory */
#define READ_DATA	0x01 /* read memory */
#define STATUS		0x02 /* read  status register */
#define COMMAND		0x03 /* write command register (see COMMAND below) */
#define NULL_COMMAND	0x04 /* null command */
#define RX_LEN		0x05 /* read  received packet length */
#define TX_ADDR		0x06 /* set adapter transmit memory address */
#define RW_ADDR		0x07 /* set adapter read/write memory address */
#define HI_NIBBLE	0x08 /* read/write the high nibble of data,
				or-ed with rest of command */

/*
 * command register, accessed through DATA_PORT with low bits = COMMAND
 */
#define RX_ALL		0x01 /* PROMISCUOUS */
#define RX_BP		0x02 /* default: BROADCAST & PHYSICAL ADDRESS */
#define RX_MBP		0x03 /* MULTICAST, BROADCAST & PHYSICAL ADDRESS */

#define TX_ENABLE	0x04 /* bit 2 */
#define RX_ENABLE	0x08 /* bit 3 */

#define RESET		0x80 /* set bit 7 high */
#define STOP_RESET	0x00 /* set bit 7 low */

/*
 * data to command register
 * (high 4 bits in write to DATA_PORT)
 */
#define RX_PAGE2_SELECT	0x10 /* bit 4, only 2 pages to select */
#define RX_BASE_PAGE	0x20 /* bit 5, always set when specifying RX_ADDR */
#define FLIP_IRQ	0x40 /* bit 6 */

/*
 * D-Link adapter internal memory:
 *
 * 0-2K 1:st transmit page (send from pointer up to 2K)
 * 2-4K	2:nd transmit page (send from pointer up to 4K)
 *
 * 4-6K 1:st receive page (data from 4K upwards)
 * 6-8K 2:nd receive page (data from 6K upwards)
 *
 * 8K+	Adapter ROM (contains magic code and last 3 bytes of Ethernet address)
 */
#define MEM_2K		0x0800 /* 2048 */
#define MEM_4K		0x1000 /* 4096 */
#define MEM_6K		0x1800 /* 6144 */
#define NODE_ADDRESS	0x2000 /* 8192 */

#define RUNT 60		/* Too small Ethernet packet */

/**************************************************
 *                                                *
 *             End of definition                  *
 *                                                *
 **************************************************/

/*
 * Index to functions, as function prototypes.
 */
/* Routines used internally. (See "convenience macros") */
static byte	de600_read_status(struct net_device *dev);
static byte	de600_read_byte(unsigned char type, struct net_device *dev);

/* Put in the device structure. */
static int	de600_open(struct net_device *dev);
static int	de600_close(struct net_device *dev);
static struct net_device_stats *get_stats(struct net_device *dev);
static int	de600_start_xmit(struct sk_buff *skb, struct net_device *dev);

/* Dispatch from interrupts. */
static void	de600_interrupt(int irq, void *dev_id, struct pt_regs *regs);
static int	de600_tx_intr(struct net_device *dev, int irq_status);
static void	de600_rx_intr(struct net_device *dev);

/* Initialization */
static void	trigger_interrupt(struct net_device *dev);
int		de600_probe(struct net_device *dev);
static int	adapter_init(struct net_device *dev);

/*
 * D-Link driver variables:
 */
static volatile int		rx_page;

#define TX_PAGES 2
static volatile int		tx_fifo[TX_PAGES];
static volatile int		tx_fifo_in;
static volatile int		tx_fifo_out;
static volatile int		free_tx_pages = TX_PAGES;
static int			was_down;

/*
 * Convenience macros/functions for D-Link adapter
 */

#define select_prn() outb_p(SELECT_PRN, COMMAND_PORT); DE600_SLOW_DOWN
#define select_nic() outb_p(SELECT_NIC, COMMAND_PORT); DE600_SLOW_DOWN

/* Thanks for hints from Mark Burton <markb@ordern.demon.co.uk> */
#define de600_put_byte(data) ( \
	outb_p(((data) << 4)   | WRITE_DATA            , DATA_PORT), \
	outb_p(((data) & 0xf0) | WRITE_DATA | HI_NIBBLE, DATA_PORT))

/*
 * The first two outb_p()'s below could perhaps be deleted if there
 * would be more delay in the last two. Not certain about it yet...
 */
#define de600_put_command(cmd) ( \
	outb_p(( rx_page        << 4)   | COMMAND            , DATA_PORT), \
	outb_p(( rx_page        & 0xf0) | COMMAND | HI_NIBBLE, DATA_PORT), \
	outb_p(((rx_page | cmd) << 4)   | COMMAND            , DATA_PORT), \
	outb_p(((rx_page | cmd) & 0xf0) | COMMAND | HI_NIBBLE, DATA_PORT))

#define de600_setup_address(addr,type) ( \
	outb_p((((addr) << 4) & 0xf0) | type            , DATA_PORT), \
	outb_p(( (addr)       & 0xf0) | type | HI_NIBBLE, DATA_PORT), \
	outb_p((((addr) >> 4) & 0xf0) | type            , DATA_PORT), \
	outb_p((((addr) >> 8) & 0xf0) | type | HI_NIBBLE, DATA_PORT))

#define rx_page_adr() ((rx_page & RX_PAGE2_SELECT)?(MEM_6K):(MEM_4K))

/* Flip bit, only 2 pages */
#define next_rx_page() (rx_page ^= RX_PAGE2_SELECT)

#define tx_page_adr(a) (((a) + 1) * MEM_2K)

static inline byte
de600_read_status(struct net_device *dev)
{
	byte status;

	outb_p(STATUS, DATA_PORT);
	status = inb(STATUS_PORT);
	outb_p(NULL_COMMAND | HI_NIBBLE, DATA_PORT);

	return status;
}

static inline byte
de600_read_byte(unsigned char type, struct net_device *dev) { /* dev used by macros */
	byte lo;

	(void)outb_p((type), DATA_PORT);
	lo = ((unsigned char)inb(STATUS_PORT)) >> 4;
	(void)outb_p((type) | HI_NIBBLE, DATA_PORT);
	return ((unsigned char)inb(STATUS_PORT) & (unsigned char)0xf0) | lo;
}

/*
 * Open/initialize the board.  This is called (in the current kernel)
 * after booting when 'ifconfig <dev->name> $IP_ADDR' is run (in rc.inet1).
 *
 * This routine should set everything up anew at each open, even
 * registers that "should" only need to be set once at boot, so that
 * there is a non-reboot way to recover if something goes wrong.
 */
static int
de600_open(struct net_device *dev)
{
	int ret = request_irq(DE600_IRQ, de600_interrupt, 0, dev->name, dev);
	if (ret) {
		printk ("%s: unable to get IRQ %d\n", dev->name, DE600_IRQ);
		return ret;
	}

	if (adapter_init(dev))
		return -EIO;

	return 0;
}

/*
 * The inverse routine to de600_open().
 */
static int
de600_close(struct net_device *dev)
{
	select_nic();
	rx_page = 0;
	de600_put_command(RESET);
	de600_put_command(STOP_RESET);
	de600_put_command(0);
	select_prn();

	if (netif_running(dev)) { /* perhaps not needed? */
		free_irq(DE600_IRQ, dev);
	}
	return 0;
}

static struct net_device_stats *
get_stats(struct net_device *dev)
{
    return (struct net_device_stats *)(dev->priv);
}

static inline void
trigger_interrupt(struct net_device *dev)
{
	de600_put_command(FLIP_IRQ);
	select_prn();
	DE600_SLOW_DOWN;
	select_nic();
	de600_put_command(0);
}

/*
 * Copy a buffer to the adapter transmit page memory.
 * Start sending.
 */
static int
de600_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	unsigned long flags;
	int	transmit_from;
	int	len;
	int	tickssofar;
	byte	*buffer = skb->data;
	int	i;

	if (free_tx_pages <= 0) {	/* Do timeouts, to avoid hangs. */
		tickssofar = jiffies - dev->trans_start;

		if (tickssofar < 5)
			return 1;

		/* else */
		printk("%s: transmit timed out (%d), %s?\n",
			dev->name,
			tickssofar,
			"network cable problem"
			);
		/* Restart the adapter. */
		if (adapter_init(dev)) {
			return 1;
		}
	}

	/* Start real output */
	PRINTK(("de600_start_xmit:len=%d, page %d/%d\n", skb->len, tx_fifo_in, free_tx_pages));

	if ((len = skb->len) < RUNT)
		len = RUNT;

	save_flags(flags);
	cli();
	select_nic();
	tx_fifo[tx_fifo_in] = transmit_from = tx_page_adr(tx_fifo_in) - len;
	tx_fifo_in = (tx_fifo_in + 1) % TX_PAGES; /* Next free tx page */

#ifdef CHECK_LOST_DE600
	/* This costs about 40 instructions per packet... */
	de600_setup_address(NODE_ADDRESS, RW_ADDR);
	de600_read_byte(READ_DATA, dev);
	if (was_down || (de600_read_byte(READ_DATA, dev) != 0xde)) {
		if (adapter_init(dev)) {
			restore_flags(flags);
			return 1;
		}
	}
#endif

	de600_setup_address(transmit_from, RW_ADDR);
	for (i = 0;  i < skb->len ; ++i, ++buffer)
		de600_put_byte(*buffer);
	for (; i < len; ++i)
		de600_put_byte(0);

	if (free_tx_pages-- == TX_PAGES) { /* No transmission going on */
		dev->trans_start = jiffies;
		netif_start_queue(dev); /* allow more packets into adapter */
		/* Send page and generate a faked interrupt */
		de600_setup_address(transmit_from, TX_ADDR);
		de600_put_command(TX_ENABLE);
	}
	else {
		if (free_tx_pages)
			netif_start_queue(dev);
		else
			netif_stop_queue(dev);
		select_prn();
	}

	restore_flags(flags);

#ifdef FAKE_SMALL_MAX
	/* This will "patch" the socket TCP proto at an early moment */
	if (skb->sk && (skb->sk->protocol == IPPROTO_TCP) &&
		(skb->sk->prot->rspace != &de600_rspace))
		skb->sk->prot->rspace = de600_rspace; /* Ugh! */
#endif

	dev_kfree_skb (skb);

	return 0;
}

/*
 * The typical workload of the driver:
 * Handle the network interface interrupts.
 */
static void
de600_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
	struct net_device	*dev = dev_id;
	byte		irq_status;
	int		retrig = 0;
	int		boguscount = 0;

	/* This might just as well be deleted now, no crummy drivers present :-) */
	if ((dev == NULL) || (DE600_IRQ != irq)) {
		printk("%s: bogus interrupt %d\n", dev?dev->name:"DE-600", irq);
		return;
	}

	select_nic();
	irq_status = de600_read_status(dev);

	do {
		PRINTK(("de600_interrupt (%02X)\n", irq_status));

		if (irq_status & RX_GOOD)
			de600_rx_intr(dev);
		else if (!(irq_status & RX_BUSY))
			de600_put_command(RX_ENABLE);

		/* Any transmission in progress? */
		if (free_tx_pages < TX_PAGES)
			retrig = de600_tx_intr(dev, irq_status);
		else
			retrig = 0;

		irq_status = de600_read_status(dev);
	} while ( (irq_status & RX_GOOD) || ((++boguscount < 100) && retrig) );
	/*
	 * Yeah, it _looks_ like busy waiting, smells like busy waiting
	 * and I know it's not PC, but please, it will only occur once
	 * in a while and then only for a loop or so (< 1ms for sure!)
	 */

	/* Enable adapter interrupts */
	select_prn();

	if (retrig)
		trigger_interrupt(dev);

	return;
}

static int
de600_tx_intr(struct net_device *dev, int irq_status)
{
	/*
	 * Returns 1 if tx still not done
	 */

	/* Check if current transmission is done yet */
	if (irq_status & TX_BUSY)
		return 1; /* tx not done, try again */

	/* else */
	/* If last transmission OK then bump fifo index */
	if (!(irq_status & TX_FAILED16)) {
		tx_fifo_out = (tx_fifo_out + 1) % TX_PAGES;
		++free_tx_pages;
		((struct net_device_stats *)(dev->priv))->tx_packets++;
		netif_wake_queue(dev);
	}

	/* More to send, or resend last packet? */
	if ((free_tx_pages < TX_PAGES) || (irq_status & TX_FAILED16)) {
		dev->trans_start = jiffies;
		de600_setup_address(tx_fifo[tx_fifo_out], TX_ADDR);
		de600_put_command(TX_ENABLE);
		return 1;
	}
	/* else */

	return 0;
}

/*
 * We have a good packet, get it out of the adapter.
 */
static void
de600_rx_intr(struct net_device *dev)
{
	struct sk_buff	*skb;
	unsigned long flags;
	int		i;
	int		read_from;
	int		size;
	register unsigned char	*buffer;

	save_flags(flags);
	cli();

	/* Get size of received packet */
	size = de600_read_byte(RX_LEN, dev);	/* low byte */
	size += (de600_read_byte(RX_LEN, dev) << 8);	/* high byte */
	size -= 4;	/* Ignore trailing 4 CRC-bytes */

	/* Tell adapter where to store next incoming packet, enable receiver */
	read_from = rx_page_adr();
	next_rx_page();
	de600_put_command(RX_ENABLE);

	restore_flags(flags);

	if ((size < 32)  ||  (size > 1535)) {
		printk("%s: Bogus packet size %d.\n", dev->name, size);
		if (size > 10000)
			adapter_init(dev);
		return;
	}

	skb = dev_alloc_skb(size+2);
	if (skb == NULL) {
		printk("%s: Couldn't allocate a sk_buff of size %d.\n",
			dev->name, size);
		return;
	}
	/* else */

	skb->dev = dev;
	skb_reserve(skb,2);	/* Align */

	/* 'skb->data' points to the start of sk_buff data area. */
	buffer = skb_put(skb,size);

	/* copy the packet into the buffer */
	de600_setup_address(read_from, RW_ADDR);
	for (i = size; i > 0; --i, ++buffer)
		*buffer = de600_read_byte(READ_DATA, dev);

	skb->protocol=eth_type_trans(skb,dev);

	netif_rx(skb);

	/* update stats */
	dev->last_rx = jiffies;
	((struct net_device_stats *)(dev->priv))->rx_packets++; /* count all receives */
	((struct net_device_stats *)(dev->priv))->rx_bytes += size; /* count all received bytes */

	/*
	 * If any worth-while packets have been received, netif_rx()
	 * has done a mark_bh(INET_BH) for us and will work on them
	 * when we get to the bottom-half routine.
	 */
}

int __init 
de600_probe(struct net_device *dev)
{
	int	i;
	static struct net_device_stats de600_netstats;
	/*dev->priv = kmalloc(sizeof(struct net_device_stats), GFP_KERNEL);*/

	SET_MODULE_OWNER(dev);

	printk("%s: D-Link DE-600 pocket adapter", dev->name);
	/* Alpha testers must have the version number to report bugs. */
	if (de600_debug > 1)
		printk(version);

	/* probe for adapter */
	rx_page = 0;
	select_nic();
	(void)de600_read_status(dev);
	de600_put_command(RESET);
	de600_put_command(STOP_RESET);
	if (de600_read_status(dev) & 0xf0) {
		printk(": not at I/O %#3x.\n", DATA_PORT);
		return -ENODEV;
	}

	/*
	 * Maybe we found one,
	 * have to check if it is a D-Link DE-600 adapter...
	 */

	/* Get the adapter ethernet address from the ROM */
	de600_setup_address(NODE_ADDRESS, RW_ADDR);
	for (i = 0; i < ETH_ALEN; i++) {
		dev->dev_addr[i] = de600_read_byte(READ_DATA, dev);
		dev->broadcast[i] = 0xff;
	}

	/* Check magic code */
	if ((dev->dev_addr[1] == 0xde) && (dev->dev_addr[2] == 0x15)) {
		/* OK, install real address */
		dev->dev_addr[0] = 0x00;
		dev->dev_addr[1] = 0x80;
		dev->dev_addr[2] = 0xc8;
		dev->dev_addr[3] &= 0x0f;
		dev->dev_addr[3] |= 0x70;
	} else {
		printk(" not identified in the printer port\n");
		return -ENODEV;
	}

#if 0 /* Not yet */
	if (check_region(DE600_IO, 3)) {
		printk(", port 0x%x busy\n", DE600_IO);
		return -EBUSY;
	}
#endif
	request_region(DE600_IO, 3, "de600");

	printk(", Ethernet Address: %02X", dev->dev_addr[0]);
	for (i = 1; i < ETH_ALEN; i++)
		printk(":%02X",dev->dev_addr[i]);
	printk("\n");

	/* Initialize the device structure. */
	dev->priv = &de600_netstats;

	memset(dev->priv, 0, sizeof(struct net_device_stats));
	dev->get_stats = get_stats;

	dev->open = de600_open;
	dev->stop = de600_close;
	dev->hard_start_xmit = &de600_start_xmit;

	ether_setup(dev);

	dev->flags&=~IFF_MULTICAST;

	select_prn();
	return 0;
}

static int
adapter_init(struct net_device *dev)
{
	int	i;
	unsigned long flags;

	save_flags(flags);
	cli();

	select_nic();
	rx_page = 0; /* used by RESET */
	de600_put_command(RESET);
	de600_put_command(STOP_RESET);
#ifdef CHECK_LOST_DE600
	/* Check if it is still there... */
	/* Get the some bytes of the adapter ethernet address from the ROM */
	de600_setup_address(NODE_ADDRESS, RW_ADDR);
	de600_read_byte(READ_DATA, dev);
	if ((de600_read_byte(READ_DATA, dev) != 0xde) ||
	    (de600_read_byte(READ_DATA, dev) != 0x15)) {
	/* was: if (de600_read_status(dev) & 0xf0) { */
		printk("Something has happened to the DE-600!  Please check it"
#ifdef SHUTDOWN_WHEN_LOST
			" and do a new ifconfig"
#endif /* SHUTDOWN_WHEN_LOST */
			"!\n");
#ifdef SHUTDOWN_WHEN_LOST
		/* Goodbye, cruel world... */
		dev->flags &= ~IFF_UP;
		de600_close(dev);
#endif /* SHUTDOWN_WHEN_LOST */
		was_down = 1;
		netif_stop_queue(dev); /* Transmit busy...  */
		restore_flags(flags);
		return 1; /* failed */
	}
#endif /* CHECK_LOST_DE600 */
	if (was_down) {
		printk("Thanks, I feel much better now!\n");
		was_down = 0;
	}

	netif_start_queue(dev);
	tx_fifo_in = 0;
	tx_fifo_out = 0;
	free_tx_pages = TX_PAGES;

	/* set the ether address. */
	de600_setup_address(NODE_ADDRESS, RW_ADDR);
	for (i = 0; i < ETH_ALEN; i++)
		de600_put_byte(dev->dev_addr[i]);

	/* where to start saving incoming packets */
	rx_page = RX_BP | RX_BASE_PAGE;
	de600_setup_address(MEM_4K, RW_ADDR);
	/* Enable receiver */
	de600_put_command(RX_ENABLE);
	select_prn();
	restore_flags(flags);

	return 0; /* OK */
}

#ifdef FAKE_SMALL_MAX
/*
 *	The new router code (coming soon 8-) ) will fix this properly.
 */
#define DE600_MIN_WINDOW 1024
#define DE600_MAX_WINDOW 2048
#define DE600_TCP_WINDOW_DIFF 1024
/*
 * Copied from "net/inet/sock.c"
 *
 * Sets a lower max receive window in order to achieve <= 2
 * packets arriving at the adapter in fast succession.
 * (No way that a DE-600 can keep up with a net saturated
 *  with packets homing in on it :-( )
 *
 * Since there are only 2 receive buffers in the DE-600
 * and it takes some time to copy from the adapter,
 * this is absolutely necessary for any TCP performance whatsoever!
 *
 * Note that the returned window info will never be smaller than
 * DE600_MIN_WINDOW, i.e. 1024
 * This differs from the standard function, that can return an
 * arbitrarily small window!
 */
static unsigned long
de600_rspace(struct sock *sk)
{
  int amt;

  if (sk != NULL) {
/*
 * Hack! You might want to play with commenting away the following line,
 * if you know what you do!
  	sk->max_unacked = DE600_MAX_WINDOW - DE600_TCP_WINDOW_DIFF;
 */

	if (atomic_read(&sk->rmem_alloc) >= sk->rcvbuf-2*DE600_MIN_WINDOW) return(0);
	amt = min_t(int, (sk->rcvbuf-atomic_read(&sk->rmem_alloc))/2/*-DE600_MIN_WINDOW*/, DE600_MAX_WINDOW);
	if (amt < 0) return(0);
	return(amt);
  }
  return(0);
}
#endif

#ifdef MODULE
static struct net_device de600_dev;

int
init_module(void)
{
	de600_dev.init = de600_probe;
	if (register_netdev(&de600_dev) != 0)
		return -EIO;
	return 0;
}

void
cleanup_module(void)
{
	unregister_netdev(&de600_dev);
	release_region(DE600_IO, 3);
}
#endif /* MODULE */

MODULE_LICENSE("GPL");

/*
 * Local variables:
 *  kernel-compile-command: "gcc -D__KERNEL__ -Ilinux/include -I../../net/inet -Wall -Wstrict-prototypes -O2 -m486 -c de600.c"
 *  module-compile-command: "gcc -D__KERNEL__ -DMODULE -Ilinux/include -I../../net/inet -Wall -Wstrict-prototypes -O2 -m486 -c de600.c"
 *  compile-command: "gcc -D__KERNEL__ -DMODULE -Ilinux/include -I../../net/inet -Wall -Wstrict-prototypes -O2 -m486 -c de600.c"
 * End:
 */
