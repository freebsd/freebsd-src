/*
 * UPnP WPS Device - Event processing
 * Copyright (c) 2000-2003 Intel Corporation
 * Copyright (c) 2006-2007 Sony Corporation
 * Copyright (c) 2008-2009 Atheros Communications
 * Copyright (c) 2009, Jouni Malinen <j@w1.fi>
 *
 * See wps_upnp.c for more details on licensing and code history.
 */

#include "includes.h"
#include <assert.h>
#include <fcntl.h>

#include "common.h"
#include "eloop.h"
#include "uuid.h"
#include "httpread.h"
#include "wps_defs.h"
#include "wps_upnp.h"
#include "wps_upnp_i.h"

/*
 * Event message generation (to subscribers)
 *
 * We make a separate copy for each message for each subscriber. This memory
 * wasted could be limited (adding code complexity) by sharing copies, keeping
 * a usage count and freeing when zero.
 *
 * Sending a message requires using a HTTP over TCP NOTIFY
 * (like a PUT) which requires a number of states..
 */

#define MAX_EVENTS_QUEUED 20   /* How far behind queued events */
#define EVENT_TIMEOUT_SEC 30   /* Drop sending event after timeout */

/* How long to wait before sending event */
#define EVENT_DELAY_SECONDS 0
#define EVENT_DELAY_MSEC 0

/*
 * Event information that we send to each subscriber is remembered in this
 * struct. The event cannot be sent by simple UDP; it has to be sent by a HTTP
 * over TCP transaction which requires various states.. It may also need to be
 * retried at a different address (if more than one is available).
 *
 * TODO: As an optimization we could share data between subscribers.
 */
struct wps_event_ {
	struct wps_event_ *next;
	struct wps_event_ *prev;        /* double linked list */
	struct subscription *s;         /* parent */
	unsigned subscriber_sequence;   /* which event for this subscription*/
	int retry;                      /* which retry */
	struct subscr_addr *addr;       /* address to connect to */
	struct wpabuf *data;            /* event data to send */
	/* The following apply while we are sending an event message. */
	int sd;            /* -1 or socket descriptor for open connection */
	int sd_registered;        /* nonzero if we must cancel registration */
	struct httpread *hread; /* NULL or open connection for event msg */
};


static void event_timeout_handler(void *eloop_data, void *user_ctx);

/* event_clean -- clean sockets etc. of event
 * Leaves data, retry count etc. alone.
 */
static void event_clean(struct wps_event_ *e)
{
	if (e->s->current_event == e) {
		eloop_cancel_timeout(event_timeout_handler, NULL, e);
		e->s->current_event = NULL;
	}
	if (e->sd_registered) {
		eloop_unregister_sock(e->sd, EVENT_TYPE_WRITE);
		e->sd_registered = 0;
	}
	if (e->sd != -1) {
		close(e->sd);
		e->sd = -1;
	}
	if (e->hread)
		httpread_destroy(e->hread);
	e->hread = NULL;
}


/* event_delete -- delete single unqueued event
 * (be sure to dequeue first if need be)
 */
void event_delete(struct wps_event_ *e)
{
	event_clean(e);
	wpabuf_free(e->data);
	os_free(e);
}


/* event_dequeue -- get next event from the queue
 * Returns NULL if empty.
 */
static struct wps_event_ *event_dequeue(struct subscription *s)
{
	struct wps_event_ **event_head = &s->event_queue;
	struct wps_event_ *e = *event_head;
	if (e == NULL)
		return NULL;
	e->next->prev = e->prev;
	e->prev->next = e->next;
	if (*event_head == e) {
		if (e == e->next) {
			/* last in queue */
			*event_head = NULL;
		} else {
			*event_head = e->next;
		}
	}
	s->n_queue--;
	e->next = e->prev = NULL;
	/* but parent "s" is still valid */
	return e;
}


/* event_enqueue_at_end -- add event to end of queue */
static void event_enqueue_at_end(struct subscription *s, struct wps_event_ *e)
{
	struct wps_event_ **event_head = &s->event_queue;
	if (*event_head == NULL) {
		*event_head = e->next = e->prev = e;
	} else {
		e->next = *event_head;
		e->prev = e->next->prev;
		e->prev->next = e;
		e->next->prev = e;
	}
	s->n_queue++;
}


/* event_enqueue_at_begin -- add event to begin of queue
 * (appropriate for retrying event only)
 */
static void event_enqueue_at_begin(struct subscription *s,
				   struct wps_event_ *e)
{
	struct wps_event_ **event_head = &s->event_queue;
	if (*event_head == NULL) {
		*event_head = e->next = e->prev = e;
	} else {
		e->prev = *event_head;
		e->next = e->prev->next;
		e->prev->next = e;
		e->next->prev = e;
		*event_head = e;
	}
	s->n_queue++;
}


/* event_delete_all -- delete entire event queue and current event */
void event_delete_all(struct subscription *s)
{
	struct wps_event_ *e;
	while ((e = event_dequeue(s)) != NULL)
		event_delete(e);
	if (s->current_event) {
		event_delete(s->current_event);
		/* will set: s->current_event = NULL;  */
	}
}


/**
 * event_retry - Called when we had a failure delivering event msg
 * @e: Event
 * @do_next_address: skip address e.g. on connect fail
 */
static void event_retry(struct wps_event_ *e, int do_next_address)
{
	struct subscription *s = e->s;
	struct upnp_wps_device_sm *sm = s->sm;

	event_clean(e);
	/* will set: s->current_event = NULL; */

	if (do_next_address)
		e->retry++;
	if (e->retry >= s->n_addr) {
		wpa_printf(MSG_DEBUG, "WPS UPnP: Giving up on sending event "
			   "for %s", e->addr->domain_and_port);
		return;
	}
	event_enqueue_at_begin(s, e);
	event_send_all_later(sm);
}


/* called if the overall event-sending process takes too long */
static void event_timeout_handler(void *eloop_data, void *user_ctx)
{
	struct wps_event_ *e = user_ctx;
	struct subscription *s = e->s;

	assert(e == s->current_event);

	wpa_printf(MSG_DEBUG, "WPS UPnP: Event send timeout");
	event_retry(e, 1);
}


/* event_got_response_handler -- called back when http response is received. */
static void event_got_response_handler(struct httpread *handle, void *cookie,
				       enum httpread_event en)
{
	struct wps_event_ *e = cookie;
	struct subscription *s = e->s;
	struct upnp_wps_device_sm *sm = s->sm;
	struct httpread *hread = e->hread;
	int reply_code = 0;

	assert(e == s->current_event);
	eloop_cancel_timeout(event_timeout_handler, NULL, e);

	if (en == HTTPREAD_EVENT_FILE_READY) {
		if (httpread_hdr_type_get(hread) == HTTPREAD_HDR_TYPE_REPLY) {
			reply_code = httpread_reply_code_get(hread);
			if (reply_code == HTTP_OK) {
				wpa_printf(MSG_DEBUG,
					   "WPS UPnP: Got event reply OK from "
					   "%s", e->addr->domain_and_port);
				event_delete(e);
				goto send_more;
			} else {
				wpa_printf(MSG_DEBUG, "WPS UPnP: Got event "
					   "error reply code %d from %s",
					   reply_code,
					   e->addr->domain_and_port);
				goto bad;
			}
		} else {
			wpa_printf(MSG_DEBUG, "WPS UPnP: Got bogus event "
				   "response %d from %s", en,
				   e->addr->domain_and_port);
		}
	} else {
		wpa_printf(MSG_DEBUG, "WPS UPnP: Event response timeout/fail "
			   "for %s", e->addr->domain_and_port);
		goto bad;
	}
	event_retry(e, 1);
	goto send_more;

send_more:
	/* Schedule sending more if there is more to send */
	if (s->event_queue)
		event_send_all_later(sm);
	return;

bad:
	/*
	 * If other side doesn't like what we say, forget about them.
	 * (There is no way to tell other side that we are dropping
	 * them...).
	 * Alternately, we could just do event_delete(e)
	 */
	wpa_printf(MSG_DEBUG, "WPS UPnP: Deleting subscription due to errors");
	subscription_unlink(s);
	subscription_destroy(s);
}


/* event_send_tx_ready -- actually write event message
 *
 * Prequisite: subscription socket descriptor has become ready to
 * write (because connection to subscriber has been made).
 *
 * It is also possible that we are called because the connect has failed;
 * it is possible to test for this, or we can just go ahead and then
 * the write will fail.
 */
static void event_send_tx_ready(int sock, void *eloop_ctx, void *sock_ctx)
{
	struct wps_event_ *e = sock_ctx;
	struct subscription *s = e->s;
	struct wpabuf *buf;
	char *b;

	assert(e == s->current_event);
	assert(e->sd == sock);

	buf = wpabuf_alloc(1000 + wpabuf_len(e->data));
	if (buf == NULL) {
		event_retry(e, 0);
		goto bad;
	}
	wpabuf_printf(buf, "NOTIFY %s HTTP/1.1\r\n", e->addr->path);
	wpabuf_put_str(buf, "SERVER: Unspecified, UPnP/1.0, Unspecified\r\n");
	wpabuf_printf(buf, "HOST: %s\r\n", e->addr->domain_and_port);
	wpabuf_put_str(buf, "CONTENT-TYPE: text/xml; charset=\"utf-8\"\r\n"
		       "NT: upnp:event\r\n"
		       "NTS: upnp:propchange\r\n");
	wpabuf_put_str(buf, "SID: uuid:");
	b = wpabuf_put(buf, 0);
	uuid_bin2str(s->uuid, b, 80);
	wpabuf_put(buf, os_strlen(b));
	wpabuf_put_str(buf, "\r\n");
	wpabuf_printf(buf, "SEQ: %u\r\n", e->subscriber_sequence);
	wpabuf_printf(buf, "CONTENT-LENGTH: %d\r\n",
		      (int) wpabuf_len(e->data));
	wpabuf_put_str(buf, "\r\n"); /* terminating empty line */
	wpabuf_put_buf(buf, e->data);

	/* Since the message size is pretty small, we should be
	 * able to get the operating system to buffer what we give it
	 * and not have to come back again later to write more...
	 */
#if 0
	/* we could: Turn blocking back on? */
	fcntl(e->sd, F_SETFL, 0);
#endif
	wpa_printf(MSG_DEBUG, "WPS UPnP: Sending event to %s",
		   e->addr->domain_and_port);
	if (send_wpabuf(e->sd, buf) < 0) {
		event_retry(e, 1);
		goto bad;
	}
	wpabuf_free(buf);
	buf = NULL;

	if (e->sd_registered) {
		e->sd_registered = 0;
		eloop_unregister_sock(e->sd, EVENT_TYPE_WRITE);
	}
	/* Set up to read the reply */
	e->hread = httpread_create(e->sd, event_got_response_handler,
				   e /* cookie */,
				   0 /* no data expected */,
				   EVENT_TIMEOUT_SEC);
	if (e->hread == NULL) {
		wpa_printf(MSG_ERROR, "WPS UPnP: httpread_create failed");
		event_retry(e, 0);
		goto bad;
	}
	return;

bad:
	/* Schedule sending more if there is more to send */
	if (s->event_queue)
		event_send_all_later(s->sm);
	wpabuf_free(buf);
}


/* event_send_start -- prepare to send a event message to subscriber
 *
 * This gets complicated because:
 * -- The message is sent via TCP and we have to keep the stream open
 *      for 30 seconds to get a response... then close it.
 * -- But we might have other event happen in the meantime...
 *      we have to queue them, if we lose them then the subscriber will
 *      be forced to unsubscribe and subscribe again.
 * -- If multiple URLs are provided then we are supposed to try successive
 *      ones after 30 second timeout.
 * -- The URLs might use domain names instead of dotted decimal addresses,
 *      and resolution of those may cause unwanted sleeping.
 * -- Doing the initial TCP connect can take a while, so we have to come
 *      back after connection and then send the data.
 *
 * Returns nonzero on error;
 *
 * Prerequisite: No current event send (s->current_event == NULL)
 *      and non-empty queue.
 */
static int event_send_start(struct subscription *s)
{
	struct wps_event_ *e;
	int itry;

	/*
	 * Assume we are called ONLY with no current event and ONLY with
	 * nonempty event queue and ONLY with at least one address to send to.
	 */
	assert(s->addr_list != NULL);
	assert(s->current_event == NULL);
	assert(s->event_queue != NULL);

	s->current_event = e = event_dequeue(s);

	/* Use address acc. to no. of retries */
	e->addr = s->addr_list;
	for (itry = 0; itry < e->retry; itry++)
		e->addr = e->addr->next;

	e->sd = socket(AF_INET, SOCK_STREAM, 0);
	if (e->sd < 0) {
		event_retry(e, 0);
		return -1;
	}
	/* set non-blocking so we don't sleep waiting for connection */
	if (fcntl(e->sd, F_SETFL, O_NONBLOCK) != 0) {
		event_retry(e, 0);
		return -1;
	}
	/*
	 * Start the connect. It might succeed immediately but more likely will
	 * return errno EINPROGRESS.
	 */
	if (connect(e->sd, (struct sockaddr *) &e->addr->saddr,
		    sizeof(e->addr->saddr))) {
		if (errno != EINPROGRESS) {
			event_retry(e, 1);
			return -1;
		}
	}
	/* Call back when ready for writing (or on failure...). */
	if (eloop_register_sock(e->sd, EVENT_TYPE_WRITE, event_send_tx_ready,
				NULL, e)) {
		event_retry(e, 0);
		return -1;
	}
	e->sd_registered = 1;
	/* Don't wait forever! */
	if (eloop_register_timeout(EVENT_TIMEOUT_SEC, 0, event_timeout_handler,
				   NULL, e)) {
		event_retry(e, 0);
		return -1;
	}
	return 0;
}


/* event_send_all_later_handler -- actually send events as needed */
void event_send_all_later_handler(void *eloop_data, void *user_ctx)
{
	struct upnp_wps_device_sm *sm = user_ctx;
	struct subscription *s;
	struct subscription *s_old;
	int nerrors = 0;

	sm->event_send_all_queued = 0;
	s = sm->subscriptions;
	if (s == NULL)
		return;
	do {
		if (s->addr_list == NULL) {
			/* if we've given up on all addresses */
			wpa_printf(MSG_DEBUG, "WPS UPnP: Removing "
				   "subscription with no addresses");
			s_old = s;
			s = s_old->next;
			subscription_unlink(s_old);
			subscription_destroy(s_old);
		} else {
			if (s->current_event == NULL /* not busy */ &&
			    s->event_queue != NULL /* more to do */) {
				if (event_send_start(s))
					nerrors++;
			}
			s = s->next;
		}
	} while (sm->subscriptions != NULL && s != sm->subscriptions);

	if (nerrors) {
		/* Try again later */
		event_send_all_later(sm);
	}
}


/* event_send_all_later -- schedule sending events to all subscribers
 * that need it.
 * This avoids two problems:
 * -- After getting a subscription, we should not send the first event
 *      until after our reply is fully queued to be sent back,
 * -- Possible stack depth or infinite recursion issues.
 */
void event_send_all_later(struct upnp_wps_device_sm *sm)
{
	/*
	 * The exact time in the future isn't too important. Waiting a bit
	 * might let us do several together.
	 */
	if (sm->event_send_all_queued)
		return;
	sm->event_send_all_queued = 1;
	eloop_register_timeout(EVENT_DELAY_SECONDS, EVENT_DELAY_MSEC,
			       event_send_all_later_handler, NULL, sm);
}


/* event_send_stop_all -- cleanup */
void event_send_stop_all(struct upnp_wps_device_sm *sm)
{
	if (sm->event_send_all_queued)
		eloop_cancel_timeout(event_send_all_later_handler, NULL, sm);
	sm->event_send_all_queued = 0;
}


/**
 * event_add - Add a new event to a queue
 * @s: Subscription
 * @data: Event data (is copied; caller retains ownership)
 * Returns: 0 on success, 1 on error
 */
int event_add(struct subscription *s, const struct wpabuf *data)
{
	struct wps_event_ *e;

	if (s->n_queue >= MAX_EVENTS_QUEUED) {
		wpa_printf(MSG_DEBUG, "WPS UPnP: Too many events queued for "
			   "subscriber");
		return 1;
	}

	e = os_zalloc(sizeof(*e));
	if (e == NULL)
		return 1;
	e->s = s;
	e->sd = -1;
	e->data = wpabuf_dup(data);
	if (e->data == NULL) {
		os_free(e);
		return 1;
	}
	e->subscriber_sequence = s->next_subscriber_sequence++;
	if (s->next_subscriber_sequence == 0)
		s->next_subscriber_sequence++;
	event_enqueue_at_end(s, e);
	event_send_all_later(s->sm);
	return 0;
}
