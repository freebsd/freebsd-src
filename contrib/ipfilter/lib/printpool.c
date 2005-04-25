/*	$NetBSD$	*/

/*
 * Copyright (C) 2002 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */

#include "ipf.h"

#define	PRINTF	(void)printf
#define	FPRINTF	(void)fprintf

ip_pool_t *printpool(pp, copyfunc, name, opts)
ip_pool_t *pp;
copyfunc_t copyfunc;
char *name;
int opts;
{
	ip_pool_node_t *ipnp, *ipnpn, ipn;
	ip_pool_t ipp;

	if ((*copyfunc)(pp, &ipp, sizeof(ipp)))
		return NULL;

	if ((name != NULL) && strncmp(name, ipp.ipo_name, FR_GROUPLEN))
		return ipp.ipo_next;

	if ((opts & OPT_DEBUG) == 0) {
		if ((ipp.ipo_flags & IPOOL_ANON) != 0)
			PRINTF("# 'anonymous' tree %s\n", ipp.ipo_name);
		PRINTF("table role = ");
	} else {
		PRINTF("Name: %s", ipp.ipo_name);
		if ((ipp.ipo_flags & IPOOL_ANON) == IPOOL_ANON)
			PRINTF("(anon)");
		putchar(' ');
		PRINTF("Role: ");
	}

	switch (ipp.ipo_unit)
	{
	case IPL_LOGIPF :
		printf("ipf");
		break;
	case IPL_LOGNAT :
		printf("nat");
		break;
	case IPL_LOGSTATE :
		printf("state");
		break;
	case IPL_LOGAUTH :
		printf("auth");
		break;
	case IPL_LOGSYNC :
		printf("sync");
		break;
	case IPL_LOGSCAN :
		printf("scan");
		break;
	case IPL_LOGLOOKUP :
		printf("lookup");
		break;
	case IPL_LOGCOUNT :
		printf("count");
		break;
	default :
		printf("unknown(%d)", ipp.ipo_unit);
	}

	if ((opts & OPT_DEBUG) == 0) {
		PRINTF(" type = tree number = %s\n", ipp.ipo_name);
		PRINTF("\t{");
	} else {
		putchar(' ');

		PRINTF("\tReferences: %d\tHits: %lu\n", ipp.ipo_ref,
			ipp.ipo_hits);
		PRINTF("\tNodes Starting at %p\n", ipp.ipo_list);
	}

	ipnpn = ipp.ipo_list;
	ipp.ipo_list = NULL;
	while (ipnpn != NULL) {
		ipnp = (ip_pool_node_t *)malloc(sizeof(*ipnp));
		(*copyfunc)(ipnpn, ipnp, sizeof(ipn));
		ipnpn = ipnp->ipn_next;
		ipnp->ipn_next = ipp.ipo_list;
		ipp.ipo_list = ipnp;
	}

	if (ipp.ipo_list == NULL) {
		putchar(';');
	} else {
		for (ipnp = ipp.ipo_list; ipnp != NULL; ) {
			ipnp = printpoolnode(ipnp, opts);

			if ((opts & OPT_DEBUG) == 0) {
				putchar(';');
			}
		}
	}

	if ((opts & OPT_DEBUG) == 0)
		PRINTF(" };\n");

	return ipp.ipo_next;
}
