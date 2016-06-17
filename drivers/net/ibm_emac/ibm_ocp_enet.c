/*
 * ibm_ocp_enet.c
 *
 * Ethernet driver for the built in ethernet on the IBM 4xx PowerPC
 * processors.
 * 
 * (c) 2003 Benjamin Herrenschmidt <benh@kernel.crashing.org>
 *
 * Based on original work by
 *
 *      Armin Kuster <akuster@mvista.com>
 * 	Johnnie Peters <jpeters@mvista.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR   IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT,  INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * TODO
 *       - Check for races in the "remove" code path
 *       - Add some Power Management to the MAC and the PHY
 *       - Audit remaining of non-rewritten code (--BenH)
 *       - Cleanup message display using msglevel mecanism
 *
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/ptrace.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/ethtool.h>
#include <linux/mii.h>

#include <asm/processor.h>
#include <asm/bitops.h>
#include <asm/io.h>
#include <asm/dma.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/ocp.h>

#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/crc32.h>

#include "ibm_ocp_enet.h"

//#define MDIO_DEBUG(fmt) printk fmt
#define MDIO_DEBUG(fmt)

//#define LINK_DEBUG(fmt) printk fmt
#define LINK_DEBUG(fmt)

//#define PKT_DEBUG(fmt) printk fmt
#define PKT_DEBUG(fmt)

#define DRV_NAME        "emac"
#define DRV_VERSION     "2.0"
#define DRV_AUTHOR      "Benjamin Herrenschmidt <benh@kernel.crashing.org>"
#define DRV_DESC        "IBM OCP EMAC Ethernet driver"

MODULE_AUTHOR(DRV_AUTHOR);
MODULE_DESCRIPTION(DRV_DESC);
MODULE_LICENSE("GPL");

static int skb_res = SKB_RES;
MODULE_PARM(skb_res, "i");
MODULE_PARM_DESC(skb_res, "Amount of data to reserve on skb buffs\n"
		 "The 405 handles a misaligned IP header fine but\n"
		 "this can help if you are routing to a tunnel or a\n"
		 "device that needs aligned data. 0..2");

#define ZMII_PRIV(ocpdev) ((struct ibm_ocp_zmii*)ocp_get_drvdata(ocpdev))

static unsigned int zmii_enable[][4] = {
	{ZMII_SMII0, ZMII_RMII0, ZMII_MII0,
	 ~(ZMII_MDI1 | ZMII_MDI2 | ZMII_MDI3)},
	{ZMII_SMII1, ZMII_RMII1, ZMII_MII1,
	 ~(ZMII_MDI0 | ZMII_MDI2 | ZMII_MDI3)},
	{ZMII_SMII2, ZMII_RMII2, ZMII_MII2,
	 ~(ZMII_MDI0 | ZMII_MDI1 | ZMII_MDI3)},
	{ZMII_SMII3, ZMII_RMII3, ZMII_MII3, ~(ZMII_MDI0 | ZMII_MDI1 | ZMII_MDI2)}
};
static unsigned int mdi_enable[] =
    { ZMII_MDI0, ZMII_MDI1, ZMII_MDI2, ZMII_MDI3 };

static unsigned int zmii_speed = 0x0;
static unsigned int zmii_speed100[] = { ZMII_MII0_100MB, ZMII_MII1_100MB };

/* Since multiple EMACs share MDIO lines in various ways, we need
 * to avoid re-using the same PHY ID in cases where the arch didn't
 * setup precise emac_phy_map entries
 */
static u32 busy_phy_map = 0;

static struct net_device_stats *
emac_stats(struct net_device *dev)
{
	struct ocp_enet_private *fep = dev->priv;
	return &fep->stats;
}

static int
emac_init_zmii(struct ocp_device *ocpdev, int mode)
{
	struct ibm_ocp_zmii *zmii = ZMII_PRIV(ocpdev);
	struct zmii_regs *zmiip;
	const char *mode_name[] = { "SMII", "RMII", "MII" };
	
	if (zmii){
		/* We have already initialized ZMII device,
		   so just increment refcount and return */	
		zmii->users++;
		return 0;		   
	}
	
	zmii = kmalloc(sizeof(struct ibm_ocp_zmii), GFP_KERNEL);
	if (zmii == NULL) {
		printk(KERN_ERR "zmii%d: Out of memory allocating ZMII structure!\n",
			ocpdev->def->index);
		return -ENOMEM;
	}
	memset(zmii, 0, sizeof(*zmii));
		
	zmiip = (struct zmii_regs *)ioremap(ocpdev->def->paddr, sizeof(*zmiip));
	if (zmiip == NULL){
		printk(KERN_ERR "zmii%d: Cannot ioremap bridge registers!\n",
			ocpdev->def->index);
		
		kfree(zmii);
		return -ENOMEM;	
	}	
	
	if (mode == ZMII_AUTO) {
		if (zmiip->fer & (ZMII_MII0 | ZMII_MII1 | 
				  ZMII_MII2 | ZMII_MII3))
			mode = MII;
		if (zmiip->fer & (ZMII_RMII0 | ZMII_RMII1 |
				  ZMII_RMII2 | ZMII_RMII3))
			mode = RMII;
		if (zmiip->fer & (ZMII_SMII0 | ZMII_SMII1 |
				  ZMII_SMII2 | ZMII_SMII3))
			mode = SMII;

		/* Failsafe: ZMII_AUTO is invalid index into the arrays,
		   so force SMII if all else fails. */

		if (mode == ZMII_AUTO)
			mode = SMII;
	}
	
	zmii->base = zmiip;
	zmii->mode = mode;
	zmii->users++;
	ocp_set_drvdata(ocpdev, zmii);

	printk(KERN_NOTICE "zmii%d: bridge in %s mode\n", ocpdev->def->index,
		mode_name[mode]);
	return 0;
}

static void
emac_enable_zmii_port(struct ocp_device *ocpdev, int input)
{
	u32 mask;
	struct ibm_ocp_zmii *zmii = ZMII_PRIV(ocpdev);

	mask  = in_be32(&zmii->base->fer);
	mask &= zmii_enable[input][MDI];	/* turn all non enabled MDI's off */
	mask |= zmii_enable[input][zmii->mode] | mdi_enable[input];
	out_be32(&zmii->base->fer, mask);
}

static void
emac_zmii_port_speed(struct ocp_device *ocpdev, int input, int speed)
{
	struct ibm_ocp_zmii *zmii = ZMII_PRIV(ocpdev);

	if (speed == 100)
		zmii_speed |= zmii_speed100[input];
	else
		zmii_speed &= ~zmii_speed100[input];

	out_be32(&zmii->base->ssr, zmii_speed);
}

static void
emac_fini_zmii(struct ocp_device *ocpdev)
{
	struct ibm_ocp_zmii *zmii = ZMII_PRIV(ocpdev);
	BUG_ON(!zmii || zmii->users == 0);

	if (!--zmii->users){
		ocp_set_drvdata(ocpdev, NULL);
		iounmap((void*)zmii->base);
		kfree(zmii);
	}
}

int
emac_phy_read(struct net_device *dev, int mii_id, int reg)
{
	register int i;
	uint32_t stacr;
	struct ocp_enet_private *fep = dev->priv;
	volatile emac_t *emacp = fep->emacp;

	MDIO_DEBUG(("%s: phy_read, id: 0x%x, reg: 0x%x\n", dev->name, mii_id, reg));

	/* Enable proper ZMII port */
	if (fep->zmii_dev)
		emac_enable_zmii_port(fep->zmii_dev, fep->zmii_input);
	/* Use the EMAC that has the MDIO port */
	if (fep->mdio_dev) {
		dev = fep->mdio_dev;
		fep = dev->priv;
	}
	
	/* Wait for data transfer complete bit */
	for (i = 0; i < OCP_RESET_DELAY; ++i) {
		if (emacp->em0stacr & EMAC_STACR_OC)
			break;
		udelay(MDIO_DELAY);	/* changed to 2 with new scheme -armin */
	}
	if ((emacp->em0stacr & EMAC_STACR_OC) == 0) {
		printk(KERN_WARNING "%s: PHY read timeout #1!\n", dev->name);
		return -1;
	}

	/* Clear the speed bits and make a read request to the PHY */
	stacr = ((EMAC_STACR_READ | (reg & 0x1f)) & ~EMAC_STACR_CLK_100MHZ);
	stacr |= ((mii_id & 0x1F) << 5);

	out_be32(&emacp->em0stacr, stacr);
	stacr = in_be32(&emacp->em0stacr);
	/* Wait for data transfer complete bit */
	for (i = 0; i < OCP_RESET_DELAY; ++i) {
		if ((stacr = in_be32(&emacp->em0stacr)) & EMAC_STACR_OC)
			break;
		udelay(MDIO_DELAY);
	}
	if ((stacr & EMAC_STACR_OC) == 0) {
		printk(KERN_WARNING "%s: PHY read timeout #2!\n", dev->name);
		return -1;
	}

	/* Check for a read error */
	if (stacr & EMAC_STACR_PHYE) {
	        MDIO_DEBUG(("OCP MDIO PHY error !\n"));
		return -1;
	}
   
	MDIO_DEBUG((" -> 0x%x\n", stacr >> 16));

	return (stacr >> 16);
}


void
emac_phy_write(struct net_device *dev, int mii_id, int reg, int data)
{
	register int i = 0;
	uint32_t stacr;
	struct ocp_enet_private *fep = dev->priv;
	volatile emac_t *emacp = fep->emacp;

	MDIO_DEBUG(("%s phy_write, id: 0x%x, reg: 0x%x, data: 0x%x\n",
            dev->name, mii_id, reg, data));

	/* Enable proper ZMII port */
	if (fep->zmii_dev)
		emac_enable_zmii_port(fep->zmii_dev, fep->zmii_input);
	/* Use the EMAC that has the MDIO port */
	if (fep->mdio_dev) {
		dev = fep->mdio_dev;
		fep = dev->priv;
	}

	/* Wait for data transfer complete bit */
	for (i = 0; i < OCP_RESET_DELAY; ++i) {
		if (emacp->em0stacr & EMAC_STACR_OC)
			break;
		udelay(MDIO_DELAY);	/* changed to 2 with new scheme -armin */
	}
	if ((emacp->em0stacr & EMAC_STACR_OC) == 0) {
		printk(KERN_WARNING "%s: PHY write timeout #2!\n", dev->name);
		return;
	}

	/* Clear the speed bits and make a read request to the PHY */

	stacr = ((EMAC_STACR_WRITE | (reg & 0x1f)) & ~EMAC_STACR_CLK_100MHZ);
	stacr |= ((mii_id & 0x1f) << 5) | ((data & 0xffff) << 16);

	out_be32(&emacp->em0stacr, stacr);

	/* Wait for data transfer complete bit */
	for (i = 0; i < OCP_RESET_DELAY; ++i) {
		if ((stacr = emacp->em0stacr) & EMAC_STACR_OC)
			break;
		udelay(MDIO_DELAY);
	}
	if ((emacp->em0stacr & EMAC_STACR_OC) == 0)
		printk(KERN_WARNING "%s: PHY write timeout #2!\n", dev->name);

	/* Check for a write error */
	if ((stacr & EMAC_STACR_PHYE) != 0) {
		MDIO_DEBUG(("OCP MDIO PHY error !\n"));
        }
}

static void
emac_wakeup_irq(int irq, void *param, struct pt_regs *regs)
{
	struct net_device *dev = param;

	/* On Linux the 405 ethernet will always be active if configured
	 * in.  This interrupt should never occur.
	 */
	printk(KERN_INFO "%s: WakeUp interrupt !\n", dev->name);
}

static void
emac_txeob_dev(void *param, u32 chanmask)
{
	struct net_device *dev = param;
	struct ocp_enet_private *fep = dev->priv;
        unsigned long flags;

        spin_lock_irqsave(&fep->lock, flags);

	PKT_DEBUG(("emac_txeob_dev() entry, tx_cnt: %d\n", fep->tx_cnt));

	while (fep->tx_cnt &&
	       !(fep->tx_desc[fep->ack_slot].ctrl & MAL_TX_CTRL_READY)) {

		/* Tell the system the transmit completed. */
		dev_kfree_skb_irq(fep->tx_skb[fep->ack_slot]);

		if (fep->tx_desc[fep->ack_slot].ctrl &
		    (EMAC_TX_ST_EC | EMAC_TX_ST_MC | EMAC_TX_ST_SC))
			fep->stats.collisions++;

		fep->tx_skb[fep->ack_slot] = (struct sk_buff *) NULL;
		if (++fep->ack_slot == NUM_TX_BUFF)
			fep->ack_slot = 0;

		fep->tx_cnt--;
	}
	if (fep->tx_cnt < NUM_TX_BUFF)
       		netif_wake_queue(dev);

	PKT_DEBUG(("emac_txeob_dev() exit, tx_cnt: %d\n", fep->tx_cnt));
	
        spin_unlock_irqrestore(&fep->lock, flags);
}

/*
  Fill/Re-fill the rx chain with valid ctrl/ptrs.
  This function will fill from rx_slot up to the parm end.
  So to completely fill the chain pre-set rx_slot to 0 and
  pass in an end of 0.
 */
static void
emac_rx_fill(struct net_device *dev, int end)
{
	int i;
	struct ocp_enet_private *fep = dev->priv;
	unsigned char *ptr;

	i = fep->rx_slot;
	do {
		if (fep->rx_skb[i] != NULL) {
			/*We will trust the skb is still in a good state */
			ptr = (char *) virt_to_phys(fep->rx_skb[i]->data);
		} else {

			/* We don't want the 16 bytes skb_reserve done by dev_alloc_skb,
			 * it breaks our cache line alignement. However, we still allocate
			 * +16 so that we end up allocating the exact same size as
			 * dev_alloc_skb() would do.
			 * Also, because of the skb_res, the max DMA size we give to EMAC
			 * is slighly wrong, causing it to potentially DMA 2 more bytes
                         * from a broken/oversized packet. These 16 bytes will take care
                         * that we don't walk on somebody else toes with that.
			 */
			fep->rx_skb[i] =
			    alloc_skb(DESC_RX_BUF_SIZE + 16, GFP_ATOMIC);

			if (fep->rx_skb[i] == NULL) {
				/* Keep rx_slot here, the next time clean/fill is called
				 * we will try again before the MAL wraps back here
				 * If the MAL tries to use this descriptor with
				 * the EMPTY bit off it will cause the
				 * rxde interrupt.  That is where we will
				 * try again to allocate an sk_buff.
				 */
				break;

			}

			if (skb_res)
				skb_reserve(fep->rx_skb[i], skb_res);

			/* We must NOT consistent_sync the cache line right after the
			 * buffer, so we must crop our sync size to account for the
			 * reserved space
			 */
			consistent_sync((void *) fep->rx_skb[i]->
					data, (DESC_RX_BUF_SIZE-skb_res),
					PCI_DMA_FROMDEVICE);
			ptr = (char *) virt_to_phys(fep->rx_skb[i]->data);
		}
		fep->rx_desc[i].ctrl = MAL_RX_CTRL_EMPTY | MAL_RX_CTRL_INTR |	/*could be smarter about this to avoid ints at high loads */
		    (i == (NUM_RX_BUFF - 1) ? MAL_RX_CTRL_WRAP : 0);

		fep->rx_desc[i].data_ptr = ptr;
		/*
		   * 440GP uses the previously reserved bits in the
		   * data_len to encode the upper 4-bits of the buffer
		   * physical address (ERPN). Initialize these.
		 */
		fep->rx_desc[i].data_len = 0;
	} while ((i = (i + 1) % NUM_RX_BUFF) != end);

	fep->rx_slot = i;
}

static void 
emac_rx_clean(struct net_device *dev, int call_rx_fill)
{
	int i;
	int error, frame_length;
	struct ocp_enet_private *fep = dev->priv;
	unsigned short ctrl;
	int slots_walked = 0;

	i = fep->rx_slot;

	PKT_DEBUG(("emac_rx_clean() entry, call_rx_fill: %d, rx_slot: %d\n", call_rx_fill, fep->rx_slot));

	do {
		if (fep->rx_skb[i] == NULL)
			goto skip;	/*we have already handled the packet but haved failed to alloc */
		/* 
		   since rx_desc is in uncached mem we don't keep reading it directly 
		   we pull out a local copy of ctrl and do the checks on the copy.
		 */
		ctrl = fep->rx_desc[i].ctrl;
		if (ctrl & MAL_RX_CTRL_EMPTY)
			break;	/*we don't have any more ready packets */

		if (ctrl & EMAC_BAD_RX_PACKET) {

			fep->stats.rx_errors++;
			fep->stats.rx_dropped++;

			if (ctrl & EMAC_RX_ST_OE)
				fep->stats.rx_fifo_errors++;
			if (ctrl & EMAC_RX_ST_AE)
				fep->stats.rx_frame_errors++;
			if (ctrl & EMAC_RX_ST_BFCS)
				fep->stats.rx_crc_errors++;
			if (ctrl & (EMAC_RX_ST_RP | EMAC_RX_ST_PTL |
				    EMAC_RX_ST_ORE | EMAC_RX_ST_IRE))
				fep->stats.rx_length_errors++;
		} else {

			/* Send the skb up the chain. */
		frame_length = fep->rx_desc[i].data_len - 4;

			skb_put(fep->rx_skb[i], frame_length);
			fep->rx_skb[i]->dev = dev;
			fep->rx_skb[i]->protocol =
			    eth_type_trans(fep->rx_skb[i], dev);

			error = netif_rx(fep->rx_skb[i]);
			if ((error == NET_RX_DROP) || (error == NET_RX_BAD)) {
				fep->stats.rx_dropped++;
			} else {
				fep->stats.rx_packets++;
				fep->stats.rx_bytes += frame_length;
			}
			fep->rx_skb[i] = NULL;
		}
	      skip:
		slots_walked = 1;

	} while ((i = (i + 1) % NUM_RX_BUFF) != fep->rx_slot);

	PKT_DEBUG(("emac_rx_clean() exit, rx_slot: %d\n", fep->rx_slot));

	if (slots_walked && call_rx_fill)
           emac_rx_fill(dev, i);
}

static void
emac_rxeob_dev(void *param, u32 chanmask)
{
	struct net_device *dev = param;
	struct ocp_enet_private *fep = dev->priv;
        unsigned long flags;

	spin_lock_irqsave(&fep->lock, flags);
	emac_rx_clean(dev, 1);
	spin_unlock_irqrestore(&fep->lock, flags);
}

/*
 * This interrupt should never occurr, we don't program
 * the MAL for contiunous mode.
 */
static void
emac_txde_dev(void *param, u32 chanmask)
{
	struct net_device *dev = param;
	struct ocp_enet_private *fep = dev->priv;

	printk(KERN_WARNING "%s: transmit descriptor error\n", dev->name);

	emac_mac_dump(dev);
	emac_mal_dump(dev);

	/* Reenable the transmit channel */
	mal_enable_tx_channels(fep->mal, fep->commac.tx_chan_mask);
}

/*
 * This interrupt should be very rare at best.  This occurs when
 * the hardware has a problem with the receive descriptors.  The manual
 * states that it occurs when the hardware cannot the receive descriptor
 * empty bit is not set.  The recovery mechanism will be to
 * traverse through the descriptors, handle any that are marked to be
 * handled and reinitialize each along the way.  At that point the driver
 * will be restarted.
 */
static void
emac_rxde_dev(void *param, u32 chanmask)
{
 	struct net_device *dev = param;
     	struct ocp_enet_private *fep = dev->priv;
        unsigned long flags;

	printk(KERN_WARNING "%s: receive descriptor error\n", fep->ndev->name);

	emac_mac_dump(dev);
	emac_mal_dump(dev);
	emac_desc_dump(dev);

	/* Disable RX channel */
	spin_lock_irqsave(&fep->lock, flags);
       	mal_disable_rx_channels(fep->mal, fep->commac.rx_chan_mask);
       	
	/* For now, charge the error against all emacs */
	fep->stats.rx_errors++;

	/* so do we have any good packets still? */
	emac_rx_clean(dev,0);

	/* When the interface is restarted it resets processing to the
	 *  first descriptor in the table.
	 */

	fep->rx_slot = 0;
	emac_rx_fill(dev, 0);

	set_mal_dcrn(fep->mal, DCRN_MALRXEOBISR, fep->commac.rx_chan_mask);
	set_mal_dcrn(fep->mal, DCRN_MALRXDEIR, fep->commac.rx_chan_mask);

	/* Reenable the receive channels */
       	mal_enable_rx_channels(fep->mal, fep->commac.rx_chan_mask);
	spin_unlock_irqrestore(&fep->lock, flags);
}

static void
emac_mac_irq(int irq, void *dev_instance, struct pt_regs *regs)
{
	struct net_device *dev = dev_instance;
	struct ocp_enet_private *fep = dev->priv;
	volatile emac_t *emacp = fep->emacp;
	unsigned long tmp_em0isr;

	/* EMAC interrupt */
	tmp_em0isr = in_be32(&emacp->em0isr);
	if (tmp_em0isr & (EMAC_ISR_TE0 | EMAC_ISR_TE1)) {
		/* This error is a hard transmit error - could retransmit */
		fep->stats.tx_errors++;

		/* Reenable the transmit channel */
		mal_enable_tx_channels(fep->mal, fep->commac.tx_chan_mask);

	} else {
		fep->stats.rx_errors++;
	}

	if (tmp_em0isr & EMAC_ISR_RP)
		fep->stats.rx_length_errors++;
	if (tmp_em0isr & EMAC_ISR_ALE)
		fep->stats.rx_frame_errors++;
	if (tmp_em0isr & EMAC_ISR_BFCS)
		fep->stats.rx_crc_errors++;
	if (tmp_em0isr & EMAC_ISR_PTLE)
		fep->stats.rx_length_errors++;
	if (tmp_em0isr & EMAC_ISR_ORE)
		fep->stats.rx_length_errors++;
	if (tmp_em0isr & EMAC_ISR_TE0)
		fep->stats.tx_aborted_errors++;

	emac_err_dump(dev, tmp_em0isr);

	out_be32(&emacp->em0isr, tmp_em0isr);
}

static int
emac_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	unsigned short ctrl;
	unsigned long flags;
	struct ocp_enet_private *fep = dev->priv;
	volatile emac_t *emacp = fep->emacp;

        spin_lock_irqsave(&fep->lock, flags);

	PKT_DEBUG(("emac_start_xmit() entry, queue stopped: %d, fep->tx_cnt: %d\n",
		netif_queue_stopped(dev), fep->tx_cnt));

	/* That shouldn't happen... */
	if (netif_queue_stopped(dev) || (fep->tx_cnt == NUM_TX_BUFF)) {
		printk("%s: start_xmit called on full queue !\n", dev->name);
		BUG();
	}

	if (++fep->tx_cnt == NUM_TX_BUFF) {
		PKT_DEBUG(("emac_start_xmit() stopping queue\n"));
		netif_stop_queue(dev);
	}
	
	/* Store the skb buffer for later ack by the transmit end of buffer
	 * interrupt.
	 */
	fep->tx_skb[fep->tx_slot] = skb;
	consistent_sync((void *) skb->data, skb->len, PCI_DMA_TODEVICE);

	ctrl = EMAC_TX_CTRL_DFLT;
	if ((NUM_TX_BUFF - 1) == fep->tx_slot)
		ctrl |= MAL_TX_CTRL_WRAP;
	fep->tx_desc[fep->tx_slot].data_ptr = (char *) virt_to_phys(skb->data);
	fep->tx_desc[fep->tx_slot].data_len = (short) skb->len;
	fep->tx_desc[fep->tx_slot].ctrl = ctrl;

	/* Send the packet out. */
	out_be32(&emacp->em0tmr0, EMAC_TMR0_XMIT);

	if (++fep->tx_slot == NUM_TX_BUFF)
		fep->tx_slot = 0;

	fep->stats.tx_packets++;
	fep->stats.tx_bytes += skb->len;

	PKT_DEBUG(("emac_start_xmit() exitn"));

        spin_unlock_irqrestore(&fep->lock, flags);

	return 0;
}

static int
emac_adjust_to_link(struct ocp_enet_private *fep)
{
	volatile emac_t *emacp = fep->emacp;
	unsigned long mode_reg;
	int full_duplex, speed;

	full_duplex = 0;
	speed = SPEED_10;

	/* set mode register 1 defaults */
	mode_reg = EMAC_M1_DEFAULT;

	/* Read link mode on PHY */
    	if (fep->phy_mii.def->ops->read_link(&fep->phy_mii) == 0) {
	        /* If an error occurred, we don't deal with it yet */
		full_duplex = (fep->phy_mii.duplex == DUPLEX_FULL);
		speed = fep->phy_mii.speed;	        
	}

	/* set speed (default is 10Mb) */
	if (speed == SPEED_100) {
		mode_reg |= EMAC_M1_MF_100MBPS;
		if (fep->zmii_dev)
			emac_zmii_port_speed(fep->zmii_dev, fep->zmii_input, 100);
	} else {
		mode_reg &= ~EMAC_M1_MF_100MBPS;
		if (fep->zmii_dev)
			emac_zmii_port_speed(fep->zmii_dev, fep->zmii_input, 10);
	}

	if (full_duplex)
		mode_reg |= EMAC_M1_FDE | EMAC_M1_EIFC | EMAC_M1_IST;
	else
		mode_reg &= ~(EMAC_M1_FDE | EMAC_M1_EIFC | EMAC_M1_ILE);

        LINK_DEBUG(("%s: adjust to link, speed: %d, duplex: %d, opened: %d\n",
            fep->ndev->name, speed, full_duplex, fep->opened));

        printk(KERN_INFO "%s: Speed: %s, %s duplex.\n",
	       fep->ndev->name,
	       speed == SPEED_100 ? "100" : "10",
	       full_duplex ? "Full" : "Half");
	if (fep->opened)
	        out_be32(&emacp->em0mr1, mode_reg);

	return 0;
}

static void
__emac_set_multicast_list(struct net_device *dev)
{
	struct ocp_enet_private *fep = dev->priv;
	volatile emac_t *emacp = fep->emacp;
	u32 rmr = in_be32(&emacp->em0rmr);
	
	/* First clear all special bits, they can be set later */
	rmr &= ~(EMAC_RMR_PME | EMAC_RMR_PMME | EMAC_RMR_MAE);
	
	if (dev->flags & IFF_PROMISC) {
		rmr |= EMAC_RMR_PME;

	} else if (dev->flags & IFF_ALLMULTI || 32 < dev->mc_count) {
		/* Must be setting up to use multicast.  Now check for promiscuous
		 * multicast
		 */
		rmr |= EMAC_RMR_PMME;
	} else if (dev->flags & IFF_MULTICAST && 0 < dev->mc_count) {

		unsigned short em0gaht[4] = { 0, 0, 0, 0 };
		struct dev_mc_list *dmi;

		/* Need to hash on the multicast address. */
		for (dmi = dev->mc_list; dmi; dmi = dmi->next) {
			unsigned long mc_crc;
			unsigned int bit_number;

			mc_crc = ether_crc(6, (char *) dmi->dmi_addr);
			bit_number = 63 - (mc_crc >> 26);	/* MSB: 0 LSB: 63 */
			em0gaht[bit_number >> 4] |=
			    0x8000 >> (bit_number & 0x0f);
		}
		emacp->em0gaht1 = em0gaht[0];
		emacp->em0gaht2 = em0gaht[1];
		emacp->em0gaht3 = em0gaht[2];
		emacp->em0gaht4 = em0gaht[3];

		/* Turn on multicast addressing */
		rmr |= EMAC_RMR_MAE;
	}
	
	out_be32(&emacp->em0rmr, rmr);
}

static void
emac_init_rings(struct net_device *dev)
{
	struct ocp_enet_private *ep = dev->priv;
	int loop;

	ep->tx_desc = (struct mal_descriptor *) ((char *) ep->mal->tx_virt_addr +
				      (ep->mal_tx_chan * MAL_DT_ALIGN));
	ep->rx_desc = (struct mal_descriptor *) ((char *) ep->mal->rx_virt_addr +
				      (ep->mal_rx_chan * MAL_DT_ALIGN));

	/* Fill in the transmit descriptor ring. */
	for (loop = 0; loop < NUM_TX_BUFF; loop++) {
                if (ep->tx_skb[loop])
                        dev_kfree_skb_irq(ep->tx_skb[loop]);
		ep->tx_skb[loop] = NULL;
		ep->tx_desc[loop].ctrl = 0;
		ep->tx_desc[loop].data_len = 0;
		ep->tx_desc[loop].data_ptr = NULL;
	}
	ep->tx_desc[loop - 1].ctrl |= MAL_TX_CTRL_WRAP;

	/* Format the receive descriptor ring. */
	ep->rx_slot = 0;
	emac_rx_fill(dev, 0);
	if (ep->rx_slot != 0) {
		printk(KERN_ERR
		       "%s: Not enough mem for RxChain durning Open?\n",
		       dev->name);
		/*We couldn't fill the ring at startup?
		 *We could clean up and fail to open but right now we will try to
		 *carry on. It may be a sign of a bad NUM_RX_BUFF value
		 */
	}

	ep->tx_cnt = 0;
	ep->tx_slot = 0;
	ep->ack_slot = 0;
}

static void
emac_reset_configure(struct ocp_enet_private *fep)
{
	volatile emac_t *emacp = fep->emacp;
        int i;

	mal_disable_tx_channels(fep->mal, fep->commac.tx_chan_mask);
	mal_disable_rx_channels(fep->mal, fep->commac.rx_chan_mask);

	/* Reset the EMAC */
	out_be32(&emacp->em0mr0, EMAC_M0_SRST);
	udelay(20);
        for (i=0; i<100; i++) {
                if ((in_be32(&emacp->em0mr0) & EMAC_M0_SRST) == 0)
                        break;
                udelay(10);
        }
	
        if (i >= 100) {
                printk(KERN_ERR "%s: Cannot reset EMAC\n", fep->ndev->name);
		return;
        }

	/* Switch IRQs off for now */
	out_be32(&emacp->em0iser, 0);

	/* Configure MAL rx channel */
	mal_set_rcbs(fep->mal, fep->mal_rx_chan, DESC_BUF_SIZE_REG);

	/* set the high address */
	out_be32(&emacp->em0iahr, (fep->ndev->dev_addr[0] << 8) | fep->ndev->dev_addr[1]);

	/* set the low address */
	out_be32(&emacp->em0ialr,
		 (fep->ndev->dev_addr[2] << 24) | (fep->ndev->dev_addr[3] << 16)
		 | (fep->ndev->dev_addr[4] << 8) | fep->ndev->dev_addr[5]);

        /* Adjust to link */
	if (netif_carrier_ok(fep->ndev))
	    emac_adjust_to_link(fep);

	/* enable broadcast/individual address and RX FIFO defaults */
	out_be32(&emacp->em0rmr, EMAC_RMR_DEFAULT);

	/* set transmit request threshold register */
	out_be32(&emacp->em0trtr, EMAC_TRTR_DEFAULT);

        /* Reconfigure multicast */
        __emac_set_multicast_list(fep->ndev);

	/* Set receiver/transmitter defaults */
	out_be32(&emacp->em0rwmr, EMAC_RWMR_DEFAULT);
	out_be32(&emacp->em0tmr0, EMAC_TMR0_DEFAULT);
	out_be32(&emacp->em0tmr1, EMAC_TMR1_DEFAULT);

	/* set frame gap */
	out_be32(&emacp->em0ipgvr, CONFIG_IBM_OCP_ENET_GAP);

        /* Init ring buffers */
        emac_init_rings(fep->ndev);
}

static void
emac_kick(struct ocp_enet_private *fep)
{
	volatile emac_t *emacp = fep->emacp;
	unsigned long emac_ier;

	emac_ier = EMAC_ISR_PP | EMAC_ISR_BP | EMAC_ISR_RP |
	    EMAC_ISR_SE | EMAC_ISR_PTLE | EMAC_ISR_ALE |
	    EMAC_ISR_BFCS | EMAC_ISR_ORE | EMAC_ISR_IRE;

	out_be32(&emacp->em0iser, emac_ier);

	/* enable all MAL transmit and receive channels */
	mal_enable_tx_channels(fep->mal, fep->commac.tx_chan_mask);
	mal_enable_rx_channels(fep->mal, fep->commac.rx_chan_mask);

	/* set transmit and receive enable */
	out_be32(&emacp->em0mr0, EMAC_M0_TXE | EMAC_M0_RXE);
}

static void
emac_start_link(struct ocp_enet_private *fep, struct ethtool_cmd *ep)
{
	u32 advertise;
	int autoneg;
	int forced_speed;
	int forced_duplex;

	/* Default advertise */
	advertise = ADVERTISED_10baseT_Half | ADVERTISED_10baseT_Full |
		    ADVERTISED_100baseT_Half | ADVERTISED_100baseT_Full;
	autoneg = fep->want_autoneg;
	forced_speed = fep->phy_mii.speed;
	forced_duplex = fep->phy_mii.duplex;

	/* Setup link parameters */
	if (ep) {
	    if (ep->autoneg == AUTONEG_ENABLE) {
		advertise = ep->advertising;
		autoneg = 1;
	    } else {
		autoneg = 0;
		forced_speed = ep->speed;
		forced_duplex = ep->duplex;
	    }
	}

	/* Configure PHY & start aneg */
	fep->want_autoneg = autoneg;
	if (autoneg) {
                LINK_DEBUG(("%s: start link aneg, advertise: 0x%x\n",
                        fep->ndev->name, advertise));
		fep->phy_mii.def->ops->setup_aneg(&fep->phy_mii, advertise);
        } else {
                LINK_DEBUG(("%s: start link forced, speed: %d, duplex: %d\n",
                        fep->ndev->name, forced_speed, forced_duplex));
		fep->phy_mii.def->ops->setup_forced(&fep->phy_mii, forced_speed,
				forced_duplex);
        }
	fep->timer_ticks = 0;
	mod_timer(&fep->link_timer, jiffies + HZ);
}

static void
emac_link_timer(unsigned long data)
{
        struct ocp_enet_private *fep = (struct ocp_enet_private *)data;
	int link;

	if (fep->going_away)
		return;

	spin_lock_irq(&fep->lock);
	
	link = fep->phy_mii.def->ops->poll_link(&fep->phy_mii);
        LINK_DEBUG(("%s: poll_link: %d\n", fep->ndev->name, link));

	if (link == netif_carrier_ok(fep->ndev)) {
	        if (!link && fep->want_autoneg && (++fep->timer_ticks) > 10)
		        emac_start_link(fep, NULL);
	        goto out;
        }
	printk(KERN_INFO "%s: Link is %s\n", fep->ndev->name, link ? "Up" : "Down");
	if (link) {
	        netif_carrier_on(fep->ndev);
                /* Chip needs a full reset on config change. That sucks, so I
                 * should ultimately move that to some tasklet to limit
                 * latency peaks caused by this code
                 */
                emac_reset_configure(fep);
		if (fep->opened)
                	emac_kick(fep);
	} else {
	        fep->timer_ticks = 0;
	        netif_carrier_off(fep->ndev);
	}
out:
	mod_timer(&fep->link_timer, jiffies + HZ);
	spin_unlock_irq(&fep->lock);
}

static void
emac_set_multicast_list(struct net_device *dev)
{
	struct ocp_enet_private *fep = dev->priv;

	spin_lock_irq(&fep->lock);
        __emac_set_multicast_list(dev);
	spin_unlock_irq(&fep->lock);
}

static int
emac_ethtool(struct net_device *dev, void* ep_user)
{
        struct ocp_enet_private *fep = dev->priv;
        struct ethtool_cmd ecmd;
        unsigned long features = fep->phy_mii.def->features;

        if (copy_from_user(&ecmd, ep_user, sizeof(ecmd)))
                return -EFAULT;

        switch(ecmd.cmd) {
        case ETHTOOL_GDRVINFO: {
                struct ethtool_drvinfo info;
                memset(&info, 0, sizeof(info));
                info.cmd = ETHTOOL_GDRVINFO;
                strncpy(info.driver, DRV_NAME, ETHTOOL_BUSINFO_LEN);
                strncpy(info.version, DRV_VERSION, ETHTOOL_BUSINFO_LEN);
                info.fw_version[0] = '\0';
                sprintf(info.bus_info, "OCP EMAC %d", fep->ocpdev->def->index);
                info.regdump_len = 0;
                if (copy_to_user(ep_user, &info, sizeof(info)))
                        return -EFAULT;
                return 0;
                }

        case ETHTOOL_GSET:
                ecmd.supported = features;
                ecmd.port = PORT_MII;
                ecmd.transceiver = XCVR_EXTERNAL;
                ecmd.phy_address = fep->mii_phy_addr;
                spin_lock_irq(&fep->lock);
                ecmd.autoneg = fep->want_autoneg;
                ecmd.speed = fep->phy_mii.speed;
                ecmd.duplex = fep->phy_mii.duplex;
                spin_unlock_irq(&fep->lock);
                if (copy_to_user(ep_user, &ecmd, sizeof(ecmd)))
                        return -EFAULT;
                return 0;

        case ETHTOOL_SSET:
                if (!capable(CAP_NET_ADMIN))
                        return -EPERM;
                
                if (ecmd.autoneg != AUTONEG_ENABLE &&
                    ecmd.autoneg != AUTONEG_DISABLE)
                        return -EINVAL;
                if (ecmd.autoneg == AUTONEG_ENABLE &&
                    ecmd.advertising == 0)
                        return -EINVAL;
                if (ecmd.duplex != DUPLEX_HALF && ecmd.duplex != DUPLEX_FULL)
                        return -EINVAL;
                if (ecmd.autoneg == AUTONEG_DISABLE)
                        switch(ecmd.speed) {
                        case SPEED_10:
                                if (ecmd.duplex == DUPLEX_HALF &&
                                    (features & SUPPORTED_10baseT_Half) == 0)
                                        return -EINVAL;
                                if (ecmd.duplex == DUPLEX_FULL &&
                                    (features & SUPPORTED_10baseT_Full) == 0)
                                        return -EINVAL;
                                break;
                        case SPEED_100:
                                if (ecmd.duplex == DUPLEX_HALF &&
                                    (features & SUPPORTED_100baseT_Half) == 0)
                                        return -EINVAL;
                                if (ecmd.duplex == DUPLEX_FULL &&
                                    (features & SUPPORTED_100baseT_Full) == 0)
                                        return -EINVAL;
                                break;
                        default:
                                return -EINVAL;
                        }
                else if ((features & SUPPORTED_Autoneg) == 0)
                        return -EINVAL;
                spin_lock_irq(&fep->lock);
                emac_start_link(fep, &ecmd);
                spin_unlock_irq(&fep->lock);                
                return 0;

        case ETHTOOL_NWAY_RST:
                if (!fep->want_autoneg)
                        return -EINVAL;
                spin_lock_irq(&fep->lock);
                emac_start_link(fep, NULL);
                spin_unlock_irq(&fep->lock);
                return 0;

        case ETHTOOL_GLINK: {
                struct ethtool_value edata;
                memset(&edata, 0, sizeof(edata));
                edata.cmd = ETHTOOL_GLINK;
                edata.data = netif_carrier_ok(dev);
                if (copy_to_user(ep_user, &edata, sizeof(edata)))
                        return -EFAULT;
                return 0;
                }
        }

        return -EOPNOTSUPP;
}

static int
emac_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	struct ocp_enet_private *fep = dev->priv;
	uint *data = (uint *) & rq->ifr_data;

	switch (cmd) {
        case SIOCETHTOOL:
                return emac_ethtool(dev, rq->ifr_data);
	case SIOCDEVPRIVATE:
	case SIOCGMIIPHY:
		data[0] = fep->mii_phy_addr;
		/*FALLTHRU*/
	case SIOCDEVPRIVATE + 1:
	case SIOCGMIIREG:
		data[3] = emac_phy_read(dev, fep->mii_phy_addr, data[1]);
		return 0;
	case SIOCDEVPRIVATE + 2:
	case SIOCSMIIREG:
		if (!capable(CAP_NET_ADMIN))
			return -EPERM;

		emac_phy_write(dev, fep->mii_phy_addr, data[1], data[2]);
		return 0;
	default:
		return -EOPNOTSUPP;
	}
}
 
static int
emac_open(struct net_device *dev)
{
	struct ocp_enet_private *fep = dev->priv;
	int rc;

	spin_lock_irq(&fep->lock);

	fep->opened = 1;
        
        /* Reset & configure the chip */
        emac_reset_configure(fep);

	spin_unlock_irq(&fep->lock);

        /* Request our interrupt lines */
	rc = request_irq(dev->irq, emac_mac_irq, 0, "OCP EMAC MAC", dev);
	if (rc != 0)
		goto bail;
	rc = request_irq(fep->wol_irq, emac_wakeup_irq, 0, "OCP EMAC Wakeup", dev);
	if (rc != 0) {
		free_irq(dev->irq, dev);
		goto bail;
	}
        /* Kick the chip rx & tx channels into life */ 
	spin_lock_irq(&fep->lock);
        emac_kick(fep);
        spin_unlock_irq(&fep->lock);

	netif_start_queue(dev);
bail:
	return rc;
}

static int
emac_close(struct net_device *dev)
{
	struct ocp_enet_private *fep = dev->priv;
	volatile emac_t *emacp = fep->emacp;

	/* XXX Stop IRQ emitting here */
	spin_lock_irq(&fep->lock);
	fep->opened = 0;
	mal_disable_tx_channels(fep->mal, fep->commac.tx_chan_mask);
	mal_disable_rx_channels(fep->mal, fep->commac.rx_chan_mask);
	netif_stop_queue(dev);

	out_be32(&emacp->em0mr0, EMAC_M0_SRST);
	udelay(10);

	if (emacp->em0mr0 & EMAC_M0_SRST) {
		/*not sure what to do here hopefully it clears before another open */
		printk(KERN_ERR "%s: Phy SoftReset didn't clear, no link?\n",
		       dev->name);
	}

	/* Free the irq's */
	free_irq(dev->irq, dev);
	free_irq(fep->wol_irq, dev);

	spin_unlock_irq(&fep->lock);

	return 0;
}

static void
emac_remove(struct ocp_device *ocpdev)
{
	struct net_device *dev = ocp_get_drvdata(ocpdev);
	struct ocp_enet_private *ep = dev->priv;

	/* FIXME: locking, races, ... */
	ep->going_away = 1;
	ocp_set_drvdata(ocpdev, NULL);
	if (ep->zmii_dev)
		emac_fini_zmii(ep->zmii_dev);

	unregister_netdev(dev);
	del_timer_sync(&ep->link_timer);
	mal_unregister_commac(ep->mal, &ep->commac);
	iounmap((void *)ep->emacp);
	kfree(dev);
}

struct mal_commac_ops emac_commac_ops = {
	.txeob = &emac_txeob_dev,
	.txde = &emac_txde_dev,
	.rxeob = &emac_rxeob_dev,
	.rxde = &emac_rxde_dev,
};

static int
emac_probe(struct ocp_device *ocpdev)
{
	int rc = 0, i;
	bd_t *bd;
	struct net_device *ndev;
	struct ocp_enet_private *ep;
	struct ocp_device *maldev;
	struct ibm_ocp_mal *mal;
	struct ocp_func_emac_data *emacdata;
	struct ocp_device *mdiodev;
	struct net_device *mdio_ndev = NULL;
	int commac_reg = 0;
	u32 phy_map;
	
	emacdata = (struct ocp_func_emac_data *)ocpdev->def->additions;
	if (emacdata == NULL) {
		printk(KERN_ERR "emac%d: Missing additional datas !\n", ocpdev->def->index);
		return -ENODEV;
	}

	/* Wait for MAL to show up */
	maldev = ocp_find_device(OCP_ANY_ID, OCP_FUNC_MAL, emacdata->mal_idx);
	if (maldev == NULL)
		return -EAGAIN;
	/* Check if MAL driver attached yet */
	mal = (struct ibm_ocp_mal *)ocp_get_drvdata(maldev);
	if (mal == NULL)
		return -EAGAIN;

	/* If we depend on another EMAC for MDIO, wait for it to show up */
	if (emacdata->mdio_idx >= 0 && emacdata->mdio_idx != ocpdev->def->index) {
		mdiodev = ocp_find_device(OCP_ANY_ID, OCP_FUNC_EMAC, emacdata->mdio_idx);
		if (mdiodev == NULL)
			return -EAGAIN;
		mdio_ndev = (struct net_device *)ocp_get_drvdata(mdiodev);
		if (mdio_ndev == NULL)
			return -EAGAIN;
	}

	/* Allocate our net_device structure */
	ndev = alloc_etherdev(sizeof (struct ocp_enet_private));
	if (ndev == NULL) {
		printk(KERN_ERR
		       "emac%d: Could not allocate ethernet device.\n", ocpdev->def->index);
		return -ENOMEM;
	}
	ep = ndev->priv;
	memset(ep, 0, sizeof(*ep));
	ep->ndev = ndev;
	ep->ocpdev = ocpdev;
	ndev->irq = ocpdev->def->irq;
	ep->wol_irq = emacdata->wol_irq;
	ep->mdio_dev = mdio_ndev;
	ocp_set_drvdata(ocpdev, ndev);
	spin_lock_init(&ep->lock);

	/* Fill out MAL informations and register commac */
	ep->mal = mal;
	ep->mal_tx_chan = emacdata->mal_tx1_chan;
	ep->mal_rx_chan = emacdata->mal_rx_chan;
	ep->commac.ops = &emac_commac_ops;
	ep->commac.dev = ndev;
	ep->commac.tx_chan_mask = MAL_CHAN_MASK(ep->mal_tx_chan);
	ep->commac.rx_chan_mask = MAL_CHAN_MASK(ep->mal_rx_chan);
	rc = mal_register_commac(ep->mal, &ep->commac);
	if (rc != 0)
		goto bail;
	commac_reg = 1;

	/* Map our MMIOs */
	ep->emacp = (volatile emac_t *)ioremap(ocpdev->def->paddr, sizeof (emac_t));

	/* Check if we need to attach to a ZMII */
	if (emacdata->zmii_idx >= 0) {
		ep->zmii_input = emacdata->zmii_mux;
		ep->zmii_dev = ocp_find_device(OCP_ANY_ID, OCP_FUNC_ZMII, emacdata->zmii_idx);
		if (ep->zmii_dev == NULL)
	                printk(KERN_WARNING "emac%d: ZMII %d requested but not found !\n",
	                	ocpdev->def->index, emacdata->zmii_idx);
		else if ((rc = emac_init_zmii(ep->zmii_dev, ZMII_AUTO)) != 0)
			goto bail;
	}

	/* Reset the EMAC */
	out_be32(&ep->emacp->em0mr0, EMAC_M0_SRST);
	udelay(20);
        for (i=0; i<100; i++) {
                if ((in_be32(&ep->emacp->em0mr0) & EMAC_M0_SRST) == 0)
                        break;
                udelay(10);
        }
	
        if (i >= 100) {
                printk(KERN_ERR "emac%d: Cannot reset EMAC\n", ocpdev->def->index);
		rc = -ENXIO;
		goto bail;
        }

	/* Init link monitoring timer */
	init_timer(&ep->link_timer);
	ep->link_timer.function = emac_link_timer;
	ep->link_timer.data = (unsigned long) ep;
	ep->timer_ticks = 0;

	/* Fill up the mii_phy structure */
	ep->phy_mii.dev = ndev;
	ep->phy_mii.mdio_read = emac_phy_read;
	ep->phy_mii.mdio_write = emac_phy_write;

	/* Find PHY */
	phy_map = emac_phy_map[ocpdev->def->index] | busy_phy_map;
	for (i = 0; i <= 0x1f; i++, phy_map >>= 1) {
		if ((phy_map & 0x1) == 0) {
			int  val = emac_phy_read(ndev, i, MII_BMCR);
			if (val != 0xffff && val != -1)
			        break;
		}
	}
	if (i == 0x20) {
	        printk(KERN_WARNING "emac%d: Can't find PHY.\n", ocpdev->def->index);
		rc = -ENODEV;
		goto bail;
	}
	busy_phy_map |= 1 << i;
	ep->mii_phy_addr = i;
	rc = mii_phy_probe(&ep->phy_mii, i);
	if (rc) {
	        printk(KERN_WARNING "emac%d: Failed to probe PHY type.\n", ocpdev->def->index);
		rc = -ENODEV;
		goto bail;
	}

	/* Setup initial PHY config & startup aneg */
	if (ep->phy_mii.def->ops->init)
	        ep->phy_mii.def->ops->init(&ep->phy_mii);
	netif_carrier_off(ndev);
        if (ep->phy_mii.def->features & SUPPORTED_Autoneg)
                ep->want_autoneg = 1;
	emac_start_link(ep, NULL);


	/* read the MAC Address */
	bd = (bd_t *) __res;
	for (i = 0; i < 6; i++)
		ndev->dev_addr[i] = bd->BD_EMAC_ADDR(ocpdev->def->index, i);	/* Marco to disques array */

	/* Fill in the driver function table */
	ndev->open = &emac_open;
	ndev->hard_start_xmit = &emac_start_xmit;
	ndev->stop = &emac_close;
	ndev->get_stats = &emac_stats;
	ndev->set_multicast_list = &emac_set_multicast_list;
	ndev->do_ioctl = &emac_ioctl;

	SET_MODULE_OWNER(ndev);
	
	rc = register_netdev(ndev);
	if (rc != 0)
		goto bail;

	printk("%s: IBM emac, MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
		ndev->name,
		ndev->dev_addr[0], ndev->dev_addr[1], ndev->dev_addr[2],
		ndev->dev_addr[3], ndev->dev_addr[4], ndev->dev_addr[5]);
	printk(KERN_INFO "%s: Found %s PHY (0x%02x)\n",
		ndev->name, ep->phy_mii.def->name, ep->mii_phy_addr);


bail:
	if (rc && commac_reg)
		mal_unregister_commac(ep->mal, &ep->commac);
	if (rc && ndev)
		kfree(ndev);

	return rc;

}

/* Structure for a device driver */
static struct ocp_device_id emac_ids[] =
{
	{ .vendor = OCP_ANY_ID, .function = OCP_FUNC_EMAC },
	{ .vendor = OCP_VENDOR_INVALID }
};

static struct ocp_driver emac_driver =
{
	.name 		= "emac",
	.id_table	= emac_ids,
	
	.probe		= emac_probe,
	.remove		= emac_remove,
};

static int __init
emac_init(void)
{
	int rc;
	
        printk(KERN_INFO DRV_NAME ": " DRV_DESC ", version " DRV_VERSION "\n");
        printk(KERN_INFO "Maintained by " DRV_AUTHOR "\n");

	if (skb_res > 2) {
	    printk(KERN_WARNING "Invalid skb_res: %d, cropping to 2\n", skb_res);
	    skb_res = 2;
	}
	rc = ocp_register_driver(&emac_driver);
	if (rc == 0) {
		ocp_unregister_driver(&emac_driver);
		return -ENODEV;
	}

	return 0;
}



static void __exit
emac_exit(void)
{
	ocp_unregister_driver(&emac_driver);
}

module_init(emac_init);
module_exit(emac_exit);
