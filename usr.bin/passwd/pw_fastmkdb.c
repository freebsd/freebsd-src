/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
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

#include <sys/param.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include <db.h>
#include <pwd.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

/* #define PW_COMPACT */
/* Compact pwd.db/spwd.db structure by Alex G. Bulushev, bag@demos.su */
#ifdef PW_COMPACT
# define HI_BSIZE 1024
# define HI_NELEM 4500
# define HI_CACHE (1024 * 1024)
#else
# define HI_BSIZE 2048
# define HI_NELEM 4500
# define HI_CACHE (4000 * 1024)
#endif

#define	INSECURE	1
#define	SECURE		2
#define	PERM_INSECURE	(S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)
#define	PERM_SECURE	(S_IRUSR|S_IWUSR)

static enum state { FILE_INSECURE, FILE_SECURE, FILE_ORIG } clean;
static struct passwd pwd;			/* password structure */

extern char *tempname;
extern char *progname;
extern int globcnt;

pw_fastmkdb(new_pwd)
	struct passwd *new_pwd;
{
	register int len;
	register char *p, *t;
	DB *dp, *edp;
	sigset_t set;
	DBT data, key;
#ifdef PW_COMPACT
	DBT pdata, sdata;
#endif
	char buf[MAX(MAXPATHLEN, LINE_MAX * 2)], tbuf[1024];
	char buf2[MAX(MAXPATHLEN, LINE_MAX * 2)];
	int uid;
	int gid;
	HASHINFO openinfo;

	/* Hash database parameters */
	openinfo.bsize=HI_BSIZE;
	openinfo.ffactor=32;
	openinfo.nelem=HI_NELEM;       /* Default value was 300 */
	openinfo.cachesize=HI_CACHE;   /* Default value was 512 */
	openinfo.hash=NULL;
	openinfo.lorder=0;

	/*
	 * This could be done to allow the user to interrupt.  Probably
	 * not worth the effort.
	 */
	sigemptyset(&set);
	sigaddset(&set, SIGTSTP);
	sigaddset(&set, SIGHUP);
	sigaddset(&set, SIGINT);
	sigaddset(&set, SIGQUIT);
	sigaddset(&set, SIGTERM);
	(void)sigprocmask(SIG_BLOCK, &set, (sigset_t *)NULL);

	/* Open the insecure password database. */
	dp = dbopen(_PATH_MP_DB, O_RDWR, PERM_INSECURE, DB_HASH, &openinfo);
	if (!dp)
		error(_PATH_MP_DB);
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
	bcopy((char *)new_pwd, (char *)&pwd, sizeof(struct passwd));
	data.data = (u_char *)buf;
	key.data = (u_char *)tbuf;
#ifdef PW_COMPACT
	pdata.data = (u_char *)&globcnt;
	pdata.size = sizeof(int);
	sdata.data = (u_char *)pwd.pw_passwd;
	sdata.size = strlen(pwd.pw_passwd) + 1;
#endif
#define	COMPACT(e)	t = e; while (*p++ = *t++);

	/* Create insecure data. */
	uid = pwd.pw_uid;	/* force a particular size */
	gid = pwd.pw_gid;
	p = buf;
	COMPACT(pwd.pw_name);
#ifndef PW_COMPACT
	COMPACT("*");
#endif
	bcopy((char *)&uid, p, sizeof(uid));
	p += sizeof(int);
	bcopy((char *)&gid, p, sizeof(gid));
	p += sizeof(int);
	bcopy((char *)&pwd.pw_change, p, sizeof(time_t));
	p += sizeof(time_t);
	COMPACT(pwd.pw_class);
	COMPACT(pwd.pw_gecos);
	COMPACT(pwd.pw_dir);
	COMPACT(pwd.pw_shell);
	bcopy((char *)&pwd.pw_expire, p, sizeof(time_t));
	p += sizeof(time_t);
	data.size = p - buf;

	/* Store insecure by name. */
	tbuf[0] = _PW_KEYBYNAME;
	len = strlen(pwd.pw_name);
	bcopy(pwd.pw_name, tbuf + 1, len);
	key.size = len + 1;
#ifdef PW_COMPACT
	if ((dp->put)(dp, &key, &pdata, 0) == -1)
#else
	if ((dp->put)(dp, &key, &data, 0) == -1)
#endif
		error("put");

	/* Store insecure by uid. */
	tbuf[0] = _PW_KEYBYUID;
	bcopy((char *)&uid, tbuf + 1, sizeof(uid));
	key.size = sizeof(uid) + 1;
#ifdef PW_COMPACT
	if ((dp->put)(dp, &key, &pdata, 0) == -1)
#else
	if ((dp->put)(dp, &key, &data, 0) == -1)
#endif
		error("put");

	/* Store insecure by number. */
	tbuf[0] = _PW_KEYBYNUM;
	bcopy((char *)&globcnt, tbuf + 1, sizeof(globcnt));
	key.size = sizeof(globcnt) + 1;
	if ((dp->put)(dp, &key, &data, 0) == -1)
		error("put");

	(void)(dp->close)(dp);

	/* Open the encrypted password database. */
	edp = dbopen(_PATH_SMP_DB, O_RDWR, PERM_SECURE, DB_HASH, &openinfo);
	if (!edp)
		error(_PATH_SMP_DB);
#ifdef PW_COMPACT
	/* Store secure. */
	if ((edp->put)(edp, &key, &sdata, 0) == -1)
		error("put");
#else

	/* Create secure data. */
	p = buf;
	COMPACT(pwd.pw_name);
	COMPACT(pwd.pw_passwd);
	bcopy((char *)&uid, p, sizeof(uid));
	p += sizeof(int);
	bcopy((char *)&gid, p, sizeof(gid));
	p += sizeof(int);
	bcopy((char *)&pwd.pw_change, p, sizeof(time_t));
	p += sizeof(time_t);
	COMPACT(pwd.pw_class);
	COMPACT(pwd.pw_gecos);
	COMPACT(pwd.pw_dir);
	COMPACT(pwd.pw_shell);
	bcopy((char *)&pwd.pw_expire, p, sizeof(time_t));
	p += sizeof(time_t);
	data.size = p - buf;

	/* Store secure by name. */
	tbuf[0] = _PW_KEYBYNAME;
	len = strlen(pwd.pw_name);
	bcopy(pwd.pw_name, tbuf + 1, len);
	key.size = len + 1;
	if ((edp->put)(edp, &key, &data, 0) == -1)
		error("put");

	/* Store secure by number. */
	tbuf[0] = _PW_KEYBYNUM;
	bcopy((char *)&globcnt, tbuf + 1, sizeof(globcnt));
	key.size = sizeof(globcnt) + 1;
	if ((edp->put)(edp, &key, &data, 0) == -1)
		error("put");

	/* Store secure by uid. */
	tbuf[0] = _PW_KEYBYUID;
	bcopy((char *)&uid, tbuf + 1, sizeof(uid));
	key.size = sizeof(uid) + 1;
	if ((edp->put)(edp, &key, &data, 0) == -1)
		error("put");
#endif

	(void)(edp->close)(edp);

	/*
	 * Move the master password LAST -- chpass(1), passwd(1) and vipw(8)
	 * all use flock(2) on it to block other incarnations of themselves.
	 * The rename means that everything is unlocked, as the original file
	 * can no longer be accessed.
	 */
	mv(tempname, _PATH_MASTERPASSWD);
	return(0);
}

mv(from, to)
	char *from, *to;
{
	int sverrno;
	char buf[MAXPATHLEN];

	if (rename(from, to)) {
		sverrno = errno;
		(void)sprintf(buf, "%s to %s", from, to);
		errno = sverrno;
		error(buf);
	}
}

error(name)
	char *name;
{
	(void)fprintf(stderr, "%s: %s: %s\n", progname, name, strerror(errno));
	return(-1);
}
