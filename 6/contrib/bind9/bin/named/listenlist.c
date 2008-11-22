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

/* $Id: listenlist.c,v 1.9.208.1 2004/03/06 10:21:18 marka Exp $ */

#include <config.h>

#include <isc/mem.h>
#include <isc/util.h>

#include <dns/acl.h>

#include <named/listenlist.h>

static void
destroy(ns_listenlist_t *list);

isc_result_t
ns_listenelt_create(isc_mem_t *mctx, in_port_t port,
		    dns_acl_t *acl, ns_listenelt_t **target)
{
	ns_listenelt_t *elt = NULL;
	REQUIRE(target != NULL && *target == NULL);
	elt = isc_mem_get(mctx, sizeof(*elt));
	if (elt == NULL)
		return (ISC_R_NOMEMORY);
	elt->mctx = mctx;
	ISC_LINK_INIT(elt, link);
	elt->port = port;
	elt->acl = acl;
	*target = elt;
	return (ISC_R_SUCCESS);
}

void
ns_listenelt_destroy(ns_listenelt_t *elt) {
	if (elt->acl != NULL)
		dns_acl_detach(&elt->acl);
	isc_mem_put(elt->mctx, elt, sizeof(*elt));
}

isc_result_t
ns_listenlist_create(isc_mem_t *mctx, ns_listenlist_t **target) {
	ns_listenlist_t *list = NULL;
	REQUIRE(target != NULL && *target == NULL);
	list = isc_mem_get(mctx, sizeof(*list));
	if (list == NULL)
		return (ISC_R_NOMEMORY);
	list->mctx = mctx;
	list->refcount = 1;
	ISC_LIST_INIT(list->elts);
	*target = list;
	return (ISC_R_SUCCESS);
}

static void
destroy(ns_listenlist_t *list) {
	ns_listenelt_t *elt, *next;
	for (elt = ISC_LIST_HEAD(list->elts);
	     elt != NULL;
	     elt = next)
	{
		next = ISC_LIST_NEXT(elt, link);
		ns_listenelt_destroy(elt);
	}
	isc_mem_put(list->mctx, list, sizeof(*list));
}

void
ns_listenlist_attach(ns_listenlist_t *source, ns_listenlist_t **target) {
	INSIST(source->refcount > 0);
	source->refcount++;
	*target = source;
}

void
ns_listenlist_detach(ns_listenlist_t **listp) {
	ns_listenlist_t *list = *listp;
	INSIST(list->refcount > 0);
	list->refcount--;
	if (list->refcount == 0)
		destroy(list);
	*listp = NULL;
}

isc_result_t
ns_listenlist_default(isc_mem_t *mctx, in_port_t port,
		      isc_boolean_t enabled, ns_listenlist_t **target)
{
	isc_result_t result;
	dns_acl_t *acl = NULL;
	ns_listenelt_t *elt = NULL;
	ns_listenlist_t *list = NULL;

	REQUIRE(target != NULL && *target == NULL);
	if (enabled)
		result = dns_acl_any(mctx, &acl);
	else
		result = dns_acl_none(mctx, &acl);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	result = ns_listenelt_create(mctx, port, acl, &elt);
	if (result != ISC_R_SUCCESS)
		goto cleanup_acl;

	result = ns_listenlist_create(mctx, &list);
	if (result != ISC_R_SUCCESS)
		goto cleanup_listenelt;

	ISC_LIST_APPEND(list->elts, elt, link);

	*target = list;
	return (ISC_R_SUCCESS);

 cleanup_listenelt:
	ns_listenelt_destroy(elt);
 cleanup_acl:
	dns_acl_detach(&acl);
 cleanup:
	return (result);
}
