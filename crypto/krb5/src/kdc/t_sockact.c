/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* kdc/t_sockact.c - socket activation test harness */
/*
 * Copyright (C) 2025 by the Massachusetts Institute of Technology.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Usage: t_sockact address... -- program args...
 *
 * This program simulates systemd socket activation by creating one or more
 * listener sockets at the specified addresses, setting LISTEN_FDS and
 * LISTEN_PID in the environment, and executing the specified command.  (The
 * real systemd would not execute the program until there is input on one of
 * the listener sockets, but we do not need to simulate that, and executing the
 * command immediately allow easier integration with k5test.py.)
 */

#include "k5-int.h"
#include "socket-utils.h"

static int max_fd;

static void
create_socket(const struct sockaddr *addr)
{
    int fd, one = 1;

    fd = socket(addr->sa_family, SOCK_STREAM, 0);
    if (fd < 0)
        abort();
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) != 0)
        abort();
#if defined(SO_REUSEPORT) && defined(__APPLE__)
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &one, sizeof(one)) != 0)
        abort();
#endif
#if defined(IPV6_V6ONLY)
    if (addr->sa_family == AF_INET6) {
        if (setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &one, sizeof(one)) != 0)
            abort();
    }
#endif
    if (bind(fd, addr, sa_socklen(addr)) != 0)
        abort();
    if (listen(fd, 5) != 0)
        abort();
    max_fd = fd;
}

int
main(int argc, char **argv)
{
    const char *addrstr;
    struct sockaddr_storage ss = { 0 };
    struct addrinfo hints = { 0 }, *ai_list = NULL, *ai = NULL;
    char *host, nbuf[128];
    int i, port;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--") == 0)
            break;
        addrstr = argv[i];
        if (*addrstr == '/') {
            ss.ss_family = AF_UNIX;
            strlcpy(ss2sun(&ss)->sun_path, addrstr,
                    sizeof(ss2sun(&ss)->sun_path));
            create_socket(ss2sa(&ss));
        } else {
            if (k5_parse_host_string(addrstr, 0, &host, &port) != 0 || !port)
                abort();
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_family = AF_UNSPEC;
            hints.ai_flags = AI_PASSIVE;
#ifdef AI_NUMERICSERV
            hints.ai_flags |= AI_NUMERICSERV;
#endif
            (void)snprintf(nbuf, sizeof(nbuf), "%d", port);
            if (getaddrinfo(host, nbuf, &hints, &ai_list) != 0)
                abort();
            for (ai = ai_list; ai != NULL; ai = ai->ai_next)
                create_socket(ai->ai_addr);
            freeaddrinfo(ai_list);
            free(host);
        }
    }
    argv += i + 1;

    (void)snprintf(nbuf, sizeof(nbuf), "%d", max_fd - 2);
    setenv("LISTEN_FDS", nbuf, 1);
    (void)snprintf(nbuf, sizeof(nbuf), "%lu", (unsigned long)getpid());
    setenv("LISTEN_PID", nbuf, 1);
    execv(argv[0], argv);
    abort();
    return 1;
}
