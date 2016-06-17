/*
 * Network device driver for the GMAC ethernet controller on
 * Apple G4 Powermacs.
 *
 * Copyright (C) 2000 Paul Mackerras & Ben. Herrenschmidt
 * 
 * portions based on sunhme.c by David S. Miller
 *
 * Changes:
 * Arnaldo Carvalho de Melo <acme@conectiva.com.br> - 08/06/2000
 * - check init_etherdev return in gmac_probe1
 * BenH <benh@kernel.crashing.org> - 03/09/2000
 * - Add support for new PHYs
 * - Add some PowerBook sleep code
 * BenH <benh@kernel.crashing.org> - ??/??/????
 *  - PHY updates
 * BenH <benh@kernel.crashing.org> - 08/08/2001
 * - Add more PHYs, fixes to sleep code
 * Matt Domsch <Matt_Domsch@dell.com> - 11/12/2001
 * - use library crc32 functions
 */

#include <linux/module.h>

#include <linux/config.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/fcntl.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/crc32.h>
#include <asm/prom.h>
#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/machdep.h>
#include <asm/pmac_feature.h>
#include <asm/keylargo.h>
#include <asm/pci-bridge.h>
#ifdef CONFIG_PMAC_PBOOK
#include <linux/adb.h>
#include <linux/pmu.h>
#include <asm/irq.h>
#endif

#include "gmac.h"

#define DEBUG_PHY

/* Driver version 1.5, kernel 2.4.x */
#define GMAC_VERSION	"v1.5k4"

#define DUMMY_BUF_LEN	RX_BUF_ALLOC_SIZE + RX_OFFSET + GMAC_BUFFER_ALIGN
static unsigned char *dummy_buf;
static struct net_device *gmacs;

/* Prototypes */
static int mii_read(struct gmac *gm, int phy, int r);
static int mii_write(struct gmac *gm, int phy, int r, int v);
static void mii_poll_start(struct gmac *gm);
static void mii_poll_stop(struct gmac *gm);
static void mii_interrupt(struct gmac *gm);
static int mii_lookup_and_reset(struct gmac *gm);
static void mii_setup_phy(struct gmac *gm);
static int mii_do_reset_phy(struct gmac *gm, int phy_addr);
static void mii_init_BCM5400(struct gmac *gm);
static void mii_init_BCM5401(struct gmac *gm);

static void gmac_set_power(struct gmac *gm, int power_up);
static int gmac_powerup_and_reset(struct net_device *dev);
static void gmac_set_gigabit_mode(struct gmac *gm, int gigabit);
static void gmac_set_duplex_mode(struct gmac *gm, int full_duplex);
static void gmac_mac_init(struct gmac *gm, unsigned char *mac_addr);
static void gmac_init_rings(struct gmac *gm, int from_irq);
static void gmac_start_dma(struct gmac *gm);
static void gmac_stop_dma(struct gmac *gm);
static void gmac_set_multicast(struct net_device *dev);
static int gmac_open(struct net_device *dev);
static int gmac_close(struct net_device *dev);
static void gmac_tx_timeout(struct net_device *dev);
static int gmac_xmit_start(struct sk_buff *skb, struct net_device *dev);
static void gmac_tx_cleanup(struct net_device *dev, int force_cleanup);
static void gmac_receive(struct net_device *dev);
static void gmac_interrupt(int irq, void *dev_id, struct pt_regs *regs);
static struct net_device_stats *gmac_stats(struct net_device *dev);
static int gmac_probe(void);
static void gmac_probe1(struct device_node *gmac);

#ifdef CONFIG_PMAC_PBOOK
int gmac_sleep_notify(struct pmu_sleep_notifier *self, int when);
static struct pmu_sleep_notifier gmac_sleep_notifier = {
	gmac_sleep_notify, SLEEP_LEVEL_NET,
};
#endif

/*
 * Read via the mii interface from a PHY register
 */
static int
mii_read(struct gmac *gm, int phy, int r)
{
	int timeout;

	GM_OUT(GM_MIF_FRAME_CTL_DATA,
		(0x01 << GM_MIF_FRAME_START_SHIFT) |
		(0x02 << GM_MIF_FRAME_OPCODE_SHIFT) |
		GM_MIF_FRAME_TURNAROUND_HI |
		(phy << GM_MIF_FRAME_PHY_ADDR_SHIFT) |
		(r << GM_MIF_FRAME_REG_ADDR_SHIFT));
		
	for (timeout = 1000; timeout > 0; --timeout) {
		udelay(20);
		if (GM_IN(GM_MIF_FRAME_CTL_DATA) & GM_MIF_FRAME_TURNAROUND_LO)
			return GM_IN(GM_MIF_FRAME_CTL_DATA) & GM_MIF_FRAME_DATA_MASK;
	}
	return -1;
}

/*
 * Write on the mii interface to a PHY register
 */
static int
mii_write(struct gmac *gm, int phy, int r, int v)
{
	int timeout;

	GM_OUT(GM_MIF_FRAME_CTL_DATA,
		(0x01 << GM_MIF_FRAME_START_SHIFT) |
		(0x01 << GM_MIF_FRAME_OPCODE_SHIFT) |
		GM_MIF_FRAME_TURNAROUND_HI |
		(phy << GM_MIF_FRAME_PHY_ADDR_SHIFT) |
		(r << GM_MIF_FRAME_REG_ADDR_SHIFT) |
		(v & GM_MIF_FRAME_DATA_MASK));

	for (timeout = 1000; timeout > 0; --timeout) {
		udelay(20);
		if (GM_IN(GM_MIF_FRAME_CTL_DATA) & GM_MIF_FRAME_TURNAROUND_LO)
			return 0;
	}
	return -1;
}

/*
 * Start MIF autopolling of the PHY status register
 */
static void 
mii_poll_start(struct gmac *gm)
{
	unsigned int tmp;
	
	/* Start the MIF polling on the external transceiver. */
	tmp = GM_IN(GM_MIF_CFG);
	tmp &= ~(GM_MIF_CFGPR_MASK | GM_MIF_CFGPD_MASK);
	tmp |= ((gm->phy_addr & 0x1f) << GM_MIF_CFGPD_SHIFT);
	tmp |= (MII_SR << GM_MIF_CFGPR_SHIFT);
	tmp |= GM_MIF_CFGPE;
	GM_OUT(GM_MIF_CFG, tmp);

	/* Let the bits set. */
	udelay(GM_MIF_POLL_DELAY);

	GM_OUT(GM_MIF_IRQ_MASK, 0xffc0);
}

/*
 * Stop MIF autopolling of the PHY status register
 */
static void 
mii_poll_stop(struct gmac *gm)
{
	GM_OUT(GM_MIF_IRQ_MASK, 0xffff);
	GM_BIC(GM_MIF_CFG, GM_MIF_CFGPE);
	udelay(GM_MIF_POLL_DELAY);
}

/*
 * Called when the MIF detect a change of the PHY status
 * 
 * handles monitoring the link and updating GMAC with the correct
 * duplex mode.
 * 
 * Note: Are we missing status changes ? In this case, we'll have to
 * a timer and control the autoneg. process more closely. Also, we may
 * want to stop rx and tx side when the link is down.
 */

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

static void
mii_interrupt(struct gmac *gm)
{
	int		phy_status;
	int		lpar_ability;
	
	mii_poll_stop(gm);

	/* May the status change before polling is re-enabled ? */
	mii_poll_start(gm);
	
	/* We read the Auxilliary Status Summary register */
	phy_status = mii_read(gm, gm->phy_addr, MII_SR);
	if ((phy_status ^ gm->phy_status) & (MII_SR_ASSC | MII_SR_LKS)) {
		int		full_duplex = 0;
		int		link_100 = 0;
		int		gigabit = 0;
#ifdef DEBUG_PHY
		printk(KERN_INFO "%s: Link state change, phy_status: 0x%04x\n",
			gm->dev->name, phy_status);
#endif
		gm->phy_status = phy_status;

		/* Should we enable that in generic mode ? */
		lpar_ability = mii_read(gm, gm->phy_addr, MII_ANLPA);
		if (lpar_ability & MII_ANLPA_PAUS)
			GM_BIS(GM_MAC_CTRL_CONFIG, GM_MAC_CTRL_CONF_SND_PAUSE_EN);
		else
			GM_BIC(GM_MAC_CTRL_CONFIG, GM_MAC_CTRL_CONF_SND_PAUSE_EN);

		/* Link ? Check for speed and duplex */
		if ((phy_status & MII_SR_LKS) && (phy_status & MII_SR_ASSC)) {
		    int restart = 0;
		    int aux_stat, link;
		    switch (gm->phy_type) {
		      case PHY_B5201:
		      case PHY_B5221:
		    	aux_stat = mii_read(gm, gm->phy_addr, MII_BCM5201_AUXCTLSTATUS);
#ifdef DEBUG_PHY
			printk(KERN_INFO "%s:    Link up ! BCM5201/5221 aux_stat: 0x%04x\n",
				gm->dev->name, aux_stat);
#endif
		    	full_duplex = ((aux_stat & MII_BCM5201_AUXCTLSTATUS_DUPLEX) != 0);
		    	link_100 = ((aux_stat & MII_BCM5201_AUXCTLSTATUS_SPEED) != 0);
			netif_carrier_on(gm->dev);
		        break;
		      case PHY_B5400:
		      case PHY_B5401:
		      case PHY_B5411:
		    	aux_stat = mii_read(gm, gm->phy_addr, MII_BCM5400_AUXSTATUS);
		    	link = (aux_stat & MII_BCM5400_AUXSTATUS_LINKMODE_MASK) >>
		    			MII_BCM5400_AUXSTATUS_LINKMODE_SHIFT;
#ifdef DEBUG_PHY
			printk(KERN_INFO "%s:    Link up ! BCM54xx aux_stat: 0x%04x (link mode: %d)\n",
				gm->dev->name, aux_stat, link);
#endif
		    	full_duplex = phy_BCM5400_link_table[link][0];
		    	link_100 = phy_BCM5400_link_table[link][1];
		    	gigabit = phy_BCM5400_link_table[link][2];
			netif_carrier_on(gm->dev);
		    	break;
		      case PHY_LXT971:
		    	aux_stat = mii_read(gm, gm->phy_addr, MII_LXT971_STATUS2);
#ifdef DEBUG_PHY
			printk(KERN_INFO "%s:    Link up ! LXT971 stat2: 0x%04x\n",
				gm->dev->name, aux_stat);
#endif
		    	full_duplex = ((aux_stat & MII_LXT971_STATUS2_FULLDUPLEX) != 0);
		    	link_100 = ((aux_stat & MII_LXT971_STATUS2_SPEED) != 0);
			netif_carrier_on(gm->dev);
		    	break;
		      default:
		    	full_duplex = (lpar_ability & MII_ANLPA_FDAM) != 0;
		    	link_100 = (lpar_ability & MII_ANLPA_100M) != 0;
		    	break;
		    }
#ifdef DEBUG_PHY
		    printk(KERN_INFO "%s:    Full Duplex: %d, Speed: %s\n",
		    	gm->dev->name, full_duplex,
		    	gigabit ? "1000" : (link_100 ? "100" : "10"));
#endif
                    if (gigabit != gm->gigabit) {
                    	gm->gigabit = gigabit;
                    	gmac_set_gigabit_mode(gm, gm->gigabit);
                    	restart = 1;
                    }
		    if (full_duplex != gm->full_duplex) {
			gm->full_duplex = full_duplex;
			gmac_set_duplex_mode(gm, gm->full_duplex);
			restart = 1;
		    }
		    if (restart)
			gmac_start_dma(gm);
		} else if (!(phy_status & MII_SR_LKS)) {
#ifdef DEBUG_PHY
		    printk(KERN_INFO "%s:    Link down !\n", gm->dev->name);
#endif
			netif_carrier_off(gm->dev);
		}
	}
}

#ifdef CONFIG_PMAC_PBOOK
/* Power management: stop PHY chip for suspend mode
 * 
 * TODO: This will have to be modified is WOL is to be supported.
 */
static void
gmac_suspend(struct gmac* gm)
{
	int data, timeout;
	unsigned long flags;
	
	gm->sleeping = 1;
	netif_device_detach(gm->dev);


	spin_lock_irqsave(&gm->lock, flags);
	if (gm->opened) {
		disable_irq(gm->dev->irq);
		/* Stop polling PHY */
		mii_poll_stop(gm);
	}
	/* Mask out all chips interrupts */
	GM_OUT(GM_IRQ_MASK, 0xffffffff);
	spin_unlock_irqrestore(&gm->lock, flags);
	
	if (gm->opened) {
		int i;
		/* Empty Tx ring of any remaining gremlins */
		gmac_tx_cleanup(gm->dev, 1);
	
		/* Empty Rx ring of any remaining gremlins */
		for (i = 0; i < NRX; ++i) {
			if (gm->rx_buff[i] != 0) {
				dev_kfree_skb_irq(gm->rx_buff[i]);
				gm->rx_buff[i] = 0;
			}
		}
	}

	/* Clear interrupts on 5201 */
	if (gm->phy_type == PHY_B5201 || gm->phy_type == PHY_B5221)
		mii_write(gm, gm->phy_addr, MII_BCM5201_INTERRUPT, 0);
		
	/* Drive MDIO high */
	GM_OUT(GM_MIF_CFG, 0);
	
	/* Unchanged, don't ask me why */
	data = mii_read(gm, gm->phy_addr, MII_ANLPA);
	mii_write(gm, gm->phy_addr, MII_ANLPA, data);
	
	/* Stop everything */
	GM_OUT(GM_MAC_RX_CONFIG, 0);
	GM_OUT(GM_MAC_TX_CONFIG, 0);
	GM_OUT(GM_MAC_XIF_CONFIG, 0);
	GM_OUT(GM_TX_CONF, 0);
	GM_OUT(GM_RX_CONF, 0);
	
	/* Set MAC in reset state */
	GM_OUT(GM_RESET, GM_RESET_TX | GM_RESET_RX);
	for (timeout = 100; timeout > 0; --timeout) {
		mdelay(10);
		if ((GM_IN(GM_RESET) & (GM_RESET_TX | GM_RESET_RX)) == 0)
			break;
	}
	GM_OUT(GM_MAC_TX_RESET, GM_MAC_TX_RESET_NOW);
	GM_OUT(GM_MAC_RX_RESET, GM_MAC_RX_RESET_NOW);

	/* Superisolate PHY */
	if (gm->phy_type == PHY_B5201 || gm->phy_type == PHY_B5221)
		mii_write(gm, gm->phy_addr, MII_BCM5201_MULTIPHY,
			MII_BCM5201_MULTIPHY_SUPERISOLATE);

	/* Put MDIO in sane electric state. According to an obscure
	 * Apple comment, not doing so may let them drive some current
	 * during sleep and possibly damage BCM PHYs.
	 */
	GM_OUT(GM_MIF_CFG, GM_MIF_CFGBB);
	GM_OUT(GM_MIF_BB_CLOCK, 0);
	GM_OUT(GM_MIF_BB_DATA, 0);
	GM_OUT(GM_MIF_BB_OUT_ENABLE, 0);
	GM_OUT(GM_MAC_XIF_CONFIG,
		GM_MAC_XIF_CONF_GMII_MODE|GM_MAC_XIF_CONF_MII_INT_LOOP);
	(void)GM_IN(GM_MAC_XIF_CONFIG);
	
	/* Unclock the GMAC chip */
	gmac_set_power(gm, 0);
}

static void
gmac_resume(struct gmac *gm)
{
	int data;

	if (gmac_powerup_and_reset(gm->dev)) {
		printk(KERN_ERR "%s: Couldn't revive gmac ethernet !\n", gm->dev->name);
		return;
	}

	gm->sleeping = 0;
	
	if (gm->opened) {
		/* Create fresh rings */
		gmac_init_rings(gm, 1);
		/* re-initialize the MAC */
		gmac_mac_init(gm, gm->dev->dev_addr);	
		/* re-initialize the multicast tables & promisc mode if any */
		gmac_set_multicast(gm->dev);
	}

	/* Early enable Tx and Rx so that we are clocked */
	GM_BIS(GM_TX_CONF, GM_TX_CONF_DMA_EN);
	mdelay(20);
	GM_BIS(GM_RX_CONF, GM_RX_CONF_DMA_EN);
	mdelay(20);
	GM_BIS(GM_MAC_TX_CONFIG, GM_MAC_TX_CONF_ENABLE);
	mdelay(20);
	GM_BIS(GM_MAC_RX_CONFIG, GM_MAC_RX_CONF_ENABLE);
	mdelay(20);
	if (gm->phy_type == PHY_B5201 || gm->phy_type == PHY_B5221) {
		data = mii_read(gm, gm->phy_addr, MII_BCM5201_MULTIPHY);
		mii_write(gm, gm->phy_addr, MII_BCM5201_MULTIPHY,
			data & ~MII_BCM5201_MULTIPHY_SUPERISOLATE);
	}
	mdelay(1);

	if (gm->opened) {
		/* restart polling PHY */
		mii_interrupt(gm);
		/* restart DMA operations */
		gmac_start_dma(gm);
		netif_device_attach(gm->dev);
		enable_irq(gm->dev->irq);
	} else {
		/* Driver not opened, just leave things off. Note that
		 * we could be smart and superisolate the PHY when the
		 * driver is closed, but I won't do that unless I have
		 * a better understanding of some electrical issues with
		 * this PHY chip --BenH
		 */
		GM_OUT(GM_MAC_RX_CONFIG, 0);
		GM_OUT(GM_MAC_TX_CONFIG, 0);
		GM_OUT(GM_MAC_XIF_CONFIG, 0);
		GM_OUT(GM_TX_CONF, 0);
		GM_OUT(GM_RX_CONF, 0);
	}
}
#endif

static int
mii_do_reset_phy(struct gmac *gm, int phy_addr)
{
	int mii_control, timeout;
	
	mii_control = mii_read(gm, phy_addr, MII_CR);
	mii_write(gm, phy_addr, MII_CR, mii_control | MII_CR_RST);
	mdelay(10);
	for (timeout = 100; timeout > 0; --timeout) {
		mii_control = mii_read(gm, phy_addr, MII_CR);
		if (mii_control == -1) {
			printk(KERN_ERR "%s PHY died after reset !\n",
				gm->dev->name);
			return 1;
		}
		if ((mii_control & MII_CR_RST) == 0)
			break;
		mdelay(10);
	}
	if (mii_control & MII_CR_RST) {
		printk(KERN_ERR "%s PHY reset timeout !\n", gm->dev->name);
		return 1;
	}
	mii_write(gm, phy_addr, MII_CR, mii_control & ~MII_CR_ISOL);
	return 0;
}

/* Here's a bunch of configuration routines for
 * Broadcom PHYs used on various Mac models. Unfortunately,
 * except for the 5201, Broadcom never sent me any documentation,
 * so this is from my understanding of Apple's Open Firmware
 * drivers and Darwin's implementation
 */
 
static void
mii_init_BCM5400(struct gmac *gm)
{
	int data;

	/* Configure for gigabit full duplex */
	data = mii_read(gm, gm->phy_addr, MII_BCM5400_AUXCONTROL);
	data |= MII_BCM5400_AUXCONTROL_PWR10BASET;
	mii_write(gm, gm->phy_addr, MII_BCM5400_AUXCONTROL, data);
	
	data = mii_read(gm, gm->phy_addr, MII_BCM5400_GB_CONTROL);
	data |= MII_BCM5400_GB_CONTROL_FULLDUPLEXCAP;
	mii_write(gm, gm->phy_addr, MII_BCM5400_GB_CONTROL, data);
	
	mdelay(10);

	/* Reset and configure cascaded 10/100 PHY */
	mii_do_reset_phy(gm, 0x1f);
	
	data = mii_read(gm, 0x1f, MII_BCM5201_MULTIPHY);
	data |= MII_BCM5201_MULTIPHY_SERIALMODE;
	mii_write(gm, 0x1f, MII_BCM5201_MULTIPHY, data);

	data = mii_read(gm, gm->phy_addr, MII_BCM5400_AUXCONTROL);
	data &= ~MII_BCM5400_AUXCONTROL_PWR10BASET;
	mii_write(gm, gm->phy_addr, MII_BCM5400_AUXCONTROL, data);
}

static void
mii_init_BCM5401(struct gmac *gm)
{
	int data;
	int rev;

	rev = mii_read(gm, gm->phy_addr, MII_ID1) & 0x000f;
	if (rev == 0 || rev == 3) {
		/* Some revisions of 5401 appear to need this
		 * initialisation sequence to disable, according
		 * to OF, "tap power management"
		 * 
		 * WARNING ! OF and Darwin don't agree on the
		 * register addresses. OF seem to interpret the
		 * register numbers below as decimal
		 */
		mii_write(gm, gm->phy_addr, 0x18, 0x0c20);
		mii_write(gm, gm->phy_addr, 0x17, 0x0012);
		mii_write(gm, gm->phy_addr, 0x15, 0x1804);
		mii_write(gm, gm->phy_addr, 0x17, 0x0013);
		mii_write(gm, gm->phy_addr, 0x15, 0x1204);
		mii_write(gm, gm->phy_addr, 0x17, 0x8006);
		mii_write(gm, gm->phy_addr, 0x15, 0x0132);
		mii_write(gm, gm->phy_addr, 0x17, 0x8006);
		mii_write(gm, gm->phy_addr, 0x15, 0x0232);
		mii_write(gm, gm->phy_addr, 0x17, 0x201f);
		mii_write(gm, gm->phy_addr, 0x15, 0x0a20);
	}
	
	/* Configure for gigabit full duplex */
	data = mii_read(gm, gm->phy_addr, MII_BCM5400_GB_CONTROL);
	data |= MII_BCM5400_GB_CONTROL_FULLDUPLEXCAP;
	mii_write(gm, gm->phy_addr, MII_BCM5400_GB_CONTROL, data);

	mdelay(10);

	/* Reset and configure cascaded 10/100 PHY */
	mii_do_reset_phy(gm, 0x1f);
	
	data = mii_read(gm, 0x1f, MII_BCM5201_MULTIPHY);
	data |= MII_BCM5201_MULTIPHY_SERIALMODE;
	mii_write(gm, 0x1f, MII_BCM5201_MULTIPHY, data);
}

static void
mii_init_BCM5411(struct gmac *gm)
{
	int data;

	/* Here's some more Apple black magic to setup
	 * some voltage stuffs.
	 */
	mii_write(gm, gm->phy_addr, 0x1c, 0x8c23);
	mii_write(gm, gm->phy_addr, 0x1c, 0x8ca3);
	mii_write(gm, gm->phy_addr, 0x1c, 0x8c23);

	/* Here, Apple seems to want to reset it, do
	 * it as well
	 */
	mii_write(gm, gm->phy_addr, MII_CR, MII_CR_RST);

	/* Start autoneg */
	mii_write(gm, gm->phy_addr, MII_CR,
			MII_CR_ASSE|MII_CR_FDM|	/* Autospeed, full duplex */
			MII_CR_RAN|
			MII_CR_SPEEDSEL2 /* chip specific, gigabit enable ? */);

	data = mii_read(gm, gm->phy_addr, MII_BCM5400_GB_CONTROL);
	data |= MII_BCM5400_GB_CONTROL_FULLDUPLEXCAP;
	mii_write(gm, gm->phy_addr, MII_BCM5400_GB_CONTROL, data);
}

static int
mii_lookup_and_reset(struct gmac *gm)
{
	int	i, mii_status, mii_control;

	gm->phy_addr = -1;
	gm->phy_type = PHY_UNKNOWN;

	/* Hard reset the PHY */
	pmac_call_feature(PMAC_FTR_GMAC_PHY_RESET, gm->of_node, 0, 0);
		
	/* Find the PHY */
	for(i=0; i<=31; i++) {
		mii_control = mii_read(gm, i, MII_CR);
		mii_status = mii_read(gm, i, MII_SR);
		if (mii_control != -1  && mii_status != -1 &&
			(mii_control != 0xffff || mii_status != 0xffff))
			break;
	}
	gm->phy_addr = i;
	if (gm->phy_addr > 31)
		return 0;

	/* Reset it */
	if (mii_do_reset_phy(gm, gm->phy_addr))
		goto fail;
	
	/* Read the PHY ID */
	gm->phy_id = (mii_read(gm, gm->phy_addr, MII_ID0) << 16) |
		mii_read(gm, gm->phy_addr, MII_ID1);
#ifdef DEBUG_PHY
	printk(KERN_INFO "%s: PHY ID: 0x%08x\n", gm->dev->name, gm->phy_id);
#endif
	if ((gm->phy_id & MII_BCM5400_MASK) == MII_BCM5400_ID) {
		gm->phy_type = PHY_B5400;
		printk(KERN_INFO  "%s: Found Broadcom BCM5400 PHY (Gigabit)\n",
			gm->dev->name);
		mii_init_BCM5400(gm);		
	} else if ((gm->phy_id & MII_BCM5401_MASK) == MII_BCM5401_ID) {
		gm->phy_type = PHY_B5401;
		printk(KERN_INFO  "%s: Found Broadcom BCM5401 PHY (Gigabit)\n",
			gm->dev->name);
		mii_init_BCM5401(gm);		
	} else if ((gm->phy_id & MII_BCM5411_MASK) == MII_BCM5411_ID) {
		gm->phy_type = PHY_B5411;
		printk(KERN_INFO  "%s: Found Broadcom BCM5411 PHY (Gigabit)\n",
			gm->dev->name);
		mii_init_BCM5411(gm);		
	} else if ((gm->phy_id & MII_BCM5201_MASK) == MII_BCM5201_ID) {
		gm->phy_type = PHY_B5201;
		printk(KERN_INFO "%s: Found Broadcom BCM5201 PHY\n", gm->dev->name);
	} else if ((gm->phy_id & MII_BCM5221_MASK) == MII_BCM5221_ID) {
		gm->phy_type = PHY_B5221;
		printk(KERN_INFO "%s: Found Broadcom BCM5221 PHY\n", gm->dev->name);
	} else if ((gm->phy_id & MII_LXT971_MASK) == MII_LXT971_ID) {
		gm->phy_type = PHY_LXT971;
		printk(KERN_INFO "%s: Found LevelOne LX971 PHY\n", gm->dev->name);
	} else {
		printk(KERN_WARNING "%s: Warning ! Unknown PHY ID 0x%08x, using generic mode...\n",
			gm->dev->name, gm->phy_id);
	}

	return 1;
	
fail:
	gm->phy_addr = -1;
	return 0;
}

/* 
 * Setup the PHY autonegociation parameters
 * 
 * Code to force the PHY duplex mode and speed should be
 * added here
 */
static void
mii_setup_phy(struct gmac *gm)
{
	int data;
	
	/* Stop auto-negociation */
	data = mii_read(gm, gm->phy_addr, MII_CR);
	mii_write(gm, gm->phy_addr, MII_CR, data & ~MII_CR_ASSE);

	/* Set advertisement to 10/100 and Half/Full duplex
	 * (full capabilities) */
	data = mii_read(gm, gm->phy_addr, MII_ANA);
	data |= MII_ANA_TXAM | MII_ANA_FDAM | MII_ANA_10M;
	mii_write(gm, gm->phy_addr, MII_ANA, data);
	
	/* Restart auto-negociation */
	data = mii_read(gm, gm->phy_addr, MII_CR);
	data |= MII_CR_ASSE;
	mii_write(gm, gm->phy_addr, MII_CR, data);
	data |= MII_CR_RAN;
	mii_write(gm, gm->phy_addr, MII_CR, data);
}

/* 
 * Turn On/Off the gmac cell inside Uni-N
 * 
 * ToDo: Add code to support powering down of the PHY.
 */
static void
gmac_set_power(struct gmac *gm, int power_up)
{
	if (power_up) {
		pmac_call_feature(PMAC_FTR_GMAC_ENABLE, gm->of_node, 0, 1);
		if (gm->pci_devfn != 0xff) {
			u16 cmd;
			
			/*
			 * Make sure PCI is correctly configured
			 *
			 * We use old pci_bios versions of the function since, by
			 * default, gmac is not powered up, and so will be absent
			 * from the kernel initial PCI lookup. 
			 * 
			 * Should be replaced by 2.4 new PCI mecanisms and really
			 * regiser the device.
			 */
			pcibios_read_config_word(gm->pci_bus, gm->pci_devfn,
				PCI_COMMAND, &cmd);
			cmd |= PCI_COMMAND_MEMORY | PCI_COMMAND_MASTER | PCI_COMMAND_INVALIDATE;
	    		pcibios_write_config_word(gm->pci_bus, gm->pci_devfn,
	    			PCI_COMMAND, cmd);
	    		pcibios_write_config_byte(gm->pci_bus, gm->pci_devfn,
	    			PCI_LATENCY_TIMER, 16);
	    		pcibios_write_config_byte(gm->pci_bus, gm->pci_devfn,
	    			PCI_CACHE_LINE_SIZE, 8);
		}
	} else {
		pmac_call_feature(PMAC_FTR_GMAC_ENABLE, gm->of_node, 0, 0);
	}
}

/*
 * Makes sure the GMAC cell is powered up, and reset it
 */
static int
gmac_powerup_and_reset(struct net_device *dev)
{
	struct gmac *gm = (struct gmac *) dev->priv;
	int timeout;
	
	/* turn on GB clock */
	gmac_set_power(gm, 1);
	/* Perform a software reset */
	GM_OUT(GM_RESET, GM_RESET_TX | GM_RESET_RX);
	for (timeout = 100; timeout > 0; --timeout) {
		mdelay(10);
		if ((GM_IN(GM_RESET) & (GM_RESET_TX | GM_RESET_RX)) == 0) {
			/* Mask out all chips interrupts */
			GM_OUT(GM_IRQ_MASK, 0xffffffff);
			GM_OUT(GM_MAC_TX_RESET, GM_MAC_TX_RESET_NOW);
			GM_OUT(GM_MAC_RX_RESET, GM_MAC_RX_RESET_NOW);
			return 0;
		}
	}
	printk(KERN_ERR "%s reset failed!\n", dev->name);
	gmac_set_power(gm, 0);
	gm->phy_type = 0;
	return -1;
}

/*
 * Set the MAC duplex mode.
 * 
 * Side effect: stops Tx MAC
 */
static void
gmac_set_duplex_mode(struct gmac *gm, int full_duplex)
{
	/* Stop Tx MAC */
	GM_BIC(GM_MAC_TX_CONFIG, GM_MAC_TX_CONF_ENABLE);
	while(GM_IN(GM_MAC_TX_CONFIG) & GM_MAC_TX_CONF_ENABLE)
		;
	
	if (full_duplex) {
		GM_BIS(GM_MAC_TX_CONFIG, GM_MAC_TX_CONF_IGNORE_CARRIER
			| GM_MAC_TX_CONF_IGNORE_COLL);
		GM_BIC(GM_MAC_XIF_CONFIG, GM_MAC_XIF_CONF_DISABLE_ECHO);
	} else {
		GM_BIC(GM_MAC_TX_CONFIG, GM_MAC_TX_CONF_IGNORE_CARRIER
			| GM_MAC_TX_CONF_IGNORE_COLL);
		GM_BIS(GM_MAC_XIF_CONFIG, GM_MAC_XIF_CONF_DISABLE_ECHO);
	}
}

/* Set the MAC gigabit mode. Side effect: stops Tx MAC */
static void
gmac_set_gigabit_mode(struct gmac *gm, int gigabit)
{
	/* Stop Tx MAC */
	GM_BIC(GM_MAC_TX_CONFIG, GM_MAC_TX_CONF_ENABLE);
	while(GM_IN(GM_MAC_TX_CONFIG) & GM_MAC_TX_CONF_ENABLE)
		;
	
	if (gigabit) {
		GM_BIS(GM_MAC_XIF_CONFIG, GM_MAC_XIF_CONF_GMII_MODE);
	} else {
		GM_BIC(GM_MAC_XIF_CONFIG, GM_MAC_XIF_CONF_GMII_MODE);
	}
}

/*
 * Initialize a bunch of registers to put the chip into a known
 * and hopefully happy state
 */
static void
gmac_mac_init(struct gmac *gm, unsigned char *mac_addr)
{
	int i, fifo_size;

	/* Set random seed to low bits of MAC address */
	GM_OUT(GM_MAC_RANDOM_SEED, mac_addr[5] | (mac_addr[4] << 8));
	
	/* Configure the data path mode to MII/GII */
	GM_OUT(GM_PCS_DATAPATH_MODE, GM_PCS_DATAPATH_MII);
	
	/* Configure XIF to MII mode. Full duplex led is set
	 * by Apple, so...
	 */
	GM_OUT(GM_MAC_XIF_CONFIG, GM_MAC_XIF_CONF_TX_MII_OUT_EN
		| GM_MAC_XIF_CONF_FULL_DPLX_LED);

	/* Mask out all MAC interrupts */
	GM_OUT(GM_MAC_TX_MASK, 0xffff);
	GM_OUT(GM_MAC_RX_MASK, 0xffff);
	GM_OUT(GM_MAC_CTRLSTAT_MASK, 0xff);
	
	/* Setup bits of MAC */
	GM_OUT(GM_MAC_SND_PAUSE, GM_MAC_SND_PAUSE_DEFAULT);
	GM_OUT(GM_MAC_CTRL_CONFIG, GM_MAC_CTRL_CONF_RCV_PAUSE_EN);
	
	/* Configure GEM DMA */
	GM_OUT(GM_GCONF, GM_GCONF_BURST_SZ |
		(31 << GM_GCONF_TXDMA_LIMIT_SHIFT) |
		(31 << GM_GCONF_RXDMA_LIMIT_SHIFT));
	GM_OUT(GM_TX_CONF,
		(GM_TX_CONF_FIFO_THR_DEFAULT << GM_TX_CONF_FIFO_THR_SHIFT) |
		NTX_CONF);

	/* 34 byte offset for checksum computation.  This works because ip_input() will clear out
	 * the skb->csum and skb->ip_summed fields and recompute the csum if IP options are
	 * present in the header.  34 == (ethernet header len) + sizeof(struct iphdr)
 	*/
	GM_OUT(GM_RX_CONF,
		(RX_OFFSET << GM_RX_CONF_FBYTE_OFF_SHIFT) |
		(0x22 << GM_RX_CONF_CHK_START_SHIFT) |
		(GM_RX_CONF_DMA_THR_DEFAULT << GM_RX_CONF_DMA_THR_SHIFT) |
		NRX_CONF);

	/* Configure other bits of MAC */
	GM_OUT(GM_MAC_INTR_PKT_GAP0, GM_MAC_INTR_PKT_GAP0_DEFAULT);
	GM_OUT(GM_MAC_INTR_PKT_GAP1, GM_MAC_INTR_PKT_GAP1_DEFAULT);
	GM_OUT(GM_MAC_INTR_PKT_GAP2, GM_MAC_INTR_PKT_GAP2_DEFAULT);
	GM_OUT(GM_MAC_MIN_FRAME_SIZE, GM_MAC_MIN_FRAME_SIZE_DEFAULT);
	GM_OUT(GM_MAC_MAX_FRAME_SIZE, GM_MAC_MAX_FRAME_SIZE_DEFAULT);
	GM_OUT(GM_MAC_PREAMBLE_LEN, GM_MAC_PREAMBLE_LEN_DEFAULT);
	GM_OUT(GM_MAC_JAM_SIZE, GM_MAC_JAM_SIZE_DEFAULT);
	GM_OUT(GM_MAC_ATTEMPT_LIMIT, GM_MAC_ATTEMPT_LIMIT_DEFAULT);
	GM_OUT(GM_MAC_SLOT_TIME, GM_MAC_SLOT_TIME_DEFAULT);
	GM_OUT(GM_MAC_CONTROL_TYPE, GM_MAC_CONTROL_TYPE_DEFAULT);
	
	/* Setup MAC addresses, clear filters, clear hash table */
	GM_OUT(GM_MAC_ADDR_NORMAL0, (mac_addr[4] << 8) + mac_addr[5]);
	GM_OUT(GM_MAC_ADDR_NORMAL1, (mac_addr[2] << 8) + mac_addr[3]);
	GM_OUT(GM_MAC_ADDR_NORMAL2, (mac_addr[0] << 8) + mac_addr[1]);
	GM_OUT(GM_MAC_ADDR_ALT0, 0);
	GM_OUT(GM_MAC_ADDR_ALT1, 0);
	GM_OUT(GM_MAC_ADDR_ALT2, 0);
	GM_OUT(GM_MAC_ADDR_CTRL0, 0x0001);
	GM_OUT(GM_MAC_ADDR_CTRL1, 0xc200);
	GM_OUT(GM_MAC_ADDR_CTRL2, 0x0180);
	GM_OUT(GM_MAC_ADDR_FILTER0, 0);
	GM_OUT(GM_MAC_ADDR_FILTER1, 0);
	GM_OUT(GM_MAC_ADDR_FILTER2, 0);
	GM_OUT(GM_MAC_ADDR_FILTER_MASK1_2, 0);
	GM_OUT(GM_MAC_ADDR_FILTER_MASK0, 0);
	for (i = 0; i < 27; ++i)
		GM_OUT(GM_MAC_ADDR_FILTER_HASH0 + i, 0);
	
	/* Clear stat counters */
	GM_OUT(GM_MAC_COLLISION_CTR, 0);
	GM_OUT(GM_MAC_FIRST_COLLISION_CTR, 0);
	GM_OUT(GM_MAC_EXCS_COLLISION_CTR, 0);
	GM_OUT(GM_MAC_LATE_COLLISION_CTR, 0);
	GM_OUT(GM_MAC_DEFER_TIMER_COUNTER, 0);
	GM_OUT(GM_MAC_PEAK_ATTEMPTS, 0);
	GM_OUT(GM_MAC_RX_FRAME_CTR, 0);
	GM_OUT(GM_MAC_RX_LEN_ERR_CTR, 0);
	GM_OUT(GM_MAC_RX_ALIGN_ERR_CTR, 0);
	GM_OUT(GM_MAC_RX_CRC_ERR_CTR, 0);
	GM_OUT(GM_MAC_RX_CODE_VIOLATION_CTR, 0);
	
	/* default to half duplex */
	GM_OUT(GM_MAC_TX_CONFIG, 0);
	GM_OUT(GM_MAC_RX_CONFIG, 0);
	gmac_set_duplex_mode(gm, gm->full_duplex);
	
	/* Setup pause thresholds */
	fifo_size = GM_IN(GM_RX_FIFO_SIZE);
	GM_OUT(GM_RX_PTH,
		((fifo_size - ((GM_MAC_MAX_FRAME_SIZE_ALIGN + 8) * 2 / GM_RX_PTH_UNITS))
			<< GM_RX_PTH_OFF_SHIFT) |
		((fifo_size - ((GM_MAC_MAX_FRAME_SIZE_ALIGN + 8) * 3 / GM_RX_PTH_UNITS))
			<< GM_RX_PTH_ON_SHIFT));
		
	/* Setup interrupt blanking */
	if (GM_IN(GM_BIF_CFG) & GM_BIF_CFG_M66EN)
		GM_OUT(GM_RX_BLANK, (5 << GM_RX_BLANK_INTR_PACKETS_SHIFT)
			| (8 << GM_RX_BLANK_INTR_TIME_SHIFT));
	else
		GM_OUT(GM_RX_BLANK, (5 << GM_RX_BLANK_INTR_PACKETS_SHIFT)
			| (4 << GM_RX_BLANK_INTR_TIME_SHIFT));	
}

/*
 * Fill the Rx and Tx rings with good initial values, alloc
 * fresh Rx skb's.
 */
static void
gmac_init_rings(struct gmac *gm, int from_irq)
{
	int i;
	struct sk_buff *skb;
	unsigned char *data;
	struct gmac_dma_desc *ring;
	int gfp_flags = GFP_KERNEL;

	if (from_irq || in_interrupt())
		gfp_flags = GFP_ATOMIC;

	/* init rx ring */
	ring = (struct gmac_dma_desc *) gm->rxring;
	memset(ring, 0, NRX * sizeof(struct gmac_dma_desc));
	for (i = 0; i < NRX; ++i, ++ring) {
		data = dummy_buf;
		gm->rx_buff[i] = skb = gmac_alloc_skb(RX_BUF_ALLOC_SIZE, gfp_flags);
		if (skb != 0) {
			skb->dev = gm->dev;
			skb_put(skb, ETH_FRAME_LEN + RX_OFFSET);
			skb_reserve(skb, RX_OFFSET);
			data = skb->data - RX_OFFSET;
		}
		st_le32(&ring->lo_addr, virt_to_bus(data));
		st_le32(&ring->size, RX_SZ_OWN | ((RX_BUF_ALLOC_SIZE-RX_OFFSET) << RX_SZ_SHIFT));
	}

	/* init tx ring */
	ring = (struct gmac_dma_desc *) gm->txring;
	memset(ring, 0, NTX * sizeof(struct gmac_dma_desc));

	gm->next_rx = 0;
	gm->next_tx = 0;
	gm->tx_gone = 0;

	/* set pointers in chip */
	mb();
	GM_OUT(GM_RX_DESC_HI, 0);
	GM_OUT(GM_RX_DESC_LO, virt_to_bus(gm->rxring));
	GM_OUT(GM_TX_DESC_HI, 0);
	GM_OUT(GM_TX_DESC_LO, virt_to_bus(gm->txring));
}

/*
 * Start the Tx and Rx DMA engines and enable interrupts
 * 
 * Note: The various mdelay(20); come from Darwin implentation. Some
 * tests (doc ?) are needed to replace those with something more intrusive.
 */
static void
gmac_start_dma(struct gmac *gm)
{
	/* Enable Tx and Rx */
	GM_BIS(GM_TX_CONF, GM_TX_CONF_DMA_EN);
	mdelay(20);
	GM_BIS(GM_RX_CONF, GM_RX_CONF_DMA_EN);
	mdelay(20);
	GM_BIS(GM_MAC_RX_CONFIG, GM_MAC_RX_CONF_ENABLE);
	mdelay(20);
	GM_BIS(GM_MAC_TX_CONFIG, GM_MAC_TX_CONF_ENABLE);
	mdelay(20);
	/* Kick the receiver and enable interrupts */
	GM_OUT(GM_RX_KICK, NRX);
	GM_BIC(GM_IRQ_MASK, 	GM_IRQ_TX_INT_ME |
				GM_IRQ_TX_ALL |
				GM_IRQ_RX_DONE |
				GM_IRQ_RX_TAG_ERR |
				GM_IRQ_MAC_RX |
				GM_IRQ_MIF |
				GM_IRQ_BUS_ERROR);
}

/*
 * Stop the Tx and Rx DMA engines after disabling interrupts
 * 
 * Note: The various mdelay(20); come from Darwin implentation. Some
 * tests (doc ?) are needed to replace those with something more intrusive.
 */
static void
gmac_stop_dma(struct gmac *gm)
{
	/* disable interrupts */
	GM_OUT(GM_IRQ_MASK, 0xffffffff);
	/* Enable Tx and Rx */
	GM_BIC(GM_TX_CONF, GM_TX_CONF_DMA_EN);
	mdelay(20);
	GM_BIC(GM_RX_CONF, GM_RX_CONF_DMA_EN);
	mdelay(20);
	GM_BIC(GM_MAC_RX_CONFIG, GM_MAC_RX_CONF_ENABLE);
	mdelay(20);
	GM_BIC(GM_MAC_TX_CONFIG, GM_MAC_TX_CONF_ENABLE);
	mdelay(20);
}

/*
 * Configure promisc mode and setup multicast hash table
 * filter
 */
static void
gmac_set_multicast(struct net_device *dev)
{
	struct gmac *gm = (struct gmac *) dev->priv;
	struct dev_mc_list *dmi = dev->mc_list;
	int i,j,k,b;
	u32 crc;
	int multicast_hash = 0;
	int multicast_all = 0;
	int promisc = 0;

	if (gm->sleeping)
		return;

	/* Lock out others. */
	netif_stop_queue(dev);


	if (dev->flags & IFF_PROMISC)
		promisc = 1;
	else if ((dev->flags & IFF_ALLMULTI) /* || (dev->mc_count > XXX) */) {
		multicast_all = 1;
	} else {
		u16 hash_table[16];

		for(i = 0; i < 16; i++)
			hash_table[i] = 0;

	    	for (i = 0; i < dev->mc_count; i++) {
			crc = ether_crc_le(6, dmi->dmi_addr);
			j = crc >> 24;	/* bit number in multicast_filter */
			hash_table[j >> 4] |= 1 << (15 - (j & 0xf));
			dmi = dmi->next;
	    	}

	    	for (i = 0; i < 16; i++)
	    		GM_OUT(GM_MAC_ADDR_FILTER_HASH0 + (i*4), hash_table[i]);
		GM_BIS(GM_MAC_RX_CONFIG, GM_MAC_RX_CONF_HASH_ENABLE);
	    	multicast_hash = 1;
	}

	if (promisc)
		GM_BIS(GM_MAC_RX_CONFIG, GM_MAC_RX_CONF_RX_ALL);
	else
		GM_BIC(GM_MAC_RX_CONFIG, GM_MAC_RX_CONF_RX_ALL);

	if (multicast_hash)
		GM_BIS(GM_MAC_RX_CONFIG, GM_MAC_RX_CONF_HASH_ENABLE);
	else
		GM_BIC(GM_MAC_RX_CONFIG, GM_MAC_RX_CONF_HASH_ENABLE);

	if (multicast_all)
		GM_BIS(GM_MAC_RX_CONFIG, GM_MAC_RX_CONF_RX_ALL_MULTI);
	else
		GM_BIC(GM_MAC_RX_CONFIG, GM_MAC_RX_CONF_RX_ALL_MULTI);
	
	/* Let us get going again. */
	netif_wake_queue(dev);
}

/*
 * Open the interface
 */
static int
gmac_open(struct net_device *dev)
{
	int ret;
	struct gmac *gm = (struct gmac *) dev->priv;

	/* Power up and reset chip */
	if (gmac_powerup_and_reset(dev))
		return -EIO;

	/* Get our interrupt */
	ret = request_irq(dev->irq, gmac_interrupt, 0, dev->name, dev);
	if (ret) {
		printk(KERN_ERR "%s can't get irq %d\n", dev->name, dev->irq);
		return ret;
	}

	gm->full_duplex = 0;
	gm->phy_status = 0;
	
	/* Find a PHY */
	if (!mii_lookup_and_reset(gm))
		printk(KERN_WARNING "%s WARNING ! Can't find PHY\n", dev->name);

	/* Configure the PHY */
	mii_setup_phy(gm);
	
	/* Initialize the descriptor rings */
	gmac_init_rings(gm, 0);

	/* Initialize the MAC */
	gmac_mac_init(gm, dev->dev_addr);
	
	/* Initialize the multicast tables & promisc mode if any */
	gmac_set_multicast(dev);

	/* Initialize the carrier status */
	netif_carrier_off(dev);

	/*
	 * Check out PHY status and start auto-poll
	 * 
	 * Note: do this before enabling interrutps
	 */
	mii_interrupt(gm);

	/* Start the chip */
	gmac_start_dma(gm);

	gm->opened = 1;

	return 0;
}

/* 
 * Close the interface
 */
static int
gmac_close(struct net_device *dev)
{
	struct gmac *gm = (struct gmac *) dev->priv;
	int i;

	gm->opened = 0;

	/* Stop chip and interrupts */
	gmac_stop_dma(gm);

	/* Stop polling PHY */
	mii_poll_stop(gm);

	/* Free interrupt */
	free_irq(dev->irq, dev);
	
	/* Shut down chip */
	gmac_set_power(gm, 0);
	gm->phy_type = 0;

	/* Empty rings of any remaining gremlins */
	for (i = 0; i < NRX; ++i) {
		if (gm->rx_buff[i] != 0) {
			dev_kfree_skb(gm->rx_buff[i]);
			gm->rx_buff[i] = 0;
		}
	}
	for (i = 0; i < NTX; ++i) {
		if (gm->tx_buff[i] != 0) {
			dev_kfree_skb(gm->tx_buff[i]);
			gm->tx_buff[i] = 0;
		}
	}

	return 0;
}

#ifdef CONFIG_PMAC_PBOOK
int
gmac_sleep_notify(struct pmu_sleep_notifier *self, int when)
{
	struct gmac *gm;
	
	/* XXX should handle more than one */
	if (gmacs == NULL)
		return PBOOK_SLEEP_OK;

	gm = (struct gmac *) gmacs->priv;
	if (!gm->opened)
		return PBOOK_SLEEP_OK;
		
	switch (when) {
	case PBOOK_SLEEP_REQUEST:
		break;
	case PBOOK_SLEEP_REJECT:
		break;
	case PBOOK_SLEEP_NOW:
		gmac_suspend(gm);
		break;
	case PBOOK_WAKE:
		gmac_resume(gm);
		break;
	}
	return PBOOK_SLEEP_OK;
}
#endif /* CONFIG_PMAC_PBOOK */

/*
 * Handle a transmit timeout
 */
static void
gmac_tx_timeout(struct net_device *dev)
{
	struct gmac *gm = (struct gmac *) dev->priv;
	int i, timeout;
	unsigned long flags;

	if (gm->sleeping)
		return;

	printk (KERN_ERR "%s: transmit timed out, resetting\n", dev->name);

	spin_lock_irqsave(&gm->lock, flags);

	/* Stop chip */
	gmac_stop_dma(gm);
	/* Empty Tx ring of any remaining gremlins */
	gmac_tx_cleanup(dev, 1);
	/* Empty Rx ring of any remaining gremlins */
	for (i = 0; i < NRX; ++i) {
		if (gm->rx_buff[i] != 0) {
			dev_kfree_skb_irq(gm->rx_buff[i]);
			gm->rx_buff[i] = 0;
		}
	}
	/* Perform a software reset */
	GM_OUT(GM_RESET, GM_RESET_TX | GM_RESET_RX);
	for (timeout = 100; timeout > 0; --timeout) {
		mdelay(10);
		if ((GM_IN(GM_RESET) & (GM_RESET_TX | GM_RESET_RX)) == 0) {
			/* Mask out all chips interrupts */
			GM_OUT(GM_IRQ_MASK, 0xffffffff);
			GM_OUT(GM_MAC_TX_RESET, GM_MAC_TX_RESET_NOW);
			GM_OUT(GM_MAC_RX_RESET, GM_MAC_RX_RESET_NOW);
			break;
		}
	}
	if (!timeout)
		printk(KERN_ERR "%s reset chip failed !\n", dev->name);
	/* Create fresh rings */
	gmac_init_rings(gm, 1);
	/* re-initialize the MAC */
	gmac_mac_init(gm, dev->dev_addr);	
	/* re-initialize the multicast tables & promisc mode if any */
	gmac_set_multicast(dev);
	/* Restart PHY auto-poll */
	mii_interrupt(gm);
	/* Restart chip */
	gmac_start_dma(gm);
	
	spin_unlock_irqrestore(&gm->lock, flags);

	netif_wake_queue(dev);
}

/*
 * Add a packet to the transmit ring
 */
static int
gmac_xmit_start(struct sk_buff *skb, struct net_device *dev)
{
	struct gmac *gm = (struct gmac *) dev->priv;
	volatile struct gmac_dma_desc *dp;
	unsigned long flags;
	int i;

	if (gm->sleeping)
		return 1;
		
	spin_lock_irqsave(&gm->lock, flags);

	i = gm->next_tx;
	if (gm->tx_buff[i] != 0) {
		/* 
		 * Buffer is full, can't send this packet at the moment
		 * 
		 * Can this ever happen in 2.4 ?
		 */
		netif_stop_queue(dev);
		spin_unlock_irqrestore(&gm->lock, flags);
		return 1;
	}
	gm->next_tx = (i + 1) & (NTX - 1);
	gm->tx_buff[i] = skb;
	
	dp = &gm->txring[i];
	/* FIXME: Interrupt on all packet for now, change this to every N packet,
	 * with N to be adjusted
	 */
	dp->flags = TX_FL_INTERRUPT;
	dp->hi_addr = 0;
	st_le32(&dp->lo_addr, virt_to_bus(skb->data));
	mb();
	st_le32(&dp->size, TX_SZ_SOP | TX_SZ_EOP | skb->len);
	mb();

	GM_OUT(GM_TX_KICK, gm->next_tx);

	if (gm->tx_buff[gm->next_tx] != 0)
		netif_stop_queue(dev);

	spin_unlock_irqrestore(&gm->lock, flags);

	dev->trans_start = jiffies;

	return 0;
}

/*
 * Handle servicing of the transmit ring by deallocating used
 * Tx packets and restoring flow control when necessary
 */
static void
gmac_tx_cleanup(struct net_device *dev, int force_cleanup)
{
	struct gmac *gm = (struct gmac *) dev->priv;
	volatile struct gmac_dma_desc *dp;
	struct sk_buff *skb;
	int gone, i;

	i = gm->tx_gone;

	/* Note: If i==gone, we empty the entire ring. This works because
	 * if the ring was empty, we wouldn't have received the interrupt
	 */
	do {
		gone = GM_IN(GM_TX_COMP);
		skb = gm->tx_buff[i];
		if (skb == NULL)
			break;
		dp = &gm->txring[i];
		if (force_cleanup)
			++gm->stats.tx_errors;
		else {
			++gm->stats.tx_packets;
			gm->stats.tx_bytes += skb->len;
		}
		gm->tx_buff[i] = NULL;
		dev_kfree_skb_irq(skb);
		if (++i >= NTX)
			i = 0;
	} while (force_cleanup || i != gone);
	gm->tx_gone = i;

	if (!force_cleanup && netif_queue_stopped(dev) &&
	    (gm->tx_buff[gm->next_tx] == 0))
		netif_wake_queue(dev);
}

/*
 * Handle servicing of receive ring
 */
static void
gmac_receive(struct net_device *dev)
{
	struct gmac *gm = (struct gmac *) dev->priv;
	int i = gm->next_rx;
	volatile struct gmac_dma_desc *dp;
	struct sk_buff *skb, *new_skb;
	int len, flags, drop, last;
	unsigned char *data;
	u16 csum;

	last = -1;
	for (;;) {
		dp = &gm->rxring[i];
		/* Buffer not yet filled, no more Rx buffers to handle */
		if (ld_le32(&dp->size) & RX_SZ_OWN)
			break;
		/* Get packet length, flags, etc... */
		len = (ld_le32(&dp->size) >> 16) & 0x7fff;
		flags = ld_le32(&dp->flags);
		skb = gm->rx_buff[i];
		drop = 0;
		new_skb = NULL;
		csum = ld_le32(&dp->size) & RX_SZ_CKSUM_MASK;
		
		/* Handle errors */
		if ((len < ETH_ZLEN)||(flags & RX_FL_CRC_ERROR)||(!skb)) {
			++gm->stats.rx_errors;
			if (len < ETH_ZLEN)
				++gm->stats.rx_length_errors;
			if (flags & RX_FL_CRC_ERROR)
				++gm->stats.rx_crc_errors;
			if (!skb) {
				++gm->stats.rx_dropped;
				skb = gmac_alloc_skb(RX_BUF_ALLOC_SIZE, GFP_ATOMIC);
				if (skb) {
					gm->rx_buff[i] = skb;
			    		skb->dev = dev;
			    		skb_put(skb, ETH_FRAME_LEN + RX_OFFSET);
			    		skb_reserve(skb, RX_OFFSET);
				}
			}
			drop = 1;
		} else {
			/* Large packet, alloc a new skb for the ring */
			if (len > RX_COPY_THRESHOLD) {
			    new_skb = gmac_alloc_skb(RX_BUF_ALLOC_SIZE, GFP_ATOMIC);
			    if(!new_skb) {
			    	printk(KERN_INFO "%s: Out of SKBs in Rx, packet dropped !\n",
			    		dev->name);
			    	drop = 1;
			    	++gm->stats.rx_dropped;
			    	goto finish;
			    }

			    gm->rx_buff[i] = new_skb;
			    new_skb->dev = dev;
			    skb_put(new_skb, ETH_FRAME_LEN + RX_OFFSET);
			    skb_reserve(new_skb, RX_OFFSET);
			    skb_trim(skb, len);
			} else {
			    /* Small packet, copy it to a new small skb */
			    struct sk_buff *copy_skb = dev_alloc_skb(len + RX_OFFSET);

			    if(!copy_skb) {
				printk(KERN_INFO "%s: Out of SKBs in Rx, packet dropped !\n",
					dev->name);
				drop = 1;
				++gm->stats.rx_dropped;
			    	goto finish;
			    }

			    copy_skb->dev = dev;
			    skb_reserve(copy_skb, RX_OFFSET);
			    skb_put(copy_skb, len);
			    memcpy(copy_skb->data, skb->data, len);

			    new_skb = skb;
			    skb = copy_skb;
			}
		}
	finish:
		/* Need to drop packet ? */
		if (drop) {
			new_skb = skb;
			skb = NULL;
		}
		
		/* Put back ring entry */
		data = new_skb ? (new_skb->data - RX_OFFSET) : dummy_buf;
		dp->hi_addr = 0;
		st_le32(&dp->lo_addr, virt_to_bus(data));
		mb();
		st_le32(&dp->size, RX_SZ_OWN | ((RX_BUF_ALLOC_SIZE-RX_OFFSET) << RX_SZ_SHIFT));
		
		/* Got Rx packet ? */
		if (skb) {
			/* Yes, baby, keep that hot ;) */
			if(!(csum ^ 0xffff))
				skb->ip_summed = CHECKSUM_UNNECESSARY;
			else
				skb->ip_summed = CHECKSUM_NONE;
			skb->ip_summed = CHECKSUM_NONE;
			skb->protocol = eth_type_trans(skb, dev);
			gm->stats.rx_bytes += skb->len;
			netif_rx(skb);
			dev->last_rx = jiffies;
			++gm->stats.rx_packets;
		}
		
		last = i;
		if (++i >= NRX)
			i = 0;
	}
	gm->next_rx = i;
	if (last >= 0) {
		mb();
		GM_OUT(GM_RX_KICK, last & 0xfffffffc);
	}
}

/*
 * Service chip interrupts
 */
static void
gmac_interrupt(int irq, void *dev_id, struct pt_regs *regs)
{
	struct net_device *dev = (struct net_device *) dev_id;
	struct gmac *gm = (struct gmac *) dev->priv;
	unsigned int status;

	status = GM_IN(GM_IRQ_STATUS);
	if (status & (GM_IRQ_BUS_ERROR | GM_IRQ_MIF))
		GM_OUT(GM_IRQ_ACK, status & (GM_IRQ_BUS_ERROR | GM_IRQ_MIF));
	
	if (status & (GM_IRQ_RX_TAG_ERR | GM_IRQ_BUS_ERROR)) {
		printk(KERN_ERR "%s: IRQ Error status: 0x%08x\n",
			dev->name, status);
	}
	
	if (status & GM_IRQ_MIF) {
		spin_lock(&gm->lock);
		mii_interrupt(gm);
		spin_unlock(&gm->lock);
	}
	
	if (status & GM_IRQ_RX_DONE) {
		spin_lock(&gm->lock);
		gmac_receive(dev);
		spin_unlock(&gm->lock);
	}
		
	if (status & (GM_IRQ_TX_INT_ME | GM_IRQ_TX_ALL)) {
		spin_lock(&gm->lock);
		gmac_tx_cleanup(dev, 0);
		spin_unlock(&gm->lock);
	}
}

/*
 * Retreive some error stats from chip and return them
 * to above layer
 */
static struct net_device_stats *
gmac_stats(struct net_device *dev)
{
	struct gmac *gm = (struct gmac *) dev->priv;
	struct net_device_stats *stats = &gm->stats;

	if (gm && gm->opened && !gm->sleeping) {
		stats->rx_crc_errors += GM_IN(GM_MAC_RX_CRC_ERR_CTR);
		GM_OUT(GM_MAC_RX_CRC_ERR_CTR, 0);

		stats->rx_frame_errors += GM_IN(GM_MAC_RX_ALIGN_ERR_CTR);
		GM_OUT(GM_MAC_RX_ALIGN_ERR_CTR, 0);

		stats->rx_length_errors += GM_IN(GM_MAC_RX_LEN_ERR_CTR);
		GM_OUT(GM_MAC_RX_LEN_ERR_CTR, 0);

		stats->tx_aborted_errors += GM_IN(GM_MAC_EXCS_COLLISION_CTR);

		stats->collisions +=
			(GM_IN(GM_MAC_EXCS_COLLISION_CTR) +
			 GM_IN(GM_MAC_LATE_COLLISION_CTR));
		GM_OUT(GM_MAC_EXCS_COLLISION_CTR, 0);
		GM_OUT(GM_MAC_LATE_COLLISION_CTR, 0);
	}

	return stats;
}

static int __init
gmac_probe(void)
{
	struct device_node *gmac;

	/* We bump use count during probe since get_free_page can sleep
	 * which can be a race condition if module is unloaded at this
	 * point.
	 */
	MOD_INC_USE_COUNT;
	
	/*
	 * We don't use PCI scanning on pmac since the GMAC cell is disabled
	 * by default, and thus absent from kernel original PCI probing.
	 */
	for (gmac = find_compatible_devices("network", "gmac"); gmac != 0;
	     gmac = gmac->next)
		gmac_probe1(gmac);

#ifdef CONFIG_PMAC_PBOOK
	if (gmacs)
		pmu_register_sleep_notifier(&gmac_sleep_notifier);
#endif

	MOD_DEC_USE_COUNT;

	return gmacs? 0: -ENODEV;
}

static void
gmac_probe1(struct device_node *gmac)
{
	struct gmac *gm;
	unsigned long tx_descpage, rx_descpage;
	unsigned char *addr;
	struct net_device *dev;
	int i;

	if (gmac->n_addrs < 1 || gmac->n_intrs < 1) {
		printk(KERN_ERR "can't use GMAC %s: %d addrs and %d intrs\n",
		       gmac->full_name, gmac->n_addrs, gmac->n_intrs);
		return;
	}

	addr = get_property(gmac, "local-mac-address", NULL);
	if (addr == NULL) {
		printk(KERN_ERR "Can't get mac-address for GMAC %s\n",
		       gmac->full_name);
		return;
	}

	if (dummy_buf == NULL) {
		dummy_buf = kmalloc(DUMMY_BUF_LEN, GFP_KERNEL);
		if (dummy_buf == NULL) {
			printk(KERN_ERR "GMAC: failed to allocated dummy buffer\n");
			return;
		}
	}

	tx_descpage = get_free_page(GFP_KERNEL);
	if (tx_descpage == 0) {
		printk(KERN_ERR "GMAC: can't get a page for tx descriptors\n");
		return;
	}
	rx_descpage = get_free_page(GFP_KERNEL);
	if (rx_descpage == 0) {
		printk(KERN_ERR "GMAC: can't get a page for rx descriptors\n");
		goto out_txdesc;
	}

	dev = init_etherdev(NULL, sizeof(struct gmac));
	if (!dev) {
		printk(KERN_ERR "GMAC: init_etherdev failed, out of memory\n");
		goto out_rxdesc;
	}
	SET_MODULE_OWNER(dev);

	gm = dev->priv;
	gm->of_node = gmac;
	if (!request_OF_resource(gmac, 0, " (gmac)")) {
		printk(KERN_ERR "GMAC: can't request IO resource !\n");
		gm->of_node = NULL;
		goto out_unreg;
	}
	dev->base_addr = gmac->addrs[0].address;
	gm->regs = (volatile unsigned int *)
		ioremap(gmac->addrs[0].address, 0x10000);
	if (!gm->regs) {
		printk(KERN_ERR "GMAC: unable to map I/O registers\n");
		goto out_unreg;
	}
	dev->irq = gmac->intrs[0].line;
	gm->dev = dev;

	spin_lock_init(&gm->lock);
	
	if (pci_device_from_OF_node(gmac, &gm->pci_bus, &gm->pci_devfn)) {
		gm->pci_bus = gm->pci_devfn = 0xff;
		printk(KERN_ERR "Can't locate GMAC PCI entry\n");
	}

	printk(KERN_INFO "%s: GMAC at", dev->name);
	for (i = 0; i < 6; ++i) {
		dev->dev_addr[i] = addr[i];
		printk("%c%.2x", (i? ':': ' '), addr[i]);
	}
	printk(", driver " GMAC_VERSION "\n");

	gm->tx_desc_page = tx_descpage;
	gm->rx_desc_page = rx_descpage;
	gm->rxring = (volatile struct gmac_dma_desc *) rx_descpage;
	gm->txring = (volatile struct gmac_dma_desc *) tx_descpage;

	gm->phy_addr = 0;
	gm->opened = 0;
	gm->sleeping = 0;

	dev->open = gmac_open;
	dev->stop = gmac_close;
	dev->hard_start_xmit = gmac_xmit_start;
	dev->get_stats = gmac_stats;
	dev->set_multicast_list = &gmac_set_multicast;
	dev->tx_timeout = &gmac_tx_timeout;
	dev->watchdog_timeo = 5*HZ;

	ether_setup(dev);

	gm->next_gmac = gmacs;
	gmacs = dev;
	return;

out_unreg:
	unregister_netdev(dev);
	if (gm->of_node)
		release_OF_resource(gm->of_node, 0);
	kfree(dev);
out_rxdesc:
	free_page(rx_descpage);
out_txdesc:
	free_page(tx_descpage);
}

MODULE_AUTHOR("Paul Mackerras/Ben Herrenschmidt");
MODULE_DESCRIPTION("PowerMac GMAC driver.");
MODULE_LICENSE("GPL");
EXPORT_NO_SYMBOLS;

static void __exit gmac_cleanup_module(void)
{
	struct gmac *gm;
	struct net_device *dev;

#ifdef CONFIG_PMAC_PBOOK
	if (gmacs)
		pmu_unregister_sleep_notifier(&gmac_sleep_notifier);
#endif

	while ((dev = gmacs) != NULL) {
		gm = (struct gmac *) dev->priv;
		unregister_netdev(dev);
		iounmap((void *) gm->regs);
		free_page(gm->tx_desc_page);
		free_page(gm->rx_desc_page);
		release_OF_resource(gm->of_node, 0);
		gmacs = gm->next_gmac;
		kfree(dev);
	}
	if (dummy_buf != NULL) {
		kfree(dummy_buf);
		dummy_buf = NULL;
	}
}

module_init(gmac_probe);
module_exit(gmac_cleanup_module);
