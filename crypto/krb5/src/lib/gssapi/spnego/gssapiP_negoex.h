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

#include "k5-int.h"

/*
 * { iso(1) identified-organization(3) dod(6) internet(1) private(4)
 *   enterprise(1) microsoft (311) security(2) mechanisms(2) negoex(30) }
 */
#define NEGOEX_OID_LENGTH 10
#define NEGOEX_OID "\x2b\x06\x01\x04\x01\x82\x37\x02\x02\x1e"

#define MESSAGE_SIGNATURE   0x535458454F47454EULL

#define EXTENSION_LENGTH                    12

#define EXTENSION_FLAG_CRITICAL             0x80000000

#define CHECKSUM_SCHEME_RFC3961             1

#define NEGOEX_KEYUSAGE_INITIATOR_CHECKSUM  23
#define NEGOEX_KEYUSAGE_ACCEPTOR_CHECKSUM   25

#define CHECKSUM_HEADER_LENGTH              20

#define GUID_LENGTH                         16

typedef uint8_t auth_scheme[GUID_LENGTH];
typedef uint8_t conversation_id[GUID_LENGTH];
#define GUID_EQ(a, b) (memcmp(a, b, GUID_LENGTH) == 0)

#define NEGO_MESSAGE_HEADER_LENGTH          96
#define EXCHANGE_MESSAGE_HEADER_LENGTH      64
#define VERIFY_MESSAGE_HEADER_LENGTH        80
#define ALERT_MESSAGE_HEADER_LENGTH         72
#define ALERT_LENGTH                        12
#define ALERT_PULSE_LENGTH                  8

#define ALERT_TYPE_PULSE                    1
#define ALERT_VERIFY_NO_KEY                 1

enum message_type {
    INITIATOR_NEGO = 0,         /* NEGO_MESSAGE */
    ACCEPTOR_NEGO,              /* NEGO_MESSAGE */
    INITIATOR_META_DATA,        /* EXCHANGE_MESSAGE */
    ACCEPTOR_META_DATA,         /* EXCHANGE_MESSAGE */
    CHALLENGE,                  /* EXCHANGE_MESSAGE */
    AP_REQUEST,                 /* EXCHANGE_MESSAGE */
    VERIFY,                     /* VERIFY_MESSAGE */
    ALERT,                      /* ALERT */
};

struct nego_message {
    uint8_t random[32];
    const uint8_t *schemes;
    uint16_t nschemes;
};

struct exchange_message {
    auth_scheme scheme;
    gss_buffer_desc token;
};

struct verify_message {
    auth_scheme scheme;
    uint32_t cksum_type;
    const uint8_t *cksum;
    size_t cksum_len;
    size_t offset_in_token;
};

struct alert_message {
    auth_scheme scheme;
    int verify_no_key;
};

struct negoex_message {
    uint32_t type;
    union {
        struct nego_message n;
        struct exchange_message e;
        struct verify_message v;
        struct alert_message a;
    } u;
};

struct negoex_auth_mech {
    K5_TAILQ_ENTRY(negoex_auth_mech) links;
    gss_OID oid;
    auth_scheme scheme;
    gss_ctx_id_t mech_context;
    gss_buffer_desc metadata;
    krb5_keyblock key;
    krb5_keyblock verify_key;
    int complete;
    int sent_checksum;
    int verified_checksum;
};

/* negoex_util.c */

OM_uint32
negoex_parse_token(OM_uint32 *minor, spnego_gss_ctx_id_t ctx,
                   gss_const_buffer_t token,
                   struct negoex_message **messages_out, size_t *count_out);


struct nego_message *
negoex_locate_nego_message(struct negoex_message *messages, size_t nmessages,
                           enum message_type type);
struct exchange_message *
negoex_locate_exchange_message(struct negoex_message *messages,
                               size_t nmessages, enum message_type type);
struct verify_message *
negoex_locate_verify_message(struct negoex_message *messages,
                             size_t nmessages);
struct alert_message *
negoex_locate_alert_message(struct negoex_message *messages, size_t nmessages);

void
negoex_add_nego_message(spnego_gss_ctx_id_t ctx, enum message_type type,
                        uint8_t random[32]);
void
negoex_add_exchange_message(spnego_gss_ctx_id_t ctx, enum message_type type,
                            const auth_scheme scheme, gss_buffer_t token);
void
negoex_add_verify_message(spnego_gss_ctx_id_t ctx, const auth_scheme scheme,
                          uint32_t cksum_type, const uint8_t *cksum,
                          uint32_t cksum_len);

void
negoex_add_verify_no_key_alert(spnego_gss_ctx_id_t ctx,
                               const auth_scheme scheme);

OM_uint32
negoex_random(OM_uint32 *minor, spnego_gss_ctx_id_t ctx,
              unsigned char *data, size_t length);

void
negoex_prep_context_for_spnego(spnego_gss_ctx_id_t ctx);

OM_uint32
negoex_prep_context_for_negoex(OM_uint32 *minor, spnego_gss_ctx_id_t ctx);

void
negoex_release_context(spnego_gss_ctx_id_t ctx);

OM_uint32
negoex_add_auth_mech(OM_uint32 *minor, spnego_gss_ctx_id_t ctx,
                     gss_const_OID oid, auth_scheme scheme);

void
negoex_delete_auth_mech(spnego_gss_ctx_id_t ctx,
                        struct negoex_auth_mech *mech);

void
negoex_select_auth_mech(spnego_gss_ctx_id_t ctx,
                        struct negoex_auth_mech *mech);

struct negoex_auth_mech *
negoex_locate_auth_scheme(spnego_gss_ctx_id_t ctx, const auth_scheme scheme);

void
negoex_common_auth_schemes(spnego_gss_ctx_id_t ctx,
                           const uint8_t *schemes, uint16_t nschemes);

void
negoex_restrict_auth_schemes(spnego_gss_ctx_id_t ctx,
                             const uint8_t *schemes, uint16_t nschemes);

/* negoex_ctx.c */

OM_uint32
negoex_init(OM_uint32 *minor, spnego_gss_ctx_id_t ctx, gss_cred_id_t cred,
            gss_name_t target_name, OM_uint32 req_flags, OM_uint32 time_req,
            gss_buffer_t input_token, gss_channel_bindings_t bindings,
            gss_buffer_t output_token, OM_uint32 *time_rec);

OM_uint32
negoex_accept(OM_uint32 *minor, spnego_gss_ctx_id_t ctx, gss_cred_id_t cred,
              gss_buffer_t input_token, gss_channel_bindings_t bindings,
              gss_buffer_t output_token, OM_uint32 *time_rec);
