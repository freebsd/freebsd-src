/* Copyright 2011 Justin Erenkrantz and Greg Stein
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

/*** Setup a SSL tunnel over a HTTP proxy, according to RFC 2817. ***/

#include <apr_pools.h>
#include <apr_strings.h>

#include "serf.h"
#include "serf_private.h"


/* Structure passed around as baton for the CONNECT request and respone. */
typedef struct {
    apr_pool_t *pool;
    const char *uri;
} req_ctx_t;

/* forward declaration. */
static apr_status_t setup_request(serf_request_t *request,
                                  void *setup_baton,
                                  serf_bucket_t **req_bkt,
                                  serf_response_acceptor_t *acceptor,
                                  void **acceptor_baton,
                                  serf_response_handler_t *handler,
                                  void **handler_baton,
                                  apr_pool_t *pool);

static serf_bucket_t* accept_response(serf_request_t *request,
                                      serf_bucket_t *stream,
                                      void *acceptor_baton,
                                      apr_pool_t *pool)
{
    serf_bucket_t *c;
    serf_bucket_alloc_t *bkt_alloc;
#if 0
    req_ctx_t *ctx = acceptor_baton;
#endif

    /* get the per-request bucket allocator */
    bkt_alloc = serf_request_get_alloc(request);

    /* Create a barrier so the response doesn't eat us! */
    c = serf_bucket_barrier_create(stream, bkt_alloc);

    return serf_bucket_response_create(c, bkt_alloc);
}

/* If a 200 OK was received for the CONNECT request, consider the connection
   as ready for use. */
static apr_status_t handle_response(serf_request_t *request,
                                    serf_bucket_t *response,
                                    void *handler_baton,
                                    apr_pool_t *pool)
{
    apr_status_t status;
    serf_status_line sl;
    req_ctx_t *ctx = handler_baton;

    if (! response) {
        serf_connection_request_create(request->conn,
                                       setup_request,
                                       ctx);
        return APR_SUCCESS;
    }

    status = serf_bucket_response_status(response, &sl);
    if (SERF_BUCKET_READ_ERROR(status)) {
        return status;
    }
    if (!sl.version && (APR_STATUS_IS_EOF(status) ||
                      APR_STATUS_IS_EAGAIN(status)))
    {
        return status;
    }

    status = serf_bucket_response_wait_for_headers(response);
    if (status && !APR_STATUS_IS_EOF(status)) {
        return status;
    }

    /* RFC 2817:  Any successful (2xx) response to a CONNECT request indicates
       that the proxy has established a connection to the requested host and
       port, and has switched to tunneling the current connection to that server
       connection.
    */
    if (sl.code >= 200 && sl.code < 300) {
        request->conn->state = SERF_CONN_CONNECTED;

        /* Body is supposed to be empty. */
        apr_pool_destroy(ctx->pool);
        serf_bucket_destroy(request->conn->ssltunnel_ostream);
        request->conn->stream = NULL;
        ctx = NULL;

        serf__log(CONN_VERBOSE, __FILE__,
                  "successfully set up ssl tunnel on connection 0x%x\n",
                  request->conn);

        return APR_EOF;
    }

    /* Authentication failure and 2xx Ok are handled at this point,
       the rest are errors. */
    return SERF_ERROR_SSLTUNNEL_SETUP_FAILED;
}

/* Prepare the CONNECT request. */
static apr_status_t setup_request(serf_request_t *request,
                                  void *setup_baton,
                                  serf_bucket_t **req_bkt,
                                  serf_response_acceptor_t *acceptor,
                                  void **acceptor_baton,
                                  serf_response_handler_t *handler,
                                  void **handler_baton,
                                  apr_pool_t *pool)
{
    req_ctx_t *ctx = setup_baton;

    *req_bkt =
        serf_request_bucket_request_create(request,
                                           "CONNECT", ctx->uri,
                                           NULL,
                                           serf_request_get_alloc(request));
    *acceptor = accept_response;
    *acceptor_baton = ctx;
    *handler = handle_response;
    *handler_baton = ctx;

    return APR_SUCCESS;
}

static apr_status_t detect_eof(void *baton, serf_bucket_t *aggregate_bucket)
{
    serf_connection_t *conn = baton;
    conn->hit_eof = 1;
    return APR_EAGAIN;
}

/* SSL tunnel is needed, push a CONNECT request on the connection. */
apr_status_t serf__ssltunnel_connect(serf_connection_t *conn)
{
    req_ctx_t *ctx;
    apr_pool_t *ssltunnel_pool;

    apr_pool_create(&ssltunnel_pool, conn->pool);

    ctx = apr_palloc(ssltunnel_pool, sizeof(*ctx));
    ctx->pool = ssltunnel_pool;
    ctx->uri = apr_psprintf(ctx->pool, "%s:%d", conn->host_info.hostinfo,
                            conn->host_info.port);

    conn->ssltunnel_ostream = serf__bucket_stream_create(conn->allocator,
                                                         detect_eof,
                                                         conn);

    /* TODO: should be the first request on the connection. */
    serf_connection_priority_request_create(conn,
                                            setup_request,
                                            ctx);

    conn->state = SERF_CONN_SETUP_SSLTUNNEL;
    serf__log(CONN_VERBOSE, __FILE__,
              "setting up ssl tunnel on connection 0x%x\n", conn);

    return APR_SUCCESS;
}
