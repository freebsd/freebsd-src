/*
 * EAP-TEAP common helper functions (RFC 7170)
 * Copyright (c) 2008-2019, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "crypto/sha1.h"
#include "crypto/sha256.h"
#include "crypto/sha384.h"
#include "crypto/tls.h"
#include "eap_defs.h"
#include "eap_teap_common.h"


static int tls_cipher_suite_mac_sha384(u16 cs);


void eap_teap_put_tlv_hdr(struct wpabuf *buf, u16 type, u16 len)
{
	struct teap_tlv_hdr hdr;

	hdr.tlv_type = host_to_be16(type);
	hdr.length = host_to_be16(len);
	wpabuf_put_data(buf, &hdr, sizeof(hdr));
}


void eap_teap_put_tlv(struct wpabuf *buf, u16 type, const void *data, u16 len)
{
	eap_teap_put_tlv_hdr(buf, type, len);
	wpabuf_put_data(buf, data, len);
}


void eap_teap_put_tlv_buf(struct wpabuf *buf, u16 type,
			  const struct wpabuf *data)
{
	eap_teap_put_tlv_hdr(buf, type, wpabuf_len(data));
	wpabuf_put_buf(buf, data);
}


struct wpabuf * eap_teap_tlv_eap_payload(struct wpabuf *buf)
{
	struct wpabuf *e;

	if (!buf)
		return NULL;

	/* Encapsulate EAP packet in EAP-Payload TLV */
	wpa_printf(MSG_DEBUG, "EAP-TEAP: Add EAP-Payload TLV");
	e = wpabuf_alloc(sizeof(struct teap_tlv_hdr) + wpabuf_len(buf));
	if (!e) {
		wpa_printf(MSG_ERROR,
			   "EAP-TEAP: Failed to allocate memory for TLV encapsulation");
		wpabuf_free(buf);
		return NULL;
	}
	eap_teap_put_tlv_buf(e, TEAP_TLV_MANDATORY | TEAP_TLV_EAP_PAYLOAD, buf);
	wpabuf_free(buf);

	/* TODO: followed by optional TLVs associated with the EAP packet */

	return e;
}


static int eap_teap_tls_prf(u16 tls_cs, const u8 *secret, size_t secret_len,
			    const char *label, const u8 *seed, size_t seed_len,
			    u8 *out, size_t outlen)
{
	/* TODO: TLS-PRF for TLSv1.3 */
	if (tls_cipher_suite_mac_sha384(tls_cs))
		return tls_prf_sha384(secret, secret_len, label, seed, seed_len,
				      out, outlen);
	return tls_prf_sha256(secret, secret_len, label, seed, seed_len,
			      out, outlen);
}


int eap_teap_derive_eap_msk(u16 tls_cs, const u8 *simck, u8 *msk)
{
	/*
	 * RFC 7170, Section 5.4: EAP Master Session Key Generation
	 * MSK = TLS-PRF(S-IMCK[j], "Session Key Generating Function", 64)
	 */

	if (eap_teap_tls_prf(tls_cs, simck, EAP_TEAP_SIMCK_LEN,
			     "Session Key Generating Function", (u8 *) "", 0,
			     msk, EAP_TEAP_KEY_LEN) < 0)
		return -1;
	wpa_hexdump_key(MSG_DEBUG, "EAP-TEAP: Derived key (MSK)",
			msk, EAP_TEAP_KEY_LEN);
	return 0;
}


int eap_teap_derive_eap_emsk(u16 tls_cs, const u8 *simck, u8 *emsk)
{
	/*
	 * RFC 7170, Section 5.4: EAP Master Session Key Generation
	 * EMSK = TLS-PRF(S-IMCK[j],
	 *        "Extended Session Key Generating Function", 64)
	 */

	if (eap_teap_tls_prf(tls_cs, simck, EAP_TEAP_SIMCK_LEN,
			     "Extended Session Key Generating Function",
			     (u8 *) "", 0, emsk, EAP_EMSK_LEN) < 0)
		return -1;
	wpa_hexdump_key(MSG_DEBUG, "EAP-TEAP: Derived key (EMSK)",
			emsk, EAP_EMSK_LEN);
	return 0;
}


int eap_teap_derive_cmk_basic_pw_auth(u16 tls_cs, const u8 *s_imck_msk, u8 *cmk)
{
	u8 imsk[32], imck[EAP_TEAP_IMCK_LEN];
	int res;

	/* FIX: The Basic-Password-Auth (i.e., no inner EAP) case is
	 * not fully defined in RFC 7170, so this CMK derivation may
	 * need to be changed if a fixed definition is eventually
	 * published. For now, derive CMK[0] based on S-IMCK[0] and
	 * IMSK of 32 octets of zeros. */
	os_memset(imsk, 0, 32);
	res = eap_teap_tls_prf(tls_cs, s_imck_msk, EAP_TEAP_SIMCK_LEN,
			       "Inner Methods Compound Keys",
			       imsk, 32, imck, sizeof(imck));
	if (res < 0)
		return -1;
	os_memcpy(cmk, &imck[EAP_TEAP_SIMCK_LEN], EAP_TEAP_CMK_LEN);
	wpa_hexdump_key(MSG_DEBUG, "EAP-TEAP: CMK[no-inner-EAP]",
			cmk, EAP_TEAP_CMK_LEN);
	forced_memzero(imck, sizeof(imck));
	return 0;
}


int eap_teap_derive_imck(u16 tls_cs,
			 const u8 *prev_s_imck_msk, const u8 *prev_s_imck_emsk,
			 const u8 *msk, size_t msk_len,
			 const u8 *emsk, size_t emsk_len,
			 u8 *s_imck_msk, u8 *cmk_msk,
			 u8 *s_imck_emsk, u8 *cmk_emsk)
{
	u8 imsk[64], imck[EAP_TEAP_IMCK_LEN];
	int res;

	/*
	 * RFC 7170, Section 5.2:
	 * IMSK = First 32 octets of TLS-PRF(EMSK, "TEAPbindkey@ietf.org" |
	 *                                   "\0" | 64)
	 * (if EMSK is not available, MSK is used instead; if neither is
	 * available, IMSK is 32 octets of zeros; MSK is truncated to 32 octets
	 * or padded to 32 octets, if needed)
	 * (64 is encoded as a 2-octet field in network byte order)
	 *
	 * S-IMCK[0] = session_key_seed
	 * IMCK[j] = TLS-PRF(S-IMCK[j-1], "Inner Methods Compound Keys",
	 *                   IMSK[j], 60)
	 * S-IMCK[j] = first 40 octets of IMCK[j]
	 * CMK[j] = last 20 octets of IMCK[j]
	 */

	wpa_hexdump_key(MSG_DEBUG, "EAP-TEAP: MSK[j]", msk, msk_len);
	wpa_hexdump_key(MSG_DEBUG, "EAP-TEAP: EMSK[j]", emsk, emsk_len);

	if (emsk && emsk_len > 0) {
		u8 context[3];

		context[0] = 0;
		context[1] = 0;
		context[2] = 64;
		if (eap_teap_tls_prf(tls_cs, emsk, emsk_len,
				     "TEAPbindkey@ietf.org",
				     context, sizeof(context), imsk, 64) < 0)
			return -1;

		wpa_hexdump_key(MSG_DEBUG, "EAP-TEAP: IMSK from EMSK",
				imsk, 32);

		res = eap_teap_tls_prf(tls_cs,
				       prev_s_imck_emsk, EAP_TEAP_SIMCK_LEN,
				       "Inner Methods Compound Keys",
				       imsk, 32, imck, EAP_TEAP_IMCK_LEN);
		forced_memzero(imsk, sizeof(imsk));
		if (res < 0)
			return -1;

		os_memcpy(s_imck_emsk, imck, EAP_TEAP_SIMCK_LEN);
		wpa_hexdump_key(MSG_DEBUG, "EAP-TEAP: EMSK S-IMCK[j]",
				s_imck_emsk, EAP_TEAP_SIMCK_LEN);
		os_memcpy(cmk_emsk, &imck[EAP_TEAP_SIMCK_LEN],
			  EAP_TEAP_CMK_LEN);
		wpa_hexdump_key(MSG_DEBUG, "EAP-TEAP: EMSK CMK[j]",
				cmk_emsk, EAP_TEAP_CMK_LEN);
		forced_memzero(imck, EAP_TEAP_IMCK_LEN);
	}

	if (msk && msk_len > 0) {
		size_t copy_len = msk_len;

		os_memset(imsk, 0, 32); /* zero pad, if needed */
		if (copy_len > 32)
			copy_len = 32;
		os_memcpy(imsk, msk, copy_len);
		wpa_hexdump_key(MSG_DEBUG, "EAP-TEAP: IMSK from MSK", imsk, 32);
	} else {
		os_memset(imsk, 0, 32);
		wpa_hexdump_key(MSG_DEBUG, "EAP-TEAP: Zero IMSK", imsk, 32);
	}

	res = eap_teap_tls_prf(tls_cs, prev_s_imck_msk, EAP_TEAP_SIMCK_LEN,
			       "Inner Methods Compound Keys",
			       imsk, 32, imck, EAP_TEAP_IMCK_LEN);
	forced_memzero(imsk, sizeof(imsk));
	if (res < 0)
		return -1;

	os_memcpy(s_imck_msk, imck, EAP_TEAP_SIMCK_LEN);
	wpa_hexdump_key(MSG_DEBUG, "EAP-TEAP: MSK S-IMCK[j]",
			s_imck_msk, EAP_TEAP_SIMCK_LEN);
	os_memcpy(cmk_msk, &imck[EAP_TEAP_SIMCK_LEN], EAP_TEAP_CMK_LEN);
	wpa_hexdump_key(MSG_DEBUG, "EAP-TEAP: MSK CMK[j]",
			cmk_msk, EAP_TEAP_CMK_LEN);
	forced_memzero(imck, EAP_TEAP_IMCK_LEN);

	return 0;
}


static int tls_cipher_suite_match(const u16 *list, size_t count, u16 cs)
{
	size_t i;

	for (i = 0; i < count; i++) {
		if (list[i] == cs)
			return 1;
	}

	return 0;
}


static int tls_cipher_suite_mac_sha1(u16 cs)
{
	static const u16 sha1_cs[] = {
		0x0005, 0x0007, 0x000a, 0x000d, 0x0010, 0x0013, 0x0016, 0x001b,
		0x002f, 0x0030, 0x0031, 0x0032, 0x0033, 0x0034, 0x0035, 0x0036,
		0x0037, 0x0038, 0x0039, 0x003a, 0x0041, 0x0042, 0x0043, 0x0044,
		0x0045, 0x0046, 0x0084, 0x0085, 0x0086, 0x0087, 0x0088, 0x0089,
		0x008a, 0x008b, 0x008c, 0x008d, 0x008e, 0x008f, 0x0090, 0x0091,
		0x0092, 0x0093, 0x0094, 0x0095, 0x0096, 0x0097, 0x0098, 0x0099,
		0x009a, 0x009b,
		0xc002, 0xc003, 0xc004, 0xc005, 0xc007, 0xc008, 0xc009, 0xc009,
		0xc00a, 0xc00c, 0xc00d, 0xc00e, 0xc00f, 0xc011, 0xc012, 0xc013,
		0xc014, 0xc016, 0xc017, 0xc018, 0xc019, 0xc01a, 0xc01b, 0xc01c,
		0xc014, 0xc01e, 0xc01f, 0xc020, 0xc021, 0xc022, 0xc033, 0xc034,
		0xc035, 0xc036
	};

	return tls_cipher_suite_match(sha1_cs, ARRAY_SIZE(sha1_cs), cs);
}


static int tls_cipher_suite_mac_sha256(u16 cs)
{
	static const u16 sha256_cs[] = {
		0x003c, 0x003d, 0x003e, 0x003f, 0x0040, 0x0067, 0x0068, 0x0069,
		0x006a, 0x006b, 0x006c, 0x006d, 0x009c, 0x009e, 0x00a0, 0x00a2,
		0x00a4, 0x00a6, 0x00a8, 0x00aa, 0x00ac, 0x00ae, 0x00b2, 0x00b6,
		0x00ba, 0x00bb, 0x00bc, 0x00bd, 0x00be, 0x00bd, 0x00be, 0x00be,
		0x00bf, 0x00bf, 0x00c0, 0x00c1, 0x00c2, 0x00c3, 0x00c4, 0x00c5,
		0x1301, 0x1303, 0x1304, 0x1305,
		0xc023, 0xc025, 0xc027, 0xc029, 0xc02b, 0xc02d, 0xc02f, 0xc031,
		0xc037, 0xc03c, 0xc03e, 0xc040, 0xc040, 0xc042, 0xc044, 0xc046,
		0xc048, 0xc04a, 0xc04c, 0xc04e, 0xc050, 0xc052, 0xc054, 0xc056,
		0xc058, 0xc05a, 0xc05c, 0xc05e, 0xc060, 0xc062, 0xc064, 0xc066,
		0xc068, 0xc06a, 0xc06c, 0xc06e, 0xc070, 0xc072, 0xc074, 0xc076,
		0xc078, 0xc07a, 0xc07c, 0xc07e, 0xc080, 0xc082, 0xc084, 0xc086,
		0xc088, 0xc08a, 0xc08c, 0xc08e, 0xc090, 0xc092, 0xc094, 0xc096,
		0xc098, 0xc09a, 0xc0b0, 0xc0b2, 0xc0b4,
		0xcca8, 0xcca9, 0xccaa, 0xccab, 0xccac, 0xccad, 0xccae,
		0xd001, 0xd003, 0xd005
	};

	return tls_cipher_suite_match(sha256_cs, ARRAY_SIZE(sha256_cs), cs);
}


static int tls_cipher_suite_mac_sha384(u16 cs)
{
	static const u16 sha384_cs[] = {
		0x009d, 0x009f, 0x00a1, 0x00a3, 0x00a5, 0x00a7, 0x00a9, 0x00ab,
		0x00ad, 0x00af, 0x00b3, 0x00b7, 0x1302,
		0xc024, 0xc026, 0xc028, 0xc02a, 0xc02c, 0xc02e, 0xc030, 0xc032,
		0xc038, 0xc03d, 0xc03f, 0xc041, 0xc043, 0xc045, 0xc047, 0xc049,
		0xc04b, 0xc04d, 0xc04f, 0xc051, 0xc053, 0xc055, 0xc057, 0xc059,
		0xc05b, 0xc05d, 0xc05f, 0xc061, 0xc063, 0xc065, 0xc067, 0xc069,
		0xc06b, 0xc06d, 0xc06f, 0xc071, 0xc073, 0xc075, 0xc077, 0xc079,
		0xc07b, 0xc07d, 0xc07f, 0xc081, 0xc083, 0xc085, 0xc087, 0xc089,
		0xc08b, 0xc08d, 0xc08f, 0xc091, 0xc093, 0xc095, 0xc097, 0xc099,
		0xc09b, 0xc0b1, 0xc0b3, 0xc0b5,
		0xd002
	};

	return tls_cipher_suite_match(sha384_cs, ARRAY_SIZE(sha384_cs), cs);
}


static int eap_teap_tls_mac(u16 tls_cs, const u8 *cmk, size_t cmk_len,
			    const u8 *buffer, size_t buffer_len,
			    u8 *mac, size_t mac_len)
{
	int res;
	u8 tmp[48];

	os_memset(tmp, 0, sizeof(tmp));
	os_memset(mac, 0, mac_len);

	if (tls_cipher_suite_mac_sha1(tls_cs)) {
		wpa_printf(MSG_DEBUG, "EAP-TEAP: MAC algorithm: HMAC-SHA1");
		res = hmac_sha1(cmk, cmk_len, buffer, buffer_len, tmp);
	} else if (tls_cipher_suite_mac_sha256(tls_cs)) {
		wpa_printf(MSG_DEBUG, "EAP-TEAP: MAC algorithm: HMAC-SHA256");
		res = hmac_sha256(cmk, cmk_len, buffer, buffer_len, tmp);
	} else if (tls_cipher_suite_mac_sha384(tls_cs)) {
		wpa_printf(MSG_DEBUG, "EAP-TEAP: MAC algorithm: HMAC-SHA384");
		res = hmac_sha384(cmk, cmk_len, buffer, buffer_len, tmp);
	} else {
		wpa_printf(MSG_INFO,
			   "EAP-TEAP: Unsupported TLS cipher suite 0x%04x",
			   tls_cs);
		res = -1;
	}
	if (res < 0)
		return res;

	/* FIX: RFC 7170 does not describe how to handle truncation of the
	 * Compound MAC or if the fields are supposed to be of variable length
	 * based on the negotiated TLS cipher suite (they are defined as having
	 * fixed size of 20 octets in the TLV description) */
	if (mac_len > sizeof(tmp))
		mac_len = sizeof(tmp);
	os_memcpy(mac, tmp, mac_len);
	return 0;
}


int eap_teap_compound_mac(u16 tls_cs, const struct teap_tlv_crypto_binding *cb,
			  const struct wpabuf *server_outer_tlvs,
			  const struct wpabuf *peer_outer_tlvs,
			  const u8 *cmk, u8 *compound_mac)
{
	u8 *pos, *buffer;
	size_t bind_len, buffer_len;
	struct teap_tlv_crypto_binding *tmp_cb;
	int res;

	/* RFC 7170, Section 5.3 */
	bind_len = sizeof(struct teap_tlv_hdr) + be_to_host16(cb->length);
	buffer_len = bind_len + 1;
	if (server_outer_tlvs)
		buffer_len += wpabuf_len(server_outer_tlvs);
	if (peer_outer_tlvs)
		buffer_len += wpabuf_len(peer_outer_tlvs);
	buffer = os_malloc(buffer_len);
	if (!buffer)
		return -1;

	pos = buffer;
	/* 1. The entire Crypto-Binding TLV attribute with both the EMSK and MSK
	 * Compound MAC fields zeroed out. */
	os_memcpy(pos, cb, bind_len);
	pos += bind_len;
	tmp_cb = (struct teap_tlv_crypto_binding *) buffer;
	os_memset(tmp_cb->emsk_compound_mac, 0, EAP_TEAP_COMPOUND_MAC_LEN);
	os_memset(tmp_cb->msk_compound_mac, 0, EAP_TEAP_COMPOUND_MAC_LEN);

	/* 2. The EAP Type sent by the other party in the first TEAP message. */
	/* This is supposed to be the EAP Type sent by the other party in the
	 * first TEAP message, but since we cannot get here without having
	 * successfully negotiated use of TEAP, this can only be the fixed EAP
	 * Type of TEAP. */
	*pos++ = EAP_TYPE_TEAP;

	/* 3. All the Outer TLVs from the first TEAP message sent by EAP server
	 * to peer. */
	if (server_outer_tlvs) {
		os_memcpy(pos, wpabuf_head(server_outer_tlvs),
			  wpabuf_len(server_outer_tlvs));
		pos += wpabuf_len(server_outer_tlvs);
	}

	/* 4. All the Outer TLVs from the first TEAP message sent by the peer to
	 * the EAP server. */
	if (peer_outer_tlvs) {
		os_memcpy(pos, wpabuf_head(peer_outer_tlvs),
			  wpabuf_len(peer_outer_tlvs));
		pos += wpabuf_len(peer_outer_tlvs);
	}

	buffer_len = pos - buffer;

	wpa_hexdump_key(MSG_MSGDUMP,
			"EAP-TEAP: CMK for Compound MAC calculation",
			cmk, EAP_TEAP_CMK_LEN);
	wpa_hexdump(MSG_MSGDUMP,
		    "EAP-TEAP: BUFFER for Compound MAC calculation",
		    buffer, buffer_len);
	res = eap_teap_tls_mac(tls_cs, cmk, EAP_TEAP_CMK_LEN,
			       buffer, buffer_len,
			       compound_mac, EAP_TEAP_COMPOUND_MAC_LEN);
	os_free(buffer);

	return res;
}


int eap_teap_parse_tlv(struct eap_teap_tlv_parse *tlv,
		       int tlv_type, u8 *pos, size_t len)
{
	switch (tlv_type) {
	case TEAP_TLV_IDENTITY_TYPE:
		if (len < 2) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: Too short Identity-Type TLV");
			tlv->result = TEAP_STATUS_FAILURE;
			break;
		}
		tlv->identity_type = WPA_GET_BE16(pos);
		wpa_printf(MSG_DEBUG, "EAP-TEAP: Identity-Type: %u",
			   tlv->identity_type);
		break;
	case TEAP_TLV_RESULT:
		wpa_hexdump(MSG_MSGDUMP, "EAP-TEAP: Result TLV", pos, len);
		if (tlv->result) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: More than one Result TLV in the message");
			tlv->result = TEAP_STATUS_FAILURE;
			return -2;
		}
		if (len < 2) {
			wpa_printf(MSG_INFO, "EAP-TEAP: Too short Result TLV");
			tlv->result = TEAP_STATUS_FAILURE;
			break;
		}
		tlv->result = WPA_GET_BE16(pos);
		if (tlv->result != TEAP_STATUS_SUCCESS &&
		    tlv->result != TEAP_STATUS_FAILURE) {
			wpa_printf(MSG_INFO, "EAP-TEAP: Unknown Result %d",
				   tlv->result);
			tlv->result = TEAP_STATUS_FAILURE;
		}
		wpa_printf(MSG_DEBUG, "EAP-TEAP: Result: %s",
			   tlv->result == TEAP_STATUS_SUCCESS ?
			   "Success" : "Failure");
		break;
	case TEAP_TLV_NAK:
		wpa_hexdump(MSG_MSGDUMP, "EAP-TEAP: NAK TLV", pos, len);
		if (len < 6) {
			wpa_printf(MSG_INFO, "EAP-TEAP: Too short NAK TLV");
			tlv->result = TEAP_STATUS_FAILURE;
			break;
		}
		tlv->nak = pos;
		tlv->nak_len = len;
		break;
	case TEAP_TLV_ERROR:
		if (len < 4) {
			wpa_printf(MSG_INFO, "EAP-TEAP: Too short Error TLV");
			tlv->result = TEAP_STATUS_FAILURE;
			break;
		}
		tlv->error_code = WPA_GET_BE32(pos);
		wpa_printf(MSG_DEBUG, "EAP-TEAP: Error: %u", tlv->error_code);
		break;
	case TEAP_TLV_REQUEST_ACTION:
		wpa_hexdump(MSG_MSGDUMP, "EAP-TEAP: Request-Action TLV",
			    pos, len);
		if (tlv->request_action) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: More than one Request-Action TLV in the message");
			tlv->iresult = TEAP_STATUS_FAILURE;
			return -2;
		}
		if (len < 2) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: Too short Request-Action TLV");
			tlv->iresult = TEAP_STATUS_FAILURE;
			break;
		}
		tlv->request_action_status = pos[0];
		tlv->request_action = pos[1];
		wpa_printf(MSG_DEBUG,
			   "EAP-TEAP: Request-Action: Status=%u Action=%u",
			   tlv->request_action_status, tlv->request_action);
		break;
	case TEAP_TLV_EAP_PAYLOAD:
		wpa_hexdump(MSG_MSGDUMP, "EAP-TEAP: EAP-Payload TLV",
			    pos, len);
		if (tlv->eap_payload_tlv) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: More than one EAP-Payload TLV in the message");
			tlv->iresult = TEAP_STATUS_FAILURE;
			return -2;
		}
		tlv->eap_payload_tlv = pos;
		tlv->eap_payload_tlv_len = len;
		break;
	case TEAP_TLV_INTERMEDIATE_RESULT:
		wpa_hexdump(MSG_MSGDUMP, "EAP-TEAP: Intermediate-Result TLV",
			    pos, len);
		if (len < 2) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: Too short Intermediate-Result TLV");
			tlv->iresult = TEAP_STATUS_FAILURE;
			break;
		}
		if (tlv->iresult) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: More than one Intermediate-Result TLV in the message");
			tlv->iresult = TEAP_STATUS_FAILURE;
			return -2;
		}
		tlv->iresult = WPA_GET_BE16(pos);
		if (tlv->iresult != TEAP_STATUS_SUCCESS &&
		    tlv->iresult != TEAP_STATUS_FAILURE) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: Unknown Intermediate Result %d",
				   tlv->iresult);
			tlv->iresult = TEAP_STATUS_FAILURE;
		}
		wpa_printf(MSG_DEBUG, "EAP-TEAP: Intermediate Result: %s",
			   tlv->iresult == TEAP_STATUS_SUCCESS ?
			   "Success" : "Failure");
		break;
	case TEAP_TLV_PAC:
		wpa_hexdump(MSG_MSGDUMP, "EAP-TEAP: PAC TLV", pos, len);
		if (tlv->pac) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: More than one PAC TLV in the message");
			tlv->iresult = TEAP_STATUS_FAILURE;
			return -2;
		}
		tlv->pac = pos;
		tlv->pac_len = len;
		break;
	case TEAP_TLV_CRYPTO_BINDING:
		wpa_hexdump(MSG_MSGDUMP, "EAP-TEAP: Crypto-Binding TLV",
			    pos, len);
		if (tlv->crypto_binding) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: More than one Crypto-Binding TLV in the message");
			tlv->iresult = TEAP_STATUS_FAILURE;
			return -2;
		}
		tlv->crypto_binding_len = sizeof(struct teap_tlv_hdr) + len;
		if (tlv->crypto_binding_len < sizeof(*tlv->crypto_binding)) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: Too short Crypto-Binding TLV");
			tlv->iresult = TEAP_STATUS_FAILURE;
			return -2;
		}
		tlv->crypto_binding = (struct teap_tlv_crypto_binding *)
			(pos - sizeof(struct teap_tlv_hdr));
		break;
	case TEAP_TLV_BASIC_PASSWORD_AUTH_REQ:
		wpa_hexdump_ascii(MSG_MSGDUMP,
				  "EAP-TEAP: Basic-Password-Auth-Req TLV",
				  pos, len);
		if (tlv->basic_auth_req) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: More than one Basic-Password-Auth-Req TLV in the message");
			tlv->iresult = TEAP_STATUS_FAILURE;
			return -2;
		}
		tlv->basic_auth_req = pos;
		tlv->basic_auth_req_len = len;
		break;
	case TEAP_TLV_BASIC_PASSWORD_AUTH_RESP:
		wpa_hexdump_ascii(MSG_MSGDUMP,
				  "EAP-TEAP: Basic-Password-Auth-Resp TLV",
				  pos, len);
		if (tlv->basic_auth_resp) {
			wpa_printf(MSG_INFO,
				   "EAP-TEAP: More than one Basic-Password-Auth-Resp TLV in the message");
			tlv->iresult = TEAP_STATUS_FAILURE;
			return -2;
		}
		tlv->basic_auth_resp = pos;
		tlv->basic_auth_resp_len = len;
		break;
	default:
		/* Unknown TLV */
		return -1;
	}

	return 0;
}


const char * eap_teap_tlv_type_str(enum teap_tlv_types type)
{
	switch (type) {
	case TEAP_TLV_AUTHORITY_ID:
		return "Authority-ID";
	case TEAP_TLV_IDENTITY_TYPE:
		return "Identity-Type";
	case TEAP_TLV_RESULT:
		return "Result";
	case TEAP_TLV_NAK:
		return "NAK";
	case TEAP_TLV_ERROR:
		return "Error";
	case TEAP_TLV_CHANNEL_BINDING:
		return "Channel-Binding";
	case TEAP_TLV_VENDOR_SPECIFIC:
		return "Vendor-Specific";
	case TEAP_TLV_REQUEST_ACTION:
		return "Request-Action";
	case TEAP_TLV_EAP_PAYLOAD:
		return "EAP-Payload";
	case TEAP_TLV_INTERMEDIATE_RESULT:
		return "Intermediate-Result";
	case TEAP_TLV_PAC:
		return "PAC";
	case TEAP_TLV_CRYPTO_BINDING:
		return "Crypto-Binding";
	case TEAP_TLV_BASIC_PASSWORD_AUTH_REQ:
		return "Basic-Password-Auth-Req";
	case TEAP_TLV_BASIC_PASSWORD_AUTH_RESP:
		return "Basic-Password-Auth-Resp";
	case TEAP_TLV_PKCS7:
		return "PKCS#7";
	case TEAP_TLV_PKCS10:
		return "PKCS#10";
	case TEAP_TLV_TRUSTED_SERVER_ROOT:
		return "Trusted-Server-Root";
	}

	return "?";
}


struct wpabuf * eap_teap_tlv_result(int status, int intermediate)
{
	struct wpabuf *buf;
	struct teap_tlv_result *result;

	if (status != TEAP_STATUS_FAILURE && status != TEAP_STATUS_SUCCESS)
		return NULL;

	buf = wpabuf_alloc(sizeof(*result));
	if (!buf)
		return NULL;
	wpa_printf(MSG_DEBUG, "EAP-TEAP: Add %sResult TLV(status=%s)",
		   intermediate ? "Intermediate-" : "",
		   status == TEAP_STATUS_SUCCESS ? "Success" : "Failure");
	result = wpabuf_put(buf, sizeof(*result));
	result->tlv_type = host_to_be16(TEAP_TLV_MANDATORY |
					(intermediate ?
					 TEAP_TLV_INTERMEDIATE_RESULT :
					 TEAP_TLV_RESULT));
	result->length = host_to_be16(2);
	result->status = host_to_be16(status);
	return buf;
}


struct wpabuf * eap_teap_tlv_error(enum teap_error_codes error)
{
	struct wpabuf *buf;

	buf = wpabuf_alloc(4 + 4);
	if (!buf)
		return NULL;
	wpa_printf(MSG_DEBUG, "EAP-TEAP: Add Error TLV(Error Code=%d)",
		   error);
	wpabuf_put_be16(buf, TEAP_TLV_MANDATORY | TEAP_TLV_ERROR);
	wpabuf_put_be16(buf, 4);
	wpabuf_put_be32(buf, error);
	return buf;
}


struct wpabuf * eap_teap_tlv_identity_type(enum teap_identity_types id)
{
	struct wpabuf *buf;

	buf = wpabuf_alloc(4 + 2);
	if (!buf)
		return NULL;
	wpa_printf(MSG_DEBUG,
		   "EAP-TEAP: Add Identity-Type TLV(Identity-Type=%d)", id);
	wpabuf_put_be16(buf, TEAP_TLV_IDENTITY_TYPE);
	wpabuf_put_be16(buf, 2);
	wpabuf_put_be16(buf, id);
	return buf;
}


int eap_teap_allowed_anon_prov_phase2_method(int vendor, enum eap_type type)
{
	/* RFC 7170, Section 3.8.3: MUST provide mutual authentication,
	 * provide key generation, and be resistant to dictionary attack.
	 * Section 3.8 also mentions requirement for using EMSK Compound MAC. */
	return vendor == EAP_VENDOR_IETF &&
		(type == EAP_TYPE_PWD || type == EAP_TYPE_EKE);
}


int eap_teap_allowed_anon_prov_cipher_suite(u16 cs)
{
	/* RFC 7170, Section 3.8.3: anonymous ciphersuites MAY be supported as
	 * long as the TLS pre-master secret is generated form contribution from
	 * both peers. Accept the recommended TLS_DH_anon_WITH_AES_128_CBC_SHA
	 * cipher suite and other ciphersuites that use DH in some form, have
	 * SHA-1 or stronger MAC function, and use reasonable strong cipher. */
	static const u16 ok_cs[] = {
		/* DH-anon */
		0x0034, 0x003a, 0x006c, 0x006d, 0x00a6, 0x00a7,
		/* DHE-RSA */
		0x0033, 0x0039, 0x0067, 0x006b, 0x009e, 0x009f,
		/* ECDH-anon */
		0xc018, 0xc019,
		/* ECDH-RSA */
		0xc003, 0xc00f, 0xc029, 0xc02a, 0xc031, 0xc032,
		/* ECDH-ECDSA */
		0xc004, 0xc005, 0xc025, 0xc026, 0xc02d, 0xc02e,
		/* ECDHE-RSA */
		0xc013, 0xc014, 0xc027, 0xc028, 0xc02f, 0xc030,
		/* ECDHE-ECDSA */
		0xc009, 0xc00a, 0xc023, 0xc024, 0xc02b, 0xc02c,
	};

	return tls_cipher_suite_match(ok_cs, ARRAY_SIZE(ok_cs), cs);
}
