/*
 * Copyright (C) 2004, 2005, 2007  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000, 2001  Internet Software Consortium.
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

/* $Id: lwdclient.c,v 1.22 2007-06-18 23:47:18 tbox Exp $ */

/*! \file */

#include <config.h>

#include <isc/socket.h>
#include <isc/string.h>
#include <isc/task.h>
#include <isc/util.h>

#include <dns/adb.h>
#include <dns/view.h>
#include <dns/log.h>

#include <named/types.h>
#include <named/log.h>
#include <named/lwresd.h>
#include <named/lwdclient.h>

#define SHUTTINGDOWN(cm) ((cm->flags & NS_LWDCLIENTMGR_FLAGSHUTTINGDOWN) != 0)

static void
lwdclientmgr_shutdown_callback(isc_task_t *task, isc_event_t *ev);

void
ns_lwdclient_log(int level, const char *format, ...) {
	va_list args;

	va_start(args, format);
	isc_log_vwrite(dns_lctx,
		       DNS_LOGCATEGORY_DATABASE, DNS_LOGMODULE_ADB,
		       ISC_LOG_DEBUG(level), format, args);
	va_end(args);
}

isc_result_t
ns_lwdclientmgr_create(ns_lwreslistener_t *listener, unsigned int nclients,
		    isc_taskmgr_t *taskmgr)
{
	ns_lwresd_t *lwresd = listener->manager;
	ns_lwdclientmgr_t *cm;
	ns_lwdclient_t *client;
	unsigned int i;
	isc_result_t result = ISC_R_FAILURE;

	cm = isc_mem_get(lwresd->mctx, sizeof(ns_lwdclientmgr_t));
	if (cm == NULL)
		return (ISC_R_NOMEMORY);

	cm->listener = NULL;
	ns_lwreslistener_attach(listener, &cm->listener);
	cm->mctx = lwresd->mctx;
	cm->sock = NULL;
	isc_socket_attach(listener->sock, &cm->sock);
	cm->view = lwresd->view;
	cm->lwctx = NULL;
	cm->task = NULL;
	cm->flags = 0;
	ISC_LINK_INIT(cm, link);
	ISC_LIST_INIT(cm->idle);
	ISC_LIST_INIT(cm->running);

	if (lwres_context_create(&cm->lwctx, cm->mctx,
				 ns__lwresd_memalloc, ns__lwresd_memfree,
				 LWRES_CONTEXT_SERVERMODE)
	    != ISC_R_SUCCESS)
		goto errout;

	for (i = 0; i < nclients; i++) {
		client = isc_mem_get(lwresd->mctx, sizeof(ns_lwdclient_t));
		if (client != NULL) {
			ns_lwdclient_log(50, "created client %p, manager %p",
					 client, cm);
			ns_lwdclient_initialize(client, cm);
		}
	}

	/*
	 * If we could create no clients, clean up and return.
	 */
	if (ISC_LIST_EMPTY(cm->idle))
		goto errout;

	result = isc_task_create(taskmgr, 0, &cm->task);
	if (result != ISC_R_SUCCESS)
		goto errout;
	isc_task_setname(cm->task, "lwdclient", NULL);

	/*
	 * This MUST be last, since there is no way to cancel an onshutdown...
	 */
	result = isc_task_onshutdown(cm->task, lwdclientmgr_shutdown_callback,
				     cm);
	if (result != ISC_R_SUCCESS)
		goto errout;

	ns_lwreslistener_linkcm(listener, cm);

	return (ISC_R_SUCCESS);

 errout:
	client = ISC_LIST_HEAD(cm->idle);
	while (client != NULL) {
		ISC_LIST_UNLINK(cm->idle, client, link);
		isc_mem_put(lwresd->mctx, client, sizeof(*client));
		client = ISC_LIST_HEAD(cm->idle);
	}

	if (cm->task != NULL)
		isc_task_detach(&cm->task);

	if (cm->lwctx != NULL)
		lwres_context_destroy(&cm->lwctx);

	isc_mem_put(lwresd->mctx, cm, sizeof(*cm));
	return (result);
}

static void
lwdclientmgr_destroy(ns_lwdclientmgr_t *cm) {
	ns_lwdclient_t *client;
	ns_lwreslistener_t *listener;

	if (!SHUTTINGDOWN(cm))
		return;

	/*
	 * run through the idle list and free the clients there.  Idle
	 * clients do not have a recv running nor do they have any finds
	 * or similar running.
	 */
	client = ISC_LIST_HEAD(cm->idle);
	while (client != NULL) {
		ns_lwdclient_log(50, "destroying client %p, manager %p",
				 client, cm);
		ISC_LIST_UNLINK(cm->idle, client, link);
		isc_mem_put(cm->mctx, client, sizeof(*client));
		client = ISC_LIST_HEAD(cm->idle);
	}

	if (!ISC_LIST_EMPTY(cm->running))
		return;

	lwres_context_destroy(&cm->lwctx);
	cm->view = NULL;
	isc_socket_detach(&cm->sock);
	isc_task_detach(&cm->task);

	listener = cm->listener;
	ns_lwreslistener_unlinkcm(listener, cm);
	ns_lwdclient_log(50, "destroying manager %p", cm);
	isc_mem_put(cm->mctx, cm, sizeof(*cm));
	ns_lwreslistener_detach(&listener);
}

static void
process_request(ns_lwdclient_t *client) {
	lwres_buffer_t b;
	isc_result_t result;

	lwres_buffer_init(&b, client->buffer, client->recvlength);
	lwres_buffer_add(&b, client->recvlength);

	result = lwres_lwpacket_parseheader(&b, &client->pkt);
	if (result != ISC_R_SUCCESS) {
		ns_lwdclient_log(50, "invalid packet header received");
		goto restart;
	}

	ns_lwdclient_log(50, "opcode %08x", client->pkt.opcode);

	switch (client->pkt.opcode) {
	case LWRES_OPCODE_GETADDRSBYNAME:
		ns_lwdclient_processgabn(client, &b);
		return;
	case LWRES_OPCODE_GETNAMEBYADDR:
		ns_lwdclient_processgnba(client, &b);
		return;
	case LWRES_OPCODE_GETRDATABYNAME:
		ns_lwdclient_processgrbn(client, &b);
		return;
	case LWRES_OPCODE_NOOP:
		ns_lwdclient_processnoop(client, &b);
		return;
	default:
		ns_lwdclient_log(50, "unknown opcode %08x", client->pkt.opcode);
		goto restart;
	}

	/*
	 * Drop the packet.
	 */
 restart:
	ns_lwdclient_log(50, "restarting client %p...", client);
	ns_lwdclient_stateidle(client);
}

void
ns_lwdclient_recv(isc_task_t *task, isc_event_t *ev) {
	isc_result_t result;
	ns_lwdclient_t *client = ev->ev_arg;
	ns_lwdclientmgr_t *cm = client->clientmgr;
	isc_socketevent_t *dev = (isc_socketevent_t *)ev;

	INSIST(dev->region.base == client->buffer);
	INSIST(NS_LWDCLIENT_ISRECV(client));

	NS_LWDCLIENT_SETRECVDONE(client);

	INSIST((cm->flags & NS_LWDCLIENTMGR_FLAGRECVPENDING) != 0);
	cm->flags &= ~NS_LWDCLIENTMGR_FLAGRECVPENDING;

	ns_lwdclient_log(50,
			 "event received: task %p, length %u, result %u (%s)",
			 task, dev->n, dev->result,
			 isc_result_totext(dev->result));

	if (dev->result != ISC_R_SUCCESS) {
		isc_event_free(&ev);
		dev = NULL;

		/*
		 * Go idle.
		 */
		ns_lwdclient_stateidle(client);

		return;
	}

	client->recvlength = dev->n;
	client->address = dev->address;
	if ((dev->attributes & ISC_SOCKEVENTATTR_PKTINFO) != 0) {
		client->pktinfo = dev->pktinfo;
		client->pktinfo_valid = ISC_TRUE;
	} else
		client->pktinfo_valid = ISC_FALSE;
	isc_event_free(&ev);
	dev = NULL;

	result = ns_lwdclient_startrecv(cm);
	if (result != ISC_R_SUCCESS)
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_LWRESD, ISC_LOG_ERROR,
			      "could not start lwres "
			      "client handler: %s",
			      isc_result_totext(result));

	process_request(client);
}

/*
 * This function will start a new recv() on a socket for this client manager.
 */
isc_result_t
ns_lwdclient_startrecv(ns_lwdclientmgr_t *cm) {
	ns_lwdclient_t *client;
	isc_result_t result;
	isc_region_t r;

	if (SHUTTINGDOWN(cm)) {
		lwdclientmgr_destroy(cm);
		return (ISC_R_SUCCESS);
	}

	/*
	 * If a recv is already running, don't bother.
	 */
	if ((cm->flags & NS_LWDCLIENTMGR_FLAGRECVPENDING) != 0)
		return (ISC_R_SUCCESS);

	/*
	 * If we have no idle slots, just return success.
	 */
	client = ISC_LIST_HEAD(cm->idle);
	if (client == NULL)
		return (ISC_R_SUCCESS);
	INSIST(NS_LWDCLIENT_ISIDLE(client));

	/*
	 * Issue the recv.  If it fails, return that it did.
	 */
	r.base = client->buffer;
	r.length = LWRES_RECVLENGTH;
	result = isc_socket_recv(cm->sock, &r, 0, cm->task, ns_lwdclient_recv,
				 client);
	if (result != ISC_R_SUCCESS)
		return (result);

	/*
	 * Set the flag to say we've issued a recv() call.
	 */
	cm->flags |= NS_LWDCLIENTMGR_FLAGRECVPENDING;

	/*
	 * Remove the client from the idle list, and put it on the running
	 * list.
	 */
	NS_LWDCLIENT_SETRECV(client);
	ISC_LIST_UNLINK(cm->idle, client, link);
	ISC_LIST_APPEND(cm->running, client, link);

	return (ISC_R_SUCCESS);
}

static void
lwdclientmgr_shutdown_callback(isc_task_t *task, isc_event_t *ev) {
	ns_lwdclientmgr_t *cm = ev->ev_arg;
	ns_lwdclient_t *client;

	REQUIRE(!SHUTTINGDOWN(cm));

	ns_lwdclient_log(50, "got shutdown event, task %p, lwdclientmgr %p",
			 task, cm);

	/*
	 * run through the idle list and free the clients there.  Idle
	 * clients do not have a recv running nor do they have any finds
	 * or similar running.
	 */
	client = ISC_LIST_HEAD(cm->idle);
	while (client != NULL) {
		ns_lwdclient_log(50, "destroying client %p, manager %p",
				 client, cm);
		ISC_LIST_UNLINK(cm->idle, client, link);
		isc_mem_put(cm->mctx, client, sizeof(*client));
		client = ISC_LIST_HEAD(cm->idle);
	}

	/*
	 * Cancel any pending I/O.
	 */
	isc_socket_cancel(cm->sock, task, ISC_SOCKCANCEL_ALL);

	/*
	 * Run through the running client list and kill off any finds
	 * in progress.
	 */
	client = ISC_LIST_HEAD(cm->running);
	while (client != NULL) {
		if (client->find != client->v4find
		    && client->find != client->v6find)
			dns_adb_cancelfind(client->find);
		if (client->v4find != NULL)
			dns_adb_cancelfind(client->v4find);
		if (client->v6find != NULL)
			dns_adb_cancelfind(client->v6find);
		client = ISC_LIST_NEXT(client, link);
	}

	cm->flags |= NS_LWDCLIENTMGR_FLAGSHUTTINGDOWN;

	isc_event_free(&ev);
}

/*
 * Do all the crap needed to move a client from the run queue to the idle
 * queue.
 */
void
ns_lwdclient_stateidle(ns_lwdclient_t *client) {
	ns_lwdclientmgr_t *cm;
	isc_result_t result;

	cm = client->clientmgr;

	INSIST(client->sendbuf == NULL);
	INSIST(client->sendlength == 0);
	INSIST(client->arg == NULL);
	INSIST(client->v4find == NULL);
	INSIST(client->v6find == NULL);

	ISC_LIST_UNLINK(cm->running, client, link);
	ISC_LIST_PREPEND(cm->idle, client, link);

	NS_LWDCLIENT_SETIDLE(client);

	result = ns_lwdclient_startrecv(cm);
	if (result != ISC_R_SUCCESS)
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_LWRESD, ISC_LOG_ERROR,
			      "could not start lwres "
			      "client handler: %s",
			      isc_result_totext(result));
}

void
ns_lwdclient_send(isc_task_t *task, isc_event_t *ev) {
	ns_lwdclient_t *client = ev->ev_arg;
	ns_lwdclientmgr_t *cm = client->clientmgr;
	isc_socketevent_t *dev = (isc_socketevent_t *)ev;

	UNUSED(task);
	UNUSED(dev);

	INSIST(NS_LWDCLIENT_ISSEND(client));
	INSIST(client->sendbuf == dev->region.base);

	ns_lwdclient_log(50, "task %p for client %p got send-done event",
			 task, client);

	if (client->sendbuf != client->buffer)
		lwres_context_freemem(cm->lwctx, client->sendbuf,
				      client->sendlength);
	client->sendbuf = NULL;
	client->sendlength = 0;

	ns_lwdclient_stateidle(client);

	isc_event_free(&ev);
}

isc_result_t
ns_lwdclient_sendreply(ns_lwdclient_t *client, isc_region_t *r) {
	struct in6_pktinfo *pktinfo;
	ns_lwdclientmgr_t *cm = client->clientmgr;

	if (client->pktinfo_valid)
		pktinfo = &client->pktinfo;
	else
		pktinfo = NULL;
	return (isc_socket_sendto(cm->sock, r, cm->task, ns_lwdclient_send,
				  client, &client->address, pktinfo));
}

void
ns_lwdclient_initialize(ns_lwdclient_t *client, ns_lwdclientmgr_t *cmgr) {
	client->clientmgr = cmgr;
	ISC_LINK_INIT(client, link);
	NS_LWDCLIENT_SETIDLE(client);
	client->arg = NULL;

	client->recvlength = 0;

	client->sendbuf = NULL;
	client->sendlength = 0;

	client->find = NULL;
	client->v4find = NULL;
	client->v6find = NULL;
	client->find_wanted = 0;

	client->options = 0;
	client->byaddr = NULL;

	client->lookup = NULL;

	client->pktinfo_valid = ISC_FALSE;

	ISC_LIST_APPEND(cmgr->idle, client, link);
}
