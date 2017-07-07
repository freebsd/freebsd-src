/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 1993 by OpenVision Technologies, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software
 * and its documentation for any purpose is hereby granted without fee,
 * provided that the above copyright notice appears in all copies and
 * that both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of OpenVision not be used
 * in advertising or publicity pertaining to distribution of the software
 * without specific, written prior permission. OpenVision makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied warranty.
 *
 * OPENVISION DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL OPENVISION BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
 * USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (c) 2006-2008, Novell, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *   * The copyright holder's name is not used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * $Id$
 */

#include "gssapiP_krb5.h"

OM_uint32 KRB5_CALLCONV
gss_krb5_get_tkt_flags(OM_uint32 *minor_status,
                       gss_ctx_id_t context_handle,
                       krb5_flags *ticket_flags)
{
    static const gss_OID_desc req_oid = {
        GSS_KRB5_GET_TKT_FLAGS_OID_LENGTH,
        GSS_KRB5_GET_TKT_FLAGS_OID };
    OM_uint32 major_status;
    gss_buffer_set_t data_set = GSS_C_NO_BUFFER_SET;

    if (ticket_flags == NULL)
        return GSS_S_CALL_INACCESSIBLE_WRITE;

    major_status = gss_inquire_sec_context_by_oid(minor_status,
                                                  context_handle,
                                                  (gss_OID)&req_oid,
                                                  &data_set);
    if (major_status != GSS_S_COMPLETE)
        return major_status;

    if (data_set == GSS_C_NO_BUFFER_SET ||
        data_set->count != 1 ||
        data_set->elements[0].length != sizeof(*ticket_flags)) {
        *minor_status = EINVAL;
        return GSS_S_FAILURE;
    }

    *ticket_flags = *((krb5_flags *)data_set->elements[0].value);

    gss_release_buffer_set(minor_status, &data_set);

    *minor_status = 0;

    return GSS_S_COMPLETE;
}

OM_uint32 KRB5_CALLCONV
gss_krb5_copy_ccache(OM_uint32 *minor_status,
                     gss_cred_id_t cred_handle,
                     krb5_ccache out_ccache)
{
    static const gss_OID_desc req_oid = {
        GSS_KRB5_COPY_CCACHE_OID_LENGTH,
        GSS_KRB5_COPY_CCACHE_OID };
    OM_uint32 major_status;
    gss_buffer_desc req_buffer;

    if (out_ccache == NULL)
        return GSS_S_CALL_INACCESSIBLE_WRITE;

    req_buffer.value = out_ccache;
    req_buffer.length = sizeof(out_ccache);

    major_status = gss_set_cred_option(minor_status,
                                       &cred_handle,
                                       (gss_OID)&req_oid,
                                       &req_buffer);

    return major_status;
}

OM_uint32 KRB5_CALLCONV
gss_krb5_import_cred(OM_uint32 *minor_status,
                     krb5_ccache id,
                     krb5_principal keytab_principal,
                     krb5_keytab keytab,
                     gss_cred_id_t *cred)
{
    static const gss_OID_desc req_oid = {
        GSS_KRB5_IMPORT_CRED_OID_LENGTH,
        GSS_KRB5_IMPORT_CRED_OID };
    OM_uint32 major_status;
    struct krb5_gss_import_cred_req req;
    gss_buffer_desc req_buffer;

    if (cred == NULL)
        return GSS_S_CALL_INACCESSIBLE_WRITE;

    *cred = GSS_C_NO_CREDENTIAL;

    req.id = id;
    req.keytab_principal = keytab_principal;
    req.keytab = keytab;

    req_buffer.value = &req;
    req_buffer.length = sizeof(req);

    major_status = gss_set_cred_option(minor_status,
                                       cred,
                                       (gss_OID)&req_oid,
                                       &req_buffer);

    return major_status;
}

OM_uint32 KRB5_CALLCONV
gss_krb5_export_lucid_sec_context(OM_uint32 *minor_status,
                                  gss_ctx_id_t *context_handle,
                                  OM_uint32 version,
                                  void **kctx)
{
    unsigned char oid_buf[GSS_KRB5_EXPORT_LUCID_SEC_CONTEXT_OID_LENGTH + 6];
    gss_OID_desc req_oid;
    OM_uint32 major_status, minor;
    gss_buffer_set_t data_set = GSS_C_NO_BUFFER_SET;

    if (kctx == NULL)
        return GSS_S_CALL_INACCESSIBLE_WRITE;

    *kctx = NULL;

    req_oid.elements = oid_buf;
    req_oid.length = sizeof(oid_buf);

    major_status = generic_gss_oid_compose(minor_status,
                                           GSS_KRB5_EXPORT_LUCID_SEC_CONTEXT_OID,
                                           GSS_KRB5_EXPORT_LUCID_SEC_CONTEXT_OID_LENGTH,
                                           (int)version,
                                           &req_oid);
    if (GSS_ERROR(major_status))
        return major_status;

    major_status = gss_inquire_sec_context_by_oid(minor_status,
                                                  *context_handle,
                                                  &req_oid,
                                                  &data_set);
    if (GSS_ERROR(major_status))
        return major_status;

    if (data_set == GSS_C_NO_BUFFER_SET ||
        data_set->count != 1 ||
        data_set->elements[0].length != sizeof(void *)) {
        *minor_status = EINVAL;
        return GSS_S_FAILURE;
    }

    *kctx = *((void **)data_set->elements[0].value);

    /* Clean up the context state (it is an error for
     * someone to attempt to use this context again)
     */
    (void)gss_delete_sec_context(minor_status, context_handle, NULL);
    *context_handle = GSS_C_NO_CONTEXT;

    generic_gss_release_buffer_set(&minor, &data_set);

    return GSS_S_COMPLETE;
}

OM_uint32 KRB5_CALLCONV
gss_krb5_set_allowable_enctypes(OM_uint32 *minor_status,
                                gss_cred_id_t cred,
                                OM_uint32 num_ktypes,
                                krb5_enctype *ktypes)
{
    static const gss_OID_desc req_oid = {
        GSS_KRB5_SET_ALLOWABLE_ENCTYPES_OID_LENGTH,
        GSS_KRB5_SET_ALLOWABLE_ENCTYPES_OID };
    OM_uint32 major_status;
    struct krb5_gss_set_allowable_enctypes_req req;
    gss_buffer_desc req_buffer;

    req.num_ktypes = num_ktypes;
    req.ktypes = ktypes;

    req_buffer.length = sizeof(req);
    req_buffer.value = &req;

    major_status = gss_set_cred_option(minor_status,
                                       &cred,
                                       (gss_OID)&req_oid,
                                       &req_buffer);

    return major_status;
}

OM_uint32 KRB5_CALLCONV
gss_krb5_ccache_name(OM_uint32 *minor_status,
                     const char *name,
                     const char **out_name)
{
    static const gss_OID_desc req_oid = {
        GSS_KRB5_CCACHE_NAME_OID_LENGTH,
        GSS_KRB5_CCACHE_NAME_OID };
    OM_uint32 major_status;
    struct krb5_gss_ccache_name_req req;
    gss_buffer_desc req_buffer;

    req.name = name;
    req.out_name = out_name;

    req_buffer.length = sizeof(req);
    req_buffer.value = &req;

    major_status = gssspi_mech_invoke(minor_status,
                                      (gss_OID)gss_mech_krb5,
                                      (gss_OID)&req_oid,
                                      &req_buffer);

    return major_status;
}

OM_uint32 KRB5_CALLCONV
gss_krb5_free_lucid_sec_context(OM_uint32 *minor_status, void *kctx)
{
    static const gss_OID_desc req_oid = {
        GSS_KRB5_FREE_LUCID_SEC_CONTEXT_OID_LENGTH,
        GSS_KRB5_FREE_LUCID_SEC_CONTEXT_OID };
    OM_uint32 major_status;
    gss_buffer_desc req_buffer;

    req_buffer.length = sizeof(kctx);
    req_buffer.value = kctx;

    major_status = gssspi_mech_invoke(minor_status,
                                      (gss_OID)gss_mech_krb5,
                                      (gss_OID)&req_oid,
                                      &req_buffer);

    return major_status;
}

OM_uint32 KRB5_CALLCONV
krb5_gss_register_acceptor_identity(const char *keytab)
{
    static const gss_OID_desc req_oid = {
        GSS_KRB5_REGISTER_ACCEPTOR_IDENTITY_OID_LENGTH,
        GSS_KRB5_REGISTER_ACCEPTOR_IDENTITY_OID };
    OM_uint32 major_status;
    OM_uint32 minor_status;
    gss_buffer_desc req_buffer;

    req_buffer.length = (keytab == NULL) ? 0 : strlen(keytab);
    req_buffer.value = (char *)keytab;

    major_status = gssspi_mech_invoke(&minor_status,
                                      (gss_OID)gss_mech_krb5,
                                      (gss_OID)&req_oid,
                                      &req_buffer);

    return major_status;
}

#ifndef _WIN32
krb5_error_code
krb5_gss_use_kdc_context(void)
{
    static const gss_OID_desc req_oid = {
        GSS_KRB5_USE_KDC_CONTEXT_OID_LENGTH,
        GSS_KRB5_USE_KDC_CONTEXT_OID };
    OM_uint32 major_status;
    OM_uint32 minor_status;
    gss_buffer_desc req_buffer;
    krb5_error_code ret;

    req_buffer.length = 0;
    req_buffer.value = NULL;

    major_status = gssspi_mech_invoke(&minor_status,
                                      (gss_OID)gss_mech_krb5,
                                      (gss_OID)&req_oid,
                                      &req_buffer);

    if (major_status != GSS_S_COMPLETE) {
        if (minor_status != 0)
            ret = (krb5_error_code)minor_status;
        else
            ret = KRB5KRB_ERR_GENERIC;
    } else
        ret = 0;

    return ret;
}
#endif

/*
 * This API should go away and be replaced with an accessor
 * into a gss_name_t.
 */
OM_uint32 KRB5_CALLCONV
gsskrb5_extract_authz_data_from_sec_context(OM_uint32 *minor_status,
                                            const gss_ctx_id_t context_handle,
                                            int ad_type,
                                            gss_buffer_t ad_data)
{
    gss_OID_desc req_oid;
    unsigned char oid_buf[GSS_KRB5_EXTRACT_AUTHZ_DATA_FROM_SEC_CONTEXT_OID_LENGTH + 6];
    OM_uint32 major_status;
    gss_buffer_set_t data_set = GSS_C_NO_BUFFER_SET;

    if (ad_data == NULL)
        return GSS_S_CALL_INACCESSIBLE_WRITE;

    req_oid.elements = oid_buf;
    req_oid.length = sizeof(oid_buf);

    major_status = generic_gss_oid_compose(minor_status,
                                           GSS_KRB5_EXTRACT_AUTHZ_DATA_FROM_SEC_CONTEXT_OID,
                                           GSS_KRB5_EXTRACT_AUTHZ_DATA_FROM_SEC_CONTEXT_OID_LENGTH,
                                           ad_type,
                                           &req_oid);
    if (GSS_ERROR(major_status))
        return major_status;

    major_status = gss_inquire_sec_context_by_oid(minor_status,
                                                  context_handle,
                                                  (gss_OID)&req_oid,
                                                  &data_set);
    if (major_status != GSS_S_COMPLETE) {
        return major_status;
    }

    if (data_set == GSS_C_NO_BUFFER_SET ||
        data_set->count != 1) {
        return GSS_S_FAILURE;
    }

    ad_data->length = data_set->elements[0].length;
    ad_data->value = data_set->elements[0].value;

    data_set->elements[0].length = 0;
    data_set->elements[0].value = NULL;

    data_set->count = 0;

    gss_release_buffer_set(minor_status, &data_set);

    return GSS_S_COMPLETE;
}

OM_uint32 KRB5_CALLCONV
gss_krb5_set_cred_rcache(OM_uint32 *minor_status,
                         gss_cred_id_t cred,
                         krb5_rcache rcache)
{
    static const gss_OID_desc req_oid = {
        GSS_KRB5_SET_CRED_RCACHE_OID_LENGTH,
        GSS_KRB5_SET_CRED_RCACHE_OID };
    OM_uint32 major_status;
    gss_buffer_desc req_buffer;

    req_buffer.length = sizeof(rcache);
    req_buffer.value = rcache;

    major_status = gss_set_cred_option(minor_status,
                                       &cred,
                                       (gss_OID)&req_oid,
                                       &req_buffer);

    return major_status;
}

OM_uint32 KRB5_CALLCONV
gsskrb5_extract_authtime_from_sec_context(OM_uint32 *minor_status,
                                          gss_ctx_id_t context_handle,
                                          krb5_timestamp *authtime)
{
    static const gss_OID_desc req_oid = {
        GSS_KRB5_EXTRACT_AUTHTIME_FROM_SEC_CONTEXT_OID_LENGTH,
        GSS_KRB5_EXTRACT_AUTHTIME_FROM_SEC_CONTEXT_OID };
    OM_uint32 major_status;
    gss_buffer_set_t data_set = GSS_C_NO_BUFFER_SET;

    if (authtime == NULL)
        return GSS_S_CALL_INACCESSIBLE_WRITE;

    major_status = gss_inquire_sec_context_by_oid(minor_status,
                                                  context_handle,
                                                  (gss_OID)&req_oid,
                                                  &data_set);
    if (major_status != GSS_S_COMPLETE)
        return major_status;

    if (data_set == GSS_C_NO_BUFFER_SET ||
        data_set->count != 1 ||
        data_set->elements[0].length != sizeof(*authtime)) {
        *minor_status = EINVAL;
        return GSS_S_FAILURE;
    }

    *authtime = *((krb5_timestamp *)data_set->elements[0].value);

    gss_release_buffer_set(minor_status, &data_set);

    *minor_status = 0;

    return GSS_S_COMPLETE;
}
