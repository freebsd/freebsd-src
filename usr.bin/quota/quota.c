/*
 * Copyright (c) 1980, 1990 Regents of the University of California.
 * All rights reserved.
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
char copyright[] =
"@(#) Copyright (c) 1980, 1990 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)quota.c	5.12 (Berkeley) 9/27/90";
#endif /* not lint */

/*
 * Disk quota reporting program.
 */
#include <sys/param.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <ufs/quota.h>
#include <stdio.h>
#include <fstab.h>
#include <ctype.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>

char *qfname = QUOTAFILENAME;
char *qfextension[] = INITQFNAMES;

struct quotause {
	struct	quotause *next;
	long	flags;
	struct	dqblk dqblk;
	char	fsname[MAXPATHLEN + 1];
} *getprivs();
#define	FOUND	0x01

int	qflag;
int	vflag;

main(argc, argv)
	char *argv[];
{
	int ngroups, gidset[NGROUPS];
	int i, gflag = 0, uflag = 0;
	char ch;
	extern char *optarg;
	extern int optind, errno;

	if (quotactl("/", 0, 0, (caddr_t)0) < 0 && errno == EOPNOTSUPP) {
		fprintf(stderr, "There are no quotas on this system\n");
		exit(0);
	}
	while ((ch = getopt(argc, argv, "ugvq")) != EOF) {
		switch(ch) {
		case 'g':
			gflag++;
			break;
		case 'u':
			uflag++;
			break;
		case 'v':
			vflag++;
			break;
		case 'q':
			qflag++;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (!uflag && !gflag)
		uflag++;
	if (argc == 0) {
		if (uflag)
			showuid(getuid());
		if (gflag) {
			ngroups = getgroups(NGROUPS, gidset);
			if (ngroups < 0) {
				perror("quota: getgroups");
				exit(1);
			}
			for (i = 1; i < ngroups; i++)
				showgid(gidset[i]);
		}
		exit(0);
	}
	if (uflag && gflag)
		usage();
	if (uflag) {
		for (; argc > 0; argc--, argv++) {
			if (alldigits(*argv))
				showuid(atoi(*argv));
			else
				showusrname(*argv);
		}
		exit(0);
	}
	if (gflag) {
		for (; argc > 0; argc--, argv++) {
			if (alldigits(*argv))
				showgid(atoi(*argv));
			else
				showgrpname(*argv);
		}
		exit(0);
	}
}

usage()
{

	fprintf(stderr, "%s\n%s\n%s\n",
		"Usage: quota [-guqv]",
		"\tquota [-qv] -u username ...",
		"\tquota [-qv] -g groupname ...");
	exit(1);
}

/*
 * Print out quotas for a specified user identifier.
 */
showuid(uid)
	u_long uid;
{
	struct passwd *pwd = getpwuid(uid);
	u_long myuid;
	char *name;

	if (pwd == NULL)
		name = "(no account)";
	else
		name = pwd->pw_name;
	myuid = getuid();
	if (uid != myuid && myuid != 0) {
		printf("quota: %s (uid %d): permission denied\n", name, uid);
		return;
	}
	showquotas(USRQUOTA, uid, name);
}

/*
 * Print out quotas for a specifed user name.
 */
showusrname(name)
	char *name;
{
	struct passwd *pwd = getpwnam(name);
	u_long myuid;

	if (pwd == NULL) {
		fprintf(stderr, "quota: %s: unknown user\n", name);
		return;
	}
	myuid = getuid();
	if (pwd->pw_uid != myuid && myuid != 0) {
		fprintf(stderr, "quota: %s (uid %d): permission denied\n",
		    name, pwd->pw_uid);
		return;
	}
	showquotas(USRQUOTA, pwd->pw_uid, name);
}

/*
 * Print out quotas for a specified group identifier.
 */
showgid(gid)
	u_long gid;
{
	struct group *grp = getgrgid(gid);
	int ngroups, gidset[NGROUPS];
	register int i;
	char *name;

	if (grp == NULL)
		name = "(no entry)";
	else
		name = grp->gr_name;
	ngroups = getgroups(NGROUPS, gidset);
	if (ngroups < 0) {
		perror("quota: getgroups");
		return;
	}
	for (i = 1; i < ngroups; i++)
		if (gid == gidset[i])
			break;
	if (i >= ngroups && getuid() != 0) {
		fprintf(stderr, "quota: %s (gid %d): permission denied\n",
		    name, gid);
		return;
	}
	showquotas(GRPQUOTA, gid, name);
}

/*
 * Print out quotas for a specifed group name.
 */
showgrpname(name)
	char *name;
{
	struct group *grp = getgrnam(name);
	int ngroups, gidset[NGROUPS];
	register int i;

	if (grp == NULL) {
		fprintf(stderr, "quota: %s: unknown group\n", name);
		return;
	}
	ngroups = getgroups(NGROUPS, gidset);
	if (ngroups < 0) {
		perror("quota: getgroups");
		return;
	}
	for (i = 1; i < ngroups; i++)
		if (grp->gr_gid == gidset[i])
			break;
	if (i >= ngroups && getuid() != 0) {
		fprintf(stderr, "quota: %s (gid %d): permission denied\n",
		    name, grp->gr_gid);
		return;
	}
	showquotas(GRPQUOTA, grp->gr_gid, name);
}

showquotas(type, id, name)
	int type;
	u_long id;
	char *name;
{
	register struct quotause *qup;
	struct quotause *quplist, *getprivs();
	char *msgi, *msgb, *timeprt();
	int myuid, fd, lines = 0;
	static int first;
	static time_t now;

	if (now == 0)
		time(&now);
	quplist = getprivs(id, type);
	for (qup = quplist; qup; qup = qup->next) {
		if (!vflag &&
		    qup->dqblk.dqb_isoftlimit == 0 &&
		    qup->dqblk.dqb_ihardlimit == 0 &&
		    qup->dqblk.dqb_bsoftlimit == 0 &&
		    qup->dqblk.dqb_bhardlimit == 0)
			continue;
		msgi = (char *)0;
		if (qup->dqblk.dqb_ihardlimit &&
		    qup->dqblk.dqb_curinodes >= qup->dqblk.dqb_ihardlimit)
			msgi = "File limit reached on";
		else if (qup->dqblk.dqb_isoftlimit &&
		    qup->dqblk.dqb_curinodes >= qup->dqblk.dqb_isoftlimit)
			if (qup->dqblk.dqb_itime > now)
				msgi = "In file grace period on";
			else
				msgi = "Over file quota on";
		msgb = (char *)0;
		if (qup->dqblk.dqb_bhardlimit &&
		    qup->dqblk.dqb_curblocks >= qup->dqblk.dqb_bhardlimit)
			msgb = "Block limit reached on";
		else if (qup->dqblk.dqb_bsoftlimit &&
		    qup->dqblk.dqb_curblocks >= qup->dqblk.dqb_bsoftlimit)
			if (qup->dqblk.dqb_btime > now)
				msgb = "In block grace period on";
			else
				msgb = "Over block quota on";
		if (qflag) {
			if ((msgi != (char *)0 || msgb != (char *)0) &&
			    lines++ == 0)
				heading(type, id, name, "");
			if (msgi != (char *)0)
				printf("\t%s %s\n", msgi, qup->fsname);
			if (msgb != (char *)0)
				printf("\t%s %s\n", msgb, qup->fsname);
			continue;
		}
		if (vflag ||
		    qup->dqblk.dqb_curblocks ||
		    qup->dqblk.dqb_curinodes) {
			if (lines++ == 0)
				heading(type, id, name, "");
			printf("%15s%8d%c%7d%8d%8s"
				, qup->fsname
				, dbtob(qup->dqblk.dqb_curblocks) / 1024
				, (msgb == (char *)0) ? ' ' : '*'
				, dbtob(qup->dqblk.dqb_bsoftlimit) / 1024
				, dbtob(qup->dqblk.dqb_bhardlimit) / 1024
				, (msgb == (char *)0) ? ""
				    : timeprt(qup->dqblk.dqb_btime));
			printf("%8d%c%7d%8d%8s\n"
				, qup->dqblk.dqb_curinodes
				, (msgi == (char *)0) ? ' ' : '*'
				, qup->dqblk.dqb_isoftlimit
				, qup->dqblk.dqb_ihardlimit
				, (msgi == (char *)0) ? ""
				    : timeprt(qup->dqblk.dqb_itime)
			);
			continue;
		}
	}
	if (!qflag && lines == 0)
		heading(type, id, name, "none");
}

heading(type, id, name, tag)
	int type;
	u_long id;
	char *name, *tag;
{

	printf("Disk quotas for %s %s (%cid %d): %s\n", qfextension[type],
	    name, *qfextension[type], id, tag);
	if (!qflag && tag[0] == '\0') {
		printf("%15s%8s %7s%8s%8s%8s %7s%8s%8s\n"
			, "Filesystem"
			, "blocks"
			, "quota"
			, "limit"
			, "grace"
			, "files"
			, "quota"
			, "limit"
			, "grace"
		);
	}
}

/*
 * Calculate the grace period and return a printable string for it.
 */
char *
timeprt(seconds)
	time_t seconds;
{
	time_t hours, minutes;
	static char buf[20];
	static time_t now;

	if (now == 0)
		time(&now);
	if (now > seconds)
		return ("none");
	seconds -= now;
	minutes = (seconds + 30) / 60;
	hours = (minutes + 30) / 60;
	if (hours >= 36) {
		sprintf(buf, "%ddays", (hours + 12) / 24);
		return (buf);
	}
	if (minutes >= 60) {
		sprintf(buf, "%2d:%d", minutes / 60, minutes % 60);
		return (buf);
	}
	sprintf(buf, "%2d", minutes);
	return (buf);
}

/*
 * Collect the requested quota information.
 */
struct quotause *
getprivs(id, quotatype)
	register long id;
	int quotatype;
{
	register struct fstab *fs;
	register struct quotause *qup, *quptail;
	struct quotause *quphead;
	char *qfpathname;
	int qcmd, fd;

	setfsent();
	quphead = (struct quotause *)0;
	qcmd = QCMD(Q_GETQUOTA, quotatype);
	while (fs = getfsent()) {
		if (strcmp(fs->fs_vfstype, "ufs"))
			continue;
		if (!hasquota(fs, quotatype, &qfpathname))
			continue;
		if ((qup = (struct quotause *)malloc(sizeof *qup)) == NULL) {
			fprintf(stderr, "quota: out of memory\n");
			exit(2);
		}
		if (quotactl(fs->fs_file, qcmd, id, &qup->dqblk) != 0) {
			if ((fd = open(qfpathname, O_RDONLY)) < 0) {
				perror(qfpathname);
				free(qup);
				continue;
			}
			lseek(fd, (long)(id * sizeof(struct dqblk)), L_SET);
			switch (read(fd, &qup->dqblk, sizeof(struct dqblk))) {
			case 0:			/* EOF */
				/*
				 * Convert implicit 0 quota (EOF)
				 * into an explicit one (zero'ed dqblk)
				 */
				bzero((caddr_t)&qup->dqblk,
				    sizeof(struct dqblk));
				break;

			case sizeof(struct dqblk):	/* OK */
				break;

			default:		/* ERROR */
				fprintf(stderr, "quota: read error");
				perror(qfpathname);
				close(fd);
				free(qup);
				continue;
			}
			close(fd);
		}
		strcpy(qup->fsname, fs->fs_file);
		if (quphead == NULL)
			quphead = qup;
		else
			quptail->next = qup;
		quptail = qup;
		qup->next = 0;
	}
	endfsent();
	return (quphead);
}

/*
 * Check to see if a particular quota is to be enabled.
 */
hasquota(fs, type, qfnamep)
	register struct fstab *fs;
	int type;
	char **qfnamep;
{
	register char *opt;
	char *cp, *index(), *strtok();
	static char initname, usrname[100], grpname[100];
	static char buf[BUFSIZ];

	if (!initname) {
		sprintf(usrname, "%s%s", qfextension[USRQUOTA], qfname);
		sprintf(grpname, "%s%s", qfextension[GRPQUOTA], qfname);
		initname = 1;
	}
	strcpy(buf, fs->fs_mntops);
	for (opt = strtok(buf, ","); opt; opt = strtok(NULL, ",")) {
		if (cp = index(opt, '='))
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

alldigits(s)
	register char *s;
{
	register c;

	c = *s++;
	do {
		if (!isdigit(c))
			return (0);
	} while (c = *s++);
	return (1);
}
