/*	$FreeBSD: src/contrib/ipfilter/lib/resetlexer.c,v 1.2 2005/04/25 18:20:12 darrenr Exp $	*/

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
