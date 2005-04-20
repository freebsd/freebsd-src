/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000, 2001, 2003  Internet Software Consortium.
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

/* $Id: lwdgrbn.c,v 1.11.208.3 2004/03/08 04:04:19 marka Exp $ */

#include <config.h>

#include <isc/mem.h>
#include <isc/socket.h>
#include <isc/string.h>		/* Required for HP/UX (and others?) */
#include <isc/util.h>

#include <dns/db.h>
#include <dns/lookup.h>
#include <dns/rdata.h>
#include <dns/rdataset.h>
#include <dns/rdatasetiter.h>
#include <dns/result.h>
#include <dns/view.h>

#include <named/types.h>
#include <named/lwdclient.h>
#include <named/lwresd.h>
#include <named/lwsearch.h>

static void start_lookup(ns_lwdclient_t *);

static isc_result_t
fill_array(int *pos, dns_rdataset_t *rdataset,
	   int size, unsigned char **rdatas, lwres_uint16_t *rdatalen)
{
	dns_rdata_t rdata;
	isc_result_t result;
	isc_region_t r;

	UNUSED(size);

	dns_rdata_init(&rdata);
	for (result = dns_rdataset_first(rdataset);
	     result == ISC_R_SUCCESS;
	     result = dns_rdataset_next(rdataset))
	{
		INSIST(*pos < size);
		dns_rdataset_current(rdataset, &rdata);
		dns_rdata_toregion(&rdata, &r);
		rdatas[*pos] = r.base;
		rdatalen[*pos] = r.length;
		dns_rdata_reset(&rdata);
		(*pos)++;
	}
	if (result == ISC_R_NOMORE)
		result = ISC_R_SUCCESS;
	return (result);
}

static isc_result_t
iterate_node(lwres_grbnresponse_t *grbn, dns_db_t *db, dns_dbnode_t *node,
	     isc_mem_t *mctx)
{
	int used = 0, count;
	int size = 8, oldsize = 0;
	unsigned char **rdatas = NULL, **oldrdatas = NULL, **newrdatas = NULL;
	lwres_uint16_t *lens = NULL, *oldlens = NULL, *newlens = NULL;
	dns_rdatasetiter_t *iter = NULL;
	dns_rdataset_t set;
	dns_ttl_t ttl = ISC_INT32_MAX;
	lwres_uint32_t flags = LWRDATA_VALIDATED;
	isc_result_t result = ISC_R_NOMEMORY;

	result = dns_db_allrdatasets(db, node, NULL, 0, &iter);
	if (result != ISC_R_SUCCESS)
		goto out;

	rdatas = isc_mem_get(mctx, size * sizeof(*rdatas));
	if (rdatas == NULL)
		goto out;
	lens = isc_mem_get(mctx, size * sizeof(*lens));
	if (lens == NULL)
		goto out;

	for (result = dns_rdatasetiter_first(iter);
	     result == ISC_R_SUCCESS;
	     result = dns_rdatasetiter_next(iter))
	{
		result = ISC_R_NOMEMORY;
		dns_rdataset_init(&set);
		dns_rdatasetiter_current(iter, &set);

		if (set.type != dns_rdatatype_rrsig) {
			dns_rdataset_disassociate(&set);
			continue;
		}

		count = dns_rdataset_count(&set);
		if (used + count > size) {
			/* copy & reallocate */
			oldsize = size;
			oldrdatas = rdatas;
			oldlens = lens;
			rdatas = NULL;
			lens = NULL;

			size *= 2;

			rdatas = isc_mem_get(mctx, size * sizeof(*rdatas));
			if (rdatas == NULL)
				goto out;
			lens = isc_mem_get(mctx, size * sizeof(*lens));
			if (lens == NULL)
				goto out;
			memcpy(rdatas, oldrdatas, used * sizeof(*rdatas));
			memcpy(lens, oldlens, used * sizeof(*lens));
			isc_mem_put(mctx, oldrdatas,
				    oldsize * sizeof(*oldrdatas));
			isc_mem_put(mctx, oldlens, oldsize * sizeof(*oldlens));
			oldrdatas = NULL;
			oldlens = NULL;
		}
		if (set.ttl < ttl)
			ttl = set.ttl;
		if (set.trust != dns_trust_secure)
			flags &= (~LWRDATA_VALIDATED);
		result = fill_array(&used, &set, size, rdatas, lens);
		dns_rdataset_disassociate(&set);
		if (result != ISC_R_SUCCESS)
			goto out;
	}
	if (result == ISC_R_NOMORE)
		result = ISC_R_SUCCESS;
	if (result != ISC_R_SUCCESS)
		goto out;
	dns_rdatasetiter_destroy(&iter);

	/*
	 * If necessary, shrink and copy the arrays.
	 */
	if (size != used) {
		result = ISC_R_NOMEMORY;
		newrdatas = isc_mem_get(mctx, used * sizeof(*rdatas));
		if (newrdatas == NULL)
			goto out;
		newlens = isc_mem_get(mctx, used * sizeof(*lens));
		if (newlens == NULL)
			goto out;
		memcpy(newrdatas, rdatas, used * sizeof(*rdatas));
		memcpy(newlens, lens, used * sizeof(*lens));
		isc_mem_put(mctx, rdatas, size * sizeof(*rdatas));
		isc_mem_put(mctx, lens, size * sizeof(*lens));
		grbn->rdatas = newrdatas;
		grbn->rdatalen = newlens;
	} else {
		grbn->rdatas = rdatas;
		grbn->rdatalen = lens;
	}
	grbn->nrdatas = used;
	grbn->ttl = ttl;
	grbn->flags = flags;
	return (ISC_R_SUCCESS);

 out:
	dns_rdatasetiter_destroy(&iter);
	if (rdatas != NULL)
		isc_mem_put(mctx, rdatas, size * sizeof(*rdatas));
	if (lens != NULL)
		isc_mem_put(mctx, lens, size * sizeof(*lens));
	if (oldrdatas != NULL)
		isc_mem_put(mctx, oldrdatas, oldsize * sizeof(*oldrdatas));
	if (oldlens != NULL)
		isc_mem_put(mctx, oldlens, oldsize * sizeof(*oldlens));
	if (newrdatas != NULL)
		isc_mem_put(mctx, newrdatas, used * sizeof(*oldrdatas));
	if (newlens != NULL)
		isc_mem_put(mctx, newlens, used * sizeof(*oldlens));
	return (result);
}

static void
lookup_done(isc_task_t *task, isc_event_t *event) {
	ns_lwdclient_t *client;
	ns_lwdclientmgr_t *cm;
	dns_lookupevent_t *levent;
	lwres_buffer_t lwb;
	dns_name_t *name;
	dns_rdataset_t *rdataset;
	dns_rdataset_t *sigrdataset;
	isc_result_t result;
	lwres_result_t lwresult;
	isc_region_t r;
	isc_buffer_t b;
	lwres_grbnresponse_t *grbn;
	int i;

	UNUSED(task);

	lwb.base = NULL;
	client = event->ev_arg;
	cm = client->clientmgr;
	INSIST(client->lookup == (dns_lookup_t *)event->ev_sender);

	levent = (dns_lookupevent_t *)event;
	grbn = &client->grbn;

	ns_lwdclient_log(50, "lookup event result = %s",
			 isc_result_totext(levent->result));

	result = levent->result;
	if (result != ISC_R_SUCCESS) {
		dns_lookup_destroy(&client->lookup);
		isc_event_free(&event);
		levent = NULL;

		switch (result) {
		case DNS_R_NXDOMAIN:
		case DNS_R_NCACHENXDOMAIN:
			result = ns_lwsearchctx_next(&client->searchctx);
			if (result != ISC_R_SUCCESS)
				lwresult = LWRES_R_NOTFOUND;
			else {
				start_lookup(client);
				return;
			}
			break;
		case DNS_R_NXRRSET:
		case DNS_R_NCACHENXRRSET:
			lwresult = LWRES_R_TYPENOTFOUND;
			break;
		default:
			lwresult = LWRES_R_FAILURE;
		}
		ns_lwdclient_errorpktsend(client, lwresult);
		return;
	}

	name = levent->name;
	b = client->recv_buffer;

	grbn->flags = 0;

	grbn->nrdatas = 0;
	grbn->rdatas = NULL;
	grbn->rdatalen = NULL;

	grbn->nsigs = 0;
	grbn->sigs = NULL;
	grbn->siglen = NULL;

	result = dns_name_totext(name, ISC_TRUE, &client->recv_buffer);
	if (result != ISC_R_SUCCESS)
		goto out;
	grbn->realname = (char *)isc_buffer_used(&b);
	grbn->realnamelen = isc_buffer_usedlength(&client->recv_buffer) -
			    isc_buffer_usedlength(&b);
	ns_lwdclient_log(50, "found name '%.*s'", grbn->realnamelen,
			 grbn->realname);

	grbn->rdclass = cm->view->rdclass;
	grbn->rdtype = client->rdtype;

	rdataset = levent->rdataset;
	if (rdataset != NULL) {
		/* The normal case */
		grbn->nrdatas = dns_rdataset_count(rdataset);
		grbn->rdatas = isc_mem_get(cm->mctx, grbn->nrdatas *
					   sizeof(unsigned char *));
		if (grbn->rdatas == NULL)
			goto out;
		grbn->rdatalen = isc_mem_get(cm->mctx, grbn->nrdatas *
					     sizeof(lwres_uint16_t));
		if (grbn->rdatalen == NULL)
			goto out;

		i = 0;
		result = fill_array(&i, rdataset, grbn->nrdatas, grbn->rdatas,
				    grbn->rdatalen);
		if (result != ISC_R_SUCCESS)
			goto out;
		INSIST(i == grbn->nrdatas);
		grbn->ttl = rdataset->ttl;
		if (rdataset->trust == dns_trust_secure)
			grbn->flags |= LWRDATA_VALIDATED;
	} else {
		/* The SIG query case */
		result = iterate_node(grbn, levent->db, levent->node,
				      cm->mctx);
		if (result != ISC_R_SUCCESS)
			goto out;
	}
	ns_lwdclient_log(50, "filled in %d rdata%s", grbn->nrdatas,
			 (grbn->nrdatas == 1) ? "" : "s");

	sigrdataset = levent->sigrdataset;
	if (sigrdataset != NULL) {
		grbn->nsigs = dns_rdataset_count(sigrdataset);
		grbn->sigs = isc_mem_get(cm->mctx, grbn->nsigs *
					 sizeof(unsigned char *));
		if (grbn->sigs == NULL)
			goto out;
		grbn->siglen = isc_mem_get(cm->mctx, grbn->nsigs *
					   sizeof(lwres_uint16_t));
		if (grbn->siglen == NULL)
			goto out;

		i = 0;
		result = fill_array(&i, sigrdataset, grbn->nsigs, grbn->sigs,
				    grbn->siglen);
		if (result != ISC_R_SUCCESS)
			goto out;
		INSIST(i == grbn->nsigs);
		ns_lwdclient_log(50, "filled in %d signature%s", grbn->nsigs,
				 (grbn->nsigs == 1) ? "" : "s");
	}

	dns_lookup_destroy(&client->lookup);
	isc_event_free(&event);

	/*
	 * Render the packet.
	 */
	client->pkt.recvlength = LWRES_RECVLENGTH;
	client->pkt.authtype = 0; /* XXXMLG */
	client->pkt.authlength = 0;
	client->pkt.result = LWRES_R_SUCCESS;

	lwresult = lwres_grbnresponse_render(cm->lwctx,
					     grbn, &client->pkt, &lwb);
	if (lwresult != LWRES_R_SUCCESS)
		goto out;

	isc_mem_put(cm->mctx, grbn->rdatas,
		    grbn->nrdatas * sizeof(unsigned char *));
	isc_mem_put(cm->mctx, grbn->rdatalen,
		    grbn->nrdatas * sizeof(lwres_uint16_t));

	if (grbn->sigs != NULL)
		isc_mem_put(cm->mctx, grbn->sigs,
			    grbn->nsigs * sizeof(unsigned char *));
	if (grbn->siglen != NULL)
		isc_mem_put(cm->mctx, grbn->siglen,
			    grbn->nsigs * sizeof(lwres_uint16_t));

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
	if (grbn->rdatas != NULL)
		isc_mem_put(cm->mctx, grbn->rdatas,
			    grbn->nrdatas * sizeof(unsigned char *));
	if (grbn->rdatalen != NULL)
		isc_mem_put(cm->mctx, grbn->rdatalen,
			    grbn->nrdatas * sizeof(lwres_uint16_t));

	if (grbn->sigs != NULL)
		isc_mem_put(cm->mctx, grbn->sigs,
			    grbn->nsigs * sizeof(unsigned char *));
	if (grbn->siglen != NULL)
		isc_mem_put(cm->mctx, grbn->siglen,
			    grbn->nsigs * sizeof(lwres_uint16_t));

	if (client->lookup != NULL)
		dns_lookup_destroy(&client->lookup);
	if (lwb.base != NULL)
		lwres_context_freemem(cm->lwctx, lwb.base, lwb.length);

	if (event != NULL)
		isc_event_free(&event);

	ns_lwdclient_log(50, "error constructing getrrsetbyname response");
	ns_lwdclient_errorpktsend(client, LWRES_R_FAILURE);
}

static void
start_lookup(ns_lwdclient_t *client) {
	isc_result_t result;
	ns_lwdclientmgr_t *cm;
	dns_fixedname_t absname;

	cm = client->clientmgr;

	INSIST(client->lookup == NULL);

	dns_fixedname_init(&absname);
	result = ns_lwsearchctx_current(&client->searchctx,
					dns_fixedname_name(&absname));
	/*
	 * This will return failure if relative name + suffix is too long.
	 * In this case, just go on to the next entry in the search path.
	 */
	if (result != ISC_R_SUCCESS)
		start_lookup(client);

	result = dns_lookup_create(cm->mctx,
				   dns_fixedname_name(&absname),
				   client->rdtype, cm->view,
				   client->options, cm->task, lookup_done,
				   client, &client->lookup);
	if (result != ISC_R_SUCCESS) {
		ns_lwdclient_errorpktsend(client, LWRES_R_FAILURE);
		return;
	}
}

static void
init_grbn(ns_lwdclient_t *client) {
	client->grbn.rdclass = 0;
	client->grbn.rdtype = 0;
	client->grbn.ttl = 0;
	client->grbn.nrdatas = 0;
	client->grbn.realname = NULL;
	client->grbn.realnamelen = 0;
	client->grbn.rdatas = 0;
	client->grbn.rdatalen = 0;
	client->grbn.base = NULL;
	client->grbn.baselen = 0;
	isc_buffer_init(&client->recv_buffer, client->buffer, LWRES_RECVLENGTH);
}

void
ns_lwdclient_processgrbn(ns_lwdclient_t *client, lwres_buffer_t *b) {
	lwres_grbnrequest_t *req;
	isc_result_t result;
	ns_lwdclientmgr_t *cm;
	isc_buffer_t namebuf;

	REQUIRE(NS_LWDCLIENT_ISRECVDONE(client));
	INSIST(client->byaddr == NULL);

	cm = client->clientmgr;
	req = NULL;

	result = lwres_grbnrequest_parse(cm->lwctx,
					 b, &client->pkt, &req);
	if (result != LWRES_R_SUCCESS)
		goto out;
	if (req->name == NULL)
		goto out;

	client->options = 0;
	if (req->rdclass != cm->view->rdclass)
		goto out;

	if (req->rdclass == dns_rdataclass_any ||
	    req->rdtype == dns_rdatatype_any)
		goto out;

	client->rdtype = req->rdtype;

	isc_buffer_init(&namebuf, req->name, req->namelen);
	isc_buffer_add(&namebuf, req->namelen);

	dns_fixedname_init(&client->query_name);
	result = dns_name_fromtext(dns_fixedname_name(&client->query_name),
				   &namebuf, NULL, ISC_FALSE, NULL);
	if (result != ISC_R_SUCCESS)
		goto out;
	ns_lwsearchctx_init(&client->searchctx,
			    cm->listener->manager->search,
			    dns_fixedname_name(&client->query_name),
			    cm->listener->manager->ndots);
	ns_lwsearchctx_first(&client->searchctx);

	ns_lwdclient_log(50, "client %p looking for type %d",
			 client, client->rdtype);

	/*
	 * We no longer need to keep this around.
	 */
	lwres_grbnrequest_free(cm->lwctx, &req);

	/*
	 * Initialize the real name and alias arrays in the reply we're
	 * going to build up.
	 */
	init_grbn(client);

	/*
	 * Start the find.
	 */
	start_lookup(client);

	return;

	/*
	 * We're screwed.  Return an error packet to our caller.
	 */
 out:
	if (req != NULL)
		lwres_grbnrequest_free(cm->lwctx, &req);

	ns_lwdclient_errorpktsend(client, LWRES_R_FAILURE);
}
