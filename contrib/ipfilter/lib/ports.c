/*	$FreeBSD$	*/

/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * Id: ports.c,v 1.9.4.1 2004/12/09 19:41:22 darrenr Exp
 */

#include <ctype.h>

#include "ipf.h"


/*
 * check for possible presence of the port fields in the line
 */
int	ports(seg, proto, pp, cp, tp, linenum)
char	***seg;
char	*proto;
u_short	*pp;
int	*cp;
u_short	*tp;
int     linenum;
{
	int	comp = -1;

	if (!*seg || !**seg || !***seg)
		return 0;
	if (!strcasecmp(**seg, "port") && *(*seg + 1) && *(*seg + 2)) {
		(*seg)++;
		if (ISALNUM(***seg) && *(*seg + 2)) {
			if (portnum(**seg, proto, pp, linenum) == 0)
				return -1;
			(*seg)++;
			if (!strcmp(**seg, "<>"))
				comp = FR_OUTRANGE;
			else if (!strcmp(**seg, "><"))
				comp = FR_INRANGE;
			else {
				fprintf(stderr,
					"%d: unknown range operator (%s)\n",
					linenum, **seg);
				return -1;
			}
			(*seg)++;
			if (**seg == NULL) {
				fprintf(stderr, "%d: missing 2nd port value\n",
					linenum);
				return -1;
			}
			if (portnum(**seg, proto, tp, linenum) == 0)
				return -1;
		} else if (!strcmp(**seg, "=") || !strcasecmp(**seg, "eq"))
			comp = FR_EQUAL;
		else if (!strcmp(**seg, "!=") || !strcasecmp(**seg, "ne"))
			comp = FR_NEQUAL;
		else if (!strcmp(**seg, "<") || !strcasecmp(**seg, "lt"))
			comp = FR_LESST;
		else if (!strcmp(**seg, ">") || !strcasecmp(**seg, "gt"))
			comp = FR_GREATERT;
		else if (!strcmp(**seg, "<=") || !strcasecmp(**seg, "le"))
			comp = FR_LESSTE;
		else if (!strcmp(**seg, ">=") || !strcasecmp(**seg, "ge"))
			comp = FR_GREATERTE;
		else {
			fprintf(stderr, "%d: unknown comparator (%s)\n",
					linenum, **seg);
			return -1;
		}
		if (comp != FR_OUTRANGE && comp != FR_INRANGE) {
			(*seg)++;
			if (portnum(**seg, proto, pp, linenum) == 0)
				return -1;
		}
		*cp = comp;
		(*seg)++;
	}
	return 0;
}
