/* $OpenBSD: addr.c,v 1.7 2023/03/27 03:31:05 djm Exp $ */

/*
 * Copyright (c) 2004-2008 Damien Miller <djm@mindrot.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "includes.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "addr.h"

#define _SA(x)	((struct sockaddr *)(x))

int
addr_unicast_masklen(int af)
{
	switch (af) {
	case AF_INET:
		return 32;
	case AF_INET6:
		return 128;
	default:
		return -1;
	}
}

static inline int
masklen_valid(int af, u_int masklen)
{
	switch (af) {
	case AF_INET:
		return masklen <= 32 ? 0 : -1;
	case AF_INET6:
		return masklen <= 128 ? 0 : -1;
	default:
		return -1;
	}
}

int
addr_xaddr_to_sa(const struct xaddr *xa, struct sockaddr *sa, socklen_t *len,
    u_int16_t port)
{
	struct sockaddr_in *in4 = (struct sockaddr_in *)sa;
	struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)sa;

	if (xa == NULL || sa == NULL || len == NULL)
		return -1;

	switch (xa->af) {
	case AF_INET:
		if (*len < sizeof(*in4))
			return -1;
		memset(sa, '\0', sizeof(*in4));
		*len = sizeof(*in4);
#ifdef SOCK_HAS_LEN
		in4->sin_len = sizeof(*in4);
#endif
		in4->sin_family = AF_INET;
		in4->sin_port = htons(port);
		memcpy(&in4->sin_addr, &xa->v4, sizeof(in4->sin_addr));
		break;
	case AF_INET6:
		if (*len < sizeof(*in6))
			return -1;
		memset(sa, '\0', sizeof(*in6));
		*len = sizeof(*in6);
#ifdef SOCK_HAS_LEN
		in6->sin6_len = sizeof(*in6);
#endif
		in6->sin6_family = AF_INET6;
		in6->sin6_port = htons(port);
		memcpy(&in6->sin6_addr, &xa->v6, sizeof(in6->sin6_addr));
#ifdef HAVE_STRUCT_SOCKADDR_IN6_SIN6_SCOPE_ID
		in6->sin6_scope_id = xa->scope_id;
#endif
		break;
	default:
		return -1;
	}
	return 0;
}

/*
 * Convert struct sockaddr to struct xaddr
 * Returns 0 on success, -1 on failure.
 */
int
addr_sa_to_xaddr(struct sockaddr *sa, socklen_t slen, struct xaddr *xa)
{
	struct sockaddr_in *in4 = (struct sockaddr_in *)sa;
	struct sockaddr_in6 *in6 = (struct sockaddr_in6 *)sa;

	memset(xa, '\0', sizeof(*xa));

	switch (sa->sa_family) {
	case AF_INET:
		if (slen < (socklen_t)sizeof(*in4))
			return -1;
		xa->af = AF_INET;
		memcpy(&xa->v4, &in4->sin_addr, sizeof(xa->v4));
		break;
	case AF_INET6:
		if (slen < (socklen_t)sizeof(*in6))
			return -1;
		xa->af = AF_INET6;
		memcpy(&xa->v6, &in6->sin6_addr, sizeof(xa->v6));
#ifdef HAVE_STRUCT_SOCKADDR_IN6_SIN6_SCOPE_ID
		xa->scope_id = in6->sin6_scope_id;
#endif
		break;
	default:
		return -1;
	}

	return 0;
}

int
addr_invert(struct xaddr *n)
{
	int i;

	if (n == NULL)
		return -1;

	switch (n->af) {
	case AF_INET:
		n->v4.s_addr = ~n->v4.s_addr;
		return 0;
	case AF_INET6:
		for (i = 0; i < 4; i++)
			n->addr32[i] = ~n->addr32[i];
		return 0;
	default:
		return -1;
	}
}

/*
 * Calculate a netmask of length 'l' for address family 'af' and
 * store it in 'n'.
 * Returns 0 on success, -1 on failure.
 */
int
addr_netmask(int af, u_int l, struct xaddr *n)
{
	int i;

	if (masklen_valid(af, l) != 0 || n == NULL)
		return -1;

	memset(n, '\0', sizeof(*n));
	switch (af) {
	case AF_INET:
		n->af = AF_INET;
		if (l == 0)
			return 0;
		n->v4.s_addr = htonl((0xffffffff << (32 - l)) & 0xffffffff);
		return 0;
	case AF_INET6:
		n->af = AF_INET6;
		for (i = 0; i < 4 && l >= 32; i++, l -= 32)
			n->addr32[i] = 0xffffffffU;
		if (i < 4 && l != 0)
			n->addr32[i] = htonl((0xffffffff << (32 - l)) &
			    0xffffffff);
		return 0;
	default:
		return -1;
	}
}

int
addr_hostmask(int af, u_int l, struct xaddr *n)
{
	if (addr_netmask(af, l, n) == -1 || addr_invert(n) == -1)
		return -1;
	return 0;
}

/*
 * Perform logical AND of addresses 'a' and 'b', storing result in 'dst'.
 * Returns 0 on success, -1 on failure.
 */
int
addr_and(struct xaddr *dst, const struct xaddr *a, const struct xaddr *b)
{
	int i;

	if (dst == NULL || a == NULL || b == NULL || a->af != b->af)
		return -1;

	memcpy(dst, a, sizeof(*dst));
	switch (a->af) {
	case AF_INET:
		dst->v4.s_addr &= b->v4.s_addr;
		return 0;
	case AF_INET6:
		dst->scope_id = a->scope_id;
		for (i = 0; i < 4; i++)
			dst->addr32[i] &= b->addr32[i];
		return 0;
	default:
		return -1;
	}
}

int
addr_or(struct xaddr *dst, const struct xaddr *a, const struct xaddr *b)
{
	int i;

	if (dst == NULL || a == NULL || b == NULL || a->af != b->af)
		return (-1);

	memcpy(dst, a, sizeof(*dst));
	switch (a->af) {
	case AF_INET:
		dst->v4.s_addr |= b->v4.s_addr;
		return (0);
	case AF_INET6:
		for (i = 0; i < 4; i++)
			dst->addr32[i] |= b->addr32[i];
		return (0);
	default:
		return (-1);
	}
}

int
addr_cmp(const struct xaddr *a, const struct xaddr *b)
{
	int i;

	if (a->af != b->af)
		return (a->af == AF_INET6 ? 1 : -1);

	switch (a->af) {
	case AF_INET:
		/*
		 * Can't just subtract here as 255.255.255.255 - 0.0.0.0 is
		 * too big to fit into a signed int
		 */
		if (a->v4.s_addr == b->v4.s_addr)
			return 0;
		return (ntohl(a->v4.s_addr) > ntohl(b->v4.s_addr) ? 1 : -1);
	case AF_INET6:
		/*
		 * Do this a byte at a time to avoid the above issue and
		 * any endian problems
		 */
		for (i = 0; i < 16; i++)
			if (a->addr8[i] - b->addr8[i] != 0)
				return (a->addr8[i] - b->addr8[i]);
		if (a->scope_id == b->scope_id)
			return (0);
		return (a->scope_id > b->scope_id ? 1 : -1);
	default:
		return (-1);
	}
}

int
addr_is_all0s(const struct xaddr *a)
{
	int i;

	switch (a->af) {
	case AF_INET:
		return (a->v4.s_addr == 0 ? 0 : -1);
	case AF_INET6:
		for (i = 0; i < 4; i++)
			if (a->addr32[i] != 0)
				return -1;
		return 0;
	default:
		return -1;
	}
}

/* Increment the specified address. Note, does not do overflow checking */
void
addr_increment(struct xaddr *a)
{
	int i;
	uint32_t n;

	switch (a->af) {
	case AF_INET:
		a->v4.s_addr = htonl(ntohl(a->v4.s_addr) + 1);
		break;
	case AF_INET6:
		for (i = 0; i < 4; i++) {
			/* Increment with carry */
			n = ntohl(a->addr32[3 - i]) + 1;
			a->addr32[3 - i] = htonl(n);
			if (n != 0)
				break;
		}
		break;
	}
}

/*
 * Test whether host portion of address 'a', as determined by 'masklen'
 * is all zeros.
 * Returns 0 if host portion of address is all-zeros,
 * -1 if not all zeros or on failure.
 */
int
addr_host_is_all0s(const struct xaddr *a, u_int masklen)
{
	struct xaddr tmp_addr, tmp_mask, tmp_result;

	memcpy(&tmp_addr, a, sizeof(tmp_addr));
	if (addr_hostmask(a->af, masklen, &tmp_mask) == -1)
		return -1;
	if (addr_and(&tmp_result, &tmp_addr, &tmp_mask) == -1)
		return -1;
	return addr_is_all0s(&tmp_result);
}

#if 0
int
addr_host_to_all0s(struct xaddr *a, u_int masklen)
{
	struct xaddr tmp_mask;

	if (addr_netmask(a->af, masklen, &tmp_mask) == -1)
		return (-1);
	if (addr_and(a, a, &tmp_mask) == -1)
		return (-1);
	return (0);
}
#endif

int
addr_host_to_all1s(struct xaddr *a, u_int masklen)
{
	struct xaddr tmp_mask;

	if (addr_hostmask(a->af, masklen, &tmp_mask) == -1)
		return (-1);
	if (addr_or(a, a, &tmp_mask) == -1)
		return (-1);
	return (0);
}

/*
 * Parse string address 'p' into 'n'.
 * Returns 0 on success, -1 on failure.
 */
int
addr_pton(const char *p, struct xaddr *n)
{
	struct addrinfo hints, *ai;

	memset(&hints, '\0', sizeof(hints));
	hints.ai_flags = AI_NUMERICHOST;

	if (p == NULL || getaddrinfo(p, NULL, &hints, &ai) != 0)
		return -1;

	if (ai == NULL)
		return -1;

	if (ai->ai_addr == NULL) {
		freeaddrinfo(ai);
		return -1;
	}

	if (n != NULL && addr_sa_to_xaddr(ai->ai_addr, ai->ai_addrlen,
	    n) == -1) {
		freeaddrinfo(ai);
		return -1;
	}

	freeaddrinfo(ai);
	return 0;
}

int
addr_sa_pton(const char *h, const char *s, struct sockaddr *sa, socklen_t slen)
{
	struct addrinfo hints, *ai;

	memset(&hints, '\0', sizeof(hints));
	hints.ai_flags = AI_NUMERICHOST;

	if (h == NULL || getaddrinfo(h, s, &hints, &ai) != 0)
		return -1;

	if (ai == NULL)
		return -1;

	if (ai->ai_addr == NULL) {
		freeaddrinfo(ai);
		return -1;
	}

	if (sa != NULL) {
		if (slen < ai->ai_addrlen) {
			freeaddrinfo(ai);
			return -1;
		}
		memcpy(sa, &ai->ai_addr, ai->ai_addrlen);
	}

	freeaddrinfo(ai);
	return 0;
}

int
addr_ntop(const struct xaddr *n, char *p, size_t len)
{
	struct sockaddr_storage ss;
	socklen_t slen = sizeof(ss);

	if (addr_xaddr_to_sa(n, _SA(&ss), &slen, 0) == -1)
		return -1;
	if (p == NULL || len == 0)
		return -1;
	if (getnameinfo(_SA(&ss), slen, p, len, NULL, 0,
	    NI_NUMERICHOST) != 0)
		return -1;

	return 0;
}

/*
 * Parse a CIDR address (x.x.x.x/y or xxxx:yyyy::/z).
 * Return -1 on parse error, -2 on inconsistency or 0 on success.
 */
int
addr_pton_cidr(const char *p, struct xaddr *n, u_int *l)
{
	struct xaddr tmp;
	long unsigned int masklen = 999;
	char addrbuf[64], *mp, *cp;

	/* Don't modify argument */
	if (p == NULL || strlcpy(addrbuf, p, sizeof(addrbuf)) >= sizeof(addrbuf))
		return -1;

	if ((mp = strchr(addrbuf, '/')) != NULL) {
		*mp = '\0';
		mp++;
		masklen = strtoul(mp, &cp, 10);
		if (*mp < '0' || *mp > '9' || *cp != '\0' || masklen > 128)
			return -1;
	}

	if (addr_pton(addrbuf, &tmp) == -1)
		return -1;

	if (mp == NULL)
		masklen = addr_unicast_masklen(tmp.af);
	if (masklen_valid(tmp.af, masklen) == -1)
		return -2;
	if (addr_host_is_all0s(&tmp, masklen) != 0)
		return -2;

	if (n != NULL)
		memcpy(n, &tmp, sizeof(*n));
	if (l != NULL)
		*l = masklen;

	return 0;
}

int
addr_netmatch(const struct xaddr *host, const struct xaddr *net, u_int masklen)
{
	struct xaddr tmp_mask, tmp_result;

	if (host->af != net->af)
		return -1;

	if (addr_netmask(host->af, masklen, &tmp_mask) == -1)
		return -1;
	if (addr_and(&tmp_result, host, &tmp_mask) == -1)
		return -1;
	return addr_cmp(&tmp_result, net);
}
