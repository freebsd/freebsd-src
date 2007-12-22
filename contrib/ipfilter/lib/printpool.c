/*	$FreeBSD$	*/

/*
 * Copyright (C) 2002-2005 by Darren Reed.
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

	printpooldata(&ipp, opts);

	if ((ipp.ipo_flags & IPOOL_DELETE) != 0)
		PRINTF("# ");
	if ((opts & OPT_DEBUG) == 0)
		PRINTF("\t{");

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
