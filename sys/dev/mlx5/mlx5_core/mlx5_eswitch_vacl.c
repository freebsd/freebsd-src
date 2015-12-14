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

#include <linux/if_ether.h>
#include <linux/etherdevice.h>
#include <dev/mlx5/driver.h>
#include <dev/mlx5/flow_table.h>
#include <dev/mlx5/eswitch_vacl.h>
#include "mlx5_core.h"

enum {
	MLX5_ACL_LOOPBACK_GROUP_IDX	= 0,
	MLX5_ACL_UNTAGGED_GROUP_IDX	= 1,
	MLX5_ACL_VLAN_GROUP_IDX		= 2,
	MLX5_ACL_UNKNOWN_VLAN_GROUP_IDX	= 3,
	MLX5_ACL_DEFAULT_GROUP_IDX	= 4,
	MLX5_ACL_GROUPS_NUM,
};

struct mlx_vacl_fr {
	bool			applied;
	u32			fi;
	u16			action;
};

struct mlx5_vacl_table {
	struct mlx5_core_dev	*dev;
	u16			vport;
	void			*ft;
	int			max_ft_size;
	int			acl_type;

	struct mlx_vacl_fr	loopback_fr;
	struct mlx_vacl_fr	untagged_fr;
	struct mlx_vacl_fr	unknown_vlan_fr;
	struct mlx_vacl_fr	default_fr;

	bool			vlan_filter_enabled;
	bool			vlan_filter_applied;
	unsigned long		*vlan_allowed_bitmap;
	u32			vlan_fi_table[4096];

	bool			spoofchk_enabled;
	u8			smac[ETH_ALEN];
};

static int mlx5_vacl_table_allow_vlan(void *acl_t, u16 vlan)
{
	struct mlx5_vacl_table *acl_table = (struct mlx5_vacl_table *)acl_t;
	u32 *flow_context = NULL;
	void *in_match_criteria = NULL;
	void *in_match_value = NULL;
	u8 *smac;
	int vlan_mc_enable = MLX5_MATCH_OUTER_HEADERS;
	int err = 0;

	if (!test_bit(vlan, acl_table->vlan_allowed_bitmap))
		return -EINVAL;

	flow_context = mlx5_vzalloc(MLX5_ST_SZ_BYTES(flow_context));
	if (!flow_context) {
		err = -ENOMEM;
		goto out;
	}

	in_match_criteria = mlx5_vzalloc(MLX5_ST_SZ_BYTES(fte_match_param));
	if (!in_match_criteria) {
		err = -ENOMEM;
		goto out;
	}

	/* Apply vlan rule */
	MLX5_SET(flow_context, flow_context, action,
		 MLX5_FLOW_CONTEXT_ACTION_ALLOW);
	in_match_value = MLX5_ADDR_OF(flow_context, flow_context, match_value);
	MLX5_SET(fte_match_param, in_match_value, outer_headers.vlan_tag, 1);
	MLX5_SET(fte_match_param, in_match_value, outer_headers.first_vid,
		 vlan);
	MLX5_SET(fte_match_param, in_match_criteria, outer_headers.vlan_tag, 1);
	MLX5_SET(fte_match_param, in_match_criteria, outer_headers.first_vid,
		 0xfff);
	if (acl_table->spoofchk_enabled) {
		smac = MLX5_ADDR_OF(fte_match_param,
				    in_match_value,
				    outer_headers.smac_47_16);
		ether_addr_copy(smac, acl_table->smac);
		smac = MLX5_ADDR_OF(fte_match_param,
				    in_match_criteria,
				    outer_headers.smac_47_16);
		memset(smac, 0xff, ETH_ALEN);
	}
	err = mlx5_add_flow_table_entry(acl_table->ft, vlan_mc_enable,
					in_match_criteria, flow_context,
					&acl_table->vlan_fi_table[vlan]);
out:
	if (flow_context)
		vfree(flow_context);
	if (in_match_criteria)
		vfree(in_match_criteria);
	return err;
}

static int mlx5_vacl_table_apply_loopback_filter(void *acl_t, u16 new_action)
{
	struct mlx5_vacl_table *acl_table = (struct mlx5_vacl_table *)acl_t;
	u8 loopback_mc_enable = MLX5_MATCH_MISC_PARAMETERS;
	u32 *flow_context = NULL;
	void *in_match_criteria = NULL;
	void *in_match_value = NULL;
	void *mv_misc = NULL;
	void *mc_misc = NULL;
	int err = 0;

	flow_context = mlx5_vzalloc(MLX5_ST_SZ_BYTES(flow_context));
	if (!flow_context) {
		err = -ENOMEM;
		goto out;
	}

	in_match_criteria = mlx5_vzalloc(MLX5_ST_SZ_BYTES(fte_match_param));
	if (!in_match_criteria) {
		err = -ENOMEM;
		goto out;
	}

	if (acl_table->loopback_fr.applied)
		mlx5_del_flow_table_entry(acl_table->ft,
					  acl_table->loopback_fr.fi);

	/* Apply new loopback rule */
	MLX5_SET(flow_context, flow_context, action, new_action);
	in_match_value = MLX5_ADDR_OF(flow_context, flow_context, match_value);
	mv_misc  = MLX5_ADDR_OF(fte_match_param, in_match_value,
				misc_parameters);
	mc_misc  = MLX5_ADDR_OF(fte_match_param, in_match_criteria,
				misc_parameters);
	MLX5_SET(fte_match_set_misc, mv_misc, source_port, acl_table->vport);

	MLX5_SET_TO_ONES(fte_match_set_misc, mc_misc, source_port);

	err = mlx5_add_flow_table_entry(acl_table->ft, loopback_mc_enable,
					in_match_criteria, flow_context,
					&acl_table->loopback_fr.fi);
	if (err) {
		acl_table->loopback_fr.applied = false;
	} else {
		acl_table->loopback_fr.applied = true;
		acl_table->loopback_fr.action  = new_action;
	}

out:
	if (flow_context)
		vfree(flow_context);
	if (in_match_criteria)
		vfree(in_match_criteria);
	return err;
}

static int mlx5_vacl_table_apply_default(void *acl_t, u16 new_action)
{
	struct mlx5_vacl_table *acl_table = (struct mlx5_vacl_table *)acl_t;
	u8 default_mc_enable = 0;
	u32 *flow_context = NULL;
	void *in_match_criteria = NULL;
	int err = 0;

	if (!acl_table->spoofchk_enabled)
		return -EINVAL;

	flow_context = mlx5_vzalloc(MLX5_ST_SZ_BYTES(flow_context));
	if (!flow_context) {
		err = -ENOMEM;
		goto out;
	}

	in_match_criteria = mlx5_vzalloc(MLX5_ST_SZ_BYTES(fte_match_param));
	if (!in_match_criteria) {
		err = -ENOMEM;
		goto out;
	}

	if (acl_table->default_fr.applied)
		mlx5_del_flow_table_entry(acl_table->ft,
					  acl_table->default_fr.fi);

	/* Apply new default rule */
	MLX5_SET(flow_context, flow_context, action, new_action);
	err = mlx5_add_flow_table_entry(acl_table->ft, default_mc_enable,
					in_match_criteria, flow_context,
					&acl_table->default_fr.fi);
	if (err) {
		acl_table->default_fr.applied = false;
	} else {
		acl_table->default_fr.applied = true;
		acl_table->default_fr.action  = new_action;
	}

out:
	if (flow_context)
		vfree(flow_context);
	if (in_match_criteria)
		vfree(in_match_criteria);
	return err;
}

static int mlx5_vacl_table_apply_untagged(void *acl_t, u16 new_action)
{
	struct mlx5_vacl_table *acl_table = (struct mlx5_vacl_table *)acl_t;
	u8 untagged_mc_enable = MLX5_MATCH_OUTER_HEADERS;
	u8 *smac;
	u32 *flow_context = NULL;
	void *in_match_criteria = NULL;
	void *in_match_value = NULL;
	int err = 0;

	flow_context = mlx5_vzalloc(MLX5_ST_SZ_BYTES(flow_context));
	if (!flow_context) {
		err = -ENOMEM;
		goto out;
	}

	in_match_criteria = mlx5_vzalloc(MLX5_ST_SZ_BYTES(fte_match_param));
	if (!in_match_criteria) {
		err = -ENOMEM;
		goto out;
	}

	if (acl_table->untagged_fr.applied)
		mlx5_del_flow_table_entry(acl_table->ft,
					  acl_table->untagged_fr.fi);

	/* Apply new untagged rule */
	MLX5_SET(flow_context, flow_context, action, new_action);
	in_match_value = MLX5_ADDR_OF(flow_context, flow_context, match_value);
	MLX5_SET(fte_match_param, in_match_value, outer_headers.vlan_tag, 0);
	MLX5_SET(fte_match_param, in_match_criteria, outer_headers.vlan_tag, 1);
	if (acl_table->spoofchk_enabled) {
		smac = MLX5_ADDR_OF(fte_match_param,
				    in_match_value,
				    outer_headers.smac_47_16);
		ether_addr_copy(smac, acl_table->smac);
		smac = MLX5_ADDR_OF(fte_match_param,
				    in_match_criteria,
				    outer_headers.smac_47_16);
		memset(smac, 0xff, ETH_ALEN);
	}
	err = mlx5_add_flow_table_entry(acl_table->ft, untagged_mc_enable,
					in_match_criteria, flow_context,
					&acl_table->untagged_fr.fi);
	if (err) {
		acl_table->untagged_fr.applied = false;
	} else {
		acl_table->untagged_fr.applied = true;
		acl_table->untagged_fr.action  = new_action;
	}

out:
	if (flow_context)
		vfree(flow_context);
	if (in_match_criteria)
		vfree(in_match_criteria);
	return err;
}

static int mlx5_vacl_table_apply_unknown_vlan(void *acl_t, u16 new_action)
{
	struct mlx5_vacl_table *acl_table = (struct mlx5_vacl_table *)acl_t;
	u8 default_mc_enable = (!acl_table->spoofchk_enabled) ? 0 :
				MLX5_MATCH_OUTER_HEADERS;
	u32 *flow_context = NULL;
	void *in_match_criteria = NULL;
	void *in_match_value = NULL;
	u8 *smac;
	int err = 0;

	flow_context = mlx5_vzalloc(MLX5_ST_SZ_BYTES(flow_context));
	if (!flow_context) {
		err = -ENOMEM;
		goto out;
	}

	in_match_criteria = mlx5_vzalloc(MLX5_ST_SZ_BYTES(fte_match_param));
	if (!in_match_criteria) {
		err = -ENOMEM;
		goto out;
	}

	if (acl_table->unknown_vlan_fr.applied)
		mlx5_del_flow_table_entry(acl_table->ft,
					  acl_table->unknown_vlan_fr.fi);

	/* Apply new unknown vlan rule */
	MLX5_SET(flow_context, flow_context, action, new_action);
	if (acl_table->spoofchk_enabled) {
		in_match_value = MLX5_ADDR_OF(flow_context, flow_context,
					      match_value);
		smac = MLX5_ADDR_OF(fte_match_param,
				    in_match_value,
				    outer_headers.smac_47_16);
		ether_addr_copy(smac, acl_table->smac);
		smac = MLX5_ADDR_OF(fte_match_param,
				    in_match_criteria,
				    outer_headers.smac_47_16);
		memset(smac, 0xff, ETH_ALEN);
	}
	err = mlx5_add_flow_table_entry(acl_table->ft, default_mc_enable,
					in_match_criteria, flow_context,
					&acl_table->unknown_vlan_fr.fi);
	if (err) {
		acl_table->unknown_vlan_fr.applied = false;
	} else {
		acl_table->unknown_vlan_fr.applied = true;
		acl_table->unknown_vlan_fr.action  = new_action;
	}

out:
	if (flow_context)
		vfree(flow_context);
	if (in_match_criteria)
		vfree(in_match_criteria);
	return err;
}

static int mlx5_vacl_table_apply_vlan_filter(void *acl_t)
{
	struct mlx5_vacl_table *acl_table = (struct mlx5_vacl_table *)acl_t;
	int index = 0;
	int err_index = 0;
	int err = 0;

	if (acl_table->vlan_filter_applied)
		return 0;

	for (index = find_first_bit(acl_table->vlan_allowed_bitmap, 4096);
		index < 4096;
		index = find_next_bit(acl_table->vlan_allowed_bitmap,
				      4096, ++index)) {
		err = mlx5_vacl_table_allow_vlan(acl_t, index);
		if (err)
			goto err_disable_vlans;
	}

	acl_table->vlan_filter_applied = true;
	return 0;

err_disable_vlans:
	for (err_index = find_first_bit(acl_table->vlan_allowed_bitmap, 4096);
		err_index < index;
		err_index = find_next_bit(acl_table->vlan_allowed_bitmap, 4096,
					  ++err_index)) {
		mlx5_del_flow_table_entry(acl_table->ft,
					  acl_table->vlan_fi_table[err_index]);
	}
	return err;
}

static void mlx5_vacl_table_disapply_vlan_filter(void *acl_t)
{
	struct mlx5_vacl_table *acl_table = (struct mlx5_vacl_table *)acl_t;
	int index = 0;

	if (!acl_table->vlan_filter_applied)
		return;

	for (index = find_first_bit(acl_table->vlan_allowed_bitmap, 4096);
		index < 4096;
		index = find_next_bit(acl_table->vlan_allowed_bitmap, 4096,
				      ++index)) {
		mlx5_del_flow_table_entry(acl_table->ft,
					  acl_table->vlan_fi_table[index]);
	}

	acl_table->vlan_filter_applied = false;
}

static void mlx5_vacl_table_disapply_all_filters(void *acl_t)
{
	struct mlx5_vacl_table *acl_table = (struct mlx5_vacl_table *)acl_t;

	if (acl_table->default_fr.applied) {
		mlx5_del_flow_table_entry(acl_table->ft,
					  acl_table->default_fr.fi);
		acl_table->default_fr.applied = false;
	}
	if (acl_table->unknown_vlan_fr.applied) {
		mlx5_del_flow_table_entry(acl_table->ft,
					  acl_table->unknown_vlan_fr.fi);
		acl_table->unknown_vlan_fr.applied = false;
	}
	if (acl_table->loopback_fr.applied) {
		mlx5_del_flow_table_entry(acl_table->ft,
					  acl_table->loopback_fr.fi);
		acl_table->loopback_fr.applied = false;
	}
	if (acl_table->untagged_fr.applied) {
		mlx5_del_flow_table_entry(acl_table->ft,
					  acl_table->untagged_fr.fi);
		acl_table->untagged_fr.applied = false;
	}
	if (acl_table->vlan_filter_applied) {
		mlx5_vacl_table_disapply_vlan_filter(acl_t);
		acl_table->vlan_filter_applied = false;
	}
}

static int mlx5_vacl_table_apply_all_filters(void *acl_t)
{
	struct mlx5_vacl_table *acl_table = (struct mlx5_vacl_table *)acl_t;
	int err = 0;

	if (!acl_table->default_fr.applied && acl_table->spoofchk_enabled) {
		err = mlx5_vacl_table_apply_default(acl_table,
						    acl_table->default_fr.action);
		if (err)
			goto err_disapply_all;
	}

	if (!acl_table->unknown_vlan_fr.applied) {
		err = mlx5_vacl_table_apply_unknown_vlan(acl_table,
							 acl_table->unknown_vlan_fr.action);
		if (err)
			goto err_disapply_all;
	}

	if (!acl_table->loopback_fr.applied &&
	    acl_table->acl_type == MLX5_FLOW_TABLE_TYPE_EGRESS_ACL) {
		err = mlx5_vacl_table_apply_loopback_filter(
						acl_table,
						acl_table->loopback_fr.action);
		if (err)
			goto err_disapply_all;
	}

	if (!acl_table->untagged_fr.applied) {
		err = mlx5_vacl_table_apply_untagged(acl_table,
						     acl_table->untagged_fr.action);
		if (err)
			goto err_disapply_all;
	}

	if (!acl_table->vlan_filter_applied && acl_table->vlan_filter_enabled) {
		err = mlx5_vacl_table_apply_vlan_filter(acl_t);
		if (err)
			goto err_disapply_all;
	}

	goto out;

err_disapply_all:
	mlx5_vacl_table_disapply_all_filters(acl_t);

out:
	return err;
}

static void mlx5_vacl_table_destroy_ft(void *acl_t)
{
	struct mlx5_vacl_table *acl_table = (struct mlx5_vacl_table *)acl_t;

	mlx5_vacl_table_disapply_all_filters(acl_t);
	if (acl_table->ft)
		mlx5_destroy_flow_table(acl_table->ft);
	acl_table->ft = NULL;
}

static int mlx5_vacl_table_create_ft(void *acl_t, bool spoofchk)
{
	struct mlx5_vacl_table *acl_table = (struct mlx5_vacl_table *)acl_t;
	int log_acl_ft_size;
	int err = 0;
	int groups_num = MLX5_ACL_GROUPS_NUM - 1;
	int shift_idx = MLX5_ACL_UNTAGGED_GROUP_IDX;
	u8 *smac;
	struct mlx5_flow_table_group *g;

	if (acl_table->ft)
		return -EINVAL;

	g = kcalloc(MLX5_ACL_GROUPS_NUM, sizeof(*g), GFP_KERNEL);
	if (!g)
		goto out;

	acl_table->spoofchk_enabled = spoofchk;

	/*
	 * for vlan group
	 */
	log_acl_ft_size = 4096;
	/*
	 * for loopback filter rule
	 */
	log_acl_ft_size += 1;
	/*
	 * for untagged rule
	 */
	log_acl_ft_size += 1;
	/*
	 * for unknown vlan rule
	 */
	log_acl_ft_size += 1;
	/*
	 * for default rule
	 */
	log_acl_ft_size += 1;

	log_acl_ft_size = order_base_2(log_acl_ft_size);
	log_acl_ft_size = min_t(int, log_acl_ft_size, acl_table->max_ft_size);

	if (log_acl_ft_size < 2)
		goto out;

	if (acl_table->acl_type == MLX5_FLOW_TABLE_TYPE_EGRESS_ACL) {
		/* Loopback filter group */
		g[MLX5_ACL_LOOPBACK_GROUP_IDX].log_sz = 0;
		g[MLX5_ACL_LOOPBACK_GROUP_IDX].match_criteria_enable =
				MLX5_MATCH_MISC_PARAMETERS;
		MLX5_SET_TO_ONES(fte_match_param,
				 g[MLX5_ACL_LOOPBACK_GROUP_IDX].match_criteria,
				 misc_parameters.source_port);
		groups_num++;
		shift_idx = MLX5_ACL_LOOPBACK_GROUP_IDX;
	}
	/* Untagged traffic group */
	g[MLX5_ACL_UNTAGGED_GROUP_IDX - shift_idx].log_sz = 0;
	g[MLX5_ACL_UNTAGGED_GROUP_IDX - shift_idx].match_criteria_enable =
			MLX5_MATCH_OUTER_HEADERS;
	MLX5_SET(fte_match_param,
		 g[MLX5_ACL_UNTAGGED_GROUP_IDX - shift_idx].match_criteria,
		 outer_headers.vlan_tag, 1);
	if (spoofchk) {
		smac = MLX5_ADDR_OF(fte_match_param,
				    g[MLX5_ACL_UNTAGGED_GROUP_IDX - shift_idx]
				      .match_criteria,
				    outer_headers.smac_47_16);
		memset(smac, 0xff, ETH_ALEN);
	}

	/* Allowed vlans group */
	g[MLX5_ACL_VLAN_GROUP_IDX - shift_idx].log_sz = log_acl_ft_size - 1;
	g[MLX5_ACL_VLAN_GROUP_IDX - shift_idx].match_criteria_enable =
			MLX5_MATCH_OUTER_HEADERS;
	MLX5_SET(fte_match_param,
		 g[MLX5_ACL_VLAN_GROUP_IDX - shift_idx].match_criteria,
		 outer_headers.vlan_tag, 1);
	MLX5_SET(fte_match_param,
		 g[MLX5_ACL_VLAN_GROUP_IDX - shift_idx].match_criteria,
		 outer_headers.first_vid, 0xfff);
	if (spoofchk) {
		smac = MLX5_ADDR_OF(fte_match_param,
				    g[MLX5_ACL_VLAN_GROUP_IDX - shift_idx]
				      .match_criteria,
				    outer_headers.smac_47_16);
		memset(smac, 0xff, ETH_ALEN);
	}

	/* Unknown vlan traffic group */
	g[MLX5_ACL_UNKNOWN_VLAN_GROUP_IDX - shift_idx].log_sz = 0;
	g[MLX5_ACL_UNKNOWN_VLAN_GROUP_IDX - shift_idx].match_criteria_enable =
			(spoofchk ? MLX5_MATCH_OUTER_HEADERS : 0);
	if (spoofchk) {
		smac = MLX5_ADDR_OF(
				fte_match_param,
				g[MLX5_ACL_UNKNOWN_VLAN_GROUP_IDX - shift_idx]
				  .match_criteria,
				outer_headers.smac_47_16);
		memset(smac, 0xff, ETH_ALEN);
	}

	/*
	 * Default group - for spoofchk only.
	 */
	g[MLX5_ACL_DEFAULT_GROUP_IDX - shift_idx].log_sz = 0;
	g[MLX5_ACL_DEFAULT_GROUP_IDX - shift_idx].match_criteria_enable = 0;

	acl_table->ft = mlx5_create_flow_table(acl_table->dev,
					       0,
					       acl_table->acl_type,
					       acl_table->vport,
					       groups_num,
					       g);
	if (!acl_table->ft) {
		err = -ENOMEM;
		goto out;
	}

	err = mlx5_vacl_table_apply_all_filters(acl_t);
	if (err)
		goto err_destroy_ft;

	goto out;

err_destroy_ft:
	mlx5_vacl_table_destroy_ft(acl_table->ft);
	acl_table->ft = NULL;

out:
	kfree(g);
	return err;
}

void *mlx5_vacl_table_create(struct mlx5_core_dev *dev,
			     u16 vport, bool is_egress)
{
	struct mlx5_vacl_table *acl_table;
	int err = 0;

	if (is_egress && !MLX5_CAP_ESW_FLOWTABLE_EGRESS_ACL(dev, ft_support))
		return NULL;

	if (!is_egress && !MLX5_CAP_ESW_FLOWTABLE_INGRESS_ACL(dev, ft_support))
		return NULL;

	acl_table = kzalloc(sizeof(*acl_table), GFP_KERNEL);
	if (!acl_table)
		return NULL;

	acl_table->acl_type = is_egress ? MLX5_FLOW_TABLE_TYPE_EGRESS_ACL :
					  MLX5_FLOW_TABLE_TYPE_INGRESS_ACL;
	acl_table->max_ft_size = (is_egress ?
					MLX5_CAP_ESW_FLOWTABLE_EGRESS_ACL(dev,
									  log_max_ft_size) :
					MLX5_CAP_ESW_FLOWTABLE_INGRESS_ACL(dev,
									   log_max_ft_size));
	acl_table->dev = dev;
	acl_table->vport = vport;

	/*
	 * default behavior : Allow and if spoofchk drop the default
	 */
	acl_table->default_fr.action = MLX5_FLOW_CONTEXT_ACTION_DROP;
	acl_table->loopback_fr.action = MLX5_FLOW_CONTEXT_ACTION_DROP;
	acl_table->unknown_vlan_fr.action = MLX5_FLOW_CONTEXT_ACTION_ALLOW;
	acl_table->untagged_fr.action = MLX5_FLOW_CONTEXT_ACTION_ALLOW;
	err = mlx5_vacl_table_create_ft(acl_table, false);
	if (err)
		goto err_free_acl_table;

	acl_table->vlan_allowed_bitmap = kcalloc(BITS_TO_LONGS(4096),
						 sizeof(uintptr_t),
						 GFP_KERNEL);
	if (!acl_table->vlan_allowed_bitmap)
		goto err_destroy_ft;

	goto out;

err_destroy_ft:
	mlx5_vacl_table_destroy_ft(acl_table->ft);
	acl_table->ft = NULL;

err_free_acl_table:
	kfree(acl_table);
	acl_table = NULL;

out:
	return (void *)acl_table;
}
EXPORT_SYMBOL(mlx5_vacl_table_create);

void mlx5_vacl_table_cleanup(void *acl_t)
{
	struct mlx5_vacl_table *acl_table = (struct mlx5_vacl_table *)acl_t;

	mlx5_vacl_table_destroy_ft(acl_t);
	kfree(acl_table->vlan_allowed_bitmap);
	kfree(acl_table);
}
EXPORT_SYMBOL(mlx5_vacl_table_cleanup);

int mlx5_vacl_table_add_vlan(void *acl_t, u16 vlan)
{
	struct mlx5_vacl_table *acl_table = (struct mlx5_vacl_table *)acl_t;
	int err = 0;

	if (test_bit(vlan, acl_table->vlan_allowed_bitmap))
		return 0;
	__set_bit(vlan, acl_table->vlan_allowed_bitmap);
	if (!acl_table->vlan_filter_applied)
		return 0;

	err = mlx5_vacl_table_allow_vlan(acl_t, vlan);
	if (err)
		goto err_clear_vbit;

	goto out;

err_clear_vbit:
	__clear_bit(vlan, acl_table->vlan_allowed_bitmap);

out:
	return err;
}
EXPORT_SYMBOL(mlx5_vacl_table_add_vlan);

void mlx5_vacl_table_del_vlan(void *acl_t, u16 vlan)
{
	struct mlx5_vacl_table *acl_table = (struct mlx5_vacl_table *)acl_t;

	if (!test_bit(vlan, acl_table->vlan_allowed_bitmap))
		return;

	__clear_bit(vlan, acl_table->vlan_allowed_bitmap);

	if (!acl_table->vlan_filter_applied)
		return;

	mlx5_del_flow_table_entry(acl_table->ft,
				  acl_table->vlan_fi_table[vlan]);
}
EXPORT_SYMBOL(mlx5_vacl_table_del_vlan);

int mlx5_vacl_table_enable_vlan_filter(void *acl_t)
{
	struct mlx5_vacl_table *acl_table = (struct mlx5_vacl_table *)acl_t;

	acl_table->vlan_filter_enabled = true;
	return mlx5_vacl_table_apply_vlan_filter(acl_t);
}
EXPORT_SYMBOL(mlx5_vacl_table_enable_vlan_filter);

void mlx5_vacl_table_disable_vlan_filter(void *acl_t)
{
	struct mlx5_vacl_table *acl_table = (struct mlx5_vacl_table *)acl_t;

	acl_table->vlan_filter_enabled = false;
	mlx5_vacl_table_disapply_vlan_filter(acl_t);
}
EXPORT_SYMBOL(mlx5_vacl_table_disable_vlan_filter);

int mlx5_vacl_table_drop_untagged(void *acl_t)
{
	return mlx5_vacl_table_apply_untagged(acl_t,
			MLX5_FLOW_CONTEXT_ACTION_DROP);
}
EXPORT_SYMBOL(mlx5_vacl_table_drop_untagged);

int mlx5_vacl_table_allow_untagged(void *acl_t)
{
	return mlx5_vacl_table_apply_untagged(acl_t,
			MLX5_FLOW_CONTEXT_ACTION_ALLOW);
}
EXPORT_SYMBOL(mlx5_vacl_table_allow_untagged);

int mlx5_vacl_table_drop_unknown_vlan(void *acl_t)
{
	return mlx5_vacl_table_apply_unknown_vlan(acl_t,
			MLX5_FLOW_CONTEXT_ACTION_DROP);
}
EXPORT_SYMBOL(mlx5_vacl_table_drop_unknown_vlan);

int mlx5_vacl_table_allow_unknown_vlan(void *acl_t)
{
	return mlx5_vacl_table_apply_unknown_vlan(acl_t,
			MLX5_FLOW_CONTEXT_ACTION_ALLOW);
}
EXPORT_SYMBOL(mlx5_vacl_table_allow_unknown_vlan);

int mlx5_vacl_table_set_spoofchk(void *acl_t, bool spoofchk, u8 *vport_mac)
{
	struct mlx5_vacl_table *acl_table = (struct mlx5_vacl_table *)acl_t;
	int err = 0;

	if (spoofchk == acl_table->spoofchk_enabled) {
		if (!spoofchk ||
		    (spoofchk && !memcmp(acl_table->smac, vport_mac, ETH_ALEN)))
			return 0;
	}

	ether_addr_copy(acl_table->smac, vport_mac);
	if (spoofchk != acl_table->spoofchk_enabled) {
		mlx5_vacl_table_destroy_ft(acl_t);
		err = mlx5_vacl_table_create_ft(acl_t, spoofchk);
	} else {
		mlx5_vacl_table_disapply_all_filters(acl_t);
		err = mlx5_vacl_table_apply_all_filters(acl_t);
	}

	return err;
}
EXPORT_SYMBOL(mlx5_vacl_table_set_spoofchk);

