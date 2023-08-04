/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Copyright (C) 2011-2018 PADL Software Pty Ltd.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "k5-platform.h"
#include "gssapiP_spnego.h"
#include <generic/gssapiP_generic.h>

/*
 * The initial context token emitted by the initiator is a INITIATOR_NEGO
 * message followed by zero or more INITIATOR_META_DATA tokens, and zero
 * or one AP_REQUEST tokens.
 *
 * Upon receiving this, the acceptor computes the list of mutually supported
 * authentication mechanisms and performs the metadata exchange. The output
 * token is ACCEPTOR_NEGO followed by zero or more ACCEPTOR_META_DATA tokens,
 * and zero or one CHALLENGE tokens.
 *
 * Once the metadata exchange is complete and a mechanism is selected, the
 * selected mechanism's context token exchange continues with AP_REQUEST and
 * CHALLENGE messages.
 *
 * Once the context token exchange is complete, VERIFY messages are sent to
 * authenticate the entire exchange.
 */

static void
zero_and_release_buffer_set(gss_buffer_set_t *pbuffers)
{
    OM_uint32 tmpmin;
    gss_buffer_set_t buffers = *pbuffers;
    uint32_t i;

    if (buffers != GSS_C_NO_BUFFER_SET) {
        for (i = 0; i < buffers->count; i++)
            zap(buffers->elements[i].value, buffers->elements[i].length);

        gss_release_buffer_set(&tmpmin, &buffers);
    }

    *pbuffers = GSS_C_NO_BUFFER_SET;
}

static OM_uint32
buffer_set_to_key(OM_uint32 *minor, gss_buffer_set_t buffers,
                  krb5_keyblock *key)
{
    krb5_error_code ret;

    /* Returned keys must be in two buffers, with the key contents in the first
     * and the enctype as a 32-bit little-endian integer in the second. */
    if (buffers->count != 2 || buffers->elements[1].length != 4) {
        *minor = ERR_NEGOEX_NO_VERIFY_KEY;
        return GSS_S_FAILURE;
    }

    krb5_free_keyblock_contents(NULL, key);

    key->contents = k5memdup(buffers->elements[0].value,
                             buffers->elements[0].length, &ret);
    if (key->contents == NULL) {
        *minor = ret;
        return GSS_S_FAILURE;
    }
    key->length = buffers->elements[0].length;
    key->enctype = load_32_le(buffers->elements[1].value);

    return GSS_S_COMPLETE;
}

static OM_uint32
get_session_keys(OM_uint32 *minor, struct negoex_auth_mech *mech)
{
    OM_uint32 major, tmpmin;
    gss_buffer_set_t buffers = GSS_C_NO_BUFFER_SET;

    major = gss_inquire_sec_context_by_oid(&tmpmin, mech->mech_context,
                                           GSS_C_INQ_NEGOEX_KEY, &buffers);
    if (major == GSS_S_COMPLETE) {
        major = buffer_set_to_key(minor, buffers, &mech->key);
        zero_and_release_buffer_set(&buffers);
        if (major != GSS_S_COMPLETE)
            return major;
    }

    major = gss_inquire_sec_context_by_oid(&tmpmin, mech->mech_context,
                                           GSS_C_INQ_NEGOEX_VERIFY_KEY,
                                           &buffers);
    if (major == GSS_S_COMPLETE) {
        major = buffer_set_to_key(minor, buffers, &mech->verify_key);
        zero_and_release_buffer_set(&buffers);
        if (major != GSS_S_COMPLETE)
            return major;
    }

    return GSS_S_COMPLETE;
}

static OM_uint32
emit_initiator_nego(OM_uint32 *minor, spnego_gss_ctx_id_t ctx)
{
    OM_uint32 major;
    uint8_t random[32];

    major = negoex_random(minor, ctx, random, 32);
    if (major != GSS_S_COMPLETE)
        return major;

    negoex_add_nego_message(ctx, INITIATOR_NEGO, random);
    return GSS_S_COMPLETE;
}

static OM_uint32
process_initiator_nego(OM_uint32 *minor, spnego_gss_ctx_id_t ctx,
                       struct negoex_message *messages, size_t nmessages)
{
    struct nego_message *msg;

    assert(!ctx->initiate && ctx->negoex_step == 1);

    msg = negoex_locate_nego_message(messages, nmessages, INITIATOR_NEGO);
    if (msg == NULL) {
        *minor = ERR_NEGOEX_MISSING_NEGO_MESSAGE;
        return GSS_S_DEFECTIVE_TOKEN;
    }

    negoex_restrict_auth_schemes(ctx, msg->schemes, msg->nschemes);
    return GSS_S_COMPLETE;
}

static OM_uint32
emit_acceptor_nego(OM_uint32 *minor, spnego_gss_ctx_id_t ctx)
{
    OM_uint32 major;
    uint8_t random[32];

    major = negoex_random(minor, ctx, random, 32);
    if (major != GSS_S_COMPLETE)
        return major;

    negoex_add_nego_message(ctx, ACCEPTOR_NEGO, random);
    return GSS_S_COMPLETE;
}

static OM_uint32
process_acceptor_nego(OM_uint32 *minor, spnego_gss_ctx_id_t ctx,
                      struct negoex_message *messages, size_t nmessages)
{
    struct nego_message *msg;

    msg = negoex_locate_nego_message(messages, nmessages, ACCEPTOR_NEGO);
    if (msg == NULL) {
        *minor = ERR_NEGOEX_MISSING_NEGO_MESSAGE;
        return GSS_S_DEFECTIVE_TOKEN;
    }

    /* Reorder and prune our mech list to match the acceptor's list (or a
     * subset of it). */
    negoex_common_auth_schemes(ctx, msg->schemes, msg->nschemes);

    return GSS_S_COMPLETE;
}

static void
query_meta_data(spnego_gss_ctx_id_t ctx, gss_cred_id_t cred,
                gss_name_t target, OM_uint32 req_flags)
{
    OM_uint32 major, minor;
    struct negoex_auth_mech *p, *next;

    K5_TAILQ_FOREACH_SAFE(p, &ctx->negoex_mechs, links, next) {
        major = gssspi_query_meta_data(&minor, p->oid, cred, &p->mech_context,
                                       target, req_flags, &p->metadata);
        /* GSS_Query_meta_data failure removes mechanism from list. */
        if (major != GSS_S_COMPLETE)
            negoex_delete_auth_mech(ctx, p);
    }
}

static void
exchange_meta_data(spnego_gss_ctx_id_t ctx, gss_cred_id_t cred,
                   gss_name_t target, OM_uint32 req_flags,
                   struct negoex_message *messages, size_t nmessages)
{
    OM_uint32 major, minor;
    struct negoex_auth_mech *mech;
    enum message_type type;
    struct exchange_message *msg;
    uint32_t i;

    type = ctx->initiate ? ACCEPTOR_META_DATA : INITIATOR_META_DATA;

    for (i = 0; i < nmessages; i++) {
        if (messages[i].type != type)
            continue;
        msg = &messages[i].u.e;

        mech = negoex_locate_auth_scheme(ctx, msg->scheme);
        if (mech == NULL)
            continue;

        major = gssspi_exchange_meta_data(&minor, mech->oid, cred,
                                          &mech->mech_context, target,
                                          req_flags, &msg->token);
        /* GSS_Exchange_meta_data failure removes mechanism from list. */
        if (major != GSS_S_COMPLETE)
            negoex_delete_auth_mech(ctx, mech);
    }
}

/*
 * In the initiator, if we are processing the acceptor's first reply, discard
 * the optimistic context if the acceptor ignored the optimistic token.  If the
 * acceptor continued the optimistic mech, discard all other mechs.
 */
static void
check_optimistic_result(spnego_gss_ctx_id_t ctx,
                        struct negoex_message *messages, size_t nmessages)
{
    struct negoex_auth_mech *mech;
    OM_uint32 tmpmin;

    assert(ctx->initiate && ctx->negoex_step == 2);

    /* Do nothing if we didn't make an optimistic context. */
    mech = K5_TAILQ_FIRST(&ctx->negoex_mechs);
    if (mech == NULL || mech->mech_context == GSS_C_NO_CONTEXT)
        return;

    /* If the acceptor used the optimistic token, it will send an acceptor
     * token or a checksum (or both) in its first reply. */
    if (negoex_locate_exchange_message(messages, nmessages,
                                       CHALLENGE) != NULL ||
        negoex_locate_verify_message(messages, nmessages) != NULL) {
        /* The acceptor continued the optimistic mech, and metadata exchange
         * didn't remove it.  Commit to this mechanism. */
        negoex_select_auth_mech(ctx, mech);
    } else {
        /* The acceptor ignored the optimistic token.  Restart the mech. */
        (void)gss_delete_sec_context(&tmpmin, &mech->mech_context, NULL);
        krb5_free_keyblock_contents(NULL, &mech->key);
        krb5_free_keyblock_contents(NULL, &mech->verify_key);
        mech->complete = mech->sent_checksum = FALSE;
    }
}

/* Perform an initiator step of the underlying mechanism exchange. */
static OM_uint32
mech_init(OM_uint32 *minor, spnego_gss_ctx_id_t ctx, gss_cred_id_t cred,
          gss_name_t target, OM_uint32 req_flags, OM_uint32 time_req,
          struct negoex_message *messages, size_t nmessages,
          gss_channel_bindings_t bindings, gss_buffer_t output_token,
          OM_uint32 *time_rec)
{
    OM_uint32 major, first_major = 0, first_minor = 0;
    struct negoex_auth_mech *mech = NULL;
    gss_buffer_t input_token = GSS_C_NO_BUFFER;
    struct exchange_message *msg;
    int first_mech;

    output_token->value = NULL;
    output_token->length = 0;

    /* Allow disabling of optimistic token for testing. */
    if (ctx->negoex_step == 1 &&
        secure_getenv("NEGOEX_NO_OPTIMISTIC_TOKEN") != NULL)
        return GSS_S_COMPLETE;

    if (K5_TAILQ_EMPTY(&ctx->negoex_mechs)) {
        *minor = ERR_NEGOEX_NO_AVAILABLE_MECHS;
        return GSS_S_FAILURE;
    }

    /*
     * Get the input token.  The challenge could be for the optimistic mech,
     * which we might have discarded in metadata exchange, so ignore the
     * challenge if it doesn't match the first auth mech.
     */
    mech = K5_TAILQ_FIRST(&ctx->negoex_mechs);
    msg = negoex_locate_exchange_message(messages, nmessages, CHALLENGE);
    if (msg != NULL && GUID_EQ(msg->scheme, mech->scheme))
        input_token = &msg->token;

    if (mech->complete)
        return GSS_S_COMPLETE;

    first_mech = TRUE;

    while (!K5_TAILQ_EMPTY(&ctx->negoex_mechs)) {
        mech = K5_TAILQ_FIRST(&ctx->negoex_mechs);

        major = gss_init_sec_context(minor, cred, &mech->mech_context, target,
                                     mech->oid, req_flags, time_req, bindings,
                                     input_token, &ctx->actual_mech,
                                     output_token, &ctx->ctx_flags, time_rec);

        if (major == GSS_S_COMPLETE)
            mech->complete = 1;

        if (!GSS_ERROR(major))
            return get_session_keys(minor, mech);

        /* Remember the error we got from the first mech. */
        if (first_mech) {
            first_major = major;
            first_minor = *minor;
        }

        /* If we still have multiple mechs to try, move on to the next one. */
        negoex_delete_auth_mech(ctx, mech);
        first_mech = FALSE;
        input_token = GSS_C_NO_BUFFER;
    }

    if (K5_TAILQ_EMPTY(&ctx->negoex_mechs)) {
        major = first_major;
        *minor = first_minor;
    }

    return major;
}

/* Perform an acceptor step of the underlying mechanism exchange. */
static OM_uint32
mech_accept(OM_uint32 *minor, spnego_gss_ctx_id_t ctx,
            gss_cred_id_t cred, struct negoex_message *messages,
            size_t nmessages, gss_channel_bindings_t bindings,
            gss_buffer_t output_token, OM_uint32 *time_rec)
{
    OM_uint32 major, tmpmin;
    struct negoex_auth_mech *mech;
    struct exchange_message *msg;

    assert(!ctx->initiate && !K5_TAILQ_EMPTY(&ctx->negoex_mechs));

    msg = negoex_locate_exchange_message(messages, nmessages, AP_REQUEST);
    if (msg == NULL) {
        /* No input token is okay on the first request or if the mech is
         * complete. */
        if (ctx->negoex_step == 1 ||
            K5_TAILQ_FIRST(&ctx->negoex_mechs)->complete)
            return GSS_S_COMPLETE;
        *minor = ERR_NEGOEX_MISSING_AP_REQUEST_MESSAGE;
        return GSS_S_DEFECTIVE_TOKEN;
    }

    if (ctx->negoex_step == 1) {
        /* Ignore the optimistic token if it isn't for our most preferred
         * mech. */
        mech = K5_TAILQ_FIRST(&ctx->negoex_mechs);
        if (!GUID_EQ(msg->scheme, mech->scheme))
            return GSS_S_COMPLETE;
    } else {
        /* The initiator has selected a mech; discard other entries. */
        mech = negoex_locate_auth_scheme(ctx, msg->scheme);
        if (mech == NULL) {
            *minor = ERR_NEGOEX_NO_AVAILABLE_MECHS;
            return GSS_S_FAILURE;
        }
        negoex_select_auth_mech(ctx, mech);
    }

    if (mech->complete)
        return GSS_S_COMPLETE;

    if (ctx->internal_name != GSS_C_NO_NAME)
        gss_release_name(&tmpmin, &ctx->internal_name);
    if (ctx->deleg_cred != GSS_C_NO_CREDENTIAL)
        gss_release_cred(&tmpmin, &ctx->deleg_cred);

    major = gss_accept_sec_context(minor, &mech->mech_context, cred,
                                   &msg->token, bindings, &ctx->internal_name,
                                   &ctx->actual_mech, output_token,
                                   &ctx->ctx_flags, time_rec,
                                   &ctx->deleg_cred);

    if (major == GSS_S_COMPLETE)
        mech->complete = 1;

    if (!GSS_ERROR(major)) {
        major = get_session_keys(minor, mech);
    } else if (ctx->negoex_step == 1) {
        /* This was an optimistic token; pretend this never happened. */
        major = GSS_S_COMPLETE;
        *minor = 0;
        gss_release_buffer(&tmpmin, output_token);
        gss_delete_sec_context(&tmpmin, &mech->mech_context, GSS_C_NO_BUFFER);
    }

    return major;
}

static krb5_keyusage
verify_keyusage(spnego_gss_ctx_id_t ctx, int make_checksum)
{
    /* Of course, these are the wrong way around in the spec. */
    return (ctx->initiate ^ !make_checksum) ?
        NEGOEX_KEYUSAGE_ACCEPTOR_CHECKSUM : NEGOEX_KEYUSAGE_INITIATOR_CHECKSUM;
}

static OM_uint32
verify_checksum(OM_uint32 *minor, spnego_gss_ctx_id_t ctx,
                struct negoex_message *messages, size_t nmessages,
                gss_buffer_t input_token, int *send_alert_out)
{
    krb5_error_code ret;
    struct negoex_auth_mech *mech = K5_TAILQ_FIRST(&ctx->negoex_mechs);
    struct verify_message *msg;
    krb5_crypto_iov iov[3];
    krb5_keyusage usage = verify_keyusage(ctx, FALSE);
    krb5_boolean valid;

    *send_alert_out = FALSE;
    assert(mech != NULL);

    /* The other party may not be ready to send a verify token yet, or (in the
     * first initiator step) may send one for a mechanism we don't support. */
    msg = negoex_locate_verify_message(messages, nmessages);
    if (msg == NULL || !GUID_EQ(msg->scheme, mech->scheme))
        return GSS_S_COMPLETE;

    /* A recoverable error may cause us to be unable to verify a token from the
     * other party.  In this case we should send an alert. */
    if (mech->verify_key.enctype == ENCTYPE_NULL) {
        *send_alert_out = TRUE;
        return GSS_S_COMPLETE;
    }

    /* Verify the checksum over the existing transcript and the portion of the
     * input token leading up to the verify message. */
    assert(input_token != NULL);
    iov[0].flags = KRB5_CRYPTO_TYPE_DATA;
    iov[0].data = make_data(ctx->negoex_transcript.data,
                            ctx->negoex_transcript.len);
    iov[1].flags = KRB5_CRYPTO_TYPE_DATA;
    iov[1].data = make_data(input_token->value, msg->offset_in_token);
    iov[2].flags = KRB5_CRYPTO_TYPE_CHECKSUM;
    iov[2].data = make_data((uint8_t *)msg->cksum, msg->cksum_len);

    ret = krb5_c_verify_checksum_iov(ctx->kctx, msg->cksum_type,
                                     &mech->verify_key, usage, iov, 3, &valid);
    if (ret) {
        *minor = ret;
        return GSS_S_FAILURE;
    }
    if (!valid || !krb5_c_is_keyed_cksum(msg->cksum_type)) {
        *minor = ERR_NEGOEX_INVALID_CHECKSUM;
        return GSS_S_BAD_SIG;
    }

    mech->verified_checksum = TRUE;
    return GSS_S_COMPLETE;
}

static OM_uint32
make_checksum(OM_uint32 *minor, spnego_gss_ctx_id_t ctx)
{
    krb5_error_code ret;
    krb5_data d;
    krb5_keyusage usage = verify_keyusage(ctx, TRUE);
    krb5_checksum cksum;
    struct negoex_auth_mech *mech = K5_TAILQ_FIRST(&ctx->negoex_mechs);

    assert(mech != NULL);

    if (mech->key.enctype == ENCTYPE_NULL) {
        if (mech->complete) {
            *minor = ERR_NEGOEX_NO_VERIFY_KEY;
            return GSS_S_UNAVAILABLE;
        } else {
            return GSS_S_COMPLETE;
        }
    }

    d = make_data(ctx->negoex_transcript.data, ctx->negoex_transcript.len);
    ret = krb5_c_make_checksum(ctx->kctx, 0, &mech->key, usage, &d, &cksum);
    if (ret) {
        *minor = ret;
        return GSS_S_FAILURE;
    }

    negoex_add_verify_message(ctx, mech->scheme, cksum.checksum_type,
                              cksum.contents, cksum.length);

    mech->sent_checksum = TRUE;
    krb5_free_checksum_contents(ctx->kctx, &cksum);
    return GSS_S_COMPLETE;
}

/* If the other side sent a VERIFY_NO_KEY pulse alert, clear the checksum state
 * on the mechanism so that we send another VERIFY message. */
static void
process_alerts(spnego_gss_ctx_id_t ctx,
               struct negoex_message *messages, uint32_t nmessages)
{
    struct alert_message *msg;
    struct negoex_auth_mech *mech;

    msg = negoex_locate_alert_message(messages, nmessages);
    if (msg != NULL && msg->verify_no_key) {
        mech = negoex_locate_auth_scheme(ctx, msg->scheme);
        if (mech != NULL) {
            mech->sent_checksum = FALSE;
            krb5_free_keyblock_contents(NULL, &mech->key);
            krb5_free_keyblock_contents(NULL, &mech->verify_key);
        }
    }
}

static OM_uint32
make_output_token(OM_uint32 *minor, spnego_gss_ctx_id_t ctx,
                  gss_buffer_t mech_output_token, int send_alert,
                  gss_buffer_t output_token)
{
    OM_uint32 major;
    struct negoex_auth_mech *mech;
    enum message_type type;
    size_t old_transcript_len = ctx->negoex_transcript.len;

    output_token->length = 0;
    output_token->value = NULL;

    /* If the mech is complete and we previously sent a checksum, we just
     * processed the last leg and don't need to send another token. */
    if (mech_output_token->length == 0 &&
        K5_TAILQ_FIRST(&ctx->negoex_mechs)->sent_checksum)
        return GSS_S_COMPLETE;

    if (ctx->negoex_step == 1) {
        if (ctx->initiate)
            major = emit_initiator_nego(minor, ctx);
        else
            major = emit_acceptor_nego(minor, ctx);
        if (major != GSS_S_COMPLETE)
            return major;

        type = ctx->initiate ? INITIATOR_META_DATA : ACCEPTOR_META_DATA;
        K5_TAILQ_FOREACH(mech, &ctx->negoex_mechs, links) {
            if (mech->metadata.length > 0) {
                negoex_add_exchange_message(ctx, type, mech->scheme,
                                            &mech->metadata);
            }
        }
    }

    mech = K5_TAILQ_FIRST(&ctx->negoex_mechs);

    if (mech_output_token->length > 0) {
        type = ctx->initiate ? AP_REQUEST : CHALLENGE;
        negoex_add_exchange_message(ctx, type, mech->scheme,
                                    mech_output_token);
    }

    if (send_alert)
        negoex_add_verify_no_key_alert(ctx, mech->scheme);

    /* Try to add a VERIFY message if we haven't already done so. */
    if (!mech->sent_checksum) {
        major = make_checksum(minor, ctx);
        if (major != GSS_S_COMPLETE)
            return major;
    }

    if (ctx->negoex_transcript.data == NULL) {
        *minor = ENOMEM;
        return GSS_S_FAILURE;
    }

    /* Copy what we added to the transcript into the output token. */
    output_token->length = ctx->negoex_transcript.len - old_transcript_len;
    output_token->value = gssalloc_malloc(output_token->length);
    if (output_token->value == NULL) {
        *minor = ENOMEM;
        return GSS_S_FAILURE;
    }
    memcpy(output_token->value,
           (uint8_t *)ctx->negoex_transcript.data + old_transcript_len,
           output_token->length);

    return GSS_S_COMPLETE;
}

OM_uint32
negoex_init(OM_uint32 *minor, spnego_gss_ctx_id_t ctx, gss_cred_id_t cred,
            gss_name_t target_name, OM_uint32 req_flags, OM_uint32 time_req,
            gss_buffer_t input_token, gss_channel_bindings_t bindings,
            gss_buffer_t output_token, OM_uint32 *time_rec)
{
    OM_uint32 major, tmpmin;
    gss_buffer_desc mech_output_token = GSS_C_EMPTY_BUFFER;
    struct negoex_message *messages = NULL;
    struct negoex_auth_mech *mech;
    size_t nmessages = 0;
    int send_alert = FALSE;

    if (ctx->negoex_step == 0 && input_token != GSS_C_NO_BUFFER &&
        input_token->length != 0)
        return GSS_S_DEFECTIVE_TOKEN;

    major = negoex_prep_context_for_negoex(minor, ctx);
    if (major != GSS_S_COMPLETE)
        goto cleanup;

    ctx->negoex_step++;

    if (input_token != GSS_C_NO_BUFFER && input_token->length > 0) {
        major = negoex_parse_token(minor, ctx, input_token, &messages,
                                   &nmessages);
        if (major != GSS_S_COMPLETE)
            goto cleanup;
    }

    process_alerts(ctx, messages, nmessages);

    if (ctx->negoex_step == 1) {
        /* Choose a random conversation ID. */
        major = negoex_random(minor, ctx, ctx->negoex_conv_id, GUID_LENGTH);
        if (major != GSS_S_COMPLETE)
            goto cleanup;

        /* Query each mech for its metadata (this may prune the mech list). */
        query_meta_data(ctx, cred, target_name, req_flags);
    } else if (ctx->negoex_step == 2) {
        /* See if the mech processed the optimistic token. */
        check_optimistic_result(ctx, messages, nmessages);

        /* Pass the acceptor metadata to each mech to prune the list. */
        exchange_meta_data(ctx, cred, target_name, req_flags,
                           messages, nmessages);

        /* Process the ACCEPTOR_NEGO message. */
        major = process_acceptor_nego(minor, ctx, messages, nmessages);
        if (major != GSS_S_COMPLETE)
            goto cleanup;
    }

    /* Process the input token and/or produce an output token.  This may prune
     * the mech list, but on success there will be at least one mech entry. */
    major = mech_init(minor, ctx, cred, target_name, req_flags, time_req,
                      messages, nmessages, bindings, &mech_output_token,
                      time_rec);
    if (major != GSS_S_COMPLETE)
        goto cleanup;
    assert(!K5_TAILQ_EMPTY(&ctx->negoex_mechs));

    /* At this point in step 2 we have performed the metadata exchange and
     * chosen a mech we can use, so discard any fallback mech entries. */
    if (ctx->negoex_step == 2)
        negoex_select_auth_mech(ctx, K5_TAILQ_FIRST(&ctx->negoex_mechs));

    major = verify_checksum(minor, ctx, messages, nmessages, input_token,
                            &send_alert);
    if (major != GSS_S_COMPLETE)
        goto cleanup;

    if (input_token != GSS_C_NO_BUFFER) {
        k5_buf_add_len(&ctx->negoex_transcript, input_token->value,
                       input_token->length);
    }

    major = make_output_token(minor, ctx, &mech_output_token, send_alert,
                              output_token);
    if (major != GSS_S_COMPLETE)
        goto cleanup;

    mech = K5_TAILQ_FIRST(&ctx->negoex_mechs);
    major = (mech->complete && mech->verified_checksum) ? GSS_S_COMPLETE :
        GSS_S_CONTINUE_NEEDED;

cleanup:
    free(messages);
    gss_release_buffer(&tmpmin, &mech_output_token);
    negoex_prep_context_for_spnego(ctx);
    return major;
}

OM_uint32
negoex_accept(OM_uint32 *minor, spnego_gss_ctx_id_t ctx, gss_cred_id_t cred,
              gss_buffer_t input_token, gss_channel_bindings_t bindings,
              gss_buffer_t output_token, OM_uint32 *time_rec)
{
    OM_uint32 major, tmpmin;
    gss_buffer_desc mech_output_token = GSS_C_EMPTY_BUFFER;
    struct negoex_message *messages = NULL;
    struct negoex_auth_mech *mech;
    size_t nmessages;
    int send_alert = FALSE;

    if (input_token == GSS_C_NO_BUFFER || input_token->length == 0) {
        major = GSS_S_DEFECTIVE_TOKEN;
        goto cleanup;
    }

    major = negoex_prep_context_for_negoex(minor, ctx);
    if (major != GSS_S_COMPLETE)
        goto cleanup;

    ctx->negoex_step++;

    major = negoex_parse_token(minor, ctx, input_token, &messages, &nmessages);
    if (major != GSS_S_COMPLETE)
        goto cleanup;

    process_alerts(ctx, messages, nmessages);

    if (ctx->negoex_step == 1) {
        /* Read the INITIATOR_NEGO message to prune the candidate mech list. */
        major = process_initiator_nego(minor, ctx, messages, nmessages);
        if (major != GSS_S_COMPLETE)
            goto cleanup;

        /*
         * Pass the initiator metadata to each mech to prune the list, and
         * query each mech for its acceptor metadata (which may also prune the
         * list).
         */
        exchange_meta_data(ctx, cred, GSS_C_NO_NAME, 0, messages, nmessages);
        query_meta_data(ctx, cred, GSS_C_NO_NAME, 0);

        if (K5_TAILQ_EMPTY(&ctx->negoex_mechs)) {
            *minor = ERR_NEGOEX_NO_AVAILABLE_MECHS;
            major = GSS_S_FAILURE;
            goto cleanup;
        }
    }

    /*
     * Process the input token and possibly produce an output token.  This may
     * prune the list to a single mech.  Continue on error if an output token
     * is generated, so that we send the token to the initiator.
     */
    major = mech_accept(minor, ctx, cred, messages, nmessages, bindings,
                        &mech_output_token, time_rec);
    if (major != GSS_S_COMPLETE && mech_output_token.length == 0)
        goto cleanup;

    if (major == GSS_S_COMPLETE) {
        major = verify_checksum(minor, ctx, messages, nmessages, input_token,
                                &send_alert);
        if (major != GSS_S_COMPLETE)
            goto cleanup;
    }

    k5_buf_add_len(&ctx->negoex_transcript,
                   input_token->value, input_token->length);

    major = make_output_token(minor, ctx, &mech_output_token, send_alert,
                              output_token);
    if (major != GSS_S_COMPLETE)
        goto cleanup;

    mech = K5_TAILQ_FIRST(&ctx->negoex_mechs);
    major = (mech->complete && mech->verified_checksum) ? GSS_S_COMPLETE :
        GSS_S_CONTINUE_NEEDED;

cleanup:
    free(messages);
    gss_release_buffer(&tmpmin, &mech_output_token);
    negoex_prep_context_for_spnego(ctx);
    return major;
}
