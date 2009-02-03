/*
 * Copyright (C) 2004, 2005, 2008  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: lwdnoop.c,v 1.7.18.4 2008/01/22 23:27:05 tbox Exp $ */

/*! \file */

#include <config.h>

#include <isc/socket.h>
#include <isc/util.h>

#include <named/types.h>
#include <named/lwdclient.h>

void
ns_lwdclient_processnoop(ns_lwdclient_t *client, lwres_buffer_t *b) {
	lwres_nooprequest_t *req;
	lwres_noopresponse_t resp;
	isc_result_t result;
	lwres_result_t lwres;
	isc_region_t r;
	lwres_buffer_t lwb;

	REQUIRE(NS_LWDCLIENT_ISRECVDONE(client));
	INSIST(client->byaddr == NULL);

	req = NULL;

	result = lwres_nooprequest_parse(client->clientmgr->lwctx,
					 b, &client->pkt, &req);
	if (result != LWRES_R_SUCCESS)
		goto send_error;

	client->pkt.recvlength = LWRES_RECVLENGTH;
	client->pkt.authtype = 0; /* XXXMLG */
	client->pkt.authlength = 0;
	client->pkt.result = LWRES_R_SUCCESS;

	resp.datalength = req->datalength;
	resp.data = req->data;

	lwres = lwres_noopresponse_render(client->clientmgr->lwctx, &resp,
					  &client->pkt, &lwb);
	if (lwres != LWRES_R_SUCCESS)
		goto cleanup_req;

	r.base = lwb.base;
	r.length = lwb.used;
	client->sendbuf = r.base;
	client->sendlength = r.length;
	result = ns_lwdclient_sendreply(client, &r);
	if (result != ISC_R_SUCCESS)
		goto cleanup_lwb;

	/*
	 * We can now destroy request.
	 */
	lwres_nooprequest_free(client->clientmgr->lwctx, &req);

	NS_LWDCLIENT_SETSEND(client);

	return;

 cleanup_lwb:
	lwres_context_freemem(client->clientmgr->lwctx, lwb.base, lwb.length);

 cleanup_req:
	lwres_nooprequest_free(client->clientmgr->lwctx, &req);

 send_error:
	ns_lwdclient_errorpktsend(client, LWRES_R_FAILURE);
}
