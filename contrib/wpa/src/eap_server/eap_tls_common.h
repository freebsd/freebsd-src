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

#ifndef EAP_TLS_COMMON_H
#define EAP_TLS_COMMON_H

struct eap_ssl_data {
	struct tls_connection *conn;

	size_t tls_out_limit;

	int phase2;

	struct eap_sm *eap;

	enum { MSG, FRAG_ACK, WAIT_FRAG_ACK } state;
	struct wpabuf *in_buf;
	struct wpabuf *out_buf;
	size_t out_used;
	struct wpabuf tmpbuf;
};


/* EAP TLS Flags */
#define EAP_TLS_FLAGS_LENGTH_INCLUDED 0x80
#define EAP_TLS_FLAGS_MORE_FRAGMENTS 0x40
#define EAP_TLS_FLAGS_START 0x20
#define EAP_TLS_VERSION_MASK 0x07

 /* could be up to 128 bytes, but only the first 64 bytes are used */
#define EAP_TLS_KEY_LEN 64


int eap_server_tls_ssl_init(struct eap_sm *sm, struct eap_ssl_data *data,
			    int verify_peer);
void eap_server_tls_ssl_deinit(struct eap_sm *sm, struct eap_ssl_data *data);
u8 * eap_server_tls_derive_key(struct eap_sm *sm, struct eap_ssl_data *data,
			       char *label, size_t len);
struct wpabuf * eap_server_tls_build_msg(struct eap_ssl_data *data,
					 int eap_type, int version, u8 id);
struct wpabuf * eap_server_tls_build_ack(u8 id, int eap_type, int version);
int eap_server_tls_phase1(struct eap_sm *sm, struct eap_ssl_data *data);
struct wpabuf * eap_server_tls_encrypt(struct eap_sm *sm,
				       struct eap_ssl_data *data,
				       const u8 *plain, size_t plain_len);
int eap_server_tls_process(struct eap_sm *sm, struct eap_ssl_data *data,
			   struct wpabuf *respData, void *priv, int eap_type,
			   int (*proc_version)(struct eap_sm *sm, void *priv,
					       int peer_version),
			   void (*proc_msg)(struct eap_sm *sm, void *priv,
					    const struct wpabuf *respData));

#endif /* EAP_TLS_COMMON_H */
