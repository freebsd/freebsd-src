/*
 * Copyright (c) 1995 - 2000 Kungliga Tekniska Högskolan
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
RCSID("$Id: mini_inetd.c,v 1.18.2.1 2000/10/10 13:22:33 assar Exp $");
#endif

#include <stdio.h>

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETINET_IN6_H
#include <netinet/in6.h>
#endif
#ifdef HAVE_NETINET6_IN6_H
#include <netinet6/in6.h>
#endif


#include <roken.h>

static int
listen_v4 (int port)
{
     struct sockaddr_in sa;
     int s;

     s = socket(AF_INET, SOCK_STREAM, 0);
     if(s < 0) {
	 if (errno == ENOSYS)
	     return -1;
	  perror("socket");
	  exit(1);
     }
     socket_set_reuseaddr (s, 1);
     memset(&sa, 0, sizeof(sa));
     sa.sin_family      = AF_INET;
     sa.sin_port        = port;
     sa.sin_addr.s_addr = INADDR_ANY;
     if(bind(s, (struct sockaddr*)&sa, sizeof(sa)) < 0){
	  perror("bind");
	  exit(1);
     }
     if(listen(s, SOMAXCONN) < 0){
	  perror("listen");
	  exit(1);
     }
     return s;
}

#ifdef HAVE_IPV6
static int
listen_v6 (int port)
{
     struct sockaddr_in6 sa;
     int s;

     s = socket(AF_INET6, SOCK_STREAM, 0);
     if(s < 0) {
	 if (errno == ENOSYS)
	     return -1;
	 perror("socket");
	 exit(1);
     }
     socket_set_reuseaddr (s, 1);
     memset(&sa, 0, sizeof(sa));
     sa.sin6_family = AF_INET6;
     sa.sin6_port   = port;
     sa.sin6_addr   = in6addr_any;
     if(bind(s, (struct sockaddr*)&sa, sizeof(sa)) < 0){
	  perror("bind");
	  exit(1);
     }
     if(listen(s, SOMAXCONN) < 0){
	  perror("listen");
	  exit(1);
     }
     return s;
}
#endif /* HAVE_IPV6 */

/*
 * accept a connection on `s' and pretend it's served by inetd.
 */

static void
accept_it (int s)
{
    int s2;

    s2 = accept(s, NULL, 0);
    if(s2 < 0){
	perror("accept");
	exit(1);
    }
    close(s);
    dup2(s2, STDIN_FILENO);
    dup2(s2, STDOUT_FILENO);
    /* dup2(s2, STDERR_FILENO); */
    close(s2);
}

/*
 * Listen on `port' emulating inetd.
 */

void
mini_inetd (int port)
{
    int ret;
    int max_fd = -1;
    int sock_v4 = -1;
    int sock_v6 = -1;
    fd_set orig_read_set, read_set;

    FD_ZERO(&orig_read_set);

    sock_v4 = listen_v4 (port);
    if (sock_v4 >= 0) {
	max_fd  = max(max_fd, sock_v4);
	if (max_fd >= FD_SETSIZE)
	    errx (1, "fd too large");
	FD_SET(sock_v4, &orig_read_set);
    }
#ifdef HAVE_IPV6
    sock_v6 = listen_v6 (port);
    if (sock_v6 >= 0) {
	max_fd  = max(max_fd, sock_v6);
	if (max_fd >= FD_SETSIZE)
	    errx (1, "fd too large");
	FD_SET(sock_v6, &orig_read_set);
    }
#endif    

    do {
	read_set = orig_read_set;

	ret = select (max_fd + 1, &read_set, NULL, NULL, NULL);
	if (ret < 0 && ret != EINTR) {
	    perror ("select");
	    exit (1);
	}
    } while (ret <= 0);

    if (sock_v4 > 0 && FD_ISSET (sock_v4, &read_set)) {
	accept_it (sock_v4);
	return;
    }
    if (sock_v6 > 0 && FD_ISSET (sock_v6, &read_set)) {
	accept_it (sock_v6);
	return;
    }
    abort ();
}
