/*-
 * Copyright 2021 Intel Corp
 * Copyright 2021 Rubicon Communications, LLC (Netgate)
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _IGC_HW_H_
#define _IGC_HW_H_

#include "igc_osdep.h"
#include "igc_regs.h"
#include "igc_defines.h"

struct igc_hw;

#define IGC_DEV_ID_I225_LM			0x15F2
#define IGC_DEV_ID_I225_V			0x15F3
#define IGC_DEV_ID_I225_K			0x3100
#define IGC_DEV_ID_I225_I			0x15F8
#define IGC_DEV_ID_I220_V			0x15F7
#define IGC_DEV_ID_I225_K2			0x3101
#define IGC_DEV_ID_I225_LMVP			0x5502
#define IGC_DEV_ID_I226_K			0x3102
#define IGC_DEV_ID_I226_LMVP			0x5503
#define IGC_DEV_ID_I225_IT			0x0D9F
#define IGC_DEV_ID_I226_LM			0x125B
#define IGC_DEV_ID_I226_V			0x125C
#define IGC_DEV_ID_I226_IT			0x125D
#define IGC_DEV_ID_I221_V			0x125E
#define IGC_DEV_ID_I226_BLANK_NVM		0x125F
#define IGC_DEV_ID_I225_BLANK_NVM		0x15FD

#define IGC_REVISION_0	0
#define IGC_REVISION_1	1
#define IGC_REVISION_2	2
#define IGC_REVISION_3	3
#define IGC_REVISION_4	4

#define IGC_FUNC_1		1

#define IGC_ALT_MAC_ADDRESS_OFFSET_LAN0	0
#define IGC_ALT_MAC_ADDRESS_OFFSET_LAN1	3

enum igc_mac_type {
	igc_undefined = 0,
	igc_i225,
	igc_num_macs  /* List is 1-based, so subtract 1 for TRUE count. */
};

enum igc_media_type {
	igc_media_type_unknown = 0,
	igc_media_type_copper = 1,
	igc_num_media_types
};

enum igc_nvm_type {
	igc_nvm_unknown = 0,
	igc_nvm_eeprom_spi,
	igc_nvm_flash_hw,
	igc_nvm_invm,
};

enum igc_phy_type {
	igc_phy_unknown = 0,
	igc_phy_none,
	igc_phy_i225,
};

enum igc_bus_type {
	igc_bus_type_unknown = 0,
	igc_bus_type_pci,
	igc_bus_type_pcix,
	igc_bus_type_pci_express,
	igc_bus_type_reserved
};

enum igc_bus_speed {
	igc_bus_speed_unknown = 0,
	igc_bus_speed_33,
	igc_bus_speed_66,
	igc_bus_speed_100,
	igc_bus_speed_120,
	igc_bus_speed_133,
	igc_bus_speed_2500,
	igc_bus_speed_5000,
	igc_bus_speed_reserved
};

enum igc_bus_width {
	igc_bus_width_unknown = 0,
	igc_bus_width_pcie_x1,
	igc_bus_width_pcie_x2,
	igc_bus_width_pcie_x4 = 4,
	igc_bus_width_pcie_x8 = 8,
	igc_bus_width_32,
	igc_bus_width_64,
	igc_bus_width_reserved
};

enum igc_fc_mode {
	igc_fc_none = 0,
	igc_fc_rx_pause,
	igc_fc_tx_pause,
	igc_fc_full,
	igc_fc_default = 0xFF
};

enum igc_ms_type {
	igc_ms_hw_default = 0,
	igc_ms_force_master,
	igc_ms_force_slave,
	igc_ms_auto
};

enum igc_smart_speed {
	igc_smart_speed_default = 0,
	igc_smart_speed_on,
	igc_smart_speed_off
};

#define __le16 u16
#define __le32 u32
#define __le64 u64
/* Receive Descriptor */
struct igc_rx_desc {
	__le64 buffer_addr; /* Address of the descriptor's data buffer */
	__le16 length;      /* Length of data DMAed into data buffer */
	__le16 csum; /* Packet checksum */
	u8  status;  /* Descriptor status */
	u8  errors;  /* Descriptor Errors */
	__le16 special;
};

/* Receive Descriptor - Extended */
union igc_rx_desc_extended {
	struct {
		__le64 buffer_addr;
		__le64 reserved;
	} read;
	struct {
		struct {
			__le32 mrq; /* Multiple Rx Queues */
			union {
				__le32 rss; /* RSS Hash */
				struct {
					__le16 ip_id;  /* IP id */
					__le16 csum;   /* Packet Checksum */
				} csum_ip;
			} hi_dword;
		} lower;
		struct {
			__le32 status_error;  /* ext status/error */
			__le16 length;
			__le16 vlan; /* VLAN tag */
		} upper;
	} wb;  /* writeback */
};

#define MAX_PS_BUFFERS 4

/* Number of packet split data buffers (not including the header buffer) */
#define PS_PAGE_BUFFERS	(MAX_PS_BUFFERS - 1)

/* Receive Descriptor - Packet Split */
union igc_rx_desc_packet_split {
	struct {
		/* one buffer for protocol header(s), three data buffers */
		__le64 buffer_addr[MAX_PS_BUFFERS];
	} read;
	struct {
		struct {
			__le32 mrq;  /* Multiple Rx Queues */
			union {
				__le32 rss; /* RSS Hash */
				struct {
					__le16 ip_id;    /* IP id */
					__le16 csum;     /* Packet Checksum */
				} csum_ip;
			} hi_dword;
		} lower;
		struct {
			__le32 status_error;  /* ext status/error */
			__le16 length0;  /* length of buffer 0 */
			__le16 vlan;  /* VLAN tag */
		} middle;
		struct {
			__le16 header_status;
			/* length of buffers 1-3 */
			__le16 length[PS_PAGE_BUFFERS];
		} upper;
		__le64 reserved;
	} wb; /* writeback */
};

/* Transmit Descriptor */
struct igc_tx_desc {
	__le64 buffer_addr;   /* Address of the descriptor's data buffer */
	union {
		__le32 data;
		struct {
			__le16 length;  /* Data buffer length */
			u8 cso;  /* Checksum offset */
			u8 cmd;  /* Descriptor control */
		} flags;
	} lower;
	union {
		__le32 data;
		struct {
			u8 status; /* Descriptor status */
			u8 css;  /* Checksum start */
			__le16 special;
		} fields;
	} upper;
};

/* Offload Context Descriptor */
struct igc_context_desc {
	union {
		__le32 ip_config;
		struct {
			u8 ipcss;  /* IP checksum start */
			u8 ipcso;  /* IP checksum offset */
			__le16 ipcse;  /* IP checksum end */
		} ip_fields;
	} lower_setup;
	union {
		__le32 tcp_config;
		struct {
			u8 tucss;  /* TCP checksum start */
			u8 tucso;  /* TCP checksum offset */
			__le16 tucse;  /* TCP checksum end */
		} tcp_fields;
	} upper_setup;
	__le32 cmd_and_length;
	union {
		__le32 data;
		struct {
			u8 status;  /* Descriptor status */
			u8 hdr_len;  /* Header length */
			__le16 mss;  /* Maximum segment size */
		} fields;
	} tcp_seg_setup;
};

/* Offload data descriptor */
struct igc_data_desc {
	__le64 buffer_addr;  /* Address of the descriptor's buffer address */
	union {
		__le32 data;
		struct {
			__le16 length;  /* Data buffer length */
			u8 typ_len_ext;
			u8 cmd;
		} flags;
	} lower;
	union {
		__le32 data;
		struct {
			u8 status;  /* Descriptor status */
			u8 popts;  /* Packet Options */
			__le16 special;
		} fields;
	} upper;
};

/* Statistics counters collected by the MAC */
struct igc_hw_stats {
	u64 crcerrs;
	u64 algnerrc;
	u64 symerrs;
	u64 rxerrc;
	u64 mpc;
	u64 scc;
	u64 ecol;
	u64 mcc;
	u64 latecol;
	u64 colc;
	u64 dc;
	u64 tncrs;
	u64 sec;
	u64 rlec;
	u64 xonrxc;
	u64 xontxc;
	u64 xoffrxc;
	u64 xofftxc;
	u64 fcruc;
	u64 prc64;
	u64 prc127;
	u64 prc255;
	u64 prc511;
	u64 prc1023;
	u64 prc1522;
	u64 tlpic;
	u64 rlpic;
	u64 gprc;
	u64 bprc;
	u64 mprc;
	u64 gptc;
	u64 gorc;
	u64 gotc;
	u64 rnbc;
	u64 ruc;
	u64 rfc;
	u64 roc;
	u64 rjc;
	u64 mgprc;
	u64 mgpdc;
	u64 mgptc;
	u64 tor;
	u64 tot;
	u64 tpr;
	u64 tpt;
	u64 ptc64;
	u64 ptc127;
	u64 ptc255;
	u64 ptc511;
	u64 ptc1023;
	u64 ptc1522;
	u64 mptc;
	u64 bptc;
	u64 tsctc;
	u64 iac;
	u64 rxdmtc;
	u64 htdpmc;
	u64 rpthc;
	u64 hgptc;
	u64 hgorc;
	u64 hgotc;
	u64 lenerrs;
	u64 scvpc;
	u64 hrmpc;
	u64 doosync;
	u64 o2bgptc;
	u64 o2bspc;
	u64 b2ospc;
	u64 b2ogprc;
};

#include "igc_mac.h"
#include "igc_phy.h"
#include "igc_nvm.h"

/* Function pointers for the MAC. */
struct igc_mac_operations {
	s32  (*init_params)(struct igc_hw *);
	s32  (*check_for_link)(struct igc_hw *);
	void (*clear_hw_cntrs)(struct igc_hw *);
	void (*clear_vfta)(struct igc_hw *);
	s32  (*get_bus_info)(struct igc_hw *);
	void (*set_lan_id)(struct igc_hw *);
	s32  (*get_link_up_info)(struct igc_hw *, u16 *, u16 *);
	void (*update_mc_addr_list)(struct igc_hw *, u8 *, u32);
	s32  (*reset_hw)(struct igc_hw *);
	s32  (*init_hw)(struct igc_hw *);
	s32  (*setup_link)(struct igc_hw *);
	s32  (*setup_physical_interface)(struct igc_hw *);
	void (*write_vfta)(struct igc_hw *, u32, u32);
	void (*config_collision_dist)(struct igc_hw *);
	int  (*rar_set)(struct igc_hw *, u8*, u32);
	s32  (*read_mac_addr)(struct igc_hw *);
	s32  (*validate_mdi_setting)(struct igc_hw *);
	s32  (*acquire_swfw_sync)(struct igc_hw *, u16);
	void (*release_swfw_sync)(struct igc_hw *, u16);
};

/* When to use various PHY register access functions:
 *
 *                 Func   Caller
 *   Function      Does   Does    When to use
 *   ~~~~~~~~~~~~  ~~~~~  ~~~~~~  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *   X_reg         L,P,A  n/a     for simple PHY reg accesses
 *   X_reg_locked  P,A    L       for multiple accesses of different regs
 *                                on different pages
 *   X_reg_page    A      L,P     for multiple accesses of different regs
 *                                on the same page
 *
 * Where X=[read|write], L=locking, P=sets page, A=register access
 *
 */
struct igc_phy_operations {
	s32  (*init_params)(struct igc_hw *);
	s32  (*acquire)(struct igc_hw *);
	s32  (*check_reset_block)(struct igc_hw *);
	s32  (*force_speed_duplex)(struct igc_hw *);
	s32  (*get_info)(struct igc_hw *);
	s32  (*set_page)(struct igc_hw *, u16);
	s32  (*read_reg)(struct igc_hw *, u32, u16 *);
	s32  (*read_reg_locked)(struct igc_hw *, u32, u16 *);
	s32  (*read_reg_page)(struct igc_hw *, u32, u16 *);
	void (*release)(struct igc_hw *);
	s32  (*reset)(struct igc_hw *);
	s32  (*set_d0_lplu_state)(struct igc_hw *, bool);
	s32  (*set_d3_lplu_state)(struct igc_hw *, bool);
	s32  (*write_reg)(struct igc_hw *, u32, u16);
	s32  (*write_reg_locked)(struct igc_hw *, u32, u16);
	s32  (*write_reg_page)(struct igc_hw *, u32, u16);
	void (*power_up)(struct igc_hw *);
	void (*power_down)(struct igc_hw *);
};

/* Function pointers for the NVM. */
struct igc_nvm_operations {
	s32  (*init_params)(struct igc_hw *);
	s32  (*acquire)(struct igc_hw *);
	s32  (*read)(struct igc_hw *, u16, u16, u16 *);
	void (*release)(struct igc_hw *);
	void (*reload)(struct igc_hw *);
	s32  (*update)(struct igc_hw *);
	s32  (*validate)(struct igc_hw *);
	s32  (*write)(struct igc_hw *, u16, u16, u16 *);
};

struct igc_info {
	s32 (*get_invariants)(struct igc_hw *hw);
	struct igc_mac_operations *mac_ops;
	const struct igc_phy_operations *phy_ops;
	struct igc_nvm_operations *nvm_ops;
};

extern const struct igc_info igc_i225_info;

struct igc_mac_info {
	struct igc_mac_operations ops;
	u8 addr[ETH_ADDR_LEN];
	u8 perm_addr[ETH_ADDR_LEN];

	enum igc_mac_type type;

	u32 mc_filter_type;

	u16 current_ifs_val;
	u16 ifs_max_val;
	u16 ifs_min_val;
	u16 ifs_ratio;
	u16 ifs_step_size;
	u16 mta_reg_count;
	u16 uta_reg_count;

	/* Maximum size of the MTA register table in all supported adapters */
#define MAX_MTA_REG 128
	u32 mta_shadow[MAX_MTA_REG];
	u16 rar_entry_count;

	u8  forced_speed_duplex;

	bool asf_firmware_present;
	bool autoneg;
	bool get_link_status;
	u32  max_frame_size;
};

struct igc_phy_info {
	struct igc_phy_operations ops;
	enum igc_phy_type type;

	enum igc_smart_speed smart_speed;

	u32 addr;
	u32 id;
	u32 reset_delay_us; /* in usec */
	u32 revision;

	enum igc_media_type media_type;

	u16 autoneg_advertised;
	u16 autoneg_mask;

	u8 mdix;

	bool polarity_correction;
	bool speed_downgraded;
	bool autoneg_wait_to_complete;
};

struct igc_nvm_info {
	struct igc_nvm_operations ops;
	enum igc_nvm_type type;

	u16 word_size;
	u16 delay_usec;
	u16 address_bits;
	u16 opcode_bits;
	u16 page_size;
};

struct igc_bus_info {
	enum igc_bus_type type;
	enum igc_bus_speed speed;
	enum igc_bus_width width;

	u16 func;
	u16 pci_cmd_word;
};

struct igc_fc_info {
	u32 high_water;  /* Flow control high-water mark */
	u32 low_water;  /* Flow control low-water mark */
	u16 pause_time;  /* Flow control pause timer */
	u16 refresh_time;  /* Flow control refresh timer */
	bool send_xon;  /* Flow control send XON */
	bool strict_ieee;  /* Strict IEEE mode */
	enum igc_fc_mode current_mode;  /* FC mode in effect */
	enum igc_fc_mode requested_mode;  /* FC mode requested by caller */
};

struct igc_dev_spec_i225 {
	bool eee_disable;
	bool clear_semaphore_once;
	u32 mtu;
};

struct igc_hw {
	void *back;

	u8 *hw_addr;
	u8 *flash_address;
	unsigned long io_base;

	struct igc_mac_info  mac;
	struct igc_fc_info   fc;
	struct igc_phy_info  phy;
	struct igc_nvm_info  nvm;
	struct igc_bus_info  bus;

	union {
		struct igc_dev_spec_i225 _i225;
	} dev_spec;

	u16 device_id;
	u16 subsystem_vendor_id;
	u16 subsystem_device_id;
	u16 vendor_id;

	u8  revision_id;
};

#include "igc_i225.h"
#include "igc_base.h"

/* These functions must be implemented by drivers */
s32  igc_read_pcie_cap_reg(struct igc_hw *hw, u32 reg, u16 *value);
s32  igc_write_pcie_cap_reg(struct igc_hw *hw, u32 reg, u16 *value);
void igc_read_pci_cfg(struct igc_hw *hw, u32 reg, u16 *value);
void igc_write_pci_cfg(struct igc_hw *hw, u32 reg, u16 *value);

#endif
