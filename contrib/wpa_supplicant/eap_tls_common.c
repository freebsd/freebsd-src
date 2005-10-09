/*
 * WPA Supplicant / EAP-TLS/PEAP/TTLS/FAST common functions
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

#include "common.h"
#include "eap_i.h"
#include "eap_tls_common.h"
#include "wpa_supplicant.h"
#include "config_ssid.h"
#include "md5.h"
#include "sha1.h"
#include "tls.h"


int eap_tls_ssl_init(struct eap_sm *sm, struct eap_ssl_data *data,
		     struct wpa_ssid *config)
{
	int ret = -1;
	char *ca_cert, *client_cert, *private_key, *private_key_passwd,
		*dh_file, *subject_match;

	data->eap = sm;
	data->phase2 = sm->init_phase2;
	if (config == NULL) {
		ca_cert = NULL;
		client_cert = NULL;
		private_key = NULL;
		private_key_passwd = NULL;
		dh_file = NULL;
		subject_match = NULL;
	} else if (data->phase2) {
		ca_cert = (char *) config->ca_cert2;
		client_cert = (char *) config->client_cert2;
		private_key = (char *) config->private_key2;
		private_key_passwd = (char *) config->private_key2_passwd;
		dh_file = (char *) config->dh_file2;
		subject_match = (char *) config->subject_match2;
	} else {
		ca_cert = (char *) config->ca_cert;
		client_cert = (char *) config->client_cert;
		private_key = (char *) config->private_key;
		private_key_passwd = (char *) config->private_key_passwd;
		dh_file = (char *) config->dh_file;
		subject_match = (char *) config->subject_match;
	}
	data->conn = tls_connection_init(sm->ssl_ctx);
	if (data->conn == NULL) {
		wpa_printf(MSG_INFO, "SSL: Failed to initialize new TLS "
			   "connection");
		goto done;
	}

	if (tls_connection_ca_cert(sm->ssl_ctx, data->conn, ca_cert,
				   subject_match)) {
		wpa_printf(MSG_INFO, "TLS: Failed to load root certificate "
			   "'%s'", ca_cert);
		goto done;
	}

	if (tls_connection_client_cert(sm->ssl_ctx, data->conn, client_cert)) {
		wpa_printf(MSG_INFO, "TLS: Failed to load client certificate "
			   "'%s'", client_cert);
		goto done;
	}

	if (tls_connection_private_key(sm->ssl_ctx, data->conn, private_key,
				       private_key_passwd)) {
		wpa_printf(MSG_INFO, "TLS: Failed to load private key '%s'",
			   private_key);
		goto done;
	}

	if (dh_file && tls_connection_dh(sm->ssl_ctx, data->conn, dh_file)) {
		wpa_printf(MSG_INFO, "TLS: Failed to load DH file '%s'",
			   dh_file);
		goto done;
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

	if (config && config->phase1 &&
	    strstr(config->phase1, "include_tls_length=1")) {
		wpa_printf(MSG_DEBUG, "TLS: Include TLS Message Length in "
			   "unfragmented packets");
		data->include_tls_length = 1;
	}

	ret = 0;

done:
	return ret;
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
		if (data->tls_in_len + *in_len == 0) {
			free(data->tls_in);
			data->tls_in = NULL;
			data->tls_in_len = 0;
			wpa_printf(MSG_WARNING, "SSL: Invalid reassembly "
				   "state: tls_in_left=%d tls_in_len=%d "
				   "*in_len=%d",
				   data->tls_in_left, data->tls_in_len,
				   *in_len);
			return -1;
		}
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
			   int eap_type, int peap_version,
			   u8 id, u8 *in_data, size_t in_len,
			   u8 **out_data, size_t *out_len)
{
	size_t len;
	u8 *pos, *flags;
	struct eap_hdr *resp;

	WPA_ASSERT(data->tls_out_len == 0 || in_len == 0);
	*out_len = 0;

	if (data->tls_out_len == 0) {
		/* No more data to send out - expect to receive more data from
		 * the AS. */
		int res = eap_tls_data_reassemble(sm, data, &in_data, &in_len);
		if (res < 0 || res == 1)
			return res;
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
		data->tls_out = tls_connection_handshake(sm->ssl_ctx,
							 data->conn,
							 in_data, in_len,
							 &data->tls_out_len);

		/* Clear reassembled input data (if the buffer was needed). */
		data->tls_in_left = data->tls_in_total = data->tls_in_len = 0;
		free(data->tls_in);
		data->tls_in = NULL;
	}

	if (data->tls_out == NULL) {
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
	resp = malloc(sizeof(struct eap_hdr) + 2 + 4 + data->tls_out_limit);
	if (resp == NULL) {
		*out_data = NULL;
		return -1;
	}
	resp->code = EAP_CODE_RESPONSE;
	resp->identifier = id;
	pos = (u8 *) (resp + 1);
	*pos++ = eap_type;
	flags = pos++;
	*flags = peap_version;
	if (data->tls_out_pos == 0 &&
	    (data->tls_out_len > data->tls_out_limit ||
	     data->include_tls_length)) {
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
	*out_len = (pos - (u8 *) resp) + len;
	resp->length = host_to_be16(*out_len);
	*out_data = (u8 *) resp;

	if (!(*flags & EAP_TLS_FLAGS_MORE_FRAGMENTS)) {
		data->tls_out_len = 0;
		data->tls_out_pos = 0;
		free(data->tls_out);
		data->tls_out = NULL;
	}

	return 0;
}


u8 * eap_tls_build_ack(struct eap_ssl_data *data, size_t *respDataLen, u8 id,
		       int eap_type, int peap_version)
{
	struct eap_hdr *resp;
	u8 *pos;

	*respDataLen = sizeof(struct eap_hdr) + 2;
	resp = malloc(*respDataLen);
	if (resp == NULL)
		return NULL;
	wpa_printf(MSG_DEBUG, "SSL: Building ACK");
	resp->code = EAP_CODE_RESPONSE;
	resp->identifier = id;
	resp->length = host_to_be16(*respDataLen);
	pos = (u8 *) (resp + 1);
	*pos++ = eap_type; /* Type */
	*pos = peap_version; /* Flags */
	return (u8 *) resp;
}


int eap_tls_reauth_init(struct eap_sm *sm, struct eap_ssl_data *data)
{
	return tls_connection_shutdown(sm->ssl_ctx, data->conn);
}


int eap_tls_status(struct eap_sm *sm, struct eap_ssl_data *data, char *buf,
		   size_t buflen, int verbose)
{
	char name[128];
	int len = 0;

	if (tls_get_cipher(sm->ssl_ctx, data->conn, name, sizeof(name)) == 0) {
		len += snprintf(buf + len, buflen - len,
				"EAP TLS cipher=%s\n", name);
	}

	return len;
}
