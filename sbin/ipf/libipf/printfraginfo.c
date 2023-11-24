
/*
 * Copyright (C) 2012 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id$
 */
#include "ipf.h"
#include "kmem.h"


void
printfraginfo(char *prefix, struct ipfr *ifr)
{
	int family;

	PRINTF("%s", prefix);
	if (ifr->ipfr_v == 6) {
		PRINTF("inet6");
		family = AF_INET6;
	} else {
		PRINTF("inet");
		family = AF_INET;
	}

	PRINTF(" %s -> ", hostname(family, &ifr->ipfr_src));
	PRINTF("%s id %x ttl %lu pr %d pkts %u bytes %u seen0 %d ref %d\n",
		hostname(family, &ifr->ipfr_dst), ifr->ipfr_id,
		ifr->ipfr_ttl, ifr->ipfr_p, ifr->ipfr_pkts, ifr->ipfr_bytes,
		ifr->ipfr_seen0, ifr->ipfr_ref);
}
