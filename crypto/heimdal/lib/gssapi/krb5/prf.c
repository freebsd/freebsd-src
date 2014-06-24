/*
 * Copyright (c) 2007 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#include "krb5/gsskrb5_locl.h"

RCSID("$Id: prf.c 21129 2007-06-18 20:28:44Z lha $");

OM_uint32
_gsskrb5_pseudo_random(OM_uint32 *minor_status,
		       gss_ctx_id_t context_handle,
		       int prf_key,
		       const gss_buffer_t prf_in,
		       ssize_t desired_output_len,
		       gss_buffer_t prf_out)
{
    gsskrb5_ctx ctx = (gsskrb5_ctx)context_handle;
    krb5_context context;
    krb5_error_code ret;
    krb5_crypto crypto;
    krb5_data input, output;
    uint32_t num;
    unsigned char *p;
    krb5_keyblock *key = NULL;

    if (ctx == NULL) {
	*minor_status = 0;
	return GSS_S_NO_CONTEXT;
    }

    if (desired_output_len <= 0) {
	*minor_status = 0;
	return GSS_S_FAILURE;
    }

    GSSAPI_KRB5_INIT (&context);

    switch(prf_key) {
    case GSS_C_PRF_KEY_FULL:
	_gsskrb5i_get_acceptor_subkey(ctx, context, &key);
	break;
    case GSS_C_PRF_KEY_PARTIAL:
	_gsskrb5i_get_initiator_subkey(ctx, context, &key);
	break;
    default:
	_gsskrb5_set_status("unknown kerberos prf_key");
	*minor_status = 0;
	return GSS_S_FAILURE;
    }

    if (key == NULL) {
	_gsskrb5_set_status("no prf_key found");
	*minor_status = 0;
	return GSS_S_FAILURE;
    }

    ret = krb5_crypto_init(context, key, 0, &crypto);
    krb5_free_keyblock (context, key);
    if (ret) {
	*minor_status = ret;
	return GSS_S_FAILURE;
    }

    prf_out->value = malloc(desired_output_len);
    if (prf_out->value == NULL) {
	_gsskrb5_set_status("Out of memory");
	*minor_status = GSS_KRB5_S_KG_INPUT_TOO_LONG;
	krb5_crypto_destroy(context, crypto);
	return GSS_S_FAILURE;
    }
    prf_out->length = desired_output_len;

    HEIMDAL_MUTEX_lock(&ctx->ctx_id_mutex);

    input.length = prf_in->length + 4;
    input.data = malloc(prf_in->length + 4);
    if (input.data == NULL) {
	OM_uint32 junk;
	_gsskrb5_set_status("Out of memory");
	*minor_status = GSS_KRB5_S_KG_INPUT_TOO_LONG;
	gss_release_buffer(&junk, prf_out);
	krb5_crypto_destroy(context, crypto);
	HEIMDAL_MUTEX_unlock(&ctx->ctx_id_mutex);
	return GSS_S_FAILURE;
    }
    memcpy(((unsigned char *)input.data) + 4, prf_in->value, prf_in->length);

    num = 0;
    p = prf_out->value;
    while(desired_output_len > 0) {
	_gsskrb5_encode_be_om_uint32(num, input.data);
	ret = krb5_crypto_prf(context, crypto, &input, &output);
	if (ret) {
	    OM_uint32 junk;
	    *minor_status = ret;
	    free(input.data);
	    gss_release_buffer(&junk, prf_out);
	    krb5_crypto_destroy(context, crypto);
	    HEIMDAL_MUTEX_unlock(&ctx->ctx_id_mutex);
	    return GSS_S_FAILURE;
	}
	memcpy(p, output.data, min(desired_output_len, output.length));
	p += output.length;
	desired_output_len -= output.length;
	krb5_data_free(&output);
	num++;
    }

    krb5_crypto_destroy(context, crypto);

    HEIMDAL_MUTEX_unlock(&ctx->ctx_id_mutex);

    return GSS_S_COMPLETE;
}
