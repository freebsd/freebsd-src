/*
 * RADIUS authentication server
 * Copyright (c) 2005-2009, 2011, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef RADIUS_SERVER_H
#define RADIUS_SERVER_H

struct radius_server_data;
struct radius_msg;
struct eap_user;

/**
 * struct radius_server_conf - RADIUS server configuration
 */
struct radius_server_conf {
	/**
	 * auth_port - UDP port to listen to as an authentication server
	 */
	int auth_port;

	/**
	 * acct_port - UDP port to listen to as an accounting server
	 */
	int acct_port;

	/**
	 * client_file - RADIUS client configuration file
	 *
	 * This file contains the RADIUS clients and the shared secret to be
	 * used with them in a format where each client is on its own line. The
	 * first item on the line is the IPv4 or IPv6 address of the client
	 * with an optional address mask to allow full network to be specified
	 * (e.g., 192.168.1.2 or 192.168.1.0/24). This is followed by white
	 * space (space or tabulator) and the shared secret. Lines starting
	 * with '#' are skipped and can be used as comments.
	 */
	char *client_file;

	/**
	 * sqlite_file - SQLite database for storing debug log information
	 */
	const char *sqlite_file;

	/**
	 * conf_ctx - Context pointer for callbacks
	 *
	 * This is used as the ctx argument in get_eap_user() and acct_req_cb()
	 * calls.
	 */
	void *conf_ctx;

	const char *erp_domain;

	/**
	 * ipv6 - Whether to enable IPv6 support in the RADIUS server
	 */
	int ipv6;

	/**
	 * get_eap_user - Callback for fetching EAP user information
	 * @ctx: Context data from conf_ctx
	 * @identity: User identity
	 * @identity_len: identity buffer length in octets
	 * @phase2: Whether this is for Phase 2 identity
	 * @user: Data structure for filling in the user information
	 * Returns: 0 on success, -1 on failure
	 *
	 * This is used to fetch information from user database. The callback
	 * will fill in information about allowed EAP methods and the user
	 * password. The password field will be an allocated copy of the
	 * password data and RADIUS server will free it after use.
	 */
	int (*get_eap_user)(void *ctx, const u8 *identity, size_t identity_len,
			    int phase2, struct eap_user *user);

	/**
	 * acct_req_cb - Callback for processing received RADIUS accounting
	 * requests
	 * @ctx: Context data from conf_ctx
	 * @msg: Received RADIUS accounting request
	 * @status_type: Status type from the message (parsed Acct-Status-Type
	 * attribute)
	 * Returns: 0 on success, -1 on failure
	 *
	 * This can be used to log accounting information into file, database,
	 * syslog server, etc.
	 * Callback should not modify the message.
	 * If 0 is returned, response is automatically created. Otherwise,
	 * no response is created.
	 *
	 * acct_req_cb can be set to NULL to omit any custom processing of
	 * accounting requests. Statistics counters will be incremented in any
	 * case.
	 */
	int (*acct_req_cb)(void *ctx, struct radius_msg *msg, u32 status_type);

	/**
	 * eap_req_id_text - Optional data for EAP-Request/Identity
	 *
	 * This can be used to configure an optional, displayable message that
	 * will be sent in EAP-Request/Identity. This string can contain an
	 * ASCII-0 character (nul) to separate network infromation per RFC
	 * 4284. The actual string length is explicit provided in
	 * eap_req_id_text_len since nul character will not be used as a string
	 * terminator.
	 */
	const char *eap_req_id_text;

	/**
	 * eap_req_id_text_len - Length of eap_req_id_text buffer in octets
	 */
	size_t eap_req_id_text_len;

#ifdef CONFIG_RADIUS_TEST
	const char *dump_msk_file;
#endif /* CONFIG_RADIUS_TEST */

	char *t_c_server_url;

	struct eap_config *eap_cfg;
};


struct radius_server_data *
radius_server_init(struct radius_server_conf *conf);

void radius_server_erp_flush(struct radius_server_data *data);
void radius_server_deinit(struct radius_server_data *data);

int radius_server_get_mib(struct radius_server_data *data, char *buf,
			  size_t buflen);

void radius_server_eap_pending_cb(struct radius_server_data *data, void *ctx);
int radius_server_dac_request(struct radius_server_data *data, const char *req);

#endif /* RADIUS_SERVER_H */
