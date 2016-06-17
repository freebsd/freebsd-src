/* via-rhine.c: A Linux Ethernet device driver for VIA Rhine family chips. */
/*
	Written 1998-2001 by Donald Becker.

	Current Maintainer: Roger Luethi <rl@hellgate.ch>

	This software may be used and distributed according to the terms of
	the GNU General Public License (GPL), incorporated herein by reference.
	Drivers based on or derived from this code fall under the GPL and must
	retain the authorship, copyright and license notice.  This file is not
	a complete program and may only be used when the entire operating
	system is licensed under the GPL.

	This driver is designed for the VIA VT86C100A Rhine-I.
	It also works with the Rhine-II (6102) and Rhine-III (6105/6105L/6105LOM
	and management NIC 6105M).

	The author may be reached as becker@scyld.com, or C/O
	Scyld Computing Corporation
	410 Severn Ave., Suite 210
	Annapolis MD 21403


	This driver contains some changes from the original Donald Becker
	version. He may or may not be interested in bug reports on this
	code. You can find his versions at:
	http://www.scyld.com/network/via-rhine.html


	Linux kernel version history:
	
	LK1.1.0:
	- Jeff Garzik: softnet 'n stuff
	
	LK1.1.1:
	- Justin Guyett: softnet and locking fixes
	- Jeff Garzik: use PCI interface

	LK1.1.2:
	- Urban Widmark: minor cleanups, merges from Becker 1.03a/1.04 versions

	LK1.1.3:
	- Urban Widmark: use PCI DMA interface (with thanks to the eepro100.c
			 code) update "Theory of Operation" with
			 softnet/locking changes
	- Dave Miller: PCI DMA and endian fixups
	- Jeff Garzik: MOD_xxx race fixes, updated PCI resource allocation

	LK1.1.4:
	- Urban Widmark: fix gcc 2.95.2 problem and
	                 remove writel's to fixed address 0x7c

	LK1.1.5:
	- Urban Widmark: mdio locking, bounce buffer changes
	                 merges from Beckers 1.05 version
	                 added netif_running_on/off support

	LK1.1.6:
	- Urban Widmark: merges from Beckers 1.08b version (VT6102 + mdio)
	                 set netif_running_on/off on startup, del_timer_sync
	
	LK1.1.7:
	- Manfred Spraul: added reset into tx_timeout

	LK1.1.9:
	- Urban Widmark: merges from Beckers 1.10 version
	                 (media selection + eeprom reload)
	- David Vrabel:  merges from D-Link "1.11" version
	                 (disable WOL and PME on startup)

	LK1.1.10:
	- Manfred Spraul: use "singlecopy" for unaligned buffers
	                  don't allocate bounce buffers for !ReqTxAlign cards

	LK1.1.11:
	- David Woodhouse: Set dev->base_addr before the first time we call
					   wait_for_reset(). It's a lot happier that way.
					   Free np->tx_bufs only if we actually allocated it.

	LK1.1.12:
	- Martin Eriksson: Allow Memory-Mapped IO to be enabled.

	LK1.1.13 (jgarzik):
	- Add ethtool support
	- Replace some MII-related magic numbers with constants
	
	LK1.1.14 (Ivan G.):
 	- fixes comments for Rhine-III
	- removes W_MAX_TIMEOUT (unused)
	- adds HasDavicomPhy for Rhine-I (basis: linuxfet driver; my card
	  is R-I and has Davicom chip, flag is referenced in kernel driver)
	- sends chip_id as a parameter to wait_for_reset since np is not
	  initialized on first call
	- changes mmio "else if (chip_id==VT6102)" to "else" so it will work
	  for Rhine-III's (documentation says same bit is correct)		
	- transmit frame queue message is off by one - fixed
	- adds IntrNormalSummary to "Something Wicked" exclusion list
	  so normal interrupts will not trigger the message (src: Donald Becker)
 	(Roger Luethi)
 	- show confused chip where to continue after Tx error
 	- location of collision counter is chip specific
 	- allow selecting backoff algorithm (module parameter)

	LK1.1.15 (jgarzik):
	- Use new MII lib helper generic_mii_ioctl

	LK1.1.16 (Roger Luethi)
	- Etherleak fix
	- Handle Tx buffer underrun
	- Fix bugs in full duplex handling
	- New reset code uses "force reset" cmd on Rhine-II
	- Various clean ups

	LK1.1.17 (Roger Luethi)
	- Fix race in via_rhine_start_tx()
	- On errors, wait for Tx engine to turn off before scavenging
	- Handle Tx descriptor write-back race on Rhine-II
	- Force flushing for PCI posted writes
	- More reset code changes

	LK1.1.18 (Roger Luethi)
	- No filtering multicast in promisc mode (Edward Peng)
	- Fix for Rhine-I Tx timeouts

	LK1.1.19 (Roger Luethi)
	- Increase Tx threshold for unspecified errors

*/

#define DRV_NAME	"via-rhine"
#define DRV_VERSION	"1.1.19"
#define DRV_RELDATE	"July-12-2003"


/* A few user-configurable values.
   These may be modified when a driver module is loaded. */

static int debug = 1;			/* 1 normal messages, 0 quiet .. 7 verbose. */
static int max_interrupt_work = 20;

/* Set the copy breakpoint for the copy-only-tiny-frames scheme.
   Setting to > 1518 effectively disables this feature. */
static int rx_copybreak;

/* Select a backoff algorithm (Ethernet capture effect) */
static int backoff;

/* Used to pass the media type, etc.
   Both 'options[]' and 'full_duplex[]' should exist for driver
   interoperability.
   The media type is usually passed in 'options[]'.
   The default is autonegotiation for speed and duplex.
     This should rarely be overridden.
   Use option values 0x10/0x20 for 10Mbps, 0x100,0x200 for 100Mbps.
   Use option values 0x10 and 0x100 for forcing half duplex fixed speed.
   Use option values 0x20 and 0x200 for forcing full duplex operation.
*/
#define MAX_UNITS 8		/* More are supported, limit only on options */
static int options[MAX_UNITS] = {-1, -1, -1, -1, -1, -1, -1, -1};
static int full_duplex[MAX_UNITS] = {-1, -1, -1, -1, -1, -1, -1, -1};

/* Maximum number of multicast addresses to filter (vs. rx-all-multicast).
   The Rhine has a 64 element 8390-like hash table.  */
static const int multicast_filter_limit = 32;


/* Operational parameters that are set at compile time. */

/* Keep the ring sizes a power of two for compile efficiency.
   The compiler will convert <unsigned>'%'<2^N> into a bit mask.
   Making the Tx ring too large decreases the effectiveness of channel
   bonding and packet priority.
   There are no ill effects from too-large receive rings. */
#define TX_RING_SIZE	16
#define TX_QUEUE_LEN	10		/* Limit ring entries actually used.  */
#define RX_RING_SIZE	16


/* Operational parameters that usually are not changed. */

/* Time in jiffies before concluding the transmitter is hung. */
#define TX_TIMEOUT  (2*HZ)

#define PKT_BUF_SZ		1536			/* Size of each temporary Rx buffer.*/

#if !defined(__OPTIMIZE__)  ||  !defined(__KERNEL__)
#warning  You must compile this file with the correct options!
#warning  See the last lines of the source file.
#error  You must compile this driver with "-O".
#endif

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/crc32.h>
#include <asm/processor.h>		/* Processor type for cache alignment. */
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>

/* These identify the driver base version and may not be removed. */
static char version[] __devinitdata =
KERN_INFO DRV_NAME ".c:v1.10-LK" DRV_VERSION "  " DRV_RELDATE "  Written by Donald Becker\n"
KERN_INFO "  http://www.scyld.com/network/via-rhine.html\n";

static char shortname[] = DRV_NAME;


/* This driver was written to use PCI memory space, however most versions
   of the Rhine only work correctly with I/O space accesses. */
#ifdef CONFIG_VIA_RHINE_MMIO
#define USE_MEM
#else
#define USE_IO
#undef readb
#undef readw
#undef readl
#undef writeb
#undef writew
#undef writel
#define readb inb
#define readw inw
#define readl inl
#define writeb outb
#define writew outw
#define writel outl
#endif

MODULE_AUTHOR("Donald Becker <becker@scyld.com>");
MODULE_DESCRIPTION("VIA Rhine PCI Fast Ethernet driver");
MODULE_LICENSE("GPL");

MODULE_PARM(max_interrupt_work, "i");
MODULE_PARM(debug, "i");
MODULE_PARM(rx_copybreak, "i");
MODULE_PARM(backoff, "i");
MODULE_PARM(options, "1-" __MODULE_STRING(MAX_UNITS) "i");
MODULE_PARM(full_duplex, "1-" __MODULE_STRING(MAX_UNITS) "i");
MODULE_PARM_DESC(max_interrupt_work, "VIA Rhine maximum events handled per interrupt");
MODULE_PARM_DESC(debug, "VIA Rhine debug level (0-7)");
MODULE_PARM_DESC(rx_copybreak, "VIA Rhine copy breakpoint for copy-only-tiny-frames");
MODULE_PARM_DESC(backoff, "VIA Rhine: Bits 0-3: backoff algorithm");
MODULE_PARM_DESC(options, "VIA Rhine: Bits 0-3: media type, bit 17: full duplex");
MODULE_PARM_DESC(full_duplex, "VIA Rhine full duplex setting(s) (1)");

/*
				Theory of Operation

I. Board Compatibility

This driver is designed for the VIA 86c100A Rhine-II PCI Fast Ethernet
controller.

II. Board-specific settings

Boards with this chip are functional only in a bus-master PCI slot.

Many operational settings are loaded from the EEPROM to the Config word at
offset 0x78. For most of these settings, this driver assumes that they are
correct.
If this driver is compiled to use PCI memory space operations the EEPROM
must be configured to enable memory ops.

III. Driver operation

IIIa. Ring buffers

This driver uses two statically allocated fixed-size descriptor lists
formed into rings by a branch from the final descriptor to the beginning of
the list.  The ring sizes are set at compile time by RX/TX_RING_SIZE.

IIIb/c. Transmit/Receive Structure

This driver attempts to use a zero-copy receive and transmit scheme.

Alas, all data buffers are required to start on a 32 bit boundary, so
the driver must often copy transmit packets into bounce buffers.

The driver allocates full frame size skbuffs for the Rx ring buffers at
open() time and passes the skb->data field to the chip as receive data
buffers.  When an incoming frame is less than RX_COPYBREAK bytes long,
a fresh skbuff is allocated and the frame is copied to the new skbuff.
When the incoming frame is larger, the skbuff is passed directly up the
protocol stack.  Buffers consumed this way are replaced by newly allocated
skbuffs in the last phase of via_rhine_rx().

The RX_COPYBREAK value is chosen to trade-off the memory wasted by
using a full-sized skbuff for small frames vs. the copying costs of larger
frames.  New boards are typically used in generously configured machines
and the underfilled buffers have negligible impact compared to the benefit of
a single allocation size, so the default value of zero results in never
copying packets.  When copying is done, the cost is usually mitigated by using
a combined copy/checksum routine.  Copying also preloads the cache, which is
most useful with small frames.

Since the VIA chips are only able to transfer data to buffers on 32 bit
boundaries, the IP header at offset 14 in an ethernet frame isn't
longword aligned for further processing.  Copying these unaligned buffers
has the beneficial effect of 16-byte aligning the IP header.

IIId. Synchronization

The driver runs as two independent, single-threaded flows of control.  One
is the send-packet routine, which enforces single-threaded use by the
dev->priv->lock spinlock. The other thread is the interrupt handler, which 
is single threaded by the hardware and interrupt handling software.

The send packet thread has partial control over the Tx ring. It locks the 
dev->priv->lock whenever it's queuing a Tx packet. If the next slot in the ring
is not available it stops the transmit queue by calling netif_stop_queue.

The interrupt handler has exclusive control over the Rx ring and records stats
from the Tx ring.  After reaping the stats, it marks the Tx queue entry as
empty by incrementing the dirty_tx mark. If at least half of the entries in
the Rx ring are available the transmit queue is woken up if it was stopped.

IV. Notes

IVb. References

Preliminary VT86C100A manual from http://www.via.com.tw/
http://www.scyld.com/expert/100mbps.html
http://www.scyld.com/expert/NWay.html
ftp://ftp.via.com.tw/public/lan/Products/NIC/VT86C100A/Datasheet/VT86C100A03.pdf
ftp://ftp.via.com.tw/public/lan/Products/NIC/VT6102/Datasheet/VT6102_021.PDF


IVc. Errata

The VT86C100A manual is not reliable information.
The 3043 chip does not handle unaligned transmit or receive buffers, resulting
in significant performance degradation for bounce buffer copies on transmit
and unaligned IP headers on receive.
The chip does not pad to minimum transmit length.

*/


/* This table drives the PCI probe routines.  It's mostly boilerplate in all
   of the drivers, and will likely be provided by some future kernel.
   Note the matching code -- the first table entry matchs all 56** cards but
   second only the 1234 card.
*/

enum pci_flags_bit {
	PCI_USES_IO=1, PCI_USES_MEM=2, PCI_USES_MASTER=4,
	PCI_ADDR0=0x10<<0, PCI_ADDR1=0x10<<1, PCI_ADDR2=0x10<<2, PCI_ADDR3=0x10<<3,
};

enum via_rhine_chips {
	VT86C100A = 0,
	VT6102,
	VT6105,
	VT6105M
};

struct via_rhine_chip_info {
	const char *name;
	u16 pci_flags;
	int io_size;
	int drv_flags;
};


enum chip_capability_flags {
	CanHaveMII=1, HasESIPhy=2, HasDavicomPhy=4,
	ReqTxAlign=0x10, HasWOL=0x20, };

#ifdef USE_MEM
#define RHINE_IOTYPE (PCI_USES_MEM | PCI_USES_MASTER | PCI_ADDR1)
#else
#define RHINE_IOTYPE (PCI_USES_IO  | PCI_USES_MASTER | PCI_ADDR0)
#endif
/* Beware of PCI posted writes */
#define IOSYNC	do { readb(dev->base_addr + StationAddr); } while (0)

/* directly indexed by enum via_rhine_chips, above */
static struct via_rhine_chip_info via_rhine_chip_info[] __devinitdata =
{
	{ "VIA VT86C100A Rhine", RHINE_IOTYPE, 128,
	  CanHaveMII | ReqTxAlign | HasDavicomPhy },
	{ "VIA VT6102 Rhine-II", RHINE_IOTYPE, 256,
	  CanHaveMII | HasWOL },
	{ "VIA VT6105 Rhine-III", RHINE_IOTYPE, 256,
	  CanHaveMII | HasWOL },
	{ "VIA VT6105M Rhine-III", RHINE_IOTYPE, 256,
	  CanHaveMII | HasWOL },
};

static struct pci_device_id via_rhine_pci_tbl[] =
{
	{0x1106, 0x3043, PCI_ANY_ID, PCI_ANY_ID, 0, 0, VT86C100A},
	{0x1106, 0x3065, PCI_ANY_ID, PCI_ANY_ID, 0, 0, VT6102},
	{0x1106, 0x3106, PCI_ANY_ID, PCI_ANY_ID, 0, 0, VT6105}, /* 6105{,L,LOM} */
	{0x1106, 0x3053, PCI_ANY_ID, PCI_ANY_ID, 0, 0, VT6105M},
	{0,}			/* terminate list */
};
MODULE_DEVICE_TABLE(pci, via_rhine_pci_tbl);


/* Offsets to the device registers. */
enum register_offsets {
	StationAddr=0x00, RxConfig=0x06, TxConfig=0x07, ChipCmd=0x08,
	IntrStatus=0x0C, IntrEnable=0x0E,
	MulticastFilter0=0x10, MulticastFilter1=0x14,
	RxRingPtr=0x18, TxRingPtr=0x1C, GFIFOTest=0x54,
	MIIPhyAddr=0x6C, MIIStatus=0x6D, PCIBusConfig=0x6E,
	MIICmd=0x70, MIIRegAddr=0x71, MIIData=0x72, MACRegEEcsr=0x74,
	ConfigA=0x78, ConfigB=0x79, ConfigC=0x7A, ConfigD=0x7B,
	RxMissed=0x7C, RxCRCErrs=0x7E, MiscCmd=0x81,
	StickyHW=0x83, IntrStatus2=0x84, WOLcrClr=0xA4, WOLcgClr=0xA7,
	PwrcsrClr=0xAC,
};

/* Bits in ConfigD */
enum backoff_bits {
	BackOptional=0x01, BackModify=0x02,
	BackCaptureEffect=0x04, BackRandom=0x08
};

#ifdef USE_MEM
/* Registers we check that mmio and reg are the same. */
int mmio_verify_registers[] = {
	RxConfig, TxConfig, IntrEnable, ConfigA, ConfigB, ConfigC, ConfigD,
	0
};
#endif

/* Bits in the interrupt status/mask registers. */
enum intr_status_bits {
	IntrRxDone=0x0001, IntrRxErr=0x0004, IntrRxEmpty=0x0020,
	IntrTxDone=0x0002, IntrTxError=0x0008, IntrTxUnderrun=0x0210,
	IntrPCIErr=0x0040,
	IntrStatsMax=0x0080, IntrRxEarly=0x0100,
	IntrRxOverflow=0x0400, IntrRxDropped=0x0800, IntrRxNoBuf=0x1000,
	IntrTxAborted=0x2000, IntrLinkChange=0x4000,
	IntrRxWakeUp=0x8000,
	IntrNormalSummary=0x0003, IntrAbnormalSummary=0xC260,
	IntrTxDescRace=0x080000,	/* mapped from IntrStatus2 */
	IntrTxErrSummary=0x082218,
};

/* The Rx and Tx buffer descriptors. */
struct rx_desc {
	s32 rx_status;
	u32 desc_length; /* Chain flag, Buffer/frame length */
	u32 addr;
	u32 next_desc;
};
struct tx_desc {
	s32 tx_status;
	u32 desc_length; /* Chain flag, Tx Config, Frame length */
	u32 addr;
	u32 next_desc;
};

/* Initial value for tx_desc.desc_length, Buffer size goes to bits 0-10 */
#define TXDESC 0x00e08000

enum rx_status_bits {
	RxOK=0x8000, RxWholePkt=0x0300, RxErr=0x008F
};

/* Bits in *_desc.*_status */
enum desc_status_bits {
	DescOwn=0x80000000
};

/* Bits in ChipCmd. */
enum chip_cmd_bits {
	CmdInit=0x0001, CmdStart=0x0002, CmdStop=0x0004, CmdRxOn=0x0008,
	CmdTxOn=0x0010, CmdTxDemand=0x0020, CmdRxDemand=0x0040,
	CmdEarlyRx=0x0100, CmdEarlyTx=0x0200, CmdFDuplex=0x0400,
	CmdNoTxPoll=0x0800, CmdReset=0x8000,
};

#define MAX_MII_CNT	4
struct netdev_private {
	/* Descriptor rings */
	struct rx_desc *rx_ring;
	struct tx_desc *tx_ring;
	dma_addr_t rx_ring_dma;
	dma_addr_t tx_ring_dma;

	/* The addresses of receive-in-place skbuffs. */
	struct sk_buff *rx_skbuff[RX_RING_SIZE];
	dma_addr_t rx_skbuff_dma[RX_RING_SIZE];

	/* The saved address of a sent-in-place packet/buffer, for later free(). */
	struct sk_buff *tx_skbuff[TX_RING_SIZE];
	dma_addr_t tx_skbuff_dma[TX_RING_SIZE];

	/* Tx bounce buffers */
	unsigned char *tx_buf[TX_RING_SIZE];
	unsigned char *tx_bufs;
	dma_addr_t tx_bufs_dma;

	struct pci_dev *pdev;
	struct net_device_stats stats;
	struct timer_list timer;	/* Media monitoring timer. */
	spinlock_t lock;

	/* Frequently used values: keep some adjacent for cache effect. */
	int chip_id, drv_flags;
	struct rx_desc *rx_head_desc;
	unsigned int cur_rx, dirty_rx;		/* Producer/consumer ring indices */
	unsigned int cur_tx, dirty_tx;
	unsigned int rx_buf_sz;				/* Based on MTU+slack. */
	u16 chip_cmd;						/* Current setting for ChipCmd */

	/* These values are keep track of the transceiver/media in use. */
	unsigned int default_port:4;		/* Last dev->if_port value. */
	u8 tx_thresh, rx_thresh;

	/* MII transceiver section. */
	unsigned char phys[MAX_MII_CNT];			/* MII device addresses. */
	unsigned int mii_cnt;			/* number of MIIs found, but only the first one is used */
	u16 mii_status;						/* last read MII status */
	struct mii_if_info mii_if;
};

static int  mdio_read(struct net_device *dev, int phy_id, int location);
static void mdio_write(struct net_device *dev, int phy_id, int location, int value);
static int  via_rhine_open(struct net_device *dev);
static void via_rhine_check_duplex(struct net_device *dev);
static void via_rhine_timer(unsigned long data);
static void via_rhine_tx_timeout(struct net_device *dev);
static int  via_rhine_start_tx(struct sk_buff *skb, struct net_device *dev);
static irqreturn_t via_rhine_interrupt(int irq, void *dev_instance, struct pt_regs *regs);
static void via_rhine_tx(struct net_device *dev);
static void via_rhine_rx(struct net_device *dev);
static void via_rhine_error(struct net_device *dev, int intr_status);
static void via_rhine_set_rx_mode(struct net_device *dev);
static struct net_device_stats *via_rhine_get_stats(struct net_device *dev);
static int netdev_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
static struct ethtool_ops netdev_ethtool_ops;
static int  via_rhine_close(struct net_device *dev);

static inline u32 get_intr_status(struct net_device *dev)
{
	long ioaddr = dev->base_addr;
	struct netdev_private *np = dev->priv;
	u32 intr_status;

	intr_status = readw(ioaddr + IntrStatus);
	/* On Rhine-II, Bit 3 indicates Tx descriptor write-back race. */
	if (np->chip_id == VT6102)
		intr_status |= readb(ioaddr + IntrStatus2) << 16;
	return intr_status;
}

static void wait_for_reset(struct net_device *dev, int chip_id, char *name)
{
	long ioaddr = dev->base_addr;
	int boguscnt = 20;

	IOSYNC;

	if (readw(ioaddr + ChipCmd) & CmdReset) {
		printk(KERN_INFO "%s: Reset not complete yet. "
			"Trying harder.\n", name);

		/* Rhine-II needs to be forced sometimes */
		if (chip_id == VT6102)
			writeb(0x40, ioaddr + MiscCmd);

		/* VT86C100A may need long delay after reset (dlink) */
		/* Seen on Rhine-II as well (rl) */
		while ((readw(ioaddr + ChipCmd) & CmdReset) && --boguscnt)
			udelay(5);

	}

	if (debug > 1)
		printk(KERN_INFO "%s: Reset %s.\n", name,
			boguscnt ? "succeeded" : "failed");
}

#ifdef USE_MEM
static void __devinit enable_mmio(long ioaddr, int chip_id)
{
	int n;
	if (chip_id == VT86C100A) {
		/* More recent docs say that this bit is reserved ... */
		n = inb(ioaddr + ConfigA) | 0x20;
		outb(n, ioaddr + ConfigA);
	} else {
		n = inb(ioaddr + ConfigD) | 0x80;
		outb(n, ioaddr + ConfigD);
	}
}
#endif

static void __devinit reload_eeprom(long ioaddr)
{
	int i;
	outb(0x20, ioaddr + MACRegEEcsr);
	/* Typically 2 cycles to reload. */
	for (i = 0; i < 150; i++)
		if (! (inb(ioaddr + MACRegEEcsr) & 0x20))
			break;
}

static int __devinit via_rhine_init_one (struct pci_dev *pdev,
					 const struct pci_device_id *ent)
{
	struct net_device *dev;
	struct netdev_private *np;
	int i, option;
	int chip_id = (int) ent->driver_data;
	static int card_idx = -1;
	long ioaddr;
	long memaddr;
	int io_size;
	int pci_flags;
#ifdef USE_MEM
	long ioaddr0;
#endif
	
/* when built into the kernel, we only print version if device is found */
#ifndef MODULE
	static int printed_version;
	if (!printed_version++)
		printk(version);
#endif

	card_idx++;
	option = card_idx < MAX_UNITS ? options[card_idx] : 0;
	io_size = via_rhine_chip_info[chip_id].io_size;
	pci_flags = via_rhine_chip_info[chip_id].pci_flags;

	if (pci_enable_device (pdev))
		goto err_out;

	/* this should always be supported */
	if (pci_set_dma_mask(pdev, 0xffffffff)) {
		printk(KERN_ERR "32-bit PCI DMA addresses not supported by the card!?\n");
		goto err_out;
	}
	
	/* sanity check */
	if ((pci_resource_len (pdev, 0) < io_size) ||
	    (pci_resource_len (pdev, 1) < io_size)) {
		printk (KERN_ERR "Insufficient PCI resources, aborting\n");
		goto err_out;
	}

	ioaddr = pci_resource_start (pdev, 0);
	memaddr = pci_resource_start (pdev, 1);

	if (pci_flags & PCI_USES_MASTER)
		pci_set_master (pdev);

	dev = alloc_etherdev(sizeof(*np));
	if (dev == NULL) {
		printk (KERN_ERR "init_ethernet failed for card #%d\n", card_idx);
		goto err_out;
	}
	SET_MODULE_OWNER(dev);
	
	if (pci_request_regions(pdev, shortname))
		goto err_out_free_netdev;

#ifdef USE_MEM
	ioaddr0 = ioaddr;
	enable_mmio(ioaddr0, chip_id);

	ioaddr = (long) ioremap (memaddr, io_size);
	if (!ioaddr) {
		printk (KERN_ERR "ioremap failed for device %s, region 0x%X @ 0x%lX\n",
				pci_name(pdev), io_size, memaddr);
		goto err_out_free_res;
	}

	/* Check that selected MMIO registers match the PIO ones */
	i = 0;
	while (mmio_verify_registers[i]) {
		int reg = mmio_verify_registers[i++];
		unsigned char a = inb(ioaddr0+reg);
		unsigned char b = readb(ioaddr+reg);
		if (a != b) {
			printk (KERN_ERR "MMIO do not match PIO [%02x] (%02x != %02x)\n",
					reg, a, b);
			goto err_out_unmap;
		}
	}
#endif

	/* D-Link provided reset code (with comment additions) */
	if (via_rhine_chip_info[chip_id].drv_flags & HasWOL) {
		unsigned char byOrgValue;

		/* clear sticky bit before reset & read ethernet address */
		byOrgValue = readb(ioaddr + StickyHW);
		byOrgValue = byOrgValue & 0xFC;
		writeb(byOrgValue, ioaddr + StickyHW);

		/* (bits written are cleared?) */
		/* disable force PME-enable */
		writeb(0x80, ioaddr + WOLcgClr);
		/* disable power-event config bit */
		writeb(0xFF, ioaddr + WOLcrClr);
		/* clear power status (undocumented in vt6102 docs?) */
		writeb(0xFF, ioaddr + PwrcsrClr);
	}

	/* Reset the chip to erase previous misconfiguration. */
	writew(CmdReset, ioaddr + ChipCmd);

	dev->base_addr = ioaddr;
	wait_for_reset(dev, chip_id, shortname);

	/* Reload the station address from the EEPROM. */
#ifdef USE_IO
	reload_eeprom(ioaddr);
#else
	reload_eeprom(ioaddr0);
	/* Reloading from eeprom overwrites cfgA-D, so we must re-enable MMIO.
	   If reload_eeprom() was done first this could be avoided, but it is
	   not known if that still works with the "win98-reboot" problem. */
	enable_mmio(ioaddr0, chip_id);
#endif

	for (i = 0; i < 6; i++)
		dev->dev_addr[i] = readb(ioaddr + StationAddr + i);

	if (!is_valid_ether_addr(dev->dev_addr)) {
		printk(KERN_ERR "Invalid MAC address for card #%d\n", card_idx);
		goto err_out_unmap;
	}

	if (chip_id == VT6102) {
		/*
		 * for 3065D, EEPROM reloaded will cause bit 0 in MAC_REG_CFGA
		 * turned on.  it makes MAC receive magic packet
		 * automatically. So, we turn it off. (D-Link)
		 */
		writeb(readb(ioaddr + ConfigA) & 0xFE, ioaddr + ConfigA);
	}

	/* Select backoff algorithm */
	if (backoff)
		writeb(readb(ioaddr + ConfigD) & (0xF0 | backoff),
			ioaddr + ConfigD);

	dev->irq = pdev->irq;

	np = dev->priv;
	spin_lock_init (&np->lock);
	np->chip_id = chip_id;
	np->drv_flags = via_rhine_chip_info[chip_id].drv_flags;
	np->pdev = pdev;
	np->mii_if.dev = dev;
	np->mii_if.mdio_read = mdio_read;
	np->mii_if.mdio_write = mdio_write;
	np->mii_if.phy_id_mask = 0x1f;
	np->mii_if.reg_num_mask = 0x1f;

	if (dev->mem_start)
		option = dev->mem_start;

	/* The chip-specific entries in the device structure. */
	dev->open = via_rhine_open;
	dev->hard_start_xmit = via_rhine_start_tx;
	dev->stop = via_rhine_close;
	dev->get_stats = via_rhine_get_stats;
	dev->set_multicast_list = via_rhine_set_rx_mode;
	dev->do_ioctl = netdev_ioctl;
	dev->ethtool_ops = &netdev_ethtool_ops;
	dev->tx_timeout = via_rhine_tx_timeout;
	dev->watchdog_timeo = TX_TIMEOUT;
	if (np->drv_flags & ReqTxAlign)
		dev->features |= NETIF_F_SG|NETIF_F_HW_CSUM;

	/* dev->name not defined before register_netdev()! */
	i = register_netdev(dev);
	if (i)
		goto err_out_unmap;

	/* The lower four bits are the media type. */
	if (option > 0) {
		if (option & 0x220)
			np->mii_if.full_duplex = 1;
		np->default_port = option & 15;
	}
	if (card_idx < MAX_UNITS  &&  full_duplex[card_idx] > 0)
		np->mii_if.full_duplex = 1;

	if (np->mii_if.full_duplex) {
		printk(KERN_INFO "%s: Set to forced full duplex, autonegotiation"
			   " disabled.\n", dev->name);
		np->mii_if.force_media = 1;
	}

	printk(KERN_INFO "%s: %s at 0x%lx, ",
		   dev->name, via_rhine_chip_info[chip_id].name,
		   (pci_flags & PCI_USES_IO) ? ioaddr : memaddr);

	for (i = 0; i < 5; i++)
			printk("%2.2x:", dev->dev_addr[i]);
	printk("%2.2x, IRQ %d.\n", dev->dev_addr[i], pdev->irq);

	pci_set_drvdata(pdev, dev);

	if (np->drv_flags & CanHaveMII) {
		int phy, phy_idx = 0;
		np->phys[0] = 1;		/* Standard for this chip. */
		for (phy = 1; phy < 32 && phy_idx < MAX_MII_CNT; phy++) {
			int mii_status = mdio_read(dev, phy, 1);
			if (mii_status != 0xffff  &&  mii_status != 0x0000) {
				np->phys[phy_idx++] = phy;
				np->mii_if.advertising = mdio_read(dev, phy, 4);
				printk(KERN_INFO "%s: MII PHY found at address %d, status "
					   "0x%4.4x advertising %4.4x Link %4.4x.\n",
					   dev->name, phy, mii_status, np->mii_if.advertising,
					   mdio_read(dev, phy, 5));

				/* set IFF_RUNNING */
				if (mii_status & BMSR_LSTATUS)
					netif_carrier_on(dev);
				else
					netif_carrier_off(dev);
			}
		}
		np->mii_cnt = phy_idx;
		np->mii_if.phy_id = np->phys[0];
	}

	/* Allow forcing the media type. */
	if (option > 0) {
		if (option & 0x220)
			np->mii_if.full_duplex = 1;
		np->default_port = option & 0x3ff;
		if (np->default_port & 0x330) {
			/* FIXME: shouldn't someone check this variable? */
			/* np->medialock = 1; */
			printk(KERN_INFO "  Forcing %dMbs %s-duplex operation.\n",
				   (option & 0x300 ? 100 : 10),
				   (option & 0x220 ? "full" : "half"));
			if (np->mii_cnt)
				mdio_write(dev, np->phys[0], MII_BMCR,
						   ((option & 0x300) ? 0x2000 : 0) |  /* 100mbps? */
						   ((option & 0x220) ? 0x0100 : 0));  /* Full duplex? */
		}
	}

	return 0;

err_out_unmap:
#ifdef USE_MEM
	iounmap((void *)ioaddr);
err_out_free_res:
#endif
	pci_release_regions(pdev);
err_out_free_netdev:
	kfree (dev);
err_out:
	return -ENODEV;
}

static int alloc_ring(struct net_device* dev)
{
	struct netdev_private *np = dev->priv;
	void *ring;
	dma_addr_t ring_dma;

	ring = pci_alloc_consistent(np->pdev, 
				    RX_RING_SIZE * sizeof(struct rx_desc) +
				    TX_RING_SIZE * sizeof(struct tx_desc),
				    &ring_dma);
	if (!ring) {
		printk(KERN_ERR "Could not allocate DMA memory.\n");
		return -ENOMEM;
	}
	if (np->drv_flags & ReqTxAlign) {
		np->tx_bufs = pci_alloc_consistent(np->pdev, PKT_BUF_SZ * TX_RING_SIZE,
								   &np->tx_bufs_dma);
		if (np->tx_bufs == NULL) {
			pci_free_consistent(np->pdev, 
				    RX_RING_SIZE * sizeof(struct rx_desc) +
				    TX_RING_SIZE * sizeof(struct tx_desc),
				    ring, ring_dma);
			return -ENOMEM;
		}
	}

	np->rx_ring = ring;
	np->tx_ring = ring + RX_RING_SIZE * sizeof(struct rx_desc);
	np->rx_ring_dma = ring_dma;
	np->tx_ring_dma = ring_dma + RX_RING_SIZE * sizeof(struct rx_desc);

	return 0;
}

void free_ring(struct net_device* dev)
{
	struct netdev_private *np = dev->priv;

	pci_free_consistent(np->pdev, 
			    RX_RING_SIZE * sizeof(struct rx_desc) +
			    TX_RING_SIZE * sizeof(struct tx_desc),
			    np->rx_ring, np->rx_ring_dma);
	np->tx_ring = NULL;

	if (np->tx_bufs)
		pci_free_consistent(np->pdev, PKT_BUF_SZ * TX_RING_SIZE,
							np->tx_bufs, np->tx_bufs_dma);

	np->tx_bufs = NULL;

}

static void alloc_rbufs(struct net_device *dev)
{
	struct netdev_private *np = dev->priv;
	dma_addr_t next;
	int i;

	np->dirty_rx = np->cur_rx = 0;

	np->rx_buf_sz = (dev->mtu <= 1500 ? PKT_BUF_SZ : dev->mtu + 32);
	np->rx_head_desc = &np->rx_ring[0];
	next = np->rx_ring_dma;
	
	/* Init the ring entries */
	for (i = 0; i < RX_RING_SIZE; i++) {
		np->rx_ring[i].rx_status = 0;
		np->rx_ring[i].desc_length = cpu_to_le32(np->rx_buf_sz);
		next += sizeof(struct rx_desc);
		np->rx_ring[i].next_desc = cpu_to_le32(next);
		np->rx_skbuff[i] = 0;
	}
	/* Mark the last entry as wrapping the ring. */
	np->rx_ring[i-1].next_desc = cpu_to_le32(np->rx_ring_dma);

	/* Fill in the Rx buffers.  Handle allocation failure gracefully. */
	for (i = 0; i < RX_RING_SIZE; i++) {
		struct sk_buff *skb = dev_alloc_skb(np->rx_buf_sz);
		np->rx_skbuff[i] = skb;
		if (skb == NULL)
			break;
		skb->dev = dev;                 /* Mark as being used by this device. */

		np->rx_skbuff_dma[i] =
			pci_map_single(np->pdev, skb->tail, np->rx_buf_sz,
						   PCI_DMA_FROMDEVICE);

		np->rx_ring[i].addr = cpu_to_le32(np->rx_skbuff_dma[i]);
		np->rx_ring[i].rx_status = cpu_to_le32(DescOwn);
	}
	np->dirty_rx = (unsigned int)(i - RX_RING_SIZE);
}

static void free_rbufs(struct net_device* dev)
{
	struct netdev_private *np = dev->priv;
	int i;

	/* Free all the skbuffs in the Rx queue. */
	for (i = 0; i < RX_RING_SIZE; i++) {
		np->rx_ring[i].rx_status = 0;
		np->rx_ring[i].addr = cpu_to_le32(0xBADF00D0); /* An invalid address. */
		if (np->rx_skbuff[i]) {
			pci_unmap_single(np->pdev,
							 np->rx_skbuff_dma[i],
							 np->rx_buf_sz, PCI_DMA_FROMDEVICE);
			dev_kfree_skb(np->rx_skbuff[i]);
		}
		np->rx_skbuff[i] = 0;
	}
}

static void alloc_tbufs(struct net_device* dev)
{
	struct netdev_private *np = dev->priv;
	dma_addr_t next;
	int i;

	np->dirty_tx = np->cur_tx = 0;
	next = np->tx_ring_dma;
	for (i = 0; i < TX_RING_SIZE; i++) {
		np->tx_skbuff[i] = 0;
		np->tx_ring[i].tx_status = 0;
		np->tx_ring[i].desc_length = cpu_to_le32(TXDESC);
		next += sizeof(struct tx_desc);
		np->tx_ring[i].next_desc = cpu_to_le32(next);
		np->tx_buf[i] = &np->tx_bufs[i * PKT_BUF_SZ];
	}
	np->tx_ring[i-1].next_desc = cpu_to_le32(np->tx_ring_dma);

}

static void free_tbufs(struct net_device* dev)
{
	struct netdev_private *np = dev->priv;
	int i;

	for (i = 0; i < TX_RING_SIZE; i++) {
		np->tx_ring[i].tx_status = 0;
		np->tx_ring[i].desc_length = cpu_to_le32(TXDESC);
		np->tx_ring[i].addr = cpu_to_le32(0xBADF00D0); /* An invalid address. */
		if (np->tx_skbuff[i]) {
			if (np->tx_skbuff_dma[i]) {
				pci_unmap_single(np->pdev,
								 np->tx_skbuff_dma[i],
								 np->tx_skbuff[i]->len, PCI_DMA_TODEVICE);
			}
			dev_kfree_skb(np->tx_skbuff[i]);
		}
		np->tx_skbuff[i] = 0;
		np->tx_buf[i] = 0;
	}
}

static void init_registers(struct net_device *dev)
{
	struct netdev_private *np = dev->priv;
	long ioaddr = dev->base_addr;
	int i;

	for (i = 0; i < 6; i++)
		writeb(dev->dev_addr[i], ioaddr + StationAddr + i);

	/* Initialize other registers. */
	writew(0x0006, ioaddr + PCIBusConfig);	/* Tune configuration??? */
	/* Configure initial FIFO thresholds. */
	writeb(0x20, ioaddr + TxConfig);
	np->tx_thresh = 0x20;
	np->rx_thresh = 0x60;			/* Written in via_rhine_set_rx_mode(). */
	np->mii_if.full_duplex = 0;

	if (dev->if_port == 0)
		dev->if_port = np->default_port;

	writel(np->rx_ring_dma, ioaddr + RxRingPtr);
	writel(np->tx_ring_dma, ioaddr + TxRingPtr);

	via_rhine_set_rx_mode(dev);

	/* Enable interrupts by setting the interrupt mask. */
	writew(IntrRxDone | IntrRxErr | IntrRxEmpty| IntrRxOverflow |
		   IntrRxDropped | IntrRxNoBuf | IntrTxAborted |
		   IntrTxDone | IntrTxError | IntrTxUnderrun |
		   IntrPCIErr | IntrStatsMax | IntrLinkChange,
		   ioaddr + IntrEnable);

	np->chip_cmd = CmdStart|CmdTxOn|CmdRxOn|CmdNoTxPoll;
	if (np->mii_if.force_media)
		np->chip_cmd |= CmdFDuplex;
	writew(np->chip_cmd, ioaddr + ChipCmd);

	via_rhine_check_duplex(dev);

	/* The LED outputs of various MII xcvrs should be configured.  */
	/* For NS or Mison phys, turn on bit 1 in register 0x17 */
	/* For ESI phys, turn on bit 7 in register 0x17. */
	mdio_write(dev, np->phys[0], 0x17, mdio_read(dev, np->phys[0], 0x17) |
			   (np->drv_flags & HasESIPhy) ? 0x0080 : 0x0001);
}
/* Read and write over the MII Management Data I/O (MDIO) interface. */

static int mdio_read(struct net_device *dev, int phy_id, int regnum)
{
	long ioaddr = dev->base_addr;
	int boguscnt = 1024;

	/* Wait for a previous command to complete. */
	while ((readb(ioaddr + MIICmd) & 0x60) && --boguscnt > 0)
		;
	writeb(0x00, ioaddr + MIICmd);
	writeb(phy_id, ioaddr + MIIPhyAddr);
	writeb(regnum, ioaddr + MIIRegAddr);
	writeb(0x40, ioaddr + MIICmd);			/* Trigger read */
	boguscnt = 1024;
	while ((readb(ioaddr + MIICmd) & 0x40) && --boguscnt > 0)
		;
	return readw(ioaddr + MIIData);
}

static void mdio_write(struct net_device *dev, int phy_id, int regnum, int value)
{
	struct netdev_private *np = dev->priv;
	long ioaddr = dev->base_addr;
	int boguscnt = 1024;

	if (phy_id == np->phys[0]) {
		switch (regnum) {
		case MII_BMCR:					/* Is user forcing speed/duplex? */
			if (value & 0x9000)			/* Autonegotiation. */
				np->mii_if.force_media = 0;
			else
				np->mii_if.full_duplex = (value & 0x0100) ? 1 : 0;
			break;
		case MII_ADVERTISE:
			np->mii_if.advertising = value;
			break;
		}
	}

	/* Wait for a previous command to complete. */
	while ((readb(ioaddr + MIICmd) & 0x60) && --boguscnt > 0)
		;
	writeb(0x00, ioaddr + MIICmd);
	writeb(phy_id, ioaddr + MIIPhyAddr);
	writeb(regnum, ioaddr + MIIRegAddr);
	writew(value, ioaddr + MIIData);
	writeb(0x20, ioaddr + MIICmd);			/* Trigger write. */
}


static int via_rhine_open(struct net_device *dev)
{
	struct netdev_private *np = dev->priv;
	long ioaddr = dev->base_addr;
	int i;

	/* Reset the chip. */
	writew(CmdReset, ioaddr + ChipCmd);

	i = request_irq(np->pdev->irq, &via_rhine_interrupt, SA_SHIRQ, dev->name, dev);
	if (i)
		return i;

	if (debug > 1)
		printk(KERN_DEBUG "%s: via_rhine_open() irq %d.\n",
			   dev->name, np->pdev->irq);
	
	i = alloc_ring(dev);
	if (i)
		return i;
	alloc_rbufs(dev);
	alloc_tbufs(dev);
	wait_for_reset(dev, np->chip_id, dev->name);
	init_registers(dev);
	if (debug > 2)
		printk(KERN_DEBUG "%s: Done via_rhine_open(), status %4.4x "
			   "MII status: %4.4x.\n",
			   dev->name, readw(ioaddr + ChipCmd),
			   mdio_read(dev, np->phys[0], MII_BMSR));

	netif_start_queue(dev);

	/* Set the timer to check for link beat. */
	init_timer(&np->timer);
	np->timer.expires = jiffies + 2 * HZ/100;
	np->timer.data = (unsigned long)dev;
	np->timer.function = &via_rhine_timer;				/* timer handler */
	add_timer(&np->timer);

	return 0;
}

static void via_rhine_check_duplex(struct net_device *dev)
{
	struct netdev_private *np = dev->priv;
	long ioaddr = dev->base_addr;
	int mii_lpa = mdio_read(dev, np->phys[0], MII_LPA);
	int negotiated = mii_lpa & np->mii_if.advertising;
	int duplex;

	if (np->mii_if.force_media  ||  mii_lpa == 0xffff)
		return;
	duplex = (negotiated & 0x0100) || (negotiated & 0x01C0) == 0x0040;
	if (np->mii_if.full_duplex != duplex) {
		np->mii_if.full_duplex = duplex;
		if (debug)
			printk(KERN_INFO "%s: Setting %s-duplex based on MII #%d link"
				   " partner capability of %4.4x.\n", dev->name,
				   duplex ? "full" : "half", np->phys[0], mii_lpa);
		if (duplex)
			np->chip_cmd |= CmdFDuplex;
		else
			np->chip_cmd &= ~CmdFDuplex;
		writew(np->chip_cmd, ioaddr + ChipCmd);
	}
}


static void via_rhine_timer(unsigned long data)
{
	struct net_device *dev = (struct net_device *)data;
	struct netdev_private *np = dev->priv;
	long ioaddr = dev->base_addr;
	int next_tick = 10*HZ;
	int mii_status;

	if (debug > 3) {
		printk(KERN_DEBUG "%s: VIA Rhine monitor tick, status %4.4x.\n",
			   dev->name, readw(ioaddr + IntrStatus));
	}

	spin_lock_irq (&np->lock);

	via_rhine_check_duplex(dev);

	/* make IFF_RUNNING follow the MII status bit "Link established" */
	mii_status = mdio_read(dev, np->phys[0], MII_BMSR);
	if ( (mii_status & BMSR_LSTATUS) != (np->mii_status & BMSR_LSTATUS) ) {
		if (mii_status & BMSR_LSTATUS)
			netif_carrier_on(dev);
		else
			netif_carrier_off(dev);
	}
	np->mii_status = mii_status;

	spin_unlock_irq (&np->lock);

	np->timer.expires = jiffies + next_tick;
	add_timer(&np->timer);
}


static void via_rhine_tx_timeout (struct net_device *dev)
{
	struct netdev_private *np = dev->priv;
	long ioaddr = dev->base_addr;

	printk (KERN_WARNING "%s: Transmit timed out, status %4.4x, PHY status "
		"%4.4x, resetting...\n",
		dev->name, readw (ioaddr + IntrStatus),
		mdio_read (dev, np->phys[0], MII_BMSR));

	dev->if_port = 0;

	/* protect against concurrent rx interrupts */
	disable_irq(np->pdev->irq);

	spin_lock(&np->lock);

	/* Reset the chip. */
	writew(CmdReset, ioaddr + ChipCmd);

	/* clear all descriptors */
	free_tbufs(dev);
	free_rbufs(dev);
	alloc_tbufs(dev);
	alloc_rbufs(dev);

	/* Reinitialize the hardware. */
	wait_for_reset(dev, np->chip_id, dev->name);
	init_registers(dev);
	
	spin_unlock(&np->lock);
	enable_irq(np->pdev->irq);

	dev->trans_start = jiffies;
	np->stats.tx_errors++;
	netif_wake_queue(dev);
}

static int via_rhine_start_tx(struct sk_buff *skb, struct net_device *dev)
{
	struct netdev_private *np = dev->priv;
	unsigned entry;
	u32 intr_status;

	/* Caution: the write order is important here, set the field
	   with the "ownership" bits last. */

	/* Calculate the next Tx descriptor entry. */
	entry = np->cur_tx % TX_RING_SIZE;

	if (skb->len < ETH_ZLEN) {
		skb = skb_padto(skb, ETH_ZLEN);
		if (skb == NULL)
			return 0;
	}

	np->tx_skbuff[entry] = skb;

	if ((np->drv_flags & ReqTxAlign) &&
		(((long)skb->data & 3) || skb_shinfo(skb)->nr_frags != 0 || skb->ip_summed == CHECKSUM_HW)
		) {
		/* Must use alignment buffer. */
		if (skb->len > PKT_BUF_SZ) {
			/* packet too long, drop it */
			dev_kfree_skb(skb);
			np->tx_skbuff[entry] = NULL;
			np->stats.tx_dropped++;
			return 0;
		}
		skb_copy_and_csum_dev(skb, np->tx_buf[entry]);
		np->tx_skbuff_dma[entry] = 0;
		np->tx_ring[entry].addr = cpu_to_le32(np->tx_bufs_dma +
										  (np->tx_buf[entry] - np->tx_bufs));
	} else {
		np->tx_skbuff_dma[entry] =
			pci_map_single(np->pdev, skb->data, skb->len, PCI_DMA_TODEVICE);
		np->tx_ring[entry].addr = cpu_to_le32(np->tx_skbuff_dma[entry]);
	}

	np->tx_ring[entry].desc_length = 
		cpu_to_le32(TXDESC | (skb->len >= ETH_ZLEN ? skb->len : ETH_ZLEN));

	/* lock eth irq */
	spin_lock_irq (&np->lock);
	wmb();
	np->tx_ring[entry].tx_status = cpu_to_le32(DescOwn);
	wmb();

	np->cur_tx++;

	/* Non-x86 Todo: explicitly flush cache lines here. */

	/*
	 * Wake the potentially-idle transmit channel unless errors are
	 * pending (the ISR must sort them out first).
	 */
	intr_status = get_intr_status(dev);
	if ((intr_status & IntrTxErrSummary) == 0) {
		writew(CmdTxDemand | np->chip_cmd, dev->base_addr + ChipCmd);
	}
	IOSYNC;

	if (np->cur_tx == np->dirty_tx + TX_QUEUE_LEN)
		netif_stop_queue(dev);

	dev->trans_start = jiffies;

	spin_unlock_irq (&np->lock);

	if (debug > 4) {
		printk(KERN_DEBUG "%s: Transmit frame #%d queued in slot %d.\n",
			   dev->name, np->cur_tx-1, entry);
	}
	return 0;
}

/* The interrupt handler does all of the Rx thread work and cleans up
   after the Tx thread. */
static irqreturn_t via_rhine_interrupt(int irq, void *dev_instance, struct pt_regs *rgs)
{
	struct net_device *dev = dev_instance;
	long ioaddr;
	u32 intr_status;
	int boguscnt = max_interrupt_work;
	int handled = 0;

	ioaddr = dev->base_addr;
	
	while ((intr_status = get_intr_status(dev))) {
		handled = 1;

		/* Acknowledge all of the current interrupt sources ASAP. */
		if (intr_status & IntrTxDescRace)
			writeb(0x08, ioaddr + IntrStatus2);
		writew(intr_status & 0xffff, ioaddr + IntrStatus);
		IOSYNC;

		if (debug > 4)
			printk(KERN_DEBUG "%s: Interrupt, status %8.8x.\n",
				   dev->name, intr_status);

		if (intr_status & (IntrRxDone | IntrRxErr | IntrRxDropped |
						   IntrRxWakeUp | IntrRxEmpty | IntrRxNoBuf))
			via_rhine_rx(dev);

		if (intr_status & (IntrTxErrSummary | IntrTxDone)) {
			if (intr_status & IntrTxErrSummary) {
				int cnt = 20;
				/* Avoid scavenging before Tx engine turned off */
				while ((readw(ioaddr+ChipCmd) & CmdTxOn) && --cnt)
					udelay(5);
				if (debug > 2 && !cnt)
					printk(KERN_WARNING "%s: via_rhine_interrupt() "
						   "Tx engine still on.\n",
						   dev->name);
			}
			via_rhine_tx(dev);
		}

		/* Abnormal error summary/uncommon events handlers. */
		if (intr_status & (IntrPCIErr | IntrLinkChange |
				   IntrStatsMax | IntrTxError | IntrTxAborted |
				   IntrTxUnderrun | IntrTxDescRace))
			via_rhine_error(dev, intr_status);

		if (--boguscnt < 0) {
			printk(KERN_WARNING "%s: Too much work at interrupt, "
				   "status=%#8.8x.\n",
				   dev->name, intr_status);
			break;
		}
	}

	if (debug > 3)
		printk(KERN_DEBUG "%s: exiting interrupt, status=%8.8x.\n",
			   dev->name, readw(ioaddr + IntrStatus));
	return IRQ_RETVAL(handled);
}

/* This routine is logically part of the interrupt handler, but isolated
   for clarity. */
static void via_rhine_tx(struct net_device *dev)
{
	struct netdev_private *np = dev->priv;
	int txstatus = 0, entry = np->dirty_tx % TX_RING_SIZE;

	spin_lock (&np->lock);

	/* find and cleanup dirty tx descriptors */
	while (np->dirty_tx != np->cur_tx) {
		txstatus = le32_to_cpu(np->tx_ring[entry].tx_status);
		if (debug > 6)
			printk(KERN_DEBUG " Tx scavenge %d status %8.8x.\n",
				   entry, txstatus);
		if (txstatus & DescOwn)
			break;
		if (txstatus & 0x8000) {
			if (debug > 1)
				printk(KERN_DEBUG "%s: Transmit error, Tx status %8.8x.\n",
					   dev->name, txstatus);
			np->stats.tx_errors++;
			if (txstatus & 0x0400) np->stats.tx_carrier_errors++;
			if (txstatus & 0x0200) np->stats.tx_window_errors++;
			if (txstatus & 0x0100) np->stats.tx_aborted_errors++;
			if (txstatus & 0x0080) np->stats.tx_heartbeat_errors++;
			if (((np->chip_id == VT86C100A) && txstatus & 0x0002) ||
				(txstatus & 0x0800) || (txstatus & 0x1000)) {
				np->stats.tx_fifo_errors++;
				np->tx_ring[entry].tx_status = cpu_to_le32(DescOwn);
				break; /* Keep the skb - we try again */
			}
			/* Transmitter restarted in 'abnormal' handler. */
		} else {
			if (np->chip_id == VT86C100A)
				np->stats.collisions += (txstatus >> 3) & 0x0F;
			else
				np->stats.collisions += txstatus & 0x0F;
			if (debug > 6)
				printk(KERN_DEBUG "collisions: %1.1x:%1.1x\n",
					(txstatus >> 3) & 0xF,
					txstatus & 0xF);
			np->stats.tx_bytes += np->tx_skbuff[entry]->len;
			np->stats.tx_packets++;
		}
		/* Free the original skb. */
		if (np->tx_skbuff_dma[entry]) {
			pci_unmap_single(np->pdev,
							 np->tx_skbuff_dma[entry],
							 np->tx_skbuff[entry]->len, PCI_DMA_TODEVICE);
		}
		dev_kfree_skb_irq(np->tx_skbuff[entry]);
		np->tx_skbuff[entry] = NULL;
		entry = (++np->dirty_tx) % TX_RING_SIZE;
	}
	if ((np->cur_tx - np->dirty_tx) < TX_QUEUE_LEN - 4)
		netif_wake_queue (dev);

	spin_unlock (&np->lock);
}

/* This routine is logically part of the interrupt handler, but isolated
   for clarity and better register allocation. */
static void via_rhine_rx(struct net_device *dev)
{
	struct netdev_private *np = dev->priv;
	int entry = np->cur_rx % RX_RING_SIZE;
	int boguscnt = np->dirty_rx + RX_RING_SIZE - np->cur_rx;

	if (debug > 4) {
		printk(KERN_DEBUG "%s: via_rhine_rx(), entry %d status %8.8x.\n",
			   dev->name, entry, le32_to_cpu(np->rx_head_desc->rx_status));
	}

	/* If EOP is set on the next entry, it's a new packet. Send it up. */
	while ( ! (np->rx_head_desc->rx_status & cpu_to_le32(DescOwn))) {
		struct rx_desc *desc = np->rx_head_desc;
		u32 desc_status = le32_to_cpu(desc->rx_status);
		int data_size = desc_status >> 16;

		if (debug > 4)
			printk(KERN_DEBUG "  via_rhine_rx() status is %8.8x.\n",
				   desc_status);
		if (--boguscnt < 0)
			break;
		if ( (desc_status & (RxWholePkt | RxErr)) !=  RxWholePkt) {
			if ((desc_status & RxWholePkt) !=  RxWholePkt) {
				printk(KERN_WARNING "%s: Oversized Ethernet frame spanned "
					   "multiple buffers, entry %#x length %d status %8.8x!\n",
					   dev->name, entry, data_size, desc_status);
				printk(KERN_WARNING "%s: Oversized Ethernet frame %p vs %p.\n",
					   dev->name, np->rx_head_desc, &np->rx_ring[entry]);
				np->stats.rx_length_errors++;
			} else if (desc_status & RxErr) {
				/* There was a error. */
				if (debug > 2)
					printk(KERN_DEBUG "  via_rhine_rx() Rx error was %8.8x.\n",
						   desc_status);
				np->stats.rx_errors++;
				if (desc_status & 0x0030) np->stats.rx_length_errors++;
				if (desc_status & 0x0048) np->stats.rx_fifo_errors++;
				if (desc_status & 0x0004) np->stats.rx_frame_errors++;
				if (desc_status & 0x0002) {
					/* this can also be updated outside the interrupt handler */
					spin_lock (&np->lock);
					np->stats.rx_crc_errors++;
					spin_unlock (&np->lock);
				}
			}
		} else {
			struct sk_buff *skb;
			/* Length should omit the CRC */
			int pkt_len = data_size - 4;

			/* Check if the packet is long enough to accept without copying
			   to a minimally-sized skbuff. */
			if (pkt_len < rx_copybreak &&
				(skb = dev_alloc_skb(pkt_len + 2)) != NULL) {
				skb->dev = dev;
				skb_reserve(skb, 2);	/* 16 byte align the IP header */
				pci_dma_sync_single(np->pdev, np->rx_skbuff_dma[entry],
						    np->rx_buf_sz, PCI_DMA_FROMDEVICE);

				/* *_IP_COPYSUM isn't defined anywhere and eth_copy_and_sum
				   is memcpy for all archs so this is kind of pointless right
				   now ... or? */
#if HAS_IP_COPYSUM                     /* Call copy + cksum if available. */
				eth_copy_and_sum(skb, np->rx_skbuff[entry]->tail, pkt_len, 0);
				skb_put(skb, pkt_len);
#else
				memcpy(skb_put(skb, pkt_len), np->rx_skbuff[entry]->tail,
					   pkt_len);
#endif
			} else {
				skb = np->rx_skbuff[entry];
				if (skb == NULL) {
					printk(KERN_ERR "%s: Inconsistent Rx descriptor chain.\n",
						   dev->name);
					break;
				}
				np->rx_skbuff[entry] = NULL;
				skb_put(skb, pkt_len);
				pci_unmap_single(np->pdev, np->rx_skbuff_dma[entry],
								 np->rx_buf_sz, PCI_DMA_FROMDEVICE);
			}
			skb->protocol = eth_type_trans(skb, dev);
			netif_rx(skb);
			dev->last_rx = jiffies;
			np->stats.rx_bytes += pkt_len;
			np->stats.rx_packets++;
		}
		entry = (++np->cur_rx) % RX_RING_SIZE;
		np->rx_head_desc = &np->rx_ring[entry];
	}

	/* Refill the Rx ring buffers. */
	for (; np->cur_rx - np->dirty_rx > 0; np->dirty_rx++) {
		struct sk_buff *skb;
		entry = np->dirty_rx % RX_RING_SIZE;
		if (np->rx_skbuff[entry] == NULL) {
			skb = dev_alloc_skb(np->rx_buf_sz);
			np->rx_skbuff[entry] = skb;
			if (skb == NULL)
				break;			/* Better luck next round. */
			skb->dev = dev;			/* Mark as being used by this device. */
			np->rx_skbuff_dma[entry] =
				pci_map_single(np->pdev, skb->tail, np->rx_buf_sz, 
							   PCI_DMA_FROMDEVICE);
			np->rx_ring[entry].addr = cpu_to_le32(np->rx_skbuff_dma[entry]);
		}
		np->rx_ring[entry].rx_status = cpu_to_le32(DescOwn);
	}

	/* Pre-emptively restart Rx engine. */
	writew(readw(dev->base_addr + ChipCmd) | CmdRxOn | CmdRxDemand,
		   dev->base_addr + ChipCmd);
}

/* Clears the "tally counters" for CRC errors and missed frames(?).
   It has been reported that some chips need a write of 0 to clear
   these, for others the counters are set to 1 when written to and
   instead cleared when read. So we clear them both ways ... */
static inline void clear_tally_counters(const long ioaddr)
{
	writel(0, ioaddr + RxMissed);
	readw(ioaddr + RxCRCErrs);
	readw(ioaddr + RxMissed);
}

static void via_rhine_restart_tx(struct net_device *dev) {
	struct netdev_private *np = dev->priv;
	long ioaddr = dev->base_addr;
	int entry = np->dirty_tx % TX_RING_SIZE;
	u32 intr_status;

	/*
	 * If new errors occured, we need to sort them out before doing Tx.
	 * In that case the ISR will be back here RSN anyway.
	 */
	intr_status = get_intr_status(dev);

	if ((intr_status & IntrTxErrSummary) == 0) {

		/* We know better than the chip where it should continue. */
		writel(np->tx_ring_dma + entry * sizeof(struct tx_desc),
			   ioaddr + TxRingPtr);

		writew(CmdTxDemand | np->chip_cmd, ioaddr + ChipCmd);
		IOSYNC;
	}
	else {
		/* This should never happen */
		if (debug > 1)
			printk(KERN_WARNING "%s: via_rhine_restart_tx() "
				   "Another error occured %8.8x.\n",
				   dev->name, intr_status);
	}

}

static void via_rhine_error(struct net_device *dev, int intr_status)
{
	struct netdev_private *np = dev->priv;
	long ioaddr = dev->base_addr;

	spin_lock (&np->lock);

	if (intr_status & (IntrLinkChange)) {
		if (readb(ioaddr + MIIStatus) & 0x02) {
			/* Link failed, restart autonegotiation. */
			if (np->drv_flags & HasDavicomPhy)
				mdio_write(dev, np->phys[0], MII_BMCR, 0x3300);
		} else
			via_rhine_check_duplex(dev);
		if (debug)
			printk(KERN_ERR "%s: MII status changed: Autonegotiation "
				   "advertising %4.4x  partner %4.4x.\n", dev->name,
			   mdio_read(dev, np->phys[0], MII_ADVERTISE),
			   mdio_read(dev, np->phys[0], MII_LPA));
	}
	if (intr_status & IntrStatsMax) {
		np->stats.rx_crc_errors	+= readw(ioaddr + RxCRCErrs);
		np->stats.rx_missed_errors	+= readw(ioaddr + RxMissed);
		clear_tally_counters(ioaddr);
	}
	if (intr_status & IntrTxAborted) {
		if (debug > 1)
			printk(KERN_INFO "%s: Abort %8.8x, frame dropped.\n",
				   dev->name, intr_status);
	}
	if (intr_status & IntrTxUnderrun) {
		if (np->tx_thresh < 0xE0)
			writeb(np->tx_thresh += 0x20, ioaddr + TxConfig);
		if (debug > 1)
			printk(KERN_INFO "%s: Transmitter underrun, Tx "
				   "threshold now %2.2x.\n",
				   dev->name, np->tx_thresh);
	}
	if (intr_status & IntrTxDescRace) {
		if (debug > 2)
			printk(KERN_INFO "%s: Tx descriptor write-back race.\n",
				   dev->name);
	}
	if ((intr_status & IntrTxError) && ~( IntrTxAborted | IntrTxUnderrun |
										   IntrTxDescRace )) {
		if (np->tx_thresh < 0xE0) {
			writeb(np->tx_thresh += 0x20, ioaddr + TxConfig);
		}
		if (debug > 1)
			printk(KERN_INFO "%s: Unspecified error. Tx "
				   "threshold now %2.2x.\n",
				   dev->name, np->tx_thresh);
	}
	if (intr_status & ( IntrTxAborted | IntrTxUnderrun | IntrTxDescRace |
						IntrTxError ))
		via_rhine_restart_tx(dev);

	if (intr_status & ~( IntrLinkChange | IntrStatsMax | IntrTxUnderrun |
						 IntrTxError | IntrTxAborted | IntrNormalSummary |
						 IntrTxDescRace )) {
		if (debug > 1)
			printk(KERN_ERR "%s: Something Wicked happened! %8.8x.\n",
				   dev->name, intr_status);
	}

	spin_unlock (&np->lock);
}

static struct net_device_stats *via_rhine_get_stats(struct net_device *dev)
{
	struct netdev_private *np = dev->priv;
	long ioaddr = dev->base_addr;
	unsigned long flags;

	spin_lock_irqsave(&np->lock, flags);
	np->stats.rx_crc_errors	+= readw(ioaddr + RxCRCErrs);
	np->stats.rx_missed_errors	+= readw(ioaddr + RxMissed);
	clear_tally_counters(ioaddr);
	spin_unlock_irqrestore(&np->lock, flags);

	return &np->stats;
}

static void via_rhine_set_rx_mode(struct net_device *dev)
{
	struct netdev_private *np = dev->priv;
	long ioaddr = dev->base_addr;
	u32 mc_filter[2];			/* Multicast hash filter */
	u8 rx_mode;					/* Note: 0x02=accept runt, 0x01=accept errs */

	if (dev->flags & IFF_PROMISC) {			/* Set promiscuous. */
		/* Unconditionally log net taps. */
		printk(KERN_NOTICE "%s: Promiscuous mode enabled.\n", dev->name);
		rx_mode = 0x1C;
		writel(0xffffffff, ioaddr + MulticastFilter0);
		writel(0xffffffff, ioaddr + MulticastFilter1);
	} else if ((dev->mc_count > multicast_filter_limit)
			   ||  (dev->flags & IFF_ALLMULTI)) {
		/* Too many to match, or accept all multicasts. */
		writel(0xffffffff, ioaddr + MulticastFilter0);
		writel(0xffffffff, ioaddr + MulticastFilter1);
		rx_mode = 0x0C;
	} else {
		struct dev_mc_list *mclist;
		int i;
		memset(mc_filter, 0, sizeof(mc_filter));
		for (i = 0, mclist = dev->mc_list; mclist && i < dev->mc_count;
			 i++, mclist = mclist->next) {
			int bit_nr = ether_crc(ETH_ALEN, mclist->dmi_addr) >> 26;

			mc_filter[bit_nr >> 5] |= cpu_to_le32(1 << (bit_nr & 31));
		}
		writel(mc_filter[0], ioaddr + MulticastFilter0);
		writel(mc_filter[1], ioaddr + MulticastFilter1);
		rx_mode = 0x0C;
	}
	writeb(np->rx_thresh | rx_mode, ioaddr + RxConfig);
}

static void netdev_get_drvinfo (struct net_device *dev, struct ethtool_drvinfo *info)
{
	struct netdev_private *np = dev->priv;

	strcpy (info->driver, DRV_NAME);
	strcpy (info->version, DRV_VERSION);
	strcpy (info->bus_info, pci_name(np->pdev));
}

static int netdev_get_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct netdev_private *np = dev->priv;
	int rc;

	if (!(np->drv_flags & CanHaveMII))
		return -EINVAL;

	spin_lock_irq(&np->lock);
	rc = mii_ethtool_gset(&np->mii_if, cmd);
	spin_unlock_irq(&np->lock);

	return rc;
}

static int netdev_set_settings(struct net_device *dev, struct ethtool_cmd *cmd)
{
	struct netdev_private *np = dev->priv;
	int rc;

	if (!(np->drv_flags & CanHaveMII))
		return -EINVAL;

	spin_lock_irq(&np->lock);
	rc = mii_ethtool_sset(&np->mii_if, cmd);
	spin_unlock_irq(&np->lock);

	return rc;
}

static int netdev_nway_reset(struct net_device *dev)
{
	struct netdev_private *np = dev->priv;

	if (!(np->drv_flags & CanHaveMII))
		return -EINVAL;

	return mii_nway_restart(&np->mii_if);
}

static u32 netdev_get_link(struct net_device *dev)
{
	struct netdev_private *np = dev->priv;

	if (!(np->drv_flags & CanHaveMII))
		return 0;	/* -EINVAL */

	return mii_link_ok(&np->mii_if);
}

static u32 netdev_get_msglevel(struct net_device *dev)
{
	return debug;
}

static void netdev_set_msglevel(struct net_device *dev, u32 value)
{
	debug = value;
}

static struct ethtool_ops netdev_ethtool_ops = {
	.get_drvinfo		= netdev_get_drvinfo,
	.get_settings		= netdev_get_settings,
	.set_settings		= netdev_set_settings,
	.nway_reset		= netdev_nway_reset,
	.get_link		= netdev_get_link,
	.get_msglevel		= netdev_get_msglevel,
	.set_msglevel		= netdev_set_msglevel,
	.get_sg			= ethtool_op_get_sg,
	.get_tx_csum		= ethtool_op_get_tx_csum,
};

static int netdev_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct netdev_private *np = dev->priv;
	struct mii_ioctl_data *data = (struct mii_ioctl_data *) & rq->ifr_data;
	int rc;

	if (!netif_running(dev))
		return -EINVAL;

	spin_lock_irq(&np->lock);
	rc = generic_mii_ioctl(&np->mii_if, data, cmd, NULL);
	spin_unlock_irq(&np->lock);

	return rc;
}

static int via_rhine_close(struct net_device *dev)
{
	long ioaddr = dev->base_addr;
	struct netdev_private *np = dev->priv;

	del_timer_sync(&np->timer);

	spin_lock_irq(&np->lock);

	netif_stop_queue(dev);

	if (debug > 1)
		printk(KERN_DEBUG "%s: Shutting down ethercard, status was %4.4x.\n",
			   dev->name, readw(ioaddr + ChipCmd));

	/* Switch to loopback mode to avoid hardware races. */
	writeb(np->tx_thresh | 0x02, ioaddr + TxConfig);

	/* Disable interrupts by clearing the interrupt mask. */
	writew(0x0000, ioaddr + IntrEnable);

	/* Stop the chip's Tx and Rx processes. */
	writew(CmdStop, ioaddr + ChipCmd);

	spin_unlock_irq(&np->lock);

	free_irq(np->pdev->irq, dev);
	free_rbufs(dev);
	free_tbufs(dev);
	free_ring(dev);

	return 0;
}


static void __devexit via_rhine_remove_one (struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	
	unregister_netdev(dev);

	pci_release_regions(pdev);

#ifdef USE_MEM
	iounmap((char *)(dev->base_addr));
#endif

	kfree(dev);
	pci_disable_device(pdev);
	pci_set_drvdata(pdev, NULL);
}


static struct pci_driver via_rhine_driver = {
	.name		= "via-rhine",
	.id_table	= via_rhine_pci_tbl,
	.probe		= via_rhine_init_one,
	.remove		= __devexit_p(via_rhine_remove_one),
};


static int __init via_rhine_init (void)
{
/* when a module, this is printed whether or not devices are found in probe */
#ifdef MODULE
	printk(version);
#endif
	return pci_module_init (&via_rhine_driver);
}


static void __exit via_rhine_cleanup (void)
{
	pci_unregister_driver (&via_rhine_driver);
}


module_init(via_rhine_init);
module_exit(via_rhine_cleanup);


/*
 * Local variables:
 *  compile-command: "gcc -DMODULE -D__KERNEL__ -I/usr/src/linux/net/inet -Wall -Wstrict-prototypes -O6 -c via-rhine.c `[ -f /usr/include/linux/modversions.h ] && echo -DMODVERSIONS`"
 *  c-indent-level: 4
 *  c-basic-offset: 4
 *  tab-width: 4
 * End:
 */
