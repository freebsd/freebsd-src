/*
 * Copyright (C) 2004, 2005  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2000-2002  Internet Software Consortium.
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

/*
 * $Id: gssapi_link.c,v 1.1.6.3 2005-04-29 00:15:53 marka Exp $
 */

#ifdef GSSAPI

#include <config.h>

#include <isc/buffer.h>
#include <isc/mem.h>
#include <isc/string.h>
#include <isc/util.h>

#include <dst/result.h>

#include "dst_internal.h"
#include "dst_parse.h"

#include <gssapi/gssapi.h>

#define INITIAL_BUFFER_SIZE 1024
#define BUFFER_EXTRA 1024

#define REGION_TO_GBUFFER(r, gb) \
	do { \
		(gb).length = (r).length;	\
		(gb).value = (r).base;	\
	} while (0)

typedef struct gssapi_ctx {
	isc_buffer_t *buffer;
	gss_ctx_id_t *context_id;
} gssapi_ctx_t;


static isc_result_t
gssapi_createctx(dst_key_t *key, dst_context_t *dctx) {
	gssapi_ctx_t *ctx;
	isc_result_t result;

	UNUSED(key);

	ctx = isc_mem_get(dctx->mctx, sizeof(gssapi_ctx_t));
	if (ctx == NULL)
		return (ISC_R_NOMEMORY);
	ctx->buffer = NULL;
	result = isc_buffer_allocate(dctx->mctx, &ctx->buffer,
				     INITIAL_BUFFER_SIZE);
	if (result != ISC_R_SUCCESS) {
		isc_mem_put(dctx->mctx, ctx, sizeof(gssapi_ctx_t));
		return (result);
	}
	ctx->context_id = key->opaque;
	dctx->opaque = ctx;
	return (ISC_R_SUCCESS);
}

static void
gssapi_destroyctx(dst_context_t *dctx) {
	gssapi_ctx_t *ctx = dctx->opaque;

	if (ctx != NULL) {
		if (ctx->buffer != NULL)
			isc_buffer_free(&ctx->buffer);
		isc_mem_put(dctx->mctx, ctx, sizeof(gssapi_ctx_t));
		dctx->opaque = NULL;
	}
}

static isc_result_t
gssapi_adddata(dst_context_t *dctx, const isc_region_t *data) {
	gssapi_ctx_t *ctx = dctx->opaque;
	isc_buffer_t *newbuffer = NULL;
	isc_region_t r;
	unsigned int length;
	isc_result_t result;

	result = isc_buffer_copyregion(ctx->buffer, data);
	if (result == ISC_R_SUCCESS)
		return (ISC_R_SUCCESS);

	length = isc_buffer_length(ctx->buffer) + data->length + BUFFER_EXTRA;

	result = isc_buffer_allocate(dctx->mctx, &newbuffer, length);
	if (result != ISC_R_SUCCESS)
		return (result);

	isc_buffer_usedregion(ctx->buffer, &r);
	(void) isc_buffer_copyregion(newbuffer, &r);
	(void) isc_buffer_copyregion(newbuffer, data);

	isc_buffer_free(&ctx->buffer);
	ctx->buffer = newbuffer;

	return (ISC_R_SUCCESS);
}

static isc_result_t
gssapi_sign(dst_context_t *dctx, isc_buffer_t *sig) {
	gssapi_ctx_t *ctx = dctx->opaque;
	isc_region_t message;
	gss_buffer_desc gmessage, gsig;
	OM_uint32 minor, gret;

	isc_buffer_usedregion(ctx->buffer, &message);
	REGION_TO_GBUFFER(message, gmessage);

	gret = gss_get_mic(&minor, ctx->context_id,
			   GSS_C_QOP_DEFAULT, &gmessage, &gsig);
	if (gret != 0)
		return (ISC_R_FAILURE);

	if (gsig.length > isc_buffer_availablelength(sig)) {
		gss_release_buffer(&minor, &gsig);
		return (ISC_R_NOSPACE);
	}

	isc_buffer_putmem(sig, gsig.value, gsig.length);

	gss_release_buffer(&minor, &gsig);

	return (ISC_R_SUCCESS);
}

static isc_result_t
gssapi_verify(dst_context_t *dctx, const isc_region_t *sig) {
	gssapi_ctx_t *ctx = dctx->opaque;
	isc_region_t message;
	gss_buffer_desc gmessage, gsig;
	OM_uint32 minor, gret;

	isc_buffer_usedregion(ctx->buffer, &message);
	REGION_TO_GBUFFER(message, gmessage);

	REGION_TO_GBUFFER(*sig, gsig);

	gret = gss_verify_mic(&minor, ctx->context_id, &gmessage, &gsig, NULL);
	if (gret != 0)
		return (ISC_R_FAILURE);

	return (ISC_R_SUCCESS);
}

static isc_boolean_t
gssapi_compare(const dst_key_t *key1, const dst_key_t *key2) {
	gss_ctx_id_t gsskey1 = key1->opaque;
	gss_ctx_id_t gsskey2 = key2->opaque;

	/* No idea */
	return (ISC_TF(gsskey1 == gsskey2));
}

static isc_result_t
gssapi_generate(dst_key_t *key, int unused) {
	UNUSED(key);
	UNUSED(unused);

	/* No idea */
	return (ISC_R_FAILURE);
}

static isc_boolean_t
gssapi_isprivate(const dst_key_t *key) {
	UNUSED(key);
        return (ISC_TRUE);
}

static void
gssapi_destroy(dst_key_t *key) {
	UNUSED(key);
	/* No idea */
}

static dst_func_t gssapi_functions = {
	gssapi_createctx,
	gssapi_destroyctx,
	gssapi_adddata,
	gssapi_sign,
	gssapi_verify,
	NULL, /*%< computesecret */
	gssapi_compare,
	NULL, /*%< paramcompare */
	gssapi_generate,
	gssapi_isprivate,
	gssapi_destroy,
	NULL, /*%< todns */
	NULL, /*%< fromdns */
	NULL, /*%< tofile */
	NULL, /*%< parse */
	NULL, /*%< cleanup */
};

isc_result_t
dst__gssapi_init(dst_func_t **funcp) {
	REQUIRE(funcp != NULL);
	if (*funcp == NULL)
		*funcp = &gssapi_functions;
	return (ISC_R_SUCCESS);
}

#else
int  gssapi_link_unneeded = 1;
#endif

/*! \file */
