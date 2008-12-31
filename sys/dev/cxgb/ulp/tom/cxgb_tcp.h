
/*-
 * Copyright (c) 2007, Chelsio Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Neither the name of the Chelsio Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/dev/cxgb/ulp/tom/cxgb_tcp.h,v 1.2.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $
 */
#ifndef CXGB_TCP_H_
#define CXGB_TCP_H_
#ifdef TCP_USRREQS_OVERLOAD
struct tcpcb *cxgb_tcp_drop(struct tcpcb *tp, int errno);
#else
#define cxgb_tcp_drop	tcp_drop
#endif
void cxgb_tcp_ctlinput(int cmd, struct sockaddr *sa, void *vip);
struct tcpcb *cxgb_tcp_close(struct tcpcb *tp);

extern struct pr_usrreqs cxgb_tcp_usrreqs;
#ifdef INET6
extern struct pr_usrreqs cxgb_tcp6_usrreqs;
#endif

#include <sys/sysctl.h>
SYSCTL_DECL(_net_inet_tcp_cxgb);
#endif  /* CXGB_TCP_H_ */
