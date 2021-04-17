/*
 * Testing tool for TLSv1 client routines
 * Copyright (c) 2019, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "crypto/tls.h"
#include "../fuzzer-common.h"

#ifndef CERTDIR
#define CERTDIR "../../hwsim/auth_serv/"
#endif

struct context {
	const u8 *data;
	size_t data_len;
	size_t data_offset;
};


static struct wpabuf * read_msg(struct context *ctx)
{
	u16 msg_len;
	struct wpabuf *msg;

	if (ctx->data_len - ctx->data_offset < 2) {
		wpa_printf(MSG_ERROR, "TEST-ERROR: Could not read msg len");
		return NULL;
	}
	msg_len = WPA_GET_BE16(&ctx->data[ctx->data_offset]);
	ctx->data_offset += 2;

	msg = wpabuf_alloc(msg_len);
	if (!msg)
		return NULL;
	if (msg_len > 0 && ctx->data_len - ctx->data_offset < msg_len) {
		wpa_printf(MSG_ERROR, "TEST-ERROR: Truncated msg (msg_len=%u)",
			   msg_len);
		wpabuf_free(msg);
		return NULL;
	}
	wpabuf_put_data(msg, &ctx->data[ctx->data_offset], msg_len);
	ctx->data_offset += msg_len;
	wpa_hexdump_buf(MSG_DEBUG, "TEST: Read message from file", msg);

	return msg;
}


int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	struct context ctx;
	struct tls_config conf;
	void *tls_client;
	struct tls_connection_params params;
	struct tls_connection *conn_client = NULL;
	int ret = -1;
	struct wpabuf *in = NULL, *out = NULL, *appl;

	wpa_fuzzer_set_debug_level();

	os_memset(&ctx, 0, sizeof(ctx));
	ctx.data = data;
	ctx.data_len = size;

	os_memset(&conf, 0, sizeof(conf));
	tls_client = tls_init(&conf);
	if (!tls_client)
		goto fail;

	os_memset(&params, 0, sizeof(params));
	params.ca_cert = CERTDIR "ca.pem";
	params.client_cert = CERTDIR "server.pem";
	params.private_key = CERTDIR "server.key";
	params.dh_file = CERTDIR "dh.conf";

	conn_client = tls_connection_init(tls_client);
	if (!conn_client)
		goto fail;

	in = NULL;
	for (;;) {
		appl = NULL;
		out = tls_connection_handshake(tls_client, conn_client, in,
					       &appl);
		wpabuf_free(in);
		in = NULL;
		if (!out)
			goto fail;
		if (tls_connection_get_failed(tls_client, conn_client)) {
			wpa_printf(MSG_ERROR, "TLS handshake failed");
			goto fail;
		}
		if (tls_connection_established(tls_client, conn_client))
			break;

		appl = NULL;
		in = read_msg(&ctx);
		wpabuf_free(out);
		out = NULL;
		if (!in)
			goto fail;
		if (tls_connection_established(tls_client, conn_client))
			break;
	}

	wpabuf_free(in);
	in = wpabuf_alloc(100);
	if (!in)
		goto fail;
	wpabuf_put_str(in, "PING");
	wpabuf_free(out);
	out = tls_connection_encrypt(tls_client, conn_client, in);
	wpabuf_free(in);
	in = NULL;
	if (!out)
		goto fail;

	wpabuf_free(in);
	in = wpabuf_alloc(100);
	if (!in)
		goto fail;
	wpabuf_put_str(in, "PONG");
	wpabuf_free(out);
	out = read_msg(&ctx);
	wpabuf_free(in);
	in = NULL;
	if (!out)
		goto fail;

	in = tls_connection_decrypt(tls_client, conn_client, out);
	wpabuf_free(out);
	out = NULL;
	if (!in)
		goto fail;
	wpa_hexdump_buf(MSG_DEBUG, "Client decrypted ApplData", in);

	ret = 0;
fail:
	if (tls_client) {
		if (conn_client)
			tls_connection_deinit(tls_client, conn_client);
		tls_deinit(tls_client);
	}
	wpabuf_free(in);
	wpabuf_free(out);

	return ret;
}
