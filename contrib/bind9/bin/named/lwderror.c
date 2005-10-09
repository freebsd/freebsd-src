/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000, 2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
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

/* $Id: lwderror.c,v 1.7.208.1 2004/03/06 10:21:18 marka Exp $ */

#include <config.h>

#include <isc/socket.h>
#include <isc/util.h>

#include <named/types.h>
#include <named/lwdclient.h>

/*
 * Generate an error packet for the client, schedule a send, and put us in
 * the SEND state.
 *
 * The client->pkt structure will be modified to form an error return.
 * The receiver needs to verify that it is in fact an error, and do the
 * right thing with it.  The opcode will be unchanged.  The result needs
 * to be set before calling this function.
 *
 * The only change this code makes is to set the receive buffer size to the
 * size we use, set the reply bit, and recompute any security information.
 */
void
ns_lwdclient_errorpktsend(ns_lwdclient_t *client, isc_uint32_t _result) {
	isc_result_t result;
	int lwres;
	isc_region_t r;
	lwres_buffer_t b;

	REQUIRE(NS_LWDCLIENT_ISRUNNING(client));

	/*
	 * Since we are only sending the packet header, we can safely toss
	 * the receive buffer.  This means we won't need to allocate space
	 * for sending an error reply.  This is a Good Thing.
	 */
	client->pkt.length = LWRES_LWPACKET_LENGTH;
	client->pkt.pktflags |= LWRES_LWPACKETFLAG_RESPONSE;
	client->pkt.recvlength = LWRES_RECVLENGTH;
	client->pkt.authtype = 0; /* XXXMLG */
	client->pkt.authlength = 0;
	client->pkt.result = _result;

	lwres_buffer_init(&b, client->buffer, LWRES_RECVLENGTH);
	lwres = lwres_lwpacket_renderheader(&b, &client->pkt);
	if (lwres != LWRES_R_SUCCESS) {
		ns_lwdclient_stateidle(client);
		return;
	}

	r.base = client->buffer;
	r.length = b.used;
	client->sendbuf = client->buffer;
	result = ns_lwdclient_sendreply(client, &r);
	if (result != ISC_R_SUCCESS) {
		ns_lwdclient_stateidle(client);
		return;
	}

	NS_LWDCLIENT_SETSEND(client);
}
