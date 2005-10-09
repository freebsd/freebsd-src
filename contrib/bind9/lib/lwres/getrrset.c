/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000-2003  Internet Software Consortium.
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

/* $Id: getrrset.c,v 1.11.2.3.2.2 2004/03/06 08:15:31 marka Exp $ */

#include <config.h>

#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include <lwres/lwres.h>
#include <lwres/net.h>
#include <lwres/netdb.h>	/* XXX #include <netdb.h> */

#include "assert_p.h"

static unsigned int
lwresult_to_result(lwres_result_t lwresult) {
	switch (lwresult) {
	case LWRES_R_SUCCESS:	return (ERRSET_SUCCESS);
	case LWRES_R_NOMEMORY:	return (ERRSET_NOMEMORY);
	case LWRES_R_NOTFOUND:	return (ERRSET_NONAME);
	case LWRES_R_TYPENOTFOUND: return (ERRSET_NODATA);
	default:		return (ERRSET_FAIL);
	}
}

/*
 * malloc / calloc functions that guarantee to only
 * return NULL if there is an error, like they used
 * to before the ANSI C committee broke them.
 */

static void *
sane_malloc(size_t size) {
	if (size == 0U)
		size = 1;
	return (malloc(size));
}

static void *
sane_calloc(size_t number, size_t size) {
	size_t len = number * size;
	void *mem  = sane_malloc(len);
	if (mem != NULL)
		memset(mem, 0, len);
	return (mem);
}

int
lwres_getrrsetbyname(const char *hostname, unsigned int rdclass,
		     unsigned int rdtype, unsigned int flags,
		     struct rrsetinfo **res)
{
	lwres_context_t *lwrctx = NULL;
	lwres_result_t lwresult;
	lwres_grbnresponse_t *response = NULL;
	struct rrsetinfo *rrset = NULL;
	unsigned int i;
	unsigned int lwflags;
	unsigned int result;

	if (rdclass > 0xffff || rdtype > 0xffff) {
		result = ERRSET_INVAL;
		goto fail;
	}

	/*
	 * Don't allow queries of class or type ANY
	 */
	if (rdclass == 0xff || rdtype == 0xff) {
		result = ERRSET_INVAL;
		goto fail;
	}

	lwresult = lwres_context_create(&lwrctx, NULL, NULL, NULL, 0);
	if (lwresult != LWRES_R_SUCCESS) {
		result = lwresult_to_result(lwresult);
		goto fail;
	}
	(void) lwres_conf_parse(lwrctx, lwres_resolv_conf);

	/*
	 * If any input flags were defined, lwflags would be set here
	 * based on them
	 */
	UNUSED(flags);
	lwflags = 0;

	lwresult = lwres_getrdatabyname(lwrctx, hostname,
					(lwres_uint16_t)rdclass, 
					(lwres_uint16_t)rdtype,
					lwflags, &response);
	if (lwresult != LWRES_R_SUCCESS) {
		result = lwresult_to_result(lwresult);
		goto fail;
	}

	rrset = sane_malloc(sizeof(struct rrsetinfo));
	if (rrset == NULL) {
		result = ERRSET_NOMEMORY;
		goto fail;
	}
	rrset->rri_name = NULL;
	rrset->rri_rdclass = response->rdclass;
	rrset->rri_rdtype = response->rdtype;
	rrset->rri_ttl = response->ttl;
	rrset->rri_flags = 0;
	rrset->rri_nrdatas = 0;
	rrset->rri_rdatas = NULL;
	rrset->rri_nsigs = 0;
	rrset->rri_sigs = NULL;

	rrset->rri_name = sane_malloc(response->realnamelen + 1);
	if (rrset->rri_name == NULL) {
		result = ERRSET_NOMEMORY;
		goto fail;
	}
	strncpy(rrset->rri_name, response->realname, response->realnamelen);
	rrset->rri_name[response->realnamelen] = 0;

	if ((response->flags & LWRDATA_VALIDATED) != 0)
		rrset->rri_flags |= RRSET_VALIDATED;

	rrset->rri_nrdatas = response->nrdatas;
	rrset->rri_rdatas = sane_calloc(rrset->rri_nrdatas,
				   sizeof(struct rdatainfo));
	if (rrset->rri_rdatas == NULL) {
		result = ERRSET_NOMEMORY;
		goto fail;
	}
	for (i = 0; i < rrset->rri_nrdatas; i++) {
		rrset->rri_rdatas[i].rdi_length = response->rdatalen[i];
		rrset->rri_rdatas[i].rdi_data =
				sane_malloc(rrset->rri_rdatas[i].rdi_length);
		if (rrset->rri_rdatas[i].rdi_data == NULL) {
			result = ERRSET_NOMEMORY;
			goto fail;
		}
		memcpy(rrset->rri_rdatas[i].rdi_data, response->rdatas[i],
		       rrset->rri_rdatas[i].rdi_length);
	}
	rrset->rri_nsigs = response->nsigs;
	rrset->rri_sigs = sane_calloc(rrset->rri_nsigs,
				      sizeof(struct rdatainfo));
	if (rrset->rri_sigs == NULL) {
		result = ERRSET_NOMEMORY;
		goto fail;
	}
	for (i = 0; i < rrset->rri_nsigs; i++) {
		rrset->rri_sigs[i].rdi_length = response->siglen[i];
		rrset->rri_sigs[i].rdi_data =
				sane_malloc(rrset->rri_sigs[i].rdi_length);
		if (rrset->rri_sigs[i].rdi_data == NULL) {
			result = ERRSET_NOMEMORY;
			goto fail;
		}
		memcpy(rrset->rri_sigs[i].rdi_data, response->sigs[i],
		       rrset->rri_sigs[i].rdi_length);
	}

	lwres_grbnresponse_free(lwrctx, &response);
	lwres_conf_clear(lwrctx);
	lwres_context_destroy(&lwrctx);
	*res = rrset;
	return (ERRSET_SUCCESS);
 fail:
	if (rrset != NULL)
		lwres_freerrset(rrset);
	if (response != NULL)
		lwres_grbnresponse_free(lwrctx, &response);
	if (lwrctx != NULL) {
		lwres_conf_clear(lwrctx);
		lwres_context_destroy(&lwrctx);
	}
	return (result);
}

void
lwres_freerrset(struct rrsetinfo *rrset) {
	unsigned int i;
	for (i = 0; i < rrset->rri_nrdatas; i++) {
		if (rrset->rri_rdatas[i].rdi_data == NULL)
			break;
		free(rrset->rri_rdatas[i].rdi_data);
	}
	free(rrset->rri_rdatas);
	for (i = 0; i < rrset->rri_nsigs; i++) {
		if (rrset->rri_sigs[i].rdi_data == NULL)
			break;
		free(rrset->rri_sigs[i].rdi_data);
	}
	free(rrset->rri_sigs);
	free(rrset->rri_name);
	free(rrset);
}
