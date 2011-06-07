/*
 * Copyright (C) 2004, 2005, 2007, 2008  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000-2002  Internet Software Consortium.
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

/* $Id: lwdgnba.c,v 1.22 2008-01-14 23:46:56 tbox Exp $ */

/*! \file */

#include <config.h>

#include <isc/socket.h>
#include <isc/string.h>		/* Required for HP/UX (and others?) */
#include <isc/util.h>

#include <dns/adb.h>
#include <dns/byaddr.h>
#include <dns/result.h>

#include <named/types.h>
#include <named/lwdclient.h>

static void start_byaddr(ns_lwdclient_t *);

static void
byaddr_done(isc_task_t *task, isc_event_t *event) {
	ns_lwdclient_t *client;
	ns_lwdclientmgr_t *cm;
	dns_byaddrevent_t *bevent;
	int lwres;
	lwres_buffer_t lwb;
	dns_name_t *name;
	isc_result_t result;
	lwres_result_t lwresult;
	isc_region_t r;
	isc_buffer_t b;
	lwres_gnbaresponse_t *gnba;
	isc_uint16_t naliases;

	UNUSED(task);

	lwb.base = NULL;
	client = event->ev_arg;
	cm = client->clientmgr;
	INSIST(client->byaddr == (dns_byaddr_t *)event->ev_sender);

	bevent = (dns_byaddrevent_t *)event;
	gnba = &client->gnba;

	ns_lwdclient_log(50, "byaddr event result = %s",
			 isc_result_totext(bevent->result));

	result = bevent->result;
	if (result != ISC_R_SUCCESS) {
		dns_byaddr_destroy(&client->byaddr);
		isc_event_free(&event);
		bevent = NULL;

		if (client->na.family != AF_INET6 ||
		    (client->options & DNS_BYADDROPT_IPV6INT) != 0) {
			if (result == DNS_R_NCACHENXDOMAIN ||
			    result == DNS_R_NCACHENXRRSET ||
			    result == DNS_R_NXDOMAIN ||
			    result == DNS_R_NXRRSET)
				lwresult = LWRES_R_NOTFOUND;
			else
				lwresult = LWRES_R_FAILURE;
			ns_lwdclient_errorpktsend(client, lwresult);
			return;
		}

		/*
		 * Fall back to ip6.int reverse if the default ip6.arpa
		 * fails.
		 */
		client->options |= DNS_BYADDROPT_IPV6INT;

		start_byaddr(client);
		return;
	}

	for (name = ISC_LIST_HEAD(bevent->names);
	     name != NULL;
	     name = ISC_LIST_NEXT(name, link))
	{
		b = client->recv_buffer;

		result = dns_name_totext(name, ISC_TRUE, &client->recv_buffer);
		if (result != ISC_R_SUCCESS)
			goto out;
		ns_lwdclient_log(50, "found name '%.*s'",
				 (int)(client->recv_buffer.used - b.used),
				 (char *)(b.base) + b.used);
		if (gnba->realname == NULL) {
			gnba->realname = (char *)(b.base) + b.used;
			gnba->realnamelen = client->recv_buffer.used - b.used;
		} else {
			naliases = gnba->naliases;
			if (naliases >= LWRES_MAX_ALIASES)
				break;
			gnba->aliases[naliases] = (char *)(b.base) + b.used;
			gnba->aliaslen[naliases] =
				client->recv_buffer.used - b.used;
			gnba->naliases++;
		}
	}

	dns_byaddr_destroy(&client->byaddr);
	isc_event_free(&event);

	/*
	 * Render the packet.
	 */
	client->pkt.recvlength = LWRES_RECVLENGTH;
	client->pkt.authtype = 0; /* XXXMLG */
	client->pkt.authlength = 0;
	client->pkt.result = LWRES_R_SUCCESS;

	lwres = lwres_gnbaresponse_render(cm->lwctx,
					  gnba, &client->pkt, &lwb);
	if (lwres != LWRES_R_SUCCESS)
		goto out;

	r.base = lwb.base;
	r.length = lwb.used;
	client->sendbuf = r.base;
	client->sendlength = r.length;
	result = ns_lwdclient_sendreply(client, &r);
	if (result != ISC_R_SUCCESS)
		goto out;

	NS_LWDCLIENT_SETSEND(client);

	return;

 out:
	if (client->byaddr != NULL)
		dns_byaddr_destroy(&client->byaddr);
	if (lwb.base != NULL)
		lwres_context_freemem(cm->lwctx,
				      lwb.base, lwb.length);

	if (event != NULL)
		isc_event_free(&event);
}

static void
start_byaddr(ns_lwdclient_t *client) {
	isc_result_t result;
	ns_lwdclientmgr_t *cm;

	cm = client->clientmgr;

	INSIST(client->byaddr == NULL);

	result = dns_byaddr_create(cm->mctx, &client->na, cm->view,
				   client->options, cm->task, byaddr_done,
				   client, &client->byaddr);
	if (result != ISC_R_SUCCESS) {
		ns_lwdclient_errorpktsend(client, LWRES_R_FAILURE);
		return;
	}
}

static void
init_gnba(ns_lwdclient_t *client) {
	int i;

	/*
	 * Initialize the real name and alias arrays in the reply we're
	 * going to build up.
	 */
	for (i = 0; i < LWRES_MAX_ALIASES; i++) {
		client->aliases[i] = NULL;
		client->aliaslen[i] = 0;
	}
	for (i = 0; i < LWRES_MAX_ADDRS; i++) {
		client->addrs[i].family = 0;
		client->addrs[i].length = 0;
		memset(client->addrs[i].address, 0, LWRES_ADDR_MAXLEN);
		LWRES_LINK_INIT(&client->addrs[i], link);
	}

	client->gnba.naliases = 0;
	client->gnba.realname = NULL;
	client->gnba.aliases = client->aliases;
	client->gnba.realnamelen = 0;
	client->gnba.aliaslen = client->aliaslen;
	client->gnba.base = NULL;
	client->gnba.baselen = 0;
	isc_buffer_init(&client->recv_buffer, client->buffer, LWRES_RECVLENGTH);
}

void
ns_lwdclient_processgnba(ns_lwdclient_t *client, lwres_buffer_t *b) {
	lwres_gnbarequest_t *req;
	isc_result_t result;
	isc_sockaddr_t sa;
	ns_lwdclientmgr_t *cm;

	REQUIRE(NS_LWDCLIENT_ISRECVDONE(client));
	INSIST(client->byaddr == NULL);

	cm = client->clientmgr;
	req = NULL;

	result = lwres_gnbarequest_parse(cm->lwctx,
					 b, &client->pkt, &req);
	if (result != LWRES_R_SUCCESS)
		goto out;

	client->options = 0;
	if (req->addr.family == LWRES_ADDRTYPE_V4) {
		client->na.family = AF_INET;
		if (req->addr.length != 4)
			goto out;
		memcpy(&client->na.type.in, req->addr.address, 4);
	} else if (req->addr.family == LWRES_ADDRTYPE_V6) {
		client->na.family = AF_INET6;
		if (req->addr.length != 16)
			goto out;
		memcpy(&client->na.type.in6, req->addr.address, 16);
	} else {
		goto out;
	}
	isc_sockaddr_fromnetaddr(&sa, &client->na, 53);

	ns_lwdclient_log(50, "client %p looking for addrtype %08x",
			 client, req->addr.family);

	/*
	 * We no longer need to keep this around.
	 */
	lwres_gnbarequest_free(cm->lwctx, &req);

	/*
	 * Initialize the real name and alias arrays in the reply we're
	 * going to build up.
	 */
	init_gnba(client);
	client->options = 0;

	/*
	 * Start the find.
	 */
	start_byaddr(client);

	return;

	/*
	 * We're screwed.  Return an error packet to our caller.
	 */
 out:
	if (req != NULL)
		lwres_gnbarequest_free(cm->lwctx, &req);

	ns_lwdclient_errorpktsend(client, LWRES_R_FAILURE);
}
