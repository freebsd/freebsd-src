/*
 * Copyright (c) 1980, 1988, 1993
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
 *
 * $FreeBSD: src/lib/libc/gen/fstab.c,v 1.8 2000/01/27 23:06:15 jasone Exp $
 */

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
static char sccsid[] = "@(#)fstab.c	8.1 (Berkeley) 6/4/93";
#else
static char rcsid[] =
 "$FreeBSD: src/lib/libc/gen/fstab.c,v 1.8 2000/01/27 23:06:15 jasone Exp $";
#endif
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <errno.h>
#include <fstab.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static FILE *_fs_fp;
static struct fstab _fs_fstab;
static int LineNo = 0;

static void error __P((int));
static void fixfsfile __P((void));
static int fstabscan __P((void));

static void
fixfsfile()
{
	static char buf[sizeof(_PATH_DEV) + MNAMELEN];
	struct stat sb;
	struct statfs sf;

	if (strcmp(_fs_fstab.fs_file, "/") != 0)
		return;
	if (statfs("/", &sf) != 0)
		return;
	if (sf.f_mntfromname[0] == '/')
		buf[0] = '\0';
	else
		strcpy(buf, _PATH_DEV);
	strcat(buf, sf.f_mntfromname);
	if (stat(buf, &sb) != 0 ||
	    (!S_ISBLK(sb.st_mode) && !S_ISCHR(sb.st_mode)))
		return;
	_fs_fstab.fs_spec = buf;
}

static int
fstabscan()
{
	char *cp, *p;
#define	MAXLINELENGTH	1024
	static char line[MAXLINELENGTH];
	char subline[MAXLINELENGTH];
	int typexx;

	for (;;) {

		if (!(p = fgets(line, sizeof(line), _fs_fp)))
			return(0);
/* OLD_STYLE_FSTAB */
		++LineNo;
		if (*line == '#' || *line == '\n')
			continue;
		if (!strpbrk(p, " \t")) {
			_fs_fstab.fs_spec = strsep(&p, ":\n");
			_fs_fstab.fs_file = strsep(&p, ":\n");
			fixfsfile();
			_fs_fstab.fs_type = strsep(&p, ":\n");
			if (_fs_fstab.fs_type) {
				if (!strcmp(_fs_fstab.fs_type, FSTAB_XX))
					continue;
				_fs_fstab.fs_mntops = _fs_fstab.fs_type;
				_fs_fstab.fs_vfstype =
				    strcmp(_fs_fstab.fs_type, FSTAB_SW) ?
				    "ufs" : "swap";
				if ((cp = strsep(&p, ":\n")) != NULL) {
					_fs_fstab.fs_freq = atoi(cp);
					if ((cp = strsep(&p, ":\n")) != NULL) {
						_fs_fstab.fs_passno = atoi(cp);
						return(1);
					}
				}
			}
			goto bad;
		}
/* OLD_STYLE_FSTAB */
		while ((cp = strsep(&p, " \t\n")) != NULL && *cp == '\0')
			;
		_fs_fstab.fs_spec = cp;
		if (!_fs_fstab.fs_spec || *_fs_fstab.fs_spec == '#')
			continue;
		while ((cp = strsep(&p, " \t\n")) != NULL && *cp == '\0')
			;
		_fs_fstab.fs_file = cp;
		fixfsfile();
		while ((cp = strsep(&p, " \t\n")) != NULL && *cp == '\0')
			;
		_fs_fstab.fs_vfstype = cp;
		while ((cp = strsep(&p, " \t\n")) != NULL && *cp == '\0')
			;
		_fs_fstab.fs_mntops = cp;
		if (_fs_fstab.fs_mntops == NULL)
			goto bad;
		_fs_fstab.fs_freq = 0;
		_fs_fstab.fs_passno = 0;
		while ((cp = strsep(&p, " \t\n")) != NULL && *cp == '\0')
			;
		if (cp != NULL) {
			_fs_fstab.fs_freq = atoi(cp);
			while ((cp = strsep(&p, " \t\n")) != NULL && *cp == '\0')
				;
			if (cp != NULL)
				_fs_fstab.fs_passno = atoi(cp);
		}
		strcpy(subline, _fs_fstab.fs_mntops);
		p = subline;
		for (typexx = 0, cp = strsep(&p, ","); cp;
		     cp = strsep(&p, ",")) {
			if (strlen(cp) != 2)
				continue;
			if (!strcmp(cp, FSTAB_RW)) {
				_fs_fstab.fs_type = FSTAB_RW;
				break;
			}
			if (!strcmp(cp, FSTAB_RQ)) {
				_fs_fstab.fs_type = FSTAB_RQ;
				break;
			}
			if (!strcmp(cp, FSTAB_RO)) {
				_fs_fstab.fs_type = FSTAB_RO;
				break;
			}
			if (!strcmp(cp, FSTAB_SW)) {
				_fs_fstab.fs_type = FSTAB_SW;
				break;
			}
			if (!strcmp(cp, FSTAB_XX)) {
				_fs_fstab.fs_type = FSTAB_XX;
				typexx++;
				break;
			}
		}
		if (typexx)
			continue;
		if (cp != NULL)
			return(1);

bad:		/* no way to distinguish between EOF and syntax error */
		error(EFTYPE);
	}
	/* NOTREACHED */
}

struct fstab *
getfsent()
{
	if ((!_fs_fp && !setfsent()) || !fstabscan())
		return((struct fstab *)NULL);
	return(&_fs_fstab);
}

struct fstab *
getfsspec(name)
	register const char *name;
{
	if (setfsent())
		while (fstabscan())
			if (!strcmp(_fs_fstab.fs_spec, name))
				return(&_fs_fstab);
	return((struct fstab *)NULL);
}

struct fstab *
getfsfile(name)
	register const char *name;
{
	if (setfsent())
		while (fstabscan())
			if (!strcmp(_fs_fstab.fs_file, name))
				return(&_fs_fstab);
	return((struct fstab *)NULL);
}

int 
setfsent()
{
	if (_fs_fp) {
		rewind(_fs_fp);
		LineNo = 0;
		return(1);
	}
	if ((_fs_fp = fopen(_PATH_FSTAB, "r")) != NULL) {
		LineNo = 0;
		return(1);
	}
	error(errno);
	return(0);
}

void
endfsent()
{
	if (_fs_fp) {
		(void)fclose(_fs_fp);
		_fs_fp = NULL;
	}
}

static void
error(err)
	int err;
{
	char *p;
	char num[30];

	(void)_write(STDERR_FILENO, "fstab: ", 7);
	(void)_write(STDERR_FILENO, _PATH_FSTAB, sizeof(_PATH_FSTAB) - 1);
	(void)_write(STDERR_FILENO, ":", 1);
	sprintf(num, "%d: ", LineNo);
	(void)_write(STDERR_FILENO, num, strlen(num));
	p = strerror(err);
	(void)_write(STDERR_FILENO, p, strlen(p));
	(void)_write(STDERR_FILENO, "\n", 1);
}
