/*
 * Copyright (c) 1980, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Robert Elz at The University of Melbourne.
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
"@(#) Copyright (c) 1980, 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)quotaon.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
	"$Id: quotaon.c,v 1.1.1.1.8.2 1997/11/04 07:24:32 charnier Exp $";
#endif /* not lint */

/*
 * Turn quota on/off for a filesystem.
 */
#include <sys/param.h>
#include <sys/file.h>
#include <sys/mount.h>
#include <ufs/ufs/quota.h>
#include <err.h>
#include <fstab.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char *qfname = QUOTAFILENAME;
char *qfextension[] = INITQFNAMES;

int	aflag;		/* all file systems */
int	gflag;		/* operate on group quotas */
int	uflag;		/* operate on user quotas */
int	vflag;		/* verbose */

int hasquota __P((struct fstab *, int, char **));
int oneof __P((char *, char *[], int));
int quotaonoff __P((struct fstab *fs, int, int, char *));
int readonly __P((struct fstab *));
static void usage __P((void));

int
main(argc, argv)
	int argc;
	char **argv;
{
	register struct fstab *fs;
	char ch, *qfnp, *whoami;
	long argnum, done = 0;
	int i, offmode = 0, errs = 0;

	whoami = rindex(*argv, '/') + 1;
	if (whoami == (char *)1)
		whoami = *argv;
	if (strcmp(whoami, "quotaoff") == 0)
		offmode++;
	else if (strcmp(whoami, "quotaon") != 0)
		errx(1, "name must be quotaon or quotaoff");
	while ((ch = getopt(argc, argv, "avug")) != -1) {
		switch(ch) {
		case 'a':
			aflag++;
			break;
		case 'g':
			gflag++;
			break;
		case 'u':
			uflag++;
			break;
		case 'v':
			vflag++;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc <= 0 && !aflag)
		usage();
	if (!gflag && !uflag) {
		gflag++;
		uflag++;
	}
	setfsent();
	while ((fs = getfsent()) != NULL) {
		if (strcmp(fs->fs_vfstype, "ufs") ||
		    strcmp(fs->fs_type, FSTAB_RW))
			continue;
		if (aflag) {
			if (gflag && hasquota(fs, GRPQUOTA, &qfnp))
				errs += quotaonoff(fs, offmode, GRPQUOTA, qfnp);
			if (uflag && hasquota(fs, USRQUOTA, &qfnp))
				errs += quotaonoff(fs, offmode, USRQUOTA, qfnp);
			continue;
		}
		if ((argnum = oneof(fs->fs_file, argv, argc)) >= 0 ||
		    (argnum = oneof(fs->fs_spec, argv, argc)) >= 0) {
			done |= 1 << argnum;
			if (gflag && hasquota(fs, GRPQUOTA, &qfnp))
				errs += quotaonoff(fs, offmode, GRPQUOTA, qfnp);
			if (uflag && hasquota(fs, USRQUOTA, &qfnp))
				errs += quotaonoff(fs, offmode, USRQUOTA, qfnp);
		}
	}
	endfsent();
	for (i = 0; i < argc; i++)
		if ((done & (1 << i)) == 0)
			warnx("%s not found in fstab", argv[i]);
	exit(errs);
}

static void
usage()
{

	fprintf(stderr, "%s\n%s\n%s\n%s\n",
		"usage: quotaon [-g] [-u] [-v] -a",
		"       quotaon [-g] [-u] [-v] filesystem ...",
		"       quotaoff [-g] [-u] [-v] -a",
		"       quotaoff [-g] [-u] [-v] filesystem ...");
	exit(1);
}

int
quotaonoff(fs, offmode, type, qfpathname)
	register struct fstab *fs;
	int offmode, type;
	char *qfpathname;
{

	if (strcmp(fs->fs_file, "/") && readonly(fs))
		return (1);
	if (offmode) {
		if (quotactl(fs->fs_file, QCMD(Q_QUOTAOFF, type), 0, 0) < 0) {
			warn("%s", fs->fs_file);
			return (1);
		}
		if (vflag)
			printf("%s: quotas turned off\n", fs->fs_file);
		return (0);
	}
	if (quotactl(fs->fs_file, QCMD(Q_QUOTAON, type), 0, qfpathname) < 0) {
		warnx("using %s on", qfpathname);
		warn("%s", fs->fs_file);
		return (1);
	}
	if (vflag)
		printf("%s: %s quotas turned on\n", fs->fs_file,
		    qfextension[type]);
	return (0);
}

/*
 * Check to see if target appears in list of size cnt.
 */
int
oneof(target, list, cnt)
	register char *target, *list[];
	int cnt;
{
	register int i;

	for (i = 0; i < cnt; i++)
		if (strcmp(target, list[i]) == 0)
			return (i);
	return (-1);
}

/*
 * Check to see if a particular quota is to be enabled.
 */
int
hasquota(fs, type, qfnamep)
	register struct fstab *fs;
	int type;
	char **qfnamep;
{
	register char *opt;
	char *cp;
	static char initname, usrname[100], grpname[100];
	static char buf[BUFSIZ];

	if (!initname) {
		sprintf(usrname, "%s%s", qfextension[USRQUOTA], qfname);
		sprintf(grpname, "%s%s", qfextension[GRPQUOTA], qfname);
		initname = 1;
	}
	strcpy(buf, fs->fs_mntops);
	for (opt = strtok(buf, ","); opt; opt = strtok(NULL, ",")) {
		if ((cp = index(opt, '=')))
			*cp++ = '\0';
		if (type == USRQUOTA && strcmp(opt, usrname) == 0)
			break;
		if (type == GRPQUOTA && strcmp(opt, grpname) == 0)
			break;
	}
	if (!opt)
		return (0);
	if (cp) {
		*qfnamep = cp;
		return (1);
	}
	(void) sprintf(buf, "%s/%s.%s", fs->fs_file, qfname, qfextension[type]);
	*qfnamep = buf;
	return (1);
}

/*
 * Verify file system is mounted and not readonly.
 */
int
readonly(fs)
	register struct fstab *fs;
{
	struct statfs fsbuf;

	if (statfs(fs->fs_file, &fsbuf) < 0 ||
	    strcmp(fsbuf.f_mntonname, fs->fs_file) ||
	    strcmp(fsbuf.f_mntfromname, fs->fs_spec)) {
		printf("%s: not mounted\n", fs->fs_file);
		return (1);
	}
	if (fsbuf.f_flags & MNT_RDONLY) {
		printf("%s: mounted read-only\n", fs->fs_file);
		return (1);
	}
	return (0);
}
