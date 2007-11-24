/*
 * Copyright (c) 2003 Kungliga Tekniska Högskolan
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

RCSID("$Id: add_cred.c,v 1.2.2.1 2003/10/21 21:00:47 lha Exp $");

OM_uint32 gss_add_cred (
     OM_uint32           *minor_status,
     const gss_cred_id_t input_cred_handle,
     const gss_name_t    desired_name,
     const gss_OID       desired_mech,
     gss_cred_usage_t    cred_usage,
     OM_uint32           initiator_time_req,
     OM_uint32           acceptor_time_req,
     gss_cred_id_t       *output_cred_handle,
     gss_OID_set         *actual_mechs,
     OM_uint32           *initiator_time_rec,
     OM_uint32           *acceptor_time_rec)
{
    OM_uint32 ret, lifetime;
    gss_cred_id_t cred, handle;

    handle = NULL;
    cred = input_cred_handle;

    if (gss_oid_equal(desired_mech, GSS_KRB5_MECHANISM) == 0) {
	*minor_status = 0;
	return GSS_S_BAD_MECH;
    }

    if (cred == GSS_C_NO_CREDENTIAL && output_cred_handle == NULL) {
	*minor_status = 0;
	return GSS_S_NO_CRED;
    }

    /* check if requested output usage is compatible with output usage */ 
    if (output_cred_handle != NULL &&
	(cred->usage != cred_usage && cred->usage != GSS_C_BOTH)) {
	*minor_status = GSS_KRB5_S_G_BAD_USAGE;
	return(GSS_S_FAILURE);
    }
	
    /* check that we have the same name */
    if (desired_name != GSS_C_NO_NAME &&
	krb5_principal_compare(gssapi_krb5_context, desired_name,
			       cred->principal) != FALSE) {
	*minor_status = 0;
	return GSS_S_BAD_NAME;
    }

    /* make a copy */
    if (output_cred_handle) {

	handle = (gss_cred_id_t)malloc(sizeof(*handle));
	if (handle == GSS_C_NO_CREDENTIAL) {
	    *minor_status = ENOMEM;
	    return (GSS_S_FAILURE);
	}

	memset(handle, 0, sizeof (*handle));

	handle->usage = cred_usage;
	handle->lifetime = cred->lifetime;
	handle->principal = NULL;
	handle->keytab = NULL;
	handle->ccache = NULL;
	handle->mechanisms = NULL;
	
	ret = GSS_S_FAILURE;

	ret = gss_duplicate_name(minor_status, cred->principal,
				 &handle->principal);
	if (ret) {
	    free(handle);
	    *minor_status = ENOMEM;
	    return GSS_S_FAILURE;
	}

	if (cred->keytab) {
	    krb5_error_code kret;
	    char name[KRB5_KT_PREFIX_MAX_LEN + MAXPATHLEN];
	    int len;
	    
	    ret = GSS_S_FAILURE;

	    kret = krb5_kt_get_type(gssapi_krb5_context, cred->keytab,
				    name, KRB5_KT_PREFIX_MAX_LEN);
	    if (kret) {
		*minor_status = kret;
		goto failure;
	    }
	    len = strlen(name);
	    name[len++] = ':';

	    kret = krb5_kt_get_name(gssapi_krb5_context, cred->keytab,
				    name + len, 
				    sizeof(name) - len);
	    if (kret) {
		*minor_status = kret;
		goto failure;
	    }

	    kret = krb5_kt_resolve(gssapi_krb5_context, name,
				   &handle->keytab);
	    if (kret){
		*minor_status = kret;
		goto failure;
	    }
	}

	if (cred->ccache) {
	    krb5_error_code kret;
	    const char *type, *name;
	    char *type_name;

	    ret = GSS_S_FAILURE;

	    type = krb5_cc_get_type(gssapi_krb5_context, cred->ccache);
	    if (type == NULL){
		*minor_status = ENOMEM;
		goto failure;
	    }

	    if (strcmp(type, "MEMORY") == 0) {
		ret = krb5_cc_gen_new(gssapi_krb5_context, &krb5_mcc_ops,
				      &handle->ccache);
		if (ret) {
		    *minor_status = ret;
		    goto failure;
		}

		ret = krb5_cc_copy_cache(gssapi_krb5_context, cred->ccache,
					 handle->ccache);
		if (ret) {
		    *minor_status = ret;
		    goto failure;
		}

	    } else {

		name = krb5_cc_get_name(gssapi_krb5_context, cred->ccache);
		if (name == NULL) {
		    *minor_status = ENOMEM;
		    goto failure;
		}
		
		asprintf(&type_name, "%s:%s", type, name);
		if (type_name == NULL) {
		    *minor_status = ENOMEM;
		    goto failure;
		}
		
		kret = krb5_cc_resolve(gssapi_krb5_context, type_name,
				       &handle->ccache);
		free(type_name);
		if (kret) {
		    *minor_status = kret;
		    goto failure;
		}	    
	    }
	}

	ret = gss_create_empty_oid_set(minor_status, &handle->mechanisms);
	if (ret)
	    goto failure;

	ret = gss_add_oid_set_member(minor_status, GSS_KRB5_MECHANISM,
				     &handle->mechanisms);
	if (ret)
	    goto failure;
    }

    ret = gss_inquire_cred(minor_status, cred, NULL, &lifetime,
			   NULL, actual_mechs);
    if (ret)
	goto failure;

    if (initiator_time_rec)
	*initiator_time_rec = lifetime;
    if (acceptor_time_rec)
	*acceptor_time_rec = lifetime;

    if (output_cred_handle)
	*output_cred_handle = handle;

    *minor_status = 0;
    return ret;

 failure:

    if (handle) {
	if (handle->principal)
	    gss_release_name(NULL, &handle->principal);
	if (handle->keytab)
	    krb5_kt_close(gssapi_krb5_context, handle->keytab);
	if (handle->ccache)
	    krb5_cc_destroy(gssapi_krb5_context, handle->ccache);
	if (handle->mechanisms)
	    gss_release_oid_set(NULL, &handle->mechanisms);
	free(handle);
    }
    return ret;
}
