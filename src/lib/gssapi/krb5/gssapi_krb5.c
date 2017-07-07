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
 * Copyright (C) 1998 by the FundsXpress, INC.
 *
 * All rights reserved.
 *
 * Export of this software from the United States of America may require
 * a specific license from the United States Government.  It is the
 * responsibility of any person or organization contemplating export to
 * obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of FundsXpress. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  FundsXpress makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
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


/* For declaration of krb5_ser_context_init */
#include "k5-int.h"
#include "gssapiP_krb5.h"
#include "mglueP.h"

#ifndef NO_PASSWORD
#include <pwd.h>
#endif

/** exported constants defined in gssapi_krb5{,_nx}.h **/

/* these are bogus, but will compile */

/*
 * The OID of the draft krb5 mechanism, assigned by IETF, is:
 *      iso(1) org(3) dod(5) internet(1) security(5)
 *      kerberosv5(2) = 1.3.5.1.5.2
 * The OID of the krb5_name type is:
 *      iso(1) member-body(2) US(840) mit(113554) infosys(1) gssapi(2)
 *      krb5(2) krb5_name(1) = 1.2.840.113554.1.2.2.1
 * The OID of the krb5_principal type is:
 *      iso(1) member-body(2) US(840) mit(113554) infosys(1) gssapi(2)
 *      krb5(2) krb5_principal(2) = 1.2.840.113554.1.2.2.2
 * The OID of the proposed standard krb5 mechanism is:
 *      iso(1) member-body(2) US(840) mit(113554) infosys(1) gssapi(2)
 *      krb5(2) = 1.2.840.113554.1.2.2
 * The OID of the proposed standard krb5 v2 mechanism is:
 *      iso(1) member-body(2) US(840) mit(113554) infosys(1) gssapi(2)
 *      krb5v2(3) = 1.2.840.113554.1.2.3
 * Provisionally reserved for Kerberos session key algorithm
 * identifiers is:
 *      iso(1) member-body(2) US(840) mit(113554) infosys(1) gssapi(2)
 *      krb5(2) krb5_enctype(4) = 1.2.840.113554.1.2.2.4
 * Provisionally reserved for Kerberos mechanism-specific APIs:
 *      iso(1) member-body(2) US(840) mit(113554) infosys(1) gssapi(2)
 *      krb5(2) krb5_gssapi_ext(5) = 1.2.840.113554.1.2.2.5
 */

/*
 * Encoding rules: The first two values are encoded in one byte as 40
 * * value1 + value2.  Subsequent values are encoded base 128, most
 * significant digit first, with the high bit (\200) set on all octets
 * except the last in each value's encoding.
 */

#define NO_CI_FLAGS_X_OID_LENGTH 6
#define NO_CI_FLAGS_X_OID "\x2a\x85\x70\x2b\x0d\x1d"

const gss_OID_desc krb5_gss_oid_array[] = {
    /* this is the official, rfc-specified OID */
    {GSS_MECH_KRB5_OID_LENGTH, GSS_MECH_KRB5_OID},
    /* this pre-RFC mech OID */
    {GSS_MECH_KRB5_OLD_OID_LENGTH, GSS_MECH_KRB5_OLD_OID},
    /* this is the unofficial, incorrect mech OID emitted by MS */
    {GSS_MECH_KRB5_WRONG_OID_LENGTH, GSS_MECH_KRB5_WRONG_OID},
    /* IAKERB OID */
    {GSS_MECH_IAKERB_OID_LENGTH, GSS_MECH_IAKERB_OID},
    /* this is the v2 assigned OID */
    {9, "\052\206\110\206\367\022\001\002\003"},
    /* these two are name type OID's */
    /* 2.1.1. Kerberos Principal Name Form:  (rfc 1964)
     * This name form shall be represented by the Object Identifier {iso(1)
     * member-body(2) United States(840) mit(113554) infosys(1) gssapi(2)
     * krb5(2) krb5_name(1)}.  The recommended symbolic name for this type
     * is "GSS_KRB5_NT_PRINCIPAL_NAME". */
    {10, "\052\206\110\206\367\022\001\002\002\001"},
    /* gss_nt_krb5_principal.  Object identifier for a krb5_principal. Do not use. */
    {10, "\052\206\110\206\367\022\001\002\002\002"},
    {NO_CI_FLAGS_X_OID_LENGTH, NO_CI_FLAGS_X_OID},
    { 0, 0 }
};

#define kg_oids ((gss_OID)krb5_gss_oid_array)

const gss_OID gss_mech_krb5             = &kg_oids[0];
const gss_OID gss_mech_krb5_old         = &kg_oids[1];
const gss_OID gss_mech_krb5_wrong       = &kg_oids[2];
const gss_OID gss_mech_iakerb           = &kg_oids[3];


const gss_OID gss_nt_krb5_name                  = &kg_oids[5];
const gss_OID gss_nt_krb5_principal             = &kg_oids[6];
const gss_OID GSS_KRB5_NT_PRINCIPAL_NAME        = &kg_oids[5];

const gss_OID GSS_KRB5_CRED_NO_CI_FLAGS_X       = &kg_oids[7];

static const gss_OID_set_desc oidsets[] = {
    {1, &kg_oids[0]}, /* RFC OID */
    {1, &kg_oids[1]}, /* pre-RFC OID */
    {3, &kg_oids[0]}, /* all names for krb5 mech */
    {4, &kg_oids[0]}, /* all krb5 names and IAKERB */
};

#define kg_oidsets ((gss_OID_set)oidsets)

const gss_OID_set gss_mech_set_krb5             = &kg_oidsets[0];
const gss_OID_set gss_mech_set_krb5_old         = &kg_oidsets[1];
const gss_OID_set gss_mech_set_krb5_both        = &kg_oidsets[2];
const gss_OID_set kg_all_mechs                  = &kg_oidsets[3];

g_set kg_vdb = G_SET_INIT;

/** default credential support */

/*
 * init_sec_context() will explicitly re-acquire default credentials,
 * so handling the expiration/invalidation condition here isn't needed.
 */
OM_uint32
kg_get_defcred(minor_status, cred)
    OM_uint32 *minor_status;
    gss_cred_id_t *cred;
{
    OM_uint32 major;

    if ((major = krb5_gss_acquire_cred(minor_status,
                                       (gss_name_t) NULL, GSS_C_INDEFINITE,
                                       GSS_C_NULL_OID_SET, GSS_C_INITIATE,
                                       cred, NULL, NULL)) && GSS_ERROR(major)) {
        return(major);
    }
    *minor_status = 0;
    return(GSS_S_COMPLETE);
}

OM_uint32
kg_sync_ccache_name (krb5_context context, OM_uint32 *minor_status)
{
    OM_uint32 err = 0;

    /*
     * Sync up the context ccache name with the GSSAPI ccache name.
     * If kg_ccache_name is NULL -- normal unless someone has called
     * gss_krb5_ccache_name() -- then the system default ccache will
     * be picked up and used by resetting the context default ccache.
     * This is needed for platforms which support multiple ccaches.
     */

    if (!err) {
        /* if NULL, resets the context default ccache */
        err = krb5_cc_set_default_name(context,
                                       (char *) k5_getspecific(K5_KEY_GSS_KRB5_CCACHE_NAME));
    }

    *minor_status = err;
    return (*minor_status == 0) ? GSS_S_COMPLETE : GSS_S_FAILURE;
}

/* This function returns whether or not the caller set a cccache name.  Used by
 * gss_acquire_cred to figure out if the caller wants to only look at this
 * ccache or search the cache collection for the desired name */
OM_uint32
kg_caller_provided_ccache_name (OM_uint32 *minor_status,
                                int *out_caller_provided_name)
{
    if (out_caller_provided_name) {
        *out_caller_provided_name =
            (k5_getspecific(K5_KEY_GSS_KRB5_CCACHE_NAME) != NULL);
    }

    *minor_status = 0;
    return GSS_S_COMPLETE;
}

OM_uint32
kg_get_ccache_name (OM_uint32 *minor_status, const char **out_name)
{
    const char *name = NULL;
    OM_uint32 err = 0;
    char *kg_ccache_name;

    kg_ccache_name = k5_getspecific(K5_KEY_GSS_KRB5_CCACHE_NAME);

    if (kg_ccache_name != NULL) {
        name = strdup(kg_ccache_name);
        if (name == NULL)
            err = ENOMEM;
    } else {
        krb5_context context = NULL;

        /* Reset the context default ccache (see text above), and then
           retrieve it.  */
        err = krb5_gss_init_context(&context);
        if (!err)
            err = krb5_cc_set_default_name (context, NULL);
        if (!err) {
            name = krb5_cc_default_name(context);
            if (name) {
                name = strdup(name);
                if (name == NULL)
                    err = ENOMEM;
            }
        }
        if (err && context)
            save_error_info(err, context);
        if (context)
            krb5_free_context(context);
    }

    if (!err) {
        if (out_name) {
            *out_name = name;
        }
    }

    *minor_status = err;
    return (*minor_status == 0) ? GSS_S_COMPLETE : GSS_S_FAILURE;
}

OM_uint32
kg_set_ccache_name (OM_uint32 *minor_status, const char *name)
{
    char *new_name = NULL;
    char *swap = NULL;
    char *kg_ccache_name;
    krb5_error_code kerr;

    if (name) {
        new_name = strdup(name);
        if (new_name == NULL) {
            *minor_status = ENOMEM;
            return GSS_S_FAILURE;
        }
    }

    kg_ccache_name = k5_getspecific(K5_KEY_GSS_KRB5_CCACHE_NAME);
    swap = kg_ccache_name;
    kg_ccache_name = new_name;
    new_name = swap;
    kerr = k5_setspecific(K5_KEY_GSS_KRB5_CCACHE_NAME, kg_ccache_name);
    if (kerr != 0) {
        /* Can't store, so free up the storage.  */
        free(kg_ccache_name);
        /* ??? free(new_name); */
        *minor_status = kerr;
        return GSS_S_FAILURE;
    }

    free (new_name);
    *minor_status = 0;
    return GSS_S_COMPLETE;
}

#define g_OID_prefix_equal(o1, o2)                                      \
    (((o1)->length >= (o2)->length) &&                                  \
     (memcmp((o1)->elements, (o2)->elements, (o2)->length) == 0))

/*
 * gss_inquire_sec_context_by_oid() methods
 */
static struct {
    gss_OID_desc oid;
    OM_uint32 (*func)(OM_uint32 *, const gss_ctx_id_t, const gss_OID, gss_buffer_set_t *);
} krb5_gss_inquire_sec_context_by_oid_ops[] = {
    {
        {GSS_KRB5_GET_TKT_FLAGS_OID_LENGTH, GSS_KRB5_GET_TKT_FLAGS_OID},
        gss_krb5int_get_tkt_flags
    },
    {
        {GSS_KRB5_EXTRACT_AUTHZ_DATA_FROM_SEC_CONTEXT_OID_LENGTH, GSS_KRB5_EXTRACT_AUTHZ_DATA_FROM_SEC_CONTEXT_OID},
        gss_krb5int_extract_authz_data_from_sec_context
    },
    {
        {GSS_KRB5_INQ_SSPI_SESSION_KEY_OID_LENGTH, GSS_KRB5_INQ_SSPI_SESSION_KEY_OID},
        gss_krb5int_inq_session_key
    },
    {
        {GSS_KRB5_EXPORT_LUCID_SEC_CONTEXT_OID_LENGTH, GSS_KRB5_EXPORT_LUCID_SEC_CONTEXT_OID},
        gss_krb5int_export_lucid_sec_context
    },
    {
        {GSS_KRB5_EXTRACT_AUTHTIME_FROM_SEC_CONTEXT_OID_LENGTH, GSS_KRB5_EXTRACT_AUTHTIME_FROM_SEC_CONTEXT_OID},
        gss_krb5int_extract_authtime_from_sec_context
    }
};

OM_uint32 KRB5_CALLCONV
krb5_gss_inquire_sec_context_by_oid (OM_uint32 *minor_status,
                                     const gss_ctx_id_t context_handle,
                                     const gss_OID desired_object,
                                     gss_buffer_set_t *data_set)
{
    krb5_gss_ctx_id_rec *ctx;
    size_t i;

    if (minor_status == NULL)
        return GSS_S_CALL_INACCESSIBLE_WRITE;

    *minor_status = 0;

    if (desired_object == GSS_C_NO_OID)
        return GSS_S_CALL_INACCESSIBLE_READ;

    if (data_set == NULL)
        return GSS_S_CALL_INACCESSIBLE_WRITE;

    *data_set = GSS_C_NO_BUFFER_SET;

    ctx = (krb5_gss_ctx_id_rec *) context_handle;

    if (ctx->terminated || !ctx->established)
        return GSS_S_NO_CONTEXT;

    for (i = 0; i < sizeof(krb5_gss_inquire_sec_context_by_oid_ops)/
             sizeof(krb5_gss_inquire_sec_context_by_oid_ops[0]); i++) {
        if (g_OID_prefix_equal(desired_object, &krb5_gss_inquire_sec_context_by_oid_ops[i].oid)) {
            return (*krb5_gss_inquire_sec_context_by_oid_ops[i].func)(minor_status,
                                                                      context_handle,
                                                                      desired_object,
                                                                      data_set);
        }
    }

    *minor_status = EINVAL;

    return GSS_S_UNAVAILABLE;
}

/*
 * gss_inquire_cred_by_oid() methods
 */
#if 0
static struct {
    gss_OID_desc oid;
    OM_uint32 (*func)(OM_uint32 *, const gss_cred_id_t, const gss_OID, gss_buffer_set_t *);
} krb5_gss_inquire_cred_by_oid_ops[] = {
};
#endif

static OM_uint32 KRB5_CALLCONV
krb5_gss_inquire_cred_by_oid(OM_uint32 *minor_status,
                             const gss_cred_id_t cred_handle,
                             const gss_OID desired_object,
                             gss_buffer_set_t *data_set)
{
    OM_uint32 major_status = GSS_S_FAILURE;
#if 0
    size_t i;
#endif

    if (minor_status == NULL)
        return GSS_S_CALL_INACCESSIBLE_WRITE;

    *minor_status = 0;

    if (desired_object == GSS_C_NO_OID)
        return GSS_S_CALL_INACCESSIBLE_READ;

    if (data_set == NULL)
        return GSS_S_CALL_INACCESSIBLE_WRITE;

    *data_set = GSS_C_NO_BUFFER_SET;
    if (cred_handle == GSS_C_NO_CREDENTIAL) {
        *minor_status = (OM_uint32)KRB5_NOCREDS_SUPPLIED;
        return GSS_S_NO_CRED;
    }

    major_status = krb5_gss_validate_cred(minor_status, cred_handle);
    if (GSS_ERROR(major_status))
        return major_status;

#if 0
    for (i = 0; i < sizeof(krb5_gss_inquire_cred_by_oid_ops)/
             sizeof(krb5_gss_inquire_cred_by_oid_ops[0]); i++) {
        if (g_OID_prefix_equal(desired_object, &krb5_gss_inquire_cred_by_oid_ops[i].oid)) {
            return (*krb5_gss_inquire_cred_by_oid_ops[i].func)(minor_status,
                                                               cred_handle,
                                                               desired_object,
                                                               data_set);
        }
    }
#endif

    *minor_status = EINVAL;

    return GSS_S_UNAVAILABLE;
}

/*
 * gss_set_sec_context_option() methods
 * (Disabled until we have something to populate the array.)
 */
#if 0
static struct {
    gss_OID_desc oid;
    OM_uint32 (*func)(OM_uint32 *, gss_ctx_id_t *, const gss_OID, const gss_buffer_t);
} krb5_gss_set_sec_context_option_ops[] = {
};
#endif

OM_uint32 KRB5_CALLCONV
krb5_gss_set_sec_context_option (OM_uint32 *minor_status,
                                 gss_ctx_id_t *context_handle,
                                 const gss_OID desired_object,
                                 const gss_buffer_t value)
{
#if 0
    size_t i;
#endif

    if (minor_status == NULL)
        return GSS_S_CALL_INACCESSIBLE_WRITE;

    *minor_status = 0;

    if (context_handle == NULL)
        return GSS_S_CALL_INACCESSIBLE_READ;

    if (desired_object == GSS_C_NO_OID)
        return GSS_S_CALL_INACCESSIBLE_READ;

#if 0
    for (i = 0; i < sizeof(krb5_gss_set_sec_context_option_ops)/
             sizeof(krb5_gss_set_sec_context_option_ops[0]); i++) {
        if (g_OID_prefix_equal(desired_object, &krb5_gss_set_sec_context_option_ops[i].oid)) {
            return (*krb5_gss_set_sec_context_option_ops[i].func)(minor_status,
                                                                  context_handle,
                                                                  desired_object,
                                                                  value);
        }
    }
#endif

    *minor_status = EINVAL;

    return GSS_S_UNAVAILABLE;
}

static OM_uint32
no_ci_flags(OM_uint32 *minor_status,
            gss_cred_id_t *cred_handle,
            const gss_OID desired_oid,
            const gss_buffer_t value)
{
    krb5_gss_cred_id_t cred;

    cred = (krb5_gss_cred_id_t) *cred_handle;
    cred->suppress_ci_flags = 1;

    *minor_status = 0;
    return GSS_S_COMPLETE;
}
/*
 * gssspi_set_cred_option() methods
 */
static struct {
    gss_OID_desc oid;
    OM_uint32 (*func)(OM_uint32 *, gss_cred_id_t *, const gss_OID, const gss_buffer_t);
} krb5_gssspi_set_cred_option_ops[] = {
    {
        {GSS_KRB5_COPY_CCACHE_OID_LENGTH, GSS_KRB5_COPY_CCACHE_OID},
        gss_krb5int_copy_ccache
    },
    {
        {GSS_KRB5_SET_ALLOWABLE_ENCTYPES_OID_LENGTH, GSS_KRB5_SET_ALLOWABLE_ENCTYPES_OID},
        gss_krb5int_set_allowable_enctypes
    },
    {
        {GSS_KRB5_SET_CRED_RCACHE_OID_LENGTH, GSS_KRB5_SET_CRED_RCACHE_OID},
        gss_krb5int_set_cred_rcache
    },
    {
        {GSS_KRB5_IMPORT_CRED_OID_LENGTH, GSS_KRB5_IMPORT_CRED_OID},
        gss_krb5int_import_cred
    },
    {
        {NO_CI_FLAGS_X_OID_LENGTH, NO_CI_FLAGS_X_OID},
        no_ci_flags
    },
};

static OM_uint32 KRB5_CALLCONV
krb5_gssspi_set_cred_option(OM_uint32 *minor_status,
                            gss_cred_id_t *cred_handle,
                            const gss_OID desired_object,
                            const gss_buffer_t value)
{
    OM_uint32 major_status = GSS_S_FAILURE;
    size_t i;

    if (minor_status == NULL)
        return GSS_S_CALL_INACCESSIBLE_WRITE;

    if (cred_handle == NULL)
        return GSS_S_CALL_INACCESSIBLE_WRITE;

    *minor_status = 0;

    if (desired_object == GSS_C_NO_OID)
        return GSS_S_CALL_INACCESSIBLE_READ;

    if (*cred_handle != GSS_C_NO_CREDENTIAL) {
        major_status = krb5_gss_validate_cred(minor_status, *cred_handle);
        if (GSS_ERROR(major_status))
            return major_status;
    }

    for (i = 0; i < sizeof(krb5_gssspi_set_cred_option_ops)/
             sizeof(krb5_gssspi_set_cred_option_ops[0]); i++) {
        if (g_OID_prefix_equal(desired_object, &krb5_gssspi_set_cred_option_ops[i].oid)) {
            return (*krb5_gssspi_set_cred_option_ops[i].func)(minor_status,
                                                              cred_handle,
                                                              desired_object,
                                                              value);
        }
    }

    *minor_status = EINVAL;

    return GSS_S_UNAVAILABLE;
}

/*
 * gssspi_mech_invoke() methods
 */
static struct {
    gss_OID_desc oid;
    OM_uint32 (*func)(OM_uint32 *, const gss_OID, const gss_OID, gss_buffer_t);
} krb5_gssspi_mech_invoke_ops[] = {
    {
        {GSS_KRB5_REGISTER_ACCEPTOR_IDENTITY_OID_LENGTH, GSS_KRB5_REGISTER_ACCEPTOR_IDENTITY_OID},
        gss_krb5int_register_acceptor_identity
    },
    {
        {GSS_KRB5_CCACHE_NAME_OID_LENGTH, GSS_KRB5_CCACHE_NAME_OID},
        gss_krb5int_ccache_name
    },
    {
        {GSS_KRB5_FREE_LUCID_SEC_CONTEXT_OID_LENGTH, GSS_KRB5_FREE_LUCID_SEC_CONTEXT_OID},
        gss_krb5int_free_lucid_sec_context
    },
#ifndef _WIN32
    {
        {GSS_KRB5_USE_KDC_CONTEXT_OID_LENGTH, GSS_KRB5_USE_KDC_CONTEXT_OID},
        krb5int_gss_use_kdc_context
    },
#endif
};

static OM_uint32 KRB5_CALLCONV
krb5_gssspi_mech_invoke (OM_uint32 *minor_status,
                         const gss_OID desired_mech,
                         const gss_OID desired_object,
                         gss_buffer_t value)
{
    size_t i;

    if (minor_status == NULL)
        return GSS_S_CALL_INACCESSIBLE_WRITE;

    *minor_status = 0;

    if (desired_mech == GSS_C_NO_OID)
        return GSS_S_BAD_MECH;

    if (desired_object == GSS_C_NO_OID)
        return GSS_S_CALL_INACCESSIBLE_READ;

    for (i = 0; i < sizeof(krb5_gssspi_mech_invoke_ops)/
             sizeof(krb5_gssspi_mech_invoke_ops[0]); i++) {
        if (g_OID_prefix_equal(desired_object, &krb5_gssspi_mech_invoke_ops[i].oid)) {
            return (*krb5_gssspi_mech_invoke_ops[i].func)(minor_status,
                                                          desired_mech,
                                                          desired_object,
                                                          value);
        }
    }

    *minor_status = EINVAL;

    return GSS_S_UNAVAILABLE;
}

#define GS2_KRB5_SASL_NAME        "GS2-KRB5"
#define GS2_KRB5_SASL_NAME_LEN    (sizeof(GS2_KRB5_SASL_NAME) - 1)

#define GS2_IAKERB_SASL_NAME      "GS2-IAKERB"
#define GS2_IAKERB_SASL_NAME_LEN  (sizeof(GS2_IAKERB_SASL_NAME) - 1)

static OM_uint32 KRB5_CALLCONV
krb5_gss_inquire_mech_for_saslname(OM_uint32 *minor_status,
                                   const gss_buffer_t sasl_mech_name,
                                   gss_OID *mech_type)
{
    *minor_status = 0;

    if (sasl_mech_name->length == GS2_KRB5_SASL_NAME_LEN &&
        memcmp(sasl_mech_name->value,
               GS2_KRB5_SASL_NAME, GS2_KRB5_SASL_NAME_LEN) == 0) {
        if (mech_type != NULL)
            *mech_type = (gss_OID)gss_mech_krb5;
        return GSS_S_COMPLETE;
    } else if (sasl_mech_name->length == GS2_IAKERB_SASL_NAME_LEN &&
               memcmp(sasl_mech_name->value,
                      GS2_IAKERB_SASL_NAME, GS2_IAKERB_SASL_NAME_LEN) == 0) {
        if (mech_type != NULL)
            *mech_type = (gss_OID)gss_mech_iakerb;
        return GSS_S_COMPLETE;
    }

    return GSS_S_BAD_MECH;
}

static OM_uint32 KRB5_CALLCONV
krb5_gss_inquire_saslname_for_mech(OM_uint32 *minor_status,
                                   const gss_OID desired_mech,
                                   gss_buffer_t sasl_mech_name,
                                   gss_buffer_t mech_name,
                                   gss_buffer_t mech_description)
{
    if (g_OID_equal(desired_mech, gss_mech_iakerb)) {
        if (!g_make_string_buffer(GS2_IAKERB_SASL_NAME, sasl_mech_name) ||
            !g_make_string_buffer("iakerb", mech_name) ||
            !g_make_string_buffer("Initial and Pass Through Authentication "
                                  "Kerberos Mechanism (IAKERB)",
                                  mech_description))
            goto fail;
    } else {
        if (!g_make_string_buffer(GS2_KRB5_SASL_NAME, sasl_mech_name) ||
            !g_make_string_buffer("krb5", mech_name) ||
            !g_make_string_buffer("Kerberos 5 GSS-API Mechanism",
                                  mech_description))
            goto fail;
    }

    *minor_status = 0;
    return GSS_S_COMPLETE;

fail:
    *minor_status = ENOMEM;
    return GSS_S_FAILURE;
}

static OM_uint32 KRB5_CALLCONV
krb5_gss_inquire_attrs_for_mech(OM_uint32 *minor_status,
                                gss_const_OID mech,
                                gss_OID_set *mech_attrs,
                                gss_OID_set *known_mech_attrs)
{
    OM_uint32 major, tmpMinor;

    if (mech_attrs == NULL) {
        *minor_status = 0;
        return GSS_S_COMPLETE;
    }

    major = gss_create_empty_oid_set(minor_status, mech_attrs);
    if (GSS_ERROR(major))
        goto cleanup;

#define MA_SUPPORTED(ma)    do {                                        \
        major = gss_add_oid_set_member(minor_status, (gss_OID)ma,       \
                                       mech_attrs);                     \
        if (GSS_ERROR(major))                                           \
            goto cleanup;                                               \
    } while (0)

    MA_SUPPORTED(GSS_C_MA_MECH_CONCRETE);
    MA_SUPPORTED(GSS_C_MA_ITOK_FRAMED);
    MA_SUPPORTED(GSS_C_MA_AUTH_INIT);
    MA_SUPPORTED(GSS_C_MA_AUTH_TARG);
    MA_SUPPORTED(GSS_C_MA_DELEG_CRED);
    MA_SUPPORTED(GSS_C_MA_INTEG_PROT);
    MA_SUPPORTED(GSS_C_MA_CONF_PROT);
    MA_SUPPORTED(GSS_C_MA_MIC);
    MA_SUPPORTED(GSS_C_MA_WRAP);
    MA_SUPPORTED(GSS_C_MA_PROT_READY);
    MA_SUPPORTED(GSS_C_MA_REPLAY_DET);
    MA_SUPPORTED(GSS_C_MA_OOS_DET);
    MA_SUPPORTED(GSS_C_MA_CBINDINGS);
    MA_SUPPORTED(GSS_C_MA_CTX_TRANS);

    if (g_OID_equal(mech, gss_mech_iakerb)) {
        MA_SUPPORTED(GSS_C_MA_AUTH_INIT_INIT);
        MA_SUPPORTED(GSS_C_MA_NOT_DFLT_MECH);
    } else if (!g_OID_equal(mech, gss_mech_krb5)) {
        MA_SUPPORTED(GSS_C_MA_DEPRECATED);
    }

cleanup:
    if (GSS_ERROR(major))
        gss_release_oid_set(&tmpMinor, mech_attrs);

    return major;
}

static OM_uint32 KRB5_CALLCONV
krb5_gss_localname(OM_uint32 *minor,
                   const gss_name_t pname,
                   const gss_const_OID mech_type,
                   gss_buffer_t localname)
{
    krb5_context context;
    krb5_error_code code;
    krb5_gss_name_t kname;
    char lname[BUFSIZ];

    code = krb5_gss_init_context(&context);
    if (code != 0) {
        *minor = code;
        return GSS_S_FAILURE;
    }

    kname = (krb5_gss_name_t)pname;

    code = krb5_aname_to_localname(context, kname->princ,
                                   sizeof(lname), lname);
    if (code != 0) {
        *minor = KRB5_NO_LOCALNAME;
        krb5_free_context(context);
        return GSS_S_FAILURE;
    }


    krb5_free_context(context);
    localname->value = gssalloc_strdup(lname);
    localname->length = strlen(lname);

    return (code == 0) ? GSS_S_COMPLETE : GSS_S_FAILURE;
}


static OM_uint32 KRB5_CALLCONV
krb5_gss_authorize_localname(OM_uint32 *minor,
                             const gss_name_t pname,
                             gss_const_buffer_t local_user,
                             gss_const_OID name_type)
{
    krb5_context context;
    krb5_error_code code;
    krb5_gss_name_t kname;
    char *user;
    int user_ok;

    if (name_type != GSS_C_NO_OID &&
        !g_OID_equal(name_type, GSS_C_NT_USER_NAME)) {
        return GSS_S_BAD_NAMETYPE;
    }

    kname = (krb5_gss_name_t)pname;

    code = krb5_gss_init_context(&context);
    if (code != 0) {
        *minor = code;
        return GSS_S_FAILURE;
    }

    user = k5memdup0(local_user->value, local_user->length, &code);
    if (user == NULL) {
        *minor = code;
        krb5_free_context(context);
        return GSS_S_FAILURE;
    }

    user_ok = krb5_kuserok(context, kname->princ, user);

    free(user);
    krb5_free_context(context);

    *minor = 0;
    return user_ok ? GSS_S_COMPLETE : GSS_S_UNAUTHORIZED;
}

static struct gss_config krb5_mechanism = {
    { GSS_MECH_KRB5_OID_LENGTH, GSS_MECH_KRB5_OID },
    NULL,
    krb5_gss_acquire_cred,
    krb5_gss_release_cred,
    krb5_gss_init_sec_context,
#ifdef LEAN_CLIENT
    NULL,
#else
    krb5_gss_accept_sec_context,
#endif
    krb5_gss_process_context_token,
    krb5_gss_delete_sec_context,
    krb5_gss_context_time,
    krb5_gss_get_mic,
    krb5_gss_verify_mic,
#if defined(IOV_SHIM_EXERCISE_WRAP) || defined(IOV_SHIM_EXERCISE)
    NULL,
#else
    krb5_gss_wrap,
#endif
#if defined(IOV_SHIM_EXERCISE_UNWRAP) || defined(IOV_SHIM_EXERCISE)
    NULL,
#else
    krb5_gss_unwrap,
#endif
    krb5_gss_display_status,
    krb5_gss_indicate_mechs,
    krb5_gss_compare_name,
    krb5_gss_display_name,
    krb5_gss_import_name,
    krb5_gss_release_name,
    krb5_gss_inquire_cred,
    NULL,                /* add_cred */
#ifdef LEAN_CLIENT
    NULL,
    NULL,
#else
    krb5_gss_export_sec_context,
    krb5_gss_import_sec_context,
#endif
    krb5_gss_inquire_cred_by_mech,
    krb5_gss_inquire_names_for_mech,
    krb5_gss_inquire_context,
    krb5_gss_internal_release_oid,
    krb5_gss_wrap_size_limit,
    krb5_gss_localname,

    krb5_gss_authorize_localname,
    krb5_gss_export_name,
    krb5_gss_duplicate_name,
    krb5_gss_store_cred,
    krb5_gss_inquire_sec_context_by_oid,
    krb5_gss_inquire_cred_by_oid,
    krb5_gss_set_sec_context_option,
    krb5_gssspi_set_cred_option,
    krb5_gssspi_mech_invoke,
    NULL,                /* wrap_aead */
    NULL,                /* unwrap_aead */
    krb5_gss_wrap_iov,
    krb5_gss_unwrap_iov,
    krb5_gss_wrap_iov_length,
    NULL,               /* complete_auth_token */
    krb5_gss_acquire_cred_impersonate_name,
    NULL,               /* krb5_gss_add_cred_impersonate_name */
    NULL,               /* display_name_ext */
    krb5_gss_inquire_name,
    krb5_gss_get_name_attribute,
    krb5_gss_set_name_attribute,
    krb5_gss_delete_name_attribute,
    krb5_gss_export_name_composite,
    krb5_gss_map_name_to_any,
    krb5_gss_release_any_name_mapping,
    krb5_gss_pseudo_random,
    NULL,               /* set_neg_mechs */
    krb5_gss_inquire_saslname_for_mech,
    krb5_gss_inquire_mech_for_saslname,
    krb5_gss_inquire_attrs_for_mech,
    krb5_gss_acquire_cred_from,
    krb5_gss_store_cred_into,
    krb5_gss_acquire_cred_with_password,
    krb5_gss_export_cred,
    krb5_gss_import_cred,
    NULL,               /* import_sec_context_by_mech */
    NULL,               /* import_name_by_mech */
    NULL,               /* import_cred_by_mech */
    krb5_gss_get_mic_iov,
    krb5_gss_verify_mic_iov,
    krb5_gss_get_mic_iov_length,
};

/* Functions which use security contexts or acquire creds are IAKERB-specific;
 * other functions can borrow from the krb5 mech. */
static struct gss_config iakerb_mechanism = {
    { GSS_MECH_KRB5_OID_LENGTH, GSS_MECH_KRB5_OID },
    NULL,
    iakerb_gss_acquire_cred,
    krb5_gss_release_cred,
    iakerb_gss_init_sec_context,
#ifdef LEAN_CLIENT
    NULL,
#else
    iakerb_gss_accept_sec_context,
#endif
    iakerb_gss_process_context_token,
    iakerb_gss_delete_sec_context,
    iakerb_gss_context_time,
    iakerb_gss_get_mic,
    iakerb_gss_verify_mic,
#if defined(IOV_SHIM_EXERCISE_WRAP) || defined(IOV_SHIM_EXERCISE)
    NULL,
#else
    iakerb_gss_wrap,
#endif
#if defined(IOV_SHIM_EXERCISE_UNWRAP) || defined(IOV_SHIM_EXERCISE)
    NULL,
#else
    iakerb_gss_unwrap,
#endif
    krb5_gss_display_status,
    krb5_gss_indicate_mechs,
    krb5_gss_compare_name,
    krb5_gss_display_name,
    krb5_gss_import_name,
    krb5_gss_release_name,
    krb5_gss_inquire_cred,
    NULL,                /* add_cred */
#ifdef LEAN_CLIENT
    NULL,
    NULL,
#else
    iakerb_gss_export_sec_context,
    iakerb_gss_import_sec_context,
#endif
    krb5_gss_inquire_cred_by_mech,
    krb5_gss_inquire_names_for_mech,
    iakerb_gss_inquire_context,
    krb5_gss_internal_release_oid,
    iakerb_gss_wrap_size_limit,
    krb5_gss_localname,
    krb5_gss_authorize_localname,
    krb5_gss_export_name,
    krb5_gss_duplicate_name,
    krb5_gss_store_cred,
    iakerb_gss_inquire_sec_context_by_oid,
    krb5_gss_inquire_cred_by_oid,
    iakerb_gss_set_sec_context_option,
    krb5_gssspi_set_cred_option,
    krb5_gssspi_mech_invoke,
    NULL,                /* wrap_aead */
    NULL,                /* unwrap_aead */
    iakerb_gss_wrap_iov,
    iakerb_gss_unwrap_iov,
    iakerb_gss_wrap_iov_length,
    NULL,               /* complete_auth_token */
    NULL,               /* acquire_cred_impersonate_name */
    NULL,               /* add_cred_impersonate_name */
    NULL,               /* display_name_ext */
    krb5_gss_inquire_name,
    krb5_gss_get_name_attribute,
    krb5_gss_set_name_attribute,
    krb5_gss_delete_name_attribute,
    krb5_gss_export_name_composite,
    krb5_gss_map_name_to_any,
    krb5_gss_release_any_name_mapping,
    iakerb_gss_pseudo_random,
    NULL,               /* set_neg_mechs */
    krb5_gss_inquire_saslname_for_mech,
    krb5_gss_inquire_mech_for_saslname,
    krb5_gss_inquire_attrs_for_mech,
    krb5_gss_acquire_cred_from,
    krb5_gss_store_cred_into,
    iakerb_gss_acquire_cred_with_password,
    krb5_gss_export_cred,
    krb5_gss_import_cred,
    NULL,               /* import_sec_context_by_mech */
    NULL,               /* import_name_by_mech */
    NULL,               /* import_cred_by_mech */
    iakerb_gss_get_mic_iov,
    iakerb_gss_verify_mic_iov,
    iakerb_gss_get_mic_iov_length,
};

#ifdef _GSS_STATIC_LINK
#include "mglueP.h"
static int gss_iakerbmechglue_init(void)
{
    struct gss_mech_config mech_iakerb;

    memset(&mech_iakerb, 0, sizeof(mech_iakerb));
    mech_iakerb.mech = &iakerb_mechanism;

    mech_iakerb.mechNameStr = "iakerb";
    mech_iakerb.mech_type = (gss_OID)gss_mech_iakerb;
    gssint_register_mechinfo(&mech_iakerb);

    return 0;
}

static int gss_krb5mechglue_init(void)
{
    struct gss_mech_config mech_krb5;

    memset(&mech_krb5, 0, sizeof(mech_krb5));
    mech_krb5.mech = &krb5_mechanism;

    mech_krb5.mechNameStr = "kerberos_v5";
    mech_krb5.mech_type = (gss_OID)gss_mech_krb5;
    gssint_register_mechinfo(&mech_krb5);

    mech_krb5.mechNameStr = "kerberos_v5_old";
    mech_krb5.mech_type = (gss_OID)gss_mech_krb5_old;
    gssint_register_mechinfo(&mech_krb5);

    mech_krb5.mechNameStr = "mskrb";
    mech_krb5.mech_type = (gss_OID)gss_mech_krb5_wrong;
    gssint_register_mechinfo(&mech_krb5);

    return 0;
}
#else
MAKE_INIT_FUNCTION(gss_krb5int_lib_init);
MAKE_FINI_FUNCTION(gss_krb5int_lib_fini);

gss_mechanism KRB5_CALLCONV
gss_mech_initialize(void)
{
    return &krb5_mechanism;
}
#endif /* _GSS_STATIC_LINK */

int gss_krb5int_lib_init(void)
{
    int err;

#ifdef SHOW_INITFINI_FUNCS
    printf("gss_krb5int_lib_init\n");
#endif

    add_error_table(&et_k5g_error_table);

#ifndef LEAN_CLIENT
    err = k5_mutex_finish_init(&gssint_krb5_keytab_lock);
    if (err)
        return err;
#endif /* LEAN_CLIENT */
    err = k5_key_register(K5_KEY_GSS_KRB5_SET_CCACHE_OLD_NAME, free);
    if (err)
        return err;
    err = k5_key_register(K5_KEY_GSS_KRB5_CCACHE_NAME, free);
    if (err)
        return err;
    err = k5_key_register(K5_KEY_GSS_KRB5_ERROR_MESSAGE,
                          krb5_gss_delete_error_info);
    if (err)
        return err;
#ifndef _WIN32
    err = k5_mutex_finish_init(&kg_kdc_flag_mutex);
    if (err)
        return err;
    err = k5_mutex_finish_init(&kg_vdb.mutex);
    if (err)
        return err;
#endif
#ifdef _GSS_STATIC_LINK
    err = gss_krb5mechglue_init();
    if (err)
        return err;
    err = gss_iakerbmechglue_init();
    if (err)
        return err;
#endif

    return 0;
}

void gss_krb5int_lib_fini(void)
{
#ifndef _GSS_STATIC_LINK
    if (!INITIALIZER_RAN(gss_krb5int_lib_init) || PROGRAM_EXITING()) {
# ifdef SHOW_INITFINI_FUNCS
        printf("gss_krb5int_lib_fini: skipping\n");
# endif
        return;
    }
#endif
#ifdef SHOW_INITFINI_FUNCS
    printf("gss_krb5int_lib_fini\n");
#endif
    remove_error_table(&et_k5g_error_table);

    k5_key_delete(K5_KEY_GSS_KRB5_SET_CCACHE_OLD_NAME);
    k5_key_delete(K5_KEY_GSS_KRB5_CCACHE_NAME);
    k5_key_delete(K5_KEY_GSS_KRB5_ERROR_MESSAGE);
    k5_mutex_destroy(&kg_vdb.mutex);
#ifndef _WIN32
    k5_mutex_destroy(&kg_kdc_flag_mutex);
#endif
#ifndef LEAN_CLIENT
    k5_mutex_destroy(&gssint_krb5_keytab_lock);
#endif /* LEAN_CLIENT */
}

#ifdef _GSS_STATIC_LINK
extern OM_uint32 gssint_lib_init(void);
#endif

OM_uint32 gss_krb5int_initialize_library (void)
{
#ifdef _GSS_STATIC_LINK
    return gssint_mechglue_initialize_library();
#else
    return CALL_INIT_FUNCTION(gss_krb5int_lib_init);
#endif
}
