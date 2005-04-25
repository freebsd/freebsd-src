/*	$NetBSD$	*/

/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * Id: printportcmp.c,v 1.7 2003/02/16 02:31:05 darrenr Exp
 */

#include "ipf.h"


void	printportcmp(pr, frp)
int	pr;
frpcmp_t	*frp;
{
	static char *pcmp1[] = { "*", "=", "!=", "<", ">", "<=", ">=",
				 "<>", "><", ":" };

	if (frp->frp_cmp == FR_INRANGE || frp->frp_cmp == FR_OUTRANGE)
		printf(" port %d %s %d", frp->frp_port,
			     pcmp1[frp->frp_cmp], frp->frp_top);
	else if (frp->frp_cmp == FR_INCRANGE)
		printf(" port %d:%d", frp->frp_port, frp->frp_top);
	else
		printf(" port %s %s", pcmp1[frp->frp_cmp],
			     portname(pr, frp->frp_port));
}
