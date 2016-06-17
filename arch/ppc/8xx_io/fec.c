/*
 * Fast Ethernet Controller (FEC) driver for Motorola MPC8xx.
 * Copyright (c) 1997 Dan Malek (dmalek@jlc.net)
 *
 * This version of the driver is specific to the FADS implementation,
 * since the board contains control registers external to the processor
 * for the control of the LevelOne LXT970 transceiver.  The MPC860T manual
 * describes connections using the internal parallel port I/O, which
 * is basically all of Port D.
 *
 * Includes support for the following PHYs: QS6612, LXT970, LXT971/2.
 *
 * Right now, I am very wasteful with the buffers.  I allocate memory
 * pages and then divide them into 2K frame buffers.  This way I know I
 * have buffers large enough to hold one frame within one buffer descriptor.
 * Once I get this working, I will use 64 or 128 byte CPM buffers, which
 * will be much more memory efficient and will easily handle lots of
 * small packets.
 *
 * Much better multiple PHY support by Magnus Damm.
 * Copyright (c) 2000 Ericsson Radio Systems AB.
 *
 * Make use of MII for PHY control configurable.
 * Some fixes.
 * Copyright (c) 2000 Wolfgang Denk, DENX Software Engineering.
 */

/* List of PHYs we wish to support.
*/
#undef	CONFIG_FEC_LXT970
#define	CONFIG_FEC_LXT971
#undef	CONFIG_FEC_QS6612
#undef	CONFIG_FEC_DP83843
#undef	CONFIG_FEC_DP83846A

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/spinlock.h>
#include <linux/mii.h>
#include <linux/ethtool.h>

#include <asm/8xx_immap.h>
#include <asm/pgtable.h>
#include <asm/mpc8xx.h>
#include <asm/irq.h>
#include <asm/bitops.h>
#include <asm/uaccess.h>
#include <asm/commproc.h>

#ifdef	CONFIG_USE_MDIO
/* Forward declarations of some structures to support different PHYs
*/

typedef struct {
	uint mii_data;
	void (*funct)(uint mii_reg, struct net_device *dev, uint data);
} phy_cmd_t;

typedef struct {
	uint id;
	char *name;

	const phy_cmd_t *config;
	const phy_cmd_t *startup;
	const phy_cmd_t *ack_int;
	const phy_cmd_t *shutdown;
} phy_info_t;
#endif	/* CONFIG_USE_MDIO */

/* The number of Tx and Rx buffers.  These are allocated from the page
 * pool.  The code may assume these are power of two, so it is best
 * to keep them that size.
 * We don't need to allocate pages for the transmitter.  We just use
 * the skbuffer directly.
 */
#ifdef CONFIG_ENET_BIG_BUFFERS
#define FEC_ENET_RX_PAGES	16
#define FEC_ENET_RX_FRSIZE	2048
#define FEC_ENET_RX_FRPPG	(PAGE_SIZE / FEC_ENET_RX_FRSIZE)
#define RX_RING_SIZE		(FEC_ENET_RX_FRPPG * FEC_ENET_RX_PAGES)
#define TX_RING_SIZE		16	/* Must be power of two */
#define TX_RING_MOD_MASK	15	/*   for this to work */
#else
#define FEC_ENET_RX_PAGES	4
#define FEC_ENET_RX_FRSIZE	2048
#define FEC_ENET_RX_FRPPG	(PAGE_SIZE / FEC_ENET_RX_FRSIZE)
#define RX_RING_SIZE		(FEC_ENET_RX_FRPPG * FEC_ENET_RX_PAGES)
#define TX_RING_SIZE		8	/* Must be power of two */
#define TX_RING_MOD_MASK	7	/*   for this to work */
#endif

/* Interrupt events/masks.
*/
#define FEC_ENET_HBERR	((uint)0x80000000)	/* Heartbeat error */
#define FEC_ENET_BABR	((uint)0x40000000)	/* Babbling receiver */
#define FEC_ENET_BABT	((uint)0x20000000)	/* Babbling transmitter */
#define FEC_ENET_GRA	((uint)0x10000000)	/* Graceful stop complete */
#define FEC_ENET_TXF	((uint)0x08000000)	/* Full frame transmitted */
#define FEC_ENET_TXB	((uint)0x04000000)	/* A buffer was transmitted */
#define FEC_ENET_RXF	((uint)0x02000000)	/* Full frame received */
#define FEC_ENET_RXB	((uint)0x01000000)	/* A buffer was received */
#define FEC_ENET_MII	((uint)0x00800000)	/* MII interrupt */
#define FEC_ENET_EBERR	((uint)0x00400000)	/* SDMA bus error */

/*
*/
#define FEC_ECNTRL_PINMUX	0x00000004
#define FEC_ECNTRL_ETHER_EN	0x00000002
#define FEC_ECNTRL_RESET	0x00000001

#define FEC_RCNTRL_BC_REJ	0x00000010
#define FEC_RCNTRL_PROM		0x00000008
#define FEC_RCNTRL_MII_MODE	0x00000004
#define FEC_RCNTRL_DRT		0x00000002
#define FEC_RCNTRL_LOOP		0x00000001

#define FEC_TCNTRL_FDEN		0x00000004
#define FEC_TCNTRL_HBC		0x00000002
#define FEC_TCNTRL_GTS		0x00000001

/* Delay to wait for FEC reset command to complete (in us)
*/
#define FEC_RESET_DELAY		50

/* The FEC stores dest/src/type, data, and checksum for receive packets.
 */
#define PKT_MAXBUF_SIZE		1518
#define PKT_MINBUF_SIZE		64
#define PKT_MAXBLR_SIZE		1520

/* The FEC buffer descriptors track the ring buffers.  The rx_bd_base and
 * tx_bd_base always point to the base of the buffer descriptors.  The
 * cur_rx and cur_tx point to the currently available buffer.
 * The dirty_tx tracks the current buffer that is being sent by the
 * controller.  The cur_tx and dirty_tx are equal under both completely
 * empty and completely full conditions.  The empty/ready indicator in
 * the buffer descriptor determines the actual condition.
 */
struct fec_enet_private {
	/* The saved address of a sent-in-place packet/buffer, for skfree(). */
	struct	sk_buff* tx_skbuff[TX_RING_SIZE];
	ushort	skb_cur;
	ushort	skb_dirty;

	/* CPM dual port RAM relative addresses.
	*/
	cbd_t	*rx_bd_base;		/* Address of Rx and Tx buffers. */
	cbd_t	*tx_bd_base;
	cbd_t	*cur_rx, *cur_tx;		/* The next free ring entry */
	cbd_t	*dirty_tx;	/* The ring entries to be free()ed. */

	/* Virtual addresses for the receive buffers because we can't
	 * do a __va() on them anymore.
	 */
	unsigned char *rx_vaddr[RX_RING_SIZE];

	struct	net_device_stats stats;
	uint	tx_free;
	spinlock_t lock;

#ifdef	CONFIG_USE_MDIO
	uint	phy_id;
	uint	phy_id_done;
	uint	phy_status;
	uint	phy_speed;
	phy_info_t	*phy;
	struct tq_struct phy_task;

	uint	sequence_done;

	uint	phy_addr;

	struct timer_list phy_timer_list;
	u16 old_status;
#endif	/* CONFIG_USE_MDIO */

	int	link;
	int	old_link;
	int	full_duplex;
};

static int fec_enet_open(struct net_device *dev);
static int fec_enet_start_xmit(struct sk_buff *skb, struct net_device *dev);
#ifdef	CONFIG_USE_MDIO
static void fec_enet_mii(struct net_device *dev);
#endif	/* CONFIG_USE_MDIO */
static void fec_enet_interrupt(int irq, void * dev_id, struct pt_regs * regs);
static void  fec_enet_tx(struct net_device *dev);
static void  fec_enet_rx(struct net_device *dev);
static int fec_enet_close(struct net_device *dev);
static struct net_device_stats *fec_enet_get_stats(struct net_device *dev);
static void set_multicast_list(struct net_device *dev);
static void fec_restart(struct net_device *dev, int duplex);
static void fec_stop(struct net_device *dev);
static	ushort	my_enet_addr[3];

#ifdef	CONFIG_USE_MDIO
static int fec_enet_ioctl(struct net_device *dev, struct ifreq *rq, int cmd);
static int netdev_ethtool_ioctl(struct net_device *dev, void *useraddr);

static void mdio_callback(uint regval, struct net_device *dev, uint data);
static int mdio_read(struct net_device *dev, int phy_id, int location);

#if defined(CONFIG_FEC_DP83846A)
static void mdio_timer_callback(unsigned long data);
#endif /* CONFIG_FEC_DP83846A */

/* MII processing.  We keep this as simple as possible.  Requests are
 * placed on the list (if there is room).  When the request is finished
 * by the MII, an optional function may be called.
 */
typedef struct mii_list {
	uint	mii_regval;
	void	(*mii_func)(uint val, struct net_device *dev, uint data);
	struct	mii_list *mii_next;
	uint	mii_data;
} mii_list_t;

#define		NMII	20
mii_list_t	mii_cmds[NMII];
mii_list_t	*mii_free;
mii_list_t	*mii_head;
mii_list_t	*mii_tail;
int         mii_stopped;

typedef struct mdio_read_data {
	u16 regval;
	struct task_struct *sleeping_task;
} mdio_read_data_t;

static int	mii_queue(struct net_device *dev, int request,
				void (*func)(uint, struct net_device *, uint), uint data);
static void mii_queue_relink(uint mii_reg, struct net_device *dev, uint data);

/* Make MII read/write commands for the FEC.
*/
#define mk_mii_read(REG)	(0x60020000 | ((REG & 0x1f) << 18))
#define mk_mii_write(REG, VAL)	(0x50020000 | ((REG & 0x1f) << 18) | \
						(VAL & 0xffff))
#define mk_mii_end	0
#endif	/* CONFIG_USE_MDIO */

/* Transmitter timeout.
*/
#define TX_TIMEOUT (2*HZ)

#ifdef	CONFIG_USE_MDIO
/* Register definitions for the PHY.
*/

#define MII_REG_CR          0  /* Control Register                         */
#define MII_REG_SR          1  /* Status Register                          */
#define MII_REG_PHYIR1      2  /* PHY Identification Register 1            */
#define MII_REG_PHYIR2      3  /* PHY Identification Register 2            */
#define MII_REG_ANAR        4  /* A-N Advertisement Register               */
#define MII_REG_ANLPAR      5  /* A-N Link Partner Ability Register        */
#define MII_REG_ANER        6  /* A-N Expansion Register                   */
#define MII_REG_ANNPTR      7  /* A-N Next Page Transmit Register          */
#define MII_REG_ANLPRNPR    8  /* A-N Link Partner Received Next Page Reg. */

/* values for phy_status */

#define PHY_CONF_ANE	0x0001  /* 1 auto-negotiation enabled */
#define PHY_CONF_LOOP	0x0002  /* 1 loopback mode enabled */
#define PHY_CONF_SPMASK	0x00f0  /* mask for speed */
#define PHY_CONF_10HDX	0x0010  /* 10 Mbit half duplex supported */
#define PHY_CONF_10FDX	0x0020  /* 10 Mbit full duplex supported */
#define PHY_CONF_100HDX	0x0040  /* 100 Mbit half duplex supported */
#define PHY_CONF_100FDX	0x0080  /* 100 Mbit full duplex supported */

#define PHY_STAT_LINK	0x0100  /* 1 up - 0 down */
#define PHY_STAT_FAULT	0x0200  /* 1 remote fault */
#define PHY_STAT_ANC	0x0400  /* 1 auto-negotiation complete	*/
#define PHY_STAT_SPMASK	0xf000  /* mask for speed */
#define PHY_STAT_10HDX	0x1000  /* 10 Mbit half duplex selected	*/
#define PHY_STAT_10FDX	0x2000  /* 10 Mbit full duplex selected	*/
#define PHY_STAT_100HDX	0x4000  /* 100 Mbit half duplex selected */
#define PHY_STAT_100FDX	0x8000  /* 100 Mbit full duplex selected */
#endif	/* CONFIG_USE_MDIO */

static int
fec_enet_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct fec_enet_private *fep;
	volatile fec_t	*fecp;
	volatile cbd_t	*bdp;

	fep = dev->priv;
	fecp = (volatile fec_t*)dev->base_addr;

	/* Fill in a Tx ring entry */
	bdp = fep->cur_tx;

#ifndef final_version
	if (!fep->tx_free || (bdp->cbd_sc & BD_ENET_TX_READY)) {
		/* Ooops.  All transmit buffers are full.  Bail out.
		 * This should not happen, since the tx queue should be stopped.
		 */
		printk("%s: tx queue full!.\n", dev->name);
		return 1;
	}
#endif

	/* Clear all of the status flags.
	 */
	bdp->cbd_sc &= ~BD_ENET_TX_STATS;

	/* Set buffer length and buffer pointer.
	*/
	bdp->cbd_bufaddr = __pa(skb->data);
	bdp->cbd_datlen = skb->len;

	/* Save skb pointer.
	*/
	fep->tx_skbuff[fep->skb_cur] = skb;

	fep->stats.tx_bytes += skb->len;
	fep->skb_cur = (fep->skb_cur+1) & TX_RING_MOD_MASK;

	/* Push the data cache so the CPM does not get stale memory
	 * data.
	 */
	flush_dcache_range((unsigned long)skb->data,
			   (unsigned long)skb->data + skb->len);

	spin_lock_irq(&fep->lock);

	/* Send it on its way.  Tell FEC its ready, interrupt when done,
	 * its the last BD of the frame, and to put the CRC on the end.
	 */

	bdp->cbd_sc |= (BD_ENET_TX_READY | BD_ENET_TX_INTR
			| BD_ENET_TX_LAST | BD_ENET_TX_TC);

	dev->trans_start = jiffies;

	/* Trigger transmission start */
	fecp->fec_x_des_active = 0x01000000;

	/* If this was the last BD in the ring, start at the beginning again.
	*/
	if (bdp->cbd_sc & BD_ENET_TX_WRAP) {
		bdp = fep->tx_bd_base;
	} else {
		bdp++;
	}

	if (!--fep->tx_free)
		netif_stop_queue(dev);

	fep->cur_tx = (cbd_t *)bdp;

	spin_unlock_irq(&fep->lock);

	return 0;
}

static void
fec_timeout(struct net_device *dev)
{
	struct fec_enet_private *fep = dev->priv;

	printk("%s: transmit timed out.\n", dev->name);
	fep->stats.tx_errors++;
#ifndef final_version
	{
	int	i;
	cbd_t	*bdp;

	printk("Ring data dump: cur_tx %lx, tx_free %d, dirty_tx %lx, cur_rx %lx\n",
	       (unsigned long)fep->cur_tx, fep->tx_free,
	       (unsigned long)fep->dirty_tx,
	       (unsigned long)fep->cur_rx);

	bdp = fep->tx_bd_base;
	printk(" tx: %u buffers\n",  TX_RING_SIZE);
	for (i = 0 ; i < TX_RING_SIZE; i++) {
		printk("  %08x: %04x %04x %08x\n",
		       (uint) bdp,
		       bdp->cbd_sc,
		       bdp->cbd_datlen,
		       bdp->cbd_bufaddr);
		bdp++;
	}

	bdp = fep->rx_bd_base;
	printk(" rx: %lu buffers\n",  RX_RING_SIZE);
	for (i = 0 ; i < RX_RING_SIZE; i++) {
		printk("  %08x: %04x %04x %08x\n",
		       (uint) bdp,
		       bdp->cbd_sc,
		       bdp->cbd_datlen,
		       bdp->cbd_bufaddr);
		bdp++;
	}
	}
#endif
	if (fep->tx_free)
		netif_wake_queue(dev);
}

/* The interrupt handler.
 * This is called from the MPC core interrupt.
 */
static	void
fec_enet_interrupt(int irq, void * dev_id, struct pt_regs * regs)
{
	struct	net_device *dev = dev_id;
	volatile fec_t	*fecp;
	uint	int_events;
	fecp = (volatile fec_t*)dev->base_addr;

	/* Get the interrupt events that caused us to be here.
	*/
	while ((int_events = fecp->fec_ievent) != 0) {
		fecp->fec_ievent = int_events;
		if ((int_events & (FEC_ENET_HBERR | FEC_ENET_BABR |
				   FEC_ENET_BABT | FEC_ENET_EBERR)) != 0) {
			printk("FEC ERROR %x\n", int_events);
		}

		/* Handle receive event in its own function.
		 */
		if (int_events & FEC_ENET_RXF)
			fec_enet_rx(dev);

		/* Transmit OK, or non-fatal error. Update the buffer
		   descriptors. FEC handles all errors, we just discover
		   them as part of the transmit process.
		*/
		if (int_events & FEC_ENET_TXF)
			fec_enet_tx(dev);

		if (int_events & FEC_ENET_MII) {
#ifdef	CONFIG_USE_MDIO
			fec_enet_mii(dev);
#else
printk("%s[%d] %s: unexpected FEC_ENET_MII event\n", __FILE__,__LINE__,__FUNCTION__);
#endif	/* CONFIG_USE_MDIO */
		}

	}
}


static void
fec_enet_tx(struct net_device *dev)
{
	struct	fec_enet_private *fep;
	volatile cbd_t	*bdp;
	struct	sk_buff	*skb;

	fep = dev->priv;
	spin_lock(&fep->lock);
	bdp = fep->dirty_tx;

	while ((bdp->cbd_sc&BD_ENET_TX_READY) == 0) {
		if (fep->tx_free == TX_RING_SIZE) break;

		skb = fep->tx_skbuff[fep->skb_dirty];
		/* Check for errors. */
		if (bdp->cbd_sc & (BD_ENET_TX_HB | BD_ENET_TX_LC |
				   BD_ENET_TX_RL | BD_ENET_TX_UN |
				   BD_ENET_TX_CSL)) {
			fep->stats.tx_errors++;
			if (bdp->cbd_sc & BD_ENET_TX_HB)  /* No heartbeat */
				fep->stats.tx_heartbeat_errors++;
			if (bdp->cbd_sc & BD_ENET_TX_LC)  /* Late collision */
				fep->stats.tx_window_errors++;
			if (bdp->cbd_sc & BD_ENET_TX_RL)  /* Retrans limit */
				fep->stats.tx_aborted_errors++;
			if (bdp->cbd_sc & BD_ENET_TX_UN)  /* Underrun */
				fep->stats.tx_fifo_errors++;
			if (bdp->cbd_sc & BD_ENET_TX_CSL) /* Carrier lost */
				fep->stats.tx_carrier_errors++;
		} else
			fep->stats.tx_packets++;

#ifndef final_version
		if (bdp->cbd_sc & BD_ENET_TX_READY)
			printk("HEY! Enet xmit interrupt and TX_READY.\n");
#endif
		/* Deferred means some collisions occurred during transmit,
		 * but we eventually sent the packet OK.
		 */
		if (bdp->cbd_sc & BD_ENET_TX_DEF)
			fep->stats.collisions++;

		/* Free the sk buffer associated with this last transmit.
		 */
#if 0
printk("TXI: %x %x %x\n", bdp, skb, fep->skb_dirty);
#endif
		dev_kfree_skb_irq (skb/*, FREE_WRITE*/);
		fep->tx_skbuff[fep->skb_dirty] = NULL;
		fep->skb_dirty = (fep->skb_dirty + 1) & TX_RING_MOD_MASK;

		/* Update pointer to next buffer descriptor to be transmitted.
		 */
		if (bdp->cbd_sc & BD_ENET_TX_WRAP)
			bdp = fep->tx_bd_base;
		else
			bdp++;

		/* Since we have freed up a buffer, the ring is no longer
		 * full.
		 */
		if (!fep->tx_free++) {
			if (netif_queue_stopped(dev))
				netif_wake_queue(dev);
		}
	}
	fep->dirty_tx = (cbd_t *)bdp;
	spin_unlock(&fep->lock);
}


/* During a receive, the cur_rx points to the current incoming buffer.
 * When we update through the ring, if the next incoming buffer has
 * not been given to the system, we just set the empty indicator,
 * effectively tossing the packet.
 */
static void
fec_enet_rx(struct net_device *dev)
{
	struct	fec_enet_private *fep;
	volatile fec_t	*fecp;
	volatile cbd_t *bdp;
	struct	sk_buff	*skb;
	ushort	pkt_len;
	__u8 *data;

	fep = dev->priv;
	fecp = (volatile fec_t*)dev->base_addr;

	/* First, grab all of the stats for the incoming packet.
	 * These get messed up if we get called due to a busy condition.
	 */
	bdp = fep->cur_rx;

while (!(bdp->cbd_sc & BD_ENET_RX_EMPTY)) {

#ifndef final_version
	/* Since we have allocated space to hold a complete frame,
	 * the last indicator should be set.
	 */
	if ((bdp->cbd_sc & BD_ENET_RX_LAST) == 0)
		printk("FEC ENET: rcv is not +last\n");
#endif

	/* Check for errors. */
	if (bdp->cbd_sc & (BD_ENET_RX_LG | BD_ENET_RX_SH | BD_ENET_RX_NO |
			   BD_ENET_RX_CR | BD_ENET_RX_OV)) {
		fep->stats.rx_errors++;
		if (bdp->cbd_sc & (BD_ENET_RX_LG | BD_ENET_RX_SH)) {
		/* Frame too long or too short. */
			fep->stats.rx_length_errors++;
		}
		if (bdp->cbd_sc & BD_ENET_RX_NO)	/* Frame alignment */
			fep->stats.rx_frame_errors++;
		if (bdp->cbd_sc & BD_ENET_RX_CR)	/* CRC Error */
			fep->stats.rx_crc_errors++;
		if (bdp->cbd_sc & BD_ENET_RX_OV)	/* FIFO overrun */
			fep->stats.rx_crc_errors++;
	}

	/* Report late collisions as a frame error.
	 * On this error, the BD is closed, but we don't know what we
	 * have in the buffer.  So, just drop this frame on the floor.
	 */
	if (bdp->cbd_sc & BD_ENET_RX_CL) {
		fep->stats.rx_errors++;
		fep->stats.rx_frame_errors++;
		goto rx_processing_done;
	}

	/* Process the incoming frame.
	 */
	fep->stats.rx_packets++;
	pkt_len = bdp->cbd_datlen;
	fep->stats.rx_bytes += pkt_len;
	data = fep->rx_vaddr[bdp - fep->rx_bd_base];

	/* This does 16 byte alignment, exactly what we need.
	 * The packet length includes FCS, but we don't want to
	 * include that when passing upstream as it messes up
	 * bridging applications.
	 */
	skb = dev_alloc_skb(pkt_len-4);

	if (skb == NULL) {
		printk("%s: Memory squeeze, dropping packet.\n", dev->name);
		fep->stats.rx_dropped++;
	} else {
		skb->dev = dev;
		skb_put(skb,pkt_len-4);	/* Make room */
		eth_copy_and_sum(skb, data, pkt_len-4, 0);
		skb->protocol=eth_type_trans(skb,dev);
		netif_rx(skb);
	}
  rx_processing_done:

	/* Clear the status flags for this buffer.
	*/
	bdp->cbd_sc &= ~BD_ENET_RX_STATS;

	/* Mark the buffer empty.
	*/
	bdp->cbd_sc |= BD_ENET_RX_EMPTY;

	/* Update BD pointer to next entry.
	*/
	if (bdp->cbd_sc & BD_ENET_RX_WRAP)
		bdp = fep->rx_bd_base;
	else
		bdp++;

#if 1
	/* Doing this here will keep the FEC running while we process
	 * incoming frames.  On a heavily loaded network, we should be
	 * able to keep up at the expense of system resources.
	 */
	fecp->fec_r_des_active = 0x01000000;
#endif
   } /* while (!(bdp->cbd_sc & BD_ENET_RX_EMPTY)) */
	fep->cur_rx = (cbd_t *)bdp;

#if 0
	/* Doing this here will allow us to process all frames in the
	 * ring before the FEC is allowed to put more there.  On a heavily
	 * loaded network, some frames may be lost.  Unfortunately, this
	 * increases the interrupt overhead since we can potentially work
	 * our way back to the interrupt return only to come right back
	 * here.
	 */
	fecp->fec_r_des_active = 0x01000000;
#endif
}


#ifdef	CONFIG_USE_MDIO
static void
fec_enet_mii(struct net_device *dev)
{
	struct	fec_enet_private *fep;
	volatile fec_t	*ep;
	mii_list_t	*mip;
	uint		mii_reg;

	fep = (struct fec_enet_private *)dev->priv;
	ep = &(((immap_t *)IMAP_ADDR)->im_cpm.cp_fec);
	mii_reg = ep->fec_mii_data;

	/* Ignore this answer if the request queue has been stopped.
	 * The request will be re-issued when the queue is restarted.
	 */
	if (mii_stopped)
		return;

	if ((mip = mii_head) == NULL) {
		printk("MII and no head!\n");
		return;
	}

	if (mip->mii_func != NULL)
		(*(mip->mii_func))(mii_reg, dev, mip->mii_data);

	mii_head = mip->mii_next;
	mip->mii_next = mii_free;
	mii_free = mip;

	if ((mip = mii_head) != NULL) {
		ep->fec_mii_data = mip->mii_regval;
	}
}

static int
mii_queue(struct net_device *dev, int regval, void (*func)(uint, struct net_device *, uint), uint data)
{
	struct fec_enet_private *fep;
	unsigned long	flags;
	mii_list_t	*mip;
	int		retval;

	/* Add PHY address to register command.
	*/
	fep = dev->priv;
	regval |= fep->phy_addr << 23;

	retval = 0;

	save_flags(flags);
	cli();

	if ((mip = mii_free) != NULL) {
		mii_free = mip->mii_next;
		mip->mii_regval = regval;
		mip->mii_func = func;
		mip->mii_next = NULL;
		mip->mii_data = data;
		if (mii_head) {
			mii_tail->mii_next = mip;
			mii_tail = mip;
		} else {
			mii_head = mii_tail = mip;
			if (!mii_stopped)
				(&(((immap_t *)IMAP_ADDR)->im_cpm.cp_fec))->fec_mii_data = regval;
		}
	} else {
		retval = 1;
	}

	restore_flags(flags);

	return(retval);
}

/* Functions to stop/start the transmission of requests through the MDIO.
 * They are intended to be used before and after a FEC reset.
 */
static void stop_mii_queue(struct net_device *dev)
{
	unsigned long flags;
	save_flags(flags);
	cli();
	mii_stopped = 1;
	restore_flags(flags);
}

static void start_mii_queue(struct net_device *dev)
{
	unsigned long flags;
	volatile fec_t *ep = &(((immap_t *)IMAP_ADDR)->im_cpm.cp_fec);
	save_flags(flags);
	cli();
	mii_stopped = 0;
	if(mii_head)
		ep->fec_mii_data = mii_head->mii_regval;
	restore_flags(flags);
}

static void mii_do_cmd(struct net_device *dev, const phy_cmd_t *c)
{
	int k;

	if(!c)
		return;

	for(k = 0; (c+k)->mii_data != mk_mii_end; k++)
		mii_queue(dev, (c+k)->mii_data, (c+k)->funct, 0);
}

static void mii_parse_sr(uint mii_reg, struct net_device *dev, uint data)
{
	struct fec_enet_private *fep = dev->priv;
	volatile uint *s = &(fep->phy_status);

	*s &= ~(PHY_STAT_LINK | PHY_STAT_FAULT | PHY_STAT_ANC);

	if (mii_reg & 0x0004)
		*s |= PHY_STAT_LINK;
	if (mii_reg & 0x0010)
		*s |= PHY_STAT_FAULT;
	if (mii_reg & 0x0020)
		*s |= PHY_STAT_ANC;

	fep->link = (*s & PHY_STAT_LINK) ? 1 : 0;
}

static void mii_parse_cr(uint mii_reg, struct net_device *dev, uint data)
{
	struct fec_enet_private *fep = dev->priv;
	volatile uint *s = &(fep->phy_status);

	*s &= ~(PHY_CONF_ANE | PHY_CONF_LOOP);

	if (mii_reg & 0x1000)
		*s |= PHY_CONF_ANE;
	if (mii_reg & 0x4000)
		*s |= PHY_CONF_LOOP;
}

static void mii_parse_anar(uint mii_reg, struct net_device *dev, uint data)
{
	struct fec_enet_private *fep = dev->priv;
	volatile uint *s = &(fep->phy_status);

	*s &= ~(PHY_CONF_SPMASK);

	if (mii_reg & 0x0020)
		*s |= PHY_CONF_10HDX;
	if (mii_reg & 0x0040)
		*s |= PHY_CONF_10FDX;
	if (mii_reg & 0x0080)
		*s |= PHY_CONF_100HDX;
	if (mii_reg & 0x00100)
		*s |= PHY_CONF_100FDX;
}
#if 0
static void mii_disp_reg(uint mii_reg, struct net_device *dev)
{
	printk("reg %u = 0x%04x\n", (mii_reg >> 18) & 0x1f, mii_reg & 0xffff);
}
#endif

/* ------------------------------------------------------------------------- */
/* The Level one LXT970 is used by many boards				     */

#ifdef CONFIG_FEC_LXT970

#define MII_LXT970_MIRROR    16  /* Mirror register           */
#define MII_LXT970_IER       17  /* Interrupt Enable Register */
#define MII_LXT970_ISR       18  /* Interrupt Status Register */
#define MII_LXT970_CONFIG    19  /* Configuration Register    */
#define MII_LXT970_CSR       20  /* Chip Status Register      */

static void mii_parse_lxt970_csr(uint mii_reg, struct net_device *dev, uint data)
{
	struct fec_enet_private *fep = dev->priv;
	volatile uint *s = &(fep->phy_status);

	*s &= ~(PHY_STAT_SPMASK);

	if (mii_reg & 0x0800) {
		if (mii_reg & 0x1000)
			*s |= PHY_STAT_100FDX;
		else
			*s |= PHY_STAT_100HDX;
	}
	else {
		if (mii_reg & 0x1000)
			*s |= PHY_STAT_10FDX;
		else
			*s |= PHY_STAT_10HDX;
	}
}

static phy_info_t phy_info_lxt970 = {
	0x07810000,
	"LXT970",

	(const phy_cmd_t []) {  /* config */
#if 0
//		{ mk_mii_write(MII_REG_ANAR, 0x0021), NULL },

		/* Set default operation of 100-TX....for some reason
		 * some of these bits are set on power up, which is wrong.
		 */
		{ mk_mii_write(MII_LXT970_CONFIG, 0), NULL },
#endif
		{ mk_mii_read(MII_REG_CR), mii_parse_cr },
		{ mk_mii_read(MII_REG_ANAR), mii_parse_anar },
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) {  /* startup - enable interrupts */
		{ mk_mii_write(MII_LXT970_IER, 0x0002), NULL },
		{ mk_mii_write(MII_REG_CR, 0x1200), NULL }, /* autonegotiate */
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) { /* ack_int */
		/* read SR and ISR to acknowledge */

		{ mk_mii_read(MII_REG_SR), mii_parse_sr },
		{ mk_mii_read(MII_LXT970_ISR), NULL },

		/* find out the current status */

		{ mk_mii_read(MII_LXT970_CSR), mii_parse_lxt970_csr },
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) {  /* shutdown - disable interrupts */
		{ mk_mii_write(MII_LXT970_IER, 0x0000), NULL },
		{ mk_mii_end, }
	},
};

#endif /* CONFIG_FEC_LXT970 */

/* ------------------------------------------------------------------------- */
/* The Level one LXT971 is used on some of my custom boards                  */

#ifdef CONFIG_FEC_LXT971

/* register definitions for the 971 */

#define MII_LXT971_PCR       16  /* Port Control Register     */
#define MII_LXT971_SR2       17  /* Status Register 2         */
#define MII_LXT971_IER       18  /* Interrupt Enable Register */
#define MII_LXT971_ISR       19  /* Interrupt Status Register */
#define MII_LXT971_LCR       20  /* LED Control Register      */
#define MII_LXT971_TCR       30  /* Transmit Control Register */

/*
 * I had some nice ideas of running the MDIO faster...
 * The 971 should support 8MHz and I tried it, but things acted really
 * weird, so 2.5 MHz ought to be enough for anyone...
 */

static void mii_parse_lxt971_sr2(uint mii_reg, struct net_device *dev, uint data)
{
	struct fec_enet_private *fep = dev->priv;
	volatile uint *s = &(fep->phy_status);

	*s &= ~(PHY_STAT_SPMASK);

	if (mii_reg & 0x4000) {
		if (mii_reg & 0x0200)
			*s |= PHY_STAT_100FDX;
		else
			*s |= PHY_STAT_100HDX;
	}
	else {
		if (mii_reg & 0x0200)
			*s |= PHY_STAT_10FDX;
		else
			*s |= PHY_STAT_10HDX;
	}
	if (mii_reg & 0x0008)
		*s |= PHY_STAT_FAULT;
}

static phy_info_t phy_info_lxt971 = {
	0x0001378e,
	"LXT971",

	(const phy_cmd_t []) {  /* config */
//		{ mk_mii_write(MII_REG_ANAR, 0x021), NULL }, /* 10  Mbps, HD */
		{ mk_mii_read(MII_REG_CR), mii_parse_cr },
		{ mk_mii_read(MII_REG_ANAR), mii_parse_anar },
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) {  /* startup - enable interrupts */
		{ mk_mii_write(MII_LXT971_IER, 0x00f2), NULL },
		{ mk_mii_write(MII_REG_CR, 0x1200), NULL }, /* autonegotiate */

		/* Somehow does the 971 tell me that the link is down
		 * the first read after power-up.
		 * read here to get a valid value in ack_int */

		{ mk_mii_read(MII_REG_SR), mii_parse_sr },
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) { /* ack_int */
		/* find out the current status */

		{ mk_mii_read(MII_REG_SR), mii_parse_sr },
		{ mk_mii_read(MII_LXT971_SR2), mii_parse_lxt971_sr2 },

		/* we only need to read ISR to acknowledge */

		{ mk_mii_read(MII_LXT971_ISR), NULL },
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) {  /* shutdown - disable interrupts */
		{ mk_mii_write(MII_LXT971_IER, 0x0000), NULL },
		{ mk_mii_end, }
	},
};

#endif /* CONFIG_FEC_LXT970 */


/* ------------------------------------------------------------------------- */
/* The Quality Semiconductor QS6612 is used on the RPX CLLF                  */

#ifdef CONFIG_FEC_QS6612

/* register definitions */

#define MII_QS6612_MCR       17  /* Mode Control Register      */
#define MII_QS6612_FTR       27  /* Factory Test Register      */
#define MII_QS6612_MCO       28  /* Misc. Control Register     */
#define MII_QS6612_ISR       29  /* Interrupt Source Register  */
#define MII_QS6612_IMR       30  /* Interrupt Mask Register    */
#define MII_QS6612_PCR       31  /* 100BaseTx PHY Control Reg. */

static void mii_parse_qs6612_pcr(uint mii_reg, struct net_device *dev, uint data)
{
	struct fec_enet_private *fep = dev->priv;
	volatile uint *s = &(fep->phy_status);

	*s &= ~(PHY_STAT_SPMASK);

	switch((mii_reg >> 2) & 7) {
	case 1: *s |= PHY_STAT_10HDX; break;
	case 2: *s |= PHY_STAT_100HDX; break;
	case 5: *s |= PHY_STAT_10FDX; break;
	case 6: *s |= PHY_STAT_100FDX; break;
	}
}

static phy_info_t phy_info_qs6612 = {
	0x00181440,
	"QS6612",

	(const phy_cmd_t []) {  /* config */
//	{ mk_mii_write(MII_REG_ANAR, 0x061), NULL }, /* 10  Mbps */

		/* The PHY powers up isolated on the RPX,
		 * so send a command to allow operation.
		 */

		{ mk_mii_write(MII_QS6612_PCR, 0x0dc0), NULL },

		/* parse cr and anar to get some info */

		{ mk_mii_read(MII_REG_CR), mii_parse_cr },
		{ mk_mii_read(MII_REG_ANAR), mii_parse_anar },
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) {  /* startup - enable interrupts */
		{ mk_mii_write(MII_QS6612_IMR, 0x0050), NULL },
		{ mk_mii_write(MII_REG_CR, 0x1200), NULL }, /* autonegotiate */
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) { /* ack_int */

		/* we need to read ISR, SR and ANER to acknowledge */

		{ mk_mii_read(MII_QS6612_ISR), NULL },
		{ mk_mii_read(MII_REG_SR), mii_parse_sr },
		{ mk_mii_read(MII_REG_ANER), NULL },

		/* read pcr to get info */

		{ mk_mii_read(MII_QS6612_PCR), mii_parse_qs6612_pcr },
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) {  /* shutdown - disable interrupts */
		{ mk_mii_write(MII_QS6612_IMR, 0x0000), NULL },
		{ mk_mii_end, }
	},
};


#endif /* CONFIG_FEC_QS6612 */


/* -------------------------------------------------------------------- */
/* The National Semiconductor DP83843BVJE is used on a Mediatrix board  */
/* -------------------------------------------------------------------- */

#ifdef CONFIG_FEC_DP83843

/* Register definitions */
#define MII_DP83843_PHYSTS 0x10  /* PHY Status Register */
#define MII_DP83843_MIPSCR 0x11  /* Specific Status Register */
#define MII_DP83843_MIPGSR 0x12  /* Generic Status Register */

static void mii_parse_dp83843_physts(uint mii_reg, struct net_device *dev, uint data)
{
	struct fec_enet_private *fep = dev->priv;
	volatile uint *s = &(fep->phy_status);

	*s &= ~(PHY_STAT_SPMASK);

	if (mii_reg & 0x0002)
	{
		if (mii_reg & 0x0004)
			*s |= PHY_STAT_10FDX;
		else
			*s |= PHY_STAT_10HDX;
	}
	else
	{
		if (mii_reg & 0x0004)
			*s |= PHY_STAT_100FDX;
		else
			*s |= PHY_STAT_100HDX;
	}
}

static phy_info_t phy_info_dp83843 = {
	0x020005c1,
	"DP83843BVJE",

	(const phy_cmd_t []) {  /* config */
		{ mk_mii_write(MII_REG_ANAR, 0x01E1), NULL  }, /* Auto-Negociation Register Control set to    */
		                                               /* auto-negociate 10/100MBps, Half/Full duplex */
		{ mk_mii_read(MII_REG_CR),   mii_parse_cr   },
		{ mk_mii_read(MII_REG_ANAR), mii_parse_anar },
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) {  /* startup */
		{ mk_mii_write(MII_DP83843_MIPSCR, 0x0002), NULL }, /* Enable interrupts */
		{ mk_mii_write(MII_REG_CR, 0x1200), NULL         }, /* Enable and Restart Auto-Negotiation */
		{ mk_mii_read(MII_REG_SR), mii_parse_sr	         },
		{ mk_mii_read(MII_REG_CR), mii_parse_cr },
		{ mk_mii_read(MII_DP83843_PHYSTS), mii_parse_dp83843_physts },
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) { /* ack_int */
		{ mk_mii_read(MII_DP83843_MIPGSR), NULL },  /* Acknowledge interrupts */
		{ mk_mii_read(MII_REG_SR), mii_parse_sr },  /* Find out the current status */
		{ mk_mii_read(MII_REG_CR), mii_parse_cr },
		{ mk_mii_read(MII_DP83843_PHYSTS), mii_parse_dp83843_physts },
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) {  /* shutdown - disable interrupts */
		{ mk_mii_end, }
	}
};

#endif /* CONFIG_FEC_DP83843 */


/* ----------------------------------------------------------------- */
/* The National Semiconductor DP83846A is used on a Mediatrix board  */
/* ----------------------------------------------------------------- */

#ifdef CONFIG_FEC_DP83846A

/* Register definitions */
#define MII_DP83846A_PHYSTS 0x10  /* PHY Status Register */

static void mii_parse_dp83846a_physts(uint mii_reg, struct net_device *dev, uint data)
{
	struct fec_enet_private *fep = (struct fec_enet_private *)dev->priv;
	volatile uint *s = &(fep->phy_status);
	int link_change_mask;

	*s &= ~(PHY_STAT_SPMASK);

	if (mii_reg & 0x0002) {
		if (mii_reg & 0x0004)
			*s |= PHY_STAT_10FDX;
		else
			*s |= PHY_STAT_10HDX;
	}
	else {
		if (mii_reg & 0x0004)
			*s |= PHY_STAT_100FDX;
		else
			*s |= PHY_STAT_100HDX;
	}

	link_change_mask = PHY_STAT_LINK | PHY_STAT_10FDX | PHY_STAT_10HDX | PHY_STAT_100FDX | PHY_STAT_100HDX;
	if(fep->old_status != (link_change_mask & *s))
	{
		fep->old_status = (link_change_mask & *s);
		mii_queue_relink(mii_reg, dev, 0);
	}
}

static phy_info_t phy_info_dp83846a = {
	0x020005c2,
	"DP83846A",

	(const phy_cmd_t []) {  /* config */
		{ mk_mii_write(MII_REG_ANAR, 0x01E1), NULL  }, /* Auto-Negociation Register Control set to    */
		                                               /* auto-negociate 10/100MBps, Half/Full duplex */
		{ mk_mii_read(MII_REG_CR),   mii_parse_cr   },
		{ mk_mii_read(MII_REG_ANAR), mii_parse_anar },
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) {  /* startup */
		{ mk_mii_write(MII_REG_CR, 0x1200), NULL }, /* Enable and Restart Auto-Negotiation */
		{ mk_mii_read(MII_REG_SR), mii_parse_sr },
		{ mk_mii_read(MII_REG_CR), mii_parse_cr   },
		{ mk_mii_read(MII_DP83846A_PHYSTS), mii_parse_dp83846a_physts },
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) { /* ack_int */
		{ mk_mii_read(MII_REG_SR), mii_parse_sr },
		{ mk_mii_read(MII_REG_CR), mii_parse_cr   },
		{ mk_mii_read(MII_DP83846A_PHYSTS), mii_parse_dp83846a_physts },
		{ mk_mii_end, }
	},
	(const phy_cmd_t []) {  /* shutdown - disable interrupts */
		{ mk_mii_end, }
	}
};

#endif /* CONFIG_FEC_DP83846A */


static phy_info_t *phy_info[] = {

#ifdef CONFIG_FEC_LXT970
	&phy_info_lxt970,
#endif /* CONFIG_FEC_LXT970 */

#ifdef CONFIG_FEC_LXT971
	&phy_info_lxt971,
#endif /* CONFIG_FEC_LXT971 */

#ifdef CONFIG_FEC_QS6612
	&phy_info_qs6612,
#endif /* CONFIG_FEC_LXT971 */

#ifdef CONFIG_FEC_DP83843
	&phy_info_dp83843,
#endif /* CONFIG_FEC_DP83843 */

#ifdef CONFIG_FEC_DP83846A
	&phy_info_dp83846a,
#endif /* CONFIG_FEC_DP83846A */

	NULL
};

static void mii_display_status(struct net_device *dev)
{
	struct fec_enet_private *fep = dev->priv;
	volatile uint *s = &(fep->phy_status);

	if (!fep->link && !fep->old_link) {
		/* Link is still down - don't print anything */
		return;
	}

	printk("%s: status: ", dev->name);

	if (!fep->link) {
		printk("link down");
	} else {
		printk("link up");

		switch(*s & PHY_STAT_SPMASK) {
		case PHY_STAT_100FDX: printk(", 100 Mbps Full Duplex"); break;
		case PHY_STAT_100HDX: printk(", 100 Mbps Half Duplex"); break;
		case PHY_STAT_10FDX: printk(", 10 Mbps Full Duplex"); break;
		case PHY_STAT_10HDX: printk(", 10 Mbps Half Duplex"); break;
		default:
			printk(", Unknown speed/duplex");
		}

		if (*s & PHY_STAT_ANC)
			printk(", auto-negotiation complete");
	}

	if (*s & PHY_STAT_FAULT)
		printk(", remote fault");

	printk(".\n");
}

static void mii_display_config(struct net_device *dev)
{
	struct fec_enet_private *fep = dev->priv;
	volatile uint *s = &(fep->phy_status);

	printk("%s: config: auto-negotiation ", dev->name);

	if (*s & PHY_CONF_ANE)
		printk("on");
	else
		printk("off");

	if (*s & PHY_CONF_100FDX)
		printk(", 100FDX");
	if (*s & PHY_CONF_100HDX)
		printk(", 100HDX");
	if (*s & PHY_CONF_10FDX)
		printk(", 10FDX");
	if (*s & PHY_CONF_10HDX)
		printk(", 10HDX");
	if (!(*s & PHY_CONF_SPMASK))
		printk(", No speed/duplex selected?");

	if (*s & PHY_CONF_LOOP)
		printk(", loopback enabled");

	printk(".\n");

	fep->sequence_done = 1;
}

static void mii_relink(struct net_device *dev)
{
	struct fec_enet_private *fep = dev->priv;
	int duplex;

	fep->link = (fep->phy_status & PHY_STAT_LINK) ? 1 : 0;
	mii_display_status(dev);
	fep->old_link = fep->link;

	if (fep->link) {
		duplex = 0;
		if (fep->phy_status
		    & (PHY_STAT_100FDX | PHY_STAT_10FDX))
			duplex = 1;
		fec_restart(dev, duplex);
	}
	else
		fec_stop(dev);

#if 0
	enable_irq(fep->mii_irq);
#endif

}

static void mii_queue_relink(uint mii_reg, struct net_device *dev, uint data)
{
	struct fec_enet_private *fep = dev->priv;

	fep->phy_task.routine = (void *)mii_relink;
	fep->phy_task.data = dev;
	schedule_task(&fep->phy_task);
}

static void mii_queue_config(uint mii_reg, struct net_device *dev, uint data)
{
	struct fec_enet_private *fep = dev->priv;

	fep->phy_task.routine = (void *)mii_display_config;
	fep->phy_task.data = dev;
	schedule_task(&fep->phy_task);
}



phy_cmd_t phy_cmd_relink[] = { { mk_mii_read(MII_REG_CR), mii_queue_relink },
			       { mk_mii_end, } };
phy_cmd_t phy_cmd_config[] = { { mk_mii_read(MII_REG_CR), mii_queue_config },
			       { mk_mii_end, } };



/* Read remainder of PHY ID.
*/
static void
mii_discover_phy3(uint mii_reg, struct net_device *dev, uint data)
{
	struct fec_enet_private *fep;
	int	i;

	fep = dev->priv;
	fep->phy_id |= (mii_reg & 0xffff);

	for(i = 0; phy_info[i]; i++)
		if(phy_info[i]->id == (fep->phy_id >> 4))
			break;

	if(!phy_info[i])
		panic("%s: PHY id 0x%08x is not supported!\n",
		      dev->name, fep->phy_id);

	fep->phy = phy_info[i];
	fep->phy_id_done = 1;

	printk("%s: Phy @ 0x%x, type %s (0x%08x)\n",
		dev->name, fep->phy_addr, fep->phy->name, fep->phy_id);
}

/* Scan all of the MII PHY addresses looking for someone to respond
 * with a valid ID.  This usually happens quickly.
 */
static void
mii_discover_phy(uint mii_reg, struct net_device *dev, uint data)
{
	struct fec_enet_private *fep;
	uint	phytype;

	fep = dev->priv;

	if ((phytype = (mii_reg & 0xffff)) != 0xffff) {

		/* Got first part of ID, now get remainder.
		*/
		fep->phy_id = phytype << 16;
		mii_queue(dev, mk_mii_read(MII_REG_PHYIR2), mii_discover_phy3, 0);
	} else {
		fep->phy_addr++;
		if (fep->phy_addr < 32) {
			mii_queue(dev, mk_mii_read(MII_REG_PHYIR1),
							mii_discover_phy, 0);
		} else {
			printk("fec: No PHY device found.\n");
		}
	}
}
#endif	/* CONFIG_USE_MDIO */

/* This interrupt occurs when the PHY detects a link change.
*/
static void
mii_link_interrupt(int irq, void * dev_id, struct pt_regs * regs)
{
#ifdef	CONFIG_USE_MDIO
	struct	net_device *dev = dev_id;
	struct fec_enet_private *fep = dev->priv;

	/*
	 * Acknowledge the interrupt if possible. If we have not
	 * found the PHY yet we can't process or acknowledge the
	 * interrupt now. Instead we ignore this interrupt for now,
	 * which we can do since it is edge triggered. It will be
	 * acknowledged later by fec_enet_open().
	 */
	if (fep->phy) {
		mii_do_cmd(dev, fep->phy->ack_int);
		mii_do_cmd(dev, phy_cmd_relink);  /* restart and display status */
	}
#else
	printk ("FEC: unexpected Link interrupt\n");
#endif	/* CONFIG_USE_MDIO */

}

static int
fec_enet_open(struct net_device *dev)
{
	struct fec_enet_private *fep = dev->priv;

	/* I should reset the ring buffers here, but I don't yet know
	 * a simple way to do that.
	 */

#ifdef	CONFIG_USE_MDIO
	fep->sequence_done = 0;
	fep->link = 0;

	if (fep->phy) {
		mii_do_cmd(dev, fep->phy->config);
		mii_do_cmd(dev, phy_cmd_config);  /* display configuration */
		while(!fep->sequence_done)
			schedule();

		mii_do_cmd(dev, fep->phy->startup);

#if defined(CONFIG_USE_MDIO) && defined(CONFIG_FEC_DP83846A)
		if(fep->phy == &phy_info_dp83846a)
		{
			/* Initializing timers
			 */
			init_timer( &fep->phy_timer_list );

			/* Starting timer for periodic link status check
			 * After 100 milli-seconds, mdio_timer_callback function is called.
			 */
			fep->phy_timer_list.expires  = jiffies + (100 * HZ / 1000);
			fep->phy_timer_list.data     = (unsigned long)dev;
			fep->phy_timer_list.function = mdio_timer_callback;
			add_timer( &fep->phy_timer_list );
		}

#if defined(CONFIG_IP_PNP)
        printk("%s: Waiting for the link to be up...\n", dev->name);

        while(fep->link == 0 || ((((volatile fec_t*)dev->base_addr)->fec_ecntrl & FEC_ECNTRL_ETHER_EN) == 0))
        {
            schedule();
        }
#endif /* CONFIG_IP_PNP */

#endif /* CONFIG_USE_MDIO && CONFIG_FEC_DP83846A */

		netif_start_queue(dev);
		return 0;		/* Success */
	}
	return -ENODEV;		/* No PHY we understand */
#else
	fep->link = 1;
	netif_start_queue(dev);
	return 0;	/* Success */
#endif	/* CONFIG_USE_MDIO */

}

static int
fec_enet_close(struct net_device *dev)
{
	/* Don't know what to do yet.
	*/
	netif_stop_queue(dev);
	fec_stop(dev);

	return 0;
}

static struct net_device_stats *fec_enet_get_stats(struct net_device *dev)
{
	struct fec_enet_private *fep = (struct fec_enet_private *)dev->priv;

	return &fep->stats;
}

#ifdef CONFIG_USE_MDIO

#if defined(CONFIG_FEC_DP83846A)
/* Execute the ack_int command set and schedules next timer call back.  */
static void mdio_timer_callback(unsigned long data)
{
	struct net_device *dev = (struct net_device *)data;
	struct fec_enet_private *fep = (struct fec_enet_private *)(dev->priv);
	mii_do_cmd(dev, fep->phy->ack_int);

	if(fep->link == 0)
	{
		fep->phy_timer_list.expires  = jiffies + (100 * HZ / 1000); /* Sleep for 100ms */
	}
	else
	{
		fep->phy_timer_list.expires  = jiffies + (1 * HZ); /* Sleep for 1 sec. */
	}
	add_timer( &fep->phy_timer_list );
}
#endif /* CONFIG_FEC_DP83846A */

static void mdio_callback(uint regval, struct net_device *dev, uint data)
{
	mdio_read_data_t* mrd = (mdio_read_data_t *)data;
	mrd->regval = 0xFFFF & regval;
	wake_up_process(mrd->sleeping_task);
}

static int mdio_read(struct net_device *dev, int phy_id, int location)
{
	uint retval;
	mdio_read_data_t* mrd = (mdio_read_data_t *)kmalloc(sizeof(*mrd), GFP_KERNEL);

	mrd->sleeping_task = current;
	set_current_state(TASK_INTERRUPTIBLE);
	mii_queue(dev, mk_mii_read(location), mdio_callback, (unsigned int) mrd);
	schedule();

	retval = mrd->regval;

	kfree(mrd);

	return retval;
}

void mdio_write(struct net_device *dev, int phy_id, int location, int value)
{
	mii_queue(dev, mk_mii_write(location, value), NULL, 0);
}

static int fec_enet_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct fec_enet_private *cep = (struct fec_enet_private *)dev->priv;
	struct mii_ioctl_data *data = (struct mii_ioctl_data *)&rq->ifr_data;

	int phy = cep->phy_addr & 0x1f;
	int retval;

	if (data == NULL)
	{
		retval = -EINVAL;
	}
	else
	{
		switch(cmd)
		{
		case SIOCETHTOOL:
			return netdev_ethtool_ioctl(dev, (void*)rq->ifr_data);
			break;

		case SIOCGMIIPHY:			/* Get address of MII PHY in use. */
		case SIOCDEVPRIVATE:		/* for binary compat, remove in 2.5 */
			data->phy_id = phy;

		case SIOCGMIIREG:			/* Read MII PHY register.	*/
		case SIOCDEVPRIVATE+1:		/* for binary compat, remove in 2.5 */
			data->val_out = mdio_read(dev, data->phy_id & 0x1f, data->reg_num & 0x1f);
			retval = 0;
			break;

		case SIOCSMIIREG:			/* Write MII PHY register.	*/
		case SIOCDEVPRIVATE+2:		/* for binary compat, remove in 2.5 */
			if (!capable(CAP_NET_ADMIN))
			{
				retval = -EPERM;
			}
			else
			{
				mdio_write(dev, data->phy_id & 0x1f, data->reg_num & 0x1f, data->val_in);
				retval = 0;
			}
			break;

		default:
			retval = -EOPNOTSUPP;
			break;
		}
	}
	return retval;
}


static int netdev_ethtool_ioctl (struct net_device *dev, void *useraddr)
{
	u32 ethcmd;

	/* dev_ioctl() in ../../net/core/dev.c has already checked
	   capable(CAP_NET_ADMIN), so don't bother with that here.  */

	if (copy_from_user (&ethcmd, useraddr, sizeof (ethcmd)))
		return -EFAULT;

	switch (ethcmd) {
	case ETHTOOL_GDRVINFO:
		{
			struct ethtool_drvinfo info = { ETHTOOL_GDRVINFO };
			strcpy (info.driver, dev->name);
			strcpy (info.version, "0.2");
			strcpy (info.bus_info, "");
			if (copy_to_user (useraddr, &info, sizeof (info)))
				return -EFAULT;
			return 0;
		}
	default:
		break;
	}

	return -EOPNOTSUPP;
}

#endif	/* CONFIG_USE_MDIO */

/* Set or clear the multicast filter for this adaptor.
 * Skeleton taken from sunlance driver.
 * The CPM Ethernet implementation allows Multicast as well as individual
 * MAC address filtering.  Some of the drivers check to make sure it is
 * a group multicast address, and discard those that are not.  I guess I
 * will do the same for now, but just remove the test if you want
 * individual filtering as well (do the upper net layers want or support
 * this kind of feature?).
 */

static void set_multicast_list(struct net_device *dev)
{
	struct	fec_enet_private *fep;
	volatile fec_t *ep;

	fep = (struct fec_enet_private *)dev->priv;
	ep = &(((immap_t *)IMAP_ADDR)->im_cpm.cp_fec);

	if (dev->flags&IFF_PROMISC) {

		/* Log any net taps. */
		printk("%s: Promiscuous mode enabled.\n", dev->name);
		ep->fec_r_cntrl |= FEC_RCNTRL_PROM;
	} else {

		ep->fec_r_cntrl &= ~FEC_RCNTRL_PROM;

		if (dev->flags & IFF_ALLMULTI) {
			/* Catch all multicast addresses, so set the
			 * filter to all 1's.
			 */
			ep->fec_hash_table_high = 0xffffffff;
			ep->fec_hash_table_low = 0xffffffff;
		}
#if 0
		else {
			/* Clear filter and add the addresses in the list.
			*/
			ep->sen_gaddr1 = 0;
			ep->sen_gaddr2 = 0;
			ep->sen_gaddr3 = 0;
			ep->sen_gaddr4 = 0;

			dmi = dev->mc_list;

			for (i=0; i<dev->mc_count; i++, dmi = dmi->next) {

				/* Only support group multicast for now.
				*/
				if (!(dmi->dmi_addr[0] & 1))
					continue;

				/* The address in dmi_addr is LSB first,
				 * and taddr is MSB first.  We have to
				 * copy bytes MSB first from dmi_addr.
				 */
				mcptr = (u_char *)dmi->dmi_addr + 5;
				tdptr = (u_char *)&ep->sen_taddrh;
				for (j=0; j<6; j++)
					*tdptr++ = *mcptr--;

				/* Ask CPM to run CRC and set bit in
				 * filter mask.
				 */
				cpmp->cp_cpcr = mk_cr_cmd(CPM_CR_CH_SCC1, CPM_CR_SET_GADDR) | CPM_CR_FLG;
				/* this delay is necessary here -- Cort */
				udelay(10);
				while (cpmp->cp_cpcr & CPM_CR_FLG);
			}
		}
#endif
	}
}

/* Initialize the FEC Ethernet on 860T.
 */
int __init fec_enet_init(void)
{
	struct net_device *dev;
	struct fec_enet_private *fep;
	int i, j, k;
	unsigned char	*eap, *iap, *ba;
	dma_addr_t	mem_addr;
	volatile	cbd_t	*bdp;
	cbd_t		*cbd_base;
	volatile	immap_t	*immap;
	volatile	fec_t	*fecp;
	bd_t		*bd;
#ifdef CONFIG_SCC_ENET
	unsigned char	tmpaddr[6];
#endif

	immap = (immap_t *)IMAP_ADDR;	/* pointer to internal registers */

	bd = (bd_t *)__res;

	/* Allocate some private information.
	*/
	fep = (struct fec_enet_private *)kmalloc(sizeof(*fep), GFP_KERNEL);
	if (fep == NULL)
		return -ENOMEM;

	__clear_user(fep,sizeof(*fep));

	/* Create an Ethernet device instance.
	*/
	dev = init_etherdev(0, 0);

	fecp = &(immap->im_cpm.cp_fec);

	/* Whack a reset.  We should wait for this.
	*/
	fecp->fec_ecntrl = FEC_ECNTRL_PINMUX | FEC_ECNTRL_RESET;
	for (i = 0;
	     (fecp->fec_ecntrl & FEC_ECNTRL_RESET) && (i < FEC_RESET_DELAY);
	     ++i) {
		udelay(1);
	}
	if (i == FEC_RESET_DELAY) {
		printk ("FEC Reset timeout!\n");
	}

	/* Set the Ethernet address.  If using multiple Enets on the 8xx,
	 * this needs some work to get unique addresses.
	 */
	eap = (unsigned char *)my_enet_addr;
	iap = bd->bi_enetaddr;

#ifdef CONFIG_SCC_ENET
	/*
         * If a board has Ethernet configured both on a SCC and the
         * FEC, it needs (at least) 2 MAC addresses (we know that Sun
         * disagrees, but anyway). For the FEC port, we create
         * another address by setting one of the address bits above
         * something that would have (up to now) been allocated.
	 */
	for (i=0; i<6; i++)
		tmpaddr[i] = *iap++;
	tmpaddr[3] |= 0x80;
	iap = tmpaddr;
#endif

	for (i=0; i<6; i++) {
		dev->dev_addr[i] = *eap++ = *iap++;
	}

	/* Allocate memory for buffer descriptors.
	*/
	if (((RX_RING_SIZE + TX_RING_SIZE) * sizeof(cbd_t)) > PAGE_SIZE) {
		printk("FEC init error.  Need more space.\n");
		printk("FEC initialization failed.\n");
		return 1;
	}
	cbd_base = (cbd_t *)consistent_alloc(GFP_KERNEL, PAGE_SIZE, &mem_addr);

	/* Set receive and transmit descriptor base.
	*/
	fep->rx_bd_base = cbd_base;
	fep->tx_bd_base = cbd_base + RX_RING_SIZE;

	fep->skb_cur = fep->skb_dirty = 0;

	/* Initialize the receive buffer descriptors.
	*/
	bdp = fep->rx_bd_base;
	k = 0;
	for (i=0; i<FEC_ENET_RX_PAGES; i++) {

		/* Allocate a page.
		*/
		ba = (unsigned char *)consistent_alloc(GFP_KERNEL, PAGE_SIZE, &mem_addr);

		/* Initialize the BD for every fragment in the page.
		*/
		for (j=0; j<FEC_ENET_RX_FRPPG; j++) {
			bdp->cbd_sc = BD_ENET_RX_EMPTY;
			bdp->cbd_bufaddr = mem_addr;
			fep->rx_vaddr[k++] = ba;
			mem_addr += FEC_ENET_RX_FRSIZE;
			ba += FEC_ENET_RX_FRSIZE;
			bdp++;
		}
	}

	/* Set the last buffer to wrap.
	*/
	bdp--;
	bdp->cbd_sc |= BD_SC_WRAP;

	/* Install our interrupt handler. */
	if (request_irq(FEC_INTERRUPT, fec_enet_interrupt, 0, "fec", dev) != 0)
		panic("Could not allocate FEC IRQ!");

#ifdef CONFIG_RPXCLASSIC
	/* Make Port C, bit 15 an input that causes interrupts.
	*/
	immap->im_ioport.iop_pcpar &= ~0x0001;
	immap->im_ioport.iop_pcdir &= ~0x0001;
	immap->im_ioport.iop_pcso  &= ~0x0001;
	immap->im_ioport.iop_pcint |=  0x0001;
	if (request_irq(CPM_IRQ_OFFSET + CPMVEC_PIO_PC15, mii_link_interrupt,
			0, cpm_int_name[CPMVEC_PIO_PC15], dev) != 0)
		panic("Could not allocate Port C FEC IRQ!");

	/* Make LEDS reflect Link status.
	*/
	*((uint *) RPX_CSR_ADDR) &= ~BCSR2_FETHLEDMODE;
#endif

#ifdef PHY_INTERRUPT
	((immap_t *)IMAP_ADDR)->im_siu_conf.sc_siel |=
		(0x80000000 >> PHY_INTERRUPT);

	if (request_irq(PHY_INTERRUPT, mii_link_interrupt, 0, "mii", dev) != 0)
		panic("Could not allocate MII IRQ!");
#endif

	dev->base_addr = (unsigned long)fecp;
	dev->priv = fep;

	/* The FEC Ethernet specific entries in the device structure. */
	dev->open = fec_enet_open;
	dev->hard_start_xmit = fec_enet_start_xmit;
	dev->tx_timeout = fec_timeout;
	dev->watchdog_timeo = TX_TIMEOUT;
	dev->stop = fec_enet_close;
	dev->get_stats = fec_enet_get_stats;
	dev->set_multicast_list = set_multicast_list;

#ifdef	CONFIG_USE_MDIO
	dev->do_ioctl = fec_enet_ioctl;

	for (i=0; i<NMII-1; i++)
		mii_cmds[i].mii_next = &mii_cmds[i+1];
	mii_free = mii_cmds;
#endif	/* CONFIG_USE_MDIO */

	/* Configure all of port D for MII.
	*/
	immap->im_ioport.iop_pdpar = 0x1fff;

	/* Bits moved from Rev. D onward.
	*/
	if ((mfspr(IMMR) & 0xffff) < 0x0501)
		immap->im_ioport.iop_pddir = 0x1c58;	/* Pre rev. D */
	else
		immap->im_ioport.iop_pddir = 0x1fff;	/* Rev. D and later */

#ifdef	CONFIG_USE_MDIO
	/* Set MII speed to 2.5 MHz
	*/
	fecp->fec_mii_speed = fep->phy_speed =
		(( (bd->bi_intfreq + 500000) / 2500000 / 2 ) & 0x3F ) << 1;
#else
	fecp->fec_mii_speed = 0;	/* turn off MDIO */
#endif	/* CONFIG_USE_MDIO */

	printk ("%s: FEC ENET Version 0.2, FEC irq %d"
#ifdef PHY_INTERRUPT
		", MII irq %d"
#endif
		", addr ",
		dev->name, FEC_INTERRUPT
#ifdef PHY_INTERRUPT
		, PHY_INTERRUPT
#endif
	);
	for (i=0; i<6; i++)
		printk("%02x%c", dev->dev_addr[i], (i==5) ? '\n' : ':');

#ifdef	CONFIG_USE_MDIO	/* start in full duplex mode, and negotiate speed */
	fec_restart (dev, 1);
#else			/* always use half duplex mode only */
	fec_restart (dev, 0);
#endif

#ifdef	CONFIG_USE_MDIO
	/* Queue up command to detect the PHY and initialize the
	 * remainder of the interface.
	 */
	fep->phy_id_done = 0;
	fep->phy_addr = 0;
	mii_queue(dev, mk_mii_read(MII_REG_PHYIR1), mii_discover_phy, 0);

	fep->old_status = 0;
#endif	/* CONFIG_USE_MDIO */

	return 0;
}

/* This function is called to start or restart the FEC during a link
 * change.  This only happens when switching between half and full
 * duplex.
 */
static void
fec_restart(struct net_device *dev, int duplex)
{
	struct fec_enet_private *fep;
	int i;
	volatile	cbd_t	*bdp;
	volatile	immap_t	*immap;
	volatile	fec_t	*fecp;

	immap = (immap_t *)IMAP_ADDR;	/* pointer to internal registers */

	fecp = &(immap->im_cpm.cp_fec);

	fep = dev->priv;

#ifdef CONFIG_USE_MDIO
	/* Stop the MDIO communication prior to reset. */
	stop_mii_queue(dev);
#endif

	/* Whack a reset.  We should wait for this.
	*/
	fecp->fec_ecntrl = FEC_ECNTRL_PINMUX | FEC_ECNTRL_RESET;
	for (i = 0;
	     (fecp->fec_ecntrl & FEC_ECNTRL_RESET) && (i < FEC_RESET_DELAY);
	     ++i) {
		udelay(1);
	}
	if (i == FEC_RESET_DELAY) {
		printk ("FEC Reset timeout!\n");
	}

	/* Set station address.
	*/
	fecp->fec_addr_low  = (my_enet_addr[0] << 16) | my_enet_addr[1];
	fecp->fec_addr_high =  my_enet_addr[2];

	/* Reset all multicast.
	*/
	fecp->fec_hash_table_high = 0;
	fecp->fec_hash_table_low  = 0;

	/* Set maximum receive buffer size.
	*/
	fecp->fec_r_buff_size = PKT_MAXBLR_SIZE;
	fecp->fec_r_hash = PKT_MAXBUF_SIZE;

	/* Set receive and transmit descriptor base.
	*/
	fecp->fec_r_des_start = iopa((uint)(fep->rx_bd_base));
	fecp->fec_x_des_start = iopa((uint)(fep->tx_bd_base));

	fep->dirty_tx = fep->cur_tx = fep->tx_bd_base;
	fep->tx_free = TX_RING_SIZE;
	fep->cur_rx = fep->rx_bd_base;

	/* Reset SKB transmit buffers.
	*/
	fep->skb_cur = fep->skb_dirty = 0;
	for (i=0; i<=TX_RING_MOD_MASK; i++) {
		if (fep->tx_skbuff[i] != NULL) {
			dev_kfree_skb(fep->tx_skbuff[i]);
			fep->tx_skbuff[i] = NULL;
		}
	}

	/* Initialize the receive buffer descriptors.
	*/
	bdp = fep->rx_bd_base;
	for (i=0; i<RX_RING_SIZE; i++) {

		/* Initialize the BD for every fragment in the page.
		*/
		bdp->cbd_sc = BD_ENET_RX_EMPTY;
		bdp++;
	}

	/* Set the last buffer to wrap.
	*/
	bdp--;
	bdp->cbd_sc |= BD_SC_WRAP;

	/* ...and the same for transmmit.
	*/
	bdp = fep->tx_bd_base;
	for (i=0; i<TX_RING_SIZE; i++) {

		/* Initialize the BD for every fragment in the page.
		*/
		bdp->cbd_sc = 0;
		bdp->cbd_bufaddr = 0;
		bdp++;
	}

	/* Set the last buffer to wrap.
	*/
	bdp--;
	bdp->cbd_sc |= BD_SC_WRAP;

	/* Enable MII mode.
	*/
	if (duplex) {
		fecp->fec_r_cntrl = FEC_RCNTRL_MII_MODE;	/* MII enable */
		fecp->fec_x_cntrl = FEC_TCNTRL_FDEN;		/* FD enable */
	}
	else {
		fecp->fec_r_cntrl = FEC_RCNTRL_MII_MODE | FEC_RCNTRL_DRT;
		fecp->fec_x_cntrl = 0;
	}
	fep->full_duplex = duplex;

	/* Enable big endian and don't care about SDMA FC.
	*/
	fecp->fec_fun_code = 0x78000000;

#ifdef	CONFIG_USE_MDIO
	/* Set MII speed.
	*/
	fecp->fec_mii_speed = fep->phy_speed;
#endif	/* CONFIG_USE_MDIO */

	/* Clear any outstanding interrupt.
	*/
	fecp->fec_ievent = 0xffc0;

	fecp->fec_ivec = (FEC_INTERRUPT/2) << 29;

	/* Enable interrupts we wish to service.
	*/
	fecp->fec_imask = ( FEC_ENET_TXF | FEC_ENET_TXB |
			    FEC_ENET_RXF | FEC_ENET_RXB | FEC_ENET_MII );

	/* And last, enable the transmit and receive processing.
	*/
	fecp->fec_ecntrl = FEC_ECNTRL_PINMUX | FEC_ECNTRL_ETHER_EN;
	fecp->fec_r_des_active = 0x01000000;

#ifdef CONFIG_USE_MDIO
	/* Re-start any pending MDIO communication. */
	start_mii_queue(dev);
#endif
}

static void
fec_stop(struct net_device *dev)
{
	volatile	immap_t	*immap;
	volatile	fec_t	*fecp;
	struct fec_enet_private *fep;
	int i;

	immap = (immap_t *)IMAP_ADDR;	/* pointer to internal registers */

	fecp = &(immap->im_cpm.cp_fec);

	if ((fecp->fec_ecntrl & FEC_ECNTRL_ETHER_EN) == 0)
		return;	/* already down */

	fep = dev->priv;


	fecp->fec_x_cntrl = 0x01;	/* Graceful transmit stop */

	for (i = 0;
	     ((fecp->fec_ievent & 0x10000000) == 0) && (i < FEC_RESET_DELAY);
	     ++i) {
		udelay(1);
	}
	if (i == FEC_RESET_DELAY) {
		printk ("FEC timeout on graceful transmit stop\n");
	}

	/* Disable FEC. Let only MII interrupts. */
	fecp->fec_imask = FEC_ENET_MII;
	fecp->fec_ecntrl &= ~(FEC_ECNTRL_ETHER_EN);
}
