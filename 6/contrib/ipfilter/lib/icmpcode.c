/*	$FreeBSD$	*/

/*
 * Copyright (C) 2000-2006 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: icmpcode.c,v 1.7.2.5 2006/06/16 17:21:02 darrenr Exp $
 */

#include <ctype.h>

#include "ipf.h"

#ifndef	MIN
# define	MIN(a,b)	((a) > (b) ? (b) : (a))
#endif


char	*icmpcodes[MAX_ICMPCODE + 1] = {
	"net-unr", "host-unr", "proto-unr", "port-unr", "needfrag", "srcfail",
	"net-unk", "host-unk", "isolate", "net-prohib", "host-prohib",
	"net-tos", "host-tos", "filter-prohib", "host-preced", "preced-cutoff",
	NULL };
