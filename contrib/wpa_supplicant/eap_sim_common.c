/*
 * WPA Supplicant / EAP-SIM/AKA shared routines
 * Copyright (c) 2004-2005, Jouni Malinen <jkmaline@cc.hut.fi>
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "eap_i.h"
#include "sha1.h"
#include "aes_wrap.h"
#include "eap_sim_common.h"


#define MSK_LEN 8
#define EMSK_LEN 8


static void eap_sim_prf(const u8 *key, u8 *x, size_t xlen)
{
	u8 xkey[64];
	u32 t[5], _t[5];
	int i, j, m, k;
	u8 *xpos = x;
	u32 carry;

	/* FIPS 186-2 + change notice 1 */

	memcpy(xkey, key, EAP_SIM_MK_LEN);
	memset(xkey + EAP_SIM_MK_LEN, 0, 64 - EAP_SIM_MK_LEN);
	t[0] = 0x67452301;
	t[1] = 0xEFCDAB89;
	t[2] = 0x98BADCFE;
	t[3] = 0x10325476;
	t[4] = 0xC3D2E1F0;

	m = xlen / 40;
	for (j = 0; j < m; j++) {
		/* XSEED_j = 0 */
		for (i = 0; i < 2; i++) {
			/* XVAL = (XKEY + XSEED_j) mod 2^b */

			/* w_i = G(t, XVAL) */
			memcpy(_t, t, 20);
			sha1_transform((u8 *) _t, xkey);
			_t[0] = host_to_be32(_t[0]);
			_t[1] = host_to_be32(_t[1]);
			_t[2] = host_to_be32(_t[2]);
			_t[3] = host_to_be32(_t[3]);
			_t[4] = host_to_be32(_t[4]);
			memcpy(xpos, _t, 20);

			/* XKEY = (1 + XKEY + w_i) mod 2^b */
			carry = 1;
			for (k = 19; k >= 0; k--) {
				carry += xkey[k] + xpos[k];
				xkey[k] = carry & 0xff;
				carry >>= 8;
			}

			xpos += SHA1_MAC_LEN;
		}
		/* x_j = w_0|w_1 */
	}
}


void eap_sim_derive_keys(const u8 *mk, u8 *k_encr, u8 *k_aut, u8 *msk)
{
	u8 buf[120], *pos;
	eap_sim_prf(mk, buf, 120);
	pos = buf;
	memcpy(k_encr, pos, EAP_SIM_K_ENCR_LEN);
	pos += EAP_SIM_K_ENCR_LEN;
	memcpy(k_aut, pos, EAP_SIM_K_AUT_LEN);
	pos += EAP_SIM_K_AUT_LEN;
	memcpy(msk, pos, EAP_SIM_KEYING_DATA_LEN);
	pos += MSK_LEN;

	wpa_hexdump_key(MSG_DEBUG, "EAP-SIM: K_encr",
			k_encr, EAP_SIM_K_ENCR_LEN);
	wpa_hexdump_key(MSG_DEBUG, "EAP-SIM: K_aut",
			k_aut, EAP_SIM_K_ENCR_LEN);
	wpa_hexdump_key(MSG_DEBUG, "EAP-SIM: MSK",
			msk, MSK_LEN);
	wpa_hexdump_key(MSG_DEBUG, "EAP-SIM: Ext. MSK",
			msk + MSK_LEN, EMSK_LEN);
	wpa_hexdump_key(MSG_DEBUG, "EAP-SIM: keying material",
		    msk, EAP_SIM_KEYING_DATA_LEN);
}


void eap_sim_derive_keys_reauth(unsigned int _counter,
				const u8 *identity, size_t identity_len,
				const u8 *nonce_s, const u8 *mk, u8 *msk)
{
	u8 xkey[SHA1_MAC_LEN];
	u8 counter[2];
	const u8 *addr[4];
	size_t len[4];

	addr[0] = identity;
	len[0] = identity_len;
	addr[1] = counter;
	len[1] = 2;
	addr[2] = nonce_s;
	len[2] = EAP_SIM_NONCE_S_LEN;
	addr[3] = mk;
	len[3] = EAP_SIM_MK_LEN;

	counter[0] = _counter >> 8;
	counter[1] = _counter & 0xff;

	wpa_printf(MSG_DEBUG, "EAP-SIM: Deriving keying data from reauth");
	wpa_hexdump_ascii(MSG_DEBUG, "EAP-SIM: Identity",
			  identity, identity_len);
	wpa_hexdump(MSG_DEBUG, "EAP-SIM: counter", counter, 2);
	wpa_hexdump(MSG_DEBUG, "EAP-SIM: NONCE_S", nonce_s,
		    EAP_SIM_NONCE_S_LEN);
	wpa_hexdump_key(MSG_DEBUG, "EAP-SIM: MK", mk, EAP_SIM_MK_LEN);

	/* XKEY' = SHA1(Identity|counter|NONCE_S|MK) */
	sha1_vector(4, addr, len, xkey);
	wpa_hexdump(MSG_DEBUG, "EAP-SIM: XKEY'", xkey, SHA1_MAC_LEN);

	eap_sim_prf(xkey, msk, EAP_SIM_KEYING_DATA_LEN);
	wpa_hexdump(MSG_DEBUG, "EAP-SIM: MSK", msk, MSK_LEN);
	wpa_hexdump(MSG_DEBUG, "EAP-SIM: Ext. MSK", msk + MSK_LEN, EMSK_LEN);
	wpa_hexdump(MSG_DEBUG, "EAP-SIM: keying material",
		    msk, EAP_SIM_KEYING_DATA_LEN);
}


int eap_sim_verify_mac(const u8 *k_aut, u8 *req, size_t req_len, u8 *mac,
		       u8 *extra, size_t extra_len)
{
	unsigned char hmac[SHA1_MAC_LEN];
	const u8 *addr[2];
	size_t len[2];
	u8 rx_mac[EAP_SIM_MAC_LEN];

	if (mac == NULL)
		return -1;

	addr[0] = req;
	len[0] = req_len;
	addr[1] = extra;
	len[1] = extra_len;

	/* HMAC-SHA1-128 */
	memcpy(rx_mac, mac, EAP_SIM_MAC_LEN);
	memset(mac, 0, EAP_SIM_MAC_LEN);
	hmac_sha1_vector(k_aut, EAP_SIM_K_AUT_LEN, 2, addr, len, hmac);
	memcpy(mac, rx_mac, EAP_SIM_MAC_LEN);

	return (memcmp(hmac, mac, EAP_SIM_MAC_LEN) == 0) ? 0 : 1;
}


void eap_sim_add_mac(const u8 *k_aut, u8 *msg, size_t msg_len, u8 *mac,
		     const u8 *extra, size_t extra_len)
{
	unsigned char hmac[SHA1_MAC_LEN];
	const u8 *addr[2];
	size_t len[2];

	addr[0] = msg;
	len[0] = msg_len;
	addr[1] = extra;
	len[1] = extra_len;

	/* HMAC-SHA1-128 */
	memset(mac, 0, EAP_SIM_MAC_LEN);
	hmac_sha1_vector(k_aut, EAP_SIM_K_AUT_LEN, 2, addr, len, hmac);
	memcpy(mac, hmac, EAP_SIM_MAC_LEN);
}


int eap_sim_parse_attr(u8 *start, u8 *end, struct eap_sim_attrs *attr, int aka,
		       int encr)
{
	u8 *pos = start, *apos;
	size_t alen, plen;
	int list_len, i;

	memset(attr, 0, sizeof(*attr));
	attr->id_req = NO_ID_REQ;
	attr->notification = -1;
	attr->counter = -1;
	attr->selected_version = -1;
	attr->client_error_code = -1;

	while (pos < end) {
		if (pos + 2 > end) {
			wpa_printf(MSG_INFO, "EAP-SIM: Attribute overflow(1)");
			return -1;
		}
		wpa_printf(MSG_MSGDUMP, "EAP-SIM: Attribute: Type=%d Len=%d",
			   pos[0], pos[1] * 4);
		if (pos + pos[1] * 4 > end) {
			wpa_printf(MSG_INFO, "EAP-SIM: Attribute overflow "
				   "(pos=%p len=%d end=%p)",
				   pos, pos[1] * 4, end);
			return -1;
		}
		apos = pos + 2;
		alen = pos[1] * 4 - 2;
		wpa_hexdump(MSG_MSGDUMP, "EAP-SIM: Attribute data",
			    apos, alen);

		switch (pos[0]) {
		case EAP_SIM_AT_RAND:
			wpa_printf(MSG_DEBUG, "EAP-SIM: AT_RAND");
			apos += 2;
			alen -= 2;
			if ((!aka && (alen % GSM_RAND_LEN)) ||
			    (aka && alen != AKA_RAND_LEN)) {
				wpa_printf(MSG_INFO, "EAP-SIM: Invalid AT_RAND"
					   " (len %lu)",
					   (unsigned long) alen);
				return -1;
			}
			attr->rand = apos;
			attr->num_chal = alen / GSM_RAND_LEN;
			break;
		case EAP_SIM_AT_AUTN:
			wpa_printf(MSG_DEBUG, "EAP-AKA: AT_AUTN");
			if (!aka) {
				wpa_printf(MSG_DEBUG, "EAP-SIM: "
					   "Unexpected AT_AUTN");
				return -1;
			}
			apos += 2;
			alen -= 2;
			if (alen != AKA_AUTN_LEN) {
				wpa_printf(MSG_INFO, "EAP-AKA: Invalid AT_AUTN"
					   " (len %lu)",
					   (unsigned long) alen);
				return -1;
			}
			attr->autn = apos;
			break;
		case EAP_SIM_AT_PADDING:
			if (!encr) {
				wpa_printf(MSG_ERROR, "EAP-SIM: Unencrypted "
					   "AT_PADDING");
				return -1;
			}
			wpa_printf(MSG_DEBUG, "EAP-SIM: (encr) AT_PADDING");
			for (i = 2; i < alen; i++) {
				if (apos[i] != 0) {
					wpa_printf(MSG_INFO, "EAP-SIM: (encr) "
						   "AT_PADDING used a non-zero"
						   " padding byte");
					wpa_hexdump(MSG_DEBUG, "EAP-SIM: "
						    "(encr) padding bytes",
						    apos + 2, alen - 2);
					return -1;
				}
			}
			break;
		case EAP_SIM_AT_NONCE_MT:
			wpa_printf(MSG_DEBUG, "EAP-SIM: AT_NONCE_MT");
			if (alen != 2 + EAP_SIM_NONCE_MT_LEN) {
				wpa_printf(MSG_INFO, "EAP-SIM: Invalid "
					   "AT_NONCE_MT length");
				return -1;
			}
			attr->nonce_mt = apos + 2;
			break;
		case EAP_SIM_AT_PERMANENT_ID_REQ:
			wpa_printf(MSG_DEBUG, "EAP-SIM: AT_PERMANENT_ID_REQ");
			attr->id_req = PERMANENT_ID;
			break;
		case EAP_SIM_AT_MAC:
			wpa_printf(MSG_DEBUG, "EAP-SIM: AT_MAC");
			if (alen != 2 + EAP_SIM_MAC_LEN) {
				wpa_printf(MSG_INFO, "EAP-SIM: Invalid AT_MAC "
					   "length");
				return -1;
			}
			attr->mac = apos + 2;
			break;
		case EAP_SIM_AT_NOTIFICATION:
			if (alen != 2) {
				wpa_printf(MSG_INFO, "EAP-SIM: Invalid "
					   "AT_NOTIFICATION length %lu",
					   (unsigned long) alen);
				return -1;
			}
			attr->notification = apos[0] * 256 + apos[1];
			wpa_printf(MSG_DEBUG, "EAP-SIM: AT_NOTIFICATION %d",
				   attr->notification);
			break;
		case EAP_SIM_AT_ANY_ID_REQ:
			wpa_printf(MSG_DEBUG, "EAP-SIM: AT_ANY_ID_REQ");
			attr->id_req = ANY_ID;
			break;
		case EAP_SIM_AT_IDENTITY:
			wpa_printf(MSG_DEBUG, "EAP-SIM: AT_IDENTITY");
			attr->identity = apos + 2;
			attr->identity_len = alen - 2;
			break;
		case EAP_SIM_AT_VERSION_LIST:
			if (aka) {
				wpa_printf(MSG_DEBUG, "EAP-AKA: "
					   "Unexpected AT_VERSION_LIST");
				return -1;
			}
			list_len = apos[0] * 256 + apos[1];
			wpa_printf(MSG_DEBUG, "EAP-SIM: AT_VERSION_LIST");
			if (list_len < 2 || list_len > alen - 2) {
				wpa_printf(MSG_WARNING, "EAP-SIM: Invalid "
					   "AT_VERSION_LIST (list_len=%d "
					   "attr_len=%lu)", list_len,
					   (unsigned long) alen);
				return -1;
			}
			attr->version_list = apos + 2;
			attr->version_list_len = list_len;
			break;
		case EAP_SIM_AT_SELECTED_VERSION:
			wpa_printf(MSG_DEBUG, "EAP-SIM: AT_SELECTED_VERSION");
			if (alen != 2) {
				wpa_printf(MSG_INFO, "EAP-SIM: Invalid "
					   "AT_SELECTED_VERSION length %lu",
					   (unsigned long) alen);
				return -1;
			}
			attr->selected_version = apos[0] * 256 + apos[1];
			wpa_printf(MSG_DEBUG, "EAP-SIM: AT_SELECTED_VERSION "
				   "%d", attr->selected_version);
			break;
		case EAP_SIM_AT_FULLAUTH_ID_REQ:
			wpa_printf(MSG_DEBUG, "EAP-SIM: AT_FULLAUTH_ID_REQ");
			attr->id_req = FULLAUTH_ID;
			break;
		case EAP_SIM_AT_COUNTER:
			if (!encr) {
				wpa_printf(MSG_ERROR, "EAP-SIM: Unencrypted "
					   "AT_COUNTER");
				return -1;
			}
			if (alen != 2) {
				wpa_printf(MSG_INFO, "EAP-SIM: (encr) Invalid "
					   "AT_COUNTER (alen=%lu)",
					   (unsigned long) alen);
				return -1;
			}
			attr->counter = apos[0] * 256 + apos[1];
			wpa_printf(MSG_DEBUG, "EAP-SIM: (encr) AT_COUNTER %d",
				   attr->counter);
			break;
		case EAP_SIM_AT_NONCE_S:
			if (!encr) {
				wpa_printf(MSG_ERROR, "EAP-SIM: Unencrypted "
					   "AT_NONCE_S");
				return -1;
			}
			wpa_printf(MSG_DEBUG, "EAP-SIM: (encr) "
				   "AT_NONCE_S");
			if (alen != 2 + EAP_SIM_NONCE_S_LEN) {
				wpa_printf(MSG_INFO, "EAP-SIM: (encr) Invalid "
					   "AT_NONCE_S (alen=%lu)",
					   (unsigned long) alen);
				return -1;
			}
			attr->nonce_s = apos + 2;
			break;
		case EAP_SIM_AT_CLIENT_ERROR_CODE:
			if (alen != 2) {
				wpa_printf(MSG_INFO, "EAP-SIM: Invalid "
					   "AT_CLIENT_ERROR_CODE length %lu",
					   (unsigned long) alen);
				return -1;
			}
			attr->client_error_code = apos[0] * 256 + apos[1];
			wpa_printf(MSG_DEBUG, "EAP-SIM: AT_CLIENT_ERROR_CODE "
				   "%d", attr->client_error_code);
			break;
		case EAP_SIM_AT_IV:
			wpa_printf(MSG_DEBUG, "EAP-SIM: AT_IV");
			if (alen != 2 + EAP_SIM_MAC_LEN) {
				wpa_printf(MSG_INFO, "EAP-SIM: Invalid AT_IV "
					   "length %lu", (unsigned long) alen);
				return -1;
			}
			attr->iv = apos + 2;
			break;
		case EAP_SIM_AT_ENCR_DATA:
			wpa_printf(MSG_DEBUG, "EAP-SIM: AT_ENCR_DATA");
			attr->encr_data = apos + 2;
			attr->encr_data_len = alen - 2;
			if (attr->encr_data_len % 16) {
				wpa_printf(MSG_INFO, "EAP-SIM: Invalid "
					   "AT_ENCR_DATA length %lu",
					   (unsigned long)
					   attr->encr_data_len);
				return -1;
			}
			break;
		case EAP_SIM_AT_NEXT_PSEUDONYM:
			if (!encr) {
				wpa_printf(MSG_ERROR, "EAP-SIM: Unencrypted "
					   "AT_NEXT_PSEUDONYM");
				return -1;
			}
			wpa_printf(MSG_DEBUG, "EAP-SIM: (encr) "
				   "AT_NEXT_PSEUDONYM");
			plen = apos[0] * 256 + apos[1];
			if (plen > alen - 2) {
				wpa_printf(MSG_INFO, "EAP-SIM: (encr) Invalid"
					   " AT_NEXT_PSEUDONYM (actual"
					   " len %lu, attr len %lu)",
					   (unsigned long) plen,
					   (unsigned long) alen);
				return -1;
			}
			attr->next_pseudonym = pos + 4;
			attr->next_pseudonym_len = plen;
			break;
		case EAP_SIM_AT_NEXT_REAUTH_ID:
			if (!encr) {
				wpa_printf(MSG_ERROR, "EAP-SIM: Unencrypted "
					   "AT_NEXT_REAUTH_ID");
				return -1;
			}
			wpa_printf(MSG_DEBUG, "EAP-SIM: (encr) "
				   "AT_NEXT_REAUTH_ID");
			plen = apos[0] * 256 + apos[1];
			if (plen > alen - 2) {
				wpa_printf(MSG_INFO, "EAP-SIM: (encr) Invalid"
					   " AT_NEXT_REAUTH_ID (actual"
					   " len %lu, attr len %lu)",
					   (unsigned long) plen,
					   (unsigned long) alen);
				return -1;
			}
			attr->next_reauth_id = pos + 4;
			attr->next_reauth_id_len = plen;
			break;
		default:
			if (pos[0] < 128) {
				wpa_printf(MSG_INFO, "EAP-SIM: Unrecognized "
					   "non-skippable attribute %d",
					   pos[0]);
				return -1;
			}

			wpa_printf(MSG_DEBUG, "EAP-SIM: Unrecognized skippable"
				   " attribute %d ignored", pos[0]);
			break;
		}

		pos += pos[1] * 4;
	}

	wpa_printf(MSG_DEBUG, "EAP-SIM: Attributes parsed successfully "
		   "(aka=%d encr=%d)", aka, encr);

	return 0;
}


int eap_sim_parse_encr(const u8 *k_encr, u8 *encr_data, size_t encr_data_len,
		       const u8 *iv, struct eap_sim_attrs *attr, int aka)
{
	if (!iv) {
		wpa_printf(MSG_INFO, "EAP-SIM: Encrypted data, but no IV");
		return -1;
	}
	aes_128_cbc_decrypt(k_encr, iv, encr_data, encr_data_len);
	wpa_hexdump(MSG_MSGDUMP, "EAP-SIM: Decrypted AT_ENCR_DATA",
		    encr_data, encr_data_len);

	if (eap_sim_parse_attr(encr_data, encr_data + encr_data_len, attr,
			       aka, 1)) {
		wpa_printf(MSG_INFO, "EAP-SIM: (encr) Failed to parse "
			   "decrypted AT_ENCR_DATA");
		return -1;
	}

	return 0;
}


#define EAP_SIM_INIT_LEN 128

struct eap_sim_msg {
	u8 *buf;
	size_t buf_len, used;
	size_t mac, iv, encr; /* index from buf */
};


struct eap_sim_msg * eap_sim_msg_init(int code, int id, int type, int subtype)
{
	struct eap_sim_msg *msg;
	struct eap_hdr *eap;
	u8 *pos;

	msg = malloc(sizeof(*msg));
	if (msg == NULL)
		return NULL;
	memset(msg, 0, sizeof(*msg));

	msg->buf = malloc(EAP_SIM_INIT_LEN);
	if (msg->buf == NULL) {
		free(msg);
		return NULL;
	}
	memset(msg->buf, 0, EAP_SIM_INIT_LEN);
	msg->buf_len = EAP_SIM_INIT_LEN;
	eap = (struct eap_hdr *) msg->buf;
	eap->code = code;
	eap->identifier = id;
	msg->used = sizeof(*eap);

	pos = (u8 *) (eap + 1);
	*pos++ = type;
	*pos++ = subtype;
	*pos++ = 0; /* Reserved */
	*pos++ = 0; /* Reserved */
	msg->used += 4;

	return msg;
}


u8 * eap_sim_msg_finish(struct eap_sim_msg *msg, size_t *len, const u8 *k_aut,
			const u8 *extra, size_t extra_len)
{
	struct eap_hdr *eap;
	u8 *buf;

	if (msg == NULL)
		return NULL;

	eap = (struct eap_hdr *) msg->buf;
	eap->length = host_to_be16(msg->used);

	if (k_aut && msg->mac) {
		eap_sim_add_mac(k_aut, msg->buf, msg->used,
				msg->buf + msg->mac, extra, extra_len);
	}

	*len = msg->used;
	buf = msg->buf;
	free(msg);
	return buf;
}


void eap_sim_msg_free(struct eap_sim_msg *msg)
{
	if (msg) {
		free(msg->buf);
		free(msg);
	}
}


static int eap_sim_msg_resize(struct eap_sim_msg *msg, size_t add_len)
{
	if (msg->used + add_len > msg->buf_len) {
		u8 *nbuf = realloc(msg->buf, msg->used + add_len);
		if (nbuf == NULL)
			return -1;
		msg->buf = nbuf;
		msg->buf_len = msg->used + add_len;
	}
	return 0;
}


u8 * eap_sim_msg_add_full(struct eap_sim_msg *msg, u8 attr,
			  const u8 *data, size_t len)
{
	int attr_len = 2 + len;
	int pad_len;
	u8 *start, *pos;

	if (msg == NULL)
		return NULL;

	pad_len = (4 - attr_len % 4) % 4;
	attr_len += pad_len;
	if (eap_sim_msg_resize(msg, attr_len))
		return NULL;
	start = pos = msg->buf + msg->used;
	*pos++ = attr;
	*pos++ = attr_len / 4;
	memcpy(pos, data, len);
	if (pad_len) {
		pos += len;
		memset(pos, 0, pad_len);
	}
	msg->used += attr_len;
	return start;
}


u8 * eap_sim_msg_add(struct eap_sim_msg *msg, u8 attr, u16 value,
		     const u8 *data, size_t len)
{
	int attr_len = 4 + len;
	int pad_len;
	u8 *start, *pos;

	if (msg == NULL)
		return NULL;

	pad_len = (4 - attr_len % 4) % 4;
	attr_len += pad_len;
	if (eap_sim_msg_resize(msg, attr_len))
		return NULL;
	start = pos = msg->buf + msg->used;
	*pos++ = attr;
	*pos++ = attr_len / 4;
	*pos++ = value >> 8;
	*pos++ = value & 0xff;
	if (data)
		memcpy(pos, data, len);
	if (pad_len) {
		pos += len;
		memset(pos, 0, pad_len);
	}
	msg->used += attr_len;
	return start;
}


u8 * eap_sim_msg_add_mac(struct eap_sim_msg *msg, u8 attr)
{
	u8 *pos = eap_sim_msg_add(msg, attr, 0, NULL, EAP_SIM_MAC_LEN);
	if (pos)
		msg->mac = (pos - msg->buf) + 4;
	return pos;
}


int eap_sim_msg_add_encr_start(struct eap_sim_msg *msg, u8 attr_iv,
			       u8 attr_encr)
{
	u8 *pos = eap_sim_msg_add(msg, attr_iv, 0, NULL, EAP_SIM_IV_LEN);
	if (pos == NULL)
		return -1;
	msg->iv = (pos - msg->buf) + 4;
	if (hostapd_get_rand(msg->buf + msg->iv, EAP_SIM_IV_LEN)) {
		msg->iv = 0;
		return -1;
	}

	pos = eap_sim_msg_add(msg, attr_encr, 0, NULL, 0);
	if (pos == NULL) {
		msg->iv = 0;
		return -1;
	}
	msg->encr = pos - msg->buf;

	return 0;
}


int eap_sim_msg_add_encr_end(struct eap_sim_msg *msg, u8 *k_encr, int attr_pad)
{
	size_t encr_len;

	if (k_encr == NULL || msg->iv == 0 || msg->encr == 0)
		return -1;

	encr_len = msg->used - msg->encr - 4;
	if (encr_len % 16) {
		u8 *pos;
		int pad_len = 16 - (encr_len % 16);
		if (pad_len < 4) {
			wpa_printf(MSG_WARNING, "EAP-SIM: "
				   "eap_sim_msg_add_encr_end - invalid pad_len"
				   " %d", pad_len);
			return -1;
		}
		wpa_printf(MSG_DEBUG, "   *AT_PADDING");
		pos = eap_sim_msg_add(msg, attr_pad, 0, NULL, pad_len - 4);
		if (pos == NULL)
			return -1;
		memset(pos + 4, 0, pad_len - 4);
		encr_len += pad_len;
	}
	wpa_printf(MSG_DEBUG, "   (AT_ENCR_DATA data len %lu)",
		   (unsigned long) encr_len);
	msg->buf[msg->encr + 1] = encr_len / 4 + 1;
	aes_128_cbc_encrypt(k_encr, msg->buf + msg->iv,
			    msg->buf + msg->encr + 4, encr_len);

	return 0;
}


void eap_sim_report_notification(void *msg_ctx, int notification, int aka)
{
	const char *type = aka ? "AKA" : "SIM";

	switch (notification) {
	case EAP_SIM_GENERAL_FAILURE_AFTER_AUTH:
		wpa_printf(MSG_WARNING, "EAP-%s: General failure "
			   "notification (after authentication)", type);
		break;
	case EAP_SIM_TEMPORARILY_DENIED:
		wpa_printf(MSG_WARNING, "EAP-%s: Failure notification: "
			   "User has been temporarily denied access to the "
			   "requested service", type);
		break;
	case EAP_SIM_NOT_SUBSCRIBED:
		wpa_printf(MSG_WARNING, "EAP-%s: Failure notification: "
			   "User has not subscribed to the requested service",
			   type);
		break;
	case EAP_SIM_GENERAL_FAILURE_BEFORE_AUTH:
		wpa_printf(MSG_WARNING, "EAP-%s: General failure "
			   "notification (before authentication)", type);
		break;
	case EAP_SIM_SUCCESS:
		wpa_printf(MSG_INFO, "EAP-%s: Successful authentication "
			   "notification", type);
		break;
	default:
		if (notification >= 32768) {
			wpa_printf(MSG_INFO, "EAP-%s: Unrecognized "
				   "non-failure notification %d",
				   type, notification);
		} else {
			wpa_printf(MSG_WARNING, "EAP-%s: Unrecognized "
				   "failure notification %d",
				   type, notification);
		}
	}
}


#ifdef TEST_MAIN_EAP_SIM_COMMON
static int test_eap_sim_prf(void)
{
	/* http://csrc.nist.gov/encryption/dss/Examples-1024bit.pdf */
	u8 xkey[] = {
		0xbd, 0x02, 0x9b, 0xbe, 0x7f, 0x51, 0x96, 0x0b,
		0xcf, 0x9e, 0xdb, 0x2b, 0x61, 0xf0, 0x6f, 0x0f,
		0xeb, 0x5a, 0x38, 0xb6
	};
	u8 w[] = {
		0x20, 0x70, 0xb3, 0x22, 0x3d, 0xba, 0x37, 0x2f,
		0xde, 0x1c, 0x0f, 0xfc, 0x7b, 0x2e, 0x3b, 0x49,
		0x8b, 0x26, 0x06, 0x14, 0x3c, 0x6c, 0x18, 0xba,
		0xcb, 0x0f, 0x6c, 0x55, 0xba, 0xbb, 0x13, 0x78,
		0x8e, 0x20, 0xd7, 0x37, 0xa3, 0x27, 0x51, 0x16
	};
	u8 buf[40];

	printf("Testing EAP-SIM PRF (FIPS 186-2 + change notice 1)\n");
	eap_sim_prf(xkey, buf, sizeof(buf));
	if (memcmp(w, buf, sizeof(w) != 0)) {
		printf("eap_sim_prf failed\n");
		return 1;
	}

	return 0;
}


int main(int argc, char *argv[])
{
	int errors = 0;

	errors += test_eap_sim_prf();

	return errors;
}
#endif /* TEST_MAIN_EAP_SIM_COMMON */
