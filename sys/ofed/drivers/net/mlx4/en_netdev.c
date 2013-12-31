/*
 * Copyright (c) 2007 Mellanox Technologies. All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include "mlx4_en.h"

#include <linux/mlx4/driver.h>
#include <linux/mlx4/device.h>
#include <linux/mlx4/cmd.h>
#include <linux/mlx4/cq.h>

#include <linux/delay.h>
#include <net/ethernet.h>
#include <net/if_vlan_var.h>
#include <sys/sockio.h>

static void mlx4_en_init_locked(struct mlx4_en_priv *priv);
static void mlx4_en_sysctl_stat(struct mlx4_en_priv *priv);

static void mlx4_en_vlan_rx_add_vid(void *arg, struct net_device *dev, u16 vid)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	int idx;
	u8 field;

	if (arg != priv)
		return;

	if ((vid == 0) || (vid > 4095))    /* Invalid */
		return;
	en_dbg(HW, priv, "adding VLAN:%d\n", vid);
	idx = vid >> 5;
	field = 1 << (vid & 0x1f);
	spin_lock(&priv->vlan_lock);
	priv->vlgrp_modified = true;
	if (priv->vlan_unregister[idx] & field)
		priv->vlan_unregister[idx] &= ~field;
	else
		priv->vlan_register[idx] |= field;
	priv->vlans[idx] |= field;
	spin_unlock(&priv->vlan_lock);
}

static void mlx4_en_vlan_rx_kill_vid(void *arg, struct net_device *dev, u16 vid)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	int idx;
	u8 field;

	if (arg != priv)
		return;

	if ((vid == 0) || (vid > 4095))    /* Invalid */
		return;
	en_dbg(HW, priv, "Killing VID:%d\n", vid);
	idx = vid >> 5;
	field = 1 << (vid & 0x1f);
	spin_lock(&priv->vlan_lock);
	priv->vlgrp_modified = true;
	if (priv->vlan_register[idx] & field)
		priv->vlan_register[idx] &= ~field;
	else
		priv->vlan_unregister[idx] |= field;
	priv->vlans[idx] &= ~field;
	spin_unlock(&priv->vlan_lock);
}

u64 mlx4_en_mac_to_u64(u8 *addr)
{
	u64 mac = 0;
	int i;

	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		mac <<= 8;
		mac |= addr[i];
	}
	return mac;
}

static int mlx4_en_cache_mclist(struct net_device *dev, u64 **mcaddrp)
{
	struct ifmultiaddr *ifma;
	u64 *mcaddr;
	int cnt;
	int i;

	*mcaddrp = NULL;
restart:
	cnt = 0;
	if_maddr_rlock(dev);
	TAILQ_FOREACH(ifma, &dev->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		if (((struct sockaddr_dl *)ifma->ifma_addr)->sdl_alen !=
		    ETHER_ADDR_LEN)
			continue;
		cnt++;
	}
	if_maddr_runlock(dev);
	if (cnt == 0)
		return (0);
	mcaddr = kmalloc(sizeof(u64) * cnt, GFP_KERNEL);
	if (mcaddr == NULL)
		return (0);
	i = 0;
	if_maddr_rlock(dev);
	TAILQ_FOREACH(ifma, &dev->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		if (((struct sockaddr_dl *)ifma->ifma_addr)->sdl_alen !=
		    ETHER_ADDR_LEN)
			continue;
		/* Make sure the list didn't grow. */
		if (i == cnt) {
			if_maddr_runlock(dev);
			kfree(mcaddr);
			goto restart;
		}
		mcaddr[i++] = mlx4_en_mac_to_u64(
		    LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
	}
	if_maddr_runlock(dev);
	*mcaddrp = mcaddr;
	return (i);
}

static void mlx4_en_stop_port(struct net_device *dev)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
 
	queue_work(priv->mdev->workqueue, &priv->stop_port_task);
}

static void mlx4_en_start_port(struct net_device *dev)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);

	queue_work(priv->mdev->workqueue, &priv->start_port_task);
}

static void mlx4_en_set_multicast(struct net_device *dev)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);

	if (!priv->port_up)
		return;

	queue_work(priv->mdev->workqueue, &priv->mcast_task);
}

static void mlx4_en_do_set_multicast(struct work_struct *work)
{
	struct mlx4_en_priv *priv = container_of(work, struct mlx4_en_priv,
						 mcast_task);
	struct net_device *dev = priv->dev;
	struct mlx4_en_dev *mdev = priv->mdev;
	int err;

	mutex_lock(&mdev->state_lock);
	if (!mdev->device_up) {
		en_dbg(HW, priv, "Card is not up, "
				 "ignoring multicast change.\n");
		goto out;
	}
	if (!priv->port_up) {
		en_dbg(HW, priv, "Port is down, "
				 "ignoring  multicast change.\n");
		goto out;
	}

	/*
	 * Promsicuous mode: disable all filters
	 */

	if (dev->if_flags & IFF_PROMISC) {
		if (!(priv->flags & MLX4_EN_FLAG_PROMISC)) {
			priv->flags |= MLX4_EN_FLAG_PROMISC;

			/* Enable promiscouos mode */
			err = mlx4_SET_PORT_qpn_calc(mdev->dev, priv->port,
						     priv->base_qpn, 1);
			if (err)
				en_err(priv, "Failed enabling "
					     "promiscous mode\n");

			/* Disable port multicast filter (unconditionally) */
			err = mlx4_SET_MCAST_FLTR(mdev->dev, priv->port, 0,
						  0, MLX4_MCAST_DISABLE);
			if (err)
				en_err(priv, "Failed disabling "
					     "multicast filter\n");

			/* Disable port VLAN filter */
			err = mlx4_SET_VLAN_FLTR(mdev->dev, priv->port, NULL);
			if (err)
				en_err(priv, "Failed disabling VLAN filter\n");
		}
		goto out;
	}

	/*
	 * Not in promiscous mode
	 */

	if (priv->flags & MLX4_EN_FLAG_PROMISC) {
		priv->flags &= ~MLX4_EN_FLAG_PROMISC;

		/* Disable promiscouos mode */
		err = mlx4_SET_PORT_qpn_calc(mdev->dev, priv->port,
					     priv->base_qpn, 0);
		if (err)
			en_err(priv, "Failed disabling promiscous mode\n");

		/* Enable port VLAN filter */
		err = mlx4_SET_VLAN_FLTR(mdev->dev, priv->port, priv->vlans);
		if (err)
			en_err(priv, "Failed enabling VLAN filter\n");
	}

	/* Enable/disable the multicast filter according to IFF_ALLMULTI */
	if (dev->if_flags & IFF_ALLMULTI) {
		err = mlx4_SET_MCAST_FLTR(mdev->dev, priv->port, 0,
					  0, MLX4_MCAST_DISABLE);
		if (err)
			en_err(priv, "Failed disabling multicast filter\n");
	} else {
		u64 *mcaddr;
		int mccount;
		int i;

		err = mlx4_SET_MCAST_FLTR(mdev->dev, priv->port, 0,
					  0, MLX4_MCAST_DISABLE);
		if (err)
			en_err(priv, "Failed disabling multicast filter\n");

		/* Flush mcast filter and init it with broadcast address */
		mlx4_SET_MCAST_FLTR(mdev->dev, priv->port, ETH_BCAST,
				    1, MLX4_MCAST_CONFIG);

		/* Update multicast list - we cache all addresses so they won't
		 * change while HW is updated holding the command semaphor */
		mccount = mlx4_en_cache_mclist(dev, &mcaddr);
		for (i = 0; i < mccount; i++)
			mlx4_SET_MCAST_FLTR(mdev->dev, priv->port,
					    mcaddr[i], 0, MLX4_MCAST_CONFIG);
		err = mlx4_SET_MCAST_FLTR(mdev->dev, priv->port, 0,
					  0, MLX4_MCAST_ENABLE);
		if (err)
			en_err(priv, "Failed enabling multicast filter\n");

		kfree(mcaddr);
	}
out:
	mutex_unlock(&mdev->state_lock);
}

#ifdef CONFIG_NET_POLL_CONTROLLER
static void mlx4_en_netpoll(struct net_device *dev)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_cq *cq;
	unsigned long flags;
	int i;

	for (i = 0; i < priv->rx_ring_num; i++) {
		cq = &priv->rx_cq[i];
		spin_lock_irqsave(&cq->lock, flags);
		napi_synchronize(&cq->napi);
		mlx4_en_process_rx_cq(dev, cq, 0);
		spin_unlock_irqrestore(&cq->lock, flags);
	}
}
#endif

static void mlx4_en_watchdog_timeout(void *arg)
{
	struct mlx4_en_priv *priv = arg;
	struct mlx4_en_dev *mdev = priv->mdev;

	en_dbg(DRV, priv, "Scheduling watchdog\n");
	queue_work(mdev->workqueue, &priv->watchdog_task);
	if (priv->port_up)
		callout_reset(&priv->watchdog_timer, MLX4_EN_WATCHDOG_TIMEOUT,
		    mlx4_en_watchdog_timeout, priv);
}


/* XXX This clears user settings in too many cases. */
static void mlx4_en_set_default_moderation(struct mlx4_en_priv *priv)
{
	struct mlx4_en_cq *cq;
	int i;

	/* If we haven't received a specific coalescing setting
	 * (module param), we set the moderation paramters as follows:
	 * - moder_cnt is set to the number of mtu sized packets to
	 *   satisfy our coelsing target.
	 * - moder_time is set to a fixed value.
	 */
	priv->rx_frames = MLX4_EN_RX_COAL_TARGET / priv->dev->if_mtu + 1;
	priv->rx_usecs = MLX4_EN_RX_COAL_TIME;
	en_dbg(INTR, priv, "Default coalesing params for mtu:%ld - "
			   "rx_frames:%d rx_usecs:%d\n",
		 priv->dev->if_mtu, priv->rx_frames, priv->rx_usecs);

	/* Setup cq moderation params */
	for (i = 0; i < priv->rx_ring_num; i++) {
		cq = &priv->rx_cq[i];
		cq->moder_cnt = priv->rx_frames;
		cq->moder_time = priv->rx_usecs;
		priv->last_moder_time[i] = MLX4_EN_AUTO_CONF;
		priv->last_moder_packets[i] = 0;
		priv->last_moder_bytes[i] = 0;
	}

	for (i = 0; i < priv->tx_ring_num; i++) {
		cq = &priv->tx_cq[i];
		cq->moder_cnt = MLX4_EN_TX_COAL_PKTS;
		cq->moder_time = MLX4_EN_TX_COAL_TIME;
	}

	/* Reset auto-moderation params */
	priv->pkt_rate_low = MLX4_EN_RX_RATE_LOW;
	priv->rx_usecs_low = MLX4_EN_RX_COAL_TIME_LOW;
	priv->pkt_rate_high = MLX4_EN_RX_RATE_HIGH;
	priv->rx_usecs_high = MLX4_EN_RX_COAL_TIME_HIGH;
	priv->sample_interval = MLX4_EN_SAMPLE_INTERVAL;
	priv->adaptive_rx_coal = 1;
	priv->last_moder_jiffies = 0;
	priv->last_moder_tx_packets = 0;
}

static void mlx4_en_auto_moderation(struct mlx4_en_priv *priv)
{
	unsigned long period = (unsigned long) (jiffies - priv->last_moder_jiffies);
	struct mlx4_en_cq *cq;
	unsigned long packets;
	unsigned long rate;
	unsigned long avg_pkt_size;
	unsigned long rx_packets;
	unsigned long rx_bytes;
	unsigned long rx_pkt_diff;
	int moder_time;
	int ring, err;

	if (!priv->adaptive_rx_coal || period < priv->sample_interval * HZ)
		return;
	for (ring = 0; ring < priv->rx_ring_num; ring++) {
		spin_lock(&priv->stats_lock);
		rx_packets = priv->rx_ring[ring].packets;
		rx_bytes = priv->rx_ring[ring].bytes;
		spin_unlock(&priv->stats_lock);

		rx_pkt_diff = ((unsigned long) (rx_packets -
				priv->last_moder_packets[ring]));
		packets = rx_pkt_diff;
		rate = packets * HZ / period;
		avg_pkt_size = packets ? ((unsigned long) (rx_bytes -
				priv->last_moder_bytes[ring])) / packets : 0;

		/* Apply auto-moderation only when packet rate
		* exceeds a rate that it matters */
		if (rate > (MLX4_EN_RX_RATE_THRESH / priv->rx_ring_num) &&
				avg_pkt_size > MLX4_EN_AVG_PKT_SMALL) {
			if (rate < priv->pkt_rate_low ||
			    avg_pkt_size < MLX4_EN_AVG_PKT_SMALL)
				moder_time = priv->rx_usecs_low;
			else if (rate > priv->pkt_rate_high)
				moder_time = priv->rx_usecs_high;
			else
				moder_time = (rate - priv->pkt_rate_low) *
					(priv->rx_usecs_high - priv->rx_usecs_low) /
					(priv->pkt_rate_high - priv->pkt_rate_low) +
					priv->rx_usecs_low;
		} else {
			moder_time = priv->rx_usecs_low;
		}

		if (moder_time != priv->last_moder_time[ring]) {
			priv->last_moder_time[ring] = moder_time;
			cq = &priv->rx_cq[ring];
			cq->moder_time = moder_time;
			err = mlx4_en_set_cq_moder(priv, cq);
			if (err)
				en_err(priv, "Failed modifying moderation "
					"for cq:%d\n", ring);
		}
		priv->last_moder_packets[ring] = rx_packets;
		priv->last_moder_bytes[ring] = rx_bytes;
	}

	priv->last_moder_jiffies = jiffies;
}

static void mlx4_en_handle_vlans(struct mlx4_en_priv *priv)
{
	u8 vlan_register[VLAN_FLTR_SIZE];
	u8 vlan_unregister[VLAN_FLTR_SIZE];
	int i, j, idx;
	u16 vid;

	/* cache the vlan data for processing 
	 * done under lock to avoid changes during work */
	spin_lock(&priv->vlan_lock);
	for (i = 0; i < VLAN_FLTR_SIZE; i++) {
		vlan_register[i] = priv->vlan_register[i];
		priv->vlan_register[i] = 0;
		vlan_unregister[i] = priv->vlan_unregister[i];
		priv->vlan_unregister[i] = 0;
	}
	priv->vlgrp_modified = false;
	spin_unlock(&priv->vlan_lock);

	/* Configure the vlan filter 
	 * The vlgrp is updated with all the vids that need to be allowed */
	if (mlx4_SET_VLAN_FLTR(priv->mdev->dev, priv->port, priv->vlans))
		en_err(priv, "Failed configuring VLAN filter\n");

	/* Configure the VLAN table */
	for (i = 0; i < VLAN_FLTR_SIZE; i++) {
		for (j = 0; j < 32; j++) {
			vid = (i << 5) + j;
			if (vlan_register[i] & (1 << j))
				if (mlx4_register_vlan(priv->mdev->dev, priv->port, vid, &idx))
					en_dbg(HW, priv, "failed registering vlan %d\n", vid);
			if (vlan_unregister[i] & (1 << j)) {
				if (!mlx4_find_cached_vlan(priv->mdev->dev, priv->port, vid, &idx))
					mlx4_unregister_vlan(priv->mdev->dev, priv->port, idx);
				else
					en_dbg(HW, priv, "could not find vid %d in cache\n", vid);
			}
		}
	}
}

static void mlx4_en_do_get_stats(struct work_struct *work)
{
	struct delayed_work *delay = to_delayed_work(work);
	struct mlx4_en_priv *priv = container_of(delay, struct mlx4_en_priv,
						 stats_task);
	struct mlx4_en_dev *mdev = priv->mdev;
	int err;

	err = mlx4_en_DUMP_ETH_STATS(mdev, priv->port, 0);
	if (err)
		en_dbg(HW, priv, "Could not update stats \n");


	mutex_lock(&mdev->state_lock);
	if (mdev->device_up) {
		if (priv->port_up) {
			if (priv->vlgrp_modified)
				mlx4_en_handle_vlans(priv);

			mlx4_en_auto_moderation(priv);
		}

		queue_delayed_work(mdev->workqueue, &priv->stats_task, STATS_DELAY);
	}
	mlx4_en_QUERY_PORT(priv->mdev, priv->port);
	mutex_unlock(&mdev->state_lock);
}

static void mlx4_en_linkstate(struct work_struct *work)
{
	struct mlx4_en_priv *priv = container_of(work, struct mlx4_en_priv,
						 linkstate_task);
	struct mlx4_en_dev *mdev = priv->mdev;
	int linkstate = priv->link_state;

	mutex_lock(&mdev->state_lock);
	/* If observable port state changed set carrier state and
	 * report to system log */
	if (priv->last_link_state != linkstate) {
		if (linkstate == MLX4_DEV_EVENT_PORT_DOWN) {
			if_link_state_change(priv->dev, LINK_STATE_DOWN);
		} else {
			en_info(priv, "Link Up\n");
			if_link_state_change(priv->dev, LINK_STATE_UP);
		}
	}
	priv->last_link_state = linkstate;
	mutex_unlock(&mdev->state_lock);
}

static void mlx4_en_lock_and_stop_port(struct work_struct *work)
{
	struct mlx4_en_priv *priv = container_of(work, struct mlx4_en_priv,
						 stop_port_task);
	struct net_device *dev = priv->dev;
	struct mlx4_en_dev *mdev = priv->mdev;
 
	mutex_lock(&mdev->state_lock);
	mlx4_en_do_stop_port(dev);
	mutex_unlock(&mdev->state_lock);
}

static void mlx4_en_lock_and_start_port(struct work_struct *work)
{
	struct mlx4_en_priv *priv = container_of(work, struct mlx4_en_priv,
						 start_port_task);
	struct net_device *dev = priv->dev;
	struct mlx4_en_dev *mdev = priv->mdev;

	mutex_lock(&mdev->state_lock);
	mlx4_en_do_start_port(dev);
	mutex_unlock(&mdev->state_lock);
}

int mlx4_en_do_start_port(struct net_device *dev)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	struct mlx4_en_cq *cq;
	struct mlx4_en_tx_ring *tx_ring;
	u64 config;
	int rx_index = 0;
	int tx_index = 0;
	int err = 0;
	int i;
	int j;

	if (priv->port_up) {
		en_dbg(DRV, priv, "start port called while port already up\n");
		return 0;
	}

	/* Calculate Rx buf size */
	dev->if_mtu = min(dev->if_mtu, priv->max_mtu);
	mlx4_en_calc_rx_buf(dev);
	en_dbg(DRV, priv, "Rx buf size:%d\n", priv->rx_mb_size);

	/* Configure rx cq's and rings */
	err = mlx4_en_activate_rx_rings(priv);
	if (err) {
		en_err(priv, "Failed to activate RX rings\n");
		return err;
	}

	for (i = 0; i < priv->rx_ring_num; i++) {
		cq = &priv->rx_cq[i];

		err = mlx4_en_activate_cq(priv, cq);
		if (err) {
			en_err(priv, "Failed activating Rx CQ\n");
			goto cq_err;
		}
		for (j = 0; j < cq->size; j++)
			cq->buf[j].owner_sr_opcode = MLX4_CQE_OWNER_MASK;
		err = mlx4_en_set_cq_moder(priv, cq);
		if (err) {
			en_err(priv, "Failed setting cq moderation parameters");
			mlx4_en_deactivate_cq(priv, cq);
			goto cq_err;
		}
		mlx4_en_arm_cq(priv, cq);
		priv->rx_ring[i].cqn = cq->mcq.cqn;
		++rx_index;
	}

	err = mlx4_en_config_rss_steer(priv);
	if (err) {
		en_err(priv, "Failed configuring rss steering\n");
		goto cq_err;
	}

	/* Configure tx cq's and rings */
	for (i = 0; i < priv->tx_ring_num; i++) {
		/* Configure cq */
		cq = &priv->tx_cq[i];
		err = mlx4_en_activate_cq(priv, cq);
		if (err) {
			en_err(priv, "Failed allocating Tx CQ\n");
			goto tx_err;
		}
		err = mlx4_en_set_cq_moder(priv, cq);
		if (err) {
			en_err(priv, "Failed setting cq moderation parameters");
			mlx4_en_deactivate_cq(priv, cq);
			goto tx_err;
		}
		en_dbg(DRV, priv, "Resetting index of collapsed CQ:%d to -1\n", i);
		cq->buf->wqe_index = cpu_to_be16(0xffff);

		/* Configure ring */
		tx_ring = &priv->tx_ring[i];
		err = mlx4_en_activate_tx_ring(priv, tx_ring, cq->mcq.cqn);
		if (err) {
			en_err(priv, "Failed allocating Tx ring\n");
			mlx4_en_deactivate_cq(priv, cq);
			goto tx_err;
		}
		/* Set initial ownership of all Tx TXBBs to SW (1) */
		for (j = 0; j < tx_ring->buf_size; j += STAMP_STRIDE)
			*((u32 *) (tx_ring->buf + j)) = 0xffffffff;
		++tx_index;
	}

	/* Configure port */
	err = mlx4_SET_PORT_general(mdev->dev, priv->port,
				    priv->rx_mb_size + ETHER_CRC_LEN,
				    priv->prof->tx_pause,
				    priv->prof->tx_ppp,
				    priv->prof->rx_pause,
				    priv->prof->rx_ppp);
	if (err) {
		en_err(priv, "Failed setting port general configurations "
			     "for port %d, with error %d\n", priv->port, err);
		goto tx_err;
	}
	/* Set default qp number */
	err = mlx4_SET_PORT_qpn_calc(mdev->dev, priv->port, priv->base_qpn, 0);
	if (err) {
		en_err(priv, "Failed setting default qp numbers\n");
		goto tx_err;
	}
	/* Set port mac number */
	en_dbg(DRV, priv, "Setting mac for port %d\n", priv->port);
	err = mlx4_register_mac(mdev->dev, priv->port,
				mlx4_en_mac_to_u64(IF_LLADDR(dev)));
	if (err < 0) {
		en_err(priv, "Failed setting port mac err=%d\n", err);
		goto tx_err;
	}
	mdev->mac_removed[priv->port] = 0;

	/* Init port */
	en_dbg(HW, priv, "Initializing port\n");
	err = mlx4_INIT_PORT(mdev->dev, priv->port);
	if (err) {
		en_err(priv, "Failed Initializing port\n");
		goto mac_err;
	}

	/* Set the various hardware offload abilities */
	dev->if_hwassist = 0;
	if (dev->if_capenable & IFCAP_TSO4)
		dev->if_hwassist |= CSUM_TSO;
	if (dev->if_capenable & IFCAP_TXCSUM)
		dev->if_hwassist |= (CSUM_TCP | CSUM_UDP | CSUM_IP);
	if (dev->if_capenable & IFCAP_RXCSUM)
		priv->rx_csum = 1;
	else
		priv->rx_csum = 0;

	err = mlx4_wol_read(priv->mdev->dev, &config, priv->port);
	if (err) {
		en_err(priv, "Failed to get WoL info, unable to modify\n");
		goto wol_err;
	}
	if (dev->if_capenable & IFCAP_WOL_MAGIC) {
		config |= MLX4_EN_WOL_DO_MODIFY | MLX4_EN_WOL_ENABLED |
		    MLX4_EN_WOL_MAGIC;
	} else {
		config &= ~(MLX4_EN_WOL_ENABLED | MLX4_EN_WOL_MAGIC);
		config |= MLX4_EN_WOL_DO_MODIFY;
	}

	err = mlx4_wol_write(priv->mdev->dev, config, priv->port);
	if (err) {
		en_err(priv, "Failed to set WoL information\n");
		goto wol_err;
	}

	priv->port_up = true;

	/* Populate multicast list */
	mlx4_en_set_multicast(dev);

	/* Enable the queues. */
	dev->if_drv_flags &= ~IFF_DRV_OACTIVE;
	dev->if_drv_flags |= IFF_DRV_RUNNING;

	callout_reset(&priv->watchdog_timer, MLX4_EN_WATCHDOG_TIMEOUT,
	    mlx4_en_watchdog_timeout, priv);

	return 0;

wol_err:
	/* close port*/
	mlx4_CLOSE_PORT(mdev->dev, priv->port);

mac_err:
	mlx4_unregister_mac(mdev->dev, priv->port, priv->mac);
tx_err:
	while (tx_index--) {
		mlx4_en_deactivate_tx_ring(priv, &priv->tx_ring[tx_index]);
		mlx4_en_deactivate_cq(priv, &priv->tx_cq[tx_index]);
	}

	mlx4_en_release_rss_steer(priv);
cq_err:
	while (rx_index--)
		mlx4_en_deactivate_cq(priv, &priv->rx_cq[rx_index]);
	for (i = 0; i < priv->rx_ring_num; i++)
		mlx4_en_deactivate_rx_ring(priv, &priv->rx_ring[i]);

	return err; /* need to close devices */
}


void mlx4_en_do_stop_port(struct net_device *dev)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	int i;

	if (!priv->port_up) {
		en_dbg(DRV, priv, "stop port called while port already down\n");
		return;
	}

	/* Set port as not active */
	priv->port_up = false;

	/* Unregister Mac address for the port */
	mlx4_unregister_mac(mdev->dev, priv->port, priv->mac);
	mdev->mac_removed[priv->port] = 1;

	/* Free TX Rings */
	for (i = 0; i < priv->tx_ring_num; i++) {
		mlx4_en_deactivate_tx_ring(priv, &priv->tx_ring[i]);
		mlx4_en_deactivate_cq(priv, &priv->tx_cq[i]);
	}
	msleep(10);

	for (i = 0; i < priv->tx_ring_num; i++)
		mlx4_en_free_tx_buf(dev, &priv->tx_ring[i]);

	/* Free RSS qps */
	mlx4_en_release_rss_steer(priv);

	/* Free RX Rings */
	for (i = 0; i < priv->rx_ring_num; i++) {
		mlx4_en_deactivate_rx_ring(priv, &priv->rx_ring[i]);
		mlx4_en_deactivate_cq(priv, &priv->rx_cq[i]);
	}

	/* close port*/
	mlx4_CLOSE_PORT(mdev->dev, priv->port);

	callout_stop(&priv->watchdog_timer);

	dev->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
}

static void mlx4_en_restart(struct work_struct *work)
{
	struct mlx4_en_priv *priv = container_of(work, struct mlx4_en_priv,
						 watchdog_task);
	struct mlx4_en_dev *mdev = priv->mdev;
	struct net_device *dev = priv->dev;
	struct mlx4_en_tx_ring *ring;
	int i;

	if (priv->blocked == 0 || priv->port_up == 0)
		return;
	for (i = 0; i < priv->tx_ring_num; i++) {
		ring = &priv->tx_ring[i];
		if (ring->blocked &&
		    ring->watchdog_time + MLX4_EN_WATCHDOG_TIMEOUT < ticks)
			goto reset;
	}
	return;

reset:
	priv->port_stats.tx_timeout++;
	en_dbg(DRV, priv, "Watchdog task called for port %d\n", priv->port);

	mutex_lock(&mdev->state_lock);
	if (priv->port_up) {
		mlx4_en_do_stop_port(dev);
		if (mlx4_en_do_start_port(dev))
			en_err(priv, "Failed restarting port %d\n", priv->port);
	}
	mutex_unlock(&mdev->state_lock);
}


static void
mlx4_en_init(void *arg)
{
	struct mlx4_en_priv *priv;
	struct mlx4_en_dev *mdev;

	priv = arg;
	mdev = priv->mdev;
	mutex_lock(&mdev->state_lock);
	mlx4_en_init_locked(priv);
	mutex_unlock(&mdev->state_lock);
}

static void
mlx4_en_init_locked(struct mlx4_en_priv *priv)
{

	struct mlx4_en_dev *mdev;
	struct ifnet *dev;
	int i;

	dev = priv->dev;
	mdev = priv->mdev;
	if (dev->if_drv_flags & IFF_DRV_RUNNING)
		mlx4_en_do_stop_port(dev);

	if (!mdev->device_up) {
		en_err(priv, "Cannot open - device down/disabled\n");
		return;
	}

	/* Reset HW statistics and performance counters */
	if (mlx4_en_DUMP_ETH_STATS(mdev, priv->port, 1))
		en_dbg(HW, priv, "Failed dumping statistics\n");

	memset(&priv->pstats, 0, sizeof(priv->pstats));

	for (i = 0; i < priv->tx_ring_num; i++) {
		priv->tx_ring[i].bytes = 0;
		priv->tx_ring[i].packets = 0;
	}
	for (i = 0; i < priv->rx_ring_num; i++) {
		priv->rx_ring[i].bytes = 0;
		priv->rx_ring[i].packets = 0;
	}

	mlx4_en_set_default_moderation(priv);
	if (mlx4_en_do_start_port(dev))
		en_err(priv, "Failed starting port:%d\n", priv->port);
}

void mlx4_en_free_resources(struct mlx4_en_priv *priv)
{
	int i;

	for (i = 0; i < priv->tx_ring_num; i++) {
		if (priv->tx_ring[i].tx_info)
			mlx4_en_destroy_tx_ring(priv, &priv->tx_ring[i]);
		if (priv->tx_cq[i].buf)
			mlx4_en_destroy_cq(priv, &priv->tx_cq[i]);
	}

	for (i = 0; i < priv->rx_ring_num; i++) {
		if (priv->rx_ring[i].rx_info)
			mlx4_en_destroy_rx_ring(priv, &priv->rx_ring[i]);
		if (priv->rx_cq[i].buf)
			mlx4_en_destroy_cq(priv, &priv->rx_cq[i]);
	}
	/* Free the stats tree when we resize the rings. */
	if (priv->sysctl)
		sysctl_ctx_free(&priv->stat_ctx);

}

int mlx4_en_alloc_resources(struct mlx4_en_priv *priv)
{
	struct mlx4_en_port_profile *prof = priv->prof;
	int i;

	/* Create tx Rings */
	for (i = 0; i < priv->tx_ring_num; i++) {
		if (mlx4_en_create_cq(priv, &priv->tx_cq[i],
				      prof->tx_ring_size, i, TX))
			goto err;

		if (mlx4_en_create_tx_ring(priv, &priv->tx_ring[i],
					   prof->tx_ring_size, TXBB_SIZE))
			goto err;
	}

	/* Create rx Rings */
	for (i = 0; i < priv->rx_ring_num; i++) {
		if (mlx4_en_create_cq(priv, &priv->rx_cq[i],
				      prof->rx_ring_size, i, RX))
			goto err;

		if (mlx4_en_create_rx_ring(priv, &priv->rx_ring[i],
					   prof->rx_ring_size))
			goto err;
	}

	/* Re-create stat sysctls in case the number of rings changed. */
	mlx4_en_sysctl_stat(priv);

	/* Populate Tx priority mappings */
	mlx4_en_set_prio_map(priv, priv->tx_prio_map,
			     priv->tx_ring_num - MLX4_EN_NUM_HASH_RINGS);

	return 0;

err:
	en_err(priv, "Failed to allocate NIC resources\n");
	return -ENOMEM;
}


void mlx4_en_destroy_netdev(struct net_device *dev)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;

	en_dbg(DRV, priv, "Destroying netdev on port:%d\n", priv->port);

	if (priv->vlan_attach != NULL)
		EVENTHANDLER_DEREGISTER(vlan_config, priv->vlan_attach);
	if (priv->vlan_detach != NULL)
		EVENTHANDLER_DEREGISTER(vlan_unconfig, priv->vlan_detach);

	/* Unregister device - this will close the port if it was up */
	if (priv->registered)
		ether_ifdetach(dev);

	if (priv->allocated)
		mlx4_free_hwq_res(mdev->dev, &priv->res, MLX4_EN_PAGE_SIZE);

	mutex_lock(&mdev->state_lock);
	mlx4_en_do_stop_port(dev);
	mutex_unlock(&mdev->state_lock);

	cancel_delayed_work(&priv->stats_task);
	/* flush any pending task for this netdev */
	flush_workqueue(mdev->workqueue);
	callout_drain(&priv->watchdog_timer);

	/* Detach the netdev so tasks would not attempt to access it */
	mutex_lock(&mdev->state_lock);
	mdev->pndev[priv->port] = NULL;
	mutex_unlock(&mdev->state_lock);

	mlx4_en_free_resources(priv);

	if (priv->sysctl)
		sysctl_ctx_free(&priv->conf_ctx);

	mtx_destroy(&priv->stats_lock.m);
	mtx_destroy(&priv->vlan_lock.m);
	kfree(priv);
	if_free(dev);
}

static int mlx4_en_change_mtu(struct net_device *dev, int new_mtu)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	int err = 0;

	en_dbg(DRV, priv, "Change MTU called - current:%ld new:%d\n",
		 dev->if_mtu, new_mtu);

	if ((new_mtu < MLX4_EN_MIN_MTU) || (new_mtu > priv->max_mtu)) {
		en_err(priv, "Bad MTU size:%d.\n", new_mtu);
		return -EPERM;
	}
	mutex_lock(&mdev->state_lock);
	dev->if_mtu = new_mtu;
	if (dev->if_drv_flags & IFF_DRV_RUNNING) {
		if (!mdev->device_up) {
			/* NIC is probably restarting - let watchdog task reset
			 * the port */
			en_dbg(DRV, priv, "Change MTU called with card down!?\n");
		} else {
			mlx4_en_do_stop_port(dev);
			mlx4_en_set_default_moderation(priv);
			err = mlx4_en_do_start_port(dev);
			if (err) {
				en_err(priv, "Failed restarting port:%d\n",
					 priv->port);
				queue_work(mdev->workqueue, &priv->watchdog_task);
			}
		}
	}
	mutex_unlock(&mdev->state_lock);
	return 0;
}

static int mlx4_en_calc_media(struct mlx4_en_priv *priv)
{
	int trans_type;
	int active;

	active = IFM_ETHER;
	if (priv->last_link_state == MLX4_DEV_EVENT_PORT_DOWN)
		return (active);
	/*
	 * [ShaharK] mlx4_en_QUERY_PORT sleeps and cannot be called under a
	 * non-sleepable lock.
	 * I moved it to the periodic mlx4_en_do_get_stats.
 	if (mlx4_en_QUERY_PORT(priv->mdev, priv->port))
 		return (active);
	*/
	active |= IFM_FDX;
	trans_type = priv->port_state.transciver;
	/* XXX I don't know all of the transceiver values. */
	switch (priv->port_state.link_speed) {
	case 1000:
		active |= IFM_1000_T;
		break;
	case 10000:
		if (trans_type > 0 && trans_type <= 0xC)
			active |= IFM_10G_SR;
		else if (trans_type == 0x80 || trans_type == 0)
			active |= IFM_10G_CX4;
		break;
	case 40000:
		active |= IFM_40G_CR4;
		break;
	}
	if (priv->prof->tx_pause)
		active |= IFM_ETH_TXPAUSE;
	if (priv->prof->rx_pause)
		active |= IFM_ETH_RXPAUSE;

	return (active);
}


static void mlx4_en_media_status(struct ifnet *dev, struct ifmediareq *ifmr)
{
	struct mlx4_en_priv *priv;

	priv = dev->if_softc;
	ifmr->ifm_status = IFM_AVALID;
	if (priv->last_link_state != MLX4_DEV_EVENT_PORT_DOWN)
		ifmr->ifm_status |= IFM_ACTIVE;
	ifmr->ifm_active = mlx4_en_calc_media(priv);

	return;
}

static int mlx4_en_media_change(struct ifnet *dev)
{
	struct mlx4_en_priv *priv;
        struct ifmedia *ifm;
	int rxpause;
	int txpause;
	int error;

	priv = dev->if_softc;
	ifm = &priv->media;
	rxpause = txpause = 0;
	error = 0;

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return (EINVAL);
        switch (IFM_SUBTYPE(ifm->ifm_media)) {
        case IFM_AUTO:
		break;
	case IFM_10G_SR:
	case IFM_10G_CX4:
	case IFM_1000_T:
		if (IFM_SUBTYPE(ifm->ifm_media) ==
		    IFM_SUBTYPE(mlx4_en_calc_media(priv)) &&
		    (ifm->ifm_media & IFM_FDX))
			break;
		/* Fallthrough */
	default:
                printf("%s: Only auto media type\n", if_name(dev));
                return (EINVAL);
	}
	/* Allow user to set/clear pause */
	if (IFM_OPTIONS(ifm->ifm_media) & IFM_ETH_RXPAUSE)
		rxpause = 1;
	if (IFM_OPTIONS(ifm->ifm_media) & IFM_ETH_TXPAUSE)
		txpause = 1;
	if (priv->prof->tx_pause != txpause || priv->prof->rx_pause != rxpause) {
		priv->prof->tx_pause = txpause;
		priv->prof->rx_pause = rxpause;
		error = -mlx4_SET_PORT_general(priv->mdev->dev, priv->port,
		     priv->rx_mb_size + ETHER_CRC_LEN, priv->prof->tx_pause,
		     priv->prof->tx_ppp, priv->prof->rx_pause,
		     priv->prof->rx_ppp);
	}
	return (error);
}

static int mlx4_en_ioctl(struct ifnet *dev, u_long command, caddr_t data)
{
	struct mlx4_en_priv *priv;
	struct mlx4_en_dev *mdev;
	struct ifreq *ifr;
	int error;
	int mask;

	error = 0;
	mask = 0;
	priv = dev->if_softc;
	mdev = priv->mdev;
	ifr = (struct ifreq *) data;
	switch (command) {
	case SIOCSIFMTU:
		error = -mlx4_en_change_mtu(dev, ifr->ifr_mtu);
		break;
	case SIOCSIFFLAGS:
		if (dev->if_flags & IFF_UP) {
			if ((dev->if_drv_flags & IFF_DRV_RUNNING) == 0)
				mlx4_en_start_port(dev);
			else
				mlx4_en_set_multicast(dev);
		} else {
			if (dev->if_drv_flags & IFF_DRV_RUNNING) {
				mlx4_en_stop_port(dev);
                                if_link_state_change(dev, LINK_STATE_DOWN);
                                /*
				 * Since mlx4_en_stop_port is defered we
				 * have to wait till it's finished.
				 */
                                for (int count=0; count<10; count++) {
                                        if (dev->if_drv_flags & IFF_DRV_RUNNING) {
                                                DELAY(20000);
                                        } else {
                                                break;
                                        }
                                }
			}
		}
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		mlx4_en_set_multicast(dev);
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(dev, ifr, &priv->media, command);
		break;
	case SIOCSIFCAP:
		mutex_lock(&mdev->state_lock);
		mask = ifr->ifr_reqcap ^ dev->if_capenable;
		if (mask & IFCAP_HWCSUM)
			dev->if_capenable ^= IFCAP_HWCSUM;
		if (mask & IFCAP_TSO4)
			dev->if_capenable ^= IFCAP_TSO4;
		if (mask & IFCAP_LRO)
			dev->if_capenable ^= IFCAP_LRO;
		if (mask & IFCAP_VLAN_HWTAGGING)
			dev->if_capenable ^= IFCAP_VLAN_HWTAGGING;
		if (mask & IFCAP_VLAN_HWFILTER)
			dev->if_capenable ^= IFCAP_VLAN_HWFILTER;
		if (mask & IFCAP_WOL_MAGIC)
			dev->if_capenable ^= IFCAP_WOL_MAGIC;
		if (dev->if_drv_flags & IFF_DRV_RUNNING)
			mlx4_en_init_locked(priv);
		mutex_unlock(&mdev->state_lock);
		VLAN_CAPABILITIES(dev);
		break;
	default:
		error = ether_ioctl(dev, command, data);
		break;
	}

	return (error);
}

static int mlx4_en_set_ring_size(struct net_device *dev,
    int rx_size, int tx_size)
{
	struct mlx4_en_priv *priv = netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	int port_up = 0;
	int err = 0;

	rx_size = roundup_pow_of_two(rx_size);
	rx_size = max_t(u32, rx_size, MLX4_EN_MIN_RX_SIZE);
	rx_size = min_t(u32, rx_size, MLX4_EN_MAX_RX_SIZE);
	tx_size = roundup_pow_of_two(tx_size);
	tx_size = max_t(u32, tx_size, MLX4_EN_MIN_TX_SIZE);
	tx_size = min_t(u32, tx_size, MLX4_EN_MAX_TX_SIZE);

	if (rx_size == (priv->port_up ?
	    priv->rx_ring[0].actual_size : priv->rx_ring[0].size) &&
	    tx_size == priv->tx_ring[0].size)
		return 0;

	mutex_lock(&mdev->state_lock);
	if (priv->port_up) {
		port_up = 1;
		mlx4_en_do_stop_port(dev);
	}
	mlx4_en_free_resources(priv);
	priv->prof->tx_ring_size = tx_size;
	priv->prof->rx_ring_size = rx_size;
	err = mlx4_en_alloc_resources(priv);
	if (err) {
		en_err(priv, "Failed reallocating port resources\n");
		goto out;
	}
	if (port_up) {
		err = mlx4_en_do_start_port(dev);
		if (err)
			en_err(priv, "Failed starting port\n");
	}
out:
	mutex_unlock(&mdev->state_lock);
	return err;
}

static int mlx4_en_set_rx_ring_size(SYSCTL_HANDLER_ARGS)
{
	struct mlx4_en_priv *priv;
	int size;
	int error;

	priv = arg1;
	size = priv->prof->rx_ring_size;
	error = sysctl_handle_int(oidp, &size, 0, req);
	if (error || !req->newptr)
		return (error);
	error = -mlx4_en_set_ring_size(priv->dev, size,
	    priv->prof->tx_ring_size);

	return (error);
}

static int mlx4_en_set_tx_ring_size(SYSCTL_HANDLER_ARGS)
{
	struct mlx4_en_priv *priv;
	int size;
	int error;

	priv = arg1;
	size = priv->prof->tx_ring_size;
	error = sysctl_handle_int(oidp, &size, 0, req);
	if (error || !req->newptr)
		return (error);
	error = -mlx4_en_set_ring_size(priv->dev, priv->prof->rx_ring_size,
	    size);

	return (error);
}

static int mlx4_en_set_tx_ppp(SYSCTL_HANDLER_ARGS)
{
	struct mlx4_en_priv *priv;
	int ppp;
	int error;

	priv = arg1;
	ppp = priv->prof->tx_ppp;
	error = sysctl_handle_int(oidp, &ppp, 0, req);
	if (error || !req->newptr)
		return (error);
	if (ppp > 0xff || ppp < 0)
		return (-EINVAL);
	priv->prof->tx_ppp = ppp;
	error = -mlx4_SET_PORT_general(priv->mdev->dev, priv->port,
				       priv->rx_mb_size + ETHER_CRC_LEN,
				       priv->prof->tx_pause,
				       priv->prof->tx_ppp,
				       priv->prof->rx_pause,
				       priv->prof->rx_ppp);

	return (error);
}

static int mlx4_en_set_rx_ppp(SYSCTL_HANDLER_ARGS)
{
	struct mlx4_en_priv *priv;
	struct mlx4_en_dev *mdev;
	int tx_ring_num;
	int ppp;
	int error;
	int port_up;

	port_up = 0;
	priv = arg1;
	mdev = priv->mdev;
	ppp = priv->prof->rx_ppp;
	error = sysctl_handle_int(oidp, &ppp, 0, req);
	if (error || !req->newptr)
		return (error);
	if (ppp > 0xff || ppp < 0)
		return (-EINVAL);
	/* See if we have to change the number of tx queues. */
	if (!ppp != !priv->prof->rx_ppp) {
		tx_ring_num = MLX4_EN_NUM_HASH_RINGS + 1 +
		    (!!ppp) * MLX4_EN_NUM_PPP_RINGS;
		mutex_lock(&mdev->state_lock);
		if (priv->port_up) {
			port_up = 1;
			mlx4_en_do_stop_port(priv->dev);
		}
		mlx4_en_free_resources(priv);
		priv->tx_ring_num = tx_ring_num;
		priv->prof->rx_ppp = ppp;
		error = -mlx4_en_alloc_resources(priv);
		if (error)
			en_err(priv, "Failed reallocating port resources\n");
		if (error == 0 && port_up) {
			error = -mlx4_en_do_start_port(priv->dev);
			if (error)
				en_err(priv, "Failed starting port\n");
		}
		mutex_unlock(&mdev->state_lock);
		return (error);

	}
	priv->prof->rx_ppp = ppp;
	error = -mlx4_SET_PORT_general(priv->mdev->dev, priv->port,
				       priv->rx_mb_size + ETHER_CRC_LEN,
				       priv->prof->tx_pause,
				       priv->prof->tx_ppp,
				       priv->prof->rx_pause,
				       priv->prof->rx_ppp);

	return (error);
}

static void mlx4_en_sysctl_conf(struct mlx4_en_priv *priv)
{
	struct net_device *dev;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *node;
	struct sysctl_oid_list *node_list;
	struct sysctl_oid *coal;
	struct sysctl_oid_list *coal_list;

	dev = priv->dev;
	ctx = &priv->conf_ctx;

	sysctl_ctx_init(ctx);
	priv->sysctl = SYSCTL_ADD_NODE(ctx, SYSCTL_STATIC_CHILDREN(_hw),
	    OID_AUTO, dev->if_xname, CTLFLAG_RD, 0, "mlx4 10gig ethernet");
	node = SYSCTL_ADD_NODE(ctx, SYSCTL_CHILDREN(priv->sysctl), OID_AUTO,
	    "conf", CTLFLAG_RD, NULL, "Configuration");
	node_list = SYSCTL_CHILDREN(node);

	SYSCTL_ADD_UINT(ctx, node_list, OID_AUTO, "msg_enable",
	    CTLFLAG_RW, &priv->msg_enable, 0,
	    "Driver message enable bitfield");
	SYSCTL_ADD_UINT(ctx, node_list, OID_AUTO, "rx_rings",
	    CTLTYPE_INT | CTLFLAG_RD, &priv->rx_ring_num, 0,
	    "Number of receive rings");
	SYSCTL_ADD_UINT(ctx, node_list, OID_AUTO, "tx_rings",
	    CTLTYPE_INT | CTLFLAG_RD, &priv->tx_ring_num, 0,
	    "Number of transmit rings");
	SYSCTL_ADD_PROC(ctx, node_list, OID_AUTO, "rx_size",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, priv, 0,
	    mlx4_en_set_rx_ring_size, "I", "Receive ring size");
	SYSCTL_ADD_PROC(ctx, node_list, OID_AUTO, "tx_size",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, priv, 0,
	    mlx4_en_set_tx_ring_size, "I", "Transmit ring size");
	SYSCTL_ADD_UINT(ctx, node_list, OID_AUTO, "ip_reasm",
	    CTLFLAG_RW, &priv->ip_reasm, 0,
	    "Allow reassembly of IP fragments.");
	SYSCTL_ADD_PROC(ctx, node_list, OID_AUTO, "tx_ppp",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, priv, 0,
	    mlx4_en_set_tx_ppp, "I", "TX Per-priority pause");
	SYSCTL_ADD_PROC(ctx, node_list, OID_AUTO, "rx_ppp",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE, priv, 0,
	    mlx4_en_set_rx_ppp, "I", "RX Per-priority pause");

	/* Add coalescer configuration. */
	coal = SYSCTL_ADD_NODE(ctx, node_list, OID_AUTO,
	    "coalesce", CTLFLAG_RD, NULL, "Interrupt coalesce configuration");
	coal_list = SYSCTL_CHILDREN(node);
	SYSCTL_ADD_UINT(ctx, coal_list, OID_AUTO, "pkt_rate_low",
	    CTLFLAG_RW, &priv->pkt_rate_low, 0,
	    "Packets per-second for minimum delay");
	SYSCTL_ADD_UINT(ctx, coal_list, OID_AUTO, "rx_usecs_low",
	    CTLFLAG_RW, &priv->rx_usecs_low, 0,
	    "Minimum RX delay in micro-seconds");
	SYSCTL_ADD_UINT(ctx, coal_list, OID_AUTO, "pkt_rate_high",
	    CTLFLAG_RW, &priv->pkt_rate_high, 0,
	    "Packets per-second for maximum delay");
	SYSCTL_ADD_UINT(ctx, coal_list, OID_AUTO, "rx_usecs_high",
	    CTLFLAG_RW, &priv->rx_usecs_high, 0,
	    "Maximum RX delay in micro-seconds");
	SYSCTL_ADD_UINT(ctx, coal_list, OID_AUTO, "sample_interval",
	    CTLFLAG_RW, &priv->sample_interval, 0,
	    "adaptive frequency in units of HZ ticks");
	SYSCTL_ADD_UINT(ctx, coal_list, OID_AUTO, "adaptive_rx_coal",
	    CTLFLAG_RW, &priv->adaptive_rx_coal, 0,
	    "Enable adaptive rx coalescing");
}

static void mlx4_en_sysctl_stat(struct mlx4_en_priv *priv)
{
	struct net_device *dev;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *node;
	struct sysctl_oid_list *node_list;
	struct sysctl_oid *ring_node;
	struct sysctl_oid_list *ring_list;
	struct mlx4_en_tx_ring *tx_ring;
	struct mlx4_en_rx_ring *rx_ring;
	char namebuf[128];
	int i;

	dev = priv->dev;

	ctx = &priv->stat_ctx;
	sysctl_ctx_init(ctx);
	node = SYSCTL_ADD_NODE(ctx, SYSCTL_CHILDREN(priv->sysctl), OID_AUTO,
	    "stat", CTLFLAG_RD, NULL, "Statistics");
	node_list = SYSCTL_CHILDREN(node);

#ifdef MLX4_EN_PERF_STAT
	SYSCTL_ADD_UINT(ctx, node_list, OID_AUTO, "tx_poll", CTLFLAG_RD,
	    &priv->pstats.tx_poll, "TX Poll calls");
	SYSCTL_ADD_QUAD(ctx, node_list, OID_AUTO, "tx_pktsz_avg", CTLFLAG_RD,
	    &priv->pstats.tx_pktsz_avg, "TX average packet size");
	SYSCTL_ADD_UINT(ctx, node_list, OID_AUTO, "inflight_avg", CTLFLAG_RD,
	    &priv->pstats.inflight_avg, "TX average packets in-flight");
	SYSCTL_ADD_UINT(ctx, node_list, OID_AUTO, "tx_coal_avg", CTLFLAG_RD,
	    &priv->pstats.tx_coal_avg, "TX average coalesced completions");
	SYSCTL_ADD_UINT(ctx, node_list, OID_AUTO, "rx_coal_avg", CTLFLAG_RD,
	    &priv->pstats.rx_coal_avg, "RX average coalesced completions");
#endif

	SYSCTL_ADD_ULONG(ctx, node_list, OID_AUTO, "tso_packets", CTLFLAG_RD,
	    &priv->port_stats.tso_packets, "TSO packets sent");
	SYSCTL_ADD_ULONG(ctx, node_list, OID_AUTO, "queue_stopped", CTLFLAG_RD,
	    &priv->port_stats.queue_stopped, "Queue full");
	SYSCTL_ADD_ULONG(ctx, node_list, OID_AUTO, "wake_queue", CTLFLAG_RD,
	    &priv->port_stats.wake_queue, "Queue resumed after full");
	SYSCTL_ADD_ULONG(ctx, node_list, OID_AUTO, "tx_timeout", CTLFLAG_RD,
	    &priv->port_stats.tx_timeout, "Transmit timeouts");
	SYSCTL_ADD_ULONG(ctx, node_list, OID_AUTO, "rx_alloc_failed", CTLFLAG_RD,
	    &priv->port_stats.rx_alloc_failed, "RX failed to allocate mbuf");
	SYSCTL_ADD_ULONG(ctx, node_list, OID_AUTO, "rx_chksum_good", CTLFLAG_RD,
	    &priv->port_stats.rx_chksum_good, "RX checksum offload success");
	SYSCTL_ADD_ULONG(ctx, node_list, OID_AUTO, "rx_chksum_none", CTLFLAG_RD,
	    &priv->port_stats.rx_chksum_none, "RX without checksum offload");
	SYSCTL_ADD_ULONG(ctx, node_list, OID_AUTO, "tx_chksum_offload",
	    CTLFLAG_RD, &priv->port_stats.tx_chksum_offload,
	    "TX checksum offloads");

	/* Could strdup the names and add in a loop.  This is simpler. */
	SYSCTL_ADD_ULONG(ctx, node_list, OID_AUTO, "broadcast", CTLFLAG_RD,
	    &priv->pkstats.broadcast, "Broadcast packets");
	SYSCTL_ADD_ULONG(ctx, node_list, OID_AUTO, "tx_prio0", CTLFLAG_RD,
	    &priv->pkstats.tx_prio[0], "TX Priority 0 packets");
	SYSCTL_ADD_ULONG(ctx, node_list, OID_AUTO, "tx_prio1", CTLFLAG_RD,
	    &priv->pkstats.tx_prio[1], "TX Priority 1 packets");
	SYSCTL_ADD_ULONG(ctx, node_list, OID_AUTO, "tx_prio2", CTLFLAG_RD,
	    &priv->pkstats.tx_prio[2], "TX Priority 2 packets");
	SYSCTL_ADD_ULONG(ctx, node_list, OID_AUTO, "tx_prio3", CTLFLAG_RD,
	    &priv->pkstats.tx_prio[3], "TX Priority 3 packets");
	SYSCTL_ADD_ULONG(ctx, node_list, OID_AUTO, "tx_prio4", CTLFLAG_RD,
	    &priv->pkstats.tx_prio[4], "TX Priority 4 packets");
	SYSCTL_ADD_ULONG(ctx, node_list, OID_AUTO, "tx_prio5", CTLFLAG_RD,
	    &priv->pkstats.tx_prio[5], "TX Priority 5 packets");
	SYSCTL_ADD_ULONG(ctx, node_list, OID_AUTO, "tx_prio6", CTLFLAG_RD,
	    &priv->pkstats.tx_prio[6], "TX Priority 6 packets");
	SYSCTL_ADD_ULONG(ctx, node_list, OID_AUTO, "tx_prio7", CTLFLAG_RD,
	    &priv->pkstats.tx_prio[7], "TX Priority 7 packets");
	SYSCTL_ADD_ULONG(ctx, node_list, OID_AUTO, "rx_prio0", CTLFLAG_RD,
	    &priv->pkstats.rx_prio[0], "RX Priority 0 packets");
	SYSCTL_ADD_ULONG(ctx, node_list, OID_AUTO, "rx_prio1", CTLFLAG_RD,
	    &priv->pkstats.rx_prio[1], "RX Priority 1 packets");
	SYSCTL_ADD_ULONG(ctx, node_list, OID_AUTO, "rx_prio2", CTLFLAG_RD,
	    &priv->pkstats.rx_prio[2], "RX Priority 2 packets");
	SYSCTL_ADD_ULONG(ctx, node_list, OID_AUTO, "rx_prio3", CTLFLAG_RD,
	    &priv->pkstats.rx_prio[3], "RX Priority 3 packets");
	SYSCTL_ADD_ULONG(ctx, node_list, OID_AUTO, "rx_prio4", CTLFLAG_RD,
	    &priv->pkstats.rx_prio[4], "RX Priority 4 packets");
	SYSCTL_ADD_ULONG(ctx, node_list, OID_AUTO, "rx_prio5", CTLFLAG_RD,
	    &priv->pkstats.rx_prio[5], "RX Priority 5 packets");
	SYSCTL_ADD_ULONG(ctx, node_list, OID_AUTO, "rx_prio6", CTLFLAG_RD,
	    &priv->pkstats.rx_prio[6], "RX Priority 6 packets");
	SYSCTL_ADD_ULONG(ctx, node_list, OID_AUTO, "rx_prio7", CTLFLAG_RD,
	    &priv->pkstats.rx_prio[7], "RX Priority 7 packets");

	for (i = 0; i < priv->tx_ring_num; i++) {
		tx_ring = &priv->tx_ring[i];
		snprintf(namebuf, sizeof(namebuf), "tx_ring%d", i);
		ring_node = SYSCTL_ADD_NODE(ctx, node_list, OID_AUTO, namebuf,
		    CTLFLAG_RD, NULL, "TX Ring");
		ring_list = SYSCTL_CHILDREN(ring_node);
		SYSCTL_ADD_ULONG(ctx, ring_list, OID_AUTO, "packets",
		    CTLFLAG_RD, &tx_ring->packets, "TX packets");
		SYSCTL_ADD_ULONG(ctx, ring_list, OID_AUTO, "bytes",
		    CTLFLAG_RD, &tx_ring->bytes, "TX bytes");
		SYSCTL_ADD_ULONG(ctx, ring_list, OID_AUTO, "error",
		    CTLFLAG_RD, &tx_ring->errors, "TX soft errors");

	}
	for (i = 0; i < priv->rx_ring_num; i++) {
		rx_ring = &priv->rx_ring[i];
		snprintf(namebuf, sizeof(namebuf), "rx_ring%d", i);
		ring_node = SYSCTL_ADD_NODE(ctx, node_list, OID_AUTO, namebuf,
		    CTLFLAG_RD, NULL, "RX Ring");
		ring_list = SYSCTL_CHILDREN(ring_node);
		SYSCTL_ADD_ULONG(ctx, ring_list, OID_AUTO, "packets",
		    CTLFLAG_RD, &rx_ring->packets, "RX packets");
		SYSCTL_ADD_ULONG(ctx, ring_list, OID_AUTO, "bytes",
		    CTLFLAG_RD, &rx_ring->bytes, "RX bytes");
		SYSCTL_ADD_ULONG(ctx, ring_list, OID_AUTO, "error",
		    CTLFLAG_RD, &rx_ring->errors, "RX soft errors");
		SYSCTL_ADD_UINT(ctx, ring_list, OID_AUTO, "lro_queued",
		    CTLFLAG_RD, &rx_ring->lro.lro_queued, 0, "LRO Queued");
		SYSCTL_ADD_UINT(ctx, ring_list, OID_AUTO, "lro_flushed",
		    CTLFLAG_RD, &rx_ring->lro.lro_flushed, 0, "LRO Flushed");
	}
}

int mlx4_en_init_netdev(struct mlx4_en_dev *mdev, int port,
			struct mlx4_en_port_profile *prof)
{
	static volatile int mlx4_en_unit;
	struct net_device *dev;
	struct mlx4_en_priv *priv;
	uint8_t dev_addr[ETHER_ADDR_LEN];
	int err;
	int i;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	dev = priv->dev = if_alloc(IFT_ETHER);
	if (dev == NULL) {
		mlx4_err(mdev, "Net device allocation failed\n");
		kfree(priv);
		return -ENOMEM;
	}
	dev->if_softc = priv;
	if_initname(dev, "mlxen", atomic_fetchadd_int(&mlx4_en_unit, 1));
	dev->if_mtu = ETHERMTU;
	dev->if_baudrate = 1000000000;
	dev->if_init = mlx4_en_init;
	dev->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	dev->if_ioctl = mlx4_en_ioctl;
	dev->if_transmit = mlx4_en_transmit;
	dev->if_qflush = mlx4_en_qflush;
	dev->if_snd.ifq_maxlen = prof->tx_ring_size;

	/*
	 * Initialize driver private data
	 */
	priv->dev = dev;
	priv->mdev = mdev;
	priv->prof = prof;
	priv->port = port;
	priv->port_up = false;
	priv->rx_csum = 1;
	priv->flags = prof->flags;
	priv->tx_ring_num = prof->tx_ring_num;
	priv->rx_ring_num = prof->rx_ring_num;
	priv->mac_index = -1;
	priv->msg_enable = MLX4_EN_MSG_LEVEL;
	priv->ip_reasm = priv->mdev->profile.ip_reasm;
	mtx_init(&priv->stats_lock.m, "mlx4 stats", NULL, MTX_DEF);
	mtx_init(&priv->vlan_lock.m, "mlx4 vlan", NULL, MTX_DEF);
	INIT_WORK(&priv->start_port_task, mlx4_en_lock_and_start_port);
	INIT_WORK(&priv->stop_port_task, mlx4_en_lock_and_stop_port);
	INIT_WORK(&priv->mcast_task, mlx4_en_do_set_multicast);
	INIT_WORK(&priv->watchdog_task, mlx4_en_restart);
	INIT_WORK(&priv->linkstate_task, mlx4_en_linkstate);
	INIT_DELAYED_WORK(&priv->stats_task, mlx4_en_do_get_stats);
	callout_init(&priv->watchdog_timer, 1);

	/* Query for default mac and max mtu */
	priv->max_mtu = mdev->dev->caps.eth_mtu_cap[priv->port];
	priv->mac = mdev->dev->caps.def_mac[priv->port];

	if (ILLEGAL_MAC(priv->mac)) {
		en_err(priv, "Port: %d, invalid mac burned: 0x%llx, quiting\n",
			 priv->port, priv->mac);
		err = -EINVAL;
		goto out;
	}

	mlx4_en_sysctl_conf(priv);

	err = mlx4_en_alloc_resources(priv);
	if (err)
		goto out;

	/* Allocate page for receive rings */
	err = mlx4_alloc_hwq_res(mdev->dev, &priv->res,
				MLX4_EN_PAGE_SIZE, MLX4_EN_PAGE_SIZE);
	if (err) {
		en_err(priv, "Failed to allocate page for rx qps\n");
		goto out;
	}
	priv->allocated = 1;

	/*
	 * Set driver features
	 */
	dev->if_capabilities |= IFCAP_RXCSUM | IFCAP_TXCSUM;
	dev->if_capabilities |= IFCAP_VLAN_MTU | IFCAP_VLAN_HWTAGGING;
	dev->if_capabilities |= IFCAP_VLAN_HWCSUM | IFCAP_VLAN_HWFILTER;
	dev->if_capabilities |= IFCAP_LINKSTATE | IFCAP_JUMBO_MTU;
	if (mdev->LSO_support)
		dev->if_capabilities |= IFCAP_TSO4 | IFCAP_VLAN_HWTSO;
	if (mdev->profile.num_lro)
		dev->if_capabilities |= IFCAP_LRO;
	dev->if_capenable = dev->if_capabilities;
	/*
	 * Setup wake-on-lan.
	 */
#if 0
	if (priv->mdev->dev->caps.wol) {
		u64 config;
		if (mlx4_wol_read(priv->mdev->dev, &config, priv->port) == 0) {
			if (config & MLX4_EN_WOL_MAGIC)
				dev->if_capabilities |= IFCAP_WOL_MAGIC;
			if (config & MLX4_EN_WOL_ENABLED)
				dev->if_capenable |= IFCAP_WOL_MAGIC;
		}
	}
#endif

        /* Register for VLAN events */
	priv->vlan_attach = EVENTHANDLER_REGISTER(vlan_config,
            mlx4_en_vlan_rx_add_vid, priv, EVENTHANDLER_PRI_FIRST);
	priv->vlan_detach = EVENTHANDLER_REGISTER(vlan_unconfig,
            mlx4_en_vlan_rx_kill_vid, priv, EVENTHANDLER_PRI_FIRST);

	mdev->pndev[priv->port] = dev;

	priv->last_link_state = MLX4_DEV_EVENT_PORT_DOWN;
	if_link_state_change(dev, LINK_STATE_DOWN);

	/* Set default MAC */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		dev_addr[ETHER_ADDR_LEN - 1 - i] = (u8) (priv->mac >> (8 * i));

	ether_ifattach(dev, dev_addr);
	ifmedia_init(&priv->media, IFM_IMASK | IFM_ETH_FMASK,
	    mlx4_en_media_change, mlx4_en_media_status);
	ifmedia_add(&priv->media, IFM_ETHER | IFM_FDX | IFM_1000_T, 0, NULL);
	ifmedia_add(&priv->media, IFM_ETHER | IFM_FDX | IFM_10G_SR, 0, NULL);
	ifmedia_add(&priv->media, IFM_ETHER | IFM_FDX | IFM_10G_CX4, 0, NULL);
	ifmedia_add(&priv->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&priv->media, IFM_ETHER | IFM_AUTO);

	en_warn(priv, "Using %d TX rings\n", prof->tx_ring_num);
	en_warn(priv, "Using %d RX rings\n", prof->rx_ring_num);

	priv->registered = 1;
	queue_delayed_work(mdev->workqueue, &priv->stats_task, STATS_DELAY);

	return 0;

out:
	mlx4_en_destroy_netdev(dev);
	return err;
}

