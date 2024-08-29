/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2024, Intel Corporation
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

#ifndef _ICE_ADMINQ_CMD_H_
#define _ICE_ADMINQ_CMD_H_

/* This header file defines the Admin Queue commands, error codes and
 * descriptor format. It is shared between Firmware and Software.
 */

#include "ice_osdep.h"
#include "ice_defs.h"
#include "ice_bitops.h"

#define ICE_MAX_VSI			768
#define ICE_AQC_TOPO_MAX_LEVEL_NUM	0x9
#define ICE_AQ_SET_MAC_FRAME_SIZE_MAX	9728

enum ice_aq_res_access_type {
	ICE_RES_READ = 1,
	ICE_RES_WRITE
};

struct ice_aqc_generic {
	__le32 param0;
	__le32 param1;
	__le32 addr_high;
	__le32 addr_low;
};

/* Get version (direct 0x0001) */
struct ice_aqc_get_ver {
	__le32 rom_ver;
	__le32 fw_build;
	u8 fw_branch;
	u8 fw_major;
	u8 fw_minor;
	u8 fw_patch;
	u8 api_branch;
	u8 api_major;
	u8 api_minor;
	u8 api_patch;
};

/* Send driver version (indirect 0x0002) */
struct ice_aqc_driver_ver {
	u8 major_ver;
	u8 minor_ver;
	u8 build_ver;
	u8 subbuild_ver;
	u8 reserved[4];
	__le32 addr_high;
	__le32 addr_low;
};

/* Queue Shutdown (direct 0x0003) */
struct ice_aqc_q_shutdown {
	u8 driver_unloading;
#define ICE_AQC_DRIVER_UNLOADING	BIT(0)
	u8 reserved[15];
};

/* Get Expanded Error Code (0x0005, direct) */
struct ice_aqc_get_exp_err {
	__le32 reason;
#define ICE_AQC_EXPANDED_ERROR_NOT_PROVIDED	0xFFFFFFFF
	__le32 identifier;
	u8 rsvd[8];
};

/* Request resource ownership (direct 0x0008)
 * Release resource ownership (direct 0x0009)
 */
struct ice_aqc_req_res {
	__le16 res_id;
#define ICE_AQC_RES_ID_NVM		1
#define ICE_AQC_RES_ID_SDP		2
#define ICE_AQC_RES_ID_CHNG_LOCK	3
#define ICE_AQC_RES_ID_GLBL_LOCK	4
	__le16 access_type;
#define ICE_AQC_RES_ACCESS_READ		1
#define ICE_AQC_RES_ACCESS_WRITE	2

	/* Upon successful completion, FW writes this value and driver is
	 * expected to release resource before timeout. This value is provided
	 * in milliseconds.
	 */
	__le32 timeout;
#define ICE_AQ_RES_NVM_READ_DFLT_TIMEOUT_MS	3000
#define ICE_AQ_RES_NVM_WRITE_DFLT_TIMEOUT_MS	180000
#define ICE_AQ_RES_CHNG_LOCK_DFLT_TIMEOUT_MS	1000
#define ICE_AQ_RES_GLBL_LOCK_DFLT_TIMEOUT_MS	3000
	/* For SDP: pin ID of the SDP */
	__le32 res_number;
	/* Status is only used for ICE_AQC_RES_ID_GLBL_LOCK */
	__le16 status;
#define ICE_AQ_RES_GLBL_SUCCESS		0
#define ICE_AQ_RES_GLBL_IN_PROG		1
#define ICE_AQ_RES_GLBL_DONE		2
	u8 reserved[2];
};

/* Get function capabilities (indirect 0x000A)
 * Get device capabilities (indirect 0x000B)
 */
struct ice_aqc_list_caps {
	u8 cmd_flags;
	u8 pf_index;
	u8 reserved[2];
	__le32 count;
	__le32 addr_high;
	__le32 addr_low;
};

/* Device/Function buffer entry, repeated per reported capability */
struct ice_aqc_list_caps_elem {
	__le16 cap;
#define ICE_AQC_CAPS_SWITCHING_MODE			0x0001
#define ICE_AQC_CAPS_MANAGEABILITY_MODE			0x0002
#define ICE_AQC_CAPS_OS2BMC				0x0004
#define ICE_AQC_CAPS_VALID_FUNCTIONS			0x0005
#define ICE_AQC_MAX_VALID_FUNCTIONS			0x8
#define ICE_AQC_CAPS_ALTERNATE_RAM			0x0006
#define ICE_AQC_CAPS_WOL_PROXY				0x0008
#define ICE_AQC_CAPS_SRIOV				0x0012
#define ICE_AQC_CAPS_VF					0x0013
#define ICE_AQC_CAPS_VMDQ				0x0014
#define ICE_AQC_CAPS_802_1QBG				0x0015
#define ICE_AQC_CAPS_802_1BR				0x0016
#define ICE_AQC_CAPS_VSI				0x0017
#define ICE_AQC_CAPS_DCB				0x0018
#define ICE_AQC_CAPS_RSVD				0x0021
#define ICE_AQC_CAPS_ISCSI				0x0022
#define ICE_AQC_CAPS_RSS				0x0040
#define ICE_AQC_CAPS_RXQS				0x0041
#define ICE_AQC_CAPS_TXQS				0x0042
#define ICE_AQC_CAPS_MSIX				0x0043
#define ICE_AQC_CAPS_MAX_MTU				0x0047
#define ICE_AQC_CAPS_CEM				0x00F2
#define ICE_AQC_CAPS_IWARP				0x0051
#define ICE_AQC_CAPS_LED				0x0061
#define ICE_AQC_CAPS_SDP				0x0062
#define ICE_AQC_CAPS_WR_CSR_PROT			0x0064
#define ICE_AQC_CAPS_SENSOR_READING			0x0067
#define ICE_AQC_CAPS_LOGI_TO_PHYSI_PORT_MAP		0x0073
#define ICE_AQC_CAPS_SKU				0x0074
#define ICE_AQC_CAPS_PORT_MAP				0x0075
#define ICE_AQC_CAPS_PCIE_RESET_AVOIDANCE		0x0076
#define ICE_AQC_CAPS_POST_UPDATE_RESET_RESTRICT		0x0077
#define ICE_AQC_CAPS_NVM_MGMT				0x0080
#define ICE_AQC_CAPS_EXT_TOPO_DEV_IMG0			0x0081
#define ICE_AQC_CAPS_EXT_TOPO_DEV_IMG1			0x0082
#define ICE_AQC_CAPS_EXT_TOPO_DEV_IMG2			0x0083
#define ICE_AQC_CAPS_EXT_TOPO_DEV_IMG3			0x0084
#define ICE_AQC_CAPS_TX_SCHED_TOPO_COMP_MODE		0x0085
#define ICE_AQC_CAPS_NAC_TOPOLOGY			0x0087
#define ICE_AQC_CAPS_DYN_FLATTENING			0x008A
#define ICE_AQC_CAPS_OROM_RECOVERY_UPDATE		0x0090
#define ICE_AQC_CAPS_ROCEV2_LAG				0x0092
#define ICE_AQC_BIT_ROCEV2_LAG				0x01
#define ICE_AQC_BIT_SRIOV_LAG				0x02
#define ICE_AQC_CAPS_NEXT_CLUSTER_ID			0x0096
	u8 major_ver;
	u8 minor_ver;
	/* Number of resources described by this capability */
	__le32 number;
	/* Only meaningful for some types of resources */
	__le32 logical_id;
	/* Only meaningful for some types of resources */
	__le32 phys_id;
	__le64 rsvd1;
	__le64 rsvd2;
};

/* Manage MAC address, read command - indirect (0x0107)
 * This struct is also used for the response
 */
struct ice_aqc_manage_mac_read {
	__le16 flags; /* Zeroed by device driver */
#define ICE_AQC_MAN_MAC_LAN_ADDR_VALID		BIT(4)
#define ICE_AQC_MAN_MAC_SAN_ADDR_VALID		BIT(5)
#define ICE_AQC_MAN_MAC_PORT_ADDR_VALID		BIT(6)
#define ICE_AQC_MAN_MAC_WOL_ADDR_VALID		BIT(7)
#define ICE_AQC_MAN_MAC_MC_MAG_EN		BIT(8)
#define ICE_AQC_MAN_MAC_WOL_PRESERVE_ON_PFR	BIT(9)
#define ICE_AQC_MAN_MAC_READ_S			4
#define ICE_AQC_MAN_MAC_READ_M			(0xF << ICE_AQC_MAN_MAC_READ_S)
	u8 rsvd[2];
	u8 num_addr; /* Used in response */
	u8 rsvd1[3];
	__le32 addr_high;
	__le32 addr_low;
};

/* Response buffer format for manage MAC read command */
struct ice_aqc_manage_mac_read_resp {
	u8 lport_num;
	u8 addr_type;
#define ICE_AQC_MAN_MAC_ADDR_TYPE_LAN		0
#define ICE_AQC_MAN_MAC_ADDR_TYPE_WOL		1
	u8 mac_addr[ETH_ALEN];
};

/* Manage MAC address, write command - direct (0x0108) */
struct ice_aqc_manage_mac_write {
	u8 rsvd;
	u8 flags;
#define ICE_AQC_MAN_MAC_WR_MC_MAG_EN		BIT(0)
#define ICE_AQC_MAN_MAC_WR_WOL_LAA_PFR_KEEP	BIT(1)
#define ICE_AQC_MAN_MAC_WR_S		6
#define ICE_AQC_MAN_MAC_WR_M		MAKEMASK(3, ICE_AQC_MAN_MAC_WR_S)
#define ICE_AQC_MAN_MAC_UPDATE_LAA	0
#define ICE_AQC_MAN_MAC_UPDATE_LAA_WOL	BIT(ICE_AQC_MAN_MAC_WR_S)
	/* byte stream in network order */
	u8 mac_addr[ETH_ALEN];
	__le32 addr_high;
	__le32 addr_low;
};

/* Clear PXE Command and response (direct 0x0110) */
struct ice_aqc_clear_pxe {
	u8 rx_cnt;
#define ICE_AQC_CLEAR_PXE_RX_CNT		0x2
	u8 reserved[15];
};

/* Configure No-Drop Policy Command (direct 0x0112) */
struct ice_aqc_config_no_drop_policy {
	u8 opts;
#define ICE_AQC_FORCE_NO_DROP			BIT(0)
	u8 rsvd[15];
};

/* Get switch configuration (0x0200) */
struct ice_aqc_get_sw_cfg {
	/* Reserved for command and copy of request flags for response */
	__le16 flags;
	/* First desc in case of command and next_elem in case of response
	 * In case of response, if it is not zero, means all the configuration
	 * was not returned and new command shall be sent with this value in
	 * the 'first desc' field
	 */
	__le16 element;
	/* Reserved for command, only used for response */
	__le16 num_elems;
	__le16 rsvd;
	__le32 addr_high;
	__le32 addr_low;
};

/* Each entry in the response buffer is of the following type: */
struct ice_aqc_get_sw_cfg_resp_elem {
	/* VSI/Port Number */
	__le16 vsi_port_num;
#define ICE_AQC_GET_SW_CONF_RESP_VSI_PORT_NUM_S	0
#define ICE_AQC_GET_SW_CONF_RESP_VSI_PORT_NUM_M	\
			(0x3FF << ICE_AQC_GET_SW_CONF_RESP_VSI_PORT_NUM_S)
#define ICE_AQC_GET_SW_CONF_RESP_TYPE_S	14
#define ICE_AQC_GET_SW_CONF_RESP_TYPE_M	(0x3 << ICE_AQC_GET_SW_CONF_RESP_TYPE_S)
#define ICE_AQC_GET_SW_CONF_RESP_PHYS_PORT	0
#define ICE_AQC_GET_SW_CONF_RESP_VIRT_PORT	1
#define ICE_AQC_GET_SW_CONF_RESP_VSI		2

	/* SWID VSI/Port belongs to */
	__le16 swid;

	/* Bit 14..0 : PF/VF number VSI belongs to
	 * Bit 15 : VF indication bit
	 */
	__le16 pf_vf_num;
#define ICE_AQC_GET_SW_CONF_RESP_FUNC_NUM_S	0
#define ICE_AQC_GET_SW_CONF_RESP_FUNC_NUM_M	\
				(0x7FFF << ICE_AQC_GET_SW_CONF_RESP_FUNC_NUM_S)
#define ICE_AQC_GET_SW_CONF_RESP_IS_VF		BIT(15)
};

/* Set Port parameters, (direct, 0x0203) */
struct ice_aqc_set_port_params {
	__le16 cmd_flags;
#define ICE_AQC_SET_P_PARAMS_SAVE_BAD_PACKETS	BIT(0)
#define ICE_AQC_SET_P_PARAMS_PAD_SHORT_PACKETS	BIT(1)
#define ICE_AQC_SET_P_PARAMS_DOUBLE_VLAN_ENA	BIT(2)
	__le16 bad_frame_vsi;
#define ICE_AQC_SET_P_PARAMS_VSI_S	0
#define ICE_AQC_SET_P_PARAMS_VSI_M	(0x3FF << ICE_AQC_SET_P_PARAMS_VSI_S)
#define ICE_AQC_SET_P_PARAMS_VSI_VALID	BIT(15)
	__le16 swid;
#define ICE_AQC_SET_P_PARAMS_SWID_S	0
#define ICE_AQC_SET_P_PARAMS_SWID_M	(0xFF << ICE_AQC_SET_P_PARAMS_SWID_S)
#define ICE_AQC_SET_P_PARAMS_LOGI_PORT_ID_S	8
#define ICE_AQC_SET_P_PARAMS_LOGI_PORT_ID_M	\
				(0x3F << ICE_AQC_SET_P_PARAMS_LOGI_PORT_ID_S)
#define ICE_AQC_SET_P_PARAMS_IS_LOGI_PORT	BIT(14)
#define ICE_AQC_SET_P_PARAMS_SWID_VALID		BIT(15)
	u8 lb_mode;
#define ICE_AQC_SET_P_PARAMS_LOOPBACK_MODE_VALID BIT(2)
#define ICE_AQC_SET_P_PARAMS_LOOPBACK_MODE_NORMAL 0x00
#define ICE_AQC_SET_P_PARAMS_LOOPBACK_MODE_NO 0x01
#define ICE_AQC_SET_P_PARAMS_LOOPBACK_MODE_HIGH 0x02
	u8 reserved[9];
};

/* These resource type defines are used for all switch resource
 * commands where a resource type is required, such as:
 * Get Resource Allocation command (indirect 0x0204)
 * Allocate Resources command (indirect 0x0208)
 * Free Resources command (indirect 0x0209)
 * Get Allocated Resource Descriptors Command (indirect 0x020A)
 */
#define ICE_AQC_RES_TYPE_VEB_COUNTER			0x00
#define ICE_AQC_RES_TYPE_VLAN_COUNTER			0x01
#define ICE_AQC_RES_TYPE_MIRROR_RULE			0x02
#define ICE_AQC_RES_TYPE_VSI_LIST_REP			0x03
#define ICE_AQC_RES_TYPE_VSI_LIST_PRUNE			0x04
#define ICE_AQC_RES_TYPE_RECIPE				0x05
#define ICE_AQC_RES_TYPE_PROFILE			0x06
#define ICE_AQC_RES_TYPE_SWID				0x07
#define ICE_AQC_RES_TYPE_VSI				0x08
#define ICE_AQC_RES_TYPE_FLU				0x09
#define ICE_AQC_RES_TYPE_WIDE_TABLE_1			0x0A
#define ICE_AQC_RES_TYPE_WIDE_TABLE_2			0x0B
#define ICE_AQC_RES_TYPE_WIDE_TABLE_4			0x0C
#define ICE_AQC_RES_TYPE_GLOBAL_RSS_HASH		0x20
#define ICE_AQC_RES_TYPE_FDIR_COUNTER_BLOCK		0x21
#define ICE_AQC_RES_TYPE_FDIR_GUARANTEED_ENTRIES	0x22
#define ICE_AQC_RES_TYPE_FDIR_SHARED_ENTRIES		0x23
#define ICE_AQC_RES_TYPE_FLEX_DESC_PROG			0x30
#define ICE_AQC_RES_TYPE_SWITCH_PROF_BLDR_PROFID	0x48
#define ICE_AQC_RES_TYPE_SWITCH_PROF_BLDR_TCAM		0x49
#define ICE_AQC_RES_TYPE_ACL_PROF_BLDR_PROFID		0x50
#define ICE_AQC_RES_TYPE_ACL_PROF_BLDR_TCAM		0x51
#define ICE_AQC_RES_TYPE_HASH_PROF_BLDR_PROFID		0x60
#define ICE_AQC_RES_TYPE_HASH_PROF_BLDR_TCAM		0x61
/* Resource types 0x62-67 are reserved for Hash profile builder */
#define ICE_AQC_RES_TYPE_QHASH_PROF_BLDR_PROFID		0x68
#define ICE_AQC_RES_TYPE_QHASH_PROF_BLDR_TCAM		0x69

#define ICE_AQC_RES_TYPE_FLAG_SHARED			BIT(7)
#define ICE_AQC_RES_TYPE_FLAG_SCAN_BOTTOM		BIT(12)
#define ICE_AQC_RES_TYPE_FLAG_IGNORE_INDEX		BIT(13)
#define ICE_AQC_RES_TYPE_FLAG_SUBSCRIBE_SHARED		BIT(14)
#define ICE_AQC_RES_TYPE_FLAG_SUBSCRIBE_CTL		BIT(15)

#define ICE_AQC_RES_TYPE_FLAG_DEDICATED			0x00

#define ICE_AQC_RES_TYPE_S	0
#define ICE_AQC_RES_TYPE_M	(0x07F << ICE_AQC_RES_TYPE_S)

/* Get Resource Allocation command (indirect 0x0204) */
struct ice_aqc_get_res_alloc {
	__le16 resp_elem_num; /* Used in response, reserved in command */
	u8 reserved[6];
	__le32 addr_high;
	__le32 addr_low;
};

/* Get Resource Allocation Response Buffer per response */
struct ice_aqc_get_res_resp_elem {
	__le16 res_type; /* Types defined above cmd 0x0204 */
	__le16 total_capacity; /* Resources available to all PF's */
	__le16 total_function; /* Resources allocated for a PF */
	__le16 total_shared; /* Resources allocated as shared */
	__le16 total_free; /* Resources un-allocated/not reserved by any PF */
};

/* Allocate Resources command (indirect 0x0208)
 * Free Resources command (indirect 0x0209)
 */
struct ice_aqc_alloc_free_res_cmd {
	__le16 num_entries; /* Number of Resource entries */
	u8 reserved[6];
	__le32 addr_high;
	__le32 addr_low;
};

/* Resource descriptor */
struct ice_aqc_res_elem {
	union {
		__le16 sw_resp;
		__le16 flu_resp;
	} e;
};

/* Buffer for Allocate/Free Resources commands */
struct ice_aqc_alloc_free_res_elem {
	__le16 res_type; /* Types defined above cmd 0x0204 */
#define ICE_AQC_RES_TYPE_VSI_PRUNE_LIST_S	8
#define ICE_AQC_RES_TYPE_VSI_PRUNE_LIST_M	\
				(0xF << ICE_AQC_RES_TYPE_VSI_PRUNE_LIST_S)
	__le16 num_elems;
	struct ice_aqc_res_elem elem[STRUCT_HACK_VAR_LEN];
};

/* Get Allocated Resource Descriptors Command (indirect 0x020A) */
struct ice_aqc_get_allocd_res_desc {
	union {
		struct {
			__le16 res; /* Types defined above cmd 0x0204 */
			__le16 first_desc;
			__le32 reserved;
		} cmd;
		struct {
			__le16 res;
			__le16 next_desc;
			__le16 num_desc;
			__le16 reserved;
		} resp;
	} ops;
	__le32 addr_high;
	__le32 addr_low;
};

/* Request buffer for Set VLAN Mode AQ command (indirect 0x020C) */
struct ice_aqc_set_vlan_mode {
	u8 reserved;
	u8 l2tag_prio_tagging;
#define ICE_AQ_VLAN_PRIO_TAG_S			0
#define ICE_AQ_VLAN_PRIO_TAG_M			(0x7 << ICE_AQ_VLAN_PRIO_TAG_S)
#define ICE_AQ_VLAN_PRIO_TAG_NOT_SUPPORTED	0x0
#define ICE_AQ_VLAN_PRIO_TAG_STAG		0x1
#define ICE_AQ_VLAN_PRIO_TAG_OUTER_CTAG		0x2
#define ICE_AQ_VLAN_PRIO_TAG_OUTER_VLAN		0x3
#define ICE_AQ_VLAN_PRIO_TAG_INNER_CTAG		0x4
#define ICE_AQ_VLAN_PRIO_TAG_MAX		0x4
#define ICE_AQ_VLAN_PRIO_TAG_ERROR		0x7
	u8 l2tag_reserved[64];
	u8 rdma_packet;
#define ICE_AQ_VLAN_RDMA_TAG_S			0
#define ICE_AQ_VLAN_RDMA_TAG_M			(0x3F << ICE_AQ_VLAN_RDMA_TAG_S)
#define ICE_AQ_SVM_VLAN_RDMA_PKT_FLAG_SETTING	0x10
#define ICE_AQ_DVM_VLAN_RDMA_PKT_FLAG_SETTING	0x1A
	u8 rdma_reserved[2];
	u8 mng_vlan_prot_id;
#define ICE_AQ_VLAN_MNG_PROTOCOL_ID_OUTER	0x10
#define ICE_AQ_VLAN_MNG_PROTOCOL_ID_INNER	0x11
	u8 prot_id_reserved[30];
};

/* Response buffer for Get VLAN Mode AQ command (indirect 0x020D) */
struct ice_aqc_get_vlan_mode {
	u8 vlan_mode;
#define ICE_AQ_VLAN_MODE_DVM_ENA	BIT(0)
	u8 l2tag_prio_tagging;
	u8 reserved[98];
};

/* Add VSI (indirect 0x0210)
 * Update VSI (indirect 0x0211)
 * Get VSI (indirect 0x0212)
 * Free VSI (indirect 0x0213)
 */
struct ice_aqc_add_get_update_free_vsi {
	__le16 vsi_num;
#define ICE_AQ_VSI_NUM_S	0
#define ICE_AQ_VSI_NUM_M	(0x03FF << ICE_AQ_VSI_NUM_S)
#define ICE_AQ_VSI_IS_VALID	BIT(15)
	__le16 cmd_flags;
#define ICE_AQ_VSI_KEEP_ALLOC	0x1
	u8 vf_id;
	u8 reserved;
	__le16 vsi_flags;
#define ICE_AQ_VSI_TYPE_S	0
#define ICE_AQ_VSI_TYPE_M	(0x3 << ICE_AQ_VSI_TYPE_S)
#define ICE_AQ_VSI_TYPE_VF	0x0
#define ICE_AQ_VSI_TYPE_VMDQ2	0x1
#define ICE_AQ_VSI_TYPE_PF	0x2
#define ICE_AQ_VSI_TYPE_EMP_MNG	0x3
	__le32 addr_high;
	__le32 addr_low;
};

/* Response descriptor for:
 * Add VSI (indirect 0x0210)
 * Update VSI (indirect 0x0211)
 * Free VSI (indirect 0x0213)
 */
struct ice_aqc_add_update_free_vsi_resp {
	__le16 vsi_num;
	__le16 ext_status;
	__le16 vsi_used;
	__le16 vsi_free;
	__le32 addr_high;
	__le32 addr_low;
};

struct ice_aqc_get_vsi_resp {
	__le16 vsi_num;
	u8 vf_id;
	/* The vsi_flags field uses the ICE_AQ_VSI_TYPE_* defines for values.
	 * These are found above in struct ice_aqc_add_get_update_free_vsi.
	 */
	u8 vsi_flags;
	__le16 vsi_used;
	__le16 vsi_free;
	__le32 addr_high;
	__le32 addr_low;
};

struct ice_aqc_vsi_props {
	__le16 valid_sections;
#define ICE_AQ_VSI_PROP_SW_VALID		BIT(0)
#define ICE_AQ_VSI_PROP_SECURITY_VALID		BIT(1)
#define ICE_AQ_VSI_PROP_VLAN_VALID		BIT(2)
#define ICE_AQ_VSI_PROP_OUTER_TAG_VALID		BIT(3)
#define ICE_AQ_VSI_PROP_INGRESS_UP_VALID	BIT(4)
#define ICE_AQ_VSI_PROP_EGRESS_UP_VALID		BIT(5)
#define ICE_AQ_VSI_PROP_RXQ_MAP_VALID		BIT(6)
#define ICE_AQ_VSI_PROP_Q_OPT_VALID		BIT(7)
#define ICE_AQ_VSI_PROP_OUTER_UP_VALID		BIT(8)
#define ICE_AQ_VSI_PROP_FLOW_DIR_VALID		BIT(11)
#define ICE_AQ_VSI_PROP_PASID_VALID		BIT(12)
	/* switch section */
	u8 sw_id;
	u8 sw_flags;
#define ICE_AQ_VSI_SW_FLAG_ALLOW_LB		BIT(5)
#define ICE_AQ_VSI_SW_FLAG_LOCAL_LB		BIT(6)
#define ICE_AQ_VSI_SW_FLAG_SRC_PRUNE		BIT(7)
	u8 sw_flags2;
#define ICE_AQ_VSI_SW_FLAG_RX_PRUNE_EN_S	0
#define ICE_AQ_VSI_SW_FLAG_RX_PRUNE_EN_M	(0xF << ICE_AQ_VSI_SW_FLAG_RX_PRUNE_EN_S)
#define ICE_AQ_VSI_SW_FLAG_RX_VLAN_PRUNE_ENA	BIT(0)
#define ICE_AQ_VSI_SW_FLAG_RX_PASS_PRUNE_ENA	BIT(3)
#define ICE_AQ_VSI_SW_FLAG_LAN_ENA		BIT(4)
	u8 veb_stat_id;
#define ICE_AQ_VSI_SW_VEB_STAT_ID_S		0
#define ICE_AQ_VSI_SW_VEB_STAT_ID_M		(0x1F << ICE_AQ_VSI_SW_VEB_STAT_ID_S)
#define ICE_AQ_VSI_SW_VEB_STAT_ID_VALID		BIT(5)
	/* security section */
	u8 sec_flags;
#define ICE_AQ_VSI_SEC_FLAG_ALLOW_DEST_OVRD	BIT(0)
#define ICE_AQ_VSI_SEC_FLAG_ENA_MAC_ANTI_SPOOF	BIT(2)
#define ICE_AQ_VSI_SEC_TX_PRUNE_ENA_S		4
#define ICE_AQ_VSI_SEC_TX_PRUNE_ENA_M		(0xF << ICE_AQ_VSI_SEC_TX_PRUNE_ENA_S)
#define ICE_AQ_VSI_SEC_TX_VLAN_PRUNE_ENA	BIT(0)
	u8 sec_reserved;
	/* VLAN section */
	__le16 port_based_inner_vlan; /* VLANS include priority bits */
	u8 inner_vlan_reserved[2];
	u8 inner_vlan_flags;
#define ICE_AQ_VSI_INNER_VLAN_TX_MODE_S		0
#define ICE_AQ_VSI_INNER_VLAN_TX_MODE_M		(0x3 << ICE_AQ_VSI_INNER_VLAN_TX_MODE_S)
#define ICE_AQ_VSI_INNER_VLAN_TX_MODE_ACCEPTUNTAGGED	0x1
#define ICE_AQ_VSI_INNER_VLAN_TX_MODE_ACCEPTTAGGED	0x2
#define ICE_AQ_VSI_INNER_VLAN_TX_MODE_ALL	0x3
#define ICE_AQ_VSI_INNER_VLAN_INSERT_PVID	BIT(2)
#define ICE_AQ_VSI_INNER_VLAN_EMODE_S		3
#define ICE_AQ_VSI_INNER_VLAN_EMODE_M		(0x3 << ICE_AQ_VSI_INNER_VLAN_EMODE_S)
#define ICE_AQ_VSI_INNER_VLAN_EMODE_STR_BOTH	(0x0 << ICE_AQ_VSI_INNER_VLAN_EMODE_S)
#define ICE_AQ_VSI_INNER_VLAN_EMODE_STR_UP	(0x1 << ICE_AQ_VSI_INNER_VLAN_EMODE_S)
#define ICE_AQ_VSI_INNER_VLAN_EMODE_STR		(0x2 << ICE_AQ_VSI_INNER_VLAN_EMODE_S)
#define ICE_AQ_VSI_INNER_VLAN_EMODE_NOTHING	(0x3 << ICE_AQ_VSI_INNER_VLAN_EMODE_S)
#define ICE_AQ_VSI_INNER_VLAN_BLOCK_TX_DESC	BIT(5)
	u8 inner_vlan_reserved2[3];
	/* ingress egress up sections */
	__le32 ingress_table; /* bitmap, 3 bits per up */
#define ICE_AQ_VSI_UP_TABLE_UP0_S		0
#define ICE_AQ_VSI_UP_TABLE_UP0_M		(0x7 << ICE_AQ_VSI_UP_TABLE_UP0_S)
#define ICE_AQ_VSI_UP_TABLE_UP1_S		3
#define ICE_AQ_VSI_UP_TABLE_UP1_M		(0x7 << ICE_AQ_VSI_UP_TABLE_UP1_S)
#define ICE_AQ_VSI_UP_TABLE_UP2_S		6
#define ICE_AQ_VSI_UP_TABLE_UP2_M		(0x7 << ICE_AQ_VSI_UP_TABLE_UP2_S)
#define ICE_AQ_VSI_UP_TABLE_UP3_S		9
#define ICE_AQ_VSI_UP_TABLE_UP3_M		(0x7 << ICE_AQ_VSI_UP_TABLE_UP3_S)
#define ICE_AQ_VSI_UP_TABLE_UP4_S		12
#define ICE_AQ_VSI_UP_TABLE_UP4_M		(0x7 << ICE_AQ_VSI_UP_TABLE_UP4_S)
#define ICE_AQ_VSI_UP_TABLE_UP5_S		15
#define ICE_AQ_VSI_UP_TABLE_UP5_M		(0x7 << ICE_AQ_VSI_UP_TABLE_UP5_S)
#define ICE_AQ_VSI_UP_TABLE_UP6_S		18
#define ICE_AQ_VSI_UP_TABLE_UP6_M		(0x7 << ICE_AQ_VSI_UP_TABLE_UP6_S)
#define ICE_AQ_VSI_UP_TABLE_UP7_S		21
#define ICE_AQ_VSI_UP_TABLE_UP7_M		(0x7 << ICE_AQ_VSI_UP_TABLE_UP7_S)
	__le32 egress_table;   /* same defines as for ingress table */
	/* outer tags section */
	__le16 port_based_outer_vlan;
	u8 outer_vlan_flags;
#define ICE_AQ_VSI_OUTER_VLAN_EMODE_S		0
#define ICE_AQ_VSI_OUTER_VLAN_EMODE_M		(0x3 << ICE_AQ_VSI_OUTER_VLAN_EMODE_S)
#define ICE_AQ_VSI_OUTER_VLAN_EMODE_SHOW_BOTH	0x0
#define ICE_AQ_VSI_OUTER_VLAN_EMODE_SHOW_UP	0x1
#define ICE_AQ_VSI_OUTER_VLAN_EMODE_SHOW	0x2
#define ICE_AQ_VSI_OUTER_VLAN_EMODE_NOTHING	0x3
#define ICE_AQ_VSI_OUTER_TAG_TYPE_S		2
#define ICE_AQ_VSI_OUTER_TAG_TYPE_M		(0x3 << ICE_AQ_VSI_OUTER_TAG_TYPE_S)
#define ICE_AQ_VSI_OUTER_TAG_NONE		0x0
#define ICE_AQ_VSI_OUTER_TAG_STAG		0x1
#define ICE_AQ_VSI_OUTER_TAG_VLAN_8100		0x2
#define ICE_AQ_VSI_OUTER_TAG_VLAN_9100		0x3
#define ICE_AQ_VSI_OUTER_VLAN_PORT_BASED_INSERT		BIT(4)
#define ICE_AQ_VSI_OUTER_VLAN_TX_MODE_S			5
#define ICE_AQ_VSI_OUTER_VLAN_TX_MODE_M			(0x3 << ICE_AQ_VSI_OUTER_VLAN_TX_MODE_S)
#define ICE_AQ_VSI_OUTER_VLAN_TX_MODE_ACCEPTUNTAGGED	0x1
#define ICE_AQ_VSI_OUTER_VLAN_TX_MODE_ACCEPTTAGGED	0x2
#define ICE_AQ_VSI_OUTER_VLAN_TX_MODE_ALL		0x3
#define ICE_AQ_VSI_OUTER_VLAN_BLOCK_TX_DESC		BIT(7)
	u8 outer_vlan_reserved;
	/* queue mapping section */
	__le16 mapping_flags;
#define ICE_AQ_VSI_Q_MAP_CONTIG			0x0
#define ICE_AQ_VSI_Q_MAP_NONCONTIG		BIT(0)
	__le16 q_mapping[16];
#define ICE_AQ_VSI_Q_S				0
#define ICE_AQ_VSI_Q_M				(0x7FF << ICE_AQ_VSI_Q_S)
	__le16 tc_mapping[8];
#define ICE_AQ_VSI_TC_Q_OFFSET_S		0
#define ICE_AQ_VSI_TC_Q_OFFSET_M		(0x7FF << ICE_AQ_VSI_TC_Q_OFFSET_S)
#define ICE_AQ_VSI_TC_Q_NUM_S			11
#define ICE_AQ_VSI_TC_Q_NUM_M			(0xF << ICE_AQ_VSI_TC_Q_NUM_S)
	/* queueing option section */
	u8 q_opt_rss;
#define ICE_AQ_VSI_Q_OPT_RSS_LUT_S		0
#define ICE_AQ_VSI_Q_OPT_RSS_LUT_M		(0x3 << ICE_AQ_VSI_Q_OPT_RSS_LUT_S)
#define ICE_AQ_VSI_Q_OPT_RSS_LUT_VSI		0x0
#define ICE_AQ_VSI_Q_OPT_RSS_LUT_PF		0x2
#define ICE_AQ_VSI_Q_OPT_RSS_LUT_GBL		0x3
#define ICE_AQ_VSI_Q_OPT_RSS_GBL_LUT_S		2
#define ICE_AQ_VSI_Q_OPT_RSS_GBL_LUT_M		(0xF << ICE_AQ_VSI_Q_OPT_RSS_GBL_LUT_S)
#define ICE_AQ_VSI_Q_OPT_RSS_HASH_S		6
#define ICE_AQ_VSI_Q_OPT_RSS_HASH_M		(0x3 << ICE_AQ_VSI_Q_OPT_RSS_HASH_S)
#define ICE_AQ_VSI_Q_OPT_RSS_TPLZ		(0x0 << ICE_AQ_VSI_Q_OPT_RSS_HASH_S)
#define ICE_AQ_VSI_Q_OPT_RSS_SYM_TPLZ		(0x1 << ICE_AQ_VSI_Q_OPT_RSS_HASH_S)
#define ICE_AQ_VSI_Q_OPT_RSS_XOR		(0x2 << ICE_AQ_VSI_Q_OPT_RSS_HASH_S)
#define ICE_AQ_VSI_Q_OPT_RSS_JHASH		(0x3 << ICE_AQ_VSI_Q_OPT_RSS_HASH_S)
	u8 q_opt_tc;
#define ICE_AQ_VSI_Q_OPT_TC_OVR_S		0
#define ICE_AQ_VSI_Q_OPT_TC_OVR_M		(0x1F << ICE_AQ_VSI_Q_OPT_TC_OVR_S)
#define ICE_AQ_VSI_Q_OPT_PROF_TC_OVR		BIT(7)
	u8 q_opt_flags;
#define ICE_AQ_VSI_Q_OPT_PE_FLTR_EN		BIT(0)
	u8 q_opt_reserved[3];
	/* outer up section */
	__le32 outer_up_table; /* same structure and defines as ingress tbl */
	/* section 10 */
	__le16 sect_10_reserved;
	/* flow director section */
	__le16 fd_options;
#define ICE_AQ_VSI_FD_ENABLE			BIT(0)
#define ICE_AQ_VSI_FD_TX_AUTO_ENABLE		BIT(1)
#define ICE_AQ_VSI_FD_PROG_ENABLE		BIT(3)
	__le16 max_fd_fltr_dedicated;
	__le16 max_fd_fltr_shared;
	__le16 fd_def_q;
#define ICE_AQ_VSI_FD_DEF_Q_S			0
#define ICE_AQ_VSI_FD_DEF_Q_M			(0x7FF << ICE_AQ_VSI_FD_DEF_Q_S)
#define ICE_AQ_VSI_FD_DEF_GRP_S			12
#define ICE_AQ_VSI_FD_DEF_GRP_M			(0x7 << ICE_AQ_VSI_FD_DEF_GRP_S)
	__le16 fd_report_opt;
#define ICE_AQ_VSI_FD_REPORT_Q_S		0
#define ICE_AQ_VSI_FD_REPORT_Q_M		(0x7FF << ICE_AQ_VSI_FD_REPORT_Q_S)
#define ICE_AQ_VSI_FD_DEF_PRIORITY_S		12
#define ICE_AQ_VSI_FD_DEF_PRIORITY_M		(0x7 << ICE_AQ_VSI_FD_DEF_PRIORITY_S)
#define ICE_AQ_VSI_FD_DEF_DROP			BIT(15)
	/* PASID section */
	__le32 pasid_id;
#define ICE_AQ_VSI_PASID_ID_S			0
#define ICE_AQ_VSI_PASID_ID_M			(0xFFFFF << ICE_AQ_VSI_PASID_ID_S)
#define ICE_AQ_VSI_PASID_ID_VALID		BIT(31)
	u8 reserved[24];
};

/* Add/update mirror rule - direct (0x0260) */
#define ICE_AQC_RULE_ID_VALID_S		7
#define ICE_AQC_RULE_ID_VALID_M		(0x1 << ICE_AQC_RULE_ID_VALID_S)
#define ICE_AQC_RULE_ID_S		0
#define ICE_AQC_RULE_ID_M		(0x3F << ICE_AQC_RULE_ID_S)

/* Following defines to be used while processing caller specified mirror list
 * of VSI indexes.
 */
/* Action: Byte.bit (1.7)
 *	0 = Remove VSI from mirror rule
 *	1 = Add VSI to mirror rule
 */
#define ICE_AQC_RULE_ACT_S	15
#define ICE_AQC_RULE_ACT_M	(0x1 << ICE_AQC_RULE_ACT_S)
/* Action: 1.2:0.0 = Mirrored VSI */
#define ICE_AQC_RULE_MIRRORED_VSI_S	0
#define ICE_AQC_RULE_MIRRORED_VSI_M	(0x7FF << ICE_AQC_RULE_MIRRORED_VSI_S)

/* This is to be used by add/update mirror rule Admin Queue command.
 * In case of add mirror rule - if rule ID is specified as
 * INVAL_MIRROR_RULE_ID, new rule ID is allocated from shared pool.
 * If specified rule_id is valid, then it is used. If specified rule_id
 * is in use then new mirroring rule is added.
 */
#define ICE_INVAL_MIRROR_RULE_ID	0xFFFF

struct ice_aqc_add_update_mir_rule {
	__le16 rule_id;

	__le16 rule_type;
#define ICE_AQC_RULE_TYPE_S		0
#define ICE_AQC_RULE_TYPE_M		(0x7 << ICE_AQC_RULE_TYPE_S)
	/* VPORT ingress/egress */
#define ICE_AQC_RULE_TYPE_VPORT_INGRESS	0x1
#define ICE_AQC_RULE_TYPE_VPORT_EGRESS	0x2
	/* Physical port ingress mirroring.
	 * All traffic received by this port
	 */
#define ICE_AQC_RULE_TYPE_PPORT_INGRESS	0x6
	/* Physical port egress mirroring. All traffic sent by this port */
#define ICE_AQC_RULE_TYPE_PPORT_EGRESS	0x7

	/* Number of mirrored entries.
	 * The values are in the command buffer
	 */
	__le16 num_entries;

	/* Destination VSI */
	__le16 dest;
	__le32 addr_high;
	__le32 addr_low;
};

/* Delete mirror rule - direct(0x0261) */
struct ice_aqc_delete_mir_rule {
	__le16 rule_id;
	__le16 rsvd;

	/* Byte.bit: 20.0 = Keep allocation. If set VSI stays part of
	 * the PF allocated resources, otherwise it is returned to the
	 * shared pool
	 */
#define ICE_AQC_FLAG_KEEP_ALLOCD_S	0
#define ICE_AQC_FLAG_KEEP_ALLOCD_M	(0x1 << ICE_AQC_FLAG_KEEP_ALLOCD_S)
	__le16 flags;

	u8 reserved[10];
};

/* Set/Get storm config - (direct 0x0280, 0x0281) */
/* This structure holds get storm configuration response and same structure
 * is used to perform set_storm_cfg
 */
struct ice_aqc_storm_cfg {
	__le32 bcast_thresh_size;
	__le32 mcast_thresh_size;
	/* Bit 18:0 - Traffic upper threshold size
	 * Bit 31:19 - Reserved
	 */
#define ICE_AQ_THRESHOLD_S	0
#define ICE_AQ_THRESHOLD_M	(0x7FFFF << ICE_AQ_THRESHOLD_S)

	__le32 storm_ctrl_ctrl;
	/* Bit 0: MDIPW - Drop Multicast packets in previous window
	 * Bit 1: MDICW - Drop multicast packets in current window
	 * Bit 2: BDIPW - Drop broadcast packets in previous window
	 * Bit 3: BDICW - Drop broadcast packets in current window
	 */
#define ICE_AQ_STORM_CTRL_MDIPW_DROP_MULTICAST	BIT(0)
#define ICE_AQ_STORM_CTRL_MDICW_DROP_MULTICAST	BIT(1)
#define ICE_AQ_STORM_CTRL_BDIPW_DROP_MULTICAST	BIT(2)
#define ICE_AQ_STORM_CTRL_BDICW_DROP_MULTICAST	BIT(3)
	/* Bit 7:5 : Reserved */
	/* Bit 27:8 : Interval - BSC/MSC Time-interval specification: The
	 * interval size for applying ingress broadcast or multicast storm
	 * control.
	 */
#define ICE_AQ_STORM_BSC_MSC_TIME_INTERVAL_S	8
#define ICE_AQ_STORM_BSC_MSC_TIME_INTERVAL_M	\
			(0xFFFFF << ICE_AQ_STORM_BSC_MSC_TIME_INTERVAL_S)
	__le32 reserved;
};

#define ICE_MAX_NUM_RECIPES 64

/* Add/Update/Remove/Get switch rules (indirect 0x02A0, 0x02A1, 0x02A2, 0x02A3)
 */
struct ice_aqc_sw_rules {
	/* ops: add switch rules, referring the number of rules.
	 * ops: update switch rules, referring the number of filters
	 * ops: remove switch rules, referring the entry index.
	 * ops: get switch rules, referring to the number of filters.
	 */
	__le16 num_rules_fltr_entry_index;
	u8 reserved[6];
	__le32 addr_high;
	__le32 addr_low;
};

/* Add switch rule response:
 * Content of return buffer is same as the input buffer. The status field and
 * LUT index are updated as part of the response
 */
struct ice_aqc_sw_rules_elem_hdr {
	__le16 type; /* Switch rule type, one of T_... */
#define ICE_AQC_SW_RULES_T_LKUP_RX		0x0
#define ICE_AQC_SW_RULES_T_LKUP_TX		0x1
#define ICE_AQC_SW_RULES_T_LG_ACT		0x2
#define ICE_AQC_SW_RULES_T_VSI_LIST_SET		0x3
#define ICE_AQC_SW_RULES_T_VSI_LIST_CLEAR	0x4
#define ICE_AQC_SW_RULES_T_PRUNE_LIST_SET	0x5
#define ICE_AQC_SW_RULES_T_PRUNE_LIST_CLEAR	0x6
	__le16 status;
};

/* Add/Update/Get/Remove lookup Rx/Tx command/response entry
 * This structures describes the lookup rules and associated actions. "index"
 * is returned as part of a response to a successful Add command, and can be
 * used to identify the rule for Update/Get/Remove commands.
 */
struct ice_sw_rule_lkup_rx_tx {
	struct ice_aqc_sw_rules_elem_hdr hdr;

	__le16 recipe_id;
#define ICE_SW_RECIPE_LOGICAL_PORT_FWD		10
	/* Source port for LOOKUP_RX and source VSI in case of LOOKUP_TX */
	__le16 src;
	__le32 act;

	/* Bit 0:1 - Action type */
#define ICE_SINGLE_ACT_TYPE_S	0x00
#define ICE_SINGLE_ACT_TYPE_M	(0x3 << ICE_SINGLE_ACT_TYPE_S)

	/* Bit 2 - Loop back enable
	 * Bit 3 - LAN enable
	 */
#define ICE_SINGLE_ACT_LB_ENABLE	BIT(2)
#define ICE_SINGLE_ACT_LAN_ENABLE	BIT(3)

	/* Action type = 0 - Forward to VSI or VSI list */
#define ICE_SINGLE_ACT_VSI_FORWARDING	0x0

#define ICE_SINGLE_ACT_VSI_ID_S		4
#define ICE_SINGLE_ACT_VSI_ID_M		(0x3FF << ICE_SINGLE_ACT_VSI_ID_S)
#define ICE_SINGLE_ACT_VSI_LIST_ID_S	4
#define ICE_SINGLE_ACT_VSI_LIST_ID_M	(0x3FF << ICE_SINGLE_ACT_VSI_LIST_ID_S)
	/* This bit needs to be set if action is forward to VSI list */
#define ICE_SINGLE_ACT_VSI_LIST		BIT(14)
#define ICE_SINGLE_ACT_VALID_BIT	BIT(17)
#define ICE_SINGLE_ACT_DROP		BIT(18)

	/* Action type = 1 - Forward to Queue of Queue group */
#define ICE_SINGLE_ACT_TO_Q		0x1
#define ICE_SINGLE_ACT_Q_INDEX_S	4
#define ICE_SINGLE_ACT_Q_INDEX_M	(0x7FF << ICE_SINGLE_ACT_Q_INDEX_S)
#define ICE_SINGLE_ACT_Q_REGION_S	15
#define ICE_SINGLE_ACT_Q_REGION_M	(0x7 << ICE_SINGLE_ACT_Q_REGION_S)
#define ICE_SINGLE_ACT_Q_PRIORITY	BIT(18)

	/* Action type = 2 - Prune */
#define ICE_SINGLE_ACT_PRUNE		0x2
#define ICE_SINGLE_ACT_EGRESS		BIT(15)
#define ICE_SINGLE_ACT_INGRESS		BIT(16)
#define ICE_SINGLE_ACT_PRUNET		BIT(17)
	/* Bit 18 should be set to 0 for this action */

	/* Action type = 2 - Pointer */
#define ICE_SINGLE_ACT_PTR		0x2
#define ICE_SINGLE_ACT_PTR_VAL_S	4
#define ICE_SINGLE_ACT_PTR_VAL_M	(0x1FFF << ICE_SINGLE_ACT_PTR_VAL_S)
	/* Bit 17 should be set if pointed action includes a FWD cmd */
#define ICE_SINGLE_ACT_PTR_HAS_FWD	BIT(17)
	/* Bit 18 should be set to 1 */
#define ICE_SINGLE_ACT_PTR_BIT		BIT(18)

	/* Action type = 3 - Other actions. Last two bits
	 * are other action identifier
	 */
#define ICE_SINGLE_ACT_OTHER_ACTS		0x3
#define ICE_SINGLE_OTHER_ACT_IDENTIFIER_S	17
#define ICE_SINGLE_OTHER_ACT_IDENTIFIER_M	\
				(0x3 << ICE_SINGLE_OTHER_ACT_IDENTIFIER_S)

	/* Bit 17:18 - Defines other actions */
	/* Other action = 0 - Mirror VSI */
#define ICE_SINGLE_OTHER_ACT_MIRROR		0
#define ICE_SINGLE_ACT_MIRROR_VSI_ID_S	4
#define ICE_SINGLE_ACT_MIRROR_VSI_ID_M	\
				(0x3FF << ICE_SINGLE_ACT_MIRROR_VSI_ID_S)

	/* Other action = 3 - Set Stat count */
#define ICE_SINGLE_OTHER_ACT_STAT_COUNT		3
#define ICE_SINGLE_ACT_STAT_COUNT_INDEX_S	4
#define ICE_SINGLE_ACT_STAT_COUNT_INDEX_M	\
				(0x7F << ICE_SINGLE_ACT_STAT_COUNT_INDEX_S)

	__le16 index; /* The index of the rule in the lookup table */
	/* Length and values of the header to be matched per recipe or
	 * lookup-type
	 */
	__le16 hdr_len;
	u8 hdr_data[STRUCT_HACK_VAR_LEN];
};

#pragma pack(1)
/* Add/Update/Remove large action command/response entry
 * "index" is returned as part of a response to a successful Add command, and
 * can be used to identify the action for Update/Get/Remove commands.
 */
struct ice_sw_rule_lg_act {
	struct ice_aqc_sw_rules_elem_hdr hdr;

	__le16 index; /* Index in large action table */
	__le16 size;
	/* Max number of large actions */
#define ICE_MAX_LG_ACT	4
	/* Bit 0:1 - Action type */
#define ICE_LG_ACT_TYPE_S	0
#define ICE_LG_ACT_TYPE_M	(0x7 << ICE_LG_ACT_TYPE_S)

	/* Action type = 0 - Forward to VSI or VSI list */
#define ICE_LG_ACT_VSI_FORWARDING	0
#define ICE_LG_ACT_VSI_ID_S		3
#define ICE_LG_ACT_VSI_ID_M		(0x3FF << ICE_LG_ACT_VSI_ID_S)
#define ICE_LG_ACT_VSI_LIST_ID_S	3
#define ICE_LG_ACT_VSI_LIST_ID_M	(0x3FF << ICE_LG_ACT_VSI_LIST_ID_S)
	/* This bit needs to be set if action is forward to VSI list */
#define ICE_LG_ACT_VSI_LIST		BIT(13)

#define ICE_LG_ACT_VALID_BIT		BIT(16)

	/* Action type = 1 - Forward to Queue of Queue group */
#define ICE_LG_ACT_TO_Q			0x1
#define ICE_LG_ACT_Q_INDEX_S		3
#define ICE_LG_ACT_Q_INDEX_M		(0x7FF << ICE_LG_ACT_Q_INDEX_S)
#define ICE_LG_ACT_Q_REGION_S		14
#define ICE_LG_ACT_Q_REGION_M		(0x7 << ICE_LG_ACT_Q_REGION_S)
#define ICE_LG_ACT_Q_PRIORITY_SET	BIT(17)

	/* Action type = 2 - Prune */
#define ICE_LG_ACT_PRUNE		0x2
#define ICE_LG_ACT_EGRESS		BIT(14)
#define ICE_LG_ACT_INGRESS		BIT(15)
#define ICE_LG_ACT_PRUNET		BIT(16)

	/* Action type = 3 - Mirror VSI */
#define ICE_LG_OTHER_ACT_MIRROR		0x3
#define ICE_LG_ACT_MIRROR_VSI_ID_S	3
#define ICE_LG_ACT_MIRROR_VSI_ID_M	(0x3FF << ICE_LG_ACT_MIRROR_VSI_ID_S)

	/* Action type = 5 - Generic Value */
#define ICE_LG_ACT_GENERIC		0x5
#define ICE_LG_ACT_GENERIC_VALUE_S	3
#define ICE_LG_ACT_GENERIC_VALUE_M	(0xFFFF << ICE_LG_ACT_GENERIC_VALUE_S)
#define ICE_LG_ACT_GENERIC_OFFSET_S	19
#define ICE_LG_ACT_GENERIC_OFFSET_M	(0x7 << ICE_LG_ACT_GENERIC_OFFSET_S)
#define ICE_LG_ACT_GENERIC_PRIORITY_S	22
#define ICE_LG_ACT_GENERIC_PRIORITY_M	(0x7 << ICE_LG_ACT_GENERIC_PRIORITY_S)
#define ICE_LG_ACT_GENERIC_OFF_RX_DESC_PROF_IDX	7

	/* Action = 7 - Set Stat count */
#define ICE_LG_ACT_STAT_COUNT		0x7
#define ICE_LG_ACT_STAT_COUNT_S		3
#define ICE_LG_ACT_STAT_COUNT_M		(0x7F << ICE_LG_ACT_STAT_COUNT_S)
	__le32 act[STRUCT_HACK_VAR_LEN]; /* array of size for actions */
};
#pragma pack()

#pragma pack(1)
/* Add/Update/Remove VSI list command/response entry
 * "index" is returned as part of a response to a successful Add command, and
 * can be used to identify the VSI list for Update/Get/Remove commands.
 */
struct ice_sw_rule_vsi_list {
	struct ice_aqc_sw_rules_elem_hdr hdr;

	__le16 index; /* Index of VSI/Prune list */
	__le16 number_vsi;
	__le16 vsi[STRUCT_HACK_VAR_LEN]; /* Array of number_vsi VSI numbers */
};
#pragma pack()

#pragma pack(1)
/* Query VSI list command/response entry */
struct ice_sw_rule_vsi_list_query {
	__le16 index;
	u8 vsi_list[DIVIDE_AND_ROUND_UP(ICE_MAX_VSI, BITS_PER_BYTE)];
};
#pragma pack()

/* PFC Ignore (direct 0x0301)
 * The command and response use the same descriptor structure
 */
struct ice_aqc_pfc_ignore {
	u8	tc_bitmap;
	u8	cmd_flags; /* unused in response */
#define ICE_AQC_PFC_IGNORE_SET		BIT(7)
#define ICE_AQC_PFC_IGNORE_CLEAR	0
	u8	reserved[14];
};

/* Query PFC Mode (direct 0x0302)
 * Set PFC Mode (direct 0x0303)
 */
struct ice_aqc_set_query_pfc_mode {
	u8	pfc_mode;
/* For Set Command response, reserved in all other cases */
#define ICE_AQC_PFC_NOT_CONFIGURED	0
/* For Query Command response, reserved in all other cases */
#define ICE_AQC_DCB_DIS		0
#define ICE_AQC_PFC_VLAN_BASED_PFC	1
#define ICE_AQC_PFC_DSCP_BASED_PFC	2
	u8	rsvd[15];
};

/* Set DCB Parameters (direct 0x0306) */
struct ice_aqc_set_dcb_params {
	u8 cmd_flags; /* unused in response */
#define ICE_AQC_LINK_UP_DCB_CFG    BIT(0)
#define ICE_AQC_PERSIST_DCB_CFG    BIT(1)
	u8 valid_flags; /* unused in response */
#define ICE_AQC_LINK_UP_DCB_CFG_VALID    BIT(0)
#define ICE_AQC_PERSIST_DCB_CFG_VALID    BIT(1)
	u8 rsvd[14];
};

/* Get Default Topology (indirect 0x0400) */
struct ice_aqc_get_topo {
	u8 port_num;
	u8 num_branches;
	__le16 reserved1;
	__le32 reserved2;
	__le32 addr_high;
	__le32 addr_low;
};

/* Get/Set Tx Topology (indirect 0x0418/0x0417) */
struct ice_aqc_get_set_tx_topo {
	u8 set_flags;
#define ICE_AQC_TX_TOPO_FLAGS_CORRER		BIT(0)
#define ICE_AQC_TX_TOPO_FLAGS_SRC_RAM		BIT(1)
#define ICE_AQC_TX_TOPO_FLAGS_SET_PSM		BIT(2)
#define ICE_AQC_TX_TOPO_FLAGS_LOAD_NEW		BIT(4)
#define ICE_AQC_TX_TOPO_FLAGS_ISSUED		BIT(5)
	u8 get_flags;
#define ICE_AQC_TX_TOPO_GET_NO_UPDATE		0
#define ICE_AQC_TX_TOPO_GET_PSM			1
#define ICE_AQC_TX_TOPO_GET_RAM			2
	__le16 reserved1;
	__le32 reserved2;
	__le32 addr_high;
	__le32 addr_low;
};

/* Update TSE (indirect 0x0403)
 * Get TSE (indirect 0x0404)
 * Add TSE (indirect 0x0401)
 * Delete TSE (indirect 0x040F)
 * Move TSE (indirect 0x0408)
 * Suspend Nodes (indirect 0x0409)
 * Resume Nodes (indirect 0x040A)
 */
struct ice_aqc_sched_elem_cmd {
	__le16 num_elem_req;	/* Used by commands */
	__le16 num_elem_resp;	/* Used by responses */
	__le32 reserved;
	__le32 addr_high;
	__le32 addr_low;
};

struct ice_aqc_txsched_move_grp_info_hdr {
	__le32 src_parent_teid;
	__le32 dest_parent_teid;
	__le16 num_elems;
	u8 flags;
	u8 reserved;
};

struct ice_aqc_move_elem {
	struct ice_aqc_txsched_move_grp_info_hdr hdr;
	__le32 teid[STRUCT_HACK_VAR_LEN];
};

struct ice_aqc_elem_info_bw {
	__le16 bw_profile_idx;
	__le16 bw_alloc;
};

struct ice_aqc_txsched_elem {
	u8 elem_type; /* Special field, reserved for some aq calls */
#define ICE_AQC_ELEM_TYPE_UNDEFINED		0x0
#define ICE_AQC_ELEM_TYPE_ROOT_PORT		0x1
#define ICE_AQC_ELEM_TYPE_TC			0x2
#define ICE_AQC_ELEM_TYPE_SE_GENERIC		0x3
#define ICE_AQC_ELEM_TYPE_ENTRY_POINT		0x4
#define ICE_AQC_ELEM_TYPE_LEAF			0x5
#define ICE_AQC_ELEM_TYPE_SE_PADDED		0x6
	u8 valid_sections;
#define ICE_AQC_ELEM_VALID_GENERIC		BIT(0)
#define ICE_AQC_ELEM_VALID_CIR			BIT(1)
#define ICE_AQC_ELEM_VALID_EIR			BIT(2)
#define ICE_AQC_ELEM_VALID_SHARED		BIT(3)
	u8 generic;
#define ICE_AQC_ELEM_GENERIC_MODE_M		0x1
#define ICE_AQC_ELEM_GENERIC_PRIO_S		0x1
#define ICE_AQC_ELEM_GENERIC_PRIO_M		(0x7 << ICE_AQC_ELEM_GENERIC_PRIO_S)
#define ICE_AQC_ELEM_GENERIC_SP_S		0x4
#define ICE_AQC_ELEM_GENERIC_SP_M		(0x1 << ICE_AQC_ELEM_GENERIC_SP_S)
#define ICE_AQC_ELEM_GENERIC_ADJUST_VAL_S	0x5
#define ICE_AQC_ELEM_GENERIC_ADJUST_VAL_M	\
	(0x3 << ICE_AQC_ELEM_GENERIC_ADJUST_VAL_S)
	u8 flags; /* Special field, reserved for some aq calls */
#define ICE_AQC_ELEM_FLAG_SUSPEND_M		0x1
	struct ice_aqc_elem_info_bw cir_bw;
	struct ice_aqc_elem_info_bw eir_bw;
	__le16 srl_id;
	__le16 reserved2;
};

struct ice_aqc_txsched_elem_data {
	__le32 parent_teid;
	__le32 node_teid;
	struct ice_aqc_txsched_elem data;
};

struct ice_aqc_txsched_topo_grp_info_hdr {
	__le32 parent_teid;
	__le16 num_elems;
	__le16 reserved2;
};

struct ice_aqc_add_elem {
	struct ice_aqc_txsched_topo_grp_info_hdr hdr;
	struct ice_aqc_txsched_elem_data generic[STRUCT_HACK_VAR_LEN];
};

struct ice_aqc_get_topo_elem {
	struct ice_aqc_txsched_topo_grp_info_hdr hdr;
	struct ice_aqc_txsched_elem_data
		generic[ICE_AQC_TOPO_MAX_LEVEL_NUM];
};

struct ice_aqc_delete_elem {
	struct ice_aqc_txsched_topo_grp_info_hdr hdr;
	__le32 teid[STRUCT_HACK_VAR_LEN];
};

/* Query Port ETS (indirect 0x040E)
 *
 * This indirect command is used to query port TC node configuration.
 */
struct ice_aqc_query_port_ets {
	__le32 port_teid;
	__le32 reserved;
	__le32 addr_high;
	__le32 addr_low;
};

struct ice_aqc_port_ets_elem {
	u8 tc_valid_bits;
	u8 reserved[3];
	/* 3 bits for UP per TC 0-7, 4th byte reserved */
	__le32 up2tc;
	u8 tc_bw_share[8];
	__le32 port_eir_prof_id;
	__le32 port_cir_prof_id;
	/* 3 bits per Node priority to TC 0-7, 4th byte reserved */
	__le32 tc_node_prio;
#define ICE_TC_NODE_PRIO_S	0x4
	u8 reserved1[4];
	__le32 tc_node_teid[8]; /* Used for response, reserved in command */
};

/* Rate limiting profile for
 * Add RL profile (indirect 0x0410)
 * Query RL profile (indirect 0x0411)
 * Remove RL profile (indirect 0x0415)
 * These indirect commands acts on single or multiple
 * RL profiles with specified data.
 */
struct ice_aqc_rl_profile {
	__le16 num_profiles;
	__le16 num_processed; /* Only for response. Reserved in Command. */
	u8 reserved[4];
	__le32 addr_high;
	__le32 addr_low;
};

struct ice_aqc_rl_profile_elem {
	u8 level;
	u8 flags;
#define ICE_AQC_RL_PROFILE_TYPE_S	0x0
#define ICE_AQC_RL_PROFILE_TYPE_M	(0x3 << ICE_AQC_RL_PROFILE_TYPE_S)
#define ICE_AQC_RL_PROFILE_TYPE_CIR	0
#define ICE_AQC_RL_PROFILE_TYPE_EIR	1
#define ICE_AQC_RL_PROFILE_TYPE_SRL	2
/* The following flag is used for Query RL Profile Data */
#define ICE_AQC_RL_PROFILE_INVAL_S	0x7
#define ICE_AQC_RL_PROFILE_INVAL_M	(0x1 << ICE_AQC_RL_PROFILE_INVAL_S)

	__le16 profile_id;
	__le16 max_burst_size;
	__le16 rl_multiply;
	__le16 wake_up_calc;
	__le16 rl_encode;
};

/* Config Node Attributes (indirect 0x0419)
 * Query Node Attributes (indirect 0x041A)
 */
struct ice_aqc_node_attr {
	__le16 num_entries; /* Number of attributes structures in the buffer */
	u8 reserved[6];
	__le32 addr_high;
	__le32 addr_low;
};

struct ice_aqc_node_attr_elem {
	__le32 node_teid;
	__le16 max_children;
	__le16 children_level;
};

/* Configure L2 Node CGD (indirect 0x0414)
 * This indirect command allows configuring a congestion domain for given L2
 * node TEIDs in the scheduler topology.
 */
struct ice_aqc_cfg_l2_node_cgd {
	__le16 num_l2_nodes;
	u8 reserved[6];
	__le32 addr_high;
	__le32 addr_low;
};

struct ice_aqc_cfg_l2_node_cgd_elem {
	__le32 node_teid;
	u8 cgd;
	u8 reserved[3];
};

/* Query Scheduler Resource Allocation (indirect 0x0412)
 * This indirect command retrieves the scheduler resources allocated by
 * EMP Firmware to the given PF.
 */
struct ice_aqc_query_txsched_res {
	u8 reserved[8];
	__le32 addr_high;
	__le32 addr_low;
};

struct ice_aqc_generic_sched_props {
	__le16 phys_levels;
	__le16 logical_levels;
	u8 flattening_bitmap;
	u8 max_device_cgds;
	u8 max_pf_cgds;
	u8 rsvd0;
	__le16 rdma_qsets;
	u8 rsvd1[22];
};

struct ice_aqc_layer_props {
	u8 logical_layer;
	u8 chunk_size;
	__le16 max_device_nodes;
	__le16 max_pf_nodes;
	u8 rsvd0[4];
	__le16 max_sibl_grp_sz;
	__le16 max_cir_rl_profiles;
	__le16 max_eir_rl_profiles;
	__le16 max_srl_profiles;
	u8 rsvd1[14];
};

struct ice_aqc_query_txsched_res_resp {
	struct ice_aqc_generic_sched_props sched_props;
	struct ice_aqc_layer_props layer_props[ICE_AQC_TOPO_MAX_LEVEL_NUM];
};

/* Query Node to Root Topology (indirect 0x0413)
 * This command uses ice_aqc_get_elem as its data buffer.
 */
struct ice_aqc_query_node_to_root {
	__le32 teid;
	__le32 num_nodes; /* Response only */
	__le32 addr_high;
	__le32 addr_low;
};

/* Get PHY capabilities (indirect 0x0600) */
struct ice_aqc_get_phy_caps {
	u8 lport_num;
	u8 reserved;
	__le16 param0;
	/* 18.0 - Report qualified modules */
#define ICE_AQC_GET_PHY_RQM		BIT(0)
	/* 18.1 - 18.3 : Report mode
	 * 000b - Report topology capabilities, without media
	 * 001b - Report topology capabilities, with media
	 * 010b - Report Active configuration
	 * 011b - Report PHY Type and FEC mode capabilities
	 * 100b - Report Default capabilities
	 */
#define ICE_AQC_REPORT_MODE_S			1
#define ICE_AQC_REPORT_MODE_M			(7 << ICE_AQC_REPORT_MODE_S)
#define ICE_AQC_REPORT_TOPO_CAP_NO_MEDIA	0
#define ICE_AQC_REPORT_TOPO_CAP_MEDIA		BIT(1)
#define ICE_AQC_REPORT_ACTIVE_CFG		BIT(2)
#define ICE_AQC_REPORT_DFLT_CFG			BIT(3)
	__le32 reserved1;
	__le32 addr_high;
	__le32 addr_low;
};

/* This is #define of PHY type (Extended):
 * The first set of defines is for phy_type_low.
 */
#define ICE_PHY_TYPE_LOW_100BASE_TX		BIT_ULL(0)
#define ICE_PHY_TYPE_LOW_100M_SGMII		BIT_ULL(1)
#define ICE_PHY_TYPE_LOW_1000BASE_T		BIT_ULL(2)
#define ICE_PHY_TYPE_LOW_1000BASE_SX		BIT_ULL(3)
#define ICE_PHY_TYPE_LOW_1000BASE_LX		BIT_ULL(4)
#define ICE_PHY_TYPE_LOW_1000BASE_KX		BIT_ULL(5)
#define ICE_PHY_TYPE_LOW_1G_SGMII		BIT_ULL(6)
#define ICE_PHY_TYPE_LOW_2500BASE_T		BIT_ULL(7)
#define ICE_PHY_TYPE_LOW_2500BASE_X		BIT_ULL(8)
#define ICE_PHY_TYPE_LOW_2500BASE_KX		BIT_ULL(9)
#define ICE_PHY_TYPE_LOW_5GBASE_T		BIT_ULL(10)
#define ICE_PHY_TYPE_LOW_5GBASE_KR		BIT_ULL(11)
#define ICE_PHY_TYPE_LOW_10GBASE_T		BIT_ULL(12)
#define ICE_PHY_TYPE_LOW_10G_SFI_DA		BIT_ULL(13)
#define ICE_PHY_TYPE_LOW_10GBASE_SR		BIT_ULL(14)
#define ICE_PHY_TYPE_LOW_10GBASE_LR		BIT_ULL(15)
#define ICE_PHY_TYPE_LOW_10GBASE_KR_CR1		BIT_ULL(16)
#define ICE_PHY_TYPE_LOW_10G_SFI_AOC_ACC	BIT_ULL(17)
#define ICE_PHY_TYPE_LOW_10G_SFI_C2C		BIT_ULL(18)
#define ICE_PHY_TYPE_LOW_25GBASE_T		BIT_ULL(19)
#define ICE_PHY_TYPE_LOW_25GBASE_CR		BIT_ULL(20)
#define ICE_PHY_TYPE_LOW_25GBASE_CR_S		BIT_ULL(21)
#define ICE_PHY_TYPE_LOW_25GBASE_CR1		BIT_ULL(22)
#define ICE_PHY_TYPE_LOW_25GBASE_SR		BIT_ULL(23)
#define ICE_PHY_TYPE_LOW_25GBASE_LR		BIT_ULL(24)
#define ICE_PHY_TYPE_LOW_25GBASE_KR		BIT_ULL(25)
#define ICE_PHY_TYPE_LOW_25GBASE_KR_S		BIT_ULL(26)
#define ICE_PHY_TYPE_LOW_25GBASE_KR1		BIT_ULL(27)
#define ICE_PHY_TYPE_LOW_25G_AUI_AOC_ACC	BIT_ULL(28)
#define ICE_PHY_TYPE_LOW_25G_AUI_C2C		BIT_ULL(29)
#define ICE_PHY_TYPE_LOW_40GBASE_CR4		BIT_ULL(30)
#define ICE_PHY_TYPE_LOW_40GBASE_SR4		BIT_ULL(31)
#define ICE_PHY_TYPE_LOW_40GBASE_LR4		BIT_ULL(32)
#define ICE_PHY_TYPE_LOW_40GBASE_KR4		BIT_ULL(33)
#define ICE_PHY_TYPE_LOW_40G_XLAUI_AOC_ACC	BIT_ULL(34)
#define ICE_PHY_TYPE_LOW_40G_XLAUI		BIT_ULL(35)
#define ICE_PHY_TYPE_LOW_50GBASE_CR2		BIT_ULL(36)
#define ICE_PHY_TYPE_LOW_50GBASE_SR2		BIT_ULL(37)
#define ICE_PHY_TYPE_LOW_50GBASE_LR2		BIT_ULL(38)
#define ICE_PHY_TYPE_LOW_50GBASE_KR2		BIT_ULL(39)
#define ICE_PHY_TYPE_LOW_50G_LAUI2_AOC_ACC	BIT_ULL(40)
#define ICE_PHY_TYPE_LOW_50G_LAUI2		BIT_ULL(41)
#define ICE_PHY_TYPE_LOW_50G_AUI2_AOC_ACC	BIT_ULL(42)
#define ICE_PHY_TYPE_LOW_50G_AUI2		BIT_ULL(43)
#define ICE_PHY_TYPE_LOW_50GBASE_CP		BIT_ULL(44)
#define ICE_PHY_TYPE_LOW_50GBASE_SR		BIT_ULL(45)
#define ICE_PHY_TYPE_LOW_50GBASE_FR		BIT_ULL(46)
#define ICE_PHY_TYPE_LOW_50GBASE_LR		BIT_ULL(47)
#define ICE_PHY_TYPE_LOW_50GBASE_KR_PAM4	BIT_ULL(48)
#define ICE_PHY_TYPE_LOW_50G_AUI1_AOC_ACC	BIT_ULL(49)
#define ICE_PHY_TYPE_LOW_50G_AUI1		BIT_ULL(50)
#define ICE_PHY_TYPE_LOW_100GBASE_CR4		BIT_ULL(51)
#define ICE_PHY_TYPE_LOW_100GBASE_SR4		BIT_ULL(52)
#define ICE_PHY_TYPE_LOW_100GBASE_LR4		BIT_ULL(53)
#define ICE_PHY_TYPE_LOW_100GBASE_KR4		BIT_ULL(54)
#define ICE_PHY_TYPE_LOW_100G_CAUI4_AOC_ACC	BIT_ULL(55)
#define ICE_PHY_TYPE_LOW_100G_CAUI4		BIT_ULL(56)
#define ICE_PHY_TYPE_LOW_100G_AUI4_AOC_ACC	BIT_ULL(57)
#define ICE_PHY_TYPE_LOW_100G_AUI4		BIT_ULL(58)
#define ICE_PHY_TYPE_LOW_100GBASE_CR_PAM4	BIT_ULL(59)
#define ICE_PHY_TYPE_LOW_100GBASE_KR_PAM4	BIT_ULL(60)
#define ICE_PHY_TYPE_LOW_100GBASE_CP2		BIT_ULL(61)
#define ICE_PHY_TYPE_LOW_100GBASE_SR2		BIT_ULL(62)
#define ICE_PHY_TYPE_LOW_100GBASE_DR		BIT_ULL(63)
#define ICE_PHY_TYPE_LOW_MAX_INDEX		63
/* The second set of defines is for phy_type_high. */
#define ICE_PHY_TYPE_HIGH_100GBASE_KR2_PAM4	BIT_ULL(0)
#define ICE_PHY_TYPE_HIGH_100G_CAUI2_AOC_ACC	BIT_ULL(1)
#define ICE_PHY_TYPE_HIGH_100G_CAUI2		BIT_ULL(2)
#define ICE_PHY_TYPE_HIGH_100G_AUI2_AOC_ACC	BIT_ULL(3)
#define ICE_PHY_TYPE_HIGH_100G_AUI2		BIT_ULL(4)
#define ICE_PHY_TYPE_HIGH_200G_CR4_PAM4		BIT_ULL(5)
#define ICE_PHY_TYPE_HIGH_200G_SR4		BIT_ULL(6)
#define ICE_PHY_TYPE_HIGH_200G_FR4		BIT_ULL(7)
#define ICE_PHY_TYPE_HIGH_200G_LR4		BIT_ULL(8)
#define ICE_PHY_TYPE_HIGH_200G_DR4		BIT_ULL(9)
#define ICE_PHY_TYPE_HIGH_200G_KR4_PAM4		BIT_ULL(10)
#define ICE_PHY_TYPE_HIGH_200G_AUI4_AOC_ACC	BIT_ULL(11)
#define ICE_PHY_TYPE_HIGH_200G_AUI4		BIT_ULL(12)
#define ICE_PHY_TYPE_HIGH_200G_AUI8_AOC_ACC	BIT_ULL(13)
#define ICE_PHY_TYPE_HIGH_200G_AUI8		BIT_ULL(14)
#define ICE_PHY_TYPE_HIGH_400GBASE_FR8		BIT_ULL(15)
#define ICE_PHY_TYPE_HIGH_MAX_INDEX		15

struct ice_aqc_get_phy_caps_data {
	__le64 phy_type_low; /* Use values from ICE_PHY_TYPE_LOW_* */
	__le64 phy_type_high; /* Use values from ICE_PHY_TYPE_HIGH_* */
	u8 caps;
#define ICE_AQC_PHY_EN_TX_LINK_PAUSE			BIT(0)
#define ICE_AQC_PHY_EN_RX_LINK_PAUSE			BIT(1)
#define ICE_AQC_PHY_LOW_POWER_MODE			BIT(2)
#define ICE_AQC_PHY_EN_LINK				BIT(3)
#define ICE_AQC_PHY_AN_MODE				BIT(4)
#define ICE_AQC_PHY_EN_MOD_QUAL				BIT(5)
#define ICE_AQC_PHY_EN_LESM				BIT(6)
#define ICE_AQC_PHY_EN_AUTO_FEC				BIT(7)
#define ICE_AQC_PHY_CAPS_MASK				MAKEMASK(0xff, 0)
	u8 low_power_ctrl_an;
#define ICE_AQC_PHY_EN_D3COLD_LOW_POWER_AUTONEG		BIT(0)
#define ICE_AQC_PHY_AN_EN_CLAUSE28			BIT(1)
#define ICE_AQC_PHY_AN_EN_CLAUSE73			BIT(2)
#define ICE_AQC_PHY_AN_EN_CLAUSE37			BIT(3)
	__le16 eee_cap;
#define ICE_AQC_PHY_EEE_EN_100BASE_TX			BIT(0)
#define ICE_AQC_PHY_EEE_EN_1000BASE_T			BIT(1)
#define ICE_AQC_PHY_EEE_EN_10GBASE_T			BIT(2)
#define ICE_AQC_PHY_EEE_EN_1000BASE_KX			BIT(3)
#define ICE_AQC_PHY_EEE_EN_10GBASE_KR			BIT(4)
#define ICE_AQC_PHY_EEE_EN_25GBASE_KR			BIT(5)
#define ICE_AQC_PHY_EEE_EN_40GBASE_KR4			BIT(6)
#define ICE_AQC_PHY_EEE_EN_50GBASE_KR2			BIT(7)
#define ICE_AQC_PHY_EEE_EN_50GBASE_KR_PAM4		BIT(8)
#define ICE_AQC_PHY_EEE_EN_100GBASE_KR4			BIT(9)
#define ICE_AQC_PHY_EEE_EN_100GBASE_KR2_PAM4		BIT(10)
	__le16 eeer_value;
	u8 phy_id_oui[4]; /* PHY/Module ID connected on the port */
	u8 phy_fw_ver[8];
	u8 link_fec_options;
#define ICE_AQC_PHY_FEC_10G_KR_40G_KR4_EN		BIT(0)
#define ICE_AQC_PHY_FEC_10G_KR_40G_KR4_REQ		BIT(1)
#define ICE_AQC_PHY_FEC_25G_RS_528_REQ			BIT(2)
#define ICE_AQC_PHY_FEC_25G_KR_REQ			BIT(3)
#define ICE_AQC_PHY_FEC_25G_RS_544_REQ			BIT(4)
#define ICE_AQC_PHY_FEC_DIS				BIT(5)
#define ICE_AQC_PHY_FEC_25G_RS_CLAUSE91_EN		BIT(6)
#define ICE_AQC_PHY_FEC_25G_KR_CLAUSE74_EN		BIT(7)
#define ICE_AQC_PHY_FEC_MASK				MAKEMASK(0xdf, 0)
	u8 module_compliance_enforcement;
#define ICE_AQC_MOD_ENFORCE_STRICT_MODE			BIT(0)
	u8 extended_compliance_code;
#define ICE_MODULE_TYPE_TOTAL_BYTE			3
	u8 module_type[ICE_MODULE_TYPE_TOTAL_BYTE];
#define ICE_AQC_MOD_TYPE_BYTE0_SFP_PLUS			0xA0
#define ICE_AQC_MOD_TYPE_BYTE0_QSFP_PLUS		0x80
#define ICE_AQC_MOD_TYPE_IDENT				1
#define ICE_AQC_MOD_TYPE_BYTE1_SFP_PLUS_CU_PASSIVE	BIT(0)
#define ICE_AQC_MOD_TYPE_BYTE1_SFP_PLUS_CU_ACTIVE	BIT(1)
#define ICE_AQC_MOD_TYPE_BYTE1_10G_BASE_SR		BIT(4)
#define ICE_AQC_MOD_TYPE_BYTE1_10G_BASE_LR		BIT(5)
#define ICE_AQC_MOD_TYPE_BYTE1_10G_BASE_LRM		BIT(6)
#define ICE_AQC_MOD_TYPE_BYTE1_10G_BASE_ER		BIT(7)
#define ICE_AQC_MOD_TYPE_BYTE2_SFP_PLUS			0xA0
#define ICE_AQC_MOD_TYPE_BYTE2_QSFP_PLUS		0x86
	u8 qualified_module_count;
	u8 rsvd2[7];	/* Bytes 47:41 reserved */
#define ICE_AQC_QUAL_MOD_COUNT_MAX			16
	struct {
		u8 v_oui[3];
		u8 rsvd3;
		u8 v_part[16];
		__le32 v_rev;
		__le64 rsvd4;
	} qual_modules[ICE_AQC_QUAL_MOD_COUNT_MAX];
};

/* Set PHY capabilities (direct 0x0601)
 * NOTE: This command must be followed by setup link and restart auto-neg
 */
struct ice_aqc_set_phy_cfg {
	u8 lport_num;
	u8 reserved[7];
	__le32 addr_high;
	__le32 addr_low;
};

/* Set PHY config command data structure */
struct ice_aqc_set_phy_cfg_data {
	__le64 phy_type_low; /* Use values from ICE_PHY_TYPE_LOW_* */
	__le64 phy_type_high; /* Use values from ICE_PHY_TYPE_HIGH_* */
	u8 caps;
#define ICE_AQ_PHY_ENA_VALID_MASK	MAKEMASK(0xef, 0)
#define ICE_AQ_PHY_ENA_TX_PAUSE_ABILITY	BIT(0)
#define ICE_AQ_PHY_ENA_RX_PAUSE_ABILITY	BIT(1)
#define ICE_AQ_PHY_ENA_LOW_POWER	BIT(2)
#define ICE_AQ_PHY_ENA_LINK		BIT(3)
#define ICE_AQ_PHY_ENA_AUTO_LINK_UPDT	BIT(5)
#define ICE_AQ_PHY_ENA_LESM		BIT(6)
#define ICE_AQ_PHY_ENA_AUTO_FEC		BIT(7)
	u8 low_power_ctrl_an;
	__le16 eee_cap; /* Value from ice_aqc_get_phy_caps */
	__le16 eeer_value;
	u8 link_fec_opt; /* Use defines from ice_aqc_get_phy_caps */
	u8 module_compliance_enforcement;
};

/* Set MAC Config command data structure (direct 0x0603) */
struct ice_aqc_set_mac_cfg {
	__le16 max_frame_size;
	u8 params;
#define ICE_AQ_SET_MAC_PACE_S		3
#define ICE_AQ_SET_MAC_PACE_M		(0xF << ICE_AQ_SET_MAC_PACE_S)
#define ICE_AQ_SET_MAC_PACE_TYPE_M	BIT(7)
#define ICE_AQ_SET_MAC_PACE_TYPE_RATE	0
#define ICE_AQ_SET_MAC_PACE_TYPE_FIXED	ICE_AQ_SET_MAC_PACE_TYPE_M
	u8 tx_tmr_priority;
	__le16 tx_tmr_value;
	__le16 fc_refresh_threshold;
	u8 drop_opts;
#define ICE_AQ_SET_MAC_AUTO_DROP_MASK		BIT(0)
#define ICE_AQ_SET_MAC_AUTO_DROP_NONE		0
#define ICE_AQ_SET_MAC_AUTO_DROP_BLOCKING_PKTS	BIT(0)
	u8 reserved[7];
};

/* Restart AN command data structure (direct 0x0605)
 * Also used for response, with only the lport_num field present.
 */
struct ice_aqc_restart_an {
	u8 lport_num;
	u8 reserved;
	u8 cmd_flags;
#define ICE_AQC_RESTART_AN_LINK_RESTART	BIT(1)
#define ICE_AQC_RESTART_AN_LINK_ENABLE	BIT(2)
	u8 reserved2[13];
};

/* Get link status (indirect 0x0607), also used for Link Status Event */
struct ice_aqc_get_link_status {
	u8 lport_num;
	u8 reserved;
	__le16 cmd_flags;
#define ICE_AQ_LSE_M			0x3
#define ICE_AQ_LSE_NOP			0x0
#define ICE_AQ_LSE_DIS			0x2
#define ICE_AQ_LSE_ENA			0x3
	/* only response uses this flag */
#define ICE_AQ_LSE_IS_ENABLED		0x1
	__le32 reserved2;
	__le32 addr_high;
	__le32 addr_low;
};

enum ice_get_link_status_data_version {
	ICE_GET_LINK_STATUS_DATA_V1 = 1,
	ICE_GET_LINK_STATUS_DATA_V2 = 2,
};

#define ICE_GET_LINK_STATUS_DATALEN_V1		32
#define ICE_GET_LINK_STATUS_DATALEN_V2		56

/* Get link status response data structure, also used for Link Status Event */
#pragma pack(1)
struct ice_aqc_get_link_status_data {
	u8 topo_media_conflict;
#define ICE_AQ_LINK_TOPO_CONFLICT	BIT(0)
#define ICE_AQ_LINK_MEDIA_CONFLICT	BIT(1)
#define ICE_AQ_LINK_TOPO_CORRUPT	BIT(2)
#define ICE_AQ_LINK_TOPO_UNREACH_PRT	BIT(4)
#define ICE_AQ_LINK_TOPO_UNDRUTIL_PRT	BIT(5)
#define ICE_AQ_LINK_TOPO_UNDRUTIL_MEDIA	BIT(6)
#define ICE_AQ_LINK_TOPO_UNSUPP_MEDIA	BIT(7)
	u8 link_cfg_err;
#define ICE_AQ_LINK_CFG_ERR			BIT(0)
#define ICE_AQ_LINK_ACT_PORT_OPT_INVAL		BIT(2)
#define ICE_AQ_LINK_FEAT_ID_OR_CONFIG_ID_INVAL	BIT(3)
#define ICE_AQ_LINK_TOPO_CRITICAL_SDP_ERR	BIT(4)
#define ICE_AQ_LINK_MODULE_POWER_UNSUPPORTED	BIT(5)
#define ICE_AQ_LINK_EXTERNAL_PHY_LOAD_FAILURE	BIT(6)
#define ICE_AQ_LINK_INVAL_MAX_POWER_LIMIT	BIT(7)
	u8 link_info;
#define ICE_AQ_LINK_UP			BIT(0)	/* Link Status */
#define ICE_AQ_LINK_FAULT		BIT(1)
#define ICE_AQ_LINK_FAULT_TX		BIT(2)
#define ICE_AQ_LINK_FAULT_RX		BIT(3)
#define ICE_AQ_LINK_FAULT_REMOTE	BIT(4)
#define ICE_AQ_LINK_UP_PORT		BIT(5)	/* External Port Link Status */
#define ICE_AQ_MEDIA_AVAILABLE		BIT(6)
#define ICE_AQ_SIGNAL_DETECT		BIT(7)
	u8 an_info;
#define ICE_AQ_AN_COMPLETED		BIT(0)
#define ICE_AQ_LP_AN_ABILITY		BIT(1)
#define ICE_AQ_PD_FAULT			BIT(2)	/* Parallel Detection Fault */
#define ICE_AQ_FEC_EN			BIT(3)
#define ICE_AQ_PHY_LOW_POWER		BIT(4)	/* Low Power State */
#define ICE_AQ_LINK_PAUSE_TX		BIT(5)
#define ICE_AQ_LINK_PAUSE_RX		BIT(6)
#define ICE_AQ_QUALIFIED_MODULE		BIT(7)
	u8 ext_info;
#define ICE_AQ_LINK_PHY_TEMP_ALARM	BIT(0)
#define ICE_AQ_LINK_EXCESSIVE_ERRORS	BIT(1)	/* Excessive Link Errors */
	/* Port Tx Suspended */
#define ICE_AQ_LINK_TX_S		2
#define ICE_AQ_LINK_TX_M		(0x03 << ICE_AQ_LINK_TX_S)
#define ICE_AQ_LINK_TX_ACTIVE		0
#define ICE_AQ_LINK_TX_DRAINED		1
#define ICE_AQ_LINK_TX_FLUSHED		3
	u8 lb_status;
#define ICE_AQ_LINK_LB_PHY_LCL		BIT(0)
#define ICE_AQ_LINK_LB_PHY_RMT		BIT(1)
#define ICE_AQ_LINK_LB_MAC_LCL		BIT(2)
#define ICE_AQ_LINK_LB_PHY_IDX_S	3
#define ICE_AQ_LINK_LB_PHY_IDX_M	(0x7 << ICE_AQ_LB_PHY_IDX_S)
	__le16 max_frame_size;
	u8 cfg;
#define ICE_AQ_LINK_25G_KR_FEC_EN	BIT(0)
#define ICE_AQ_LINK_25G_RS_528_FEC_EN	BIT(1)
#define ICE_AQ_LINK_25G_RS_544_FEC_EN	BIT(2)
#define ICE_AQ_FEC_MASK			MAKEMASK(0x7, 0)
	/* Pacing Config */
#define ICE_AQ_CFG_PACING_S		3
#define ICE_AQ_CFG_PACING_M		(0xF << ICE_AQ_CFG_PACING_S)
#define ICE_AQ_CFG_PACING_TYPE_M	BIT(7)
#define ICE_AQ_CFG_PACING_TYPE_AVG	0
#define ICE_AQ_CFG_PACING_TYPE_FIXED	ICE_AQ_CFG_PACING_TYPE_M
	/* External Device Power Ability */
	u8 power_desc;
#define ICE_AQ_PWR_CLASS_M		0x3F
#define ICE_AQ_LINK_PWR_BASET_LOW_HIGH	0
#define ICE_AQ_LINK_PWR_BASET_HIGH	1
#define ICE_AQ_LINK_PWR_QSFP_CLASS_1	0
#define ICE_AQ_LINK_PWR_QSFP_CLASS_2	1
#define ICE_AQ_LINK_PWR_QSFP_CLASS_3	2
#define ICE_AQ_LINK_PWR_QSFP_CLASS_4	3
	__le16 link_speed;
#define ICE_AQ_LINK_SPEED_M             0xFFF
#define ICE_AQ_LINK_SPEED_10MB		BIT(0)
#define ICE_AQ_LINK_SPEED_100MB		BIT(1)
#define ICE_AQ_LINK_SPEED_1000MB	BIT(2)
#define ICE_AQ_LINK_SPEED_2500MB	BIT(3)
#define ICE_AQ_LINK_SPEED_5GB		BIT(4)
#define ICE_AQ_LINK_SPEED_10GB		BIT(5)
#define ICE_AQ_LINK_SPEED_20GB		BIT(6)
#define ICE_AQ_LINK_SPEED_25GB		BIT(7)
#define ICE_AQ_LINK_SPEED_40GB		BIT(8)
#define ICE_AQ_LINK_SPEED_50GB		BIT(9)
#define ICE_AQ_LINK_SPEED_100GB		BIT(10)
#define ICE_AQ_LINK_SPEED_200GB		BIT(11)
#define ICE_AQ_LINK_SPEED_UNKNOWN	BIT(15)
	__le16 reserved3; /* Aligns next field to 8-byte boundary */
	u8 ext_fec_status;
#define ICE_AQ_LINK_RS_272_FEC_EN	BIT(0) /* RS 272 FEC enabled */
	u8 reserved4;
	__le64 phy_type_low; /* Use values from ICE_PHY_TYPE_LOW_* */
	__le64 phy_type_high; /* Use values from ICE_PHY_TYPE_HIGH_* */
	/* Get link status version 2 link partner data */
	__le64 lp_phy_type_low; /* Use values from ICE_PHY_TYPE_LOW_* */
	__le64 lp_phy_type_high; /* Use values from ICE_PHY_TYPE_HIGH_* */
	u8 lp_fec_adv;
#define ICE_AQ_LINK_LP_10G_KR_FEC_CAP	BIT(0)
#define ICE_AQ_LINK_LP_25G_KR_FEC_CAP	BIT(1)
#define ICE_AQ_LINK_LP_RS_528_FEC_CAP	BIT(2)
#define ICE_AQ_LINK_LP_50G_KR_272_FEC_CAP BIT(3)
#define ICE_AQ_LINK_LP_100G_KR_272_FEC_CAP BIT(4)
#define ICE_AQ_LINK_LP_200G_KR_272_FEC_CAP BIT(5)
	u8 lp_fec_req;
#define ICE_AQ_LINK_LP_10G_KR_FEC_REQ	BIT(0)
#define ICE_AQ_LINK_LP_25G_KR_FEC_REQ	BIT(1)
#define ICE_AQ_LINK_LP_RS_528_FEC_REQ	BIT(2)
#define ICE_AQ_LINK_LP_KR_272_FEC_REQ	BIT(3)
	u8 lp_flowcontrol;
#define ICE_AQ_LINK_LP_PAUSE_ADV	BIT(0)
#define ICE_AQ_LINK_LP_ASM_DIR_ADV	BIT(1)
	u8 reserved[5];
};

#pragma pack()

/* Set event mask command (direct 0x0613) */
struct ice_aqc_set_event_mask {
	u8	lport_num;
	u8	reserved[7];
	__le16	event_mask;
#define ICE_AQ_LINK_EVENT_UPDOWN		BIT(1)
#define ICE_AQ_LINK_EVENT_MEDIA_NA		BIT(2)
#define ICE_AQ_LINK_EVENT_LINK_FAULT		BIT(3)
#define ICE_AQ_LINK_EVENT_PHY_TEMP_ALARM	BIT(4)
#define ICE_AQ_LINK_EVENT_EXCESSIVE_ERRORS	BIT(5)
#define ICE_AQ_LINK_EVENT_SIGNAL_DETECT		BIT(6)
#define ICE_AQ_LINK_EVENT_AN_COMPLETED		BIT(7)
#define ICE_AQ_LINK_EVENT_MODULE_QUAL_FAIL	BIT(8)
#define ICE_AQ_LINK_EVENT_PORT_TX_SUSPENDED	BIT(9)
#define ICE_AQ_LINK_EVENT_TOPO_CONFLICT		BIT(10)
#define ICE_AQ_LINK_EVENT_MEDIA_CONFLICT	BIT(11)
#define ICE_AQ_LINK_EVENT_PHY_FW_LOAD_FAIL	BIT(12)
	u8	reserved1[6];
};

/* Set PHY Loopback command (direct 0x0619) */
struct ice_aqc_set_phy_lb {
	u8 lport_num;
	u8 lport_num_valid;
#define ICE_AQ_PHY_LB_PORT_NUM_VALID	BIT(0)
	u8 phy_index;
	u8 lb_mode;
#define ICE_AQ_PHY_LB_EN		BIT(0)
#define ICE_AQ_PHY_LB_TYPE_M		BIT(1)
#define ICE_AQ_PHY_LB_TYPE_LOCAL	0
#define ICE_AQ_PHY_LB_TYPE_REMOTE	ICE_AQ_PHY_LB_TYPE_M
#define ICE_AQ_PHY_LB_LEVEL_M		BIT(2)
#define ICE_AQ_PHY_LB_LEVEL_PMD		0
#define ICE_AQ_PHY_LB_LEVEL_PCS		ICE_AQ_PHY_LB_LEVEL_M
	u8 reserved2[12];
};

/* Set MAC Loopback command (direct 0x0620) */
struct ice_aqc_set_mac_lb {
	u8 lb_mode;
#define ICE_AQ_MAC_LB_EN		BIT(0)
#define ICE_AQ_MAC_LB_OSC_CLK		BIT(1)
	u8 reserved[15];
};

/* Get sensor reading (direct 0x0632) */
struct ice_aqc_get_sensor_reading {
	u8 sensor;
#define ICE_AQC_INT_TEMP_SENSOR		0x0
	u8 format;
#define ICE_AQC_INT_TEMP_FORMAT		0x0
	u8 reserved[6];
	__le32 addr_high;
	__le32 addr_low;
};

/* Get sensor reading response (direct 0x0632) */
struct ice_aqc_get_sensor_reading_resp {
	union {
		u8 raw[8];
		/* Output data for sensor 0x00, format 0x00 */
		struct {
			s8 temp;
			u8 temp_warning_threshold;
			u8 temp_critical_threshold;
			u8 temp_fatal_threshold;
			u8 reserved[4];
		} s0f0;
	} data;
};

/* DNL Get Status command (indirect 0x0680)
 * Structure used for the response, the command uses the generic
 * ice_aqc_generic struct to pass a buffer address to the FW.
 */
struct ice_aqc_dnl_get_status {
	u8 ctx;
	u8 status;
#define ICE_AQ_DNL_STATUS_IDLE		0x0
#define ICE_AQ_DNL_STATUS_RESERVED	0x1
#define ICE_AQ_DNL_STATUS_STOPPED	0x2
#define ICE_AQ_DNL_STATUS_FATAL		0x3 /* Fatal DNL engine error */
#define ICE_AQ_DNL_SRC_S		3
#define ICE_AQ_DNL_SRC_M		(0x3 << ICE_AQ_DNL_SRC_S)
#define ICE_AQ_DNL_SRC_NVM		(0x0 << ICE_AQ_DNL_SRC_S)
#define ICE_AQ_DNL_SRC_NVM_SCRATCH	(0x1 << ICE_AQ_DNL_SRC_S)
	u8 stack_ptr;
#define ICE_AQ_DNL_ST_PTR_S		0x0
#define ICE_AQ_DNL_ST_PTR_M		(0x7 << ICE_AQ_DNL_ST_PTR_S)
	u8 engine_flags;
#define ICE_AQ_DNL_FLAGS_ERROR		BIT(2)
#define ICE_AQ_DNL_FLAGS_NEGATIVE	BIT(3)
#define ICE_AQ_DNL_FLAGS_OVERFLOW	BIT(4)
#define ICE_AQ_DNL_FLAGS_ZERO		BIT(5)
#define ICE_AQ_DNL_FLAGS_CARRY		BIT(6)
#define ICE_AQ_DNL_FLAGS_JUMP		BIT(7)
	__le16 pc;
	__le16 activity_id;
	__le32 addr_high;
	__le32 addr_low;
};

struct ice_aqc_dnl_get_status_data {
	__le16 activity_err_code;
	__le16 act_err_code;
#define ICE_AQ_DNL_ACT_ERR_SUCCESS	0x0000 /* no error */
#define ICE_AQ_DNL_ACT_ERR_PARSE	0x8001 /* NVM parse error */
#define ICE_AQ_DNL_ACT_ERR_UNSUPPORTED	0x8002 /* unsupported action */
#define ICE_AQ_DNL_ACT_ERR_NOT_FOUND	0x8003 /* activity not found */
#define ICE_AQ_DNL_ACT_ERR_BAD_JUMP	0x8004 /* an illegal jump */
#define ICE_AQ_DNL_ACT_ERR_PSTO_OVER	0x8005 /* persistent store overflow */
#define ICE_AQ_DNL_ACT_ERR_ST_OVERFLOW	0x8006 /* stack overflow */
#define ICE_AQ_DNL_ACT_ERR_TIMEOUT	0x8007 /* activity timeout */
#define ICE_AQ_DNL_ACT_ERR_BREAK	0x0008 /* stopped at breakpoint */
#define ICE_AQ_DNL_ACT_ERR_INVAL_ARG	0x0101 /* invalid action argument */
	__le32 execution_time; /* in nanoseconds */
	__le16 lib_ver;
	u8 psto_local_sz;
	u8 psto_global_sz;
	u8 stack_sz;
#define ICE_AQ_DNL_STACK_SZ_S		0
#define ICE_AQ_DNL_STACK_SZ_M		(0xF << ICE_AQ_DNL_STACK_SZ_S)
	u8 port_count;
#define ICE_AQ_DNL_PORT_CNT_S		0
#define ICE_AQ_DNL_PORT_CNT_M		(0x1F << ICE_AQ_DNL_PORT_CNT_S)
	__le16 act_cache_cntr;
	u32 i2c_clk_cntr;
	u32 mdio_clk_cntr;
	u32 sb_iosf_clk_cntr;
};

/* DNL run command (direct 0x0681) */
struct ice_aqc_dnl_run_command {
	u8 reserved0;
	u8 command;
#define ICE_AQ_DNL_CMD_S		0
#define ICE_AQ_DNL_CMD_M		(0x7 << ICE_AQ_DNL_CMD_S)
#define ICE_AQ_DNL_CMD_RESET		0x0
#define ICE_AQ_DNL_CMD_RUN		0x1
#define ICE_AQ_DNL_CMD_STEP		0x3
#define ICE_AQ_DNL_CMD_ABORT		0x4
#define ICE_AQ_DNL_CMD_SET_PC		0x7
#define ICE_AQ_DNL_CMD_SRC_S		3
#define ICE_AQ_DNL_CMD_SRC_M		(0x3 << ICE_AQ_DNL_CMD_SRC_S)
#define ICE_AQ_DNL_CMD_SRC_DNL		0x0
#define ICE_AQ_DNL_CMD_SRC_SCRATCH	0x1
	__le16 new_pc;
	u8 reserved1[12];
};

/* DNL call command (indirect 0x0682)
 * Struct is used for both command and response
 */
struct ice_aqc_dnl_call_command {
	u8 ctx; /* Used in command, reserved in response */
	u8 reserved;
	__le16 activity_id;
#define ICE_AQC_ACT_ID_DNL	0x1129
	__le32 reserved1;
	__le32 addr_high;
	__le32 addr_low;
};

struct ice_aqc_dnl_equa_param {
	__le16 data_in;
#define ICE_AQC_RX_EQU_SHIFT	8
#define ICE_AQC_RX_EQU_PRE2	(0x10 << ICE_AQC_RX_EQU_SHIFT)
#define ICE_AQC_RX_EQU_PRE1	(0x11 << ICE_AQC_RX_EQU_SHIFT)
#define ICE_AQC_RX_EQU_POST1	(0x12 << ICE_AQC_RX_EQU_SHIFT)
#define ICE_AQC_RX_EQU_BFLF	(0x13 << ICE_AQC_RX_EQU_SHIFT)
#define ICE_AQC_RX_EQU_BFHF	(0x14 << ICE_AQC_RX_EQU_SHIFT)
#define ICE_AQC_RX_EQU_DRATE	(0x15 << ICE_AQC_RX_EQU_SHIFT)
#define ICE_AQC_TX_EQU_PRE1	0x0
#define ICE_AQC_TX_EQU_PRE3	0x3
#define ICE_AQC_TX_EQU_ATTEN	0x4
#define ICE_AQC_TX_EQU_POST1	0x8
#define ICE_AQC_TX_EQU_PRE2	0xC
	__le16 op_code_serdes_sel;
#define ICE_AQC_OP_CODE_SHIFT 	4
#define ICE_AQC_OP_CODE_RX_EQU	(0x9 << ICE_AQC_OP_CODE_SHIFT)
#define ICE_AQC_OP_CODE_TX_EQU	(0x10 << ICE_AQC_OP_CODE_SHIFT)
	__le32 reserved[3];
};

struct ice_aqc_dnl_equa_resp {
	/* Equalization value can be -ve */
	int val;
	__le32 reserved[3];
};

/* DNL call command/response buffer (indirect 0x0682) */
struct ice_aqc_dnl_call {
	union {
		struct ice_aqc_dnl_equa_param txrx_equa_reqs;
		__le32 stores[4];
		struct ice_aqc_dnl_equa_resp txrx_equa_resp;
	} sto;
};

/* Used for both commands:
 * DNL read sto command (indirect 0x0683)
 * DNL write sto command (indirect 0x0684)
 */
struct ice_aqc_dnl_read_write_command {
	u8 ctx;
	u8 sto_sel; /* STORE select */
#define ICE_AQC_DNL_STORE_SELECT_STORE	0x0
#define ICE_AQC_DNL_STORE_SELECT_PSTO	0x1
#define ICE_AQC_DNL_STORE_SELECT_STACK	0x2
	__le16 offset;
	__le32 data; /* Used for write sto only */
	__le32 addr_high; /* Used for read sto only */
	__le32 addr_low; /* Used for read sto only */
};

/* Used for both command responses:
 * DNL read sto response (indirect 0x0683)
 * DNL write sto response (indirect 0x0684)
 */
struct ice_aqc_dnl_read_write_response {
	u8 reserved;
	u8 status; /* Reserved for read command */
	__le16 size; /* Reserved for write command */
	__le32 data; /* Reserved for write command */
	__le32 addr_high; /* Reserved for write command */
	__le32 addr_low; /* Reserved for write command */
};

/* DNL set breakpoints command (indirect 0x0686) */
struct ice_aqc_dnl_set_breakpoints_command {
	__le32 reserved[2];
	__le32 addr_high;
	__le32 addr_low;
};

/* DNL set breakpoints data buffer structure (indirect 0x0686) */
struct ice_aqc_dnl_set_breakpoints {
	u8 ctx;
	u8 ena; /* 0- disabled, 1- enabled */
	__le16 offset;
	__le16 activity_id;
};

/* DNL read log data command(indirect 0x0687) */
struct ice_aqc_dnl_read_log_command {
	__le16 reserved0;
	__le16 offset;
	__le32 reserved1;
	__le32 addr_high;
	__le32 addr_low;

};

/* DNL read log data response(indirect 0x0687) */
struct ice_aqc_dnl_read_log_response {
	__le16 reserved;
	__le16 size;
	__le32 data;
	__le32 addr_high;
	__le32 addr_low;

};

struct ice_aqc_link_topo_params {
	u8 lport_num;
	u8 lport_num_valid;
#define ICE_AQC_LINK_TOPO_PORT_NUM_VALID	BIT(0)
	u8 node_type_ctx;
#define ICE_AQC_LINK_TOPO_NODE_TYPE_S		0
#define ICE_AQC_LINK_TOPO_NODE_TYPE_M	(0xF << ICE_AQC_LINK_TOPO_NODE_TYPE_S)
#define ICE_AQC_LINK_TOPO_NODE_TYPE_PHY		0
#define ICE_AQC_LINK_TOPO_NODE_TYPE_GPIO_CTRL	1
#define ICE_AQC_LINK_TOPO_NODE_TYPE_MUX_CTRL	2
#define ICE_AQC_LINK_TOPO_NODE_TYPE_LED_CTRL	3
#define ICE_AQC_LINK_TOPO_NODE_TYPE_LED		4
#define ICE_AQC_LINK_TOPO_NODE_TYPE_THERMAL	5
#define ICE_AQC_LINK_TOPO_NODE_TYPE_CAGE	6
#define ICE_AQC_LINK_TOPO_NODE_TYPE_MEZZ	7
#define ICE_AQC_LINK_TOPO_NODE_TYPE_ID_EEPROM	8
#define ICE_AQC_LINK_TOPO_NODE_CTX_S		4
#define ICE_AQC_LINK_TOPO_NODE_CTX_M		\
				(0xF << ICE_AQC_LINK_TOPO_NODE_CTX_S)
#define ICE_AQC_LINK_TOPO_NODE_CTX_GLOBAL	0
#define ICE_AQC_LINK_TOPO_NODE_CTX_BOARD	1
#define ICE_AQC_LINK_TOPO_NODE_CTX_PORT		2
#define ICE_AQC_LINK_TOPO_NODE_CTX_NODE		3
#define ICE_AQC_LINK_TOPO_NODE_CTX_PROVIDED	4
#define ICE_AQC_LINK_TOPO_NODE_CTX_OVERRIDE	5
	u8 index;
};

struct ice_aqc_link_topo_addr {
	struct ice_aqc_link_topo_params topo_params;
	__le16 handle;
#define ICE_AQC_LINK_TOPO_HANDLE_S	0
#define ICE_AQC_LINK_TOPO_HANDLE_M	(0x3FF << ICE_AQC_LINK_TOPO_HANDLE_S)
/* Used to decode the handle field */
#define ICE_AQC_LINK_TOPO_HANDLE_BRD_TYPE_M	BIT(9)
#define ICE_AQC_LINK_TOPO_HANDLE_BRD_TYPE_LOM	0
#define ICE_AQC_LINK_TOPO_HANDLE_BRD_TYPE_MEZZ	BIT(9)
#define ICE_AQC_LINK_TOPO_HANDLE_NODE_S		0
/* In case of a Mezzanine type */
#define ICE_AQC_LINK_TOPO_HANDLE_MEZZ_NODE_M	\
				(0x3F << ICE_AQC_LINK_TOPO_HANDLE_NODE_S)
#define ICE_AQC_LINK_TOPO_HANDLE_MEZZ_S	6
#define ICE_AQC_LINK_TOPO_HANDLE_MEZZ_M	(0x7 << ICE_AQC_LINK_TOPO_HANDLE_MEZZ_S)
/* In case of a LOM type */
#define ICE_AQC_LINK_TOPO_HANDLE_LOM_NODE_M	\
				(0x1FF << ICE_AQC_LINK_TOPO_HANDLE_NODE_S)
};

/* Get Link Topology Handle (direct, 0x06E0) */
struct ice_aqc_get_link_topo {
	struct ice_aqc_link_topo_addr addr;
	u8 node_part_num;
#define ICE_AQC_GET_LINK_TOPO_NODE_NR_PCA9575			0x21
	u8 rsvd[9];
};

/* Read/Write I2C (direct, 0x06E2/0x06E3) */
struct ice_aqc_i2c {
	struct ice_aqc_link_topo_addr topo_addr;
	__le16 i2c_addr;
	u8 i2c_params;
#define ICE_AQC_I2C_DATA_SIZE_S		0
#define ICE_AQC_I2C_DATA_SIZE_M		(0xF << ICE_AQC_I2C_DATA_SIZE_S)
#define ICE_AQC_I2C_ADDR_TYPE_M		BIT(4)
#define ICE_AQC_I2C_ADDR_TYPE_7BIT	0
#define ICE_AQC_I2C_ADDR_TYPE_10BIT	ICE_AQC_I2C_ADDR_TYPE_M
#define ICE_AQC_I2C_DATA_OFFSET_S	5
#define ICE_AQC_I2C_DATA_OFFSET_M	(0x3 << ICE_AQC_I2C_DATA_OFFSET_S)
#define ICE_AQC_I2C_USE_REPEATED_START	BIT(7)
	u8 rsvd;
	__le16 i2c_bus_addr;
#define ICE_AQC_I2C_ADDR_7BIT_MASK	0x7F
#define ICE_AQC_I2C_ADDR_10BIT_MASK	0x3FF
	u8 i2c_data[4]; /* Used only by write command, reserved in read. */
};

/* Read I2C Response (direct, 0x06E2) */
struct ice_aqc_read_i2c_resp {
	u8 i2c_data[16];
};

/* Read/Write MDIO (direct, 0x06E4/0x06E5) */
struct ice_aqc_mdio {
	struct ice_aqc_link_topo_addr topo_addr;
	u8 mdio_device_addr;
#define ICE_AQC_MDIO_DEV_S	0
#define ICE_AQC_MDIO_DEV_M	(0x1F << ICE_AQC_MDIO_DEV_S)
#define ICE_AQC_MDIO_CLAUSE_22	BIT(5)
#define ICE_AQC_MDIO_CLAUSE_45	BIT(6)
	u8 mdio_bus_address;
#define ICE_AQC_MDIO_BUS_ADDR_S 0
#define ICE_AQC_MDIO_BUS_ADDR_M (0x1F << ICE_AQC_MDIO_BUS_ADDR_S)
	__le16 offset;
	__le16 data; /* Input in write cmd, output in read cmd. */
	u8 rsvd1[4];
};

/* Set/Get GPIO By Function (direct, 0x06E6/0x06E7) */
struct ice_aqc_gpio_by_func {
	struct ice_aqc_link_topo_addr topo_addr;
	u8 io_func_num;
#define ICE_AQC_GPIO_FUNC_S	0
#define ICE_AQC_GPIO_FUNC_M	(0x1F << ICE_AQC_GPIO_IO_FUNC_NUM_S)
	u8 io_value; /* Input in write cmd, output in read cmd. */
#define ICE_AQC_GPIO_ON		BIT(0)
#define ICE_AQC_GPIO_OFF	0
	u8 rsvd[8];
};

/* Set LED (direct, 0x06E8) */
struct ice_aqc_set_led {
	struct ice_aqc_link_topo_addr topo_addr;
	u8 color_and_blink;
#define ICE_AQC_LED_COLOR_S		0
#define ICE_AQC_LED_COLOR_M		(0x7 << ICE_AQC_LED_COLOR_S)
#define ICE_AQC_LED_COLOR_SKIP		0
#define ICE_AQC_LED_COLOR_RED		1
#define ICE_AQC_LED_COLOR_ORANGE	2
#define ICE_AQC_LED_COLOR_YELLOW	3
#define ICE_AQC_LED_COLOR_GREEN		4
#define ICE_AQC_LED_COLOR_BLUE		5
#define ICE_AQC_LED_COLOR_PURPLE	6
#define ICE_AQC_LED_BLINK_S		3
#define ICE_AQC_LED_BLINK_M		(0x7 << ICE_AQC_LED_BLINK_S)
#define ICE_AQC_LED_BLINK_NONE		0
#define ICE_AQC_LED_BLINK_SLOW		1
#define ICE_AQC_LED_BLINK_SLOW_MAC	2
#define ICE_AQC_LED_BLINK_SLOW_FLTR	3
#define ICE_AQC_LED_BLINK_FAST		5
#define ICE_AQC_LED_BLINK_FAST_MAC	6
#define ICE_AQC_LED_BLINK_FAST_FLTR	7
	u8 rsvd[9];
};

/* Set Port Identification LED (direct, 0x06E9) */
struct ice_aqc_set_port_id_led {
	u8 lport_num;
	u8 lport_num_valid;
#define ICE_AQC_PORT_ID_PORT_NUM_VALID	BIT(0)
	u8 ident_mode;
#define ICE_AQC_PORT_IDENT_LED_BLINK	BIT(0)
#define ICE_AQC_PORT_IDENT_LED_ORIG	0
	u8 rsvd[13];
};

/* Get Port Options (indirect, 0x06EA) */
struct ice_aqc_get_port_options {
	u8 lport_num;
	u8 lport_num_valid;
#define ICE_AQC_PORT_OPT_PORT_NUM_VALID	BIT(0)
	u8 port_options_count;
#define ICE_AQC_PORT_OPT_COUNT_S	0
#define ICE_AQC_PORT_OPT_COUNT_M	(0xF << ICE_AQC_PORT_OPT_COUNT_S)
#define ICE_AQC_PORT_OPT_MAX		16
	u8 innermost_phy_index;
	u8 port_options;
#define ICE_AQC_PORT_OPT_ACTIVE_S	0
#define ICE_AQC_PORT_OPT_ACTIVE_M	(0xF << ICE_AQC_PORT_OPT_ACTIVE_S)
#define ICE_AQC_PORT_OPT_FORCED		BIT(6)
#define ICE_AQC_PORT_OPT_VALID		BIT(7)
	u8 pending_port_option_status;
#define ICE_AQC_PENDING_PORT_OPT_IDX_S	0
#define ICE_AQC_PENDING_PORT_OPT_IDX_M	(0xF << ICE_AQC_PENDING_PORT_OPT_IDX_S)
#define ICE_AQC_PENDING_PORT_OPT_VALID	BIT(7)
	u8 rsvd[2];
	__le32 addr_high;
	__le32 addr_low;
};

struct ice_aqc_get_port_options_elem {
	u8 pmd;
#define ICE_AQC_PORT_INV_PORT_OPT	4
#define ICE_AQC_PORT_OPT_PMD_COUNT_S	0
#define ICE_AQC_PORT_OPT_PMD_COUNT_M	(0xF << ICE_AQC_PORT_OPT_PMD_COUNT_S)
#define ICE_AQC_PORT_OPT_PMD_WIDTH_S	4
#define ICE_AQC_PORT_OPT_PMD_WIDTH_M	(0xF << ICE_AQC_PORT_OPT_PMD_WIDTH_S)
	u8 max_lane_speed;
#define ICE_AQC_PORT_OPT_MAX_LANE_S	0
#define ICE_AQC_PORT_OPT_MAX_LANE_M	(0xF << ICE_AQC_PORT_OPT_MAX_LANE_S)
#define ICE_AQC_PORT_OPT_MAX_LANE_100M	0
#define ICE_AQC_PORT_OPT_MAX_LANE_1G	1
#define ICE_AQC_PORT_OPT_MAX_LANE_2500M	2
#define ICE_AQC_PORT_OPT_MAX_LANE_5G	3
#define ICE_AQC_PORT_OPT_MAX_LANE_10G	4
#define ICE_AQC_PORT_OPT_MAX_LANE_25G	5
#define ICE_AQC_PORT_OPT_MAX_LANE_50G	6
#define ICE_AQC_PORT_OPT_MAX_LANE_100G	7
#define ICE_AQC_PORT_OPT_MAX_LANE_200G	8
	u8 global_scid[2];
	u8 phy_scid[2];
	u8 pf2port_cid[2];
};

/* Set Port Option (direct, 0x06EB) */
struct ice_aqc_set_port_option {
	u8 lport_num;
	u8 lport_num_valid;
#define ICE_AQC_SET_PORT_OPT_PORT_NUM_VALID	BIT(0)
	u8 selected_port_option;
	u8 rsvd[13];
};

/* Set/Get GPIO (direct, 0x06EC/0x06ED) */
struct ice_aqc_gpio {
	__le16 gpio_ctrl_handle;
#define ICE_AQC_GPIO_HANDLE_S	0
#define ICE_AQC_GPIO_HANDLE_M	(0x3FF << ICE_AQC_GPIO_HANDLE_S)
	u8 gpio_num;
	u8 gpio_val;
	u8 rsvd[12];
};

/* Read/Write SFF EEPROM command (indirect 0x06EE) */
struct ice_aqc_sff_eeprom {
	u8 lport_num;
	u8 lport_num_valid;
#define ICE_AQC_SFF_PORT_NUM_VALID	BIT(0)
	__le16 i2c_bus_addr;
#define ICE_AQC_SFF_I2CBUS_7BIT_M	0x7F
#define ICE_AQC_SFF_I2CBUS_10BIT_M	0x3FF
#define ICE_AQC_SFF_I2CBUS_TYPE_M	BIT(10)
#define ICE_AQC_SFF_I2CBUS_TYPE_7BIT	0
#define ICE_AQC_SFF_I2CBUS_TYPE_10BIT	ICE_AQC_SFF_I2CBUS_TYPE_M
#define ICE_AQC_SFF_SET_EEPROM_PAGE_S	11
#define ICE_AQC_SFF_SET_EEPROM_PAGE_M	(0x3 << ICE_AQC_SFF_SET_EEPROM_PAGE_S)
#define ICE_AQC_SFF_NO_PAGE_CHANGE	0
#define ICE_AQC_SFF_SET_23_ON_MISMATCH	1
#define ICE_AQC_SFF_SET_22_ON_MISMATCH	2
#define ICE_AQC_SFF_IS_WRITE		BIT(15)
	__le16 i2c_mem_addr;
	__le16 eeprom_page;
#define  ICE_AQC_SFF_EEPROM_BANK_S 0
#define  ICE_AQC_SFF_EEPROM_BANK_M (0xFF << ICE_AQC_SFF_EEPROM_BANK_S)
#define  ICE_AQC_SFF_EEPROM_PAGE_S 8
#define  ICE_AQC_SFF_EEPROM_PAGE_M (0xFF << ICE_AQC_SFF_EEPROM_PAGE_S)
	__le32 addr_high;
	__le32 addr_low;
};

/* SW Set GPIO command (indirect 0x6EF)
 * SW Get GPIO command (indirect 0x6F0)
 */
struct ice_aqc_sw_gpio {
	__le16 gpio_ctrl_handle;
#define ICE_AQC_SW_GPIO_CONTROLLER_HANDLE_S	0
#define ICE_AQC_SW_GPIO_CONTROLLER_HANDLE_M	(0x3FF << ICE_AQC_SW_GPIO_CONTROLLER_HANDLE_S)
	u8 gpio_num;
#define ICE_AQC_SW_GPIO_NUMBER_S	0
#define ICE_AQC_SW_GPIO_NUMBER_M	(0x1F << ICE_AQC_SW_GPIO_NUMBER_S)
	u8 gpio_params;
#define ICE_AQC_SW_GPIO_PARAMS_DIRECTION    BIT(1)
#define ICE_AQC_SW_GPIO_PARAMS_VALUE        BIT(0)
	u8 rsvd[12];
};

/* Program Topology Device NVM (direct, 0x06F2) */
struct ice_aqc_prog_topo_dev_nvm {
	struct ice_aqc_link_topo_params topo_params;
	u8 rsvd[12];
};

/* Read Topology Device NVM (direct, 0x06F3) */
struct ice_aqc_read_topo_dev_nvm {
	struct ice_aqc_link_topo_params topo_params;
	__le32 start_address;
#define ICE_AQC_READ_TOPO_DEV_NVM_DATA_READ_SIZE 8
	u8 data_read[ICE_AQC_READ_TOPO_DEV_NVM_DATA_READ_SIZE];
};

/* NVM Read command (indirect 0x0701)
 * NVM Erase commands (direct 0x0702)
 * NVM Write commands (indirect 0x0703)
 * NVM Write Activate commands (direct 0x0707)
 * NVM Shadow RAM Dump commands (direct 0x0707)
 */
struct ice_aqc_nvm {
#define ICE_AQC_NVM_MAX_OFFSET		0xFFFFFF
	__le16 offset_low;
	u8 offset_high; /* For Write Activate offset_high is used as flags2 */
	u8 cmd_flags;
#define ICE_AQC_NVM_LAST_CMD		BIT(0)
#define ICE_AQC_NVM_PCIR_REQ		BIT(0)	/* Used by NVM Write reply */
#define ICE_AQC_NVM_PRESERVATION_S	1 /* Used by NVM Write Activate only */
#define ICE_AQC_NVM_PRESERVATION_M	(3 << ICE_AQC_NVM_PRESERVATION_S)
#define ICE_AQC_NVM_NO_PRESERVATION	(0 << ICE_AQC_NVM_PRESERVATION_S)
#define ICE_AQC_NVM_PRESERVE_ALL	BIT(1)
#define ICE_AQC_NVM_FACTORY_DEFAULT	(2 << ICE_AQC_NVM_PRESERVATION_S)
#define ICE_AQC_NVM_PRESERVE_SELECTED	(3 << ICE_AQC_NVM_PRESERVATION_S)
#define ICE_AQC_NVM_ACTIV_SEL_NVM	BIT(3) /* Write Activate/SR Dump only */
#define ICE_AQC_NVM_ACTIV_SEL_OROM	BIT(4)
#define ICE_AQC_NVM_ACTIV_SEL_NETLIST	BIT(5)
#define ICE_AQC_NVM_SPECIAL_UPDATE	BIT(6)
#define ICE_AQC_NVM_REVERT_LAST_ACTIV	BIT(6) /* Write Activate only */
#define ICE_AQC_NVM_ACTIV_SEL_MASK	MAKEMASK(0x7, 3)
#define ICE_AQC_NVM_FLASH_ONLY		BIT(7)
#define ICE_AQC_NVM_RESET_LVL_M		MAKEMASK(0x3, 0) /* Write reply only */
#define ICE_AQC_NVM_POR_FLAG		0
#define ICE_AQC_NVM_PERST_FLAG		1
#define ICE_AQC_NVM_EMPR_FLAG		2
#define ICE_AQC_NVM_EMPR_ENA		BIT(0) /* Write Activate reply only */
	/* For Write Activate, several flags are sent as part of a separate
	 * flags2 field using a separate byte. For simplicity of the software
	 * interface, we pass the flags as a 16 bit value so these flags are
	 * all offset by 8 bits
	 */
#define ICE_AQC_NVM_ACTIV_REQ_EMPR	BIT(8) /* NVM Write Activate only */
	__le16 module_typeid;
	__le16 length;
#define ICE_AQC_NVM_ERASE_LEN	0xFFFF
	__le32 addr_high;
	__le32 addr_low;
};

/* NVM Module_Type ID, needed offset and read_len for struct ice_aqc_nvm. */
#define ICE_AQC_NVM_SECTOR_UNIT			4096 /* In Bytes */
#define ICE_AQC_NVM_WORD_UNIT			2 /* In Bytes */

#define ICE_AQC_NVM_START_POINT			0
#define ICE_AQC_NVM_EMP_SR_PTR_OFFSET		0x90
#define ICE_AQC_NVM_EMP_SR_PTR_RD_LEN		2 /* In Bytes */
#define ICE_AQC_NVM_EMP_SR_PTR_M		MAKEMASK(0x7FFF, 0)
#define ICE_AQC_NVM_EMP_SR_PTR_TYPE_S		15
#define ICE_AQC_NVM_EMP_SR_PTR_TYPE_M		BIT(15)
#define ICE_AQC_NVM_EMP_SR_PTR_TYPE_SECTOR	1

#define ICE_AQC_NVM_LLDP_CFG_PTR_OFFSET		0x46
#define ICE_AQC_NVM_LLDP_CFG_HEADER_LEN		2 /* In Bytes */
#define ICE_AQC_NVM_LLDP_CFG_PTR_RD_LEN		2 /* In Bytes */

#define ICE_AQC_NVM_LLDP_PRESERVED_MOD_ID	0x129
#define ICE_AQC_NVM_CUR_LLDP_PERSIST_RD_OFFSET	2 /* In Bytes */
#define ICE_AQC_NVM_LLDP_STATUS_M		MAKEMASK(0xF, 0)
#define ICE_AQC_NVM_LLDP_STATUS_M_LEN		4 /* In Bits */
#define ICE_AQC_NVM_LLDP_STATUS_RD_LEN		4 /* In Bytes */

#define ICE_AQC_NVM_SDP_CFG_PTR_OFFSET		0xD8
#define ICE_AQC_NVM_SDP_CFG_PTR_RD_LEN		2 /* In Bytes */
#define ICE_AQC_NVM_SDP_CFG_PTR_M		MAKEMASK(0x7FFF, 0)
#define ICE_AQC_NVM_SDP_CFG_PTR_TYPE_M		BIT(15)
#define ICE_AQC_NVM_SDP_CFG_HEADER_LEN		2 /* In Bytes */
#define ICE_AQC_NVM_SDP_CFG_SEC_LEN_LEN		2 /* In Bytes */
#define ICE_AQC_NVM_SDP_CFG_DATA_LEN		14 /* In Bytes */
#define ICE_AQC_NVM_SDP_CFG_MAX_SECTION_SIZE	7
#define ICE_AQC_NVM_SDP_CFG_PIN_SIZE		10
#define ICE_AQC_NVM_SDP_CFG_PIN_OFFSET		6
#define ICE_AQC_NVM_SDP_CFG_PIN_MASK		MAKEMASK(0x3FF, \
						ICE_AQC_NVM_SDP_CFG_PIN_OFFSET)
#define ICE_AQC_NVM_SDP_CFG_CHAN_OFFSET		4
#define ICE_AQC_NVM_SDP_CFG_CHAN_MASK		MAKEMASK(0x3, \
						ICE_AQC_NVM_SDP_CFG_CHAN_OFFSET)
#define ICE_AQC_NVM_SDP_CFG_DIR_OFFSET		3
#define ICE_AQC_NVM_SDP_CFG_DIR_MASK		MAKEMASK(0x1, \
						ICE_AQC_NVM_SDP_CFG_DIR_OFFSET)
#define ICE_AQC_NVM_SDP_CFG_SDP_NUM_OFFSET		0
#define ICE_AQC_NVM_SDP_CFG_SDP_NUM_MASK	MAKEMASK(0x7, \
					     ICE_AQC_NVM_SDP_CFG_SDP_NUM_OFFSET)
#define ICE_AQC_NVM_SDP_CFG_NA_PIN_MASK		MAKEMASK(0x1, 15)

#define ICE_AQC_NVM_MINSREV_MOD_ID		0x130
#define ICE_AQC_NVM_TX_TOPO_MOD_ID		0x14B
#define ICE_AQC_NVM_CMPO_MOD_ID			0x153

/* Cage Max Power override NVM module */
struct ice_aqc_nvm_cmpo {
	__le16 length;
#define ICE_AQC_NVM_CMPO_ENABLE	BIT(8)
	__le16 cages_cfg[8];
};

/* Used for reading and writing MinSRev using 0x0701 and 0x0703. Note that the
 * type field is excluded from the section when reading and writing from
 * a module using the module_typeid field with these AQ commands.
 */
struct ice_aqc_nvm_minsrev {
	__le16 length;
	__le16 validity;
#define ICE_AQC_NVM_MINSREV_NVM_VALID		BIT(0)
#define ICE_AQC_NVM_MINSREV_OROM_VALID		BIT(1)
	__le16 nvm_minsrev_l;
	__le16 nvm_minsrev_h;
	__le16 orom_minsrev_l;
	__le16 orom_minsrev_h;
};

struct ice_aqc_nvm_tx_topo_user_sel {
	__le16 length;
	u8 data;
#define ICE_AQC_NVM_TX_TOPO_USER_SEL		BIT(4)
	u8 reserved;
};

/* Used for 0x0704 as well as for 0x0705 commands */
struct ice_aqc_nvm_cfg {
	u8	cmd_flags;
#define ICE_AQC_ANVM_MULTIPLE_ELEMS	BIT(0)
#define ICE_AQC_ANVM_IMMEDIATE_FIELD	BIT(1)
#define ICE_AQC_ANVM_NEW_CFG		BIT(2)
	u8	reserved;
	__le16 count;
	__le16 id;
	u8 reserved1[2];
	__le32 addr_high;
	__le32 addr_low;
};

struct ice_aqc_nvm_cfg_data {
	__le16 field_id;
	__le16 field_options;
	__le16 field_value;
};

/* NVM Checksum Command (direct, 0x0706) */
struct ice_aqc_nvm_checksum {
	u8 flags;
#define ICE_AQC_NVM_CHECKSUM_VERIFY	BIT(0)
#define ICE_AQC_NVM_CHECKSUM_RECALC	BIT(1)
	u8 rsvd;
	__le16 checksum; /* Used only by response */
#define ICE_AQC_NVM_CHECKSUM_CORRECT	0xBABA
	u8 rsvd2[12];
};

/* Used for NVM Sanitization command - 0x070C */
struct ice_aqc_nvm_sanitization {
	u8 cmd_flags;
#define ICE_AQ_NVM_SANITIZE_REQ_READ				0
#define ICE_AQ_NVM_SANITIZE_REQ_OPERATE				BIT(0)

#define ICE_AQ_NVM_SANITIZE_READ_SUBJECT_NVM_BITS		0
#define ICE_AQ_NVM_SANITIZE_READ_SUBJECT_NVM_STATE		BIT(1)
#define ICE_AQ_NVM_SANITIZE_OPERATE_SUBJECT_CLEAR		0
	u8 values;
#define ICE_AQ_NVM_SANITIZE_NVM_BITS_HOST_CLEAN_SUPPORT		BIT(0)
#define ICE_AQ_NVM_SANITIZE_NVM_BITS_BMC_CLEAN_SUPPORT		BIT(2)
#define ICE_AQ_NVM_SANITIZE_NVM_STATE_HOST_CLEAN_DONE		BIT(0)
#define ICE_AQ_NVM_SANITIZE_NVM_STATE_HOST_CLEAN_SUCCESS	BIT(1)
#define ICE_AQ_NVM_SANITIZE_NVM_STATE_BMC_CLEAN_DONE		BIT(2)
#define ICE_AQ_NVM_SANITIZE_NVM_STATE_BMC_CLEAN_SUCCESS		BIT(3)
#define ICE_AQ_NVM_SANITIZE_OPERATE_HOST_CLEAN_DONE		BIT(0)
#define ICE_AQ_NVM_SANITIZE_OPERATE_HOST_CLEAN_SUCCESS		BIT(1)
#define ICE_AQ_NVM_SANITIZE_OPERATE_BMC_CLEAN_DONE		BIT(2)
#define ICE_AQ_NVM_SANITIZE_OPERATE_BMC_CLEAN_SUCCESS		BIT(3)
	u8 reserved[14];
};

/*
 * Send to PF command (indirect 0x0801) ID is only used by PF
 *
 * Send to VF command (indirect 0x0802) ID is only used by PF
 *
 */
struct ice_aqc_pf_vf_msg {
	__le32 id;
	u32 reserved;
	__le32 addr_high;
	__le32 addr_low;
};

/* Write/Read Alternate - Direct (direct 0x0900/0x0902) */
struct ice_aqc_read_write_alt_direct {
	__le32 dword0_addr;
	__le32 dword0_value;
	__le32 dword1_addr;
	__le32 dword1_value;
};

/* Write/Read Alternate - Indirect (indirect 0x0901/0x0903) */
struct ice_aqc_read_write_alt_indirect {
	__le32 base_dword_addr;
	__le32 num_dwords;
	__le32 addr_high;
	__le32 addr_low;
};

/* Done Alternate Write (direct 0x0904) */
struct ice_aqc_done_alt_write {
	u8 flags;
#define ICE_AQC_CMD_UEFI_BIOS_MODE	BIT(0)
#define ICE_AQC_RESP_RESET_NEEDED	BIT(1)
	u8 reserved[15];
};

/* Clear Port Alternate Write (direct 0x0906) */
struct ice_aqc_clear_port_alt_write {
	u8 reserved[16];
};

/* Get LLDP MIB (indirect 0x0A00)
 * Note: This is also used by the LLDP MIB Change Event (0x0A01)
 * as the format is the same.
 */
struct ice_aqc_lldp_get_mib {
	u8 type;
#define ICE_AQ_LLDP_MIB_TYPE_S			0
#define ICE_AQ_LLDP_MIB_TYPE_M			(0x3 << ICE_AQ_LLDP_MIB_TYPE_S)
#define ICE_AQ_LLDP_MIB_LOCAL			0
#define ICE_AQ_LLDP_MIB_REMOTE			1
#define ICE_AQ_LLDP_MIB_LOCAL_AND_REMOTE	2
#define ICE_AQ_LLDP_BRID_TYPE_S			2
#define ICE_AQ_LLDP_BRID_TYPE_M			(0x3 << ICE_AQ_LLDP_BRID_TYPE_S)
#define ICE_AQ_LLDP_BRID_TYPE_NEAREST_BRID	0
#define ICE_AQ_LLDP_BRID_TYPE_NON_TPMR		1
/* Tx pause flags in the 0xA01 event use ICE_AQ_LLDP_TX_* */
#define ICE_AQ_LLDP_TX_S			0x4
#define ICE_AQ_LLDP_TX_M			(0x03 << ICE_AQ_LLDP_TX_S)
#define ICE_AQ_LLDP_TX_ACTIVE			0
#define ICE_AQ_LLDP_TX_SUSPENDED		1
#define ICE_AQ_LLDP_TX_FLUSHED			3
/* DCBX mode */
#define ICE_AQ_LLDP_DCBX_S			6
#define ICE_AQ_LLDP_DCBX_M			(0x3 << ICE_AQ_LLDP_DCBX_S)
#define ICE_AQ_LLDP_DCBX_NA			0
#define ICE_AQ_LLDP_DCBX_CEE			1
#define ICE_AQ_LLDP_DCBX_IEEE			2
/* The following bytes are reserved for the Get LLDP MIB command (0x0A00)
 * and in the LLDP MIB Change Event (0x0A01). They are valid for the
 * Get LLDP MIB (0x0A00) response only.
 */
	u8 state;
#define ICE_AQ_LLDP_MIB_CHANGE_STATE_S		0
#define ICE_AQ_LLDP_MIB_CHANGE_STATE_M		\
				(0x1 << ICE_AQ_LLDP_MIB_CHANGE_STATE_S)
#define ICE_AQ_LLDP_MIB_CHANGE_EXECUTED		0
#define ICE_AQ_LLDP_MIB_CHANGE_PENDING		1
	__le16 local_len;
	__le16 remote_len;
	u8 reserved[2];
	__le32 addr_high;
	__le32 addr_low;
};

/* Configure LLDP MIB Change Event (direct 0x0A01) */
/* For MIB Change Event use ice_aqc_lldp_get_mib structure above */
struct ice_aqc_lldp_set_mib_change {
	u8 command;
#define ICE_AQ_LLDP_MIB_UPDATE_ENABLE		0x0
#define ICE_AQ_LLDP_MIB_UPDATE_DIS		0x1
#define ICE_AQ_LLDP_MIB_PENDING_S		1
#define ICE_AQ_LLDP_MIB_PENDING_M		\
				(0x1 << ICE_AQ_LLDP_MIB_PENDING_S)
#define ICE_AQ_LLDP_MIB_PENDING_DISABLE		0
#define ICE_AQ_LLDP_MIB_PENDING_ENABLE		1
	u8 reserved[15];
};

/* Add LLDP TLV (indirect 0x0A02)
 * Delete LLDP TLV (indirect 0x0A04)
 */
struct ice_aqc_lldp_add_delete_tlv {
	u8 type; /* only nearest bridge and non-TPMR from 0x0A00 */
	u8 reserved1[1];
	__le16 len;
	u8 reserved2[4];
	__le32 addr_high;
	__le32 addr_low;
};

/* Update LLDP TLV (indirect 0x0A03) */
struct ice_aqc_lldp_update_tlv {
	u8 type; /* only nearest bridge and non-TPMR from 0x0A00 */
	u8 reserved;
	__le16 old_len;
	__le16 new_offset;
	__le16 new_len;
	__le32 addr_high;
	__le32 addr_low;
};

/* Stop LLDP (direct 0x0A05) */
struct ice_aqc_lldp_stop {
	u8 command;
#define ICE_AQ_LLDP_AGENT_STATE_MASK	BIT(0)
#define ICE_AQ_LLDP_AGENT_STOP		0x0
#define ICE_AQ_LLDP_AGENT_SHUTDOWN	ICE_AQ_LLDP_AGENT_STATE_MASK
#define ICE_AQ_LLDP_AGENT_PERSIST_DIS	BIT(1)
	u8 reserved[15];
};

/* Start LLDP (direct 0x0A06) */
struct ice_aqc_lldp_start {
	u8 command;
#define ICE_AQ_LLDP_AGENT_START		BIT(0)
#define ICE_AQ_LLDP_AGENT_PERSIST_ENA	BIT(1)
	u8 reserved[15];
};

/* Get CEE DCBX Oper Config (0x0A07)
 * The command uses the generic descriptor struct and
 * returns the struct below as an indirect response.
 */
struct ice_aqc_get_cee_dcb_cfg_resp {
	u8 oper_num_tc;
	u8 oper_prio_tc[4];
	u8 oper_tc_bw[8];
	u8 oper_pfc_en;
	__le16 oper_app_prio;
#define ICE_AQC_CEE_APP_FCOE_S		0
#define ICE_AQC_CEE_APP_FCOE_M		(0x7 << ICE_AQC_CEE_APP_FCOE_S)
#define ICE_AQC_CEE_APP_ISCSI_S		3
#define ICE_AQC_CEE_APP_ISCSI_M		(0x7 << ICE_AQC_CEE_APP_ISCSI_S)
#define ICE_AQC_CEE_APP_FIP_S		8
#define ICE_AQC_CEE_APP_FIP_M		(0x7 << ICE_AQC_CEE_APP_FIP_S)
	__le32 tlv_status;
#define ICE_AQC_CEE_PG_STATUS_S		0
#define ICE_AQC_CEE_PG_STATUS_M		(0x7 << ICE_AQC_CEE_PG_STATUS_S)
#define ICE_AQC_CEE_PFC_STATUS_S	3
#define ICE_AQC_CEE_PFC_STATUS_M	(0x7 << ICE_AQC_CEE_PFC_STATUS_S)
#define ICE_AQC_CEE_FCOE_STATUS_S	8
#define ICE_AQC_CEE_FCOE_STATUS_M	(0x7 << ICE_AQC_CEE_FCOE_STATUS_S)
#define ICE_AQC_CEE_ISCSI_STATUS_S	11
#define ICE_AQC_CEE_ISCSI_STATUS_M	(0x7 << ICE_AQC_CEE_ISCSI_STATUS_S)
#define ICE_AQC_CEE_FIP_STATUS_S	16
#define ICE_AQC_CEE_FIP_STATUS_M	(0x7 << ICE_AQC_CEE_FIP_STATUS_S)
	u8 reserved[12];
};

/* Set Local LLDP MIB (indirect 0x0A08)
 * Used to replace the local MIB of a given LLDP agent. e.g. DCBX
 */
struct ice_aqc_lldp_set_local_mib {
	u8 type;
#define SET_LOCAL_MIB_TYPE_DCBX_M		BIT(0)
#define SET_LOCAL_MIB_TYPE_LOCAL_MIB		0
#define SET_LOCAL_MIB_TYPE_CEE_M		BIT(1)
#define SET_LOCAL_MIB_TYPE_CEE_WILLING		0
#define SET_LOCAL_MIB_TYPE_CEE_NON_WILLING	SET_LOCAL_MIB_TYPE_CEE_M
	u8 reserved0;
	__le16 length;
	u8 reserved1[4];
	__le32 addr_high;
	__le32 addr_low;
};

struct ice_aqc_lldp_set_local_mib_resp {
	u8 status;
#define SET_LOCAL_MIB_RESP_EVENT_M		BIT(0)
#define SET_LOCAL_MIB_RESP_MIB_CHANGE_SILENT	0
#define SET_LOCAL_MIB_RESP_MIB_CHANGE_EVENT	SET_LOCAL_MIB_RESP_EVENT_M
	u8 reserved[15];
};

/* Stop/Start LLDP Agent (direct 0x0A09)
 * Used for stopping/starting specific LLDP agent. e.g. DCBX.
 * The same structure is used for the response, with the command field
 * being used as the status field.
 */
struct ice_aqc_lldp_stop_start_specific_agent {
	u8 command;
#define ICE_AQC_START_STOP_AGENT_M		BIT(0)
#define ICE_AQC_START_STOP_AGENT_STOP_DCBX	0
#define ICE_AQC_START_STOP_AGENT_START_DCBX	ICE_AQC_START_STOP_AGENT_M
	u8 reserved[15];
};

/* LLDP Filter Control (direct 0x0A0A) */
struct ice_aqc_lldp_filter_ctrl {
	u8 cmd_flags;
#define ICE_AQC_LLDP_FILTER_ACTION_M		MAKEMASK(3, 0)
#define ICE_AQC_LLDP_FILTER_ACTION_ADD		0x0
#define ICE_AQC_LLDP_FILTER_ACTION_DELETE	0x1
#define ICE_AQC_LLDP_FILTER_ACTION_UPDATE	0x2
	u8 reserved1;
	__le16 vsi_num;
	u8 reserved2[12];
};

/* Get/Set RSS key (indirect 0x0B04/0x0B02) */
struct ice_aqc_get_set_rss_key {
#define ICE_AQC_GSET_RSS_KEY_VSI_VALID	BIT(15)
#define ICE_AQC_GSET_RSS_KEY_VSI_ID_S	0
#define ICE_AQC_GSET_RSS_KEY_VSI_ID_M	(0x3FF << ICE_AQC_GSET_RSS_KEY_VSI_ID_S)
	__le16 vsi_id;
	u8 reserved[6];
	__le32 addr_high;
	__le32 addr_low;
};

#define ICE_AQC_GET_SET_RSS_KEY_DATA_RSS_KEY_SIZE	0x28
#define ICE_AQC_GET_SET_RSS_KEY_DATA_HASH_KEY_SIZE	0xC
#define ICE_GET_SET_RSS_KEY_EXTEND_KEY_SIZE \
				(ICE_AQC_GET_SET_RSS_KEY_DATA_RSS_KEY_SIZE + \
				 ICE_AQC_GET_SET_RSS_KEY_DATA_HASH_KEY_SIZE)

/**
 * struct ice_aqc_get_set_rss_keys - Get/Set RSS hash key command buffer
 * @standard_rss_key: 40 most significant bytes of hash key
 * @extended_hash_key: 12 least significant bytes of hash key
 *
 * Set/Get 40 byte hash key using standard_rss_key field, and set
 * extended_hash_key field to zero. Set/Get 52 byte hash key using
 * standard_rss_key field for 40 most significant bytes and the
 * extended_hash_key field for the 12 least significant bytes of hash key.
 */
struct ice_aqc_get_set_rss_keys {
	u8 standard_rss_key[ICE_AQC_GET_SET_RSS_KEY_DATA_RSS_KEY_SIZE];
	u8 extended_hash_key[ICE_AQC_GET_SET_RSS_KEY_DATA_HASH_KEY_SIZE];
};

enum ice_lut_type {
	ICE_LUT_VSI = 0,
	ICE_LUT_PF = 1,
	ICE_LUT_GLOBAL = 2,
	ICE_LUT_TYPE_MASK = 3,
	ICE_LUT_PF_SMALL = 5, /* yields ICE_LUT_PF when &= ICE_LUT_TYPE_MASK */
};

enum ice_lut_size {
	ICE_LUT_VSI_SIZE = 64,
	ICE_LUT_PF_SMALL_SIZE = 128,
	ICE_LUT_GLOBAL_SIZE = 512,
	ICE_LUT_PF_SIZE = 2048,
};

/* Get/Set RSS LUT (indirect 0x0B05/0x0B03) */
struct ice_aqc_get_set_rss_lut {
#define ICE_AQC_GSET_RSS_LUT_VSI_VALID	BIT(15)
#define ICE_AQC_GSET_RSS_LUT_VSI_ID_S	0
#define ICE_AQC_GSET_RSS_LUT_VSI_ID_M	(0x3FF << ICE_AQC_GSET_RSS_LUT_VSI_ID_S)
	__le16 vsi_id;
#define ICE_AQC_GSET_RSS_LUT_TABLE_TYPE_S	0
#define ICE_AQC_GSET_RSS_LUT_TABLE_TYPE_M	\
	(ICE_LUT_TYPE_MASK << ICE_AQC_GSET_RSS_LUT_TABLE_TYPE_S)

#define ICE_AQC_GSET_RSS_LUT_TABLE_SIZE_S	 2
#define ICE_AQC_GSET_RSS_LUT_TABLE_SIZE_M	 \
	(ICE_LUT_TYPE_MASK << ICE_AQC_GSET_RSS_LUT_TABLE_SIZE_S)

#define ICE_AQC_GSET_RSS_LUT_TABLE_SIZE_512_FLAG 1
#define ICE_AQC_GSET_RSS_LUT_TABLE_SIZE_2K_FLAG	 2

#define ICE_AQC_GSET_RSS_LUT_GLOBAL_IDX_S	 4
#define ICE_AQC_GSET_RSS_LUT_GLOBAL_IDX_M	 \
				(0xF << ICE_AQC_GSET_RSS_LUT_GLOBAL_IDX_S)

	__le16 flags;
	__le32 reserved;
	__le32 addr_high;
	__le32 addr_low;
};

/* Sideband Control Interface Commands */
/* Neighbor Device Request (indirect 0x0C00); also used for the response. */
struct ice_aqc_neigh_dev_req {
	__le16 sb_data_len;
	u8 reserved[6];
	__le32 addr_high;
	__le32 addr_low;
};

/* Add Tx LAN Queues (indirect 0x0C30) */
struct ice_aqc_add_txqs {
	u8 num_qgrps;
	u8 reserved[3];
	__le32 reserved1;
	__le32 addr_high;
	__le32 addr_low;
};

/* This is the descriptor of each queue entry for the Add Tx LAN Queues
 * command (0x0C30). Only used within struct ice_aqc_add_tx_qgrp.
 */
struct ice_aqc_add_txqs_perq {
	__le16 txq_id;
	u8 rsvd[2];
	__le32 q_teid;
	u8 txq_ctx[22];
	u8 rsvd2[2];
	struct ice_aqc_txsched_elem info;
};

/* The format of the command buffer for Add Tx LAN Queues (0x0C30)
 * is an array of the following structs. Please note that the length of
 * each struct ice_aqc_add_tx_qgrp is variable due
 * to the variable number of queues in each group!
 */
struct ice_aqc_add_tx_qgrp {
	__le32 parent_teid;
	u8 num_txqs;
	u8 rsvd[3];
	struct ice_aqc_add_txqs_perq txqs[STRUCT_HACK_VAR_LEN];
};

/* Disable Tx LAN Queues (indirect 0x0C31) */
struct ice_aqc_dis_txqs {
	u8 cmd_type;
#define ICE_AQC_Q_DIS_CMD_S		0
#define ICE_AQC_Q_DIS_CMD_M		(0x3 << ICE_AQC_Q_DIS_CMD_S)
#define ICE_AQC_Q_DIS_CMD_NO_FUNC_RESET	(0 << ICE_AQC_Q_DIS_CMD_S)
#define ICE_AQC_Q_DIS_CMD_VM_RESET	BIT(ICE_AQC_Q_DIS_CMD_S)
#define ICE_AQC_Q_DIS_CMD_VF_RESET	(2 << ICE_AQC_Q_DIS_CMD_S)
#define ICE_AQC_Q_DIS_CMD_PF_RESET	(3 << ICE_AQC_Q_DIS_CMD_S)
#define ICE_AQC_Q_DIS_CMD_SUBSEQ_CALL	BIT(2)
#define ICE_AQC_Q_DIS_CMD_FLUSH_PIPE	BIT(3)
	u8 num_entries;
	__le16 vmvf_and_timeout;
#define ICE_AQC_Q_DIS_VMVF_NUM_S	0
#define ICE_AQC_Q_DIS_VMVF_NUM_M	(0x3FF << ICE_AQC_Q_DIS_VMVF_NUM_S)
#define ICE_AQC_Q_DIS_TIMEOUT_S		10
#define ICE_AQC_Q_DIS_TIMEOUT_M		(0x3F << ICE_AQC_Q_DIS_TIMEOUT_S)
	__le32 blocked_cgds;
	__le32 addr_high;
	__le32 addr_low;
};

/* The buffer for Disable Tx LAN Queues (indirect 0x0C31)
 * contains the following structures, arrayed one after the
 * other.
 * Note: Since the q_id is 16 bits wide, if the
 * number of queues is even, then 2 bytes of alignment MUST be
 * added before the start of the next group, to allow correct
 * alignment of the parent_teid field.
 */
#pragma pack(1)
struct ice_aqc_dis_txq_item {
	__le32 parent_teid;
	u8 num_qs;
	u8 rsvd;
	/* The length of the q_id array varies according to num_qs */
#define ICE_AQC_Q_DIS_BUF_ELEM_TYPE_S		15
#define ICE_AQC_Q_DIS_BUF_ELEM_TYPE_LAN_Q	\
			(0 << ICE_AQC_Q_DIS_BUF_ELEM_TYPE_S)
#define ICE_AQC_Q_DIS_BUF_ELEM_TYPE_RDMA_QSET	\
			(1 << ICE_AQC_Q_DIS_BUF_ELEM_TYPE_S)
	__le16 q_id[STRUCT_HACK_VAR_LEN];
};
#pragma pack()

/* Tx LAN Queues Cleanup Event (0x0C31) */
struct ice_aqc_txqs_cleanup {
	__le16 caller_opc;
	__le16 cmd_tag;
	u8 reserved[12];
};

/* Move / Reconfigure Tx Queues (indirect 0x0C32) */
struct ice_aqc_move_txqs {
	u8 cmd_type;
#define ICE_AQC_Q_CMD_TYPE_S		0
#define ICE_AQC_Q_CMD_TYPE_M		(0x3 << ICE_AQC_Q_CMD_TYPE_S)
#define ICE_AQC_Q_CMD_TYPE_MOVE		1
#define ICE_AQC_Q_CMD_TYPE_TC_CHANGE	2
#define ICE_AQC_Q_CMD_TYPE_MOVE_AND_TC	3
#define ICE_AQC_Q_CMD_SUBSEQ_CALL	BIT(2)
#define ICE_AQC_Q_CMD_FLUSH_PIPE	BIT(3)
	u8 num_qs;
	u8 rsvd;
	u8 timeout;
#define ICE_AQC_Q_CMD_TIMEOUT_S		2
#define ICE_AQC_Q_CMD_TIMEOUT_M		(0x3F << ICE_AQC_Q_CMD_TIMEOUT_S)
	__le32 blocked_cgds;
	__le32 addr_high;
	__le32 addr_low;
};

/* Per-queue data buffer for the Move Tx LAN Queues command/response */
struct ice_aqc_move_txqs_elem {
	__le16 txq_id;
	u8 q_cgd;
	u8 rsvd;
	__le32 q_teid;
};

/* Indirect data buffer for the Move Tx LAN Queues command/response */
struct ice_aqc_move_txqs_data {
	__le32 src_teid;
	__le32 dest_teid;
	struct ice_aqc_move_txqs_elem txqs[STRUCT_HACK_VAR_LEN];
};

/* Add Tx RDMA Queue Set (indirect 0x0C33) */
struct ice_aqc_add_rdma_qset {
	u8 num_qset_grps;
	u8 reserved[7];
	__le32 addr_high;
	__le32 addr_low;
};

/* This is the descriptor of each qset entry for the Add Tx RDMA Queue Set
 * command (0x0C33). Only used within struct ice_aqc_add_rdma_qset.
 */
struct ice_aqc_add_tx_rdma_qset_entry {
	__le16 tx_qset_id;
	u8 rsvd[2];
	__le32 qset_teid;
	struct ice_aqc_txsched_elem info;
};

/* The format of the command buffer for Add Tx RDMA Queue Set(0x0C33)
 * is an array of the following structs. Please note that the length of
 * each struct ice_aqc_add_rdma_qset is variable due to the variable
 * number of queues in each group!
 */
struct ice_aqc_add_rdma_qset_data {
	__le32 parent_teid;
	__le16 num_qsets;
	u8 rsvd[2];
	struct ice_aqc_add_tx_rdma_qset_entry rdma_qsets[STRUCT_HACK_VAR_LEN];
};

/* Move RDMA Queue Set (indirect 0x0C34) */
struct ice_aqc_move_rdma_qset_cmd {
	u8 num_rdma_qset;	/* Used by commands and response */
#define ICE_AQC_PF_MODE_SAME_PF		0x0
#define ICE_AQC_PF_MODE_GIVE_OWNERSHIP	0x1
#define ICE_AQC_PF_MODE_KEEP_OWNERSHIP	0x2
	u8 flags;
	u8 reserved[6];
	__le32 addr_high;
	__le32 addr_low;
};

/* Buffer */
struct ice_aqc_move_rdma_qset_buffer_desc {
	__le16 tx_qset_id;
	__le16 qset_teid;
};

struct ice_aqc_move_rdma_qset_buffer {
	__le32 src_parent_teid;
	__le32 dest_parent_teid;
	struct ice_aqc_move_rdma_qset_buffer_desc descs[STRUCT_HACK_VAR_LEN];
};

/* Download Package (indirect 0x0C40) */
/* Also used for Update Package (indirect 0x0C41 and 0x0C42) */
struct ice_aqc_download_pkg {
	u8 flags;
#define ICE_AQC_DOWNLOAD_PKG_LAST_BUF	0x01
	u8 reserved[3];
	__le32 reserved1;
	__le32 addr_high;
	__le32 addr_low;
};

struct ice_aqc_download_pkg_resp {
	__le32 error_offset;
	__le32 error_info;
	__le32 addr_high;
	__le32 addr_low;
};

/* Get Package Info List (indirect 0x0C43) */
struct ice_aqc_get_pkg_info_list {
	__le32 reserved1;
	__le32 reserved2;
	__le32 addr_high;
	__le32 addr_low;
};

/* Version format for packages */
struct ice_pkg_ver {
	u8 major;
	u8 minor;
	u8 update;
	u8 draft;
};

#define ICE_PKG_NAME_SIZE	32
#define ICE_SEG_ID_SIZE		28
#define ICE_SEG_NAME_SIZE	28

struct ice_aqc_get_pkg_info {
	struct ice_pkg_ver ver;
	char name[ICE_SEG_NAME_SIZE];
	__le32 track_id;
	u8 is_in_nvm;
	u8 is_active;
	u8 is_active_at_boot;
	u8 is_modified;
};

/* Get Package Info List response buffer format (0x0C43) */
struct ice_aqc_get_pkg_info_resp {
	__le32 count;
	struct ice_aqc_get_pkg_info pkg_info[STRUCT_HACK_VAR_LEN];
};

/* Driver Shared Parameters (direct, 0x0C90) */
struct ice_aqc_driver_shared_params {
	u8 set_or_get_op;
#define ICE_AQC_DRIVER_PARAM_OP_MASK		BIT(0)
#define ICE_AQC_DRIVER_PARAM_SET		((u8)0)
#define ICE_AQC_DRIVER_PARAM_GET		((u8)1)
	u8 param_indx;
#define ICE_AQC_DRIVER_PARAM_MAX_IDX		15
	u8 rsvd[2];
	__le32 param_val;
	__le32 addr_high;
	__le32 addr_low;
};

/* Lan Queue Overflow Event (direct, 0x1001) */
struct ice_aqc_event_lan_overflow {
	__le32 prtdcb_ruptq;
	__le32 qtx_ctl;
	u8 reserved[8];
};

/* Debug Dump Internal Data (indirect 0xFF08) */
struct ice_aqc_debug_dump_internals {
	__le16 cluster_id; /* Expresses next cluster ID in response */
#define ICE_AQC_DBG_DUMP_CLUSTER_ID_SW_E810			0
#define ICE_AQC_DBG_DUMP_CLUSTER_ID_ACL_E810			1
#define ICE_AQC_DBG_DUMP_CLUSTER_ID_TXSCHED_E810		2
#define ICE_AQC_DBG_DUMP_CLUSTER_ID_PROFILES_E810		3
/* EMP_DRAM only dumpable in device debug mode */
#define ICE_AQC_DBG_DUMP_CLUSTER_ID_EMP_DRAM_E810		4
#define ICE_AQC_DBG_DUMP_CLUSTER_ID_LINK_E810			5
/* AUX_REGS only dumpable in device debug mode */
#define ICE_AQC_DBG_DUMP_CLUSTER_ID_AUX_REGS_E810		6
#define ICE_AQC_DBG_DUMP_CLUSTER_ID_DCB_E810			7
#define ICE_AQC_DBG_DUMP_CLUSTER_ID_L2P_E810			8
#define ICE_AQC_DBG_DUMP_CLUSTER_ID_QUEUE_MNG_E810		9
#define ICE_AQC_DBG_DUMP_CLUSTER_ID_FULL_CSR_SPACE_E810		21
#define ICE_AQC_DBG_DUMP_CLUSTER_ID_MNG_TRANSACTIONS_E810	22

/* Start cluster to discover first available cluster */
#define ICE_AQC_DBG_DUMP_CLUSTER_ID_START_ALL			0
#define ICE_AQC_DBG_DUMP_CLUSTER_ID_SW_E830			100
#define ICE_AQC_DBG_DUMP_CLUSTER_ID_ACL_E830			101
#define ICE_AQC_DBG_DUMP_CLUSTER_ID_TXSCHED_E830		102
#define ICE_AQC_DBG_DUMP_CLUSTER_ID_PROFILES_E830		103
/* EMP_DRAM only dumpable in device debug mode */
#define ICE_AQC_DBG_DUMP_CLUSTER_ID_LINK_E830			105
#define ICE_AQC_DBG_DUMP_CLUSTER_ID_DCB_E830			107
#define ICE_AQC_DBG_DUMP_CLUSTER_ID_L2P_E830			108
#define ICE_AQC_DBG_DUMP_CLUSTER_ID_QUEUE_MNG_E830		109
#define ICE_AQC_DBG_DUMP_CLUSTER_ID_FULL_CSR_SPACE_E830		121
	__le16 table_id; /* Used only for non-memory clusters */
	__le32 idx; /* In table entries for tables, in bytes for memory */
	__le32 addr_high;
	__le32 addr_low;
};

enum ice_aqc_fw_logging_mod {
	ICE_AQC_FW_LOG_ID_GENERAL = 0,
	ICE_AQC_FW_LOG_ID_CTRL,
	ICE_AQC_FW_LOG_ID_LINK,
	ICE_AQC_FW_LOG_ID_LINK_TOPO,
	ICE_AQC_FW_LOG_ID_DNL,
	ICE_AQC_FW_LOG_ID_I2C,
	ICE_AQC_FW_LOG_ID_SDP,
	ICE_AQC_FW_LOG_ID_MDIO,
	ICE_AQC_FW_LOG_ID_ADMINQ,
	ICE_AQC_FW_LOG_ID_HDMA,
	ICE_AQC_FW_LOG_ID_LLDP,
	ICE_AQC_FW_LOG_ID_DCBX,
	ICE_AQC_FW_LOG_ID_DCB,
	ICE_AQC_FW_LOG_ID_XLR,
	ICE_AQC_FW_LOG_ID_NVM,
	ICE_AQC_FW_LOG_ID_AUTH,
	ICE_AQC_FW_LOG_ID_VPD,
	ICE_AQC_FW_LOG_ID_IOSF,
	ICE_AQC_FW_LOG_ID_PARSER,
	ICE_AQC_FW_LOG_ID_SW,
	ICE_AQC_FW_LOG_ID_SCHEDULER,
	ICE_AQC_FW_LOG_ID_TXQ,
	ICE_AQC_FW_LOG_ID_RSVD,
	ICE_AQC_FW_LOG_ID_POST,
	ICE_AQC_FW_LOG_ID_WATCHDOG,
	ICE_AQC_FW_LOG_ID_TASK_DISPATCH,
	ICE_AQC_FW_LOG_ID_MNG,
	ICE_AQC_FW_LOG_ID_SYNCE,
	ICE_AQC_FW_LOG_ID_HEALTH,
	ICE_AQC_FW_LOG_ID_TSDRV,
	ICE_AQC_FW_LOG_ID_PFREG,
	ICE_AQC_FW_LOG_ID_MDLVER,
	ICE_AQC_FW_LOG_ID_MAX,
};

/* Set Health Status (direct 0xFF20) */
struct ice_aqc_set_health_status_config {
	u8 event_source;
#define ICE_AQC_HEALTH_STATUS_SET_PF_SPECIFIC_MASK	BIT(0)
#define ICE_AQC_HEALTH_STATUS_SET_ALL_PF_MASK		BIT(1)
#define ICE_AQC_HEALTH_STATUS_SET_GLOBAL_MASK		BIT(2)
	u8 reserved[15];
};

#define ICE_AQC_HEALTH_STATUS_ERR_UNKNOWN_MOD_STRICT		0x101
#define ICE_AQC_HEALTH_STATUS_ERR_MOD_TYPE			0x102
#define ICE_AQC_HEALTH_STATUS_ERR_MOD_QUAL			0x103
#define ICE_AQC_HEALTH_STATUS_ERR_MOD_COMM			0x104
#define ICE_AQC_HEALTH_STATUS_ERR_MOD_CONFLICT			0x105
#define ICE_AQC_HEALTH_STATUS_ERR_MOD_NOT_PRESENT		0x106
#define ICE_AQC_HEALTH_STATUS_INFO_MOD_UNDERUTILIZED		0x107
#define ICE_AQC_HEALTH_STATUS_ERR_UNKNOWN_MOD_LENIENT		0x108
#define ICE_AQC_HEALTH_STATUS_ERR_MOD_DIAGNOSTIC_FEATURE	0x109
#define ICE_AQC_HEALTH_STATUS_ERR_INVALID_LINK_CFG		0x10B
#define ICE_AQC_HEALTH_STATUS_ERR_PORT_ACCESS			0x10C
#define ICE_AQC_HEALTH_STATUS_ERR_PORT_UNREACHABLE		0x10D
#define ICE_AQC_HEALTH_STATUS_INFO_PORT_SPEED_MOD_LIMITED	0x10F
#define ICE_AQC_HEALTH_STATUS_ERR_PARALLEL_FAULT		0x110
#define ICE_AQC_HEALTH_STATUS_INFO_PORT_SPEED_PHY_LIMITED	0x111
#define ICE_AQC_HEALTH_STATUS_ERR_NETLIST_TOPO			0x112
#define ICE_AQC_HEALTH_STATUS_ERR_NETLIST			0x113
#define ICE_AQC_HEALTH_STATUS_ERR_TOPO_CONFLICT			0x114
#define ICE_AQC_HEALTH_STATUS_ERR_LINK_HW_ACCESS		0x115
#define ICE_AQC_HEALTH_STATUS_ERR_LINK_RUNTIME			0x116
#define ICE_AQC_HEALTH_STATUS_ERR_DNL_INIT			0x117
#define ICE_AQC_HEALTH_STATUS_ERR_PHY_NVM_PROG			0x120
#define ICE_AQC_HEALTH_STATUS_ERR_PHY_FW_LOAD			0x121
#define ICE_AQC_HEALTH_STATUS_INFO_RECOVERY			0x500
#define ICE_AQC_HEALTH_STATUS_ERR_FLASH_ACCESS			0x501
#define ICE_AQC_HEALTH_STATUS_ERR_NVM_AUTH			0x502
#define ICE_AQC_HEALTH_STATUS_ERR_OROM_AUTH			0x503
#define ICE_AQC_HEALTH_STATUS_ERR_DDP_AUTH			0x504
#define ICE_AQC_HEALTH_STATUS_ERR_NVM_COMPAT			0x505
#define ICE_AQC_HEALTH_STATUS_ERR_OROM_COMPAT			0x506
#define ICE_AQC_HEALTH_STATUS_ERR_NVM_SEC_VIOLATION		0x507
#define ICE_AQC_HEALTH_STATUS_ERR_OROM_SEC_VIOLATION		0x508
#define ICE_AQC_HEALTH_STATUS_ERR_DCB_MIB			0x509
#define ICE_AQC_HEALTH_STATUS_ERR_MNG_TIMEOUT			0x50A
#define ICE_AQC_HEALTH_STATUS_ERR_BMC_RESET			0x50B
#define ICE_AQC_HEALTH_STATUS_ERR_LAST_MNG_FAIL			0x50C
#define ICE_AQC_HEALTH_STATUS_ERR_RESOURCE_ALLOC_FAIL		0x50D
#define ICE_AQC_HEALTH_STATUS_ERR_FW_LOOP			0x1000
#define ICE_AQC_HEALTH_STATUS_ERR_FW_PFR_FAIL			0x1001
#define ICE_AQC_HEALTH_STATUS_ERR_LAST_FAIL_AQ			0x1002

/* Get Health Status codes (indirect 0xFF21) */
struct ice_aqc_get_supported_health_status_codes {
	__le16 health_code_count;
	u8 reserved[6];
	__le32 addr_high;
	__le32 addr_low;
};

/* Get Health Status (indirect 0xFF22) */
struct ice_aqc_get_health_status {
	__le16 health_status_count;
	u8 reserved[6];
	__le32 addr_high;
	__le32 addr_low;
};

/* Get Health Status event buffer entry, (0xFF22)
 * repeated per reported health status
 */
struct ice_aqc_health_status_elem {
	__le16 health_status_code;
	__le16 event_source;
#define ICE_AQC_HEALTH_STATUS_PF			(0x1)
#define ICE_AQC_HEALTH_STATUS_PORT			(0x2)
#define ICE_AQC_HEALTH_STATUS_GLOBAL			(0x3)
	__le32 internal_data1;
#define ICE_AQC_HEALTH_STATUS_UNDEFINED_DATA	(0xDEADBEEF)
	__le32 internal_data2;
};

/* Clear Health Status (direct 0xFF23) */
struct ice_aqc_clear_health_status {
	__le32 reserved[4];
};

/* Set FW Logging configuration (indirect 0xFF30)
 * Register for FW Logging (indirect 0xFF31)
 * Query FW Logging (indirect 0xFF32)
 * FW Log Event (indirect 0xFF33)
 * Get FW Log (indirect 0xFF34)
 * Clear FW Log (indirect 0xFF35)
 */
struct ice_aqc_fw_log {
	u8 cmd_flags;
#define ICE_AQC_FW_LOG_CONF_UART_EN	BIT(0)
#define ICE_AQC_FW_LOG_CONF_AQ_EN	BIT(1)
#define ICE_AQC_FW_LOG_QUERY_REGISTERED	BIT(2)
#define ICE_AQC_FW_LOG_CONF_SET_VALID	BIT(3)
#define ICE_AQC_FW_LOG_AQ_REGISTER	BIT(0)
#define ICE_AQC_FW_LOG_AQ_QUERY		BIT(2)
#define ICE_AQC_FW_LOG_PERSISTENT	BIT(0)
	u8 rsp_flag;
#define ICE_AQC_FW_LOG_MORE_DATA	BIT(1)
	__le16 fw_rt_msb;
	union {
		struct {
			__le32 fw_rt_lsb;
		} sync;
		struct {
			__le16 log_resolution;
#define ICE_AQC_FW_LOG_MIN_RESOLUTION		(1)
#define ICE_AQC_FW_LOG_MAX_RESOLUTION		(128)
			__le16 mdl_cnt;
		} cfg;
	} ops;
	__le32 addr_high;
	__le32 addr_low;
};

/* Response Buffer for:
 *    Set Firmware Logging Configuration (0xFF30)
 *    Query FW Logging (0xFF32)
 */
struct ice_aqc_fw_log_cfg_resp {
	__le16 module_identifier;
	u8 log_level;
	u8 rsvd0;
};

/**
 * struct ice_aq_desc - Admin Queue (AQ) descriptor
 * @flags: ICE_AQ_FLAG_* flags
 * @opcode: AQ command opcode
 * @datalen: length in bytes of indirect/external data buffer
 * @retval: return value from firmware
 * @cookie_high: opaque data high-half
 * @cookie_low: opaque data low-half
 * @params: command-specific parameters
 *
 * Descriptor format for commands the driver posts on the Admin Transmit Queue
 * (ATQ). The firmware writes back onto the command descriptor and returns
 * the result of the command. Asynchronous events that are not an immediate
 * result of the command are written to the Admin Receive Queue (ARQ) using
 * the same descriptor format. Descriptors are in little-endian notation with
 * 32-bit words.
 */
struct ice_aq_desc {
	__le16 flags;
	__le16 opcode;
	__le16 datalen;
	__le16 retval;
	__le32 cookie_high;
	__le32 cookie_low;
	union {
		u8 raw[16];
		struct ice_aqc_generic generic;
		struct ice_aqc_get_ver get_ver;
		struct ice_aqc_driver_ver driver_ver;
		struct ice_aqc_q_shutdown q_shutdown;
		struct ice_aqc_get_exp_err exp_err;
		struct ice_aqc_req_res res_owner;
		struct ice_aqc_manage_mac_read mac_read;
		struct ice_aqc_manage_mac_write mac_write;
		struct ice_aqc_clear_pxe clear_pxe;
		struct ice_aqc_config_no_drop_policy no_drop;
		struct ice_aqc_add_update_mir_rule add_update_rule;
		struct ice_aqc_delete_mir_rule del_rule;
		struct ice_aqc_list_caps get_cap;
		struct ice_aqc_get_phy_caps get_phy;
		struct ice_aqc_set_phy_cfg set_phy;
		struct ice_aqc_restart_an restart_an;
		struct ice_aqc_get_sensor_reading get_sensor_reading;
		struct ice_aqc_get_sensor_reading_resp get_sensor_reading_resp;
		struct ice_aqc_dnl_get_status get_status;
		struct ice_aqc_dnl_run_command dnl_run;
		struct ice_aqc_dnl_call_command dnl_call;
		struct ice_aqc_dnl_read_write_command dnl_read_write;
		struct ice_aqc_dnl_read_write_response dnl_read_write_resp;
		struct ice_aqc_dnl_set_breakpoints_command dnl_set_brk;
		struct ice_aqc_dnl_read_log_command dnl_read_log;
		struct ice_aqc_dnl_read_log_response dnl_read_log_resp;
		struct ice_aqc_i2c read_write_i2c;
		struct ice_aqc_read_i2c_resp read_i2c_resp;
		struct ice_aqc_mdio read_write_mdio;
		struct ice_aqc_gpio_by_func read_write_gpio_by_func;
		struct ice_aqc_gpio read_write_gpio;
		struct ice_aqc_sw_gpio sw_read_write_gpio;
		struct ice_aqc_set_led set_led;
		struct ice_aqc_mdio read_mdio;
		struct ice_aqc_mdio write_mdio;
		struct ice_aqc_sff_eeprom read_write_sff_param;
		struct ice_aqc_set_port_id_led set_port_id_led;
		struct ice_aqc_get_port_options get_port_options;
		struct ice_aqc_set_port_option set_port_option;
		struct ice_aqc_get_sw_cfg get_sw_conf;
		struct ice_aqc_set_port_params set_port_params;
		struct ice_aqc_sw_rules sw_rules;
		struct ice_aqc_storm_cfg storm_conf;
		struct ice_aqc_get_topo get_topo;
		struct ice_aqc_sched_elem_cmd sched_elem_cmd;
		struct ice_aqc_query_txsched_res query_sched_res;
		struct ice_aqc_query_node_to_root query_node_to_root;
		struct ice_aqc_cfg_l2_node_cgd cfg_l2_node_cgd;
		struct ice_aqc_query_port_ets port_ets;
		struct ice_aqc_rl_profile rl_profile;
		struct ice_aqc_node_attr node_attr;
		struct ice_aqc_nvm nvm;
		struct ice_aqc_nvm_cfg nvm_cfg;
		struct ice_aqc_nvm_checksum nvm_checksum;
		struct ice_aqc_nvm_sanitization sanitization;
		struct ice_aqc_pf_vf_msg virt;
		struct ice_aqc_read_write_alt_direct read_write_alt_direct;
		struct ice_aqc_read_write_alt_indirect read_write_alt_indirect;
		struct ice_aqc_done_alt_write done_alt_write;
		struct ice_aqc_clear_port_alt_write clear_port_alt_write;
		struct ice_aqc_pfc_ignore pfc_ignore;
		struct ice_aqc_set_query_pfc_mode set_query_pfc_mode;
		struct ice_aqc_set_dcb_params set_dcb_params;
		struct ice_aqc_lldp_get_mib lldp_get_mib;
		struct ice_aqc_lldp_set_mib_change lldp_set_event;
		struct ice_aqc_lldp_add_delete_tlv lldp_add_delete_tlv;
		struct ice_aqc_lldp_update_tlv lldp_update_tlv;
		struct ice_aqc_lldp_stop lldp_stop;
		struct ice_aqc_lldp_start lldp_start;
		struct ice_aqc_lldp_set_local_mib lldp_set_mib;
		struct ice_aqc_lldp_stop_start_specific_agent lldp_agent_ctrl;
		struct ice_aqc_lldp_filter_ctrl lldp_filter_ctrl;
		struct ice_aqc_get_set_rss_lut get_set_rss_lut;
		struct ice_aqc_get_set_rss_key get_set_rss_key;
		struct ice_aqc_neigh_dev_req neigh_dev;
		struct ice_aqc_add_txqs add_txqs;
		struct ice_aqc_dis_txqs dis_txqs;
		struct ice_aqc_move_txqs move_txqs;
		struct ice_aqc_add_rdma_qset add_rdma_qset;
		struct ice_aqc_move_rdma_qset_cmd move_rdma_qset;
		struct ice_aqc_txqs_cleanup txqs_cleanup;
		struct ice_aqc_add_get_update_free_vsi vsi_cmd;
		struct ice_aqc_add_update_free_vsi_resp add_update_free_vsi_res;
		struct ice_aqc_get_vsi_resp get_vsi_resp;
		struct ice_aqc_download_pkg download_pkg;
		struct ice_aqc_get_pkg_info_list get_pkg_info_list;
		struct ice_aqc_driver_shared_params drv_shared_params;
		struct ice_aqc_fw_log fw_log;
		struct ice_aqc_debug_dump_internals debug_dump;
		struct ice_aqc_set_mac_lb set_mac_lb;
		struct ice_aqc_alloc_free_res_cmd sw_res_ctrl;
		struct ice_aqc_get_res_alloc get_res;
		struct ice_aqc_get_allocd_res_desc get_res_desc;
		struct ice_aqc_set_mac_cfg set_mac_cfg;
		struct ice_aqc_set_event_mask set_event_mask;
		struct ice_aqc_get_link_status get_link_status;
		struct ice_aqc_event_lan_overflow lan_overflow;
		struct ice_aqc_get_link_topo get_link_topo;
		struct ice_aqc_set_health_status_config
			set_health_status_config;
		struct ice_aqc_get_supported_health_status_codes
			get_supported_health_status_codes;
		struct ice_aqc_get_health_status get_health_status;
		struct ice_aqc_clear_health_status clear_health_status;
		struct ice_aqc_prog_topo_dev_nvm prog_topo_dev_nvm;
		struct ice_aqc_read_topo_dev_nvm read_topo_dev_nvm;
		struct ice_aqc_get_set_tx_topo get_set_tx_topo;
	} params;
};

/* FW defined boundary for a large buffer, 4k >= Large buffer > 512 bytes */
#define ICE_AQ_LG_BUF	512

/* Flags sub-structure
 * |0  |1  |2  |3  |4  |5  |6  |7  |8  |9  |10 |11 |12 |13 |14 |15 |
 * |DD |CMP|ERR|VFE| * *  RESERVED * * |LB |RD |VFC|BUF|SI |EI |FE |
 */

/* command flags and offsets */
#define ICE_AQ_FLAG_DD_S	0
#define ICE_AQ_FLAG_CMP_S	1
#define ICE_AQ_FLAG_ERR_S	2
#define ICE_AQ_FLAG_VFE_S	3
#define ICE_AQ_FLAG_LB_S	9
#define ICE_AQ_FLAG_RD_S	10
#define ICE_AQ_FLAG_VFC_S	11
#define ICE_AQ_FLAG_BUF_S	12
#define ICE_AQ_FLAG_SI_S	13
#define ICE_AQ_FLAG_EI_S	14
#define ICE_AQ_FLAG_FE_S	15

#define ICE_AQ_FLAG_DD		BIT(ICE_AQ_FLAG_DD_S)  /* 0x1    */
#define ICE_AQ_FLAG_CMP		BIT(ICE_AQ_FLAG_CMP_S) /* 0x2    */
#define ICE_AQ_FLAG_ERR		BIT(ICE_AQ_FLAG_ERR_S) /* 0x4    */
#define ICE_AQ_FLAG_VFE		BIT(ICE_AQ_FLAG_VFE_S) /* 0x8    */
#define ICE_AQ_FLAG_LB		BIT(ICE_AQ_FLAG_LB_S)  /* 0x200  */
#define ICE_AQ_FLAG_RD		BIT(ICE_AQ_FLAG_RD_S)  /* 0x400  */
#define ICE_AQ_FLAG_VFC		BIT(ICE_AQ_FLAG_VFC_S) /* 0x800  */
#define ICE_AQ_FLAG_BUF		BIT(ICE_AQ_FLAG_BUF_S) /* 0x1000 */
#define ICE_AQ_FLAG_SI		BIT(ICE_AQ_FLAG_SI_S)  /* 0x2000 */
#define ICE_AQ_FLAG_EI		BIT(ICE_AQ_FLAG_EI_S)  /* 0x4000 */
#define ICE_AQ_FLAG_FE		BIT(ICE_AQ_FLAG_FE_S)  /* 0x8000 */

/* error codes */
enum ice_aq_err {
	ICE_AQ_RC_OK		= 0,  /* Success */
	ICE_AQ_RC_EPERM		= 1,  /* Operation not permitted */
	ICE_AQ_RC_ENOENT	= 2,  /* No such element */
	ICE_AQ_RC_ESRCH		= 3,  /* Bad opcode */
	ICE_AQ_RC_EINTR		= 4,  /* Operation interrupted */
	ICE_AQ_RC_EIO		= 5,  /* I/O error */
	ICE_AQ_RC_ENXIO		= 6,  /* No such resource */
	ICE_AQ_RC_E2BIG		= 7,  /* Arg too long */
	ICE_AQ_RC_EAGAIN	= 8,  /* Try again */
	ICE_AQ_RC_ENOMEM	= 9,  /* Out of memory */
	ICE_AQ_RC_EACCES	= 10, /* Permission denied */
	ICE_AQ_RC_EFAULT	= 11, /* Bad address */
	ICE_AQ_RC_EBUSY		= 12, /* Device or resource busy */
	ICE_AQ_RC_EEXIST	= 13, /* Object already exists */
	ICE_AQ_RC_EINVAL	= 14, /* Invalid argument */
	ICE_AQ_RC_ENOTTY	= 15, /* Not a typewriter */
	ICE_AQ_RC_ENOSPC	= 16, /* No space left or allocation failure */
	ICE_AQ_RC_ENOSYS	= 17, /* Function not implemented */
	ICE_AQ_RC_ERANGE	= 18, /* Parameter out of range */
	ICE_AQ_RC_EFLUSHED	= 19, /* Cmd flushed due to prev cmd error */
	ICE_AQ_RC_BAD_ADDR	= 20, /* Descriptor contains a bad pointer */
	ICE_AQ_RC_EMODE		= 21, /* Op not allowed in current dev mode */
	ICE_AQ_RC_EFBIG		= 22, /* File too big */
	ICE_AQ_RC_ESBCOMP	= 23, /* SB-IOSF completion unsuccessful */
	ICE_AQ_RC_ENOSEC	= 24, /* Missing security manifest */
	ICE_AQ_RC_EBADSIG	= 25, /* Bad RSA signature */
	ICE_AQ_RC_ESVN		= 26, /* SVN number prohibits this package */
	ICE_AQ_RC_EBADMAN	= 27, /* Manifest hash mismatch */
	ICE_AQ_RC_EBADBUF	= 28, /* Buffer hash mismatches manifest */
	ICE_AQ_RC_EACCES_BMCU	= 29, /* BMC Update in progress */
};

/* Admin Queue command opcodes */
enum ice_adminq_opc {
	/* AQ commands */
	ice_aqc_opc_get_ver				= 0x0001,
	ice_aqc_opc_driver_ver				= 0x0002,
	ice_aqc_opc_q_shutdown				= 0x0003,
	ice_aqc_opc_get_exp_err				= 0x0005,

	/* resource ownership */
	ice_aqc_opc_req_res				= 0x0008,
	ice_aqc_opc_release_res				= 0x0009,

	/* device/function capabilities */
	ice_aqc_opc_list_func_caps			= 0x000A,
	ice_aqc_opc_list_dev_caps			= 0x000B,

	/* manage MAC address */
	ice_aqc_opc_manage_mac_read			= 0x0107,
	ice_aqc_opc_manage_mac_write			= 0x0108,

	/* PXE */
	ice_aqc_opc_clear_pxe_mode			= 0x0110,

	ice_aqc_opc_config_no_drop_policy		= 0x0112,

	/* internal switch commands */
	ice_aqc_opc_get_sw_cfg				= 0x0200,
	ice_aqc_opc_set_port_params			= 0x0203,

	/* Alloc/Free/Get Resources */
	ice_aqc_opc_get_res_alloc			= 0x0204,
	ice_aqc_opc_alloc_res				= 0x0208,
	ice_aqc_opc_free_res				= 0x0209,
	ice_aqc_opc_get_allocd_res_desc			= 0x020A,
	ice_aqc_opc_set_vlan_mode_parameters		= 0x020C,
	ice_aqc_opc_get_vlan_mode_parameters		= 0x020D,

	/* VSI commands */
	ice_aqc_opc_add_vsi				= 0x0210,
	ice_aqc_opc_update_vsi				= 0x0211,
	ice_aqc_opc_get_vsi_params			= 0x0212,
	ice_aqc_opc_free_vsi				= 0x0213,

	/* Mirroring rules - add/update, delete */
	ice_aqc_opc_add_update_mir_rule			= 0x0260,
	ice_aqc_opc_del_mir_rule			= 0x0261,

	/* storm configuration */
	ice_aqc_opc_set_storm_cfg			= 0x0280,
	ice_aqc_opc_get_storm_cfg			= 0x0281,

	/* switch rules population commands */
	ice_aqc_opc_add_sw_rules			= 0x02A0,
	ice_aqc_opc_update_sw_rules			= 0x02A1,
	ice_aqc_opc_remove_sw_rules			= 0x02A2,
	ice_aqc_opc_get_sw_rules			= 0x02A3,
	ice_aqc_opc_clear_pf_cfg			= 0x02A4,

	/* DCB commands */
	ice_aqc_opc_pfc_ignore				= 0x0301,
	ice_aqc_opc_query_pfc_mode			= 0x0302,
	ice_aqc_opc_set_pfc_mode			= 0x0303,
	ice_aqc_opc_set_dcb_params			= 0x0306,

	/* transmit scheduler commands */
	ice_aqc_opc_get_dflt_topo			= 0x0400,
	ice_aqc_opc_add_sched_elems			= 0x0401,
	ice_aqc_opc_cfg_sched_elems			= 0x0403,
	ice_aqc_opc_get_sched_elems			= 0x0404,
	ice_aqc_opc_move_sched_elems			= 0x0408,
	ice_aqc_opc_suspend_sched_elems			= 0x0409,
	ice_aqc_opc_resume_sched_elems			= 0x040A,
	ice_aqc_opc_query_port_ets			= 0x040E,
	ice_aqc_opc_delete_sched_elems			= 0x040F,
	ice_aqc_opc_add_rl_profiles			= 0x0410,
	ice_aqc_opc_query_rl_profiles			= 0x0411,
	ice_aqc_opc_query_sched_res			= 0x0412,
	ice_aqc_opc_query_node_to_root			= 0x0413,
	ice_aqc_opc_cfg_l2_node_cgd			= 0x0414,
	ice_aqc_opc_remove_rl_profiles			= 0x0415,
	ice_aqc_opc_set_tx_topo				= 0x0417,
	ice_aqc_opc_get_tx_topo				= 0x0418,
	ice_aqc_opc_cfg_node_attr			= 0x0419,
	ice_aqc_opc_query_node_attr			= 0x041A,

	/* PHY commands */
	ice_aqc_opc_get_phy_caps			= 0x0600,
	ice_aqc_opc_set_phy_cfg				= 0x0601,
	ice_aqc_opc_set_mac_cfg				= 0x0603,
	ice_aqc_opc_restart_an				= 0x0605,
	ice_aqc_opc_get_link_status			= 0x0607,
	ice_aqc_opc_set_event_mask			= 0x0613,
	ice_aqc_opc_set_mac_lb				= 0x0620,
	ice_aqc_opc_get_sensor_reading			= 0x0632,
	ice_aqc_opc_dnl_get_status			= 0x0680,
	ice_aqc_opc_dnl_run				= 0x0681,
	ice_aqc_opc_dnl_call				= 0x0682,
	ice_aqc_opc_dnl_read_sto			= 0x0683,
	ice_aqc_opc_dnl_write_sto			= 0x0684,
	ice_aqc_opc_dnl_set_breakpoints			= 0x0686,
	ice_aqc_opc_dnl_read_log			= 0x0687,
	ice_aqc_opc_get_link_topo			= 0x06E0,
	ice_aqc_opc_read_i2c				= 0x06E2,
	ice_aqc_opc_write_i2c				= 0x06E3,
	ice_aqc_opc_read_mdio				= 0x06E4,
	ice_aqc_opc_write_mdio				= 0x06E5,
	ice_aqc_opc_set_gpio_by_func			= 0x06E6,
	ice_aqc_opc_get_gpio_by_func			= 0x06E7,
	ice_aqc_opc_set_led				= 0x06E8,
	ice_aqc_opc_set_port_id_led			= 0x06E9,
	ice_aqc_opc_get_port_options			= 0x06EA,
	ice_aqc_opc_set_port_option			= 0x06EB,
	ice_aqc_opc_set_gpio				= 0x06EC,
	ice_aqc_opc_get_gpio				= 0x06ED,
	ice_aqc_opc_sff_eeprom				= 0x06EE,
	ice_aqc_opc_sw_set_gpio				= 0x06EF,
	ice_aqc_opc_sw_get_gpio				= 0x06F0,
	ice_aqc_opc_prog_topo_dev_nvm			= 0x06F2,
	ice_aqc_opc_read_topo_dev_nvm			= 0x06F3,

	/* NVM commands */
	ice_aqc_opc_nvm_read				= 0x0701,
	ice_aqc_opc_nvm_erase				= 0x0702,
	ice_aqc_opc_nvm_write				= 0x0703,
	ice_aqc_opc_nvm_cfg_read			= 0x0704,
	ice_aqc_opc_nvm_cfg_write			= 0x0705,
	ice_aqc_opc_nvm_checksum			= 0x0706,
	ice_aqc_opc_nvm_write_activate			= 0x0707,
	ice_aqc_opc_nvm_sr_dump				= 0x0707,
	ice_aqc_opc_nvm_save_factory_settings		= 0x0708,
	ice_aqc_opc_nvm_update_empr			= 0x0709,
	ice_aqc_opc_nvm_pkg_data			= 0x070A,
	ice_aqc_opc_nvm_pass_component_tbl		= 0x070B,
	ice_aqc_opc_nvm_sanitization			= 0x070C,

	/* PF/VF mailbox commands */
	ice_mbx_opc_send_msg_to_pf			= 0x0801,
	ice_mbx_opc_send_msg_to_vf			= 0x0802,
	/* Alternate Structure Commands */
	ice_aqc_opc_write_alt_direct			= 0x0900,
	ice_aqc_opc_write_alt_indirect			= 0x0901,
	ice_aqc_opc_read_alt_direct			= 0x0902,
	ice_aqc_opc_read_alt_indirect			= 0x0903,
	ice_aqc_opc_done_alt_write			= 0x0904,
	ice_aqc_opc_clear_port_alt_write		= 0x0906,
	/* LLDP commands */
	ice_aqc_opc_lldp_get_mib			= 0x0A00,
	ice_aqc_opc_lldp_set_mib_change			= 0x0A01,
	ice_aqc_opc_lldp_add_tlv			= 0x0A02,
	ice_aqc_opc_lldp_update_tlv			= 0x0A03,
	ice_aqc_opc_lldp_delete_tlv			= 0x0A04,
	ice_aqc_opc_lldp_stop				= 0x0A05,
	ice_aqc_opc_lldp_start				= 0x0A06,
	ice_aqc_opc_get_cee_dcb_cfg			= 0x0A07,
	ice_aqc_opc_lldp_set_local_mib			= 0x0A08,
	ice_aqc_opc_lldp_stop_start_specific_agent	= 0x0A09,
	ice_aqc_opc_lldp_filter_ctrl			= 0x0A0A,
	ice_execute_pending_lldp_mib			= 0x0A0B,

	/* RSS commands */
	ice_aqc_opc_set_rss_key				= 0x0B02,
	ice_aqc_opc_set_rss_lut				= 0x0B03,
	ice_aqc_opc_get_rss_key				= 0x0B04,
	ice_aqc_opc_get_rss_lut				= 0x0B05,
	/* Sideband Control Interface commands */
	ice_aqc_opc_neighbour_device_request		= 0x0C00,

	/* Tx queue handling commands/events */
	ice_aqc_opc_add_txqs				= 0x0C30,
	ice_aqc_opc_dis_txqs				= 0x0C31,
	ice_aqc_opc_txqs_cleanup			= 0x0C31,
	ice_aqc_opc_move_recfg_txqs			= 0x0C32,
	ice_aqc_opc_add_rdma_qset			= 0x0C33,
	ice_aqc_opc_move_rdma_qset			= 0x0C34,

	/* package commands */
	ice_aqc_opc_download_pkg			= 0x0C40,
	ice_aqc_opc_upload_section			= 0x0C41,
	ice_aqc_opc_update_pkg				= 0x0C42,
	ice_aqc_opc_get_pkg_info_list			= 0x0C43,

	ice_aqc_opc_driver_shared_params		= 0x0C90,

	/* Standalone Commands/Events */
	ice_aqc_opc_event_lan_overflow			= 0x1001,

	/* debug commands */
	ice_aqc_opc_debug_dump_internals		= 0xFF08,

	/* SystemDiagnostic commands */
	ice_aqc_opc_set_health_status_config		= 0xFF20,
	ice_aqc_opc_get_supported_health_status_codes	= 0xFF21,
	ice_aqc_opc_get_health_status			= 0xFF22,
	ice_aqc_opc_clear_health_status			= 0xFF23,

	/* FW Logging Commands */
	ice_aqc_opc_fw_logs_config			= 0xFF30,
	ice_aqc_opc_fw_logs_register			= 0xFF31,
	ice_aqc_opc_fw_logs_query			= 0xFF32,
	ice_aqc_opc_fw_logs_event			= 0xFF33,
	ice_aqc_opc_fw_logs_get				= 0xFF34,
	ice_aqc_opc_fw_logs_clear			= 0xFF35
};

#endif /* _ICE_ADMINQ_CMD_H_ */
