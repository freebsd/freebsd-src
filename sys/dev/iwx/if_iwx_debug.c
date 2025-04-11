/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2015 Adrian Chadd <adrian@FreeBSD.org>
 * Copyright (c) 2025 The FreeBSD Foundation
 *
 * Portions of this software were developed by Tom Jones <thj@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <net/ethernet.h>

#include <net80211/ieee80211.h>

#define le32_to_cpup(_a_) (le32toh(*(const uint32_t *)(_a_)))

#include <dev/iwx/if_iwxreg.h>
#include <dev/iwx/if_iwx_debug.h>

static int print_codes[][2] = {
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

static const char *
get_label(struct opcode_label *table, uint8_t opcode)
{
	struct opcode_label *op = table;
	while(op->label != NULL) {
		if (op->opcode == opcode)
			return op->label;
		op++;
	}
	return "NOT FOUND IN TABLE";
}

static struct opcode_label *
get_table(uint8_t group)
{
	switch (group)
	{
	case IWX_LEGACY_GROUP:
	case IWX_LONG_GROUP:
		return legacy_opcodes;
		break;
	case IWX_SYSTEM_GROUP:
		return system_opcodes;
		break;
	case IWX_MAC_CONF_GROUP:
		return macconf_opcodes;
		break;
	case IWX_DATA_PATH_GROUP:
		return data_opcodes;
		break;
	case IWX_REGULATORY_AND_NVM_GROUP:
		return reg_opcodes;
		break;
	case IWX_PHY_OPS_GROUP:
		return phyops_opcodes;
		break;
	case IWX_PROT_OFFLOAD_GROUP:
		break;
	}
	return NULL;
}

void
print_opcode(const char *func, int line, uint32_t code)
{
	int print = 0;
	uint8_t opcode = iwx_cmd_opcode(code);
	uint8_t group = iwx_cmd_groupid(code);

	struct opcode_label *table = get_table(group);
	if (table == NULL) {
		printf("Couldn't find opcode table for 0x%08x", code);
		return;
	}

	for (int i = 0; i < nitems(print_codes); i++)
		if (print_codes[i][0] == group && print_codes[i][1] == opcode)
			print = 1;

	if (print) {
		printf("%s:%d \t%s\t%s\t(0x%08x)\n", func, line,
		    get_label(command_group, group),
		    get_label(table, opcode), code);
	}
}

void
print_ratenflags(const char *func, int line, uint32_t flags, int ver)
{
	printf("%s:%d\n\t flags 0x%08x ", func, line, flags);

	if (ver >= 2) {
		printf(" rate_n_flags version 2\n");

		uint32_t type = (flags & IWX_RATE_MCS_MOD_TYPE_MSK) >> IWX_RATE_MCS_MOD_TYPE_POS;

		switch(type)
		{
		case 0:
			printf("\t(0) Legacy CCK: ");
			switch (flags & IWX_RATE_LEGACY_RATE_MSK)
			{
			case 0:
				printf("(0) 0xa - 1 Mbps\n");
				break;
			case 1:
				printf("(1) 0x14 - 2 Mbps\n");
				break;
			case 2:
				printf("(2) 0x37 - 5.5 Mbps\n");
				break;
			case 3:
				printf("(3) 0x6e - 11 nbps\n");
				break;
			}
			break;
		case 1:
			printf("\t(1) Legacy OFDM \n");
			switch (flags & IWX_RATE_LEGACY_RATE_MSK)
			{
			case 0:
				printf("(0) 6 Mbps\n");
				break;
			case 1:
				printf("(1) 9 Mbps\n");
				break;
			case 2:
				printf("(2) 12 Mbps\n");
				break;
			case 3:
				printf("(3) 18 Mbps\n");
				break;
			case 4:
				printf("(4) 24 Mbps\n");
				break;
			case 5:
				printf("(5) 36 Mbps\n");
				break;
			case 6:
				printf("(6) 48 Mbps\n");
				break;
			case 7:
				printf("(7) 54 Mbps\n");
				break;
			}
			break;
		case 2:
			printf("\t(2) High-throughput (HT)\n");
			break;
		case 3:
			printf("\t(3) Very High-throughput (VHT) \n");
			break;
		case 4:
			printf("\t(4) High-efficiency (HE)\n");
			break;
		case 5:
			printf("\t(5) Extremely High-throughput (EHT)\n");
			break;
		default:
			printf("invalid\n");
		}

		/* Not a legacy rate. */
		if (type > 1) {
			printf("\tMCS %d ", IWX_RATE_HT_MCS_INDEX(flags));
			switch((flags & IWX_RATE_MCS_CHAN_WIDTH_MSK) >> IWX_RATE_MCS_CHAN_WIDTH_POS)
			{
			case 0:
				printf("20MHz ");
				break;
			case 1:
				printf("40MHz ");
				break;
			case 2:
				printf("80MHz ");
				break;
			case 3:
				printf("160MHz ");
				break;
			case 4:
				printf("320MHz ");
				break;

			}
			printf("antennas: (%s|%s) ",
				flags & (1 << 14) ? "A" : " ",
				flags & (1 << 15) ? "B" : " ");
			if (flags & (1 << 16))
				printf("ldpc ");
			printf("\n");
		}
	} else {
		printf("%s:%d rate_n_flags versions other than < 2 not implemented",
		    __func__, __LINE__);
	}
}
