/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2020, Intel Corporation
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

#ifndef _ICE_DCB_H_
#define _ICE_DCB_H_

#include "ice_type.h"
#include "ice_common.h"

#define ICE_DCBX_OFFLOAD_DIS		0
#define ICE_DCBX_OFFLOAD_ENABLED	1

#define ICE_DCBX_STATUS_NOT_STARTED	0
#define ICE_DCBX_STATUS_IN_PROGRESS	1
#define ICE_DCBX_STATUS_DONE		2
#define ICE_DCBX_STATUS_MULTIPLE_PEERS	3
#define ICE_DCBX_STATUS_DIS		7

#define ICE_TLV_TYPE_END		0
#define ICE_TLV_TYPE_ORG		127

#define ICE_IEEE_8021QAZ_OUI		0x0080C2
#define ICE_IEEE_SUBTYPE_ETS_CFG	9
#define ICE_IEEE_SUBTYPE_ETS_REC	10
#define ICE_IEEE_SUBTYPE_PFC_CFG	11
#define ICE_IEEE_SUBTYPE_APP_PRI	12

#define ICE_CEE_DCBX_OUI		0x001B21
#define ICE_CEE_DCBX_TYPE		2

#define ICE_CEE_SUBTYPE_CTRL		1
#define ICE_CEE_SUBTYPE_PG_CFG		2
#define ICE_CEE_SUBTYPE_PFC_CFG		3
#define ICE_CEE_SUBTYPE_APP_PRI		4

#define ICE_CEE_MAX_FEAT_TYPE		3
#define ICE_LLDP_ADMINSTATUS_DIS	0
#define ICE_LLDP_ADMINSTATUS_ENA_RX	1
#define ICE_LLDP_ADMINSTATUS_ENA_TX	2
#define ICE_LLDP_ADMINSTATUS_ENA_RXTX	3

/* Defines for LLDP TLV header */
#define ICE_LLDP_TLV_LEN_S		0
#define ICE_LLDP_TLV_LEN_M		(0x01FF << ICE_LLDP_TLV_LEN_S)
#define ICE_LLDP_TLV_TYPE_S		9
#define ICE_LLDP_TLV_TYPE_M		(0x7F << ICE_LLDP_TLV_TYPE_S)
#define ICE_LLDP_TLV_SUBTYPE_S		0
#define ICE_LLDP_TLV_SUBTYPE_M		(0xFF << ICE_LLDP_TLV_SUBTYPE_S)
#define ICE_LLDP_TLV_OUI_S		8
#define ICE_LLDP_TLV_OUI_M		(0xFFFFFFUL << ICE_LLDP_TLV_OUI_S)

/* Defines for IEEE ETS TLV */
#define ICE_IEEE_ETS_MAXTC_S	0
#define ICE_IEEE_ETS_MAXTC_M		(0x7 << ICE_IEEE_ETS_MAXTC_S)
#define ICE_IEEE_ETS_CBS_S		6
#define ICE_IEEE_ETS_CBS_M		BIT(ICE_IEEE_ETS_CBS_S)
#define ICE_IEEE_ETS_WILLING_S		7
#define ICE_IEEE_ETS_WILLING_M		BIT(ICE_IEEE_ETS_WILLING_S)
#define ICE_IEEE_ETS_PRIO_0_S		0
#define ICE_IEEE_ETS_PRIO_0_M		(0x7 << ICE_IEEE_ETS_PRIO_0_S)
#define ICE_IEEE_ETS_PRIO_1_S		4
#define ICE_IEEE_ETS_PRIO_1_M		(0x7 << ICE_IEEE_ETS_PRIO_1_S)
#define ICE_CEE_PGID_PRIO_0_S		0
#define ICE_CEE_PGID_PRIO_0_M		(0xF << ICE_CEE_PGID_PRIO_0_S)
#define ICE_CEE_PGID_PRIO_1_S		4
#define ICE_CEE_PGID_PRIO_1_M		(0xF << ICE_CEE_PGID_PRIO_1_S)
#define ICE_CEE_PGID_STRICT		15

/* Defines for IEEE TSA types */
#define ICE_IEEE_TSA_STRICT		0
#define ICE_IEEE_TSA_CBS		1
#define ICE_IEEE_TSA_ETS		2
#define ICE_IEEE_TSA_VENDOR		255

/* Defines for IEEE PFC TLV */
#define ICE_IEEE_PFC_CAP_S		0
#define ICE_IEEE_PFC_CAP_M		(0xF << ICE_IEEE_PFC_CAP_S)
#define ICE_IEEE_PFC_MBC_S		6
#define ICE_IEEE_PFC_MBC_M		BIT(ICE_IEEE_PFC_MBC_S)
#define ICE_IEEE_PFC_WILLING_S		7
#define ICE_IEEE_PFC_WILLING_M		BIT(ICE_IEEE_PFC_WILLING_S)

/* Defines for IEEE APP TLV */
#define ICE_IEEE_APP_SEL_S		0
#define ICE_IEEE_APP_SEL_M		(0x7 << ICE_IEEE_APP_SEL_S)
#define ICE_IEEE_APP_PRIO_S		5
#define ICE_IEEE_APP_PRIO_M		(0x7 << ICE_IEEE_APP_PRIO_S)

/* TLV definitions for preparing MIB */
#define ICE_TLV_ID_CHASSIS_ID		0
#define ICE_TLV_ID_PORT_ID		1
#define ICE_TLV_ID_TIME_TO_LIVE		2
#define ICE_IEEE_TLV_ID_ETS_CFG		3
#define ICE_IEEE_TLV_ID_ETS_REC		4
#define ICE_IEEE_TLV_ID_PFC_CFG		5
#define ICE_IEEE_TLV_ID_APP_PRI		6
#define ICE_TLV_ID_END_OF_LLDPPDU	7
#define ICE_TLV_ID_START		ICE_IEEE_TLV_ID_ETS_CFG

#define ICE_IEEE_ETS_TLV_LEN		25
#define ICE_IEEE_PFC_TLV_LEN		6
#define ICE_IEEE_APP_TLV_LEN		11

#pragma pack(1)
/* IEEE 802.1AB LLDP Organization specific TLV */
struct ice_lldp_org_tlv {
	__be16 typelen;
	__be32 ouisubtype;
	u8 tlvinfo[STRUCT_HACK_VAR_LEN];
};
#pragma pack()

struct ice_cee_tlv_hdr {
	__be16 typelen;
	u8 operver;
	u8 maxver;
};

struct ice_cee_ctrl_tlv {
	struct ice_cee_tlv_hdr hdr;
	__be32 seqno;
	__be32 ackno;
};

struct ice_cee_feat_tlv {
	struct ice_cee_tlv_hdr hdr;
	u8 en_will_err; /* Bits: |En|Will|Err|Reserved(5)| */
#define ICE_CEE_FEAT_TLV_ENA_M		0x80
#define ICE_CEE_FEAT_TLV_WILLING_M	0x40
#define ICE_CEE_FEAT_TLV_ERR_M		0x20
	u8 subtype;
	u8 tlvinfo[STRUCT_HACK_VAR_LEN];
};

#pragma pack(1)
struct ice_cee_app_prio {
	__be16 protocol;
	u8 upper_oui_sel; /* Bits: |Upper OUI(6)|Selector(2)| */
#define ICE_CEE_APP_SELECTOR_M	0x03
	__be16 lower_oui;
	u8 prio_map;
};
#pragma pack()

/* TODO: The below structures related LLDP/DCBX variables
 * and statistics are defined but need to find how to get
 * the required information from the Firmware to use them
 */

/* IEEE 802.1AB LLDP Agent Statistics */
struct ice_lldp_stats {
	u64 remtablelastchangetime;
	u64 remtableinserts;
	u64 remtabledeletes;
	u64 remtabledrops;
	u64 remtableageouts;
	u64 txframestotal;
	u64 rxframesdiscarded;
	u64 rxportframeerrors;
	u64 rxportframestotal;
	u64 rxporttlvsdiscardedtotal;
	u64 rxporttlvsunrecognizedtotal;
	u64 remtoomanyneighbors;
};

/* IEEE 802.1Qaz DCBX variables */
struct ice_dcbx_variables {
	u32 defmaxtrafficclasses;
	u32 defprioritytcmapping;
	u32 deftcbandwidth;
	u32 deftsaassignment;
};

enum ice_status
ice_aq_get_lldp_mib(struct ice_hw *hw, u8 bridge_type, u8 mib_type, void *buf,
		    u16 buf_size, u16 *local_len, u16 *remote_len,
		    struct ice_sq_cd *cd);
enum ice_status
ice_aq_add_delete_lldp_tlv(struct ice_hw *hw, u8 bridge_type, bool add_lldp_tlv,
			   void *buf, u16 buf_size, u16 tlv_len, u16 *mib_len,
			   struct ice_sq_cd *cd);
enum ice_status
ice_aq_update_lldp_tlv(struct ice_hw *hw, u8 bridge_type, void *buf,
		       u16 buf_size, u16 old_len, u16 new_len, u16 offset,
		       u16 *mib_len, struct ice_sq_cd *cd);
enum ice_status
ice_aq_dcb_ignore_pfc(struct ice_hw *hw, u8 tcmap, bool request, u8 *tcmap_ret,
		      struct ice_sq_cd *cd);
enum ice_status
ice_aq_get_cee_dcb_cfg(struct ice_hw *hw,
		       struct ice_aqc_get_cee_dcb_cfg_resp *buff,
		       struct ice_sq_cd *cd);
enum ice_status
ice_aq_query_pfc_mode(struct ice_hw *hw, u8 *pfcmode_ret, struct ice_sq_cd *cd);
enum ice_status
ice_aq_set_pfc_mode(struct ice_hw *hw, u8 pfcmode_set, u8 *pfcmode_ret,
		    struct ice_sq_cd *cd);
enum ice_status
ice_aq_set_dcb_parameters(struct ice_hw *hw, bool dcb_enable,
			  struct ice_sq_cd *cd);
enum ice_status ice_lldp_to_dcb_cfg(u8 *lldpmib, struct ice_dcbx_cfg *dcbcfg);
u8 ice_get_dcbx_status(struct ice_hw *hw);
enum ice_status
ice_aq_get_dcb_cfg(struct ice_hw *hw, u8 mib_type, u8 bridgetype,
		   struct ice_dcbx_cfg *dcbcfg);
enum ice_status ice_get_dcb_cfg(struct ice_port_info *pi);
enum ice_status ice_set_dcb_cfg(struct ice_port_info *pi);
enum ice_status ice_init_dcb(struct ice_hw *hw, bool enable_mib_change);
void ice_dcb_cfg_to_lldp(u8 *lldpmib, u16 *miblen, struct ice_dcbx_cfg *dcbcfg);
enum ice_status
ice_query_port_ets(struct ice_port_info *pi,
		   struct ice_aqc_port_ets_elem *buf, u16 buf_size,
		   struct ice_sq_cd *cmd_details);
enum ice_status
ice_aq_query_port_ets(struct ice_port_info *pi,
		      struct ice_aqc_port_ets_elem *buf, u16 buf_size,
		      struct ice_sq_cd *cd);
enum ice_status
ice_update_port_tc_tree_cfg(struct ice_port_info *pi,
			    struct ice_aqc_port_ets_elem *buf);
enum ice_status
ice_aq_stop_lldp(struct ice_hw *hw, bool shutdown_lldp_agent, bool persist,
		 struct ice_sq_cd *cd);
enum ice_status
ice_aq_start_lldp(struct ice_hw *hw, bool persist, struct ice_sq_cd *cd);
enum ice_status
ice_aq_start_stop_dcbx(struct ice_hw *hw, bool start_dcbx_agent,
		       bool *dcbx_agent_status, struct ice_sq_cd *cd);
enum ice_status ice_cfg_lldp_mib_change(struct ice_hw *hw, bool ena_mib);
enum ice_status
ice_aq_cfg_lldp_mib_change(struct ice_hw *hw, bool ena_update,
			   struct ice_sq_cd *cd);
#endif /* _ICE_DCB_H_ */
