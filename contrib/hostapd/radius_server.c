/*
 * hostapd / RADIUS authentication server
 * Copyright (c) 2005-2006, Jouni Malinen <j@w1.fi>
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

struct radius_server_counters {
	u32 access_requests;
	u32 invalid_requests;
	u32 dup_access_requests;
	u32 access_accepts;
	u32 access_rejects;
	u32 access_challenges;
	u32 malformed_access_requests;
	u32 bad_authenticators;
	u32 packets_dropped;
	u32 unknown_types;
};

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

	struct radius_msg *last_msg;
	char *last_from_addr;
	int last_from_port;
	struct sockaddr_storage last_from;
	socklen_t last_fromlen;
	u8 last_identifier;
	struct radius_msg *last_reply;
	u8 last_authenticator[16];
};

struct radius_client {
	struct radius_client *next;
	struct in_addr addr;
	struct in_addr mask;
#ifdef CONFIG_IPV6
	struct in6_addr addr6;
	struct in6_addr mask6;
#endif /* CONFIG_IPV6 */
	char *shared_secret;
	int shared_secret_len;
	struct radius_session *sessions;
	struct radius_server_counters counters;
};

struct radius_server_data {
	int auth_sock;
	struct radius_client *clients;
	unsigned int next_sess_id;
	void *hostapd_conf;
	int num_sess;
	void *eap_sim_db_priv;
	void *ssl_ctx;
	int ipv6;
	struct os_time start_time;
	struct radius_server_counters counters;
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
radius_server_get_client(struct radius_server_data *data, struct in_addr *addr,
			 int ipv6)
{
	struct radius_client *client = data->clients;

	while (client) {
#ifdef CONFIG_IPV6
		if (ipv6) {
			struct in6_addr *addr6;
			int i;

			addr6 = (struct in6_addr *) addr;
			for (i = 0; i < 16; i++) {
				if ((addr6->s6_addr[i] &
				     client->mask6.s6_addr[i]) !=
				    (client->addr6.s6_addr[i] &
				     client->mask6.s6_addr[i])) {
					i = 17;
					break;
				}
			}
			if (i == 16) {
				break;
			}
		}
#endif /* CONFIG_IPV6 */
		if (!ipv6 && (client->addr.s_addr & client->mask.s_addr) ==
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
	if (sess->last_msg) {
		radius_msg_free(sess->last_msg);
		free(sess->last_msg);
	}
	free(sess->last_from_addr);
	if (sess->last_reply) {
		radius_msg_free(sess->last_reply);
		free(sess->last_reply);
	}
	free(sess);
	data->num_sess--;
}


static void radius_server_session_remove_timeout(void *eloop_ctx,
						 void *timeout_ctx);

static void radius_server_session_remove(struct radius_server_data *data,
					 struct radius_session *sess)
{
	struct radius_client *client = sess->client;
	struct radius_session *session, *prev;

	eloop_cancel_timeout(radius_server_session_remove_timeout, data, sess);

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


static void radius_server_session_remove_timeout(void *eloop_ctx,
						 void *timeout_ctx)
{
	struct radius_server_data *data = eloop_ctx;
	struct radius_session *sess = timeout_ctx;
	RADIUS_DEBUG("Removing completed session 0x%x", sess->sess_id);
	radius_server_session_remove(data, sess);
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

	sess = wpa_zalloc(sizeof(*sess));
	if (sess == NULL)
		return NULL;

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
		RADIUS_DEBUG("Failed to allocate reply message");
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
				struct sockaddr *from, socklen_t fromlen,
				const char *from_addr, int from_port)
{
	struct radius_msg *msg;
	int ret = 0;
	struct eap_hdr eapfail;

	RADIUS_DEBUG("Reject invalid request from %s:%d",
		     from_addr, from_port);

	msg = radius_msg_new(RADIUS_CODE_ACCESS_REJECT,
			     request->hdr->identifier);
	if (msg == NULL) {
		return -1;
	}

	memset(&eapfail, 0, sizeof(eapfail));
	eapfail.code = EAP_CODE_FAILURE;
	eapfail.identifier = 0;
	eapfail.length = htons(sizeof(eapfail));

	if (!radius_msg_add_eap(msg, (u8 *) &eapfail, sizeof(eapfail))) {
		RADIUS_DEBUG("Failed to add EAP-Message attribute");
	}


	if (radius_msg_finish_srv(msg, (u8 *) client->shared_secret,
				  client->shared_secret_len,
				  request->hdr->authenticator) < 0) {
		RADIUS_DEBUG("Failed to add Message-Authenticator attribute");
	}

	if (wpa_debug_level <= MSG_MSGDUMP) {
		radius_msg_dump(msg);
	}

	data->counters.access_rejects++;
	client->counters.access_rejects++;
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
				 struct sockaddr *from, socklen_t fromlen,
				 struct radius_client *client,
				 const char *from_addr, int from_port,
				 struct radius_session *force_sess)
{
	u8 *eap = NULL;
	size_t eap_len;
	int res, state_included = 0;
	u8 statebuf[4], resp_id;
	unsigned int state;
	struct radius_session *sess;
	struct radius_msg *reply;
	struct eap_hdr *hdr;

	if (force_sess)
		sess = force_sess;
	else {
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
	}

	if (sess) {
		RADIUS_DEBUG("Request for session 0x%x", sess->sess_id);
	} else if (state_included) {
		RADIUS_DEBUG("State attribute included but no session found");
		radius_server_reject(data, client, msg, from, fromlen,
				     from_addr, from_port);
		return -1;
	} else {
		sess = radius_server_get_new_session(data, client, msg);
		if (sess == NULL) {
			RADIUS_DEBUG("Could not create a new session");
			radius_server_reject(data, client, msg, from, fromlen,
					     from_addr, from_port);
			return -1;
		}
	}

	if (sess->last_from_port == from_port &&
	    sess->last_identifier == msg->hdr->identifier &&
	    os_memcmp(sess->last_authenticator, msg->hdr->authenticator, 16) ==
	    0) {
		RADIUS_DEBUG("Duplicate message from %s", from_addr);
		data->counters.dup_access_requests++;
		client->counters.dup_access_requests++;

		if (sess->last_reply) {
			res = sendto(data->auth_sock, sess->last_reply->buf,
				     sess->last_reply->buf_used, 0,
				     (struct sockaddr *) from, fromlen);
			if (res < 0) {
				perror("sendto[RADIUS SRV]");
			}
			return 0;
		}

		RADIUS_DEBUG("No previous reply available for duplicate "
			     "message");
		return -1;
	}
		      
	eap = radius_msg_get_eap(msg, &eap_len);
	if (eap == NULL) {
		RADIUS_DEBUG("No EAP-Message in RADIUS packet from %s",
			     from_addr);
		data->counters.packets_dropped++;
		client->counters.packets_dropped++;
		return -1;
	}

	RADIUS_DUMP("Received EAP data", eap, eap_len);
	if (eap_len >= sizeof(*hdr)) {
		hdr = (struct eap_hdr *) eap;
		resp_id = hdr->identifier;
	} else {
		resp_id = 0;
	}

	/* FIX: if Code is Request, Success, or Failure, send Access-Reject;
	 * RFC3579 Sect. 2.6.2.
	 * Include EAP-Response/Nak with no preferred method if
	 * code == request.
	 * If code is not 1-4, discard the packet silently.
	 * Or is this already done by the EAP state machine? */

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
		hdr = wpa_zalloc(sizeof(*hdr));
		if (hdr) {
			hdr->identifier = resp_id;
			hdr->length = htons(sizeof(*hdr));
			sess->eapReqData = (u8 *) hdr;
			sess->eapReqDataLen = sizeof(*hdr);
		}
	} else if (eap_sm_method_pending(sess->eap)) {
		if (sess->last_msg) {
			radius_msg_free(sess->last_msg);
			free(sess->last_msg);
		}
		sess->last_msg = msg;
		sess->last_from_port = from_port;
		free(sess->last_from_addr);
		sess->last_from_addr = strdup(from_addr);
		sess->last_fromlen = fromlen;
		memcpy(&sess->last_from, from, fromlen);
		return -2;
	} else {
		RADIUS_DEBUG("No EAP data from the state machine - ignore this"
			     " Access-Request silently (assuming it was a "
			     "duplicate)");
		data->counters.packets_dropped++;
		client->counters.packets_dropped++;
		return -1;
	}

	reply = radius_server_encapsulate_eap(data, client, sess, msg);

	free(sess->eapReqData);
	sess->eapReqData = NULL;
	sess->eapReqDataLen = 0;

	if (reply) {
		RADIUS_DEBUG("Reply to %s:%d", from_addr, from_port);
		if (wpa_debug_level <= MSG_MSGDUMP) {
			radius_msg_dump(reply);
		}

		switch (reply->hdr->code) {
		case RADIUS_CODE_ACCESS_ACCEPT:
			data->counters.access_accepts++;
			client->counters.access_accepts++;
			break;
		case RADIUS_CODE_ACCESS_REJECT:
			data->counters.access_rejects++;
			client->counters.access_rejects++;
			break;
		case RADIUS_CODE_ACCESS_CHALLENGE:
			data->counters.access_challenges++;
			client->counters.access_challenges++;
			break;
		}
		res = sendto(data->auth_sock, reply->buf, reply->buf_used, 0,
			     (struct sockaddr *) from, fromlen);
		if (res < 0) {
			perror("sendto[RADIUS SRV]");
		}
		if (sess->last_reply) {
			radius_msg_free(sess->last_reply);
			free(sess->last_reply);
		}
		sess->last_reply = reply;
		sess->last_from_port = from_port;
		sess->last_identifier = msg->hdr->identifier;
		os_memcpy(sess->last_authenticator, msg->hdr->authenticator,
			  16);
	} else {
		data->counters.packets_dropped++;
		client->counters.packets_dropped++;
	}

	if (sess->eapSuccess || sess->eapFail) {
		RADIUS_DEBUG("Removing completed session 0x%x after timeout",
			     sess->sess_id);
		eloop_cancel_timeout(radius_server_session_remove_timeout,
				     data, sess);
		eloop_register_timeout(10, 0,
				       radius_server_session_remove_timeout,
				       data, sess);
	}

	return 0;
}


static void radius_server_receive_auth(int sock, void *eloop_ctx,
				       void *sock_ctx)
{
	struct radius_server_data *data = eloop_ctx;
	u8 *buf = NULL;
	struct sockaddr_storage from;
	socklen_t fromlen;
	int len;
	struct radius_client *client = NULL;
	struct radius_msg *msg = NULL;
	char abuf[50];
	int from_port = 0;

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

#ifdef CONFIG_IPV6
	if (data->ipv6) {
		struct sockaddr_in6 *from6 = (struct sockaddr_in6 *) &from;
		if (inet_ntop(AF_INET6, &from6->sin6_addr, abuf, sizeof(abuf))
		    == NULL)
			abuf[0] = '\0';
		from_port = ntohs(from6->sin6_port);
		RADIUS_DEBUG("Received %d bytes from %s:%d",
			     len, abuf, from_port);

		client = radius_server_get_client(data,
						  (struct in_addr *)
						  &from6->sin6_addr, 1);
	}
#endif /* CONFIG_IPV6 */

	if (!data->ipv6) {
		struct sockaddr_in *from4 = (struct sockaddr_in *) &from;
		snprintf(abuf, sizeof(abuf), "%s", inet_ntoa(from4->sin_addr));
		from_port = ntohs(from4->sin_port);
		RADIUS_DEBUG("Received %d bytes from %s:%d",
			     len, abuf, from_port);

		client = radius_server_get_client(data, &from4->sin_addr, 0);
	}

	RADIUS_DUMP("Received data", buf, len);

	if (client == NULL) {
		RADIUS_DEBUG("Unknown client %s - packet ignored", abuf);
		data->counters.invalid_requests++;
		goto fail;
	}

	msg = radius_msg_parse(buf, len);
	if (msg == NULL) {
		RADIUS_DEBUG("Parsing incoming RADIUS frame failed");
		data->counters.malformed_access_requests++;
		client->counters.malformed_access_requests++;
		goto fail;
	}

	free(buf);
	buf = NULL;

	if (wpa_debug_level <= MSG_MSGDUMP) {
		radius_msg_dump(msg);
	}

	if (msg->hdr->code != RADIUS_CODE_ACCESS_REQUEST) {
		RADIUS_DEBUG("Unexpected RADIUS code %d", msg->hdr->code);
		data->counters.unknown_types++;
		client->counters.unknown_types++;
		goto fail;
	}

	data->counters.access_requests++;
	client->counters.access_requests++;

	if (radius_msg_verify_msg_auth(msg, (u8 *) client->shared_secret,
				       client->shared_secret_len, NULL)) {
		RADIUS_DEBUG("Invalid Message-Authenticator from %s", abuf);
		data->counters.bad_authenticators++;
		client->counters.bad_authenticators++;
		goto fail;
	}

	if (radius_server_request(data, msg, (struct sockaddr *) &from,
				  fromlen, client, abuf, from_port, NULL) ==
	    -2)
		return; /* msg was stored with the session */

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


#ifdef CONFIG_IPV6
static int radius_server_open_socket6(int port)
{
	int s;
	struct sockaddr_in6 addr;

	s = socket(PF_INET6, SOCK_DGRAM, 0);
	if (s < 0) {
		perror("socket[IPv6]");
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin6_family = AF_INET6;
	memcpy(&addr.sin6_addr, &in6addr_any, sizeof(in6addr_any));
	addr.sin6_port = htons(port);
	if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		perror("bind");
		close(s);
		return -1;
	}

	return s;
}
#endif /* CONFIG_IPV6 */


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
radius_server_read_clients(const char *client_file, int ipv6)
{
	FILE *f;
	const int buf_size = 1024;
	char *buf, *pos;
	struct radius_client *clients, *tail, *entry;
	int line = 0, mask, failed = 0, i;
	struct in_addr addr;
#ifdef CONFIG_IPV6
	struct in6_addr addr6;
#endif /* CONFIG_IPV6 */
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
		 * fe80::211:22ff:fe33:4455/64 secretipv6
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
		while ((*pos >= '0' && *pos <= '9') || *pos == '.' ||
		       (*pos >= 'a' && *pos <= 'f') || *pos == ':' ||
		       (*pos >= 'A' && *pos <= 'F')) {
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
			if ((pos == end) ||
			    (mask < 0 || mask > (ipv6 ? 128 : 32))) {
				failed = 1;
				break;
			}
			pos = end;
		} else {
			mask = ipv6 ? 128 : 32;
			*pos++ = '\0';
		}

		if (!ipv6 && inet_aton(buf, &addr) == 0) {
			failed = 1;
			break;
		}
#ifdef CONFIG_IPV6
		if (ipv6 && inet_pton(AF_INET6, buf, &addr6) <= 0) {
			if (inet_pton(AF_INET, buf, &addr) <= 0) {
				failed = 1;
				break;
			}
			/* Convert IPv4 address to IPv6 */
			if (mask <= 32)
				mask += (128 - 32);
			memset(addr6.s6_addr, 0, 10);
			addr6.s6_addr[10] = 0xff;
			addr6.s6_addr[11] = 0xff;
			memcpy(addr6.s6_addr + 12, (char *) &addr.s_addr, 4);
		}
#endif /* CONFIG_IPV6 */

		while (*pos == ' ' || *pos == '\t') {
			pos++;
		}

		if (*pos == '\0') {
			failed = 1;
			break;
		}

		entry = wpa_zalloc(sizeof(*entry));
		if (entry == NULL) {
			failed = 1;
			break;
		}
		entry->shared_secret = strdup(pos);
		if (entry->shared_secret == NULL) {
			failed = 1;
			free(entry);
			break;
		}
		entry->shared_secret_len = strlen(entry->shared_secret);
		entry->addr.s_addr = addr.s_addr;
		if (!ipv6) {
			val = 0;
			for (i = 0; i < mask; i++)
				val |= 1 << (31 - i);
			entry->mask.s_addr = htonl(val);
		}
#ifdef CONFIG_IPV6
		if (ipv6) {
			int offset = mask / 8;

			memcpy(entry->addr6.s6_addr, addr6.s6_addr, 16);
			memset(entry->mask6.s6_addr, 0xff, offset);
			val = 0;
			for (i = 0; i < (mask % 8); i++)
				val |= 1 << (7 - i);
			if (offset < 16)
				entry->mask6.s6_addr[offset] = val;
		}
#endif /* CONFIG_IPV6 */

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

#ifndef CONFIG_IPV6
	if (conf->ipv6) {
		fprintf(stderr, "RADIUS server compiled without IPv6 "
			"support.\n");
		return NULL;
	}
#endif /* CONFIG_IPV6 */

	data = wpa_zalloc(sizeof(*data));
	if (data == NULL)
		return NULL;

	os_get_time(&data->start_time);
	data->hostapd_conf = conf->hostapd_conf;
	data->eap_sim_db_priv = conf->eap_sim_db_priv;
	data->ssl_ctx = conf->ssl_ctx;
	data->ipv6 = conf->ipv6;

	data->clients = radius_server_read_clients(conf->client_file,
						   conf->ipv6);
	if (data->clients == NULL) {
		printf("No RADIUS clients configured.\n");
		radius_server_deinit(data);
		return NULL;
	}

#ifdef CONFIG_IPV6
	if (conf->ipv6)
		data->auth_sock = radius_server_open_socket6(conf->auth_port);
	else
#endif /* CONFIG_IPV6 */
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
	int ret, uptime;
	unsigned int idx;
	char *end, *pos;
	struct os_time now;
	struct radius_client *cli;

	/* RFC 2619 - RADIUS Authentication Server MIB */

	if (data == NULL || buflen == 0)
		return 0;

	pos = buf;
	end = buf + buflen;

	os_get_time(&now);
	uptime = (now.sec - data->start_time.sec) * 100 +
		((now.usec - data->start_time.usec) / 10000) % 100;
	ret = snprintf(pos, end - pos,
		       "RADIUS-AUTH-SERVER-MIB\n"
		       "radiusAuthServIdent=hostapd\n"
		       "radiusAuthServUpTime=%d\n"
		       "radiusAuthServResetTime=0\n"
		       "radiusAuthServConfigReset=4\n",
		       uptime);
	if (ret < 0 || ret >= end - pos) {
		*pos = '\0';
		return pos - buf;
	}
	pos += ret;

	ret = snprintf(pos, end - pos,
		       "radiusAuthServTotalAccessRequests=%u\n"
		       "radiusAuthServTotalInvalidRequests=%u\n"
		       "radiusAuthServTotalDupAccessRequests=%u\n"
		       "radiusAuthServTotalAccessAccepts=%u\n"
		       "radiusAuthServTotalAccessRejects=%u\n"
		       "radiusAuthServTotalAccessChallenges=%u\n"
		       "radiusAuthServTotalMalformedAccessRequests=%u\n"
		       "radiusAuthServTotalBadAuthenticators=%u\n"
		       "radiusAuthServTotalPacketsDropped=%u\n"
		       "radiusAuthServTotalUnknownTypes=%u\n",
		       data->counters.access_requests,
		       data->counters.invalid_requests,
		       data->counters.dup_access_requests,
		       data->counters.access_accepts,
		       data->counters.access_rejects,
		       data->counters.access_challenges,
		       data->counters.malformed_access_requests,
		       data->counters.bad_authenticators,
		       data->counters.packets_dropped,
		       data->counters.unknown_types);
	if (ret < 0 || ret >= end - pos) {
		*pos = '\0';
		return pos - buf;
	}
	pos += ret;

	for (cli = data->clients, idx = 0; cli; cli = cli->next, idx++) {
		char abuf[50], mbuf[50];
#ifdef CONFIG_IPV6
		if (data->ipv6) {
			if (inet_ntop(AF_INET6, &cli->addr6, abuf,
				      sizeof(abuf)) == NULL)
				abuf[0] = '\0';
			if (inet_ntop(AF_INET6, &cli->mask6, abuf,
				      sizeof(mbuf)) == NULL)
				mbuf[0] = '\0';
		}
#endif /* CONFIG_IPV6 */
		if (!data->ipv6) {
			snprintf(abuf, sizeof(abuf), "%s",
				 inet_ntoa(cli->addr));
			snprintf(mbuf, sizeof(mbuf), "%s",
				 inet_ntoa(cli->mask));
		}

		ret = snprintf(pos, end - pos,
			       "radiusAuthClientIndex=%u\n"
			       "radiusAuthClientAddress=%s/%s\n"
			       "radiusAuthServAccessRequests=%u\n"
			       "radiusAuthServDupAccessRequests=%u\n"
			       "radiusAuthServAccessAccepts=%u\n"
			       "radiusAuthServAccessRejects=%u\n"
			       "radiusAuthServAccessChallenges=%u\n"
			       "radiusAuthServMalformedAccessRequests=%u\n"
			       "radiusAuthServBadAuthenticators=%u\n"
			       "radiusAuthServPacketsDropped=%u\n"
			       "radiusAuthServUnknownTypes=%u\n",
			       idx,
			       abuf, mbuf,
			       cli->counters.access_requests,
			       cli->counters.dup_access_requests,
			       cli->counters.access_accepts,
			       cli->counters.access_rejects,
			       cli->counters.access_challenges,
			       cli->counters.malformed_access_requests,
			       cli->counters.bad_authenticators,
			       cli->counters.packets_dropped,
			       cli->counters.unknown_types);
		if (ret < 0 || ret >= end - pos) {
			*pos = '\0';
			return pos - buf;
		}
		pos += ret;
	}

	return pos - buf;
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
	int i, count;

	eap_user = hostapd_get_eap_user(sess->server->hostapd_conf, identity,
					identity_len, phase2);
	if (eap_user == NULL)
		return -1;

	memset(user, 0, sizeof(*user));
	count = EAP_USER_MAX_METHODS;
	if (count > EAP_MAX_METHODS)
		count = EAP_MAX_METHODS;
	for (i = 0; i < count; i++) {
		user->methods[i].vendor = eap_user->methods[i].vendor;
		user->methods[i].method = eap_user->methods[i].method;
	}

	if (eap_user->password) {
		user->password = malloc(eap_user->password_len);
		if (user->password == NULL)
			return -1;
		memcpy(user->password, eap_user->password,
		       eap_user->password_len);
		user->password_len = eap_user->password_len;
		user->password_hash = eap_user->password_hash;
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


void radius_server_eap_pending_cb(struct radius_server_data *data, void *ctx)
{
	struct radius_client *cli;
	struct radius_session *s, *sess = NULL;
	struct radius_msg *msg;

	if (data == NULL)
		return;

	for (cli = data->clients; cli; cli = cli->next) {
		for (s = cli->sessions; s; s = s->next) {
			if (s->eap == ctx && s->last_msg) {
				sess = s;
				break;
			}
			if (sess)
				break;
		}
		if (sess)
			break;
	}

	if (sess == NULL) {
		RADIUS_DEBUG("No session matched callback ctx");
		return;
	}

	msg = sess->last_msg;
	sess->last_msg = NULL;
	eap_sm_pending_cb(sess->eap);
	if (radius_server_request(data, msg,
				  (struct sockaddr *) &sess->last_from,
				  sess->last_fromlen, cli,
				  sess->last_from_addr,
				  sess->last_from_port, sess) == -2)
		return; /* msg was stored with the session */

	radius_msg_free(msg);
	free(msg);
}
