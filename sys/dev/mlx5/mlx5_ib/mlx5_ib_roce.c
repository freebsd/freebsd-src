/*-
 * Copyright (c) 2013-2015, Mellanox Technologies, Ltd.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS `AS IS' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <linux/etherdevice.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <dev/mlx5/vport.h>
#include <net/ipv6.h>
#include <rdma/ib_addr.h>
#include <rdma/ib_cache.h>
#include "mlx5_ib.h"

struct net_device *mlx5_ib_get_netdev(struct ib_device *ib_dev, u8 port)
{
	struct mlx5_ib_dev *dev = to_mdev(ib_dev);

	return mlx5_get_protocol_dev(dev->mdev, MLX5_INTERFACE_PROTOCOL_ETH);
}

 
static void ib_gid_to_mlx5_roce_addr(const union ib_gid *gid,
				     struct net_device *ndev,
				     void *mlx5_addr)
{
#define MLX5_SET_RA(p, f, v) MLX5_SET(roce_addr_layout, p, f, v)
	char *mlx5_addr_l3_addr = MLX5_ADDR_OF(roce_addr_layout, mlx5_addr,
					       source_l3_address);
	void *mlx5_addr_mac	= MLX5_ADDR_OF(roce_addr_layout, mlx5_addr,
					       source_mac_47_32);
	union ib_gid zgid;
	u16 vtag;

	memset(&zgid, 0, sizeof(zgid));
	if (0 == memcmp(gid, &zgid, sizeof(zgid)))
		return;

	ether_addr_copy(mlx5_addr_mac, IF_LLADDR(ndev));

	if (VLAN_TAG(ndev, &vtag) == 0) {
		MLX5_SET_RA(mlx5_addr, vlan_valid, 1);
		MLX5_SET_RA(mlx5_addr, vlan_id, vtag);
	}

#ifndef MLX5_USE_ROCE_VERSION_2
	MLX5_SET_RA(mlx5_addr, roce_version, MLX5_ROCE_VERSION_1);

	memcpy(mlx5_addr_l3_addr, gid, sizeof(*gid));
#else
	MLX5_SET_RA(mlx5_addr, roce_version, MLX5_ROCE_VERSION_2);

	if (ipv6_addr_v4mapped((void *)gid)) {
		MLX5_SET_RA(mlx5_addr, roce_l3_type,
			    MLX5_ROCE_L3_TYPE_IPV4);
		memcpy(&mlx5_addr_l3_addr[12], &gid->raw[12], 4);
	} else {
		MLX5_SET_RA(mlx5_addr, roce_l3_type,
			    MLX5_ROCE_L3_TYPE_IPV6);
		memcpy(mlx5_addr_l3_addr, gid, sizeof(*gid));
	}
#endif
}

int modify_gid_roce(struct ib_device *ib_dev, u8 port, unsigned int index,
		    const union ib_gid *gid, struct net_device *ndev)
{
	struct mlx5_ib_dev *dev = to_mdev(ib_dev);
	u32  in[MLX5_ST_SZ_DW(set_roce_address_in)];
	u32 out[MLX5_ST_SZ_DW(set_roce_address_out)];
	void *in_addr = MLX5_ADDR_OF(set_roce_address_in, in, roce_address);

	memset(in, 0, sizeof(in));

	ib_gid_to_mlx5_roce_addr(gid, ndev, in_addr);

	MLX5_SET(set_roce_address_in, in, roce_address_index, index);
	MLX5_SET(set_roce_address_in, in, opcode, MLX5_CMD_OP_SET_ROCE_ADDRESS);

	memset(out, 0, sizeof(out));
	return mlx5_cmd_exec(dev->mdev, in, sizeof(in), out, sizeof(out));
}

static int translate_eth_proto_oper(u32 eth_proto_oper, u8 *active_speed,
				    u8 *active_width)
{
	switch (eth_proto_oper) {
	case MLX5_PROT_MASK(MLX5_1000BASE_CX_SGMII):
	case MLX5_PROT_MASK(MLX5_1000BASE_KX):
	case MLX5_PROT_MASK(MLX5_100BASE_TX):
	case MLX5_PROT_MASK(MLX5_1000BASE_T):
		*active_width = IB_WIDTH_1X;
		*active_speed = IB_SPEED_SDR;
		break;
	case MLX5_PROT_MASK(MLX5_10GBASE_T):
	case MLX5_PROT_MASK(MLX5_10GBASE_CX4):
	case MLX5_PROT_MASK(MLX5_10GBASE_KX4):
	case MLX5_PROT_MASK(MLX5_10GBASE_KR):
	case MLX5_PROT_MASK(MLX5_10GBASE_CR):
	case MLX5_PROT_MASK(MLX5_10GBASE_SR):
	case MLX5_PROT_MASK(MLX5_10GBASE_ER):
		*active_width = IB_WIDTH_1X;
		*active_speed = IB_SPEED_QDR;
		break;
	case MLX5_PROT_MASK(MLX5_25GBASE_CR):
	case MLX5_PROT_MASK(MLX5_25GBASE_KR):
	case MLX5_PROT_MASK(MLX5_25GBASE_SR):
		*active_width = IB_WIDTH_1X;
		*active_speed = IB_SPEED_EDR;
		break;
	case MLX5_PROT_MASK(MLX5_40GBASE_CR4):
	case MLX5_PROT_MASK(MLX5_40GBASE_KR4):
	case MLX5_PROT_MASK(MLX5_40GBASE_SR4):
	case MLX5_PROT_MASK(MLX5_40GBASE_LR4):
		*active_width = IB_WIDTH_4X;
		*active_speed = IB_SPEED_QDR;
		break;
	case MLX5_PROT_MASK(MLX5_50GBASE_CR2):
	case MLX5_PROT_MASK(MLX5_50GBASE_KR2):
		*active_width = IB_WIDTH_1X;
		*active_speed = IB_SPEED_FDR;
		break;
	case MLX5_PROT_MASK(MLX5_56GBASE_R4):
		*active_width = IB_WIDTH_4X;
		*active_speed = IB_SPEED_FDR;
		break;
	case MLX5_PROT_MASK(MLX5_100GBASE_CR4):
	case MLX5_PROT_MASK(MLX5_100GBASE_SR4):
	case MLX5_PROT_MASK(MLX5_100GBASE_KR4):
	case MLX5_PROT_MASK(MLX5_100GBASE_LR4):
		*active_width = IB_WIDTH_4X;
		*active_speed = IB_SPEED_EDR;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int mlx5_query_roce_port_ptys(struct ib_device *ib_dev,
				     struct ib_port_attr *props, u8 port)
{
	struct mlx5_ib_dev *dev = to_mdev(ib_dev);
	struct mlx5_core_dev *mdev = dev->mdev;
	struct mlx5_ptys_reg *ptys;
	int err;

	ptys = kzalloc(sizeof(*ptys), GFP_KERNEL);
	if (!ptys)
		return -ENOMEM;

	ptys->proto_mask |= MLX5_PTYS_EN;
	ptys->local_port = port;

	err = mlx5_core_access_ptys(mdev, ptys, 0);
	if (err)
		goto out;

	err = translate_eth_proto_oper(ptys->eth_proto_oper,
				       &props->active_speed,
				       &props->active_width);
out:
	kfree(ptys);
	return err;
}

int mlx5_query_port_roce(struct ib_device *ib_dev, u8 port,
			 struct ib_port_attr *props)
{
	struct net_device *netdev = mlx5_ib_get_netdev(ib_dev, port);
	struct mlx5_ib_dev *dev = to_mdev(ib_dev);
	enum ib_mtu netdev_ib_mtu;

	memset(props, 0, sizeof(*props));

	props->port_cap_flags  |= IB_PORT_CM_SUP;

	props->gid_tbl_len      = MLX5_CAP_ROCE(dev->mdev,
						roce_address_table_size);
	props->max_mtu          = IB_MTU_4096;
	props->max_msg_sz       = 1 << MLX5_CAP_GEN(dev->mdev, log_max_msg);
	props->pkey_tbl_len     = 1;
	props->state            = IB_PORT_DOWN;
	props->phys_state       = 3;

	if (mlx5_query_nic_vport_qkey_viol_cntr(dev->mdev,
						(u16 *)&props->qkey_viol_cntr))
		printf("mlx5_ib: WARN: ""%s failed to query qkey violations counter\n", __func__);


	if (!netdev)
		return 0;

	if (netif_running(netdev) && netif_carrier_ok(netdev)) {
		props->state      = IB_PORT_ACTIVE;
		props->phys_state = 5;
	}

	netdev_ib_mtu = iboe_get_mtu(netdev->if_mtu);
	props->active_mtu	= min(props->max_mtu, netdev_ib_mtu);

	mlx5_query_roce_port_ptys(ib_dev, props, port);

	return 0;
}

__be16 mlx5_get_roce_udp_sport(struct mlx5_ib_dev *dev, u8 port,
			       int index, __be16 ah_s_udp_port)
{
#ifndef MLX5_USE_ROCE_VERSION_2
	return 0;
#else
	return cpu_to_be16(MLX5_CAP_ROCE(dev->mdev, r_roce_min_src_udp_port));
#endif
}

int mlx5_get_roce_gid_type(struct mlx5_ib_dev *dev, u8 port,
			   int index, int *gid_type)
{
	union ib_gid gid;
	int ret;

	ret = ib_get_cached_gid(&dev->ib_dev, port, index, &gid);

	if (!ret)
		*gid_type = -1;

	return ret;
}
