/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2021, Intel Corporation
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

#ifndef _IAVF_ADMINQ_CMD_H_
#define _IAVF_ADMINQ_CMD_H_

/* This header file defines the iavf Admin Queue commands and is shared between
 * iavf Firmware and Software.  Do not change the names in this file to IAVF
 * because this file should be diff-able against the iavf version, even
 * though many parts have been removed in this VF version.
 *
 * This file needs to comply with the Linux Kernel coding style.
 */

#define IAVF_FW_API_VERSION_MAJOR	0x0001
#define IAVF_FW_API_VERSION_MINOR_X722	0x0006
#define IAVF_FW_API_VERSION_MINOR_X710	0x0007

#define IAVF_FW_MINOR_VERSION(_h) ((_h)->mac.type == IAVF_MAC_XL710 ? \
					IAVF_FW_API_VERSION_MINOR_X710 : \
					IAVF_FW_API_VERSION_MINOR_X722)

/* API version 1.7 implements additional link and PHY-specific APIs  */
#define IAVF_MINOR_VER_GET_LINK_INFO_XL710 0x0007
/* API version 1.6 for X722 devices adds ability to stop FW LLDP agent */
#define IAVF_MINOR_VER_FW_LLDP_STOPPABLE_X722 0x0006

struct iavf_aq_desc {
	__le16 flags;
	__le16 opcode;
	__le16 datalen;
	__le16 retval;
	__le32 cookie_high;
	__le32 cookie_low;
	union {
		struct {
			__le32 param0;
			__le32 param1;
			__le32 param2;
			__le32 param3;
		} internal;
		struct {
			__le32 param0;
			__le32 param1;
			__le32 addr_high;
			__le32 addr_low;
		} external;
		u8 raw[16];
	} params;
};

/* Flags sub-structure
 * |0  |1  |2  |3  |4  |5  |6  |7  |8  |9  |10 |11 |12 |13 |14 |15 |
 * |DD |CMP|ERR|VFE| * *  RESERVED * * |LB |RD |VFC|BUF|SI |EI |FE |
 */

/* command flags and offsets*/
#define IAVF_AQ_FLAG_DD_SHIFT	0
#define IAVF_AQ_FLAG_CMP_SHIFT	1
#define IAVF_AQ_FLAG_ERR_SHIFT	2
#define IAVF_AQ_FLAG_VFE_SHIFT	3
#define IAVF_AQ_FLAG_LB_SHIFT	9
#define IAVF_AQ_FLAG_RD_SHIFT	10
#define IAVF_AQ_FLAG_VFC_SHIFT	11
#define IAVF_AQ_FLAG_BUF_SHIFT	12
#define IAVF_AQ_FLAG_SI_SHIFT	13
#define IAVF_AQ_FLAG_EI_SHIFT	14
#define IAVF_AQ_FLAG_FE_SHIFT	15

#define IAVF_AQ_FLAG_DD		(1 << IAVF_AQ_FLAG_DD_SHIFT)  /* 0x1    */
#define IAVF_AQ_FLAG_CMP	(1 << IAVF_AQ_FLAG_CMP_SHIFT) /* 0x2    */
#define IAVF_AQ_FLAG_ERR	(1 << IAVF_AQ_FLAG_ERR_SHIFT) /* 0x4    */
#define IAVF_AQ_FLAG_VFE	(1 << IAVF_AQ_FLAG_VFE_SHIFT) /* 0x8    */
#define IAVF_AQ_FLAG_LB		(1 << IAVF_AQ_FLAG_LB_SHIFT)  /* 0x200  */
#define IAVF_AQ_FLAG_RD		(1 << IAVF_AQ_FLAG_RD_SHIFT)  /* 0x400  */
#define IAVF_AQ_FLAG_VFC	(1 << IAVF_AQ_FLAG_VFC_SHIFT) /* 0x800  */
#define IAVF_AQ_FLAG_BUF	(1 << IAVF_AQ_FLAG_BUF_SHIFT) /* 0x1000 */
#define IAVF_AQ_FLAG_SI		(1 << IAVF_AQ_FLAG_SI_SHIFT)  /* 0x2000 */
#define IAVF_AQ_FLAG_EI		(1 << IAVF_AQ_FLAG_EI_SHIFT)  /* 0x4000 */
#define IAVF_AQ_FLAG_FE		(1 << IAVF_AQ_FLAG_FE_SHIFT)  /* 0x8000 */

/* error codes */
enum iavf_admin_queue_err {
	IAVF_AQ_RC_OK		= 0,  /* success */
	IAVF_AQ_RC_EPERM	= 1,  /* Operation not permitted */
	IAVF_AQ_RC_ENOENT	= 2,  /* No such element */
	IAVF_AQ_RC_ESRCH	= 3,  /* Bad opcode */
	IAVF_AQ_RC_EINTR	= 4,  /* operation interrupted */
	IAVF_AQ_RC_EIO		= 5,  /* I/O error */
	IAVF_AQ_RC_ENXIO	= 6,  /* No such resource */
	IAVF_AQ_RC_E2BIG	= 7,  /* Arg too long */
	IAVF_AQ_RC_EAGAIN	= 8,  /* Try again */
	IAVF_AQ_RC_ENOMEM	= 9,  /* Out of memory */
	IAVF_AQ_RC_EACCES	= 10, /* Permission denied */
	IAVF_AQ_RC_EFAULT	= 11, /* Bad address */
	IAVF_AQ_RC_EBUSY	= 12, /* Device or resource busy */
	IAVF_AQ_RC_EEXIST	= 13, /* object already exists */
	IAVF_AQ_RC_EINVAL	= 14, /* Invalid argument */
	IAVF_AQ_RC_ENOTTY	= 15, /* Not a typewriter */
	IAVF_AQ_RC_ENOSPC	= 16, /* No space left or alloc failure */
	IAVF_AQ_RC_ENOSYS	= 17, /* Function not implemented */
	IAVF_AQ_RC_ERANGE	= 18, /* Parameter out of range */
	IAVF_AQ_RC_EFLUSHED	= 19, /* Cmd flushed due to prev cmd error */
	IAVF_AQ_RC_BAD_ADDR	= 20, /* Descriptor contains a bad pointer */
	IAVF_AQ_RC_EMODE	= 21, /* Op not allowed in current dev mode */
	IAVF_AQ_RC_EFBIG	= 22, /* File too large */
};

/* Admin Queue command opcodes */
enum iavf_admin_queue_opc {
	/* aq commands */
	iavf_aqc_opc_get_version	= 0x0001,
	iavf_aqc_opc_driver_version	= 0x0002,
	iavf_aqc_opc_queue_shutdown	= 0x0003,
	iavf_aqc_opc_set_pf_context	= 0x0004,

	/* resource ownership */
	iavf_aqc_opc_request_resource	= 0x0008,
	iavf_aqc_opc_release_resource	= 0x0009,

	iavf_aqc_opc_list_func_capabilities	= 0x000A,
	iavf_aqc_opc_list_dev_capabilities	= 0x000B,

	/* Proxy commands */
	iavf_aqc_opc_set_proxy_config		= 0x0104,
	iavf_aqc_opc_set_ns_proxy_table_entry	= 0x0105,

	/* LAA */
	iavf_aqc_opc_mac_address_read	= 0x0107,
	iavf_aqc_opc_mac_address_write	= 0x0108,

	/* PXE */
	iavf_aqc_opc_clear_pxe_mode	= 0x0110,

	/* WoL commands */
	iavf_aqc_opc_set_wol_filter	= 0x0120,
	iavf_aqc_opc_get_wake_reason	= 0x0121,
	iavf_aqc_opc_clear_all_wol_filters = 0x025E,

	/* internal switch commands */
	iavf_aqc_opc_get_switch_config		= 0x0200,
	iavf_aqc_opc_add_statistics		= 0x0201,
	iavf_aqc_opc_remove_statistics		= 0x0202,
	iavf_aqc_opc_set_port_parameters	= 0x0203,
	iavf_aqc_opc_get_switch_resource_alloc	= 0x0204,
	iavf_aqc_opc_set_switch_config		= 0x0205,
	iavf_aqc_opc_rx_ctl_reg_read		= 0x0206,
	iavf_aqc_opc_rx_ctl_reg_write		= 0x0207,

	iavf_aqc_opc_add_vsi			= 0x0210,
	iavf_aqc_opc_update_vsi_parameters	= 0x0211,
	iavf_aqc_opc_get_vsi_parameters		= 0x0212,

	iavf_aqc_opc_add_pv			= 0x0220,
	iavf_aqc_opc_update_pv_parameters	= 0x0221,
	iavf_aqc_opc_get_pv_parameters		= 0x0222,

	iavf_aqc_opc_add_veb			= 0x0230,
	iavf_aqc_opc_update_veb_parameters	= 0x0231,
	iavf_aqc_opc_get_veb_parameters		= 0x0232,

	iavf_aqc_opc_delete_element		= 0x0243,

	iavf_aqc_opc_add_macvlan		= 0x0250,
	iavf_aqc_opc_remove_macvlan		= 0x0251,
	iavf_aqc_opc_add_vlan			= 0x0252,
	iavf_aqc_opc_remove_vlan		= 0x0253,
	iavf_aqc_opc_set_vsi_promiscuous_modes	= 0x0254,
	iavf_aqc_opc_add_tag			= 0x0255,
	iavf_aqc_opc_remove_tag			= 0x0256,
	iavf_aqc_opc_add_multicast_etag		= 0x0257,
	iavf_aqc_opc_remove_multicast_etag	= 0x0258,
	iavf_aqc_opc_update_tag			= 0x0259,
	iavf_aqc_opc_add_control_packet_filter	= 0x025A,
	iavf_aqc_opc_remove_control_packet_filter	= 0x025B,
	iavf_aqc_opc_add_cloud_filters		= 0x025C,
	iavf_aqc_opc_remove_cloud_filters	= 0x025D,
	iavf_aqc_opc_clear_wol_switch_filters	= 0x025E,
	iavf_aqc_opc_replace_cloud_filters	= 0x025F,

	iavf_aqc_opc_add_mirror_rule	= 0x0260,
	iavf_aqc_opc_delete_mirror_rule	= 0x0261,

	/* Dynamic Device Personalization */
	iavf_aqc_opc_write_personalization_profile	= 0x0270,
	iavf_aqc_opc_get_personalization_profile_list	= 0x0271,

	/* DCB commands */
	iavf_aqc_opc_dcb_ignore_pfc	= 0x0301,
	iavf_aqc_opc_dcb_updated	= 0x0302,
	iavf_aqc_opc_set_dcb_parameters = 0x0303,

	/* TX scheduler */
	iavf_aqc_opc_configure_vsi_bw_limit		= 0x0400,
	iavf_aqc_opc_configure_vsi_ets_sla_bw_limit	= 0x0406,
	iavf_aqc_opc_configure_vsi_tc_bw		= 0x0407,
	iavf_aqc_opc_query_vsi_bw_config		= 0x0408,
	iavf_aqc_opc_query_vsi_ets_sla_config		= 0x040A,
	iavf_aqc_opc_configure_switching_comp_bw_limit	= 0x0410,

	iavf_aqc_opc_enable_switching_comp_ets			= 0x0413,
	iavf_aqc_opc_modify_switching_comp_ets			= 0x0414,
	iavf_aqc_opc_disable_switching_comp_ets			= 0x0415,
	iavf_aqc_opc_configure_switching_comp_ets_bw_limit	= 0x0416,
	iavf_aqc_opc_configure_switching_comp_bw_config		= 0x0417,
	iavf_aqc_opc_query_switching_comp_ets_config		= 0x0418,
	iavf_aqc_opc_query_port_ets_config			= 0x0419,
	iavf_aqc_opc_query_switching_comp_bw_config		= 0x041A,
	iavf_aqc_opc_suspend_port_tx				= 0x041B,
	iavf_aqc_opc_resume_port_tx				= 0x041C,
	iavf_aqc_opc_configure_partition_bw			= 0x041D,
	/* hmc */
	iavf_aqc_opc_query_hmc_resource_profile	= 0x0500,
	iavf_aqc_opc_set_hmc_resource_profile	= 0x0501,

	/* phy commands*/

	/* phy commands*/
	iavf_aqc_opc_get_phy_abilities		= 0x0600,
	iavf_aqc_opc_set_phy_config		= 0x0601,
	iavf_aqc_opc_set_mac_config		= 0x0603,
	iavf_aqc_opc_set_link_restart_an	= 0x0605,
	iavf_aqc_opc_get_link_status		= 0x0607,
	iavf_aqc_opc_set_phy_int_mask		= 0x0613,
	iavf_aqc_opc_get_local_advt_reg		= 0x0614,
	iavf_aqc_opc_set_local_advt_reg		= 0x0615,
	iavf_aqc_opc_get_partner_advt		= 0x0616,
	iavf_aqc_opc_set_lb_modes		= 0x0618,
	iavf_aqc_opc_get_phy_wol_caps		= 0x0621,
	iavf_aqc_opc_set_phy_debug		= 0x0622,
	iavf_aqc_opc_upload_ext_phy_fm		= 0x0625,
	iavf_aqc_opc_run_phy_activity		= 0x0626,
	iavf_aqc_opc_set_phy_register		= 0x0628,
	iavf_aqc_opc_get_phy_register		= 0x0629,

	/* NVM commands */
	iavf_aqc_opc_nvm_read			= 0x0701,
	iavf_aqc_opc_nvm_erase			= 0x0702,
	iavf_aqc_opc_nvm_update			= 0x0703,
	iavf_aqc_opc_nvm_config_read		= 0x0704,
	iavf_aqc_opc_nvm_config_write		= 0x0705,
	iavf_aqc_opc_nvm_progress		= 0x0706,
	iavf_aqc_opc_oem_post_update		= 0x0720,
	iavf_aqc_opc_thermal_sensor		= 0x0721,

	/* virtualization commands */
	iavf_aqc_opc_send_msg_to_pf		= 0x0801,
	iavf_aqc_opc_send_msg_to_vf		= 0x0802,
	iavf_aqc_opc_send_msg_to_peer		= 0x0803,

	/* alternate structure */
	iavf_aqc_opc_alternate_write		= 0x0900,
	iavf_aqc_opc_alternate_write_indirect	= 0x0901,
	iavf_aqc_opc_alternate_read		= 0x0902,
	iavf_aqc_opc_alternate_read_indirect	= 0x0903,
	iavf_aqc_opc_alternate_write_done	= 0x0904,
	iavf_aqc_opc_alternate_set_mode		= 0x0905,
	iavf_aqc_opc_alternate_clear_port	= 0x0906,

	/* LLDP commands */
	iavf_aqc_opc_lldp_get_mib	= 0x0A00,
	iavf_aqc_opc_lldp_update_mib	= 0x0A01,
	iavf_aqc_opc_lldp_add_tlv	= 0x0A02,
	iavf_aqc_opc_lldp_update_tlv	= 0x0A03,
	iavf_aqc_opc_lldp_delete_tlv	= 0x0A04,
	iavf_aqc_opc_lldp_stop		= 0x0A05,
	iavf_aqc_opc_lldp_start		= 0x0A06,
	iavf_aqc_opc_get_cee_dcb_cfg	= 0x0A07,
	iavf_aqc_opc_lldp_set_local_mib	= 0x0A08,
	iavf_aqc_opc_lldp_stop_start_spec_agent	= 0x0A09,

	/* Tunnel commands */
	iavf_aqc_opc_add_udp_tunnel	= 0x0B00,
	iavf_aqc_opc_del_udp_tunnel	= 0x0B01,
	iavf_aqc_opc_set_rss_key	= 0x0B02,
	iavf_aqc_opc_set_rss_lut	= 0x0B03,
	iavf_aqc_opc_get_rss_key	= 0x0B04,
	iavf_aqc_opc_get_rss_lut	= 0x0B05,

	/* Async Events */
	iavf_aqc_opc_event_lan_overflow		= 0x1001,

	/* OEM commands */
	iavf_aqc_opc_oem_parameter_change	= 0xFE00,
	iavf_aqc_opc_oem_device_status_change	= 0xFE01,
	iavf_aqc_opc_oem_ocsd_initialize	= 0xFE02,
	iavf_aqc_opc_oem_ocbb_initialize	= 0xFE03,

	/* debug commands */
	iavf_aqc_opc_debug_read_reg		= 0xFF03,
	iavf_aqc_opc_debug_write_reg		= 0xFF04,
	iavf_aqc_opc_debug_modify_reg		= 0xFF07,
	iavf_aqc_opc_debug_dump_internals	= 0xFF08,
};

/* command structures and indirect data structures */

/* Structure naming conventions:
 * - no suffix for direct command descriptor structures
 * - _data for indirect sent data
 * - _resp for indirect return data (data which is both will use _data)
 * - _completion for direct return data
 * - _element_ for repeated elements (may also be _data or _resp)
 *
 * Command structures are expected to overlay the params.raw member of the basic
 * descriptor, and as such cannot exceed 16 bytes in length.
 */

/* This macro is used to generate a compilation error if a structure
 * is not exactly the correct length. It gives a divide by zero error if the
 * structure is not of the correct size, otherwise it creates an enum that is
 * never used.
 */
#define IAVF_CHECK_STRUCT_LEN(n, X) enum iavf_static_assert_enum_##X \
	{ iavf_static_assert_##X = (n)/((sizeof(struct X) == (n)) ? 1 : 0) }

/* This macro is used extensively to ensure that command structures are 16
 * bytes in length as they have to map to the raw array of that size.
 */
#define IAVF_CHECK_CMD_LENGTH(X)	IAVF_CHECK_STRUCT_LEN(16, X)

/* Queue Shutdown (direct 0x0003) */
struct iavf_aqc_queue_shutdown {
	__le32	driver_unloading;
#define IAVF_AQ_DRIVER_UNLOADING	0x1
	u8	reserved[12];
};

IAVF_CHECK_CMD_LENGTH(iavf_aqc_queue_shutdown);

#define IAVF_AQC_WOL_PRESERVE_STATUS	0x200
#define IAVF_AQC_MC_MAG_EN		0x0100
#define IAVF_AQC_WOL_PRESERVE_ON_PFR	0x0200

struct iavf_aqc_vsi_properties_data {
	/* first 96 byte are written by SW */
	__le16	valid_sections;
#define IAVF_AQ_VSI_PROP_SWITCH_VALID		0x0001
#define IAVF_AQ_VSI_PROP_SECURITY_VALID		0x0002
#define IAVF_AQ_VSI_PROP_VLAN_VALID		0x0004
#define IAVF_AQ_VSI_PROP_CAS_PV_VALID		0x0008
#define IAVF_AQ_VSI_PROP_INGRESS_UP_VALID	0x0010
#define IAVF_AQ_VSI_PROP_EGRESS_UP_VALID	0x0020
#define IAVF_AQ_VSI_PROP_QUEUE_MAP_VALID	0x0040
#define IAVF_AQ_VSI_PROP_QUEUE_OPT_VALID	0x0080
#define IAVF_AQ_VSI_PROP_OUTER_UP_VALID		0x0100
#define IAVF_AQ_VSI_PROP_SCHED_VALID		0x0200
	/* switch section */
	__le16	switch_id; /* 12bit id combined with flags below */
#define IAVF_AQ_VSI_SW_ID_SHIFT		0x0000
#define IAVF_AQ_VSI_SW_ID_MASK		(0xFFF << IAVF_AQ_VSI_SW_ID_SHIFT)
#define IAVF_AQ_VSI_SW_ID_FLAG_NOT_STAG	0x1000
#define IAVF_AQ_VSI_SW_ID_FLAG_ALLOW_LB	0x2000
#define IAVF_AQ_VSI_SW_ID_FLAG_LOCAL_LB	0x4000
	u8	sw_reserved[2];
	/* security section */
	u8	sec_flags;
#define IAVF_AQ_VSI_SEC_FLAG_ALLOW_DEST_OVRD	0x01
#define IAVF_AQ_VSI_SEC_FLAG_ENABLE_VLAN_CHK	0x02
#define IAVF_AQ_VSI_SEC_FLAG_ENABLE_MAC_CHK	0x04
	u8	sec_reserved;
	/* VLAN section */
	__le16	pvid; /* VLANS include priority bits */
	__le16	fcoe_pvid;
	u8	port_vlan_flags;
#define IAVF_AQ_VSI_PVLAN_MODE_SHIFT	0x00
#define IAVF_AQ_VSI_PVLAN_MODE_MASK	(0x03 << \
					 IAVF_AQ_VSI_PVLAN_MODE_SHIFT)
#define IAVF_AQ_VSI_PVLAN_MODE_TAGGED	0x01
#define IAVF_AQ_VSI_PVLAN_MODE_UNTAGGED	0x02
#define IAVF_AQ_VSI_PVLAN_MODE_ALL	0x03
#define IAVF_AQ_VSI_PVLAN_INSERT_PVID	0x04
#define IAVF_AQ_VSI_PVLAN_EMOD_SHIFT	0x03
#define IAVF_AQ_VSI_PVLAN_EMOD_MASK	(0x3 << \
					 IAVF_AQ_VSI_PVLAN_EMOD_SHIFT)
#define IAVF_AQ_VSI_PVLAN_EMOD_STR_BOTH	0x0
#define IAVF_AQ_VSI_PVLAN_EMOD_STR_UP	0x08
#define IAVF_AQ_VSI_PVLAN_EMOD_STR	0x10
#define IAVF_AQ_VSI_PVLAN_EMOD_NOTHING	0x18
	u8	pvlan_reserved[3];
	/* ingress egress up sections */
	__le32	ingress_table; /* bitmap, 3 bits per up */
#define IAVF_AQ_VSI_UP_TABLE_UP0_SHIFT	0
#define IAVF_AQ_VSI_UP_TABLE_UP0_MASK	(0x7 << \
					 IAVF_AQ_VSI_UP_TABLE_UP0_SHIFT)
#define IAVF_AQ_VSI_UP_TABLE_UP1_SHIFT	3
#define IAVF_AQ_VSI_UP_TABLE_UP1_MASK	(0x7 << \
					 IAVF_AQ_VSI_UP_TABLE_UP1_SHIFT)
#define IAVF_AQ_VSI_UP_TABLE_UP2_SHIFT	6
#define IAVF_AQ_VSI_UP_TABLE_UP2_MASK	(0x7 << \
					 IAVF_AQ_VSI_UP_TABLE_UP2_SHIFT)
#define IAVF_AQ_VSI_UP_TABLE_UP3_SHIFT	9
#define IAVF_AQ_VSI_UP_TABLE_UP3_MASK	(0x7 << \
					 IAVF_AQ_VSI_UP_TABLE_UP3_SHIFT)
#define IAVF_AQ_VSI_UP_TABLE_UP4_SHIFT	12
#define IAVF_AQ_VSI_UP_TABLE_UP4_MASK	(0x7 << \
					 IAVF_AQ_VSI_UP_TABLE_UP4_SHIFT)
#define IAVF_AQ_VSI_UP_TABLE_UP5_SHIFT	15
#define IAVF_AQ_VSI_UP_TABLE_UP5_MASK	(0x7 << \
					 IAVF_AQ_VSI_UP_TABLE_UP5_SHIFT)
#define IAVF_AQ_VSI_UP_TABLE_UP6_SHIFT	18
#define IAVF_AQ_VSI_UP_TABLE_UP6_MASK	(0x7 << \
					 IAVF_AQ_VSI_UP_TABLE_UP6_SHIFT)
#define IAVF_AQ_VSI_UP_TABLE_UP7_SHIFT	21
#define IAVF_AQ_VSI_UP_TABLE_UP7_MASK	(0x7 << \
					 IAVF_AQ_VSI_UP_TABLE_UP7_SHIFT)
	__le32	egress_table;   /* same defines as for ingress table */
	/* cascaded PV section */
	__le16	cas_pv_tag;
	u8	cas_pv_flags;
#define IAVF_AQ_VSI_CAS_PV_TAGX_SHIFT		0x00
#define IAVF_AQ_VSI_CAS_PV_TAGX_MASK		(0x03 << \
						 IAVF_AQ_VSI_CAS_PV_TAGX_SHIFT)
#define IAVF_AQ_VSI_CAS_PV_TAGX_LEAVE		0x00
#define IAVF_AQ_VSI_CAS_PV_TAGX_REMOVE		0x01
#define IAVF_AQ_VSI_CAS_PV_TAGX_COPY		0x02
#define IAVF_AQ_VSI_CAS_PV_INSERT_TAG		0x10
#define IAVF_AQ_VSI_CAS_PV_ETAG_PRUNE		0x20
#define IAVF_AQ_VSI_CAS_PV_ACCEPT_HOST_TAG	0x40
	u8	cas_pv_reserved;
	/* queue mapping section */
	__le16	mapping_flags;
#define IAVF_AQ_VSI_QUE_MAP_CONTIG	0x0
#define IAVF_AQ_VSI_QUE_MAP_NONCONTIG	0x1
	__le16	queue_mapping[16];
#define IAVF_AQ_VSI_QUEUE_SHIFT		0x0
#define IAVF_AQ_VSI_QUEUE_MASK		(0x7FF << IAVF_AQ_VSI_QUEUE_SHIFT)
	__le16	tc_mapping[8];
#define IAVF_AQ_VSI_TC_QUE_OFFSET_SHIFT	0
#define IAVF_AQ_VSI_TC_QUE_OFFSET_MASK	(0x1FF << \
					 IAVF_AQ_VSI_TC_QUE_OFFSET_SHIFT)
#define IAVF_AQ_VSI_TC_QUE_NUMBER_SHIFT	9
#define IAVF_AQ_VSI_TC_QUE_NUMBER_MASK	(0x7 << \
					 IAVF_AQ_VSI_TC_QUE_NUMBER_SHIFT)
	/* queueing option section */
	u8	queueing_opt_flags;
#define IAVF_AQ_VSI_QUE_OPT_MULTICAST_UDP_ENA	0x04
#define IAVF_AQ_VSI_QUE_OPT_UNICAST_UDP_ENA	0x08
#define IAVF_AQ_VSI_QUE_OPT_TCP_ENA	0x10
#define IAVF_AQ_VSI_QUE_OPT_FCOE_ENA	0x20
#define IAVF_AQ_VSI_QUE_OPT_RSS_LUT_PF	0x00
#define IAVF_AQ_VSI_QUE_OPT_RSS_LUT_VSI	0x40
	u8	queueing_opt_reserved[3];
	/* scheduler section */
	u8	up_enable_bits;
	u8	sched_reserved;
	/* outer up section */
	__le32	outer_up_table; /* same structure and defines as ingress tbl */
	u8	cmd_reserved[8];
	/* last 32 bytes are written by FW */
	__le16	qs_handle[8];
#define IAVF_AQ_VSI_QS_HANDLE_INVALID	0xFFFF
	__le16	stat_counter_idx;
	__le16	sched_id;
	u8	resp_reserved[12];
};

IAVF_CHECK_STRUCT_LEN(128, iavf_aqc_vsi_properties_data);

/* Get VEB Parameters (direct 0x0232)
 * uses iavf_aqc_switch_seid for the descriptor
 */
struct iavf_aqc_get_veb_parameters_completion {
	__le16	seid;
	__le16	switch_id;
	__le16	veb_flags; /* only the first/last flags from 0x0230 is valid */
	__le16	statistic_index;
	__le16	vebs_used;
	__le16	vebs_free;
	u8	reserved[4];
};

IAVF_CHECK_CMD_LENGTH(iavf_aqc_get_veb_parameters_completion);

#define IAVF_LINK_SPEED_100MB_SHIFT	0x1
#define IAVF_LINK_SPEED_1000MB_SHIFT	0x2
#define IAVF_LINK_SPEED_10GB_SHIFT	0x3
#define IAVF_LINK_SPEED_40GB_SHIFT	0x4
#define IAVF_LINK_SPEED_20GB_SHIFT	0x5
#define IAVF_LINK_SPEED_25GB_SHIFT	0x6

enum iavf_aq_link_speed {
	IAVF_LINK_SPEED_UNKNOWN	= 0,
	IAVF_LINK_SPEED_100MB	= (1 << IAVF_LINK_SPEED_100MB_SHIFT),
	IAVF_LINK_SPEED_1GB	= (1 << IAVF_LINK_SPEED_1000MB_SHIFT),
	IAVF_LINK_SPEED_10GB	= (1 << IAVF_LINK_SPEED_10GB_SHIFT),
	IAVF_LINK_SPEED_40GB	= (1 << IAVF_LINK_SPEED_40GB_SHIFT),
	IAVF_LINK_SPEED_20GB	= (1 << IAVF_LINK_SPEED_20GB_SHIFT),
	IAVF_LINK_SPEED_25GB	= (1 << IAVF_LINK_SPEED_25GB_SHIFT),
};

#define IAVF_AQ_LINK_UP_FUNCTION	0x01

/* Send to PF command (indirect 0x0801) id is only used by PF
 * Send to VF command (indirect 0x0802) id is only used by PF
 * Send to Peer PF command (indirect 0x0803)
 */
struct iavf_aqc_pf_vf_message {
	__le32	id;
	u8	reserved[4];
	__le32	addr_high;
	__le32	addr_low;
};

IAVF_CHECK_CMD_LENGTH(iavf_aqc_pf_vf_message);

/* Get CEE DCBX Oper Config (0x0A07)
 * uses the generic descriptor struct
 * returns below as indirect response
 */

#define IAVF_AQC_CEE_APP_FCOE_SHIFT	0x0
#define IAVF_AQC_CEE_APP_FCOE_MASK	(0x7 << IAVF_AQC_CEE_APP_FCOE_SHIFT)
#define IAVF_AQC_CEE_APP_ISCSI_SHIFT	0x3
#define IAVF_AQC_CEE_APP_ISCSI_MASK	(0x7 << IAVF_AQC_CEE_APP_ISCSI_SHIFT)
#define IAVF_AQC_CEE_APP_FIP_SHIFT	0x8
#define IAVF_AQC_CEE_APP_FIP_MASK	(0x7 << IAVF_AQC_CEE_APP_FIP_SHIFT)

#define IAVF_AQC_CEE_PG_STATUS_SHIFT	0x0
#define IAVF_AQC_CEE_PG_STATUS_MASK	(0x7 << IAVF_AQC_CEE_PG_STATUS_SHIFT)
#define IAVF_AQC_CEE_PFC_STATUS_SHIFT	0x3
#define IAVF_AQC_CEE_PFC_STATUS_MASK	(0x7 << IAVF_AQC_CEE_PFC_STATUS_SHIFT)
#define IAVF_AQC_CEE_APP_STATUS_SHIFT	0x8
#define IAVF_AQC_CEE_APP_STATUS_MASK	(0x7 << IAVF_AQC_CEE_APP_STATUS_SHIFT)
#define IAVF_AQC_CEE_FCOE_STATUS_SHIFT	0x8
#define IAVF_AQC_CEE_FCOE_STATUS_MASK	(0x7 << IAVF_AQC_CEE_FCOE_STATUS_SHIFT)
#define IAVF_AQC_CEE_ISCSI_STATUS_SHIFT	0xB
#define IAVF_AQC_CEE_ISCSI_STATUS_MASK	(0x7 << IAVF_AQC_CEE_ISCSI_STATUS_SHIFT)
#define IAVF_AQC_CEE_FIP_STATUS_SHIFT	0x10
#define IAVF_AQC_CEE_FIP_STATUS_MASK	(0x7 << IAVF_AQC_CEE_FIP_STATUS_SHIFT)

/* struct iavf_aqc_get_cee_dcb_cfg_v1_resp was originally defined with
 * word boundary layout issues, which the Linux compilers silently deal
 * with by adding padding, making the actual struct larger than designed.
 * However, the FW compiler for the NIC is less lenient and complains
 * about the struct.  Hence, the struct defined here has an extra byte in
 * fields reserved3 and reserved4 to directly acknowledge that padding,
 * and the new length is used in the length check macro.
 */
struct iavf_aqc_get_cee_dcb_cfg_v1_resp {
	u8	reserved1;
	u8	oper_num_tc;
	u8	oper_prio_tc[4];
	u8	reserved2;
	u8	oper_tc_bw[8];
	u8	oper_pfc_en;
	u8	reserved3[2];
	__le16	oper_app_prio;
	u8	reserved4[2];
	__le16	tlv_status;
};

IAVF_CHECK_STRUCT_LEN(0x18, iavf_aqc_get_cee_dcb_cfg_v1_resp);

struct iavf_aqc_get_cee_dcb_cfg_resp {
	u8	oper_num_tc;
	u8	oper_prio_tc[4];
	u8	oper_tc_bw[8];
	u8	oper_pfc_en;
	__le16	oper_app_prio;
	__le32	tlv_status;
	u8	reserved[12];
};

IAVF_CHECK_STRUCT_LEN(0x20, iavf_aqc_get_cee_dcb_cfg_resp);

/*	Set Local LLDP MIB (indirect 0x0A08)
 *	Used to replace the local MIB of a given LLDP agent. e.g. DCBx
 */
struct iavf_aqc_lldp_set_local_mib {
#define SET_LOCAL_MIB_AC_TYPE_DCBX_SHIFT	0
#define SET_LOCAL_MIB_AC_TYPE_DCBX_MASK	(1 << \
					SET_LOCAL_MIB_AC_TYPE_DCBX_SHIFT)
#define SET_LOCAL_MIB_AC_TYPE_LOCAL_MIB	0x0
#define SET_LOCAL_MIB_AC_TYPE_NON_WILLING_APPS_SHIFT	(1)
#define SET_LOCAL_MIB_AC_TYPE_NON_WILLING_APPS_MASK	(1 << \
				SET_LOCAL_MIB_AC_TYPE_NON_WILLING_APPS_SHIFT)
#define SET_LOCAL_MIB_AC_TYPE_NON_WILLING_APPS		0x1
	u8	type;
	u8	reserved0;
	__le16	length;
	u8	reserved1[4];
	__le32	address_high;
	__le32	address_low;
};

IAVF_CHECK_CMD_LENGTH(iavf_aqc_lldp_set_local_mib);

struct iavf_aqc_lldp_set_local_mib_resp {
#define SET_LOCAL_MIB_RESP_EVENT_TRIGGERED_MASK      0x01
	u8  status;
	u8  reserved[15];
};

IAVF_CHECK_STRUCT_LEN(0x10, iavf_aqc_lldp_set_local_mib_resp);

/*	Stop/Start LLDP Agent (direct 0x0A09)
 *	Used for stopping/starting specific LLDP agent. e.g. DCBx
 */
struct iavf_aqc_lldp_stop_start_specific_agent {
#define IAVF_AQC_START_SPECIFIC_AGENT_SHIFT	0
#define IAVF_AQC_START_SPECIFIC_AGENT_MASK \
				(1 << IAVF_AQC_START_SPECIFIC_AGENT_SHIFT)
	u8	command;
	u8	reserved[15];
};

IAVF_CHECK_CMD_LENGTH(iavf_aqc_lldp_stop_start_specific_agent);

struct iavf_aqc_get_set_rss_key {
#define IAVF_AQC_SET_RSS_KEY_VSI_VALID		(0x1 << 15)
#define IAVF_AQC_SET_RSS_KEY_VSI_ID_SHIFT	0
#define IAVF_AQC_SET_RSS_KEY_VSI_ID_MASK	(0x3FF << \
					IAVF_AQC_SET_RSS_KEY_VSI_ID_SHIFT)
	__le16	vsi_id;
	u8	reserved[6];
	__le32	addr_high;
	__le32	addr_low;
};

IAVF_CHECK_CMD_LENGTH(iavf_aqc_get_set_rss_key);

struct iavf_aqc_get_set_rss_key_data {
	u8 standard_rss_key[0x28];
	u8 extended_hash_key[0xc];
};

IAVF_CHECK_STRUCT_LEN(0x34, iavf_aqc_get_set_rss_key_data);

struct  iavf_aqc_get_set_rss_lut {
#define IAVF_AQC_SET_RSS_LUT_VSI_VALID		(0x1 << 15)
#define IAVF_AQC_SET_RSS_LUT_VSI_ID_SHIFT	0
#define IAVF_AQC_SET_RSS_LUT_VSI_ID_MASK	(0x3FF << \
					IAVF_AQC_SET_RSS_LUT_VSI_ID_SHIFT)
	__le16	vsi_id;
#define IAVF_AQC_SET_RSS_LUT_TABLE_TYPE_SHIFT	0
#define IAVF_AQC_SET_RSS_LUT_TABLE_TYPE_MASK	(0x1 << \
					IAVF_AQC_SET_RSS_LUT_TABLE_TYPE_SHIFT)

#define IAVF_AQC_SET_RSS_LUT_TABLE_TYPE_VSI	0
#define IAVF_AQC_SET_RSS_LUT_TABLE_TYPE_PF	1
	__le16	flags;
	u8	reserved[4];
	__le32	addr_high;
	__le32	addr_low;
};

IAVF_CHECK_CMD_LENGTH(iavf_aqc_get_set_rss_lut);
#endif /* _IAVF_ADMINQ_CMD_H_ */
