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

#include <rdma/ib_mad.h>
#include <rdma/ib_smi.h>
#include <rdma/ib_pma.h>
#include "mlx5_ib.h"
#include <dev/mlx5/vport.h>

#define MAX_U32 0xffffffffULL
#define MAX_U16 0xffffUL

/* Counters should be saturate once they reach their maximum value */
#define ASSIGN_32BIT_COUNTER(counter, value) do {	\
	if ((value) > MAX_U32)				\
		counter = cpu_to_be32(MAX_U32);		\
	else						\
		counter = cpu_to_be32(value);		\
} while (0)

/* Counters should be saturate once they reach their maximum value */
#define ASSIGN_16BIT_COUNTER(counter, value) do {	\
	if ((value) > MAX_U16)				\
		counter = cpu_to_be16(MAX_U16);		\
	else						\
		counter = cpu_to_be16(value);		\
} while (0)

enum {
	MLX5_IB_VENDOR_CLASS1 = 0x9,
	MLX5_IB_VENDOR_CLASS2 = 0xa
};

int mlx5_MAD_IFC(struct mlx5_ib_dev *dev, int ignore_mkey, int ignore_bkey,
		 u8 port, struct ib_wc *in_wc, struct ib_grh *in_grh,
		 void *in_mad, void *response_mad)
{
	u8 op_modifier = 0;

	/* Key check traps can't be generated unless we have in_wc to
	 * tell us where to send the trap.
	 */
	if (ignore_mkey || !in_wc)
		op_modifier |= 0x1;
	if (ignore_bkey || !in_wc)
		op_modifier |= 0x2;

	return mlx5_core_mad_ifc(dev->mdev, in_mad, response_mad, op_modifier, port);
}

static int process_mad(struct ib_device *ibdev, int mad_flags, u8 port_num,
		       struct ib_wc *in_wc, struct ib_grh *in_grh,
		       struct ib_mad *in_mad, struct ib_mad *out_mad)
{
	u16 slid;
	int err;

	slid = in_wc ? in_wc->slid : be16_to_cpu(IB_LID_PERMISSIVE);

	if (in_mad->mad_hdr.method == IB_MGMT_METHOD_TRAP && slid == 0)
		return IB_MAD_RESULT_SUCCESS | IB_MAD_RESULT_CONSUMED;

	if (in_mad->mad_hdr.mgmt_class == IB_MGMT_CLASS_SUBN_LID_ROUTED ||
	    in_mad->mad_hdr.mgmt_class == IB_MGMT_CLASS_SUBN_DIRECTED_ROUTE) {
		if (in_mad->mad_hdr.method   != IB_MGMT_METHOD_GET &&
		    in_mad->mad_hdr.method   != IB_MGMT_METHOD_SET &&
		    in_mad->mad_hdr.method   != IB_MGMT_METHOD_TRAP_REPRESS)
			return IB_MAD_RESULT_SUCCESS;

		/* Don't process SMInfo queries -- the SMA can't handle them.
		 */
		if (in_mad->mad_hdr.attr_id == IB_SMP_ATTR_SM_INFO)
			return IB_MAD_RESULT_SUCCESS;
	} else if (in_mad->mad_hdr.mgmt_class == IB_MGMT_CLASS_PERF_MGMT ||
		   in_mad->mad_hdr.mgmt_class == MLX5_IB_VENDOR_CLASS1   ||
		   in_mad->mad_hdr.mgmt_class == MLX5_IB_VENDOR_CLASS2   ||
		   in_mad->mad_hdr.mgmt_class == IB_MGMT_CLASS_CONG_MGMT) {
		if (in_mad->mad_hdr.method  != IB_MGMT_METHOD_GET &&
		    in_mad->mad_hdr.method  != IB_MGMT_METHOD_SET)
			return IB_MAD_RESULT_SUCCESS;
	} else {
		return IB_MAD_RESULT_SUCCESS;
	}

	err = mlx5_MAD_IFC(to_mdev(ibdev),
			   mad_flags & IB_MAD_IGNORE_MKEY,
			   mad_flags & IB_MAD_IGNORE_BKEY,
			   port_num, in_wc, in_grh, in_mad, out_mad);
	if (err)
		return IB_MAD_RESULT_FAILURE;

	/* set return bit in status of directed route responses */
	if (in_mad->mad_hdr.mgmt_class == IB_MGMT_CLASS_SUBN_DIRECTED_ROUTE)
		out_mad->mad_hdr.status |= cpu_to_be16(1 << 15);

	if (in_mad->mad_hdr.method == IB_MGMT_METHOD_TRAP_REPRESS)
		/* no response for trap repress */
		return IB_MAD_RESULT_SUCCESS | IB_MAD_RESULT_CONSUMED;

	return IB_MAD_RESULT_SUCCESS | IB_MAD_RESULT_REPLY;
}

static void pma_cnt_ext_assign(struct ib_pma_portcounters_ext *pma_cnt_ext,
			       struct mlx5_vport_counters *vc)
{
	pma_cnt_ext->port_xmit_data = cpu_to_be64((vc->transmitted_ib_unicast.octets +
						   vc->transmitted_ib_multicast.octets) >> 2);
	pma_cnt_ext->port_rcv_data = cpu_to_be64((vc->received_ib_unicast.octets +
						  vc->received_ib_multicast.octets) >> 2);
	pma_cnt_ext->port_xmit_packets = cpu_to_be64(vc->transmitted_ib_unicast.packets +
						     vc->transmitted_ib_multicast.packets);
	pma_cnt_ext->port_rcv_packets = cpu_to_be64(vc->received_ib_unicast.packets +
						    vc->received_ib_multicast.packets);
	pma_cnt_ext->port_unicast_xmit_packets = cpu_to_be64(vc->transmitted_ib_unicast.packets);
	pma_cnt_ext->port_unicast_rcv_packets = cpu_to_be64(vc->received_ib_unicast.packets);
	pma_cnt_ext->port_multicast_xmit_packets = cpu_to_be64(vc->transmitted_ib_multicast.packets);
	pma_cnt_ext->port_multicast_rcv_packets = cpu_to_be64(vc->received_ib_multicast.packets);
}

static void pma_cnt_assign(struct ib_pma_portcounters *pma_cnt,
			   struct mlx5_vport_counters *vc)
{
	ASSIGN_32BIT_COUNTER(pma_cnt->port_xmit_data,
			     (vc->transmitted_ib_unicast.octets +
			      vc->transmitted_ib_multicast.octets) >> 2);
	ASSIGN_32BIT_COUNTER(pma_cnt->port_rcv_data,
			     (vc->received_ib_unicast.octets +
			      vc->received_ib_multicast.octets) >> 2);
	ASSIGN_32BIT_COUNTER(pma_cnt->port_xmit_packets,
			     vc->transmitted_ib_unicast.packets +
			     vc->transmitted_ib_multicast.packets);
	ASSIGN_32BIT_COUNTER(pma_cnt->port_rcv_packets,
			     vc->received_ib_unicast.packets +
			     vc->received_ib_multicast.packets);
}

static int process_pma_cmd(struct ib_device *ibdev, u8 port_num,
			   struct ib_mad *in_mad, struct ib_mad *out_mad)
{
	struct mlx5_ib_dev *dev = to_mdev(ibdev);
	struct mlx5_vport_counters *vc;
	int err;
	int ext;

	vc = kzalloc(sizeof(*vc), GFP_KERNEL);
	if (!vc)
		return -ENOMEM;

	ext = in_mad->mad_hdr.attr_id == IB_PMA_PORT_COUNTERS_EXT;

	err = mlx5_get_vport_counters(dev->mdev, port_num, vc);
	if (!err) {
		if (ext) {
			struct ib_pma_portcounters_ext *pma_cnt_ext =
				(struct ib_pma_portcounters_ext *)(out_mad->data + 40);

			pma_cnt_ext_assign(pma_cnt_ext, vc);
		} else {
			struct ib_pma_portcounters *pma_cnt =
				(struct ib_pma_portcounters *)(out_mad->data + 40);

			ASSIGN_16BIT_COUNTER(pma_cnt->port_rcv_errors,
					     (u16)vc->received_errors.packets);

			pma_cnt_assign(pma_cnt, vc);
		}
		err = IB_MAD_RESULT_SUCCESS | IB_MAD_RESULT_REPLY;
	}

	kfree(vc);
	return err;
}

int mlx5_ib_process_mad(struct ib_device *ibdev, int mad_flags, u8 port_num,
			struct ib_wc *in_wc, struct ib_grh *in_grh,
			struct ib_mad *in_mad, struct ib_mad *out_mad)
{
	struct mlx5_ib_dev *dev = to_mdev(ibdev);
	struct mlx5_core_dev *mdev = dev->mdev;

	memset(out_mad->data, 0, sizeof(out_mad->data));

	if (MLX5_CAP_GEN(mdev, vport_counters) &&
	    in_mad->mad_hdr.mgmt_class == IB_MGMT_CLASS_PERF_MGMT &&
	    in_mad->mad_hdr.method == IB_MGMT_METHOD_GET) {
		/* TBD: read error counters from the PPCNT */
		return process_pma_cmd(ibdev, port_num, in_mad, out_mad);
	} else {
		return process_mad(ibdev, mad_flags, port_num, in_wc, in_grh,
				   in_mad, out_mad);
	}
}

int mlx5_query_ext_port_caps(struct mlx5_ib_dev *dev, u8 port)
{
	struct ib_smp *in_mad  = NULL;
	struct ib_smp *out_mad = NULL;
	int err = -ENOMEM;
	u16 packet_error;

	in_mad  = kzalloc(sizeof(*in_mad), GFP_KERNEL);
	out_mad = kmalloc(sizeof(*out_mad), GFP_KERNEL);
	if (!in_mad || !out_mad)
		goto out;

	init_query_mad(in_mad);
	in_mad->attr_id = MLX5_ATTR_EXTENDED_PORT_INFO;
	in_mad->attr_mod = cpu_to_be32(port);

	err = mlx5_MAD_IFC(dev, 1, 1, 1, NULL, NULL, in_mad, out_mad);

	packet_error = be16_to_cpu(out_mad->status);

	dev->mdev->port_caps[port - 1].ext_port_cap = (!err && !packet_error) ?
		MLX_EXT_PORT_CAP_FLAG_EXTENDED_PORT_INFO : 0;

out:
	kfree(in_mad);
	kfree(out_mad);
	return err;
}

int mlx5_query_smp_attr_node_info_mad_ifc(struct ib_device *ibdev,
					  struct ib_smp *out_mad)
{
	struct ib_smp *in_mad = NULL;
	int err = -ENOMEM;

	in_mad = kzalloc(sizeof(*in_mad), GFP_KERNEL);
	if (!in_mad)
		return -ENOMEM;

	init_query_mad(in_mad);
	in_mad->attr_id = IB_SMP_ATTR_NODE_INFO;

	err = mlx5_MAD_IFC(to_mdev(ibdev), 1, 1, 1, NULL, NULL, in_mad,
			   out_mad);

	kfree(in_mad);
	return err;
}

int mlx5_query_system_image_guid_mad_ifc(struct ib_device *ibdev,
					 __be64 *sys_image_guid)
{
	struct ib_smp *out_mad = NULL;
	int err = -ENOMEM;

	out_mad = kmalloc(sizeof(*out_mad), GFP_KERNEL);
	if (!out_mad)
		return -ENOMEM;

	err = mlx5_query_smp_attr_node_info_mad_ifc(ibdev, out_mad);
	if (err)
		goto out;

	memcpy(sys_image_guid, out_mad->data + 4, 8);

out:
	kfree(out_mad);

	return err;
}

int mlx5_query_max_pkeys_mad_ifc(struct ib_device *ibdev,
				 u16 *max_pkeys)
{
	struct ib_smp *out_mad = NULL;
	int err = -ENOMEM;

	out_mad = kmalloc(sizeof(*out_mad), GFP_KERNEL);
	if (!out_mad)
		return -ENOMEM;

	err = mlx5_query_smp_attr_node_info_mad_ifc(ibdev, out_mad);
	if (err)
		goto out;

	*max_pkeys = be16_to_cpup((__be16 *)(out_mad->data + 28));

out:
	kfree(out_mad);

	return err;
}

int mlx5_query_vendor_id_mad_ifc(struct ib_device *ibdev,
				 u32 *vendor_id)
{
	struct ib_smp *out_mad = NULL;
	int err = -ENOMEM;

	out_mad = kmalloc(sizeof(*out_mad), GFP_KERNEL);
	if (!out_mad)
		return -ENOMEM;

	err = mlx5_query_smp_attr_node_info_mad_ifc(ibdev, out_mad);
	if (err)
		goto out;

	*vendor_id = be32_to_cpup((__be32 *)(out_mad->data + 36)) & 0xffff;

out:
	kfree(out_mad);

	return err;
}

int mlx5_query_node_desc_mad_ifc(struct mlx5_ib_dev *dev, char *node_desc)
{
	struct ib_smp *in_mad  = NULL;
	struct ib_smp *out_mad = NULL;
	int err = -ENOMEM;

	in_mad  = kzalloc(sizeof(*in_mad), GFP_KERNEL);
	out_mad = kmalloc(sizeof(*out_mad), GFP_KERNEL);
	if (!in_mad || !out_mad)
		goto out;

	init_query_mad(in_mad);
	in_mad->attr_id = IB_SMP_ATTR_NODE_DESC;

	err = mlx5_MAD_IFC(dev, 1, 1, 1, NULL, NULL, in_mad, out_mad);
	if (err)
		goto out;

	memcpy(node_desc, out_mad->data, 64);
out:
	kfree(in_mad);
	kfree(out_mad);
	return err;
}

int mlx5_query_node_guid_mad_ifc(struct mlx5_ib_dev *dev, u64 *node_guid)
{
	struct ib_smp *in_mad  = NULL;
	struct ib_smp *out_mad = NULL;
	int err = -ENOMEM;

	in_mad  = kzalloc(sizeof(*in_mad), GFP_KERNEL);
	out_mad = kmalloc(sizeof(*out_mad), GFP_KERNEL);
	if (!in_mad || !out_mad)
		goto out;

	init_query_mad(in_mad);
	in_mad->attr_id = IB_SMP_ATTR_NODE_INFO;

	err = mlx5_MAD_IFC(dev, 1, 1, 1, NULL, NULL, in_mad, out_mad);
	if (err)
		goto out;

	memcpy(&dev->ib_dev.node_guid, out_mad->data + 12, 8);
out:
	kfree(in_mad);
	kfree(out_mad);
	return err;
}

int mlx5_query_pkey_mad_ifc(struct ib_device *ibdev, u8 port, u16 index,
			    u16 *pkey)
{
	struct ib_smp *in_mad  = NULL;
	struct ib_smp *out_mad = NULL;
	int err = -ENOMEM;

	in_mad  = kzalloc(sizeof(*in_mad), GFP_KERNEL);
	out_mad = kmalloc(sizeof(*out_mad), GFP_KERNEL);
	if (!in_mad || !out_mad)
		goto out;

	init_query_mad(in_mad);
	in_mad->attr_id  = IB_SMP_ATTR_PKEY_TABLE;
	in_mad->attr_mod = cpu_to_be32(index / 32);

	err = mlx5_MAD_IFC(to_mdev(ibdev), 1, 1, port, NULL, NULL, in_mad,
			   out_mad);
	if (err)
		goto out;

	*pkey = be16_to_cpu(((__be16 *)out_mad->data)[index % 32]);

out:
	kfree(in_mad);
	kfree(out_mad);
	return err;
}

int mlx5_query_gids_mad_ifc(struct ib_device *ibdev, u8 port, int index,
			    union ib_gid *gid)
{
	struct ib_smp *in_mad  = NULL;
	struct ib_smp *out_mad = NULL;
	int err = -ENOMEM;

	in_mad  = kzalloc(sizeof(*in_mad), GFP_KERNEL);
	out_mad = kmalloc(sizeof(*out_mad), GFP_KERNEL);
	if (!in_mad || !out_mad)
		goto out;

	init_query_mad(in_mad);
	in_mad->attr_id  = IB_SMP_ATTR_PORT_INFO;
	in_mad->attr_mod = cpu_to_be32(port);

	err = mlx5_MAD_IFC(to_mdev(ibdev), 1, 1, port, NULL, NULL, in_mad,
			   out_mad);
	if (err)
		goto out;

	memcpy(gid->raw, out_mad->data + 8, 8);

	init_query_mad(in_mad);
	in_mad->attr_id  = IB_SMP_ATTR_GUID_INFO;
	in_mad->attr_mod = cpu_to_be32(index / 8);

	err = mlx5_MAD_IFC(to_mdev(ibdev), 1, 1, port, NULL, NULL, in_mad,
			   out_mad);
	if (err)
		goto out;

	memcpy(gid->raw + 8, out_mad->data + (index % 8) * 8, 8);

out:
	kfree(in_mad);
	kfree(out_mad);
	return err;
}

int mlx5_query_port_mad_ifc(struct ib_device *ibdev, u8 port,
			    struct ib_port_attr *props)
{
	struct mlx5_ib_dev *dev = to_mdev(ibdev);
	struct mlx5_core_dev *mdev = dev->mdev;
	struct ib_smp *in_mad  = NULL;
	struct ib_smp *out_mad = NULL;
	int ext_active_speed;
	int err = -ENOMEM;

	if (port < 1 || port > MLX5_CAP_GEN(mdev, num_ports)) {
		mlx5_ib_warn(dev, "invalid port number %d\n", port);
		return -EINVAL;
	}

	in_mad  = kzalloc(sizeof(*in_mad), GFP_KERNEL);
	out_mad = kmalloc(sizeof(*out_mad), GFP_KERNEL);
	if (!in_mad || !out_mad)
		goto out;

	memset(props, 0, sizeof(*props));

	init_query_mad(in_mad);
	in_mad->attr_id  = IB_SMP_ATTR_PORT_INFO;
	in_mad->attr_mod = cpu_to_be32(port);

	err = mlx5_MAD_IFC(dev, 1, 1, port, NULL, NULL, in_mad, out_mad);
	if (err) {
		mlx5_ib_warn(dev, "err %d\n", err);
		goto out;
	}

	props->lid		= be16_to_cpup((__be16 *)(out_mad->data + 16));
	props->lmc		= out_mad->data[34] & 0x7;
	props->sm_lid		= be16_to_cpup((__be16 *)(out_mad->data + 18));
	props->sm_sl		= out_mad->data[36] & 0xf;
	props->state		= out_mad->data[32] & 0xf;
	props->phys_state	= out_mad->data[33] >> 4;
	props->port_cap_flags	= be32_to_cpup((__be32 *)(out_mad->data + 20));
	props->gid_tbl_len	= out_mad->data[50];
	props->max_msg_sz	= 1 << MLX5_CAP_GEN(mdev, log_max_msg);
	props->pkey_tbl_len	= mdev->port_caps[port - 1].pkey_table_len;
	props->bad_pkey_cntr	= be16_to_cpup((__be16 *)(out_mad->data + 46));
	props->qkey_viol_cntr	= be16_to_cpup((__be16 *)(out_mad->data + 48));
	props->active_width	= out_mad->data[31] & 0xf;
	props->active_speed	= out_mad->data[35] >> 4;
	props->max_mtu		= out_mad->data[41] & 0xf;
	props->active_mtu	= out_mad->data[36] >> 4;
	props->subnet_timeout	= out_mad->data[51] & 0x1f;
	props->max_vl_num	= out_mad->data[37] >> 4;
	props->init_type_reply	= out_mad->data[41] >> 4;

	/* Check if extended speeds (EDR/FDR/...) are supported */
	if (props->port_cap_flags & IB_PORT_EXTENDED_SPEEDS_SUP) {
		ext_active_speed = out_mad->data[62] >> 4;

		switch (ext_active_speed) {
		case 1:
			props->active_speed = 16; /* FDR */
			break;
		case 2:
			props->active_speed = 32; /* EDR */
			break;
		}
	}

	/* If reported active speed is QDR, check if is FDR-10 */
	if (props->active_speed == 4) {
		if (mdev->port_caps[port - 1].ext_port_cap &
		    MLX_EXT_PORT_CAP_FLAG_EXTENDED_PORT_INFO) {
			init_query_mad(in_mad);
			in_mad->attr_id = MLX5_ATTR_EXTENDED_PORT_INFO;
			in_mad->attr_mod = cpu_to_be32(port);

			err = mlx5_MAD_IFC(dev, 1, 1, port,
					   NULL, NULL, in_mad, out_mad);
			if (err)
				goto out;

			/* Checking LinkSpeedActive for FDR-10 */
			if (out_mad->data[15] & 0x1)
				props->active_speed = 8;
		}
	}

out:
	kfree(in_mad);
	kfree(out_mad);

	return err;
}
