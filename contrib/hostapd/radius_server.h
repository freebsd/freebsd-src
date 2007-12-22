/*
 * hostapd / RADIUS authentication server
 * Copyright (c) 2005, Jouni Malinen <j@w1.fi>
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

struct radius_server_conf {
	int auth_port;
	char *client_file;
	void *hostapd_conf;
	void *eap_sim_db_priv;
	void *ssl_ctx;
	int ipv6;
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
