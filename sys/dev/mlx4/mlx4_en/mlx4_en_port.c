/*
 * Copyright (c) 2007, 2014 Mellanox Technologies. All rights reserved.
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

#include <sys/types.h>
#include <linux/if_vlan.h>

#include <dev/mlx4/device.h>
#include <dev/mlx4/cmd.h>

#include "en_port.h"
#include "en.h"


int mlx4_SET_VLAN_FLTR(struct mlx4_dev *dev, struct mlx4_en_priv *priv)
{
	struct mlx4_cmd_mailbox *mailbox;
	struct mlx4_set_vlan_fltr_mbox *filter;
	int i;
	int j;
	int index = 0;
	u32 entry;
	int err = 0;

	mailbox = mlx4_alloc_cmd_mailbox(dev);
	if (IS_ERR(mailbox))
		return PTR_ERR(mailbox);

	filter = mailbox->buf;
	for (i = VLAN_FLTR_SIZE - 1; i >= 0; i--) {
		entry = 0;
		for (j = 0; j < 32; j++) {
			if (test_bit(index, priv->active_vlans))
				entry |= 1 << j;
			index++;
		}
		filter->entry[i] = cpu_to_be32(entry);
	}
	err = mlx4_cmd(dev, mailbox->dma, priv->port, 0, MLX4_CMD_SET_VLAN_FLTR,
		       MLX4_CMD_TIME_CLASS_B, MLX4_CMD_WRAPPED);
	mlx4_free_cmd_mailbox(dev, mailbox);
	return err;
}

int mlx4_en_QUERY_PORT(struct mlx4_en_dev *mdev, u8 port)
{
	struct mlx4_en_query_port_context *qport_context;
	struct mlx4_en_priv *priv = mlx4_netdev_priv(mdev->pndev[port]);
	struct mlx4_en_port_state *state = &priv->port_state;
	struct mlx4_cmd_mailbox *mailbox;
	int err;

	mailbox = mlx4_alloc_cmd_mailbox(mdev->dev);
	if (IS_ERR(mailbox))
		return PTR_ERR(mailbox);
	err = mlx4_cmd_box(mdev->dev, 0, mailbox->dma, port, 0,
			   MLX4_CMD_QUERY_PORT, MLX4_CMD_TIME_CLASS_B,
			   MLX4_CMD_WRAPPED);
	if (err)
		goto out;
	qport_context = mailbox->buf;

	/* This command is always accessed from Ethtool context
	 * already synchronized, no need in locking */
	state->link_state = !!(qport_context->link_up & MLX4_EN_LINK_UP_MASK);
	switch (qport_context->link_speed & MLX4_EN_SPEED_MASK) {
	case MLX4_EN_100M_SPEED:
		state->link_speed = 100;
		break;
	case MLX4_EN_1G_SPEED:
		state->link_speed = 1000;
		break;
	case MLX4_EN_10G_SPEED_XAUI:
	case MLX4_EN_10G_SPEED_XFI:
		state->link_speed = 10000;
		break;
	case MLX4_EN_20G_SPEED:
		state->link_speed = 20000;
		break;
	case MLX4_EN_40G_SPEED:
		state->link_speed = 40000;
		break;
	case MLX4_EN_56G_SPEED:
		state->link_speed = 56000;
		break;
	default:
		state->link_speed = -1;
		break;
	}

	state->transceiver = qport_context->transceiver;

	state->flags = 0; /* Reset and recalculate the port flags */
	state->flags |= (qport_context->link_up & MLX4_EN_ANC_MASK) ?
		MLX4_EN_PORT_ANC : 0;
	state->flags |= (qport_context->autoneg & MLX4_EN_AUTONEG_MASK) ?
		MLX4_EN_PORT_ANE : 0;

out:
	mlx4_free_cmd_mailbox(mdev->dev, mailbox);
	return err;
}

/* Each counter set is located in struct mlx4_en_stat_out_mbox
 * with a const offset between its prio components.
 * This function runs over a counter set and sum all of it's prio components.
 */
static u64 en_stats_adder(__be64 *start, __be64 *next, int num)
{
	__be64 *curr = start;
	u64 ret = 0;
	int i;
	int offset = next - start;

	for (i = 0; i < num; i++) {
		ret += be64_to_cpu(*curr);
		curr += offset;
	}

	return ret;
}

static void mlx4_en_fold_software_stats(if_t dev)
{
	struct mlx4_en_priv *priv = mlx4_netdev_priv(dev);
	struct mlx4_en_dev *mdev = priv->mdev;
	u64 packets, bytes;
	int i;

	if (!priv->port_up || mlx4_is_master(mdev->dev))
		return;

	packets = 0;
	bytes = 0;
	for (i = 0; i < priv->rx_ring_num; i++) {
		const struct mlx4_en_rx_ring *ring = priv->rx_ring[i];

		packets += READ_ONCE(ring->packets);
		bytes += READ_ONCE(ring->bytes);
	}
	priv->pkstats.rx_packets = packets;
	priv->pkstats.rx_bytes = bytes;

	packets = 0;
	bytes = 0;
	for (i = 0; i < priv->tx_ring_num; i++) {
		const struct mlx4_en_tx_ring *ring = priv->tx_ring[i];

		packets += READ_ONCE(ring->packets);
		bytes += READ_ONCE(ring->bytes);
	}
	priv->pkstats.tx_packets = packets;
	priv->pkstats.tx_bytes = bytes;
}

int mlx4_en_DUMP_ETH_STATS(struct mlx4_en_dev *mdev, u8 port, u8 reset)
{
	struct mlx4_counter tmp_vport_stats;
	struct mlx4_en_stat_out_mbox *mlx4_en_stats;
	struct mlx4_en_stat_out_flow_control_mbox *flowstats;
	if_t dev = mdev->pndev[port];
	struct mlx4_en_priv *priv = mlx4_netdev_priv(dev);
	struct mlx4_en_vport_stats *vport_stats = &priv->vport_stats;
	struct mlx4_cmd_mailbox *mailbox;
	u64 in_mod = reset << 8 | port;
	int err;
	int i, counter_index;

	mailbox = mlx4_alloc_cmd_mailbox(mdev->dev);
	if (IS_ERR(mailbox))
		return PTR_ERR(mailbox);
	err = mlx4_cmd_box(mdev->dev, 0, mailbox->dma, in_mod, 0,
			   MLX4_CMD_DUMP_ETH_STATS, MLX4_CMD_TIME_CLASS_B,
			   MLX4_CMD_NATIVE);
	if (err)
		goto out;

	mlx4_en_stats = mailbox->buf;

	spin_lock(&priv->stats_lock);

	priv->port_stats.rx_chksum_good = 0;
	priv->port_stats.rx_chksum_none = 0;
	for (i = 0; i < priv->rx_ring_num; i++) {
		priv->port_stats.rx_chksum_good += priv->rx_ring[i]->csum_ok;
		priv->port_stats.rx_chksum_none += priv->rx_ring[i]->csum_none;
	}
	priv->port_stats.tx_chksum_offload = 0;
	priv->port_stats.queue_stopped = 0;
	priv->port_stats.wake_queue = 0;
	priv->port_stats.oversized_packets = 0;
	priv->port_stats.tso_packets = 0;
	priv->port_stats.defrag_attempts = 0;

	for (i = 0; i < priv->tx_ring_num; i++) {
		const struct mlx4_en_tx_ring *ring;
		ring = priv->tx_ring[i];

		priv->port_stats.tx_chksum_offload += ring->tx_csum;
		priv->port_stats.queue_stopped     += ring->queue_stopped;
		priv->port_stats.wake_queue        += ring->wake_queue;
		priv->port_stats.oversized_packets += ring->oversized_packets;
		priv->port_stats.tso_packets       += ring->tso_packets;
		priv->port_stats.defrag_attempts   += ring->defrag_attempts;
	}

	priv->pkstats.rx_errors =
			   be64_to_cpu(mlx4_en_stats->PCS) +
			   be32_to_cpu(mlx4_en_stats->RJBBR) +
			   be32_to_cpu(mlx4_en_stats->RCRC) +
			   be32_to_cpu(mlx4_en_stats->RRUNT) +
			   be64_to_cpu(mlx4_en_stats->RInRangeLengthErr) +
			   be64_to_cpu(mlx4_en_stats->ROutRangeLengthErr) +
			   be32_to_cpu(mlx4_en_stats->RSHORT) +
			   en_stats_adder(&mlx4_en_stats->RGIANT_prio_0,
					  &mlx4_en_stats->RGIANT_prio_1,
					  NUM_PRIORITIES);
	priv->pkstats.tx_errors =
	    en_stats_adder(&mlx4_en_stats->TGIANT_prio_0,
					  &mlx4_en_stats->TGIANT_prio_1,
					  NUM_PRIORITIES);
	priv->pkstats.rx_multicast_packets =
	    en_stats_adder(&mlx4_en_stats->MCAST_prio_0,
					  &mlx4_en_stats->MCAST_prio_1,
					  NUM_PRIORITIES);
	priv->pkstats.rx_dropped = be32_to_cpu(mlx4_en_stats->RDROP);
	priv->pkstats.rx_length_errors = be32_to_cpu(mlx4_en_stats->RdropLength);
	priv->pkstats.rx_over_errors = be32_to_cpu(mlx4_en_stats->RdropOvflw);
	priv->pkstats.rx_crc_errors = be32_to_cpu(mlx4_en_stats->RCRC);
	priv->pkstats.tx_dropped = be32_to_cpu(mlx4_en_stats->TDROP);

	/* RX stats */
	priv->pkstats.rx_packets = en_stats_adder(&mlx4_en_stats->RTOT_prio_0,
					   &mlx4_en_stats->RTOT_prio_1,
					   NUM_PRIORITIES);
	priv->pkstats.rx_bytes = en_stats_adder(&mlx4_en_stats->ROCT_prio_0,
					 &mlx4_en_stats->ROCT_prio_1,
					 NUM_PRIORITIES);
	priv->pkstats.rx_broadcast_packets =
			en_stats_adder(&mlx4_en_stats->RBCAST_prio_0,
				       &mlx4_en_stats->RBCAST_prio_1,
				       NUM_PRIORITIES);
	priv->pkstats.rx_jabbers = be32_to_cpu(mlx4_en_stats->RJBBR);
	priv->pkstats.rx_in_range_length_error =
		be64_to_cpu(mlx4_en_stats->RInRangeLengthErr);
	priv->pkstats.rx_out_range_length_error =
		be64_to_cpu(mlx4_en_stats->ROutRangeLengthErr);

	/* Tx stats */
	priv->pkstats.tx_packets = en_stats_adder(&mlx4_en_stats->TTOT_prio_0,
					   &mlx4_en_stats->TTOT_prio_1,
					   NUM_PRIORITIES);
	priv->pkstats.tx_bytes = en_stats_adder(&mlx4_en_stats->TOCT_prio_0,
					 &mlx4_en_stats->TOCT_prio_1,
					 NUM_PRIORITIES);
	priv->pkstats.tx_multicast_packets =
		en_stats_adder(&mlx4_en_stats->TMCAST_prio_0,
			       &mlx4_en_stats->TMCAST_prio_1,
			       NUM_PRIORITIES);
	priv->pkstats.tx_broadcast_packets =
		en_stats_adder(&mlx4_en_stats->TBCAST_prio_0,
			       &mlx4_en_stats->TBCAST_prio_1,
			       NUM_PRIORITIES);

	priv->pkstats.rx_prio[0][0] = be64_to_cpu(mlx4_en_stats->RTOT_prio_0);
	priv->pkstats.rx_prio[0][1] = be64_to_cpu(mlx4_en_stats->ROCT_prio_0);
	priv->pkstats.rx_prio[1][0] = be64_to_cpu(mlx4_en_stats->RTOT_prio_1);
	priv->pkstats.rx_prio[1][1] = be64_to_cpu(mlx4_en_stats->ROCT_prio_1);
	priv->pkstats.rx_prio[2][0] = be64_to_cpu(mlx4_en_stats->RTOT_prio_2);
	priv->pkstats.rx_prio[2][1] = be64_to_cpu(mlx4_en_stats->ROCT_prio_2);
	priv->pkstats.rx_prio[3][0] = be64_to_cpu(mlx4_en_stats->RTOT_prio_3);
	priv->pkstats.rx_prio[3][1] = be64_to_cpu(mlx4_en_stats->ROCT_prio_3);
	priv->pkstats.rx_prio[4][0] = be64_to_cpu(mlx4_en_stats->RTOT_prio_4);
	priv->pkstats.rx_prio[4][1] = be64_to_cpu(mlx4_en_stats->ROCT_prio_4);
	priv->pkstats.rx_prio[5][0] = be64_to_cpu(mlx4_en_stats->RTOT_prio_5);
	priv->pkstats.rx_prio[5][1] = be64_to_cpu(mlx4_en_stats->ROCT_prio_5);
	priv->pkstats.rx_prio[6][0] = be64_to_cpu(mlx4_en_stats->RTOT_prio_6);
	priv->pkstats.rx_prio[6][1] = be64_to_cpu(mlx4_en_stats->ROCT_prio_6);
	priv->pkstats.rx_prio[7][0] = be64_to_cpu(mlx4_en_stats->RTOT_prio_7);
	priv->pkstats.rx_prio[7][1] = be64_to_cpu(mlx4_en_stats->ROCT_prio_7);
	priv->pkstats.rx_prio[8][0] = be64_to_cpu(mlx4_en_stats->RTOT_novlan);
	priv->pkstats.rx_prio[8][1] = be64_to_cpu(mlx4_en_stats->ROCT_novlan);
	priv->pkstats.tx_prio[0][0] = be64_to_cpu(mlx4_en_stats->TTOT_prio_0);
	priv->pkstats.tx_prio[0][1] = be64_to_cpu(mlx4_en_stats->TOCT_prio_0);
	priv->pkstats.tx_prio[1][0] = be64_to_cpu(mlx4_en_stats->TTOT_prio_1);
	priv->pkstats.tx_prio[1][1] = be64_to_cpu(mlx4_en_stats->TOCT_prio_1);
	priv->pkstats.tx_prio[2][0] = be64_to_cpu(mlx4_en_stats->TTOT_prio_2);
	priv->pkstats.tx_prio[2][1] = be64_to_cpu(mlx4_en_stats->TOCT_prio_2);
	priv->pkstats.tx_prio[3][0] = be64_to_cpu(mlx4_en_stats->TTOT_prio_3);
	priv->pkstats.tx_prio[3][1] = be64_to_cpu(mlx4_en_stats->TOCT_prio_3);
	priv->pkstats.tx_prio[4][0] = be64_to_cpu(mlx4_en_stats->TTOT_prio_4);
	priv->pkstats.tx_prio[4][1] = be64_to_cpu(mlx4_en_stats->TOCT_prio_4);
	priv->pkstats.tx_prio[5][0] = be64_to_cpu(mlx4_en_stats->TTOT_prio_5);
	priv->pkstats.tx_prio[5][1] = be64_to_cpu(mlx4_en_stats->TOCT_prio_5);
	priv->pkstats.tx_prio[6][0] = be64_to_cpu(mlx4_en_stats->TTOT_prio_6);
	priv->pkstats.tx_prio[6][1] = be64_to_cpu(mlx4_en_stats->TOCT_prio_6);
	priv->pkstats.tx_prio[7][0] = be64_to_cpu(mlx4_en_stats->TTOT_prio_7);
	priv->pkstats.tx_prio[7][1] = be64_to_cpu(mlx4_en_stats->TOCT_prio_7);
	priv->pkstats.tx_prio[8][0] = be64_to_cpu(mlx4_en_stats->TTOT_novlan);
	priv->pkstats.tx_prio[8][1] = be64_to_cpu(mlx4_en_stats->TOCT_novlan);

	mlx4_en_fold_software_stats(dev);

	spin_unlock(&priv->stats_lock);

	memset(&tmp_vport_stats, 0, sizeof(tmp_vport_stats));
	counter_index = mlx4_get_default_counter_index(mdev->dev, port);
	err = mlx4_get_counter_stats(mdev->dev, counter_index,
				     &tmp_vport_stats, reset);

	spin_lock(&priv->stats_lock);
	if (!err) {
		/* ethtool stats format */
		vport_stats->rx_bytes = be64_to_cpu(tmp_vport_stats.rx_bytes);
		vport_stats->rx_frames = be64_to_cpu(tmp_vport_stats.rx_frames);
		vport_stats->tx_bytes = be64_to_cpu(tmp_vport_stats.tx_bytes);
		vport_stats->tx_frames = be64_to_cpu(tmp_vport_stats.tx_frames);
	}

	if (reset == 0) {
		if_inc_counter(dev, IFCOUNTER_IPACKETS,
		    priv->pkstats.rx_packets - priv->pkstats_last.rx_packets);
		if_inc_counter(dev, IFCOUNTER_OPACKETS,
		    priv->pkstats.tx_packets - priv->pkstats_last.tx_packets);
		if_inc_counter(dev, IFCOUNTER_IBYTES,
		    priv->pkstats.rx_bytes - priv->pkstats_last.rx_bytes);
		if_inc_counter(dev, IFCOUNTER_OBYTES,
		    priv->pkstats.tx_bytes - priv->pkstats_last.tx_bytes);
		if_inc_counter(dev, IFCOUNTER_IERRORS,
		    priv->pkstats.rx_errors - priv->pkstats_last.rx_errors);
		if_inc_counter(dev, IFCOUNTER_IQDROPS,
		    priv->pkstats.rx_dropped - priv->pkstats_last.rx_dropped);
		if_inc_counter(dev, IFCOUNTER_IMCASTS,
		    priv->pkstats.rx_multicast_packets - priv->pkstats_last.rx_multicast_packets);
		if_inc_counter(dev, IFCOUNTER_OMCASTS,
		    priv->pkstats.tx_multicast_packets - priv->pkstats_last.tx_multicast_packets);
	}
	priv->pkstats_last = priv->pkstats;

	spin_unlock(&priv->stats_lock);

	/* 0xffs indicates invalid value */
	memset(mailbox->buf, 0xff, sizeof(*flowstats) * MLX4_NUM_PRIORITIES);

	if (mdev->dev->caps.flags2 & MLX4_DEV_CAP_FLAG2_FLOWSTATS_EN) {
		memset(mailbox->buf, 0,
		       sizeof(*flowstats) * MLX4_NUM_PRIORITIES);
		err = mlx4_cmd_box(mdev->dev, 0, mailbox->dma,
				   in_mod | MLX4_DUMP_ETH_STATS_FLOW_CONTROL,
				   0, MLX4_CMD_DUMP_ETH_STATS,
				   MLX4_CMD_TIME_CLASS_B, MLX4_CMD_NATIVE);
		if (err)
			goto out;
	}

	flowstats = mailbox->buf;

	spin_lock(&priv->stats_lock);

	for (i = 0; i < MLX4_NUM_PRIORITIES; i++)	{
		priv->rx_priority_flowstats[i].rx_pause =
			be64_to_cpu(flowstats[i].rx_pause);
		priv->rx_priority_flowstats[i].rx_pause_duration =
			be64_to_cpu(flowstats[i].rx_pause_duration);
		priv->rx_priority_flowstats[i].rx_pause_transition =
			be64_to_cpu(flowstats[i].rx_pause_transition);
		priv->tx_priority_flowstats[i].tx_pause =
			be64_to_cpu(flowstats[i].tx_pause);
		priv->tx_priority_flowstats[i].tx_pause_duration =
			be64_to_cpu(flowstats[i].tx_pause_duration);
		priv->tx_priority_flowstats[i].tx_pause_transition =
			be64_to_cpu(flowstats[i].tx_pause_transition);
	}

	/* if pfc is not in use, all priorities counters have the same value */
	priv->rx_flowstats.rx_pause =
		be64_to_cpu(flowstats[0].rx_pause);
	priv->rx_flowstats.rx_pause_duration =
		be64_to_cpu(flowstats[0].rx_pause_duration);
	priv->rx_flowstats.rx_pause_transition =
		be64_to_cpu(flowstats[0].rx_pause_transition);
	priv->tx_flowstats.tx_pause =
		be64_to_cpu(flowstats[0].tx_pause);
	priv->tx_flowstats.tx_pause_duration =
		be64_to_cpu(flowstats[0].tx_pause_duration);
	priv->tx_flowstats.tx_pause_transition =
		be64_to_cpu(flowstats[0].tx_pause_transition);

	spin_unlock(&priv->stats_lock);

out:
	mlx4_free_cmd_mailbox(mdev->dev, mailbox);
	return err;
}

int mlx4_en_get_vport_stats(struct mlx4_en_dev *mdev, u8 port)
{
	struct mlx4_en_priv *priv = mlx4_netdev_priv(mdev->pndev[port]);
	struct mlx4_counter tmp_vport_stats;
	struct mlx4_en_vf_stats *vf_stats = &priv->vf_stats;
	int err, i, counter_index;

	spin_lock(&priv->stats_lock);

	priv->pkstats.rx_packets = 0;
	priv->pkstats.rx_bytes = 0;
	priv->port_stats.rx_chksum_good = 0;
	priv->port_stats.rx_chksum_none = 0;
	for (i = 0; i < priv->rx_ring_num; i++) {
		priv->pkstats.rx_packets += priv->rx_ring[i]->packets;
		priv->pkstats.rx_bytes += priv->rx_ring[i]->bytes;
		priv->port_stats.rx_chksum_good += priv->rx_ring[i]->csum_ok;
		priv->port_stats.rx_chksum_none += priv->rx_ring[i]->csum_none;
	}
	priv->pkstats.tx_packets = 0;
	priv->pkstats.tx_bytes = 0;
	priv->port_stats.tx_chksum_offload = 0;
	priv->port_stats.queue_stopped = 0;
	priv->port_stats.wake_queue = 0;

	for (i = 0; i < priv->tx_ring_num; i++) {
		const struct mlx4_en_tx_ring *ring = priv->tx_ring[i];

		priv->pkstats.tx_packets += ring->packets;
		priv->pkstats.tx_bytes += ring->bytes;
		priv->port_stats.tx_chksum_offload += ring->tx_csum;
		priv->port_stats.queue_stopped     += ring->queue_stopped;
		priv->port_stats.wake_queue        += ring->wake_queue;
		priv->port_stats.oversized_packets += priv->tx_ring[i]->oversized_packets;
	}

	spin_unlock(&priv->stats_lock);

	memset(&tmp_vport_stats, 0, sizeof(tmp_vport_stats));

	counter_index = mlx4_get_default_counter_index(mdev->dev, port);
	err = mlx4_get_counter_stats(mdev->dev, counter_index,
				     &tmp_vport_stats, 0);

	if (!err) {
		spin_lock(&priv->stats_lock);

		vf_stats->rx_bytes = be64_to_cpu(tmp_vport_stats.rx_bytes);
		vf_stats->rx_frames = be64_to_cpu(tmp_vport_stats.rx_frames);
		vf_stats->tx_bytes = be64_to_cpu(tmp_vport_stats.tx_bytes);
		vf_stats->tx_frames = be64_to_cpu(tmp_vport_stats.tx_frames);

		priv->pkstats.rx_packets = vf_stats->rx_frames;
		priv->pkstats.rx_bytes = vf_stats->rx_bytes;
		priv->pkstats.tx_packets = vf_stats->tx_frames;
		priv->pkstats.tx_bytes = vf_stats->tx_bytes;

		/* PF&VFs are not expected to report errors in ifconfig.
		 * rx_errors will be reprted in PF's ethtool statistics,
		 * see: mlx4_en_DUMP_ETH_STATS
		 */
		priv->pkstats.rx_errors = 0;
		priv->pkstats.rx_dropped = 0;
		priv->pkstats.tx_dropped = 0;
		priv->pkstats.rx_multicast_packets = 0;

		spin_unlock(&priv->stats_lock);
	}

	return err;
}
