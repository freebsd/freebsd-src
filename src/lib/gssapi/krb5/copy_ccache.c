/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include "gssapiP_krb5.h"

OM_uint32
gss_krb5int_copy_ccache(OM_uint32 *minor_status,
                        gss_cred_id_t *cred_handle,
                        const gss_OID desired_object,
                        const gss_buffer_t value)
{
    krb5_gss_cred_id_t k5creds;
    krb5_error_code code;
    krb5_context context;
    krb5_ccache out_ccache;

    assert(value->length == sizeof(out_ccache));

    if (value->length != sizeof(out_ccache))
        return GSS_S_FAILURE;

    out_ccache = (krb5_ccache)value->value;

    /* cred handle will have been validated by gssspi_set_cred_option() */
    k5creds = (krb5_gss_cred_id_t) *cred_handle;
    k5_mutex_lock(&k5creds->lock);
    if (k5creds->usage == GSS_C_ACCEPT) {
        k5_mutex_unlock(&k5creds->lock);
        *minor_status = (OM_uint32) G_BAD_USAGE;
        return(GSS_S_FAILURE);
    }

    code = krb5_gss_init_context(&context);
    if (code) {
        k5_mutex_unlock(&k5creds->lock);
        *minor_status = code;
        return GSS_S_FAILURE;
    }

    code = krb5_cc_copy_creds(context, k5creds->ccache, out_ccache);
    if (code) {
        k5_mutex_unlock(&k5creds->lock);
        *minor_status = code;
        save_error_info(*minor_status, context);
        krb5_free_context(context);
        return(GSS_S_FAILURE);
    }
    k5_mutex_unlock(&k5creds->lock);
    *minor_status = code;
    if (code)
        save_error_info(*minor_status, context);
    krb5_free_context(context);
    return code ? GSS_S_FAILURE : GSS_S_COMPLETE;
}
