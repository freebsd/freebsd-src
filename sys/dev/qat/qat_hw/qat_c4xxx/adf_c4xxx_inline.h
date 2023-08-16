/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#ifndef ADF_C4XXX_INLINE_H_
#define ADF_C4XXX_INLINE_H_

/* Inline register addresses in SRAM BAR */
#define ARAM_CSR_BAR_OFFSET 0x100000
#define ADF_C4XXX_REG_SA_CTRL_LOCK (ARAM_CSR_BAR_OFFSET + 0x00)
#define ADF_C4XXX_REG_SA_SCRATCH_0 (ARAM_CSR_BAR_OFFSET + 0x04)
#define ADF_C4XXX_REG_SA_SCRATCH_2 (ARAM_CSR_BAR_OFFSET + 0x0C)
#define ADF_C4XXX_REG_SA_ENTRY_CTRL (ARAM_CSR_BAR_OFFSET + 0x18)
#define ADF_C4XXX_REG_SA_DB_CTRL (ARAM_CSR_BAR_OFFSET + 0x1C)
#define ADF_C4XXX_REG_SA_REMAP (ARAM_CSR_BAR_OFFSET + 0x20)
#define ADF_C4XXX_REG_SA_INLINE_CAPABILITY (ARAM_CSR_BAR_OFFSET + 0x24)
#define ADF_C4XXX_REG_SA_INLINE_ENABLE (ARAM_CSR_BAR_OFFSET + 0x28)
#define ADF_C4XXX_REG_SA_LINK_UP (ARAM_CSR_BAR_OFFSET + 0x2C)
#define ADF_C4XXX_REG_SA_FUNC_LIMITS (ARAM_CSR_BAR_OFFSET + 0x38)

#define ADF_C4XXX_SADB_SIZE_BIT BIT(24)
#define ADF_C4XXX_SADB_SIZE_IN_WORDS(accel_dev)                                \
	((accel_dev)->aram_info->sadb_region_size / 32)
#define ADF_C4XXX_DEFAULT_MAX_CHAIN_LEN 0
#define ADF_C4XXX_DEFAULT_LIMIT_CHAIN_LEN 0
/* SADB CTRL register bit offsets */
#define ADF_C4XXX_SADB_BIT_OFFSET 6
#define ADF_C4XXX_MAX_CHAIN_LEN_BIT_OFFS 1

#define ADF_C4XXX_SADB_REG_VALUE(accel_dev)                                    \
	((ADF_C4XXX_SADB_SIZE_IN_WORDS(accel_dev)                              \
	  << ADF_C4XXX_SADB_BIT_OFFSET) |                                      \
	 (ADF_C4XXX_DEFAULT_MAX_CHAIN_LEN                                      \
	  << ADF_C4XXX_MAX_CHAIN_LEN_BIT_OFFS) |                               \
	 (ADF_C4XXX_DEFAULT_LIMIT_CHAIN_LEN))

#define ADF_C4XXX_INLINE_INGRESS_OFFSET 0x0
#define ADF_C4XXX_INLINE_EGRESS_OFFSET 0x1000

/* MAC_CFG register access related definitions */
#define ADF_C4XXX_STATS_REQUEST_ENABLED BIT(16)
#define ADF_C4XXX_STATS_REQUEST_DISABLED ~BIT(16)
#define ADF_C4XXX_UNLOCK true
#define ADF_C4XXX_LOCK false

/* MAC IP register access related definitions */
#define ADF_C4XXX_MAC_STATS_READY BIT(0)
#define ADF_C4XXX_MAX_NUM_STAT_READY_READS 10
#define ADF_C4XXX_MAC_STATS_POLLING_INTERVAL 100
#define ADF_C4XXX_MAC_ERROR_TX_UNDERRUN BIT(6)
#define ADF_C4XXX_MAC_ERROR_TX_FCS BIT(7)
#define ADF_C4XXX_MAC_ERROR_TX_DATA_CORRUPT BIT(8)
#define ADF_C4XXX_MAC_ERROR_RX_OVERRUN BIT(9)
#define ADF_C4XXX_MAC_ERROR_RX_RUNT BIT(10)
#define ADF_C4XXX_MAC_ERROR_RX_UNDERSIZE BIT(11)
#define ADF_C4XXX_MAC_ERROR_RX_JABBER BIT(12)
#define ADF_C4XXX_MAC_ERROR_RX_OVERSIZE BIT(13)
#define ADF_C4XXX_MAC_ERROR_RX_FCS BIT(14)
#define ADF_C4XXX_MAC_ERROR_RX_FRAME BIT(15)
#define ADF_C4XXX_MAC_ERROR_RX_CODE BIT(16)
#define ADF_C4XXX_MAC_ERROR_RX_PREAMBLE BIT(17)
#define ADF_C4XXX_MAC_RX_LINK_UP BIT(21)
#define ADF_C4XXX_MAC_INVALID_SPEED BIT(31)
#define ADF_C4XXX_MAC_PIA_RX_FIFO_OVERRUN (1ULL << 32)
#define ADF_C4XXX_MAC_PIA_TX_FIFO_OVERRUN (1ULL << 33)
#define ADF_C4XXX_MAC_PIA_TX_FIFO_UNDERRUN (1ULL << 34)

/* 64-bit inline control registers. It will require
 * adding ADF_C4XXX_INLINE_INGRESS_OFFSET to the address for ingress
 * direction or ADF_C4XXX_INLINE_EGRESS_OFFSET to the address for
 * egress direction
 */
#define ADF_C4XXX_MAC_IP 0x8
#define ADF_C4XXX_MAC_CFG 0x18
#define ADF_C4XXX_MAC_PIA_CFG 0xA0

/* Default MAC_CFG value
 * - MAC_LINKUP_ENABLE = 1
 * - MAX_FRAME_LENGTH = 0x2600
 */
#define ADF_C4XXX_MAC_CFG_VALUE 0x00000000FA0C2600

/* Bit definitions for MAC_PIA_CFG register */
#define ADF_C4XXX_ONPI_ENABLE BIT(0)
#define ADF_C4XXX_XOFF_ENABLE BIT(10)

/* New default value for MAC_PIA_CFG register */
#define ADF_C4XXX_MAC_PIA_CFG_VALUE                                            \
	(ADF_C4XXX_XOFF_ENABLE | ADF_C4XXX_ONPI_ENABLE)

/* 64-bit Inline statistics registers. It will require
 * adding ADF_C4XXX_INLINE_INGRESS_OFFSET to the address for ingress
 * direction or ADF_C4XXX_INLINE_EGRESS_OFFSET to the address for
 * egress direction
 */
#define ADF_C4XXX_MAC_STAT_TX_OCTET 0x100
#define ADF_C4XXX_MAC_STAT_TX_FRAME 0x110
#define ADF_C4XXX_MAC_STAT_TX_BAD_FRAME 0x118
#define ADF_C4XXX_MAC_STAT_TX_FCS_ERROR 0x120
#define ADF_C4XXX_MAC_STAT_TX_64 0x130
#define ADF_C4XXX_MAC_STAT_TX_65 0x138
#define ADF_C4XXX_MAC_STAT_TX_128 0x140
#define ADF_C4XXX_MAC_STAT_TX_256 0x148
#define ADF_C4XXX_MAC_STAT_TX_512 0x150
#define ADF_C4XXX_MAC_STAT_TX_1024 0x158
#define ADF_C4XXX_MAC_STAT_TX_1519 0x160
#define ADF_C4XXX_MAC_STAT_TX_JABBER 0x168
#define ADF_C4XXX_MAC_STAT_RX_OCTET 0x200
#define ADF_C4XXX_MAC_STAT_RX_FRAME 0x210
#define ADF_C4XXX_MAC_STAT_RX_BAD_FRAME 0x218
#define ADF_C4XXX_MAC_STAT_RX_FCS_ERROR 0x220
#define ADF_C4XXX_MAC_STAT_RX_64 0x250
#define ADF_C4XXX_MAC_STAT_RX_65 0x258
#define ADF_C4XXX_MAC_STAT_RX_128 0x260
#define ADF_C4XXX_MAC_STAT_RX_256 0x268
#define ADF_C4XXX_MAC_STAT_RX_512 0x270
#define ADF_C4XXX_MAC_STAT_RX_1024 0x278
#define ADF_C4XXX_MAC_STAT_RX_1519 0x280
#define ADF_C4XXX_MAC_STAT_RX_OVERSIZE 0x288
#define ADF_C4XXX_MAC_STAT_RX_JABBER 0x290

/* 32-bit Inline statistics registers. It will require
 * adding ADF_C4XXX_INLINE_INGRESS_OFFSET to the address for ingress
 * direction or ADF_C4XXX_INLINE_EGRESS_OFFSET to the address for
 * egress direction
 */
#define ADF_C4XXX_IC_PAR_IPSEC_DESC_COUNT 0xBC0
#define ADF_C4XXX_IC_PAR_MIXED_DESC_COUNT 0xBC4
#define ADF_C4XXX_IC_PAR_FULLY_CLEAR_DESC_COUNT 0xBC8
#define ADF_C4XXX_IC_PAR_CLR_COUNT 0xBCC
#define ADF_C4XXX_IC_CTPB_PKT_COUNT 0xDF4
#define ADF_C4XXX_RB_DATA_COUNT 0xDF8
#define ADF_C4XXX_IC_CLEAR_DESC_COUNT 0xDFC
#define ADF_C4XXX_IC_IPSEC_DESC_COUNT 0xE00

/* REG_CMD_DIS_MISC bit definitions */
#define ADF_C4XXX_BYTE_SWAP_ENABLE BIT(0)
#define ADF_C4XXX_REG_CMD_DIS_MISC_DEFAULT_VALUE (ADF_C4XXX_BYTE_SWAP_ENABLE)

/* Command Dispatch Misc Register */
#define ADF_C4XXX_INGRESS_CMD_DIS_MISC (ADF_C4XXX_INLINE_INGRESS_OFFSET + 0x8A8)

#define ADF_C4XXX_EGRESS_CMD_DIS_MISC (ADF_C4XXX_INLINE_EGRESS_OFFSET + 0x8A8)

/* Congestion management threshold registers */
#define ADF_C4XXX_NEXT_FCTHRESH_OFFSET 4

/* Number of congestion management domains */
#define ADF_C4XXX_NUM_CONGEST_DOMAINS 8

#define ADF_C4XXX_BB_FCHTHRESH_OFFSET 0xB78

/* IC_BB_FCHTHRESH registers */
#define ADF_C4XXX_ICI_BB_FCHTHRESH_OFFSET                                      \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + ADF_C4XXX_BB_FCHTHRESH_OFFSET)

#define ADF_C4XXX_ICE_BB_FCHTHRESH_OFFSET                                      \
	(ADF_C4XXX_INLINE_EGRESS_OFFSET + ADF_C4XXX_BB_FCHTHRESH_OFFSET)

#define ADF_C4XXX_WR_ICI_BB_FCHTHRESH(csr_base_addr, index, value)             \
	ADF_CSR_WR(csr_base_addr,                                              \
		   (ADF_C4XXX_ICI_BB_FCHTHRESH_OFFSET +                        \
		    (index)*ADF_C4XXX_NEXT_FCTHRESH_OFFSET),                   \
		   value)

#define ADF_C4XXX_WR_ICE_BB_FCHTHRESH(csr_base_addr, index, value)             \
	ADF_CSR_WR(csr_base_addr,                                              \
		   (ADF_C4XXX_ICE_BB_FCHTHRESH_OFFSET +                        \
		    (index)*ADF_C4XXX_NEXT_FCTHRESH_OFFSET),                   \
		   value)

#define ADF_C4XXX_BB_FCLTHRESH_OFFSET 0xB98

/* IC_BB_FCLTHRESH registers */
#define ADF_C4XXX_ICI_BB_FCLTHRESH_OFFSET                                      \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + ADF_C4XXX_BB_FCLTHRESH_OFFSET)

#define ADF_C4XXX_ICE_BB_FCLTHRESH_OFFSET                                      \
	(ADF_C4XXX_INLINE_EGRESS_OFFSET + ADF_C4XXX_BB_FCLTHRESH_OFFSET)

#define ADF_C4XXX_WR_ICI_BB_FCLTHRESH(csr_base_addr, index, value)             \
	ADF_CSR_WR(csr_base_addr,                                              \
		   (ADF_C4XXX_ICI_BB_FCLTHRESH_OFFSET +                        \
		    (index)*ADF_C4XXX_NEXT_FCTHRESH_OFFSET),                   \
		   value)

#define ADF_C4XXX_WR_ICE_BB_FCLTHRESH(csr_base_addr, index, value)             \
	ADF_CSR_WR(csr_base_addr,                                              \
		   (ADF_C4XXX_ICE_BB_FCLTHRESH_OFFSET +                        \
		    (index)*ADF_C4XXX_NEXT_FCTHRESH_OFFSET),                   \
		   value)

#define ADF_C4XXX_BB_BEHTHRESH_OFFSET 0xBB8
#define ADF_C4XXX_BB_BELTHRESH_OFFSET 0xBBC
#define ADF_C4XXX_BEWIP_THRESH_OFFSET 0xDEC
#define ADF_C4XXX_CTPB_THRESH_OFFSET 0xDE8
#define ADF_C4XXX_CIRQ_OFFSET 0xDE4
#define ADF_C4XXX_Q2MEMAP_OFFSET 0xC04

/* IC_BB_BEHTHRESH register */
#define ADF_C4XXX_ICI_BB_BEHTHRESH_OFFSET                                      \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + ADF_C4XXX_BB_BEHTHRESH_OFFSET)

#define ADF_C4XXX_ICE_BB_BEHTHRESH_OFFSET                                      \
	(ADF_C4XXX_INLINE_EGRESS_OFFSET + ADF_C4XXX_BB_BEHTHRESH_OFFSET)

/* IC_BB_BELTHRESH register */
#define ADF_C4XXX_ICI_BB_BELTHRESH_OFFSET                                      \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + ADF_C4XXX_BB_BELTHRESH_OFFSET)

#define ADF_C4XXX_ICE_BB_BELTHRESH_OFFSET                                      \
	(ADF_C4XXX_INLINE_EGRESS_OFFSET + ADF_C4XXX_BB_BELTHRESH_OFFSET)

/* IC_BEWIP_THRESH register */
#define ADF_C4XXX_ICI_BEWIP_THRESH_OFFSET                                      \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + ADF_C4XXX_BEWIP_THRESH_OFFSET)

#define ADF_C4XXX_ICE_BEWIP_THRESH_OFFSET                                      \
	(ADF_C4XXX_INLINE_EGRESS_OFFSET + ADF_C4XXX_BEWIP_THRESH_OFFSET)

/* IC_CTPB_THRESH register */
#define ADF_C4XXX_ICI_CTPB_THRESH_OFFSET                                       \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + ADF_C4XXX_CTPB_THRESH_OFFSET)

#define ADF_C4XXX_ICE_CTPB_THRESH_OFFSET                                       \
	(ADF_C4XXX_INLINE_EGRESS_OFFSET + ADF_C4XXX_CTPB_THRESH_OFFSET)

/* ADF_C4XXX_ICI_CIRQ_OFFSET */
#define ADF_C4XXX_ICI_CIRQ_OFFSET                                              \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + ADF_C4XXX_CIRQ_OFFSET)

#define ADF_C4XXX_ICE_CIRQ_OFFSET                                              \
	(ADF_C4XXX_INLINE_EGRESS_OFFSET + ADF_C4XXX_CIRQ_OFFSET)

/* IC_Q2MEMAP register */
#define ADF_C4XXX_ICI_Q2MEMAP_OFFSET                                           \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + ADF_C4XXX_Q2MEMAP_OFFSET)

#define ADF_C4XXX_ICE_Q2MEMAP_OFFSET                                           \
	(ADF_C4XXX_INLINE_EGRESS_OFFSET + ADF_C4XXX_Q2MEMAP_OFFSET)

#define ADF_C4XXX_NEXT_Q2MEMAP_OFFSET 4
#define ADF_C4XXX_NUM_Q2MEMAP_REGISTERS 8

#define ADF_C4XXX_WR_CSR_ICI_Q2MEMAP(csr_base_addr, index, value)              \
	ADF_CSR_WR(csr_base_addr,                                              \
		   (ADF_C4XXX_ICI_Q2MEMAP_OFFSET +                             \
		    (index)*ADF_C4XXX_NEXT_Q2MEMAP_OFFSET),                    \
		   value)

#define ADF_C4XXX_WR_CSR_ICE_Q2MEMAP(csr_base_addr, index, value)              \
	ADF_CSR_WR(csr_base_addr,                                              \
		   (ADF_C4XXX_ICE_Q2MEMAP_OFFSET +                             \
		    (index)*ADF_C4XXX_NEXT_Q2MEMAP_OFFSET),                    \
		   value)

/* IC_PARSE_CTRL register */
#define ADF_C4XXX_DEFAULT_KEY_LENGTH 21
#define ADF_C4XXX_DEFAULT_REL_ABS_OFFSET 1
#define ADF_C4XXX_DEFAULT_NUM_TUPLES 4
#define ADF_C4XXX_IC_PARSE_CTRL_OFFSET_DEFAULT_VALUE                           \
	((ADF_C4XXX_DEFAULT_KEY_LENGTH << 4) |                                 \
	 (ADF_C4XXX_DEFAULT_REL_ABS_OFFSET << 3) |                             \
	 (ADF_C4XXX_DEFAULT_NUM_TUPLES))

/* Configuration parsing register definitions */
#define ADF_C4XXX_IC_PARSE_CTRL_OFFSET (ADF_C4XXX_INLINE_INGRESS_OFFSET + 0xB00)

/* Fixed data parsing register */
#define ADF_C4XXX_IC_PARSE_FIXED_DATA(i)                                       \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + 0xB04 + ((i)*4))
#define ADF_C4XXX_DEFAULT_IC_PARSE_FIXED_DATA_0 0x32

/* Fixed length parsing register */
#define ADF_C4XXX_IC_PARSE_FIXED_LENGTH                                        \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + 0xB14)
#define ADF_C4XXX_DEFAULT_IC_PARSE_FIXED_LEN 0x0

/* IC_PARSE_IPV4 offset and length registers */
#define ADF_C4XXX_IC_PARSE_IPV4_OFFSET_0                                       \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + 0xB18)
#define ADF_C4XXX_IC_PARSE_IPV4_OFFSET_1                                       \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + 0xB1C)
#define ADF_C4XXX_IC_PARSE_IPV4_OFFSET_2                                       \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + 0xB20)
#define ADF_C4XXX_IC_PARSE_IPV4_OFFSET_3                                       \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + 0xB24)
#define ADF_C4XXX_IC_PARSE_IPV4_OFFSET_4                                       \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + 0xB28)
#define ADF_C4XXX_IC_PARSE_IPV4_OFFSET_5                                       \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + 0xB2C)

#define ADF_C4XXX_IC_PARSE_IPV4_LENGTH_0                                       \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + 0xB30)
#define ADF_C4XXX_IC_PARSE_IPV4_LENGTH_1                                       \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + 0xB34)
#define ADF_C4XXX_IC_PARSE_IPV4_LENGTH_2                                       \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + 0xB38)
#define ADF_C4XXX_IC_PARSE_IPV4_LENGTH_3                                       \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + 0xB3C)
#define ADF_C4XXX_IC_PARSE_IPV4_LENGTH_4                                       \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + 0xB40)
#define ADF_C4XXX_IC_PARSE_IPV4_LENGTH_5                                       \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + 0xB44)

#define ADF_C4XXX_IPV4_OFFSET_0_PARSER_BASE 0x1
#define ADF_C4XXX_IPV4_OFFSET_0_OFFSET 0x0
#define ADF_C4XXX_DEFAULT_IC_PARSE_IPV4_OFFS_0_VALUE                           \
	((ADF_C4XXX_IPV4_OFFSET_0_PARSER_BASE << 29) |                         \
	 ADF_C4XXX_IPV4_OFFSET_0_OFFSET)
#define ADF_C4XXX_DEFAULT_IC_PARSE_IPV4_LEN_0_VALUE 0

#define ADF_C4XXX_IPV4_OFFSET_1_PARSER_BASE 0x2
#define ADF_C4XXX_IPV4_OFFSET_1_OFFSET 0x0
#define ADF_C4XXX_DEFAULT_IC_PARSE_IPV4_OFFS_1_VALUE                           \
	((ADF_C4XXX_IPV4_OFFSET_1_PARSER_BASE << 29) |                         \
	 ADF_C4XXX_IPV4_OFFSET_1_OFFSET)
#define ADF_C4XXX_DEFAULT_IC_PARSE_IPV4_LEN_1_VALUE 3

#define ADF_C4XXX_IPV4_OFFSET_2_PARSER_BASE 0x4
#define ADF_C4XXX_IPV4_OFFSET_2_OFFSET 0x10
#define ADF_C4XXX_DEFAULT_IC_PARSE_IPV4_OFFS_2_VALUE                           \
	((ADF_C4XXX_IPV4_OFFSET_2_PARSER_BASE << 29) |                         \
	 ADF_C4XXX_IPV4_OFFSET_2_OFFSET)
#define ADF_C4XXX_DEFAULT_IC_PARSE_IPV4_LEN_2_VALUE 3

#define ADF_C4XXX_IPV4_OFFSET_3_PARSER_BASE 0x0
#define ADF_C4XXX_IPV4_OFFSET_3_OFFSET 0x0
#define ADF_C4XXX_DEFAULT_IC_PARSE_IPV4_OFFS_3_VALUE                           \
	((ADF_C4XXX_IPV4_OFFSET_3_PARSER_BASE << 29) |                         \
	 ADF_C4XXX_IPV4_OFFSET_3_OFFSET)
#define ADF_C4XXX_DEFAULT_IC_PARSE_IPV4_LEN_3_VALUE 0

/* IC_PARSE_IPV6 offset and length registers */
#define ADF_C4XXX_IC_PARSE_IPV6_OFFSET_0                                       \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + 0xB48)
#define ADF_C4XXX_IC_PARSE_IPV6_OFFSET_1                                       \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + 0xB4C)
#define ADF_C4XXX_IC_PARSE_IPV6_OFFSET_2                                       \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + 0xB50)
#define ADF_C4XXX_IC_PARSE_IPV6_OFFSET_3                                       \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + 0xB54)
#define ADF_C4XXX_IC_PARSE_IPV6_OFFSET_4                                       \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + 0xB58)
#define ADF_C4XXX_IC_PARSE_IPV6_OFFSET_5                                       \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + 0xB5C)

#define ADF_C4XXX_IC_PARSE_IPV6_LENGTH_0                                       \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + 0xB60)
#define ADF_C4XXX_IC_PARSE_IPV6_LENGTH_1                                       \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + 0xB64)
#define ADF_C4XXX_IC_PARSE_IPV6_LENGTH_2                                       \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + 0xB68)
#define ADF_C4XXX_IC_PARSE_IPV6_LENGTH_3                                       \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + 0xB6C)
#define ADF_C4XXX_IC_PARSE_IPV6_LENGTH_4                                       \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + 0xB70)
#define ADF_C4XXX_IC_PARSE_IPV6_LENGTH_5                                       \
	(ADF_C4XXX_INLINE_INGRESS_OFFSET + 0xB74)

#define ADF_C4XXX_IPV6_OFFSET_0_PARSER_BASE 0x1
#define ADF_C4XXX_IPV6_OFFSET_0_OFFSET 0x0
#define ADF_C4XXX_DEFAULT_IC_PARSE_IPV6_OFFS_0_VALUE                           \
	((ADF_C4XXX_IPV6_OFFSET_0_PARSER_BASE << 29) |                         \
	 (ADF_C4XXX_IPV6_OFFSET_0_OFFSET))
#define ADF_C4XXX_DEFAULT_IC_PARSE_IPV6_LEN_0_VALUE 0

#define ADF_C4XXX_IPV6_OFFSET_1_PARSER_BASE 0x2
#define ADF_C4XXX_IPV6_OFFSET_1_OFFSET 0x0
#define ADF_C4XXX_DEFAULT_IC_PARSE_IPV6_OFFS_1_VALUE                           \
	((ADF_C4XXX_IPV6_OFFSET_1_PARSER_BASE << 29) |                         \
	 (ADF_C4XXX_IPV6_OFFSET_1_OFFSET))
#define ADF_C4XXX_DEFAULT_IC_PARSE_IPV6_LEN_1_VALUE 3

#define ADF_C4XXX_IPV6_OFFSET_2_PARSER_BASE 0x4
#define ADF_C4XXX_IPV6_OFFSET_2_OFFSET 0x18
#define ADF_C4XXX_DEFAULT_IC_PARSE_IPV6_OFFS_2_VALUE                           \
	((ADF_C4XXX_IPV6_OFFSET_2_PARSER_BASE << 29) |                         \
	 (ADF_C4XXX_IPV6_OFFSET_2_OFFSET))
#define ADF_C4XXX_DEFAULT_IC_PARSE_IPV6_LEN_2_VALUE 0xF

#define ADF_C4XXX_IPV6_OFFSET_3_PARSER_BASE 0x0
#define ADF_C4XXX_IPV6_OFFSET_3_OFFSET 0x0
#define ADF_C4XXX_DEFAULT_IC_PARSE_IPV6_OFFS_3_VALUE                           \
	((ADF_C4XXX_IPV6_OFFSET_3_PARSER_BASE << 29) |                         \
	 (ADF_C4XXX_IPV6_OFFSET_3_OFFSET))
#define ADF_C4XXX_DEFAULT_IC_PARSE_IPV6_LEN_3_VALUE 0x0

/* error notification configuration registers */

#define ADF_C4XXX_IC_CD_RF_PARITY_ERR_0 0xA00
#define ADF_C4XXX_IC_CD_RF_PARITY_ERR_1 0xA04
#define ADF_C4XXX_IC_CD_RF_PARITY_ERR_2 0xA08
#define ADF_C4XXX_IC_CD_RF_PARITY_ERR_3 0xA0C
#define ADF_C4XXX_IC_CD_CERR 0xA10
#define ADF_C4XXX_IC_CD_UERR 0xA14

#define ADF_C4XXX_IC_INLN_RF_PARITY_ERR_0 0xF00
#define ADF_C4XXX_IC_INLN_RF_PARITY_ERR_1 0xF04
#define ADF_C4XXX_IC_INLN_RF_PARITY_ERR_2 0xF08
#define ADF_C4XXX_IC_INLN_RF_PARITY_ERR_3 0xF0C
#define ADF_C4XXX_IC_INLN_RF_PARITY_ERR_4 0xF10
#define ADF_C4XXX_IC_INLN_RF_PARITY_ERR_5 0xF14
#define ADF_C4XXX_IC_PARSER_CERR 0xF18
#define ADF_C4XXX_IC_PARSER_UERR 0xF1C
#define ADF_C4XXX_IC_CTPB_CERR 0xF28
#define ADF_C4XXX_IC_CTPB_UERR 0xF2C
#define ADF_C4XXX_IC_CPPM_ERR_STAT 0xF3C
#define ADF_C4XXX_IC_CONGESTION_MGMT_INT 0xF58

#define ADF_C4XXX_IC_CPPT_ERR_STAT 0x704
#define ADF_C4XXX_IC_MAC_IM 0x10

#define ADF_C4XXX_CD_RF_PARITY_ERR_0_VAL 0x22222222
#define ADF_C4XXX_CD_RF_PARITY_ERR_1_VAL 0x22222323
#define ADF_C4XXX_CD_RF_PARITY_ERR_2_VAL 0x00022222
#define ADF_C4XXX_CD_RF_PARITY_ERR_3_VAL 0x00000000
#define ADF_C4XXX_CD_UERR_VAL 0x00000008
#define ADF_C4XXX_CD_CERR_VAL 0x00000008
#define ADF_C4XXX_PARSER_UERR_VAL 0x00100008
#define ADF_C4XXX_PARSER_CERR_VAL 0x00000008
#define ADF_C4XXX_INLN_RF_PARITY_ERR_0_VAL 0x33333333
#define ADF_C4XXX_INLN_RF_PARITY_ERR_1_VAL 0x33333333
#define ADF_C4XXX_INLN_RF_PARITY_ERR_2_VAL 0x33333333
#define ADF_C4XXX_INLN_RF_PARITY_ERR_3_VAL 0x22222222
#define ADF_C4XXX_INLN_RF_PARITY_ERR_4_VAL 0x22222222
#define ADF_C4XXX_INLN_RF_PARITY_ERR_5_VAL 0x00333232
#define ADF_C4XXX_CTPB_UERR_VAL 0x00000008
#define ADF_C4XXX_CTPB_CERR_VAL 0x00000008
#define ADF_C4XXX_CPPM_ERR_STAT_VAL 0x00007000
#define ADF_C4XXX_CPPT_ERR_STAT_VAL 0x000001C0
#define ADF_C4XXX_CONGESTION_MGMT_INI_VAL 0x00000001
#define ADF_C4XXX_MAC_IM_VAL 0x000000087FDC003E

/* parser ram ecc uerr */
#define ADF_C4XXX_PARSER_UERR_INTR BIT(0)
/* multiple err */
#define ADF_C4XXX_PARSER_MUL_UERR_INTR BIT(18)
#define ADF_C4XXX_PARSER_DESC_UERR_INTR_ENA BIT(20)

#define ADF_C4XXX_RF_PAR_ERR_BITS 32
#define ADF_C4XXX_MAX_STR_LEN 64
#define RF_PAR_MUL_MAP(bit_num) (((bit_num)-2) / 4)
#define RF_PAR_MAP(bit_num) (((bit_num)-3) / 4)

/* cd rf parity error
 * BIT(2) rf parity mul 0
 * BIT(3) rf parity 0
 * BIT(10) rf parity mul 2
 * BIT(11) rf parity 2
 */
#define ADF_C4XXX_CD_RF_PAR_ERR_1_INTR (BIT(2) | BIT(3) | BIT(10) | BIT(11))

/* inln rf parity error
 * BIT(2) rf parity mul 0
 * BIT(3) rf parity 0
 * BIT(6) rf parity mul 1
 * BIT(7) rf parity 1
 * BIT(10) rf parity mul 2
 * BIT(11) rf parity 2
 * BIT(14) rf parity mul 3
 * BIT(15) rf parity 3
 * BIT(18) rf parity mul 4
 * BIT(19) rf parity 4
 * BIT(22) rf parity mul 5
 * BIT(23) rf parity 5
 * BIT(26) rf parity mul 6
 * BIT(27) rf parity 6
 * BIT(30) rf parity mul 7
 * BIT(31) rf parity 7
 */
#define ADF_C4XXX_INLN_RF_PAR_ERR_0_INTR                                       \
	(BIT(2) | BIT(3) | BIT(6) | BIT(7) | BIT(10) | BIT(11) | BIT(14) |     \
	 BIT(15) | BIT(18) | BIT(19) | BIT(22) | BIT(23) | BIT(26) | BIT(27) | \
	 BIT(30) | BIT(31))
#define ADF_C4XXX_INLN_RF_PAR_ERR_1_INTR ADF_C4XXX_INLN_RF_PAR_ERR_0_INTR
#define ADF_C4XXX_INLN_RF_PAR_ERR_2_INTR ADF_C4XXX_INLN_RF_PAR_ERR_0_INTR
#define ADF_C4XXX_INLN_RF_PAR_ERR_5_INTR                                       \
	(BIT(6) | BIT(7) | BIT(14) | BIT(15) | BIT(18) | BIT(19) | BIT(22) |   \
	 BIT(23))

/* Congestion mgmt events */
#define ADF_C4XXX_CONGESTION_MGMT_CTPB_GLOBAL_CROSSED BIT(1)
#define ADF_C4XXX_CONGESTION_MGMT_XOFF_CIRQ_OUT BIT(2)
#define ADF_C4XXX_CONGESTION_MGMT_XOFF_CIRQ_IN BIT(3)

/* AEAD algorithm definitions in REG_SA_SCRATCH[0] register.
 * Bits<6:5> are reserved for expansion.
 */
#define AES128_GCM BIT(0)
#define AES192_GCM BIT(1)
#define AES256_GCM BIT(2)
#define AES128_CCM BIT(3)
#define CHACHA20_POLY1305 BIT(4)
/* Cipher algorithm definitions in REG_SA_SCRATCH[0] register
 * Bit<15> is reserved for expansion.
 */
#define CIPHER_NULL BIT(7)
#define AES128_CBC BIT(8)
#define AES192_CBC BIT(9)
#define AES256_CBC BIT(10)
#define AES128_CTR BIT(11)
#define AES192_CTR BIT(12)
#define AES256_CTR BIT(13)
#define _3DES_CBC BIT(14)
/* Authentication algorithm definitions in REG_SA_SCRATCH[0] register
 * Bits<25:30> are reserved for expansion.
 */
#define HMAC_MD5_96 BIT(16)
#define HMAC_SHA1_96 BIT(17)
#define HMAC_SHA256_128 BIT(18)
#define HMAC_SHA384_192 BIT(19)
#define HMAC_SHA512_256 BIT(20)
#define AES_GMAC_AES_128 BIT(21)
#define AES_XCBC_MAC_96 BIT(22)
#define AES_CMAC_96 BIT(23)
#define AUTH_NULL BIT(24)

/* Algo group0:DEFAULT */
#define ADF_C4XXX_DEFAULT_SUPPORTED_ALGORITHMS                                 \
	(AES128_GCM |                                                          \
	 (AES192_GCM | AES256_GCM | AES128_CCM | CHACHA20_POLY1305) |          \
	 (CIPHER_NULL | AES128_CBC | AES192_CBC | AES256_CBC) |                \
	 (AES128_CTR | AES192_CTR | AES256_CTR | _3DES_CBC) |                  \
	 (HMAC_MD5_96 | HMAC_SHA1_96 | HMAC_SHA256_128) |                      \
	 (HMAC_SHA384_192 | HMAC_SHA512_256 | AES_GMAC_AES_128) |              \
	 (AES_XCBC_MAC_96 | AES_CMAC_96 | AUTH_NULL))

/* Algo group1 */
#define ADF_C4XXX_SUPPORTED_ALGORITHMS_GROUP1                                  \
	(AES128_GCM | (AES256_GCM | CHACHA20_POLY1305))

/* Supported crypto offload features in REG_SA_SCRATCH[2] register */
#define ADF_C4XXX_IPSEC_ESP BIT(0)
#define ADF_C4XXX_IPSEC_AH BIT(1)
#define ADF_C4XXX_UDP_ENCAPSULATION BIT(2)
#define ADF_C4XXX_IPSEC_TUNNEL_MODE BIT(3)
#define ADF_C4XXX_IPSEC_TRANSPORT_MODE BIT(4)
#define ADF_C4XXX_IPSEC_EXT_SEQ_NUM BIT(5)

#define ADF_C4XXX_DEFAULT_CY_OFFLOAD_FEATURES                                  \
	(ADF_C4XXX_IPSEC_ESP |                                                 \
	 (ADF_C4XXX_UDP_ENCAPSULATION | ADF_C4XXX_IPSEC_TUNNEL_MODE) |         \
	 (ADF_C4XXX_IPSEC_TRANSPORT_MODE | ADF_C4XXX_IPSEC_EXT_SEQ_NUM))

/* REG_SA_CTRL_LOCK default value */
#define ADF_C4XXX_DEFAULT_SA_CTRL_LOCKOUT BIT(0)

/* SA ENTRY CTRL default values */
#define ADF_C4XXX_DEFAULT_LU_KEY_LEN 21

/* Sa size for algo group0 */
#define ADF_C4XXX_DEFAULT_SA_SIZE 6

/* Sa size for algo group1 */
#define ADF_C4XXX_ALGO_GROUP1_SA_SIZE 2

/* SA size is based on 32byte granularity
 * A value of zero indicates an SA size of 32 bytes
 */
#define ADF_C4XXX_SA_SIZE_IN_BYTES(sa_size) (((sa_size) + 1) * 32)

/* SA ENTRY CTRL register bit offsets */
#define ADF_C4XXX_LU_KEY_LEN_BIT_OFFSET 5

/* REG_SA_FUNC_LIMITS default value */
#define ADF_C4XXX_FUNC_LIMIT(accel_dev, sa_size)                               \
	(ADF_C4XXX_SADB_SIZE_IN_WORDS(accel_dev) / ((sa_size) + 1))

/* REG_SA_INLINE_ENABLE bit definition */
#define ADF_C4XXX_INLINE_ENABLED BIT(0)

/* REG_SA_INLINE_CAPABILITY bit definitions */
#define ADF_C4XXX_INLINE_INGRESS_ENABLE BIT(0)
#define ADF_C4XXX_INLINE_EGRESS_ENABLE BIT(1)
#define ADF_C4XXX_INLINE_CAPABILITIES                                          \
	(ADF_C4XXX_INLINE_INGRESS_ENABLE | ADF_C4XXX_INLINE_EGRESS_ENABLE)

/* Congestion management profile information */
enum congest_mngt_profile_info {
	CIRQ_CFG_1 = 0,
	CIRQ_CFG_2,
	CIRQ_CFG_3,
	BEST_EFFORT_SINGLE_QUEUE,
	BEST_EFFORT_8_QUEUES,
};

/* IPsec Algo Group */
enum ipsec_algo_group_info {
	IPSEC_DEFAUL_ALGO_GROUP = 0,
	IPSEC_ALGO_GROUP1,
	IPSEC_ALGO_GROUP_DELIMITER
};

int get_congestion_management_profile(struct adf_accel_dev *accel_dev,
				      u8 *profile);
int c4xxx_init_congestion_management(struct adf_accel_dev *accel_dev);
int c4xxx_init_debugfs_inline_dir(struct adf_accel_dev *accel_dev);
void c4xxx_exit_debugfs_inline_dir(struct adf_accel_dev *accel_dev);
#endif /* ADF_C4XXX_INLINE_H_ */
