/*
 * qperf - handle RDS tests.
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
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "qperf.h"


/*
 * Parameters.
 */
#define AF_INET_RDS 28                  /* Family for RDS */


/*
 * Function prototypes.
 */
static void     client_get_hosts(char *lhost, char *rhost);
static void     connect_tcp(char *server, char *port, SS *addr,
                                                    socklen_t *len, int *fd);
static void     get_socket_ip(SA *saptr, int salen, char *ip, int n);
static int      get_socket_port(int fd);
static int      init(void);
static void     qgetnameinfo(SA *sa, socklen_t salen, char *host,
                    size_t hostlen, char *serv, size_t servlen, int flags);
static int      rds_socket(char *host, int port);
static void     rds_makeaddr(SS *addr, socklen_t *len, char *host, int port);
static void     set_parameters(long msgSize);
static void     server_get_hosts(char *lhost, char *rhost);
static void     set_socket_buffer_size(int fd);


/*
 * Static variables.
 */
static SS        RAddr;
static socklen_t RLen;


/*
 * Measure RDS bandwidth (client side).
 */
void
run_client_rds_bw(void)
{
    char *buf;
    int sockfd;

    par_use(L_ACCESS_RECV);
    par_use(R_ACCESS_RECV);
    set_parameters(8*1024);
    client_send_request();
    sockfd = init();
    buf = qmalloc(Req.msg_size);
    sync_test();
    while (!Finished) {
        int n = sendto(sockfd, buf, Req.msg_size, 0, (SA *)&RAddr, RLen);

        if (Finished)
            break;
        if (n != Req.msg_size) {
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
    show_results(BANDWIDTH);
}


/*
 * Measure RDS bandwidth (server side).
 */
void
run_server_rds_bw(void)
{
    char *buf;
    int sockfd;

    sockfd = init();
    sync_test();
    buf = qmalloc(Req.msg_size);
    while (!Finished) {
        int n = read(sockfd, buf, Req.msg_size);
        if (Finished)
            break;
        if (n != Req.msg_size) {
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
    close(sockfd);
}


/*
 * Measure RDS latency (client side).
 */
void
run_client_rds_lat(void)
{
    char *buf;
    int sockfd;

    set_parameters(1);
    client_send_request();
    sockfd = init();
    buf = qmalloc(Req.msg_size);
    sync_test();
    while (!Finished) {
        int n = sendto(sockfd, buf, Req.msg_size, 0, (SA *)&RAddr, RLen);

        if (Finished)
            break;
        if (n != Req.msg_size) {
            LStat.s.no_errs++;
            continue;
        }
        LStat.s.no_bytes += n;
        LStat.s.no_msgs++;

        n = read(sockfd, buf, Req.msg_size);
        if (Finished)
            break;
        if (n != Req.msg_size) {
            LStat.r.no_errs++;
            continue;
        }
        LStat.r.no_bytes += n;
        LStat.r.no_msgs++;
    }
    stop_test_timer();
    exchange_results();
    free(buf);
    close(sockfd);
    show_results(LATENCY);
}


/*
 * Measure RDS latency (server side).
 */
void
run_server_rds_lat(void)
{
    char *buf;
    int sockfd;

    sockfd = init();
    sync_test();
    buf = qmalloc(Req.msg_size);
    while (!Finished) {
        SS raddr;
        socklen_t rlen = sizeof(raddr);
        int n = recvfrom(sockfd, buf, Req.msg_size, 0, (SA *)&raddr, &rlen);

        if (Finished)
            break;
        if (n != Req.msg_size) {
            LStat.r.no_errs++;
            continue;
        }
        LStat.r.no_bytes += n;
        LStat.r.no_msgs++;

        n = sendto(sockfd, buf, Req.msg_size, 0, (SA *)&raddr, rlen);
        if (Finished)
            break;
        if (n != Req.msg_size) {
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
set_parameters(long msgSize)
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
 * Initialize and return open socket.
 */
static int
init(void)
{
    int sockfd;
    uint32_t lport;
    uint32_t rport;
    char lhost[NI_MAXHOST];
    char rhost[NI_MAXHOST];

    if (is_client())
        client_get_hosts(lhost, rhost);
    else
        server_get_hosts(lhost, rhost);
    sockfd = rds_socket(lhost, Req.port);
    lport = get_socket_port(sockfd);
    encode_uint32(&lport, lport);
    send_mesg(&lport, sizeof(lport), "RDS port");
    recv_mesg(&rport, sizeof(rport), "RDS port");
    rport = decode_uint32(&rport);
    rds_makeaddr(&RAddr, &RLen, rhost, rport);
    return sockfd;
}


/*
 * Have an exchange with the client over TCP/IP and get the IP of our local
 * host.
 */
static void
server_get_hosts(char *lhost, char *rhost)
{
    int fd, lfd;
    uint32_t port;
    struct sockaddr_in laddr, raddr;
    socklen_t rlen;

    lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd < 0)
        error(SYS, "socket failed");
    setsockopt_one(lfd, SO_REUSEADDR);

    memset(&laddr, 0, sizeof(laddr));
    laddr.sin_family = AF_INET;
    laddr.sin_addr.s_addr = INADDR_ANY;
    laddr.sin_port = htons(0);
    if (bind(lfd, (SA *)&laddr, sizeof(laddr)) < 0)
        error(SYS, "bind INET failed");

    port = get_socket_port(lfd);
    encode_uint32(&port, port);
    send_mesg(&port, sizeof(port), "TCP IPv4 server port");

    if (listen(lfd, 1) < 0)
        error(SYS, "listen failed");

    rlen = sizeof(raddr);
    fd = accept(lfd, (SA *)&raddr, &rlen);
    if (fd < 0)
        error(SYS, "accept failed");
    close(lfd);
    get_socket_ip((SA *)&raddr, rlen, rhost, NI_MAXHOST);
    send_mesg(rhost, NI_MAXHOST, "client IP");
    recv_mesg(lhost, NI_MAXHOST, "server IP");
    close(fd);
}


/*
 * Have an exchange with the server over TCP/IP and get the IPs of our local
 * and the remote host.
 */
static void
client_get_hosts(char *lhost, char *rhost)
{
    SS raddr;
    socklen_t rlen;
    char *service;
    uint32_t port;
    int fd = -1;

    recv_mesg(&port, sizeof(port), "TCP IPv4 server port");
    port = decode_uint32(&port);
    service = qasprintf("%d", port);
    connect_tcp(ServerName, service, &raddr, &rlen, &fd);
    free(service);
    get_socket_ip((SA *)&raddr, rlen, rhost, NI_MAXHOST);
    send_mesg(rhost, NI_MAXHOST, "server IP");
    recv_mesg(lhost, NI_MAXHOST, "client IP");
    close(fd);
}


/*
 * Make a RDS socket.
 */
static int
rds_socket(char *host, int port)
{
    int sockfd;
    SS sockaddr;
    socklen_t socklen;

    sockfd = socket(AF_INET_RDS, SOCK_SEQPACKET, 0);
    if (sockfd < 0)
        error(SYS, "socket failed");
    setsockopt_one(sockfd, SO_REUSEADDR);
    rds_makeaddr(&sockaddr, &socklen, host, port);
    if (bind(sockfd, (SA *)&sockaddr, socklen) != SUCCESS0)
        error(SYS, "bind RDS failed");
    set_socket_buffer_size(sockfd);
    return sockfd;
}


/*
 * Make a RDS address.
 */
static void
rds_makeaddr(SS *addr, socklen_t *len, char *host, int port)
{
    struct sockaddr_in *sap = (struct sockaddr_in *)addr;

    memset(sap, 0, sizeof(*sap));
    sap->sin_family = AF_INET;
    inet_pton(AF_INET, host, &sap->sin_addr.s_addr);
    sap->sin_port = htons(port);
    *len = sizeof(struct sockaddr_in);
}


/*
 * Connect over TCP/IP to the server/port and return the socket structure, its
 * length and the open socket file descriptor.
 */
static void
connect_tcp(char *server, char *port, SS *addr, socklen_t *len, int *fd)
{
    int stat;
    struct addrinfo *aip, *ailist;
    struct addrinfo hints ={
        .ai_flags    = AI_NUMERICSERV,
        .ai_family   = AF_INET,
        .ai_socktype = SOCK_STREAM
    };

    stat = getaddrinfo(server, port, &hints, &ailist);
    if (stat != 0)
        error(0, "getaddrinfo failed: %s", gai_strerror(stat));
    for (aip = ailist; aip; aip = aip->ai_next) {
        if (fd) {
            *fd = socket(aip->ai_family, aip->ai_socktype, aip->ai_protocol);
            if (*fd < 0)
                error(SYS, "socket failed");
            if (connect(*fd, aip->ai_addr, aip->ai_addrlen) < 0)
                error(SYS, "connect failed");
            break;
        }
        break;
    }
    if (!aip)
        error(0, "connect_tcp failed");
    memcpy(addr, aip->ai_addr, aip->ai_addrlen);
    *len = aip->ai_addrlen;
    freeaddrinfo(ailist);
}


/*
 * Given an open socket, return the port associated with it.  There must be a
 * more efficient way to do this that is portable.
 */
static int
get_socket_port(int fd)
{
    int port;
    char p[NI_MAXSERV];
    SS sa;
    socklen_t salen = sizeof(sa);

    if (getsockname(fd, (SA *)&sa, &salen) < 0)
        error(SYS, "getsockname failed");
    qgetnameinfo((SA *)&sa, salen, 0, 0, p, sizeof(p), NI_NUMERICSERV);
    port = atoi(p);
    if (!port)
        error(SYS, "invalid port");
    return port;
}


/*
 * Given a socket, return its IP address.
 */
static void
get_socket_ip(SA *saptr, int salen, char *ip, int n)
{
    qgetnameinfo(saptr, salen, ip, n, 0, 0, NI_NUMERICHOST);
}


/*
 * Call getnameinfo and exit with an error on failure.
 */
static void
qgetnameinfo(SA *sa, socklen_t salen, char *host, size_t hostlen,
                                      char *serv, size_t servlen, int flags)
{
    int stat = getnameinfo(sa, salen, host, hostlen, serv, servlen, flags);

    if (stat < 0)
        error(0, "getnameinfo failed: %s", gai_strerror(stat));
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
        error(SYS, "failed to set send buffer size on socket");
    if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) < 0)
        error(SYS, "failed to set receive buffer size on socket");
}
