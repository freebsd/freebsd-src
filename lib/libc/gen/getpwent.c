/*
 * Copyright (c) 1988 The Regents of the University of California.
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

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)getpwent.c	5.21 (Berkeley) 3/14/91";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <fcntl.h>
#include <db.h>
#include <syslog.h>
#include <pwd.h>
#include <utmp.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static struct passwd _pw_passwd;	/* password structure */
static DB *_pw_db;			/* password database */
static int _pw_keynum;			/* key counter */
static int _pw_stayopen;		/* keep fd's open */
static int __hashpw(), __initdb();

struct passwd *
getpwent()
{
	DBT key;
	char bf[sizeof(_pw_keynum) + 1];

	if (!_pw_db && !__initdb())
		return((struct passwd *)NULL);

	++_pw_keynum;
	bf[0] = _PW_KEYBYNUM;
	bcopy((char *)&_pw_keynum, bf + 1, sizeof(_pw_keynum));
	key.data = (u_char *)bf;
	key.size = sizeof(_pw_keynum) + 1;
	return(__hashpw(&key) ? &_pw_passwd : (struct passwd *)NULL);
}

struct passwd *
getpwnam(name)
	const char *name;
{
	DBT key;
	int len, rval;
	char bf[UT_NAMESIZE + 1];

	if (!_pw_db && !__initdb())
		return((struct passwd *)NULL);

	bf[0] = _PW_KEYBYNAME;
	len = strlen(name);
	bcopy(name, bf + 1, MIN(len, UT_NAMESIZE));
	key.data = (u_char *)bf;
	key.size = len + 1;
	rval = __hashpw(&key);

	if (!_pw_stayopen) {
		(void)(_pw_db->close)(_pw_db);
		_pw_db = (DB *)NULL;
	}
	return(rval ? &_pw_passwd : (struct passwd *)NULL);
}

struct passwd *
#ifdef __STDC__
getpwuid(uid_t uid)
#else
getpwuid(uid)
	int uid;
#endif
{
	DBT key;
	int keyuid, rval;
	char bf[sizeof(keyuid) + 1];

	if (!_pw_db && !__initdb())
		return((struct passwd *)NULL);

	bf[0] = _PW_KEYBYUID;
	keyuid = uid;
	bcopy(&keyuid, bf + 1, sizeof(keyuid));
	key.data = (u_char *)bf;
	key.size = sizeof(keyuid) + 1;
	rval = __hashpw(&key);

	if (!_pw_stayopen) {
		(void)(_pw_db->close)(_pw_db);
		_pw_db = (DB *)NULL;
	}
	return(rval ? &_pw_passwd : (struct passwd *)NULL);
}

int
setpassent(stayopen)
	int stayopen;
{
	_pw_keynum = 0;
	_pw_stayopen = stayopen;
	return(1);
}

int
setpwent()
{
	_pw_keynum = 0;
	_pw_stayopen = 0;
	return(1);
}

void
endpwent()
{
	_pw_keynum = 0;
	if (_pw_db) {
		(void)(_pw_db->close)(_pw_db);
		_pw_db = (DB *)NULL;
	}
}

static
__initdb()
{
	static int warned;
	char *p;

	p = (geteuid()) ? _PATH_MP_DB : _PATH_SMP_DB;
	_pw_db = dbopen(p, O_RDONLY, 0, DB_HASH, NULL);
	if (_pw_db)
		return(1);
	if (!warned)
		syslog(LOG_ERR, "%s: %m", p);
	return(0);
}

static
__hashpw(key)
	DBT *key;
{
	register char *p, *t;
	static u_int max;
	static char *line;
	DBT data;

	if ((_pw_db->get)(_pw_db, key, &data, 0))
		return(0);
	p = (char *)data.data;
	if (data.size > max && !(line = realloc(line, max += 1024)))
		return(0);

	t = line;
#define	EXPAND(e)	e = t; while (*t++ = *p++);
	EXPAND(_pw_passwd.pw_name);
	EXPAND(_pw_passwd.pw_passwd);
	bcopy(p, (char *)&_pw_passwd.pw_uid, sizeof(int));
	p += sizeof(int);
	bcopy(p, (char *)&_pw_passwd.pw_gid, sizeof(int));
	p += sizeof(int);
	bcopy(p, (char *)&_pw_passwd.pw_change, sizeof(time_t));
	p += sizeof(time_t);
	EXPAND(_pw_passwd.pw_class);
	EXPAND(_pw_passwd.pw_gecos);
	EXPAND(_pw_passwd.pw_dir);
	EXPAND(_pw_passwd.pw_shell);
	bcopy(p, (char *)&_pw_passwd.pw_expire, sizeof(time_t));
	p += sizeof(time_t);
	return(1);
}
