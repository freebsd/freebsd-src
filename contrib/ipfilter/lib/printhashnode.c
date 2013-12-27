/*	$FreeBSD$	*/

/*
 * Copyright (C) 2012 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */

#include "ipf.h"


iphtent_t *
printhashnode(iph, ipep, copyfunc, opts, fields)
	iphtable_t *iph;
	iphtent_t *ipep;
	copyfunc_t copyfunc;
	int opts;
	wordtab_t *fields;
{
	iphtent_t ipe;
	u_int hv;
	int i;

	if ((*copyfunc)(ipep, &ipe, sizeof(ipe)))
		return NULL;

	hv = IPE_V4_HASH_FN(ipe.ipe_addr.i6[0], ipe.ipe_mask.i6[0],
			    iph->iph_size);

	if (fields != NULL) {
		for (i = 0; fields[i].w_value != 0; i++) {
			printpoolfield(&ipe, IPLT_HASH, i);
			if (fields[i + 1].w_value != 0)
				printf("\t");
		}
		printf("\n");
	} else if ((opts & OPT_DEBUG) != 0) {
		PRINTF("\t%d\tAddress: %s", hv,
			inet_ntoa(ipe.ipe_addr.in4));
		printmask(ipe.ipe_family, (u_32_t *)&ipe.ipe_mask.in4_addr);
		PRINTF("\tRef. Count: %d\tGroup: %s\n", ipe.ipe_ref,
			ipe.ipe_group);
#ifdef USE_QUAD_T
		PRINTF("\tHits: %"PRIu64"\tBytes: %"PRIu64"\n",
		       ipe.ipe_hits, ipe.ipe_bytes);
#else
		PRINTF("\tHits: %lu\tBytes: %lu\n",
		       ipe.ipe_hits, ipe.ipe_bytes);
#endif
	} else {
		putchar(' ');
		printip(ipe.ipe_family, (u_32_t *)&ipe.ipe_addr.in4_addr);
		printmask(ipe.ipe_family, (u_32_t *)&ipe.ipe_mask.in4_addr);
		if (ipe.ipe_value != 0) {
			switch (iph->iph_type & ~IPHASH_ANON)
			{
			case IPHASH_GROUPMAP :
				if (strncmp(ipe.ipe_group, iph->iph_name,
					    FR_GROUPLEN))
					PRINTF(", group=%s", ipe.ipe_group);
				break;
			}
		}
		putchar(';');
	}

	ipep = ipe.ipe_next;
	return ipep;
}
