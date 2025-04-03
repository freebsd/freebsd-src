/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2015 Adrian Chadd <adrian@FreeBSD.org>
 * Copyright (c) 2025 The FreeBSD Foundation
 *
 * Portions of this software were developed by Tom Jones <thj@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
 */

#ifndef	__IF_IWX_DEBUG_H__
#define	__IF_IWX_DEBUG_H__

#ifdef	IWX_DEBUG
enum {
	IWX_DEBUG_XMIT		= 0x00000001,	/* basic xmit operation */
	IWX_DEBUG_RECV		= 0x00000002,	/* basic recv operation */
	IWX_DEBUG_STATE		= 0x00000004,	/* 802.11 state transitions */
	IWX_DEBUG_TXPOW		= 0x00000008,	/* tx power processing */
	IWX_DEBUG_RESET		= 0x00000010,	/* reset processing */
	IWX_DEBUG_OPS		= 0x00000020,	/* iwx_ops processing */
	IWX_DEBUG_BEACON 	= 0x00000040,	/* beacon handling */
	IWX_DEBUG_WATCHDOG 	= 0x00000080,	/* watchdog timeout */
	IWX_DEBUG_INTR		= 0x00000100,	/* ISR */
	IWX_DEBUG_CALIBRATE	= 0x00000200,	/* periodic calibration */
	IWX_DEBUG_NODE		= 0x00000400,	/* node management */
	IWX_DEBUG_LED		= 0x00000800,	/* led management */
	IWX_DEBUG_CMD		= 0x00001000,	/* cmd submission */
	IWX_DEBUG_TXRATE	= 0x00002000,	/* TX rate debugging */
	IWX_DEBUG_PWRSAVE	= 0x00004000,	/* Power save operations */
	IWX_DEBUG_SCAN		= 0x00008000,	/* Scan related operations */
	IWX_DEBUG_STATS		= 0x00010000,	/* Statistics updates */
	IWX_DEBUG_FIRMWARE_TLV	= 0x00020000,	/* Firmware TLV parsing */
	IWX_DEBUG_TRANS		= 0x00040000,	/* Transport layer (eg PCIe) */
	IWX_DEBUG_EEPROM	= 0x00080000,	/* EEPROM/channel information */
	IWX_DEBUG_TEMP		= 0x00100000,	/* Thermal Sensor handling */
	IWX_DEBUG_FW		= 0x00200000,	/* Firmware management */
	IWX_DEBUG_LAR		= 0x00400000,	/* Location Aware Regulatory */
	IWX_DEBUG_TE		= 0x00800000,	/* Time Event handling */
						/* 0x0n000000 are available */
	IWX_DEBUG_NI		= 0x10000000,	/* Not Implemented  */
	IWX_DEBUG_REGISTER	= 0x20000000,	/* print chipset register */
	IWX_DEBUG_TRACE		= 0x40000000,	/* Print begin and start driver function */
	IWX_DEBUG_FATAL		= 0x80000000,	/* fatal errors */
	IWX_DEBUG_ANY		= 0xffffffff
};

#define IWX_DPRINTF(sc, m, fmt, ...) do {			\
	if (sc->sc_debug & (m))				\
		device_printf(sc->sc_dev, fmt, ##__VA_ARGS__);	\
} while (0)
#else
#define IWX_DPRINTF(sc, m, fmt, ...) do { (void) sc; } while (0)
#endif

void print_opcode(const char *, int, int, uint32_t);
void print_ratenflags(const char *, int , uint32_t , int );
void iwx_dump_cmd(uint32_t , void *, uint16_t, const char *, int);
void iwx_bbl_add_entry(uint32_t, int, int);
void iwx_bbl_print_log(void);

#define IWX_BBL_NONE	0x00
#define IWX_BBL_PKT_TX	0x01
#define IWX_BBL_PKT_RX	0x02
#define IWX_BBL_PKT_DUP 0x04
#define IWX_BBL_CMD_TX	0x10
#define IWX_BBL_CMD_RX	0x20
#define IWX_BBL_ANY	0xFF

static int print_mask = IWX_BBL_NONE; //IWX_BBL_NONE | IWX_BBL_CMD_TX;
static int print_codes[][2] = {
#if 0
	for example:
	IWX_LEGACY_GROUP, IWX_ADD_STA_KEY,
	IWX_LEGACY_GROUP, IWX_SCD_QUEUE_CONFIG_CMD,
	IWX_LEGACY_GROUP, IWX_ADD_STA,
	IWX_LEGACY_GROUP, IWX_REMOVE_STA,
#endif
};

static int dump_mask = IWX_BBL_NONE;
static int dump_codes[][2] = {
#if 0
	for example:
	IWX_LEGACY_GROUP, IWX_ADD_STA_KEY,
	IWX_LEGACY_GROUP, IWX_SCD_QUEUE_CONFIG_CMD,
	IWX_LEGACY_GROUP, IWX_ADD_STA,
	IWX_LEGACY_GROUP, IWX_REMOVE_STA,
#endif
};

struct opcode_label {
	uint8_t opcode;
	const char *label;
};

static struct opcode_label command_group[] = {
	{ 0x0, "IWX_LEGACY_GROUP"}, 
	{ 0x1, "IWX_LONG_GROUP"},
	{ 0x2, "IWX_SYSTEM_GROUP"},
	{ 0x3, "IWX_MAC_CONF_GROUP"},
	{ 0x4, "IWX_PHY_OPS_GROUP"},
	{ 0x5, "IWX_DATA_PATH_GROUP"},
	{ 0xb, "IWX_PROT_OFFLOAD_GROUP"},
	{ 0xc, "IWX_REGULATORY_AND_NVM_GROUP"},
	{ 0, NULL }
};

static struct opcode_label legacy_opcodes[] = {
	{ 0xc0, "IWX_REPLY_RX_PHY_CMD" },
	{ 0xc1, "IWX_REPLY_RX_MPDU_CMD" },
	{ 0xc2, "IWX_BAR_FRAME_RELEASE" },
	{ 0xc3, "IWX_FRAME_RELEASE" },
	{ 0xc5, "IWX_BA_NOTIF" },
	{ 0x62, "IWX_TEMPERATURE_NOTIFICATION" },
	{ 0xc8, "IWX_MCC_UPDATE_CMD" },
	{ 0xc9, "IWX_MCC_CHUB_UPDATE_CMD" },
	{ 0x65, "IWX_CALIBRATION_CFG_CMD" },
	{ 0x66, "IWX_CALIBRATION_RES_NOTIFICATION" },
	{ 0x67, "IWX_CALIBRATION_COMPLETE_NOTIFICATION" },
	{ 0x68, "IWX_RADIO_VERSION_NOTIFICATION" },
	{ 0x00, "IWX_CMD_DTS_MEASUREMENT_TRIGGER_WIDE" },
	{ 0x01, "IWX_SOC_CONFIGURATION_CMD" },
	{ 0x02, "IWX_REPLY_ERROR" },
	{ 0x03, "IWX_CTDP_CONFIG_CMD" },
	{ 0x04, "IWX_INIT_COMPLETE_NOTIF" },
	{ 0x05, "IWX_SESSION_PROTECTION_CMD" },
	{ 0x5d, "IWX_BT_COEX_CI" },
	{ 0x07, "IWX_FW_ERROR_RECOVERY_CMD" },
	{ 0x08, "IWX_RLC_CONFIG_CMD" },
	{ 0xd0, "IWX_MCAST_FILTER_CMD" },
	{ 0xd1, "IWX_REPLY_SF_CFG_CMD" },
	{ 0xd2, "IWX_REPLY_BEACON_FILTERING_CMD" },
	{ 0xd3, "IWX_D3_CONFIG_CMD" },
	{ 0xd4, "IWX_PROT_OFFLOAD_CONFIG_CMD" },
	{ 0xd5, "IWX_OFFLOADS_QUERY_CMD" },
	{ 0xd6, "IWX_REMOTE_WAKE_CONFIG_CMD" },
	{ 0x77, "IWX_POWER_TABLE_CMD" },
	{ 0x78, "IWX_PSM_UAPSD_AP_MISBEHAVING_NOTIFICATION" },
	{ 0xcc, "IWX_BT_COEX_PRIO_TABLE" },
	{ 0xcd, "IWX_BT_COEX_PROT_ENV" },
	{ 0xce, "IWX_BT_PROFILE_NOTIFICATION" },
	{ 0x6a, "IWX_PHY_CONFIGURATION_CMD" },
	{ 0x16, "IWX_RX_BAID_ALLOCATION_CONFIG_CMD" },
	{ 0x17, "IWX_ADD_STA_KEY" },
	{ 0x18, "IWX_ADD_STA" },
	{ 0x19, "IWX_REMOVE_STA" },
	{ 0xe0, "IWX_WOWLAN_PATTERNS" },
	{ 0xe1, "IWX_WOWLAN_CONFIGURATION" },
	{ 0xe2, "IWX_WOWLAN_TSC_RSC_PARAM" },
	{ 0xe3, "IWX_WOWLAN_TKIP_PARAM" },
	{ 0xe4, "IWX_WOWLAN_KEK_KCK_MATERIAL" },
	{ 0xe5, "IWX_WOWLAN_GET_STATUSES" },
	{ 0xe6, "IWX_WOWLAN_TX_POWER_PER_DB" },
	{ 0x0f, "IWX_SCAN_COMPLETE_UMAC" },
	{ 0x88, "IWX_NVM_ACCESS_CMD" },
	{ 0x20, "IWX_WEP_KEY" },
	{ 0xdc, "IWX_CMD_DTS_MEASUREMENT_TRIGGER" },
	{ 0xdd, "IWX_DTS_MEASUREMENT_NOTIFICATION" },
	{ 0x28, "IWX_MAC_CONTEXT_CMD" },
	{ 0x29, "IWX_TIME_EVENT_CMD" },
	{ 0x01, "IWX_ALIVE" },
	{ 0xf0, "IWX_REPLY_DEBUG_CMD" },
	{ 0x90, "IWX_BEACON_NOTIFICATION" },
	{ 0xf5, "IWX_RX_NO_DATA_NOTIF" },
	{ 0x08, "IWX_PHY_CONTEXT_CMD" },
	{ 0x91, "IWX_BEACON_TEMPLATE_CMD" },
	{ 0xf6, "IWX_THERMAL_DUAL_CHAIN_REQUEST" },
	{ 0x09, "IWX_DBG_CFG" },
	{ 0xf7, "IWX_DEBUG_LOG_MSG" },
	{ 0x1c, "IWX_TX_CMD" },
	{ 0x1d, "IWX_SCD_QUEUE_CFG" },
	{ 0x1e, "IWX_TXPATH_FLUSH" },
	{ 0x1f, "IWX_MGMT_MCAST_KEY" },
	{ 0x98, "IWX_TX_ANT_CONFIGURATION_CMD" },
	{ 0xee, "IWX_LTR_CONFIG" },
	{ 0x8e, "IWX_SET_CALIB_DEFAULT_CMD" },
	{ 0xFE, "IWX_CT_KILL_NOTIFICATION" },
	{ 0xFF, "IWX_DTS_MEASUREMENT_NOTIF_WIDE" },
	{ 0x2a, "IWX_TIME_EVENT_NOTIFICATION" },
	{ 0x2b, "IWX_BINDING_CONTEXT_CMD" },
	{ 0x2c, "IWX_TIME_QUOTA_CMD" },
	{ 0x2d, "IWX_NON_QOS_TX_COUNTER_CMD" },
	{ 0xa0, "IWX_CARD_STATE_CMD" },
	{ 0xa1, "IWX_CARD_STATE_NOTIFICATION" },
	{ 0xa2, "IWX_MISSED_BEACONS_NOTIFICATION" },
	{ 0x0c, "IWX_SCAN_CFG_CMD" },
	{ 0x0d, "IWX_SCAN_REQ_UMAC" },
	{ 0xfb, "IWX_SESSION_PROTECTION_NOTIF" },
	{ 0x0e, "IWX_SCAN_ABORT_UMAC" },
	{ 0xfe, "IWX_PNVM_INIT_COMPLETE" },
	{ 0xa9, "IWX_MAC_PM_POWER_TABLE" },
	{ 0xff, "IWX_FSEQ_VER_MISMATCH_NOTIFICATION | IWX_REPLY_MAX" },
	{ 0x9b, "IWX_BT_CONFIG" },
	{ 0x9c, "IWX_STATISTICS_CMD" },
	{ 0x9d, "IWX_STATISTICS_NOTIFICATION" },
	{ 0x9f, "IWX_REDUCE_TX_POWER_CMD" },
	{ 0xb1, "IWX_MFUART_LOAD_NOTIFICATION" },
	{ 0xb5, "IWX_SCAN_ITERATION_COMPLETE_UMAC" },
	{ 0x54, "IWX_NET_DETECT_CONFIG_CMD" },
	{ 0x56, "IWX_NET_DETECT_PROFILES_QUERY_CMD" },
	{ 0x57, "IWX_NET_DETECT_PROFILES_CMD" },
	{ 0x58, "IWX_NET_DETECT_HOTSPOTS_CMD" },
	{ 0x59, "IWX_NET_DETECT_HOTSPOTS_QUERY_CMD" },
	{ 0, NULL }
};

/* SYSTEM_GROUP group subcommand IDs */
static struct opcode_label system_opcodes[] = {
	{ 0x00, "IWX_SHARED_MEM_CFG_CMD" },
	{ 0x01, "IWX_SOC_CONFIGURATION_CMD" },
	{ 0x03, "IWX_INIT_EXTENDED_CFG_CMD" },
	{ 0x07, "IWX_FW_ERROR_RECOVERY_CMD" },
	{ 0xff, "IWX_FSEQ_VER_MISMATCH_NOTIFICATION | IWX_REPLY_MAX" },
	{ 0, NULL }
};

/* MAC_CONF group subcommand IDs */
static struct opcode_label macconf_opcodes[] = {
	{ 0x05, "IWX_SESSION_PROTECTION_CMD" },
	{ 0xfb, "IWX_SESSION_PROTECTION_NOTIF" },
	{ 0, NULL }
};

/* DATA_PATH group subcommand IDs */
static struct opcode_label data_opcodes[] = {
	{ 0x00, "IWX_DQA_ENABLE_CMD" },
	{ 0x08, "IWX_RLC_CONFIG_CMD" },
	{ 0x0f, "IWX_TLC_MNG_CONFIG_CMD" },
	{ 0x16, "IWX_RX_BAID_ALLOCATION_CONFIG_CMD" },
	{ 0x17, "IWX_SCD_QUEUE_CONFIG_CMD" },
	{ 0xf5, "IWX_RX_NO_DATA_NOTIF" },
	{ 0xf6, "IWX_THERMAL_DUAL_CHAIN_REQUEST" },
	{ 0xf7, "IWX_TLC_MNG_UPDATE_NOTIF" },
	{ 0, NULL }
};

/* REGULATORY_AND_NVM group subcommand IDs */
static struct opcode_label reg_opcodes[] = {
	{ 0x00, "IWX_NVM_ACCESS_COMPLETE" },
	{ 0x02, "IWX_NVM_GET_INFO " },
	{ 0xfe, "IWX_PNVM_INIT_COMPLETE" },
	{ 0, NULL }
};

/* PHY_OPS subcommand IDs */
static struct opcode_label phyops_opcodes[] = {
	{0x00, 	"IWX_CMD_DTS_MEASUREMENT_TRIGGER_WIDE"},
	{0x03,	"IWX_CTDP_CONFIG_CMD"},
	{0x04,	"IWX_TEMP_REPORTING_THRESHOLDS_CMD"},
	{0xFE,	"IWX_CT_KILL_NOTIFICATION"},
	{0xFF,	"IWX_DTS_MEASUREMENT_NOTIF_WIDE"},
};

struct iwx_bbl_entry {
	uint8_t type;
	uint32_t code;
	uint32_t seq;
	uint32_t ticks;
	uint32_t count;
};
#define IWX_BBL_ENTRIES	2000
static struct iwx_bbl_entry iwx_bb_log[IWX_BBL_ENTRIES];

#endif	/* __IF_IWX_DEBUG_H__ */
