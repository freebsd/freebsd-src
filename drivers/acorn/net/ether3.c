/*
 *  linux/drivers/acorn/net/ether3.c
 *
 *  Copyright (C) 1995-2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * SEEQ nq8005 ethernet driver for Acorn/ANT Ether3 card
 *  for Acorn machines
 *
 * By Russell King, with some suggestions from borris@ant.co.uk
 *
 * Changelog:
 * 1.04	RMK	29/02/1996	Won't pass packets that are from our ethernet
 *				address up to the higher levels - they're
 *				silently ignored.  I/F can now be put into
 *				multicast mode.  Receiver routine optimised.
 * 1.05	RMK	30/02/1996	Now claims interrupt at open when part of
 *				the kernel rather than when a module.
 * 1.06	RMK	02/03/1996	Various code cleanups
 * 1.07	RMK	13/10/1996	Optimised interrupt routine and transmit
 *				routines.
 * 1.08	RMK	14/10/1996	Fixed problem with too many packets,
 *				prevented the kernel message about dropped
 *				packets appearing too many times a second.
 *				Now does not disable all IRQs, only the IRQ
 *				used by this card.
 * 1.09	RMK	10/11/1996	Only enables TX irq when buffer space is low,
 *				but we still service the TX queue if we get a
 *				RX interrupt.
 * 1.10	RMK	15/07/1997	Fixed autoprobing of NQ8004.
 * 1.11	RMK	16/11/1997	Fixed autoprobing of NQ8005A.
 * 1.12	RMK	31/12/1997	Removed reference to dev_tint for Linux 2.1.
 *      RMK	27/06/1998	Changed asm/delay.h to linux/delay.h.
 * 1.13	RMK	29/06/1998	Fixed problem with transmission of packets.
 *				Chip seems to have a bug in, whereby if the
 *				packet starts two bytes from the end of the
 *				buffer, it corrupts the receiver chain, and
 *				never updates the transmit status correctly.
 * 1.14	RMK	07/01/1998	Added initial code for ETHERB addressing.
 * 1.15	RMK	30/04/1999	More fixes to the transmit routine for buggy
 *				hardware.
 * 1.16	RMK	10/02/2000	Updated for 2.3.43
 * 1.17	RMK	13/05/2000	Updated for 2.3.99-pre8
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
#include <linux/init.h>
#include <linux/delay.h>

#include <asm/system.h>
#include <asm/bitops.h>
#include <asm/ecard.h>
#include <asm/io.h>
#include <asm/irq.h>

static char version[] __initdata = "ether3 ethernet driver (c) 1995-2000 R.M.King v1.17\n";

#include "ether3.h"

static unsigned int net_debug = NET_DEBUG;
static const card_ids __init ether3_cids[] = {
	{ MANU_ANT2, PROD_ANT_ETHER3 },
	{ MANU_ANT,  PROD_ANT_ETHER3 },
	{ MANU_ANT,  PROD_ANT_ETHERB },
	{ 0xffff, 0xffff }
};

static void	ether3_setmulticastlist(struct net_device *dev);
static int	ether3_rx(struct net_device *dev, struct dev_priv *priv, unsigned int maxcnt);
static void	ether3_tx(struct net_device *dev, struct dev_priv *priv);
static int	ether3_open (struct net_device *dev);
static int	ether3_sendpacket (struct sk_buff *skb, struct net_device *dev);
static void	ether3_interrupt (int irq, void *dev_id, struct pt_regs *regs);
static int	ether3_close (struct net_device *dev);
static struct net_device_stats *ether3_getstats (struct net_device *dev);
static void	ether3_setmulticastlist (struct net_device *dev);
static void	ether3_timeout(struct net_device *dev);

#define BUS_16		2
#define BUS_8		1
#define BUS_UNKNOWN	0

/* --------------------------------------------------------------------------- */

typedef enum {
	buffer_write,
	buffer_read
} buffer_rw_t;

/*
 * ether3 read/write.  Slow things down a bit...
 * The SEEQ8005 doesn't like us writing to it's registers
 * too quickly.
 */
static inline void ether3_outb(int v, const int r)
{
	outb(v, r);
	udelay(1);
}

static inline void ether3_outw(int v, const int r)
{
	outw(v, r);
	udelay(1);
}
#define ether3_inb(r)		({ unsigned int __v = inb((r)); udelay(1); __v; })
#define ether3_inw(r)		({ unsigned int __v = inw((r)); udelay(1); __v; })

static int
ether3_setbuffer(struct net_device *dev, buffer_rw_t read, int start)
{
	struct dev_priv *priv = (struct dev_priv *)dev->priv;
	int timeout = 1000;

	ether3_outw(priv->regs.config1 | CFG1_LOCBUFMEM, REG_CONFIG1);
	ether3_outw(priv->regs.command | CMD_FIFOWRITE, REG_COMMAND);

	while ((ether3_inw(REG_STATUS) & STAT_FIFOEMPTY) == 0) {
		if (!timeout--) {
			printk("%s: setbuffer broken\n", dev->name);
			priv->broken = 1;
			return 1;
		}
		udelay(1);
	}

	if (read == buffer_read) {
		ether3_outw(start, REG_DMAADDR);
		ether3_outw(priv->regs.command | CMD_FIFOREAD, REG_COMMAND);
	} else {
		ether3_outw(priv->regs.command | CMD_FIFOWRITE, REG_COMMAND);
		ether3_outw(start, REG_DMAADDR);
	}
	return 0;
}

/*
 * write data to the buffer memory
 */
#define ether3_writebuffer(dev,data,length)			\
	outsw(REG_BUFWIN, (data), (length) >> 1)

#define ether3_writeword(dev,data)				\
	outw((data), REG_BUFWIN)

#define ether3_writelong(dev,data)	{			\
	unsigned long reg_bufwin = REG_BUFWIN;			\
	outw((data), reg_bufwin);				\
	outw((data) >> 16, reg_bufwin);				\
}

/*
 * read data from the buffer memory
 */
#define ether3_readbuffer(dev,data,length)			\
	insw(REG_BUFWIN, (data), (length) >> 1)

#define ether3_readword(dev)					\
	inw(REG_BUFWIN)

#define ether3_readlong(dev)	 				\
	inw(REG_BUFWIN) | (inw(REG_BUFWIN) << 16)

/*
 * Switch LED off...
 */
static void
ether3_ledoff(unsigned long data)
{
	struct net_device *dev = (struct net_device *)data;
	struct dev_priv *priv = (struct dev_priv *)dev->priv;
	ether3_outw(priv->regs.config2 |= CFG2_CTRLO, REG_CONFIG2);
}

/*
 * switch LED on...
 */
static inline void
ether3_ledon(struct net_device *dev, struct dev_priv *priv)
{
	del_timer(&priv->timer);
	priv->timer.expires = jiffies + HZ / 50; /* leave on for 1/50th second */
	priv->timer.data = (unsigned long)dev;
	priv->timer.function = ether3_ledoff;
	add_timer(&priv->timer);
	if (priv->regs.config2 & CFG2_CTRLO)
		ether3_outw(priv->regs.config2 &= ~CFG2_CTRLO, REG_CONFIG2);
}

/*
 * Read the ethernet address string from the on board rom.
 * This is an ascii string!!!
 */
static int __init
ether3_addr(char *addr, struct expansion_card *ec)
{
	struct in_chunk_dir cd;
	char *s;
	
	if (ecard_readchunk(&cd, ec, 0xf5, 0) && (s = strchr(cd.d.string, '('))) {
		int i;
		for (i = 0; i<6; i++) {
			addr[i] = simple_strtoul(s + 1, &s, 0x10);
			if (*s != (i==5?')' : ':' ))
				break;
		}
		if (i == 6)
			return 0;
	}
	/* I wonder if we should even let the user continue in this case
	 *   - no, it would be better to disable the device
	 */
	printk(KERN_ERR "ether3: Couldn't read a valid MAC address from card.\n");
	return -ENODEV;
}

/* --------------------------------------------------------------------------- */

static int __init
ether3_ramtest(struct net_device *dev, unsigned char byte)
{
	unsigned char *buffer = kmalloc(RX_END, GFP_KERNEL);
	int i,ret = 0;
	int max_errors = 4;
	int bad = -1;

	if (!buffer)
		return 1;

	memset(buffer, byte, RX_END);
	ether3_setbuffer(dev, buffer_write, 0);
	ether3_writebuffer(dev, buffer, TX_END);
	ether3_setbuffer(dev, buffer_write, RX_START);
	ether3_writebuffer(dev, buffer + RX_START, RX_LEN);
	memset(buffer, byte ^ 0xff, RX_END);
	ether3_setbuffer(dev, buffer_read, 0);
	ether3_readbuffer(dev, buffer, TX_END);
	ether3_setbuffer(dev, buffer_read, RX_START);
	ether3_readbuffer(dev, buffer + RX_START, RX_LEN);

	for (i = 0; i < RX_END; i++) {
		if (buffer[i] != byte) {
			if (max_errors > 0 && bad != buffer[i]) {
				printk("%s: RAM failed with (%02X instead of %02X) at 0x%04X",
				       dev->name, buffer[i], byte, i);
				ret = 2;
				max_errors--;
				bad = i;
			}
		} else {
			if (bad != -1) {
				if (bad != i - 1)
					printk(" - 0x%04X\n", i - 1);
				printk("\n");
				bad = -1;
			}
		}
	}
	if (bad != -1)
		printk(" - 0xffff\n");
	kfree(buffer);

	return ret;
}

/* ------------------------------------------------------------------------------- */

static int __init
ether3_init_2(struct net_device *dev)
{
	struct dev_priv *priv = (struct dev_priv *)dev->priv;
	int i;

	priv->regs.config1 = CFG1_RECVCOMPSTAT0|CFG1_DMABURST8;
	priv->regs.config2 = CFG2_CTRLO|CFG2_RECVCRC|CFG2_ERRENCRC;
	priv->regs.command = 0;

	/*
	 * Set up our hardware address
	 */
	ether3_outw(priv->regs.config1 | CFG1_BUFSELSTAT0, REG_CONFIG1);
	for (i = 0; i < 6; i++)
		ether3_outb(dev->dev_addr[i], REG_BUFWIN);

	if (dev->flags & IFF_PROMISC)
		priv->regs.config1 |= CFG1_RECVPROMISC;
	else if (dev->flags & IFF_MULTICAST)
		priv->regs.config1 |= CFG1_RECVSPECBRMULTI;
	else
		priv->regs.config1 |= CFG1_RECVSPECBROAD;

	/*
	 * There is a problem with the NQ8005 in that it occasionally loses the
	 * last two bytes.  To get round this problem, we receive the CRC as
	 * well.  That way, if we do loose the last two, then it doesn't matter.
	 */
	ether3_outw(priv->regs.config1 | CFG1_TRANSEND, REG_CONFIG1);
	ether3_outw((TX_END>>8) - 1, REG_BUFWIN);
	ether3_outw(priv->rx_head, REG_RECVPTR);
	ether3_outw(0, REG_TRANSMITPTR);
	ether3_outw(priv->rx_head >> 8, REG_RECVEND);
	ether3_outw(priv->regs.config2, REG_CONFIG2);
	ether3_outw(priv->regs.config1 | CFG1_LOCBUFMEM, REG_CONFIG1);
	ether3_outw(priv->regs.command, REG_COMMAND);

	i = ether3_ramtest(dev, 0x5A);
	if(i)
		return i;
	i = ether3_ramtest(dev, 0x1E);
	if(i)
		return i;

	ether3_setbuffer(dev, buffer_write, 0);
	ether3_writelong(dev, 0);
	return 0;
}

static void
ether3_init_for_open(struct net_device *dev)
{
	struct dev_priv *priv = (struct dev_priv *)dev->priv;
	int i;

	memset(&priv->stats, 0, sizeof(struct net_device_stats));

	/* Reset the chip */
	ether3_outw(CFG2_RESET, REG_CONFIG2);
	udelay(4);

	priv->regs.command = 0;
	ether3_outw(CMD_RXOFF|CMD_TXOFF, REG_COMMAND);
	while (ether3_inw(REG_STATUS) & (STAT_RXON|STAT_TXON));

	ether3_outw(priv->regs.config1 | CFG1_BUFSELSTAT0, REG_CONFIG1);
	for (i = 0; i < 6; i++)
		ether3_outb(dev->dev_addr[i], REG_BUFWIN);

	priv->tx_head	= 0;
	priv->tx_tail	= 0;
	priv->regs.config2 |= CFG2_CTRLO;
	priv->rx_head	= RX_START;

	ether3_outw(priv->regs.config1 | CFG1_TRANSEND, REG_CONFIG1);
	ether3_outw((TX_END>>8) - 1, REG_BUFWIN);
	ether3_outw(priv->rx_head, REG_RECVPTR);
	ether3_outw(priv->rx_head >> 8, REG_RECVEND);
	ether3_outw(0, REG_TRANSMITPTR);
	ether3_outw(priv->regs.config2, REG_CONFIG2);
	ether3_outw(priv->regs.config1 | CFG1_LOCBUFMEM, REG_CONFIG1);

	ether3_setbuffer(dev, buffer_write, 0);
	ether3_writelong(dev, 0);

	priv->regs.command = CMD_ENINTRX | CMD_ENINTTX;
	ether3_outw(priv->regs.command | CMD_RXON, REG_COMMAND);
}

static inline int
ether3_probe_bus_8(struct net_device *dev, int val)
{
	int write_low, write_high, read_low, read_high;

	write_low = val & 255;
	write_high = val >> 8;

	printk(KERN_DEBUG "ether3_probe: write8 [%02X:%02X]", write_high, write_low);

	ether3_outb(write_low, REG_RECVPTR);
	ether3_outb(write_high, REG_RECVPTR + 1);

	read_low = ether3_inb(REG_RECVPTR);
	read_high = ether3_inb(REG_RECVPTR + 1);

	printk(", read8 [%02X:%02X]\n", read_high, read_low);

	return read_low == write_low && read_high == write_high;
}

static inline int
ether3_probe_bus_16(struct net_device *dev, int val)
{
	int read_val;

	ether3_outw(val, REG_RECVPTR);
	read_val = ether3_inw(REG_RECVPTR);

	printk(KERN_DEBUG "ether3_probe: write16 [%04X], read16 [%04X]\n", val, read_val);

	return read_val == val;
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
ether3_open(struct net_device *dev)
{
	if (!is_valid_ether_addr(dev->dev_addr))
		return -EINVAL;

	if (request_irq(dev->irq, ether3_interrupt, 0, "ether3", dev))
		return -EAGAIN;

	ether3_init_for_open(dev);

	netif_start_queue(dev);

	return 0;
}

/*
 * The inverse routine to ether3_open().
 */
static int
ether3_close(struct net_device *dev)
{
	struct dev_priv *priv = (struct dev_priv *)dev->priv;

	netif_stop_queue(dev);

	disable_irq(dev->irq);

	ether3_outw(CMD_RXOFF|CMD_TXOFF, REG_COMMAND);
	priv->regs.command = 0;
	while (ether3_inw(REG_STATUS) & (STAT_RXON|STAT_TXON));
	ether3_outb(0x80, REG_CONFIG2 + 1);
	ether3_outw(0, REG_COMMAND);

	free_irq(dev->irq, dev);

	return 0;
}

/*
 * Get the current statistics.	This may be called with the card open or
 * closed.
 */
static struct net_device_stats *ether3_getstats(struct net_device *dev)
{
	struct dev_priv *priv = (struct dev_priv *)dev->priv;
	return &priv->stats;
}

static int
ether3_set_mac_address(struct net_device *dev, void *p)
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
 * Set or clear promiscuous/multicast mode filter for this adaptor.
 *
 * We don't attempt any packet filtering.  The card may have a SEEQ 8004
 * in which does not have the other ethernet address registers present...
 */
static void ether3_setmulticastlist(struct net_device *dev)
{
	struct dev_priv *priv = (struct dev_priv *)dev->priv;

	priv->regs.config1 &= ~CFG1_RECVPROMISC;

	if (dev->flags & IFF_PROMISC) {
		/* promiscuous mode */
		priv->regs.config1 |= CFG1_RECVPROMISC;
	} else if (dev->flags & IFF_ALLMULTI) {
		priv->regs.config1 |= CFG1_RECVSPECBRMULTI;
	} else
		priv->regs.config1 |= CFG1_RECVSPECBROAD;

	ether3_outw(priv->regs.config1 | CFG1_LOCBUFMEM, REG_CONFIG1);
}

static void
ether3_timeout(struct net_device *dev)
{
	struct dev_priv *priv = (struct dev_priv *)dev->priv;
	unsigned long flags;

	del_timer(&priv->timer);

	save_flags_cli(flags);
	printk(KERN_ERR "%s: transmit timed out, network cable problem?\n", dev->name);
	printk(KERN_ERR "%s: state: { status=%04X cfg1=%04X cfg2=%04X }\n", dev->name,
		ether3_inw(REG_STATUS), ether3_inw(REG_CONFIG1), ether3_inw(REG_CONFIG2));
	printk(KERN_ERR "%s: { rpr=%04X rea=%04X tpr=%04X }\n", dev->name,
		ether3_inw(REG_RECVPTR), ether3_inw(REG_RECVEND), ether3_inw(REG_TRANSMITPTR));
	printk(KERN_ERR "%s: tx head=%X tx tail=%X\n", dev->name,
		priv->tx_head, priv->tx_tail);
	ether3_setbuffer(dev, buffer_read, priv->tx_tail);
	printk(KERN_ERR "%s: packet status = %08X\n", dev->name, ether3_readlong(dev));
	restore_flags(flags);

	priv->regs.config2 |= CFG2_CTRLO;
	priv->stats.tx_errors += 1;
	ether3_outw(priv->regs.config2, REG_CONFIG2);
	priv->tx_head = priv->tx_tail = 0;

	netif_wake_queue(dev);
}

/*
 * Transmit a packet
 */
static int
ether3_sendpacket(struct sk_buff *skb, struct net_device *dev)
{
	struct dev_priv *priv = (struct dev_priv *)dev->priv;
	unsigned long flags;
	unsigned int length = ETH_ZLEN < skb->len ? skb->len : ETH_ZLEN;
	unsigned int ptr, next_ptr;

	length = (length + 1) & ~1;

	if (priv->broken) {
		dev_kfree_skb(skb);
		priv->stats.tx_dropped ++;
		netif_start_queue(dev);
		return 0;
	}

	if (skb->len != length) {
		skb = skb_padto(skb, length);
		if (!skb)
			goto out;
	}

	next_ptr = (priv->tx_head + 1) & 15;

	save_flags_cli(flags);

	if (priv->tx_tail == next_ptr) {
		restore_flags(flags);
		return 1;	/* unable to queue */
	}

	dev->trans_start = jiffies;
	ptr		 = 0x600 * priv->tx_head;
	priv->tx_head	 = next_ptr;
	next_ptr	*= 0x600;

#define TXHDR_FLAGS (TXHDR_TRANSMIT|TXHDR_CHAINCONTINUE|TXHDR_DATAFOLLOWS|TXHDR_ENSUCCESS)

	ether3_setbuffer(dev, buffer_write, next_ptr);
	ether3_writelong(dev, 0);
	ether3_setbuffer(dev, buffer_write, ptr);
	ether3_writelong(dev, 0);
	ether3_writebuffer(dev, skb->data, length);
	ether3_writeword(dev, htons(next_ptr));
	ether3_writeword(dev, TXHDR_CHAINCONTINUE >> 16);
	ether3_setbuffer(dev, buffer_write, ptr);
	ether3_writeword(dev, htons((ptr + length + 4)));
	ether3_writeword(dev, TXHDR_FLAGS >> 16);
	ether3_ledon(dev, priv);

	if (!(ether3_inw(REG_STATUS) & STAT_TXON)) {
		ether3_outw(ptr, REG_TRANSMITPTR);
		ether3_outw(priv->regs.command | CMD_TXON, REG_COMMAND);
	}

	next_ptr = (priv->tx_head + 1) & 15;
	restore_flags(flags);

	dev_kfree_skb(skb);

	if (priv->tx_tail == next_ptr)
		netif_stop_queue(dev);

 out:
	return 0;
}

static void
ether3_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct net_device *dev = (struct net_device *)dev_id;
	struct dev_priv *priv;
	unsigned int status;

#if NET_DEBUG > 1
	if(net_debug & DEBUG_INT)
		printk("eth3irq: %d ", irq);
#endif

	priv = (struct dev_priv *)dev->priv;

	status = ether3_inw(REG_STATUS);

	if (status & STAT_INTRX) {
		ether3_outw(CMD_ACKINTRX | priv->regs.command, REG_COMMAND);
		ether3_rx(dev, priv, 12);
	}

	if (status & STAT_INTTX) {
		ether3_outw(CMD_ACKINTTX | priv->regs.command, REG_COMMAND);
		ether3_tx(dev, priv);
	}

#if NET_DEBUG > 1
	if(net_debug & DEBUG_INT)
		printk("done\n");
#endif
}

/*
 * If we have a good packet(s), get it/them out of the buffers.
 */
static int
ether3_rx(struct net_device *dev, struct dev_priv *priv, unsigned int maxcnt)
{
	unsigned int next_ptr = priv->rx_head, received = 0;
	ether3_ledon(dev, priv);

	do {
		unsigned int this_ptr, status;
		unsigned char addrs[16];

		/*
		 * read the first 16 bytes from the buffer.
		 * This contains the status bytes etc and ethernet addresses,
		 * and we also check the source ethernet address to see if
		 * it originated from us.
		 */
		{
			unsigned int temp_ptr;
			ether3_setbuffer(dev, buffer_read, next_ptr);
			temp_ptr = ether3_readword(dev);
			status = ether3_readword(dev);
			if ((status & (RXSTAT_DONE | RXHDR_CHAINCONTINUE | RXHDR_RECEIVE)) !=
				(RXSTAT_DONE | RXHDR_CHAINCONTINUE) || !temp_ptr)
				break;

			this_ptr = next_ptr + 4;
			next_ptr = ntohs(temp_ptr);
		}
		ether3_setbuffer(dev, buffer_read, this_ptr);
		ether3_readbuffer(dev, addrs+2, 12);

if (next_ptr < RX_START || next_ptr >= RX_END) {
 int i;
 printk("%s: bad next pointer @%04X: ", dev->name, priv->rx_head);
 printk("%02X %02X %02X %02X ", next_ptr >> 8, next_ptr & 255, status & 255, status >> 8);
 for (i = 2; i < 14; i++)
   printk("%02X ", addrs[i]);
 printk("\n");
 next_ptr = priv->rx_head;
 break;
}
		/*
 		 * ignore our own packets...
	 	 */
		if (!(*(unsigned long *)&dev->dev_addr[0] ^ *(unsigned long *)&addrs[2+6]) &&
		    !(*(unsigned short *)&dev->dev_addr[4] ^ *(unsigned short *)&addrs[2+10])) {
			maxcnt ++; /* compensate for loopedback packet */
			ether3_outw(next_ptr >> 8, REG_RECVEND);
		} else
		if (!(status & (RXSTAT_OVERSIZE|RXSTAT_CRCERROR|RXSTAT_DRIBBLEERROR|RXSTAT_SHORTPACKET))) {
			unsigned int length = next_ptr - this_ptr;
			struct sk_buff *skb;

			if (next_ptr <= this_ptr)
				length += RX_END - RX_START;

			skb = dev_alloc_skb(length + 2);
			if (skb) {
				unsigned char *buf;

				skb->dev = dev;
				skb_reserve(skb, 2);
				buf = skb_put(skb, length);
				ether3_readbuffer(dev, buf + 12, length - 12);
				ether3_outw(next_ptr >> 8, REG_RECVEND);
				*(unsigned short *)(buf + 0)	= *(unsigned short *)(addrs + 2);
				*(unsigned long *)(buf + 2)	= *(unsigned long *)(addrs + 4);
				*(unsigned long *)(buf + 6)	= *(unsigned long *)(addrs + 8);
				*(unsigned short *)(buf + 10)	= *(unsigned short *)(addrs + 12);
				skb->protocol = eth_type_trans(skb, dev);
				netif_rx(skb);
				received ++;
			} else
				goto dropping;
		} else {
			struct net_device_stats *stats = &priv->stats;
			ether3_outw(next_ptr >> 8, REG_RECVEND);
			if (status & RXSTAT_OVERSIZE)	  stats->rx_over_errors ++;
			if (status & RXSTAT_CRCERROR)	  stats->rx_crc_errors ++;
			if (status & RXSTAT_DRIBBLEERROR) stats->rx_fifo_errors ++;
			if (status & RXSTAT_SHORTPACKET)  stats->rx_length_errors ++;
			stats->rx_errors++;
		}
	}
	while (-- maxcnt);

done:
	priv->stats.rx_packets += received;
	priv->rx_head = next_ptr;
	/*
	 * If rx went off line, then that means that the buffer may be full.  We
	 * have dropped at least one packet.
	 */
	if (!(ether3_inw(REG_STATUS) & STAT_RXON)) {
		priv->stats.rx_dropped ++;
    		ether3_outw(next_ptr, REG_RECVPTR);
		ether3_outw(priv->regs.command | CMD_RXON, REG_COMMAND);
	}

	return maxcnt;

dropping:{
	static unsigned long last_warned;

	ether3_outw(next_ptr >> 8, REG_RECVEND);
	/*
	 * Don't print this message too many times...
	 */
	if (time_after(jiffies, last_warned + 10 * HZ)) {
		last_warned = jiffies;
		printk("%s: memory squeeze, dropping packet.\n", dev->name);
	}
	priv->stats.rx_dropped ++;
	goto done;
	}
}

/*
 * Update stats for the transmitted packet(s)
 */
static void
ether3_tx(struct net_device *dev, struct dev_priv *priv)
{
	unsigned int tx_tail = priv->tx_tail;
	int max_work = 14;

	do {
	    	unsigned long status;

    		/*
	    	 * Read the packet header
    		 */
	    	ether3_setbuffer(dev, buffer_read, tx_tail * 0x600);
    		status = ether3_readlong(dev);

		/*
		 * Check to see if this packet has been transmitted
		 */
		if ((status & (TXSTAT_DONE | TXHDR_TRANSMIT)) !=
		    (TXSTAT_DONE | TXHDR_TRANSMIT))
			break;

		/*
		 * Update errors
		 */
		if (!(status & (TXSTAT_BABBLED | TXSTAT_16COLLISIONS)))
			priv->stats.tx_packets++;
		else {
			priv->stats.tx_errors ++;
			if (status & TXSTAT_16COLLISIONS) priv->stats.collisions += 16;
			if (status & TXSTAT_BABBLED) priv->stats.tx_fifo_errors ++;
		}

		tx_tail = (tx_tail + 1) & 15;
	} while (--max_work);

	if (priv->tx_tail != tx_tail) {
		priv->tx_tail = tx_tail;
		netif_wake_queue(dev);
	}
}

static void __init ether3_banner(void)
{
	static unsigned version_printed = 0;

	if (net_debug && version_printed++ == 0)
		printk(KERN_INFO "%s", version);
}

static const char * __init
ether3_get_dev(struct net_device *dev, struct expansion_card *ec)
{
	const char *name = "ether3";
	dev->base_addr = ecard_address(ec, ECARD_MEMC, 0);
	dev->irq = ec->irq;

	if (ec->cid.manufacturer == MANU_ANT &&
	    ec->cid.product == PROD_ANT_ETHERB) {
		dev->base_addr += 0x200;
		name = "etherb";
	}

	ec->irqaddr = (volatile unsigned char *)ioaddr(dev->base_addr);
	ec->irqmask = 0xf0;

	if (ether3_addr(dev->dev_addr, ec))
		name = NULL;

	return name;
}

static struct net_device * __init ether3_init_one(struct expansion_card *ec)
{
	struct net_device *dev;
	struct dev_priv *priv;
	const char *name;
	int i, bus_type;

	ether3_banner();

	ecard_claim(ec);

	dev = init_etherdev(NULL, sizeof(struct dev_priv));
	if (!dev)
		goto out;

	SET_MODULE_OWNER(dev);

	name = ether3_get_dev(dev, ec);
	if (!name)
		goto free;

	/*
	 * this will not fail - the nature of the bus ensures this
	 */
	if (!request_region(dev->base_addr, 128, dev->name))
		goto free;

	priv = (struct dev_priv *) dev->priv;

	/* Reset card...
	 */
	ether3_outb(0x80, REG_CONFIG2 + 1);
	bus_type = BUS_UNKNOWN;
	udelay(4);

	/* Test using Receive Pointer (16-bit register) to find out
	 * how the ether3 is connected to the bus...
	 */
	if (ether3_probe_bus_8(dev, 0x100) &&
	    ether3_probe_bus_8(dev, 0x201))
		bus_type = BUS_8;

	if (bus_type == BUS_UNKNOWN &&
	    ether3_probe_bus_16(dev, 0x101) &&
	    ether3_probe_bus_16(dev, 0x201))
		bus_type = BUS_16;

	switch (bus_type) {
	case BUS_UNKNOWN:
		printk(KERN_ERR "%s: unable to identify bus width\n", dev->name);
		goto failed;

	case BUS_8:
		printk(KERN_ERR "%s: %s found, but is an unsupported "
			"8-bit card\n", dev->name, name);
		goto failed;

	default:
		break;
	}

	printk("%s: %s in slot %d, ", dev->name, name, ec->slot_no);
	for (i = 0; i < 6; i++)
		printk("%2.2x%c", dev->dev_addr[i], i == 5 ? '\n' : ':');

	if (ether3_init_2(dev))
		goto failed;

	dev->open		= ether3_open;
	dev->stop		= ether3_close;
	dev->hard_start_xmit	= ether3_sendpacket;
	dev->get_stats		= ether3_getstats;
	dev->set_multicast_list	= ether3_setmulticastlist;
	dev->set_mac_address	= ether3_set_mac_address;
	dev->tx_timeout		= ether3_timeout;
	dev->watchdog_timeo	= 5 * HZ / 100;
	return 0;

failed:
	release_region(dev->base_addr, 128);
free:
	unregister_netdev(dev);
	kfree(dev);
out:
	ecard_release(ec);
	return NULL;
}

static struct expansion_card	*e_card[MAX_ECARDS];
static struct net_device	*e_dev[MAX_ECARDS];

static int ether3_init(void)
{
	int i, ret = -ENODEV;

	ecard_startfind();

	for (i = 0; i < MAX_ECARDS; i++) {
		struct net_device *dev;
		struct expansion_card *ec;

		ec = ecard_find(0, ether3_cids);
		if (!ec)
			break;

		dev = ether3_init_one(ec);
		if (!dev)
			break;

		e_card[i] = ec;
		e_dev[i]  = dev;
		ret = 0;
	}

	return ret;
}

static void ether3_exit(void)
{
	int i;

	for (i = 0; i < MAX_ECARDS; i++) {
		if (e_dev[i]) {
			unregister_netdev(e_dev[i]);
			release_region(e_dev[i]->base_addr, 128);
			kfree(e_dev[i]);
			e_dev[i] = NULL;
		}
		if (e_card[i]) {
			ecard_release(e_card[i]);
			e_card[i] = NULL;
		}
	}
}

module_init(ether3_init);
module_exit(ether3_exit);

MODULE_LICENSE("GPL");
