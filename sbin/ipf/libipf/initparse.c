
/*
 * Copyright (C) 2012 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id$
 */
#include "ipf.h"


char	thishost[MAXHOSTNAMELEN];


void initparse(void)
{
	gethostname(thishost, sizeof(thishost));
	thishost[sizeof(thishost) - 1] = '\0';
}
