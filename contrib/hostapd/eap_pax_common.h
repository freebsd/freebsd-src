/*
 * WPA Supplicant / EAP-PAX shared routines
 * Copyright (c) 2005, Jouni Malinen <jkmaline@cc.hut.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 *
 * See README and COPYING for more details.
 */

#ifndef EAP_PAX_COMMON_H
#define EAP_PAX_COMMON_H

struct eap_pax_hdr {
	u8 code;
	u8 identifier;
	u16 length; /* including code, identifier, and length */
	u8 type; /* EAP_TYPE_PAX */
	u8 op_code;
	u8 flags;
	u8 mac_id;
	u8 dh_group_id;
	u8 public_key_id;
	/* Followed by variable length payload and ICV */
} __attribute__ ((packed));


/* op_code: */
enum {
	EAP_PAX_OP_STD_1 = 0x01,
	EAP_PAX_OP_STD_2 = 0x02,
	EAP_PAX_OP_STD_3 = 0x03,
	EAP_PAX_OP_SEC_1 = 0x11,
	EAP_PAX_OP_SEC_2 = 0x12,
	EAP_PAX_OP_SEC_3 = 0x13,
	EAP_PAX_OP_SEC_4 = 0x14,
	EAP_PAX_OP_SEC_5 = 0x15,
	EAP_PAX_OP_ACK = 0x21
};

/* flags: */
#define EAP_PAX_FLAGS_MF			0x01
#define EAP_PAX_FLAGS_CE			0x02

/* mac_id: */
#define EAP_PAX_MAC_HMAC_SHA1_128		0x01
#define EAP_PAX_MAC_AES_CBC_MAC_128		0x02

/* dh_group_id: */
#define EAP_PAX_DH_GROUP_NONE			0x00
#define EAP_PAX_DH_GROUP_3072_MODP		0x01

/* public_key_id: */
#define EAP_PAX_PUBLIC_KEY_NONE			0x00
#define EAP_PAX_PUBLIC_KEY_RSA_OAEP_2048	0x01


#define EAP_PAX_RAND_LEN 32
#define EAP_PAX_MSK_LEN 64
#define EAP_PAX_MAC_LEN 16
#define EAP_PAX_ICV_LEN 16
#define EAP_PAX_AK_LEN 16
#define EAP_PAX_MK_LEN 16
#define EAP_PAX_CK_LEN 16
#define EAP_PAX_ICK_LEN 16


int eap_pax_kdf(u8 mac_id, const u8 *key, size_t key_len,
		const char *identifier,
		const u8 *entropy, size_t entropy_len,
		size_t output_len, u8 *output);
int eap_pax_mac(u8 mac_id, const u8 *key, size_t key_len,
		const u8 *data1, size_t data1_len,
		const u8 *data2, size_t data2_len,
		const u8 *data3, size_t data3_len,
		u8 *mac);
int eap_pax_initial_key_derivation(u8 mac_id, const u8 *ak, const u8 *e,
				   u8 *mk, u8 *ck, u8 *ick);

#endif /* EAP_PAX_COMMON_H */
