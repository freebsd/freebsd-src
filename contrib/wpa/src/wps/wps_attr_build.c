/*
 * Wi-Fi Protected Setup - attribute building
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
#include "crypto/aes_wrap.h"
#include "crypto/crypto.h"
#include "crypto/dh_group5.h"
#include "crypto/sha256.h"
#include "wps_i.h"


int wps_build_public_key(struct wps_data *wps, struct wpabuf *msg)
{
	struct wpabuf *pubkey;

	wpa_printf(MSG_DEBUG, "WPS:  * Public Key");
	wpabuf_free(wps->dh_privkey);
	if (wps->dev_pw_id != DEV_PW_DEFAULT && wps->wps->dh_privkey) {
		wpa_printf(MSG_DEBUG, "WPS: Using pre-configured DH keys");
		wps->dh_privkey = wpabuf_dup(wps->wps->dh_privkey);
		wps->dh_ctx = wps->wps->dh_ctx;
		wps->wps->dh_ctx = NULL;
		pubkey = wpabuf_dup(wps->wps->dh_pubkey);
	} else {
		wpa_printf(MSG_DEBUG, "WPS: Generate new DH keys");
		wps->dh_privkey = NULL;
		dh5_free(wps->dh_ctx);
		wps->dh_ctx = dh5_init(&wps->dh_privkey, &pubkey);
		pubkey = wpabuf_zeropad(pubkey, 192);
	}
	if (wps->dh_ctx == NULL || wps->dh_privkey == NULL || pubkey == NULL) {
		wpa_printf(MSG_DEBUG, "WPS: Failed to initialize "
			   "Diffie-Hellman handshake");
		wpabuf_free(pubkey);
		return -1;
	}

	wpabuf_put_be16(msg, ATTR_PUBLIC_KEY);
	wpabuf_put_be16(msg, wpabuf_len(pubkey));
	wpabuf_put_buf(msg, pubkey);

	if (wps->registrar) {
		wpabuf_free(wps->dh_pubkey_r);
		wps->dh_pubkey_r = pubkey;
	} else {
		wpabuf_free(wps->dh_pubkey_e);
		wps->dh_pubkey_e = pubkey;
	}

	return 0;
}


int wps_build_req_type(struct wpabuf *msg, enum wps_request_type type)
{
	wpa_printf(MSG_DEBUG, "WPS:  * Request Type");
	wpabuf_put_be16(msg, ATTR_REQUEST_TYPE);
	wpabuf_put_be16(msg, 1);
	wpabuf_put_u8(msg, type);
	return 0;
}


int wps_build_resp_type(struct wpabuf *msg, enum wps_response_type type)
{
	wpa_printf(MSG_DEBUG, "WPS:  * Response Type (%d)", type);
	wpabuf_put_be16(msg, ATTR_RESPONSE_TYPE);
	wpabuf_put_be16(msg, 1);
	wpabuf_put_u8(msg, type);
	return 0;
}


int wps_build_config_methods(struct wpabuf *msg, u16 methods)
{
	wpa_printf(MSG_DEBUG, "WPS:  * Config Methods (%x)", methods);
	wpabuf_put_be16(msg, ATTR_CONFIG_METHODS);
	wpabuf_put_be16(msg, 2);
	wpabuf_put_be16(msg, methods);
	return 0;
}


int wps_build_uuid_e(struct wpabuf *msg, const u8 *uuid)
{
	wpa_printf(MSG_DEBUG, "WPS:  * UUID-E");
	wpabuf_put_be16(msg, ATTR_UUID_E);
	wpabuf_put_be16(msg, WPS_UUID_LEN);
	wpabuf_put_data(msg, uuid, WPS_UUID_LEN);
	return 0;
}


int wps_build_dev_password_id(struct wpabuf *msg, u16 id)
{
	wpa_printf(MSG_DEBUG, "WPS:  * Device Password ID (%d)", id);
	wpabuf_put_be16(msg, ATTR_DEV_PASSWORD_ID);
	wpabuf_put_be16(msg, 2);
	wpabuf_put_be16(msg, id);
	return 0;
}


int wps_build_config_error(struct wpabuf *msg, u16 err)
{
	wpa_printf(MSG_DEBUG, "WPS:  * Configuration Error (%d)", err);
	wpabuf_put_be16(msg, ATTR_CONFIG_ERROR);
	wpabuf_put_be16(msg, 2);
	wpabuf_put_be16(msg, err);
	return 0;
}


int wps_build_authenticator(struct wps_data *wps, struct wpabuf *msg)
{
	u8 hash[SHA256_MAC_LEN];
	const u8 *addr[2];
	size_t len[2];

	if (wps->last_msg == NULL) {
		wpa_printf(MSG_DEBUG, "WPS: Last message not available for "
			   "building authenticator");
		return -1;
	}

	/* Authenticator = HMAC-SHA256_AuthKey(M_prev || M_curr*)
	 * (M_curr* is M_curr without the Authenticator attribute)
	 */
	addr[0] = wpabuf_head(wps->last_msg);
	len[0] = wpabuf_len(wps->last_msg);
	addr[1] = wpabuf_head(msg);
	len[1] = wpabuf_len(msg);
	hmac_sha256_vector(wps->authkey, WPS_AUTHKEY_LEN, 2, addr, len, hash);

	wpa_printf(MSG_DEBUG, "WPS:  * Authenticator");
	wpabuf_put_be16(msg, ATTR_AUTHENTICATOR);
	wpabuf_put_be16(msg, WPS_AUTHENTICATOR_LEN);
	wpabuf_put_data(msg, hash, WPS_AUTHENTICATOR_LEN);

	return 0;
}


int wps_build_version(struct wpabuf *msg)
{
	wpa_printf(MSG_DEBUG, "WPS:  * Version");
	wpabuf_put_be16(msg, ATTR_VERSION);
	wpabuf_put_be16(msg, 1);
	wpabuf_put_u8(msg, WPS_VERSION);
	return 0;
}


int wps_build_msg_type(struct wpabuf *msg, enum wps_msg_type msg_type)
{
	wpa_printf(MSG_DEBUG, "WPS:  * Message Type (%d)", msg_type);
	wpabuf_put_be16(msg, ATTR_MSG_TYPE);
	wpabuf_put_be16(msg, 1);
	wpabuf_put_u8(msg, msg_type);
	return 0;
}


int wps_build_enrollee_nonce(struct wps_data *wps, struct wpabuf *msg)
{
	wpa_printf(MSG_DEBUG, "WPS:  * Enrollee Nonce");
	wpabuf_put_be16(msg, ATTR_ENROLLEE_NONCE);
	wpabuf_put_be16(msg, WPS_NONCE_LEN);
	wpabuf_put_data(msg, wps->nonce_e, WPS_NONCE_LEN);
	return 0;
}


int wps_build_registrar_nonce(struct wps_data *wps, struct wpabuf *msg)
{
	wpa_printf(MSG_DEBUG, "WPS:  * Registrar Nonce");
	wpabuf_put_be16(msg, ATTR_REGISTRAR_NONCE);
	wpabuf_put_be16(msg, WPS_NONCE_LEN);
	wpabuf_put_data(msg, wps->nonce_r, WPS_NONCE_LEN);
	return 0;
}


int wps_build_auth_type_flags(struct wps_data *wps, struct wpabuf *msg)
{
	wpa_printf(MSG_DEBUG, "WPS:  * Authentication Type Flags");
	wpabuf_put_be16(msg, ATTR_AUTH_TYPE_FLAGS);
	wpabuf_put_be16(msg, 2);
	wpabuf_put_be16(msg, WPS_AUTH_TYPES);
	return 0;
}


int wps_build_encr_type_flags(struct wps_data *wps, struct wpabuf *msg)
{
	wpa_printf(MSG_DEBUG, "WPS:  * Encryption Type Flags");
	wpabuf_put_be16(msg, ATTR_ENCR_TYPE_FLAGS);
	wpabuf_put_be16(msg, 2);
	wpabuf_put_be16(msg, WPS_ENCR_TYPES);
	return 0;
}


int wps_build_conn_type_flags(struct wps_data *wps, struct wpabuf *msg)
{
	wpa_printf(MSG_DEBUG, "WPS:  * Connection Type Flags");
	wpabuf_put_be16(msg, ATTR_CONN_TYPE_FLAGS);
	wpabuf_put_be16(msg, 1);
	wpabuf_put_u8(msg, WPS_CONN_ESS);
	return 0;
}


int wps_build_assoc_state(struct wps_data *wps, struct wpabuf *msg)
{
	wpa_printf(MSG_DEBUG, "WPS:  * Association State");
	wpabuf_put_be16(msg, ATTR_ASSOC_STATE);
	wpabuf_put_be16(msg, 2);
	wpabuf_put_be16(msg, WPS_ASSOC_NOT_ASSOC);
	return 0;
}


int wps_build_key_wrap_auth(struct wps_data *wps, struct wpabuf *msg)
{
	u8 hash[SHA256_MAC_LEN];

	wpa_printf(MSG_DEBUG, "WPS:  * Key Wrap Authenticator");
	hmac_sha256(wps->authkey, WPS_AUTHKEY_LEN, wpabuf_head(msg),
		    wpabuf_len(msg), hash);

	wpabuf_put_be16(msg, ATTR_KEY_WRAP_AUTH);
	wpabuf_put_be16(msg, WPS_KWA_LEN);
	wpabuf_put_data(msg, hash, WPS_KWA_LEN);
	return 0;
}


int wps_build_encr_settings(struct wps_data *wps, struct wpabuf *msg,
			    struct wpabuf *plain)
{
	size_t pad_len;
	const size_t block_size = 16;
	u8 *iv, *data;

	wpa_printf(MSG_DEBUG, "WPS:  * Encrypted Settings");

	/* PKCS#5 v2.0 pad */
	pad_len = block_size - wpabuf_len(plain) % block_size;
	os_memset(wpabuf_put(plain, pad_len), pad_len, pad_len);

	wpabuf_put_be16(msg, ATTR_ENCR_SETTINGS);
	wpabuf_put_be16(msg, block_size + wpabuf_len(plain));

	iv = wpabuf_put(msg, block_size);
	if (os_get_random(iv, block_size) < 0)
		return -1;

	data = wpabuf_put(msg, 0);
	wpabuf_put_buf(msg, plain);
	if (aes_128_cbc_encrypt(wps->keywrapkey, iv, data, wpabuf_len(plain)))
		return -1;

	return 0;
}


#ifdef CONFIG_WPS_OOB
int wps_build_oob_dev_password(struct wpabuf *msg, struct wps_context *wps)
{
	size_t hash_len;
	const u8 *addr[1];
	u8 pubkey_hash[WPS_HASH_LEN];
	u8 dev_password_bin[WPS_OOB_DEVICE_PASSWORD_LEN];

	wpa_printf(MSG_DEBUG, "WPS:  * OOB Device Password");

	addr[0] = wpabuf_head(wps->dh_pubkey);
	hash_len = wpabuf_len(wps->dh_pubkey);
	sha256_vector(1, addr, &hash_len, pubkey_hash);

	if (os_get_random((u8 *) &wps->oob_dev_pw_id, sizeof(u16)) < 0) {
		wpa_printf(MSG_ERROR, "WPS: device password id "
			   "generation error");
		return -1;
	}
	wps->oob_dev_pw_id |= 0x0010;

	if (os_get_random(dev_password_bin, WPS_OOB_DEVICE_PASSWORD_LEN) < 0) {
		wpa_printf(MSG_ERROR, "WPS: OOB device password "
			   "generation error");
		return -1;
	}

	wpabuf_put_be16(msg, ATTR_OOB_DEVICE_PASSWORD);
	wpabuf_put_be16(msg, WPS_OOB_DEVICE_PASSWORD_ATTR_LEN);
	wpabuf_put_data(msg, pubkey_hash, WPS_OOB_PUBKEY_HASH_LEN);
	wpabuf_put_be16(msg, wps->oob_dev_pw_id);
	wpabuf_put_data(msg, dev_password_bin, WPS_OOB_DEVICE_PASSWORD_LEN);

	wpa_snprintf_hex_uppercase(
		wpabuf_put(wps->oob_conf.dev_password,
			   wpabuf_size(wps->oob_conf.dev_password)),
		wpabuf_size(wps->oob_conf.dev_password),
		dev_password_bin, WPS_OOB_DEVICE_PASSWORD_LEN);

	return 0;
}
#endif /* CONFIG_WPS_OOB */
