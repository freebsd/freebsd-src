/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2024, Intel Corporation
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

#ifndef _VIRTCHNL_INLINE_IPSEC_H_
#define _VIRTCHNL_INLINE_IPSEC_H_

#define VIRTCHNL_IPSEC_MAX_CRYPTO_CAP_NUM	3
#define VIRTCHNL_IPSEC_MAX_ALGO_CAP_NUM		16
#define VIRTCHNL_IPSEC_MAX_TX_DESC_NUM		128
#define VIRTCHNL_IPSEC_MAX_CRYPTO_ITEM_NUMBER	2
#define VIRTCHNL_IPSEC_MAX_KEY_LEN		128
#define VIRTCHNL_IPSEC_MAX_SA_DESTROY_NUM	8
#define VIRTCHNL_IPSEC_SA_DESTROY		0
#define VIRTCHNL_IPSEC_BROADCAST_VFID		0xFFFFFFFF
#define VIRTCHNL_IPSEC_INVALID_REQ_ID		0xFFFF
#define VIRTCHNL_IPSEC_INVALID_SA_CFG_RESP	0xFFFFFFFF
#define VIRTCHNL_IPSEC_INVALID_SP_CFG_RESP	0xFFFFFFFF

/* crypto type */
#define VIRTCHNL_AUTH		1
#define VIRTCHNL_CIPHER		2
#define VIRTCHNL_AEAD		3

/* caps enabled */
#define VIRTCHNL_IPSEC_ESN_ENA			BIT(0)
#define VIRTCHNL_IPSEC_UDP_ENCAP_ENA		BIT(1)
#define VIRTCHNL_IPSEC_SA_INDEX_SW_ENA		BIT(2)
#define VIRTCHNL_IPSEC_AUDIT_ENA		BIT(3)
#define VIRTCHNL_IPSEC_BYTE_LIMIT_ENA		BIT(4)
#define VIRTCHNL_IPSEC_DROP_ON_AUTH_FAIL_ENA	BIT(5)
#define VIRTCHNL_IPSEC_ARW_CHECK_ENA		BIT(6)
#define VIRTCHNL_IPSEC_24BIT_SPI_ENA		BIT(7)

/* algorithm type */
/* Hash Algorithm */
#define VIRTCHNL_HASH_NO_ALG	0 /* NULL algorithm */
#define VIRTCHNL_AES_CBC_MAC	1 /* AES-CBC-MAC algorithm */
#define VIRTCHNL_AES_CMAC	2 /* AES CMAC algorithm */
#define VIRTCHNL_AES_GMAC	3 /* AES GMAC algorithm */
#define VIRTCHNL_AES_XCBC_MAC	4 /* AES XCBC algorithm */
#define VIRTCHNL_MD5_HMAC	5 /* HMAC using MD5 algorithm */
#define VIRTCHNL_SHA1_HMAC	6 /* HMAC using 128 bit SHA algorithm */
#define VIRTCHNL_SHA224_HMAC	7 /* HMAC using 224 bit SHA algorithm */
#define VIRTCHNL_SHA256_HMAC	8 /* HMAC using 256 bit SHA algorithm */
#define VIRTCHNL_SHA384_HMAC	9 /* HMAC using 384 bit SHA algorithm */
#define VIRTCHNL_SHA512_HMAC	10 /* HMAC using 512 bit SHA algorithm */
#define VIRTCHNL_SHA3_224_HMAC	11 /* HMAC using 224 bit SHA3 algorithm */
#define VIRTCHNL_SHA3_256_HMAC	12 /* HMAC using 256 bit SHA3 algorithm */
#define VIRTCHNL_SHA3_384_HMAC	13 /* HMAC using 384 bit SHA3 algorithm */
#define VIRTCHNL_SHA3_512_HMAC	14 /* HMAC using 512 bit SHA3 algorithm */
/* Cipher Algorithm */
#define VIRTCHNL_CIPHER_NO_ALG	15 /* NULL algorithm */
#define VIRTCHNL_3DES_CBC	16 /* Triple DES algorithm in CBC mode */
#define VIRTCHNL_AES_CBC	17 /* AES algorithm in CBC mode */
#define VIRTCHNL_AES_CTR	18 /* AES algorithm in Counter mode */
/* AEAD Algorithm */
#define VIRTCHNL_AES_CCM	19 /* AES algorithm in CCM mode */
#define VIRTCHNL_AES_GCM	20 /* AES algorithm in GCM mode */
#define VIRTCHNL_CHACHA20_POLY1305 21 /* algorithm of ChaCha20-Poly1305 */

/* protocol type */
#define VIRTCHNL_PROTO_ESP	1
#define VIRTCHNL_PROTO_AH	2
#define VIRTCHNL_PROTO_RSVD1	3

/* sa mode */
#define VIRTCHNL_SA_MODE_TRANSPORT	1
#define VIRTCHNL_SA_MODE_TUNNEL		2
#define VIRTCHNL_SA_MODE_TRAN_TUN	3
#define VIRTCHNL_SA_MODE_UNKNOWN	4

/* sa direction */
#define VIRTCHNL_DIR_INGRESS		1
#define VIRTCHNL_DIR_EGRESS		2
#define VIRTCHNL_DIR_INGRESS_EGRESS	3

/* sa termination */
#define VIRTCHNL_TERM_SOFTWARE	1
#define VIRTCHNL_TERM_HARDWARE	2

/* sa ip type */
#define VIRTCHNL_IPV4	1
#define VIRTCHNL_IPV6	2

/* for virtchnl_ipsec_resp */
enum inline_ipsec_resp {
	INLINE_IPSEC_SUCCESS = 0,
	INLINE_IPSEC_FAIL = -1,
	INLINE_IPSEC_ERR_FIFO_FULL = -2,
	INLINE_IPSEC_ERR_NOT_READY = -3,
	INLINE_IPSEC_ERR_VF_DOWN = -4,
	INLINE_IPSEC_ERR_INVALID_PARAMS = -5,
	INLINE_IPSEC_ERR_NO_MEM = -6,
};

/* Detailed opcodes for DPDK and IPsec use */
enum inline_ipsec_ops {
	INLINE_IPSEC_OP_GET_CAP = 0,
	INLINE_IPSEC_OP_GET_STATUS = 1,
	INLINE_IPSEC_OP_SA_CREATE = 2,
	INLINE_IPSEC_OP_SA_UPDATE = 3,
	INLINE_IPSEC_OP_SA_DESTROY = 4,
	INLINE_IPSEC_OP_SP_CREATE = 5,
	INLINE_IPSEC_OP_SP_DESTROY = 6,
	INLINE_IPSEC_OP_SA_READ = 7,
	INLINE_IPSEC_OP_EVENT = 8,
	INLINE_IPSEC_OP_RESP = 9,
};

#pragma pack(1)
/* Not all valid, if certain field is invalid, set 1 for all bits */
struct virtchnl_algo_cap  {
	u32 algo_type;

	u16 block_size;

	u16 min_key_size;
	u16 max_key_size;
	u16 inc_key_size;

	u16 min_iv_size;
	u16 max_iv_size;
	u16 inc_iv_size;

	u16 min_digest_size;
	u16 max_digest_size;
	u16 inc_digest_size;

	u16 min_aad_size;
	u16 max_aad_size;
	u16 inc_aad_size;
};
#pragma pack()

/* vf record the capability of crypto from the virtchnl */
struct virtchnl_sym_crypto_cap {
	u8 crypto_type;
	u8 algo_cap_num;
	struct virtchnl_algo_cap algo_cap_list[VIRTCHNL_IPSEC_MAX_ALGO_CAP_NUM];
};

/* VIRTCHNL_OP_GET_IPSEC_CAP
 * VF pass virtchnl_ipsec_cap to PF
 * and PF return capability of ipsec from virtchnl.
 */
#pragma pack(1)
struct virtchnl_ipsec_cap {
	/* max number of SA per VF */
	u16 max_sa_num;

	/* IPsec SA Protocol - value ref VIRTCHNL_PROTO_XXX */
	u8 virtchnl_protocol_type;

	/* IPsec SA Mode - value ref VIRTCHNL_SA_MODE_XXX */
	u8 virtchnl_sa_mode;

	/* IPSec SA Direction - value ref VIRTCHNL_DIR_XXX */
	u8 virtchnl_direction;

	/* termination mode - value ref VIRTCHNL_TERM_XXX */
	u8 termination_mode;

	/* number of supported crypto capability */
	u8 crypto_cap_num;

	/* descriptor ID */
	u16 desc_id;

	/* capabilities enabled - value ref VIRTCHNL_IPSEC_XXX_ENA */
	u32 caps_enabled;

	/* crypto capabilities */
	struct virtchnl_sym_crypto_cap cap[VIRTCHNL_IPSEC_MAX_CRYPTO_CAP_NUM];
};

/* configuration of crypto function */
struct virtchnl_ipsec_crypto_cfg_item {
	u8 crypto_type;

	u32 algo_type;

	/* Length of valid IV data. */
	u16 iv_len;

	/* Length of digest */
	u16 digest_len;

	/* SA salt */
	u32 salt;

	/* The length of the symmetric key */
	u16 key_len;

	/* key data buffer */
	u8 key_data[VIRTCHNL_IPSEC_MAX_KEY_LEN];
};
#pragma pack()

struct virtchnl_ipsec_sym_crypto_cfg {
	struct virtchnl_ipsec_crypto_cfg_item
		items[VIRTCHNL_IPSEC_MAX_CRYPTO_ITEM_NUMBER];
};

#pragma pack(1)
/* VIRTCHNL_OP_IPSEC_SA_CREATE
 * VF send this SA configuration to PF using virtchnl;
 * PF create SA as configuration and PF driver will return
 * an unique index (sa_idx) for the created SA.
 */
struct virtchnl_ipsec_sa_cfg {
	/* IPsec SA Protocol - AH/ESP */
	u8 virtchnl_protocol_type;

	/* termination mode - value ref VIRTCHNL_TERM_XXX */
	u8 virtchnl_termination;

	/* type of outer IP - IPv4/IPv6 */
	u8 virtchnl_ip_type;

	/* type of esn - !0:enable/0:disable */
	u8 esn_enabled;

	/* udp encap - !0:enable/0:disable */
	u8 udp_encap_enabled;

	/* IPSec SA Direction - value ref VIRTCHNL_DIR_XXX */
	u8 virtchnl_direction;

	/* reserved */
	u8 reserved1;

	/* SA security parameter index */
	u32 spi;

	/* outer src ip address */
	u8 src_addr[16];

	/* outer dst ip address */
	u8 dst_addr[16];

	/* SPD reference. Used to link an SA with its policy.
	 * PF drivers may ignore this field.
	 */
	u16 spd_ref;

	/* high 32 bits of esn */
	u32 esn_hi;

	/* low 32 bits of esn */
	u32 esn_low;

	/* When enabled, sa_index must be valid */
	u8 sa_index_en;

	/* SA index when sa_index_en is true */
	u32 sa_index;

	/* auditing mode - enable/disable */
	u8 audit_en;

	/* lifetime byte limit - enable/disable
	 * When enabled, byte_limit_hard and byte_limit_soft
	 * must be valid.
	 */
	u8 byte_limit_en;

	/* hard byte limit count */
	u64 byte_limit_hard;

	/* soft byte limit count */
	u64 byte_limit_soft;

	/* drop on authentication failure - enable/disable */
	u8 drop_on_auth_fail_en;

	/* anti-reply window check - enable/disable
	 * When enabled, arw_size must be valid.
	 */
	u8 arw_check_en;

	/* size of arw window, offset by 1. Setting to 0
	 * represents ARW window size of 1. Setting to 127
	 * represents ARW window size of 128
	 */
	u8 arw_size;

	/* no ip offload mode - enable/disable
	 * When enabled, ip type and address must not be valid.
	 */
	u8 no_ip_offload_en;

	/* SA Domain. Used to logical separate an SADB into groups.
	 * PF drivers supporting a single group ignore this field.
	 */
	u16 sa_domain;

	/* crypto configuration */
	struct virtchnl_ipsec_sym_crypto_cfg crypto_cfg;
};
#pragma pack()

/* VIRTCHNL_OP_IPSEC_SA_UPDATE
 * VF send configuration of index of SA to PF
 * PF will update SA according to configuration
 */
struct virtchnl_ipsec_sa_update {
	u32 sa_index; /* SA to update */
	u32 esn_hi; /* high 32 bits of esn */
	u32 esn_low; /* low 32 bits of esn */
};

#pragma pack(1)
/* VIRTCHNL_OP_IPSEC_SA_DESTROY
 * VF send configuration of index of SA to PF
 * PF will destroy SA according to configuration
 * flag bitmap indicate all SA or just selected SA will
 * be destroyed
 */
struct virtchnl_ipsec_sa_destroy {
	/* All zero bitmap indicates all SA will be destroyed.
	 * Non-zero bitmap indicates the selected SA in
	 * array sa_index will be destroyed.
	 */
	u8 flag;

	/* selected SA index */
	u32 sa_index[VIRTCHNL_IPSEC_MAX_SA_DESTROY_NUM];
};

/* VIRTCHNL_OP_IPSEC_SA_READ
 * VF send this SA configuration to PF using virtchnl;
 * PF read SA and will return configuration for the created SA.
 */
struct virtchnl_ipsec_sa_read {
	/* SA valid - invalid/valid */
	u8 valid;

	/* SA active - inactive/active */
	u8 active;

	/* SA SN rollover - not_rollover/rollover */
	u8 sn_rollover;

	/* IPsec SA Protocol - AH/ESP */
	u8 virtchnl_protocol_type;

	/* termination mode - value ref VIRTCHNL_TERM_XXX */
	u8 virtchnl_termination;

	/* auditing mode - enable/disable */
	u8 audit_en;

	/* lifetime byte limit - enable/disable
	 * When set to limit, byte_limit_hard and byte_limit_soft
	 * must be valid.
	 */
	u8 byte_limit_en;

	/* hard byte limit count */
	u64 byte_limit_hard;

	/* soft byte limit count */
	u64 byte_limit_soft;

	/* drop on authentication failure - enable/disable */
	u8 drop_on_auth_fail_en;

	/* anti-replay window check - enable/disable
	 * When set to check, arw_size, arw_top, and arw must be valid
	 */
	u8 arw_check_en;

	/* size of arw window, offset by 1. Setting to 0
	 * represents ARW window size of 1. Setting to 127
	 * represents ARW window size of 128
	 */
	u8 arw_size;

	/* reserved */
	u8 reserved1;

	/* top of anti-replay-window */
	u64 arw_top;

	/* anti-replay-window */
	u8 arw[16];

	/* packets processed  */
	u64 packets_processed;

	/* bytes processed  */
	u64 bytes_processed;

	/* packets dropped  */
	u32 packets_dropped;

	/* authentication failures */
	u32 auth_fails;

	/* ARW check failures */
	u32 arw_fails;

	/* type of esn - enable/disable */
	u8 esn;

	/* IPSec SA Direction - value ref VIRTCHNL_DIR_XXX */
	u8 virtchnl_direction;

	/* SA security parameter index */
	u32 spi;

	/* SA salt */
	u32 salt;

	/* high 32 bits of esn */
	u32 esn_hi;

	/* low 32 bits of esn */
	u32 esn_low;

	/* SA Domain. Used to logical separate an SADB into groups.
	 * PF drivers supporting a single group ignore this field.
	 */
	u16 sa_domain;

	/* SPD reference. Used to link an SA with its policy.
	 * PF drivers may ignore this field.
	 */
	u16 spd_ref;

	/* crypto configuration. Salt and keys are set to 0 */
	struct virtchnl_ipsec_sym_crypto_cfg crypto_cfg;
};
#pragma pack()

/* Add allowlist entry in IES */
struct virtchnl_ipsec_sp_cfg {
	u32 spi;
	u32 dip[4];

	/* Drop frame if true or redirect to QAT if false. */
	u8 drop;

	/* Congestion domain. For future use. */
	u8 cgd;

	/* 0 for IPv4 table, 1 for IPv6 table. */
	u8 table_id;

	/* Set TC (congestion domain) if true. For future use. */
	u8 set_tc;

	/* 0 for NAT-T unsupported, 1 for NAT-T supported */
	u8 is_udp;

	/* reserved */
	u8 reserved;

	/* NAT-T UDP port number. Only valid in case NAT-T supported */
	u16 udp_port;
};

#pragma pack(1)
/* Delete allowlist entry in IES */
struct virtchnl_ipsec_sp_destroy {
	/* 0 for IPv4 table, 1 for IPv6 table. */
	u8 table_id;
	u32 rule_id;
};
#pragma pack()

/* Response from IES to allowlist operations */
struct virtchnl_ipsec_sp_cfg_resp {
	u32 rule_id;
};

struct virtchnl_ipsec_sa_cfg_resp {
	u32 sa_handle;
};

#define INLINE_IPSEC_EVENT_RESET	0x1
#define INLINE_IPSEC_EVENT_CRYPTO_ON	0x2
#define INLINE_IPSEC_EVENT_CRYPTO_OFF	0x4

struct virtchnl_ipsec_event {
	u32 ipsec_event_data;
};

#define INLINE_IPSEC_STATUS_AVAILABLE	0x1
#define INLINE_IPSEC_STATUS_UNAVAILABLE	0x2

struct virtchnl_ipsec_status {
	u32 status;
};

struct virtchnl_ipsec_resp {
	u32 resp;
};

/* Internal message descriptor for VF <-> IPsec communication */
struct inline_ipsec_msg {
	u16 ipsec_opcode;
	u16 req_id;

	union {
		/* IPsec request */
		struct virtchnl_ipsec_sa_cfg sa_cfg[0];
		struct virtchnl_ipsec_sp_cfg sp_cfg[0];
		struct virtchnl_ipsec_sa_update sa_update[0];
		struct virtchnl_ipsec_sa_destroy sa_destroy[0];
		struct virtchnl_ipsec_sp_destroy sp_destroy[0];

		/* IPsec response */
		struct virtchnl_ipsec_sa_cfg_resp sa_cfg_resp[0];
		struct virtchnl_ipsec_sp_cfg_resp sp_cfg_resp[0];
		struct virtchnl_ipsec_cap ipsec_cap[0];
		struct virtchnl_ipsec_status ipsec_status[0];
		/* response to del_sa, del_sp, update_sa */
		struct virtchnl_ipsec_resp ipsec_resp[0];

		/* IPsec event (no req_id is required) */
		struct virtchnl_ipsec_event event[0];

		/* Reserved */
		struct virtchnl_ipsec_sa_read sa_read[0];
	} ipsec_data;
};

static inline u16 virtchnl_inline_ipsec_val_msg_len(u16 opcode)
{
	u16 valid_len = sizeof(struct inline_ipsec_msg);

	switch (opcode) {
	case INLINE_IPSEC_OP_GET_CAP:
	case INLINE_IPSEC_OP_GET_STATUS:
		break;
	case INLINE_IPSEC_OP_SA_CREATE:
		valid_len += sizeof(struct virtchnl_ipsec_sa_cfg);
		break;
	case INLINE_IPSEC_OP_SP_CREATE:
		valid_len += sizeof(struct virtchnl_ipsec_sp_cfg);
		break;
	case INLINE_IPSEC_OP_SA_UPDATE:
		valid_len += sizeof(struct virtchnl_ipsec_sa_update);
		break;
	case INLINE_IPSEC_OP_SA_DESTROY:
		valid_len += sizeof(struct virtchnl_ipsec_sa_destroy);
		break;
	case INLINE_IPSEC_OP_SP_DESTROY:
		valid_len += sizeof(struct virtchnl_ipsec_sp_destroy);
		break;
	/* Only for msg length caculation of response to VF in case of
	 * inline ipsec failure.
	 */
	case INLINE_IPSEC_OP_RESP:
		valid_len += sizeof(struct virtchnl_ipsec_resp);
		break;
	default:
		valid_len = 0;
		break;
	}

	return valid_len;
}

#endif /* _VIRTCHNL_INLINE_IPSEC_H_ */
