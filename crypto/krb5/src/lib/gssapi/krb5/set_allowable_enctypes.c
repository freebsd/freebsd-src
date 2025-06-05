/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/gssapi/krb5/set_allowable_enctypes.c */
/*
 * Copyright 2004  by the Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */

/*
 * krb5_gss_set_allowable_enctypes()
 */

/*
 * gss_krb5_set_allowable_enctypes
 *
 * This function may be called by a context initiator after calling
 * gss_acquire_cred(), but before calling gss_init_sec_context(),
 * to restrict the set of enctypes which will be negotiated during
 * context establishment to those in the provided array.
 *
 * 'cred_handle' must be a valid credential handle obtained via
 * gss_acquire_cred().  It may not be GSS_C_NO_CREDENTIAL.
 * gss_acquire_cred() may be called with GSS_C_NO_CREDENTIAL
 * to get a handle to the default credential.
 *
 * The purpose of this function is to limit the keys that may
 * be exported via gss_krb5_export_lucid_sec_context(); thus it
 * should limit the enctypes of all keys that will be needed
 * after the security context has been established.
 * (i.e. context establishment may use a session key with a
 * stronger enctype than in the provided array, however a
 * subkey must be established within the enctype limits
 * established by this function.)
 *
 */

#include "gssapiP_krb5.h"
#ifdef HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#include "gssapi_krb5.h"

OM_uint32
gss_krb5int_set_allowable_enctypes(OM_uint32 *minor_status,
                                   gss_cred_id_t *cred_handle,
                                   const gss_OID desired_oid,
                                   const gss_buffer_t value)
{
    unsigned int i, j;
    krb5_enctype * new_ktypes;
    OM_uint32 major_status;
    krb5_gss_cred_id_t cred;
    krb5_error_code kerr = 0;
    struct krb5_gss_set_allowable_enctypes_req *req;

    /* Assume a failure */
    *minor_status = 0;
    major_status = GSS_S_FAILURE;

    assert(value->length == sizeof(*req));
    req = (struct krb5_gss_set_allowable_enctypes_req *)value->value;

    /* verify and valildate cred handle */
    cred = (krb5_gss_cred_id_t) *cred_handle;

    if (req->ktypes == NULL) {
        k5_mutex_lock(&cred->lock);
        if (cred->req_enctypes)
            free(cred->req_enctypes);
        cred->req_enctypes = NULL;
        k5_mutex_unlock(&cred->lock);
        return GSS_S_COMPLETE;
    }

    /* Copy the requested enctypes into the cred structure.  Filter out the
     * ones we don't consider valid.  Error out if no enctypes are valid. */
    new_ktypes = k5calloc(req->num_ktypes + 1, sizeof(*new_ktypes), &kerr);
    if (new_ktypes == NULL)
        goto error_out;
    for (i = 0, j = 0; i < req->num_ktypes && req->ktypes[i]; i++) {
        if (krb5_c_valid_enctype(req->ktypes[i]))
            new_ktypes[j++] = req->ktypes[i];
    }
    new_ktypes[j] = 0;
    if (j == 0) {
        free(new_ktypes);
        kerr = KRB5_PROG_ETYPE_NOSUPP;
        goto error_out;
    }
    k5_mutex_lock(&cred->lock);
    if (cred->req_enctypes)
        free(cred->req_enctypes);
    cred->req_enctypes = new_ktypes;
    k5_mutex_unlock(&cred->lock);

    /* Success! */
    return GSS_S_COMPLETE;

error_out:
    *minor_status = kerr;
    return(major_status);
}
