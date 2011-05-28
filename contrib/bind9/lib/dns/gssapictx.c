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

/* $Id: gssapictx.c,v 1.1.6.3 2005-04-29 00:15:54 marka Exp $ */

#include <config.h>

#include <stdlib.h>

#include <isc/buffer.h>
#include <isc/dir.h>
#include <isc/entropy.h>
#include <isc/lex.h>
#include <isc/mem.h>
#include <isc/once.h>
#include <isc/random.h>
#include <isc/string.h>
#include <isc/time.h>
#include <isc/util.h>

#include <dns/fixedname.h>
#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/rdataclass.h>
#include <dns/result.h>
#include <dns/types.h>
#include <dns/keyvalues.h>

#include <dst/gssapi.h>
#include <dst/result.h>

#include "dst_internal.h"

#ifdef GSSAPI

#include <gssapi/gssapi.h>

#define RETERR(x) do { \
	result = (x); \
	if (result != ISC_R_SUCCESS) \
		goto out; \
	} while (0)

#define REGION_TO_GBUFFER(r, gb)		\
	do {					\
		(gb).length = (r).length;	\
		(gb).value = (r).base;		\
	} while (0)

#define GBUFFER_TO_REGION(gb, r)		\
	do {					\
		(r).length = (gb).length;	\
		(r).base = (gb).value;		\
	} while (0)

static inline void
name_to_gbuffer(dns_name_t *name, isc_buffer_t *buffer,
		gss_buffer_desc *gbuffer)
{
	dns_name_t tname, *namep;
	isc_region_t r;
	isc_result_t result;

	if (!dns_name_isabsolute(name))
		namep = name;
	else {
		unsigned int labels;
		dns_name_init(&tname, NULL);
		labels = dns_name_countlabels(name);
		dns_name_getlabelsequence(name, 0, labels - 1, &tname);
		namep = &tname;
	}
					
	result = dns_name_totext(namep, ISC_FALSE, buffer);
	isc_buffer_putuint8(buffer, 0);
	isc_buffer_usedregion(buffer, &r);
	REGION_TO_GBUFFER(r, *gbuffer);
}

isc_result_t
dst_gssapi_acquirecred(dns_name_t *name, isc_boolean_t initiate, void **cred) {
	isc_buffer_t namebuf;
	gss_name_t gname;
	gss_buffer_desc gnamebuf;
	unsigned char array[DNS_NAME_MAXTEXT + 1];
	OM_uint32 gret, minor;
	gss_OID_set mechs;
	OM_uint32 lifetime;
	gss_cred_usage_t usage;

	REQUIRE(cred != NULL && *cred == NULL);

	if (name != NULL) {
		isc_buffer_init(&namebuf, array, sizeof(array));
		name_to_gbuffer(name, &namebuf, &gnamebuf);
		gret = gss_import_name(&minor, &gnamebuf, GSS_C_NO_OID,
				       &gname);
		if (gret != GSS_S_COMPLETE)
			return (ISC_R_FAILURE);
	} else
		gname = NULL;

	if (initiate)
		usage = GSS_C_INITIATE;
	else
		usage = GSS_C_ACCEPT;

	gret = gss_acquire_cred(&minor, gname, GSS_C_INDEFINITE,
				GSS_C_NO_OID_SET, usage,
				cred, &mechs, &lifetime);
	if (gret != GSS_S_COMPLETE)
		return (ISC_R_FAILURE);
	return (ISC_R_SUCCESS);
}

isc_result_t
dst_gssapi_initctx(dns_name_t *name, void *cred,
		   isc_region_t *intoken, isc_buffer_t *outtoken,
		   void **context)
{
	isc_region_t r;
	isc_buffer_t namebuf;
	gss_buffer_desc gnamebuf, gintoken, *gintokenp, gouttoken;
	OM_uint32 gret, minor, flags, ret_flags;
	gss_OID mech_type, ret_mech_type;
	OM_uint32 lifetime;
	gss_name_t gname;
	isc_result_t result;
	unsigned char array[DNS_NAME_MAXTEXT + 1];

	isc_buffer_init(&namebuf, array, sizeof(array));
	name_to_gbuffer(name, &namebuf, &gnamebuf);
	gret = gss_import_name(&minor, &gnamebuf, GSS_C_NO_OID, &gname);
	if (gret != GSS_S_COMPLETE)
		return (ISC_R_FAILURE);

	if (intoken != NULL) {
		REGION_TO_GBUFFER(*intoken, gintoken);
		gintokenp = &gintoken;
	} else
		gintokenp = NULL;

	if (*context == NULL)
		*context = GSS_C_NO_CONTEXT;
	flags = GSS_C_REPLAY_FLAG | GSS_C_MUTUAL_FLAG | GSS_C_DELEG_FLAG |
		GSS_C_SEQUENCE_FLAG | GSS_C_CONF_FLAG | GSS_C_INTEG_FLAG;
	mech_type = GSS_C_NO_OID;

	gret = gss_init_sec_context(&minor, cred, context, gname,
				    mech_type, flags, 0,
				    GSS_C_NO_CHANNEL_BINDINGS, gintokenp,
				    &ret_mech_type, &gouttoken, &ret_flags,
				    &lifetime);
	if (gret != GSS_S_COMPLETE && gret != GSS_S_CONTINUE_NEEDED)
		return (ISC_R_FAILURE);

	GBUFFER_TO_REGION(gouttoken, r);
	RETERR(isc_buffer_copyregion(outtoken, &r));

	if (gret == GSS_S_COMPLETE)
		return (ISC_R_SUCCESS);
	else
		return (DNS_R_CONTINUE);

 out:
 	return (result);
}

isc_result_t
dst_gssapi_acceptctx(dns_name_t *name, void *cred,
		     isc_region_t *intoken, isc_buffer_t *outtoken,
		     void **context)
{
	isc_region_t r;
	isc_buffer_t namebuf;
	gss_buffer_desc gnamebuf, gintoken, gouttoken;
	OM_uint32 gret, minor, flags;
	gss_OID mech_type;
	OM_uint32 lifetime;
	gss_cred_id_t delegated_cred;
	gss_name_t gname;
	isc_result_t result;
	unsigned char array[DNS_NAME_MAXTEXT + 1];

	isc_buffer_init(&namebuf, array, sizeof(array));
	name_to_gbuffer(name, &namebuf, &gnamebuf);
	gret = gss_import_name(&minor, &gnamebuf, GSS_C_NO_OID, &gname);
	if (gret != GSS_S_COMPLETE)
		return (ISC_R_FAILURE);

	REGION_TO_GBUFFER(*intoken, gintoken);

	if (*context == NULL)
		*context = GSS_C_NO_CONTEXT;

	gret = gss_accept_sec_context(&minor, context, cred, &gintoken,
				      GSS_C_NO_CHANNEL_BINDINGS, gname,
				      &mech_type, &gouttoken, &flags,
				      &lifetime, &delegated_cred);
	if (gret != GSS_S_COMPLETE)
		return (ISC_R_FAILURE);

	GBUFFER_TO_REGION(gouttoken, r);
	RETERR(isc_buffer_copyregion(outtoken, &r));

	return (ISC_R_SUCCESS);

 out:
	return (result);
}

#else

isc_result_t
dst_gssapi_acquirecred(dns_name_t *name, isc_boolean_t initiate, void **cred) {
	UNUSED(name);
	UNUSED(initiate);
	UNUSED(cred);
	return (ISC_R_NOTIMPLEMENTED);
}

isc_result_t
dst_gssapi_initctx(dns_name_t *name, void *cred,
		   isc_region_t *intoken, isc_buffer_t *outtoken,
		   void **context)
{
	UNUSED(name);
	UNUSED(cred);
	UNUSED(intoken);
	UNUSED(outtoken);
	UNUSED(context);
	return (ISC_R_NOTIMPLEMENTED);
}

isc_result_t
dst_gssapi_acceptctx(dns_name_t *name, void *cred,
		     isc_region_t *intoken, isc_buffer_t *outtoken,
		     void **context)
{
	UNUSED(name);
	UNUSED(cred);
	UNUSED(intoken);
	UNUSED(outtoken);
	UNUSED(context);
	return (ISC_R_NOTIMPLEMENTED);
}

#endif

/*! \file */
