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
#if 0
static char sccsid[] = "@(#)compare.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#ifdef MD5
#include <md5.h>
#endif
#ifdef SHA1
#include <sha.h>
#endif
#ifdef RMD160
#include <ripemd.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "mtree.h"
#include "extern.h"

extern int uflag;
extern int lineno;

static const char *ftype(u_int);

#define	INDENTNAMELEN	8
#define	LABEL \
	if (!label++) { \
		len = printf("%s changed\n", RP(p)); \
		tab = "\t"; \
	}

int
compare(name, s, p)
	char *name __unused;
	register NODE *s;
	register FTSENT *p;
{
	extern int uflag;
	u_long len, val;
	int fd, label;
	char *cp;
	const char *tab = "";
	char *fflags;

	label = 0;
	switch(s->type) {
	case F_BLOCK:
		if (!S_ISBLK(p->fts_statp->st_mode))
			goto typeerr;
		break;
	case F_CHAR:
		if (!S_ISCHR(p->fts_statp->st_mode))
			goto typeerr;
		break;
	case F_DIR:
		if (!S_ISDIR(p->fts_statp->st_mode))
			goto typeerr;
		break;
	case F_FIFO:
		if (!S_ISFIFO(p->fts_statp->st_mode))
			goto typeerr;
		break;
	case F_FILE:
		if (!S_ISREG(p->fts_statp->st_mode))
			goto typeerr;
		break;
	case F_LINK:
		if (!S_ISLNK(p->fts_statp->st_mode))
			goto typeerr;
		break;
	case F_SOCK:
		if (!S_ISSOCK(p->fts_statp->st_mode)) {
typeerr:		LABEL;
			(void)printf("\ttype expected %s found %s\n",
			    ftype(s->type), inotype(p->fts_statp->st_mode));
			return (label);
		}
		break;
	}
	/* Set the uid/gid first, then set the mode. */
	if (s->flags & (F_UID | F_UNAME) && s->st_uid != p->fts_statp->st_uid) {
		LABEL;
		(void)printf("%suser expected %lu found %lu",
		    tab, (u_long)s->st_uid, (u_long)p->fts_statp->st_uid);
		if (uflag)
			if (chown(p->fts_accpath, s->st_uid, -1))
				(void)printf(" not modified: %s\n",
				    strerror(errno));
			else
				(void)printf(" modified\n");
		else
			(void)printf("\n");
		tab = "\t";
	}
	if (s->flags & (F_GID | F_GNAME) && s->st_gid != p->fts_statp->st_gid) {
		LABEL;
		(void)printf("%sgid expected %lu found %lu",
		    tab, (u_long)s->st_gid, (u_long)p->fts_statp->st_gid);
		if (uflag)
			if (chown(p->fts_accpath, -1, s->st_gid))
				(void)printf(" not modified: %s\n",
				    strerror(errno));
			else
				(void)printf(" modified\n");
		else
			(void)printf("\n");
		tab = "\t";
	}
	if (s->flags & F_MODE &&
	    !S_ISLNK(p->fts_statp->st_mode) &&
	    s->st_mode != (p->fts_statp->st_mode & MBITS)) {
		LABEL;
		(void)printf("%spermissions expected %#o found %#o",
		    tab, s->st_mode, p->fts_statp->st_mode & MBITS);
		if (uflag)
			if (chmod(p->fts_accpath, s->st_mode))
				(void)printf(" not modified: %s\n",
				    strerror(errno));
			else
				(void)printf(" modified\n");
		else
			(void)printf("\n");
		tab = "\t";
	}
	if (s->flags & F_NLINK && s->type != F_DIR &&
	    s->st_nlink != p->fts_statp->st_nlink) {
		LABEL;
		(void)printf("%slink_count expected %u found %u\n",
		    tab, s->st_nlink, p->fts_statp->st_nlink);
		tab = "\t";
	}
	if (s->flags & F_SIZE && s->st_size != p->fts_statp->st_size &&
		!S_ISDIR(p->fts_statp->st_mode)) {
		LABEL;
		(void)printf("%ssize expected %jd found %jd\n", tab,
		    (intmax_t)s->st_size, (intmax_t)p->fts_statp->st_size);
		tab = "\t";
	}
	/*
	 * XXX
	 * Catches nano-second differences, but doesn't display them.
	 */
	if ((s->flags & F_TIME) &&
	     ((s->st_mtimespec.tv_sec != p->fts_statp->st_mtimespec.tv_sec) ||
	     (s->st_mtimespec.tv_nsec != p->fts_statp->st_mtimespec.tv_nsec))) {
		LABEL;
		(void)printf("%smodification time expected %.24s ",
		    tab, ctime(&s->st_mtimespec.tv_sec));
		(void)printf("found %.24s\n",
		    ctime(&p->fts_statp->st_mtimespec.tv_sec));
		tab = "\t";
	}
	if (s->flags & F_CKSUM) {
		if ((fd = open(p->fts_accpath, O_RDONLY, 0)) < 0) {
			LABEL;
			(void)printf("%scksum: %s: %s\n",
			    tab, p->fts_accpath, strerror(errno));
			tab = "\t";
		} else if (crc(fd, &val, &len)) {
			(void)close(fd);
			LABEL;
			(void)printf("%scksum: %s: %s\n",
			    tab, p->fts_accpath, strerror(errno));
			tab = "\t";
		} else {
			(void)close(fd);
			if (s->cksum != val) {
				LABEL;
				(void)printf("%scksum expected %lu found %lu\n",
				    tab, s->cksum, val);
			}
			tab = "\t";
		}
	}
	/*
	 * XXX
	 * since chflags(2) will reset file times, the utimes() above
	 * may have been useless!  oh well, we'd rather have correct
	 * flags, rather than times?
	 */
	if ((s->flags & F_FLAGS) && s->st_flags != p->fts_statp->st_flags) {
		LABEL;
		fflags = flags_to_string(s->st_flags);
		(void)printf("%sflags expected \"%s\"", tab, fflags);
		free(fflags);

		fflags = flags_to_string(p->fts_statp->st_flags);
		(void)printf(" found \"%s\"", fflags);
		free(fflags);

		if (uflag)
			if (chflags(p->fts_accpath, s->st_flags))
				(void)printf(" not modified: %s\n",
				    strerror(errno));
			else
				(void)printf(" modified\n");
		else
			(void)printf("\n");
		tab = "\t";
	}
#ifdef MD5
	if (s->flags & F_MD5) {
		char *new_digest, buf[33];

		new_digest = MD5File(p->fts_accpath, buf);
		if (!new_digest) {
			LABEL;
			printf("%sMD5: %s: %s\n", tab, p->fts_accpath,
			       strerror(errno));
			tab = "\t";
		} else if (strcmp(new_digest, s->md5digest)) {
			LABEL;
			printf("%sMD5 expected %s found %s\n", tab, s->md5digest,
			       new_digest);
			tab = "\t";
		}
	}
#endif /* MD5 */
#ifdef SHA1
	if (s->flags & F_SHA1) {
		char *new_digest, buf[41];

		new_digest = SHA1_File(p->fts_accpath, buf);
		if (!new_digest) {
			LABEL;
			printf("%sSHA-1: %s: %s\n", tab, p->fts_accpath,
			       strerror(errno));
			tab = "\t";
		} else if (strcmp(new_digest, s->sha1digest)) {
			LABEL;
			printf("%sSHA-1 expected %s found %s\n", 
			       tab, s->sha1digest, new_digest);
			tab = "\t";
		}
	}
#endif /* SHA1 */
#ifdef RMD160
	if (s->flags & F_RMD160) {
		char *new_digest, buf[41];

		new_digest = RIPEMD160_File(p->fts_accpath, buf);
		if (!new_digest) {
			LABEL;
			printf("%sRIPEMD160: %s: %s\n", tab,
			       p->fts_accpath, strerror(errno));
			tab = "\t";
		} else if (strcmp(new_digest, s->rmd160digest)) {
			LABEL;
			printf("%sRIPEMD160 expected %s found %s\n",
			       tab, s->rmd160digest, new_digest);
			tab = "\t";
		}
	}
#endif /* RMD160 */

	if (s->flags & F_SLINK &&
	    strcmp(cp = rlink(p->fts_accpath), s->slink)) {
		LABEL;
		(void)printf("%slink_ref expected %s found %s\n",
		      tab, cp, s->slink);
	}
	return (label);
}

const char *
inotype(type)
	u_int type;
{
	switch(type & S_IFMT) {
	case S_IFBLK:
		return ("block");
	case S_IFCHR:
		return ("char");
	case S_IFDIR:
		return ("dir");
	case S_IFIFO:
		return ("fifo");
	case S_IFREG:
		return ("file");
	case S_IFLNK:
		return ("link");
	case S_IFSOCK:
		return ("socket");
	default:
		return ("unknown");
	}
	/* NOTREACHED */
}

static const char *
ftype(type)
	u_int type;
{
	switch(type) {
	case F_BLOCK:
		return ("block");
	case F_CHAR:
		return ("char");
	case F_DIR:
		return ("dir");
	case F_FIFO:
		return ("fifo");
	case F_FILE:
		return ("file");
	case F_LINK:
		return ("link");
	case F_SOCK:
		return ("socket");
	default:
		return ("unknown");
	}
	/* NOTREACHED */
}

char *
rlink(name)
	char *name;
{
	static char lbuf[MAXPATHLEN];
	register int len;

	if ((len = readlink(name, lbuf, sizeof(lbuf) - 1)) == -1)
		err(1, "line %d: %s", lineno, name);
	lbuf[len] = '\0';
	return (lbuf);
}
