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

/* $Id: lwres_noop.c,v 1.14.206.1 2004/03/06 08:15:33 marka Exp $ */

#include <config.h>

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <lwres/lwbuffer.h>
#include <lwres/lwpacket.h>
#include <lwres/lwres.h>
#include <lwres/result.h>

#include "context_p.h"
#include "assert_p.h"

lwres_result_t
lwres_nooprequest_render(lwres_context_t *ctx, lwres_nooprequest_t *req,
			 lwres_lwpacket_t *pkt, lwres_buffer_t *b)
{
	unsigned char *buf;
	size_t buflen;
	int ret;
	size_t payload_length;

	REQUIRE(ctx != NULL);
	REQUIRE(req != NULL);
	REQUIRE(pkt != NULL);
	REQUIRE(b != NULL);

	payload_length = sizeof(lwres_uint16_t) + req->datalength;

	buflen = LWRES_LWPACKET_LENGTH + payload_length;
	buf = CTXMALLOC(buflen);
	if (buf == NULL)
		return (LWRES_R_NOMEMORY);
	lwres_buffer_init(b, buf, buflen);

	pkt->length = buflen;
	pkt->version = LWRES_LWPACKETVERSION_0;
	pkt->pktflags &= ~LWRES_LWPACKETFLAG_RESPONSE;
	pkt->opcode = LWRES_OPCODE_NOOP;
	pkt->result = 0;
	pkt->authtype = 0;
	pkt->authlength = 0;

	ret = lwres_lwpacket_renderheader(b, pkt);
	if (ret != LWRES_R_SUCCESS) {
		lwres_buffer_invalidate(b);
		CTXFREE(buf, buflen);
		return (ret);
	}

	INSIST(SPACE_OK(b, payload_length));

	/*
	 * Put the length and the data.  We know this will fit because we
	 * just checked for it.
	 */
	lwres_buffer_putuint16(b, req->datalength);
	lwres_buffer_putmem(b, req->data, req->datalength);

	INSIST(LWRES_BUFFER_AVAILABLECOUNT(b) == 0);

	return (LWRES_R_SUCCESS);
}

lwres_result_t
lwres_noopresponse_render(lwres_context_t *ctx, lwres_noopresponse_t *req,
			  lwres_lwpacket_t *pkt, lwres_buffer_t *b)
{
	unsigned char *buf;
	size_t buflen;
	int ret;
	size_t payload_length;

	REQUIRE(ctx != NULL);
	REQUIRE(req != NULL);
	REQUIRE(pkt != NULL);
	REQUIRE(b != NULL);

	payload_length = sizeof(lwres_uint16_t) + req->datalength;

	buflen = LWRES_LWPACKET_LENGTH + payload_length;
	buf = CTXMALLOC(buflen);
	if (buf == NULL)
		return (LWRES_R_NOMEMORY);
	lwres_buffer_init(b, buf, buflen);

	pkt->length = buflen;
	pkt->version = LWRES_LWPACKETVERSION_0;
	pkt->pktflags |= LWRES_LWPACKETFLAG_RESPONSE;
	pkt->opcode = LWRES_OPCODE_NOOP;
	pkt->authtype = 0;
	pkt->authlength = 0;

	ret = lwres_lwpacket_renderheader(b, pkt);
	if (ret != LWRES_R_SUCCESS) {
		lwres_buffer_invalidate(b);
		CTXFREE(buf, buflen);
		return (ret);
	}

	INSIST(SPACE_OK(b, payload_length));

	/*
	 * Put the length and the data.  We know this will fit because we
	 * just checked for it.
	 */
	lwres_buffer_putuint16(b, req->datalength);
	lwres_buffer_putmem(b, req->data, req->datalength);

	INSIST(LWRES_BUFFER_AVAILABLECOUNT(b) == 0);

	return (LWRES_R_SUCCESS);
}

lwres_result_t
lwres_nooprequest_parse(lwres_context_t *ctx, lwres_buffer_t *b,
			lwres_lwpacket_t *pkt, lwres_nooprequest_t **structp)
{
	int ret;
	lwres_nooprequest_t *req;

	REQUIRE(ctx != NULL);
	REQUIRE(b != NULL);
	REQUIRE(pkt != NULL);
	REQUIRE(structp != NULL && *structp == NULL);

	if ((pkt->pktflags & LWRES_LWPACKETFLAG_RESPONSE) != 0)
		return (LWRES_R_FAILURE);

	req = CTXMALLOC(sizeof(lwres_nooprequest_t));
	if (req == NULL)
		return (LWRES_R_NOMEMORY);

	if (!SPACE_REMAINING(b, sizeof(lwres_uint16_t))) {
		ret = LWRES_R_UNEXPECTEDEND;
		goto out;
	}
	req->datalength = lwres_buffer_getuint16(b);

	if (!SPACE_REMAINING(b, req->datalength)) {
		ret = LWRES_R_UNEXPECTEDEND;
		goto out;
	}
	req->data = b->base + b->current;
	lwres_buffer_forward(b, req->datalength);

	if (LWRES_BUFFER_REMAINING(b) != 0) {
		ret = LWRES_R_TRAILINGDATA;
		goto out;
	}

	/* success! */
	*structp = req;
	return (LWRES_R_SUCCESS);

	/* Error return */
 out:
	CTXFREE(req, sizeof(lwres_nooprequest_t));
	return (ret);
}

lwres_result_t
lwres_noopresponse_parse(lwres_context_t *ctx, lwres_buffer_t *b,
			 lwres_lwpacket_t *pkt, lwres_noopresponse_t **structp)
{
	int ret;
	lwres_noopresponse_t *req;

	REQUIRE(ctx != NULL);
	REQUIRE(b != NULL);
	REQUIRE(pkt != NULL);
	REQUIRE(structp != NULL && *structp == NULL);

	if ((pkt->pktflags & LWRES_LWPACKETFLAG_RESPONSE) == 0)
		return (LWRES_R_FAILURE);

	req = CTXMALLOC(sizeof(lwres_noopresponse_t));
	if (req == NULL)
		return (LWRES_R_NOMEMORY);

	if (!SPACE_REMAINING(b, sizeof(lwres_uint16_t))) {
		ret = LWRES_R_UNEXPECTEDEND;
		goto out;
	}
	req->datalength = lwres_buffer_getuint16(b);

	if (!SPACE_REMAINING(b, req->datalength)) {
		ret = LWRES_R_UNEXPECTEDEND;
		goto out;
	}
	req->data = b->base + b->current;

	lwres_buffer_forward(b, req->datalength);
	if (LWRES_BUFFER_REMAINING(b) != 0) {
		ret = LWRES_R_TRAILINGDATA;
		goto out;
	}

	/* success! */
	*structp = req;
	return (LWRES_R_SUCCESS);

	/* Error return */
 out:
	CTXFREE(req, sizeof(lwres_noopresponse_t));
	return (ret);
}

void
lwres_noopresponse_free(lwres_context_t *ctx, lwres_noopresponse_t **structp)
{
	lwres_noopresponse_t *noop;

	REQUIRE(ctx != NULL);
	REQUIRE(structp != NULL && *structp != NULL);

	noop = *structp;
	*structp = NULL;

	CTXFREE(noop, sizeof(lwres_noopresponse_t));
}

void
lwres_nooprequest_free(lwres_context_t *ctx, lwres_nooprequest_t **structp)
{
	lwres_nooprequest_t *noop;

	REQUIRE(ctx != NULL);
	REQUIRE(structp != NULL && *structp != NULL);

	noop = *structp;
	*structp = NULL;

	CTXFREE(noop, sizeof(lwres_nooprequest_t));
}
