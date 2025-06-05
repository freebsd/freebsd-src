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

#include "gssapiP_spnego.h"
#include <generic/gssapiP_generic.h>
#include "k5-input.h"

static void
release_auth_mech(struct negoex_auth_mech *mech);

OM_uint32
negoex_random(OM_uint32 *minor, spnego_gss_ctx_id_t ctx,
              uint8_t *data, size_t length)
{
    krb5_data d = make_data(data, length);

    *minor = krb5_c_random_make_octets(ctx->kctx, &d);
    return *minor ? GSS_S_FAILURE : GSS_S_COMPLETE;
}

/*
 * SPNEGO functions expect to find the active mech context in ctx->ctx_handle,
 * but the metadata exchange APIs force us to have one mech context per mech
 * entry.  To address this mismatch, move the active mech context (if we have
 * one) to ctx->ctx_handle at the end of NegoEx processing.
 */
void
negoex_prep_context_for_spnego(spnego_gss_ctx_id_t ctx)
{
    struct negoex_auth_mech *mech;

    mech = K5_TAILQ_FIRST(&ctx->negoex_mechs);
    if (mech == NULL || mech->mech_context == GSS_C_NO_CONTEXT)
        return;

    assert(ctx->ctx_handle == GSS_C_NO_CONTEXT);
    ctx->ctx_handle = mech->mech_context;
    mech->mech_context = GSS_C_NO_CONTEXT;
}

OM_uint32
negoex_prep_context_for_negoex(OM_uint32 *minor, spnego_gss_ctx_id_t ctx)
{
    krb5_error_code ret;
    struct negoex_auth_mech *mech;

    if (ctx->kctx != NULL) {
        /* The context is already initialized for NegoEx.  Undo what
         * negoex_prep_for_spnego() did, if applicable. */
        if (ctx->ctx_handle != GSS_C_NO_CONTEXT) {
            mech = K5_TAILQ_FIRST(&ctx->negoex_mechs);
            assert(mech != NULL && mech->mech_context == GSS_C_NO_CONTEXT);
            mech->mech_context = ctx->ctx_handle;
            ctx->ctx_handle = GSS_C_NO_CONTEXT;
        }
        return GSS_S_COMPLETE;
    }

    /* Initialize the NegoEX context fields.  (negoex_mechs is already set up
     * by SPNEGO.) */
    ret = krb5_init_context(&ctx->kctx);
    if (ret) {
        *minor = ret;
        return GSS_S_FAILURE;
    }

    k5_buf_init_dynamic(&ctx->negoex_transcript);

    return GSS_S_COMPLETE;
}

static void
release_all_mechs(spnego_gss_ctx_id_t ctx)
{
    struct negoex_auth_mech *mech, *next;

    K5_TAILQ_FOREACH_SAFE(mech, &ctx->negoex_mechs, links, next)
        release_auth_mech(mech);
    K5_TAILQ_INIT(&ctx->negoex_mechs);
}

void
negoex_release_context(spnego_gss_ctx_id_t ctx)
{
    k5_buf_free(&ctx->negoex_transcript);
    release_all_mechs(ctx);
    krb5_free_context(ctx->kctx);
    ctx->kctx = NULL;
}

static const char *
typestr(enum message_type type)
{
    if (type == INITIATOR_NEGO)
        return "INITIATOR_NEGO";
    else if (type == ACCEPTOR_NEGO)
        return "ACCEPTOR_NEGO";
    else if (type == INITIATOR_META_DATA)
        return "INITIATOR_META_DATA";
    else if (type == ACCEPTOR_META_DATA)
        return "ACCEPTOR_META_DATA";
    else if (type == CHALLENGE)
        return "CHALLENGE";
    else if (type == AP_REQUEST)
        return "AP_REQUEST";
    else if (type == VERIFY)
        return "VERIFY";
    else if (type == ALERT)
        return "ALERT";
    else
        return "UNKNOWN";
}

static void
add_guid(struct k5buf *buf, const uint8_t guid[GUID_LENGTH])
{
    uint32_t data1 = load_32_le(guid);
    uint16_t data2 = load_16_le(guid + 4), data3 = load_16_le(guid + 6);

    k5_buf_add_fmt(buf, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                   data1, data2, data3, guid[8], guid[9], guid[10], guid[11],
                   guid[12], guid[13], guid[14], guid[15]);
}

static char *
guid_to_string(const uint8_t guid[GUID_LENGTH])
{
    struct k5buf buf;

    k5_buf_init_dynamic(&buf);
    add_guid(&buf, guid);
    return k5_buf_cstring(&buf);
}

/* Check that the described vector lies within the message, and return a
 * pointer to its first element. */
static inline const uint8_t *
vector_base(size_t offset, size_t count, size_t width,
            const uint8_t *msg_base, size_t msg_len)
{
    if (offset > msg_len || count > (msg_len - offset) / width)
        return NULL;
    return msg_base + offset;
}

/* Trace a received message.  Call after the context sequence number is
 * incremented. */
static void
trace_received_message(spnego_gss_ctx_id_t ctx,
                       const struct negoex_message *msg)
{
    struct k5buf buf;
    uint16_t i;
    char *info = NULL;

    if (msg->type == INITIATOR_NEGO || msg->type == ACCEPTOR_NEGO) {
        k5_buf_init_dynamic(&buf);
        for (i = 0; i < msg->u.n.nschemes; i++) {
            add_guid(&buf, msg->u.n.schemes + i * GUID_LENGTH);
            if (i + 1 < msg->u.n.nschemes)
                k5_buf_add(&buf, " ");
        }
        info = k5_buf_cstring(&buf);
    } else if (msg->type == INITIATOR_META_DATA ||
               msg->type == ACCEPTOR_META_DATA ||
               msg->type == CHALLENGE || msg->type == AP_REQUEST) {
        info = guid_to_string(msg->u.e.scheme);
    } else if (msg->type == VERIFY) {
        info = guid_to_string(msg->u.v.scheme);
    } else if (msg->type == ALERT) {
        info = guid_to_string(msg->u.a.scheme);
    }

    if (info == NULL)
        return;

    TRACE_NEGOEX_INCOMING(ctx->kctx, ctx->negoex_seqnum - 1,
                          typestr(msg->type), info);
    free(info);
}

/* Trace an outgoing message with a GUID info string.  Call after the context
 * sequence number is incremented. */
static void
trace_outgoing_message(spnego_gss_ctx_id_t ctx, enum message_type type,
                       const uint8_t guid[GUID_LENGTH])
{
    char *info = guid_to_string(guid);

    if (info == NULL)
        return;
    TRACE_NEGOEX_OUTGOING(ctx->kctx, ctx->negoex_seqnum - 1, typestr(type),
                          info);
    free(info);
}

static OM_uint32
parse_nego_message(OM_uint32 *minor, struct k5input *in,
                   const uint8_t *msg_base, size_t msg_len,
                   struct nego_message *msg)
{
    const uint8_t *p;
    uint64_t protocol_version;
    uint32_t extension_type;
    size_t offset, count, i;

    p = k5_input_get_bytes(in, sizeof(msg->random));
    if (p != NULL)
        memcpy(msg->random, p, sizeof(msg->random));
    protocol_version = k5_input_get_uint64_le(in);
    if (protocol_version != 0) {
        *minor = ERR_NEGOEX_UNSUPPORTED_VERSION;
        return GSS_S_UNAVAILABLE;
    }

    offset = k5_input_get_uint32_le(in);
    count = k5_input_get_uint16_le(in);
    msg->schemes = vector_base(offset, count, GUID_LENGTH, msg_base, msg_len);
    msg->nschemes = count;
    if (msg->schemes == NULL) {
        *minor = ERR_NEGOEX_INVALID_MESSAGE_SIZE;
        return GSS_S_DEFECTIVE_TOKEN;
    }

    offset = k5_input_get_uint32_le(in);
    count = k5_input_get_uint16_le(in);
    p = vector_base(offset, count, EXTENSION_LENGTH, msg_base, msg_len);
    for (i = 0; i < count; i++) {
        extension_type = load_32_le(p + i * EXTENSION_LENGTH);
        if (extension_type & EXTENSION_FLAG_CRITICAL) {
            *minor = ERR_NEGOEX_UNSUPPORTED_CRITICAL_EXTENSION;
            return GSS_S_UNAVAILABLE;
        }
    }

    return GSS_S_COMPLETE;
}

static OM_uint32
parse_exchange_message(OM_uint32 *minor, struct k5input *in,
                       const uint8_t *msg_base, size_t msg_len,
                       struct exchange_message *msg)
{
    const uint8_t *p;
    size_t offset, len;

    p = k5_input_get_bytes(in, GUID_LENGTH);
    if (p != NULL)
        memcpy(msg->scheme, p, GUID_LENGTH);

    offset = k5_input_get_uint32_le(in);
    len = k5_input_get_uint32_le(in);
    p = vector_base(offset, len, 1, msg_base, msg_len);
    if (p == NULL) {
        *minor = ERR_NEGOEX_INVALID_MESSAGE_SIZE;
        return GSS_S_DEFECTIVE_TOKEN;
    }
    msg->token.value = (void *)p;
    msg->token.length = len;

    return GSS_S_COMPLETE;
}

static OM_uint32
parse_verify_message(OM_uint32 *minor, struct k5input *in,
                     const uint8_t *msg_base, size_t msg_len,
                     size_t token_offset, struct verify_message *msg)
{
    const uint8_t *p;
    size_t offset, len;
    uint32_t hdrlen, cksum_scheme;

    p = k5_input_get_bytes(in, GUID_LENGTH);
    if (p != NULL)
        memcpy(msg->scheme, p, GUID_LENGTH);

    hdrlen = k5_input_get_uint32_le(in);
    if (hdrlen != CHECKSUM_HEADER_LENGTH) {
        *minor = ERR_NEGOEX_INVALID_MESSAGE_SIZE;
        return GSS_S_DEFECTIVE_TOKEN;
    }
    cksum_scheme = k5_input_get_uint32_le(in);
    if (cksum_scheme != CHECKSUM_SCHEME_RFC3961) {
        *minor = ERR_NEGOEX_UNKNOWN_CHECKSUM_SCHEME;
        return GSS_S_UNAVAILABLE;
    }
    msg->cksum_type = k5_input_get_uint32_le(in);

    offset = k5_input_get_uint32_le(in);
    len = k5_input_get_uint32_le(in);
    msg->cksum = vector_base(offset, len, 1, msg_base, msg_len);
    msg->cksum_len = len;
    if (msg->cksum == NULL) {
        *minor = ERR_NEGOEX_INVALID_MESSAGE_SIZE;
        return GSS_S_DEFECTIVE_TOKEN;
    }

    msg->offset_in_token = token_offset;
    return GSS_S_COMPLETE;
}

static OM_uint32
parse_alert_message(OM_uint32 *minor, struct k5input *in,
                    const uint8_t *msg_base, size_t msg_len,
                    struct alert_message *msg)
{
    const uint8_t *p;
    uint32_t atype, reason;
    size_t alerts_offset, nalerts, value_offset, value_len, i;
    struct k5input alerts_in, pulse_in;

    p = k5_input_get_bytes(in, GUID_LENGTH);
    if (p != NULL)
        memcpy(msg->scheme, p, GUID_LENGTH);
    (void)k5_input_get_uint32_le(in);  /* skip over ErrorCode */
    alerts_offset = k5_input_get_uint32_le(in);
    nalerts = k5_input_get_uint32_le(in);
    p = vector_base(alerts_offset, nalerts, ALERT_LENGTH, msg_base, msg_len);
    if (p == NULL) {
        *minor = ERR_NEGOEX_INVALID_MESSAGE_SIZE;
        return GSS_S_DEFECTIVE_TOKEN;
    }

    /* Look for a VERIFY_NO_KEY pulse alert in the alerts vector. */
    msg->verify_no_key = FALSE;
    k5_input_init(&alerts_in, p, nalerts * ALERT_LENGTH);
    for (i = 0; i < nalerts; i++) {
        atype = k5_input_get_uint32_le(&alerts_in);
        value_offset = k5_input_get_uint32_le(&alerts_in);
        value_len = k5_input_get_uint32_le(&alerts_in);
        p = vector_base(value_offset, value_len, 1, msg_base, msg_len);
        if (p == NULL) {
            *minor = ERR_NEGOEX_INVALID_MESSAGE_SIZE;
            return GSS_S_DEFECTIVE_TOKEN;
        }

        if (atype == ALERT_TYPE_PULSE && value_len >= ALERT_PULSE_LENGTH) {
            k5_input_init(&pulse_in, p, value_len);
            (void)k5_input_get_uint32_le(&pulse_in);  /* skip header length */
            reason = k5_input_get_uint32_le(&pulse_in);
            if (reason == ALERT_VERIFY_NO_KEY)
                msg->verify_no_key = TRUE;
        }
    }

    return GSS_S_COMPLETE;
}

static OM_uint32
parse_message(OM_uint32 *minor, spnego_gss_ctx_id_t ctx, struct k5input *in,
              const uint8_t *token_base, struct negoex_message *msg)
{
    OM_uint32 major;
    const uint8_t *msg_base = in->ptr, *conv_id;
    size_t token_remaining = in->len, header_len, msg_len;
    uint64_t signature;
    uint32_t type, seqnum;

    signature = k5_input_get_uint64_le(in);
    type = k5_input_get_uint32_le(in);
    seqnum = k5_input_get_uint32_le(in);
    header_len = k5_input_get_uint32_le(in);
    msg_len = k5_input_get_uint32_le(in);
    conv_id = k5_input_get_bytes(in, GUID_LENGTH);

    if (in->status || msg_len > token_remaining || header_len > msg_len) {
        *minor = ERR_NEGOEX_INVALID_MESSAGE_SIZE;
        return GSS_S_DEFECTIVE_TOKEN;
    }
    if (signature != MESSAGE_SIGNATURE) {
        *minor = ERR_NEGOEX_INVALID_MESSAGE_SIGNATURE;
        return GSS_S_DEFECTIVE_TOKEN;
    }
    if (seqnum != ctx->negoex_seqnum) {
        *minor = ERR_NEGOEX_MESSAGE_OUT_OF_SEQUENCE;
        return GSS_S_DEFECTIVE_TOKEN;
    }
    if (seqnum == 0) {
        memcpy(ctx->negoex_conv_id, conv_id, GUID_LENGTH);
    } else if (!GUID_EQ(conv_id, ctx->negoex_conv_id)) {
        *minor = ERR_NEGOEX_INVALID_CONVERSATION_ID;
        return GSS_S_DEFECTIVE_TOKEN;
    }

    /* Restrict the input region to the header. */
    in->len = header_len - (in->ptr - msg_base);

    msg->type = type;
    if (type == INITIATOR_NEGO || type == ACCEPTOR_NEGO) {
        major = parse_nego_message(minor, in, msg_base, msg_len, &msg->u.n);
    } else if (type == INITIATOR_META_DATA || type == ACCEPTOR_META_DATA ||
               type == CHALLENGE || type == AP_REQUEST) {
        major = parse_exchange_message(minor, in, msg_base, msg_len,
                                       &msg->u.e);
    } else if (type == VERIFY) {
        major = parse_verify_message(minor, in, msg_base, msg_len,
                                     msg_base - token_base, &msg->u.v);
    } else if (type == ALERT) {
        major = parse_alert_message(minor, in, msg_base, msg_len, &msg->u.a);
    } else {
        *minor = ERR_NEGOEX_INVALID_MESSAGE_TYPE;
        return GSS_S_DEFECTIVE_TOKEN;
    }
    if (major != GSS_S_COMPLETE)
        return major;

    /* Reset the input buffer to the remainder of the token. */
    if (!in->status)
        k5_input_init(in, msg_base + msg_len, token_remaining - msg_len);

    ctx->negoex_seqnum++;
    trace_received_message(ctx, msg);
    return GSS_S_COMPLETE;
}

/*
 * Parse token into an array of negoex_message structures.  All pointer fields
 * within the parsed messages are aliases into token, so the result can be
 * freed with free().  An unknown protocol version, a critical extension, or an
 * unknown checksum scheme will cause a parsing failure.  Increment the
 * sequence number in ctx for each message, and record and check the
 * conversation ID in ctx as appropriate.
 */
OM_uint32
negoex_parse_token(OM_uint32 *minor, spnego_gss_ctx_id_t ctx,
                   gss_const_buffer_t token,
                   struct negoex_message **messages_out, size_t *count_out)
{
    OM_uint32 major = GSS_S_COMPLETE;
    size_t count = 0;
    struct k5input in;
    struct negoex_message *messages = NULL, *newptr;

    *messages_out = NULL;
    *count_out = 0;
    assert(token != GSS_C_NO_BUFFER);
    k5_input_init(&in, token->value, token->length);

    while (in.status == 0 && in.len > 0) {
        newptr = realloc(messages, (count + 1) * sizeof(*newptr));
        if (newptr == NULL) {
            free(messages);
            *minor = ENOMEM;
            return GSS_S_FAILURE;
        }
        messages = newptr;

        major = parse_message(minor, ctx, &in, token->value, &messages[count]);
        if (major != GSS_S_COMPLETE)
            break;

        count++;
    }

    if (in.status) {
        *minor = ERR_NEGOEX_INVALID_MESSAGE_SIZE;
        major = GSS_S_DEFECTIVE_TOKEN;
    }
    if (major != GSS_S_COMPLETE) {
        free(messages);
        return major;
    }

    *messages_out = messages;
    *count_out = count;
    return GSS_S_COMPLETE;
}

static struct negoex_message *
locate_message(struct negoex_message *messages, size_t nmessages,
               enum message_type type)
{
    uint32_t i;

    for (i = 0; i < nmessages; i++) {
        if (messages[i].type == type)
            return &messages[i];
    }

    return NULL;
}

struct nego_message *
negoex_locate_nego_message(struct negoex_message *messages, size_t nmessages,
                           enum message_type type)
{
    struct negoex_message *msg = locate_message(messages, nmessages, type);

    return (msg == NULL) ? NULL : &msg->u.n;
}

struct exchange_message *
negoex_locate_exchange_message(struct negoex_message *messages,
                               size_t nmessages, enum message_type type)
{
    struct negoex_message *msg = locate_message(messages, nmessages, type);

    return (msg == NULL) ? NULL : &msg->u.e;
}

struct verify_message *
negoex_locate_verify_message(struct negoex_message *messages,
                             size_t nmessages)
{
    struct negoex_message *msg = locate_message(messages, nmessages, VERIFY);

    return (msg == NULL) ? NULL : &msg->u.v;
}

struct alert_message *
negoex_locate_alert_message(struct negoex_message *messages, size_t nmessages)
{
    struct negoex_message *msg = locate_message(messages, nmessages, ALERT);

    return (msg == NULL) ? NULL : &msg->u.a;
}

/*
 * Add the encoding of a MESSAGE_HEADER structure to buf, given the number of
 * bytes of the payload following the full header.  Increment the sequence
 * number in ctx.  Set *payload_start_out to the position of the payload within
 * the message.
 */
static void
put_message_header(spnego_gss_ctx_id_t ctx, enum message_type type,
                   uint32_t payload_len, uint32_t *payload_start_out)
{
    size_t header_len;

    if (type == INITIATOR_NEGO || type == ACCEPTOR_NEGO)
        header_len = NEGO_MESSAGE_HEADER_LENGTH;
    else if (type == INITIATOR_META_DATA || type == ACCEPTOR_META_DATA ||
             type == CHALLENGE || type == AP_REQUEST)
        header_len = EXCHANGE_MESSAGE_HEADER_LENGTH;
    else if (type == VERIFY)
        header_len = VERIFY_MESSAGE_HEADER_LENGTH;
    else if (type == ALERT)
        header_len = ALERT_MESSAGE_HEADER_LENGTH;
    else
        abort();

    k5_buf_add_uint64_le(&ctx->negoex_transcript, MESSAGE_SIGNATURE);
    k5_buf_add_uint32_le(&ctx->negoex_transcript, type);
    k5_buf_add_uint32_le(&ctx->negoex_transcript, ctx->negoex_seqnum++);
    k5_buf_add_uint32_le(&ctx->negoex_transcript, header_len);
    k5_buf_add_uint32_le(&ctx->negoex_transcript, header_len + payload_len);
    k5_buf_add_len(&ctx->negoex_transcript, ctx->negoex_conv_id, GUID_LENGTH);

    *payload_start_out = header_len;
}

void
negoex_add_nego_message(spnego_gss_ctx_id_t ctx, enum message_type type,
                        uint8_t random[32])
{
    struct negoex_auth_mech *mech;
    uint32_t payload_start, seqnum = ctx->negoex_seqnum;
    uint16_t nschemes;
    struct k5buf buf;

    nschemes = 0;
    K5_TAILQ_FOREACH(mech, &ctx->negoex_mechs, links)
        nschemes++;

    put_message_header(ctx, type, nschemes * GUID_LENGTH, &payload_start);
    k5_buf_add_len(&ctx->negoex_transcript, random, 32);
    /* ProtocolVersion */
    k5_buf_add_uint64_le(&ctx->negoex_transcript, 0);
    /* AuthSchemes vector */
    k5_buf_add_uint32_le(&ctx->negoex_transcript, payload_start);
    k5_buf_add_uint16_le(&ctx->negoex_transcript, nschemes);
    /* Extensions vector */
    k5_buf_add_uint32_le(&ctx->negoex_transcript, payload_start);
    k5_buf_add_uint16_le(&ctx->negoex_transcript, 0);
    /* Four bytes of padding to reach a multiple of 8 bytes. */
    k5_buf_add_len(&ctx->negoex_transcript, "\0\0\0\0", 4);

    /* Payload (auth schemes); also build guid string for tracing. */
    k5_buf_init_dynamic(&buf);
    K5_TAILQ_FOREACH(mech, &ctx->negoex_mechs, links) {
        k5_buf_add_len(&ctx->negoex_transcript, mech->scheme, GUID_LENGTH);
        add_guid(&buf, mech->scheme);
        k5_buf_add(&buf, " ");
    }

    if (buf.len > 0) {
        k5_buf_truncate(&buf, buf.len - 1);
        TRACE_NEGOEX_OUTGOING(ctx->kctx, seqnum, typestr(type),
                              k5_buf_cstring(&buf));
        k5_buf_free(&buf);
    }
}

void
negoex_add_exchange_message(spnego_gss_ctx_id_t ctx, enum message_type type,
                            const auth_scheme scheme, gss_buffer_t token)
{
    uint32_t payload_start;

    put_message_header(ctx, type, token->length, &payload_start);
    k5_buf_add_len(&ctx->negoex_transcript, scheme, GUID_LENGTH);
    /* Exchange byte vector */
    k5_buf_add_uint32_le(&ctx->negoex_transcript, payload_start);
    k5_buf_add_uint32_le(&ctx->negoex_transcript, token->length);
    /* Payload (token) */
    k5_buf_add_len(&ctx->negoex_transcript, token->value, token->length);

    trace_outgoing_message(ctx, type, scheme);
}

void
negoex_add_verify_message(spnego_gss_ctx_id_t ctx, const auth_scheme scheme,
                          uint32_t cksum_type, const uint8_t *cksum,
                          uint32_t cksum_len)
{
    uint32_t payload_start;

    put_message_header(ctx, VERIFY, cksum_len, &payload_start);
    k5_buf_add_len(&ctx->negoex_transcript, scheme, GUID_LENGTH);
    k5_buf_add_uint32_le(&ctx->negoex_transcript, CHECKSUM_HEADER_LENGTH);
    k5_buf_add_uint32_le(&ctx->negoex_transcript, CHECKSUM_SCHEME_RFC3961);
    k5_buf_add_uint32_le(&ctx->negoex_transcript, cksum_type);
    /* ChecksumValue vector */
    k5_buf_add_uint32_le(&ctx->negoex_transcript, payload_start);
    k5_buf_add_uint32_le(&ctx->negoex_transcript, cksum_len);
    /* Four bytes of padding to reach a multiple of 8 bytes. */
    k5_buf_add_len(&ctx->negoex_transcript, "\0\0\0\0", 4);
    /* Payload (checksum contents) */
    k5_buf_add_len(&ctx->negoex_transcript, cksum, cksum_len);

    trace_outgoing_message(ctx, VERIFY, scheme);
}

/* Add an ALERT_MESSAGE containing a single ALERT_TYPE_PULSE alert with the
 * reason ALERT_VERIFY_NO_KEY. */
void
negoex_add_verify_no_key_alert(spnego_gss_ctx_id_t ctx,
                               const auth_scheme scheme)
{
    uint32_t payload_start;

    put_message_header(ctx, ALERT, ALERT_LENGTH + ALERT_PULSE_LENGTH,
                       &payload_start);
    k5_buf_add_len(&ctx->negoex_transcript, scheme, GUID_LENGTH);
    /* ErrorCode */
    k5_buf_add_uint32_le(&ctx->negoex_transcript, 0);
    /* Alerts vector */
    k5_buf_add_uint32_le(&ctx->negoex_transcript, payload_start);
    k5_buf_add_uint16_le(&ctx->negoex_transcript, 1);
    /* Six bytes of padding to reach a multiple of 8 bytes. */
    k5_buf_add_len(&ctx->negoex_transcript, "\0\0\0\0\0\0", 6);
    /* Payload part 1: a single ALERT element */
    k5_buf_add_uint32_le(&ctx->negoex_transcript, ALERT_TYPE_PULSE);
    k5_buf_add_uint32_le(&ctx->negoex_transcript,
                         payload_start + ALERT_LENGTH);
    k5_buf_add_uint32_le(&ctx->negoex_transcript, ALERT_PULSE_LENGTH);
    /* Payload part 2: ALERT_PULSE */
    k5_buf_add_uint32_le(&ctx->negoex_transcript, ALERT_PULSE_LENGTH);
    k5_buf_add_uint32_le(&ctx->negoex_transcript, ALERT_VERIFY_NO_KEY);

    trace_outgoing_message(ctx, ALERT, scheme);
}

static void
release_auth_mech(struct negoex_auth_mech *mech)
{
    OM_uint32 tmpmin;

    if (mech == NULL)
        return;

    gss_delete_sec_context(&tmpmin, &mech->mech_context, NULL);
    generic_gss_release_oid(&tmpmin, &mech->oid);
    gss_release_buffer(&tmpmin, &mech->metadata);
    krb5_free_keyblock_contents(NULL, &mech->key);
    krb5_free_keyblock_contents(NULL, &mech->verify_key);

    free(mech);
}

void
negoex_delete_auth_mech(spnego_gss_ctx_id_t ctx,
                        struct negoex_auth_mech *mech)
{
    K5_TAILQ_REMOVE(&ctx->negoex_mechs, mech, links);
    release_auth_mech(mech);
}

/* Remove all auth mech entries except for mech from ctx->mechs. */
void
negoex_select_auth_mech(spnego_gss_ctx_id_t ctx,
                        struct negoex_auth_mech *mech)
{
    assert(mech != NULL);
    K5_TAILQ_REMOVE(&ctx->negoex_mechs, mech, links);
    release_all_mechs(ctx);
    K5_TAILQ_INSERT_HEAD(&ctx->negoex_mechs, mech, links);
}

OM_uint32
negoex_add_auth_mech(OM_uint32 *minor, spnego_gss_ctx_id_t ctx,
                     gss_const_OID oid, auth_scheme scheme)
{
    OM_uint32 major;
    struct negoex_auth_mech *mech;

    mech = calloc(1, sizeof(*mech));
    if (mech == NULL) {
        *minor = ENOMEM;
        return GSS_S_FAILURE;
    }

    major = generic_gss_copy_oid(minor, (gss_OID)oid, &mech->oid);
    if (major != GSS_S_COMPLETE) {
        free(mech);
        return major;
    }

    memcpy(mech->scheme, scheme, GUID_LENGTH);

    K5_TAILQ_INSERT_TAIL(&ctx->negoex_mechs, mech, links);

    *minor = 0;
    return GSS_S_COMPLETE;
}

struct negoex_auth_mech *
negoex_locate_auth_scheme(spnego_gss_ctx_id_t ctx, const auth_scheme scheme)
{
    struct negoex_auth_mech *mech;

    K5_TAILQ_FOREACH(mech, &ctx->negoex_mechs, links) {
        if (GUID_EQ(mech->scheme, scheme))
            return mech;
    }

    return NULL;
}

/* Prune ctx->mechs to the schemes present in schemes, and reorder them to
 * match its order. */
void
negoex_common_auth_schemes(spnego_gss_ctx_id_t ctx,
                           const uint8_t *schemes, uint16_t nschemes)
{
    struct negoex_mech_list list;
    struct negoex_auth_mech *mech;
    uint16_t i;

    /* Construct a new list in the order of schemes. */
    K5_TAILQ_INIT(&list);
    for (i = 0; i < nschemes; i++) {
        mech = negoex_locate_auth_scheme(ctx, schemes + i * GUID_LENGTH);
        if (mech == NULL)
            continue;
        K5_TAILQ_REMOVE(&ctx->negoex_mechs, mech, links);
        K5_TAILQ_INSERT_TAIL(&list, mech, links);
    }

    /* Release any leftover entries and replace the context list. */
    release_all_mechs(ctx);
    K5_TAILQ_CONCAT(&ctx->negoex_mechs, &list, links);
}

/* Prune ctx->mechs to the schemes present in schemes, but do not change
 * their order. */
void
negoex_restrict_auth_schemes(spnego_gss_ctx_id_t ctx,
                             const uint8_t *schemes, uint16_t nschemes)
{
    struct negoex_auth_mech *mech, *next;
    uint16_t i;
    int found;

    K5_TAILQ_FOREACH_SAFE(mech, &ctx->negoex_mechs, links, next) {
        found = FALSE;
        for (i = 0; i < nschemes && !found; i++) {
            if (GUID_EQ(mech->scheme, schemes + i * GUID_LENGTH))
                found = TRUE;
        }

        if (!found)
            negoex_delete_auth_mech(ctx, mech);
    }
}
