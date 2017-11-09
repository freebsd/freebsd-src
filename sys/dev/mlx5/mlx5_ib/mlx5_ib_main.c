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

#include <linux/errno.h>
#include <linux/pci.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/io-mapping.h>
#include <linux/sched.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <net/ipv6.h>
#include <linux/list.h>
#include <dev/mlx5/driver.h>
#include <dev/mlx5/vport.h>
#include <asm/pgtable.h>
#include <linux/fs.h>
#undef inode

#include <rdma/ib_user_verbs.h>
#include <rdma/ib_smi.h>
#include <rdma/ib_umem.h>
#include "user.h"
#include "mlx5_ib.h"

#include <sys/unistd.h>

#define DRIVER_NAME "mlx5_ib"
#define DRIVER_VERSION "3.2-rc1"
#define DRIVER_RELDATE	"May 2016"

#undef MODULE_VERSION
#include <sys/module.h>

MODULE_AUTHOR("Eli Cohen <eli@mellanox.com>");
MODULE_DESCRIPTION("Mellanox Connect-IB HCA IB driver");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DEPEND(mlx5ib, mlx5, 1, 1, 1);
MODULE_DEPEND(mlx5ib, ibcore, 1, 1, 1);
MODULE_VERSION(mlx5ib, 1);

static int deprecated_prof_sel = 2;
module_param_named(prof_sel, deprecated_prof_sel, int, 0444);
MODULE_PARM_DESC(prof_sel, "profile selector. Deprecated here. Moved to module mlx5_core");

enum {
	MLX5_STANDARD_ATOMIC_SIZE = 0x8,
};

struct workqueue_struct *mlx5_ib_wq;

static char mlx5_version[] =
	DRIVER_NAME ": Mellanox Connect-IB Infiniband driver v"
	DRIVER_VERSION " (" DRIVER_RELDATE ")\n";

static void get_atomic_caps(struct mlx5_ib_dev *dev,
			    struct ib_device_attr *props)
{
	int tmp;
	u8 atomic_operations;
	u8 atomic_size_qp;
	u8 atomic_req_endianess;

	atomic_operations = MLX5_CAP_ATOMIC(dev->mdev, atomic_operations);
	atomic_size_qp = MLX5_CAP_ATOMIC(dev->mdev, atomic_size_qp);
	atomic_req_endianess = MLX5_CAP_ATOMIC(dev->mdev,
					       atomic_req_8B_endianess_mode) ||
			       !mlx5_host_is_le();

	tmp = MLX5_ATOMIC_OPS_CMP_SWAP | MLX5_ATOMIC_OPS_FETCH_ADD;
	if (((atomic_operations & tmp) == tmp)
	    && (atomic_size_qp & 8)) {
		if (atomic_req_endianess) {
			props->atomic_cap = IB_ATOMIC_HCA;
		} else {
			props->atomic_cap = IB_ATOMIC_NONE;
		}
	} else {
		props->atomic_cap = IB_ATOMIC_NONE;
	}

	tmp = MLX5_ATOMIC_OPS_MASKED_CMP_SWAP | MLX5_ATOMIC_OPS_MASKED_FETCH_ADD;
	if (((atomic_operations & tmp) == tmp)
	    &&(atomic_size_qp & 8)) {
		if (atomic_req_endianess)
			props->masked_atomic_cap = IB_ATOMIC_HCA;
		else {
			props->masked_atomic_cap = IB_ATOMIC_NONE;
		}
	} else {
		props->masked_atomic_cap = IB_ATOMIC_NONE;
	}
}

static enum rdma_link_layer
mlx5_ib_port_link_layer(struct ib_device *device, u8 port_num)
{
	struct mlx5_ib_dev *dev = to_mdev(device);

	switch (MLX5_CAP_GEN(dev->mdev, port_type)) {
	case MLX5_CAP_PORT_TYPE_IB:
		return IB_LINK_LAYER_INFINIBAND;
	case MLX5_CAP_PORT_TYPE_ETH:
		return IB_LINK_LAYER_ETHERNET;
	default:
		return IB_LINK_LAYER_UNSPECIFIED;
	}
}

static int mlx5_use_mad_ifc(struct mlx5_ib_dev *dev)
{
	return !dev->mdev->issi;
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

static int mlx5_query_system_image_guid(struct ib_device *ibdev,
					__be64 *sys_image_guid)
{
	struct mlx5_ib_dev *dev = to_mdev(ibdev);
	struct mlx5_core_dev *mdev = dev->mdev;
	u64 tmp;
	int err;

	switch (mlx5_get_vport_access_method(ibdev)) {
	case MLX5_VPORT_ACCESS_METHOD_MAD:
		return mlx5_query_system_image_guid_mad_ifc(ibdev,
							    sys_image_guid);

	case MLX5_VPORT_ACCESS_METHOD_HCA:
		err = mlx5_query_hca_vport_system_image_guid(mdev, &tmp);
		if (!err)
			*sys_image_guid = cpu_to_be64(tmp);
		return err;

	case MLX5_VPORT_ACCESS_METHOD_NIC:
		err = mlx5_query_nic_vport_system_image_guid(mdev, &tmp);
		if (!err)
			*sys_image_guid = cpu_to_be64(tmp);
		return err;

	default:
		return -EINVAL;
	}
}

static int mlx5_query_max_pkeys(struct ib_device *ibdev,
				u16 *max_pkeys)
{
	struct mlx5_ib_dev *dev = to_mdev(ibdev);
	struct mlx5_core_dev *mdev = dev->mdev;

	switch (mlx5_get_vport_access_method(ibdev)) {
	case MLX5_VPORT_ACCESS_METHOD_MAD:
		return mlx5_query_max_pkeys_mad_ifc(ibdev, max_pkeys);

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
		return mlx5_query_vendor_id_mad_ifc(ibdev, vendor_id);

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
		return mlx5_query_node_guid_mad_ifc(dev, node_guid);

	case MLX5_VPORT_ACCESS_METHOD_HCA:
		err = mlx5_query_hca_vport_node_guid(dev->mdev, &tmp);
		if (!err)
			*node_guid = cpu_to_be64(tmp);
		return err;

	case MLX5_VPORT_ACCESS_METHOD_NIC:
		err = mlx5_query_nic_vport_node_guid(dev->mdev, &tmp);
		if (!err)
			*node_guid = cpu_to_be64(tmp);
		return err;

	default:
		return -EINVAL;
	}
}

struct mlx5_reg_node_desc {
	u8	desc[64];
};

static int mlx5_query_node_desc(struct mlx5_ib_dev *dev, char *node_desc)
{
	struct mlx5_reg_node_desc in;

	if (mlx5_use_mad_ifc(dev))
		return mlx5_query_node_desc_mad_ifc(dev, node_desc);

	memset(&in, 0, sizeof(in));

	return mlx5_core_access_reg(dev->mdev, &in, sizeof(in), node_desc,
				    sizeof(struct mlx5_reg_node_desc),
				    MLX5_REG_NODE_DESC, 0, 0);
}

static int mlx5_ib_query_device(struct ib_device *ibdev,
				struct ib_device_attr *props)
{
	struct mlx5_ib_dev *dev = to_mdev(ibdev);
	struct mlx5_core_dev *mdev = dev->mdev;
	int max_sq_desc;
	int max_rq_sg;
	int max_sq_sg;
	int err;


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
		((u64)fw_rev_min(dev->mdev) << 16) |
		fw_rev_sub(dev->mdev);
	props->device_cap_flags    = IB_DEVICE_CHANGE_PHY_PORT |
		IB_DEVICE_PORT_ACTIVE_EVENT		|
		IB_DEVICE_SYS_IMAGE_GUID		|
		IB_DEVICE_RC_RNR_NAK_GEN;

	if (MLX5_CAP_GEN(mdev, pkv))
		props->device_cap_flags |= IB_DEVICE_BAD_PKEY_CNTR;
	if (MLX5_CAP_GEN(mdev, qkv))
		props->device_cap_flags |= IB_DEVICE_BAD_QKEY_CNTR;
	if (MLX5_CAP_GEN(mdev, apm))
		props->device_cap_flags |= IB_DEVICE_AUTO_PATH_MIG;
	props->device_cap_flags |= IB_DEVICE_LOCAL_DMA_LKEY;
	if (MLX5_CAP_GEN(mdev, xrc))
		props->device_cap_flags |= IB_DEVICE_XRC;
	props->device_cap_flags |= IB_DEVICE_MEM_MGT_EXTENSIONS;
	if (MLX5_CAP_GEN(mdev, block_lb_mc))
		props->device_cap_flags |= IB_DEVICE_BLOCK_MULTICAST_LOOPBACK;

	props->vendor_part_id	   = mdev->pdev->device;
	props->hw_ver		   = mdev->pdev->revision;

	props->max_mr_size	   = ~0ull;
	props->page_size_cap	   = ~(u32)((1ull << MLX5_CAP_GEN(mdev, log_pg_sz)) -1);
	props->max_qp		   = 1 << MLX5_CAP_GEN(mdev, log_max_qp);
	props->max_qp_wr	   = 1 << MLX5_CAP_GEN(mdev, log_max_qp_sz);
	max_rq_sg =  MLX5_CAP_GEN(mdev, max_wqe_sz_rq) /
		     sizeof(struct mlx5_wqe_data_seg);
	max_sq_desc = min((int)MLX5_CAP_GEN(mdev, max_wqe_sz_sq), 512);
	max_sq_sg = (max_sq_desc -
		     sizeof(struct mlx5_wqe_ctrl_seg) -
		     sizeof(struct mlx5_wqe_raddr_seg)) / sizeof(struct mlx5_wqe_data_seg);
	props->max_sge = min(max_rq_sg, max_sq_sg);
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
	props->max_fast_reg_page_list_len = (unsigned int)-1;
	get_atomic_caps(dev, props);
	props->max_mcast_grp	   = 1 << MLX5_CAP_GEN(mdev, log_max_mcg);
	props->max_mcast_qp_attach = MLX5_CAP_GEN(mdev, max_qp_mcg);
	props->max_total_mcast_qp_attach = props->max_mcast_qp_attach *
					   props->max_mcast_grp;
	props->max_map_per_fmr = INT_MAX; /* no limit in ConnectIB */
	props->max_ah		= INT_MAX;

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
		mlx5_ib_warn(dev, "active_width %d is not supported by IB spec\n",
			     (int)active_width);
		err = -EINVAL;
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

/*
 * TODO: Move to IB core
 */
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

static int mlx5_query_port_ib(struct ib_device *ibdev, u8 port,
			      struct ib_port_attr *props)
{
	struct mlx5_ib_dev *dev = to_mdev(ibdev);
	struct mlx5_core_dev *mdev = dev->mdev;
	u32 *rep;
	int outlen = MLX5_ST_SZ_BYTES(query_hca_vport_context_out);
	struct mlx5_ptys_reg *ptys;
	struct mlx5_pmtu_reg *pmtu;
	struct mlx5_pvlc_reg pvlc;
	void *ctx;
	int err;

	rep = mlx5_vzalloc(outlen);
	ptys = kzalloc(sizeof(*ptys), GFP_KERNEL);
	pmtu = kzalloc(sizeof(*pmtu), GFP_KERNEL);
	if (!rep || !ptys || !pmtu) {
		err = -ENOMEM;
		goto out;
	}

	memset(props, 0, sizeof(*props));

	/* what if I am pf with dual port */
	err = mlx5_query_hca_vport_context(mdev, port, 0, rep, outlen);
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
		return mlx5_query_port_mad_ifc(ibdev, port, props);

	case MLX5_VPORT_ACCESS_METHOD_HCA:
		return mlx5_query_port_ib(ibdev, port, props);

	case MLX5_VPORT_ACCESS_METHOD_NIC:
		return mlx5_query_port_roce(ibdev, port, props);

	default:
		return -EINVAL;
	}
}

static void
mlx5_addrconf_ifid_eui48(u8 *eui, u16 vlan_id, struct net_device *dev)
{
	if (dev->if_addrlen != ETH_ALEN)
		return;

	memcpy(eui, IF_LLADDR(dev), 3);
	memcpy(eui + 5, IF_LLADDR(dev) + 3, 3);

	if (vlan_id < 0x1000) {
		eui[3] = vlan_id >> 8;
		eui[4] = vlan_id & 0xff;
	} else {
		eui[3] = 0xFF;
		eui[4] = 0xFE;
	}
	eui[0] ^= 2;
}

static void
mlx5_make_default_gid(struct net_device *dev, union ib_gid *gid)
{
	gid->global.subnet_prefix = cpu_to_be64(0xfe80000000000000LL);
	mlx5_addrconf_ifid_eui48(&gid->raw[8], 0xFFFF, dev);
}

static void
mlx5_ib_roce_port_update(void *arg)
{
	struct mlx5_ib_port *port = (struct mlx5_ib_port *)arg;
	struct mlx5_ib_dev *dev = port->dev;
	struct mlx5_core_dev *mdev = dev->mdev;
	struct net_device *xdev[MLX5_IB_GID_MAX];
	struct net_device *idev;
	struct net_device *ndev;
	union ib_gid gid_temp;

	while (port->port_gone == 0) {
		int update = 0;
		int gid_index = 0;
		int j;
		int error;

		ndev = mlx5_get_protocol_dev(mdev, MLX5_INTERFACE_PROTOCOL_ETH);
		if (ndev == NULL) {
			pause("W", hz);
			continue;
		}

		CURVNET_SET_QUIET(ndev->if_vnet);

		memset(&gid_temp, 0, sizeof(gid_temp));
		mlx5_make_default_gid(ndev, &gid_temp);
		if (bcmp(&gid_temp, &port->gid_table[gid_index], sizeof(gid_temp))) {
			port->gid_table[gid_index] = gid_temp;
			update = 1;
		}
		xdev[gid_index] = ndev;
		gid_index++;

		IFNET_RLOCK();
		TAILQ_FOREACH(idev, &V_ifnet, if_link) {
			if (idev == ndev)
				break;
		}
		if (idev != NULL) {
		    TAILQ_FOREACH(idev, &V_ifnet, if_link) {
			u16 vid;

			if (idev != ndev) {
				if (idev->if_type != IFT_L2VLAN)
					continue;
				if (ndev != rdma_vlan_dev_real_dev(idev))
					continue;
			}

			/* setup valid MAC-based GID */
			memset(&gid_temp, 0, sizeof(gid_temp));
			gid_temp.global.subnet_prefix = cpu_to_be64(0xfe80000000000000LL);
			vid = rdma_vlan_dev_vlan_id(idev);
			mlx5_addrconf_ifid_eui48(&gid_temp.raw[8], vid, idev);

			/* check for existing entry */
			for (j = 0; j != gid_index; j++) {
				if (bcmp(&gid_temp, &port->gid_table[j], sizeof(gid_temp)) == 0)
					break;
			}

			/* check if new entry should be added */
			if (j == gid_index && gid_index < MLX5_IB_GID_MAX) {
				if (bcmp(&gid_temp, &port->gid_table[gid_index], sizeof(gid_temp))) {
					port->gid_table[gid_index] = gid_temp;
					update = 1;
				}
				xdev[gid_index] = idev;
				gid_index++;
			}
		    }
		}
		IFNET_RUNLOCK();
		CURVNET_RESTORE();

		if (update != 0 &&
		    mlx5_ib_port_link_layer(&dev->ib_dev, 1) == IB_LINK_LAYER_ETHERNET) {
			struct ib_event event = {
			    .device = &dev->ib_dev,
			    .element.port_num = port->port_num + 1,
			    .event = IB_EVENT_GID_CHANGE,
			};

			/* add new entries, if any */
			for (j = 0; j != gid_index; j++) {
				error = modify_gid_roce(&dev->ib_dev, port->port_num, j,
				    port->gid_table + j, xdev[j]);
				if (error != 0)
					printf("mlx5_ib: Failed to update ROCE GID table: %d\n", error);
			}
			memset(&gid_temp, 0, sizeof(gid_temp));

			/* clear old entries, if any */
			for (; j != MLX5_IB_GID_MAX; j++) {
				if (bcmp(&gid_temp, port->gid_table + j, sizeof(gid_temp)) == 0)
					continue;
				port->gid_table[j] = gid_temp;
				(void) modify_gid_roce(&dev->ib_dev, port->port_num, j,
				    port->gid_table + j, ndev);
			}

			/* make sure ibcore gets updated */
			ib_dispatch_event(&event);
		}
		pause("W", hz);
	}
	do {
		struct ib_event event = {
			.device = &dev->ib_dev,
			.element.port_num = port->port_num + 1,
			.event = IB_EVENT_GID_CHANGE,
		};
		/* make sure ibcore gets updated */
		ib_dispatch_event(&event);

		/* wait a bit */
		pause("W", hz);
	} while (0);
	port->port_gone = 2;
	kthread_exit();
}

static int mlx5_ib_query_gid(struct ib_device *ibdev, u8 port, int index,
			     union ib_gid *gid)
{
	struct mlx5_ib_dev *dev = to_mdev(ibdev);
	struct mlx5_core_dev *mdev = dev->mdev;

	switch (mlx5_get_vport_access_method(ibdev)) {
	case MLX5_VPORT_ACCESS_METHOD_MAD:
		return mlx5_query_gids_mad_ifc(ibdev, port, index, gid);

	case MLX5_VPORT_ACCESS_METHOD_HCA:
		return mlx5_query_hca_vport_gid(mdev, port, 0, index, gid);

	case MLX5_VPORT_ACCESS_METHOD_NIC:
		if (port == 0 || port > MLX5_CAP_GEN(mdev, num_ports) ||
		    index < 0 || index >= MLX5_IB_GID_MAX ||
		    dev->port[port - 1].port_gone != 0)
			memset(gid, 0, sizeof(*gid));
		else
			*gid = dev->port[port - 1].gid_table[index];
		return 0;

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
		return mlx5_query_pkey_mad_ifc(ibdev, port, index, pkey);

	case MLX5_VPORT_ACCESS_METHOD_HCA:
	case MLX5_VPORT_ACCESS_METHOD_NIC:
		return mlx5_query_hca_vport_pkey(mdev, 0, port, 0, index,
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
	memcpy(&in, props->node_desc, 64);
	err = mlx5_core_access_reg(dev->mdev, &in, sizeof(in), &out,
				   sizeof(out), MLX5_REG_NODE_DESC, 0, 1);
	if (err)
		return err;

	memcpy(ibdev->node_desc, props->node_desc, 64);

	return err;
}

static int mlx5_ib_modify_port(struct ib_device *ibdev, u8 port, int mask,
			       struct ib_port_modify *props)
{
	u8 is_eth = (mlx5_ib_port_link_layer(ibdev, port) ==
		     IB_LINK_LAYER_ETHERNET);
	struct mlx5_ib_dev *dev = to_mdev(ibdev);
	struct ib_port_attr attr;
	u32 tmp;
	int err;

	/* return OK if this is RoCE. CM calls ib_modify_port() regardless
	 * of whether port link layer is ETH or IB. For ETH ports, qkey
	 * violations and port capabilities are not valid.
	 */
	if (is_eth)
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

enum mlx5_cap_flags {
	MLX5_CAP_COMPACT_AV = 1 << 0,
};

static void set_mlx5_flags(u32 *flags, struct mlx5_core_dev *dev)
{
	*flags |= MLX5_CAP_GEN(dev, compact_address_vector) ?
		  MLX5_CAP_COMPACT_AV : 0;
}

static struct ib_ucontext *mlx5_ib_alloc_ucontext(struct ib_device *ibdev,
						  struct ib_udata *udata)
{
	struct mlx5_ib_dev *dev = to_mdev(ibdev);
	struct mlx5_ib_alloc_ucontext_req_v2 req;
	struct mlx5_ib_alloc_ucontext_resp resp;
	struct mlx5_ib_ucontext *context;
	struct mlx5_uuar_info *uuari;
	struct mlx5_uar *uars;
	int gross_uuars;
	int num_uars;
	int ver;
	int uuarn;
	int err;
	int i;
	size_t reqlen;

	if (!dev->ib_active)
		return ERR_PTR(-EAGAIN);

	memset(&req, 0, sizeof(req));
	memset(&resp, 0, sizeof(resp));

	reqlen = udata->inlen - sizeof(struct ib_uverbs_cmd_hdr);
	if (reqlen == sizeof(struct mlx5_ib_alloc_ucontext_req))
		ver = 0;
	else if (reqlen == sizeof(struct mlx5_ib_alloc_ucontext_req_v2))
		ver = 2;
	else {
		mlx5_ib_err(dev, "request malformed, reqlen: %ld\n", (long)reqlen);
		return ERR_PTR(-EINVAL);
	}

	err = ib_copy_from_udata(&req, udata, reqlen);
	if (err) {
		mlx5_ib_err(dev, "copy failed\n");
		return ERR_PTR(err);
	}

	if (req.reserved) {
		mlx5_ib_err(dev, "request corrupted\n");
		return ERR_PTR(-EINVAL);
	}

	if (req.total_num_uuars == 0 || req.total_num_uuars > MLX5_MAX_UUARS) {
		mlx5_ib_warn(dev, "wrong num_uuars: %d\n", req.total_num_uuars);
		return ERR_PTR(-ENOMEM);
	}

	req.total_num_uuars = ALIGN(req.total_num_uuars,
				    MLX5_NON_FP_BF_REGS_PER_PAGE);
	if (req.num_low_latency_uuars > req.total_num_uuars - 1) {
		mlx5_ib_warn(dev, "wrong num_low_latency_uuars: %d ( > %d)\n",
			     req.total_num_uuars, req.total_num_uuars);
		return ERR_PTR(-EINVAL);
	}

	num_uars = req.total_num_uuars / MLX5_NON_FP_BF_REGS_PER_PAGE;
	gross_uuars = num_uars * MLX5_BF_REGS_PER_PAGE;
	resp.qp_tab_size = 1 << MLX5_CAP_GEN(dev->mdev, log_max_qp);
	if (mlx5_core_is_pf(dev->mdev) && MLX5_CAP_GEN(dev->mdev, bf))
		resp.bf_reg_size = 1 << MLX5_CAP_GEN(dev->mdev, log_bf_reg_size);
	resp.cache_line_size = L1_CACHE_BYTES;
	resp.max_sq_desc_sz = MLX5_CAP_GEN(dev->mdev, max_wqe_sz_sq);
	resp.max_rq_desc_sz = MLX5_CAP_GEN(dev->mdev, max_wqe_sz_rq);
	resp.max_send_wqebb = 1 << MLX5_CAP_GEN(dev->mdev, log_max_qp_sz);
	resp.max_recv_wr = 1 << MLX5_CAP_GEN(dev->mdev, log_max_qp_sz);
	resp.max_srq_recv_wr = 1 << MLX5_CAP_GEN(dev->mdev, log_max_srq_sz);
	set_mlx5_flags(&resp.flags, dev->mdev);

	if (offsetof(struct mlx5_ib_alloc_ucontext_resp, max_desc_sz_sq_dc) < udata->outlen)
		resp.max_desc_sz_sq_dc = MLX5_CAP_GEN(dev->mdev, max_wqe_sz_sq_dc);

	if (offsetof(struct mlx5_ib_alloc_ucontext_resp, atomic_arg_sizes_dc) < udata->outlen)
		resp.atomic_arg_sizes_dc = MLX5_CAP_ATOMIC(dev->mdev, atomic_size_dc);

	context = kzalloc(sizeof(*context), GFP_KERNEL);
	if (!context)
		return ERR_PTR(-ENOMEM);

	uuari = &context->uuari;
	mutex_init(&uuari->lock);
	uars = kcalloc(num_uars, sizeof(*uars), GFP_KERNEL);
	if (!uars) {
		err = -ENOMEM;
		goto out_ctx;
	}

	uuari->bitmap = kcalloc(BITS_TO_LONGS(gross_uuars),
				sizeof(*uuari->bitmap),
				GFP_KERNEL);
	if (!uuari->bitmap) {
		err = -ENOMEM;
		goto out_uar_ctx;
	}
	/*
	 * clear all fast path uuars
	 */
	for (i = 0; i < gross_uuars; i++) {
		uuarn = i & 3;
		if (uuarn == 2 || uuarn == 3)
			set_bit(i, uuari->bitmap);
	}

	uuari->count = kcalloc(gross_uuars, sizeof(*uuari->count), GFP_KERNEL);
	if (!uuari->count) {
		err = -ENOMEM;
		goto out_bitmap;
	}

	for (i = 0; i < num_uars; i++) {
		err = mlx5_cmd_alloc_uar(dev->mdev, &uars[i].index);
		if (err) {
			mlx5_ib_err(dev, "uar alloc failed at %d\n", i);
			goto out_uars;
		}
	}
	for (i = 0; i < MLX5_IB_MAX_CTX_DYNAMIC_UARS; i++)
		context->dynamic_wc_uar_index[i] = MLX5_IB_INVALID_UAR_INDEX;

	INIT_LIST_HEAD(&context->db_page_list);
	mutex_init(&context->db_page_mutex);

	resp.tot_uuars = req.total_num_uuars;
	resp.num_ports = MLX5_CAP_GEN(dev->mdev, num_ports);
	err = ib_copy_to_udata(udata, &resp,
			       min_t(size_t, udata->outlen, sizeof(resp)));
	if (err)
		goto out_uars;

	uuari->ver = ver;
	uuari->num_low_latency_uuars = req.num_low_latency_uuars;
	uuari->uars = uars;
	uuari->num_uars = num_uars;

	if (mlx5_ib_port_link_layer(&dev->ib_dev, 1) ==
	    IB_LINK_LAYER_ETHERNET) {
		err = mlx5_alloc_transport_domain(dev->mdev, &context->tdn);
		if (err)
			goto out_uars;
	}

	return &context->ibucontext;

out_uars:
	for (i--; i >= 0; i--)
		mlx5_cmd_free_uar(dev->mdev, uars[i].index);
	kfree(uuari->count);

out_bitmap:
	kfree(uuari->bitmap);

out_uar_ctx:
	kfree(uars);

out_ctx:
	kfree(context);
	return ERR_PTR(err);
}

static int mlx5_ib_dealloc_ucontext(struct ib_ucontext *ibcontext)
{
	struct mlx5_ib_ucontext *context = to_mucontext(ibcontext);
	struct mlx5_ib_dev *dev = to_mdev(ibcontext->device);
	struct mlx5_uuar_info *uuari = &context->uuari;
	int i;

	if (mlx5_ib_port_link_layer(&dev->ib_dev, 1) ==
	    IB_LINK_LAYER_ETHERNET)
		mlx5_dealloc_transport_domain(dev->mdev, context->tdn);

	for (i = 0; i < uuari->num_uars; i++) {
		if (mlx5_cmd_free_uar(dev->mdev, uuari->uars[i].index))
			mlx5_ib_warn(dev, "failed to free UAR 0x%x\n", uuari->uars[i].index);
	}
	for (i = 0; i < MLX5_IB_MAX_CTX_DYNAMIC_UARS; i++) {
		if (context->dynamic_wc_uar_index[i] != MLX5_IB_INVALID_UAR_INDEX)
			mlx5_cmd_free_uar(dev->mdev, context->dynamic_wc_uar_index[i]);
	}

	kfree(uuari->count);
	kfree(uuari->bitmap);
	kfree(uuari->uars);
	kfree(context);

	return 0;
}

static phys_addr_t uar_index2pfn(struct mlx5_ib_dev *dev, int index)
{
	return (pci_resource_start(dev->mdev->pdev, 0) >> PAGE_SHIFT) + index;
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

static int uar_mmap(struct vm_area_struct *vma, pgprot_t prot, bool is_wc,
		    struct mlx5_uuar_info *uuari, struct mlx5_ib_dev *dev,
		    struct mlx5_ib_ucontext *context)
{
	unsigned long idx;
	phys_addr_t pfn;

	if (vma->vm_end - vma->vm_start != PAGE_SIZE) {
		mlx5_ib_warn(dev, "wrong size, expected PAGE_SIZE(%ld) got %ld\n",
			     (long)PAGE_SIZE, (long)(vma->vm_end - vma->vm_start));
		return -EINVAL;
	}

	idx = get_index(vma->vm_pgoff);
	if (idx >= uuari->num_uars) {
		mlx5_ib_warn(dev, "wrong offset, idx:%ld num_uars:%d\n",
			     idx, uuari->num_uars);
		return -EINVAL;
	}

	pfn = uar_index2pfn(dev, uuari->uars[idx].index);
	mlx5_ib_dbg(dev, "uar idx 0x%lx, pfn 0x%llx\n", idx,
		    (unsigned long long)pfn);

	vma->vm_page_prot = prot;
	if (io_remap_pfn_range(vma, vma->vm_start, pfn,
			       PAGE_SIZE, vma->vm_page_prot)) {
		mlx5_ib_err(dev, "io remap failed\n");
		return -EAGAIN;
	}

	mlx5_ib_dbg(dev, "mapped %s at 0x%lx, PA 0x%llx\n", is_wc ? "WC" : "NC",
		    (long)vma->vm_start, (unsigned long long)pfn << PAGE_SHIFT);

	return 0;
}

static int mlx5_ib_mmap(struct ib_ucontext *ibcontext, struct vm_area_struct *vma)
{
	struct mlx5_ib_ucontext *context = to_mucontext(ibcontext);
	struct mlx5_ib_dev *dev = to_mdev(ibcontext->device);
	struct mlx5_uuar_info *uuari = &context->uuari;
	unsigned long command;

	command = get_command(vma->vm_pgoff);
	switch (command) {
	case MLX5_IB_MMAP_REGULAR_PAGE:
		return uar_mmap(vma, pgprot_writecombine(vma->vm_page_prot),
				true,
				uuari, dev, context);

		break;

	case MLX5_IB_MMAP_WC_PAGE:
		return uar_mmap(vma, pgprot_writecombine(vma->vm_page_prot),
				true, uuari, dev, context);
		break;

	case MLX5_IB_MMAP_NC_PAGE:
		return uar_mmap(vma, pgprot_noncached(vma->vm_page_prot),
				false, uuari, dev, context);
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static int alloc_pa_mkey(struct mlx5_ib_dev *dev, u32 *key, u32 pdn)
{
	struct mlx5_create_mkey_mbox_in *in;
	struct mlx5_mkey_seg *seg;
	struct mlx5_core_mr mr;
	int err;

	in = kzalloc(sizeof(*in), GFP_KERNEL);
	if (!in)
		return -ENOMEM;

	seg = &in->seg;
	seg->flags = MLX5_PERM_LOCAL_READ | MLX5_ACCESS_MODE_PA;
	seg->flags_pd = cpu_to_be32(pdn | MLX5_MKEY_LEN64);
	seg->qpn_mkey7_0 = cpu_to_be32(0xffffff << 8);
	seg->start_addr = 0;

	err = mlx5_core_create_mkey(dev->mdev, &mr, in, sizeof(*in),
				    NULL, NULL, NULL);
	if (err) {
		mlx5_ib_warn(dev, "failed to create mkey, %d\n", err);
		goto err_in;
	}

	kfree(in);
	*key = mr.key;

	return 0;

err_in:
	kfree(in);

	return err;
}

static void free_pa_mkey(struct mlx5_ib_dev *dev, u32 key)
{
	struct mlx5_core_mr mr;
	int err;

	memset(&mr, 0, sizeof(mr));
	mr.key = key;
	err = mlx5_core_destroy_mkey(dev->mdev, &mr);
	if (err)
		mlx5_ib_warn(dev, "failed to destroy mkey 0x%x\n", key);
}

static struct ib_pd *mlx5_ib_alloc_pd(struct ib_device *ibdev,
				      struct ib_ucontext *context,
				      struct ib_udata *udata)
{
	struct mlx5_ib_dev *dev = to_mdev(ibdev);
	struct mlx5_ib_alloc_pd_resp resp;
	struct mlx5_ib_pd *pd;
	int err;

	pd = kmalloc(sizeof(*pd), GFP_KERNEL);
	if (!pd)
		return ERR_PTR(-ENOMEM);

	err = mlx5_core_alloc_pd(to_mdev(ibdev)->mdev, &pd->pdn);
	if (err) {
		mlx5_ib_warn(dev, "pd alloc failed\n");
		kfree(pd);
		return ERR_PTR(err);
	}

	if (context) {
		resp.pdn = pd->pdn;
		if (ib_copy_to_udata(udata, &resp, sizeof(resp))) {
			mlx5_ib_err(dev, "copy failed\n");
			mlx5_core_dealloc_pd(to_mdev(ibdev)->mdev, pd->pdn);
			kfree(pd);
			return ERR_PTR(-EFAULT);
		}
	} else {
		err = alloc_pa_mkey(to_mdev(ibdev), &pd->pa_lkey, pd->pdn);
		if (err) {
			mlx5_ib_err(dev, "alloc mkey failed\n");
			mlx5_core_dealloc_pd(to_mdev(ibdev)->mdev, pd->pdn);
			kfree(pd);
			return ERR_PTR(err);
		}
	}

	return &pd->ibpd;
}

static int mlx5_ib_dealloc_pd(struct ib_pd *pd)
{
	struct mlx5_ib_dev *mdev = to_mdev(pd->device);
	struct mlx5_ib_pd *mpd = to_mpd(pd);

	if (!pd->uobject)
		free_pa_mkey(mdev, mpd->pa_lkey);

	mlx5_core_dealloc_pd(mdev->mdev, mpd->pdn);
	kfree(mpd);

	return 0;
}

static int mlx5_ib_mcg_attach(struct ib_qp *ibqp, union ib_gid *gid, u16 lid)
{
	struct mlx5_ib_dev *dev = to_mdev(ibqp->device);
	int err;

	if (ibqp->qp_type == IB_QPT_RAW_PACKET)
		err = -EOPNOTSUPP;
	else
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

	if (ibqp->qp_type == IB_QPT_RAW_PACKET)
		err = -EOPNOTSUPP;
	else
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

static ssize_t show_fw_ver(struct device *device, struct device_attribute *attr,
			   char *buf)
{
	struct mlx5_ib_dev *dev =
		container_of(device, struct mlx5_ib_dev, ib_dev.dev);
	return sprintf(buf, "%d.%d.%04d\n", fw_rev_maj(dev->mdev),
		       fw_rev_min(dev->mdev), fw_rev_sub(dev->mdev));
}

static ssize_t show_rev(struct device *device, struct device_attribute *attr,
			char *buf)
{
	struct mlx5_ib_dev *dev =
		container_of(device, struct mlx5_ib_dev, ib_dev.dev);
	return sprintf(buf, "%x\n", (unsigned)dev->mdev->pdev->revision);
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
static DEVICE_ATTR(fw_ver,   S_IRUGO, show_fw_ver, NULL);
static DEVICE_ATTR(hca_type, S_IRUGO, show_hca,    NULL);
static DEVICE_ATTR(board_id, S_IRUGO, show_board,  NULL);
static DEVICE_ATTR(fw_pages, S_IRUGO, show_fw_pages, NULL);
static DEVICE_ATTR(reg_pages, S_IRUGO, show_reg_pages, NULL);

static struct device_attribute *mlx5_class_attributes[] = {
	&dev_attr_hw_rev,
	&dev_attr_fw_ver,
	&dev_attr_hca_type,
	&dev_attr_board_id,
	&dev_attr_fw_pages,
	&dev_attr_reg_pages,
};

static void mlx5_ib_handle_internal_error(struct mlx5_ib_dev *ibdev)
{
	struct mlx5_ib_qp *mqp;
	struct mlx5_ib_cq *send_mcq, *recv_mcq;
	struct mlx5_core_cq *mcq;
	struct list_head cq_armed_list;
	unsigned long flags_qp;
	unsigned long flags_cq;
	unsigned long flags;

	mlx5_ib_warn(ibdev, " started\n");
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
		mcq->comp(mcq);
	}
	spin_unlock_irqrestore(&ibdev->reset_flow_resource_lock, flags);
	mlx5_ib_warn(ibdev, " ended\n");
	return;
}

static void mlx5_ib_event(struct mlx5_core_dev *dev, void *context,
			  enum mlx5_dev_event event, unsigned long param)
{
	struct mlx5_ib_dev *ibdev = (struct mlx5_ib_dev *)context;
	struct ib_event ibev;

	u8 port = 0;

	switch (event) {
	case MLX5_DEV_EVENT_SYS_ERROR:
		ibdev->ib_active = false;
		ibev.event = IB_EVENT_DEVICE_FATAL;
		mlx5_ib_handle_internal_error(ibdev);
		break;

	case MLX5_DEV_EVENT_PORT_UP:
		ibev.event = IB_EVENT_PORT_ACTIVE;
		port = (u8)param;
		break;

	case MLX5_DEV_EVENT_PORT_DOWN:
	case MLX5_DEV_EVENT_PORT_INITIALIZED:
		ibev.event = IB_EVENT_PORT_ERR;
		port = (u8)param;
		break;

	case MLX5_DEV_EVENT_LID_CHANGE:
		ibev.event = IB_EVENT_LID_CHANGE;
		port = (u8)param;
		break;

	case MLX5_DEV_EVENT_PKEY_CHANGE:
		ibev.event = IB_EVENT_PKEY_CHANGE;
		port = (u8)param;
		break;

	case MLX5_DEV_EVENT_GUID_CHANGE:
		ibev.event = IB_EVENT_GID_CHANGE;
		port = (u8)param;
		break;

	case MLX5_DEV_EVENT_CLIENT_REREG:
		ibev.event = IB_EVENT_CLIENT_REREGISTER;
		port = (u8)param;
		break;

	default:
		break;
	}

	ibev.device	      = &ibdev->ib_dev;
	ibev.element.port_num = port;

	if ((event != MLX5_DEV_EVENT_SYS_ERROR) &&
	    (port < 1 || port > ibdev->num_ports)) {
		mlx5_ib_warn(ibdev, "warning: event on port %d\n", port);
		return;
	}

	if (ibdev->ib_active)
		ib_dispatch_event(&ibev);
}

static void get_ext_port_caps(struct mlx5_ib_dev *dev)
{
	int port;

	for (port = 1; port <= MLX5_CAP_GEN(dev->mdev, num_ports); port++)
		mlx5_query_ext_port_caps(dev, port);
}

static void config_atomic_responder(struct mlx5_ib_dev *dev,
				    struct ib_device_attr *props)
{
	enum ib_atomic_cap cap = props->atomic_cap;

#if 0
	if (cap == IB_ATOMIC_HCA ||
	    cap == IB_ATOMIC_GLOB)
#endif
		dev->enable_atomic_resp = 1;

	dev->atomic_cap = cap;
}

enum mlx5_addr_align {
	MLX5_ADDR_ALIGN_0	= 0,
	MLX5_ADDR_ALIGN_64	= 64,
	MLX5_ADDR_ALIGN_128	= 128,
};

static int get_port_caps(struct mlx5_ib_dev *dev)
{
	struct ib_device_attr *dprops = NULL;
	struct ib_port_attr *pprops = NULL;
	int err = -ENOMEM;
	int port;

	pprops = kmalloc(sizeof(*pprops), GFP_KERNEL);
	if (!pprops)
		goto out;

	dprops = kmalloc(sizeof(*dprops), GFP_KERNEL);
	if (!dprops)
		goto out;

	err = mlx5_ib_query_device(&dev->ib_dev, dprops);
	if (err) {
		mlx5_ib_warn(dev, "query_device failed %d\n", err);
		goto out;
	}
	config_atomic_responder(dev, dprops);

	for (port = 1; port <= MLX5_CAP_GEN(dev->mdev, num_ports); port++) {
		err = mlx5_ib_query_port(&dev->ib_dev, port, pprops);
		if (err) {
			mlx5_ib_warn(dev, "query_port %d failed %d\n",
				     port, err);
			break;
		}
		dev->mdev->port_caps[port - 1].pkey_table_len = dprops->max_pkeys;
		dev->mdev->port_caps[port - 1].gid_table_len = pprops->gid_tbl_len;
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

	ib_dereg_mr(dev->umrc.mr);
	ib_dealloc_pd(dev->umrc.pd);
}

enum {
	MAX_UMR_WR = 128,
};

static int create_umr_res(struct mlx5_ib_dev *dev)
{
	struct ib_pd *pd;
	struct ib_mr *mr;
	int ret;

	pd = ib_alloc_pd(&dev->ib_dev);
	if (IS_ERR(pd)) {
		mlx5_ib_dbg(dev, "Couldn't create PD for sync UMR QP\n");
		ret = PTR_ERR(pd);
		goto error_0;
	}

	mr = ib_get_dma_mr(pd,  IB_ACCESS_LOCAL_WRITE);
	if (IS_ERR(mr)) {
		mlx5_ib_dbg(dev, "Couldn't create DMA MR for sync UMR QP\n");
		ret = PTR_ERR(mr);
		goto error_1;
	}

	dev->umrc.mr = mr;
	dev->umrc.pd = pd;

	ret = mlx5_mr_cache_init(dev);
	if (ret) {
		mlx5_ib_warn(dev, "mr cache init failed %d\n", ret);
		goto error_4;
	}

	return 0;

error_4:
	ib_dereg_mr(mr);
error_1:
	ib_dealloc_pd(pd);
error_0:
	return ret;
}

static int create_dev_resources(struct mlx5_ib_resources *devr)
{
	struct ib_srq_init_attr attr;
	struct mlx5_ib_dev *dev;
	int ret = 0;

	dev = container_of(devr, struct mlx5_ib_dev, devr);

	devr->p0 = mlx5_ib_alloc_pd(&dev->ib_dev, NULL, NULL);
	if (IS_ERR(devr->p0)) {
		ret = PTR_ERR(devr->p0);
		goto error0;
	}
	devr->p0->device  = &dev->ib_dev;
	devr->p0->uobject = NULL;
	atomic_set(&devr->p0->usecnt, 0);

	devr->c0 = mlx5_ib_create_cq(&dev->ib_dev, 1, 0, NULL, NULL);
	if (IS_ERR(devr->c0)) {
		ret = PTR_ERR(devr->c0);
		goto error1;
	}
	devr->c0->device        = &dev->ib_dev;
	devr->c0->uobject       = NULL;
	devr->c0->comp_handler  = NULL;
	devr->c0->event_handler = NULL;
	devr->c0->cq_context    = NULL;
	atomic_set(&devr->c0->usecnt, 0);

	devr->x0 = mlx5_ib_alloc_xrcd(&dev->ib_dev, NULL, NULL);
	if (IS_ERR(devr->x0)) {
		ret = PTR_ERR(devr->x0);
		goto error2;
	}
	devr->x0->device = &dev->ib_dev;
	devr->x0->inode = NULL;
	atomic_set(&devr->x0->usecnt, 0);
	mutex_init(&devr->x0->tgt_qp_mutex);
	INIT_LIST_HEAD(&devr->x0->tgt_qp_list);

	devr->x1 = mlx5_ib_alloc_xrcd(&dev->ib_dev, NULL, NULL);
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
	attr.ext.xrc.cq = devr->c0;
	attr.ext.xrc.xrcd = devr->x0;

	devr->s0 = mlx5_ib_create_srq(devr->p0, &attr, NULL);
	if (IS_ERR(devr->s0)) {
		ret = PTR_ERR(devr->s0);
		goto error4;
	}
	devr->s0->device	= &dev->ib_dev;
	devr->s0->pd		= devr->p0;
	devr->s0->uobject       = NULL;
	devr->s0->event_handler = NULL;
	devr->s0->srq_context   = NULL;
	devr->s0->srq_type      = IB_SRQT_XRC;
	devr->s0->ext.xrc.xrcd  = devr->x0;
	devr->s0->ext.xrc.cq	= devr->c0;
	atomic_inc(&devr->s0->ext.xrc.xrcd->usecnt);
	atomic_inc(&devr->s0->ext.xrc.cq->usecnt);
	atomic_inc(&devr->p0->usecnt);
	atomic_set(&devr->s0->usecnt, 0);

	memset(&attr, 0, sizeof(attr));
	attr.attr.max_sge = 1;
	attr.attr.max_wr = 1;
	attr.srq_type = IB_SRQT_BASIC;
	devr->s1 = mlx5_ib_create_srq(devr->p0, &attr, NULL);
	if (IS_ERR(devr->s1)) {
		ret = PTR_ERR(devr->s1);
		goto error5;
	}
	devr->s1->device	= &dev->ib_dev;
	devr->s1->pd		= devr->p0;
	devr->s1->uobject       = NULL;
	devr->s1->event_handler = NULL;
	devr->s1->srq_context   = NULL;
	devr->s1->srq_type      = IB_SRQT_BASIC;
	devr->s1->ext.xrc.cq	= devr->c0;
	atomic_inc(&devr->p0->usecnt);
	atomic_set(&devr->s1->usecnt, 0);

	return 0;

error5:
	mlx5_ib_destroy_srq(devr->s0);
error4:
	mlx5_ib_dealloc_xrcd(devr->x1);
error3:
	mlx5_ib_dealloc_xrcd(devr->x0);
error2:
	mlx5_ib_destroy_cq(devr->c0);
error1:
	mlx5_ib_dealloc_pd(devr->p0);
error0:
	return ret;
}

static void destroy_dev_resources(struct mlx5_ib_resources *devr)
{
	mlx5_ib_destroy_srq(devr->s1);
	mlx5_ib_destroy_srq(devr->s0);
	mlx5_ib_dealloc_xrcd(devr->x0);
	mlx5_ib_dealloc_xrcd(devr->x1);
	mlx5_ib_destroy_cq(devr->c0);
	mlx5_ib_dealloc_pd(devr->p0);
}

static void enable_dc_tracer(struct mlx5_ib_dev *dev)
{
	struct device *device = dev->ib_dev.dma_device;
	struct mlx5_dc_tracer *dct = &dev->dctr;
	int order;
	void *tmp;
	int size;
	int err;

	size = MLX5_CAP_GEN(dev->mdev, num_ports) * 4096;
	if (size <= PAGE_SIZE)
		order = 0;
	else
		order = 1;

	dct->pg = alloc_pages(GFP_KERNEL, order);
	if (!dct->pg) {
		mlx5_ib_err(dev, "failed to allocate %d pages\n", order);
		return;
	}

	tmp = page_address(dct->pg);
	memset(tmp, 0xff, size);

	dct->size = size;
	dct->order = order;
	dct->dma = dma_map_page(device, dct->pg, 0, size, DMA_FROM_DEVICE);
	if (dma_mapping_error(device, dct->dma)) {
		mlx5_ib_err(dev, "dma mapping error\n");
		goto map_err;
	}

	err = mlx5_core_set_dc_cnak_trace(dev->mdev, 1, dct->dma);
	if (err) {
		mlx5_ib_warn(dev, "failed to enable DC tracer\n");
		goto cmd_err;
	}

	return;

cmd_err:
	dma_unmap_page(device, dct->dma, size, DMA_FROM_DEVICE);
map_err:
	__free_pages(dct->pg, dct->order);
	dct->pg = NULL;
}

static void disable_dc_tracer(struct mlx5_ib_dev *dev)
{
	struct device *device = dev->ib_dev.dma_device;
	struct mlx5_dc_tracer *dct = &dev->dctr;
	int err;

	if (!dct->pg)
		return;

	err = mlx5_core_set_dc_cnak_trace(dev->mdev, 0, dct->dma);
	if (err) {
		mlx5_ib_warn(dev, "failed to disable DC tracer\n");
		return;
	}

	dma_unmap_page(device, dct->dma, dct->size, DMA_FROM_DEVICE);
	__free_pages(dct->pg, dct->order);
	dct->pg = NULL;
}

enum {
	MLX5_DC_CNAK_SIZE		= 128,
	MLX5_NUM_BUF_IN_PAGE		= PAGE_SIZE / MLX5_DC_CNAK_SIZE,
	MLX5_CNAK_TX_CQ_SIGNAL_FACTOR	= 128,
	MLX5_DC_CNAK_SL			= 0,
	MLX5_DC_CNAK_VL			= 0,
};

static int init_dc_improvements(struct mlx5_ib_dev *dev)
{
	if (!mlx5_core_is_pf(dev->mdev))
		return 0;

	if (!(MLX5_CAP_GEN(dev->mdev, dc_cnak_trace)))
		return 0;

	enable_dc_tracer(dev);

	return 0;
}

static void cleanup_dc_improvements(struct mlx5_ib_dev *dev)
{

	disable_dc_tracer(dev);
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
				     "couldn't allocate queue counter for port %d\n",
				     i + 1);
			goto dealloc_counters;
		}
	}

	return 0;

dealloc_counters:
	while (--i >= 0)
		mlx5_ib_dealloc_q_port_counter(dev, i);

	return ret;
}

struct port_attribute {
	struct attribute attr;
	ssize_t (*show)(struct mlx5_ib_port *,
			struct port_attribute *, char *buf);
	ssize_t (*store)(struct mlx5_ib_port *,
			 struct port_attribute *,
			 const char *buf, size_t count);
};

struct port_counter_attribute {
	struct port_attribute	attr;
	size_t			offset;
};

static ssize_t port_attr_show(struct kobject *kobj,
			      struct attribute *attr, char *buf)
{
	struct port_attribute *port_attr =
		container_of(attr, struct port_attribute, attr);
	struct mlx5_ib_port_sysfs_group *p =
		container_of(kobj, struct mlx5_ib_port_sysfs_group,
			     kobj);
	struct mlx5_ib_port *mibport = container_of(p, struct mlx5_ib_port,
						    group);

	if (!port_attr->show)
		return -EIO;

	return port_attr->show(mibport, port_attr, buf);
}

static ssize_t show_port_counter(struct mlx5_ib_port *p,
				 struct port_attribute *port_attr,
				 char *buf)
{
	int outlen = MLX5_ST_SZ_BYTES(query_q_counter_out);
	struct port_counter_attribute *counter_attr =
		container_of(port_attr, struct port_counter_attribute, attr);
	void *out;
	int ret;

	out = mlx5_vzalloc(outlen);
	if (!out)
		return -ENOMEM;

	ret = mlx5_vport_query_q_counter(p->dev->mdev,
					 p->q_cnt_id, 0,
					 out, outlen);
	if (ret)
		goto free;

	ret = sprintf(buf, "%d\n",
		      be32_to_cpu(*(__be32 *)(out + counter_attr->offset)));

free:
	kfree(out);
	return ret;
}

#define PORT_COUNTER_ATTR(_name)					\
struct port_counter_attribute port_counter_attr_##_name = {		\
	.attr  = __ATTR(_name, S_IRUGO, show_port_counter, NULL),	\
	.offset = MLX5_BYTE_OFF(query_q_counter_out, _name)		\
}

static PORT_COUNTER_ATTR(rx_write_requests);
static PORT_COUNTER_ATTR(rx_read_requests);
static PORT_COUNTER_ATTR(rx_atomic_requests);
static PORT_COUNTER_ATTR(rx_dct_connect);
static PORT_COUNTER_ATTR(out_of_buffer);
static PORT_COUNTER_ATTR(out_of_sequence);
static PORT_COUNTER_ATTR(duplicate_request);
static PORT_COUNTER_ATTR(rnr_nak_retry_err);
static PORT_COUNTER_ATTR(packet_seq_err);
static PORT_COUNTER_ATTR(implied_nak_seq_err);
static PORT_COUNTER_ATTR(local_ack_timeout_err);

static struct attribute *counter_attrs[] = {
	&port_counter_attr_rx_write_requests.attr.attr,
	&port_counter_attr_rx_read_requests.attr.attr,
	&port_counter_attr_rx_atomic_requests.attr.attr,
	&port_counter_attr_rx_dct_connect.attr.attr,
	&port_counter_attr_out_of_buffer.attr.attr,
	&port_counter_attr_out_of_sequence.attr.attr,
	&port_counter_attr_duplicate_request.attr.attr,
	&port_counter_attr_rnr_nak_retry_err.attr.attr,
	&port_counter_attr_packet_seq_err.attr.attr,
	&port_counter_attr_implied_nak_seq_err.attr.attr,
	&port_counter_attr_local_ack_timeout_err.attr.attr,
	NULL
};

static struct attribute_group port_counters_group = {
	.name  = "counters",
	.attrs  = counter_attrs
};

static const struct sysfs_ops port_sysfs_ops = {
	.show = port_attr_show
};

static struct kobj_type port_type = {
	.sysfs_ops     = &port_sysfs_ops,
};

static int add_port_attrs(struct mlx5_ib_dev *dev,
			  struct kobject *parent,
			  struct mlx5_ib_port_sysfs_group *port,
			  u8 port_num)
{
	int ret;

	ret = kobject_init_and_add(&port->kobj, &port_type,
				   parent,
				   "%d", port_num);
	if (ret)
		return ret;

	if (MLX5_CAP_GEN(dev->mdev, out_of_seq_cnt) &&
	    MLX5_CAP_GEN(dev->mdev, retransmission_q_counters)) {
		ret = sysfs_create_group(&port->kobj, &port_counters_group);
		if (ret)
			goto put_kobj;
	}

	port->enabled = true;
	return ret;

put_kobj:
	kobject_put(&port->kobj);
	return ret;
}

static void destroy_ports_attrs(struct mlx5_ib_dev *dev,
				unsigned int num_ports)
{
	unsigned int i;

	for (i = 0; i < num_ports; i++) {
		struct mlx5_ib_port_sysfs_group *port =
			&dev->port[i].group;

		if (!port->enabled)
			continue;

		if (MLX5_CAP_GEN(dev->mdev, out_of_seq_cnt) &&
		    MLX5_CAP_GEN(dev->mdev, retransmission_q_counters))
			sysfs_remove_group(&port->kobj,
					   &port_counters_group);
		kobject_put(&port->kobj);
		port->enabled = false;
	}

	if (dev->ports_parent) {
		kobject_put(dev->ports_parent);
		dev->ports_parent = NULL;
	}
}

static int create_port_attrs(struct mlx5_ib_dev *dev)
{
	int ret = 0;
	unsigned int i = 0;
	struct device *device = &dev->ib_dev.dev;

	dev->ports_parent = kobject_create_and_add("mlx5_ports",
						   &device->kobj);
	if (!dev->ports_parent)
		return -ENOMEM;

	for (i = 0; i < dev->num_ports; i++) {
		ret = add_port_attrs(dev,
				     dev->ports_parent,
				     &dev->port[i].group,
				     i + 1);

		if (ret)
			goto _destroy_ports_attrs;
	}

	return 0;

_destroy_ports_attrs:
	destroy_ports_attrs(dev, i);
	return ret;
}

static void *mlx5_ib_add(struct mlx5_core_dev *mdev)
{
	struct mlx5_ib_dev *dev;
	int err;
	int i;

	printk_once(KERN_INFO "%s", mlx5_version);

	dev = (struct mlx5_ib_dev *)ib_alloc_device(sizeof(*dev));
	if (!dev)
		return NULL;

	dev->mdev = mdev;

	dev->port = kcalloc(MLX5_CAP_GEN(mdev, num_ports), sizeof(*dev->port),
			     GFP_KERNEL);
	if (!dev->port)
		goto err_dealloc;

	for (i = 0; i < MLX5_CAP_GEN(mdev, num_ports); i++) {
		dev->port[i].dev = dev;
		dev->port[i].port_num = i;
		dev->port[i].port_gone = 0;
		memset(dev->port[i].gid_table, 0, sizeof(dev->port[i].gid_table));
	}

	err = get_port_caps(dev);
	if (err)
		goto err_free_port;

	if (mlx5_use_mad_ifc(dev))
		get_ext_port_caps(dev);

	if (mlx5_ib_port_link_layer(&dev->ib_dev, 1) ==
	    IB_LINK_LAYER_ETHERNET) {
		if (MLX5_CAP_GEN(mdev, roce)) {
			err = mlx5_nic_vport_enable_roce(mdev);
			if (err)
				goto err_free_port;
		} else {
			goto err_free_port;
		}
	}

	MLX5_INIT_DOORBELL_LOCK(&dev->uar_lock);

	strlcpy(dev->ib_dev.name, "mlx5_%d", IB_DEVICE_NAME_MAX);
	dev->ib_dev.owner		= THIS_MODULE;
	dev->ib_dev.node_type		= RDMA_NODE_IB_CA;
	dev->ib_dev.local_dma_lkey	= mdev->special_contexts.resd_lkey;
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
		(1ull << IB_USER_VERBS_CMD_REG_MR)		|
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

	dev->ib_dev.query_device	= mlx5_ib_query_device;
	dev->ib_dev.query_port		= mlx5_ib_query_port;
	dev->ib_dev.get_link_layer	= mlx5_ib_port_link_layer;
	dev->ib_dev.query_gid		= mlx5_ib_query_gid;
	dev->ib_dev.query_pkey		= mlx5_ib_query_pkey;
	dev->ib_dev.modify_device	= mlx5_ib_modify_device;
	dev->ib_dev.modify_port		= mlx5_ib_modify_port;
	dev->ib_dev.alloc_ucontext	= mlx5_ib_alloc_ucontext;
	dev->ib_dev.dealloc_ucontext	= mlx5_ib_dealloc_ucontext;
	dev->ib_dev.mmap		= mlx5_ib_mmap;
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
	dev->ib_dev.reg_phys_mr		= mlx5_ib_reg_phys_mr;
	dev->ib_dev.dereg_mr		= mlx5_ib_dereg_mr;
	dev->ib_dev.attach_mcast	= mlx5_ib_mcg_attach;
	dev->ib_dev.detach_mcast	= mlx5_ib_mcg_detach;
	dev->ib_dev.process_mad		= mlx5_ib_process_mad;
	dev->ib_dev.alloc_fast_reg_mr	= mlx5_ib_alloc_fast_reg_mr;
	dev->ib_dev.alloc_fast_reg_page_list = mlx5_ib_alloc_fast_reg_page_list;
	dev->ib_dev.free_fast_reg_page_list  = mlx5_ib_free_fast_reg_page_list;

	if (MLX5_CAP_GEN(mdev, xrc)) {
		dev->ib_dev.alloc_xrcd = mlx5_ib_alloc_xrcd;
		dev->ib_dev.dealloc_xrcd = mlx5_ib_dealloc_xrcd;
		dev->ib_dev.uverbs_cmd_mask |=
			(1ull << IB_USER_VERBS_CMD_OPEN_XRCD) |
			(1ull << IB_USER_VERBS_CMD_CLOSE_XRCD);
	}

	err = init_node_data(dev);
	if (err)
		goto err_disable_roce;

	mutex_init(&dev->cap_mask_mutex);
	INIT_LIST_HEAD(&dev->qp_list);
	spin_lock_init(&dev->reset_flow_resource_lock);

	err = create_dev_resources(&dev->devr);
	if (err)
		goto err_disable_roce;


	err = mlx5_ib_alloc_q_counters(dev);
	if (err)
		goto err_odp;

	err = ib_register_device(&dev->ib_dev, NULL);
	if (err)
		goto err_q_cnt;

	err = create_umr_res(dev);
	if (err)
		goto err_dev;

	if (MLX5_CAP_GEN(dev->mdev, port_type) ==
	    MLX5_CAP_PORT_TYPE_IB) {
		if (init_dc_improvements(dev))
			mlx5_ib_dbg(dev, "init_dc_improvements - continuing\n");
	}

	err = create_port_attrs(dev);
	if (err)
		goto err_dc;

	for (i = 0; i < ARRAY_SIZE(mlx5_class_attributes); i++) {
		err = device_create_file(&dev->ib_dev.dev,
					 mlx5_class_attributes[i]);
		if (err)
			goto err_port_attrs;
	}

	if (1) {
		struct thread *rl_thread = NULL;
		struct proc *rl_proc = NULL;

		for (i = 0; i < MLX5_CAP_GEN(mdev, num_ports); i++) {
			(void) kproc_kthread_add(mlx5_ib_roce_port_update, dev->port + i, &rl_proc, &rl_thread,
			    RFHIGHPID, 0, "mlx5-ib-roce-port", "mlx5-ib-roce_port-%d", i);
		}
	}

	dev->ib_active = true;

	return dev;

err_port_attrs:
	destroy_ports_attrs(dev, dev->num_ports);

err_dc:
	if (MLX5_CAP_GEN(dev->mdev, port_type) ==
	    MLX5_CAP_PORT_TYPE_IB)
		cleanup_dc_improvements(dev);
	destroy_umrc_res(dev);

err_dev:
	ib_unregister_device(&dev->ib_dev);

err_q_cnt:
	mlx5_ib_dealloc_q_counters(dev);

err_odp:
	destroy_dev_resources(&dev->devr);

err_disable_roce:
	if (mlx5_ib_port_link_layer(&dev->ib_dev, 1) ==
	    IB_LINK_LAYER_ETHERNET && MLX5_CAP_GEN(mdev, roce))
		mlx5_nic_vport_disable_roce(mdev);
err_free_port:
	kfree(dev->port);

err_dealloc:
	ib_dealloc_device((struct ib_device *)dev);

	return NULL;
}

static void mlx5_ib_remove(struct mlx5_core_dev *mdev, void *context)
{
	struct mlx5_ib_dev *dev = context;
	int i;

	for (i = 0; i < MLX5_CAP_GEN(mdev, num_ports); i++) {
		dev->port[i].port_gone = 1;
		while (dev->port[i].port_gone != 2)
			pause("W", hz);
	}

	for (i = 0; i < ARRAY_SIZE(mlx5_class_attributes); i++) {
		device_remove_file(&dev->ib_dev.dev,
		    mlx5_class_attributes[i]);
	}

	destroy_ports_attrs(dev, dev->num_ports);
	if (MLX5_CAP_GEN(dev->mdev, port_type) ==
	    MLX5_CAP_PORT_TYPE_IB)
		cleanup_dc_improvements(dev);
	mlx5_ib_dealloc_q_counters(dev);
	ib_unregister_device(&dev->ib_dev);
	destroy_umrc_res(dev);
	destroy_dev_resources(&dev->devr);

	if (mlx5_ib_port_link_layer(&dev->ib_dev, 1) ==
	    IB_LINK_LAYER_ETHERNET && MLX5_CAP_GEN(mdev, roce))
		mlx5_nic_vport_disable_roce(mdev);

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

	if (deprecated_prof_sel != 2)
		printf("mlx5_ib: WARN: ""prof_sel is deprecated for mlx5_ib, set it for mlx5_core\n");

	err = mlx5_register_interface(&mlx5_ib_interface);
	if (err)
		goto clean_odp;

	mlx5_ib_wq = create_singlethread_workqueue("mlx5_ib_wq");
	if (!mlx5_ib_wq) {
		printf("mlx5_ib: ERR: ""%s: failed to create mlx5_ib_wq\n", __func__);
		goto err_unreg;
	}

	return err;

err_unreg:
	mlx5_unregister_interface(&mlx5_ib_interface);

clean_odp:
	return err;
}

static void __exit mlx5_ib_cleanup(void)
{
	destroy_workqueue(mlx5_ib_wq);
	mlx5_unregister_interface(&mlx5_ib_interface);
}

module_init_order(mlx5_ib_init, SI_ORDER_THIRD);
module_exit_order(mlx5_ib_cleanup, SI_ORDER_THIRD);
