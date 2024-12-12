/*-
 * Copyright (c) 2020-2021, Mellanox Technologies, Ltd.
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

#include "opt_inet.h"
#include "opt_inet6.h"

#include <dev/mlx5/mlx5_en/en.h>

#include <dev/mlx5/mlx5_core/fs_core.h>
#include <dev/mlx5/mlx5_core/fs_tcp.h>
#include <dev/mlx5/device.h>

#include <sys/domain.h>

#include <netinet/in_pcb.h>

#if defined(INET) || defined(INET6)
static void
accel_fs_tcp_set_ipv4_flow(struct mlx5_flow_spec *spec, struct inpcb *inp)
{
	MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria, outer_headers.ip_protocol);
	MLX5_SET(fte_match_param, spec->match_value, outer_headers.ip_protocol, IPPROTO_TCP);
	MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria, outer_headers.ip_version);
	MLX5_SET(fte_match_param, spec->match_value, outer_headers.ip_version, 4);
	memcpy(MLX5_ADDR_OF(fte_match_param, spec->match_value,
	    outer_headers.src_ipv4_src_ipv6.ipv4_layout.ipv4),
	    &inp->inp_faddr, 4);
	memcpy(MLX5_ADDR_OF(fte_match_param, spec->match_value,
	    outer_headers.dst_ipv4_dst_ipv6.ipv4_layout.ipv4),
	    &inp->inp_laddr, 4);
	MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria,
	    outer_headers.src_ipv4_src_ipv6.ipv4_layout.ipv4);
	MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria,
	    outer_headers.dst_ipv4_dst_ipv6.ipv4_layout.ipv4);
}
#endif

#ifdef INET6
static void
accel_fs_tcp_set_ipv6_flow(struct mlx5_flow_spec *spec, struct inpcb *inp)
{
	MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria, outer_headers.ip_protocol);
	MLX5_SET(fte_match_param, spec->match_value, outer_headers.ip_protocol, IPPROTO_TCP);
	MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria, outer_headers.ip_version);
	MLX5_SET(fte_match_param, spec->match_value, outer_headers.ip_version, 6);
	memcpy(MLX5_ADDR_OF(fte_match_param, spec->match_value,
	    outer_headers.src_ipv4_src_ipv6.ipv6_layout.ipv6),
	    &inp->in6p_faddr, 16);
	memcpy(MLX5_ADDR_OF(fte_match_param, spec->match_value,
	    outer_headers.dst_ipv4_dst_ipv6.ipv6_layout.ipv6),
	    &inp->in6p_laddr, 16);
	memset(MLX5_ADDR_OF(fte_match_param, spec->match_criteria,
	    outer_headers.src_ipv4_src_ipv6.ipv6_layout.ipv6),
	    0xff, 16);
	memset(MLX5_ADDR_OF(fte_match_param, spec->match_criteria,
	    outer_headers.dst_ipv4_dst_ipv6.ipv6_layout.ipv6),
	    0xff, 16);
}
#endif

void
mlx5e_accel_fs_del_inpcb(struct mlx5_flow_handle *rule)
{
	mlx5_del_flow_rules(&rule);
}

struct mlx5_flow_handle *
mlx5e_accel_fs_add_inpcb(struct mlx5e_priv *priv,
    struct inpcb *inp, uint32_t tirn, uint32_t flow_tag,
    uint16_t vlan_id)
{
	struct mlx5_flow_destination dest = {};
	struct mlx5e_flow_table *ft = NULL;
#if defined(INET) || defined(INET6)
	struct mlx5e_accel_fs_tcp *fs_tcp = &priv->fts.accel_tcp;
#endif
	struct mlx5_flow_handle *flow;
	struct mlx5_flow_spec *spec;
	struct mlx5_flow_act flow_act = {};

	spec = kvzalloc(sizeof(*spec), GFP_KERNEL);
	if (!spec)
		return (ERR_PTR(-ENOMEM));

	spec->match_criteria_enable = MLX5_MATCH_OUTER_HEADERS;
	spec->flow_context.flags = FLOW_CONTEXT_HAS_TAG;
	spec->flow_context.flow_tag = flow_tag;

	INP_RLOCK(inp);
	/* Set VLAN ID to match, if any. */
	MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria, outer_headers.cvlan_tag);
	MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria, outer_headers.first_vid);
	if (vlan_id != MLX5E_ACCEL_FS_ADD_INPCB_NO_VLAN) {
		MLX5_SET_TO_ONES(fte_match_param, spec->match_value, outer_headers.cvlan_tag);
		MLX5_SET(fte_match_param, spec->match_value, outer_headers.first_vid, vlan_id);
	}

	/* Set TCP port numbers. */
	MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria,
	    outer_headers.tcp_dport);
	MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria,
	    outer_headers.tcp_sport);
	MLX5_SET(fte_match_param, spec->match_value, outer_headers.tcp_dport,
	    ntohs(inp->inp_lport));
	MLX5_SET(fte_match_param, spec->match_value, outer_headers.tcp_sport,
	    ntohs(inp->inp_fport));

	/* Set IP addresses. */
	switch (INP_SOCKAF(inp->inp_socket)) {
#ifdef INET
	case AF_INET:
		accel_fs_tcp_set_ipv4_flow(spec, inp);
		ft = &fs_tcp->tables[MLX5E_ACCEL_FS_IPV4_TCP];
		break;
#endif
#ifdef INET6
	case AF_INET6:
		if ((inp->inp_flags & IN6P_IPV6_V6ONLY) == 0 &&
		    IN6_IS_ADDR_V4MAPPED(&inp->in6p_faddr)) {
			accel_fs_tcp_set_ipv4_flow(spec, inp);
			ft = &fs_tcp->tables[MLX5E_ACCEL_FS_IPV4_TCP];
		} else {
			accel_fs_tcp_set_ipv6_flow(spec, inp);
			ft = &fs_tcp->tables[MLX5E_ACCEL_FS_IPV6_TCP];
		}
		break;
#endif
	default:
		break;
	}
	INP_RUNLOCK(inp);

	if (!ft) {
		flow = ERR_PTR(-EINVAL);
		goto out;
	}

	dest.type = MLX5_FLOW_DESTINATION_TYPE_TIR;
	dest.tir_num = tirn;
	flow_act.action = MLX5_FLOW_CONTEXT_ACTION_FWD_DEST;

	flow = mlx5_add_flow_rules(ft->t, spec, &flow_act, &dest, 1);
out:
	kvfree(spec);
	return (flow);
}

static int
accel_fs_tcp_add_default_rule(struct mlx5e_priv *priv, int type)
{
	static struct mlx5_flow_spec spec = {};
	struct mlx5_flow_destination dest = {};
	struct mlx5e_accel_fs_tcp *fs_tcp;
	struct mlx5_flow_handle *rule;
	struct mlx5_flow_act flow_act = {
		.action = MLX5_FLOW_CONTEXT_ACTION_FWD_DEST,
	};

	fs_tcp = &priv->fts.accel_tcp;

	spec.flow_context.flags = FLOW_CONTEXT_HAS_TAG;
	spec.flow_context.flow_tag = MLX5_FS_DEFAULT_FLOW_TAG;
	dest.type = MLX5_FLOW_DESTINATION_TYPE_FLOW_TABLE;

	/*
	 * Traffic not matched by flow table rules should be forwarded
	 * to the next flow table in order to not be dropped by the
	 * default action. Refer to the diagram in
	 * mlx5_en_flow_table.c for more information about the order
	 * of flow tables.
	 */
	dest.ft = (type == MLX5E_ACCEL_FS_TCP_NUM_TYPES - 1) ?
		  ((priv->fts.ipsec_ft) ? priv->fts.ipsec_ft : priv->fts.vlan.t)  :
		  fs_tcp->tables[type + 1].t;

	rule = mlx5_add_flow_rules(fs_tcp->tables[type].t, &spec, &flow_act,
				   &dest, 1);
	if (IS_ERR(rule))
		return (PTR_ERR(rule));

	fs_tcp->default_rules[type] = rule;
	return (0);
}

#define	MLX5E_ACCEL_FS_TCP_NUM_GROUPS	(2)
#define	MLX5E_ACCEL_FS_TCP_GROUP1_SIZE	(BIT(16) - 1)
#define	MLX5E_ACCEL_FS_TCP_GROUP2_SIZE	(BIT(0))
#define	MLX5E_ACCEL_FS_TCP_TABLE_SIZE	(MLX5E_ACCEL_FS_TCP_GROUP1_SIZE +\
					 MLX5E_ACCEL_FS_TCP_GROUP2_SIZE)
static int
accel_fs_tcp_create_groups(struct mlx5e_flow_table *ft, int type)
{
	int inlen = MLX5_ST_SZ_BYTES(create_flow_group_in);
	void *outer_headers_c;
	int ix = 0;
	u32 *in;
	int err;
	u8 *mc;

	ft->g = kcalloc(MLX5E_ACCEL_FS_TCP_NUM_GROUPS, sizeof(*ft->g), GFP_KERNEL);
	in = kvzalloc(inlen, GFP_KERNEL);
	if (!in || !ft->g) {
		kfree(ft->g);
		kvfree(in);
		return (-ENOMEM);
	}

	mc = MLX5_ADDR_OF(create_flow_group_in, in, match_criteria);
	outer_headers_c = MLX5_ADDR_OF(fte_match_param, mc, outer_headers);
	MLX5_SET_TO_ONES(fte_match_set_lyr_2_4, outer_headers_c, ip_protocol);
	MLX5_SET_TO_ONES(fte_match_set_lyr_2_4, outer_headers_c, ip_version);
	MLX5_SET_TO_ONES(fte_match_set_lyr_2_4, outer_headers_c, cvlan_tag);
	MLX5_SET_TO_ONES(fte_match_set_lyr_2_4, outer_headers_c, first_vid);

	switch (type) {
	case MLX5E_ACCEL_FS_IPV4_TCP:
	case MLX5E_ACCEL_FS_IPV6_TCP:
		MLX5_SET_TO_ONES(fte_match_set_lyr_2_4, outer_headers_c, tcp_dport);
		MLX5_SET_TO_ONES(fte_match_set_lyr_2_4, outer_headers_c, tcp_sport);
		break;
	default:
		err = -EINVAL;
		goto out;
	}

	switch (type) {
	case MLX5E_ACCEL_FS_IPV4_TCP:
		MLX5_SET_TO_ONES(fte_match_set_lyr_2_4, outer_headers_c,
		    src_ipv4_src_ipv6.ipv4_layout.ipv4);
		MLX5_SET_TO_ONES(fte_match_set_lyr_2_4, outer_headers_c,
		    dst_ipv4_dst_ipv6.ipv4_layout.ipv4);
		break;
	case MLX5E_ACCEL_FS_IPV6_TCP:
		memset(MLX5_ADDR_OF(fte_match_set_lyr_2_4, outer_headers_c,
		    src_ipv4_src_ipv6.ipv6_layout.ipv6),
		    0xff, 16);
		memset(MLX5_ADDR_OF(fte_match_set_lyr_2_4, outer_headers_c,
		    dst_ipv4_dst_ipv6.ipv6_layout.ipv6),
		    0xff, 16);
		break;
	default:
		err = -EINVAL;
		goto out;
	}

	MLX5_SET_CFG(in, match_criteria_enable, MLX5_MATCH_OUTER_HEADERS);
	MLX5_SET_CFG(in, start_flow_index, ix);
	ix += MLX5E_ACCEL_FS_TCP_GROUP1_SIZE;
	MLX5_SET_CFG(in, end_flow_index, ix - 1);
	ft->g[ft->num_groups] = mlx5_create_flow_group(ft->t, in);
	if (IS_ERR(ft->g[ft->num_groups]))
		goto err;
	ft->num_groups++;

	/* Default Flow Group */
	memset(in, 0, inlen);
	MLX5_SET_CFG(in, start_flow_index, ix);
	ix += MLX5E_ACCEL_FS_TCP_GROUP2_SIZE;
	MLX5_SET_CFG(in, end_flow_index, ix - 1);
	ft->g[ft->num_groups] = mlx5_create_flow_group(ft->t, in);
	if (IS_ERR(ft->g[ft->num_groups]))
		goto err;
	ft->num_groups++;

	kvfree(in);
	return (0);

err:
	err = PTR_ERR(ft->g[ft->num_groups]);
	ft->g[ft->num_groups] = NULL;
out:
	kvfree(in);

	return (err);
}

static void
accel_fs_tcp_destroy_groups(struct mlx5e_flow_table *ft)
{
        int i;

        for (i = ft->num_groups - 1; i >= 0; i--) {
                if (!IS_ERR_OR_NULL(ft->g[i]))
                        mlx5_destroy_flow_group(ft->g[i]);
                ft->g[i] = NULL;
        }
        ft->num_groups = 0;
}

static int
accel_fs_tcp_create_table(struct mlx5e_priv *priv, int type)
{
	struct mlx5e_flow_table *ft = &priv->fts.accel_tcp.tables[type];
	struct mlx5_flow_table_attr ft_attr = {};
	int err;

	ft->num_groups = 0;
	ft_attr.max_fte = MLX5E_ACCEL_FS_TCP_TABLE_SIZE;
	ft_attr.level = type;
	ft->t = mlx5_create_flow_table(priv->fts.accel_tcp.ns, &ft_attr);
	if (IS_ERR(ft->t)) {
		err = PTR_ERR(ft->t);
		ft->t = NULL;
		return (err);
	}

	err = accel_fs_tcp_create_groups(ft, type);
	if (err)
		goto err_destroy_flow_table;

	return (0);

err_destroy_flow_table:
	mlx5_destroy_flow_table(ft->t);
	ft->t = NULL;
	return (err);
}

static void
accel_fs_tcp_destroy_table(struct mlx5e_priv *priv, int i)
{
	struct mlx5e_accel_fs_tcp *fs_tcp;
	struct mlx5e_flow_table *ft;

	fs_tcp = &priv->fts.accel_tcp;
	ft = fs_tcp->tables + i;

	accel_fs_tcp_destroy_groups(ft);
	kfree(ft->g);
	ft->g = NULL;
	mlx5_destroy_flow_table(ft->t);
	ft->t = NULL;
}

void
mlx5e_accel_fs_tcp_destroy(struct mlx5e_priv *priv)
{
	int i;

	if (!MLX5_CAP_FLOWTABLE_NIC_RX(priv->mdev, ft_field_support.outer_ip_version))
		return;

	for (i = 0; i < MLX5E_ACCEL_FS_TCP_NUM_TYPES; i++) {
		mlx5_del_flow_rules(&priv->fts.accel_tcp.default_rules[i]);
		accel_fs_tcp_destroy_table(priv, i);
	}
}

int
mlx5e_accel_fs_tcp_create(struct mlx5e_priv *priv)
{
	int i, err;

	if (!MLX5_CAP_FLOWTABLE_NIC_RX(priv->mdev, ft_field_support.outer_ip_version))
		return (0);

	/* Setup namespace pointer. */
	priv->fts.accel_tcp.ns = mlx5_get_flow_namespace(
	    priv->mdev, MLX5_FLOW_NAMESPACE_OFFLOADS);

	/*
	 * Create flow tables first, because the priority level is
	 * assigned at allocation time.
	 */
	for (i = 0; i != MLX5E_ACCEL_FS_TCP_NUM_TYPES; i++) {
		err = accel_fs_tcp_create_table(priv, i);
		if (err)
			goto err_destroy_tables;
	}

	/* Create default rules last. */
	for (i = 0; i != MLX5E_ACCEL_FS_TCP_NUM_TYPES; i++) {
		err = accel_fs_tcp_add_default_rule(priv, i);
		if (err)
			goto err_destroy_rules;
	}
	return (0);

err_destroy_rules:
	while (i--)
		mlx5_del_flow_rules(&priv->fts.accel_tcp.default_rules[i]);
	i = MLX5E_ACCEL_FS_TCP_NUM_TYPES;

err_destroy_tables:
	while (i--)
		accel_fs_tcp_destroy_table(priv, i);
	return (err);
}
