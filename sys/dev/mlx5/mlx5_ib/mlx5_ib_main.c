/*-
 * Copyright (c) 2013-2021, Mellanox Technologies, Ltd.  All rights reserved.
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
 */

#include "opt_rss.h"
#include "opt_ratelimit.h"

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#if defined(CONFIG_X86)
#include <asm/pat.h>
#endif
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/fs.h>
#undef inode
#include <rdma/ib_user_verbs.h>
#include <rdma/ib_addr.h>
#include <rdma/ib_cache.h>
#include <dev/mlx5/port.h>
#include <dev/mlx5/vport.h>
#include <linux/list.h>
#include <rdma/ib_smi.h>
#include <rdma/ib_umem.h>
#include <rdma/uverbs_ioctl.h>
#include <linux/in.h>
#include <linux/etherdevice.h>
#include <dev/mlx5/fs.h>
#include <dev/mlx5/mlx5_ib/mlx5_ib.h>

MODULE_DESCRIPTION("Mellanox Connect-IB HCA IB driver");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DEPEND(mlx5ib, linuxkpi, 1, 1, 1);
MODULE_DEPEND(mlx5ib, mlx5, 1, 1, 1);
MODULE_DEPEND(mlx5ib, ibcore, 1, 1, 1);
MODULE_VERSION(mlx5ib, 1);

enum {
	MLX5_ATOMIC_SIZE_QP_8BYTES = 1 << 3,
};

static enum rdma_link_layer
mlx5_port_type_cap_to_rdma_ll(int port_type_cap)
{
	switch (port_type_cap) {
	case MLX5_CAP_PORT_TYPE_IB:
		return IB_LINK_LAYER_INFINIBAND;
	case MLX5_CAP_PORT_TYPE_ETH:
		return IB_LINK_LAYER_ETHERNET;
	default:
		return IB_LINK_LAYER_UNSPECIFIED;
	}
}

static enum rdma_link_layer
mlx5_ib_port_link_layer(struct ib_device *device, u8 port_num)
{
	struct mlx5_ib_dev *dev = to_mdev(device);
	int port_type_cap = MLX5_CAP_GEN(dev->mdev, port_type);

	return mlx5_port_type_cap_to_rdma_ll(port_type_cap);
}

static bool mlx5_netdev_match(if_t ndev,
			      struct mlx5_core_dev *mdev,
			      const char *dname)
{
	return if_gettype(ndev) == IFT_ETHER &&
	  if_getdname(ndev) != NULL &&
	  strcmp(if_getdname(ndev), dname) == 0 &&
	  if_getsoftc(ndev) != NULL &&
	  *(struct mlx5_core_dev **)if_getsoftc(ndev) == mdev;
}

static int mlx5_netdev_event(struct notifier_block *this,
			     unsigned long event, void *ptr)
{
	if_t ndev = netdev_notifier_info_to_ifp(ptr);
	struct mlx5_ib_dev *ibdev = container_of(this, struct mlx5_ib_dev,
						 roce.nb);

	switch (event) {
	case NETDEV_REGISTER:
	case NETDEV_UNREGISTER:
		write_lock(&ibdev->roce.netdev_lock);
		/* check if network interface belongs to mlx5en */
		if (mlx5_netdev_match(ndev, ibdev->mdev, "mce"))
			ibdev->roce.netdev = (event == NETDEV_UNREGISTER) ?
					     NULL : ndev;
		write_unlock(&ibdev->roce.netdev_lock);
		break;

	case NETDEV_UP:
	case NETDEV_DOWN: {
		if_t upper = NULL;

		if ((upper == ndev || (!upper && ndev == ibdev->roce.netdev))
		    && ibdev->ib_active) {
			struct ib_event ibev = {0};

			ibev.device = &ibdev->ib_dev;
			ibev.event = (event == NETDEV_UP) ?
				     IB_EVENT_PORT_ACTIVE : IB_EVENT_PORT_ERR;
			ibev.element.port_num = 1;
			ib_dispatch_event(&ibev);
		}
		break;
	}

	default:
		break;
	}

	return NOTIFY_DONE;
}

static if_t mlx5_ib_get_netdev(struct ib_device *device,
					     u8 port_num)
{
	struct mlx5_ib_dev *ibdev = to_mdev(device);
	if_t ndev;

	/* Ensure ndev does not disappear before we invoke if_ref()
	 */
	read_lock(&ibdev->roce.netdev_lock);
	ndev = ibdev->roce.netdev;
	if (ndev)
		if_ref(ndev);
	read_unlock(&ibdev->roce.netdev_lock);

	return ndev;
}

static int translate_eth_proto_oper(u32 eth_proto_oper, u8 *active_speed,
				    u8 *active_width)
{
	switch (eth_proto_oper) {
	case MLX5E_PROT_MASK(MLX5E_1000BASE_CX_SGMII):
	case MLX5E_PROT_MASK(MLX5E_1000BASE_KX):
	case MLX5E_PROT_MASK(MLX5E_100BASE_TX):
	case MLX5E_PROT_MASK(MLX5E_1000BASE_T):
		*active_width = IB_WIDTH_1X;
		*active_speed = IB_SPEED_SDR;
		break;
	case MLX5E_PROT_MASK(MLX5E_10GBASE_T):
	case MLX5E_PROT_MASK(MLX5E_10GBASE_CX4):
	case MLX5E_PROT_MASK(MLX5E_10GBASE_KX4):
	case MLX5E_PROT_MASK(MLX5E_10GBASE_KR):
	case MLX5E_PROT_MASK(MLX5E_10GBASE_CR):
	case MLX5E_PROT_MASK(MLX5E_10GBASE_SR):
	case MLX5E_PROT_MASK(MLX5E_10GBASE_ER_LR):
		*active_width = IB_WIDTH_1X;
		*active_speed = IB_SPEED_QDR;
		break;
	case MLX5E_PROT_MASK(MLX5E_25GBASE_CR):
	case MLX5E_PROT_MASK(MLX5E_25GBASE_KR):
	case MLX5E_PROT_MASK(MLX5E_25GBASE_SR):
		*active_width = IB_WIDTH_1X;
		*active_speed = IB_SPEED_EDR;
		break;
	case MLX5E_PROT_MASK(MLX5E_40GBASE_CR4):
	case MLX5E_PROT_MASK(MLX5E_40GBASE_KR4):
	case MLX5E_PROT_MASK(MLX5E_40GBASE_SR4):
	case MLX5E_PROT_MASK(MLX5E_40GBASE_LR4_ER4):
		*active_width = IB_WIDTH_4X;
		*active_speed = IB_SPEED_QDR;
		break;
	case MLX5E_PROT_MASK(MLX5E_50GBASE_CR2):
	case MLX5E_PROT_MASK(MLX5E_50GBASE_KR2):
	case MLX5E_PROT_MASK(MLX5E_50GBASE_KR4):
	case MLX5E_PROT_MASK(MLX5E_50GBASE_SR2):
		*active_width = IB_WIDTH_1X;
		*active_speed = IB_SPEED_HDR;
		break;
	case MLX5E_PROT_MASK(MLX5E_56GBASE_R4):
		*active_width = IB_WIDTH_4X;
		*active_speed = IB_SPEED_FDR;
		break;
	case MLX5E_PROT_MASK(MLX5E_100GBASE_CR4):
	case MLX5E_PROT_MASK(MLX5E_100GBASE_SR4):
	case MLX5E_PROT_MASK(MLX5E_100GBASE_KR4):
	case MLX5E_PROT_MASK(MLX5E_100GBASE_LR4):
		*active_width = IB_WIDTH_4X;
		*active_speed = IB_SPEED_EDR;
		break;
	default:
		*active_width = IB_WIDTH_4X;
		*active_speed = IB_SPEED_QDR;
		return -EINVAL;
	}

	return 0;
}

static int translate_eth_ext_proto_oper(u32 eth_proto_oper, u8 *active_speed,
					u8 *active_width)
{
	switch (eth_proto_oper) {
	case MLX5E_PROT_MASK(MLX5E_SGMII_100M):
	case MLX5E_PROT_MASK(MLX5E_1000BASE_X_SGMII):
		*active_width = IB_WIDTH_1X;
		*active_speed = IB_SPEED_SDR;
		break;
	case MLX5E_PROT_MASK(MLX5E_5GBASE_R):
		*active_width = IB_WIDTH_1X;
		*active_speed = IB_SPEED_DDR;
		break;
	case MLX5E_PROT_MASK(MLX5E_10GBASE_XFI_XAUI_1):
		*active_width = IB_WIDTH_1X;
		*active_speed = IB_SPEED_QDR;
		break;
	case MLX5E_PROT_MASK(MLX5E_40GBASE_XLAUI_4_XLPPI_4):
		*active_width = IB_WIDTH_4X;
		*active_speed = IB_SPEED_QDR;
		break;
	case MLX5E_PROT_MASK(MLX5E_25GAUI_1_25GBASE_CR_KR):
		*active_width = IB_WIDTH_1X;
		*active_speed = IB_SPEED_EDR;
		break;
	case MLX5E_PROT_MASK(MLX5E_50GAUI_2_LAUI_2_50GBASE_CR2_KR2):
		*active_width = IB_WIDTH_2X;
		*active_speed = IB_SPEED_EDR;
		break;
	case MLX5E_PROT_MASK(MLX5E_50GAUI_1_LAUI_1_50GBASE_CR_KR):
		*active_width = IB_WIDTH_1X;
		*active_speed = IB_SPEED_HDR;
		break;
	case MLX5E_PROT_MASK(MLX5E_CAUI_4_100GBASE_CR4_KR4):
		*active_width = IB_WIDTH_4X;
		*active_speed = IB_SPEED_EDR;
		break;
	case MLX5E_PROT_MASK(MLX5E_100GAUI_2_100GBASE_CR2_KR2):
		*active_width = IB_WIDTH_2X;
		*active_speed = IB_SPEED_HDR;
		break;
	case MLX5E_PROT_MASK(MLX5E_100GAUI_1_100GBASE_CR_KR):
		*active_width = IB_WIDTH_1X;
		*active_speed = IB_SPEED_NDR;
		break;
	case MLX5E_PROT_MASK(MLX5E_200GAUI_4_200GBASE_CR4_KR4):
		*active_width = IB_WIDTH_4X;
		*active_speed = IB_SPEED_HDR;
		break;
	case MLX5E_PROT_MASK(MLX5E_200GAUI_2_200GBASE_CR2_KR2):
		*active_width = IB_WIDTH_2X;
		*active_speed = IB_SPEED_NDR;
		break;
	case MLX5E_PROT_MASK(MLX5E_400GAUI_4_400GBASE_CR4_KR4):
		*active_width = IB_WIDTH_4X;
		*active_speed = IB_SPEED_NDR;
		break;
	default:
		*active_width = IB_WIDTH_4X;
		*active_speed = IB_SPEED_QDR;
		return -EINVAL;
	}

	return 0;
}

static int mlx5_query_port_roce(struct ib_device *device, u8 port_num,
				struct ib_port_attr *props)
{
	struct mlx5_ib_dev *dev = to_mdev(device);
	u32 out[MLX5_ST_SZ_DW(ptys_reg)] = {};
	if_t ndev;
	enum ib_mtu ndev_ib_mtu;
	u16 qkey_viol_cntr;
	u32 eth_prot_oper;
	bool ext;
	int err;

	memset(props, 0, sizeof(*props));

	/* Possible bad flows are checked before filling out props so in case
	 * of an error it will still be zeroed out.
	 */
	err = mlx5_query_port_ptys(dev->mdev, out, sizeof(out), MLX5_PTYS_EN,
	    port_num);
	if (err)
		return err;

	ext = MLX5_CAP_PCAM_FEATURE(dev->mdev, ptys_extended_ethernet);
	eth_prot_oper = MLX5_GET_ETH_PROTO(ptys_reg, out, ext, eth_proto_oper);

	if (ext)
		translate_eth_ext_proto_oper(eth_prot_oper, &props->active_speed,
		    &props->active_width);
	else
		translate_eth_proto_oper(eth_prot_oper, &props->active_speed,
		    &props->active_width);

	props->port_cap_flags  |= IB_PORT_CM_SUP;
	props->port_cap_flags  |= IB_PORT_IP_BASED_GIDS;

	props->gid_tbl_len      = MLX5_CAP_ROCE(dev->mdev,
						roce_address_table_size);
	props->max_mtu          = IB_MTU_4096;
	props->max_msg_sz       = 1 << MLX5_CAP_GEN(dev->mdev, log_max_msg);
	props->pkey_tbl_len     = 1;
	props->state            = IB_PORT_DOWN;
	props->phys_state       = IB_PORT_PHYS_STATE_DISABLED;

	mlx5_query_nic_vport_qkey_viol_cntr(dev->mdev, &qkey_viol_cntr);
	props->qkey_viol_cntr = qkey_viol_cntr;

	ndev = mlx5_ib_get_netdev(device, port_num);
	if (!ndev)
		return 0;

	if (if_getdrvflags(ndev) & IFF_DRV_RUNNING &&
	    if_getlinkstate(ndev) == LINK_STATE_UP) {
		props->state      = IB_PORT_ACTIVE;
		props->phys_state = IB_PORT_PHYS_STATE_LINK_UP;
	}

	ndev_ib_mtu = iboe_get_mtu(if_getmtu(ndev));

	if_rele(ndev);

	props->active_mtu	= min(props->max_mtu, ndev_ib_mtu);
	return 0;
}

static void ib_gid_to_mlx5_roce_addr(const union ib_gid *gid,
				     const struct ib_gid_attr *attr,
				     void *mlx5_addr)
{
#define MLX5_SET_RA(p, f, v) MLX5_SET(roce_addr_layout, p, f, v)
	char *mlx5_addr_l3_addr	= MLX5_ADDR_OF(roce_addr_layout, mlx5_addr,
					       source_l3_address);
	void *mlx5_addr_mac	= MLX5_ADDR_OF(roce_addr_layout, mlx5_addr,
					       source_mac_47_32);
	u16 vlan_id;

	if (!gid)
		return;
	ether_addr_copy(mlx5_addr_mac, if_getlladdr(attr->ndev));

	vlan_id = rdma_vlan_dev_vlan_id(attr->ndev);
	if (vlan_id != 0xffff) {
		MLX5_SET_RA(mlx5_addr, vlan_valid, 1);
		MLX5_SET_RA(mlx5_addr, vlan_id, vlan_id);
	}

	switch (attr->gid_type) {
	case IB_GID_TYPE_IB:
		MLX5_SET_RA(mlx5_addr, roce_version, MLX5_ROCE_VERSION_1);
		break;
	case IB_GID_TYPE_ROCE_UDP_ENCAP:
		MLX5_SET_RA(mlx5_addr, roce_version, MLX5_ROCE_VERSION_2);
		break;

	default:
		WARN_ON(true);
	}

	if (attr->gid_type != IB_GID_TYPE_IB) {
		if (ipv6_addr_v4mapped((void *)gid))
			MLX5_SET_RA(mlx5_addr, roce_l3_type,
				    MLX5_ROCE_L3_TYPE_IPV4);
		else
			MLX5_SET_RA(mlx5_addr, roce_l3_type,
				    MLX5_ROCE_L3_TYPE_IPV6);
	}

	if ((attr->gid_type == IB_GID_TYPE_IB) ||
	    !ipv6_addr_v4mapped((void *)gid))
		memcpy(mlx5_addr_l3_addr, gid, sizeof(*gid));
	else
		memcpy(&mlx5_addr_l3_addr[12], &gid->raw[12], 4);
}

static int set_roce_addr(struct ib_device *device, u8 port_num,
			 unsigned int index,
			 const union ib_gid *gid,
			 const struct ib_gid_attr *attr)
{
	struct mlx5_ib_dev *dev = to_mdev(device);
	u32  in[MLX5_ST_SZ_DW(set_roce_address_in)]  = {0};
	u32 out[MLX5_ST_SZ_DW(set_roce_address_out)] = {0};
	void *in_addr = MLX5_ADDR_OF(set_roce_address_in, in, roce_address);
	enum rdma_link_layer ll = mlx5_ib_port_link_layer(device, port_num);

	if (ll != IB_LINK_LAYER_ETHERNET)
		return -EINVAL;

	ib_gid_to_mlx5_roce_addr(gid, attr, in_addr);

	MLX5_SET(set_roce_address_in, in, roce_address_index, index);
	MLX5_SET(set_roce_address_in, in, opcode, MLX5_CMD_OP_SET_ROCE_ADDRESS);
	return mlx5_cmd_exec(dev->mdev, in, sizeof(in), out, sizeof(out));
}

static int mlx5_ib_add_gid(struct ib_device *device, u8 port_num,
			   unsigned int index, const union ib_gid *gid,
			   const struct ib_gid_attr *attr,
			   __always_unused void **context)
{
	return set_roce_addr(device, port_num, index, gid, attr);
}

static int mlx5_ib_del_gid(struct ib_device *device, u8 port_num,
			   unsigned int index, __always_unused void **context)
{
	return set_roce_addr(device, port_num, index, NULL, NULL);
}

__be16 mlx5_get_roce_udp_sport(struct mlx5_ib_dev *dev, u8 port_num,
			       int index)
{
	struct ib_gid_attr attr;
	union ib_gid gid;

	if (ib_get_cached_gid(&dev->ib_dev, port_num, index, &gid, &attr))
		return 0;

	if (!attr.ndev)
		return 0;

	if_rele(attr.ndev);

	if (attr.gid_type != IB_GID_TYPE_ROCE_UDP_ENCAP)
		return 0;

	return cpu_to_be16(MLX5_CAP_ROCE(dev->mdev, r_roce_min_src_udp_port));
}

int mlx5_get_roce_gid_type(struct mlx5_ib_dev *dev, u8 port_num,
			   int index, enum ib_gid_type *gid_type)
{
	struct ib_gid_attr attr;
	union ib_gid gid;
	int ret;

	ret = ib_get_cached_gid(&dev->ib_dev, port_num, index, &gid, &attr);
	if (ret)
		return ret;

	if (!attr.ndev)
		return -ENODEV;

	if_rele(attr.ndev);

	*gid_type = attr.gid_type;

	return 0;
}

static int mlx5_use_mad_ifc(struct mlx5_ib_dev *dev)
{
	if (MLX5_CAP_GEN(dev->mdev, port_type) == MLX5_CAP_PORT_TYPE_IB)
		return !MLX5_CAP_GEN(dev->mdev, ib_virt);
	return 0;
}

enum {
	MLX5_VPORT_ACCESS_METHOD_MAD,
	MLX5_VPORT_ACCESS_METHOD_HCA,
	MLX5_VPORT_ACCESS_METHOD_NIC,
};

static int mlx5_get_vport_access_method(struct ib_device *ibdev)
{
	if (mlx5_use_mad_ifc(to_mdev(ibdev)))
		return MLX5_VPORT_ACCESS_METHOD_MAD;

	if (mlx5_ib_port_link_layer(ibdev, 1) ==
	    IB_LINK_LAYER_ETHERNET)
		return MLX5_VPORT_ACCESS_METHOD_NIC;

	return MLX5_VPORT_ACCESS_METHOD_HCA;
}

static void get_atomic_caps(struct mlx5_ib_dev *dev,
			    struct ib_device_attr *props)
{
	u8 tmp;
	u8 atomic_operations = MLX5_CAP_ATOMIC(dev->mdev, atomic_operations);
	u8 atomic_size_qp = MLX5_CAP_ATOMIC(dev->mdev, atomic_size_qp);
	u8 atomic_req_8B_endianness_mode =
		MLX5_CAP_ATOMIC(dev->mdev, atomic_req_8B_endianess_mode);

	/* Check if HW supports 8 bytes standard atomic operations and capable
	 * of host endianness respond
	 */
	tmp = MLX5_ATOMIC_OPS_CMP_SWAP | MLX5_ATOMIC_OPS_FETCH_ADD;
	if (((atomic_operations & tmp) == tmp) &&
	    (atomic_size_qp & MLX5_ATOMIC_SIZE_QP_8BYTES) &&
	    (atomic_req_8B_endianness_mode)) {
		props->atomic_cap = IB_ATOMIC_HCA;
	} else {
		props->atomic_cap = IB_ATOMIC_NONE;
	}
}

static int mlx5_query_system_image_guid(struct ib_device *ibdev,
					__be64 *sys_image_guid)
{
	struct mlx5_ib_dev *dev = to_mdev(ibdev);
	struct mlx5_core_dev *mdev = dev->mdev;
	u64 tmp;
	int err;

	switch (mlx5_get_vport_access_method(ibdev)) {
	case MLX5_VPORT_ACCESS_METHOD_MAD:
		return mlx5_query_mad_ifc_system_image_guid(ibdev,
							    sys_image_guid);

	case MLX5_VPORT_ACCESS_METHOD_HCA:
		err = mlx5_query_hca_vport_system_image_guid(mdev, &tmp);
		break;

	case MLX5_VPORT_ACCESS_METHOD_NIC:
		err = mlx5_query_nic_vport_system_image_guid(mdev, &tmp);
		break;

	default:
		return -EINVAL;
	}

	if (!err)
		*sys_image_guid = cpu_to_be64(tmp);

	return err;

}

static int mlx5_query_max_pkeys(struct ib_device *ibdev,
				u16 *max_pkeys)
{
	struct mlx5_ib_dev *dev = to_mdev(ibdev);
	struct mlx5_core_dev *mdev = dev->mdev;

	switch (mlx5_get_vport_access_method(ibdev)) {
	case MLX5_VPORT_ACCESS_METHOD_MAD:
		return mlx5_query_mad_ifc_max_pkeys(ibdev, max_pkeys);

	case MLX5_VPORT_ACCESS_METHOD_HCA:
	case MLX5_VPORT_ACCESS_METHOD_NIC:
		*max_pkeys = mlx5_to_sw_pkey_sz(MLX5_CAP_GEN(mdev,
						pkey_table_size));
		return 0;

	default:
		return -EINVAL;
	}
}

static int mlx5_query_vendor_id(struct ib_device *ibdev,
				u32 *vendor_id)
{
	struct mlx5_ib_dev *dev = to_mdev(ibdev);

	switch (mlx5_get_vport_access_method(ibdev)) {
	case MLX5_VPORT_ACCESS_METHOD_MAD:
		return mlx5_query_mad_ifc_vendor_id(ibdev, vendor_id);

	case MLX5_VPORT_ACCESS_METHOD_HCA:
	case MLX5_VPORT_ACCESS_METHOD_NIC:
		return mlx5_core_query_vendor_id(dev->mdev, vendor_id);

	default:
		return -EINVAL;
	}
}

static int mlx5_query_node_guid(struct mlx5_ib_dev *dev,
				__be64 *node_guid)
{
	u64 tmp;
	int err;

	switch (mlx5_get_vport_access_method(&dev->ib_dev)) {
	case MLX5_VPORT_ACCESS_METHOD_MAD:
		return mlx5_query_mad_ifc_node_guid(dev, node_guid);

	case MLX5_VPORT_ACCESS_METHOD_HCA:
		err = mlx5_query_hca_vport_node_guid(dev->mdev, &tmp);
		break;

	case MLX5_VPORT_ACCESS_METHOD_NIC:
		err = mlx5_query_nic_vport_node_guid(dev->mdev, &tmp);
		break;

	default:
		return -EINVAL;
	}

	if (!err)
		*node_guid = cpu_to_be64(tmp);

	return err;
}

struct mlx5_reg_node_desc {
	u8	desc[IB_DEVICE_NODE_DESC_MAX];
};

static int mlx5_query_node_desc(struct mlx5_ib_dev *dev, char *node_desc)
{
	struct mlx5_reg_node_desc in;

	if (mlx5_use_mad_ifc(dev))
		return mlx5_query_mad_ifc_node_desc(dev, node_desc);

	memset(&in, 0, sizeof(in));

	return mlx5_core_access_reg(dev->mdev, &in, sizeof(in), node_desc,
				    sizeof(struct mlx5_reg_node_desc),
				    MLX5_REG_NODE_DESC, 0, 0);
}

static int mlx5_ib_query_device(struct ib_device *ibdev,
				struct ib_device_attr *props,
				struct ib_udata *uhw)
{
	struct mlx5_ib_dev *dev = to_mdev(ibdev);
	struct mlx5_core_dev *mdev = dev->mdev;
	int err = -ENOMEM;
	int max_sq_desc;
	int max_rq_sg;
	int max_sq_sg;
	u64 min_page_size = 1ull << MLX5_CAP_GEN(mdev, log_pg_sz);
	struct mlx5_ib_query_device_resp resp = {};
	size_t resp_len;
	u64 max_tso;

	resp_len = sizeof(resp.comp_mask) + sizeof(resp.response_length);
	if (uhw->outlen && uhw->outlen < resp_len)
		return -EINVAL;
	else
		resp.response_length = resp_len;

	if (uhw->inlen && !ib_is_udata_cleared(uhw, 0, uhw->inlen))
		return -EINVAL;

	memset(props, 0, sizeof(*props));
	err = mlx5_query_system_image_guid(ibdev,
					   &props->sys_image_guid);
	if (err)
		return err;

	err = mlx5_query_max_pkeys(ibdev, &props->max_pkeys);
	if (err)
		return err;

	err = mlx5_query_vendor_id(ibdev, &props->vendor_id);
	if (err)
		return err;

	props->fw_ver = ((u64)fw_rev_maj(dev->mdev) << 32) |
		((u32)fw_rev_min(dev->mdev) << 16) |
		fw_rev_sub(dev->mdev);
	props->device_cap_flags    = IB_DEVICE_CHANGE_PHY_PORT |
		IB_DEVICE_PORT_ACTIVE_EVENT		|
		IB_DEVICE_SYS_IMAGE_GUID		|
		IB_DEVICE_RC_RNR_NAK_GEN		|
		IB_DEVICE_KNOWSEPOCH;

	if (MLX5_CAP_GEN(mdev, pkv))
		props->device_cap_flags |= IB_DEVICE_BAD_PKEY_CNTR;
	if (MLX5_CAP_GEN(mdev, qkv))
		props->device_cap_flags |= IB_DEVICE_BAD_QKEY_CNTR;
	if (MLX5_CAP_GEN(mdev, apm))
		props->device_cap_flags |= IB_DEVICE_AUTO_PATH_MIG;
	if (MLX5_CAP_GEN(mdev, xrc))
		props->device_cap_flags |= IB_DEVICE_XRC;
	if (MLX5_CAP_GEN(mdev, imaicl)) {
		props->device_cap_flags |= IB_DEVICE_MEM_WINDOW |
					   IB_DEVICE_MEM_WINDOW_TYPE_2B;
		props->max_mw = 1 << MLX5_CAP_GEN(mdev, log_max_mkey);
		/* We support 'Gappy' memory registration too */
		props->device_cap_flags |= IB_DEVICE_SG_GAPS_REG;
	}
	props->device_cap_flags |= IB_DEVICE_MEM_MGT_EXTENSIONS;
	if (MLX5_CAP_GEN(mdev, sho)) {
		props->device_cap_flags |= IB_DEVICE_SIGNATURE_HANDOVER;
		/* At this stage no support for signature handover */
		props->sig_prot_cap = IB_PROT_T10DIF_TYPE_1 |
				      IB_PROT_T10DIF_TYPE_2 |
				      IB_PROT_T10DIF_TYPE_3;
		props->sig_guard_cap = IB_GUARD_T10DIF_CRC |
				       IB_GUARD_T10DIF_CSUM;
	}
	if (MLX5_CAP_GEN(mdev, block_lb_mc))
		props->device_cap_flags |= IB_DEVICE_BLOCK_MULTICAST_LOOPBACK;

	if (MLX5_CAP_GEN(dev->mdev, eth_net_offloads)) {
		if (MLX5_CAP_ETH(mdev, csum_cap))
			props->device_cap_flags |= IB_DEVICE_RAW_IP_CSUM;

		if (field_avail(typeof(resp), tso_caps, uhw->outlen)) {
			max_tso = MLX5_CAP_ETH(mdev, max_lso_cap);
			if (max_tso) {
				resp.tso_caps.max_tso = 1 << max_tso;
				resp.tso_caps.supported_qpts |=
					1 << IB_QPT_RAW_PACKET;
				resp.response_length += sizeof(resp.tso_caps);
			}
		}

		if (field_avail(typeof(resp), rss_caps, uhw->outlen)) {
			resp.rss_caps.rx_hash_function =
						MLX5_RX_HASH_FUNC_TOEPLITZ;
			resp.rss_caps.rx_hash_fields_mask =
						MLX5_RX_HASH_SRC_IPV4 |
						MLX5_RX_HASH_DST_IPV4 |
						MLX5_RX_HASH_SRC_IPV6 |
						MLX5_RX_HASH_DST_IPV6 |
						MLX5_RX_HASH_SRC_PORT_TCP |
						MLX5_RX_HASH_DST_PORT_TCP |
						MLX5_RX_HASH_SRC_PORT_UDP |
						MLX5_RX_HASH_DST_PORT_UDP;
			resp.response_length += sizeof(resp.rss_caps);
		}
	} else {
		if (field_avail(typeof(resp), tso_caps, uhw->outlen))
			resp.response_length += sizeof(resp.tso_caps);
		if (field_avail(typeof(resp), rss_caps, uhw->outlen))
			resp.response_length += sizeof(resp.rss_caps);
	}

	if (MLX5_CAP_GEN(mdev, ipoib_ipoib_offloads)) {
		props->device_cap_flags |= IB_DEVICE_UD_IP_CSUM;
		props->device_cap_flags |= IB_DEVICE_UD_TSO;
	}

	if (MLX5_CAP_GEN(dev->mdev, eth_net_offloads) &&
	    MLX5_CAP_ETH(dev->mdev, scatter_fcs))
		props->device_cap_flags |= IB_DEVICE_RAW_SCATTER_FCS;

	if (mlx5_get_flow_namespace(dev->mdev, MLX5_FLOW_NAMESPACE_BYPASS))
		props->device_cap_flags |= IB_DEVICE_MANAGED_FLOW_STEERING;

	props->vendor_part_id	   = mdev->pdev->device;
	props->hw_ver		   = mdev->pdev->revision;

	props->max_mr_size	   = ~0ull;
	props->page_size_cap	   = ~(min_page_size - 1);
	props->max_qp		   = 1 << MLX5_CAP_GEN(mdev, log_max_qp);
	props->max_qp_wr	   = 1 << MLX5_CAP_GEN(mdev, log_max_qp_sz);
	max_rq_sg =  MLX5_CAP_GEN(mdev, max_wqe_sz_rq) /
		     sizeof(struct mlx5_wqe_data_seg);
	max_sq_desc = min_t(int, MLX5_CAP_GEN(mdev, max_wqe_sz_sq), 512);
	max_sq_sg = (max_sq_desc - sizeof(struct mlx5_wqe_ctrl_seg) -
		     sizeof(struct mlx5_wqe_raddr_seg)) /
		sizeof(struct mlx5_wqe_data_seg);
	props->max_sge = min(max_rq_sg, max_sq_sg);
	props->max_sge_rd	   = MLX5_MAX_SGE_RD;
	props->max_cq		   = 1 << MLX5_CAP_GEN(mdev, log_max_cq);
	props->max_cqe = (1 << MLX5_CAP_GEN(mdev, log_max_cq_sz)) - 1;
	props->max_mr		   = 1 << MLX5_CAP_GEN(mdev, log_max_mkey);
	props->max_pd		   = 1 << MLX5_CAP_GEN(mdev, log_max_pd);
	props->max_qp_rd_atom	   = 1 << MLX5_CAP_GEN(mdev, log_max_ra_req_qp);
	props->max_qp_init_rd_atom = 1 << MLX5_CAP_GEN(mdev, log_max_ra_res_qp);
	props->max_srq		   = 1 << MLX5_CAP_GEN(mdev, log_max_srq);
	props->max_srq_wr = (1 << MLX5_CAP_GEN(mdev, log_max_srq_sz)) - 1;
	props->local_ca_ack_delay  = MLX5_CAP_GEN(mdev, local_ca_ack_delay);
	props->max_res_rd_atom	   = props->max_qp_rd_atom * props->max_qp;
	props->max_srq_sge	   = max_rq_sg - 1;
	props->max_fast_reg_page_list_len =
		1 << MLX5_CAP_GEN(mdev, log_max_klm_list_size);
	get_atomic_caps(dev, props);
	props->masked_atomic_cap   = IB_ATOMIC_NONE;
	props->max_mcast_grp	   = 1 << MLX5_CAP_GEN(mdev, log_max_mcg);
	props->max_mcast_qp_attach = MLX5_CAP_GEN(mdev, max_qp_mcg);
	props->max_total_mcast_qp_attach = props->max_mcast_qp_attach *
					   props->max_mcast_grp;
	props->max_map_per_fmr = INT_MAX; /* no limit in ConnectIB */
	props->hca_core_clock = MLX5_CAP_GEN(mdev, device_frequency_khz);
	props->timestamp_mask = 0x7FFFFFFFFFFFFFFFULL;

#ifdef CONFIG_INFINIBAND_ON_DEMAND_PAGING
	if (MLX5_CAP_GEN(mdev, pg))
		props->device_cap_flags |= IB_DEVICE_ON_DEMAND_PAGING;
	props->odp_caps = dev->odp_caps;
#endif

	if (MLX5_CAP_GEN(mdev, cd))
		props->device_cap_flags |= IB_DEVICE_CROSS_CHANNEL;

	if (!mlx5_core_is_pf(mdev))
		props->device_cap_flags |= IB_DEVICE_VIRTUAL_FUNCTION;

	if (mlx5_ib_port_link_layer(ibdev, 1) ==
	    IB_LINK_LAYER_ETHERNET) {
		props->rss_caps.max_rwq_indirection_tables =
			1 << MLX5_CAP_GEN(dev->mdev, log_max_rqt);
		props->rss_caps.max_rwq_indirection_table_size =
			1 << MLX5_CAP_GEN(dev->mdev, log_max_rqt_size);
		props->rss_caps.supported_qpts = 1 << IB_QPT_RAW_PACKET;
		props->max_wq_type_rq =
			1 << MLX5_CAP_GEN(dev->mdev, log_max_rq);
	}

	if (uhw->outlen) {
		err = ib_copy_to_udata(uhw, &resp, resp.response_length);

		if (err)
			return err;
	}

	return 0;
}

enum mlx5_ib_width {
	MLX5_IB_WIDTH_1X	= 1 << 0,
	MLX5_IB_WIDTH_2X	= 1 << 1,
	MLX5_IB_WIDTH_4X	= 1 << 2,
	MLX5_IB_WIDTH_8X	= 1 << 3,
	MLX5_IB_WIDTH_12X	= 1 << 4
};

static int translate_active_width(struct ib_device *ibdev, u8 active_width,
				  u8 *ib_width)
{
	struct mlx5_ib_dev *dev = to_mdev(ibdev);
	int err = 0;

	if (active_width & MLX5_IB_WIDTH_1X) {
		*ib_width = IB_WIDTH_1X;
	} else if (active_width & MLX5_IB_WIDTH_2X) {
		*ib_width = IB_WIDTH_2X;
	} else if (active_width & MLX5_IB_WIDTH_4X) {
		*ib_width = IB_WIDTH_4X;
	} else if (active_width & MLX5_IB_WIDTH_8X) {
		*ib_width = IB_WIDTH_8X;
	} else if (active_width & MLX5_IB_WIDTH_12X) {
		*ib_width = IB_WIDTH_12X;
	} else {
		mlx5_ib_dbg(dev, "Invalid active_width %d\n",
			    (int)active_width);
		err = -EINVAL;
	}

	return err;
}

enum ib_max_vl_num {
	__IB_MAX_VL_0		= 1,
	__IB_MAX_VL_0_1		= 2,
	__IB_MAX_VL_0_3		= 3,
	__IB_MAX_VL_0_7		= 4,
	__IB_MAX_VL_0_14	= 5,
};

enum mlx5_vl_hw_cap {
	MLX5_VL_HW_0	= 1,
	MLX5_VL_HW_0_1	= 2,
	MLX5_VL_HW_0_2	= 3,
	MLX5_VL_HW_0_3	= 4,
	MLX5_VL_HW_0_4	= 5,
	MLX5_VL_HW_0_5	= 6,
	MLX5_VL_HW_0_6	= 7,
	MLX5_VL_HW_0_7	= 8,
	MLX5_VL_HW_0_14	= 15
};

static int translate_max_vl_num(struct ib_device *ibdev, u8 vl_hw_cap,
				u8 *max_vl_num)
{
	switch (vl_hw_cap) {
	case MLX5_VL_HW_0:
		*max_vl_num = __IB_MAX_VL_0;
		break;
	case MLX5_VL_HW_0_1:
		*max_vl_num = __IB_MAX_VL_0_1;
		break;
	case MLX5_VL_HW_0_3:
		*max_vl_num = __IB_MAX_VL_0_3;
		break;
	case MLX5_VL_HW_0_7:
		*max_vl_num = __IB_MAX_VL_0_7;
		break;
	case MLX5_VL_HW_0_14:
		*max_vl_num = __IB_MAX_VL_0_14;
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int mlx5_query_hca_port(struct ib_device *ibdev, u8 port,
			       struct ib_port_attr *props)
{
	struct mlx5_ib_dev *dev = to_mdev(ibdev);
	struct mlx5_core_dev *mdev = dev->mdev;
	u32 *rep;
	int replen = MLX5_ST_SZ_BYTES(query_hca_vport_context_out);
	struct mlx5_ptys_reg *ptys;
	struct mlx5_pmtu_reg *pmtu;
	struct mlx5_pvlc_reg pvlc;
	void *ctx;
	int err;

	rep = mlx5_vzalloc(replen);
	ptys = kzalloc(sizeof(*ptys), GFP_KERNEL);
	pmtu = kzalloc(sizeof(*pmtu), GFP_KERNEL);
	if (!rep || !ptys || !pmtu) {
		err = -ENOMEM;
		goto out;
	}

	memset(props, 0, sizeof(*props));

	err = mlx5_query_hca_vport_context(mdev, port, 0, rep, replen);
	if (err)
		goto out;

	ctx = MLX5_ADDR_OF(query_hca_vport_context_out, rep, hca_vport_context);

	props->lid		= MLX5_GET(hca_vport_context, ctx, lid);
	props->lmc		= MLX5_GET(hca_vport_context, ctx, lmc);
	props->sm_lid		= MLX5_GET(hca_vport_context, ctx, sm_lid);
	props->sm_sl		= MLX5_GET(hca_vport_context, ctx, sm_sl);
	props->state		= MLX5_GET(hca_vport_context, ctx, vport_state);
	props->phys_state	= MLX5_GET(hca_vport_context, ctx,
					port_physical_state);
	props->port_cap_flags	= MLX5_GET(hca_vport_context, ctx, cap_mask1);
	props->gid_tbl_len	= mlx5_get_gid_table_len(MLX5_CAP_GEN(mdev, gid_table_size));
	props->max_msg_sz	= 1 << MLX5_CAP_GEN(mdev, log_max_msg);
	props->pkey_tbl_len	= mlx5_to_sw_pkey_sz(MLX5_CAP_GEN(mdev, pkey_table_size));
	props->bad_pkey_cntr	= MLX5_GET(hca_vport_context, ctx,
					pkey_violation_counter);
	props->qkey_viol_cntr	= MLX5_GET(hca_vport_context, ctx,
					qkey_violation_counter);
	props->subnet_timeout	= MLX5_GET(hca_vport_context, ctx,
					subnet_timeout);
	props->init_type_reply	= MLX5_GET(hca_vport_context, ctx,
					init_type_reply);
	props->grh_required	= MLX5_GET(hca_vport_context, ctx, grh_required);

	ptys->proto_mask |= MLX5_PTYS_IB;
	ptys->local_port = port;
	err = mlx5_core_access_ptys(mdev, ptys, 0);
	if (err)
		goto out;

	err = translate_active_width(ibdev, ptys->ib_link_width_oper,
				     &props->active_width);
	if (err)
		goto out;

	props->active_speed	= (u8)ptys->ib_proto_oper;

	pmtu->local_port = port;
	err = mlx5_core_access_pmtu(mdev, pmtu, 0);
	if (err)
		goto out;

	props->max_mtu		= pmtu->max_mtu;
	props->active_mtu	= pmtu->oper_mtu;

	memset(&pvlc, 0, sizeof(pvlc));
	pvlc.local_port = port;
	err = mlx5_core_access_pvlc(mdev, &pvlc, 0);
	if (err)
		goto out;

	err = translate_max_vl_num(ibdev, pvlc.vl_hw_cap,
				   &props->max_vl_num);
out:
	kvfree(rep);
	kfree(ptys);
	kfree(pmtu);
	return err;
}

int mlx5_ib_query_port(struct ib_device *ibdev, u8 port,
		       struct ib_port_attr *props)
{
	switch (mlx5_get_vport_access_method(ibdev)) {
	case MLX5_VPORT_ACCESS_METHOD_MAD:
		return mlx5_query_mad_ifc_port(ibdev, port, props);

	case MLX5_VPORT_ACCESS_METHOD_HCA:
		return mlx5_query_hca_port(ibdev, port, props);

	case MLX5_VPORT_ACCESS_METHOD_NIC:
		return mlx5_query_port_roce(ibdev, port, props);

	default:
		return -EINVAL;
	}
}

static int mlx5_ib_query_gid(struct ib_device *ibdev, u8 port, int index,
			     union ib_gid *gid)
{
	struct mlx5_ib_dev *dev = to_mdev(ibdev);
	struct mlx5_core_dev *mdev = dev->mdev;

	switch (mlx5_get_vport_access_method(ibdev)) {
	case MLX5_VPORT_ACCESS_METHOD_MAD:
		return mlx5_query_mad_ifc_gids(ibdev, port, index, gid);

	case MLX5_VPORT_ACCESS_METHOD_HCA:
		return mlx5_query_hca_vport_gid(mdev, port, 0, index, gid);

	default:
		return -EINVAL;
	}

}

static int mlx5_ib_query_pkey(struct ib_device *ibdev, u8 port, u16 index,
			      u16 *pkey)
{
	struct mlx5_ib_dev *dev = to_mdev(ibdev);
	struct mlx5_core_dev *mdev = dev->mdev;

	switch (mlx5_get_vport_access_method(ibdev)) {
	case MLX5_VPORT_ACCESS_METHOD_MAD:
		return mlx5_query_mad_ifc_pkey(ibdev, port, index, pkey);

	case MLX5_VPORT_ACCESS_METHOD_HCA:
	case MLX5_VPORT_ACCESS_METHOD_NIC:
		return mlx5_query_hca_vport_pkey(mdev, 0, port,  0, index,
						 pkey);
	default:
		return -EINVAL;
	}
}

static int mlx5_ib_modify_device(struct ib_device *ibdev, int mask,
				 struct ib_device_modify *props)
{
	struct mlx5_ib_dev *dev = to_mdev(ibdev);
	struct mlx5_reg_node_desc in;
	struct mlx5_reg_node_desc out;
	int err;

	if (mask & ~IB_DEVICE_MODIFY_NODE_DESC)
		return -EOPNOTSUPP;

	if (!(mask & IB_DEVICE_MODIFY_NODE_DESC))
		return 0;

	/*
	 * If possible, pass node desc to FW, so it can generate
	 * a 144 trap.  If cmd fails, just ignore.
	 */
	memcpy(&in, props->node_desc, IB_DEVICE_NODE_DESC_MAX);
	err = mlx5_core_access_reg(dev->mdev, &in, sizeof(in), &out,
				   sizeof(out), MLX5_REG_NODE_DESC, 0, 1);
	if (err)
		return err;

	memcpy(ibdev->node_desc, props->node_desc, IB_DEVICE_NODE_DESC_MAX);

	return err;
}

static int mlx5_ib_modify_port(struct ib_device *ibdev, u8 port, int mask,
			       struct ib_port_modify *props)
{
	struct mlx5_ib_dev *dev = to_mdev(ibdev);
	struct ib_port_attr attr;
	u32 tmp;
	int err;

	/*
	 * CM layer calls ib_modify_port() regardless of the link
	 * layer. For Ethernet ports, qkey violation and Port
	 * capabilities are meaningless.
	 */
	if (mlx5_ib_port_link_layer(ibdev, port) == IB_LINK_LAYER_ETHERNET)
		return 0;

	mutex_lock(&dev->cap_mask_mutex);

	err = mlx5_ib_query_port(ibdev, port, &attr);
	if (err)
		goto out;

	tmp = (attr.port_cap_flags | props->set_port_cap_mask) &
		~props->clr_port_cap_mask;

	err = mlx5_set_port_caps(dev->mdev, port, tmp);

out:
	mutex_unlock(&dev->cap_mask_mutex);
	return err;
}

static void print_lib_caps(struct mlx5_ib_dev *dev, u64 caps)
{
	mlx5_ib_dbg(dev, "MLX5_LIB_CAP_4K_UAR = %s\n",
		    caps & MLX5_LIB_CAP_4K_UAR ? "y" : "n");
}

static u16 calc_dynamic_bfregs(int uars_per_sys_page)
{
	/* Large page with non 4k uar support might limit the dynamic size */
	if (uars_per_sys_page == 1  && PAGE_SIZE > 4096)
		return MLX5_MIN_DYN_BFREGS;

	return MLX5_MAX_DYN_BFREGS;
}

static int calc_total_bfregs(struct mlx5_ib_dev *dev, bool lib_uar_4k,
			     struct mlx5_ib_alloc_ucontext_req_v2 *req,
			     struct mlx5_bfreg_info *bfregi)
{
	int uars_per_sys_page;
	int bfregs_per_sys_page;
	int ref_bfregs = req->total_num_bfregs;

	if (req->total_num_bfregs == 0)
		return -EINVAL;

	BUILD_BUG_ON(MLX5_MAX_BFREGS % MLX5_NON_FP_BFREGS_IN_PAGE);
	BUILD_BUG_ON(MLX5_MAX_BFREGS < MLX5_NON_FP_BFREGS_IN_PAGE);

	if (req->total_num_bfregs > MLX5_MAX_BFREGS)
		return -ENOMEM;

	uars_per_sys_page = get_uars_per_sys_page(dev, lib_uar_4k);
	bfregs_per_sys_page = uars_per_sys_page * MLX5_NON_FP_BFREGS_PER_UAR;
	/* This holds the required static allocation asked by the user */
	req->total_num_bfregs = ALIGN(req->total_num_bfregs, bfregs_per_sys_page);
	if (req->num_low_latency_bfregs > req->total_num_bfregs - 1)
		return -EINVAL;

	bfregi->num_static_sys_pages = req->total_num_bfregs / bfregs_per_sys_page;
	bfregi->num_dyn_bfregs = ALIGN(calc_dynamic_bfregs(uars_per_sys_page), bfregs_per_sys_page);
	bfregi->total_num_bfregs = req->total_num_bfregs + bfregi->num_dyn_bfregs;
	bfregi->num_sys_pages = bfregi->total_num_bfregs / bfregs_per_sys_page;

	mlx5_ib_dbg(dev, "uar_4k: fw support %s, lib support %s, user requested %d bfregs, allocated %d, total bfregs %d, using %d sys pages\n",
		    MLX5_CAP_GEN(dev->mdev, uar_4k) ? "yes" : "no",
		    lib_uar_4k ? "yes" : "no", ref_bfregs,
		    req->total_num_bfregs, bfregi->total_num_bfregs,
		    bfregi->num_sys_pages);

	return 0;
}

static int allocate_uars(struct mlx5_ib_dev *dev, struct mlx5_ib_ucontext *context)
{
	struct mlx5_bfreg_info *bfregi;
	int err;
	int i;

	bfregi = &context->bfregi;
	for (i = 0; i < bfregi->num_static_sys_pages; i++) {
		err = mlx5_cmd_alloc_uar(dev->mdev, &bfregi->sys_pages[i]);
		if (err)
			goto error;

		mlx5_ib_dbg(dev, "allocated uar %d\n", bfregi->sys_pages[i]);
	}

	for (i = bfregi->num_static_sys_pages; i < bfregi->num_sys_pages; i++)
		bfregi->sys_pages[i] = MLX5_IB_INVALID_UAR_INDEX;

	return 0;

error:
	for (--i; i >= 0; i--)
		if (mlx5_cmd_free_uar(dev->mdev, bfregi->sys_pages[i]))
			mlx5_ib_warn(dev, "failed to free uar %d\n", i);

	return err;
}

static void deallocate_uars(struct mlx5_ib_dev *dev,
			    struct mlx5_ib_ucontext *context)
{
	struct mlx5_bfreg_info *bfregi;
	int i;

	bfregi = &context->bfregi;
	for (i = 0; i < bfregi->num_sys_pages; i++)
		if (i < bfregi->num_static_sys_pages ||
		    bfregi->sys_pages[i] != MLX5_IB_INVALID_UAR_INDEX)
			mlx5_cmd_free_uar(dev->mdev, bfregi->sys_pages[i]);
}

static int mlx5_ib_alloc_transport_domain(struct mlx5_ib_dev *dev, u32 *tdn,
					  u16 uid)
{
	int err;

	if (!MLX5_CAP_GEN(dev->mdev, log_max_transport_domain))
		return 0;

	err = mlx5_alloc_transport_domain(dev->mdev, tdn, uid);
	if (err)
		return err;

	if ((MLX5_CAP_GEN(dev->mdev, port_type) != MLX5_CAP_PORT_TYPE_ETH) ||
	    (!MLX5_CAP_GEN(dev->mdev, disable_local_lb_uc) &&
	     !MLX5_CAP_GEN(dev->mdev, disable_local_lb_mc)))
		return 0;

	mutex_lock(&dev->lb_mutex);
	dev->user_td++;

	if (dev->user_td == 2)
		err = mlx5_nic_vport_update_local_lb(dev->mdev, true);

	mutex_unlock(&dev->lb_mutex);

	if (err != 0)
		mlx5_dealloc_transport_domain(dev->mdev, *tdn, uid);
	return err;
}

static void mlx5_ib_dealloc_transport_domain(struct mlx5_ib_dev *dev, u32 tdn,
					     u16 uid)
{
	if (!MLX5_CAP_GEN(dev->mdev, log_max_transport_domain))
		return;

	mlx5_dealloc_transport_domain(dev->mdev, tdn, uid);

	if ((MLX5_CAP_GEN(dev->mdev, port_type) != MLX5_CAP_PORT_TYPE_ETH) ||
	    (!MLX5_CAP_GEN(dev->mdev, disable_local_lb_uc) &&
	     !MLX5_CAP_GEN(dev->mdev, disable_local_lb_mc)))
		return;

	mutex_lock(&dev->lb_mutex);
	dev->user_td--;

	if (dev->user_td < 2)
		mlx5_nic_vport_update_local_lb(dev->mdev, false);

	mutex_unlock(&dev->lb_mutex);
}

static int mlx5_ib_alloc_ucontext(struct ib_ucontext *uctx,
				  struct ib_udata *udata)
{
	struct ib_device *ibdev = uctx->device;
	struct mlx5_ib_dev *dev = to_mdev(ibdev);
	struct mlx5_ib_alloc_ucontext_req_v2 req = {};
	struct mlx5_ib_alloc_ucontext_resp resp = {};
	struct mlx5_ib_ucontext *context = to_mucontext(uctx);
	struct mlx5_bfreg_info *bfregi;
	int ver;
	int err;
	size_t min_req_v2 = offsetof(struct mlx5_ib_alloc_ucontext_req_v2,
				     max_cqe_version);
	bool lib_uar_4k;
	bool lib_uar_dyn;

	if (!dev->ib_active)
		return -EAGAIN;

	if (udata->inlen == sizeof(struct mlx5_ib_alloc_ucontext_req))
		ver = 0;
	else if (udata->inlen >= min_req_v2)
		ver = 2;
	else
		return -EINVAL;

	err = ib_copy_from_udata(&req, udata, min(udata->inlen, sizeof(req)));
	if (err)
		return err;

	if (req.flags & ~MLX5_IB_ALLOC_UCTX_DEVX)
		return -EOPNOTSUPP;

	if (req.comp_mask || req.reserved0 || req.reserved1 || req.reserved2)
		return -EOPNOTSUPP;

	req.total_num_bfregs = ALIGN(req.total_num_bfregs,
				    MLX5_NON_FP_BFREGS_PER_UAR);
	if (req.num_low_latency_bfregs > req.total_num_bfregs - 1)
		return -EINVAL;

	resp.qp_tab_size = 1 << MLX5_CAP_GEN(dev->mdev, log_max_qp);
	if (mlx5_core_is_pf(dev->mdev) && MLX5_CAP_GEN(dev->mdev, bf))
		resp.bf_reg_size = 1 << MLX5_CAP_GEN(dev->mdev, log_bf_reg_size);
	resp.cache_line_size = cache_line_size();
	resp.max_sq_desc_sz = MLX5_CAP_GEN(dev->mdev, max_wqe_sz_sq);
	resp.max_rq_desc_sz = MLX5_CAP_GEN(dev->mdev, max_wqe_sz_rq);
	resp.max_send_wqebb = 1 << MLX5_CAP_GEN(dev->mdev, log_max_qp_sz);
	resp.max_recv_wr = 1 << MLX5_CAP_GEN(dev->mdev, log_max_qp_sz);
	resp.max_srq_recv_wr = 1 << MLX5_CAP_GEN(dev->mdev, log_max_srq_sz);
	resp.cqe_version = min_t(__u8,
				 (__u8)MLX5_CAP_GEN(dev->mdev, cqe_version),
				 req.max_cqe_version);
	resp.log_uar_size = MLX5_CAP_GEN(dev->mdev, uar_4k) ?
				MLX5_ADAPTER_PAGE_SHIFT : PAGE_SHIFT;
	resp.num_uars_per_page = MLX5_CAP_GEN(dev->mdev, uar_4k) ?
					MLX5_CAP_GEN(dev->mdev, num_of_uars_per_page) : 1;
	resp.response_length = min(offsetof(typeof(resp), response_length) +
				   sizeof(resp.response_length), udata->outlen);

	lib_uar_4k = req.lib_caps & MLX5_LIB_CAP_4K_UAR;
	lib_uar_dyn = req.lib_caps & MLX5_LIB_CAP_DYN_UAR;
	bfregi = &context->bfregi;

	if (lib_uar_dyn) {
		bfregi->lib_uar_dyn = lib_uar_dyn;
		goto uar_done;
	}

	/* updates req->total_num_bfregs */
	err = calc_total_bfregs(dev, lib_uar_4k, &req, bfregi);
	if (err)
		goto out_ctx;

	mutex_init(&bfregi->lock);
	bfregi->lib_uar_4k = lib_uar_4k;
	bfregi->count = kcalloc(bfregi->total_num_bfregs, sizeof(*bfregi->count),
				GFP_KERNEL);
	if (!bfregi->count) {
		err = -ENOMEM;
		goto out_ctx;
	}

	bfregi->sys_pages = kcalloc(bfregi->num_sys_pages,
				    sizeof(*bfregi->sys_pages),
				    GFP_KERNEL);
	if (!bfregi->sys_pages) {
		err = -ENOMEM;
		goto out_count;
	}

	err = allocate_uars(dev, context);
	if (err)
		goto out_sys_pages;

uar_done:
	if (req.flags & MLX5_IB_ALLOC_UCTX_DEVX) {
		err = mlx5_ib_devx_create(dev, true);
		if (err < 0)
			goto out_uars;
		context->devx_uid = err;
	}

	err = mlx5_ib_alloc_transport_domain(dev, &context->tdn,
					     context->devx_uid);
	if (err)
		goto out_devx;

	INIT_LIST_HEAD(&context->db_page_list);
	mutex_init(&context->db_page_mutex);

	resp.tot_bfregs = lib_uar_dyn ? 0 : req.total_num_bfregs;
	resp.num_ports = MLX5_CAP_GEN(dev->mdev, num_ports);

	if (field_avail(typeof(resp), cqe_version, udata->outlen))
		resp.response_length += sizeof(resp.cqe_version);

	if (field_avail(typeof(resp), cmds_supp_uhw, udata->outlen)) {
		resp.cmds_supp_uhw |= MLX5_USER_CMDS_SUPP_UHW_QUERY_DEVICE |
				      MLX5_USER_CMDS_SUPP_UHW_CREATE_AH;
		resp.response_length += sizeof(resp.cmds_supp_uhw);
	}

	/*
	 * We don't want to expose information from the PCI bar that is located
	 * after 4096 bytes, so if the arch only supports larger pages, let's
	 * pretend we don't support reading the HCA's core clock. This is also
	 * forced by mmap function.
	 */
	if (offsetofend(typeof(resp), hca_core_clock_offset) <= udata->outlen) {
		if (PAGE_SIZE <= 4096) {
			resp.comp_mask |=
				MLX5_IB_ALLOC_UCONTEXT_RESP_MASK_CORE_CLOCK_OFFSET;
			resp.hca_core_clock_offset =
				offsetof(struct mlx5_init_seg, internal_timer_h) % PAGE_SIZE;
		}
		resp.response_length += sizeof(resp.hca_core_clock_offset);
	}

	if (offsetofend(typeof(resp), log_uar_size) <= udata->outlen)
		resp.response_length += sizeof(resp.log_uar_size);

	if (offsetofend(typeof(resp), num_uars_per_page) <= udata->outlen)
		resp.response_length += sizeof(resp.num_uars_per_page);

	if (offsetofend(typeof(resp), num_dyn_bfregs) <= udata->outlen) {
		resp.num_dyn_bfregs = bfregi->num_dyn_bfregs;
		resp.response_length += sizeof(resp.num_dyn_bfregs);
	}

	err = ib_copy_to_udata(udata, &resp, resp.response_length);
	if (err)
		goto out_mdev;

	bfregi->ver = ver;
	bfregi->num_low_latency_bfregs = req.num_low_latency_bfregs;
	context->cqe_version = resp.cqe_version;
	context->lib_caps = req.lib_caps;
	print_lib_caps(dev, context->lib_caps);

	return 0;

out_mdev:
	mlx5_ib_dealloc_transport_domain(dev, context->tdn, context->devx_uid);
out_devx:
	if (req.flags & MLX5_IB_ALLOC_UCTX_DEVX)
		mlx5_ib_devx_destroy(dev, context->devx_uid);

out_uars:
	deallocate_uars(dev, context);

out_sys_pages:
	kfree(bfregi->sys_pages);

out_count:
	kfree(bfregi->count);

out_ctx:
	return err;
}

static void mlx5_ib_dealloc_ucontext(struct ib_ucontext *ibcontext)
{
	struct mlx5_ib_ucontext *context = to_mucontext(ibcontext);
	struct mlx5_ib_dev *dev = to_mdev(ibcontext->device);
	struct mlx5_bfreg_info *bfregi;

	bfregi = &context->bfregi;
	mlx5_ib_dealloc_transport_domain(dev, context->tdn, context->devx_uid);

	if (context->devx_uid)
		mlx5_ib_devx_destroy(dev, context->devx_uid);

	deallocate_uars(dev, context);
	kfree(bfregi->sys_pages);
	kfree(bfregi->count);
}

static phys_addr_t uar_index2pfn(struct mlx5_ib_dev *dev,
				 int uar_idx)
{
	int fw_uars_per_page;

	fw_uars_per_page = MLX5_CAP_GEN(dev->mdev, uar_4k) ? MLX5_UARS_IN_PAGE : 1;

	return (pci_resource_start(dev->mdev->pdev, 0) >> PAGE_SHIFT) + uar_idx / fw_uars_per_page;
}

static int get_command(unsigned long offset)
{
	return (offset >> MLX5_IB_MMAP_CMD_SHIFT) & MLX5_IB_MMAP_CMD_MASK;
}

static int get_arg(unsigned long offset)
{
	return offset & ((1 << MLX5_IB_MMAP_CMD_SHIFT) - 1);
}

static int get_index(unsigned long offset)
{
	return get_arg(offset);
}

/* Index resides in an extra byte to enable larger values than 255 */
static int get_extended_index(unsigned long offset)
{
	return get_arg(offset) | ((offset >> 16) & 0xff) << 8;
}


static void mlx5_ib_disassociate_ucontext(struct ib_ucontext *ibcontext)
{
}

static inline char *mmap_cmd2str(enum mlx5_ib_mmap_cmd cmd)
{
	switch (cmd) {
	case MLX5_IB_MMAP_WC_PAGE:
		return "WC";
	case MLX5_IB_MMAP_REGULAR_PAGE:
		return "best effort WC";
	case MLX5_IB_MMAP_NC_PAGE:
		return "NC";
	default:
		return NULL;
	}
}

static int mlx5_ib_mmap_clock_info_page(struct mlx5_ib_dev *dev,
					struct vm_area_struct *vma,
					struct mlx5_ib_ucontext *context)
{
	if ((vma->vm_end - vma->vm_start != PAGE_SIZE) ||
	    !(vma->vm_flags & VM_SHARED))
		return -EINVAL;

	if (get_index(vma->vm_pgoff) != MLX5_IB_CLOCK_INFO_V1)
		return -EOPNOTSUPP;

	if (vma->vm_flags & (VM_WRITE | VM_EXEC))
		return -EPERM;

	return -EOPNOTSUPP;
}

static void mlx5_ib_mmap_free(struct rdma_user_mmap_entry *entry)
{
	struct mlx5_user_mmap_entry *mentry = to_mmmap(entry);
	struct mlx5_ib_dev *dev = to_mdev(entry->ucontext->device);

	switch (mentry->mmap_flag) {
	case MLX5_IB_MMAP_TYPE_UAR_WC:
	case MLX5_IB_MMAP_TYPE_UAR_NC:
		mlx5_cmd_free_uar(dev->mdev, mentry->page_idx);
		kfree(mentry);
		break;
	default:
		WARN_ON(true);
	}
}

static int uar_mmap(struct mlx5_ib_dev *dev, enum mlx5_ib_mmap_cmd cmd,
		    struct vm_area_struct *vma,
		    struct mlx5_ib_ucontext *context)
{
	struct mlx5_bfreg_info *bfregi = &context->bfregi;
	int err;
	unsigned long idx;
	phys_addr_t pfn;
	pgprot_t prot;
	u32 bfreg_dyn_idx = 0;
	u32 uar_index;
	int dyn_uar = (cmd == MLX5_IB_MMAP_ALLOC_WC);
	int max_valid_idx = dyn_uar ? bfregi->num_sys_pages :
				bfregi->num_static_sys_pages;

	if (bfregi->lib_uar_dyn)
		return -EINVAL;

	if (vma->vm_end - vma->vm_start != PAGE_SIZE)
		return -EINVAL;

	if (dyn_uar)
		idx = get_extended_index(vma->vm_pgoff) + bfregi->num_static_sys_pages;
	else
		idx = get_index(vma->vm_pgoff);

	if (idx >= max_valid_idx) {
		mlx5_ib_warn(dev, "invalid uar index %lu, max=%d\n",
			     idx, max_valid_idx);
		return -EINVAL;
	}

	switch (cmd) {
	case MLX5_IB_MMAP_WC_PAGE:
	case MLX5_IB_MMAP_ALLOC_WC:
	case MLX5_IB_MMAP_REGULAR_PAGE:
		/* For MLX5_IB_MMAP_REGULAR_PAGE do the best effort to get WC */
		prot = pgprot_writecombine(vma->vm_page_prot);
		break;
	case MLX5_IB_MMAP_NC_PAGE:
		prot = pgprot_noncached(vma->vm_page_prot);
		break;
	default:
		return -EINVAL;
	}

	if (dyn_uar) {
		int uars_per_page;

		uars_per_page = get_uars_per_sys_page(dev, bfregi->lib_uar_4k);
		bfreg_dyn_idx = idx * (uars_per_page * MLX5_NON_FP_BFREGS_PER_UAR);
		if (bfreg_dyn_idx >= bfregi->total_num_bfregs) {
			mlx5_ib_warn(dev, "invalid bfreg_dyn_idx %u, max=%u\n",
				     bfreg_dyn_idx, bfregi->total_num_bfregs);
			return -EINVAL;
		}

		mutex_lock(&bfregi->lock);
		/* Fail if uar already allocated, first bfreg index of each
		 * page holds its count.
		 */
		if (bfregi->count[bfreg_dyn_idx]) {
			mlx5_ib_warn(dev, "wrong offset, idx %lu is busy, bfregn=%u\n", idx, bfreg_dyn_idx);
			mutex_unlock(&bfregi->lock);
			return -EINVAL;
		}

		bfregi->count[bfreg_dyn_idx]++;
		mutex_unlock(&bfregi->lock);

		err = mlx5_cmd_alloc_uar(dev->mdev, &uar_index);
		if (err) {
			mlx5_ib_warn(dev, "UAR alloc failed\n");
			goto free_bfreg;
		}
	} else {
		uar_index = bfregi->sys_pages[idx];
	}

	pfn = uar_index2pfn(dev, uar_index);
	mlx5_ib_dbg(dev, "uar idx 0x%lx, pfn %pa\n", idx, &pfn);

	err = rdma_user_mmap_io(&context->ibucontext, vma, pfn, PAGE_SIZE,
				prot, NULL);
	if (err) {
		mlx5_ib_err(dev,
			    "rdma_user_mmap_io failed with error=%d, mmap_cmd=%s\n",
			    err, mmap_cmd2str(cmd));
		goto err;
	}

	if (dyn_uar)
		bfregi->sys_pages[idx] = uar_index;
	return 0;

err:
	if (!dyn_uar)
		return err;

	mlx5_cmd_free_uar(dev->mdev, idx);

free_bfreg:
	mlx5_ib_free_bfreg(dev, bfregi, bfreg_dyn_idx);

	return err;
}

static unsigned long mlx5_vma_to_pgoff(struct vm_area_struct *vma)
{
	unsigned long idx;
	u8 command;

	command = get_command(vma->vm_pgoff);
	idx = get_extended_index(vma->vm_pgoff);

	return (command << 16 | idx);
}

static int mlx5_ib_mmap_offset(struct mlx5_ib_dev *dev,
			       struct vm_area_struct *vma,
			       struct ib_ucontext *ucontext)
{
	struct mlx5_user_mmap_entry *mentry;
	struct rdma_user_mmap_entry *entry;
	unsigned long pgoff;
	pgprot_t prot;
	phys_addr_t pfn;
	int ret;

	pgoff = mlx5_vma_to_pgoff(vma);
	entry = rdma_user_mmap_entry_get_pgoff(ucontext, pgoff);
	if (!entry)
		return -EINVAL;

	mentry = to_mmmap(entry);
	pfn = (mentry->address >> PAGE_SHIFT);
	if (mentry->mmap_flag == MLX5_IB_MMAP_TYPE_VAR ||
	    mentry->mmap_flag == MLX5_IB_MMAP_TYPE_UAR_NC)
		prot = pgprot_noncached(vma->vm_page_prot);
	else
		prot = pgprot_writecombine(vma->vm_page_prot);
	ret = rdma_user_mmap_io(ucontext, vma, pfn,
				entry->npages * PAGE_SIZE,
				prot,
				entry);
	rdma_user_mmap_entry_put(&mentry->rdma_entry);
	return ret;
}

static int mlx5_ib_mmap(struct ib_ucontext *ibcontext, struct vm_area_struct *vma)
{
	struct mlx5_ib_ucontext *context = to_mucontext(ibcontext);
	struct mlx5_ib_dev *dev = to_mdev(ibcontext->device);
	unsigned long command;
	phys_addr_t pfn;

	command = get_command(vma->vm_pgoff);
	switch (command) {
	case MLX5_IB_MMAP_WC_PAGE:
	case MLX5_IB_MMAP_ALLOC_WC:
		if (!dev->wc_support)
			return -EPERM;
		/* FALLTHROUGH */
	case MLX5_IB_MMAP_NC_PAGE:
	case MLX5_IB_MMAP_REGULAR_PAGE:
		return uar_mmap(dev, command, vma, context);

	case MLX5_IB_MMAP_GET_CONTIGUOUS_PAGES:
		return -ENOSYS;

	case MLX5_IB_MMAP_CORE_CLOCK:
		if (vma->vm_end - vma->vm_start != PAGE_SIZE)
			return -EINVAL;

		if (vma->vm_flags & VM_WRITE)
			return -EPERM;

		/* Don't expose to user-space information it shouldn't have */
		if (PAGE_SIZE > 4096)
			return -EOPNOTSUPP;

		pfn = (dev->mdev->iseg_base +
		       offsetof(struct mlx5_init_seg, internal_timer_h)) >>
			PAGE_SHIFT;
		return rdma_user_mmap_io(&context->ibucontext, vma, pfn,
					 PAGE_SIZE,
					 pgprot_noncached(vma->vm_page_prot),
					 NULL);
	case MLX5_IB_MMAP_CLOCK_INFO:
		return mlx5_ib_mmap_clock_info_page(dev, vma, context);

	default:
		return mlx5_ib_mmap_offset(dev, vma, ibcontext);
	}

	return 0;
}

static int mlx5_ib_alloc_pd(struct ib_pd *ibpd, struct ib_udata *udata)
{
	struct mlx5_ib_pd *pd = to_mpd(ibpd);
	struct ib_device *ibdev = ibpd->device;
	struct mlx5_ib_alloc_pd_resp resp;
	int err;
	struct mlx5_ib_ucontext *context = rdma_udata_to_drv_context(
		udata, struct mlx5_ib_ucontext, ibucontext);
	u16 uid = context ? context->devx_uid : 0;

	err = mlx5_core_alloc_pd(to_mdev(ibdev)->mdev, &pd->pdn, uid);
	if (err)
		return (err);

	pd->uid = uid;
	if (udata) {
		resp.pdn = pd->pdn;
		if (ib_copy_to_udata(udata, &resp, sizeof(resp))) {
			mlx5_core_dealloc_pd(to_mdev(ibdev)->mdev, pd->pdn, uid);
			return -EFAULT;
		}
	}

	return 0;
}

static void mlx5_ib_dealloc_pd(struct ib_pd *pd, struct ib_udata *udata)
{
	struct mlx5_ib_dev *mdev = to_mdev(pd->device);
	struct mlx5_ib_pd *mpd = to_mpd(pd);

	mlx5_core_dealloc_pd(mdev->mdev, mpd->pdn, mpd->uid);
}

enum {
	MATCH_CRITERIA_ENABLE_OUTER_BIT,
	MATCH_CRITERIA_ENABLE_MISC_BIT,
	MATCH_CRITERIA_ENABLE_INNER_BIT
};

#define HEADER_IS_ZERO(match_criteria, headers)			           \
	!(memchr_inv(MLX5_ADDR_OF(fte_match_param, match_criteria, headers), \
		    0, MLX5_FLD_SZ_BYTES(fte_match_param, headers)))       \

static u8 get_match_criteria_enable(u32 *match_criteria)
{
	u8 match_criteria_enable;

	match_criteria_enable =
		(!HEADER_IS_ZERO(match_criteria, outer_headers)) <<
		MATCH_CRITERIA_ENABLE_OUTER_BIT;
	match_criteria_enable |=
		(!HEADER_IS_ZERO(match_criteria, misc_parameters)) <<
		MATCH_CRITERIA_ENABLE_MISC_BIT;
	match_criteria_enable |=
		(!HEADER_IS_ZERO(match_criteria, inner_headers)) <<
		MATCH_CRITERIA_ENABLE_INNER_BIT;

	return match_criteria_enable;
}

static void set_proto(void *outer_c, void *outer_v, u8 mask, u8 val)
{
	MLX5_SET(fte_match_set_lyr_2_4, outer_c, ip_protocol, mask);
	MLX5_SET(fte_match_set_lyr_2_4, outer_v, ip_protocol, val);
}

static void set_tos(void *outer_c, void *outer_v, u8 mask, u8 val)
{
	MLX5_SET(fte_match_set_lyr_2_4, outer_c, ip_ecn, mask);
	MLX5_SET(fte_match_set_lyr_2_4, outer_v, ip_ecn, val);
	MLX5_SET(fte_match_set_lyr_2_4, outer_c, ip_dscp, mask >> 2);
	MLX5_SET(fte_match_set_lyr_2_4, outer_v, ip_dscp, val >> 2);
}

#define LAST_ETH_FIELD vlan_tag
#define LAST_IB_FIELD sl
#define LAST_IPV4_FIELD tos
#define LAST_IPV6_FIELD traffic_class
#define LAST_TCP_UDP_FIELD src_port

/* Field is the last supported field */
#define FIELDS_NOT_SUPPORTED(filter, field)\
	memchr_inv((void *)&filter.field  +\
		   sizeof(filter.field), 0,\
		   sizeof(filter) -\
		   offsetof(typeof(filter), field) -\
		   sizeof(filter.field))

static int parse_flow_attr(u32 *match_c, u32 *match_v,
			   const union ib_flow_spec *ib_spec)
{
	void *outer_headers_c = MLX5_ADDR_OF(fte_match_param, match_c,
					     outer_headers);
	void *outer_headers_v = MLX5_ADDR_OF(fte_match_param, match_v,
					     outer_headers);
	void *misc_params_c = MLX5_ADDR_OF(fte_match_param, match_c,
					   misc_parameters);
	void *misc_params_v = MLX5_ADDR_OF(fte_match_param, match_v,
					   misc_parameters);

	switch (ib_spec->type) {
	case IB_FLOW_SPEC_ETH:
		if (FIELDS_NOT_SUPPORTED(ib_spec->eth.mask, LAST_ETH_FIELD))
			return -ENOTSUPP;

		ether_addr_copy(MLX5_ADDR_OF(fte_match_set_lyr_2_4, outer_headers_c,
					     dmac_47_16),
				ib_spec->eth.mask.dst_mac);
		ether_addr_copy(MLX5_ADDR_OF(fte_match_set_lyr_2_4, outer_headers_v,
					     dmac_47_16),
				ib_spec->eth.val.dst_mac);

		ether_addr_copy(MLX5_ADDR_OF(fte_match_set_lyr_2_4, outer_headers_c,
					     smac_47_16),
				ib_spec->eth.mask.src_mac);
		ether_addr_copy(MLX5_ADDR_OF(fte_match_set_lyr_2_4, outer_headers_v,
					     smac_47_16),
				ib_spec->eth.val.src_mac);

		if (ib_spec->eth.mask.vlan_tag) {
			MLX5_SET(fte_match_set_lyr_2_4, outer_headers_c,
				 cvlan_tag, 1);
			MLX5_SET(fte_match_set_lyr_2_4, outer_headers_v,
				 cvlan_tag, 1);

			MLX5_SET(fte_match_set_lyr_2_4, outer_headers_c,
				 first_vid, ntohs(ib_spec->eth.mask.vlan_tag));
			MLX5_SET(fte_match_set_lyr_2_4, outer_headers_v,
				 first_vid, ntohs(ib_spec->eth.val.vlan_tag));

			MLX5_SET(fte_match_set_lyr_2_4, outer_headers_c,
				 first_cfi,
				 ntohs(ib_spec->eth.mask.vlan_tag) >> 12);
			MLX5_SET(fte_match_set_lyr_2_4, outer_headers_v,
				 first_cfi,
				 ntohs(ib_spec->eth.val.vlan_tag) >> 12);

			MLX5_SET(fte_match_set_lyr_2_4, outer_headers_c,
				 first_prio,
				 ntohs(ib_spec->eth.mask.vlan_tag) >> 13);
			MLX5_SET(fte_match_set_lyr_2_4, outer_headers_v,
				 first_prio,
				 ntohs(ib_spec->eth.val.vlan_tag) >> 13);
		}
		MLX5_SET(fte_match_set_lyr_2_4, outer_headers_c,
			 ethertype, ntohs(ib_spec->eth.mask.ether_type));
		MLX5_SET(fte_match_set_lyr_2_4, outer_headers_v,
			 ethertype, ntohs(ib_spec->eth.val.ether_type));
		break;
	case IB_FLOW_SPEC_IPV4:
		if (FIELDS_NOT_SUPPORTED(ib_spec->ipv4.mask, LAST_IPV4_FIELD))
			return -ENOTSUPP;

		MLX5_SET(fte_match_set_lyr_2_4, outer_headers_c,
			 ethertype, 0xffff);
		MLX5_SET(fte_match_set_lyr_2_4, outer_headers_v,
			 ethertype, ETH_P_IP);

		memcpy(MLX5_ADDR_OF(fte_match_set_lyr_2_4, outer_headers_c,
				    src_ipv4_src_ipv6.ipv4_layout.ipv4),
		       &ib_spec->ipv4.mask.src_ip,
		       sizeof(ib_spec->ipv4.mask.src_ip));
		memcpy(MLX5_ADDR_OF(fte_match_set_lyr_2_4, outer_headers_v,
				    src_ipv4_src_ipv6.ipv4_layout.ipv4),
		       &ib_spec->ipv4.val.src_ip,
		       sizeof(ib_spec->ipv4.val.src_ip));
		memcpy(MLX5_ADDR_OF(fte_match_set_lyr_2_4, outer_headers_c,
				    dst_ipv4_dst_ipv6.ipv4_layout.ipv4),
		       &ib_spec->ipv4.mask.dst_ip,
		       sizeof(ib_spec->ipv4.mask.dst_ip));
		memcpy(MLX5_ADDR_OF(fte_match_set_lyr_2_4, outer_headers_v,
				    dst_ipv4_dst_ipv6.ipv4_layout.ipv4),
		       &ib_spec->ipv4.val.dst_ip,
		       sizeof(ib_spec->ipv4.val.dst_ip));

		set_tos(outer_headers_c, outer_headers_v,
			ib_spec->ipv4.mask.tos, ib_spec->ipv4.val.tos);

		set_proto(outer_headers_c, outer_headers_v,
			  ib_spec->ipv4.mask.proto, ib_spec->ipv4.val.proto);
		break;
	case IB_FLOW_SPEC_IPV6:
		if (FIELDS_NOT_SUPPORTED(ib_spec->ipv6.mask, LAST_IPV6_FIELD))
			return -ENOTSUPP;

		MLX5_SET(fte_match_set_lyr_2_4, outer_headers_c,
			 ethertype, 0xffff);
		MLX5_SET(fte_match_set_lyr_2_4, outer_headers_v,
			 ethertype, ETH_P_IPV6);

		memcpy(MLX5_ADDR_OF(fte_match_set_lyr_2_4, outer_headers_c,
				    src_ipv4_src_ipv6.ipv6_layout.ipv6),
		       &ib_spec->ipv6.mask.src_ip,
		       sizeof(ib_spec->ipv6.mask.src_ip));
		memcpy(MLX5_ADDR_OF(fte_match_set_lyr_2_4, outer_headers_v,
				    src_ipv4_src_ipv6.ipv6_layout.ipv6),
		       &ib_spec->ipv6.val.src_ip,
		       sizeof(ib_spec->ipv6.val.src_ip));
		memcpy(MLX5_ADDR_OF(fte_match_set_lyr_2_4, outer_headers_c,
				    dst_ipv4_dst_ipv6.ipv6_layout.ipv6),
		       &ib_spec->ipv6.mask.dst_ip,
		       sizeof(ib_spec->ipv6.mask.dst_ip));
		memcpy(MLX5_ADDR_OF(fte_match_set_lyr_2_4, outer_headers_v,
				    dst_ipv4_dst_ipv6.ipv6_layout.ipv6),
		       &ib_spec->ipv6.val.dst_ip,
		       sizeof(ib_spec->ipv6.val.dst_ip));

		set_tos(outer_headers_c, outer_headers_v,
			ib_spec->ipv6.mask.traffic_class,
			ib_spec->ipv6.val.traffic_class);

		set_proto(outer_headers_c, outer_headers_v,
			  ib_spec->ipv6.mask.next_hdr,
			  ib_spec->ipv6.val.next_hdr);

		MLX5_SET(fte_match_set_misc, misc_params_c,
			 outer_ipv6_flow_label,
			 ntohl(ib_spec->ipv6.mask.flow_label));
		MLX5_SET(fte_match_set_misc, misc_params_v,
			 outer_ipv6_flow_label,
			 ntohl(ib_spec->ipv6.val.flow_label));
		break;
	case IB_FLOW_SPEC_TCP:
		if (FIELDS_NOT_SUPPORTED(ib_spec->tcp_udp.mask,
					 LAST_TCP_UDP_FIELD))
			return -ENOTSUPP;

		MLX5_SET(fte_match_set_lyr_2_4, outer_headers_c, ip_protocol,
			 0xff);
		MLX5_SET(fte_match_set_lyr_2_4, outer_headers_v, ip_protocol,
			 IPPROTO_TCP);

		MLX5_SET(fte_match_set_lyr_2_4, outer_headers_c, tcp_sport,
			 ntohs(ib_spec->tcp_udp.mask.src_port));
		MLX5_SET(fte_match_set_lyr_2_4, outer_headers_v, tcp_sport,
			 ntohs(ib_spec->tcp_udp.val.src_port));

		MLX5_SET(fte_match_set_lyr_2_4, outer_headers_c, tcp_dport,
			 ntohs(ib_spec->tcp_udp.mask.dst_port));
		MLX5_SET(fte_match_set_lyr_2_4, outer_headers_v, tcp_dport,
			 ntohs(ib_spec->tcp_udp.val.dst_port));
		break;
	case IB_FLOW_SPEC_UDP:
		if (FIELDS_NOT_SUPPORTED(ib_spec->tcp_udp.mask,
					 LAST_TCP_UDP_FIELD))
			return -ENOTSUPP;

		MLX5_SET(fte_match_set_lyr_2_4, outer_headers_c, ip_protocol,
			 0xff);
		MLX5_SET(fte_match_set_lyr_2_4, outer_headers_v, ip_protocol,
			 IPPROTO_UDP);

		MLX5_SET(fte_match_set_lyr_2_4, outer_headers_c, udp_sport,
			 ntohs(ib_spec->tcp_udp.mask.src_port));
		MLX5_SET(fte_match_set_lyr_2_4, outer_headers_v, udp_sport,
			 ntohs(ib_spec->tcp_udp.val.src_port));

		MLX5_SET(fte_match_set_lyr_2_4, outer_headers_c, udp_dport,
			 ntohs(ib_spec->tcp_udp.mask.dst_port));
		MLX5_SET(fte_match_set_lyr_2_4, outer_headers_v, udp_dport,
			 ntohs(ib_spec->tcp_udp.val.dst_port));
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/* If a flow could catch both multicast and unicast packets,
 * it won't fall into the multicast flow steering table and this rule
 * could steal other multicast packets.
 */
static bool flow_is_multicast_only(struct ib_flow_attr *ib_attr)
{
	struct ib_flow_spec_eth *eth_spec;

	if (ib_attr->type != IB_FLOW_ATTR_NORMAL ||
	    ib_attr->size < sizeof(struct ib_flow_attr) +
	    sizeof(struct ib_flow_spec_eth) ||
	    ib_attr->num_of_specs < 1)
		return false;

	eth_spec = (struct ib_flow_spec_eth *)(ib_attr + 1);
	if (eth_spec->type != IB_FLOW_SPEC_ETH ||
	    eth_spec->size != sizeof(*eth_spec))
		return false;

	return is_multicast_ether_addr(eth_spec->mask.dst_mac) &&
	       is_multicast_ether_addr(eth_spec->val.dst_mac);
}

static bool is_valid_attr(const struct ib_flow_attr *flow_attr)
{
	union ib_flow_spec *ib_spec = (union ib_flow_spec *)(flow_attr + 1);
	bool has_ipv4_spec = false;
	bool eth_type_ipv4 = true;
	unsigned int spec_index;

	/* Validate that ethertype is correct */
	for (spec_index = 0; spec_index < flow_attr->num_of_specs; spec_index++) {
		if (ib_spec->type == IB_FLOW_SPEC_ETH &&
		    ib_spec->eth.mask.ether_type) {
			if (!((ib_spec->eth.mask.ether_type == htons(0xffff)) &&
			      ib_spec->eth.val.ether_type == htons(ETH_P_IP)))
				eth_type_ipv4 = false;
		} else if (ib_spec->type == IB_FLOW_SPEC_IPV4) {
			has_ipv4_spec = true;
		}
		ib_spec = (void *)ib_spec + ib_spec->size;
	}
	return !has_ipv4_spec || eth_type_ipv4;
}

static void put_flow_table(struct mlx5_ib_dev *dev,
			   struct mlx5_ib_flow_prio *prio, bool ft_added)
{
	prio->refcount -= !!ft_added;
	if (!prio->refcount) {
		mlx5_destroy_flow_table(prio->flow_table);
		prio->flow_table = NULL;
	}
}

static int mlx5_ib_destroy_flow(struct ib_flow *flow_id)
{
	struct mlx5_ib_dev *dev = to_mdev(flow_id->qp->device);
	struct mlx5_ib_flow_handler *handler = container_of(flow_id,
							  struct mlx5_ib_flow_handler,
							  ibflow);
	struct mlx5_ib_flow_handler *iter, *tmp;

	mutex_lock(&dev->flow_db.lock);

	list_for_each_entry_safe(iter, tmp, &handler->list, list) {
		mlx5_del_flow_rules(&iter->rule);
		put_flow_table(dev, iter->prio, true);
		list_del(&iter->list);
		kfree(iter);
	}

	mlx5_del_flow_rules(&handler->rule);
	put_flow_table(dev, handler->prio, true);
	mutex_unlock(&dev->flow_db.lock);

	kfree(handler);

	return 0;
}

static int ib_prio_to_core_prio(unsigned int priority, bool dont_trap)
{
	priority *= 2;
	if (!dont_trap)
		priority++;
	return priority;
}

enum flow_table_type {
	MLX5_IB_FT_RX,
	MLX5_IB_FT_TX
};

#define MLX5_FS_MAX_TYPES	 10
#define MLX5_FS_MAX_ENTRIES	 32000UL
static struct mlx5_ib_flow_prio *get_flow_table(struct mlx5_ib_dev *dev,
						struct ib_flow_attr *flow_attr,
						enum flow_table_type ft_type)
{
	bool dont_trap = flow_attr->flags & IB_FLOW_ATTR_FLAGS_DONT_TRAP;
	struct mlx5_flow_table_attr ft_attr = {};
	struct mlx5_flow_namespace *ns = NULL;
	struct mlx5_ib_flow_prio *prio;
	struct mlx5_flow_table *ft;
	int num_entries;
	int num_groups;
	int priority;
	int err = 0;

	if (flow_attr->type == IB_FLOW_ATTR_NORMAL) {
		if (flow_is_multicast_only(flow_attr) &&
		    !dont_trap)
			priority = MLX5_IB_FLOW_MCAST_PRIO;
		else
			priority = ib_prio_to_core_prio(flow_attr->priority,
							dont_trap);
		ns = mlx5_get_flow_namespace(dev->mdev,
					     MLX5_FLOW_NAMESPACE_BYPASS);
		num_entries = MLX5_FS_MAX_ENTRIES;
		num_groups = MLX5_FS_MAX_TYPES;
		prio = &dev->flow_db.prios[priority];
	} else if (flow_attr->type == IB_FLOW_ATTR_ALL_DEFAULT ||
		   flow_attr->type == IB_FLOW_ATTR_MC_DEFAULT) {
		ns = mlx5_get_flow_namespace(dev->mdev,
					     MLX5_FLOW_NAMESPACE_LEFTOVERS);
		build_leftovers_ft_param("bypass", &priority,
					 &num_entries,
					 &num_groups);
		prio = &dev->flow_db.prios[MLX5_IB_FLOW_LEFTOVERS_PRIO];
	} else if (flow_attr->type == IB_FLOW_ATTR_SNIFFER) {
		if (!MLX5_CAP_FLOWTABLE(dev->mdev,
					allow_sniffer_and_nic_rx_shared_tir))
			return ERR_PTR(-ENOTSUPP);

		ns = mlx5_get_flow_namespace(dev->mdev, ft_type == MLX5_IB_FT_RX ?
					     MLX5_FLOW_NAMESPACE_SNIFFER_RX :
					     MLX5_FLOW_NAMESPACE_SNIFFER_TX);

		prio = &dev->flow_db.sniffer[ft_type];
		priority = 0;
		num_entries = 1;
		num_groups = 1;
	}

	if (!ns)
		return ERR_PTR(-ENOTSUPP);

	ft = prio->flow_table;
	if (!ft) {
		ft_attr.prio = priority;
		ft_attr.max_fte = num_entries;
		ft_attr.autogroup.max_num_groups = num_groups;

		ft = mlx5_create_auto_grouped_flow_table(ns, &ft_attr);

		if (!IS_ERR(ft)) {
			prio->refcount = 0;
			prio->flow_table = ft;
		} else {
			err = PTR_ERR(ft);
		}
	}

	return err ? ERR_PTR(err) : prio;
}

static struct mlx5_ib_flow_handler *create_flow_rule(struct mlx5_ib_dev *dev,
						     struct mlx5_ib_flow_prio *ft_prio,
						     const struct ib_flow_attr *flow_attr,
						     struct mlx5_flow_destination *dst)
{
	struct mlx5_flow_table	*ft = ft_prio->flow_table;
	struct mlx5_ib_flow_handler *handler;
	struct mlx5_flow_spec *spec;
	const void *ib_flow = (const void *)flow_attr + sizeof(*flow_attr);
	unsigned int spec_index;
	struct mlx5_flow_act flow_act = {};

	u32 action;
	int err = 0;

	if (!is_valid_attr(flow_attr))
		return ERR_PTR(-EINVAL);

	spec = mlx5_vzalloc(sizeof(*spec));
	handler = kzalloc(sizeof(*handler), GFP_KERNEL);
	if (!handler || !spec) {
		err = -ENOMEM;
		goto free;
	}

	spec->flow_context.flags = FLOW_CONTEXT_HAS_TAG;
	spec->flow_context.flow_tag = MLX5_FS_DEFAULT_FLOW_TAG;

	INIT_LIST_HEAD(&handler->list);

	for (spec_index = 0; spec_index < flow_attr->num_of_specs; spec_index++) {
		err = parse_flow_attr(spec->match_criteria,
				      spec->match_value, ib_flow);
		if (err < 0)
			goto free;

		ib_flow += ((union ib_flow_spec *)ib_flow)->size;
	}

	spec->match_criteria_enable = get_match_criteria_enable(spec->match_criteria);
	action = dst ? MLX5_FLOW_CONTEXT_ACTION_FWD_DEST : 0;
	flow_act.action = action;
	handler->rule = mlx5_add_flow_rules(ft, spec, &flow_act, dst, 1);

	if (IS_ERR(handler->rule)) {
		err = PTR_ERR(handler->rule);
		goto free;
	}

	ft_prio->refcount++;
	handler->prio = ft_prio;

	ft_prio->flow_table = ft;
free:
	if (err)
		kfree(handler);
	kvfree(spec);
	return err ? ERR_PTR(err) : handler;
}

static struct mlx5_ib_flow_handler *create_dont_trap_rule(struct mlx5_ib_dev *dev,
							  struct mlx5_ib_flow_prio *ft_prio,
							  struct ib_flow_attr *flow_attr,
							  struct mlx5_flow_destination *dst)
{
	struct mlx5_ib_flow_handler *handler_dst = NULL;
	struct mlx5_ib_flow_handler *handler = NULL;

	handler = create_flow_rule(dev, ft_prio, flow_attr, NULL);
	if (!IS_ERR(handler)) {
		handler_dst = create_flow_rule(dev, ft_prio,
					       flow_attr, dst);
		if (IS_ERR(handler_dst)) {
			mlx5_del_flow_rules(&handler->rule);
			ft_prio->refcount--;
			kfree(handler);
			handler = handler_dst;
		} else {
			list_add(&handler_dst->list, &handler->list);
		}
	}

	return handler;
}
enum {
	LEFTOVERS_MC,
	LEFTOVERS_UC,
};

static struct mlx5_ib_flow_handler *create_leftovers_rule(struct mlx5_ib_dev *dev,
							  struct mlx5_ib_flow_prio *ft_prio,
							  struct ib_flow_attr *flow_attr,
							  struct mlx5_flow_destination *dst)
{
	struct mlx5_ib_flow_handler *handler_ucast = NULL;
	struct mlx5_ib_flow_handler *handler = NULL;

	static struct {
		struct ib_flow_attr	flow_attr;
		struct ib_flow_spec_eth eth_flow;
	} leftovers_specs[] = {
		[LEFTOVERS_MC] = {
			.flow_attr = {
				.num_of_specs = 1,
				.size = sizeof(leftovers_specs[0])
			},
			.eth_flow = {
				.type = IB_FLOW_SPEC_ETH,
				.size = sizeof(struct ib_flow_spec_eth),
				.mask = {.dst_mac = {0x1} },
				.val =  {.dst_mac = {0x1} }
			}
		},
		[LEFTOVERS_UC] = {
			.flow_attr = {
				.num_of_specs = 1,
				.size = sizeof(leftovers_specs[0])
			},
			.eth_flow = {
				.type = IB_FLOW_SPEC_ETH,
				.size = sizeof(struct ib_flow_spec_eth),
				.mask = {.dst_mac = {0x1} },
				.val = {.dst_mac = {} }
			}
		}
	};

	handler = create_flow_rule(dev, ft_prio,
				   &leftovers_specs[LEFTOVERS_MC].flow_attr,
				   dst);
	if (!IS_ERR(handler) &&
	    flow_attr->type == IB_FLOW_ATTR_ALL_DEFAULT) {
		handler_ucast = create_flow_rule(dev, ft_prio,
						 &leftovers_specs[LEFTOVERS_UC].flow_attr,
						 dst);
		if (IS_ERR(handler_ucast)) {
			mlx5_del_flow_rules(&handler->rule);
			ft_prio->refcount--;
			kfree(handler);
			handler = handler_ucast;
		} else {
			list_add(&handler_ucast->list, &handler->list);
		}
	}

	return handler;
}

static struct mlx5_ib_flow_handler *create_sniffer_rule(struct mlx5_ib_dev *dev,
							struct mlx5_ib_flow_prio *ft_rx,
							struct mlx5_ib_flow_prio *ft_tx,
							struct mlx5_flow_destination *dst)
{
	struct mlx5_ib_flow_handler *handler_rx;
	struct mlx5_ib_flow_handler *handler_tx;
	int err;
	static const struct ib_flow_attr flow_attr  = {
		.num_of_specs = 0,
		.type = IB_FLOW_ATTR_SNIFFER,
		.size = sizeof(flow_attr)
	};

	handler_rx = create_flow_rule(dev, ft_rx, &flow_attr, dst);
	if (IS_ERR(handler_rx)) {
		err = PTR_ERR(handler_rx);
		goto err;
	}

	handler_tx = create_flow_rule(dev, ft_tx, &flow_attr, dst);
	if (IS_ERR(handler_tx)) {
		err = PTR_ERR(handler_tx);
		goto err_tx;
	}

	list_add(&handler_tx->list, &handler_rx->list);

	return handler_rx;

err_tx:
	mlx5_del_flow_rules(&handler_rx->rule);
	ft_rx->refcount--;
	kfree(handler_rx);
err:
	return ERR_PTR(err);
}

static struct ib_flow *mlx5_ib_create_flow(struct ib_qp *qp,
					   struct ib_flow_attr *flow_attr,
					   int domain,
					   struct ib_udata *udata)
{
	struct mlx5_ib_dev *dev = to_mdev(qp->device);
	struct mlx5_ib_qp *mqp = to_mqp(qp);
	struct mlx5_ib_flow_handler *handler = NULL;
	struct mlx5_flow_destination *dst = NULL;
	struct mlx5_ib_flow_prio *ft_prio_tx = NULL;
	struct mlx5_ib_flow_prio *ft_prio;
	struct mlx5_ib_create_flow *ucmd = NULL, ucmd_hdr;
	size_t min_ucmd_sz, required_ucmd_sz;
	int err;

	if (udata && udata->inlen) {
		min_ucmd_sz = offsetofend(struct mlx5_ib_create_flow, reserved);
		if (udata->inlen < min_ucmd_sz)
			return ERR_PTR(-EOPNOTSUPP);

		err = ib_copy_from_udata(&ucmd_hdr, udata, min_ucmd_sz);
		if (err)
			return ERR_PTR(err);

		/* currently supports only one counters data */
		if (ucmd_hdr.ncounters_data > 1)
			return ERR_PTR(-EINVAL);

		required_ucmd_sz = min_ucmd_sz +
			sizeof(struct mlx5_ib_flow_counters_data) *
			ucmd_hdr.ncounters_data;
		if (udata->inlen > required_ucmd_sz &&
		    !ib_is_udata_cleared(udata, required_ucmd_sz,
					 udata->inlen - required_ucmd_sz))
			return ERR_PTR(-EOPNOTSUPP);

		ucmd = kzalloc(required_ucmd_sz, GFP_KERNEL);
		if (!ucmd)
			return ERR_PTR(-ENOMEM);

		err = ib_copy_from_udata(ucmd, udata, required_ucmd_sz);
		if (err)
			goto free_ucmd;
	}

	if (flow_attr->priority > MLX5_IB_FLOW_LAST_PRIO) {
		err = -ENOMEM;
		goto free_ucmd;
	}

	if (flow_attr->flags & ~IB_FLOW_ATTR_FLAGS_DONT_TRAP) {
		err = -EINVAL;
		goto free_ucmd;
	}

	dst = kzalloc(sizeof(*dst), GFP_KERNEL);
	if (!dst) {
		err = -ENOMEM;
		goto free_ucmd;
	}

	mutex_lock(&dev->flow_db.lock);

	ft_prio = get_flow_table(dev, flow_attr, MLX5_IB_FT_RX);
	if (IS_ERR(ft_prio)) {
		err = PTR_ERR(ft_prio);
		goto unlock;
	}
	if (flow_attr->type == IB_FLOW_ATTR_SNIFFER) {
		ft_prio_tx = get_flow_table(dev, flow_attr, MLX5_IB_FT_TX);
		if (IS_ERR(ft_prio_tx)) {
			err = PTR_ERR(ft_prio_tx);
			ft_prio_tx = NULL;
			goto destroy_ft;
		}
	}

	dst->type = MLX5_FLOW_DESTINATION_TYPE_TIR;
	if (mqp->flags & MLX5_IB_QP_RSS)
		dst->tir_num = mqp->rss_qp.tirn;
	else
		dst->tir_num = mqp->raw_packet_qp.rq.tirn;

	switch (flow_attr->type) {
	case IB_FLOW_ATTR_NORMAL:
		if (mqp->flags & IB_QP_CREATE_SOURCE_QPN) {
			err = -EOPNOTSUPP;
			goto destroy_ft;
		}
		if (flow_attr->flags & IB_FLOW_ATTR_FLAGS_DONT_TRAP) {
			handler = create_dont_trap_rule(dev, ft_prio, flow_attr, dst);
		} else {
			handler = create_flow_rule(dev, ft_prio, flow_attr, dst);
		}
		break;
	case IB_FLOW_ATTR_ALL_DEFAULT:
	case IB_FLOW_ATTR_MC_DEFAULT:
		handler = create_leftovers_rule(dev, ft_prio, flow_attr, dst);
		break;
	case IB_FLOW_ATTR_SNIFFER:
		handler = create_sniffer_rule(dev, ft_prio, ft_prio_tx, dst);
		break;
	default:
		err = -EINVAL;
		goto destroy_ft;
	}

	if (IS_ERR(handler)) {
		err = PTR_ERR(handler);
		handler = NULL;
		goto destroy_ft;
	}

	mutex_unlock(&dev->flow_db.lock);
	kfree(dst);
	kfree(ucmd);

	return &handler->ibflow;

destroy_ft:
	put_flow_table(dev, ft_prio, false);
	if (ft_prio_tx)
		put_flow_table(dev, ft_prio_tx, false);
unlock:
	mutex_unlock(&dev->flow_db.lock);
	kfree(dst);
free_ucmd:
	kfree(ucmd);
	return ERR_PTR(err);
}

static int mlx5_ib_mcg_attach(struct ib_qp *ibqp, union ib_gid *gid, u16 lid)
{
	struct mlx5_ib_dev *dev = to_mdev(ibqp->device);
	int err;

	err = mlx5_core_attach_mcg(dev->mdev, gid, ibqp->qp_num);
	if (err)
		mlx5_ib_warn(dev, "failed attaching QPN 0x%x, MGID %pI6\n",
			     ibqp->qp_num, gid->raw);

	return err;
}

static int mlx5_ib_mcg_detach(struct ib_qp *ibqp, union ib_gid *gid, u16 lid)
{
	struct mlx5_ib_dev *dev = to_mdev(ibqp->device);
	int err;

	err = mlx5_core_detach_mcg(dev->mdev, gid, ibqp->qp_num);
	if (err)
		mlx5_ib_warn(dev, "failed detaching QPN 0x%x, MGID %pI6\n",
			     ibqp->qp_num, gid->raw);

	return err;
}

static int init_node_data(struct mlx5_ib_dev *dev)
{
	int err;

	err = mlx5_query_node_desc(dev, dev->ib_dev.node_desc);
	if (err)
		return err;

	return mlx5_query_node_guid(dev, &dev->ib_dev.node_guid);
}

static ssize_t show_fw_pages(struct device *device, struct device_attribute *attr,
			     char *buf)
{
	struct mlx5_ib_dev *dev =
		container_of(device, struct mlx5_ib_dev, ib_dev.dev);

	return sprintf(buf, "%lld\n", (long long)dev->mdev->priv.fw_pages);
}

static ssize_t show_reg_pages(struct device *device,
			      struct device_attribute *attr, char *buf)
{
	struct mlx5_ib_dev *dev =
		container_of(device, struct mlx5_ib_dev, ib_dev.dev);

	return sprintf(buf, "%d\n", atomic_read(&dev->mdev->priv.reg_pages));
}

static ssize_t show_hca(struct device *device, struct device_attribute *attr,
			char *buf)
{
	struct mlx5_ib_dev *dev =
		container_of(device, struct mlx5_ib_dev, ib_dev.dev);
	return sprintf(buf, "MT%d\n", dev->mdev->pdev->device);
}

static ssize_t show_rev(struct device *device, struct device_attribute *attr,
			char *buf)
{
	struct mlx5_ib_dev *dev =
		container_of(device, struct mlx5_ib_dev, ib_dev.dev);
	return sprintf(buf, "%x\n", dev->mdev->pdev->revision);
}

static ssize_t show_board(struct device *device, struct device_attribute *attr,
			  char *buf)
{
	struct mlx5_ib_dev *dev =
		container_of(device, struct mlx5_ib_dev, ib_dev.dev);
	return sprintf(buf, "%.*s\n", MLX5_BOARD_ID_LEN,
		       dev->mdev->board_id);
}

static DEVICE_ATTR(hw_rev,   S_IRUGO, show_rev,    NULL);
static DEVICE_ATTR(hca_type, S_IRUGO, show_hca,    NULL);
static DEVICE_ATTR(board_id, S_IRUGO, show_board,  NULL);
static DEVICE_ATTR(fw_pages, S_IRUGO, show_fw_pages, NULL);
static DEVICE_ATTR(reg_pages, S_IRUGO, show_reg_pages, NULL);

static struct device_attribute *mlx5_class_attributes[] = {
	&dev_attr_hw_rev,
	&dev_attr_hca_type,
	&dev_attr_board_id,
	&dev_attr_fw_pages,
	&dev_attr_reg_pages,
};

static void pkey_change_handler(struct work_struct *work)
{
	struct mlx5_ib_port_resources *ports =
		container_of(work, struct mlx5_ib_port_resources,
			     pkey_change_work);

	mutex_lock(&ports->devr->mutex);
	mlx5_ib_gsi_pkey_change(ports->gsi);
	mutex_unlock(&ports->devr->mutex);
}

static void mlx5_ib_handle_internal_error(struct mlx5_ib_dev *ibdev)
{
	struct mlx5_ib_qp *mqp;
	struct mlx5_ib_cq *send_mcq, *recv_mcq;
	struct mlx5_core_cq *mcq;
	struct list_head cq_armed_list;
	unsigned long flags_qp;
	unsigned long flags_cq;
	unsigned long flags;

	INIT_LIST_HEAD(&cq_armed_list);

	/* Go over qp list reside on that ibdev, sync with create/destroy qp.*/
	spin_lock_irqsave(&ibdev->reset_flow_resource_lock, flags);
	list_for_each_entry(mqp, &ibdev->qp_list, qps_list) {
		spin_lock_irqsave(&mqp->sq.lock, flags_qp);
		if (mqp->sq.tail != mqp->sq.head) {
			send_mcq = to_mcq(mqp->ibqp.send_cq);
			spin_lock_irqsave(&send_mcq->lock, flags_cq);
			if (send_mcq->mcq.comp &&
			    mqp->ibqp.send_cq->comp_handler) {
				if (!send_mcq->mcq.reset_notify_added) {
					send_mcq->mcq.reset_notify_added = 1;
					list_add_tail(&send_mcq->mcq.reset_notify,
						      &cq_armed_list);
				}
			}
			spin_unlock_irqrestore(&send_mcq->lock, flags_cq);
		}
		spin_unlock_irqrestore(&mqp->sq.lock, flags_qp);
		spin_lock_irqsave(&mqp->rq.lock, flags_qp);
		/* no handling is needed for SRQ */
		if (!mqp->ibqp.srq) {
			if (mqp->rq.tail != mqp->rq.head) {
				recv_mcq = to_mcq(mqp->ibqp.recv_cq);
				spin_lock_irqsave(&recv_mcq->lock, flags_cq);
				if (recv_mcq->mcq.comp &&
				    mqp->ibqp.recv_cq->comp_handler) {
					if (!recv_mcq->mcq.reset_notify_added) {
						recv_mcq->mcq.reset_notify_added = 1;
						list_add_tail(&recv_mcq->mcq.reset_notify,
							      &cq_armed_list);
					}
				}
				spin_unlock_irqrestore(&recv_mcq->lock,
						       flags_cq);
			}
		}
		spin_unlock_irqrestore(&mqp->rq.lock, flags_qp);
	}
	/*At that point all inflight post send were put to be executed as of we
	 * lock/unlock above locks Now need to arm all involved CQs.
	 */
	list_for_each_entry(mcq, &cq_armed_list, reset_notify) {
		mcq->comp(mcq, NULL);
	}
	spin_unlock_irqrestore(&ibdev->reset_flow_resource_lock, flags);
}

static void mlx5_ib_event(struct mlx5_core_dev *dev, void *context,
			  enum mlx5_dev_event event, unsigned long param)
{
	struct mlx5_ib_dev *ibdev = (struct mlx5_ib_dev *)context;
	struct ib_event ibev;
	bool fatal = false;
	u8 port = (u8)param;

	switch (event) {
	case MLX5_DEV_EVENT_SYS_ERROR:
		ibev.event = IB_EVENT_DEVICE_FATAL;
		mlx5_ib_handle_internal_error(ibdev);
		fatal = true;
		break;

	case MLX5_DEV_EVENT_PORT_UP:
	case MLX5_DEV_EVENT_PORT_DOWN:
	case MLX5_DEV_EVENT_PORT_INITIALIZED:
		/* In RoCE, port up/down events are handled in
		 * mlx5_netdev_event().
		 */
		if (mlx5_ib_port_link_layer(&ibdev->ib_dev, port) ==
			IB_LINK_LAYER_ETHERNET)
			return;

		ibev.event = (event == MLX5_DEV_EVENT_PORT_UP) ?
			     IB_EVENT_PORT_ACTIVE : IB_EVENT_PORT_ERR;
		break;

	case MLX5_DEV_EVENT_LID_CHANGE:
		ibev.event = IB_EVENT_LID_CHANGE;
		break;

	case MLX5_DEV_EVENT_PKEY_CHANGE:
		ibev.event = IB_EVENT_PKEY_CHANGE;

		schedule_work(&ibdev->devr.ports[port - 1].pkey_change_work);
		break;

	case MLX5_DEV_EVENT_GUID_CHANGE:
		ibev.event = IB_EVENT_GID_CHANGE;
		break;

	case MLX5_DEV_EVENT_CLIENT_REREG:
		ibev.event = IB_EVENT_CLIENT_REREGISTER;
		break;

	default:
		/* unsupported event */
		return;
	}

	ibev.device	      = &ibdev->ib_dev;
	ibev.element.port_num = port;

	if (!rdma_is_port_valid(&ibdev->ib_dev, port)) {
		mlx5_ib_warn(ibdev, "warning: event(%d) on port %d\n", event, port);
		return;
	}

	if (ibdev->ib_active)
		ib_dispatch_event(&ibev);

	if (fatal)
		ibdev->ib_active = false;
}

static void get_ext_port_caps(struct mlx5_ib_dev *dev)
{
	int port;

	for (port = 1; port <= MLX5_CAP_GEN(dev->mdev, num_ports); port++)
		mlx5_query_ext_port_caps(dev, port);
}

static int get_port_caps(struct mlx5_ib_dev *dev)
{
	struct ib_device_attr *dprops = NULL;
	struct ib_port_attr *pprops = NULL;
	int err = -ENOMEM;
	int port;
	struct ib_udata uhw = {.inlen = 0, .outlen = 0};

	pprops = kmalloc(sizeof(*pprops), GFP_KERNEL);
	if (!pprops)
		goto out;

	dprops = kmalloc(sizeof(*dprops), GFP_KERNEL);
	if (!dprops)
		goto out;

	err = mlx5_ib_query_device(&dev->ib_dev, dprops, &uhw);
	if (err) {
		mlx5_ib_warn(dev, "query_device failed %d\n", err);
		goto out;
	}

	for (port = 1; port <= MLX5_CAP_GEN(dev->mdev, num_ports); port++) {
		err = mlx5_ib_query_port(&dev->ib_dev, port, pprops);
		if (err) {
			mlx5_ib_warn(dev, "query_port %d failed %d\n",
				     port, err);
			break;
		}
		dev->mdev->port_caps[port - 1].pkey_table_len =
						dprops->max_pkeys;
		dev->mdev->port_caps[port - 1].gid_table_len =
						pprops->gid_tbl_len;
		mlx5_ib_dbg(dev, "pkey_table_len %d, gid_table_len %d\n",
			    dprops->max_pkeys, pprops->gid_tbl_len);
	}

out:
	kfree(pprops);
	kfree(dprops);

	return err;
}

static void destroy_umrc_res(struct mlx5_ib_dev *dev)
{
	int err;

	err = mlx5_mr_cache_cleanup(dev);
	if (err)
		mlx5_ib_warn(dev, "mr cache cleanup failed\n");

	if (dev->umrc.qp)
		mlx5_ib_destroy_qp(dev->umrc.qp, NULL);
	if (dev->umrc.cq)
		ib_free_cq(dev->umrc.cq);
	if (dev->umrc.pd)
		ib_dealloc_pd(dev->umrc.pd);
}

enum {
	MAX_UMR_WR = 128,
};

static int create_umr_res(struct mlx5_ib_dev *dev)
{
	struct ib_qp_init_attr *init_attr = NULL;
	struct ib_qp_attr *attr = NULL;
	struct ib_pd *pd;
	struct ib_cq *cq;
	struct ib_qp *qp;
	int ret;

	attr = kzalloc(sizeof(*attr), GFP_KERNEL);
	init_attr = kzalloc(sizeof(*init_attr), GFP_KERNEL);
	if (!attr || !init_attr) {
		ret = -ENOMEM;
		goto error_0;
	}

	pd = ib_alloc_pd(&dev->ib_dev, 0);
	if (IS_ERR(pd)) {
		mlx5_ib_dbg(dev, "Couldn't create PD for sync UMR QP\n");
		ret = PTR_ERR(pd);
		goto error_0;
	}

	cq = ib_alloc_cq(&dev->ib_dev, NULL, 128, 0, IB_POLL_SOFTIRQ);
	if (IS_ERR(cq)) {
		mlx5_ib_dbg(dev, "Couldn't create CQ for sync UMR QP\n");
		ret = PTR_ERR(cq);
		goto error_2;
	}

	init_attr->send_cq = cq;
	init_attr->recv_cq = cq;
	init_attr->sq_sig_type = IB_SIGNAL_ALL_WR;
	init_attr->cap.max_send_wr = MAX_UMR_WR;
	init_attr->cap.max_send_sge = 1;
	init_attr->qp_type = MLX5_IB_QPT_REG_UMR;
	init_attr->port_num = 1;
	qp = mlx5_ib_create_qp(pd, init_attr, NULL);
	if (IS_ERR(qp)) {
		mlx5_ib_dbg(dev, "Couldn't create sync UMR QP\n");
		ret = PTR_ERR(qp);
		goto error_3;
	}
	qp->device     = &dev->ib_dev;
	qp->real_qp    = qp;
	qp->uobject    = NULL;
	qp->qp_type    = MLX5_IB_QPT_REG_UMR;

	attr->qp_state = IB_QPS_INIT;
	attr->port_num = 1;
	ret = mlx5_ib_modify_qp(qp, attr, IB_QP_STATE | IB_QP_PKEY_INDEX |
				IB_QP_PORT, NULL);
	if (ret) {
		mlx5_ib_dbg(dev, "Couldn't modify UMR QP\n");
		goto error_4;
	}

	memset(attr, 0, sizeof(*attr));
	attr->qp_state = IB_QPS_RTR;
	attr->path_mtu = IB_MTU_256;

	ret = mlx5_ib_modify_qp(qp, attr, IB_QP_STATE, NULL);
	if (ret) {
		mlx5_ib_dbg(dev, "Couldn't modify umr QP to rtr\n");
		goto error_4;
	}

	memset(attr, 0, sizeof(*attr));
	attr->qp_state = IB_QPS_RTS;
	ret = mlx5_ib_modify_qp(qp, attr, IB_QP_STATE, NULL);
	if (ret) {
		mlx5_ib_dbg(dev, "Couldn't modify umr QP to rts\n");
		goto error_4;
	}

	dev->umrc.qp = qp;
	dev->umrc.cq = cq;
	dev->umrc.pd = pd;

	sema_init(&dev->umrc.sem, MAX_UMR_WR);
	ret = mlx5_mr_cache_init(dev);
	if (ret) {
		mlx5_ib_warn(dev, "mr cache init failed %d\n", ret);
		goto error_4;
	}

	kfree(attr);
	kfree(init_attr);

	return 0;

error_4:
	mlx5_ib_destroy_qp(qp, NULL);
	dev->umrc.qp = NULL;

error_3:
	ib_free_cq(cq);
	dev->umrc.cq = NULL;

error_2:
	ib_dealloc_pd(pd);
	dev->umrc.pd = NULL;

error_0:
	kfree(attr);
	kfree(init_attr);
	return ret;
}

static int create_dev_resources(struct mlx5_ib_resources *devr)
{
	struct ib_srq_init_attr attr;
	struct mlx5_ib_dev *dev;
	struct ib_device *ibdev;
	struct ib_cq_init_attr cq_attr = {.cqe = 1};
	int port;
	int ret = 0;

	dev = container_of(devr, struct mlx5_ib_dev, devr);
	ibdev = &dev->ib_dev;

	mutex_init(&devr->mutex);

	devr->p0 = rdma_zalloc_drv_obj(ibdev, ib_pd);
	if (!devr->p0)
		return -ENOMEM;

	devr->p0->device  = ibdev;
	devr->p0->uobject = NULL;
	atomic_set(&devr->p0->usecnt, 0);

	ret = mlx5_ib_alloc_pd(devr->p0, NULL);
	if (ret)
		goto error0;

	devr->c0 = rdma_zalloc_drv_obj(ibdev, ib_cq);
	if (!devr->c0) {
		ret = -ENOMEM;
		goto error1;
	}

	devr->c0->device = &dev->ib_dev;
	atomic_set(&devr->c0->usecnt, 0);

	ret = mlx5_ib_create_cq(devr->c0, &cq_attr, NULL);
	if (ret)
		goto err_create_cq;

	devr->x0 = mlx5_ib_alloc_xrcd(&dev->ib_dev, NULL);
	if (IS_ERR(devr->x0)) {
		ret = PTR_ERR(devr->x0);
		goto error2;
	}
	devr->x0->device = &dev->ib_dev;
	devr->x0->inode = NULL;
	atomic_set(&devr->x0->usecnt, 0);
	mutex_init(&devr->x0->tgt_qp_mutex);
	INIT_LIST_HEAD(&devr->x0->tgt_qp_list);

	devr->x1 = mlx5_ib_alloc_xrcd(&dev->ib_dev, NULL);
	if (IS_ERR(devr->x1)) {
		ret = PTR_ERR(devr->x1);
		goto error3;
	}
	devr->x1->device = &dev->ib_dev;
	devr->x1->inode = NULL;
	atomic_set(&devr->x1->usecnt, 0);
	mutex_init(&devr->x1->tgt_qp_mutex);
	INIT_LIST_HEAD(&devr->x1->tgt_qp_list);

	memset(&attr, 0, sizeof(attr));
	attr.attr.max_sge = 1;
	attr.attr.max_wr = 1;
	attr.srq_type = IB_SRQT_XRC;
	attr.ext.cq = devr->c0;
	attr.ext.xrc.xrcd = devr->x0;

	devr->s0 = rdma_zalloc_drv_obj(ibdev, ib_srq);
	if (!devr->s0) {
		ret = -ENOMEM;
		goto error4;
	}

	devr->s0->device	= &dev->ib_dev;
	devr->s0->pd		= devr->p0;
	devr->s0->srq_type      = IB_SRQT_XRC;
	devr->s0->ext.xrc.xrcd	= devr->x0;
	devr->s0->ext.cq	= devr->c0;
	ret = mlx5_ib_create_srq(devr->s0, &attr, NULL);
	if (ret)
		goto err_create;

	atomic_inc(&devr->s0->ext.xrc.xrcd->usecnt);
	atomic_inc(&devr->s0->ext.cq->usecnt);
	atomic_inc(&devr->p0->usecnt);
	atomic_set(&devr->s0->usecnt, 0);

	memset(&attr, 0, sizeof(attr));
	attr.attr.max_sge = 1;
	attr.attr.max_wr = 1;
	attr.srq_type = IB_SRQT_BASIC;
	devr->s1 = rdma_zalloc_drv_obj(ibdev, ib_srq);
	if (!devr->s1) {
		ret = -ENOMEM;
		goto error5;
	}

	devr->s1->device	= &dev->ib_dev;
	devr->s1->pd		= devr->p0;
	devr->s1->srq_type      = IB_SRQT_BASIC;
	devr->s1->ext.cq	= devr->c0;

	ret = mlx5_ib_create_srq(devr->s1, &attr, NULL);
	if (ret)
		goto error6;

	atomic_inc(&devr->p0->usecnt);
	atomic_set(&devr->s1->usecnt, 0);

	for (port = 0; port < ARRAY_SIZE(devr->ports); ++port) {
		INIT_WORK(&devr->ports[port].pkey_change_work,
			  pkey_change_handler);
		devr->ports[port].devr = devr;
	}

	return 0;

error6:
	kfree(devr->s1);
error5:
	mlx5_ib_destroy_srq(devr->s0, NULL);
err_create:
	kfree(devr->s0);
error4:
	mlx5_ib_dealloc_xrcd(devr->x1, NULL);
error3:
	mlx5_ib_dealloc_xrcd(devr->x0, NULL);
error2:
	mlx5_ib_destroy_cq(devr->c0, NULL);
err_create_cq:
	kfree(devr->c0);
error1:
	mlx5_ib_dealloc_pd(devr->p0, NULL);
error0:
	kfree(devr->p0);
	return ret;
}

static void destroy_dev_resources(struct mlx5_ib_resources *devr)
{
	int port;

	mlx5_ib_destroy_srq(devr->s1, NULL);
	kfree(devr->s1);
	mlx5_ib_destroy_srq(devr->s0, NULL);
	kfree(devr->s0);
	mlx5_ib_dealloc_xrcd(devr->x0, NULL);
	mlx5_ib_dealloc_xrcd(devr->x1, NULL);
	mlx5_ib_destroy_cq(devr->c0, NULL);
	kfree(devr->c0);
	mlx5_ib_dealloc_pd(devr->p0, NULL);
	kfree(devr->p0);

	/* Make sure no change P_Key work items are still executing */
	for (port = 0; port < ARRAY_SIZE(devr->ports); ++port)
		cancel_work_sync(&devr->ports[port].pkey_change_work);
}

static u32 get_core_cap_flags(struct ib_device *ibdev)
{
	struct mlx5_ib_dev *dev = to_mdev(ibdev);
	enum rdma_link_layer ll = mlx5_ib_port_link_layer(ibdev, 1);
	u8 l3_type_cap = MLX5_CAP_ROCE(dev->mdev, l3_type);
	u8 roce_version_cap = MLX5_CAP_ROCE(dev->mdev, roce_version);
	u32 ret = 0;

	if (ll == IB_LINK_LAYER_INFINIBAND)
		return RDMA_CORE_PORT_IBA_IB;

	if (!(l3_type_cap & MLX5_ROCE_L3_TYPE_IPV4_CAP))
		return 0;

	if (!(l3_type_cap & MLX5_ROCE_L3_TYPE_IPV6_CAP))
		return 0;

	if (roce_version_cap & MLX5_ROCE_VERSION_1_CAP)
		ret |= RDMA_CORE_PORT_IBA_ROCE;

	if (roce_version_cap & MLX5_ROCE_VERSION_2_CAP)
		ret |= RDMA_CORE_PORT_IBA_ROCE_UDP_ENCAP;

	return ret;
}

static int mlx5_port_immutable(struct ib_device *ibdev, u8 port_num,
			       struct ib_port_immutable *immutable)
{
	struct ib_port_attr attr;
	struct mlx5_ib_dev *dev = to_mdev(ibdev);
	enum rdma_link_layer ll = mlx5_ib_port_link_layer(ibdev, port_num);
	int err;

	err = mlx5_ib_query_port(ibdev, port_num, &attr);
	if (err)
		return err;

	immutable->pkey_tbl_len = attr.pkey_tbl_len;
	immutable->gid_tbl_len = attr.gid_tbl_len;
	immutable->core_cap_flags = get_core_cap_flags(ibdev);
	if ((ll == IB_LINK_LAYER_INFINIBAND) || MLX5_CAP_GEN(dev->mdev, roce))
		immutable->max_mad_size = IB_MGMT_MAD_SIZE;

	return 0;
}

static void get_dev_fw_str(struct ib_device *ibdev, char *str,
			   size_t str_len)
{
	struct mlx5_ib_dev *dev =
		container_of(ibdev, struct mlx5_ib_dev, ib_dev);
	snprintf(str, str_len, "%d.%d.%04d", fw_rev_maj(dev->mdev),
		       fw_rev_min(dev->mdev), fw_rev_sub(dev->mdev));
}

static int mlx5_roce_lag_init(struct mlx5_ib_dev *dev)
{
	return 0;
}

static void mlx5_roce_lag_cleanup(struct mlx5_ib_dev *dev)
{
}

static void mlx5_remove_roce_notifier(struct mlx5_ib_dev *dev)
{
	if (dev->roce.nb.notifier_call) {
		unregister_netdevice_notifier(&dev->roce.nb);
		dev->roce.nb.notifier_call = NULL;
	}
}

static int
mlx5_enable_roce_if_cb(if_t ifp, void *arg)
{
	struct mlx5_ib_dev *dev = arg;

	/* check if network interface belongs to mlx5en */
	if (!mlx5_netdev_match(ifp, dev->mdev, "mce"))
		return (0);

	write_lock(&dev->roce.netdev_lock);
	dev->roce.netdev = ifp;
	write_unlock(&dev->roce.netdev_lock);

	return (0);
}

static int mlx5_enable_roce(struct mlx5_ib_dev *dev)
{
	struct epoch_tracker et;
	VNET_ITERATOR_DECL(vnet_iter);
	int err;

	/* Check if mlx5en net device already exists */
	VNET_LIST_RLOCK();
	NET_EPOCH_ENTER(et);
	VNET_FOREACH(vnet_iter) {
		CURVNET_SET_QUIET(vnet_iter);
		if_foreach(mlx5_enable_roce_if_cb, dev);
		CURVNET_RESTORE();
	}
	NET_EPOCH_EXIT(et);
	VNET_LIST_RUNLOCK();

	dev->roce.nb.notifier_call = mlx5_netdev_event;
	err = register_netdevice_notifier(&dev->roce.nb);
	if (err) {
		dev->roce.nb.notifier_call = NULL;
		return err;
	}

	if (MLX5_CAP_GEN(dev->mdev, roce)) {
		err = mlx5_nic_vport_enable_roce(dev->mdev);
		if (err)
			goto err_unregister_netdevice_notifier;
	}

	err = mlx5_roce_lag_init(dev);
	if (err)
		goto err_disable_roce;

	return 0;

err_disable_roce:
	if (MLX5_CAP_GEN(dev->mdev, roce))
		mlx5_nic_vport_disable_roce(dev->mdev);

err_unregister_netdevice_notifier:
	mlx5_remove_roce_notifier(dev);
	return err;
}

static void mlx5_disable_roce(struct mlx5_ib_dev *dev)
{
	mlx5_roce_lag_cleanup(dev);
	if (MLX5_CAP_GEN(dev->mdev, roce))
		mlx5_nic_vport_disable_roce(dev->mdev);
}

static void mlx5_ib_dealloc_q_port_counter(struct mlx5_ib_dev *dev, u8 port_num)
{
	mlx5_vport_dealloc_q_counter(dev->mdev,
				     MLX5_INTERFACE_PROTOCOL_IB,
				     dev->port[port_num].q_cnt_id);
	dev->port[port_num].q_cnt_id = 0;
}

static void mlx5_ib_dealloc_q_counters(struct mlx5_ib_dev *dev)
{
	unsigned int i;

	for (i = 0; i < dev->num_ports; i++)
		mlx5_ib_dealloc_q_port_counter(dev, i);
}

static int mlx5_ib_alloc_q_counters(struct mlx5_ib_dev *dev)
{
	int i;
	int ret;

	for (i = 0; i < dev->num_ports; i++) {
		ret = mlx5_vport_alloc_q_counter(dev->mdev,
						 MLX5_INTERFACE_PROTOCOL_IB,
						 &dev->port[i].q_cnt_id);
		if (ret) {
			mlx5_ib_warn(dev,
				     "couldn't allocate queue counter for port %d, err %d\n",
				     i + 1, ret);
			goto dealloc_counters;
		}
	}

	return 0;

dealloc_counters:
	while (--i >= 0)
		mlx5_ib_dealloc_q_port_counter(dev, i);

	return ret;
}

static const char * const names[] = {
	"rx_write_requests",
	"rx_read_requests",
	"rx_atomic_requests",
	"out_of_buffer",
	"out_of_sequence",
	"duplicate_request",
	"rnr_nak_retry_err",
	"packet_seq_err",
	"implied_nak_seq_err",
	"local_ack_timeout_err",
};

static const size_t stats_offsets[] = {
	MLX5_BYTE_OFF(query_q_counter_out, rx_write_requests),
	MLX5_BYTE_OFF(query_q_counter_out, rx_read_requests),
	MLX5_BYTE_OFF(query_q_counter_out, rx_atomic_requests),
	MLX5_BYTE_OFF(query_q_counter_out, out_of_buffer),
	MLX5_BYTE_OFF(query_q_counter_out, out_of_sequence),
	MLX5_BYTE_OFF(query_q_counter_out, duplicate_request),
	MLX5_BYTE_OFF(query_q_counter_out, rnr_nak_retry_err),
	MLX5_BYTE_OFF(query_q_counter_out, packet_seq_err),
	MLX5_BYTE_OFF(query_q_counter_out, implied_nak_seq_err),
	MLX5_BYTE_OFF(query_q_counter_out, local_ack_timeout_err),
};

static struct rdma_hw_stats *mlx5_ib_alloc_hw_stats(struct ib_device *ibdev,
						    u8 port_num)
{
	BUILD_BUG_ON(ARRAY_SIZE(names) != ARRAY_SIZE(stats_offsets));

	/* We support only per port stats */
	if (port_num == 0)
		return NULL;

	return rdma_alloc_hw_stats_struct(names, ARRAY_SIZE(names),
					  RDMA_HW_STATS_DEFAULT_LIFESPAN);
}

static int mlx5_ib_get_hw_stats(struct ib_device *ibdev,
				struct rdma_hw_stats *stats,
				u8 port, int index)
{
	struct mlx5_ib_dev *dev = to_mdev(ibdev);
	int outlen = MLX5_ST_SZ_BYTES(query_q_counter_out);
	void *out;
	__be32 val;
	int ret;
	int i;

	if (!port || !stats)
		return -ENOSYS;

	out = mlx5_vzalloc(outlen);
	if (!out)
		return -ENOMEM;

	ret = mlx5_vport_query_q_counter(dev->mdev,
					dev->port[port - 1].q_cnt_id, 0,
					out, outlen);
	if (ret)
		goto free;

	for (i = 0; i < ARRAY_SIZE(names); i++) {
		val = *(__be32 *)(out + stats_offsets[i]);
		stats->value[i] = (u64)be32_to_cpu(val);
	}
free:
	kvfree(out);
	return ARRAY_SIZE(names);
}

static int mlx5_ib_stage_bfreg_init(struct mlx5_ib_dev *dev)
{
	int err;

	err = mlx5_alloc_bfreg(dev->mdev, &dev->bfreg, false, false);
	if (err)
		return err;

	err = mlx5_alloc_bfreg(dev->mdev, &dev->fp_bfreg, false, true);
	if (err) {
		mlx5_free_bfreg(dev->mdev, &dev->bfreg);
		return err;
	}

	err = mlx5_alloc_bfreg(dev->mdev, &dev->wc_bfreg, true, false);
	if (err) {
		mlx5_free_bfreg(dev->mdev, &dev->fp_bfreg);
		mlx5_free_bfreg(dev->mdev, &dev->bfreg);
	}

	return err;
}

static void mlx5_ib_stage_bfreg_cleanup(struct mlx5_ib_dev *dev)
{
	mlx5_free_bfreg(dev->mdev, &dev->wc_bfreg);
	mlx5_free_bfreg(dev->mdev, &dev->fp_bfreg);
	mlx5_free_bfreg(dev->mdev, &dev->bfreg);
}

static void *mlx5_ib_add(struct mlx5_core_dev *mdev)
{
	struct mlx5_ib_dev *dev;
	enum rdma_link_layer ll;
	int port_type_cap;
	int err;
	int i;

	port_type_cap = MLX5_CAP_GEN(mdev, port_type);
	ll = mlx5_port_type_cap_to_rdma_ll(port_type_cap);

	dev = (struct mlx5_ib_dev *)ib_alloc_device(sizeof(*dev));
	if (!dev)
		return NULL;

	dev->mdev = mdev;

	dev->port = kcalloc(MLX5_CAP_GEN(mdev, num_ports), sizeof(*dev->port),
			    GFP_KERNEL);
	if (!dev->port)
		goto err_dealloc;

	rwlock_init(&dev->roce.netdev_lock);
	err = get_port_caps(dev);
	if (err)
		goto err_free_port;

	if (mlx5_use_mad_ifc(dev))
		get_ext_port_caps(dev);

	MLX5_INIT_DOORBELL_LOCK(&dev->uar_lock);

	mutex_init(&dev->lb_mutex);

	INIT_IB_DEVICE_OPS(&dev->ib_dev.ops, mlx5, MLX5);
	snprintf(dev->ib_dev.name, IB_DEVICE_NAME_MAX, "mlx5_%d", device_get_unit(mdev->pdev->dev.bsddev));
	dev->ib_dev.owner		= THIS_MODULE;
	dev->ib_dev.node_type		= RDMA_NODE_IB_CA;
	dev->ib_dev.local_dma_lkey	= 0 /* not supported for now */;
	dev->num_ports		= MLX5_CAP_GEN(mdev, num_ports);
	dev->ib_dev.phys_port_cnt     = dev->num_ports;
	dev->ib_dev.num_comp_vectors    =
		dev->mdev->priv.eq_table.num_comp_vectors;
	dev->ib_dev.dma_device	= &mdev->pdev->dev;

	dev->ib_dev.uverbs_abi_ver	= MLX5_IB_UVERBS_ABI_VERSION;
	dev->ib_dev.uverbs_cmd_mask	=
		(1ull << IB_USER_VERBS_CMD_GET_CONTEXT)		|
		(1ull << IB_USER_VERBS_CMD_QUERY_DEVICE)	|
		(1ull << IB_USER_VERBS_CMD_QUERY_PORT)		|
		(1ull << IB_USER_VERBS_CMD_ALLOC_PD)		|
		(1ull << IB_USER_VERBS_CMD_DEALLOC_PD)		|
		(1ull << IB_USER_VERBS_CMD_CREATE_AH)		|
		(1ull << IB_USER_VERBS_CMD_DESTROY_AH)		|
		(1ull << IB_USER_VERBS_CMD_REG_MR)		|
		(1ull << IB_USER_VERBS_CMD_REREG_MR)		|
		(1ull << IB_USER_VERBS_CMD_DEREG_MR)		|
		(1ull << IB_USER_VERBS_CMD_CREATE_COMP_CHANNEL)	|
		(1ull << IB_USER_VERBS_CMD_CREATE_CQ)		|
		(1ull << IB_USER_VERBS_CMD_RESIZE_CQ)		|
		(1ull << IB_USER_VERBS_CMD_DESTROY_CQ)		|
		(1ull << IB_USER_VERBS_CMD_CREATE_QP)		|
		(1ull << IB_USER_VERBS_CMD_MODIFY_QP)		|
		(1ull << IB_USER_VERBS_CMD_QUERY_QP)		|
		(1ull << IB_USER_VERBS_CMD_DESTROY_QP)		|
		(1ull << IB_USER_VERBS_CMD_ATTACH_MCAST)	|
		(1ull << IB_USER_VERBS_CMD_DETACH_MCAST)	|
		(1ull << IB_USER_VERBS_CMD_CREATE_SRQ)		|
		(1ull << IB_USER_VERBS_CMD_MODIFY_SRQ)		|
		(1ull << IB_USER_VERBS_CMD_QUERY_SRQ)		|
		(1ull << IB_USER_VERBS_CMD_DESTROY_SRQ)		|
		(1ull << IB_USER_VERBS_CMD_CREATE_XSRQ)		|
		(1ull << IB_USER_VERBS_CMD_OPEN_QP);
	dev->ib_dev.uverbs_ex_cmd_mask =
		(1ull << IB_USER_VERBS_EX_CMD_QUERY_DEVICE)	|
		(1ull << IB_USER_VERBS_EX_CMD_CREATE_CQ)	|
		(1ull << IB_USER_VERBS_EX_CMD_CREATE_QP);

	dev->ib_dev.query_device	= mlx5_ib_query_device;
	dev->ib_dev.query_port		= mlx5_ib_query_port;
	dev->ib_dev.get_link_layer	= mlx5_ib_port_link_layer;
	if (ll == IB_LINK_LAYER_ETHERNET)
		dev->ib_dev.get_netdev	= mlx5_ib_get_netdev;
	dev->ib_dev.query_gid		= mlx5_ib_query_gid;
	dev->ib_dev.add_gid		= mlx5_ib_add_gid;
	dev->ib_dev.del_gid		= mlx5_ib_del_gid;
	dev->ib_dev.query_pkey		= mlx5_ib_query_pkey;
	dev->ib_dev.modify_device	= mlx5_ib_modify_device;
	dev->ib_dev.modify_port		= mlx5_ib_modify_port;
	dev->ib_dev.alloc_ucontext	= mlx5_ib_alloc_ucontext;
	dev->ib_dev.dealloc_ucontext	= mlx5_ib_dealloc_ucontext;
	dev->ib_dev.mmap		= mlx5_ib_mmap;
	dev->ib_dev.mmap_free		= mlx5_ib_mmap_free;
	dev->ib_dev.alloc_pd		= mlx5_ib_alloc_pd;
	dev->ib_dev.dealloc_pd		= mlx5_ib_dealloc_pd;
	dev->ib_dev.create_ah		= mlx5_ib_create_ah;
	dev->ib_dev.query_ah		= mlx5_ib_query_ah;
	dev->ib_dev.destroy_ah		= mlx5_ib_destroy_ah;
	dev->ib_dev.create_srq		= mlx5_ib_create_srq;
	dev->ib_dev.modify_srq		= mlx5_ib_modify_srq;
	dev->ib_dev.query_srq		= mlx5_ib_query_srq;
	dev->ib_dev.destroy_srq		= mlx5_ib_destroy_srq;
	dev->ib_dev.post_srq_recv	= mlx5_ib_post_srq_recv;
	dev->ib_dev.create_qp		= mlx5_ib_create_qp;
	dev->ib_dev.modify_qp		= mlx5_ib_modify_qp;
	dev->ib_dev.query_qp		= mlx5_ib_query_qp;
	dev->ib_dev.destroy_qp		= mlx5_ib_destroy_qp;
	dev->ib_dev.post_send		= mlx5_ib_post_send;
	dev->ib_dev.post_recv		= mlx5_ib_post_recv;
	dev->ib_dev.create_cq		= mlx5_ib_create_cq;
	dev->ib_dev.modify_cq		= mlx5_ib_modify_cq;
	dev->ib_dev.resize_cq		= mlx5_ib_resize_cq;
	dev->ib_dev.destroy_cq		= mlx5_ib_destroy_cq;
	dev->ib_dev.poll_cq		= mlx5_ib_poll_cq;
	dev->ib_dev.req_notify_cq	= mlx5_ib_arm_cq;
	dev->ib_dev.get_dma_mr		= mlx5_ib_get_dma_mr;
	dev->ib_dev.reg_user_mr		= mlx5_ib_reg_user_mr;
	dev->ib_dev.rereg_user_mr	= mlx5_ib_rereg_user_mr;
	dev->ib_dev.dereg_mr		= mlx5_ib_dereg_mr;
	dev->ib_dev.attach_mcast	= mlx5_ib_mcg_attach;
	dev->ib_dev.detach_mcast	= mlx5_ib_mcg_detach;
	dev->ib_dev.process_mad		= mlx5_ib_process_mad;
	dev->ib_dev.alloc_mr		= mlx5_ib_alloc_mr;
	dev->ib_dev.map_mr_sg		= mlx5_ib_map_mr_sg;
	dev->ib_dev.check_mr_status	= mlx5_ib_check_mr_status;
	dev->ib_dev.get_port_immutable  = mlx5_port_immutable;
	dev->ib_dev.get_dev_fw_str      = get_dev_fw_str;
	if (mlx5_core_is_pf(mdev)) {
		dev->ib_dev.get_vf_config	= mlx5_ib_get_vf_config;
		dev->ib_dev.set_vf_link_state	= mlx5_ib_set_vf_link_state;
		dev->ib_dev.get_vf_stats	= mlx5_ib_get_vf_stats;
		dev->ib_dev.set_vf_guid		= mlx5_ib_set_vf_guid;
	}

	dev->ib_dev.disassociate_ucontext = mlx5_ib_disassociate_ucontext;

	mlx5_ib_internal_fill_odp_caps(dev);

	if (MLX5_CAP_GEN(mdev, imaicl)) {
		dev->ib_dev.alloc_mw		= mlx5_ib_alloc_mw;
		dev->ib_dev.dealloc_mw		= mlx5_ib_dealloc_mw;
		dev->ib_dev.uverbs_cmd_mask |=
			(1ull << IB_USER_VERBS_CMD_ALLOC_MW)	|
			(1ull << IB_USER_VERBS_CMD_DEALLOC_MW);
	}

	if (MLX5_CAP_GEN(dev->mdev, out_of_seq_cnt) &&
	    MLX5_CAP_GEN(dev->mdev, retransmission_q_counters)) {
		dev->ib_dev.get_hw_stats	= mlx5_ib_get_hw_stats;
		dev->ib_dev.alloc_hw_stats	= mlx5_ib_alloc_hw_stats;
	}

	if (MLX5_CAP_GEN(mdev, xrc)) {
		dev->ib_dev.alloc_xrcd = mlx5_ib_alloc_xrcd;
		dev->ib_dev.dealloc_xrcd = mlx5_ib_dealloc_xrcd;
		dev->ib_dev.uverbs_cmd_mask |=
			(1ull << IB_USER_VERBS_CMD_OPEN_XRCD) |
			(1ull << IB_USER_VERBS_CMD_CLOSE_XRCD);
	}

	if (mlx5_ib_port_link_layer(&dev->ib_dev, 1) ==
	    IB_LINK_LAYER_ETHERNET) {
		dev->ib_dev.create_flow	= mlx5_ib_create_flow;
		dev->ib_dev.destroy_flow = mlx5_ib_destroy_flow;
		dev->ib_dev.create_wq	 = mlx5_ib_create_wq;
		dev->ib_dev.modify_wq	 = mlx5_ib_modify_wq;
		dev->ib_dev.destroy_wq	 = mlx5_ib_destroy_wq;
		dev->ib_dev.create_rwq_ind_table = mlx5_ib_create_rwq_ind_table;
		dev->ib_dev.destroy_rwq_ind_table = mlx5_ib_destroy_rwq_ind_table;
		dev->ib_dev.uverbs_ex_cmd_mask |=
			(1ull << IB_USER_VERBS_EX_CMD_CREATE_FLOW) |
			(1ull << IB_USER_VERBS_EX_CMD_DESTROY_FLOW) |
			(1ull << IB_USER_VERBS_EX_CMD_CREATE_WQ) |
			(1ull << IB_USER_VERBS_EX_CMD_MODIFY_WQ) |
			(1ull << IB_USER_VERBS_EX_CMD_DESTROY_WQ) |
			(1ull << IB_USER_VERBS_EX_CMD_CREATE_RWQ_IND_TBL) |
			(1ull << IB_USER_VERBS_EX_CMD_DESTROY_RWQ_IND_TBL);
	}
	err = init_node_data(dev);
	if (err)
		goto err_free_port;

	mutex_init(&dev->flow_db.lock);
	mutex_init(&dev->cap_mask_mutex);
	INIT_LIST_HEAD(&dev->qp_list);
	spin_lock_init(&dev->reset_flow_resource_lock);

	if (ll == IB_LINK_LAYER_ETHERNET) {
		err = mlx5_enable_roce(dev);
		if (err)
			goto err_free_port;
	}

	err = create_dev_resources(&dev->devr);
	if (err)
		goto err_disable_roce;

	err = mlx5_ib_odp_init_one(dev);
	if (err)
		goto err_rsrc;

	err = mlx5_ib_alloc_q_counters(dev);
	if (err)
		goto err_odp;

	err = mlx5_ib_stage_bfreg_init(dev);
	if (err)
		goto err_q_cnt;

	err = ib_register_device(&dev->ib_dev, NULL);
	if (err)
		goto err_bfreg;

	err = create_umr_res(dev);
	if (err)
		goto err_dev;

	for (i = 0; i < ARRAY_SIZE(mlx5_class_attributes); i++) {
		err = device_create_file(&dev->ib_dev.dev,
					 mlx5_class_attributes[i]);
		if (err)
			goto err_umrc;
	}

	err = mlx5_ib_init_congestion(dev);
	if (err)
		goto err_umrc;

	dev->ib_active = true;

	return dev;

err_umrc:
	destroy_umrc_res(dev);

err_dev:
	ib_unregister_device(&dev->ib_dev);

err_bfreg:
	mlx5_ib_stage_bfreg_cleanup(dev);

err_q_cnt:
	mlx5_ib_dealloc_q_counters(dev);

err_odp:
	mlx5_ib_odp_remove_one(dev);

err_rsrc:
	destroy_dev_resources(&dev->devr);

err_disable_roce:
	if (ll == IB_LINK_LAYER_ETHERNET) {
		mlx5_disable_roce(dev);
		mlx5_remove_roce_notifier(dev);
	}

err_free_port:
	kfree(dev->port);

err_dealloc:
	ib_dealloc_device((struct ib_device *)dev);

	return NULL;
}

static void mlx5_ib_remove(struct mlx5_core_dev *mdev, void *context)
{
	struct mlx5_ib_dev *dev = context;
	enum rdma_link_layer ll = mlx5_ib_port_link_layer(&dev->ib_dev, 1);

	mlx5_ib_cleanup_congestion(dev);
	mlx5_remove_roce_notifier(dev);
	ib_unregister_device(&dev->ib_dev);
	mlx5_ib_stage_bfreg_cleanup(dev);
	mlx5_ib_dealloc_q_counters(dev);
	destroy_umrc_res(dev);
	mlx5_ib_odp_remove_one(dev);
	destroy_dev_resources(&dev->devr);
	if (ll == IB_LINK_LAYER_ETHERNET)
		mlx5_disable_roce(dev);
	kfree(dev->port);
	ib_dealloc_device(&dev->ib_dev);
}

static struct mlx5_interface mlx5_ib_interface = {
	.add            = mlx5_ib_add,
	.remove         = mlx5_ib_remove,
	.event          = mlx5_ib_event,
	.protocol	= MLX5_INTERFACE_PROTOCOL_IB,
};

static int __init mlx5_ib_init(void)
{
	int err;

	err = mlx5_ib_odp_init();
	if (err)
		return err;

	err = mlx5_register_interface(&mlx5_ib_interface);
	if (err)
		goto clean_odp;

	return err;

clean_odp:
	mlx5_ib_odp_cleanup();
	return err;
}

static void __exit mlx5_ib_cleanup(void)
{
	mlx5_unregister_interface(&mlx5_ib_interface);
	mlx5_ib_odp_cleanup();
}

module_init_order(mlx5_ib_init, SI_ORDER_SEVENTH);
module_exit_order(mlx5_ib_cleanup, SI_ORDER_SEVENTH);
