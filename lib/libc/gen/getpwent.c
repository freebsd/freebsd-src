/*	$NetBSD: getpwent.c,v 1.40.2.2 1999/04/27 22:09:45 perry Exp $	*/

/*
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 * Portions Copyright (c) 1994, 1995, Jason Downs.  All rights reserved.
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

#if 0
#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)getpwent.c	8.2 (Berkeley) 4/27/95";
#endif /* LIBC_SCCS and not lint */
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "un-namespace.h"
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
#include <nsswitch.h>
#ifdef HESIOD
#include <hesiod.h>
#endif
#ifdef YP
#include <machine/param.h>
#include <stdio.h>
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>
#endif
#include "un-namespace.h"

extern void setnetgrent __P((char *));
extern int getnetgrent __P((char **, char **, char **));
extern int innetgr __P((const char *, const char *, const char *, const char *));

#include "pw_scan.h"

#if defined(YP) || defined(HESIOD)
#define _PASSWD_COMPAT
#endif

/*
 * The lookup techniques and data extraction code here must be kept
 * in sync with that in `pwd_mkdb'.
 */

static struct passwd _pw_passwd = { "", "", 0, 0, 0, "", "", "", "", 0, 0 };
static DB *_pw_db;			/* password database */
static int _pw_keynum;			/* key counter. no more records if -1 */
static int _pw_stayopen;		/* keep fd's open */

static int __hashpw __P((DBT *));
static int __initdb __P((void));

static const ns_src compatsrc[] = {
	{ NSSRC_COMPAT, NS_SUCCESS },
	{ 0 }
};

#ifdef YP
static char     *__ypcurrent, *__ypdomain;
static int      __ypcurrentlen;
static int	_pw_ypdone;		/* non-zero if no more yp records */
#endif

#ifdef HESIOD
static int	_pw_hesnum;		/* hes counter. no more records if -1 */
#endif

#ifdef _PASSWD_COMPAT
enum _pwmode { PWMODE_NONE, PWMODE_FULL, PWMODE_USER, PWMODE_NETGRP };
static enum _pwmode __pwmode;

enum _ypmap { YPMAP_NONE, YPMAP_ADJUNCT, YPMAP_MASTER };

static struct passwd	*__pwproto = (struct passwd *)NULL;
static int		 __pwproto_flags;
static char		 line[1024];
static long		 prbuf[1024 / sizeof(long)];
static DB		*__pwexclude = (DB *)NULL;
 
static int	__pwexclude_add __P((const char *));
static int	__pwexclude_is __P((const char *));
static void	__pwproto_set __P((void));
static int	__ypmaptype __P((void));
static int	__pwparse __P((struct passwd *, char *));

	/* macros for deciding which YP maps to use. */
#define PASSWD_BYNAME	(__ypmaptype() == YPMAP_MASTER \
			    ? "master.passwd.byname" : "passwd.byname")
#define PASSWD_BYUID	(__ypmaptype() == YPMAP_MASTER \
			    ? "master.passwd.byuid" : "passwd.byuid")

/*
 * add a name to the compat mode exclude list
 */
static int
__pwexclude_add(name)
	const char *name;
{
	DBT key;
	DBT data;

	/* initialize the exclusion table if needed. */
	if(__pwexclude == (DB *)NULL) {
		__pwexclude = dbopen(NULL, O_RDWR, 600, DB_HASH, NULL);
		if(__pwexclude == (DB *)NULL)
			return 1;
	}

	/* set up the key */
	key.size = strlen(name);
	/* LINTED key does not get modified */
	key.data = (char *)name;

	/* data is nothing. */
	data.data = NULL;
	data.size = 0;

	/* store it */
	if((__pwexclude->put)(__pwexclude, &key, &data, 0) == -1)
		return 1;
	
	return 0;
}

/*
 * test if a name is on the compat mode exclude list
 */
static int
__pwexclude_is(name)
	const char *name;
{
	DBT key;
	DBT data;

	if(__pwexclude == (DB *)NULL)
		return 0;	/* nothing excluded */

	/* set up the key */
	key.size = strlen(name);
	/* LINTED key does not get modified */
	key.data = (char *)name;

	if((__pwexclude->get)(__pwexclude, &key, &data, 0) == 0)
		return 1;	/* excluded */
	
	return 0;
}

/*
 * Setup the compat mode prototype template that may be used in
 * __pwparse.  Only pw_passwd, pw_uid, pw_gid, pw_gecos, pw_dir, and
 * pw_shell are used.  The other fields are zero'd.
 */
static void
__pwproto_set()
{
	char *ptr;
	struct passwd *pw = &_pw_passwd;

	/* make this the new prototype */
	ptr = (char *)(void *)prbuf;

	/* first allocate the struct. */
	__pwproto = (struct passwd *)(void *)ptr;
	ptr += sizeof(struct passwd);
	memset(__pwproto, 0, sizeof(*__pwproto));

	__pwproto_flags = 0;

	/* password */
	if(pw->pw_passwd && (pw->pw_passwd)[0]) {
		ptr = (char *)ALIGN((u_long)ptr);
		memmove(ptr, pw->pw_passwd, strlen(pw->pw_passwd) + 1);
		__pwproto->pw_passwd = ptr;
		ptr += (strlen(pw->pw_passwd) + 1);
		__pwproto_flags |= _PWF_PASSWD;
	} 

	/* uid, gid */
	if (pw->pw_fields & _PWF_UID) {
		__pwproto->pw_uid = pw->pw_uid;
		__pwproto_flags |= _PWF_UID;
	}
	if (pw->pw_fields & _PWF_GID) {
		__pwproto->pw_gid = pw->pw_gid;
		__pwproto_flags |= _PWF_GID;
	}

	/* gecos */
	if(pw->pw_gecos && (pw->pw_gecos)[0]) {
		ptr = (char *)ALIGN((u_long)ptr);
		memmove(ptr, pw->pw_gecos, strlen(pw->pw_gecos) + 1);
		__pwproto->pw_gecos = ptr;
		ptr += (strlen(pw->pw_gecos) + 1);
		__pwproto_flags |= _PWF_GECOS;
	}
	
	/* dir */
	if(pw->pw_dir && (pw->pw_dir)[0]) {
		ptr = (char *)ALIGN((u_long)ptr);
		memmove(ptr, pw->pw_dir, strlen(pw->pw_dir) + 1);
		__pwproto->pw_dir = ptr;
		ptr += (strlen(pw->pw_dir) + 1);
		__pwproto_flags |= _PWF_DIR;
	}

	/* shell */
	if(pw->pw_shell && (pw->pw_shell)[0]) {
		ptr = (char *)ALIGN((u_long)ptr);
		memmove(ptr, pw->pw_shell, strlen(pw->pw_shell) + 1);
		__pwproto->pw_shell = ptr;
		ptr += (strlen(pw->pw_shell) + 1);
		__pwproto_flags |= _PWF_SHELL;
	}
}

static int
__ypmaptype()
{
	static int maptype = -1;
	int order, r;

	if (maptype != -1)
		return (maptype);

	maptype = YPMAP_NONE;
	if (geteuid() != 0)
		return (maptype);

	if (!__ypdomain) {
		if( _yp_check(&__ypdomain) == 0)
			return (maptype);
	}

	r = yp_order(__ypdomain, "master.passwd.byname", &order);
	if (r == 0) {
		maptype = YPMAP_MASTER;
		return (maptype);
	}

	/*
	 * NIS+ in YP compat mode doesn't support
	 * YPPROC_ORDER -- no point in continuing.
	 */
	if (r == YPERR_YPERR)
		return (maptype);

	/* master.passwd doesn't exist -- try passwd.adjunct */
	if (r == YPERR_MAP) {
		r = yp_order(__ypdomain, "passwd.adjunct.byname", &order);
		if (r == 0)
			maptype = YPMAP_ADJUNCT;
		return (maptype);
	}

	return (maptype);
}

/*
 * parse a passwd file line (from NIS or HESIOD).
 * assumed to be `old-style' if maptype != YPMAP_MASTER.
 */
static int
__pwparse(pw, s)
	struct passwd *pw;
	char *s;
{
	static char adjunctpw[YPMAXRECORD + 2];
	int flags, maptype;

	maptype = __ypmaptype();
	flags = 0;
	if (maptype == YPMAP_MASTER)
		flags |= _PWSCAN_MASTER;
	if (! __pw_scan(s, pw, flags))
		return 1;

	/* now let the prototype override, if set. */
	if(__pwproto != (struct passwd *)NULL) {
#ifdef PW_OVERRIDE_PASSWD
		if(__pwproto_flags & _PWF_PASSWD)
			pw->pw_passwd = __pwproto->pw_passwd;
#endif
		if(__pwproto_flags & _PWF_UID)
			pw->pw_uid = __pwproto->pw_uid;
		if(__pwproto_flags & _PWF_GID)
			pw->pw_gid = __pwproto->pw_gid;
		if(__pwproto_flags & _PWF_GECOS)
			pw->pw_gecos = __pwproto->pw_gecos;
		if(__pwproto_flags & _PWF_DIR)
			pw->pw_dir = __pwproto->pw_dir;
		if(__pwproto_flags & _PWF_SHELL)
			pw->pw_shell = __pwproto->pw_shell;
	}
	if ((maptype == YPMAP_ADJUNCT) &&
	    (strstr(pw->pw_passwd, "##") != NULL)) {
		char *data, *bp;
		int datalen;

		if (yp_match(__ypdomain, "passwd.adjunct.byname", pw->pw_name,
		    (int)strlen(pw->pw_name), &data, &datalen) == 0) {
			if (datalen > sizeof(adjunctpw) - 1)
				datalen = sizeof(adjunctpw) - 1;
			strncpy(adjunctpw, data, (size_t)datalen);

				/* skip name to get password */
			if ((bp = strsep(&data, ":")) != NULL &&
			    (bp = strsep(&data, ":")) != NULL)
				pw->pw_passwd = bp;
		}
	}
	return 0;
}
#endif /* _PASSWD_COMPAT */

/*
 * local files implementation of getpw*()
 * varargs: type, [ uid (type == _PW_KEYBYUID) | name (type == _PW_KEYBYNAME) ]
 */
static int	_local_getpw __P((void *, void *, va_list));

/*ARGSUSED*/
static int
_local_getpw(rv, cb_data, ap)
	void	*rv;
	void	*cb_data;
	va_list	 ap;
{
	DBT		 key;
	char		 bf[/*CONSTCOND*/ MAX(MAXLOGNAME, sizeof(_pw_keynum)) + 1];
	uid_t		 uid;
	int		 search, len, rval;
	const char	*name;

	if (!_pw_db && !__initdb())
		return NS_UNAVAIL;

	search = va_arg(ap, int);
	bf[0] = search;
	switch (search) {
	case _PW_KEYBYNUM:
		if (_pw_keynum == -1)
			return NS_NOTFOUND;	/* no more local records */
		++_pw_keynum;
		memmove(bf + 1, &_pw_keynum, sizeof(_pw_keynum));
		key.size = sizeof(_pw_keynum) + 1;
		break;
	case _PW_KEYBYNAME:
		name = va_arg(ap, const char *);
		len = strlen(name);
		if (len > sizeof(bf) - 1)
			return NS_NOTFOUND;
		memmove(bf + 1, name, len);
		key.size = len + 1;
		break;
	case _PW_KEYBYUID:
		uid = va_arg(ap, uid_t);
		memmove(bf + 1, &uid, sizeof(len));
		key.size = sizeof(uid) + 1;
		break;
	default:
		abort();
	}

	key.data = (u_char *)bf;
	rval = __hashpw(&key);
	if (rval == NS_NOTFOUND && search == _PW_KEYBYNUM)
		_pw_keynum = -1;	/* flag `no more local records' */

	if (!_pw_stayopen && (search != _PW_KEYBYNUM)) {
		(void)(_pw_db->close)(_pw_db);
		_pw_db = (DB *)NULL;
	}
	return (rval);
}

#ifdef HESIOD
/*
 * hesiod implementation of getpw*()
 * varargs: type, [ uid (type == _PW_KEYBYUID) | name (type == _PW_KEYBYNAME) ]
 */
static int	_dns_getpw __P((void *, void *, va_list));

/*ARGSUSED*/
static int
_dns_getpw(rv, cb_data, ap)
	void	*rv;
	void	*cb_data;
	va_list	 ap;
{
	const char	 *name;
	uid_t		  uid;
	int		  search;

	const char	 *map;
	char		**hp;
	void		 *context;
	int		  r;

	search = va_arg(ap, int);
 nextdnsbynum:
	switch (search) {
	case _PW_KEYBYNUM:
		if (_pw_hesnum == -1)
			return NS_NOTFOUND;	/* no more hesiod records */
		snprintf(line, sizeof(line) - 1, "passwd-%u", _pw_hesnum);
		_pw_hesnum++;
		map = "passwd";
		break;
	case _PW_KEYBYNAME:
		name = va_arg(ap, const char *);
		strncpy(line, name, sizeof(line));
		map = "passwd";
		break;
	case _PW_KEYBYUID:
		uid = va_arg(ap, uid_t);
		snprintf(line, sizeof(line), "%u", (unsigned int)uid);
		map = "uid";		/* XXX this is `passwd' on ultrix */
		break;
	default:
		abort();
	}
	line[sizeof(line) - 1] = '\0';

	r = NS_UNAVAIL;
	if (hesiod_init(&context) == -1)
		return (r);

	hp = hesiod_resolve(context, line, map);
	if (hp == NULL) {
		if (errno == ENOENT) {
					/* flag `no more hesiod records' */
			if (search == _PW_KEYBYNUM)
				_pw_hesnum = -1;
			r = NS_NOTFOUND;
		}
		goto cleanup_dns_getpw;
	}

	strncpy(line, hp[0], sizeof(line));	/* only check first elem */
	line[sizeof(line) - 1] = '\0';
	hesiod_free_list(context, hp);
	if (__pwparse(&_pw_passwd, line)) {
		if (search == _PW_KEYBYNUM)
			goto nextdnsbynum;	/* skip dogdy entries */
		r = NS_UNAVAIL;
	} else
		r = NS_SUCCESS;
 cleanup_dns_getpw:
	hesiod_end(context);
	return (r);
}
#endif

#ifdef YP
/*
 * nis implementation of getpw*()
 * varargs: type, [ uid (type == _PW_KEYBYUID) | name (type == _PW_KEYBYNAME) ]
 */
static int	_nis_getpw __P((void *, void *, va_list));

/*ARGSUSED*/
static int
_nis_getpw(rv, cb_data, ap)
	void	*rv;
	void	*cb_data;
	va_list	 ap;
{
	const char	*name;
	uid_t		 uid;
	int		 search;
	char		*key, *data;
	char		*map;
	int		 keylen, datalen, r, rval;

	if(__ypdomain == NULL) {
		if(_yp_check(&__ypdomain) == 0)
			return NS_UNAVAIL;
	}

	map = PASSWD_BYNAME;
	search = va_arg(ap, int);
	switch (search) {
	case _PW_KEYBYNUM:
		break;
	case _PW_KEYBYNAME:
		name = va_arg(ap, const char *);
		strncpy(line, name, sizeof(line));
		break;
	case _PW_KEYBYUID:
		uid = va_arg(ap, uid_t);
		snprintf(line, sizeof(line), "%u", (unsigned int)uid);
		map = PASSWD_BYUID;
		break;
	default:
		abort();
	}
	line[sizeof(line) - 1] = '\0';
	rval = NS_UNAVAIL;
	if (search != _PW_KEYBYNUM) {
		data = NULL;
		r = yp_match(__ypdomain, map, line, (int)strlen(line),
				&data, &datalen);
		if (r == YPERR_KEY)
			rval = NS_NOTFOUND;
		if (r != 0) {
			if (data)
				free(data);
			return (rval);
		}
		data[datalen] = '\0';		/* clear trailing \n */
		strncpy(line, data, sizeof(line));
		line[sizeof(line) - 1] = '\0';
		free(data);
		if (__pwparse(&_pw_passwd, line))
			return NS_UNAVAIL;
		return NS_SUCCESS;
	}

	if (_pw_ypdone)
		return NS_NOTFOUND;
	for (;;) {
		data = key = NULL;
		if (__ypcurrent) {
			r = yp_next(__ypdomain, map,
					__ypcurrent, __ypcurrentlen,
					&key, &keylen, &data, &datalen);
			free(__ypcurrent);
			switch (r) {
			case 0:
				__ypcurrent = key;
				__ypcurrentlen = keylen;
				break;
			case YPERR_NOMORE:
				__ypcurrent = NULL;
					/* flag `no more yp records' */
				_pw_ypdone = 1;
				rval = NS_NOTFOUND;
			}
		} else {
			r = yp_first(__ypdomain, map, &__ypcurrent,
					&__ypcurrentlen, &data, &datalen);
		}
		if (r != 0) {
			if (key)
				free(key);
			if (data)
				free(data);
			return (rval);
		}
		data[datalen] = '\0';		/* clear trailing \n */
		strncpy(line, data, sizeof(line));
		line[sizeof(line) - 1] = '\0';
				free(data);
		if (! __pwparse(&_pw_passwd, line))
			return NS_SUCCESS;
	}
	/* NOTREACHED */
} /* _nis_getpw */
#endif

#ifdef _PASSWD_COMPAT
/*
 * See if the compat token is in the database.  Only works if pwd_mkdb knows
 * about the token.
 */
static int	__has_compatpw __P((void));

static int
__has_compatpw()
{
	DBT key, data;
	DBT pkey, pdata;
	char bf[MAXLOGNAME];
	u_char cyp[] = { _PW_KEYYPENABLED };

	/*LINTED*/
	key.data = cyp;
	key.size = 1;

	/* Pre-token database support. */
	bf[0] = _PW_KEYBYNAME;
	bf[1] = '+';
	pkey.data = (u_char *)bf;
	pkey.size = 2;

	if ((_pw_db->get)(_pw_db, &key, &data, 0)
	    && (_pw_db->get)(_pw_db, &pkey, &pdata, 0))
		return 0;		/* No compat token */
	return 1;
}

/*
 * log an error if "files" or "compat" is specified in passwd_compat database
 */
static int	_bad_getpw __P((void *, void *, va_list));

/*ARGSUSED*/
static int
_bad_getpw(rv, cb_data, ap)
	void	*rv;
	void	*cb_data;
	va_list	 ap;
{
	static int warned;
	if (!warned) {
		syslog(LOG_ERR,
			"nsswitch.conf passwd_compat database can't use '%s'",
			(char *)cb_data);
	}
	warned = 1;
	return NS_UNAVAIL;
}

/*
 * when a name lookup in compat mode is required (e.g., '+name', or a name in
 * '+@netgroup'), look it up in the 'passwd_compat' nsswitch database.
 * only Hesiod and NIS is supported - it doesn't make sense to lookup
 * compat names from 'files' or 'compat'.
 */
static int	__getpwcompat __P((int, uid_t, const char *));

static int
__getpwcompat(type, uid, name)
	int		 type;
	uid_t		 uid;
	const char	*name;
{
	static const ns_dtab dtab[] = {
		NS_FILES_CB(_bad_getpw, "files")
		NS_DNS_CB(_dns_getpw, NULL)
		NS_NIS_CB(_nis_getpw, NULL)
		NS_COMPAT_CB(_bad_getpw, "compat")
		{ 0 }
	};
	static const ns_src defaultnis[] = {
		{ NSSRC_NIS, 	NS_SUCCESS },
		{ 0 }
	};

	switch (type) {
	case _PW_KEYBYNUM:
		return nsdispatch(NULL, dtab, NSDB_PASSWD_COMPAT, "getpwcompat",
		    defaultnis, type);
	case _PW_KEYBYNAME:
		return nsdispatch(NULL, dtab, NSDB_PASSWD_COMPAT, "getpwcompat",
		    defaultnis, type, name);
	case _PW_KEYBYUID:
		return nsdispatch(NULL, dtab, NSDB_PASSWD_COMPAT, "getpwcompat",
		    defaultnis, type, uid);
	default:
		abort();
		/*NOTREACHED*/
	}
}
#endif /* _PASSWD_COMPAT */

/*
 * compat implementation of getpwent()
 * varargs (ignored):
 *	type, [ uid (type == _PW_KEYBYUID) | name (type == _PW_KEYBYNAME) ]
 */
static int	_compat_getpwent __P((void *, void *, va_list));

/*ARGSUSED*/
static int
_compat_getpwent(rv, cb_data, ap)
	void	*rv;
	void	*cb_data;
	va_list	 ap;
{
	DBT		 key;
	int		 rval;
	char		 bf[sizeof(_pw_keynum) + 1];
#ifdef _PASSWD_COMPAT
	static char	*name = NULL;
	char		*user, *host, *dom;
	int		 has_compatpw;
#endif

	if (!_pw_db && !__initdb())
		return NS_UNAVAIL;

#ifdef _PASSWD_COMPAT
	has_compatpw = __has_compatpw();

again:
	if (has_compatpw && (__pwmode != PWMODE_NONE)) {
		int r;

		switch (__pwmode) {
		case PWMODE_FULL:
			r = __getpwcompat(_PW_KEYBYNUM, 0, NULL);
			if (r == NS_SUCCESS)
				return r;
			__pwmode = PWMODE_NONE;
			break;

		case PWMODE_NETGRP:
			r = getnetgrent(&host, &user, &dom);
			if (r == 0) {	/* end of group */
				endnetgrent();
				__pwmode = PWMODE_NONE;
				break;
			}
			if (!user || !*user)
				break;
			r = __getpwcompat(_PW_KEYBYNAME, 0, user);
			if (r == NS_SUCCESS)
				return r;
			break;

		case PWMODE_USER:
			if (name == NULL) {
				__pwmode = PWMODE_NONE;
				break;
			}
			r = __getpwcompat(_PW_KEYBYNAME, 0, name);
			free(name);
			name = NULL;
			if (r == NS_SUCCESS)
				return r;
			break;

		case PWMODE_NONE:
			abort();
		}
		goto again;
	}
#endif

	if (_pw_keynum == -1)
		return NS_NOTFOUND;	/* no more local records */
	++_pw_keynum;
	bf[0] = _PW_KEYBYNUM;
	memmove(bf + 1, &_pw_keynum, sizeof(_pw_keynum));
	key.data = (u_char *)bf;
	key.size = sizeof(_pw_keynum) + 1;
	rval = __hashpw(&key);
	if (rval == NS_NOTFOUND)
		_pw_keynum = -1;	/* flag `no more local records' */
	else if (rval == NS_SUCCESS) {
#ifdef _PASSWD_COMPAT
		/* if we don't have YP at all, don't bother. */
		if (has_compatpw) {
			if(_pw_passwd.pw_name[0] == '+') {
				/* set the mode */
				switch(_pw_passwd.pw_name[1]) {
				case '\0':
					__pwmode = PWMODE_FULL;
					break;
				case '@':
					__pwmode = PWMODE_NETGRP;
					setnetgrent(_pw_passwd.pw_name + 2);
					break;
				default:
					__pwmode = PWMODE_USER;
					name = strdup(_pw_passwd.pw_name + 1);
					break;
				}

				/* save the prototype */
				__pwproto_set();
				goto again;
			} else if(_pw_passwd.pw_name[0] == '-') {
				/* an attempted exclusion */
				switch(_pw_passwd.pw_name[1]) {
				case '\0':
					break;
				case '@':
					setnetgrent(_pw_passwd.pw_name + 2);
					while(getnetgrent(&host, &user, &dom)) {
						if(user && *user)
							__pwexclude_add(user);
					}
					endnetgrent();
					break;
				default:
					__pwexclude_add(_pw_passwd.pw_name + 1);
					break;
				}
				goto again;
			}
		}
#endif
	}
	return (rval);
}

/*
 * compat implementation of getpwnam() and getpwuid()
 * varargs: type, [ uid (type == _PW_KEYBYUID) | name (type == _PW_KEYBYNAME) ]
 */
static int	_compat_getpw __P((void *, void *, va_list));

static int
_compat_getpw(rv, cb_data, ap)
	void	*rv;
	void	*cb_data;
	va_list	 ap;
{
#ifdef _PASSWD_COMPAT
	DBT		key;
	int		search, rval, r, s, keynum;
	uid_t		uid;
	char		bf[sizeof(keynum) + 1];
	char		*name, *host, *user, *dom;
#endif

	if (!_pw_db && !__initdb())
		return NS_UNAVAIL;

		/*
		 * If there isn't a compat token in the database, use files.
		 */
#ifdef _PASSWD_COMPAT
	if (! __has_compatpw())
#endif
		return (_local_getpw(rv, cb_data, ap));

#ifdef _PASSWD_COMPAT
	search = va_arg(ap, int);
	uid = 0;
	name = NULL;
	rval = NS_NOTFOUND;
	switch (search) {
	case _PW_KEYBYNAME:
		name = va_arg(ap, char *);
		break;
	case _PW_KEYBYUID:
		uid = va_arg(ap, uid_t);
		break;
	default:
		abort();
	}

	for (s = -1, keynum = 1 ; ; keynum++) {
		bf[0] = _PW_KEYBYNUM;
		memmove(bf + 1, &keynum, sizeof(keynum));
		key.data = (u_char *)bf;
		key.size = sizeof(keynum) + 1;
		if(__hashpw(&key) != NS_SUCCESS)
			break;
		switch(_pw_passwd.pw_name[0]) {
		case '+':
			/* save the prototype */
			__pwproto_set();

			switch(_pw_passwd.pw_name[1]) {
			case '\0':
				r = __getpwcompat(search, uid, name);
				if (r != NS_SUCCESS)
					continue;
				break;
			case '@':
pwnam_netgrp:
#if 0			/* XXX: is this a hangover from pre-nsswitch?  */
				if(__ypcurrent) {
					free(__ypcurrent);
					__ypcurrent = NULL;
				}
#endif
				if (s == -1)		/* first time */
					setnetgrent(_pw_passwd.pw_name + 2);
				s = getnetgrent(&host, &user, &dom);
				if (s == 0) {		/* end of group */
					endnetgrent();
					s = -1;
					continue;
				}
				if (!user || !*user)
					goto pwnam_netgrp;

				r = __getpwcompat(_PW_KEYBYNAME, 0, user);

				if (r == NS_UNAVAIL)
					return r;
				if (r == NS_NOTFOUND) {
					/*
					 * just because this user is bad
					 * it doesn't mean they all are.
					 */
					goto pwnam_netgrp;
				}
				break;
			default:
				user = _pw_passwd.pw_name + 1;
				r = __getpwcompat(_PW_KEYBYNAME, 0, user);

				if (r == NS_UNAVAIL)
					return r;
				if (r == NS_NOTFOUND)
					continue;
				break;
			}
			if(__pwexclude_is(_pw_passwd.pw_name)) {
				if(s == 1)		/* inside netgroup */
					goto pwnam_netgrp;
				continue;
			}
			break;
		case '-':
			/* attempted exclusion */
			switch(_pw_passwd.pw_name[1]) {
			case '\0':
				break;
			case '@':
				setnetgrent(_pw_passwd.pw_name + 2);
				while(getnetgrent(&host, &user, &dom)) {
					if(user && *user)
						__pwexclude_add(user);
				}
				endnetgrent();
				break;
			default:
				__pwexclude_add(_pw_passwd.pw_name + 1);
				break;
			}
			break;
		}
		if ((search == _PW_KEYBYNAME &&
			    strcmp(_pw_passwd.pw_name, name) == 0)
		 || (search == _PW_KEYBYUID && _pw_passwd.pw_uid == uid)) {
			rval = NS_SUCCESS;
			break;
		}
		if(s == 1)				/* inside netgroup */
			goto pwnam_netgrp;
		continue;
	}
	__pwproto = (struct passwd *)NULL;

	if (!_pw_stayopen) {
		(void)(_pw_db->close)(_pw_db);
		_pw_db = (DB *)NULL;
	}
	if(__pwexclude != (DB *)NULL) {
		(void)(__pwexclude->close)(__pwexclude);
			__pwexclude = (DB *)NULL;
	}
	return rval;
#endif /* _PASSWD_COMPAT */
}

struct passwd *
getpwent()
{
	int		r;
	static const ns_dtab dtab[] = {
		NS_FILES_CB(_local_getpw, NULL)
		NS_DNS_CB(_dns_getpw, NULL)
		NS_NIS_CB(_nis_getpw, NULL)
		NS_COMPAT_CB(_compat_getpwent, NULL)
		{ 0 }
	};

	r = nsdispatch(NULL, dtab, NSDB_PASSWD, "getpwent", compatsrc,
	    _PW_KEYBYNUM);
	if (r != NS_SUCCESS)
		return (struct passwd *)NULL;
	return &_pw_passwd;
}

struct passwd *
getpwnam(name)
	const char *name;
{
	int		r;
	static const ns_dtab dtab[] = {
		NS_FILES_CB(_local_getpw, NULL)
		NS_DNS_CB(_dns_getpw, NULL)
		NS_NIS_CB(_nis_getpw, NULL)
		NS_COMPAT_CB(_compat_getpw, NULL)
		{ 0 }
	};

	if (name == NULL || name[0] == '\0')
		return (struct passwd *)NULL;

	r = nsdispatch(NULL, dtab, NSDB_PASSWD, "getpwnam", compatsrc,
	    _PW_KEYBYNAME, name);
	return (r == NS_SUCCESS ? &_pw_passwd : (struct passwd *)NULL);
}

struct passwd *
getpwuid(uid)
	uid_t uid;
{
	int		r;
	static const ns_dtab dtab[] = {
		NS_FILES_CB(_local_getpw, NULL)
		NS_DNS_CB(_dns_getpw, NULL)
		NS_NIS_CB(_nis_getpw, NULL)
		NS_COMPAT_CB(_compat_getpw, NULL)
		{ 0 }
	};

	r = nsdispatch(NULL, dtab, NSDB_PASSWD, "getpwuid", compatsrc,
	    _PW_KEYBYUID, uid);
	return (r == NS_SUCCESS ? &_pw_passwd : (struct passwd *)NULL);
}

int
setpassent(stayopen)
	int stayopen;
{
	_pw_keynum = 0;
	_pw_stayopen = stayopen;
#ifdef YP
	__pwmode = PWMODE_NONE;
	if(__ypcurrent)
		free(__ypcurrent);
	__ypcurrent = NULL;
	_pw_ypdone = 0;
#endif
#ifdef HESIOD
	_pw_hesnum = 0;
#endif
#ifdef _PASSWD_COMPAT
	if(__pwexclude != (DB *)NULL) {
		(void)(__pwexclude->close)(__pwexclude);
		__pwexclude = (DB *)NULL;
	}
	__pwproto = (struct passwd *)NULL;
#endif
	return 1;
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
	}
#ifdef _PASSWD_COMPAT
	__pwmode = PWMODE_NONE;
#endif
#ifdef YP
	if(__ypcurrent)
		free(__ypcurrent);
	__ypcurrent = NULL;
	_pw_ypdone = 0;
#endif
#ifdef HESIOD
	_pw_hesnum = 0;
#endif
#ifdef _PASSWD_COMPAT
	if(__pwexclude != (DB *)NULL) {
		(void)(__pwexclude->close)(__pwexclude);
		__pwexclude = (DB *)NULL;
	}
	__pwproto = (struct passwd *)NULL;
#endif
}

static int
__initdb()
{
	static int warned;
	char *p;

#ifdef _PASSWD_COMPAT
	__pwmode = PWMODE_NONE;
#endif
	if (geteuid() == 0) {
		_pw_db = dbopen((p = _PATH_SMP_DB), O_RDONLY, 0, DB_HASH, NULL);
		if (_pw_db)
			return(1);
	}
	_pw_db = dbopen((p = _PATH_MP_DB), O_RDONLY, 0, DB_HASH, NULL);
	if (_pw_db)
		return 1;
	if (!warned)
		syslog(LOG_ERR, "%s: %m", p);
	warned = 1;
	return 0;
}

static int
__hashpw(key)
	DBT *key;
{
	char *p, *t;
	static u_int max;
	static char *buf;
	int32_t pw_change, pw_expire;
	DBT data;

	switch ((_pw_db->get)(_pw_db, key, &data, 0)) {
	case 0:
		break;			/* found */
	case 1:
		return NS_NOTFOUND;
	case -1:			
		return NS_UNAVAIL;	/* error in db routines */
	default:
		abort();
	}

	p = (char *)data.data;
	if (data.size > max && !(buf = realloc(buf, (max += 1024))))
		return NS_UNAVAIL;

	/* THIS CODE MUST MATCH THAT IN pwd_mkdb. */
	t = buf;
#define	EXPAND(e)	e = t; while ((*t++ = *p++));
#define	SCALAR(v)	memmove(&(v), p, sizeof v); p += sizeof v
	EXPAND(_pw_passwd.pw_name);
	EXPAND(_pw_passwd.pw_passwd);
	SCALAR(_pw_passwd.pw_uid);
	SCALAR(_pw_passwd.pw_gid);
	SCALAR(pw_change);
	EXPAND(_pw_passwd.pw_class);
	EXPAND(_pw_passwd.pw_gecos);
	EXPAND(_pw_passwd.pw_dir);
	EXPAND(_pw_passwd.pw_shell);
	SCALAR(pw_expire);
	SCALAR(_pw_passwd.pw_fields);
	_pw_passwd.pw_change = pw_change;
	_pw_passwd.pw_expire = pw_expire;

	return NS_SUCCESS;
}
