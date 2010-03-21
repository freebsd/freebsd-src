/*	$FreeBSD: src/contrib/ipfilter/lib/getportproto.c,v 1.3.12.1 2010/02/10 00:26:20 kensmith Exp $	*/

/*
 * Copyright (C) 2002-2005 by Darren Reed.
 * 
 * See the IPFILTER.LICENCE file for details on licencing.  
 *   
 * $Id: getportproto.c,v 1.2.4.4 2006/06/16 17:21:00 darrenr Exp $ 
 */     

#include <ctype.h>
#include "ipf.h"

int getportproto(name, proto)
char *name;
int proto;
{
	struct servent *s;
	struct protoent *p;

	if (ISDIGIT(*name)) {
		int number;
		char *s;

		for (s = name; *s != '\0'; s++)
			if (!ISDIGIT(*s))
				return -1;

		number = atoi(name);
		if (number < 0 || number > 65535)
			return -1;
		return htons(number);
	}

	p = getprotobynumber(proto);
	s = getservbyname(name, p ? p->p_name : NULL);
	if (s != NULL)
		return s->s_port;
	return -1;
}
