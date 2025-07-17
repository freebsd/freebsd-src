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

#ifndef _MLX5_FS_
#define _MLX5_FS_

#include <linux/list.h>
#include <linux/bitops.h>

#include <dev/mlx5/mlx5_ifc.h>
#include <dev/mlx5/device.h>
#include <dev/mlx5/driver.h>

enum mlx5_flow_destination_type {
        MLX5_FLOW_DESTINATION_TYPE_NONE,
        MLX5_FLOW_DESTINATION_TYPE_VPORT,
        MLX5_FLOW_DESTINATION_TYPE_FLOW_TABLE,
        MLX5_FLOW_DESTINATION_TYPE_TIR,
        MLX5_FLOW_DESTINATION_TYPE_FLOW_SAMPLER,
        MLX5_FLOW_DESTINATION_TYPE_UPLINK,
        MLX5_FLOW_DESTINATION_TYPE_PORT,
        MLX5_FLOW_DESTINATION_TYPE_COUNTER,
        MLX5_FLOW_DESTINATION_TYPE_FLOW_TABLE_NUM,
        MLX5_FLOW_DESTINATION_TYPE_RANGE,
        MLX5_FLOW_DESTINATION_TYPE_TABLE_TYPE,
};

enum {
        MLX5_FLOW_CONTEXT_ACTION_FWD_NEXT_PRIO  = 1 << 16,
        MLX5_FLOW_CONTEXT_ACTION_ENCRYPT        = 1 << 17,
        MLX5_FLOW_CONTEXT_ACTION_DECRYPT        = 1 << 18,
        MLX5_FLOW_CONTEXT_ACTION_FWD_NEXT_NS    = 1 << 19,
};

enum {
        MLX5_FLOW_TABLE_TUNNEL_EN_REFORMAT = BIT(0),
        MLX5_FLOW_TABLE_TUNNEL_EN_DECAP = BIT(1),
        MLX5_FLOW_TABLE_TERMINATION = BIT(2),
        MLX5_FLOW_TABLE_UNMANAGED = BIT(3),
        MLX5_FLOW_TABLE_OTHER_VPORT = BIT(4),
};

/*Flow tag*/
enum {
	MLX5_FS_DEFAULT_FLOW_TAG  = 0xFFFFFF,
	MLX5_FS_ETH_FLOW_TAG  = 0xFFFFFE,
	MLX5_FS_SNIFFER_FLOW_TAG  = 0xFFFFFD,
};

enum {
	MLX5_FS_FLOW_TAG_MASK = 0xFFFFFF,
};

#define FS_MAX_TYPES		10
#define FS_MAX_ENTRIES		32000U

#define	FS_REFORMAT_KEYWORD "_reformat"

enum mlx5_flow_namespace_type {
	MLX5_FLOW_NAMESPACE_BYPASS,
	MLX5_FLOW_NAMESPACE_KERNEL_RX_MACSEC,
	MLX5_FLOW_NAMESPACE_LAG,
	MLX5_FLOW_NAMESPACE_OFFLOADS,
	MLX5_FLOW_NAMESPACE_ETHTOOL,
	MLX5_FLOW_NAMESPACE_KERNEL,
	MLX5_FLOW_NAMESPACE_LEFTOVERS,
	MLX5_FLOW_NAMESPACE_ANCHOR,
	MLX5_FLOW_NAMESPACE_FDB_BYPASS,
	MLX5_FLOW_NAMESPACE_FDB,
	MLX5_FLOW_NAMESPACE_ESW_EGRESS,
	MLX5_FLOW_NAMESPACE_ESW_INGRESS,
	MLX5_FLOW_NAMESPACE_SNIFFER_RX,
	MLX5_FLOW_NAMESPACE_SNIFFER_TX,
	MLX5_FLOW_NAMESPACE_EGRESS,
	MLX5_FLOW_NAMESPACE_EGRESS_IPSEC,
	MLX5_FLOW_NAMESPACE_EGRESS_MACSEC,
	MLX5_FLOW_NAMESPACE_RDMA_RX,
	MLX5_FLOW_NAMESPACE_RDMA_RX_KERNEL,
	MLX5_FLOW_NAMESPACE_RDMA_TX,
	MLX5_FLOW_NAMESPACE_PORT_SEL,
	MLX5_FLOW_NAMESPACE_RDMA_RX_COUNTERS,
	MLX5_FLOW_NAMESPACE_RDMA_TX_COUNTERS,
	MLX5_FLOW_NAMESPACE_RDMA_RX_IPSEC,
	MLX5_FLOW_NAMESPACE_RDMA_TX_IPSEC,
};

enum {
	FDB_BYPASS_PATH,
	FDB_TC_OFFLOAD,
	FDB_FT_OFFLOAD,
	FDB_TC_MISS,
	FDB_BR_OFFLOAD,
	FDB_SLOW_PATH,
	FDB_PER_VPORT,
};

struct mlx5_flow_table;
struct mlx5_flow_group;
struct mlx5_flow_rule;
struct mlx5_flow_namespace;
struct mlx5_flow_handle;

enum {
	FLOW_CONTEXT_HAS_TAG = BIT(0),
};

struct mlx5_flow_context {
	u32 flags;
	u32 flow_tag;
	u32 flow_source;
};

struct mlx5_flow_spec {
	u8   match_criteria_enable;
	u32  match_criteria[MLX5_ST_SZ_DW(fte_match_param)];
	u32  match_value[MLX5_ST_SZ_DW(fte_match_param)];
	struct mlx5_flow_context flow_context;
};

enum {
	MLX5_FLOW_DEST_VPORT_VHCA_ID      = BIT(0),
	MLX5_FLOW_DEST_VPORT_REFORMAT_ID  = BIT(1),
};

enum mlx5_flow_dest_range_field {
	MLX5_FLOW_DEST_RANGE_FIELD_PKT_LEN = 0,
};

struct mlx5_flow_destination {
	enum mlx5_flow_destination_type type;
	union {
		u32                     tir_num;
		u32                     ft_num;
		struct mlx5_flow_table  *ft;
		u32                     counter_id;
		struct {
			u16             num;
			u16             vhca_id;
			struct mlx5_pkt_reformat *pkt_reformat;
			u8              flags;
		} vport;
		struct {
			struct mlx5_flow_table         *hit_ft;
			struct mlx5_flow_table         *miss_ft;
			enum mlx5_flow_dest_range_field field;
			u32                             min;
			u32                             max;
		} range;
		u32                     sampler_id;
	};
};

struct mlx5_exe_aso {
	u32 object_id;
	u8 type;
	u8 return_reg_id;
	union {
		u32 ctrl_data;
		struct {
			u8 meter_idx;
			u8 init_color;
		} flow_meter;
	};
};

enum {
	FLOW_ACT_NO_APPEND = BIT(0),
	FLOW_ACT_IGNORE_FLOW_LEVEL = BIT(1),
};

struct mlx5_fs_vlan {
	u16 ethtype;
	u16 vid;
	u8  prio;
};

#define MLX5_FS_VLAN_DEPTH      2

enum mlx5_flow_act_crypto_type {
	MLX5_FLOW_ACT_CRYPTO_TYPE_IPSEC,
};

enum mlx5_flow_act_crypto_op {
	MLX5_FLOW_ACT_CRYPTO_OP_ENCRYPT,
	MLX5_FLOW_ACT_CRYPTO_OP_DECRYPT,
};

struct mlx5_flow_act_crypto_params {
	u32 obj_id;
	u8 type; /* see enum mlx5_flow_act_crypto_type */
	u8 op; /* see enum mlx5_flow_act_crypto_op */
};

struct mlx5_flow_act {
	u32 action;
	struct mlx5_modify_hdr  *modify_hdr;
	struct mlx5_pkt_reformat *pkt_reformat;
	struct mlx5_flow_act_crypto_params crypto;
	u32 flags;
	struct mlx5_fs_vlan vlan[MLX5_FS_VLAN_DEPTH];
	struct ib_counters *counters;
	struct mlx5_flow_group *fg;
	struct mlx5_exe_aso exe_aso;
};

#define FT_NAME_STR_SZ 20
#define LEFTOVERS_RULE_NUM 2
static inline void build_leftovers_ft_param(char *name,
	unsigned int *priority,
	int *n_ent,
	int *n_grp)
{
	snprintf(name, FT_NAME_STR_SZ, "leftovers");
	*priority = 0; /*Priority of leftovers_prio-0*/
	*n_ent = LEFTOVERS_RULE_NUM + 1; /*1: star rules*/
	*n_grp = LEFTOVERS_RULE_NUM;
}

static inline bool outer_header_zero(u32 *match_criteria)
{
	int size = MLX5_ST_SZ_BYTES(fte_match_param);
	char *outer_headers_c = MLX5_ADDR_OF(fte_match_param, match_criteria,
					     outer_headers);

	return outer_headers_c[0] == 0 && !memcmp(outer_headers_c,
						  outer_headers_c + 1,
						  size - 1);
}

struct mlx5_flow_namespace *
mlx5_get_flow_vport_acl_namespace(struct mlx5_core_dev *dev,
                                  enum mlx5_flow_namespace_type type,
                                  int vport);

struct mlx5_flow_table_attr {
        int prio;
        int max_fte;
        u32 level;
        u32 flags;
        u16 uid;
        struct mlx5_flow_table *next_ft;

        struct {
                int max_num_groups;
                int num_reserved_entries;
        } autogroup;
};

struct mlx5_flow_namespace *
mlx5_get_fdb_sub_ns(struct mlx5_core_dev *dev, int n);

struct mlx5_flow_namespace *
mlx5_get_flow_namespace(struct mlx5_core_dev *dev,
			enum mlx5_flow_namespace_type type);

/* The underlying implementation create two more entries for
 * chaining flow tables. the user should be aware that if he pass
 * max_num_ftes as 2^N it will result in doubled size flow table
 */
struct mlx5_flow_table *
mlx5_create_auto_grouped_flow_table(struct mlx5_flow_namespace *ns,
				    struct mlx5_flow_table_attr *ft_attr);

struct mlx5_flow_table *
mlx5_create_vport_flow_table(struct mlx5_flow_namespace *ns,
                             struct mlx5_flow_table_attr *ft_attr, u16 vport);

struct mlx5_flow_table *mlx5_create_lag_demux_flow_table(
                                               struct mlx5_flow_namespace *ns,
                                               int prio, u32 level);

struct mlx5_flow_table *
mlx5_create_flow_table(struct mlx5_flow_namespace *ns,
                       struct mlx5_flow_table_attr *ft_attr);
int mlx5_destroy_flow_table(struct mlx5_flow_table *ft);

/* inbox should be set with the following values:
 * start_flow_index
 * end_flow_index
 * match_criteria_enable
 * match_criteria
 */
struct mlx5_flow_group *
mlx5_create_flow_group(struct mlx5_flow_table *ft, u32 *in);
void mlx5_destroy_flow_group(struct mlx5_flow_group *fg);

struct mlx5_flow_handle *
mlx5_add_flow_rules(struct mlx5_flow_table *ft,
                    const struct mlx5_flow_spec *spec,
                    struct mlx5_flow_act *flow_act,
                    struct mlx5_flow_destination *dest,
                    int num_dest);
void mlx5_del_flow_rules(struct mlx5_flow_handle **pp);

int mlx5_modify_rule_destination(struct mlx5_flow_handle *handler,
                                 struct mlx5_flow_destination *new_dest,
                                 struct mlx5_flow_destination *old_dest);

/*The following API is for sniffer*/
typedef int (*rule_event_fn)(struct mlx5_flow_rule *rule,
			     bool ctx_changed,
			     void *client_data,
			     void *context);

struct mlx5_flow_handler;

struct flow_client_priv_data;

void mlx5e_sniffer_roce_mode_notify(
	struct mlx5_core_dev *mdev,
	int action);

int mlx5_set_rule_private_data(struct mlx5_flow_rule *rule, struct
			       mlx5_flow_handler *handler,  void
			       *client_data);

struct mlx5_flow_handler *mlx5_register_rule_notifier(struct mlx5_core_dev *dev,
						      enum mlx5_flow_namespace_type ns_type,
						      rule_event_fn add_cb,
						      rule_event_fn del_cb,
						      void *context);

void mlx5_unregister_rule_notifier(struct mlx5_flow_handler *handler);

void mlx5_flow_iterate_existing_rules(struct mlx5_flow_namespace *ns,
					     rule_event_fn cb,
					     void *context);

void mlx5_get_match_criteria(u32 *match_criteria,
			     struct mlx5_flow_rule *rule);

void mlx5_get_match_value(u32 *match_value,
			  struct mlx5_flow_rule *rule);

u8 mlx5_get_match_criteria_enable(struct mlx5_flow_rule *rule);

struct mlx5_flow_rules_list *get_roce_flow_rules(u8 roce_mode);

void mlx5_del_flow_rules_list(struct mlx5_flow_rules_list *rules_list);

struct mlx5_flow_rules_list {
	struct list_head head;
};

struct mlx5_flow_rule_node {
	struct	list_head list;
	u32	match_criteria[MLX5_ST_SZ_DW(fte_match_param)];
	u32	match_value[MLX5_ST_SZ_DW(fte_match_param)];
	u8	match_criteria_enable;
};

struct mlx5_core_fs_mask {
	u8	match_criteria_enable;
	u32	match_criteria[MLX5_ST_SZ_DW(fte_match_param)];
};

bool fs_match_exact_val(
		struct mlx5_core_fs_mask *mask,
		void *val1,
		void *val2);

bool fs_match_exact_mask(
		u8 match_criteria_enable1,
		u8 match_criteria_enable2,
		void *mask1,
		void *mask2);
/**********end API for sniffer**********/
struct mlx5_modify_hdr *mlx5_modify_header_alloc(struct mlx5_core_dev *dev,
						 enum mlx5_flow_namespace_type ns_type,
						 u8 num_actions,
						 void *modify_actions);
void mlx5_modify_header_dealloc(struct mlx5_core_dev *dev,
				struct mlx5_modify_hdr *modify_hdr);

struct mlx5_pkt_reformat_params {
        int type;
        u8 param_0;
        u8 param_1;
        size_t size;
        void *data;
};

struct mlx5_pkt_reformat *mlx5_packet_reformat_alloc(struct mlx5_core_dev *dev,
						     struct mlx5_pkt_reformat_params *params,
						     enum mlx5_flow_namespace_type ns_type);
void mlx5_packet_reformat_dealloc(struct mlx5_core_dev *dev,
					  struct mlx5_pkt_reformat *pkt_reformat);
/********** Flow counters API **********/
struct mlx5_fc;
struct mlx5_fc *mlx5_fc_create(struct mlx5_core_dev *dev, bool aging);

/* As mlx5_fc_create() but doesn't queue stats refresh thread. */
struct mlx5_fc *mlx5_fc_create_ex(struct mlx5_core_dev *dev, bool aging);

void mlx5_fc_destroy(struct mlx5_core_dev *dev, struct mlx5_fc *counter);
u64 mlx5_fc_query_lastuse(struct mlx5_fc *counter);
void mlx5_fc_query_cached(struct mlx5_fc *counter,
                          u64 *bytes, u64 *packets, u64 *lastuse);
int mlx5_fc_query(struct mlx5_core_dev *dev, struct mlx5_fc *counter,
                  u64 *packets, u64 *bytes);
u32 mlx5_fc_id(struct mlx5_fc *counter);
/******* End of Flow counters API ******/

u32 mlx5_flow_table_id(struct mlx5_flow_table *ft);
int mlx5_fs_add_rx_underlay_qpn(struct mlx5_core_dev *dev, u32 underlay_qpn);
int mlx5_fs_remove_rx_underlay_qpn(struct mlx5_core_dev *dev, u32 underlay_qpn);
#endif
