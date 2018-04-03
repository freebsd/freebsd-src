/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/responder.c - Test harness for responder callbacks and the like. */
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
 *  A helper for testing PKINIT and responder callbacks.
 *
 *  This test helper takes multiple options and one argument.
 *
 *  responder [options] principal
 *   -X preauth_option  -> preauth options, as for kinit
 *   -x challenge       -> expected responder challenge, of the form
 *                         "question=challenge"
 *   -r response        -> provide a reponder answer, in the form
 *                         "question=answer"
 *   -c                 -> print the pkinit challenge
 *   -p identity=pin    -> provide a pkinit answer, in the form "identity=pin"
 *   -o index=value:pin -> provide an OTP answer, in the form "index=value:pin"
 *   principal          -> client principal name
 *
 *  If the responder callback isn't called, that's treated as an error.
 *
 *  If an expected responder challenge is specified, when the responder
 *  callback is called, the challenge associated with the specified question is
 *  compared against the specified value.  If the value provided to the
 *  callback doesn't parse as JSON, a literal string compare is performed,
 *  otherwise both values are parsed as JSON and then re-encoded before
 *  comparison.  In either case, the comparison must succeed.
 *
 *  Any missing data or mismatches are treated as errors.
 */

#include <k5-platform.h>
#include <k5-json.h>
#include <sys/types.h>
#include <unistd.h>
#include <krb5.h>

struct responder_data {
    krb5_boolean called;
    krb5_boolean print_pkinit_challenge;
    const char *challenge;
    const char *response;
    const char *pkinit_answer;
    const char *otp_answer;
};

static krb5_error_code
responder(krb5_context ctx, void *rawdata, krb5_responder_context rctx)
{
    krb5_error_code err;
    char *key, *value, *pin, *encoded1, *encoded2;
    const char *challenge;
    k5_json_value decoded1, decoded2;
    k5_json_object ids;
    k5_json_number val;
    krb5_int32 token_flags;
    struct responder_data *data = rawdata;
    krb5_responder_pkinit_challenge *chl;
    krb5_responder_otp_challenge *ochl;
    unsigned int i, n;

    data->called = TRUE;

    /* Check that a particular challenge has the specified expected value. */
    if (data->challenge != NULL) {
        /* Separate the challenge name and its expected value. */
        key = strdup(data->challenge);
        if (key == NULL)
            exit(ENOMEM);
        value = key + strcspn(key, "=");
        if (*value != '\0')
            *value++ = '\0';
        /* Read the challenge. */
        challenge = krb5_responder_get_challenge(ctx, rctx, key);
        err = k5_json_decode(value, &decoded1);
        /* Check for "no challenge". */
        if (challenge == NULL && *value == '\0') {
            fprintf(stderr, "OK: (no challenge) == (no challenge)\n");
        } else if (err != 0) {
            /* It's not JSON, so assume we're just after a string compare. */
            if (strcmp(challenge, value) == 0) {
                fprintf(stderr, "OK: \"%s\" == \"%s\"\n", challenge, value);
            } else {
                fprintf(stderr, "ERROR: \"%s\" != \"%s\"\n", challenge, value);
                exit(1);
            }
        } else {
            /* Assume we're after a JSON compare - decode the actual value. */
            err = k5_json_decode(challenge, &decoded2);
            if (err != 0) {
                fprintf(stderr, "error decoding \"%s\"\n", challenge);
                exit(1);
            }
            /* Re-encode the expected challenge and the actual challenge... */
            err = k5_json_encode(decoded1, &encoded1);
            if (err != 0) {
                fprintf(stderr, "error encoding json data\n");
                exit(1);
            }
            err = k5_json_encode(decoded2, &encoded2);
            if (err != 0) {
                fprintf(stderr, "error encoding json data\n");
                exit(1);
            }
            k5_json_release(decoded1);
            k5_json_release(decoded2);
            /* ... and see if they look the same. */
            if (strcmp(encoded1, encoded2) == 0) {
                fprintf(stderr, "OK: \"%s\" == \"%s\"\n", encoded1, encoded2);
            } else {
                fprintf(stderr, "ERROR: \"%s\" != \"%s\"\n", encoded1,
                        encoded2);
                exit(1);
            }
            free(encoded1);
            free(encoded2);
        }
        free(key);
    }

    /* Provide a particular response for a challenge. */
    if (data->response != NULL) {
        /* Separate the challenge and its data content... */
        key = strdup(data->response);
        if (key == NULL)
            exit(ENOMEM);
        value = key + strcspn(key, "=");
        if (*value != '\0')
            *value++ = '\0';
        /* ... and pass it in. */
        err = krb5_responder_set_answer(ctx, rctx, key, value);
        if (err != 0) {
            fprintf(stderr, "error setting response\n");
            exit(1);
        }
        free(key);
    }

    if (data->print_pkinit_challenge) {
        /* Read the PKINIT challenge, formatted as a structure. */
        err = krb5_responder_pkinit_get_challenge(ctx, rctx, &chl);
        if (err != 0) {
            fprintf(stderr, "error getting pkinit challenge\n");
            exit(1);
        }
        if (chl != NULL) {
            for (n = 0; chl->identities[n] != NULL; n++)
                continue;
            for (i = 0; chl->identities[i] != NULL; i++) {
                if (chl->identities[i]->token_flags != -1) {
                    printf("identity %u/%u: %s (flags=0x%lx)\n", i + 1, n,
                           chl->identities[i]->identity,
                           (long)chl->identities[i]->token_flags);
                } else {
                    printf("identity %u/%u: %s\n", i + 1, n,
                           chl->identities[i]->identity);
                }
            }
        }
        krb5_responder_pkinit_challenge_free(ctx, rctx, chl);
    }

    /* Provide a particular response for the PKINIT challenge. */
    if (data->pkinit_answer != NULL) {
        /* Read the PKINIT challenge, formatted as a structure. */
        err = krb5_responder_pkinit_get_challenge(ctx, rctx, &chl);
        if (err != 0) {
            fprintf(stderr, "error getting pkinit challenge\n");
            exit(1);
        }
        /*
         * In case order matters, if the identity starts with "FILE:", exercise
         * the set_answer function, with the real answer second.
         */
        if (chl != NULL &&
            chl->identities != NULL &&
            chl->identities[0] != NULL) {
            if (strncmp(chl->identities[0]->identity, "FILE:", 5) == 0)
                krb5_responder_pkinit_set_answer(ctx, rctx, "foo", "bar");
        }
        /* Provide the real answer. */
        key = strdup(data->pkinit_answer);
        if (key == NULL)
            exit(ENOMEM);
        value = strrchr(key, '=');
        if (value != NULL)
            *value++ = '\0';
        else
            value = "";
        err = krb5_responder_pkinit_set_answer(ctx, rctx, key, value);
        if (err != 0) {
            fprintf(stderr, "error setting response\n");
            exit(1);
        }
        free(key);
        /*
         * In case order matters, if the identity starts with "PKCS12:",
         * exercise the set_answer function, with the real answer first.
         */
        if (chl != NULL &&
            chl->identities != NULL &&
            chl->identities[0] != NULL) {
            if (strncmp(chl->identities[0]->identity, "PKCS12:", 7) == 0)
                krb5_responder_pkinit_set_answer(ctx, rctx, "foo", "bar");
        }
        krb5_responder_pkinit_challenge_free(ctx, rctx, chl);
    }

    /*
     * Something we always check: read the PKINIT challenge, both as a
     * structure and in JSON form, reconstruct the JSON form from the
     * structure's contents, and check that they're the same.
     */
    challenge = krb5_responder_get_challenge(ctx, rctx,
                                             KRB5_RESPONDER_QUESTION_PKINIT);
    if (challenge != NULL) {
        krb5_responder_pkinit_get_challenge(ctx, rctx, &chl);
        if (chl == NULL) {
            fprintf(stderr, "pkinit raw challenge set, "
                    "but structure is NULL\n");
            exit(1);
        }
        if (k5_json_object_create(&ids) != 0) {
            fprintf(stderr, "error creating json objects\n");
            exit(1);
        }
        for (i = 0; chl->identities[i] != NULL; i++) {
            token_flags = chl->identities[i]->token_flags;
            if (k5_json_number_create(token_flags, &val) != 0) {
                fprintf(stderr, "error creating json number\n");
                exit(1);
            }
            if (k5_json_object_set(ids, chl->identities[i]->identity,
                                   val) != 0) {
                fprintf(stderr, "error adding json number to object\n");
                exit(1);
            }
            k5_json_release(val);
        }
        /* Encode the structure... */
        err = k5_json_encode(ids, &encoded1);
        if (err != 0) {
            fprintf(stderr, "error encoding json data\n");
            exit(1);
        }
        k5_json_release(ids);
        /* ... and see if they look the same. */
        if (strcmp(encoded1, challenge) != 0) {
            fprintf(stderr, "\"%s\" != \"%s\"\n", encoded1, challenge);
            exit(1);
        }
        krb5_responder_pkinit_challenge_free(ctx, rctx, chl);
        free(encoded1);
    }

    /* Provide a particular response for an OTP challenge. */
    if (data->otp_answer != NULL) {
        if (krb5_responder_otp_get_challenge(ctx, rctx, &ochl) == 0) {
            key = strchr(data->otp_answer, '=');
            if (key != NULL) {
                /* Make a copy of the answer that we can chop up. */
                key = strdup(data->otp_answer);
                if (key == NULL)
                    return ENOMEM;
                /* Isolate the ti value. */
                value = strchr(key, '=');
                *value++ = '\0';
                n = atoi(key);
                /* Break the value and PIN apart. */
                pin = strchr(value, ':');
                if (pin != NULL)
                    *pin++ = '\0';
                err = krb5_responder_otp_set_answer(ctx, rctx, n, value, pin);
                if (err != 0) {
                    fprintf(stderr, "error setting response\n");
                    exit(1);
                }
                free(key);
            }
            krb5_responder_otp_challenge_free(ctx, rctx, ochl);
        }
    }

    return 0;
}

int
main(int argc, char **argv)
{
    krb5_context context;
    krb5_ccache ccache;
    krb5_get_init_creds_opt *opts;
    krb5_principal principal;
    krb5_creds creds;
    krb5_error_code err;
    const char *errmsg;
    char *opt, *val;
    struct responder_data response;
    int c;

    err = krb5_init_context(&context);
    if (err != 0) {
        fprintf(stderr, "error starting Kerberos: %s\n", error_message(err));
        return err;
    }
    err = krb5_get_init_creds_opt_alloc(context, &opts);
    if (err != 0) {
        fprintf(stderr, "error initializing options: %s\n",
                error_message(err));
        return err;
    }
    err = krb5_cc_default(context, &ccache);
    if (err != 0) {
        fprintf(stderr, "error resolving default ccache: %s\n",
                error_message(err));
        return err;
    }
    err = krb5_get_init_creds_opt_set_out_ccache(context, opts, ccache);
    if (err != 0) {
        fprintf(stderr, "error setting output ccache: %s\n",
                error_message(err));
        return err;
    }

    memset(&response, 0, sizeof(response));
    while ((c = getopt(argc, argv, "X:x:cr:p:")) != -1) {
        switch (c) {
        case 'X':
            /* Like kinit, set a generic preauth option. */
            opt = strdup(optarg);
            val = opt + strcspn(opt, "=");
            if (*val != '\0') {
                *val++ = '\0';
            }
            err = krb5_get_init_creds_opt_set_pa(context, opts, opt, val);
            if (err != 0) {
                fprintf(stderr, "error setting option \"%s\": %s\n", opt,
                        error_message(err));
                return err;
            }
            free(opt);
            break;
        case 'x':
            /* Check that a particular question has a specific challenge. */
            response.challenge = optarg;
            break;
        case 'c':
            /* Note that we want a dump of the PKINIT challenge structure. */
            response.print_pkinit_challenge = TRUE;
            break;
        case 'r':
            /* Set a verbatim response for a verbatim challenge. */
            response.response = optarg;
            break;
        case 'p':
            /* Set a PKINIT answer for a specific PKINIT identity. */
            response.pkinit_answer = optarg;
            break;
        case 'o':
            /* Set an OTP answer for a specific OTP tokeninfo. */
            response.otp_answer = optarg;
            break;
        }
    }

    if (argc > optind) {
        err = krb5_parse_name(context, argv[optind], &principal);
        if (err != 0) {
            fprintf(stderr, "error parsing name \"%s\": %s", argv[optind],
                    error_message(err));
            return err;
        }
    } else {
        fprintf(stderr, "error: no principal name provided\n");
        return -1;
    }

    err = krb5_get_init_creds_opt_set_responder(context, opts,
                                                responder, &response);
    if (err != 0) {
        fprintf(stderr, "error setting responder: %s\n", error_message(err));
        return err;
    }
    memset(&creds, 0, sizeof(creds));
    err = krb5_get_init_creds_password(context, &creds, principal, NULL,
                                       NULL, NULL, 0, NULL, opts);
    if (err == 0)
        krb5_free_cred_contents(context, &creds);
    krb5_free_principal(context, principal);
    krb5_get_init_creds_opt_free(context, opts);
    krb5_cc_close(context, ccache);

    if (!response.called) {
        fprintf(stderr, "error: responder callback wasn't called\n");
        err = 1;
    } else if (err) {
        errmsg = krb5_get_error_message(context, err);
        fprintf(stderr, "error: krb5_get_init_creds_password failed: %s\n",
                errmsg);
        krb5_free_error_message(context, errmsg);
        err = 2;
    }
    krb5_free_context(context);
    return err;
}
