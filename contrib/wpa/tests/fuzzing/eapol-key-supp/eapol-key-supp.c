/*
 * Testing tool for EAPOL-Key Supplicant routines
 * Copyright (c) 2006-2019, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/eloop.h"
#include "rsn_supp/wpa.h"
#include "../fuzzer-common.h"


struct wpa {
	const u8 *data;
	size_t data_len;
	size_t data_offset;
	int wpa1;

	u8 auth_addr[ETH_ALEN];
	u8 supp_addr[ETH_ALEN];
	u8 psk[PMK_LEN];

	/* from authenticator */
	u8 *auth_eapol;
	size_t auth_eapol_len;

	struct wpa_sm *supp;

	u8 supp_ie[80];
	size_t supp_ie_len;
};


const struct wpa_driver_ops *const wpa_drivers[] = { NULL };


static u8 * read_msg(struct wpa *wpa, size_t *ret_len)
{
	u16 msg_len;
	u8 *msg;

	if (wpa->data_len - wpa->data_offset < 2) {
		wpa_printf(MSG_ERROR, "TEST-ERROR: Could not read msg len");
		eloop_terminate();
		return NULL;
	}
	msg_len = WPA_GET_BE16(&wpa->data[wpa->data_offset]);
	wpa->data_offset += 2;

	msg = os_malloc(msg_len);
	if (!msg)
		return NULL;
	if (msg_len > 0 && wpa->data_len - wpa->data_offset < msg_len) {
		wpa_printf(MSG_ERROR, "TEST-ERROR: Truncated msg (msg_len=%u)",
			   msg_len);
		os_free(msg);
		eloop_terminate();
		return NULL;
	}
	os_memcpy(msg, &wpa->data[wpa->data_offset], msg_len);
	wpa->data_offset += msg_len;
	wpa_hexdump(MSG_DEBUG, "TEST: Read message from file", msg, msg_len);

	*ret_len = msg_len;
	return msg;
}


static int supp_get_bssid(void *ctx, u8 *bssid)
{
	struct wpa *wpa = ctx;
	wpa_printf(MSG_DEBUG, "SUPP: %s", __func__);
	os_memcpy(bssid, wpa->auth_addr, ETH_ALEN);
	return 0;
}


static void supp_set_state(void *ctx, enum wpa_states state)
{
	wpa_printf(MSG_DEBUG, "SUPP: %s(state=%d)", __func__, state);
}


static void supp_eapol_rx(void *eloop_data, void *user_ctx)
{
	struct wpa *wpa = eloop_data;

	wpa_printf(MSG_DEBUG, "SUPP: RX EAPOL frame");
	wpa_sm_rx_eapol(wpa->supp, wpa->auth_addr, wpa->auth_eapol,
			wpa->auth_eapol_len);
}


static int supp_read_msg(struct wpa *wpa)
{
	os_free(wpa->auth_eapol);
	wpa->auth_eapol = read_msg(wpa, &wpa->auth_eapol_len);
	if (!wpa->auth_eapol)
		return -1;
	eloop_register_timeout(0, 0, supp_eapol_rx, wpa, NULL);
	return 0;
}


static int supp_ether_send(void *ctx, const u8 *dest, u16 proto, const u8 *buf,
			   size_t len)
{
	struct wpa *wpa = ctx;

	wpa_printf(MSG_DEBUG, "SUPP: %s(dest=" MACSTR " proto=0x%04x "
		   "len=%lu)",
		   __func__, MAC2STR(dest), proto, (unsigned long) len);

	return supp_read_msg(wpa);
}


static u8 * supp_alloc_eapol(void *ctx, u8 type, const void *data,
			     u16 data_len, size_t *msg_len, void **data_pos)
{
	struct ieee802_1x_hdr *hdr;

	wpa_printf(MSG_DEBUG, "SUPP: %s(type=%d data_len=%d)",
		   __func__, type, data_len);

	*msg_len = sizeof(*hdr) + data_len;
	hdr = os_malloc(*msg_len);
	if (hdr == NULL)
		return NULL;

	hdr->version = 2;
	hdr->type = type;
	hdr->length = host_to_be16(data_len);

	if (data)
		os_memcpy(hdr + 1, data, data_len);
	else
		os_memset(hdr + 1, 0, data_len);

	if (data_pos)
		*data_pos = hdr + 1;

	return (u8 *) hdr;
}


static int supp_get_beacon_ie(void *ctx)
{
	struct wpa *wpa = ctx;
	const u8 *ie;
	static const u8 wpaie[] = {
		0xdd, 0x16, 0x00, 0x50, 0xf2, 0x01, 0x01, 0x00,
		0x00, 0x50, 0xf2, 0x02, 0x01, 0x00, 0x00, 0x50,
		0xf2, 0x02, 0x01, 0x00, 0x00, 0x50, 0xf2, 0x02
	};
	static const u8 rsne[] = {
		0x30, 0x14, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x04,
		0x01, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x01, 0x00,
		0x00, 0x0f, 0xac, 0x02, 0xc0, 0x00
	};

	wpa_printf(MSG_DEBUG, "SUPP: %s", __func__);

	ie = wpa->wpa1 ? wpaie : rsne;
	if (ie[0] == WLAN_EID_RSN)
		return wpa_sm_set_ap_rsn_ie(wpa->supp, ie, 2 + ie[1]);
	return wpa_sm_set_ap_wpa_ie(wpa->supp, ie, 2 + ie[1]);
}


static int supp_set_key(void *ctx, enum wpa_alg alg,
			const u8 *addr, int key_idx, int set_tx,
			const u8 *seq, size_t seq_len,
			const u8 *key, size_t key_len, enum key_flag key_flag)
{
	wpa_printf(MSG_DEBUG, "SUPP: %s(alg=%d addr=" MACSTR " key_idx=%d "
		   "set_tx=%d key_flag=0x%x)",
		   __func__, alg, MAC2STR(addr), key_idx, set_tx, key_flag);
	wpa_hexdump(MSG_DEBUG, "SUPP: set_key - seq", seq, seq_len);
	wpa_hexdump(MSG_DEBUG, "SUPP: set_key - key", key, key_len);
	return 0;
}


static int supp_mlme_setprotection(void *ctx, const u8 *addr,
				   int protection_type, int key_type)
{
	wpa_printf(MSG_DEBUG, "SUPP: %s(addr=" MACSTR " protection_type=%d "
		   "key_type=%d)",
		   __func__, MAC2STR(addr), protection_type, key_type);
	return 0;
}


static void supp_cancel_auth_timeout(void *ctx)
{
	wpa_printf(MSG_DEBUG, "SUPP: %s", __func__);
}


static void * supp_get_network_ctx(void *ctx)
{
	return (void *) 1;
}


static void supp_deauthenticate(void *ctx, u16 reason_code)
{
	wpa_printf(MSG_DEBUG, "SUPP: %s(%d)", __func__, reason_code);
}


static enum wpa_states supp_get_state(void *ctx)
{
	return WPA_COMPLETED;
}


static int supp_init(struct wpa *wpa)
{
	struct wpa_sm_ctx *ctx = os_zalloc(sizeof(*ctx));

	if (!ctx)
		return -1;

	ctx->ctx = wpa;
	ctx->msg_ctx = wpa;
	ctx->set_state = supp_set_state;
	ctx->get_bssid = supp_get_bssid;
	ctx->ether_send = supp_ether_send;
	ctx->get_beacon_ie = supp_get_beacon_ie;
	ctx->alloc_eapol = supp_alloc_eapol;
	ctx->set_key = supp_set_key;
	ctx->mlme_setprotection = supp_mlme_setprotection;
	ctx->cancel_auth_timeout = supp_cancel_auth_timeout;
	ctx->get_network_ctx = supp_get_network_ctx;
	ctx->deauthenticate = supp_deauthenticate;
	ctx->get_state = supp_get_state;
	wpa->supp = wpa_sm_init(ctx);
	if (!wpa->supp) {
		wpa_printf(MSG_DEBUG, "SUPP: wpa_sm_init() failed");
		return -1;
	}

	wpa_sm_set_own_addr(wpa->supp, wpa->supp_addr);
	if (wpa->wpa1) {
		wpa_sm_set_param(wpa->supp, WPA_PARAM_RSN_ENABLED, 0);
		wpa_sm_set_param(wpa->supp, WPA_PARAM_PROTO, WPA_PROTO_WPA);
		wpa_sm_set_param(wpa->supp, WPA_PARAM_PAIRWISE,
				 WPA_CIPHER_TKIP);
		wpa_sm_set_param(wpa->supp, WPA_PARAM_GROUP, WPA_CIPHER_TKIP);
		wpa_sm_set_param(wpa->supp, WPA_PARAM_KEY_MGMT,
				 WPA_KEY_MGMT_PSK);
	} else {
		wpa_sm_set_param(wpa->supp, WPA_PARAM_RSN_ENABLED, 1);
		wpa_sm_set_param(wpa->supp, WPA_PARAM_PROTO, WPA_PROTO_RSN);
		wpa_sm_set_param(wpa->supp, WPA_PARAM_PAIRWISE,
				 WPA_CIPHER_CCMP);
		wpa_sm_set_param(wpa->supp, WPA_PARAM_GROUP, WPA_CIPHER_CCMP);
		wpa_sm_set_param(wpa->supp, WPA_PARAM_KEY_MGMT,
				 WPA_KEY_MGMT_PSK);
		wpa_sm_set_param(wpa->supp, WPA_PARAM_MFP,
				 MGMT_FRAME_PROTECTION_OPTIONAL);
	}
	wpa_sm_set_pmk(wpa->supp, wpa->psk, PMK_LEN, NULL, NULL);

	wpa->supp_ie_len = sizeof(wpa->supp_ie);
	if (wpa_sm_set_assoc_wpa_ie_default(wpa->supp, wpa->supp_ie,
					    &wpa->supp_ie_len) < 0) {
		wpa_printf(MSG_DEBUG, "SUPP: wpa_sm_set_assoc_wpa_ie_default()"
			   " failed");
		return -1;
	}

	wpa_sm_notify_assoc(wpa->supp, wpa->auth_addr);
	supp_read_msg(wpa);

	return 0;
}


static void deinit(struct wpa *wpa)
{
	wpa_sm_deinit(wpa->supp);
	os_free(wpa->auth_eapol);
	wpa->auth_eapol = NULL;
}


int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	struct wpa wpa;

	wpa_fuzzer_set_debug_level();

	if (os_program_init())
		return -1;

	os_memset(&wpa, 0, sizeof(wpa));
	wpa.data = data;
	wpa.data_len = size;

	os_memset(wpa.auth_addr, 0x12, ETH_ALEN);
	os_memset(wpa.supp_addr, 0x32, ETH_ALEN);
	os_memset(wpa.psk, 0x44, PMK_LEN);

	if (eloop_init()) {
		wpa_printf(MSG_ERROR, "Failed to initialize event loop");
		goto fail;
	}

	if (supp_init(&wpa) < 0)
		goto fail;

	wpa_printf(MSG_DEBUG, "Starting eloop");
	eloop_run();
	wpa_printf(MSG_DEBUG, "eloop done");

fail:
	deinit(&wpa);

	eloop_destroy();

	os_program_deinit();

	return 0;
}
