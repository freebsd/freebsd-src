/*-
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

#ifndef lint
static char sccsid[] = "@(#)create.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <time.h>
#include <fcntl.h>
#include <fts.h>
#include <dirent.h>
#include <grp.h>
#include <pwd.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <md5.h>
#include "mtree.h"
#include "extern.h"

#define	INDENTNAMELEN	15
#define	MAXLINELEN	80

extern long int crc_total;
extern int ftsoptions;
extern int dflag, iflag, nflag, sflag;
extern u_short keys;
extern char fullpath[MAXPATHLEN];

static gid_t gid;
static uid_t uid;
static mode_t mode;

static int	dsort __P((const FTSENT **, const FTSENT **));
static void	output __P((int, int *, const char *, ...));
static int	statd __P((FTS *, FTSENT *, uid_t *, gid_t *, mode_t *));
static void	statf __P((int, FTSENT *));

void
cwalk()
{
	register FTS *t;
	register FTSENT *p;
	time_t clock;
	char *argv[2], host[MAXHOSTNAMELEN];
	int indent = 0;

	(void)time(&clock);
	(void)gethostname(host, sizeof(host));
	(void)printf(
	    "#\t   user: %s\n#\tmachine: %s\n#\t   tree: %s\n#\t   date: %s",
	    getlogin(), host, fullpath, ctime(&clock));

	argv[0] = ".";
	argv[1] = NULL;
	if ((t = fts_open(argv, ftsoptions, dsort)) == NULL)
		err("fts_open: %s", strerror(errno));
	while ((p = fts_read(t))) {
		if (iflag)
			indent = p->fts_level * 4;
		switch(p->fts_info) {
		case FTS_D:
			if (!dflag)
				(void)printf("\n");
			if (!nflag)
				(void)printf("# %s\n", p->fts_path);
			statd(t, p, &uid, &gid, &mode);
			statf(indent, p);
			break;
		case FTS_DP:
			if (!nflag && (p->fts_level > 0))
				(void)printf("%*s# %s\n", indent, "", p->fts_path);
			(void)printf("%*s..\n", indent, "");
			if (!dflag)
				(void)printf("\n");
			break;
		case FTS_DNR:
		case FTS_ERR:
		case FTS_NS:
			(void)fprintf(stderr, "mtree: %s: %s\n",
			    p->fts_path, strerror(p->fts_errno));
			break;
		default:
			if (!dflag)
				statf(indent, p);
			break;

		}
	}
	(void)fts_close(t);
	if (sflag && keys & F_CKSUM)
		(void)fprintf(stderr,
		    "mtree: %s checksum: %lu\n", fullpath, crc_total);
}

static void
statf(indent, p)
	int indent;
	FTSENT *p;
{
	struct group *gr;
	struct passwd *pw;
	u_long len, val;
	int fd, offset;

	if (iflag || S_ISDIR(p->fts_statp->st_mode))
		offset = printf("%*s%s", indent, "", p->fts_name);
	else
		offset = printf("%*s    %s", indent, "", p->fts_name);

	if (offset > (INDENTNAMELEN + indent))
		offset = MAXLINELEN;
	else
		offset += printf("%*s", (INDENTNAMELEN + indent) - offset, "");

	if (!S_ISREG(p->fts_statp->st_mode) && !dflag)
		output(indent, &offset, "type=%s", inotype(p->fts_statp->st_mode));
	if (p->fts_statp->st_uid != uid) {
		if (keys & F_UNAME) {
			if ((pw = getpwuid(p->fts_statp->st_uid)) != NULL) {
				output(indent, &offset, "uname=%s", pw->pw_name);
			} else {
				err("could not get uname for uid=%u",
				    p->fts_statp->st_uid);
			}
		}
		if (keys & F_UID)
			output(indent, &offset, "uid=%u", p->fts_statp->st_uid);
	}
	if (p->fts_statp->st_gid != gid) {
		if (keys & F_GNAME) {
			if ((gr = getgrgid(p->fts_statp->st_gid)) != NULL) {
				output(indent, &offset, "gname=%s", gr->gr_name);
			} else {
				err("could not get gname for gid=%u",
				    p->fts_statp->st_gid);
			}
		}
		if (keys & F_GID)
			output(indent, &offset, "gid=%u", p->fts_statp->st_gid);
	}
	if (keys & F_MODE && (p->fts_statp->st_mode & MBITS) != mode)
		output(indent, &offset, "mode=%#o", p->fts_statp->st_mode & MBITS);
	if (keys & F_NLINK && p->fts_statp->st_nlink != 1)
		output(indent, &offset, "nlink=%u", p->fts_statp->st_nlink);
	if (keys & F_SIZE)
		output(indent, &offset, "size=%qd", p->fts_statp->st_size);
	if (keys & F_TIME)
		output(indent, &offset, "time=%ld.%ld",
		    p->fts_statp->st_mtimespec.tv_sec,
		    p->fts_statp->st_mtimespec.tv_nsec);
	if (keys & F_CKSUM && S_ISREG(p->fts_statp->st_mode)) {
		if ((fd = open(p->fts_accpath, O_RDONLY, 0)) < 0 ||
		    crc(fd, &val, &len))
			err("%s: %s", p->fts_accpath, strerror(errno));
		(void)close(fd);
		output(indent, &offset, "cksum=%lu", val);
	}
	if (keys & F_MD5 && S_ISREG(p->fts_statp->st_mode)) {
		char *md5digest, buf[33];

		md5digest = MD5File(p->fts_accpath,buf);
		if (!md5digest) {
			err("%s: %s", p->fts_accpath, strerror(errno));
		} else {
			output(indent, &offset, "md5digest=%s", md5digest);
		}
	}
	if (keys & F_SLINK &&
	    (p->fts_info == FTS_SL || p->fts_info == FTS_SLNONE))
		output(indent, &offset, "link=%s", rlink(p->fts_accpath));
	(void)putchar('\n');
}

#define	MAXGID	5000
#define	MAXUID	5000
#define	MAXMODE	MBITS + 1

static int
statd(t, parent, puid, pgid, pmode)
	FTS *t;
	FTSENT *parent;
	uid_t *puid;
	gid_t *pgid;
	mode_t *pmode;
{
	register FTSENT *p;
	register gid_t sgid;
	register uid_t suid;
	register mode_t smode;
	struct group *gr;
	struct passwd *pw;
	gid_t savegid = *pgid;
	uid_t saveuid = *puid;
	mode_t savemode = *pmode;
	u_short maxgid, maxuid, maxmode, g[MAXGID], u[MAXUID], m[MAXMODE];
	static int first = 1;

	if ((p = fts_children(t, 0)) == NULL) {
		if (errno)
			err("%s: %s", RP(parent), strerror(errno));
		return (1);
	}

	bzero(g, sizeof(g));
	bzero(u, sizeof(u));
	bzero(m, sizeof(m));

	maxuid = maxgid = maxmode = 0;
	for (; p; p = p->fts_link) {
		if (!dflag || (dflag && S_ISDIR(p->fts_statp->st_mode))) {
			smode = p->fts_statp->st_mode & MBITS;
			if (smode < MAXMODE && ++m[smode] > maxmode) {
				savemode = smode;
				maxmode = m[smode];
			}
			sgid = p->fts_statp->st_gid;
			if (sgid < MAXGID && ++g[sgid] > maxgid) {
				savegid = sgid;
				maxgid = g[sgid];
			}
			suid = p->fts_statp->st_uid;
			if (suid < MAXUID && ++u[suid] > maxuid) {
				saveuid = suid;
				maxuid = u[suid];
			}
		}
	}
	/*
	 * If the /set record is the same as the last one we do not need to output
	 * a new one.  So first we check to see if anything changed.  Note that we
	 * always output a /set record for the first directory.
	 */
	if ((((keys & F_UNAME) | (keys & F_UID)) && (*puid != saveuid)) ||
	    (((keys & F_GNAME) | (keys & F_GID)) && (*pgid != savegid)) ||
	    ((keys & F_MODE) && (*pmode != savemode)) || (first)) {
		first = 0;
		if (dflag)
			(void)printf("/set type=dir");
		else
			(void)printf("/set type=file");
		if (keys & F_UNAME)
			if ((pw = getpwuid(saveuid)) != NULL)
				(void)printf(" uname=%s", pw->pw_name);
			else
				err("could not get uname for uid=%u", saveuid);
		if (keys & F_UID)
			(void)printf(" uid=%lu", saveuid);
		if (keys & F_GNAME)
			if ((gr = getgrgid(savegid)) != NULL)
				(void)printf(" gname=%s", gr->gr_name);
			else
				err("could not get gname for gid=%u", savegid);
		if (keys & F_GID)
			(void)printf(" gid=%lu", savegid);
		if (keys & F_MODE)
			(void)printf(" mode=%#o", savemode);
		if (keys & F_NLINK)
			(void)printf(" nlink=1");
		(void)printf("\n");
		*puid = saveuid;
		*pgid = savegid;
		*pmode = savemode;
	}
	return (0);
}

static int
dsort(a, b)
	const FTSENT **a, **b;
{
	if (S_ISDIR((*a)->fts_statp->st_mode)) {
		if (!S_ISDIR((*b)->fts_statp->st_mode))
			return (1);
	} else if (S_ISDIR((*b)->fts_statp->st_mode))
		return (-1);
	return (strcmp((*a)->fts_name, (*b)->fts_name));
}

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

void
#if __STDC__
output(int indent, int *offset, const char *fmt, ...)
#else
output(indent, offset, fmt, va_alist)
	int indent;
	int *offset;
	char *fmt;
        va_dcl
#endif
{
	va_list ap;
	char buf[1024];
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	(void)vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	if (*offset + strlen(buf) > MAXLINELEN - 3) {
		(void)printf(" \\\n%*s", INDENTNAMELEN + indent, "");
		*offset = INDENTNAMELEN + indent;
	}
	*offset += printf(" %s", buf) + 1;
}
