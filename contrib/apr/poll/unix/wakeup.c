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

#include "apr.h"
#include "apr_poll.h"
#include "apr_time.h"
#include "apr_portable.h"
#include "apr_arch_file_io.h"
#include "apr_arch_networkio.h"
#include "apr_arch_poll_private.h"
#include "apr_arch_inherit.h"

#if !APR_FILES_AS_SOCKETS

#ifdef WIN32

apr_status_t apr_poll_create_wakeup_pipe(apr_pool_t *pool, apr_pollfd_t *pfd,
                                         apr_file_t **wakeup_pipe)
{
    apr_status_t rv;

    if ((rv = apr_file_socket_pipe_create(&wakeup_pipe[0], &wakeup_pipe[1],
                                      pool)) != APR_SUCCESS)
        return rv;

    pfd->reqevents = APR_POLLIN;
    pfd->desc_type = APR_POLL_FILE;
    pfd->desc.f = wakeup_pipe[0];
    return APR_SUCCESS;
}

apr_status_t apr_poll_close_wakeup_pipe(apr_file_t **wakeup_pipe)
{
    apr_status_t rv0 = APR_SUCCESS;
    apr_status_t rv1 = APR_SUCCESS;

    /* Close both sides of the wakeup pipe */
    if (wakeup_pipe[0]) {
        rv0 = apr_file_socket_pipe_close(wakeup_pipe[0]);
        wakeup_pipe[0] = NULL;
    }
    if (wakeup_pipe[1]) {
        rv1 = apr_file_socket_pipe_close(wakeup_pipe[1]);
        wakeup_pipe[1] = NULL;
    }
    return rv0 ? rv0 : rv1;
}

#else /* !WIN32 */

apr_status_t apr_poll_create_wakeup_pipe(apr_pollfd_t *pfd, apr_file_t **wakeup_pipe)
{
    return APR_ENOTIMPL;
}

apr_status_t apr_poll_close_wakeup_pipe(apr_file_t **wakeup_pipe)
{
    return APR_ENOTIMPL;
}

#endif /* !WIN32 */

#else  /* APR_FILES_AS_SOCKETS */

apr_status_t apr_poll_create_wakeup_pipe(apr_pool_t *pool, apr_pollfd_t *pfd,
                                         apr_file_t **wakeup_pipe)
{
    apr_status_t rv;

    if ((rv = apr_file_pipe_create(&wakeup_pipe[0], &wakeup_pipe[1],
                                   pool)) != APR_SUCCESS)
        return rv;

    pfd->p = pool;
    pfd->reqevents = APR_POLLIN;
    pfd->desc_type = APR_POLL_FILE;
    pfd->desc.f = wakeup_pipe[0];

    {
        int flags;

        if ((flags = fcntl(wakeup_pipe[0]->filedes, F_GETFD)) == -1)
            return errno;

        flags |= FD_CLOEXEC;
        if (fcntl(wakeup_pipe[0]->filedes, F_SETFD, flags) == -1)
            return errno;
    }
    {
        int flags;

        if ((flags = fcntl(wakeup_pipe[1]->filedes, F_GETFD)) == -1)
            return errno;

        flags |= FD_CLOEXEC;
        if (fcntl(wakeup_pipe[1]->filedes, F_SETFD, flags) == -1)
            return errno;
    }

    return APR_SUCCESS;
}

apr_status_t apr_poll_close_wakeup_pipe(apr_file_t **wakeup_pipe)
{
    apr_status_t rv0 = APR_SUCCESS;
    apr_status_t rv1 = APR_SUCCESS;

    /* Close both sides of the wakeup pipe */
    if (wakeup_pipe[0]) {
        rv0 = apr_file_close(wakeup_pipe[0]);
        wakeup_pipe[0] = NULL;
    }
    if (wakeup_pipe[1]) {
        rv1 = apr_file_close(wakeup_pipe[1]);
        wakeup_pipe[1] = NULL;
    }
    return rv0 ? rv0 : rv1;
}

#endif /* APR_FILES_AS_SOCKETS */

/* Read and discard whatever is in the wakeup pipe.
 */
void apr_poll_drain_wakeup_pipe(apr_file_t **wakeup_pipe)
{
    char rb[512];
    apr_size_t nr = sizeof(rb);

    while (apr_file_read(wakeup_pipe[0], rb, &nr) == APR_SUCCESS) {
        /* Although we write just one byte to the other end of the pipe
         * during wakeup, multiple threads could call the wakeup.
         * So simply drain out from the input side of the pipe all
         * the data.
         */
        if (nr != sizeof(rb))
            break;
    }
}
