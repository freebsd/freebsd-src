/*-
 * Copyright (c) 2014 Andrey V. Elsukov <ae@FreeBSD.org>
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
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "stub.h"

void
ifa_free(void *ifa)
{

}

void
ifa_ref(void *ifa)
{

}

int
in_pcb_lport(struct inpcb *inp, struct in_addr *laddrp, u_short *lportp,
    struct ucred *cred, int lookupflags)
{

	return (EINVAL);
}

int
in_pcbinshash(struct inpcb *inp)
{

	return (EINVAL);
}

int
prison_local_ip6(struct ucred *cred, const struct sockaddr_in6 *ia6,
    int v6only)
{

	return (EAFNOSUPPORT);
}

int
prison_check_ip6(struct ucred *cred, const struct sockaddr_in6 *ia6)
{

	return (EAFNOSUPPORT);
}

int
prison_saddrsel_ip6(struct ucred *cred, const struct sockaddr_in6 *ia6)
{

	return (EAFNOSUPPORT);
}

/* Copy from netinet6/in6.c */
int
in6_mask2len(struct in6_addr *mask, u_char *lim0)
{
	int x = 0, y;
	u_char *lim = lim0, *p;

	/* ignore the scope_id part */
	if (lim0 == NULL || lim0 - (u_char *)mask > sizeof(*mask))
		lim = (u_char *)mask + sizeof(*mask);
	for (p = (u_char *)mask; p < lim; x++, p++) {
		if (*p != 0xff)
			break;
	}
	y = 0;
	if (p < lim) {
		for (y = 0; y < 8; y++) {
			if ((*p & (0x80 >> y)) == 0)
				break;
		}
	}

	/*
	 * when the limit pointer is given, do a stricter check on the
	 * remaining bits.
	 */
	if (p < lim) {
		if (y != 0 && (*p & (0x00ff >> y)) != 0)
			return (-1);
		for (p = p + 1; p < lim; p++)
			if (*p != 0)
				return (-1);
	}

	return (x * 8 + y);
}

int
in6_matchlen(struct in6_addr *src, struct in6_addr *dst)
{
	int match = 0;
	u_char *s = (u_char *)src, *d = (u_char *)dst;
	u_char *lim = s + 16, r;

	while (s < lim)
		if ((r = (*d++ ^ *s++)) != 0) {
			while (r < 128) {
				match++;
				r <<= 1;
			}
			break;
		} else
			match += 8;
	return (match);
}

struct in6_ifaddr *
in6ifa_ifwithaddr(const struct in6_addr *addr, uint32_t zoneid)
{
	struct in6_ifaddr *ia;

	TAILQ_FOREACH(ia, &V_in6_ifaddrhead, ia_link) {
		if (IN6_ARE_ADDR_EQUAL(IA6_IN6(ia), addr)) {
			if (zoneid != 0 &&
			    zoneid != ia->ia_addr.sin6_scope_id)
				continue;
			break;
		}
	}
	return (ia);
}
/* Copy from netinet6/scope6.c */
struct ifnet*
in6_getlinkifnet(uint32_t zoneid)
{

	return (ifnet_byindex(zoneid));
}

uint32_t
in6_getscopezone(const struct ifnet *ifp, int scope)
{

	if (scope == __IPV6_ADDR_SCOPE_INTFACELOCAL ||
	    scope == __IPV6_ADDR_SCOPE_LINKLOCAL)
		return (ifp->if_index);
	return (0);
}

int
in6_addrscope(const struct in6_addr *addr)
{

	if (IN6_IS_ADDR_MULTICAST(addr))
		return (__IPV6_ADDR_MC_SCOPE(addr));
	if (IN6_IS_ADDR_LINKLOCAL(addr) ||
	    IN6_IS_ADDR_LOOPBACK(addr))
		return (__IPV6_ADDR_SCOPE_LINKLOCAL);
	return (__IPV6_ADDR_SCOPE_GLOBAL);
}

int
ifa_preferred(struct ifaddr *cur, struct ifaddr *next)
{

	return (cur->ifa_carp && (!next->ifa_carp ||
	    (carp_master(next->ifa_carp) && !carp_master(cur->ifa_carp))));
}


