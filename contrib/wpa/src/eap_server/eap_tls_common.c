/*
 * hostapd / EAP-TLS/PEAP/TTLS/FAST common functions
 * Copyright (c) 2004-2008, Jouni Malinen <j@w1.fi>
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
#include "eap_i.h"
#include "eap_tls_common.h"
#include "sha1.h"
#include "tls.h"


int eap_server_tls_ssl_init(struct eap_sm *sm, struct eap_ssl_data *data,
			    int verify_peer)
{
	data->eap = sm;
	data->phase2 = sm->init_phase2;

	data->conn = tls_connection_init(sm->ssl_ctx);
	if (data->conn == NULL) {
		wpa_printf(MSG_INFO, "SSL: Failed to initialize new TLS "
			   "connection");
		return -1;
	}

	if (tls_connection_set_verify(sm->ssl_ctx, data->conn, verify_peer)) {
		wpa_printf(MSG_INFO, "SSL: Failed to configure verification "
			   "of TLS peer certificate");
		tls_connection_deinit(sm->ssl_ctx, data->conn);
		data->conn = NULL;
		return -1;
	}

	/* TODO: make this configurable */
	data->tls_out_limit = 1398;
	if (data->phase2) {
		/* Limit the fragment size in the inner TLS authentication
		 * since the outer authentication with EAP-PEAP does not yet
		 * support fragmentation */
		if (data->tls_out_limit > 100)
			data->tls_out_limit -= 100;
	}
	return 0;
}


void eap_server_tls_ssl_deinit(struct eap_sm *sm, struct eap_ssl_data *data)
{
	tls_connection_deinit(sm->ssl_ctx, data->conn);
	os_free(data->in_buf);
	os_free(data->out_buf);
}


u8 * eap_server_tls_derive_key(struct eap_sm *sm, struct eap_ssl_data *data,
			       char *label, size_t len)
{
	struct tls_keys keys;
	u8 *rnd = NULL, *out;

	out = os_malloc(len);
	if (out == NULL)
		return NULL;

	if (tls_connection_prf(sm->ssl_ctx, data->conn, label, 0, out, len) ==
	    0)
		return out;

	if (tls_connection_get_keys(sm->ssl_ctx, data->conn, &keys))
		goto fail;

	if (keys.client_random == NULL || keys.server_random == NULL ||
	    keys.master_key == NULL)
		goto fail;

	rnd = os_malloc(keys.client_random_len + keys.server_random_len);
	if (rnd == NULL)
		goto fail;
	os_memcpy(rnd, keys.client_random, keys.client_random_len);
	os_memcpy(rnd + keys.client_random_len, keys.server_random,
		  keys.server_random_len);

	if (tls_prf(keys.master_key, keys.master_key_len,
		    label, rnd, keys.client_random_len +
		    keys.server_random_len, out, len))
		goto fail;

	os_free(rnd);
	return out;

fail:
	os_free(out);
	os_free(rnd);
	return NULL;
}


struct wpabuf * eap_server_tls_build_msg(struct eap_ssl_data *data,
					 int eap_type, int version, u8 id)
{
	struct wpabuf *req;
	u8 flags;
	size_t send_len, plen;

	wpa_printf(MSG_DEBUG, "SSL: Generating Request");
	if (data->out_buf == NULL) {
		wpa_printf(MSG_ERROR, "SSL: out_buf NULL in %s", __func__);
		return NULL;
	}

	flags = version;
	send_len = wpabuf_len(data->out_buf) - data->out_used;
	if (1 + send_len > data->tls_out_limit) {
		send_len = data->tls_out_limit - 1;
		flags |= EAP_TLS_FLAGS_MORE_FRAGMENTS;
		if (data->out_used == 0) {
			flags |= EAP_TLS_FLAGS_LENGTH_INCLUDED;
			send_len -= 4;
		}
	}

	plen = 1 + send_len;
	if (flags & EAP_TLS_FLAGS_LENGTH_INCLUDED)
		plen += 4;

	req = eap_msg_alloc(EAP_VENDOR_IETF, eap_type, plen,
			    EAP_CODE_REQUEST, id);
	if (req == NULL)
		return NULL;

	wpabuf_put_u8(req, flags); /* Flags */
	if (flags & EAP_TLS_FLAGS_LENGTH_INCLUDED)
		wpabuf_put_be32(req, wpabuf_len(data->out_buf));

	wpabuf_put_data(req, wpabuf_head_u8(data->out_buf) + data->out_used,
			send_len);
	data->out_used += send_len;

	if (data->out_used == wpabuf_len(data->out_buf)) {
		wpa_printf(MSG_DEBUG, "SSL: Sending out %lu bytes "
			   "(message sent completely)",
			   (unsigned long) send_len);
		wpabuf_free(data->out_buf);
		data->out_buf = NULL;
		data->out_used = 0;
		data->state = MSG;
	} else {
		wpa_printf(MSG_DEBUG, "SSL: Sending out %lu bytes "
			   "(%lu more to send)", (unsigned long) send_len,
			   (unsigned long) wpabuf_len(data->out_buf) -
			   data->out_used);
		data->state = WAIT_FRAG_ACK;
	}

	return req;
}


struct wpabuf * eap_server_tls_build_ack(u8 id, int eap_type, int version)
{
	struct wpabuf *req;

	req = eap_msg_alloc(EAP_VENDOR_IETF, eap_type, 1, EAP_CODE_REQUEST,
			    id);
	if (req == NULL)
		return NULL;
	wpa_printf(MSG_DEBUG, "SSL: Building ACK");
	wpabuf_put_u8(req, version); /* Flags */
	return req;
}


static int eap_server_tls_process_cont(struct eap_ssl_data *data,
				       const u8 *buf, size_t len)
{
	/* Process continuation of a pending message */
	if (len > wpabuf_tailroom(data->in_buf)) {
		wpa_printf(MSG_DEBUG, "SSL: Fragment overflow");
		return -1;
	}

	wpabuf_put_data(data->in_buf, buf, len);
	wpa_printf(MSG_DEBUG, "SSL: Received %lu bytes, waiting for %lu "
		   "bytes more", (unsigned long) len,
		   (unsigned long) wpabuf_tailroom(data->in_buf));

	return 0;
}


static int eap_server_tls_process_fragment(struct eap_ssl_data *data,
					   u8 flags, u32 message_length,
					   const u8 *buf, size_t len)
{
	/* Process a fragment that is not the last one of the message */
	if (data->in_buf == NULL && !(flags & EAP_TLS_FLAGS_LENGTH_INCLUDED)) {
		wpa_printf(MSG_DEBUG, "SSL: No Message Length field in a "
			   "fragmented packet");
		return -1;
	}

	if (data->in_buf == NULL) {
		/* First fragment of the message */

		/* Limit length to avoid rogue peers from causing large
		 * memory allocations. */
		if (message_length > 65536) {
			wpa_printf(MSG_INFO, "SSL: Too long TLS fragment (size"
				   " over 64 kB)");
			return -1;
		}

		data->in_buf = wpabuf_alloc(message_length);
		if (data->in_buf == NULL) {
			wpa_printf(MSG_DEBUG, "SSL: No memory for message");
			return -1;
		}
		wpabuf_put_data(data->in_buf, buf, len);
		wpa_printf(MSG_DEBUG, "SSL: Received %lu bytes in first "
			   "fragment, waiting for %lu bytes more",
			   (unsigned long) len,
			   (unsigned long) wpabuf_tailroom(data->in_buf));
	}

	return 0;
}


int eap_server_tls_phase1(struct eap_sm *sm, struct eap_ssl_data *data)
{
	u8 *next;
	size_t next_len;

	next = tls_connection_server_handshake(
		sm->ssl_ctx, data->conn,
		wpabuf_mhead(data->in_buf),
		wpabuf_len(data->in_buf),
		&next_len);
	if (next == NULL) {
		wpa_printf(MSG_INFO, "SSL: TLS processing failed");
		return -1;
	}
	if (data->out_buf) {
		/* This should not happen.. */
		wpa_printf(MSG_INFO, "SSL: pending tls_out data when "
			   "processing new message");
		os_free(data->out_buf);
		WPA_ASSERT(data->out_buf == NULL);
	}
	data->out_buf = wpabuf_alloc_ext_data(next, next_len);
	if (data->out_buf == NULL) {
		os_free(next);
		return -1;
	}
	return 0;
}


static int eap_server_tls_reassemble(struct eap_ssl_data *data, u8 flags,
				     const u8 **pos, size_t *left)
{
	unsigned int tls_msg_len = 0;
	const u8 *end = *pos + *left;

	if (flags & EAP_TLS_FLAGS_LENGTH_INCLUDED) {
		if (*left < 4) {
			wpa_printf(MSG_INFO, "SSL: Short frame with TLS "
				   "length");
			return -1;
		}
		tls_msg_len = WPA_GET_BE32(*pos);
		wpa_printf(MSG_DEBUG, "SSL: TLS Message Length: %d",
			   tls_msg_len);
		*pos += 4;
		*left -= 4;
	}

	wpa_printf(MSG_DEBUG, "SSL: Received packet: Flags 0x%x "
		   "Message Length %u", flags, tls_msg_len);

	if (data->state == WAIT_FRAG_ACK) {
		if (*left != 0) {
			wpa_printf(MSG_DEBUG, "SSL: Unexpected payload in "
				   "WAIT_FRAG_ACK state");
			return -1;
		}
		wpa_printf(MSG_DEBUG, "SSL: Fragment acknowledged");
		return 1;
	}

	if (data->in_buf &&
	    eap_server_tls_process_cont(data, *pos, end - *pos) < 0)
		return -1;
		
	if (flags & EAP_TLS_FLAGS_MORE_FRAGMENTS) {
		if (eap_server_tls_process_fragment(data, flags, tls_msg_len,
						    *pos, end - *pos) < 0)
			return -1;

		data->state = FRAG_ACK;
		return 1;
	}

	if (data->state == FRAG_ACK) {
		wpa_printf(MSG_DEBUG, "SSL: All fragments received");
		data->state = MSG;
	}

	if (data->in_buf == NULL) {
		/* Wrap unfragmented messages as wpabuf without extra copy */
		wpabuf_set(&data->tmpbuf, *pos, end - *pos);
		data->in_buf = &data->tmpbuf;
	}

	return 0;
}


static void eap_server_tls_free_in_buf(struct eap_ssl_data *data)
{
	if (data->in_buf != &data->tmpbuf)
		wpabuf_free(data->in_buf);
	data->in_buf = NULL;
}


struct wpabuf * eap_server_tls_encrypt(struct eap_sm *sm,
				       struct eap_ssl_data *data,
				       const u8 *plain, size_t plain_len)
{
	int res;
	struct wpabuf *buf;
	size_t buf_len;

	/* reserve some extra room for encryption overhead */
	buf_len = plain_len + 300;
	buf = wpabuf_alloc(buf_len);
	if (buf == NULL)
		return NULL;
	res = tls_connection_encrypt(sm->ssl_ctx, data->conn,
				     plain, plain_len, wpabuf_put(buf, 0),
				     buf_len);
	if (res < 0) {
		wpa_printf(MSG_INFO, "SSL: Failed to encrypt Phase 2 data");
		wpabuf_free(buf);
		return NULL;
	}

	wpabuf_put(buf, res);

	return buf;
}


int eap_server_tls_process(struct eap_sm *sm, struct eap_ssl_data *data,
			   struct wpabuf *respData, void *priv, int eap_type,
			   int (*proc_version)(struct eap_sm *sm, void *priv,
					       int peer_version),
			   void (*proc_msg)(struct eap_sm *sm, void *priv,
					    const struct wpabuf *respData))
{
	const u8 *pos;
	u8 flags;
	size_t left;
	int ret, res = 0;

	pos = eap_hdr_validate(EAP_VENDOR_IETF, eap_type, respData, &left);
	if (pos == NULL || left < 1)
		return 0; /* Should not happen - frame already validated */
	flags = *pos++;
	left--;
	wpa_printf(MSG_DEBUG, "SSL: Received packet(len=%lu) - Flags 0x%02x",
		   (unsigned long) wpabuf_len(respData), flags);

	if (proc_version &&
	    proc_version(sm, priv, flags & EAP_TLS_VERSION_MASK) < 0)
		return -1;

	ret = eap_server_tls_reassemble(data, flags, &pos, &left);
	if (ret < 0) {
		res = -1;
		goto done;
	} else if (ret == 1)
		return 0;

	if (proc_msg)
		proc_msg(sm, priv, respData);

	if (tls_connection_get_write_alerts(sm->ssl_ctx, data->conn) > 1) {
		wpa_printf(MSG_INFO, "SSL: Locally detected fatal error in "
			   "TLS processing");
		res = -1;
	}

done:
	eap_server_tls_free_in_buf(data);

	return res;
}
