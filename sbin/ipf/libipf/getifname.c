/*	$FreeBSD$	*/

/*
 * Copyright (C) 2012 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id$
 */

#include "ipf.h"

#include "kmem.h"

/*
 * Given a pointer to an interface in the kernel, return a pointer to a
 * string which is the interface name.
 */
char *
getifname(struct ifnet *ptr)
{
#if SOLARIS
#  include <sys/mutex.h>
#  include <sys/condvar.h>
# include "../pfil/qif.h"
	char *ifname;
	qif_t qif;

	if ((void *)ptr == (void *)-1)
		return "!";
	if (ptr == NULL)
		return "-";

	if (kmemcpy((char *)&qif, (u_long)ptr, sizeof(qif)) == -1)
		return "X";
	ifname = strdup(qif.qf_name);
	if ((ifname != NULL) && (*ifname == '\0')) {
		free(ifname);
		return "!";
	}
	return ifname;
#else
	struct ifnet netif;

	if ((void *)ptr == (void *)-1)
		return "!";
	if (ptr == NULL)
		return "-";

	if (kmemcpy((char *)&netif, (u_long)ptr, sizeof(netif)) == -1)
		return "X";
	return strdup(netif.if_xname);
#endif
}
