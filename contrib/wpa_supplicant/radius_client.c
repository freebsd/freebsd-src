/*
 * Host AP (software wireless LAN access point) user space daemon for
 * Host AP kernel driver / RADIUS client / modified for eapol_test
 * Copyright (c) 2002-2004, Jouni Malinen <jkmaline@cc.hut.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "common.h"
#include "wpa.h"
#include "config_ssid.h"
#include "wpa_supplicant.h"
#include "wpa_supplicant_i.h"
#include "radius.h"
#include "radius_client.h"
#include "eloop.h"
#include "l2_packet.h"

#include "wpa_supplicant.h"
#define hostapd_logger(h, a, m, l, t...) wpa_printf(MSG_DEBUG, t)
#define HOSTAPD_DEBUG(l, a...) wpa_printf(MSG_DEBUG, a)
#define HOSTAPD_DEBUG_COND(l) 1

/* Defaults for RADIUS retransmit values (exponential backoff) */
#define RADIUS_CLIENT_FIRST_WAIT 1 /* seconds */
#define RADIUS_CLIENT_MAX_WAIT 120 /* seconds */
#define RADIUS_CLIENT_MAX_RETRIES 10 /* maximum number of retransmit attempts
				      * before entry is removed from retransmit
				      * list */
#define RADIUS_CLIENT_MAX_ENTRIES 30 /* maximum number of entries in retransmit
				      * list (oldest will be removed, if this
				      * limit is exceeded) */
#define RADIUS_CLIENT_NUM_FAILOVER 4 /* try to change RADIUS server after this
				      * many failed retry attempts */



static int
radius_change_server(struct wpa_supplicant *wpa_s, struct hostapd_radius_server *nserv,
		     struct hostapd_radius_server *oserv,
		     int sock, int auth);


static void radius_client_msg_free(struct radius_msg_list *req)
{
	radius_msg_free(req->msg);
	free(req->msg);
	free(req);
}


int radius_client_register(struct wpa_supplicant *wpa_s, RadiusType msg_type,
			   RadiusRxResult (*handler)(struct wpa_supplicant *wpa_s,
						     struct radius_msg *msg,
						     struct radius_msg *req,
						     u8 *shared_secret,
						     size_t shared_secret_len,
						     void *data),
			   void *data)
{
	struct radius_rx_handler **handlers, *newh;
	size_t *num;

	if (msg_type == RADIUS_ACCT) {
		handlers = &wpa_s->radius->acct_handlers;
		num = &wpa_s->radius->num_acct_handlers;
	} else {
		handlers = &wpa_s->radius->auth_handlers;
		num = &wpa_s->radius->num_auth_handlers;
	}

	newh = (struct radius_rx_handler *)
		realloc(*handlers,
			(*num + 1) * sizeof(struct radius_rx_handler));
	if (newh == NULL)
		return -1;

	newh[*num].handler = handler;
	newh[*num].data = data;
	(*num)++;
	*handlers = newh;

	return 0;
}


static int radius_client_retransmit(struct wpa_supplicant *wpa_s,
				    struct radius_msg_list *entry, time_t now)
{
	int s;

	if (entry->msg_type == RADIUS_ACCT ||
	    entry->msg_type == RADIUS_ACCT_INTERIM)
		s = wpa_s->radius->acct_serv_sock;
	else
		s = wpa_s->radius->auth_serv_sock;

	/* retransmit; remove entry if too many attempts */
	entry->attempts++;
	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL, "Resending RADIUS message (id=%d)"
		      "\n", entry->msg->hdr->identifier);

	if (send(s, entry->msg->buf, entry->msg->buf_used, 0) < 0)
		perror("send[RADIUS]");

	entry->next_try = now + entry->next_wait;
	entry->next_wait *= 2;
	if (entry->next_wait > RADIUS_CLIENT_MAX_WAIT)
		entry->next_wait = RADIUS_CLIENT_MAX_WAIT;
	if (entry->attempts >= RADIUS_CLIENT_MAX_RETRIES) {
		printf("Removing un-ACKed RADIUS message due to too many "
		       "failed retransmit attempts\n");
		return 1;
	}

	return 0;
}


static void radius_client_timer(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	time_t now, first;
	struct radius_msg_list *entry, *prev, *tmp;
	int auth_failover = 0, acct_failover = 0;

	entry = wpa_s->radius->msgs;
	if (!entry)
		return;

	time(&now);
	first = 0;

	prev = NULL;
	while (entry) {
		if (now >= entry->next_try &&
		    radius_client_retransmit(wpa_s, entry, now)) {
			if (prev)
				prev->next = entry->next;
			else
				wpa_s->radius->msgs = entry->next;

			tmp = entry;
			entry = entry->next;
			radius_client_msg_free(tmp);
			wpa_s->radius->num_msgs--;
			continue;
		}

		if (entry->attempts > RADIUS_CLIENT_NUM_FAILOVER) {
			if (entry->msg_type == RADIUS_ACCT ||
			    entry->msg_type == RADIUS_ACCT_INTERIM)
				acct_failover++;
			else
				auth_failover++;
		}

		if (first == 0 || entry->next_try < first)
			first = entry->next_try;

		prev = entry;
		entry = entry->next;
	}

	if (wpa_s->radius->msgs) {
		if (first < now)
			first = now;
		eloop_register_timeout(first - now, 0,
				       radius_client_timer, wpa_s, NULL);
	}

	if (auth_failover && wpa_s->num_auth_servers > 1) {
		struct hostapd_radius_server *next, *old;
		old = wpa_s->auth_server;
		hostapd_logger(wpa_s, NULL, HOSTAPD_MODULE_RADIUS,
			       HOSTAPD_LEVEL_NOTICE,
			       "No response from Authentication server "
			       "%s:%d - failover",
			       inet_ntoa(old->addr), old->port);

		next = old + 1;
		if (next > &(wpa_s->auth_servers
			     [wpa_s->num_auth_servers - 1]))
			next = wpa_s->auth_servers;
		wpa_s->auth_server = next;
		radius_change_server(wpa_s, next, old,
				     wpa_s->radius->auth_serv_sock, 1);
	}

	if (acct_failover && wpa_s->num_acct_servers > 1) {
		struct hostapd_radius_server *next, *old;
		old = wpa_s->acct_server;
		hostapd_logger(wpa_s, NULL, HOSTAPD_MODULE_RADIUS,
			       HOSTAPD_LEVEL_NOTICE,
			       "No response from Accounting server "
			       "%s:%d - failover",
			       inet_ntoa(old->addr), old->port);
		next = old + 1;
		if (next > &wpa_s->acct_servers
		    [wpa_s->num_acct_servers - 1])
			next = wpa_s->acct_servers;
		wpa_s->acct_server = next;
		radius_change_server(wpa_s, next, old,
				     wpa_s->radius->acct_serv_sock, 0);
	}
}


static void radius_client_list_add(struct wpa_supplicant *wpa_s, struct radius_msg *msg,
				   RadiusType msg_type, u8 *shared_secret,
				   size_t shared_secret_len, u8 *addr)
{
	struct radius_msg_list *entry, *prev;

	if (eloop_terminated()) {
		/* No point in adding entries to retransmit queue since event
		 * loop has already been terminated. */
		radius_msg_free(msg);
		free(msg);
		return;
	}

	entry = malloc(sizeof(*entry));
	if (entry == NULL) {
		printf("Failed to add RADIUS packet into retransmit list\n");
		radius_msg_free(msg);
		free(msg);
		return;
	}

	memset(entry, 0, sizeof(*entry));
	if (addr)
		memcpy(entry->addr, addr, ETH_ALEN);
	entry->msg = msg;
	entry->msg_type = msg_type;
	entry->shared_secret = shared_secret;
	entry->shared_secret_len = shared_secret_len;
	time(&entry->first_try);
	entry->next_try = entry->first_try + RADIUS_CLIENT_FIRST_WAIT;
	entry->attempts = 1;
	entry->next_wait = RADIUS_CLIENT_FIRST_WAIT * 2;

	if (!wpa_s->radius->msgs)
		eloop_register_timeout(RADIUS_CLIENT_FIRST_WAIT, 0,
				       radius_client_timer, wpa_s, NULL);

	entry->next = wpa_s->radius->msgs;
	wpa_s->radius->msgs = entry;

	if (wpa_s->radius->num_msgs >= RADIUS_CLIENT_MAX_ENTRIES) {
		printf("Removing the oldest un-ACKed RADIUS packet due to "
		       "retransmit list limits.\n");
		prev = NULL;
		while (entry->next) {
			prev = entry;
			entry = entry->next;
		}
		if (prev) {
			prev->next = NULL;
			radius_client_msg_free(entry);
		}
	} else
		wpa_s->radius->num_msgs++;
}


static void radius_client_list_del(struct wpa_supplicant *wpa_s,
				   RadiusType msg_type, u8 *addr)
{
	struct radius_msg_list *entry, *prev, *tmp;

	if (addr == NULL)
		return;

	entry = wpa_s->radius->msgs;
	prev = NULL;
	while (entry) {
		if (entry->msg_type == msg_type &&
		    memcmp(entry->addr, addr, ETH_ALEN) == 0) {
			if (prev)
				prev->next = entry->next;
			else
				wpa_s->radius->msgs = entry->next;
			tmp = entry;
			entry = entry->next;
			HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
				      "Removing matching RADIUS message for "
				      MACSTR "\n", MAC2STR(addr));
			radius_client_msg_free(tmp);
			wpa_s->radius->num_msgs--;
			continue;
		}
		prev = entry;
		entry = entry->next;
	}
}


int radius_client_send(struct wpa_supplicant *wpa_s, struct radius_msg *msg,
		       RadiusType msg_type, u8 *addr)
{
	u8 *shared_secret;
	size_t shared_secret_len;
	char *name;
	int s, res;

	if (msg_type == RADIUS_ACCT_INTERIM) {
		/* Remove any pending interim acct update for the same STA. */
		radius_client_list_del(wpa_s, msg_type, addr);
	}

	if (msg_type == RADIUS_ACCT || msg_type == RADIUS_ACCT_INTERIM) {
		shared_secret = wpa_s->acct_server->shared_secret;
		shared_secret_len = wpa_s->acct_server->shared_secret_len;
		radius_msg_finish_acct(msg, shared_secret, shared_secret_len);
		name = "accounting";
		s = wpa_s->radius->acct_serv_sock;
	} else {
		shared_secret = wpa_s->auth_server->shared_secret;
		shared_secret_len = wpa_s->auth_server->shared_secret_len;
		radius_msg_finish(msg, shared_secret, shared_secret_len);
		name = "authentication";
		s = wpa_s->radius->auth_serv_sock;
	}

	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
		      "Sending RADIUS message to %s server\n", name);
	if (HOSTAPD_DEBUG_COND(HOSTAPD_DEBUG_MSGDUMPS))
		radius_msg_dump(msg);

	res = send(s, msg->buf, msg->buf_used, 0);
	if (res < 0)
		perror("send[RADIUS]");

	radius_client_list_add(wpa_s, msg, msg_type, shared_secret,
			       shared_secret_len, addr);

	return res;
}


static void radius_client_receive(int sock, void *eloop_ctx, void *sock_ctx)
{
	struct wpa_supplicant *wpa_s = (struct wpa_supplicant *) eloop_ctx;
	RadiusType msg_type = (RadiusType) sock_ctx;
	int len, i;
	unsigned char buf[3000];
	struct radius_msg *msg;
	struct radius_rx_handler *handlers;
	size_t num_handlers;
	struct radius_msg_list *req, *prev_req;

	len = recv(sock, buf, sizeof(buf), 0);
	if (len < 0) {
		perror("recv[RADIUS]");
		return;
	}
	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
		      "Received %d bytes from RADIUS server\n", len);
	if (len == sizeof(buf)) {
		printf("Possibly too long UDP frame for our buffer - "
		       "dropping it\n");
		return;
	}

	msg = radius_msg_parse(buf, len);
	if (msg == NULL) {
		printf("Parsing incoming RADIUS frame failed\n");
		return;
	}

	HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
		      "Received RADIUS message\n");
	if (HOSTAPD_DEBUG_COND(HOSTAPD_DEBUG_MSGDUMPS))
		radius_msg_dump(msg);

	if (msg_type == RADIUS_ACCT) {
		handlers = wpa_s->radius->acct_handlers;
		num_handlers = wpa_s->radius->num_acct_handlers;
	} else {
		handlers = wpa_s->radius->auth_handlers;
		num_handlers = wpa_s->radius->num_auth_handlers;
	}

	prev_req = NULL;
	req = wpa_s->radius->msgs;
	while (req) {
		/* TODO: also match by src addr:port of the packet when using
		 * alternative RADIUS servers (?) */
		if ((req->msg_type == msg_type ||
		     (req->msg_type == RADIUS_ACCT_INTERIM &&
		      msg_type == RADIUS_ACCT)) &&
		    req->msg->hdr->identifier == msg->hdr->identifier)
			break;

		prev_req = req;
		req = req->next;
	}

	if (req == NULL) {
		HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
			      "No matching RADIUS request found (type=%d "
			      "id=%d) - dropping packet\n",
			      msg_type, msg->hdr->identifier);
		goto fail;
	}

	/* Remove ACKed RADIUS packet from retransmit list */
	if (prev_req)
		prev_req->next = req->next;
	else
		wpa_s->radius->msgs = req->next;
	wpa_s->radius->num_msgs--;

	for (i = 0; i < num_handlers; i++) {
		RadiusRxResult res;
		res = handlers[i].handler(wpa_s, msg, req->msg,
					  req->shared_secret,
					  req->shared_secret_len,
					  handlers[i].data);
		switch (res) {
		case RADIUS_RX_PROCESSED:
			radius_msg_free(msg);
			free(msg);
			/* continue */
		case RADIUS_RX_QUEUED:
			radius_client_msg_free(req);
			return;
		case RADIUS_RX_UNKNOWN:
			/* continue with next handler */
			break;
		}
	}

	printf("No RADIUS RX handler found (type=%d code=%d id=%d) - dropping "
	       "packet\n", msg_type, msg->hdr->code, msg->hdr->identifier);
	radius_client_msg_free(req);

 fail:
	radius_msg_free(msg);
	free(msg);
}


u8 radius_client_get_id(struct wpa_supplicant *wpa_s)
{
	struct radius_msg_list *entry, *prev, *remove;
	u8 id = wpa_s->radius->next_radius_identifier++;

	/* remove entries with matching id from retransmit list to avoid
	 * using new reply from the RADIUS server with an old request */
	entry = wpa_s->radius->msgs;
	prev = NULL;
	while (entry) {
		if (entry->msg->hdr->identifier == id) {
			HOSTAPD_DEBUG(HOSTAPD_DEBUG_MINIMAL,
				      "Removing pending RADIUS message, since "
				      "its id (%d) is reused\n", id);
			if (prev)
				prev->next = entry->next;
			else
				wpa_s->radius->msgs = entry->next;
			remove = entry;
		} else
			remove = NULL;
		prev = entry;
		entry = entry->next;

		if (remove)
			radius_client_msg_free(remove);
	}

	return id;
}


void radius_client_flush(struct wpa_supplicant *wpa_s)
{
	struct radius_msg_list *entry, *prev;

	if (!wpa_s->radius)
		return;

	eloop_cancel_timeout(radius_client_timer, wpa_s, NULL);

	entry = wpa_s->radius->msgs;
	wpa_s->radius->msgs = NULL;
	wpa_s->radius->num_msgs = 0;
	while (entry) {
		prev = entry;
		entry = entry->next;
		radius_client_msg_free(prev);
	}
}


static int
radius_change_server(struct wpa_supplicant *wpa_s, struct hostapd_radius_server *nserv,
		     struct hostapd_radius_server *oserv,
		     int sock, int auth)
{
	struct sockaddr_in serv;

	hostapd_logger(wpa_s, NULL, HOSTAPD_MODULE_RADIUS, HOSTAPD_LEVEL_INFO,
		       "%s server %s:%d",
		       auth ? "Authentication" : "Accounting",
		       inet_ntoa(nserv->addr), nserv->port);

	if (!oserv || nserv->shared_secret_len != oserv->shared_secret_len ||
	    memcmp(nserv->shared_secret, oserv->shared_secret,
		   nserv->shared_secret_len) != 0) {
		/* Pending RADIUS packets used different shared
		 * secret, so they would need to be modified. Could
		 * update all message authenticators and
		 * User-Passwords, etc. and retry with new server. For
		 * now, just drop all pending packets. */
		radius_client_flush(wpa_s);
	} else {
		/* Reset retry counters for the new server */
		struct radius_msg_list *entry;
		entry = wpa_s->radius->msgs;
		while (entry) {
			entry->next_try = entry->first_try +
				RADIUS_CLIENT_FIRST_WAIT;
			entry->attempts = 0;
			entry->next_wait = RADIUS_CLIENT_FIRST_WAIT * 2;
			entry = entry->next;
		}
		if (wpa_s->radius->msgs) {
			eloop_cancel_timeout(radius_client_timer, wpa_s, NULL);
			eloop_register_timeout(RADIUS_CLIENT_FIRST_WAIT, 0,
					       radius_client_timer, wpa_s,
					       NULL);
		}
	}

	memset(&serv, 0, sizeof(serv));
	serv.sin_family = AF_INET;
	serv.sin_addr.s_addr = nserv->addr.s_addr;
	serv.sin_port = htons(nserv->port);

	if (connect(sock, (struct sockaddr *) &serv, sizeof(serv)) < 0) {
		perror("connect[radius]");
		return -1;
	}

	return 0;
}


static void radius_retry_primary_timer(void *eloop_ctx, void *timeout_ctx)
{
	struct wpa_supplicant *wpa_s = eloop_ctx;
	struct hostapd_radius_server *oserv;

	if (wpa_s->radius->auth_serv_sock >= 0 && wpa_s->auth_servers &&
	    wpa_s->auth_server != wpa_s->auth_servers) {
		oserv = wpa_s->auth_server;
		wpa_s->auth_server = wpa_s->auth_servers;
		radius_change_server(wpa_s, wpa_s->auth_server, oserv,
				     wpa_s->radius->auth_serv_sock, 1);
	}

	if (wpa_s->radius->acct_serv_sock >= 0 && wpa_s->acct_servers &&
	    wpa_s->acct_server != wpa_s->acct_servers) {
		oserv = wpa_s->acct_server;
		wpa_s->acct_server = wpa_s->acct_servers;
		radius_change_server(wpa_s, wpa_s->acct_server, oserv,
				     wpa_s->radius->acct_serv_sock, 0);
	}

	if (wpa_s->radius_retry_primary_interval)
		eloop_register_timeout(wpa_s->
				       radius_retry_primary_interval, 0,
				       radius_retry_primary_timer, wpa_s, NULL);
}


static int radius_client_init_auth(struct wpa_supplicant *wpa_s)
{
	wpa_s->radius->auth_serv_sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (wpa_s->radius->auth_serv_sock < 0) {
		perror("socket[PF_INET,SOCK_DGRAM]");
		return -1;
	}

	radius_change_server(wpa_s, wpa_s->auth_server, NULL,
			     wpa_s->radius->auth_serv_sock, 1);

	if (eloop_register_read_sock(wpa_s->radius->auth_serv_sock,
				     radius_client_receive, wpa_s,
				     (void *) RADIUS_AUTH)) {
		printf("Could not register read socket for authentication "
		       "server\n");
		return -1;
	}

	return 0;
}


static int radius_client_init_acct(struct wpa_supplicant *wpa_s)
{
	wpa_s->radius->acct_serv_sock = socket(PF_INET, SOCK_DGRAM, 0);
	if (wpa_s->radius->acct_serv_sock < 0) {
		perror("socket[PF_INET,SOCK_DGRAM]");
		return -1;
	}

	radius_change_server(wpa_s, wpa_s->acct_server, NULL,
			     wpa_s->radius->acct_serv_sock, 0);

	if (eloop_register_read_sock(wpa_s->radius->acct_serv_sock,
				     radius_client_receive, wpa_s,
				     (void *) RADIUS_ACCT)) {
		printf("Could not register read socket for accounting "
		       "server\n");
		return -1;
	}

	return 0;
}


int radius_client_init(struct wpa_supplicant *wpa_s)
{
	wpa_s->radius = malloc(sizeof(struct radius_client_data));
	if (wpa_s->radius == NULL)
		return -1;

	memset(wpa_s->radius, 0, sizeof(struct radius_client_data));
	wpa_s->radius->auth_serv_sock = wpa_s->radius->acct_serv_sock = -1;

	if (wpa_s->auth_server && radius_client_init_auth(wpa_s))
		return -1;

	if (wpa_s->acct_server && radius_client_init_acct(wpa_s))
		return -1;

	if (wpa_s->radius_retry_primary_interval)
		eloop_register_timeout(wpa_s->radius_retry_primary_interval, 0,
				       radius_retry_primary_timer, wpa_s,
				       NULL);

	return 0;
}


void radius_client_deinit(struct wpa_supplicant *wpa_s)
{
	if (!wpa_s->radius)
		return;

	eloop_cancel_timeout(radius_retry_primary_timer, wpa_s, NULL);

	radius_client_flush(wpa_s);
	free(wpa_s->radius->auth_handlers);
	free(wpa_s->radius->acct_handlers);
	free(wpa_s->radius);
	wpa_s->radius = NULL;
}
