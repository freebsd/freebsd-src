/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
#ifndef _ICP_QAT_FW_INIT_ADMIN_H_
#define _ICP_QAT_FW_INIT_ADMIN_H_

#include "icp_qat_fw.h"

enum icp_qat_fw_init_admin_cmd_id {
	ICP_QAT_FW_INIT_ME = 0,
	ICP_QAT_FW_TRNG_ENABLE = 1,
	ICP_QAT_FW_TRNG_DISABLE = 2,
	ICP_QAT_FW_CONSTANTS_CFG = 3,
	ICP_QAT_FW_STATUS_GET = 4,
	ICP_QAT_FW_COUNTERS_GET = 5,
	ICP_QAT_FW_LOOPBACK = 6,
	ICP_QAT_FW_HEARTBEAT_SYNC = 7,
	ICP_QAT_FW_HEARTBEAT_GET = 8,
	ICP_QAT_FW_COMP_CAPABILITY_GET = 9,
	ICP_QAT_FW_CRYPTO_CAPABILITY_GET = 10,
	ICP_QAT_FW_HEARTBEAT_TIMER_SET = 13,
	ICP_QAT_FW_RL_SLA_CONFIG = 14,
	ICP_QAT_FW_RL_INIT = 15,
	ICP_QAT_FW_RL_DU_START = 16,
	ICP_QAT_FW_RL_DU_STOP = 17,
	ICP_QAT_FW_TIMER_GET = 19,
	ICP_QAT_FW_CNV_STATS_GET = 20,
	ICP_QAT_FW_PKE_REPLAY_STATS_GET = 21
};

enum icp_qat_fw_init_admin_resp_status {
	ICP_QAT_FW_INIT_RESP_STATUS_SUCCESS = 0,
	ICP_QAT_FW_INIT_RESP_STATUS_FAIL = 1,
	ICP_QAT_FW_INIT_RESP_STATUS_UNSUPPORTED = 4
};

enum icp_qat_fw_cnv_error_type {
	CNV_ERR_TYPE_NO_ERROR = 0,
	CNV_ERR_TYPE_CHECKSUM_ERROR,
	CNV_ERR_TYPE_DECOMP_PRODUCED_LENGTH_ERROR,
	CNV_ERR_TYPE_DECOMPRESSION_ERROR,
	CNV_ERR_TYPE_TRANSLATION_ERROR,
	CNV_ERR_TYPE_DECOMP_CONSUMED_LENGTH_ERROR,
	CNV_ERR_TYPE_UNKNOWN_ERROR
};

#define CNV_ERROR_TYPE_GET(latest_error)                                       \
	({                                                                     \
		__typeof__(latest_error) _lerror = latest_error;               \
		(_lerror >> 12) > CNV_ERR_TYPE_UNKNOWN_ERROR ?                 \
		    CNV_ERR_TYPE_UNKNOWN_ERROR :                               \
		    (enum icp_qat_fw_cnv_error_type)(_lerror >> 12);           \
	})
#define CNV_ERROR_LENGTH_DELTA_GET(latest_error)                               \
	({                                                                     \
		__typeof__(latest_error) _lerror = latest_error;               \
		((s16)((_lerror & 0x0FFF) | (_lerror & 0x0800 ? 0xF000 : 0))); \
	})
#define CNV_ERROR_DECOMP_STATUS_GET(latest_error) ((s8)(latest_error & 0xFF))

struct icp_qat_fw_init_admin_req {
	u16 init_cfg_sz;
	u8 resrvd1;
	u8 cmd_id;
	u32 max_req_duration;
	u64 opaque_data;

	union {
		/* ICP_QAT_FW_INIT_ME */
		struct {
			u64 resrvd2;
			u16 ibuf_size_in_kb;
			u16 resrvd3;
			u32 resrvd4;
		};
		/* ICP_QAT_FW_CONSTANTS_CFG */
		struct {
			u64 init_cfg_ptr;
			u64 resrvd5;
		};
		/* ICP_QAT_FW_HEARTBEAT_TIMER_SET */
		struct {
			u64 hb_cfg_ptr;
			u32 heartbeat_ticks;
			u32 resrvd6;
		};
		/* ICP_QAT_FW_RL_SLA_CONFIG */
		struct {
			u32 credit_per_sla;
			u8 service_id;
			u8 vf_id;
			u8 resrvd7;
			u8 resrvd8;
			u32 resrvd9;
			u32 resrvd10;
		};
		/* ICP_QAT_FW_RL_INIT */
		struct {
			u32 rl_period;
			u8 config;
			u8 resrvd11;
			u8 num_me;
			u8 resrvd12;
			u8 pke_svc_arb_map;
			u8 bulk_crypto_svc_arb_map;
			u8 compression_svc_arb_map;
			u8 resrvd13;
			u32 resrvd14;
		};
		/* ICP_QAT_FW_RL_DU_STOP */
		struct {
			u64 cfg_ptr;
			u32 resrvd15;
			u32 resrvd16;
		};
	};
} __packed;

struct icp_qat_fw_init_admin_resp {
	u8 flags;
	u8 resrvd1;
	u8 status;
	u8 cmd_id;
	union {
		u32 resrvd2;
		u32 ras_event_count;
		/* ICP_QAT_FW_STATUS_GET */
		struct {
			u16 version_minor_num;
			u16 version_major_num;
		};
		/* ICP_QAT_FW_COMP_CAPABILITY_GET */
		u32 extended_features;
		/* ICP_QAT_FW_CNV_STATS_GET */
		struct {
			u16 error_count;
			u16 latest_error;
		};
	};
	u64 opaque_data;
	union {
		u32 resrvd3[4];
		/* ICP_QAT_FW_STATUS_GET */
		struct {
			u32 version_patch_num;
			u8 context_id;
			u8 ae_id;
			u16 resrvd4;
			u64 resrvd5;
		};
		/* ICP_QAT_FW_COMP_CAPABILITY_GET */
		struct {
			u16 compression_algos;
			u16 checksum_algos;
			u32 deflate_capabilities;
			u32 resrvd6;
			u32 deprecated;
		};
		/* ICP_QAT_FW_CRYPTO_CAPABILITY_GET */
		struct {
			u32 cipher_algos;
			u32 hash_algos;
			u16 keygen_algos;
			u16 other;
			u16 public_key_algos;
			u16 prime_algos;
		};
		/* ICP_QAT_FW_RL_DU_STOP */
		struct {
			u32 resrvd7;
			u8 granularity;
			u8 resrvd8;
			u16 resrvd9;
			u32 total_du_time;
			u32 resrvd10;
		};
		/* ICP_QAT_FW_TIMER_GET  */
		struct {
			u64 timestamp;
			u64 resrvd11;
		};
		/* ICP_QAT_FW_COUNTERS_GET */
		struct {
			u64 req_rec_count;
			u64 resp_sent_count;
		};
		/* ICP_QAT_FW_PKE_REPLAY_STATS_GET */
		struct {
			u32 successful_count;
			u32 unsuccessful_count;
			u64 resrvd12;
		};
	};
} __packed;

enum icp_qat_fw_init_admin_init_flag { ICP_QAT_FW_INIT_FLAG_PKE_DISABLED = 0 };

struct icp_qat_fw_init_admin_hb_cnt {
	u16 resp_heartbeat_cnt;
	u16 req_heartbeat_cnt;
};

struct icp_qat_fw_init_admin_hb_stats {
	struct icp_qat_fw_init_admin_hb_cnt stats[ADF_NUM_HB_CNT_PER_AE];
};

#define ICP_QAT_FW_COMN_HEARTBEAT_OK 0
#define ICP_QAT_FW_COMN_HEARTBEAT_BLOCKED 1
#define ICP_QAT_FW_COMN_HEARTBEAT_FLAG_BITPOS 0
#define ICP_QAT_FW_COMN_HEARTBEAT_FLAG_MASK 0x1
#define ICP_QAT_FW_COMN_STATUS_RESRVD_FLD_MASK 0xFE
#define ICP_QAT_FW_COMN_HEARTBEAT_HDR_FLAG_GET(hdr_t)                          \
	ICP_QAT_FW_COMN_HEARTBEAT_FLAG_GET(hdr_t.flags)

#define ICP_QAT_FW_COMN_HEARTBEAT_HDR_FLAG_SET(hdr_t, val)                     \
	ICP_QAT_FW_COMN_HEARTBEAT_FLAG_SET(hdr_t, val)

#define ICP_QAT_FW_COMN_HEARTBEAT_FLAG_GET(flags)                              \
	QAT_FIELD_GET(flags,                                                   \
		      ICP_QAT_FW_COMN_HEARTBEAT_FLAG_BITPOS,                   \
		      ICP_QAT_FW_COMN_HEARTBEAT_FLAG_MASK)
#endif
