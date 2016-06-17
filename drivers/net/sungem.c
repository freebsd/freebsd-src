/* $Id: sungem.c,v 1.44.2.22 2002/03/13 01:18:12 davem Exp $
 * sungem.c: Sun GEM ethernet driver.
 *
 * Copyright (C) 2000, 2001, 2002 David S. Miller (davem@redhat.com)
 * 
 * Support for Apple GMAC and assorted PHYs by
 * Benjamin Herrenscmidt (benh@kernel.crashing.org)
 * 
 * TODO: 
 *  - Get rid of all those nasty mdelay's and replace them
 * with schedule_timeout.
 *  - Implement WOL
 *  - Currently, forced Gb mode is only supported on bcm54xx
 *    PHY for which I use the SPD2 bit of the control register.
 *    On m1011 PHY, I can't force as I don't have the specs, but
 *    I can at least detect gigabit with autoneg.
 */

#include <linux/config.h>

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
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/mii.h>
#include <linux/ethtool.h>
#include <linux/crc32.h>
#include <linux/random.h>

#include <asm/system.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/byteorder.h>
#include <asm/uaccess.h>
#include <asm/irq.h>

#ifdef __sparc__
#include <asm/idprom.h>
#include <asm/openprom.h>
#include <asm/oplib.h>
#include <asm/pbm.h>
#endif

#ifdef CONFIG_ALL_PPC
#include <asm/pci-bridge.h>
#include <asm/prom.h>
#include <asm/machdep.h>
#include <asm/pmac_feature.h>
#endif

#include "sungem.h"

#define DEFAULT_MSG	(NETIF_MSG_DRV		| \
			 NETIF_MSG_PROBE	| \
			 NETIF_MSG_LINK)

#define DRV_NAME	"sungem"
#define DRV_VERSION	"0.97"
#define DRV_RELDATE	"3/20/02"
#define DRV_AUTHOR	"David S. Miller (davem@redhat.com)"

static char version[] __devinitdata =
        DRV_NAME ".c:v" DRV_VERSION " " DRV_RELDATE " " DRV_AUTHOR "\n";

MODULE_AUTHOR(DRV_AUTHOR);
MODULE_DESCRIPTION("Sun GEM Gbit ethernet driver");
MODULE_LICENSE("GPL");

MODULE_PARM(gem_debug, "i");
MODULE_PARM_DESC(gem_debug, "bitmapped message enable number");
MODULE_PARM(link_mode, "i");
MODULE_PARM_DESC(link_mode, "default link mode");

int gem_debug = -1;
static int link_mode;

static u16 link_modes[] __devinitdata = {
	BMCR_ANENABLE,			/* 0 : autoneg */
	0,				/* 1 : 10bt half duplex */
	BMCR_SPEED100,			/* 2 : 100bt half duplex */
	BMCR_SPD2, /* bcm54xx only */   /* 3 : 1000bt half duplex */
	BMCR_FULLDPLX,			/* 4 : 10bt full duplex */
	BMCR_SPEED100|BMCR_FULLDPLX,	/* 5 : 100bt full duplex */
	BMCR_SPD2|BMCR_FULLDPLX		/* 6 : 1000bt full duplex */
};

#define GEM_MODULE_NAME	"gem"
#define PFX GEM_MODULE_NAME ": "

static struct pci_device_id gem_pci_tbl[] __devinitdata = {
	{ PCI_VENDOR_ID_SUN, PCI_DEVICE_ID_SUN_GEM,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },

	/* These models only differ from the original GEM in
	 * that their tx/rx fifos are of a different size and
	 * they only support 10/100 speeds. -DaveM
	 * 
	 * Apple's GMAC does support gigabit on machines with
	 * the BCM54xx PHYs. -BenH
	 */
	{ PCI_VENDOR_ID_SUN, PCI_DEVICE_ID_SUN_RIO_GEM,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ PCI_VENDOR_ID_APPLE, PCI_DEVICE_ID_APPLE_UNI_N_GMAC,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{ PCI_VENDOR_ID_APPLE, PCI_DEVICE_ID_APPLE_UNI_N_GMACP,
	  PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0UL },
	{0, }
};

MODULE_DEVICE_TABLE(pci, gem_pci_tbl);

static u16 __phy_read(struct gem *gp, int reg, int phy_addr)
{
	u32 cmd;
	int limit = 10000;

	cmd  = (1 << 30);
	cmd |= (2 << 28);
	cmd |= (phy_addr << 23) & MIF_FRAME_PHYAD;
	cmd |= (reg << 18) & MIF_FRAME_REGAD;
	cmd |= (MIF_FRAME_TAMSB);
	writel(cmd, gp->regs + MIF_FRAME);

	while (limit--) {
		cmd = readl(gp->regs + MIF_FRAME);
		if (cmd & MIF_FRAME_TALSB)
			break;

		udelay(10);
	}

	if (!limit)
		cmd = 0xffff;

	return cmd & MIF_FRAME_DATA;
}

static inline u16 phy_read(struct gem *gp, int reg)
{
	return __phy_read(gp, reg, gp->mii_phy_addr);
}

static void __phy_write(struct gem *gp, int reg, u16 val, int phy_addr)
{
	u32 cmd;
	int limit = 10000;

	cmd  = (1 << 30);
	cmd |= (1 << 28);
	cmd |= (phy_addr << 23) & MIF_FRAME_PHYAD;
	cmd |= (reg << 18) & MIF_FRAME_REGAD;
	cmd |= (MIF_FRAME_TAMSB);
	cmd |= (val & MIF_FRAME_DATA);
	writel(cmd, gp->regs + MIF_FRAME);

	while (limit--) {
		cmd = readl(gp->regs + MIF_FRAME);
		if (cmd & MIF_FRAME_TALSB)
			break;

		udelay(10);
	}
}

static inline void phy_write(struct gem *gp, int reg, u16 val)
{
	__phy_write(gp, reg, val, gp->mii_phy_addr);
}

static void gem_handle_mif_event(struct gem *gp, u32 reg_val, u32 changed_bits)
{
	if (netif_msg_intr(gp))
		printk(KERN_DEBUG "%s: mif interrupt\n", gp->dev->name);
}

static int gem_pcs_interrupt(struct net_device *dev, struct gem *gp, u32 gem_status)
{
	u32 pcs_istat = readl(gp->regs + PCS_ISTAT);
	u32 pcs_miistat;

	if (netif_msg_intr(gp))
		printk(KERN_DEBUG "%s: pcs interrupt, pcs_istat: 0x%x\n",
			gp->dev->name, pcs_istat);

	if (!(pcs_istat & PCS_ISTAT_LSC)) {
		printk(KERN_ERR "%s: PCS irq but no link status change???\n",
		       dev->name);
		return 0;
	}

	/* The link status bit latches on zero, so you must
	 * read it twice in such a case to see a transition
	 * to the link being up.
	 */
	pcs_miistat = readl(gp->regs + PCS_MIISTAT);
	if (!(pcs_miistat & PCS_MIISTAT_LS))
		pcs_miistat |=
			(readl(gp->regs + PCS_MIISTAT) &
			 PCS_MIISTAT_LS);

	if (pcs_miistat & PCS_MIISTAT_ANC) {
		/* The remote-fault indication is only valid
		 * when autoneg has completed.
		 */
		if (pcs_miistat & PCS_MIISTAT_RF)
			printk(KERN_INFO "%s: PCS AutoNEG complete, "
			       "RemoteFault\n", dev->name);
		else
			printk(KERN_INFO "%s: PCS AutoNEG complete.\n",
			       dev->name);
	}

	if (pcs_miistat & PCS_MIISTAT_LS) {
		printk(KERN_INFO "%s: PCS link is now up.\n",
		       dev->name);
	} else {
		printk(KERN_INFO "%s: PCS link is now down.\n",
		       dev->name);

		/* If this happens and the link timer is not running,
		 * reset so we re-negotiate.
		 */
		if (!timer_pending(&gp->link_timer))
			return 1;
	}

	return 0;
}

static int gem_txmac_interrupt(struct net_device *dev, struct gem *gp, u32 gem_status)
{
	u32 txmac_stat = readl(gp->regs + MAC_TXSTAT);

	if (netif_msg_intr(gp))
		printk(KERN_DEBUG "%s: txmac interrupt, txmac_stat: 0x%x\n",
			gp->dev->name, txmac_stat);

	/* Defer timer expiration is quite normal,
	 * don't even log the event.
	 */
	if ((txmac_stat & MAC_TXSTAT_DTE) &&
	    !(txmac_stat & ~MAC_TXSTAT_DTE))
		return 0;

	if (txmac_stat & MAC_TXSTAT_URUN) {
		printk(KERN_ERR "%s: TX MAC xmit underrun.\n",
		       dev->name);
		gp->net_stats.tx_fifo_errors++;
	}

	if (txmac_stat & MAC_TXSTAT_MPE) {
		printk(KERN_ERR "%s: TX MAC max packet size error.\n",
		       dev->name);
		gp->net_stats.tx_errors++;
	}

	/* The rest are all cases of one of the 16-bit TX
	 * counters expiring.
	 */
	if (txmac_stat & MAC_TXSTAT_NCE)
		gp->net_stats.collisions += 0x10000;

	if (txmac_stat & MAC_TXSTAT_ECE) {
		gp->net_stats.tx_aborted_errors += 0x10000;
		gp->net_stats.collisions += 0x10000;
	}

	if (txmac_stat & MAC_TXSTAT_LCE) {
		gp->net_stats.tx_aborted_errors += 0x10000;
		gp->net_stats.collisions += 0x10000;
	}

	/* We do not keep track of MAC_TXSTAT_FCE and
	 * MAC_TXSTAT_PCE events.
	 */
	return 0;
}

/* When we get a RX fifo overflow, the RX unit in GEM is probably hung
 * so we do the following.
 *
 * If any part of the reset goes wrong, we return 1 and that causes the
 * whole chip to be reset.
 */
static int gem_rxmac_reset(struct gem *gp)
{
	struct net_device *dev = gp->dev;
	int limit, i;
	u64 desc_dma;
	u32 val;

	/* First, reset MAC RX. */
	writel(gp->mac_rx_cfg & ~MAC_RXCFG_ENAB,
	       gp->regs + MAC_RXCFG);
	for (limit = 0; limit < 5000; limit++) {
		if (!(readl(gp->regs + MAC_RXCFG) & MAC_RXCFG_ENAB))
			break;
		udelay(10);
	}
	if (limit == 5000) {
		printk(KERN_ERR "%s: RX MAC will not disable, resetting whole "
		       "chip.\n", dev->name);
		return 1;
	}

	/* Second, disable RX DMA. */
	writel(0, gp->regs + RXDMA_CFG);
	for (limit = 0; limit < 5000; limit++) {
		if (!(readl(gp->regs + RXDMA_CFG) & RXDMA_CFG_ENABLE))
			break;
		udelay(10);
	}
	if (limit == 5000) {
		printk(KERN_ERR "%s: RX DMA will not disable, resetting whole "
		       "chip.\n", dev->name);
		return 1;
	}

	udelay(5000);

	/* Execute RX reset command. */
	writel(gp->swrst_base | GREG_SWRST_RXRST,
	       gp->regs + GREG_SWRST);
	for (limit = 0; limit < 5000; limit++) {
		if (!(readl(gp->regs + GREG_SWRST) & GREG_SWRST_RXRST))
			break;
		udelay(10);
	}
	if (limit == 5000) {
		printk(KERN_ERR "%s: RX reset command will not execute, resetting "
		       "whole chip.\n", dev->name);
		return 1;
	}

	/* Refresh the RX ring. */
	for (i = 0; i < RX_RING_SIZE; i++) {
		struct gem_rxd *rxd = &gp->init_block->rxd[i];

		if (gp->rx_skbs[i] == NULL) {
			printk(KERN_ERR "%s: Parts of RX ring empty, resetting "
			       "whole chip.\n", dev->name);
			return 1;
		}

		rxd->status_word = cpu_to_le64(RXDCTRL_FRESH(gp));
	}
	gp->rx_new = gp->rx_old = 0;

	/* Now we must reprogram the rest of RX unit. */
	desc_dma = (u64) gp->gblock_dvma;
	desc_dma += (INIT_BLOCK_TX_RING_SIZE * sizeof(struct gem_txd));
	writel(desc_dma >> 32, gp->regs + RXDMA_DBHI);
	writel(desc_dma & 0xffffffff, gp->regs + RXDMA_DBLOW);
	writel(RX_RING_SIZE - 4, gp->regs + RXDMA_KICK);
	val = (RXDMA_CFG_BASE | (RX_OFFSET << 10) |
	       ((14 / 2) << 13) | RXDMA_CFG_FTHRESH_128);
	writel(val, gp->regs + RXDMA_CFG);
	if (readl(gp->regs + GREG_BIFCFG) & GREG_BIFCFG_M66EN)
		writel(((5 & RXDMA_BLANK_IPKTS) |
			((8 << 12) & RXDMA_BLANK_ITIME)),
		       gp->regs + RXDMA_BLANK);
	else
		writel(((5 & RXDMA_BLANK_IPKTS) |
			((4 << 12) & RXDMA_BLANK_ITIME)),
		       gp->regs + RXDMA_BLANK);
	val  = (((gp->rx_pause_off / 64) << 0) & RXDMA_PTHRESH_OFF);
	val |= (((gp->rx_pause_on / 64) << 12) & RXDMA_PTHRESH_ON);
	writel(val, gp->regs + RXDMA_PTHRESH);
	val = readl(gp->regs + RXDMA_CFG);
	writel(val | RXDMA_CFG_ENABLE, gp->regs + RXDMA_CFG);
	writel(MAC_RXSTAT_RCV, gp->regs + MAC_RXMASK);
	val = readl(gp->regs + MAC_RXCFG);
	writel(val | MAC_RXCFG_ENAB, gp->regs + MAC_RXCFG);

	return 0;
}

static int gem_rxmac_interrupt(struct net_device *dev, struct gem *gp, u32 gem_status)
{
	u32 rxmac_stat = readl(gp->regs + MAC_RXSTAT);
	int ret = 0;

	if (netif_msg_intr(gp))
		printk(KERN_DEBUG "%s: rxmac interrupt, rxmac_stat: 0x%x\n",
			gp->dev->name, rxmac_stat);

	if (rxmac_stat & MAC_RXSTAT_OFLW) {
		gp->net_stats.rx_over_errors++;
		gp->net_stats.rx_fifo_errors++;

		ret = gem_rxmac_reset(gp);
	}

	if (rxmac_stat & MAC_RXSTAT_ACE)
		gp->net_stats.rx_frame_errors += 0x10000;

	if (rxmac_stat & MAC_RXSTAT_CCE)
		gp->net_stats.rx_crc_errors += 0x10000;

	if (rxmac_stat & MAC_RXSTAT_LCE)
		gp->net_stats.rx_length_errors += 0x10000;

	/* We do not track MAC_RXSTAT_FCE and MAC_RXSTAT_VCE
	 * events.
	 */
	return ret;
}

static int gem_mac_interrupt(struct net_device *dev, struct gem *gp, u32 gem_status)
{
	u32 mac_cstat = readl(gp->regs + MAC_CSTAT);

	if (netif_msg_intr(gp))
		printk(KERN_DEBUG "%s: mac interrupt, mac_cstat: 0x%x\n",
			gp->dev->name, mac_cstat);

	/* This interrupt is just for pause frame and pause
	 * tracking.  It is useful for diagnostics and debug
	 * but probably by default we will mask these events.
	 */
	if (mac_cstat & MAC_CSTAT_PS)
		gp->pause_entered++;

	if (mac_cstat & MAC_CSTAT_PRCV)
		gp->pause_last_time_recvd = (mac_cstat >> 16);

	return 0;
}

static int gem_mif_interrupt(struct net_device *dev, struct gem *gp, u32 gem_status)
{
	u32 mif_status = readl(gp->regs + MIF_STATUS);
	u32 reg_val, changed_bits;

	reg_val = (mif_status & MIF_STATUS_DATA) >> 16;
	changed_bits = (mif_status & MIF_STATUS_STAT);

	gem_handle_mif_event(gp, reg_val, changed_bits);

	return 0;
}

static int gem_pci_interrupt(struct net_device *dev, struct gem *gp, u32 gem_status)
{
	u32 pci_estat = readl(gp->regs + GREG_PCIESTAT);

	if (gp->pdev->vendor == PCI_VENDOR_ID_SUN &&
	    gp->pdev->device == PCI_DEVICE_ID_SUN_GEM) {
		printk(KERN_ERR "%s: PCI error [%04x] ",
		       dev->name, pci_estat);

		if (pci_estat & GREG_PCIESTAT_BADACK)
			printk("<No ACK64# during ABS64 cycle> ");
		if (pci_estat & GREG_PCIESTAT_DTRTO)
			printk("<Delayed transaction timeout> ");
		if (pci_estat & GREG_PCIESTAT_OTHER)
			printk("<other>");
		printk("\n");
	} else {
		pci_estat |= GREG_PCIESTAT_OTHER;
		printk(KERN_ERR "%s: PCI error\n", dev->name);
	}

	if (pci_estat & GREG_PCIESTAT_OTHER) {
		u16 pci_cfg_stat;

		/* Interrogate PCI config space for the
		 * true cause.
		 */
		pci_read_config_word(gp->pdev, PCI_STATUS,
				     &pci_cfg_stat);
		printk(KERN_ERR "%s: Read PCI cfg space status [%04x]\n",
		       dev->name, pci_cfg_stat);
		if (pci_cfg_stat & PCI_STATUS_PARITY)
			printk(KERN_ERR "%s: PCI parity error detected.\n",
			       dev->name);
		if (pci_cfg_stat & PCI_STATUS_SIG_TARGET_ABORT)
			printk(KERN_ERR "%s: PCI target abort.\n",
			       dev->name);
		if (pci_cfg_stat & PCI_STATUS_REC_TARGET_ABORT)
			printk(KERN_ERR "%s: PCI master acks target abort.\n",
			       dev->name);
		if (pci_cfg_stat & PCI_STATUS_REC_MASTER_ABORT)
			printk(KERN_ERR "%s: PCI master abort.\n",
			       dev->name);
		if (pci_cfg_stat & PCI_STATUS_SIG_SYSTEM_ERROR)
			printk(KERN_ERR "%s: PCI system error SERR#.\n",
			       dev->name);
		if (pci_cfg_stat & PCI_STATUS_DETECTED_PARITY)
			printk(KERN_ERR "%s: PCI parity error.\n",
			       dev->name);

		/* Write the error bits back to clear them. */
		pci_cfg_stat &= (PCI_STATUS_PARITY |
				 PCI_STATUS_SIG_TARGET_ABORT |
				 PCI_STATUS_REC_TARGET_ABORT |
				 PCI_STATUS_REC_MASTER_ABORT |
				 PCI_STATUS_SIG_SYSTEM_ERROR |
				 PCI_STATUS_DETECTED_PARITY);
		pci_write_config_word(gp->pdev,
				      PCI_STATUS, pci_cfg_stat);
	}

	/* For all PCI errors, we should reset the chip. */
	return 1;
}

/* All non-normal interrupt conditions get serviced here.
 * Returns non-zero if we should just exit the interrupt
 * handler right now (ie. if we reset the card which invalidates
 * all of the other original irq status bits).
 */
static int gem_abnormal_irq(struct net_device *dev, struct gem *gp, u32 gem_status)
{
	if (gem_status & GREG_STAT_RXNOBUF) {
		/* Frame arrived, no free RX buffers available. */
		if (netif_msg_rx_err(gp))
			printk(KERN_DEBUG "%s: no buffer for rx frame\n",
				gp->dev->name);
		gp->net_stats.rx_dropped++;
	}

	if (gem_status & GREG_STAT_RXTAGERR) {
		/* corrupt RX tag framing */
		if (netif_msg_rx_err(gp))
			printk(KERN_DEBUG "%s: corrupt rx tag framing\n",
				gp->dev->name);
		gp->net_stats.rx_errors++;

		goto do_reset;
	}

	if (gem_status & GREG_STAT_PCS) {
		if (gem_pcs_interrupt(dev, gp, gem_status))
			goto do_reset;
	}

	if (gem_status & GREG_STAT_TXMAC) {
		if (gem_txmac_interrupt(dev, gp, gem_status))
			goto do_reset;
	}

	if (gem_status & GREG_STAT_RXMAC) {
		if (gem_rxmac_interrupt(dev, gp, gem_status))
			goto do_reset;
	}

	if (gem_status & GREG_STAT_MAC) {
		if (gem_mac_interrupt(dev, gp, gem_status))
			goto do_reset;
	}

	if (gem_status & GREG_STAT_MIF) {
		if (gem_mif_interrupt(dev, gp, gem_status))
			goto do_reset;
	}

	if (gem_status & GREG_STAT_PCIERR) {
		if (gem_pci_interrupt(dev, gp, gem_status))
			goto do_reset;
	}

	return 0;

do_reset:
	gp->reset_task_pending = 2;
	schedule_task(&gp->reset_task);

	return 1;
}

static __inline__ void gem_tx(struct net_device *dev, struct gem *gp, u32 gem_status)
{
	int entry, limit;

	if (netif_msg_intr(gp))
		printk(KERN_DEBUG "%s: tx interrupt, gem_status: 0x%x\n",
			gp->dev->name, gem_status);

	entry = gp->tx_old;
	limit = ((gem_status & GREG_STAT_TXNR) >> GREG_STAT_TXNR_SHIFT);
	while (entry != limit) {
		struct sk_buff *skb;
		struct gem_txd *txd;
		dma_addr_t dma_addr;
		u32 dma_len;
		int frag;

		if (netif_msg_tx_done(gp))
			printk(KERN_DEBUG "%s: tx done, slot %d\n",
				gp->dev->name, entry);
		skb = gp->tx_skbs[entry];
		if (skb_shinfo(skb)->nr_frags) {
			int last = entry + skb_shinfo(skb)->nr_frags;
			int walk = entry;
			int incomplete = 0;

			last &= (TX_RING_SIZE - 1);
			for (;;) {
				walk = NEXT_TX(walk);
				if (walk == limit)
					incomplete = 1;
				if (walk == last)
					break;
			}
			if (incomplete)
				break;
		}
		gp->tx_skbs[entry] = NULL;
		gp->net_stats.tx_bytes += skb->len;

		for (frag = 0; frag <= skb_shinfo(skb)->nr_frags; frag++) {
			txd = &gp->init_block->txd[entry];

			dma_addr = le64_to_cpu(txd->buffer);
			dma_len = le64_to_cpu(txd->control_word) & TXDCTRL_BUFSZ;

			pci_unmap_page(gp->pdev, dma_addr, dma_len, PCI_DMA_TODEVICE);
			entry = NEXT_TX(entry);
		}

		gp->net_stats.tx_packets++;
		dev_kfree_skb_irq(skb);
	}
	gp->tx_old = entry;

	if (netif_queue_stopped(dev) &&
	    TX_BUFFS_AVAIL(gp) > (MAX_SKB_FRAGS + 1))
		netif_wake_queue(dev);
}

static __inline__ void gem_post_rxds(struct gem *gp, int limit)
{
	int cluster_start, curr, count, kick;

	cluster_start = curr = (gp->rx_new & ~(4 - 1));
	count = 0;
	kick = -1;
	wmb();
	while (curr != limit) {
		curr = NEXT_RX(curr);
		if (++count == 4) {
			struct gem_rxd *rxd =
				&gp->init_block->rxd[cluster_start];
			for (;;) {
				rxd->status_word = cpu_to_le64(RXDCTRL_FRESH(gp));
				rxd++;
				cluster_start = NEXT_RX(cluster_start);
				if (cluster_start == curr)
					break;
			}
			kick = curr;
			count = 0;
		}
	}
	if (kick >= 0) {
		mb();
		writel(kick, gp->regs + RXDMA_KICK);
	}
}

static void gem_rx(struct gem *gp)
{
	int entry, drops;
	u32 done;

	if (netif_msg_intr(gp))
		printk(KERN_DEBUG "%s: rx interrupt, done: %d, rx_new: %d\n",
			gp->dev->name, readl(gp->regs + RXDMA_DONE), gp->rx_new);

	entry = gp->rx_new;
	drops = 0;
	done = readl(gp->regs + RXDMA_DONE);
	for (;;) {
		struct gem_rxd *rxd = &gp->init_block->rxd[entry];
		struct sk_buff *skb;
		u64 status = cpu_to_le64(rxd->status_word);
		dma_addr_t dma_addr;
		int len;

		if ((status & RXDCTRL_OWN) != 0)
			break;

		/* When writing back RX descriptor, GEM writes status
		 * then buffer address, possibly in seperate transactions.
		 * If we don't wait for the chip to write both, we could
		 * post a new buffer to this descriptor then have GEM spam
		 * on the buffer address.  We sync on the RX completion
		 * register to prevent this from happening.
		 */
		if (entry == done) {
			done = readl(gp->regs + RXDMA_DONE);
			if (entry == done)
				break;
		}

		skb = gp->rx_skbs[entry];

		len = (status & RXDCTRL_BUFSZ) >> 16;
		if ((len < ETH_ZLEN) || (status & RXDCTRL_BAD)) {
			gp->net_stats.rx_errors++;
			if (len < ETH_ZLEN)
				gp->net_stats.rx_length_errors++;
			if (len & RXDCTRL_BAD)
				gp->net_stats.rx_crc_errors++;

			/* We'll just return it to GEM. */
		drop_it:
			gp->net_stats.rx_dropped++;
			goto next;
		}

		dma_addr = cpu_to_le64(rxd->buffer);
		if (len > RX_COPY_THRESHOLD) {
			struct sk_buff *new_skb;

			new_skb = gem_alloc_skb(RX_BUF_ALLOC_SIZE(gp), GFP_ATOMIC);
			if (new_skb == NULL) {
				drops++;
				goto drop_it;
			}
			pci_unmap_page(gp->pdev, dma_addr,
				       RX_BUF_ALLOC_SIZE(gp),
				       PCI_DMA_FROMDEVICE);
			gp->rx_skbs[entry] = new_skb;
			new_skb->dev = gp->dev;
			skb_put(new_skb, (ETH_FRAME_LEN + RX_OFFSET));
			rxd->buffer = cpu_to_le64(pci_map_page(gp->pdev,
							       virt_to_page(new_skb->data),
							       ((unsigned long) new_skb->data &
								~PAGE_MASK),
							       RX_BUF_ALLOC_SIZE(gp),
							       PCI_DMA_FROMDEVICE));
			skb_reserve(new_skb, RX_OFFSET);

			/* Trim the original skb for the netif. */
			skb_trim(skb, len);
		} else {
			struct sk_buff *copy_skb = dev_alloc_skb(len + 2);

			if (copy_skb == NULL) {
				drops++;
				goto drop_it;
			}

			copy_skb->dev = gp->dev;
			skb_reserve(copy_skb, 2);
			skb_put(copy_skb, len);
			pci_dma_sync_single(gp->pdev, dma_addr, len, PCI_DMA_FROMDEVICE);
			memcpy(copy_skb->data, skb->data, len);

			/* We'll reuse the original ring buffer. */
			skb = copy_skb;
		}

		skb->csum = ntohs((status & RXDCTRL_TCPCSUM) ^ 0xffff);
		skb->ip_summed = CHECKSUM_HW;
		skb->protocol = eth_type_trans(skb, gp->dev);
		netif_rx(skb);

		gp->net_stats.rx_packets++;
		gp->net_stats.rx_bytes += len;
		gp->dev->last_rx = jiffies;

	next:
		entry = NEXT_RX(entry);
	}

	gem_post_rxds(gp, entry);

	gp->rx_new = entry;

	if (drops)
		printk(KERN_INFO "%s: Memory squeeze, deferring packet.\n",
		       gp->dev->name);
}

static void gem_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct net_device *dev = dev_id;
	struct gem *gp = dev->priv;
	u32 gem_status = readl(gp->regs + GREG_STAT);

	spin_lock(&gp->lock);

	if (gem_status & GREG_STAT_ABNORMAL) {
		if (gem_abnormal_irq(dev, gp, gem_status))
			goto out;
	}
	if (gem_status & (GREG_STAT_TXALL | GREG_STAT_TXINTME))
		gem_tx(dev, gp, gem_status);
	if (gem_status & GREG_STAT_RXDONE)
		gem_rx(gp);

out:
	spin_unlock(&gp->lock);
}

static void gem_tx_timeout(struct net_device *dev)
{
	struct gem *gp = dev->priv;

	printk(KERN_ERR "%s: transmit timed out, resetting\n", dev->name);
	if (!gp->hw_running) {
		printk("%s: hrm.. hw not running !\n", dev->name);
		return;
	}
	printk(KERN_ERR "%s: TX_STATE[%08x:%08x:%08x]\n",
	       dev->name,
	       readl(gp->regs + TXDMA_CFG),
	       readl(gp->regs + MAC_TXSTAT),
	       readl(gp->regs + MAC_TXCFG));
	printk(KERN_ERR "%s: RX_STATE[%08x:%08x:%08x]\n",
	       dev->name,
	       readl(gp->regs + RXDMA_CFG),
	       readl(gp->regs + MAC_RXSTAT),
	       readl(gp->regs + MAC_RXCFG));

	spin_lock_irq(&gp->lock);

	gp->reset_task_pending = 2;
	schedule_task(&gp->reset_task);

	spin_unlock_irq(&gp->lock);
}

static __inline__ int gem_intme(int entry)
{
	/* Algorithm: IRQ every 1/2 of descriptors. */
	if (!(entry & ((TX_RING_SIZE>>1)-1)))
		return 1;

	return 0;
}

static int gem_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct gem *gp = dev->priv;
	int entry;
	u64 ctrl;

	ctrl = 0;
	if (skb->ip_summed == CHECKSUM_HW) {
		u64 csum_start_off, csum_stuff_off;

		csum_start_off = (u64) (skb->h.raw - skb->data);
		csum_stuff_off = (u64) ((skb->h.raw + skb->csum) - skb->data);

		ctrl = (TXDCTRL_CENAB |
			(csum_start_off << 15) |
			(csum_stuff_off << 21));
	}

	spin_lock_irq(&gp->lock);

	/* This is a hard error, log it. */
	if (TX_BUFFS_AVAIL(gp) <= (skb_shinfo(skb)->nr_frags + 1)) {
		netif_stop_queue(dev);
		spin_unlock_irq(&gp->lock);
		printk(KERN_ERR PFX "%s: BUG! Tx Ring full when queue awake!\n",
		       dev->name);
		return 1;
	}

	entry = gp->tx_new;
	gp->tx_skbs[entry] = skb;

	if (skb_shinfo(skb)->nr_frags == 0) {
		struct gem_txd *txd = &gp->init_block->txd[entry];
		dma_addr_t mapping;
		u32 len;

		len = skb->len;
		mapping = pci_map_page(gp->pdev,
				       virt_to_page(skb->data),
				       ((unsigned long) skb->data &
					~PAGE_MASK),
				       len, PCI_DMA_TODEVICE);
		ctrl |= TXDCTRL_SOF | TXDCTRL_EOF | len;
		if (gem_intme(entry))
			ctrl |= TXDCTRL_INTME;
		txd->buffer = cpu_to_le64(mapping);
		wmb();
		txd->control_word = cpu_to_le64(ctrl);
		entry = NEXT_TX(entry);
	} else {
		struct gem_txd *txd;
		u32 first_len;
		u64 intme;
		dma_addr_t first_mapping;
		int frag, first_entry = entry;

		intme = 0;
		if (gem_intme(entry))
			intme |= TXDCTRL_INTME;

		/* We must give this initial chunk to the device last.
		 * Otherwise we could race with the device.
		 */
		first_len = skb->len - skb->data_len;
		first_mapping = pci_map_page(gp->pdev, virt_to_page(skb->data),
					     ((unsigned long) skb->data & ~PAGE_MASK),
					     first_len, PCI_DMA_TODEVICE);
		entry = NEXT_TX(entry);

		for (frag = 0; frag < skb_shinfo(skb)->nr_frags; frag++) {
			skb_frag_t *this_frag = &skb_shinfo(skb)->frags[frag];
			u32 len;
			dma_addr_t mapping;
			u64 this_ctrl;

			len = this_frag->size;
			mapping = pci_map_page(gp->pdev,
					       this_frag->page,
					       this_frag->page_offset,
					       len, PCI_DMA_TODEVICE);
			this_ctrl = ctrl;
			if (frag == skb_shinfo(skb)->nr_frags - 1)
				this_ctrl |= TXDCTRL_EOF;
			
			txd = &gp->init_block->txd[entry];
			txd->buffer = cpu_to_le64(mapping);
			wmb();
			txd->control_word = cpu_to_le64(this_ctrl | len);

			if (gem_intme(entry))
				intme |= TXDCTRL_INTME;

			entry = NEXT_TX(entry);
		}
		txd = &gp->init_block->txd[first_entry];
		txd->buffer = cpu_to_le64(first_mapping);
		wmb();
		txd->control_word =
			cpu_to_le64(ctrl | TXDCTRL_SOF | intme | first_len);
	}

	gp->tx_new = entry;
	if (TX_BUFFS_AVAIL(gp) <= (MAX_SKB_FRAGS + 1))
		netif_stop_queue(dev);

	if (netif_msg_tx_queued(gp))
		printk(KERN_DEBUG "%s: tx queued, slot %d, skblen %d\n",
		       dev->name, entry, skb->len);
	mb();
	writel(gp->tx_new, gp->regs + TXDMA_KICK);
	spin_unlock_irq(&gp->lock);

	dev->trans_start = jiffies;

	return 0;
}

/* Jumbo-grams don't seem to work :-( */
#define GEM_MIN_MTU	68
#if 1
#define GEM_MAX_MTU	1500
#else
#define GEM_MAX_MTU	9000
#endif

static int gem_change_mtu(struct net_device *dev, int new_mtu)
{
	struct gem *gp = dev->priv;

	if (new_mtu < GEM_MIN_MTU || new_mtu > GEM_MAX_MTU)
		return -EINVAL;

	if (!netif_running(dev) || !netif_device_present(dev)) {
		/* We'll just catch it later when the
		 * device is up'd or resumed.
		 */
		dev->mtu = new_mtu;
		return 0;
	}

	spin_lock_irq(&gp->lock);
	dev->mtu = new_mtu;
	gp->reset_task_pending = 1;
	schedule_task(&gp->reset_task);
	spin_unlock_irq(&gp->lock);

	flush_scheduled_tasks();

	return 0;
}

#define STOP_TRIES 32

/* Must be invoked under gp->lock. */
static void gem_stop(struct gem *gp)
{
	int limit;
	u32 val;

	/* Make sure we won't get any more interrupts */
	writel(0xffffffff, gp->regs + GREG_IMASK);

	/* Reset the chip */
	writel(gp->swrst_base | GREG_SWRST_TXRST | GREG_SWRST_RXRST,
	       gp->regs + GREG_SWRST);

	limit = STOP_TRIES;

	do {
		udelay(20);
		val = readl(gp->regs + GREG_SWRST);
		if (limit-- <= 0)
			break;
	} while (val & (GREG_SWRST_TXRST | GREG_SWRST_RXRST));

	if (limit <= 0)
		printk(KERN_ERR "gem: SW reset is ghetto.\n");
}

/* Must be invoked under gp->lock. */
static void gem_start_dma(struct gem *gp)
{
	unsigned long val;
	
	/* We are ready to rock, turn everything on. */
	val = readl(gp->regs + TXDMA_CFG);
	writel(val | TXDMA_CFG_ENABLE, gp->regs + TXDMA_CFG);
	val = readl(gp->regs + RXDMA_CFG);
	writel(val | RXDMA_CFG_ENABLE, gp->regs + RXDMA_CFG);
	val = readl(gp->regs + MAC_TXCFG);
	writel(val | MAC_TXCFG_ENAB, gp->regs + MAC_TXCFG);
	val = readl(gp->regs + MAC_RXCFG);
	writel(val | MAC_RXCFG_ENAB, gp->regs + MAC_RXCFG);

	(void) readl(gp->regs + MAC_RXCFG);
	udelay(100);

	writel(GREG_STAT_TXDONE, gp->regs + GREG_IMASK);

	writel(RX_RING_SIZE - 4, gp->regs + RXDMA_KICK);

}

/* Link modes of the BCM5400 PHY */
static int phy_BCM5400_link_table[8][3] = {
	{ 0, 0, 0 },	/* No link */
	{ 0, 0, 0 },	/* 10BT Half Duplex */
	{ 1, 0, 0 },	/* 10BT Full Duplex */
	{ 0, 1, 0 },	/* 100BT Half Duplex */
	{ 0, 1, 0 },	/* 100BT Half Duplex */
	{ 1, 1, 0 },	/* 100BT Full Duplex*/
	{ 1, 0, 1 },	/* 1000BT */
	{ 1, 0, 1 },	/* 1000BT */
};

/* Must be invoked under gp->lock. */
static void gem_begin_auto_negotiation(struct gem *gp, struct ethtool_cmd *ep)
{
	u16 ctl;
	
	/* Setup link parameters */
	if (!ep)
		goto start_aneg;
	if (ep->autoneg == AUTONEG_ENABLE) {
		/* TODO: parse ep->advertising */
		gp->link_advertise |= (ADVERTISE_10HALF | ADVERTISE_10FULL);
		gp->link_advertise |= (ADVERTISE_100HALF | ADVERTISE_100FULL);
		/* Can I advertise gigabit here ? I'd need BCM PHY docs... */
		gp->link_cntl = BMCR_ANENABLE;
	} else {
		gp->link_cntl = 0;
		if (ep->speed == SPEED_100)
			gp->link_cntl |= BMCR_SPEED100;
		else if (ep->speed == SPEED_1000 && gp->gigabit_capable)
			/* Hrm... check if this is right... */
			gp->link_cntl |= BMCR_SPD2;
		if (ep->duplex == DUPLEX_FULL)
			gp->link_cntl |= BMCR_FULLDPLX;
	}

start_aneg:
	if (!gp->hw_running)
		return;

	/* Configure PHY & start aneg */
	ctl = phy_read(gp, MII_BMCR);
	ctl &= ~(BMCR_FULLDPLX|BMCR_SPEED100|BMCR_ANENABLE);
	ctl |= gp->link_cntl;
	if (ctl & BMCR_ANENABLE) {
		ctl |= BMCR_ANRESTART;
		gp->lstate = link_aneg;
	} else {
		gp->lstate = link_force_ok;
	}
	phy_write(gp, MII_BMCR, ctl);

	gp->timer_ticks = 0;
	mod_timer(&gp->link_timer, jiffies + ((12 * HZ) / 10));
}

/* Must be invoked under gp->lock. */
static void gem_read_mii_link_mode(struct gem *gp, int *fd, int *spd, int *pause)
{
	u32 val;

	*fd = 0;
	*spd = 10;
	*pause = 0;
	
	if (gp->phy_mod == phymod_bcm5400 ||
	    gp->phy_mod == phymod_bcm5401 ||
	    gp->phy_mod == phymod_bcm5411) {
		int link_mode;	

	    	val = phy_read(gp, MII_BCM5400_AUXSTATUS);
		link_mode = ((val & MII_BCM5400_AUXSTATUS_LINKMODE_MASK) >>
			     MII_BCM5400_AUXSTATUS_LINKMODE_SHIFT);
		*fd = phy_BCM5400_link_table[link_mode][0];
		*spd = phy_BCM5400_link_table[link_mode][2] ?
			1000 :
			(phy_BCM5400_link_table[link_mode][1] ? 100 : 10);
		val = phy_read(gp, MII_LPA);
		if (val & LPA_PAUSE)
			*pause = 1;
	} else {
		val = phy_read(gp, MII_LPA);

		if (val & (LPA_10FULL | LPA_100FULL))
			*fd = 1;
		if (val & (LPA_100FULL | LPA_100HALF))
			*spd = 100;

		if (gp->phy_mod == phymod_m1011) {
			val = phy_read(gp, 0x0a);
			if (val & 0xc00)
				*spd = 1000;
			if (val & 0x800)
				*fd = 1;
		}
	}
}

/* A link-up condition has occurred, initialize and enable the
 * rest of the chip.
 *
 * Must be invoked under gp->lock.
 */
static void gem_set_link_modes(struct gem *gp)
{
	u32 val;
	int full_duplex, speed, pause;

	full_duplex = 0;
	speed = 10;
	pause = 0;

	if (gp->phy_type == phy_mii_mdio0 ||
	    gp->phy_type == phy_mii_mdio1) {
		val = phy_read(gp, MII_BMCR);
		if (val & BMCR_ANENABLE)
			gem_read_mii_link_mode(gp, &full_duplex, &speed, &pause);
		else {
			if (val & BMCR_FULLDPLX)
				full_duplex = 1;
			if (val & BMCR_SPEED100)
				speed = 100;
		}
	} else {
		u32 pcs_lpa = readl(gp->regs + PCS_MIILP);

		if (pcs_lpa & PCS_MIIADV_FD)
			full_duplex = 1;
		speed = 1000;
	}

	if (netif_msg_link(gp))
		printk(KERN_INFO "%s: Link is up at %d Mbps, %s-duplex.\n",
			gp->dev->name, speed, (full_duplex ? "full" : "half"));

	val = (MAC_TXCFG_EIPG0 | MAC_TXCFG_NGU);
	if (full_duplex) {
		val |= (MAC_TXCFG_ICS | MAC_TXCFG_ICOLL);
	} else {
		/* MAC_TXCFG_NBO must be zero. */
	}	
	writel(val, gp->regs + MAC_TXCFG);

	val = (MAC_XIFCFG_OE | MAC_XIFCFG_LLED);
	if (!full_duplex &&
	    (gp->phy_type == phy_mii_mdio0 ||
	     gp->phy_type == phy_mii_mdio1)) {
		val |= MAC_XIFCFG_DISE;
	} else if (full_duplex) {
		val |= MAC_XIFCFG_FLED;
	}

	if (speed == 1000)
		val |= (MAC_XIFCFG_GMII);

	writel(val, gp->regs + MAC_XIFCFG);

	/* If gigabit and half-duplex, enable carrier extension
	 * mode.  Else, disable it.
	 */
	if (speed == 1000 && !full_duplex) {
		val = readl(gp->regs + MAC_TXCFG);
		writel(val | MAC_TXCFG_TCE, gp->regs + MAC_TXCFG);

		val = readl(gp->regs + MAC_RXCFG);
		writel(val | MAC_RXCFG_RCE, gp->regs + MAC_RXCFG);
	} else {
		val = readl(gp->regs + MAC_TXCFG);
		writel(val & ~MAC_TXCFG_TCE, gp->regs + MAC_TXCFG);

		val = readl(gp->regs + MAC_RXCFG);
		writel(val & ~MAC_RXCFG_RCE, gp->regs + MAC_RXCFG);
	}

	if (gp->phy_type == phy_serialink ||
	    gp->phy_type == phy_serdes) {
 		u32 pcs_lpa = readl(gp->regs + PCS_MIILP);

		if (pcs_lpa & (PCS_MIIADV_SP | PCS_MIIADV_AP))
			pause = 1;
	}

	if (netif_msg_link(gp)) {
		if (pause) {
			printk(KERN_INFO "%s: Pause is enabled "
			       "(rxfifo: %d off: %d on: %d)\n",
			       gp->dev->name,
			       gp->rx_fifo_sz,
			       gp->rx_pause_off,
			       gp->rx_pause_on);
		} else {
			printk(KERN_INFO "%s: Pause is disabled\n",
			       gp->dev->name);
		}
	}

	if (!full_duplex)
		writel(512, gp->regs + MAC_STIME);
	else
		writel(64, gp->regs + MAC_STIME);
	val = readl(gp->regs + MAC_MCCFG);
	if (pause)
		val |= (MAC_MCCFG_SPE | MAC_MCCFG_RPE);
	else
		val &= ~(MAC_MCCFG_SPE | MAC_MCCFG_RPE);
	writel(val, gp->regs + MAC_MCCFG);

	gem_start_dma(gp);
}

/* Must be invoked under gp->lock. */
static int gem_mdio_link_not_up(struct gem *gp)
{
	u16 val;
	
	if (gp->lstate == link_force_ret) {
		if (netif_msg_link(gp))
			printk(KERN_INFO "%s: Autoneg failed again, keeping"
				" forced mode\n", gp->dev->name);
		phy_write(gp, MII_BMCR, gp->link_fcntl);
		gp->timer_ticks = 5;
		gp->lstate = link_force_ok;
	} else if (gp->lstate == link_aneg) {
		val = phy_read(gp, MII_BMCR);

		if (netif_msg_link(gp))
			printk(KERN_INFO "%s: switching to forced 100bt\n",
				gp->dev->name);
		/* Try forced modes. */
		val &= ~(BMCR_ANRESTART | BMCR_ANENABLE);
		val &= ~(BMCR_FULLDPLX);
		val |= BMCR_SPEED100;
		phy_write(gp, MII_BMCR, val);
		gp->timer_ticks = 5;
		gp->lstate = link_force_try;
	} else {
		/* Downgrade from 100 to 10 Mbps if necessary.
		 * If already at 10Mbps, warn user about the
		 * situation every 10 ticks.
		 */
		val = phy_read(gp, MII_BMCR);
		if (val & BMCR_SPEED100) {
			val &= ~BMCR_SPEED100;
			phy_write(gp, MII_BMCR, val);
			gp->timer_ticks = 5;
			if (netif_msg_link(gp))
				printk(KERN_INFO "%s: switching to forced 10bt\n",
					gp->dev->name);
		} else
			return 1;
	}
	return 0;
}

static void gem_init_rings(struct gem *);
static void gem_init_hw(struct gem *, int);

static void gem_reset_task(void *data)
{
	struct gem *gp = (struct gem *) data;

	/* The link went down, we reset the ring, but keep
	 * DMA stopped. Todo: Use this function for reset
	 * on error as well.
	 */

	spin_lock_irq(&gp->lock);

	if (gp->hw_running && gp->opened) {
		/* Make sure we don't get interrupts or tx packets */
		netif_stop_queue(gp->dev);

		writel(0xffffffff, gp->regs + GREG_IMASK);

		/* Reset the chip & rings */
		gem_stop(gp);
		gem_init_rings(gp);

		gem_init_hw(gp,
			    (gp->reset_task_pending == 2));

		netif_wake_queue(gp->dev);
	}
	gp->reset_task_pending = 0;

	spin_unlock_irq(&gp->lock);
}

static void gem_link_timer(unsigned long data)
{
	struct gem *gp = (struct gem *) data;

	if (!gp->hw_running)
		return;

	spin_lock_irq(&gp->lock);

	/* If the link of task is still pending, we just
	 * reschedule the link timer
	 */
	if (gp->reset_task_pending)
		goto restart;
	    	
	if (gp->phy_type == phy_mii_mdio0 ||
	    gp->phy_type == phy_mii_mdio1) {
		u16 val = phy_read(gp, MII_BMSR);
		u16 cntl = phy_read(gp, MII_BMCR);
		int up;

		/* When using autoneg, we really wait for ANEGCOMPLETE or we may
		 * get a "transcient" incorrect link state
		 */
		if (cntl & BMCR_ANENABLE)
			up = (val & (BMSR_ANEGCOMPLETE | BMSR_LSTATUS)) == (BMSR_ANEGCOMPLETE | BMSR_LSTATUS);
		else
			up = (val & BMSR_LSTATUS) != 0;
		if (up) {
			/* Ok, here we got a link. If we had it due to a forced
			 * fallback, and we were configured for autoneg, we do
			 * retry a short autoneg pass. If you know your hub is
			 * broken, use ethtool ;)
			 */
			if (gp->lstate == link_force_try && (gp->link_cntl & BMCR_ANENABLE)) {
				gp->lstate = link_force_ret;
				gp->link_fcntl = phy_read(gp, MII_BMCR);
				gp->timer_ticks = 5;
				if (netif_msg_link(gp))
					printk(KERN_INFO "%s: Got link after fallback, retrying"
						" autoneg once...\n", gp->dev->name);
				phy_write(gp, MII_BMCR,
					  gp->link_fcntl | BMCR_ANENABLE | BMCR_ANRESTART);
			} else if (gp->lstate != link_up) {
				gp->lstate = link_up;
				if (gp->opened)
					gem_set_link_modes(gp);
			}
		} else {
			int restart = 0;

			/* If the link was previously up, we restart the
			 * whole process
			 */
			if (gp->lstate == link_up) {
				gp->lstate = link_down;
				if (netif_msg_link(gp))
					printk(KERN_INFO "%s: Link down\n",
						gp->dev->name);
				gp->reset_task_pending = 2;
				schedule_task(&gp->reset_task);
				restart = 1;
			} else if (++gp->timer_ticks > 10)
				restart = gem_mdio_link_not_up(gp);

			if (restart) {
				gem_begin_auto_negotiation(gp, NULL);
				goto out_unlock;
			}
		}
	} else {
		u32 val = readl(gp->regs + PCS_MIISTAT);

		if (!(val & PCS_MIISTAT_LS))
			val = readl(gp->regs + PCS_MIISTAT);

		if ((val & PCS_MIISTAT_LS) != 0) {
			gp->lstate = link_up;
			if (gp->opened)
				gem_set_link_modes(gp);
		}
	}

restart:
	mod_timer(&gp->link_timer, jiffies + ((12 * HZ) / 10));
out_unlock:
	spin_unlock_irq(&gp->lock);
}

/* Must be invoked under gp->lock. */
static void gem_clean_rings(struct gem *gp)
{
	struct gem_init_block *gb = gp->init_block;
	struct sk_buff *skb;
	int i;
	dma_addr_t dma_addr;

	for (i = 0; i < RX_RING_SIZE; i++) {
		struct gem_rxd *rxd;

		rxd = &gb->rxd[i];
		if (gp->rx_skbs[i] != NULL) {
			skb = gp->rx_skbs[i];
			dma_addr = le64_to_cpu(rxd->buffer);
			pci_unmap_page(gp->pdev, dma_addr,
				       RX_BUF_ALLOC_SIZE(gp),
				       PCI_DMA_FROMDEVICE);
			dev_kfree_skb_any(skb);
			gp->rx_skbs[i] = NULL;
		}
		rxd->status_word = 0;
		wmb();
		rxd->buffer = 0;
	}

	for (i = 0; i < TX_RING_SIZE; i++) {
		if (gp->tx_skbs[i] != NULL) {
			struct gem_txd *txd;
			int frag;

			skb = gp->tx_skbs[i];
			gp->tx_skbs[i] = NULL;

			for (frag = 0; frag <= skb_shinfo(skb)->nr_frags; frag++) {
				int ent = i & (TX_RING_SIZE - 1);

				txd = &gb->txd[ent];
				dma_addr = le64_to_cpu(txd->buffer);
				pci_unmap_page(gp->pdev, dma_addr,
					       le64_to_cpu(txd->control_word) &
					       TXDCTRL_BUFSZ, PCI_DMA_TODEVICE);

				if (frag != skb_shinfo(skb)->nr_frags)
					i++;
			}
			dev_kfree_skb_any(skb);
		}
	}
}

/* Must be invoked under gp->lock. */
static void gem_init_rings(struct gem *gp)
{
	struct gem_init_block *gb = gp->init_block;
	struct net_device *dev = gp->dev;
	int i;
	dma_addr_t dma_addr;

	gp->rx_new = gp->rx_old = gp->tx_new = gp->tx_old = 0;

	gem_clean_rings(gp);

	for (i = 0; i < RX_RING_SIZE; i++) {
		struct sk_buff *skb;
		struct gem_rxd *rxd = &gb->rxd[i];

		skb = gem_alloc_skb(RX_BUF_ALLOC_SIZE(gp), GFP_ATOMIC);
		if (!skb) {
			rxd->buffer = 0;
			rxd->status_word = 0;
			continue;
		}

		gp->rx_skbs[i] = skb;
		skb->dev = dev;
		skb_put(skb, (ETH_FRAME_LEN + RX_OFFSET));
		dma_addr = pci_map_page(gp->pdev,
					virt_to_page(skb->data),
					((unsigned long) skb->data &
					 ~PAGE_MASK),
					RX_BUF_ALLOC_SIZE(gp),
					PCI_DMA_FROMDEVICE);
		rxd->buffer = cpu_to_le64(dma_addr);
		wmb();
		rxd->status_word = cpu_to_le64(RXDCTRL_FRESH(gp));
		skb_reserve(skb, RX_OFFSET);
	}

	for (i = 0; i < TX_RING_SIZE; i++) {
		struct gem_txd *txd = &gb->txd[i];

		txd->control_word = 0;
		wmb();
		txd->buffer = 0;
	}
	wmb();
}

/* Must be invoked under gp->lock. */
static int gem_reset_one_mii_phy(struct gem *gp, int phy_addr)
{
	u16 val;
	int limit = 10000;
	
	val = __phy_read(gp, MII_BMCR, phy_addr);
	val &= ~BMCR_ISOLATE;
	val |= BMCR_RESET;
	__phy_write(gp, MII_BMCR, val, phy_addr);

	udelay(100);

	while (limit--) {
		val = __phy_read(gp, MII_BMCR, phy_addr);
		if ((val & BMCR_RESET) == 0)
			break;
		udelay(10);
	}
	if ((val & BMCR_ISOLATE) && limit > 0)
		__phy_write(gp, MII_BMCR, val & ~BMCR_ISOLATE, phy_addr);
	
	return (limit <= 0);
}

/* Must be invoked under gp->lock. */
static void gem_init_bcm5201_phy(struct gem *gp)
{
	u16 data;

	data = phy_read(gp, MII_BCM5201_MULTIPHY);
	data &= ~MII_BCM5201_MULTIPHY_SUPERISOLATE;
	phy_write(gp, MII_BCM5201_MULTIPHY, data);
}

/* Must be invoked under gp->lock. */
static void gem_init_bcm5400_phy(struct gem *gp)
{
	u16 data;

	/* Configure for gigabit full duplex */
	data = phy_read(gp, MII_BCM5400_AUXCONTROL);
	data |= MII_BCM5400_AUXCONTROL_PWR10BASET;
	phy_write(gp, MII_BCM5400_AUXCONTROL, data);
	
	data = phy_read(gp, MII_BCM5400_GB_CONTROL);
	data |= MII_BCM5400_GB_CONTROL_FULLDUPLEXCAP;
	phy_write(gp, MII_BCM5400_GB_CONTROL, data);
	
	mdelay(10);

	/* Reset and configure cascaded 10/100 PHY */
	gem_reset_one_mii_phy(gp, 0x1f);
	
	data = __phy_read(gp, MII_BCM5201_MULTIPHY, 0x1f);
	data |= MII_BCM5201_MULTIPHY_SERIALMODE;
	__phy_write(gp, MII_BCM5201_MULTIPHY, data, 0x1f);

	data = phy_read(gp, MII_BCM5400_AUXCONTROL);
	data &= ~MII_BCM5400_AUXCONTROL_PWR10BASET;
	phy_write(gp, MII_BCM5400_AUXCONTROL, data);
}

/* Must be invoked under gp->lock. */
static void gem_init_bcm5401_phy(struct gem *gp)
{
	u16 data;
	int rev;

	rev = phy_read(gp, MII_PHYSID2) & 0x000f;
	if (rev == 0 || rev == 3) {
		/* Some revisions of 5401 appear to need this
		 * initialisation sequence to disable, according
		 * to OF, "tap power management"
		 * 
		 * WARNING ! OF and Darwin don't agree on the
		 * register addresses. OF seem to interpret the
		 * register numbers below as decimal
		 *
		 * Note: This should (and does) match tg3_init_5401phy_dsp
		 *       in the tg3.c driver. -DaveM
		 */
		phy_write(gp, 0x18, 0x0c20);
		phy_write(gp, 0x17, 0x0012);
		phy_write(gp, 0x15, 0x1804);
		phy_write(gp, 0x17, 0x0013);
		phy_write(gp, 0x15, 0x1204);
		phy_write(gp, 0x17, 0x8006);
		phy_write(gp, 0x15, 0x0132);
		phy_write(gp, 0x17, 0x8006);
		phy_write(gp, 0x15, 0x0232);
		phy_write(gp, 0x17, 0x201f);
		phy_write(gp, 0x15, 0x0a20);
	}
	
	/* Configure for gigabit full duplex */
	data = phy_read(gp, MII_BCM5400_GB_CONTROL);
	data |= MII_BCM5400_GB_CONTROL_FULLDUPLEXCAP;
	phy_write(gp, MII_BCM5400_GB_CONTROL, data);

	mdelay(1);

	/* Reset and configure cascaded 10/100 PHY */
	gem_reset_one_mii_phy(gp, 0x1f);
	
	data = __phy_read(gp, MII_BCM5201_MULTIPHY, 0x1f);
	data |= MII_BCM5201_MULTIPHY_SERIALMODE;
	__phy_write(gp, MII_BCM5201_MULTIPHY, data, 0x1f);
}

/* Must be invoked under gp->lock. */
static void gem_init_bcm5411_phy(struct gem *gp)
{
	u16 data;

	/* Here's some more Apple black magic to setup
	 * some voltage stuffs.
	 */
	phy_write(gp, 0x1c, 0x8c23);
	phy_write(gp, 0x1c, 0x8ca3);
	phy_write(gp, 0x1c, 0x8c23);

	/* Here, Apple seems to want to reset it, do
	 * it as well
	 */
	phy_write(gp, MII_BMCR, BMCR_RESET);

	/* Start autoneg */
	phy_write(gp, MII_BMCR,
		  (BMCR_ANENABLE | BMCR_FULLDPLX |
		   BMCR_ANRESTART | BMCR_SPD2));

	data = phy_read(gp, MII_BCM5400_GB_CONTROL);
	data |= MII_BCM5400_GB_CONTROL_FULLDUPLEXCAP;
	phy_write(gp, MII_BCM5400_GB_CONTROL, data);
}

/* Must be invoked under gp->lock. */
static void gem_init_phy(struct gem *gp)
{
	u32 mifcfg;

	if (!gp->wake_on_lan && gp->phy_mod == phymod_bcm5201)
		phy_write(gp, MII_BCM5201_INTERRUPT, 0);

	/* Revert MIF CFG setting done on stop_phy */
	mifcfg = readl(gp->regs + MIF_CFG);
	mifcfg &= ~MIF_CFG_BBMODE;
	writel(mifcfg, gp->regs + MIF_CFG);
	
#ifdef CONFIG_ALL_PPC
	if (gp->pdev->vendor == PCI_VENDOR_ID_APPLE) {
		int i;

		pmac_call_feature(PMAC_FTR_GMAC_PHY_RESET, gp->of_node, 0, 0);
		for (i = 0; i < 32; i++) {
			gp->mii_phy_addr = i;
			if (phy_read(gp, MII_BMCR) != 0xffff)
				break;
		}
		if (i == 32) {
			printk(KERN_WARNING "%s: GMAC PHY not responding !\n",
			       gp->dev->name);
			return;
		}
	}
#endif /* CONFIG_ALL_PPC */

	if (gp->pdev->vendor == PCI_VENDOR_ID_SUN &&
	    gp->pdev->device == PCI_DEVICE_ID_SUN_GEM) {
		u32 val;

		/* Init datapath mode register. */
		if (gp->phy_type == phy_mii_mdio0 ||
		    gp->phy_type == phy_mii_mdio1) {
			val = PCS_DMODE_MGM;
		} else if (gp->phy_type == phy_serialink) {
			val = PCS_DMODE_SM | PCS_DMODE_GMOE;
		} else {
			val = PCS_DMODE_ESM;
		}

		writel(val, gp->regs + PCS_DMODE);
	}

	if (gp->phy_type == phy_mii_mdio0 ||
	    gp->phy_type == phy_mii_mdio1) {
		u32 phy_id;
		u16 val;
	
		/* Take PHY out of isloate mode and reset it. */
		gem_reset_one_mii_phy(gp, gp->mii_phy_addr);

		phy_id = (phy_read(gp, MII_PHYSID1) << 16 | phy_read(gp, MII_PHYSID2))
			 	& 0xfffffff0;
		printk(KERN_INFO "%s: MII PHY ID: %x ", gp->dev->name, phy_id);
		switch(phy_id) {
		case 0x406210:
			gp->phy_mod = phymod_bcm5201;
			gem_init_bcm5201_phy(gp);
			printk("BCM 5201\n");
			break;

		case 0x4061e0:
			printk("BCM 5221\n");
			gp->phy_mod = phymod_bcm5221;
			break;

		case 0x206040:
			printk("BCM 5400\n");
			gp->phy_mod = phymod_bcm5400;
			gem_init_bcm5400_phy(gp);
			gp->gigabit_capable = 1;
			break;

		case 0x206050:
			printk("BCM 5401\n");
			gp->phy_mod = phymod_bcm5401;
			gem_init_bcm5401_phy(gp);
			gp->gigabit_capable = 1;
			break;

		case 0x206070:
			printk("BCM 5411\n");
			gp->phy_mod = phymod_bcm5411;
			gem_init_bcm5411_phy(gp);
			gp->gigabit_capable = 1;
			break;
		case 0x1410c60:
			printk("M1011 (Marvel ?)\n");
			gp->phy_mod = phymod_m1011;
			gp->gigabit_capable = 1;
			break;

		case 0x18074c0:
			printk("Lucent\n");
			gp->phy_mod = phymod_generic;
			break;

		case 0x437420:
			printk("Enable Semiconductor\n");
			gp->phy_mod = phymod_generic;
			break;

		default:
			printk("Unknown (Using generic mode)\n");
			gp->phy_mod = phymod_generic;
			break;
		};

		/* Init advertisement and enable autonegotiation. */
		val = phy_read(gp, MII_BMCR);
		val &= ~BMCR_ANENABLE;
		phy_write(gp, MII_BMCR, val);
		udelay(10);
		
		phy_write(gp, MII_ADVERTISE,
			  phy_read(gp, MII_ADVERTISE) |
			  (ADVERTISE_10HALF | ADVERTISE_10FULL |
			   ADVERTISE_100HALF | ADVERTISE_100FULL));
	} else {
		u32 val;
		int limit;

		/* Reset PCS unit. */
		val = readl(gp->regs + PCS_MIICTRL);
		val |= PCS_MIICTRL_RST;
		writeb(val, gp->regs + PCS_MIICTRL);

		limit = 32;
		while (readl(gp->regs + PCS_MIICTRL) & PCS_MIICTRL_RST) {
			udelay(100);
			if (limit-- <= 0)
				break;
		}
		if (limit <= 0)
			printk(KERN_WARNING "%s: PCS reset bit would not clear.\n",
			       gp->dev->name);

		/* Make sure PCS is disabled while changing advertisement
		 * configuration.
		 */
		val = readl(gp->regs + PCS_CFG);
		val &= ~(PCS_CFG_ENABLE | PCS_CFG_TO);
		writel(val, gp->regs + PCS_CFG);

		/* Advertise all capabilities except assymetric
		 * pause.
		 */
		val = readl(gp->regs + PCS_MIIADV);
		val |= (PCS_MIIADV_FD | PCS_MIIADV_HD |
			PCS_MIIADV_SP | PCS_MIIADV_AP);
		writel(val, gp->regs + PCS_MIIADV);

		/* Enable and restart auto-negotiation, disable wrapback/loopback,
		 * and re-enable PCS.
		 */
		val = readl(gp->regs + PCS_MIICTRL);
		val |= (PCS_MIICTRL_RAN | PCS_MIICTRL_ANE);
		val &= ~PCS_MIICTRL_WB;
		writel(val, gp->regs + PCS_MIICTRL);

		val = readl(gp->regs + PCS_CFG);
		val |= PCS_CFG_ENABLE;
		writel(val, gp->regs + PCS_CFG);

		/* Make sure serialink loopback is off.  The meaning
		 * of this bit is logically inverted based upon whether
		 * you are in Serialink or SERDES mode.
		 */
		val = readl(gp->regs + PCS_SCTRL);
		if (gp->phy_type == phy_serialink)
			val &= ~PCS_SCTRL_LOOP;
		else
			val |= PCS_SCTRL_LOOP;
		writel(val, gp->regs + PCS_SCTRL);
		gp->gigabit_capable = 1;
	}

	/* BMCR_SPD2 is a broadcom 54xx specific thing afaik */
	if (gp->phy_mod != phymod_bcm5400 && gp->phy_mod != phymod_bcm5401 &&
	    gp->phy_mod != phymod_bcm5411)
	    	gp->link_cntl &= ~BMCR_SPD2;
}

/* Must be invoked under gp->lock. */
static void gem_init_dma(struct gem *gp)
{
	u64 desc_dma = (u64) gp->gblock_dvma;
	u32 val;

	val = (TXDMA_CFG_BASE | (0x7ff << 10) | TXDMA_CFG_PMODE);
	writel(val, gp->regs + TXDMA_CFG);

	writel(desc_dma >> 32, gp->regs + TXDMA_DBHI);
	writel(desc_dma & 0xffffffff, gp->regs + TXDMA_DBLOW);
	desc_dma += (INIT_BLOCK_TX_RING_SIZE * sizeof(struct gem_txd));

	writel(0, gp->regs + TXDMA_KICK);

	val = (RXDMA_CFG_BASE | (RX_OFFSET << 10) |
	       ((14 / 2) << 13) | RXDMA_CFG_FTHRESH_128);
	writel(val, gp->regs + RXDMA_CFG);

	writel(desc_dma >> 32, gp->regs + RXDMA_DBHI);
	writel(desc_dma & 0xffffffff, gp->regs + RXDMA_DBLOW);

	writel(RX_RING_SIZE - 4, gp->regs + RXDMA_KICK);

	val  = (((gp->rx_pause_off / 64) << 0) & RXDMA_PTHRESH_OFF);
	val |= (((gp->rx_pause_on / 64) << 12) & RXDMA_PTHRESH_ON);
	writel(val, gp->regs + RXDMA_PTHRESH);

	if (readl(gp->regs + GREG_BIFCFG) & GREG_BIFCFG_M66EN)
		writel(((5 & RXDMA_BLANK_IPKTS) |
			((8 << 12) & RXDMA_BLANK_ITIME)),
		       gp->regs + RXDMA_BLANK);
	else
		writel(((5 & RXDMA_BLANK_IPKTS) |
			((4 << 12) & RXDMA_BLANK_ITIME)),
		       gp->regs + RXDMA_BLANK);
}

/* Must be invoked under gp->lock. */
static u32
gem_setup_multicast(struct gem *gp)
{
	u32 rxcfg = 0;
	int i;
	
	if ((gp->dev->flags & IFF_ALLMULTI) ||
	    (gp->dev->mc_count > 256)) {
	    	for (i=0; i<16; i++)
			writel(0xffff, gp->regs + MAC_HASH0 + (i << 2));
		rxcfg |= MAC_RXCFG_HFE;
	} else if (gp->dev->flags & IFF_PROMISC) {
		rxcfg |= MAC_RXCFG_PROM;
	} else {
		u16 hash_table[16];
		u32 crc;
		struct dev_mc_list *dmi = gp->dev->mc_list;
		int i;

		for (i = 0; i < 16; i++)
			hash_table[i] = 0;

		for (i = 0; i < gp->dev->mc_count; i++) {
			char *addrs = dmi->dmi_addr;

			dmi = dmi->next;

			if (!(*addrs & 1))
				continue;

 			crc = ether_crc_le(6, addrs);
			crc >>= 24;
			hash_table[crc >> 4] |= 1 << (15 - (crc & 0xf));
		}
	    	for (i=0; i<16; i++)
			writel(hash_table[i], gp->regs + MAC_HASH0 + (i << 2));
		rxcfg |= MAC_RXCFG_HFE;
	}

	return rxcfg;
}

/* Must be invoked under gp->lock. */
static void gem_init_mac(struct gem *gp)
{
	unsigned char *e = &gp->dev->dev_addr[0];

	if (gp->pdev->vendor == PCI_VENDOR_ID_SUN &&
	    gp->pdev->device == PCI_DEVICE_ID_SUN_GEM)
		writel(0x1bf0, gp->regs + MAC_SNDPAUSE);

	writel(0x00, gp->regs + MAC_IPG0);
	writel(0x08, gp->regs + MAC_IPG1);
	writel(0x04, gp->regs + MAC_IPG2);
	writel(0x40, gp->regs + MAC_STIME);
	writel(0x40, gp->regs + MAC_MINFSZ);

	/* Ethernet payload + header + FCS + optional VLAN tag. */
	writel(0x20000000 | (gp->dev->mtu + ETH_HLEN + 4 + 4), gp->regs + MAC_MAXFSZ);

	writel(0x07, gp->regs + MAC_PASIZE);
	writel(0x04, gp->regs + MAC_JAMSIZE);
	writel(0x10, gp->regs + MAC_ATTLIM);
	writel(0x8808, gp->regs + MAC_MCTYPE);

	writel((e[5] | (e[4] << 8)) & 0x3ff, gp->regs + MAC_RANDSEED);

	writel((e[4] << 8) | e[5], gp->regs + MAC_ADDR0);
	writel((e[2] << 8) | e[3], gp->regs + MAC_ADDR1);
	writel((e[0] << 8) | e[1], gp->regs + MAC_ADDR2);

	writel(0, gp->regs + MAC_ADDR3);
	writel(0, gp->regs + MAC_ADDR4);
	writel(0, gp->regs + MAC_ADDR5);

	writel(0x0001, gp->regs + MAC_ADDR6);
	writel(0xc200, gp->regs + MAC_ADDR7);
	writel(0x0180, gp->regs + MAC_ADDR8);

	writel(0, gp->regs + MAC_AFILT0);
	writel(0, gp->regs + MAC_AFILT1);
	writel(0, gp->regs + MAC_AFILT2);
	writel(0, gp->regs + MAC_AF21MSK);
	writel(0, gp->regs + MAC_AF0MSK);

	gp->mac_rx_cfg = gem_setup_multicast(gp);

	writel(0, gp->regs + MAC_NCOLL);
	writel(0, gp->regs + MAC_FASUCC);
	writel(0, gp->regs + MAC_ECOLL);
	writel(0, gp->regs + MAC_LCOLL);
	writel(0, gp->regs + MAC_DTIMER);
	writel(0, gp->regs + MAC_PATMPS);
	writel(0, gp->regs + MAC_RFCTR);
	writel(0, gp->regs + MAC_LERR);
	writel(0, gp->regs + MAC_AERR);
	writel(0, gp->regs + MAC_FCSERR);
	writel(0, gp->regs + MAC_RXCVERR);

	/* Clear RX/TX/MAC/XIF config, we will set these up and enable
	 * them once a link is established.
	 */
	writel(0, gp->regs + MAC_TXCFG);
	writel(gp->mac_rx_cfg, gp->regs + MAC_RXCFG);
	writel(0, gp->regs + MAC_MCCFG);
	writel(0, gp->regs + MAC_XIFCFG);

	/* Setup MAC interrupts.  We want to get all of the interesting
	 * counter expiration events, but we do not want to hear about
	 * normal rx/tx as the DMA engine tells us that.
	 */
	writel(MAC_TXSTAT_XMIT, gp->regs + MAC_TXMASK);
	writel(MAC_RXSTAT_RCV, gp->regs + MAC_RXMASK);

	/* Don't enable even the PAUSE interrupts for now, we
	 * make no use of those events other than to record them.
	 */
	writel(0xffffffff, gp->regs + MAC_MCMASK);
}

/* Must be invoked under gp->lock. */
static void gem_init_pause_thresholds(struct gem *gp)
{
	/* Calculate pause thresholds.  Setting the OFF threshold to the
	 * full RX fifo size effectively disables PAUSE generation which
	 * is what we do for 10/100 only GEMs which have FIFOs too small
	 * to make real gains from PAUSE.
	 */
	if (gp->rx_fifo_sz <= (2 * 1024)) {
		gp->rx_pause_off = gp->rx_pause_on = gp->rx_fifo_sz;
	} else {
		int max_frame = (gp->dev->mtu + ETH_HLEN + 4 + 4 + 64) & ~63;
		int off = (gp->rx_fifo_sz - (max_frame * 2));
		int on = off - max_frame;

		gp->rx_pause_off = off;
		gp->rx_pause_on = on;
	}

	{
		u32 cfg;

		cfg  = 0;
#if !defined(CONFIG_SPARC64) && !defined(CONFIG_ALPHA)
		cfg |= GREG_CFG_IBURST;
#endif
		cfg |= ((31 << 1) & GREG_CFG_TXDMALIM);
		cfg |= ((31 << 6) & GREG_CFG_RXDMALIM);
		writel(cfg, gp->regs + GREG_CFG);
	}
}

static int gem_check_invariants(struct gem *gp)
{
	struct pci_dev *pdev = gp->pdev;
	u32 mif_cfg;

	/* On Apple's sungem, we can't rely on registers as the chip
	 * was been powered down by the firmware. The PHY is looked
	 * up later on.
	 */
	if (pdev->vendor == PCI_VENDOR_ID_APPLE) {
		gp->phy_type = phy_mii_mdio0;
		gp->tx_fifo_sz = readl(gp->regs + TXDMA_FSZ) * 64;
		gp->rx_fifo_sz = readl(gp->regs + RXDMA_FSZ) * 64;
		gp->swrst_base = 0;
		return 0;
	}

	mif_cfg = readl(gp->regs + MIF_CFG);

	if (pdev->vendor == PCI_VENDOR_ID_SUN &&
	    pdev->device == PCI_DEVICE_ID_SUN_RIO_GEM) {
		/* One of the MII PHYs _must_ be present
		 * as this chip has no gigabit PHY.
		 */
		if ((mif_cfg & (MIF_CFG_MDI0 | MIF_CFG_MDI1)) == 0) {
			printk(KERN_ERR PFX "RIO GEM lacks MII phy, mif_cfg[%08x]\n",
			       mif_cfg);
			return -1;
		}
	}

	/* Determine initial PHY interface type guess.  MDIO1 is the
	 * external PHY and thus takes precedence over MDIO0.
	 */
	
	if (mif_cfg & MIF_CFG_MDI1) {
		gp->phy_type = phy_mii_mdio1;
		mif_cfg |= MIF_CFG_PSELECT;
		writel(mif_cfg, gp->regs + MIF_CFG);
	} else if (mif_cfg & MIF_CFG_MDI0) {
		gp->phy_type = phy_mii_mdio0;
		mif_cfg &= ~MIF_CFG_PSELECT;
		writel(mif_cfg, gp->regs + MIF_CFG);
	} else {
		gp->phy_type = phy_serialink;
	}
	if (gp->phy_type == phy_mii_mdio1 ||
	    gp->phy_type == phy_mii_mdio0) {
		int i;

		for (i = 0; i < 32; i++) {
			gp->mii_phy_addr = i;
			if (phy_read(gp, MII_BMCR) != 0xffff)
				break;
		}
		if (i == 32) {
			if (pdev->device != PCI_DEVICE_ID_SUN_GEM) {
				printk(KERN_ERR PFX "RIO MII phy will not respond.\n");
				return -1;
			}
			gp->phy_type = phy_serdes;
		}
	}

	/* Fetch the FIFO configurations now too. */
	gp->tx_fifo_sz = readl(gp->regs + TXDMA_FSZ) * 64;
	gp->rx_fifo_sz = readl(gp->regs + RXDMA_FSZ) * 64;

	if (pdev->vendor == PCI_VENDOR_ID_SUN) {
		if (pdev->device == PCI_DEVICE_ID_SUN_GEM) {
			if (gp->tx_fifo_sz != (9 * 1024) ||
			    gp->rx_fifo_sz != (20 * 1024)) {
				printk(KERN_ERR PFX "GEM has bogus fifo sizes tx(%d) rx(%d)\n",
				       gp->tx_fifo_sz, gp->rx_fifo_sz);
				return -1;
			}
			gp->swrst_base = 0;
		} else {
			if (gp->tx_fifo_sz != (2 * 1024) ||
			    gp->rx_fifo_sz != (2 * 1024)) {
				printk(KERN_ERR PFX "RIO GEM has bogus fifo sizes tx(%d) rx(%d)\n",
				       gp->tx_fifo_sz, gp->rx_fifo_sz);
				return -1;
			}
			gp->swrst_base = (64 / 4) << GREG_SWRST_CACHE_SHIFT;
		}
	}

	return 0;
}

/* Must be invoked under gp->lock. */
static void gem_init_hw(struct gem *gp, int restart_link)
{
	/* On Apple's gmac, I initialize the PHY only after
	 * setting up the chip. It appears the gigabit PHYs
	 * don't quite like beeing talked to on the GII when
	 * the chip is not running, I suspect it might not
	 * be clocked at that point. --BenH
	 */
	if (restart_link)
		gem_init_phy(gp);
	gem_init_pause_thresholds(gp);
	gem_init_dma(gp);
	gem_init_mac(gp);

	if (restart_link) {
		/* Default aneg parameters */
		gp->timer_ticks = 0;
		gp->lstate = link_down;

		/* Can I advertise gigabit here ? I'd need BCM PHY docs... */
		gem_begin_auto_negotiation(gp, NULL);
	} else {
		if (gp->lstate == link_up)
			gem_set_link_modes(gp);
	}
}

#ifdef CONFIG_ALL_PPC
/* Enable the chip's clock and make sure it's config space is
 * setup properly. There appear to be no need to restore the
 * base addresses.
 */
static void gem_apple_powerup(struct gem *gp)
{
	u16 cmd;
	u32 mif_cfg;

	pmac_call_feature(PMAC_FTR_GMAC_ENABLE, gp->of_node, 0, 1);

	current->state = TASK_UNINTERRUPTIBLE;
	schedule_timeout((21 * HZ) / 1000);

	pci_read_config_word(gp->pdev, PCI_COMMAND, &cmd);
	cmd |= PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER | PCI_COMMAND_INVALIDATE;
    	pci_write_config_word(gp->pdev, PCI_COMMAND, cmd);
    	pci_write_config_byte(gp->pdev, PCI_LATENCY_TIMER, 6);
    	pci_write_config_byte(gp->pdev, PCI_CACHE_LINE_SIZE, 8);

	mdelay(1);
	
	mif_cfg = readl(gp->regs + MIF_CFG);
	mif_cfg &= ~(MIF_CFG_PSELECT|MIF_CFG_POLL|MIF_CFG_BBMODE|MIF_CFG_MDI1);
	mif_cfg |= MIF_CFG_MDI0;
	writel(mif_cfg, gp->regs + MIF_CFG);
	writel(PCS_DMODE_MGM, gp->regs + PCS_DMODE);
	writel(MAC_XIFCFG_OE, gp->regs + MAC_XIFCFG);

	mdelay(1);
}

/* Turn off the chip's clock */
static void gem_apple_powerdown(struct gem *gp)
{
	pmac_call_feature(PMAC_FTR_GMAC_ENABLE, gp->of_node, 0, 0);
}

#endif /* CONFIG_ALL_PPC */

/* Must be invoked under gp->lock. */
static void gem_stop_phy(struct gem *gp)
{
	u32 mifcfg;

	if (!gp->wake_on_lan && gp->phy_mod == phymod_bcm5201)
		phy_write(gp, MII_BCM5201_INTERRUPT, 0);

	/* Make sure we aren't polling PHY status change. We
	 * don't currently use that feature though
	 */
	mifcfg = readl(gp->regs + MIF_CFG);
	mifcfg &= ~MIF_CFG_POLL;
	writel(mifcfg, gp->regs + MIF_CFG);

	/* Here's a strange hack used by both MacOS 9 and X */
	phy_write(gp, MII_LPA, phy_read(gp, MII_LPA));

	if (gp->wake_on_lan) {
		/* Setup wake-on-lan */
	} else
		writel(0, gp->regs + MAC_RXCFG);
	writel(0, gp->regs + MAC_TXCFG);
	writel(0, gp->regs + MAC_XIFCFG);
	writel(0, gp->regs + TXDMA_CFG);
	writel(0, gp->regs + RXDMA_CFG);

	if (!gp->wake_on_lan) {
		gem_stop(gp);
		writel(MAC_TXRST_CMD, gp->regs + MAC_TXRST);
		writel(MAC_RXRST_CMD, gp->regs + MAC_RXRST);
		if (gp->phy_mod == phymod_bcm5400 || gp->phy_mod == phymod_bcm5401 ||
		    gp->phy_mod == phymod_bcm5411) {
#if 0 /* Commented out in Darwin... someone has those dawn docs ? */
			phy_write(gp, MII_BMCR, BMCR_PDOWN);
#endif
		} else if (gp->phy_mod == phymod_bcm5201 || gp->phy_mod == phymod_bcm5221) {
#if 0 /* Commented out in Darwin... someone has those dawn docs ? */
			u16 val = phy_read(gp, MII_BCM5201_AUXMODE2)
			phy_write(gp, MII_BCM5201_AUXMODE2,
				  val & ~MII_BCM5201_AUXMODE2_LOWPOWER);
#endif				
			phy_write(gp, MII_BCM5201_MULTIPHY, MII_BCM5201_MULTIPHY_SUPERISOLATE);
		} else if (gp->phy_mod == phymod_m1011)
			phy_write(gp, MII_BMCR, BMCR_PDOWN);

		/* According to Apple, we must set the MDIO pins to this begnign
		 * state or we may 1) eat more current, 2) damage some PHYs
		 */
		writel(mifcfg | MIF_CFG_BBMODE, gp->regs + MIF_CFG);
		writel(0, gp->regs + MIF_BBCLK);
		writel(0, gp->regs + MIF_BBDATA);
		writel(0, gp->regs + MIF_BBOENAB);
		writel(MAC_XIFCFG_GMII | MAC_XIFCFG_LBCK, gp->regs + MAC_XIFCFG);
		(void) readl(gp->regs + MAC_XIFCFG);
	}
}

/* Shut down the chip, must be called with pm_sem held.  */
static void gem_shutdown(struct gem *gp)
{
	/* Make us not-running to avoid timers respawning */
	gp->hw_running = 0;

	/* Stop the link timer */
	del_timer_sync(&gp->link_timer);

	/* Stop the reset task */
	while (gp->reset_task_pending)
		schedule();
	
	/* Actually stop the chip */
	spin_lock_irq(&gp->lock);
	if (gp->pdev->vendor == PCI_VENDOR_ID_APPLE) {
		gem_stop_phy(gp);

		spin_unlock_irq(&gp->lock);

#ifdef CONFIG_ALL_PPC
		/* Power down the chip */
		gem_apple_powerdown(gp);
#endif /* CONFIG_ALL_PPC */
	} else {
		gem_stop(gp);

		spin_unlock_irq(&gp->lock);
	}
}

static void gem_pm_task(void *data)
{
	struct gem *gp = (struct gem *) data;

	/* We assume if we can't lock the pm_sem, then open() was
	 * called again (or suspend()), and we can safely ignore
	 * the PM request
	 */
	if (down_trylock(&gp->pm_sem))
		return;

	/* Driver was re-opened or already shut down */
	if (gp->opened || !gp->hw_running) {
		up(&gp->pm_sem);
		return;
	}

	gem_shutdown(gp);

	up(&gp->pm_sem);
}

static void gem_pm_timer(unsigned long data)
{
	struct gem *gp = (struct gem *) data;

	schedule_task(&gp->pm_task);
}

static int gem_open(struct net_device *dev)
{
	struct gem *gp = dev->priv;
	int hw_was_up;

	down(&gp->pm_sem);

	hw_was_up = gp->hw_running;

	/* Stop the PM timer/task */
	del_timer(&gp->pm_timer);
	flush_scheduled_tasks();

	/* The power-management semaphore protects the hw_running
	 * etc. state so it is safe to do this bit without gp->lock
	 */
	if (!gp->hw_running) {
#ifdef CONFIG_ALL_PPC
		/* First, we need to bring up the chip */
		if (gp->pdev->vendor == PCI_VENDOR_ID_APPLE) {
			gem_apple_powerup(gp);
			gem_check_invariants(gp);
		}
#endif /* CONFIG_ALL_PPC */

		/* Reset the chip */
		spin_lock_irq(&gp->lock);
		gem_stop(gp);
		spin_unlock_irq(&gp->lock);

		gp->hw_running = 1;
	}

	/* We can now request the interrupt as we know it's masked
	 * on the controller
	 */
	if (request_irq(gp->pdev->irq, gem_interrupt,
			SA_SHIRQ, dev->name, (void *)dev)) {
		printk(KERN_ERR "%s: failed to request irq !\n", gp->dev->name);

		spin_lock_irq(&gp->lock);
#ifdef CONFIG_ALL_PPC
		if (!hw_was_up && gp->pdev->vendor == PCI_VENDOR_ID_APPLE)
			gem_apple_powerdown(gp);
#endif /* CONFIG_ALL_PPC */
		/* Fire the PM timer that will shut us down in about 10 seconds */
		gp->pm_timer.expires = jiffies + 10*HZ;
		add_timer(&gp->pm_timer);
		up(&gp->pm_sem);
		spin_unlock_irq(&gp->lock);

		return -EAGAIN;
	}

       	spin_lock_irq(&gp->lock);

	/* Allocate & setup ring buffers */
	gem_init_rings(gp);

	/* Init & setup chip hardware */
	gem_init_hw(gp, !hw_was_up);

	gp->opened = 1;

	spin_unlock_irq(&gp->lock);

	up(&gp->pm_sem);

	return 0;
}

static int gem_close(struct net_device *dev)
{
	struct gem *gp = dev->priv;

	/* Make sure we don't get distracted by suspend/resume */
	down(&gp->pm_sem);

	/* Stop traffic, mark us closed */
	spin_lock_irq(&gp->lock);

	gp->opened = 0;	
	writel(0xffffffff, gp->regs + GREG_IMASK);
	netif_stop_queue(dev);

	/* Stop chip */
	gem_stop(gp);

	/* Get rid of rings */
	gem_clean_rings(gp);

	/* Bye, the pm timer will finish the job */
	free_irq(gp->pdev->irq, (void *) dev);

	spin_unlock_irq(&gp->lock);

	/* Fire the PM timer that will shut us down in about 10 seconds */
	gp->pm_timer.expires = jiffies + 10*HZ;
	add_timer(&gp->pm_timer);

	up(&gp->pm_sem);
	
	return 0;
}

#ifdef CONFIG_PM
static int gem_suspend(struct pci_dev *pdev, u32 state)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	struct gem *gp = dev->priv;

	/* We hold the PM semaphore during entire driver
	 * sleep time
	 */
	down(&gp->pm_sem);

	printk(KERN_INFO "%s: suspending, WakeOnLan %s\n",
	       dev->name, gp->wake_on_lan ? "enabled" : "disabled");
	
	/* If the driver is opened, we stop the DMA */
	if (gp->opened) {
		spin_lock_irq(&gp->lock);

		/* Stop traffic, mark us closed */
		netif_device_detach(dev);

		writel(0xffffffff, gp->regs + GREG_IMASK);

		/* Stop chip */
		gem_stop(gp);

		/* Get rid of ring buffers */
		gem_clean_rings(gp);

		spin_unlock_irq(&gp->lock);

		if (gp->pdev->vendor == PCI_VENDOR_ID_APPLE)
			disable_irq(gp->pdev->irq);
	}

	if (gp->hw_running) {
		/* Kill PM timer if any */
		del_timer_sync(&gp->pm_timer);
		flush_scheduled_tasks();

		gem_shutdown(gp);
	}

	return 0;
}

static int gem_resume(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	struct gem *gp = dev->priv;

	printk(KERN_INFO "%s: resuming\n", dev->name);

	if (gp->opened) {
#ifdef CONFIG_ALL_PPC
		/* First, we need to bring up the chip */
		if (gp->pdev->vendor == PCI_VENDOR_ID_APPLE) {
			gem_apple_powerup(gp);
			gem_check_invariants(gp);
		}
#endif /* CONFIG_ALL_PPC */
		spin_lock_irq(&gp->lock);

		gem_stop(gp);
		gp->hw_running = 1;
		gem_init_rings(gp);
		gem_init_hw(gp, 1);

		spin_unlock_irq(&gp->lock);

		netif_device_attach(dev);
		if (gp->pdev->vendor == PCI_VENDOR_ID_APPLE)
			enable_irq(gp->pdev->irq);
	}
	up(&gp->pm_sem);

	return 0;
}
#endif /* CONFIG_PM */

static struct net_device_stats *gem_get_stats(struct net_device *dev)
{
	struct gem *gp = dev->priv;
	struct net_device_stats *stats = &gp->net_stats;

	spin_lock_irq(&gp->lock);

	if (gp->hw_running) {
		stats->rx_crc_errors += readl(gp->regs + MAC_FCSERR);
		writel(0, gp->regs + MAC_FCSERR);

		stats->rx_frame_errors += readl(gp->regs + MAC_AERR);
		writel(0, gp->regs + MAC_AERR);

		stats->rx_length_errors += readl(gp->regs + MAC_LERR);
		writel(0, gp->regs + MAC_LERR);

		stats->tx_aborted_errors += readl(gp->regs + MAC_ECOLL);
		stats->collisions +=
			(readl(gp->regs + MAC_ECOLL) +
			 readl(gp->regs + MAC_LCOLL));
		writel(0, gp->regs + MAC_ECOLL);
		writel(0, gp->regs + MAC_LCOLL);
	}

	spin_unlock_irq(&gp->lock);

	return &gp->net_stats;
}

static void gem_set_multicast(struct net_device *dev)
{
	struct gem *gp = dev->priv;
	u32 rxcfg, rxcfg_new;
	int limit = 10000;
	
	if (!gp->hw_running)
		return;
		
	spin_lock_irq(&gp->lock);

	netif_stop_queue(dev);

	rxcfg = readl(gp->regs + MAC_RXCFG);
	gp->mac_rx_cfg = rxcfg_new = gem_setup_multicast(gp);
	
	writel(rxcfg & ~MAC_RXCFG_ENAB, gp->regs + MAC_RXCFG);
	while (readl(gp->regs + MAC_RXCFG) & MAC_RXCFG_ENAB) {
		if (!limit--)
			break;
		udelay(10);
	}

	rxcfg &= ~(MAC_RXCFG_PROM | MAC_RXCFG_HFE);
	rxcfg |= rxcfg_new;

	writel(rxcfg, gp->regs + MAC_RXCFG);

	netif_wake_queue(dev);

	spin_unlock_irq(&gp->lock);
}

/* Eventually add support for changing the advertisement
 * on autoneg.
 */
static int gem_ethtool_ioctl(struct net_device *dev, void *ep_user)
{
	struct gem *gp = dev->priv;
	u16 bmcr;
	int full_duplex, speed, pause;
	struct ethtool_cmd ecmd;

	if (copy_from_user(&ecmd, ep_user, sizeof(ecmd)))
		return -EFAULT;
		
	switch(ecmd.cmd) {
        case ETHTOOL_GDRVINFO: {
		struct ethtool_drvinfo info = { cmd: ETHTOOL_GDRVINFO };

		strncpy(info.driver, DRV_NAME, ETHTOOL_BUSINFO_LEN);
		strncpy(info.version, DRV_VERSION, ETHTOOL_BUSINFO_LEN);
		info.fw_version[0] = '\0';
		strncpy(info.bus_info, gp->pdev->slot_name, ETHTOOL_BUSINFO_LEN);
		info.regdump_len = 0; /*SUNGEM_NREGS;*/

		if (copy_to_user(ep_user, &info, sizeof(info)))
			return -EFAULT;

		return 0;
	}

	case ETHTOOL_GSET:
		ecmd.supported =
			(SUPPORTED_10baseT_Half | SUPPORTED_10baseT_Full |
			 SUPPORTED_100baseT_Half | SUPPORTED_100baseT_Full |
			 SUPPORTED_Autoneg | SUPPORTED_TP | SUPPORTED_MII);

		if (gp->gigabit_capable)
			ecmd.supported |=
				(SUPPORTED_1000baseT_Half |
				 SUPPORTED_1000baseT_Full);

		/* XXX hardcoded stuff for now */
		ecmd.port = PORT_MII;
		ecmd.transceiver = XCVR_EXTERNAL;
		ecmd.phy_address = 0; /* XXX fixed PHYAD */

		/* Record PHY settings if HW is on. */
		spin_lock_irq(&gp->lock);
		if (gp->hw_running) {
			bmcr = phy_read(gp, MII_BMCR);
			gem_read_mii_link_mode(gp, &full_duplex, &speed, &pause);
		} else
			bmcr = 0;
		spin_unlock_irq(&gp->lock);
		if (bmcr & BMCR_ANENABLE) {
			ecmd.autoneg = AUTONEG_ENABLE;
			ecmd.speed = speed == 10 ? SPEED_10 : (speed == 1000 ? SPEED_1000 : SPEED_100);
			ecmd.duplex = full_duplex ? DUPLEX_FULL : DUPLEX_HALF;
		} else {
			ecmd.autoneg = AUTONEG_DISABLE;
			ecmd.speed =
				(bmcr & BMCR_SPEED100) ?
				SPEED_100 : SPEED_10;
			ecmd.duplex =
				(bmcr & BMCR_FULLDPLX) ?
				DUPLEX_FULL : DUPLEX_HALF;
		}
		if (copy_to_user(ep_user, &ecmd, sizeof(ecmd)))
			return -EFAULT;
		return 0;

	case ETHTOOL_SSET:
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

		/* Apply settings and restart link process. */
		spin_lock_irq(&gp->lock);
		gem_begin_auto_negotiation(gp, &ecmd);
		spin_unlock_irq(&gp->lock);

		return 0;

	case ETHTOOL_NWAY_RST:
		if ((gp->link_cntl & BMCR_ANENABLE) == 0)
			return -EINVAL;

		/* Restart link process. */
		spin_lock_irq(&gp->lock);
		gem_begin_auto_negotiation(gp, NULL);
		spin_unlock_irq(&gp->lock);

		return 0;

	case ETHTOOL_GWOL:
	case ETHTOOL_SWOL:
		break; /* todo */

	/* get link status */
	case ETHTOOL_GLINK: {
		struct ethtool_value edata = { cmd: ETHTOOL_GLINK };

		edata.data = (gp->lstate == link_up);
		if (copy_to_user(ep_user, &edata, sizeof(edata)))
			return -EFAULT;
		return 0;
	}

	/* get message-level */
	case ETHTOOL_GMSGLVL: {
		struct ethtool_value edata = { cmd: ETHTOOL_GMSGLVL };

		edata.data = gp->msg_enable;
		if (copy_to_user(ep_user, &edata, sizeof(edata)))
			return -EFAULT;
		return 0;
	}

	/* set message-level */
	case ETHTOOL_SMSGLVL: {
		struct ethtool_value edata;

		if (copy_from_user(&edata, ep_user, sizeof(edata)))
			return -EFAULT;
		gp->msg_enable = edata.data;
		return 0;
	}

#if 0
	case ETHTOOL_GREGS: {
		struct ethtool_regs regs;
		u32 *regbuf;
		int r = 0;

		if (copy_from_user(&regs, useraddr, sizeof(regs)))
			return -EFAULT;
		
		if (regs.len > SUNGEM_NREGS) {
			regs.len = SUNGEM_NREGS;
		}
		regs.version = 0;
		if (copy_to_user(useraddr, &regs, sizeof(regs)))
			return -EFAULT;

		if (!gp->hw_running)
			return -ENODEV;
		useraddr += offsetof(struct ethtool_regs, data);

		/* Use kmalloc to avoid bloating the stack */
		regbuf = kmalloc(4 * SUNGEM_NREGS, GFP_KERNEL);
		if (!regbuf)
			return -ENOMEM;
		spin_lock_irq(&np->lock);
		gem_get_regs(gp, regbuf);
		spin_unlock_irq(&np->lock);

		if (copy_to_user(useraddr, regbuf, regs.len*sizeof(u32)))
			r = -EFAULT;
		kfree(regbuf);
		return r;
	}
#endif	
	};

	return -EOPNOTSUPP;
}

static int gem_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	struct gem *gp = dev->priv;
	struct mii_ioctl_data *data = (struct mii_ioctl_data *)&ifr->ifr_data;
	int rc = -EOPNOTSUPP;
	
	/* Hold the PM semaphore while doing ioctl's or we may collide
	 * with open/close and power management and oops.
	 */
	down(&gp->pm_sem);
	
	switch (cmd) {
	case SIOCETHTOOL:
		rc = gem_ethtool_ioctl(dev, ifr->ifr_data);
		break;

	case SIOCGMIIPHY:		/* Get address of MII PHY in use. */
		data->phy_id = gp->mii_phy_addr;
		/* Fallthrough... */

	case SIOCGMIIREG:		/* Read MII PHY register. */
		data->val_out = __phy_read(gp, data->reg_num & 0x1f, data->phy_id & 0x1f);
		rc = 0;
		break;

	case SIOCSMIIREG:		/* Write MII PHY register. */
		if (!capable(CAP_NET_ADMIN)) {
			rc = -EPERM;
		} else {
			__phy_write(gp, data->reg_num & 0x1f, data->val_in, data->phy_id & 0x1f);
			rc = 0;
		}
		break;
	};

	up(&gp->pm_sem);
	
	return rc;
}

#if (!defined(__sparc__) && !defined(CONFIG_ALL_PPC))
/* Fetch MAC address from vital product data of PCI ROM. */
static void find_eth_addr_in_vpd(void *rom_base, int len, unsigned char *dev_addr)
{
	int this_offset;

	for (this_offset = 0x20; this_offset < len; this_offset++) {
		void *p = rom_base + this_offset;
		int i;

		if (readb(p + 0) != 0x90 ||
		    readb(p + 1) != 0x00 ||
		    readb(p + 2) != 0x09 ||
		    readb(p + 3) != 0x4e ||
		    readb(p + 4) != 0x41 ||
		    readb(p + 5) != 0x06)
			continue;

		this_offset += 6;
		p += 6;

		for (i = 0; i < 6; i++)
			dev_addr[i] = readb(p + i);
		break;
	}
}

static void get_gem_mac_nonobp(struct pci_dev *pdev, unsigned char *dev_addr)
{
	u32 rom_reg_orig;
	void *p;

	if (pdev->resource[PCI_ROM_RESOURCE].parent == NULL) {
		if (pci_assign_resource(pdev, PCI_ROM_RESOURCE) < 0)
			goto use_random;
	}

	pci_read_config_dword(pdev, pdev->rom_base_reg, &rom_reg_orig);
	pci_write_config_dword(pdev, pdev->rom_base_reg,
			       rom_reg_orig | PCI_ROM_ADDRESS_ENABLE);

	p = ioremap(pci_resource_start(pdev, PCI_ROM_RESOURCE), (64 * 1024));
	if (p != NULL && readb(p) == 0x55 && readb(p + 1) == 0xaa)
		find_eth_addr_in_vpd(p, (64 * 1024), dev_addr);

	if (p != NULL)
		iounmap(p);

	pci_write_config_dword(pdev, pdev->rom_base_reg, rom_reg_orig);
	return;

use_random:
	/* Sun MAC prefix then 3 random bytes. */
	dev_addr[0] = 0x08;
	dev_addr[1] = 0x00;
	dev_addr[2] = 0x20;
	get_random_bytes(dev_addr + 3, 3);
	return;
}
#endif /* not Sparc and not PPC */

static int __devinit gem_get_device_address(struct gem *gp)
{
#if defined(__sparc__) || defined(CONFIG_ALL_PPC)
	struct net_device *dev = gp->dev;
#endif

#if defined(__sparc__)
	struct pci_dev *pdev = gp->pdev;
	struct pcidev_cookie *pcp = pdev->sysdata;
	int node = -1;

	if (pcp != NULL) {
		node = pcp->prom_node;
		if (prom_getproplen(node, "local-mac-address") == 6)
			prom_getproperty(node, "local-mac-address",
					 dev->dev_addr, 6);
		else
			node = -1;
	}
	if (node == -1)
		memcpy(dev->dev_addr, idprom->id_ethaddr, 6);
#elif defined(CONFIG_ALL_PPC)
	unsigned char *addr;

	addr = get_property(gp->of_node, "local-mac-address", NULL);
	if (addr == NULL) {
		printk("\n");
		printk(KERN_ERR "%s: can't get mac-address\n", dev->name);
		return -1;
	}
	memcpy(dev->dev_addr, addr, MAX_ADDR_LEN);
#else
	get_gem_mac_nonobp(gp->pdev, gp->dev->dev_addr);
#endif
	return 0;
}

static int __devinit gem_init_one(struct pci_dev *pdev,
				  const struct pci_device_id *ent)
{
	static int gem_version_printed = 0;
	unsigned long gemreg_base, gemreg_len;
	struct net_device *dev;
	struct gem *gp;
	int i, err, pci_using_dac;

	if (gem_version_printed++ == 0)
		printk(KERN_INFO "%s", version);

	/* Apple gmac note: during probe, the chip is powered up by
	 * the arch code to allow the code below to work (and to let
	 * the chip be probed on the config space. It won't stay powered
	 * up until the interface is brought up however, so we can't rely
	 * on register configuration done at this point.
	 */
	err = pci_enable_device(pdev);
	if (err) {
		printk(KERN_ERR PFX "Cannot enable MMIO operation, "
		       "aborting.\n");
		return err;
	}
	pci_set_master(pdev);

	/* Configure DMA attributes. */

	/* All of the GEM documentation states that 64-bit DMA addressing
	 * is fully supported and should work just fine.  However the
	 * front end for RIO based GEMs is different and only supports
	 * 32-bit addressing.
	 *
	 * For now we assume the various PPC GEMs are 32-bit only as well.
	 */
	if (pdev->vendor == PCI_VENDOR_ID_SUN &&
	    pdev->device == PCI_DEVICE_ID_SUN_GEM &&
	    !pci_set_dma_mask(pdev, (u64) 0xffffffffffffffff)) {
		pci_using_dac = 1;
	} else {
		err = pci_set_dma_mask(pdev, (u64) 0xffffffff);
		if (err) {
			printk(KERN_ERR PFX "No usable DMA configuration, "
			       "aborting.\n");
			return err;
		}
		pci_using_dac = 0;
	}

	gemreg_base = pci_resource_start(pdev, 0);
	gemreg_len = pci_resource_len(pdev, 0);

	if ((pci_resource_flags(pdev, 0) & IORESOURCE_IO) != 0) {
		printk(KERN_ERR PFX "Cannot find proper PCI device "
		       "base address, aborting.\n");
		return -ENODEV;
	}

	dev = alloc_etherdev(sizeof(*gp));
	if (!dev) {
		printk(KERN_ERR PFX "Etherdev alloc failed, aborting.\n");
		return -ENOMEM;
	}
	SET_MODULE_OWNER(dev);

	gp = dev->priv;

	if (pci_request_regions(pdev, dev->name)) {
		printk(KERN_ERR PFX "Cannot obtain PCI resources, "
		       "aborting.\n");
		goto err_out_free_netdev;
	}

	gp->pdev = pdev;
	dev->base_addr = (long) pdev;
	gp->dev = dev;

	gp->msg_enable = (gem_debug < 0 ? DEFAULT_MSG : gem_debug);

	spin_lock_init(&gp->lock);
	init_MUTEX(&gp->pm_sem);

	init_timer(&gp->link_timer);
	gp->link_timer.function = gem_link_timer;
	gp->link_timer.data = (unsigned long) gp;

	init_timer(&gp->pm_timer);
	gp->pm_timer.function = gem_pm_timer;
	gp->pm_timer.data = (unsigned long) gp;

	INIT_TQUEUE(&gp->pm_task, gem_pm_task, gp);
	INIT_TQUEUE(&gp->reset_task, gem_reset_task, gp);
	
	/* Default link parameters */
	if (link_mode >= 0 && link_mode <= 6)
		gp->link_cntl = link_modes[link_mode];
	else
		gp->link_cntl = BMCR_ANENABLE;
	gp->lstate = link_down;
	gp->timer_ticks = 0;

	gp->regs = (unsigned long) ioremap(gemreg_base, gemreg_len);
	if (gp->regs == 0UL) {
		printk(KERN_ERR PFX "Cannot map device registers, "
		       "aborting.\n");
		goto err_out_free_res;
	}

	/* On Apple, we power the chip up now in order for check
	 * invariants to work, but also because the firmware might
	 * not have properly shut down the PHY.
	 */
#ifdef CONFIG_ALL_PPC
	if (pdev->vendor == PCI_VENDOR_ID_APPLE)
		gem_apple_powerup(gp);
#endif
	spin_lock_irq(&gp->lock);
	gem_stop(gp);
	spin_unlock_irq(&gp->lock);

	if (gem_check_invariants(gp))
		goto err_out_iounmap;

	spin_lock_irq(&gp->lock);
	gp->hw_running = 1;
	gem_init_phy(gp);
	gem_begin_auto_negotiation(gp, NULL);
	spin_unlock_irq(&gp->lock);

	/* It is guarenteed that the returned buffer will be at least
	 * PAGE_SIZE aligned.
	 */
	gp->init_block = (struct gem_init_block *)
		pci_alloc_consistent(pdev, sizeof(struct gem_init_block),
				     &gp->gblock_dvma);
	if (!gp->init_block) {
		printk(KERN_ERR PFX "Cannot allocate init block, "
		       "aborting.\n");
		goto err_out_iounmap;
	}

#ifdef CONFIG_ALL_PPC
	gp->of_node = pci_device_to_OF_node(pdev);
#endif	
	if (gem_get_device_address(gp))
		goto err_out_free_consistent;

	if (register_netdev(dev)) {
		printk(KERN_ERR PFX "Cannot register net device, "
		       "aborting.\n");
		goto err_out_free_consistent;
	}

	printk(KERN_INFO "%s: Sun GEM (PCI) 10/100/1000BaseT Ethernet ",
	       dev->name);

	for (i = 0; i < 6; i++)
		printk("%2.2x%c", dev->dev_addr[i],
		       i == 5 ? ' ' : ':');
	printk("\n");

	pci_set_drvdata(pdev, dev);

	dev->open = gem_open;
	dev->stop = gem_close;
	dev->hard_start_xmit = gem_start_xmit;
	dev->get_stats = gem_get_stats;
	dev->set_multicast_list = gem_set_multicast;
	dev->do_ioctl = gem_ioctl;
	dev->tx_timeout = gem_tx_timeout;
	dev->watchdog_timeo = 5 * HZ;
	dev->change_mtu = gem_change_mtu;
	dev->irq = pdev->irq;
	dev->dma = 0;

	/* GEM can do it all... */
	dev->features |= NETIF_F_SG | NETIF_F_HW_CSUM;
	if (pci_using_dac)
		dev->features |= NETIF_F_HIGHDMA;

	/* Fire the PM timer that will shut us down in about 10 seconds */
	gp->pm_timer.expires = jiffies + 10*HZ;
	add_timer(&gp->pm_timer);

	return 0;

err_out_free_consistent:
	pci_free_consistent(pdev,
			    sizeof(struct gem_init_block),
			    gp->init_block,
			    gp->gblock_dvma);

err_out_iounmap:
	down(&gp->pm_sem);
	/* Stop the PM timer & task */
	del_timer_sync(&gp->pm_timer);
	flush_scheduled_tasks();
	if (gp->hw_running)
		gem_shutdown(gp);
	up(&gp->pm_sem);

	iounmap((void *) gp->regs);

err_out_free_res:
	pci_release_regions(pdev);

err_out_free_netdev:
	kfree(dev);

	return -ENODEV;

}

static void __devexit gem_remove_one(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);

	if (dev) {
		struct gem *gp = dev->priv;

		unregister_netdev(dev);

		down(&gp->pm_sem);
		/* Stop the PM timer & task */
		del_timer_sync(&gp->pm_timer);
		flush_scheduled_tasks();
		if (gp->hw_running)
			gem_shutdown(gp);
		up(&gp->pm_sem);

		pci_free_consistent(pdev,
				    sizeof(struct gem_init_block),
				    gp->init_block,
				    gp->gblock_dvma);
		iounmap((void *) gp->regs);
		pci_release_regions(pdev);
		kfree(dev);

		pci_set_drvdata(pdev, NULL);
	}
}

static struct pci_driver gem_driver = {
	name:		GEM_MODULE_NAME,
	id_table:	gem_pci_tbl,
	probe:		gem_init_one,
	remove:		__devexit_p(gem_remove_one),
#ifdef CONFIG_PM
	suspend:	gem_suspend,
	resume:		gem_resume,
#endif /* CONFIG_PM */
};

static int __init gem_init(void)
{
	return pci_module_init(&gem_driver);
}

static void __exit gem_cleanup(void)
{
	pci_unregister_driver(&gem_driver);
}

module_init(gem_init);
module_exit(gem_cleanup);
