/*
 * hostapd / RADIUS authentication server
 * Copyright (c) 2005, Jouni Malinen <jkmaline@cc.hut.fi>
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
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>

#include "common.h"
#include "radius.h"
#include "eloop.h"
#include "config.h"
#include "eap.h"
#include "radius_server.h"

#define RADIUS_SESSION_TIMEOUT 60
#define RADIUS_MAX_SESSION 100
#define RADIUS_MAX_MSG_LEN 3000

static struct eapol_callbacks radius_server_eapol_cb;

struct radius_client;
struct radius_server_data;

struct radius_session {
	struct radius_session *next;
	struct radius_client *client;
	struct radius_server_data *server;
	unsigned int sess_id;
	struct eap_sm *eap;
	u8 *eapKeyData, *eapReqData;
	size_t eapKeyDataLen, eapReqDataLen;
	Boolean eapSuccess, eapRestart, eapFail, eapResp, eapReq, eapNoReq;
	Boolean portEnabled, eapTimeout;
};

struct radius_client {
	struct radius_client *next;
	struct in_addr addr;
	struct in_addr mask;
	char *shared_secret;
	int shared_secret_len;
	struct radius_session *sessions;
};

struct radius_server_data {
	int auth_sock;
	struct radius_client *clients;
	struct radius_server_session *sessions;
	unsigned int next_sess_id;
	void *hostapd_conf;
	int num_sess;
	void *eap_sim_db_priv;
	void *ssl_ctx;
};


extern int wpa_debug_level;

#define RADIUS_DEBUG(args...) \
wpa_printf(MSG_DEBUG, "RADIUS SRV: " args)
#define RADIUS_ERROR(args...) \
wpa_printf(MSG_ERROR, "RADIUS SRV: " args)
#define RADIUS_DUMP(args...) \
wpa_hexdump(MSG_MSGDUMP, "RADIUS SRV: " args)
#define RADIUS_DUMP_ASCII(args...) \
wpa_hexdump_ascii(MSG_MSGDUMP, "RADIUS SRV: " args)


static void radius_server_session_timeout(void *eloop_ctx, void *timeout_ctx);



static struct radius_client *
radius_server_get_client(struct radius_server_data *data, struct in_addr *addr)
{
	struct radius_client *client = data->clients;

	while (client) {
		if ((client->addr.s_addr & client->mask.s_addr) ==
		    (addr->s_addr & client->mask.s_addr)) {
			break;
		}

		client = client->next;
	}

	return client;
}


static struct radius_session *
radius_server_get_session(struct radius_client *client, unsigned int sess_id)
{
	struct radius_session *sess = client->sessions;

	while (sess) {
		if (sess->sess_id == sess_id) {
			break;
		}
		sess = sess->next;
	}

	return sess;
}


static void radius_server_session_free(struct radius_server_data *data,
				       struct radius_session *sess)
{
	eloop_cancel_timeout(radius_server_session_timeout, data, sess);
	free(sess->eapKeyData);
	free(sess->eapReqData);
	eap_sm_deinit(sess->eap);
	free(sess);
	data->num_sess--;
}


static void radius_server_session_remove(struct radius_server_data *data,
					 struct radius_session *sess)
{
	struct radius_client *client = sess->client;
	struct radius_session *session, *prev;

	prev = NULL;
	session = client->sessions;
	while (session) {
		if (session == sess) {
			if (prev == NULL) {
				client->sessions = sess->next;
			} else {
				prev->next = sess->next;
			}
			radius_server_session_free(data, sess);
			break;
		}
		prev = session;
		session = session->next;
	}
}


static void radius_server_session_timeout(void *eloop_ctx, void *timeout_ctx)
{
	struct radius_server_data *data = eloop_ctx;
	struct radius_session *sess = timeout_ctx;

	RADIUS_DEBUG("Timing out authentication session 0x%x", sess->sess_id);
	radius_server_session_remove(data, sess);
}


static struct radius_session *
radius_server_new_session(struct radius_server_data *data,
			  struct radius_client *client)
{
	struct radius_session *sess;

	if (data->num_sess >= RADIUS_MAX_SESSION) {
		RADIUS_DEBUG("Maximum number of existing session - no room "
			     "for a new session");
		return NULL;
	}

	sess = malloc(sizeof(*sess));
	if (sess == NULL) {
		return NULL;
	}
	memset(sess, 0, sizeof(*sess));
	sess->server = data;
	sess->client = client;
	sess->sess_id = data->next_sess_id++;
	sess->next = client->sessions;
	client->sessions = sess;
	eloop_register_timeout(RADIUS_SESSION_TIMEOUT, 0,
			       radius_server_session_timeout, data, sess);
	data->num_sess++;
	return sess;
}


static struct radius_session *
radius_server_get_new_session(struct radius_server_data *data,
			      struct radius_client *client,
			      struct radius_msg *msg)
{
	u8 *user;
	size_t user_len;
	const struct hostapd_eap_user *eap_user;
	int res;
	struct radius_session *sess;
	struct eap_config eap_conf;

	RADIUS_DEBUG("Creating a new session");

	user = malloc(256);
	if (user == NULL) {
		return NULL;
	}
	res = radius_msg_get_attr(msg, RADIUS_ATTR_USER_NAME, user, 256);
	if (res < 0 || res > 256) {
		RADIUS_DEBUG("Could not get User-Name");
		free(user);
		return NULL;
	}
	user_len = res;
	RADIUS_DUMP_ASCII("User-Name", user, user_len);

	eap_user = hostapd_get_eap_user(data->hostapd_conf, user, user_len, 0);
	free(user);

	if (eap_user) {
		RADIUS_DEBUG("Matching user entry found");
		sess = radius_server_new_session(data, client);
		if (sess == NULL) {
			RADIUS_DEBUG("Failed to create a new session");
			return NULL;
		}
	} else {
		RADIUS_DEBUG("User-Name not found from user database");
		return NULL;
	}

	memset(&eap_conf, 0, sizeof(eap_conf));
	eap_conf.ssl_ctx = data->ssl_ctx;
	eap_conf.eap_sim_db_priv = data->eap_sim_db_priv;
	eap_conf.backend_auth = TRUE;
	sess->eap = eap_sm_init(sess, &radius_server_eapol_cb, &eap_conf);
	if (sess->eap == NULL) {
		RADIUS_DEBUG("Failed to initialize EAP state machine for the "
			     "new session");
		radius_server_session_free(data, sess);
		return NULL;
	}
	sess->eapRestart = TRUE;
	sess->portEnabled = TRUE;

	RADIUS_DEBUG("New session 0x%x initialized", sess->sess_id);

	return sess;
}


static struct radius_msg *
radius_server_encapsulate_eap(struct radius_server_data *data,
			      struct radius_client *client,
			      struct radius_session *sess,
			      struct radius_msg *request)
{
	struct radius_msg *msg;
	int code;
	unsigned int sess_id;

	if (sess->eapFail) {
		code = RADIUS_CODE_ACCESS_REJECT;
	} else if (sess->eapSuccess) {
		code = RADIUS_CODE_ACCESS_ACCEPT;
	} else {
		code = RADIUS_CODE_ACCESS_CHALLENGE;
	}

	msg = radius_msg_new(code, request->hdr->identifier);
	if (msg == NULL) {
		return NULL;
	}

	sess_id = htonl(sess->sess_id);
	if (code == RADIUS_CODE_ACCESS_CHALLENGE &&
	    !radius_msg_add_attr(msg, RADIUS_ATTR_STATE,
				 (u8 *) &sess_id, sizeof(sess_id))) {
		RADIUS_DEBUG("Failed to add State attribute");
	}

	if (sess->eapReqData &&
	    !radius_msg_add_eap(msg, sess->eapReqData, sess->eapReqDataLen)) {
		RADIUS_DEBUG("Failed to add EAP-Message attribute");
	}

	if (code == RADIUS_CODE_ACCESS_ACCEPT && sess->eapKeyData) {
		int len;
		if (sess->eapKeyDataLen > 64) {
			len = 32;
		} else {
			len = sess->eapKeyDataLen / 2;
		}
		if (!radius_msg_add_mppe_keys(msg, request->hdr->authenticator,
					      (u8 *) client->shared_secret,
					      client->shared_secret_len,
					      sess->eapKeyData + len, len,
					      sess->eapKeyData, len)) {
			RADIUS_DEBUG("Failed to add MPPE key attributes");
		}
	}

	if (radius_msg_finish_srv(msg, (u8 *) client->shared_secret,
				  client->shared_secret_len,
				  request->hdr->authenticator) < 0) {
		RADIUS_DEBUG("Failed to add Message-Authenticator attribute");
	}

	return msg;
}


static int radius_server_reject(struct radius_server_data *data,
				struct radius_client *client,
				struct radius_msg *request,
				struct sockaddr_in *from)
{
	struct radius_msg *msg;
	int ret = 0;

	RADIUS_DEBUG("Reject invalid request from %s:%d",
		     inet_ntoa(from->sin_addr), ntohs(from->sin_port));

	msg = radius_msg_new(RADIUS_CODE_ACCESS_REJECT,
			     request->hdr->identifier);
	if (msg == NULL) {
		return -1;
	}

	if (radius_msg_finish_srv(msg, (u8 *) client->shared_secret,
				  client->shared_secret_len,
				  request->hdr->authenticator) < 0) {
		RADIUS_DEBUG("Failed to add Message-Authenticator attribute");
	}

	if (wpa_debug_level <= MSG_MSGDUMP) {
		radius_msg_dump(msg);
	}

	if (sendto(data->auth_sock, msg->buf, msg->buf_used, 0,
		   (struct sockaddr *) from, sizeof(*from)) < 0) {
		perror("sendto[RADIUS SRV]");
		ret = -1;
	}

	radius_msg_free(msg);
	free(msg);

	return ret;
}


static int radius_server_request(struct radius_server_data *data,
				 struct radius_msg *msg,
				 struct sockaddr_in *from,
				 struct radius_client *client)
{
	u8 *eap = NULL;
	size_t eap_len;
	int res, state_included;
	u8 statebuf[4], resp_id;
	unsigned int state;
	struct radius_session *sess;
	struct radius_msg *reply;
	struct eap_hdr *hdr;

	/* TODO: Implement duplicate packet processing */

	res = radius_msg_get_attr(msg, RADIUS_ATTR_STATE, statebuf,
				  sizeof(statebuf));
	state_included = res >= 0;
	if (res == sizeof(statebuf)) {
		state = (statebuf[0] << 24) | (statebuf[1] << 16) |
			(statebuf[2] << 8) | statebuf[3];
		sess = radius_server_get_session(client, state);
	} else {
		sess = NULL;
	}

	if (sess) {
		RADIUS_DEBUG("Request for session 0x%x", sess->sess_id);
	} else if (state_included) {
		RADIUS_DEBUG("State attribute included but no session found");
		radius_server_reject(data, client, msg, from);
		return -1;
	} else {
		sess = radius_server_get_new_session(data, client, msg);
		if (sess == NULL) {
			RADIUS_DEBUG("Could not create a new session");
			return -1;
		}
	}

	eap = radius_msg_get_eap(msg, &eap_len);
	if (eap == NULL) {
		RADIUS_DEBUG("No EAP-Message in RADIUS packet from %s",
			     inet_ntoa(from->sin_addr));
		return -1;
	}

	RADIUS_DUMP("Received EAP data", eap, eap_len);
	if (eap_len >= sizeof(*hdr)) {
		hdr = (struct eap_hdr *) eap;
		resp_id = hdr->identifier;
	} else {
		resp_id = 0;
	}

	eap_set_eapRespData(sess->eap, eap, eap_len);
	free(eap);
	eap = NULL;
	sess->eapResp = TRUE;
	eap_sm_step(sess->eap);

	if (sess->eapReqData) {
		RADIUS_DUMP("EAP data from the state machine",
			    sess->eapReqData, sess->eapReqDataLen);
	} else if (sess->eapFail) {
		RADIUS_DEBUG("No EAP data from the state machine, but eapFail "
			     "set - generate EAP-Failure");
		hdr = malloc(sizeof(*hdr));
		if (hdr) {
			memset(hdr, 0, sizeof(*hdr));
			hdr->identifier = resp_id;
			hdr->length = htons(sizeof(*hdr));
			sess->eapReqData = (u8 *) hdr;
			sess->eapReqDataLen = sizeof(*hdr);
		}
	} else {
		RADIUS_DEBUG("No EAP data from the state machine - ignore this"
			     " Access-Request silently (assuming it was a "
			     "duplicate)");
		return -1;
	}

	reply = radius_server_encapsulate_eap(data, client, sess, msg);

	free(sess->eapReqData);
	sess->eapReqData = NULL;
	sess->eapReqDataLen = 0;

	if (reply) {
		RADIUS_DEBUG("Reply to %s:%d", inet_ntoa(from->sin_addr),
			     ntohs(from->sin_port));
		if (wpa_debug_level <= MSG_MSGDUMP) {
			radius_msg_dump(reply);
		}

		res = sendto(data->auth_sock, reply->buf, reply->buf_used, 0,
			     (struct sockaddr *) from, sizeof(*from));
		if (res < 0) {
			perror("sendto[RADIUS SRV]");
		}
		radius_msg_free(reply);
		free(reply);
	}

	if (sess->eapSuccess || sess->eapFail) {
		RADIUS_DEBUG("Removing completed session 0x%x", sess->sess_id);
		radius_server_session_remove(data, sess);
	}

	return 0;
}


static void radius_server_receive_auth(int sock, void *eloop_ctx,
				       void *sock_ctx)
{
	struct radius_server_data *data = eloop_ctx;
	u8 *buf = NULL;
	struct sockaddr_in from;
	socklen_t fromlen;
	int len;
	struct radius_client *client;
	struct radius_msg *msg = NULL;

	buf = malloc(RADIUS_MAX_MSG_LEN);
	if (buf == NULL) {
		goto fail;
	}

	fromlen = sizeof(from);
	len = recvfrom(sock, buf, RADIUS_MAX_MSG_LEN, 0,
		       (struct sockaddr *) &from, &fromlen);
	if (len < 0) {
		perror("recvfrom[radius_server]");
		goto fail;
	}

	RADIUS_DEBUG("Received %d bytes from %s:%d",
		     len, inet_ntoa(from.sin_addr), ntohs(from.sin_port));
	RADIUS_DUMP("Received data", buf, len);

	client = radius_server_get_client(data, &from.sin_addr);
	if (client == NULL) {
		RADIUS_DEBUG("Unknown client %s - packet ignored",
			     inet_ntoa(from.sin_addr));
		goto fail;
	}

	msg = radius_msg_parse(buf, len);
	if (msg == NULL) {
		RADIUS_DEBUG("Parsing incoming RADIUS frame failed");
		goto fail;
	}

	free(buf);
	buf = NULL;

	if (wpa_debug_level <= MSG_MSGDUMP) {
		radius_msg_dump(msg);
	}

	if (msg->hdr->code != RADIUS_CODE_ACCESS_REQUEST) {
		RADIUS_DEBUG("Unexpected RADIUS code %d", msg->hdr->code);
		goto fail;
	}

	if (radius_msg_verify_msg_auth(msg, (u8 *) client->shared_secret,
				       client->shared_secret_len, NULL)) {
		RADIUS_DEBUG("Invalid Message-Authenticator from %s",
			     inet_ntoa(from.sin_addr));
		goto fail;
	}

	radius_server_request(data, msg, &from, client);

fail:
	if (msg) {
		radius_msg_free(msg);
		free(msg);
	}
	free(buf);
}


static int radius_server_open_socket(int port)
{
	int s;
	struct sockaddr_in addr;

	s = socket(PF_INET, SOCK_DGRAM, 0);
	if (s < 0) {
		perror("socket");
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		perror("bind");
		close(s);
		return -1;
	}

	return s;
}


static void radius_server_free_sessions(struct radius_server_data *data,
					struct radius_session *sessions)
{
	struct radius_session *session, *prev;

	session = sessions;
	while (session) {
		prev = session;
		session = session->next;
		radius_server_session_free(data, prev);
	}
}


static void radius_server_free_clients(struct radius_server_data *data,
				       struct radius_client *clients)
{
	struct radius_client *client, *prev;

	client = clients;
	while (client) {
		prev = client;
		client = client->next;

		radius_server_free_sessions(data, prev->sessions);
		free(prev->shared_secret);
		free(prev);
	}
}


static struct radius_client *
radius_server_read_clients(const char *client_file)
{
	FILE *f;
	const int buf_size = 1024;
	char *buf, *pos;
	struct radius_client *clients, *tail, *entry;
	int line = 0, mask, failed = 0, i;
	struct in_addr addr;
	unsigned int val;

	f = fopen(client_file, "r");
	if (f == NULL) {
		RADIUS_ERROR("Could not open client file '%s'", client_file);
		return NULL;
	}

	buf = malloc(buf_size);
	if (buf == NULL) {
		fclose(f);
		return NULL;
	}

	clients = tail = NULL;
	while (fgets(buf, buf_size, f)) {
		/* Configuration file format:
		 * 192.168.1.0/24 secret
		 * 192.168.1.2 secret
		 */
		line++;
		buf[buf_size - 1] = '\0';
		pos = buf;
		while (*pos != '\0' && *pos != '\n')
			pos++;
		if (*pos == '\n')
			*pos = '\0';
		if (*buf == '\0' || *buf == '#')
			continue;

		pos = buf;
		while ((*pos >= '0' && *pos <= '9') || *pos == '.') {
			pos++;
		}

		if (*pos == '\0') {
			failed = 1;
			break;
		}

		if (*pos == '/') {
			char *end;
			*pos++ = '\0';
			mask = strtol(pos, &end, 10);
			if ((pos == end) || (mask < 0 || mask > 32)) {
				failed = 1;
				break;
			}
			pos = end;
		} else {
			mask = 32;
			*pos++ = '\0';
		}

		if (inet_aton(buf, &addr) == 0) {
			failed = 1;
			break;
		}

		while (*pos == ' ' || *pos == '\t') {
			pos++;
		}

		if (*pos == '\0') {
			failed = 1;
			break;
		}

		entry = malloc(sizeof(*entry));
		if (entry == NULL) {
			failed = 1;
			break;
		}
		memset(entry, 0, sizeof(*entry));
		entry->shared_secret = strdup(pos);
		if (entry->shared_secret == NULL) {
			failed = 1;
			free(entry);
			break;
		}
		entry->shared_secret_len = strlen(entry->shared_secret);
		entry->addr.s_addr = addr.s_addr;
		val = 0;
		for (i = 0; i < mask; i++)
			val |= 1 << (31 - i);
		entry->mask.s_addr = htonl(val);

		if (tail == NULL) {
			clients = tail = entry;
		} else {
			tail->next = entry;
			tail = entry;
		}
	}

	if (failed) {
		RADIUS_ERROR("Invalid line %d in '%s'", line, client_file);
		radius_server_free_clients(NULL, clients);
		clients = NULL;
	}

	free(buf);
	fclose(f);

	return clients;
}


struct radius_server_data *
radius_server_init(struct radius_server_conf *conf)
{
	struct radius_server_data *data;

	data = malloc(sizeof(*data));
	if (data == NULL) {
		return NULL;
	}
	memset(data, 0, sizeof(*data));
	data->hostapd_conf = conf->hostapd_conf;
	data->eap_sim_db_priv = conf->eap_sim_db_priv;
	data->ssl_ctx = conf->ssl_ctx;

	data->clients = radius_server_read_clients(conf->client_file);
	if (data->clients == NULL) {
		printf("No RADIUS clients configured.\n");
		radius_server_deinit(data);
		return NULL;
	}

	data->auth_sock = radius_server_open_socket(conf->auth_port);
	if (data->auth_sock < 0) {
		printf("Failed to open UDP socket for RADIUS authentication "
		       "server\n");
		radius_server_deinit(data);
		return NULL;
	}
	if (eloop_register_read_sock(data->auth_sock,
				     radius_server_receive_auth,
				     data, NULL)) {
		radius_server_deinit(data);
		return NULL;
	}

	return data;
}


void radius_server_deinit(struct radius_server_data *data)
{
	if (data == NULL)
		return;

	if (data->auth_sock >= 0) {
		eloop_unregister_read_sock(data->auth_sock);
		close(data->auth_sock);
	}

	radius_server_free_clients(data, data->clients);

	free(data);
}


int radius_server_get_mib(struct radius_server_data *data, char *buf,
			  size_t buflen)
{
	/* TODO: add support for RADIUS authentication server MIB */
	return 0;
}


static Boolean radius_server_get_bool(void *ctx, enum eapol_bool_var variable)
{
	struct radius_session *sess = ctx;
	if (sess == NULL)
		return FALSE;
	switch (variable) {
	case EAPOL_eapSuccess:
		return sess->eapSuccess;
	case EAPOL_eapRestart:
		return sess->eapRestart;
	case EAPOL_eapFail:
		return sess->eapFail;
	case EAPOL_eapResp:
		return sess->eapResp;
	case EAPOL_eapReq:
		return sess->eapReq;
	case EAPOL_eapNoReq:
		return sess->eapNoReq;
	case EAPOL_portEnabled:
		return sess->portEnabled;
	case EAPOL_eapTimeout:
		return sess->eapTimeout;
	}
	return FALSE;
}


static void radius_server_set_bool(void *ctx, enum eapol_bool_var variable,
			      Boolean value)
{
	struct radius_session *sess = ctx;
	if (sess == NULL)
		return;
	switch (variable) {
	case EAPOL_eapSuccess:
		sess->eapSuccess = value;
		break;
	case EAPOL_eapRestart:
		sess->eapRestart = value;
		break;
	case EAPOL_eapFail:
		sess->eapFail = value;
		break;
	case EAPOL_eapResp:
		sess->eapResp = value;
		break;
	case EAPOL_eapReq:
		sess->eapReq = value;
		break;
	case EAPOL_eapNoReq:
		sess->eapNoReq = value;
		break;
	case EAPOL_portEnabled:
		sess->portEnabled = value;
		break;
	case EAPOL_eapTimeout:
		sess->eapTimeout = value;
		break;
	}
}


static void radius_server_set_eapReqData(void *ctx, const u8 *eapReqData,
				    size_t eapReqDataLen)
{
	struct radius_session *sess = ctx;
	if (sess == NULL)
		return;

	free(sess->eapReqData);
	sess->eapReqData = malloc(eapReqDataLen);
	if (sess->eapReqData) {
		memcpy(sess->eapReqData, eapReqData, eapReqDataLen);
		sess->eapReqDataLen = eapReqDataLen;
	} else {
		sess->eapReqDataLen = 0;
	}
}


static void radius_server_set_eapKeyData(void *ctx, const u8 *eapKeyData,
				    size_t eapKeyDataLen)
{
	struct radius_session *sess = ctx;

	if (sess == NULL)
		return;

	free(sess->eapKeyData);
	if (eapKeyData) {
		sess->eapKeyData = malloc(eapKeyDataLen);
		if (sess->eapKeyData) {
			memcpy(sess->eapKeyData, eapKeyData, eapKeyDataLen);
			sess->eapKeyDataLen = eapKeyDataLen;
		} else {
			sess->eapKeyDataLen = 0;
		}
	} else {
		sess->eapKeyData = NULL;
		sess->eapKeyDataLen = 0;
	}
}


static int radius_server_get_eap_user(void *ctx, const u8 *identity,
				      size_t identity_len, int phase2,
				      struct eap_user *user)
{
	struct radius_session *sess = ctx;
	const struct hostapd_eap_user *eap_user;

	eap_user = hostapd_get_eap_user(sess->server->hostapd_conf, identity,
					identity_len, phase2);
	if (eap_user == NULL)
		return -1;

	memset(user, 0, sizeof(*user));
	memcpy(user->methods, eap_user->methods,
	       EAP_USER_MAX_METHODS > EAP_MAX_METHODS ?
	       EAP_USER_MAX_METHODS : EAP_MAX_METHODS);

	if (eap_user->password) {
		user->password = malloc(eap_user->password_len);
		if (user->password == NULL)
			return -1;
		memcpy(user->password, eap_user->password,
		       eap_user->password_len);
		user->password_len = eap_user->password_len;
	}
	user->force_version = eap_user->force_version;

	return 0;
}


static struct eapol_callbacks radius_server_eapol_cb =
{
	.get_bool = radius_server_get_bool,
	.set_bool = radius_server_set_bool,
	.set_eapReqData = radius_server_set_eapReqData,
	.set_eapKeyData = radius_server_set_eapKeyData,
	.get_eap_user = radius_server_get_eap_user,
};
