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

#include <linux/if_vlan.h>

#include <linux/mlx4/device.h>
#include <linux/mlx4/cmd.h>

#if 0 //  moved to port.c
int mlx4_SET_MCAST_FLTR(struct mlx4_dev *dev, u8 port,
			u64 mac, u64 clear, u8 mode)
{
	return mlx4_cmd(dev, (mac | (clear << 63)), port, mode,
			MLX4_CMD_SET_MCAST_FLTR, MLX4_CMD_TIME_CLASS_B, MLX4_CMD_NATIVE);
}
#endif

int mlx4_SET_VLAN_FLTR(struct mlx4_dev *dev, u8 port, u32 *vlans)
{
	struct mlx4_cmd_mailbox *mailbox;
	struct mlx4_set_vlan_fltr_mbox *filter;
	int i, j;
	int err = 0;

	mailbox = mlx4_alloc_cmd_mailbox(dev);
	if (IS_ERR(mailbox))
		return PTR_ERR(mailbox);

	filter = mailbox->buf;
	memset(filter, 0, sizeof *filter);
	if (vlans)
		for (i = 0, j = VLAN_FLTR_SIZE - 1; i < VLAN_FLTR_SIZE;
		    i++, j--)
			filter->entry[j] = cpu_to_be32(vlans[i]);
	err = mlx4_cmd(dev, mailbox->dma, port, 0, MLX4_CMD_SET_VLAN_FLTR,
		       MLX4_CMD_TIME_CLASS_B, MLX4_CMD_NATIVE);
	mlx4_free_cmd_mailbox(dev, mailbox);
	return err;
}


#if 0 //moved to port.c - shahark
int mlx4_SET_PORT_general(struct mlx4_dev *dev, u8 port, int mtu,
			  u8 pptx, u8 pfctx, u8 pprx, u8 pfcrx)
{
	struct mlx4_cmd_mailbox *mailbox;
	struct mlx4_set_port_general_context *context;
	int err;
	u32 in_mod;

	mailbox = mlx4_alloc_cmd_mailbox(dev);
	if (IS_ERR(mailbox))
		return PTR_ERR(mailbox);
	context = mailbox->buf;
	memset(context, 0, sizeof *context);

	context->flags = SET_PORT_GEN_ALL_VALID;
	context->mtu = cpu_to_be16(mtu);
	context->pptx = (pptx * (!pfctx)) << 7;
	context->pfctx = pfctx;
	context->pprx = (pprx * (!pfcrx)) << 7;
	context->pfcrx = pfcrx;

	in_mod = MLX4_SET_PORT_GENERAL << 8 | port;
	err = mlx4_cmd(dev, mailbox->dma, in_mod, 1, MLX4_CMD_SET_PORT,
		       MLX4_CMD_TIME_CLASS_B, MLX4_CMD_NATIVE);

	mlx4_free_cmd_mailbox(dev, mailbox);
	return err;
}
int mlx4_SET_PORT_qpn_calc(struct mlx4_dev *dev, u8 port, u32 base_qpn,
			   u8 promisc)
{

        printf("%s %s:%d\n", __func__, __FILE__, __LINE__);



	struct mlx4_cmd_mailbox *mailbox;
	struct mlx4_set_port_rqp_calc_context *context;
	int err;
	u32 in_mod;

	mailbox = mlx4_alloc_cmd_mailbox(dev);
	if (IS_ERR(mailbox))
		return PTR_ERR(mailbox);
	context = mailbox->buf;
	memset(context, 0, sizeof *context);

	context->base_qpn = cpu_to_be32(base_qpn);
	context->promisc = cpu_to_be32(promisc << SET_PORT_PROMISC_EN_SHIFT | base_qpn);
/*
	context->mcast = cpu_to_be32((dev->caps.mc_promisc_mode <<
				      SET_PORT_PROMISC_MODE_SHIFT) | base_qpn);
*/
	context->intra_no_vlan = 0;
	context->no_vlan = MLX4_NO_VLAN_IDX;
	context->intra_vlan_miss = 0;
	context->vlan_miss = MLX4_VLAN_MISS_IDX;

	in_mod = MLX4_SET_PORT_RQP_CALC << 8 | port;
	err = mlx4_cmd(dev, mailbox->dma, in_mod, 1, MLX4_CMD_SET_PORT,
		       MLX4_CMD_TIME_CLASS_B, MLX4_CMD_NATIVE);

	mlx4_free_cmd_mailbox(dev, mailbox);
	return err;
}
#endif

int mlx4_en_QUERY_PORT(struct mlx4_en_dev *mdev, u8 port)
{
	struct mlx4_en_query_port_context *qport_context;
	struct mlx4_en_priv *priv = netdev_priv(mdev->pndev[port]);
	struct mlx4_en_port_state *state = &priv->port_state;
	struct mlx4_cmd_mailbox *mailbox;
	int err;

	mailbox = mlx4_alloc_cmd_mailbox(mdev->dev);
	if (IS_ERR(mailbox))
		return PTR_ERR(mailbox);
	memset(mailbox->buf, 0, sizeof(*qport_context));
	err = mlx4_cmd_box(mdev->dev, 0, mailbox->dma, port, 0,
			   MLX4_CMD_QUERY_PORT, MLX4_CMD_TIME_CLASS_B, MLX4_CMD_WRAPPED);
	if (err)
		goto out;
	qport_context = mailbox->buf;

	/* This command is always accessed from Ethtool context
	 * already synchronized, no need in locking */
	state->link_state = !!(qport_context->link_up & MLX4_EN_LINK_UP_MASK);
	switch (qport_context->link_speed & MLX4_EN_SPEED_MASK) {
	case MLX4_EN_1G_SPEED:
		state->link_speed = 1000;
		break;
	case MLX4_EN_10G_SPEED_XAUI:
	case MLX4_EN_10G_SPEED_XFI:
		state->link_speed = 10000;
		break;
	case MLX4_EN_40G_SPEED:
		state->link_speed = 40000;
		break;
	default:
		state->link_speed = -1;
		break;
	}
	state->transciver = qport_context->transceiver;
	if (be32_to_cpu(qport_context->transceiver_code_hi) & 0x400)
		state->transciver = 0x80;

out:
	mlx4_free_cmd_mailbox(mdev->dev, mailbox);
	return err;
}

#if 0
static int read_iboe_counters(struct mlx4_dev *dev, int index, u64 counters[])
{
	struct mlx4_cmd_mailbox *mailbox;
	int err;
	int mode;
	struct mlx4_counters_ext *ext;
	struct mlx4_counters *reg;

	mailbox = mlx4_alloc_cmd_mailbox(dev);
	if (IS_ERR(mailbox))
		return -ENOMEM;

	err = mlx4_cmd_box(dev, 0, mailbox->dma, index, 0,
			   MLX4_CMD_QUERY_IF_STAT, MLX4_CMD_TIME_CLASS_C, MLX4_CMD_WRAPPED);
	if (err)
		goto out;

	mode = be32_to_cpu(((struct mlx4_counters *)mailbox->buf)->counter_mode) & 0xf;
	switch (mode) {
	case 0:
		reg = mailbox->buf;
		counters[0] = be64_to_cpu(reg->rx_frames);
		counters[1] = be64_to_cpu(reg->tx_frames);
		counters[2] = be64_to_cpu(reg->rx_bytes);
		counters[3] = be64_to_cpu(reg->tx_bytes);
		break;
	case 1:
		ext = mailbox->buf;
		counters[0] = be64_to_cpu(ext->rx_uni_frames);
		counters[1] = be64_to_cpu(ext->tx_uni_frames);
		counters[2] = be64_to_cpu(ext->rx_uni_bytes);
		counters[3] = be64_to_cpu(ext->tx_uni_bytes);
		break;
	default:
		err = -EINVAL;
	}

out:
	mlx4_free_cmd_mailbox(dev, mailbox);
	return err;
}
#endif

int mlx4_en_DUMP_ETH_STATS(struct mlx4_en_dev *mdev, u8 port, u8 reset)
{
	struct mlx4_en_stat_out_mbox *mlx4_en_stats;
	struct net_device *dev;
	struct mlx4_en_priv *priv;
	struct mlx4_cmd_mailbox *mailbox;
	u64 in_mod = reset << 8 | port;
	unsigned long oerror;
	unsigned long ierror;
	int err;
	int i;
	//int counter;
	u64 counters[4];

	dev = mdev->pndev[port];
	priv = netdev_priv(dev);
	memset(counters, 0, sizeof counters);
        /*
	counter = mlx4_get_iboe_counter(priv->mdev->dev, port);
	if (counter >= 0)
		err = read_iboe_counters(priv->mdev->dev, counter, counters);
        */

	mailbox = mlx4_alloc_cmd_mailbox(mdev->dev);
	if (IS_ERR(mailbox))
		return PTR_ERR(mailbox);
	memset(mailbox->buf, 0, sizeof(*mlx4_en_stats));
	err = mlx4_cmd_box(mdev->dev, 0, mailbox->dma, in_mod, 0,
			   MLX4_CMD_DUMP_ETH_STATS, MLX4_CMD_TIME_CLASS_B, MLX4_CMD_WRAPPED);
	if (err)
		goto out;

	mlx4_en_stats = mailbox->buf;

	spin_lock(&priv->stats_lock);

	oerror = ierror = 0;
	dev->if_ipackets = counters[0];
	dev->if_ibytes = counters[2];
	for (i = 0; i < priv->rx_ring_num; i++) {
		dev->if_ipackets += priv->rx_ring[i].packets;
		dev->if_ibytes += priv->rx_ring[i].bytes;
		ierror += priv->rx_ring[i].errors;
	}
	dev->if_opackets = counters[1];
	dev->if_obytes = counters[3];
	for (i = 0; i <= priv->tx_ring_num; i++) {
		dev->if_opackets += priv->tx_ring[i].packets;
		dev->if_obytes += priv->tx_ring[i].bytes;
		oerror += priv->tx_ring[i].errors;
	}

	dev->if_ierrors = be32_to_cpu(mlx4_en_stats->RDROP) + ierror;
	dev->if_oerrors = be32_to_cpu(mlx4_en_stats->TDROP) + oerror;
	dev->if_imcasts = be64_to_cpu(mlx4_en_stats->MCAST_prio_0) +
			  be64_to_cpu(mlx4_en_stats->MCAST_prio_1) +
			  be64_to_cpu(mlx4_en_stats->MCAST_prio_2) +
			  be64_to_cpu(mlx4_en_stats->MCAST_prio_3) +
			  be64_to_cpu(mlx4_en_stats->MCAST_prio_4) +
			  be64_to_cpu(mlx4_en_stats->MCAST_prio_5) +
			  be64_to_cpu(mlx4_en_stats->MCAST_prio_6) +
			  be64_to_cpu(mlx4_en_stats->MCAST_prio_7) +
			  be64_to_cpu(mlx4_en_stats->MCAST_novlan);
	dev->if_omcasts = be64_to_cpu(mlx4_en_stats->TMCAST_prio_0) +
			  be64_to_cpu(mlx4_en_stats->TMCAST_prio_1) +
			  be64_to_cpu(mlx4_en_stats->TMCAST_prio_2) +
			  be64_to_cpu(mlx4_en_stats->TMCAST_prio_3) +
			  be64_to_cpu(mlx4_en_stats->TMCAST_prio_4) +
			  be64_to_cpu(mlx4_en_stats->TMCAST_prio_5) +
			  be64_to_cpu(mlx4_en_stats->TMCAST_prio_6) +
			  be64_to_cpu(mlx4_en_stats->TMCAST_prio_7) +
			  be64_to_cpu(mlx4_en_stats->TMCAST_novlan);
	dev->if_collisions = 0;

	priv->pkstats.broadcast =
				be64_to_cpu(mlx4_en_stats->RBCAST_prio_0) +
				be64_to_cpu(mlx4_en_stats->RBCAST_prio_1) +
				be64_to_cpu(mlx4_en_stats->RBCAST_prio_2) +
				be64_to_cpu(mlx4_en_stats->RBCAST_prio_3) +
				be64_to_cpu(mlx4_en_stats->RBCAST_prio_4) +
				be64_to_cpu(mlx4_en_stats->RBCAST_prio_5) +
				be64_to_cpu(mlx4_en_stats->RBCAST_prio_6) +
				be64_to_cpu(mlx4_en_stats->RBCAST_prio_7) +
				be64_to_cpu(mlx4_en_stats->RBCAST_novlan);
	priv->pkstats.rx_prio[0] = be64_to_cpu(mlx4_en_stats->RTOT_prio_0);
	priv->pkstats.rx_prio[1] = be64_to_cpu(mlx4_en_stats->RTOT_prio_1);
	priv->pkstats.rx_prio[2] = be64_to_cpu(mlx4_en_stats->RTOT_prio_2);
	priv->pkstats.rx_prio[3] = be64_to_cpu(mlx4_en_stats->RTOT_prio_3);
	priv->pkstats.rx_prio[4] = be64_to_cpu(mlx4_en_stats->RTOT_prio_4);
	priv->pkstats.rx_prio[5] = be64_to_cpu(mlx4_en_stats->RTOT_prio_5);
	priv->pkstats.rx_prio[6] = be64_to_cpu(mlx4_en_stats->RTOT_prio_6);
	priv->pkstats.rx_prio[7] = be64_to_cpu(mlx4_en_stats->RTOT_prio_7);
	priv->pkstats.tx_prio[0] = be64_to_cpu(mlx4_en_stats->TTOT_prio_0);
	priv->pkstats.tx_prio[1] = be64_to_cpu(mlx4_en_stats->TTOT_prio_1);
	priv->pkstats.tx_prio[2] = be64_to_cpu(mlx4_en_stats->TTOT_prio_2);
	priv->pkstats.tx_prio[3] = be64_to_cpu(mlx4_en_stats->TTOT_prio_3);
	priv->pkstats.tx_prio[4] = be64_to_cpu(mlx4_en_stats->TTOT_prio_4);
	priv->pkstats.tx_prio[5] = be64_to_cpu(mlx4_en_stats->TTOT_prio_5);
	priv->pkstats.tx_prio[6] = be64_to_cpu(mlx4_en_stats->TTOT_prio_6);
	priv->pkstats.tx_prio[7] = be64_to_cpu(mlx4_en_stats->TTOT_prio_7);
	spin_unlock(&priv->stats_lock);

out:
	mlx4_free_cmd_mailbox(mdev->dev, mailbox);
	return err;
}

