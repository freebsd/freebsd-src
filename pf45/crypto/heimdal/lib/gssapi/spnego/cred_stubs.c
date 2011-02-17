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

#include "spnego/spnego_locl.h"

RCSID("$Id: cred_stubs.c 20619 2007-05-08 13:43:45Z lha $");

OM_uint32
_gss_spnego_release_cred(OM_uint32 *minor_status, gss_cred_id_t *cred_handle)
{
    gssspnego_cred cred;
    OM_uint32 ret;
    
    *minor_status = 0;

    if (*cred_handle == GSS_C_NO_CREDENTIAL) {
	return GSS_S_COMPLETE;
    }
    cred = (gssspnego_cred)*cred_handle;

    ret = gss_release_cred(minor_status, &cred->negotiated_cred_id);

    free(cred);
    *cred_handle = GSS_C_NO_CREDENTIAL;

    return ret;
}

OM_uint32
_gss_spnego_alloc_cred(OM_uint32 *minor_status,
		       gss_cred_id_t mech_cred_handle,
		       gss_cred_id_t *cred_handle)
{
    gssspnego_cred cred;

    if (*cred_handle != GSS_C_NO_CREDENTIAL) {
	*minor_status = EINVAL;
	return GSS_S_FAILURE;
    }

    cred = calloc(1, sizeof(*cred));
    if (cred == NULL) {
	*cred_handle = GSS_C_NO_CREDENTIAL;
	*minor_status = ENOMEM;
	return GSS_S_FAILURE;
    }

    cred->negotiated_cred_id = mech_cred_handle;

    *cred_handle = (gss_cred_id_t)cred;

    return GSS_S_COMPLETE; 
}

/*
 * For now, just a simple wrapper that avoids recursion. When
 * we support gss_{get,set}_neg_mechs() we will need to expose
 * more functionality.
 */
OM_uint32 _gss_spnego_acquire_cred
(OM_uint32 *minor_status,
 const gss_name_t desired_name,
 OM_uint32 time_req,
 const gss_OID_set desired_mechs,
 gss_cred_usage_t cred_usage,
 gss_cred_id_t * output_cred_handle,
 gss_OID_set * actual_mechs,
 OM_uint32 * time_rec
    )
{
    const spnego_name dname = (const spnego_name)desired_name;
    gss_name_t name = GSS_C_NO_NAME;
    OM_uint32 ret, tmp;
    gss_OID_set_desc actual_desired_mechs;
    gss_OID_set mechs;
    int i, j;
    gss_cred_id_t cred_handle = GSS_C_NO_CREDENTIAL;
    gssspnego_cred cred;

    *output_cred_handle = GSS_C_NO_CREDENTIAL;

    if (dname) {
	ret = gss_import_name(minor_status, &dname->value, &dname->type, &name);
	if (ret) {
	    return ret;
	}
    }
    
    ret = gss_indicate_mechs(minor_status, &mechs);
    if (ret != GSS_S_COMPLETE) {
	gss_release_name(minor_status, &name);
	return ret;
    }

    /* Remove ourselves from this list */
    actual_desired_mechs.count = mechs->count;
    actual_desired_mechs.elements = malloc(actual_desired_mechs.count *
					   sizeof(gss_OID_desc));
    if (actual_desired_mechs.elements == NULL) {
	*minor_status = ENOMEM;
	ret = GSS_S_FAILURE;
	goto out;
    }

    for (i = 0, j = 0; i < mechs->count; i++) {
	if (gss_oid_equal(&mechs->elements[i], GSS_SPNEGO_MECHANISM))
	    continue;

	actual_desired_mechs.elements[j] = mechs->elements[i];
	j++;
    }
    actual_desired_mechs.count = j;

    ret = _gss_spnego_alloc_cred(minor_status, GSS_C_NO_CREDENTIAL,
				 &cred_handle);
    if (ret != GSS_S_COMPLETE)
	goto out;

    cred = (gssspnego_cred)cred_handle;
    ret = gss_acquire_cred(minor_status, name,
			   time_req, &actual_desired_mechs,
			   cred_usage,
			   &cred->negotiated_cred_id,
			   actual_mechs, time_rec);
    if (ret != GSS_S_COMPLETE)
	goto out;

    *output_cred_handle = cred_handle;

out:
    gss_release_name(minor_status, &name);
    gss_release_oid_set(&tmp, &mechs);
    if (actual_desired_mechs.elements != NULL) {
	free(actual_desired_mechs.elements);
    }
    if (ret != GSS_S_COMPLETE) {
	_gss_spnego_release_cred(&tmp, &cred_handle);
    }

    return ret;
}

OM_uint32 _gss_spnego_inquire_cred
           (OM_uint32 * minor_status,
            const gss_cred_id_t cred_handle,
            gss_name_t * name,
            OM_uint32 * lifetime,
            gss_cred_usage_t * cred_usage,
            gss_OID_set * mechanisms
           )
{
    gssspnego_cred cred;
    spnego_name sname = NULL;
    OM_uint32 ret;

    if (cred_handle == GSS_C_NO_CREDENTIAL) {
	*minor_status = 0;
	return GSS_S_NO_CRED;
    }

    if (name) {
	sname = calloc(1, sizeof(*sname));
	if (sname == NULL) {
	    *minor_status = ENOMEM;
	    return GSS_S_FAILURE;
	}
    }

    cred = (gssspnego_cred)cred_handle;

    ret = gss_inquire_cred(minor_status,
			   cred->negotiated_cred_id,
			   sname ? &sname->mech : NULL,
			   lifetime,
			   cred_usage,
			   mechanisms);
    if (ret) {
	if (sname)
	    free(sname);
	return ret;
    }
    if (name)
	*name = (gss_name_t)sname;

    return ret;
}

OM_uint32 _gss_spnego_add_cred (
            OM_uint32 * minor_status,
            const gss_cred_id_t input_cred_handle,
            const gss_name_t desired_name,
            const gss_OID desired_mech,
            gss_cred_usage_t cred_usage,
            OM_uint32 initiator_time_req,
            OM_uint32 acceptor_time_req,
            gss_cred_id_t * output_cred_handle,
            gss_OID_set * actual_mechs,
            OM_uint32 * initiator_time_rec,
            OM_uint32 * acceptor_time_rec
           )
{
    gss_cred_id_t spnego_output_cred_handle = GSS_C_NO_CREDENTIAL;
    OM_uint32 ret, tmp;
    gssspnego_cred input_cred, output_cred;

    *output_cred_handle = GSS_C_NO_CREDENTIAL;

    ret = _gss_spnego_alloc_cred(minor_status, GSS_C_NO_CREDENTIAL,
				 &spnego_output_cred_handle);
    if (ret)
	return ret;

    input_cred = (gssspnego_cred)input_cred_handle;
    output_cred = (gssspnego_cred)spnego_output_cred_handle;

    ret = gss_add_cred(minor_status,
		       input_cred->negotiated_cred_id,
		       desired_name,
		       desired_mech,
		       cred_usage,
		       initiator_time_req,
		       acceptor_time_req,
		       &output_cred->negotiated_cred_id,
		       actual_mechs,
		       initiator_time_rec,
		       acceptor_time_rec);
    if (ret) {
	_gss_spnego_release_cred(&tmp, &spnego_output_cred_handle);
	return ret;
    }

    *output_cred_handle = spnego_output_cred_handle;

    return GSS_S_COMPLETE;
}

OM_uint32 _gss_spnego_inquire_cred_by_mech (
            OM_uint32 * minor_status,
            const gss_cred_id_t cred_handle,
            const gss_OID mech_type,
            gss_name_t * name,
            OM_uint32 * initiator_lifetime,
            OM_uint32 * acceptor_lifetime,
            gss_cred_usage_t * cred_usage
           )
{
    gssspnego_cred cred;
    spnego_name sname = NULL;
    OM_uint32 ret;

    if (cred_handle == GSS_C_NO_CREDENTIAL) {
	*minor_status = 0;
	return GSS_S_NO_CRED;
    }

    if (name) {
	sname = calloc(1, sizeof(*sname));
	if (sname == NULL) {
	    *minor_status = ENOMEM;
	    return GSS_S_FAILURE;
	}
    }

    cred = (gssspnego_cred)cred_handle;

    ret = gss_inquire_cred_by_mech(minor_status,
				   cred->negotiated_cred_id,
				   mech_type,
				   sname ? &sname->mech : NULL,
				   initiator_lifetime,
				   acceptor_lifetime,
				   cred_usage);

    if (ret) {
	if (sname)
	    free(sname);
	return ret;
    }
    if (name)
	*name = (gss_name_t)sname;

    return GSS_S_COMPLETE;
}

OM_uint32 _gss_spnego_inquire_cred_by_oid
           (OM_uint32 * minor_status,
            const gss_cred_id_t cred_handle,
            const gss_OID desired_object,
            gss_buffer_set_t *data_set)
{
    gssspnego_cred cred;
    OM_uint32 ret;

    if (cred_handle == GSS_C_NO_CREDENTIAL) {
	*minor_status = 0;
	return GSS_S_NO_CRED;
    }
    cred = (gssspnego_cred)cred_handle;

    ret = gss_inquire_cred_by_oid(minor_status,
				  cred->negotiated_cred_id,
				  desired_object,
				  data_set);

    return ret;
}

