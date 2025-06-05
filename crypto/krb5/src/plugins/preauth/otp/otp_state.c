/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* plugins/preauth/otp/otp_state.c - Verify OTP token values using RADIUS */
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

#include "otp_state.h"

#include <krad.h>
#include <k5-json.h>

#include <ctype.h>

#ifndef HOST_NAME_MAX
/* SUSv2 */
#define HOST_NAME_MAX 255
#endif

#define DEFAULT_TYPE_NAME "DEFAULT"
#define DEFAULT_SOCKET_FMT KDC_RUN_DIR "/%s.socket"
#define DEFAULT_TIMEOUT 5
#define DEFAULT_RETRIES 3
#define MAX_SECRET_LEN 1024

typedef struct token_type_st {
    char *name;
    char *server;
    char *secret;
    int timeout;
    size_t retries;
    krb5_boolean strip_realm;
    char **indicators;
} token_type;

typedef struct token_st {
    const token_type *type;
    krb5_data username;
    char **indicators;
} token;

typedef struct request_st {
    otp_state *state;
    token *tokens;
    ssize_t index;
    otp_cb cb;
    void *data;
    krad_attrset *attrs;
} request;

struct otp_state_st {
    krb5_context ctx;
    token_type *types;
    krad_client *radius;
    krad_attrset *attrs;
};

static void request_send(request *req);

static krb5_error_code
read_secret_file(const char *secret_file, char **secret)
{
    char buf[MAX_SECRET_LEN];
    krb5_error_code retval;
    char *filename = NULL;
    FILE *file;
    size_t i, j;

    *secret = NULL;

    retval = k5_path_join(KDC_DIR, secret_file, &filename);
    if (retval != 0) {
        com_err("otp", retval, "Unable to resolve secret file '%s'", filename);
        goto cleanup;
    }

    file = fopen(filename, "r");
    if (file == NULL) {
        retval = errno;
        com_err("otp", retval, "Unable to open secret file '%s'", filename);
        goto cleanup;
    }

    if (fgets(buf, sizeof(buf), file) == NULL)
        retval = EIO;
    fclose(file);
    if (retval != 0) {
        com_err("otp", retval, "Unable to read secret file '%s'", filename);
        goto cleanup;
    }

    /* Strip whitespace. */
    for (i = 0; buf[i] != '\0'; i++) {
        if (!isspace(buf[i]))
            break;
    }
    for (j = strlen(buf); j > i; j--) {
        if (!isspace(buf[j - 1]))
            break;
    }

    *secret = k5memdup0(&buf[i], j - i, &retval);

cleanup:
    free(filename);
    return retval;
}

/* Free the contents of a single token type. */
static void
token_type_free(token_type *type)
{
    if (type == NULL)
        return;

    free(type->name);
    free(type->server);
    free(type->secret);
    profile_free_list(type->indicators);
}

/* Construct the internal default token type. */
static krb5_error_code
token_type_default(token_type *out)
{
    char *name = NULL, *server = NULL, *secret = NULL;

    memset(out, 0, sizeof(*out));

    name = strdup(DEFAULT_TYPE_NAME);
    if (name == NULL)
        goto oom;
    if (asprintf(&server, DEFAULT_SOCKET_FMT, name) < 0)
        goto oom;
    secret = strdup("");
    if (secret == NULL)
        goto oom;

    out->name = name;
    out->server = server;
    out->secret = secret;
    out->timeout = DEFAULT_TIMEOUT * 1000;
    out->retries = DEFAULT_RETRIES;
    out->strip_realm = FALSE;
    return 0;

oom:
    free(name);
    free(server);
    free(secret);
    return ENOMEM;
}

/* Decode a single token type from the profile. */
static krb5_error_code
token_type_decode(profile_t profile, const char *name, token_type *out)
{
    char *server = NULL, *name_copy = NULL, *secret = NULL, *pstr = NULL;
    char **indicators = NULL;
    const char *keys[4];
    int strip_realm, timeout, retries;
    krb5_error_code retval;

    memset(out, 0, sizeof(*out));

    /* Set the name. */
    name_copy = strdup(name);
    if (name_copy == NULL)
        return ENOMEM;

    /* Set strip_realm. */
    retval = profile_get_boolean(profile, "otp", name, "strip_realm", TRUE,
                                 &strip_realm);
    if (retval != 0)
        goto cleanup;

    /* Set the server. */
    retval = profile_get_string(profile, "otp", name, "server", NULL, &pstr);
    if (retval != 0)
        goto cleanup;
    if (pstr != NULL) {
        server = strdup(pstr);
        profile_release_string(pstr);
        if (server == NULL) {
            retval = ENOMEM;
            goto cleanup;
        }
    } else if (asprintf(&server, DEFAULT_SOCKET_FMT, name) < 0) {
        retval = ENOMEM;
        goto cleanup;
    }

    /* Get the secret (optional for Unix-domain sockets). */
    retval = profile_get_string(profile, "otp", name, "secret", NULL, &pstr);
    if (retval != 0)
        goto cleanup;
    if (pstr != NULL) {
        retval = read_secret_file(pstr, &secret);
        profile_release_string(pstr);
        if (retval != 0)
            goto cleanup;
    } else {
        if (server[0] != '/') {
            com_err("otp", EINVAL, "Secret missing (token type '%s')", name);
            retval = EINVAL;
            goto cleanup;
        }

        /* Use the default empty secret for UNIX domain stream sockets. */
        secret = strdup("");
        if (secret == NULL) {
            retval = ENOMEM;
            goto cleanup;
        }
    }

    /* Get the timeout (profile value in seconds, result in milliseconds). */
    retval = profile_get_integer(profile, "otp", name, "timeout",
                                 DEFAULT_TIMEOUT, &timeout);
    if (retval != 0)
        goto cleanup;
    timeout *= 1000;

    /* Get the number of retries. */
    retval = profile_get_integer(profile, "otp", name, "retries",
                                 DEFAULT_RETRIES, &retries);
    if (retval != 0)
        goto cleanup;

    /* Get the authentication indicators to assert if this token is used. */
    keys[0] = "otp";
    keys[1] = name;
    keys[2] = "indicator";
    keys[3] = NULL;
    retval = profile_get_values(profile, keys, &indicators);
    if (retval == PROF_NO_RELATION)
        retval = 0;
    if (retval != 0)
        goto cleanup;

    out->name = name_copy;
    out->server = server;
    out->secret = secret;
    out->timeout = timeout;
    out->retries = retries;
    out->strip_realm = strip_realm;
    out->indicators = indicators;
    name_copy = server = secret = NULL;
    indicators = NULL;

cleanup:
    free(name_copy);
    free(server);
    free(secret);
    profile_free_list(indicators);
    return retval;
}

/* Free an array of token types. */
static void
token_types_free(token_type *types)
{
    size_t i;

    if (types == NULL)
        return;

    for (i = 0; types[i].server != NULL; i++)
        token_type_free(&types[i]);

    free(types);
}

/* Decode an array of token types from the profile. */
static krb5_error_code
token_types_decode(profile_t profile, token_type **out)
{
    const char *hier[2] = { "otp", NULL };
    token_type *types = NULL;
    char **names = NULL;
    krb5_error_code retval;
    size_t i, pos;
    krb5_boolean have_default = FALSE;

    retval = profile_get_subsection_names(profile, hier, &names);
    if (retval != 0)
        return retval;

    /* Check if any of the profile subsections overrides the default. */
    for (i = 0; names[i] != NULL; i++) {
        if (strcmp(names[i], DEFAULT_TYPE_NAME) == 0)
            have_default = TRUE;
    }

    /* Leave space for the default (possibly) and the terminator. */
    types = k5calloc(i + 2, sizeof(token_type), &retval);
    if (types == NULL)
        goto cleanup;

    /* If no default has been specified, use our internal default. */
    pos = 0;
    if (!have_default) {
        retval = token_type_default(&types[pos++]);
        if (retval != 0)
            goto cleanup;
    }

    /* Decode each profile section into a token type element. */
    for (i = 0; names[i] != NULL; i++) {
        retval = token_type_decode(profile, names[i], &types[pos++]);
        if (retval != 0)
            goto cleanup;
    }

    *out = types;
    types = NULL;

cleanup:
    profile_free_list(names);
    token_types_free(types);
    return retval;
}

/* Free a null-terminated array of strings. */
static void
free_strings(char **list)
{
    char **p;

    for (p = list; p != NULL && *p != NULL; p++)
        free(*p);
    free(list);
}

/* Free the contents of a single token. */
static void
token_free_contents(token *t)
{
    if (t == NULL)
        return;
    free(t->username.data);
    free_strings(t->indicators);
}

/* Decode a JSON array of strings into a null-terminated list of C strings. */
static krb5_error_code
indicators_decode(krb5_context ctx, k5_json_value val, char ***indicators_out)
{
    k5_json_array arr;
    k5_json_value obj;
    char **indicators;
    size_t len, i;

    *indicators_out = NULL;

    if (k5_json_get_tid(val) != K5_JSON_TID_ARRAY)
        return EINVAL;
    arr = val;
    len = k5_json_array_length(arr);
    indicators = calloc(len + 1, sizeof(*indicators));
    if (indicators == NULL)
        return ENOMEM;

    for (i = 0; i < len; i++) {
        obj = k5_json_array_get(arr, i);
        if (k5_json_get_tid(obj) != K5_JSON_TID_STRING) {
            free_strings(indicators);
            return EINVAL;
        }
        indicators[i] = strdup(k5_json_string_utf8(obj));
        if (indicators[i] == NULL) {
            free_strings(indicators);
            return ENOMEM;
        }
    }

    *indicators_out = indicators;
    return 0;
}

/* Decode a single token from a JSON token object. */
static krb5_error_code
token_decode(krb5_context ctx, krb5_const_principal princ,
             const token_type *types, k5_json_object obj, token *out)
{
    const char *typename = DEFAULT_TYPE_NAME;
    const token_type *type = NULL;
    char *username = NULL, **indicators = NULL;
    krb5_error_code retval;
    k5_json_value val;
    size_t i;
    int flags;

    memset(out, 0, sizeof(*out));

    /* Find the token type. */
    val = k5_json_object_get(obj, "type");
    if (val != NULL && k5_json_get_tid(val) == K5_JSON_TID_STRING)
        typename = k5_json_string_utf8(val);
    for (i = 0; types[i].server != NULL; i++) {
        if (strcmp(typename, types[i].name) == 0)
            type = &types[i];
    }
    if (type == NULL)
        return EINVAL;

    /* Get the username, either from obj or from unparsing the principal. */
    val = k5_json_object_get(obj, "username");
    if (val != NULL && k5_json_get_tid(val) == K5_JSON_TID_STRING) {
        username = strdup(k5_json_string_utf8(val));
        if (username == NULL)
            return ENOMEM;
    } else {
        flags = type->strip_realm ? KRB5_PRINCIPAL_UNPARSE_NO_REALM : 0;
        retval = krb5_unparse_name_flags(ctx, princ, flags, &username);
        if (retval != 0)
            return retval;
    }

    /* Get the authentication indicators if specified. */
    val = k5_json_object_get(obj, "indicators");
    if (val != NULL) {
        retval = indicators_decode(ctx, val, &indicators);
        if (retval != 0) {
            free(username);
            return retval;
        }
    }

    out->type = type;
    out->username = string2data(username);
    out->indicators = indicators;
    return 0;
}

/* Free an array of tokens. */
static void
tokens_free(token *tokens)
{
    size_t i;

    if (tokens == NULL)
        return;

    for (i = 0; tokens[i].type != NULL; i++)
        token_free_contents(&tokens[i]);

    free(tokens);
}

/* Decode a principal config string into a JSON array.  Treat an empty string
 * or array as if it were "[{}]" which uses the default token type. */
static krb5_error_code
decode_config_json(const char *config, k5_json_array *out)
{
    krb5_error_code retval;
    k5_json_value val;
    k5_json_object obj;

    *out = NULL;

    /* Decode the config string and make sure it's an array. */
    retval = k5_json_decode((config != NULL) ? config : "[{}]", &val);
    if (retval != 0)
        goto error;
    if (k5_json_get_tid(val) != K5_JSON_TID_ARRAY) {
        retval = EINVAL;
        goto error;
    }

    /* If the array is empty, add in an empty object. */
    if (k5_json_array_length(val) == 0) {
        retval = k5_json_object_create(&obj);
        if (retval != 0)
            goto error;
        retval = k5_json_array_add(val, obj);
        k5_json_release(obj);
        if (retval != 0)
            goto error;
    }

    *out = val;
    return 0;

error:
    k5_json_release(val);
    return retval;
}

/* Decode an array of tokens from the configuration string. */
static krb5_error_code
tokens_decode(krb5_context ctx, krb5_const_principal princ,
              const token_type *types, const char *config, token **out)
{
    krb5_error_code retval;
    k5_json_array arr = NULL;
    k5_json_value obj;
    token *tokens = NULL;
    size_t len, i;

    retval = decode_config_json(config, &arr);
    if (retval != 0)
        return retval;
    len = k5_json_array_length(arr);

    tokens = k5calloc(len + 1, sizeof(token), &retval);
    if (tokens == NULL)
        goto cleanup;

    for (i = 0; i < len; i++) {
        obj = k5_json_array_get(arr, i);
        if (k5_json_get_tid(obj) != K5_JSON_TID_OBJECT) {
            retval = EINVAL;
            goto cleanup;
        }
        retval = token_decode(ctx, princ, types, obj, &tokens[i]);
        if (retval != 0)
            goto cleanup;
    }

    *out = tokens;
    tokens = NULL;

cleanup:
    k5_json_release(arr);
    tokens_free(tokens);
    return retval;
}

static void
request_free(request *req)
{
    if (req == NULL)
        return;

    krad_attrset_free(req->attrs);
    tokens_free(req->tokens);
    free(req);
}

krb5_error_code
otp_state_new(krb5_context ctx, otp_state **out)
{
    char hostname[HOST_NAME_MAX + 1];
    krb5_error_code retval;
    profile_t profile;
    krb5_data hndata;
    otp_state *self;

    retval = gethostname(hostname, sizeof(hostname));
    if (retval != 0)
        return retval;

    self = calloc(1, sizeof(otp_state));
    if (self == NULL)
        return ENOMEM;

    retval = krb5_get_profile(ctx, &profile);
    if (retval != 0)
        goto error;

    retval = token_types_decode(profile, &self->types);
    profile_abandon(profile);
    if (retval != 0)
        goto error;

    retval = krad_attrset_new(ctx, &self->attrs);
    if (retval != 0)
        goto error;

    hndata = make_data(hostname, strlen(hostname));
    retval = krad_attrset_add(self->attrs,
                              krad_attr_name2num("NAS-Identifier"), &hndata);
    if (retval != 0)
        goto error;

    retval = krad_attrset_add_number(self->attrs,
                                     krad_attr_name2num("Service-Type"),
                                     KRAD_SERVICE_TYPE_AUTHENTICATE_ONLY);
    if (retval != 0)
        goto error;

    self->ctx = ctx;
    *out = self;
    return 0;

error:
    otp_state_free(self);
    return retval;
}

void
otp_state_free(otp_state *self)
{
    if (self == NULL)
        return;

    krad_attrset_free(self->attrs);
    krad_client_free(self->radius);
    token_types_free(self->types);
    free(self);
}

static void
callback(krb5_error_code retval, const krad_packet *rqst,
         const krad_packet *resp, void *data)
{
    request *req = data;
    token *tok = &req->tokens[req->index];
    char *const *indicators;

    req->index++;

    if (retval != 0)
        goto error;

    /* If we received an accept packet, success! */
    if (krad_packet_get_code(resp) ==
        krad_code_name2num("Access-Accept")) {
        indicators = tok->indicators;
        if (indicators == NULL)
            indicators = tok->type->indicators;
        req->cb(req->data, retval, otp_response_success, indicators);
        request_free(req);
        return;
    }

    /* If we have no more tokens to try, failure! */
    if (req->tokens[req->index].type == NULL)
        goto error;

    /* Try the next token. */
    request_send(req);
    return;

error:
    req->cb(req->data, retval, otp_response_fail, NULL);
    request_free(req);
}

static void
request_send(request *req)
{
    krb5_error_code retval;
    token *tok = &req->tokens[req->index];
    const token_type *t = tok->type;

    retval = krad_attrset_add(req->attrs, krad_attr_name2num("User-Name"),
                              &tok->username);
    if (retval != 0)
        goto error;

    retval = krad_client_send(req->state->radius,
                              krad_code_name2num("Access-Request"), req->attrs,
                              t->server, t->secret, t->timeout, t->retries,
                              callback, req);
    krad_attrset_del(req->attrs, krad_attr_name2num("User-Name"), 0);
    if (retval != 0)
        goto error;

    return;

error:
    req->cb(req->data, retval, otp_response_fail, NULL);
    request_free(req);
}

void
otp_state_verify(otp_state *state, verto_ctx *ctx, krb5_const_principal princ,
                 const char *config, const krb5_pa_otp_req *req,
                 otp_cb cb, void *data)
{
    krb5_error_code retval;
    request *rqst = NULL;
    char *name;

    if (state->radius == NULL) {
        retval = krad_client_new(state->ctx, ctx, &state->radius);
        if (retval != 0)
            goto error;
    }

    rqst = calloc(1, sizeof(request));
    if (rqst == NULL) {
        (*cb)(data, ENOMEM, otp_response_fail, NULL);
        return;
    }
    rqst->state = state;
    rqst->data = data;
    rqst->cb = cb;

    retval = krad_attrset_copy(state->attrs, &rqst->attrs);
    if (retval != 0)
        goto error;

    retval = krad_attrset_add(rqst->attrs, krad_attr_name2num("User-Password"),
                              &req->otp_value);
    if (retval != 0)
        goto error;

    retval = tokens_decode(state->ctx, princ, state->types, config,
                           &rqst->tokens);
    if (retval != 0) {
        if (krb5_unparse_name(state->ctx, princ, &name) == 0) {
            com_err("otp", retval,
                    "Can't decode otp config string for principal '%s'", name);
            krb5_free_unparsed_name(state->ctx, name);
        }
        goto error;
    }

    request_send(rqst);
    return;

error:
    (*cb)(data, retval, otp_response_fail, NULL);
    request_free(rqst);
}
