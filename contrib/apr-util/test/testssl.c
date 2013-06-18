/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*
 * testssl: Simple APR SSL sockets test.
 */

#include "apr.h"
#include "apr_general.h"
#include "apr_pools.h"
#include "apr_errno.h"
#include "apr_getopt.h"
#include "apr_time.h"
#define APR_WANT_STRFUNC
#include "apr_want.h"

#include "apr_ssl.h"
#include "apr_network_io.h"

#if APR_HAVE_STDIO_H
#include <stdio.h>
#endif
#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdlib.h>     /* for atexit(), malloc() */
#include <string.h>

struct sslTestCase {
    char *host;
    int port;
    const char *request;
    int result;
} tests[] = {
    { "svn.apache.org", 443, "GET / HTTP/1.0\n\n", 1 },
    { NULL }
};

static apr_ssl_socket_t *createSocket(apr_ssl_factory_t *asf, 
                                      apr_pollset_t *pollset,
                                      apr_pool_t *pool, int blocking)
{
    apr_ssl_socket_t *sock;
    apr_status_t rv;
    printf("::Creating SSL socket\n");
    rv = apr_ssl_socket_create(&sock, AF_INET, SOCK_STREAM, 0, asf, NULL);
    if (rv != APR_SUCCESS) {
        printf("\tFailed to create socket\n");
        return NULL;
    }
    rv = apr_pollset_add_ssl_socket(pollset, sock);
    if (rv != APR_SUCCESS) {
        printf("\tFailed to add to pollset\n");
        return NULL;
    }
    printf("\tOK\n");
    return sock;
}

static apr_status_t connectSocket(apr_ssl_socket_t *sock,
                                  const char *host, int port,
                                  apr_pool_t *pool)
{
    apr_status_t rv;
    apr_sockaddr_t *remoteSA;

    printf("::Connecting socket\n");
    rv = apr_sockaddr_info_get(&remoteSA, host, APR_UNSPEC, port, 0, pool);
    if (rv != APR_SUCCESS) {
        printf("\tFailed to get address for '%s', port %d\n", host, port);
        return rv;
    }
    rv = apr_ssl_socket_connect(sock, remoteSA);
    if (rv != APR_SUCCESS) {
        printf("\tFailed to connect to '%s' port %d\n", host, port);
        return rv;
    }
    printf("\tOK\n");
    return rv;
}

static apr_status_t socketRead(apr_ssl_socket_t *sock,
                               apr_pollset_t *pollset,
                               char *buf, apr_size_t *len)
{
    int lrv;
    const apr_pollfd_t *descs = NULL;
    apr_status_t rv;

    printf("::Reading from socket\n");
    rv = apr_ssl_socket_set_poll_events(sock, APR_POLLIN);
    if (rv != APR_SUCCESS) {
        printf("\tUnable to change socket poll events!\n");
        return rv;
    }

    rv = apr_pollset_poll(pollset, 30 * APR_USEC_PER_SEC, &lrv, &descs);
    if (APR_STATUS_IS_TIMEUP(rv)) {
        printf("\tTime up!\n");
        return rv;
    }

    if (lrv != 1) {
        printf("\tIncorrect return count, %d\n", lrv);
        return rv;
    }
    if (descs[0].client_data != sock) {
        printf("\tWrong socket returned?!\n");
        return rv;
    }
    if ((descs[0].rtnevents & APR_POLLIN) == 0) {
        printf("\tSocket wasn't ready? huh? req [%08x] vs rtn [%08x]\n",
               descs[0].reqevents, descs[0].rtnevents);
        return rv;
    }
    rv = apr_ssl_socket_recv(sock, buf, len);
    if (rv == APR_SUCCESS)
        printf("\tOK, read %d bytes\n", *len);
    else
        printf("\tFailed\n");
    return rv;
}

static apr_status_t socketWrite(apr_ssl_socket_t *sock,
                                apr_pollset_t *pollset,
                                const char *buf, apr_size_t *len)
{
    int lrv;
    const apr_pollfd_t *descs = NULL;
    apr_status_t rv;

    printf("::Writing to socket\n");
    rv = apr_ssl_socket_set_poll_events(sock, APR_POLLOUT);
    if (rv != APR_SUCCESS) {
        printf("\tUnable to change socket poll events!\n");
        return rv;
    }

    rv = apr_pollset_poll(pollset, 30 * APR_USEC_PER_SEC, &lrv, &descs);
    if (APR_STATUS_IS_TIMEUP(rv)) {
        printf("\tTime up!\n");
        return rv;
    }
    if (lrv != 1) {
        printf("\tIncorrect return count, %d\n", lrv);
        return rv;
    }
    if (descs[0].client_data != sock) {
        printf("\tWrong socket returned?!\n");
        return rv;
    }
    if ((descs[0].rtnevents & APR_POLLOUT) == 0) {
        printf("\tSocket wasn't ready? huh?\n");
        return rv;
    }
    rv = apr_ssl_socket_send(sock, buf, len);
    if (rv == APR_SUCCESS)
        printf("\tOK, wrote %d bytes\n", *len);
    else
        printf("\tFailed\n");
    return rv;
}

apr_status_t socketClose(apr_ssl_socket_t *sock, apr_pollset_t *pollset)
{
    apr_status_t rv;
    printf("::Closing socket\n");
    rv = apr_pollset_remove_ssl_socket(sock);
    if (rv != APR_SUCCESS)
        printf("\tUnable to remove socket from pollset?\n");
    rv = apr_ssl_socket_close(sock);
    if (rv != APR_SUCCESS)
        printf("\tFailed to close SSL socket\n");
    else
        printf("\tOK\n");
    return rv;
}


int main(int argc, const char * const * argv)
{
    apr_pool_t *pool;
    apr_ssl_factory_t *asf = NULL;
    apr_status_t rv;
    apr_pollset_t *pollset;

    (void) apr_initialize();
    apr_pool_create(&pool, NULL);
    atexit(apr_terminate);

    printf("SSL Library: %s\n", apr_ssl_library_name());

    if (apr_pollset_create(&pollset, 1, pool, 0) != APR_SUCCESS) {
        printf("Failed to create pollset!\n");
        exit(1);
    }

    if (apr_ssl_factory_create(&asf, NULL, NULL, NULL, 
                               APR_SSL_FACTORY_CLIENT, pool) != APR_SUCCESS) {
        fprintf(stderr, "Unable to create client factory\n");
    } else {
        int i;
        for(i = 0; tests[i].host; i++) {
            apr_ssl_socket_t *sslSock = createSocket(asf, pollset, pool, 0);
            if (!sslSock)
                continue;

            rv = connectSocket(sslSock, tests[i].host, tests[i].port, pool);
            if (rv == APR_SUCCESS) {
                apr_size_t len = strlen(tests[i].request);
                rv = socketWrite(sslSock, pollset, tests[i].request, &len);
                if (rv == APR_SUCCESS) {
                    char buffer[4096];
                    len = 4096;
                    rv = socketRead(sslSock, pollset, buffer, &len);
                }
            }
            socketClose(sslSock, pollset);
        }
    }

    apr_pollset_destroy(pollset);
    apr_pool_destroy(pool);

    return 0;
}


