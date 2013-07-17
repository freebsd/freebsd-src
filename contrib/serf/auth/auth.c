/* Copyright 2009 Justin Erenkrantz and Greg Stein
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "serf.h"
#include "serf_private.h"
#include "auth.h"

#include <apr.h>
#include <apr_base64.h>
#include <apr_strings.h>

static apr_status_t
default_auth_response_handler(peer_t peer,
                              int code,
                              serf_connection_t *conn,
                              serf_request_t *request,
                              serf_bucket_t *response,
                              apr_pool_t *pool)
{
    return APR_SUCCESS;
}

static const serf__authn_scheme_t serf_authn_schemes[] = {
    {
        401,
        "Basic",
        SERF_AUTHN_BASIC,
        serf__init_basic,
        serf__init_basic_connection,
        serf__handle_basic_auth,
        serf__setup_request_basic_auth,
        default_auth_response_handler,
    },
    {
          407,
          "Basic",
          SERF_AUTHN_BASIC,
          serf__init_basic,
          serf__init_basic_connection,
          serf__handle_basic_auth,
          serf__setup_request_basic_auth,
          default_auth_response_handler,
    },
    {
        401,
        "Digest",
        SERF_AUTHN_DIGEST,
        serf__init_digest,
        serf__init_digest_connection,
        serf__handle_digest_auth,
        serf__setup_request_digest_auth,
        serf__validate_response_digest_auth,
    },
    {
        407,
        "Digest",
        SERF_AUTHN_DIGEST,
        serf__init_digest,
        serf__init_digest_connection,
        serf__handle_digest_auth,
        serf__setup_request_digest_auth,
        serf__validate_response_digest_auth,
    },
#ifdef SERF_HAVE_KERB
    {
        401,
        "Negotiate",
        SERF_AUTHN_NEGOTIATE,
        serf__init_kerb,
        serf__init_kerb_connection,
        serf__handle_kerb_auth,
        serf__setup_request_kerb_auth,
        serf__validate_response_kerb_auth,
    },
    {
        407,
        "Negotiate",
        SERF_AUTHN_NEGOTIATE,
        serf__init_kerb,
        serf__init_kerb_connection,
        serf__handle_kerb_auth,
        serf__setup_request_kerb_auth,
        serf__validate_response_kerb_auth,
    },
#endif
    /* ADD NEW AUTHENTICATION IMPLEMENTATIONS HERE (as they're written) */

    /* sentinel */
    { 0 }
};


/**
 * Baton passed to the response header callback function
 */
typedef struct {
    int code;
    apr_status_t status;
    const char *header;
    serf_request_t *request;
    serf_bucket_t *response;
    void *baton;
    apr_pool_t *pool;
    const serf__authn_scheme_t *scheme;
    const char *last_scheme_name;
} auth_baton_t;

/* Reads and discards all bytes in the response body. */
static apr_status_t discard_body(serf_bucket_t *response)
{
    apr_status_t status;
    const char *data;
    apr_size_t len;

    while (1) {
        status = serf_bucket_read(response, SERF_READ_ALL_AVAIL, &data, &len);

        if (status) {
            return status;
        }

        /* feed me */
    }
}

/**
 * handle_auth_header is called for each header in the response. It filters
 * out the Authenticate headers (WWW or Proxy depending on what's needed) and
 * tries to find a matching scheme handler.
 *
 * Returns a non-0 value of a matching handler was found.
 */
static int handle_auth_header(void *baton,
                              const char *key,
                              const char *header)
{
    auth_baton_t *ab = baton;
    int scheme_found = FALSE;
    const char *auth_name;
    const char *auth_attr;
    const serf__authn_scheme_t *scheme = NULL;
    serf_connection_t *conn = ab->request->conn;
    serf_context_t *ctx = conn->ctx;

    /* We're only interested in xxxx-Authenticate headers. */
    if (strcmp(key, ab->header) != 0)
        return 0;

    /* Extract the authentication scheme name, and prepare for reading
       the attributes.  */
    auth_attr = strchr(header, ' ');
    if (auth_attr) {
        auth_name = apr_pstrmemdup(ab->pool, header, auth_attr - header);
        ++auth_attr;
    }
    else
        auth_name = header;

    ab->last_scheme_name = auth_name;

    /* Find the matching authentication handler.
       Note that we don't reuse the auth scheme stored in the context,
       as that may have changed. (ex. fallback from ntlm to basic.) */
    for (scheme = serf_authn_schemes; scheme->code != 0; ++scheme) {
        if (! (ab->code == scheme->code &&
               ctx->authn_types & scheme->type))
            continue;

        serf__log_skt(AUTH_VERBOSE, __FILE__, conn->skt,
                      "Client supports: %s\n", scheme->name);
        if (strcmp(auth_name, scheme->name) == 0) {
            serf__auth_handler_func_t handler = scheme->handle_func;
            apr_status_t status = 0;

            serf__log_skt(AUTH_VERBOSE, __FILE__, conn->skt,
                          "... matched: %s\n", scheme->name);
            /* If this is the first time we use this scheme on this connection,
               make sure to initialize the authentication handler first. */
            if (ab->code == 401 && ctx->authn_info.scheme != scheme) {
                status = scheme->init_ctx_func(ab->code, ctx, ctx->pool);
                if (!status) {
                    status = scheme->init_conn_func(ab->code, conn, conn->pool);

                    if (!status)
                        ctx->authn_info.scheme = scheme;
                    else
                        ctx->authn_info.scheme = NULL;
                }
            }
            else if (ab->code == 407 && ctx->proxy_authn_info.scheme != scheme) {
                status = scheme->init_ctx_func(ab->code, ctx, ctx->pool);
                if (!status) {
                    status = scheme->init_conn_func(ab->code, conn, conn->pool);

                    if (!status)
                        ctx->proxy_authn_info.scheme = scheme;
                    else
                        ctx->proxy_authn_info.scheme = NULL;
                }
            }

            if (!status) {
                scheme_found = TRUE;
                ab->scheme = scheme;
                status = handler(ab->code, ab->request, ab->response,
                                 header, auth_attr, ab->baton, ctx->pool);
            }

            /* If the authentication fails, cache the error for now. Try the
               next available scheme. If there's none raise the error. */
            if (status) {
                scheme_found = FALSE;
                scheme = NULL;
            }
            /* Let the caller now if the authentication setup was succesful
               or not. */
            ab->status = status;

            break;
        }
    }

    /* If a matching scheme handler was found, we can stop iterating
       over the response headers - so return a non-0 value. */
    return scheme_found;
}

/* Dispatch authentication handling. This function matches the possible
   authentication mechanisms with those available. Server and proxy
   authentication are evaluated separately. */
static apr_status_t dispatch_auth(int code,
                                  serf_request_t *request,
                                  serf_bucket_t *response,
                                  void *baton,
                                  apr_pool_t *pool)
{
    serf_bucket_t *hdrs;

    if (code == 401 || code == 407) {
        auth_baton_t ab = { 0 };
        const char *auth_hdr;

        ab.code = code;
        ab.status = APR_SUCCESS;
        ab.request = request;
        ab.response = response;
        ab.baton = baton;
        ab.pool = pool;

        /* Before iterating over all authn headers, check if there are any. */
        if (code == 401)
            ab.header = "WWW-Authenticate";
        else
            ab.header = "Proxy-Authenticate";

        hdrs = serf_bucket_response_get_headers(response);
        auth_hdr = serf_bucket_headers_get(hdrs, ab.header);

        if (!auth_hdr) {
            return SERF_ERROR_AUTHN_FAILED;
        }
        serf__log_skt(AUTH_VERBOSE, __FILE__, request->conn->skt,
                      "%s authz required. Response header(s): %s\n",
                      code == 401 ? "Server" : "Proxy", auth_hdr);

        /* Iterate over all headers. Try to find a matching authentication scheme
           handler.

           Note: it is possible to have multiple Authentication: headers. We do
           not want to combine them (per normal header combination rules) as that
           would make it hard to parse. Instead, we want to individually parse
           and handle each header in the response, looking for one that we can
           work with.
        */
        serf_bucket_headers_do(hdrs,
                               handle_auth_header,
                               &ab);
        if (ab.status != APR_SUCCESS)
            return ab.status;

        if (!ab.scheme || ab.scheme->name == NULL) {
            /* No matching authentication found. */
            return SERF_ERROR_AUTHN_NOT_SUPPORTED;
        }
    }

    return APR_SUCCESS;
}

/* Read the headers of the response and try the available
   handlers if authentication or validation is needed. */
apr_status_t serf__handle_auth_response(int *consumed_response,
                                        serf_request_t *request,
                                        serf_bucket_t *response,
                                        void *baton,
                                        apr_pool_t *pool)
{
    apr_status_t status;
    serf_status_line sl;

    *consumed_response = 0;

    /* TODO: the response bucket was created by the application, not at all
       guaranteed that this is of type response_bucket!! */
    status = serf_bucket_response_status(response, &sl);
    if (SERF_BUCKET_READ_ERROR(status)) {
        return status;
    }
    if (!sl.version && (APR_STATUS_IS_EOF(status) ||
                        APR_STATUS_IS_EAGAIN(status))) {
        return status;
    }

    status = serf_bucket_response_wait_for_headers(response);
    if (status) {
        if (!APR_STATUS_IS_EOF(status)) {
            return status;
        }

        /* If status is APR_EOF, there were no headers to read.
           This can be ok in some situations, and it definitely
           means there's no authentication requested now. */
        return APR_SUCCESS;
    }

    if (sl.code == 401 || sl.code == 407) {
        /* Authentication requested. */

        /* Don't bother handling the authentication request if the response
           wasn't received completely yet. Serf will call serf__handle_auth_response
           again when more data is received. */
        status = discard_body(response);
        *consumed_response = 1;
        
        /* Discard all response body before processing authentication. */
        if (!APR_STATUS_IS_EOF(status)) {
            return status;
        }

        status = dispatch_auth(sl.code, request, response, baton, pool);
        if (status != APR_SUCCESS) {
            return status;
        }

        /* Requeue the request with the necessary auth headers. */
        /* ### Application doesn't know about this request! */
        serf_connection_priority_request_create(request->conn,
                                                request->setup,
                                                request->setup_baton);

        return APR_EOF;
    } else {
        /* Validate the response authn headers if needed. */
        serf__validate_response_func_t validate_resp;
        serf_connection_t *conn = request->conn;
        serf_context_t *ctx = conn->ctx;
        apr_status_t resp_status = APR_SUCCESS;
        
        if (ctx->authn_info.scheme) {
            validate_resp = ctx->authn_info.scheme->validate_response_func;
            resp_status = validate_resp(HOST, sl.code, conn, request, response,
                                        pool);
        }
        if (!resp_status && ctx->proxy_authn_info.scheme) {
            validate_resp = ctx->proxy_authn_info.scheme->validate_response_func;
            resp_status = validate_resp(PROXY, sl.code, conn, request, response,
                                        pool);
        }
        if (resp_status) {
            /* If there was an error in the final step of the authentication,
               consider the reponse body as invalid and discard it. */
            status = discard_body(response);
            *consumed_response = 1;
            if (!APR_STATUS_IS_EOF(status)) {
                return status;
            }
            /* The whole body was discarded, now return our error. */
            return resp_status;
        }
    }

    return APR_SUCCESS;
}

/**
 * base64 encode the authentication data and build an authentication
 * header in this format:
 * [SCHEME] [BASE64 of auth DATA]
 */
void serf__encode_auth_header(const char **header,
                              const char *scheme,
                              const char *data, apr_size_t data_len,
                              apr_pool_t *pool)
{
    apr_size_t encoded_len, scheme_len;
    char *ptr;

    encoded_len = apr_base64_encode_len(data_len);
    scheme_len = strlen(scheme);

    ptr = apr_palloc(pool, encoded_len + scheme_len + 1);
    *header = ptr;

    apr_cpystrn(ptr, scheme, scheme_len + 1);
    ptr += scheme_len;
    *ptr++ = ' ';

    apr_base64_encode(ptr, data, data_len);
}
