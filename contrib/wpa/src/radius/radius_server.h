/*
 * hostapd / RADIUS authentication server
 * Copyright (c) 2005-2007, Jouni Malinen <j@w1.fi>
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

#ifndef RADIUS_SERVER_H
#define RADIUS_SERVER_H

struct radius_server_data;
struct eap_user;

struct radius_server_conf {
	int auth_port;
	char *client_file;
	void *conf_ctx;
	void *eap_sim_db_priv;
	void *ssl_ctx;
	u8 *pac_opaque_encr_key;
	u8 *eap_fast_a_id;
	size_t eap_fast_a_id_len;
	char *eap_fast_a_id_info;
	int eap_fast_prov;
	int pac_key_lifetime;
	int pac_key_refresh_time;
	int eap_sim_aka_result_ind;
	int tnc;
	struct wps_context *wps;
	int ipv6;
	int (*get_eap_user)(void *ctx, const u8 *identity, size_t identity_len,
			    int phase2, struct eap_user *user);
	const char *eap_req_id_text;
	size_t eap_req_id_text_len;
};


#ifdef RADIUS_SERVER

struct radius_server_data *
radius_server_init(struct radius_server_conf *conf);

void radius_server_deinit(struct radius_server_data *data);

int radius_server_get_mib(struct radius_server_data *data, char *buf,
			  size_t buflen);

void radius_server_eap_pending_cb(struct radius_server_data *data, void *ctx);

#else /* RADIUS_SERVER */

static inline struct radius_server_data *
radius_server_init(struct radius_server_conf *conf)
{
	return NULL;
}

static inline void radius_server_deinit(struct radius_server_data *data)
{
}

static inline int radius_server_get_mib(struct radius_server_data *data,
					char *buf, size_t buflen)
{
	return 0;
}

static inline void
radius_server_eap_pending_cb(struct radius_server_data *data, void *ctx)
{
}

#endif /* RADIUS_SERVER */

#endif /* RADIUS_SERVER_H */
