/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)getnetgrent.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */

#include <stdio.h>
#include <strings.h>

#define _PATH_NETGROUP "/etc/netgroup"

/*
 * Static Variables and functions used by setnetgrent(), getnetgrent() and
 * endnetgrent().
 * There are two linked lists:
 * - linelist is just used by setnetgrent() to parse the net group file via.
 *   parse_netgrp()
 * - netgrp is the list of entries for the current netgroup
 */
struct linelist {
	struct linelist	*l_next;	/* Chain ptr. */
	int		l_parsed;	/* Flag for cycles */
	char		*l_groupname;	/* Name of netgroup */
	char		*l_line;	/* Netgroup entrie(s) to be parsed */
};

struct netgrp {
	struct netgrp	*ng_next;	/* Chain ptr */
	char		*ng_str[3];	/* Field pointers, see below */
};
#define NG_HOST		0	/* Host name */
#define NG_USER		1	/* User name */
#define NG_DOM		2	/* and Domain name */

static struct linelist	*linehead = (struct linelist *)0;
static struct netgrp	*nextgrp = (struct netgrp *)0;
static struct {
	struct netgrp	*gr;
	char		*grname;
} grouphead = {
	(struct netgrp *)0,
	(char *)0,
};
static FILE *netf = (FILE *)0;
static int parse_netgrp();
static struct linelist *read_for_group();
void setnetgrent(), endnetgrent();
int getnetgrent(), innetgr();

#define	LINSIZ	1024	/* Length of netgroup file line */

/*
 * setnetgrent()
 * Parse the netgroup file looking for the netgroup and build the list
 * of netgrp structures. Let parse_netgrp() and read_for_group() do
 * most of the work.
 */
void
setnetgrent(group)
	char *group;
{

	if (grouphead.gr == (struct netgrp *)0 ||
		strcmp(group, grouphead.grname)) {
		endnetgrent();
		if (netf = fopen(_PATH_NETGROUP, "r")) {
			if (parse_netgrp(group))
				endnetgrent();
			else {
				grouphead.grname = (char *)
					malloc(strlen(group) + 1);
				strcpy(grouphead.grname, group);
			}
			fclose(netf);
		}
	}
	nextgrp = grouphead.gr;
}

/*
 * Get the next netgroup off the list.
 */
int
getnetgrent(hostp, userp, domp)
	char **hostp, **userp, **domp;
{

	if (nextgrp) {
		*hostp = nextgrp->ng_str[NG_HOST];
		*userp = nextgrp->ng_str[NG_USER];
		*domp = nextgrp->ng_str[NG_DOM];
		nextgrp = nextgrp->ng_next;
		return (1);
	}
	return (0);
}

/*
 * endnetgrent() - cleanup
 */
void
endnetgrent()
{
	register struct linelist *lp, *olp;
	register struct netgrp *gp, *ogp;

	lp = linehead;
	while (lp) {
		olp = lp;
		lp = lp->l_next;
		free(olp->l_groupname);
		free(olp->l_line);
		free((char *)olp);
	}
	linehead = (struct linelist *)0;
	if (grouphead.grname) {
		free(grouphead.grname);
		grouphead.grname = (char *)0;
	}
	gp = grouphead.gr;
	while (gp) {
		ogp = gp;
		gp = gp->ng_next;
		if (ogp->ng_str[NG_HOST])
			free(ogp->ng_str[NG_HOST]);
		if (ogp->ng_str[NG_USER])
			free(ogp->ng_str[NG_USER]);
		if (ogp->ng_str[NG_DOM])
			free(ogp->ng_str[NG_DOM]);
		free((char *)ogp);
	}
	grouphead.gr = (struct netgrp *)0;
}

/*
 * Search for a match in a netgroup.
 */
int
innetgr(group, host, user, dom)
	char *group, *host, *user, *dom;
{
	char *hst, *usr, *dm;

	setnetgrent(group);
	while (getnetgrent(&hst, &usr, &dm))
		if ((host == (char *)0 || !strcmp(host, hst)) &&
		    (user == (char *)0 || !strcmp(user, usr)) &&
		    (dom == (char *)0 || !strcmp(dom, dm))) {
			endnetgrent();
			return (1);
		}
	endnetgrent();
	return (0);
}

/*
 * Parse the netgroup file setting up the linked lists.
 */
static int
parse_netgrp(group)
	char *group;
{
	register char *spos, *epos;
	register int len, strpos;
	char *pos, *gpos;
	struct netgrp *grp;
	struct linelist *lp = linehead;

	/*
	 * First, see if the line has already been read in.
	 */
	while (lp) {
		if (!strcmp(group, lp->l_groupname))
			break;
		lp = lp->l_next;
	}
	if (lp == (struct linelist *)0 &&
	    (lp = read_for_group(group)) == (struct linelist *)0)
		return (1);
	if (lp->l_parsed) {
		fprintf(stderr, "Cycle in netgroup %s\n", lp->l_groupname);
		return (1);
	} else
		lp->l_parsed = 1;
	pos = lp->l_line;
	while (*pos != '\0') {
		if (*pos == '(') {
			grp = (struct netgrp *)malloc(sizeof (struct netgrp));
			bzero((char *)grp, sizeof (struct netgrp));
			grp->ng_next = grouphead.gr;
			grouphead.gr = grp;
			pos++;
			gpos = strsep(&pos, ")");
			for (strpos = 0; strpos < 3; strpos++) {
				if (spos = strsep(&gpos, ",")) {
					while (*spos == ' ' || *spos == '\t')
						spos++;
					if (epos = strpbrk(spos, " \t")) {
						*epos = '\0';
						len = epos - spos;
					} else
						len = strlen(spos);
					if (len > 0) {
						grp->ng_str[strpos] =  (char *)
							malloc(len + 1);
						bcopy(spos, grp->ng_str[strpos],
							len + 1);
					}
				} else
					goto errout;
			}
		} else {
			spos = strsep(&pos, ", \t");
			if (parse_netgrp(spos))
				return (1);
		}
		while (*pos == ' ' || *pos == ',' || *pos == '\t')
			pos++;
	}
	return (0);
errout:
	fprintf(stderr, "Bad netgroup %s at ..%s\n", lp->l_groupname,
		spos);
	return (1);
}

/*
 * Read the netgroup file and save lines until the line for the netgroup
 * is found. Return 1 if eof is encountered.
 */
static struct linelist *
read_for_group(group)
	char *group;
{
	register char *pos, *spos, *linep, *olinep;
	register int len, olen;
	int cont;
	struct linelist *lp;
	char line[LINSIZ + 1];

	while (fgets(line, LINSIZ, netf) != NULL) {
		pos = line;
		if (*pos == '#')
			continue;
		while (*pos == ' ' || *pos == '\t')
			pos++;
		spos = pos;
		while (*pos != ' ' && *pos != '\t' && *pos != '\n' &&
			*pos != '\0')
			pos++;
		len = pos - spos;
		while (*pos == ' ' || *pos == '\t')
			pos++;
		if (*pos != '\n' && *pos != '\0') {
			lp = (struct linelist *)malloc(sizeof (*lp));
			lp->l_parsed = 0;
			lp->l_groupname = (char *)malloc(len + 1);
			bcopy(spos, lp->l_groupname, len);
			*(lp->l_groupname + len) = '\0';
			len = strlen(pos);
			olen = 0;

			/*
			 * Loop around handling line continuations.
			 */
			do {
				if (*(pos + len - 1) == '\n')
					len--;
				if (*(pos + len - 1) == '\\') {
					len--;
					cont = 1;
				} else
					cont = 0;
				if (len > 0) {
					linep = (char *)malloc(olen + len + 1);
					if (olen > 0) {
						bcopy(olinep, linep, olen);
						free(olinep);
					}
					bcopy(pos, linep + olen, len);
					olen += len;
					*(linep + olen) = '\0';
					olinep = linep;
				}
				if (cont) {
					if (fgets(line, LINSIZ, netf)) {
						pos = line;
						len = strlen(pos);
					} else
						cont = 0;
				}
			} while (cont);
			lp->l_line = linep;
			lp->l_next = linehead;
			linehead = lp;

			/*
			 * If this is the one we wanted, we are done.
			 */
			if (!strcmp(lp->l_groupname, group))
				return (lp);
		}
	}
	return ((struct linelist *)0);
}
