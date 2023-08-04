/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/preauth/spake/groups.h - SPAKE group interfaces */
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

#ifndef GROUPS_H
#define GROUPS_H

#include "k5-int.h"
#include "iana.h"

typedef struct groupstate_st groupstate;
typedef struct groupdata_st groupdata;
typedef struct groupdef_st groupdef;

struct groupdef_st {
    const spake_iana *reg;

    /*
     * Optional: create a per-group data object to allow more efficient keygen
     * and result computations.  Saving a reference to gdef is okay; its
     * lifetime will always be longer than the resulting object.
     */
    krb5_error_code (*init)(krb5_context context, const groupdef *gdef,
                            groupdata **gdata_out);

    /* Optional: release a group data object. */
    void (*fini)(groupdata *gdata);

    /*
     * Mandatory: generate a random private scalar (x or y) and a public
     * element (T or S), using wbytes for the w value.  If use_m is true, use
     * the M element (generating T); otherwise use the N element (generating
     * S).  wbytes and priv_out have length reg->mult_len; pub_out has length
     * reg->elem_len.  priv_out and pub_out are caller-allocated.
     */
    krb5_error_code (*keygen)(krb5_context context, groupdata *gdata,
                              const uint8_t *wbytes, krb5_boolean use_m,
                              uint8_t *priv_out, uint8_t *pub_out);

    /*
     * Mandatory: compute K given a private scalar (x or y) and the other
     * party's public element (S or T), using wbytes for the w value.  If use_m
     * is true, use the M element (computing K from y and T); otherwise use the
     * N element (computing K from x and S).  wbytes and ourpriv have length
     * reg->mult_len; theirpub and elem_out have length reg->elem_len.
     * elem_out is caller-allocated.
     */
    krb5_error_code (*result)(krb5_context context, groupdata *gdata,
                              const uint8_t *wbytes, const uint8_t *ourpriv,
                              const uint8_t *theirpub, krb5_boolean use_m,
                              uint8_t *elem_out);

    /*
     * Mandatory: compute the group's specified hash function over datas (with
     * ndata elements), placing the result in result_out.  result_out is
     * caller-allocated with length reg->hash_len.
     */
    krb5_error_code (*hash)(krb5_context context, groupdata *gdata,
                            const krb5_data *datas, size_t ndata,
                            uint8_t *result_out);
};

/* Initialize an object which holds group configuration and pre-computation
 * state for each group.  is_kdc is true for KDCs, false for clients. */
krb5_error_code group_init_state(krb5_context context, krb5_boolean is_kdc,
                                 groupstate **out);

/* Release resources held by gstate. */
void group_free_state(groupstate *gstate);

/* Return true if group is permitted by configuration. */
krb5_boolean group_is_permitted(groupstate *gstate, int32_t group);

/* Set *list_out and *count_out to the list of groups permitted by
 * configuration. */
void group_get_permitted(groupstate *gstate, int32_t **list_out,
                         int32_t *count_out);

/* Return the KDC optimistic challenge group if one is configured.  Valid for
 * KDC groupstate objects only. */
krb5_int32 group_optimistic_challenge(groupstate *gstate);

/* Set *len_out to the multiplier length for group. */
krb5_error_code group_mult_len(int32_t group, size_t *len_out);

/*
 * Generate a SPAKE private scalar (x or y) and public element (T or S), given
 * an input multiplier wbytes.  Use constant M if gstate is a KDC groupstate
 * object, N if it is a client object.  Allocate storage and place the results
 * in *priv_out and *pub_out.
 */
krb5_error_code group_keygen(krb5_context context, groupstate *gstate,
                             int32_t group, const krb5_data *wbytes,
                             krb5_data *priv_out, krb5_data *pub_out);

/*
 * Compute the SPAKE result K from our private scalar (x or y) and their public
 * key (S or T), deriving the input scalar w from ikey.  Use the other party's
 * constant, N if gstate is a KDC groupstate object or M if it is a client
 * object.  Allocate storage and place the result in *spakeresult_out.
 */
krb5_error_code group_result(krb5_context context, groupstate *gstate,
                             int32_t group, const krb5_data *wbytes,
                             const krb5_data *ourpriv,
                             const krb5_data *theirpub,
                             krb5_data *spakeresult_out);

/* Set *result_out to the hash output length for group. */
krb5_error_code group_hash_len(int32_t group, size_t *result_out);

/*
 * Compute the group's specified hash function over dlist (with ndata
 * elements).  result_out is caller-allocated with enough bytes for the hash
 * output as given by group_hash_len().
 */
krb5_error_code group_hash(krb5_context context, groupstate *gstate,
                           int32_t group, const krb5_data *dlist, size_t ndata,
                           uint8_t *result_out);

#endif /* GROUPS_H */
