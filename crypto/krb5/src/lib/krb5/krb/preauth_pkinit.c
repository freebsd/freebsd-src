/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/preauth_pkinit.c - PKINIT clpreauth helpers */
/*
 * Copyright 2013 Red Hat, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * This file defines libkrb5 APIs for manipulating PKINIT responder questions
 * and answers.  The main body of the PKINIT clpreauth module is in the
 * plugins/preauth/pkinit directory.
 */

#include "k5-int.h"
#include "k5-json.h"
#include "int-proto.h"
#include "init_creds_ctx.h"

struct get_one_challenge_data {
    krb5_responder_pkinit_identity **identities;
    krb5_error_code err;
};

static void
get_one_challenge(void *arg, const char *key, k5_json_value val)
{
    struct get_one_challenge_data *data;
    unsigned long token_flags;
    int i;

    data = arg;
    if (data->err != 0)
        return;
    if (k5_json_get_tid(val) != K5_JSON_TID_NUMBER) {
        data->err = EINVAL;
        return;
    }

    token_flags = k5_json_number_value(val);
    /* Find the slot for this entry. */
    for (i = 0; data->identities[i] != NULL; i++)
        continue;
    /* Set the identity (a copy of the key) and the token flags. */
    data->identities[i] = k5alloc(sizeof(*data->identities[i]), &data->err);
    if (data->identities[i] == NULL)
        return;
    data->identities[i]->identity = strdup(key);
    if (data->identities[i]->identity == NULL) {
        data->err = ENOMEM;
        return;
    }
    data->identities[i]->token_flags = token_flags;
}

krb5_error_code KRB5_CALLCONV
krb5_responder_pkinit_get_challenge(krb5_context ctx,
                                    krb5_responder_context rctx,
                                    krb5_responder_pkinit_challenge **chl_out)
{
    const char *challenge;
    k5_json_value j;
    struct get_one_challenge_data get_one_challenge_data;
    krb5_responder_pkinit_challenge *chl = NULL;
    unsigned int n_ids;
    krb5_error_code ret;

    *chl_out = NULL;
    challenge = krb5_responder_get_challenge(ctx, rctx,
                                             KRB5_RESPONDER_QUESTION_PKINIT);
    if (challenge == NULL)
       return 0;

    ret = k5_json_decode(challenge, &j);
    if (ret != 0)
        return ret;

    /* Create the returned object. */
    chl = k5alloc(sizeof(*chl), &ret);
    if (chl == NULL)
        goto failed;

    /* Create the list of identities. */
    n_ids = k5_json_object_count(j);
    chl->identities = k5calloc(n_ids + 1, sizeof(chl->identities[0]), &ret);
    if (chl->identities == NULL)
        goto failed;

    /* Populate the object with identities. */
    memset(&get_one_challenge_data, 0, sizeof(get_one_challenge_data));
    get_one_challenge_data.identities = chl->identities;
    k5_json_object_iterate(j, get_one_challenge, &get_one_challenge_data);
    if (get_one_challenge_data.err != 0) {
        ret = get_one_challenge_data.err;
        goto failed;
    }

    /* All done. */
    k5_json_release(j);
    *chl_out = chl;
    return 0;

failed:
    k5_json_release(j);
    krb5_responder_pkinit_challenge_free(ctx, rctx, chl);
    return ret;
}

krb5_error_code KRB5_CALLCONV
krb5_responder_pkinit_set_answer(krb5_context ctx, krb5_responder_context rctx,
                                 const char *identity, const char *pin)
{
    char *answer = NULL;
    const char *old_answer;
    k5_json_value answers = NULL;
    k5_json_string jpin = NULL;
    krb5_error_code ret = ENOMEM;

    /* If there's an answer already set, we're adding/removing a value. */
    old_answer = k5_response_items_get_answer(rctx->items,
                                              KRB5_RESPONDER_QUESTION_PKINIT);

    /* If we're removing a value, and we have no values, we're done. */
    if (old_answer == NULL && pin == NULL)
        return 0;

    /* Decode the old answers. */
    if (old_answer == NULL)
        old_answer = "{}";
    ret = k5_json_decode(old_answer, &answers);
    if (ret != 0)
        goto cleanup;

    if (k5_json_get_tid(answers) != K5_JSON_TID_OBJECT) {
        ret = EINVAL;
        goto cleanup;
    }

    /* Create and add the new pin string, if we're adding a value. */
    if (pin != NULL) {
        ret = k5_json_string_create(pin, &jpin);
        if (ret != 0)
            goto cleanup;
        ret = k5_json_object_set(answers, identity, jpin);
        if (ret != 0)
            goto cleanup;
    } else {
        ret = k5_json_object_set(answers, identity, NULL);
        if (ret != 0)
            goto cleanup;
    }

    /* Encode and we're done. */
    ret = k5_json_encode(answers, &answer);
    if (ret != 0)
        goto cleanup;

    ret = krb5_responder_set_answer(ctx, rctx, KRB5_RESPONDER_QUESTION_PKINIT,
                                    answer);

cleanup:
    k5_json_release(jpin);
    k5_json_release(answers);
    free(answer);
    return ret;
}

void KRB5_CALLCONV
krb5_responder_pkinit_challenge_free(krb5_context ctx,
                                     krb5_responder_context rctx,
                                     krb5_responder_pkinit_challenge *chl)
{
   unsigned int i;

   if (chl == NULL)
       return;
   for (i = 0; chl->identities != NULL && chl->identities[i] != NULL; i++) {
       free(chl->identities[i]->identity);
       free(chl->identities[i]);
   }
   free(chl->identities);
   free(chl);
}
