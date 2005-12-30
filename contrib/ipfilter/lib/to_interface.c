/*	$FreeBSD$	*/

/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: to_interface.c,v 1.8 2002/01/28 06:50:48 darrenr Exp $
 */

#include "ipf.h"


int to_interface(fdp, to, linenum)
frdest_t *fdp;
char *to;
int linenum;
{
	char *s;

	s = strchr(to, ':');
	fdp->fd_ifp = NULL;
	if (s) {
		*s++ = '\0';
		if (hostnum((u_32_t *)&fdp->fd_ip, s, linenum, NULL) == -1)
			return -1;
	}
	(void) strncpy(fdp->fd_ifname, to, sizeof(fdp->fd_ifname) - 1);
	fdp->fd_ifname[sizeof(fdp->fd_ifname) - 1] = '\0';
	return 0;
}
