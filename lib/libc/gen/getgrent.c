/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
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
/* static char *sccsid = "from: @(#)getgrent.c	5.9 (Berkeley) 4/1/91"; */
static char *rcsid = "$Id: getgrent.c,v 1.3 1994/01/11 19:00:57 nate Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <grp.h>
#ifdef YP
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#endif

static FILE *_gr_fp;
static struct group _gr_group;
static int _gr_stayopen;
static int grscan(), start_gr();

#define	MAXGRP		200
static char *members[MAXGRP];
#define	MAXLINELENGTH	1024
static char line[MAXLINELENGTH];

#ifdef YP
static char	*__ypcurrent, *__ypdomain;
static int	__ypcurrentlen, __ypmode=0;
#endif

struct group *
getgrent()
{
	if (!_gr_fp && !start_gr() || !grscan(0, 0, NULL))
		return(NULL);
	return(&_gr_group);
}

struct group *
getgrnam(name)
	const char *name;
{
	int rval;

	if (!start_gr())
		return(NULL);
	rval = grscan(1, 0, name);
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
	rval = grscan(1, gid, NULL);
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
	return((_gr_fp = fopen(_PATH_GROUP, "r")) ? 1 : 0);
}

void
setgrent()
{
	(void) setgroupent(0);
}

int
setgroupent(stayopen)
	int stayopen;
{
	if (!start_gr())
		return(0);
	_gr_stayopen = stayopen;
	return(1);
}

void
endgrent()
{
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

	for (;;) {
#ifdef YP
		if(__ypmode) {
			char *key, *data;
			int keylen, datalen;
			int r;

			if(!__ypdomain) {
				if(yp_get_default_domain(&__ypdomain)) {
					__ypmode = 0;
					continue;
				}
			}
			if(__ypcurrent) {
				r = yp_next(__ypdomain, "group.byname",
					__ypcurrent, __ypcurrentlen,
					&key, &keylen, &data, &datalen);
				free(__ypcurrent);
				/*printf("yp_next %d\n", r);*/
				switch(r) {
				case 0:
					break;
				default:
					__ypcurrent = NULL;
					__ypmode = 0;
					free(data);
					continue;
				}
				__ypcurrent = key;
				__ypcurrentlen = keylen;
				bcopy(data, line, datalen);
				free(data);
			} else {
				r = yp_first(__ypdomain, "group.byname",
					&__ypcurrent, &__ypcurrentlen,
					&data, &datalen);
				/*printf("yp_first %d\n", r);*/
				switch(r) {
				case 0:
					break;
				default:
					__ypmode = 0;
					free(data);
					continue;
				}
				bcopy(data, line, datalen);
				free(data);
			}
			line[datalen] = '\0';
			/*printf("line = %s\n", line);*/
			bp = line;
			goto parse;
		}
#endif
		if (!fgets(line, sizeof(line), _gr_fp))
			return(0);
		bp = line;
		/* skip lines that are too big */
		if (!strchr(line, '\n')) {
			int ch;

			while ((ch = getc(_gr_fp)) != '\n' && ch != EOF)
				;
			continue;
		}
#ifdef YP
		if ((strcmp("+\n", line) == 0) || (strncmp("+:*:0:", line, 5) == 0)) {
			if(_yp_check(NULL)) {
				__ypmode = 1;
				continue;
			}
		}
parse:
#endif
		_gr_group.gr_name = strsep(&bp, ":\n");
		if (search && name && strcmp(_gr_group.gr_name, name))
			continue;
		_gr_group.gr_passwd = strsep(&bp, ":\n");
		if (!(cp = strsep(&bp, ":\n")))
			continue;
		_gr_group.gr_gid = atoi(cp);
		if (search && name == NULL && _gr_group.gr_gid != gid)
			continue;
		for (m = _gr_group.gr_mem = members;; ++m) {
			if (m == &members[MAXGRP - 1]) {
				*m = NULL;
				break;
			}
			if ((*m = strsep(&bp, ", \n")) == NULL)
				break;
		}
		return(1);
	}
	/* NOTREACHED */
}
