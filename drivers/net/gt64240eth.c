/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2001 Patton Electronics Company
 * Copyright (C) 2002 Momentum Computer
 *
 * Copyright 2000 MontaVista Software Inc.
 * Author: MontaVista Software, Inc.
 *         	stevel@mvista.com or support@mvista.com
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * Ethernet driver for the MIPS GT96100 Advanced Communication Controller.
 * 
 * Modified for the Gallileo/Marvell GT-64240 Communication Controller.
 *
 * Support for Rx NAPI, Rx checksum offload, IOCTL and ETHTOOL added
 * Manish Lachwani (lachwani@pmc-sierra.com) - 09/16/2003
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/errno.h>
#include <linux/in.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/mii.h>

#include <asm/irq.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#define DESC_DATA_BE 1

#include "gt64240eth.h"

// enable this port (set hash size to 1/2K)
//- #define PORT_CONFIG pcrHS
#define PORT_CONFIG (pcrHS | pcrHD)
//- #define PORT_CONFIG pcrHS |pcrPM |pcrPBF|pcrHDM
//- GT64240ETH_WRITE(gp, GT64240_ETH_PORT_CONFIG, pcrEN | pcrHS);
//- GT64240ETH_WRITE(gp, GT64240_ETH_PORT_CONFIG, pcrEN | pcrHS | pcrPM);
//- GT64240ETH_WRITE(gp, GT64240_ETH_PORT_CONFIG, pcrEN | pcrHS | pcrPM | 1<<pcrLPBKBit);

// clear all the MIB ctr regs
#define EXT_CONFIG_CLEAR (pcxrFCTL | pcxrFCTLen | pcxrFLP | pcxrDPLXen | pcxrPRIOrxOverride | pcxrRMIIen)

/*
 * _debug level:
 * <= 2	none.
 *  > 2	some warnings such as queue full, .....
 *  > 3	lots of change-of-state messages.
 *  > 4	EXTENSIVE data/descriptor dumps.
 */
#ifdef GT64240_DEBUG
static int gt64240_debug = GT64240_DEBUG;
#else
static int gt64240_debug = 0;
#endif

/********************************************************/

// prototypes
static void *dmaalloc(size_t size, dma_addr_t * dma_handle);
static void dmafree(size_t size, void *vaddr);
static void gt64240_delay(int msec);
static int gt64240_add_hash_entry(struct net_device *dev,
				  unsigned char *addr);
static void read_mib_counters(struct gt64240_private *gp);
static int read_MII(struct net_device *dev, u32 reg);
static int write_MII(struct net_device *dev, u32 reg, u16 data);
#if 1
static void dump_tx_ring(struct net_device *dev);
static void dump_rx_ring(struct net_device *dev);
#endif
static void dump_MII(struct net_device *dev);
static void dump_tx_desc(struct net_device *dev, int i);
static void dump_rx_desc(struct net_device *dev, int i);
static void dump_skb(struct net_device *dev, struct sk_buff *skb);
static void dump_hw_addr(unsigned char *addr_str);
static void update_stats(struct gt64240_private *gp);
static void abort(struct net_device *dev, u32 abort_bits);
static void hard_stop(struct net_device *dev);
static void enable_ether_irq(struct net_device *dev);
static void disable_ether_irq(struct net_device *dev);
static int __init gt64240_probe1(uint32_t ioaddr, int irq, int port_num);
static void reset_tx(struct net_device *dev);
static void reset_rx(struct net_device *dev);
static int gt64240_init(struct net_device *dev);
static int gt64240_open(struct net_device *dev);
static int gt64240_close(struct net_device *dev);
static int gt64240_tx(struct sk_buff *skb, struct net_device *dev);
#ifdef GT64240_NAPI
static int gt64240_poll(struct net_device *dev, int *budget);
static int gt64240_rx(struct net_device *dev, u32 status, int budget);
#else
static int gt64240_rx(struct net_device *dev, u32 status);
#endif
static void gt64240_interrupt(int irq, void *dev_id, struct pt_regs *regs);
static void gt64240_tx_timeout(struct net_device *dev);
static void gt64240_set_rx_mode(struct net_device *dev);
static struct net_device_stats *gt64240_get_stats(struct net_device *dev);

extern char *__init prom_getcmdline(void);
extern int prom_get_mac_addrs(unsigned char
			      station_addr[NUM_INTERFACES][6]);

static char version[] __devinitdata =
	"gt64240eth.o: version 0.1, <www.patton.com>\n";

// PHY device addresses
static u32 gt64240_phy_addr[NUM_INTERFACES] __devinitdata = { 0x8, 0x1, 0xa };

// Need real Ethernet addresses -- in parse_mac_addr_options(),
// these will be replaced by prom_get_mac_addrs() and/or prom_getcmdline().
static unsigned char gt64240_station_addr[NUM_INTERFACES][6] = {
	{0x00, 0x01, 0x02, 0x03, 0x04, 0x05},
	{0x01, 0x02, 0x03, 0x04, 0x05, 0x06},
	{0x02, 0x03, 0x04, 0x05, 0x06, 0x07}
};

static int max_interrupt_work = 32;

/*
 * Base address and interupt of the GT64240 ethernet controllers
 */
static struct {
	unsigned int port;
	int irq;
} gt64240_iflist[NUM_INTERFACES] = {
	{
	GT64240_ETH0_BASE, 8}, {
	GT64240_ETH1_BASE, 8}, {
	GT64240_ETH2_BASE, 8}
};

/*
  DMA memory allocation, derived from pci_alloc_consistent.
*/
static void *dmaalloc(size_t size, dma_addr_t * dma_handle)
{
	void *ret;

	ret =
	    (void *) __get_free_pages(GFP_ATOMIC | GFP_DMA,
				      get_order(size));

	if (ret != NULL) {
		dma_cache_inv((unsigned long) ret, size);
		if (dma_handle != NULL)
			*dma_handle = virt_to_phys(ret);

		/* bump virtual address up to non-cached area */
		ret = (void *) KSEG1ADDR(ret);
	}

	return ret;
}

static void dmafree(size_t size, void *vaddr)
{
	vaddr = (void *) KSEG0ADDR(vaddr);
	free_pages((unsigned long) vaddr, get_order(size));
}

static void gt64240_delay(int ms)
{
	if (in_interrupt())
		return;
	else {
		current->state = TASK_INTERRUPTIBLE;
		schedule_timeout(ms * HZ / 1000);
	}
}

unsigned char prom_mac_addr_base[6];

int prom_get_mac_addrs(unsigned char station_addr[NUM_INTERFACES][6])
{
	memcpy(station_addr[0], prom_mac_addr_base, 6);
	memcpy(station_addr[1], prom_mac_addr_base, 6);
	memcpy(station_addr[2], prom_mac_addr_base, 6);

	station_addr[1][5] += 1;
	station_addr[2][5] += 2;

	return 0;
}

void parse_mac_addr_options(void)
{
	prom_get_mac_addrs(gt64240_station_addr);
}

static int read_MII(struct net_device *dev, u32 reg)
{
	struct gt64240_private *gp = (struct gt64240_private *) dev->priv;
	int timedout = 20;
	u32 smir = smirOpCode | (gp->phy_addr << smirPhyAdBit) |
	    (reg << smirRegAdBit);

	// wait for last operation to complete
	while ((GT64240_READ(GT64240_ETH_SMI_REG)) & smirBusy) {
		// snooze for 1 msec and check again
		gt64240_delay(1);

		if (--timedout == 0) {
			printk("%s: read_MII busy timeout!!\n", dev->name);
			return -1;
		}
	}

	GT64240_WRITE(GT64240_ETH_SMI_REG, smir);

	timedout = 20;
	// wait for read to complete
	while (!
	       ((smir =
		 GT64240_READ(GT64240_ETH_SMI_REG)) & smirReadValid)) {
		// snooze for 1 msec and check again
		gt64240_delay(1);

		if (--timedout == 0) {
			printk("%s: read_MII timeout!!\n", dev->name);
			return -1;
		}
	}

	return (int) (smir & smirDataMask);
}

/* Ethtool support */
static int netdev_ethtool_ioctl(struct net_device *dev, void *useraddr)
{
	struct gt64240_private *gp = (struct gt64240_private *) dev->priv;
	u32 ethcmd;

	if (copy_from_user(&ethcmd, useraddr, sizeof(ethcmd)))
		return -EFAULT;

	switch (ethcmd) {

		/* Get driver info */
	case ETHTOOL_GDRVINFO:{
			struct ethtool_drvinfo info = { ETHTOOL_GDRVINFO };
			strncpy(info.driver, "gt64260",
				sizeof(info.driver) - 1);
			strncpy(info.version, version,
				sizeof(info.version) - 1);
			if (copy_to_user(useraddr, &info, sizeof(info)))
				return -EFAULT;
			return 0;
		}
		/* get settings */
	case ETHTOOL_GSET:{
			struct ethtool_cmd ecmd = { ETHTOOL_GSET };
			spin_lock_irq(&gp->lock);
			mii_ethtool_gset(&gp->mii_if, &ecmd);
			spin_unlock_irq(&gp->lock);
			if (copy_to_user(useraddr, &ecmd, sizeof(ecmd)))
				return -EFAULT;
			return 0;
		}
		/* set settings */
	case ETHTOOL_SSET:{
			int r;
			struct ethtool_cmd ecmd;
			if (copy_from_user(&ecmd, useraddr, sizeof(ecmd)))
				return -EFAULT;
			spin_lock_irq(&gp->lock);
			r = mii_ethtool_sset(&gp->mii_if, &ecmd);
			spin_unlock_irq(&gp->lock);
			return r;
		}
		/* restart autonegotiation */
	case ETHTOOL_NWAY_RST:{
			return mii_nway_restart(&gp->mii_if);
		}
		/* get link status */
	case ETHTOOL_GLINK:{
			struct ethtool_value edata = { ETHTOOL_GLINK };
			edata.data = mii_link_ok(&gp->mii_if);
			if (copy_to_user(useraddr, &edata, sizeof(edata)))
				return -EFAULT;
			return 0;
		}
		/* get message-level */
	case ETHTOOL_GMSGLVL:{
			struct ethtool_value edata = { ETHTOOL_GMSGLVL };
			edata.data = 0;	/* XXX */
			if (copy_to_user(useraddr, &edata, sizeof(edata)))
				return -EFAULT;
			return 0;
		}
		/* set message-level */
	case ETHTOOL_SMSGLVL:{
			struct ethtool_value edata;
			if (copy_from_user
			    (&edata, useraddr, sizeof(edata)))
				return -EFAULT;
			/* debug = edata.data; *//* XXX */
			return 0;
		}
	}
	return -EOPNOTSUPP;
}

static int gt64240_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct mii_ioctl_data *data =
	    (struct mii_ioctl_data *) &rq->ifr_data;
	int phy = dev->base_addr & 0x1f;
	int retval;

	switch (cmd) {
	case SIOCETHTOOL:
		retval = netdev_ethtool_ioctl(dev, (void *) rq->ifr_data);
		break;

	case SIOCGMIIPHY:	/* Get address of MII PHY in use. */
	case SIOCDEVPRIVATE:	/* for binary compat, remove in 2.5 */
		data->phy_id = phy;
		/* Fall through */

	case SIOCGMIIREG:	/* Read MII PHY register. */
	case SIOCDEVPRIVATE + 1:	/* for binary compat, remove in 2.5 */
		data->val_out = read_MII(dev, data->reg_num & 0x1f);
		retval = 0;
		break;

	case SIOCSMIIREG:	/* Write MII PHY register. */
	case SIOCDEVPRIVATE + 2:	/* for binary compat, remove in 2.5 */
		if (!capable(CAP_NET_ADMIN)) {
			retval = -EPERM;
		} else {
			write_MII(dev, data->reg_num & 0x1f, data->val_in);
			retval = 0;
		}
		break;

	default:
		retval = -EOPNOTSUPP;
		break;
	}
	return retval;
}

static void dump_tx_desc(struct net_device *dev, int i)
{
	struct gt64240_private *gp = (struct gt64240_private *) dev->priv;
	gt64240_td_t *td = &gp->tx_ring[i];

	printk
	    ("%s:tx[%d]: self=%08x cmd=%08x, cnt=%4d. bufp=%08x, next=%08x\n",
	     dev->name, i, td, td->cmdstat, td->byte_cnt, td->buff_ptr,
	     td->next);
}

static void dump_rx_desc(struct net_device *dev, int i)
{
	struct gt64240_private *gp = (struct gt64240_private *) dev->priv;
	gt64240_rd_t *rd = &gp->rx_ring[i];

	printk
	    ("%s:rx_dsc[%d]: self=%08x cst=%08x,size=%4d. cnt=%4d. bufp=%08x, next=%08x\n",
	     dev->name, i, rd, rd->cmdstat, rd->buff_sz, rd->byte_cnt,
	     rd->buff_ptr, rd->next);
}

// These routines work, just disabled to avoid compile warnings
static int write_MII(struct net_device *dev, u32 reg, u16 data)
{
	struct gt64240_private *gp = (struct gt64240_private *) dev->priv;
	int timedout = 20;
	u32 smir =
	    (gp->phy_addr << smirPhyAdBit) | (reg << smirRegAdBit) | data;

	// wait for last operation to complete
	while (GT64240_READ(GT64240_ETH_SMI_REG) & smirBusy) {
		// snooze for 1 msec and check again
		gt64240_delay(1);

		if (--timedout == 0) {
			printk("%s: write_MII busy timeout!!\n",
			       dev->name);
			return -1;
		}
	}

	GT64240_WRITE(GT64240_ETH_SMI_REG, smir);
	return 0;
}

static void dump_tx_ring(struct net_device *dev)
{
	struct gt64240_private *gp = (struct gt64240_private *) dev->priv;
	int i;

	printk("%s: dump_tx_ring: txno/txni/cnt=%d/%d/%d\n",
	       dev->name, gp->tx_next_out, gp->tx_next_in, gp->tx_count);

	for (i = 0; i < TX_RING_SIZE; i++)
		dump_tx_desc(dev, i);
}

static void dump_rx_ring(struct net_device *dev)
{
	struct gt64240_private *gp = (struct gt64240_private *) dev->priv;
	int i;

	printk("%s: dump_rx_ring: rxno=%d\n", dev->name, gp->rx_next_out);

	for (i = 0; i < RX_RING_SIZE; i++)
		dump_rx_desc(dev, i);
}

static void dump_MII(struct net_device *dev)
{
	int i, val;

	for (i = 0; i < 7; i++) {
		if ((val = read_MII(dev, i)) >= 0)
			printk("%s: MII Reg %d=%x\n", dev->name, i, val);
	}
	for (i = 16; i < 21; i++) {
		if ((val = read_MII(dev, i)) >= 0)
			printk("%s: MII Reg %d=%x\n", dev->name, i, val);
	}
}


static void dump_hw_addr(unsigned char *addr_str)
{
	int i;
	for (i = 0; i < 6; i++) {
		printk("%2.2x", addr_str[i]);
		printk(i < 5 ? ":" : "\n");
	}
}


static void dump_skb(struct net_device *dev, struct sk_buff *skb)
{
	int i;
	unsigned char *skbdata;

	printk("%s: dump_skb: skb=%p, skb->data=%p, skb->len=%d.",
	       dev->name, skb, skb->data, skb->len);

	skbdata = (unsigned char *) KSEG1ADDR(skb->data);

	for (i = 0; i < skb->len; i++) {
		if (!(i % 16))
			printk("\r\n   %3.3x: %2.2x,", i, skbdata[i]);
		else
			printk("%2.2x,", skbdata[i]);
	}
	printk("\r\n");
}


static void dump_data(struct net_device *dev, char *ptr, int len)
{
	int i;
	unsigned char *data;

	printk("%s: dump_data: ptr=%p, len=%d.", dev->name, ptr, len);

	data = (unsigned char *) KSEG1ADDR(ptr);

	for (i = 0; i < len; i++) {
		if (!(i % 16))
			printk("\n   %3.3x: %2.2x,", i, data[i]);
		else
			printk("%2.2x,", data[i]);
	}
	printk("\n");
}


/*--------------------------------------------------------------*/
/*	A D D    H A S H    E N T R Y				*/
/*--------------------------------------------------------------*/
static int gt64240_add_hash_entry(struct net_device *dev,
				  unsigned char *addr)
{
	struct gt64240_private *gp;
	int i;
	u32 value1, value0, *entry;
	u16 hashResult;
	unsigned char hash_ea[6];
	static int flag = 0;
	static unsigned char swapped[256];

	if (flag == 0) {	/* Create table to swap bits in a byte  */
		flag = 1;
		for (i = 0; i < 256; i++) {
			swapped[i] = (i & 0x01) << 7;
			swapped[i] |= (i & 0x02) << 5;
			swapped[i] |= (i & 0x04) << 3;
			swapped[i] |= (i & 0x08) << 1;
			swapped[i] |= (i & 0x10) >> 1;
			swapped[i] |= (i & 0x20) >> 3;
			swapped[i] |= (i & 0x40) >> 5;
			swapped[i] |= (i & 0x80) >> 7;
		}
	}

	for (i = 0; i < 6; i++) {	/* swap bits from mac to create hash mac */
		hash_ea[i] = swapped[addr[i]];
	}

	gp = (struct gt64240_private *) dev->priv;

	/* create hash entry address    */
	hashResult = (((hash_ea[5] >> 2) & 0x3F) << 9) & 0x7E00;
	hashResult |= ((hash_ea[4] & 0x7F) << 2) | (hash_ea[5] & 0x03);
	hashResult ^=
	    ((hash_ea[3] & 0xFF) << 1) | ((hash_ea[4] >> 7) & 0x01);
	hashResult ^= ((hash_ea[1] & 0x01) << 8) | (hash_ea[2] & 0xFF);

	value0 = hteValid | hteRD;	/* Create hash table entry value */
	value0 |= (u32) addr[0] << 3;
	value0 |= (u32) addr[1] << 11;
	value0 |= (u32) addr[2] << 19;
	value0 |= ((u32) addr[3] & 0x1f) << 27;

	value1 = ((u32) addr[3] >> 5) & 0x07;
	value1 |= (u32) addr[4] << 3;
	value1 |= (u32) addr[5] << 11;

	/* Inset entry value into hash table */
	for (i = 0; i < HASH_HOP_NUMBER; i++) {
		entry = (u32 *) ((u32) gp->hash_table +
				 (((u32) hashResult & 0x07ff) << 3));
		if ((*entry & hteValid) && !(*entry & hteSkip)) {
			hashResult += 2;	/* oops, occupied, go to next entry */
		} else {
#ifdef __LITTLE_ENDIAN
			entry[1] = value1;
			entry[0] = value0;
#else
			entry[0] = value1;
			entry[1] = value0;
#endif
			break;
		}
	}
	if (i >= HASH_HOP_NUMBER) {
		printk("%s: gt64240_add_hash_entry expired!\n", dev->name);
		return (-1);
	}
	return (0);
}


static void read_mib_counters(struct gt64240_private *gp)
{
	u32 *mib_regs = (u32 *) & gp->mib;
	int i;

	for (i = 0; i < sizeof(mib_counters_t) / sizeof(u32); i++)
		mib_regs[i] =
		    GT64240ETH_READ(gp,
				    GT64240_ETH_MIB_COUNT_BASE +
				    i * sizeof(u32));
}


static void update_stats(struct gt64240_private *gp)
{
	mib_counters_t *mib = &gp->mib;
	struct net_device_stats *stats = &gp->stats;

	read_mib_counters(gp);

	stats->rx_packets = mib->totalFramesReceived;
	stats->tx_packets = mib->framesSent;
	stats->rx_bytes = mib->totalByteReceived;
	stats->tx_bytes = mib->byteSent;
	stats->rx_errors = mib->totalFramesReceived - mib->framesReceived;
	//the tx error counters are incremented by the ISR
	//rx_dropped incremented by gt64240_rx
	//tx_dropped incremented by gt64240_tx
	stats->multicast = mib->multicastFramesReceived;
	// collisions incremented by gt64240_tx_complete
	stats->rx_length_errors = mib->oversizeFrames + mib->fragments;
	// The RxError condition means the Rx DMA encountered a
	// CPU owned descriptor, which, if things are working as
	// they should, means the Rx ring has overflowed.
	stats->rx_over_errors = mib->macRxError;
	stats->rx_crc_errors = mib->cRCError;
}

static void abort(struct net_device *dev, u32 abort_bits)
{
	struct gt64240_private *gp = (struct gt64240_private *) dev->priv;
	int timedout = 100;	// wait up to 100 msec for hard stop to complete

	if (gt64240_debug > 3)
		printk("%s: abort\n", dev->name);

	// Return if neither Rx or Tx abort bits are set
	if (!(abort_bits & (sdcmrAR | sdcmrAT)))
		return;

	// make sure only the Rx/Tx abort bits are set
	abort_bits &= (sdcmrAR | sdcmrAT);

	spin_lock(&gp->lock);

	// abort any Rx/Tx DMA immediately
	GT64240ETH_WRITE(gp, GT64240_ETH_SDMA_COMM, abort_bits);

	if (gt64240_debug > 3)
		printk("%s: abort: SDMA cmd  = %x/%x\n",
		       dev->name, abort_bits, GT64240ETH_READ(gp,
							      GT64240_ETH_SDMA_COMM));

	// wait for abort to complete
	while ((GT64240ETH_READ(gp, GT64240_ETH_SDMA_COMM)) & abort_bits) {
		// snooze for 20 msec and check again
		gt64240_delay(1);

		if (--timedout == 0) {
			printk("%s: abort timeout!!\n", dev->name);
			break;
		}
	}

	spin_unlock(&gp->lock);
}


static void hard_stop(struct net_device *dev)
{
	struct gt64240_private *gp = (struct gt64240_private *) dev->priv;

	if (gt64240_debug > 3)
		printk("%s: hard stop\n", dev->name);

	disable_ether_irq(dev);

	abort(dev, sdcmrAR | sdcmrAT);

	// disable port
	GT64240ETH_WRITE(gp, GT64240_ETH_PORT_CONFIG, 0);
	if (gt64240_debug > 3)
		printk("%s: gt64240_hard_stop: Port Config=%x\n",
		       dev->name, GT64240ETH_READ(gp,
						  GT64240_ETH_PORT_CONFIG));

}


static void enable_ether_irq(struct net_device *dev)
{
	struct gt64240_private *gp = (struct gt64240_private *) dev->priv;
	u32 intMask;

	intMask =
	    icrTxBufferLow | icrTxEndLow | icrTxErrorLow |
	    icrTxBufferHigh | icrTxEndHigh | icrTxErrorHigh | icrTxUdr |
	    icrRxBuffer | icrRxOVR | icrRxError | icrMIIPhySTC |
	    icrEtherIntSum;


//- GT64240ETH_WRITE(gp, GT64240_ETH_INT_CAUSE, 0); /* CLEAR existing ints */
	// unmask device interrupts:
	GT64240ETH_WRITE(gp, GT64240_ETH_INT_MASK, intMask);

	// now route ethernet interrupts to GT PCI1 (eth0 and eth1 will be
	// sharing it).
	GT_READ(PCI_1INTERRUPT_CAUSE_MASK_REGISTER_HIGH, &intMask);
	intMask |= 1 << gp->port_num;
	GT_WRITE(PCI_1INTERRUPT_CAUSE_MASK_REGISTER_HIGH, intMask);
}

static void disable_ether_irq(struct net_device *dev)
{
	struct gt64240_private *gp = (struct gt64240_private *) dev->priv;
	u32 intMask;

	GT_READ(PCI_1INTERRUPT_CAUSE_MASK_REGISTER_HIGH, &intMask);
	intMask &= ~(1 << gp->port_num);
	GT_WRITE(PCI_1INTERRUPT_CAUSE_MASK_REGISTER_HIGH, intMask);

	// mask all device interrupts: 
	GT64240ETH_WRITE(gp, GT64240_ETH_INT_MASK, 0);
}

#ifdef GT64240_NAPI
static inline void __netif_rx_complete(struct net_device *dev)
{
	if (!test_bit(__LINK_STATE_RX_SCHED, &dev->state))
		BUG();
	list_del(&dev->poll_list);
	clear_bit(__LINK_STATE_RX_SCHED, &dev->state);
}
#endif


/*
 * Probe for a GT64240 ethernet controller.
 */
static int __init gt64240_probe(void)
{
	unsigned int base_addr = 0;
	int i;
	int found = 0;

	if (gt64240_debug > 2)
		printk("gt64240_probe at 0x%08x\n", base_addr);

	parse_mac_addr_options();

	for (i = 0; i < NUM_INTERFACES; i++) {
		int base_addr = gt64240_iflist[i].port;

		if (check_region(base_addr, GT64240_ETH_IO_SIZE)) {
			printk("gt64240_probe: ioaddr 0x%lx taken?\n",
			       base_addr);
			continue;
		}

		if (gt64240_probe1(base_addr, gt64240_iflist[i].irq, i) ==
		    0) {
			/* Does not seem to be the "traditional" way folks do this, */
			/* but I want to init both eth ports if at all possible!    */
			/* So, until I find out the "correct" way to do this:       */
			if (++found == NUM_INTERFACES)	/* That's all of them! */
				return 0;
		}
	}
	if (found)
		return 0;	/* as long as we found at least one! */
	return -ENODEV;
}

module_init(gt64240_probe);

static int __init gt64240_probe1(uint32_t ioaddr, int irq, int port_num)
{
	struct net_device *dev = NULL;
	static unsigned version_printed = 0;
	struct gt64240_private *gp = NULL;
	int retval;
	u16 vendor_id, device_id;
	u32 cpuConfig;
	unsigned char chip_rev;

	// probe for GT64240 by reading PCI0 vendor/device ID register
	pcibios_read_config_word(0, 0, PCI_VENDOR_ID, &vendor_id);
	pcibios_read_config_word(0, 0, PCI_DEVICE_ID, &device_id);

	dev = init_etherdev(NULL, sizeof(struct gt64240_private));

	if (gt64240_debug > 2)
		printk
		    ("%s: gt64240_probe1 vendId=0x%08x, devId=0x%08x, addr=0x%08lx, irq=%d.,port=%d.\n",
		     dev->name, vendor_id, device_id, ioaddr, irq,
		     port_num);

	if (irq < 0) {
		printk
		    ("gt64240_probe1: irq unknown - probing not supported\n");
		return -ENODEV;
	}
#if 1				/* KLUDGE Alert: no check on return value: */
	if (!request_region(ioaddr, GT64240_ETH_IO_SIZE, "gt64240eth"))
		printk("*** request_region() failed!\n");
#endif

	cpuConfig = GT64240_READ(CPU_CONFIGURATION);
	printk("gt64240_probe1: cpu in %s-endian mode\n",
	       (cpuConfig & (1 << 12)) ? "little" : "big");

	printk
	    ("%s: GT64240 found at ioaddr 0x%lx, irq %d., PCI devID=%x\n",
	     dev->name, ioaddr, irq, device_id);

	/* Allocate a new 'dev' if needed. */
	if (dev == NULL)
		dev = init_etherdev(0, sizeof(struct gt64240_private));

	if (gt64240_debug && version_printed++ == 0)
		printk("%s: %s", dev->name, version);

	/* private struct aligned and zeroed by init_etherdev */
	/* Fill in the 'dev' fields. */
	dev->base_addr = ioaddr;
	dev->irq = irq;
	memcpy(dev->dev_addr, gt64240_station_addr[port_num],
	       sizeof(dev->dev_addr));

	printk("%s: HW Address ", dev->name);
	dump_hw_addr(dev->dev_addr);

	/* Initialize our private structure. */
	if (dev->priv == NULL) {

		gp = (struct gt64240_private *) kmalloc(sizeof(*gp),
							GFP_KERNEL);
		if (gp == NULL) {
			retval = -ENOMEM;
			goto free_region;
		}

		dev->priv = gp;
	}

	gp = dev->priv;

	memset(gp, 0, sizeof(*gp));	// clear it

	gp->port_num = port_num;
	gp->io_size = GT64240_ETH_IO_SIZE;
	gp->port_offset = port_num * GT64240_ETH_IO_SIZE;
	gp->phy_addr = gt64240_phy_addr[port_num];

	pcibios_read_config_byte(0, 0, PCI_REVISION_ID, &chip_rev);
	gp->chip_rev = chip_rev;
	printk("%s: GT64240 chip revision=%d\n", dev->name, gp->chip_rev);

	printk("%s: GT64240 ethernet port %d\n", dev->name, gp->port_num);

#ifdef GT64240_NAPI
	printk("Rx NAPI supported \n");
#endif

/* MII Initialization */
	gp->mii_if.dev = dev;
	gp->mii_if.phy_id = dev->base_addr;
	gp->mii_if.mdio_read = read_MII;
	gp->mii_if.mdio_write = write_MII;
	gp->mii_if.advertising = read_MII(dev, MII_ADVERTISE);

	// Allocate Rx and Tx descriptor rings
	if (gp->rx_ring == NULL) {
		// All descriptors in ring must be 16-byte aligned
		gp->rx_ring = dmaalloc(sizeof(gt64240_rd_t) * RX_RING_SIZE
				       +
				       sizeof(gt64240_td_t) * TX_RING_SIZE,
				       &gp->rx_ring_dma);
		if (gp->rx_ring == NULL) {
			retval = -ENOMEM;
			goto free_region;
		}

		gp->tx_ring =
		    (gt64240_td_t *) (gp->rx_ring + RX_RING_SIZE);
		gp->tx_ring_dma =
		    gp->rx_ring_dma + sizeof(gt64240_rd_t) * RX_RING_SIZE;
	}
	// Allocate the Rx Data Buffers
	if (gp->rx_buff == NULL) {
		gp->rx_buff =
		    dmaalloc(PKT_BUF_SZ * RX_RING_SIZE, &gp->rx_buff_dma);
		if (gp->rx_buff == NULL) {
			dmafree(sizeof(gt64240_rd_t) * RX_RING_SIZE
				+ sizeof(gt64240_td_t) * TX_RING_SIZE,
				gp->rx_ring);
			retval = -ENOMEM;
			goto free_region;
		}
	}

	if (gt64240_debug > 3)
		printk("%s: gt64240_probe1, rx_ring=%p, tx_ring=%p\n",
		       dev->name, gp->rx_ring, gp->tx_ring);

	// Allocate Rx Hash Table
	if (gp->hash_table == NULL) {
		gp->hash_table = (char *) dmaalloc(RX_HASH_TABLE_SIZE,
						   &gp->hash_table_dma);
		if (gp->hash_table == NULL) {
			dmafree(sizeof(gt64240_rd_t) * RX_RING_SIZE
				+ sizeof(gt64240_td_t) * TX_RING_SIZE,
				gp->rx_ring);
			dmafree(PKT_BUF_SZ * RX_RING_SIZE, gp->rx_buff);
			retval = -ENOMEM;
			goto free_region;
		}
	}

	if (gt64240_debug > 3)
		printk("%s: gt64240_probe1, hash=%p\n",
		       dev->name, gp->hash_table);

	spin_lock_init(&gp->lock);

	dev->open = gt64240_open;
	dev->hard_start_xmit = gt64240_tx;
	dev->stop = gt64240_close;
	dev->get_stats = gt64240_get_stats;
	dev->do_ioctl = gt64240_ioctl;
	dev->set_multicast_list = gt64240_set_rx_mode;
	dev->tx_timeout = gt64240_tx_timeout;
	dev->watchdog_timeo = GT64240ETH_TX_TIMEOUT;

#ifdef GT64240_NAPI
	dev->poll = gt64240_poll;
	dev->weight = 64;
#endif

	/* Fill in the fields of the device structure with ethernet values. */
	ether_setup(dev);
	return 0;

      free_region:
	release_region(ioaddr, gp->io_size);
	unregister_netdev(dev);
	if (dev->priv != NULL)
		kfree(dev->priv);
	kfree(dev);
	printk("%s: gt64240_probe1 failed.  Returns %d\n",
	       dev->name, retval);
	return retval;
}


static void reset_tx(struct net_device *dev)
{
	struct gt64240_private *gp = (struct gt64240_private *) dev->priv;
	int i;

	abort(dev, sdcmrAT);

	for (i = 0; i < TX_RING_SIZE; i++) {
		if (gp->tx_skbuff[i]) {
			if (in_interrupt())
				dev_kfree_skb_irq(gp->tx_skbuff[i]);
			else
				dev_kfree_skb(gp->tx_skbuff[i]);
			gp->tx_skbuff[i] = NULL;
		}
//-     gp->tx_ring[i].cmdstat = 0; // CPU owns
		gp->tx_ring[i].cmdstat =
		    (u32) (txGenCRC | txEI | txPad | txFirst | txLast);
		gp->tx_ring[i].byte_cnt = 0;
		gp->tx_ring[i].buff_ptr = 0;
		gp->tx_ring[i].next =
		    gp->tx_ring_dma + sizeof(gt64240_td_t) * (i + 1);
		if (gt64240_debug > 4)
			dump_tx_desc(dev, i);
	}
	/* Wrap the ring. */
	gp->tx_ring[i - 1].next = gp->tx_ring_dma;
	if (gt64240_debug > 4)
		dump_tx_desc(dev, i - 1);

	// setup only the lowest priority TxCDP reg
	GT64240ETH_WRITE(gp, GT64240_ETH_CURR_TX_DESC_PTR0,
			 gp->tx_ring_dma);
//- GT64240ETH_WRITE(gp, GT64240_ETH_CURR_TX_DESC_PTR0, 0);     /* ROLLINS */
//- GT64240ETH_WRITE(gp, GT64240_ETH_CURR_TX_DESC_PTR0,virt_to_phys(&gp->tx_ring[0]));  /* ROLLINS */

	GT64240ETH_WRITE(gp, GT64240_ETH_CURR_TX_DESC_PTR1, 0);

	// init Tx indeces and pkt counter
	gp->tx_next_in = gp->tx_next_out = 0;
	gp->tx_count = 0;
}

static void reset_rx(struct net_device *dev)
{
	struct gt64240_private *gp = (struct gt64240_private *) dev->priv;
	int i;

	abort(dev, sdcmrAR);

	for (i = 0; i < RX_RING_SIZE; i++) {
		gp->rx_ring[i].next =
		    gp->rx_ring_dma + sizeof(gt64240_rd_t) * (i + 1);
		gp->rx_ring[i].buff_ptr = gp->rx_buff_dma + i * PKT_BUF_SZ;
		gp->rx_ring[i].buff_sz = PKT_BUF_SZ;
		gp->rx_ring[i].byte_cnt = 0;	/* just for debug printk's */
		// Give ownership to device, set first and last, enable interrupt
		gp->rx_ring[i].cmdstat =
		    (uint32_t) (rxFirst | rxLast | rxOwn | rxEI);
		if (gt64240_debug > 4)
			dump_rx_desc(dev, i);
	}
	/* Wrap the ring. */
	gp->rx_ring[i - 1].next = gp->rx_ring_dma;
	if (gt64240_debug > 4)
		dump_rx_desc(dev, i - 1);

	// Setup only the lowest priority RxFDP and RxCDP regs
	for (i = 0; i < 4; i++) {
		if (i == 0) {
			GT64240ETH_WRITE(gp, GT64240_ETH_1ST_RX_DESC_PTR0,
					 gp->rx_ring_dma);
			GT64240ETH_WRITE(gp, GT64240_ETH_CURR_RX_DESC_PTR0,
					 gp->rx_ring_dma);
		} else {
			GT64240ETH_WRITE(gp,
					 GT64240_ETH_1ST_RX_DESC_PTR0 +
					 i * 4, 0);
			GT64240ETH_WRITE(gp,
					 GT64240_ETH_CURR_RX_DESC_PTR0 +
					 i * 4, 0);
		}
	}

	// init Rx NextOut index
	gp->rx_next_out = 0;
}


static int gt64240_init(struct net_device *dev)
{
	struct gt64240_private *gp = (struct gt64240_private *) dev->priv;
	u32 ciu;

	if (gt64240_debug > 3) {
		printk("%s: gt64240_init: dev=%p\n", dev->name, dev);
		printk("%s: gt64240_init: scs0_lo=%04x, scs0_hi=%04x\n",
		       dev->name, GT64240_READ(0x008),
		       GT64240_READ(0x010));
		printk("%s: gt64240_init: scs1_lo=%04x, scs1_hi=%04x\n",
		       dev->name, GT64240_READ(0x208),
		       GT64240_READ(0x210));
		printk("%s: gt64240_init: scs2_lo=%04x, scs2_hi=%04x\n",
		       dev->name, GT64240_READ(0x018),
		       GT64240_READ(0x020));
		printk("%s: gt64240_init: scs3_lo=%04x, scs3_hi=%04x\n",
		       dev->name, GT64240_READ(0x218),
		       GT64240_READ(0x220));
	}
	// Stop and disable Port
	hard_stop(dev);

	GT64240_WRITE(COMM_UNIT_INTERRUPT_MASK, 0x07070777);	/*+prk21aug01 */
	if (gt64240_debug > 2)
		printk
		    ("%s: gt64240_init: CIU Cause=%08x, Mask=%08x, EAddr=%08x\n",
		     dev->name, GT64240_READ(COMM_UNIT_INTERRUPT_CAUSE),
		     GT64240_READ(COMM_UNIT_INTERRUPT_MASK),
		     GT64240_READ(COMM_UNIT_ERROR_ADDRESS));

	// Set-up hash table
	memset(gp->hash_table, 0, RX_HASH_TABLE_SIZE);	// clear it
	gp->hash_mode = 0;
	// Add a single entry to hash table - our ethernet address
	gt64240_add_hash_entry(dev, dev->dev_addr);
	// Set-up DMA ptr to hash table
	GT64240ETH_WRITE(gp, GT64240_ETH_HASH_TBL_PTR, gp->hash_table_dma);
	if (gt64240_debug > 3)
		printk("%s: gt64240_init: Hash Tbl Ptr=%x\n", dev->name,
		       GT64240ETH_READ(gp, GT64240_ETH_HASH_TBL_PTR));

	// Setup Tx
	reset_tx(dev);

	if (gt64240_debug > 3)
		printk("%s: gt64240_init: Curr Tx Desc Ptr0=%x\n",
		       dev->name, GT64240ETH_READ(gp,
						  GT64240_ETH_CURR_TX_DESC_PTR0));

	// Setup Rx
	reset_rx(dev);

	if (gt64240_debug > 3)
		printk("%s: gt64240_init: 1st/Curr Rx Desc Ptr0=%x/%x\n",
		       dev->name, GT64240ETH_READ(gp,
						  GT64240_ETH_1ST_RX_DESC_PTR0),
		       GT64240ETH_READ(gp, GT64240_ETH_CURR_RX_DESC_PTR0));

	if (gt64240_debug > 3)
		dump_MII(dev);
	write_MII(dev, 0, 0x8000);	/* force a PHY reset -- self-clearing! */

	if (gt64240_debug > 3)
		printk("%s: gt64240_init: PhyAD=%x\n", dev->name,
		       GT64240_READ(GT64240_ETH_PHY_ADDR_REG));

	// setup DMA
	// We want the Rx/Tx DMA to write/read data to/from memory in
	// Big Endian mode. Also set DMA Burst Size to 8 64Bit words.
#ifdef DESC_DATA_BE
	GT64240ETH_WRITE(gp, GT64240_ETH_SDMA_CONFIG,
			 (0xf << sdcrRCBit) | sdcrRIFB | (3 <<
							  sdcrBSZBit));
#else
	GT64240ETH_WRITE(gp, GT64240_ETH_SDMA_CONFIG, sdcrBLMR | sdcrBLMT |
//-                  (0xf<<sdcrRCBit) | sdcrRIFB | (3<<sdcrBSZBit));
			 (0xf << sdcrRCBit) | sdcrRIFB | (2 <<
							  sdcrBSZBit));
#endif

	if (gt64240_debug > 3)
		printk("%s: gt64240_init: SDMA Config=%x\n", dev->name,
		       GT64240ETH_READ(gp, GT64240_ETH_SDMA_CONFIG));

#if 0
	// start Rx DMA
	GT64240ETH_WRITE(gp, GT64240_ETH_SDMA_COMM, sdcmrERD);
#endif

	if (gt64240_debug > 3)
		printk("%s: gt64240_init: SDMA Cmd =%x\n", dev->name,
		       GT64240ETH_READ(gp, GT64240_ETH_SDMA_COMM));

#if 1
	GT64240ETH_WRITE(gp, GT64240_ETH_PORT_CONFIG, PORT_CONFIG);
#endif

	if (gt64240_debug > 3)
		printk("%s: gt64240_init: Port Config=%x\n", dev->name,
		       GT64240ETH_READ(gp, GT64240_ETH_PORT_CONFIG));

	/*
	 * Disable all Type-of-Service queueing. All Rx packets will be
	 * treated normally and will be sent to the lowest priority
	 * queue.
	 *
	 * Disable flow-control for now. FIX! support flow control?
	 */

#if 1
	// clear all the MIB ctr regs
	GT64240ETH_WRITE(gp, GT64240_ETH_PORT_CONFIG_EXT,
			 EXT_CONFIG_CLEAR);
	read_mib_counters(gp);
	GT64240ETH_WRITE(gp, GT64240_ETH_PORT_CONFIG_EXT,
			 EXT_CONFIG_CLEAR | pcxrMIBclrMode);

#endif
	if (gt64240_debug > 3)
		printk("%s: gt64240_init: Port Config Ext=%x\n", dev->name,
		       GT64240ETH_READ(gp, GT64240_ETH_PORT_CONFIG_EXT));

	if (gt64240_debug > 3)
		printk("%s: gt64240_init: Port Command=%x\n", dev->name,
		       GT64240ETH_READ(gp, GT64240_ETH_PORT_COMMAND));
	GT64240ETH_WRITE(gp, GT64240_ETH_PORT_COMMAND, 0x0);

	netif_start_queue(dev);

	/* enable the port */
	GT64240ETH_WRITE(gp, GT64240_ETH_PORT_CONFIG,
			 (PORT_CONFIG | pcrEN));
	if (gt64240_debug > 3)
		printk("%s: gt64240_init: Port Config=%x\n", dev->name,
		       GT64240ETH_READ(gp, GT64240_ETH_PORT_CONFIG));
#if 1
	// start Rx DMA
	GT64240ETH_WRITE(gp, GT64240_ETH_SDMA_COMM, sdcmrERD);
#endif


	// enable interrupts
	enable_ether_irq(dev);

//---    gp->last_psr |= psrLink;   /* KLUDGE ALERT */

	// we should now be receiving frames
	return 0;
}


static int gt64240_open(struct net_device *dev)
{
	int retval;

	MOD_INC_USE_COUNT;

	if (gt64240_debug > 3)
		printk("%s: gt64240_open: dev=%p\n", dev->name, dev);

	if ((retval = request_irq(dev->irq, &gt64240_interrupt,
				  SA_SHIRQ, dev->name, dev))) {
		printk("%s: unable to get IRQ %d\n", dev->name, dev->irq);
		MOD_DEC_USE_COUNT;
		return retval;
	}
	// Initialize and startup the GT-64240 ethernet port
	if ((retval = gt64240_init(dev))) {
		printk("%s: error in gt64240_open\n", dev->name);
		free_irq(dev->irq, dev);
		MOD_DEC_USE_COUNT;
		return retval;
	}

	if (gt64240_debug > 3)
		printk("%s: gt64240_open: Initialization done.\n",
		       dev->name);

	return 0;
}

static int gt64240_close(struct net_device *dev)
{
	if (gt64240_debug > 3)
		printk("%s: gt64240_close: dev=%p\n", dev->name, dev);

	// stop the device
	if (netif_device_present(dev)) {
		netif_stop_queue(dev);
		hard_stop(dev);
	}

	free_irq(dev->irq, dev);

	MOD_DEC_USE_COUNT;
	return 0;
}

#ifdef GT64240_NAPI
/*
 * Function will release Tx skbs which are now complete
 */
static void gt64240_tx_fill(struct net_device *netdev, u32 status)
{
	struct gt64240_private *gp =
	    (struct gt64240_private *) netdev->priv;
	int nextOut, cdp;
	gt64240_td_t *td;
	u32 cmdstat;

	cdp = (GT64240ETH_READ(gp, GT64240_ETH_CURR_TX_DESC_PTR0)
	       - gp->tx_ring_dma) / sizeof(gt64240_td_t);

	for (nextOut = gp->tx_next_out; nextOut != cdp;
	     nextOut = (nextOut + 1) % TX_RING_SIZE) {
		if (--gp->intr_work_done == 0)
			break;

		td = &gp->tx_ring[nextOut];
		cmdstat = td->cmdstat;

		if (cmdstat & (u32) txOwn)
			break;

		if (gp->tx_full) {
			gp->tx_full = 0;
			if (gp->last_psr & psrLink) {
				netif_wake_queue(netdev);
			}
		}
		// decrement tx ring buffer count
		if (gp->tx_count)
			gp->tx_count--;

		// free the skb
		if (gp->tx_skbuff[nextOut]) {
			dev_kfree_skb_irq(gp->tx_skbuff[nextOut]);
			gp->tx_skbuff[nextOut] = NULL;
		}
	}

	gp->tx_next_out = nextOut;

	if ((status & icrTxEndLow) && gp->tx_count != 0)
		// we must restart the DMA
		GT64240ETH_WRITE(gp, GT64240_ETH_SDMA_COMM,
				 sdcmrERD | sdcmrTXDL);
}

/*
 * Main function for NAPI
 */
static int gt64240_poll(struct net_device *netdev, int *budget)
{
	struct gt64240_private *gp =
	    (struct gt64240_private *) netdev->priv;
	unsigned long flags;
	int done = 1, orig_budget, work_done;
	u32 intMask, status = GT64240ETH_READ(gp, GT64240_ETH_INT_CAUSE);

	spin_lock_irqsave(&gp->lock, flags);
	gt64240_tx_fill(netdev, status);

	if (GT64240ETH_READ(gp, GT64240_ETH_CURR_RX_DESC_PTR0) !=
	    gp->rx_next_out) {
		orig_budget = *budget;
		if (orig_budget > netdev->quota)
			orig_budget = netdev->quota;

		work_done = gt64240_rx(netdev, status, orig_budget);
		*budget -= work_done;
		netdev->quota -= work_done;
		if (work_done >= orig_budget)
			done = 0;
		if (done) {
			__netif_rx_complete(netdev);
			enable_ether_irq(netdev);
		}
	}

	spin_unlock_irqrestore(&gp->lock, flags);
}
#endif

static int gt64240_tx(struct sk_buff *skb, struct net_device *dev)
{
	struct gt64240_private *gp = (struct gt64240_private *) dev->priv;
	unsigned long flags;
	int nextIn;

	spin_lock_irqsave(&gp->lock, flags);

	nextIn = gp->tx_next_in;

	if (gt64240_debug > 3) {
		printk("%s: gt64240_tx: nextIn=%d.\n", dev->name, nextIn);
	}

	if (gp->tx_count >= TX_RING_SIZE) {
		printk("%s: Tx Ring full, pkt dropped.\n", dev->name);
		gp->stats.tx_dropped++;
		spin_unlock_irqrestore(&gp->lock, flags);
		return 1;
	}

	if (!(gp->last_psr & psrLink)) {
		printk("%s: gt64240_tx: Link down, pkt dropped.\n",
		       dev->name);
		gp->stats.tx_dropped++;
		spin_unlock_irqrestore(&gp->lock, flags);
//---   dump_MII(dev);          /* KLUDGE ALERT !!! */
		return 1;
	}

	if (gp->tx_ring[nextIn].cmdstat & txOwn) {
		printk
		    ("%s: gt64240_tx: device owns descriptor, pkt dropped.\n",
		     dev->name);
		gp->stats.tx_dropped++;
		// stop the queue, so Tx timeout can fix it
		netif_stop_queue(dev);
		spin_unlock_irqrestore(&gp->lock, flags);
		return 1;
	}
	// Prepare the Descriptor at tx_next_in
	gp->tx_skbuff[nextIn] = skb;
	gp->tx_ring[nextIn].byte_cnt = skb->len;
	gp->tx_ring[nextIn].buff_ptr = virt_to_phys(skb->data);

	// make sure packet gets written back to memory
	dma_cache_wback_inv((unsigned long) (skb->data), skb->len);
	mb();

	// Give ownership to device, set first and last desc, enable interrupt
	// Setting of ownership bit must be *last*!
	gp->tx_ring[nextIn].cmdstat =
	    txOwn | txGenCRC | txEI | txPad | txFirst | txLast;

	if (gt64240_debug > 5) {
		dump_tx_desc(dev, nextIn);
		dump_skb(dev, skb);
	}
	// increment tx_next_in with wrap
	gp->tx_next_in = (nextIn + 1) % TX_RING_SIZE;

//+prk20aug01:
	if (0) {		/* ROLLINS */
		GT64240ETH_WRITE(gp, GT64240_ETH_CURR_TX_DESC_PTR0,
				 virt_to_phys(&gp->tx_ring[nextIn]));
	}

	if (gt64240_debug > 3) {	/*+prk17aug01 */
		printk
		    ("%s: gt64240_tx: TX_PTR0=0x%08x, EthPortStatus=0x%08x\n",
		     dev->name, GT64240ETH_READ(gp,
						GT64240_ETH_CURR_TX_DESC_PTR0),
		     GT64240ETH_READ(gp, GT64240_ETH_PORT_STATUS));
	}
	// If DMA is stopped, restart
	if (!((GT64240ETH_READ(gp, GT64240_ETH_PORT_STATUS)) & psrTxLow)) {
		GT64240ETH_WRITE(gp, GT64240_ETH_SDMA_COMM,
				 sdcmrERD | sdcmrTXDL);
	}

	if (gt64240_debug > 3) {	/*+prk17aug01 */
		printk
		    ("%s: gt64240_tx: TX_PTR0=0x%08x, EthPortStatus=0x%08x\n",
		     dev->name, GT64240ETH_READ(gp,
						GT64240_ETH_CURR_TX_DESC_PTR0),
		     GT64240ETH_READ(gp, GT64240_ETH_PORT_STATUS));
	}
	// increment count and stop queue if full
	if (++gp->tx_count >= TX_RING_SIZE) {
		gp->tx_full = 1;
		netif_stop_queue(dev);
	}

	dev->trans_start = jiffies;
	spin_unlock_irqrestore(&gp->lock, flags);

	return 0;
}


static int
#ifdef GT64240_NAPI
gt64240_rx(struct net_device *dev, u32 status, int budget)
#else
gt64240_rx(struct net_device *dev, u32 status)
#endif
{
	struct gt64240_private *gp = (struct gt64240_private *) dev->priv;
	struct sk_buff *skb;
	int pkt_len, nextOut, cdp;
	gt64240_rd_t *rd;
	u32 cmdstat;

	if (gt64240_debug > 3)
		printk("%s: gt64240_rx: dev=%p, status=%x\n",
		       dev->name, dev, status);

	cdp = (GT64240ETH_READ(gp, GT64240_ETH_CURR_RX_DESC_PTR0)
	       - gp->rx_ring_dma) / sizeof(gt64240_rd_t);

	// Continue until we reach the current descriptor pointer
	for (nextOut = gp->rx_next_out; nextOut != cdp;
	     nextOut = (nextOut + 1) % RX_RING_SIZE) {

#ifdef GT64240_NAPI
		if (budget <= 0)
			break;

		budget--;
#endif

		if (--gp->intr_work_done == 0)
			break;

		if (gt64240_debug > 4)
			dump_rx_desc(dev, nextOut);

		rd = &gp->rx_ring[nextOut];
		cmdstat = rd->cmdstat;

		if (gt64240_debug > 3)
			printk("%s: isr: Rx desc cmdstat=%x, nextOut=%d\n",
			       dev->name, cmdstat, nextOut);

		if (cmdstat & (u32) rxOwn) {
			if (gt64240_debug > 2)
				printk
				    ("%s: gt64240_rx: device owns descriptor!\n",
				     dev->name);
			// DMA is not finished updating descriptor???
			// Leave and come back later to pick-up where we left off.
			break;
		}
		// must be first and last (ie only) buffer of packet
		if (!(cmdstat & (u32) rxFirst)
		    || !(cmdstat & (u32) rxLast)) {
			printk
			    ("%s: gt64240_rx: desc not first and last!\n",
			     dev->name);
			cmdstat |= (u32) rxOwn;
			rd->cmdstat = cmdstat;
			continue;
		}
		// Drop this received pkt if there were any errors
		if ((cmdstat & (u32) rxErrorSummary)
		    || (status & icrRxError)) {
			// update the detailed rx error counters that are not covered
			// by the MIB counters.
			if (cmdstat & (u32) rxOverrun)
				gp->stats.rx_fifo_errors++;
			cmdstat |= (u32) rxOwn;
			rd->cmdstat = cmdstat;
			continue;
		}

		pkt_len = rd->byte_cnt;

		/* Create new skb. */
//      skb = dev_alloc_skb(pkt_len+2);
		skb = dev_alloc_skb(1538);
		if (skb == NULL) {
			printk("%s: Memory squeeze, dropping packet.\n",
			       dev->name);
			gp->stats.rx_dropped++;
			cmdstat |= (u32) rxOwn;
			rd->cmdstat = cmdstat;
			continue;
		}
		skb->dev = dev;
		skb_reserve(skb, 2);	/* 16 byte IP header align */
		memcpy(skb_put(skb, pkt_len),
		       &gp->rx_buff[nextOut * PKT_BUF_SZ], pkt_len);
		skb->protocol = eth_type_trans(skb, dev);
		if (gt64240_debug > 4)	/* will probably Oops! */
			dump_data(dev, &gp->rx_buff[nextOut * PKT_BUF_SZ],
				  pkt_len);
		if (gt64240_debug > 4)
			dump_skb(dev, skb);

		/* NIC performed some checksum computation */
		skb->ip_summed = CHECKSUM_UNNECESSARY;
#ifdef GT64240_NAPI
		netif_receive_skb(skb);
#else
		netif_rx(skb);	/* pass the packet to upper layers */
#endif

		// now we can release ownership of this desc back to device
		cmdstat |= (u32) rxOwn;
		rd->cmdstat = cmdstat;

		dev->last_rx = jiffies;
	}

	if (gt64240_debug > 3 && nextOut == gp->rx_next_out)
		printk("%s: gt64240_rx: RxCDP did not increment?\n",
		       dev->name);

	gp->rx_next_out = nextOut;
	return 0;
}


static void gt64240_tx_complete(struct net_device *dev, u32 status)
{
	struct gt64240_private *gp = (struct gt64240_private *) dev->priv;
	int nextOut, cdp;
	gt64240_td_t *td;
	u32 cmdstat;

	cdp = (GT64240ETH_READ(gp, GT64240_ETH_CURR_TX_DESC_PTR0)
	       - gp->tx_ring_dma) / sizeof(gt64240_td_t);

	if (gt64240_debug > 3) {	/*+prk17aug01 */
		nextOut = gp->tx_next_out;
		printk
		    ("%s: tx_complete: TX_PTR0=0x%08x, cdp=%d. nextOut=%d.\n",
		     dev->name, GT64240ETH_READ(gp,
						GT64240_ETH_CURR_TX_DESC_PTR0),
		     cdp, nextOut);
		td = &gp->tx_ring[nextOut];
	}

/*** NEED to check and CLEAR these errors every time thru here: ***/
	if (gt64240_debug > 2) {
		if (GT64240_READ(COMM_UNIT_INTERRUPT_CAUSE))
			printk
			    ("%s: gt64240_tx_complete: CIU Cause=%08x, Mask=%08x, EAddr=%08x\n",
			     dev->name,
			     GT64240_READ(COMM_UNIT_INTERRUPT_CAUSE),
			     GT64240_READ(COMM_UNIT_INTERRUPT_MASK),
			     GT64240_READ(COMM_UNIT_ERROR_ADDRESS));
		GT64240_WRITE(COMM_UNIT_INTERRUPT_CAUSE, 0);
	}
	// Continue until we reach the current descriptor pointer
	for (nextOut = gp->tx_next_out; nextOut != cdp;
	     nextOut = (nextOut + 1) % TX_RING_SIZE) {

		if (--gp->intr_work_done == 0)
			break;

		td = &gp->tx_ring[nextOut];
		cmdstat = td->cmdstat;

		if (cmdstat & (u32) txOwn) {
			// DMA is not finished writing descriptor???
			// Leave and come back later to pick-up where we left off.
			break;
		}
		// increment Tx error stats
		if (cmdstat & (u32) txErrorSummary) {
			if (gt64240_debug > 2)
				printk
				    ("%s: tx_complete: Tx error, cmdstat = %x\n",
				     dev->name, cmdstat);
			gp->stats.tx_errors++;
			if (cmdstat & (u32) txReTxLimit)
				gp->stats.tx_aborted_errors++;
			if (cmdstat & (u32) txUnderrun)
				gp->stats.tx_fifo_errors++;
			if (cmdstat & (u32) txLateCollision)
				gp->stats.tx_window_errors++;
		}

		if (cmdstat & (u32) txCollision)
			gp->stats.collisions +=
			    (unsigned long) ((cmdstat & txReTxCntMask) >>
					     txReTxCntBit);

		// Wake the queue if the ring was full
		if (gp->tx_full) {
			gp->tx_full = 0;
			if (gp->last_psr & psrLink) {
				netif_wake_queue(dev);
			}
		}
		// decrement tx ring buffer count
		if (gp->tx_count)
			gp->tx_count--;

		// free the skb
		if (gp->tx_skbuff[nextOut]) {
			if (gt64240_debug > 3)
				printk
				    ("%s: tx_complete: good Tx, skb=%p\n",
				     dev->name, gp->tx_skbuff[nextOut]);
			dev_kfree_skb_irq(gp->tx_skbuff[nextOut]);
			gp->tx_skbuff[nextOut] = NULL;
		} else {
			printk("%s: tx_complete: no skb!\n", dev->name);
		}
	}

	gp->tx_next_out = nextOut;

	if ((status & icrTxEndLow) && gp->tx_count != 0) {
		// we must restart the DMA
		GT64240ETH_WRITE(gp, GT64240_ETH_SDMA_COMM,
				 sdcmrERD | sdcmrTXDL);
	}
}


static void gt64240_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct net_device *dev = (struct net_device *) dev_id;
	struct gt64240_private *gp = (struct gt64240_private *) dev->priv;
	u32 status;

	if (dev == NULL) {
		printk("%s: isr: null dev ptr\n", dev->name);
		return;
	}

	spin_lock(&gp->lock);

	if (gt64240_debug > 3)
		printk("%s: isr: entry\n", dev->name);

	gp->intr_work_done = max_interrupt_work;

	while (gp->intr_work_done > 0) {

		status = GT64240ETH_READ(gp, GT64240_ETH_INT_CAUSE);
#ifdef GT64240_NAPI
		/* dont ack Rx interrupts */
		if (!(status & icrRxBuffer))
			GT64240ETH_WRITE(gp, GT64240_ETH_INT_CAUSE, 0);
#else
		// ACK interrupts
		GT64240ETH_WRITE(gp, GT64240_ETH_INT_CAUSE, 0);
#endif

		if (gt64240_debug > 3)
			printk("%s: isr: work=%d., icr=%x\n", dev->name,
			       gp->intr_work_done, status);

		if ((status & icrEtherIntSum) == 0) {
			if (!(status &
			      (icrTxBufferLow | icrTxBufferHigh |
			       icrRxBuffer))) {
				/* exit from the while() loop */
				break;
			}
		}

		if (status & icrMIIPhySTC) {
			u32 psr =
			    GT64240ETH_READ(gp, GT64240_ETH_PORT_STATUS);
			if (gp->last_psr != psr) {
				printk("%s: port status: 0x%08x\n",
				       dev->name, psr);
				printk
				    ("%s:    %s MBit/s, %s-duplex, flow-control %s, link is %s,\n",
				     dev->name,
				     psr & psrSpeed ? "100" : "10",
				     psr & psrDuplex ? "full" : "half",
				     psr & psrFctl ? "disabled" :
				     "enabled",
				     psr & psrLink ? "up" : "down");
				printk
				    ("%s:    TxLowQ is %s, TxHighQ is %s, Transmitter is %s\n",
				     dev->name,
				     psr & psrTxLow ? "running" :
				     "stopped",
				     psr & psrTxHigh ? "running" :
				     "stopped",
				     psr & psrTxInProg ? "on" : "off");

				if ((psr & psrLink) && !gp->tx_full &&
				    netif_queue_stopped(dev)) {
					printk
					    ("%s: isr: Link up, waking queue.\n",
					     dev->name);
					netif_wake_queue(dev);
				} else if (!(psr & psrLink)
					   && !netif_queue_stopped(dev)) {
					printk
					    ("%s: isr: Link down, stopping queue.\n",
					     dev->name);
					netif_stop_queue(dev);
				}

				gp->last_psr = psr;
			}
		}

		if (status & (icrTxBufferLow | icrTxEndLow))
			gt64240_tx_complete(dev, status);

		if (status & icrRxBuffer) {
#ifdef GT64240_NAPI
			if (netif_rx_schedule_prep(dev)) {
				disable_ether_irq(dev);
				__netif_rx_schedule(dev);
			}
#else
			gt64240_rx(dev, status);
#endif
		}
		// Now check TX errors (RX errors were handled in gt64240_rx)
		if (status & icrTxErrorLow) {
			printk("%s: isr: Tx resource error\n", dev->name);
		}

		if (status & icrTxUdr) {
			printk("%s: isr: Tx underrun error\n", dev->name);
		}
	}

	if (gp->intr_work_done == 0) {
		// ACK any remaining pending interrupts
		GT64240ETH_WRITE(gp, GT64240_ETH_INT_CAUSE, 0);
		if (gt64240_debug > 3)
			printk("%s: isr: hit max work\n", dev->name);
	}

	if (gt64240_debug > 3)
		printk("%s: isr: exit, icr=%x\n",
		       dev->name, GT64240ETH_READ(gp,
						  GT64240_ETH_INT_CAUSE));

	spin_unlock(&gp->lock);
}


static void gt64240_tx_timeout(struct net_device *dev)
{
	struct gt64240_private *gp = (struct gt64240_private *) dev->priv;
	unsigned long flags;

	spin_lock_irqsave(&gp->lock, flags);


	if (!(gp->last_psr & psrLink)) {
		spin_unlock_irqrestore(&gp->lock, flags);
	} else {
		printk("======------> gt64240_tx_timeout: %d jiffies \n",
		       GT64240ETH_TX_TIMEOUT);

		disable_ether_irq(dev);
		spin_unlock_irqrestore(&gp->lock, flags);
		reset_tx(dev);
		enable_ether_irq(dev);

		netif_wake_queue(dev);
	}
}


static void gt64240_set_rx_mode(struct net_device *dev)
{
	struct gt64240_private *gp = (struct gt64240_private *) dev->priv;
	unsigned long flags;
	struct dev_mc_list *mcptr;

	if (gt64240_debug > 3)
		printk("%s: gt64240_set_rx_mode: dev=%p, flags=%x\n",
		       dev->name, dev, dev->flags);

	// stop the Receiver DMA
	abort(dev, sdcmrAR);

	spin_lock_irqsave(&gp->lock, flags);

	if (dev->flags & IFF_PROMISC)
		GT64240ETH_SETBIT(gp, GT64240_ETH_PORT_CONFIG, pcrPM);
	else
		GT64240ETH_CLRBIT(gp, GT64240_ETH_PORT_CONFIG, pcrPM);
/*
	GT64240ETH_WRITE(gp, GT64240_ETH_PORT_CONFIG,
		(PORT_CONFIG | pcrPM | pcrEN));
*/

	memset(gp->hash_table, 0, RX_HASH_TABLE_SIZE);	// clear hash table
	// Add our ethernet address
	gt64240_add_hash_entry(dev, dev->dev_addr);
	if (dev->mc_count) {
		for (mcptr = dev->mc_list; mcptr; mcptr = mcptr->next) {
			if (gt64240_debug > 2) {
				printk("%s: gt64240_set_rx_mode: addr=\n",
				       dev->name);
				dump_hw_addr(mcptr->dmi_addr);
			}
			gt64240_add_hash_entry(dev, mcptr->dmi_addr);
		}
	}

	if (gt64240_debug > 3)
		printk("%s: gt64240_set_rx: Port Config=%x\n", dev->name,
		       GT64240ETH_READ(gp, GT64240_ETH_PORT_CONFIG));

	// restart Rx DMA
	GT64240ETH_WRITE(gp, GT64240_ETH_SDMA_COMM, sdcmrERD);

	spin_unlock_irqrestore(&gp->lock, flags);
}

static struct net_device_stats *gt64240_get_stats(struct net_device *dev)
{
	struct gt64240_private *gp = (struct gt64240_private *) dev->priv;
	unsigned long flags;

	if (gt64240_debug > 3)
		printk("%s: gt64240_get_stats: dev=%p\n", dev->name, dev);

	if (netif_device_present(dev)) {
		spin_lock_irqsave(&gp->lock, flags);
		update_stats(gp);
		spin_unlock_irqrestore(&gp->lock, flags);
	}

	return &gp->stats;
}
