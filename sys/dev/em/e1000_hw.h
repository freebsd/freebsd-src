/*******************************************************************************

  Copyright (c) 2001-2007, Intel Corporation 
  All rights reserved.
  
  Redistribution and use in source and binary forms, with or without 
  modification, are permitted provided that the following conditions are met:
  
   1. Redistributions of source code must retain the above copyright notice, 
      this list of conditions and the following disclaimer.
  
   2. Redistributions in binary form must reproduce the above copyright 
      notice, this list of conditions and the following disclaimer in the 
      documentation and/or other materials provided with the distribution.
  
   3. Neither the name of the Intel Corporation nor the names of its 
      contributors may be used to endorse or promote products derived from 
      this software without specific prior written permission.
  
  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE 
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF 
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/
/* $FreeBSD: src/sys/dev/em/e1000_hw.h,v 1.3.4.1 2007/11/28 23:24:37 jfv Exp $ */


#ifndef _E1000_HW_H_
#define _E1000_HW_H_

#include "e1000_osdep.h"
#include "e1000_regs.h"
#include "e1000_defines.h"

struct e1000_hw;

#ifndef NO_82542_SUPPORT
#define E1000_DEV_ID_82542                    0x1000
#endif
#define E1000_DEV_ID_82543GC_FIBER            0x1001
#define E1000_DEV_ID_82543GC_COPPER           0x1004
#define E1000_DEV_ID_82544EI_COPPER           0x1008
#define E1000_DEV_ID_82544EI_FIBER            0x1009
#define E1000_DEV_ID_82544GC_COPPER           0x100C
#define E1000_DEV_ID_82544GC_LOM              0x100D
#define E1000_DEV_ID_82540EM                  0x100E
#define E1000_DEV_ID_82540EM_LOM              0x1015
#define E1000_DEV_ID_82540EP_LOM              0x1016
#define E1000_DEV_ID_82540EP                  0x1017
#define E1000_DEV_ID_82540EP_LP               0x101E
#define E1000_DEV_ID_82545EM_COPPER           0x100F
#define E1000_DEV_ID_82545EM_FIBER            0x1011
#define E1000_DEV_ID_82545GM_COPPER           0x1026
#define E1000_DEV_ID_82545GM_FIBER            0x1027
#define E1000_DEV_ID_82545GM_SERDES           0x1028
#define E1000_DEV_ID_82546EB_COPPER           0x1010
#define E1000_DEV_ID_82546EB_FIBER            0x1012
#define E1000_DEV_ID_82546EB_QUAD_COPPER      0x101D
#define E1000_DEV_ID_82546GB_COPPER           0x1079
#define E1000_DEV_ID_82546GB_FIBER            0x107A
#define E1000_DEV_ID_82546GB_SERDES           0x107B
#define E1000_DEV_ID_82546GB_PCIE             0x108A
#define E1000_DEV_ID_82546GB_QUAD_COPPER      0x1099
#define E1000_DEV_ID_82546GB_QUAD_COPPER_KSP3 0x10B5
#define E1000_DEV_ID_82541EI                  0x1013
#define E1000_DEV_ID_82541EI_MOBILE           0x1018
#define E1000_DEV_ID_82541ER_LOM              0x1014
#define E1000_DEV_ID_82541ER                  0x1078
#define E1000_DEV_ID_82541GI                  0x1076
#define E1000_DEV_ID_82541GI_LF               0x107C
#define E1000_DEV_ID_82541GI_MOBILE           0x1077
#define E1000_DEV_ID_82547EI                  0x1019
#define E1000_DEV_ID_82547EI_MOBILE           0x101A
#define E1000_DEV_ID_82547GI                  0x1075
#define E1000_DEV_ID_82571EB_COPPER           0x105E
#define E1000_DEV_ID_82571EB_FIBER            0x105F
#define E1000_DEV_ID_82571EB_SERDES           0x1060
#define E1000_DEV_ID_82571EB_SERDES_DUAL      0x10D9
#define E1000_DEV_ID_82571EB_SERDES_QUAD      0x10DA
#define E1000_DEV_ID_82571EB_QUAD_COPPER      0x10A4
#define E1000_DEV_ID_82571PT_QUAD_COPPER      0x10D5
#define E1000_DEV_ID_82571EB_QUAD_FIBER       0x10A5
#define E1000_DEV_ID_82571EB_QUAD_COPPER_LP   0x10BC
#define E1000_DEV_ID_82572EI_COPPER           0x107D
#define E1000_DEV_ID_82572EI_FIBER            0x107E
#define E1000_DEV_ID_82572EI_SERDES           0x107F
#define E1000_DEV_ID_82572EI                  0x10B9
#define E1000_DEV_ID_82573E                   0x108B
#define E1000_DEV_ID_82573E_IAMT              0x108C
#define E1000_DEV_ID_82573L                   0x109A
#define E1000_DEV_ID_80003ES2LAN_COPPER_DPT   0x1096
#define E1000_DEV_ID_80003ES2LAN_SERDES_DPT   0x1098
#define E1000_DEV_ID_80003ES2LAN_COPPER_SPT   0x10BA
#define E1000_DEV_ID_80003ES2LAN_SERDES_SPT   0x10BB
#define E1000_DEV_ID_ICH8_IGP_M_AMT           0x1049
#define E1000_DEV_ID_ICH8_IGP_AMT             0x104A
#define E1000_DEV_ID_ICH8_IGP_C               0x104B
#define E1000_DEV_ID_ICH8_IFE                 0x104C
#define E1000_DEV_ID_ICH8_IFE_GT              0x10C4
#define E1000_DEV_ID_ICH8_IFE_G               0x10C5
#define E1000_DEV_ID_ICH8_IGP_M               0x104D
#define E1000_DEV_ID_ICH9_IGP_AMT             0x10BD
#define E1000_DEV_ID_ICH9_IGP_C               0x294C
#define E1000_DEV_ID_ICH9_IFE                 0x10C0
#define E1000_DEV_ID_ICH9_IFE_GT              0x10C3
#define E1000_DEV_ID_ICH9_IFE_G               0x10C2
#define E1000_DEV_ID_82575EB_COPPER           0x10A7
#define E1000_DEV_ID_82575EB_FIBER_SERDES     0x10A9
#define E1000_DEV_ID_82575GB_QUAD_COPPER      0x10D6

#define E1000_REVISION_0 0
#define E1000_REVISION_1 1
#define E1000_REVISION_2 2
#define E1000_REVISION_3 3
#define E1000_REVISION_4 4

#define E1000_FUNC_0     0
#define E1000_FUNC_1     1

typedef enum {
	e1000_undefined = 0,
#ifndef NO_82542_SUPPORT
	e1000_82542,
#endif
	e1000_82543,
	e1000_82544,
	e1000_82540,
	e1000_82545,
	e1000_82545_rev_3,
	e1000_82546,
	e1000_82546_rev_3,
	e1000_82541,
	e1000_82541_rev_2,
	e1000_82547,
	e1000_82547_rev_2,
	e1000_82571,
	e1000_82572,
	e1000_82573,
	e1000_80003es2lan,
	e1000_ich8lan,
	e1000_ich9lan,
	e1000_82575,
	e1000_num_macs  /* List is 1-based, so subtract 1 for true count. */
} e1000_mac_type;

typedef enum {
	e1000_media_type_unknown = 0,
	e1000_media_type_copper = 1,
	e1000_media_type_fiber = 2,
	e1000_media_type_internal_serdes = 3,
	e1000_num_media_types
} e1000_media_type;

typedef enum {
	e1000_nvm_unknown = 0,
	e1000_nvm_none,
	e1000_nvm_eeprom_spi,
	e1000_nvm_eeprom_microwire,
	e1000_nvm_flash_hw,
	e1000_nvm_flash_sw
} e1000_nvm_type;

typedef enum {
	e1000_nvm_override_none = 0,
	e1000_nvm_override_spi_small,
	e1000_nvm_override_spi_large,
	e1000_nvm_override_microwire_small,
	e1000_nvm_override_microwire_large
} e1000_nvm_override;

typedef enum {
	e1000_phy_unknown = 0,
	e1000_phy_none,
	e1000_phy_m88,
	e1000_phy_igp,
	e1000_phy_igp_2,
	e1000_phy_gg82563,
	e1000_phy_igp_3,
	e1000_phy_ife,
} e1000_phy_type;

typedef enum {
	e1000_bus_type_unknown = 0,
	e1000_bus_type_pci,
	e1000_bus_type_pcix,
	e1000_bus_type_pci_express,
	e1000_bus_type_reserved
} e1000_bus_type;

typedef enum {
	e1000_bus_speed_unknown = 0,
	e1000_bus_speed_33,
	e1000_bus_speed_66,
	e1000_bus_speed_100,
	e1000_bus_speed_120,
	e1000_bus_speed_133,
	e1000_bus_speed_2500,
	e1000_bus_speed_5000,
	e1000_bus_speed_reserved
} e1000_bus_speed;

typedef enum {
	e1000_bus_width_unknown = 0,
	e1000_bus_width_pcie_x1,
	e1000_bus_width_pcie_x2,
	e1000_bus_width_pcie_x4 = 4,
	e1000_bus_width_pcie_x8 = 8,
	e1000_bus_width_32,
	e1000_bus_width_64,
	e1000_bus_width_reserved
} e1000_bus_width;

typedef enum {
	e1000_1000t_rx_status_not_ok = 0,
	e1000_1000t_rx_status_ok,
	e1000_1000t_rx_status_undefined = 0xFF
} e1000_1000t_rx_status;

typedef enum {
	e1000_rev_polarity_normal = 0,
	e1000_rev_polarity_reversed,
	e1000_rev_polarity_undefined = 0xFF
} e1000_rev_polarity;

typedef enum {
	e1000_fc_none = 0,
	e1000_fc_rx_pause,
	e1000_fc_tx_pause,
	e1000_fc_full,
	e1000_fc_default = 0xFF
} e1000_fc_type;

typedef enum {
	e1000_ffe_config_enabled = 0,
	e1000_ffe_config_active,
	e1000_ffe_config_blocked
} e1000_ffe_config;

typedef enum {
	e1000_dsp_config_disabled = 0,
	e1000_dsp_config_enabled,
	e1000_dsp_config_activated,
	e1000_dsp_config_undefined = 0xFF
} e1000_dsp_config;

/* Receive Descriptor */
struct e1000_rx_desc {
	u64 buffer_addr; /* Address of the descriptor's data buffer */
	u16 length;      /* Length of data DMAed into data buffer */
	u16 csum;        /* Packet checksum */
	u8  status;      /* Descriptor status */
	u8  errors;      /* Descriptor Errors */
	u16 special;
};

/* Receive Descriptor - Extended */
union e1000_rx_desc_extended {
	struct {
		u64 buffer_addr;
		u64 reserved;
	} read;
	struct {
		struct {
			u32 mrq;              /* Multiple Rx Queues */
			union {
				u32 rss;            /* RSS Hash */
				struct {
					u16 ip_id;  /* IP id */
					u16 csum;   /* Packet Checksum */
				} csum_ip;
			} hi_dword;
		} lower;
		struct {
			u32 status_error;     /* ext status/error */
			u16 length;
			u16 vlan;             /* VLAN tag */
		} upper;
	} wb;  /* writeback */
};

#define MAX_PS_BUFFERS 4
/* Receive Descriptor - Packet Split */
union e1000_rx_desc_packet_split {
	struct {
		/* one buffer for protocol header(s), three data buffers */
		u64 buffer_addr[MAX_PS_BUFFERS];
	} read;
	struct {
		struct {
			u32 mrq;              /* Multiple Rx Queues */
			union {
				u32 rss;              /* RSS Hash */
				struct {
					u16 ip_id;    /* IP id */
					u16 csum;     /* Packet Checksum */
				} csum_ip;
			} hi_dword;
		} lower;
		struct {
			u32 status_error;     /* ext status/error */
			u16 length0;          /* length of buffer 0 */
			u16 vlan;             /* VLAN tag */
		} middle;
		struct {
			u16 header_status;
			u16 length[3];        /* length of buffers 1-3 */
		} upper;
		u64 reserved;
	} wb; /* writeback */
};

/* Transmit Descriptor */
struct e1000_tx_desc {
	u64 buffer_addr;      /* Address of the descriptor's data buffer */
	union {
		u32 data;
		struct {
			u16 length;    /* Data buffer length */
			u8 cso;        /* Checksum offset */
			u8 cmd;        /* Descriptor control */
		} flags;
	} lower;
	union {
		u32 data;
		struct {
			u8 status;     /* Descriptor status */
			u8 css;        /* Checksum start */
			u16 special;
		} fields;
	} upper;
};

/* Offload Context Descriptor */
struct e1000_context_desc {
	union {
		u32 ip_config;
		struct {
			u8 ipcss;      /* IP checksum start */
			u8 ipcso;      /* IP checksum offset */
			u16 ipcse;     /* IP checksum end */
		} ip_fields;
	} lower_setup;
	union {
		u32 tcp_config;
		struct {
			u8 tucss;      /* TCP checksum start */
			u8 tucso;      /* TCP checksum offset */
			u16 tucse;     /* TCP checksum end */
		} tcp_fields;
	} upper_setup;
	u32 cmd_and_length;
	union {
		u32 data;
		struct {
			u8 status;     /* Descriptor status */
			u8 hdr_len;    /* Header length */
			u16 mss;       /* Maximum segment size */
		} fields;
	} tcp_seg_setup;
};

/* Offload data descriptor */
struct e1000_data_desc {
	u64 buffer_addr;   /* Address of the descriptor's buffer address */
	union {
		u32 data;
		struct {
			u16 length;    /* Data buffer length */
			u8 typ_len_ext;
			u8 cmd;
		} flags;
	} lower;
	union {
		u32 data;
		struct {
			u8 status;     /* Descriptor status */
			u8 popts;      /* Packet Options */
			u16 special;
		} fields;
	} upper;
};

/* Statistics counters collected by the MAC */
struct e1000_hw_stats {
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
	u64 cexterr;
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
	u64 tsctfc;
	u64 iac;
	u64 icrxptc;
	u64 icrxatc;
	u64 ictxptc;
	u64 ictxatc;
	u64 ictxqec;
	u64 ictxqmtc;
	u64 icrxdmtc;
	u64 icrxoc;
	u64 cbtmpc;
	u64 htdpmc;
	u64 cbrdpc;
	u64 cbrmpc;
	u64 rpthc;
	u64 hgptc;
	u64 htcbdpc;
	u64 hgorc;
	u64 hgotc;
	u64 lenerrs;
	u64 scvpc;
	u64 hrmpc;
};

struct e1000_phy_stats {
	u32 idle_errors;
	u32 receive_errors;
};

struct e1000_host_mng_dhcp_cookie {
	u32 signature;
	u8  status;
	u8  reserved0;
	u16 vlan_id;
	u32 reserved1;
	u16 reserved2;
	u8  reserved3;
	u8  checksum;
};

/* Host Interface "Rev 1" */
struct e1000_host_command_header {
	u8 command_id;
	u8 command_length;
	u8 command_options;
	u8 checksum;
};

#define E1000_HI_MAX_DATA_LENGTH     252
struct e1000_host_command_info {
	struct e1000_host_command_header command_header;
	u8 command_data[E1000_HI_MAX_DATA_LENGTH];
};

/* Host Interface "Rev 2" */
struct e1000_host_mng_command_header {
	u8  command_id;
	u8  checksum;
	u16 reserved1;
	u16 reserved2;
	u16 command_length;
};

#define E1000_HI_MAX_MNG_DATA_LENGTH 0x6F8
struct e1000_host_mng_command_info {
	struct e1000_host_mng_command_header command_header;
	u8 command_data[E1000_HI_MAX_MNG_DATA_LENGTH];
};

#include "e1000_mac.h"
#include "e1000_phy.h"
#include "e1000_nvm.h"
#include "e1000_manage.h"

struct e1000_functions {
	/* Function pointers for the MAC. */
	s32  (*init_mac_params)(struct e1000_hw *);
	s32  (*blink_led)(struct e1000_hw *);
	s32  (*check_for_link)(struct e1000_hw *);
	bool (*check_mng_mode)(struct e1000_hw *hw);
	s32  (*cleanup_led)(struct e1000_hw *);
	void (*clear_hw_cntrs)(struct e1000_hw *);
	void (*clear_vfta)(struct e1000_hw *);
	s32  (*get_bus_info)(struct e1000_hw *);
	s32  (*get_link_up_info)(struct e1000_hw *, u16 *, u16 *);
	s32  (*led_on)(struct e1000_hw *);
	s32  (*led_off)(struct e1000_hw *);
	void (*update_mc_addr_list)(struct e1000_hw *, u8 *, u32, u32,
	                            u32);
	void (*remove_device)(struct e1000_hw *);
	s32  (*reset_hw)(struct e1000_hw *);
	s32  (*init_hw)(struct e1000_hw *);
	s32  (*setup_link)(struct e1000_hw *);
	s32  (*setup_physical_interface)(struct e1000_hw *);
	s32  (*setup_led)(struct e1000_hw *);
	void (*write_vfta)(struct e1000_hw *, u32, u32);
	void (*mta_set)(struct e1000_hw *, u32);
	void (*config_collision_dist)(struct e1000_hw*);
	void (*rar_set)(struct e1000_hw*, u8*, u32);
	s32  (*read_mac_addr)(struct e1000_hw*);
	s32  (*validate_mdi_setting)(struct e1000_hw*);
	s32  (*mng_host_if_write)(struct e1000_hw*, u8*, u16, u16, u8*);
	s32  (*mng_write_cmd_header)(struct e1000_hw *hw,
                      struct e1000_host_mng_command_header*);
	s32  (*mng_enable_host_if)(struct e1000_hw*);
	s32  (*wait_autoneg)(struct e1000_hw*);

	/* Function pointers for the PHY. */
	s32  (*init_phy_params)(struct e1000_hw *);
	s32  (*acquire_phy)(struct e1000_hw *);
	s32  (*check_polarity)(struct e1000_hw *);
	s32  (*check_reset_block)(struct e1000_hw *);
	s32  (*commit_phy)(struct e1000_hw *);
	s32  (*force_speed_duplex)(struct e1000_hw *);
	s32  (*get_cfg_done)(struct e1000_hw *hw);
	s32  (*get_cable_length)(struct e1000_hw *);
	s32  (*get_phy_info)(struct e1000_hw *);
	s32  (*read_phy_reg)(struct e1000_hw *, u32, u16 *);
	void (*release_phy)(struct e1000_hw *);
	s32  (*reset_phy)(struct e1000_hw *);
	s32  (*set_d0_lplu_state)(struct e1000_hw *, bool);
	s32  (*set_d3_lplu_state)(struct e1000_hw *, bool);
	s32  (*write_phy_reg)(struct e1000_hw *, u32, u16);
	void (*power_up_phy)(struct e1000_hw *);
	void (*power_down_phy)(struct e1000_hw *);

	/* Function pointers for the NVM. */
	s32  (*init_nvm_params)(struct e1000_hw *);
	s32  (*acquire_nvm)(struct e1000_hw *);
	s32  (*read_nvm)(struct e1000_hw *, u16, u16, u16 *);
	void (*release_nvm)(struct e1000_hw *);
	void (*reload_nvm)(struct e1000_hw *);
	s32  (*update_nvm)(struct e1000_hw *);
	s32  (*valid_led_default)(struct e1000_hw *, u16 *);
	s32  (*validate_nvm)(struct e1000_hw *);
	s32  (*write_nvm)(struct e1000_hw *, u16, u16, u16 *);
};

struct e1000_mac_info {
	u8 addr[6];
	u8 perm_addr[6];

	e1000_mac_type type;

	u32 collision_delta;
	u32 ledctl_default;
	u32 ledctl_mode1;
	u32 ledctl_mode2;
	u32 mc_filter_type;
	u32 tx_packet_delta;
	u32 txcw;

	u16 current_ifs_val;
	u16 ifs_max_val;
	u16 ifs_min_val;
	u16 ifs_ratio;
	u16 ifs_step_size;
	u16 mta_reg_count;
	u16 rar_entry_count;

	u8  forced_speed_duplex;

	bool adaptive_ifs;
	bool arc_subsystem_valid;
	bool asf_firmware_present;
	bool autoneg;
	bool autoneg_failed;
	bool disable_av;
	bool disable_hw_init_bits;
	bool get_link_status;
	bool ifs_params_forced;
	bool in_ifs_mode;
	bool report_tx_early;
	bool serdes_has_link;
	bool tx_pkt_filtering;
};

struct e1000_phy_info {
	e1000_phy_type type;

	e1000_1000t_rx_status local_rx;
	e1000_1000t_rx_status remote_rx;
	e1000_ms_type ms_type;
	e1000_ms_type original_ms_type;
	e1000_rev_polarity cable_polarity;
	e1000_smart_speed smart_speed;

	u32 addr;
	u32 id;
	u32 reset_delay_us; /* in usec */
	u32 revision;

	e1000_media_type media_type;

	u16 autoneg_advertised;
	u16 autoneg_mask;
	u16 cable_length;
	u16 max_cable_length;
	u16 min_cable_length;

	u8 mdix;

	bool disable_polarity_correction;
	bool is_mdix;
	bool polarity_correction;
	bool reset_disable;
	bool speed_downgraded;
	bool autoneg_wait_to_complete;
};

struct e1000_nvm_info {
	e1000_nvm_type type;
	e1000_nvm_override override;

	u32 flash_bank_size;
	u32 flash_base_addr;

	u16 word_size;
	u16 delay_usec;
	u16 address_bits;
	u16 opcode_bits;
	u16 page_size;
};

struct e1000_bus_info {
	e1000_bus_type type;
	e1000_bus_speed speed;
	e1000_bus_width width;

	u32 snoop;

	u16 func;
	u16 pci_cmd_word;
};

struct e1000_fc_info {
	u32 high_water;     /* Flow control high-water mark */
	u32 low_water;      /* Flow control low-water mark */
	u16 pause_time;     /* Flow control pause timer */
	bool send_xon;      /* Flow control send XON */
	bool strict_ieee;   /* Strict IEEE mode */
	e1000_fc_type type; /* Type of flow control */
	e1000_fc_type original_type;
};

struct e1000_hw {
	void *back;
	void *dev_spec;

	u8 *hw_addr;
	u8 *flash_address;
	unsigned long io_base;

	struct e1000_functions func;
	struct e1000_mac_info  mac;
	struct e1000_fc_info   fc;
	struct e1000_phy_info  phy;
	struct e1000_nvm_info  nvm;
	struct e1000_bus_info  bus;
	struct e1000_host_mng_dhcp_cookie mng_cookie;

	u32 dev_spec_size;

	u16 device_id;
	u16 subsystem_vendor_id;
	u16 subsystem_device_id;
	u16 vendor_id;

	u8  revision_id;
};

/* These functions must be implemented by drivers */
void e1000_pci_clear_mwi(struct e1000_hw *hw);
void e1000_pci_set_mwi(struct e1000_hw *hw);
s32  e1000_alloc_zeroed_dev_spec_struct(struct e1000_hw *hw, u32 size);
s32  e1000_read_pcie_cap_reg(struct e1000_hw *hw, u32 reg, u16 *value);
void e1000_free_dev_spec_struct(struct e1000_hw *hw);
void e1000_read_pci_cfg(struct e1000_hw *hw, u32 reg, u16 *value);
void e1000_write_pci_cfg(struct e1000_hw *hw, u32 reg, u16 *value);

#endif
