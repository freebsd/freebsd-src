/*
 * Copyright (c) 2017-2018 Cavium, Inc. 
 * All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
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
 *
 * $FreeBSD$
 *
 */

/****************************************************************************
 *
 * Name:        mcp_private.h
 *
 * Description: MCP private data. Located in HSI only to provide debug access
 *              for diag.
 *
 ****************************************************************************/

#ifndef MCP_PRIVATE_H
#define MCP_PRIVATE_H

#if (!defined MFW_SIM) && (!defined RECOVERY)
#include "eth.h"
#include "pmm.h"
#include "ah_eth.h"
#endif
#include "mcp_public.h"

typedef enum active_mf_mode {
	MF_MODE_SF = 0,
	MF_MODE_MF_ALLOWED,
	MF_MODE_MF_SWITCH_INDEPENDENT,
	MF_MODE_NIV
} active_mf_mode_t;

enum ov_current_cfg {
	CURR_CFG_NONE =	0,
	CURR_CFG_OS,
	CURR_CFG_VENDOR_SPEC,
	CURR_CFG_OTHER,
	CURR_CFG_VC_CLP,
	CURR_CFG_CNU,
	CURR_CFG_DCI,
	CURR_CFG_HII,
};

struct dci_info_global {
	enum ov_current_cfg current_cfg;
	u8 pci_bus_num;
	u8 boot_progress;
};

/* Resource allocation information of one resource */
struct resource_info_private {
	u16 size; /* number of allocated resources */
	u16 offset; /* Offset of the 1st resource */
	u8 flags;
};

/* Cache for resource allocation of one PF */
struct res_alloc_cache {
	u8 pf_num;
	struct resource_info_private res[RESOURCE_MAX_NUM];
};

struct pf_sb_t {
	u8 sb_for_pf_size;
	u8 sb_for_pf_offset;
	u8 sb_for_vf_size;
	u8 sb_for_vf_offset;
};

/**************************************/
/*                                    */
/*     P R I V A T E    G L O B A L   */
/*                                    */
/**************************************/
struct private_global {
	active_mf_mode_t mf_mode; /* TBD - require initialization */
	u32 exp_rom_nvm_addr;

	/* The pmm_config structure holds all active phy/link configuration */
#ifndef RECOVERY
#ifdef b900
	struct pmm_config eth_cfg;
#else
	struct ah_eth eth_cfg;
#endif
#endif

	u32 lldp_counter;

	u32 avs_init_timestamp;

	u32 seconds_since_mcp_reset;

	u32 last_malloc_dir_used_timestamp;
#define MAX_USED_DIR_ALLOWED_TIME (3) /* Seconds */

	u32 drv_nvm_state;
	/* Per PF bitmask */
#define DRV_NVM_STATE_IN_PROGRESS_MASK		(0x0000ffff)
#define DRV_NVM_STATE_IN_PROGRESS_OFFSET	(0)

	u32 storm_fw_ver;

	/* OneView data*/
	struct dci_info_global dci_global;

	/* Resource allocation cached data */
	struct res_alloc_cache res_alloc;
#define G_RES_ALLOC_P	(&g_spad.private_data.global.res_alloc)
	u32 resource_max_values[RESOURCE_MAX_NUM];
};

/**************************************/
/*                                    */
/*     P R I V A T E    P A T H       */
/*                                    */
/**************************************/
struct private_path {
	u32 recovery_countdown; /* Counting down 2 seconds, using TMR3 */
#define RECOVERY_MAX_COUNTDOWN_SECONDS 2

	u32 drv_load_vars; /* When the seconds_since_mcp_reset gets here */
#define DRV_LOAD_TIMEOUT_MASK			0x0000ffff
#define DRV_LOAD_TIMEOUT_SHIFT			0
#define DRV_LOAD_NEED_FORCE_MASK		0xffff0000
#define DRV_LOAD_NEED_FORCE_SHIFT		16
	struct load_rsp_stc drv_load_params;
};


/**************************************/
/*                                    */
/*     P R I V A T E    P O R T       */
/*                                    */
/**************************************/
struct drv_port_info_t {
	u32_t port_state;
#define DRV_STATE_LINK_LOCK_FLAG                    0x00000001
#define DRV_WAIT_DBG_PRN                            0x00000002

	/* There are maximum 8 PFs per port */
#define DRV_STATE_LOADED_MASK                       0x0000ff00
#define DRV_STATE_LOADED_SHIFT                      8

#define DRV_STATE_PF_TRANSITION_MASK                0x00ff0000
#define DRV_STATE_PF_TRANSITION_SHIFT               16

#define DRV_STATE_PF_PHY_INIT_MASK	                 0xff000000
#define DRV_STATE_PF_PHY_INIT_SHIFT                 24
};

typedef enum _lldp_subscriber_e {
	LLDP_SUBSCRIBER_MANDATORY = 0,
	LLDP_SUBSCRIBER_DCBX_IEEE,
	LLDP_SUBSCRIBER_DCBX_CEE,
	LLDP_SUBSCRIBER_EEE,
	LLDP_SUBSCRIBER_DCI,
	MAX_SUBSCRIBERS
} lldp_subscriber_e;

typedef struct {
	u16 valid;
	u16 type_len;
#define LLDP_LEN_MASK           (0x01ff)
#define LLDP_LEN_SHIFT          (0)
#define LLDP_TYPE_MASK          (0xfe00)
#define LLDP_TYPE_SHIFT         (9)
	u8 *value_p;
} tlv_s;

typedef u16(*lldp_prepare_tlv_func)(u8 port, lldp_agent_e lldp_agent, u8 *buffer);

typedef struct {
	u16 valid;
	lldp_prepare_tlv_func func;
} subscriber_callback_send_s;

typedef u8(*lldp_process_func)(u8 port, u8 num, u8 **tlvs);

#define MAX_NUM_SUBTYPES	4
typedef struct {
	u8 valid;
	u8 oui[3];
	u8 subtype_list[MAX_NUM_SUBTYPES];
	u8 num_subtypes;
	lldp_process_func func;
} subscriber_callback_receive_s;

#define MAX_ETH_HEADER      14  /* TODO: to be extended per requirements */
#define MAX_PACKET_SIZE     (1516)  /* So it can be divided by 4 */
#define LLDP_CHASSIS_ID_TLV_LEN     7
#define LLDP_PORT_ID_TLV_LEN     7
#define MAX_TLV_BUFFER		128 /* In dwords. 512 in bytes*/
typedef struct {
	u16 len;
	u8 header[MAX_ETH_HEADER];
} lldp_eth_header_s;

typedef struct {
	struct lldp_config_params_s lldp_config_params;
	u16 lldp_ttl;
	u8 lldp_cur_credit;
	subscriber_callback_send_s subscriber_callback_send[MAX_SUBSCRIBERS];
	lldp_eth_header_s lldp_eth_header;
	u32 lldp_time_to_send;
	u32 lldp_ttl_expired;
	u32 lldp_sent;
	u8 first_lldp;
	subscriber_callback_receive_s subscriber_callback_receive[MAX_SUBSCRIBERS];
} lldp_params_s;

#define MAX_TLVS		20
typedef struct {
	u8 current_received_tlv_index;
	u8 *received_tlvs[MAX_TLVS];
} lldp_receive_data_s;

#define MAX_REGISTERED_TLVS	6

typedef struct {
	u32 config; /* Uses same defines as local config plus some more below*/
#define DCBX_MODE_MASK				0x00000010
#define DCBX_MODE_SHIFT				4
#define DCBX_MODE_DRIVER			0
#define DCBX_MODE_DEFAULT			1
#define DCBX_CHANGED_MASK			0x00000f00
#define DCBX_CHANGED_SHIFT			8
#define DCBX_CONTROL_CHANGED_MASK		0x00000100
#define DCBX_CONTROL_CHANGED_SHIFT		8
#define DCBX_PFC_CHANGED_MASK			0x00000200
#define DCBX_PFC_CHANGED_SHIFT			9
#define DCBX_ETS_CHANGED_MASK			0x00000400
#define DCBX_ETS_CHANGED_SHIFT			10
#define DCBX_APP_CHANGED_MASK			0x00000800
#define DCBX_APP_CHANGED_SHIFT			11

	u32 seq_no;
	u32 ack_no;
	u32 received_seq_no;
	u8 tc_map[8];
	u8 num_used_tcs;
} dcbx_state_s;

#ifdef CONFIG_HP_DCI_SUPPORT
struct dci_info_port {
	u32 config;
#define DCI_PORT_CFG_ENABLE_SHIFT		(0)
#define DCI_PORT_CFG_ENABLE_MASK		(1 << DCI_PORT_CFG_ENABLE_SHIFT)
#define DCI_PORT_CFG_ENABLE_DIAG_SHIFT		(1)
#define DCI_PORT_CFG_ENABLE_DIAG_MASK		(1 << DCI_PORT_CFG_ENABLE_DIAG_SHIFT)
#define DCI_PORT_CFG_DIAG_L_LOOP_SHIFT		(2)
#define DCI_PORT_CFG_DIAG_L_LOOP_MASK		(1 << DCI_PORT_CFG_DIAG_L_LOOP_SHIFT)
#define DCI_PORT_CFG_DIAG_R_LOOP_SHIFT		(3)
#define DCI_PORT_CFG_DIAG_R_LOOP_MASK		(1 << DCI_PORT_CFG_DIAG_R_LOOP_SHIFT)

};
#endif

struct private_port {
	struct drv_port_info_t port_info;
	active_mf_mode_t mf_mode;
	u32 prev_link_change_count;
	/* LLDP structures */
	lldp_params_s lldp_params[LLDP_MAX_LLDP_AGENTS];
	lldp_receive_data_s lldp_receive_data[MAX_SUBSCRIBERS];

	/* DCBX */
	dcbx_state_s dcbx_state;

	u32 net_buffer[MAX_PACKET_SIZE / 4]; /* Buffer to send any packet to network */

	/* time stamp of the end of NIG drain time for the TX drain */
	u32 nig_drain_end_ts;
	/* time stamp of the end of NIG drain time for the TC pause drain, this timer is used togther for all TC */
	u32 nig_drain_tc_end_ts;
	u32 tc_drain_en_bitmap;
	u32 recv_lldp_tlvs[LLDP_MAX_LLDP_AGENTS][MAX_TLV_BUFFER];
	tlv_s lldp_core_tlv_desc[LLDP_MAX_LLDP_AGENTS][MAX_REGISTERED_TLVS];
	u8 current_core_tlv_num[LLDP_MAX_LLDP_AGENTS];
	struct mcp_mac lldp_mac;
#ifdef CONFIG_HP_DCI_SUPPORT
	struct dci_info_port dci_port;
#endif
	u32 temperature;

};

/**************************************/
/*                                    */
/*     P R I V A T E    F U N C       */
/*                                    */
/**************************************/
struct drv_func_info_t {
	u32_t func_state;
#define DRV_STATE_UNKNOWN                           0x00000000
#define DRV_STATE_UNLOADED                          0x00000001
#define DRV_STATE_D3                                0x00000004

#define DRV_STATE_PRESENT_FLAG                      0x00000100
#define DRV_STATE_RUNNING                          (0x00000002 | DRV_STATE_PRESENT_FLAG)

#define DRV_STATE_NOT_RESPONDING                    0x00000003 /* Will result with non-zero value when compared with DRV_STATE_RUNNING or with DRV_STATE_UNLOADED */
#define DRV_STATE_BACK_AFTER_TO                    (DRV_STATE_NOT_RESPONDING | DRV_STATE_PRESENT_FLAG)

#define DRV_STATE_DIAG                             (0x00000010 | DRV_STATE_PRESENT_FLAG)

#define DRV_STATE_TRANSITION_FLAG                   0x00001000
#define DRV_STATE_LOADING_TRANSITION               (DRV_STATE_TRANSITION_FLAG | DRV_STATE_PRESENT_FLAG)
#define DRV_STATE_UNLOADING_TRANSITION             (DRV_STATE_TRANSITION_FLAG | DRV_STATE_PRESENT_FLAG | DRV_STATE_UNLOADED)

	u32_t driver_last_activity;

	u32_t wol_mac_addr[2];
	u32_t drv_feature_support; /* See DRV_MB_PARAM_FEATURE_SUPPORT_FUNC_* */

	u8_t unload_wol_param; /* See drv_mb_param */
	u8_t eswitch_mode;
};

struct dci_info_func {
	u8 config;
#define DCI_FUNC_CFG_FNIC_ENABLE_SHIFT		(0)
#define DCI_FUNC_CFG_FNIC_ENABLE_MASK		(1 << DCI_FUNC_CFG_FNIC_ENABLE_SHIFT)
#define DCI_FUNC_CFG_OS_MTU_OVERRIDE_SHIFT	(1)
#define DCI_FUNC_CFG_OS_MTU_OVERRIDE_MASK	(1 << DCI_FUNC_CFG_OS_MTU_OVERRIDE_SHIFT)
#define DCI_FUNC_CFG_DIAG_WOL_ENABLE_SHIFT	(2)
#define DCI_FUNC_CFG_DIAG_WOL_ENABLE_MASK	(1 << DCI_FUNC_CFG_DIAG_WOL_ENABLE_SHIFT)

	u8 drv_state;
	u16 fcoe_cvid;
	u8 fcoe_fabric_name[8];
};

struct private_func {
	struct drv_func_info_t func_info;
	u32 init_hw_page;
	u32 num_of_msix;
	struct pf_sb_t sb;
	struct dci_info_func dci_func;
};


/**************************************/
/*                                    */
/*     P R I V A T E    D A T A       */
/*                                    */
/**************************************/
struct mcp_private_data {
	/* Basically no need for section offsets here, since this is private data.
	 * TBD - should consider adding section offsets if we want diag to parse this correctly !!
	 */
	struct private_global global;
	struct private_path path[MCP_GLOB_PATH_MAX];
	struct private_port port[MCP_GLOB_PORT_MAX];
	struct private_func func[MCP_GLOB_FUNC_MAX];

};
#endif /* MCP_PRIVATE_H */
