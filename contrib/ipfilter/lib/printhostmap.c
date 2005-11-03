/*	$FreeBSD: src/contrib/ipfilter/lib/printhostmap.c,v 1.2 2005/04/25 18:20:12 darrenr Exp $	*/

#include "ipf.h"

void printhostmap(hmp, hv)
hostmap_t *hmp;
u_int hv;
{
	printf("%s,", inet_ntoa(hmp->hm_srcip));
	printf("%s -> ", inet_ntoa(hmp->hm_dstip));
	printf("%s ", inet_ntoa(hmp->hm_mapip));
	printf("(use = %d hv = %u)\n", hmp->hm_ref, hv);
}
