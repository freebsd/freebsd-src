/*
 * Testing tool for EAPOL-Key Authenticator routines
 * Copyright (c) 2006-2019, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/eloop.h"
#include "ap/wpa_auth.h"
#include "../fuzzer-common.h"


struct wpa {
	const u8 *data;
	size_t data_len;
	size_t data_offset;
	int wpa1;

	u8 auth_addr[ETH_ALEN];
	u8 supp_addr[ETH_ALEN];
	u8 psk[PMK_LEN];

	/* from supplicant */
	u8 *supp_eapol;
	size_t supp_eapol_len;

	struct wpa_auth_callbacks auth_cb;
	struct wpa_authenticator *auth_group;
	struct wpa_state_machine *auth;

	u8 supp_ie[80];
	size_t supp_ie_len;

	int key_request_done;
	int key_request_done1;
	int auth_sent;
};


const struct wpa_driver_ops *const wpa_drivers[] = { NULL };


static int auth_read_msg(struct wpa *wpa);
static void supp_eapol_key_request(void *eloop_data, void *user_ctx);


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


static void auth_eapol_rx(void *eloop_data, void *user_ctx)
{
	struct wpa *wpa = eloop_data;

	wpa_printf(MSG_DEBUG, "AUTH: RX EAPOL frame");
	wpa->auth_sent = 0;
	wpa_receive(wpa->auth_group, wpa->auth, wpa->supp_eapol,
		    wpa->supp_eapol_len);
	if (!wpa->auth_sent) {
		/* Speed up process by not going through retransmit timeout */
		wpa_printf(MSG_DEBUG,
			   "AUTH: No response was sent - process next message");
		auth_read_msg(wpa);
	}
	if (wpa->wpa1 && wpa->key_request_done && !wpa->key_request_done1) {
		wpa->key_request_done1 = 1;
		eloop_register_timeout(0, 0, supp_eapol_key_request,
				       wpa, NULL);
	}

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


static int auth_read_msg(struct wpa *wpa)
{
	os_free(wpa->supp_eapol);
	wpa->supp_eapol = read_msg(wpa, &wpa->supp_eapol_len);
	if (!wpa->supp_eapol)
		return -1;
	eloop_register_timeout(0, 0, auth_eapol_rx, wpa, NULL);
	return 0;
}


static int auth_send_eapol(void *ctx, const u8 *addr, const u8 *data,
			   size_t data_len, int encrypt)
{
	struct wpa *wpa = ctx;

	wpa_printf(MSG_DEBUG, "AUTH: %s(addr=" MACSTR " data_len=%lu "
		   "encrypt=%d)",
		   __func__, MAC2STR(addr), (unsigned long) data_len, encrypt);
	wpa->auth_sent = 1;

	return auth_read_msg(wpa);
}


static const u8 * auth_get_psk(void *ctx, const u8 *addr,
			       const u8 *p2p_dev_addr, const u8 *prev_psk,
			       size_t *psk_len, int *vlan_id)
{
	struct wpa *wpa = ctx;

	wpa_printf(MSG_DEBUG, "AUTH: %s (addr=" MACSTR " prev_psk=%p)",
		   __func__, MAC2STR(addr), prev_psk);
	if (vlan_id)
		*vlan_id = 0;
	if (psk_len)
		*psk_len = PMK_LEN;
	if (prev_psk)
		return NULL;
	return wpa->psk;
}


static void supp_eapol_key_request(void *eloop_data, void *user_ctx)
{
	struct wpa *wpa = eloop_data;

	wpa_printf(MSG_DEBUG, "SUPP: EAPOL-Key Request trigger");
	if (!eloop_is_timeout_registered(auth_eapol_rx, wpa, NULL))
		auth_read_msg(wpa);
}


static int auth_set_key(void *ctx, int vlan_id, enum wpa_alg alg,
			const u8 *addr, int idx, u8 *key,
			size_t key_len, enum key_flag key_flag)
{
	struct wpa *wpa = ctx;

	wpa_printf(MSG_DEBUG,
		   "AUTH: %s (vlan_id=%d alg=%d idx=%d key_len=%d key_flag=0x%x)",
		   __func__, vlan_id, alg, idx, (int) key_len, key_flag);
	if (addr)
		wpa_printf(MSG_DEBUG, "AUTH: addr=" MACSTR, MAC2STR(addr));

	if (alg != WPA_ALG_NONE && idx == 0 && key_len > 0 &&
	    !wpa->key_request_done) {
		wpa_printf(MSG_DEBUG, "Test EAPOL-Key Request");
		wpa->key_request_done = 1;
		if (!wpa->wpa1)
			eloop_register_timeout(0, 0, supp_eapol_key_request,
					       wpa, NULL);
	}

	return 0;
}


static int auth_init_group(struct wpa *wpa)
{
	struct wpa_auth_config conf;

	wpa_printf(MSG_DEBUG, "AUTH: Initializing group state machine");

	os_memset(&conf, 0, sizeof(conf));
	if (wpa->wpa1) {
		conf.wpa = 1;
		conf.wpa_key_mgmt = WPA_KEY_MGMT_PSK;
		conf.wpa_pairwise = WPA_CIPHER_TKIP;
		conf.wpa_group = WPA_CIPHER_TKIP;
	} else {
		conf.wpa = 2;
		conf.wpa_key_mgmt = WPA_KEY_MGMT_PSK;
		conf.wpa_pairwise = WPA_CIPHER_CCMP;
		conf.rsn_pairwise = WPA_CIPHER_CCMP;
		conf.wpa_group = WPA_CIPHER_CCMP;
		conf.ieee80211w = 2;
		conf.group_mgmt_cipher = WPA_CIPHER_AES_128_CMAC;
	}
	conf.eapol_version = 2;
	conf.wpa_group_update_count = 4;
	conf.wpa_pairwise_update_count = 4;

	wpa->auth_cb.logger = auth_logger;
	wpa->auth_cb.send_eapol = auth_send_eapol;
	wpa->auth_cb.get_psk = auth_get_psk;
	wpa->auth_cb.set_key = auth_set_key;

	wpa->auth_group = wpa_init(wpa->auth_addr, &conf, &wpa->auth_cb, wpa);
	if (!wpa->auth_group) {
		wpa_printf(MSG_DEBUG, "AUTH: wpa_init() failed");
		return -1;
	}

	return 0;
}


static int auth_init(struct wpa *wpa)
{
	const u8 *supp_ie;
	size_t supp_ie_len;
	static const u8 ie_rsn[] = {
		0x30, 0x14, 0x01, 0x00, 0x00, 0x0f, 0xac, 0x04,
		0x01, 0x00, 0x00, 0x0f, 0xac, 0x04, 0x01, 0x00,
		0x00, 0x0f, 0xac, 0x02, 0x80, 0x00
	};
	static const u8 ie_wpa[] = {
		0xdd, 0x16, 0x00, 0x50, 0xf2, 0x01, 0x01, 0x00,
		0x00, 0x50, 0xf2, 0x02, 0x01, 0x00, 0x00, 0x50,
		0xf2, 0x02, 0x01, 0x00, 0x00, 0x50, 0xf2, 0x02
	};

	if (wpa->wpa1) {
		supp_ie = ie_wpa;
		supp_ie_len = sizeof(ie_wpa);
	} else {
		supp_ie = ie_rsn;
		supp_ie_len = sizeof(ie_rsn);
	}

	wpa->auth = wpa_auth_sta_init(wpa->auth_group, wpa->supp_addr, NULL);
	if (!wpa->auth) {
		wpa_printf(MSG_DEBUG, "AUTH: wpa_auth_sta_init() failed");
		return -1;
	}

	if (wpa_validate_wpa_ie(wpa->auth_group, wpa->auth, 2412, supp_ie,
				supp_ie_len, NULL, 0, NULL, 0, NULL, 0) !=
	    WPA_IE_OK) {
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
	wpa_deinit(wpa->auth_group);
	os_free(wpa->supp_eapol);
	wpa->supp_eapol = NULL;
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

	if (auth_init_group(&wpa) < 0)
		goto fail;

	if (auth_init(&wpa) < 0)
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
