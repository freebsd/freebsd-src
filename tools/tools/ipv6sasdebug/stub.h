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
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/mutex.h>

#define	_SYS_SYSTM_H_
#define	_SYS_RMLOCK_H_
#define	_NET_IF_LLATBL_H_
#define	_NETINET_IN_VAR_H_
#define	_NETINET6_IN6_VAR_H_
#define	_NETINET6_ND6_H_
#define	_NET_IF_VAR_H_

#include <net/if.h>
#include <net/if_dl.h>
#define	_WANT_RTENTRY
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* net/vnet.h */
#define	VNET_DEFINE(type, var)		type var
#define	VNET(var)			var
#define	IS_DEFAULT_VNET(v)		1

/* netinet6/nd6.h */
#define	ND_IFINFO(ifp)		(&(ifp)->if_ndifinfo)
#define	ND6_IFF_PERFORMNUD	0x1
#define	ND6_IFF_ACCEPT_RTADV	0x2
#define	ND6_IFF_IFDISABLED	0x8
#define	ND6_IFF_DONT_SET_IFROUTE 0x10
#define	ND6_IFF_AUTO_LINKLOCAL	0x20
#define	ND6_IFF_NO_RADR		0x40
#define	ND6_IFF_NO_PREFER_IFACE	0x80

struct nd_ifinfo {
	uint32_t		linkmtu;
	uint32_t		maxmtu;
	uint32_t		flags;
	uint8_t			chlim;
};

/* net/if_var.h */
TAILQ_HEAD(ifaddrhead, ifaddr);
struct ifnet {
	uint32_t		if_index;
	int			if_flags;

	char			if_xname[IFNAMSIZ];
	struct ifaddrhead	if_addrhead;
	struct nd_ifinfo	if_ndifinfo;
	SLIST_ENTRY(ifnet)	if_link;
};

struct ifaddr {
	struct sockaddr		*ifa_addr;
	struct ifnet		*ifa_ifp;
	uint8_t			ifa_carp;
	TAILQ_ENTRY(ifaddr)	ifa_link;
};

/* netinet6/in6.h */
#define	IN6_IS_ADDR_MC_INTFACELOCAL	IN6_IS_ADDR_MC_NODELOCAL
#define	IPV6_ADDR_SCOPE_LINKLOCAL	__IPV6_ADDR_SCOPE_LINKLOCAL
#define	IFA6_IS_DEPRECATED(ia)	\
    ((ia)->ia6_lifetime.ia6t_preferred > (ia)->ia6_lifetime.ia6t_pltime)
#define	IFA6_IS_VALID(ia) \
    ((ia)->ia6_lifetime.ia6t_expire > (ia)->ia6_lifetime.ia6t_vltime)
#define	s6_addr32			__u6_addr.__u6_addr32

/* netinet6/in6_var.h */
#define IN6_IFF_ANYCAST		0x01
#define IN6_IFF_TENTATIVE	0x02
#define IN6_IFF_DUPLICATED	0x04
#define IN6_IFF_DETACHED	0x08
#define IN6_IFF_DEPRECATED	0x10
#define IN6_IFF_NODAD		0x20
#define IN6_IFF_AUTOCONF	0x40
#define IN6_IFF_TEMPORARY	0x80
#define	IN6_IFF_PREFER_SOURCE	0x0100
#define IN6_IFF_NOPFX		0x8000
#define IN6_IFF_NOTREADY	(IN6_IFF_TENTATIVE|IN6_IFF_DUPLICATED)
#define	IN6_MASK_ADDR(a, m)     do { \
	(a)->s6_addr32[0] &= (m)->s6_addr32[0]; \
	(a)->s6_addr32[1] &= (m)->s6_addr32[1]; \
	(a)->s6_addr32[2] &= (m)->s6_addr32[2]; \
	(a)->s6_addr32[3] &= (m)->s6_addr32[3]; \
} while (0)

struct in6_addrlifetime {
	time_t ia6t_expire;	/* valid lifetime expiration time */
	time_t ia6t_preferred;	/* preferred lifetime expiration time */
	u_int32_t ia6t_vltime;	/* valid lifetime */
	u_int32_t ia6t_pltime;	/* prefix lifetime */
};

TAILQ_HEAD(in6_ifaddrhead, in6_ifaddr);
struct in6_ifaddr {
	struct ifaddr		ia_ifa;
	TAILQ_ENTRY(in6_ifaddr)	ia_link;
	struct sockaddr_in6	ia_addr;
	struct in6_addrlifetime	ia6_lifetime;
	int			ia6_flags;
#define	ia_ifp			ia_ifa.ifa_ifp
};

#define IA6_IN6(ia)		(&((ia)->ia_addr.sin6_addr))
#define IA6_SIN6(ia)		(&((ia)->ia_addr))
#define	SIOCAADDRCTL_POLICY	0
#define	SIOCDADDRCTL_POLICY	1
#define IN6_ARE_SCOPE_CMP(a,b) ((a)-(b))
#define IN6_ARE_SCOPE_EQUAL(a,b) ((a)==(b))

struct in6_addrpolicy {
	struct sockaddr_in6	addr;
	struct sockaddr_in6	addrmask;
	int			preced;
	int			label;
	uint64_t		use;
};

/* netinet6/ip6_var.h */
#define	IP6STAT_INC(c)

struct ucred {
};

#define	IPV6SASDEBUG(fmt, ...)	printf("%s: " fmt "\n", __func__, ##__VA_ARGS__)
#define	KASSERT(exp, msg)
#define	RO_RTFREE(ro)
#define	RTFREE(rt)
#define	INP_LOCK_ASSERT(pcb)
#define	INP_WLOCK_ASSERT(pcb)
#define	IF_ADDR_RLOCK(ifp)
#define	IF_ADDR_RUNLOCK(ifp)
#define	IN6_IFADDR_RLOCK()
#define	IN6_IFADDR_RUNLOCK()
#define	SYSCTL_DECL(n)
#define	SYSCTL_NODE(n, i, m, f, h, d)	int sysctl_ ## m
#define	SYSCTL_HANDLER_ARGS	struct sysctl_req *req
#define	SYSCTL_OUT(r, p, l)	0
struct sysctl_req {
	void		*oldptr;
	size_t		oldlen;
	void		*newptr;
	size_t		newlen;
};

#define	rw_assert(l,w)

#define	mtx_init(m,n,t,o)
#define	mtx_lock(m)
#define	mtx_unlock(m)
#define	mtx_assert(m, f)

#define	sx_init(l, n)
#define	sx_slock(l)
#define	sx_sunlock(l)
#define	sx_xlock(l)
#define	sx_xunlock(l)

#define	malloc(sz, t, f)	malloc((sz))
#define	free(p, t)		free((p))

struct ifnet *ifnet_byindex(uint32_t);
int ifnet_getflags(const char *);
void ifnet_getcarpstatus(const char *);
void ifnet_getndinfo(const char *, uint32_t*, uint32_t*, uint32_t*, uint8_t*);
int addr_getflags(const char *, struct sockaddr_in6 *);
void addr_getlifetime(const char *, struct sockaddr_in6 *, struct in6_addrlifetime *);
void v_getsysctl(void);
int carp_master(uint8_t);

void in6_rtalloc(struct route_in6 *, u_int);

const char *ip6_sprintf(char *, const struct in6_addr *);
struct in6_ifaddr *in6ifa_ifwithaddr(const struct in6_addr *, uint32_t);
void ifa_free(void *);
void ifa_ref(void *);

int in6_addrscope(const struct in6_addr *);
uint32_t in6_getscopezone(const struct ifnet *, int);
int in6_matchlen(struct in6_addr *, struct in6_addr *);
int in6_mask2len(struct in6_addr *, u_char *);
int prison_check_ip6(struct ucred *, const struct sockaddr_in6 *);
int prison_local_ip6(struct ucred *, const struct sockaddr_in6 *, int);
int prison_saddrsel_ip6(struct ucred *, const struct sockaddr_in6 *);
struct ifnet *in6_getlinkifnet(uint32_t);
int in_pcb_lport(struct inpcb *, struct in_addr *, u_short *, struct ucred *, int);
int in_pcbinshash(struct inpcb *);
int ifa_preferred(struct ifaddr *, struct ifaddr *);

void addrsel_policy_init(void);
void addrsel_policy_populate(int (struct in6_addrpolicy *), int);
int in6_src_ioctl(u_long, caddr_t);
int in6_selectsrc(struct sockaddr_in6 *, struct ip6_pktopts *,
    struct inpcb *, struct route_in6 *, struct ucred *,
    struct ifnet **, struct in6_addr *);
extern int V_ip6_use_deprecated;
extern int V_ip6_prefer_tempaddr;
extern int V_ip6_defhlim;
extern const struct sockaddr_in6 sa6_any;
extern struct in6_ifaddrhead V_in6_ifaddrhead;
extern int has_carp;
