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

/* $Id: lwres_gabn.c,v 1.27.12.3 2004/03/08 09:05:10 marka Exp $ */

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
lwres_gabnrequest_render(lwres_context_t *ctx, lwres_gabnrequest_t *req,
			 lwres_lwpacket_t *pkt, lwres_buffer_t *b)
{
	unsigned char *buf;
	size_t buflen;
	int ret;
	size_t payload_length;
	lwres_uint16_t datalen;

	REQUIRE(ctx != NULL);
	REQUIRE(req != NULL);
	REQUIRE(req->name != NULL);
	REQUIRE(pkt != NULL);
	REQUIRE(b != NULL);

	datalen = strlen(req->name);

	payload_length = 4 + 4 + 2 + req->namelen + 1;

	buflen = LWRES_LWPACKET_LENGTH + payload_length;
	buf = CTXMALLOC(buflen);
	if (buf == NULL)
		return (LWRES_R_NOMEMORY);

	lwres_buffer_init(b, buf, buflen);

	pkt->length = buflen;
	pkt->version = LWRES_LWPACKETVERSION_0;
	pkt->pktflags &= ~LWRES_LWPACKETFLAG_RESPONSE;
	pkt->opcode = LWRES_OPCODE_GETADDRSBYNAME;
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
	 * Flags.
	 */
	lwres_buffer_putuint32(b, req->flags);

	/*
	 * Address types we'll accept.
	 */
	lwres_buffer_putuint32(b, req->addrtypes);

	/*
	 * Put the length and the data.  We know this will fit because we
	 * just checked for it.
	 */
	lwres_buffer_putuint16(b, datalen);
	lwres_buffer_putmem(b, (unsigned char *)req->name, datalen);
	lwres_buffer_putuint8(b, 0); /* trailing NUL */

	INSIST(LWRES_BUFFER_AVAILABLECOUNT(b) == 0);

	return (LWRES_R_SUCCESS);
}

lwres_result_t
lwres_gabnresponse_render(lwres_context_t *ctx, lwres_gabnresponse_t *req,
			  lwres_lwpacket_t *pkt, lwres_buffer_t *b)
{
	unsigned char *buf;
	size_t buflen;
	int ret;
	size_t payload_length;
	lwres_uint16_t datalen;
	lwres_addr_t *addr;
	int x;

	REQUIRE(ctx != NULL);
	REQUIRE(req != NULL);
	REQUIRE(pkt != NULL);
	REQUIRE(b != NULL);

	/* naliases, naddrs */
	payload_length = 4 + 2 + 2;
	/* real name encoding */
	payload_length += 2 + req->realnamelen + 1;
	/* each alias */
	for (x = 0; x < req->naliases; x++)
		payload_length += 2 + req->aliaslen[x] + 1;
	/* each address */
	x = 0;
	addr = LWRES_LIST_HEAD(req->addrs);
	while (addr != NULL) {
		payload_length += 4 + 2;
		payload_length += addr->length;
		addr = LWRES_LIST_NEXT(addr, link);
		x++;
	}
	INSIST(x == req->naddrs);

	buflen = LWRES_LWPACKET_LENGTH + payload_length;
	buf = CTXMALLOC(buflen);
	if (buf == NULL)
		return (LWRES_R_NOMEMORY);
	lwres_buffer_init(b, buf, buflen);

	pkt->length = buflen;
	pkt->version = LWRES_LWPACKETVERSION_0;
	pkt->pktflags |= LWRES_LWPACKETFLAG_RESPONSE;
	pkt->opcode = LWRES_OPCODE_GETADDRSBYNAME;
	pkt->authtype = 0;
	pkt->authlength = 0;

	ret = lwres_lwpacket_renderheader(b, pkt);
	if (ret != LWRES_R_SUCCESS) {
		lwres_buffer_invalidate(b);
		CTXFREE(buf, buflen);
		return (ret);
	}

	/*
	 * Check space needed here.
	 */
	INSIST(SPACE_OK(b, payload_length));

	/* Flags. */
	lwres_buffer_putuint32(b, req->flags);

	/* encode naliases and naddrs */
	lwres_buffer_putuint16(b, req->naliases);
	lwres_buffer_putuint16(b, req->naddrs);

	/* encode the real name */
	datalen = req->realnamelen;
	lwres_buffer_putuint16(b, datalen);
	lwres_buffer_putmem(b, (unsigned char *)req->realname, datalen);
	lwres_buffer_putuint8(b, 0);

	/* encode the aliases */
	for (x = 0; x < req->naliases; x++) {
		datalen = req->aliaslen[x];
		lwres_buffer_putuint16(b, datalen);
		lwres_buffer_putmem(b, (unsigned char *)req->aliases[x],
				    datalen);
		lwres_buffer_putuint8(b, 0);
	}

	/* encode the addresses */
	addr = LWRES_LIST_HEAD(req->addrs);
	while (addr != NULL) {
		lwres_buffer_putuint32(b, addr->family);
		lwres_buffer_putuint16(b, addr->length);
		lwres_buffer_putmem(b, addr->address, addr->length);
		addr = LWRES_LIST_NEXT(addr, link);
	}

	INSIST(LWRES_BUFFER_AVAILABLECOUNT(b) == 0);
	INSIST(LWRES_BUFFER_USEDCOUNT(b) == pkt->length);

	return (LWRES_R_SUCCESS);
}

lwres_result_t
lwres_gabnrequest_parse(lwres_context_t *ctx, lwres_buffer_t *b,
			lwres_lwpacket_t *pkt, lwres_gabnrequest_t **structp)
{
	int ret;
	char *name;
	lwres_gabnrequest_t *gabn;
	lwres_uint32_t addrtypes;
	lwres_uint32_t flags;
	lwres_uint16_t namelen;

	REQUIRE(ctx != NULL);
	REQUIRE(pkt != NULL);
	REQUIRE(b != NULL);
	REQUIRE(structp != NULL && *structp == NULL);

	if ((pkt->pktflags & LWRES_LWPACKETFLAG_RESPONSE) != 0)
		return (LWRES_R_FAILURE);

	if (!SPACE_REMAINING(b, 4 + 4))
		return (LWRES_R_UNEXPECTEDEND);

	flags = lwres_buffer_getuint32(b);
	addrtypes = lwres_buffer_getuint32(b);

	/*
	 * Pull off the name itself
	 */
	ret = lwres_string_parse(b, &name, &namelen);
	if (ret != LWRES_R_SUCCESS)
		return (ret);

	if (LWRES_BUFFER_REMAINING(b) != 0)
		return (LWRES_R_TRAILINGDATA);

	gabn = CTXMALLOC(sizeof(lwres_gabnrequest_t));
	if (gabn == NULL)
		return (LWRES_R_NOMEMORY);

	gabn->flags = flags;
	gabn->addrtypes = addrtypes;
	gabn->name = name;
	gabn->namelen = namelen;

	*structp = gabn;
	return (LWRES_R_SUCCESS);
}

lwres_result_t
lwres_gabnresponse_parse(lwres_context_t *ctx, lwres_buffer_t *b,
			lwres_lwpacket_t *pkt, lwres_gabnresponse_t **structp)
{
	lwres_result_t ret;
	unsigned int x;
	lwres_uint32_t flags;
	lwres_uint16_t naliases;
	lwres_uint16_t naddrs;
	lwres_gabnresponse_t *gabn;
	lwres_addrlist_t addrlist;
	lwres_addr_t *addr;

	REQUIRE(ctx != NULL);
	REQUIRE(pkt != NULL);
	REQUIRE(b != NULL);
	REQUIRE(structp != NULL && *structp == NULL);

	gabn = NULL;

	if ((pkt->pktflags & LWRES_LWPACKETFLAG_RESPONSE) == 0)
		return (LWRES_R_FAILURE);

	/*
	 * Pull off the name itself
	 */
	if (!SPACE_REMAINING(b, 4 + 2 + 2))
		return (LWRES_R_UNEXPECTEDEND);
	flags = lwres_buffer_getuint32(b);
	naliases = lwres_buffer_getuint16(b);
	naddrs = lwres_buffer_getuint16(b);

	gabn = CTXMALLOC(sizeof(lwres_gabnresponse_t));
	if (gabn == NULL)
		return (LWRES_R_NOMEMORY);
	gabn->aliases = NULL;
	gabn->aliaslen = NULL;
	LWRES_LIST_INIT(gabn->addrs);
	gabn->base = NULL;

	gabn->flags = flags;
	gabn->naliases = naliases;
	gabn->naddrs = naddrs;

	LWRES_LIST_INIT(addrlist);

	if (naliases > 0) {
		gabn->aliases = CTXMALLOC(sizeof(char *) * naliases);
		if (gabn->aliases == NULL) {
			ret = LWRES_R_NOMEMORY;
			goto out;
		}

		gabn->aliaslen = CTXMALLOC(sizeof(lwres_uint16_t) * naliases);
		if (gabn->aliaslen == NULL) {
			ret = LWRES_R_NOMEMORY;
			goto out;
		}
	}

	for (x = 0; x < naddrs; x++) {
		addr = CTXMALLOC(sizeof(lwres_addr_t));
		if (addr == NULL) {
			ret = LWRES_R_NOMEMORY;
			goto out;
		}
		LWRES_LINK_INIT(addr, link);
		LWRES_LIST_APPEND(addrlist, addr, link);
	}

	/*
	 * Now, pull off the real name.
	 */
	ret = lwres_string_parse(b, &gabn->realname, &gabn->realnamelen);
	if (ret != LWRES_R_SUCCESS)
		goto out;

	/*
	 * Parse off the aliases.
	 */
	for (x = 0; x < gabn->naliases; x++) {
		ret = lwres_string_parse(b, &gabn->aliases[x],
					 &gabn->aliaslen[x]);
		if (ret != LWRES_R_SUCCESS)
			goto out;
	}

	/*
	 * Pull off the addresses.  We already strung the linked list
	 * up above.
	 */
	addr = LWRES_LIST_HEAD(addrlist);
	for (x = 0; x < gabn->naddrs; x++) {
		INSIST(addr != NULL);
		ret = lwres_addr_parse(b, addr);
		if (ret != LWRES_R_SUCCESS)
			goto out;
		addr = LWRES_LIST_NEXT(addr, link);
	}

	if (LWRES_BUFFER_REMAINING(b) != 0) {
		ret = LWRES_R_TRAILINGDATA;
		goto out;
	}

	gabn->addrs = addrlist;

	*structp = gabn;
	return (LWRES_R_SUCCESS);

 out:
	if (gabn != NULL) {
		if (gabn->aliases != NULL)
			CTXFREE(gabn->aliases, sizeof(char *) * naliases);
		if (gabn->aliaslen != NULL)
			CTXFREE(gabn->aliaslen,
				sizeof(lwres_uint16_t) * naliases);
		addr = LWRES_LIST_HEAD(addrlist);
		while (addr != NULL) {
			LWRES_LIST_UNLINK(addrlist, addr, link);
			CTXFREE(addr, sizeof(lwres_addr_t));
			addr = LWRES_LIST_HEAD(addrlist);
		}
		CTXFREE(gabn, sizeof(lwres_gabnresponse_t));
	}

	return (ret);
}

void
lwres_gabnrequest_free(lwres_context_t *ctx, lwres_gabnrequest_t **structp)
{
	lwres_gabnrequest_t *gabn;

	REQUIRE(ctx != NULL);
	REQUIRE(structp != NULL && *structp != NULL);

	gabn = *structp;
	*structp = NULL;

	CTXFREE(gabn, sizeof(lwres_gabnrequest_t));
}

void
lwres_gabnresponse_free(lwres_context_t *ctx, lwres_gabnresponse_t **structp)
{
	lwres_gabnresponse_t *gabn;
	lwres_addr_t *addr;

	REQUIRE(ctx != NULL);
	REQUIRE(structp != NULL && *structp != NULL);

	gabn = *structp;
	*structp = NULL;

	if (gabn->naliases > 0) {
		CTXFREE(gabn->aliases, sizeof(char *) * gabn->naliases);
		CTXFREE(gabn->aliaslen,
			sizeof(lwres_uint16_t) * gabn->naliases);
	}
	addr = LWRES_LIST_HEAD(gabn->addrs);
	while (addr != NULL) {
		LWRES_LIST_UNLINK(gabn->addrs, addr, link);
		CTXFREE(addr, sizeof(lwres_addr_t));
		addr = LWRES_LIST_HEAD(gabn->addrs);
	}
	if (gabn->base != NULL)
		CTXFREE(gabn->base, gabn->baselen);
	CTXFREE(gabn, sizeof(lwres_gabnresponse_t));
}
