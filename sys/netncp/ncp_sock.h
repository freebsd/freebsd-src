/*
 * Copyright (c) 1999, Boris Popov
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#ifndef _NETNCP_NCP_SOCK_H_
#define _NETNCP_NCP_SOCK_H_

struct ncp_conn;
struct mbuf;
struct ncp_rq;
struct proc;
struct socket;
struct timeval;

int  ncp_sock_connect_ipx(struct ncp_conn *);
int  ncp_sock_connect_in(struct ncp_conn *);
int  ncp_sock_recv(struct socket *so, struct mbuf **mp, int *rlen);
int  ncp_sock_send(struct socket *so, struct mbuf *data, struct ncp_rq *rqp);
int  ncp_sock_disconnect(struct ncp_conn *conn);
int  ncp_poll(struct socket *so, int events);
int  ncp_sock_rselect(struct socket *so,struct proc *p, struct timeval *tv,int events);
int  ncp_sock_checksum(struct ncp_conn *conn, int enable);

void ncp_check_rq(struct ncp_conn *conn);
void ncp_check_conn(struct ncp_conn *conn);

void ncp_check_wd(struct ncp_conn *conn);

#endif /* _NCP_SOCK_H_ */
