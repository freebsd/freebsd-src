/*	$NetBSD$	*/

/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * Id: extras.c,v 1.12 2002/07/13 12:06:49 darrenr Exp
 */

#include "ipf.h"


/*
 * deal with extra bits on end of the line
 */
int	extras(cp, fr, linenum)
char	***cp;
struct	frentry	*fr;
int     linenum;
{
	u_short	secmsk;
	u_long	opts;
	int	notopt;

	opts = 0;
	secmsk = 0;
	notopt = 0;
	(*cp)++;
	if (!**cp)
		return -1;

	while (**cp) {
		if (!strcasecmp(**cp, "not") || !strcasecmp(**cp, "no")) {
			notopt = 1;
			(*cp)++;
			continue;
		} else if (!strncasecmp(**cp, "ipopt", 5)) {
			if (!notopt)
				fr->fr_flx |= FI_OPTIONS;
			fr->fr_mflx |= FI_OPTIONS;
			goto nextopt;
		} else if (!strcasecmp(**cp, "lowttl")) {
			if (!notopt)
				fr->fr_flx |= FI_LOWTTL;
			fr->fr_mflx |= FI_LOWTTL;
			goto nextopt;
		} else if (!strcasecmp(**cp, "bad-src")) {
			if (!notopt)
				fr->fr_flx |= FI_BADSRC;
			fr->fr_mflx |= FI_BADSRC;
			goto nextopt;
		} else if (!strncasecmp(**cp, "mbcast", 6)) {
			if (!notopt)
				fr->fr_flx |= FI_MBCAST;
			fr->fr_mflx |= FI_MBCAST;
			goto nextopt;
		} else if (!strncasecmp(**cp, "nat", 3)) {
			if (!notopt)
				fr->fr_flx |= FI_NATED;
			fr->fr_mflx |= FI_NATED;
			goto nextopt;
		} else if (!strncasecmp(**cp, "frag", 4)) {
			if (!notopt)
				fr->fr_flx |= FI_FRAG;
			fr->fr_mflx |= FI_FRAG;
			goto nextopt;
		} else if (!strncasecmp(**cp, "opt", 3)) {
			if (!*(*cp + 1)) {
				fprintf(stderr, "%d: opt missing arguements\n",
					linenum);
				return -1;
			}
			(*cp)++;
			if (!(opts = optname(cp, &secmsk, linenum)))
				return -1;

			if (notopt) {
				if (!secmsk) {
					fr->fr_optmask |= opts;
				} else {
					fr->fr_optmask |= (opts & ~0x0100);
					fr->fr_secmask |= secmsk;
				}
				fr->fr_secbits &= ~secmsk;
				fr->fr_optbits &= ~opts;
			} else {
				fr->fr_optmask |= opts;
				fr->fr_secmask |= secmsk;
				fr->fr_optbits |= opts;
				fr->fr_secbits |= secmsk;
			}
		} else if (!strncasecmp(**cp, "short", 5)) {
			if (fr->fr_tcpf) {
				fprintf(stderr,
				"%d: short cannot be used with TCP flags\n",
					linenum);
				return -1;
			}

			if (!notopt)
				fr->fr_flx |= FI_SHORT;
			fr->fr_mflx |= FI_SHORT;
			goto nextopt;
		} else
			return -1;
nextopt:
		notopt = 0;
		opts = 0;
		secmsk = 0;
		(*cp)++;
	}
	return 0;
}
