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
#endif

#ifndef lint
static const char sccsid[] = "from: @(#)quota.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

/*
 * Disk quota reporting program.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/socket.h>

#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#include <rpcsvc/rquota.h>

#include <ufs/ufs/quota.h>

#include <ctype.h>
#include <err.h>
#include <fstab.h>
#include <grp.h>
#include <netdb.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

const char *qfname = QUOTAFILENAME;
const char *qfextension[] = INITQFNAMES;

struct quotause {
	struct	quotause *next;
	long	flags;
	struct	dqblk dqblk;
	char	fsname[MAXPATHLEN + 1];
};

static const char *timeprt(time_t seconds);
static struct quotause *getprivs(long id, int quotatype);
static void usage(void);
static void showuid(u_long uid);
static void showgid(u_long gid);
static void showusrname(char *name);
static void showgrpname(char *name);
static void showquotas(int type, u_long id, const char *name);
static void heading(int type, u_long id, const char *name, const char *tag);
static int ufshasquota(struct fstab *fs, int type, char **qfnamep);
static int getufsquota(struct fstab *fs, struct quotause *qup, long id,
	int quotatype);
static int getnfsquota(struct statfs *fst, struct quotause *qup, long id,
	int quotatype);
static int callaurpc(char *host, int prognum, int versnum, int procnum, 
	xdrproc_t inproc, char *in, xdrproc_t outproc, char *out);
static int alldigits(char *s);

int	lflag;
int	qflag;
int	vflag;

int
main(int argc, char *argv[])
{
	int ngroups; 
	gid_t mygid, gidset[NGROUPS];
	int i, ch, gflag = 0, uflag = 0;

	while ((ch = getopt(argc, argv, "glquv")) != -1) {
		switch(ch) {
		case 'g':
			gflag++;
			break;
		case 'l':
			lflag++;
			break;
		case 'q':
			qflag++;
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
	if (!uflag && !gflag)
		uflag++;
	if (argc == 0) {
		if (uflag)
			showuid(getuid());
		if (gflag) {
			mygid = getgid();
			ngroups = getgroups(NGROUPS, gidset);
			if (ngroups < 0)
				err(1, "getgroups");
			showgid(mygid);
			for (i = 0; i < ngroups; i++)
				if (gidset[i] != mygid)
					showgid(gidset[i]);
		}
		return(0);
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
		return(0);
	}
	if (gflag) {
		for (; argc > 0; argc--, argv++) {
			if (alldigits(*argv))
				showgid(atoi(*argv));
			else
				showgrpname(*argv);
		}
	}
	return(0);
}

static void
usage(void)
{

	fprintf(stderr, "%s\n%s\n%s\n",
	    "usage: quota [-glu] [-v | -q]",
	    "       quota [-lu] [-v | -q] user ...",
	    "       quota -g [-l] [-v | -q] group ...");
	exit(1);
}

/*
 * Print out quotas for a specified user identifier.
 */
static void
showuid(u_long uid)
{
	struct passwd *pwd = getpwuid(uid);
	const char *name;

	if (pwd == NULL)
		name = "(no account)";
	else
		name = pwd->pw_name;
	showquotas(USRQUOTA, uid, name);
}

/*
 * Print out quotas for a specifed user name.
 */
static void
showusrname(char *name)
{
	struct passwd *pwd = getpwnam(name);

	if (pwd == NULL) {
		warnx("%s: unknown user", name);
		return;
	}
	showquotas(USRQUOTA, pwd->pw_uid, name);
}

/*
 * Print out quotas for a specified group identifier.
 */
static void
showgid(u_long gid)
{
	struct group *grp = getgrgid(gid);
	const char *name;

	if (grp == NULL)
		name = "(no entry)";
	else
		name = grp->gr_name;
	showquotas(GRPQUOTA, gid, name);
}

/*
 * Print out quotas for a specifed group name.
 */
static void
showgrpname(char *name)
{
	struct group *grp = getgrnam(name);

	if (grp == NULL) {
		warnx("%s: unknown group", name);
		return;
	}
	showquotas(GRPQUOTA, grp->gr_gid, name);
}

static void
showquotas(int type, u_long id, const char *name)
{
	struct quotause *qup;
	struct quotause *quplist;
	const char *msgi, *msgb;
	const char *nam;
	int lines = 0;
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
		    qup->dqblk.dqb_curinodes >= qup->dqblk.dqb_isoftlimit) {
			if (qup->dqblk.dqb_itime > now)
				msgi = "In file grace period on";
			else
				msgi = "Over file quota on";
		}
		msgb = (char *)0;
		if (qup->dqblk.dqb_bhardlimit &&
		    qup->dqblk.dqb_curblocks >= qup->dqblk.dqb_bhardlimit)
			msgb = "Block limit reached on";
		else if (qup->dqblk.dqb_bsoftlimit &&
		    qup->dqblk.dqb_curblocks >= qup->dqblk.dqb_bsoftlimit) {
			if (qup->dqblk.dqb_btime > now)
				msgb = "In block grace period on";
			else
				msgb = "Over block quota on";
		}
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
			nam = qup->fsname;
			if (strlen(qup->fsname) > 15) {
				printf("%s\n", qup->fsname);
				nam = "";
			} 
			printf("%15s%8lu%c%7lu%8lu%8s"
				, nam
				, (u_long) (dbtob(qup->dqblk.dqb_curblocks)
					    / 1024)
				, (msgb == (char *)0) ? ' ' : '*'
				, (u_long) (dbtob(qup->dqblk.dqb_bsoftlimit)
					    / 1024)
				, (u_long) (dbtob(qup->dqblk.dqb_bhardlimit)
					    / 1024)
				, (msgb == (char *)0) ? ""
				    :timeprt(qup->dqblk.dqb_btime));
			printf("%8lu%c%7lu%8lu%8s\n"
				, (u_long)qup->dqblk.dqb_curinodes
				, (msgi == (char *)0) ? ' ' : '*'
				, (u_long)qup->dqblk.dqb_isoftlimit
				, (u_long)qup->dqblk.dqb_ihardlimit
				, (msgi == (char *)0) ? ""
				    : timeprt(qup->dqblk.dqb_itime)
			);
			continue;
		}
	}
	if (!qflag && lines == 0)
		heading(type, id, name, "none");
}

static void
heading(int type, u_long id, const char *name, const char *tag)
{

	printf("Disk quotas for %s %s (%cid %lu): %s\n", qfextension[type],
	    name, *qfextension[type], id, tag);
	if (!qflag && tag[0] == '\0') {
		printf("%15s%8s %7s%8s%8s%8s %7s%8s%8s\n"
			, "Filesystem"
			, "usage"
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
static const char *
timeprt(time_t seconds)
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
		sprintf(buf, "%lddays", ((long)hours + 12) / 24);
		return (buf);
	}
	if (minutes >= 60) {
		sprintf(buf, "%2ld:%ld", (long)minutes / 60,
		    (long)minutes % 60);
		return (buf);
	}
	sprintf(buf, "%2ld", (long)minutes);
	return (buf);
}

/*
 * Collect the requested quota information.
 */
static struct quotause *
getprivs(long id, int quotatype)
{
	struct quotause *qup, *quptail = NULL;
	struct fstab *fs;
	struct quotause *quphead;
	struct statfs *fst;
	int nfst, i;

	qup = quphead = (struct quotause *)0;

	nfst = getmntinfo(&fst, MNT_NOWAIT);
	if (nfst == 0)
		errx(2, "no filesystems mounted!");
	setfsent();
	for (i=0; i<nfst; i++) {
		if (qup == NULL) {
			if ((qup = (struct quotause *)malloc(sizeof *qup))
			    == NULL)
				errx(2, "out of memory");
		}
		if (strcmp(fst[i].f_fstypename, "nfs") == 0) {
			if (lflag)
				continue;
			if (getnfsquota(&fst[i], qup, id, quotatype)
			    == 0)
				continue;
		} else if (strcmp(fst[i].f_fstypename, "ufs") == 0) {
			/*
			 * XXX
			 * UFS filesystems must be in /etc/fstab, and must
			 * indicate that they have quotas on (?!) This is quite
			 * unlike SunOS where quotas can be enabled/disabled
			 * on a filesystem independent of /etc/fstab, and it
			 * will still print quotas for them.
			 */
			if ((fs = getfsspec(fst[i].f_mntfromname)) == NULL)
				continue;
			if (getufsquota(fs, qup, id, quotatype) == 0)
				continue;
		} else
			continue;
		strcpy(qup->fsname, fst[i].f_mntonname);
		if (quphead == NULL)
			quphead = qup;
		else
			quptail->next = qup;
		quptail = qup;
		quptail->next = 0;
		qup = NULL;
	}
	if (qup)
		free(qup);
	endfsent();
	return (quphead);
}

/*
 * Check to see if a particular quota is to be enabled.
 */
static int
ufshasquota(struct fstab *fs, int type, char **qfnamep)
{
	static char initname, usrname[100], grpname[100];
	static char buf[BUFSIZ];
	char *opt, *cp;

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

static int
getufsquota(struct fstab *fs, struct quotause *qup, long id, int quotatype)
{
	char *qfpathname;
	int fd, qcmd;

	qcmd = QCMD(Q_GETQUOTA, quotatype);
	if (!ufshasquota(fs, quotatype, &qfpathname))
		return (0);

	if (quotactl(fs->fs_file, qcmd, id, (char *)&qup->dqblk) != 0) {
		if ((fd = open(qfpathname, O_RDONLY)) < 0) {
			warn("%s", qfpathname);
			return (0);
		}
		(void) lseek(fd, (off_t)(id * sizeof(struct dqblk)), L_SET);
		switch (read(fd, &qup->dqblk, sizeof(struct dqblk))) {
		case 0:				/* EOF */
			/*
			 * Convert implicit 0 quota (EOF)
			 * into an explicit one (zero'ed dqblk)
			 */
			bzero((caddr_t)&qup->dqblk, sizeof(struct dqblk));
			break;
		case sizeof(struct dqblk):	/* OK */
			break;
		default:		/* ERROR */
			warn("read error: %s", qfpathname);
			close(fd);
			return (0);
		}
		close(fd);
	}
	return (1);
}

static int
getnfsquota(struct statfs *fst, struct quotause *qup, long id, int quotatype)
{
	struct getquota_args gq_args;
	struct getquota_rslt gq_rslt;
	struct dqblk *dqp = &qup->dqblk;
	struct timeval tv;
	char *cp;

	if (fst->f_flags & MNT_LOCAL)
		return (0);

	/*
	 * rpc.rquotad does not support group quotas
	 */
	if (quotatype != USRQUOTA)
		return (0);

	/*
	 * must be some form of "hostname:/path"
	 */
	cp = strchr(fst->f_mntfromname, ':');
	if (cp == NULL) {
		warnx("cannot find hostname for %s", fst->f_mntfromname);
		return (0);
	}
 
	*cp = '\0';
	if (*(cp+1) != '/') {
		*cp = ':';
		return (0);
	}

	/* Avoid attempting the RPC for special amd(8) filesystems. */
	if (strncmp(fst->f_mntfromname, "pid", 3) == 0 &&
	    strchr(fst->f_mntfromname, '@') != NULL) {
		*cp = ':';
		return (0);
	}

	gq_args.gqa_pathp = cp + 1;
	gq_args.gqa_uid = id;
	if (callaurpc(fst->f_mntfromname, RQUOTAPROG, RQUOTAVERS,
	    RQUOTAPROC_GETQUOTA, (xdrproc_t)xdr_getquota_args, (char *)&gq_args,
	    (xdrproc_t)xdr_getquota_rslt, (char *)&gq_rslt) != 0) {
		*cp = ':';
		return (0);
	}

	switch (gq_rslt.status) {
	case Q_NOQUOTA:
		break;
	case Q_EPERM:
		warnx("quota permission error, host: %s",
			fst->f_mntfromname);
		break;
	case Q_OK:
		gettimeofday(&tv, NULL);
			/* blocks*/
		dqp->dqb_bhardlimit =
		    gq_rslt.getquota_rslt_u.gqr_rquota.rq_bhardlimit *
		    (gq_rslt.getquota_rslt_u.gqr_rquota.rq_bsize / DEV_BSIZE);
		dqp->dqb_bsoftlimit =
		    gq_rslt.getquota_rslt_u.gqr_rquota.rq_bsoftlimit *
		    (gq_rslt.getquota_rslt_u.gqr_rquota.rq_bsize / DEV_BSIZE);
		dqp->dqb_curblocks =
		    gq_rslt.getquota_rslt_u.gqr_rquota.rq_curblocks *
		    (gq_rslt.getquota_rslt_u.gqr_rquota.rq_bsize / DEV_BSIZE);
			/* inodes */
		dqp->dqb_ihardlimit =
			gq_rslt.getquota_rslt_u.gqr_rquota.rq_fhardlimit;
		dqp->dqb_isoftlimit =
			gq_rslt.getquota_rslt_u.gqr_rquota.rq_fsoftlimit;
		dqp->dqb_curinodes =
			gq_rslt.getquota_rslt_u.gqr_rquota.rq_curfiles;
			/* grace times */
		dqp->dqb_btime =
		    tv.tv_sec + gq_rslt.getquota_rslt_u.gqr_rquota.rq_btimeleft;
		dqp->dqb_itime =
		    tv.tv_sec + gq_rslt.getquota_rslt_u.gqr_rquota.rq_ftimeleft;
		*cp = ':';
		return (1);
	default:
		warnx("bad rpc result, host: %s", fst->f_mntfromname);
		break;
	}
	*cp = ':';
	return (0);
}
 
static int
callaurpc(char *host, int prognum, int versnum, int procnum,
    xdrproc_t inproc, char *in, xdrproc_t outproc, char *out)
{
	struct sockaddr_in server_addr;
	enum clnt_stat clnt_stat;
	struct hostent *hp;
	struct timeval timeout, tottimeout;
 
	CLIENT *client = NULL;
	int sock = RPC_ANYSOCK;
 
	if ((hp = gethostbyname(host)) == NULL)
		return ((int) RPC_UNKNOWNHOST);
	timeout.tv_usec = 0;
	timeout.tv_sec = 6;
	bcopy(hp->h_addr, &server_addr.sin_addr,
			MIN(hp->h_length,(int)sizeof(server_addr.sin_addr)));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port =  0;

	if ((client = clntudp_create(&server_addr, prognum,
	    versnum, timeout, &sock)) == NULL)
		return ((int) rpc_createerr.cf_stat);

	client->cl_auth = authunix_create_default();
	tottimeout.tv_sec = 25;
	tottimeout.tv_usec = 0;
	clnt_stat = clnt_call(client, procnum, inproc, in,
	    outproc, out, tottimeout);
 
	return ((int) clnt_stat);
}

static int
alldigits(char *s)
{
	int c;

	c = *s++;
	do {
		if (!isdigit(c))
			return (0);
	} while ((c = *s++));
	return (1);
}
