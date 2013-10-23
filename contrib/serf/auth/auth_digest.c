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

/*** Digest authentication ***/

#include <serf.h>
#include <serf_private.h>
#include <auth/auth.h>

#include <apr.h>
#include <apr_base64.h>
#include <apr_strings.h>
#include <apr_uuid.h>
#include <apr_md5.h>

/** Digest authentication, implements RFC 2617. **/

/* Stores the context information related to Digest authentication.
   The context is per connection. */
typedef struct digest_authn_info_t {
    /* nonce-count for digest authentication */
    unsigned int digest_nc;

    const char *header;

    const char *ha1;

    const char *realm;
    const char *cnonce;
    const char *nonce;
    const char *opaque;
    const char *algorithm;
    const char *qop;
    const char *username;

    apr_pool_t *pool;
} digest_authn_info_t;

static char
int_to_hex(int v)
{
    return (v < 10) ? '0' + v : 'a' + (v - 10);
}

/**
 * Convert a string if ASCII characters HASHVAL to its hexadecimal
 * representation.
 *
 * The returned string will be allocated in the POOL and be null-terminated.
 */
static const char *
hex_encode(const unsigned char *hashval,
           apr_pool_t *pool)
{
    int i;
    char *hexval = apr_palloc(pool, (APR_MD5_DIGESTSIZE * 2) + 1);
    for (i = 0; i < APR_MD5_DIGESTSIZE; i++) {
        hexval[2 * i] = int_to_hex((hashval[i] >> 4) & 0xf);
        hexval[2 * i + 1] = int_to_hex(hashval[i] & 0xf);
    }
    hexval[APR_MD5_DIGESTSIZE * 2] = '\0';
    return hexval;
}

/**
 * Returns a 36-byte long string of random characters.
 * UUIDs are formatted as: 00112233-4455-6677-8899-AABBCCDDEEFF.
 *
 * The returned string will be allocated in the POOL and be null-terminated.
 */
static const char *
random_cnonce(apr_pool_t *pool)
{
    apr_uuid_t uuid;
    char *buf = apr_palloc(pool, APR_UUID_FORMATTED_LENGTH + 1);

    apr_uuid_get(&uuid);
    apr_uuid_format(buf, &uuid);

    return hex_encode((unsigned char*)buf, pool);
}

static const char *
build_digest_ha1(const char *username,
                 const char *password,
                 const char *realm_name,
                 apr_pool_t *pool)
{
    const char *tmp;
    unsigned char ha1[APR_MD5_DIGESTSIZE];
    apr_status_t status;

    /* calculate ha1:
       MD5 hash of the combined user name, authentication realm and password */
    tmp = apr_psprintf(pool, "%s:%s:%s",
                       username,
                       realm_name,
                       password);
    status = apr_md5(ha1, tmp, strlen(tmp));

    return hex_encode(ha1, pool);
}

static const char *
build_digest_ha2(const char *uri,
                 const char *method,
                 const char *qop,
                 apr_pool_t *pool)
{
    if (!qop || strcmp(qop, "auth") == 0) {
        const char *tmp;
        unsigned char ha2[APR_MD5_DIGESTSIZE];
        apr_status_t status;

        /* calculate ha2:
           MD5 hash of the combined method and URI */
        tmp = apr_psprintf(pool, "%s:%s",
                           method,
                           uri);
        status = apr_md5(ha2, tmp, strlen(tmp));

        return hex_encode(ha2, pool);
    } else {
        /* TODO: auth-int isn't supported! */
    }

    return NULL;
}

static const char *
build_auth_header(digest_authn_info_t *digest_info,
                  const char *path,
                  const char *method,
                  apr_pool_t *pool)
{
    char *hdr;
    const char *ha2;
    const char *response;
    unsigned char response_hdr[APR_MD5_DIGESTSIZE];
    const char *response_hdr_hex;
    apr_status_t status;

    ha2 = build_digest_ha2(path, method, digest_info->qop, pool);

    hdr = apr_psprintf(pool,
                       "Digest realm=\"%s\","
                       " username=\"%s\","
                       " nonce=\"%s\","
                       " uri=\"%s\"",
                       digest_info->realm, digest_info->username,
                       digest_info->nonce,
                       path);

    if (digest_info->qop) {
        if (! digest_info->cnonce)
            digest_info->cnonce = random_cnonce(digest_info->pool);

        hdr = apr_psprintf(pool, "%s, nc=%08x, cnonce=\"%s\", qop=\"%s\"",
                           hdr,
                           digest_info->digest_nc,
                           digest_info->cnonce,
                           digest_info->qop);

        /* Build the response header:
           MD5 hash of the combined HA1 result, server nonce (nonce),
           request counter (nc), client nonce (cnonce),
           quality of protection code (qop) and HA2 result. */
        response = apr_psprintf(pool, "%s:%s:%08x:%s:%s:%s",
                                digest_info->ha1, digest_info->nonce,
                                digest_info->digest_nc,
                                digest_info->cnonce, digest_info->qop, ha2);
    } else {
        /* Build the response header:
           MD5 hash of the combined HA1 result, server nonce (nonce)
           and HA2 result. */
        response = apr_psprintf(pool, "%s:%s:%s",
                                digest_info->ha1, digest_info->nonce, ha2);
    }

    status = apr_md5(response_hdr, response, strlen(response));
    response_hdr_hex = hex_encode(response_hdr, pool);

    hdr = apr_psprintf(pool, "%s, response=\"%s\"", hdr, response_hdr_hex);

    if (digest_info->opaque) {
        hdr = apr_psprintf(pool, "%s, opaque=\"%s\"", hdr,
                           digest_info->opaque);
    }
    if (digest_info->algorithm) {
        hdr = apr_psprintf(pool, "%s, algorithm=\"%s\"", hdr,
                           digest_info->algorithm);
    }

    return hdr;
}

apr_status_t
serf__handle_digest_auth(int code,
                         serf_request_t *request,
                         serf_bucket_t *response,
                         const char *auth_hdr,
                         const char *auth_attr,
                         void *baton,
                         apr_pool_t *pool)
{
    char *attrs;
    char *nextkv;
    const char *realm_name = NULL;
    const char *nonce = NULL;
    const char *algorithm = NULL;
    const char *qop = NULL;
    const char *opaque = NULL;
    const char *key;
    serf_connection_t *conn = request->conn;
    serf_context_t *ctx = conn->ctx;
    serf__authn_info_t *authn_info = (code == 401) ? &ctx->authn_info :
        &ctx->proxy_authn_info;
    digest_authn_info_t *digest_info = (code == 401) ? conn->authn_baton :
        conn->proxy_authn_baton;
    apr_status_t status;
    apr_pool_t *cred_pool;
    char *username, *password;

    /* Can't do Digest authentication if there's no callback to get
       username & password. */
    if (!ctx->cred_cb) {
        return SERF_ERROR_AUTHN_FAILED;
    }

    /* Need a copy cuz we're going to write NUL characters into the string.  */
    attrs = apr_pstrdup(pool, auth_attr);

    /* We're expecting a list of key=value pairs, separated by a comma.
       Ex. realm="SVN Digest",
       nonce="f+zTl/leBAA=e371bd3070adfb47b21f5fc64ad8cc21adc371a5",
       algorithm=MD5, qop="auth" */
    for ( ; (key = apr_strtok(attrs, ",", &nextkv)) != NULL; attrs = NULL) {
        char *val;

        val = strchr(key, '=');
        if (val == NULL)
            continue;
        *val++ = '\0';

        /* skip leading spaces */
        while (*key && *key == ' ')
            key++;

        /* If the value is quoted, then remove the quotes.  */
        if (*val == '"') {
            apr_size_t last = strlen(val) - 1;

            if (val[last] == '"') {
                val[last] = '\0';
                val++;
            }
        }

        if (strcmp(key, "realm") == 0)
            realm_name = val;
        else if (strcmp(key, "nonce") == 0)
            nonce = val;
        else if (strcmp(key, "algorithm") == 0)
            algorithm = val;
        else if (strcmp(key, "qop") == 0)
            qop = val;
        else if (strcmp(key, "opaque") == 0)
            opaque = val;

        /* Ignore all unsupported attributes. */
    }

    if (!realm_name) {
        return SERF_ERROR_AUTHN_MISSING_ATTRIBUTE;
    }

    authn_info->realm = apr_psprintf(conn->pool, "<%s://%s:%d> %s",
                                     conn->host_info.scheme,
                                     conn->host_info.hostname,
                                     conn->host_info.port,
                                     realm_name);

    /* Ask the application for credentials */
    apr_pool_create(&cred_pool, pool);
    status = (*ctx->cred_cb)(&username, &password, request, baton,
                             code, authn_info->scheme->name,
                             authn_info->realm, cred_pool);
    if (status) {
        apr_pool_destroy(cred_pool);
        return status;
    }

    digest_info->header = (code == 401) ? "Authorization" :
                                          "Proxy-Authorization";

    /* Store the digest authentication parameters in the context relative
       to this connection, so we can use it to create the Authorization header
       when setting up requests. */
    digest_info->pool = conn->pool;
    digest_info->qop = apr_pstrdup(digest_info->pool, qop);
    digest_info->nonce = apr_pstrdup(digest_info->pool, nonce);
    digest_info->cnonce = NULL;
    digest_info->opaque = apr_pstrdup(digest_info->pool, opaque);
    digest_info->algorithm = apr_pstrdup(digest_info->pool, algorithm);
    digest_info->realm = apr_pstrdup(digest_info->pool, realm_name);
    digest_info->username = apr_pstrdup(digest_info->pool, username);
    digest_info->digest_nc++;

    digest_info->ha1 = build_digest_ha1(username, password, digest_info->realm,
                                        digest_info->pool);

    apr_pool_destroy(cred_pool);

    /* If the handshake is finished tell serf it can send as much requests as it
       likes. */
    serf_connection_set_max_outstanding_requests(conn, 0);

    return APR_SUCCESS;
}

apr_status_t
serf__init_digest(int code,
                  serf_context_t *ctx,
                  apr_pool_t *pool)
{
    return APR_SUCCESS;
}

apr_status_t
serf__init_digest_connection(int code,
                             serf_connection_t *conn,
                             apr_pool_t *pool)
{
    /* Digest authentication is done per connection, so keep all progress
       information per connection. */
    if (code == 401) {
        conn->authn_baton = apr_pcalloc(pool, sizeof(digest_authn_info_t));
    } else {
        conn->proxy_authn_baton = apr_pcalloc(pool, sizeof(digest_authn_info_t));
    }

    /* Make serf send the initial requests one by one */
    serf_connection_set_max_outstanding_requests(conn, 1);

    return APR_SUCCESS;
}

apr_status_t
serf__setup_request_digest_auth(peer_t peer,
                                int code,
                                serf_connection_t *conn,
                                serf_request_t *request,
                                const char *method,
                                const char *uri,
                                serf_bucket_t *hdrs_bkt)
{
    digest_authn_info_t *digest_info = (peer == HOST) ? conn->authn_baton :
        conn->proxy_authn_baton;
    apr_status_t status = APR_SUCCESS;

    if (digest_info && digest_info->realm) {
        const char *value;
        apr_uri_t parsed_uri;

        /* TODO: per request pool? */

        /* Extract path from uri. */
        status = apr_uri_parse(conn->pool, uri, &parsed_uri);

        /* Build a new Authorization header. */
        digest_info->header = (peer == HOST) ? "Authorization" :
            "Proxy-Authorization";
        value = build_auth_header(digest_info, parsed_uri.path, method,
                                  conn->pool);

        serf_bucket_headers_setn(hdrs_bkt, digest_info->header,
                                 value);
        digest_info->digest_nc++;

        /* Store the uri of this request on the serf_request_t object, to make
           it available when validating the Authentication-Info header of the
           matching response. */
        request->auth_baton = parsed_uri.path;
    }

    return status;
}

apr_status_t
serf__validate_response_digest_auth(peer_t peer,
                                    int code,
                                    serf_connection_t *conn,
                                    serf_request_t *request,
                                    serf_bucket_t *response,
                                    apr_pool_t *pool)
{
    const char *key;
    char *auth_attr;
    char *nextkv;
    const char *rspauth = NULL;
    const char *qop = NULL;
    const char *nc_str = NULL;
    serf_bucket_t *hdrs;
    digest_authn_info_t *digest_info = (peer == HOST) ? conn->authn_baton :
        conn->proxy_authn_baton;

    hdrs = serf_bucket_response_get_headers(response);

    /* Need a copy cuz we're going to write NUL characters into the string.  */
    if (peer == HOST)
        auth_attr = apr_pstrdup(pool,
            serf_bucket_headers_get(hdrs, "Authentication-Info"));
    else
        auth_attr = apr_pstrdup(pool,
            serf_bucket_headers_get(hdrs, "Proxy-Authentication-Info"));

    /* If there's no Authentication-Info header there's nothing to validate. */
    if (! auth_attr)
        return APR_SUCCESS;

    /* We're expecting a list of key=value pairs, separated by a comma.
       Ex. rspauth="8a4b8451084b082be6b105e2b7975087",
       cnonce="346531653132652d303033392d3435", nc=00000007,
       qop=auth */
    for ( ; (key = apr_strtok(auth_attr, ",", &nextkv)) != NULL; auth_attr = NULL) {
        char *val;

        val = strchr(key, '=');
        if (val == NULL)
            continue;
        *val++ = '\0';

        /* skip leading spaces */
        while (*key && *key == ' ')
            key++;

        /* If the value is quoted, then remove the quotes.  */
        if (*val == '"') {
            apr_size_t last = strlen(val) - 1;

            if (val[last] == '"') {
                val[last] = '\0';
                val++;
            }
        }

        if (strcmp(key, "rspauth") == 0)
            rspauth = val;
        else if (strcmp(key, "qop") == 0)
            qop = val;
        else if (strcmp(key, "nc") == 0)
            nc_str = val;
    }

    if (rspauth) {
        const char *ha2, *tmp, *resp_hdr_hex;
        unsigned char resp_hdr[APR_MD5_DIGESTSIZE];
        const char *req_uri = request->auth_baton;

        ha2 = build_digest_ha2(req_uri, "", qop, pool);
        tmp = apr_psprintf(pool, "%s:%s:%s:%s:%s:%s",
                           digest_info->ha1, digest_info->nonce, nc_str,
                           digest_info->cnonce, digest_info->qop, ha2);
        apr_md5(resp_hdr, tmp, strlen(tmp));
        resp_hdr_hex =  hex_encode(resp_hdr, pool);

        /* Incorrect response-digest in Authentication-Info header. */
        if (strcmp(rspauth, resp_hdr_hex) != 0) {
            return SERF_ERROR_AUTHN_FAILED;
        }
    }

    return APR_SUCCESS;
}
