/*
 *  linux/drivers/acorn/net/ether1.c
 *
 *  Copyright (C) 1996-2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Acorn ether1 driver (82586 chip) for Acorn machines
 *
 * We basically keep two queues in the cards memory - one for transmit
 * and one for receive.  Each has a head and a tail.  The head is where
 * we/the chip adds packets to be transmitted/received, and the tail
 * is where the transmitter has got to/where the receiver will stop.
 * Both of these queues are circular, and since the chip is running
 * all the time, we have to be careful when we modify the pointers etc
 * so that the buffer memory contents is valid all the time.
 *
 * Change log:
 * 1.00	RMK			Released
 * 1.01	RMK	19/03/1996	Transfers the last odd byte onto/off of the card now.
 * 1.02	RMK	25/05/1997	Added code to restart RU if it goes not ready
 * 1.03	RMK	14/09/1997	Cleaned up the handling of a reset during the TX interrupt.
 *				Should prevent lockup.
 * 1.04 RMK	17/09/1997	Added more info when initialsation of chip goes wrong.
 *				TDR now only reports failure when chip reports non-zero
 *				TDR time-distance.
 * 1.05	RMK	31/12/1997	Removed calls to dev_tint for 2.1
 * 1.06	RMK	10/02/2000	Updated for 2.3.43
 * 1.07	RMK	13/05/2000	Updated for 2.3.99-pre8
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
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#include <asm/system.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/ecard.h>

#define __ETHER1_C
#include "ether1.h"

static unsigned int net_debug = NET_DEBUG;

#define BUFFER_SIZE	0x10000
#define TX_AREA_START	0x00100
#define TX_AREA_END	0x05000
#define RX_AREA_START	0x05000
#define RX_AREA_END	0x0fc00

static int ether1_open(struct net_device *dev);
static int ether1_sendpacket(struct sk_buff *skb, struct net_device *dev);
static void ether1_interrupt(int irq, void *dev_id, struct pt_regs *regs);
static int ether1_close(struct net_device *dev);
static struct net_device_stats *ether1_getstats(struct net_device *dev);
static void ether1_setmulticastlist(struct net_device *dev);
static void ether1_timeout(struct net_device *dev);

/* ------------------------------------------------------------------------- */

static char version[] __initdata = "ether1 ethernet driver (c) 2000 Russell King v1.07\n";

#define BUS_16 16
#define BUS_8  8

static const card_ids __init ether1_cids[] = {
	{ MANU_ACORN, PROD_ACORN_ETHER1 },
	{ 0xffff, 0xffff }
};

/* ------------------------------------------------------------------------- */

#define DISABLEIRQS 1
#define NORMALIRQS  0

#define ether1_inw(dev, addr, type, offset, svflgs) ether1_inw_p (dev, addr + (int)(&((type *)0)->offset), svflgs)
#define ether1_outw(dev, val, addr, type, offset, svflgs) ether1_outw_p (dev, val, addr + (int)(&((type *)0)->offset), svflgs)

static inline unsigned short
ether1_inw_p (struct net_device *dev, int addr, int svflgs)
{
	unsigned long flags;
	unsigned short ret;

	if (svflgs) {
		save_flags_cli (flags);
	}
	outb (addr >> 12, REG_PAGE);
	ret = inw (ETHER1_RAM + ((addr & 4095) >> 1));
	if (svflgs)
		restore_flags (flags);
	return ret;
}

static inline void
ether1_outw_p (struct net_device *dev, unsigned short val, int addr, int svflgs)
{
	unsigned long flags;

	if (svflgs) {
		save_flags_cli (flags);
	}
	outb (addr >> 12, REG_PAGE);
	outw (val, ETHER1_RAM + ((addr & 4095) >> 1));
	if (svflgs)
		restore_flags (flags);
}

/*
 * Some inline assembler to allow fast transfers on to/off of the card.
 * Since this driver depends on some features presented by the ARM
 * specific architecture, and that you can't configure this driver
 * without specifiing ARM mode, this is not a problem.
 *
 * This routine is essentially an optimised memcpy from the card's
 * onboard RAM to kernel memory.
 */
static void
ether1_writebuffer (struct net_device *dev, void *data, unsigned int start, unsigned int length)
{
	unsigned int page, thislen, offset, addr;

	offset = start & 4095;
	page = start >> 12;
	addr = ioaddr(ETHER1_RAM + (offset >> 1));

	if (offset + length > 4096)
		thislen = 4096 - offset;
	else
		thislen = length;

	do {
		int used;

		outb(page, REG_PAGE);
		length -= thislen;

		__asm__ __volatile__(
	"subs	%3, %3, #2
	bmi	2f
1:	ldr	%0, [%1], #2
	mov	%0, %0, lsl #16
	orr	%0, %0, %0, lsr #16
	str	%0, [%2], #4
	subs	%3, %3, #2
	bmi	2f
	ldr	%0, [%1], #2
	mov	%0, %0, lsl #16
	orr	%0, %0, %0, lsr #16
	str	%0, [%2], #4
	subs	%3, %3, #2
	bmi	2f
	ldr	%0, [%1], #2
	mov	%0, %0, lsl #16
	orr	%0, %0, %0, lsr #16
	str	%0, [%2], #4
	subs	%3, %3, #2
	bmi	2f
	ldr	%0, [%1], #2
	mov	%0, %0, lsl #16
	orr	%0, %0, %0, lsr #16
	str	%0, [%2], #4
	subs	%3, %3, #2
	bpl	1b
2:	adds	%3, %3, #1
	ldreqb	%0, [%1]
	streqb	%0, [%2]"
		: "=&r" (used), "=&r" (data)
		: "r"  (addr), "r" (thislen), "1" (data));

		addr = ioaddr(ETHER1_RAM);

		thislen = length;
		if (thislen > 4096)
			thislen = 4096;
		page++;
	} while (thislen);
}

static void
ether1_readbuffer (struct net_device *dev, void *data, unsigned int start, unsigned int length)
{
	unsigned int page, thislen, offset, addr;

	offset = start & 4095;
	page = start >> 12;
	addr = ioaddr(ETHER1_RAM + (offset >> 1));

	if (offset + length > 4096)
		thislen = 4096 - offset;
	else
		thislen = length;

	do {
		int used;

		outb(page, REG_PAGE);
		length -= thislen;

		__asm__ __volatile__(
	"subs	%3, %3, #2
	bmi	2f
1:	ldr	%0, [%2], #4
	strb	%0, [%1], #1
	mov	%0, %0, lsr #8
	strb	%0, [%1], #1
	subs	%3, %3, #2
	bmi	2f
	ldr	%0, [%2], #4
	strb	%0, [%1], #1
	mov	%0, %0, lsr #8
	strb	%0, [%1], #1
	subs	%3, %3, #2
	bmi	2f
	ldr	%0, [%2], #4
	strb	%0, [%1], #1
	mov	%0, %0, lsr #8
	strb	%0, [%1], #1
	subs	%3, %3, #2
	bmi	2f
	ldr	%0, [%2], #4
	strb	%0, [%1], #1
	mov	%0, %0, lsr #8
	strb	%0, [%1], #1
	subs	%3, %3, #2
	bpl	1b
2:	adds	%3, %3, #1
	ldreqb	%0, [%2]
	streqb	%0, [%1]"
		: "=&r" (used), "=&r" (data)
		: "r"  (addr), "r" (thislen), "1" (data));

		addr = ioaddr(ETHER1_RAM);

		thislen = length;
		if (thislen > 4096)
			thislen = 4096;
		page++;
	} while (thislen);
}

static int __init
ether1_ramtest(struct net_device *dev, unsigned char byte)
{
	unsigned char *buffer = kmalloc (BUFFER_SIZE, GFP_KERNEL);
	int i, ret = BUFFER_SIZE;
	int max_errors = 15;
	int bad = -1;
	int bad_start = 0;

	if (!buffer)
		return 1;

	memset (buffer, byte, BUFFER_SIZE);
	ether1_writebuffer (dev, buffer, 0, BUFFER_SIZE);
	memset (buffer, byte ^ 0xff, BUFFER_SIZE);
	ether1_readbuffer (dev, buffer, 0, BUFFER_SIZE);

	for (i = 0; i < BUFFER_SIZE; i++) {
		if (buffer[i] != byte) {
			if (max_errors >= 0 && bad != buffer[i]) {
				if (bad != -1)
					printk ("\n");
				printk (KERN_CRIT "%s: RAM failed with (%02X instead of %02X) at 0x%04X",
					dev->name, buffer[i], byte, i);
				ret = -ENODEV;
				max_errors --;
				bad = buffer[i];
				bad_start = i;
			}
		} else {
			if (bad != -1) {
			    	if (bad_start == i - 1)
					printk ("\n");
				else
					printk (" - 0x%04X\n", i - 1);
				bad = -1;
			}
		}
	}

	if (bad != -1)
		printk (" - 0x%04X\n", BUFFER_SIZE);
	kfree (buffer);

	return ret;
}

static int
ether1_reset (struct net_device *dev)
{
	outb (CTRL_RST|CTRL_ACK, REG_CONTROL);
	return BUS_16;
}

static int __init
ether1_init_2(struct net_device *dev)
{
	int i;
	dev->mem_start = 0;

	i = ether1_ramtest (dev, 0x5a);

	if (i > 0)
		i = ether1_ramtest (dev, 0x1e);

	if (i <= 0)
	    	return -ENODEV;

	dev->mem_end = i;
	return 0;
}

/*
 * These are the structures that are loaded into the ether RAM card to
 * initialise the 82586
 */

/* at 0x0100 */
#define NOP_ADDR	(TX_AREA_START)
#define NOP_SIZE	(0x06)
static nop_t  init_nop  = {
	0,
	CMD_NOP,
	NOP_ADDR
};

/* at 0x003a */
#define TDR_ADDR	(0x003a)
#define TDR_SIZE	(0x08)
static tdr_t  init_tdr	= {
	0,
	CMD_TDR | CMD_INTR,
	NOP_ADDR,
	0
};

/* at 0x002e */
#define MC_ADDR		(0x002e)
#define MC_SIZE		(0x0c)
static mc_t   init_mc   = {
	0,
	CMD_SETMULTICAST,
	TDR_ADDR,
	0,
	{ { 0, } }
};

/* at 0x0022 */
#define SA_ADDR		(0x0022)
#define SA_SIZE		(0x0c)
static sa_t   init_sa   = {
	0,
	CMD_SETADDRESS,
	MC_ADDR,
	{ 0, }
};

/* at 0x0010 */
#define CFG_ADDR	(0x0010)
#define CFG_SIZE	(0x12)
static cfg_t  init_cfg  = {
	0,
	CMD_CONFIG,
	SA_ADDR,
	8,
	8,
	CFG8_SRDY,
	CFG9_PREAMB8 | CFG9_ADDRLENBUF | CFG9_ADDRLEN(6),
	0,
	0x60,
	0,
	CFG13_RETRY(15) | CFG13_SLOTH(2),
	0,
};

/* at 0x0000 */
#define SCB_ADDR	(0x0000)
#define SCB_SIZE	(0x10)
static scb_t  init_scb  = {
	0,
	SCB_CMDACKRNR | SCB_CMDACKCNA | SCB_CMDACKFR | SCB_CMDACKCX,
	CFG_ADDR,
	RX_AREA_START,
	0,
	0,
	0,
	0
};

/* at 0xffee */
#define ISCP_ADDR	(0xffee)
#define ISCP_SIZE	(0x08)
static iscp_t init_iscp = {
	1,
	SCB_ADDR,
	0x0000,
	0x0000
};

/* at 0xfff6 */
#define SCP_ADDR	(0xfff6)
#define SCP_SIZE	(0x0a)
static scp_t  init_scp  = {
	SCP_SY_16BBUS,
	{ 0, 0 },
	ISCP_ADDR,
	0
};

#define RFD_SIZE	(0x16)
static rfd_t  init_rfd	= {
	0,
	0,
	0,
	0,
	{ 0, },
	{ 0, },
	0
};

#define RBD_SIZE	(0x0a)
static rbd_t  init_rbd	= {
	0,
	0,
	0,
	0,
	ETH_FRAME_LEN + 8
};

#define TX_SIZE		(0x08)
#define TBD_SIZE	(0x08)

static int
ether1_init_for_open (struct net_device *dev)
{
	struct ether1_priv *priv = (struct ether1_priv *)dev->priv;
	int i, status, addr, next, next2;
	int failures = 0;

	outb (CTRL_RST|CTRL_ACK, REG_CONTROL);

	for (i = 0; i < 6; i++)
		init_sa.sa_addr[i] = dev->dev_addr[i];

	/* load data structures into ether1 RAM */
	ether1_writebuffer (dev, &init_scp,  SCP_ADDR,  SCP_SIZE);
	ether1_writebuffer (dev, &init_iscp, ISCP_ADDR, ISCP_SIZE);
	ether1_writebuffer (dev, &init_scb,  SCB_ADDR,  SCB_SIZE);
	ether1_writebuffer (dev, &init_cfg,  CFG_ADDR,  CFG_SIZE);
	ether1_writebuffer (dev, &init_sa,   SA_ADDR,   SA_SIZE);
	ether1_writebuffer (dev, &init_mc,   MC_ADDR,   MC_SIZE);
	ether1_writebuffer (dev, &init_tdr,  TDR_ADDR,  TDR_SIZE);
	ether1_writebuffer (dev, &init_nop,  NOP_ADDR,  NOP_SIZE);

	if (ether1_inw (dev, CFG_ADDR, cfg_t, cfg_command, NORMALIRQS) != CMD_CONFIG) {
		printk (KERN_ERR "%s: detected either RAM fault or compiler bug\n",
			dev->name);
		return 1;
	}

	/*
	 * setup circularly linked list of { rfd, rbd, buffer }, with
	 * all rfds circularly linked, rbds circularly linked.
	 * First rfd is linked to scp, first rbd is linked to first
	 * rfd.  Last rbd has a suspend command.
	 */
	addr = RX_AREA_START;
	do {
		next = addr + RFD_SIZE + RBD_SIZE + ETH_FRAME_LEN + 10;
		next2 = next + RFD_SIZE + RBD_SIZE + ETH_FRAME_LEN + 10;

		if (next2 >= RX_AREA_END) {
			next = RX_AREA_START;
			init_rfd.rfd_command = RFD_CMDEL | RFD_CMDSUSPEND;
			priv->rx_tail = addr;
		} else
			init_rfd.rfd_command = 0;
		if (addr == RX_AREA_START)
			init_rfd.rfd_rbdoffset = addr + RFD_SIZE;
		else
			init_rfd.rfd_rbdoffset = 0;
		init_rfd.rfd_link = next;
		init_rbd.rbd_link = next + RFD_SIZE;
		init_rbd.rbd_bufl = addr + RFD_SIZE + RBD_SIZE;

		ether1_writebuffer (dev, &init_rfd, addr, RFD_SIZE);
		ether1_writebuffer (dev, &init_rbd, addr + RFD_SIZE, RBD_SIZE);
		addr = next;
	} while (next2 < RX_AREA_END);

	priv->tx_link = NOP_ADDR;
	priv->tx_head = NOP_ADDR + NOP_SIZE;
	priv->tx_tail = TDR_ADDR;
	priv->rx_head = RX_AREA_START;

	/* release reset & give 586 a prod */
	priv->resetting = 1;
	priv->initialising = 1;
	outb (CTRL_RST, REG_CONTROL);
	outb (0, REG_CONTROL);
	outb (CTRL_CA, REG_CONTROL);

	/* 586 should now unset iscp.busy */
	i = jiffies + HZ/2;
	while (ether1_inw (dev, ISCP_ADDR, iscp_t, iscp_busy, DISABLEIRQS) == 1) {
		if (time_after(jiffies, i)) {
			printk (KERN_WARNING "%s: can't initialise 82586: iscp is busy\n", dev->name);
			return 1;
		}
	}

	/* check status of commands that we issued */
	i += HZ/10;
	while (((status = ether1_inw (dev, CFG_ADDR, cfg_t, cfg_status, DISABLEIRQS))
			& STAT_COMPLETE) == 0) {
		if (time_after(jiffies, i))
			break;
	}

	if ((status & (STAT_COMPLETE | STAT_OK)) != (STAT_COMPLETE | STAT_OK)) {
		printk (KERN_WARNING "%s: can't initialise 82586: config status %04X\n", dev->name, status);
		printk (KERN_DEBUG "%s: SCB=[STS=%04X CMD=%04X CBL=%04X RFA=%04X]\n", dev->name,
			ether1_inw (dev, SCB_ADDR, scb_t, scb_status, NORMALIRQS),
			ether1_inw (dev, SCB_ADDR, scb_t, scb_command, NORMALIRQS),
			ether1_inw (dev, SCB_ADDR, scb_t, scb_cbl_offset, NORMALIRQS),
			ether1_inw (dev, SCB_ADDR, scb_t, scb_rfa_offset, NORMALIRQS));
		failures += 1;
	}

	i += HZ/10;
	while (((status = ether1_inw (dev, SA_ADDR, sa_t, sa_status, DISABLEIRQS))
			& STAT_COMPLETE) == 0) {
		if (time_after(jiffies, i))
			break;
	}

	if ((status & (STAT_COMPLETE | STAT_OK)) != (STAT_COMPLETE | STAT_OK)) {
		printk (KERN_WARNING "%s: can't initialise 82586: set address status %04X\n", dev->name, status);
		printk (KERN_DEBUG "%s: SCB=[STS=%04X CMD=%04X CBL=%04X RFA=%04X]\n", dev->name,
			ether1_inw (dev, SCB_ADDR, scb_t, scb_status, NORMALIRQS),
			ether1_inw (dev, SCB_ADDR, scb_t, scb_command, NORMALIRQS),
			ether1_inw (dev, SCB_ADDR, scb_t, scb_cbl_offset, NORMALIRQS),
			ether1_inw (dev, SCB_ADDR, scb_t, scb_rfa_offset, NORMALIRQS));
		failures += 1;
	}

	i += HZ/10;
	while (((status = ether1_inw (dev, MC_ADDR, mc_t, mc_status, DISABLEIRQS))
			& STAT_COMPLETE) == 0) {
		if (time_after(jiffies, i))
			break;
	}

	if ((status & (STAT_COMPLETE | STAT_OK)) != (STAT_COMPLETE | STAT_OK)) {
		printk (KERN_WARNING "%s: can't initialise 82586: set multicast status %04X\n", dev->name, status);
		printk (KERN_DEBUG "%s: SCB=[STS=%04X CMD=%04X CBL=%04X RFA=%04X]\n", dev->name,
			ether1_inw (dev, SCB_ADDR, scb_t, scb_status, NORMALIRQS),
			ether1_inw (dev, SCB_ADDR, scb_t, scb_command, NORMALIRQS),
			ether1_inw (dev, SCB_ADDR, scb_t, scb_cbl_offset, NORMALIRQS),
			ether1_inw (dev, SCB_ADDR, scb_t, scb_rfa_offset, NORMALIRQS));
		failures += 1;
	}

	i += HZ;
	while (((status = ether1_inw (dev, TDR_ADDR, tdr_t, tdr_status, DISABLEIRQS))
			& STAT_COMPLETE) == 0) {
		if (time_after(jiffies, i))
			break;
	}

	if ((status & (STAT_COMPLETE | STAT_OK)) != (STAT_COMPLETE | STAT_OK)) {
		printk (KERN_WARNING "%s: can't tdr (ignored)\n", dev->name);
		printk (KERN_DEBUG "%s: SCB=[STS=%04X CMD=%04X CBL=%04X RFA=%04X]\n", dev->name,
			ether1_inw (dev, SCB_ADDR, scb_t, scb_status, NORMALIRQS),
			ether1_inw (dev, SCB_ADDR, scb_t, scb_command, NORMALIRQS),
			ether1_inw (dev, SCB_ADDR, scb_t, scb_cbl_offset, NORMALIRQS),
			ether1_inw (dev, SCB_ADDR, scb_t, scb_rfa_offset, NORMALIRQS));
	} else {
		status = ether1_inw (dev, TDR_ADDR, tdr_t, tdr_result, DISABLEIRQS);
		if (status & TDR_XCVRPROB)
			printk (KERN_WARNING "%s: i/f failed tdr: transceiver problem\n", dev->name);
		else if ((status & (TDR_SHORT|TDR_OPEN)) && (status & TDR_TIME)) {
#ifdef FANCY
			printk (KERN_WARNING "%s: i/f failed tdr: cable %s %d.%d us away\n", dev->name,
				status & TDR_SHORT ? "short" : "open", (status & TDR_TIME) / 10,
				(status & TDR_TIME) % 10);
#else
			printk (KERN_WARNING "%s: i/f failed tdr: cable %s %d clks away\n", dev->name,
				status & TDR_SHORT ? "short" : "open", (status & TDR_TIME));
#endif
		}
	}

	if (failures)
		ether1_reset (dev);
	return failures ? 1 : 0;
}

/* ------------------------------------------------------------------------- */

static int
ether1_txalloc (struct net_device *dev, int size)
{
	struct ether1_priv *priv = (struct ether1_priv *)dev->priv;
	int start, tail;

	size = (size + 1) & ~1;
	tail = priv->tx_tail;

	if (priv->tx_head + size > TX_AREA_END) {
		if (tail > priv->tx_head)
			return -1;
		start = TX_AREA_START;
		if (start + size > tail)
			return -1;
		priv->tx_head = start + size;
	} else {
		if (priv->tx_head < tail && (priv->tx_head + size) > tail)
			return -1;
		start = priv->tx_head;
		priv->tx_head += size;
	}

	return start;
}

static int
ether1_open (struct net_device *dev)
{
	struct ether1_priv *priv = (struct ether1_priv *)dev->priv;

	if (!is_valid_ether_addr(dev->dev_addr)) {
		printk("%s: invalid ethernet MAC address\n", dev->name);
		return -EINVAL;
	}

	if (request_irq(dev->irq, ether1_interrupt, 0, "ether1", dev))
		return -EAGAIN;

	memset (&priv->stats, 0, sizeof (struct net_device_stats));

	if (ether1_init_for_open (dev)) {
		free_irq (dev->irq, dev);
		return -EAGAIN;
	}

	netif_start_queue(dev);

	return 0;
}

static void
ether1_timeout(struct net_device *dev)
{
	struct ether1_priv *priv = (struct ether1_priv *)dev->priv;

	printk(KERN_WARNING "%s: transmit timeout, network cable problem?\n",
		dev->name);
	printk(KERN_WARNING "%s: resetting device\n", dev->name);

	ether1_reset (dev);

	if (ether1_init_for_open (dev))
		printk (KERN_ERR "%s: unable to restart interface\n", dev->name);

	priv->stats.tx_errors++;
	netif_wake_queue(dev);
}

static int
ether1_sendpacket (struct sk_buff *skb, struct net_device *dev)
{
	struct ether1_priv *priv = (struct ether1_priv *)dev->priv;
	int tmp, tst, nopaddr, txaddr, tbdaddr, dataddr;
	unsigned long flags;
	tx_t tx;
	tbd_t tbd;
	nop_t nop;

	if (priv->restart) {
		printk(KERN_WARNING "%s: resetting device\n", dev->name);

		ether1_reset(dev);

		if (ether1_init_for_open(dev))
			printk(KERN_ERR "%s: unable to restart interface\n", dev->name);
		else
			priv->restart = 0;
	}

	if (skb->len < ETH_ZLEN) {
		skb = skb_padto(skb, ETH_ZLEN);
		if (!skb)
			goto out;
	}

	/*
	 * insert packet followed by a nop
	 */
	txaddr = ether1_txalloc (dev, TX_SIZE);
	tbdaddr = ether1_txalloc (dev, TBD_SIZE);
	dataddr = ether1_txalloc (dev, skb->len);
	nopaddr = ether1_txalloc (dev, NOP_SIZE);

	tx.tx_status = 0;
	tx.tx_command = CMD_TX | CMD_INTR;
	tx.tx_link = nopaddr;
	tx.tx_tbdoffset = tbdaddr;
	tbd.tbd_opts = TBD_EOL | skb->len;
	tbd.tbd_link = I82586_NULL;
	tbd.tbd_bufl = dataddr;
	tbd.tbd_bufh = 0;
	nop.nop_status = 0;
	nop.nop_command = CMD_NOP;
	nop.nop_link = nopaddr;

	save_flags_cli(flags);
	ether1_writebuffer (dev, &tx, txaddr, TX_SIZE);
	ether1_writebuffer (dev, &tbd, tbdaddr, TBD_SIZE);
	ether1_writebuffer (dev, skb->data, dataddr, skb->len);
	ether1_writebuffer (dev, &nop, nopaddr, NOP_SIZE);
	tmp = priv->tx_link;
	priv->tx_link = nopaddr;

	/* now reset the previous nop pointer */
	ether1_outw (dev, txaddr, tmp, nop_t, nop_link, NORMALIRQS);

	restore_flags(flags);

	/* handle transmit */
	dev->trans_start = jiffies;

	/* check to see if we have room for a full sized ether frame */
	tmp = priv->tx_head;
	tst = ether1_txalloc (dev, TX_SIZE + TBD_SIZE + NOP_SIZE + ETH_FRAME_LEN);
	priv->tx_head = tmp;
	dev_kfree_skb (skb);

	if (tst == -1)
		netif_stop_queue(dev);

 out:
	return 0;
}

static void
ether1_xmit_done (struct net_device *dev)
{
	struct ether1_priv *priv = (struct ether1_priv *)dev->priv;
	nop_t nop;
	int caddr, tst;

	caddr = priv->tx_tail;

again:
	ether1_readbuffer (dev, &nop, caddr, NOP_SIZE);

	switch (nop.nop_command & CMD_MASK) {
	case CMD_TDR:
		/* special case */
		if (ether1_inw (dev, SCB_ADDR, scb_t, scb_cbl_offset, NORMALIRQS)
				!= (unsigned short)I82586_NULL) {
			ether1_outw(dev, SCB_CMDCUCSTART | SCB_CMDRXSTART, SCB_ADDR, scb_t,
				    scb_command, NORMALIRQS);
			outb (CTRL_CA, REG_CONTROL);
		}
		priv->tx_tail = NOP_ADDR;
		return;

	case CMD_NOP:
		if (nop.nop_link == caddr) {
			if (priv->initialising == 0)
				printk (KERN_WARNING "%s: strange command complete with no tx command!\n", dev->name);
			else
			        priv->initialising = 0;
			return;
		}
		if (caddr == nop.nop_link)
			return;
		caddr = nop.nop_link;
		goto again;

	case CMD_TX:
		if (nop.nop_status & STAT_COMPLETE)
			break;
		printk (KERN_ERR "%s: strange command complete without completed command\n", dev->name);
		priv->restart = 1;
		return;

	default:
		printk (KERN_WARNING "%s: strange command %d complete! (offset %04X)", dev->name,
			nop.nop_command & CMD_MASK, caddr);
		priv->restart = 1;
		return;
	}

	while (nop.nop_status & STAT_COMPLETE) {
		if (nop.nop_status & STAT_OK) {
			priv->stats.tx_packets ++;
			priv->stats.collisions += (nop.nop_status & STAT_COLLISIONS);
		} else {
			priv->stats.tx_errors ++;

			if (nop.nop_status & STAT_COLLAFTERTX)
				priv->stats.collisions ++;
			if (nop.nop_status & STAT_NOCARRIER)
				priv->stats.tx_carrier_errors ++;
			if (nop.nop_status & STAT_TXLOSTCTS)
				printk (KERN_WARNING "%s: cts lost\n", dev->name);
			if (nop.nop_status & STAT_TXSLOWDMA)
				priv->stats.tx_fifo_errors ++;
			if (nop.nop_status & STAT_COLLEXCESSIVE)
				priv->stats.collisions += 16;
		}

		if (nop.nop_link == caddr) {
			printk (KERN_ERR "%s: tx buffer chaining error: tx command points to itself\n", dev->name);
			break;
		}

		caddr = nop.nop_link;
		ether1_readbuffer (dev, &nop, caddr, NOP_SIZE);
		if ((nop.nop_command & CMD_MASK) != CMD_NOP) {
			printk (KERN_ERR "%s: tx buffer chaining error: no nop after tx command\n", dev->name);
			break;
		}

		if (caddr == nop.nop_link)
			break;

		caddr = nop.nop_link;
		ether1_readbuffer (dev, &nop, caddr, NOP_SIZE);
		if ((nop.nop_command & CMD_MASK) != CMD_TX) {
			printk (KERN_ERR "%s: tx buffer chaining error: no tx command after nop\n", dev->name);
			break;
		}
	}
	priv->tx_tail = caddr;

	caddr = priv->tx_head;
	tst = ether1_txalloc (dev, TX_SIZE + TBD_SIZE + NOP_SIZE + ETH_FRAME_LEN);
	priv->tx_head = caddr;
	if (tst != -1)
		netif_wake_queue(dev);
}

static void
ether1_recv_done (struct net_device *dev)
{
	struct ether1_priv *priv = (struct ether1_priv *)dev->priv;
	int status;
	int nexttail, rbdaddr;
	rbd_t rbd;

	do {
		status = ether1_inw (dev, priv->rx_head, rfd_t, rfd_status, NORMALIRQS);
		if ((status & RFD_COMPLETE) == 0)
			break;

		rbdaddr = ether1_inw (dev, priv->rx_head, rfd_t, rfd_rbdoffset, NORMALIRQS);
		ether1_readbuffer (dev, &rbd, rbdaddr, RBD_SIZE);

		if ((rbd.rbd_status & (RBD_EOF | RBD_ACNTVALID)) == (RBD_EOF | RBD_ACNTVALID)) {
			int length = rbd.rbd_status & RBD_ACNT;
			struct sk_buff *skb;

			length = (length + 1) & ~1;
			skb = dev_alloc_skb (length + 2);

			if (skb) {
				skb->dev = dev;
				skb_reserve (skb, 2);

				ether1_readbuffer (dev, skb_put (skb, length), rbd.rbd_bufl, length);

				skb->protocol = eth_type_trans (skb, dev);
				netif_rx (skb);
				priv->stats.rx_packets ++;
			} else
				priv->stats.rx_dropped ++;
		} else {
			printk(KERN_WARNING "%s: %s\n", dev->name,
				(rbd.rbd_status & RBD_EOF) ? "oversized packet" : "acnt not valid");
			priv->stats.rx_dropped ++;
		}

		nexttail = ether1_inw (dev, priv->rx_tail, rfd_t, rfd_link, NORMALIRQS);
		/* nexttail should be rx_head */
		if (nexttail != priv->rx_head)
			printk(KERN_ERR "%s: receiver buffer chaining error (%04X != %04X)\n",
				dev->name, nexttail, priv->rx_head);
		ether1_outw (dev, RFD_CMDEL | RFD_CMDSUSPEND, nexttail, rfd_t, rfd_command, NORMALIRQS);
		ether1_outw (dev, 0, priv->rx_tail, rfd_t, rfd_command, NORMALIRQS);
		ether1_outw (dev, 0, priv->rx_tail, rfd_t, rfd_status, NORMALIRQS);
		ether1_outw (dev, 0, priv->rx_tail, rfd_t, rfd_rbdoffset, NORMALIRQS);
	
		priv->rx_tail = nexttail;
		priv->rx_head = ether1_inw (dev, priv->rx_head, rfd_t, rfd_link, NORMALIRQS);
	} while (1);
}

static void
ether1_interrupt (int irq, void *dev_id, struct pt_regs *regs)
{
	struct net_device *dev = (struct net_device *)dev_id;
	struct ether1_priv *priv = (struct ether1_priv *)dev->priv;
	int status;

	status = ether1_inw (dev, SCB_ADDR, scb_t, scb_status, NORMALIRQS);

	if (status) {
		ether1_outw(dev, status & (SCB_STRNR | SCB_STCNA | SCB_STFR | SCB_STCX),
			    SCB_ADDR, scb_t, scb_command, NORMALIRQS);
		outb (CTRL_CA | CTRL_ACK, REG_CONTROL);
		if (status & SCB_STCX) {
			ether1_xmit_done (dev);
		}
		if (status & SCB_STCNA) {
			if (priv->resetting == 0)
				printk (KERN_WARNING "%s: CU went not ready ???\n", dev->name);
			else
				priv->resetting += 1;
			if (ether1_inw (dev, SCB_ADDR, scb_t, scb_cbl_offset, NORMALIRQS)
					!= (unsigned short)I82586_NULL) {
				ether1_outw (dev, SCB_CMDCUCSTART, SCB_ADDR, scb_t, scb_command, NORMALIRQS);
				outb (CTRL_CA, REG_CONTROL);
			}
			if (priv->resetting == 2)
				priv->resetting = 0;
		}
		if (status & SCB_STFR) {
			ether1_recv_done (dev);
		}
		if (status & SCB_STRNR) {
			if (ether1_inw (dev, SCB_ADDR, scb_t, scb_status, NORMALIRQS) & SCB_STRXSUSP) {
				printk (KERN_WARNING "%s: RU went not ready: RU suspended\n", dev->name);
				ether1_outw (dev, SCB_CMDRXRESUME, SCB_ADDR, scb_t, scb_command, NORMALIRQS);
				outb (CTRL_CA, REG_CONTROL);
				priv->stats.rx_dropped ++;	/* we suspended due to lack of buffer space */
			} else
				printk(KERN_WARNING "%s: RU went not ready: %04X\n", dev->name,
					ether1_inw (dev, SCB_ADDR, scb_t, scb_status, NORMALIRQS));
			printk (KERN_WARNING "RU ptr = %04X\n", ether1_inw (dev, SCB_ADDR, scb_t, scb_rfa_offset,
						NORMALIRQS));
		}
	} else
	        outb (CTRL_ACK, REG_CONTROL);
}

static int
ether1_close (struct net_device *dev)
{
	ether1_reset (dev);

	free_irq(dev->irq, dev);

	return 0;
}

static struct net_device_stats *
ether1_getstats (struct net_device *dev)
{
	struct ether1_priv *priv = (struct ether1_priv *)dev->priv;
	return &priv->stats;
}

static int
ether1_set_mac_address(struct net_device *dev, void *p)
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
 * Set or clear the multicast filter for this adaptor.
 * num_addrs == -1	Promiscuous mode, receive all packets.
 * num_addrs == 0	Normal mode, clear multicast list.
 * num_addrs > 0	Multicast mode, receive normal and MC packets, and do
 *			best-effort filtering.
 */
static void
ether1_setmulticastlist (struct net_device *dev)
{
}

/* ------------------------------------------------------------------------- */

static void __init ether1_banner(void)
{
	static unsigned int version_printed = 0;

	if (net_debug && version_printed++ == 0)
		printk(KERN_INFO "%s", version);
}

static struct net_device * __init ether1_init_one(struct expansion_card *ec)
{
	struct net_device *dev;
	struct ether1_priv *priv;
	int i;

	ether1_banner();

	ecard_claim(ec);

	dev = init_etherdev(NULL, sizeof(struct ether1_priv));
	if (!dev)
		goto out;

	SET_MODULE_OWNER(dev);

	dev->base_addr	= ecard_address(ec, ECARD_IOC, ECARD_FAST);
	dev->irq	= ec->irq;

	/*
	 * these will not fail - the nature of the bus ensures this
	 */
	request_region(dev->base_addr, 16, dev->name);
	request_region(dev->base_addr + 0x800, 4096, dev->name);

	priv = (struct ether1_priv *)dev->priv;
	if ((priv->bus_type = ether1_reset(dev)) == 0)
		goto release;

	printk(KERN_INFO "%s: ether1 in slot %d, ",
		dev->name, ec->slot_no);
    
	for (i = 0; i < 6; i++) {
		dev->dev_addr[i] = inb(IDPROM_ADDRESS + i);
		printk ("%2.2x%c", dev->dev_addr[i], i == 5 ? '\n' : ':');
	}

	if (ether1_init_2(dev))
		goto release;

	dev->open		= ether1_open;
	dev->stop		= ether1_close;
	dev->hard_start_xmit    = ether1_sendpacket;
	dev->get_stats		= ether1_getstats;
	dev->set_multicast_list = ether1_setmulticastlist;
	dev->set_mac_address	= ether1_set_mac_address;
	dev->tx_timeout		= ether1_timeout;
	dev->watchdog_timeo	= 5 * HZ / 100;
	return 0;

release:
	release_region(dev->base_addr, 16);
	release_region(dev->base_addr + 0x800, 4096);
	unregister_netdev(dev);
	kfree(dev);
out:
	ecard_release(ec);
	return dev;
}

static struct expansion_card	*e_card[MAX_ECARDS];
static struct net_device	*e_dev[MAX_ECARDS];

static int __init ether1_init(void)
{
	int i, ret = -ENODEV;

	ecard_startfind();

	for (i = 0; i < MAX_ECARDS; i++) {
		struct expansion_card *ec;
		struct net_device *dev;

		ec = ecard_find(0, ether1_cids);
		if (!ec)
			break;

		dev = ether1_init_one(ec);
		if (!dev)
			break;

		e_card[i] = ec;
		e_dev[i]  = dev;
		ret = 0;
	}

	return ret;
}

static void __exit ether1_exit(void)
{
	int i;

	for (i = 0; i < MAX_ECARDS; i++) {
		if (e_dev[i]) {
			unregister_netdev(e_dev[i]);
			release_region(e_dev[i]->base_addr, 16);
			release_region(e_dev[i]->base_addr + 0x800, 4096);
			kfree(e_dev[i]);
			e_dev[i] = NULL;
		}
		if (e_card[i]) {
			ecard_release(e_card[i]);
			e_card[i] = NULL;
		}
	}
}

module_init(ether1_init);
module_exit(ether1_exit);

MODULE_LICENSE("GPL");
