/*	$FreeBSD$	*/

/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id: addkeep.c,v 1.12 2003/12/01 01:59:42 darrenr Exp $
 */

#include "ipf.h"


/*
 * Parses "keep state" and "keep frags" stuff on the end of a line.
 */
int	addkeep(cp, fp, linenum)
char	***cp;
struct	frentry	*fp;
int     linenum;
{
	char *s;

	(*cp)++;
	if (!**cp) {
		fprintf(stderr, "%d: Missing state/frag after keep\n",
			linenum);
		return -1;
	}

	if (!strcasecmp(**cp, "state")) {
		fp->fr_flags |= FR_KEEPSTATE;
		(*cp)++;
		if (**cp && !strcasecmp(**cp, "limit")) {
			(*cp)++;
			fp->fr_statemax = atoi(**cp);
			(*cp)++;
		}
		if (**cp && !strcasecmp(**cp, "scan")) {
			(*cp)++;
			if (!strcmp(**cp, "*")) {
				fp->fr_isc = NULL;
				fp->fr_isctag[0] = '\0';
			} else {
				strncpy(fp->fr_isctag, **cp,
					sizeof(fp->fr_isctag));
				fp->fr_isctag[sizeof(fp->fr_isctag)-1] = '\0';
				fp->fr_isc = NULL;
			}
			(*cp)++;
		} else
			fp->fr_isc = (struct ipscan *)-1;
	} else if (!strncasecmp(**cp, "frag", 4)) {
		fp->fr_flags |= FR_KEEPFRAG;
		(*cp)++;
	} else if (!strcasecmp(**cp, "state-age")) {
		if (fp->fr_ip.fi_p == IPPROTO_TCP) {
			fprintf(stderr, "%d: cannot use state-age with tcp\n",
				linenum);
			return -1;
		}
		if ((fp->fr_flags & FR_KEEPSTATE) == 0) {
			fprintf(stderr, "%d: state-age with no 'keep state'\n",
				linenum);
			return -1;
		}
		(*cp)++;
		if (!**cp) {
			fprintf(stderr, "%d: state-age with no arg\n",
				linenum);
			return -1;
		}
		fp->fr_age[0] = atoi(**cp);
		s = strchr(**cp, '/');
		if (s != NULL) {
			s++;
			fp->fr_age[1] = atoi(s);
		} else
			fp->fr_age[1] = fp->fr_age[0];
	} else {
		fprintf(stderr, "%d: Unrecognised state keyword \"%s\"\n",
			linenum, **cp);
		return -1;
	}
	return 0;
}
