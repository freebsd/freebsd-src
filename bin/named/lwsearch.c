/*
 * Copyright (C) 2004, 2005  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id: lwsearch.c,v 1.8.18.3 2005/07/12 01:22:17 marka Exp $ */

/*! \file */

#include <config.h>

#include <isc/magic.h>
#include <isc/mem.h>
#include <isc/mutex.h>
#include <isc/result.h>
#include <isc/types.h>
#include <isc/util.h>

#include <dns/name.h>
#include <dns/types.h>

#include <named/lwsearch.h>
#include <named/types.h>

#define LWSEARCHLIST_MAGIC		ISC_MAGIC('L', 'W', 'S', 'L')
#define VALID_LWSEARCHLIST(l)		ISC_MAGIC_VALID(l, LWSEARCHLIST_MAGIC)

isc_result_t
ns_lwsearchlist_create(isc_mem_t *mctx, ns_lwsearchlist_t **listp) {
	ns_lwsearchlist_t *list;
	isc_result_t result;

	REQUIRE(mctx != NULL);
	REQUIRE(listp != NULL && *listp == NULL);

	list = isc_mem_get(mctx, sizeof(ns_lwsearchlist_t));
	if (list == NULL)
		return (ISC_R_NOMEMORY);
	
	result = isc_mutex_init(&list->lock);
	if (result != ISC_R_SUCCESS) {
		isc_mem_put(mctx, list, sizeof(ns_lwsearchlist_t));
		return (result);
	}
	list->mctx = NULL;
	isc_mem_attach(mctx, &list->mctx);
	list->refs = 1;
	ISC_LIST_INIT(list->names);
	list->magic = LWSEARCHLIST_MAGIC;

	*listp = list;
	return (ISC_R_SUCCESS);
}

void
ns_lwsearchlist_attach(ns_lwsearchlist_t *source, ns_lwsearchlist_t **target) {
	REQUIRE(VALID_LWSEARCHLIST(source));
	REQUIRE(target != NULL && *target == NULL);

	LOCK(&source->lock);
	INSIST(source->refs > 0);
	source->refs++;
	INSIST(source->refs != 0);
	UNLOCK(&source->lock);

	*target = source;
}

void
ns_lwsearchlist_detach(ns_lwsearchlist_t **listp) {
	ns_lwsearchlist_t *list;
	isc_mem_t *mctx;

	REQUIRE(listp != NULL);
	list = *listp;
	REQUIRE(VALID_LWSEARCHLIST(list));

	LOCK(&list->lock);
	INSIST(list->refs > 0);
	list->refs--;
	UNLOCK(&list->lock);

	*listp = NULL;
	if (list->refs != 0)
		return;

	mctx = list->mctx;
	while (!ISC_LIST_EMPTY(list->names)) {
		dns_name_t *name = ISC_LIST_HEAD(list->names);
		ISC_LIST_UNLINK(list->names, name, link);
		dns_name_free(name, list->mctx);
		isc_mem_put(list->mctx, name, sizeof(dns_name_t));
	}
	list->magic = 0;
	isc_mem_put(mctx, list, sizeof(ns_lwsearchlist_t));
	isc_mem_detach(&mctx);
}

isc_result_t
ns_lwsearchlist_append(ns_lwsearchlist_t *list, dns_name_t *name) {
	dns_name_t *newname;
	isc_result_t result;

	REQUIRE(VALID_LWSEARCHLIST(list));
	REQUIRE(name != NULL);

	newname = isc_mem_get(list->mctx, sizeof(dns_name_t));
	if (newname == NULL)
		return (ISC_R_NOMEMORY);
	dns_name_init(newname, NULL);
	result = dns_name_dup(name, list->mctx, newname);
	if (result != ISC_R_SUCCESS) {
		isc_mem_put(list->mctx, newname, sizeof(dns_name_t));
		return (result);
	}
	ISC_LINK_INIT(newname, link);
	ISC_LIST_APPEND(list->names, newname, link);
	return (ISC_R_SUCCESS);
}

void
ns_lwsearchctx_init(ns_lwsearchctx_t *sctx, ns_lwsearchlist_t *list,
		    dns_name_t *name, unsigned int ndots)
{
	INSIST(sctx != NULL);
	sctx->relname = name;
	sctx->searchname = NULL;
	sctx->doneexact = ISC_FALSE;
	sctx->exactfirst = ISC_FALSE;
	sctx->ndots = ndots;
	if (dns_name_isabsolute(name) || list == NULL) {
		sctx->list = NULL;
		return;
	}
	sctx->list = list;
	sctx->searchname = ISC_LIST_HEAD(sctx->list->names);
	if (dns_name_countlabels(name) > ndots)
		sctx->exactfirst = ISC_TRUE;
}

void
ns_lwsearchctx_first(ns_lwsearchctx_t *sctx) {
	REQUIRE(sctx != NULL);
	UNUSED(sctx);
}

isc_result_t
ns_lwsearchctx_next(ns_lwsearchctx_t *sctx) {
	REQUIRE(sctx != NULL);

	if (sctx->list == NULL)
		return (ISC_R_NOMORE);

	if (sctx->searchname == NULL) {
		INSIST (!sctx->exactfirst || sctx->doneexact);
		if (sctx->exactfirst || sctx->doneexact)
			return (ISC_R_NOMORE);
		sctx->doneexact = ISC_TRUE;
	} else {
		if (sctx->exactfirst && !sctx->doneexact)
			sctx->doneexact = ISC_TRUE;
		else {
			sctx->searchname = ISC_LIST_NEXT(sctx->searchname,
							 link);
			if (sctx->searchname == NULL && sctx->doneexact)
				return (ISC_R_NOMORE);
		}
	}

	return (ISC_R_SUCCESS);
}

isc_result_t
ns_lwsearchctx_current(ns_lwsearchctx_t *sctx, dns_name_t *absname) {
	dns_name_t *tname;
	isc_boolean_t useexact = ISC_FALSE;

	REQUIRE(sctx != NULL);

	if (sctx->list == NULL ||
	    sctx->searchname == NULL ||
	    (sctx->exactfirst && !sctx->doneexact))
		useexact = ISC_TRUE;

	if (useexact) {
		if (dns_name_isabsolute(sctx->relname))
			tname = NULL;
		else
			tname = dns_rootname;
	} else
		tname = sctx->searchname;

	return (dns_name_concatenate(sctx->relname, tname, absname, NULL));
}
