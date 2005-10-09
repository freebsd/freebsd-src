/*	$FreeBSD$	*/

/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * Id: addicmp.c,v 1.10.2.1 2004/12/09 19:41:16 darrenr Exp
 */

#include <ctype.h>

#include "ipf.h"


char	*icmptypes[MAX_ICMPTYPE + 1] = {
	"echorep", (char *)NULL, (char *)NULL, "unreach", "squench",
	"redir", (char *)NULL, (char *)NULL, "echo", "routerad",
	"routersol", "timex", "paramprob", "timest", "timestrep",
	"inforeq", "inforep", "maskreq", "maskrep", "END"
};

/*
 * set the icmp field to the correct type if "icmp" word is found
 */
int	addicmp(cp, fp, linenum)
char	***cp;
struct	frentry	*fp;
int     linenum;
{
	char	**t;
	int	i;

	(*cp)++;
	if (!**cp)
		return -1;
	if (!fp->fr_proto)	/* to catch lusers */
		fp->fr_proto = IPPROTO_ICMP;
	if (ISDIGIT(***cp)) {
		if (!ratoi(**cp, &i, 0, 255)) {
			fprintf(stderr,
				"%d: Invalid icmp-type (%s) specified\n",
				linenum, **cp);
			return -1;
		}
	} else {
		for (t = icmptypes, i = 0; ; t++, i++) {
			if (!*t)
				continue;
			if (!strcasecmp("END", *t)) {
				i = -1;
				break;
			}
			if (!strcasecmp(*t, **cp))
				break;
		}
		if (i == -1) {
			fprintf(stderr,
				"%d: Unknown icmp-type (%s) specified\n",
				linenum, **cp);
			return -1;
		}
	}
	fp->fr_icmp = (u_short)(i << 8);
	fp->fr_icmpm = (u_short)0xff00;
	(*cp)++;
	if (!**cp)
		return 0;

	if (**cp && strcasecmp("code", **cp))
		return 0;
	(*cp)++;
	if (ISDIGIT(***cp)) {
		if (!ratoi(**cp, &i, 0, 255)) {
			fprintf(stderr,
				"%d: Invalid icmp code (%s) specified\n",
				linenum, **cp);
			return -1;
		}
	} else {
		i = icmpcode(**cp);
		if (i == -1) {
			fprintf(stderr,
				"%d: Unknown icmp code (%s) specified\n",
				linenum, **cp);
			return -1;
		}
	}
	i &= 0xff;
	fp->fr_icmp |= (u_short)i;
	fp->fr_icmpm = (u_short)0xffff;
	(*cp)++;
	return 0;
}
