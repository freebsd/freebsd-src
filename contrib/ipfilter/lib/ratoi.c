/*	$FreeBSD$	*/

/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: ratoi.c,v 1.4 2001/06/09 17:09:25 darrenr Exp $
 */

#include "ipf.h"


int	ratoi(ps, pi, min, max)
char 	*ps;
int	*pi, min, max;
{
	int i;
	char *pe;

	i = (int)strtol(ps, &pe, 0);
	if (*pe != '\0' || i < min || i > max)
		return 0;
	*pi = i;
	return 1;
}
