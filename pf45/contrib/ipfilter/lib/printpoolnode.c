/*	$FreeBSD$	*/

/*
 * Copyright (C) 2002-2005 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */

#include "ipf.h"

#define	PRINTF	(void)printf
#define	FPRINTF	(void)fprintf

ip_pool_node_t *printpoolnode(np, opts)
ip_pool_node_t *np;
int opts;
{

	if ((opts & OPT_DEBUG) == 0) {
		putchar(' ');
		if (np->ipn_info == 1)
			PRINTF("! ");
		printip((u_32_t *)&np->ipn_addr.adf_addr.in4);
		printmask((u_32_t *)&np->ipn_mask.adf_addr);
	} else {
		PRINTF("\tAddress: %s%s", np->ipn_info ? "! " : "",
			inet_ntoa(np->ipn_addr.adf_addr.in4));
		printmask((u_32_t *)&np->ipn_mask.adf_addr);
		PRINTF("\t\tHits %lu\tName %s\tRef %d\n",
			np->ipn_hits, np->ipn_name, np->ipn_ref);
	}
	return np->ipn_next;
}
