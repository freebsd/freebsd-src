/*	$FreeBSD: src/contrib/ipfilter/lib/initparse.c,v 1.4.10.1.4.1 2010/06/14 02:09:06 kensmith Exp $	*/

/*
 * Copyright (C) 2000-2002 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: initparse.c,v 1.6.4.1 2006/06/16 17:21:02 darrenr Exp $
 */
#include "ipf.h"


char	thishost[MAXHOSTNAMELEN];


void initparse __P((void))
{
	gethostname(thishost, sizeof(thishost));
	thishost[sizeof(thishost) - 1] = '\0';
}
