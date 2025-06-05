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

static int
guid_to_string(const uint8_t guid[16], char *buffer, size_t bufsiz)
{
    uint32_t data1;
    uint16_t data2, data3;

    data1 = load_32_le(guid);
    data2 = load_16_le(guid + 4);
    data3 = load_16_le(guid + 6);

    return snprintf(buffer, bufsiz,
                    "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                    data1, data2, data3, guid[8], guid[9], guid[10], guid[11],
                    guid[12], guid[13], guid[14], guid[15]);
}

static void
trace_auth_scheme(spnego_gss_ctx_id_t ctx, const char *prefix, int ind,
                  const auth_scheme scheme)
{
    char trace_msg[128];
    char szAuthScheme[37];

    guid_to_string(scheme, szAuthScheme, sizeof(szAuthScheme));

    snprintf(trace_msg, sizeof(trace_msg),
             "NEGOEXTS: %20s[%02u] -- AuthScheme %s",
             prefix, ind, szAuthScheme);
    TRACE_NEGOEX_AUTH_SCHEMES(ctx->kctx, trace_msg);
}

void
negoex_trace_auth_schemes(spnego_gss_ctx_id_t ctx, const char *prefix,
                          const uint8_t *schemes, uint16_t nschemes)
{
    uint16_t i;

    for (i = 0; i < nschemes; i++)
        trace_auth_scheme(ctx, prefix, i, schemes + i * GUID_LENGTH);
}

void
negoex_trace_ctx_auth_schemes(spnego_gss_ctx_id_t ctx, const char *prefix)
{
    negoex_auth_mech_t mech;
    int ind = 0;

    K5_TAILQ_FOREACH(mech, &ctx->negoex_mechs, links)
        trace_auth_scheme(ctx, prefix, ind++, mech->scheme);
}

void
negoex_trace_message(spnego_gss_ctx_id_t ctx, int direction,
                     enum message_type type, const conversation_id conv_id,
                     unsigned int seqnum, unsigned int header_len,
                     unsigned int msg_len)
{
    char trace_msg[128];
    char conv_str[37];
    char *typestr;

    if (type == INITIATOR_NEGO)
        typestr = "INITIATOR_NEGO";
    else if (type == ACCEPTOR_NEGO)
        typestr = "ACCEPTOR_NEGO";
    else if (type == INITIATOR_META_DATA)
        typestr = "INITIATOR_META_DATA";
    else if (type == ACCEPTOR_META_DATA)
        typestr = "ACCEPTOR_META_DATA";
    else if (type == CHALLENGE)
        typestr = "CHALLENGE";
    else if (type == AP_REQUEST)
        typestr = "AP_REQUEST";
    else if (type == VERIFY)
        typestr = "VERIFY";
    else if (type == ALERT)
        typestr = "ALERT";
    else
        typestr = "UNKNOWN";

    guid_to_string(conv_id, conv_str, sizeof(conv_str));
    snprintf(trace_msg, sizeof(trace_msg),
             "NEGOEXTS%c %20s[%02u] -- ConvId %s HdrLength %u MsgLength %u",
             direction ? '<' : '>', typestr, seqnum, conv_str, header_len,
             msg_len);

    TRACE_NEGOEX_MESSAGE(ctx->kctx, trace_msg);
}
