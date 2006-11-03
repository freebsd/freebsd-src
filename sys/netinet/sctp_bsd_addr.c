/*-
 * Copyright (c) 2001-2006, Cisco Systems, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * a) Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 * b) Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the distribution.
 *
 * c) Neither the name of Cisco Systems, Inc. nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/* $KAME: sctp_output.c,v 1.46 2005/03/06 16:04:17 itojun Exp $	 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ipsec.h"
#include "opt_compat.h"
#include "opt_inet6.h"
#include "opt_inet.h"
#include "opt_sctp.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/resourcevar.h>
#include <sys/uio.h>
#ifdef INET6
#include <sys/domain.h>
#endif

#include <sys/limits.h>
#include <machine/cpu.h>

#include <net/if.h>
#include <net/if_types.h>

#include <net/if_var.h>

#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#include <netinet/ip_var.h>

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/scope6_var.h>
#include <netinet6/nd6.h>

#include <netinet6/in6_pcb.h>

#include <netinet/icmp6.h>

#endif				/* INET6 */



#ifndef in6pcb
#define in6pcb		inpcb
#endif


#ifdef IPSEC
#include <netinet6/ipsec.h>
#include <netkey/key.h>
#endif				/* IPSEC */

#include <netinet/sctp_os.h>
#include <netinet/sctp_var.h>
#include <netinet/sctp_pcb.h>
#include <netinet/sctp_header.h>
#include <netinet/sctputil.h>
#include <netinet/sctp_output.h>
#include <netinet/sctp_bsd_addr.h>
#include <netinet/sctp_uio.h>
#include <netinet/sctputil.h>
#include <netinet/sctp_timer.h>
#include <netinet/sctp_asconf.h>
#include <netinet/sctp_indata.h>

/* XXX
 * This module needs to be rewritten with an eye towards getting
 * rid of the user of ifa.. and use another list method George
 * as told me of.
 */

#ifdef SCTP_DEBUG
extern uint32_t sctp_debug_on;

#endif

static struct sockaddr_in *
sctp_is_v4_ifa_addr_prefered(struct ifaddr *ifa, uint8_t loopscope, uint8_t ipv4_scope, uint8_t * sin_loop, uint8_t * sin_local)
{
	struct sockaddr_in *sin;

	/*
	 * Here we determine if its a prefered address. A prefered address
	 * means it is the same scope or higher scope then the destination.
	 * L = loopback, P = private, G = global
	 * ----------------------------------------- src    |      dest |
	 * result ----------------------------------------- L     | L |
	 * yes ----------------------------------------- P     | L |    yes
	 * ----------------------------------------- G     | L |    yes
	 * ----------------------------------------- L     | P |    no
	 * ----------------------------------------- P     | P |    yes
	 * ----------------------------------------- G     | P |    no
	 * ----------------------------------------- L     | G |    no
	 * ----------------------------------------- P     | G |    no
	 * ----------------------------------------- G     | G |    yes
	 * -----------------------------------------
	 */

	if (ifa->ifa_addr->sa_family != AF_INET) {
		/* forget non-v4 */
		return (NULL);
	}
	/* Ok the address may be ok */
	sin = (struct sockaddr_in *)ifa->ifa_addr;
	if (sin->sin_addr.s_addr == 0) {
		return (NULL);
	}
	*sin_local = *sin_loop = 0;
	if ((ifa->ifa_ifp->if_type == IFT_LOOP) ||
	    (IN4_ISLOOPBACK_ADDRESS(&sin->sin_addr))) {
		*sin_loop = 1;
		*sin_local = 1;
	}
	if ((IN4_ISPRIVATE_ADDRESS(&sin->sin_addr))) {
		*sin_local = 1;
	}
	if (!loopscope && *sin_loop) {
		/* Its a loopback address and we don't have loop scope */
		return (NULL);
	}
	if (!ipv4_scope && *sin_local) {
		/*
		 * Its a private address, and we don't have private address
		 * scope
		 */
		return (NULL);
	}
	if (((ipv4_scope == 0) && (loopscope == 0)) && (*sin_local)) {
		/* its a global src and a private dest */
		return (NULL);
	}
	/* its a prefered address */
	return (sin);
}

static struct sockaddr_in *
sctp_is_v4_ifa_addr_acceptable(struct ifaddr *ifa, uint8_t loopscope, uint8_t ipv4_scope, uint8_t * sin_loop, uint8_t * sin_local)
{
	struct sockaddr_in *sin;

	/*
	 * Here we determine if its a acceptable address. A acceptable
	 * address means it is the same scope or higher scope but we can
	 * allow for NAT which means its ok to have a global dest and a
	 * private src.
	 * 
	 * L = loopback, P = private, G = global
	 * ----------------------------------------- src    |      dest |
	 * result ----------------------------------------- L     | L |
	 * yes ----------------------------------------- P     | L |    yes
	 * ----------------------------------------- G     | L |    yes
	 * ----------------------------------------- L     | P |    no
	 * ----------------------------------------- P     | P |    yes
	 * ----------------------------------------- G     | P |    yes -
	 * probably this won't work.
	 * ----------------------------------------- L     |       G       |
	 * no ----------------------------------------- P     |       G |
	 * yes ----------------------------------------- G     |       G |
	 * yes -----------------------------------------
	 */

	if (ifa->ifa_addr->sa_family != AF_INET) {
		/* forget non-v4 */
		return (NULL);
	}
	/* Ok the address may be ok */
	sin = (struct sockaddr_in *)ifa->ifa_addr;
	if (sin->sin_addr.s_addr == 0) {
		return (NULL);
	}
	*sin_local = *sin_loop = 0;
	if ((ifa->ifa_ifp->if_type == IFT_LOOP) ||
	    (IN4_ISLOOPBACK_ADDRESS(&sin->sin_addr))) {
		*sin_loop = 1;
		*sin_local = 1;
	}
	if ((IN4_ISPRIVATE_ADDRESS(&sin->sin_addr))) {
		*sin_local = 1;
	}
	if (!loopscope && *sin_loop) {
		/* Its a loopback address and we don't have loop scope */
		return (NULL);
	}
	/* its an acceptable address */
	return (sin);
}

/*
 * This treats the address list on the ep as a restricted list (negative
 * list). If a the passed address is listed, then the address is NOT allowed
 * on the association.
 */
int
sctp_is_addr_restricted(struct sctp_tcb *stcb, struct sockaddr *addr)
{
	struct sctp_laddr *laddr;

#ifdef SCTP_DEBUG
	int cnt = 0;

#endif
	if (stcb == NULL) {
		/* There are no restrictions, no TCB :-) */
		return (0);
	}
#ifdef SCTP_DEBUG
	LIST_FOREACH(laddr, &stcb->asoc.sctp_local_addr_list, sctp_nxt_addr) {
		cnt++;
	}
	if (sctp_debug_on & SCTP_DEBUG_OUTPUT4) {
		printf("There are %d addresses on the restricted list\n", cnt);
	}
	cnt = 0;
#endif
	LIST_FOREACH(laddr, &stcb->asoc.sctp_local_addr_list, sctp_nxt_addr) {
		if (laddr->ifa == NULL) {
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_OUTPUT1) {
				printf("Help I have fallen and I can't get up!\n");
			}
#endif
			continue;
		}
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_OUTPUT4) {
			cnt++;
			printf("Restricted address[%d]:", cnt);
			sctp_print_address(laddr->ifa->ifa_addr);
		}
#endif
		if (sctp_cmpaddr(addr, laddr->ifa->ifa_addr) == 1) {
			/* Yes it is on the list */
			return (1);
		}
	}
	return (0);
}

static int
sctp_is_addr_in_ep(struct sctp_inpcb *inp, struct ifaddr *ifa)
{
	struct sctp_laddr *laddr;

	if (ifa == NULL)
		return (0);
	LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {
		if (laddr->ifa == NULL) {
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_OUTPUT1) {
				printf("Help I have fallen and I can't get up!\n");
			}
#endif
			continue;
		}
		if (laddr->ifa->ifa_addr == NULL)
			continue;
		if (laddr->ifa == ifa)
			/* same pointer */
			return (1);
		if (laddr->ifa->ifa_addr->sa_family != ifa->ifa_addr->sa_family) {
			/* skip non compatible address comparison */
			continue;
		}
		if (sctp_cmpaddr(ifa->ifa_addr, laddr->ifa->ifa_addr) == 1) {
			/* Yes it is restricted */
			return (1);
		}
	}
	return (0);
}



static struct in_addr
sctp_choose_v4_boundspecific_inp(struct sctp_inpcb *inp,
    struct route *ro,
    uint8_t ipv4_scope,
    uint8_t loopscope)
{
	struct in_addr ans;
	struct sctp_laddr *laddr;
	struct sockaddr_in *sin;
	struct ifnet *ifn;
	struct ifaddr *ifa;
	uint8_t sin_loop, sin_local;
	struct rtentry *rt;

	/*
	 * first question, is the ifn we will emit on in our list, if so, we
	 * want that one.
	 */
	rt = ro->ro_rt;
	ifn = rt->rt_ifp;
	if (ifn) {
		/* is a prefered one on the interface we route out? */
		TAILQ_FOREACH(ifa, &ifn->if_addrlist, ifa_list) {
			sin = sctp_is_v4_ifa_addr_prefered(ifa, loopscope, ipv4_scope, &sin_loop, &sin_local);
			if (sin == NULL)
				continue;
			if (sctp_is_addr_in_ep(inp, ifa)) {
				return (sin->sin_addr);
			}
		}
		/* is an acceptable one on the interface we route out? */
		TAILQ_FOREACH(ifa, &ifn->if_addrlist, ifa_list) {
			sin = sctp_is_v4_ifa_addr_acceptable(ifa, loopscope, ipv4_scope, &sin_loop, &sin_local);
			if (sin == NULL)
				continue;
			if (sctp_is_addr_in_ep(inp, ifa)) {
				return (sin->sin_addr);
			}
		}
	}
	/* ok, what about a prefered address in the inp */
	for (laddr = LIST_FIRST(&inp->sctp_addr_list);
	    laddr && (laddr != inp->next_addr_touse);
	    laddr = LIST_NEXT(laddr, sctp_nxt_addr)) {
		if (laddr->ifa == NULL) {
			/* address has been removed */
			continue;
		}
		sin = sctp_is_v4_ifa_addr_prefered(laddr->ifa, loopscope, ipv4_scope, &sin_loop, &sin_local);
		if (sin == NULL)
			continue;
		return (sin->sin_addr);

	}
	/* ok, what about an acceptable address in the inp */
	for (laddr = LIST_FIRST(&inp->sctp_addr_list);
	    laddr && (laddr != inp->next_addr_touse);
	    laddr = LIST_NEXT(laddr, sctp_nxt_addr)) {
		if (laddr->ifa == NULL) {
			/* address has been removed */
			continue;
		}
		sin = sctp_is_v4_ifa_addr_acceptable(laddr->ifa, loopscope, ipv4_scope, &sin_loop, &sin_local);
		if (sin == NULL)
			continue;
		return (sin->sin_addr);

	}

	/*
	 * no address bound can be a source for the destination we are in
	 * trouble
	 */
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_OUTPUT1) {
		printf("Src address selection for EP, no acceptable src address found for address\n");
	}
#endif
	RTFREE(ro->ro_rt);
	ro->ro_rt = NULL;
	memset(&ans, 0, sizeof(ans));
	return (ans);
}



static struct in_addr
sctp_choose_v4_boundspecific_stcb(struct sctp_inpcb *inp,
    struct sctp_tcb *stcb,
    struct sctp_nets *net,
    struct route *ro,
    uint8_t ipv4_scope,
    uint8_t loopscope,
    int non_asoc_addr_ok)
{
	/*
	 * Here we have two cases, bound all asconf allowed. bound all
	 * asconf not allowed.
	 * 
	 */
	struct sctp_laddr *laddr, *starting_point;
	struct in_addr ans;
	struct ifnet *ifn;
	struct ifaddr *ifa;
	uint8_t sin_loop, sin_local, start_at_beginning = 0;
	struct sockaddr_in *sin;
	struct rtentry *rt;

	/*
	 * first question, is the ifn we will emit on in our list, if so, we
	 * want that one.
	 */
	rt = ro->ro_rt;
	ifn = rt->rt_ifp;

	if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_DO_ASCONF)) {
		/*
		 * Here we use the list of addresses on the endpoint. Then
		 * the addresses listed on the "restricted" list is just
		 * that, address that have not been added and can't be used
		 * (unless the non_asoc_addr_ok is set).
		 */
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_OUTPUT1) {
			printf("Have a STCB - asconf allowed, not bound all have a netgative list\n");
		}
#endif
		/*
		 * first question, is the ifn we will emit on in our list,
		 * if so, we want that one.
		 */
		if (ifn) {
			/* first try for an prefered address on the ep */
			TAILQ_FOREACH(ifa, &ifn->if_addrlist, ifa_list) {
				if (sctp_is_addr_in_ep(inp, ifa)) {
					sin = sctp_is_v4_ifa_addr_prefered(ifa, loopscope, ipv4_scope, &sin_loop, &sin_local);
					if (sin == NULL)
						continue;
					if ((non_asoc_addr_ok == 0) &&
					    (sctp_is_addr_restricted(stcb, (struct sockaddr *)sin))) {
						/* on the no-no list */
						continue;
					}
					return (sin->sin_addr);
				}
			}
			/* next try for an acceptable address on the ep */
			TAILQ_FOREACH(ifa, &ifn->if_addrlist, ifa_list) {
				if (sctp_is_addr_in_ep(inp, ifa)) {
					sin = sctp_is_v4_ifa_addr_acceptable(ifa, loopscope, ipv4_scope, &sin_loop, &sin_local);
					if (sin == NULL)
						continue;
					if ((non_asoc_addr_ok == 0) &&
					    (sctp_is_addr_restricted(stcb, (struct sockaddr *)sin))) {
						/* on the no-no list */
						continue;
					}
					return (sin->sin_addr);
				}
			}

		}
		/*
		 * if we can't find one like that then we must look at all
		 * addresses bound to pick one at first prefereable then
		 * secondly acceptable.
		 */
		starting_point = stcb->asoc.last_used_address;
sctpv4_from_the_top:
		if (stcb->asoc.last_used_address == NULL) {
			start_at_beginning = 1;
			stcb->asoc.last_used_address = LIST_FIRST(&inp->sctp_addr_list);
		}
		/* search beginning with the last used address */
		for (laddr = stcb->asoc.last_used_address; laddr;
		    laddr = LIST_NEXT(laddr, sctp_nxt_addr)) {
			if (laddr->ifa == NULL) {
				/* address has been removed */
				continue;
			}
			sin = sctp_is_v4_ifa_addr_prefered(laddr->ifa, loopscope, ipv4_scope, &sin_loop, &sin_local);
			if (sin == NULL)
				continue;
			if ((non_asoc_addr_ok == 0) &&
			    (sctp_is_addr_restricted(stcb, (struct sockaddr *)sin))) {
				/* on the no-no list */
				continue;
			}
			return (sin->sin_addr);

		}
		if (start_at_beginning == 0) {
			stcb->asoc.last_used_address = NULL;
			goto sctpv4_from_the_top;
		}
		/* now try for any higher scope than the destination */
		stcb->asoc.last_used_address = starting_point;
		start_at_beginning = 0;
sctpv4_from_the_top2:
		if (stcb->asoc.last_used_address == NULL) {
			start_at_beginning = 1;
			stcb->asoc.last_used_address = LIST_FIRST(&inp->sctp_addr_list);
		}
		/* search beginning with the last used address */
		for (laddr = stcb->asoc.last_used_address; laddr;
		    laddr = LIST_NEXT(laddr, sctp_nxt_addr)) {
			if (laddr->ifa == NULL) {
				/* address has been removed */
				continue;
			}
			sin = sctp_is_v4_ifa_addr_acceptable(laddr->ifa, loopscope, ipv4_scope, &sin_loop, &sin_local);
			if (sin == NULL)
				continue;
			if ((non_asoc_addr_ok == 0) &&
			    (sctp_is_addr_restricted(stcb, (struct sockaddr *)sin))) {
				/* on the no-no list */
				continue;
			}
			return (sin->sin_addr);
		}
		if (start_at_beginning == 0) {
			stcb->asoc.last_used_address = NULL;
			goto sctpv4_from_the_top2;
		}
	} else {
		/*
		 * Here we have an address list on the association, thats
		 * the only valid source addresses that we can use.
		 */
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_OUTPUT1) {
			printf("Have a STCB - no asconf allowed, not bound all have a postive list\n");
		}
#endif
		/*
		 * First look at all addresses for one that is on the
		 * interface we route out
		 */
		LIST_FOREACH(laddr, &stcb->asoc.sctp_local_addr_list,
		    sctp_nxt_addr) {
			if (laddr->ifa == NULL) {
				/* address has been removed */
				continue;
			}
			sin = sctp_is_v4_ifa_addr_prefered(laddr->ifa, loopscope, ipv4_scope, &sin_loop, &sin_local);
			if (sin == NULL)
				continue;
			/*
			 * first question, is laddr->ifa an address
			 * associated with the emit interface
			 */
			if (ifn) {
				TAILQ_FOREACH(ifa, &ifn->if_addrlist, ifa_list) {
					if (laddr->ifa == ifa) {
						sin = (struct sockaddr_in *)laddr->ifa->ifa_addr;
						return (sin->sin_addr);
					}
					if (sctp_cmpaddr(ifa->ifa_addr, laddr->ifa->ifa_addr) == 1) {
						sin = (struct sockaddr_in *)laddr->ifa->ifa_addr;
						return (sin->sin_addr);
					}
				}
			}
		}
		/* what about an acceptable one on the interface? */
		LIST_FOREACH(laddr, &stcb->asoc.sctp_local_addr_list,
		    sctp_nxt_addr) {
			if (laddr->ifa == NULL) {
				/* address has been removed */
				continue;
			}
			sin = sctp_is_v4_ifa_addr_acceptable(laddr->ifa, loopscope, ipv4_scope, &sin_loop, &sin_local);
			if (sin == NULL)
				continue;
			/*
			 * first question, is laddr->ifa an address
			 * associated with the emit interface
			 */
			if (ifn) {
				TAILQ_FOREACH(ifa, &ifn->if_addrlist, ifa_list) {
					if (laddr->ifa == ifa) {
						sin = (struct sockaddr_in *)laddr->ifa->ifa_addr;
						return (sin->sin_addr);
					}
					if (sctp_cmpaddr(ifa->ifa_addr, laddr->ifa->ifa_addr) == 1) {
						sin = (struct sockaddr_in *)laddr->ifa->ifa_addr;
						return (sin->sin_addr);
					}
				}
			}
		}
		/* ok, next one that is preferable in general */
		LIST_FOREACH(laddr, &stcb->asoc.sctp_local_addr_list,
		    sctp_nxt_addr) {
			if (laddr->ifa == NULL) {
				/* address has been removed */
				continue;
			}
			sin = sctp_is_v4_ifa_addr_prefered(laddr->ifa, loopscope, ipv4_scope, &sin_loop, &sin_local);
			if (sin == NULL)
				continue;
			return (sin->sin_addr);
		}

		/* last, what about one that is acceptable */
		LIST_FOREACH(laddr, &stcb->asoc.sctp_local_addr_list,
		    sctp_nxt_addr) {
			if (laddr->ifa == NULL) {
				/* address has been removed */
				continue;
			}
			sin = sctp_is_v4_ifa_addr_acceptable(laddr->ifa, loopscope, ipv4_scope, &sin_loop, &sin_local);
			if (sin == NULL)
				continue;
			return (sin->sin_addr);
		}
	}
	RTFREE(ro->ro_rt);
	ro->ro_rt = NULL;
	memset(&ans, 0, sizeof(ans));
	return (ans);
}

static struct sockaddr_in *
sctp_select_v4_nth_prefered_addr_from_ifn_boundall(struct ifnet *ifn, struct sctp_tcb *stcb, int non_asoc_addr_ok,
    uint8_t loopscope, uint8_t ipv4_scope, int cur_addr_num)
{
	struct ifaddr *ifa;
	struct sockaddr_in *sin;
	uint8_t sin_loop, sin_local;
	int num_eligible_addr = 0;

	TAILQ_FOREACH(ifa, &ifn->if_addrlist, ifa_list) {
		sin = sctp_is_v4_ifa_addr_prefered(ifa, loopscope, ipv4_scope, &sin_loop, &sin_local);
		if (sin == NULL)
			continue;
		if (stcb) {
			if ((non_asoc_addr_ok == 0) && sctp_is_addr_restricted(stcb, (struct sockaddr *)sin)) {
				/*
				 * It is restricted for some reason..
				 * probably not yet added.
				 */
				continue;
			}
		}
		if (cur_addr_num == num_eligible_addr) {
			return (sin);
		}
	}
	return (NULL);
}


static int
sctp_count_v4_num_prefered_boundall(struct ifnet *ifn, struct sctp_tcb *stcb, int non_asoc_addr_ok,
    uint8_t loopscope, uint8_t ipv4_scope, uint8_t * sin_loop, uint8_t * sin_local)
{
	struct ifaddr *ifa;
	struct sockaddr_in *sin;
	int num_eligible_addr = 0;

	TAILQ_FOREACH(ifa, &ifn->if_addrlist, ifa_list) {
		sin = sctp_is_v4_ifa_addr_prefered(ifa, loopscope, ipv4_scope, sin_loop, sin_local);
		if (sin == NULL)
			continue;
		if (stcb) {
			if ((non_asoc_addr_ok == 0) && sctp_is_addr_restricted(stcb, (struct sockaddr *)sin)) {
				/*
				 * It is restricted for some reason..
				 * probably not yet added.
				 */
				continue;
			}
		}
		num_eligible_addr++;
	}
	return (num_eligible_addr);

}

static struct in_addr
sctp_choose_v4_boundall(struct sctp_inpcb *inp,
    struct sctp_tcb *stcb,
    struct sctp_nets *net,
    struct route *ro,
    uint8_t ipv4_scope,
    uint8_t loopscope,
    int non_asoc_addr_ok)
{
	int cur_addr_num = 0, num_prefered = 0;
	uint8_t sin_loop, sin_local;
	struct ifnet *ifn;
	struct sockaddr_in *sin;
	struct in_addr ans;
	struct ifaddr *ifa;
	struct rtentry *rt;

	/*
	 * For v4 we can use (in boundall) any address in the association.
	 * If non_asoc_addr_ok is set we can use any address (at least in
	 * theory). So we look for prefered addresses first. If we find one,
	 * we use it. Otherwise we next try to get an address on the
	 * interface, which we should be able to do (unless non_asoc_addr_ok
	 * is false and we are routed out that way). In these cases where we
	 * can't use the address of the interface we go through all the
	 * ifn's looking for an address we can use and fill that in. Punting
	 * means we send back address 0, which will probably cause problems
	 * actually since then IP will fill in the address of the route ifn,
	 * which means we probably already rejected it.. i.e. here comes an
	 * abort :-<.
	 */
	rt = ro->ro_rt;
	ifn = rt->rt_ifp;
	if (net) {
		cur_addr_num = net->indx_of_eligible_next_to_use;
	}
	if (ifn == NULL) {
		goto bound_all_v4_plan_c;
	}
	num_prefered = sctp_count_v4_num_prefered_boundall(ifn, stcb, non_asoc_addr_ok, loopscope, ipv4_scope, &sin_loop, &sin_local);
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_OUTPUT1) {
		printf("Found %d prefered source addresses\n", num_prefered);
	}
#endif
	if (num_prefered == 0) {
		/*
		 * no eligible addresses, we must use some other interface
		 * address if we can find one.
		 */
		goto bound_all_v4_plan_b;
	}
	/*
	 * Ok we have num_eligible_addr set with how many we can use, this
	 * may vary from call to call due to addresses being deprecated
	 * etc..
	 */
	if (cur_addr_num >= num_prefered) {
		cur_addr_num = 0;
	}
	/*
	 * select the nth address from the list (where cur_addr_num is the
	 * nth) and 0 is the first one, 1 is the second one etc...
	 */
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_OUTPUT1) {
		printf("cur_addr_num:%d\n", cur_addr_num);
	}
#endif
	sin = sctp_select_v4_nth_prefered_addr_from_ifn_boundall(ifn, stcb, non_asoc_addr_ok, loopscope,
	    ipv4_scope, cur_addr_num);

	/* if sin is NULL something changed??, plan_a now */
	if (sin) {
		return (sin->sin_addr);
	}
	/*
	 * plan_b: Look at the interface that we emit on and see if we can
	 * find an acceptable address.
	 */
bound_all_v4_plan_b:
	TAILQ_FOREACH(ifa, &ifn->if_addrlist, ifa_list) {
		sin = sctp_is_v4_ifa_addr_acceptable(ifa, loopscope, ipv4_scope, &sin_loop, &sin_local);
		if (sin == NULL)
			continue;
		if (stcb) {
			if ((non_asoc_addr_ok == 0) && sctp_is_addr_restricted(stcb, (struct sockaddr *)sin)) {
				/*
				 * It is restricted for some reason..
				 * probably not yet added.
				 */
				continue;
			}
		}
		return (sin->sin_addr);
	}
	/*
	 * plan_c: Look at all interfaces and find a prefered address. If we
	 * reache here we are in trouble I think.
	 */
bound_all_v4_plan_c:
	for (ifn = TAILQ_FIRST(&ifnet);
	    ifn && (ifn != inp->next_ifn_touse);
	    ifn = TAILQ_NEXT(ifn, if_list)) {
		if (loopscope == 0 && ifn->if_type == IFT_LOOP) {
			/* wrong base scope */
			continue;
		}
		if (ifn == rt->rt_ifp)
			/* already looked at this guy */
			continue;
		num_prefered = sctp_count_v4_num_prefered_boundall(ifn, stcb, non_asoc_addr_ok,
		    loopscope, ipv4_scope, &sin_loop, &sin_local);
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_OUTPUT1) {
			printf("Found ifn:%x %d prefered source addresses\n", (uint32_t) ifn, num_prefered);
		}
#endif
		if (num_prefered == 0) {
			/*
			 * None on this interface.
			 */
			continue;
		}
		/*
		 * Ok we have num_eligible_addr set with how many we can
		 * use, this may vary from call to call due to addresses
		 * being deprecated etc..
		 */
		if (cur_addr_num >= num_prefered) {
			cur_addr_num = 0;
		}
		sin = sctp_select_v4_nth_prefered_addr_from_ifn_boundall(ifn, stcb, non_asoc_addr_ok, loopscope,
		    ipv4_scope, cur_addr_num);
		if (sin == NULL)
			continue;
		return (sin->sin_addr);

	}

	/*
	 * plan_d: We are in deep trouble. No prefered address on any
	 * interface. And the emit interface does not even have an
	 * acceptable address. Take anything we can get! If this does not
	 * work we are probably going to emit a packet that will illicit an
	 * ABORT, falling through.
	 */

	for (ifn = TAILQ_FIRST(&ifnet);
	    ifn && (ifn != inp->next_ifn_touse);
	    ifn = TAILQ_NEXT(ifn, if_list)) {
		if (loopscope == 0 && ifn->if_type == IFT_LOOP) {
			/* wrong base scope */
			continue;
		}
		if (ifn == rt->rt_ifp)
			/* already looked at this guy */
			continue;

		TAILQ_FOREACH(ifa, &ifn->if_addrlist, ifa_list) {
			sin = sctp_is_v4_ifa_addr_acceptable(ifa, loopscope, ipv4_scope, &sin_loop, &sin_local);
			if (sin == NULL)
				continue;
			if (stcb) {
				if ((non_asoc_addr_ok == 0) && sctp_is_addr_restricted(stcb, (struct sockaddr *)sin)) {
					/*
					 * It is restricted for some
					 * reason.. probably not yet added.
					 */
					continue;
				}
			}
			return (sin->sin_addr);
		}
	}
	/*
	 * Ok we can find NO address to source from that is not on our
	 * negative list. It is either the special ASCONF case where we are
	 * sourceing from a intf that has been ifconfig'd to a different
	 * address (i.e. it holds a ADD/DEL/SET-PRIM and the proper lookup
	 * address. OR we are hosed, and this baby is going to abort the
	 * association.
	 */
	if (non_asoc_addr_ok) {
		return (((struct sockaddr_in *)(rt->rt_ifa->ifa_addr))->sin_addr);
	} else {
		RTFREE(ro->ro_rt);
		ro->ro_rt = NULL;
		memset(&ans, 0, sizeof(ans));
		return (ans);
	}
}



/* tcb may be NULL */
struct in_addr
sctp_ipv4_source_address_selection(struct sctp_inpcb *inp,
    struct sctp_tcb *stcb, struct route *ro, struct sctp_nets *net,
    int non_asoc_addr_ok)
{
	struct in_addr ans;
	struct sockaddr_in *to = (struct sockaddr_in *)&ro->ro_dst;
	uint8_t ipv4_scope, loopscope;

	/*
	 * Rules: - Find the route if needed, cache if I can. - Look at
	 * interface address in route, Is it in the bound list. If so we
	 * have the best source. - If not we must rotate amongst the
	 * addresses.
	 * 
	 * Cavets and issues
	 * 
	 * Do we need to pay attention to scope. We can have a private address
	 * or a global address we are sourcing or sending to. So if we draw
	 * it out source     *      dest   *  result
	 * ------------------------------------------ a   Private    *
	 * Global  *  NAT? ------------------------------------------ b
	 * Private    *     Private *  No problem
	 * ------------------------------------------ c   Global     *
	 * Private *  Huh, How will this work?
	 * ------------------------------------------ d   Global     *
	 * Global  *  No Problem ------------------------------------------
	 * 
	 * And then we add to that what happens if there are multiple addresses
	 * assigned to an interface. Remember the ifa on a ifn is a linked
	 * list of addresses. So one interface can have more than one IPv4
	 * address. What happens if we have both a private and a global
	 * address? Do we then use context of destination to sort out which
	 * one is best? And what about NAT's sending P->G may get you a NAT
	 * translation, or should you select the G thats on the interface in
	 * preference.
	 * 
	 * Decisions:
	 * 
	 * - count the number of addresses on the interface. - if its one, no
	 * problem except case <c>. For <a> we will assume a NAT out there.
	 * - if there are more than one, then we need to worry about scope P
	 * or G. We should prefer G -> G and P -> P if possible. Then as a
	 * secondary fall back to mixed types G->P being a last ditch one. -
	 * The above all works for bound all, but bound specific we need to
	 * use the same concept but instead only consider the bound
	 * addresses. If the bound set is NOT assigned to the interface then
	 * we must use rotation amongst them.
	 * 
	 * Notes: For v4, we can always punt and let ip_output decide by
	 * sending back a source of 0.0.0.0
	 */

	if (ro->ro_rt == NULL) {
		/*
		 * Need a route to cache.
		 * 
		 */
		rtalloc_ign(ro, 0UL);
	}
	if (ro->ro_rt == NULL) {
		/* No route to host .. punt */
		memset(&ans, 0, sizeof(ans));
		return (ans);
	}
	/* Setup our scopes */
	if (stcb) {
		ipv4_scope = stcb->asoc.ipv4_local_scope;
		loopscope = stcb->asoc.loopback_scope;
	} else {
		/* Scope based on outbound address */
		if ((IN4_ISPRIVATE_ADDRESS(&to->sin_addr))) {
			ipv4_scope = 1;
			loopscope = 0;
		} else if (IN4_ISLOOPBACK_ADDRESS(&to->sin_addr)) {
			ipv4_scope = 1;
			loopscope = 1;
		} else {
			ipv4_scope = 0;
			loopscope = 0;
		}
	}
	if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) {
		/*
		 * When bound to all if the address list is set it is a
		 * negative list. Addresses being added by asconf.
		 */
		return (sctp_choose_v4_boundall(inp, stcb, net, ro,
		    ipv4_scope, loopscope, non_asoc_addr_ok));
	}
	/*
	 * Three possiblities here:
	 * 
	 * a) stcb is NULL, which means we operate only from the list of
	 * addresses (ifa's) bound to the assoc and we care not about the
	 * list. b) stcb is NOT-NULL, which means we have an assoc structure
	 * and auto-asconf is on. This means that the list of addresses is a
	 * NOT list. We use the list from the inp, but any listed address in
	 * our list is NOT yet added. However if the non_asoc_addr_ok is set
	 * we CAN use an address NOT available (i.e. being added). Its a
	 * negative list. c) stcb is NOT-NULL, which means we have an assoc
	 * structure and auto-asconf is off. This means that the list of
	 * addresses is the ONLY addresses I can use.. its positive.
	 * 
	 * Note we collapse b & c into the same function just like in the v6
	 * address selection.
	 */
	if (stcb) {
		return (sctp_choose_v4_boundspecific_stcb(inp, stcb, net,
		    ro, ipv4_scope, loopscope, non_asoc_addr_ok));
	} else {
		return (sctp_choose_v4_boundspecific_inp(inp, ro,
		    ipv4_scope, loopscope));
	}
	/* this should not be reached */
	memset(&ans, 0, sizeof(ans));
	return (ans);
}



static struct sockaddr_in6 *
sctp_is_v6_ifa_addr_acceptable(struct ifaddr *ifa, int loopscope, int loc_scope, int *sin_loop, int *sin_local)
{
	struct in6_ifaddr *ifa6;
	struct sockaddr_in6 *sin6;


	if (ifa->ifa_addr->sa_family != AF_INET6) {
		/* forget non-v6 */
		return (NULL);
	}
	ifa6 = (struct in6_ifaddr *)ifa;
	/* ok to use deprecated addresses? */
	if (!ip6_use_deprecated) {
		if (IFA6_IS_DEPRECATED(ifa6)) {
			/* can't use this type */
			return (NULL);
		}
	}
	/* are we ok, with the current state of this address? */
	if (ifa6->ia6_flags &
	    (IN6_IFF_DETACHED | IN6_IFF_NOTREADY | IN6_IFF_ANYCAST)) {
		/* Can't use these types */
		return (NULL);
	}
	/* Ok the address may be ok */
	sin6 = (struct sockaddr_in6 *)ifa->ifa_addr;
	*sin_local = *sin_loop = 0;
	if ((ifa->ifa_ifp->if_type == IFT_LOOP) ||
	    (IN6_IS_ADDR_LOOPBACK(&sin6->sin6_addr))) {
		*sin_loop = 1;
	}
	if (!loopscope && *sin_loop) {
		/* Its a loopback address and we don't have loop scope */
		return (NULL);
	}
	if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
		/* we skip unspecifed addresses */
		return (NULL);
	}
	if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
		*sin_local = 1;
	}
	if (!loc_scope && *sin_local) {
		/*
		 * Its a link local address, and we don't have link local
		 * scope
		 */
		return (NULL);
	}
	return (sin6);
}


static struct sockaddr_in6 *
sctp_choose_v6_boundspecific_stcb(struct sctp_inpcb *inp,
    struct sctp_tcb *stcb,
    struct sctp_nets *net,
    struct route *ro,
    uint8_t loc_scope,
    uint8_t loopscope,
    int non_asoc_addr_ok)
{
	/*
	 * Each endpoint has a list of local addresses associated with it.
	 * The address list is either a "negative list" i.e. those addresses
	 * that are NOT allowed to be used as a source OR a "postive list"
	 * i.e. those addresses that CAN be used.
	 * 
	 * Its a negative list if asconf is allowed. What we do in this case is
	 * use the ep address list BUT we have to cross check it against the
	 * negative list.
	 * 
	 * In the case where NO asconf is allowed, we have just a straight
	 * association level list that we must use to find a source address.
	 */
	struct sctp_laddr *laddr, *starting_point;
	struct sockaddr_in6 *sin6;
	int sin_loop, sin_local;
	int start_at_beginning = 0;
	struct ifnet *ifn;
	struct ifaddr *ifa;
	struct rtentry *rt;

	rt = ro->ro_rt;
	ifn = rt->rt_ifp;
	if (sctp_is_feature_on(inp, SCTP_PCB_FLAGS_DO_ASCONF)) {
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_OUTPUT1) {
			printf("Have a STCB - asconf allowed, not bound all have a netgative list\n");
		}
#endif
		/*
		 * first question, is the ifn we will emit on in our list,
		 * if so, we want that one.
		 */
		if (ifn) {
			TAILQ_FOREACH(ifa, &ifn->if_addrlist, ifa_list) {
				if (sctp_is_addr_in_ep(inp, ifa)) {
					sin6 = sctp_is_v6_ifa_addr_acceptable(ifa, loopscope, loc_scope, &sin_loop, &sin_local);
					if (sin6 == NULL)
						continue;
					if ((non_asoc_addr_ok == 0) &&
					    (sctp_is_addr_restricted(stcb, (struct sockaddr *)sin6))) {
						/* on the no-no list */
						continue;
					}
					return (sin6);
				}
			}
		}
		starting_point = stcb->asoc.last_used_address;
		/* First try for matching scope */
sctp_from_the_top:
		if (stcb->asoc.last_used_address == NULL) {
			start_at_beginning = 1;
			stcb->asoc.last_used_address = LIST_FIRST(&inp->sctp_addr_list);
		}
		/* search beginning with the last used address */
		for (laddr = stcb->asoc.last_used_address; laddr;
		    laddr = LIST_NEXT(laddr, sctp_nxt_addr)) {
			if (laddr->ifa == NULL) {
				/* address has been removed */
				continue;
			}
			sin6 = sctp_is_v6_ifa_addr_acceptable(laddr->ifa, loopscope, loc_scope, &sin_loop, &sin_local);
			if (sin6 == NULL)
				continue;
			if ((non_asoc_addr_ok == 0) && (sctp_is_addr_restricted(stcb, (struct sockaddr *)sin6))) {
				/* on the no-no list */
				continue;
			}
			/* is it of matching scope ? */
			if ((loopscope == 0) &&
			    (loc_scope == 0) &&
			    (sin_loop == 0) &&
			    (sin_local == 0)) {
				/* all of global scope we are ok with it */
				return (sin6);
			}
			if (loopscope && sin_loop)
				/* both on the loopback, thats ok */
				return (sin6);
			if (loc_scope && sin_local)
				/* both local scope */
				return (sin6);

		}
		if (start_at_beginning == 0) {
			stcb->asoc.last_used_address = NULL;
			goto sctp_from_the_top;
		}
		/* now try for any higher scope than the destination */
		stcb->asoc.last_used_address = starting_point;
		start_at_beginning = 0;
sctp_from_the_top2:
		if (stcb->asoc.last_used_address == NULL) {
			start_at_beginning = 1;
			stcb->asoc.last_used_address = LIST_FIRST(&inp->sctp_addr_list);
		}
		/* search beginning with the last used address */
		for (laddr = stcb->asoc.last_used_address; laddr;
		    laddr = LIST_NEXT(laddr, sctp_nxt_addr)) {
			if (laddr->ifa == NULL) {
				/* address has been removed */
				continue;
			}
			sin6 = sctp_is_v6_ifa_addr_acceptable(laddr->ifa, loopscope, loc_scope, &sin_loop, &sin_local);
			if (sin6 == NULL)
				continue;
			if ((non_asoc_addr_ok == 0) && (sctp_is_addr_restricted(stcb, (struct sockaddr *)sin6))) {
				/* on the no-no list */
				continue;
			}
			return (sin6);
		}
		if (start_at_beginning == 0) {
			stcb->asoc.last_used_address = NULL;
			goto sctp_from_the_top2;
		}
	} else {
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_OUTPUT1) {
			printf("Have a STCB - no asconf allowed, not bound all have a postive list\n");
		}
#endif
		/* First try for interface output match */
		LIST_FOREACH(laddr, &stcb->asoc.sctp_local_addr_list,
		    sctp_nxt_addr) {
			if (laddr->ifa == NULL) {
				/* address has been removed */
				continue;
			}
			sin6 = sctp_is_v6_ifa_addr_acceptable(laddr->ifa, loopscope, loc_scope, &sin_loop, &sin_local);
			if (sin6 == NULL)
				continue;
			/*
			 * first question, is laddr->ifa an address
			 * associated with the emit interface
			 */
			if (ifn) {
				TAILQ_FOREACH(ifa, &ifn->if_addrlist, ifa_list) {
					if (laddr->ifa == ifa) {
						sin6 = (struct sockaddr_in6 *)laddr->ifa->ifa_addr;
						return (sin6);
					}
					if (sctp_cmpaddr(ifa->ifa_addr, laddr->ifa->ifa_addr) == 1) {
						sin6 = (struct sockaddr_in6 *)laddr->ifa->ifa_addr;
						return (sin6);
					}
				}
			}
		}
		/* Next try for matching scope */
		LIST_FOREACH(laddr, &stcb->asoc.sctp_local_addr_list,
		    sctp_nxt_addr) {
			if (laddr->ifa == NULL) {
				/* address has been removed */
				continue;
			}
			sin6 = sctp_is_v6_ifa_addr_acceptable(laddr->ifa, loopscope, loc_scope, &sin_loop, &sin_local);
			if (sin6 == NULL)
				continue;

			if ((loopscope == 0) &&
			    (loc_scope == 0) &&
			    (sin_loop == 0) &&
			    (sin_local == 0)) {
				/* all of global scope we are ok with it */
				return (sin6);
			}
			if (loopscope && sin_loop)
				/* both on the loopback, thats ok */
				return (sin6);
			if (loc_scope && sin_local)
				/* both local scope */
				return (sin6);
		}
		/* ok, now try for a higher scope in the source address */
		/* First try for matching scope */
		LIST_FOREACH(laddr, &stcb->asoc.sctp_local_addr_list,
		    sctp_nxt_addr) {
			if (laddr->ifa == NULL) {
				/* address has been removed */
				continue;
			}
			sin6 = sctp_is_v6_ifa_addr_acceptable(laddr->ifa, loopscope, loc_scope, &sin_loop, &sin_local);
			if (sin6 == NULL)
				continue;
			return (sin6);
		}
	}
	RTFREE(ro->ro_rt);
	ro->ro_rt = NULL;
	return (NULL);
}

static struct sockaddr_in6 *
sctp_choose_v6_boundspecific_inp(struct sctp_inpcb *inp,
    struct route *ro,
    uint8_t loc_scope,
    uint8_t loopscope)
{
	/*
	 * Here we are bound specific and have only an inp. We must find an
	 * address that is bound that we can give out as a src address. We
	 * prefer two addresses of same scope if we can find them that way.
	 */
	struct sctp_laddr *laddr;
	struct sockaddr_in6 *sin6;
	struct ifnet *ifn;
	struct ifaddr *ifa;
	int sin_loop, sin_local;
	struct rtentry *rt;

	/*
	 * first question, is the ifn we will emit on in our list, if so, we
	 * want that one.
	 */

	rt = ro->ro_rt;
	ifn = rt->rt_ifp;
	if (ifn) {
		TAILQ_FOREACH(ifa, &ifn->if_addrlist, ifa_list) {
			sin6 = sctp_is_v6_ifa_addr_acceptable(ifa, loopscope, loc_scope, &sin_loop, &sin_local);
			if (sin6 == NULL)
				continue;
			if (sctp_is_addr_in_ep(inp, ifa)) {
				return (sin6);
			}
		}
	}
	for (laddr = LIST_FIRST(&inp->sctp_addr_list);
	    laddr && (laddr != inp->next_addr_touse);
	    laddr = LIST_NEXT(laddr, sctp_nxt_addr)) {
		if (laddr->ifa == NULL) {
			/* address has been removed */
			continue;
		}
		sin6 = sctp_is_v6_ifa_addr_acceptable(laddr->ifa, loopscope, loc_scope, &sin_loop, &sin_local);
		if (sin6 == NULL)
			continue;

		if ((loopscope == 0) &&
		    (loc_scope == 0) &&
		    (sin_loop == 0) &&
		    (sin_local == 0)) {
			/* all of global scope we are ok with it */
			return (sin6);
		}
		if (loopscope && sin_loop)
			/* both on the loopback, thats ok */
			return (sin6);
		if (loc_scope && sin_local)
			/* both local scope */
			return (sin6);

	}
	/*
	 * if we reach here, we could not find two addresses of the same
	 * scope to give out. Lets look for any higher level scope for a
	 * source address.
	 */
	for (laddr = LIST_FIRST(&inp->sctp_addr_list);
	    laddr && (laddr != inp->next_addr_touse);
	    laddr = LIST_NEXT(laddr, sctp_nxt_addr)) {
		if (laddr->ifa == NULL) {
			/* address has been removed */
			continue;
		}
		sin6 = sctp_is_v6_ifa_addr_acceptable(laddr->ifa, loopscope, loc_scope, &sin_loop, &sin_local);
		if (sin6 == NULL)
			continue;
		return (sin6);
	}
	/* no address bound can be a source for the destination */
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_OUTPUT1) {
		printf("Src address selection for EP, no acceptable src address found for address\n");
	}
#endif
	RTFREE(ro->ro_rt);
	ro->ro_rt = NULL;
	return (NULL);
}


static struct sockaddr_in6 *
sctp_select_v6_nth_addr_from_ifn_boundall(struct ifnet *ifn, struct sctp_tcb *stcb, int non_asoc_addr_ok, uint8_t loopscope,
    uint8_t loc_scope, int cur_addr_num, int match_scope)
{
	struct ifaddr *ifa;
	struct sockaddr_in6 *sin6;
	int sin_loop, sin_local;
	int num_eligible_addr = 0;

	TAILQ_FOREACH(ifa, &ifn->if_addrlist, ifa_list) {
		sin6 = sctp_is_v6_ifa_addr_acceptable(ifa, loopscope, loc_scope, &sin_loop, &sin_local);
		if (sin6 == NULL)
			continue;
		if (stcb) {
			if ((non_asoc_addr_ok == 0) && sctp_is_addr_restricted(stcb, (struct sockaddr *)sin6)) {
				/*
				 * It is restricted for some reason..
				 * probably not yet added.
				 */
				continue;
			}
		}
		if (match_scope) {
			/* Here we are asked to match scope if possible */
			if (loopscope && sin_loop)
				/* src and destination are loopback scope */
				return (sin6);
			if (loc_scope && sin_local)
				/* src and destination are local scope */
				return (sin6);
			if ((loopscope == 0) &&
			    (loc_scope == 0) &&
			    (sin_loop == 0) &&
			    (sin_local == 0)) {
				/* src and destination are global scope */
				return (sin6);
			}
			continue;
		}
		if (num_eligible_addr == cur_addr_num) {
			/* this is it */
			return (sin6);
		}
		num_eligible_addr++;
	}
	return (NULL);
}


static int
sctp_count_v6_num_eligible_boundall(struct ifnet *ifn, struct sctp_tcb *stcb,
    int non_asoc_addr_ok, uint8_t loopscope, uint8_t loc_scope)
{
	struct ifaddr *ifa;
	struct sockaddr_in6 *sin6;
	int num_eligible_addr = 0;
	int sin_loop, sin_local;

	TAILQ_FOREACH(ifa, &ifn->if_addrlist, ifa_list) {
		sin6 = sctp_is_v6_ifa_addr_acceptable(ifa, loopscope, loc_scope, &sin_loop, &sin_local);
		if (sin6 == NULL)
			continue;
		if (stcb) {
			if ((non_asoc_addr_ok == 0) && sctp_is_addr_restricted(stcb, (struct sockaddr *)sin6)) {
				/*
				 * It is restricted for some reason..
				 * probably not yet added.
				 */
				continue;
			}
		}
		num_eligible_addr++;
	}
	return (num_eligible_addr);
}


static struct sockaddr_in6 *
sctp_choose_v6_boundall(struct sctp_inpcb *inp,
    struct sctp_tcb *stcb,
    struct sctp_nets *net,
    struct route *ro,
    uint8_t loc_scope,
    uint8_t loopscope,
    int non_asoc_addr_ok)
{
	/*
	 * Ok, we are bound all SO any address is ok to use as long as it is
	 * NOT in the negative list.
	 */
	int num_eligible_addr;
	int cur_addr_num = 0;
	int started_at_beginning = 0;
	int match_scope_prefered;

	/*
	 * first question is, how many eligible addresses are there for the
	 * destination ifn that we are using that are within the proper
	 * scope?
	 */
	struct ifnet *ifn;
	struct sockaddr_in6 *sin6;
	struct rtentry *rt;

	rt = ro->ro_rt;
	ifn = rt->rt_ifp;
	if (net) {
		cur_addr_num = net->indx_of_eligible_next_to_use;
	}
	if (cur_addr_num == 0) {
		match_scope_prefered = 1;
	} else {
		match_scope_prefered = 0;
	}
	num_eligible_addr = sctp_count_v6_num_eligible_boundall(ifn, stcb, non_asoc_addr_ok, loopscope, loc_scope);
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_OUTPUT1) {
		printf("Found %d eligible source addresses\n", num_eligible_addr);
	}
#endif
	if (num_eligible_addr == 0) {
		/*
		 * no eligible addresses, we must use some other interface
		 * address if we can find one.
		 */
		goto bound_all_v6_plan_b;
	}
	/*
	 * Ok we have num_eligible_addr set with how many we can use, this
	 * may vary from call to call due to addresses being deprecated
	 * etc..
	 */
	if (cur_addr_num >= num_eligible_addr) {
		cur_addr_num = 0;
	}
	/*
	 * select the nth address from the list (where cur_addr_num is the
	 * nth) and 0 is the first one, 1 is the second one etc...
	 */
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_OUTPUT1) {
		printf("cur_addr_num:%d match_scope_prefered:%d select it\n",
		    cur_addr_num, match_scope_prefered);
	}
#endif
	sin6 = sctp_select_v6_nth_addr_from_ifn_boundall(ifn, stcb, non_asoc_addr_ok, loopscope,
	    loc_scope, cur_addr_num, match_scope_prefered);
	if (match_scope_prefered && (sin6 == NULL)) {
		/* retry without the preference for matching scope */
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_OUTPUT1) {
			printf("retry with no match_scope_prefered\n");
		}
#endif
		sin6 = sctp_select_v6_nth_addr_from_ifn_boundall(ifn, stcb, non_asoc_addr_ok, loopscope,
		    loc_scope, cur_addr_num, 0);
	}
	if (sin6) {
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_OUTPUT1) {
			printf("Selected address %d ifn:%x for the route\n", cur_addr_num, (uint32_t) ifn);
		}
#endif
		if (net) {
			/* store so we get the next one */
			if (cur_addr_num < 255)
				net->indx_of_eligible_next_to_use = cur_addr_num + 1;
			else
				net->indx_of_eligible_next_to_use = 0;
		}
		return (sin6);
	}
	num_eligible_addr = 0;
bound_all_v6_plan_b:
	/*
	 * ok, if we reach here we either fell through due to something
	 * changing during an interupt (unlikely) or we have NO eligible
	 * source addresses for the ifn of the route (most likely). We must
	 * look at all the other interfaces EXCEPT rt->rt_ifp and do the
	 * same game.
	 */
	if (inp->next_ifn_touse == NULL) {
		started_at_beginning = 1;
		inp->next_ifn_touse = TAILQ_FIRST(&ifnet);
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_OUTPUT1) {
			printf("Start at first IFN:%x\n", (uint32_t) inp->next_ifn_touse);
		}
#endif
	} else {
		inp->next_ifn_touse = TAILQ_NEXT(inp->next_ifn_touse, if_list);
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_OUTPUT1) {
			printf("Resume at IFN:%x\n", (uint32_t) inp->next_ifn_touse);
		}
#endif
		if (inp->next_ifn_touse == NULL) {
#ifdef SCTP_DEBUG
			if (sctp_debug_on & SCTP_DEBUG_OUTPUT1) {
				printf("IFN Resets\n");
			}
#endif
			started_at_beginning = 1;
			inp->next_ifn_touse = TAILQ_FIRST(&ifnet);
		}
	}
	for (ifn = inp->next_ifn_touse; ifn;
	    ifn = TAILQ_NEXT(ifn, if_list)) {
		if (loopscope == 0 && ifn->if_type == IFT_LOOP) {
			/* wrong base scope */
			continue;
		}
		if (loc_scope && (ifn->if_index != loc_scope)) {
			/*
			 * by definition the scope (from to->sin6_scopeid)
			 * must match that of the interface. If not then we
			 * could pick a wrong scope for the address.
			 * Ususally we don't hit plan-b since the route
			 * handles this. However we can hit plan-b when we
			 * send to local-host so the route is the loopback
			 * interface, but the destination is a link local.
			 */
			continue;
		}
		if (ifn == rt->rt_ifp) {
			/* already looked at this guy */
			continue;
		}
		/*
		 * Address rotation will only work when we are not rotating
		 * sourced interfaces and are using the interface of the
		 * route. We would need to have a per interface index in
		 * order to do proper rotation.
		 */
		num_eligible_addr = sctp_count_v6_num_eligible_boundall(ifn, stcb, non_asoc_addr_ok, loopscope, loc_scope);
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_OUTPUT1) {
			printf("IFN:%x has %d eligible\n", (uint32_t) ifn, num_eligible_addr);
		}
#endif
		if (num_eligible_addr == 0) {
			/* none we can use */
			continue;
		}
		/*
		 * Ok we have num_eligible_addr set with how many we can
		 * use, this may vary from call to call due to addresses
		 * being deprecated etc..
		 */
		inp->next_ifn_touse = ifn;

		/*
		 * select the first one we can find with perference for
		 * matching scope.
		 */
		sin6 = sctp_select_v6_nth_addr_from_ifn_boundall(ifn, stcb, non_asoc_addr_ok, loopscope, loc_scope, 0, 1);
		if (sin6 == NULL) {
			/*
			 * can't find one with matching scope how about a
			 * source with higher scope
			 */
			sin6 = sctp_select_v6_nth_addr_from_ifn_boundall(ifn, stcb, non_asoc_addr_ok, loopscope, loc_scope, 0, 0);
			if (sin6 == NULL)
				/* Hmm, can't find one in the interface now */
				continue;
		}
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_OUTPUT1) {
			printf("Selected the %d'th address of ifn:%x\n",
			    cur_addr_num,
			    (uint32_t) ifn);
		}
#endif
		return (sin6);
	}
	if (started_at_beginning == 0) {
		/*
		 * we have not been through all of them yet, force us to go
		 * through them all.
		 */
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_OUTPUT1) {
			printf("Force a recycle\n");
		}
#endif
		inp->next_ifn_touse = NULL;
		goto bound_all_v6_plan_b;
	}
	RTFREE(ro->ro_rt);
	ro->ro_rt = NULL;
	return (NULL);

}

/* stcb and net may be NULL */
struct in6_addr
sctp_ipv6_source_address_selection(struct sctp_inpcb *inp,
    struct sctp_tcb *stcb, struct route *ro, struct sctp_nets *net,
    int non_asoc_addr_ok)
{
	struct in6_addr ans;
	struct sockaddr_in6 *rt_addr;
	uint8_t loc_scope, loopscope;
	struct sockaddr_in6 *to = (struct sockaddr_in6 *)&ro->ro_dst;

	/*
	 * This routine is tricky standard v6 src address selection cannot
	 * take into account what we have bound etc, so we can't use it.
	 * 
	 * Instead here is what we must do: 1) Make sure we have a route, if we
	 * don't have a route we can never reach the peer. 2) Once we have a
	 * route, determine the scope of the route. Link local, loopback or
	 * global. 3) Next we divide into three types. Either we are bound
	 * all.. which means we want to use one of the addresses of the
	 * interface we are going out. <or> 4a) We have not stcb, which
	 * means we are using the specific addresses bound on an inp, in
	 * this case we are similar to the stcb case (4b below) accept the
	 * list is always a positive list.<or> 4b) We are bound specific
	 * with a stcb, which means we have a list of bound addresses and we
	 * must see if the ifn of the route is actually one of the bound
	 * addresses. If not, then we must rotate addresses amongst properly
	 * scoped bound addresses, if so we use the address of the
	 * interface. 5) Always, no matter which path we take through the
	 * above we must be sure the source address we use is allowed to be
	 * used. I.e. IN6_IFF_DETACHED, IN6_IFF_NOTREADY, and
	 * IN6_IFF_ANYCAST addresses cannot be used. 6) Addresses that are
	 * deprecated MAY be used if (!ip6_use_deprecated) { if
	 * (IFA6_IS_DEPRECATED(ifa6)) { skip the address } }
	 */

	/*** 1> determine route, if not already done */
	if (ro->ro_rt == NULL) {
		/*
		 * Need a route to cache.
		 */
		int scope_save;

		scope_save = to->sin6_scope_id;
		to->sin6_scope_id = 0;

		rtalloc_ign(ro, 0UL);
		to->sin6_scope_id = scope_save;
	}
	if (ro->ro_rt == NULL) {
		/*
		 * no route to host. this packet is going no-where. We
		 * probably should make sure we arrange to send back an
		 * error.
		 */
#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_OUTPUT1) {
			printf("No route to host, this packet cannot be sent!\n");
		}
#endif
		memset(&ans, 0, sizeof(ans));
		return (ans);
	}
	/*** 2a> determine scope for outbound address/route */
	loc_scope = loopscope = 0;
	/*
	 * We base our scope on the outbound packet scope and route, NOT the
	 * TCB (if there is one). This way in local scope we will only use a
	 * local scope src address when we send to a local address.
	 */

	if (IN6_IS_ADDR_LOOPBACK(&to->sin6_addr)) {
		/*
		 * If the route goes to the loopback address OR the address
		 * is a loopback address, we are loopback scope.
		 */
		loc_scope = 0;
		loopscope = 1;
		if (net != NULL) {
			/* mark it as local */
			net->addr_is_local = 1;
		}
	} else if (IN6_IS_ADDR_LINKLOCAL(&to->sin6_addr)) {
		if (to->sin6_scope_id)
			loc_scope = to->sin6_scope_id;
		else {
			loc_scope = 1;
		}
		loopscope = 0;
	}
	/*
	 * now, depending on which way we are bound we call the appropriate
	 * routine to do steps 3-6
	 */
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_OUTPUT1) {
		printf("Destination address:");
		sctp_print_address((struct sockaddr *)to);
	}
#endif

	if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) {
		rt_addr = sctp_choose_v6_boundall(inp, stcb, net, ro, loc_scope, loopscope, non_asoc_addr_ok);
	} else {
		if (stcb)
			rt_addr = sctp_choose_v6_boundspecific_stcb(inp, stcb, net, ro, loc_scope, loopscope, non_asoc_addr_ok);
		else
			/*
			 * we can't have a non-asoc address since we have no
			 * association
			 */
			rt_addr = sctp_choose_v6_boundspecific_inp(inp, ro, loc_scope, loopscope);
	}
	if (rt_addr == NULL) {
		/* no suitable address? */
		struct in6_addr in6;

#ifdef SCTP_DEBUG
		if (sctp_debug_on & SCTP_DEBUG_OUTPUT1) {
			printf("V6 packet will reach dead-end no suitable src address\n");
		}
#endif
		memset(&in6, 0, sizeof(in6));
		return (in6);
	}
#ifdef SCTP_DEBUG
	if (sctp_debug_on & SCTP_DEBUG_OUTPUT1) {
		printf("Source address selected is:");
		sctp_print_address((struct sockaddr *)rt_addr);
	}
#endif
	return (rt_addr->sin6_addr);
}


static
int
sctp_is_address_in_scope(struct ifaddr *ifa,
    int ipv4_addr_legal,
    int ipv6_addr_legal,
    int loopback_scope,
    int ipv4_local_scope,
    int local_scope,
    int site_scope)
{
	if ((loopback_scope == 0) &&
	    (ifa->ifa_ifp) &&
	    (ifa->ifa_ifp->if_type == IFT_LOOP)) {
		/*
		 * skip loopback if not in scope *
		 */
		return (0);
	}
	if ((ifa->ifa_addr->sa_family == AF_INET) && ipv4_addr_legal) {
		struct sockaddr_in *sin;

		sin = (struct sockaddr_in *)ifa->ifa_addr;
		if (sin->sin_addr.s_addr == 0) {
			/* not in scope , unspecified */
			return (0);
		}
		if ((ipv4_local_scope == 0) &&
		    (IN4_ISPRIVATE_ADDRESS(&sin->sin_addr))) {
			/* private address not in scope */
			return (0);
		}
	} else if ((ifa->ifa_addr->sa_family == AF_INET6) && ipv6_addr_legal) {
		struct sockaddr_in6 *sin6;
		struct in6_ifaddr *ifa6;

		ifa6 = (struct in6_ifaddr *)ifa;
		/* ok to use deprecated addresses? */
		if (!ip6_use_deprecated) {
			if (ifa6->ia6_flags &
			    IN6_IFF_DEPRECATED) {
				return (0);
			}
		}
		if (ifa6->ia6_flags &
		    (IN6_IFF_DETACHED |
		    IN6_IFF_ANYCAST |
		    IN6_IFF_NOTREADY)) {
			return (0);
		}
		sin6 = (struct sockaddr_in6 *)ifa->ifa_addr;
		if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
			/* skip unspecifed addresses */
			return (0);
		}
		if (		/* (local_scope == 0) && */
		    (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr))) {
			return (0);
		}
		if ((site_scope == 0) &&
		    (IN6_IS_ADDR_SITELOCAL(&sin6->sin6_addr))) {
			return (0);
		}
	} else {
		return (0);
	}
	return (1);
}

static struct mbuf *
sctp_add_addr_to_mbuf(struct mbuf *m, struct ifaddr *ifa)
{
	struct sctp_paramhdr *parmh;
	struct mbuf *mret;
	int len;

	if (ifa->ifa_addr->sa_family == AF_INET) {
		len = sizeof(struct sctp_ipv4addr_param);
	} else if (ifa->ifa_addr->sa_family == AF_INET6) {
		len = sizeof(struct sctp_ipv6addr_param);
	} else {
		/* unknown type */
		return (m);
	}

	if (M_TRAILINGSPACE(m) >= len) {
		/* easy side we just drop it on the end */
		parmh = (struct sctp_paramhdr *)(m->m_data + m->m_len);
		mret = m;
	} else {
		/* Need more space */
		mret = m;
		while (mret->m_next != NULL) {
			mret = mret->m_next;
		}
		mret->m_next = sctp_get_mbuf_for_msg(len, 0, M_DONTWAIT, 1, MT_DATA);
		if (mret->m_next == NULL) {
			/* We are hosed, can't add more addresses */
			return (m);
		}
		mret = mret->m_next;
		parmh = mtod(mret, struct sctp_paramhdr *);
	}
	/* now add the parameter */
	if (ifa->ifa_addr->sa_family == AF_INET) {
		struct sctp_ipv4addr_param *ipv4p;
		struct sockaddr_in *sin;

		sin = (struct sockaddr_in *)ifa->ifa_addr;
		ipv4p = (struct sctp_ipv4addr_param *)parmh;
		parmh->param_type = htons(SCTP_IPV4_ADDRESS);
		parmh->param_length = htons(len);
		ipv4p->addr = sin->sin_addr.s_addr;
		mret->m_len += len;
	} else if (ifa->ifa_addr->sa_family == AF_INET6) {
		struct sctp_ipv6addr_param *ipv6p;
		struct sockaddr_in6 *sin6;

		sin6 = (struct sockaddr_in6 *)ifa->ifa_addr;
		ipv6p = (struct sctp_ipv6addr_param *)parmh;
		parmh->param_type = htons(SCTP_IPV6_ADDRESS);
		parmh->param_length = htons(len);
		memcpy(ipv6p->addr, &sin6->sin6_addr,
		    sizeof(ipv6p->addr));
		/* clear embedded scope in the address */
		in6_clearscope((struct in6_addr *)ipv6p->addr);
		mret->m_len += len;
	} else {
		return (m);
	}
	return (mret);
}


struct mbuf *
sctp_add_addresses_to_i_ia(struct sctp_inpcb *inp, struct sctp_scoping *scope, struct mbuf *m_at, int cnt_inits_to)
{
	int cnt;

	if (inp->sctp_flags & SCTP_PCB_FLAGS_BOUNDALL) {
		struct ifnet *ifn;
		struct ifaddr *ifa;

		cnt = cnt_inits_to;
		TAILQ_FOREACH(ifn, &ifnet, if_list) {
			if ((scope->loopback_scope == 0) &&
			    (ifn->if_type == IFT_LOOP)) {
				/*
				 * Skip loopback devices if loopback_scope
				 * not set
				 */
				continue;
			}
			TAILQ_FOREACH(ifa, &ifn->if_addrlist, ifa_list) {
				if (sctp_is_address_in_scope(ifa,
				    scope->ipv4_addr_legal,
				    scope->ipv6_addr_legal,
				    scope->loopback_scope,
				    scope->ipv4_local_scope,
				    scope->local_scope,
				    scope->site_scope) == 0) {
					continue;
				}
				cnt++;
			}
		}
		if (cnt > 1) {
			TAILQ_FOREACH(ifn, &ifnet, if_list) {
				if ((scope->loopback_scope == 0) &&
				    (ifn->if_type == IFT_LOOP)) {
					/*
					 * Skip loopback devices if
					 * loopback_scope not set
					 */
					continue;
				}
				TAILQ_FOREACH(ifa, &ifn->if_addrlist, ifa_list) {
					if (sctp_is_address_in_scope(ifa,
					    scope->ipv4_addr_legal,
					    scope->ipv6_addr_legal,
					    scope->loopback_scope,
					    scope->ipv4_local_scope,
					    scope->local_scope,
					    scope->site_scope) == 0) {
						continue;
					}
					m_at = sctp_add_addr_to_mbuf(m_at, ifa);
				}
			}
		}
	} else {
		struct sctp_laddr *laddr;
		int cnt;

		cnt = cnt_inits_to;
		/* First, how many ? */
		LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {
			if (laddr->ifa == NULL) {
				continue;
			}
			if (laddr->ifa->ifa_addr == NULL)
				continue;
			if (sctp_is_address_in_scope(laddr->ifa,
			    scope->ipv4_addr_legal,
			    scope->ipv6_addr_legal,
			    scope->loopback_scope,
			    scope->ipv4_local_scope,
			    scope->local_scope,
			    scope->site_scope) == 0) {
				continue;
			}
			cnt++;
		}
		/*
		 * To get through a NAT we only list addresses if we have
		 * more than one. That way if you just bind a single address
		 * we let the source of the init dictate our address.
		 */
		if (cnt > 1) {
			LIST_FOREACH(laddr, &inp->sctp_addr_list, sctp_nxt_addr) {
				if (laddr->ifa == NULL) {
					continue;
				}
				if (laddr->ifa->ifa_addr == NULL) {
					continue;
				}
				if (sctp_is_address_in_scope(laddr->ifa,
				    scope->ipv4_addr_legal,
				    scope->ipv6_addr_legal,
				    scope->loopback_scope,
				    scope->ipv4_local_scope,
				    scope->local_scope,
				    scope->site_scope) == 0) {
					continue;
				}
				m_at = sctp_add_addr_to_mbuf(m_at, laddr->ifa);
			}
		}
	}
	return (m_at);
}
