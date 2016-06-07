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

#ifndef __MLX5_VPORT_H__
#define __MLX5_VPORT_H__

#include <dev/mlx5/driver.h>
int mlx5_vport_alloc_q_counter(struct mlx5_core_dev *mdev,
			       int *counter_set_id);
int mlx5_vport_dealloc_q_counter(struct mlx5_core_dev *mdev,
				 int counter_set_id);
int mlx5_vport_query_out_of_rx_buffer(struct mlx5_core_dev *mdev,
				      int counter_set_id,
				      u32 *out_of_rx_buffer);

u8 mlx5_query_vport_state(struct mlx5_core_dev *mdev, u8 opmod);
int mlx5_arm_vport_context_events(struct mlx5_core_dev *mdev,
				  u8 vport,
				  u32 events_mask);
int mlx5_query_vport_promisc(struct mlx5_core_dev *mdev,
			     u32 vport,
			     u8 *promisc_uc,
			     u8 *promisc_mc,
			     u8 *promisc_all);
int mlx5_modify_nic_vport_promisc(struct mlx5_core_dev *mdev,
				  int promisc_uc,
				  int promisc_mc,
				  int promisc_all);
int mlx5_query_nic_vport_mac_address(struct mlx5_core_dev *mdev,
				     u32 vport, u8 *addr);
int mlx5_set_nic_vport_current_mac(struct mlx5_core_dev *mdev, int vport,
				   bool other_vport, u8 *addr);
int mlx5_set_nic_vport_vlan_list(struct mlx5_core_dev *dev, u32 vport,
				 u16 *vlan_list, int list_len);
int mlx5_set_nic_vport_mc_list(struct mlx5_core_dev *mdev, int vport,
			       u64 *addr_list, size_t addr_list_len);
int mlx5_set_nic_vport_promisc(struct mlx5_core_dev *mdev, int vport,
			       bool promisc_mc, bool promisc_uc,
			       bool promisc_all);
int mlx5_query_nic_vport_mac_list(struct mlx5_core_dev *dev,
				  u32 vport,
				  enum mlx5_list_type list_type,
				  u8 addr_list[][ETH_ALEN],
				  int *list_size);
int mlx5_modify_nic_vport_mac_list(struct mlx5_core_dev *dev,
				   enum mlx5_list_type list_type,
				   u8 addr_list[][ETH_ALEN],
				   int list_size);
int mlx5_query_nic_vport_vlan_list(struct mlx5_core_dev *dev,
				   u32 vport,
				   u16 *vlan_list,
				   int *list_size);
int mlx5_modify_nic_vport_vlans(struct mlx5_core_dev *dev,
				u16 vlans[],
				int list_size);
int mlx5_set_nic_vport_permanent_mac(struct mlx5_core_dev *mdev, int vport,
				     u8 *addr);
int mlx5_nic_vport_enable_roce(struct mlx5_core_dev *mdev);
int mlx5_nic_vport_disable_roce(struct mlx5_core_dev *mdev);
int mlx5_query_nic_vport_system_image_guid(struct mlx5_core_dev *mdev,
					   u64 *system_image_guid);
int mlx5_query_nic_vport_node_guid(struct mlx5_core_dev *mdev, u64 *node_guid);
int mlx5_query_nic_vport_port_guid(struct mlx5_core_dev *mdev, u64 *port_guid);
int mlx5_query_nic_vport_qkey_viol_cntr(struct mlx5_core_dev *mdev,
					u16 *qkey_viol_cntr);
int mlx5_query_hca_vport_node_guid(struct mlx5_core_dev *mdev, u64 *node_guid);
int mlx5_query_hca_vport_system_image_guid(struct mlx5_core_dev *mdev,
					   u64 *system_image_guid);
int mlx5_query_hca_vport_context(struct mlx5_core_dev *mdev,
				 u8 port_num, u8 vport_num, u32 *out,
				 int outlen);
int mlx5_query_hca_vport_pkey(struct mlx5_core_dev *dev, u8 other_vport,
			      u8 port_num, u16 vf_num, u16 pkey_index,
			      u16 *pkey);
int mlx5_query_hca_vport_gid(struct mlx5_core_dev *dev, u8 port_num,
			     u16 vport_num, u16 gid_index, union ib_gid *gid);
int mlx5_set_eswitch_cvlan_info(struct mlx5_core_dev *mdev, u8 vport,
				u8 insert_mode, u8 strip_mode,
				u16 vlan, u8 cfi, u8 pcp);
int mlx5_query_vport_counter(struct mlx5_core_dev *dev,
			     u8 port_num, u16 vport_num,
			     void *out, int out_size);
int mlx5_get_vport_counters(struct mlx5_core_dev *dev, u8 port_num,
			    struct mlx5_vport_counters *vc);
#endif /* __MLX5_VPORT_H__ */
