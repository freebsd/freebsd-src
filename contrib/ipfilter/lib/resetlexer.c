/*	$FreeBSD: src/contrib/ipfilter/lib/resetlexer.c,v 1.3.10.1.6.1 2010/12/21 17:09:25 kensmith Exp $	*/

/*
 * Copyright (C) 2002 by Darren Reed.
 * 
 * See the IPFILTER.LICENCE file for details on licencing.  
 *   
 * $Id: resetlexer.c,v 1.1.4.1 2006/06/16 17:21:16 darrenr Exp $ 
 */     

#include "ipf.h"

long	string_start = -1;
long	string_end = -1;
char	*string_val = NULL;
long	pos = 0;


void resetlexer()
{
	string_start = -1;
	string_end = -1;
	string_val = NULL;
	pos = 0;
}
