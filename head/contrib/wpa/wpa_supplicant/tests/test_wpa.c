/*
 * Test program for combined WPA authenticator/supplicant
 * Copyright (c) 2006-2007, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "eloop.h"
#include "common/ieee802_11_defs.h"
#include "../config.h"
#include "rsn_supp/wpa.h"
#include "rsn_supp/wpa_ie.h"
#include "ap/wpa_auth.h"


struct wpa {
	u8 auth_addr[ETH_ALEN];
	u8 supp_addr[ETH_ALEN];
	u8 psk[PMK_LEN];

	/* from authenticator */
	u8 auth_eapol_dst[ETH_ALEN];
	u8 *auth_eapol;
	size_t auth_eapol_len;

	/* from supplicant */
	u8 *supp_eapol;
	size_t supp_eapol_len;

	struct wpa_sm *supp;
	struct wpa_authenticator *auth_group;
	struct wpa_state_machine *auth;

	struct wpa_ssid ssid;
	u8 supp_ie[80];
	size_t supp_ie_len;
};


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


static void auth_eapol_rx(void *eloop_data, void *user_ctx)
{
	struct wpa *wpa = eloop_data;

	wpa_printf(MSG_DEBUG, "AUTH: RX EAPOL frame");
	wpa_receive(wpa->auth_group, wpa->auth, wpa->supp_eapol,
		    wpa->supp_eapol_len);
}


static int supp_ether_send(void *ctx, const u8 *dest, u16 proto, const u8 *buf,
			   size_t len)
{
	struct wpa *wpa = ctx;

	wpa_printf(MSG_DEBUG, "SUPP: %s(dest=" MACSTR " proto=0x%04x "
		   "len=%lu)",
		   __func__, MAC2STR(dest), proto, (unsigned long) len);

	os_free(wpa->supp_eapol);
	wpa->supp_eapol = os_malloc(len);
	if (wpa->supp_eapol == NULL)
		return -1;
	os_memcpy(wpa->supp_eapol, buf, len);
	wpa->supp_eapol_len = len;
	eloop_register_timeout(0, 0, auth_eapol_rx, wpa, NULL);

	return 0;
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
	size_t ielen;

	wpa_printf(MSG_DEBUG, "SUPP: %s", __func__);

	ie = wpa_auth_get_wpa_ie(wpa->auth_group, &ielen);
	if (ie == NULL || ielen < 1)
		return -1;
	if (ie[0] == WLAN_EID_RSN)
		return wpa_sm_set_ap_rsn_ie(wpa->supp, ie, 2 + ie[1]);
	return wpa_sm_set_ap_wpa_ie(wpa->supp, ie, 2 + ie[1]);
}


static int supp_set_key(void *ctx, enum wpa_alg alg,
			const u8 *addr, int key_idx, int set_tx,
			const u8 *seq, size_t seq_len,
			const u8 *key, size_t key_len)
{
	wpa_printf(MSG_DEBUG, "SUPP: %s(alg=%d addr=" MACSTR " key_idx=%d "
		   "set_tx=%d)",
		   __func__, alg, MAC2STR(addr), key_idx, set_tx);
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


static int supp_init(struct wpa *wpa)
{
	struct wpa_sm_ctx *ctx = os_zalloc(sizeof(*ctx));
	if (ctx == NULL)
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
	wpa->supp = wpa_sm_init(ctx);
	if (wpa->supp == NULL) {
		wpa_printf(MSG_DEBUG, "SUPP: wpa_sm_init() failed");
		return -1;
	}

	wpa_sm_set_own_addr(wpa->supp, wpa->supp_addr);
	wpa_sm_set_param(wpa->supp, WPA_PARAM_RSN_ENABLED, 1);
	wpa_sm_set_param(wpa->supp, WPA_PARAM_PROTO, WPA_PROTO_RSN);
	wpa_sm_set_param(wpa->supp, WPA_PARAM_PAIRWISE, WPA_CIPHER_CCMP);
	wpa_sm_set_param(wpa->supp, WPA_PARAM_GROUP, WPA_CIPHER_CCMP);
	wpa_sm_set_param(wpa->supp, WPA_PARAM_KEY_MGMT, WPA_KEY_MGMT_PSK);
	wpa_sm_set_pmk(wpa->supp, wpa->psk, PMK_LEN);

	wpa->supp_ie_len = sizeof(wpa->supp_ie);
	if (wpa_sm_set_assoc_wpa_ie_default(wpa->supp, wpa->supp_ie,
					    &wpa->supp_ie_len) < 0) {
		wpa_printf(MSG_DEBUG, "SUPP: wpa_sm_set_assoc_wpa_ie_default()"
			   " failed");
		return -1;
	}

	wpa_sm_notify_assoc(wpa->supp, wpa->auth_addr);

	return 0;
}


static void auth_logger(void *ctx, const u8 *addr, logger_level level,
			const char *txt)
{
	if (addr)
		wpa_printf(MSG_DEBUG, "AUTH: " MACSTR " - %s",
			   MAC2STR(addr), txt);
	else
		wpa_printf(MSG_DEBUG, "AUTH: %s", txt);
}


static void supp_eapol_rx(void *eloop_data, void *user_ctx)
{
	struct wpa *wpa = eloop_data;

	wpa_printf(MSG_DEBUG, "SUPP: RX EAPOL frame");
	wpa_sm_rx_eapol(wpa->supp, wpa->auth_addr, wpa->auth_eapol,
			wpa->auth_eapol_len);
}


static int auth_send_eapol(void *ctx, const u8 *addr, const u8 *data,
			   size_t data_len, int encrypt)
{
	struct wpa *wpa = ctx;

	wpa_printf(MSG_DEBUG, "AUTH: %s(addr=" MACSTR " data_len=%lu "
		   "encrypt=%d)",
		   __func__, MAC2STR(addr), (unsigned long) data_len, encrypt);

	os_free(wpa->auth_eapol);
	wpa->auth_eapol = os_malloc(data_len);
	if (wpa->auth_eapol == NULL)
		return -1;
	os_memcpy(wpa->auth_eapol_dst, addr, ETH_ALEN);
	os_memcpy(wpa->auth_eapol, data, data_len);
	wpa->auth_eapol_len = data_len;
	eloop_register_timeout(0, 0, supp_eapol_rx, wpa, NULL);

	return 0;
}


static const u8 * auth_get_psk(void *ctx, const u8 *addr, const u8 *prev_psk)
{
	struct wpa *wpa = ctx;
	wpa_printf(MSG_DEBUG, "AUTH: %s (addr=" MACSTR " prev_psk=%p)",
		   __func__, MAC2STR(addr), prev_psk);
	if (prev_psk)
		return NULL;
	return wpa->psk;
}


static int auth_init_group(struct wpa *wpa)
{
	struct wpa_auth_config conf;
	struct wpa_auth_callbacks cb;

	wpa_printf(MSG_DEBUG, "AUTH: Initializing group state machine");

	os_memset(&conf, 0, sizeof(conf));
	conf.wpa = 2;
	conf.wpa_key_mgmt = WPA_KEY_MGMT_PSK;
	conf.wpa_pairwise = WPA_CIPHER_CCMP;
	conf.rsn_pairwise = WPA_CIPHER_CCMP;
	conf.wpa_group = WPA_CIPHER_CCMP;
	conf.eapol_version = 2;

	os_memset(&cb, 0, sizeof(cb));
	cb.ctx = wpa;
	cb.logger = auth_logger;
	cb.send_eapol = auth_send_eapol;
	cb.get_psk = auth_get_psk;

	wpa->auth_group = wpa_init(wpa->auth_addr, &conf, &cb);
	if (wpa->auth_group == NULL) {
		wpa_printf(MSG_DEBUG, "AUTH: wpa_init() failed");
		return -1;
	}

	return 0;
}


static int auth_init(struct wpa *wpa)
{
	wpa->auth = wpa_auth_sta_init(wpa->auth_group, wpa->supp_addr, NULL);
	if (wpa->auth == NULL) {
		wpa_printf(MSG_DEBUG, "AUTH: wpa_auth_sta_init() failed");
		return -1;
	}

	if (wpa_validate_wpa_ie(wpa->auth_group, wpa->auth, wpa->supp_ie,
				wpa->supp_ie_len, NULL, 0) != WPA_IE_OK) {
		wpa_printf(MSG_DEBUG, "AUTH: wpa_validate_wpa_ie() failed");
		return -1;
	}

	wpa_auth_sm_event(wpa->auth, WPA_ASSOC);

	wpa_auth_sta_associated(wpa->auth_group, wpa->auth);

	return 0;
}


static void deinit(struct wpa *wpa)
{
	wpa_auth_sta_deinit(wpa->auth);
	wpa_sm_deinit(wpa->supp);
	wpa_deinit(wpa->auth_group);
	os_free(wpa->auth_eapol);
	wpa->auth_eapol = NULL;
	os_free(wpa->supp_eapol);
	wpa->supp_eapol = NULL;
}


int main(int argc, char *argv[])
{
	struct wpa wpa;

	if (os_program_init())
		return -1;

	os_memset(&wpa, 0, sizeof(wpa));
	os_memset(wpa.auth_addr, 0x12, ETH_ALEN);
	os_memset(wpa.supp_addr, 0x32, ETH_ALEN);
	os_memset(wpa.psk, 0x44, PMK_LEN);

	wpa_debug_level = 0;
	wpa_debug_show_keys = 1;

	if (eloop_init()) {
		wpa_printf(MSG_ERROR, "Failed to initialize event loop");
		return -1;
	}

	if (auth_init_group(&wpa) < 0)
		return -1;

	if (supp_init(&wpa) < 0)
		return -1;

	if (auth_init(&wpa) < 0)
		return -1;

	wpa_printf(MSG_DEBUG, "Starting eloop");
	eloop_run();
	wpa_printf(MSG_DEBUG, "eloop done");

	deinit(&wpa);

	eloop_destroy();

	os_program_deinit();

	return 0;
}
