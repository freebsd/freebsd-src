/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Portions Copyright (c) 1994, Jason Downs. All Rights Reserved.
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
static char sccsid[] = "@(#)getgrent.c	8.2 (Berkeley) 3/21/94";
#endif /* LIBC_SCCS and not lint */
/*	$NetBSD: getgrent.c,v 1.34.2.1 1999/04/27 14:10:58 perry Exp $	*/
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>

#include <errno.h>
#include <grp.h>
#include <limits.h>
#include <nsswitch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#ifdef HESIOD
#include <hesiod.h>
#include <arpa/nameser.h>
#endif
#ifdef YP
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#endif

#if defined(YP) || defined(HESIOD)
#define _GROUP_COMPAT
#endif

static FILE		*_gr_fp;
static struct group	_gr_group;
static int		_gr_stayopen;
static int		_gr_filesdone;

static void grcleanup(void);
static int grscan(int, gid_t, const char *);
static char *getline(void);
static int copyline(const char*);
static int matchline(int, gid_t, const char *);
static int start_gr(void);




/* initial size for malloc and increase steps for realloc */
#define	MAXGRP		64
#define	MAXLINELENGTH	256

#ifdef HESIOD
#if MAXLINELENGTH < NS_MAXLABEL + 1
#error "MAXLINELENGTH must be at least NS_MAXLABEL + 1"
#endif
#endif

static char		**members;       /* list of group members */
static int		  maxgrp;        /* current length of **members */
static char		 *line;	         /* buffer for group line */
static int		  maxlinelength; /* current length of *line */

/*
 * Lines longer than MAXLINELENGTHLIMIT will be counted as an error.
 * <= 0 disable check for maximum line length
 * 256K is enough for 64,000 uids
 */
#define MAXLINELENGTHLIMIT	(256 * 1024)

#ifdef YP
static char	*__ypcurrent, *__ypdomain;
static int	 __ypcurrentlen;
static int	 _gr_ypdone;
#endif

#ifdef HESIOD
static int	_gr_hesnum;
#endif

#ifdef _GROUP_COMPAT
enum _grmode { GRMODE_NONE, GRMODE_FULL, GRMODE_NAME };
static enum _grmode	 __grmode;
#endif

struct group *
getgrent()
{
	if ((!_gr_fp && !start_gr()) || !grscan(0, 0, NULL))
 		return (NULL);
	return &_gr_group;
}

struct group *
getgrnam(name)
	const char *name;
{
	int rval;

	if (!start_gr())
		return NULL;
	rval = grscan(1, 0, name);
	if (!_gr_stayopen)
		endgrent();
	return (rval) ? &_gr_group : NULL;
}

struct group *
getgrgid(gid)
	gid_t gid;
{
	int rval;

	if (!start_gr())
		return NULL;
	rval = grscan(1, gid, NULL);
	if (!_gr_stayopen)
		endgrent();
	return (rval) ? &_gr_group : NULL;
}

void
grcleanup()
{
	_gr_filesdone = 0;
#ifdef YP
	if (__ypcurrent)
		free(__ypcurrent);
	__ypcurrent = NULL;
	_gr_ypdone = 0;
#endif
#ifdef HESIOD
	_gr_hesnum = 0;
#endif
#ifdef _GROUP_COMPAT
	__grmode = GRMODE_NONE;
#endif
}

static int
start_gr()
{
	grcleanup();
	if (maxlinelength == 0) {
		if ((line = (char *)malloc(MAXLINELENGTH)) == NULL)
			return 0;
		maxlinelength = MAXLINELENGTH;
	}
	if (maxgrp == 0) {
		if ((members = (char **) malloc(sizeof(char**) * 
					       MAXGRP)) == NULL)
			return 0;
		maxgrp = MAXGRP;
	}
	if (_gr_fp) {
		rewind(_gr_fp);
		return 1;
	}
	return (_gr_fp = fopen(_PATH_GROUP, "r")) ? 1 : 0;
}

int
setgrent(void)
{
	return setgroupent(0);
}

int
setgroupent(stayopen)
	int stayopen;
{
	if (!start_gr())
		return 0;
	_gr_stayopen = stayopen;
	return 1;
}

void
endgrent()
{
	grcleanup();
	if (_gr_fp) {
		(void)fclose(_gr_fp);
		_gr_fp = NULL;
	}
}


static int _local_grscan(void *, void *, va_list);

/*ARGSUSED*/
static int
_local_grscan(rv, cb_data, ap)
	void	*rv;
	void	*cb_data;
	va_list	 ap;
{
	int		 search = va_arg(ap, int);
	gid_t		 gid = va_arg(ap, gid_t);
	const char	*name = va_arg(ap, const char *);

	if (_gr_filesdone)
		return NS_NOTFOUND;
	for (;;) {
		if (getline() == NULL) {
			if (!search)
				_gr_filesdone = 1;
			return NS_NOTFOUND;
		}
		if (matchline(search, gid, name))
			return NS_SUCCESS;
	}
	/* NOTREACHED */
}

#ifdef HESIOD
static int _dns_grscan(void *, void *, va_list);

/*ARGSUSED*/
static int
_dns_grscan(rv, cb_data, ap)
	void	*rv;
	void	*cb_data;
	va_list	 ap;
{
	int		 search = va_arg(ap, int);
	gid_t		 gid = va_arg(ap, gid_t);
	const char	*name = va_arg(ap, const char *);

	char		**hp;
	void		 *context;
	int		  r;

	r = NS_UNAVAIL;
	if (!search && _gr_hesnum == -1)
		return NS_NOTFOUND;
	if (hesiod_init(&context) == -1)
		return (r);

	for (;;) {
		if (search) {
			if (name)
				strlcpy(line, name, maxlinelength);
			else
				snprintf(line, maxlinelength, "%u",
				    (unsigned int)gid);
		} else {
			snprintf(line, maxlinelength, "group-%u", _gr_hesnum);
			_gr_hesnum++;
		}

		hp = hesiod_resolve(context, line, "group");
		if (hp == NULL) {
			if (errno == ENOENT) {
				if (!search)
					_gr_hesnum = -1;
				r = NS_NOTFOUND;
			}
			break;
		}

						/* only check first elem */
		if (copyline(hp[0]) == 0)
			return NS_UNAVAIL;
		hesiod_free_list(context, hp);
		if (matchline(search, gid, name)) {
			r = NS_SUCCESS;
			break;
		} else if (search) {
			r = NS_NOTFOUND;
			break;
		}
	}
	hesiod_end(context);
	return (r);
}
#endif

#ifdef YP
static int _nis_grscan(void *, void *, va_list);

/*ARGSUSED*/
static int
_nis_grscan(rv, cb_data, ap)
	void	*rv;
	void	*cb_data;
	va_list	 ap;
{
	int		 search = va_arg(ap, int);
	gid_t		 gid = va_arg(ap, gid_t);
	const char	*name = va_arg(ap, const char *);

	char	*key, *data;
	int	 keylen, datalen;
	int	 r;

	if(__ypdomain == NULL) {
		switch (yp_get_default_domain(&__ypdomain)) {
		case 0:
			break;
		case YPERR_RESRC:
			return NS_TRYAGAIN;
		default:
			return NS_UNAVAIL;
		}
	}

	if (search) {			/* specific group or gid */
		if (name)
			strlcpy(line, name, maxlinelength);
		else
			snprintf(line, maxlinelength, "%u", (unsigned int)gid);
		data = NULL;
		r = yp_match(__ypdomain,
				(name) ? "group.byname" : "group.bygid",
				line, (int)strlen(line), &data, &datalen);
		switch (r) {
		case 0:
			break;
		case YPERR_KEY:
			if (data)
				free(data);
			return NS_NOTFOUND;
		default:
			if (data)
				free(data);
			return NS_UNAVAIL;
		}
		data[datalen] = '\0';			/* clear trailing \n */
		if (copyline(data) == 0)
			return NS_UNAVAIL;
		free(data);
		if (matchline(search, gid, name))
			return NS_SUCCESS;
		else
			return NS_NOTFOUND;
	}

						/* ! search */
	if (_gr_ypdone)		
		return NS_NOTFOUND;
	for (;;) {
		data = NULL;
		if(__ypcurrent) {
			key = NULL;
			r = yp_next(__ypdomain, "group.byname",
				__ypcurrent, __ypcurrentlen,
				&key, &keylen, &data, &datalen);
			free(__ypcurrent);
			switch (r) {
			case 0:
				break;
			case YPERR_NOMORE:
				__ypcurrent = NULL;
				if (key)
					free(key);
				if (data)
					free(data);
				_gr_ypdone = 1;
				return NS_NOTFOUND;
			default:
				if (key)
					free(key);
				if (data)
					free(data);
				return NS_UNAVAIL;
			}
			__ypcurrent = key;
			__ypcurrentlen = keylen;
		} else {
			if (yp_first(__ypdomain, "group.byname",
					&__ypcurrent, &__ypcurrentlen,
					&data, &datalen)) {
				if (data)
					free(data);
				return NS_UNAVAIL;
			}
		}
		data[datalen] = '\0';			/* clear trailing \n */
		if (copyline(data) == 0)
			return NS_UNAVAIL;
		free(data);
		if (matchline(search, gid, name))
			return NS_SUCCESS;
	}
	/* NOTREACHED */
}
#endif

#ifdef _GROUP_COMPAT
/*
 * log an error if "files" or "compat" is specified in group_compat database
 */
static int _bad_grscan(void *, void *, va_list);

/*ARGSUSED*/
static int
_bad_grscan(rv, cb_data, ap)
	void	*rv;
	void	*cb_data;
	va_list	 ap;
{
	static int warned;

	if (!warned) {
		syslog(LOG_ERR,
			"nsswitch.conf group_compat database can't use '%s'",
			(char *)cb_data);
	}
	warned = 1;
	return NS_UNAVAIL;
}

/*
 * when a name lookup in compat mode is required, look it up in group_compat
 * nsswitch database. only Hesiod and NIS is supported - it doesn't make
 * sense to lookup compat names from 'files' or 'compat'
 */

static int __grscancompat(int, gid_t, const char *);

static int
__grscancompat(search, gid, name)
	int		 search;
	gid_t		 gid;
	const char	*name;
{
	static const ns_dtab dtab[] = {
		NS_FILES_CB(_bad_grscan, "files")
		NS_DNS_CB(_dns_grscan, NULL)
		NS_NIS_CB(_nis_grscan, NULL)
		NS_COMPAT_CB(_bad_grscan, "compat")
		{ 0 }
	};
	static const ns_src defaultnis[] = {
		{ NSSRC_NIS, 	NS_SUCCESS },
		{ 0 }
	};

	return (nsdispatch(NULL, dtab, NSDB_GROUP_COMPAT, "grscancompat",
	    defaultnis, search, gid, name));
}
#endif


static int _compat_grscan(void *, void *, va_list);

/*ARGSUSED*/
static int
_compat_grscan(rv, cb_data, ap)
	void	*rv;
	void	*cb_data;
	va_list	 ap;
{
	int		 search = va_arg(ap, int);
	gid_t		 gid = va_arg(ap, gid_t);
	const char	*name = va_arg(ap, const char *);

#ifdef _GROUP_COMPAT
	static char	*grname = NULL;
#endif

	for (;;) {
#ifdef _GROUP_COMPAT
		if(__grmode != GRMODE_NONE) {
			int	 r;

			switch(__grmode) {
			case GRMODE_FULL:
				r = __grscancompat(search, gid, name);
				if (r == NS_SUCCESS)
					return r;
				__grmode = GRMODE_NONE;
				break;
			case GRMODE_NAME:
				if(grname == (char *)NULL) {
					__grmode = GRMODE_NONE;
					break;
				}
				r = __grscancompat(1, 0, grname);
				free(grname);
				grname = (char *)NULL;
				if (r != NS_SUCCESS)
					break;
				if (!search)
					return NS_SUCCESS;
				if (name) {
					if (! strcmp(_gr_group.gr_name, name))
						return NS_SUCCESS;
				} else {
					if (_gr_group.gr_gid == gid)
						return NS_SUCCESS;
				}
				break;
			case GRMODE_NONE:
				abort();
			}
			continue;
		}
#endif /* _GROUP_COMPAT */

		if (getline() == NULL)
			return NS_NOTFOUND;

#ifdef _GROUP_COMPAT
		if (line[0] == '+') {
			char	*tptr, *bp;

			switch(line[1]) {
			case ':':
			case '\0':
			case '\n':
				__grmode = GRMODE_FULL;
				break;
			default:
				__grmode = GRMODE_NAME;
				bp = line;
				tptr = strsep(&bp, ":\n");
				grname = strdup(tptr + 1);
				break;
			}
			continue;
		}
#endif /* _GROUP_COMPAT */
		if (matchline(search, gid, name))
			return NS_SUCCESS;
	}
	/* NOTREACHED */
}

static int
grscan(search, gid, name)
	int		 search;
	gid_t		 gid;
	const char	*name;
{
	int		r;
	static const ns_dtab dtab[] = {
		NS_FILES_CB(_local_grscan, NULL)
		NS_DNS_CB(_dns_grscan, NULL)
		NS_NIS_CB(_nis_grscan, NULL)
		NS_COMPAT_CB(_compat_grscan, NULL)
		{ 0 }
	};
	static const ns_src compatsrc[] = {
		{ NSSRC_COMPAT, NS_SUCCESS },
		{ 0 }
	};

	r = nsdispatch(NULL, dtab, NSDB_GROUP, "grscan", compatsrc,
	    search, gid, name);
	return (r == NS_SUCCESS) ? 1 : 0;
}

static int
matchline(search, gid, name)
	int		 search;
	gid_t		 gid;
	const char	*name;
{
	unsigned long	id;
	char		**m;
	char		*cp, *bp, *ep;

	if (line[0] == '+')
		return 0;	/* sanity check to prevent recursion */
	bp = line;
	_gr_group.gr_name = strsep(&bp, ":\n");
	if (search && name && strcmp(_gr_group.gr_name, name))
		return 0;
	_gr_group.gr_passwd = strsep(&bp, ":\n");
	if (!(cp = strsep(&bp, ":\n")))
		return 0;
	id = strtoul(cp, &ep, 10);
	if (*ep != '\0')
		return 0;
	_gr_group.gr_gid = (gid_t)id;
	if (search && name == NULL && _gr_group.gr_gid != gid)
		return 0;
	cp = NULL;
	if (bp == NULL)
		return 0;
	for (_gr_group.gr_mem = m = members;; bp++) {
		if (m == &members[maxgrp - 1]) {
			members = (char **) reallocf(members, sizeof(char **) *
						     (maxgrp + MAXGRP));
			if (members == NULL)
				return 0;
			_gr_group.gr_mem = members;
			m = &members[maxgrp - 1];
			maxgrp += MAXGRP;
		}
		if (*bp == ',') {
			if (cp) {
				*bp = '\0';
				*m++ = cp;
				cp = NULL;
			}
		} else if (*bp == '\0' || *bp == '\n' || *bp == ' ') {
			if (cp) {
				*bp = '\0';
				*m++ = cp;
			}
			break;
		} else if (cp == NULL)
			cp = bp;
        }
	*m = NULL;
        return 1;
}

static char *
getline(void)
{
	const char	*cp;

 tryagain:
	if (fgets(line, maxlinelength, _gr_fp) == NULL)
		return NULL;
	if (index(line, '\n') == NULL) {
		do {
			if (feof(_gr_fp))
				return NULL;
			if (MAXLINELENGTHLIMIT > 0 && 
			    maxlinelength >= MAXLINELENGTHLIMIT)
				return NULL;
			line = (char *)reallocf(line, maxlinelength +
						MAXLINELENGTH);
			if (line == NULL)
				return NULL;
			if (fgets(line + maxlinelength - 1,
				  MAXLINELENGTH + 1, _gr_fp) == NULL)
				return NULL;
			maxlinelength += MAXLINELENGTH;
		} while (index(line + maxlinelength - MAXLINELENGTH - 1, 
			       '\n') == NULL);
	}


	/* 
	 * Ignore comments: ^[ \t]*#
	 */
	for (cp = line; *cp != '\0'; cp++)
		if (*cp != ' ' && *cp != '\t')
			break;
	if (*cp == '#' || *cp == '\0') 
		goto tryagain;

	if (cp != line) /* skip white space at beginning of line */
		bcopy(cp, line, strlen(cp));
	
	return line;
}

static int
copyline(const char *src)
{
	size_t	sz;

	sz = strlen(src);
	if (sz > maxlinelength - 1) {
		sz = ((sz/MAXLINELENGTH)+1) * MAXLINELENGTH;
		if ((line = (char *) reallocf(line, sz)) == NULL)
			return 0;
		maxlinelength = sz;
	}
	strlcpy(line, src, maxlinelength);
	return 1;
}

