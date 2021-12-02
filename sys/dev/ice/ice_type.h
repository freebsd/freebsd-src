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

#ifndef _ICE_TYPE_H_
#define _ICE_TYPE_H_

#define ETH_ALEN	6

#define ETH_HEADER_LEN	14

#define BIT(a) (1UL << (a))
#ifndef BIT_ULL
#define BIT_ULL(a) (1ULL << (a))
#endif /* BIT_ULL */

#define BITS_PER_BYTE	8

#define _FORCE_

#define ICE_BYTES_PER_WORD	2
#define ICE_BYTES_PER_DWORD	4
#define ICE_MAX_TRAFFIC_CLASS	8

#ifndef MIN_T
#define MIN_T(_t, _a, _b)	min((_t)(_a), (_t)(_b))
#endif

#define IS_ASCII(_ch)	((_ch) < 0x80)

#define STRUCT_HACK_VAR_LEN
/**
 * ice_struct_size - size of struct with C99 flexible array member
 * @ptr: pointer to structure
 * @field: flexible array member (last member of the structure)
 * @num: number of elements of that flexible array member
 */
#define ice_struct_size(ptr, field, num) \
	(sizeof(*(ptr)) + sizeof(*(ptr)->field) * (num))

#define FLEX_ARRAY_SIZE(_ptr, _mem, cnt) ((cnt) * sizeof(_ptr->_mem[0]))

#include "ice_status.h"
#include "ice_hw_autogen.h"
#include "ice_devids.h"
#include "ice_osdep.h"
#include "ice_bitops.h" /* Must come before ice_controlq.h */
#include "ice_controlq.h"
#include "ice_lan_tx_rx.h"
#include "ice_flex_type.h"
#include "ice_protocol_type.h"
#include "ice_vlan_mode.h"
#include "ice_fwlog.h"

static inline bool ice_is_tc_ena(ice_bitmap_t bitmap, u8 tc)
{
	return !!(bitmap & BIT(tc));
}

/**
 * DIV_S64 - Divide signed 64-bit value with signed 64-bit divisor
 * @dividend: value to divide
 * @divisor: value to divide by
 *
 * Use DIV_S64 for any 64-bit divide which operates on signed 64-bit dividends.
 * Do not use this for unsigned 64-bit dividends as it will not produce
 * correct results if the dividend is larger than S64_MAX.
 */
static inline s64 DIV_S64(s64 dividend, s64 divisor)
{
	return dividend / divisor;
}

/**
 * DIV_U64 - Divide unsigned 64-bit value by unsigned 64-bit divisor
 * @dividend: value to divide
 * @divisor: value to divide by
 *
 * Use DIV_U64 for any 64-bit divide which operates on unsigned 64-bit
 * dividends. Do not use this for signed 64-bit dividends as it will not
 * handle negative values correctly.
 */
static inline u64 DIV_U64(u64 dividend, u64 divisor)
{
	return dividend / divisor;
}

static inline u64 round_up_64bit(u64 a, u32 b)
{
	return DIV_U64(((a) + (b) / 2), (b));
}

static inline u32 ice_round_to_num(u32 N, u32 R)
{
	return ((((N) % (R)) < ((R) / 2)) ? (((N) / (R)) * (R)) :
		((((N) + (R) - 1) / (R)) * (R)));
}

/* Driver always calls main vsi_handle first */
#define ICE_MAIN_VSI_HANDLE		0

/* Switch from ms to the 1usec global time (this is the GTIME resolution) */
#define ICE_MS_TO_GTIME(time)		((time) * 1000)

/* Data type manipulation macros. */
#define ICE_HI_DWORD(x)		((u32)((((x) >> 16) >> 16) & 0xFFFFFFFF))
#define ICE_LO_DWORD(x)		((u32)((x) & 0xFFFFFFFF))
#define ICE_HI_WORD(x)		((u16)(((x) >> 16) & 0xFFFF))
#define ICE_LO_WORD(x)		((u16)((x) & 0xFFFF))

/* debug masks - set these bits in hw->debug_mask to control output */
#define ICE_DBG_TRACE		BIT_ULL(0) /* for function-trace only */
#define ICE_DBG_INIT		BIT_ULL(1)
#define ICE_DBG_RELEASE		BIT_ULL(2)
#define ICE_DBG_FW_LOG		BIT_ULL(3)
#define ICE_DBG_LINK		BIT_ULL(4)
#define ICE_DBG_PHY		BIT_ULL(5)
#define ICE_DBG_QCTX		BIT_ULL(6)
#define ICE_DBG_NVM		BIT_ULL(7)
#define ICE_DBG_LAN		BIT_ULL(8)
#define ICE_DBG_FLOW		BIT_ULL(9)
#define ICE_DBG_DCB		BIT_ULL(10)
#define ICE_DBG_DIAG		BIT_ULL(11)
#define ICE_DBG_FD		BIT_ULL(12)
#define ICE_DBG_SW		BIT_ULL(13)
#define ICE_DBG_SCHED		BIT_ULL(14)

#define ICE_DBG_RDMA		BIT_ULL(15)
#define ICE_DBG_PKG		BIT_ULL(16)
#define ICE_DBG_RES		BIT_ULL(17)
#define ICE_DBG_AQ_MSG		BIT_ULL(24)
#define ICE_DBG_AQ_DESC		BIT_ULL(25)
#define ICE_DBG_AQ_DESC_BUF	BIT_ULL(26)
#define ICE_DBG_AQ_CMD		BIT_ULL(27)
#define ICE_DBG_AQ		(ICE_DBG_AQ_MSG		| \
				 ICE_DBG_AQ_DESC	| \
				 ICE_DBG_AQ_DESC_BUF	| \
				 ICE_DBG_AQ_CMD)
#define ICE_DBG_PARSER		BIT_ULL(28)

#define ICE_DBG_USER		BIT_ULL(31)
#define ICE_DBG_ALL		0xFFFFFFFFFFFFFFFFULL

#define IS_UNICAST_ETHER_ADDR(addr) \
	((bool)((((u8 *)(addr))[0] % ((u8)0x2)) == 0))

#define IS_MULTICAST_ETHER_ADDR(addr) \
	((bool)((((u8 *)(addr))[0] % ((u8)0x2)) == 1))

/* Check whether an address is broadcast. */
#define IS_BROADCAST_ETHER_ADDR(addr)	\
	((bool)((((u16 *)(addr))[0] == ((u16)0xffff))))

#define IS_ZERO_ETHER_ADDR(addr) \
	(((bool)((((u16 *)(addr))[0] == ((u16)0x0)))) && \
	 ((bool)((((u16 *)(addr))[1] == ((u16)0x0)))) && \
	 ((bool)((((u16 *)(addr))[2] == ((u16)0x0)))))

#ifndef IS_ETHER_ADDR_EQUAL
#define IS_ETHER_ADDR_EQUAL(addr1, addr2) \
	(((bool)((((u16 *)(addr1))[0] == ((u16 *)(addr2))[0]))) && \
	 ((bool)((((u16 *)(addr1))[1] == ((u16 *)(addr2))[1]))) && \
	 ((bool)((((u16 *)(addr1))[2] == ((u16 *)(addr2))[2]))))
#endif

enum ice_aq_res_ids {
	ICE_NVM_RES_ID = 1,
	ICE_SPD_RES_ID,
	ICE_CHANGE_LOCK_RES_ID,
	ICE_GLOBAL_CFG_LOCK_RES_ID
};

/* FW update timeout definitions are in milliseconds */
#define ICE_NVM_TIMEOUT			180000
#define ICE_CHANGE_LOCK_TIMEOUT		1000
#define ICE_GLOBAL_CFG_LOCK_TIMEOUT	3000

enum ice_aq_res_access_type {
	ICE_RES_READ = 1,
	ICE_RES_WRITE
};

struct ice_driver_ver {
	u8 major_ver;
	u8 minor_ver;
	u8 build_ver;
	u8 subbuild_ver;
	u8 driver_string[32];
};

enum ice_fc_mode {
	ICE_FC_NONE = 0,
	ICE_FC_RX_PAUSE,
	ICE_FC_TX_PAUSE,
	ICE_FC_FULL,
	ICE_FC_AUTO,
	ICE_FC_PFC,
	ICE_FC_DFLT
};

enum ice_phy_cache_mode {
	ICE_FC_MODE = 0,
	ICE_SPEED_MODE,
	ICE_FEC_MODE
};

enum ice_fec_mode {
	ICE_FEC_NONE = 0,
	ICE_FEC_RS,
	ICE_FEC_BASER,
	ICE_FEC_AUTO
};

struct ice_phy_cache_mode_data {
	union {
		enum ice_fec_mode curr_user_fec_req;
		enum ice_fc_mode curr_user_fc_req;
		u16 curr_user_speed_req;
	} data;
};

enum ice_set_fc_aq_failures {
	ICE_SET_FC_AQ_FAIL_NONE = 0,
	ICE_SET_FC_AQ_FAIL_GET,
	ICE_SET_FC_AQ_FAIL_SET,
	ICE_SET_FC_AQ_FAIL_UPDATE
};

/* These are structs for managing the hardware information and the operations */
/* MAC types */
enum ice_mac_type {
	ICE_MAC_UNKNOWN = 0,
	ICE_MAC_VF,
	ICE_MAC_E810,
	ICE_MAC_GENERIC,
};

/* Media Types */
enum ice_media_type {
	ICE_MEDIA_UNKNOWN = 0,
	ICE_MEDIA_FIBER,
	ICE_MEDIA_BASET,
	ICE_MEDIA_BACKPLANE,
	ICE_MEDIA_DA,
	ICE_MEDIA_AUI,
};

/* Software VSI types. */
enum ice_vsi_type {
	ICE_VSI_PF = 0,
	ICE_VSI_VF = 1,
	ICE_VSI_LB = 6,
};

struct ice_link_status {
	/* Refer to ice_aq_phy_type for bits definition */
	u64 phy_type_low;
	u64 phy_type_high;
	u8 topo_media_conflict;
	u16 max_frame_size;
	u16 link_speed;
	u16 req_speeds;
	u8 link_cfg_err;
	u8 lse_ena;	/* Link Status Event notification */
	u8 link_info;
	u8 an_info;
	u8 ext_info;
	u8 fec_info;
	u8 pacing;
	/* Refer to #define from module_type[ICE_MODULE_TYPE_TOTAL_BYTE] of
	 * ice_aqc_get_phy_caps structure
	 */
	u8 module_type[ICE_MODULE_TYPE_TOTAL_BYTE];
};

/* Different data queue types: These are mainly for SW consumption. */
enum ice_q {
	ICE_DATA_Q_DOORBELL,
	ICE_DATA_Q_CMPL,
	ICE_DATA_Q_QUANTA,
	ICE_DATA_Q_RX,
	ICE_DATA_Q_TX,
};

/* Different reset sources for which a disable queue AQ call has to be made in
 * order to clean the Tx scheduler as a part of the reset
 */
enum ice_disq_rst_src {
	ICE_NO_RESET = 0,
	ICE_VM_RESET,
	ICE_VF_RESET,
};

/* PHY info such as phy_type, etc... */
struct ice_phy_info {
	struct ice_link_status link_info;
	struct ice_link_status link_info_old;
	u64 phy_type_low;
	u64 phy_type_high;
	enum ice_media_type media_type;
	u8 get_link_info;
	/* Please refer to struct ice_aqc_get_link_status_data to get
	 * detail of enable bit in curr_user_speed_req
	 */
	u16 curr_user_speed_req;
	enum ice_fec_mode curr_user_fec_req;
	enum ice_fc_mode curr_user_fc_req;
	struct ice_aqc_set_phy_cfg_data curr_user_phy_cfg;
};

#define ICE_MAX_NUM_MIRROR_RULES	64

/* Common HW capabilities for SW use */
struct ice_hw_common_caps {
	/* Write CSR protection */
	u64 wr_csr_prot;
	u32 switching_mode;
	/* switching mode supported - EVB switching (including cloud) */
#define ICE_NVM_IMAGE_TYPE_EVB		0x0

	/* Manageablity mode & supported protocols over MCTP */
	u32 mgmt_mode;
#define ICE_MGMT_MODE_PASS_THRU_MODE_M		0xF
#define ICE_MGMT_MODE_CTL_INTERFACE_M		0xF0
#define ICE_MGMT_MODE_REDIR_SB_INTERFACE_M	0xF00

	u32 mgmt_protocols_mctp;
#define ICE_MGMT_MODE_PROTO_RSVD	BIT(0)
#define ICE_MGMT_MODE_PROTO_PLDM	BIT(1)
#define ICE_MGMT_MODE_PROTO_OEM		BIT(2)
#define ICE_MGMT_MODE_PROTO_NC_SI	BIT(3)

	u32 os2bmc;
	u32 valid_functions;
	/* DCB capabilities */
	u32 active_tc_bitmap;
	u32 maxtc;

	/* RSS related capabilities */
	u32 rss_table_size;		/* 512 for PFs and 64 for VFs */
	u32 rss_table_entry_width;	/* RSS Entry width in bits */

	/* Tx/Rx queues */
	u32 num_rxq;			/* Number/Total Rx queues */
	u32 rxq_first_id;		/* First queue ID for Rx queues */
	u32 num_txq;			/* Number/Total Tx queues */
	u32 txq_first_id;		/* First queue ID for Tx queues */

	/* MSI-X vectors */
	u32 num_msix_vectors;
	u32 msix_vector_first_id;

	/* Max MTU for function or device */
	u32 max_mtu;

	/* WOL related */
	u32 num_wol_proxy_fltr;
	u32 wol_proxy_vsi_seid;

	/* LED/SDP pin count */
	u32 led_pin_num;
	u32 sdp_pin_num;

	/* LED/SDP - Supports up to 12 LED pins and 8 SDP signals */
#define ICE_MAX_SUPPORTED_GPIO_LED	12
#define ICE_MAX_SUPPORTED_GPIO_SDP	8
	u8 led[ICE_MAX_SUPPORTED_GPIO_LED];
	u8 sdp[ICE_MAX_SUPPORTED_GPIO_SDP];

	/* SR-IOV virtualization */
	u8 sr_iov_1_1;			/* SR-IOV enabled */

	/* EVB capabilities */
	u8 evb_802_1_qbg;		/* Edge Virtual Bridging */
	u8 evb_802_1_qbh;		/* Bridge Port Extension */

	u8 dcb;
	u8 iscsi;
	u8 mgmt_cem;
	u8 iwarp;

	/* WoL and APM support */
#define ICE_WOL_SUPPORT_M		BIT(0)
#define ICE_ACPI_PROG_MTHD_M		BIT(1)
#define ICE_PROXY_SUPPORT_M		BIT(2)
	u8 apm_wol_support;
	u8 acpi_prog_mthd;
	u8 proxy_support;
	bool sec_rev_disabled;
	bool update_disabled;
	bool nvm_unified_update;
#define ICE_NVM_MGMT_SEC_REV_DISABLED		BIT(0)
#define ICE_NVM_MGMT_UPDATE_DISABLED		BIT(1)
#define ICE_NVM_MGMT_UNIFIED_UPD_SUPPORT	BIT(3)
	/* PCIe reset avoidance */
	bool pcie_reset_avoidance; /* false: not supported, true: supported */
	/* Post update reset restriction */
	bool reset_restrict_support; /* false: not supported, true: supported */

	/* External topology device images within the NVM */
#define ICE_EXT_TOPO_DEV_IMG_COUNT	4
	u32 ext_topo_dev_img_ver_high[ICE_EXT_TOPO_DEV_IMG_COUNT];
	u32 ext_topo_dev_img_ver_low[ICE_EXT_TOPO_DEV_IMG_COUNT];
	u8 ext_topo_dev_img_part_num[ICE_EXT_TOPO_DEV_IMG_COUNT];
#define ICE_EXT_TOPO_DEV_IMG_PART_NUM_S	8
#define ICE_EXT_TOPO_DEV_IMG_PART_NUM_M	\
		MAKEMASK(0xFF, ICE_EXT_TOPO_DEV_IMG_PART_NUM_S)
	bool ext_topo_dev_img_load_en[ICE_EXT_TOPO_DEV_IMG_COUNT];
#define ICE_EXT_TOPO_DEV_IMG_LOAD_EN	BIT(0)
	bool ext_topo_dev_img_prog_en[ICE_EXT_TOPO_DEV_IMG_COUNT];
#define ICE_EXT_TOPO_DEV_IMG_PROG_EN	BIT(1)
};

/* Function specific capabilities */
struct ice_hw_func_caps {
	struct ice_hw_common_caps common_cap;
	u32 num_allocd_vfs;		/* Number of allocated VFs */
	u32 vf_base_id;			/* Logical ID of the first VF */
	u32 guar_num_vsi;
};

/* Device wide capabilities */
struct ice_hw_dev_caps {
	struct ice_hw_common_caps common_cap;
	u32 num_vfs_exposed;		/* Total number of VFs exposed */
	u32 num_vsi_allocd_to_host;	/* Excluding EMP VSI */
	u32 num_funcs;
};

/* Information about MAC such as address, etc... */
struct ice_mac_info {
	u8 lan_addr[ETH_ALEN];
	u8 perm_addr[ETH_ALEN];
	u8 port_addr[ETH_ALEN];
	u8 wol_addr[ETH_ALEN];
};

/* PCI bus types */
enum ice_bus_type {
	ice_bus_unknown = 0,
	ice_bus_pci_express,
	ice_bus_embedded, /* Is device Embedded versus card */
	ice_bus_reserved
};

/* PCI bus speeds */
enum ice_pcie_bus_speed {
	ice_pcie_speed_unknown	= 0xff,
	ice_pcie_speed_2_5GT	= 0x14,
	ice_pcie_speed_5_0GT	= 0x15,
	ice_pcie_speed_8_0GT	= 0x16,
	ice_pcie_speed_16_0GT	= 0x17
};

/* PCI bus widths */
enum ice_pcie_link_width {
	ice_pcie_lnk_width_resrv	= 0x00,
	ice_pcie_lnk_x1			= 0x01,
	ice_pcie_lnk_x2			= 0x02,
	ice_pcie_lnk_x4			= 0x04,
	ice_pcie_lnk_x8			= 0x08,
	ice_pcie_lnk_x12		= 0x0C,
	ice_pcie_lnk_x16		= 0x10,
	ice_pcie_lnk_x32		= 0x20,
	ice_pcie_lnk_width_unknown	= 0xff,
};

/* Reset types used to determine which kind of reset was requested. These
 * defines match what the RESET_TYPE field of the GLGEN_RSTAT register.
 * ICE_RESET_PFR does not match any RESET_TYPE field in the GLGEN_RSTAT register
 * because its reset source is different than the other types listed.
 */
enum ice_reset_req {
	ICE_RESET_POR	= 0,
	ICE_RESET_INVAL	= 0,
	ICE_RESET_CORER	= 1,
	ICE_RESET_GLOBR	= 2,
	ICE_RESET_EMPR	= 3,
	ICE_RESET_PFR	= 4,
};

/* Bus parameters */
struct ice_bus_info {
	enum ice_pcie_bus_speed speed;
	enum ice_pcie_link_width width;
	enum ice_bus_type type;
	u16 domain_num;
	u16 device;
	u8 func;
	u8 bus_num;
};

/* Flow control (FC) parameters */
struct ice_fc_info {
	enum ice_fc_mode current_mode;	/* FC mode in effect */
	enum ice_fc_mode req_mode;	/* FC mode requested by caller */
};

/* Option ROM version information */
struct ice_orom_info {
	u8 major;			/* Major version of OROM */
	u8 patch;			/* Patch version of OROM */
	u16 build;			/* Build version of OROM */
	u32 srev;			/* Security revision */
};

/* NVM version information */
struct ice_nvm_info {
	u32 eetrack;
	u32 srev;
	u8 major;
	u8 minor;
};

/* Minimum Security Revision information */
struct ice_minsrev_info {
	u32 nvm;
	u32 orom;
	u8 nvm_valid : 1;
	u8 orom_valid : 1;
};

/* netlist version information */
struct ice_netlist_info {
	u32 major;			/* major high/low */
	u32 minor;			/* minor high/low */
	u32 type;			/* type high/low */
	u32 rev;			/* revision high/low */
	u32 hash;			/* SHA-1 hash word */
	u16 cust_ver;			/* customer version */
};

/* Enumeration of possible flash banks for the NVM, OROM, and Netlist modules
 * of the flash image.
 */
enum ice_flash_bank {
	ICE_INVALID_FLASH_BANK,
	ICE_1ST_FLASH_BANK,
	ICE_2ND_FLASH_BANK,
};

/* Enumeration of which flash bank is desired to read from, either the active
 * bank or the inactive bank. Used to abstract 1st and 2nd bank notion from
 * code which just wants to read the active or inactive flash bank.
 */
enum ice_bank_select {
	ICE_ACTIVE_FLASH_BANK,
	ICE_INACTIVE_FLASH_BANK,
};

/* information for accessing NVM, OROM, and Netlist flash banks */
struct ice_bank_info {
	u32 nvm_ptr;				/* Pointer to 1st NVM bank */
	u32 nvm_size;				/* Size of NVM bank */
	u32 orom_ptr;				/* Pointer to 1st OROM bank */
	u32 orom_size;				/* Size of OROM bank */
	u32 netlist_ptr;			/* Pointer to 1st Netlist bank */
	u32 netlist_size;			/* Size of Netlist bank */
	enum ice_flash_bank nvm_bank;		/* Active NVM bank */
	enum ice_flash_bank orom_bank;		/* Active OROM bank */
	enum ice_flash_bank netlist_bank;	/* Active Netlist bank */
};

/* Flash Chip Information */
struct ice_flash_info {
	struct ice_orom_info orom;	/* Option ROM version info */
	struct ice_nvm_info nvm;	/* NVM version information */
	struct ice_netlist_info netlist;/* Netlist version info */
	struct ice_bank_info banks;	/* Flash Bank information */
	u16 sr_words;			/* Shadow RAM size in words */
	u32 flash_size;			/* Size of available flash in bytes */
	u8 blank_nvm_mode;		/* is NVM empty (no FW present) */
};

struct ice_link_default_override_tlv {
	u8 options;
#define ICE_LINK_OVERRIDE_OPT_M		0x3F
#define ICE_LINK_OVERRIDE_STRICT_MODE	BIT(0)
#define ICE_LINK_OVERRIDE_EPCT_DIS	BIT(1)
#define ICE_LINK_OVERRIDE_PORT_DIS	BIT(2)
#define ICE_LINK_OVERRIDE_EN		BIT(3)
#define ICE_LINK_OVERRIDE_AUTO_LINK_DIS	BIT(4)
#define ICE_LINK_OVERRIDE_EEE_EN	BIT(5)
	u8 phy_config;
#define ICE_LINK_OVERRIDE_PHY_CFG_S	8
#define ICE_LINK_OVERRIDE_PHY_CFG_M	(0xC3 << ICE_LINK_OVERRIDE_PHY_CFG_S)
#define ICE_LINK_OVERRIDE_PAUSE_M	0x3
#define ICE_LINK_OVERRIDE_LESM_EN	BIT(6)
#define ICE_LINK_OVERRIDE_AUTO_FEC_EN	BIT(7)
	u8 fec_options;
#define ICE_LINK_OVERRIDE_FEC_OPT_M	0xFF
	u8 rsvd1;
	u64 phy_type_low;
	u64 phy_type_high;
};

#define ICE_NVM_VER_LEN	32

/* Max number of port to queue branches w.r.t topology */
#define ICE_TXSCHED_MAX_BRANCHES ICE_MAX_TRAFFIC_CLASS

#define ice_for_each_traffic_class(_i)	\
	for ((_i) = 0; (_i) < ICE_MAX_TRAFFIC_CLASS; (_i)++)

/* ICE_DFLT_AGG_ID means that all new VM(s)/VSI node connects
 * to driver defined policy for default aggregator
 */
#define ICE_INVAL_TEID 0xFFFFFFFF
#define ICE_DFLT_AGG_ID 0

struct ice_sched_node {
	struct ice_sched_node *parent;
	struct ice_sched_node *sibling; /* next sibling in the same layer */
	struct ice_sched_node **children;
	struct ice_aqc_txsched_elem_data info;
	u32 agg_id;			/* aggregator group ID */
	u16 vsi_handle;
	u8 in_use;			/* suspended or in use */
	u8 tx_sched_layer;		/* Logical Layer (1-9) */
	u8 num_children;
	u8 tc_num;
	u8 owner;
#define ICE_SCHED_NODE_OWNER_LAN	0
#define ICE_SCHED_NODE_OWNER_AE		1
#define ICE_SCHED_NODE_OWNER_RDMA	2
};

/* Access Macros for Tx Sched Elements data */
#define ICE_TXSCHED_GET_NODE_TEID(x) LE32_TO_CPU((x)->info.node_teid)
#define ICE_TXSCHED_GET_PARENT_TEID(x) LE32_TO_CPU((x)->info.parent_teid)
#define ICE_TXSCHED_GET_CIR_RL_ID(x)	\
	LE16_TO_CPU((x)->info.cir_bw.bw_profile_idx)
#define ICE_TXSCHED_GET_EIR_RL_ID(x)	\
	LE16_TO_CPU((x)->info.eir_bw.bw_profile_idx)
#define ICE_TXSCHED_GET_SRL_ID(x) LE16_TO_CPU((x)->info.srl_id)
#define ICE_TXSCHED_GET_CIR_BWALLOC(x)	\
	LE16_TO_CPU((x)->info.cir_bw.bw_alloc)
#define ICE_TXSCHED_GET_EIR_BWALLOC(x)	\
	LE16_TO_CPU((x)->info.eir_bw.bw_alloc)

struct ice_sched_rl_profile {
	u32 rate; /* In Kbps */
	struct ice_aqc_rl_profile_elem info;
};

/* The aggregator type determines if identifier is for a VSI group,
 * aggregator group, aggregator of queues, or queue group.
 */
enum ice_agg_type {
	ICE_AGG_TYPE_UNKNOWN = 0,
	ICE_AGG_TYPE_TC,
	ICE_AGG_TYPE_AGG, /* aggregator */
	ICE_AGG_TYPE_VSI,
	ICE_AGG_TYPE_QG,
	ICE_AGG_TYPE_Q
};

/* Rate limit types */
enum ice_rl_type {
	ICE_UNKNOWN_BW = 0,
	ICE_MIN_BW,		/* for CIR profile */
	ICE_MAX_BW,		/* for EIR profile */
	ICE_SHARED_BW		/* for shared profile */
};

#define ICE_SCHED_MIN_BW		500		/* in Kbps */
#define ICE_SCHED_MAX_BW		100000000	/* in Kbps */
#define ICE_SCHED_DFLT_BW		0xFFFFFFFF	/* unlimited */
#define ICE_SCHED_NO_PRIORITY		0
#define ICE_SCHED_NO_BW_WT		0
#define ICE_SCHED_DFLT_RL_PROF_ID	0
#define ICE_SCHED_NO_SHARED_RL_PROF_ID	0xFFFF
#define ICE_SCHED_DFLT_BW_WT		4
#define ICE_SCHED_INVAL_PROF_ID		0xFFFF
#define ICE_SCHED_DFLT_BURST_SIZE	(15 * 1024)	/* in bytes (15k) */

/* Access Macros for Tx Sched RL Profile data */
#define ICE_TXSCHED_GET_RL_PROF_ID(p) LE16_TO_CPU((p)->info.profile_id)
#define ICE_TXSCHED_GET_RL_MBS(p) LE16_TO_CPU((p)->info.max_burst_size)
#define ICE_TXSCHED_GET_RL_MULTIPLIER(p) LE16_TO_CPU((p)->info.rl_multiply)
#define ICE_TXSCHED_GET_RL_WAKEUP_MV(p) LE16_TO_CPU((p)->info.wake_up_calc)
#define ICE_TXSCHED_GET_RL_ENCODE(p) LE16_TO_CPU((p)->info.rl_encode)

#define ICE_MAX_PORT_PER_PCI_DEV	8

/* The following tree example shows the naming conventions followed under
 * ice_port_info struct for default scheduler tree topology.
 *
 *                 A tree on a port
 *                       *                ---> root node
 *        (TC0)/  /  /  / \  \  \  \(TC7) ---> num_branches (range:1- 8)
 *            *  *  *  *   *  *  *  *     |
 *           /                            |
 *          *                             |
 *         /                              |-> num_elements (range:1 - 9)
 *        *                               |   implies num_of_layers
 *       /                                |
 *   (a)*                                 |
 *
 *  (a) is the last_node_teid(not of type Leaf). A leaf node is created under
 *  (a) as child node where queues get added, add Tx/Rx queue admin commands;
 *  need TEID of (a) to add queues.
 *
 *  This tree
 *       -> has 8 branches (one for each TC)
 *       -> First branch (TC0) has 4 elements
 *       -> has 4 layers
 *       -> (a) is the topmost layer node created by firmware on branch 0
 *
 *  Note: Above asterisk tree covers only basic terminology and scenario.
 *  Refer to the documentation for more info.
 */

 /* Data structure for saving BW information */
enum ice_bw_type {
	ICE_BW_TYPE_PRIO,
	ICE_BW_TYPE_CIR,
	ICE_BW_TYPE_CIR_WT,
	ICE_BW_TYPE_EIR,
	ICE_BW_TYPE_EIR_WT,
	ICE_BW_TYPE_SHARED,
	ICE_BW_TYPE_CNT		/* This must be last */
};

struct ice_bw {
	u32 bw;
	u16 bw_alloc;
};

struct ice_bw_type_info {
	ice_declare_bitmap(bw_t_bitmap, ICE_BW_TYPE_CNT);
	u8 generic;
	struct ice_bw cir_bw;
	struct ice_bw eir_bw;
	u32 shared_bw;
};

/* VSI queue context structure for given TC */
struct ice_q_ctx {
	u16  q_handle;
	u32  q_teid;
	/* bw_t_info saves queue BW information */
	struct ice_bw_type_info bw_t_info;
};

/* VSI type list entry to locate corresponding VSI/aggregator nodes */
struct ice_sched_vsi_info {
	struct ice_sched_node *vsi_node[ICE_MAX_TRAFFIC_CLASS];
	struct ice_sched_node *ag_node[ICE_MAX_TRAFFIC_CLASS];
	u16 max_lanq[ICE_MAX_TRAFFIC_CLASS];
	u16 max_rdmaq[ICE_MAX_TRAFFIC_CLASS];
	/* bw_t_info saves VSI BW information */
	struct ice_bw_type_info bw_t_info[ICE_MAX_TRAFFIC_CLASS];
};

/* CEE or IEEE 802.1Qaz ETS Configuration data */
struct ice_dcb_ets_cfg {
	u8 willing;
	u8 cbs;
	u8 maxtcs;
	u8 prio_table[ICE_MAX_TRAFFIC_CLASS];
	u8 tcbwtable[ICE_MAX_TRAFFIC_CLASS];
	u8 tsatable[ICE_MAX_TRAFFIC_CLASS];
};

/* CEE or IEEE 802.1Qaz PFC Configuration data */
struct ice_dcb_pfc_cfg {
	u8 willing;
	u8 mbc;
	u8 pfccap;
	u8 pfcena;
};

/* CEE or IEEE 802.1Qaz Application Priority data */
struct ice_dcb_app_priority_table {
	u16 prot_id;
	u8 priority;
	u8 selector;
};

#define ICE_MAX_USER_PRIORITY		8
#define ICE_DCBX_MAX_APPS		64
#define ICE_DSCP_NUM_VAL		64
#define ICE_LLDPDU_SIZE			1500
#define ICE_TLV_STATUS_OPER		0x1
#define ICE_TLV_STATUS_SYNC		0x2
#define ICE_TLV_STATUS_ERR		0x4
#define ICE_APP_PROT_ID_FCOE		0x8906
#define ICE_APP_PROT_ID_ISCSI		0x0cbc
#define ICE_APP_PROT_ID_ISCSI_860	0x035c
#define ICE_APP_PROT_ID_FIP		0x8914
#define ICE_APP_SEL_ETHTYPE		0x1
#define ICE_APP_SEL_TCPIP		0x2
#define ICE_CEE_APP_SEL_ETHTYPE		0x0
#define ICE_CEE_APP_SEL_TCPIP		0x1

struct ice_dcbx_cfg {
	u32 numapps;
	u32 tlv_status; /* CEE mode TLV status */
	struct ice_dcb_ets_cfg etscfg;
	struct ice_dcb_ets_cfg etsrec;
	struct ice_dcb_pfc_cfg pfc;
#define ICE_QOS_MODE_VLAN	0x0
#define ICE_QOS_MODE_DSCP	0x1
	u8 pfc_mode;
	struct ice_dcb_app_priority_table app[ICE_DCBX_MAX_APPS];
	/* when DSCP mapping defined by user set its bit to 1 */
	ice_declare_bitmap(dscp_mapped, ICE_DSCP_NUM_VAL);
	/* array holding DSCP -> UP/TC values for DSCP L3 QoS mode */
	u8 dscp_map[ICE_DSCP_NUM_VAL];
	u8 dcbx_mode;
#define ICE_DCBX_MODE_CEE	0x1
#define ICE_DCBX_MODE_IEEE	0x2
	u8 app_mode;
#define ICE_DCBX_APPS_NON_WILLING	0x1
};

struct ice_qos_cfg {
	struct ice_dcbx_cfg local_dcbx_cfg;	/* Oper/Local Cfg */
	struct ice_dcbx_cfg desired_dcbx_cfg;	/* CEE Desired Cfg */
	struct ice_dcbx_cfg remote_dcbx_cfg;	/* Peer Cfg */
	u8 dcbx_status : 3;			/* see ICE_DCBX_STATUS_DIS */
	u8 is_sw_lldp : 1;
};

struct ice_port_info {
	struct ice_sched_node *root;	/* Root Node per Port */
	struct ice_hw *hw;		/* back pointer to HW instance */
	u32 last_node_teid;		/* scheduler last node info */
	u16 sw_id;			/* Initial switch ID belongs to port */
	u16 pf_vf_num;
	u8 port_state;
#define ICE_SCHED_PORT_STATE_INIT	0x0
#define ICE_SCHED_PORT_STATE_READY	0x1
	u8 lport;
#define ICE_LPORT_MASK			0xff
	u16 dflt_tx_vsi_rule_id;
	u16 dflt_tx_vsi_num;
	u16 dflt_rx_vsi_rule_id;
	u16 dflt_rx_vsi_num;
	struct ice_fc_info fc;
	struct ice_mac_info mac;
	struct ice_phy_info phy;
	struct ice_lock sched_lock;	/* protect access to TXSched tree */
	struct ice_sched_node *
		sib_head[ICE_MAX_TRAFFIC_CLASS][ICE_AQC_TOPO_MAX_LEVEL_NUM];
	struct ice_bw_type_info root_node_bw_t_info;
	struct ice_bw_type_info tc_node_bw_t_info[ICE_MAX_TRAFFIC_CLASS];
	struct ice_qos_cfg qos_cfg;
	u8 is_vf:1;
};

struct ice_switch_info {
	struct LIST_HEAD_TYPE vsi_list_map_head;
	struct ice_sw_recipe *recp_list;
	u16 prof_res_bm_init;
	u16 max_used_prof_index;

	ice_declare_bitmap(prof_res_bm[ICE_MAX_NUM_PROFILES], ICE_MAX_FV_WORDS);
};


/* Enum defining the different states of the mailbox snapshot in the
 * PF-VF mailbox overflow detection algorithm. The snapshot can be in
 * states:
 * 1. ICE_MAL_VF_DETECT_STATE_NEW_SNAPSHOT - generate a new static snapshot
 * within the mailbox buffer.
 * 2. ICE_MAL_VF_DETECT_STATE_TRAVERSE - iterate through the mailbox snaphot
 * 3. ICE_MAL_VF_DETECT_STATE_DETECT - track the messages sent per VF via the
 * mailbox and mark any VFs sending more messages than the threshold limit set.
 * 4. ICE_MAL_VF_DETECT_STATE_INVALID - Invalid mailbox state set to 0xFFFFFFFF.
 */
enum ice_mbx_snapshot_state {
	ICE_MAL_VF_DETECT_STATE_NEW_SNAPSHOT = 0,
	ICE_MAL_VF_DETECT_STATE_TRAVERSE,
	ICE_MAL_VF_DETECT_STATE_DETECT,
	ICE_MAL_VF_DETECT_STATE_INVALID = 0xFFFFFFFF,
};

/* Structure to hold information of the static snapshot and the mailbox
 * buffer data used to generate and track the snapshot.
 * 1. state: the state of the mailbox snapshot in the malicious VF
 * detection state handler ice_mbx_vf_state_handler()
 * 2. head : head of the mailbox snapshot in a circular mailbox buffer
 * 3. tail : tail of the mailbox snapshot in a circular mailbox buffer
 * 4. num_iterations: number of messages traversed in circular mailbox buffer
 * 5. num_msg_proc: number of messages processed in mailbox
 * 6. num_pending_arq: number of pending asynchronous messages
 * 7. max_num_msgs_mbx: maximum messages in mailbox for currently
 * serviced work item or interrupt.
 */
struct ice_mbx_snap_buffer_data {
	enum ice_mbx_snapshot_state state;
	u32 head;
	u32 tail;
	u32 num_iterations;
	u16 num_msg_proc;
	u16 num_pending_arq;
	u16 max_num_msgs_mbx;
};

/* Structure to track messages sent by VFs on mailbox:
 * 1. vf_cntr : a counter array of VFs to track the number of
 * asynchronous messages sent by each VF
 * 2. vfcntr_len : number of entries in VF counter array
 */
struct ice_mbx_vf_counter {
	u32 *vf_cntr;
	u32 vfcntr_len;
};

/* Structure to hold data relevant to the captured static snapshot
 * of the PF-VF mailbox.
 */
struct ice_mbx_snapshot {
	struct ice_mbx_snap_buffer_data mbx_buf;
	struct ice_mbx_vf_counter mbx_vf;
};

/* Structure to hold data to be used for capturing or updating a
 * static snapshot.
 * 1. num_msg_proc: number of messages processed in mailbox
 * 2. num_pending_arq: number of pending asynchronous messages
 * 3. max_num_msgs_mbx: maximum messages in mailbox for currently
 * serviced work item or interrupt.
 * 4. async_watermark_val: An upper threshold set by caller to determine
 * if the pending arq count is large enough to assume that there is
 * the possibility of a mailicious VF.
 */
struct ice_mbx_data {
	u16 num_msg_proc;
	u16 num_pending_arq;
	u16 max_num_msgs_mbx;
	u16 async_watermark_val;
};

/* Port hardware description */
struct ice_hw {
	u8 *hw_addr;
	void *back;
	struct ice_aqc_layer_props *layer_info;
	struct ice_port_info *port_info;
	/* 2D Array for each Tx Sched RL Profile type */
	struct ice_sched_rl_profile **cir_profiles;
	struct ice_sched_rl_profile **eir_profiles;
	struct ice_sched_rl_profile **srl_profiles;
	/* PSM clock frequency for calculating RL profile params */
	u32 psm_clk_freq;
	u64 debug_mask;		/* BITMAP for debug mask */
	enum ice_mac_type mac_type;

	/* pci info */
	u16 device_id;
	u16 vendor_id;
	u16 subsystem_device_id;
	u16 subsystem_vendor_id;
	u8 revision_id;

	u8 pf_id;		/* device profile info */

	u16 max_burst_size;	/* driver sets this value */

	/* Tx Scheduler values */
	u8 num_tx_sched_layers;
	u8 num_tx_sched_phys_layers;
	u8 flattened_layers;
	u8 max_cgds;
	u8 sw_entry_point_layer;
	u16 max_children[ICE_AQC_TOPO_MAX_LEVEL_NUM];
	struct LIST_HEAD_TYPE agg_list;	/* lists all aggregator */
	/* List contain profile ID(s) and other params per layer */
	struct LIST_HEAD_TYPE rl_prof_list[ICE_AQC_TOPO_MAX_LEVEL_NUM];
	struct ice_vsi_ctx *vsi_ctx[ICE_MAX_VSI];
	u8 evb_veb;		/* true for VEB, false for VEPA */
	u8 reset_ongoing;	/* true if HW is in reset, false otherwise */
	struct ice_bus_info bus;
	struct ice_flash_info flash;
	struct ice_hw_dev_caps dev_caps;	/* device capabilities */
	struct ice_hw_func_caps func_caps;	/* function capabilities */

	struct ice_switch_info *switch_info;	/* switch filter lists */

	/* Control Queue info */
	struct ice_ctl_q_info adminq;
	struct ice_ctl_q_info mailboxq;
	u8 api_branch;		/* API branch version */
	u8 api_maj_ver;		/* API major version */
	u8 api_min_ver;		/* API minor version */
	u8 api_patch;		/* API patch version */
	u8 fw_branch;		/* firmware branch version */
	u8 fw_maj_ver;		/* firmware major version */
	u8 fw_min_ver;		/* firmware minor version */
	u8 fw_patch;		/* firmware patch version */
	u32 fw_build;		/* firmware build number */

	struct ice_fwlog_cfg fwlog_cfg;
	bool fwlog_support_ena; /* does hardware support FW logging? */

/* Device max aggregate bandwidths corresponding to the GL_PWR_MODE_CTL
 * register. Used for determining the ITR/INTRL granularity during
 * initialization.
 */
#define ICE_MAX_AGG_BW_200G	0x0
#define ICE_MAX_AGG_BW_100G	0X1
#define ICE_MAX_AGG_BW_50G	0x2
#define ICE_MAX_AGG_BW_25G	0x3
	/* ITR granularity for different speeds */
#define ICE_ITR_GRAN_ABOVE_25	2
#define ICE_ITR_GRAN_MAX_25	4
	/* ITR granularity in 1 us */
	u8 itr_gran;
	/* INTRL granularity for different speeds */
#define ICE_INTRL_GRAN_ABOVE_25	4
#define ICE_INTRL_GRAN_MAX_25	8
	/* INTRL granularity in 1 us */
	u8 intrl_gran;

	/* true if VSIs can share unicast MAC addr */
	u8 umac_shared;

#define ICE_PHY_PER_NAC		1
#define ICE_MAX_QUAD		2
#define ICE_NUM_QUAD_TYPE	2
#define ICE_PORTS_PER_QUAD	4
#define ICE_PHY_0_LAST_QUAD	1
#define ICE_PORTS_PER_PHY	8
#define ICE_NUM_EXTERNAL_PORTS		ICE_PORTS_PER_PHY

	/* Active package version (currently active) */
	struct ice_pkg_ver active_pkg_ver;
	u32 pkg_seg_id;
	u32 active_track_id;
	u8 active_pkg_name[ICE_PKG_NAME_SIZE];
	u8 active_pkg_in_nvm;

	enum ice_aq_err pkg_dwnld_status;

	/* Driver's package ver - (from the Ice Metadata section) */
	struct ice_pkg_ver pkg_ver;
	u8 pkg_name[ICE_PKG_NAME_SIZE];

	/* Driver's Ice segment format version and id (from the Ice seg) */
	struct ice_pkg_ver ice_seg_fmt_ver;
	u8 ice_seg_id[ICE_SEG_ID_SIZE];

	/* Pointer to the ice segment */
	struct ice_seg *seg;

	/* Pointer to allocated copy of pkg memory */
	u8 *pkg_copy;
	u32 pkg_size;

	/* tunneling info */
	struct ice_lock tnl_lock;
	struct ice_tunnel_table tnl;

	/* HW block tables */
	struct ice_blk_info blk[ICE_BLK_COUNT];
	struct ice_lock fl_profs_locks[ICE_BLK_COUNT];	/* lock fltr profiles */
	struct LIST_HEAD_TYPE fl_profs[ICE_BLK_COUNT];
	struct ice_lock rss_locks;	/* protect RSS configuration */
	struct LIST_HEAD_TYPE rss_list_head;
	struct ice_mbx_snapshot mbx_snapshot;
	u8 dvm_ena;
};

/* Statistics collected by each port, VSI, VEB, and S-channel */
struct ice_eth_stats {
	u64 rx_bytes;			/* gorc */
	u64 rx_unicast;			/* uprc */
	u64 rx_multicast;		/* mprc */
	u64 rx_broadcast;		/* bprc */
	u64 rx_discards;		/* rdpc */
	u64 rx_unknown_protocol;	/* rupp */
	u64 tx_bytes;			/* gotc */
	u64 tx_unicast;			/* uptc */
	u64 tx_multicast;		/* mptc */
	u64 tx_broadcast;		/* bptc */
	u64 tx_discards;		/* tdpc */
	u64 tx_errors;			/* tepc */
	u64 rx_no_desc;			/* repc */
	u64 rx_errors;			/* repc */
};

#define ICE_MAX_UP	8

/* Statistics collected per VEB per User Priority (UP) for up to 8 UPs */
struct ice_veb_up_stats {
	u64 up_rx_pkts[ICE_MAX_UP];
	u64 up_rx_bytes[ICE_MAX_UP];
	u64 up_tx_pkts[ICE_MAX_UP];
	u64 up_tx_bytes[ICE_MAX_UP];
};

/* Statistics collected by the MAC */
struct ice_hw_port_stats {
	/* eth stats collected by the port */
	struct ice_eth_stats eth;
	/* additional port specific stats */
	u64 tx_dropped_link_down;	/* tdold */
	u64 crc_errors;			/* crcerrs */
	u64 illegal_bytes;		/* illerrc */
	u64 error_bytes;		/* errbc */
	u64 mac_local_faults;		/* mlfc */
	u64 mac_remote_faults;		/* mrfc */
	u64 rx_len_errors;		/* rlec */
	u64 link_xon_rx;		/* lxonrxc */
	u64 link_xoff_rx;		/* lxoffrxc */
	u64 link_xon_tx;		/* lxontxc */
	u64 link_xoff_tx;		/* lxofftxc */
	u64 priority_xon_rx[8];		/* pxonrxc[8] */
	u64 priority_xoff_rx[8];	/* pxoffrxc[8] */
	u64 priority_xon_tx[8];		/* pxontxc[8] */
	u64 priority_xoff_tx[8];	/* pxofftxc[8] */
	u64 priority_xon_2_xoff[8];	/* pxon2offc[8] */
	u64 rx_size_64;			/* prc64 */
	u64 rx_size_127;		/* prc127 */
	u64 rx_size_255;		/* prc255 */
	u64 rx_size_511;		/* prc511 */
	u64 rx_size_1023;		/* prc1023 */
	u64 rx_size_1522;		/* prc1522 */
	u64 rx_size_big;		/* prc9522 */
	u64 rx_undersize;		/* ruc */
	u64 rx_fragments;		/* rfc */
	u64 rx_oversize;		/* roc */
	u64 rx_jabber;			/* rjc */
	u64 tx_size_64;			/* ptc64 */
	u64 tx_size_127;		/* ptc127 */
	u64 tx_size_255;		/* ptc255 */
	u64 tx_size_511;		/* ptc511 */
	u64 tx_size_1023;		/* ptc1023 */
	u64 tx_size_1522;		/* ptc1522 */
	u64 tx_size_big;		/* ptc9522 */
	u64 mac_short_pkt_dropped;	/* mspdc */
	/* EEE LPI */
	u32 tx_lpi_status;
	u32 rx_lpi_status;
	u64 tx_lpi_count;		/* etlpic */
	u64 rx_lpi_count;		/* erlpic */
};

enum ice_sw_fwd_act_type {
	ICE_FWD_TO_VSI = 0,
	ICE_FWD_TO_VSI_LIST, /* Do not use this when adding filter */
	ICE_FWD_TO_Q,
	ICE_FWD_TO_QGRP,
	ICE_DROP_PACKET,
	ICE_INVAL_ACT
};

struct ice_aq_get_set_rss_lut_params {
	u16 vsi_handle;		/* software VSI handle */
	u16 lut_size;		/* size of the LUT buffer */
	u8 lut_type;		/* type of the LUT (i.e. VSI, PF, Global) */
	u8 *lut;		/* input RSS LUT for set and output RSS LUT for get */
	u8 global_lut_id;	/* only valid when lut_type is global */
};

/* Checksum and Shadow RAM pointers */
#define ICE_SR_NVM_CTRL_WORD			0x00
#define ICE_SR_PHY_ANALOG_PTR			0x04
#define ICE_SR_OPTION_ROM_PTR			0x05
#define ICE_SR_RO_PCIR_REGS_AUTO_LOAD_PTR	0x06
#define ICE_SR_AUTO_GENERATED_POINTERS_PTR	0x07
#define ICE_SR_PCIR_REGS_AUTO_LOAD_PTR		0x08
#define ICE_SR_EMP_GLOBAL_MODULE_PTR		0x09
#define ICE_SR_EMP_IMAGE_PTR			0x0B
#define ICE_SR_PE_IMAGE_PTR			0x0C
#define ICE_SR_CSR_PROTECTED_LIST_PTR		0x0D
#define ICE_SR_MNG_CFG_PTR			0x0E
#define ICE_SR_EMP_MODULE_PTR			0x0F
#define ICE_SR_PBA_BLOCK_PTR			0x16
#define ICE_SR_BOOT_CFG_PTR			0x132
#define ICE_SR_NVM_WOL_CFG			0x19
#define ICE_NVM_OROM_VER_OFF			0x02
#define ICE_SR_NVM_DEV_STARTER_VER		0x18
#define ICE_SR_ALTERNATE_SAN_MAC_ADDR_PTR	0x27
#define ICE_SR_PERMANENT_SAN_MAC_ADDR_PTR	0x28
#define ICE_SR_NVM_MAP_VER			0x29
#define ICE_SR_NVM_IMAGE_VER			0x2A
#define ICE_SR_NVM_STRUCTURE_VER		0x2B
#define ICE_SR_NVM_EETRACK_LO			0x2D
#define ICE_SR_NVM_EETRACK_HI			0x2E
#define ICE_NVM_VER_LO_SHIFT			0
#define ICE_NVM_VER_LO_MASK			(0xff << ICE_NVM_VER_LO_SHIFT)
#define ICE_NVM_VER_HI_SHIFT			12
#define ICE_NVM_VER_HI_MASK			(0xf << ICE_NVM_VER_HI_SHIFT)
#define ICE_OEM_EETRACK_ID			0xffffffff
#define ICE_OROM_VER_PATCH_SHIFT		0
#define ICE_OROM_VER_PATCH_MASK		(0xff << ICE_OROM_VER_PATCH_SHIFT)
#define ICE_OROM_VER_BUILD_SHIFT		8
#define ICE_OROM_VER_BUILD_MASK		(0xffff << ICE_OROM_VER_BUILD_SHIFT)
#define ICE_OROM_VER_SHIFT			24
#define ICE_OROM_VER_MASK			(0xff << ICE_OROM_VER_SHIFT)
#define ICE_SR_VPD_PTR				0x2F
#define ICE_SR_PXE_SETUP_PTR			0x30
#define ICE_SR_PXE_CFG_CUST_OPTIONS_PTR		0x31
#define ICE_SR_NVM_ORIGINAL_EETRACK_LO		0x34
#define ICE_SR_NVM_ORIGINAL_EETRACK_HI		0x35
#define ICE_SR_VLAN_CFG_PTR			0x37
#define ICE_SR_POR_REGS_AUTO_LOAD_PTR		0x38
#define ICE_SR_EMPR_REGS_AUTO_LOAD_PTR		0x3A
#define ICE_SR_GLOBR_REGS_AUTO_LOAD_PTR		0x3B
#define ICE_SR_CORER_REGS_AUTO_LOAD_PTR		0x3C
#define ICE_SR_PHY_CFG_SCRIPT_PTR		0x3D
#define ICE_SR_PCIE_ALT_AUTO_LOAD_PTR		0x3E
#define ICE_SR_SW_CHECKSUM_WORD			0x3F
#define ICE_SR_PFA_PTR				0x40
#define ICE_SR_1ST_SCRATCH_PAD_PTR		0x41
#define ICE_SR_1ST_NVM_BANK_PTR			0x42
#define ICE_SR_NVM_BANK_SIZE			0x43
#define ICE_SR_1ST_OROM_BANK_PTR		0x44
#define ICE_SR_OROM_BANK_SIZE			0x45
#define ICE_SR_NETLIST_BANK_PTR			0x46
#define ICE_SR_NETLIST_BANK_SIZE		0x47
#define ICE_SR_EMP_SR_SETTINGS_PTR		0x48
#define ICE_SR_CONFIGURATION_METADATA_PTR	0x4D
#define ICE_SR_IMMEDIATE_VALUES_PTR		0x4E
#define ICE_SR_LINK_DEFAULT_OVERRIDE_PTR	0x134
#define ICE_SR_POR_REGISTERS_AUTOLOAD_PTR	0x118

/* CSS Header words */
#define ICE_NVM_CSS_HDR_LEN_L			0x02
#define ICE_NVM_CSS_HDR_LEN_H			0x03
#define ICE_NVM_CSS_SREV_L			0x14
#define ICE_NVM_CSS_SREV_H			0x15

/* Length of Authentication header section in words */
#define ICE_NVM_AUTH_HEADER_LEN			0x08

/* The Link Topology Netlist section is stored as a series of words. It is
 * stored in the NVM as a TLV, with the first two words containing the type
 * and length.
 */
#define ICE_NETLIST_LINK_TOPO_MOD_ID		0x011B
#define ICE_NETLIST_TYPE_OFFSET			0x0000
#define ICE_NETLIST_LEN_OFFSET			0x0001

/* The Link Topology section follows the TLV header. When reading the netlist
 * using ice_read_netlist_module, we need to account for the 2-word TLV
 * header.
 */
#define ICE_NETLIST_LINK_TOPO_OFFSET(n)		((n) + 2)

#define ICE_LINK_TOPO_MODULE_LEN		ICE_NETLIST_LINK_TOPO_OFFSET(0x0000)
#define ICE_LINK_TOPO_NODE_COUNT		ICE_NETLIST_LINK_TOPO_OFFSET(0x0001)

#define ICE_LINK_TOPO_NODE_COUNT_M		MAKEMASK(0x3FF, 0)

/* The Netlist ID Block is located after all of the Link Topology nodes. */
#define ICE_NETLIST_ID_BLK_SIZE			0x30
#define ICE_NETLIST_ID_BLK_OFFSET(n)		ICE_NETLIST_LINK_TOPO_OFFSET(0x0004 + 2 * (n))

/* netlist ID block field offsets (word offsets) */
#define ICE_NETLIST_ID_BLK_MAJOR_VER_LOW	0x02
#define ICE_NETLIST_ID_BLK_MAJOR_VER_HIGH	0x03
#define ICE_NETLIST_ID_BLK_MINOR_VER_LOW	0x04
#define ICE_NETLIST_ID_BLK_MINOR_VER_HIGH	0x05
#define ICE_NETLIST_ID_BLK_TYPE_LOW		0x06
#define ICE_NETLIST_ID_BLK_TYPE_HIGH		0x07
#define ICE_NETLIST_ID_BLK_REV_LOW		0x08
#define ICE_NETLIST_ID_BLK_REV_HIGH		0x09
#define ICE_NETLIST_ID_BLK_SHA_HASH_WORD(n)	(0x0A + (n))
#define ICE_NETLIST_ID_BLK_CUST_VER		0x2F

/* Auxiliary field, mask and shift definition for Shadow RAM and NVM Flash */
#define ICE_SR_VPD_SIZE_WORDS		512
#define ICE_SR_PCIE_ALT_SIZE_WORDS	512
#define ICE_SR_CTRL_WORD_1_S		0x06
#define ICE_SR_CTRL_WORD_1_M		(0x03 << ICE_SR_CTRL_WORD_1_S)
#define ICE_SR_CTRL_WORD_VALID		0x1
#define ICE_SR_CTRL_WORD_OROM_BANK	BIT(3)
#define ICE_SR_CTRL_WORD_NETLIST_BANK	BIT(4)
#define ICE_SR_CTRL_WORD_NVM_BANK	BIT(5)

#define ICE_SR_NVM_PTR_4KB_UNITS	BIT(15)

/* Shadow RAM related */
#define ICE_SR_SECTOR_SIZE_IN_WORDS	0x800
#define ICE_SR_BUF_ALIGNMENT		4096
#define ICE_SR_WORDS_IN_1KB		512
/* Checksum should be calculated such that after adding all the words,
 * including the checksum word itself, the sum should be 0xBABA.
 */
#define ICE_SR_SW_CHECKSUM_BASE		0xBABA

/* Link override related */
#define ICE_SR_PFA_LINK_OVERRIDE_WORDS		10
#define ICE_SR_PFA_LINK_OVERRIDE_PHY_WORDS	4
#define ICE_SR_PFA_LINK_OVERRIDE_OFFSET		2
#define ICE_SR_PFA_LINK_OVERRIDE_FEC_OFFSET	1
#define ICE_SR_PFA_LINK_OVERRIDE_PHY_OFFSET	2
#define ICE_FW_API_LINK_OVERRIDE_MAJ		1
#define ICE_FW_API_LINK_OVERRIDE_MIN		5
#define ICE_FW_API_LINK_OVERRIDE_PATCH		2

#define ICE_PBA_FLAG_DFLT		0xFAFA
/* Hash redirection LUT for VSI - maximum array size */
#define ICE_VSIQF_HLUT_ARRAY_SIZE	((VSIQF_HLUT_MAX_INDEX + 1) * 4)

/*
 * Defines for values in the VF_PE_DB_SIZE bits in the GLPCI_LBARCTRL register.
 * This is needed to determine the BAR0 space for the VFs
 */
#define GLPCI_LBARCTRL_VF_PE_DB_SIZE_0KB 0x0
#define GLPCI_LBARCTRL_VF_PE_DB_SIZE_8KB 0x1
#define GLPCI_LBARCTRL_VF_PE_DB_SIZE_64KB 0x2

/* AQ API version for LLDP_FILTER_CONTROL */
#define ICE_FW_API_LLDP_FLTR_MAJ	1
#define ICE_FW_API_LLDP_FLTR_MIN	7
#define ICE_FW_API_LLDP_FLTR_PATCH	1

/* AQ API version for report default configuration */
#define ICE_FW_API_REPORT_DFLT_CFG_MAJ		1
#define ICE_FW_API_REPORT_DFLT_CFG_MIN		7
#define ICE_FW_API_REPORT_DFLT_CFG_PATCH	3

/* AQ API version for FW health reports */
#define ICE_FW_API_HEALTH_REPORT_MAJ		1
#define ICE_FW_API_HEALTH_REPORT_MIN		7
#define ICE_FW_API_HEALTH_REPORT_PATCH		6

/* AQ API version for FW auto drop reports */
#define ICE_FW_API_AUTO_DROP_MAJ		1
#define ICE_FW_API_AUTO_DROP_MIN		4
#endif /* _ICE_TYPE_H_ */
