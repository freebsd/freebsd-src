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

#ifndef _NETINET6_IN6_FIB_H_
#define	_NETINET6_IN6_FIB_H_

#ifdef _KERNEL
#include <net/rt_nhops.h>
#endif

int fib6_lookup_nh_ifp(uint32_t fibnum, struct in6_addr *dst, uint32_t scopeid,
    uint32_t flowid, struct nhop6_basic *pnh6);
int fib6_lookup_nh_basic(uint32_t fibnum, const struct in6_addr *dst,
    uint32_t scopeid, uint32_t flowid, struct nhop6_basic *pnh6);
int fib6_lookup_nh_ext(uint32_t fibnum, struct in6_addr *dst,
    uint32_t scopeid, uint32_t flowid, uint32_t flags,
    struct nhop6_extended *pnh6);
void fib6_free_nh_ext(uint32_t fibnum, struct nhop6_extended *pnh6);
int rib6_lookup_nh_ext(uint32_t fibnum, struct in6_addr *dst, uint32_t scopeid,
    uint32_t flowid, uint32_t flags, struct rt6_extended *prt6);
void rib6_free_nh_ext(uint32_t fibnum, struct nhop6_extended *prt6);

void fib_free_nh_ext(uint32_t fibnum, struct nhopu_extended *pnhu);


void fib6_free_nh_prepend(uint32_t fibnum, struct nhop_prepend *nh);
void fib6_choose_prepend(uint32_t fibnum, struct nhop_prepend *nh_src,
    uint32_t flowid, struct nhop_prepend *nh, struct nhop6_extended *nh_ext);
int fib6_lookup_prepend(uint32_t fibnum, struct in6_addr *dst, uint32_t scopeid,
    struct mbuf *m, struct nhop_prepend *nh, struct nhop6_extended *nh_ext);

int fib6_sendmbuf(struct ifnet *ifp, struct ifnet *origifp, struct mbuf *m,
    struct nhop_prepend *nh);

#endif

