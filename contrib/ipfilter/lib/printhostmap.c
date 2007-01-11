/*	$FreeBSD: src/contrib/ipfilter/lib/printhostmap.c,v 1.2.2.1 2006/08/24 07:37:05 guido Exp $	*/

#include "ipf.h"

void printhostmap(hmp, hv)
hostmap_t *hmp;
u_int hv;
{
	struct in_addr in;

	printf("%s,", inet_ntoa(hmp->hm_srcip));
	printf("%s -> ", inet_ntoa(hmp->hm_dstip));
	in.s_addr = htonl(hmp->hm_mapip.s_addr);
	printf("%s ", inet_ntoa(in));
	printf("(use = %d hv = %u)\n", hmp->hm_ref, hv);
}
