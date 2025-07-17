/*
 * RADIUS client
 * Copyright (c) 2002-2024, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"
#include <fcntl.h>
#include <net/if.h>

#include "common.h"
#include "eloop.h"
#include "crypto/tls.h"
#include "radius.h"
#include "radius_client.h"

/* Defaults for RADIUS retransmit values (exponential backoff) */

/**
 * RADIUS_CLIENT_FIRST_WAIT - RADIUS client timeout for first retry in seconds
 */
#define RADIUS_CLIENT_FIRST_WAIT 3

/**
 * RADIUS_CLIENT_MAX_WAIT - RADIUS client maximum retry timeout in seconds
 */
#define RADIUS_CLIENT_MAX_WAIT 120

/**
 * RADIUS_CLIENT_MAX_FAILOVER - RADIUS client maximum retries
 *
 * Maximum number of server failovers before the entry is removed from
 * retransmit list.
 */
#define RADIUS_CLIENT_MAX_FAILOVER 3

/**
 * RADIUS_CLIENT_MAX_ENTRIES - RADIUS client maximum pending messages
 *
 * Maximum number of entries in retransmit list (oldest entries will be
 * removed, if this limit is exceeded).
 */
#define RADIUS_CLIENT_MAX_ENTRIES 30

/**
 * RADIUS_CLIENT_NUM_FAILOVER - RADIUS client failover point
 *
 * The number of failed retry attempts after which the RADIUS server will be
 * changed (if one of more backup servers are configured).
 */
#define RADIUS_CLIENT_NUM_FAILOVER 4


/**
 * struct radius_rx_handler - RADIUS client RX handler
 *
 * This data structure is used internally inside the RADIUS client module to
 * store registered RX handlers. These handlers are registered by calls to
 * radius_client_register() and unregistered when the RADIUS client is
 * deinitialized with a call to radius_client_deinit().
 */
struct radius_rx_handler {
	/**
	 * handler - Received RADIUS message handler
	 */
	RadiusRxResult (*handler)(struct radius_msg *msg,
				  struct radius_msg *req,
				  const u8 *shared_secret,
				  size_t shared_secret_len,
				  void *data);

	/**
	 * data - Context data for the handler
	 */
	void *data;
};


/**
 * struct radius_msg_list - RADIUS client message retransmit list
 *
 * This data structure is used internally inside the RADIUS client module to
 * store pending RADIUS requests that may still need to be retransmitted.
 */
struct radius_msg_list {
	/**
	 * addr - STA/client address
	 *
	 * This is used to find RADIUS messages for the same STA.
	 */
	u8 addr[ETH_ALEN];

	/**
	 * msg - RADIUS message
	 */
	struct radius_msg *msg;

	/**
	 * msg_type - Message type
	 */
	RadiusType msg_type;

	/**
	 * first_try - Time of the first transmission attempt
	 */
	os_time_t first_try;

	/**
	 * next_try - Time for the next transmission attempt
	 */
	os_time_t next_try;

	/**
	 * attempts - Number of transmission attempts for one server
	 */
	int attempts;

	/**
	 * accu_attempts - Number of accumulated attempts
	 */
	int accu_attempts;

	/**
	 * next_wait - Next retransmission wait time in seconds
	 */
	int next_wait;

	/**
	 * last_attempt - Time of the last transmission attempt
	 */
	struct os_reltime last_attempt;

	/**
	 * shared_secret - Shared secret with the target RADIUS server
	 */
	const u8 *shared_secret;

	/**
	 * shared_secret_len - shared_secret length in octets
	 */
	size_t shared_secret_len;

	/* TODO: server config with failover to backup server(s) */

	/**
	 * next - Next message in the list
	 */
	struct radius_msg_list *next;
};


/**
 * struct radius_client_data - Internal RADIUS client data
 *
 * This data structure is used internally inside the RADIUS client module.
 * External users allocate this by calling radius_client_init() and free it by
 * calling radius_client_deinit(). The pointer to this opaque data is used in
 * calls to other functions as an identifier for the RADIUS client instance.
 */
struct radius_client_data {
	/**
	 * ctx - Context pointer for hostapd_logger() callbacks
	 */
	void *ctx;

	/**
	 * conf - RADIUS client configuration (list of RADIUS servers to use)
	 */
	struct hostapd_radius_servers *conf;

	/**
	 * auth_sock - Currently used socket for RADIUS authentication server
	 */
	int auth_sock;

	/**
	 * auth_tls - Whether current authentication connection uses TLS
	 */
	bool auth_tls;

	/**
	 * auth_tls_ready - Whether authentication TLS is ready
	 */
	bool auth_tls_ready;

	/**
	 * acct_sock - Currently used socket for RADIUS accounting server
	 */
	int acct_sock;

	/**
	 * acct_tls - Whether current accounting connection uses TLS
	 */
	bool acct_tls;

	/**
	 * acct_tls_ready - Whether accounting TLS is ready
	 */
	bool acct_tls_ready;

	/**
	 * auth_handlers - Authentication message handlers
	 */
	struct radius_rx_handler *auth_handlers;

	/**
	 * num_auth_handlers - Number of handlers in auth_handlers
	 */
	size_t num_auth_handlers;

	/**
	 * acct_handlers - Accounting message handlers
	 */
	struct radius_rx_handler *acct_handlers;

	/**
	 * num_acct_handlers - Number of handlers in acct_handlers
	 */
	size_t num_acct_handlers;

	/**
	 * msgs - Pending outgoing RADIUS messages
	 */
	struct radius_msg_list *msgs;

	/**
	 * num_msgs - Number of pending messages in the msgs list
	 */
	size_t num_msgs;

	/**
	 * next_radius_identifier - Next RADIUS message identifier to use
	 */
	u8 next_radius_identifier;

	/**
	 * interim_error_cb - Interim accounting error callback
	 */
	void (*interim_error_cb)(const u8 *addr, void *ctx);

	/**
	 * interim_error_cb_ctx - interim_error_cb() context data
	 */
	void *interim_error_cb_ctx;

#ifdef CONFIG_RADIUS_TLS
	void *tls_ctx;
	struct tls_connection *auth_tls_conn;
	struct tls_connection *acct_tls_conn;
#endif /* CONFIG_RADIUS_TLS */
};


static int
radius_change_server(struct radius_client_data *radius,
		     struct hostapd_radius_server *nserv,
		     struct hostapd_radius_server *oserv,
		     int auth);
static int radius_client_init_acct(struct radius_client_data *radius);
static int radius_client_init_auth(struct radius_client_data *radius);
static void radius_client_auth_failover(struct radius_client_data *radius);
static void radius_client_acct_failover(struct radius_client_data *radius);


static void radius_client_msg_free(struct radius_msg_list *req)
{
	radius_msg_free(req->msg);
	os_free(req);
}


/**
 * radius_client_register - Register a RADIUS client RX handler
 * @radius: RADIUS client context from radius_client_init()
 * @msg_type: RADIUS client type (RADIUS_AUTH or RADIUS_ACCT)
 * @handler: Handler for received RADIUS messages
 * @data: Context pointer for handler callbacks
 * Returns: 0 on success, -1 on failure
 *
 * This function is used to register a handler for processing received RADIUS
 * authentication and accounting messages. The handler() callback function will
 * be called whenever a RADIUS message is received from the active server.
 *
 * There can be multiple registered RADIUS message handlers. The handlers will
 * be called in order until one of them indicates that it has processed or
 * queued the message.
 */
int radius_client_register(struct radius_client_data *radius,
			   RadiusType msg_type,
			   RadiusRxResult (*handler)(struct radius_msg *msg,
						     struct radius_msg *req,
						     const u8 *shared_secret,
						     size_t shared_secret_len,
						     void *data),
			   void *data)
{
	struct radius_rx_handler **handlers, *newh;
	size_t *num;

	if (msg_type == RADIUS_ACCT) {
		handlers = &radius->acct_handlers;
		num = &radius->num_acct_handlers;
	} else {
		handlers = &radius->auth_handlers;
		num = &radius->num_auth_handlers;
	}

	newh = os_realloc_array(*handlers, *num + 1,
				sizeof(struct radius_rx_handler));
	if (newh == NULL)
		return -1;

	newh[*num].handler = handler;
	newh[*num].data = data;
	(*num)++;
	*handlers = newh;

	return 0;
}


/**
 * radius_client_set_interim_erro_cb - Register an interim acct error callback
 * @radius: RADIUS client context from radius_client_init()
 * @addr: Station address from the failed message
 * @cb: Handler for interim accounting errors
 * @ctx: Context pointer for handler callbacks
 *
 * This function is used to register a handler for processing failed
 * transmission attempts of interim accounting update messages.
 */
void radius_client_set_interim_error_cb(struct radius_client_data *radius,
					void (*cb)(const u8 *addr, void *ctx),
					void *ctx)
{
	radius->interim_error_cb = cb;
	radius->interim_error_cb_ctx = ctx;
}


/*
 * Returns >0 if message queue was flushed (i.e., the message that triggered
 * the error is not available anymore)
 */
static int radius_client_handle_send_error(struct radius_client_data *radius,
					   int s, RadiusType msg_type)
{
#ifndef CONFIG_NATIVE_WINDOWS
	int _errno = errno;
	wpa_printf(MSG_INFO, "send[RADIUS,s=%d]: %s", s, strerror(errno));
	if (_errno == ENOTCONN || _errno == EDESTADDRREQ || _errno == EINVAL ||
	    _errno == EBADF || _errno == ENETUNREACH || _errno == EACCES) {
		hostapd_logger(radius->ctx, NULL, HOSTAPD_MODULE_RADIUS,
			       HOSTAPD_LEVEL_INFO,
			       "Send failed - maybe interface status changed -"
			       " try to connect again");
		if (msg_type == RADIUS_ACCT ||
		    msg_type == RADIUS_ACCT_INTERIM) {
			radius_client_init_acct(radius);
			return 0;
		} else {
			radius_client_init_auth(radius);
			return 1;
		}
	}
#endif /* CONFIG_NATIVE_WINDOWS */

	return 0;
}


static int radius_client_retransmit(struct radius_client_data *radius,
				    struct radius_msg_list *entry,
				    os_time_t now)
{
	struct hostapd_radius_servers *conf = radius->conf;
	int s;
	struct wpabuf *buf;
	size_t prev_num_msgs;
	u8 *acct_delay_time;
	size_t acct_delay_time_len;
	int num_servers;
#ifdef CONFIG_RADIUS_TLS
	struct wpabuf *out = NULL;
	struct tls_connection *conn = NULL;
	bool acct = false;
#endif /* CONFIG_RADIUS_TLS */

	if (entry->msg_type == RADIUS_ACCT ||
	    entry->msg_type == RADIUS_ACCT_INTERIM) {
#ifdef CONFIG_RADIUS_TLS
		acct = true;
		if (radius->acct_tls)
			conn = radius->acct_tls_conn;
#endif /* CONFIG_RADIUS_TLS */
		num_servers = conf->num_acct_servers;
		if (radius->acct_sock < 0)
			radius_client_init_acct(radius);
		if (radius->acct_sock < 0 && conf->num_acct_servers > 1) {
			prev_num_msgs = radius->num_msgs;
			radius_client_acct_failover(radius);
			if (prev_num_msgs != radius->num_msgs)
				return 0;
		}
		s = radius->acct_sock;
		if (entry->attempts == 0)
			conf->acct_server->requests++;
		else {
			conf->acct_server->timeouts++;
			conf->acct_server->retransmissions++;
		}
	} else {
#ifdef CONFIG_RADIUS_TLS
		if (radius->auth_tls)
			conn = radius->auth_tls_conn;
#endif /* CONFIG_RADIUS_TLS */
		num_servers = conf->num_auth_servers;
		if (radius->auth_sock < 0)
			radius_client_init_auth(radius);
		if (radius->auth_sock < 0 && conf->num_auth_servers > 1) {
			prev_num_msgs = radius->num_msgs;
			radius_client_auth_failover(radius);
			if (prev_num_msgs != radius->num_msgs)
				return 0;
		}
		s = radius->auth_sock;
		if (entry->attempts == 0)
			conf->auth_server->requests++;
		else {
			conf->auth_server->timeouts++;
			conf->auth_server->retransmissions++;
		}
	}

	if (entry->msg_type == RADIUS_ACCT_INTERIM) {
		wpa_printf(MSG_DEBUG,
			   "RADIUS: Failed to transmit interim accounting update to "
			   MACSTR " - drop message and request a new update",
			   MAC2STR(entry->addr));
		if (radius->interim_error_cb)
			radius->interim_error_cb(entry->addr,
						 radius->interim_error_cb_ctx);
		return 1;
	}

	if (s < 0) {
		wpa_printf(MSG_INFO,
			   "RADIUS: No valid socket for retransmission");
		return 1;
	}

#ifdef CONFIG_RADIUS_TLS
	if ((acct && radius->acct_tls && !radius->acct_tls_ready) ||
	    (!acct && radius->auth_tls && !radius->auth_tls_ready)) {
		wpa_printf(MSG_DEBUG,
			   "RADIUS: TLS connection not yet ready for TX");
		goto not_ready;
	}
#endif /* CONFIG_RADIUS_TLS */

	if (entry->msg_type == RADIUS_ACCT &&
	    radius_msg_get_attr_ptr(entry->msg, RADIUS_ATTR_ACCT_DELAY_TIME,
				    &acct_delay_time, &acct_delay_time_len,
				    NULL) == 0 &&
	    acct_delay_time_len == 4) {
		struct radius_hdr *hdr;
		u32 delay_time;

		/*
		 * Need to assign a new identifier since attribute contents
		 * changes.
		 */
		hdr = radius_msg_get_hdr(entry->msg);
		hdr->identifier = radius_client_get_id(radius);

		/* Update Acct-Delay-Time to show wait time in queue */
		delay_time = now - entry->first_try;
		WPA_PUT_BE32(acct_delay_time, delay_time);

		wpa_printf(MSG_DEBUG,
			   "RADIUS: Updated Acct-Delay-Time to %u for retransmission",
			   delay_time);
		radius_msg_finish_acct(entry->msg, entry->shared_secret,
				       entry->shared_secret_len);
		if (radius->conf->msg_dumps)
			radius_msg_dump(entry->msg);
	}

	/* retransmit; remove entry if too many attempts */
	if (entry->accu_attempts >= RADIUS_CLIENT_MAX_FAILOVER *
	    RADIUS_CLIENT_NUM_FAILOVER * num_servers) {
		wpa_printf(MSG_INFO,
			   "RADIUS: Removing un-ACKed message due to too many failed retransmit attempts");
		return 1;
	}

	entry->attempts++;
	entry->accu_attempts++;
	hostapd_logger(radius->ctx, entry->addr, HOSTAPD_MODULE_RADIUS,
		       HOSTAPD_LEVEL_DEBUG, "Resending RADIUS message (id=%d)",
		       radius_msg_get_hdr(entry->msg)->identifier);

	os_get_reltime(&entry->last_attempt);
	buf = radius_msg_get_buf(entry->msg);
#ifdef CONFIG_RADIUS_TLS
	if (conn) {
		out = tls_connection_encrypt(radius->tls_ctx, conn, buf);
		if (!out) {
			wpa_printf(MSG_INFO,
				   "RADIUS: Failed to encrypt RADIUS message (TLS)");
			return -1;
		}
		wpa_printf(MSG_DEBUG,
			   "RADIUS: TLS encryption of %zu bytes of plaintext to %zu bytes of ciphertext",
			   wpabuf_len(buf), wpabuf_len(out));
		buf = out;
	}
#endif /* CONFIG_RADIUS_TLS */

	wpa_printf(MSG_DEBUG, "RADIUS: Send %zu bytes to the server",
		   wpabuf_len(buf));
	if (send(s, wpabuf_head(buf), wpabuf_len(buf), 0) < 0) {
		if (radius_client_handle_send_error(radius, s, entry->msg_type)
		    > 0) {
#ifdef CONFIG_RADIUS_TLS
			wpabuf_free(out);
#endif /* CONFIG_RADIUS_TLS */
			return 0;
		}
	}
#ifdef CONFIG_RADIUS_TLS
	wpabuf_free(out);

not_ready:
#endif /* CONFIG_RADIUS_TLS */

	entry->next_try = now + entry->next_wait;
	entry->next_wait *= 2;
	if (entry->next_wait > RADIUS_CLIENT_MAX_WAIT)
		entry->next_wait = RADIUS_CLIENT_MAX_WAIT;

	return 0;
}


static void radius_client_timer(void *eloop_ctx, void *timeout_ctx)
{
	struct radius_client_data *radius = eloop_ctx;
	struct os_reltime now;
	os_time_t first;
	struct radius_msg_list *entry, *prev, *tmp;
	int auth_failover = 0, acct_failover = 0;
	size_t prev_num_msgs;
	int s;

	entry = radius->msgs;
	if (!entry)
		return;

	os_get_reltime(&now);

	while (entry) {
		if (now.sec >= entry->next_try) {
			s = entry->msg_type == RADIUS_AUTH ? radius->auth_sock :
				radius->acct_sock;
			if (entry->attempts >= RADIUS_CLIENT_NUM_FAILOVER ||
			    (s < 0 && entry->attempts > 0)) {
				if (entry->msg_type == RADIUS_ACCT ||
				    entry->msg_type == RADIUS_ACCT_INTERIM)
					acct_failover++;
				else
					auth_failover++;
			}
		}
		entry = entry->next;
	}

	if (auth_failover)
		radius_client_auth_failover(radius);

	if (acct_failover)
		radius_client_acct_failover(radius);

	entry = radius->msgs;
	first = 0;

	prev = NULL;
	while (entry) {
		prev_num_msgs = radius->num_msgs;
		if (now.sec >= entry->next_try &&
		    radius_client_retransmit(radius, entry, now.sec)) {
			if (prev)
				prev->next = entry->next;
			else
				radius->msgs = entry->next;

			tmp = entry;
			entry = entry->next;
			radius_client_msg_free(tmp);
			radius->num_msgs--;
			continue;
		}

		if (prev_num_msgs != radius->num_msgs) {
			wpa_printf(MSG_DEBUG,
				   "RADIUS: Message removed from queue - restart from beginning");
			entry = radius->msgs;
			prev = NULL;
			continue;
		}

		if (first == 0 || entry->next_try < first)
			first = entry->next_try;

		prev = entry;
		entry = entry->next;
	}

	if (radius->msgs) {
		if (first < now.sec)
			first = now.sec;
		eloop_cancel_timeout(radius_client_timer, radius, NULL);
		eloop_register_timeout(first - now.sec, 0,
				       radius_client_timer, radius, NULL);
		hostapd_logger(radius->ctx, NULL, HOSTAPD_MODULE_RADIUS,
			       HOSTAPD_LEVEL_DEBUG, "Next RADIUS client "
			       "retransmit in %ld seconds",
			       (long int) (first - now.sec));
	}
}


static void radius_client_auth_failover(struct radius_client_data *radius)
{
	struct hostapd_radius_servers *conf = radius->conf;
	struct hostapd_radius_server *next, *old;
	struct radius_msg_list *entry;
	char abuf[50];

	old = conf->auth_server;
	hostapd_logger(radius->ctx, NULL, HOSTAPD_MODULE_RADIUS,
		       HOSTAPD_LEVEL_NOTICE,
		       "No response from Authentication server %s:%d - failover",
		       hostapd_ip_txt(&old->addr, abuf, sizeof(abuf)),
		       old->port);

	for (entry = radius->msgs; entry; entry = entry->next) {
		if (entry->msg_type == RADIUS_AUTH)
			old->timeouts++;
	}

	next = old + 1;
	if (next > &(conf->auth_servers[conf->num_auth_servers - 1]))
		next = conf->auth_servers;
	conf->auth_server = next;
	radius_change_server(radius, next, old, 1);
}


static void radius_client_acct_failover(struct radius_client_data *radius)
{
	struct hostapd_radius_servers *conf = radius->conf;
	struct hostapd_radius_server *next, *old;
	struct radius_msg_list *entry;
	char abuf[50];

	old = conf->acct_server;
	hostapd_logger(radius->ctx, NULL, HOSTAPD_MODULE_RADIUS,
		       HOSTAPD_LEVEL_NOTICE,
		       "No response from Accounting server %s:%d - failover",
		       hostapd_ip_txt(&old->addr, abuf, sizeof(abuf)),
		       old->port);

	for (entry = radius->msgs; entry; entry = entry->next) {
		if (entry->msg_type == RADIUS_ACCT ||
		    entry->msg_type == RADIUS_ACCT_INTERIM)
			old->timeouts++;
	}

	next = old + 1;
	if (next > &conf->acct_servers[conf->num_acct_servers - 1])
		next = conf->acct_servers;
	conf->acct_server = next;
	radius_change_server(radius, next, old, 0);
}


static void radius_client_update_timeout(struct radius_client_data *radius)
{
	struct os_reltime now;
	os_time_t first;
	struct radius_msg_list *entry;

	eloop_cancel_timeout(radius_client_timer, radius, NULL);

	if (radius->msgs == NULL) {
		return;
	}

	first = 0;
	for (entry = radius->msgs; entry; entry = entry->next) {
		if (first == 0 || entry->next_try < first)
			first = entry->next_try;
	}

	os_get_reltime(&now);
	if (first < now.sec)
		first = now.sec;
	eloop_register_timeout(first - now.sec, 0, radius_client_timer, radius,
			       NULL);
	hostapd_logger(radius->ctx, NULL, HOSTAPD_MODULE_RADIUS,
		       HOSTAPD_LEVEL_DEBUG, "Next RADIUS client retransmit in"
		       " %ld seconds", (long int) (first - now.sec));
}


static void radius_client_list_add(struct radius_client_data *radius,
				   struct radius_msg *msg,
				   RadiusType msg_type,
				   const u8 *shared_secret,
				   size_t shared_secret_len, const u8 *addr)
{
	struct radius_msg_list *entry, *prev;

	if (eloop_terminated()) {
		/* No point in adding entries to retransmit queue since event
		 * loop has already been terminated. */
		radius_msg_free(msg);
		return;
	}

	entry = os_zalloc(sizeof(*entry));
	if (entry == NULL) {
		wpa_printf(MSG_INFO, "RADIUS: Failed to add packet into retransmit list");
		radius_msg_free(msg);
		return;
	}

	if (addr)
		os_memcpy(entry->addr, addr, ETH_ALEN);
	entry->msg = msg;
	entry->msg_type = msg_type;
	entry->shared_secret = shared_secret;
	entry->shared_secret_len = shared_secret_len;
	os_get_reltime(&entry->last_attempt);
	entry->first_try = entry->last_attempt.sec;
	entry->next_try = entry->first_try + RADIUS_CLIENT_FIRST_WAIT;
	entry->attempts = 1;
	entry->accu_attempts = 1;
	entry->next_wait = RADIUS_CLIENT_FIRST_WAIT * 2;
	if (entry->next_wait > RADIUS_CLIENT_MAX_WAIT)
		entry->next_wait = RADIUS_CLIENT_MAX_WAIT;
	entry->next = radius->msgs;
	radius->msgs = entry;
	radius_client_update_timeout(radius);

	if (radius->num_msgs >= RADIUS_CLIENT_MAX_ENTRIES) {
		wpa_printf(MSG_INFO, "RADIUS: Removing the oldest un-ACKed packet due to retransmit list limits");
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
		radius->num_msgs++;
}


static int radius_client_disable_pmtu_discovery(int s)
{
	int r = -1;
#if defined(IP_MTU_DISCOVER) && defined(IP_PMTUDISC_DONT)
	/* Turn off Path MTU discovery on IPv4/UDP sockets. */
	int action = IP_PMTUDISC_DONT;
	r = setsockopt(s, IPPROTO_IP, IP_MTU_DISCOVER, &action,
		       sizeof(action));
	if (r == -1)
		wpa_printf(MSG_ERROR, "RADIUS: Failed to set IP_MTU_DISCOVER: %s",
			   strerror(errno));
#endif
	return r;
}


static void radius_close_auth_socket(struct radius_client_data *radius)
{
	if (radius->auth_sock >= 0) {
#ifdef CONFIG_RADIUS_TLS
		if (radius->conf->auth_server->tls)
			eloop_unregister_sock(radius->auth_sock,
					      EVENT_TYPE_WRITE);
#endif /* CONFIG_RADIUS_TLS */
		eloop_unregister_read_sock(radius->auth_sock);
		close(radius->auth_sock);
		radius->auth_sock = -1;
	}
}


static void radius_close_acct_socket(struct radius_client_data *radius)
{
	if (radius->acct_sock >= 0) {
#ifdef CONFIG_RADIUS_TLS
		if (radius->conf->acct_server->tls)
			eloop_unregister_sock(radius->acct_sock,
					      EVENT_TYPE_WRITE);
#endif /* CONFIG_RADIUS_TLS */
		eloop_unregister_read_sock(radius->acct_sock);
		close(radius->acct_sock);
		radius->acct_sock = -1;
	}
}


/**
 * radius_client_send - Send a RADIUS request
 * @radius: RADIUS client context from radius_client_init()
 * @msg: RADIUS message to be sent
 * @msg_type: Message type (RADIUS_AUTH, RADIUS_ACCT, RADIUS_ACCT_INTERIM)
 * @addr: MAC address of the device related to this message or %NULL
 * Returns: 0 on success, -1 on failure
 *
 * This function is used to transmit a RADIUS authentication (RADIUS_AUTH) or
 * accounting request (RADIUS_ACCT or RADIUS_ACCT_INTERIM). The only difference
 * between accounting and interim accounting messages is that the interim
 * message will not be retransmitted. Instead, a callback is used to indicate
 * that the transmission failed for the specific station @addr so that a new
 * interim accounting update message can be generated with up-to-date session
 * data instead of trying to resend old information.
 *
 * The message is added on the retransmission queue and will be retransmitted
 * automatically until a response is received or maximum number of retries
 * (RADIUS_CLIENT_MAX_FAILOVER * RADIUS_CLIENT_NUM_FAILOVER) is reached. No
 * such retries are used with RADIUS_ACCT_INTERIM, i.e., such a pending message
 * is removed from the queue automatically on transmission failure.
 *
 * The related device MAC address can be used to identify pending messages that
 * can be removed with radius_client_flush_auth().
 */
int radius_client_send(struct radius_client_data *radius,
		       struct radius_msg *msg, RadiusType msg_type,
		       const u8 *addr)
{
	struct hostapd_radius_servers *conf = radius->conf;
	const u8 *shared_secret;
	size_t shared_secret_len;
	char *name;
	int s, res;
	struct wpabuf *buf;
#ifdef CONFIG_RADIUS_TLS
	struct wpabuf *out = NULL;
	struct tls_connection *conn = NULL;
	bool acct = false;
#endif /* CONFIG_RADIUS_TLS */

	if (msg_type == RADIUS_ACCT || msg_type == RADIUS_ACCT_INTERIM) {
#ifdef CONFIG_RADIUS_TLS
		acct = true;
		if (radius->acct_tls)
			conn = radius->acct_tls_conn;
#endif /* CONFIG_RADIUS_TLS */
		if (conf->acct_server && radius->acct_sock < 0)
			radius_client_init_acct(radius);

		if (conf->acct_server == NULL || radius->acct_sock < 0 ||
		    conf->acct_server->shared_secret == NULL) {
			hostapd_logger(radius->ctx, NULL,
				       HOSTAPD_MODULE_RADIUS,
				       HOSTAPD_LEVEL_INFO,
				       "No accounting server configured");
			return -1;
		}
		shared_secret = conf->acct_server->shared_secret;
		shared_secret_len = conf->acct_server->shared_secret_len;
		radius_msg_finish_acct(msg, shared_secret, shared_secret_len);
		name = "accounting";
		s = radius->acct_sock;
		conf->acct_server->requests++;
	} else {
#ifdef CONFIG_RADIUS_TLS
		if (radius->auth_tls)
			conn = radius->auth_tls_conn;
#endif /* CONFIG_RADIUS_TLS */
		if (conf->auth_server && radius->auth_sock < 0)
			radius_client_init_auth(radius);

		if (conf->auth_server == NULL || radius->auth_sock < 0 ||
		    conf->auth_server->shared_secret == NULL) {
			hostapd_logger(radius->ctx, NULL,
				       HOSTAPD_MODULE_RADIUS,
				       HOSTAPD_LEVEL_INFO,
				       "No authentication server configured");
			return -1;
		}
		shared_secret = conf->auth_server->shared_secret;
		shared_secret_len = conf->auth_server->shared_secret_len;
		radius_msg_finish(msg, shared_secret, shared_secret_len);
		name = "authentication";
		s = radius->auth_sock;
		conf->auth_server->requests++;
	}

	hostapd_logger(radius->ctx, NULL, HOSTAPD_MODULE_RADIUS,
		       HOSTAPD_LEVEL_DEBUG, "Sending RADIUS message to %s "
		       "server", name);
	if (conf->msg_dumps)
		radius_msg_dump(msg);

#ifdef CONFIG_RADIUS_TLS
	if ((acct && radius->acct_tls && !radius->acct_tls_ready) ||
	    (!acct && radius->auth_tls && !radius->auth_tls_ready)) {
		wpa_printf(MSG_DEBUG,
			   "RADIUS: TLS connection not yet ready for TX");
		goto skip_send;
	}
#endif /* CONFIG_RADIUS_TLS */

	buf = radius_msg_get_buf(msg);
#ifdef CONFIG_RADIUS_TLS
	if (conn) {
		out = tls_connection_encrypt(radius->tls_ctx, conn, buf);
		if (!out) {
			wpa_printf(MSG_INFO,
				   "RADIUS: Failed to encrypt RADIUS message (TLS)");
			return -1;
		}
		wpa_printf(MSG_DEBUG,
			   "RADIUS: TLS encryption of %zu bytes of plaintext to %zu bytes of ciphertext",
			   wpabuf_len(buf), wpabuf_len(out));
		buf = out;
	}
#endif /* CONFIG_RADIUS_TLS */
	wpa_printf(MSG_DEBUG, "RADIUS: Send %zu bytes to the server",
		   wpabuf_len(buf));
	res = send(s, wpabuf_head(buf), wpabuf_len(buf), 0);
#ifdef CONFIG_RADIUS_TLS
	wpabuf_free(out);
#endif /* CONFIG_RADIUS_TLS */
	if (res < 0)
		radius_client_handle_send_error(radius, s, msg_type);

#ifdef CONFIG_RADIUS_TLS
skip_send:
#endif /* CONFIG_RADIUS_TLS */
	radius_client_list_add(radius, msg, msg_type, shared_secret,
			       shared_secret_len, addr);

	return 0;
}


#ifdef CONFIG_RADIUS_TLS

static void radius_client_close_tcp(struct radius_client_data *radius,
				    int sock, RadiusType msg_type)
{
	wpa_printf(MSG_DEBUG, "RADIUS: Closing TCP connection (sock %d)",
		   sock);
	if (msg_type == RADIUS_ACCT) {
		radius->acct_tls_ready = false;
		radius_close_acct_socket(radius);
	} else {
		radius->auth_tls_ready = false;
		radius_close_auth_socket(radius);
	}
}


static void
radius_client_process_tls_handshake(struct radius_client_data *radius,
				    int sock, RadiusType msg_type,
				    u8 *buf, size_t len)
{
	struct wpabuf *in, *out = NULL, *appl;
	struct tls_connection *conn;
	int res;
	bool ready = false;

	wpa_printf(MSG_DEBUG,
		   "RADIUS: Process %zu bytes of received TLS handshake message",
		   len);

	if (msg_type == RADIUS_ACCT)
		conn = radius->acct_tls_conn;
	else
		conn = radius->auth_tls_conn;

	in = wpabuf_alloc_copy(buf, len);
	if (!in)
		return;

	appl = NULL;
	out = tls_connection_handshake(radius->tls_ctx, conn, in, &appl);
	wpabuf_free(in);
	if (!out) {
		wpa_printf(MSG_DEBUG,
			   "RADIUS: Could not generate TLS handshake data");
		goto fail;
	}

	if (tls_connection_get_failed(radius->tls_ctx, conn)) {
		wpa_printf(MSG_INFO, "RADIUS: TLS handshake failed");
		goto fail;
	}

	if (tls_connection_established(radius->tls_ctx, conn)) {
		wpa_printf(MSG_DEBUG,
			   "RADIUS: TLS connection established (sock=%d)",
			   sock);
		if (msg_type == RADIUS_ACCT)
			radius->acct_tls_ready = true;
		else
			radius->auth_tls_ready = true;
		ready = true;
	}

	wpa_printf(MSG_DEBUG, "RADIUS: Sending %zu bytes of TLS handshake",
		   wpabuf_len(out));
	res = send(sock, wpabuf_head(out), wpabuf_len(out), 0);
	if (res < 0) {
		wpa_printf(MSG_INFO, "RADIUS: send: %s", strerror(errno));
		goto fail;
	}
	if ((size_t) res != wpabuf_len(out)) {
		wpa_printf(MSG_INFO,
			   "RADIUS: Could not send all data for TLS handshake: only %d bytes sent",
			   res);
		goto fail;
	}
	wpabuf_free(out);

	if (ready) {
		struct radius_msg_list *entry, *prev, *tmp;
		struct os_reltime now;

		/* Send all pending message of matching type since the TLS
		 * tunnel has now been established. */

		os_get_reltime(&now);

		entry = radius->msgs;
		prev = NULL;
		while (entry) {
			if (entry->msg_type != msg_type) {
				prev = entry;
				entry = entry->next;
				continue;
			}

			if (radius_client_retransmit(radius, entry, now.sec)) {
				if (prev)
					prev->next = entry->next;
				else
					radius->msgs = entry->next;

				tmp = entry;
				entry = entry->next;
				radius_client_msg_free(tmp);
				radius->num_msgs--;
				continue;
			}

			prev = entry;
			entry = entry->next;
		}
	}

	return;

fail:
	wpabuf_free(out);
	tls_connection_deinit(radius->tls_ctx, conn);
	if (msg_type == RADIUS_ACCT)
		radius->acct_tls_conn = NULL;
	else
		radius->auth_tls_conn = NULL;
	radius_client_close_tcp(radius, sock, msg_type);
}

#endif /* CONFIG_RADIUS_TLS */


static void radius_client_receive(int sock, void *eloop_ctx, void *sock_ctx)
{
	struct radius_client_data *radius = eloop_ctx;
	struct hostapd_radius_servers *conf = radius->conf;
	RadiusType msg_type = (uintptr_t) sock_ctx;
	int len, roundtrip;
	unsigned char buf[RADIUS_MAX_MSG_LEN];
	struct msghdr msghdr = {0};
	struct iovec iov;
	struct radius_msg *msg;
	struct radius_hdr *hdr;
	struct radius_rx_handler *handlers;
	size_t num_handlers, i;
	struct radius_msg_list *req, *prev_req;
	struct os_reltime now;
	struct hostapd_radius_server *rconf;
	int invalid_authenticator = 0;
#ifdef CONFIG_RADIUS_TLS
	struct tls_connection *conn = NULL;
	bool tls, tls_ready;
#endif /* CONFIG_RADIUS_TLS */

	if (msg_type == RADIUS_ACCT) {
#ifdef CONFIG_RADIUS_TLS
		if (radius->acct_tls)
			conn = radius->acct_tls_conn;
		tls = radius->acct_tls;
		tls_ready = radius->acct_tls_ready;
#endif /* CONFIG_RADIUS_TLS */
		handlers = radius->acct_handlers;
		num_handlers = radius->num_acct_handlers;
		rconf = conf->acct_server;
	} else {
#ifdef CONFIG_RADIUS_TLS
		if (radius->auth_tls)
			conn = radius->auth_tls_conn;
		tls = radius->auth_tls;
		tls_ready = radius->auth_tls_ready;
#endif /* CONFIG_RADIUS_TLS */
		handlers = radius->auth_handlers;
		num_handlers = radius->num_auth_handlers;
		rconf = conf->auth_server;
	}

	iov.iov_base = buf;
	iov.iov_len = RADIUS_MAX_MSG_LEN;
	msghdr.msg_iov = &iov;
	msghdr.msg_iovlen = 1;
	msghdr.msg_flags = 0;
	len = recvmsg(sock, &msghdr, MSG_DONTWAIT);
	if (len < 0) {
		wpa_printf(MSG_INFO, "recvmsg[RADIUS]: %s", strerror(errno));
		return;
	}
#ifdef CONFIG_RADIUS_TLS
	if (tls && len == 0) {
		wpa_printf(MSG_DEBUG, "RADIUS: No TCP data available");
		goto close_tcp;
	}

	if (tls && !tls_ready) {
		radius_client_process_tls_handshake(radius, sock, msg_type,
						    buf, len);
		return;
	}

	if (conn) {
		struct wpabuf *out, *in;

		in = wpabuf_alloc_copy(buf, len);
		if (!in)
			return;
		wpa_printf(MSG_DEBUG,
			   "RADIUS: Process %d bytes of encrypted TLS data",
			   len);
		out = tls_connection_decrypt(radius->tls_ctx, conn, in);
		wpabuf_free(in);
		if (!out) {
			wpa_printf(MSG_INFO,
				   "RADIUS: Failed to decrypt TLS data");
			goto close_tcp;
		}
		if (wpabuf_len(out) == 0) {
			wpa_printf(MSG_DEBUG,
				   "RADIUS: Full message not yet received - continue waiting for additional TLS data");
			wpabuf_free(out);
			return;
		}
		if (wpabuf_len(out) > RADIUS_MAX_MSG_LEN) {
			wpa_printf(MSG_INFO,
				   "RADIUS: Too long RADIUS message from TLS: %zu",
				   wpabuf_len(out));
			wpabuf_free(out);
			goto close_tcp;
		}
		os_memcpy(buf, wpabuf_head(out), wpabuf_len(out));
		len = wpabuf_len(out);
		wpabuf_free(out);
	}
#endif /* CONFIG_RADIUS_TLS */

	hostapd_logger(radius->ctx, NULL, HOSTAPD_MODULE_RADIUS,
		       HOSTAPD_LEVEL_DEBUG, "Received %d bytes from RADIUS "
		       "server", len);

	if (msghdr.msg_flags & MSG_TRUNC) {
		wpa_printf(MSG_INFO, "RADIUS: Possibly too long UDP frame for our buffer - dropping it");
		return;
	}

	msg = radius_msg_parse(buf, len);
	if (msg == NULL) {
		wpa_printf(MSG_INFO, "RADIUS: Parsing incoming frame failed");
		rconf->malformed_responses++;
		return;
	}
	hdr = radius_msg_get_hdr(msg);

	hostapd_logger(radius->ctx, NULL, HOSTAPD_MODULE_RADIUS,
		       HOSTAPD_LEVEL_DEBUG, "Received RADIUS message");
	if (conf->msg_dumps)
		radius_msg_dump(msg);

	switch (hdr->code) {
	case RADIUS_CODE_ACCESS_ACCEPT:
		rconf->access_accepts++;
		break;
	case RADIUS_CODE_ACCESS_REJECT:
		rconf->access_rejects++;
		break;
	case RADIUS_CODE_ACCESS_CHALLENGE:
		rconf->access_challenges++;
		break;
	case RADIUS_CODE_ACCOUNTING_RESPONSE:
		rconf->responses++;
		break;
	}

	prev_req = NULL;
	req = radius->msgs;
	while (req) {
		/* TODO: also match by src addr:port of the packet when using
		 * alternative RADIUS servers (?) */
		if ((req->msg_type == msg_type ||
		     (req->msg_type == RADIUS_ACCT_INTERIM &&
		      msg_type == RADIUS_ACCT)) &&
		    radius_msg_get_hdr(req->msg)->identifier ==
		    hdr->identifier)
			break;

		prev_req = req;
		req = req->next;
	}

	if (req == NULL) {
		hostapd_logger(radius->ctx, NULL, HOSTAPD_MODULE_RADIUS,
			       HOSTAPD_LEVEL_DEBUG,
			       "No matching RADIUS request found (type=%d "
			       "id=%d) - dropping packet",
			       msg_type, hdr->identifier);
		goto fail;
	}

	os_get_reltime(&now);
	roundtrip = (now.sec - req->last_attempt.sec) * 100 +
		(now.usec - req->last_attempt.usec) / 10000;
	hostapd_logger(radius->ctx, req->addr, HOSTAPD_MODULE_RADIUS,
		       HOSTAPD_LEVEL_DEBUG,
		       "Received RADIUS packet matched with a pending "
		       "request, round trip time %d.%02d sec",
		       roundtrip / 100, roundtrip % 100);
	rconf->round_trip_time = roundtrip;

	/* Remove ACKed RADIUS packet from retransmit list */
	if (prev_req)
		prev_req->next = req->next;
	else
		radius->msgs = req->next;
	radius->num_msgs--;

	for (i = 0; i < num_handlers; i++) {
		RadiusRxResult res;
		res = handlers[i].handler(msg, req->msg, req->shared_secret,
					  req->shared_secret_len,
					  handlers[i].data);
		switch (res) {
		case RADIUS_RX_PROCESSED:
			radius_msg_free(msg);
			/* fall through */
		case RADIUS_RX_QUEUED:
			radius_client_msg_free(req);
			return;
		case RADIUS_RX_INVALID_AUTHENTICATOR:
			invalid_authenticator++;
			/* fall through */
		case RADIUS_RX_UNKNOWN:
			/* continue with next handler */
			break;
		}
	}

	if (invalid_authenticator)
		rconf->bad_authenticators++;
	else
		rconf->unknown_types++;
	hostapd_logger(radius->ctx, req->addr, HOSTAPD_MODULE_RADIUS,
		       HOSTAPD_LEVEL_DEBUG, "No RADIUS RX handler found "
		       "(type=%d code=%d id=%d)%s - dropping packet",
		       msg_type, hdr->code, hdr->identifier,
		       invalid_authenticator ? " [INVALID AUTHENTICATOR]" :
		       "");
	radius_client_msg_free(req);

 fail:
	radius_msg_free(msg);
	return;

#ifdef CONFIG_RADIUS_TLS
close_tcp:
	radius_client_close_tcp(radius, sock, msg_type);
#endif /* CONFIG_RADIUS_TLS */
}


#ifdef CONFIG_RADIUS_TLS
static void radius_client_write_ready(int sock, void *eloop_ctx, void *sock_ctx)
{
	struct radius_client_data *radius = eloop_ctx;
	RadiusType msg_type = (uintptr_t) sock_ctx;
	struct tls_connection *conn = NULL;
	struct wpabuf *in, *out = NULL, *appl;
	int res = -1;
	struct tls_connection_params params;
	struct hostapd_radius_server *server;

	wpa_printf(MSG_DEBUG, "RADIUS: TCP connection established - start TLS handshake (sock=%d)",
		   sock);

	if (msg_type == RADIUS_ACCT) {
		eloop_unregister_sock(sock, EVENT_TYPE_WRITE);
		eloop_register_read_sock(sock, radius_client_receive, radius,
					 (void *) RADIUS_ACCT);
		if (radius->acct_tls_conn) {
			wpa_printf(MSG_DEBUG,
				   "RADIUS: Deinit previously used TLS connection");
			tls_connection_deinit(radius->tls_ctx,
					      radius->acct_tls_conn);
			radius->acct_tls_conn = NULL;
		}
		server = radius->conf->acct_server;
	} else {
		eloop_unregister_sock(sock, EVENT_TYPE_WRITE);
		eloop_register_read_sock(sock, radius_client_receive, radius,
					 (void *) RADIUS_AUTH);
		if (radius->auth_tls_conn) {
			wpa_printf(MSG_DEBUG,
				   "RADIUS: Deinit previously used TLS connection");
			tls_connection_deinit(radius->tls_ctx,
					      radius->auth_tls_conn);
			radius->auth_tls_conn = NULL;
		}
		server = radius->conf->auth_server;
	}

	if (!server)
		goto fail;

	conn = tls_connection_init(radius->tls_ctx);
	if (!conn) {
		wpa_printf(MSG_INFO,
			   "RADIUS: Failed to initiate TLS connection");
		goto fail;
	}

	os_memset(&params, 0, sizeof(params));
	params.ca_cert = server->ca_cert;
	params.client_cert = server->client_cert;
	params.private_key = server->private_key;
	params.private_key_passwd = server->private_key_passwd;
	params.flags = TLS_CONN_DISABLE_TLSv1_0 | TLS_CONN_DISABLE_TLSv1_1;
	if (tls_connection_set_params(radius->tls_ctx, conn, &params)) {
		wpa_printf(MSG_INFO,
			   "RADIUS: Failed to set TLS connection parameters");
		goto fail;
	}

	in = NULL;
	appl = NULL;
	out = tls_connection_handshake(radius->tls_ctx, conn, in, &appl);
	if (!out) {
		wpa_printf(MSG_DEBUG,
			   "RADIUS: Could not generate TLS handshake data");
		goto fail;
	}

	if (tls_connection_get_failed(radius->tls_ctx, conn)) {
		wpa_printf(MSG_INFO, "RADIUS: TLS handshake failed");
		goto fail;
	}

	wpa_printf(MSG_DEBUG, "RADIUS: Sending %zu bytes of TLS handshake",
		   wpabuf_len(out));
	res = send(sock, wpabuf_head(out), wpabuf_len(out), 0);
	if (res < 0) {
		wpa_printf(MSG_INFO, "RADIUS: send: %s", strerror(errno));
		goto fail;
	}
	if ((size_t) res != wpabuf_len(out)) {
		wpa_printf(MSG_INFO,
			   "RADIUS: Could not send all data for TLS handshake: only %d bytes sent",
			   res);
		goto fail;
	}
	wpabuf_free(out);

	if (msg_type == RADIUS_ACCT)
		radius->acct_tls_conn = conn;
	else
		radius->auth_tls_conn = conn;
	return;

fail:
	wpa_printf(MSG_INFO, "RADIUS: Failed to perform TLS handshake");
	tls_connection_deinit(radius->tls_ctx, conn);
	wpabuf_free(out);
	radius_client_close_tcp(radius, sock, msg_type);
}
#endif /* CONFIG_RADIUS_TLS */


/**
 * radius_client_get_id - Get an identifier for a new RADIUS message
 * @radius: RADIUS client context from radius_client_init()
 * Returns: Allocated identifier
 *
 * This function is used to fetch a unique (among pending requests) identifier
 * for a new RADIUS message.
 */
u8 radius_client_get_id(struct radius_client_data *radius)
{
	struct radius_msg_list *entry, *prev, *_remove;
	u8 id = radius->next_radius_identifier++;

	/* remove entries with matching id from retransmit list to avoid
	 * using new reply from the RADIUS server with an old request */
	entry = radius->msgs;
	prev = NULL;
	while (entry) {
		if (radius_msg_get_hdr(entry->msg)->identifier == id) {
			hostapd_logger(radius->ctx, entry->addr,
				       HOSTAPD_MODULE_RADIUS,
				       HOSTAPD_LEVEL_DEBUG,
				       "Removing pending RADIUS message, "
				       "since its id (%d) is reused", id);
			if (prev)
				prev->next = entry->next;
			else
				radius->msgs = entry->next;
			_remove = entry;
		} else {
			_remove = NULL;
			prev = entry;
		}
		entry = entry->next;

		if (_remove)
			radius_client_msg_free(_remove);
	}

	return id;
}


/**
 * radius_client_flush - Flush all pending RADIUS client messages
 * @radius: RADIUS client context from radius_client_init()
 * @only_auth: Whether only authentication messages are removed
 */
void radius_client_flush(struct radius_client_data *radius, int only_auth)
{
	struct radius_msg_list *entry, *prev, *tmp;

	if (!radius)
		return;

	prev = NULL;
	entry = radius->msgs;

	while (entry) {
		if (!only_auth || entry->msg_type == RADIUS_AUTH) {
			if (prev)
				prev->next = entry->next;
			else
				radius->msgs = entry->next;

			tmp = entry;
			entry = entry->next;
			radius_client_msg_free(tmp);
			radius->num_msgs--;
		} else {
			prev = entry;
			entry = entry->next;
		}
	}

	if (radius->msgs == NULL)
		eloop_cancel_timeout(radius_client_timer, radius, NULL);
}


static void radius_client_update_acct_msgs(struct radius_client_data *radius,
					   const u8 *shared_secret,
					   size_t shared_secret_len)
{
	struct radius_msg_list *entry;

	if (!radius)
		return;

	for (entry = radius->msgs; entry; entry = entry->next) {
		if (entry->msg_type == RADIUS_ACCT) {
			entry->shared_secret = shared_secret;
			entry->shared_secret_len = shared_secret_len;
			radius_msg_finish_acct(entry->msg, shared_secret,
					       shared_secret_len);
		}
	}
}


static int
radius_change_server(struct radius_client_data *radius,
		     struct hostapd_radius_server *nserv,
		     struct hostapd_radius_server *oserv,
		     int auth)
{
	struct sockaddr_in serv, claddr;
#ifdef CONFIG_IPV6
	struct sockaddr_in6 serv6, claddr6;
#endif /* CONFIG_IPV6 */
	struct sockaddr *addr, *cl_addr;
	socklen_t addrlen, claddrlen;
	char abuf[50];
	int sel_sock;
	struct radius_msg_list *entry;
	struct hostapd_radius_servers *conf = radius->conf;
	int type = SOCK_DGRAM;
	bool tls = nserv->tls;

	if (tls) {
#ifdef CONFIG_RADIUS_TLS
		type = SOCK_STREAM;
#else /* CONFIG_RADIUS_TLS */
		wpa_printf(MSG_ERROR, "RADIUS: TLS not supported");
		return -1;
#endif /* CONFIG_RADIUS_TLS */
	}

	hostapd_logger(radius->ctx, NULL, HOSTAPD_MODULE_RADIUS,
		       HOSTAPD_LEVEL_INFO,
		       "%s server %s:%d",
		       auth ? "Authentication" : "Accounting",
		       hostapd_ip_txt(&nserv->addr, abuf, sizeof(abuf)),
		       nserv->port);

	if (oserv && oserv == nserv) {
		/* Reconnect to same server, flush */
		if (auth)
			radius_client_flush(radius, 1);
	}

	if (oserv && oserv != nserv &&
	    (nserv->shared_secret_len != oserv->shared_secret_len ||
	     os_memcmp(nserv->shared_secret, oserv->shared_secret,
		       nserv->shared_secret_len) != 0)) {
		/* Pending RADIUS packets used different shared secret, so
		 * they need to be modified. Update accounting message
		 * authenticators here. Authentication messages are removed
		 * since they would require more changes and the new RADIUS
		 * server may not be prepared to receive them anyway due to
		 * missing state information. Client will likely retry
		 * authentication, so this should not be an issue. */
		if (auth)
			radius_client_flush(radius, 1);
		else {
			radius_client_update_acct_msgs(
				radius, nserv->shared_secret,
				nserv->shared_secret_len);
		}
	}

	/* Reset retry counters */
	for (entry = radius->msgs; oserv && entry; entry = entry->next) {
		if ((auth && entry->msg_type != RADIUS_AUTH) ||
		    (!auth && entry->msg_type != RADIUS_ACCT))
			continue;
		entry->next_try = entry->first_try + RADIUS_CLIENT_FIRST_WAIT;
		entry->attempts = 0;
		entry->next_wait = RADIUS_CLIENT_FIRST_WAIT * 2;
	}

	if (radius->msgs) {
		eloop_cancel_timeout(radius_client_timer, radius, NULL);
		eloop_register_timeout(RADIUS_CLIENT_FIRST_WAIT, 0,
				       radius_client_timer, radius, NULL);
	}

	switch (nserv->addr.af) {
	case AF_INET:
		os_memset(&serv, 0, sizeof(serv));
		serv.sin_family = AF_INET;
		serv.sin_addr.s_addr = nserv->addr.u.v4.s_addr;
		serv.sin_port = htons(nserv->port);
		addr = (struct sockaddr *) &serv;
		addrlen = sizeof(serv);
		sel_sock = socket(PF_INET, type, 0);
		if (sel_sock >= 0)
			radius_client_disable_pmtu_discovery(sel_sock);
		break;
#ifdef CONFIG_IPV6
	case AF_INET6:
		os_memset(&serv6, 0, sizeof(serv6));
		serv6.sin6_family = AF_INET6;
		os_memcpy(&serv6.sin6_addr, &nserv->addr.u.v6,
			  sizeof(struct in6_addr));
		serv6.sin6_port = htons(nserv->port);
		addr = (struct sockaddr *) &serv6;
		addrlen = sizeof(serv6);
		sel_sock = socket(PF_INET6, type, 0);
		break;
#endif /* CONFIG_IPV6 */
	default:
		return -1;
	}

	if (sel_sock < 0) {
		wpa_printf(MSG_INFO,
			   "RADIUS: Failed to open server socket (af=%d auth=%d)",
			   nserv->addr.af, auth);
		return -1;
	}

#ifdef CONFIG_RADIUS_TLS
	if (tls && fcntl(sel_sock, F_SETFL, O_NONBLOCK) != 0) {
		wpa_printf(MSG_DEBUG, "RADIUS: fnctl(O_NONBLOCK) failed: %s",
			   strerror(errno));
		close(sel_sock);
		return -1;
	}
#endif /* CONFIG_RADIUS_TLS */

#ifdef __linux__
	if (conf->force_client_dev && conf->force_client_dev[0]) {
		if (setsockopt(sel_sock, SOL_SOCKET, SO_BINDTODEVICE,
			       conf->force_client_dev,
			       os_strlen(conf->force_client_dev)) < 0) {
			wpa_printf(MSG_ERROR,
				   "RADIUS: setsockopt[SO_BINDTODEVICE]: %s",
				   strerror(errno));
			/* Probably not a critical error; continue on and hope
			 * for the best. */
		} else {
			wpa_printf(MSG_DEBUG,
				   "RADIUS: Bound client socket to device: %s",
				   conf->force_client_dev);
		}
	}
#endif /* __linux__ */

	if (conf->force_client_addr) {
		switch (conf->client_addr.af) {
		case AF_INET:
			os_memset(&claddr, 0, sizeof(claddr));
			claddr.sin_family = AF_INET;
			claddr.sin_addr.s_addr = conf->client_addr.u.v4.s_addr;
			claddr.sin_port = htons(0);
			cl_addr = (struct sockaddr *) &claddr;
			claddrlen = sizeof(claddr);
			break;
#ifdef CONFIG_IPV6
		case AF_INET6:
			os_memset(&claddr6, 0, sizeof(claddr6));
			claddr6.sin6_family = AF_INET6;
			os_memcpy(&claddr6.sin6_addr, &conf->client_addr.u.v6,
				  sizeof(struct in6_addr));
			claddr6.sin6_port = htons(0);
			cl_addr = (struct sockaddr *) &claddr6;
			claddrlen = sizeof(claddr6);
			break;
#endif /* CONFIG_IPV6 */
		default:
			close(sel_sock);
			return -1;
		}

		if (bind(sel_sock, cl_addr, claddrlen) < 0) {
			wpa_printf(MSG_INFO, "bind[radius]: %s",
				   strerror(errno));
			close(sel_sock);
			return -2;
		}
	}

	if (connect(sel_sock, addr, addrlen) < 0) {
		if (nserv->tls && errno == EINPROGRESS) {
			wpa_printf(MSG_DEBUG,
				   "RADIUS: TCP connection establishment in progress (sock %d)",
				   sel_sock);
		} else {
			wpa_printf(MSG_INFO, "connect[radius]: %s",
				   strerror(errno));
			close(sel_sock);
			return -2;
		}
	}

#ifndef CONFIG_NATIVE_WINDOWS
	switch (nserv->addr.af) {
	case AF_INET:
		claddrlen = sizeof(claddr);
		if (getsockname(sel_sock, (struct sockaddr *) &claddr,
				&claddrlen) == 0) {
			wpa_printf(MSG_DEBUG, "RADIUS local address: %s:%u",
				   inet_ntoa(claddr.sin_addr),
				   ntohs(claddr.sin_port));
		}
		break;
#ifdef CONFIG_IPV6
	case AF_INET6: {
		claddrlen = sizeof(claddr6);
		if (getsockname(sel_sock, (struct sockaddr *) &claddr6,
				&claddrlen) == 0) {
			wpa_printf(MSG_DEBUG, "RADIUS local address: %s:%u",
				   inet_ntop(AF_INET6, &claddr6.sin6_addr,
					     abuf, sizeof(abuf)),
				   ntohs(claddr6.sin6_port));
		}
		break;
	}
#endif /* CONFIG_IPV6 */
	}
#endif /* CONFIG_NATIVE_WINDOWS */

	if (auth) {
		radius_close_auth_socket(radius);
		radius->auth_sock = sel_sock;
	} else {
		radius_close_acct_socket(radius);
		radius->acct_sock = sel_sock;
	}

	if (!tls)
		eloop_register_read_sock(sel_sock, radius_client_receive,
					 radius,
					 auth ? (void *) RADIUS_AUTH :
					 (void *) RADIUS_ACCT);
#ifdef CONFIG_RADIUS_TLS
	if (tls)
		eloop_register_sock(sel_sock, EVENT_TYPE_WRITE,
				    radius_client_write_ready, radius,
				    auth ? (void *) RADIUS_AUTH :
				    (void *) RADIUS_ACCT);
#endif /* CONFIG_RADIUS_TLS */

	if (auth) {
		radius->auth_tls = nserv->tls;
		radius->auth_tls_ready = false;
	} else {
		radius->acct_tls = nserv->tls;
		radius->acct_tls_ready = false;
	}

	return 0;
}


static void radius_retry_primary_timer(void *eloop_ctx, void *timeout_ctx)
{
	struct radius_client_data *radius = eloop_ctx;
	struct hostapd_radius_servers *conf = radius->conf;
	struct hostapd_radius_server *oserv;

	if (radius->auth_sock >= 0 && conf->auth_servers &&
	    conf->auth_server != conf->auth_servers) {
		oserv = conf->auth_server;
		conf->auth_server = conf->auth_servers;
		if (radius_change_server(radius, conf->auth_server, oserv,
					 1) < 0) {
			conf->auth_server = oserv;
			radius_change_server(radius, oserv, conf->auth_server,
					     1);
		}
	}

	if (radius->acct_sock >= 0 && conf->acct_servers &&
	    conf->acct_server != conf->acct_servers) {
		oserv = conf->acct_server;
		conf->acct_server = conf->acct_servers;
		if (radius_change_server(radius, conf->acct_server, oserv,
					 0) < 0) {
			conf->acct_server = oserv;
			radius_change_server(radius, oserv, conf->acct_server,
					     0);
		}
	}

	if (conf->retry_primary_interval)
		eloop_register_timeout(conf->retry_primary_interval, 0,
				       radius_retry_primary_timer, radius,
				       NULL);
}


static int radius_client_init_auth(struct radius_client_data *radius)
{
	radius_close_auth_socket(radius);
	return radius_change_server(radius, radius->conf->auth_server, NULL, 1);
}


static int radius_client_init_acct(struct radius_client_data *radius)
{
	radius_close_acct_socket(radius);
	return radius_change_server(radius, radius->conf->acct_server, NULL, 0);
}


#ifdef CONFIG_RADIUS_TLS
static void radius_tls_event_cb(void *ctx, enum tls_event ev,
				union tls_event_data *data)
{
	wpa_printf(MSG_DEBUG, "RADIUS: TLS event %d", ev);
}
#endif /* CONFIG_RADIUS_TLS */


/**
 * radius_client_init - Initialize RADIUS client
 * @ctx: Callback context to be used in hostapd_logger() calls
 * @conf: RADIUS client configuration (RADIUS servers)
 * Returns: Pointer to private RADIUS client context or %NULL on failure
 *
 * The caller is responsible for keeping the configuration data available for
 * the lifetime of the RADIUS client, i.e., until radius_client_deinit() is
 * called for the returned context pointer.
 */
struct radius_client_data *
radius_client_init(void *ctx, struct hostapd_radius_servers *conf)
{
	struct radius_client_data *radius;

	radius = os_zalloc(sizeof(struct radius_client_data));
	if (radius == NULL)
		return NULL;

	radius->ctx = ctx;
	radius->conf = conf;
	radius->auth_sock = radius->acct_sock = -1;

	if (conf->auth_server && radius_client_init_auth(radius) == -1) {
		radius_client_deinit(radius);
		return NULL;
	}

	if (conf->acct_server && radius_client_init_acct(radius) == -1) {
		radius_client_deinit(radius);
		return NULL;
	}

	if (conf->retry_primary_interval)
		eloop_register_timeout(conf->retry_primary_interval, 0,
				       radius_retry_primary_timer, radius,
				       NULL);

#ifdef CONFIG_RADIUS_TLS
	if ((conf->auth_server && conf->auth_server->tls) ||
	    (conf->acct_server && conf->acct_server->tls)) {
		struct tls_config tls_conf;

		os_memset(&tls_conf, 0, sizeof(tls_conf));
		tls_conf.event_cb = radius_tls_event_cb;
		radius->tls_ctx = tls_init(&tls_conf);
		if (!radius->tls_ctx) {
			radius_client_deinit(radius);
			return NULL;
		}
	}
#endif /* CONFIG_RADIUS_TLS */


	return radius;
}


/**
 * radius_client_deinit - Deinitialize RADIUS client
 * @radius: RADIUS client context from radius_client_init()
 */
void radius_client_deinit(struct radius_client_data *radius)
{
	if (!radius)
		return;

	radius_close_auth_socket(radius);
	radius_close_acct_socket(radius);

	eloop_cancel_timeout(radius_retry_primary_timer, radius, NULL);

	radius_client_flush(radius, 0);
	os_free(radius->auth_handlers);
	os_free(radius->acct_handlers);
#ifdef CONFIG_RADIUS_TLS
	if (radius->tls_ctx) {
		tls_connection_deinit(radius->tls_ctx, radius->auth_tls_conn);
		tls_connection_deinit(radius->tls_ctx, radius->acct_tls_conn);
		tls_deinit(radius->tls_ctx);
	}
#endif /* CONFIG_RADIUS_TLS */
	os_free(radius);
}


/**
 * radius_client_flush_auth - Flush pending RADIUS messages for an address
 * @radius: RADIUS client context from radius_client_init()
 * @addr: MAC address of the related device
 *
 * This function can be used to remove pending RADIUS authentication messages
 * that are related to a specific device. The addr parameter is matched with
 * the one used in radius_client_send() call that was used to transmit the
 * authentication request.
 */
void radius_client_flush_auth(struct radius_client_data *radius,
			      const u8 *addr)
{
	struct radius_msg_list *entry, *prev, *tmp;

	prev = NULL;
	entry = radius->msgs;
	while (entry) {
		if (entry->msg_type == RADIUS_AUTH &&
		    ether_addr_equal(entry->addr, addr)) {
			hostapd_logger(radius->ctx, addr,
				       HOSTAPD_MODULE_RADIUS,
				       HOSTAPD_LEVEL_DEBUG,
				       "Removing pending RADIUS authentication"
				       " message for removed client");

			if (prev)
				prev->next = entry->next;
			else
				radius->msgs = entry->next;

			tmp = entry;
			entry = entry->next;
			radius_client_msg_free(tmp);
			radius->num_msgs--;
			continue;
		}

		prev = entry;
		entry = entry->next;
	}
}


static int radius_client_dump_auth_server(char *buf, size_t buflen,
					  struct hostapd_radius_server *serv,
					  struct radius_client_data *cli)
{
	int pending = 0;
	struct radius_msg_list *msg;
	char abuf[50];

	if (cli) {
		for (msg = cli->msgs; msg; msg = msg->next) {
			if (msg->msg_type == RADIUS_AUTH)
				pending++;
		}
	}

	return os_snprintf(buf, buflen,
			   "radiusAuthServerIndex=%d\n"
			   "radiusAuthServerAddress=%s\n"
			   "radiusAuthClientServerPortNumber=%d\n"
			   "radiusAuthClientRoundTripTime=%d\n"
			   "radiusAuthClientAccessRequests=%u\n"
			   "radiusAuthClientAccessRetransmissions=%u\n"
			   "radiusAuthClientAccessAccepts=%u\n"
			   "radiusAuthClientAccessRejects=%u\n"
			   "radiusAuthClientAccessChallenges=%u\n"
			   "radiusAuthClientMalformedAccessResponses=%u\n"
			   "radiusAuthClientBadAuthenticators=%u\n"
			   "radiusAuthClientPendingRequests=%u\n"
			   "radiusAuthClientTimeouts=%u\n"
			   "radiusAuthClientUnknownTypes=%u\n"
			   "radiusAuthClientPacketsDropped=%u\n",
			   serv->index,
			   hostapd_ip_txt(&serv->addr, abuf, sizeof(abuf)),
			   serv->port,
			   serv->round_trip_time,
			   serv->requests,
			   serv->retransmissions,
			   serv->access_accepts,
			   serv->access_rejects,
			   serv->access_challenges,
			   serv->malformed_responses,
			   serv->bad_authenticators,
			   pending,
			   serv->timeouts,
			   serv->unknown_types,
			   serv->packets_dropped);
}


static int radius_client_dump_acct_server(char *buf, size_t buflen,
					  struct hostapd_radius_server *serv,
					  struct radius_client_data *cli)
{
	int pending = 0;
	struct radius_msg_list *msg;
	char abuf[50];

	if (cli) {
		for (msg = cli->msgs; msg; msg = msg->next) {
			if (msg->msg_type == RADIUS_ACCT ||
			    msg->msg_type == RADIUS_ACCT_INTERIM)
				pending++;
		}
	}

	return os_snprintf(buf, buflen,
			   "radiusAccServerIndex=%d\n"
			   "radiusAccServerAddress=%s\n"
			   "radiusAccClientServerPortNumber=%d\n"
			   "radiusAccClientRoundTripTime=%d\n"
			   "radiusAccClientRequests=%u\n"
			   "radiusAccClientRetransmissions=%u\n"
			   "radiusAccClientResponses=%u\n"
			   "radiusAccClientMalformedResponses=%u\n"
			   "radiusAccClientBadAuthenticators=%u\n"
			   "radiusAccClientPendingRequests=%u\n"
			   "radiusAccClientTimeouts=%u\n"
			   "radiusAccClientUnknownTypes=%u\n"
			   "radiusAccClientPacketsDropped=%u\n",
			   serv->index,
			   hostapd_ip_txt(&serv->addr, abuf, sizeof(abuf)),
			   serv->port,
			   serv->round_trip_time,
			   serv->requests,
			   serv->retransmissions,
			   serv->responses,
			   serv->malformed_responses,
			   serv->bad_authenticators,
			   pending,
			   serv->timeouts,
			   serv->unknown_types,
			   serv->packets_dropped);
}


/**
 * radius_client_get_mib - Get RADIUS client MIB information
 * @radius: RADIUS client context from radius_client_init()
 * @buf: Buffer for returning MIB data in text format
 * @buflen: Maximum buf length in octets
 * Returns: Number of octets written into the buffer
 */
int radius_client_get_mib(struct radius_client_data *radius, char *buf,
			  size_t buflen)
{
	struct hostapd_radius_servers *conf;
	int i;
	struct hostapd_radius_server *serv;
	int count = 0;

	if (!radius)
		return 0;

	conf = radius->conf;

	if (conf->auth_servers) {
		for (i = 0; i < conf->num_auth_servers; i++) {
			serv = &conf->auth_servers[i];
			count += radius_client_dump_auth_server(
				buf + count, buflen - count, serv,
				serv == conf->auth_server ?
				radius : NULL);
		}
	}

	if (conf->acct_servers) {
		for (i = 0; i < conf->num_acct_servers; i++) {
			serv = &conf->acct_servers[i];
			count += radius_client_dump_acct_server(
				buf + count, buflen - count, serv,
				serv == conf->acct_server ?
				radius : NULL);
		}
	}

	return count;
}


void radius_client_reconfig(struct radius_client_data *radius,
			    struct hostapd_radius_servers *conf)
{
	if (radius)
		radius->conf = conf;
}
