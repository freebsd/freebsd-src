/*
 * Copyright (c) 1997 - 2001 Kungliga Tekniska Högskolan
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

#include "gssapi_locl.h"

RCSID("$Id: acquire_cred.c,v 1.4 2001/01/30 00:49:05 assar Exp $");

OM_uint32 gss_acquire_cred
           (OM_uint32 * minor_status,
            const gss_name_t desired_name,
            OM_uint32 time_req,
            const gss_OID_set desired_mechs,
            gss_cred_usage_t cred_usage,
            gss_cred_id_t * output_cred_handle,
            gss_OID_set * actual_mechs,
            OM_uint32 * time_rec
           )
{
    gss_cred_id_t handle;
    OM_uint32 ret;
    krb5_principal def_princ;
    krb5_ccache ccache;
    krb5_error_code pret = -1, kret = 0;
    krb5_keytab kt;
    krb5_creds cred;
    krb5_get_init_creds_opt opt;

    handle = (gss_cred_id_t)malloc(sizeof(*handle));
    if (handle == GSS_C_NO_CREDENTIAL) {
        return GSS_S_FAILURE;
    }
    memset(handle, 0, sizeof (*handle));

    ret = gss_duplicate_name(minor_status, desired_name, &handle->principal);
    if (ret) {
        return ret;
    }

    if (krb5_cc_default(gssapi_krb5_context, &ccache) == 0 &&
      (pret = krb5_cc_get_principal(gssapi_krb5_context, ccache,
					 &def_princ)) == 0 &&
      krb5_principal_compare(gssapi_krb5_context, handle->principal,
				 def_princ) == TRUE) {
	handle->ccache = ccache;
	handle->keytab = NULL;
    } else {
	kret = krb5_kt_default(gssapi_krb5_context, &kt);
	if (kret != 0)
	    goto out;
	krb5_get_init_creds_opt_init(&opt);
	memset(&cred, 0, sizeof(cred));
	kret = krb5_get_init_creds_keytab(gssapi_krb5_context, &cred,
	  handle->principal, kt, 0, NULL, &opt);
	if (kret != 0) {
	    krb5_kt_close(gssapi_krb5_context, kt);
	    goto out;
	}
	kret = krb5_cc_gen_new(gssapi_krb5_context, &krb5_mcc_ops, &ccache);
	if (kret != 0) {
	    krb5_kt_close(gssapi_krb5_context, kt);
	    goto out;
	}
	kret = krb5_cc_initialize(gssapi_krb5_context, ccache, cred.client);
	if (kret != 0) {
	    krb5_kt_close(gssapi_krb5_context, kt);
	    krb5_cc_close(gssapi_krb5_context, ccache);
	    goto out;
	}
	kret = krb5_cc_store_cred(gssapi_krb5_context, ccache, &cred);
	if (kret != 0) {
	    krb5_kt_close(gssapi_krb5_context, kt);
	    krb5_cc_close(gssapi_krb5_context, ccache);
	    goto out;
	}
	handle->ccache = ccache;
	handle->keytab = kt;
    }


    /* XXX */
    handle->lifetime = time_req;
    handle->usage = cred_usage;

    ret = gss_create_empty_oid_set(minor_status, &handle->mechanisms);
    if (ret) {
        return ret;
    }
    ret = gss_add_oid_set_member(minor_status, GSS_KRB5_MECHANISM,
				 &handle->mechanisms);
    if (ret) {
        return ret;
    }

    ret = gss_inquire_cred(minor_status, handle, NULL, time_rec, NULL,
			   actual_mechs);
    if (ret) {
        return ret;
    }

    *output_cred_handle = handle;

out:
    if (pret == 0)
	krb5_free_principal(gssapi_krb5_context, def_princ);

    if (kret != 0) {
	*minor_status = kret;
	return GSS_S_FAILURE;
    }

    return GSS_S_COMPLETE;
}
