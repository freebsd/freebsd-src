/*
 * qperf - handle socket tests.
 *
 * Copyright (c) 2002-2009 Johann George.  All rights reserved.
 * Copyright (c) 2006-2009 QLogic Corporation.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#define _GNU_SOURCE
#include <errno.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "qperf.h"


/*
 * Parameters.
 */
#define AF_INET_SDP 27                  /* Family for SDP */


/*
 * Kinds.
 */
typedef enum {
    K_SCTP,
    K_SDP,
    K_TCP,
    K_UDP,
} KIND;

char *Kinds[] ={ "SCTP", "SDP", "TCP", "UDP", };


/*
 * Function prototypes.
 */
static void     client_init(int *fd, KIND kind);
static void     datagram_client_bw(KIND kind);
static void     datagram_client_lat(KIND kind);
static void     datagram_server_bw(KIND kind);
static void     datagram_server_init(int *fd, KIND kind);
static void     datagram_server_lat(KIND kind);
static void     get_socket_port(int fd, uint32_t *port);
static AI      *getaddrinfo_kind(int serverflag, KIND kind, int port);
static void     ip_parameters(long msgSize);
static char    *kind_name(KIND kind);
static int      recv_full(int fd, void *ptr, int len);
static int      send_full(int fd, void *ptr, int len);
static void     set_socket_buffer_size(int fd);
static void     stream_client_bw(KIND kind);
static void     stream_client_lat(KIND kind);
static void     stream_server_bw(KIND kind);
static void     stream_server_init(int *fd, KIND kind);
static void     stream_server_lat(KIND kind);


/*
 * Measure SCTP bandwidth (client side).
 */
void
run_client_sctp_bw(void)
{
    par_use(L_ACCESS_RECV);
    par_use(R_ACCESS_RECV);
    ip_parameters(32*1024);
    stream_client_bw(K_SCTP);
}


/*
 * Measure SCTP bandwidth (server side).
 */
void
run_server_sctp_bw(void)
{
    stream_server_bw(K_SCTP);
}


/*
 * Measure SCTP latency (client side).
 */
void
run_client_sctp_lat(void)
{
    ip_parameters(1);
    stream_client_lat(K_SCTP);
}


/*
 * Measure SCTP latency (server side).
 */
void
run_server_sctp_lat(void)
{
    stream_server_lat(K_SCTP);
}


/*
 * Measure SDP bandwidth (client side).
 */
void
run_client_sdp_bw(void)
{
    par_use(L_ACCESS_RECV);
    par_use(R_ACCESS_RECV);
    ip_parameters(64*1024);
    stream_client_bw(K_SDP);
}


/*
 * Measure SDP bandwidth (server side).
 */
void
run_server_sdp_bw(void)
{
    stream_server_bw(K_SDP);
}


/*
 * Measure SDP latency (client side).
 */
void
run_client_sdp_lat(void)
{
    ip_parameters(1);
    stream_client_lat(K_SDP);
}


/*
 * Measure SDP latency (server side).
 */
void
run_server_sdp_lat(void)
{
    stream_server_lat(K_SDP);
}


/*
 * Measure TCP bandwidth (client side).
 */
void
run_client_tcp_bw(void)
{
    par_use(L_ACCESS_RECV);
    par_use(R_ACCESS_RECV);
    ip_parameters(64*1024);
    stream_client_bw(K_TCP);
}


/*
 * Measure TCP bandwidth (server side).
 */
void
run_server_tcp_bw(void)
{
    stream_server_bw(K_TCP);
}


/*
 * Measure TCP latency (client side).
 */
void
run_client_tcp_lat(void)
{
    ip_parameters(1);
    stream_client_lat(K_TCP);
}


/*
 * Measure TCP latency (server side).
 */
void
run_server_tcp_lat(void)
{
    stream_server_lat(K_TCP);
}


/*
 * Measure UDP bandwidth (client side).
 */
void
run_client_udp_bw(void)
{
    par_use(L_ACCESS_RECV);
    par_use(R_ACCESS_RECV);
    ip_parameters(32*1024);
    datagram_client_bw(K_UDP);
}


/*
 * Measure UDP bandwidth (server side).
 */
void
run_server_udp_bw(void)
{
    datagram_server_bw(K_UDP);
}


/*
 * Measure UDP latency (client side).
 */
void
run_client_udp_lat(void)
{
    ip_parameters(1);
    datagram_client_lat(K_UDP);
}


/*
 * Measure UDP latency (server side).
 */
void
run_server_udp_lat(void)
{
    datagram_server_lat(K_UDP);
}


/*
 * Measure stream bandwidth (client side).
 */
static void
stream_client_bw(KIND kind)
{
    char *buf;
    int sockFD;

    client_init(&sockFD, kind);
    buf = qmalloc(Req.msg_size);
    sync_test();
    while (!Finished) {
        int n = send_full(sockFD, buf, Req.msg_size);

        if (Finished)
            break;
        if (n < 0) {
            LStat.s.no_errs++;
            continue;
        }
        LStat.s.no_bytes += n;
        LStat.s.no_msgs++;
    }
    stop_test_timer();
    exchange_results();
    free(buf);
    close(sockFD);
    show_results(BANDWIDTH);
}


/*
 * Measure stream bandwidth (server side).
 */
static void
stream_server_bw(KIND kind)
{
    int sockFD = -1;
    char *buf = 0;

    stream_server_init(&sockFD, kind);
    sync_test();
    buf = qmalloc(Req.msg_size);
    while (!Finished) {
        int n = recv_full(sockFD, buf, Req.msg_size);

        if (Finished)
            break;
        if (n < 0) {
            LStat.r.no_errs++;
            continue;
        }
        LStat.r.no_bytes += n;
        LStat.r.no_msgs++;
        if (Req.access_recv)
            touch_data(buf, Req.msg_size);
    }
    stop_test_timer();
    exchange_results();
    free(buf);
    if (sockFD >= 0)
        close(sockFD);
}


/*
 * Measure stream latency (client side).
 */
static void
stream_client_lat(KIND kind)
{
    char *buf;
    int sockFD;

    client_init(&sockFD, kind);
    buf = qmalloc(Req.msg_size);
    sync_test();
    while (!Finished) {
        int n = send_full(sockFD, buf, Req.msg_size);

        if (Finished)
            break;
        if (n < 0) {
            LStat.s.no_errs++;
            continue;
        }
        LStat.s.no_bytes += n;
        LStat.s.no_msgs++;

        n = recv_full(sockFD, buf, Req.msg_size);
        if (Finished)
            break;
        if (n < 0) {
            LStat.r.no_errs++;
            continue;
        }
        LStat.r.no_bytes += n;
        LStat.r.no_msgs++;
    }
    stop_test_timer();
    exchange_results();
    free(buf);
    close(sockFD);
    show_results(LATENCY);
}


/*
 * Measure stream latency (server side).
 */
static void
stream_server_lat(KIND kind)
{
    int sockFD = -1;
    char *buf = 0;

    stream_server_init(&sockFD, kind);
    sync_test();
    buf = qmalloc(Req.msg_size);
    while (!Finished) {
        int n = recv_full(sockFD, buf, Req.msg_size);

        if (Finished)
            break;
        if (n < 0) {
            LStat.r.no_errs++;
            continue;
        }
        LStat.r.no_bytes += n;
        LStat.r.no_msgs++;

        n = send_full(sockFD, buf, Req.msg_size);
        if (Finished)
            break;
        if (n < 0) {
            LStat.s.no_errs++;
            continue;
        }
        LStat.s.no_bytes += n;
        LStat.s.no_msgs++;
    }
    stop_test_timer();
    exchange_results();
    free(buf);
    close(sockFD);
}


/*
 * Measure datagram bandwidth (client side).
 */
static void
datagram_client_bw(KIND kind)
{
    char *buf;
    int sockFD;

    client_init(&sockFD, kind);
    buf = qmalloc(Req.msg_size);
    sync_test();
    while (!Finished) {
        int n = write(sockFD, buf, Req.msg_size);

        if (Finished)
            break;
        if (n < 0) {
            LStat.s.no_errs++;
            continue;
        }
        LStat.s.no_bytes += n;
        LStat.s.no_msgs++;
    }
    stop_test_timer();
    exchange_results();
    free(buf);
    close(sockFD);
    show_results(BANDWIDTH_SR);
}


/*
 * Measure datagram bandwidth (server side).
 */
static void
datagram_server_bw(KIND kind)
{
    int sockFD;
    char *buf = 0;

    datagram_server_init(&sockFD, kind);
    sync_test();
    buf = qmalloc(Req.msg_size);
    while (!Finished) {
        int n = recv(sockFD, buf, Req.msg_size, 0);

        if (Finished)
            break;
        if (n < 0) {
            LStat.r.no_errs++;
            continue;
        }
        LStat.r.no_bytes += n;
        LStat.r.no_msgs++;
        if (Req.access_recv)
            touch_data(buf, Req.msg_size);
    }
    stop_test_timer();
    exchange_results();
    free(buf);
    close(sockFD);
}


/*
 * Measure datagram latency (client side).
 */
static void
datagram_client_lat(KIND kind)
{
    char *buf;
    int sockFD;

    client_init(&sockFD, kind);
    buf = qmalloc(Req.msg_size);
    sync_test();
    while (!Finished) {
        int n = write(sockFD, buf, Req.msg_size);

        if (Finished)
            break;
        if (n < 0) {
            LStat.s.no_errs++;
            continue;
        }
        LStat.s.no_bytes += n;
        LStat.s.no_msgs++;

        n = read(sockFD, buf, Req.msg_size);
        if (Finished)
            break;
        if (n < 0) {
            LStat.r.no_errs++;
            continue;
        }
        LStat.r.no_bytes += n;
        LStat.r.no_msgs++;
    }
    stop_test_timer();
    exchange_results();
    free(buf);
    close(sockFD);
    show_results(LATENCY);
}


/*
 * Measure datagram latency (server side).
 */
static void
datagram_server_lat(KIND kind)
{
    int sockfd;
    char *buf = 0;

    datagram_server_init(&sockfd, kind);
    sync_test();
    buf = qmalloc(Req.msg_size);
    while (!Finished) {
        SS clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int n = recvfrom(sockfd, buf, Req.msg_size, 0,
                         (SA *)&clientAddr, &clientLen);

        if (Finished)
            break;
        if (n < 0) {
            LStat.r.no_errs++;
            continue;
        }
        LStat.r.no_bytes += n;
        LStat.r.no_msgs++;

        n = sendto(sockfd, buf, Req.msg_size, 0, (SA *)&clientAddr, clientLen);
        if (Finished)
            break;
        if (n < 0) {
            LStat.s.no_errs++;
            continue;
        }
        LStat.s.no_bytes += n;
        LStat.s.no_msgs++;
    }
    stop_test_timer();
    exchange_results();
    free(buf);
    close(sockfd);
}


/*
 * Set default IP parameters and ensure that any that are set are being used.
 */
static void
ip_parameters(long msgSize)
{
    setp_u32(0, L_MSG_SIZE, msgSize);
    setp_u32(0, R_MSG_SIZE, msgSize);
    par_use(L_PORT);
    par_use(R_PORT);
    par_use(L_SOCK_BUF_SIZE);
    par_use(R_SOCK_BUF_SIZE);
    opt_check();
}


/*
 * Socket client initialization.
 */
static void
client_init(int *fd, KIND kind)
{
    uint32_t rport;
    AI *ai, *ailist;

    client_send_request();
    recv_mesg(&rport, sizeof(rport), "port");
    rport = decode_uint32(&rport);
    ailist = getaddrinfo_kind(0, kind, rport);
    for (ai = ailist; ai; ai = ai->ai_next) {
        if (!ai->ai_family)
            continue;
        *fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
	setsockopt_one(*fd, SO_REUSEADDR);
        if (connect(*fd, ai->ai_addr, ai->ai_addrlen) == SUCCESS0)
            break;
        close(*fd);
    }
    freeaddrinfo(ailist);
    if (!ai)
        error(0, "could not make %s connection to server", kind_name(kind));
    if (Debug) {
        uint32_t lport;
        get_socket_port(*fd, &lport);
        debug("sending from %s port %d to %d", kind_name(kind), lport, rport);
    }
}


/*
 * Socket server initialization.
 */
static void
stream_server_init(int *fd, KIND kind)
{
    uint32_t port;
    AI *ai;
    int listenFD = -1;

    AI *ailist = getaddrinfo_kind(1, kind,  Req.port);
    for (ai = ailist; ai; ai = ai->ai_next) {
        if (!ai->ai_family)
            continue;
        listenFD = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (listenFD < 0)
            continue;
        setsockopt_one(listenFD, SO_REUSEADDR);
        if (bind(listenFD, ai->ai_addr, ai->ai_addrlen) == SUCCESS0)
            break;
        close(listenFD);
        listenFD = -1;
    }
    freeaddrinfo(ailist);
    if (!ai)
        error(0, "unable to make %s socket", kind_name(kind));
    if (listen(listenFD, 1) < 0)
        error(SYS, "listen failed");

    get_socket_port(listenFD, &port);
    encode_uint32(&port, port);
    send_mesg(&port, sizeof(port), "port");
    *fd = accept(listenFD, 0, 0);
    if (*fd < 0)
        error(SYS, "accept failed");
    debug("accepted %s connection", kind_name(kind));
    set_socket_buffer_size(*fd);
    close(listenFD);
    debug("receiving to %s port %d", kind_name(kind), port);
}


/*
 * Datagram server initialization.
 */
static void
datagram_server_init(int *fd, KIND kind)
{
    uint32_t port;
    AI *ai;
    int sockfd = -1;

    AI *ailist = getaddrinfo_kind(1, kind, Req.port);
    for (ai = ailist; ai; ai = ai->ai_next) {
        if (!ai->ai_family)
            continue;
        sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (sockfd < 0)
            continue;
        setsockopt_one(sockfd, SO_REUSEADDR);
        if (bind(sockfd, ai->ai_addr, ai->ai_addrlen) == SUCCESS0)
            break;
        close(sockfd);
        sockfd = -1;
    }
    freeaddrinfo(ailist);
    if (!ai)
        error(0, "unable to make %s socket", kind_name(kind));

    set_socket_buffer_size(sockfd);
    get_socket_port(sockfd, &port);
    encode_uint32(&port, port);
    send_mesg(&port, sizeof(port), "port");
    *fd = sockfd;
}


/*
 * A version of getaddrinfo that takes a numeric port and prints out an error
 * on failure.
 */
static AI *
getaddrinfo_kind(int serverflag, KIND kind, int port)
{
    AI *aip, *ailist;
    AI hints ={
        .ai_flags    = AI_NUMERICSERV,
        .ai_family   = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM
    };

    if (serverflag)
        hints.ai_flags |= AI_PASSIVE;
    if (kind == K_UDP)
        hints.ai_socktype = SOCK_DGRAM;

    ailist = getaddrinfo_port(serverflag ? 0 : ServerName, port, &hints);
    for (aip = ailist; aip; aip = aip->ai_next) {
        if (kind == K_SDP) {
            if (aip->ai_family == AF_INET || aip->ai_family == AF_INET6)
                aip->ai_family = AF_INET_SDP;
            else
                aip->ai_family = 0;
        } else if (kind == K_SCTP) {
            if (aip->ai_protocol == IPPROTO_TCP)
                aip->ai_protocol = IPPROTO_SCTP;
            else
                aip->ai_family = 0;
        }
    }
    return ailist;
}


/*
 * Set both the send and receive socket buffer sizes.
 */
static void
set_socket_buffer_size(int fd)
{
    int size = Req.sock_buf_size;

    if (!size)
        return;
    if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)) < 0)
        error(SYS, "Failed to set send buffer size on socket");
    if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) < 0)
        error(SYS, "Failed to set receive buffer size on socket");
}


/*
 * Given an open socket, return the port associated with it.  There must be a
 * more efficient way to do this that is portable.
 */
static void
get_socket_port(int fd, uint32_t *port)
{
    char p[NI_MAXSERV];
    SS sa;
    socklen_t salen = sizeof(sa);

    if (getsockname(fd, (SA *)&sa, &salen) < 0)
        error(SYS, "getsockname failed");
    if (getnameinfo((SA *)&sa, salen, 0, 0, p, sizeof(p), NI_NUMERICSERV) < 0)
        error(SYS, "getnameinfo failed");
    *port = atoi(p);
    if (!port)
        error(0, "invalid port");
}


/*
 * Send a complete message to a socket.  A zero byte write indicates an end of
 * file which suggests that we are finished.
 */
static int
send_full(int fd, void *ptr, int len)
{
    int n = len;

    while (!Finished && n) {
        int i = write(fd, ptr, n);

        if (i < 0)
            return i;
        ptr += i;
        n   -= i;
        if (i == 0)
            set_finished();
    }
    return len-n;
}


/*
 * Receive a complete message from a socket.  A zero byte read indicates an end
 * of file which suggests that we are finished.
 */
static int
recv_full(int fd, void *ptr, int len)
{
    int n = len;

    while (!Finished && n) {
        int i = read(fd, ptr, n);

        if (i < 0)
            return i;
        ptr += i;
        n   -= i;
        if (i == 0)
            set_finished();
    }
    return len-n;
}


/*
 * Return the name of a transport kind.
 */
static char *
kind_name(KIND kind)
{
    if (kind < 0 || kind >= cardof(Kinds))
        return "unknown type";
    else
        return Kinds[kind];
}
