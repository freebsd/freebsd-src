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

#if 0
#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1988, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)chown.c	8.8 (Berkeley) 4/4/94";
#endif /* not lint */
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fts.h>
#include <grp.h>
#include <libgen.h>
#include <pwd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void	a_gid(const char *);
static void	a_uid(const char *);
static void	chownerr(const char *);
static uid_t	id(const char *, const char *);
static void	usage(void);

static uid_t uid;
static gid_t gid;
static int ischown;
static const char *gname;

int
main(int argc, char **argv)
{
	FTS *ftsp;
	FTSENT *p;
	int Hflag, Lflag, Rflag, fflag, hflag, vflag, xflag;
	int ch, fts_options, rval;
	char *cp;

	ischown = (strcmp(basename(argv[0]), "chown") == 0);

	Hflag = Lflag = Rflag = fflag = hflag = vflag = xflag = 0;
	while ((ch = getopt(argc, argv, "HLPRfhvx")) != -1)
		switch (ch) {
		case 'H':
			Hflag = 1;
			Lflag = 0;
			break;
		case 'L':
			Lflag = 1;
			Hflag = 0;
			break;
		case 'P':
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
			vflag++;
			break;
		case 'x':
			xflag = 1;
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
		if (hflag && (Hflag || Lflag))
			errx(1, "the -R%c and -h options may not be "
			    "specified together", Hflag ? 'H' : 'L');
		if (Hflag)
			fts_options |= FTS_COMFOLLOW;
		else if (Lflag) {
			fts_options &= ~FTS_PHYSICAL;
			fts_options |= FTS_LOGICAL;
		}
	} else
		fts_options = hflag ? FTS_PHYSICAL : FTS_LOGICAL;
	if (xflag)
		fts_options |= FTS_XDEV;

	uid = (uid_t)-1;
	gid = (gid_t)-1;
	if (ischown) {
		if ((cp = strchr(*argv, ':')) != NULL) {
			*cp++ = '\0';
			a_gid(cp);
		}
#ifdef SUPPORT_DOT
		else if ((cp = strchr(*argv, '.')) != NULL) {
			warnx("separation of user and group with a period is deprecated");
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
		case FTS_D:			/* Change it at FTS_DP. */
			if (!Rflag)
				fts_set(ftsp, p, FTS_SKIP);
			continue;
		case FTS_DNR:			/* Warn, chown. */
			warnx("%s: %s", p->fts_path, strerror(p->fts_errno));
			rval = 1;
			break;
		case FTS_ERR:			/* Warn, continue. */
		case FTS_NS:
			warnx("%s: %s", p->fts_path, strerror(p->fts_errno));
			rval = 1;
			continue;
		case FTS_SL:
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
		if ((uid == (uid_t)-1 || uid == p->fts_statp->st_uid) &&
		    (gid == (gid_t)-1 || gid == p->fts_statp->st_gid))
			continue;
		if ((hflag ? lchown : chown)(p->fts_accpath, uid, gid) == -1) {
			if (!fflag) {
				chownerr(p->fts_path);
				rval = 1;
			}
		} else {
			if (vflag) {
				printf("%s", p->fts_path);
				if (vflag > 1) {
					if (ischown) {
						printf(": %ju:%ju -> %ju:%ju",
						    (uintmax_t)
						    p->fts_statp->st_uid, 
						    (uintmax_t)
						    p->fts_statp->st_gid,
						    (uid == (uid_t)-1) ? 
						    (uintmax_t)
						    p->fts_statp->st_uid : 
						    (uintmax_t)uid,
						    (gid == (gid_t)-1) ? 
						    (uintmax_t)
						    p->fts_statp->st_gid :
						    (uintmax_t)gid);
					} else {
						printf(": %ju -> %ju",
						    (uintmax_t)
						    p->fts_statp->st_gid,
						    (gid == (gid_t)-1) ? 
						    (uintmax_t)
						    p->fts_statp->st_gid : 
						    (uintmax_t)gid);
					}
				}
				printf("\n");
			}
		}
	}
	if (errno)
		err(1, "fts_read");
	exit(rval);
}

static void
a_gid(const char *s)
{
	struct group *gr;

	if (*s == '\0')			/* Argument was "uid[:.]". */
		return;
	gname = s;
	gid = ((gr = getgrnam(s)) != NULL) ? gr->gr_gid : id(s, "group");
}

static void
a_uid(const char *s)
{
	struct passwd *pw;

	if (*s == '\0')			/* Argument was "[:.]gid". */
		return;
	uid = ((pw = getpwnam(s)) != NULL) ? pw->pw_uid : id(s, "user");
}

static uid_t
id(const char *name, const char *type)
{
	uid_t val;
	char *ep;

	/*
	 * XXX
	 * We know that uid_t's and gid_t's are unsigned longs.
	 */
	errno = 0;
	val = strtoul(name, &ep, 10);
	if (errno || *ep != '\0')
		errx(1, "%s: illegal %s name", name, type);
	return (val);
}

static void
chownerr(const char *file)
{
	static uid_t euid = -1;
	static int ngroups = -1;
	static long ngroups_max;
	gid_t *groups;

	/* Check for chown without being root. */
	if (errno != EPERM || (uid != (uid_t)-1 &&
	    euid == (uid_t)-1 && (euid = geteuid()) != 0)) {
		warn("%s", file);
		return;
	}

	/* Check group membership; kernel just returns EPERM. */
	if (gid != (gid_t)-1 && ngroups == -1 &&
	    euid == (uid_t)-1 && (euid = geteuid()) != 0) {
		ngroups_max = sysconf(_SC_NGROUPS_MAX) + 1;
		if ((groups = malloc(sizeof(gid_t) * ngroups_max)) == NULL)
			err(1, "malloc");
		ngroups = getgroups(ngroups_max, groups);
		while (--ngroups >= 0 && gid != groups[ngroups]);
		free(groups);
		if (ngroups < 0) {
			warnx("you are not a member of group %s", gname);
			return;
		}
	}
	warn("%s", file);
}

static void
usage(void)
{

	if (ischown)
		(void)fprintf(stderr, "%s\n%s\n",
		    "usage: chown [-fhvx] [-R [-H | -L | -P]] owner[:group]"
		    " file ...",
		    "       chown [-fhvx] [-R [-H | -L | -P]] :group file ...");
	else
		(void)fprintf(stderr, "%s\n",
		    "usage: chgrp [-fhvx] [-R [-H | -L | -P]] group file ...");
	exit(1);
}
