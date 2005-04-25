/*	$NetBSD$	*/

/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * Id: initparse.c,v 1.6 2002/01/28 06:50:46 darrenr Exp
 */
#include "ipf.h"


char	thishost[MAXHOSTNAMELEN];


void initparse __P((void))
{
	gethostname(thishost, sizeof(thishost));
	thishost[sizeof(thishost) - 1] = '\0';
}
