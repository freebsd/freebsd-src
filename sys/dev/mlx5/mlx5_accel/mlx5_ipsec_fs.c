/*-
 * Copyright (c) 2023 NVIDIA corporation & affiliates.
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
 */

#include "opt_ipsec.h"

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <net/pfkeyv2.h>
#include <netipsec/key_var.h>
#include <netipsec/keydb.h>
#include <netipsec/ipsec.h>
#include <netipsec/xform.h>
#include <netipsec/ipsec_offload.h>
#include <dev/mlx5/fs.h>
#include <dev/mlx5/mlx5_en/en.h>
#include <dev/mlx5/qp.h>
#include <dev/mlx5/mlx5_accel/ipsec.h>
#include <dev/mlx5/mlx5_core/fs_core.h>
#include <dev/mlx5/mlx5_core/fs_chains.h>

/*
 * TX tables are organized differently for Ethernet and for RoCE:
 *
 *                       +=========+
 *       Ethernet Tx     | SA KSPI | match
 * --------------------->|Flowtable|----->+         +
 *                       |         |\     |        / \
 *                       +=========+ |    |       /   \         +=========+     +=========+
 *                              miss |    |      /     \        |  Status |     |         |
 *                      DROP<--------+    |---->|Encrypt|------>|Flowtable|---->|  TX NS  |
 *                                        |      \     /        |         |     |         |
 *                                        |       \   /         +=========+     +=========+
 *       +=========+      +=========+     |        \ /               |
 *  RoCE |  Policy | match|SA ReqId |match|         +                |
 *  Tx   |Flowtable|----->|Flowtable|---->+                          |
 *  ---->|IP header|      |ReqId+IP |                                |
 *       |         |      | header  |--------------------------------+
 *       +=========+      +=========+         miss                   |
 *            |                                                      |
 *            |                   miss                               |
 *            +-------------------------------------------------------
 *
 *                                                                                  +=========+
 *                                                                                  |   RDMA  |
 *                                                                                  |Flowtable|
 *                                                                                  |         |
 * Rx Tables and rules:                                                             +=========+
 *                                             +                                        /
 *       +=========+      +=========+         / \         +=========+      +=========+ /match
 *       |  Policy |      |   SA    |        /   \        |  Status |      |  RoCE   |/
 *  ---->|Flowtable| match|Flowtable| match /     \       |Flowtable|----->|Flowtable|
 *       |IP header|----->|IP header|----->|Decrypt|----->|         |      | Roce V2 |
 *       |         |      |+ESP+SPI |       \     /       |         |      | UDP port|\
 *       +=========+      +=========+        \   /        +=========+      +=========+ \miss
 *             |               |              \ /                                       \
 *             |               |               +                                      +=========+
 *             |     miss      |          miss                                       | Ethernet|
 *             +--------------->---------------------------------------------------->|  RX NS  |
 *                                                                                   |         |
 *                                                                                   +=========+
 *
 */

#define NUM_IPSEC_FTE BIT(15)
#define IPSEC_TUNNEL_DEFAULT_TTL 0x40

struct mlx5e_ipsec_fc {
	struct mlx5_fc *cnt;
	struct mlx5_fc *drop;
};

struct mlx5e_ipsec_ft {
	struct mutex mutex; /* Protect changes to this struct */
	struct mlx5_flow_table *pol;
	struct mlx5_flow_table *sa_kspi;
	struct mlx5_flow_table *sa;
	struct mlx5_flow_table *status;
	u32 refcnt;
};

struct mlx5e_ipsec_tx_roce {
	struct mlx5_flow_group *g;
	struct mlx5_flow_table *ft;
	struct mlx5_flow_handle *rule;
	struct mlx5_flow_namespace *ns;
};

struct mlx5e_ipsec_miss {
	struct mlx5_flow_group *group;
	struct mlx5_flow_handle *rule;
};

struct mlx5e_ipsec_tx {
	struct mlx5e_ipsec_ft ft;
	struct mlx5e_ipsec_miss pol;
	struct mlx5e_ipsec_miss kspi_miss;
	struct mlx5e_ipsec_rule status;
	struct mlx5e_ipsec_rule kspi_bypass_rule; /*rule for IPSEC bypass*/
	struct mlx5_flow_namespace *ns;
	struct mlx5e_ipsec_fc *fc;
	struct mlx5_fs_chains *chains;
	struct mlx5e_ipsec_tx_roce roce;
};

struct mlx5e_ipsec_rx_roce {
	struct mlx5_flow_group *g;
	struct mlx5_flow_table *ft;
	struct mlx5_flow_handle *rule;
	struct mlx5e_ipsec_miss roce_miss;

	struct mlx5_flow_table *ft_rdma;
	struct mlx5_flow_namespace *ns_rdma;
};

struct mlx5e_ipsec_rx {
	struct mlx5e_ipsec_ft ft;
	struct mlx5e_ipsec_miss pol;
	struct mlx5e_ipsec_miss sa;
	struct mlx5e_ipsec_rule status;
	struct mlx5_flow_namespace *ns;
	struct mlx5e_ipsec_fc *fc;
	struct mlx5_fs_chains *chains;
	struct mlx5e_ipsec_rx_roce roce;
};

static void setup_fte_reg_a_with_tag(struct mlx5_flow_spec *spec,
                                     u16 kspi);
static void setup_fte_reg_a_no_tag(struct mlx5_flow_spec *spec);

static void setup_fte_no_frags(struct mlx5_flow_spec *spec)
{
	/* Non fragmented */
	spec->match_criteria_enable |= MLX5_MATCH_OUTER_HEADERS;

	MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria, outer_headers.frag);
	MLX5_SET(fte_match_param, spec->match_value, outer_headers.frag, 0);
}

static void setup_fte_esp(struct mlx5_flow_spec *spec)
{
	/* ESP header */
	spec->match_criteria_enable |= MLX5_MATCH_OUTER_HEADERS;

	MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria, outer_headers.ip_protocol);
	MLX5_SET(fte_match_param, spec->match_value, outer_headers.ip_protocol, IPPROTO_ESP);
}

static void setup_fte_spi(struct mlx5_flow_spec *spec, u32 spi, bool encap)
{
	/* SPI number */
	spec->match_criteria_enable |= MLX5_MATCH_MISC_PARAMETERS;

	if (encap) {
		MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria, misc_parameters.inner_esp_spi);
		MLX5_SET(fte_match_param, spec->match_value, misc_parameters.inner_esp_spi, spi);
	} else {
		MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria, misc_parameters.outer_esp_spi);
		MLX5_SET(fte_match_param, spec->match_value, misc_parameters.outer_esp_spi, spi);
	}
}

static void
setup_fte_vid(struct mlx5_flow_spec *spec, u16 vid)
{
	/* virtual lan tag */
	spec->match_criteria_enable |= MLX5_MATCH_OUTER_HEADERS;

	MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria,
	    outer_headers.cvlan_tag);
	MLX5_SET(fte_match_param, spec->match_value,
	    outer_headers.cvlan_tag, 1);
	MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria,
	    outer_headers.first_vid);
	MLX5_SET(fte_match_param, spec->match_value, outer_headers.first_vid,
	    vid);
}

static void
clear_fte_vid(struct mlx5_flow_spec *spec)
{
	MLX5_SET(fte_match_param, spec->match_criteria,
	    outer_headers.cvlan_tag, 0);
	MLX5_SET(fte_match_param, spec->match_value,
	    outer_headers.cvlan_tag, 0);
	MLX5_SET(fte_match_param, spec->match_criteria,
	    outer_headers.first_vid, 0);
	MLX5_SET(fte_match_param, spec->match_value,
	    outer_headers.first_vid, 0);
}

static void
setup_fte_no_vid(struct mlx5_flow_spec *spec)
{
	MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria,
	    outer_headers.cvlan_tag);
	MLX5_SET(fte_match_param, spec->match_value,
	    outer_headers.cvlan_tag, 0);
}

static struct mlx5_fs_chains *
ipsec_chains_create(struct mlx5_core_dev *mdev, struct mlx5_flow_table *miss_ft,
		    enum mlx5_flow_namespace_type ns, int base_prio,
		    int base_level, struct mlx5_flow_table **root_ft)
{
	struct mlx5_chains_attr attr = {};
	struct mlx5_fs_chains *chains;
	struct mlx5_flow_table *ft;
	int err;

	attr.flags = MLX5_CHAINS_AND_PRIOS_SUPPORTED |
		     MLX5_CHAINS_IGNORE_FLOW_LEVEL_SUPPORTED;
	attr.max_grp_num = 2;
	attr.default_ft = miss_ft;
	attr.ns = ns;
	attr.fs_base_prio = base_prio;
	attr.fs_base_level = base_level;
	chains = mlx5_chains_create(mdev, &attr);
	if (IS_ERR(chains))
		return chains;

	/* Create chain 0, prio 1, level 0 to connect chains to prev in fs_core */
	ft = mlx5_chains_get_table(chains, 0, 1, 0);
	if (IS_ERR(ft)) {
		err = PTR_ERR(ft);
		goto err_chains_get;
	}

	*root_ft = ft;
	return chains;

err_chains_get:
	mlx5_chains_destroy(chains);
	return ERR_PTR(err);
}

static void ipsec_chains_destroy(struct mlx5_fs_chains *chains)
{
	mlx5_chains_put_table(chains, 0, 1, 0);
	mlx5_chains_destroy(chains);
}

static struct mlx5_flow_table *
ipsec_chains_get_table(struct mlx5_fs_chains *chains, u32 prio)
{
	return mlx5_chains_get_table(chains, 0, prio + 1, 0);
}

static void ipsec_chains_put_table(struct mlx5_fs_chains *chains, u32 prio)
{
	mlx5_chains_put_table(chains, 0, prio + 1, 0);
}

static struct mlx5_flow_table *ipsec_rx_ft_create(struct mlx5_flow_namespace *ns,
						  int level, int prio,
						  int max_num_groups)
{
	struct mlx5_flow_table_attr ft_attr = {};

	ft_attr.max_fte = NUM_IPSEC_FTE;
	ft_attr.level = level;
	ft_attr.prio = prio;
	ft_attr.autogroup.max_num_groups = max_num_groups;
	ft_attr.autogroup.num_reserved_entries = 1;

	return mlx5_create_auto_grouped_flow_table(ns, &ft_attr);
}

static int ipsec_miss_create(struct mlx5_core_dev *mdev,
			     struct mlx5_flow_table *ft,
			     struct mlx5e_ipsec_miss *miss,
			     struct mlx5_flow_destination *dest)
{
	int inlen = MLX5_ST_SZ_BYTES(create_flow_group_in);
	struct mlx5_flow_act flow_act = {};
	struct mlx5_flow_spec *spec;
	u32 *flow_group_in;
	int err = 0;

	flow_group_in = kvzalloc(inlen, GFP_KERNEL);
	spec = kvzalloc(sizeof(*spec), GFP_KERNEL);
	if (!flow_group_in || !spec) {
		err = -ENOMEM;
		goto out;
	}

	/* Create miss_group */
	MLX5_SET(create_flow_group_in, flow_group_in, start_flow_index, ft->max_fte - 1);
	MLX5_SET(create_flow_group_in, flow_group_in, end_flow_index, ft->max_fte - 1);
	miss->group = mlx5_create_flow_group(ft, flow_group_in);
	if (IS_ERR(miss->group)) {
		err = PTR_ERR(miss->group);
		mlx5_core_err(mdev, "fail to create IPsec miss_group err=%d\n",
			      err);
		goto out;
	}

	if (dest)
		flow_act.action = MLX5_FLOW_RULE_FWD_ACTION_DEST;
	else
		flow_act.action = MLX5_FLOW_RULE_FWD_ACTION_DROP;
	/* Create miss rule */
	miss->rule = mlx5_add_flow_rules(ft, NULL, &flow_act, dest, 1);
	if (IS_ERR(miss->rule)) {
		mlx5_destroy_flow_group(miss->group);
		err = PTR_ERR(miss->rule);
		mlx5_core_err(mdev, "fail to create IPsec miss_rule err=%d\n",
			      err);
		goto out;
	}
out:
	kvfree(flow_group_in);
	kvfree(spec);
	return err;
}

static int setup_modify_header(struct mlx5_core_dev *mdev, u32 val, u8 dir,
                               struct mlx5_flow_act *flow_act)
{
        u8 action[MLX5_UN_SZ_BYTES(set_add_copy_action_in_auto)] = {};
        enum mlx5_flow_namespace_type ns_type;
        struct mlx5_modify_hdr *modify_hdr;

        MLX5_SET(set_action_in, action, action_type, MLX5_ACTION_TYPE_SET);
        switch (dir) {
        case IPSEC_DIR_INBOUND:
                MLX5_SET(set_action_in, action, field,
                         MLX5_ACTION_IN_FIELD_METADATA_REG_B);
                ns_type = MLX5_FLOW_NAMESPACE_KERNEL;
                break;
        case IPSEC_DIR_OUTBOUND:
                MLX5_SET(set_action_in, action, field,
                         MLX5_ACTION_IN_FIELD_METADATA_REG_C_0);
                ns_type = MLX5_FLOW_NAMESPACE_EGRESS;
                break;
        default:
                return -EINVAL;
        }

        MLX5_SET(set_action_in, action, data, val);
        MLX5_SET(set_action_in, action, offset, 0);
        MLX5_SET(set_action_in, action, length, 32);

        modify_hdr = mlx5_modify_header_alloc(mdev, ns_type, 1, action);
        if (IS_ERR(modify_hdr)) {
                mlx5_core_err(mdev, "Failed to allocate modify_header %ld\n",
                              PTR_ERR(modify_hdr));
                return PTR_ERR(modify_hdr);
        }

        flow_act->modify_hdr = modify_hdr;
        flow_act->action |= MLX5_FLOW_CONTEXT_ACTION_MOD_HDR;
        return 0;
}

static int
setup_pkt_transport_reformat(struct mlx5_accel_esp_xfrm_attrs *attrs,
			     struct mlx5_pkt_reformat_params *reformat_params)
{
	struct udphdr *udphdr;
	size_t bfflen = 16;
	char *reformatbf;
	__be32 spi;
	void *hdr;

	if (attrs->family == AF_INET) {
		if (attrs->encap)
			reformat_params->type = MLX5_REFORMAT_TYPE_ADD_ESP_TRANSPORT_OVER_UDPV4;
		else
			reformat_params->type = MLX5_REFORMAT_TYPE_ADD_ESP_TRANSPORT_OVER_IPV4;
	} else {
		if (attrs->encap)
			reformat_params->type =
			    MLX5_REFORMAT_TYPE_ADD_ESP_TRANSPORT_OVER_UDPV6;
		else
			reformat_params->type =
			    MLX5_REFORMAT_TYPE_ADD_ESP_TRANSPORT_OVER_IPV6;
	}

	if (attrs->encap)
		bfflen += sizeof(*udphdr);
	reformatbf = kzalloc(bfflen, GFP_KERNEL);
	if (!reformatbf)
		return -ENOMEM;

	hdr = reformatbf;
	if (attrs->encap) {
		udphdr = (struct udphdr *)reformatbf;
		udphdr->uh_sport = attrs->sport;
		udphdr->uh_dport = attrs->dport;
		hdr += sizeof(*udphdr);
	}

	/* convert to network format */
	spi = htonl(attrs->spi);
	memcpy(hdr, &spi, 4);

	reformat_params->param_0 = attrs->authsize;
	reformat_params->size = bfflen;
	reformat_params->data = reformatbf;

	return 0;
}

static int setup_pkt_reformat(struct mlx5_core_dev *mdev,
			      struct mlx5_accel_esp_xfrm_attrs *attrs,
			      struct mlx5_flow_act *flow_act)
{
	enum mlx5_flow_namespace_type ns_type = MLX5_FLOW_NAMESPACE_EGRESS;
	struct mlx5_pkt_reformat_params reformat_params = {};
	struct mlx5_pkt_reformat *pkt_reformat;
	int ret;

	if (attrs->dir == IPSEC_DIR_INBOUND) {
		if (attrs->encap)
			reformat_params.type = MLX5_REFORMAT_TYPE_DEL_ESP_TRANSPORT_OVER_UDP;
		else
			reformat_params.type = MLX5_REFORMAT_TYPE_DEL_ESP_TRANSPORT;
		ns_type = MLX5_FLOW_NAMESPACE_KERNEL;
		goto cmd;
	}

	ret = setup_pkt_transport_reformat(attrs, &reformat_params);
	if (ret)
		return ret;
cmd:
	pkt_reformat =
		mlx5_packet_reformat_alloc(mdev, &reformat_params, ns_type);
	if (reformat_params.data)
		kfree(reformat_params.data);
	if (IS_ERR(pkt_reformat))
		return PTR_ERR(pkt_reformat);

	flow_act->pkt_reformat = pkt_reformat;
	flow_act->action |= MLX5_FLOW_CONTEXT_ACTION_PACKET_REFORMAT;
	return 0;
}

static void setup_fte_addr4(struct mlx5_flow_spec *spec, __be32 *saddr,
                            __be32 *daddr)
{
        spec->match_criteria_enable |= MLX5_MATCH_OUTER_HEADERS;

        MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria, outer_headers.ip_version);
        MLX5_SET(fte_match_param, spec->match_value, outer_headers.ip_version, 4);

        memcpy(MLX5_ADDR_OF(fte_match_param, spec->match_value,
                            outer_headers.src_ipv4_src_ipv6.ipv4_layout.ipv4), saddr, 4);
        memcpy(MLX5_ADDR_OF(fte_match_param, spec->match_value,
                            outer_headers.dst_ipv4_dst_ipv6.ipv4_layout.ipv4), daddr, 4);
        MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria,
                         outer_headers.src_ipv4_src_ipv6.ipv4_layout.ipv4);
        MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria,
                         outer_headers.dst_ipv4_dst_ipv6.ipv4_layout.ipv4);
}

static void setup_fte_addr6(struct mlx5_flow_spec *spec, __be32 *saddr,
                            __be32 *daddr)
{
        spec->match_criteria_enable |= MLX5_MATCH_OUTER_HEADERS;

        MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria, outer_headers.ip_version);
        MLX5_SET(fte_match_param, spec->match_value, outer_headers.ip_version, 6);

        memcpy(MLX5_ADDR_OF(fte_match_param, spec->match_value,
                            outer_headers.src_ipv4_src_ipv6.ipv6_layout.ipv6), saddr, 16);
        memcpy(MLX5_ADDR_OF(fte_match_param, spec->match_value,
                            outer_headers.dst_ipv4_dst_ipv6.ipv6_layout.ipv6), daddr, 16);
        memset(MLX5_ADDR_OF(fte_match_param, spec->match_criteria,
                            outer_headers.src_ipv4_src_ipv6.ipv6_layout.ipv6), 0xff, 16);
        memset(MLX5_ADDR_OF(fte_match_param, spec->match_criteria,
                            outer_headers.dst_ipv4_dst_ipv6.ipv6_layout.ipv6), 0xff, 16);
}

static int rx_add_rule(struct mlx5e_ipsec_sa_entry *sa_entry)
{
	struct mlx5e_ipsec_rule *ipsec_rule = &sa_entry->ipsec_rule;
	struct mlx5_accel_esp_xfrm_attrs *attrs = &sa_entry->attrs;
	struct mlx5_core_dev *mdev = mlx5e_ipsec_sa2dev(sa_entry);
	struct mlx5e_ipsec *ipsec = sa_entry->ipsec;
	struct mlx5_flow_destination dest[2] = {};
	struct mlx5_flow_act flow_act = {};
	struct mlx5_flow_handle *rule;
	struct mlx5_flow_spec *spec;
	struct mlx5e_ipsec_rx *rx;
	struct mlx5_fc *counter;
	int err;

	rx = (attrs->family == AF_INET) ? ipsec->rx_ipv4 : ipsec->rx_ipv6;

	spec = kvzalloc(sizeof(*spec), GFP_KERNEL);
	if (!spec)
		return -ENOMEM;

	if (!attrs->drop) {
		err = setup_modify_header(mdev, sa_entry->kspi | BIT(31), IPSEC_DIR_INBOUND,
					  &flow_act);
		if (err)
			goto err_mod_header;
	}

	err = setup_pkt_reformat(mdev, attrs, &flow_act);
	if (err)
		goto err_pkt_reformat;

	counter = mlx5_fc_create(mdev, false);
	if (IS_ERR(counter)) {
		err = PTR_ERR(counter);
		goto err_add_cnt;
	}

	flow_act.crypto.type = MLX5_FLOW_CONTEXT_ENCRYPT_DECRYPT_TYPE_IPSEC;
	flow_act.crypto.op = MLX5_FLOW_ACT_CRYPTO_OP_DECRYPT;
	flow_act.crypto.obj_id = sa_entry->ipsec_obj_id;
	flow_act.flags |= FLOW_ACT_NO_APPEND;

	flow_act.action |= MLX5_FLOW_CONTEXT_ACTION_CRYPTO_DECRYPT |
		MLX5_FLOW_CONTEXT_ACTION_COUNT;

	if (attrs->drop)
		flow_act.action |= MLX5_FLOW_CONTEXT_ACTION_DROP;
	else
		flow_act.action |= MLX5_FLOW_CONTEXT_ACTION_FWD_DEST;

	dest[0].type = MLX5_FLOW_DESTINATION_TYPE_FLOW_TABLE;
	dest[0].ft = rx->ft.status;
	dest[1].type = MLX5_FLOW_DESTINATION_TYPE_COUNTER;
	dest[1].counter_id = mlx5_fc_id(counter);

	if (attrs->family == AF_INET)
		setup_fte_addr4(spec, &attrs->saddr.a4, &attrs->daddr.a4);
	else
		setup_fte_addr6(spec, attrs->saddr.a6, attrs->daddr.a6);

	if (!attrs->encap)
		setup_fte_esp(spec);

	setup_fte_spi(spec, attrs->spi, attrs->encap);
	setup_fte_no_frags(spec);

	if (sa_entry->vid != VLAN_NONE)
		setup_fte_vid(spec, sa_entry->vid);
	else
		setup_fte_no_vid(spec);

	rule = mlx5_add_flow_rules(rx->ft.sa, spec, &flow_act, dest, 2);
	if (IS_ERR(rule)) {
		err = PTR_ERR(rule);
		mlx5_core_err(mdev, "fail to add RX ipsec rule err=%d\n", err);
		goto err_add_flow;
	}
	ipsec_rule->rule = rule;

	/* Add another rule for zero vid */
	if (sa_entry->vid == VLAN_NONE) {
		clear_fte_vid(spec);
		setup_fte_vid(spec, 0);
		rule = mlx5_add_flow_rules(rx->ft.sa, spec, &flow_act, dest, 2);
		if (IS_ERR(rule)) {
			err = PTR_ERR(rule);
			mlx5_core_err(mdev,
			    "fail to add RX ipsec zero vid rule err=%d\n",
			    err);
			goto err_add_flow;
		}
		ipsec_rule->vid_zero_rule = rule;
	}

	kvfree(spec);
	ipsec_rule->fc = counter;
	ipsec_rule->modify_hdr = flow_act.modify_hdr;
	ipsec_rule->pkt_reformat = flow_act.pkt_reformat;
	return 0;

err_add_flow:
	mlx5_fc_destroy(mdev, counter);
	if (ipsec_rule->rule != NULL)
		mlx5_del_flow_rules(&ipsec_rule->rule);
err_add_cnt:
	mlx5_packet_reformat_dealloc(mdev, flow_act.pkt_reformat);
err_pkt_reformat:
	if (flow_act.modify_hdr != NULL)
		mlx5_modify_header_dealloc(mdev, flow_act.modify_hdr);
err_mod_header:
	kvfree(spec);

	return err;
}

static struct mlx5_flow_table *ipsec_tx_ft_create(struct mlx5_flow_namespace *ns,
						  int level, int prio,
						  int max_num_groups)
{
	struct mlx5_flow_table_attr ft_attr = {};

        ft_attr.autogroup.num_reserved_entries = 1;
        ft_attr.autogroup.max_num_groups = max_num_groups;
        ft_attr.max_fte = NUM_IPSEC_FTE;
        ft_attr.level = level;
        ft_attr.prio = prio;

	return mlx5_create_auto_grouped_flow_table(ns, &ft_attr);
}

static int ipsec_counter_rule_tx(struct mlx5_core_dev *mdev, struct mlx5e_ipsec_tx *tx)
{
	struct mlx5_flow_destination dest = {};
	struct mlx5_flow_act flow_act = {};
	struct mlx5_flow_handle *fte;
	int err;

	/* create fte */
	flow_act.action = MLX5_FLOW_CONTEXT_ACTION_COUNT |
		MLX5_FLOW_CONTEXT_ACTION_ALLOW;

	dest.type = MLX5_FLOW_DESTINATION_TYPE_COUNTER;
	dest.counter_id = mlx5_fc_id(tx->fc->cnt);
	fte = mlx5_add_flow_rules(tx->ft.status, NULL, &flow_act, &dest, 1);
	if (IS_ERR_OR_NULL(fte)) {
		err = PTR_ERR(fte);
		mlx5_core_err(mdev, "Fail to add ipsec tx counter rule err=%d\n", err);
		goto err_rule;
	}

	tx->status.rule = fte;
	return 0;

err_rule:
	return err;
}

static void tx_destroy_roce(struct mlx5e_ipsec_tx *tx) {
	if (!tx->roce.ft)
		return;

	mlx5_del_flow_rules(&tx->roce.rule);
	mlx5_destroy_flow_group(tx->roce.g);
	mlx5_destroy_flow_table(tx->roce.ft);
	tx->roce.ft = NULL;
}

/* IPsec TX flow steering */
static void tx_destroy(struct mlx5e_ipsec_tx *tx)
{
	tx_destroy_roce(tx);
	if (tx->chains) {
		ipsec_chains_destroy(tx->chains);
	} else {
		mlx5_del_flow_rules(&tx->pol.rule);
		mlx5_destroy_flow_group(tx->pol.group);
		mlx5_destroy_flow_table(tx->ft.pol);
	}
	mlx5_destroy_flow_table(tx->ft.sa);
	mlx5_del_flow_rules(&tx->kspi_miss.rule);
	mlx5_destroy_flow_group(tx->kspi_miss.group);
	mlx5_del_flow_rules(&tx->kspi_bypass_rule.rule);
	mlx5_del_flow_rules(&tx->kspi_bypass_rule.kspi_rule);
	mlx5_destroy_flow_table(tx->ft.sa_kspi);
	mlx5_del_flow_rules(&tx->status.rule);
	mlx5_destroy_flow_table(tx->ft.status);
}

static int ipsec_tx_roce_rule_setup(struct mlx5_core_dev *mdev,
				    struct mlx5e_ipsec_tx *tx)
{
	struct mlx5_flow_destination dst = {};
	struct mlx5_flow_act flow_act = {};
	struct mlx5_flow_handle *rule;
	int err = 0;

	flow_act.action = MLX5_FLOW_CONTEXT_ACTION_FWD_DEST;
	dst.type = MLX5_FLOW_DESTINATION_TYPE_TABLE_TYPE;
	dst.ft = tx->ft.pol;
	rule = mlx5_add_flow_rules(tx->roce.ft, NULL, &flow_act, &dst, 1);
	if (IS_ERR(rule)) {
		err = PTR_ERR(rule);
		mlx5_core_err(mdev, "Fail to add TX roce ipsec rule err=%d\n",
			      err);
		goto out;
	}
	tx->roce.rule = rule;

out:
	return err;
}

static int ipsec_tx_create_roce(struct mlx5_core_dev *mdev, struct mlx5e_ipsec_tx *tx)
{
	struct mlx5_flow_table_attr ft_attr = {};
	struct mlx5_flow_table *ft;
	struct mlx5_flow_group *g;
	int ix = 0;
	int err;
	u32 *in;

	if (!tx->roce.ns)
		return -EOPNOTSUPP;

	in = kvzalloc(MLX5_ST_SZ_BYTES(create_flow_group_in), GFP_KERNEL);
	if (!in)
		return -ENOMEM;

	ft_attr.max_fte = 1;
	ft = mlx5_create_flow_table(tx->roce.ns, &ft_attr);
	if (IS_ERR(ft)) {
		err = PTR_ERR(ft);
		mlx5_core_err(mdev, "Fail to create ipsec tx roce ft err=%d\n",
			      err);
		goto fail_table;
	}
	tx->roce.ft = ft;

	MLX5_SET_CFG(in, start_flow_index, ix);
	ix += 1;
	MLX5_SET_CFG(in, end_flow_index, ix - 1);
	g = mlx5_create_flow_group(ft, in);
	if (IS_ERR(g)) {
		err = PTR_ERR(g);
		mlx5_core_err(mdev, "Fail to create ipsec tx roce group err=%d\n",
			      err);
		goto fail_group;
	}
	tx->roce.g = g;

	err = ipsec_tx_roce_rule_setup(mdev, tx);
	if (err) {
		mlx5_core_err(mdev, "Fail to create RoCE IPsec tx rules err=%d\n", err);
		goto fail_rule;
	}

	kvfree(in);
	return 0;

fail_rule:
	mlx5_destroy_flow_group(tx->roce.g);
fail_group:
	mlx5_destroy_flow_table(tx->roce.ft);
	tx->roce.ft = NULL;
fail_table:
	kvfree(in);
	return err;
}

/*
 * Setting a rule in KSPI table for values that should bypass IPSEC.
 *
 * mdev - mlx5 core device
 * tx - IPSEC TX
 * return - 0 for success errno for failure
 */
static int tx_create_kspi_bypass_rules(struct mlx5_core_dev *mdev,
                                       struct mlx5e_ipsec_tx *tx)
{
	struct mlx5_flow_destination dest = {};
	struct mlx5_flow_act flow_act = {};
	struct mlx5_flow_act flow_act_kspi = {};
	struct mlx5_flow_handle *rule;
	struct mlx5_flow_spec *spec;
	int err;

	spec = kvzalloc(sizeof(*spec), GFP_KERNEL);
	if (!spec)
		return -ENOMEM;

	dest.ft = tx->ft.status;
	dest.type = MLX5_FLOW_DESTINATION_TYPE_FLOW_TABLE;
	flow_act_kspi.action |= MLX5_FLOW_CONTEXT_ACTION_FWD_DEST;

	setup_fte_reg_a_with_tag(spec, IPSEC_ACCEL_DRV_SPI_BYPASS);
	rule = mlx5_add_flow_rules(tx->ft.sa_kspi, spec, &flow_act_kspi,
								&dest, 1);
	if (IS_ERR(rule)) {
		err = PTR_ERR(rule);
		mlx5_core_err(mdev, "Fail to add ipsec kspi bypass rule err=%d\n",
                      err);
		goto err_add_kspi_rule;
	}
	tx->kspi_bypass_rule.kspi_rule = rule;

	/* set the rule for packets withoiut ipsec tag. */
	flow_act.action |= MLX5_FLOW_CONTEXT_ACTION_FWD_DEST;
	memset(spec, 0, sizeof(*spec));
	setup_fte_reg_a_no_tag(spec);
	rule = mlx5_add_flow_rules(tx->ft.sa_kspi, spec, &flow_act, &dest, 1);
	if (IS_ERR(rule)) {
		err = PTR_ERR(rule);
		mlx5_core_err(mdev, "Fail to add ipsec kspi bypass rule err=%d\n", err);
		goto err_add_rule;
	}
	tx->kspi_bypass_rule.rule = rule;

	kvfree(spec);
	return 0;
err_add_rule:
	mlx5_del_flow_rules(&tx->kspi_bypass_rule.kspi_rule);
err_add_kspi_rule:
	kvfree(spec);
	return err;
}


static int tx_create(struct mlx5_core_dev *mdev, struct mlx5e_ipsec_tx *tx)
{
	struct mlx5_flow_destination dest = {};
	struct mlx5_flow_table *ft;
	int err;

	/*
	 *  Tx flow is different for ethernet traffic then for RoCE packets
	 *  For Ethernet packets we start in SA KSPI table that matches KSPI of SA rule
	 *  to the KSPI in the packet metadata
	 *  For RoCE traffic we start in Policy table, then move to SA table
	 *  which matches either reqid of the SA rule to reqid reported by policy table
	 *  or ip header fields of SA to the packet IP header fields.
	 *  Tables are ordered by their level so we set kspi
	 *  with level 0 to have it first one for ethernet traffic.
	 *  For RoCE the RoCE TX table direct the packets to policy table explicitly
	 */
	ft = ipsec_tx_ft_create(tx->ns, 0, 0, 4);
	if (IS_ERR(ft))
		return PTR_ERR(ft);
	tx->ft.sa_kspi = ft;

	ft = ipsec_tx_ft_create(tx->ns, 2, 0, 4);
	if (IS_ERR(ft)) {
		err = PTR_ERR(ft);
		goto err_reqid_ft;
	}
	tx->ft.sa = ft;

	if (mlx5_ipsec_device_caps(mdev) & MLX5_IPSEC_CAP_PRIO) {
		tx->chains = ipsec_chains_create(
				mdev, tx->ft.sa, MLX5_FLOW_NAMESPACE_EGRESS_IPSEC, 0, 1,
				&tx->ft.pol);
		if (IS_ERR(tx->chains)) {
			err = PTR_ERR(tx->chains);
			goto err_pol_ft;
		}
	} else {
		ft = ipsec_tx_ft_create(tx->ns, 1, 0, 2);
		if (IS_ERR(ft)) {
			err = PTR_ERR(ft);
			goto err_pol_ft;
		}
		tx->ft.pol = ft;
		dest.type = MLX5_FLOW_DESTINATION_TYPE_FLOW_TABLE;
		dest.ft = tx->ft.sa;
		err = ipsec_miss_create(mdev, tx->ft.pol, &tx->pol, &dest);
		if (err)
			goto err_pol_miss;
	}

	ft = ipsec_tx_ft_create(tx->ns, 2, 0, 1);
	if (IS_ERR(ft)) {
		err = PTR_ERR(ft);
		goto err_status_ft;
	}
	tx->ft.status = ft;

	/* set miss rule for kspi table with drop action*/
	err = ipsec_miss_create(mdev, tx->ft.sa_kspi, &tx->kspi_miss, NULL);
	if (err)
		goto err_kspi_miss;

	err = tx_create_kspi_bypass_rules(mdev, tx);
	if (err)
		goto err_kspi_rule;

	err = ipsec_counter_rule_tx(mdev, tx);
	if (err)
		goto err_status_rule;

	err = ipsec_tx_create_roce(mdev, tx);
	if (err)
		goto err_counter_rule;

	return 0;

err_counter_rule:
	mlx5_del_flow_rules(&tx->status.rule);
err_status_rule:
	mlx5_del_flow_rules(&tx->kspi_bypass_rule.rule);
	mlx5_del_flow_rules(&tx->kspi_bypass_rule.kspi_rule);
err_kspi_rule:
	mlx5_destroy_flow_table(tx->ft.status);
err_status_ft:
	if (tx->chains) {
		ipsec_chains_destroy(tx->chains);
	} else {
		mlx5_del_flow_rules(&tx->pol.rule);
		mlx5_destroy_flow_group(tx->pol.group);
	}
err_pol_miss:
	if (!tx->chains)
		mlx5_destroy_flow_table(tx->ft.pol);
err_pol_ft:
	mlx5_del_flow_rules(&tx->kspi_miss.rule);
	mlx5_destroy_flow_group(tx->kspi_miss.group);
err_kspi_miss:
	mlx5_destroy_flow_table(tx->ft.sa);
err_reqid_ft:
	mlx5_destroy_flow_table(tx->ft.sa_kspi);
	return err;
}

static int tx_get(struct mlx5_core_dev *mdev, struct mlx5e_ipsec *ipsec,
		  struct mlx5e_ipsec_tx *tx)
{
	int err;

	if (tx->ft.refcnt)
		goto skip;

	err = tx_create(mdev, tx);
	if (err)
		return err;

skip:
	tx->ft.refcnt++;
	return 0;
}

static void tx_put(struct mlx5e_ipsec *ipsec, struct mlx5e_ipsec_tx *tx)
{
	if (--tx->ft.refcnt)
		return;

	tx_destroy(tx);
}

static struct mlx5e_ipsec_tx *tx_ft_get(struct mlx5_core_dev *mdev,
					struct mlx5e_ipsec *ipsec)
{
	struct mlx5e_ipsec_tx *tx = ipsec->tx;
	int err;

	mutex_lock(&tx->ft.mutex);
	err = tx_get(mdev, ipsec, tx);
	mutex_unlock(&tx->ft.mutex);
	if (err)
		return ERR_PTR(err);

	return tx;
}

static struct mlx5_flow_table *tx_ft_get_policy(struct mlx5_core_dev *mdev,
                                                struct mlx5e_ipsec *ipsec,
                                                u32 prio)
{
        struct mlx5e_ipsec_tx *tx = ipsec->tx;
        struct mlx5_flow_table *ft;
        int err;

        mutex_lock(&tx->ft.mutex);
        err = tx_get(mdev, ipsec, tx);
        if (err)
            goto err_get;

        ft = tx->chains ? ipsec_chains_get_table(tx->chains, prio) : tx->ft.pol;
        if (IS_ERR(ft)) {
                err = PTR_ERR(ft);
                goto err_get_ft;
        }

        mutex_unlock(&tx->ft.mutex);
        return ft;

err_get_ft:
        tx_put(ipsec, tx);
err_get:
        mutex_unlock(&tx->ft.mutex);
        return ERR_PTR(err);
}

static void tx_ft_put_policy(struct mlx5e_ipsec *ipsec, u32 prio)
{
        struct mlx5e_ipsec_tx *tx = ipsec->tx;

        mutex_lock(&tx->ft.mutex);
        if (tx->chains)
                ipsec_chains_put_table(tx->chains, prio);

        tx_put(ipsec, tx);
        mutex_unlock(&tx->ft.mutex);
}

static void tx_ft_put(struct mlx5e_ipsec *ipsec)
{
	struct mlx5e_ipsec_tx *tx = ipsec->tx;

	mutex_lock(&tx->ft.mutex);
	tx_put(ipsec, tx);
	mutex_unlock(&tx->ft.mutex);
}

static void setup_fte_reg_a_with_tag(struct mlx5_flow_spec *spec,
									 u16 kspi)
{
       /* Add IPsec indicator in metadata_reg_a. */
       spec->match_criteria_enable |= MLX5_MATCH_MISC_PARAMETERS_2;

       MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria,
                        misc_parameters_2.metadata_reg_a);
       MLX5_SET(fte_match_param, spec->match_value,
                misc_parameters_2.metadata_reg_a,
                MLX5_ETH_WQE_FT_META_IPSEC << 23 |  kspi);
}

static void setup_fte_reg_a_no_tag(struct mlx5_flow_spec *spec)
{
       /* Add IPsec indicator in metadata_reg_a. */
       spec->match_criteria_enable |= MLX5_MATCH_MISC_PARAMETERS_2;

       MLX5_SET(fte_match_param, spec->match_criteria,
                misc_parameters_2.metadata_reg_a,
				MLX5_ETH_WQE_FT_META_IPSEC << 23);
       MLX5_SET(fte_match_param, spec->match_value,
                misc_parameters_2.metadata_reg_a,
                0);
}

static void setup_fte_reg_c0(struct mlx5_flow_spec *spec, u32 reqid)
{
	/* Pass policy check before choosing this SA */
	spec->match_criteria_enable |= MLX5_MATCH_MISC_PARAMETERS_2;

	MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria,
			 misc_parameters_2.metadata_reg_c_0);
	MLX5_SET(fte_match_param, spec->match_value,
		 misc_parameters_2.metadata_reg_c_0, reqid);
}

static void setup_fte_upper_proto_match(struct mlx5_flow_spec *spec, struct upspec *upspec)
{
        switch (upspec->proto) {
        case IPPROTO_UDP:
                if (upspec->dport) {
                        MLX5_SET_TO_ONES(fte_match_set_lyr_2_4,
                                         spec->match_criteria, udp_dport);
                        MLX5_SET(fte_match_set_lyr_2_4, spec->match_value,
                                 udp_dport, upspec->dport);
                }

                if (upspec->sport) {
                        MLX5_SET_TO_ONES(fte_match_set_lyr_2_4,
                                         spec->match_criteria, udp_sport);
                        MLX5_SET(fte_match_set_lyr_2_4, spec->match_value,
                                 udp_dport, upspec->sport);
                }
                break;
        case IPPROTO_TCP:
                if (upspec->dport) {
                        MLX5_SET_TO_ONES(fte_match_set_lyr_2_4,
                                         spec->match_criteria, tcp_dport);
                        MLX5_SET(fte_match_set_lyr_2_4, spec->match_value,
                                 tcp_dport, upspec->dport);
                }

                if (upspec->sport) {
                        MLX5_SET_TO_ONES(fte_match_set_lyr_2_4,
                                         spec->match_criteria, tcp_sport);
                        MLX5_SET(fte_match_set_lyr_2_4, spec->match_value,
                                 tcp_dport, upspec->sport);
                }
                break;
        default:
                return;
        }

        spec->match_criteria_enable |= MLX5_MATCH_OUTER_HEADERS;
	MLX5_SET_TO_ONES(fte_match_set_lyr_2_4, spec->match_criteria, ip_protocol);
	MLX5_SET(fte_match_set_lyr_2_4, spec->match_value, ip_protocol, upspec->proto);
}

static int tx_add_kspi_rule(struct mlx5e_ipsec_sa_entry *sa_entry,
							struct mlx5e_ipsec_tx *tx,
							struct mlx5_flow_act *flow_act,
							struct mlx5_flow_destination *dest,
							int num_dest)
{
	struct mlx5e_ipsec_rule *ipsec_rule = &sa_entry->ipsec_rule;
	struct mlx5_core_dev *mdev = mlx5e_ipsec_sa2dev(sa_entry);
	struct mlx5_flow_handle *rule;
	struct mlx5_flow_spec *spec;
	int err;

	spec = kvzalloc(sizeof(*spec), GFP_KERNEL);
	if (!spec)
		return -ENOMEM;

	setup_fte_no_frags(spec);
	setup_fte_reg_a_with_tag(spec, sa_entry->kspi);

	rule = mlx5_add_flow_rules(tx->ft.sa_kspi, spec, flow_act, dest, num_dest);
	if (IS_ERR(rule)) {
		err = PTR_ERR(rule);
		mlx5_core_err(mdev, "fail to add TX ipsec kspi rule err=%d\n", err);
		goto err_add_kspi_flow;
	}
	ipsec_rule->kspi_rule = rule;
	kvfree(spec);
	return 0;

err_add_kspi_flow:
	kvfree(spec);
	return err;
}

static int tx_add_reqid_ip_rules(struct mlx5e_ipsec_sa_entry *sa_entry,
								struct mlx5e_ipsec_tx *tx,
								struct mlx5_flow_act *flow_act,
								struct mlx5_flow_destination *dest,
								int num_dest)
{
	struct mlx5e_ipsec_rule *ipsec_rule = &sa_entry->ipsec_rule;
	struct mlx5_accel_esp_xfrm_attrs *attrs = &sa_entry->attrs;
	struct mlx5_core_dev *mdev = mlx5e_ipsec_sa2dev(sa_entry);
	struct mlx5_flow_handle *rule;
	struct mlx5_flow_spec *spec;
	int err;

	spec = kvzalloc(sizeof(*spec), GFP_KERNEL);
	if (!spec)
		return -ENOMEM;

	flow_act->flags |= FLOW_ACT_IGNORE_FLOW_LEVEL;

	if(attrs->reqid) {
		setup_fte_no_frags(spec);
		setup_fte_reg_c0(spec, attrs->reqid);
		rule = mlx5_add_flow_rules(tx->ft.sa, spec, flow_act, dest, num_dest);
		if (IS_ERR(rule)) {
			err = PTR_ERR(rule);
			mlx5_core_err(mdev, "fail to add TX ipsec reqid rule err=%d\n", err);
			goto err_add_reqid_rule;
		}
		ipsec_rule->reqid_rule = rule;
		memset(spec, 0, sizeof(*spec));
	}

	if (attrs->family == AF_INET)
		setup_fte_addr4(spec, &attrs->saddr.a4, &attrs->daddr.a4);
	else
		setup_fte_addr6(spec, attrs->saddr.a6, attrs->daddr.a6);
	setup_fte_no_frags(spec);

	rule = mlx5_add_flow_rules(tx->ft.sa, spec, flow_act, dest, num_dest);
	if (IS_ERR(rule)) {
		err = PTR_ERR(rule);
		mlx5_core_err(mdev, "fail to add TX ipsec ip rule err=%d\n", err);
		goto err_add_ip_rule;
	}
	ipsec_rule->rule = rule;
	kvfree(spec);
	return 0;

err_add_ip_rule:
	mlx5_del_flow_rules(&ipsec_rule->reqid_rule);
err_add_reqid_rule:
	kvfree(spec);
	return err;
}

static int tx_add_rule(struct mlx5e_ipsec_sa_entry *sa_entry)
{
	struct mlx5e_ipsec_rule *ipsec_rule = &sa_entry->ipsec_rule;
	struct mlx5_accel_esp_xfrm_attrs *attrs = &sa_entry->attrs;
	struct mlx5_core_dev *mdev = mlx5e_ipsec_sa2dev(sa_entry);
	struct mlx5e_ipsec *ipsec = sa_entry->ipsec;
	struct mlx5_flow_destination dest[2] = {};
	struct mlx5_flow_act flow_act = {};
	struct mlx5e_ipsec_tx *tx;
	struct mlx5_fc *counter;
	int err;

	tx = tx_ft_get(mdev, ipsec);
	if (IS_ERR(tx))
		return PTR_ERR(tx);

	err = setup_pkt_reformat(mdev, attrs, &flow_act);
	if (err)
		goto err_pkt_reformat;

	counter = mlx5_fc_create(mdev, false);
	if (IS_ERR(counter)) {
		err = PTR_ERR(counter);
		goto err_add_cnt;
	}

	flow_act.crypto.type = MLX5_FLOW_CONTEXT_ENCRYPT_DECRYPT_TYPE_IPSEC;
        flow_act.crypto.obj_id = sa_entry->ipsec_obj_id;
        flow_act.flags |= FLOW_ACT_NO_APPEND;
        flow_act.action |= MLX5_FLOW_CONTEXT_ACTION_CRYPTO_ENCRYPT |
                           MLX5_FLOW_CONTEXT_ACTION_COUNT;

	if (attrs->drop)
		flow_act.action |= MLX5_FLOW_CONTEXT_ACTION_DROP;
	else
		flow_act.action |= MLX5_FLOW_CONTEXT_ACTION_FWD_DEST;

	dest[0].ft = tx->ft.status;
	dest[0].type = MLX5_FLOW_DESTINATION_TYPE_FLOW_TABLE;
	dest[1].type = MLX5_FLOW_DESTINATION_TYPE_COUNTER;
	dest[1].counter_id = mlx5_fc_id(counter);

	err = tx_add_kspi_rule(sa_entry, tx, &flow_act, dest, 2);
	if (err) {
		goto err_add_kspi_rule;
	}

	err = tx_add_reqid_ip_rules(sa_entry, tx, &flow_act, dest, 2);
	if (err) {
		goto err_add_reqid_ip_rule;
	}

	ipsec_rule->fc = counter;
	ipsec_rule->pkt_reformat = flow_act.pkt_reformat;
	return 0;

err_add_reqid_ip_rule:
	mlx5_del_flow_rules(&ipsec_rule->kspi_rule);
err_add_kspi_rule:
	mlx5_fc_destroy(mdev, counter);
err_add_cnt:
	if (flow_act.pkt_reformat)
		mlx5_packet_reformat_dealloc(mdev, flow_act.pkt_reformat);
err_pkt_reformat:
	tx_ft_put(ipsec);
	return err;
}

static int tx_add_policy(struct mlx5e_ipsec_pol_entry *pol_entry)
{
        struct mlx5_accel_pol_xfrm_attrs *attrs = &pol_entry->attrs;
        struct mlx5_core_dev *mdev = mlx5e_ipsec_pol2dev(pol_entry);
        struct mlx5e_ipsec_tx *tx = pol_entry->ipsec->tx;
        struct mlx5_flow_destination dest[2] = {};
        struct mlx5_flow_act flow_act = {};
        struct mlx5_flow_handle *rule;
        struct mlx5_flow_spec *spec;
        struct mlx5_flow_table *ft;
        int err, dstn = 0;

        ft = tx_ft_get_policy(mdev, pol_entry->ipsec, attrs->prio);
        if (IS_ERR(ft))
            return PTR_ERR(ft);

        spec = kvzalloc(sizeof(*spec), GFP_KERNEL);
        if (!spec) {
            err = -ENOMEM;
            goto err_alloc;
        }

        if (attrs->family == AF_INET)
                setup_fte_addr4(spec, &attrs->saddr.a4, &attrs->daddr.a4);
        else
                setup_fte_addr6(spec, attrs->saddr.a6, attrs->daddr.a6);

        setup_fte_no_frags(spec);
	setup_fte_upper_proto_match(spec, &attrs->upspec);

        switch (attrs->action) {
        case IPSEC_POLICY_IPSEC:
                flow_act.action |= MLX5_FLOW_CONTEXT_ACTION_FWD_DEST;
                err = setup_modify_header(mdev, attrs->reqid,
                                          IPSEC_DIR_OUTBOUND, &flow_act);
                if (err)
                        goto err_mod_header;
                 break;
        case IPSEC_POLICY_DISCARD:
                flow_act.action |= MLX5_FLOW_CONTEXT_ACTION_DROP |
                                   MLX5_FLOW_CONTEXT_ACTION_COUNT;
                dest[dstn].type = MLX5_FLOW_DESTINATION_TYPE_COUNTER;
                dest[dstn].counter_id = mlx5_fc_id(tx->fc->drop);
                dstn++;
                break;
        default:
                err = -EINVAL;
                goto err_mod_header;
        }

        flow_act.flags |= FLOW_ACT_NO_APPEND;
        dest[dstn].ft = tx->ft.sa;
        dest[dstn].type = MLX5_FLOW_DESTINATION_TYPE_FLOW_TABLE;
        dstn++;
        rule = mlx5_add_flow_rules(ft, spec, &flow_act, dest, dstn);
        if (IS_ERR(rule)) {
                err = PTR_ERR(rule);
                mlx5_core_err(mdev, "fail to add TX ipsec rule err=%d\n", err);
                goto err_action;
        }

        kvfree(spec);
        pol_entry->ipsec_rule.rule = rule;
        pol_entry->ipsec_rule.modify_hdr = flow_act.modify_hdr;
        return 0;

err_action:
        if (flow_act.modify_hdr)
                mlx5_modify_header_dealloc(mdev, flow_act.modify_hdr);
err_mod_header:
        kvfree(spec);
err_alloc:
        tx_ft_put_policy(pol_entry->ipsec, attrs->prio);
        return err;
}

static int rx_add_policy(struct mlx5e_ipsec_pol_entry *pol_entry)
{
        struct mlx5_accel_pol_xfrm_attrs *attrs = &pol_entry->attrs;
        struct mlx5_core_dev *mdev = mlx5e_ipsec_pol2dev(pol_entry);
	struct mlx5e_ipsec *ipsec = pol_entry->ipsec;
        struct mlx5_flow_destination dest[2];
        struct mlx5_flow_act flow_act = {};
        struct mlx5_flow_handle *rule;
        struct mlx5_flow_spec *spec;
        struct mlx5_flow_table *ft;
        struct mlx5e_ipsec_rx *rx;
	int err, dstn = 0;

        rx = (attrs->family == AF_INET) ? ipsec->rx_ipv4 : ipsec->rx_ipv6;
        ft = rx->chains ? ipsec_chains_get_table(rx->chains, attrs->prio) : rx->ft.pol;
        if (IS_ERR(ft))
                return PTR_ERR(ft);

        spec = kvzalloc(sizeof(*spec), GFP_KERNEL);
        if (!spec) {
                err = -ENOMEM;
                goto err_alloc;
        }

        switch (attrs->action) {
        case IPSEC_POLICY_IPSEC:
                flow_act.action |= MLX5_FLOW_CONTEXT_ACTION_FWD_DEST;
                break;
        case IPSEC_POLICY_DISCARD:
                flow_act.action |= MLX5_FLOW_CONTEXT_ACTION_DROP | MLX5_FLOW_CONTEXT_ACTION_COUNT;
                dest[dstn].type = MLX5_FLOW_DESTINATION_TYPE_COUNTER;
                dest[dstn].counter_id = mlx5_fc_id(rx->fc->drop);
                dstn++;
                break;
        default:
                err = -EINVAL;
                goto err_action;
        }

        flow_act.flags |= FLOW_ACT_NO_APPEND;
        dest[dstn].type = MLX5_FLOW_DESTINATION_TYPE_FLOW_TABLE;
        dest[dstn].ft = rx->ft.sa;
        dstn++;

	if (attrs->family == AF_INET)
		setup_fte_addr4(spec, &attrs->saddr.a4, &attrs->daddr.a4);
	else
		setup_fte_addr6(spec, attrs->saddr.a6, attrs->daddr.a6);

	setup_fte_no_frags(spec);
	setup_fte_upper_proto_match(spec, &attrs->upspec);
	if (attrs->vid != VLAN_NONE)
		setup_fte_vid(spec, attrs->vid);
	else
		setup_fte_no_vid(spec);

	rule = mlx5_add_flow_rules(ft, spec, &flow_act, dest, dstn);
	if (IS_ERR(rule)) {
		err = PTR_ERR(rule);
		mlx5_core_err(mdev,
		    "Failed to add RX IPsec policy rule err=%d\n", err);
		goto err_action;
	}
	pol_entry->ipsec_rule.rule = rule;

	/* Add also rule for zero vid */
	if (attrs->vid == VLAN_NONE) {
		clear_fte_vid(spec);
		setup_fte_vid(spec, 0);
		rule = mlx5_add_flow_rules(ft, spec, &flow_act, dest, dstn);
		if (IS_ERR(rule)) {
			err = PTR_ERR(rule);
			mlx5_core_err(mdev,
			    "Failed to add RX IPsec policy rule err=%d\n",
			    err);
			goto err_action;
		}
		pol_entry->ipsec_rule.vid_zero_rule = rule;
	}

	kvfree(spec);
        return 0;

err_action:
	if (pol_entry->ipsec_rule.rule != NULL)
		mlx5_del_flow_rules(&pol_entry->ipsec_rule.rule);
	kvfree(spec);
err_alloc:
        if (rx->chains != NULL)
                ipsec_chains_put_table(rx->chains, attrs->prio);
        return err;
}

static void ipsec_fs_destroy_counters(struct mlx5e_ipsec *ipsec)
{
	struct mlx5e_ipsec_rx *rx_ipv4 = ipsec->rx_ipv4;
	struct mlx5_core_dev *mdev = ipsec->mdev;
	struct mlx5e_ipsec_tx *tx = ipsec->tx;

	mlx5_fc_destroy(mdev, rx_ipv4->fc->drop);
	mlx5_fc_destroy(mdev, rx_ipv4->fc->cnt);
	kfree(rx_ipv4->fc);
	mlx5_fc_destroy(mdev, tx->fc->drop);
	mlx5_fc_destroy(mdev, tx->fc->cnt);
	kfree(tx->fc);
}

static int ipsec_fs_init_counters(struct mlx5e_ipsec *ipsec)
{
	struct mlx5e_ipsec_rx *rx_ipv4 = ipsec->rx_ipv4;
	struct mlx5e_ipsec_rx *rx_ipv6 = ipsec->rx_ipv6;
	struct mlx5_core_dev *mdev = ipsec->mdev;
	struct mlx5e_ipsec_tx *tx = ipsec->tx;
	struct mlx5e_ipsec_fc *fc;
	struct mlx5_fc *counter;
	int err;

	fc = kzalloc(sizeof(*tx->fc), GFP_KERNEL);
	if (!fc)
		return -ENOMEM;

	tx->fc = fc;
	counter = mlx5_fc_create(mdev, false);
	if (IS_ERR(counter)) {
		err = PTR_ERR(counter);
		goto err_tx_fc_alloc;
	}

	fc->cnt = counter;
	counter = mlx5_fc_create(mdev, false);
	if (IS_ERR(counter)) {
		err = PTR_ERR(counter);
		goto err_tx_fc_cnt;
	}

	fc->drop = counter;

	fc = kzalloc(sizeof(*tx->fc), GFP_KERNEL);
	if (!fc) {
		err = -ENOMEM;
		goto err_tx_fc_drop;
	}

	/* Both IPv4 and IPv6 point to same flow counters struct. */
	rx_ipv4->fc = fc;
	rx_ipv6->fc = fc;
	counter = mlx5_fc_create(mdev, false);
	if (IS_ERR(counter)) {
		err = PTR_ERR(counter);
		goto err_rx_fc_alloc;
	}

	fc->cnt = counter;
	counter = mlx5_fc_create(mdev, false);
	if (IS_ERR(counter)) {
		err = PTR_ERR(counter);
		goto err_rx_fc_cnt;
	}

	fc->drop = counter;
	return 0;

err_rx_fc_cnt:
	mlx5_fc_destroy(mdev, rx_ipv4->fc->cnt);
err_rx_fc_alloc:
	kfree(rx_ipv4->fc);
err_tx_fc_drop:
	mlx5_fc_destroy(mdev, tx->fc->drop);
err_tx_fc_cnt:
	mlx5_fc_destroy(mdev, tx->fc->cnt);
err_tx_fc_alloc:
	kfree(tx->fc);
	return err;
}

static int ipsec_status_rule(struct mlx5_core_dev *mdev,
			     struct mlx5e_ipsec_rx *rx,
			     struct mlx5_flow_destination *dest)
{
	u8 action[MLX5_UN_SZ_BYTES(set_add_copy_action_in_auto)] = {};
	struct mlx5_flow_act flow_act = {};
	struct mlx5_modify_hdr *modify_hdr;
	struct mlx5_flow_handle *rule;
	struct mlx5_flow_spec *spec;
	int err;

	spec = kvzalloc(sizeof(*spec), GFP_KERNEL);
	if (!spec)
		return -ENOMEM;

	/* Action to copy 7 bit ipsec_syndrome to regB[24:30] */
	MLX5_SET(copy_action_in, action, action_type, MLX5_ACTION_TYPE_COPY);
	MLX5_SET(copy_action_in, action, src_field, MLX5_ACTION_IN_FIELD_IPSEC_SYNDROME);
	MLX5_SET(copy_action_in, action, src_offset, 0);
	MLX5_SET(copy_action_in, action, length, 7);
	MLX5_SET(copy_action_in, action, dst_field, MLX5_ACTION_IN_FIELD_METADATA_REG_B);
	MLX5_SET(copy_action_in, action, dst_offset, 24);

	modify_hdr = mlx5_modify_header_alloc(mdev, MLX5_FLOW_NAMESPACE_KERNEL,
					      1, action);

	if (IS_ERR(modify_hdr)) {
		err = PTR_ERR(modify_hdr);
		mlx5_core_err(mdev,
			      "fail to alloc ipsec copy modify_header_id err=%d\n", err);
		goto out_spec;
	}

	/* create fte */
	flow_act.action = MLX5_FLOW_CONTEXT_ACTION_MOD_HDR |
		MLX5_FLOW_CONTEXT_ACTION_FWD_DEST |
		MLX5_FLOW_CONTEXT_ACTION_COUNT;
	flow_act.modify_hdr = modify_hdr;

	rule = mlx5_add_flow_rules(rx->ft.status, spec, &flow_act, dest, 2);
	if (IS_ERR(rule)) {
		err = PTR_ERR(rule);
		mlx5_core_err(mdev, "fail to add ipsec rx err copy rule err=%d\n", err);
		goto out;
	}

	kvfree(spec);
	rx->status.rule = rule;
	rx->status.modify_hdr = modify_hdr;
	return 0;

out:
	mlx5_modify_header_dealloc(mdev, modify_hdr);
out_spec:
	kvfree(spec);
	return err;
}

static void ipsec_fs_rx_roce_rules_destroy(struct mlx5e_ipsec_rx_roce *rx_roce)
{
	if (!rx_roce->ns_rdma)
		return;

	mlx5_del_flow_rules(&rx_roce->roce_miss.rule);
	mlx5_del_flow_rules(&rx_roce->rule);
	mlx5_destroy_flow_group(rx_roce->roce_miss.group);
	mlx5_destroy_flow_group(rx_roce->g);
}

static void ipsec_fs_rx_catchall_rules_destroy(struct mlx5_core_dev *mdev, struct mlx5e_ipsec_rx *rx)
{
	mutex_lock(&rx->ft.mutex);
	mlx5_del_flow_rules(&rx->sa.rule);
	mlx5_destroy_flow_group(rx->sa.group);
	if (rx->chains == NULL) {
		mlx5_del_flow_rules(&rx->pol.rule);
		mlx5_destroy_flow_group(rx->pol.group);
	}
	mlx5_del_flow_rules(&rx->status.rule);
	mlx5_modify_header_dealloc(mdev, rx->status.modify_hdr);
	ipsec_fs_rx_roce_rules_destroy(&rx->roce);
	mutex_unlock(&rx->ft.mutex);
}

static void ipsec_fs_rx_roce_table_destroy(struct mlx5e_ipsec_rx_roce *rx_roce)
{
	if (!rx_roce->ns_rdma)
		return;

	mlx5_destroy_flow_table(rx_roce->ft_rdma);
	mlx5_destroy_flow_table(rx_roce->ft);
}

static void ipsec_fs_rx_table_destroy(struct mlx5_core_dev *mdev, struct mlx5e_ipsec_rx *rx)
{
	mutex_lock(&rx->ft.mutex);
	if (rx->chains) {
		ipsec_chains_destroy(rx->chains);
	} else {
		mlx5_del_flow_rules(&rx->pol.rule);
		mlx5_destroy_flow_table(rx->ft.pol);
        }
	mlx5_destroy_flow_table(rx->ft.sa);
	mlx5_destroy_flow_table(rx->ft.status);
	ipsec_fs_rx_roce_table_destroy(&rx->roce);
	mutex_unlock(&rx->ft.mutex);
}

static void ipsec_roce_setup_udp_dport(struct mlx5_flow_spec *spec, u16 dport)
{
	spec->match_criteria_enable |= MLX5_MATCH_OUTER_HEADERS;
	MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria, outer_headers.ip_protocol);
	MLX5_SET(fte_match_param, spec->match_value, outer_headers.ip_protocol, IPPROTO_UDP);
	MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria, outer_headers.udp_dport);
	MLX5_SET(fte_match_param, spec->match_value, outer_headers.udp_dport, dport);
}

static int ipsec_roce_rx_rule_setup(struct mlx5_flow_destination *default_dst,
				    struct mlx5e_ipsec_rx_roce *roce, struct mlx5_core_dev *mdev)
{
	struct mlx5_flow_destination dst = {};
	struct mlx5_flow_act flow_act = {};
	struct mlx5_flow_handle *rule;
	struct mlx5_flow_spec *spec;
	int err = 0;

	spec = kvzalloc(sizeof(*spec), GFP_KERNEL);
	if (!spec)
		return -ENOMEM;

	ipsec_roce_setup_udp_dport(spec, ROCE_V2_UDP_DPORT);

	//flow_act.action = MLX5_FLOW_CONTEXT_ACTION_FWD_DEST;//not needed it is added in command
	dst.type = MLX5_FLOW_DESTINATION_TYPE_TABLE_TYPE;
	dst.ft = roce->ft_rdma;

	flow_act.action = MLX5_FLOW_CONTEXT_ACTION_FWD_DEST;
	rule = mlx5_add_flow_rules(roce->ft, spec, &flow_act, &dst, 1);
	if (IS_ERR(rule)) {
		err = PTR_ERR(rule);
		mlx5_core_err(mdev, "Fail to add RX roce ipsec rule err=%d\n",
			      err);
		goto fail_add_rule;
	}

	roce->rule = rule;

	rule = mlx5_add_flow_rules(roce->ft, NULL, &flow_act, default_dst, 1);
	if (IS_ERR(rule)) {
		err = PTR_ERR(rule);
		mlx5_core_err(mdev, "Fail to add RX roce ipsec miss rule err=%d\n",
			      err);
		goto fail_add_default_rule;
	}

	roce->roce_miss.rule = rule;

	kvfree(spec);
	return 0;

fail_add_default_rule:
	mlx5_del_flow_rules(&roce->rule);
fail_add_rule:
	kvfree(spec);
	return err;
}

static int ipsec_roce_rx_rules(struct mlx5e_ipsec_rx *rx, struct mlx5_flow_destination *defdst,
			       struct mlx5_core_dev *mdev)
{
	int inlen = MLX5_ST_SZ_BYTES(create_flow_group_in);
	struct mlx5_flow_group *g;
	void *outer_headers_c;
	u32 *in;
	int err = 0;
	int ix = 0;
	u8 *mc;

	if (!rx->roce.ns_rdma)
		return 0;

	in = kvzalloc(inlen, GFP_KERNEL);
	if (!in)
		return -ENOMEM;

	mc = MLX5_ADDR_OF(create_flow_group_in, in, match_criteria);
	outer_headers_c = MLX5_ADDR_OF(fte_match_param, mc, outer_headers);
	MLX5_SET_TO_ONES(fte_match_set_lyr_2_4, outer_headers_c, ip_protocol);
	MLX5_SET_TO_ONES(fte_match_set_lyr_2_4, outer_headers_c, udp_dport);

	MLX5_SET_CFG(in, match_criteria_enable, MLX5_MATCH_OUTER_HEADERS);
	MLX5_SET_CFG(in, start_flow_index, ix);
	ix += 1;
	MLX5_SET_CFG(in, end_flow_index, ix - 1);
	g = mlx5_create_flow_group(rx->roce.ft, in);
	if (IS_ERR(g)) {
		err = PTR_ERR(g);
		mlx5_core_err(mdev, "Fail to create ipsec rx roce group at nic err=%d\n", err);
		goto fail_group;
	}
	rx->roce.g = g;

	memset(in, 0, MLX5_ST_SZ_BYTES(create_flow_group_in));
	MLX5_SET_CFG(in, start_flow_index, ix);
	ix += 1;
	MLX5_SET_CFG(in, end_flow_index, ix - 1);
	g = mlx5_create_flow_group(rx->roce.ft, in);
	if (IS_ERR(g)) {
		err = PTR_ERR(g);
		mlx5_core_err(mdev, "Fail to create ipsec rx roce miss group at nic err=%d\n",
			      err);
		goto fail_mgroup;
	}
	rx->roce.roce_miss.group = g;

	err = ipsec_roce_rx_rule_setup(defdst, &rx->roce, mdev);
	if (err)
		goto fail_setup_rule;

	kvfree(in);
	return 0;

fail_setup_rule:
	mlx5_destroy_flow_group(rx->roce.roce_miss.group);
fail_mgroup:
	mlx5_destroy_flow_group(rx->roce.g);
fail_group:
	kvfree(in);
	return err;
}

static int ipsec_fs_rx_catchall_rules(struct mlx5e_priv *priv,
				      struct mlx5e_ipsec_rx *rx,
				      struct mlx5_flow_destination *defdst)
{
	struct mlx5_core_dev *mdev = priv->mdev;
	struct mlx5_flow_destination dest[2] = {};
	int err = 0;

	mutex_lock(&rx->ft.mutex);
	/* IPsec RoCE RX rules */
	err = ipsec_roce_rx_rules(rx, defdst, mdev);
	if (err)
		goto out;

	/* IPsec Rx IP Status table rule */
	dest[0].type = MLX5_FLOW_DESTINATION_TYPE_FLOW_TABLE;
	if (rx->roce.ft)
		dest[0].ft = rx->roce.ft;
	else
		dest[0].ft = priv->fts.vlan.t;

	dest[1].type = MLX5_FLOW_DESTINATION_TYPE_COUNTER;
        dest[1].counter_id = mlx5_fc_id(rx->fc->cnt);
        err = ipsec_status_rule(mdev, rx, dest);
        if (err)
                goto err_roce_rules_destroy;

	if (!rx->chains) {
		/* IPsec Rx IP policy default miss rule */
		err = ipsec_miss_create(mdev, rx->ft.pol, &rx->pol, defdst);
		if (err)
			goto err_status_rule_destroy;
	}

	/* FIXME: This is workaround to current design
	 * which installs SA on firt packet. So we need to forward this
	 * packet to the stack. It doesn't work with RoCE and eswitch traffic,
	 */
	err = ipsec_miss_create(mdev, rx->ft.sa, &rx->sa, defdst);
	if (err)
		goto err_status_sa_rule_destroy;

	mutex_unlock(&rx->ft.mutex);
	return 0;

err_status_sa_rule_destroy:
	if (!rx->chains) {
		mlx5_del_flow_rules(&rx->pol.rule);
		mlx5_destroy_flow_group(rx->pol.group);
	}
err_status_rule_destroy:
	mlx5_del_flow_rules(&rx->status.rule);
	mlx5_modify_header_dealloc(mdev, rx->status.modify_hdr);
err_roce_rules_destroy:
	ipsec_fs_rx_roce_rules_destroy(&rx->roce);
out:
	mutex_unlock(&rx->ft.mutex);
	return err;
}

static int ipsec_fs_rx_roce_tables_create(struct mlx5e_ipsec_rx *rx,
					  int rx_init_level, int rdma_init_level)
{
	struct mlx5_flow_table_attr ft_attr = {};
	struct mlx5_flow_table *ft;
	int err = 0;

	if (!rx->roce.ns_rdma)
		return 0;

	ft_attr.max_fte = 2;
	ft_attr.level = rx_init_level;
	ft = mlx5_create_flow_table(rx->ns, &ft_attr);
	if (IS_ERR(ft)) {
		err = PTR_ERR(ft);
		return err;
	}
	rx->roce.ft = ft;

	ft_attr.max_fte = 0;
	ft_attr.level = rdma_init_level;
	ft = mlx5_create_flow_table(rx->roce.ns_rdma, &ft_attr);
	if (IS_ERR(ft)) {
		err = PTR_ERR(ft);
		goto out;
	}
	rx->roce.ft_rdma = ft;

	return 0;
out:
	mlx5_destroy_flow_table(rx->roce.ft);
	rx->roce.ft = NULL;
	return err;
}

static int ipsec_fs_rx_table_create(struct mlx5_core_dev *mdev, struct mlx5e_ipsec_rx *rx,
				    int rx_init_level, int rdma_init_level)
{
	struct mlx5_flow_namespace *ns = rx->ns;
	struct mlx5_flow_table *ft;
	int err = 0;

	mutex_lock(&rx->ft.mutex);

	/* IPsec Rx IP SA table create */
	ft = ipsec_rx_ft_create(ns, rx_init_level + 1, 0, 1);
	if (IS_ERR(ft)) {
		err = PTR_ERR(ft);
		goto out;
	}
	rx->ft.sa = ft;

	/* IPsec Rx IP Status table create */
	ft = ipsec_rx_ft_create(ns, rx_init_level + 2, 0, 1);
	if (IS_ERR(ft)) {
		err = PTR_ERR(ft);
		goto err_sa_table_destroy;
	}
	rx->ft.status = ft;

	if (mlx5_ipsec_device_caps(mdev) & MLX5_IPSEC_CAP_PRIO) {
		rx->chains = ipsec_chains_create(mdev, rx->ft.sa,
				MLX5_FLOW_NAMESPACE_KERNEL, 0,
				rx_init_level, &rx->ft.pol);
		if (IS_ERR(rx->chains)) {
			err = PTR_ERR(rx->chains);
			goto err_status_table_destroy;
		}
	} else {
		ft = ipsec_rx_ft_create(ns, rx_init_level, 0, 1);
		if (IS_ERR(ft)) {
			err = PTR_ERR(ft);
			goto err_status_table_destroy;
		}
		rx->ft.pol = ft;
	}

	/* IPsec RoCE RX tables create*/
	err = ipsec_fs_rx_roce_tables_create(rx, rx_init_level + 3,
					     rdma_init_level);
	if (err)
		goto err_pol_table_destroy;

	goto out;

err_pol_table_destroy:
	mlx5_destroy_flow_table(rx->ft.pol);
err_status_table_destroy:
	mlx5_destroy_flow_table(rx->ft.status);
err_sa_table_destroy:
	mlx5_destroy_flow_table(rx->ft.sa);
out:
	mutex_unlock(&rx->ft.mutex);
	return err;
}

#define NIC_RDMA_BOTH_DIRS_CAPS (MLX5_FT_NIC_RX_2_NIC_RX_RDMA | MLX5_FT_NIC_TX_RDMA_2_NIC_TX)

static void mlx5e_accel_ipsec_fs_init_roce(struct mlx5e_ipsec *ipsec)
{
	struct mlx5_core_dev *mdev = ipsec->mdev;
	struct mlx5_flow_namespace *ns;

	if ((MLX5_CAP_GEN_2(ipsec->mdev, flow_table_type_2_type) &
	      NIC_RDMA_BOTH_DIRS_CAPS) != NIC_RDMA_BOTH_DIRS_CAPS) {
		mlx5_core_dbg(mdev, "Failed to init roce ns, capabilities not supported\n");
		return;
	}

	ns = mlx5_get_flow_namespace(ipsec->mdev, MLX5_FLOW_NAMESPACE_RDMA_RX_IPSEC);
	if (!ns) {
		mlx5_core_err(mdev, "Failed to init roce rx ns\n");
		return;
	}

	ipsec->rx_ipv4->roce.ns_rdma = ns;
	ipsec->rx_ipv6->roce.ns_rdma = ns;

	ns = mlx5_get_flow_namespace(ipsec->mdev, MLX5_FLOW_NAMESPACE_RDMA_TX_IPSEC);
	if (!ns) {
		ipsec->rx_ipv4->roce.ns_rdma = NULL;
		ipsec->rx_ipv6->roce.ns_rdma = NULL;
		mlx5_core_err(mdev, "Failed to init roce tx ns\n");
		return;
	}

	ipsec->tx->roce.ns = ns;
}

int mlx5e_accel_ipsec_fs_add_rule(struct mlx5e_ipsec_sa_entry *sa_entry)
{
	if (sa_entry->attrs.dir == IPSEC_DIR_OUTBOUND)
		return tx_add_rule(sa_entry);

	return rx_add_rule(sa_entry);
}

void mlx5e_accel_ipsec_fs_del_rule(struct mlx5e_ipsec_sa_entry *sa_entry)
{
	struct mlx5e_ipsec_rule *ipsec_rule = &sa_entry->ipsec_rule;
	struct mlx5_core_dev *mdev = mlx5e_ipsec_sa2dev(sa_entry);

	mlx5_del_flow_rules(&ipsec_rule->rule);
	mlx5_del_flow_rules(&ipsec_rule->kspi_rule);
	if (ipsec_rule->vid_zero_rule != NULL)
		mlx5_del_flow_rules(&ipsec_rule->vid_zero_rule);
	if (ipsec_rule->reqid_rule != NULL)
		mlx5_del_flow_rules(&ipsec_rule->reqid_rule);
	mlx5_fc_destroy(mdev, ipsec_rule->fc);
	mlx5_packet_reformat_dealloc(mdev, ipsec_rule->pkt_reformat);
	if (sa_entry->attrs.dir == IPSEC_DIR_OUTBOUND) {
		tx_ft_put(sa_entry->ipsec);
		return;
	}

	if (ipsec_rule->modify_hdr != NULL)
		mlx5_modify_header_dealloc(mdev, ipsec_rule->modify_hdr);
}

int mlx5e_accel_ipsec_fs_add_pol(struct mlx5e_ipsec_pol_entry *pol_entry)
{
	if (pol_entry->attrs.dir == IPSEC_DIR_OUTBOUND)
		return tx_add_policy(pol_entry);

	return rx_add_policy(pol_entry);
}

void mlx5e_accel_ipsec_fs_del_pol(struct mlx5e_ipsec_pol_entry *pol_entry)
{
	struct mlx5e_ipsec_rule *ipsec_rule = &pol_entry->ipsec_rule;
	struct mlx5_core_dev *mdev = mlx5e_ipsec_pol2dev(pol_entry);

	mlx5_del_flow_rules(&ipsec_rule->rule);
	if (ipsec_rule->vid_zero_rule != NULL)
		mlx5_del_flow_rules(&ipsec_rule->vid_zero_rule);

	if (pol_entry->attrs.dir == IPSEC_DIR_INBOUND) {
		struct mlx5e_ipsec_rx *rx;

                rx = (pol_entry->attrs.family == AF_INET)
                         ? pol_entry->ipsec->rx_ipv4
                         : pol_entry->ipsec->rx_ipv6;
                if (rx->chains)
                        ipsec_chains_put_table(rx->chains,
                                               pol_entry->attrs.prio);
                return;
	}

	if (ipsec_rule->modify_hdr)
		mlx5_modify_header_dealloc(mdev, ipsec_rule->modify_hdr);

	tx_ft_put_policy(pol_entry->ipsec, pol_entry->attrs.prio);
}

void mlx5e_accel_ipsec_fs_rx_catchall_rules_destroy(struct mlx5e_priv *priv)
{
	/* Check if IPsec supported */
	if (!priv->ipsec)
		return;

	ipsec_fs_rx_catchall_rules_destroy(priv->mdev, priv->ipsec->rx_ipv4);
	ipsec_fs_rx_catchall_rules_destroy(priv->mdev, priv->ipsec->rx_ipv6);
}

int mlx5e_accel_ipsec_fs_rx_catchall_rules(struct mlx5e_priv *priv)
{
	struct mlx5e_ipsec *ipsec = priv->ipsec;
	struct mlx5_flow_destination dest = {};
	int err = 0;

	/* Check if IPsec supported */
	if (!ipsec)
		return 0;

	dest.type = MLX5_FLOW_DESTINATION_TYPE_FLOW_TABLE;
	dest.ft = priv->fts.vlan.t;
	err = ipsec_fs_rx_catchall_rules(priv, ipsec->rx_ipv6, &dest);
	if (err)
		goto out;

	err = ipsec_fs_rx_catchall_rules(priv, ipsec->rx_ipv4, &dest);
	if (err)
		ipsec_fs_rx_catchall_rules_destroy(priv->mdev, priv->ipsec->rx_ipv6);
out:
	return err;
}

void mlx5e_accel_ipsec_fs_rx_tables_destroy(struct mlx5e_priv *priv)
{
	struct mlx5_core_dev *mdev = priv->mdev;
	struct mlx5e_ipsec *ipsec = priv->ipsec;

	/* Check if IPsec supported */
	if (!ipsec)
		return;

	ipsec_fs_rx_table_destroy(mdev, ipsec->rx_ipv6);
	ipsec_fs_rx_table_destroy(mdev, ipsec->rx_ipv4);
}

int mlx5e_accel_ipsec_fs_rx_tables_create(struct mlx5e_priv *priv)
{
	struct mlx5e_ipsec *ipsec = priv->ipsec;
	int err = 0;

	/* Check if IPsec supported */
	if (!ipsec)
		return 0;

	err = ipsec_fs_rx_table_create(ipsec->mdev, ipsec->rx_ipv4, 0, 0);
	if (err)
		goto out;

	err = ipsec_fs_rx_table_create(ipsec->mdev, ipsec->rx_ipv6, 4, 1);
	if (err) {
		ipsec_fs_rx_table_destroy(priv->mdev, ipsec->rx_ipv4);
		goto out;
	}

	priv->fts.ipsec_ft = priv->ipsec->rx_ipv4->ft.pol;
out:
	return err;
}

void mlx5e_accel_ipsec_fs_cleanup(struct mlx5e_ipsec *ipsec)
{
	WARN_ON(ipsec->tx->ft.refcnt);
	mutex_destroy(&ipsec->rx_ipv6->ft.mutex);
	mutex_destroy(&ipsec->rx_ipv4->ft.mutex);
	mutex_destroy(&ipsec->tx->ft.mutex);
	ipsec_fs_destroy_counters(ipsec);
	kfree(ipsec->rx_ipv6);
	kfree(ipsec->rx_ipv4);
	kfree(ipsec->tx);
}

int mlx5e_accel_ipsec_fs_init(struct mlx5e_ipsec *ipsec)
{
	struct mlx5_flow_namespace *tns, *rns;
	int err = -ENOMEM;

	tns = mlx5_get_flow_namespace(ipsec->mdev, MLX5_FLOW_NAMESPACE_EGRESS_IPSEC);
	if (!tns)
		return -EOPNOTSUPP;

	rns = mlx5_get_flow_namespace(ipsec->mdev, MLX5_FLOW_NAMESPACE_KERNEL);
	if (!rns)
		return -EOPNOTSUPP;

	ipsec->tx = kzalloc(sizeof(*ipsec->tx), GFP_KERNEL);
	if (!ipsec->tx)
		return -ENOMEM;

	ipsec->rx_ipv4 = kzalloc(sizeof(*ipsec->rx_ipv4), GFP_KERNEL);
	if (!ipsec->rx_ipv4)
		goto err_tx;

	ipsec->rx_ipv6 = kzalloc(sizeof(*ipsec->rx_ipv6), GFP_KERNEL);
	if (!ipsec->rx_ipv6)
		goto err_rx_ipv4;

	err = ipsec_fs_init_counters(ipsec);
	if (err)
		goto err_rx_ipv6;

	ipsec->tx->ns = tns;
	mutex_init(&ipsec->tx->ft.mutex);
	ipsec->rx_ipv4->ns = rns;
	ipsec->rx_ipv6->ns = rns;
	mutex_init(&ipsec->rx_ipv4->ft.mutex);
	mutex_init(&ipsec->rx_ipv6->ft.mutex);

	mlx5e_accel_ipsec_fs_init_roce(ipsec);

	return 0;

err_rx_ipv6:
	kfree(ipsec->rx_ipv6);
err_rx_ipv4:
	kfree(ipsec->rx_ipv4);
err_tx:
	kfree(ipsec->tx);
	return err;
}

void mlx5e_accel_ipsec_fs_modify(struct mlx5e_ipsec_sa_entry *sa_entry)
{
        struct mlx5e_ipsec_sa_entry sa_entry_shadow = {};
        int err;

        memcpy(&sa_entry_shadow, sa_entry, sizeof(*sa_entry));
        memset(&sa_entry_shadow.ipsec_rule, 0x00, sizeof(sa_entry->ipsec_rule));

        err = mlx5e_accel_ipsec_fs_add_rule(&sa_entry_shadow);
        if (err)
                return;
        mlx5e_accel_ipsec_fs_del_rule(sa_entry);
        memcpy(sa_entry, &sa_entry_shadow, sizeof(*sa_entry));
}
