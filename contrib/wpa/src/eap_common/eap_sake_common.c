/*
 * EAP server/peer: EAP-SAKE shared routines
 * Copyright (c) 2006-2019, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "wpabuf.h"
#include "crypto/sha1.h"
#include "eap_defs.h"
#include "eap_sake_common.h"


static int eap_sake_parse_add_attr(struct eap_sake_parse_attr *attr,
				   u8 attr_id, u8 len, const u8 *data)
{
	size_t i;

	switch (attr_id) {
	case EAP_SAKE_AT_RAND_S:
		wpa_printf(MSG_DEBUG, "EAP-SAKE: Parse: AT_RAND_S");
		if (len != EAP_SAKE_RAND_LEN) {
			wpa_printf(MSG_DEBUG, "EAP-SAKE: AT_RAND_S with "
				   "invalid payload length %d", len);
			return -1;
		}
		attr->rand_s = data;
		break;
	case EAP_SAKE_AT_RAND_P:
		wpa_printf(MSG_DEBUG, "EAP-SAKE: Parse: AT_RAND_P");
		if (len != EAP_SAKE_RAND_LEN) {
			wpa_printf(MSG_DEBUG, "EAP-SAKE: AT_RAND_P with "
				   "invalid payload length %d", len);
			return -1;
		}
		attr->rand_p = data;
		break;
	case EAP_SAKE_AT_MIC_S:
		wpa_printf(MSG_DEBUG, "EAP-SAKE: Parse: AT_MIC_S");
		if (len != EAP_SAKE_MIC_LEN) {
			wpa_printf(MSG_DEBUG, "EAP-SAKE: AT_MIC_S with "
				   "invalid payload length %d", len);
			return -1;
		}
		attr->mic_s = data;
		break;
	case EAP_SAKE_AT_MIC_P:
		wpa_printf(MSG_DEBUG, "EAP-SAKE: Parse: AT_MIC_P");
		if (len != EAP_SAKE_MIC_LEN) {
			wpa_printf(MSG_DEBUG, "EAP-SAKE: AT_MIC_P with "
				   "invalid payload length %d", len);
			return -1;
		}
		attr->mic_p = data;
		break;
	case EAP_SAKE_AT_SERVERID:
		wpa_printf(MSG_DEBUG, "EAP-SAKE: Parse: AT_SERVERID");
		attr->serverid = data;
		attr->serverid_len = len;
		break;
	case EAP_SAKE_AT_PEERID:
		wpa_printf(MSG_DEBUG, "EAP-SAKE: Parse: AT_PEERID");
		attr->peerid = data;
		attr->peerid_len = len;
		break;
	case EAP_SAKE_AT_SPI_S:
		wpa_printf(MSG_DEBUG, "EAP-SAKE: Parse: AT_SPI_S");
		attr->spi_s = data;
		attr->spi_s_len = len;
		break;
	case EAP_SAKE_AT_SPI_P:
		wpa_printf(MSG_DEBUG, "EAP-SAKE: Parse: AT_SPI_P");
		attr->spi_p = data;
		attr->spi_p_len = len;
		break;
	case EAP_SAKE_AT_ANY_ID_REQ:
		wpa_printf(MSG_DEBUG, "EAP-SAKE: Parse: AT_ANY_ID_REQ");
		if (len != 2) {
			wpa_printf(MSG_DEBUG, "EAP-SAKE: Invalid AT_ANY_ID_REQ"
				   " payload length %d", len);
			return -1;
		}
		attr->any_id_req = data;
		break;
	case EAP_SAKE_AT_PERM_ID_REQ:
		wpa_printf(MSG_DEBUG, "EAP-SAKE: Parse: AT_PERM_ID_REQ");
		if (len != 2) {
			wpa_printf(MSG_DEBUG, "EAP-SAKE: Invalid "
				   "AT_PERM_ID_REQ payload length %d", len);
			return -1;
		}
		attr->perm_id_req = data;
		break;
	case EAP_SAKE_AT_ENCR_DATA:
		wpa_printf(MSG_DEBUG, "EAP-SAKE: Parse: AT_ENCR_DATA");
		attr->encr_data = data;
		attr->encr_data_len = len;
		break;
	case EAP_SAKE_AT_IV:
		wpa_printf(MSG_DEBUG, "EAP-SAKE: Parse: AT_IV");
		attr->iv = data;
		attr->iv_len = len;
		break;
	case EAP_SAKE_AT_PADDING:
		wpa_printf(MSG_DEBUG, "EAP-SAKE: Parse: AT_PADDING");
		for (i = 0; i < len; i++) {
			if (data[i]) {
				wpa_printf(MSG_DEBUG, "EAP-SAKE: AT_PADDING "
					   "with non-zero pad byte");
				return -1;
			}
		}
		break;
	case EAP_SAKE_AT_NEXT_TMPID:
		wpa_printf(MSG_DEBUG, "EAP-SAKE: Parse: AT_NEXT_TMPID");
		attr->next_tmpid = data;
		attr->next_tmpid_len = len;
		break;
	case EAP_SAKE_AT_MSK_LIFE:
		wpa_printf(MSG_DEBUG, "EAP-SAKE: Parse: AT_MSK_LIFE");
		if (len != 4) {
			wpa_printf(MSG_DEBUG, "EAP-SAKE: Invalid "
				   "AT_MSK_LIFE payload length %d", len);
			return -1;
		}
		attr->msk_life = data;
		break;
	default:
		if (attr_id < 128) {
			wpa_printf(MSG_DEBUG, "EAP-SAKE: Unknown non-skippable"
				   " attribute %d", attr_id);
			return -1;
		}
		wpa_printf(MSG_DEBUG, "EAP-SAKE: Ignoring unknown skippable "
			   "attribute %d", attr_id);
		break;
	}

	if (attr->iv && !attr->encr_data) {
		wpa_printf(MSG_DEBUG, "EAP-SAKE: AT_IV included without "
			   "AT_ENCR_DATA");
		return -1;
	}

	return 0;
}


/**
 * eap_sake_parse_attributes - Parse EAP-SAKE attributes
 * @buf: Packet payload (starting with the first attribute)
 * @len: Payload length
 * @attr: Structure to be filled with found attributes
 * Returns: 0 on success or -1 on failure
 */
int eap_sake_parse_attributes(const u8 *buf, size_t len,
			      struct eap_sake_parse_attr *attr)
{
	const u8 *pos = buf, *end = buf + len;

	os_memset(attr, 0, sizeof(*attr));
	while (pos < end) {
		u8 attr_id, attr_len;

		if (end - pos < 2) {
			wpa_printf(MSG_DEBUG, "EAP-SAKE: Too short attribute");
			return -1;
		}

		attr_id = *pos++;
		attr_len = *pos++;
		/* Attribute length value includes the Type and Length fields */
		if (attr_len < 2) {
			wpa_printf(MSG_DEBUG,
				   "EAP-SAKE: Invalid attribute length (%d)",
				   attr_len);
			return -1;
		}
		attr_len -= 2;

		if (attr_len > end - pos) {
			wpa_printf(MSG_DEBUG, "EAP-SAKE: Attribute underflow");
			return -1;
		}

		if (eap_sake_parse_add_attr(attr, attr_id, attr_len, pos))
			return -1;

		pos += attr_len;
	}

	return 0;
}


/**
 * eap_sake_kdf - EAP-SAKE Key Derivation Function (KDF)
 * @key: Key for KDF
 * @key_len: Length of the key in bytes
 * @label: A unique label for each purpose of the KDF
 * @data: Extra data (start) to bind into the key
 * @data_len: Length of the data
 * @data2: Extra data (end) to bind into the key
 * @data2_len: Length of the data2
 * @buf: Buffer for the generated pseudo-random key
 * @buf_len: Number of bytes of key to generate
 * Returns: 0 on success or -1 on failure
 *
 * This function is used to derive new, cryptographically separate keys from a
 * given key (e.g., SMS). This is identical to the PRF used in IEEE 802.11i.
 */
static int eap_sake_kdf(const u8 *key, size_t key_len, const char *label,
			const u8 *data, size_t data_len,
			const u8 *data2, size_t data2_len,
			u8 *buf, size_t buf_len)
{
	u8 counter = 0;
	size_t pos, plen;
	u8 hash[SHA1_MAC_LEN];
	size_t label_len = os_strlen(label) + 1;
	const unsigned char *addr[4];
	size_t len[4];

	addr[0] = (u8 *) label; /* Label | Y */
	len[0] = label_len;
	addr[1] = data; /* Msg[start] */
	len[1] = data_len;
	addr[2] = data2; /* Msg[end] */
	len[2] = data2_len;
	addr[3] = &counter; /* Length */
	len[3] = 1;

	pos = 0;
	while (pos < buf_len) {
		plen = buf_len - pos;
		if (plen >= SHA1_MAC_LEN) {
			if (hmac_sha1_vector(key, key_len, 4, addr, len,
					     &buf[pos]) < 0)
				return -1;
			pos += SHA1_MAC_LEN;
		} else {
			if (hmac_sha1_vector(key, key_len, 4, addr, len,
					     hash) < 0)
				return -1;
			os_memcpy(&buf[pos], hash, plen);
			break;
		}
		counter++;
	}

	return 0;
}


/**
 * eap_sake_derive_keys - Derive EAP-SAKE keys
 * @root_secret_a: 16-byte Root-Secret-A
 * @root_secret_b: 16-byte Root-Secret-B
 * @rand_s: 16-byte RAND_S
 * @rand_p: 16-byte RAND_P
 * @tek: Buffer for Temporary EAK Keys (TEK-Auth[16] | TEK-Cipher[16])
 * @msk: Buffer for 64-byte MSK
 * @emsk: Buffer for 64-byte EMSK
 * Returns: 0 on success or -1 on failure
 *
 * This function derives EAP-SAKE keys as defined in RFC 4763, section 3.2.6.
 */
int eap_sake_derive_keys(const u8 *root_secret_a, const u8 *root_secret_b,
			 const u8 *rand_s, const u8 *rand_p, u8 *tek, u8 *msk,
			 u8 *emsk)
{
	u8 sms_a[EAP_SAKE_SMS_LEN];
	u8 sms_b[EAP_SAKE_SMS_LEN];
	u8 key_buf[EAP_MSK_LEN + EAP_EMSK_LEN];

	wpa_printf(MSG_DEBUG, "EAP-SAKE: Deriving keys");

	wpa_hexdump_key(MSG_DEBUG, "EAP-SAKE: Root-Secret-A",
			root_secret_a, EAP_SAKE_ROOT_SECRET_LEN);
	if (eap_sake_kdf(root_secret_a, EAP_SAKE_ROOT_SECRET_LEN,
			 "SAKE Master Secret A",
			 rand_p, EAP_SAKE_RAND_LEN, rand_s, EAP_SAKE_RAND_LEN,
			 sms_a, EAP_SAKE_SMS_LEN) < 0)
		return -1;
	wpa_hexdump_key(MSG_DEBUG, "EAP-SAKE: SMS-A", sms_a, EAP_SAKE_SMS_LEN);
	if (eap_sake_kdf(sms_a, EAP_SAKE_SMS_LEN, "Transient EAP Key",
			 rand_s, EAP_SAKE_RAND_LEN, rand_p, EAP_SAKE_RAND_LEN,
			 tek, EAP_SAKE_TEK_LEN) < 0)
		return -1;
	wpa_hexdump_key(MSG_DEBUG, "EAP-SAKE: TEK-Auth",
			tek, EAP_SAKE_TEK_AUTH_LEN);
	wpa_hexdump_key(MSG_DEBUG, "EAP-SAKE: TEK-Cipher",
			tek + EAP_SAKE_TEK_AUTH_LEN, EAP_SAKE_TEK_CIPHER_LEN);

	wpa_hexdump_key(MSG_DEBUG, "EAP-SAKE: Root-Secret-B",
			root_secret_b, EAP_SAKE_ROOT_SECRET_LEN);
	if (eap_sake_kdf(root_secret_b, EAP_SAKE_ROOT_SECRET_LEN,
			 "SAKE Master Secret B",
			 rand_p, EAP_SAKE_RAND_LEN, rand_s, EAP_SAKE_RAND_LEN,
			 sms_b, EAP_SAKE_SMS_LEN) < 0)
		return -1;
	wpa_hexdump_key(MSG_DEBUG, "EAP-SAKE: SMS-B", sms_b, EAP_SAKE_SMS_LEN);
	if (eap_sake_kdf(sms_b, EAP_SAKE_SMS_LEN, "Master Session Key",
			 rand_s, EAP_SAKE_RAND_LEN, rand_p, EAP_SAKE_RAND_LEN,
			 key_buf, sizeof(key_buf)) < 0)
		return -1;
	os_memcpy(msk, key_buf, EAP_MSK_LEN);
	os_memcpy(emsk, key_buf + EAP_MSK_LEN, EAP_EMSK_LEN);
	wpa_hexdump_key(MSG_DEBUG, "EAP-SAKE: MSK", msk, EAP_MSK_LEN);
	wpa_hexdump_key(MSG_DEBUG, "EAP-SAKE: EMSK", emsk, EAP_EMSK_LEN);
	return 0;
}


/**
 * eap_sake_compute_mic - Compute EAP-SAKE MIC for an EAP packet
 * @tek_auth: 16-byte TEK-Auth
 * @rand_s: 16-byte RAND_S
 * @rand_p: 16-byte RAND_P
 * @serverid: SERVERID
 * @serverid_len: SERVERID length
 * @peerid: PEERID
 * @peerid_len: PEERID length
 * @peer: MIC calculation for 0 = Server, 1 = Peer message
 * @eap: EAP packet
 * @eap_len: EAP packet length
 * @mic_pos: MIC position in the EAP packet (must be [eap .. eap + eap_len])
 * @mic: Buffer for the computed 16-byte MIC
 * Returns: 0 on success or -1 on failure
 */
int eap_sake_compute_mic(const u8 *tek_auth,
			 const u8 *rand_s, const u8 *rand_p,
			 const u8 *serverid, size_t serverid_len,
			 const u8 *peerid, size_t peerid_len,
			 int peer, const u8 *eap, size_t eap_len,
			 const u8 *mic_pos, u8 *mic)
{
	u8 _rand[2 * EAP_SAKE_RAND_LEN];
	u8 *tmp, *pos;
	size_t tmplen;
	int ret;

	tmplen = serverid_len + 1 + peerid_len + 1 + eap_len;
	tmp = os_malloc(tmplen);
	if (tmp == NULL)
		return -1;
	pos = tmp;
	if (peer) {
		if (peerid) {
			os_memcpy(pos, peerid, peerid_len);
			pos += peerid_len;
		}
		*pos++ = 0x00;
		if (serverid) {
			os_memcpy(pos, serverid, serverid_len);
			pos += serverid_len;
		}
		*pos++ = 0x00;

		os_memcpy(_rand, rand_s, EAP_SAKE_RAND_LEN);
		os_memcpy(_rand + EAP_SAKE_RAND_LEN, rand_p,
			  EAP_SAKE_RAND_LEN);
	} else {
		if (serverid) {
			os_memcpy(pos, serverid, serverid_len);
			pos += serverid_len;
		}
		*pos++ = 0x00;
		if (peerid) {
			os_memcpy(pos, peerid, peerid_len);
			pos += peerid_len;
		}
		*pos++ = 0x00;

		os_memcpy(_rand, rand_p, EAP_SAKE_RAND_LEN);
		os_memcpy(_rand + EAP_SAKE_RAND_LEN, rand_s,
			  EAP_SAKE_RAND_LEN);
	}

	os_memcpy(pos, eap, eap_len);
	os_memset(pos + (mic_pos - eap), 0, EAP_SAKE_MIC_LEN);

	ret = eap_sake_kdf(tek_auth, EAP_SAKE_TEK_AUTH_LEN,
			   peer ? "Peer MIC" : "Server MIC",
			   _rand, 2 * EAP_SAKE_RAND_LEN, tmp, tmplen,
			   mic, EAP_SAKE_MIC_LEN);

	os_free(tmp);

	return ret;
}


void eap_sake_add_attr(struct wpabuf *buf, u8 type, const u8 *data,
		       size_t len)
{
	wpabuf_put_u8(buf, type);
	wpabuf_put_u8(buf, 2 + len); /* Length; including attr header */
	if (data)
		wpabuf_put_data(buf, data, len);
	else
		os_memset(wpabuf_put(buf, len), 0, len);
}
