/*
 * hostapd / EAP-TLS/PEAP/TTLS common functions
 * Copyright (c) 2004-2005, Jouni Malinen <j@w1.fi>
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

#ifndef EAP_TLS_COMMON_H
#define EAP_TLS_COMMON_H

struct eap_ssl_data {
	struct tls_connection *conn;

	u8 *tls_out;
	size_t tls_out_len;
	size_t tls_out_pos;
	size_t tls_out_limit;
	u8 *tls_in;
	size_t tls_in_len;
	size_t tls_in_left;
	size_t tls_in_total;

	int phase2;

	struct eap_sm *eap;
};


/* EAP TLS Flags */
#define EAP_TLS_FLAGS_LENGTH_INCLUDED 0x80
#define EAP_TLS_FLAGS_MORE_FRAGMENTS 0x40
#define EAP_TLS_FLAGS_START 0x20
#define EAP_PEAP_VERSION_MASK 0x07

 /* could be up to 128 bytes, but only the first 64 bytes are used */
#define EAP_TLS_KEY_LEN 64


int eap_tls_ssl_init(struct eap_sm *sm, struct eap_ssl_data *data,
		     int verify_peer);
void eap_tls_ssl_deinit(struct eap_sm *sm, struct eap_ssl_data *data);
u8 * eap_tls_derive_key(struct eap_sm *sm, struct eap_ssl_data *data,
			char *label, size_t len);
int eap_tls_data_reassemble(struct eap_sm *sm, struct eap_ssl_data *data,
			    u8 **in_data, size_t *in_len);
int eap_tls_process_helper(struct eap_sm *sm, struct eap_ssl_data *data,
			   u8 *in_data, size_t in_len);
int eap_tls_buildReq_helper(struct eap_sm *sm, struct eap_ssl_data *data,
			    int eap_type, int peap_version, u8 id,
			    u8 **out_data, size_t *out_len);
u8 * eap_tls_build_ack(size_t *reqDataLen, u8 id, int eap_type,
		       int peap_version);

#endif /* EAP_TLS_COMMON_H */
