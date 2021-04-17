/*
 * EAP-SIM peer fuzzer
 * Copyright (c) 2019, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "utils/includes.h"

#include "utils/common.h"
#include "eap_peer/eap_methods.h"
#include "eap_peer/eap_config.h"
#include "eap_peer/eap_i.h"
#include "../fuzzer-common.h"

int eap_peer_sim_register(void);

struct eap_method * registered_eap_method = NULL;


struct eap_method * eap_peer_method_alloc(int version, int vendor,
					  enum eap_type method,
					  const char *name)
{
	struct eap_method *eap;
	eap = os_zalloc(sizeof(*eap));
	if (!eap)
		return NULL;
	eap->version = version;
	eap->vendor = vendor;
	eap->method = method;
	eap->name = name;
	return eap;
}


int eap_peer_method_register(struct eap_method *method)
{
	registered_eap_method = method;
	return 0;
}


static struct eap_peer_config eap_sim_config = {
	.identity = (u8 *) "1232010000000000",
	.identity_len = 16,
	.password = (u8 *) "90dca4eda45b53cf0f12d7c9c3bc6a89:cb9cccc4b9258e6dca4760379fb82581",
	.password_len = 65,
};

struct eap_peer_config * eap_get_config(struct eap_sm *sm)
{
	return &eap_sim_config;
}


const u8 * eap_get_config_identity(struct eap_sm *sm, size_t *len)
{
	static const char *id = "1232010000000000";

	*len = os_strlen(id);
	return (const u8 *) id;
}


void eap_set_anon_id(struct eap_sm *sm, const u8 *id, size_t len)
{
}


void eap_sm_request_identity(struct eap_sm *sm)
{
}


void eap_sm_request_sim(struct eap_sm *sm, const char *req)
{
}


int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	const u8 *pos, *end;
	struct eap_sm *sm;
	void *priv;
	struct eap_method_ret ret;

	wpa_fuzzer_set_debug_level();

	eap_peer_sim_register();
	sm = os_zalloc(sizeof(*sm));
	if (!sm)
		return 0;
	priv = registered_eap_method->init(sm);
	os_memset(&ret, 0, sizeof(ret));

	pos = data;
	end = pos + size;

	while (end - pos > 2) {
		u16 flen;
		struct wpabuf *buf, *req;

		flen = WPA_GET_BE16(pos);
		pos += 2;
		if (end - pos < flen)
			break;
		req = wpabuf_alloc_copy(pos, flen);
		if (!req)
			break;
		wpa_hexdump_buf(MSG_MSGDUMP, "fuzzer - request", req);
		buf = registered_eap_method->process(sm, priv, &ret, req);
		wpa_hexdump_buf(MSG_MSGDUMP, "fuzzer - local response", buf);
		wpabuf_free(req);
		wpabuf_free(buf);
		pos += flen;
	}

	registered_eap_method->deinit(sm, priv);
	os_free(registered_eap_method);
	os_free(sm);

	return 0;
}
