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
static char sccsid[] = "@(#)edquota.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

/*
 * Disk quota editor.
 */
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <ufs/ufs/quota.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fstab.h>
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "pathnames.h"

const char *qfname = QUOTAFILENAME;
const char *qfextension[] = INITQFNAMES;
const char *quotagroup = QUOTAGROUP;
char tmpfil[] = _PATH_TMP;

struct quotause {
	struct	quotause *next;
	long	flags;
	struct	dqblk dqblk;
	char	fsname[MAXPATHLEN + 1];
	char	qfname[1];	/* actually longer */
};
#define	FOUND	0x01

int alldigits(const char *s);
int cvtatos(time_t, char *, time_t *);
char *cvtstoa(time_t);
int editit(char *);
void freeprivs(struct quotause *);
int getentry(const char *, int);
struct quotause *getprivs(long, int, char *);
int hasquota(struct fstab *, int, char **);
void putprivs(long, int, struct quotause *);
int readprivs(struct quotause *, char *);
int readtimes(struct quotause *, char *);
static void usage(void);
int writetimes(struct quotause *, int, int);
int writeprivs(struct quotause *, int, char *, int);

int
main(int argc, char **argv)
{
	struct quotause *qup, *protoprivs, *curprivs;
	long id, protoid;
	long long lim;
	int i, quotatype, range, tmpfd;
	uid_t startuid, enduid;
	u_int32_t *limp;
	char *protoname, *cp, *oldoptarg, ch;
	int eflag = 0, tflag = 0, pflag = 0;
	char *fspath = NULL;
	char buf[30];

	if (argc < 2)
		usage();
	if (getuid())
		errx(1, "permission denied");
	quotatype = USRQUOTA;
	protoprivs = NULL;
	while ((ch = getopt(argc, argv, "ugtf:p:e:")) != -1) {
		switch(ch) {
		case 'f':
			fspath = optarg;
			break;
		case 'p':
			protoname = optarg;
			pflag++;
			break;
		case 'g':
			quotatype = GRPQUOTA;
			break;
		case 'u':
			quotatype = USRQUOTA;
			break;
		case 't':
			tflag++;
			break;
		case 'e':
			if ((qup = malloc(sizeof(*qup))) == NULL)
				errx(2, "out of memory");
			bzero(qup, sizeof(*qup));
			i = 0;
			oldoptarg = optarg;
			for (cp = optarg; (cp = strsep(&optarg, ":")) != NULL;
			    i++) {
				if (cp != oldoptarg)
					*(cp - 1) = ':';
				limp = NULL;
				switch (i) {
				case 0:
					strlcpy(qup->fsname, cp,
					    sizeof(qup->fsname));
					break;
				case 1:
					limp = &qup->dqblk.dqb_bsoftlimit;
					break;
				case 2:
					limp = &qup->dqblk.dqb_bhardlimit;
					break;
				case 3:
					limp = &qup->dqblk.dqb_isoftlimit;
					break;
				case 4:
					limp = &qup->dqblk.dqb_ihardlimit;
					break;
				default:
					warnx("incorrect quota specification: "
					    "%s", oldoptarg);
					usage();
					break; /* XXX: report an error */
				}
				if (limp != NULL) {
					lim = strtoll(cp, NULL, 10);
					if (lim < 0 || lim > UINT_MAX)
						errx(1, "invalid limit value: "
						    "%lld", lim);
					*limp = (u_int32_t)lim;
				}
			}
			qup->dqblk.dqb_bsoftlimit =
			    btodb((off_t)qup->dqblk.dqb_bsoftlimit * 1024);
			qup->dqblk.dqb_bhardlimit =
			    btodb((off_t)qup->dqblk.dqb_bhardlimit * 1024);
			if (protoprivs == NULL) {
				protoprivs = curprivs = qup;
			} else {
				curprivs->next = qup;
				curprivs = qup;
			}
			eflag++;
			pflag++;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (pflag) {
		if (protoprivs == NULL) {
			if ((protoid = getentry(protoname, quotatype)) == -1)
				exit(1);
			protoprivs = getprivs(protoid, quotatype, fspath);
			for (qup = protoprivs; qup; qup = qup->next) {
				qup->dqblk.dqb_btime = 0;
				qup->dqblk.dqb_itime = 0;
			}
		}
		for (; argc-- > 0; argv++) {
			if (strspn(*argv, "0123456789-") == strlen(*argv) &&
			    (cp = strchr(*argv, '-')) != NULL) {
				*cp++ = '\0';
				startuid = atoi(*argv);
				enduid = atoi(cp);
				if (enduid < startuid)
					errx(1,
	"ending uid (%d) must be >= starting uid (%d) when using uid ranges",
						enduid, startuid);
				range = 1;
			} else {
				startuid = enduid = 0;
				range = 0;
			}
			for ( ; startuid <= enduid; startuid++) {
				if (range)
					snprintf(buf, sizeof(buf), "%d",
					    startuid);
				else
					snprintf(buf, sizeof(buf), "%s",
						*argv);
				if ((id = getentry(buf, quotatype)) < 0)
					continue;
				if (eflag) {
					for (qup = protoprivs; qup;
					    qup = qup->next) {
						curprivs = getprivs(id,
						    quotatype, qup->fsname);
						if (curprivs == NULL)
							continue;
						strcpy(qup->qfname,
						    curprivs->qfname);
						strcpy(qup->fsname,
						    curprivs->fsname);
					}
				}
				putprivs(id, quotatype, protoprivs);						
			}
		}
		exit(0);
	}
	tmpfd = mkstemp(tmpfil);
	fchown(tmpfd, getuid(), getgid());
	if (tflag) {
		protoprivs = getprivs(0, quotatype, fspath);
		if (writetimes(protoprivs, tmpfd, quotatype) == 0)
			exit(1);
		if (editit(tmpfil) && readtimes(protoprivs, tmpfil))
			putprivs(0, quotatype, protoprivs);
		freeprivs(protoprivs);
		close(tmpfd);
		unlink(tmpfil);
		exit(0);
	}
	for ( ; argc > 0; argc--, argv++) {
		if ((id = getentry(*argv, quotatype)) == -1)
			continue;
		curprivs = getprivs(id, quotatype, fspath);
		if (writeprivs(curprivs, tmpfd, *argv, quotatype) == 0)
			continue;
		if (editit(tmpfil) && readprivs(curprivs, tmpfil))
			putprivs(id, quotatype, curprivs);
		freeprivs(curprivs);
	}
	close(tmpfd);
	unlink(tmpfil);
	exit(0);
}

static void
usage()
{
	fprintf(stderr, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
		"usage: edquota [-u] [-f fspath] [-p username] username ...",
		"       edquota [-u] -e fspath[:bslim[:bhlim[:islim[:ihlim]]]] [-e ...]",
		"               username ...",
		"       edquota -g [-f fspath] [-p groupname] groupname ...",
		"       edquota -g -e fspath[:bslim[:bhlim[:islim[:ihlim]]]] [-e ...]",
		"               groupname ...",
		"       edquota [-u] -t [-f fspath]",
		"       edquota -g -t [-f fspath]");
	exit(1);
}

/*
 * This routine converts a name for a particular quota type to
 * an identifier. This routine must agree with the kernel routine
 * getinoquota as to the interpretation of quota types.
 */
int
getentry(name, quotatype)
	const char *name;
	int quotatype;
{
	struct passwd *pw;
	struct group *gr;

	if (alldigits(name))
		return (atoi(name));
	switch(quotatype) {
	case USRQUOTA:
		if ((pw = getpwnam(name)))
			return (pw->pw_uid);
		warnx("%s: no such user", name);
		break;
	case GRPQUOTA:
		if ((gr = getgrnam(name)))
			return (gr->gr_gid);
		warnx("%s: no such group", name);
		break;
	default:
		warnx("%d: unknown quota type", quotatype);
		break;
	}
	sleep(1);
	return (-1);
}

/*
 * Collect the requested quota information.
 */
struct quotause *
getprivs(id, quotatype, fspath)
	register long id;
	int quotatype;
	char *fspath;
{
	register struct fstab *fs;
	register struct quotause *qup, *quptail;
	struct quotause *quphead;
	int qcmd, qupsize, fd;
	char *qfpathname;
	static int warned = 0;

	setfsent();
	quphead = (struct quotause *)0;
	qcmd = QCMD(Q_GETQUOTA, quotatype);
	while ((fs = getfsent())) {
		if (fspath && *fspath && strcmp(fspath, fs->fs_spec) &&
		    strcmp(fspath, fs->fs_file))
			continue;
		if (strcmp(fs->fs_vfstype, "ufs"))
			continue;
		if (!hasquota(fs, quotatype, &qfpathname))
			continue;
		qupsize = sizeof(*qup) + strlen(qfpathname);
		if ((qup = (struct quotause *)malloc(qupsize)) == NULL)
			errx(2, "out of memory");
		if (quotactl(fs->fs_file, qcmd, id, &qup->dqblk) != 0) {
	    		if (errno == EOPNOTSUPP && !warned) {
				warned++;
		warnx("warning: quotas are not compiled into this kernel");
				sleep(3);
			}
			if ((fd = open(qfpathname, O_RDONLY)) < 0) {
				fd = open(qfpathname, O_RDWR|O_CREAT, 0640);
				if (fd < 0 && errno != ENOENT) {
					warn("%s", qfpathname);
					free(qup);
					continue;
				}
				warnx("creating quota file %s", qfpathname);
				sleep(3);
				(void) fchown(fd, getuid(),
				    getentry(quotagroup, GRPQUOTA));
				(void) fchmod(fd, 0640);
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
				warn("read error in %s", qfpathname);
				close(fd);
				free(qup);
				continue;
			}
			close(fd);
		}
		strcpy(qup->qfname, qfpathname);
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
 * Store the requested quota information.
 */
void
putprivs(id, quotatype, quplist)
	long id;
	int quotatype;
	struct quotause *quplist;
{
	register struct quotause *qup;
	int qcmd, fd;

	qcmd = QCMD(Q_SETQUOTA, quotatype);
	for (qup = quplist; qup; qup = qup->next) {
		if (quotactl(qup->fsname, qcmd, id, &qup->dqblk) == 0)
			continue;
		if ((fd = open(qup->qfname, O_WRONLY)) < 0) {
			warn("%s", qup->qfname);
		} else {
			lseek(fd, (long)id * (long)sizeof (struct dqblk), 0);
			if (write(fd, &qup->dqblk, sizeof (struct dqblk)) !=
			    sizeof (struct dqblk)) {
				warn("%s", qup->qfname);
			}
			close(fd);
		}
	}
}

/*
 * Take a list of priviledges and get it edited.
 */
int
editit(tmpf)
	char *tmpf;
{
	long omask;
	int pid, status;

	omask = sigblock(sigmask(SIGINT)|sigmask(SIGQUIT)|sigmask(SIGHUP));
 top:
	if ((pid = fork()) < 0) {

		if (errno == EPROCLIM) {
			warnx("you have too many processes");
			return(0);
		}
		if (errno == EAGAIN) {
			sleep(1);
			goto top;
		}
		warn("fork");
		return (0);
	}
	if (pid == 0) {
		register const char *ed;

		sigsetmask(omask);
		setgid(getgid());
		setuid(getuid());
		if ((ed = getenv("EDITOR")) == (char *)0)
			ed = _PATH_VI;
		execlp(ed, ed, tmpf, (char *)0);
		err(1, "%s", ed);
	}
	waitpid(pid, &status, 0);
	sigsetmask(omask);
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		return (0);
	return (1);
}

/*
 * Convert a quotause list to an ASCII file.
 */
int
writeprivs(quplist, outfd, name, quotatype)
	struct quotause *quplist;
	int outfd;
	char *name;
	int quotatype;
{
	register struct quotause *qup;
	FILE *fd;

	ftruncate(outfd, 0);
	lseek(outfd, 0, L_SET);
	if ((fd = fdopen(dup(outfd), "w")) == NULL)
		err(1, "%s", tmpfil);
	fprintf(fd, "Quotas for %s %s:\n", qfextension[quotatype], name);
	for (qup = quplist; qup; qup = qup->next) {
		fprintf(fd, "%s: %s %lu, limits (soft = %lu, hard = %lu)\n",
		    qup->fsname, "kbytes in use:",
		    (unsigned long)(dbtob(qup->dqblk.dqb_curblocks) / 1024),
		    (unsigned long)(dbtob(qup->dqblk.dqb_bsoftlimit) / 1024),
		    (unsigned long)(dbtob(qup->dqblk.dqb_bhardlimit) / 1024));
		fprintf(fd, "%s %lu, limits (soft = %lu, hard = %lu)\n",
		    "\tinodes in use:",
		    (unsigned long)qup->dqblk.dqb_curinodes,
		    (unsigned long)qup->dqblk.dqb_isoftlimit,
		    (unsigned long)qup->dqblk.dqb_ihardlimit);
	}
	fclose(fd);
	return (1);
}

/*
 * Merge changes to an ASCII file into a quotause list.
 */
int
readprivs(quplist, inname)
	struct quotause *quplist;
	char *inname;
{
	register struct quotause *qup;
	FILE *fd;
	unsigned long bhardlimit, bsoftlimit, curblocks;
	unsigned long ihardlimit, isoftlimit, curinodes;
	int cnt;
	register char *cp;
	struct dqblk dqblk;
	char *fsp, line1[BUFSIZ], line2[BUFSIZ];

	fd = fopen(inname, "r");
	if (fd == NULL) {
		warnx("can't re-read temp file!!");
		return (0);
	}
	/*
	 * Discard title line, then read pairs of lines to process.
	 */
	(void) fgets(line1, sizeof (line1), fd);
	while (fgets(line1, sizeof (line1), fd) != NULL &&
	       fgets(line2, sizeof (line2), fd) != NULL) {
		if ((fsp = strtok(line1, " \t:")) == NULL) {
			warnx("%s: bad format", line1);
			return (0);
		}
		if ((cp = strtok((char *)0, "\n")) == NULL) {
			warnx("%s: %s: bad format", fsp, &fsp[strlen(fsp) + 1]);
			return (0);
		}
		cnt = sscanf(cp,
		    " kbytes in use: %lu, limits (soft = %lu, hard = %lu)",
		    &curblocks, &bsoftlimit, &bhardlimit);
		if (cnt != 3) {
			warnx("%s:%s: bad format", fsp, cp);
			return (0);
		}
		dqblk.dqb_curblocks = btodb((off_t)curblocks * 1024);
		dqblk.dqb_bsoftlimit = btodb((off_t)bsoftlimit * 1024);
		dqblk.dqb_bhardlimit = btodb((off_t)bhardlimit * 1024);
		if ((cp = strtok(line2, "\n")) == NULL) {
			warnx("%s: %s: bad format", fsp, line2);
			return (0);
		}
		cnt = sscanf(cp,
		    "\tinodes in use: %lu, limits (soft = %lu, hard = %lu)",
		    &curinodes, &isoftlimit, &ihardlimit);
		if (cnt != 3) {
			warnx("%s: %s: bad format", fsp, line2);
			return (0);
		}
		dqblk.dqb_curinodes = curinodes;
		dqblk.dqb_isoftlimit = isoftlimit;
		dqblk.dqb_ihardlimit = ihardlimit;
		for (qup = quplist; qup; qup = qup->next) {
			if (strcmp(fsp, qup->fsname))
				continue;
			/*
			 * Cause time limit to be reset when the quota
			 * is next used if previously had no soft limit
			 * or were under it, but now have a soft limit
			 * and are over it.
			 */
			if (dqblk.dqb_bsoftlimit &&
			    qup->dqblk.dqb_curblocks >= dqblk.dqb_bsoftlimit &&
			    (qup->dqblk.dqb_bsoftlimit == 0 ||
			     qup->dqblk.dqb_curblocks <
			     qup->dqblk.dqb_bsoftlimit))
				qup->dqblk.dqb_btime = 0;
			if (dqblk.dqb_isoftlimit &&
			    qup->dqblk.dqb_curinodes >= dqblk.dqb_isoftlimit &&
			    (qup->dqblk.dqb_isoftlimit == 0 ||
			     qup->dqblk.dqb_curinodes <
			     qup->dqblk.dqb_isoftlimit))
				qup->dqblk.dqb_itime = 0;
			qup->dqblk.dqb_bsoftlimit = dqblk.dqb_bsoftlimit;
			qup->dqblk.dqb_bhardlimit = dqblk.dqb_bhardlimit;
			qup->dqblk.dqb_isoftlimit = dqblk.dqb_isoftlimit;
			qup->dqblk.dqb_ihardlimit = dqblk.dqb_ihardlimit;
			qup->flags |= FOUND;
			if (dqblk.dqb_curblocks == qup->dqblk.dqb_curblocks &&
			    dqblk.dqb_curinodes == qup->dqblk.dqb_curinodes)
				break;
			warnx("%s: cannot change current allocation", fsp);
			break;
		}
	}
	fclose(fd);
	/*
	 * Disable quotas for any filesystems that have not been found.
	 */
	for (qup = quplist; qup; qup = qup->next) {
		if (qup->flags & FOUND) {
			qup->flags &= ~FOUND;
			continue;
		}
		qup->dqblk.dqb_bsoftlimit = 0;
		qup->dqblk.dqb_bhardlimit = 0;
		qup->dqblk.dqb_isoftlimit = 0;
		qup->dqblk.dqb_ihardlimit = 0;
	}
	return (1);
}

/*
 * Convert a quotause list to an ASCII file of grace times.
 */
int
writetimes(quplist, outfd, quotatype)
	struct quotause *quplist;
	int outfd;
	int quotatype;
{
	register struct quotause *qup;
	FILE *fd;

	ftruncate(outfd, 0);
	lseek(outfd, 0, L_SET);
	if ((fd = fdopen(dup(outfd), "w")) == NULL)
		err(1, "%s", tmpfil);
	fprintf(fd, "Time units may be: days, hours, minutes, or seconds\n");
	fprintf(fd, "Grace period before enforcing soft limits for %ss:\n",
	    qfextension[quotatype]);
	for (qup = quplist; qup; qup = qup->next) {
		fprintf(fd, "%s: block grace period: %s, ",
		    qup->fsname, cvtstoa(qup->dqblk.dqb_btime));
		fprintf(fd, "file grace period: %s\n",
		    cvtstoa(qup->dqblk.dqb_itime));
	}
	fclose(fd);
	return (1);
}

/*
 * Merge changes of grace times in an ASCII file into a quotause list.
 */
int
readtimes(quplist, inname)
	struct quotause *quplist;
	char *inname;
{
	register struct quotause *qup;
	FILE *fd;
	int cnt;
	register char *cp;
	time_t itime, btime, iseconds, bseconds;
	long l_itime, l_btime;
	char *fsp, bunits[10], iunits[10], line1[BUFSIZ];

	fd = fopen(inname, "r");
	if (fd == NULL) {
		warnx("can't re-read temp file!!");
		return (0);
	}
	/*
	 * Discard two title lines, then read lines to process.
	 */
	(void) fgets(line1, sizeof (line1), fd);
	(void) fgets(line1, sizeof (line1), fd);
	while (fgets(line1, sizeof (line1), fd) != NULL) {
		if ((fsp = strtok(line1, " \t:")) == NULL) {
			warnx("%s: bad format", line1);
			return (0);
		}
		if ((cp = strtok((char *)0, "\n")) == NULL) {
			warnx("%s: %s: bad format", fsp, &fsp[strlen(fsp) + 1]);
			return (0);
		}
		cnt = sscanf(cp,
		    " block grace period: %ld %s file grace period: %ld %s",
		    &l_btime, bunits, &l_itime, iunits);
		if (cnt != 4) {
			warnx("%s:%s: bad format", fsp, cp);
			return (0);
		}
		btime = l_btime;
		itime = l_itime;
		if (cvtatos(btime, bunits, &bseconds) == 0)
			return (0);
		if (cvtatos(itime, iunits, &iseconds) == 0)
			return (0);
		for (qup = quplist; qup; qup = qup->next) {
			if (strcmp(fsp, qup->fsname))
				continue;
			qup->dqblk.dqb_btime = bseconds;
			qup->dqblk.dqb_itime = iseconds;
			qup->flags |= FOUND;
			break;
		}
	}
	fclose(fd);
	/*
	 * reset default grace periods for any filesystems
	 * that have not been found.
	 */
	for (qup = quplist; qup; qup = qup->next) {
		if (qup->flags & FOUND) {
			qup->flags &= ~FOUND;
			continue;
		}
		qup->dqblk.dqb_btime = 0;
		qup->dqblk.dqb_itime = 0;
	}
	return (1);
}

/*
 * Convert seconds to ASCII times.
 */
char *
cvtstoa(secs)
	time_t secs;
{
	static char buf[20];

	if (secs % (24 * 60 * 60) == 0) {
		secs /= 24 * 60 * 60;
		sprintf(buf, "%ld day%s", (long)secs, secs == 1 ? "" : "s");
	} else if (secs % (60 * 60) == 0) {
		secs /= 60 * 60;
		sprintf(buf, "%ld hour%s", (long)secs, secs == 1 ? "" : "s");
	} else if (secs % 60 == 0) {
		secs /= 60;
		sprintf(buf, "%ld minute%s", (long)secs, secs == 1 ? "" : "s");
	} else
		sprintf(buf, "%ld second%s", (long)secs, secs == 1 ? "" : "s");
	return (buf);
}

/*
 * Convert ASCII input times to seconds.
 */
int
cvtatos(period, units, seconds)
	time_t period;
	char *units;
	time_t *seconds;
{

	if (bcmp(units, "second", 6) == 0)
		*seconds = period;
	else if (bcmp(units, "minute", 6) == 0)
		*seconds = period * 60;
	else if (bcmp(units, "hour", 4) == 0)
		*seconds = period * 60 * 60;
	else if (bcmp(units, "day", 3) == 0)
		*seconds = period * 24 * 60 * 60;
	else {
		printf("%s: bad units, specify %s\n", units,
		    "days, hours, minutes, or seconds");
		return (0);
	}
	return (1);
}

/*
 * Free a list of quotause structures.
 */
void
freeprivs(quplist)
	struct quotause *quplist;
{
	register struct quotause *qup, *nextqup;

	for (qup = quplist; qup; qup = nextqup) {
		nextqup = qup->next;
		free(qup);
	}
}

/*
 * Check whether a string is completely composed of digits.
 */
int
alldigits(s)
	register const char *s;
{
	register int c;

	c = *s++;
	do {
		if (!isdigit(c))
			return (0);
	} while ((c = *s++));
	return (1);
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
