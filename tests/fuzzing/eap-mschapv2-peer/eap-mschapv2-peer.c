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


static struct eap_peer_config eap_mschapv2_config = {
	.identity = (u8 *) "user",
	.identity_len = 4,
	.password = (u8 *) "password",
	.password_len = 8,
};

struct eap_peer_config * eap_get_config(struct eap_sm *sm)
{
	return &eap_mschapv2_config;
}


const u8 * eap_get_config_identity(struct eap_sm *sm, size_t *len)
{
	static const char *id = "user";

	*len = os_strlen(id);
	return (const u8 *) id;
}


const u8 * eap_get_config_password(struct eap_sm *sm, size_t *len)
{
	struct eap_peer_config *config = eap_get_config(sm);

	*len = config->password_len;
	return config->password;
}


const u8 * eap_get_config_password2(struct eap_sm *sm, size_t *len, int *hash)
{
	struct eap_peer_config *config = eap_get_config(sm);

	*len = config->password_len;
	if (hash)
		*hash = !!(config->flags & EAP_CONFIG_FLAGS_PASSWORD_NTHASH);
	return config->password;
}


const u8 * eap_get_config_new_password(struct eap_sm *sm, size_t *len)
{
	*len = 3;
	return (const u8 *) "new";
}


void eap_sm_request_identity(struct eap_sm *sm)
{
}


void eap_sm_request_password(struct eap_sm *sm)
{
}


void eap_sm_request_new_password(struct eap_sm *sm)
{
}


int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	const u8 *pos, *end;
	struct eap_sm *sm;
	void *priv;
	struct eap_method_ret ret;

	wpa_fuzzer_set_debug_level();

	eap_peer_mschapv2_register();
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
