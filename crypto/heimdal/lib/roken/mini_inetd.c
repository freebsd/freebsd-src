/*
 * Copyright (c) 1995 - 2001 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$Id: mini_inetd.c,v 1.30 2002/02/18 19:08:55 joda Exp $");
#endif

#include <err.h>
#include "roken.h"

/*
 * accept a connection on `s' and pretend it's served by inetd.
 */

static void
accept_it (int s)
{
    int s2;

    s2 = accept(s, NULL, NULL);
    if(s2 < 0)
	err (1, "accept");
    close(s);
    dup2(s2, STDIN_FILENO);
    dup2(s2, STDOUT_FILENO);
    /* dup2(s2, STDERR_FILENO); */
    close(s2);
}

/*
 * Listen on a specified port, emulating inetd.
 */

void
mini_inetd_addrinfo (struct addrinfo *ai)
{
    int ret;
    struct addrinfo *a;
    int n, nalloc, i;
    int *fds;
    fd_set orig_read_set, read_set;
    int max_fd = -1;

    for (nalloc = 0, a = ai; a != NULL; a = a->ai_next)
	++nalloc;

    fds = malloc (nalloc * sizeof(*fds));
    if (fds == NULL)
	errx (1, "mini_inetd: out of memory");

    FD_ZERO(&orig_read_set);

    for (i = 0, a = ai; a != NULL; a = a->ai_next) {
	fds[i] = socket (a->ai_family, a->ai_socktype, a->ai_protocol);
	if (fds[i] < 0) {
	    warn ("socket af = %d", a->ai_family);
	    continue;
	}
	socket_set_reuseaddr (fds[i], 1);
	if (bind (fds[i], a->ai_addr, a->ai_addrlen) < 0) {
	    warn ("bind af = %d", a->ai_family);
	    close(fds[i]);
	    continue;
	}
	if (listen (fds[i], SOMAXCONN) < 0) {
	    warn ("listen af = %d", a->ai_family);
	    close(fds[i]);
	    continue;
	}
	if (fds[i] >= FD_SETSIZE)
	    errx (1, "fd too large");
	FD_SET(fds[i], &orig_read_set);
	max_fd = max(max_fd, fds[i]);
	++i;
    }
    if (i == 0)
	errx (1, "no sockets");
    n = i;

    do {
	read_set = orig_read_set;

	ret = select (max_fd + 1, &read_set, NULL, NULL, NULL);
	if (ret < 0 && errno != EINTR)
	    err (1, "select");
    } while (ret <= 0);

    for (i = 0; i < n; ++i)
	if (FD_ISSET (fds[i], &read_set)) {
	    accept_it (fds[i]);
	    return;
	}
    abort ();
}

void
mini_inetd (int port)
{
    int error;
    struct addrinfo *ai, hints;
    char portstr[NI_MAXSERV];

    memset (&hints, 0, sizeof(hints));
    hints.ai_flags    = AI_PASSIVE;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family   = PF_UNSPEC;

    snprintf (portstr, sizeof(portstr), "%d", ntohs(port));

    error = getaddrinfo (NULL, portstr, &hints, &ai);
    if (error)
	errx (1, "getaddrinfo: %s", gai_strerror (error));

    mini_inetd_addrinfo(ai);
    
    freeaddrinfo(ai);
}
