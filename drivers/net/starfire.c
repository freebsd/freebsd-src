/* starfire.c: Linux device driver for the Adaptec Starfire network adapter. */
/*
	Written 1998-2000 by Donald Becker.

	Current maintainer is Ion Badulescu <ionut@cs.columbia.edu>. Please
	send all bug reports to me, and not to Donald Becker, as this code
	has been modified quite a bit from Donald's original version.

	This software may be used and distributed according to the terms of
	the GNU General Public License (GPL), incorporated herein by reference.
	Drivers based on or derived from this code fall under the GPL and must
	retain the authorship, copyright and license notice.  This file is not
	a complete program and may only be used when the entire operating
	system is licensed under the GPL.

	The author may be reached as becker@scyld.com, or C/O
	Scyld Computing Corporation
	410 Severn Ave., Suite 210
	Annapolis MD 21403

	Support and updates available at
	http://www.scyld.com/network/starfire.html

	-----------------------------------------------------------

	Linux kernel-specific changes:

	LK1.1.1 (jgarzik):
	- Use PCI driver interface
	- Fix MOD_xxx races
	- softnet fixups

	LK1.1.2 (jgarzik):
	- Merge Becker version 0.15

	LK1.1.3 (Andrew Morton)
	- Timer cleanups

	LK1.1.4 (jgarzik):
	- Merge Becker version 1.03

	LK1.2.1 (Ion Badulescu <ionut@cs.columbia.edu>)
	- Support hardware Rx/Tx checksumming
	- Use the GFP firmware taken from Adaptec's Netware driver

	LK1.2.2 (Ion Badulescu)
	- Backported to 2.2.x

	LK1.2.3 (Ion Badulescu)
	- Fix the flaky mdio interface
	- More compat clean-ups

	LK1.2.4 (Ion Badulescu)
	- More 2.2.x initialization fixes

	LK1.2.5 (Ion Badulescu)
	- Several fixes from Manfred Spraul

	LK1.2.6 (Ion Badulescu)
	- Fixed ifup/ifdown/ifup problem in 2.4.x

	LK1.2.7 (Ion Badulescu)
	- Removed unused code
	- Made more functions static and __init

	LK1.2.8 (Ion Badulescu)
	- Quell bogus error messages, inform about the Tx threshold
	- Removed #ifdef CONFIG_PCI, this driver is PCI only

	LK1.2.9 (Ion Badulescu)
	- Merged Jeff Garzik's changes from 2.4.4-pre5
	- Added 2.2.x compatibility stuff required by the above changes

	LK1.2.9a (Ion Badulescu)
	- More updates from Jeff Garzik

	LK1.3.0 (Ion Badulescu)
	- Merged zerocopy support

	LK1.3.1 (Ion Badulescu)
	- Added ethtool support
	- Added GPIO (media change) interrupt support

	LK1.3.2 (Ion Badulescu)
	- Fixed 2.2.x compatibility issues introduced in 1.3.1
	- Fixed ethtool ioctl returning uninitialized memory

	LK1.3.3 (Ion Badulescu)
	- Initialize the TxMode register properly
	- Don't dereference dev->priv after freeing it

	LK1.3.4 (Ion Badulescu)
	- Fixed initialization timing problems
	- Fixed interrupt mask definitions

	LK1.3.5 (jgarzik)
	- ethtool NWAY_RST, GLINK, [GS]MSGLVL support

	LK1.3.6:
	- Sparc64 support and fixes (Ion Badulescu)
	- Better stats and error handling (Ion Badulescu)
	- Use new pci_set_mwi() PCI API function (jgarzik)

	LK1.3.7 (Ion Badulescu)
	- minimal implementation of tx_timeout()
	- correctly shutdown the Rx/Tx engines in netdev_close()
	- added calls to netif_carrier_on/off
	(patch from Stefan Rompf <srompf@isg.de>)
	- VLAN support

	LK1.3.8 (Ion Badulescu)
	- adjust DMA burst size on sparc64
	- 64-bit support
	- reworked zerocopy support for 64-bit buffers
	- working and usable interrupt mitigation/latency
	- reduced Tx interrupt frequency for lower interrupt overhead

	LK1.3.9 (Ion Badulescu)
	- bugfix for mcast filter
	- enable the right kind of Tx interrupts (TxDMADone, not TxDone)

TODO:
	- full NAPI support
*/

#define DRV_NAME	"starfire"
#define DRV_VERSION	"1.03+LK1.3.9"
#define DRV_RELDATE	"December 13, 2002"

#include <linux/config.h>
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <asm/processor.h>		/* Processor type for cache alignment. */
#include <asm/uaccess.h>
#include <asm/io.h>

/*
 * Adaptec's license for their Novell drivers (which is where I got the
 * firmware files) does not allow one to redistribute them. Thus, we can't
 * include the firmware with this driver.
 *
 * However, should a legal-to-use firmware become available,
 * the driver developer would need only to obtain the firmware in the
 * form of a C header file.
 * Once that's done, the #undef below must be changed into a #define
 * for this driver to really use the firmware. Note that Rx/Tx
 * hardware TCP checksumming is not possible without the firmware.
 *
 * WANTED: legal firmware to include with this GPL'd driver.
 */
#undef HAS_FIRMWARE
/*
 * The current frame processor firmware fails to checksum a fragment
 * of length 1. If and when this is fixed, the #define below can be removed.
 */
#define HAS_BROKEN_FIRMWARE
/*
 * Define this if using the driver with the zero-copy patch
 */
#if defined(HAS_FIRMWARE) && defined(MAX_SKB_FRAGS)
#define ZEROCOPY
#endif

#ifdef HAS_FIRMWARE
#include "starfire_firmware.h"
#endif /* HAS_FIRMWARE */

#if defined(CONFIG_VLAN_8021Q) || defined(CONFIG_VLAN_8021Q_MODULE)
#define VLAN_SUPPORT
#endif

/* The user-configurable values.
   These may be modified when a driver module is loaded.*/

/* Used for tuning interrupt latency vs. overhead. */
static int intr_latency;
static int small_frames;

static int debug = 1;			/* 1 normal messages, 0 quiet .. 7 verbose. */
static int max_interrupt_work = 20;
static int mtu;
/* Maximum number of multicast addresses to filter (vs. rx-all-multicast).
   The Starfire has a 512 element hash table based on the Ethernet CRC. */
static int multicast_filter_limit = 512;
/* Whether to do TCP/UDP checksums in hardware */
#ifdef HAS_FIRMWARE
static int enable_hw_cksum = 1;
#else
static int enable_hw_cksum = 0;
#endif

#define PKT_BUF_SZ	1536		/* Size of each temporary Rx buffer.*/
/*
 * Set the copy breakpoint for the copy-only-tiny-frames scheme.
 * Setting to > 1518 effectively disables this feature.
 *
 * NOTE:
 * The ia64 doesn't allow for unaligned loads even of integers being
 * misaligned on a 2 byte boundary. Thus always force copying of
 * packets as the starfire doesn't allow for misaligned DMAs ;-(
 * 23/10/2000 - Jes
 *
 * The Alpha and the Sparc don't like unaligned loads, either. On Sparc64,
 * at least, having unaligned frames leads to a rather serious performance
 * penalty. -Ion
 */
#if defined(__ia64__) || defined(__alpha__) || defined(__sparc__)
static int rx_copybreak = PKT_BUF_SZ;
#else
static int rx_copybreak /* = 0 */;
#endif

/* PCI DMA burst size -- on sparc64 we want to force it to 64 bytes, on the others the default of 128 is fine. */
#ifdef __sparc__
#define DMA_BURST_SIZE 64
#else
#define DMA_BURST_SIZE 128
#endif

/* Used to pass the media type, etc.
   Both 'options[]' and 'full_duplex[]' exist for driver interoperability.
   The media type is usually passed in 'options[]'.
   These variables are deprecated, use ethtool instead. -Ion
*/
#define MAX_UNITS 8		/* More are supported, limit only on options */
static int options[MAX_UNITS] = {0, };
static int full_duplex[MAX_UNITS] = {0, };

/* Operational parameters that are set at compile time. */

/* The "native" ring sizes are either 256 or 2048.
   However in some modes a descriptor may be marked to wrap the ring earlier.
*/
#define RX_RING_SIZE	256
#define TX_RING_SIZE	32
/* The completion queues are fixed at 1024 entries i.e. 4K or 8KB. */
#define DONE_Q_SIZE	1024
/* All queues must be aligned on a 256-byte boundary */
#define QUEUE_ALIGN	256

#if RX_RING_SIZE > 256
#define RX_Q_ENTRIES Rx2048QEntries
#else
#define RX_Q_ENTRIES Rx256QEntries
#endif

/* Operational parameters that usually are not changed. */
/* Time in jiffies before concluding the transmitter is hung. */
#define TX_TIMEOUT	(2 * HZ)

/*
 * This SUCKS.
 * We need a much better method to determine if dma_addr_t is 64-bit.
 */
#if (defined(__i386__) && defined(CONFIG_HIGHMEM) && (LINUX_VERSION_CODE > 0x20500 || defined(CONFIG_HIGHMEM64G))) || defined(__x86_64__) || defined (__ia64__) || defined(__mips64__) || (defined(__mips__) && defined(CONFIG_HIGHMEM) && defined(CONFIG_64BIT_PHYS_ADDR))
/* 64-bit dma_addr_t */
#define ADDR_64BITS	/* This chip uses 64 bit addresses. */
#define cpu_to_dma(x) cpu_to_le64(x)
#define dma_to_cpu(x) le64_to_cpu(x)
#define RX_DESC_Q_ADDR_SIZE RxDescQAddr64bit
#define TX_DESC_Q_ADDR_SIZE TxDescQAddr64bit
#define RX_COMPL_Q_ADDR_SIZE RxComplQAddr64bit
#define TX_COMPL_Q_ADDR_SIZE TxComplQAddr64bit
#define RX_DESC_ADDR_SIZE RxDescAddr64bit
#else  /* 32-bit dma_addr_t */
#define cpu_to_dma(x) cpu_to_le32(x)
#define dma_to_cpu(x) le32_to_cpu(x)
#define RX_DESC_Q_ADDR_SIZE RxDescQAddr32bit
#define TX_DESC_Q_ADDR_SIZE TxDescQAddr32bit
#define RX_COMPL_Q_ADDR_SIZE RxComplQAddr32bit
#define TX_COMPL_Q_ADDR_SIZE TxComplQAddr32bit
#define RX_DESC_ADDR_SIZE RxDescAddr32bit
#endif

#ifdef MAX_SKB_FRAGS
#define skb_first_frag_len(skb)	skb_headlen(skb)
#define skb_num_frags(skb) (skb_shinfo(skb)->nr_frags + 1)
#else  /* not MAX_SKB_FRAGS */
#define skb_first_frag_len(skb)	(skb->len)
#define skb_num_frags(skb) 1
#endif /* not MAX_SKB_FRAGS */

/* 2.2.x compatibility code */
#if LINUX_VERSION_CODE < 0x20300

#include "starfire-kcomp22.h"

#else  /* LINUX_VERSION_CODE > 0x20300 */

#include <linux/crc32.h>
#include <linux/ethtool.h>
#include <linux/mii.h>

#include <linux/if_vlan.h>

#define COMPAT_MOD_INC_USE_COUNT
#define COMPAT_MOD_DEC_USE_COUNT

#define init_tx_timer(dev, func, timeout) \
	dev->tx_timeout = func; \
	dev->watchdog_timeo = timeout;
#define kick_tx_timer(dev, func, timeout)

#define netif_start_if(dev)
#define netif_stop_if(dev)

#define PCI_SLOT_NAME(pci_dev)	(pci_dev)->slot_name

#endif /* LINUX_VERSION_CODE > 0x20300 */
/* end of compatibility code */


/* These identify the driver base version and may not be removed. */
static char version[] __devinitdata =
KERN_INFO "starfire.c:v1.03 7/26/2000  Written by Donald Becker <becker@scyld.com>\n"
KERN_INFO " (unofficial 2.2/2.4 kernel port, version " DRV_VERSION ", " DRV_RELDATE ")\n";

MODULE_AUTHOR("Donald Becker <becker@scyld.com>");
MODULE_DESCRIPTION("Adaptec Starfire Ethernet driver");
MODULE_LICENSE("GPL");

MODULE_PARM(max_interrupt_work, "i");
MODULE_PARM(mtu, "i");
MODULE_PARM(debug, "i");
MODULE_PARM(rx_copybreak, "i");
MODULE_PARM(intr_latency, "i");
MODULE_PARM(small_frames, "i");
MODULE_PARM(options, "1-" __MODULE_STRING(MAX_UNITS) "i");
MODULE_PARM(full_duplex, "1-" __MODULE_STRING(MAX_UNITS) "i");
MODULE_PARM(enable_hw_cksum, "i");
MODULE_PARM_DESC(max_interrupt_work, "Maximum events handled per interrupt");
MODULE_PARM_DESC(mtu, "MTU (all boards)");
MODULE_PARM_DESC(debug, "Debug level (0-6)");
MODULE_PARM_DESC(rx_copybreak, "Copy breakpoint for copy-only-tiny-frames");
MODULE_PARM_DESC(intr_latency, "Maximum interrupt latency, in microseconds");
MODULE_PARM_DESC(small_frames, "Maximum size of receive frames that bypass interrupt latency (0,64,128,256,512)");
MODULE_PARM_DESC(options, "Deprecated: Bits 0-3: media type, bit 17: full duplex");
MODULE_PARM_DESC(full_duplex, "Deprecated: Forced full-duplex setting (0/1)");
MODULE_PARM_DESC(enable_hw_cksum, "Enable/disable hardware cksum support (0/1)");

/*
				Theory of Operation

I. Board Compatibility

This driver is for the Adaptec 6915 "Starfire" 64 bit PCI Ethernet adapter.

II. Board-specific settings

III. Driver operation

IIIa. Ring buffers

The Starfire hardware uses multiple fixed-size descriptor queues/rings.  The
ring sizes are set fixed by the hardware, but may optionally be wrapped
earlier by the END bit in the descriptor.
This driver uses that hardware queue size for the Rx ring, where a large
number of entries has no ill effect beyond increases the potential backlog.
The Tx ring is wrapped with the END bit, since a large hardware Tx queue
disables the queue layer priority ordering and we have no mechanism to
utilize the hardware two-level priority queue.  When modifying the
RX/TX_RING_SIZE pay close attention to page sizes and the ring-empty warning
levels.

IIIb/c. Transmit/Receive Structure

See the Adaptec manual for the many possible structures, and options for
each structure.  There are far too many to document all of them here.

For transmit this driver uses type 0/1 transmit descriptors (depending
on the 32/64 bitness of the architecture), and relies on automatic
minimum-length padding.  It does not use the completion queue
consumer index, but instead checks for non-zero status entries.

For receive this driver uses type 0/1/2/3 receive descriptors.  The driver
allocates full frame size skbuffs for the Rx ring buffers, so all frames
should fit in a single descriptor.  The driver does not use the completion
queue consumer index, but instead checks for non-zero status entries.

When an incoming frame is less than RX_COPYBREAK bytes long, a fresh skbuff
is allocated and the frame is copied to the new skbuff.  When the incoming
frame is larger, the skbuff is passed directly up the protocol stack.
Buffers consumed this way are replaced by newly allocated skbuffs in a later
phase of receive.

A notable aspect of operation is that unaligned buffers are not permitted by
the Starfire hardware.  Thus the IP header at offset 14 in an ethernet frame
isn't longword aligned, which may cause problems on some machine
e.g. Alphas and IA64. For these architectures, the driver is forced to copy
the frame into a new skbuff unconditionally. Copied frames are put into the
skbuff at an offset of "+2", thus 16-byte aligning the IP header.

IIId. Synchronization

The driver runs as two independent, single-threaded flows of control.  One
is the send-packet routine, which enforces single-threaded use by the
dev->tbusy flag.  The other thread is the interrupt handler, which is single
threaded by the hardware and interrupt handling software.

The send packet thread has partial control over the Tx ring and the netif_queue
status. If the number of free Tx slots in the ring falls below a certain number
(currently hardcoded to 4), it signals the upper layer to stop the queue.

The interrupt handler has exclusive control over the Rx ring and records stats
from the Tx ring.  After reaping the stats, it marks the Tx queue entry as
empty by incrementing the dirty_tx mark. Iff the netif_queue is stopped and the
number of free Tx slow is above the threshold, it signals the upper layer to
restart the queue.

IV. Notes

IVb. References

The Adaptec Starfire manuals, available only from Adaptec.
http://www.scyld.com/expert/100mbps.html
http://www.scyld.com/expert/NWay.html

IVc. Errata

- StopOnPerr is broken, don't enable
- Hardware ethernet padding exposes random data, perform software padding
  instead (unverified -- works correctly for all the hardware I have)

*/



enum chip_capability_flags {CanHaveMII=1, };

enum chipset {
	CH_6915 = 0,
};

static struct pci_device_id starfire_pci_tbl[] __devinitdata = {
	{ 0x9004, 0x6915, PCI_ANY_ID, PCI_ANY_ID, 0, 0, CH_6915 },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, starfire_pci_tbl);

/* A chip capabilities table, matching the CH_xxx entries in xxx_pci_tbl[] above. */
static struct chip_info {
	const char *name;
	int drv_flags;
} netdrv_tbl[] __devinitdata = {
	{ "Adaptec Starfire 6915", CanHaveMII },
};


/* Offsets to the device registers.
   Unlike software-only systems, device drivers interact with complex hardware.
   It's not useful to define symbolic names for every register bit in the
   device.  The name can only partially document the semantics and make
   the driver longer and more difficult to read.
   In general, only the important configuration values or bits changed
   multiple times should be defined symbolically.
*/
enum register_offsets {
	PCIDeviceConfig=0x50040, GenCtrl=0x50070, IntrTimerCtrl=0x50074,
	IntrClear=0x50080, IntrStatus=0x50084, IntrEnable=0x50088,
	MIICtrl=0x52000, TxStationAddr=0x50120, EEPROMCtrl=0x51000,
	GPIOCtrl=0x5008C, TxDescCtrl=0x50090,
	TxRingPtr=0x50098, HiPriTxRingPtr=0x50094, /* Low and High priority. */
	TxRingHiAddr=0x5009C,		/* 64 bit address extension. */
	TxProducerIdx=0x500A0, TxConsumerIdx=0x500A4,
	TxThreshold=0x500B0,
	CompletionHiAddr=0x500B4, TxCompletionAddr=0x500B8,
	RxCompletionAddr=0x500BC, RxCompletionQ2Addr=0x500C0,
	CompletionQConsumerIdx=0x500C4, RxDMACtrl=0x500D0,
	RxDescQCtrl=0x500D4, RxDescQHiAddr=0x500DC, RxDescQAddr=0x500E0,
	RxDescQIdx=0x500E8, RxDMAStatus=0x500F0, RxFilterMode=0x500F4,
	TxMode=0x55000, VlanType=0x55064,
	PerfFilterTable=0x56000, HashTable=0x56100,
	TxGfpMem=0x58000, RxGfpMem=0x5a000,
};

/*
 * Bits in the interrupt status/mask registers.
 * Warning: setting Intr[Ab]NormalSummary in the IntrEnable register
 * enables all the interrupt sources that are or'ed into those status bits.
 */
enum intr_status_bits {
	IntrLinkChange=0xf0000000, IntrStatsMax=0x08000000,
	IntrAbnormalSummary=0x02000000, IntrGeneralTimer=0x01000000,
	IntrSoftware=0x800000, IntrRxComplQ1Low=0x400000,
	IntrTxComplQLow=0x200000, IntrPCI=0x100000,
	IntrDMAErr=0x080000, IntrTxDataLow=0x040000,
	IntrRxComplQ2Low=0x020000, IntrRxDescQ1Low=0x010000,
	IntrNormalSummary=0x8000, IntrTxDone=0x4000,
	IntrTxDMADone=0x2000, IntrTxEmpty=0x1000,
	IntrEarlyRxQ2=0x0800, IntrEarlyRxQ1=0x0400,
	IntrRxQ2Done=0x0200, IntrRxQ1Done=0x0100,
	IntrRxGFPDead=0x80, IntrRxDescQ2Low=0x40,
	IntrNoTxCsum=0x20, IntrTxBadID=0x10,
	IntrHiPriTxBadID=0x08, IntrRxGfp=0x04,
	IntrTxGfp=0x02, IntrPCIPad=0x01,
	/* not quite bits */
	IntrRxDone=IntrRxQ2Done | IntrRxQ1Done,
	IntrRxEmpty=IntrRxDescQ1Low | IntrRxDescQ2Low,
	IntrNormalMask=0xff00, IntrAbnormalMask=0x3ff00fe,
};

/* Bits in the RxFilterMode register. */
enum rx_mode_bits {
	AcceptBroadcast=0x04, AcceptAllMulticast=0x02, AcceptAll=0x01,
	AcceptMulticast=0x10, PerfectFilter=0x40, HashFilter=0x30,
	PerfectFilterVlan=0x80, MinVLANPrio=0xE000, VlanMode=0x0200,
	WakeupOnGFP=0x0800,
};

/* Bits in the TxMode register */
enum tx_mode_bits {
	MiiSoftReset=0x8000, MIILoopback=0x4000,
	TxFlowEnable=0x0800, RxFlowEnable=0x0400,
	PadEnable=0x04, FullDuplex=0x02, HugeFrame=0x01,
};

/* Bits in the TxDescCtrl register. */
enum tx_ctrl_bits {
	TxDescSpaceUnlim=0x00, TxDescSpace32=0x10, TxDescSpace64=0x20,
	TxDescSpace128=0x30, TxDescSpace256=0x40,
	TxDescType0=0x00, TxDescType1=0x01, TxDescType2=0x02,
	TxDescType3=0x03, TxDescType4=0x04,
	TxNoDMACompletion=0x08,
	TxDescQAddr64bit=0x80, TxDescQAddr32bit=0,
	TxHiPriFIFOThreshShift=24, TxPadLenShift=16,
	TxDMABurstSizeShift=8,
};

/* Bits in the RxDescQCtrl register. */
enum rx_ctrl_bits {
	RxBufferLenShift=16, RxMinDescrThreshShift=0,
	RxPrefetchMode=0x8000, RxVariableQ=0x2000,
	Rx2048QEntries=0x4000, Rx256QEntries=0,
	RxDescAddr64bit=0x1000, RxDescAddr32bit=0,
	RxDescQAddr64bit=0x0100, RxDescQAddr32bit=0,
	RxDescSpace4=0x000, RxDescSpace8=0x100,
	RxDescSpace16=0x200, RxDescSpace32=0x300,
	RxDescSpace64=0x400, RxDescSpace128=0x500,
	RxConsumerWrEn=0x80,
};

/* Bits in the RxDMACtrl register. */
enum rx_dmactrl_bits {
	RxReportBadFrames=0x80000000, RxDMAShortFrames=0x40000000,
	RxDMABadFrames=0x20000000, RxDMACrcErrorFrames=0x10000000,
	RxDMAControlFrame=0x08000000, RxDMAPauseFrame=0x04000000,
	RxChecksumIgnore=0, RxChecksumRejectTCPUDP=0x02000000,
	RxChecksumRejectTCPOnly=0x01000000,
	RxCompletionQ2Enable=0x800000,
	RxDMAQ2Disable=0, RxDMAQ2FPOnly=0x100000,
	RxDMAQ2SmallPkt=0x200000, RxDMAQ2HighPrio=0x300000,
	RxDMAQ2NonIP=0x400000,
	RxUseBackupQueue=0x080000, RxDMACRC=0x040000,
	RxEarlyIntThreshShift=12, RxHighPrioThreshShift=8,
	RxBurstSizeShift=0,
};

/* Bits in the RxCompletionAddr register */
enum rx_compl_bits {
	RxComplQAddr64bit=0x80, RxComplQAddr32bit=0,
	RxComplProducerWrEn=0x40,
	RxComplType0=0x00, RxComplType1=0x10,
	RxComplType2=0x20, RxComplType3=0x30,
	RxComplThreshShift=0,
};

/* Bits in the TxCompletionAddr register */
enum tx_compl_bits {
	TxComplQAddr64bit=0x80, TxComplQAddr32bit=0,
	TxComplProducerWrEn=0x40,
	TxComplIntrStatus=0x20,
	CommonQueueMode=0x10,
	TxComplThreshShift=0,
};

/* Bits in the GenCtrl register */
enum gen_ctrl_bits {
	RxEnable=0x05, TxEnable=0x0a,
	RxGFPEnable=0x10, TxGFPEnable=0x20,
};

/* Bits in the IntrTimerCtrl register */
enum intr_ctrl_bits {
	Timer10X=0x800, EnableIntrMasking=0x60, SmallFrameBypass=0x100,
	SmallFrame64=0, SmallFrame128=0x200, SmallFrame256=0x400, SmallFrame512=0x600,
	IntrLatencyMask=0x1f,
};

/* The Rx and Tx buffer descriptors. */
struct starfire_rx_desc {
	dma_addr_t rxaddr;
};
enum rx_desc_bits {
	RxDescValid=1, RxDescEndRing=2,
};

/* Completion queue entry. */
struct short_rx_done_desc {
	u32 status;			/* Low 16 bits is length. */
};
struct basic_rx_done_desc {
	u32 status;			/* Low 16 bits is length. */
	u16 vlanid;
	u16 status2;
};
struct csum_rx_done_desc {
	u32 status;			/* Low 16 bits is length. */
	u16 csum;			/* Partial checksum */
	u16 status2;
};
struct full_rx_done_desc {
	u32 status;			/* Low 16 bits is length. */
	u16 status3;
	u16 status2;
	u16 vlanid;
	u16 csum;			/* partial checksum */
	u32 timestamp;
};
/* XXX: this is ugly and I'm not sure it's worth the trouble -Ion */
#ifdef HAS_FIRMWARE
#ifdef VLAN_SUPPORT
typedef struct full_rx_done_desc rx_done_desc;
#define RxComplType RxComplType3
#else  /* not VLAN_SUPPORT */
typedef struct csum_rx_done_desc rx_done_desc;
#define RxComplType RxComplType2
#endif /* not VLAN_SUPPORT */
#else  /* not HAS_FIRMWARE */
#ifdef VLAN_SUPPORT
typedef struct basic_rx_done_desc rx_done_desc;
#define RxComplType RxComplType1
#else  /* not VLAN_SUPPORT */
typedef struct short_rx_done_desc rx_done_desc;
#define RxComplType RxComplType0
#endif /* not VLAN_SUPPORT */
#endif /* not HAS_FIRMWARE */

enum rx_done_bits {
	RxOK=0x20000000, RxFIFOErr=0x10000000, RxBufQ2=0x08000000,
};

/* Type 1 Tx descriptor. */
struct starfire_tx_desc_1 {
	u32 status;			/* Upper bits are status, lower 16 length. */
	u32 addr;
};

/* Type 2 Tx descriptor. */
struct starfire_tx_desc_2 {
	u32 status;			/* Upper bits are status, lower 16 length. */
	u32 reserved;
	u64 addr;
};

#ifdef ADDR_64BITS
typedef struct starfire_tx_desc_2 starfire_tx_desc;
#define TX_DESC_TYPE TxDescType2
#else  /* not ADDR_64BITS */
typedef struct starfire_tx_desc_1 starfire_tx_desc;
#define TX_DESC_TYPE TxDescType1
#endif /* not ADDR_64BITS */
#define TX_DESC_SPACING TxDescSpaceUnlim

enum tx_desc_bits {
	TxDescID=0xB0000000,
	TxCRCEn=0x01000000, TxDescIntr=0x08000000,
	TxRingWrap=0x04000000, TxCalTCP=0x02000000,
};
struct tx_done_desc {
	u32 status;			/* timestamp, index. */
#if 0
	u32 intrstatus;			/* interrupt status */
#endif
};

struct rx_ring_info {
	struct sk_buff *skb;
	dma_addr_t mapping;
};
struct tx_ring_info {
	struct sk_buff *skb;
	dma_addr_t mapping;
	unsigned int used_slots;
};

#define PHY_CNT		2
struct netdev_private {
	/* Descriptor rings first for alignment. */
	struct starfire_rx_desc *rx_ring;
	starfire_tx_desc *tx_ring;
	dma_addr_t rx_ring_dma;
	dma_addr_t tx_ring_dma;
	/* The addresses of rx/tx-in-place skbuffs. */
	struct rx_ring_info rx_info[RX_RING_SIZE];
	struct tx_ring_info tx_info[TX_RING_SIZE];
	/* Pointers to completion queues (full pages). */
	rx_done_desc *rx_done_q;
	dma_addr_t rx_done_q_dma;
	unsigned int rx_done;
	struct tx_done_desc *tx_done_q;
	dma_addr_t tx_done_q_dma;
	unsigned int tx_done;
	struct net_device_stats stats;
	struct pci_dev *pci_dev;
#ifdef VLAN_SUPPORT
	struct vlan_group *vlgrp;
#endif
	void *queue_mem;
	dma_addr_t queue_mem_dma;
	size_t queue_mem_size;

	/* Frequently used values: keep some adjacent for cache effect. */
	spinlock_t lock;
	unsigned int cur_rx, dirty_rx;	/* Producer/consumer ring indices */
	unsigned int cur_tx, dirty_tx, reap_tx;
	unsigned int rx_buf_sz;		/* Based on MTU+slack. */
	/* These values keep track of the transceiver/media in use. */
	int speed100;			/* Set if speed == 100MBit. */
	u32 tx_mode;
	u32 intr_timer_ctrl;
	u8 tx_threshold;
	/* MII transceiver section. */
	struct mii_if_info mii_if;		/* MII lib hooks/info */
	int phy_cnt;			/* MII device addresses. */
	unsigned char phys[PHY_CNT];	/* MII device addresses. */
};


static int	mdio_read(struct net_device *dev, int phy_id, int location);
static void	mdio_write(struct net_device *dev, int phy_id, int location, int value);
static int	netdev_open(struct net_device *dev);
static void	check_duplex(struct net_device *dev);
static void	tx_timeout(struct net_device *dev);
static void	init_ring(struct net_device *dev);
static int	start_tx(struct sk_buff *skb, struct net_device *dev);
static void	intr_handler(int irq, void *dev_instance, struct pt_regs *regs);
static void	netdev_error(struct net_device *dev, int intr_status);
static int	netdev_rx(struct net_device *dev);
static void	netdev_error(struct net_device *dev, int intr_status);
static void	set_rx_mode(struct net_device *dev);
static struct net_device_stats *get_stats(struct net_device *dev);
static int	netdev_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
static int	netdev_close(struct net_device *dev);
static void	netdev_media_change(struct net_device *dev);


#ifdef VLAN_SUPPORT
static void netdev_vlan_rx_register(struct net_device *dev, struct vlan_group *grp)
{
        struct netdev_private *np = dev->priv;

        spin_lock(&np->lock);
	if (debug > 2)
		printk("%s: Setting vlgrp to %p\n", dev->name, grp);
        np->vlgrp = grp;
	set_rx_mode(dev);
        spin_unlock(&np->lock);
}

static void netdev_vlan_rx_add_vid(struct net_device *dev, unsigned short vid)
{
	struct netdev_private *np = dev->priv;

	spin_lock(&np->lock);
	if (debug > 1)
		printk("%s: Adding vlanid %d to vlan filter\n", dev->name, vid);
	set_rx_mode(dev);
	spin_unlock(&np->lock);
}

static void netdev_vlan_rx_kill_vid(struct net_device *dev, unsigned short vid)
{
	struct netdev_private *np = dev->priv;

	spin_lock(&np->lock);
	if (debug > 1)
		printk("%s: removing vlanid %d from vlan filter\n", dev->name, vid);
	if (np->vlgrp)
		np->vlgrp->vlan_devices[vid] = NULL;
	set_rx_mode(dev);
	spin_unlock(&np->lock);
}
#endif /* VLAN_SUPPORT */


static int __devinit starfire_init_one(struct pci_dev *pdev,
				       const struct pci_device_id *ent)
{
	struct netdev_private *np;
	int i, irq, option, chip_idx = ent->driver_data;
	struct net_device *dev;
	static int card_idx = -1;
	long ioaddr;
	int drv_flags, io_size;
	int boguscnt;

/* when built into the kernel, we only print version if device is found */
#ifndef MODULE
	static int printed_version;
	if (!printed_version++)
		printk(version);
#endif

	card_idx++;

	if (pci_enable_device (pdev))
		return -EIO;

	ioaddr = pci_resource_start(pdev, 0);
	io_size = pci_resource_len(pdev, 0);
	if (!ioaddr || ((pci_resource_flags(pdev, 0) & IORESOURCE_MEM) == 0)) {
		printk(KERN_ERR DRV_NAME " %d: no PCI MEM resources, aborting\n", card_idx);
		return -ENODEV;
	}

	dev = alloc_etherdev(sizeof(*np));
	if (!dev) {
		printk(KERN_ERR DRV_NAME " %d: cannot alloc etherdev, aborting\n", card_idx);
		return -ENOMEM;
	}
	SET_MODULE_OWNER(dev);

	irq = pdev->irq;

	if (pci_request_regions (pdev, dev->name)) {
		printk(KERN_ERR DRV_NAME " %d: cannot reserve PCI resources, aborting\n", card_idx);
		goto err_out_free_netdev;
	}

	/* ioremap is borken in Linux-2.2.x/sparc64 */
#if !defined(CONFIG_SPARC64) || LINUX_VERSION_CODE > 0x20300
	ioaddr = (long) ioremap(ioaddr, io_size);
	if (!ioaddr) {
		printk(KERN_ERR DRV_NAME " %d: cannot remap %#x @ %#lx, aborting\n",
			card_idx, io_size, ioaddr);
		goto err_out_free_res;
	}
#endif /* !CONFIG_SPARC64 || Linux 2.3.0+ */

	pci_set_master(pdev);

	/* enable MWI -- it vastly improves Rx performance on sparc64 */
	pci_set_mwi(pdev);

#ifdef MAX_SKB_FRAGS
	dev->features |= NETIF_F_SG;
#endif /* MAX_SKB_FRAGS */
#ifdef ZEROCOPY
	/* Starfire can do TCP/UDP checksumming */
	if (enable_hw_cksum)
		dev->features |= NETIF_F_IP_CSUM;
#endif /* ZEROCOPY */
#ifdef VLAN_SUPPORT
	dev->features |= NETIF_F_HW_VLAN_RX | NETIF_F_HW_VLAN_FILTER;
	dev->vlan_rx_register = netdev_vlan_rx_register;
	dev->vlan_rx_add_vid = netdev_vlan_rx_add_vid;
	dev->vlan_rx_kill_vid = netdev_vlan_rx_kill_vid;
#endif /* VLAN_RX_KILL_VID */
#ifdef ADDR_64BITS
	dev->features |= NETIF_F_HIGHDMA;
#endif /* ADDR_64BITS */

	/* Serial EEPROM reads are hidden by the hardware. */
	for (i = 0; i < 6; i++)
		dev->dev_addr[i] = readb(ioaddr + EEPROMCtrl + 20 - i);

#if ! defined(final_version) /* Dump the EEPROM contents during development. */
	if (debug > 4)
		for (i = 0; i < 0x20; i++)
			printk("%2.2x%s",
			       (unsigned int)readb(ioaddr + EEPROMCtrl + i),
			       i % 16 != 15 ? " " : "\n");
#endif

	/* Issue soft reset */
	writel(MiiSoftReset, ioaddr + TxMode);
	udelay(1000);
	writel(0, ioaddr + TxMode);

	/* Reset the chip to erase previous misconfiguration. */
	writel(1, ioaddr + PCIDeviceConfig);
	boguscnt = 1000;
	while (--boguscnt > 0) {
		udelay(10);
		if ((readl(ioaddr + PCIDeviceConfig) & 1) == 0)
			break;
	}
	if (boguscnt == 0)
		printk("%s: chipset reset never completed!\n", dev->name);
	/* wait a little longer */
	udelay(1000);

	dev->base_addr = ioaddr;
	dev->irq = irq;

	np = dev->priv;
	spin_lock_init(&np->lock);
	pci_set_drvdata(pdev, dev);

	np->pci_dev = pdev;

	np->mii_if.dev = dev;
	np->mii_if.mdio_read = mdio_read;
	np->mii_if.mdio_write = mdio_write;
	np->mii_if.phy_id_mask = 0x1f;
	np->mii_if.reg_num_mask = 0x1f;

	drv_flags = netdrv_tbl[chip_idx].drv_flags;

	option = card_idx < MAX_UNITS ? options[card_idx] : 0;
	if (dev->mem_start)
		option = dev->mem_start;

	/* The lower four bits are the media type. */
	if (option & 0x200)
		np->mii_if.full_duplex = 1;

	if (card_idx < MAX_UNITS && full_duplex[card_idx] > 0)
		np->mii_if.full_duplex = 1;

	if (np->mii_if.full_duplex)
		np->mii_if.force_media = 1;
	else
		np->mii_if.force_media = 0;
	np->speed100 = 1;

	/* timer resolution is 128 * 0.8us */
	np->intr_timer_ctrl = (((intr_latency * 10) / 1024) & IntrLatencyMask) |
		Timer10X | EnableIntrMasking;

	if (small_frames > 0) {
		np->intr_timer_ctrl |= SmallFrameBypass;
		switch (small_frames) {
		case 1 ... 64:
			np->intr_timer_ctrl |= SmallFrame64;
			break;
		case 65 ... 128:
			np->intr_timer_ctrl |= SmallFrame128;
			break;
		case 129 ... 256:
			np->intr_timer_ctrl |= SmallFrame256;
			break;
		default:
			np->intr_timer_ctrl |= SmallFrame512;
			if (small_frames > 512)
				printk("Adjusting small_frames down to 512\n");
			break;
		}
	}

	/* The chip-specific entries in the device structure. */
	dev->open = &netdev_open;
	dev->hard_start_xmit = &start_tx;
	init_tx_timer(dev, tx_timeout, TX_TIMEOUT);
	dev->stop = &netdev_close;
	dev->get_stats = &get_stats;
	dev->set_multicast_list = &set_rx_mode;
	dev->do_ioctl = &netdev_ioctl;

	if (mtu)
		dev->mtu = mtu;

	if (register_netdev(dev))
		goto err_out_cleardev;

	printk(KERN_INFO "%s: %s at %#lx, ",
		   dev->name, netdrv_tbl[chip_idx].name, ioaddr);
	for (i = 0; i < 5; i++)
		printk("%2.2x:", dev->dev_addr[i]);
	printk("%2.2x, IRQ %d.\n", dev->dev_addr[i], irq);

	if (drv_flags & CanHaveMII) {
		int phy, phy_idx = 0;
		int mii_status;
		for (phy = 0; phy < 32 && phy_idx < PHY_CNT; phy++) {
			mdio_write(dev, phy, MII_BMCR, BMCR_RESET);
			mdelay(100);
			boguscnt = 1000;
			while (--boguscnt > 0)
				if ((mdio_read(dev, phy, MII_BMCR) & BMCR_RESET) == 0)
					break;
			if (boguscnt == 0) {
				printk("%s: PHY reset never completed!\n", dev->name);
				continue;
			}
			mii_status = mdio_read(dev, phy, MII_BMSR);
			if (mii_status != 0) {
				np->phys[phy_idx++] = phy;
				np->mii_if.advertising = mdio_read(dev, phy, MII_ADVERTISE);
				printk(KERN_INFO "%s: MII PHY found at address %d, status "
					   "%#4.4x advertising %#4.4x.\n",
					   dev->name, phy, mii_status, np->mii_if.advertising);
				/* there can be only one PHY on-board */
				break;
			}
		}
		np->phy_cnt = phy_idx;
		if (np->phy_cnt > 0)
			np->mii_if.phy_id = np->phys[0];
		else
			memset(&np->mii_if, 0, sizeof(np->mii_if));
	}

	printk(KERN_INFO "%s: scatter-gather and hardware TCP cksumming %s.\n",
	       dev->name, enable_hw_cksum ? "enabled" : "disabled");
	return 0;

err_out_cleardev:
	pci_set_drvdata(pdev, NULL);
	iounmap((void *)ioaddr);
err_out_free_res:
	pci_release_regions (pdev);
err_out_free_netdev:
	kfree(dev);
	return -ENODEV;
}


/* Read the MII Management Data I/O (MDIO) interfaces. */
static int mdio_read(struct net_device *dev, int phy_id, int location)
{
	long mdio_addr = dev->base_addr + MIICtrl + (phy_id<<7) + (location<<2);
	int result, boguscnt=1000;
	/* ??? Should we add a busy-wait here? */
	do
		result = readl(mdio_addr);
	while ((result & 0xC0000000) != 0x80000000 && --boguscnt > 0);
	if (boguscnt == 0)
		return 0;
	if ((result & 0xffff) == 0xffff)
		return 0;
	return result & 0xffff;
}


static void mdio_write(struct net_device *dev, int phy_id, int location, int value)
{
	long mdio_addr = dev->base_addr + MIICtrl + (phy_id<<7) + (location<<2);
	writel(value, mdio_addr);
	/* The busy-wait will occur before a read. */
}


static int netdev_open(struct net_device *dev)
{
	struct netdev_private *np = dev->priv;
	long ioaddr = dev->base_addr;
	int i, retval;
	size_t tx_done_q_size, rx_done_q_size, tx_ring_size, rx_ring_size;

	/* Do we ever need to reset the chip??? */

	COMPAT_MOD_INC_USE_COUNT;

	retval = request_irq(dev->irq, &intr_handler, SA_SHIRQ, dev->name, dev);
	if (retval) {
		COMPAT_MOD_DEC_USE_COUNT;
		return retval;
	}

	/* Disable the Rx and Tx, and reset the chip. */
	writel(0, ioaddr + GenCtrl);
	writel(1, ioaddr + PCIDeviceConfig);
	if (debug > 1)
		printk(KERN_DEBUG "%s: netdev_open() irq %d.\n",
		       dev->name, dev->irq);

	/* Allocate the various queues. */
	if (np->queue_mem == 0) {
		tx_done_q_size = ((sizeof(struct tx_done_desc) * DONE_Q_SIZE + QUEUE_ALIGN - 1) / QUEUE_ALIGN) * QUEUE_ALIGN;
		rx_done_q_size = ((sizeof(rx_done_desc) * DONE_Q_SIZE + QUEUE_ALIGN - 1) / QUEUE_ALIGN) * QUEUE_ALIGN;
		tx_ring_size = ((sizeof(starfire_tx_desc) * TX_RING_SIZE + QUEUE_ALIGN - 1) / QUEUE_ALIGN) * QUEUE_ALIGN;
		rx_ring_size = sizeof(struct starfire_rx_desc) * RX_RING_SIZE;
		np->queue_mem_size = tx_done_q_size + rx_done_q_size + tx_ring_size + rx_ring_size;
		np->queue_mem = pci_alloc_consistent(np->pci_dev, np->queue_mem_size, &np->queue_mem_dma);
		if (np->queue_mem == 0) {
			COMPAT_MOD_DEC_USE_COUNT;
			return -ENOMEM;
		}

		np->tx_done_q     = np->queue_mem;
		np->tx_done_q_dma = np->queue_mem_dma;
		np->rx_done_q     = (void *) np->tx_done_q + tx_done_q_size;
		np->rx_done_q_dma = np->tx_done_q_dma + tx_done_q_size;
		np->tx_ring       = (void *) np->rx_done_q + rx_done_q_size;
		np->tx_ring_dma   = np->rx_done_q_dma + rx_done_q_size;
		np->rx_ring       = (void *) np->tx_ring + tx_ring_size;
		np->rx_ring_dma   = np->tx_ring_dma + tx_ring_size;
	}

	/* Start with no carrier, it gets adjusted later */
	netif_carrier_off(dev);
	init_ring(dev);
	/* Set the size of the Rx buffers. */
	writel((np->rx_buf_sz << RxBufferLenShift) |
	       (0 << RxMinDescrThreshShift) |
	       RxPrefetchMode | RxVariableQ |
	       RX_Q_ENTRIES |
	       RX_DESC_Q_ADDR_SIZE | RX_DESC_ADDR_SIZE |
	       RxDescSpace4,
	       ioaddr + RxDescQCtrl);

	/* Set up the Rx DMA controller. */
	writel(RxChecksumIgnore |
	       (0 << RxEarlyIntThreshShift) |
	       (6 << RxHighPrioThreshShift) |
	       ((DMA_BURST_SIZE / 32) << RxBurstSizeShift),
	       ioaddr + RxDMACtrl);

	/* Set Tx descriptor */
	writel((2 << TxHiPriFIFOThreshShift) |
	       (0 << TxPadLenShift) |
	       ((DMA_BURST_SIZE / 32) << TxDMABurstSizeShift) |
	       TX_DESC_Q_ADDR_SIZE |
	       TX_DESC_SPACING | TX_DESC_TYPE,
	       ioaddr + TxDescCtrl);

	writel( (np->queue_mem_dma >> 16) >> 16, ioaddr + RxDescQHiAddr);
	writel( (np->queue_mem_dma >> 16) >> 16, ioaddr + TxRingHiAddr);
	writel( (np->queue_mem_dma >> 16) >> 16, ioaddr + CompletionHiAddr);
	writel(np->rx_ring_dma, ioaddr + RxDescQAddr);
	writel(np->tx_ring_dma, ioaddr + TxRingPtr);

	writel(np->tx_done_q_dma, ioaddr + TxCompletionAddr);
	writel(np->rx_done_q_dma |
	       RxComplType |
	       (0 << RxComplThreshShift),
	       ioaddr + RxCompletionAddr);

	if (debug > 1)
		printk(KERN_DEBUG "%s: Filling in the station address.\n", dev->name);

	/* Fill both the Tx SA register and the Rx perfect filter. */
	for (i = 0; i < 6; i++)
		writeb(dev->dev_addr[i], ioaddr + TxStationAddr + 5 - i);
	/* The first entry is special because it bypasses the VLAN filter.
	   Don't use it. */
	writew(0, ioaddr + PerfFilterTable);
	writew(0, ioaddr + PerfFilterTable + 4);
	writew(0, ioaddr + PerfFilterTable + 8);
	for (i = 1; i < 16; i++) {
		u16 *eaddrs = (u16 *)dev->dev_addr;
		long setup_frm = ioaddr + PerfFilterTable + i * 16;
		writew(cpu_to_be16(eaddrs[2]), setup_frm); setup_frm += 4;
		writew(cpu_to_be16(eaddrs[1]), setup_frm); setup_frm += 4;
		writew(cpu_to_be16(eaddrs[0]), setup_frm); setup_frm += 8;
	}

	/* Initialize other registers. */
	/* Configure the PCI bus bursts and FIFO thresholds. */
	np->tx_mode = TxFlowEnable|RxFlowEnable|PadEnable;	/* modified when link is up. */
	writel(MiiSoftReset | np->tx_mode, ioaddr + TxMode);
	udelay(1000);
	writel(np->tx_mode, ioaddr + TxMode);
	np->tx_threshold = 4;
	writel(np->tx_threshold, ioaddr + TxThreshold);

	writel(np->intr_timer_ctrl, ioaddr + IntrTimerCtrl);

	netif_start_if(dev);
	netif_start_queue(dev);

	if (debug > 1)
		printk(KERN_DEBUG "%s: Setting the Rx and Tx modes.\n", dev->name);
	set_rx_mode(dev);

	np->mii_if.advertising = mdio_read(dev, np->phys[0], MII_ADVERTISE);
	check_duplex(dev);

	/* Enable GPIO interrupts on link change */
	writel(0x0f00ff00, ioaddr + GPIOCtrl);

	/* Set the interrupt mask */
	writel(IntrRxDone | IntrRxEmpty | IntrDMAErr |
	       IntrTxDMADone | IntrStatsMax | IntrLinkChange |
	       IntrRxGFPDead | IntrNoTxCsum | IntrTxBadID,
	       ioaddr + IntrEnable);
	/* Enable PCI interrupts. */
	writel(0x00800000 | readl(ioaddr + PCIDeviceConfig),
	       ioaddr + PCIDeviceConfig);

#ifdef VLAN_SUPPORT
	/* Set VLAN type to 802.1q */
	writel(ETH_P_8021Q, ioaddr + VlanType);
#endif /* VLAN_SUPPORT */

#ifdef HAS_FIRMWARE
	/* Load Rx/Tx firmware into the frame processors */
	for (i = 0; i < FIRMWARE_RX_SIZE * 2; i++)
		writel(firmware_rx[i], ioaddr + RxGfpMem + i * 4);
	for (i = 0; i < FIRMWARE_TX_SIZE * 2; i++)
		writel(firmware_tx[i], ioaddr + TxGfpMem + i * 4);
#endif /* HAS_FIRMWARE */
	if (enable_hw_cksum)
		/* Enable the Rx and Tx units, and the Rx/Tx frame processors. */
		writel(TxEnable|TxGFPEnable|RxEnable|RxGFPEnable, ioaddr + GenCtrl);
	else
		/* Enable the Rx and Tx units only. */
		writel(TxEnable|RxEnable, ioaddr + GenCtrl);

	if (debug > 1)
		printk(KERN_DEBUG "%s: Done netdev_open().\n",
		       dev->name);

	return 0;
}


static void check_duplex(struct net_device *dev)
{
	struct netdev_private *np = dev->priv;
	u16 reg0;
	int silly_count = 1000;

	mdio_write(dev, np->phys[0], MII_ADVERTISE, np->mii_if.advertising);
	mdio_write(dev, np->phys[0], MII_BMCR, BMCR_RESET);
	udelay(500);
	while (--silly_count && mdio_read(dev, np->phys[0], MII_BMCR) & BMCR_RESET)
		/* do nothing */;
	if (!silly_count) {
		printk("%s: MII reset failed!\n", dev->name);
		return;
	}

	reg0 = mdio_read(dev, np->phys[0], MII_BMCR);

	if (!np->mii_if.force_media) {
		reg0 |= BMCR_ANENABLE | BMCR_ANRESTART;
	} else {
		reg0 &= ~(BMCR_ANENABLE | BMCR_ANRESTART);
		if (np->speed100)
			reg0 |= BMCR_SPEED100;
		if (np->mii_if.full_duplex)
			reg0 |= BMCR_FULLDPLX;
		printk(KERN_DEBUG "%s: Link forced to %sMbit %s-duplex\n",
		       dev->name,
		       np->speed100 ? "100" : "10",
		       np->mii_if.full_duplex ? "full" : "half");
	}
	mdio_write(dev, np->phys[0], MII_BMCR, reg0);
}


static void tx_timeout(struct net_device *dev)
{
	struct netdev_private *np = dev->priv;
	long ioaddr = dev->base_addr;
	int old_debug;

	printk(KERN_WARNING "%s: Transmit timed out, status %#8.8x, "
	       "resetting...\n", dev->name, (int) readl(ioaddr + IntrStatus));

	/* Perhaps we should reinitialize the hardware here. */

	/*
	 * Stop and restart the interface.
	 * Cheat and increase the debug level temporarily.
	 */
	old_debug = debug;
	debug = 2;
	netdev_close(dev);
	netdev_open(dev);
	debug = old_debug;

	/* Trigger an immediate transmit demand. */

	dev->trans_start = jiffies;
	np->stats.tx_errors++;
	netif_wake_queue(dev);
}


/* Initialize the Rx and Tx rings, along with various 'dev' bits. */
static void init_ring(struct net_device *dev)
{
	struct netdev_private *np = dev->priv;
	int i;

	np->cur_rx = np->cur_tx = np->reap_tx = 0;
	np->dirty_rx = np->dirty_tx = np->rx_done = np->tx_done = 0;

	np->rx_buf_sz = (dev->mtu <= 1500 ? PKT_BUF_SZ : dev->mtu + 32);

	/* Fill in the Rx buffers.  Handle allocation failure gracefully. */
	for (i = 0; i < RX_RING_SIZE; i++) {
		struct sk_buff *skb = dev_alloc_skb(np->rx_buf_sz);
		np->rx_info[i].skb = skb;
		if (skb == NULL)
			break;
		np->rx_info[i].mapping = pci_map_single(np->pci_dev, skb->tail, np->rx_buf_sz, PCI_DMA_FROMDEVICE);
		skb->dev = dev;			/* Mark as being used by this device. */
		/* Grrr, we cannot offset to correctly align the IP header. */
		np->rx_ring[i].rxaddr = cpu_to_dma(np->rx_info[i].mapping | RxDescValid);
	}
	writew(i - 1, dev->base_addr + RxDescQIdx);
	np->dirty_rx = (unsigned int)(i - RX_RING_SIZE);

	/* Clear the remainder of the Rx buffer ring. */
	for (  ; i < RX_RING_SIZE; i++) {
		np->rx_ring[i].rxaddr = 0;
		np->rx_info[i].skb = NULL;
		np->rx_info[i].mapping = 0;
	}
	/* Mark the last entry as wrapping the ring. */
	np->rx_ring[RX_RING_SIZE - 1].rxaddr |= cpu_to_dma(RxDescEndRing);

	/* Clear the completion rings. */
	for (i = 0; i < DONE_Q_SIZE; i++) {
		np->rx_done_q[i].status = 0;
		np->tx_done_q[i].status = 0;
	}

	for (i = 0; i < TX_RING_SIZE; i++)
		memset(&np->tx_info[i], 0, sizeof(np->tx_info[i]));

	return;
}


static int start_tx(struct sk_buff *skb, struct net_device *dev)
{
	struct netdev_private *np = dev->priv;
	unsigned int entry;
	u32 status;
	int i;

	kick_tx_timer(dev, tx_timeout, TX_TIMEOUT);

	/*
	 * be cautious here, wrapping the queue has weird semantics
	 * and we may not have enough slots even when it seems we do.
	 */
	if ((np->cur_tx - np->dirty_tx) + skb_num_frags(skb) * 2 > TX_RING_SIZE) {
		netif_stop_queue(dev);
		return 1;
	}

#if defined(ZEROCOPY) && defined(HAS_BROKEN_FIRMWARE)
	{
		int has_bad_length = 0;

		if (skb_first_frag_len(skb) == 1)
			has_bad_length = 1;
		else {
			for (i = 0; i < skb_shinfo(skb)->nr_frags; i++)
				if (skb_shinfo(skb)->frags[i].size == 1) {
					has_bad_length = 1;
					break;
				}
		}

		if (has_bad_length)
			skb_checksum_help(skb);
	}
#endif /* ZEROCOPY && HAS_BROKEN_FIRMWARE */

	entry = np->cur_tx % TX_RING_SIZE;
	for (i = 0; i < skb_num_frags(skb); i++) {
		int wrap_ring = 0;
		status = TxDescID;

		if (i == 0) {
			np->tx_info[entry].skb = skb;
			status |= TxCRCEn;
			if (entry >= TX_RING_SIZE - skb_num_frags(skb)) {
				status |= TxRingWrap;
				wrap_ring = 1;
			}
			if (np->reap_tx) {
				status |= TxDescIntr;
				np->reap_tx = 0;
			}
			if (skb->ip_summed == CHECKSUM_HW) {
				status |= TxCalTCP;
				np->stats.tx_compressed++;
			}
			status |= skb_first_frag_len(skb) | (skb_num_frags(skb) << 16);

			np->tx_info[entry].mapping =
				pci_map_single(np->pci_dev, skb->data, skb_first_frag_len(skb), PCI_DMA_TODEVICE);
		} else {
#ifdef MAX_SKB_FRAGS
			skb_frag_t *this_frag = &skb_shinfo(skb)->frags[i - 1];
			status |= this_frag->size;
			np->tx_info[entry].mapping =
				pci_map_single(np->pci_dev, page_address(this_frag->page) + this_frag->page_offset, this_frag->size, PCI_DMA_TODEVICE);
#endif /* MAX_SKB_FRAGS */
		}

		np->tx_ring[entry].addr = cpu_to_dma(np->tx_info[entry].mapping);
		np->tx_ring[entry].status = cpu_to_le32(status);
		if (debug > 3)
			printk(KERN_DEBUG "%s: Tx #%d/#%d slot %d status %#8.8x.\n",
			       dev->name, np->cur_tx, np->dirty_tx,
			       entry, status);
		if (wrap_ring) {
			np->tx_info[entry].used_slots = TX_RING_SIZE - entry;
			np->cur_tx += np->tx_info[entry].used_slots;
			entry = 0;
		} else {
			np->tx_info[entry].used_slots = 1;
			np->cur_tx += np->tx_info[entry].used_slots;
			entry++;
		}
		/* scavenge the tx descriptors twice per TX_RING_SIZE */
		if (np->cur_tx % (TX_RING_SIZE / 2) == 0)
			np->reap_tx = 1;
	}

	/* Non-x86: explicitly flush descriptor cache lines here. */
	/* Ensure all descriptors are written back before the transmit is
	   initiated. - Jes */
	wmb();

	/* Update the producer index. */
	writel(entry * (sizeof(starfire_tx_desc) / 8), dev->base_addr + TxProducerIdx);

	/* 4 is arbitrary, but should be ok */
	if ((np->cur_tx - np->dirty_tx) + 4 > TX_RING_SIZE)
		netif_stop_queue(dev);

	dev->trans_start = jiffies;

	return 0;
}


/* The interrupt handler does all of the Rx thread work and cleans up
   after the Tx thread. */
static void intr_handler(int irq, void *dev_instance, struct pt_regs *rgs)
{
	struct net_device *dev = dev_instance;
	struct netdev_private *np;
	long ioaddr;
	int boguscnt = max_interrupt_work;
	int consumer;
	int tx_status;

	ioaddr = dev->base_addr;
	np = dev->priv;

	do {
		u32 intr_status = readl(ioaddr + IntrClear);

		if (debug > 4)
			printk(KERN_DEBUG "%s: Interrupt status %#8.8x.\n",
			       dev->name, intr_status);

		if (intr_status == 0 || intr_status == (u32) -1)
			break;

		if (intr_status & (IntrRxDone | IntrRxEmpty))
			netdev_rx(dev);

		/* Scavenge the skbuff list based on the Tx-done queue.
		   There are redundant checks here that may be cleaned up
		   after the driver has proven to be reliable. */
		consumer = readl(ioaddr + TxConsumerIdx);
		if (debug > 3)
			printk(KERN_DEBUG "%s: Tx Consumer index is %d.\n",
			       dev->name, consumer);

		while ((tx_status = le32_to_cpu(np->tx_done_q[np->tx_done].status)) != 0) {
			if (debug > 3)
				printk(KERN_DEBUG "%s: Tx completion #%d entry %d is %#8.8x.\n",
				       dev->name, np->dirty_tx, np->tx_done, tx_status);
			if ((tx_status & 0xe0000000) == 0xa0000000) {
				np->stats.tx_packets++;
			} else if ((tx_status & 0xe0000000) == 0x80000000) {
				u16 entry = (tx_status & 0x7fff) / sizeof(starfire_tx_desc);
				struct sk_buff *skb = np->tx_info[entry].skb;
				np->tx_info[entry].skb = NULL;
				pci_unmap_single(np->pci_dev,
						 np->tx_info[entry].mapping,
						 skb_first_frag_len(skb),
						 PCI_DMA_TODEVICE);
				np->tx_info[entry].mapping = 0;
				np->dirty_tx += np->tx_info[entry].used_slots;
				entry = (entry + np->tx_info[entry].used_slots) % TX_RING_SIZE;
#ifdef MAX_SKB_FRAGS
				{
					int i;
					for (i = 0; i < skb_shinfo(skb)->nr_frags; i++) {
						pci_unmap_single(np->pci_dev,
								 np->tx_info[entry].mapping,
								 skb_shinfo(skb)->frags[i].size,
								 PCI_DMA_TODEVICE);
						np->dirty_tx++;
						entry++;
					}
				}
#endif /* MAX_SKB_FRAGS */
				dev_kfree_skb_irq(skb);
			}
			np->tx_done_q[np->tx_done].status = 0;
			np->tx_done = (np->tx_done + 1) % DONE_Q_SIZE;
		}
		writew(np->tx_done, ioaddr + CompletionQConsumerIdx + 2);

		if (netif_queue_stopped(dev) &&
		    (np->cur_tx - np->dirty_tx + 4 < TX_RING_SIZE)) {
			/* The ring is no longer full, wake the queue. */
			netif_wake_queue(dev);
		}

		/* Stats overflow */
		if (intr_status & IntrStatsMax)
			get_stats(dev);

		/* Media change interrupt. */
		if (intr_status & IntrLinkChange)
			netdev_media_change(dev);

		/* Abnormal error summary/uncommon events handlers. */
		if (intr_status & IntrAbnormalSummary)
			netdev_error(dev, intr_status);

		if (--boguscnt < 0) {
			if (debug > 1)
				printk(KERN_WARNING "%s: Too much work at interrupt, "
				       "status=%#8.8x.\n",
				       dev->name, intr_status);
			break;
		}
	} while (1);

	if (debug > 4)
		printk(KERN_DEBUG "%s: exiting interrupt, status=%#8.8x.\n",
		       dev->name, (int) readl(ioaddr + IntrStatus));
}


/* This routine is logically part of the interrupt handler, but separated
   for clarity and better register allocation. */
static int netdev_rx(struct net_device *dev)
{
	struct netdev_private *np = dev->priv;
	int boguscnt = np->dirty_rx + RX_RING_SIZE - np->cur_rx;
	u32 desc_status;

	/* If EOP is set on the next entry, it's a new packet. Send it up. */
	while ((desc_status = le32_to_cpu(np->rx_done_q[np->rx_done].status)) != 0) {
		struct sk_buff *skb;
		u16 pkt_len;
		int entry;
		rx_done_desc *desc = &np->rx_done_q[np->rx_done];

		if (debug > 4)
			printk(KERN_DEBUG "  netdev_rx() status of %d was %#8.8x.\n", np->rx_done, desc_status);
		if (--boguscnt < 0)
			break;
		if (!(desc_status & RxOK)) {
			/* There was a error. */
			if (debug > 2)
				printk(KERN_DEBUG "  netdev_rx() Rx error was %#8.8x.\n", desc_status);
			np->stats.rx_errors++;
			if (desc_status & RxFIFOErr)
				np->stats.rx_fifo_errors++;
			goto next_rx;
		}

		pkt_len = desc_status;	/* Implicitly Truncate */
		entry = (desc_status >> 16) & 0x7ff;

		if (debug > 4)
			printk(KERN_DEBUG "  netdev_rx() normal Rx pkt length %d, bogus_cnt %d.\n", pkt_len, boguscnt);
		/* Check if the packet is long enough to accept without copying
		   to a minimally-sized skbuff. */
		if (pkt_len < rx_copybreak
		    && (skb = dev_alloc_skb(pkt_len + 2)) != NULL) {
			skb->dev = dev;
			skb_reserve(skb, 2);	/* 16 byte align the IP header */
			pci_dma_sync_single(np->pci_dev,
					    np->rx_info[entry].mapping,
					    pkt_len, PCI_DMA_FROMDEVICE);
			eth_copy_and_sum(skb, np->rx_info[entry].skb->tail, pkt_len, 0);
			skb_put(skb, pkt_len);
		} else {
			pci_unmap_single(np->pci_dev, np->rx_info[entry].mapping, np->rx_buf_sz, PCI_DMA_FROMDEVICE);
			skb = np->rx_info[entry].skb;
			skb_put(skb, pkt_len);
			np->rx_info[entry].skb = NULL;
			np->rx_info[entry].mapping = 0;
		}
#ifndef final_version			/* Remove after testing. */
		/* You will want this info for the initial debug. */
		if (debug > 5)
			printk(KERN_DEBUG "  Rx data %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:"
			       "%2.2x %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x %2.2x%2.2x.\n",
			       skb->data[0], skb->data[1], skb->data[2], skb->data[3],
			       skb->data[4], skb->data[5], skb->data[6], skb->data[7],
			       skb->data[8], skb->data[9], skb->data[10],
			       skb->data[11], skb->data[12], skb->data[13]);
#endif

		skb->protocol = eth_type_trans(skb, dev);
#if defined(HAS_FIRMWARE) || defined(VLAN_SUPPORT)
		if (debug > 4)
			printk(KERN_DEBUG "  netdev_rx() status2 of %d was %#4.4x.\n", np->rx_done, le16_to_cpu(desc->status2));
#endif
#ifdef HAS_FIRMWARE
		if (le16_to_cpu(desc->status2) & 0x0100) {
			skb->ip_summed = CHECKSUM_UNNECESSARY;
			np->stats.rx_compressed++;
		}
		/*
		 * This feature doesn't seem to be working, at least
		 * with the two firmware versions I have. If the GFP sees
		 * an IP fragment, it either ignores it completely, or reports
		 * "bad checksum" on it.
		 *
		 * Maybe I missed something -- corrections are welcome.
		 * Until then, the printk stays. :-) -Ion
		 */
		else if (le16_to_cpu(desc->status2) & 0x0040) {
			skb->ip_summed = CHECKSUM_HW;
			skb->csum = le16_to_cpu(desc->csum);
			printk(KERN_DEBUG "%s: checksum_hw, status2 = %#x\n", dev->name, le16_to_cpu(desc->status2));
		}
#endif /* HAS_FIRMWARE */
#ifdef VLAN_SUPPORT
		if (np->vlgrp && le16_to_cpu(desc->status2) & 0x0200) {
			if (debug > 4)
				printk(KERN_DEBUG "  netdev_rx() vlanid = %d\n", le16_to_cpu(desc->vlanid));
			/* vlan_hwaccel_rx expects a packet with the VLAN tag stripped out */
			vlan_hwaccel_rx(skb, np->vlgrp, le16_to_cpu(desc->vlanid) & VLAN_VID_MASK);
		} else
#endif /* VLAN_SUPPORT */
			netif_rx(skb);
		dev->last_rx = jiffies;
		np->stats.rx_packets++;

	next_rx:
		np->cur_rx++;
		desc->status = 0;
		np->rx_done = (np->rx_done + 1) % DONE_Q_SIZE;
	}
	writew(np->rx_done, dev->base_addr + CompletionQConsumerIdx);

	/* Refill the Rx ring buffers. */
	for (; np->cur_rx - np->dirty_rx > 0; np->dirty_rx++) {
		struct sk_buff *skb;
		int entry = np->dirty_rx % RX_RING_SIZE;
		if (np->rx_info[entry].skb == NULL) {
			skb = dev_alloc_skb(np->rx_buf_sz);
			np->rx_info[entry].skb = skb;
			if (skb == NULL)
				break;	/* Better luck next round. */
			np->rx_info[entry].mapping =
				pci_map_single(np->pci_dev, skb->tail, np->rx_buf_sz, PCI_DMA_FROMDEVICE);
			skb->dev = dev;	/* Mark as being used by this device. */
			np->rx_ring[entry].rxaddr =
				cpu_to_dma(np->rx_info[entry].mapping | RxDescValid);
		}
		if (entry == RX_RING_SIZE - 1)
			np->rx_ring[entry].rxaddr |= cpu_to_dma(RxDescEndRing);
		/* We could defer this until later... */
		writew(entry, dev->base_addr + RxDescQIdx);
	}

	if (debug > 5)
		printk(KERN_DEBUG "  exiting netdev_rx() status of %d was %#8.8x.\n",
		       np->rx_done, desc_status);

	/* Restart Rx engine if stopped. */
	return 0;
}


static void netdev_media_change(struct net_device *dev)
{
	struct netdev_private *np = dev->priv;
	long ioaddr = dev->base_addr;
	u16 reg0, reg1, reg4, reg5;
	u32 new_tx_mode;
	u32 new_intr_timer_ctrl;

	/* reset status first */
	mdio_read(dev, np->phys[0], MII_BMCR);
	mdio_read(dev, np->phys[0], MII_BMSR);

	reg0 = mdio_read(dev, np->phys[0], MII_BMCR);
	reg1 = mdio_read(dev, np->phys[0], MII_BMSR);

	if (reg1 & BMSR_LSTATUS) {
		/* link is up */
		if (reg0 & BMCR_ANENABLE) {
			/* autonegotiation is enabled */
			reg4 = mdio_read(dev, np->phys[0], MII_ADVERTISE);
			reg5 = mdio_read(dev, np->phys[0], MII_LPA);
			if (reg4 & ADVERTISE_100FULL && reg5 & LPA_100FULL) {
				np->speed100 = 1;
				np->mii_if.full_duplex = 1;
			} else if (reg4 & ADVERTISE_100HALF && reg5 & LPA_100HALF) {
				np->speed100 = 1;
				np->mii_if.full_duplex = 0;
			} else if (reg4 & ADVERTISE_10FULL && reg5 & LPA_10FULL) {
				np->speed100 = 0;
				np->mii_if.full_duplex = 1;
			} else {
				np->speed100 = 0;
				np->mii_if.full_duplex = 0;
			}
		} else {
			/* autonegotiation is disabled */
			if (reg0 & BMCR_SPEED100)
				np->speed100 = 1;
			else
				np->speed100 = 0;
			if (reg0 & BMCR_FULLDPLX)
				np->mii_if.full_duplex = 1;
			else
				np->mii_if.full_duplex = 0;
		}
		netif_carrier_on(dev);
		printk(KERN_DEBUG "%s: Link is up, running at %sMbit %s-duplex\n",
		       dev->name,
		       np->speed100 ? "100" : "10",
		       np->mii_if.full_duplex ? "full" : "half");

		new_tx_mode = np->tx_mode & ~FullDuplex;	/* duplex setting */
		if (np->mii_if.full_duplex)
			new_tx_mode |= FullDuplex;
		if (np->tx_mode != new_tx_mode) {
			np->tx_mode = new_tx_mode;
			writel(np->tx_mode | MiiSoftReset, ioaddr + TxMode);
			udelay(1000);
			writel(np->tx_mode, ioaddr + TxMode);
		}

		new_intr_timer_ctrl = np->intr_timer_ctrl & ~Timer10X;
		if (np->speed100)
			new_intr_timer_ctrl |= Timer10X;
		if (np->intr_timer_ctrl != new_intr_timer_ctrl) {
			np->intr_timer_ctrl = new_intr_timer_ctrl;
			writel(new_intr_timer_ctrl, ioaddr + IntrTimerCtrl);
		}
	} else {
		netif_carrier_off(dev);
		printk(KERN_DEBUG "%s: Link is down\n", dev->name);
	}
}


static void netdev_error(struct net_device *dev, int intr_status)
{
	struct netdev_private *np = dev->priv;

	/* Came close to underrunning the Tx FIFO, increase threshold. */
	if (intr_status & IntrTxDataLow) {
		if (np->tx_threshold <= PKT_BUF_SZ / 16) {
			writel(++np->tx_threshold, dev->base_addr + TxThreshold);
			printk(KERN_NOTICE "%s: PCI bus congestion, increasing Tx FIFO threshold to %d bytes\n",
			       dev->name, np->tx_threshold * 16);
		} else
			printk(KERN_WARNING "%s: PCI Tx underflow -- adapter is probably malfunctioning\n", dev->name);
	}
	if (intr_status & IntrRxGFPDead) {
		np->stats.rx_fifo_errors++;
		np->stats.rx_errors++;
	}
	if (intr_status & (IntrNoTxCsum | IntrDMAErr)) {
		np->stats.tx_fifo_errors++;
		np->stats.tx_errors++;
	}
	if ((intr_status & ~(IntrNormalMask | IntrAbnormalSummary | IntrLinkChange | IntrStatsMax | IntrTxDataLow | IntrRxGFPDead | IntrNoTxCsum | IntrPCIPad)) && debug)
		printk(KERN_ERR "%s: Something Wicked happened! %#8.8x.\n",
		       dev->name, intr_status);
}


static struct net_device_stats *get_stats(struct net_device *dev)
{
	long ioaddr = dev->base_addr;
	struct netdev_private *np = dev->priv;

	/* This adapter architecture needs no SMP locks. */
	np->stats.tx_bytes = readl(ioaddr + 0x57010);
	np->stats.rx_bytes = readl(ioaddr + 0x57044);
	np->stats.tx_packets = readl(ioaddr + 0x57000);
	np->stats.tx_aborted_errors =
		readl(ioaddr + 0x57024) + readl(ioaddr + 0x57028);
	np->stats.tx_window_errors = readl(ioaddr + 0x57018);
	np->stats.collisions =
		readl(ioaddr + 0x57004) + readl(ioaddr + 0x57008);

	/* The chip only need report frame silently dropped. */
	np->stats.rx_dropped += readw(ioaddr + RxDMAStatus);
	writew(0, ioaddr + RxDMAStatus);
	np->stats.rx_crc_errors = readl(ioaddr + 0x5703C);
	np->stats.rx_frame_errors = readl(ioaddr + 0x57040);
	np->stats.rx_length_errors = readl(ioaddr + 0x57058);
	np->stats.rx_missed_errors = readl(ioaddr + 0x5707C);

	return &np->stats;
}


/* Chips may use the upper or lower CRC bits, and may reverse and/or invert
   them.  Select the endian-ness that results in minimal calculations.
*/
static void set_rx_mode(struct net_device *dev)
{
	long ioaddr = dev->base_addr;
	u32 rx_mode = MinVLANPrio;
	struct dev_mc_list *mclist;
	int i;
#ifdef VLAN_SUPPORT
	struct netdev_private *np = dev->priv;

	rx_mode |= VlanMode;
	if (np->vlgrp) {
		int vlan_count = 0;
		long filter_addr = ioaddr + HashTable + 8;
		for (i = 0; i < VLAN_VID_MASK; i++) {
			if (np->vlgrp->vlan_devices[i]) {
				if (vlan_count >= 32)
					break;
				writew(cpu_to_be16(i), filter_addr);
				filter_addr += 16;
				vlan_count++;
			}
		}
		if (i == VLAN_VID_MASK) {
			rx_mode |= PerfectFilterVlan;
			while (vlan_count < 32) {
				writew(0, filter_addr);
				filter_addr += 16;
				vlan_count++;
			}
		}
	}
#endif /* VLAN_SUPPORT */

	if (dev->flags & IFF_PROMISC) {	/* Set promiscuous. */
		rx_mode |= AcceptAll;
	} else if ((dev->mc_count > multicast_filter_limit)
		   || (dev->flags & IFF_ALLMULTI)) {
		/* Too many to match, or accept all multicasts. */
		rx_mode |= AcceptBroadcast|AcceptAllMulticast|PerfectFilter;
	} else if (dev->mc_count <= 14) {
		/* Use the 16 element perfect filter, skip first two entries. */
		long filter_addr = ioaddr + PerfFilterTable + 2 * 16;
		u16 *eaddrs;
		for (i = 2, mclist = dev->mc_list; mclist && i < dev->mc_count + 2;
		     i++, mclist = mclist->next) {
			eaddrs = (u16 *)mclist->dmi_addr;
			writew(cpu_to_be16(eaddrs[2]), filter_addr); filter_addr += 4;
			writew(cpu_to_be16(eaddrs[1]), filter_addr); filter_addr += 4;
			writew(cpu_to_be16(eaddrs[0]), filter_addr); filter_addr += 8;
		}
		eaddrs = (u16 *)dev->dev_addr;
		while (i++ < 16) {
			writew(cpu_to_be16(eaddrs[0]), filter_addr); filter_addr += 4;
			writew(cpu_to_be16(eaddrs[1]), filter_addr); filter_addr += 4;
			writew(cpu_to_be16(eaddrs[2]), filter_addr); filter_addr += 8;
		}
		rx_mode |= AcceptBroadcast|PerfectFilter;
	} else {
		/* Must use a multicast hash table. */
		long filter_addr;
		u16 *eaddrs;
		u16 mc_filter[32] __attribute__ ((aligned(sizeof(long))));	/* Multicast hash filter */

		memset(mc_filter, 0, sizeof(mc_filter));
		for (i = 0, mclist = dev->mc_list; mclist && i < dev->mc_count;
		     i++, mclist = mclist->next) {
			int bit_nr = ether_crc_le(ETH_ALEN, mclist->dmi_addr) >> 23;
			__u32 *fptr = (__u32 *) &mc_filter[(bit_nr >> 4) & ~1];

			*fptr |= cpu_to_le32(1 << (bit_nr & 31));
		}
		/* Clear the perfect filter list, skip first two entries. */
		filter_addr = ioaddr + PerfFilterTable + 2 * 16;
		eaddrs = (u16 *)dev->dev_addr;
		for (i = 2; i < 16; i++) {
			writew(cpu_to_be16(eaddrs[0]), filter_addr); filter_addr += 4;
			writew(cpu_to_be16(eaddrs[1]), filter_addr); filter_addr += 4;
			writew(cpu_to_be16(eaddrs[2]), filter_addr); filter_addr += 8;
		}
		for (filter_addr = ioaddr + HashTable, i = 0; i < 32; filter_addr+= 16, i++)
			writew(mc_filter[i], filter_addr);
		rx_mode |= AcceptBroadcast|PerfectFilter|HashFilter;
	}
	writel(rx_mode, ioaddr + RxFilterMode);
}


static int netdev_ethtool_ioctl(struct net_device *dev, void *useraddr)
{
	struct ethtool_cmd ecmd;
	struct netdev_private *np = dev->priv;

	if (copy_from_user(&ecmd, useraddr, sizeof(ecmd)))
		return -EFAULT;

	switch (ecmd.cmd) {
	case ETHTOOL_GDRVINFO: {
		struct ethtool_drvinfo info;
		memset(&info, 0, sizeof(info));
		info.cmd = ecmd.cmd;
		strcpy(info.driver, DRV_NAME);
		strcpy(info.version, DRV_VERSION);
		*info.fw_version = 0;
		strcpy(info.bus_info, PCI_SLOT_NAME(np->pci_dev));
		if (copy_to_user(useraddr, &info, sizeof(info)))
		       return -EFAULT;
		return 0;
	}

	/* get settings */
	case ETHTOOL_GSET: {
		struct ethtool_cmd ecmd = { ETHTOOL_GSET };
		spin_lock_irq(&np->lock);
		mii_ethtool_gset(&np->mii_if, &ecmd);
		spin_unlock_irq(&np->lock);
		if (copy_to_user(useraddr, &ecmd, sizeof(ecmd)))
			return -EFAULT;
		return 0;
	}
	/* set settings */
	case ETHTOOL_SSET: {
		int r;
		struct ethtool_cmd ecmd;
		if (copy_from_user(&ecmd, useraddr, sizeof(ecmd)))
			return -EFAULT;
		spin_lock_irq(&np->lock);
		r = mii_ethtool_sset(&np->mii_if, &ecmd);
		spin_unlock_irq(&np->lock);
		check_duplex(dev);
		return r;
	}
	/* restart autonegotiation */
	case ETHTOOL_NWAY_RST: {
		return mii_nway_restart(&np->mii_if);
	}
	/* get link status */
	case ETHTOOL_GLINK: {
		struct ethtool_value edata = {ETHTOOL_GLINK};
		edata.data = mii_link_ok(&np->mii_if);
		if (copy_to_user(useraddr, &edata, sizeof(edata)))
			return -EFAULT;
		return 0;
	}

	/* get message-level */
	case ETHTOOL_GMSGLVL: {
		struct ethtool_value edata = {ETHTOOL_GMSGLVL};
		edata.data = debug;
		if (copy_to_user(useraddr, &edata, sizeof(edata)))
			return -EFAULT;
		return 0;
	}
	/* set message-level */
	case ETHTOOL_SMSGLVL: {
		struct ethtool_value edata;
		if (copy_from_user(&edata, useraddr, sizeof(edata)))
			return -EFAULT;
		debug = edata.data;
		return 0;
	}
	default:
		return -EOPNOTSUPP;
	}
}


static int netdev_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct netdev_private *np = dev->priv;
	struct mii_ioctl_data *data = (struct mii_ioctl_data *) & rq->ifr_data;
	int rc;

	if (!netif_running(dev))
		return -EINVAL;

	if (cmd == SIOCETHTOOL)
		rc = netdev_ethtool_ioctl(dev, (void *) rq->ifr_data);

	else {
		spin_lock_irq(&np->lock);
		rc = generic_mii_ioctl(&np->mii_if, data, cmd, NULL);
		spin_unlock_irq(&np->lock);

		if ((cmd == SIOCSMIIREG) && (data->phy_id == np->phys[0]))
			check_duplex(dev);
	}

	return rc;
}

static int netdev_close(struct net_device *dev)
{
	long ioaddr = dev->base_addr;
	struct netdev_private *np = dev->priv;
	int i;

	netif_stop_queue(dev);
	netif_stop_if(dev);

	if (debug > 1) {
		printk(KERN_DEBUG "%s: Shutting down ethercard, Intr status %#8.8x.\n",
			   dev->name, (int) readl(ioaddr + IntrStatus));
		printk(KERN_DEBUG "%s: Queue pointers were Tx %d / %d, Rx %d / %d.\n",
		       dev->name, np->cur_tx, np->dirty_tx,
		       np->cur_rx, np->dirty_rx);
	}

	/* Disable interrupts by clearing the interrupt mask. */
	writel(0, ioaddr + IntrEnable);

	/* Stop the chip's Tx and Rx processes. */
	writel(0, ioaddr + GenCtrl);
	readl(ioaddr + GenCtrl);

	if (debug > 5) {
		printk(KERN_DEBUG"  Tx ring at %#llx:\n",
		       (long long) np->tx_ring_dma);
		for (i = 0; i < 8 /* TX_RING_SIZE is huge! */; i++)
			printk(KERN_DEBUG " #%d desc. %#8.8x %#llx -> %#8.8x.\n",
			       i, le32_to_cpu(np->tx_ring[i].status),
			       (long long) dma_to_cpu(np->tx_ring[i].addr),
			       le32_to_cpu(np->tx_done_q[i].status));
		printk(KERN_DEBUG "  Rx ring at %#llx -> %p:\n",
		       (long long) np->rx_ring_dma, np->rx_done_q);
		if (np->rx_done_q)
			for (i = 0; i < 8 /* RX_RING_SIZE */; i++) {
				printk(KERN_DEBUG " #%d desc. %#llx -> %#8.8x\n",
				       i, (long long) dma_to_cpu(np->rx_ring[i].rxaddr), le32_to_cpu(np->rx_done_q[i].status));
		}
	}

	free_irq(dev->irq, dev);

	/* Free all the skbuffs in the Rx queue. */
	for (i = 0; i < RX_RING_SIZE; i++) {
		np->rx_ring[i].rxaddr = cpu_to_dma(0xBADF00D0); /* An invalid address. */
		if (np->rx_info[i].skb != NULL) {
			pci_unmap_single(np->pci_dev, np->rx_info[i].mapping, np->rx_buf_sz, PCI_DMA_FROMDEVICE);
			dev_kfree_skb(np->rx_info[i].skb);
		}
		np->rx_info[i].skb = NULL;
		np->rx_info[i].mapping = 0;
	}
	for (i = 0; i < TX_RING_SIZE; i++) {
		struct sk_buff *skb = np->tx_info[i].skb;
		if (skb == NULL)
			continue;
		pci_unmap_single(np->pci_dev,
				 np->tx_info[i].mapping,
				 skb_first_frag_len(skb), PCI_DMA_TODEVICE);
		np->tx_info[i].mapping = 0;
		dev_kfree_skb(skb);
		np->tx_info[i].skb = NULL;
	}

	COMPAT_MOD_DEC_USE_COUNT;

	return 0;
}


static void __devexit starfire_remove_one (struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	struct netdev_private *np;

	if (!dev)
		BUG();

	np = dev->priv;
	if (np->queue_mem)
		pci_free_consistent(pdev, np->queue_mem_size, np->queue_mem, np->queue_mem_dma);

	unregister_netdev(dev);
	iounmap((char *)dev->base_addr);
	pci_release_regions(pdev);

	pci_set_drvdata(pdev, NULL);
	kfree(dev);			/* Will also free np!! */
}


static struct pci_driver starfire_driver = {
	.name		= DRV_NAME,
	.probe		= starfire_init_one,
	.remove		= __devexit_p(starfire_remove_one),
	.id_table	= starfire_pci_tbl,
};


static int __init starfire_init (void)
{
/* when a module, this is printed whether or not devices are found in probe */
#ifdef MODULE
	printk(version);
#endif
#ifndef ADDR_64BITS
	/* we can do this test only at run-time... sigh */
	if (sizeof(dma_addr_t) == sizeof(u64)) {
		printk("This driver has not been ported to this 64-bit architecture yet\n");
		return -ENODEV;
	}
#endif /* not ADDR_64BITS */
#ifndef HAS_FIRMWARE
	/* unconditionally disable hw cksums if firmware is not present */
	enable_hw_cksum = 0;
#endif /* not HAS_FIRMWARE */
	return pci_module_init (&starfire_driver);
}


static void __exit starfire_cleanup (void)
{
	pci_unregister_driver (&starfire_driver);
}


module_init(starfire_init);
module_exit(starfire_cleanup);


/*
 * Local variables:
 *  c-basic-offset: 8
 *  tab-width: 8
 * End:
 */
