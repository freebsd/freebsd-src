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
static char sccsid[] = "@(#)getpwent.c	8.2 (Berkeley) 4/27/95";
#endif /* LIBC_SCCS and not lint */

#include <stdio.h>
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
#include <grp.h>

extern void setnetgrent __P(( char * ));
extern int getnetgrent __P(( char **, char **, char ** ));
extern int innetgr __P(( const char *, const char *, const char *, const char * ));

/*
 * The lookup techniques and data extraction code here must be kept
 * in sync with that in `pwd_mkdb'.
 */

static struct passwd _pw_passwd;	/* password structure */
static DB *_pw_db;			/* password database */
static int _pw_keynum;			/* key counter */
static int _pw_stayopen;		/* keep fd's open */
#ifdef YP
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>

static struct passwd _pw_copy;
static DBT empty = { NULL, 0 };
static DB *_ypcache = (DB *)NULL;
static int _yp_exclusions = 0;
static int _yp_enabled;			/* set true when yp enabled */
static int _pw_stepping_yp;		/* set true when stepping thru map */
static char _ypnam[YPMAXRECORD];
#define YP_HAVE_MASTER 2
#define YP_HAVE_ADJUNCT 1
#define YP_HAVE_NONE 0
static int _gotmaster;
static char *_pw_yp_domain;
static inline int unwind __P(( char * ));
static inline void _ypinitdb __P(( void ));
static int _havemaster __P((char *));
static int _getyppass __P((struct passwd *, const char *, const char * ));
static int _nextyppass __P((struct passwd *));
#endif
static int __hashpw(), __initdb();

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
		if (unwind((char *)&_ypnam))
			return(&_pw_passwd);
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
	if(_pw_passwd.pw_name[0] == '+' || _pw_passwd.pw_name[0] == '-') {
		bzero((char *)&_ypnam, sizeof(_ypnam));
		bcopy(_pw_passwd.pw_name, _ypnam,
			strlen(_pw_passwd.pw_name));
		_pw_copy = _pw_passwd;
		if (unwind((char *)&_ypnam) == 0)
			goto tryagain;
		else
			return(&_pw_passwd);
	}
#else
	/* Ignore YP password file entries when YP is disabled. */
	if(_pw_passwd.pw_name[0] == '+' || _pw_passwd.pw_name[0] == '-') {
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
	if (!rval && _yp_enabled)
		rval = _getyppass(&_pw_passwd, name, "passwd.byname");
#endif
	/*
	 * Prevent login attempts when YP is not enabled but YP entries
	 * are in /etc/master.passwd.
	 */
	if (rval && (_pw_passwd.pw_name[0] == '+'||
			_pw_passwd.pw_name[0] == '-')) rval = 0;

	endpwent();
	return(rval ? &_pw_passwd : (struct passwd *)NULL);
}

struct passwd *
getpwuid(uid)
	uid_t uid;
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
	/*
	 * Prevent login attempts when YP is not enabled but YP entries
	 * are in /etc/master.passwd.
	 */
	if (rval && (_pw_passwd.pw_name[0] == '+'||
			_pw_passwd.pw_name[0] == '-')) rval = 0;

	endpwent();
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

void
setpwent()
{
	(void)setpassent(0);
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
#ifdef YP
	if (_ypcache) {
		(void)(_ypcache->close)(_ypcache);
		_ypcache = (DB *)NULL;
		_yp_exclusions = 0;
	}
#endif
}

static int
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
			_yp_enabled = (int)*((char *)data.data) - 2;
		/* Don't even bother with this if we aren't root. */
			if (!geteuid()) {
				if (!_pw_yp_domain)
					if (yp_get_default_domain(&_pw_yp_domain))
					return(1);
				_gotmaster = _havemaster(_pw_yp_domain);
			} else _gotmaster = YP_HAVE_NONE;
			if (!_ypcache)
				_ypinitdb();
		}
#endif
		return(1);
	}
	if (!warned++)
		syslog(LOG_ERR, "%s: %m", p);
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

	if ((_pw_db->get)(_pw_db, key, &data, 0))
		return(0);
	p = (char *)data.data;

	/* Increase buffer size for long lines if necessary. */
	if (data.size > max) {
		max = data.size + 1024;
		if (!(line = realloc(line, max)))
			return(0);
	}

	/* THIS CODE MUST MATCH THAT IN pwd_mkdb. */
	t = line;
#define	EXPAND(e)	e = t; while ( (*t++ = *p++) );
#define	SCALAR(v)	memmove(&(v), p, sizeof v); p += sizeof v
	EXPAND(_pw_passwd.pw_name);
	EXPAND(_pw_passwd.pw_passwd);
	SCALAR(_pw_passwd.pw_uid);
	SCALAR(_pw_passwd.pw_gid);
	SCALAR(_pw_passwd.pw_change);
	EXPAND(_pw_passwd.pw_class);
	EXPAND(_pw_passwd.pw_gecos);
	EXPAND(_pw_passwd.pw_dir);
	EXPAND(_pw_passwd.pw_shell);
	SCALAR(_pw_passwd.pw_expire);
	bcopy(p, (char *)&_pw_passwd.pw_fields, sizeof _pw_passwd.pw_fields);
	p += sizeof _pw_passwd.pw_fields;
	return(1);
}

#ifdef YP

/*
 * Create a DB hash database in memory. Bet you didn't know you
 * could do a dbopen() will a NULL filename, did you.
 */
static inline void _ypinitdb()
{
	if (_ypcache == (DB *)NULL)
		_ypcache = dbopen(NULL, O_RDWR, 600, DB_HASH, NULL);
	return;
}

/*
 * See if a user is in the blackballed list.
 */
static inline int lookup(name)
	char *name;
{
	DBT key;

	if (!_yp_exclusions)
		return(0);

	key.data = name;
	key.size = strlen(name);

	if ((_ypcache->get)(_ypcache, &key, &empty, 0)) {
		return(0);
	}

	return(1);
}

/*
 * Store a blackballed user in an in-core hash database.
 */
static inline void store(key)
	char *key;
{
	DBT lkey;
/*
	if (lookup(key))
		return;
*/

	_yp_exclusions = 1;

	lkey.data = key;
	lkey.size = strlen(key);

	(void)(_ypcache->put)(_ypcache, &lkey, &empty, R_NOOVERWRITE);
}

/*
 * Parse the + entries in the password database and do appropriate
 * NIS lookups. While ugly to look at, this is optimized to do only
 * as many lookups as are absolutely necessary in any given case.
 * Basically, the getpwent() function will feed us + and - lines
 * as they appear in the database. For + lines, we do netgroup/group
 * and user lookups to find all usernames that match the rule and
 * extract them from the NIS passwd maps. For - lines, we save the
 * matching names in a database and a) exlude them, and b) make sure
 * we don't consider them when processing other + lines that appear
 * later.
 */
static inline int unwind(grp)
	char *grp;
{
	char *user, *host, *domain;
	static int latch = 0;
	static struct group *gr = NULL;
	int rv = 0;

	if (grp[0] == '+') {
		if (strlen(grp) == 1) {
			return(_nextyppass(&_pw_passwd));
		}
		if (grp[1] == '@') {
			_pw_stepping_yp = 1;
grpagain:
			if (gr != NULL) {
				if (*gr->gr_mem != NULL) {
					if (lookup(*gr->gr_mem)) {
						gr->gr_mem++;
						goto grpagain;
					}
					rv = _getyppass(&_pw_passwd,
							*gr->gr_mem,
							"passwd.byname");
					gr->gr_mem++;
					return(rv);
				} else {
					endgrent();
					latch = 0;
					gr = NULL;
					return(0);
				}
			}
			if (!latch) {
				setnetgrent(grp+2);
				latch++;
			}
again:
			if (getnetgrent(&host, &user, &domain) == 0) {
				if ((gr = getgrnam(grp+2)) != NULL)
					goto grpagain;
				latch = 0;
				_pw_stepping_yp = 0;
				return(0);
			} else {
				if (lookup(user))
					goto again;
				if (_getyppass(&_pw_passwd, user,
							"passwd.byname"))
					return(1);
				else
					goto again;
			}
		} else {
			if (lookup(grp+1))
				return(0);
			return(_getyppass(&_pw_passwd, grp+1, "passwd.byname"));
		}
	} else {
		if (grp[1] == '@') {
			setnetgrent(grp+2);
			rv = 0;
			while(getnetgrent(&host, &user, &domain) != 0) {
				store(user);
				rv++;
			}
			if (!rv && (gr = getgrnam(grp+2)) != NULL) {
				while(gr->gr_mem) {
					store(gr->gr_mem);
					gr->gr_mem++;
				}
			}
		} else {
			store(grp+1);
		}
	}
	return(0);
}

/*
 * See if a user is a member of a particular group.
 */
static inline int ingr(grp, name)
	char *grp;
	char *name;
{
	register struct group *gr;

	if ((gr = getgrnam(grp)) == NULL)
		return(0);

	while(*gr->gr_mem) {
		if (!strcmp(*gr->gr_mem, name)) {
			endgrent();
			return(1);
		}
		gr->gr_mem++;
	}

	endgrent();
	return(0);
}

/*
 * Check a user against the +@netgroup/-@netgroup lines listed in
 * the local password database. Also checks +user/-user lines.
 * If no netgroup exists that matches +@netgroup/-@netgroup,
 * try searching regular groups with the same name.
 */
static inline int verf(name)
	char *name;
{
	DBT key;
	char bf[sizeof(_pw_keynum) + 1];
	int keynum = 0;

again:
	++keynum;
	bf[0] = _PW_KEYYPBYNUM;
	bcopy((char *)&keynum, bf + 1, sizeof(keynum));
	key.data = (u_char *)bf;
	key.size = sizeof(keynum) + 1;
	if (!__hashpw(&key)) {
		/* Try again using old format */
		bf[0] = _PW_KEYBYNUM;
		bcopy((char *)&keynum, bf + 1, sizeof(keynum));
		key.data = (u_char *)bf;
		if (!__hashpw(&key))
			return(0);
	}
	if (_pw_passwd.pw_name[0] != '+' && (_pw_passwd.pw_name[0] != '-'))
		goto again;
	if (_pw_passwd.pw_name[0] == '+') {
		if (strlen(_pw_passwd.pw_name) == 1) /* Wildcard */
			return(1);
		if (_pw_passwd.pw_name[1] == '@') {
			if ((innetgr(_pw_passwd.pw_name+2, NULL, name,
							_pw_yp_domain) ||
			    ingr(_pw_passwd.pw_name+2, name)) && !lookup(name))
				return(1);
			else
				goto again;
		} else {
			if (!strcmp(name, _pw_passwd.pw_name+1) &&
								!lookup(name))
				return(1);
			else
				goto again;
		}
	}
	if (_pw_passwd.pw_name[0] == '-') {
		/* Note that a minus wildcard is a no-op. */
		if (_pw_passwd.pw_name[1] == '@') {
			if (innetgr(_pw_passwd.pw_name+2, NULL, name,
							_pw_yp_domain) ||
			    ingr(_pw_passwd.pw_name+2, name)) {
				store(name);
				return(0);
			} else
				goto again;
		} else {
			if (!strcmp(name, _pw_passwd.pw_name+1)) {
				store(name);
				return(0);
			} else
				goto again;
		}
		
	}
	return(0);
}

static char * _get_adjunct_pw(name)
	char *name;
{
	static char adjunctbuf[YPMAXRECORD+2];
	int rval;
	char *result;
	int resultlen;
	char *map = "passwd.adjunct.byname";
	char *s;

	if ((rval = yp_match(_pw_yp_domain, map, name, strlen(name),
		    &result, &resultlen)))
		return(NULL);

	strncpy(adjunctbuf, result, resultlen);
	adjunctbuf[resultlen] = '\0';
	free(result);
	result = (char *)&adjunctbuf;

	/* Don't care about the name. */
	if ((s = strsep(&result, ":")) == NULL)
		return (NULL); /* name */
	if ((s = strsep(&result, ":")) == NULL)
		return (NULL); /* password */

	return(s);
}

static int
_pw_breakout_yp(struct passwd *pw, char *res, int resultlen, int master)
{
	char *s, *result;
	static char resbuf[YPMAXRECORD+2];

	/*
	 * Be triple, ultra super-duper paranoid: reject entries
	 * that start with a + or -. yp_mkdb and /var/yp/Makefile
	 * are _both_ supposed to strip these out, but you never
	 * know.
	 */
	if (*res == '+' || *res == '-')
		return 0;

	/*
	 * The NIS protocol definition limits the size of an NIS
	 * record to YPMAXRECORD bytes. We need to do a copy to
	 * a static buffer here since the memory pointed to by
	 * res will be free()ed when this function returns.
	 */
	strncpy((char *)&resbuf, res, resultlen);
	resbuf[resultlen] = '\0';
	result = (char *)&resbuf;

	/*
	 * XXX Sanity check: make sure all fields are valid (no NULLs).
	 * If we find a badly formatted entry, we punt.
	 */
	if ((s = strsep(&result, ":")) == NULL) return 0; /* name */
	/*
	 * We don't care what pw_fields says: we _always_ want the
	 * username returned to us by NIS.
	 */
	pw->pw_name = s;
	pw->pw_fields |= _PWF_NAME;

	if ((s = strsep(&result, ":")) == NULL) return 0; /* password */
	if(!(pw->pw_fields & _PWF_PASSWD)) {
		/* SunOS passwd.adjunct hack */
		if (master == YP_HAVE_ADJUNCT && strstr(s, "##") != NULL) {
			char *realpw;
			realpw = _get_adjunct_pw(pw->pw_name);
			if (realpw == NULL)
				pw->pw_passwd = s;
			else
				pw->pw_passwd = realpw;
		} else {
			pw->pw_passwd = s;
		}
		pw->pw_fields |= _PWF_PASSWD;
	}

	if ((s = strsep(&result, ":")) == NULL) return 0; /* uid */
	if(!(pw->pw_fields & _PWF_UID)) {
		pw->pw_uid = atoi(s);
		pw->pw_fields |= _PWF_UID;
	}

	if ((s = strsep(&result, ":")) == NULL) return 0; /* gid */
	if(!(pw->pw_fields & _PWF_GID))  {
		pw->pw_gid = atoi(s);
		pw->pw_fields |= _PWF_GID;
	}

	if (master == YP_HAVE_MASTER) {
		if ((s = strsep(&result, ":")) == NULL) return 0; /* class */
		if(!(pw->pw_fields & _PWF_CLASS))  {
			pw->pw_class = s;
			pw->pw_fields |= _PWF_CLASS;
		}

		if ((s = strsep(&result, ":")) == NULL) return 0; /* change */
		if(!(pw->pw_fields & _PWF_CHANGE))  {
			pw->pw_change = atol(s);
			pw->pw_fields |= _PWF_CHANGE;
		}

		if ((s = strsep(&result, ":")) == NULL) return 0; /* expire */
		if(!(pw->pw_fields & _PWF_EXPIRE))  {
			pw->pw_expire = atol(s);
			pw->pw_fields |= _PWF_EXPIRE;
		}
	}

	if ((s = strsep(&result, ":")) == NULL) return 0; /* gecos */
	if(!(pw->pw_fields & _PWF_GECOS)) {
		pw->pw_gecos = s;
		pw->pw_fields |= _PWF_GECOS;
	}

	if ((s = strsep(&result, ":")) == NULL) return 0; /* dir */
	if(!(pw->pw_fields & _PWF_DIR)) {
		pw->pw_dir = s;
		pw->pw_fields |= _PWF_DIR;
	}

	if ((s = strsep(&result, ":")) == NULL) return 0; /* shell */
	if(!(pw->pw_fields & _PWF_SHELL)) {
		pw->pw_shell = s;
		pw->pw_fields |= _PWF_SHELL;
	}

	/* Be consistent. */
	if ((s = strchr(pw->pw_shell, '\n'))) *s = '\0';

	return 1;
}

static int
_havemaster(char *_yp_domain)
{
	int order;
	int rval;

	if (!(rval = yp_order(_yp_domain, "master.passwd.byname", &order)))
		return(YP_HAVE_MASTER);

	/*
	 * NIS+ in YP compat mode doesn't support
	 * YPPROC_ORDER -- no point in continuing.
	 */
	if (rval == YPERR_YPERR)
		return(YP_HAVE_NONE);

	/* master.passwd doesn't exist -- try passwd.adjunct */
	if (rval == YPERR_MAP) {
		rval = yp_order(_yp_domain, "passwd.adjunct.byname", &order);
		if (!rval)
			return(YP_HAVE_ADJUNCT);
	}

	return (YP_HAVE_NONE);
}

static int
_getyppass(struct passwd *pw, const char *name, const char *map)
{
	char *result, *s;
	int resultlen;
	int rv;
	char mastermap[YPMAXRECORD];

	if(!_pw_yp_domain) {
		if(yp_get_default_domain(&_pw_yp_domain))
		  return 0;
	}

	sprintf(mastermap,"%s",map);

	if (_gotmaster == YP_HAVE_MASTER)
		sprintf(mastermap,"master.%s", map);

	if(yp_match(_pw_yp_domain, (char *)&mastermap, name, strlen(name),
		    &result, &resultlen))
		return 0;

	if (!_pw_stepping_yp) {
		s = strchr(result, ':');
		if (s) {
			*s = '\0';
		} else {
			/* Must be a malformed entry if no colons. */
			free(result);
			return(0);
		}

		if (!verf(result)) {
			*s = ':';
			free(result);
			return(0);
		}

		*s = ':'; /* Put back the colon we previously replaced with a NUL. */
	}

	rv = _pw_breakout_yp(pw, result, resultlen, _gotmaster);
	free(result);
	return(rv);
}

static int
_nextyppass(struct passwd *pw)
{
	static char *key;
	static int keylen;
	char *lastkey, *result, *s;
	int resultlen;
	int rv;
	char *map = "passwd.byname";

	if(!_pw_yp_domain) {
		if(yp_get_default_domain(&_pw_yp_domain))
		  return 0;
	}

	if (_gotmaster == YP_HAVE_MASTER)
		map = "master.passwd.byname";

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

		s = strchr(result, ':');
		if (s) {
			*s = '\0';
		} else {
			/* Must be a malformed entry if no colons. */
			free(result);
			goto tryagain;
		}

		if (lookup(result)) {
			*s = ':';
			free(result);
			goto tryagain;
		}

		*s = ':'; /* Put back the colon we previously replaced with a NUL. */
		if (_pw_breakout_yp(pw, result, resultlen, _gotmaster)) {
			free(result);
			return(1);
		} else {
			free(result);
			goto tryagain;
		}
	}
}

#endif /* YP */
