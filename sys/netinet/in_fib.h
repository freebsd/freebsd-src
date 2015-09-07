/*-
 * Copyright (c) 2015
 * 	Alexander V. Chernikov <melifaro@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _NETINET_IN_FIB_H_
#define	_NETINET_IN_FIB_H_

#ifdef _KERNEL
#include <net/rt_nhops.h>
#endif

int fib4_lookup_nh_ifp(uint32_t fibnum, struct in_addr dst, uint32_t flowid,
    struct nhop4_basic *pnh4);
int fib4_lookup_nh_basic(uint32_t fibnum, struct in_addr dst, uint32_t flowid,
    struct nhop4_basic *pnh4);
int fib4_lookup_nh_ext(uint32_t fibnum, struct in_addr dst,
    uint32_t flowid, uint32_t flags, struct nhop4_extended *pnh4);
void fib4_free_nh_ext(uint32_t fibnum, struct nhop4_extended *pnh4);
void fib4_source_to_sa_ext(const struct nhopu_extended *pnhu,
    struct sockaddr_in *sin);
int rib4_lookup_nh_ext(uint32_t fibnum, struct in_addr dst, uint32_t flowid,
    uint32_t flags, struct rt4_extended *prt4);
void rib4_free_nh_ext(uint32_t fibnum, struct rt4_extended *prt4);

void fib4_free_nh_prepend(uint32_t fibnum, struct nhop_prepend *nh);
void fib4_choose_prepend(uint32_t fibnum, struct nhop_prepend *nh_src,
    uint32_t flowid, struct nhop_prepend *nh, struct nhop4_extended *nh_ext);
int fib4_lookup_prepend(uint32_t fibnum, struct in_addr dst, struct mbuf *m,
    struct nhop_prepend *nh, struct nhop4_extended *nh_ext);

int fib4_sendmbuf(struct ifnet *ifp, struct mbuf *m, struct nhop_prepend *nh,
    struct in_addr dst);

#endif

