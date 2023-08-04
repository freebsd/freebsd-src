/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* include/k5-spake.h - SPAKE preauth mech declarations */
/*
 * Copyright (C) 2015 by the Massachusetts Institute of Technology.
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

/*
 * The SPAKE preauth mechanism allows long-term client keys to be used for
 * preauthentication without exposing them to offline dictionary attacks.  The
 * negotiated key can also be used for second-factor authentication.  This
 * header file declares structures and encoder/decoder functions for the
 * mechanism's padata messages.
 */

#ifndef K5_SPAKE_H
#define K5_SPAKE_H

#include "k5-int.h"

/* SPAKESecondFactor is contained within a SPAKEChallenge, SPAKEResponse, or
 * EncryptedData message and contains a second-factor challenge or response. */
typedef struct krb5_spake_factor_st {
    int32_t type;
    krb5_data *data;
} krb5_spake_factor;

/* SPAKESupport is sent from the client to the KDC to indicate which group the
 * client supports. */
typedef struct krb5_spake_support_st {
    int32_t ngroups;
    int32_t *groups;
} krb5_spake_support;

/* SPAKEChallenge is sent from the KDC to the client to communicate its group
 * selection, public value, and second-factor challenge options. */
typedef struct krb5_spake_challenge_st {
    int32_t group;
    krb5_data pubkey;
    krb5_spake_factor **factors;
} krb5_spake_challenge;

/* SPAKEResponse is sent from the client to the KDC to communicate its public
 * value and encrypted second-factor response. */
typedef struct krb5_spake_response_st {
    krb5_data pubkey;
    krb5_enc_data factor;
} krb5_spake_response;

enum krb5_spake_msgtype {
    SPAKE_MSGTYPE_UNKNOWN = -1,
    SPAKE_MSGTYPE_SUPPORT = 0,
    SPAKE_MSGTYPE_CHALLENGE = 1,
    SPAKE_MSGTYPE_RESPONSE = 2,
    SPAKE_MSGTYPE_ENCDATA = 3
};

/* PA-SPAKE is a choice among the message types which can appear in a PA-SPAKE
 * padata element. */
typedef struct krb5_pa_spake_st {
    enum krb5_spake_msgtype choice;
    union krb5_spake_message_choices {
        krb5_spake_support support;
        krb5_spake_challenge challenge;
        krb5_spake_response response;
        krb5_enc_data encdata;
    } u;
} krb5_pa_spake;

krb5_error_code encode_krb5_spake_factor(const krb5_spake_factor *val,
                                         krb5_data **code_out);
krb5_error_code decode_krb5_spake_factor(const krb5_data *code,
                                         krb5_spake_factor **val_out);
void k5_free_spake_factor(krb5_context context, krb5_spake_factor *val);

krb5_error_code encode_krb5_pa_spake(const krb5_pa_spake *val,
                                     krb5_data **code_out);
krb5_error_code decode_krb5_pa_spake(const krb5_data *code,
                                     krb5_pa_spake **val_out);
void k5_free_pa_spake(krb5_context context, krb5_pa_spake *val);

#endif /* K5_SPAKE_H */
