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
/*static char *sccsid = "from: @(#)getpwent.c	5.21 (Berkeley) 3/14/91";*/
static char *rcsid = "$Id: getpwent.c,v 1.9 1994/05/05 18:16:44 ache Exp $";
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
#ifdef YP
#include <stdio.h>
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#endif

/* #define PW_COMPACT */
/* Compact pwd.db/spwd.db structure by Alex G. Bulushev, bag@demos.su */

static struct passwd _pw_passwd;	/* password structure */
static DB *_pw_db;			/* password database */
#ifdef PW_COMPACT
static DB *_spw_db;                     /* shadow password database */
#endif
static int _pw_keynum;			/* key counter */
static int _pw_stayopen;		/* keep fd's open */
static int __hashpw(), __initdb();

#ifdef YP
static char     *__ypcurrent, *__ypdomain;
static int      __ypcurrentlen, __ypmode=0;
static char	line[1024];

static int
__ypparse(pw, s)
struct passwd *pw;
char *s;
{
	char *bp, *cp;

	bp = s;
	pw->pw_name = strsep(&bp, ":\n");
	pw->pw_passwd = strsep(&bp, ":\n");
	if (!(cp = strsep(&bp, ":\n")))
		return 1;
	pw->pw_uid = atoi(cp);
	if (!(cp = strsep(&bp, ":\n")))
		return 0;
	pw->pw_gid = atoi(cp);
	pw->pw_change = 0;
	pw->pw_class = "";
	pw->pw_gecos = strsep(&bp, ":\n");
	pw->pw_dir = strsep(&bp, ":\n");
	pw->pw_shell = strsep(&bp, ":\n");
	pw->pw_expire = 0;
	return 0;
}
#endif

struct passwd *
getpwent()
{
	DBT key;
	char bf[sizeof(_pw_keynum) + 1];
#ifdef YP
	char *bp, *cp;
#endif

	if (!_pw_db && !__initdb())
		return((struct passwd *)NULL);

#ifdef YP
again:
	if(__ypmode) {
		char *key, *data;
		int keylen, datalen;
		int r;

		if(!__ypdomain) {
			if( _yp_check(&__ypdomain) == 0) {
				__ypmode = 0;
				goto again;
			}
		}
		if(__ypcurrent) {
			r = yp_next(__ypdomain, "passwd.byname",
				__ypcurrent, __ypcurrentlen,
				&key, &keylen, &data, &datalen);
			free(__ypcurrent);
			__ypcurrent = NULL;
			/*printf("yp_next %d\n", r);*/
			switch(r) {
			case 0:
				break;
			default:
				__ypcurrent = NULL;
				__ypmode = 0;
				free(data);
				data = NULL;
				goto again;
			}
			__ypcurrent = key;
			__ypcurrentlen = keylen;
			bcopy(data, line, datalen);
			free(data);
			data = NULL;
		} else {
			r = yp_first(__ypdomain, "passwd.byname",
				&__ypcurrent, &__ypcurrentlen,
				&data, &datalen);
			/*printf("yp_first %d\n", r);*/
			switch(r) {
			case 0:
				break;
			default:
				__ypmode = 0;
				free(data);
				goto again;
			}
			bcopy(data, line, datalen);
			free(data);
			data = NULL;
		}
		line[datalen] = '\0';
		/*printf("line = %s\n", line);*/
		bp = line;
		goto parse;
	}
#endif

	++_pw_keynum;
	bf[0] = _PW_KEYBYNUM;
	bcopy((char *)&_pw_keynum, bf + 1, sizeof(_pw_keynum));
	key.data = (u_char *)bf;
	key.size = sizeof(_pw_keynum) + 1;
	if(__hashpw(&key)) {
#ifdef YP
		if(strcmp(_pw_passwd.pw_name, "+") == 0) {
			__ypmode = 1;
			goto again;
		}
#endif
		return &_pw_passwd;
	}
	return (struct passwd *)NULL;

#ifdef YP
parse:
	_pw_passwd.pw_name = strsep(&bp, ":\n");
	_pw_passwd.pw_passwd = strsep(&bp, ":\n");
	if (!(cp = strsep(&bp, ":\n")))
		goto again;
	_pw_passwd.pw_uid = atoi(cp);
	if (!(cp = strsep(&bp, ":\n")))
		goto again;
	_pw_passwd.pw_gid = atoi(cp);
	_pw_passwd.pw_change = 0;
	_pw_passwd.pw_class = "";
	_pw_passwd.pw_gecos = strsep(&bp, ":\n");
	_pw_passwd.pw_dir = strsep(&bp, ":\n");
	_pw_passwd.pw_shell = strsep(&bp, ":\n");
	_pw_passwd.pw_expire = 0;
	return &_pw_passwd;
#endif
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

#ifdef YP
	bf[0] = _PW_KEYBYNAME;
	len = strlen("+");
	bcopy("+", bf + 1, MIN(len, UT_NAMESIZE));
	key.data = (u_char *)bf;
	key.size = len + 1;

	/*
	 * If there is a user called "+", then YP is active. In that
	 * case we must sequence through the passwd file in sequence.
	 */
	if ( __hashpw(&key)) {
		int r;

		for(_pw_keynum=1; _pw_keynum; _pw_keynum++) {
			bf[0] = _PW_KEYBYNUM;
			bcopy((char *)&_pw_keynum, bf + 1, sizeof(_pw_keynum));
			key.data = (u_char *)bf;
			key.size = sizeof(_pw_keynum) + 1;
			if(__hashpw(&key) == 0)
				break;
			if(strcmp(_pw_passwd.pw_name, "+") == 0) {
				if(!__ypdomain) {
					if(_yp_check(&__ypdomain) == 0) {
						continue;
					}
				}
				if(__ypcurrent) {
					free(__ypcurrent);
					__ypcurrent = NULL;
				}
				r = yp_match(__ypdomain, "passwd.byname",
					name, strlen(name),
					&__ypcurrent, &__ypcurrentlen);
				switch(r) {
				case 0:
					break;
				default:
					free(__ypcurrent);
					__ypcurrent = NULL;
					continue;
				}
				bcopy(__ypcurrent, line, __ypcurrentlen);
				line[__ypcurrentlen] = '\0';
				if(__ypparse(&_pw_passwd, line))
					continue;
			}
			if( strcmp(_pw_passwd.pw_name, name) == 0) {
				if (!_pw_stayopen) {
					(void)(_pw_db->close)(_pw_db);
					_pw_db = (DB *)NULL;
#ifdef PW_COMPACT
				    if (_spw_db) {
					(void)(_spw_db->close)(_spw_db);
					_spw_db = (DB *)NULL;
				    }
#endif
				}
				return &_pw_passwd;
			}
			continue;
		}
		if (!_pw_stayopen) {
			(void)(_pw_db->close)(_pw_db);
			_pw_db = (DB *)NULL;
#ifdef PW_COMPACT
			if (_spw_db) {
			     (void)(_spw_db->close)(_spw_db);
			      _spw_db = (DB *)NULL;
			}
#endif
		}
		return (struct passwd *)NULL;
	}
#endif /* YP */

	bf[0] = _PW_KEYBYNAME;
	len = strlen(name);
	bcopy(name, bf + 1, MIN(len, UT_NAMESIZE));
	key.data = (u_char *)bf;
	key.size = len + 1;
	rval = __hashpw(&key);

	if (!_pw_stayopen) {
		(void)(_pw_db->close)(_pw_db);
		_pw_db = (DB *)NULL;
#ifdef PW_COMPACT
		if (_spw_db) {
		      (void)(_spw_db->close)(_spw_db);
		      _spw_db = (DB *)NULL;
		 }
#endif
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
	char bf[sizeof(_pw_keynum) + 1];
	int keyuid, rval, len;

	if (!_pw_db && !__initdb())
		return((struct passwd *)NULL);

#ifdef YP
	bf[0] = _PW_KEYBYNAME;
	len = strlen("+");
	bcopy("+", bf + 1, MIN(len, UT_NAMESIZE));
	key.data = (u_char *)bf;
	key.size = len + 1;

	/*
	 * If there is a user called "+", then YP is active. In that
	 * case we must sequence through the passwd file in sequence.
	 */
	if ( __hashpw(&key)) {
		char uidbuf[20];
		int r;

		for(_pw_keynum=1; _pw_keynum; _pw_keynum++) {
			bf[0] = _PW_KEYBYNUM;
			bcopy((char *)&_pw_keynum, bf + 1, sizeof(_pw_keynum));
			key.data = (u_char *)bf;
			key.size = sizeof(_pw_keynum) + 1;
			if(__hashpw(&key) == 0)
				break;
			if(strcmp(_pw_passwd.pw_name, "+") == 0) {
				if(!__ypdomain) {
					if(_yp_check(&__ypdomain) == 0) {
						continue;
					}
				}
				if(__ypcurrent) {
					free(__ypcurrent);
					__ypcurrent = NULL;
				}
				sprintf(uidbuf, "%d", uid);
				r = yp_match(__ypdomain, "passwd.byuid",
					uidbuf, strlen(uidbuf),
					&__ypcurrent, &__ypcurrentlen);
				switch(r) {
				case 0:
					break;
				default:
					free(__ypcurrent);
					__ypcurrent = NULL;
					continue;
				}
				bcopy(__ypcurrent, line, __ypcurrentlen);
				line[__ypcurrentlen] = '\0';
				if(__ypparse(&_pw_passwd, line))
					continue;
			}
			if( _pw_passwd.pw_uid == uid) {
				if (!_pw_stayopen) {
					(void)(_pw_db->close)(_pw_db);
					_pw_db = (DB *)NULL;
#ifdef PW_COMPACT
				    if (_spw_db) {
					(void)(_spw_db->close)(_spw_db);
					_spw_db = (DB *)NULL;
				    }
#endif
				}
				return &_pw_passwd;
			}
			continue;
		}
		if (!_pw_stayopen) {
			(void)(_pw_db->close)(_pw_db);
			_pw_db = (DB *)NULL;
#ifdef PW_COMPACT
			if (_spw_db) {
			     (void)(_spw_db->close)(_spw_db);
			     _spw_db = (DB *)NULL;
			}
#endif
		}
		return (struct passwd *)NULL;
	}
#endif /* YP */

	bf[0] = _PW_KEYBYUID;
	keyuid = uid;
	bcopy(&keyuid, bf + 1, sizeof(keyuid));
	key.data = (u_char *)bf;
	key.size = sizeof(keyuid) + 1;
	rval = __hashpw(&key);

	if (!_pw_stayopen) {
		(void)(_pw_db->close)(_pw_db);
		_pw_db = (DB *)NULL;
#ifdef PW_COMPACT
		if (_spw_db) {
		      (void)(_spw_db->close)(_spw_db);
		      _spw_db = (DB *)NULL;
		}
#endif
	}
	return(rval ? &_pw_passwd : (struct passwd *)NULL);
}

int
setpassent(stayopen)
	int stayopen;
{
	_pw_keynum = 0;
	_pw_stayopen = stayopen;
#ifdef YP
	__ypmode = 0;
	if(__ypcurrent)
		free(__ypcurrent);
	__ypcurrent = NULL;
#endif
	return(1);
}

void
setpwent()
{
	(void) setpassent(0);
}

void
endpwent()
{
	_pw_keynum = 0;
	if (_pw_db) {
		(void)(_pw_db->close)(_pw_db);
		_pw_db = (DB *)NULL;
#ifdef PW_COMPACT
		if (_spw_db) {
		     (void)(_spw_db->close)(_spw_db);
		     _spw_db = (DB *)NULL;
		}
#endif
	}
#ifdef YP
	__ypmode = 0;
	if(__ypcurrent)
		free(__ypcurrent);
	__ypcurrent = NULL;
#endif
}

static int
__initdb()
{
	static int warned;
	char *p;

#ifdef PW_COMPACT
	if (!geteuid()) {
	  _spw_db = dbopen(_PATH_SMP_DB, O_RDONLY, 0, DB_HASH, NULL);
	  if (!_spw_db && !warned)
		syslog(LOG_ERR, "%s: %m", _PATH_SMP_DB);
	}
	_pw_db = dbopen(_PATH_MP_DB, O_RDONLY, 0, DB_HASH, NULL);
	if (_pw_db)
		return(1);
	if (!warned)
		syslog(LOG_ERR, "%s: %m", _PATH_MP_DB);
	if (_spw_db) {
		(void)(_spw_db->close)(_spw_db);
		_spw_db = (DB *)NULL;
	}
#else
	p = (geteuid()) ? _PATH_MP_DB : _PATH_SMP_DB;
	_pw_db = dbopen(p, O_RDONLY, 0, DB_HASH, NULL);
	if (_pw_db)
		return(1);
	if (!warned)
		syslog(LOG_ERR, "%s: %m", p);
#endif
	warned = 1;
	return(0);
}

static int
__hashpw(key)
	DBT *key;
{
	register char *p, *t;
	static u_int max;
	static char *line;
	DBT data;
#ifdef PW_COMPACT
	DBT _key, *__key;
	char bf[sizeof(_pw_keynum) + 1];
#endif

	if ((_pw_db->get)(_pw_db, key, &data, 0))
		return(0);
#ifdef PW_COMPACT
	__key = key;
	if (((char *)(*__key).data)[0] != _PW_KEYBYNUM) {
		if (data.size != sizeof(_pw_keynum)) return(0);
		bf[0] = _PW_KEYBYNUM;
		bcopy(data.data, bf + 1, sizeof(_pw_keynum));
		_key.data = (u_char *)bf;
		_key.size = sizeof(_pw_keynum) + 1;
		__key = (DBT *)&_key;
		if ((_pw_db->get)(_pw_db, __key, &data, 0))
			 return(0);
	}
#endif
	p = (char *)data.data;
	if (data.size > max && !(line = realloc(line, max += 1024)))
		return(0);

	t = line;
#define	EXPAND(e)	e = t; while (*t++ = *p++);
	EXPAND(_pw_passwd.pw_name);
#ifndef PW_COMPACT
	EXPAND(_pw_passwd.pw_passwd);
#endif
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
#ifdef PW_COMPACT
	if (_spw_db && !(_spw_db->get)(_spw_db, __key, &data, 0))
	     p = (char *)data.data;
	else p = "*";
	EXPAND(_pw_passwd.pw_passwd);
#endif
	return(1);
}
