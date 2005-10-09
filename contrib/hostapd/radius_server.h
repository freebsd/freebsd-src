#ifndef RADIUS_SERVER_H
#define RADIUS_SERVER_H

struct radius_server_data;

struct radius_server_conf {
	int auth_port;
	char *client_file;
	void *hostapd_conf;
	void *eap_sim_db_priv;
	void *ssl_ctx;
};


#ifdef RADIUS_SERVER

struct radius_server_data *
radius_server_init(struct radius_server_conf *conf);

void radius_server_deinit(struct radius_server_data *data);

int radius_server_get_mib(struct radius_server_data *data, char *buf,
			  size_t buflen);

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

#endif /* RADIUS_SERVER */

#endif /* RADIUS_SERVER_H */
