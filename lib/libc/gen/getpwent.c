/*
 * Copyright (c) 1988, 1993
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

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)getpwent.c	8.1 (Berkeley) 6/4/93";
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
#ifdef YP
static struct passwd _pw_copy;
static int _yp_enabled;			/* set true when yp enabled */
static int _pw_stepping_yp;		/* set true when stepping thru map */
#endif
static int __hashpw(), __initdb();

static int _havemaster(const char *);
static int _getyppass(struct passwd *, const char *, const char *);
static int _nextyppass(struct passwd *);

struct passwd *
getpwent()
{
	DBT key;
	char bf[sizeof(_pw_keynum) + 1];
	int rv;

	if (!_pw_db && !__initdb())
		return((struct passwd *)NULL);

#ifdef YP
	if(_pw_stepping_yp) {
		_pw_passwd = _pw_copy;
		return (_nextyppass(&_pw_passwd) ? &_pw_passwd : 0);
	}
#endif

tryagain:
	++_pw_keynum;
	bf[0] = _PW_KEYBYNUM;
	bcopy((char *)&_pw_keynum, bf + 1, sizeof(_pw_keynum));
	key.data = (u_char *)bf;
	key.size = sizeof(_pw_keynum) + 1;
	rv = __hashpw(&key);
	if(!rv) return (struct passwd *)NULL;
#ifdef YP
	if(_pw_passwd.pw_name[0] == '+' && _pw_passwd.pw_name[1]) {
		_getyppass(&_pw_passwd, &_pw_passwd.pw_name[1], 
			   "passwd.byname");
	} else if(_pw_passwd.pw_name[0] == '+') {
		_pw_copy = _pw_passwd;
		return (_nextyppass(&_pw_passwd) ? &_pw_passwd : 0);
	}
#else
	/* Ignore YP password file entries when YP is disabled. */
	if(_pw_passwd.pw_name[0] == '+') {
		goto tryagain;
	}
#endif
	return(&_pw_passwd);
}

struct passwd *
getpwnam(name)
	const char *name;
{
	DBT key;
	int len, rval;
	char bf[UT_NAMESIZE + 2];

	if (!_pw_db && !__initdb())
		return((struct passwd *)NULL);

	bf[0] = _PW_KEYBYNAME;
	len = strlen(name);
	bcopy(name, bf + 1, MIN(len, UT_NAMESIZE));
	key.data = (u_char *)bf;
	key.size = len + 1;
	rval = __hashpw(&key);

#ifdef YP
	if (!rval && _yp_enabled) {
		bf[1] = '+';
		bcopy(name, bf + 2, MIN(len, UT_NAMESIZE - 1));
		key.data = (u_char *)bf;
		key.size = len + 2;
		rval = __hashpw(&key);
		if (!rval && _yp_enabled < 0) {
			key.size = 2;
			rval = __hashpw(&key);
		}
		if(rval)
			rval = _getyppass(&_pw_passwd, name, "passwd.byname");
	}
#endif
	/*
	 * Prevent login attempts when YP is not enabled but YP entries
	 * are in /etc/master.passwd.
	 */
	if (rval && _pw_passwd.pw_name[0] == '+') rval = 0;

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

#ifdef YP
	if (!rval && _yp_enabled) {
		char ypbuf[16];	/* big enough for 32-bit uids and then some */
		snprintf(ypbuf, sizeof ypbuf, "%u", (unsigned)uid);
		rval = _getyppass(&_pw_passwd, ypbuf, "passwd.byuid");
	}
#endif
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
#ifdef YP
	_pw_stepping_yp = 0;
#endif
	_pw_stayopen = stayopen;
	return(1);
}

int
setpwent()
{
	_pw_keynum = 0;
#ifdef YP
	_pw_stepping_yp = 0;
#endif
	_pw_stayopen = 0;
	return(1);
}

void
endpwent()
{
	_pw_keynum = 0;
#ifdef YP
	_pw_stepping_yp = 0;
#endif
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
	if (_pw_db) {
#ifdef YP
		DBT key, data;
		char buf[] = { _PW_KEYYPENABLED };
		key.data = buf;
		key.size = 1;
		if ((_pw_db->get)(_pw_db, &key, &data, 0)) {
			_yp_enabled = 0;
		} else {
			/* Distinguish between old and new versions of
			   pwd_mkdb. */
			if(data.size != 1) {
				_yp_enabled = -1;
			} else {
				_yp_enabled = (int)*((char *)data.data) - 2;
			}
		}
#endif
		return(1);
	}
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

	/*
	 * XXX The pw_fields member of _pw_passwd needs to be cleared
	 * at some point since __hashpw() can be called several times in
	 * a single program. If we leave here after the second invokation
	 * with garbage data in pw_fields, it can totally screw up NIS
	 * lookups (the pw_breakout functions only populate the pw_passwd
	 * structure if the pw_fields bits are clear).
	 */
	if ((_pw_db->get)(_pw_db, key, &data, 0)) {
		if (_pw_passwd.pw_fields)
			_pw_passwd.pw_fields = 0;
		return(0);
	}
	p = (char *)data.data;
	if (data.size > max && !(line = realloc(line, max += 1024))) {
		if (_pw_passwd.pw_fields)
			_pw_passwd.pw_fields = 0;
		return(0);
	}

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
	bcopy(p, (char *)&_pw_passwd.pw_fields, sizeof _pw_passwd.pw_fields);
	p += sizeof _pw_passwd.pw_fields;
	return(1);
}

#ifdef YP
static void
_pw_breakout_yp(struct passwd *pw, char *result)
{
	char *s;

	s = strsep(&result, ":"); /* name */
	if(!(pw->pw_fields & _PWF_NAME) || (pw->pw_name[0] == '+')) {
		pw->pw_name = s;
		pw->pw_fields |= _PWF_NAME;
	}

	s = strsep(&result, ":"); /* password */
	if(!(pw->pw_fields & _PWF_PASSWD)) {
		pw->pw_passwd = s;
		pw->pw_fields |= _PWF_PASSWD;
	}

	s = strsep(&result, ":"); /* uid */
	if(!(pw->pw_fields & _PWF_UID)) {
		pw->pw_uid = atoi(s);
		pw->pw_fields |= _PWF_UID;
	}

	s = strsep(&result, ":"); /* gid */
	if(!(pw->pw_fields & _PWF_GID))  {
		pw->pw_gid = atoi(s);
		pw->pw_fields |= _PWF_GID;
	}

	s = strsep(&result, ":"); /* gecos */
	if(!(pw->pw_fields & _PWF_GECOS)) {
		pw->pw_gecos = s;
		pw->pw_fields |= _PWF_GECOS;
	}

	s = strsep(&result, ":"); /* dir */
	if(!(pw->pw_fields & _PWF_DIR)) {
		pw->pw_dir = s;
		pw->pw_fields |= _PWF_DIR;
	}

	s = strsep(&result, ":"); /* shell */
	if(!(pw->pw_fields & _PWF_SHELL)) {
		pw->pw_shell = s;
		pw->pw_fields |= _PWF_SHELL;
	}
}

static void
_masterpw_breakout_yp(struct passwd *pw, char *result)
{
	char *s;

	s = strsep(&result, ":"); /* name */
	if(!(pw->pw_fields & _PWF_NAME) || (pw->pw_name[0] == '+')) {
		pw->pw_name = s;
		pw->pw_fields |= _PWF_NAME;
	}

	s = strsep(&result, ":"); /* password */
	if(!(pw->pw_fields & _PWF_PASSWD)) {
		pw->pw_passwd = s;
		pw->pw_fields |= _PWF_PASSWD;
	}

	s = strsep(&result, ":"); /* uid */
	if(!(pw->pw_fields & _PWF_UID)) {
		pw->pw_uid = atoi(s);
		pw->pw_fields |= _PWF_UID;
	}

	s = strsep(&result, ":"); /* gid */
	if(!(pw->pw_fields & _PWF_GID))  {
		pw->pw_gid = atoi(s);
		pw->pw_fields |= _PWF_GID;
	}

	s = strsep(&result, ":"); /* class */
	if(!(pw->pw_fields & _PWF_CLASS))  {
		pw->pw_class = s;
		pw->pw_fields |= _PWF_CLASS;
	}

	s = strsep(&result, ":"); /* change */
	if(!(pw->pw_fields & _PWF_CHANGE))  {
		pw->pw_change = atol(s);
		pw->pw_fields |= _PWF_CHANGE;
	}

	s = strsep(&result, ":"); /* expire */
	if(!(pw->pw_fields & _PWF_EXPIRE))  {
		pw->pw_expire = atol(s);
		pw->pw_fields |= _PWF_EXPIRE;
	}

	s = strsep(&result, ":"); /* gecos */
	if(!(pw->pw_fields & _PWF_GECOS)) {
		pw->pw_gecos = s;
		pw->pw_fields |= _PWF_GECOS;
	}

	s = strsep(&result, ":"); /* dir */
	if(!(pw->pw_fields & _PWF_DIR)) {
		pw->pw_dir = s;
		pw->pw_fields |= _PWF_DIR;
	}

	s = strsep(&result, ":"); /* shell */
	if(!(pw->pw_fields & _PWF_SHELL)) {
		pw->pw_shell = s;
		pw->pw_fields |= _PWF_SHELL;
	}
}

static char *_pw_yp_domain;

static int
_havemaster(const char *_pw_yp_domain)
{
	char *result;
	static char *key;
	int resultlen;
	static int keylen;
	
	if (yp_first(_pw_yp_domain, "master.passwd.byname",
		      &key, &keylen, &result, &resultlen))
		return 0;
	
	return 1;
}

static int
_getyppass(struct passwd *pw, const char *name, const char *map)
{
	char *result, *s;
	static char resultbuf[1024];
	int resultlen;
	char mastermap[1024];
	int gotmaster = 0;

	if(!_pw_yp_domain) {
		if(yp_get_default_domain(&_pw_yp_domain))
		  return 0;
	}

	sprintf(mastermap,"%s",map);

	/* Don't even bother with this if we aren't root. */
	if (!geteuid())
		if (_havemaster(_pw_yp_domain)) {
			sprintf(mastermap,"master.%s", map);
			gotmaster++;
		}

	if(yp_match(_pw_yp_domain, &mastermap, name, strlen(name), 
		    &result, &resultlen))
		return 0;

	s = strchr(result, '\n');
	if(s) *s = '\0';

	if(resultlen >= sizeof resultbuf) return 0;
	strcpy(resultbuf, result);
	result = resultbuf;
	if (gotmaster)
		_masterpw_breakout_yp(pw, resultbuf);
	else
		_pw_breakout_yp(pw, resultbuf);

	return 1;
}

static int
_nextyppass(struct passwd *pw)
{
	static char *key;
	static int keylen;
	char *lastkey, *result;
	static char resultbuf[1024];
	int resultlen;
	int rv;
	char *map = "passwd.byname";
	int gotmaster = 0;

	if(!_pw_yp_domain) {
		if(yp_get_default_domain(&_pw_yp_domain))
		  return 0;
	}

	/* Don't even bother with this if we aren't root. */
	if (!geteuid())
		if(_havemaster(_pw_yp_domain)) {
			map = "master.passwd.byname";
			gotmaster++;
		}

	if(!_pw_stepping_yp) {
		if(key) free(key);
			rv = yp_first(_pw_yp_domain, map,
				      &key, &keylen, &result, &resultlen);
		if(rv) {
			return 0;
		}
		_pw_stepping_yp = 1;
		goto unpack;
	} else {
tryagain:
		lastkey = key;
			rv = yp_next(_pw_yp_domain, map, key, keylen,
			     &key, &keylen, &result, &resultlen);
		free(lastkey);
unpack:
		if(rv) {
			_pw_stepping_yp = 0;
			return 0;
		}

		if(resultlen > sizeof(resultbuf)) {
			free(result);
			goto tryagain;
		}

		strcpy(resultbuf, result);
		free(result);
		if(result = strchr(resultbuf, '\n')) *result = '\0';
		if (gotmaster)
			_masterpw_breakout_yp(pw, resultbuf);
		else
			_pw_breakout_yp(pw, resultbuf);
	}
	return 1;
}
		
#endif /* YP */
