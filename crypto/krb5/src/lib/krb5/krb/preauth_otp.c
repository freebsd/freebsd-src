/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* lib/krb5/krb/preauth_otp.c - OTP clpreauth module */
/*
 * Copyright 2011 NORDUnet A/S.  All rights reserved.
 * Copyright 2011 Red Hat, Inc.  All rights reserved.
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

#include "k5-int.h"
#include "k5-json.h"
#include "int-proto.h"
#include "os-proto.h"

#include <krb5/clpreauth_plugin.h>
#include <ctype.h>

static krb5_preauthtype otp_client_supported_pa_types[] =
    { KRB5_PADATA_OTP_CHALLENGE, 0 };

/* Frees a tokeninfo. */
static void
free_tokeninfo(krb5_responder_otp_tokeninfo *ti)
{
    if (ti == NULL)
        return;

    free(ti->alg_id);
    free(ti->challenge);
    free(ti->token_id);
    free(ti->vendor);
    free(ti);
}

/* Converts a property of a json object into a char*. */
static krb5_error_code
codec_value_to_string(k5_json_object obj, const char *key, char **string)
{
    k5_json_value val;
    char *str;

    val = k5_json_object_get(obj, key);
    if (val == NULL)
        return ENOENT;

    if (k5_json_get_tid(val) != K5_JSON_TID_STRING)
        return EINVAL;

    str = strdup(k5_json_string_utf8(val));
    if (str == NULL)
        return ENOMEM;

    *string = str;
    return 0;
}

/* Converts a property of a json object into a krb5_data struct. */
static krb5_error_code
codec_value_to_data(k5_json_object obj, const char *key, krb5_data *data)
{
    krb5_error_code retval;
    char *tmp;

    retval = codec_value_to_string(obj, key, &tmp);
    if (retval != 0)
        return retval;

    *data = string2data(tmp);
    return 0;
}

/* Converts a krb5_data struct into a property of a JSON object. */
static krb5_error_code
codec_data_to_value(krb5_data *data, k5_json_object obj, const char *key)
{
    krb5_error_code retval;
    k5_json_string str;

    if (data->data == NULL)
        return 0;

    retval = k5_json_string_create_len(data->data, data->length, &str);
    if (retval)
        return retval;

    retval = k5_json_object_set(obj, key, str);
    k5_json_release(str);
    return retval;
}

/* Converts a property of a json object into a krb5_int32. */
static krb5_error_code
codec_value_to_int32(k5_json_object obj, const char *key, krb5_int32 *int32)
{
    k5_json_value val;

    val = k5_json_object_get(obj, key);
    if (val == NULL)
        return ENOENT;

    if (k5_json_get_tid(val) != K5_JSON_TID_NUMBER)
        return EINVAL;

    *int32 = k5_json_number_value(val);
    return 0;
}

/* Converts a krb5_int32 into a property of a JSON object. */
static krb5_error_code
codec_int32_to_value(krb5_int32 int32, k5_json_object obj, const char *key)
{
    krb5_error_code retval;
    k5_json_number num;

    if (int32 == -1)
        return 0;

    retval = k5_json_number_create(int32, &num);
    if (retval)
        return retval;

    retval = k5_json_object_set(obj, key, num);
    k5_json_release(num);
    return retval;
}

/* Converts a krb5_otp_tokeninfo into a JSON object. */
static krb5_error_code
codec_encode_tokeninfo(krb5_otp_tokeninfo *ti, k5_json_object *out)
{
    krb5_error_code retval;
    k5_json_object obj;
    krb5_flags flags;

    retval = k5_json_object_create(&obj);
    if (retval != 0)
        goto error;

    flags = KRB5_RESPONDER_OTP_FLAGS_COLLECT_TOKEN;
    if (ti->flags & KRB5_OTP_FLAG_COLLECT_PIN) {
        flags |= KRB5_RESPONDER_OTP_FLAGS_COLLECT_PIN;
        if (ti->flags & KRB5_OTP_FLAG_SEPARATE_PIN)
            flags |= KRB5_RESPONDER_OTP_FLAGS_NEXTOTP;
    }
    if (ti->flags & KRB5_OTP_FLAG_NEXTOTP)
        flags |= KRB5_RESPONDER_OTP_FLAGS_NEXTOTP;

    retval = codec_int32_to_value(flags, obj, "flags");
    if (retval != 0)
        goto error;

    retval = codec_data_to_value(&ti->vendor, obj, "vendor");
    if (retval != 0)
        goto error;

    retval = codec_data_to_value(&ti->challenge, obj, "challenge");
    if (retval != 0)
        goto error;

    retval = codec_int32_to_value(ti->length, obj, "length");
    if (retval != 0)
        goto error;

    if (ti->format != KRB5_OTP_FORMAT_BASE64 &&
        ti->format != KRB5_OTP_FORMAT_BINARY) {
        retval = codec_int32_to_value(ti->format, obj, "format");
        if (retval != 0)
            goto error;
    }

    retval = codec_data_to_value(&ti->token_id, obj, "tokenID");
    if (retval != 0)
        goto error;

    retval = codec_data_to_value(&ti->alg_id, obj, "algID");
    if (retval != 0)
        goto error;

    *out = obj;
    return 0;

error:
    k5_json_release(obj);
    return retval;
}

/* Converts a krb5_pa_otp_challenge into a JSON object. */
static krb5_error_code
codec_encode_challenge(krb5_context ctx, krb5_pa_otp_challenge *chl,
                       char **json)
{
    k5_json_object obj = NULL, tmp = NULL;
    k5_json_string str = NULL;
    k5_json_array arr = NULL;
    krb5_error_code retval;
    int i;

    retval = k5_json_object_create(&obj);
    if (retval != 0)
        goto cleanup;

    if (chl->service.data) {
        retval = k5_json_string_create_len(chl->service.data,
                                           chl->service.length, &str);
        if (retval != 0)
            goto cleanup;
        retval = k5_json_object_set(obj, "service", str);
        k5_json_release(str);
        if (retval != 0)
            goto cleanup;
    }

    retval = k5_json_array_create(&arr);
    if (retval != 0)
        goto cleanup;

    for (i = 0; chl->tokeninfo[i] != NULL ; i++) {
        retval = codec_encode_tokeninfo(chl->tokeninfo[i], &tmp);
        if (retval != 0)
            goto cleanup;

        retval = k5_json_array_add(arr, tmp);
        k5_json_release(tmp);
        if (retval != 0)
            goto cleanup;
    }

    retval = k5_json_object_set(obj, "tokenInfo", arr);
    if (retval != 0)
        goto cleanup;

    retval = k5_json_encode(obj, json);
    if (retval)
        goto cleanup;

cleanup:
    k5_json_release(arr);
    k5_json_release(obj);
    return retval;
}

/* Converts a JSON object into a krb5_responder_otp_tokeninfo. */
static krb5_responder_otp_tokeninfo *
codec_decode_tokeninfo(k5_json_object obj)
{
    krb5_responder_otp_tokeninfo *ti = NULL;
    krb5_error_code retval;

    ti = calloc(1, sizeof(krb5_responder_otp_tokeninfo));
    if (ti == NULL)
        goto error;

    retval = codec_value_to_int32(obj, "flags", &ti->flags);
    if (retval != 0)
        goto error;

    retval = codec_value_to_string(obj, "vendor", &ti->vendor);
    if (retval != 0 && retval != ENOENT)
        goto error;

    retval = codec_value_to_string(obj, "challenge", &ti->challenge);
    if (retval != 0 && retval != ENOENT)
        goto error;

    retval = codec_value_to_int32(obj, "length", &ti->length);
    if (retval == ENOENT)
        ti->length = -1;
    else if (retval != 0)
        goto error;

    retval = codec_value_to_int32(obj, "format", &ti->format);
    if (retval == ENOENT)
        ti->format = -1;
    else if (retval != 0)
        goto error;

    retval = codec_value_to_string(obj, "tokenID", &ti->token_id);
    if (retval != 0 && retval != ENOENT)
        goto error;

    retval = codec_value_to_string(obj, "algID", &ti->alg_id);
    if (retval != 0 && retval != ENOENT)
        goto error;

    return ti;

error:
    free_tokeninfo(ti);
    return NULL;
}

/* Converts a JSON object into a krb5_responder_otp_challenge. */
static krb5_responder_otp_challenge *
codec_decode_challenge(krb5_context ctx, const char *json)
{
    krb5_responder_otp_challenge *chl = NULL;
    k5_json_value obj = NULL, arr = NULL, tmp = NULL;
    krb5_error_code retval;
    size_t i;

    retval = k5_json_decode(json, &obj);
    if (retval != 0)
        goto error;

    if (k5_json_get_tid(obj) != K5_JSON_TID_OBJECT)
        goto error;

    arr = k5_json_object_get(obj, "tokenInfo");
    if (arr == NULL)
        goto error;

    if (k5_json_get_tid(arr) != K5_JSON_TID_ARRAY)
        goto error;

    chl = calloc(1, sizeof(krb5_responder_otp_challenge));
    if (chl == NULL)
        goto error;

    chl->tokeninfo = calloc(k5_json_array_length(arr) + 1,
                            sizeof(krb5_responder_otp_tokeninfo*));
    if (chl->tokeninfo == NULL)
        goto error;

    retval = codec_value_to_string(obj, "service", &chl->service);
    if (retval != 0 && retval != ENOENT)
        goto error;

    for (i = 0; i < k5_json_array_length(arr); i++) {
        tmp = k5_json_array_get(arr, i);
        if (k5_json_get_tid(tmp) != K5_JSON_TID_OBJECT)
            goto error;

        chl->tokeninfo[i] = codec_decode_tokeninfo(tmp);
        if (chl->tokeninfo[i] == NULL)
            goto error;
    }

    k5_json_release(obj);
    return chl;

error:
    if (chl != NULL) {
        for (i = 0; chl->tokeninfo != NULL && chl->tokeninfo[i] != NULL; i++)
            free_tokeninfo(chl->tokeninfo[i]);
        free(chl->tokeninfo);
        free(chl);
    }
    k5_json_release(obj);
    return NULL;
}

/* Decode the responder answer into a tokeninfo, a value and a pin. */
static krb5_error_code
codec_decode_answer(krb5_context context, const char *answer,
                    krb5_otp_tokeninfo **tis, krb5_otp_tokeninfo **ti,
                    krb5_data *value, krb5_data *pin)
{
    krb5_error_code retval;
    k5_json_value val = NULL;
    krb5_int32 indx, i;
    krb5_data tmp;

    if (answer == NULL)
        return EBADMSG;

    retval = k5_json_decode(answer, &val);
    if (retval != 0)
        goto cleanup;

    if (k5_json_get_tid(val) != K5_JSON_TID_OBJECT)
        goto cleanup;

    retval = codec_value_to_int32(val, "tokeninfo", &indx);
    if (retval != 0)
        goto cleanup;

    for (i = 0; tis[i] != NULL; i++) {
        if (i == indx) {
            retval = codec_value_to_data(val, "value", &tmp);
            if (retval != 0 && retval != ENOENT)
                goto cleanup;

            retval = codec_value_to_data(val, "pin", pin);
            if (retval != 0 && retval != ENOENT) {
                krb5_free_data_contents(context, &tmp);
                goto cleanup;
            }

            *value = tmp;
            *ti = tis[i];
            retval = 0;
            goto cleanup;
        }
    }
    retval = EINVAL;

cleanup:
    k5_json_release(val);
    return retval;
}

/* Takes the nonce from the challenge and encrypts it into the request. */
static krb5_error_code
encrypt_nonce(krb5_context ctx, krb5_keyblock *key,
              const krb5_pa_otp_challenge *chl, krb5_pa_otp_req *req)
{
    krb5_error_code retval;
    krb5_enc_data encdata;
    krb5_data *er;

    /* Encode the nonce. */
    retval = encode_krb5_pa_otp_enc_req(&chl->nonce, &er);
    if (retval != 0)
        return retval;

    /* Do the encryption. */
    retval = krb5_encrypt_helper(ctx, key, KRB5_KEYUSAGE_PA_OTP_REQUEST,
                                 er, &encdata);
    krb5_free_data(ctx, er);
    if (retval != 0)
        return retval;

    req->enc_data = encdata;
    return 0;
}

/* Checks to see if the user-supplied otp value matches the length and format
 * of the supplied tokeninfo. */
static int
otpvalue_matches_tokeninfo(const char *otpvalue, krb5_otp_tokeninfo *ti)
{
    int (*table[])(int c) = { isdigit, isxdigit, isalnum };

    if (otpvalue == NULL || ti == NULL)
        return 0;

    if (ti->length >= 0 && strlen(otpvalue) != (size_t)ti->length)
        return 0;

    if (ti->format >= 0 && ti->format < 3) {
        while (*otpvalue) {
            if (!(*table[ti->format])((unsigned char)*otpvalue++))
                return 0;
        }
    }

    return 1;
}

/* Performs a prompt and saves the response in the out parameter. */
static krb5_error_code
doprompt(krb5_context context, krb5_prompter_fct prompter, void *prompter_data,
         const char *banner, const char *prompttxt, char *out, size_t len)
{
    krb5_prompt prompt;
    krb5_data prompt_reply;
    krb5_error_code retval;
    krb5_prompt_type prompt_type = KRB5_PROMPT_TYPE_PREAUTH;

    if (prompttxt == NULL || out == NULL)
        return EINVAL;

    memset(out, 0, len);

    prompt_reply = make_data(out, len);
    prompt.reply = &prompt_reply;
    prompt.prompt = (char *)prompttxt;
    prompt.hidden = 1;

    /* PROMPTER_INVOCATION */
    k5_set_prompt_types(context, &prompt_type);
    retval = (*prompter)(context, prompter_data, NULL, banner, 1, &prompt);
    k5_set_prompt_types(context, NULL);
    if (retval != 0)
        return retval;

    return 0;
}

/* Forces the user to choose a single tokeninfo via prompting. */
static krb5_error_code
prompt_for_tokeninfo(krb5_context context, krb5_prompter_fct prompter,
                     void *prompter_data, krb5_otp_tokeninfo **tis,
                     krb5_otp_tokeninfo **out_ti)
{
    char response[1024], *prompt;
    krb5_otp_tokeninfo *ti = NULL;
    krb5_error_code retval = 0;
    struct k5buf buf;
    int i = 0, j = 0;

    k5_buf_init_dynamic(&buf);
    k5_buf_add(&buf, _("Please choose from the following:\n"));
    for (i = 0; tis[i] != NULL; i++) {
        k5_buf_add_fmt(&buf, "\t%d. %s ", i + 1, _("Vendor:"));
        k5_buf_add_len(&buf, tis[i]->vendor.data, tis[i]->vendor.length);
        k5_buf_add(&buf, "\n");
    }
    prompt = k5_buf_cstring(&buf);
    if (prompt == NULL)
        return ENOMEM;

    do {
        retval = doprompt(context, prompter, prompter_data, prompt,
                          _("Enter #"), response, sizeof(response));
        if (retval != 0)
            goto cleanup;

        errno = 0;
        j = strtol(response, NULL, 0);
        if (errno != 0) {
            retval = errno;
            goto cleanup;
        }
        if (j < 1 || j > i)
            continue;

        ti = tis[--j];
    } while (ti == NULL);

    *out_ti = ti;

cleanup:
    k5_buf_free(&buf);
    return retval;
}

/* Builds a challenge string from the given tokeninfo. */
static krb5_error_code
make_challenge(const krb5_otp_tokeninfo *ti, char **challenge)
{
    if (challenge == NULL)
        return EINVAL;

    *challenge = NULL;

    if (ti == NULL || ti->challenge.data == NULL)
        return 0;

    if (asprintf(challenge, "%s %.*s\n",
                 _("OTP Challenge:"),
                 ti->challenge.length,
                 ti->challenge.data) < 0)
        return ENOMEM;

    return 0;
}

/* Determines if a pin is required. If it is, it will be prompted for. */
static inline krb5_error_code
collect_pin(krb5_context context, krb5_prompter_fct prompter,
            void *prompter_data, const krb5_otp_tokeninfo *ti,
            krb5_data *out_pin)
{
    krb5_error_code retval;
    char otppin[1024];
    krb5_flags collect;
    krb5_data pin;

    /* If no PIN will be collected, don't prompt. */
    collect = ti->flags & (KRB5_OTP_FLAG_COLLECT_PIN |
                           KRB5_OTP_FLAG_SEPARATE_PIN);
    if (collect == 0) {
        *out_pin = empty_data();
        return 0;
    }

    /* Collect the PIN. */
    retval = doprompt(context, prompter, prompter_data, NULL,
                      _("OTP Token PIN"), otppin, sizeof(otppin));
    if (retval != 0)
        return retval;

    /* Set the PIN. */
    pin = make_data(strdup(otppin), strlen(otppin));
    if (pin.data == NULL)
        return ENOMEM;

    *out_pin = pin;
    return 0;
}

/* Builds a request using the specified tokeninfo, value and pin. */
static krb5_error_code
make_request(krb5_context ctx, krb5_otp_tokeninfo *ti, const krb5_data *value,
             const krb5_data *pin, krb5_pa_otp_req **out_req)
{
    krb5_pa_otp_req *req = NULL;
    krb5_error_code retval = 0;

    if (ti == NULL)
        return 0;

    if (ti->format == KRB5_OTP_FORMAT_BASE64)
        return ENOTSUP;

    req = calloc(1, sizeof(krb5_pa_otp_req));
    if (req == NULL)
        return ENOMEM;

    req->flags = ti->flags & KRB5_OTP_FLAG_NEXTOTP;

    retval = krb5int_copy_data_contents(ctx, &ti->vendor, &req->vendor);
    if (retval != 0)
        goto error;

    req->format = ti->format;

    retval = krb5int_copy_data_contents(ctx, &ti->token_id, &req->token_id);
    if (retval != 0)
        goto error;

    retval = krb5int_copy_data_contents(ctx, &ti->alg_id, &req->alg_id);
    if (retval != 0)
        goto error;

    retval = krb5int_copy_data_contents(ctx, value, &req->otp_value);
    if (retval != 0)
        goto error;

    if (ti->flags & KRB5_OTP_FLAG_COLLECT_PIN) {
        if (ti->flags & KRB5_OTP_FLAG_SEPARATE_PIN) {
            if (pin == NULL || pin->data == NULL) {
                retval = EINVAL; /* No pin found! */
                goto error;
            }

            retval = krb5int_copy_data_contents(ctx, pin, &req->pin);
            if (retval != 0)
                goto error;
        } else if (pin != NULL && pin->data != NULL) {
            krb5_free_data_contents(ctx, &req->otp_value);
            retval = asprintf(&req->otp_value.data, "%.*s%.*s",
                              pin->length, pin->data,
                              value->length, value->data);
            if (retval < 0) {
                retval = ENOMEM;
                req->otp_value = empty_data();
                goto error;
            }
            req->otp_value.length = req->pin.length + req->otp_value.length;
        } /* Otherwise, the responder has already combined them. */
    }

    *out_req = req;
    return 0;

error:
    k5_free_pa_otp_req(ctx, req);
    return retval;
}

/*
 * Filters a set of tokeninfos given an otp value.  If the set is reduced to
 * a single tokeninfo, it will be set in out_ti.  Otherwise, a new shallow copy
 * will be allocated in out_filtered.
 */
static inline krb5_error_code
filter_tokeninfos(krb5_context context, const char *otpvalue,
                  krb5_otp_tokeninfo **tis,
                  krb5_otp_tokeninfo ***out_filtered,
                  krb5_otp_tokeninfo **out_ti)
{
    krb5_otp_tokeninfo **filtered;
    size_t i = 0, j = 0;

    while (tis[i] != NULL)
        i++;

    filtered = calloc(i + 1, sizeof(const krb5_otp_tokeninfo *));
    if (filtered == NULL)
        return ENOMEM;

    /* Make a list of tokeninfos that match the value. */
    for (i = 0, j = 0; tis[i] != NULL; i++) {
        if (otpvalue_matches_tokeninfo(otpvalue, tis[i]))
            filtered[j++] = tis[i];
    }

    /* It is an error if we have no matching tokeninfos. */
    if (filtered[0] == NULL) {
        free(filtered);
        k5_setmsg(context, KRB5_PREAUTH_FAILED,
                  _("OTP value doesn't match any token formats"));
        return KRB5_PREAUTH_FAILED; /* We have no supported tokeninfos. */
    }

    /* Otherwise, if we have just one tokeninfo, choose it. */
    if (filtered[1] == NULL) {
        *out_ti = filtered[0];
        *out_filtered = NULL;
        free(filtered);
        return 0;
    }

    /* Otherwise, we'll return the remaining list. */
    *out_ti = NULL;
    *out_filtered = filtered;
    return 0;
}

/* Outputs the selected tokeninfo and possibly a value and pin.
 * Prompting may occur. */
static krb5_error_code
prompt_for_token(krb5_context context, krb5_prompter_fct prompter,
                 void *prompter_data, krb5_otp_tokeninfo **tis,
                 krb5_otp_tokeninfo **out_ti, krb5_data *out_value,
                 krb5_data *out_pin)
{
    krb5_otp_tokeninfo **filtered = NULL;
    krb5_otp_tokeninfo *ti = NULL;
    krb5_error_code retval;
    int i, challengers = 0;
    char *challenge = NULL;
    char otpvalue[1024];
    krb5_data value, pin;

    memset(otpvalue, 0, sizeof(otpvalue));

    if (tis == NULL || tis[0] == NULL || out_ti == NULL)
        return EINVAL;

    /* Count how many challenges we have. */
    for (i = 0; tis[i] != NULL; i++) {
        if (tis[i]->challenge.data != NULL)
            challengers++;
    }

    /* If we have only one tokeninfo as input, choose it. */
    if (i == 1)
        ti = tis[0];

    /* Setup our challenge, if present. */
    if (challengers > 0) {
        /* If we have multiple tokeninfos still, choose now. */
        if (ti == NULL) {
            retval = prompt_for_tokeninfo(context, prompter, prompter_data,
                                          tis, &ti);
            if (retval != 0)
                return retval;
        }

        /* Create the challenge prompt. */
        retval = make_challenge(ti, &challenge);
        if (retval != 0)
            return retval;
    }

    /* Prompt for token value. */
    retval = doprompt(context, prompter, prompter_data, challenge,
                      _("Enter OTP Token Value"), otpvalue, sizeof(otpvalue));
    free(challenge);
    if (retval != 0)
        return retval;

    if (ti == NULL) {
        /* Filter out tokeninfos that don't match our token value. */
        retval = filter_tokeninfos(context, otpvalue, tis, &filtered, &ti);
        if (retval != 0)
            return retval;

        /* If we still don't have a single tokeninfo, choose now. */
        if (filtered != NULL) {
            retval = prompt_for_tokeninfo(context, prompter, prompter_data,
                                          filtered, &ti);
            free(filtered);
            if (retval != 0)
                return retval;
        }
    }

    assert(ti != NULL);

    /* Set the value. */
    value = make_data(strdup(otpvalue), strlen(otpvalue));
    if (value.data == NULL)
        return ENOMEM;

    /* Collect the PIN, if necessary. */
    retval = collect_pin(context, prompter, prompter_data, ti, &pin);
    if (retval != 0) {
        krb5_free_data_contents(context, &value);
        return retval;
    }

    *out_value = value;
    *out_pin = pin;
    *out_ti = ti;
    return 0;
}

/* Encode the OTP request into a krb5_pa_data buffer. */
static krb5_error_code
set_pa_data(const krb5_pa_otp_req *req, krb5_pa_data ***pa_data_out)
{
    krb5_pa_data **out = NULL;
    krb5_data *tmp;

    /* Allocate the preauth data array and one item. */
    out = calloc(2, sizeof(krb5_pa_data *));
    if (out == NULL)
        goto error;
    out[0] = calloc(1, sizeof(krb5_pa_data));
    out[1] = NULL;
    if (out[0] == NULL)
        goto error;

    /* Encode our request into the preauth data item. */
    memset(out[0], 0, sizeof(krb5_pa_data));
    out[0]->pa_type = KRB5_PADATA_OTP_REQUEST;
    if (encode_krb5_pa_otp_req(req, &tmp) != 0)
        goto error;
    out[0]->contents = (krb5_octet *)tmp->data;
    out[0]->length = tmp->length;
    free(tmp);

    *pa_data_out = out;
    return 0;

error:
    if (out != NULL) {
        free(out[0]);
        free(out);
    }
    return ENOMEM;
}

/* Tests krb5_data to see if it is printable. */
static krb5_boolean
is_printable_string(const krb5_data *data)
{
    unsigned int i;

    if (data == NULL)
        return FALSE;

    for (i = 0; i < data->length; i++) {
        if (!isprint((unsigned char)data->data[i]))
            return FALSE;
    }

    return TRUE;
}

/* Returns TRUE when the given tokeninfo contains the subset of features we
 * support. */
static krb5_boolean
is_tokeninfo_supported(krb5_otp_tokeninfo *ti)
{
    krb5_flags supported_flags = KRB5_OTP_FLAG_COLLECT_PIN |
                                 KRB5_OTP_FLAG_NO_COLLECT_PIN |
                                 KRB5_OTP_FLAG_SEPARATE_PIN;

    /* Flags we don't support... */
    if (ti->flags & ~supported_flags)
        return FALSE;

    /* We don't currently support hashing. */
    if (ti->supported_hash_alg != NULL || ti->iteration_count >= 0)
        return FALSE;

    /* Remove tokeninfos with invalid vendor strings. */
    if (!is_printable_string(&ti->vendor))
        return FALSE;

    /* Remove tokeninfos with non-printable challenges. */
    if (!is_printable_string(&ti->challenge))
        return FALSE;

    /* We don't currently support base64. */
    if (ti->format == KRB5_OTP_FORMAT_BASE64)
        return FALSE;

    return TRUE;
}

/* Removes unsupported tokeninfos. Returns an error if no tokeninfos remain. */
static krb5_error_code
filter_supported_tokeninfos(krb5_context context, krb5_otp_tokeninfo **tis)
{
    size_t i, j;

    /* Filter out any tokeninfos we don't support. */
    for (i = 0, j = 0; tis[i] != NULL; i++) {
        if (!is_tokeninfo_supported(tis[i]))
            k5_free_otp_tokeninfo(context, tis[i]);
        else
            tis[j++] = tis[i];
    }

    /* Terminate the array. */
    tis[j] = NULL;

    if (tis[0] != NULL)
        return 0;

    k5_setmsg(context, KRB5_PREAUTH_FAILED, _("No supported tokens"));
    return KRB5_PREAUTH_FAILED; /* We have no supported tokeninfos. */
}

/*
 * Try to find tokeninfos which match configuration data recorded in the input
 * ccache, and if exactly one is found, drop the rest.
 */
static krb5_error_code
filter_config_tokeninfos(krb5_context context,
                         krb5_clpreauth_callbacks cb,
                         krb5_clpreauth_rock rock,
                         krb5_otp_tokeninfo **tis)
{
    krb5_otp_tokeninfo *match = NULL;
    size_t i, j;
    const char *vendor, *alg_id, *token_id;

    /* Pull up what we know about the token we want to use. */
    vendor = cb->get_cc_config(context, rock, "vendor");
    alg_id = cb->get_cc_config(context, rock, "algID");
    token_id = cb->get_cc_config(context, rock, "tokenID");

    /* Look for a single matching entry. */
    for (i = 0; tis[i] != NULL; i++) {
        if (vendor != NULL && tis[i]->vendor.length > 0 &&
            !data_eq_string(tis[i]->vendor, vendor))
            continue;
        if (alg_id != NULL && tis[i]->alg_id.length > 0 &&
            !data_eq_string(tis[i]->alg_id, alg_id))
            continue;
        if (token_id != NULL && tis[i]->token_id.length > 0 &&
            !data_eq_string(tis[i]->token_id, token_id))
            continue;
        /* Oh, we already had a matching entry. More than one -> no change. */
        if (match != NULL)
            return 0;
        match = tis[i];
    }

    /* No matching entry -> no change. */
    if (match == NULL)
        return 0;

    /* Prune out everything except the best match. */
    for (i = 0, j = 0; tis[i] != NULL; i++) {
        if (tis[i] != match)
            k5_free_otp_tokeninfo(context, tis[i]);
        else
            tis[j++] = tis[i];
    }
    tis[j] = NULL;

    return 0;
}

static void
otp_client_request_init(krb5_context context, krb5_clpreauth_moddata moddata,
                        krb5_clpreauth_modreq *modreq_out)
{
    *modreq_out = calloc(1, sizeof(krb5_pa_otp_challenge *));
}

static krb5_error_code
otp_client_prep_questions(krb5_context context, krb5_clpreauth_moddata moddata,
                          krb5_clpreauth_modreq modreq,
                          krb5_get_init_creds_opt *opt,
                          krb5_clpreauth_callbacks cb,
                          krb5_clpreauth_rock rock, krb5_kdc_req *request,
                          krb5_data *encoded_request_body,
                          krb5_data *encoded_previous_request,
                          krb5_pa_data *pa_data)
{
    krb5_pa_otp_challenge *chl;
    krb5_error_code retval;
    krb5_data tmp;
    char *json;

    if (modreq == NULL)
        return ENOMEM;

    /* Decode the challenge. */
    tmp = make_data(pa_data->contents, pa_data->length);
    retval = decode_krb5_pa_otp_challenge(&tmp,
                                          (krb5_pa_otp_challenge **)modreq);
    if (retval != 0)
        return retval;
    chl = *(krb5_pa_otp_challenge **)modreq;

    /* Remove unsupported tokeninfos. */
    retval = filter_supported_tokeninfos(context, chl->tokeninfo);
    if (retval != 0)
        return retval;

    /* Remove tokeninfos that don't match the recorded description, if that
     * results in there being only one that does. */
    retval = filter_config_tokeninfos(context, cb, rock, chl->tokeninfo);
    if (retval != 0)
        return retval;

    /* Make the JSON representation. */
    retval = codec_encode_challenge(context, chl, &json);
    if (retval != 0)
        return retval;

    /* Ask the question. */
    retval = cb->ask_responder_question(context, rock,
                                        KRB5_RESPONDER_QUESTION_OTP,
                                        json);
    free(json);
    return retval;
}

/*
 * Save the vendor, algID, and tokenID values for the selected token to the
 * out_ccache, so that later we can try to use them to select the right one
 * without having ot ask the user.
 */
static void
save_config_tokeninfo(krb5_context context,
                      krb5_clpreauth_callbacks cb,
                      krb5_clpreauth_rock rock,
                      krb5_otp_tokeninfo *ti)
{
    char *tmp;
    if (ti->vendor.length > 0 &&
        asprintf(&tmp, "%.*s", ti->vendor.length, ti->vendor.data) >= 0) {
        cb->set_cc_config(context, rock, "vendor", tmp);
        free(tmp);
    }
    if (ti->alg_id.length > 0 &&
        asprintf(&tmp, "%.*s", ti->alg_id.length, ti->alg_id.data) >= 0) {
        cb->set_cc_config(context, rock, "algID", tmp);
        free(tmp);
    }
    if (ti->token_id.length > 0 &&
        asprintf(&tmp, "%.*s", ti->token_id.length, ti->token_id.data) >= 0) {
        cb->set_cc_config(context, rock, "tokenID", tmp);
        free(tmp);
    }
}

static krb5_error_code
otp_client_process(krb5_context context, krb5_clpreauth_moddata moddata,
                   krb5_clpreauth_modreq modreq, krb5_get_init_creds_opt *opt,
                   krb5_clpreauth_callbacks cb, krb5_clpreauth_rock rock,
                   krb5_kdc_req *request, krb5_data *encoded_request_body,
                   krb5_data *encoded_previous_request, krb5_pa_data *pa_data,
                   krb5_prompter_fct prompter, void *prompter_data,
                   krb5_pa_data ***pa_data_out)
{
    krb5_pa_otp_challenge *chl = NULL;
    krb5_otp_tokeninfo *ti = NULL;
    krb5_keyblock *as_key = NULL;
    krb5_pa_otp_req *req = NULL;
    krb5_error_code retval = 0;
    krb5_data value, pin;
    const char *answer;

    if (modreq == NULL)
        return ENOMEM;
    chl = *(krb5_pa_otp_challenge **)modreq;

    *pa_data_out = NULL;

    /* Get FAST armor key. */
    as_key = cb->fast_armor(context, rock);
    if (as_key == NULL)
        return ENOENT;

    /* Attempt to get token selection from the responder. */
    pin = empty_data();
    value = empty_data();
    answer = cb->get_responder_answer(context, rock,
                                      KRB5_RESPONDER_QUESTION_OTP);
    retval = codec_decode_answer(context, answer, chl->tokeninfo, &ti, &value,
                                 &pin);
    if (retval != 0) {
        /* If the responder doesn't have a token selection,
         * we need to select the token via prompting. */
        retval = prompt_for_token(context, prompter, prompter_data,
                                  chl->tokeninfo, &ti, &value, &pin);
        if (retval != 0)
            goto error;
    }

    /* Make the request. */
    retval = make_request(context, ti, &value, &pin, &req);
    if (retval != 0)
        goto error;

    /* Save information about the token which was used. */
    save_config_tokeninfo(context, cb, rock, ti);

    /* Encrypt the challenge's nonce and set it in the request. */
    retval = encrypt_nonce(context, as_key, chl, req);
    if (retval != 0)
        goto error;

    /* Use FAST armor key as response key. */
    retval = cb->set_as_key(context, rock, as_key);
    if (retval != 0)
        goto error;

    /* Encode the request into the pa_data output. */
    retval = set_pa_data(req, pa_data_out);
    if (retval != 0)
        goto error;
    cb->disable_fallback(context, rock);

error:
    krb5_free_data_contents(context, &value);
    krb5_free_data_contents(context, &pin);
    k5_free_pa_otp_req(context, req);
    return retval;
}

static void
otp_client_request_fini(krb5_context context, krb5_clpreauth_moddata moddata,
                        krb5_clpreauth_modreq modreq)
{
    if (modreq == NULL)
        return;

    k5_free_pa_otp_challenge(context, *(krb5_pa_otp_challenge **)modreq);
    free(modreq);
}

krb5_error_code
clpreauth_otp_initvt(krb5_context context, int maj_ver, int min_ver,
                     krb5_plugin_vtable vtable)
{
    krb5_clpreauth_vtable vt;

    if (maj_ver != 1)
        return KRB5_PLUGIN_VER_NOTSUPP;

    vt = (krb5_clpreauth_vtable)vtable;
    vt->name = "otp";
    vt->pa_type_list = otp_client_supported_pa_types;
    vt->request_init = otp_client_request_init;
    vt->prep_questions = otp_client_prep_questions;
    vt->process = otp_client_process;
    vt->request_fini = otp_client_request_fini;
    vt->gic_opts = NULL;

    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_responder_otp_get_challenge(krb5_context ctx,
                                 krb5_responder_context rctx,
                                 krb5_responder_otp_challenge **chl)
{
    const char *answer;
    krb5_responder_otp_challenge *challenge;

    answer = krb5_responder_get_challenge(ctx, rctx,
                                          KRB5_RESPONDER_QUESTION_OTP);
    if (answer == NULL) {
        *chl = NULL;
        return 0;
    }

    challenge = codec_decode_challenge(ctx, answer);
    if (challenge == NULL)
        return ENOMEM;

    *chl = challenge;
    return 0;
}

krb5_error_code KRB5_CALLCONV
krb5_responder_otp_set_answer(krb5_context ctx, krb5_responder_context rctx,
                              size_t ti, const char *value, const char *pin)
{
    krb5_error_code retval;
    k5_json_object obj = NULL;
    k5_json_number num;
    k5_json_string str;
    char *tmp;

    retval = k5_json_object_create(&obj);
    if (retval != 0)
        goto error;

    retval = k5_json_number_create(ti, &num);
    if (retval != 0)
        goto error;

    retval = k5_json_object_set(obj, "tokeninfo", num);
    k5_json_release(num);
    if (retval != 0)
        goto error;

    if (value != NULL) {
        retval = k5_json_string_create(value, &str);
        if (retval != 0)
            goto error;

        retval = k5_json_object_set(obj, "value", str);
        k5_json_release(str);
        if (retval != 0)
            goto error;
    }

    if (pin != NULL) {
        retval = k5_json_string_create(pin, &str);
        if (retval != 0)
            goto error;

        retval = k5_json_object_set(obj, "pin", str);
        k5_json_release(str);
        if (retval != 0)
            goto error;
    }

    retval = k5_json_encode(obj, &tmp);
    if (retval != 0)
        goto error;
    k5_json_release(obj);

    retval = krb5_responder_set_answer(ctx, rctx, KRB5_RESPONDER_QUESTION_OTP,
                                       tmp);
    free(tmp);
    return retval;

error:
    k5_json_release(obj);
    return retval;
}

void KRB5_CALLCONV
krb5_responder_otp_challenge_free(krb5_context ctx,
                                  krb5_responder_context rctx,
                                  krb5_responder_otp_challenge *chl)
{
    size_t i;

    if (chl == NULL)
        return;

    for (i = 0; chl->tokeninfo[i]; i++)
        free_tokeninfo(chl->tokeninfo[i]);
    free(chl->service);
    free(chl->tokeninfo);
    free(chl);
}
