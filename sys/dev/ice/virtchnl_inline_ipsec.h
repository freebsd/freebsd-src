/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2020, Intel Corporation
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

#ifndef _VIRTCHNL_INLINE_IPSEC_H_
#define _VIRTCHNL_INLINE_IPSEC_H_

#define VIRTCHNL_IPSEC_MAX_CRYPTO_CAP_NUM	3
#define VIRTCHNL_IPSEC_MAX_ALGO_CAP_NUM		16
#define VIRTCHNL_IPSEC_MAX_TX_DESC_NUM		128
#define VIRTCHNL_IPSEC_MAX_CRYPTO_ITEM_NUMBER	2
#define VIRTCHNL_IPSEC_MAX_KEY_LEN		128
#define VIRTCHNL_IPSEC_MAX_SA_DESTROY_NUM	8
#define VIRTCHNL_IPSEC_SELECTED_SA_DESTROY	0
#define VIRTCHNL_IPSEC_ALL_SA_DESTROY		1

/* crypto type */
#define VIRTCHNL_AUTH		1
#define VIRTCHNL_CIPHER		2
#define VIRTCHNL_AEAD		3

/* algorithm type */
/* Hash Algorithm */
#define VIRTCHNL_NO_ALG		0 /* NULL algorithm */
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
#define VIRTCHNL_3DES_CBC	15 /* Triple DES algorithm in CBC mode */
#define VIRTCHNL_AES_CBC	16 /* AES algorithm in CBC mode */
#define VIRTCHNL_AES_CTR	17 /* AES algorithm in Counter mode */
/* AEAD Algorithm */
#define VIRTCHNL_AES_CCM	18 /* AES algorithm in CCM mode */
#define VIRTCHNL_AES_GCM	19 /* AES algorithm in GCM mode */
#define VIRTCHNL_CHACHA20_POLY1305 20 /* algorithm of ChaCha20-Poly1305 */

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
struct virtchnl_ipsec_cap {
	/* max number of SA per VF */
	u16 max_sa_num;

	/* IPsec SA Protocol - value ref VIRTCHNL_PROTO_XXX */
	u8 virtchnl_protocol_type;

	/* IPsec SA Mode - value ref VIRTCHNL_SA_MODE_XXX */
	u8 virtchnl_sa_mode;

	/* IPSec SA Direction - value ref VIRTCHNL_DIR_XXX */
	u8 virtchnl_direction;

	/* type of esn - !0:enable/0:disable */
	u8 esn_enabled;

	/* type of udp_encap - !0:enable/0:disable */
	u8 udp_encap_enabled;

	/* termination mode - value ref VIRTCHNL_TERM_XXX */
	u8 termination_mode;

	/* SA index mode - !0:enable/0:disable */
	u8 sa_index_sw_enabled;

	/* auditing mode - !0:enable/0:disable */
	u8 audit_enabled;

	/* lifetime byte limit - !0:enable/0:disable */
	u8 byte_limit_enabled;

	/* drop on authentication failure - !0:enable/0:disable */
	u8 drop_on_auth_fail_enabled;

	/* anti-replay window check - !0:enable/0:disable */
	u8 arw_check_enabled;

	/* number of supported crypto capability */
	u8 crypto_cap_num;

	/* descriptor ID */
	u16 desc_id;

	/* crypto capabilities */
	struct virtchnl_sym_crypto_cap cap[VIRTCHNL_IPSEC_MAX_CRYPTO_CAP_NUM];
};

/* using desc_id to record the format of rx descriptor */
struct virtchnl_rx_desc_fmt {
	u16 desc_id;
};

/* using desc_id to record the format of tx descriptor */
struct virtchnl_tx_desc_fmt {
	u8 desc_num;
	u16 desc_ids[VIRTCHNL_IPSEC_MAX_TX_DESC_NUM];
};

/* configuration of crypto function */
struct virtchnl_ipsec_crypto_cfg_item {
	u8 crypto_type;

	u32 algo_type;

	/* Length of valid IV data. */
	u16 iv_len;

	/* Length of digest */
	u16 digest_len;

	/* The length of the symmetric key */
	u16 key_len;

	/* key data buffer */
	u8 key_data[VIRTCHNL_IPSEC_MAX_KEY_LEN];
};

struct virtchnl_ipsec_sym_crypto_cfg {
	struct virtchnl_ipsec_crypto_cfg_item
		items[VIRTCHNL_IPSEC_MAX_CRYPTO_ITEM_NUMBER];
};

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

	/* SA salt */
	u32 salt;

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

/* VIRTCHNL_OP_IPSEC_SA_UPDATE
 * VF send configuration of index of SA to PF
 * PF will update SA according to configuration
 */
struct virtchnl_ipsec_sa_update {
	u32 sa_index; /* SA to update */
	u32 esn_hi; /* high 32 bits of esn */
	u32 esn_low; /* low 32 bits of esn */
};

/* VIRTCHNL_OP_IPSEC_SA_DESTROY
 * VF send configuration of index of SA to PF
 * PF will destroy SA according to configuration
 * flag bitmap indicate all SA or just selected SA will
 * be destroyed
 */
struct virtchnl_ipsec_sa_destroy {
	/* VIRTCHNL_SELECTED_SA_DESTROY: selected SA will be destroyed.
	 * VIRTCHNL_ALL_SA_DESTROY: all SA will be destroyed.
	 */
	u8 flag;

	u8 pad1; /* pading */
	u16 pad2; /* pading */

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

#endif /* _VIRTCHNL_INLINE_IPSEC_H_ */
