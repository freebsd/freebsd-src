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

static uint16_t bbl_idx = 0;
static uint32_t bbl_seq = 0;
static uint8_t bbl_compress = 1;

static const char *
iwx_bbl_to_str(int type)
{
	switch(type) {
	case IWX_BBL_PKT_TX:
		return ("IWX_BBL_PKT_TX");
	case IWX_BBL_PKT_RX:
		return ("IWX_BBL_PKT_RX");
	case IWX_BBL_PKT_DUP:
		return ("IWX_BBL_PKT_DUP");
	case IWX_BBL_CMD_TX:
		return ("IWX_BBL_CMD_TX");
	case IWX_BBL_CMD_RX:
		return ("IWX_BBL_CMD_RX");
	case IWX_BBL_ANY:
		return ("IWX_BBL_ANY");
	default:
		return ("ERROR");
	}
}

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
print_opcode(const char *func, int line, int type, uint32_t code)
{
	int print = print_mask & type;
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
		printf("%s:%d %s\t%s\t%s\t(0x%08x)\n", func, line,
		    iwx_bbl_to_str(type), get_label(command_group, group),
		    get_label(table, opcode), code);
	}
}

void
iwx_dump_cmd(uint32_t id, void *data, uint16_t len, const char *str, int type)
{
	int dump = dump_mask & type;
	uint8_t opcode = iwx_cmd_opcode(id);
	uint8_t group = iwx_cmd_groupid(id);

	for (int i = 0; i < nitems(dump_codes); i++)
		if (dump_codes[i][0] == group && dump_codes[i][1] == opcode)
			dump = 1;

	if (dump)
		hexdump(data, len, str, 0);
}

void 
iwx_bbl_add_entry(uint32_t code, int type, int ticks)
{
	/* 
	 * Compress together repeated notifications, but increment the sequence
	 * number so we can track things processing.
	 */
	if (bbl_compress && (iwx_bb_log[bbl_idx].code == code &&
	    iwx_bb_log[bbl_idx].type == type)) {
		iwx_bb_log[bbl_idx].count++;
		iwx_bb_log[bbl_idx].seq = bbl_seq++;
		iwx_bb_log[bbl_idx].ticks = ticks;
		return;
	}

	if (bbl_idx++ > IWX_BBL_ENTRIES) {
#if 0
		printf("iwx bbl roll over: type %d (%lu)\n", type, code);
#endif
		bbl_idx = 0;	
	}	
	iwx_bb_log[bbl_idx].code = code;
	iwx_bb_log[bbl_idx].type = type;
	iwx_bb_log[bbl_idx].seq = bbl_seq++;
	iwx_bb_log[bbl_idx].ticks = ticks;
	iwx_bb_log[bbl_idx].count = 1;
}

static void
iwx_bbl_print_entry(struct iwx_bbl_entry *e)
{
	uint8_t opcode = iwx_cmd_opcode(e->code);
	uint8_t group = iwx_cmd_groupid(e->code);

	switch(e->type) {
	case IWX_BBL_PKT_TX:
		printf("pkt     ");
		printf("seq %08d\t pkt len %u",
			e->seq, e->code);
		break;
		printf("pkt dup ");
		printf("seq %08d\t dup count %u",
			e->seq, e->code);
		break;
	case IWX_BBL_CMD_TX:
		printf("tx ->   ");
		printf("seq %08d\tcode 0x%08x (%s:%s)",
			e->seq, e->code, get_label(command_group, group),
			get_label(get_table(group), opcode));
		break;
	case IWX_BBL_CMD_RX:
		printf("rx      ");
		printf("seq %08d\tcode 0x%08x (%s:%s)",
			e->seq, e->code, get_label(command_group, group),
			get_label(get_table(group), opcode));
		break;
	}
	if (e->count > 1)
		printf(" (count %d)", e->count);
	printf("\n");
}

void
iwx_bbl_print_log(void)
{
	int start = -1;

	start = bbl_idx+1;
	if (start > IWX_BBL_ENTRIES-1)
		start = 0;

	for (int i = start; i < IWX_BBL_ENTRIES; i++) {
		struct iwx_bbl_entry *e = &iwx_bb_log[i];
		printf("bbl entry %05d %05d: ", i, e->ticks);
		iwx_bbl_print_entry(e);
	}
	for (int i = 0; i < start; i++) {
		struct iwx_bbl_entry *e = &iwx_bb_log[i];
		printf("bbl entry %05d %05d: ", i, e->ticks);
		iwx_bbl_print_entry(e);
	}
	printf("iwx bblog index %d seq %d\n", bbl_idx, bbl_seq);
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
