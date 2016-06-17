/* znet.c: An Zenith Z-Note ethernet driver for linux. */

static const char version[] = "znet.c:v1.02 9/23/94 becker@cesdis.gsfc.nasa.gov\n";

/*
	Written by Donald Becker.

	The author may be reached as becker@scyld.com.
	This driver is based on the Linux skeleton driver.  The copyright of the
	skeleton driver is held by the United States Government, as represented
	by DIRNSA, and it is released under the GPL.

	Thanks to Mike Hollick for alpha testing and suggestions.

  References:
	   The Crynwr packet driver.

	  "82593 CSMA/CD Core LAN Controller" Intel datasheet, 1992
	  Intel Microcommunications Databook, Vol. 1, 1990.
    As usual with Intel, the documentation is incomplete and inaccurate.
	I had to read the Crynwr packet driver to figure out how to actually
	use the i82593, and guess at what register bits matched the loosely
	related i82586.

					Theory of Operation

	The i82593 used in the Zenith Z-Note series operates using two(!) slave
	DMA	channels, one interrupt, and one 8-bit I/O port.

	While there	several ways to configure '593 DMA system, I chose the one
	that seemed commensurate with the highest system performance in the face
	of moderate interrupt latency: Both DMA channels are configured as
	recirculating ring buffers, with one channel (#0) dedicated to Rx and
	the other channel (#1) to Tx and configuration.  (Note that this is
	different than the Crynwr driver, where the Tx DMA channel is initialized
	before each operation.  That approach simplifies operation and Tx error
	recovery, but requires additional I/O in normal operation and precludes
	transmit buffer	chaining.)

	Both rings are set to 8192 bytes using {TX,RX}_RING_SIZE.  This provides
	a reasonable ring size for Rx, while simplifying DMA buffer allocation --
	DMA buffers must not cross a 128K boundary.  (In truth the size selection
	was influenced by my lack of '593 documentation.  I thus was constrained
	to use the Crynwr '593 initialization table, which sets the Rx ring size
	to 8K.)

	Despite my usual low opinion about Intel-designed parts, I must admit
	that the bulk data handling of the i82593 is a good design for
	an integrated system, like a laptop, where using two slave DMA channels
	doesn't pose a problem.  I still take issue with using only a single I/O
	port.  In the same controlled environment there are essentially no
	limitations on I/O space, and using multiple locations would eliminate
	the	need for multiple operations when looking at status registers,
	setting the Rx ring boundary, or switching to promiscuous mode.

	I also question Zenith's selection of the '593: one of the advertised
	advantages of earlier Intel parts was that if you figured out the magic
	initialization incantation you could use the same part on many different
	network types.  Zenith's use of the "FriendlyNet" (sic) connector rather
	than an	on-board transceiver leads me to believe that they were planning
	to take advantage of this.  But, uhmmm, the '593 omits all but ethernet
	functionality from the serial subsystem.
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <asm/system.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/dma.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/if_arp.h>

#ifndef ZNET_DEBUG
#define ZNET_DEBUG 1
#endif
static unsigned int znet_debug = ZNET_DEBUG;

/* The DMA modes we need aren't in <dma.h>. */
#define DMA_RX_MODE		0x14	/* Auto init, I/O to mem, ++, demand. */
#define DMA_TX_MODE		0x18	/* Auto init, Mem to I/O, ++, demand. */
#define dma_page_eq(ptr1, ptr2) ((long)(ptr1)>>17 == (long)(ptr2)>>17)
#define DMA_BUF_SIZE 8192
#define RX_BUF_SIZE 8192
#define TX_BUF_SIZE 8192

/* Commands to the i82593 channel 0. */
#define CMD0_CHNL_0			0x00
#define CMD0_CHNL_1			0x10		/* Switch to channel 1. */
#define CMD0_NOP (CMD0_CHNL_0)
#define CMD0_PORT_1	CMD0_CHNL_1
#define CMD1_PORT_0	1
#define CMD0_IA_SETUP		1
#define CMD0_CONFIGURE		2
#define CMD0_MULTICAST_LIST 3
#define CMD0_TRANSMIT		4
#define CMD0_DUMP			6
#define CMD0_DIAGNOSE		7
#define CMD0_Rx_ENABLE		8
#define CMD0_Rx_DISABLE		10
#define CMD0_Rx_STOP		11
#define CMD0_RETRANSMIT		12
#define CMD0_ABORT			13
#define CMD0_RESET			14

#define CMD0_ACK 0x80

#define CMD0_STAT0 (0 << 5)
#define CMD0_STAT1 (1 << 5)
#define CMD0_STAT2 (2 << 5)
#define CMD0_STAT3 (3 << 5)

#define TX_TIMEOUT	10

#define net_local znet_private
struct znet_private {
	int rx_dma, tx_dma;
	struct net_device_stats stats;
	spinlock_t lock;
	/* The starting, current, and end pointers for the packet buffers. */
	ushort *rx_start, *rx_cur, *rx_end;
	ushort *tx_start, *tx_cur, *tx_end;
	ushort tx_buf_len;			/* Tx buffer length, in words. */
};

/* Only one can be built-in;-> */
static struct znet_private zn;
static ushort dma_buffer1[DMA_BUF_SIZE/2];
static ushort dma_buffer2[DMA_BUF_SIZE/2];
static ushort dma_buffer3[DMA_BUF_SIZE/2 + 8];

/* The configuration block.  What an undocumented nightmare.  The first
   set of values are those suggested (without explanation) for ethernet
   in the Intel 82586 databook.	 The rest appear to be completely undocumented,
   except for cryptic notes in the Crynwr packet driver.  This driver uses
   the Crynwr values verbatim. */

static unsigned char i593_init[] = {
  0xAA,					/* 0: 16-byte input & 80-byte output FIFO. */
						/*	  threshold, 96-byte FIFO, 82593 mode. */
  0x88,					/* 1: Continuous w/interrupts, 128-clock DMA.*/
  0x2E,					/* 2: 8-byte preamble, NO address insertion, */
						/*	  6-byte Ethernet address, loopback off.*/
  0x00,					/* 3: Default priorities & backoff methods. */
  0x60,					/* 4: 96-bit interframe spacing. */
  0x00,					/* 5: 512-bit slot time (low-order). */
  0xF2,					/* 6: Slot time (high-order), 15 COLL retries. */
  0x00,					/* 7: Promisc-off, broadcast-on, default CRC. */
  0x00,					/* 8: Default carrier-sense, collision-detect. */
  0x40,					/* 9: 64-byte minimum frame length. */
  0x5F,					/* A: Type/length checks OFF, no CRC input,
						   "jabber" termination, etc. */
  0x00,					/* B: Full-duplex disabled. */
  0x3F,					/* C: Default multicast addresses & backoff. */
  0x07,					/* D: Default IFS retriggering. */
  0x31,					/* E: Internal retransmit, drop "runt" packets,
						   synchr. DRQ deassertion, 6 status bytes. */
  0x22,					/* F: Receive ring-buffer size (8K), 
						   receive-stop register enable. */
};

struct netidblk {
	char magic[8];		/* The magic number (string) "NETIDBLK" */
	unsigned char netid[8]; /* The physical station address */
	char nettype, globalopt;
	char vendor[8];		/* The machine vendor and product name. */
	char product[8];
	char irq1, irq2;		/* Interrupts, only one is currently used.	*/
	char dma1, dma2;
	short dma_mem_misc[8];		/* DMA buffer locations (unused in Linux). */
	short iobase1, iosize1;
	short iobase2, iosize2;		/* Second iobase unused. */
	char driver_options;			/* Misc. bits */
	char pad;
};

int znet_probe(struct net_device *dev);
static int	znet_open(struct net_device *dev);
static int	znet_send_packet(struct sk_buff *skb, struct net_device *dev);
static void	znet_interrupt(int irq, void *dev_id, struct pt_regs *regs);
static void	znet_rx(struct net_device *dev);
static int	znet_close(struct net_device *dev);
static struct net_device_stats *net_get_stats(struct net_device *dev);
static void set_multicast_list(struct net_device *dev);
static void hardware_init(struct net_device *dev);
static void update_stop_hit(short ioaddr, unsigned short rx_stop_offset);
static void znet_tx_timeout (struct net_device *dev);

#ifdef notdef
static struct sigaction znet_sigaction = { &znet_interrupt, 0, 0, NULL, };
#endif


/* The Z-Note probe is pretty easy.  The NETIDBLK exists in the safe-to-probe
   BIOS area.  We just scan for the signature, and pull the vital parameters
   out of the structure. */

int __init znet_probe(struct net_device *dev)
{
	int i;
	struct netidblk *netinfo;
	char *p;

	/* This code scans the region 0xf0000 to 0xfffff for a "NETIDBLK". */
	for(p = (char *)phys_to_virt(0xf0000); p < (char *)phys_to_virt(0x100000); p++)
		if (*p == 'N'  &&  strncmp(p, "NETIDBLK", 8) == 0)
			break;

	if (p >= (char *)phys_to_virt(0x100000)) {
		if (znet_debug > 1)
			printk(KERN_INFO "No Z-Note ethernet adaptor found.\n");
		return -ENODEV;
	}
	netinfo = (struct netidblk *)p;
	dev->base_addr = netinfo->iobase1;
	dev->irq = netinfo->irq1;

	printk(KERN_INFO "%s: ZNET at %#3lx,", dev->name, dev->base_addr);

	/* The station address is in the "netidblk" at 0x0f0000. */
	for (i = 0; i < 6; i++)
		printk(" %2.2x", dev->dev_addr[i] = netinfo->netid[i]);

	printk(", using IRQ %d DMA %d and %d.\n", dev->irq, netinfo->dma1,
		netinfo->dma2);

	if (znet_debug > 1) {
		printk(KERN_INFO "%s: vendor '%16.16s' IRQ1 %d IRQ2 %d DMA1 %d DMA2 %d.\n",
			   dev->name, netinfo->vendor,
			   netinfo->irq1, netinfo->irq2,
			   netinfo->dma1, netinfo->dma2);
		printk(KERN_INFO "%s: iobase1 %#x size %d iobase2 %#x size %d net type %2.2x.\n",
			   dev->name, netinfo->iobase1, netinfo->iosize1,
			   netinfo->iobase2, netinfo->iosize2, netinfo->nettype);
	}

	if (znet_debug > 0)
		printk("%s%s", KERN_INFO, version);

	dev->priv = (void *) &zn;
	zn.rx_dma = netinfo->dma1;
	zn.tx_dma = netinfo->dma2;
	zn.lock = SPIN_LOCK_UNLOCKED;

	/* These should never fail.  You can't add devices to a sealed box! */
	if (request_irq(dev->irq, &znet_interrupt, 0, "ZNet", dev)
		|| request_dma(zn.rx_dma,"ZNet rx")
		|| request_dma(zn.tx_dma,"ZNet tx")) {
		printk(KERN_WARNING "%s: Not opened -- resource busy?!?\n", dev->name);
		return -EBUSY;
	}

	/* Allocate buffer memory.	We can cross a 128K boundary, so we
	   must be careful about the allocation.  It's easiest to waste 8K. */
	if (dma_page_eq(dma_buffer1, &dma_buffer1[RX_BUF_SIZE/2-1]))
	  zn.rx_start = dma_buffer1;
	else 
	  zn.rx_start = dma_buffer2;

	if (dma_page_eq(dma_buffer3, &dma_buffer3[RX_BUF_SIZE/2-1]))
	  zn.tx_start = dma_buffer3;
	else
	  zn.tx_start = dma_buffer2;
	zn.rx_end = zn.rx_start + RX_BUF_SIZE/2;
	zn.tx_buf_len = TX_BUF_SIZE/2;
	zn.tx_end = zn.tx_start + zn.tx_buf_len;

	/* The ZNET-specific entries in the device structure. */
	dev->open = &znet_open;
	dev->hard_start_xmit = &znet_send_packet;
	dev->stop = &znet_close;
	dev->get_stats	= net_get_stats;
	dev->set_multicast_list = &set_multicast_list;
	dev->tx_timeout = znet_tx_timeout;
	dev->watchdog_timeo = TX_TIMEOUT;

	/* Fill in the 'dev' with ethernet-generic values. */
	ether_setup(dev);

	return 0;
}


static int znet_open(struct net_device *dev)
{
	int ioaddr = dev->base_addr;

	if (znet_debug > 2)
		printk(KERN_DEBUG "%s: znet_open() called.\n", dev->name);

	/* Turn on the 82501 SIA, using zenith-specific magic. */
	outb(0x10, 0xe6);					/* Select LAN control register */
	outb(inb(0xe7) | 0x84, 0xe7);		/* Turn on LAN power (bit 2). */
	/* According to the Crynwr driver we should wait 50 msec. for the
	   LAN clock to stabilize.  My experiments indicates that the '593 can
	   be initialized immediately.  The delay is probably needed for the
	   DC-to-DC converter to come up to full voltage, and for the oscillator
	   to be spot-on at 20Mhz before transmitting.
	   Until this proves to be a problem we rely on the higher layers for the
	   delay and save allocating a timer entry. */

	/* This follows the packet driver's lead, and checks for success. */
	if (inb(ioaddr) != 0x10 && inb(ioaddr) != 0x00)
		printk(KERN_WARNING "%s: Problem turning on the transceiver power.\n",
			   dev->name);

	hardware_init(dev);
	netif_start_queue (dev);

	return 0;
}


static void znet_tx_timeout (struct net_device *dev)
{
	int ioaddr = dev->base_addr;
	ushort event, tx_status, rx_offset, state;

	outb (CMD0_STAT0, ioaddr);
	event = inb (ioaddr);
	outb (CMD0_STAT1, ioaddr);
	tx_status = inw (ioaddr);
	outb (CMD0_STAT2, ioaddr);
	rx_offset = inw (ioaddr);
	outb (CMD0_STAT3, ioaddr);
	state = inb (ioaddr);
	printk (KERN_WARNING "%s: transmit timed out, status %02x %04x %04x %02x,"
	 " resetting.\n", dev->name, event, tx_status, rx_offset, state);
	if (tx_status == 0x0400)
		printk (KERN_WARNING "%s: Tx carrier error, check transceiver cable.\n",
			dev->name);
	outb (CMD0_RESET, ioaddr);
	hardware_init (dev);
	netif_wake_queue (dev);
}

static int znet_send_packet(struct sk_buff *skb, struct net_device *dev)
{
	int ioaddr = dev->base_addr;
	struct net_local *lp = (struct net_local *)dev->priv;
	unsigned long flags;
	short length = skb->len;

	if (znet_debug > 4)
		printk(KERN_DEBUG "%s: ZNet_send_packet.\n", dev->name);

	if(length < ETH_ZLEN)
	{
		skb = skb_padto(skb, ETH_ZLEN);
		if(skb == NULL)
			return 0;
		length = ETH_ZLEN;
	}
	
	netif_stop_queue (dev);
	
	/* Check that the part hasn't reset itself, probably from suspend. */
	outb(CMD0_STAT0, ioaddr);
	if (inw(ioaddr) == 0x0010
		&& inw(ioaddr) == 0x0000
		&& inw(ioaddr) == 0x0010)
	  hardware_init(dev);

	if (1) {
		unsigned char *buf = (void *)skb->data;
		ushort *tx_link = zn.tx_cur - 1;
		ushort rnd_len = (length + 1)>>1;
		
		lp->stats.tx_bytes+=length;

		{
			short dma_port = ((zn.tx_dma&3)<<2) + IO_DMA2_BASE;
			unsigned addr = inb(dma_port);
			addr |= inb(dma_port) << 8;
			addr <<= 1;
			if (((int)zn.tx_cur & 0x1ffff) != addr)
			  printk(KERN_WARNING "Address mismatch at Tx: %#x vs %#x.\n",
					 (int)zn.tx_cur & 0xffff, addr);
			zn.tx_cur = (ushort *)(((int)zn.tx_cur & 0xfe0000) | addr);
		}

		if (zn.tx_cur >= zn.tx_end)
		  zn.tx_cur = zn.tx_start;
		*zn.tx_cur++ = length;
		if (zn.tx_cur + rnd_len + 1 > zn.tx_end) {
			int semi_cnt = (zn.tx_end - zn.tx_cur)<<1; /* Cvrt to byte cnt. */
			memcpy(zn.tx_cur, buf, semi_cnt);
			rnd_len -= semi_cnt>>1;
			memcpy(zn.tx_start, buf + semi_cnt, length - semi_cnt);
			zn.tx_cur = zn.tx_start + rnd_len;
		} else {
			memcpy(zn.tx_cur, buf, skb->len);
			zn.tx_cur += rnd_len;
		}
		*zn.tx_cur++ = 0;

		spin_lock_irqsave(&lp->lock, flags);
		{
			*tx_link = CMD0_TRANSMIT + CMD0_CHNL_1;
			/* Is this always safe to do? */
			outb(CMD0_TRANSMIT + CMD0_CHNL_1,ioaddr);
		}
		spin_unlock_irqrestore (&lp->lock, flags);

		dev->trans_start = jiffies;
		netif_start_queue (dev);

		if (znet_debug > 4)
		  printk(KERN_DEBUG "%s: Transmitter queued, length %d.\n", dev->name, length);
	}
	dev_kfree_skb(skb); 
	return 0;
}

/* The ZNET interrupt handler. */
static void	znet_interrupt(int irq, void *dev_id, struct pt_regs * regs)
{
	struct net_device *dev = dev_id;
	struct net_local *lp = (struct net_local *)dev->priv;
	int ioaddr;
	int boguscnt = 20;

	if (dev == NULL) {
		printk(KERN_WARNING "znet_interrupt(): IRQ %d for unknown device.\n", irq);
		return;
	}

	spin_lock (&lp->lock);
	
	ioaddr = dev->base_addr;

	outb(CMD0_STAT0, ioaddr);
	do {
		ushort status = inb(ioaddr);
		if (znet_debug > 5) {
			ushort result, rx_ptr, running;
			outb(CMD0_STAT1, ioaddr);
			result = inw(ioaddr);
			outb(CMD0_STAT2, ioaddr);
			rx_ptr = inw(ioaddr);
			outb(CMD0_STAT3, ioaddr);
			running = inb(ioaddr);
			printk(KERN_DEBUG "%s: interrupt, status %02x, %04x %04x %02x serial %d.\n",
				 dev->name, status, result, rx_ptr, running, boguscnt);
		}
		if ((status & 0x80) == 0)
			break;

		if ((status & 0x0F) == 4) {	/* Transmit done. */
			int tx_status;
			outb(CMD0_STAT1, ioaddr);
			tx_status = inw(ioaddr);
			/* It's undocumented, but tx_status seems to match the i82586. */
			if (tx_status & 0x2000) {
				lp->stats.tx_packets++;
				lp->stats.collisions += tx_status & 0xf;
			} else {
				if (tx_status & 0x0600)  lp->stats.tx_carrier_errors++;
				if (tx_status & 0x0100)  lp->stats.tx_fifo_errors++;
				if (!(tx_status & 0x0040)) lp->stats.tx_heartbeat_errors++;
				if (tx_status & 0x0020)  lp->stats.tx_aborted_errors++;
				/* ...and the catch-all. */
				if ((tx_status | 0x0760) != 0x0760)
				  lp->stats.tx_errors++;
			}
			netif_wake_queue (dev);
		}

		if ((status & 0x40)
			|| (status & 0x0f) == 11) {
			znet_rx(dev);
		}
		/* Clear the interrupts we've handled. */
		outb(CMD0_ACK,ioaddr);
	} while (boguscnt--);

	spin_unlock (&lp->lock);
	
	return;
}

static void znet_rx(struct net_device *dev)
{
	struct net_local *lp = (struct net_local *)dev->priv;
	int ioaddr = dev->base_addr;
	int boguscount = 1;
	short next_frame_end_offset = 0; 		/* Offset of next frame start. */
	short *cur_frame_end;
	short cur_frame_end_offset;

	outb(CMD0_STAT2, ioaddr);
	cur_frame_end_offset = inw(ioaddr);

	if (cur_frame_end_offset == zn.rx_cur - zn.rx_start) {
		printk(KERN_WARNING "%s: Interrupted, but nothing to receive, offset %03x.\n",
			   dev->name, cur_frame_end_offset);
		return;
	}

	/* Use same method as the Crynwr driver: construct a forward list in
	   the same area of the backwards links we now have.  This allows us to
	   pass packets to the upper layers in the order they were received --
	   important for fast-path sequential operations. */
	 while (zn.rx_start + cur_frame_end_offset != zn.rx_cur
			&& ++boguscount < 5) {
		unsigned short hi_cnt, lo_cnt, hi_status, lo_status;
		int count, status;

		if (cur_frame_end_offset < 4) {
			/* Oh no, we have a special case: the frame trailer wraps around
			   the end of the ring buffer.  We've saved space at the end of
			   the ring buffer for just this problem. */
			memcpy(zn.rx_end, zn.rx_start, 8);
			cur_frame_end_offset += (RX_BUF_SIZE/2);
		}
		cur_frame_end = zn.rx_start + cur_frame_end_offset - 4;

		lo_status = *cur_frame_end++;
		hi_status = *cur_frame_end++;
		status = ((hi_status & 0xff) << 8) + (lo_status & 0xff);
		lo_cnt = *cur_frame_end++;
		hi_cnt = *cur_frame_end++;
		count = ((hi_cnt & 0xff) << 8) + (lo_cnt & 0xff);

		if (znet_debug > 5)
		  printk(KERN_DEBUG "Constructing trailer at location %03x, %04x %04x %04x %04x"
				 " count %#x status %04x.\n",
				 cur_frame_end_offset<<1, lo_status, hi_status, lo_cnt, hi_cnt,
				 count, status);
		cur_frame_end[-4] = status;
		cur_frame_end[-3] = next_frame_end_offset;
		cur_frame_end[-2] = count;
		next_frame_end_offset = cur_frame_end_offset;
		cur_frame_end_offset -= ((count + 1)>>1) + 3;
		if (cur_frame_end_offset < 0)
		  cur_frame_end_offset += RX_BUF_SIZE/2;
	};

	/* Now step  forward through the list. */
	do {
		ushort *this_rfp_ptr = zn.rx_start + next_frame_end_offset;
		int status = this_rfp_ptr[-4];
		int pkt_len = this_rfp_ptr[-2];
	  
		if (znet_debug > 5)
		  printk(KERN_DEBUG "Looking at trailer ending at %04x status %04x length %03x"
				 " next %04x.\n", next_frame_end_offset<<1, status, pkt_len,
				 this_rfp_ptr[-3]<<1);
		/* Once again we must assume that the i82586 docs apply. */
		if ( ! (status & 0x2000)) {				/* There was an error. */
			lp->stats.rx_errors++;
			if (status & 0x0800) lp->stats.rx_crc_errors++;
			if (status & 0x0400) lp->stats.rx_frame_errors++;
			if (status & 0x0200) lp->stats.rx_over_errors++; /* Wrong. */
			if (status & 0x0100) lp->stats.rx_fifo_errors++;
			if (status & 0x0080) lp->stats.rx_length_errors++;
		} else if (pkt_len > 1536) {
			lp->stats.rx_length_errors++;
		} else {
			/* Malloc up new buffer. */
			struct sk_buff *skb;

			skb = dev_alloc_skb(pkt_len);
			if (skb == NULL) {
				if (znet_debug)
				  printk(KERN_WARNING "%s: Memory squeeze, dropping packet.\n", dev->name);
				lp->stats.rx_dropped++;
				break;
			}
			skb->dev = dev;

			if (&zn.rx_cur[(pkt_len+1)>>1] > zn.rx_end) {
				int semi_cnt = (zn.rx_end - zn.rx_cur)<<1;
				memcpy(skb_put(skb,semi_cnt), zn.rx_cur, semi_cnt);
				memcpy(skb_put(skb,pkt_len-semi_cnt), zn.rx_start,
					   pkt_len - semi_cnt);
			} else {
				memcpy(skb_put(skb,pkt_len), zn.rx_cur, pkt_len);
				if (znet_debug > 6) {
					unsigned int *packet = (unsigned int *) skb->data;
					printk(KERN_DEBUG "Packet data is %08x %08x %08x %08x.\n", packet[0],
						   packet[1], packet[2], packet[3]);
				}
		  }
		  skb->protocol=eth_type_trans(skb,dev);
		  netif_rx(skb);
		  dev->last_rx = jiffies;
		  lp->stats.rx_packets++;
		  lp->stats.rx_bytes += pkt_len;
		}
		zn.rx_cur = this_rfp_ptr;
		if (zn.rx_cur >= zn.rx_end)
			zn.rx_cur -= RX_BUF_SIZE/2;
		update_stop_hit(ioaddr, (zn.rx_cur - zn.rx_start)<<1);
		next_frame_end_offset = this_rfp_ptr[-3];
		if (next_frame_end_offset == 0)		/* Read all the frames? */
			break;			/* Done for now */
		this_rfp_ptr = zn.rx_start + next_frame_end_offset;
	} while (--boguscount);

	/* If any worth-while packets have been received, dev_rint()
	   has done a mark_bh(INET_BH) for us and will work on them
	   when we get to the bottom-half routine. */
	return;
}

/* The inverse routine to znet_open(). */
static int znet_close(struct net_device *dev)
{
	unsigned long flags;
	int ioaddr = dev->base_addr;

	netif_stop_queue (dev);

	outb(CMD0_RESET, ioaddr);			/* CMD0_RESET */

	flags=claim_dma_lock();
	disable_dma(zn.rx_dma);
	disable_dma(zn.tx_dma);
	release_dma_lock(flags);

	free_irq(dev->irq, dev);

	if (znet_debug > 1)
		printk(KERN_DEBUG "%s: Shutting down ethercard.\n", dev->name);
	/* Turn off transceiver power. */
	outb(0x10, 0xe6);					/* Select LAN control register */
	outb(inb(0xe7) & ~0x84, 0xe7);		/* Turn on LAN power (bit 2). */

	return 0;
}

/* Get the current statistics.	This may be called with the card open or
   closed. */
static struct net_device_stats *net_get_stats(struct net_device *dev)
{
		struct net_local *lp = (struct net_local *)dev->priv;

		return &lp->stats;
}

/* Set or clear the multicast filter for this adaptor.
   As a side effect this routine must also initialize the device parameters.
   This is taken advantage of in open().

   N.B. that we change i593_init[] in place.  This (properly) makes the
   mode change persistent, but must be changed if this code is moved to
   a multiple adaptor environment.
 */
static void set_multicast_list(struct net_device *dev)
{
	short ioaddr = dev->base_addr;

	if (dev->flags&IFF_PROMISC) {
		/* Enable promiscuous mode */
		i593_init[7] &= ~3;		i593_init[7] |= 1;
		i593_init[13] &= ~8;	i593_init[13] |= 8;
	} else if (dev->mc_list || (dev->flags&IFF_ALLMULTI)) {
		/* Enable accept-all-multicast mode */
		i593_init[7] &= ~3;		i593_init[7] |= 0;
		i593_init[13] &= ~8;	i593_init[13] |= 8;
	} else {					/* Enable normal mode. */
		i593_init[7] &= ~3;		i593_init[7] |= 0;
		i593_init[13] &= ~8;	i593_init[13] |= 0;
	}
	*zn.tx_cur++ = sizeof(i593_init);
	memcpy(zn.tx_cur, i593_init, sizeof(i593_init));
	zn.tx_cur += sizeof(i593_init)/2;
	outb(CMD0_CONFIGURE+CMD0_CHNL_1, ioaddr);
#ifdef not_tested
	if (num_addrs > 0) {
		int addrs_len = 6*num_addrs;
		*zn.tx_cur++ = addrs_len;
		memcpy(zn.tx_cur, addrs, addrs_len);
		outb(CMD0_MULTICAST_LIST+CMD0_CHNL_1, ioaddr);
		zn.tx_cur += addrs_len>>1;
	}
#endif
}

void show_dma(void)
{
	unsigned long flags;
	short dma_port = ((zn.tx_dma&3)<<2) + IO_DMA2_BASE;
	unsigned addr = inb(dma_port);
	addr |= inb(dma_port) << 8;

	flags=claim_dma_lock();
	printk("Addr: %04x cnt:%3x...", addr<<1, get_dma_residue(zn.tx_dma));
	release_dma_lock(flags);
}

/* Initialize the hardware.  We have to do this when the board is open()ed
   or when we come out of suspend mode. */
static void hardware_init(struct net_device *dev)
{
	unsigned long flags;
	short ioaddr = dev->base_addr;

	zn.rx_cur = zn.rx_start;
	zn.tx_cur = zn.tx_start;

	/* Reset the chip, and start it up. */
	outb(CMD0_RESET, ioaddr);

	flags=claim_dma_lock();
	disable_dma(zn.rx_dma); 		/* reset by an interrupting task. */
	clear_dma_ff(zn.rx_dma);
	set_dma_mode(zn.rx_dma, DMA_RX_MODE);
	set_dma_addr(zn.rx_dma, (unsigned int) zn.rx_start);
	set_dma_count(zn.rx_dma, RX_BUF_SIZE);
	enable_dma(zn.rx_dma);
	/* Now set up the Tx channel. */
	disable_dma(zn.tx_dma);
	clear_dma_ff(zn.tx_dma);
	set_dma_mode(zn.tx_dma, DMA_TX_MODE);
	set_dma_addr(zn.tx_dma, (unsigned int) zn.tx_start);
	set_dma_count(zn.tx_dma, zn.tx_buf_len<<1);
	enable_dma(zn.tx_dma);
	release_dma_lock(flags);
	
	if (znet_debug > 1)
	  printk(KERN_DEBUG "%s: Initializing the i82593, tx buf %p... ", dev->name,
			 zn.tx_start);
	/* Do an empty configure command, just like the Crynwr driver.  This
	   resets to chip to its default values. */
	*zn.tx_cur++ = 0;
	*zn.tx_cur++ = 0;
	printk("stat:%02x ", inb(ioaddr)); show_dma();
	outb(CMD0_CONFIGURE+CMD0_CHNL_1, ioaddr);
	*zn.tx_cur++ = sizeof(i593_init);
	memcpy(zn.tx_cur, i593_init, sizeof(i593_init));
	zn.tx_cur += sizeof(i593_init)/2;
	printk("stat:%02x ", inb(ioaddr)); show_dma();
	outb(CMD0_CONFIGURE+CMD0_CHNL_1, ioaddr);
	*zn.tx_cur++ = 6;
	memcpy(zn.tx_cur, dev->dev_addr, 6);
	zn.tx_cur += 3;
	printk("stat:%02x ", inb(ioaddr)); show_dma();
	outb(CMD0_IA_SETUP + CMD0_CHNL_1, ioaddr);
	printk("stat:%02x ", inb(ioaddr)); show_dma();

	update_stop_hit(ioaddr, 8192);
	if (znet_debug > 1)  printk("enabling Rx.\n");
	outb(CMD0_Rx_ENABLE+CMD0_CHNL_0, ioaddr);
	netif_start_queue (dev);
}

static void update_stop_hit(short ioaddr, unsigned short rx_stop_offset)
{
	outb(CMD0_PORT_1, ioaddr);
	if (znet_debug > 5)
	  printk(KERN_DEBUG "Updating stop hit with value %02x.\n",
			 (rx_stop_offset >> 6) | 0x80);
	outb((rx_stop_offset >> 6) | 0x80, ioaddr);
	outb(CMD1_PORT_0, ioaddr);
}

/*
 * Local variables:
 *  compile-command: "gcc -D__KERNEL__ -I/usr/src/linux/net/inet -Wall -Wstrict-prototypes -O6 -m486 -c znet.c"
 *  version-control: t
 *  kept-new-versions: 5
 *  c-indent-level: 4
 *  tab-width: 4
 * End:
 */
