/*
 * Copyright (C) 2004-2008, 2011, 2012  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2001-2003  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: controlconf.c,v 1.60.544.3 2011/12/22 08:10:09 marka Exp $ */

/*! \file */

#include <config.h>

#include <isc/base64.h>
#include <isc/buffer.h>
#include <isc/event.h>
#include <isc/mem.h>
#include <isc/net.h>
#include <isc/netaddr.h>
#include <isc/random.h>
#include <isc/result.h>
#include <isc/stdtime.h>
#include <isc/string.h>
#include <isc/timer.h>
#include <isc/util.h>

#include <isccfg/namedconf.h>

#include <bind9/check.h>

#include <isccc/alist.h>
#include <isccc/cc.h>
#include <isccc/ccmsg.h>
#include <isccc/events.h>
#include <isccc/result.h>
#include <isccc/sexpr.h>
#include <isccc/symtab.h>
#include <isccc/util.h>

#include <dns/result.h>

#include <named/config.h>
#include <named/control.h>
#include <named/log.h>
#include <named/server.h>

/*
 * Note: Listeners and connections are not locked.  All event handlers are
 * executed by the server task, and all callers of exported routines must
 * be running under the server task.
 */

typedef struct controlkey controlkey_t;
typedef ISC_LIST(controlkey_t) controlkeylist_t;

typedef struct controlconnection controlconnection_t;
typedef ISC_LIST(controlconnection_t) controlconnectionlist_t;

typedef struct controllistener controllistener_t;
typedef ISC_LIST(controllistener_t) controllistenerlist_t;

struct controlkey {
	char *				keyname;
	isc_region_t			secret;
	ISC_LINK(controlkey_t)		link;
};

struct controlconnection {
	isc_socket_t *			sock;
	isccc_ccmsg_t			ccmsg;
	isc_boolean_t			ccmsg_valid;
	isc_boolean_t			sending;
	isc_timer_t *			timer;
	unsigned char			buffer[2048];
	controllistener_t *		listener;
	isc_uint32_t			nonce;
	ISC_LINK(controlconnection_t)	link;
};

struct controllistener {
	ns_controls_t *			controls;
	isc_mem_t *			mctx;
	isc_task_t *			task;
	isc_sockaddr_t			address;
	isc_socket_t *			sock;
	dns_acl_t *			acl;
	isc_boolean_t			listening;
	isc_boolean_t			exiting;
	controlkeylist_t		keys;
	controlconnectionlist_t		connections;
	isc_sockettype_t		type;
	isc_uint32_t			perm;
	isc_uint32_t			owner;
	isc_uint32_t			group;
	ISC_LINK(controllistener_t)	link;
};

struct ns_controls {
	ns_server_t			*server;
	controllistenerlist_t 		listeners;
	isc_boolean_t			shuttingdown;
	isccc_symtab_t			*symtab;
};

static void control_newconn(isc_task_t *task, isc_event_t *event);
static void control_recvmessage(isc_task_t *task, isc_event_t *event);

#define CLOCKSKEW 300

static void
free_controlkey(controlkey_t *key, isc_mem_t *mctx) {
	if (key->keyname != NULL)
		isc_mem_free(mctx, key->keyname);
	if (key->secret.base != NULL)
		isc_mem_put(mctx, key->secret.base, key->secret.length);
	isc_mem_put(mctx, key, sizeof(*key));
}

static void
free_controlkeylist(controlkeylist_t *keylist, isc_mem_t *mctx) {
	while (!ISC_LIST_EMPTY(*keylist)) {
		controlkey_t *key = ISC_LIST_HEAD(*keylist);
		ISC_LIST_UNLINK(*keylist, key, link);
		free_controlkey(key, mctx);
	}
}

static void
free_listener(controllistener_t *listener) {
	INSIST(listener->exiting);
	INSIST(!listener->listening);
	INSIST(ISC_LIST_EMPTY(listener->connections));

	if (listener->sock != NULL)
		isc_socket_detach(&listener->sock);

	free_controlkeylist(&listener->keys, listener->mctx);

	if (listener->acl != NULL)
		dns_acl_detach(&listener->acl);

	isc_mem_put(listener->mctx, listener, sizeof(*listener));
}

static void
maybe_free_listener(controllistener_t *listener) {
	if (listener->exiting &&
	    !listener->listening &&
	    ISC_LIST_EMPTY(listener->connections))
		free_listener(listener);
}

static void
maybe_free_connection(controlconnection_t *conn) {
	controllistener_t *listener = conn->listener;

	if (conn->timer != NULL)
		isc_timer_detach(&conn->timer);

	if (conn->ccmsg_valid) {
		isccc_ccmsg_cancelread(&conn->ccmsg);
		return;
	}

	if (conn->sending) {
		isc_socket_cancel(conn->sock, listener->task,
				  ISC_SOCKCANCEL_SEND);
		return;
	}

	ISC_LIST_UNLINK(listener->connections, conn, link);
	isc_mem_put(listener->mctx, conn, sizeof(*conn));
}

static void
shutdown_listener(controllistener_t *listener) {
	controlconnection_t *conn;
	controlconnection_t *next;

	if (!listener->exiting) {
		char socktext[ISC_SOCKADDR_FORMATSIZE];

		ISC_LIST_UNLINK(listener->controls->listeners, listener, link);

		isc_sockaddr_format(&listener->address, socktext,
				    sizeof(socktext));
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_CONTROL, ISC_LOG_NOTICE,
			      "stopping command channel on %s", socktext);
		if (listener->type == isc_sockettype_unix)
			isc_socket_cleanunix(&listener->address, ISC_TRUE);
		listener->exiting = ISC_TRUE;
	}

	for (conn = ISC_LIST_HEAD(listener->connections);
	     conn != NULL;
	     conn = next)
	{
		next = ISC_LIST_NEXT(conn, link);
		maybe_free_connection(conn);
	}

	if (listener->listening)
		isc_socket_cancel(listener->sock, listener->task,
				  ISC_SOCKCANCEL_ACCEPT);

	maybe_free_listener(listener);
}

static isc_boolean_t
address_ok(isc_sockaddr_t *sockaddr, dns_acl_t *acl) {
	isc_netaddr_t netaddr;
	isc_result_t result;
	int match;

	isc_netaddr_fromsockaddr(&netaddr, sockaddr);

	result = dns_acl_match(&netaddr, NULL, acl,
			       &ns_g_server->aclenv, &match, NULL);

	if (result != ISC_R_SUCCESS || match <= 0)
		return (ISC_FALSE);
	else
		return (ISC_TRUE);
}

static isc_result_t
control_accept(controllistener_t *listener) {
	isc_result_t result;
	result = isc_socket_accept(listener->sock,
				   listener->task,
				   control_newconn, listener);
	if (result != ISC_R_SUCCESS)
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "isc_socket_accept() failed: %s",
				 isc_result_totext(result));
	else
		listener->listening = ISC_TRUE;
	return (result);
}

static isc_result_t
control_listen(controllistener_t *listener) {
	isc_result_t result;

	result = isc_socket_listen(listener->sock, 0);
	if (result != ISC_R_SUCCESS)
		UNEXPECTED_ERROR(__FILE__, __LINE__,
				 "isc_socket_listen() failed: %s",
				 isc_result_totext(result));
	return (result);
}

static void
control_next(controllistener_t *listener) {
	(void)control_accept(listener);
}

static void
control_senddone(isc_task_t *task, isc_event_t *event) {
	isc_socketevent_t *sevent = (isc_socketevent_t *) event;
	controlconnection_t *conn = event->ev_arg;
	controllistener_t *listener = conn->listener;
	isc_socket_t *sock = (isc_socket_t *)sevent->ev_sender;
	isc_result_t result;

	REQUIRE(conn->sending);

	UNUSED(task);

	conn->sending = ISC_FALSE;

	if (sevent->result != ISC_R_SUCCESS &&
	    sevent->result != ISC_R_CANCELED)
	{
		char socktext[ISC_SOCKADDR_FORMATSIZE];
		isc_sockaddr_t peeraddr;

		(void)isc_socket_getpeername(sock, &peeraddr);
		isc_sockaddr_format(&peeraddr, socktext, sizeof(socktext));
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_CONTROL, ISC_LOG_WARNING,
			      "error sending command response to %s: %s",
			      socktext, isc_result_totext(sevent->result));
	}
	isc_event_free(&event);

	result = isccc_ccmsg_readmessage(&conn->ccmsg, listener->task,
					 control_recvmessage, conn);
	if (result != ISC_R_SUCCESS) {
		isc_socket_detach(&conn->sock);
		maybe_free_connection(conn);
		maybe_free_listener(listener);
	}
}

static inline void
log_invalid(isccc_ccmsg_t *ccmsg, isc_result_t result) {
	char socktext[ISC_SOCKADDR_FORMATSIZE];
	isc_sockaddr_t peeraddr;

	(void)isc_socket_getpeername(ccmsg->sock, &peeraddr);
	isc_sockaddr_format(&peeraddr, socktext, sizeof(socktext));
	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
		      NS_LOGMODULE_CONTROL, ISC_LOG_ERROR,
		      "invalid command from %s: %s",
		      socktext, isc_result_totext(result));
}

static void
control_recvmessage(isc_task_t *task, isc_event_t *event) {
	controlconnection_t *conn;
	controllistener_t *listener;
	controlkey_t *key;
	isccc_sexpr_t *request = NULL;
	isccc_sexpr_t *response = NULL;
	isccc_region_t ccregion;
	isccc_region_t secret;
	isc_stdtime_t now;
	isc_buffer_t b;
	isc_region_t r;
	isc_uint32_t len;
	isc_buffer_t text;
	char textarray[1024];
	isc_result_t result;
	isc_result_t eresult;
	isccc_sexpr_t *_ctrl;
	isccc_time_t sent;
	isccc_time_t exp;
	isc_uint32_t nonce;

	REQUIRE(event->ev_type == ISCCC_EVENT_CCMSG);

	conn = event->ev_arg;
	listener = conn->listener;
	secret.rstart = NULL;

	/* Is the server shutting down? */
	if (listener->controls->shuttingdown)
		goto cleanup;

	if (conn->ccmsg.result != ISC_R_SUCCESS) {
		if (conn->ccmsg.result != ISC_R_CANCELED &&
		    conn->ccmsg.result != ISC_R_EOF)
			log_invalid(&conn->ccmsg, conn->ccmsg.result);
		goto cleanup;
	}

	request = NULL;

	for (key = ISC_LIST_HEAD(listener->keys);
	     key != NULL;
	     key = ISC_LIST_NEXT(key, link))
	{
		ccregion.rstart = isc_buffer_base(&conn->ccmsg.buffer);
		ccregion.rend = isc_buffer_used(&conn->ccmsg.buffer);
		secret.rstart = isc_mem_get(listener->mctx, key->secret.length);
		if (secret.rstart == NULL)
			goto cleanup;
		memcpy(secret.rstart, key->secret.base, key->secret.length);
		secret.rend = secret.rstart + key->secret.length;
		result = isccc_cc_fromwire(&ccregion, &request, &secret);
		if (result == ISC_R_SUCCESS)
			break;
		isc_mem_put(listener->mctx, secret.rstart, REGION_SIZE(secret));
		log_invalid(&conn->ccmsg, result);
		goto cleanup;
	}

	if (key == NULL) {
		log_invalid(&conn->ccmsg, ISCCC_R_BADAUTH);
		goto cleanup;
	}

	/* We shouldn't be getting a reply. */
	if (isccc_cc_isreply(request)) {
		log_invalid(&conn->ccmsg, ISC_R_FAILURE);
		goto cleanup_request;
	}

	isc_stdtime_get(&now);

	/*
	 * Limit exposure to replay attacks.
	 */
	_ctrl = isccc_alist_lookup(request, "_ctrl");
	if (_ctrl == NULL) {
		log_invalid(&conn->ccmsg, ISC_R_FAILURE);
		goto cleanup_request;
	}

	if (isccc_cc_lookupuint32(_ctrl, "_tim", &sent) == ISC_R_SUCCESS) {
		if ((sent + CLOCKSKEW) < now || (sent - CLOCKSKEW) > now) {
			log_invalid(&conn->ccmsg, ISCCC_R_CLOCKSKEW);
			goto cleanup_request;
		}
	} else {
		log_invalid(&conn->ccmsg, ISC_R_FAILURE);
		goto cleanup_request;
	}

	/*
	 * Expire messages that are too old.
	 */
	if (isccc_cc_lookupuint32(_ctrl, "_exp", &exp) == ISC_R_SUCCESS &&
	    now > exp) {
		log_invalid(&conn->ccmsg, ISCCC_R_EXPIRED);
		goto cleanup_request;
	}

	/*
	 * Duplicate suppression (required for UDP).
	 */
	isccc_cc_cleansymtab(listener->controls->symtab, now);
	result = isccc_cc_checkdup(listener->controls->symtab, request, now);
	if (result != ISC_R_SUCCESS) {
		if (result == ISC_R_EXISTS)
			result = ISCCC_R_DUPLICATE;
		log_invalid(&conn->ccmsg, result);
		goto cleanup_request;
	}

	if (conn->nonce != 0 &&
	    (isccc_cc_lookupuint32(_ctrl, "_nonce", &nonce) != ISC_R_SUCCESS ||
	     conn->nonce != nonce)) {
		log_invalid(&conn->ccmsg, ISCCC_R_BADAUTH);
		goto cleanup_request;
	}

	/*
	 * Establish nonce.
	 */
	while (conn->nonce == 0)
		isc_random_get(&conn->nonce);

	isc_buffer_init(&text, textarray, sizeof(textarray));
	eresult = ns_control_docommand(request, &text);

	result = isccc_cc_createresponse(request, now, now + 60, &response);
	if (result != ISC_R_SUCCESS)
		goto cleanup_request;
	if (eresult != ISC_R_SUCCESS) {
		isccc_sexpr_t *data;

		data = isccc_alist_lookup(response, "_data");
		if (data != NULL) {
			const char *estr = isc_result_totext(eresult);
			if (isccc_cc_definestring(data, "err", estr) == NULL)
				goto cleanup_response;
		}
	}

	if (isc_buffer_usedlength(&text) > 0) {
		isccc_sexpr_t *data;

		data = isccc_alist_lookup(response, "_data");
		if (data != NULL) {
			char *str = (char *)isc_buffer_base(&text);
			if (isccc_cc_definestring(data, "text", str) == NULL)
				goto cleanup_response;
		}
	}

	_ctrl = isccc_alist_lookup(response, "_ctrl");
	if (_ctrl == NULL ||
	    isccc_cc_defineuint32(_ctrl, "_nonce", conn->nonce) == NULL)
		goto cleanup_response;

	ccregion.rstart = conn->buffer + 4;
	ccregion.rend = conn->buffer + sizeof(conn->buffer);
	result = isccc_cc_towire(response, &ccregion, &secret);
	if (result != ISC_R_SUCCESS)
		goto cleanup_response;
	isc_buffer_init(&b, conn->buffer, 4);
	len = sizeof(conn->buffer) - REGION_SIZE(ccregion);
	isc_buffer_putuint32(&b, len - 4);
	r.base = conn->buffer;
	r.length = len;

	result = isc_socket_send(conn->sock, &r, task, control_senddone, conn);
	if (result != ISC_R_SUCCESS)
		goto cleanup_response;
	conn->sending = ISC_TRUE;

	isc_mem_put(listener->mctx, secret.rstart, REGION_SIZE(secret));
	isccc_sexpr_free(&request);
	isccc_sexpr_free(&response);
	return;

 cleanup_response:
	isccc_sexpr_free(&response);

 cleanup_request:
	isccc_sexpr_free(&request);
	isc_mem_put(listener->mctx, secret.rstart, REGION_SIZE(secret));

 cleanup:
	isc_socket_detach(&conn->sock);
	isccc_ccmsg_invalidate(&conn->ccmsg);
	conn->ccmsg_valid = ISC_FALSE;
	maybe_free_connection(conn);
	maybe_free_listener(listener);
}

static void
control_timeout(isc_task_t *task, isc_event_t *event) {
	controlconnection_t *conn = event->ev_arg;

	UNUSED(task);

	isc_timer_detach(&conn->timer);
	maybe_free_connection(conn);

	isc_event_free(&event);
}

static isc_result_t
newconnection(controllistener_t *listener, isc_socket_t *sock) {
	controlconnection_t *conn;
	isc_interval_t interval;
	isc_result_t result;

	conn = isc_mem_get(listener->mctx, sizeof(*conn));
	if (conn == NULL)
		return (ISC_R_NOMEMORY);

	conn->sock = sock;
	isccc_ccmsg_init(listener->mctx, sock, &conn->ccmsg);
	conn->ccmsg_valid = ISC_TRUE;
	conn->sending = ISC_FALSE;
	conn->timer = NULL;
	isc_interval_set(&interval, 60, 0);
	result = isc_timer_create(ns_g_timermgr, isc_timertype_once,
				  NULL, &interval, listener->task,
				  control_timeout, conn, &conn->timer);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	conn->listener = listener;
	conn->nonce = 0;
	ISC_LINK_INIT(conn, link);

	result = isccc_ccmsg_readmessage(&conn->ccmsg, listener->task,
					 control_recvmessage, conn);
	if (result != ISC_R_SUCCESS)
		goto cleanup;
	isccc_ccmsg_setmaxsize(&conn->ccmsg, 2048);

	ISC_LIST_APPEND(listener->connections, conn, link);
	return (ISC_R_SUCCESS);

 cleanup:
	isccc_ccmsg_invalidate(&conn->ccmsg);
	if (conn->timer != NULL)
		isc_timer_detach(&conn->timer);
	isc_mem_put(listener->mctx, conn, sizeof(*conn));
	return (result);
}

static void
control_newconn(isc_task_t *task, isc_event_t *event) {
	isc_socket_newconnev_t *nevent = (isc_socket_newconnev_t *)event;
	controllistener_t *listener = event->ev_arg;
	isc_socket_t *sock;
	isc_sockaddr_t peeraddr;
	isc_result_t result;

	UNUSED(task);

	listener->listening = ISC_FALSE;

	if (nevent->result != ISC_R_SUCCESS) {
		if (nevent->result == ISC_R_CANCELED) {
			shutdown_listener(listener);
			goto cleanup;
		}
		goto restart;
	}

	sock = nevent->newsocket;
	isc_socket_setname(sock, "control", NULL);
	(void)isc_socket_getpeername(sock, &peeraddr);
	if (listener->type == isc_sockettype_tcp &&
	    !address_ok(&peeraddr, listener->acl)) {
		char socktext[ISC_SOCKADDR_FORMATSIZE];
		isc_sockaddr_format(&peeraddr, socktext, sizeof(socktext));
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_CONTROL, ISC_LOG_WARNING,
			      "rejected command channel message from %s",
			      socktext);
		isc_socket_detach(&sock);
		goto restart;
	}

	result = newconnection(listener, sock);
	if (result != ISC_R_SUCCESS) {
		char socktext[ISC_SOCKADDR_FORMATSIZE];
		isc_sockaddr_format(&peeraddr, socktext, sizeof(socktext));
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_CONTROL, ISC_LOG_WARNING,
			      "dropped command channel from %s: %s",
			      socktext, isc_result_totext(result));
		isc_socket_detach(&sock);
		goto restart;
	}

 restart:
	control_next(listener);
 cleanup:
	isc_event_free(&event);
}

static void
controls_shutdown(ns_controls_t *controls) {
	controllistener_t *listener;
	controllistener_t *next;

	for (listener = ISC_LIST_HEAD(controls->listeners);
	     listener != NULL;
	     listener = next)
	{
		/*
		 * This is asynchronous.  As listeners shut down, they will
		 * call their callbacks.
		 */
		next = ISC_LIST_NEXT(listener, link);
		shutdown_listener(listener);
	}
}

void
ns_controls_shutdown(ns_controls_t *controls) {
	controls_shutdown(controls);
	controls->shuttingdown = ISC_TRUE;
}

static isc_result_t
cfgkeylist_find(const cfg_obj_t *keylist, const char *keyname,
		const cfg_obj_t **objp)
{
	const cfg_listelt_t *element;
	const char *str;
	const cfg_obj_t *obj;

	for (element = cfg_list_first(keylist);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		obj = cfg_listelt_value(element);
		str = cfg_obj_asstring(cfg_map_getname(obj));
		if (strcasecmp(str, keyname) == 0)
			break;
	}
	if (element == NULL)
		return (ISC_R_NOTFOUND);
	obj = cfg_listelt_value(element);
	*objp = obj;
	return (ISC_R_SUCCESS);
}

static isc_result_t
controlkeylist_fromcfg(const cfg_obj_t *keylist, isc_mem_t *mctx,
		       controlkeylist_t *keyids)
{
	const cfg_listelt_t *element;
	char *newstr = NULL;
	const char *str;
	const cfg_obj_t *obj;
	controlkey_t *key;

	for (element = cfg_list_first(keylist);
	     element != NULL;
	     element = cfg_list_next(element))
	{
		obj = cfg_listelt_value(element);
		str = cfg_obj_asstring(obj);
		newstr = isc_mem_strdup(mctx, str);
		if (newstr == NULL)
			goto cleanup;
		key = isc_mem_get(mctx, sizeof(*key));
		if (key == NULL)
			goto cleanup;
		key->keyname = newstr;
		key->secret.base = NULL;
		key->secret.length = 0;
		ISC_LINK_INIT(key, link);
		ISC_LIST_APPEND(*keyids, key, link);
		newstr = NULL;
	}
	return (ISC_R_SUCCESS);

 cleanup:
	if (newstr != NULL)
		isc_mem_free(mctx, newstr);
	free_controlkeylist(keyids, mctx);
	return (ISC_R_NOMEMORY);
}

static void
register_keys(const cfg_obj_t *control, const cfg_obj_t *keylist,
	      controlkeylist_t *keyids, isc_mem_t *mctx, const char *socktext)
{
	controlkey_t *keyid, *next;
	const cfg_obj_t *keydef;
	char secret[1024];
	isc_buffer_t b;
	isc_result_t result;

	/*
	 * Find the keys corresponding to the keyids used by this listener.
	 */
	for (keyid = ISC_LIST_HEAD(*keyids); keyid != NULL; keyid = next) {
		next = ISC_LIST_NEXT(keyid, link);

		result = cfgkeylist_find(keylist, keyid->keyname, &keydef);
		if (result != ISC_R_SUCCESS) {
			cfg_obj_log(control, ns_g_lctx, ISC_LOG_WARNING,
				    "couldn't find key '%s' for use with "
				    "command channel %s",
				    keyid->keyname, socktext);
			ISC_LIST_UNLINK(*keyids, keyid, link);
			free_controlkey(keyid, mctx);
		} else {
			const cfg_obj_t *algobj = NULL;
			const cfg_obj_t *secretobj = NULL;
			const char *algstr = NULL;
			const char *secretstr = NULL;

			(void)cfg_map_get(keydef, "algorithm", &algobj);
			(void)cfg_map_get(keydef, "secret", &secretobj);
			INSIST(algobj != NULL && secretobj != NULL);

			algstr = cfg_obj_asstring(algobj);
			secretstr = cfg_obj_asstring(secretobj);

			if (ns_config_getkeyalgorithm(algstr, NULL, NULL) !=
			    ISC_R_SUCCESS)
			{
				cfg_obj_log(control, ns_g_lctx,
					    ISC_LOG_WARNING,
					    "unsupported algorithm '%s' in "
					    "key '%s' for use with command "
					    "channel %s",
					    algstr, keyid->keyname, socktext);
				ISC_LIST_UNLINK(*keyids, keyid, link);
				free_controlkey(keyid, mctx);
				continue;
			}

			isc_buffer_init(&b, secret, sizeof(secret));
			result = isc_base64_decodestring(secretstr, &b);

			if (result != ISC_R_SUCCESS) {
				cfg_obj_log(keydef, ns_g_lctx, ISC_LOG_WARNING,
					    "secret for key '%s' on "
					    "command channel %s: %s",
					    keyid->keyname, socktext,
					    isc_result_totext(result));
				ISC_LIST_UNLINK(*keyids, keyid, link);
				free_controlkey(keyid, mctx);
				continue;
			}

			keyid->secret.length = isc_buffer_usedlength(&b);
			keyid->secret.base = isc_mem_get(mctx,
							 keyid->secret.length);
			if (keyid->secret.base == NULL) {
				cfg_obj_log(keydef, ns_g_lctx, ISC_LOG_WARNING,
					   "couldn't register key '%s': "
					   "out of memory", keyid->keyname);
				ISC_LIST_UNLINK(*keyids, keyid, link);
				free_controlkey(keyid, mctx);
				break;
			}
			memcpy(keyid->secret.base, isc_buffer_base(&b),
			       keyid->secret.length);
		}
	}
}

#define CHECK(x) \
	do { \
		 result = (x); \
		 if (result != ISC_R_SUCCESS) \
			goto cleanup; \
	} while (0)

static isc_result_t
get_rndckey(isc_mem_t *mctx, controlkeylist_t *keyids) {
	isc_result_t result;
	cfg_parser_t *pctx = NULL;
	cfg_obj_t *config = NULL;
	const cfg_obj_t *key = NULL;
	const cfg_obj_t *algobj = NULL;
	const cfg_obj_t *secretobj = NULL;
	const char *algstr = NULL;
	const char *secretstr = NULL;
	controlkey_t *keyid = NULL;
	char secret[1024];
	isc_buffer_t b;

	CHECK(cfg_parser_create(mctx, ns_g_lctx, &pctx));
	CHECK(cfg_parse_file(pctx, ns_g_keyfile, &cfg_type_rndckey, &config));
	CHECK(cfg_map_get(config, "key", &key));

	keyid = isc_mem_get(mctx, sizeof(*keyid));
	if (keyid == NULL)
		CHECK(ISC_R_NOMEMORY);
	keyid->keyname = isc_mem_strdup(mctx,
					cfg_obj_asstring(cfg_map_getname(key)));
	keyid->secret.base = NULL;
	keyid->secret.length = 0;
	ISC_LINK_INIT(keyid, link);
	if (keyid->keyname == NULL)
		CHECK(ISC_R_NOMEMORY);

	CHECK(bind9_check_key(key, ns_g_lctx));

	(void)cfg_map_get(key, "algorithm", &algobj);
	(void)cfg_map_get(key, "secret", &secretobj);
	INSIST(algobj != NULL && secretobj != NULL);

	algstr = cfg_obj_asstring(algobj);
	secretstr = cfg_obj_asstring(secretobj);

	if (ns_config_getkeyalgorithm(algstr, NULL, NULL) != ISC_R_SUCCESS) {
		cfg_obj_log(key, ns_g_lctx,
			    ISC_LOG_WARNING,
			    "unsupported algorithm '%s' in "
			    "key '%s' for use with command "
			    "channel",
			    algstr, keyid->keyname);
		goto cleanup;
	}

	isc_buffer_init(&b, secret, sizeof(secret));
	result = isc_base64_decodestring(secretstr, &b);

	if (result != ISC_R_SUCCESS) {
		cfg_obj_log(key, ns_g_lctx, ISC_LOG_WARNING,
			    "secret for key '%s' on command channel: %s",
			    keyid->keyname, isc_result_totext(result));
		goto cleanup;
	}

	keyid->secret.length = isc_buffer_usedlength(&b);
	keyid->secret.base = isc_mem_get(mctx,
					 keyid->secret.length);
	if (keyid->secret.base == NULL) {
		cfg_obj_log(key, ns_g_lctx, ISC_LOG_WARNING,
			   "couldn't register key '%s': "
			   "out of memory", keyid->keyname);
		CHECK(ISC_R_NOMEMORY);
	}
	memcpy(keyid->secret.base, isc_buffer_base(&b),
	       keyid->secret.length);
	ISC_LIST_APPEND(*keyids, keyid, link);
	keyid = NULL;
	result = ISC_R_SUCCESS;

  cleanup:
	if (keyid != NULL)
		free_controlkey(keyid, mctx);
	if (config != NULL)
		cfg_obj_destroy(pctx, &config);
	if (pctx != NULL)
		cfg_parser_destroy(&pctx);
	return (result);
}

/*
 * Ensures that both '*global_keylistp' and '*control_keylistp' are
 * valid or both are NULL.
 */
static void
get_key_info(const cfg_obj_t *config, const cfg_obj_t *control,
	     const cfg_obj_t **global_keylistp,
	     const cfg_obj_t **control_keylistp)
{
	isc_result_t result;
	const cfg_obj_t *control_keylist = NULL;
	const cfg_obj_t *global_keylist = NULL;

	REQUIRE(global_keylistp != NULL && *global_keylistp == NULL);
	REQUIRE(control_keylistp != NULL && *control_keylistp == NULL);

	control_keylist = cfg_tuple_get(control, "keys");

	if (!cfg_obj_isvoid(control_keylist) &&
	    cfg_list_first(control_keylist) != NULL) {
		result = cfg_map_get(config, "key", &global_keylist);

		if (result == ISC_R_SUCCESS) {
			*global_keylistp = global_keylist;
			*control_keylistp = control_keylist;
		}
	}
}

static void
update_listener(ns_controls_t *cp, controllistener_t **listenerp,
		const cfg_obj_t *control, const cfg_obj_t *config,
		isc_sockaddr_t *addr, cfg_aclconfctx_t *aclconfctx,
		const char *socktext, isc_sockettype_t type)
{
	controllistener_t *listener;
	const cfg_obj_t *allow;
	const cfg_obj_t *global_keylist = NULL;
	const cfg_obj_t *control_keylist = NULL;
	dns_acl_t *new_acl = NULL;
	controlkeylist_t keys;
	isc_result_t result = ISC_R_SUCCESS;

	for (listener = ISC_LIST_HEAD(cp->listeners);
	     listener != NULL;
	     listener = ISC_LIST_NEXT(listener, link))
		if (isc_sockaddr_equal(addr, &listener->address))
			break;

	if (listener == NULL) {
		*listenerp = NULL;
		return;
	}

	/*
	 * There is already a listener for this sockaddr.
	 * Update the access list and key information.
	 *
	 * First try to deal with the key situation.  There are a few
	 * possibilities:
	 *  (a)	It had an explicit keylist and still has an explicit keylist.
	 *  (b)	It had an automagic key and now has an explicit keylist.
	 *  (c)	It had an explicit keylist and now needs an automagic key.
	 *  (d) It has an automagic key and still needs the automagic key.
	 *
	 * (c) and (d) are the annoying ones.  The caller needs to know
	 * that it should use the automagic configuration for key information
	 * in place of the named.conf configuration.
	 *
	 * XXXDCL There is one other hazard that has not been dealt with,
	 * the problem that if a key change is being caused by a control
	 * channel reload, then the response will be with the new key
	 * and not able to be decrypted by the client.
	 */
	if (control != NULL)
		get_key_info(config, control, &global_keylist,
			     &control_keylist);

	if (control_keylist != NULL) {
		INSIST(global_keylist != NULL);

		ISC_LIST_INIT(keys);
		result = controlkeylist_fromcfg(control_keylist,
						listener->mctx, &keys);
		if (result == ISC_R_SUCCESS) {
			free_controlkeylist(&listener->keys, listener->mctx);
			listener->keys = keys;
			register_keys(control, global_keylist, &listener->keys,
				      listener->mctx, socktext);
		}
	} else {
		free_controlkeylist(&listener->keys, listener->mctx);
		result = get_rndckey(listener->mctx, &listener->keys);
	}

	if (result != ISC_R_SUCCESS && global_keylist != NULL) {
		/*
		 * This message might be a little misleading since the
		 * "new keys" might in fact be identical to the old ones,
		 * but tracking whether they are identical just for the
		 * sake of avoiding this message would be too much trouble.
		 */
		if (control != NULL)
			cfg_obj_log(control, ns_g_lctx, ISC_LOG_WARNING,
				    "couldn't install new keys for "
				    "command channel %s: %s",
				    socktext, isc_result_totext(result));
		else
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_CONTROL, ISC_LOG_WARNING,
				      "couldn't install new keys for "
				      "command channel %s: %s",
				      socktext, isc_result_totext(result));
	}

	/*
	 * Now, keep the old access list unless a new one can be made.
	 */
	if (control != NULL && type == isc_sockettype_tcp) {
		allow = cfg_tuple_get(control, "allow");
		result = cfg_acl_fromconfig(allow, config, ns_g_lctx,
					    aclconfctx, listener->mctx, 0,
					    &new_acl);
	} else {
		result = dns_acl_any(listener->mctx, &new_acl);
	}

	if (result == ISC_R_SUCCESS) {
		dns_acl_detach(&listener->acl);
		dns_acl_attach(new_acl, &listener->acl);
		dns_acl_detach(&new_acl);
		/* XXXDCL say the old acl is still used? */
	} else if (control != NULL)
		cfg_obj_log(control, ns_g_lctx, ISC_LOG_WARNING,
			    "couldn't install new acl for "
			    "command channel %s: %s",
			    socktext, isc_result_totext(result));
	else
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_CONTROL, ISC_LOG_WARNING,
			      "couldn't install new acl for "
			      "command channel %s: %s",
			      socktext, isc_result_totext(result));

	if (result == ISC_R_SUCCESS && type == isc_sockettype_unix) {
		isc_uint32_t perm, owner, group;
		perm  = cfg_obj_asuint32(cfg_tuple_get(control, "perm"));
		owner = cfg_obj_asuint32(cfg_tuple_get(control, "owner"));
		group = cfg_obj_asuint32(cfg_tuple_get(control, "group"));
		result = ISC_R_SUCCESS;
		if (listener->perm != perm || listener->owner != owner ||
		    listener->group != group)
			result = isc_socket_permunix(&listener->address, perm,
						     owner, group);
		if (result == ISC_R_SUCCESS) {
			listener->perm = perm;
			listener->owner = owner;
			listener->group = group;
		} else if (control != NULL)
			cfg_obj_log(control, ns_g_lctx, ISC_LOG_WARNING,
				    "couldn't update ownership/permission for "
				    "command channel %s", socktext);
	}

	*listenerp = listener;
}

static void
add_listener(ns_controls_t *cp, controllistener_t **listenerp,
	     const cfg_obj_t *control, const cfg_obj_t *config,
	     isc_sockaddr_t *addr, cfg_aclconfctx_t *aclconfctx,
	     const char *socktext, isc_sockettype_t type)
{
	isc_mem_t *mctx = cp->server->mctx;
	controllistener_t *listener;
	const cfg_obj_t *allow;
	const cfg_obj_t *global_keylist = NULL;
	const cfg_obj_t *control_keylist = NULL;
	dns_acl_t *new_acl = NULL;
	isc_result_t result = ISC_R_SUCCESS;

	listener = isc_mem_get(mctx, sizeof(*listener));
	if (listener == NULL)
		result = ISC_R_NOMEMORY;

	if (result == ISC_R_SUCCESS) {
		listener->controls = cp;
		listener->mctx = mctx;
		listener->task = cp->server->task;
		listener->address = *addr;
		listener->sock = NULL;
		listener->listening = ISC_FALSE;
		listener->exiting = ISC_FALSE;
		listener->acl = NULL;
		listener->type = type;
		listener->perm = 0;
		listener->owner = 0;
		listener->group = 0;
		ISC_LINK_INIT(listener, link);
		ISC_LIST_INIT(listener->keys);
		ISC_LIST_INIT(listener->connections);

		/*
		 * Make the acl.
		 */
		if (control != NULL && type == isc_sockettype_tcp) {
			allow = cfg_tuple_get(control, "allow");
			result = cfg_acl_fromconfig(allow, config, ns_g_lctx,
						    aclconfctx, mctx, 0,
						    &new_acl);
		} else {
			result = dns_acl_any(mctx, &new_acl);
		}
	}

	if (result == ISC_R_SUCCESS) {
		dns_acl_attach(new_acl, &listener->acl);
		dns_acl_detach(&new_acl);

		if (config != NULL)
			get_key_info(config, control, &global_keylist,
				     &control_keylist);

		if (control_keylist != NULL) {
			result = controlkeylist_fromcfg(control_keylist,
							listener->mctx,
							&listener->keys);
			if (result == ISC_R_SUCCESS)
				register_keys(control, global_keylist,
					      &listener->keys,
					      listener->mctx, socktext);
		} else
			result = get_rndckey(mctx, &listener->keys);

		if (result != ISC_R_SUCCESS && control != NULL)
			cfg_obj_log(control, ns_g_lctx, ISC_LOG_WARNING,
				    "couldn't install keys for "
				    "command channel %s: %s",
				    socktext, isc_result_totext(result));
	}

	if (result == ISC_R_SUCCESS) {
		int pf = isc_sockaddr_pf(&listener->address);
		if ((pf == AF_INET && isc_net_probeipv4() != ISC_R_SUCCESS) ||
#ifdef ISC_PLATFORM_HAVESYSUNH
		    (pf == AF_UNIX && isc_net_probeunix() != ISC_R_SUCCESS) ||
#endif
		    (pf == AF_INET6 && isc_net_probeipv6() != ISC_R_SUCCESS))
			result = ISC_R_FAMILYNOSUPPORT;
	}

	if (result == ISC_R_SUCCESS && type == isc_sockettype_unix)
		isc_socket_cleanunix(&listener->address, ISC_FALSE);

	if (result == ISC_R_SUCCESS)
		result = isc_socket_create(ns_g_socketmgr,
					   isc_sockaddr_pf(&listener->address),
					   type, &listener->sock);
	if (result == ISC_R_SUCCESS)
		isc_socket_setname(listener->sock, "control", NULL);

#ifndef ISC_ALLOW_MAPPED
	if (result == ISC_R_SUCCESS)
		isc_socket_ipv6only(listener->sock, ISC_TRUE);
#endif

	if (result == ISC_R_SUCCESS)
		result = isc_socket_bind(listener->sock, &listener->address,
					 ISC_SOCKET_REUSEADDRESS);

	if (result == ISC_R_SUCCESS && type == isc_sockettype_unix) {
		listener->perm = cfg_obj_asuint32(cfg_tuple_get(control,
								"perm"));
		listener->owner = cfg_obj_asuint32(cfg_tuple_get(control,
								 "owner"));
		listener->group = cfg_obj_asuint32(cfg_tuple_get(control,
								 "group"));
		result = isc_socket_permunix(&listener->address, listener->perm,
					     listener->owner, listener->group);
	}
	if (result == ISC_R_SUCCESS)
		result = control_listen(listener);

	if (result == ISC_R_SUCCESS)
		result = control_accept(listener);

	if (result == ISC_R_SUCCESS) {
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_CONTROL, ISC_LOG_NOTICE,
			      "command channel listening on %s", socktext);
		*listenerp = listener;

	} else {
		if (listener != NULL) {
			listener->exiting = ISC_TRUE;
			free_listener(listener);
		}

		if (control != NULL)
			cfg_obj_log(control, ns_g_lctx, ISC_LOG_WARNING,
				    "couldn't add command channel %s: %s",
				    socktext, isc_result_totext(result));
		else
			isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
				      NS_LOGMODULE_CONTROL, ISC_LOG_NOTICE,
				      "couldn't add command channel %s: %s",
				      socktext, isc_result_totext(result));

		*listenerp = NULL;
	}

	/* XXXDCL return error results? fail hard? */
}

isc_result_t
ns_controls_configure(ns_controls_t *cp, const cfg_obj_t *config,
		      cfg_aclconfctx_t *aclconfctx)
{
	controllistener_t *listener;
	controllistenerlist_t new_listeners;
	const cfg_obj_t *controlslist = NULL;
	const cfg_listelt_t *element, *element2;
	char socktext[ISC_SOCKADDR_FORMATSIZE];

	ISC_LIST_INIT(new_listeners);

	/*
	 * Get the list of named.conf 'controls' statements.
	 */
	(void)cfg_map_get(config, "controls", &controlslist);

	/*
	 * Run through the new control channel list, noting sockets that
	 * are already being listened on and moving them to the new list.
	 *
	 * Identifying duplicate addr/port combinations is left to either
	 * the underlying config code, or to the bind attempt getting an
	 * address-in-use error.
	 */
	if (controlslist != NULL) {
		for (element = cfg_list_first(controlslist);
		     element != NULL;
		     element = cfg_list_next(element)) {
			const cfg_obj_t *controls;
			const cfg_obj_t *inetcontrols = NULL;

			controls = cfg_listelt_value(element);
			(void)cfg_map_get(controls, "inet", &inetcontrols);
			if (inetcontrols == NULL)
				continue;

			for (element2 = cfg_list_first(inetcontrols);
			     element2 != NULL;
			     element2 = cfg_list_next(element2)) {
				const cfg_obj_t *control;
				const cfg_obj_t *obj;
				isc_sockaddr_t addr;

				/*
				 * The parser handles BIND 8 configuration file
				 * syntax, so it allows unix phrases as well
				 * inet phrases with no keys{} clause.
				 */
				control = cfg_listelt_value(element2);

				obj = cfg_tuple_get(control, "address");
				addr = *cfg_obj_assockaddr(obj);
				if (isc_sockaddr_getport(&addr) == 0)
					isc_sockaddr_setport(&addr,
							     NS_CONTROL_PORT);

				isc_sockaddr_format(&addr, socktext,
						    sizeof(socktext));

				isc_log_write(ns_g_lctx,
					      NS_LOGCATEGORY_GENERAL,
					      NS_LOGMODULE_CONTROL,
					      ISC_LOG_DEBUG(9),
					      "processing control channel %s",
					      socktext);

				update_listener(cp, &listener, control, config,
						&addr, aclconfctx, socktext,
						isc_sockettype_tcp);

				if (listener != NULL)
					/*
					 * Remove the listener from the old
					 * list, so it won't be shut down.
					 */
					ISC_LIST_UNLINK(cp->listeners,
							listener, link);
				else
					/*
					 * This is a new listener.
					 */
					add_listener(cp, &listener, control,
						     config, &addr, aclconfctx,
						     socktext,
						     isc_sockettype_tcp);

				if (listener != NULL)
					ISC_LIST_APPEND(new_listeners,
							listener, link);
			}
		}
		for (element = cfg_list_first(controlslist);
		     element != NULL;
		     element = cfg_list_next(element)) {
			const cfg_obj_t *controls;
			const cfg_obj_t *unixcontrols = NULL;

			controls = cfg_listelt_value(element);
			(void)cfg_map_get(controls, "unix", &unixcontrols);
			if (unixcontrols == NULL)
				continue;

			for (element2 = cfg_list_first(unixcontrols);
			     element2 != NULL;
			     element2 = cfg_list_next(element2)) {
				const cfg_obj_t *control;
				const cfg_obj_t *path;
				isc_sockaddr_t addr;
				isc_result_t result;

				/*
				 * The parser handles BIND 8 configuration file
				 * syntax, so it allows unix phrases as well
				 * inet phrases with no keys{} clause.
				 */
				control = cfg_listelt_value(element2);

				path = cfg_tuple_get(control, "path");
				result = isc_sockaddr_frompath(&addr,
						      cfg_obj_asstring(path));
				if (result != ISC_R_SUCCESS) {
					isc_log_write(ns_g_lctx,
					      NS_LOGCATEGORY_GENERAL,
					      NS_LOGMODULE_CONTROL,
					      ISC_LOG_DEBUG(9),
					      "control channel '%s': %s",
					      cfg_obj_asstring(path),
					      isc_result_totext(result));
					continue;
				}

				isc_log_write(ns_g_lctx,
					      NS_LOGCATEGORY_GENERAL,
					      NS_LOGMODULE_CONTROL,
					      ISC_LOG_DEBUG(9),
					      "processing control channel '%s'",
					      cfg_obj_asstring(path));

				update_listener(cp, &listener, control, config,
						&addr, aclconfctx,
						cfg_obj_asstring(path),
						isc_sockettype_unix);

				if (listener != NULL)
					/*
					 * Remove the listener from the old
					 * list, so it won't be shut down.
					 */
					ISC_LIST_UNLINK(cp->listeners,
							listener, link);
				else
					/*
					 * This is a new listener.
					 */
					add_listener(cp, &listener, control,
						     config, &addr, aclconfctx,
						     cfg_obj_asstring(path),
						     isc_sockettype_unix);

				if (listener != NULL)
					ISC_LIST_APPEND(new_listeners,
							listener, link);
			}
		}
	} else {
		int i;

		for (i = 0; i < 2; i++) {
			isc_sockaddr_t addr;

			if (i == 0) {
				struct in_addr localhost;

				if (isc_net_probeipv4() != ISC_R_SUCCESS)
					continue;
				localhost.s_addr = htonl(INADDR_LOOPBACK);
				isc_sockaddr_fromin(&addr, &localhost, 0);
			} else {
				if (isc_net_probeipv6() != ISC_R_SUCCESS)
					continue;
				isc_sockaddr_fromin6(&addr,
						     &in6addr_loopback, 0);
			}
			isc_sockaddr_setport(&addr, NS_CONTROL_PORT);

			isc_sockaddr_format(&addr, socktext, sizeof(socktext));

			update_listener(cp, &listener, NULL, NULL,
					&addr, NULL, socktext,
					isc_sockettype_tcp);

			if (listener != NULL)
				/*
				 * Remove the listener from the old
				 * list, so it won't be shut down.
				 */
				ISC_LIST_UNLINK(cp->listeners,
						listener, link);
			else
				/*
				 * This is a new listener.
				 */
				add_listener(cp, &listener, NULL, NULL,
					     &addr, NULL, socktext,
					     isc_sockettype_tcp);

			if (listener != NULL)
				ISC_LIST_APPEND(new_listeners,
						listener, link);
		}
	}

	/*
	 * ns_control_shutdown() will stop whatever is on the global
	 * listeners list, which currently only has whatever sockaddrs
	 * were in the previous configuration (if any) that do not
	 * remain in the current configuration.
	 */
	controls_shutdown(cp);

	/*
	 * Put all of the valid listeners on the listeners list.
	 * Anything already on listeners in the process of shutting
	 * down will be taken care of by listen_done().
	 */
	ISC_LIST_APPENDLIST(cp->listeners, new_listeners, link);
	return (ISC_R_SUCCESS);
}

isc_result_t
ns_controls_create(ns_server_t *server, ns_controls_t **ctrlsp) {
	isc_mem_t *mctx = server->mctx;
	isc_result_t result;
	ns_controls_t *controls = isc_mem_get(mctx, sizeof(*controls));

	if (controls == NULL)
		return (ISC_R_NOMEMORY);
	controls->server = server;
	ISC_LIST_INIT(controls->listeners);
	controls->shuttingdown = ISC_FALSE;
	controls->symtab = NULL;
	result = isccc_cc_createsymtab(&controls->symtab);
	if (result != ISC_R_SUCCESS) {
		isc_mem_put(server->mctx, controls, sizeof(*controls));
		return (result);
	}
	*ctrlsp = controls;
	return (ISC_R_SUCCESS);
}

void
ns_controls_destroy(ns_controls_t **ctrlsp) {
	ns_controls_t *controls = *ctrlsp;

	REQUIRE(ISC_LIST_EMPTY(controls->listeners));

	isccc_symtab_destroy(&controls->symtab);
	isc_mem_put(controls->server->mctx, controls, sizeof(*controls));
	*ctrlsp = NULL;
}
