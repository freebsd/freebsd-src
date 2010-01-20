/*
 * Copyright (c) 2004, PADL Software Pty Ltd.
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
 * 3. Neither the name of PADL Software nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PADL SOFTWARE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL PADL SOFTWARE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 *  glue routine for _gsskrb5_inquire_sec_context_by_oid
 */

#include "krb5/gsskrb5_locl.h"

RCSID("$Id: set_sec_context_option.c 20384 2007-04-18 08:51:06Z lha $");

static OM_uint32
get_bool(OM_uint32 *minor_status,
	 const gss_buffer_t value,
	 int *flag)
{
    if (value->value == NULL || value->length != 1) {
	*minor_status = EINVAL;
	return GSS_S_FAILURE;
    }
    *flag = *((const char *)value->value) != 0;
    return GSS_S_COMPLETE;
}

static OM_uint32
get_string(OM_uint32 *minor_status,
	   const gss_buffer_t value,
	   char **str)
{
    if (value == NULL || value->length == 0) {
	*str = NULL;
    } else {
	*str = malloc(value->length + 1);
	if (*str == NULL) {
	    *minor_status = 0;
	    return GSS_S_UNAVAILABLE;
	}
	memcpy(*str, value->value, value->length);
	(*str)[value->length] = '\0';
    }
    return GSS_S_COMPLETE;
}

OM_uint32
_gsskrb5_set_sec_context_option
           (OM_uint32 *minor_status,
            gss_ctx_id_t *context_handle,
            const gss_OID desired_object,
            const gss_buffer_t value)
{
    krb5_context context;
    OM_uint32 maj_stat;

    GSSAPI_KRB5_INIT (&context);

    if (value == GSS_C_NO_BUFFER) {
	*minor_status = EINVAL;
	return GSS_S_FAILURE;
    }

    if (gss_oid_equal(desired_object, GSS_KRB5_COMPAT_DES3_MIC_X)) {
	gsskrb5_ctx ctx;
	int flag;

	if (*context_handle == GSS_C_NO_CONTEXT) {
	    *minor_status = EINVAL;
	    return GSS_S_NO_CONTEXT;
	}

	maj_stat = get_bool(minor_status, value, &flag);
	if (maj_stat != GSS_S_COMPLETE)
	    return maj_stat;

	ctx = (gsskrb5_ctx)*context_handle;
	HEIMDAL_MUTEX_lock(&ctx->ctx_id_mutex);
	if (flag)
	    ctx->more_flags |= COMPAT_OLD_DES3;
	else
	    ctx->more_flags &= ~COMPAT_OLD_DES3;
	ctx->more_flags |= COMPAT_OLD_DES3_SELECTED;
	HEIMDAL_MUTEX_unlock(&ctx->ctx_id_mutex);
	return GSS_S_COMPLETE;
    } else if (gss_oid_equal(desired_object, GSS_KRB5_SET_DNS_CANONICALIZE_X)) {
	int flag;

	maj_stat = get_bool(minor_status, value, &flag);
	if (maj_stat != GSS_S_COMPLETE)
	    return maj_stat;

	krb5_set_dns_canonicalize_hostname(context, flag);
	return GSS_S_COMPLETE;

    } else if (gss_oid_equal(desired_object, GSS_KRB5_REGISTER_ACCEPTOR_IDENTITY_X)) {
	char *str;

	maj_stat = get_string(minor_status, value, &str);
	if (maj_stat != GSS_S_COMPLETE)
	    return maj_stat;

	_gsskrb5_register_acceptor_identity(str);
	free(str);

	*minor_status = 0;
	return GSS_S_COMPLETE;

    } else if (gss_oid_equal(desired_object, GSS_KRB5_SET_DEFAULT_REALM_X)) {
	char *str;

	maj_stat = get_string(minor_status, value, &str);
	if (maj_stat != GSS_S_COMPLETE)
	    return maj_stat;
	if (str == NULL) {
	    *minor_status = 0;
	    return GSS_S_CALL_INACCESSIBLE_READ;
	}

	krb5_set_default_realm(context, str);
	free(str);

	*minor_status = 0;
	return GSS_S_COMPLETE;

    } else if (gss_oid_equal(desired_object, GSS_KRB5_SEND_TO_KDC_X)) {

	if (value == NULL || value->length == 0) {
	    krb5_set_send_to_kdc_func(context, NULL, NULL);
	} else {
	    struct gsskrb5_send_to_kdc c;

	    if (value->length != sizeof(c)) {
		*minor_status = EINVAL;
		return GSS_S_FAILURE;
	    }
	    memcpy(&c, value->value, sizeof(c));
	    krb5_set_send_to_kdc_func(context,
				      (krb5_send_to_kdc_func)c.func, 
				      c.ptr);
	}

	*minor_status = 0;
	return GSS_S_COMPLETE;
    } else if (gss_oid_equal(desired_object, GSS_KRB5_CCACHE_NAME_X)) {
	char *str;

	maj_stat = get_string(minor_status, value, &str);
	if (maj_stat != GSS_S_COMPLETE)
	    return maj_stat;
	if (str == NULL) {
	    *minor_status = 0;
	    return GSS_S_CALL_INACCESSIBLE_READ;
	}

	*minor_status = krb5_cc_set_default_name(context, str);
	free(str);
	if (*minor_status)
	    return GSS_S_FAILURE;

	return GSS_S_COMPLETE;
    }

    *minor_status = EINVAL;
    return GSS_S_FAILURE;
}
