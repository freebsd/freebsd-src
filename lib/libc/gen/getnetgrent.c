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
static char sccsid[] = "@(#)getnetgrent.c	8.2 (Berkeley) 4/27/95";
#endif /* LIBC_SCCS and not lint */

#include <stdio.h>
#include <strings.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef YP
/*
 * Notes:
 * We want to be able to use NIS netgroups properly while retaining
 * the ability to use a local /etc/netgroup file. Unfortunately, you
 * can't really do both at the same time - at least, not efficiently.
 * NetBSD deals with this problem by creating a netgroup database
 * using Berkeley DB (just like the password database) that allows
 * for lookups using netgroup, netgroup.byuser or netgroup.byhost
 * searches. This is a neat idea, but I don't have time to implement
 * something like that now. (I think ultimately it would be nice
 * if we DB-fied the group and netgroup stuff all in one shot, but
 * for now I'm satisfied just to have something that works well
 * without requiring massive code changes.)
 * 
 * Therefore, to still permit the use of the local file and maintain
 * optimum NIS performance, we allow for the following conditions:
 *
 * - If /etc/netgroup does not exist and NIS is turned on, we use
 *   NIS netgroups only.
 *
 * - If /etc/netgroup exists but is empty, we use NIS netgroups
 *   only.
 *
 * - If /etc/netgroup exists and contains _only_ a '+', we use
 *   NIS netgroups only.
 *
 * - If /etc/netgroup exists, contains locally defined netgroups
 *   and a '+', we use a mixture of NIS and the local entries.
 *   This method should return the same NIS data as just using
 *   NIS alone, but it will be slower if the NIS netgroup database
 *   is large (innetgr() in particular will suffer since extra
 *   processing has to be done in order to determine memberships
 *   using just the raw netgroup data).
 *
 * - If /etc/netgroup exists and contains only locally defined
 *   netgroup entries, we use just those local entries and ignore
 *   NIS (this is the original, pre-NIS behavior).
 */

#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/errno.h>
static char *_netgr_yp_domain;
int _use_only_yp;
static int _netgr_yp_enabled;
static int _yp_innetgr;
#endif

#ifndef _PATH_NETGROUP
#define _PATH_NETGROUP "/etc/netgroup"
#endif

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
#ifdef YP
	struct stat _yp_statp;
	char _yp_plus;
#endif

	/* Sanity check */

	if (group == NULL || !strlen(group))
		return;

	if (grouphead.gr == (struct netgrp *)0 ||
		strcmp(group, grouphead.grname)) {
		endnetgrent();
#ifdef YP
		/* Presumed guilty until proven innocent. */
		_use_only_yp = 0;
		/*
		 * If /etc/netgroup doesn't exist or is empty,
		 * use NIS exclusively.
		 */
		if (((stat(_PATH_NETGROUP, &_yp_statp) < 0) &&
			errno == ENOENT) || _yp_statp.st_size == 0)
			_use_only_yp = _netgr_yp_enabled = 1;
		if ((netf = fopen(_PATH_NETGROUP,"r")) != NULL ||_use_only_yp){
		/*
		 * Icky: grab the first character of the netgroup file
		 * and turn on NIS if it's a '+'. rewind the stream
		 * afterwards so we don't goof up read_for_group() later.
		 */
			if (netf) {
				fscanf(netf, "%c", &_yp_plus);
				rewind(netf);
				if (_yp_plus == '+')
					_use_only_yp = _netgr_yp_enabled = 1;
			}
		/*
		 * If we were called specifically for an innetgr()
		 * lookup and we're in NIS-only mode, short-circuit
		 * parse_netgroup() and cut directly to the chase.
		 */
			if (_use_only_yp && _yp_innetgr) {
				/* dohw! */
				if (netf != NULL)
					fclose(netf);
				return;
			}
#else
		if (netf = fopen(_PATH_NETGROUP, "r")) {
#endif
			if (parse_netgrp(group))
				endnetgrent();
			else {
				grouphead.grname = (char *)
					malloc(strlen(group) + 1);
				strcpy(grouphead.grname, group);
			}
			if (netf)
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
#ifdef YP
	_yp_innetgr = 0;
#endif

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
#ifdef YP
	_netgr_yp_enabled = 0;
#endif
}

#ifdef YP
static int _listmatch(list, group, len)
	char *list, *group;
	int len;
{
	char *ptr = list, *cptr;
	int glen = strlen(group);

	/* skip possible leading whitespace */
	while(isspace(*ptr))
		ptr++;

	while (ptr < list + len) {
		cptr = ptr;
		while(*ptr != ','  && !isspace(*ptr))
			ptr++;
		if (strncmp(cptr, group, glen) == 0 && glen == (ptr - cptr))
			return(1);
		while(*ptr == ','  || isspace(*ptr))
			ptr++;
	}

	return(0);
}

static int _buildkey(key, str, dom, rotation)
char *key, *str, *dom;
int *rotation;
{
	(*rotation)++;
	if (*rotation > 4)
		return(0);
	switch(*rotation) {
		case(1): sprintf((char *)key, "%s.%s", str, dom ? dom : "*");
			 break;
		case(2): sprintf((char *)key, "%s.*", str);
			 break;
		case(3): sprintf((char *)key, "*.%s", dom ? dom : "*");
			 break;
		case(4): sprintf((char *)key, "*.*");
			 break;
	}
	return(1);
}
#endif

/*
 * Search for a match in a netgroup.
 */
int
innetgr(group, host, user, dom)
	const char *group, *host, *user, *dom;
{
	char *hst, *usr, *dm;
#ifdef YP
	char *result;
	int resultlen;
	int rv;
#endif
	/* Sanity check */
	
	if (group == NULL || !strlen(group))
		return (0);

#ifdef YP
	_yp_innetgr = 1;
#endif
	setnetgrent(group);
#ifdef YP
	_yp_innetgr = 0;
	/*
	 * If we're in NIS-only mode, do the search using
	 * NIS 'reverse netgroup' lookups.
	 */
	if (_use_only_yp) {
		char _key[MAXHOSTNAMELEN];
		int rot = 0, y = 0;

		if(yp_get_default_domain(&_netgr_yp_domain))
			return(0);
		while(_buildkey(_key, user ? user : host, dom, &rot)) {
			y = yp_match(_netgr_yp_domain, user? "netgroup.byuser":
			    "netgroup.byhost", _key, strlen(_key), &result,
			    	&resultlen);
			if (y) {
				/*
				 * If we get an error other than 'no
				 * such key in map' then something is
				 * wrong and we should stop the search.
				 */
				if (y != YPERR_KEY)
					break;
			} else {
				rv = _listmatch(result, group, resultlen);
				free(result);
				if (rv)
					return(1);
				else
					return(0);
			}
		}
		/*
		 * Couldn't match using NIS-exclusive mode. If the error
	 	 * was YPERR_MAP, then the failure happened because there
	 	 * was no netgroup.byhost or netgroup.byuser map. The odds
		 * are we are talking to an Sun NIS+ server in YP emulation
		 * mode; if this is the case, then we have to do the check
		 * the 'old-fashioned' way by grovelling through the netgroup
		 * map and resolving memberships on the fly.
		 */
		if (y != YPERR_MAP)
			return(0);
	}

	setnetgrent(group);
#endif /* YP */

	while (getnetgrent(&hst, &usr, &dm))
		if ((host == NULL || hst == NULL || !strcmp(host, hst)) &&
		    (user == NULL || usr == NULL || !strcmp(user, usr)) &&
		    ( dom == NULL ||  dm == NULL || !strcmp(dom, dm))) {
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
#ifdef DEBUG
	register int fields;
#endif
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
#ifdef DEBUG
		/*
		 * This error message is largely superflous since the
		 * code handles the error condition sucessfully, and
		 * spewing it out from inside libc can actually hose
		 * certain programs.
		 */
		fprintf(stderr, "Cycle in netgroup %s\n", lp->l_groupname);
#endif
		return (1);
	} else
		lp->l_parsed = 1;
	pos = lp->l_line;
	/* Watch for null pointer dereferences, dammit! */
	while (pos != NULL && *pos != '\0') {
		if (*pos == '(') {
			grp = (struct netgrp *)malloc(sizeof (struct netgrp));
			bzero((char *)grp, sizeof (struct netgrp));
			grp->ng_next = grouphead.gr;
			grouphead.gr = grp;
			pos++;
			gpos = strsep(&pos, ")");
#ifdef DEBUG
			fields = 0;
#endif
			for (strpos = 0; strpos < 3; strpos++) {
				if ((spos = strsep(&gpos, ","))) {
#ifdef DEBUG
					fields++;
#endif
					while (*spos == ' ' || *spos == '\t')
						spos++;
					if ((epos = strpbrk(spos, " \t"))) {
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
				} else {
					/*
					 * All other systems I've tested
					 * return NULL for empty netgroup
					 * fields. It's up to user programs
					 * to handle the NULLs appropriately.
					 */
					grp->ng_str[strpos] = NULL;
				}
			}
#ifdef DEBUG
			/*
			 * Note: on other platforms, malformed netgroup
			 * entries are not normally flagged. While we
			 * can catch bad entries and report them, we should
			 * stay silent by default for compatibility's sake.
			 */
			if (fields < 3)
					fprintf(stderr, "Bad entry (%s%s%s%s%s) in netgroup \"%s\"\n",
						grp->ng_str[NG_HOST] == NULL ? "" : grp->ng_str[NG_HOST],
						grp->ng_str[NG_USER] == NULL ? "" : ",",
						grp->ng_str[NG_USER] == NULL ? "" : grp->ng_str[NG_USER],
						grp->ng_str[NG_DOM] == NULL ? "" : ",",
						grp->ng_str[NG_DOM] == NULL ? "" : grp->ng_str[NG_DOM],
						lp->l_groupname);
#endif
		} else {
			spos = strsep(&pos, ", \t");
			if (parse_netgrp(spos))
				continue;
		}
		if (pos == NULL)
			break;
		while (*pos == ' ' || *pos == ',' || *pos == '\t')
			pos++;
	}
	return (0);
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
	char line[LINSIZ + 2];
#ifdef YP
	char *result;
	int resultlen;

	while (_netgr_yp_enabled || fgets(line, LINSIZ, netf) != NULL) {
		if (_netgr_yp_enabled) {
			if(!_netgr_yp_domain)
				if(yp_get_default_domain(&_netgr_yp_domain))
					continue;
			if (yp_match(_netgr_yp_domain, "netgroup", group,
					strlen(group), &result, &resultlen)) {
				free(result);
				if (_use_only_yp)
					return ((struct linelist *)0);
				else {
					_netgr_yp_enabled = 0;
					continue;
				}
			}
			snprintf(line, LINSIZ, "%s %s", group, result);
			free(result);
		}
#else
	while (fgets(line, LINSIZ, netf) != NULL) {
#endif
		pos = (char *)&line;
#ifdef YP
		if (*pos == '+') {
			_netgr_yp_enabled = 1;
			continue;
		}
#endif
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
#ifdef YP
	/*
	 * Yucky. The recursive nature of this whole mess might require
	 * us to make more than one pass through the netgroup file.
	 * This might be best left outside the #ifdef YP, but YP is
	 * defined by default anyway, so I'll leave it like this
	 * until I know better.
	 */
	rewind(netf);
#endif
	return ((struct linelist *)0);
}
