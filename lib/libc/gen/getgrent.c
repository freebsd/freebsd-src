/*
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <grp.h>

static FILE *_gr_fp;
static struct group _gr_group;
static int _gr_stayopen;
static int grscan(), start_gr();
#ifdef YP
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
static int _gr_stepping_yp;
static int _gr_yp_enabled;
static int _getypgroup(struct group *, const char *, char *);
static int _nextypgroup(struct group *);
#endif

/* initial size for malloc and increase steps for realloc */
#define	MAXGRP		64
#define	MAXLINELENGTH	256

static char **members; 		/* list of group members */
static int maxgrp;              /* current length of **mebers */
static char *line;		/* temp buffer for group line */
static int maxlinelength;       /* current length of *line */

/* 
 * Lines longer than MAXLINELENGTHLIMIT will be count as an error.
 * <= 0 disable check for maximum line length
 * 256K is enough for 64,000 uids
 */
#define MAXLINELENGTHLIMIT	(256 * 1024)


struct group *
getgrent()
{
	if (!_gr_fp && !start_gr()) {
		return NULL;
	}

#ifdef YP
	if (_gr_stepping_yp) {
		if (_nextypgroup(&_gr_group))
			return(&_gr_group);
	}
tryagain:
#endif

	if (!grscan(0, 0, NULL))
		return(NULL);
#ifdef YP
	if(_gr_group.gr_name[0] == '+' && _gr_group.gr_name[1]) {
		_getypgroup(&_gr_group, &_gr_group.gr_name[1],
			    "group.byname");
	} else if(_gr_group.gr_name[0] == '+') {
		if (!_nextypgroup(&_gr_group))
			goto tryagain;
		else
			return(&_gr_group);
	}
#endif
	return(&_gr_group);
}

struct group *
getgrnam(name)
	const char *name;
{
	int rval;

	if (!start_gr())
		return(NULL);
#ifdef YP
	tryagain:
#endif
	rval = grscan(1, 0, name);
#ifdef YP
	if(rval == -1 && (_gr_yp_enabled < 0 || (_gr_yp_enabled &&
					_gr_group.gr_name[0] == '+'))) {
		if (!(rval = _getypgroup(&_gr_group, name, "group.byname")))
			goto tryagain;
	}
#endif
	if (!_gr_stayopen)
		endgrent();
	return(rval ? &_gr_group : NULL);
}

struct group *
#ifdef __STDC__
getgrgid(gid_t gid)
#else
getgrgid(gid)
	gid_t gid;
#endif
{
	int rval;

	if (!start_gr())
		return(NULL);
#ifdef YP
	tryagain:
#endif
	rval = grscan(1, gid, NULL);
#ifdef YP
	if(rval == -1 && _gr_yp_enabled) {
		char buf[16];
		snprintf(buf, sizeof buf, "%d", (unsigned)gid);
		if (!(rval = _getypgroup(&_gr_group, buf, "group.bygid")))
			goto tryagain;
	}
#endif
	if (!_gr_stayopen)
		endgrent();
	return(rval ? &_gr_group : NULL);
}

static int
start_gr()
{
	if (_gr_fp) {
		rewind(_gr_fp);
		return(1);
	}
	_gr_fp = fopen(_PATH_GROUP, "r");
	if(!_gr_fp) return 0;
#ifdef YP
	/*
	 * This is a disgusting hack, used to determine when YP is enabled.
	 * This would be easier if we had a group database to go along with
	 * the password database.
	 */
	{
		char *line;
		size_t linelen;
		_gr_yp_enabled = 0;
		while((line = fgetln(_gr_fp, &linelen)) != NULL) {
			if(line[0] == '+') {
				if(line[1] && line[1] != ':' && !_gr_yp_enabled) {
					_gr_yp_enabled = 1;
				} else {
					_gr_yp_enabled = -1;
					break;
				}
			}
		}
		rewind(_gr_fp);
	}
#endif

	if (maxlinelength == 0) {
		if ((line = (char *)malloc(sizeof(char) * 
					   MAXLINELENGTH)) == NULL)
			return(0);
		maxlinelength += MAXLINELENGTH;
	}

	if (maxgrp == 0) {
		if ((members = (char **)malloc(sizeof(char **) * 
					       MAXGRP)) == NULL)
			return(0);
		maxgrp += MAXGRP;
	}

	return 1;
}

int
setgrent()
{
	return(setgroupent(0));
}

int
setgroupent(stayopen)
	int stayopen;
{
	if (!start_gr())
		return(0);
	_gr_stayopen = stayopen;
#ifdef YP
	_gr_stepping_yp = 0;
#endif
	return(1);
}

void
endgrent()
{
#ifdef YP
	_gr_stepping_yp = 0;
#endif
	if (_gr_fp) {
		(void)fclose(_gr_fp);
		_gr_fp = NULL;
	}
}

static int
grscan(search, gid, name)
	register int search, gid;
	register char *name;
{
	register char *cp, **m;
	char *bp;


#ifdef YP
	int _ypfound;
#endif
	for (;;) {
#ifdef YP
		_ypfound = 0;
#endif
		if (fgets(line, maxlinelength, _gr_fp) == NULL)
			return(0);

		if (!index(line, '\n')) {
			do {
				if (feof(_gr_fp))
					return(0);
			
				/* don't allocate infinite memory */
				if (MAXLINELENGTHLIMIT > 0 && 
				    maxlinelength >= MAXLINELENGTHLIMIT)
					return(0);

				if ((line = (char *)realloc(line, 
				     sizeof(char) * 
				     (maxlinelength + MAXLINELENGTH))) == NULL)
					return(0);
			
				if (fgets(line + maxlinelength - 1, 
					  MAXLINELENGTH + 1, _gr_fp) == NULL)
					return(0);

				maxlinelength += MAXLINELENGTH;
			} while (!index(line + maxlinelength - 
				       MAXLINELENGTH - 1, '\n'));
		}

#ifdef GROUP_IGNORE_COMMENTS
		/* 
		 * Ignore comments: ^[ \t]*#
		 */
		for (cp = line; *cp != '\0'; cp++)
			if (*cp != ' ' && *cp != '\t')
				break;
		if (*cp == '#')
			continue;
#endif

		bp = line;

		if ((_gr_group.gr_name = strsep(&bp, ":\n")) == NULL)
			break;
#ifdef YP
		/*
		 * XXX   We need to be careful to avoid proceeding
		 * past this point under certain circumstances or
		 * we risk dereferencing null pointers down below.
		 */
		if (_gr_group.gr_name[0] == '+') {
			if (strlen(_gr_group.gr_name) == 1) {
				switch(search) {
				case 0:
					return(1);
				case 1:
					return(-1);
				default:
					return(0);
				}
			} else {
				cp = &_gr_group.gr_name[1];
				if (search && name != NULL)
					if (strcmp(cp, name))
						continue;
				if (!_getypgroup(&_gr_group, cp,
						"group.byname"))
					continue;
				if (search && name == NULL)
					if (gid != _gr_group.gr_gid)
						continue;
			/* We're going to override -- tell the world. */
				_ypfound++;
			}
		}
#else
		if (_gr_group.gr_name[0] == '+')
			continue;
#endif /* YP */
		if (search && name) {
			if(strcmp(_gr_group.gr_name, name)) {
				continue;
			}
		}
#ifdef YP
		if ((cp = strsep(&bp, ":\n")) == NULL)
			if (_ypfound)
				return(1);
			else
				break;
		if (strlen(cp) || !_ypfound)
			_gr_group.gr_passwd = cp;
#else
		if ((_gr_group.gr_passwd = strsep(&bp, ":\n")) == NULL)
			break;
#endif
		if (!(cp = strsep(&bp, ":\n")))
#ifdef YP
			if (_ypfound)
				return(1);
			else
#endif
				continue;
#ifdef YP
		/*
		 * Hurm. Should we be doing this? We allow UIDs to
		 * be overridden -- what about GIDs?
		 */
		if (!_ypfound)
#endif
		_gr_group.gr_gid = atoi(cp);
		if (search && name == NULL && _gr_group.gr_gid != gid)
			continue;
		cp = NULL;
		if (bp == NULL) /* !!! Must check for this! */
			break;
#ifdef YP
		if ((cp = strsep(&bp, ":\n")) == NULL)
			break;

		if (!strlen(cp) && _ypfound)
			return(1);
		else
			members[0] = NULL;
		bp = cp;
		cp = NULL;
#endif
		for (m = members; ; bp++) {
			if (m == (members + maxgrp - 1)) {
				if ((members = (char **)
				     realloc(members, 
					     sizeof(char **) * 
					     (maxgrp + MAXGRP))) == NULL)
					return(0);
				m = members + maxgrp - 1;
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
		_gr_group.gr_mem = members;
		*m = NULL;
		return(1);
	}
	/* NOTREACHED */
	return (0);
}

#ifdef YP

static int
_gr_breakout_yp(struct group *gr, char *result)
{
	char *s, *cp;
	char **m;

	/*
	 * XXX If 's' ends up being a NULL pointer, punt on this group.
	 * It means the NIS group entry is badly formatted and should
	 * be skipped.
	 */
	if ((s = strsep(&result, ":")) == NULL) return 0; /* name */
	gr->gr_name = s;

	if ((s = strsep(&result, ":")) == NULL) return 0; /* password */
	gr->gr_passwd = s;

	if ((s = strsep(&result, ":")) == NULL) return 0; /* gid */
	gr->gr_gid = atoi(s);

	if ((s = result) == NULL) return 0;
	cp = 0;

	for (m = members; ; s++) {
		if (m == members + maxgrp - 1) {
			if ((members = (char **)realloc(members, 
			     sizeof(char **) * (maxgrp + MAXGRP))) == NULL)
				return(0);
			m = members + maxgrp - 1;
			maxgrp += MAXGRP;
		}
		if (*s == ',') {
			if (cp) {
				*s = '\0';
				*m++ = cp;
				cp = NULL;
			}
		} else if (*s == '\0' || *s == '\n' || *s == ' ') {
			if (cp) {
				*s = '\0';
				*m++ = cp;
			}
			break;
		} else if (cp == NULL) {
			cp = s;
		}
	}
	_gr_group.gr_mem = members;
	*m = NULL;

	return 1;
}

static char *_gr_yp_domain;

static int
_getypgroup(struct group *gr, const char *name, char *map)
{
	char *result, *s;
	static char resultbuf[YPMAXRECORD + 2];
	int resultlen;

	if(!_gr_yp_domain) {
		if(yp_get_default_domain(&_gr_yp_domain))
		  return 0;
	}

	if(yp_match(_gr_yp_domain, map, name, strlen(name),
		    &result, &resultlen))
		return 0;

	s = strchr(result, '\n');
	if(s) *s = '\0';

	if(resultlen >= sizeof resultbuf) return 0;
	strncpy(resultbuf, result, resultlen);
	resultbuf[resultlen] = '\0';
	free(result);
	return(_gr_breakout_yp(gr, resultbuf));

}


static int
_nextypgroup(struct group *gr)
{
	static char *key;
	static int keylen;
	char *lastkey, *result;
	static char resultbuf[YPMAXRECORD + 2];
	int resultlen;
	int rv;

	if(!_gr_yp_domain) {
		if(yp_get_default_domain(&_gr_yp_domain))
		  return 0;
	}

	if(!_gr_stepping_yp) {
		if(key) free(key);
		rv = yp_first(_gr_yp_domain, "group.byname",
			      &key, &keylen, &result, &resultlen);
		if(rv) {
			return 0;
		}
		_gr_stepping_yp = 1;
		goto unpack;
	} else {
tryagain:
		lastkey = key;
		rv = yp_next(_gr_yp_domain, "group.byname", key, keylen,
			     &key, &keylen, &result, &resultlen);
		free(lastkey);
unpack:
		if(rv) {
			_gr_stepping_yp = 0;
			return 0;
		}

		if(resultlen > sizeof(resultbuf)) {
			free(result);
			goto tryagain;
		}

		strncpy(resultbuf, result, resultlen);
		resultbuf[resultlen] = '\0';
		free(result);
		if((result = strchr(resultbuf, '\n')) != NULL)
			*result = '\0';
		if (_gr_breakout_yp(gr, resultbuf))
			return(1);
		else
			goto tryagain;
	}
}

#endif /* YP */
