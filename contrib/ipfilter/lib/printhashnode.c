/*	$NetBSD$	*/

/*
 * Copyright (C) 2002 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */

#include "ipf.h"

#define	PRINTF	(void)printf
#define	FPRINTF	(void)fprintf

iphtent_t *printhashnode(iph, ipep, copyfunc, opts)
iphtable_t *iph;
iphtent_t *ipep;
copyfunc_t copyfunc;
int opts;
{
	iphtent_t ipe;

	if ((*copyfunc)(ipep, &ipe, sizeof(ipe)))
		return NULL;

	ipe.ipe_addr.in4_addr = htonl(ipe.ipe_addr.in4_addr);
	ipe.ipe_mask.in4_addr = htonl(ipe.ipe_mask.in4_addr);

	if ((opts & OPT_DEBUG) != 0) {
		PRINTF("\tAddress: %s",
			inet_ntoa(ipe.ipe_addr.in4));
		printmask((u_32_t *)&ipe.ipe_mask.in4_addr);
		PRINTF("\tRef. Count: %d\tGroup: %s\n", ipe.ipe_ref,
			ipe.ipe_group);
	} else {
		putchar(' ');
		printip((u_32_t *)&ipe.ipe_addr.in4_addr);
		printmask((u_32_t *)&ipe.ipe_mask.in4_addr);
		if (ipe.ipe_value != 0) {
			switch (iph->iph_type & ~IPHASH_ANON)
			{
			case IPHASH_GROUPMAP :
				if (strncmp(ipe.ipe_group, iph->iph_name,
					    FR_GROUPLEN))
					PRINTF(", group = %s", ipe.ipe_group);
				break;
			}
		}
		putchar(';');
	}
	ipep = ipe.ipe_next;
	return ipep;
}
