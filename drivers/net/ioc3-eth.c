/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Driver for SGI's IOC3 based Ethernet cards as found in the PCI card.
 *
 * Copyright (C) 1999, 2000, 2001 Ralf Baechle
 * Copyright (C) 1995, 1999, 2000, 2001 by Silicon Graphics, Inc.
 *
 * References:
 *  o IOC3 ASIC specification 4.51, 1996-04-18
 *  o IEEE 802.3 specification, 2000 edition
 *  o DP38840A Specification, National Semiconductor, March 1997
 *
 * To do:
 *
 *  o Handle allocation failures in ioc3_alloc_skb() more gracefully.
 *  o Handle allocation failures in ioc3_init_rings().
 *  o Use prefetching for large packets.  What is a good lower limit for
 *    prefetching?
 *  o We're probably allocating a bit too much memory.
 *  o Use hardware checksums.
 *  o Convert to using a IOC3 meta driver.
 *  o Which PHYs might possibly be attached to the IOC3 in real live,
 *    which workarounds are required for them?  Do we ever have Lucent's?
 *  o For the 2.5 branch kill the mii-tool ioctls.
 */
#include <linux/config.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/crc32.h>

#ifdef CONFIG_SERIAL
#include <linux/serial.h>
#include <asm/serial.h>
#define IOC3_BAUD (22000000 / (3*16))
#define IOC3_COM_FLAGS (ASYNC_BOOT_AUTOCONF | ASYNC_SKIP_TEST)
#endif

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/skbuff.h>
#include <linux/dp83840.h>

#include <asm/byteorder.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/uaccess.h>
#include <asm/sn/types.h>
#include <asm/sn/sn0/addrs.h>
#include <asm/sn/sn0/hubni.h>
#include <asm/sn/sn0/hubio.h>
#include <asm/sn/klconfig.h>
#include <asm/sn/ioc3.h>
#include <asm/sn/sn0/ip27.h>
#include <asm/pci/bridge.h>

/*
 * 64 RX buffers.  This is tunable in the range of 16 <= x < 512.  The
 * value must be a power of two.
 */
#define RX_BUFFS 64

/* Timer state engine. */
enum ioc3_timer_state {
	arbwait  = 0,	/* Waiting for auto negotiation to complete.          */
	lupwait  = 1,	/* Auto-neg complete, awaiting link-up status.        */
	ltrywait = 2,	/* Forcing try of all modes, from fastest to slowest. */
	asleep   = 3,	/* Time inactive.                                     */
};

/* Private per NIC data of the driver.  */
struct ioc3_private {
	struct ioc3 *regs;
	int phy;
	unsigned long *rxr;		/* pointer to receiver ring */
	struct ioc3_etxd *txr;
	struct sk_buff *rx_skbs[512];
	struct sk_buff *tx_skbs[128];
	struct net_device_stats stats;
	int rx_ci;			/* RX consumer index */
	int rx_pi;			/* RX producer index */
	int tx_ci;			/* TX consumer index */
	int tx_pi;			/* TX producer index */
	int txqlen;
	u32 emcr, ehar_h, ehar_l;
	spinlock_t ioc3_lock;
	struct net_device *dev;

	/* Members used by autonegotiation  */
	struct timer_list ioc3_timer;
	enum ioc3_timer_state timer_state; /* State of auto-neg timer.	   */
	unsigned int timer_ticks;	/* Number of clicks at each state  */
	unsigned short sw_bmcr;		/* sw copy of MII config register  */
	unsigned short sw_bmsr;		/* sw copy of MII status register  */
	unsigned short sw_physid1;	/* sw copy of PHYSID1		   */
	unsigned short sw_physid2;	/* sw copy of PHYSID2		   */
	unsigned short sw_advertise;	/* sw copy of ADVERTISE		   */
	unsigned short sw_lpa;		/* sw copy of LPA		   */
	unsigned short sw_csconfig;	/* sw copy of CSCONFIG		   */
};

static int ioc3_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
static void ioc3_set_multicast_list(struct net_device *dev);
static int ioc3_start_xmit(struct sk_buff *skb, struct net_device *dev);
static void ioc3_timeout(struct net_device *dev);
static inline unsigned int ioc3_hash(const unsigned char *addr);
static inline void ioc3_stop(struct ioc3_private *ip);
static void ioc3_init(struct ioc3_private *ip);

static const char ioc3_str[] = "IOC3 Ethernet";

/* We use this to acquire receive skb's that we can DMA directly into. */
#define ALIGNED_RX_SKB_ADDR(addr) \
	((((unsigned long)(addr) + (128 - 1)) & ~(128 - 1)) - (unsigned long)(addr))

#define ioc3_alloc_skb(__length, __gfp_flags) \
({	struct sk_buff *__skb; \
	__skb = alloc_skb((__length) + 128, (__gfp_flags)); \
	if (__skb) { \
		int __offset = ALIGNED_RX_SKB_ADDR(__skb->data); \
		if(__offset) \
			skb_reserve(__skb, __offset); \
	} \
	__skb; \
})

/* BEWARE: The IOC3 documentation documents the size of rx buffers as
   1644 while it's actually 1664.  This one was nasty to track down ...  */
#define RX_OFFSET		10
#define RX_BUF_ALLOC_SIZE	(1664 + RX_OFFSET + 128)

/* DMA barrier to separate cached and uncached accesses.  */
#define BARRIER()							\
	__asm__("sync" ::: "memory")


#define IOC3_SIZE 0x100000

#define ioc3_r(reg)							\
({									\
	u32 __res;							\
	__res = ioc3->reg;						\
	__res;								\
})

#define ioc3_w(reg,val)							\
do {									\
	(ioc3->reg = (val));						\
} while(0)

static inline u32
mcr_pack(u32 pulse, u32 sample)
{
	return (pulse << 10) | (sample << 2);
}

static int
nic_wait(struct ioc3 *ioc3)
{
	u32 mcr;

        do {
                mcr = ioc3_r(mcr);
        } while (!(mcr & 2));

        return mcr & 1;
}

static int
nic_reset(struct ioc3 *ioc3)
{
        int presence;

	ioc3_w(mcr, mcr_pack(500, 65));
	presence = nic_wait(ioc3);

	ioc3_w(mcr, mcr_pack(0, 500));
	nic_wait(ioc3);

        return presence;
}

static inline int
nic_read_bit(struct ioc3 *ioc3)
{
	int result;

	ioc3_w(mcr, mcr_pack(6, 13));
	result = nic_wait(ioc3);
	ioc3_w(mcr, mcr_pack(0, 100));
	nic_wait(ioc3);

	return result;
}

static inline void
nic_write_bit(struct ioc3 *ioc3, int bit)
{
	if (bit)
		ioc3_w(mcr, mcr_pack(6, 110));
	else
		ioc3_w(mcr, mcr_pack(80, 30));

	nic_wait(ioc3);
}

/*
 * Read a byte from an iButton device
 */
static u32
nic_read_byte(struct ioc3 *ioc3)
{
	u32 result = 0;
	int i;

	for (i = 0; i < 8; i++)
		result = (result >> 1) | (nic_read_bit(ioc3) << 7);

	return result;
}

/*
 * Write a byte to an iButton device
 */
static void
nic_write_byte(struct ioc3 *ioc3, int byte)
{
	int i, bit;

	for (i = 8; i; i--) {
		bit = byte & 1;
		byte >>= 1;

		nic_write_bit(ioc3, bit);
	}
}

static u64
nic_find(struct ioc3 *ioc3, int *last)
{
	int a, b, index, disc;
	u64 address = 0;

	nic_reset(ioc3);
	/* Search ROM.  */
	nic_write_byte(ioc3, 0xf0);

	/* Algorithm from ``Book of iButton Standards''.  */
	for (index = 0, disc = 0; index < 64; index++) {
		a = nic_read_bit(ioc3);
		b = nic_read_bit(ioc3);

		if (a && b) {
			printk("NIC search failed (not fatal).\n");
			*last = 0;
			return 0;
		}

		if (!a && !b) {
			if (index == *last) {
				address |= 1UL << index;
			} else if (index > *last) {
				address &= ~(1UL << index);
				disc = index;
			} else if ((address & (1UL << index)) == 0)
				disc = index;
			nic_write_bit(ioc3, address & (1UL << index));
			continue;
		} else {
			if (a)
				address |= 1UL << index;
			else
				address &= ~(1UL << index);
			nic_write_bit(ioc3, a);
			continue;
		}
	}

	*last = disc;

	return address;
}

static int nic_init(struct ioc3 *ioc3)
{
	const char *type;
	u8 crc;
	u8 serial[6];
	int save = 0, i;

	type = "unknown";

	while (1) {
		u64 reg;
		reg = nic_find(ioc3, &save);

		switch (reg & 0xff) {
		case 0x91:
			type = "DS1981U";
			break;
		default:
			if (save == 0) {
				/* Let the caller try again.  */
				return -1;
			}
			continue;
		}

		nic_reset(ioc3);

		/* Match ROM.  */
		nic_write_byte(ioc3, 0x55);
		for (i = 0; i < 8; i++)
			nic_write_byte(ioc3, (reg >> (i << 3)) & 0xff);

		reg >>= 8; /* Shift out type.  */
		for (i = 0; i < 6; i++) {
			serial[i] = reg & 0xff;
			reg >>= 8;
		}
		crc = reg & 0xff;
		break;
	}

	printk("Found %s NIC", type);
	if (type != "unknown") {
		printk (" registration number %02x:%02x:%02x:%02x:%02x:%02x,"
			" CRC %02x", serial[0], serial[1], serial[2],
			serial[3], serial[4], serial[5], crc);
	}
	printk(".\n");

	return 0;
}

/*
 * Read the NIC (Number-In-a-Can) device used to store the MAC address on
 * SN0 / SN00 nodeboards and PCI cards.
 */
static void ioc3_get_eaddr_nic(struct ioc3_private *ip)
{
	struct ioc3 *ioc3 = ip->regs;
	u8 nic[14];
	int tries = 2; /* There may be some problem with the battery?  */
	int i;

	ioc3_w(gpcr_s, (1 << 21));

	while (tries--) {
		if (!nic_init(ioc3))
			break;
		udelay(500);
	}

	if (tries < 0) {
		printk("Failed to read MAC address\n");
		return;
	}

	/* Read Memory.  */
	nic_write_byte(ioc3, 0xf0);
	nic_write_byte(ioc3, 0x00);
	nic_write_byte(ioc3, 0x00);

	for (i = 13; i >= 0; i--)
		nic[i] = nic_read_byte(ioc3);

	for (i = 2; i < 8; i++)
		ip->dev->dev_addr[i - 2] = nic[i];
}

/*
 * Ok, this is hosed by design.  It's necessary to know what machine the
 * NIC is in in order to know how to read the NIC address.  We also have
 * to know if it's a PCI card or a NIC in on the node board ...
 */
static void ioc3_get_eaddr(struct ioc3_private *ip)
{
	int i;


	ioc3_get_eaddr_nic(ip);

	printk("Ethernet address is ");
	for (i = 0; i < 6; i++) {
		printk("%02x", ip->dev->dev_addr[i]);
		if (i < 5)
			printk(":");
	}
	printk(".\n");
}


/*
 * Caller must hold the ioc3_lock ever for MII readers.  This is also
 * used to protect the transmitter side but it's low contention.
 */
static u16 mii_read(struct ioc3_private *ip, int reg)
{
	struct ioc3 *ioc3 = ip->regs;
	int phy = ip->phy;

	while (ioc3->micr & MICR_BUSY);
	ioc3->micr = (phy << MICR_PHYADDR_SHIFT) | reg | MICR_READTRIG;
	while (ioc3->micr & MICR_BUSY);

	return ioc3->midr_r & MIDR_DATA_MASK;
}

static void mii_write(struct ioc3_private *ip, int reg, u16 data)
{
	struct ioc3 *ioc3 = ip->regs;
	int phy = ip->phy;

	while (ioc3->micr & MICR_BUSY);
	ioc3->midr_w = data;
	ioc3->micr = (phy << MICR_PHYADDR_SHIFT) | reg;
	while (ioc3->micr & MICR_BUSY);
}

static int ioc3_mii_init(struct ioc3_private *ip);

static struct net_device_stats *ioc3_get_stats(struct net_device *dev)
{
	struct ioc3_private *ip = dev->priv;
	struct ioc3 *ioc3 = ip->regs;

	ip->stats.collisions += (ioc3->etcdc & ETCDC_COLLCNT_MASK);
	return &ip->stats;
}

static inline void
ioc3_rx(struct ioc3_private *ip)
{
	struct sk_buff *skb, *new_skb;
	struct ioc3 *ioc3 = ip->regs;
	int rx_entry, n_entry, len;
	struct ioc3_erxbuf *rxb;
	unsigned long *rxr;
	u32 w0, err;

	rxr = (unsigned long *) ip->rxr;		/* Ring base */
	rx_entry = ip->rx_ci;				/* RX consume index */
	n_entry = ip->rx_pi;

	skb = ip->rx_skbs[rx_entry];
	rxb = (struct ioc3_erxbuf *) (skb->data - RX_OFFSET);
	w0 = be32_to_cpu(rxb->w0);

	while (w0 & ERXBUF_V) {
		err = be32_to_cpu(rxb->err);		/* It's valid ...  */
		if (err & ERXBUF_GOODPKT) {
			len = ((w0 >> ERXBUF_BYTECNT_SHIFT) & 0x7ff) - 4;
			skb_trim(skb, len);
			skb->protocol = eth_type_trans(skb, ip->dev);

			new_skb = ioc3_alloc_skb(RX_BUF_ALLOC_SIZE, GFP_ATOMIC);
			if (!new_skb) {
				/* Ouch, drop packet and just recycle packet
				   to keep the ring filled.  */
				ip->stats.rx_dropped++;
				new_skb = skb;
				goto next;
			}
			netif_rx(skb);

			ip->rx_skbs[rx_entry] = NULL;	/* Poison  */

			new_skb->dev = ip->dev;

			/* Because we reserve afterwards. */
			skb_put(new_skb, (1664 + RX_OFFSET));
			rxb = (struct ioc3_erxbuf *) new_skb->data;
			skb_reserve(new_skb, RX_OFFSET);

			ip->dev->last_rx = jiffies;
			ip->stats.rx_packets++;		/* Statistics */
			ip->stats.rx_bytes += len;
		} else {
 			/* The frame is invalid and the skb never
                           reached the network layer so we can just
                           recycle it.  */
 			new_skb = skb;
 			ip->stats.rx_errors++;
		}
		if (err & ERXBUF_CRCERR)	/* Statistics */
			ip->stats.rx_crc_errors++;
		if (err & ERXBUF_FRAMERR)
			ip->stats.rx_frame_errors++;
next:
		ip->rx_skbs[n_entry] = new_skb;
		rxr[n_entry] = cpu_to_be64((0xa5UL << 56) |
		                         ((unsigned long) rxb & TO_PHYS_MASK));
		rxb->w0 = 0;				/* Clear valid flag */
		n_entry = (n_entry + 1) & 511;		/* Update erpir */

		/* Now go on to the next ring entry.  */
		rx_entry = (rx_entry + 1) & 511;
		skb = ip->rx_skbs[rx_entry];
		rxb = (struct ioc3_erxbuf *) (skb->data - RX_OFFSET);
		w0 = be32_to_cpu(rxb->w0);
	}
	ioc3->erpir = (n_entry << 3) | ERPIR_ARM;
	ip->rx_pi = n_entry;
	ip->rx_ci = rx_entry;
}

static inline void
ioc3_tx(struct ioc3_private *ip)
{
	unsigned long packets, bytes;
	struct ioc3 *ioc3 = ip->regs;
	int tx_entry, o_entry;
	struct sk_buff *skb;
	u32 etcir;

	spin_lock(&ip->ioc3_lock);
	etcir = ioc3->etcir;

	tx_entry = (etcir >> 7) & 127;
	o_entry = ip->tx_ci;
	packets = 0;
	bytes = 0;

	while (o_entry != tx_entry) {
		packets++;
		skb = ip->tx_skbs[o_entry];
		bytes += skb->len;
		dev_kfree_skb_irq(skb);
		ip->tx_skbs[o_entry] = NULL;

		o_entry = (o_entry + 1) & 127;		/* Next */

		etcir = ioc3->etcir;			/* More pkts sent?  */
		tx_entry = (etcir >> 7) & 127;
	}

	ip->stats.tx_packets += packets;
	ip->stats.tx_bytes += bytes;
	ip->txqlen -= packets;

	if (ip->txqlen < 128)
		netif_wake_queue(ip->dev);

	ip->tx_ci = o_entry;
	spin_unlock(&ip->ioc3_lock);
}

/*
 * Deal with fatal IOC3 errors.  This condition might be caused by a hard or
 * software problems, so we should try to recover
 * more gracefully if this ever happens.  In theory we might be flooded
 * with such error interrupts if something really goes wrong, so we might
 * also consider to take the interface down.
 */
static void
ioc3_error(struct ioc3_private *ip, u32 eisr)
{
	struct net_device *dev = ip->dev;
	unsigned char *iface = dev->name;

	if (eisr & EISR_RXOFLO)
		printk(KERN_ERR "%s: RX overflow.\n", iface);
	if (eisr & EISR_RXBUFOFLO)
		printk(KERN_ERR "%s: RX buffer overflow.\n", iface);
	if (eisr & EISR_RXMEMERR)
		printk(KERN_ERR "%s: RX PCI error.\n", iface);
	if (eisr & EISR_RXPARERR)
		printk(KERN_ERR "%s: RX SSRAM parity error.\n", iface);
	if (eisr & EISR_TXBUFUFLO)
		printk(KERN_ERR "%s: TX buffer underflow.\n", iface);
	if (eisr & EISR_TXMEMERR)
		printk(KERN_ERR "%s: TX PCI error.\n", iface);

	ioc3_stop(ip);
	ioc3_init(ip);
	ioc3_mii_init(ip);

	dev->trans_start = jiffies;
	netif_wake_queue(dev);
}

/* The interrupt handler does all of the Rx thread work and cleans up
   after the Tx thread.  */
static void ioc3_interrupt(int irq, void *_dev, struct pt_regs *regs)
{
	struct net_device *dev = (struct net_device *)_dev;
	struct ioc3_private *ip = dev->priv;
	struct ioc3 *ioc3 = ip->regs;
	const u32 enabled = EISR_RXTIMERINT | EISR_RXOFLO | EISR_RXBUFOFLO |
	                    EISR_RXMEMERR | EISR_RXPARERR | EISR_TXBUFUFLO |
	                    EISR_TXEXPLICIT | EISR_TXMEMERR;
	u32 eisr;

	eisr = ioc3->eisr & enabled;

	while (eisr) {
		ioc3->eisr = eisr;
		ioc3->eisr;				/* Flush */

		if (eisr & (EISR_RXOFLO | EISR_RXBUFOFLO | EISR_RXMEMERR |
		            EISR_RXPARERR | EISR_TXBUFUFLO | EISR_TXMEMERR))
			ioc3_error(ip, eisr);
		if (eisr & EISR_RXTIMERINT)
			ioc3_rx(ip);
		if (eisr & EISR_TXEXPLICIT)
			ioc3_tx(ip);

		eisr = ioc3->eisr & enabled;
	}
}

/*
 * Auto negotiation.  The scheme is very simple.  We have a timer routine that
 * keeps watching the auto negotiation process as it progresses.  The DP83840
 * is first told to start doing it's thing, we set up the time and place the
 * timer state machine in it's initial state.
 *
 * Here the timer peeks at the DP83840 status registers at each click to see
 * if the auto negotiation has completed, we assume here that the DP83840 PHY
 * will time out at some point and just tell us what (didn't) happen.  For
 * complete coverage we only allow so many of the ticks at this level to run,
 * when this has expired we print a warning message and try another strategy.
 * This "other" strategy is to force the interface into various speed/duplex
 * configurations and we stop when we see a link-up condition before the
 * maximum number of "peek" ticks have occurred.
 *
 * Once a valid link status has been detected we configure the IOC3 to speak
 * the most efficient protocol we could get a clean link for.  The priority
 * for link configurations, highest first is:
 *
 *     100 Base-T Full Duplex
 *     100 Base-T Half Duplex
 *     10 Base-T Full Duplex
 *     10 Base-T Half Duplex
 *
 * We start a new timer now, after a successful auto negotiation status has
 * been detected.  This timer just waits for the link-up bit to get set in
 * the BMCR of the DP83840.  When this occurs we print a kernel log message
 * describing the link type in use and the fact that it is up.
 *
 * If a fatal error of some sort is signalled and detected in the interrupt
 * service routine, and the chip is reset, or the link is ifconfig'd down
 * and then back up, this entire process repeats itself all over again.
 */
static int ioc3_try_next_permutation(struct ioc3_private *ip)
{
	ip->sw_bmcr = mii_read(ip, MII_BMCR);

	/* Downgrade from full to half duplex.  Only possible via ethtool.  */
	if (ip->sw_bmcr & BMCR_FULLDPLX) {
		ip->sw_bmcr &= ~BMCR_FULLDPLX;
		mii_write(ip, MII_BMCR, ip->sw_bmcr);

		return 0;
	}

	/* Downgrade from 100 to 10. */
	if (ip->sw_bmcr & BMCR_SPEED100) {
		ip->sw_bmcr &= ~BMCR_SPEED100;
		mii_write(ip, MII_BMCR, ip->sw_bmcr);

		return 0;
	}

	/* We've tried everything. */
	return -1;
}

static void
ioc3_display_link_mode(struct ioc3_private *ip)
{
	char *tmode = "";

	ip->sw_lpa = mii_read(ip, MII_LPA);

	if (ip->sw_lpa & (LPA_100HALF | LPA_100FULL)) {
		if (ip->sw_lpa & LPA_100FULL)
			tmode = "100Mb/s, Full Duplex";
		else
			tmode = "100Mb/s, Half Duplex";
	} else {
		if (ip->sw_lpa & LPA_10FULL)
			tmode = "10Mb/s, Full Duplex";
		else
			tmode = "10Mb/s, Half Duplex";
	}

	printk(KERN_INFO "%s: Link is up at %s.\n", ip->dev->name, tmode);
}

static void
ioc3_display_forced_link_mode(struct ioc3_private *ip)
{
	char *speed = "", *duplex = "";

	ip->sw_bmcr = mii_read(ip, MII_BMCR);
	if (ip->sw_bmcr & BMCR_SPEED100)
		speed = "100Mb/s, ";
	else
		speed = "10Mb/s, ";
	if (ip->sw_bmcr & BMCR_FULLDPLX)
		duplex = "Full Duplex.\n";
	else
		duplex = "Half Duplex.\n";

	printk(KERN_INFO "%s: Link has been forced up at %s%s", ip->dev->name,
	       speed, duplex);
}

static int ioc3_set_link_modes(struct ioc3_private *ip)
{
	struct ioc3 *ioc3 = ip->regs;
	int full;

	/*
	 * All we care about is making sure the bigmac tx_cfg has a
	 * proper duplex setting.
	 */
	if (ip->timer_state == arbwait) {
		ip->sw_lpa = mii_read(ip, MII_LPA);
		if (!(ip->sw_lpa & (LPA_10HALF | LPA_10FULL |
		                    LPA_100HALF | LPA_100FULL)))
			goto no_response;
		if (ip->sw_lpa & LPA_100FULL)
			full = 1;
		else if (ip->sw_lpa & LPA_100HALF)
			full = 0;
		else if (ip->sw_lpa & LPA_10FULL)
			full = 1;
		else
			full = 0;
	} else {
		/* Forcing a link mode. */
		ip->sw_bmcr = mii_read(ip, MII_BMCR);
		if (ip->sw_bmcr & BMCR_FULLDPLX)
			full = 1;
		else
			full = 0;
	}

	if (full)
		ip->emcr |= EMCR_DUPLEX;
	else
		ip->emcr &= ~EMCR_DUPLEX;

	ioc3->emcr = ip->emcr;
	ioc3->emcr;

	return 0;

no_response:

	return 1;
}

static int is_lucent_phy(struct ioc3_private *ip)
{
	unsigned short mr2, mr3;
	int ret = 0;

	mr2 = mii_read(ip, MII_PHYSID1);
	mr3 = mii_read(ip, MII_PHYSID2);
	if ((mr2 & 0xffff) == 0x0180 && ((mr3 & 0xffff) >> 10) == 0x1d) {
		ret = 1;
	}

	return ret;
}

static void ioc3_timer(unsigned long data)
{
	struct ioc3_private *ip = (struct ioc3_private *) data;
	int restart_timer = 0;

	ip->timer_ticks++;
	switch (ip->timer_state) {
	case arbwait:
		/*
		 * Only allow for 5 ticks, thats 10 seconds and much too
		 * long to wait for arbitration to complete.
		 */
		if (ip->timer_ticks >= 10) {
			/* Enter force mode. */
	do_force_mode:
			ip->sw_bmcr = mii_read(ip, MII_BMCR);
			printk(KERN_NOTICE "%s: Auto-Negotiation unsuccessful,"
			       " trying force link mode\n", ip->dev->name);
			ip->sw_bmcr = BMCR_SPEED100;
			mii_write(ip, MII_BMCR, ip->sw_bmcr);

			if (!is_lucent_phy(ip)) {
				/*
				 * OK, seems we need do disable the transceiver
				 * for the first tick to make sure we get an
				 * accurate link state at the second tick.
				 */
				ip->sw_csconfig = mii_read(ip, MII_CSCONFIG);
				ip->sw_csconfig &= ~(CSCONFIG_TCVDISAB);
				mii_write(ip, MII_CSCONFIG, ip->sw_csconfig);
			}
			ip->timer_state = ltrywait;
			ip->timer_ticks = 0;
			restart_timer = 1;
		} else {
			/* Anything interesting happen? */
			ip->sw_bmsr = mii_read(ip, MII_BMSR);
			if (ip->sw_bmsr & BMSR_ANEGCOMPLETE) {
				int ret;

				/* Just what we've been waiting for... */
				ret = ioc3_set_link_modes(ip);
				if (ret) {
					/* Ooops, something bad happened, go to
					 * force mode.
					 *
					 * XXX Broken hubs which don't support
					 * XXX 802.3u auto-negotiation make this
					 * XXX happen as well.
					 */
					goto do_force_mode;
				}

				/*
				 * Success, at least so far, advance our state
				 * engine.
				 */
				ip->timer_state = lupwait;
				restart_timer = 1;
			} else {
				restart_timer = 1;
			}
		}
		break;

	case lupwait:
		/*
		 * Auto negotiation was successful and we are awaiting a
		 * link up status.  I have decided to let this timer run
		 * forever until some sort of error is signalled, reporting
		 * a message to the user at 10 second intervals.
		 */
		ip->sw_bmsr = mii_read(ip, MII_BMSR);
		if (ip->sw_bmsr & BMSR_LSTATUS) {
			/*
			 * Wheee, it's up, display the link mode in use and put
			 * the timer to sleep.
			 */
			ioc3_display_link_mode(ip);
			ip->timer_state = asleep;
			restart_timer = 0;
		} else {
			if (ip->timer_ticks >= 10) {
				printk(KERN_NOTICE "%s: Auto negotiation successful, link still "
				       "not completely up.\n", ip->dev->name);
				ip->timer_ticks = 0;
				restart_timer = 1;
			} else {
				restart_timer = 1;
			}
		}
		break;

	case ltrywait:
		/*
		 * Making the timeout here too long can make it take
		 * annoyingly long to attempt all of the link mode
		 * permutations, but then again this is essentially
		 * error recovery code for the most part.
		 */
		ip->sw_bmsr = mii_read(ip, MII_BMSR);
		ip->sw_csconfig = mii_read(ip, MII_CSCONFIG);
		if (ip->timer_ticks == 1) {
			if (!is_lucent_phy(ip)) {
				/*
				 * Re-enable transceiver, we'll re-enable the
				 * transceiver next tick, then check link state
				 * on the following tick.
				 */
				ip->sw_csconfig |= CSCONFIG_TCVDISAB;
				mii_write(ip, MII_CSCONFIG, ip->sw_csconfig);
			}
			restart_timer = 1;
			break;
		}
		if (ip->timer_ticks == 2) {
			if (!is_lucent_phy(ip)) {
				ip->sw_csconfig &= ~(CSCONFIG_TCVDISAB);
				mii_write(ip, MII_CSCONFIG, ip->sw_csconfig);
			}
			restart_timer = 1;
			break;
		}
		if (ip->sw_bmsr & BMSR_LSTATUS) {
			/* Force mode selection success. */
			ioc3_display_forced_link_mode(ip);
			ioc3_set_link_modes(ip);  /* XXX error? then what? */
			ip->timer_state = asleep;
			restart_timer = 0;
		} else {
			if (ip->timer_ticks >= 4) { /* 6 seconds or so... */
				int ret;

				ret = ioc3_try_next_permutation(ip);
				if (ret == -1) {
					/*
					 * Aieee, tried them all, reset the
					 * chip and try all over again.
					 */
					printk(KERN_NOTICE "%s: Link down, "
					       "cable problem?\n",
					       ip->dev->name);

					ioc3_init(ip);
					return;
				}
				if (!is_lucent_phy(ip)) {
					ip->sw_csconfig = mii_read(ip,
					                    MII_CSCONFIG);
					ip->sw_csconfig |= CSCONFIG_TCVDISAB;
					mii_write(ip, MII_CSCONFIG,
					          ip->sw_csconfig);
				}
				ip->timer_ticks = 0;
				restart_timer = 1;
			} else {
				restart_timer = 1;
			}
		}
		break;

	case asleep:
	default:
		/* Can't happens.... */
		printk(KERN_ERR "%s: Aieee, link timer is asleep but we got "
		       "one anyways!\n", ip->dev->name);
		restart_timer = 0;
		ip->timer_ticks = 0;
		ip->timer_state = asleep; /* foo on you */
		break;
	};

	if (restart_timer) {
		ip->ioc3_timer.expires = jiffies + ((12 * HZ)/10); /* 1.2s */
		add_timer(&ip->ioc3_timer);
	}
}

static void
ioc3_start_auto_negotiation(struct ioc3_private *ip, struct ethtool_cmd *ep)
{
	int timeout;

	/* Read all of the registers we are interested in now. */
	ip->sw_bmsr      = mii_read(ip, MII_BMSR);
	ip->sw_bmcr      = mii_read(ip, MII_BMCR);
	ip->sw_physid1   = mii_read(ip, MII_PHYSID1);
	ip->sw_physid2   = mii_read(ip, MII_PHYSID2);

	/* XXX Check BMSR_ANEGCAPABLE, should not be necessary though. */

	ip->sw_advertise = mii_read(ip, MII_ADVERTISE);
	if (ep == NULL || ep->autoneg == AUTONEG_ENABLE) {
		/* Advertise everything we can support. */
		if (ip->sw_bmsr & BMSR_10HALF)
			ip->sw_advertise |= ADVERTISE_10HALF;
		else
			ip->sw_advertise &= ~ADVERTISE_10HALF;

		if (ip->sw_bmsr & BMSR_10FULL)
			ip->sw_advertise |= ADVERTISE_10FULL;
		else
			ip->sw_advertise &= ~ADVERTISE_10FULL;
		if (ip->sw_bmsr & BMSR_100HALF)
			ip->sw_advertise |= ADVERTISE_100HALF;
		else
			ip->sw_advertise &= ~ADVERTISE_100HALF;
		if (ip->sw_bmsr & BMSR_100FULL)
			ip->sw_advertise |= ADVERTISE_100FULL;
		else
			ip->sw_advertise &= ~ADVERTISE_100FULL;
		mii_write(ip, MII_ADVERTISE, ip->sw_advertise);

		/*
		 * XXX Currently no IOC3 card I know off supports 100BaseT4,
		 * XXX and this is because the DP83840 does not support it,
		 * XXX changes XXX would need to be made to the tx/rx logic in
		 * XXX the driver as well so I completely skip checking for it
		 * XXX in the BMSR for now.
		 */

#ifdef AUTO_SWITCH_DEBUG
		ASD(("%s: Advertising [ ", ip->dev->name));
		if (ip->sw_advertise & ADVERTISE_10HALF)
			ASD(("10H "));
		if (ip->sw_advertise & ADVERTISE_10FULL)
			ASD(("10F "));
		if (ip->sw_advertise & ADVERTISE_100HALF)
			ASD(("100H "));
		if (ip->sw_advertise & ADVERTISE_100FULL)
			ASD(("100F "));
#endif

		/* Enable Auto-Negotiation, this is usually on already... */
		ip->sw_bmcr |= BMCR_ANENABLE;
		mii_write(ip, MII_BMCR, ip->sw_bmcr);

		/* Restart it to make sure it is going. */
		ip->sw_bmcr |= BMCR_ANRESTART;
		mii_write(ip, MII_BMCR, ip->sw_bmcr);

		/* BMCR_ANRESTART self clears when the process has begun. */

		timeout = 64;  /* More than enough. */
		while (--timeout) {
			ip->sw_bmcr = mii_read(ip, MII_BMCR);
			if (!(ip->sw_bmcr & BMCR_ANRESTART))
				break; /* got it. */
			udelay(10);
		}
		if (!timeout) {
			printk(KERN_ERR "%s: IOC3 would not start auto "
			       "negotiation BMCR=0x%04x\n",
			       ip->dev->name, ip->sw_bmcr);
			printk(KERN_NOTICE "%s: Performing force link "
			       "detection.\n", ip->dev->name);
			goto force_link;
		} else {
			ip->timer_state = arbwait;
		}
	} else {
force_link:
		/*
		 * Force the link up, trying first a particular mode.  Either
		 * we are here at the request of ethtool or because the IOC3
		 * would not start to autoneg.
		 */

		/*
		 * Disable auto-negotiation in BMCR, enable the duplex and
		 * speed setting, init the timer state machine, and fire it off.
		 */
		if (ep == NULL || ep->autoneg == AUTONEG_ENABLE) {
			ip->sw_bmcr = BMCR_SPEED100;
		} else {
			if (ep->speed == SPEED_100)
				ip->sw_bmcr = BMCR_SPEED100;
			else
				ip->sw_bmcr = 0;
			if (ep->duplex == DUPLEX_FULL)
				ip->sw_bmcr |= BMCR_FULLDPLX;
		}
		mii_write(ip, MII_BMCR, ip->sw_bmcr);

		if (!is_lucent_phy(ip)) {
			/*
			 * OK, seems we need do disable the transceiver for the
			 * first tick to make sure we get an accurate link
			 * state at the second tick.
			 */
			ip->sw_csconfig = mii_read(ip, MII_CSCONFIG);
			ip->sw_csconfig &= ~(CSCONFIG_TCVDISAB);
			mii_write(ip, MII_CSCONFIG, ip->sw_csconfig);
		}
		ip->timer_state = ltrywait;
	}

	del_timer(&ip->ioc3_timer);
	ip->timer_ticks = 0;
	ip->ioc3_timer.expires = jiffies + (12 * HZ)/10;  /* 1.2 sec. */
	ip->ioc3_timer.data = (unsigned long) ip;
	ip->ioc3_timer.function = &ioc3_timer;
	add_timer(&ip->ioc3_timer);
}

static int ioc3_mii_init(struct ioc3_private *ip)
{
	int i, found;
	u16 word;

	found = 0;
	spin_lock_irq(&ip->ioc3_lock);
	for (i = 0; i < 32; i++) {
		ip->phy = i;
		word = mii_read(ip, 2);
		if ((word != 0xffff) && (word != 0x0000)) {
			found = 1;
			break;			/* Found a PHY		*/
		}
	}
	if (!found) {
		spin_unlock_irq(&ip->ioc3_lock);
		return -ENODEV;
	}

	ioc3_start_auto_negotiation(ip, NULL);		// XXX ethtool

	spin_unlock_irq(&ip->ioc3_lock);

	return 0;
}

static inline void
ioc3_clean_rx_ring(struct ioc3_private *ip)
{
	struct sk_buff *skb;
	int i;

	for (i = ip->rx_ci; i & 15; i++) {
		ip->rx_skbs[ip->rx_pi] = ip->rx_skbs[ip->rx_ci];
		ip->rxr[ip->rx_pi++] = ip->rxr[ip->rx_ci++];
	}
	ip->rx_pi &= 511;
	ip->rx_ci &= 511;

	for (i = ip->rx_ci; i != ip->rx_pi; i = (i+1) & 511) {
		struct ioc3_erxbuf *rxb;
		skb = ip->rx_skbs[i];
		rxb = (struct ioc3_erxbuf *) (skb->data - RX_OFFSET);
		rxb->w0 = 0;
	}
}

static inline void
ioc3_clean_tx_ring(struct ioc3_private *ip)
{
	struct sk_buff *skb;
	int i;

	for (i=0; i < 128; i++) {
		skb = ip->tx_skbs[i];
		if (skb) {
			ip->tx_skbs[i] = NULL;
			dev_kfree_skb_any(skb);
		}
		ip->txr[i].cmd = 0;
	}
	ip->tx_pi = 0;
	ip->tx_ci = 0;
}

static void
ioc3_free_rings(struct ioc3_private *ip)
{
	struct sk_buff *skb;
	int rx_entry, n_entry;

	if (ip->txr) {
		ioc3_clean_tx_ring(ip);
		free_pages((unsigned long)ip->txr, 2);
		ip->txr = NULL;
	}

	if (ip->rxr) {
		n_entry = ip->rx_ci;
		rx_entry = ip->rx_pi;

		while (n_entry != rx_entry) {
			skb = ip->rx_skbs[n_entry];
			if (skb)
				dev_kfree_skb_any(skb);

			n_entry = (n_entry + 1) & 511;
		}
		free_page((unsigned long)ip->rxr);
		ip->rxr = NULL;
	}
}

static void
ioc3_alloc_rings(struct net_device *dev, struct ioc3_private *ip,
		 struct ioc3 *ioc3)
{
	struct ioc3_erxbuf *rxb;
	unsigned long *rxr;
	int i;

	if (ip->rxr == NULL) {
		/* Allocate and initialize rx ring.  4kb = 512 entries  */
		ip->rxr = (unsigned long *) get_free_page(GFP_ATOMIC);
		rxr = (unsigned long *) ip->rxr;
		if (!rxr)
			printk("ioc3_alloc_rings(): get_free_page() failed!\n");

		/* Now the rx buffers.  The RX ring may be larger but
		   we only allocate 16 buffers for now.  Need to tune
		   this for performance and memory later.  */
		for (i = 0; i < RX_BUFFS; i++) {
			struct sk_buff *skb;

			skb = ioc3_alloc_skb(RX_BUF_ALLOC_SIZE, GFP_ATOMIC);
			if (!skb) {
				show_free_areas();
				continue;
			}

			ip->rx_skbs[i] = skb;
			skb->dev = dev;

			/* Because we reserve afterwards. */
			skb_put(skb, (1664 + RX_OFFSET));
			rxb = (struct ioc3_erxbuf *) skb->data;
			rxr[i] = cpu_to_be64((0xa5UL << 56) |
			                ((unsigned long) rxb & TO_PHYS_MASK));
			skb_reserve(skb, RX_OFFSET);
		}
		ip->rx_ci = 0;
		ip->rx_pi = RX_BUFFS;
	}

	if (ip->txr == NULL) {
		/* Allocate and initialize tx rings.  16kb = 128 bufs.  */
		ip->txr = (struct ioc3_etxd *)__get_free_pages(GFP_KERNEL, 2);
		if (!ip->txr)
			printk("ioc3_alloc_rings(): get_free_page() failed!\n");
		ip->tx_pi = 0;
		ip->tx_ci = 0;
	}
}

static void
ioc3_init_rings(struct net_device *dev, struct ioc3_private *ip,
	        struct ioc3 *ioc3)
{
	unsigned long ring;

	ioc3_free_rings(ip);
	ioc3_alloc_rings(dev, ip, ioc3);

	ioc3_clean_rx_ring(ip);
	ioc3_clean_tx_ring(ip);

	/* Now the rx ring base, consume & produce registers.  */
	ring = (0xa5UL << 56) | ((unsigned long)ip->rxr & TO_PHYS_MASK);
	ioc3->erbr_h = ring >> 32;
	ioc3->erbr_l = ring & 0xffffffff;
	ioc3->ercir  = (ip->rx_ci << 3);
	ioc3->erpir  = (ip->rx_pi << 3) | ERPIR_ARM;

	ring = (0xa5UL << 56) | ((unsigned long)ip->txr & TO_PHYS_MASK);

	ip->txqlen = 0;					/* nothing queued  */

	/* Now the tx ring base, consume & produce registers.  */
	ioc3->etbr_h = ring >> 32;
	ioc3->etbr_l = ring & 0xffffffff;
	ioc3->etpir  = (ip->tx_pi << 7);
	ioc3->etcir  = (ip->tx_ci << 7);
	ioc3->etcir;					/* Flush */
}

static inline void
ioc3_ssram_disc(struct ioc3_private *ip)
{
	struct ioc3 *ioc3 = ip->regs;
	volatile u32 *ssram0 = &ioc3->ssram[0x0000];
	volatile u32 *ssram1 = &ioc3->ssram[0x4000];
	unsigned int pattern = 0x5555;

	/* Assume the larger size SSRAM and enable parity checking */
	ioc3->emcr |= (EMCR_BUFSIZ | EMCR_RAMPAR);

	*ssram0 = pattern;
	*ssram1 = ~pattern & IOC3_SSRAM_DM;

	if ((*ssram0 & IOC3_SSRAM_DM) != pattern ||
	    (*ssram1 & IOC3_SSRAM_DM) != (~pattern & IOC3_SSRAM_DM)) {
		/* set ssram size to 64 KB */
		ip->emcr = EMCR_RAMPAR;
		ioc3->emcr &= ~EMCR_BUFSIZ;
	} else {
		ip->emcr = EMCR_BUFSIZ | EMCR_RAMPAR;
	}
}

static void ioc3_init(struct ioc3_private *ip)
{
	struct net_device *dev = ip->dev;
	struct ioc3 *ioc3 = ip->regs;

	del_timer(&ip->ioc3_timer);		/* Kill if running	*/

	ioc3->emcr = EMCR_RST;			/* Reset		*/
	ioc3->emcr;				/* Flush WB		*/
	udelay(4);				/* Give it time ...	*/
	ioc3->emcr = 0;
	ioc3->emcr;

	/* Misc registers  */
	ioc3->erbar = 0;
	ioc3->etcsr = (17<<ETCSR_IPGR2_SHIFT) | (11<<ETCSR_IPGR1_SHIFT) | 21;
	ioc3->etcdc;				/* Clear on read */
	ioc3->ercsr = 15;			/* RX low watermark  */
	ioc3->ertr = 0;				/* Interrupt immediately */
	ioc3->emar_h = (dev->dev_addr[5] << 8) | dev->dev_addr[4];
	ioc3->emar_l = (dev->dev_addr[3] << 24) | (dev->dev_addr[2] << 16) |
	               (dev->dev_addr[1] <<  8) | dev->dev_addr[0];
	ioc3->ehar_h = ip->ehar_h;
	ioc3->ehar_l = ip->ehar_l;
	ioc3->ersr = 42;			/* XXX should be random */

	ioc3_init_rings(ip->dev, ip, ioc3);

	ip->emcr |= ((RX_OFFSET / 2) << EMCR_RXOFF_SHIFT) | EMCR_TXDMAEN |
	             EMCR_TXEN | EMCR_RXDMAEN | EMCR_RXEN;
	ioc3->emcr = ip->emcr;
	ioc3->eier = EISR_RXTIMERINT | EISR_RXOFLO | EISR_RXBUFOFLO |
	             EISR_RXMEMERR | EISR_RXPARERR | EISR_TXBUFUFLO |
	             EISR_TXEXPLICIT | EISR_TXMEMERR;
	ioc3->eier;
}

static inline void ioc3_stop(struct ioc3_private *ip)
{
	struct ioc3 *ioc3 = ip->regs;

	ioc3->emcr = 0;				/* Shutup */
	ioc3->eier = 0;				/* Disable interrupts */
	ioc3->eier;				/* Flush */
}

static int
ioc3_open(struct net_device *dev)
{
	struct ioc3_private *ip = dev->priv;

	if (request_irq(dev->irq, ioc3_interrupt, SA_SHIRQ, ioc3_str, dev)) {
		printk(KERN_ERR "%s: Can't get irq %d\n", dev->name, dev->irq);

		return -EAGAIN;
	}

	ip->ehar_h = 0;
	ip->ehar_l = 0;
	ioc3_init(ip);

	netif_start_queue(dev);
	return 0;
}

static int
ioc3_close(struct net_device *dev)
{
	struct ioc3_private *ip = dev->priv;

	del_timer(&ip->ioc3_timer);

	netif_stop_queue(dev);

	ioc3_stop(ip);
	free_irq(dev->irq, dev);

	ioc3_free_rings(ip);
	return 0;
}

/*
 * MENET cards have four IOC3 chips, which are attached to two sets of
 * PCI slot resources each: the primary connections are on slots
 * 0..3 and the secondaries are on 4..7
 *
 * All four ethernets are brought out to connectors; six serial ports
 * (a pair from each of the first three IOC3s) are brought out to
 * MiniDINs; all other subdevices are left swinging in the wind, leave
 * them disabled.
 */
static inline int ioc3_is_menet(struct pci_dev *pdev)
{
	struct pci_dev *dev;

	return pdev->bus->parent == NULL
	       && (dev = pci_find_slot(pdev->bus->number, PCI_DEVFN(0, 0)))
	       && dev->vendor == PCI_VENDOR_ID_SGI
	       && dev->device == PCI_DEVICE_ID_SGI_IOC3
	       && (dev = pci_find_slot(pdev->bus->number, PCI_DEVFN(1, 0)))
	       && dev->vendor == PCI_VENDOR_ID_SGI
	       && dev->device == PCI_DEVICE_ID_SGI_IOC3
	       && (dev = pci_find_slot(pdev->bus->number, PCI_DEVFN(2, 0)))
	       && dev->vendor == PCI_VENDOR_ID_SGI
	       && dev->device == PCI_DEVICE_ID_SGI_IOC3;
}

static void inline ioc3_serial_probe(struct pci_dev *pdev,
				struct ioc3 *ioc3)
{
	struct serial_struct req;

	/*
	 * We need to recognice and treat the fourth MENET serial as it
	 * does not have an SuperIO chip attached to it, therefore attempting
	 * to access it will result in bus errors.  We call something an
	 * MENET if PCI slot 0, 1, 2 and 3 of a master PCI bus all have an IOC3
	 * in it.  This is paranoid but we want to avoid blowing up on a
	 * showhorn PCI box that happens to have 4 IOC3 cards in it so it's
	 * not paranoid enough ...
	 */
	if (ioc3_is_menet(pdev) && PCI_SLOT(pdev->devfn) == 3)
		return;

	/* Register to interrupt zero because we share the interrupt with
	   the serial driver which we don't properly support yet.  */
	memset(&req, 0, sizeof(req));
	req.irq             = 0;
	req.flags           = IOC3_COM_FLAGS;
	req.io_type         = SERIAL_IO_MEM;
	req.iomem_reg_shift = 0;
	req.baud_base       = IOC3_BAUD;

	req.iomem_base      = (unsigned char *) &ioc3->sregs.uarta;
	register_serial(&req);

	req.iomem_base      = (unsigned char *) &ioc3->sregs.uartb;
	register_serial(&req);
}

static int __devinit ioc3_probe(struct pci_dev *pdev,
	                        const struct pci_device_id *ent)
{
	struct net_device *dev = NULL;
	struct ioc3_private *ip;
	struct ioc3 *ioc3;
	unsigned long ioc3_base, ioc3_size;
	u32 vendor, model, rev;
	int err;

	dev = alloc_etherdev(sizeof(struct ioc3_private));
	if (!dev)
		return -ENOMEM;

	err = pci_request_regions(pdev, "ioc3");
	if (err)
		goto out_free;

	SET_MODULE_OWNER(dev);
	ip = dev->priv;
	ip->dev = dev;

	dev->irq = pdev->irq;

	ioc3_base = pci_resource_start(pdev, 0);
	ioc3_size = pci_resource_len(pdev, 0);
	ioc3 = (struct ioc3 *) ioremap(ioc3_base, ioc3_size);
	if (!ioc3) {
		printk(KERN_CRIT "ioc3eth(%s): ioremap failed, goodbye.\n",
		       pdev->slot_name);
		err = -ENOMEM;
		goto out_res;
	}
	ip->regs = ioc3;

#ifdef CONFIG_SERIAL
	ioc3_serial_probe(pdev, ioc3);
#endif

	spin_lock_init(&ip->ioc3_lock);
	init_timer(&ip->ioc3_timer);

	ioc3_stop(ip);
	ioc3_init(ip);

	ioc3_mii_init(ip);

	if (ip->phy == -1) {
		printk(KERN_CRIT "ioc3-eth(%s): Didn't find a PHY, goodbye.\n",
		       pdev->slot_name);
		err = -ENODEV;
		goto out_stop;
	}

	ioc3_ssram_disc(ip);
	ioc3_get_eaddr(ip);

	/* The IOC3-specific entries in the device structure. */
	dev->open		= ioc3_open;
	dev->hard_start_xmit	= ioc3_start_xmit;
	dev->tx_timeout		= ioc3_timeout;
	dev->watchdog_timeo	= 5 * HZ;
	dev->stop		= ioc3_close;
	dev->get_stats		= ioc3_get_stats;
	dev->do_ioctl		= ioc3_ioctl;
	dev->set_multicast_list	= ioc3_set_multicast_list;

	err = register_netdev(dev);
	if (err)
		goto out_stop;

	vendor = (ip->sw_physid1 << 12) | (ip->sw_physid2 >> 4);
	model  = (ip->sw_physid2 >> 4) & 0x3f;
	rev    = ip->sw_physid2 & 0xf;
	printk(KERN_INFO "%s: Using PHY %d, vendor 0x%x, model %d, "
	       "rev %d.\n", dev->name, ip->phy, vendor, model, rev);
	printk(KERN_INFO "%s: IOC3 SSRAM has %d kbyte.\n", dev->name,
	       ip->emcr & EMCR_BUFSIZ ? 128 : 64);

	return 0;

out_stop:
	ioc3_stop(ip);
	free_irq(dev->irq, dev);
	ioc3_free_rings(ip);
out_res:
	pci_release_regions(pdev);
out_free:
	kfree(dev);
	return err;
}

static void __devexit ioc3_remove_one (struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	struct ioc3_private *ip = dev->priv;
	struct ioc3 *ioc3 = ip->regs;

	unregister_netdev(dev);
	iounmap(ioc3);
	pci_release_regions(pdev);
	kfree(dev);
}

static struct pci_device_id ioc3_pci_tbl[] __devinitdata = {
	{ PCI_VENDOR_ID_SGI, PCI_DEVICE_ID_SGI_IOC3, PCI_ANY_ID, PCI_ANY_ID },
	{ 0 }
};
MODULE_DEVICE_TABLE(pci, ioc3_pci_tbl);

static struct pci_driver ioc3_driver = {
	.name		= "ioc3-eth",
	.id_table	= ioc3_pci_tbl,
	.probe		= ioc3_probe,
	.remove		= __devexit_p(ioc3_remove_one),
};

static int __init ioc3_init_module(void)
{
	return pci_module_init(&ioc3_driver);
}

static void __exit ioc3_cleanup_module(void)
{
	pci_unregister_driver(&ioc3_driver);
}

static int
ioc3_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	unsigned long data;
	struct ioc3_private *ip = dev->priv;
	struct ioc3 *ioc3 = ip->regs;
	unsigned int len;
	struct ioc3_etxd *desc;
	int produce;

	spin_lock_irq(&ip->ioc3_lock);

	data = (unsigned long) skb->data;
	len = skb->len;

	produce = ip->tx_pi;
	desc = &ip->txr[produce];

	if (len <= 104) {
		/* Short packet, let's copy it directly into the ring.  */
		memcpy(desc->data, skb->data, skb->len);
		if (len < ETH_ZLEN) {
			/* Very short packet, pad with zeros at the end. */
			memset(desc->data + len, 0, ETH_ZLEN - len);
			len = ETH_ZLEN;
		}
		desc->cmd    = cpu_to_be32(len | ETXD_INTWHENDONE | ETXD_D0V);
		desc->bufcnt = cpu_to_be32(len);
	} else if ((data ^ (data + len)) & 0x4000) {
		unsigned long b2, s1, s2;

		b2 = (data | 0x3fffUL) + 1UL;
		s1 = b2 - data;
		s2 = data + len - b2;

		desc->cmd    = cpu_to_be32(len | ETXD_INTWHENDONE |
		                           ETXD_B1V | ETXD_B2V);
		desc->bufcnt = cpu_to_be32((s1 << ETXD_B1CNT_SHIFT)
		                           | (s2 << ETXD_B2CNT_SHIFT));
		desc->p1     = cpu_to_be64((0xa5UL << 56) |
                                           (data & TO_PHYS_MASK));
		desc->p2     = cpu_to_be64((0xa5UL << 56) |
		                           (data & TO_PHYS_MASK));
	} else {
		/* Normal sized packet that doesn't cross a page boundary. */
		desc->cmd    = cpu_to_be32(len | ETXD_INTWHENDONE | ETXD_B1V);
		desc->bufcnt = cpu_to_be32(len << ETXD_B1CNT_SHIFT);
		desc->p1     = cpu_to_be64((0xa5UL << 56) |
		                           (data & TO_PHYS_MASK));
	}

	BARRIER();

	dev->trans_start = jiffies;
	ip->tx_skbs[produce] = skb;			/* Remember skb */
	produce = (produce + 1) & 127;
	ip->tx_pi = produce;
	ioc3->etpir = produce << 7;			/* Fire ... */

	ip->txqlen++;

	if (ip->txqlen > 127)
		netif_stop_queue(dev);

	spin_unlock_irq(&ip->ioc3_lock);

	return 0;
}

static void ioc3_timeout(struct net_device *dev)
{
	struct ioc3_private *ip = dev->priv;

	printk(KERN_ERR "%s: transmit timed out, resetting\n", dev->name);

	ioc3_stop(ip);
	ioc3_init(ip);
	ioc3_mii_init(ip);

	dev->trans_start = jiffies;
	netif_wake_queue(dev);
}

/*
 * Given a multicast ethernet address, this routine calculates the
 * address's bit index in the logical address filter mask
 */

static inline unsigned int
ioc3_hash(const unsigned char *addr)
{
	unsigned int temp = 0;
	unsigned char byte;
	u32 crc;
	int bits;

	crc = ether_crc_le(ETH_ALEN, addr);

	crc &= 0x3f;    /* bit reverse lowest 6 bits for hash index */
	for (bits = 6; --bits >= 0; ) {
		temp <<= 1;
		temp |= (crc & 0x1);
		crc >>= 1;
	}

	return temp;
}


/* We provide both the mii-tools and the ethtool ioctls.  */
static int ioc3_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct ioc3_private *ip = dev->priv;
	struct ethtool_cmd *ep_user = (struct ethtool_cmd *) rq->ifr_data;
	u16 *data = (u16 *)&rq->ifr_data;
	struct ioc3 *ioc3 = ip->regs;
	struct ethtool_cmd ecmd;

	switch (cmd) {
	case SIOCGMIIPHY:	/* Get the address of the PHY in use.  */
		if (ip->phy == -1)
			return -ENODEV;
		data[0] = ip->phy;
		return 0;

	case SIOCGMIIREG: {	/* Read a PHY register.  */
		unsigned int phy = data[0];
		unsigned int reg = data[1];

		if (phy > 0x1f || reg > 0x1f)
			return -EINVAL;

		spin_lock_irq(&ip->ioc3_lock);
		while (ioc3->micr & MICR_BUSY);
		ioc3->micr = (phy << MICR_PHYADDR_SHIFT) | reg | MICR_READTRIG;
		while (ioc3->micr & MICR_BUSY);
		data[3] = (ioc3->midr_r & MIDR_DATA_MASK);
		spin_unlock_irq(&ip->ioc3_lock);

		return 0;

	case SIOCSMIIREG:	/* Write a PHY register.  */
		phy = data[0];
		reg = data[1];

		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		if (phy > 0x1f || reg > 0x1f)
			return -EINVAL;

		spin_lock_irq(&ip->ioc3_lock);
		while (ioc3->micr & MICR_BUSY);
		ioc3->midr_w = data[2];
		ioc3->micr = (phy << MICR_PHYADDR_SHIFT) | reg;
		while (ioc3->micr & MICR_BUSY);
		spin_unlock_irq(&ip->ioc3_lock);

		return 0;
		}
	case SIOCETHTOOL:
		if (copy_from_user(&ecmd, ep_user, sizeof(ecmd)))
			return -EFAULT;

		if (ecmd.cmd == ETHTOOL_GSET) {
			ecmd.supported =
				(SUPPORTED_10baseT_Half |
				 SUPPORTED_10baseT_Full |
				 SUPPORTED_100baseT_Half |
				 SUPPORTED_100baseT_Full | SUPPORTED_Autoneg |
				 SUPPORTED_TP | SUPPORTED_MII);

			ecmd.port = PORT_TP;
			ecmd.transceiver = XCVR_INTERNAL;
			ecmd.phy_address = ip->phy;

			/* Record PHY settings. */
			spin_lock_irq(&ip->ioc3_lock);
			ip->sw_bmcr = mii_read(ip, MII_BMCR);
			ip->sw_lpa = mii_read(ip, MII_LPA);
			spin_unlock_irq(&ip->ioc3_lock);
			if (ip->sw_bmcr & BMCR_ANENABLE) {
				ecmd.autoneg = AUTONEG_ENABLE;
				ecmd.speed = (ip->sw_lpa &
			             (LPA_100HALF | LPA_100FULL)) ?
			             SPEED_100 : SPEED_10;
			if (ecmd.speed == SPEED_100)
				ecmd.duplex = (ip->sw_lpa & (LPA_100FULL)) ?
				              DUPLEX_FULL : DUPLEX_HALF;
			else
				ecmd.duplex = (ip->sw_lpa & (LPA_10FULL)) ?
				              DUPLEX_FULL : DUPLEX_HALF;
			} else {
				ecmd.autoneg = AUTONEG_DISABLE;
				ecmd.speed = (ip->sw_bmcr & BMCR_SPEED100) ?
				             SPEED_100 : SPEED_10;
				ecmd.duplex = (ip->sw_bmcr & BMCR_FULLDPLX) ?
				              DUPLEX_FULL : DUPLEX_HALF;
			}
			if (copy_to_user(ep_user, &ecmd, sizeof(ecmd)))
				return -EFAULT;
			return 0;
		} else if (ecmd.cmd == ETHTOOL_SSET) {
			/* Verify the settings we care about. */
			if (ecmd.autoneg != AUTONEG_ENABLE &&
			    ecmd.autoneg != AUTONEG_DISABLE)
				return -EINVAL;

			if (ecmd.autoneg == AUTONEG_DISABLE &&
			    ((ecmd.speed != SPEED_100 &&
			      ecmd.speed != SPEED_10) ||
			     (ecmd.duplex != DUPLEX_HALF &&
			      ecmd.duplex != DUPLEX_FULL)))
				return -EINVAL;

			/* Ok, do it to it. */
			del_timer(&ip->ioc3_timer);
			spin_lock_irq(&ip->ioc3_lock);
			ioc3_start_auto_negotiation(ip, &ecmd);
			spin_unlock_irq(&ip->ioc3_lock);

			return 0;
		} else
		default:
			return -EOPNOTSUPP;
	}

	return -EOPNOTSUPP;
}

static void ioc3_set_multicast_list(struct net_device *dev)
{
	struct dev_mc_list *dmi = dev->mc_list;
	struct ioc3_private *ip = dev->priv;
	struct ioc3 *ioc3 = ip->regs;
	u64 ehar = 0;
	int i;

	netif_stop_queue(dev);				/* Lock out others. */

	if (dev->flags & IFF_PROMISC) {			/* Set promiscuous.  */
		/* Unconditionally log net taps.  */
		printk(KERN_INFO "%s: Promiscuous mode enabled.\n", dev->name);
		ip->emcr |= EMCR_PROMISC;
		ioc3->emcr = ip->emcr;
		ioc3->emcr;
	} else {
		ip->emcr &= ~EMCR_PROMISC;
		ioc3->emcr = ip->emcr;			/* Clear promiscuous. */
		ioc3->emcr;

		if ((dev->flags & IFF_ALLMULTI) || (dev->mc_count > 64)) {
			/* Too many for hashing to make sense or we want all
			   multicast packets anyway,  so skip computing all the
			   hashes and just accept all packets.  */
			ip->ehar_h = 0xffffffff;
			ip->ehar_l = 0xffffffff;
		} else {
			for (i = 0; i < dev->mc_count; i++) {
				char *addr = dmi->dmi_addr;
				dmi = dmi->next;

				if (!(*addr & 1))
					continue;

				ehar |= (1UL << ioc3_hash(addr));
			}
			ip->ehar_h = ehar >> 32;
			ip->ehar_l = ehar & 0xffffffff;
		}
		ioc3->ehar_h = ip->ehar_h;
		ioc3->ehar_l = ip->ehar_l;
	}

	netif_wake_queue(dev);			/* Let us get going again. */
}

MODULE_AUTHOR("Ralf Baechle <ralf@oss.sgi.com>");
MODULE_DESCRIPTION("SGI IOC3 Ethernet driver");
MODULE_LICENSE("GPL");

module_init(ioc3_init_module);
module_exit(ioc3_cleanup_module);
