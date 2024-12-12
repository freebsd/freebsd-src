/*-
 * Copyright (c) 2013-2017, Mellanox Technologies, Ltd.  All rights reserved.
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

#include <linux/etherdevice.h>
#include <dev/mlx5/driver.h>
#include <dev/mlx5/mlx5_ifc.h>
#include <dev/mlx5/vport.h>
#include <dev/mlx5/fs.h>
#include <dev/mlx5/mpfs.h>
#include <dev/mlx5/mlx5_core/mlx5_core.h>
#include <dev/mlx5/mlx5_core/eswitch.h>

#define UPLINK_VPORT 0xFFFF

#define MLX5_DEBUG_ESWITCH_MASK BIT(3)

#define esw_info(dev, format, ...)				\
	printf("mlx5_core: INFO: ""(%s): E-Switch: " format, (dev)->priv.name, ##__VA_ARGS__)

#define esw_warn(dev, format, ...)				\
	printf("mlx5_core: WARN: ""(%s): E-Switch: " format, (dev)->priv.name, ##__VA_ARGS__)

#define esw_debug(dev, format, ...)				\
	mlx5_core_dbg_mask(dev, MLX5_DEBUG_ESWITCH_MASK, format, ##__VA_ARGS__)

enum {
	MLX5_ACTION_NONE = 0,
	MLX5_ACTION_ADD  = 1,
	MLX5_ACTION_DEL  = 2,
};

/* E-Switch UC L2 table hash node */
struct esw_uc_addr {
	struct l2addr_node node;
	u32                table_index;
	u32                vport;
};

/* E-Switch MC FDB table hash node */
struct esw_mc_addr { /* SRIOV only */
	struct l2addr_node     node;
	struct mlx5_flow_handle *uplink_rule; /* Forward to uplink rule */
	u32                    refcnt;
};

/* Vport UC/MC hash node */
struct vport_addr {
	struct l2addr_node     node;
	u8                     action;
	u32                    vport;
	struct mlx5_flow_handle *flow_rule; /* SRIOV only */
};

enum {
	UC_ADDR_CHANGE = BIT(0),
	MC_ADDR_CHANGE = BIT(1),
};

/* Vport context events */
#define SRIOV_VPORT_EVENTS (UC_ADDR_CHANGE | \
			    MC_ADDR_CHANGE)

static int arm_vport_context_events_cmd(struct mlx5_core_dev *dev, u16 vport,
					u32 events_mask)
{
	int in[MLX5_ST_SZ_DW(modify_nic_vport_context_in)] = {0};
	int out[MLX5_ST_SZ_DW(modify_nic_vport_context_out)] = {0};
	void *nic_vport_ctx;

	MLX5_SET(modify_nic_vport_context_in, in,
		 opcode, MLX5_CMD_OP_MODIFY_NIC_VPORT_CONTEXT);
	MLX5_SET(modify_nic_vport_context_in, in, field_select.change_event, 1);
	MLX5_SET(modify_nic_vport_context_in, in, vport_number, vport);
	if (vport)
		MLX5_SET(modify_nic_vport_context_in, in, other_vport, 1);
	nic_vport_ctx = MLX5_ADDR_OF(modify_nic_vport_context_in,
				     in, nic_vport_context);

	MLX5_SET(nic_vport_context, nic_vport_ctx, arm_change_event, 1);

	if (events_mask & UC_ADDR_CHANGE)
		MLX5_SET(nic_vport_context, nic_vport_ctx,
			 event_on_uc_address_change, 1);
	if (events_mask & MC_ADDR_CHANGE)
		MLX5_SET(nic_vport_context, nic_vport_ctx,
			 event_on_mc_address_change, 1);

	return mlx5_cmd_exec(dev, in, sizeof(in), out, sizeof(out));
}

/* E-Switch vport context HW commands */
static int query_esw_vport_context_cmd(struct mlx5_core_dev *mdev, u32 vport,
				       u32 *out, int outlen)
{
	u32 in[MLX5_ST_SZ_DW(query_esw_vport_context_in)] = {0};

	MLX5_SET(query_nic_vport_context_in, in, opcode,
		 MLX5_CMD_OP_QUERY_ESW_VPORT_CONTEXT);

	MLX5_SET(query_esw_vport_context_in, in, vport_number, vport);
	if (vport)
		MLX5_SET(query_esw_vport_context_in, in, other_vport, 1);

	return mlx5_cmd_exec(mdev, in, sizeof(in), out, outlen);
}

static int query_esw_vport_cvlan(struct mlx5_core_dev *dev, u32 vport,
				 u16 *vlan, u8 *qos)
{
	u32 out[MLX5_ST_SZ_DW(query_esw_vport_context_out)] = {0};
	int err;
	bool cvlan_strip;
	bool cvlan_insert;

	*vlan = 0;
	*qos = 0;

	if (!MLX5_CAP_ESW(dev, vport_cvlan_strip) ||
	    !MLX5_CAP_ESW(dev, vport_cvlan_insert_if_not_exist))
		return -ENOTSUPP;

	err = query_esw_vport_context_cmd(dev, vport, out, sizeof(out));
	if (err)
		goto out;

	cvlan_strip = MLX5_GET(query_esw_vport_context_out, out,
			       esw_vport_context.vport_cvlan_strip);

	cvlan_insert = MLX5_GET(query_esw_vport_context_out, out,
				esw_vport_context.vport_cvlan_insert);

	if (cvlan_strip || cvlan_insert) {
		*vlan = MLX5_GET(query_esw_vport_context_out, out,
				 esw_vport_context.cvlan_id);
		*qos = MLX5_GET(query_esw_vport_context_out, out,
				esw_vport_context.cvlan_pcp);
	}

	esw_debug(dev, "Query Vport[%d] cvlan: VLAN %d qos=%d\n",
		  vport, *vlan, *qos);
out:
	return err;
}

static int modify_esw_vport_context_cmd(struct mlx5_core_dev *dev, u16 vport,
					void *in, int inlen)
{
	u32 out[MLX5_ST_SZ_DW(modify_esw_vport_context_out)] = {0};

	MLX5_SET(modify_esw_vport_context_in, in, vport_number, vport);
	if (vport)
		MLX5_SET(modify_esw_vport_context_in, in, other_vport, 1);

	MLX5_SET(modify_esw_vport_context_in, in, opcode,
		 MLX5_CMD_OP_MODIFY_ESW_VPORT_CONTEXT);

	return mlx5_cmd_exec(dev, in, inlen, out, sizeof(out));
}

static int modify_esw_vport_cvlan(struct mlx5_core_dev *dev, u32 vport,
				  u16 vlan, u8 qos, bool set)
{
	u32 in[MLX5_ST_SZ_DW(modify_esw_vport_context_in)] = {0};

	if (!MLX5_CAP_ESW(dev, vport_cvlan_strip) ||
	    !MLX5_CAP_ESW(dev, vport_cvlan_insert_if_not_exist))
		return -ENOTSUPP;

	esw_debug(dev, "Set Vport[%d] VLAN %d qos %d set=%d\n",
		  vport, vlan, qos, set);

	if (set) {
		MLX5_SET(modify_esw_vport_context_in, in,
			 esw_vport_context.vport_cvlan_strip, 1);
		/* insert only if no vlan in packet */
		MLX5_SET(modify_esw_vport_context_in, in,
			 esw_vport_context.vport_cvlan_insert, 1);
		MLX5_SET(modify_esw_vport_context_in, in,
			 esw_vport_context.cvlan_pcp, qos);
		MLX5_SET(modify_esw_vport_context_in, in,
			 esw_vport_context.cvlan_id, vlan);
	}

	MLX5_SET(modify_esw_vport_context_in, in,
		 field_select.vport_cvlan_strip, 1);
	MLX5_SET(modify_esw_vport_context_in, in,
		 field_select.vport_cvlan_insert, 1);

	return modify_esw_vport_context_cmd(dev, vport, in, sizeof(in));
}

/* E-Switch FDB */
static struct mlx5_flow_handle *
esw_fdb_set_vport_rule(struct mlx5_eswitch *esw, u8 mac[ETH_ALEN], u32 vport)
{
	struct mlx5_flow_destination dest = {};
	struct mlx5_flow_handle *flow_rule = NULL;
	struct mlx5_flow_act flow_act = {};
	struct mlx5_flow_spec *spec;
	u8 *dmac_v;
	u8 *dmac_c;

	spec = kzalloc(sizeof(*spec), GFP_KERNEL);
	if (!spec) {
		printf("mlx5_core: WARN: ""FDB: Failed to alloc flow spec\n");
		goto out;
	}
	dmac_v = MLX5_ADDR_OF(fte_match_param, spec->match_value,
			      outer_headers.dmac_47_16);
	dmac_c = MLX5_ADDR_OF(fte_match_param, spec->match_criteria,
			      outer_headers.dmac_47_16);

	spec->match_criteria_enable = MLX5_MATCH_OUTER_HEADERS;
	ether_addr_copy(dmac_v, mac);
	/* Match criteria mask */
	memset(dmac_c, 0xff, 6);

	dest.type = MLX5_FLOW_DESTINATION_TYPE_VPORT;
	dest.vport.num = vport;

	esw_debug(esw->dev,
		  "\tFDB add rule dmac_v(%pM) dmac_c(%pM) -> vport(%d)\n",
		  dmac_v, dmac_c, vport);
	flow_act.action = MLX5_FLOW_CONTEXT_ACTION_FWD_DEST;
	flow_rule =
		mlx5_add_flow_rules(esw->fdb_table.fdb, spec,
				   &flow_act, &dest, 1);
	if (IS_ERR_OR_NULL(flow_rule)) {
		printf("mlx5_core: WARN: ""FDB: Failed to add flow rule: dmac_v(%pM) dmac_c(%pM) -> vport(%d), err(%ld)\n", dmac_v, dmac_c, vport, PTR_ERR(flow_rule));
		flow_rule = NULL;
	}
out:
	kfree(spec);
	return flow_rule;
}

static int esw_create_fdb_table(struct mlx5_eswitch *esw)
{
	int inlen = MLX5_ST_SZ_BYTES(create_flow_group_in);
	struct mlx5_flow_table_attr ft_attr = {};
	struct mlx5_core_dev *dev = esw->dev;
	struct mlx5_flow_namespace *root_ns;
	struct mlx5_flow_table *fdb;
	struct mlx5_flow_group *g;
	void *match_criteria;
	int table_size;
	u32 *flow_group_in;
	u8 *dmac;
	int err = 0;

	esw_debug(dev, "Create FDB log_max_size(%d)\n",
		  MLX5_CAP_ESW_FLOWTABLE_FDB(dev, log_max_ft_size));

	root_ns = mlx5_get_flow_namespace(dev, MLX5_FLOW_NAMESPACE_FDB);
	if (!root_ns) {
		esw_warn(dev, "Failed to get FDB flow namespace\n");
		return -ENOMEM;
	}

	flow_group_in = mlx5_vzalloc(inlen);
	if (!flow_group_in)
		return -ENOMEM;
	memset(flow_group_in, 0, inlen);

	/* (-2) Since MaorG said so .. */
	table_size = BIT(MLX5_CAP_ESW_FLOWTABLE_FDB(dev, log_max_ft_size)) - 2;

	ft_attr.prio = FDB_SLOW_PATH;
	ft_attr.max_fte = table_size;
	fdb = mlx5_create_flow_table(root_ns, &ft_attr);
	if (IS_ERR_OR_NULL(fdb)) {
		err = PTR_ERR(fdb);
		esw_warn(dev, "Failed to create FDB Table err %d\n", err);
		goto out;
	}

	MLX5_SET(create_flow_group_in, flow_group_in, match_criteria_enable,
		 MLX5_MATCH_OUTER_HEADERS);
	match_criteria = MLX5_ADDR_OF(create_flow_group_in, flow_group_in, match_criteria);
	dmac = MLX5_ADDR_OF(fte_match_param, match_criteria, outer_headers.dmac_47_16);
	MLX5_SET(create_flow_group_in, flow_group_in, start_flow_index, 0);
	MLX5_SET(create_flow_group_in, flow_group_in, end_flow_index, table_size - 1);
	eth_broadcast_addr(dmac);

	g = mlx5_create_flow_group(fdb, flow_group_in);
	if (IS_ERR_OR_NULL(g)) {
		err = PTR_ERR(g);
		esw_warn(dev, "Failed to create flow group err(%d)\n", err);
		goto out;
	}

	esw->fdb_table.addr_grp = g;
	esw->fdb_table.fdb = fdb;
out:
	kfree(flow_group_in);
	if (err && !IS_ERR_OR_NULL(fdb))
		mlx5_destroy_flow_table(fdb);
	return err;
}

static void esw_destroy_fdb_table(struct mlx5_eswitch *esw)
{
	if (!esw->fdb_table.fdb)
		return;

	esw_debug(esw->dev, "Destroy FDB Table\n");
	mlx5_destroy_flow_group(esw->fdb_table.addr_grp);
	mlx5_destroy_flow_table(esw->fdb_table.fdb);
	esw->fdb_table.fdb = NULL;
	esw->fdb_table.addr_grp = NULL;
}

/* E-Switch vport UC/MC lists management */
typedef int (*vport_addr_action)(struct mlx5_eswitch *esw,
				 struct vport_addr *vaddr);

static int esw_add_uc_addr(struct mlx5_eswitch *esw, struct vport_addr *vaddr)
{
	struct hlist_head *hash = esw->l2_table.l2_hash;
	struct esw_uc_addr *esw_uc;
	u8 *mac = vaddr->node.addr;
	u32 vport = vaddr->vport;
	int err;

	esw_uc = l2addr_hash_find(hash, mac, struct esw_uc_addr);
	if (esw_uc) {
		esw_warn(esw->dev,
			 "Failed to set L2 mac(%pM) for vport(%d), mac is already in use by vport(%d)\n",
			 mac, vport, esw_uc->vport);
		return -EEXIST;
	}

	esw_uc = l2addr_hash_add(hash, mac, struct esw_uc_addr, GFP_KERNEL);
	if (!esw_uc)
		return -ENOMEM;
	esw_uc->vport = vport;

	err = mlx5_mpfs_add_mac(esw->dev, &esw_uc->table_index, mac, 0, 0);
	if (err)
		goto abort;

	if (esw->fdb_table.fdb) /* SRIOV is enabled: Forward UC MAC to vport */
		vaddr->flow_rule = esw_fdb_set_vport_rule(esw, mac, vport);

	esw_debug(esw->dev, "\tADDED UC MAC: vport[%d] %pM index:%d fr(%p)\n",
		  vport, mac, esw_uc->table_index, vaddr->flow_rule);
	return err;
abort:
	l2addr_hash_del(esw_uc);
	return err;
}

static int esw_del_uc_addr(struct mlx5_eswitch *esw, struct vport_addr *vaddr)
{
	struct hlist_head *hash = esw->l2_table.l2_hash;
	struct esw_uc_addr *esw_uc;
	u8 *mac = vaddr->node.addr;
	u32 vport = vaddr->vport;

	esw_uc = l2addr_hash_find(hash, mac, struct esw_uc_addr);
	if (!esw_uc || esw_uc->vport != vport) {
		esw_debug(esw->dev,
			  "MAC(%pM) doesn't belong to vport (%d)\n",
			  mac, vport);
		return -EINVAL;
	}
	esw_debug(esw->dev, "\tDELETE UC MAC: vport[%d] %pM index:%d fr(%p)\n",
		  vport, mac, esw_uc->table_index, vaddr->flow_rule);

	mlx5_mpfs_del_mac(esw->dev, esw_uc->table_index);

	mlx5_del_flow_rules(&vaddr->flow_rule);

	l2addr_hash_del(esw_uc);
	return 0;
}

static int esw_add_mc_addr(struct mlx5_eswitch *esw, struct vport_addr *vaddr)
{
	struct hlist_head *hash = esw->mc_table;
	struct esw_mc_addr *esw_mc;
	u8 *mac = vaddr->node.addr;
	u32 vport = vaddr->vport;

	if (!esw->fdb_table.fdb)
		return 0;

	esw_mc = l2addr_hash_find(hash, mac, struct esw_mc_addr);
	if (esw_mc)
		goto add;

	esw_mc = l2addr_hash_add(hash, mac, struct esw_mc_addr, GFP_KERNEL);
	if (!esw_mc)
		return -ENOMEM;

	esw_mc->uplink_rule = /* Forward MC MAC to Uplink */
		esw_fdb_set_vport_rule(esw, mac, UPLINK_VPORT);
add:
	esw_mc->refcnt++;
	/* Forward MC MAC to vport */
	vaddr->flow_rule = esw_fdb_set_vport_rule(esw, mac, vport);
	esw_debug(esw->dev,
		  "\tADDED MC MAC: vport[%d] %pM fr(%p) refcnt(%d) uplinkfr(%p)\n",
		  vport, mac, vaddr->flow_rule,
		  esw_mc->refcnt, esw_mc->uplink_rule);
	return 0;
}

static int esw_del_mc_addr(struct mlx5_eswitch *esw, struct vport_addr *vaddr)
{
	struct hlist_head *hash = esw->mc_table;
	struct esw_mc_addr *esw_mc;
	u8 *mac = vaddr->node.addr;
	u32 vport = vaddr->vport;

	if (!esw->fdb_table.fdb)
		return 0;

	esw_mc = l2addr_hash_find(hash, mac, struct esw_mc_addr);
	if (!esw_mc) {
		esw_warn(esw->dev,
			 "Failed to find eswitch MC addr for MAC(%pM) vport(%d)",
			 mac, vport);
		return -EINVAL;
	}
	esw_debug(esw->dev,
		  "\tDELETE MC MAC: vport[%d] %pM fr(%p) refcnt(%d) uplinkfr(%p)\n",
		  vport, mac, vaddr->flow_rule, esw_mc->refcnt,
		  esw_mc->uplink_rule);

	mlx5_del_flow_rules(&vaddr->flow_rule);

	if (--esw_mc->refcnt)
		return 0;

	mlx5_del_flow_rules(&esw_mc->uplink_rule);

	l2addr_hash_del(esw_mc);
	return 0;
}

/* Apply vport UC/MC list to HW l2 table and FDB table */
static void esw_apply_vport_addr_list(struct mlx5_eswitch *esw,
				      u32 vport_num, int list_type)
{
	struct mlx5_vport *vport = &esw->vports[vport_num];
	bool is_uc = list_type == MLX5_NIC_VPORT_LIST_TYPE_UC;
	vport_addr_action vport_addr_add;
	vport_addr_action vport_addr_del;
	struct vport_addr *addr;
	struct l2addr_node *node;
	struct hlist_head *hash;
	struct hlist_node *tmp;
	int hi;

	vport_addr_add = is_uc ? esw_add_uc_addr :
				 esw_add_mc_addr;
	vport_addr_del = is_uc ? esw_del_uc_addr :
				 esw_del_mc_addr;

	hash = is_uc ? vport->uc_list : vport->mc_list;
	for_each_l2hash_node(node, tmp, hash, hi) {
		addr = container_of(node, struct vport_addr, node);
		switch (addr->action) {
		case MLX5_ACTION_ADD:
			vport_addr_add(esw, addr);
			addr->action = MLX5_ACTION_NONE;
			break;
		case MLX5_ACTION_DEL:
			vport_addr_del(esw, addr);
			l2addr_hash_del(addr);
			break;
		}
	}
}

/* Sync vport UC/MC list from vport context */
static void esw_update_vport_addr_list(struct mlx5_eswitch *esw,
				       u32 vport_num, int list_type)
{
	struct mlx5_vport *vport = &esw->vports[vport_num];
	bool is_uc = list_type == MLX5_NIC_VPORT_LIST_TYPE_UC;
	u8 (*mac_list)[ETH_ALEN];
	struct l2addr_node *node;
	struct vport_addr *addr;
	struct hlist_head *hash;
	struct hlist_node *tmp;
	int size;
	int err;
	int hi;
	int i;

	size = is_uc ? MLX5_MAX_UC_PER_VPORT(esw->dev) :
		       MLX5_MAX_MC_PER_VPORT(esw->dev);

	mac_list = kcalloc(size, ETH_ALEN, GFP_KERNEL);
	if (!mac_list)
		return;

	hash = is_uc ? vport->uc_list : vport->mc_list;

	for_each_l2hash_node(node, tmp, hash, hi) {
		addr = container_of(node, struct vport_addr, node);
		addr->action = MLX5_ACTION_DEL;
	}

	err = mlx5_query_nic_vport_mac_list(esw->dev, vport_num, list_type,
					    mac_list, &size);
	if (err)
		return;
	esw_debug(esw->dev, "vport[%d] context update %s list size (%d)\n",
		  vport_num, is_uc ? "UC" : "MC", size);

	for (i = 0; i < size; i++) {
		if (is_uc && !is_valid_ether_addr(mac_list[i]))
			continue;

		if (!is_uc && !is_multicast_ether_addr(mac_list[i]))
			continue;

		addr = l2addr_hash_find(hash, mac_list[i], struct vport_addr);
		if (addr) {
			addr->action = MLX5_ACTION_NONE;
			continue;
		}

		addr = l2addr_hash_add(hash, mac_list[i], struct vport_addr,
				       GFP_KERNEL);
		if (!addr) {
			esw_warn(esw->dev,
				 "Failed to add MAC(%pM) to vport[%d] DB\n",
				 mac_list[i], vport_num);
			continue;
		}
		addr->vport = vport_num;
		addr->action = MLX5_ACTION_ADD;
	}
	kfree(mac_list);
}

static void esw_vport_change_handler(struct work_struct *work)
{
	struct mlx5_vport *vport =
		container_of(work, struct mlx5_vport, vport_change_handler);
	struct mlx5_core_dev *dev = vport->dev;
	struct mlx5_eswitch *esw = dev->priv.eswitch;
	u8 mac[ETH_ALEN];

	mlx5_query_nic_vport_mac_address(dev, vport->vport, mac);
	esw_debug(dev, "vport[%d] Context Changed: perm mac: %pM\n",
		  vport->vport, mac);

	if (vport->enabled_events & UC_ADDR_CHANGE) {
		esw_update_vport_addr_list(esw, vport->vport,
					   MLX5_NIC_VPORT_LIST_TYPE_UC);
		esw_apply_vport_addr_list(esw, vport->vport,
					  MLX5_NIC_VPORT_LIST_TYPE_UC);
	}

	if (vport->enabled_events & MC_ADDR_CHANGE) {
		esw_update_vport_addr_list(esw, vport->vport,
					   MLX5_NIC_VPORT_LIST_TYPE_MC);
		esw_apply_vport_addr_list(esw, vport->vport,
					  MLX5_NIC_VPORT_LIST_TYPE_MC);
	}

	esw_debug(esw->dev, "vport[%d] Context Changed: Done\n", vport->vport);
	if (vport->enabled)
		arm_vport_context_events_cmd(dev, vport->vport,
					     vport->enabled_events);
}

static void esw_vport_enable_egress_acl(struct mlx5_eswitch *esw,
					struct mlx5_vport *vport)
{
	int inlen = MLX5_ST_SZ_BYTES(create_flow_group_in);
	struct mlx5_flow_table_attr ft_attr = {};
	struct mlx5_flow_group *vlan_grp = NULL;
	struct mlx5_flow_group *drop_grp = NULL;
	struct mlx5_core_dev *dev = esw->dev;
	struct mlx5_flow_namespace *root_ns;
	struct mlx5_flow_table *acl;
	void *match_criteria;
	u32 *flow_group_in;
	int table_size = 2;
	int err = 0;

	if (!MLX5_CAP_ESW_EGRESS_ACL(dev, ft_support))
		return;

	esw_debug(dev, "Create vport[%d] egress ACL log_max_size(%d)\n",
		  vport->vport, MLX5_CAP_ESW_EGRESS_ACL(dev, log_max_ft_size));

	root_ns = mlx5_get_flow_vport_acl_namespace(dev, MLX5_FLOW_NAMESPACE_ESW_EGRESS, vport->vport);
	if (!root_ns) {
		esw_warn(dev, "Failed to get E-Switch egress flow namespace\n");
		return;
	}

	flow_group_in = mlx5_vzalloc(inlen);
	if (!flow_group_in)
		return;

	ft_attr.max_fte = table_size;
        if (vport->vport)
                ft_attr.flags = MLX5_FLOW_TABLE_OTHER_VPORT;
	acl = mlx5_create_vport_flow_table(root_ns, &ft_attr, vport->vport);
	if (IS_ERR_OR_NULL(acl)) {
		err = PTR_ERR(acl);
		esw_warn(dev, "Failed to create E-Switch vport[%d] egress flow Table, err(%d)\n",
			 vport->vport, err);
		goto out;
	}

	MLX5_SET(create_flow_group_in, flow_group_in, match_criteria_enable, MLX5_MATCH_OUTER_HEADERS);
	match_criteria = MLX5_ADDR_OF(create_flow_group_in, flow_group_in, match_criteria);
	MLX5_SET_TO_ONES(fte_match_param, match_criteria, outer_headers.cvlan_tag);
	MLX5_SET_TO_ONES(fte_match_param, match_criteria, outer_headers.first_vid);
	MLX5_SET(create_flow_group_in, flow_group_in, start_flow_index, 0);
	MLX5_SET(create_flow_group_in, flow_group_in, end_flow_index, 0);

	vlan_grp = mlx5_create_flow_group(acl, flow_group_in);
	if (IS_ERR_OR_NULL(vlan_grp)) {
		err = PTR_ERR(vlan_grp);
		esw_warn(dev, "Failed to create E-Switch vport[%d] egress allowed vlans flow group, err(%d)\n",
			 vport->vport, err);
		goto out;
	}

	memset(flow_group_in, 0, inlen);
	MLX5_SET(create_flow_group_in, flow_group_in, start_flow_index, 1);
	MLX5_SET(create_flow_group_in, flow_group_in, end_flow_index, 1);
	drop_grp = mlx5_create_flow_group(acl, flow_group_in);
	if (IS_ERR_OR_NULL(drop_grp)) {
		err = PTR_ERR(drop_grp);
		esw_warn(dev, "Failed to create E-Switch vport[%d] egress drop flow group, err(%d)\n",
			 vport->vport, err);
		goto out;
	}

	vport->egress.acl = acl;
	vport->egress.drop_grp = drop_grp;
	vport->egress.allowed_vlans_grp = vlan_grp;
out:
	kfree(flow_group_in);
	if (err && !IS_ERR_OR_NULL(vlan_grp))
		mlx5_destroy_flow_group(vlan_grp);
	if (err && !IS_ERR_OR_NULL(acl))
		mlx5_destroy_flow_table(acl);
}

static void esw_vport_cleanup_egress_rules(struct mlx5_eswitch *esw,
					   struct mlx5_vport *vport)
{
	mlx5_del_flow_rules(&vport->egress.allowed_vlan);
	mlx5_del_flow_rules(&vport->egress.drop_rule);
}

static void esw_vport_disable_egress_acl(struct mlx5_eswitch *esw,
					 struct mlx5_vport *vport)
{
	if (IS_ERR_OR_NULL(vport->egress.acl))
		return;

	esw_debug(esw->dev, "Destroy vport[%d] E-Switch egress ACL\n", vport->vport);

	esw_vport_cleanup_egress_rules(esw, vport);
	mlx5_destroy_flow_group(vport->egress.allowed_vlans_grp);
	mlx5_destroy_flow_group(vport->egress.drop_grp);
	mlx5_destroy_flow_table(vport->egress.acl);
	vport->egress.allowed_vlans_grp = NULL;
	vport->egress.drop_grp = NULL;
	vport->egress.acl = NULL;
}

static void esw_vport_enable_ingress_acl(struct mlx5_eswitch *esw,
					 struct mlx5_vport *vport)
{
	int inlen = MLX5_ST_SZ_BYTES(create_flow_group_in);
	struct mlx5_flow_table_attr ft_attr = {};
	struct mlx5_core_dev *dev = esw->dev;
	struct mlx5_flow_namespace *root_ns;
	struct mlx5_flow_table *acl;
	struct mlx5_flow_group *g;
	void *match_criteria;
	u32 *flow_group_in;
	int table_size = 1;
	int err = 0;

	if (!MLX5_CAP_ESW_INGRESS_ACL(dev, ft_support))
		return;

	esw_debug(dev, "Create vport[%d] ingress ACL log_max_size(%d)\n",
		  vport->vport, MLX5_CAP_ESW_INGRESS_ACL(dev, log_max_ft_size));

	root_ns = mlx5_get_flow_vport_acl_namespace(dev, MLX5_FLOW_NAMESPACE_ESW_INGRESS, vport->vport);
	if (!root_ns) {
		esw_warn(dev, "Failed to get E-Switch ingress flow namespace\n");
		return;
	}

	flow_group_in = mlx5_vzalloc(inlen);
	if (!flow_group_in)
		return;

	ft_attr.max_fte = table_size;
        if (vport->vport)
                ft_attr.flags = MLX5_FLOW_TABLE_OTHER_VPORT;
	acl = mlx5_create_vport_flow_table(root_ns, &ft_attr, vport->vport);
	if (IS_ERR_OR_NULL(acl)) {
		err = PTR_ERR(acl);
		esw_warn(dev, "Failed to create E-Switch vport[%d] ingress flow Table, err(%d)\n",
			 vport->vport, err);
		goto out;
	}

	MLX5_SET(create_flow_group_in, flow_group_in, match_criteria_enable, MLX5_MATCH_OUTER_HEADERS);
	match_criteria = MLX5_ADDR_OF(create_flow_group_in, flow_group_in, match_criteria);
	MLX5_SET_TO_ONES(fte_match_param, match_criteria, outer_headers.cvlan_tag);
	MLX5_SET(create_flow_group_in, flow_group_in, start_flow_index, 0);
	MLX5_SET(create_flow_group_in, flow_group_in, end_flow_index, 0);

	g = mlx5_create_flow_group(acl, flow_group_in);
	if (IS_ERR_OR_NULL(g)) {
		err = PTR_ERR(g);
		esw_warn(dev, "Failed to create E-Switch vport[%d] ingress flow group, err(%d)\n",
			 vport->vport, err);
		goto out;
	}

	vport->ingress.acl = acl;
	vport->ingress.drop_grp = g;
out:
	kfree(flow_group_in);
	if (err && !IS_ERR_OR_NULL(acl))
		mlx5_destroy_flow_table(acl);
}

static void esw_vport_cleanup_ingress_rules(struct mlx5_eswitch *esw,
					    struct mlx5_vport *vport)
{
	mlx5_del_flow_rules(&vport->ingress.drop_rule);
}

static void esw_vport_disable_ingress_acl(struct mlx5_eswitch *esw,
					  struct mlx5_vport *vport)
{
	if (IS_ERR_OR_NULL(vport->ingress.acl))
		return;

	esw_debug(esw->dev, "Destroy vport[%d] E-Switch ingress ACL\n", vport->vport);

	esw_vport_cleanup_ingress_rules(esw, vport);
	mlx5_destroy_flow_group(vport->ingress.drop_grp);
	mlx5_destroy_flow_table(vport->ingress.acl);
	vport->ingress.acl = NULL;
	vport->ingress.drop_grp = NULL;
}

static int esw_vport_ingress_config(struct mlx5_eswitch *esw,
				    struct mlx5_vport *vport)
{
	struct mlx5_flow_act flow_act = {};
	struct mlx5_flow_spec *spec;
	int err = 0;

	if (IS_ERR_OR_NULL(vport->ingress.acl)) {
		esw_warn(esw->dev,
			 "vport[%d] configure ingress rules failed, ingress acl is not initialized!\n",
			 vport->vport);
		return -EPERM;
	}

	esw_vport_cleanup_ingress_rules(esw, vport);

	if (!vport->vlan && !vport->qos)
		return 0;

	esw_debug(esw->dev,
		  "vport[%d] configure ingress rules, vlan(%d) qos(%d)\n",
		  vport->vport, vport->vlan, vport->qos);

	spec = kzalloc(sizeof(*spec), GFP_KERNEL);
	if (!spec) {
		err = -ENOMEM;
		esw_warn(esw->dev, "vport[%d] configure ingress rules failed, err(%d)\n",
			 vport->vport, err);
		goto out;
	}
	MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria, outer_headers.cvlan_tag);
	MLX5_SET_TO_ONES(fte_match_param, spec->match_value, outer_headers.cvlan_tag);

	flow_act.action = MLX5_FLOW_CONTEXT_ACTION_DROP;
	spec->match_criteria_enable = MLX5_MATCH_OUTER_HEADERS;
	vport->ingress.drop_rule =
		mlx5_add_flow_rules(vport->ingress.acl, spec,
				    &flow_act, NULL, 0);
	if (IS_ERR_OR_NULL(vport->ingress.drop_rule)) {
		err = PTR_ERR(vport->ingress.drop_rule);
		printf("mlx5_core: WARN: ""vport[%d] configure ingress rules, err(%d)\n", vport->vport, err);
		vport->ingress.drop_rule = NULL;
	}
out:
	kfree(spec);
	return err;
}

static int esw_vport_egress_config(struct mlx5_eswitch *esw,
				   struct mlx5_vport *vport)
{
	struct mlx5_flow_act flow_act = {};
	struct mlx5_flow_spec *spec;
	int err = 0;

	if (IS_ERR_OR_NULL(vport->egress.acl)) {
		esw_warn(esw->dev, "vport[%d] configure rgress rules failed, egress acl is not initialized!\n",
			 vport->vport);
		return -EPERM;
	}

	esw_vport_cleanup_egress_rules(esw, vport);

	if (!vport->vlan && !vport->qos)
		return 0;

	esw_debug(esw->dev,
		  "vport[%d] configure egress rules, vlan(%d) qos(%d)\n",
		  vport->vport, vport->vlan, vport->qos);

	spec = kzalloc(sizeof(*spec), GFP_KERNEL);
	if (!spec) {
		err = -ENOMEM;
		esw_warn(esw->dev, "vport[%d] configure egress rules failed, err(%d)\n",
			 vport->vport, err);
		goto out;
	}

	/* Allowed vlan rule */
	MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria, outer_headers.cvlan_tag);
	MLX5_SET_TO_ONES(fte_match_param, spec->match_value, outer_headers.cvlan_tag);
	MLX5_SET_TO_ONES(fte_match_param, spec->match_criteria, outer_headers.first_vid);
	MLX5_SET(fte_match_param, spec->match_value, outer_headers.first_vid, vport->vlan);

	spec->match_criteria_enable = MLX5_MATCH_OUTER_HEADERS;
	flow_act.action = MLX5_FLOW_CONTEXT_ACTION_ALLOW;

	vport->egress.allowed_vlan =
		mlx5_add_flow_rules(vport->egress.acl, spec,
				    &flow_act, NULL, 0);
	if (IS_ERR_OR_NULL(vport->egress.allowed_vlan)) {
		err = PTR_ERR(vport->egress.allowed_vlan);
		printf("mlx5_core: WARN: ""vport[%d] configure egress allowed vlan rule failed, err(%d)\n", vport->vport, err);
		vport->egress.allowed_vlan = NULL;
		goto out;
	}

	flow_act.action = MLX5_FLOW_CONTEXT_ACTION_DROP;
	vport->egress.drop_rule =
		mlx5_add_flow_rules(vport->egress.acl, NULL,
				   &flow_act, NULL, 0);
	if (IS_ERR_OR_NULL(vport->egress.drop_rule)) {
		err = PTR_ERR(vport->egress.drop_rule);
		printf("mlx5_core: WARN: ""vport[%d] configure egress drop rule failed, err(%d)\n", vport->vport, err);
		vport->egress.drop_rule = NULL;
	}
out:
	kfree(spec);
	return err;
}

static void esw_enable_vport(struct mlx5_eswitch *esw, int vport_num,
			     int enable_events)
{
	struct mlx5_vport *vport = &esw->vports[vport_num];
	unsigned long flags;

	mutex_lock(&vport->state_lock);
	WARN_ON(vport->enabled);

	esw_debug(esw->dev, "Enabling VPORT(%d)\n", vport_num);

	if (vport_num) { /* Only VFs need ACLs for VST and spoofchk filtering */
		esw_vport_enable_ingress_acl(esw, vport);
		esw_vport_enable_egress_acl(esw, vport);
		esw_vport_ingress_config(esw, vport);
		esw_vport_egress_config(esw, vport);
	}

	mlx5_modify_vport_admin_state(esw->dev,
				      MLX5_QUERY_VPORT_STATE_IN_OP_MOD_ESW_VPORT,
				      vport_num,
				      MLX5_ESW_VPORT_ADMIN_STATE_AUTO);

	/* Sync with current vport context */
	vport->enabled_events = enable_events;
	esw_vport_change_handler(&vport->vport_change_handler);

	spin_lock_irqsave(&vport->lock, flags);
	vport->enabled = true;
	spin_unlock_irqrestore(&vport->lock, flags);

	arm_vport_context_events_cmd(esw->dev, vport_num, enable_events);

	esw->enabled_vports++;
	esw_debug(esw->dev, "Enabled VPORT(%d)\n", vport_num);
	mutex_unlock(&vport->state_lock);
}

static void esw_cleanup_vport(struct mlx5_eswitch *esw, u16 vport_num)
{
	struct mlx5_vport *vport = &esw->vports[vport_num];
	struct l2addr_node *node;
	struct vport_addr *addr;
	struct hlist_node *tmp;
	int hi;

	for_each_l2hash_node(node, tmp, vport->uc_list, hi) {
		addr = container_of(node, struct vport_addr, node);
		addr->action = MLX5_ACTION_DEL;
	}
	esw_apply_vport_addr_list(esw, vport_num, MLX5_NIC_VPORT_LIST_TYPE_UC);

	for_each_l2hash_node(node, tmp, vport->mc_list, hi) {
		addr = container_of(node, struct vport_addr, node);
		addr->action = MLX5_ACTION_DEL;
	}
	esw_apply_vport_addr_list(esw, vport_num, MLX5_NIC_VPORT_LIST_TYPE_MC);
}

static void esw_disable_vport(struct mlx5_eswitch *esw, int vport_num)
{
	struct mlx5_vport *vport = &esw->vports[vport_num];
	unsigned long flags;

	mutex_lock(&vport->state_lock);
	if (!vport->enabled) {
		mutex_unlock(&vport->state_lock);
		return;
	}

	esw_debug(esw->dev, "Disabling vport(%d)\n", vport_num);
	/* Mark this vport as disabled to discard new events */
	spin_lock_irqsave(&vport->lock, flags);
	vport->enabled = false;
	vport->enabled_events = 0;
	spin_unlock_irqrestore(&vport->lock, flags);

	mlx5_modify_vport_admin_state(esw->dev,
				      MLX5_QUERY_VPORT_STATE_IN_OP_MOD_ESW_VPORT,
				      vport_num,
				      MLX5_ESW_VPORT_ADMIN_STATE_DOWN);
	/* Wait for current already scheduled events to complete */
	flush_workqueue(esw->work_queue);
	/* Disable events from this vport */
	arm_vport_context_events_cmd(esw->dev, vport->vport, 0);
	/* We don't assume VFs will cleanup after themselves */
	esw_cleanup_vport(esw, vport_num);
	if (vport_num) {
		esw_vport_disable_egress_acl(esw, vport);
		esw_vport_disable_ingress_acl(esw, vport);
	}
	esw->enabled_vports--;
	mutex_unlock(&vport->state_lock);
}

/* Public E-Switch API */
int mlx5_eswitch_enable_sriov(struct mlx5_eswitch *esw, int nvfs)
{
	int err;
	int i;

	if (!esw || !MLX5_CAP_GEN(esw->dev, vport_group_manager) ||
	    MLX5_CAP_GEN(esw->dev, port_type) != MLX5_CAP_PORT_TYPE_ETH)
		return 0;

	if (!MLX5_CAP_GEN(esw->dev, eswitch_flow_table) ||
	    !MLX5_CAP_ESW_FLOWTABLE_FDB(esw->dev, ft_support)) {
		esw_warn(esw->dev, "E-Switch FDB is not supported, aborting ...\n");
		return -ENOTSUPP;
	}

	if (!MLX5_CAP_ESW_INGRESS_ACL(esw->dev, ft_support))
		esw_warn(esw->dev, "E-Switch ingress ACL is not supported by FW\n");

	if (!MLX5_CAP_ESW_EGRESS_ACL(esw->dev, ft_support))
		esw_warn(esw->dev, "E-Switch egress ACL is not supported by FW\n");

	esw_info(esw->dev, "E-Switch enable SRIOV: nvfs(%d)\n", nvfs);

	esw_disable_vport(esw, 0);

	err = esw_create_fdb_table(esw);
	if (err)
		goto abort;

	for (i = 0; i <= nvfs; i++)
		esw_enable_vport(esw, i, SRIOV_VPORT_EVENTS);

	esw_info(esw->dev, "SRIOV enabled: active vports(%d)\n",
		 esw->enabled_vports);
	return 0;

abort:
	esw_enable_vport(esw, 0, UC_ADDR_CHANGE);
	return err;
}

void mlx5_eswitch_disable_sriov(struct mlx5_eswitch *esw)
{
	int i;

	if (!esw || !MLX5_CAP_GEN(esw->dev, vport_group_manager) ||
	    MLX5_CAP_GEN(esw->dev, port_type) != MLX5_CAP_PORT_TYPE_ETH)
		return;

	esw_info(esw->dev, "disable SRIOV: active vports(%d)\n",
		 esw->enabled_vports);

	for (i = 0; i < esw->total_vports; i++)
		esw_disable_vport(esw, i);

	esw_destroy_fdb_table(esw);

	/* VPORT 0 (PF) must be enabled back with non-sriov configuration */
	esw_enable_vport(esw, 0, UC_ADDR_CHANGE);
}

int mlx5_eswitch_init(struct mlx5_core_dev *dev, int total_vports)
{
	int l2_table_size = 1 << MLX5_CAP_GEN(dev, log_max_l2_table);
	struct mlx5_eswitch *esw;
	int vport_num;
	int err;

	if (!MLX5_CAP_GEN(dev, vport_group_manager) ||
	    MLX5_CAP_GEN(dev, port_type) != MLX5_CAP_PORT_TYPE_ETH)
		return 0;

	esw_info(dev,
		 "Total vports %d, l2 table size(%d), per vport: max uc(%d) max mc(%d)\n",
		 total_vports, l2_table_size,
		 MLX5_MAX_UC_PER_VPORT(dev),
		 MLX5_MAX_MC_PER_VPORT(dev));

	esw = kzalloc(sizeof(*esw), GFP_KERNEL);
	if (!esw)
		return -ENOMEM;

	esw->dev = dev;

	esw->l2_table.bitmap = kcalloc(BITS_TO_LONGS(l2_table_size),
				   sizeof(uintptr_t), GFP_KERNEL);
	if (!esw->l2_table.bitmap) {
		err = -ENOMEM;
		goto abort;
	}
	esw->l2_table.size = l2_table_size;

	esw->work_queue = create_singlethread_workqueue("mlx5_esw_wq");
	if (!esw->work_queue) {
		err = -ENOMEM;
		goto abort;
	}

	esw->vports = kcalloc(total_vports, sizeof(struct mlx5_vport),
			      GFP_KERNEL);
	if (!esw->vports) {
		err = -ENOMEM;
		goto abort;
	}

	for (vport_num = 0; vport_num < total_vports; vport_num++) {
		struct mlx5_vport *vport = &esw->vports[vport_num];

		vport->vport = vport_num;
		vport->dev = dev;
		INIT_WORK(&vport->vport_change_handler,
			  esw_vport_change_handler);
		spin_lock_init(&vport->lock);
		mutex_init(&vport->state_lock);
	}

	esw->total_vports = total_vports;
	esw->enabled_vports = 0;

	dev->priv.eswitch = esw;
	esw_enable_vport(esw, 0, UC_ADDR_CHANGE);
	/* VF Vports will be enabled when SRIOV is enabled */
	return 0;
abort:
	if (esw->work_queue)
		destroy_workqueue(esw->work_queue);
	kfree(esw->l2_table.bitmap);
	kfree(esw->vports);
	kfree(esw);
	return err;
}

void mlx5_eswitch_cleanup(struct mlx5_eswitch *esw)
{
	if (!esw || !MLX5_CAP_GEN(esw->dev, vport_group_manager) ||
	    MLX5_CAP_GEN(esw->dev, port_type) != MLX5_CAP_PORT_TYPE_ETH)
		return;

	esw_info(esw->dev, "cleanup\n");
	esw_disable_vport(esw, 0);

	esw->dev->priv.eswitch = NULL;
	destroy_workqueue(esw->work_queue);
	kfree(esw->l2_table.bitmap);
	kfree(esw->vports);
	kfree(esw);
}

void mlx5_eswitch_vport_event(struct mlx5_eswitch *esw, struct mlx5_eqe *eqe)
{
	struct mlx5_eqe_vport_change *vc_eqe = &eqe->data.vport_change;
	u16 vport_num = be16_to_cpu(vc_eqe->vport_num);
	struct mlx5_vport *vport;

	if (!esw) {
		printf("mlx5_core: WARN: ""MLX5 E-Switch: vport %d got an event while eswitch is not initialized\n", vport_num);
		return;
	}

	vport = &esw->vports[vport_num];
	spin_lock(&vport->lock);
	if (vport->enabled)
		queue_work(esw->work_queue, &vport->vport_change_handler);
	spin_unlock(&vport->lock);
}

/* Vport Administration */
#define ESW_ALLOWED(esw) \
	(esw && MLX5_CAP_GEN(esw->dev, vport_group_manager) && mlx5_core_is_pf(esw->dev))
#define LEGAL_VPORT(esw, vport) (vport >= 0 && vport < esw->total_vports)

static void node_guid_gen_from_mac(u64 *node_guid, u8 mac[ETH_ALEN])
{
	((u8 *)node_guid)[7] = mac[0];
	((u8 *)node_guid)[6] = mac[1];
	((u8 *)node_guid)[5] = mac[2];
	((u8 *)node_guid)[4] = 0xff;
	((u8 *)node_guid)[3] = 0xfe;
	((u8 *)node_guid)[2] = mac[3];
	((u8 *)node_guid)[1] = mac[4];
	((u8 *)node_guid)[0] = mac[5];
}

int mlx5_eswitch_set_vport_mac(struct mlx5_eswitch *esw,
			       int vport, u8 mac[ETH_ALEN])
{
	int err = 0;
	u64 node_guid;

	if (!ESW_ALLOWED(esw))
		return -EPERM;
	if (!LEGAL_VPORT(esw, vport))
		return -EINVAL;

	err = mlx5_modify_nic_vport_mac_address(esw->dev, vport, mac);
	if (err) {
		mlx5_core_warn(esw->dev,
			       "Failed to mlx5_modify_nic_vport_mac vport(%d) err=(%d)\n",
			       vport, err);
		return err;
	}

	node_guid_gen_from_mac(&node_guid, mac);
	err = mlx5_modify_nic_vport_node_guid(esw->dev, vport, node_guid);
	if (err) {
		mlx5_core_warn(esw->dev,
			       "Failed to mlx5_modify_nic_vport_node_guid vport(%d) err=(%d)\n",
			       vport, err);
		return err;
	}

	return err;
}

int mlx5_eswitch_set_vport_state(struct mlx5_eswitch *esw,
				 int vport, int link_state)
{
	if (!ESW_ALLOWED(esw))
		return -EPERM;
	if (!LEGAL_VPORT(esw, vport))
		return -EINVAL;

	return mlx5_modify_vport_admin_state(esw->dev,
					     MLX5_QUERY_VPORT_STATE_IN_OP_MOD_ESW_VPORT,
					     vport, link_state);
}

int mlx5_eswitch_get_vport_config(struct mlx5_eswitch *esw,
				  int vport, struct mlx5_esw_vport_info *ivi)
{
	u16 vlan;
	u8 qos;

	if (!ESW_ALLOWED(esw))
		return -EPERM;
	if (!LEGAL_VPORT(esw, vport))
		return -EINVAL;

	memset(ivi, 0, sizeof(*ivi));
	ivi->vf = vport - 1;

	mlx5_query_nic_vport_mac_address(esw->dev, vport, ivi->mac);
	ivi->linkstate = mlx5_query_vport_admin_state(esw->dev,
						      MLX5_QUERY_VPORT_STATE_IN_OP_MOD_ESW_VPORT,
						      vport);
	query_esw_vport_cvlan(esw->dev, vport, &vlan, &qos);
	ivi->vlan = vlan;
	ivi->qos = qos;
	ivi->spoofchk = 0;

	return 0;
}

int mlx5_eswitch_set_vport_vlan(struct mlx5_eswitch *esw,
				int vport, u16 vlan, u8 qos)
{
	struct mlx5_vport *evport;
	int err = 0;
	int set = 0;

	if (!ESW_ALLOWED(esw))
		return -EPERM;
	if (!LEGAL_VPORT(esw, vport) || (vlan > 4095) || (qos > 7))
		return -EINVAL;

	if (vlan || qos)
		set = 1;

	evport = &esw->vports[vport];

	err = modify_esw_vport_cvlan(esw->dev, vport, vlan, qos, set);
	if (err)
		return err;

	mutex_lock(&evport->state_lock);
	evport->vlan = vlan;
	evport->qos = qos;
	if (evport->enabled) {
		esw_vport_ingress_config(esw, evport);
		esw_vport_egress_config(esw, evport);
	}
	mutex_unlock(&evport->state_lock);
	return err;
}

