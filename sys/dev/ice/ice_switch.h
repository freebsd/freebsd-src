/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2022, Intel Corporation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of the Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */
/*$FreeBSD$*/

#ifndef _ICE_SWITCH_H_
#define _ICE_SWITCH_H_

#include "ice_type.h"
#include "ice_protocol_type.h"

#define ICE_SW_CFG_MAX_BUF_LEN 2048
#define ICE_MAX_SW 256
#define ICE_DFLT_VSI_INVAL 0xff
#define ICE_FLTR_RX BIT(0)
#define ICE_FLTR_TX BIT(1)
#define ICE_FLTR_TX_RX (ICE_FLTR_RX | ICE_FLTR_TX)

#define ICE_PROFID_IPV4_GTPC_TEID	41
#define ICE_PROFID_IPV4_GTPC_NO_TEID		42
#define ICE_PROFID_IPV4_GTPU_TEID		43
#define ICE_PROFID_IPV6_GTPC_TEID		44
#define ICE_PROFID_IPV6_GTPC_NO_TEID		45
#define ICE_PROFID_IPV6_GTPU_TEID		46
#define ICE_PROFID_IPV6_GTPU_IPV6_TCP		70

#define DUMMY_ETH_HDR_LEN		16
#define ICE_SW_RULE_RX_TX_ETH_HDR_SIZE \
	(offsetof(struct ice_aqc_sw_rules_elem, pdata.lkup_tx_rx.hdr) + \
	 (DUMMY_ETH_HDR_LEN * \
	  sizeof(((struct ice_sw_rule_lkup_rx_tx *)0)->hdr[0])))
#define ICE_SW_RULE_RX_TX_NO_HDR_SIZE \
	(offsetof(struct ice_aqc_sw_rules_elem, pdata.lkup_tx_rx.hdr))
#define ICE_SW_RULE_LG_ACT_SIZE(n) \
	(offsetof(struct ice_aqc_sw_rules_elem, pdata.lg_act.act) + \
	 ((n) * sizeof(((struct ice_sw_rule_lg_act *)0)->act[0])))
#define ICE_SW_RULE_VSI_LIST_SIZE(n) \
	(offsetof(struct ice_aqc_sw_rules_elem, pdata.vsi_list.vsi) + \
	 ((n) * sizeof(((struct ice_sw_rule_vsi_list *)0)->vsi[0])))

/* Worst case buffer length for ice_aqc_opc_get_res_alloc */
#define ICE_MAX_RES_TYPES 0x80
#define ICE_AQ_GET_RES_ALLOC_BUF_LEN \
	(ICE_MAX_RES_TYPES * sizeof(struct ice_aqc_get_res_resp_elem))

#define ICE_VSI_INVAL_ID 0xFFFF
#define ICE_INVAL_Q_HANDLE 0xFFFF

/* VSI context structure for add/get/update/free operations */
struct ice_vsi_ctx {
	u16 vsi_num;
	u16 vsis_allocd;
	u16 vsis_unallocated;
	u16 flags;
	struct ice_aqc_vsi_props info;
	struct ice_sched_vsi_info sched;
	u8 alloc_from_pool;
	u8 vf_num;
	u16 num_lan_q_entries[ICE_MAX_TRAFFIC_CLASS];
	struct ice_q_ctx *lan_q_ctx[ICE_MAX_TRAFFIC_CLASS];
	u16 num_rdma_q_entries[ICE_MAX_TRAFFIC_CLASS];
	struct ice_q_ctx *rdma_q_ctx[ICE_MAX_TRAFFIC_CLASS];
};

/* This is to be used by add/update mirror rule Admin Queue command */
struct ice_mir_rule_buf {
	u16 vsi_idx; /* VSI index */

	/* For each VSI, user can specify whether corresponding VSI
	 * should be added/removed to/from mirror rule
	 *
	 * add mirror rule: this should always be TRUE.
	 * update mirror rule:  add(true) or remove(false) VSI to/from
	 * mirror rule
	 */
	u8 add;
};

/* Switch recipe ID enum values are specific to hardware */
enum ice_sw_lkup_type {
	ICE_SW_LKUP_ETHERTYPE = 0,
	ICE_SW_LKUP_MAC = 1,
	ICE_SW_LKUP_MAC_VLAN = 2,
	ICE_SW_LKUP_PROMISC = 3,
	ICE_SW_LKUP_VLAN = 4,
	ICE_SW_LKUP_DFLT = 5,
	ICE_SW_LKUP_ETHERTYPE_MAC = 8,
	ICE_SW_LKUP_PROMISC_VLAN = 9,
	ICE_SW_LKUP_LAST
};

/* type of filter src ID */
enum ice_src_id {
	ICE_SRC_ID_UNKNOWN = 0,
	ICE_SRC_ID_VSI,
	ICE_SRC_ID_QUEUE,
	ICE_SRC_ID_LPORT,
};

struct ice_fltr_info {
	/* Look up information: how to look up packet */
	enum ice_sw_lkup_type lkup_type;
	/* Forward action: filter action to do after lookup */
	enum ice_sw_fwd_act_type fltr_act;
	/* rule ID returned by firmware once filter rule is created */
	u16 fltr_rule_id;
	u16 flag;

	/* Source VSI for LOOKUP_TX or source port for LOOKUP_RX */
	u16 src;
	enum ice_src_id src_id;

	union {
		struct {
			u8 mac_addr[ETH_ALEN];
		} mac;
		struct {
			u8 mac_addr[ETH_ALEN];
			u16 vlan_id;
		} mac_vlan;
		struct {
			u16 vlan_id;
			u16 tpid;
			u8 tpid_valid;
		} vlan;
		/* Set lkup_type as ICE_SW_LKUP_ETHERTYPE
		 * if just using ethertype as filter. Set lkup_type as
		 * ICE_SW_LKUP_ETHERTYPE_MAC if MAC also needs to be
		 * passed in as filter.
		 */
		struct {
			u16 ethertype;
			u8 mac_addr[ETH_ALEN]; /* optional */
		} ethertype_mac;
	} l_data; /* Make sure to zero out the memory of l_data before using
		   * it or only set the data associated with lookup match
		   * rest everything should be zero
		   */

	/* Depending on filter action */
	union {
		/* queue ID in case of ICE_FWD_TO_Q and starting
		 * queue ID in case of ICE_FWD_TO_QGRP.
		 */
		u16 q_id:11;
		u16 hw_vsi_id:10;
		u16 vsi_list_id:10;
	} fwd_id;

	/* Sw VSI handle */
	u16 vsi_handle;

	/* Set to num_queues if action is ICE_FWD_TO_QGRP. This field
	 * determines the range of queues the packet needs to be forwarded to.
	 * Note that qgrp_size must be set to a power of 2.
	 */
	u8 qgrp_size;

	/* Rule creations populate these indicators basing on the switch type */
	u8 lb_en;	/* Indicate if packet can be looped back */
	u8 lan_en;	/* Indicate if packet can be forwarded to the uplink */
};

struct ice_adv_lkup_elem {
	enum ice_protocol_type type;
	union ice_prot_hdr h_u;	/* Header values */
	union ice_prot_hdr m_u;	/* Mask of header values to match */
};

struct ice_sw_act_ctrl {
	/* Source VSI for LOOKUP_TX or source port for LOOKUP_RX */
	u16 src;
	u16 flag;
	enum ice_sw_fwd_act_type fltr_act;
	/* Depending on filter action */
	union {
		/* This is a queue ID in case of ICE_FWD_TO_Q and starting
		 * queue ID in case of ICE_FWD_TO_QGRP.
		 */
		u16 q_id:11;
		u16 vsi_id:10;
		u16 hw_vsi_id:10;
		u16 vsi_list_id:10;
	} fwd_id;
	/* software VSI handle */
	u16 vsi_handle;
	u8 qgrp_size;
};

struct ice_rule_query_data {
	/* Recipe ID for which the requested rule was added */
	u16 rid;
	/* Rule ID that was added or is supposed to be removed */
	u16 rule_id;
	/* vsi_handle for which Rule was added or is supposed to be removed */
	u16 vsi_handle;
};

/*
 * This structure allows to pass info about lb_en and lan_en
 * flags to ice_add_adv_rule. Values in act would be used
 * only if act_valid was set to true, otherwise dflt
 * values would be used.
 */
struct ice_adv_rule_flags_info {
	u32 act;
	u8 act_valid;		/* indicate if flags in act are valid */
};

struct ice_adv_rule_info {
	enum ice_sw_tunnel_type tun_type;
	struct ice_sw_act_ctrl sw_act;
	u32 priority;
	u8 rx; /* true means LOOKUP_RX otherwise LOOKUP_TX */
	u16 fltr_rule_id;
	u16 lg_id;
	struct ice_adv_rule_flags_info flags_info;
};

/* A collection of one or more four word recipe */
struct ice_sw_recipe {
	/* For a chained recipe the root recipe is what should be used for
	 * programming rules
	 */
	u8 is_root;
	u8 root_rid;
	u8 recp_created;

	/* Number of extraction words */
	u8 n_ext_words;
	/* Protocol ID and Offset pair (extraction word) to describe the
	 * recipe
	 */
	struct ice_fv_word ext_words[ICE_MAX_CHAIN_WORDS];
	u16 word_masks[ICE_MAX_CHAIN_WORDS];

	/* if this recipe is a collection of other recipe */
	u8 big_recp;

	/* if this recipe is part of another bigger recipe then chain index
	 * corresponding to this recipe
	 */
	u8 chain_idx;

	/* if this recipe is a collection of other recipe then count of other
	 * recipes and recipe IDs of those recipes
	 */
	u8 n_grp_count;

	/* Bit map specifying the IDs associated with this group of recipe */
	ice_declare_bitmap(r_bitmap, ICE_MAX_NUM_RECIPES);

	enum ice_sw_tunnel_type tun_type;

	/* List of type ice_fltr_mgmt_list_entry or adv_rule */
	u8 adv_rule;
	struct LIST_HEAD_TYPE filt_rules;
	struct LIST_HEAD_TYPE filt_replay_rules;

	struct ice_lock filt_rule_lock;	/* protect filter rule structure */

	/* Profiles this recipe should be associated with */
	struct LIST_HEAD_TYPE fv_list;

	/* Profiles this recipe is associated with */
	u8 num_profs, *prof_ids;

	/* Bit map for possible result indexes */
	ice_declare_bitmap(res_idxs, ICE_MAX_FV_WORDS);

	/* This allows user to specify the recipe priority.
	 * For now, this becomes 'fwd_priority' when recipe
	 * is created, usually recipes can have 'fwd' and 'join'
	 * priority.
	 */
	u8 priority;

	struct LIST_HEAD_TYPE rg_list;

	/* AQ buffer associated with this recipe */
	struct ice_aqc_recipe_data_elem *root_buf;
	/* This struct saves the fv_words for a given lookup */
	struct ice_prot_lkup_ext lkup_exts;
};

/* Bookkeeping structure to hold bitmap of VSIs corresponding to VSI list ID */
struct ice_vsi_list_map_info {
	struct LIST_ENTRY_TYPE list_entry;
	ice_declare_bitmap(vsi_map, ICE_MAX_VSI);
	u16 vsi_list_id;
	/* counter to track how many rules are reusing this VSI list */
	u16 ref_cnt;
};

struct ice_fltr_list_entry {
	struct LIST_ENTRY_TYPE list_entry;
	enum ice_status status;
	struct ice_fltr_info fltr_info;
};

/**
 * enum ice_fltr_marker - Marker for syncing OS and driver filter lists
 * @ICE_FLTR_NOT_FOUND: initial state, indicates filter has not been found
 * @ICE_FLTR_FOUND: set when a filter has been found in both lists
 *
 * This enumeration is used to help sync an operating system provided filter
 * list with the filters previously added.
 *
 * This is required for FreeBSD because the operating system does not provide
 * individual indications of whether a filter has been added or deleted, but
 * instead just notifies the driver with the entire new list.
 *
 * To use this marker state, the driver shall initially reset all filters to
 * the ICE_FLTR_NOT_FOUND state. Then, for each filter in the OS list, it
 * shall search the driver list for the filter. If found, the filter state
 * will be set to ICE_FLTR_FOUND. If not found, that filter will be added.
 * Finally, the driver shall search the internal filter list for all filters
 * still marked as ICE_FLTR_NOT_FOUND and remove them.
 */
enum ice_fltr_marker {
	ICE_FLTR_NOT_FOUND,
	ICE_FLTR_FOUND,
};

/* This defines an entry in the list that maintains MAC or VLAN membership
 * to HW list mapping, since multiple VSIs can subscribe to the same MAC or
 * VLAN. As an optimization the VSI list should be created only when a
 * second VSI becomes a subscriber to the same MAC address. VSI lists are always
 * used for VLAN membership.
 */
struct ice_fltr_mgmt_list_entry {
	/* back pointer to VSI list ID to VSI list mapping */
	struct ice_vsi_list_map_info *vsi_list_info;
	u16 vsi_count;
#define ICE_INVAL_LG_ACT_INDEX 0xffff
	u16 lg_act_idx;
#define ICE_INVAL_SW_MARKER_ID 0xffff
	u16 sw_marker_id;
	struct LIST_ENTRY_TYPE list_entry;
	struct ice_fltr_info fltr_info;
#define ICE_INVAL_COUNTER_ID 0xff
	u8 counter_index;
	enum ice_fltr_marker marker;
};

struct ice_adv_fltr_mgmt_list_entry {
	struct LIST_ENTRY_TYPE list_entry;

	struct ice_adv_lkup_elem *lkups;
	struct ice_adv_rule_info rule_info;
	u16 lkups_cnt;
	struct ice_vsi_list_map_info *vsi_list_info;
	u16 vsi_count;
};

enum ice_promisc_flags {
	ICE_PROMISC_UCAST_RX = 0x1,
	ICE_PROMISC_UCAST_TX = 0x2,
	ICE_PROMISC_MCAST_RX = 0x4,
	ICE_PROMISC_MCAST_TX = 0x8,
	ICE_PROMISC_BCAST_RX = 0x10,
	ICE_PROMISC_BCAST_TX = 0x20,
	ICE_PROMISC_VLAN_RX = 0x40,
	ICE_PROMISC_VLAN_TX = 0x80,
};

struct ice_dummy_pkt_offsets {
	enum ice_protocol_type type;
	u16 offset; /* ICE_PROTOCOL_LAST indicates end of list */
};

void
ice_find_dummy_packet(struct ice_adv_lkup_elem *lkups, u16 lkups_cnt,
		      enum ice_sw_tunnel_type tun_type, const u8 **pkt,
		      u16 *pkt_len,
		      const struct ice_dummy_pkt_offsets **offsets);

enum ice_status
ice_fill_adv_dummy_packet(struct ice_adv_lkup_elem *lkups, u16 lkups_cnt,
			  struct ice_aqc_sw_rules_elem *s_rule,
			  const u8 *dummy_pkt, u16 pkt_len,
			  const struct ice_dummy_pkt_offsets *offsets);

enum ice_status
ice_add_adv_recipe(struct ice_hw *hw, struct ice_adv_lkup_elem *lkups,
		   u16 lkups_cnt, struct ice_adv_rule_info *rinfo, u16 *rid);

struct ice_adv_fltr_mgmt_list_entry *
ice_find_adv_rule_entry(struct ice_hw *hw, struct ice_adv_lkup_elem *lkups,
			u16 lkups_cnt, u16 recp_id,
			struct ice_adv_rule_info *rinfo);

enum ice_status
ice_adv_add_update_vsi_list(struct ice_hw *hw,
			    struct ice_adv_fltr_mgmt_list_entry *m_entry,
			    struct ice_adv_rule_info *cur_fltr,
			    struct ice_adv_rule_info *new_fltr);

struct ice_vsi_list_map_info *
ice_find_vsi_list_entry(struct ice_sw_recipe *recp_list, u16 vsi_handle,
			u16 *vsi_list_id);

/* VSI related commands */
enum ice_status
ice_aq_add_vsi(struct ice_hw *hw, struct ice_vsi_ctx *vsi_ctx,
	       struct ice_sq_cd *cd);
enum ice_status
ice_aq_free_vsi(struct ice_hw *hw, struct ice_vsi_ctx *vsi_ctx,
		bool keep_vsi_alloc, struct ice_sq_cd *cd);
enum ice_status
ice_aq_update_vsi(struct ice_hw *hw, struct ice_vsi_ctx *vsi_ctx,
		  struct ice_sq_cd *cd);
enum ice_status
ice_add_vsi(struct ice_hw *hw, u16 vsi_handle, struct ice_vsi_ctx *vsi_ctx,
	    struct ice_sq_cd *cd);
enum ice_status
ice_free_vsi(struct ice_hw *hw, u16 vsi_handle, struct ice_vsi_ctx *vsi_ctx,
	     bool keep_vsi_alloc, struct ice_sq_cd *cd);
enum ice_status
ice_update_vsi(struct ice_hw *hw, u16 vsi_handle, struct ice_vsi_ctx *vsi_ctx,
	       struct ice_sq_cd *cd);
struct ice_vsi_ctx *ice_get_vsi_ctx(struct ice_hw *hw, u16 vsi_handle);
void ice_clear_all_vsi_ctx(struct ice_hw *hw);
enum ice_status
ice_aq_get_vsi_params(struct ice_hw *hw, struct ice_vsi_ctx *vsi_ctx,
		      struct ice_sq_cd *cd);
enum ice_status
ice_aq_add_update_mir_rule(struct ice_hw *hw, u16 rule_type, u16 dest_vsi,
			   u16 count, struct ice_mir_rule_buf *mr_buf,
			   struct ice_sq_cd *cd, u16 *rule_id);
enum ice_status
ice_aq_delete_mir_rule(struct ice_hw *hw, u16 rule_id, bool keep_allocd,
		       struct ice_sq_cd *cd);
enum ice_status
ice_aq_get_storm_ctrl(struct ice_hw *hw, u32 *bcast_thresh, u32 *mcast_thresh,
		      u32 *ctl_bitmask);
enum ice_status
ice_aq_set_storm_ctrl(struct ice_hw *hw, u32 bcast_thresh, u32 mcast_thresh,
		      u32 ctl_bitmask);
/* Switch config */
enum ice_status ice_get_initial_sw_cfg(struct ice_hw *hw);

enum ice_status
ice_alloc_vlan_res_counter(struct ice_hw *hw, u16 *counter_id);
enum ice_status
ice_free_vlan_res_counter(struct ice_hw *hw, u16 counter_id);

enum ice_status ice_update_sw_rule_bridge_mode(struct ice_hw *hw);
enum ice_status ice_alloc_rss_global_lut(struct ice_hw *hw, bool shared_res, u16 *global_lut_id);
enum ice_status ice_free_rss_global_lut(struct ice_hw *hw, u16 global_lut_id);
enum ice_status
ice_alloc_sw(struct ice_hw *hw, bool ena_stats, bool shared_res, u16 *sw_id,
	     u16 *counter_id);
enum ice_status
ice_free_sw(struct ice_hw *hw, u16 sw_id, u16 counter_id);
enum ice_status
ice_aq_get_res_alloc(struct ice_hw *hw, u16 *num_entries,
		     struct ice_aqc_get_res_resp_elem *buf, u16 buf_size,
		     struct ice_sq_cd *cd);
enum ice_status
ice_aq_get_res_descs(struct ice_hw *hw, u16 num_entries,
		     struct ice_aqc_res_elem *buf, u16 buf_size, u16 res_type,
		     bool res_shared, u16 *desc_id, struct ice_sq_cd *cd);
enum ice_status
ice_add_vlan(struct ice_hw *hw, struct LIST_HEAD_TYPE *m_list);
enum ice_status
ice_remove_vlan(struct ice_hw *hw, struct LIST_HEAD_TYPE *v_list);
void ice_rem_all_sw_rules_info(struct ice_hw *hw);
enum ice_status ice_add_mac(struct ice_hw *hw, struct LIST_HEAD_TYPE *m_lst);
enum ice_status ice_remove_mac(struct ice_hw *hw, struct LIST_HEAD_TYPE *m_lst);
enum ice_status
ice_add_eth_mac(struct ice_hw *hw, struct LIST_HEAD_TYPE *em_list);
enum ice_status
ice_remove_eth_mac(struct ice_hw *hw, struct LIST_HEAD_TYPE *em_list);
enum ice_status
ice_cfg_iwarp_fltr(struct ice_hw *hw, u16 vsi_handle, bool enable);

enum ice_status
ice_add_mac_with_sw_marker(struct ice_hw *hw, struct ice_fltr_info *f_info,
			   u16 sw_marker);
enum ice_status
ice_add_mac_with_counter(struct ice_hw *hw, struct ice_fltr_info *f_info);
void ice_remove_vsi_fltr(struct ice_hw *hw, u16 vsi_handle);

/* Promisc/defport setup for VSIs */
enum ice_status
ice_cfg_dflt_vsi(struct ice_port_info *pi, u16 vsi_handle, bool set,
		 u8 direction);
bool ice_check_if_dflt_vsi(struct ice_port_info *pi, u16 vsi_handle,
			   bool *rule_exists);
enum ice_status
ice_set_vsi_promisc(struct ice_hw *hw, u16 vsi_handle, u8 promisc_mask,
		    u16 vid);
enum ice_status
ice_clear_vsi_promisc(struct ice_hw *hw, u16 vsi_handle, u8 promisc_mask,
		      u16 vid);
enum ice_status
ice_set_vlan_vsi_promisc(struct ice_hw *hw, u16 vsi_handle, u8 promisc_mask,
			 bool rm_vlan_promisc);

/* Get VSIs Promisc/defport settings */
enum ice_status
ice_get_vsi_promisc(struct ice_hw *hw, u16 vsi_handle, u8 *promisc_mask,
		    u16 *vid);
enum ice_status
ice_get_vsi_vlan_promisc(struct ice_hw *hw, u16 vsi_handle, u8 *promisc_mask,
			 u16 *vid);

enum ice_status ice_replay_all_fltr(struct ice_hw *hw);

enum ice_status
ice_init_def_sw_recp(struct ice_hw *hw, struct ice_sw_recipe **recp_list);
u16 ice_get_hw_vsi_num(struct ice_hw *hw, u16 vsi_handle);
bool ice_is_vsi_valid(struct ice_hw *hw, u16 vsi_handle);

enum ice_status
ice_replay_vsi_all_fltr(struct ice_hw *hw, struct ice_port_info *pi,
			u16 vsi_handle);
void ice_rm_sw_replay_rule_info(struct ice_hw *hw, struct ice_switch_info *sw);
void ice_rm_all_sw_replay_rule_info(struct ice_hw *hw);
enum ice_status
ice_aq_sw_rules(struct ice_hw *hw, void *rule_list, u16 rule_list_sz,
		u8 num_rules, enum ice_adminq_opc opc, struct ice_sq_cd *cd);
#endif /* _ICE_SWITCH_H_ */
