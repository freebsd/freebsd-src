/*	$FreeBSD$	*/

/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * Added redirect stuff and a variety of bug fixes. (mcn@EnGarde.com)
 */

#include "ipf.h"


#if !defined(lint)
static const char rcsid[] = "@(#)$Id: printactivenat.c,v 1.3.2.4 2004/05/11 16:07:32 darrenr Exp $";
#endif


void printactivenat(nat, opts)
nat_t *nat;
int opts;
{

	printf("%s", getnattype(nat->nat_ptr));

	if (nat->nat_flags & SI_CLONE)
		printf(" CLONE");

	printf(" %-15s", inet_ntoa(nat->nat_inip));

	if ((nat->nat_flags & IPN_TCPUDP) != 0)
		printf(" %-5hu", ntohs(nat->nat_inport));

	printf(" <- -> %-15s",inet_ntoa(nat->nat_outip));

	if ((nat->nat_flags & IPN_TCPUDP) != 0)
		printf(" %-5hu", ntohs(nat->nat_outport));

	printf(" [%s", inet_ntoa(nat->nat_oip));
	if ((nat->nat_flags & IPN_TCPUDP) != 0)
		printf(" %hu", ntohs(nat->nat_oport));
	printf("]");

	if (opts & OPT_VERBOSE) {
		printf("\n\tage %lu use %hu sumd %s/",
			nat->nat_age, nat->nat_use, getsumd(nat->nat_sumd[0]));
		printf("%s pr %u bkt %d/%d flags %x\n",
			getsumd(nat->nat_sumd[1]), nat->nat_p,
			nat->nat_hv[0], nat->nat_hv[1], nat->nat_flags);
		printf("\tifp %s", getifname(nat->nat_ifps[0]));
		printf(",%s ", getifname(nat->nat_ifps[1]));
#ifdef	USE_QUAD_T
		printf("bytes %qu/%qu pkts %qu/%qu",
			(unsigned long long)nat->nat_bytes[0],
			(unsigned long long)nat->nat_bytes[1],
			(unsigned long long)nat->nat_pkts[0],
			(unsigned long long)nat->nat_pkts[1]);
#else
		printf("bytes %lu/%lu pkts %lu/%lu", nat->nat_bytes[0],
			nat->nat_bytes[1], nat->nat_pkts[0], nat->nat_pkts[1]);
#endif
		printf(" ipsumd %x", nat->nat_ipsumd);
	}

	if (opts & OPT_DEBUG) {
		printf("\n\tnat_next %p _pnext %p _hm %p\n",
			nat->nat_next, nat->nat_pnext, nat->nat_hm);
		printf("\t_hnext %p/%p _phnext %p/%p\n",
			nat->nat_hnext[0], nat->nat_hnext[1],
			nat->nat_phnext[0], nat->nat_phnext[1]);
		printf("\t_data %p _me %p _state %p _aps %p\n",
			nat->nat_data, nat->nat_me, nat->nat_state, nat->nat_aps);
		printf("\tfr %p ptr %p ifps %p/%p sync %p\n",
			nat->nat_fr, nat->nat_ptr, nat->nat_ifps[0],
			nat->nat_ifps[1], nat->nat_sync);
		printf("\ttqe:pnext %p next %p ifq %p parent %p/%p\n",
			nat->nat_tqe.tqe_pnext, nat->nat_tqe.tqe_next,
			nat->nat_tqe.tqe_ifq, nat->nat_tqe.tqe_parent, nat);
		printf("\ttqe:die %ld touched %ld flags %x state %d/%d\n",
			nat->nat_tqe.tqe_die, nat->nat_tqe.tqe_touched,
			nat->nat_tqe.tqe_flags, nat->nat_tqe.tqe_state[0],
			nat->nat_tqe.tqe_state[1]);
	}
	putchar('\n');
}
