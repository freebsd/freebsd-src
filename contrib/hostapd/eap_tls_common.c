/*
 * hostapd / EAP-TLS/PEAP/TTLS common functions
 * Copyright (c) 2004-2005, Jouni Malinen <jkmaline@cc.hut.fi>
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netinet/in.h>

#include "hostapd.h"
#include "common.h"
#include "eap_i.h"
#include "eap_tls_common.h"
#include "sha1.h"
#include "tls.h"


int eap_tls_ssl_init(struct eap_sm *sm, struct eap_ssl_data *data,
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

	if (tls_connection_set_verify(sm->ssl_ctx, data->conn, verify_peer,
				      NULL)) {
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


void eap_tls_ssl_deinit(struct eap_sm *sm, struct eap_ssl_data *data)
{
	tls_connection_deinit(sm->ssl_ctx, data->conn);
	free(data->tls_in);
	free(data->tls_out);
}


u8 * eap_tls_derive_key(struct eap_sm *sm, struct eap_ssl_data *data,
			char *label, size_t len)
{
	struct tls_keys keys;
	u8 *random;
	u8 *out;

	if (tls_connection_get_keys(sm->ssl_ctx, data->conn, &keys))
		return NULL;
	out = malloc(len);
	random = malloc(keys.client_random_len + keys.server_random_len);
	if (out == NULL || random == NULL) {
		free(out);
		free(random);
		return NULL;
	}
	memcpy(random, keys.client_random, keys.client_random_len);
	memcpy(random + keys.client_random_len, keys.server_random,
	       keys.server_random_len);

	if (tls_prf(keys.master_key, keys.master_key_len,
		    label, random, keys.client_random_len +
		    keys.server_random_len, out, len)) {
		free(random);
		free(out);
		return NULL;
	}
	free(random);
	return out;
}


int eap_tls_data_reassemble(struct eap_sm *sm, struct eap_ssl_data *data,
			    u8 **in_data, size_t *in_len)
{
	u8 *buf;

	if (data->tls_in_left > *in_len || data->tls_in) {
		buf = realloc(data->tls_in, data->tls_in_len + *in_len);
		if (buf == NULL) {
			free(data->tls_in);
			data->tls_in = NULL;
			data->tls_in_len = 0;
			wpa_printf(MSG_INFO, "SSL: Could not allocate memory "
				   "for TLS data");
			return -1;
		}
		memcpy(buf + data->tls_in_len, *in_data, *in_len);
		data->tls_in = buf;
		data->tls_in_len += *in_len;
		if (*in_len > data->tls_in_left) {
			wpa_printf(MSG_INFO, "SSL: more data than TLS message "
				   "length indicated");
			data->tls_in_left = 0;
			return -1;
		}
		data->tls_in_left -= *in_len;
		if (data->tls_in_left > 0) {
			wpa_printf(MSG_DEBUG, "SSL: Need %lu bytes more input "
				   "data", (unsigned long) data->tls_in_left);
			return 1;
		}

		*in_data = data->tls_in;
		*in_len = data->tls_in_len;
	} else
		data->tls_in_left = 0;

	return 0;
}


int eap_tls_process_helper(struct eap_sm *sm, struct eap_ssl_data *data,
			   u8 *in_data, size_t in_len)
{
	WPA_ASSERT(data->tls_out_len == 0 || in_len == 0);

	if (data->tls_out_len == 0) {
		/* No more data to send out - expect to receive more data from
		 * the peer. */
		int res = eap_tls_data_reassemble(sm, data, &in_data, &in_len);
		if (res < 0 || res == 1) {
			wpa_printf(MSG_DEBUG, "SSL: data reassembly failed");
			return res;
		}
		/* Full TLS message reassembled - continue handshake processing
		 */
		if (data->tls_out) {
			/* This should not happen.. */
			wpa_printf(MSG_INFO, "SSL: eap_tls_process_helper - "
				   "pending tls_out data even though "
				   "tls_out_len = 0");
			free(data->tls_out);
			WPA_ASSERT(data->tls_out == NULL);
		}
		data->tls_out = tls_connection_server_handshake(
			sm->ssl_ctx, data->conn, in_data, in_len,
			&data->tls_out_len);

		/* Clear reassembled input data (if the buffer was needed). */
		data->tls_in_left = data->tls_in_total = data->tls_in_len = 0;
		free(data->tls_in);
		data->tls_in = NULL;
	}

	if (data->tls_out == NULL) {
		wpa_printf(MSG_DEBUG, "SSL: failed to generate output data");
		data->tls_out_len = 0;
		return -1;
	}
	if (data->tls_out_len == 0) {
		/* TLS negotiation should now be complete since all other cases
		 * needing more that should have been catched above based on
		 * the TLS Message Length field. */
		wpa_printf(MSG_DEBUG, "SSL: No data to be sent out");
		free(data->tls_out);
		data->tls_out = NULL;
		return 1;
	}

	wpa_printf(MSG_DEBUG, "SSL: %lu bytes left to be sent out (of total "
		   "%lu bytes)",
		   (unsigned long) data->tls_out_len - data->tls_out_pos,
		   (unsigned long) data->tls_out_len);

	return 0;
}


int eap_tls_buildReq_helper(struct eap_sm *sm, struct eap_ssl_data *data,
			    int eap_type, int peap_version, u8 id,
			    u8 **out_data, size_t *out_len)
{
	size_t len;
	u8 *pos, *flags;
	struct eap_hdr *req;

	*out_len = 0;

	req = malloc(sizeof(struct eap_hdr) + 2 + 4 + data->tls_out_limit);
	if (req == NULL) {
		*out_data = NULL;
		return -1;
	}
	req->code = EAP_CODE_REQUEST;
	req->identifier = id;
	pos = (u8 *) (req + 1);
	*pos++ = eap_type;
	flags = pos++;
	*flags = peap_version;
	if (data->tls_out_pos == 0 &&
	    data->tls_out_len > data->tls_out_limit) {
		*flags |= EAP_TLS_FLAGS_LENGTH_INCLUDED;
		*pos++ = (data->tls_out_len >> 24) & 0xff;
		*pos++ = (data->tls_out_len >> 16) & 0xff;
		*pos++ = (data->tls_out_len >> 8) & 0xff;
		*pos++ = data->tls_out_len & 0xff;
	}

	len = data->tls_out_len - data->tls_out_pos;
	if (len > data->tls_out_limit) {
		*flags |= EAP_TLS_FLAGS_MORE_FRAGMENTS;
		len = data->tls_out_limit;
		wpa_printf(MSG_DEBUG, "SSL: sending %lu bytes, more fragments "
			   "will follow", (unsigned long) len);
	}
	memcpy(pos, &data->tls_out[data->tls_out_pos], len);
	data->tls_out_pos += len;
	*out_len = (pos - (u8 *) req) + len;
	req->length = htons(*out_len);
	*out_data = (u8 *) req;

	if (!(*flags & EAP_TLS_FLAGS_MORE_FRAGMENTS)) {
		data->tls_out_len = 0;
		data->tls_out_pos = 0;
		free(data->tls_out);
		data->tls_out = NULL;
	}

	return 0;
}


u8 * eap_tls_build_ack(size_t *reqDataLen, u8 id, int eap_type,
		       int peap_version)
{
	struct eap_hdr *req;
	u8 *pos;

	*reqDataLen = sizeof(struct eap_hdr) + 2;
	req = malloc(*reqDataLen);
	if (req == NULL)
		return NULL;
	wpa_printf(MSG_DEBUG, "SSL: Building ACK");
	req->code = EAP_CODE_REQUEST;
	req->identifier = id;
	req->length = htons(*reqDataLen);
	pos = (u8 *) (req + 1);
	*pos++ = eap_type; /* Type */
	*pos = peap_version; /* Flags */
	return (u8 *) req;
}
