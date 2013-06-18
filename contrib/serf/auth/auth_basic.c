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

/*** Basic authentication ***/

#include <serf.h>
#include <serf_private.h>
#include <auth/auth.h>

#include <apr.h>
#include <apr_base64.h>
#include <apr_strings.h>

typedef struct basic_authn_info_t {
    const char *header;
    const char *value;
} basic_authn_info_t;

apr_status_t
serf__handle_basic_auth(int code,
                        serf_request_t *request,
                        serf_bucket_t *response,
                        const char *auth_hdr,
                        const char *auth_attr,
                        void *baton,
                        apr_pool_t *pool)
{
    const char *tmp;
    apr_size_t tmp_len;
    serf_connection_t *conn = request->conn;
    serf_context_t *ctx = conn->ctx;
    serf__authn_info_t *authn_info = (code == 401) ? &ctx->authn_info :
        &ctx->proxy_authn_info;
    basic_authn_info_t *basic_info = authn_info->baton;
    apr_status_t status;
    apr_pool_t *cred_pool;
    char *username, *password;

    /* Can't do Basic authentication if there's no callback to get
       username & password. */
    if (!ctx->cred_cb) {
        return SERF_ERROR_AUTHN_FAILED;
    }

    if (!authn_info->realm) {
        char *realm_name = NULL;
        const char *eq = strchr(auth_attr, '=');

        if (eq && strncasecmp(auth_attr, "realm", 5) == 0) {
            realm_name = apr_pstrdup(pool, eq + 1);
            if (realm_name[0] == '\"') {
                apr_size_t realm_len;

                realm_len = strlen(realm_name);
                if (realm_name[realm_len - 1] == '\"') {
                    realm_name[realm_len - 1] = '\0';
                    realm_name++;
                }
            }
        }

        if (!realm_name) {
            return SERF_ERROR_AUTHN_MISSING_ATTRIBUTE;
        }

        authn_info->realm = apr_psprintf(conn->pool, "<%s://%s:%d> %s",
                                         conn->host_info.scheme,
                                         conn->host_info.hostname,
                                         conn->host_info.port,
                                         realm_name);
    }

    /* Ask the application for credentials */
    apr_pool_create(&cred_pool, pool);
    status = (*ctx->cred_cb)(&username, &password, request, baton,
                             code, authn_info->scheme->name,
                             authn_info->realm, cred_pool);
    if (status) {
        apr_pool_destroy(cred_pool);
        return status;
    }

    tmp = apr_pstrcat(conn->pool, username, ":", password, NULL);
    tmp_len = strlen(tmp);
    apr_pool_destroy(cred_pool);

    serf__encode_auth_header(&basic_info->value,
                             authn_info->scheme->name,
                             tmp, tmp_len, pool);
    basic_info->header = (code == 401) ? "Authorization" : "Proxy-Authorization";

    return APR_SUCCESS;
}

/* For Basic authentication we expect all authn info to be the same for all
   connections in the context (same realm, username, password). Therefore we
   can keep the header value in the context instead of per connection. */
apr_status_t
serf__init_basic(int code,
                 serf_context_t *ctx,
                 apr_pool_t *pool)
{
    if (code == 401) {
        ctx->authn_info.baton = apr_pcalloc(pool, sizeof(basic_authn_info_t));
    } else {
        ctx->proxy_authn_info.baton = apr_pcalloc(pool, sizeof(basic_authn_info_t));
    }

    return APR_SUCCESS;
}

apr_status_t
serf__init_basic_connection(int code,
                            serf_connection_t *conn,
                            apr_pool_t *pool)
{
    return APR_SUCCESS;
}

apr_status_t
serf__setup_request_basic_auth(peer_t peer,
                               int code,
                               serf_connection_t *conn,
                               serf_request_t *request,
                               const char *method,
                               const char *uri,
                               serf_bucket_t *hdrs_bkt)
{
    serf_context_t *ctx = conn->ctx;
    basic_authn_info_t *authn_info;

    if (peer == HOST) {
        authn_info = ctx->authn_info.baton;
    } else {
        authn_info = ctx->proxy_authn_info.baton;
    }

    if (authn_info && authn_info->header && authn_info->value) {
        serf_bucket_headers_setn(hdrs_bkt, authn_info->header,
                                 authn_info->value);
        return APR_SUCCESS;
    }

    return SERF_ERROR_AUTHN_FAILED;
}
