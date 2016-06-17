/*
 * ibm_ocp_enet.h
 *
 * Ethernet driver for the built in ethernet on the IBM 405 PowerPC
 * processor.
 *
 *      Armin Kuster akuster@mvista.com
 *      Sept, 2001
 *
 *      Orignial driver
 *         Johnnie Peters
 *         jpeters@mvista.com
 *
 * Copyright 2000 MontaVista Softare Inc.
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
 *  Version: 1.0: Name change - armin
 *  Version: 1.0: Added extern prototypes for new mal func. - David M.
 *  		: removed #ifdef for irqs.i - Armin
 *  		: added irq_resourse to private data struct. 
 *
 *  Version: 1.1: Added new min/max value for phy & removed unused phy_id_done - *			- andrew May
 *  Version: 1.2: added all emac extern protos here - Armin
 *
 *  Version: 1.3: Using CONFIG_IBM_OCP_ZMII instead of ZMII_NUMS > 0
 *
 *  Version: 1.3: Name change *_driver to *_dev
 *  Version: 1.4: removed irq_resource & BL_* defines
 *
 */

#ifndef _IBM_OCP_ENET_H_
#define _IBM_OCP_ENET_H_

#include <linux/netdevice.h>
#include <asm/ocp.h>
#include <asm/mmu.h>		/* For phys_addr_t */

#include "ibm_emac.h"
#include "ibm_ocp_phy.h"
#include "ibm_ocp_zmii.h"
#include "ibm_ocp_mal.h"

#ifndef CONFIG_IBM_OCP_ENET_TX_BUFF
#define NUM_TX_BUFF		64
#define NUM_RX_BUFF		64
#else
#define NUM_TX_BUFF		CONFIG_IBM_OCP_ENET_TX_BUFF
#define NUM_RX_BUFF		CONFIG_IBM_OCP_ENET_RX_BUFF
#endif

/* This does 16 byte alignment, exactly what we need.
 * The packet length includes FCS, but we don't want to
 * include that when passing upstream as it messes up
 * bridging applications.
 */
#ifndef CONFIG_IBM_OCP_ENET_SKB_RES
#define SKB_RES 2
#else
#define SKB_RES CONFIG_IBM_OCP_ENET_SKB_RES
#endif

/* Note about alignement. alloc_skb() returns a cache line
 * aligned buffer. However, dev_alloc_skb() will add 16 more
 * bytes and "reserve" them, so our buffer will actually end
 * on a half cache line. What we do is to use directly
 * alloc_skb, allocate 16 more bytes to match the total amount
 * allocated by dev_alloc_skb(), but we don't reserve.
 */
#define MAX_NUM_BUF_DESC	255
#define DESC_RX_BUF_SIZE	1536
#define DESC_BUF_SIZE_REG	(DESC_RX_BUF_SIZE / 16)

/* Transmitter timeout. */
#define TX_TIMEOUT		(2*HZ)
#define OCP_RESET_DELAY		50
#define MDIO_DELAY		2

/* Power managment shift registers */
#define IBM_CPM_EMMII	0	/* Shift value for MII */
#define IBM_CPM_EMRX	1	/* Shift value for recv */
#define IBM_CPM_EMTX	2	/* Shift value for MAC */
#define IBM_CPM_EMAC(x)	(((x)>>IBM_CPM_EMMII) | ((x)>>IBM_CPM_EMRX) | ((x)>>IBM_CPM_EMTX))

#ifdef CONFIG_IBM_OCP_ENET_ERROR_MSG
void emac_serr_dump_0(struct net_device *dev);
void emac_serr_dump_1(struct net_device *dev);
void emac_err_dump(struct net_device *dev, int em0isr);
void emac_phy_dump(struct net_device *);
void emac_desc_dump(struct net_device *);
void emac_mac_dump(struct net_device *);
void emac_mal_dump(struct net_device *);
#else
#define emac_serr_dump_0(dev) do { } while (0)
#define emac_serr_dump_1(dev) do { } while (0)
#define emac_err_dump(dev,x) do { } while (0)
#define emac_phy_dump(dev) do { } while (0)
#define emac_desc_dump(dev) do { } while (0)
#define emac_mac_dump(dev) do { } while (0)
#define emac_mal_dump(dev) do { } while (0)
#endif


struct ocp_enet_private {
	struct sk_buff *tx_skb[NUM_TX_BUFF];
	struct sk_buff *rx_skb[NUM_RX_BUFF];
	struct mal_descriptor *tx_desc;
	struct mal_descriptor *rx_desc;
	struct mal_descriptor *rx_dirty;
	struct net_device_stats stats;
	int tx_cnt;
	int rx_slot;
	int dirty_rx;
	int tx_slot;
	int ack_slot;

	struct mii_phy phy_mii;
        int mii_phy_addr;
        int want_autoneg;
        int timer_ticks;
	struct timer_list link_timer;
	struct net_device *mdio_dev;

	struct ocp_device *zmii_dev;
	int zmii_input;

	struct ibm_ocp_mal *mal;
	int mal_tx_chan, mal_rx_chan;
	struct mal_commac commac;

        int opened;
        int going_away;
        int wol_irq;
	volatile emac_t *emacp;
	struct ocp_device *ocpdev;
        struct net_device *ndev;
        spinlock_t lock;
};


#endif				/* _IBM_OCP_ENET_H_ */
