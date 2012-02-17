/*
 * Copyright (c) 2004 by Internet Systems Consortium, Inc. ("ISC")
 * Copyright (c) 1996-1999 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#if !defined(LINT) && !defined(CODECENTER)
static const char rcsid[] = "$Id: lcl_ng.c,v 1.2.18.1 2005-04-27 05:01:02 sra Exp $";
#endif

/* Imports */

#include "port_before.h"

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <irs.h>
#include <isc/memcluster.h>

#include "port_after.h"

#include "irs_p.h"
#include "lcl_p.h"

/* Definitions */

#define NG_HOST         0       /*%< Host name */
#define NG_USER         1       /*%< User name */
#define NG_DOM          2       /*%< and Domain name */
#define LINSIZ		1024    /*%< Length of netgroup file line */
/*
 * XXX Warning XXX
 * This code is a hack-and-slash special.  It realy needs to be
 * rewritten with things like strdup, and realloc in mind.
 * More reasonable data structures would not be a bad thing.
 */

/*%
 * Static Variables and functions used by setnetgrent(), getnetgrent() and
 * endnetgrent().
 *
 * There are two linked lists:
 * \li linelist is just used by setnetgrent() to parse the net group file via.
 *   parse_netgrp()
 * \li netgrp is the list of entries for the current netgroup
 */
struct linelist {
	struct linelist *l_next;	/*%< Chain ptr. */
	int		l_parsed;	/*%< Flag for cycles */
	char *		l_groupname;	/*%< Name of netgroup */
	char *		l_line;		/*%< Netgroup entrie(s) to be parsed */
};

struct ng_old_struct {
	struct ng_old_struct *ng_next;	/*%< Chain ptr */
	char *		ng_str[3];	/*%< Field pointers, see below */
};

struct pvt {
	FILE			*fp;
	struct linelist		*linehead;
	struct ng_old_struct    *nextgrp;
	struct {
		struct ng_old_struct	*gr;
		char			*grname;
	} grouphead;
};

/* Forward */

static void 		ng_rewind(struct irs_ng *, const char*);
static void 		ng_close(struct irs_ng *);
static int		ng_next(struct irs_ng *, const char **,
				const char **, const char **);
static int 		ng_test(struct irs_ng *, const char *,
				const char *, const char *,
				const char *);
static void		ng_minimize(struct irs_ng *);

static int 		parse_netgrp(struct irs_ng *, const char*);
static struct linelist *read_for_group(struct irs_ng *, const char *);
static void		freelists(struct irs_ng *);

/* Public */

struct irs_ng *
irs_lcl_ng(struct irs_acc *this) {
	struct irs_ng *ng;
	struct pvt *pvt;

	UNUSED(this);
	
	if (!(ng = memget(sizeof *ng))) {
		errno = ENOMEM;
		return (NULL);
	}
	memset(ng, 0x5e, sizeof *ng);
	if (!(pvt = memget(sizeof *pvt))) {
		memput(ng, sizeof *ng);
		errno = ENOMEM;
		return (NULL);
	}
	memset(pvt, 0, sizeof *pvt);
	ng->private = pvt;
	ng->close = ng_close;
	ng->next = ng_next;
	ng->test = ng_test;
	ng->rewind = ng_rewind;
	ng->minimize = ng_minimize;
	return (ng);
}

/* Methods */

static void
ng_close(struct irs_ng *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	
	if (pvt->fp != NULL)
		fclose(pvt->fp);
	freelists(this);
	memput(pvt, sizeof *pvt);
	memput(this, sizeof *this);
}
	
/*%
 * Parse the netgroup file looking for the netgroup and build the list
 * of netgrp structures. Let parse_netgrp() and read_for_group() do
 * most of the work.
 */
static void
ng_rewind(struct irs_ng *this, const char *group) {
	struct pvt *pvt = (struct pvt *)this->private;
	
	if (pvt->fp != NULL && fseek(pvt->fp, SEEK_CUR, 0L) == -1) {
		fclose(pvt->fp);
		pvt->fp = NULL;
	}

	if (pvt->fp == NULL || pvt->grouphead.gr == NULL || 
	    strcmp(group, pvt->grouphead.grname)) {
		freelists(this);
		if (pvt->fp != NULL)
			fclose(pvt->fp);
		pvt->fp = fopen(_PATH_NETGROUP, "r");
		if (pvt->fp != NULL) {
			if (parse_netgrp(this, group))
				freelists(this);
			if (!(pvt->grouphead.grname = strdup(group)))
				freelists(this);
			fclose(pvt->fp);
			pvt->fp = NULL;
		}
	}
	pvt->nextgrp = pvt->grouphead.gr;
}

/*%
 * Get the next netgroup off the list.
 */
static int
ng_next(struct irs_ng *this, const char **host, const char **user,
	const char **domain)
{
	struct pvt *pvt = (struct pvt *)this->private;
	
	if (pvt->nextgrp) {
		*host = pvt->nextgrp->ng_str[NG_HOST];
		*user = pvt->nextgrp->ng_str[NG_USER];
		*domain = pvt->nextgrp->ng_str[NG_DOM];
		pvt->nextgrp = pvt->nextgrp->ng_next;
		return (1);
	}
	return (0);
}

/*%
 * Search for a match in a netgroup.
 */
static int
ng_test(struct irs_ng *this, const char *name,
	const char *host, const char *user, const char *domain)
{
	const char *ng_host, *ng_user, *ng_domain;

	ng_rewind(this, name);
	while (ng_next(this, &ng_host, &ng_user, &ng_domain))
		if ((host == NULL || ng_host == NULL || 
		     !strcmp(host, ng_host)) &&
		    (user ==  NULL || ng_user == NULL || 
		     !strcmp(user, ng_user)) &&
		    (domain == NULL || ng_domain == NULL ||
		     !strcmp(domain, ng_domain))) {
			freelists(this);
			return (1);
		}
	freelists(this);
	return (0);
}

static void
ng_minimize(struct irs_ng *this) {
	struct pvt *pvt = (struct pvt *)this->private;

	if (pvt->fp != NULL) {
		(void)fclose(pvt->fp);
		pvt->fp = NULL;
	}
}

/* Private */

/*%
 * endnetgrent() - cleanup
 */
static void
freelists(struct irs_ng *this) {
	struct pvt *pvt = (struct pvt *)this->private;
	struct linelist *lp, *olp;
	struct ng_old_struct *gp, *ogp;

	lp = pvt->linehead;
	while (lp) {
		olp = lp;
		lp = lp->l_next;
		free(olp->l_groupname);
		free(olp->l_line);
		free((char *)olp);
	}
	pvt->linehead = NULL;
	if (pvt->grouphead.grname) {
		free(pvt->grouphead.grname);
		pvt->grouphead.grname = NULL;
	}
	gp = pvt->grouphead.gr;
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
	pvt->grouphead.gr = NULL;
}

/*%
 * Parse the netgroup file setting up the linked lists.
 */
static int
parse_netgrp(struct irs_ng *this, const char *group) {
	struct pvt *pvt = (struct pvt *)this->private;
	char *spos, *epos;
	int len, strpos;
	char *pos, *gpos;
	struct ng_old_struct *grp;
	struct linelist *lp = pvt->linehead;

        /*
         * First, see if the line has already been read in.
         */
	while (lp) {
		if (!strcmp(group, lp->l_groupname))
			break;
		lp = lp->l_next;
	}
	if (lp == NULL &&
	    (lp = read_for_group(this, group)) == NULL)
		return (1);
	if (lp->l_parsed) {
		/*fprintf(stderr, "Cycle in netgroup %s\n", lp->l_groupname);*/
		return (1);
	} else
		lp->l_parsed = 1;
	pos = lp->l_line;
	while (*pos != '\0') {
		if (*pos == '(') {
			if (!(grp = malloc(sizeof (struct ng_old_struct)))) {
				freelists(this);
				errno = ENOMEM;
				return (1);
			}
			memset(grp, 0, sizeof (struct ng_old_struct));
			grp->ng_next = pvt->grouphead.gr;
			pvt->grouphead.gr = grp;
			pos++;
			gpos = strsep(&pos, ")");
			for (strpos = 0; strpos < 3; strpos++) {
				if ((spos = strsep(&gpos, ","))) {
					while (*spos == ' ' || *spos == '\t')
						spos++;
					if ((epos = strpbrk(spos, " \t"))) {
						*epos = '\0';
						len = epos - spos;
					} else
						len = strlen(spos);
					if (len > 0) {
						if(!(grp->ng_str[strpos] 
						   =  (char *)
						   malloc(len + 1))) {
							freelists(this);
							return (1);
						}
						memcpy(grp->ng_str[strpos],
						       spos,
						       len + 1);
					}
				} else
					goto errout;
			}
		} else {
			spos = strsep(&pos, ", \t");
			if (spos != NULL && parse_netgrp(this, spos)) {
				freelists(this);
				return (1);
			}
		}
		if (pos == NULL)
			break;
		while (*pos == ' ' || *pos == ',' || *pos == '\t')
			pos++;
	}
	return (0);
 errout:
	/*fprintf(stderr, "Bad netgroup %s at ..%s\n", lp->l_groupname,
		  spos);*/
	return (1);
}

/*%
 * Read the netgroup file and save lines until the line for the netgroup
 * is found. Return 1 if eof is encountered.
 */
static struct linelist *
read_for_group(struct irs_ng *this, const char *group) {
	struct pvt *pvt = (struct pvt *)this->private;
	char *pos, *spos, *linep = NULL, *olinep;
	int len, olen, cont;
	struct linelist *lp;
	char line[LINSIZ + 1];
	
	while (fgets(line, LINSIZ, pvt->fp) != NULL) {
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
			if (!(lp = malloc(sizeof (*lp)))) {
				freelists(this);
				return (NULL);
			}
			lp->l_parsed = 0;
			if (!(lp->l_groupname = malloc(len + 1))) {
				free(lp);
				freelists(this);
				return (NULL);
			}
			memcpy(lp->l_groupname, spos,  len);
			*(lp->l_groupname + len) = '\0';
			len = strlen(pos);
			olen = 0;
			olinep = NULL;

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
					if (!(linep = malloc(olen + len + 1))){
						if (olen > 0)
							free(olinep);
						free(lp->l_groupname);
						free(lp);
						freelists(this);
						errno = ENOMEM;
						return (NULL);
					}
					if (olen > 0) {
						memcpy(linep, olinep, olen);
						free(olinep);
					}
					memcpy(linep + olen, pos, len);
					olen += len;
					*(linep + olen) = '\0';
					olinep = linep;
				}
				if (cont) {
					if (fgets(line, LINSIZ, pvt->fp)) {
						pos = line;
						len = strlen(pos);
					} else
						cont = 0;
				}
			} while (cont);
			lp->l_line = linep;
			lp->l_next = pvt->linehead;
			pvt->linehead = lp;
			
			/*
			 * If this is the one we wanted, we are done.
			 */
			if (!strcmp(lp->l_groupname, group))
				return (lp);
		}
	}
	return (NULL);
}

/*! \file */
