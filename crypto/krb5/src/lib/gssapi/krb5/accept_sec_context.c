/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright 2000, 2004, 2007, 2008  by the Massachusetts Institute of Technology.
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

#include "k5-int.h"
#include "k5-input.h"
#include "gssapiP_krb5.h"
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <assert.h>

#ifdef CFX_EXERCISE
#define CFX_ACCEPTOR_SUBKEY (time(0) & 1)
#else
#define CFX_ACCEPTOR_SUBKEY 1
#endif

#ifndef LEAN_CLIENT

static OM_uint32
create_constrained_deleg_creds(OM_uint32 *minor_status,
                               krb5_gss_cred_id_t verifier_cred_handle,
                               krb5_ticket *ticket,
                               krb5_gss_cred_id_t *out_cred,
                               krb5_context context)
{
    OM_uint32 major_status;
    krb5_creds krb_creds;
    krb5_data *data;
    krb5_error_code code;

    assert(out_cred != NULL);
    assert(verifier_cred_handle->usage == GSS_C_BOTH);

    memset(&krb_creds, 0, sizeof(krb_creds));
    krb_creds.client = ticket->enc_part2->client;
    krb_creds.server = ticket->server;
    krb_creds.keyblock = *(ticket->enc_part2->session);
    krb_creds.ticket_flags = ticket->enc_part2->flags;
    krb_creds.times = ticket->enc_part2->times;
    krb_creds.magic = KV5M_CREDS;
    krb_creds.authdata = NULL;

    code = encode_krb5_ticket(ticket, &data);
    if (code) {
        *minor_status = code;
        return GSS_S_FAILURE;
    }

    krb_creds.ticket = *data;

    major_status = kg_compose_deleg_cred(minor_status,
                                         verifier_cred_handle,
                                         &krb_creds,
                                         GSS_C_INDEFINITE,
                                         out_cred,
                                         NULL,
                                         context);

    krb5_free_data(context, data);

    return major_status;
}

/* Decode, decrypt and store the forwarded creds in the local ccache. */
static krb5_error_code
rd_and_store_for_creds(context, auth_context, inbuf, out_cred)
    krb5_context context;
    krb5_auth_context auth_context;
    krb5_data *inbuf;
    krb5_gss_cred_id_t *out_cred;
{
    krb5_creds ** creds = NULL;
    krb5_error_code retval;
    krb5_ccache ccache = NULL;
    krb5_gss_cred_id_t cred = NULL;
    krb5_auth_context new_auth_ctx = NULL;
    krb5_int32 flags_org;

    if ((retval = krb5_auth_con_getflags(context, auth_context, &flags_org)))
        return retval;
    krb5_auth_con_setflags(context, auth_context,
                           0);

    /*
     * By the time krb5_rd_cred is called here (after krb5_rd_req has been
     * called in krb5_gss_accept_sec_context), the "keyblock" field of
     * auth_context contains a pointer to the session key, and the
     * "recv_subkey" field might contain a session subkey.  Either of
     * these (the "recv_subkey" if it isn't NULL, otherwise the
     * "keyblock") might have been used to encrypt the encrypted part of
     * the KRB_CRED message that contains the forwarded credentials.  (The
     * Java Crypto and Security Implementation from the DSTC in Australia
     * always uses the session key.  But apparently it never negotiates a
     * subkey, so this code works fine against a JCSI client.)  Up to the
     * present, though, GSSAPI clients linked against the MIT code (which
     * is almost all GSSAPI clients) don't encrypt the KRB_CRED message at
     * all -- at this level.  So if the first call to krb5_rd_cred fails,
     * we should call it a second time with another auth context freshly
     * created by krb5_auth_con_init.  All of its keyblock fields will be
     * NULL, so krb5_rd_cred will assume that the KRB_CRED message is
     * unencrypted.  (The MIT code doesn't actually send the KRB_CRED
     * message in the clear -- the "authenticator" whose "checksum" ends up
     * containing the KRB_CRED message does get encrypted.)
     */
    if (krb5_rd_cred(context, auth_context, inbuf, &creds, NULL)) {
        if ((retval = krb5_auth_con_init(context, &new_auth_ctx)))
            goto cleanup;
        krb5_auth_con_setflags(context, new_auth_ctx, 0);
        if ((retval = krb5_rd_cred(context, new_auth_ctx, inbuf,
                                   &creds, NULL)))
            goto cleanup;
    }

    if ((retval = krb5_cc_new_unique(context, "MEMORY", NULL, &ccache))) {
        ccache = NULL;
        goto cleanup;
    }

    if ((retval = krb5_cc_initialize(context, ccache, creds[0]->client)))
        goto cleanup;

    if ((retval = k5_cc_store_primary_cred(context, ccache, creds[0])))
        goto cleanup;

    /* generate a delegated credential handle */
    if (out_cred) {
        /* allocate memory for a cred_t... */
        if (!(cred =
              (krb5_gss_cred_id_t) xmalloc(sizeof(krb5_gss_cred_id_rec)))) {
            retval = ENOMEM; /* out of memory? */
            goto cleanup;
        }

        /* zero it out... */
        memset(cred, 0, sizeof(krb5_gss_cred_id_rec));

        retval = k5_mutex_init(&cred->lock);
        if (retval) {
            xfree(cred);
            cred = NULL;
            goto cleanup;
        }

        /* copy the client principle into it... */
        if ((retval =
             kg_init_name(context, creds[0]->client, NULL, NULL, NULL, 0,
                          &cred->name))) {
            k5_mutex_destroy(&cred->lock);
            retval = ENOMEM; /* out of memory? */
            xfree(cred); /* clean up memory on failure */
            cred = NULL;
            goto cleanup;
        }

        cred->usage = GSS_C_INITIATE; /* we can't accept with this */
        /* cred->name already set */
        cred->keytab = NULL; /* no keytab associated with this... */
        cred->expire = creds[0]->times.endtime; /* store the end time */
        cred->ccache = ccache; /* the ccache containing the credential */
        cred->destroy_ccache = 1;
        ccache = NULL; /* cred takes ownership so don't destroy */
    }

    /* If there were errors, there might have been a memory leak
       if (!cred)
       if ((retval = krb5_cc_close(context, ccache)))
       goto cleanup;
    */
cleanup:
    if (creds)
        krb5_free_tgt_creds(context, creds);

    if (ccache)
        (void)krb5_cc_destroy(context, ccache);

    if (out_cred)
        *out_cred = cred; /* return credential */

    if (new_auth_ctx)
        krb5_auth_con_free(context, new_auth_ctx);

    krb5_auth_con_setflags(context, auth_context, flags_org);

    return retval;
}


/*
 * Performs third leg of DCE authentication
 */
static OM_uint32
kg_accept_dce(minor_status, context_handle, verifier_cred_handle,
              input_token, input_chan_bindings, src_name, mech_type,
              output_token, ret_flags, time_rec, delegated_cred_handle)
    OM_uint32 *minor_status;
    gss_ctx_id_t *context_handle;
    gss_cred_id_t verifier_cred_handle;
    gss_buffer_t input_token;
    gss_channel_bindings_t input_chan_bindings;
    gss_name_t *src_name;
    gss_OID *mech_type;
    gss_buffer_t output_token;
    OM_uint32 *ret_flags;
    OM_uint32 *time_rec;
    gss_cred_id_t *delegated_cred_handle;
{
    krb5_error_code code;
    krb5_gss_ctx_id_rec *ctx = 0;
    krb5_timestamp now;
    krb5_gss_name_t name = NULL;
    krb5_ui_4 nonce = 0;
    krb5_data ap_rep;
    OM_uint32 major_status = GSS_S_FAILURE;

    output_token->length = 0;
    output_token->value = NULL;

    if (mech_type)
        *mech_type = GSS_C_NULL_OID;
    /* return a bogus cred handle */
    if (delegated_cred_handle)
        *delegated_cred_handle = GSS_C_NO_CREDENTIAL;

    ctx = (krb5_gss_ctx_id_rec *)*context_handle;

    code = krb5_timeofday(ctx->k5_context, &now);
    if (code != 0) {
        major_status = GSS_S_FAILURE;
        goto fail;
    }

    ap_rep.data = input_token->value;
    ap_rep.length = input_token->length;

    code = krb5_rd_rep_dce(ctx->k5_context,
                           ctx->auth_context,
                           &ap_rep,
                           &nonce);
    if (code != 0) {
        major_status = GSS_S_FAILURE;
        goto fail;
    }

    ctx->established = 1;

    if (src_name) {
        code = kg_duplicate_name(ctx->k5_context, ctx->there, &name);
        if (code) {
            major_status = GSS_S_FAILURE;
            goto fail;
        }
        *src_name = (gss_name_t) name;
    }

    if (mech_type)
        *mech_type = ctx->mech_used;

    if (time_rec) {
        *time_rec = ts_interval(ts_incr(now, -ctx->k5_context->clockskew),
                                ctx->krb_times.endtime);
    }

    /* Never return GSS_C_DELEG_FLAG since we don't support DCE credential
     * delegation yet. */
    if (ret_flags)
        *ret_flags = (ctx->gss_flags & ~GSS_C_DELEG_FLAG);

    *minor_status = 0;

    return GSS_S_COMPLETE;

fail:
    /* real failure code follows */

    (void) krb5_gss_delete_sec_context(minor_status, (gss_ctx_id_t *) &ctx,
                                       NULL);
    *context_handle = GSS_C_NO_CONTEXT;
    *minor_status = code;

    return major_status;
}

static krb5_error_code
kg_process_extension(krb5_context context,
                     krb5_auth_context auth_context,
                     int ext_type,
                     krb5_data *ext_data,
                     krb5_gss_ctx_ext_t exts)
{
    krb5_error_code code = 0;

    assert(exts != NULL);

    switch (ext_type) {
    case KRB5_GSS_EXTS_IAKERB_FINISHED:
        if (exts->iakerb.conv == NULL) {
            code = KRB5KRB_AP_ERR_MSG_TYPE; /* XXX */
        } else {
            krb5_key key;

            code = krb5_auth_con_getrecvsubkey_k(context, auth_context, &key);
            if (code != 0)
                break;

            code = iakerb_verify_finished(context, key, exts->iakerb.conv,
                                          ext_data);
            if (code == 0)
                exts->iakerb.verified = 1;

            krb5_k_free_key(context, key);
        }
        break;
    default:
        break;
    }

    return code;
}

/* The length of the MD5 channel bindings in an 0x8003 checksum */
#define CB_MD5_LEN 16

/* The minimum length of an 0x8003 checksum value (4-byte channel bindings
 * length, 16-byte channel bindings, 4-byte flags) */
#define MIN_8003_LEN (4 + CB_MD5_LEN + 4)

/* The flags we accept from the initiator's authenticator checksum. */
#define INITIATOR_FLAGS (GSS_C_INTEG_FLAG | GSS_C_CONF_FLAG |           \
                         GSS_C_MUTUAL_FLAG | GSS_C_REPLAY_FLAG |        \
                         GSS_C_SEQUENCE_FLAG | GSS_C_DCE_STYLE |        \
                         GSS_C_IDENTIFY_FLAG | GSS_C_EXTENDED_ERROR_FLAG)

/* A zero-value channel binding, for comparison */
static const uint8_t null_cb[CB_MD5_LEN];

/* Look for AP_OPTIONS in authdata.  If present and the options include
 * KERB_AP_OPTIONS_CBT, set *cbt_out to true. */
static krb5_error_code
check_cbt(krb5_context context, krb5_authdata *const *authdata,
          krb5_boolean *cbt_out)
{
    krb5_error_code code;
    krb5_authdata **ad;
    uint32_t ad_ap_options;
    const uint32_t KERB_AP_OPTIONS_CBT = 0x4000;

    *cbt_out = FALSE;

    code = krb5_find_authdata(context, NULL, authdata,
                              KRB5_AUTHDATA_AP_OPTIONS, &ad);
    if (code || ad == NULL)
        return code;
    if (ad[1] != NULL || ad[0]->length != 4) {
        code = KRB5KRB_AP_ERR_MSG_TYPE;
    } else {
        ad_ap_options = load_32_le(ad[0]->contents);
        if (ad_ap_options & KERB_AP_OPTIONS_CBT)
            *cbt_out = TRUE;
    }

    krb5_free_authdata(context, ad);
    return code;
}

/*
 * The krb5 GSS mech appropriates the authenticator checksum field from RFC
 * 4120 to store structured data instead of a checksum, indicated with checksum
 * type 0x8003 (see RFC 4121 section 4.1.1).  Some implementations instead send
 * no checksum, or a regular checksum over empty data.
 *
 * Interpret the checksum.  Read delegated creds into *deleg_out if it is not
 * NULL.  Set *flags_out to the allowed subset of token flags, plus
 * GSS_C_DELEG_FLAG if a delegated credential was present and
 * GSS_C_CHANNEL_BOUND_FLAG if matching channel bindings are present.  Process
 * any extensions found using exts.  On error, set *code_out to a krb5_error
 * code for use as a minor status value.
 */
static OM_uint32
process_checksum(OM_uint32 *minor_status, krb5_context context,
                 gss_channel_bindings_t acceptor_cb,
                 krb5_auth_context auth_context, krb5_flags ap_req_options,
                 krb5_authenticator *authenticator, krb5_gss_ctx_ext_t exts,
                 krb5_gss_cred_id_t *deleg_out, krb5_ui_4 *flags_out,
                 krb5_error_code *code_out)
{
    krb5_error_code code = 0;
    OM_uint32 status, option_id, token_flags;
    size_t cb_len, option_len;
    krb5_boolean valid, client_cbt, token_cb_present = FALSE, cb_match = FALSE;
    krb5_key subkey;
    krb5_data option, empty = empty_data();
    krb5_checksum cb_cksum;
    const uint8_t *token_cb, *option_bytes;
    struct k5input in;
    const krb5_checksum *cksum = authenticator->checksum;

    cb_cksum.contents = NULL;

    if (cksum == NULL) {
        /*
         * Some SMB client implementations use handcrafted GSSAPI code that
         * does not provide a checksum.  MS-KILE documents that the Microsoft
         * implementation considers a missing checksum acceptable; the server
         * assumes all flags are unset in this case, and does not check channel
         * bindings.
         */
        *flags_out = 0;
    } else if (cksum->checksum_type != CKSUMTYPE_KG_CB) {
        /* Samba sends a regular checksum. */
        code = krb5_auth_con_getkey_k(context, auth_context, &subkey);
        if (code) {
            status = GSS_S_FAILURE;
            goto fail;
        }

        /* Verifying the checksum ensures that this authenticator wasn't
         * replayed from one with a checksum over actual data. */
        code = krb5_k_verify_checksum(context, subkey,
                                      KRB5_KEYUSAGE_AP_REQ_AUTH_CKSUM, &empty,
                                      cksum, &valid);
        krb5_k_free_key(context, subkey);
        if (code || !valid) {
            status = GSS_S_BAD_SIG;
            goto fail;
        }

        /* Use ap_options from the request to guess the mutual flag. */
        *flags_out = GSS_C_REPLAY_FLAG | GSS_C_SEQUENCE_FLAG;
        if (ap_req_options & AP_OPTS_MUTUAL_REQUIRED)
            *flags_out |= GSS_C_MUTUAL_FLAG;
    } else {
        /* The checksum must contain at least a fixed 24-byte part. */
        if (cksum->length < MIN_8003_LEN) {
            status = GSS_S_BAD_BINDINGS;
            goto fail;
        }

        k5_input_init(&in, cksum->contents, cksum->length);
        cb_len = k5_input_get_uint32_le(&in);
        if (cb_len != CB_MD5_LEN) {
            code = KG_BAD_LENGTH;
            status = GSS_S_FAILURE;
            goto fail;
        }

        token_cb = k5_input_get_bytes(&in, cb_len);
        if (acceptor_cb != GSS_C_NO_CHANNEL_BINDINGS) {
            code = kg_checksum_channel_bindings(context, acceptor_cb,
                                                &cb_cksum);
            if (code) {
                status = GSS_S_BAD_BINDINGS;
                goto fail;
            }
            assert(cb_cksum.length == cb_len);
            token_cb_present = (k5_bcmp(token_cb, null_cb, cb_len) != 0);
            cb_match = (k5_bcmp(token_cb, cb_cksum.contents, cb_len) == 0);
            if (token_cb_present && !cb_match) {
                status = GSS_S_BAD_BINDINGS;
                goto fail;
            }
        }

        /* Read the token flags and accept some of them as context flags. */
        token_flags = k5_input_get_uint32_le(&in);
        *flags_out = token_flags & INITIATOR_FLAGS;
        if (cb_match)
            *flags_out |= GSS_C_CHANNEL_BOUND_FLAG;

        /* Read the delegated credential if present. */
        if (in.len >= 4 && (token_flags & GSS_C_DELEG_FLAG)) {
            option_id = k5_input_get_uint16_le(&in);
            option_len = k5_input_get_uint16_le(&in);
            option_bytes = k5_input_get_bytes(&in, option_len);
            option = make_data((uint8_t *)option_bytes, option_len);
            if (in.status) {
                code = KG_BAD_LENGTH;
                status = GSS_S_FAILURE;
                goto fail;
            }
            if (option_id != KRB5_GSS_FOR_CREDS_OPTION) {
                status = GSS_S_FAILURE;
                goto fail;
            }

            /* Store the delegated credential. */
            code = rd_and_store_for_creds(context, auth_context, &option,
                                          deleg_out);
            if (code) {
                status = GSS_S_FAILURE;
                goto fail;
            }
            *flags_out |= GSS_C_DELEG_FLAG;
        }

        /* Process any extensions at the end of the checksum.  Extensions use
         * 4-byte big-endian tag and length instead of 2-byte little-endian. */
        while (in.len > 0) {
            option_id = k5_input_get_uint32_be(&in);
            option_len = k5_input_get_uint32_be(&in);
            option_bytes = k5_input_get_bytes(&in, option_len);
            option = make_data((uint8_t *)option_bytes, option_len);
            if (in.status) {
                code = KG_BAD_LENGTH;
                status = GSS_S_FAILURE;
                goto fail;
            }

            code = kg_process_extension(context, auth_context, option_id,
                                        &option, exts);
            if (code) {
                status = GSS_S_FAILURE;
                goto fail;
            }
        }
    }

    /*
     * If the client asserts the KERB_AP_OPTIONS_CBT flag (from MS-KILE) in the
     * authenticator authdata, and the acceptor passed channel bindings,
     * require matching channel bindings from the client.  The intent is to
     * prevent an authenticator generated for use outside of a TLS channel from
     * being used inside of one.
     */
    code = check_cbt(context, authenticator->authorization_data, &client_cbt);
    if (code) {
        status = GSS_S_FAILURE;
        goto fail;
    }
    if (client_cbt && acceptor_cb != GSS_C_NO_CHANNEL_BINDINGS && !cb_match) {
        status = GSS_S_BAD_BINDINGS;
        goto fail;
    }

    status = GSS_S_COMPLETE;

fail:
    free(cb_cksum.contents);
    *code_out = code;
    return status;
}

static OM_uint32
kg_accept_krb5(minor_status, context_handle,
               verifier_cred_handle, input_token,
               input_chan_bindings, src_name, mech_type,
               output_token, ret_flags, time_rec,
               delegated_cred_handle, exts)
    OM_uint32 *minor_status;
    gss_ctx_id_t *context_handle;
    gss_cred_id_t verifier_cred_handle;
    gss_buffer_t input_token;
    gss_channel_bindings_t input_chan_bindings;
    gss_name_t *src_name;
    gss_OID *mech_type;
    gss_buffer_t output_token;
    OM_uint32 *ret_flags;
    OM_uint32 *time_rec;
    gss_cred_id_t *delegated_cred_handle;
    krb5_gss_ctx_ext_t exts;
{
    krb5_context context;
    unsigned char *ptr;
    krb5_gss_cred_id_t cred = 0;
    krb5_data ap_rep, ap_req;
    krb5_error_code code;
    krb5_address addr, *paddr;
    krb5_authenticator *authdat = 0;
    krb5_gss_name_t name = NULL;
    krb5_ui_4 gss_flags = 0;
    krb5_gss_ctx_id_rec *ctx = NULL;
    krb5_timestamp now;
    gss_buffer_desc token;
    krb5_auth_context auth_context = NULL;
    krb5_ticket * ticket = NULL;
    const gss_OID_desc *mech_used = NULL;
    OM_uint32 major_status;
    OM_uint32 tmp_minor_status;
    krb5_error krb_error_data;
    krb5_data scratch;
    gss_cred_id_t defcred = GSS_C_NO_CREDENTIAL;
    krb5_gss_cred_id_t deleg_cred = NULL;
    krb5int_access kaccess;
    int cred_rcache = 0;
    int no_encap = 0;
    krb5_flags ap_req_options = 0;
    krb5_enctype negotiated_etype;
    krb5_authdata_context ad_context = NULL;
    krb5_ap_req *request = NULL;
    struct k5buf buf;

    code = krb5int_accessor (&kaccess, KRB5INT_ACCESS_VERSION);
    if (code) {
        *minor_status = code;
        return(GSS_S_FAILURE);
    }

    code = krb5_gss_init_context(&context);
    if (code) {
        *minor_status = code;
        return GSS_S_FAILURE;
    }

    /* set up returns to be freeable */

    if (src_name)
        *src_name = (gss_name_t) NULL;
    output_token->length = 0;
    output_token->value = NULL;
    token.value = 0;
    ap_req.data = 0;
    ap_rep.data = 0;

    if (mech_type)
        *mech_type = GSS_C_NULL_OID;
    /* return a bogus cred handle */
    if (delegated_cred_handle)
        *delegated_cred_handle = GSS_C_NO_CREDENTIAL;

    /* handle default cred handle */
    if (verifier_cred_handle == GSS_C_NO_CREDENTIAL) {
        major_status = krb5_gss_acquire_cred(minor_status, GSS_C_NO_NAME,
                                             GSS_C_INDEFINITE, GSS_C_NO_OID_SET,
                                             GSS_C_ACCEPT, &defcred,
                                             NULL, NULL);
        if (major_status != GSS_S_COMPLETE) {
            code = *minor_status;
            goto fail;
        }
        verifier_cred_handle = defcred;
    }

    /* Resolve any initiator state in the verifier cred and lock it. */
    major_status = kg_cred_resolve(minor_status, context, verifier_cred_handle,
                                   GSS_C_NO_NAME);
    if (GSS_ERROR(major_status)) {
        code = *minor_status;
        goto fail;
    }
    cred = (krb5_gss_cred_id_t)verifier_cred_handle;

    /* make sure the supplied credentials are valid for accept */

    if ((cred->usage != GSS_C_ACCEPT) &&
        (cred->usage != GSS_C_BOTH)) {
        code = 0;
        major_status = GSS_S_NO_CRED;
        goto fail;
    }

    /* verify the token's integrity, and leave the token in ap_req.
       figure out which mech oid was used, and save it */

    ptr = (unsigned char *) input_token->value;

    if (!(code = g_verify_token_header(gss_mech_krb5,
                                       &(ap_req.length),
                                       &ptr, KG_TOK_CTX_AP_REQ,
                                       input_token->length, 1))) {
        mech_used = gss_mech_krb5;
    } else if ((code == G_WRONG_MECH)
               &&!(code = g_verify_token_header((gss_OID) gss_mech_iakerb,
                                                &(ap_req.length),
                                                &ptr, KG_TOK_CTX_AP_REQ,
                                                input_token->length, 1))) {
        mech_used = gss_mech_iakerb;
    } else if ((code == G_WRONG_MECH)
               &&!(code = g_verify_token_header((gss_OID) gss_mech_krb5_wrong,
                                                &(ap_req.length),
                                                &ptr, KG_TOK_CTX_AP_REQ,
                                                input_token->length, 1))) {
        mech_used = gss_mech_krb5_wrong;
    } else if ((code == G_WRONG_MECH) &&
               !(code = g_verify_token_header(gss_mech_krb5_old,
                                              &(ap_req.length),
                                              &ptr, KG_TOK_CTX_AP_REQ,
                                              input_token->length, 1))) {
        /*
         * Previous versions of this library used the old mech_id
         * and some broken behavior (wrong IV on checksum
         * encryption).  We support the old mech_id for
         * compatibility, and use it to decide when to use the
         * old behavior.
         */
        mech_used = gss_mech_krb5_old;
    } else if (code == G_WRONG_TOKID) {
        major_status = GSS_S_CONTINUE_NEEDED;
        code = KRB5KRB_AP_ERR_MSG_TYPE;
        mech_used = gss_mech_krb5;
        goto fail;
    } else if (code == G_BAD_TOK_HEADER) {
        /* DCE style not encapsulated */
        ap_req.length = input_token->length;
        mech_used = gss_mech_krb5;
        no_encap = 1;
    } else {
        major_status = GSS_S_DEFECTIVE_TOKEN;
        goto fail;
    }
    ap_req.data = (char *)ptr;

    /* construct the sender_addr */

    if ((input_chan_bindings != GSS_C_NO_CHANNEL_BINDINGS) &&
        (input_chan_bindings->initiator_addrtype == GSS_C_AF_INET)) {
        /* XXX is this right? */
        addr.addrtype = ADDRTYPE_INET;
        addr.length = input_chan_bindings->initiator_address.length;
        addr.contents = input_chan_bindings->initiator_address.value;

        paddr = &addr;
    } else {
        paddr = NULL;
    }

    /* decode the AP_REQ message */
    code = decode_krb5_ap_req(&ap_req, &request);
    if (code) {
        major_status = GSS_S_FAILURE;
        goto done;
    }
    ticket = request->ticket;

    /* decode the message */

    if ((code = krb5_auth_con_init(context, &auth_context))) {
        major_status = GSS_S_FAILURE;
        save_error_info((OM_uint32)code, context);
        goto fail;
    }
    if (cred->rcache) {
        cred_rcache = 1;
        if ((code = krb5_auth_con_setrcache(context, auth_context, cred->rcache))) {
            major_status = GSS_S_FAILURE;
            goto fail;
        }
    }
    if ((code = krb5_auth_con_setaddrs(context, auth_context, NULL, paddr))) {
        major_status = GSS_S_FAILURE;
        goto fail;
    }

    /* Limit the encryption types negotiated (if requested). */
    if (cred->req_enctypes) {
        if ((code = krb5_auth_con_setpermetypes(context, auth_context,
                                                cred->req_enctypes))) {
            major_status = GSS_S_FAILURE;
            goto fail;
        }
    }

    code = krb5_rd_req_decoded(context, &auth_context, request,
                               cred->acceptor_mprinc, cred->keytab,
                               &ap_req_options, NULL);
    if (code) {
        major_status = GSS_S_FAILURE;
        goto fail;
    }
    krb5_auth_con_setflags(context, auth_context,
                           KRB5_AUTH_CONTEXT_DO_SEQUENCE);

    krb5_auth_con_getauthenticator(context, auth_context, &authdat);

    major_status = process_checksum(minor_status, context, input_chan_bindings,
                                    auth_context, ap_req_options,
                                    authdat, exts,
                                    delegated_cred_handle ? &deleg_cred : NULL,
                                    &gss_flags, &code);

    if (major_status != GSS_S_COMPLETE)
        goto fail;

    if (exts->iakerb.conv && !exts->iakerb.verified) {
        major_status = GSS_S_BAD_SIG;
        goto fail;
    }

    /* only DCE_STYLE clients are allowed to send raw AP-REQs */
    if (no_encap != ((gss_flags & GSS_C_DCE_STYLE) != 0)) {
        major_status = GSS_S_DEFECTIVE_TOKEN;
        goto fail;
    }

    /* create the ctx struct and start filling it in */

    if ((ctx = (krb5_gss_ctx_id_rec *) xmalloc(sizeof(krb5_gss_ctx_id_rec)))
        == NULL) {
        code = ENOMEM;
        major_status = GSS_S_FAILURE;
        goto fail;
    }

    memset(ctx, 0, sizeof(krb5_gss_ctx_id_rec));
    ctx->magic = KG_CONTEXT;
    ctx->mech_used = (gss_OID) mech_used;
    ctx->auth_context = auth_context;
    ctx->initiate = 0;
    ctx->gss_flags = gss_flags | GSS_C_TRANS_FLAG;
    ctx->seed_init = 0;
    ctx->cred_rcache = cred_rcache;

    /* XXX move this into gss_name_t */
    if (        (code = krb5_merge_authdata(context,
                                            ticket->enc_part2->authorization_data,
                                            authdat->authorization_data,
                                            &ctx->authdata))) {
        major_status = GSS_S_FAILURE;
        goto fail;
    }
    if ((code = kg_init_name(context, ticket->server, NULL, NULL, NULL, 0,
                             &ctx->here))) {
        major_status = GSS_S_FAILURE;
        goto fail;
    }
    if ((code = krb5_auth_con_get_authdata_context(context, auth_context,
                                                   &ad_context))) {
        major_status = GSS_S_FAILURE;
        goto fail;
    }
    if ((code = kg_init_name(context, authdat->client, NULL, NULL,
                             ad_context, KG_INIT_NAME_NO_COPY, &ctx->there))) {
        major_status = GSS_S_FAILURE;
        goto fail;
    }
    /* Now owned by ctx->there */
    authdat->client = NULL;
    krb5_auth_con_set_authdata_context(context, auth_context, NULL);

    if ((code = krb5_auth_con_getrecvsubkey_k(context, auth_context,
                                              &ctx->subkey))) {
        major_status = GSS_S_FAILURE;
        goto fail;
    }

    /* use the session key if the subkey isn't present */

    if (ctx->subkey == NULL) {
        if ((code = krb5_auth_con_getkey_k(context, auth_context,
                                           &ctx->subkey))) {
            major_status = GSS_S_FAILURE;
            goto fail;
        }
    }

    if (ctx->subkey == NULL) {
        /* this isn't a very good error, but it's not clear to me this
           can actually happen */
        major_status = GSS_S_FAILURE;
        code = KRB5KDC_ERR_NULL_KEY;
        goto fail;
    }

    ctx->enc = NULL;
    ctx->seq = NULL;
    ctx->have_acceptor_subkey = 0;
    /* DCE_STYLE implies acceptor_subkey */
    if ((ctx->gss_flags & GSS_C_DCE_STYLE) == 0) {
        code = kg_setup_keys(context, ctx, ctx->subkey, &ctx->cksumtype);
        if (code) {
            major_status = GSS_S_FAILURE;
            goto fail;
        }
    }
    ctx->krb_times = ticket->enc_part2->times; /* struct copy */
    ctx->krb_flags = ticket->enc_part2->flags;

    if (delegated_cred_handle != NULL &&
        deleg_cred == NULL && /* no unconstrained delegation */
        cred->usage == GSS_C_BOTH) {
        /*
         * Now, we always fabricate a delegated credentials handle
         * containing the service ticket to ourselves, which can be
         * used for S4U2Proxy.
         */
        major_status = create_constrained_deleg_creds(minor_status, cred,
                                                      ticket, &deleg_cred,
                                                      context);
        if (GSS_ERROR(major_status))
            goto fail;
        ctx->gss_flags |= GSS_C_DELEG_FLAG;
    }

    {
        krb5_int32 seq_temp;
        krb5_auth_con_getremoteseqnumber(context, auth_context, &seq_temp);
        ctx->seq_recv = (uint32_t)seq_temp;
    }

    if ((code = krb5_timeofday(context, &now))) {
        major_status = GSS_S_FAILURE;
        goto fail;
    }

    code = g_seqstate_init(&ctx->seqstate, ctx->seq_recv,
                           (ctx->gss_flags & GSS_C_REPLAY_FLAG) != 0,
                           (ctx->gss_flags & GSS_C_SEQUENCE_FLAG) != 0,
                           ctx->proto);
    if (code) {
        major_status = GSS_S_FAILURE;
        goto fail;
    }

    /* DCE_STYLE implies mutual authentication */
    if (ctx->gss_flags & GSS_C_DCE_STYLE)
        ctx->gss_flags |= GSS_C_MUTUAL_FLAG;

    /* at this point, the entire context structure is filled in,
       so it can be released.  */

    /* generate an AP_REP if necessary */

    if (ctx->gss_flags & GSS_C_MUTUAL_FLAG) {
        krb5_int32 seq_temp;
        int cfx_generate_subkey;

        /*
         * Do not generate a subkey per RFC 4537 unless we are upgrading to CFX,
         * because pre-CFX tokens do not indicate which key to use. (Note that
         * DCE_STYLE implies that we will use a subkey.)
         */
        if (ctx->proto == 0 &&
            (ctx->gss_flags & GSS_C_DCE_STYLE) == 0 &&
            (ap_req_options & AP_OPTS_USE_SUBKEY)) {
            code = (*kaccess.auth_con_get_subkey_enctype)(context,
                                                          auth_context,
                                                          &negotiated_etype);
            if (code != 0) {
                major_status = GSS_S_FAILURE;
                goto fail;
            }

            switch (negotiated_etype) {
            case ENCTYPE_DES3_CBC_SHA1:
            case ENCTYPE_ARCFOUR_HMAC:
            case ENCTYPE_ARCFOUR_HMAC_EXP:
                /* RFC 4121 accidentally omits RC4-HMAC-EXP as a "not-newer"
                 * enctype, even though RFC 4757 treats it as one. */
                ap_req_options &= ~(AP_OPTS_USE_SUBKEY);
                break;
            }
        }

        if (ctx->proto == 1 || (ctx->gss_flags & GSS_C_DCE_STYLE) ||
            (ap_req_options & AP_OPTS_USE_SUBKEY))
            cfx_generate_subkey = CFX_ACCEPTOR_SUBKEY;
        else
            cfx_generate_subkey = 0;

        if (cfx_generate_subkey) {
            krb5_int32 acflags;
            code = krb5_auth_con_getflags(context, auth_context, &acflags);
            if (code == 0) {
                acflags |= KRB5_AUTH_CONTEXT_USE_SUBKEY;
                code = krb5_auth_con_setflags(context, auth_context, acflags);
            }
            if (code) {
                major_status = GSS_S_FAILURE;
                goto fail;
            }
        }

        if ((code = krb5_mk_rep(context, auth_context, &ap_rep))) {
            major_status = GSS_S_FAILURE;
            goto fail;
        }

        krb5_auth_con_getlocalseqnumber(context, auth_context, &seq_temp);
        ctx->seq_send = (uint32_t)seq_temp;

        if (cfx_generate_subkey) {
            /* Get the new acceptor subkey.  With the code above, there
               should always be one if we make it to this point.  */
            code = krb5_auth_con_getsendsubkey_k(context, auth_context,
                                                 &ctx->acceptor_subkey);
            if (code != 0) {
                major_status = GSS_S_FAILURE;
                goto fail;
            }
            ctx->have_acceptor_subkey = 1;

            code = kg_setup_keys(context, ctx, ctx->acceptor_subkey,
                                 &ctx->acceptor_subkey_cksumtype);
            if (code) {
                major_status = GSS_S_FAILURE;
                goto fail;
            }
        }

        /* the reply token hasn't been sent yet, but that's ok. */
        if (ctx->gss_flags & GSS_C_DCE_STYLE) {
            assert(ctx->have_acceptor_subkey);

            /* in order to force acceptor subkey to be used, don't set PROT_READY */

            /* Raw AP-REP is returned */
            code = data_to_gss(&ap_rep, output_token);
            if (code)
            {
                major_status = GSS_S_FAILURE;
                goto fail;
            }

            ctx->established = 0;

            *context_handle = (gss_ctx_id_t)ctx;
            *minor_status = 0;
            major_status = GSS_S_CONTINUE_NEEDED;

            /* Only last leg should set return arguments */
            goto fail;
        } else
            ctx->gss_flags |= GSS_C_PROT_READY_FLAG;

        ctx->established = 1;

        token.length = g_token_size(mech_used, ap_rep.length);
        token.value = gssalloc_malloc(token.length);
        if (token.value == NULL) {
            major_status = GSS_S_FAILURE;
            code = ENOMEM;
            goto fail;
        }
        k5_buf_init_fixed(&buf, token.value, token.length);
        g_make_token_header(&buf, mech_used, ap_rep.length, KG_TOK_CTX_AP_REP);
        k5_buf_add_len(&buf, ap_rep.data, ap_rep.length);
        assert(buf.len == token.length);

        ctx->established = 1;

    } else {
        token.length = 0;
        token.value = NULL;
        ctx->seq_send = ctx->seq_recv;

        ctx->established = 1;
    }

    /* set the return arguments */

    if (src_name) {
        code = kg_duplicate_name(context, ctx->there, &name);
        if (code) {
            major_status = GSS_S_FAILURE;
            goto fail;
        }
    }

    if (mech_type)
        *mech_type = (gss_OID) mech_used;

    /* Add the maximum allowable clock skew as a grace period for context
     * expiration, just as we do for the ticket. */
    if (time_rec) {
        *time_rec = ts_interval(ts_incr(now, -context->clockskew),
                                ctx->krb_times.endtime);
    }

    if (ret_flags)
        *ret_flags = ctx->gss_flags;

    *context_handle = (gss_ctx_id_t)ctx;
    *output_token = token;

    if (src_name)
        *src_name = (gss_name_t) name;

    if (delegated_cred_handle)
        *delegated_cred_handle = (gss_cred_id_t) deleg_cred;

    /* finally! */

    *minor_status = 0;
    major_status = GSS_S_COMPLETE;

fail:
    if (authdat)
        krb5_free_authenticator(context, authdat);
    /* The ctx structure has the handle of the auth_context */
    if (auth_context && !ctx) {
        if (cred_rcache)
            (void)krb5_auth_con_setrcache(context, auth_context, NULL);

        krb5_auth_con_free(context, auth_context);
    }
    if (ap_rep.data)
        krb5_free_data_contents(context, &ap_rep);
    if (major_status == GSS_S_COMPLETE ||
        (major_status == GSS_S_CONTINUE_NEEDED && code != KRB5KRB_AP_ERR_MSG_TYPE)) {
        ctx->k5_context = context;
        context = NULL;
        goto done;
    }

    /* from here on is the real "fail" code */

    if (ctx)
        (void) krb5_gss_delete_sec_context(&tmp_minor_status,
                                           (gss_ctx_id_t *) &ctx, NULL);
    if (deleg_cred) { /* free memory associated with the deleg credential */
        if (deleg_cred->ccache)
            (void)krb5_cc_close(context, deleg_cred->ccache);
        if (deleg_cred->name)
            kg_release_name(context, &deleg_cred->name);
        xfree(deleg_cred);
    }
    if (token.value)
        xfree(token.value);
    if (name) {
        (void) kg_release_name(context, &name);
    }

    *minor_status = code;

    /* We may have failed before being able to read the GSS flags from the
     * authenticator, so also check the request AP options. */
    if (cred != NULL && request != NULL &&
        ((gss_flags & GSS_C_MUTUAL_FLAG) ||
         (request->ap_options & AP_OPTS_MUTUAL_REQUIRED) ||
         major_status == GSS_S_CONTINUE_NEEDED)) {
        unsigned int tmsglen;

        /*
         * The client is expecting a response, so we can send an
         * error token back
         */
        memset(&krb_error_data, 0, sizeof(krb_error_data));

        code -= ERROR_TABLE_BASE_krb5;
        if (code < 0 || code > KRB_ERR_MAX)
            code = 60 /* KRB_ERR_GENERIC */;

        krb_error_data.error = code;
        (void) krb5_us_timeofday(context, &krb_error_data.stime,
                                 &krb_error_data.susec);

        krb_error_data.server = ticket->server;
        code = krb5_mk_error(context, &krb_error_data, &scratch);
        if (code)
            goto done;

        tmsglen = scratch.length;

        token.length = g_token_size(mech_used, tmsglen);
        token.value = gssalloc_malloc(token.length);
        if (!token.value)
            goto done;
        k5_buf_init_fixed(&buf, token.value, token.length);
        g_make_token_header(&buf, mech_used, tmsglen, KG_TOK_CTX_ERROR);
        k5_buf_add_len(&buf, scratch.data, scratch.length);
        assert(buf.len == token.length);

        krb5_free_data_contents(context, &scratch);

        *output_token = token;
    }

done:
    krb5_free_ap_req(context, request);
    if (cred)
        k5_mutex_unlock(&cred->lock);
    if (defcred)
        krb5_gss_release_cred(&tmp_minor_status, &defcred);
    if (context) {
        if (major_status && *minor_status)
            save_error_info(*minor_status, context);
        krb5_free_context(context);
    }
    return (major_status);
}
#endif /* LEAN_CLIENT */

OM_uint32 KRB5_CALLCONV
krb5_gss_accept_sec_context_ext(
    OM_uint32 *minor_status,
    gss_ctx_id_t *context_handle,
    gss_cred_id_t verifier_cred_handle,
    gss_buffer_t input_token,
    gss_channel_bindings_t input_chan_bindings,
    gss_name_t *src_name,
    gss_OID *mech_type,
    gss_buffer_t output_token,
    OM_uint32 *ret_flags,
    OM_uint32 *time_rec,
    gss_cred_id_t *delegated_cred_handle,
    krb5_gss_ctx_ext_t exts)
{
    krb5_gss_ctx_id_rec *ctx = (krb5_gss_ctx_id_rec *)*context_handle;

    /*
     * Context handle must be unspecified.  Actually, it must be
     * non-established, but currently, accept_sec_context never returns
     * a non-established context handle.
     */
    /*SUPPRESS 29*/
    if (ctx != NULL) {
        if (ctx->established == 0 && (ctx->gss_flags & GSS_C_DCE_STYLE)) {
            return kg_accept_dce(minor_status, context_handle,
                                 verifier_cred_handle, input_token,
                                 input_chan_bindings, src_name, mech_type,
                                 output_token, ret_flags, time_rec,
                                 delegated_cred_handle);
        } else {
            *minor_status = EINVAL;
            save_error_string(EINVAL, "accept_sec_context called with existing context handle");
            return GSS_S_FAILURE;
        }
    }

    return kg_accept_krb5(minor_status, context_handle,
                          verifier_cred_handle, input_token,
                          input_chan_bindings, src_name, mech_type,
                          output_token, ret_flags, time_rec,
                          delegated_cred_handle, exts);
}

OM_uint32 KRB5_CALLCONV
krb5_gss_accept_sec_context(minor_status, context_handle,
                            verifier_cred_handle, input_token,
                            input_chan_bindings, src_name, mech_type,
                            output_token, ret_flags, time_rec,
                            delegated_cred_handle)
    OM_uint32 *minor_status;
    gss_ctx_id_t *context_handle;
    gss_cred_id_t verifier_cred_handle;
    gss_buffer_t input_token;
    gss_channel_bindings_t input_chan_bindings;
    gss_name_t *src_name;
    gss_OID *mech_type;
    gss_buffer_t output_token;
    OM_uint32 *ret_flags;
    OM_uint32 *time_rec;
    gss_cred_id_t *delegated_cred_handle;
{
    krb5_gss_ctx_ext_rec exts;

    memset(&exts, 0, sizeof(exts));

    return krb5_gss_accept_sec_context_ext(minor_status,
                                           context_handle,
                                           verifier_cred_handle,
                                           input_token,
                                           input_chan_bindings,
                                           src_name,
                                           mech_type,
                                           output_token,
                                           ret_flags,
                                           time_rec,
                                           delegated_cred_handle,
                                           &exts);
}
