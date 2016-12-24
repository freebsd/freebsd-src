/*-
 * Copyright (c) 2016 Andrey V. Elsukov <ae@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/priv.h>
#include <sys/rmlock.h>
#include <sys/socket.h>
#include <sys/sockopt.h>
#include <sys/syslog.h>
#include <sys/proc.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>

#include <netipsec/ipsec_support.h>
#include <netipsec/ipsec.h>
#include <netipsec/ipsec6.h>
#include <netipsec/key.h>
#include <netipsec/key_debug.h>

/*
 * This file is build in the kernel only when 'options IPSEC' or
 * 'options IPSEC_SUPPORT' is enabled.
 */

struct rmlock ipsec_kmod_lock;
RM_SYSINIT(ipsec_kmod_lock, &ipsec_kmod_lock, "IPsec KLD lock");

#define	METHOD_DECL(...)	__VA_ARGS__
#define	METHOD_ARGS(...)	__VA_ARGS__
#define	IPSEC_KMOD_METHOD(type, name, sc, method, decl, args)		\
type name (decl)							\
{									\
	struct rm_priotracker tracker;					\
	type ret;							\
	IPSEC_ASSERT(sc != NULL, ("called with NULL methods"));		\
	rm_rlock(&ipsec_kmod_lock, &tracker);				\
	ret = (*sc->method)(args);					\
	rm_runlock(&ipsec_kmod_lock, &tracker);				\
	return (ret);							\
}

static int
ipsec_support_modevent(module_t mod, int type, void *data)
{

	switch (type) {
	case MOD_LOAD:
		return (0);
	case MOD_UNLOAD:
		return (EBUSY);
	default:
		return (EOPNOTSUPP);
	}
}

static moduledata_t ipsec_support_mod = {
	"ipsec_support",
	ipsec_support_modevent,
	0
};

/*
 * Declare IPSEC_SUPPORT as module to be able add dependency in
 * ipsec.ko and tcpmd5.ko
 */
DECLARE_MODULE(ipsec_support, ipsec_support_mod,
    SI_SUB_PROTO_DOMAIN, SI_ORDER_ANY);
MODULE_VERSION(ipsec_support, 1);

#ifdef INET
void
ipsec4_setsockaddrs(const struct mbuf *m, union sockaddr_union *src,
    union sockaddr_union *dst)
{
	static const struct sockaddr_in template = {
		sizeof (struct sockaddr_in),
		AF_INET,
		0, { 0 }, { 0, 0, 0, 0, 0, 0, 0, 0 }
	};

	src->sin = template;
	dst->sin = template;

	if (m->m_len < sizeof (struct ip)) {
		m_copydata(m, offsetof(struct ip, ip_src),
			   sizeof (struct  in_addr),
			   (caddr_t) &src->sin.sin_addr);
		m_copydata(m, offsetof(struct ip, ip_dst),
			   sizeof (struct  in_addr),
			   (caddr_t) &dst->sin.sin_addr);
	} else {
		const struct ip *ip = mtod(m, const struct ip *);
		src->sin.sin_addr = ip->ip_src;
		dst->sin.sin_addr = ip->ip_dst;
	}
}
#endif
#ifdef INET6
void
ipsec6_setsockaddrs(const struct mbuf *m, union sockaddr_union *src,
    union sockaddr_union *dst)
{
	struct ip6_hdr ip6buf;
	const struct ip6_hdr *ip6;

	if (m->m_len >= sizeof(*ip6))
		ip6 = mtod(m, const struct ip6_hdr *);
	else {
		m_copydata(m, 0, sizeof(ip6buf), (caddr_t)&ip6buf);
		ip6 = &ip6buf;
	}

	bzero(&src->sin6, sizeof(struct sockaddr_in6));
	src->sin6.sin6_family = AF_INET6;
	src->sin6.sin6_len = sizeof(struct sockaddr_in6);
	bcopy(&ip6->ip6_src, &src->sin6.sin6_addr, sizeof(ip6->ip6_src));
	if (IN6_IS_SCOPE_LINKLOCAL(&ip6->ip6_src)) {
		src->sin6.sin6_addr.s6_addr16[1] = 0;
		src->sin6.sin6_scope_id = ntohs(ip6->ip6_src.s6_addr16[1]);
	}

	bzero(&dst->sin6, sizeof(struct sockaddr_in6));
	dst->sin6.sin6_family = AF_INET6;
	dst->sin6.sin6_len = sizeof(struct sockaddr_in6);
	bcopy(&ip6->ip6_dst, &dst->sin6.sin6_addr, sizeof(ip6->ip6_dst));
	if (IN6_IS_SCOPE_LINKLOCAL(&ip6->ip6_dst)) {
		dst->sin6.sin6_addr.s6_addr16[1] = 0;
		dst->sin6.sin6_scope_id = ntohs(ip6->ip6_dst.s6_addr16[1]);
	}
}
#endif

#ifdef TCP_SIGNATURE
const int tcp_ipsec_support = 1;
#else
#ifdef IPSEC_SUPPORT
volatile int tcp_ipsec_support = 0;
const struct tcpmd5_support * volatile tcp_ipsec_methods = NULL;

IPSEC_KMOD_METHOD(int, tcpmd5_kmod_input,
    tcp_ipsec_methods,
    input, METHOD_DECL(struct mbuf *m, struct tcphdr *th, u_char *buf),
    METHOD_ARGS(m, th, buf)
)

IPSEC_KMOD_METHOD(int, tcpmd5_kmod_output,
    tcp_ipsec_methods,
    output, METHOD_DECL(struct mbuf *m, struct tcphdr *th, u_char *buf),
    METHOD_ARGS(m, th, buf)
)

IPSEC_KMOD_METHOD(int, tcpmd5_kmod_pcbctl,
    tcp_ipsec_methods,
    pcbctl, METHOD_DECL(struct inpcb *inp, struct sockopt *sopt),
    METHOD_ARGS(inp, sopt)
)
#endif
#endif

#ifdef IPSEC
/*
 * IPsec support is build in the kernel. Additional locking isn't required.
 */
#ifdef INET
static struct ipsec_support ipv4_ipsec = {
	.input = ipsec4_input,
	.forward = ipsec4_forward,
	.output = ipsec4_output,
	.pcbctl = ipsec4_pcbctl,
	.capability = ipsec4_capability,
	.check_policy = ipsec4_in_reject,
	.hdrsize = ipsec_hdrsiz_inpcb
};
const int ipv4_ipsec_support = 1;
const struct ipsec_support * const ipv4_ipsec_methods = &ipv4_ipsec;
#endif

#ifdef INET6
static struct ipsec_support ipv6_ipsec = {
	.input = ipsec6_input,
	.forward = ipsec6_forward,
	.output = ipsec6_output,
	.pcbctl = ipsec6_pcbctl,
	.capability = ipsec6_capability,
	.check_policy = ipsec6_in_reject,
	.hdrsize = ipsec_hdrsiz_inpcb
};
const int ipv6_ipsec_support = 1;
const struct ipsec_support * const ipv6_ipsec_methods = &ipv6_ipsec;
#endif
#else /* IPSEC_SUPPORT */
/*
 * IPsec support is build as kernel module.
 */
#ifdef INET
volatile int ipv4_ipsec_support = 0;
const struct ipsec_support * volatile ipv4_ipsec_methods = NULL;
const struct udpencap_support * volatile udp_ipsec_methods = NULL;

IPSEC_KMOD_METHOD(int, udpencap_kmod_input,
    udp_ipsec_methods,
    input, METHOD_DECL(struct mbuf *m, int off, int af),
    METHOD_ARGS(m, off, af)
)

IPSEC_KMOD_METHOD(int, udpencap_kmod_pcbctl,
    udp_ipsec_methods,
    pcbctl, METHOD_DECL(struct inpcb *inp, struct sockopt *sopt),
    METHOD_ARGS(inp, sopt)
)
#endif

#ifdef INET6
volatile int ipv6_ipsec_support = 0;
const struct ipsec_support * volatile ipv6_ipsec_methods = NULL;
#endif

IPSEC_KMOD_METHOD(int, ipsec_kmod_input, sc,
    input, METHOD_DECL(const struct ipsec_support *sc, struct mbuf *m,
	int offset, int proto), METHOD_ARGS(m, offset, proto)
)

IPSEC_KMOD_METHOD(int, ipsec_kmod_check_policy, sc,
    check_policy, METHOD_DECL(const struct ipsec_support *sc, struct mbuf *m,
	struct inpcb *inp), METHOD_ARGS(m, inp)
)

IPSEC_KMOD_METHOD(int, ipsec_kmod_forward, sc,
    forward, METHOD_DECL(const struct ipsec_support *sc, struct mbuf *m),
    (m)
)

IPSEC_KMOD_METHOD(int, ipsec_kmod_output, sc,
    output, METHOD_DECL(const struct ipsec_support *sc, struct mbuf *m,
	struct inpcb *inp), METHOD_ARGS(m, inp)
)

IPSEC_KMOD_METHOD(int, ipsec_kmod_pcbctl, sc,
    pcbctl, METHOD_DECL(const struct ipsec_support *sc, struct inpcb *inp,
	struct sockopt *sopt), METHOD_ARGS(inp, sopt)
)

IPSEC_KMOD_METHOD(size_t, ipsec_kmod_hdrsize, sc,
    hdrsize, METHOD_DECL(const struct ipsec_support *sc, struct inpcb *inp),
    (inp)
)

static IPSEC_KMOD_METHOD(int, ipsec_kmod_caps, sc,
    capability, METHOD_DECL(const struct ipsec_support *sc, struct mbuf *m,
	u_int cap), METHOD_ARGS(m, cap)
)

int
ipsec_kmod_capability(const struct ipsec_support *sc, struct mbuf *m,
    u_int cap)
{

	/*
	 * Since PF_KEY is build in the kernel, we can use key_havesp()
	 * without taking the lock.
	 */
	if (cap == IPSEC_CAP_OPERABLE)
		return (key_havesp(IPSEC_DIR_INBOUND) != 0 ||
		    key_havesp(IPSEC_DIR_OUTBOUND) != 0);
	return (ipsec_kmod_caps(sc, m, cap));
}
#endif
