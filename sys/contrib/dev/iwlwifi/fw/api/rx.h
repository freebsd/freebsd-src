/* SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause */
/*
 * Copyright (C) 2012-2014, 2018-2024 Intel Corporation
 * Copyright (C) 2013-2015 Intel Mobile Communications GmbH
 * Copyright (C) 2015-2017 Intel Deutschland GmbH
 */
#ifndef __iwl_fw_api_rx_h__
#define __iwl_fw_api_rx_h__

/* API for pre-9000 hardware */

#define IWL_RX_INFO_PHY_CNT 8
#define IWL_RX_INFO_ENERGY_ANT_ABC_IDX 1
#define IWL_RX_INFO_ENERGY_ANT_A_MSK 0x000000ff
#define IWL_RX_INFO_ENERGY_ANT_B_MSK 0x0000ff00
#define IWL_RX_INFO_ENERGY_ANT_A_POS 0
#define IWL_RX_INFO_ENERGY_ANT_B_POS 8
#define IWL_RX_INFO_ENERGY_ANT_C_POS 16

enum iwl_mac_context_info {
	MAC_CONTEXT_INFO_NONE,
	MAC_CONTEXT_INFO_GSCAN,
};

/**
 * struct iwl_rx_phy_info - phy info
 * (REPLY_RX_PHY_CMD = 0xc0)
 * @non_cfg_phy_cnt: non configurable DSP phy data byte count
 * @cfg_phy_cnt: configurable DSP phy data byte count
 * @stat_id: configurable DSP phy data set ID
 * @reserved1: reserved
 * @system_timestamp: GP2  at on air rise
 * @timestamp: TSF at on air rise
 * @beacon_time_stamp: beacon at on-air rise
 * @phy_flags: general phy flags: band, modulation, ...
 * @channel: channel number
 * @non_cfg_phy: for various implementations of non_cfg_phy
 * @rate_n_flags: RATE_MCS_*
 * @byte_count: frame's byte-count
 * @frame_time: frame's time on the air, based on byte count and frame rate
 *	calculation
 * @mac_active_msk: what MACs were active when the frame was received
 * @mac_context_info: additional info on the context in which the frame was
 *	received as defined in &enum iwl_mac_context_info
 *
 * Before each Rx, the device sends this data. It contains PHY information
 * about the reception of the packet.
 */
struct iwl_rx_phy_info {
	u8 non_cfg_phy_cnt;
	u8 cfg_phy_cnt;
	u8 stat_id;
	u8 reserved1;
	__le32 system_timestamp;
	__le64 timestamp;
	__le32 beacon_time_stamp;
	__le16 phy_flags;
	__le16 channel;
	__le32 non_cfg_phy[IWL_RX_INFO_PHY_CNT];
	__le32 rate_n_flags;
	__le32 byte_count;
	u8 mac_active_msk;
	u8 mac_context_info;
	__le16 frame_time;
} __packed;

/*
 * TCP offload Rx assist info
 *
 * bits 0:3 - reserved
 * bits 4:7 - MIC CRC length
 * bits 8:12 - MAC header length
 * bit 13 - Padding indication
 * bit 14 - A-AMSDU indication
 * bit 15 - Offload enabled
 */
enum iwl_csum_rx_assist_info {
	CSUM_RXA_RESERVED_MASK	= 0x000f,
	CSUM_RXA_MICSIZE_MASK	= 0x00f0,
	CSUM_RXA_HEADERLEN_MASK	= 0x1f00,
	CSUM_RXA_PADD		= BIT(13),
	CSUM_RXA_AMSDU		= BIT(14),
	CSUM_RXA_ENA		= BIT(15)
};

/**
 * struct iwl_rx_mpdu_res_start - phy info
 * @byte_count: byte count of the frame
 * @assist: see &enum iwl_csum_rx_assist_info
 */
struct iwl_rx_mpdu_res_start {
	__le16 byte_count;
	__le16 assist;
} __packed; /* _RX_MPDU_RES_START_API_S_VER_2 */

/**
 * enum iwl_rx_phy_flags - to parse %iwl_rx_phy_info phy_flags
 * @RX_RES_PHY_FLAGS_BAND_24: true if the packet was received on 2.4 band
 * @RX_RES_PHY_FLAGS_MOD_CCK: modulation is CCK
 * @RX_RES_PHY_FLAGS_SHORT_PREAMBLE: true if packet's preamble was short
 * @RX_RES_PHY_FLAGS_NARROW_BAND: narrow band (<20 MHz) receive
 * @RX_RES_PHY_FLAGS_ANTENNA: antenna on which the packet was received
 * @RX_RES_PHY_FLAGS_ANTENNA_POS: antenna bit position
 * @RX_RES_PHY_FLAGS_AGG: set if the packet was part of an A-MPDU
 * @RX_RES_PHY_FLAGS_OFDM_HT: The frame was an HT frame
 * @RX_RES_PHY_FLAGS_OFDM_GF: The frame used GF preamble
 * @RX_RES_PHY_FLAGS_OFDM_VHT: The frame was a VHT frame
 */
enum iwl_rx_phy_flags {
	RX_RES_PHY_FLAGS_BAND_24	= BIT(0),
	RX_RES_PHY_FLAGS_MOD_CCK	= BIT(1),
	RX_RES_PHY_FLAGS_SHORT_PREAMBLE	= BIT(2),
	RX_RES_PHY_FLAGS_NARROW_BAND	= BIT(3),
	RX_RES_PHY_FLAGS_ANTENNA	= (0x7 << 4),
	RX_RES_PHY_FLAGS_ANTENNA_POS	= 4,
	RX_RES_PHY_FLAGS_AGG		= BIT(7),
	RX_RES_PHY_FLAGS_OFDM_HT	= BIT(8),
	RX_RES_PHY_FLAGS_OFDM_GF	= BIT(9),
	RX_RES_PHY_FLAGS_OFDM_VHT	= BIT(10),
};

/**
 * enum iwl_mvm_rx_status - written by fw for each Rx packet
 * @RX_MPDU_RES_STATUS_CRC_OK: CRC is fine
 * @RX_MPDU_RES_STATUS_OVERRUN_OK: there was no RXE overflow
 * @RX_MPDU_RES_STATUS_SRC_STA_FOUND: station was found
 * @RX_MPDU_RES_STATUS_KEY_VALID: key was valid
 * @RX_MPDU_RES_STATUS_ICV_OK: ICV is fine, if not, the packet is destroyed
 * @RX_MPDU_RES_STATUS_MIC_OK: used for CCM alg only. TKIP MIC is checked
 *	in the driver.
 * @RX_MPDU_RES_STATUS_TTAK_OK: TTAK is fine
 * @RX_MPDU_RES_STATUS_MNG_FRAME_REPLAY_ERR:  valid for alg = CCM_CMAC or
 *	alg = CCM only. Checks replay attack for 11w frames.
 * @RX_MPDU_RES_STATUS_SEC_NO_ENC: this frame is not encrypted
 * @RX_MPDU_RES_STATUS_SEC_WEP_ENC: this frame is encrypted using WEP
 * @RX_MPDU_RES_STATUS_SEC_CCM_ENC: this frame is encrypted using CCM
 * @RX_MPDU_RES_STATUS_SEC_TKIP_ENC: this frame is encrypted using TKIP
 * @RX_MPDU_RES_STATUS_SEC_EXT_ENC: this frame is encrypted using extension
 *	algorithm
 * @RX_MPDU_RES_STATUS_SEC_CMAC_GMAC_ENC: this frame is protected using
 *	CMAC or GMAC
 * @RX_MPDU_RES_STATUS_SEC_ENC_ERR: this frame couldn't be decrypted
 * @RX_MPDU_RES_STATUS_SEC_ENC_MSK: bitmask of the encryption algorithm
 * @RX_MPDU_RES_STATUS_DEC_DONE: this frame has been successfully decrypted
 * @RX_MPDU_RES_STATUS_CSUM_DONE: checksum was done by the hw
 * @RX_MPDU_RES_STATUS_CSUM_OK: checksum found no errors
 * @RX_MPDU_RES_STATUS_STA_ID_MSK: station ID mask
 * @RX_MDPU_RES_STATUS_STA_ID_SHIFT: station ID bit shift
 */
enum iwl_mvm_rx_status {
	RX_MPDU_RES_STATUS_CRC_OK			= BIT(0),
	RX_MPDU_RES_STATUS_OVERRUN_OK			= BIT(1),
	RX_MPDU_RES_STATUS_SRC_STA_FOUND		= BIT(2),
	RX_MPDU_RES_STATUS_KEY_VALID			= BIT(3),
	RX_MPDU_RES_STATUS_ICV_OK			= BIT(5),
	RX_MPDU_RES_STATUS_MIC_OK			= BIT(6),
	RX_MPDU_RES_STATUS_TTAK_OK			= BIT(7),
	RX_MPDU_RES_STATUS_MNG_FRAME_REPLAY_ERR		= BIT(7),
	RX_MPDU_RES_STATUS_SEC_NO_ENC			= (0 << 8),
	RX_MPDU_RES_STATUS_SEC_WEP_ENC			= (1 << 8),
	RX_MPDU_RES_STATUS_SEC_CCM_ENC			= (2 << 8),
	RX_MPDU_RES_STATUS_SEC_TKIP_ENC			= (3 << 8),
	RX_MPDU_RES_STATUS_SEC_EXT_ENC			= (4 << 8),
	RX_MPDU_RES_STATUS_SEC_CMAC_GMAC_ENC		= (6 << 8),
	RX_MPDU_RES_STATUS_SEC_ENC_ERR			= (7 << 8),
	RX_MPDU_RES_STATUS_SEC_ENC_MSK			= (7 << 8),
	RX_MPDU_RES_STATUS_DEC_DONE			= BIT(11),
	RX_MPDU_RES_STATUS_CSUM_DONE			= BIT(16),
	RX_MPDU_RES_STATUS_CSUM_OK			= BIT(17),
	RX_MDPU_RES_STATUS_STA_ID_SHIFT			= 24,
	RX_MPDU_RES_STATUS_STA_ID_MSK			= 0x1f << RX_MDPU_RES_STATUS_STA_ID_SHIFT,
};

/* 9000 series API */
enum iwl_rx_mpdu_mac_flags1 {
	IWL_RX_MDPU_MFLG1_ADDRTYPE_MASK		= 0x03,
	IWL_RX_MPDU_MFLG1_MIC_CRC_LEN_MASK	= 0xf0,
	/* shift should be 4, but the length is measured in 2-byte
	 * words, so shifting only by 3 gives a byte result
	 */
	IWL_RX_MPDU_MFLG1_MIC_CRC_LEN_SHIFT	= 3,
};

enum iwl_rx_mpdu_mac_flags2 {
	/* in 2-byte words */
	IWL_RX_MPDU_MFLG2_HDR_LEN_MASK		= 0x1f,
	IWL_RX_MPDU_MFLG2_PAD			= 0x20,
	IWL_RX_MPDU_MFLG2_AMSDU			= 0x40,
};

enum iwl_rx_mpdu_amsdu_info {
	IWL_RX_MPDU_AMSDU_SUBFRAME_IDX_MASK	= 0x7f,
	IWL_RX_MPDU_AMSDU_LAST_SUBFRAME		= 0x80,
};

#define RX_MPDU_BAND_POS 6
#define RX_MPDU_BAND_MASK 0xC0
#define BAND_IN_RX_STATUS(_val) \
	(((_val) & RX_MPDU_BAND_MASK) >> RX_MPDU_BAND_POS)

enum iwl_rx_l3_proto_values {
	IWL_RX_L3_TYPE_NONE,
	IWL_RX_L3_TYPE_IPV4,
	IWL_RX_L3_TYPE_IPV4_FRAG,
	IWL_RX_L3_TYPE_IPV6_FRAG,
	IWL_RX_L3_TYPE_IPV6,
	IWL_RX_L3_TYPE_IPV6_IN_IPV4,
	IWL_RX_L3_TYPE_ARP,
	IWL_RX_L3_TYPE_EAPOL,
};

#define IWL_RX_L3_PROTO_POS 4

enum iwl_rx_l3l4_flags {
	IWL_RX_L3L4_IP_HDR_CSUM_OK		= BIT(0),
	IWL_RX_L3L4_TCP_UDP_CSUM_OK		= BIT(1),
	IWL_RX_L3L4_TCP_FIN_SYN_RST_PSH		= BIT(2),
	IWL_RX_L3L4_TCP_ACK			= BIT(3),
	IWL_RX_L3L4_L3_PROTO_MASK		= 0xf << IWL_RX_L3_PROTO_POS,
	IWL_RX_L3L4_L4_PROTO_MASK		= 0xf << 8,
	IWL_RX_L3L4_RSS_HASH_MASK		= 0xf << 12,
};

enum iwl_rx_mpdu_status {
	IWL_RX_MPDU_STATUS_CRC_OK		= BIT(0),
	IWL_RX_MPDU_STATUS_OVERRUN_OK		= BIT(1),
	IWL_RX_MPDU_STATUS_SRC_STA_FOUND	= BIT(2),
	IWL_RX_MPDU_STATUS_KEY_VALID		= BIT(3),
	IWL_RX_MPDU_STATUS_ICV_OK		= BIT(5),
	IWL_RX_MPDU_STATUS_MIC_OK		= BIT(6),
	IWL_RX_MPDU_RES_STATUS_TTAK_OK		= BIT(7),
	/* overlayed since IWL_UCODE_TLV_API_DEPRECATE_TTAK */
	IWL_RX_MPDU_STATUS_REPLAY_ERROR		= BIT(7),
	IWL_RX_MPDU_STATUS_SEC_MASK		= 0x7 << 8,
	IWL_RX_MPDU_STATUS_SEC_UNKNOWN		= IWL_RX_MPDU_STATUS_SEC_MASK,
	IWL_RX_MPDU_STATUS_SEC_NONE		= 0x0 << 8,
	IWL_RX_MPDU_STATUS_SEC_WEP		= 0x1 << 8,
	IWL_RX_MPDU_STATUS_SEC_CCM		= 0x2 << 8,
	IWL_RX_MPDU_STATUS_SEC_TKIP		= 0x3 << 8,
	IWL_RX_MPDU_STATUS_SEC_EXT_ENC		= 0x4 << 8,
	IWL_RX_MPDU_STATUS_SEC_GCM		= 0x5 << 8,
#if defined(__FreeBSD__)
	IWL_RX_MPDU_STATUS_SEC_ENC_ERR		= 0x7 << 8,
#endif
	IWL_RX_MPDU_STATUS_DECRYPTED		= BIT(11),
	IWL_RX_MPDU_STATUS_ROBUST_MNG_FRAME	= BIT(15),

	IWL_RX_MPDU_STATUS_DUPLICATE		= BIT(22),

	IWL_RX_MPDU_STATUS_STA_ID		= 0x1f000000,
};

#define IWL_RX_REORDER_DATA_INVALID_BAID 0x7f

enum iwl_rx_mpdu_reorder_data {
	IWL_RX_MPDU_REORDER_NSSN_MASK		= 0x00000fff,
	IWL_RX_MPDU_REORDER_SN_MASK		= 0x00fff000,
	IWL_RX_MPDU_REORDER_SN_SHIFT		= 12,
	IWL_RX_MPDU_REORDER_BAID_MASK		= 0x7f000000,
	IWL_RX_MPDU_REORDER_BAID_SHIFT		= 24,
	IWL_RX_MPDU_REORDER_BA_OLD_SN		= 0x80000000,
};

enum iwl_rx_mpdu_phy_info {
	IWL_RX_MPDU_PHY_AMPDU		= BIT(5),
	IWL_RX_MPDU_PHY_AMPDU_TOGGLE	= BIT(6),
	IWL_RX_MPDU_PHY_SHORT_PREAMBLE	= BIT(7),
	/* short preamble is only for CCK, for non-CCK overridden by this */
	IWL_RX_MPDU_PHY_NCCK_ADDTL_NTFY	= BIT(7),
	IWL_RX_MPDU_PHY_TSF_OVERLOAD	= BIT(8),
};

enum iwl_rx_mpdu_mac_info {
	IWL_RX_MPDU_PHY_MAC_INDEX_MASK		= 0x0f,
	IWL_RX_MPDU_PHY_PHY_INDEX_MASK		= 0xf0,
};

/* TSF overload low dword */
enum iwl_rx_phy_he_data0 {
	/* info type: HE any */
	IWL_RX_PHY_DATA0_HE_BEAM_CHNG				= 0x00000001,
	IWL_RX_PHY_DATA0_HE_UPLINK				= 0x00000002,
	IWL_RX_PHY_DATA0_HE_BSS_COLOR_MASK			= 0x000000fc,
	IWL_RX_PHY_DATA0_HE_SPATIAL_REUSE_MASK			= 0x00000f00,
	/* 1 bit reserved */
	IWL_RX_PHY_DATA0_HE_TXOP_DUR_MASK			= 0x000fe000,
	IWL_RX_PHY_DATA0_HE_LDPC_EXT_SYM			= 0x00100000,
	IWL_RX_PHY_DATA0_HE_PRE_FEC_PAD_MASK			= 0x00600000,
	IWL_RX_PHY_DATA0_HE_PE_DISAMBIG				= 0x00800000,
	IWL_RX_PHY_DATA0_HE_DOPPLER				= 0x01000000,
	/* 6 bits reserved */
	IWL_RX_PHY_DATA0_HE_DELIM_EOF				= 0x80000000,
};

/* TSF overload low dword */
enum iwl_rx_phy_eht_data0 {
	/* info type: EHT any */
	IWL_RX_PHY_DATA0_EHT_VALIDATE				= BIT(0),
	IWL_RX_PHY_DATA0_EHT_UPLINK				= BIT(1),
	IWL_RX_PHY_DATA0_EHT_BSS_COLOR_MASK			= 0x000000fc,
	IWL_RX_PHY_DATA0_ETH_SPATIAL_REUSE_MASK			= 0x00000f00,
	IWL_RX_PHY_DATA0_EHT_PS160				= BIT(12),
	IWL_RX_PHY_DATA0_EHT_TXOP_DUR_MASK			= 0x000fe000,
	IWL_RX_PHY_DATA0_EHT_LDPC_EXT_SYM			= BIT(20),
	IWL_RX_PHY_DATA0_EHT_PRE_FEC_PAD_MASK			= 0x00600000,
	IWL_RX_PHY_DATA0_EHT_PE_DISAMBIG			= BIT(23),
	IWL_RX_PHY_DATA0_EHT_BW320_SLOT				= BIT(24),
	IWL_RX_PHY_DATA0_EHT_SIGA_CRC_OK			= BIT(25),
	IWL_RX_PHY_DATA0_EHT_PHY_VER				= 0x1c000000,
	/* 2 bits reserved */
	IWL_RX_PHY_DATA0_EHT_DELIM_EOF				= BIT(31),
};

enum iwl_rx_phy_info_type {
	IWL_RX_PHY_INFO_TYPE_NONE				= 0,
	IWL_RX_PHY_INFO_TYPE_CCK				= 1,
	IWL_RX_PHY_INFO_TYPE_OFDM_LGCY				= 2,
	IWL_RX_PHY_INFO_TYPE_HT					= 3,
	IWL_RX_PHY_INFO_TYPE_VHT_SU				= 4,
	IWL_RX_PHY_INFO_TYPE_VHT_MU				= 5,
	IWL_RX_PHY_INFO_TYPE_HE_SU				= 6,
	IWL_RX_PHY_INFO_TYPE_HE_MU				= 7,
	IWL_RX_PHY_INFO_TYPE_HE_TB				= 8,
	IWL_RX_PHY_INFO_TYPE_HE_MU_EXT				= 9,
	IWL_RX_PHY_INFO_TYPE_HE_TB_EXT				= 10,
	IWL_RX_PHY_INFO_TYPE_EHT_MU				= 11,
	IWL_RX_PHY_INFO_TYPE_EHT_TB				= 12,
	IWL_RX_PHY_INFO_TYPE_EHT_MU_EXT				= 13,
	IWL_RX_PHY_INFO_TYPE_EHT_TB_EXT				= 14,
};

/* TSF overload high dword */
enum iwl_rx_phy_common_data1 {
	/*
	 * check this first - if TSF overload is set,
	 * see &enum iwl_rx_phy_info_type
	 */
	IWL_RX_PHY_DATA1_INFO_TYPE_MASK				= 0xf0000000,

	/* info type: HT/VHT/HE/EHT any */
	IWL_RX_PHY_DATA1_LSIG_LEN_MASK				= 0x0fff0000,
};

/* TSF overload high dword For HE rates*/
enum iwl_rx_phy_he_data1 {
	/* info type: HE MU/MU-EXT */
	IWL_RX_PHY_DATA1_HE_MU_SIGB_COMPRESSION			= 0x00000001,
	IWL_RX_PHY_DATA1_HE_MU_SIBG_SYM_OR_USER_NUM_MASK	= 0x0000001e,

	/* info type: HE any */
	IWL_RX_PHY_DATA1_HE_LTF_NUM_MASK			= 0x000000e0,
	IWL_RX_PHY_DATA1_HE_RU_ALLOC_SEC80			= 0x00000100,
	/* trigger encoded */
	IWL_RX_PHY_DATA1_HE_RU_ALLOC_MASK			= 0x0000fe00,

	/* info type: HE TB/TX-EXT */
	IWL_RX_PHY_DATA1_HE_TB_PILOT_TYPE			= 0x00000001,
	IWL_RX_PHY_DATA1_HE_TB_LOW_SS_MASK			= 0x0000000e,
};

/* TSF overload high dword For EHT-MU/TB rates*/
enum iwl_rx_phy_eht_data1 {
	/* info type: EHT-MU */
	IWL_RX_PHY_DATA1_EHT_MU_NUM_SIG_SYM_USIGA2	= 0x0000001f,
	/* info type: EHT-TB */
	IWL_RX_PHY_DATA1_EHT_TB_PILOT_TYPE		= BIT(0),
	IWL_RX_PHY_DATA1_EHT_TB_LOW_SS			= 0x0000001e,

	/* info type: EHT any */
	/* number of EHT-LTF symbols 0 - 1 EHT-LTF, 1 - 2 EHT-LTFs, 2 - 4 EHT-LTFs,
	 * 3 - 6 EHT-LTFs, 4 - 8 EHT-LTFs */
	IWL_RX_PHY_DATA1_EHT_SIG_LTF_NUM		= 0x000000e0,
	IWL_RX_PHY_DATA1_EHT_RU_ALLOC_B0		= 0x00000100,
	IWL_RX_PHY_DATA1_EHT_RU_ALLOC_B1_B7		= 0x0000fe00,
};

/* goes into Metadata DW 7 (Qu) or 8 (So or higher) */
enum iwl_rx_phy_he_data2 {
	/* info type: HE MU-EXT */
	/* the a1/a2/... is what the PHY/firmware calls the values */
	IWL_RX_PHY_DATA2_HE_MU_EXT_CH1_RU0		= 0x000000ff, /* a1 */
	IWL_RX_PHY_DATA2_HE_MU_EXT_CH1_RU2		= 0x0000ff00, /* a2 */
	IWL_RX_PHY_DATA2_HE_MU_EXT_CH2_RU0		= 0x00ff0000, /* b1 */
	IWL_RX_PHY_DATA2_HE_MU_EXT_CH2_RU2		= 0xff000000, /* b2 */

	/* info type: HE TB-EXT */
	IWL_RX_PHY_DATA2_HE_TB_EXT_SPTL_REUSE1		= 0x0000000f,
	IWL_RX_PHY_DATA2_HE_TB_EXT_SPTL_REUSE2		= 0x000000f0,
	IWL_RX_PHY_DATA2_HE_TB_EXT_SPTL_REUSE3		= 0x00000f00,
	IWL_RX_PHY_DATA2_HE_TB_EXT_SPTL_REUSE4		= 0x0000f000,
};

/* goes into Metadata DW 8 (Qu) or 7 (So or higher) */
enum iwl_rx_phy_he_data3 {
	/* info type: HE MU-EXT */
	IWL_RX_PHY_DATA3_HE_MU_EXT_CH1_RU1		= 0x000000ff, /* c1 */
	IWL_RX_PHY_DATA3_HE_MU_EXT_CH1_RU3		= 0x0000ff00, /* c2 */
	IWL_RX_PHY_DATA3_HE_MU_EXT_CH2_RU1		= 0x00ff0000, /* d1 */
	IWL_RX_PHY_DATA3_HE_MU_EXT_CH2_RU3		= 0xff000000, /* d2 */
};

/* goes into Metadata DW 4 high 16 bits */
enum iwl_rx_phy_he_he_data4 {
	/* info type: HE MU-EXT */
	IWL_RX_PHY_DATA4_HE_MU_EXT_CH1_CTR_RU			= 0x0001,
	IWL_RX_PHY_DATA4_HE_MU_EXT_CH2_CTR_RU			= 0x0002,
	IWL_RX_PHY_DATA4_HE_MU_EXT_CH1_CRC_OK			= 0x0004,
	IWL_RX_PHY_DATA4_HE_MU_EXT_CH2_CRC_OK			= 0x0008,
	IWL_RX_PHY_DATA4_HE_MU_EXT_SIGB_MCS_MASK		= 0x00f0,
	IWL_RX_PHY_DATA4_HE_MU_EXT_SIGB_DCM			= 0x0100,
	IWL_RX_PHY_DATA4_HE_MU_EXT_PREAMBLE_PUNC_TYPE_MASK	= 0x0600,
};

/* goes into Metadata DW 8 (Qu has no EHT) */
enum iwl_rx_phy_eht_data2 {
	/* info type: EHT-MU-EXT */
	IWL_RX_PHY_DATA2_EHT_MU_EXT_RU_ALLOC_A1	= 0x000001ff,
	IWL_RX_PHY_DATA2_EHT_MU_EXT_RU_ALLOC_A2	= 0x0003fe00,
	IWL_RX_PHY_DATA2_EHT_MU_EXT_RU_ALLOC_B1	= 0x07fc0000,

	/* info type: EHT-TB-EXT */
	IWL_RX_PHY_DATA2_EHT_TB_EXT_TRIG_SIGA1	= 0xffffffff,
};

/* goes into Metadata DW 7 (Qu has no EHT) */
enum iwl_rx_phy_eht_data3 {
	/* note: low 8 bits cannot be used */
	/* info type: EHT-MU-EXT */
	IWL_RX_PHY_DATA3_EHT_MU_EXT_RU_ALLOC_C1	= 0x0003fe00,
	IWL_RX_PHY_DATA3_EHT_MU_EXT_RU_ALLOC_C2	= 0x07fc0000,
};

/* goes into Metadata DW 4 */
enum iwl_rx_phy_eht_data4 {
	/* info type: EHT-MU-EXT */
	IWL_RX_PHY_DATA4_EHT_MU_EXT_RU_ALLOC_D1	= 0x000001ff,
	IWL_RX_PHY_DATA4_EHT_MU_EXT_RU_ALLOC_D2	= 0x0003fe00,
	IWL_RX_PHY_DATA4_EHT_MU_EXT_SIGB_MCS	= 0x000c0000,
	IWL_RX_PHY_DATA4_EHT_MU_EXT_RU_ALLOC_B2	= 0x1ff00000,
};

/* goes into Metadata DW 16 */
enum iwl_rx_phy_data5 {
	/* info type: EHT any */
	IWL_RX_PHY_DATA5_EHT_TYPE_AND_COMP		= 0x00000003,
	/* info type: EHT-TB */
	IWL_RX_PHY_DATA5_EHT_TB_SPATIAL_REUSE1		= 0x0000003c,
	IWL_RX_PHY_DATA5_EHT_TB_SPATIAL_REUSE2		= 0x000003c0,
	/* info type: EHT-MU */
	IWL_RX_PHY_DATA5_EHT_MU_PUNC_CH_CODE		= 0x0000007c,
	IWL_RX_PHY_DATA5_EHT_MU_STA_ID_USR		= 0x0003ff80,
	IWL_RX_PHY_DATA5_EHT_MU_NUM_USR_NON_OFDMA	= 0x001c0000,
	IWL_RX_PHY_DATA5_EHT_MU_SPATIAL_CONF_USR_FIELD	= 0x0fe00000,
};

/**
 * struct iwl_rx_mpdu_desc_v1 - RX MPDU descriptor
 */
struct iwl_rx_mpdu_desc_v1 {
	/* DW7 - carries rss_hash only when rpa_en == 1 */
	union {
		/**
		 * @rss_hash: RSS hash value
		 */
		__le32 rss_hash;

		/**
		 * @phy_data2: depends on info type (see @phy_data1)
		 */
		__le32 phy_data2;
	};

	/* DW8 - carries filter_match only when rpa_en == 1 */
	union {
		/**
		 * @filter_match: filter match value
		 */
		__le32 filter_match;

		/**
		 * @phy_data3: depends on info type (see @phy_data1)
		 */
		__le32 phy_data3;
	};

	/* DW9 */
	/**
	 * @rate_n_flags: RX rate/flags encoding
	 */
	__le32 rate_n_flags;
	/* DW10 */
	/**
	 * @energy_a: energy chain A
	 */
	u8 energy_a;
	/**
	 * @energy_b: energy chain B
	 */
	u8 energy_b;
	/**
	 * @channel: channel number
	 */
	u8 channel;
	/**
	 * @mac_context: MAC context mask
	 */
	u8 mac_context;
	/* DW11 */
	/**
	 * @gp2_on_air_rise: GP2 timer value on air rise (INA)
	 */
	__le32 gp2_on_air_rise;
	/* DW12 & DW13 */
	union {
		/**
		 * @tsf_on_air_rise:
		 * TSF value on air rise (INA), only valid if
		 * %IWL_RX_MPDU_PHY_TSF_OVERLOAD isn't set
		 */
		__le64 tsf_on_air_rise;

		struct {
			/**
			 * @phy_data0: depends on info_type, see @phy_data1
			 */
			__le32 phy_data0;
			/**
			 * @phy_data1: valid only if
			 * %IWL_RX_MPDU_PHY_TSF_OVERLOAD is set,
			 * see &enum iwl_rx_phy_common_data1 or
			 *     &enum iwl_rx_phy_he_data1 or
			 *     &enum iwl_rx_phy_eht_data1.
			 */
			__le32 phy_data1;
		};
	};
} __packed; /* RX_MPDU_RES_START_API_S_VER_4 */

/**
 * struct iwl_rx_mpdu_desc_v3 - RX MPDU descriptor
 */
struct iwl_rx_mpdu_desc_v3 {
	/* DW7 - carries filter_match only when rpa_en == 1 */
	union {
		/**
		 * @filter_match: filter match value
		 */
		__le32 filter_match;

		/**
		 * @phy_data3: depends on info type (see @phy_data1)
		 */
		__le32 phy_data3;
	};

	/* DW8 - carries rss_hash only when rpa_en == 1 */
	union {
		/**
		 * @rss_hash: RSS hash value
		 */
		__le32 rss_hash;

		/**
		 * @phy_data2: depends on info type (see @phy_data1)
		 */
		__le32 phy_data2;
	};
	/* DW9 */
	/**
	 * @partial_hash: 31:0 ip/tcp header hash
	 *	w/o some fields (such as IP SRC addr)
	 */
	__le32 partial_hash;
	/* DW10 */
	/**
	 * @raw_xsum: raw xsum value
	 */
	__be16 raw_xsum;
	/**
	 * @reserved_xsum: reserved high bits in the raw checksum
	 */
	__le16 reserved_xsum;
	/* DW11 */
	/**
	 * @rate_n_flags: RX rate/flags encoding
	 */
	__le32 rate_n_flags;
	/* DW12 */
	/**
	 * @energy_a: energy chain A
	 */
	u8 energy_a;
	/**
	 * @energy_b: energy chain B
	 */
	u8 energy_b;
	/**
	 * @channel: channel number
	 */
	u8 channel;
	/**
	 * @mac_context: MAC context mask
	 */
	u8 mac_context;
	/* DW13 */
	/**
	 * @gp2_on_air_rise: GP2 timer value on air rise (INA)
	 */
	__le32 gp2_on_air_rise;
	/* DW14 & DW15 */
	union {
		/**
		 * @tsf_on_air_rise:
		 * TSF value on air rise (INA), only valid if
		 * %IWL_RX_MPDU_PHY_TSF_OVERLOAD isn't set
		 */
		__le64 tsf_on_air_rise;

		struct {
			/**
			 * @phy_data0: depends on info_type, see @phy_data1
			 */
			__le32 phy_data0;
			/**
			 * @phy_data1: valid only if
			 * %IWL_RX_MPDU_PHY_TSF_OVERLOAD is set,
			 * see &enum iwl_rx_phy_data1.
			 */
			__le32 phy_data1;
		};
	};
	/* DW16 */
	/**
	 * @phy_data5: valid only if
	 * %IWL_RX_MPDU_PHY_TSF_OVERLOAD is set,
	 * see &enum iwl_rx_phy_data5.
	 */
	__le32 phy_data5;
	/* DW17 */
	/**
	 * @reserved: reserved
	 */
	__le32 reserved[1];
} __packed; /* RX_MPDU_RES_START_API_S_VER_3,
	       RX_MPDU_RES_START_API_S_VER_5 */

/**
 * struct iwl_rx_mpdu_desc - RX MPDU descriptor
 */
struct iwl_rx_mpdu_desc {
	/* DW2 */
	/**
	 * @mpdu_len: MPDU length
	 */
	__le16 mpdu_len;
	/**
	 * @mac_flags1: &enum iwl_rx_mpdu_mac_flags1
	 */
	u8 mac_flags1;
	/**
	 * @mac_flags2: &enum iwl_rx_mpdu_mac_flags2
	 */
	u8 mac_flags2;
	/* DW3 */
	/**
	 * @amsdu_info: &enum iwl_rx_mpdu_amsdu_info
	 */
	u8 amsdu_info;
	/**
	 * @phy_info: &enum iwl_rx_mpdu_phy_info
	 */
	__le16 phy_info;
	/**
	 * @mac_phy_idx: MAC/PHY index
	 */
	u8 mac_phy_idx;
	/* DW4 */
	union {
		struct {
			/* carries csum data only when rpa_en == 1 */
			/**
			 * @raw_csum: raw checksum (alledgedly unreliable)
			 */
			__le16 raw_csum;

			union {
				/**
				 * @l3l4_flags: &enum iwl_rx_l3l4_flags
				 */
				__le16 l3l4_flags;

				/**
				 * @phy_data4: depends on info type, see phy_data1
				 */
				__le16 phy_data4;
			};
		};
		/**
		 * @phy_eht_data4: depends on info type, see phy_data1
		 */
		__le32 phy_eht_data4;
	};
	/* DW5 */
	/**
	 * @status: &enum iwl_rx_mpdu_status
	 */
	__le32 status;

	/* DW6 */
	/**
	 * @reorder_data: &enum iwl_rx_mpdu_reorder_data
	 */
	__le32 reorder_data;

	union {
		/**
		 * @v1: version 1 of the remaining RX descriptor,
		 *	see &struct iwl_rx_mpdu_desc_v1
		 */
		struct iwl_rx_mpdu_desc_v1 v1;
		/**
		 * @v3: version 3 of the remaining RX descriptor,
		 *	see &struct iwl_rx_mpdu_desc_v3
		 */
		struct iwl_rx_mpdu_desc_v3 v3;
	};
} __packed; /* RX_MPDU_RES_START_API_S_VER_3,
	       RX_MPDU_RES_START_API_S_VER_4,
	       RX_MPDU_RES_START_API_S_VER_5 */

#define IWL_RX_DESC_SIZE_V1 offsetofend(struct iwl_rx_mpdu_desc, v1)

#define RX_NO_DATA_CHAIN_A_POS		0
#define RX_NO_DATA_CHAIN_A_MSK		(0xff << RX_NO_DATA_CHAIN_A_POS)
#define RX_NO_DATA_CHAIN_B_POS		8
#define RX_NO_DATA_CHAIN_B_MSK		(0xff << RX_NO_DATA_CHAIN_B_POS)
#define RX_NO_DATA_CHANNEL_POS		16
#define RX_NO_DATA_CHANNEL_MSK		(0xff << RX_NO_DATA_CHANNEL_POS)

#define RX_NO_DATA_INFO_TYPE_POS	0
#define RX_NO_DATA_INFO_TYPE_MSK	(0xff << RX_NO_DATA_INFO_TYPE_POS)
#define RX_NO_DATA_INFO_TYPE_NONE	0
#define RX_NO_DATA_INFO_TYPE_RX_ERR	1
#define RX_NO_DATA_INFO_TYPE_NDP	2
#define RX_NO_DATA_INFO_TYPE_MU_UNMATCHED	3
#define RX_NO_DATA_INFO_TYPE_TB_UNMATCHED	4

#define RX_NO_DATA_INFO_ERR_POS		8
#define RX_NO_DATA_INFO_ERR_MSK		(0xff << RX_NO_DATA_INFO_ERR_POS)
#define RX_NO_DATA_INFO_ERR_NONE	0
#define RX_NO_DATA_INFO_ERR_BAD_PLCP	1
#define RX_NO_DATA_INFO_ERR_UNSUPPORTED_RATE	2
#define RX_NO_DATA_INFO_ERR_NO_DELIM		3
#define RX_NO_DATA_INFO_ERR_BAD_MAC_HDR	4
#define RX_NO_DATA_INFO_LOW_ENERGY		5

#define RX_NO_DATA_FRAME_TIME_POS	0
#define RX_NO_DATA_FRAME_TIME_MSK	(0xfffff << RX_NO_DATA_FRAME_TIME_POS)

#define RX_NO_DATA_RX_VEC0_HE_NSTS_MSK	0x03800000
#define RX_NO_DATA_RX_VEC0_VHT_NSTS_MSK	0x38000000
#define RX_NO_DATA_RX_VEC2_EHT_NSTS_MSK	0x00f00000

/* content of OFDM_RX_VECTOR_USIG_A1_OUT */
enum iwl_rx_usig_a1 {
	IWL_RX_USIG_A1_ENHANCED_WIFI_VER_ID	= 0x00000007,
	IWL_RX_USIG_A1_BANDWIDTH		= 0x00000038,
	IWL_RX_USIG_A1_UL_FLAG			= 0x00000040,
	IWL_RX_USIG_A1_BSS_COLOR		= 0x00001f80,
	IWL_RX_USIG_A1_TXOP_DURATION		= 0x000fe000,
	IWL_RX_USIG_A1_DISREGARD		= 0x01f00000,
	IWL_RX_USIG_A1_VALIDATE			= 0x02000000,
	IWL_RX_USIG_A1_EHT_BW320_SLOT		= 0x04000000,
	IWL_RX_USIG_A1_EHT_TYPE			= 0x18000000,
	IWL_RX_USIG_A1_RDY			= 0x80000000,
};

/* content of OFDM_RX_VECTOR_USIG_A2_EHT_OUT */
enum iwl_rx_usig_a2_eht {
	IWL_RX_USIG_A2_EHT_PPDU_TYPE		= 0x00000003,
	IWL_RX_USIG_A2_EHT_USIG2_VALIDATE_B2	= 0x00000004,
	IWL_RX_USIG_A2_EHT_PUNC_CHANNEL		= 0x000000f8,
	IWL_RX_USIG_A2_EHT_USIG2_VALIDATE_B8	= 0x00000100,
	IWL_RX_USIG_A2_EHT_SIG_MCS		= 0x00000600,
	IWL_RX_USIG_A2_EHT_SIG_SYM_NUM		= 0x0000f800,
	IWL_RX_USIG_A2_EHT_TRIG_SPATIAL_REUSE_1 = 0x000f0000,
	IWL_RX_USIG_A2_EHT_TRIG_SPATIAL_REUSE_2 = 0x00f00000,
	IWL_RX_USIG_A2_EHT_TRIG_USIG2_DISREGARD	= 0x1f000000,
	IWL_RX_USIG_A2_EHT_CRC_OK		= 0x40000000,
	IWL_RX_USIG_A2_EHT_RDY			= 0x80000000,
};

/**
 * struct iwl_rx_no_data - RX no data descriptor
 * @info: 7:0 frame type, 15:8 RX error type
 * @rssi: 7:0 energy chain-A,
 *	15:8 chain-B, measured at FINA time (FINA_ENERGY), 16:23 channel
 * @on_air_rise_time: GP2 during on air rise
 * @fr_time: frame time
 * @rate: rate/mcs of frame
 * @phy_info: &enum iwl_rx_phy_he_data0 or &enum iwl_rx_phy_eht_data0
 *	      based on &enum iwl_rx_phy_info_type
 * @rx_vec: DW-12:9 raw RX vectors from DSP according to modulation type.
 *	for VHT: OFDM_RX_VECTOR_SIGA1_OUT, OFDM_RX_VECTOR_SIGA2_OUT
 *	for HE: OFDM_RX_VECTOR_HE_SIGA1_OUT, OFDM_RX_VECTOR_HE_SIGA2_OUT
 */
struct iwl_rx_no_data {
	__le32 info;
	__le32 rssi;
	__le32 on_air_rise_time;
	__le32 fr_time;
	__le32 rate;
	__le32 phy_info[2];
	__le32 rx_vec[2];
} __packed; /* RX_NO_DATA_NTFY_API_S_VER_1,
	       RX_NO_DATA_NTFY_API_S_VER_2 */

/**
 * struct iwl_rx_no_data_ver_3 - RX no data descriptor
 * @info: 7:0 frame type, 15:8 RX error type
 * @rssi: 7:0 energy chain-A,
 *	15:8 chain-B, measured at FINA time (FINA_ENERGY), 16:23 channel
 * @on_air_rise_time: GP2 during on air rise
 * @fr_time: frame time
 * @rate: rate/mcs of frame
 * @phy_info: &enum iwl_rx_phy_eht_data0 and &enum iwl_rx_phy_info_type
 * @rx_vec: DW-12:9 raw RX vectors from DSP according to modulation type.
 *	for VHT: OFDM_RX_VECTOR_SIGA1_OUT, OFDM_RX_VECTOR_SIGA2_OUT
 *	for HE: OFDM_RX_VECTOR_HE_SIGA1_OUT, OFDM_RX_VECTOR_HE_SIGA2_OUT
 *	for EHT: OFDM_RX_VECTOR_USIG_A1_OUT, OFDM_RX_VECTOR_USIG_A2_EHT_OUT,
 *	OFDM_RX_VECTOR_EHT_OUT, OFDM_RX_VECTOR_EHT_USER_FIELD_OUT
 */
struct iwl_rx_no_data_ver_3 {
	__le32 info;
	__le32 rssi;
	__le32 on_air_rise_time;
	__le32 fr_time;
	__le32 rate;
	__le32 phy_info[2];
	__le32 rx_vec[4];
} __packed; /* RX_NO_DATA_NTFY_API_S_VER_1,
	       RX_NO_DATA_NTFY_API_S_VER_2
	       RX_NO_DATA_NTFY_API_S_VER_3 */

struct iwl_frame_release {
	u8 baid;
	u8 reserved;
	__le16 nssn;
};

/**
 * enum iwl_bar_frame_release_sta_tid - STA/TID information for BAR release
 * @IWL_BAR_FRAME_RELEASE_TID_MASK: TID mask
 * @IWL_BAR_FRAME_RELEASE_STA_MASK: STA mask
 */
enum iwl_bar_frame_release_sta_tid {
	IWL_BAR_FRAME_RELEASE_TID_MASK = 0x0000000f,
	IWL_BAR_FRAME_RELEASE_STA_MASK = 0x000001f0,
};

/**
 * enum iwl_bar_frame_release_ba_info - BA information for BAR release
 * @IWL_BAR_FRAME_RELEASE_NSSN_MASK: NSSN mask
 * @IWL_BAR_FRAME_RELEASE_SN_MASK: SN mask (ignored by driver)
 * @IWL_BAR_FRAME_RELEASE_BAID_MASK: BAID mask
 */
enum iwl_bar_frame_release_ba_info {
	IWL_BAR_FRAME_RELEASE_NSSN_MASK	= 0x00000fff,
	IWL_BAR_FRAME_RELEASE_SN_MASK	= 0x00fff000,
	IWL_BAR_FRAME_RELEASE_BAID_MASK	= 0x3f000000,
};

/**
 * struct iwl_bar_frame_release - frame release from BAR info
 * @sta_tid: STA & TID information, see &enum iwl_bar_frame_release_sta_tid.
 * @ba_info: BA information, see &enum iwl_bar_frame_release_ba_info.
 */
struct iwl_bar_frame_release {
	__le32 sta_tid;
	__le32 ba_info;
} __packed; /* RX_BAR_TO_FRAME_RELEASE_API_S_VER_1 */

enum iwl_rss_hash_func_en {
	IWL_RSS_HASH_TYPE_IPV4_TCP,
	IWL_RSS_HASH_TYPE_IPV4_UDP,
	IWL_RSS_HASH_TYPE_IPV4_PAYLOAD,
	IWL_RSS_HASH_TYPE_IPV6_TCP,
	IWL_RSS_HASH_TYPE_IPV6_UDP,
	IWL_RSS_HASH_TYPE_IPV6_PAYLOAD,
};

#define IWL_RSS_HASH_KEY_CNT 10
#define IWL_RSS_INDIRECTION_TABLE_SIZE 128
#define IWL_RSS_ENABLE 1

/**
 * struct iwl_rss_config_cmd - RSS (Receive Side Scaling) configuration
 *
 * @flags: 1 - enable, 0 - disable
 * @hash_mask: Type of RSS to use. Values are from %iwl_rss_hash_func_en
 * @reserved: reserved
 * @secret_key: 320 bit input of random key configuration from driver
 * @indirection_table: indirection table
 */
struct iwl_rss_config_cmd {
	__le32 flags;
	u8 hash_mask;
	u8 reserved[3];
	__le32 secret_key[IWL_RSS_HASH_KEY_CNT];
	u8 indirection_table[IWL_RSS_INDIRECTION_TABLE_SIZE];
} __packed; /* RSS_CONFIG_CMD_API_S_VER_1 */

#define IWL_MULTI_QUEUE_SYNC_SENDER_POS 0
#define IWL_MULTI_QUEUE_SYNC_SENDER_MSK 0xf

/**
 * struct iwl_rxq_sync_cmd - RXQ notification trigger
 *
 * @flags: flags of the notification. bit 0:3 are the sender queue
 * @rxq_mask: rx queues to send the notification on
 * @count: number of bytes in payload, should be DWORD aligned
 * @payload: data to send to rx queues
 */
struct iwl_rxq_sync_cmd {
	__le32 flags;
	__le32 rxq_mask;
	__le32 count;
#if defined(__linux__)
	u8 payload[];
#elif defined(__FreeBSD__)
	u8 payload[0];
#endif
} __packed; /* MULTI_QUEUE_DRV_SYNC_HDR_CMD_API_S_VER_1 */

/**
 * struct iwl_rxq_sync_notification - Notification triggered by RXQ
 * sync command
 *
 * @count: number of bytes in payload
 * @payload: data to send to rx queues
 */
struct iwl_rxq_sync_notification {
	__le32 count;
#if defined(__linux__)
	u8 payload[];
#elif defined(__FreeBSD__)
	u8 payload[0];
#endif
} __packed; /* MULTI_QUEUE_DRV_SYNC_HDR_CMD_API_S_VER_1 */

/**
 * enum iwl_mvm_pm_event - type of station PM event
 * @IWL_MVM_PM_EVENT_AWAKE: station woke up
 * @IWL_MVM_PM_EVENT_ASLEEP: station went to sleep
 * @IWL_MVM_PM_EVENT_UAPSD: station sent uAPSD trigger
 * @IWL_MVM_PM_EVENT_PS_POLL: station sent PS-Poll
 */
enum iwl_mvm_pm_event {
	IWL_MVM_PM_EVENT_AWAKE,
	IWL_MVM_PM_EVENT_ASLEEP,
	IWL_MVM_PM_EVENT_UAPSD,
	IWL_MVM_PM_EVENT_PS_POLL,
}; /* PEER_PM_NTFY_API_E_VER_1 */

/**
 * struct iwl_mvm_pm_state_notification - station PM state notification
 * @sta_id: station ID of the station changing state
 * @type: the new powersave state, see &enum iwl_mvm_pm_event
 */
struct iwl_mvm_pm_state_notification {
	u8 sta_id;
	u8 type;
	/* private: */
	__le16 reserved;
} __packed; /* PEER_PM_NTFY_API_S_VER_1 */

#define BA_WINDOW_STREAMS_MAX		16
#define BA_WINDOW_STATUS_TID_MSK	0x000F
#define BA_WINDOW_STATUS_STA_ID_POS	4
#define BA_WINDOW_STATUS_STA_ID_MSK	0x01F0
#define BA_WINDOW_STATUS_VALID_MSK	BIT(9)

/**
 * struct iwl_ba_window_status_notif - reordering window's status notification
 * @bitmap: bitmap of received frames [start_seq_num + 0]..[start_seq_num + 63]
 * @ra_tid: bit 3:0 - TID, bit 8:4 - STA_ID, bit 9 - valid
 * @start_seq_num: the start sequence number of the bitmap
 * @mpdu_rx_count: the number of received MPDUs since entering D0i3
 */
struct iwl_ba_window_status_notif {
	__le64 bitmap[BA_WINDOW_STREAMS_MAX];
	__le16 ra_tid[BA_WINDOW_STREAMS_MAX];
	__le32 start_seq_num[BA_WINDOW_STREAMS_MAX];
	__le16 mpdu_rx_count[BA_WINDOW_STREAMS_MAX];
} __packed; /* BA_WINDOW_STATUS_NTFY_API_S_VER_1 */

/**
 * struct iwl_rfh_queue_data - RX queue configuration
 * @q_num: Q num
 * @enable: enable queue
 * @reserved: alignment
 * @urbd_stts_wrptr: DMA address of urbd_stts_wrptr
 * @fr_bd_cb: DMA address of freeRB table
 * @ur_bd_cb: DMA address of used RB table
 * @fr_bd_wid: Initial index of the free table
 */
struct iwl_rfh_queue_data {
	u8 q_num;
	u8 enable;
	__le16 reserved;
	__le64 urbd_stts_wrptr;
	__le64 fr_bd_cb;
	__le64 ur_bd_cb;
	__le32 fr_bd_wid;
} __packed; /* RFH_QUEUE_CONFIG_S_VER_1 */

/**
 * struct iwl_rfh_queue_config - RX queue configuration
 * @num_queues: number of queues configured
 * @reserved: alignment
 * @data: DMA addresses per-queue
 */
struct iwl_rfh_queue_config {
	u8 num_queues;
	u8 reserved[3];
#if defined(__linux__)
	struct iwl_rfh_queue_data data[];
#elif defined(__FreeBSD__)
	struct iwl_rfh_queue_data data[0];
#endif
} __packed; /* RFH_QUEUE_CONFIG_API_S_VER_1 */

#endif /* __iwl_fw_api_rx_h__ */
