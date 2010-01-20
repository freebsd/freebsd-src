/*	$FreeBSD$	*/

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
