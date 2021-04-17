/*
 * wpa_supplicant - EAPOL fuzzer
 * Copyright (c) 2015-2019, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "utils/eloop.h"
#include "eapol_supp/eapol_supp_sm.h"
#include "rsn_supp/wpa.h"
#include "rsn_supp/wpa_i.h"
#include "../fuzzer-common.h"


struct arg_ctx {
	const u8 *data;
	size_t data_len;
	struct wpa_sm *wpa;
	struct eapol_sm *eapol;
};


static void test_send_eapol(void *eloop_data, void *user_ctx)
{
	struct arg_ctx *ctx = eloop_data;
	u8 src[ETH_ALEN] = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 };
	u8 wpa_ie[200];
	size_t wpa_ie_len;

	wpa_hexdump(MSG_MSGDUMP, "fuzzer - EAPOL", ctx->data, ctx->data_len);

	eapol_sm_notify_portEnabled(ctx->eapol, true);

	wpa_sm_set_param(ctx->wpa, WPA_PARAM_PROTO, WPA_PROTO_RSN);
	wpa_sm_set_param(ctx->wpa, WPA_PARAM_RSN_ENABLED, 1);
	wpa_sm_set_param(ctx->wpa, WPA_PARAM_KEY_MGMT, WPA_KEY_MGMT_PSK);
	wpa_sm_set_param(ctx->wpa, WPA_PARAM_PAIRWISE, WPA_CIPHER_CCMP);
	wpa_sm_set_param(ctx->wpa, WPA_PARAM_GROUP, WPA_CIPHER_CCMP);

	wpa_ie_len = sizeof(wpa_ie);
	wpa_sm_set_assoc_wpa_ie_default(ctx->wpa, wpa_ie, &wpa_ie_len);

	if (eapol_sm_rx_eapol(ctx->eapol, src, ctx->data, ctx->data_len) <= 0)
		wpa_sm_rx_eapol(ctx->wpa, src, ctx->data, ctx->data_len);

	eloop_terminate();
}


static void * get_network_ctx(void *arg)
{
	return (void *) 1;
}


static void set_state(void *arg, enum wpa_states state)
{
}


static void deauthenticate(void *arg, u16 reason_code)
{
}


static u8 * alloc_eapol(void *arg, u8 type,
			const void *data, u16 data_len,
			size_t *msg_len, void **data_pos)
{
	struct ieee802_1x_hdr *hdr;

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


static int ether_send(void *arg, const u8 *dest, u16 proto,
		      const u8 *buf, size_t len)
{
	return 0;
}


static int get_bssid(void *ctx, u8 *bssid)
{
	return -1;
}


static int eapol_send(void *ctx, int type, const u8 *buf, size_t len)
{
	return 0;
}


static int init_wpa(struct arg_ctx *arg)
{
	struct wpa_sm_ctx *ctx;

	ctx = os_zalloc(sizeof(*ctx));
	if (ctx == NULL) {
		wpa_printf(MSG_ERROR, "Failed to allocate WPA context.");
		return -1;
	}

	ctx->ctx = arg;
	ctx->msg_ctx = arg;
	ctx->get_network_ctx = get_network_ctx;
	ctx->set_state = set_state;
	ctx->deauthenticate = deauthenticate;
	ctx->alloc_eapol = alloc_eapol;
	ctx->ether_send = ether_send;
	ctx->get_bssid = get_bssid;

	arg->wpa = wpa_sm_init(ctx);
	if (!arg->wpa)
		return -1;
	arg->wpa->pmk_len = PMK_LEN;
	return 0;
}


static int init_eapol(struct arg_ctx *arg)
{
	struct eapol_ctx *ctx;

	ctx = os_zalloc(sizeof(*ctx));
	if (ctx == NULL) {
		wpa_printf(MSG_ERROR, "Failed to allocate EAPOL context.");
		return -1;
	}

	ctx->ctx = arg;
	ctx->msg_ctx = arg;
	ctx->eapol_send = eapol_send;

	arg->eapol = eapol_sm_init(ctx);
	return arg->eapol ? 0 : -1;
}


int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	struct arg_ctx ctx;

	wpa_fuzzer_set_debug_level();

	if (os_program_init())
		return 0;

	if (eloop_init()) {
		wpa_printf(MSG_ERROR, "Failed to initialize event loop");
		return 0;
	}

	os_memset(&ctx, 0, sizeof(ctx));
	ctx.data = data;
	ctx.data_len = size;
	if (init_wpa(&ctx) || init_eapol(&ctx))
		goto fail;

	eloop_register_timeout(0, 0, test_send_eapol, &ctx, NULL);

	wpa_printf(MSG_DEBUG, "Starting eloop");
	eloop_run();
	wpa_printf(MSG_DEBUG, "eloop done");

fail:
	if (ctx.wpa)
		wpa_sm_deinit(ctx.wpa);
	if (ctx.eapol)
		eapol_sm_deinit(ctx.eapol);

	eloop_destroy();
	os_program_deinit();

	return 0;
}
