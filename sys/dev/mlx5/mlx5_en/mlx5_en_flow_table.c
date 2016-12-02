/*-
 * Copyright (c) 2015 Mellanox Technologies. All rights reserved.
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

#include "en.h"

#include <linux/list.h>
#include <dev/mlx5/flow_table.h>

enum {
	MLX5E_FULLMATCH = 0,
	MLX5E_ALLMULTI = 1,
	MLX5E_PROMISC = 2,
};

enum {
	MLX5E_UC = 0,
	MLX5E_MC_IPV4 = 1,
	MLX5E_MC_IPV6 = 2,
	MLX5E_MC_OTHER = 3,
};

enum {
	MLX5E_ACTION_NONE = 0,
	MLX5E_ACTION_ADD = 1,
	MLX5E_ACTION_DEL = 2,
};

struct mlx5e_eth_addr_hash_node {
	LIST_ENTRY(mlx5e_eth_addr_hash_node) hlist;
	u8	action;
	struct mlx5e_eth_addr_info ai;
};

static inline int
mlx5e_hash_eth_addr(const u8 * addr)
{
	return (addr[5]);
}

static void
mlx5e_add_eth_addr_to_hash(struct mlx5e_eth_addr_hash_head *hash,
    const u8 * addr)
{
	struct mlx5e_eth_addr_hash_node *hn;
	int ix = mlx5e_hash_eth_addr(addr);

	LIST_FOREACH(hn, &hash[ix], hlist) {
		if (bcmp(hn->ai.addr, addr, ETHER_ADDR_LEN) == 0) {
			if (hn->action == MLX5E_ACTION_DEL)
				hn->action = MLX5E_ACTION_NONE;
			return;
		}
	}

	hn = malloc(sizeof(*hn), M_MLX5EN, M_NOWAIT | M_ZERO);
	if (hn == NULL)
		return;

	ether_addr_copy(hn->ai.addr, addr);
	hn->action = MLX5E_ACTION_ADD;

	LIST_INSERT_HEAD(&hash[ix], hn, hlist);
}

static void
mlx5e_del_eth_addr_from_hash(struct mlx5e_eth_addr_hash_node *hn)
{
	LIST_REMOVE(hn, hlist);
	free(hn, M_MLX5EN);
}

static void
mlx5e_del_eth_addr_from_flow_table(struct mlx5e_priv *priv,
    struct mlx5e_eth_addr_info *ai)
{
	void *ft = priv->ft.main;

	if (ai->tt_vec & (1 << MLX5E_TT_IPV6_TCP))
		mlx5_del_flow_table_entry(ft, ai->ft_ix[MLX5E_TT_IPV6_TCP]);

	if (ai->tt_vec & (1 << MLX5E_TT_IPV4_TCP))
		mlx5_del_flow_table_entry(ft, ai->ft_ix[MLX5E_TT_IPV4_TCP]);

	if (ai->tt_vec & (1 << MLX5E_TT_IPV6_UDP))
		mlx5_del_flow_table_entry(ft, ai->ft_ix[MLX5E_TT_IPV6_UDP]);

	if (ai->tt_vec & (1 << MLX5E_TT_IPV4_UDP))
		mlx5_del_flow_table_entry(ft, ai->ft_ix[MLX5E_TT_IPV4_UDP]);

	if (ai->tt_vec & (1 << MLX5E_TT_IPV6))
		mlx5_del_flow_table_entry(ft, ai->ft_ix[MLX5E_TT_IPV6]);

	if (ai->tt_vec & (1 << MLX5E_TT_IPV4))
		mlx5_del_flow_table_entry(ft, ai->ft_ix[MLX5E_TT_IPV4]);

	if (ai->tt_vec & (1 << MLX5E_TT_ANY))
		mlx5_del_flow_table_entry(ft, ai->ft_ix[MLX5E_TT_ANY]);
}

static int
mlx5e_get_eth_addr_type(const u8 * addr)
{
	if (ETHER_IS_MULTICAST(addr) == 0)
		return (MLX5E_UC);

	if ((addr[0] == 0x01) &&
	    (addr[1] == 0x00) &&
	    (addr[2] == 0x5e) &&
	    !(addr[3] & 0x80))
		return (MLX5E_MC_IPV4);

	if ((addr[0] == 0x33) &&
	    (addr[1] == 0x33))
		return (MLX5E_MC_IPV6);

	return (MLX5E_MC_OTHER);
}

static	u32
mlx5e_get_tt_vec(struct mlx5e_eth_addr_info *ai, int type)
{
	int eth_addr_type;
	u32 ret;

	switch (type) {
	case MLX5E_FULLMATCH:
		eth_addr_type = mlx5e_get_eth_addr_type(ai->addr);
		switch (eth_addr_type) {
		case MLX5E_UC:
			ret =
			    (1 << MLX5E_TT_IPV4_TCP) |
			    (1 << MLX5E_TT_IPV6_TCP) |
			    (1 << MLX5E_TT_IPV4_UDP) |
			    (1 << MLX5E_TT_IPV6_UDP) |
			    (1 << MLX5E_TT_IPV4) |
			    (1 << MLX5E_TT_IPV6) |
			    (1 << MLX5E_TT_ANY) |
			    0;
			break;

		case MLX5E_MC_IPV4:
			ret =
			    (1 << MLX5E_TT_IPV4_UDP) |
			    (1 << MLX5E_TT_IPV4) |
			    0;
			break;

		case MLX5E_MC_IPV6:
			ret =
			    (1 << MLX5E_TT_IPV6_UDP) |
			    (1 << MLX5E_TT_IPV6) |
			    0;
			break;

		default:
			ret =
			    (1 << MLX5E_TT_ANY) |
			    0;
			break;
		}
		break;

	case MLX5E_ALLMULTI:
		ret =
		    (1 << MLX5E_TT_IPV4_UDP) |
		    (1 << MLX5E_TT_IPV6_UDP) |
		    (1 << MLX5E_TT_IPV4) |
		    (1 << MLX5E_TT_IPV6) |
		    (1 << MLX5E_TT_ANY) |
		    0;
		break;

	default:			/* MLX5E_PROMISC */
		ret =
		    (1 << MLX5E_TT_IPV4_TCP) |
		    (1 << MLX5E_TT_IPV6_TCP) |
		    (1 << MLX5E_TT_IPV4_UDP) |
		    (1 << MLX5E_TT_IPV6_UDP) |
		    (1 << MLX5E_TT_IPV4) |
		    (1 << MLX5E_TT_IPV6) |
		    (1 << MLX5E_TT_ANY) |
		    0;
		break;
	}

	return (ret);
}

static int
mlx5e_add_eth_addr_rule_sub(struct mlx5e_priv *priv,
    struct mlx5e_eth_addr_info *ai, int type,
    void *flow_context, void *match_criteria)
{
	u8 match_criteria_enable = 0;
	void *match_value;
	void *dest;
	u8 *dmac;
	u8 *match_criteria_dmac;
	void *ft = priv->ft.main;
	u32 *tirn = priv->tirn;
	u32 tt_vec;
	int err;

	match_value = MLX5_ADDR_OF(flow_context, flow_context, match_value);
	dmac = MLX5_ADDR_OF(fte_match_param, match_value,
	    outer_headers.dmac_47_16);
	match_criteria_dmac = MLX5_ADDR_OF(fte_match_param, match_criteria,
	    outer_headers.dmac_47_16);
	dest = MLX5_ADDR_OF(flow_context, flow_context, destination);

	MLX5_SET(flow_context, flow_context, action,
	    MLX5_FLOW_CONTEXT_ACTION_FWD_DEST);
	MLX5_SET(flow_context, flow_context, destination_list_size, 1);
	MLX5_SET(dest_format_struct, dest, destination_type,
	    MLX5_FLOW_CONTEXT_DEST_TYPE_TIR);

	switch (type) {
	case MLX5E_FULLMATCH:
		match_criteria_enable = MLX5_MATCH_OUTER_HEADERS;
		memset(match_criteria_dmac, 0xff, ETH_ALEN);
		ether_addr_copy(dmac, ai->addr);
		break;

	case MLX5E_ALLMULTI:
		match_criteria_enable = MLX5_MATCH_OUTER_HEADERS;
		match_criteria_dmac[0] = 0x01;
		dmac[0] = 0x01;
		break;

	case MLX5E_PROMISC:
		break;
	default:
		break;
	}

	tt_vec = mlx5e_get_tt_vec(ai, type);

	if (tt_vec & (1 << MLX5E_TT_ANY)) {
		MLX5_SET(dest_format_struct, dest, destination_id,
		    tirn[MLX5E_TT_ANY]);
		err = mlx5_add_flow_table_entry(ft, match_criteria_enable,
		    match_criteria, flow_context, &ai->ft_ix[MLX5E_TT_ANY]);
		if (err) {
			mlx5e_del_eth_addr_from_flow_table(priv, ai);
			return (err);
		}
		ai->tt_vec |= (1 << MLX5E_TT_ANY);
	}
	match_criteria_enable = MLX5_MATCH_OUTER_HEADERS;
	MLX5_SET_TO_ONES(fte_match_param, match_criteria,
	    outer_headers.ethertype);

	if (tt_vec & (1 << MLX5E_TT_IPV4)) {
		MLX5_SET(fte_match_param, match_value, outer_headers.ethertype,
		    ETHERTYPE_IP);
		MLX5_SET(dest_format_struct, dest, destination_id,
		    tirn[MLX5E_TT_IPV4]);
		err = mlx5_add_flow_table_entry(ft, match_criteria_enable,
		    match_criteria, flow_context, &ai->ft_ix[MLX5E_TT_IPV4]);
		if (err) {
			mlx5e_del_eth_addr_from_flow_table(priv, ai);
			return (err);
		}
		ai->tt_vec |= (1 << MLX5E_TT_IPV4);
	}
	if (tt_vec & (1 << MLX5E_TT_IPV6)) {
		MLX5_SET(fte_match_param, match_value, outer_headers.ethertype,
		    ETHERTYPE_IPV6);
		MLX5_SET(dest_format_struct, dest, destination_id,
		    tirn[MLX5E_TT_IPV6]);
		err = mlx5_add_flow_table_entry(ft, match_criteria_enable,
		    match_criteria, flow_context, &ai->ft_ix[MLX5E_TT_IPV6]);
		if (err) {
			mlx5e_del_eth_addr_from_flow_table(priv, ai);
			return (err);
		}
		ai->tt_vec |= (1 << MLX5E_TT_IPV6);
	}
	MLX5_SET_TO_ONES(fte_match_param, match_criteria,
	    outer_headers.ip_protocol);
	MLX5_SET(fte_match_param, match_value, outer_headers.ip_protocol,
	    IPPROTO_UDP);

	if (tt_vec & (1 << MLX5E_TT_IPV4_UDP)) {
		MLX5_SET(fte_match_param, match_value, outer_headers.ethertype,
		    ETHERTYPE_IP);
		MLX5_SET(dest_format_struct, dest, destination_id,
		    tirn[MLX5E_TT_IPV4_UDP]);
		err = mlx5_add_flow_table_entry(ft, match_criteria_enable,
		    match_criteria, flow_context, &ai->ft_ix[MLX5E_TT_IPV4_UDP]);
		if (err) {
			mlx5e_del_eth_addr_from_flow_table(priv, ai);
			return (err);
		}
		ai->tt_vec |= (1 << MLX5E_TT_IPV4_UDP);
	}
	if (tt_vec & (1 << MLX5E_TT_IPV6_UDP)) {
		MLX5_SET(fte_match_param, match_value, outer_headers.ethertype,
		    ETHERTYPE_IPV6);
		MLX5_SET(dest_format_struct, dest, destination_id,
		    tirn[MLX5E_TT_IPV6_UDP]);
		err = mlx5_add_flow_table_entry(ft, match_criteria_enable,
		    match_criteria, flow_context, &ai->ft_ix[MLX5E_TT_IPV6_UDP]);
		if (err) {
			mlx5e_del_eth_addr_from_flow_table(priv, ai);
			return (err);
		}
		ai->tt_vec |= (1 << MLX5E_TT_IPV6_UDP);
	}
	MLX5_SET(fte_match_param, match_value, outer_headers.ip_protocol,
	    IPPROTO_TCP);

	if (tt_vec & (1 << MLX5E_TT_IPV4_TCP)) {
		MLX5_SET(fte_match_param, match_value, outer_headers.ethertype,
		    ETHERTYPE_IP);
		MLX5_SET(dest_format_struct, dest, destination_id,
		    tirn[MLX5E_TT_IPV4_TCP]);
		err = mlx5_add_flow_table_entry(ft, match_criteria_enable,
		    match_criteria, flow_context, &ai->ft_ix[MLX5E_TT_IPV4_TCP]);
		if (err) {
			mlx5e_del_eth_addr_from_flow_table(priv, ai);
			return (err);
		}
		ai->tt_vec |= (1 << MLX5E_TT_IPV4_TCP);
	}
	if (tt_vec & (1 << MLX5E_TT_IPV6_TCP)) {
		MLX5_SET(fte_match_param, match_value, outer_headers.ethertype,
		    ETHERTYPE_IPV6);
		MLX5_SET(dest_format_struct, dest, destination_id,
		    tirn[MLX5E_TT_IPV6_TCP]);
		err = mlx5_add_flow_table_entry(ft, match_criteria_enable,
		    match_criteria, flow_context, &ai->ft_ix[MLX5E_TT_IPV6_TCP]);
		if (err) {
			mlx5e_del_eth_addr_from_flow_table(priv, ai);
			return (err);
		}
		ai->tt_vec |= (1 << MLX5E_TT_IPV6_TCP);
	}
	return (0);
}

static int
mlx5e_add_eth_addr_rule(struct mlx5e_priv *priv,
    struct mlx5e_eth_addr_info *ai, int type)
{
	u32 *flow_context;
	u32 *match_criteria;
	int err;

	flow_context = mlx5_vzalloc(MLX5_ST_SZ_BYTES(flow_context) +
	    MLX5_ST_SZ_BYTES(dest_format_struct));
	match_criteria = mlx5_vzalloc(MLX5_ST_SZ_BYTES(fte_match_param));
	if (!flow_context || !match_criteria) {
		if_printf(priv->ifp, "%s: alloc failed\n", __func__);
		err = -ENOMEM;
		goto add_eth_addr_rule_out;
	}
	err = mlx5e_add_eth_addr_rule_sub(priv, ai, type, flow_context,
	    match_criteria);
	if (err)
		if_printf(priv->ifp, "%s: failed\n", __func__);

add_eth_addr_rule_out:
	kvfree(match_criteria);
	kvfree(flow_context);
	return (err);
}

static int mlx5e_vport_context_update_vlans(struct mlx5e_priv *priv)
{
	struct ifnet *ifp = priv->ifp;
	int max_list_size;
	int list_size;
	u16 *vlans;
	int vlan;
	int err;
	int i;

	list_size = 0;
	for_each_set_bit(vlan, priv->vlan.active_vlans, VLAN_N_VID)
		list_size++;

	max_list_size = 1 << MLX5_CAP_GEN(priv->mdev, log_max_vlan_list);

	if (list_size > max_list_size) {
		if_printf(ifp,
			    "ifnet vlans list size (%d) > (%d) max vport list size, some vlans will be dropped\n",
			    list_size, max_list_size);
		list_size = max_list_size;
	}

	vlans = kcalloc(list_size, sizeof(*vlans), GFP_KERNEL);
	if (!vlans)
		return -ENOMEM;

	i = 0;
	for_each_set_bit(vlan, priv->vlan.active_vlans, VLAN_N_VID) {
		if (i >= list_size)
			break;
		vlans[i++] = vlan;
	}

	err = mlx5_modify_nic_vport_vlans(priv->mdev, vlans, list_size);
	if (err)
		if_printf(ifp, "Failed to modify vport vlans list err(%d)\n",
			   err);

	kfree(vlans);
	return err;
}

enum mlx5e_vlan_rule_type {
	MLX5E_VLAN_RULE_TYPE_UNTAGGED,
	MLX5E_VLAN_RULE_TYPE_ANY_VID,
	MLX5E_VLAN_RULE_TYPE_MATCH_VID,
};

static int
mlx5e_add_vlan_rule(struct mlx5e_priv *priv,
    enum mlx5e_vlan_rule_type rule_type, u16 vid)
{
	u8 match_criteria_enable = 0;
	u32 *flow_context;
	void *match_value;
	void *dest;
	u32 *match_criteria;
	u32 *ft_ix;
	int err;

	flow_context = mlx5_vzalloc(MLX5_ST_SZ_BYTES(flow_context) +
	    MLX5_ST_SZ_BYTES(dest_format_struct));
	match_criteria = mlx5_vzalloc(MLX5_ST_SZ_BYTES(fte_match_param));
	if (!flow_context || !match_criteria) {
		if_printf(priv->ifp, "%s: alloc failed\n", __func__);
		err = -ENOMEM;
		goto add_vlan_rule_out;
	}
	match_value = MLX5_ADDR_OF(flow_context, flow_context, match_value);
	dest = MLX5_ADDR_OF(flow_context, flow_context, destination);

	MLX5_SET(flow_context, flow_context, action,
	    MLX5_FLOW_CONTEXT_ACTION_FWD_DEST);
	MLX5_SET(flow_context, flow_context, destination_list_size, 1);
	MLX5_SET(dest_format_struct, dest, destination_type,
	    MLX5_FLOW_CONTEXT_DEST_TYPE_FLOW_TABLE);
	MLX5_SET(dest_format_struct, dest, destination_id,
	    mlx5_get_flow_table_id(priv->ft.main));

	match_criteria_enable = MLX5_MATCH_OUTER_HEADERS;
	MLX5_SET_TO_ONES(fte_match_param, match_criteria,
	    outer_headers.cvlan_tag);

	switch (rule_type) {
	case MLX5E_VLAN_RULE_TYPE_UNTAGGED:
		ft_ix = &priv->vlan.untagged_rule_ft_ix;
		break;
	case MLX5E_VLAN_RULE_TYPE_ANY_VID:
		ft_ix = &priv->vlan.any_vlan_rule_ft_ix;
		MLX5_SET(fte_match_param, match_value, outer_headers.cvlan_tag,
		    1);
		break;
	default:			/* MLX5E_VLAN_RULE_TYPE_MATCH_VID */
		ft_ix = &priv->vlan.active_vlans_ft_ix[vid];
		MLX5_SET(fte_match_param, match_value, outer_headers.cvlan_tag,
		    1);
		MLX5_SET_TO_ONES(fte_match_param, match_criteria,
		    outer_headers.first_vid);
		MLX5_SET(fte_match_param, match_value, outer_headers.first_vid,
		    vid);
		mlx5e_vport_context_update_vlans(priv);
		break;
	}

	err = mlx5_add_flow_table_entry(priv->ft.vlan, match_criteria_enable,
	    match_criteria, flow_context, ft_ix);
	if (err)
		if_printf(priv->ifp, "%s: failed\n", __func__);

add_vlan_rule_out:
	kvfree(match_criteria);
	kvfree(flow_context);
	return (err);
}

static void
mlx5e_del_vlan_rule(struct mlx5e_priv *priv,
    enum mlx5e_vlan_rule_type rule_type, u16 vid)
{
	switch (rule_type) {
	case MLX5E_VLAN_RULE_TYPE_UNTAGGED:
		mlx5_del_flow_table_entry(priv->ft.vlan,
		    priv->vlan.untagged_rule_ft_ix);
		break;
	case MLX5E_VLAN_RULE_TYPE_ANY_VID:
		mlx5_del_flow_table_entry(priv->ft.vlan,
		    priv->vlan.any_vlan_rule_ft_ix);
		break;
	case MLX5E_VLAN_RULE_TYPE_MATCH_VID:
		mlx5_del_flow_table_entry(priv->ft.vlan,
		    priv->vlan.active_vlans_ft_ix[vid]);
		mlx5e_vport_context_update_vlans(priv);
		break;
	}
}

void
mlx5e_enable_vlan_filter(struct mlx5e_priv *priv)
{
	if (priv->vlan.filter_disabled) {
		priv->vlan.filter_disabled = false;
		if (test_bit(MLX5E_STATE_OPENED, &priv->state))
			mlx5e_del_vlan_rule(priv, MLX5E_VLAN_RULE_TYPE_ANY_VID,
			    0);
	}
}

void
mlx5e_disable_vlan_filter(struct mlx5e_priv *priv)
{
	if (!priv->vlan.filter_disabled) {
		priv->vlan.filter_disabled = true;
		if (test_bit(MLX5E_STATE_OPENED, &priv->state))
			mlx5e_add_vlan_rule(priv, MLX5E_VLAN_RULE_TYPE_ANY_VID,
			    0);
	}
}

void
mlx5e_vlan_rx_add_vid(void *arg, struct ifnet *ifp, u16 vid)
{
	struct mlx5e_priv *priv = arg;

	if (ifp != priv->ifp)
		return;

	PRIV_LOCK(priv);
	set_bit(vid, priv->vlan.active_vlans);
	if (test_bit(MLX5E_STATE_OPENED, &priv->state))
		mlx5e_add_vlan_rule(priv, MLX5E_VLAN_RULE_TYPE_MATCH_VID, vid);
	PRIV_UNLOCK(priv);
}

void
mlx5e_vlan_rx_kill_vid(void *arg, struct ifnet *ifp, u16 vid)
{
	struct mlx5e_priv *priv = arg;

	if (ifp != priv->ifp)
		return;

	PRIV_LOCK(priv);
	clear_bit(vid, priv->vlan.active_vlans);
	if (test_bit(MLX5E_STATE_OPENED, &priv->state))
		mlx5e_del_vlan_rule(priv, MLX5E_VLAN_RULE_TYPE_MATCH_VID, vid);
	PRIV_UNLOCK(priv);
}

int
mlx5e_add_all_vlan_rules(struct mlx5e_priv *priv)
{
	u16 vid;
	int err;

	for_each_set_bit(vid, priv->vlan.active_vlans, VLAN_N_VID) {
		err = mlx5e_add_vlan_rule(priv, MLX5E_VLAN_RULE_TYPE_MATCH_VID,
		    vid);
		if (err)
			return (err);
	}

	err = mlx5e_add_vlan_rule(priv, MLX5E_VLAN_RULE_TYPE_UNTAGGED, 0);
	if (err)
		return (err);

	if (priv->vlan.filter_disabled) {
		err = mlx5e_add_vlan_rule(priv, MLX5E_VLAN_RULE_TYPE_ANY_VID,
		    0);
		if (err)
			return (err);
	}
	return (0);
}

void
mlx5e_del_all_vlan_rules(struct mlx5e_priv *priv)
{
	u16 vid;

	if (priv->vlan.filter_disabled)
		mlx5e_del_vlan_rule(priv, MLX5E_VLAN_RULE_TYPE_ANY_VID, 0);

	mlx5e_del_vlan_rule(priv, MLX5E_VLAN_RULE_TYPE_UNTAGGED, 0);

	for_each_set_bit(vid, priv->vlan.active_vlans, VLAN_N_VID)
	    mlx5e_del_vlan_rule(priv, MLX5E_VLAN_RULE_TYPE_MATCH_VID, vid);
}

#define	mlx5e_for_each_hash_node(hn, tmp, hash, i) \
	for (i = 0; i < MLX5E_ETH_ADDR_HASH_SIZE; i++) \
		LIST_FOREACH_SAFE(hn, &(hash)[i], hlist, tmp)

static void
mlx5e_execute_action(struct mlx5e_priv *priv,
    struct mlx5e_eth_addr_hash_node *hn)
{
	switch (hn->action) {
	case MLX5E_ACTION_ADD:
		mlx5e_add_eth_addr_rule(priv, &hn->ai, MLX5E_FULLMATCH);
		hn->action = MLX5E_ACTION_NONE;
		break;

	case MLX5E_ACTION_DEL:
		mlx5e_del_eth_addr_from_flow_table(priv, &hn->ai);
		mlx5e_del_eth_addr_from_hash(hn);
		break;

	default:
		break;
	}
}

static void
mlx5e_sync_ifp_addr(struct mlx5e_priv *priv)
{
	struct ifnet *ifp = priv->ifp;
	struct ifaddr *ifa;
	struct ifmultiaddr *ifma;

	/* XXX adding this entry might not be needed */
	mlx5e_add_eth_addr_to_hash(priv->eth_addr.if_uc,
	    LLADDR((struct sockaddr_dl *)(ifp->if_addr->ifa_addr)));

	if_addr_rlock(ifp);
	TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
		if (ifa->ifa_addr->sa_family != AF_LINK)
			continue;
		mlx5e_add_eth_addr_to_hash(priv->eth_addr.if_uc,
		    LLADDR((struct sockaddr_dl *)ifa->ifa_addr));
	}
	if_addr_runlock(ifp);

	if_maddr_rlock(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		mlx5e_add_eth_addr_to_hash(priv->eth_addr.if_mc,
		    LLADDR((struct sockaddr_dl *)ifma->ifma_addr));
	}
	if_maddr_runlock(ifp);
}

static void mlx5e_fill_addr_array(struct mlx5e_priv *priv, int list_type,
				  u8 addr_array[][ETH_ALEN], int size)
{
	bool is_uc = (list_type == MLX5_NIC_VPORT_LIST_TYPE_UC);
	struct ifnet *ifp = priv->ifp;
	struct mlx5e_eth_addr_hash_node *hn;
	struct mlx5e_eth_addr_hash_head *addr_list;
	struct mlx5e_eth_addr_hash_node *tmp;
	int i = 0;
	int hi;

	addr_list = is_uc ? priv->eth_addr.if_uc : priv->eth_addr.if_mc;

	if (is_uc) /* Make sure our own address is pushed first */
		ether_addr_copy(addr_array[i++], IF_LLADDR(ifp));
	else if (priv->eth_addr.broadcast_enabled)
		ether_addr_copy(addr_array[i++], ifp->if_broadcastaddr);

	mlx5e_for_each_hash_node(hn, tmp, addr_list, hi) {
		if (ether_addr_equal(IF_LLADDR(ifp), hn->ai.addr))
			continue;
		if (i >= size)
			break;
		ether_addr_copy(addr_array[i++], hn->ai.addr);
	}
}

static void mlx5e_vport_context_update_addr_list(struct mlx5e_priv *priv,
						 int list_type)
{
	bool is_uc = (list_type == MLX5_NIC_VPORT_LIST_TYPE_UC);
	struct mlx5e_eth_addr_hash_node *hn;
	u8 (*addr_array)[ETH_ALEN] = NULL;
	struct mlx5e_eth_addr_hash_head *addr_list;
	struct mlx5e_eth_addr_hash_node *tmp;
	int max_size;
	int size;
	int err;
	int hi;

	size = is_uc ? 0 : (priv->eth_addr.broadcast_enabled ? 1 : 0);
	max_size = is_uc ?
		1 << MLX5_CAP_GEN(priv->mdev, log_max_current_uc_list) :
		1 << MLX5_CAP_GEN(priv->mdev, log_max_current_mc_list);

	addr_list = is_uc ? priv->eth_addr.if_uc : priv->eth_addr.if_mc;
	mlx5e_for_each_hash_node(hn, tmp, addr_list, hi)
		size++;

	if (size > max_size) {
		if_printf(priv->ifp,
			    "ifp %s list size (%d) > (%d) max vport list size, some addresses will be dropped\n",
			    is_uc ? "UC" : "MC", size, max_size);
		size = max_size;
	}

	if (size) {
		addr_array = kcalloc(size, ETH_ALEN, GFP_KERNEL);
		if (!addr_array) {
			err = -ENOMEM;
			goto out;
		}
		mlx5e_fill_addr_array(priv, list_type, addr_array, size);
	}

	err = mlx5_modify_nic_vport_mac_list(priv->mdev, list_type, addr_array, size);
out:
	if (err)
		if_printf(priv->ifp,
			   "Failed to modify vport %s list err(%d)\n",
			   is_uc ? "UC" : "MC", err);
	kfree(addr_array);
}

static void mlx5e_vport_context_update(struct mlx5e_priv *priv)
{
	struct mlx5e_eth_addr_db *ea = &priv->eth_addr;

	mlx5e_vport_context_update_addr_list(priv, MLX5_NIC_VPORT_LIST_TYPE_UC);
	mlx5e_vport_context_update_addr_list(priv, MLX5_NIC_VPORT_LIST_TYPE_MC);
	mlx5_modify_nic_vport_promisc(priv->mdev, 0,
				      ea->allmulti_enabled,
				      ea->promisc_enabled);
}

static void
mlx5e_apply_ifp_addr(struct mlx5e_priv *priv)
{
	struct mlx5e_eth_addr_hash_node *hn;
	struct mlx5e_eth_addr_hash_node *tmp;
	int i;

	mlx5e_for_each_hash_node(hn, tmp, priv->eth_addr.if_uc, i)
	    mlx5e_execute_action(priv, hn);

	mlx5e_for_each_hash_node(hn, tmp, priv->eth_addr.if_mc, i)
	    mlx5e_execute_action(priv, hn);
}

static void
mlx5e_handle_ifp_addr(struct mlx5e_priv *priv)
{
	struct mlx5e_eth_addr_hash_node *hn;
	struct mlx5e_eth_addr_hash_node *tmp;
	int i;

	mlx5e_for_each_hash_node(hn, tmp, priv->eth_addr.if_uc, i)
	    hn->action = MLX5E_ACTION_DEL;
	mlx5e_for_each_hash_node(hn, tmp, priv->eth_addr.if_mc, i)
	    hn->action = MLX5E_ACTION_DEL;

	if (test_bit(MLX5E_STATE_OPENED, &priv->state))
		mlx5e_sync_ifp_addr(priv);

	mlx5e_apply_ifp_addr(priv);
}

void
mlx5e_set_rx_mode_core(struct mlx5e_priv *priv)
{
	struct mlx5e_eth_addr_db *ea = &priv->eth_addr;
	struct ifnet *ndev = priv->ifp;

	bool rx_mode_enable = test_bit(MLX5E_STATE_OPENED, &priv->state);
	bool promisc_enabled = rx_mode_enable && (ndev->if_flags & IFF_PROMISC);
	bool allmulti_enabled = rx_mode_enable && (ndev->if_flags & IFF_ALLMULTI);
	bool broadcast_enabled = rx_mode_enable;

	bool enable_promisc = !ea->promisc_enabled && promisc_enabled;
	bool disable_promisc = ea->promisc_enabled && !promisc_enabled;
	bool enable_allmulti = !ea->allmulti_enabled && allmulti_enabled;
	bool disable_allmulti = ea->allmulti_enabled && !allmulti_enabled;
	bool enable_broadcast = !ea->broadcast_enabled && broadcast_enabled;
	bool disable_broadcast = ea->broadcast_enabled && !broadcast_enabled;

	/* update broadcast address */
	ether_addr_copy(priv->eth_addr.broadcast.addr,
	    priv->ifp->if_broadcastaddr);

	if (enable_promisc)
		mlx5e_add_eth_addr_rule(priv, &ea->promisc, MLX5E_PROMISC);
	if (enable_allmulti)
		mlx5e_add_eth_addr_rule(priv, &ea->allmulti, MLX5E_ALLMULTI);
	if (enable_broadcast)
		mlx5e_add_eth_addr_rule(priv, &ea->broadcast, MLX5E_FULLMATCH);

	mlx5e_handle_ifp_addr(priv);

	if (disable_broadcast)
		mlx5e_del_eth_addr_from_flow_table(priv, &ea->broadcast);
	if (disable_allmulti)
		mlx5e_del_eth_addr_from_flow_table(priv, &ea->allmulti);
	if (disable_promisc)
		mlx5e_del_eth_addr_from_flow_table(priv, &ea->promisc);

	ea->promisc_enabled = promisc_enabled;
	ea->allmulti_enabled = allmulti_enabled;
	ea->broadcast_enabled = broadcast_enabled;

	mlx5e_vport_context_update(priv);
}

void
mlx5e_set_rx_mode_work(struct work_struct *work)
{
	struct mlx5e_priv *priv =
	    container_of(work, struct mlx5e_priv, set_rx_mode_work);

	PRIV_LOCK(priv);
	if (test_bit(MLX5E_STATE_OPENED, &priv->state))
		mlx5e_set_rx_mode_core(priv);
	PRIV_UNLOCK(priv);
}

static int
mlx5e_create_main_flow_table(struct mlx5e_priv *priv)
{
	struct mlx5_flow_table_group *g;
	u8 *dmac;

	g = malloc(9 * sizeof(*g), M_MLX5EN, M_WAITOK | M_ZERO);

	g[0].log_sz = 2;
	g[0].match_criteria_enable = MLX5_MATCH_OUTER_HEADERS;
	MLX5_SET_TO_ONES(fte_match_param, g[0].match_criteria,
	    outer_headers.ethertype);
	MLX5_SET_TO_ONES(fte_match_param, g[0].match_criteria,
	    outer_headers.ip_protocol);

	g[1].log_sz = 1;
	g[1].match_criteria_enable = MLX5_MATCH_OUTER_HEADERS;
	MLX5_SET_TO_ONES(fte_match_param, g[1].match_criteria,
	    outer_headers.ethertype);

	g[2].log_sz = 0;

	g[3].log_sz = 14;
	g[3].match_criteria_enable = MLX5_MATCH_OUTER_HEADERS;
	dmac = MLX5_ADDR_OF(fte_match_param, g[3].match_criteria,
	    outer_headers.dmac_47_16);
	memset(dmac, 0xff, ETH_ALEN);
	MLX5_SET_TO_ONES(fte_match_param, g[3].match_criteria,
	    outer_headers.ethertype);
	MLX5_SET_TO_ONES(fte_match_param, g[3].match_criteria,
	    outer_headers.ip_protocol);

	g[4].log_sz = 13;
	g[4].match_criteria_enable = MLX5_MATCH_OUTER_HEADERS;
	dmac = MLX5_ADDR_OF(fte_match_param, g[4].match_criteria,
	    outer_headers.dmac_47_16);
	memset(dmac, 0xff, ETH_ALEN);
	MLX5_SET_TO_ONES(fte_match_param, g[4].match_criteria,
	    outer_headers.ethertype);

	g[5].log_sz = 11;
	g[5].match_criteria_enable = MLX5_MATCH_OUTER_HEADERS;
	dmac = MLX5_ADDR_OF(fte_match_param, g[5].match_criteria,
	    outer_headers.dmac_47_16);
	memset(dmac, 0xff, ETH_ALEN);

	g[6].log_sz = 2;
	g[6].match_criteria_enable = MLX5_MATCH_OUTER_HEADERS;
	dmac = MLX5_ADDR_OF(fte_match_param, g[6].match_criteria,
	    outer_headers.dmac_47_16);
	dmac[0] = 0x01;
	MLX5_SET_TO_ONES(fte_match_param, g[6].match_criteria,
	    outer_headers.ethertype);
	MLX5_SET_TO_ONES(fte_match_param, g[6].match_criteria,
	    outer_headers.ip_protocol);

	g[7].log_sz = 1;
	g[7].match_criteria_enable = MLX5_MATCH_OUTER_HEADERS;
	dmac = MLX5_ADDR_OF(fte_match_param, g[7].match_criteria,
	    outer_headers.dmac_47_16);
	dmac[0] = 0x01;
	MLX5_SET_TO_ONES(fte_match_param, g[7].match_criteria,
	    outer_headers.ethertype);

	g[8].log_sz = 0;
	g[8].match_criteria_enable = MLX5_MATCH_OUTER_HEADERS;
	dmac = MLX5_ADDR_OF(fte_match_param, g[8].match_criteria,
	    outer_headers.dmac_47_16);
	dmac[0] = 0x01;
	priv->ft.main = mlx5_create_flow_table(priv->mdev, 1,
	    MLX5_FLOW_TABLE_TYPE_NIC_RCV,
	    0, 9, g);
	free(g, M_MLX5EN);

	return (priv->ft.main ? 0 : -ENOMEM);
}

static void
mlx5e_destroy_main_flow_table(struct mlx5e_priv *priv)
{
	mlx5_destroy_flow_table(priv->ft.main);
	priv->ft.main = NULL;
}

static int
mlx5e_create_vlan_flow_table(struct mlx5e_priv *priv)
{
	struct mlx5_flow_table_group *g;

	g = malloc(2 * sizeof(*g), M_MLX5EN, M_WAITOK | M_ZERO);

	g[0].log_sz = 12;
	g[0].match_criteria_enable = MLX5_MATCH_OUTER_HEADERS;
	MLX5_SET_TO_ONES(fte_match_param, g[0].match_criteria,
	    outer_headers.cvlan_tag);
	MLX5_SET_TO_ONES(fte_match_param, g[0].match_criteria,
	    outer_headers.first_vid);

	/* untagged + any vlan id */
	g[1].log_sz = 1;
	g[1].match_criteria_enable = MLX5_MATCH_OUTER_HEADERS;
	MLX5_SET_TO_ONES(fte_match_param, g[1].match_criteria,
	    outer_headers.cvlan_tag);

	priv->ft.vlan = mlx5_create_flow_table(priv->mdev, 0,
	    MLX5_FLOW_TABLE_TYPE_NIC_RCV,
	    0, 2, g);
	free(g, M_MLX5EN);

	return (priv->ft.vlan ? 0 : -ENOMEM);
}

static void
mlx5e_destroy_vlan_flow_table(struct mlx5e_priv *priv)
{
	mlx5_destroy_flow_table(priv->ft.vlan);
	priv->ft.vlan = NULL;
}

int
mlx5e_open_flow_table(struct mlx5e_priv *priv)
{
	int err;

	err = mlx5e_create_main_flow_table(priv);
	if (err)
		return (err);

	err = mlx5e_create_vlan_flow_table(priv);
	if (err)
		goto err_destroy_main_flow_table;

	return (0);

err_destroy_main_flow_table:
	mlx5e_destroy_main_flow_table(priv);

	return (err);
}

void
mlx5e_close_flow_table(struct mlx5e_priv *priv)
{
	mlx5e_destroy_vlan_flow_table(priv);
	mlx5e_destroy_main_flow_table(priv);
}
