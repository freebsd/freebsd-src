/*-
 * Copyright (c) 1991, 1993, 1994
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
"@(#) Copyright (c) 1991, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)pwd_mkdb.c	8.5 (Berkeley) 4/20/94";
#endif
static const char rcsid[] =
	"$Id$";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <db.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pw_scan.h"

#define	INSECURE	1
#define	SECURE		2
#define	PERM_INSECURE	(S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)
#define	PERM_SECURE	(S_IRUSR|S_IWUSR)

HASHINFO openinfo = {
	4096,		/* bsize */
	32,		/* ffactor */
	256,		/* nelem */
	2048 * 1024,	/* cachesize */
	NULL,		/* hash() */
	0		/* lorder */
};

static enum state { FILE_INSECURE, FILE_SECURE, FILE_ORIG } clean;
static struct passwd pwd;			/* password structure */
static char *pname;				/* password file name */
static char prefix[MAXPATHLEN];

static int Cflag;	/* flag for comments */
static char line[LINE_MAX];

void	cleanup __P((void));
void	error __P((char *));
void	cp __P((char *, char *, mode_t mode));
void	mv __P((char *, char *));
int	scan __P((FILE *, struct passwd *));
static void	usage __P((void));

int
main(argc, argv)
	int argc;
	char *argv[];
{
	DB *dp, *sdp, *pw_db;
	DBT data, sdata, key;
	FILE *fp, *oldfp;
	sigset_t set;
	int ch, cnt, ypcnt, len, makeold, tfd, yp_enabled = 0;
	char *p, *t;
	char buf[MAX(MAXPATHLEN, LINE_MAX * 2)], tbuf[1024];
	char sbuf[MAX(MAXPATHLEN, LINE_MAX * 2)];
	char buf2[MAXPATHLEN];
	char sbuf2[MAXPATHLEN];
	char *username;
	u_int method, methoduid;
	int cflag;

	cflag = 0;
	strcpy(prefix, _PATH_PWD);
	makeold = 0;
	username = NULL;
	while ((ch = getopt(argc, argv, "cd:pu:v")) != -1)
		switch(ch) {
		case 'c':                       /* verify only */
			cflag = 1;
			break;
		case 'd':
			strncpy(prefix, optarg, sizeof prefix - 1);
			break;
		case 'p':			/* create V7 "file.orig" */
			makeold = 1;
			break;
		case 'u':			/* only update this record */
			username = optarg;
			break;
		case 'v':                       /* backward compatible */
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc != 1 || (username && (*username == '+' || *username == '-')))
		usage();

	/*
	 * This could be changed to allow the user to interrupt.
	 * Probably not worth the effort.
	 */
	sigemptyset(&set);
	sigaddset(&set, SIGTSTP);
	sigaddset(&set, SIGHUP);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGQUIT);
	sigaddset(&set, SIGTERM);
	(void)sigprocmask(SIG_BLOCK, &set, (sigset_t *)NULL);

	/* We don't care what the user wants. */
	(void)umask(0);

	pname = *argv;
	/* Open the original password file */
	if (!(fp = fopen(pname, "r")))
		error(pname);

	/* check only if password database is valid */
	if (cflag) {
		for (cnt = 1; scan(fp, &pwd); ++cnt);
		exit(0);
	}

	/* Open the temporary insecure password database. */
	(void)snprintf(buf, sizeof(buf), "%s/%s.tmp", prefix, _MP_DB);
	(void)snprintf(sbuf, sizeof(sbuf), "%s/%s.tmp", prefix, _SMP_DB);
	if (username) {
		(void)snprintf(buf2, sizeof(buf2), "%s/%s", prefix, _MP_DB);
		(void)snprintf(sbuf2, sizeof(sbuf2), "%s/%s", prefix, _SMP_DB);

		clean = FILE_INSECURE;
		cp(buf2, buf, PERM_INSECURE);
		dp = dbopen(buf,
		    O_RDWR|O_EXCL, PERM_INSECURE, DB_HASH, &openinfo);
		if (dp == NULL)
			error(buf);

		clean = FILE_SECURE;
		cp(sbuf2, sbuf, PERM_SECURE);
		sdp = dbopen(sbuf,
		    O_RDWR|O_EXCL, PERM_SECURE, DB_HASH, &openinfo);
		if (sdp == NULL)
			error(sbuf);

		/*
		 * Do some trouble to check if we should store this users 
		 * uid. Don't use getpwnam/getpwuid as that interferes 
		 * with NIS.
		 */
		pw_db = dbopen(_PATH_MP_DB, O_RDONLY, 0, DB_HASH, NULL);
		if (!pw_db)
			error(_MP_DB);
		buf[0] = _PW_KEYBYNAME;
		len = strlen(username);

		/* Only check that username fits in buffer */
		memmove(buf + 1, username, MIN(len, sizeof(buf) - 1));
		key.data = (u_char *)buf;
		key.size = len + 1;
		if ((pw_db->get)(pw_db, &key, &data, 0) == 0) {
			p = (char *)data.data;

			/* jump over pw_name and pw_passwd, to get to pw_uid */
			while (*p++)
				;
			while (*p++)
				;

			buf[0] = _PW_KEYBYUID;
			memmove(buf + 1, p, sizeof(int));
			key.data = (u_char *)buf;
			key.size = sizeof(int) + 1;

			if ((pw_db->get)(pw_db, &key, &data, 0) == 0) {
				/* First field of data.data holds pw_pwname */
				if (!strcmp(data.data, username))
					methoduid = 0;
				else
					methoduid = R_NOOVERWRITE;
			} else {
				methoduid = R_NOOVERWRITE;
			}
		} else {
			methoduid = R_NOOVERWRITE;
		}
		(void)(pw_db->close)(pw_db);
		method = 0;
	} else {
		dp = dbopen(buf,
		    O_RDWR|O_CREAT|O_EXCL, PERM_INSECURE, DB_HASH, &openinfo);
		if (dp == NULL)
			error(buf);
		clean = FILE_INSECURE;

		sdp = dbopen(sbuf,
		    O_RDWR|O_CREAT|O_EXCL, PERM_SECURE, DB_HASH, &openinfo);
		if (sdp == NULL)
			error(sbuf);
		clean = FILE_SECURE;

		method = R_NOOVERWRITE;
		methoduid = R_NOOVERWRITE;
	}

	/*
	 * Open file for old password file.  Minor trickiness -- don't want to
	 * chance the file already existing, since someone (stupidly) might
	 * still be using this for permission checking.  So, open it first and
	 * fdopen the resulting fd.  The resulting file should be readable by
	 * everyone.
	 */
	if (makeold) {
		(void)snprintf(buf, sizeof(buf), "%s.orig", pname);
		if ((tfd = open(buf,
		    O_WRONLY|O_CREAT|O_EXCL, PERM_INSECURE)) < 0)
			error(buf);
		if ((oldfp = fdopen(tfd, "w")) == NULL)
			error(buf);
		clean = FILE_ORIG;
	}

	/*
	 * The databases actually contain three copies of the original data.
	 * Each password file entry is converted into a rough approximation
	 * of a ``struct passwd'', with the strings placed inline.  This
	 * object is then stored as the data for three separate keys.  The
	 * first key * is the pw_name field prepended by the _PW_KEYBYNAME
	 * character.  The second key is the pw_uid field prepended by the
	 * _PW_KEYBYUID character.  The third key is the line number in the
	 * original file prepended by the _PW_KEYBYNUM character.  (The special
	 * characters are prepended to ensure that the keys do not collide.)
	 */
	ypcnt = 1;
	data.data = (u_char *)buf;
	sdata.data = (u_char *)sbuf;
	key.data = (u_char *)tbuf;
	for (cnt = 1; scan(fp, &pwd); ++cnt) {
		if (!Cflag && 
		    (pwd.pw_name[0] == '+' || pwd.pw_name[0] == '-'))
			yp_enabled = 1;
#define	COMPACT(e)	t = e; while ((*p++ = *t++));
		if (!Cflag && 
		    (!username || (strcmp(username, pwd.pw_name) == 0))) {
			/* Create insecure data. */
			p = buf;
			COMPACT(pwd.pw_name);
			COMPACT("*");
			memmove(p, &pwd.pw_uid, sizeof(int));
			p += sizeof(int);
			memmove(p, &pwd.pw_gid, sizeof(int));
			p += sizeof(int);
			memmove(p, &pwd.pw_change, sizeof(time_t));
			p += sizeof(time_t);
			COMPACT(pwd.pw_class);
			COMPACT(pwd.pw_gecos);
			COMPACT(pwd.pw_dir);
			COMPACT(pwd.pw_shell);
			memmove(p, &pwd.pw_expire, sizeof(time_t));
			p += sizeof(time_t);
			memmove(p, &pwd.pw_fields, sizeof pwd.pw_fields);
			p += sizeof pwd.pw_fields;
			data.size = p - buf;

			/* Create secure data. */
			p = sbuf;
			COMPACT(pwd.pw_name);
			COMPACT(pwd.pw_passwd);
			memmove(p, &pwd.pw_uid, sizeof(int));
			p += sizeof(int);
			memmove(p, &pwd.pw_gid, sizeof(int));
			p += sizeof(int);
			memmove(p, &pwd.pw_change, sizeof(time_t));
			p += sizeof(time_t);
			COMPACT(pwd.pw_class);
			COMPACT(pwd.pw_gecos);
			COMPACT(pwd.pw_dir);
			COMPACT(pwd.pw_shell);
			memmove(p, &pwd.pw_expire, sizeof(time_t));
			p += sizeof(time_t);
			memmove(p, &pwd.pw_fields, sizeof pwd.pw_fields);
			p += sizeof pwd.pw_fields;
			sdata.size = p - sbuf;

			/* Store insecure by name. */
			tbuf[0] = _PW_KEYBYNAME;
			len = strlen(pwd.pw_name);
			memmove(tbuf + 1, pwd.pw_name, len);
			key.size = len + 1;
			if ((dp->put)(dp, &key, &data, method) == -1)
				error("put");

			/* Store insecure by number. */
			tbuf[0] = _PW_KEYBYNUM;
			memmove(tbuf + 1, &cnt, sizeof(cnt));
			key.size = sizeof(cnt) + 1;
			if ((dp->put)(dp, &key, &data, method) == -1)
				error("put");

			/* Store insecure by uid. */
			tbuf[0] = _PW_KEYBYUID;
			memmove(tbuf + 1, &pwd.pw_uid, sizeof(pwd.pw_uid));
			key.size = sizeof(pwd.pw_uid) + 1;
			if ((dp->put)(dp, &key, &data, methoduid) == -1)
				error("put");

			/* Store secure by name. */
			tbuf[0] = _PW_KEYBYNAME;
			len = strlen(pwd.pw_name);
			memmove(tbuf + 1, pwd.pw_name, len);
			key.size = len + 1;
			if ((sdp->put)(sdp, &key, &sdata, method) == -1)
				error("put");

			/* Store secure by number. */
			tbuf[0] = _PW_KEYBYNUM;
			memmove(tbuf + 1, &cnt, sizeof(cnt));
			key.size = sizeof(cnt) + 1;
			if ((sdp->put)(sdp, &key, &sdata, method) == -1)
				error("put");

			/* Store secure by uid. */
			tbuf[0] = _PW_KEYBYUID;
			memmove(tbuf + 1, &pwd.pw_uid, sizeof(pwd.pw_uid));
			key.size = sizeof(pwd.pw_uid) + 1;
			if ((sdp->put)(sdp, &key, &sdata, methoduid) == -1)
				error("put");

			/* Store insecure and secure special plus and special minus */
			if (pwd.pw_name[0] == '+' || pwd.pw_name[0] == '-') {
				tbuf[0] = _PW_KEYYPBYNUM;
				memmove(tbuf + 1, &ypcnt, sizeof(cnt));
				ypcnt++;
				key.size = sizeof(cnt) + 1;
				if ((dp->put)(dp, &key, &data, method) == -1)
					error("put");
				if ((sdp->put)(sdp, &key, &sdata, method) == -1)
					error("put");
			}
		}
		/* Create original format password file entry */
		if (Cflag && makeold)	/* copy comments */
			(void)fprintf(oldfp, "%s\n", line);
		else if (makeold) {
			char uidstr[20];
			char gidstr[20];

			snprintf(uidstr, sizeof(uidstr), "%d", pwd.pw_uid);
			snprintf(gidstr, sizeof(gidstr), "%d", pwd.pw_gid);

			(void)fprintf(oldfp, "%s:*:%s:%s:%s:%s:%s\n",
			    pwd.pw_name, pwd.pw_fields & _PWF_UID ? uidstr : "",
			    pwd.pw_fields & _PWF_GID ? gidstr : "",
			    pwd.pw_gecos, pwd.pw_dir, pwd.pw_shell);
		}
	}
	/* If YP enabled, set flag. */
	if (yp_enabled) {
		buf[0] = yp_enabled + 2;
		data.size = 1;
		tbuf[0] = _PW_KEYYPENABLED;
		key.size = 1;
		if ((dp->put)(dp, &key, &data, method) == -1)
			error("put");
		if ((sdp->put)(sdp, &key, &data, method) == -1)
			error("put");
	}

	if ((dp->close)(dp) == -1)
		error("close");
	if ((sdp->close)(sdp) == -1)
		error("close");
	if (makeold) {
		(void)fflush(oldfp);
		(void)fclose(oldfp);
	}

	/* Set master.passwd permissions, in case caller forgot. */
	(void)fchmod(fileno(fp), S_IRUSR|S_IWUSR);
	(void)fclose(fp);

	/* Install as the real password files. */
	(void)snprintf(buf, sizeof(buf), "%s/%s.tmp", prefix, _MP_DB);
	(void)snprintf(buf2, sizeof(buf2), "%s/%s", prefix, _MP_DB);
	mv(buf, buf2);
	(void)snprintf(buf, sizeof(buf), "%s/%s.tmp", prefix, _SMP_DB);
	(void)snprintf(buf2, sizeof(buf2), "%s/%s", prefix, _SMP_DB);
	mv(buf, buf2);
	if (makeold) {
		(void)snprintf(buf2, sizeof(buf2), "%s/%s", prefix, _PASSWD);
		(void)snprintf(buf, sizeof(buf), "%s.orig", pname);
		mv(buf, buf2);
	}
	/*
	 * Move the master password LAST -- chpass(1), passwd(1) and vipw(8)
	 * all use flock(2) on it to block other incarnations of themselves.
	 * The rename means that everything is unlocked, as the original file
	 * can no longer be accessed.
	 */
	(void)snprintf(buf, sizeof(buf), "%s/%s", prefix, _MASTERPASSWD);
	mv(pname, buf);
	exit(0);
}

int
scan(fp, pw)
	FILE *fp;
	struct passwd *pw;
{
	static int lcnt;
	char *p;

	if (!fgets(line, sizeof(line), fp))
		return (0);
	++lcnt;
	/*
	 * ``... if I swallow anything evil, put your fingers down my
	 * throat...''
	 *	-- The Who
	 */
	if (!(p = strchr(line, '\n'))) {
		warnx("line too long");
		goto fmt;

	}
	*p = '\0';

#ifdef PASSWD_IGNORE_COMMENTS
	/* 
	 * Ignore comments: ^[ \t]*#
	 */
	for (p = line; *p != '\0'; p++)
		if (*p != ' ' && *p != '\t')
			break;
	if (*p == '#' || *p == '\0') {
		Cflag = 1;
		return(1);
	} else
		Cflag = 0;
#endif 

	if (!pw_scan(line, pw)) {
		warnx("at line #%d", lcnt);
fmt:		errno = EFTYPE;	/* XXX */
		error(pname);
	}

	return (1);
}

void                    
cp(from, to, mode)              
	char *from, *to;
	mode_t mode;    
{               
	static char buf[MAXBSIZE];
	int from_fd, rcount, to_fd, wcount;

	if ((from_fd = open(from, O_RDONLY, 0)) < 0)
		error(from);
	if ((to_fd = open(to, O_WRONLY|O_CREAT|O_EXCL, mode)) < 0)
		error(to);
	while ((rcount = read(from_fd, buf, MAXBSIZE)) > 0) {
		wcount = write(to_fd, buf, rcount);
		if (rcount != wcount || wcount == -1) {
			int sverrno = errno;

			(void)snprintf(buf, sizeof(buf), "%s to %s", from, to);
			errno = sverrno;
			error(buf);
		}
	}
	if (rcount < 0) {
		int sverrno = errno;

		(void)snprintf(buf, sizeof(buf), "%s to %s", from, to);
		errno = sverrno;
		error(buf);
	}
}


void
mv(from, to)
	char *from, *to;
{
	char buf[MAXPATHLEN];

	if (rename(from, to)) {
		int sverrno = errno;
		(void)snprintf(buf, sizeof(buf), "%s to %s", from, to);
		errno = sverrno;
		error(buf);
	}
}

void
error(name)
	char *name;
{

	warn("%s", name);
	cleanup();
	exit(1);
}

void
cleanup()
{
	char buf[MAXPATHLEN];

	switch(clean) {
	case FILE_ORIG:
		(void)snprintf(buf, sizeof(buf), "%s.orig", pname);
		(void)unlink(buf);
		/* FALLTHROUGH */
	case FILE_SECURE:
		(void)snprintf(buf, sizeof(buf), "%s/%s.tmp", prefix, _SMP_DB);
		(void)unlink(buf);
		/* FALLTHROUGH */
	case FILE_INSECURE:
		(void)snprintf(buf, sizeof(buf), "%s/%s.tmp", prefix, _MP_DB);
		(void)unlink(buf);
	}
}

static void
usage()
{

	(void)fprintf(stderr,
"usage: pwd_mkdb [-c] [-p] [-d <dest dir>] [-u <local username>] file\n");
	exit(1);
}
