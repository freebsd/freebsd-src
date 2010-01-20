/*
 * Wi-Fi Protected Setup - common functionality
 * Copyright (c) 2008, Jouni Malinen <j@w1.fi>
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

#include "includes.h"

#include "common.h"
#include "dh_groups.h"
#include "sha256.h"
#include "aes_wrap.h"
#include "crypto.h"
#include "wps_i.h"
#include "wps_dev_attr.h"


void wps_kdf(const u8 *key, const u8 *label_prefix, size_t label_prefix_len,
	     const char *label, u8 *res, size_t res_len)
{
	u8 i_buf[4], key_bits[4];
	const u8 *addr[4];
	size_t len[4];
	int i, iter;
	u8 hash[SHA256_MAC_LEN], *opos;
	size_t left;

	WPA_PUT_BE32(key_bits, res_len * 8);

	addr[0] = i_buf;
	len[0] = sizeof(i_buf);
	addr[1] = label_prefix;
	len[1] = label_prefix_len;
	addr[2] = (const u8 *) label;
	len[2] = os_strlen(label);
	addr[3] = key_bits;
	len[3] = sizeof(key_bits);

	iter = (res_len + SHA256_MAC_LEN - 1) / SHA256_MAC_LEN;
	opos = res;
	left = res_len;

	for (i = 1; i <= iter; i++) {
		WPA_PUT_BE32(i_buf, i);
		hmac_sha256_vector(key, SHA256_MAC_LEN, 4, addr, len, hash);
		if (i < iter) {
			os_memcpy(opos, hash, SHA256_MAC_LEN);
			opos += SHA256_MAC_LEN;
			left -= SHA256_MAC_LEN;
		} else
			os_memcpy(opos, hash, left);
	}
}


int wps_derive_keys(struct wps_data *wps)
{
	struct wpabuf *pubkey, *dh_shared;
	u8 dhkey[SHA256_MAC_LEN], kdk[SHA256_MAC_LEN];
	const u8 *addr[3];
	size_t len[3];
	u8 keys[WPS_AUTHKEY_LEN + WPS_KEYWRAPKEY_LEN + WPS_EMSK_LEN];

	if (wps->dh_privkey == NULL) {
		wpa_printf(MSG_DEBUG, "WPS: Own DH private key not available");
		return -1;
	}

	pubkey = wps->registrar ? wps->dh_pubkey_e : wps->dh_pubkey_r;
	if (pubkey == NULL) {
		wpa_printf(MSG_DEBUG, "WPS: Peer DH public key not available");
		return -1;
	}

	dh_shared = dh_derive_shared(pubkey, wps->dh_privkey,
				     dh_groups_get(WPS_DH_GROUP));
	dh_shared = wpabuf_zeropad(dh_shared, 192);
	if (dh_shared == NULL) {
		wpa_printf(MSG_DEBUG, "WPS: Failed to derive DH shared key");
		return -1;
	}

	/* Own DH private key is not needed anymore */
	wpabuf_free(wps->dh_privkey);
	wps->dh_privkey = NULL;

	wpa_hexdump_buf_key(MSG_DEBUG, "WPS: DH shared key", dh_shared);

	/* DHKey = SHA-256(g^AB mod p) */
	addr[0] = wpabuf_head(dh_shared);
	len[0] = wpabuf_len(dh_shared);
	sha256_vector(1, addr, len, dhkey);
	wpa_hexdump_key(MSG_DEBUG, "WPS: DHKey", dhkey, sizeof(dhkey));
	wpabuf_free(dh_shared);

	/* KDK = HMAC-SHA-256_DHKey(N1 || EnrolleeMAC || N2) */
	addr[0] = wps->nonce_e;
	len[0] = WPS_NONCE_LEN;
	addr[1] = wps->mac_addr_e;
	len[1] = ETH_ALEN;
	addr[2] = wps->nonce_r;
	len[2] = WPS_NONCE_LEN;
	hmac_sha256_vector(dhkey, sizeof(dhkey), 3, addr, len, kdk);
	wpa_hexdump_key(MSG_DEBUG, "WPS: KDK", kdk, sizeof(kdk));

	wps_kdf(kdk, NULL, 0, "Wi-Fi Easy and Secure Key Derivation",
		keys, sizeof(keys));
	os_memcpy(wps->authkey, keys, WPS_AUTHKEY_LEN);
	os_memcpy(wps->keywrapkey, keys + WPS_AUTHKEY_LEN, WPS_KEYWRAPKEY_LEN);
	os_memcpy(wps->emsk, keys + WPS_AUTHKEY_LEN + WPS_KEYWRAPKEY_LEN,
		  WPS_EMSK_LEN);

	wpa_hexdump_key(MSG_DEBUG, "WPS: AuthKey",
			wps->authkey, WPS_AUTHKEY_LEN);
	wpa_hexdump_key(MSG_DEBUG, "WPS: KeyWrapKey",
			wps->keywrapkey, WPS_KEYWRAPKEY_LEN);
	wpa_hexdump_key(MSG_DEBUG, "WPS: EMSK", wps->emsk, WPS_EMSK_LEN);

	return 0;
}


int wps_derive_mgmt_keys(struct wps_data *wps)
{
	u8 nonces[2 * WPS_NONCE_LEN];
	u8 keys[WPS_MGMTAUTHKEY_LEN + WPS_MGMTENCKEY_LEN];
	u8 hash[SHA256_MAC_LEN];
	const u8 *addr[2];
	size_t len[2];
	const char *auth_label = "WFA-WLAN-Management-MgmtAuthKey";
	const char *enc_label = "WFA-WLAN-Management-MgmtEncKey";

	/* MgmtAuthKey || MgmtEncKey =
	 * kdf(EMSK, N1 || N2 || "WFA-WLAN-Management-Keys", 384) */
	os_memcpy(nonces, wps->nonce_e, WPS_NONCE_LEN);
	os_memcpy(nonces + WPS_NONCE_LEN, wps->nonce_r, WPS_NONCE_LEN);
	wps_kdf(wps->emsk, nonces, sizeof(nonces), "WFA-WLAN-Management-Keys",
		keys, sizeof(keys));
	os_memcpy(wps->mgmt_auth_key, keys, WPS_MGMTAUTHKEY_LEN);
	os_memcpy(wps->mgmt_enc_key, keys + WPS_MGMTAUTHKEY_LEN,
		  WPS_MGMTENCKEY_LEN);

	addr[0] = nonces;
	len[0] = sizeof(nonces);

	/* MgmtEncKeyID = first 128 bits of
	 * SHA-256(N1 || N2 || "WFA-WLAN-Management-MgmtAuthKey") */
	addr[1] = (const u8 *) auth_label;
	len[1] = os_strlen(auth_label);
	sha256_vector(2, addr, len, hash);
	os_memcpy(wps->mgmt_auth_key_id, hash, WPS_MGMT_KEY_ID_LEN);

	/* MgmtEncKeyID = first 128 bits of
	 * SHA-256(N1 || N2 || "WFA-WLAN-Management-MgmtEncKey") */
	addr[1] = (const u8 *) enc_label;
	len[1] = os_strlen(enc_label);
	sha256_vector(2, addr, len, hash);
	os_memcpy(wps->mgmt_enc_key_id, hash, WPS_MGMT_KEY_ID_LEN);

	wpa_hexdump_key(MSG_DEBUG, "WPS: MgmtAuthKey",
			wps->mgmt_auth_key, WPS_MGMTAUTHKEY_LEN);
	wpa_hexdump(MSG_DEBUG, "WPS: MgmtAuthKeyID",
		    wps->mgmt_auth_key_id, WPS_MGMT_KEY_ID_LEN);
	wpa_hexdump_key(MSG_DEBUG, "WPS: MgmtEncKey",
			wps->mgmt_enc_key, WPS_MGMTENCKEY_LEN);
	wpa_hexdump(MSG_DEBUG, "WPS: MgmtEncKeyID",
		    wps->mgmt_enc_key_id, WPS_MGMT_KEY_ID_LEN);

	return 0;
}


void wps_derive_psk(struct wps_data *wps, const u8 *dev_passwd,
		    size_t dev_passwd_len)
{
	u8 hash[SHA256_MAC_LEN];

	hmac_sha256(wps->authkey, WPS_AUTHKEY_LEN, dev_passwd,
		    (dev_passwd_len + 1) / 2, hash);
	os_memcpy(wps->psk1, hash, WPS_PSK_LEN);
	hmac_sha256(wps->authkey, WPS_AUTHKEY_LEN,
		    dev_passwd + (dev_passwd_len + 1) / 2,
		    dev_passwd_len / 2, hash);
	os_memcpy(wps->psk2, hash, WPS_PSK_LEN);

	wpa_hexdump_ascii_key(MSG_DEBUG, "WPS: Device Password",
			      dev_passwd, dev_passwd_len);
	wpa_hexdump_key(MSG_DEBUG, "WPS: PSK1", wps->psk1, WPS_PSK_LEN);
	wpa_hexdump_key(MSG_DEBUG, "WPS: PSK2", wps->psk2, WPS_PSK_LEN);
}


struct wpabuf * wps_decrypt_encr_settings(struct wps_data *wps, const u8 *encr,
					  size_t encr_len)
{
	struct wpabuf *decrypted;
	const size_t block_size = 16;
	size_t i;
	u8 pad;
	const u8 *pos;

	/* AES-128-CBC */
	if (encr == NULL || encr_len < 2 * block_size || encr_len % block_size)
	{
		wpa_printf(MSG_DEBUG, "WPS: No Encrypted Settings received");
		return NULL;
	}

	decrypted = wpabuf_alloc(encr_len - block_size);
	if (decrypted == NULL)
		return NULL;

	wpa_hexdump(MSG_MSGDUMP, "WPS: Encrypted Settings", encr, encr_len);
	wpabuf_put_data(decrypted, encr + block_size, encr_len - block_size);
	if (aes_128_cbc_decrypt(wps->keywrapkey, encr, wpabuf_mhead(decrypted),
				wpabuf_len(decrypted))) {
		wpabuf_free(decrypted);
		return NULL;
	}

	wpa_hexdump_buf_key(MSG_MSGDUMP, "WPS: Decrypted Encrypted Settings",
			    decrypted);

	pos = wpabuf_head_u8(decrypted) + wpabuf_len(decrypted) - 1;
	pad = *pos;
	if (pad > wpabuf_len(decrypted)) {
		wpa_printf(MSG_DEBUG, "WPS: Invalid PKCS#5 v2.0 pad value");
		wpabuf_free(decrypted);
		return NULL;
	}
	for (i = 0; i < pad; i++) {
		if (*pos-- != pad) {
			wpa_printf(MSG_DEBUG, "WPS: Invalid PKCS#5 v2.0 pad "
				   "string");
			wpabuf_free(decrypted);
			return NULL;
		}
	}
	decrypted->used -= pad;

	return decrypted;
}


/**
 * wps_pin_checksum - Compute PIN checksum
 * @pin: Seven digit PIN (i.e., eight digit PIN without the checksum digit)
 * Returns: Checksum digit
 */
unsigned int wps_pin_checksum(unsigned int pin)
{
	unsigned int accum = 0;
	while (pin) {
		accum += 3 * (pin % 10);
		pin /= 10;
		accum += pin % 10;
		pin /= 10;
	}

	return (10 - accum % 10) % 10;
}


/**
 * wps_pin_valid - Check whether a PIN has a valid checksum
 * @pin: Eight digit PIN (i.e., including the checksum digit)
 * Returns: 1 if checksum digit is valid, or 0 if not
 */
unsigned int wps_pin_valid(unsigned int pin)
{
	return wps_pin_checksum(pin / 10) == (pin % 10);
}


/**
 * wps_generate_pin - Generate a random PIN
 * Returns: Eight digit PIN (i.e., including the checksum digit)
 */
unsigned int wps_generate_pin(void)
{
	unsigned int val;

	/* Generate seven random digits for the PIN */
	if (os_get_random((unsigned char *) &val, sizeof(val)) < 0) {
		struct os_time now;
		os_get_time(&now);
		val = os_random() ^ now.sec ^ now.usec;
	}
	val %= 10000000;

	/* Append checksum digit */
	return val * 10 + wps_pin_checksum(val);
}


void wps_fail_event(struct wps_context *wps, enum wps_msg_type msg)
{
	union wps_event_data data;

	if (wps->event_cb == NULL)
		return;

	os_memset(&data, 0, sizeof(data));
	data.fail.msg = msg;
	wps->event_cb(wps->cb_ctx, WPS_EV_FAIL, &data);
}


void wps_success_event(struct wps_context *wps)
{
	if (wps->event_cb == NULL)
		return;

	wps->event_cb(wps->cb_ctx, WPS_EV_SUCCESS, NULL);
}


void wps_pwd_auth_fail_event(struct wps_context *wps, int enrollee, int part)
{
	union wps_event_data data;

	if (wps->event_cb == NULL)
		return;

	os_memset(&data, 0, sizeof(data));
	data.pwd_auth_fail.enrollee = enrollee;
	data.pwd_auth_fail.part = part;
	wps->event_cb(wps->cb_ctx, WPS_EV_PWD_AUTH_FAIL, &data);
}
