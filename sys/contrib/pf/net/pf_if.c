/*	$OpenBSD: pf_if.c,v 1.46 2006/12/13 09:01:59 itojun Exp $ */

/*
 * Copyright 2005 Henning Brauer <henning@openbsd.org>
 * Copyright 2005 Ryan McBride <mcbride@openbsd.org>
 * Copyright (c) 2001 Daniel Hartmeier
 * Copyright (c) 2003 Cedric Berger
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#if defined(__FreeBSD__)
#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#endif

#include <sys/param.h>
#include <sys/systm.h>
#ifdef __FreeBSD__
#include <sys/malloc.h>
#endif
#include <sys/mbuf.h>
#include <sys/filio.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/kernel.h>
#ifndef __FreeBSD__
#include <sys/device.h>
#endif
#include <sys/time.h>

#include <net/if.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>

#include <net/pfvar.h>

#ifdef INET6
#include <netinet/ip6.h>
#endif /* INET6 */

struct pfi_kif		 *pfi_all = NULL;
struct pfi_statehead	  pfi_statehead;
#ifdef __FreeBSD__
uma_zone_t		  pfi_addr_pl;
#else
struct pool		  pfi_addr_pl;
#endif
struct pfi_ifhead	  pfi_ifs;
long			  pfi_update = 1;
struct pfr_addr		 *pfi_buffer;
int			  pfi_buffer_cnt;
int			  pfi_buffer_max;
#ifdef __FreeBSD__
eventhandler_tag	  pfi_attach_cookie = NULL;
eventhandler_tag	  pfi_detach_cookie = NULL;
eventhandler_tag	  pfi_attach_group_cookie = NULL;
eventhandler_tag	  pfi_change_group_cookie = NULL;
eventhandler_tag	  pfi_detach_group_cookie = NULL;
eventhandler_tag	  pfi_ifaddr_event_cookie = NULL;
#endif

void		 pfi_kif_update(struct pfi_kif *);
void		 pfi_dynaddr_update(struct pfi_dynaddr *dyn);
void		 pfi_table_update(struct pfr_ktable *, struct pfi_kif *,
		    int, int);
void		 pfi_kifaddr_update(void *);
void		 pfi_instance_add(struct ifnet *, int, int);
void		 pfi_address_add(struct sockaddr *, int, int);
int		 pfi_if_compare(struct pfi_kif *, struct pfi_kif *);
int		 pfi_skip_if(const char *, struct pfi_kif *);
int		 pfi_unmask(void *);
#ifdef __FreeBSD__
void		 pfi_attach_ifnet_event(void * __unused, struct ifnet *);
void		 pfi_detach_ifnet_event(void * __unused, struct ifnet *);
void		 pfi_attach_group_event(void * __unused, struct ifg_group *);
void		 pfi_change_group_event(void * __unused, char *);
void		 pfi_detach_group_event(void * __unused, struct ifg_group *);
void		 pfi_ifaddr_event(void * __unused, struct ifnet *);

extern struct ifgrouphead ifg_head;
#endif

RB_PROTOTYPE(pfi_ifhead, pfi_kif, pfik_tree, pfi_if_compare);
RB_GENERATE(pfi_ifhead, pfi_kif, pfik_tree, pfi_if_compare);

#define PFI_BUFFER_MAX		0x10000
#define PFI_MTYPE		M_IFADDR

void
pfi_initialize(void)
{
	if (pfi_all != NULL)	/* already initialized */
		return;

	TAILQ_INIT(&pfi_statehead);
#ifndef __FreeBSD__
	pool_init(&pfi_addr_pl, sizeof(struct pfi_dynaddr), 0, 0, 0,
	    "pfiaddrpl", &pool_allocator_nointr);
#endif
	pfi_buffer_max = 64;
	pfi_buffer = malloc(pfi_buffer_max * sizeof(*pfi_buffer),
	    PFI_MTYPE, M_WAITOK);

	if ((pfi_all = pfi_kif_get(IFG_ALL)) == NULL)
		panic("pfi_kif_get for pfi_all failed");

#ifdef __FreeBSD__
	struct ifg_group *ifg;
	struct ifnet *ifp;

	IFNET_RLOCK();
	TAILQ_FOREACH(ifg, &ifg_head, ifg_next)
		pfi_attach_ifgroup(ifg);
	TAILQ_FOREACH(ifp, &ifnet, if_link)
		pfi_attach_ifnet(ifp);
	IFNET_RUNLOCK();

	pfi_attach_cookie = EVENTHANDLER_REGISTER(ifnet_arrival_event,
	    pfi_attach_ifnet_event, NULL, EVENTHANDLER_PRI_ANY);
	pfi_detach_cookie = EVENTHANDLER_REGISTER(ifnet_departure_event,
	    pfi_detach_ifnet_event, NULL, EVENTHANDLER_PRI_ANY);
	pfi_attach_group_cookie = EVENTHANDLER_REGISTER(group_attach_event,
	    pfi_attach_group_event, NULL, EVENTHANDLER_PRI_ANY);
	pfi_change_group_cookie = EVENTHANDLER_REGISTER(group_change_event,
	    pfi_change_group_event, NULL, EVENTHANDLER_PRI_ANY);
	pfi_detach_group_cookie = EVENTHANDLER_REGISTER(group_detach_event,
	    pfi_detach_group_event, NULL, EVENTHANDLER_PRI_ANY);
	pfi_ifaddr_event_cookie = EVENTHANDLER_REGISTER(ifaddr_event,
	    pfi_ifaddr_event, NULL, EVENTHANDLER_PRI_ANY);
#endif
}

struct pfi_kif *
pfi_kif_get(const char *kif_name)
{
	struct pfi_kif		*kif;
	struct pfi_kif_cmp	 s;

	bzero(&s, sizeof(s));
	strlcpy(s.pfik_name, kif_name, sizeof(s.pfik_name));
	if ((kif = RB_FIND(pfi_ifhead, &pfi_ifs, (struct pfi_kif *)&s)) != NULL)
		return (kif);

	/* create new one */
#ifdef __FreeBSD__
	if ((kif = malloc(sizeof(*kif), PFI_MTYPE, M_NOWAIT)) == NULL)
#else
	if ((kif = malloc(sizeof(*kif), PFI_MTYPE, M_DONTWAIT)) == NULL)
#endif
		return (NULL);

	bzero(kif, sizeof(*kif));
	strlcpy(kif->pfik_name, kif_name, sizeof(kif->pfik_name));
	kif->pfik_tzero = time_second;
	TAILQ_INIT(&kif->pfik_dynaddrs);

	RB_INSERT(pfi_ifhead, &pfi_ifs, kif);
	return (kif);
}

void
pfi_kif_ref(struct pfi_kif *kif, enum pfi_kif_refs what)
{
	switch (what) {
	case PFI_KIF_REF_RULE:
		kif->pfik_rules++;
		break;
	case PFI_KIF_REF_STATE:
		if (!kif->pfik_states++)
			TAILQ_INSERT_TAIL(&pfi_statehead, kif, pfik_w_states);
		break;
	default:
		panic("pfi_kif_ref with unknown type");
	}
}

void
pfi_kif_unref(struct pfi_kif *kif, enum pfi_kif_refs what)
{
	if (kif == NULL)
		return;

	switch (what) {
	case PFI_KIF_REF_NONE:
		break;
	case PFI_KIF_REF_RULE:
		if (kif->pfik_rules <= 0) {
			printf("pfi_kif_unref: rules refcount <= 0\n");
			return;
		}
		kif->pfik_rules--;
		break;
	case PFI_KIF_REF_STATE:
		if (kif->pfik_states <= 0) {
			printf("pfi_kif_unref: state refcount <= 0\n");
			return;
		}
		if (!--kif->pfik_states)
			TAILQ_REMOVE(&pfi_statehead, kif, pfik_w_states);
		break;
	default:
		panic("pfi_kif_unref with unknown type");
	}

	if (kif->pfik_ifp != NULL || kif->pfik_group != NULL || kif == pfi_all)
		return;

	if (kif->pfik_rules || kif->pfik_states)
		return;

	RB_REMOVE(pfi_ifhead, &pfi_ifs, kif);
	free(kif, PFI_MTYPE);
}

int
pfi_kif_match(struct pfi_kif *rule_kif, struct pfi_kif *packet_kif)
{
	struct ifg_list	*p;

	if (rule_kif == NULL || rule_kif == packet_kif)
		return (1);

	if (rule_kif->pfik_group != NULL)
		TAILQ_FOREACH(p, &packet_kif->pfik_ifp->if_groups, ifgl_next)
			if (p->ifgl_group == rule_kif->pfik_group)
				return (1);

	return (0);
}

void
pfi_attach_ifnet(struct ifnet *ifp)
{
	struct pfi_kif		*kif;
	int			 s;

	pfi_initialize();
	s = splsoftnet();
	pfi_update++;
	if ((kif = pfi_kif_get(ifp->if_xname)) == NULL)
		panic("pfi_kif_get failed");

	kif->pfik_ifp = ifp;
	ifp->if_pf_kif = (caddr_t)kif;

#ifndef __FreeBSD__
	if ((kif->pfik_ah_cookie = hook_establish(ifp->if_addrhooks, 1,
	    pfi_kifaddr_update, kif)) == NULL)
		panic("pfi_attach_ifnet: cannot allocate '%s' address hook",
		    ifp->if_xname);
#endif

	pfi_kif_update(kif);

	splx(s);
}

void
pfi_detach_ifnet(struct ifnet *ifp)
{
	int			 s;
	struct pfi_kif		*kif;

	if ((kif = (struct pfi_kif *)ifp->if_pf_kif) == NULL)
		return;

	s = splsoftnet();
	pfi_update++;
#ifndef __FreeBSD__
	hook_disestablish(ifp->if_addrhooks, kif->pfik_ah_cookie);
#endif
	pfi_kif_update(kif);

	kif->pfik_ifp = NULL;
	ifp->if_pf_kif = NULL;
	pfi_kif_unref(kif, PFI_KIF_REF_NONE);
	splx(s);
}

void
pfi_attach_ifgroup(struct ifg_group *ifg)
{
	struct pfi_kif	*kif;
	int		 s;

	pfi_initialize();
	s = splsoftnet();
	pfi_update++;
	if ((kif = pfi_kif_get(ifg->ifg_group)) == NULL)
		panic("pfi_kif_get failed");

	kif->pfik_group = ifg;
	ifg->ifg_pf_kif = (caddr_t)kif;

	splx(s);
}

void
pfi_detach_ifgroup(struct ifg_group *ifg)
{
	int		 s;
	struct pfi_kif	*kif;

	if ((kif = (struct pfi_kif *)ifg->ifg_pf_kif) == NULL)
		return;

	s = splsoftnet();
	pfi_update++;

	kif->pfik_group = NULL;
	ifg->ifg_pf_kif = NULL;
	pfi_kif_unref(kif, PFI_KIF_REF_NONE);
	splx(s);
}

void
pfi_group_change(const char *group)
{
	struct pfi_kif		*kif;
	int			 s;

	s = splsoftnet();
	pfi_update++;
	if ((kif = pfi_kif_get(group)) == NULL)
		panic("pfi_kif_get failed");

	pfi_kif_update(kif);

	splx(s);
}

int
pfi_match_addr(struct pfi_dynaddr *dyn, struct pf_addr *a, sa_family_t af)
{
	switch (af) {
#ifdef INET
	case AF_INET:
		switch (dyn->pfid_acnt4) {
		case 0:
			return (0);
		case 1:
			return (PF_MATCHA(0, &dyn->pfid_addr4,
			    &dyn->pfid_mask4, a, AF_INET));
		default:
			return (pfr_match_addr(dyn->pfid_kt, a, AF_INET));
		}
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		switch (dyn->pfid_acnt6) {
		case 0:
			return (0);
		case 1:
			return (PF_MATCHA(0, &dyn->pfid_addr6,
			    &dyn->pfid_mask6, a, AF_INET6));
		default:
			return (pfr_match_addr(dyn->pfid_kt, a, AF_INET6));
		}
		break;
#endif /* INET6 */
	default:
		return (0);
	}
}

int
pfi_dynaddr_setup(struct pf_addr_wrap *aw, sa_family_t af)
{
	struct pfi_dynaddr	*dyn;
	char			 tblname[PF_TABLE_NAME_SIZE];
	struct pf_ruleset	*ruleset = NULL;
	int			 s, rv = 0;

	if (aw->type != PF_ADDR_DYNIFTL)
		return (0);
	if ((dyn = pool_get(&pfi_addr_pl, PR_NOWAIT)) == NULL)
		return (1);
	bzero(dyn, sizeof(*dyn));

	s = splsoftnet();
	if (!strcmp(aw->v.ifname, "self"))
		dyn->pfid_kif = pfi_kif_get(IFG_ALL);
	else
		dyn->pfid_kif = pfi_kif_get(aw->v.ifname);
	if (dyn->pfid_kif == NULL) {
		rv = 1;
		goto _bad;
	}
	pfi_kif_ref(dyn->pfid_kif, PFI_KIF_REF_RULE);

	dyn->pfid_net = pfi_unmask(&aw->v.a.mask);
	if (af == AF_INET && dyn->pfid_net == 32)
		dyn->pfid_net = 128;
	strlcpy(tblname, aw->v.ifname, sizeof(tblname));
	if (aw->iflags & PFI_AFLAG_NETWORK)
		strlcat(tblname, ":network", sizeof(tblname));
	if (aw->iflags & PFI_AFLAG_BROADCAST)
		strlcat(tblname, ":broadcast", sizeof(tblname));
	if (aw->iflags & PFI_AFLAG_PEER)
		strlcat(tblname, ":peer", sizeof(tblname));
	if (aw->iflags & PFI_AFLAG_NOALIAS)
		strlcat(tblname, ":0", sizeof(tblname));
	if (dyn->pfid_net != 128)
		snprintf(tblname + strlen(tblname),
		    sizeof(tblname) - strlen(tblname), "/%d", dyn->pfid_net);
	if ((ruleset = pf_find_or_create_ruleset(PF_RESERVED_ANCHOR)) == NULL) {
		rv = 1;
		goto _bad;
	}

	if ((dyn->pfid_kt = pfr_attach_table(ruleset, tblname)) == NULL) {
		rv = 1;
		goto _bad;
	}

	dyn->pfid_kt->pfrkt_flags |= PFR_TFLAG_ACTIVE;
	dyn->pfid_iflags = aw->iflags;
	dyn->pfid_af = af;

	TAILQ_INSERT_TAIL(&dyn->pfid_kif->pfik_dynaddrs, dyn, entry);
	aw->p.dyn = dyn;
	pfi_kif_update(dyn->pfid_kif);
	splx(s);
	return (0);

_bad:
	if (dyn->pfid_kt != NULL)
		pfr_detach_table(dyn->pfid_kt);
	if (ruleset != NULL)
		pf_remove_if_empty_ruleset(ruleset);
	if (dyn->pfid_kif != NULL)
		pfi_kif_unref(dyn->pfid_kif, PFI_KIF_REF_RULE);
	pool_put(&pfi_addr_pl, dyn);
	splx(s);
	return (rv);
}

void
pfi_kif_update(struct pfi_kif *kif)
{
	struct ifg_list		*ifgl;
	struct pfi_dynaddr	*p;

	/* update all dynaddr */
	TAILQ_FOREACH(p, &kif->pfik_dynaddrs, entry)
		pfi_dynaddr_update(p);

	/* again for all groups kif is member of */
	if (kif->pfik_ifp != NULL)
		TAILQ_FOREACH(ifgl, &kif->pfik_ifp->if_groups, ifgl_next)
			pfi_kif_update((struct pfi_kif *)
			    ifgl->ifgl_group->ifg_pf_kif);
}

void
pfi_dynaddr_update(struct pfi_dynaddr *dyn)
{
	struct pfi_kif		*kif;
	struct pfr_ktable	*kt;

	if (dyn == NULL || dyn->pfid_kif == NULL || dyn->pfid_kt == NULL)
		panic("pfi_dynaddr_update");

	kif = dyn->pfid_kif;
	kt = dyn->pfid_kt;

	if (kt->pfrkt_larg != pfi_update) {
		/* this table needs to be brought up-to-date */
		pfi_table_update(kt, kif, dyn->pfid_net, dyn->pfid_iflags);
		kt->pfrkt_larg = pfi_update;
	}
	pfr_dynaddr_update(kt, dyn);
}

void
pfi_table_update(struct pfr_ktable *kt, struct pfi_kif *kif, int net, int flags)
{
	int			 e, size2 = 0;
	struct ifg_member	*ifgm;

	pfi_buffer_cnt = 0;

	if (kif->pfik_ifp != NULL)
		pfi_instance_add(kif->pfik_ifp, net, flags);
	else if (kif->pfik_group != NULL)
		TAILQ_FOREACH(ifgm, &kif->pfik_group->ifg_members, ifgm_next)
			pfi_instance_add(ifgm->ifgm_ifp, net, flags);

	if ((e = pfr_set_addrs(&kt->pfrkt_t, pfi_buffer, pfi_buffer_cnt, &size2,
	    NULL, NULL, NULL, 0, PFR_TFLAG_ALLMASK)))
		printf("pfi_table_update: cannot set %d new addresses "
		    "into table %s: %d\n", pfi_buffer_cnt, kt->pfrkt_name, e);
}

void
pfi_instance_add(struct ifnet *ifp, int net, int flags)
{
	struct ifaddr	*ia;
	int		 got4 = 0, got6 = 0;
	int		 net2, af;

	if (ifp == NULL)
		return;
	TAILQ_FOREACH(ia, &ifp->if_addrlist, ifa_list) {
		if (ia->ifa_addr == NULL)
			continue;
		af = ia->ifa_addr->sa_family;
		if (af != AF_INET && af != AF_INET6)
			continue;
		if ((flags & PFI_AFLAG_BROADCAST) && af == AF_INET6)
			continue;
		if ((flags & PFI_AFLAG_BROADCAST) &&
		    !(ifp->if_flags & IFF_BROADCAST))
			continue;
		if ((flags & PFI_AFLAG_PEER) &&
		    !(ifp->if_flags & IFF_POINTOPOINT))
			continue;
		if ((flags & PFI_AFLAG_NETWORK) && af == AF_INET6 &&
		    IN6_IS_ADDR_LINKLOCAL(
		    &((struct sockaddr_in6 *)ia->ifa_addr)->sin6_addr))
			continue;
		if (flags & PFI_AFLAG_NOALIAS) {
			if (af == AF_INET && got4)
				continue;
			if (af == AF_INET6 && got6)
				continue;
		}
		if (af == AF_INET)
			got4 = 1;
		else if (af == AF_INET6)
			got6 = 1;
		net2 = net;
		if (net2 == 128 && (flags & PFI_AFLAG_NETWORK)) {
			if (af == AF_INET)
				net2 = pfi_unmask(&((struct sockaddr_in *)
				    ia->ifa_netmask)->sin_addr);
			else if (af == AF_INET6)
				net2 = pfi_unmask(&((struct sockaddr_in6 *)
				    ia->ifa_netmask)->sin6_addr);
		}
		if (af == AF_INET && net2 > 32)
			net2 = 32;
		if (flags & PFI_AFLAG_BROADCAST)
			pfi_address_add(ia->ifa_broadaddr, af, net2);
		else if (flags & PFI_AFLAG_PEER)
			pfi_address_add(ia->ifa_dstaddr, af, net2);
		else
			pfi_address_add(ia->ifa_addr, af, net2);
	}
}

void
pfi_address_add(struct sockaddr *sa, int af, int net)
{
	struct pfr_addr	*p;
	int		 i;

	if (pfi_buffer_cnt >= pfi_buffer_max) {
		int		 new_max = pfi_buffer_max * 2;

		if (new_max > PFI_BUFFER_MAX) {
			printf("pfi_address_add: address buffer full (%d/%d)\n",
			    pfi_buffer_cnt, PFI_BUFFER_MAX);
			return;
		}
		p = malloc(new_max * sizeof(*pfi_buffer), PFI_MTYPE,
#ifdef __FreeBSD__
		    M_NOWAIT);
#else
		    M_DONTWAIT);
#endif
		if (p == NULL) {
			printf("pfi_address_add: no memory to grow buffer "
			    "(%d/%d)\n", pfi_buffer_cnt, PFI_BUFFER_MAX);
			return;
		}
		memcpy(pfi_buffer, p, pfi_buffer_cnt * sizeof(*pfi_buffer));
		/* no need to zero buffer */
		free(pfi_buffer, PFI_MTYPE);
		pfi_buffer = p;
		pfi_buffer_max = new_max;
	}
	if (af == AF_INET && net > 32)
		net = 128;
	p = pfi_buffer + pfi_buffer_cnt++;
	bzero(p, sizeof(*p));
	p->pfra_af = af;
	p->pfra_net = net;
	if (af == AF_INET)
		p->pfra_ip4addr = ((struct sockaddr_in *)sa)->sin_addr;
	else if (af == AF_INET6) {
		p->pfra_ip6addr = ((struct sockaddr_in6 *)sa)->sin6_addr;
		if (IN6_IS_SCOPE_EMBED(&p->pfra_ip6addr))
			p->pfra_ip6addr.s6_addr16[1] = 0;
	}
	/* mask network address bits */
	if (net < 128)
		((caddr_t)p)[p->pfra_net/8] &= ~(0xFF >> (p->pfra_net%8));
	for (i = (p->pfra_net+7)/8; i < sizeof(p->pfra_u); i++)
		((caddr_t)p)[i] = 0;
}

void
pfi_dynaddr_remove(struct pf_addr_wrap *aw)
{
	int	s;

	if (aw->type != PF_ADDR_DYNIFTL || aw->p.dyn == NULL ||
	    aw->p.dyn->pfid_kif == NULL || aw->p.dyn->pfid_kt == NULL)
		return;

	s = splsoftnet();
	TAILQ_REMOVE(&aw->p.dyn->pfid_kif->pfik_dynaddrs, aw->p.dyn, entry);
	pfi_kif_unref(aw->p.dyn->pfid_kif, PFI_KIF_REF_RULE);
	aw->p.dyn->pfid_kif = NULL;
	pfr_detach_table(aw->p.dyn->pfid_kt);
	aw->p.dyn->pfid_kt = NULL;
	pool_put(&pfi_addr_pl, aw->p.dyn);
	aw->p.dyn = NULL;
	splx(s);
}

void
pfi_dynaddr_copyout(struct pf_addr_wrap *aw)
{
	if (aw->type != PF_ADDR_DYNIFTL || aw->p.dyn == NULL ||
	    aw->p.dyn->pfid_kif == NULL)
		return;
	aw->p.dyncnt = aw->p.dyn->pfid_acnt4 + aw->p.dyn->pfid_acnt6;
}

void
pfi_kifaddr_update(void *v)
{
	int			 s;
	struct pfi_kif		*kif = (struct pfi_kif *)v;

	s = splsoftnet();
	pfi_update++;
	pfi_kif_update(kif);
	splx(s);
}

int
pfi_if_compare(struct pfi_kif *p, struct pfi_kif *q)
{
	return (strncmp(p->pfik_name, q->pfik_name, IFNAMSIZ));
}

void
pfi_fill_oldstatus(struct pf_status *pfs)
{
	struct pfi_kif		*p;
	struct pfi_kif_cmp 	 key;
	int			 i, j, k, s;

	strlcpy(key.pfik_name, pfs->ifname, sizeof(key.pfik_name));
	s = splsoftnet();
	p = RB_FIND(pfi_ifhead, &pfi_ifs, (struct pfi_kif *)&key);
	if (p == NULL) {
		splx(s);
		return;
	}
	bzero(pfs->pcounters, sizeof(pfs->pcounters));
	bzero(pfs->bcounters, sizeof(pfs->bcounters));
	for (i = 0; i < 2; i++)
		for (j = 0; j < 2; j++)
			for (k = 0; k < 2; k++) {
				pfs->pcounters[i][j][k] =
					p->pfik_packets[i][j][k];
				pfs->bcounters[i][j] +=
					p->pfik_bytes[i][j][k];
			}
	splx(s);
}

int
pfi_clr_istats(const char *name)
{
	struct pfi_kif	*p;
	int		 s;

	s = splsoftnet();
	RB_FOREACH(p, pfi_ifhead, &pfi_ifs) {
		if (pfi_skip_if(name, p))
			continue;
		bzero(p->pfik_packets, sizeof(p->pfik_packets));
		bzero(p->pfik_bytes, sizeof(p->pfik_bytes));
		p->pfik_tzero = time_second;
	}
	splx(s);

	return (0);
}

int
pfi_get_ifaces(const char *name, struct pfi_kif *buf, int *size)
{
	struct pfi_kif	*p, *nextp;
	int		 s, n = 0;
#ifdef __FreeBSD__
	int		 error;
#endif

	s = splsoftnet();
	for (p = RB_MIN(pfi_ifhead, &pfi_ifs); p; p = nextp) {
		nextp = RB_NEXT(pfi_ifhead, &pfi_ifs, p);
		if (pfi_skip_if(name, p))
			continue;
		if (*size > n++) {
			if (!p->pfik_tzero)
				p->pfik_tzero = time_second;
			pfi_kif_ref(p, PFI_KIF_REF_RULE);
#ifdef __FreeBSD__
			PF_COPYOUT(p, buf++, sizeof(*buf), error);
			if (error) {
#else
			if (copyout(p, buf++, sizeof(*buf))) {
#endif
				pfi_kif_unref(p, PFI_KIF_REF_RULE);
				splx(s);
				return (EFAULT);
			}
			nextp = RB_NEXT(pfi_ifhead, &pfi_ifs, p);
			pfi_kif_unref(p, PFI_KIF_REF_RULE);
		}
	}
	splx(s);
	*size = n;
	return (0);
}

int
pfi_skip_if(const char *filter, struct pfi_kif *p)
{
	int	n;

	if (filter == NULL || !*filter)
		return (0);
	if (!strcmp(p->pfik_name, filter))
		return (0);	/* exact match */
	n = strlen(filter);
	if (n < 1 || n >= IFNAMSIZ)
		return (1);	/* sanity check */
	if (filter[n-1] >= '0' && filter[n-1] <= '9')
		return (1);	/* only do exact match in that case */
	if (strncmp(p->pfik_name, filter, n))
		return (1);	/* prefix doesn't match */
	return (p->pfik_name[n] < '0' || p->pfik_name[n] > '9');
}

int
pfi_set_flags(const char *name, int flags)
{
	struct pfi_kif	*p;
	int		 s;

	s = splsoftnet();
	RB_FOREACH(p, pfi_ifhead, &pfi_ifs) {
		if (pfi_skip_if(name, p))
			continue;
		p->pfik_flags |= flags;
	}
	splx(s);
	return (0);
}

int
pfi_clear_flags(const char *name, int flags)
{
	struct pfi_kif	*p;
	int		 s;

	s = splsoftnet();
	RB_FOREACH(p, pfi_ifhead, &pfi_ifs) {
		if (pfi_skip_if(name, p))
			continue;
		p->pfik_flags &= ~flags;
	}
	splx(s);
	return (0);
}

/* from pf_print_state.c */
int
pfi_unmask(void *addr)
{
	struct pf_addr *m = addr;
	int i = 31, j = 0, b = 0;
	u_int32_t tmp;

	while (j < 4 && m->addr32[j] == 0xffffffff) {
		b += 32;
		j++;
	}
	if (j < 4) {
		tmp = ntohl(m->addr32[j]);
		for (i = 31; tmp & (1 << i); --i)
			b++;
	}
	return (b);
}

#ifdef __FreeBSD__
void
pfi_attach_ifnet_event(void *arg __unused, struct ifnet *ifp)
{
	PF_LOCK();
	pfi_attach_ifnet(ifp);
	PF_UNLOCK();
}

void
pfi_detach_ifnet_event(void *arg __unused, struct ifnet *ifp)
{
	PF_LOCK();
	pfi_detach_ifnet(ifp);
	PF_UNLOCK();
}

void
pfi_attach_group_event(void *arg __unused, struct ifg_group *ifg)
{
	PF_LOCK();
	pfi_attach_ifgroup(ifg);
	PF_UNLOCK();
}

void
pfi_change_group_event(void *arg __unused, char *gname)
{
	PF_LOCK();
	pfi_group_change(gname);
	PF_UNLOCK();
}

void
pfi_detach_group_event(void *arg __unused, struct ifg_group *ifg)
{
	PF_LOCK();
	pfi_detach_ifgroup(ifg);
	PF_UNLOCK();
}

void
pfi_ifaddr_event(void *arg __unused, struct ifnet *ifp)
{
	PF_LOCK();
	if (ifp && ifp->if_pf_kif)
		pfi_kifaddr_update(ifp->if_pf_kif);
	PF_UNLOCK();
}
#endif /* __FreeBSD__ */
