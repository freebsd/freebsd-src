/*
 * Copyright (c) 1988, 1993, 1994
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
static const char copyright[] =
"@(#) Copyright (c) 1988, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)chown.c	8.8 (Berkeley) 4/4/94";
#else
static const char rcsid[] =
  "$FreeBSD$";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fts.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void	a_gid __P((char *));
void	a_uid __P((char *));
void	chownerr __P((char *));
u_long	id __P((char *, char *));
void	usage __P((void));

uid_t uid;
gid_t gid;
int Rflag, ischown, fflag, hflag, vflag;
char *gname, *myname;

int
main(argc, argv)
	int argc;
	char *argv[];
{
	FTS *ftsp;
	FTSENT *p;
	int Hflag, Lflag, Pflag, ch, fts_options, hflag, rval;
	char *cp;

	myname = (cp = rindex(*argv, '/')) ? cp + 1 : *argv;
	ischown = myname[2] == 'o';

	Hflag = Lflag = Pflag = hflag = vflag = 0;
	while ((ch = getopt(argc, argv, "HLPRfhv")) != -1)
		switch (ch) {
		case 'H':
			Hflag = 1;
			Lflag = Pflag = 0;
			break;
		case 'L':
			Lflag = 1;
			Hflag = Pflag = 0;
			break;
		case 'P':
			Pflag = 1;
			Hflag = Lflag = 0;
			break;
		case 'R':
			Rflag = 1;
			break;
		case 'f':
			fflag = 1;
			break;
		case 'h':
			hflag = 1;
			break;
		case 'v':
			vflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argv += optind;
	argc -= optind;

	if (argc < 2)
		usage();

	if (Rflag) {
		fts_options = FTS_PHYSICAL;
		if (hflag && (Lflag || Hflag))
			errx(1, "the -R and -h options may not be specified together");
		if (Hflag)
			fts_options |= FTS_COMFOLLOW;
		if (Lflag) {
			fts_options &= ~FTS_PHYSICAL;
			fts_options |= FTS_LOGICAL;
		}
	} else
		fts_options = hflag ? FTS_PHYSICAL : FTS_LOGICAL;

	uid = gid = -1;
	if (ischown) {
		if ((cp = strchr(*argv, ':')) != NULL) {
			*cp++ = '\0';
			a_gid(cp);
		}
#ifdef SUPPORT_DOT
		else if ((cp = strchr(*argv, '.')) != NULL) {
			*cp++ = '\0';
			a_gid(cp);
		}
#endif
		a_uid(*argv);
	} else
		a_gid(*argv);

	if ((ftsp = fts_open(++argv, fts_options, 0)) == NULL)
		err(1, NULL);

	for (rval = 0; (p = fts_read(ftsp)) != NULL;) {
		switch (p->fts_info) {
		case FTS_D: 			/* Change it at FTS_DP. */
			if (!Rflag)
				fts_set(ftsp, p, FTS_SKIP);
			continue;
		case FTS_DNR:			/* Warn, chown, continue. */
			warnx("%s: %s", p->fts_path, strerror(p->fts_errno));
			rval = 1;
			break;
		case FTS_ERR:			/* Warn, continue. */
		case FTS_NS:
			warnx("%s: %s", p->fts_path, strerror(p->fts_errno));
			rval = 1;
			continue;
		case FTS_SL:			/* Ignore. */
		case FTS_SLNONE:
			/*
			 * The only symlinks that end up here are ones that
			 * don't point to anything and ones that we found
			 * doing a physical walk.
			 */
			if (hflag)
				break;
			else
				continue;
		default:
			break;
		}
		if ((uid == -1 || uid == p->fts_statp->st_uid) &&
		    (gid == -1 || gid == p->fts_statp->st_gid))
			continue;
		if (hflag) {
			if (lchown(p->fts_accpath, uid, gid) && !fflag) {
				chownerr(p->fts_path);
				rval = 1;
			} else {
			    	if (vflag)
					(void)printf("%s\n", p->fts_accpath);
			}
		} else {
			if (chown(p->fts_accpath, uid, gid) && !fflag) {
				chownerr(p->fts_path);
				rval = 1;
			} else {
			    	if (vflag)
					(void)printf("%s\n", p->fts_accpath);
			}
		}
	}
	if (errno)
		err(1, "fts_read");
	exit(rval);
}

void
a_gid(s)
	char *s;
{
	struct group *gr;

	if (*s == '\0')			/* Argument was "uid[:.]". */
		return;
	gname = s;
	gid = ((gr = getgrnam(s)) == NULL) ? id(s, "group") : gr->gr_gid;
}

void
a_uid(s)
	char *s;
{
	struct passwd *pw;

	if (*s == '\0')			/* Argument was "[:.]gid". */
		return;
	uid = ((pw = getpwnam(s)) == NULL) ? id(s, "user") : pw->pw_uid;
}

u_long
id(name, type)
	char *name, *type;
{
	u_long val;
	char *ep;

	/*
	 * XXX
	 * We know that uid_t's and gid_t's are unsigned longs.
	 */
	errno = 0;
	val = strtoul(name, &ep, 10);
	if (errno)
		err(1, "%s", name);
	if (*ep != '\0')
		errx(1, "%s: illegal %s name", name, type);
	return (val);
}

void
chownerr(file)
	char *file;
{
	static int euid = -1, ngroups = -1;
	gid_t groups[NGROUPS];

	/* Check for chown without being root. */
	if (errno != EPERM ||
	    (uid != -1 && euid == -1 && (euid = geteuid()) != 0))
		err(1, "%s", file);

	/* Check group membership; kernel just returns EPERM. */
	if (gid != -1 && ngroups == -1 &&
	    euid == -1 && (euid = geteuid()) != 0) {
		ngroups = getgroups(NGROUPS, groups);
		while (--ngroups >= 0 && gid != groups[ngroups]);
		if (ngroups < 0)
			errx(1, "you are not a member of group %s", gname);
	}
	warn("%s", file);
}

void
usage()
{
	(void)fprintf(stderr, "%s\n%s\n%s\n",
	    "usage: chown [-R [-H | -L | -P]] [-f] [-h] [-v] owner[:group] file ...",
	    "       chown [-R [-H | -L | -P]] [-f] [-h] [-v] :group file ...",
	    "       chgrp [-R [-H | -L | -P]] [-f] [-h] [-v] group file ...");
	exit(1);
}
