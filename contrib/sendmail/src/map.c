/*
 * Copyright (c) 1998-2001 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
 * Copyright (c) 1992, 1995-1997 Eric P. Allman.  All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#ifndef lint
static char id[] = "@(#)$Id: map.c,v 8.414.4.54 2001/06/01 08:23:24 gshapiro Exp $";
#endif /* ! lint */

#include <sendmail.h>


#ifdef NDBM
# include <ndbm.h>
# ifdef R_FIRST
  ERROR README:	You are running the Berkeley DB version of ndbm.h.  See
  ERROR README:	the README file about tweaking Berkeley DB so it can
  ERROR README:	coexist with NDBM, or delete -DNDBM from the Makefile
  ERROR README: and use -DNEWDB instead.
# endif /* R_FIRST */
#endif /* NDBM */
#ifdef NEWDB
# include <db.h>
# ifndef DB_VERSION_MAJOR
#  define DB_VERSION_MAJOR 1
# endif /* ! DB_VERSION_MAJOR */
#endif /* NEWDB */
#ifdef NIS
  struct dom_binding;	/* forward reference needed on IRIX */
# include <rpcsvc/ypclnt.h>
# ifdef NDBM
#  define NDBM_YP_COMPAT	/* create YP-compatible NDBM files */
# endif /* NDBM */
#endif /* NIS */

#ifdef NEWDB
# if DB_VERSION_MAJOR < 2
static bool	db_map_open __P((MAP *, int, char *, DBTYPE, const void *));
# endif /* DB_VERSION_MAJOR < 2 */
# if DB_VERSION_MAJOR == 2
static bool	db_map_open __P((MAP *, int, char *, DBTYPE, DB_INFO *));
# endif /* DB_VERSION_MAJOR == 2 */
# if DB_VERSION_MAJOR > 2
static bool	db_map_open __P((MAP *, int, char *, DBTYPE, void **));
# endif /* DB_VERSION_MAJOR > 2 */
#endif /* NEWDB */
static bool	extract_canonname __P((char *, char *, char *, char[], int));
#ifdef LDAPMAP
static void	ldapmap_clear __P((LDAPMAP_STRUCT *));
static STAB	*ldapmap_findconn __P((LDAPMAP_STRUCT *));
static int	ldapmap_geterrno __P((LDAP *));
static void	ldapmap_setopts __P((LDAP *, LDAPMAP_STRUCT *));
static bool	ldapmap_start __P((MAP *));
static void	ldaptimeout __P((int));
#endif /* LDAPMAP */
static void	map_close __P((STAB *, int));
static void	map_init __P((STAB *, int));
#ifdef NISPLUS
static bool	nisplus_getcanonname __P((char *, int, int *));
#endif /* NISPLUS */
#ifdef NIS
static bool	nis_getcanonname __P((char *, int, int *));
#endif /* NIS */
#if NETINFO
static bool	ni_getcanonname __P((char *, int, int *));
#endif /* NETINFO */
static bool	text_getcanonname __P((char *, int, int *));

/*
**  MAP.C -- implementations for various map classes.
**
**	Each map class implements a series of functions:
**
**	bool map_parse(MAP *map, char *args)
**		Parse the arguments from the config file.  Return TRUE
**		if they were ok, FALSE otherwise.  Fill in map with the
**		values.
**
**	char *map_lookup(MAP *map, char *key, char **args, int *pstat)
**		Look up the key in the given map.  If found, do any
**		rewriting the map wants (including "args" if desired)
**		and return the value.  Set *pstat to the appropriate status
**		on error and return NULL.  Args will be NULL if called
**		from the alias routines, although this should probably
**		not be relied upon.  It is suggested you call map_rewrite
**		to return the results -- it takes care of null termination
**		and uses a dynamically expanded buffer as needed.
**
**	void map_store(MAP *map, char *key, char *value)
**		Store the key:value pair in the map.
**
**	bool map_open(MAP *map, int mode)
**		Open the map for the indicated mode.  Mode should
**		be either O_RDONLY or O_RDWR.  Return TRUE if it
**		was opened successfully, FALSE otherwise.  If the open
**		failed an the MF_OPTIONAL flag is not set, it should
**		also print an error.  If the MF_ALIAS bit is set
**		and this map class understands the @:@ convention, it
**		should call aliaswait() before returning.
**
**	void map_close(MAP *map)
**		Close the map.
**
**	This file also includes the implementation for getcanonname.
**	It is currently implemented in a pretty ad-hoc manner; it ought
**	to be more properly integrated into the map structure.
*/

#define DBMMODE		0644

#ifndef EX_NOTFOUND
# define EX_NOTFOUND	EX_NOHOST
#endif /* ! EX_NOTFOUND */

#if O_EXLOCK && HASFLOCK && !BOGUS_O_EXCL
# define LOCK_ON_OPEN	1	/* we can open/create a locked file */
#else /* O_EXLOCK && HASFLOCK && !BOGUS_O_EXCL */
# define LOCK_ON_OPEN	0	/* no such luck -- bend over backwards */
#endif /* O_EXLOCK && HASFLOCK && !BOGUS_O_EXCL */

/*
**  MAP_PARSEARGS -- parse config line arguments for database lookup
**
**	This is a generic version of the map_parse method.
**
**	Parameters:
**		map -- the map being initialized.
**		ap -- a pointer to the args on the config line.
**
**	Returns:
**		TRUE -- if everything parsed OK.
**		FALSE -- otherwise.
**
**	Side Effects:
**		null terminates the filename; stores it in map
*/

bool
map_parseargs(map, ap)
	MAP *map;
	char *ap;
{
	register char *p = ap;

	/*
	**  there is no check whether there is really an argument,
	**  but that's not important enough to warrant extra code
	*/
	map->map_mflags |= MF_TRY0NULL | MF_TRY1NULL;
	map->map_spacesub = SpaceSub;	/* default value */
	for (;;)
	{
		while (isascii(*p) && isspace(*p))
			p++;
		if (*p != '-')
			break;
		switch (*++p)
		{
		  case 'N':
			map->map_mflags |= MF_INCLNULL;
			map->map_mflags &= ~MF_TRY0NULL;
			break;

		  case 'O':
			map->map_mflags &= ~MF_TRY1NULL;
			break;

		  case 'o':
			map->map_mflags |= MF_OPTIONAL;
			break;

		  case 'f':
			map->map_mflags |= MF_NOFOLDCASE;
			break;

		  case 'm':
			map->map_mflags |= MF_MATCHONLY;
			break;

		  case 'A':
			map->map_mflags |= MF_APPEND;
			break;

		  case 'q':
			map->map_mflags |= MF_KEEPQUOTES;
			break;

		  case 'a':
			map->map_app = ++p;
			break;

		  case 'T':
			map->map_tapp = ++p;
			break;

		  case 'k':
			while (isascii(*++p) && isspace(*p))
				continue;
			map->map_keycolnm = p;
			break;

		  case 'v':
			while (isascii(*++p) && isspace(*p))
				continue;
			map->map_valcolnm = p;
			break;

		  case 'z':
			if (*++p != '\\')
				map->map_coldelim = *p;
			else
			{
				switch (*++p)
				{
				  case 'n':
					map->map_coldelim = '\n';
					break;

				  case 't':
					map->map_coldelim = '\t';
					break;

				  default:
					map->map_coldelim = '\\';
				}
			}
			break;

		  case 't':
			map->map_mflags |= MF_NODEFER;
			break;


		  case 'S':
			map->map_spacesub = *++p;
			break;

		  case 'D':
			map->map_mflags |= MF_DEFER;
			break;

		  default:
			syserr("Illegal option %c map %s", *p, map->map_mname);
			break;
		}
		while (*p != '\0' && !(isascii(*p) && isspace(*p)))
			p++;
		if (*p != '\0')
			*p++ = '\0';
	}
	if (map->map_app != NULL)
		map->map_app = newstr(map->map_app);
	if (map->map_tapp != NULL)
		map->map_tapp = newstr(map->map_tapp);
	if (map->map_keycolnm != NULL)
		map->map_keycolnm = newstr(map->map_keycolnm);
	if (map->map_valcolnm != NULL)
		map->map_valcolnm = newstr(map->map_valcolnm);

	if (*p != '\0')
	{
		map->map_file = p;
		while (*p != '\0' && !(isascii(*p) && isspace(*p)))
			p++;
		if (*p != '\0')
			*p++ = '\0';
		map->map_file = newstr(map->map_file);
	}

	while (*p != '\0' && isascii(*p) && isspace(*p))
		p++;
	if (*p != '\0')
		map->map_rebuild = newstr(p);

	if (map->map_file == NULL &&
	    !bitset(MCF_OPTFILE, map->map_class->map_cflags))
	{
		syserr("No file name for %s map %s",
			map->map_class->map_cname, map->map_mname);
		return FALSE;
	}
	return TRUE;
}
/*
**  MAP_REWRITE -- rewrite a database key, interpolating %n indications.
**
**	It also adds the map_app string.  It can be used as a utility
**	in the map_lookup method.
**
**	Parameters:
**		map -- the map that causes this.
**		s -- the string to rewrite, NOT necessarily null terminated.
**		slen -- the length of s.
**		av -- arguments to interpolate into buf.
**
**	Returns:
**		Pointer to rewritten result.  This is static data that
**		should be copied if it is to be saved!
**
**	Side Effects:
**		none.
*/

char *
map_rewrite(map, s, slen, av)
	register MAP *map;
	register const char *s;
	size_t slen;
	char **av;
{
	register char *bp;
	register char c;
	char **avp;
	register char *ap;
	size_t l;
	size_t len;
	static size_t buflen = 0;
	static char *buf = NULL;

	if (tTd(39, 1))
	{
		dprintf("map_rewrite(%.*s), av =", (int)slen, s);
		if (av == NULL)
			dprintf(" (nullv)");
		else
		{
			for (avp = av; *avp != NULL; avp++)
				dprintf("\n\t%s", *avp);
		}
		dprintf("\n");
	}

	/* count expected size of output (can safely overestimate) */
	l = len = slen;
	if (av != NULL)
	{
		const char *sp = s;

		while (l-- > 0 && (c = *sp++) != '\0')
		{
			if (c != '%')
				continue;
			if (l-- <= 0)
				break;
			c = *sp++;
			if (!(isascii(c) && isdigit(c)))
				continue;
			for (avp = av; --c >= '0' && *avp != NULL; avp++)
				continue;
			if (*avp == NULL)
				continue;
			len += strlen(*avp);
		}
	}
	if (map->map_app != NULL)
		len += strlen(map->map_app);
	if (buflen < ++len)
	{
		/* need to malloc additional space */
		buflen = len;
		if (buf != NULL)
			sm_free(buf);
		buf = xalloc(buflen);
	}

	bp = buf;
	if (av == NULL)
	{
		memmove(bp, s, slen);
		bp += slen;

		/* assert(len > slen); */
		len -= slen;
	}
	else
	{
		while (slen-- > 0 && (c = *s++) != '\0')
		{
			if (c != '%')
			{
  pushc:
				if (--len <= 0)
					break;
				*bp++ = c;
				continue;
			}
			if (slen-- <= 0 || (c = *s++) == '\0')
				c = '%';
			if (c == '%')
				goto pushc;
			if (!(isascii(c) && isdigit(c)))
			{
				*bp++ = '%';
				--len;
				goto pushc;
			}
			for (avp = av; --c >= '0' && *avp != NULL; avp++)
				continue;
			if (*avp == NULL)
				continue;

			/* transliterate argument into output string */
			for (ap = *avp; (c = *ap++) != '\0' && len > 0; --len)
				*bp++ = c;
		}
	}
	if (map->map_app != NULL && len > 0)
		(void) strlcpy(bp, map->map_app, len);
	else
		*bp = '\0';
	if (tTd(39, 1))
		dprintf("map_rewrite => %s\n", buf);
	return buf;
}
/*
**  INITMAPS -- rebuild alias maps
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
*/

void
initmaps()
{
#if XDEBUG
	checkfd012("entering initmaps");
#endif /* XDEBUG */
	stabapply(map_init, 0);
#if XDEBUG
	checkfd012("exiting initmaps");
#endif /* XDEBUG */
}
/*
**  MAP_INIT -- rebuild a map
**
**	Parameters:
**		s -- STAB entry: if map: try to rebuild
**		unused -- unused variable
**
**	Returns:
**		none.
**
**	Side Effects:
**		will close already open rebuildable map.
*/

/* ARGSUSED1 */
static void
map_init(s, unused)
	register STAB *s;
	int unused;
{
	register MAP *map;

	/* has to be a map */
	if (s->s_type != ST_MAP)
		return;

	map = &s->s_map;
	if (!bitset(MF_VALID, map->map_mflags))
		return;

	if (tTd(38, 2))
		dprintf("map_init(%s:%s, %s)\n",
			map->map_class->map_cname == NULL ? "NULL" :
				map->map_class->map_cname,
			map->map_mname == NULL ? "NULL" : map->map_mname,
			map->map_file == NULL ? "NULL" : map->map_file);

	if (!bitset(MF_ALIAS, map->map_mflags) ||
	    !bitset(MCF_REBUILDABLE, map->map_class->map_cflags))
	{
		if (tTd(38, 3))
			dprintf("\tnot rebuildable\n");
		return;
	}

	/* if already open, close it (for nested open) */
	if (bitset(MF_OPEN, map->map_mflags))
	{
		map->map_mflags |= MF_CLOSING;
		map->map_class->map_close(map);
		map->map_mflags &= ~(MF_OPEN|MF_WRITABLE|MF_CLOSING);
	}

	(void) rebuildaliases(map, FALSE);
	return;
}
/*
**  OPENMAP -- open a map
**
**	Parameters:
**		map -- map to open (it must not be open).
**
**	Returns:
**		whether open succeeded.
**
*/

bool
openmap(map)
	MAP *map;
{
	bool restore = FALSE;
	bool savehold = HoldErrs;
	bool savequick = QuickAbort;
	int saveerrors = Errors;

	if (!bitset(MF_VALID, map->map_mflags))
		return FALSE;

	/* better safe than sorry... */
	if (bitset(MF_OPEN, map->map_mflags))
		return TRUE;

	/* Don't send a map open error out via SMTP */
	if ((OnlyOneError || QuickAbort) &&
	    (OpMode == MD_SMTP || OpMode == MD_DAEMON))
	{
		restore = TRUE;
		HoldErrs = TRUE;
		QuickAbort = FALSE;
	}

	errno = 0;
	if (map->map_class->map_open(map, O_RDONLY))
	{
		if (tTd(38, 4))
			dprintf("openmap()\t%s:%s %s: valid\n",
				map->map_class->map_cname == NULL ? "NULL" :
					map->map_class->map_cname,
				map->map_mname == NULL ? "NULL" :
					map->map_mname,
				map->map_file == NULL ? "NULL" :
					map->map_file);
		map->map_mflags |= MF_OPEN;
		map->map_pid = getpid();
	}
	else
	{
		if (tTd(38, 4))
			dprintf("openmap()\t%s:%s %s: invalid%s%s\n",
				map->map_class->map_cname == NULL ? "NULL" :
					map->map_class->map_cname,
				map->map_mname == NULL ? "NULL" :
					map->map_mname,
				map->map_file == NULL ? "NULL" :
					map->map_file,
				errno == 0 ? "" : ": ",
				errno == 0 ? "" : errstring(errno));
		if (!bitset(MF_OPTIONAL, map->map_mflags))
		{
			extern MAPCLASS BogusMapClass;

			map->map_class = &BogusMapClass;
			map->map_mflags |= MF_OPEN;
			map->map_pid = getpid();
			MapOpenErr = TRUE;
		}
		else
		{
			/* don't try again */
			map->map_mflags &= ~MF_VALID;
		}
	}

	if (restore)
	{
		Errors = saveerrors;
		HoldErrs = savehold;
		QuickAbort = savequick;
	}

	return bitset(MF_OPEN, map->map_mflags);
}
/*
**  CLOSEMAPS -- close all open maps opened by the current pid.
**
**	Parameters:
**		none
**
**	Returns:
**		none.
*/

void
closemaps()
{
	stabapply(map_close, 0);
}
/*
**  MAP_CLOSE -- close a map opened by the current pid.
**
**	Parameters:
**		s -- STAB entry: if map: try to open
**		second parameter is unused (required by stabapply())
**
**	Returns:
**		none.
*/

/* ARGSUSED1 */
static void
map_close(s, unused)
	register STAB *s;
	int unused;
{
	MAP *map;

	if (s->s_type != ST_MAP)
		return;

	map = &s->s_map;

	if (!bitset(MF_VALID, map->map_mflags) ||
	    !bitset(MF_OPEN, map->map_mflags) ||
	    bitset(MF_CLOSING, map->map_mflags) ||
	    map->map_pid != getpid())
		return;

	if (tTd(38, 5))
		dprintf("closemaps: closing %s (%s)\n",
			map->map_mname == NULL ? "NULL" : map->map_mname,
			map->map_file == NULL ? "NULL" : map->map_file);

	map->map_mflags |= MF_CLOSING;
	map->map_class->map_close(map);
	map->map_mflags &= ~(MF_OPEN|MF_WRITABLE|MF_CLOSING);
}
/*
**  GETCANONNAME -- look up name using service switch
**
**	Parameters:
**		host -- the host name to look up.
**		hbsize -- the size of the host buffer.
**		trymx -- if set, try MX records.
**
**	Returns:
**		TRUE -- if the host was found.
**		FALSE -- otherwise.
*/

bool
getcanonname(host, hbsize, trymx)
	char *host;
	int hbsize;
	bool trymx;
{
	int nmaps;
	int mapno;
	bool found = FALSE;
	bool got_tempfail = FALSE;
	auto int status;
	char *maptype[MAXMAPSTACK];
	short mapreturn[MAXMAPACTIONS];

	nmaps = switch_map_find("hosts", maptype, mapreturn);
	for (mapno = 0; mapno < nmaps; mapno++)
	{
		int i;

		if (tTd(38, 20))
			dprintf("getcanonname(%s), trying %s\n",
				host, maptype[mapno]);
		if (strcmp("files", maptype[mapno]) == 0)
		{
			found = text_getcanonname(host, hbsize, &status);
		}
#ifdef NIS
		else if (strcmp("nis", maptype[mapno]) == 0)
		{
			found = nis_getcanonname(host, hbsize, &status);
		}
#endif /* NIS */
#ifdef NISPLUS
		else if (strcmp("nisplus", maptype[mapno]) == 0)
		{
			found = nisplus_getcanonname(host, hbsize, &status);
		}
#endif /* NISPLUS */
#if NAMED_BIND
		else if (strcmp("dns", maptype[mapno]) == 0)
		{
			found = dns_getcanonname(host, hbsize, trymx, &status);
		}
#endif /* NAMED_BIND */
#if NETINFO
		else if (strcmp("netinfo", maptype[mapno]) == 0)
		{
			found = ni_getcanonname(host, hbsize, &status);
		}
#endif /* NETINFO */
		else
		{
			found = FALSE;
			status = EX_UNAVAILABLE;
		}

		/*
		**  Heuristic: if $m is not set, we are running during system
		**  startup.  In this case, when a name is apparently found
		**  but has no dot, treat is as not found.  This avoids
		**  problems if /etc/hosts has no FQDN but is listed first
		**  in the service switch.
		*/

		if (found &&
		    (macvalue('m', CurEnv) != NULL || strchr(host, '.') != NULL))
			break;

		/* see if we should continue */
		if (status == EX_TEMPFAIL)
		{
			i = MA_TRYAGAIN;
			got_tempfail = TRUE;
		}
		else if (status == EX_NOTFOUND)
			i = MA_NOTFOUND;
		else
			i = MA_UNAVAIL;
		if (bitset(1 << mapno, mapreturn[i]))
			break;
	}

	if (found)
	{
		char *d;

		if (tTd(38, 20))
			dprintf("getcanonname(%s), found\n", host);

		/*
		**  If returned name is still single token, compensate
		**  by tagging on $m.  This is because some sites set
		**  up their DNS or NIS databases wrong.
		*/

		if ((d = strchr(host, '.')) == NULL || d[1] == '\0')
		{
			d = macvalue('m', CurEnv);
			if (d != NULL &&
			    hbsize > (int) (strlen(host) + strlen(d) + 1))
			{
				if (host[strlen(host) - 1] != '.')
					(void) strlcat(host, ".", hbsize);
				(void) strlcat(host, d, hbsize);
			}
			else
				return FALSE;
		}
		return TRUE;
	}

	if (tTd(38, 20))
		dprintf("getcanonname(%s), failed, status=%d\n", host, status);

#if NAMED_BIND
	if (got_tempfail)
		SM_SET_H_ERRNO(TRY_AGAIN);
	else
		SM_SET_H_ERRNO(HOST_NOT_FOUND);
#endif /* NAMED_BIND */
	return FALSE;
}
/*
**  EXTRACT_CANONNAME -- extract canonical name from /etc/hosts entry
**
**	Parameters:
**		name -- the name against which to match.
**		dot -- where to reinsert '.' to get FQDN
**		line -- the /etc/hosts line.
**		cbuf -- the location to store the result.
**		cbuflen -- the size of cbuf.
**
**	Returns:
**		TRUE -- if the line matched the desired name.
**		FALSE -- otherwise.
*/

static bool
extract_canonname(name, dot, line, cbuf, cbuflen)
	char *name;
	char *dot;
	char *line;
	char cbuf[];
	int cbuflen;
{
	int i;
	char *p;
	bool found = FALSE;

	cbuf[0] = '\0';
	if (line[0] == '#')
		return FALSE;

	for (i = 1; ; i++)
	{
		char nbuf[MAXNAME + 1];

		p = get_column(line, i, '\0', nbuf, sizeof nbuf);
		if (p == NULL)
			break;
		if (*p == '\0')
			continue;
		if (cbuf[0] == '\0' ||
		    (strchr(cbuf, '.') == NULL && strchr(p, '.') != NULL))
		{
			snprintf(cbuf, cbuflen, "%s", p);
		}
		if (strcasecmp(name, p) == 0)
			found = TRUE;
		else if (dot != NULL)
		{
			/* try looking for the FQDN as well */
			*dot = '.';
			if (strcasecmp(name, p) == 0)
				found = TRUE;
			*dot = '\0';
		}
	}
	if (found && strchr(cbuf, '.') == NULL)
	{
		/* try to add a domain on the end of the name */
		char *domain = macvalue('m', CurEnv);

		if (domain != NULL &&
		    strlen(domain) + (i = strlen(cbuf)) + 1 < (size_t) cbuflen)
		{
			p = &cbuf[i];
			*p++ = '.';
			(void) strlcpy(p, domain, cbuflen - i - 1);
		}
	}
	return found;
}
/*
**  NDBM modules
*/

#ifdef NDBM

/*
**  NDBM_MAP_OPEN -- DBM-style map open
*/

bool
ndbm_map_open(map, mode)
	MAP *map;
	int mode;
{
	register DBM *dbm;
	int save_errno;
	int dfd;
	int pfd;
	long sff;
	int ret;
	int smode = S_IREAD;
	char dirfile[MAXNAME + 1];
	char pagfile[MAXNAME + 1];
	struct stat st;
	struct stat std, stp;

	if (tTd(38, 2))
		dprintf("ndbm_map_open(%s, %s, %d)\n",
			map->map_mname, map->map_file, mode);
	map->map_lockfd = -1;
	mode &= O_ACCMODE;

	/* do initial file and directory checks */
	snprintf(dirfile, sizeof dirfile, "%s.dir", map->map_file);
	snprintf(pagfile, sizeof pagfile, "%s.pag", map->map_file);
	sff = SFF_ROOTOK|SFF_REGONLY;
	if (mode == O_RDWR)
	{
		sff |= SFF_CREAT;
		if (!bitnset(DBS_WRITEMAPTOSYMLINK, DontBlameSendmail))
			sff |= SFF_NOSLINK;
		if (!bitnset(DBS_WRITEMAPTOHARDLINK, DontBlameSendmail))
			sff |= SFF_NOHLINK;
		smode = S_IWRITE;
	}
	else
	{
		if (!bitnset(DBS_LINKEDMAPINWRITABLEDIR, DontBlameSendmail))
			sff |= SFF_NOWLINK;
	}
	if (!bitnset(DBS_MAPINUNSAFEDIRPATH, DontBlameSendmail))
		sff |= SFF_SAFEDIRPATH;
	ret = safefile(dirfile, RunAsUid, RunAsGid, RunAsUserName,
			    sff, smode, &std);
	if (ret == 0)
		ret = safefile(pagfile, RunAsUid, RunAsGid, RunAsUserName,
			       sff, smode, &stp);

# if !_FFR_REMOVE_AUTOREBUILD
	if (ret == ENOENT && AutoRebuild &&
	    bitset(MCF_REBUILDABLE, map->map_class->map_cflags) &&
	    (bitset(MF_IMPL_NDBM, map->map_mflags) ||
	     bitset(MF_ALIAS, map->map_mflags)) &&
	    mode == O_RDONLY)
	{
		bool impl = bitset(MF_IMPL_NDBM, map->map_mflags);

		/* may be able to rebuild */
		map->map_mflags &= ~MF_IMPL_NDBM;
		if (!rebuildaliases(map, TRUE))
			return FALSE;
		if (impl)
			return impl_map_open(map, O_RDONLY);
		else
			return ndbm_map_open(map, O_RDONLY);
	}
# endif /* !_FFR_REMOVE_AUTOREBUILD */

	if (ret != 0)
	{
		char *prob = "unsafe";

		/* cannot open this map */
		if (ret == ENOENT)
			prob = "missing";
		if (tTd(38, 2))
			dprintf("\t%s map file: %d\n", prob, ret);
		if (!bitset(MF_OPTIONAL, map->map_mflags))
			syserr("dbm map \"%s\": %s map file %s",
				map->map_mname, prob, map->map_file);
		return FALSE;
	}
	if (std.st_mode == ST_MODE_NOFILE)
		mode |= O_CREAT|O_EXCL;

# if LOCK_ON_OPEN
	if (mode == O_RDONLY)
		mode |= O_SHLOCK;
	else
		mode |= O_TRUNC|O_EXLOCK;
# else /* LOCK_ON_OPEN */
	if ((mode & O_ACCMODE) == O_RDWR)
	{
#  if NOFTRUNCATE
		/*
		**  Warning: race condition.  Try to lock the file as
		**  quickly as possible after opening it.
		**	This may also have security problems on some systems,
		**	but there isn't anything we can do about it.
		*/

		mode |= O_TRUNC;
#  else /* NOFTRUNCATE */
		/*
		**  This ugly code opens the map without truncating it,
		**  locks the file, then truncates it.  Necessary to
		**  avoid race conditions.
		*/

		int dirfd;
		int pagfd;
		long sff = SFF_CREAT|SFF_OPENASROOT;

		if (!bitnset(DBS_WRITEMAPTOSYMLINK, DontBlameSendmail))
			sff |= SFF_NOSLINK;
		if (!bitnset(DBS_WRITEMAPTOHARDLINK, DontBlameSendmail))
			sff |= SFF_NOHLINK;

		dirfd = safeopen(dirfile, mode, DBMMODE, sff);
		pagfd = safeopen(pagfile, mode, DBMMODE, sff);

		if (dirfd < 0 || pagfd < 0)
		{
			save_errno = errno;
			if (dirfd >= 0)
				(void) close(dirfd);
			if (pagfd >= 0)
				(void) close(pagfd);
			errno = save_errno;
			syserr("ndbm_map_open: cannot create database %s",
				map->map_file);
			return FALSE;
		}
		if (ftruncate(dirfd, (off_t) 0) < 0 ||
		    ftruncate(pagfd, (off_t) 0) < 0)
		{
			save_errno = errno;
			(void) close(dirfd);
			(void) close(pagfd);
			errno = save_errno;
			syserr("ndbm_map_open: cannot truncate %s.{dir,pag}",
				map->map_file);
			return FALSE;
		}

		/* if new file, get "before" bits for later filechanged check */
		if (std.st_mode == ST_MODE_NOFILE &&
		    (fstat(dirfd, &std) < 0 || fstat(pagfd, &stp) < 0))
		{
			save_errno = errno;
			(void) close(dirfd);
			(void) close(pagfd);
			errno = save_errno;
			syserr("ndbm_map_open(%s.{dir,pag}): cannot fstat pre-opened file",
				map->map_file);
			return FALSE;
		}

		/* have to save the lock for the duration (bletch) */
		map->map_lockfd = dirfd;
		(void) close(pagfd);

		/* twiddle bits for dbm_open */
		mode &= ~(O_CREAT|O_EXCL);
#  endif /* NOFTRUNCATE */
	}
# endif /* LOCK_ON_OPEN */

	/* open the database */
	dbm = dbm_open(map->map_file, mode, DBMMODE);
	if (dbm == NULL)
	{
		save_errno = errno;
		if (bitset(MF_ALIAS, map->map_mflags) &&
		    aliaswait(map, ".pag", FALSE))
			return TRUE;
# if !LOCK_ON_OPEN && !NOFTRUNCATE
		if (map->map_lockfd >= 0)
			(void) close(map->map_lockfd);
# endif /* !LOCK_ON_OPEN && !NOFTRUNCATE */
		errno = save_errno;
		if (!bitset(MF_OPTIONAL, map->map_mflags))
			syserr("Cannot open DBM database %s", map->map_file);
		return FALSE;
	}
	dfd = dbm_dirfno(dbm);
	pfd = dbm_pagfno(dbm);
	if (dfd == pfd)
	{
		/* heuristic: if files are linked, this is actually gdbm */
		dbm_close(dbm);
# if !LOCK_ON_OPEN && !NOFTRUNCATE
		if (map->map_lockfd >= 0)
			(void) close(map->map_lockfd);
# endif /* !LOCK_ON_OPEN && !NOFTRUNCATE */
		errno = 0;
		syserr("dbm map \"%s\": cannot support GDBM",
			map->map_mname);
		return FALSE;
	}

	if (filechanged(dirfile, dfd, &std) ||
	    filechanged(pagfile, pfd, &stp))
	{
		save_errno = errno;
		dbm_close(dbm);
# if !LOCK_ON_OPEN && !NOFTRUNCATE
		if (map->map_lockfd >= 0)
			(void) close(map->map_lockfd);
# endif /* !LOCK_ON_OPEN && !NOFTRUNCATE */
		errno = save_errno;
		syserr("ndbm_map_open(%s): file changed after open",
			map->map_file);
		return FALSE;
	}

	map->map_db1 = (ARBPTR_T) dbm;

	/*
	**  Need to set map_mtime before the call to aliaswait()
	**  as aliaswait() will call map_lookup() which requires
	**  map_mtime to be set
	*/

	if (fstat(pfd, &st) >= 0)
		map->map_mtime = st.st_mtime;

	if (mode == O_RDONLY)
	{
# if LOCK_ON_OPEN
		if (dfd >= 0)
			(void) lockfile(dfd, map->map_file, ".dir", LOCK_UN);
		if (pfd >= 0)
			(void) lockfile(pfd, map->map_file, ".pag", LOCK_UN);
# endif /* LOCK_ON_OPEN */
		if (bitset(MF_ALIAS, map->map_mflags) &&
		    !aliaswait(map, ".pag", TRUE))
			return FALSE;
	}
	else
	{
		map->map_mflags |= MF_LOCKED;
		if (geteuid() == 0 && TrustedUid != 0)
		{
#  if HASFCHOWN
			if (fchown(dfd, TrustedUid, -1) < 0 ||
			    fchown(pfd, TrustedUid, -1) < 0)
			{
				int err = errno;

				sm_syslog(LOG_ALERT, NOQID,
					  "ownership change on %s failed: %s",
					  map->map_file, errstring(err));
				message("050 ownership change on %s failed: %s",
					map->map_file, errstring(err));
			}
#  endif /* HASFCHOWN */
		}
	}
	return TRUE;
}


/*
**  NDBM_MAP_LOOKUP -- look up a datum in a DBM-type map
*/

char *
ndbm_map_lookup(map, name, av, statp)
	MAP *map;
	char *name;
	char **av;
	int *statp;
{
	datum key, val;
	int dfd, pfd;
	char keybuf[MAXNAME + 1];
	struct stat stbuf;

	if (tTd(38, 20))
		dprintf("ndbm_map_lookup(%s, %s)\n",
			map->map_mname, name);

	key.dptr = name;
	key.dsize = strlen(name);
	if (!bitset(MF_NOFOLDCASE, map->map_mflags))
	{
		if (key.dsize > sizeof keybuf - 1)
			key.dsize = sizeof keybuf - 1;
		memmove(keybuf, key.dptr, key.dsize);
		keybuf[key.dsize] = '\0';
		makelower(keybuf);
		key.dptr = keybuf;
	}
lockdbm:
	dfd = dbm_dirfno((DBM *) map->map_db1);
	if (dfd >= 0 && !bitset(MF_LOCKED, map->map_mflags))
		(void) lockfile(dfd, map->map_file, ".dir", LOCK_SH);
	pfd = dbm_pagfno((DBM *) map->map_db1);
	if (pfd < 0 || fstat(pfd, &stbuf) < 0 ||
	    stbuf.st_mtime > map->map_mtime)
	{
		/* Reopen the database to sync the cache */
		int omode = bitset(map->map_mflags, MF_WRITABLE) ? O_RDWR
								 : O_RDONLY;

		if (dfd >= 0 && !bitset(MF_LOCKED, map->map_mflags))
			(void) lockfile(dfd, map->map_file, ".dir", LOCK_UN);
		map->map_mflags |= MF_CLOSING;
		map->map_class->map_close(map);
		map->map_mflags &= ~(MF_OPEN|MF_WRITABLE|MF_CLOSING);
		if (map->map_class->map_open(map, omode))
		{
			map->map_mflags |= MF_OPEN;
			map->map_pid = getpid();
			if ((omode && O_ACCMODE) == O_RDWR)
				map->map_mflags |= MF_WRITABLE;
			goto lockdbm;
		}
		else
		{
			if (!bitset(MF_OPTIONAL, map->map_mflags))
			{
				extern MAPCLASS BogusMapClass;

				*statp = EX_TEMPFAIL;
				map->map_class = &BogusMapClass;
				map->map_mflags |= MF_OPEN;
				map->map_pid = getpid();
				syserr("Cannot reopen NDBM database %s",
					map->map_file);
			}
			return NULL;
		}
	}
	val.dptr = NULL;
	if (bitset(MF_TRY0NULL, map->map_mflags))
	{
		val = dbm_fetch((DBM *) map->map_db1, key);
		if (val.dptr != NULL)
			map->map_mflags &= ~MF_TRY1NULL;
	}
	if (val.dptr == NULL && bitset(MF_TRY1NULL, map->map_mflags))
	{
		key.dsize++;
		val = dbm_fetch((DBM *) map->map_db1, key);
		if (val.dptr != NULL)
			map->map_mflags &= ~MF_TRY0NULL;
	}
	if (dfd >= 0 && !bitset(MF_LOCKED, map->map_mflags))
		(void) lockfile(dfd, map->map_file, ".dir", LOCK_UN);
	if (val.dptr == NULL)
		return NULL;
	if (bitset(MF_MATCHONLY, map->map_mflags))
		return map_rewrite(map, name, strlen(name), NULL);
	else
		return map_rewrite(map, val.dptr, val.dsize, av);
}


/*
**  NDBM_MAP_STORE -- store a datum in the database
*/

void
ndbm_map_store(map, lhs, rhs)
	register MAP *map;
	char *lhs;
	char *rhs;
{
	datum key;
	datum data;
	int status;
	char keybuf[MAXNAME + 1];

	if (tTd(38, 12))
		dprintf("ndbm_map_store(%s, %s, %s)\n",
			map->map_mname, lhs, rhs);

	key.dsize = strlen(lhs);
	key.dptr = lhs;
	if (!bitset(MF_NOFOLDCASE, map->map_mflags))
	{
		if (key.dsize > sizeof keybuf - 1)
			key.dsize = sizeof keybuf - 1;
		memmove(keybuf, key.dptr, key.dsize);
		keybuf[key.dsize] = '\0';
		makelower(keybuf);
		key.dptr = keybuf;
	}

	data.dsize = strlen(rhs);
	data.dptr = rhs;

	if (bitset(MF_INCLNULL, map->map_mflags))
	{
		key.dsize++;
		data.dsize++;
	}

	status = dbm_store((DBM *) map->map_db1, key, data, DBM_INSERT);
	if (status > 0)
	{
		if (!bitset(MF_APPEND, map->map_mflags))
			message("050 Warning: duplicate alias name %s", lhs);
		else
		{
			static char *buf = NULL;
			static int bufsiz = 0;
			auto int xstat;
			datum old;

			old.dptr = ndbm_map_lookup(map, key.dptr,
						   (char **)NULL, &xstat);
			if (old.dptr != NULL && *(char *) old.dptr != '\0')
			{
				old.dsize = strlen(old.dptr);
				if (data.dsize + old.dsize + 2 > bufsiz)
				{
					if (buf != NULL)
						sm_free(buf);
					bufsiz = data.dsize + old.dsize + 2;
					buf = xalloc(bufsiz);
				}
				snprintf(buf, bufsiz, "%s,%s",
					data.dptr, old.dptr);
				data.dsize = data.dsize + old.dsize + 1;
				data.dptr = buf;
				if (tTd(38, 9))
					dprintf("ndbm_map_store append=%s\n",
						data.dptr);
			}
		}
		status = dbm_store((DBM *) map->map_db1,
				   key, data, DBM_REPLACE);
	}
	if (status != 0)
		syserr("readaliases: dbm put (%s): %d", lhs, status);
}


/*
**  NDBM_MAP_CLOSE -- close the database
*/

void
ndbm_map_close(map)
	register MAP  *map;
{
	if (tTd(38, 9))
		dprintf("ndbm_map_close(%s, %s, %lx)\n",
			map->map_mname, map->map_file, map->map_mflags);

	if (bitset(MF_WRITABLE, map->map_mflags))
	{
# ifdef NDBM_YP_COMPAT
		bool inclnull;
		char buf[MAXHOSTNAMELEN];

		inclnull = bitset(MF_INCLNULL, map->map_mflags);
		map->map_mflags &= ~MF_INCLNULL;

		if (strstr(map->map_file, "/yp/") != NULL)
		{
			long save_mflags = map->map_mflags;

			map->map_mflags |= MF_NOFOLDCASE;

			(void) snprintf(buf, sizeof buf, "%010ld", curtime());
			ndbm_map_store(map, "YP_LAST_MODIFIED", buf);

			(void) gethostname(buf, sizeof buf);
			ndbm_map_store(map, "YP_MASTER_NAME", buf);

			map->map_mflags = save_mflags;
		}

		if (inclnull)
			map->map_mflags |= MF_INCLNULL;
# endif /* NDBM_YP_COMPAT */

		/* write out the distinguished alias */
		ndbm_map_store(map, "@", "@");
	}
	dbm_close((DBM *) map->map_db1);

	/* release lock (if needed) */
# if !LOCK_ON_OPEN
	if (map->map_lockfd >= 0)
		(void) close(map->map_lockfd);
# endif /* !LOCK_ON_OPEN */
}

#endif /* NDBM */
/*
**  NEWDB (Hash and BTree) Modules
*/

#ifdef NEWDB

/*
**  BT_MAP_OPEN, HASH_MAP_OPEN -- database open primitives.
**
**	These do rather bizarre locking.  If you can lock on open,
**	do that to avoid the condition of opening a database that
**	is being rebuilt.  If you don't, we'll try to fake it, but
**	there will be a race condition.  If opening for read-only,
**	we immediately release the lock to avoid freezing things up.
**	We really ought to hold the lock, but guarantee that we won't
**	be pokey about it.  That's hard to do.
*/

/* these should be K line arguments */
# if DB_VERSION_MAJOR < 2
#  define db_cachesize	cachesize
#  define h_nelem	nelem
#  ifndef DB_CACHE_SIZE
#   define DB_CACHE_SIZE	(1024 * 1024)	/* database memory cache size */
#  endif /* ! DB_CACHE_SIZE */
#  ifndef DB_HASH_NELEM
#   define DB_HASH_NELEM	4096		/* (starting) size of hash table */
#  endif /* ! DB_HASH_NELEM */
# endif /* DB_VERSION_MAJOR < 2 */

bool
bt_map_open(map, mode)
	MAP *map;
	int mode;
{
# if DB_VERSION_MAJOR < 2
	BTREEINFO btinfo;
# endif /* DB_VERSION_MAJOR < 2 */
# if DB_VERSION_MAJOR == 2
	DB_INFO btinfo;
# endif /* DB_VERSION_MAJOR == 2 */
# if DB_VERSION_MAJOR > 2
	void *btinfo = NULL;
# endif /* DB_VERSION_MAJOR > 2 */

	if (tTd(38, 2))
		dprintf("bt_map_open(%s, %s, %d)\n",
			map->map_mname, map->map_file, mode);

# if DB_VERSION_MAJOR < 3
	memset(&btinfo, '\0', sizeof btinfo);
#  ifdef DB_CACHE_SIZE
	btinfo.db_cachesize = DB_CACHE_SIZE;
#  endif /* DB_CACHE_SIZE */
# endif /* DB_VERSION_MAJOR < 3 */

	return db_map_open(map, mode, "btree", DB_BTREE, &btinfo);
}

bool
hash_map_open(map, mode)
	MAP *map;
	int mode;
{
# if DB_VERSION_MAJOR < 2
	HASHINFO hinfo;
# endif /* DB_VERSION_MAJOR < 2 */
# if DB_VERSION_MAJOR == 2
	DB_INFO hinfo;
# endif /* DB_VERSION_MAJOR == 2 */
# if DB_VERSION_MAJOR > 2
	void *hinfo = NULL;
# endif /* DB_VERSION_MAJOR > 2 */

	if (tTd(38, 2))
		dprintf("hash_map_open(%s, %s, %d)\n",
			map->map_mname, map->map_file, mode);

# if DB_VERSION_MAJOR < 3
	memset(&hinfo, '\0', sizeof hinfo);
#  ifdef DB_HASH_NELEM
	hinfo.h_nelem = DB_HASH_NELEM;
#  endif /* DB_HASH_NELEM */
#  ifdef DB_CACHE_SIZE
	hinfo.db_cachesize = DB_CACHE_SIZE;
#  endif /* DB_CACHE_SIZE */
# endif /* DB_VERSION_MAJOR < 3 */

	return db_map_open(map, mode, "hash", DB_HASH, &hinfo);
}

static bool
db_map_open(map, mode, mapclassname, dbtype, openinfo)
	MAP *map;
	int mode;
	char *mapclassname;
	DBTYPE dbtype;
# if DB_VERSION_MAJOR < 2
	const void *openinfo;
# endif /* DB_VERSION_MAJOR < 2 */
# if DB_VERSION_MAJOR == 2
	DB_INFO *openinfo;
# endif /* DB_VERSION_MAJOR == 2 */
# if DB_VERSION_MAJOR > 2
	void **openinfo;
# endif /* DB_VERSION_MAJOR > 2 */
{
	DB *db = NULL;
	int i;
	int omode;
	int smode = S_IREAD;
	int fd;
	long sff;
	int save_errno;
	struct stat st;
	char buf[MAXNAME + 1];

	/* do initial file and directory checks */
	(void) strlcpy(buf, map->map_file, sizeof buf - 3);
	i = strlen(buf);
	if (i < 3 || strcmp(&buf[i - 3], ".db") != 0)
		(void) strlcat(buf, ".db", sizeof buf);

	mode &= O_ACCMODE;
	omode = mode;

	sff = SFF_ROOTOK|SFF_REGONLY;
	if (mode == O_RDWR)
	{
		sff |= SFF_CREAT;
		if (!bitnset(DBS_WRITEMAPTOSYMLINK, DontBlameSendmail))
			sff |= SFF_NOSLINK;
		if (!bitnset(DBS_WRITEMAPTOHARDLINK, DontBlameSendmail))
			sff |= SFF_NOHLINK;
		smode = S_IWRITE;
	}
	else
	{
		if (!bitnset(DBS_LINKEDMAPINWRITABLEDIR, DontBlameSendmail))
			sff |= SFF_NOWLINK;
	}
	if (!bitnset(DBS_MAPINUNSAFEDIRPATH, DontBlameSendmail))
		sff |= SFF_SAFEDIRPATH;
	i = safefile(buf, RunAsUid, RunAsGid, RunAsUserName, sff, smode, &st);

# if !_FFR_REMOVE_AUTOREBUILD
	if (i == ENOENT && AutoRebuild &&
	    bitset(MCF_REBUILDABLE, map->map_class->map_cflags) &&
	    (bitset(MF_IMPL_HASH, map->map_mflags) ||
	     bitset(MF_ALIAS, map->map_mflags)) &&
	    mode == O_RDONLY)
	{
		bool impl = bitset(MF_IMPL_HASH, map->map_mflags);

		/* may be able to rebuild */
		map->map_mflags &= ~MF_IMPL_HASH;
		if (!rebuildaliases(map, TRUE))
			return FALSE;
		if (impl)
			return impl_map_open(map, O_RDONLY);
		else
			return db_map_open(map, O_RDONLY, mapclassname,
					   dbtype, openinfo);
	}
# endif /* !_FFR_REMOVE_AUTOREBUILD */

	if (i != 0)
	{
		char *prob = "unsafe";

		/* cannot open this map */
		if (i == ENOENT)
			prob = "missing";
		if (tTd(38, 2))
			dprintf("\t%s map file: %s\n", prob, errstring(i));
		errno = i;
		if (!bitset(MF_OPTIONAL, map->map_mflags))
			syserr("%s map \"%s\": %s map file %s",
				mapclassname, map->map_mname, prob, buf);
		return FALSE;
	}
	if (st.st_mode == ST_MODE_NOFILE)
		omode |= O_CREAT|O_EXCL;

	map->map_lockfd = -1;

# if LOCK_ON_OPEN
	if (mode == O_RDWR)
		omode |= O_TRUNC|O_EXLOCK;
	else
		omode |= O_SHLOCK;
# else /* LOCK_ON_OPEN */
	/*
	**  Pre-lock the file to avoid race conditions.  In particular,
	**  since dbopen returns NULL if the file is zero length, we
	**  must have a locked instance around the dbopen.
	*/

	fd = open(buf, omode, DBMMODE);
	if (fd < 0)
	{
		if (!bitset(MF_OPTIONAL, map->map_mflags))
			syserr("db_map_open: cannot pre-open database %s", buf);
		return FALSE;
	}

	/* make sure no baddies slipped in just before the open... */
	if (filechanged(buf, fd, &st))
	{
		save_errno = errno;
		(void) close(fd);
		errno = save_errno;
		syserr("db_map_open(%s): file changed after pre-open", buf);
		return FALSE;
	}

	/* if new file, get the "before" bits for later filechanged check */
	if (st.st_mode == ST_MODE_NOFILE && fstat(fd, &st) < 0)
	{
		save_errno = errno;
		(void) close(fd);
		errno = save_errno;
		syserr("db_map_open(%s): cannot fstat pre-opened file",
			buf);
		return FALSE;
	}

	/* actually lock the pre-opened file */
	if (!lockfile(fd, buf, NULL, mode == O_RDONLY ? LOCK_SH : LOCK_EX))
		syserr("db_map_open: cannot lock %s", buf);

	/* set up mode bits for dbopen */
	if (mode == O_RDWR)
		omode |= O_TRUNC;
	omode &= ~(O_EXCL|O_CREAT);
# endif /* LOCK_ON_OPEN */

# if DB_VERSION_MAJOR < 2
	db = dbopen(buf, omode, DBMMODE, dbtype, openinfo);
# else /* DB_VERSION_MAJOR < 2 */
	{
		int flags = 0;
#  if DB_VERSION_MAJOR > 2
		int ret;
#  endif /* DB_VERSION_MAJOR > 2 */

		if (mode == O_RDONLY)
			flags |= DB_RDONLY;
		if (bitset(O_CREAT, omode))
			flags |= DB_CREATE;
		if (bitset(O_TRUNC, omode))
			flags |= DB_TRUNCATE;

#  if !HASFLOCK && defined(DB_FCNTL_LOCKING)
		flags |= DB_FCNTL_LOCKING;
#  endif /* !HASFLOCK && defined(DB_FCNTL_LOCKING) */

#  if DB_VERSION_MAJOR > 2
		ret = db_create(&db, NULL, 0);
#  ifdef DB_CACHE_SIZE
		if (ret == 0 && db != NULL)
		{
			ret = db->set_cachesize(db, 0, DB_CACHE_SIZE, 0);
			if (ret != 0)
			{
				(void) db->close(db, 0);
				db = NULL;
			}
		}
#  endif /* DB_CACHE_SIZE */
#  ifdef DB_HASH_NELEM
		if (dbtype == DB_HASH && ret == 0 && db != NULL)
		{
			ret = db->set_h_nelem(db, DB_HASH_NELEM);
			if (ret != 0)
			{
				(void) db->close(db, 0);
				db = NULL;
			}
		}
#  endif /* DB_HASH_NELEM */
		if (ret == 0 && db != NULL)
		{
			ret = db->open(db, buf, NULL, dbtype, flags, DBMMODE);
			if (ret != 0)
			{
#ifdef DB_OLD_VERSION
				if (ret == DB_OLD_VERSION)
					ret = EINVAL;
#endif /* DB_OLD_VERSION */
				(void) db->close(db, 0);
				db = NULL;
			}
		}
		errno = ret;
#  else /* DB_VERSION_MAJOR > 2 */
		errno = db_open(buf, dbtype, flags, DBMMODE,
				NULL, openinfo, &db);
#  endif /* DB_VERSION_MAJOR > 2 */
	}
# endif /* DB_VERSION_MAJOR < 2 */
	save_errno = errno;

# if !LOCK_ON_OPEN
	if (mode == O_RDWR)
		map->map_lockfd = fd;
	else
		(void) close(fd);
# endif /* !LOCK_ON_OPEN */

	if (db == NULL)
	{
		if (mode == O_RDONLY && bitset(MF_ALIAS, map->map_mflags) &&
		    aliaswait(map, ".db", FALSE))
			return TRUE;
# if !LOCK_ON_OPEN
		if (map->map_lockfd >= 0)
			(void) close(map->map_lockfd);
# endif /* !LOCK_ON_OPEN */
		errno = save_errno;
		if (!bitset(MF_OPTIONAL, map->map_mflags))
			syserr("Cannot open %s database %s",
				mapclassname, buf);
		return FALSE;
	}

# if DB_VERSION_MAJOR < 2
	fd = db->fd(db);
# else /* DB_VERSION_MAJOR < 2 */
	fd = -1;
	errno = db->fd(db, &fd);
# endif /* DB_VERSION_MAJOR < 2 */
	if (filechanged(buf, fd, &st))
	{
		save_errno = errno;
# if DB_VERSION_MAJOR < 2
		(void) db->close(db);
# else /* DB_VERSION_MAJOR < 2 */
		errno = db->close(db, 0);
# endif /* DB_VERSION_MAJOR < 2 */
# if !LOCK_ON_OPEN
		if (map->map_lockfd >= 0)
			(void) close(map->map_lockfd);
# endif /* !LOCK_ON_OPEN */
		errno = save_errno;
		syserr("db_map_open(%s): file changed after open", buf);
		return FALSE;
	}

	if (mode == O_RDWR)
		map->map_mflags |= MF_LOCKED;
# if LOCK_ON_OPEN
	if (fd >= 0 && mode == O_RDONLY)
	{
		(void) lockfile(fd, buf, NULL, LOCK_UN);
	}
# endif /* LOCK_ON_OPEN */

	/* try to make sure that at least the database header is on disk */
	if (mode == O_RDWR)
	{
		(void) db->sync(db, 0);
		if (geteuid() == 0 && TrustedUid != 0)
		{
#  if HASFCHOWN
			if (fchown(fd, TrustedUid, -1) < 0)
			{
				int err = errno;

				sm_syslog(LOG_ALERT, NOQID,
					  "ownership change on %s failed: %s",
					  buf, errstring(err));
				message("050 ownership change on %s failed: %s",
					buf, errstring(err));
			}
#  endif /* HASFCHOWN */
		}
	}

	map->map_db2 = (ARBPTR_T) db;

	/*
	**  Need to set map_mtime before the call to aliaswait()
	**  as aliaswait() will call map_lookup() which requires
	**  map_mtime to be set
	*/

	if (fd >= 0 && fstat(fd, &st) >= 0)
		map->map_mtime = st.st_mtime;

	if (mode == O_RDONLY && bitset(MF_ALIAS, map->map_mflags) &&
	    !aliaswait(map, ".db", TRUE))
		return FALSE;
	return TRUE;
}


/*
**  DB_MAP_LOOKUP -- look up a datum in a BTREE- or HASH-type map
*/

char *
db_map_lookup(map, name, av, statp)
	MAP *map;
	char *name;
	char **av;
	int *statp;
{
	DBT key, val;
	register DB *db = (DB *) map->map_db2;
	int i;
	int st;
	int save_errno;
	int fd;
	struct stat stbuf;
	char keybuf[MAXNAME + 1];
	char buf[MAXNAME + 1];

	memset(&key, '\0', sizeof key);
	memset(&val, '\0', sizeof val);

	if (tTd(38, 20))
		dprintf("db_map_lookup(%s, %s)\n",
			map->map_mname, name);

	i = strlen(map->map_file);
	if (i > MAXNAME)
		i = MAXNAME;
	(void) strlcpy(buf, map->map_file, i + 1);
	if (i > 3 && strcmp(&buf[i - 3], ".db") == 0)
		buf[i - 3] = '\0';

	key.size = strlen(name);
	if (key.size > sizeof keybuf - 1)
		key.size = sizeof keybuf - 1;
	key.data = keybuf;
	memmove(keybuf, name, key.size);
	keybuf[key.size] = '\0';
	if (!bitset(MF_NOFOLDCASE, map->map_mflags))
		makelower(keybuf);
  lockdb:
# if DB_VERSION_MAJOR < 2
	fd = db->fd(db);
# else /* DB_VERSION_MAJOR < 2 */
	fd = -1;
	errno = db->fd(db, &fd);
# endif /* DB_VERSION_MAJOR < 2 */
	if (fd >= 0 && !bitset(MF_LOCKED, map->map_mflags))
		(void) lockfile(fd, buf, ".db", LOCK_SH);
	if (fd < 0 || fstat(fd, &stbuf) < 0 || stbuf.st_mtime > map->map_mtime)
	{
		/* Reopen the database to sync the cache */
		int omode = bitset(map->map_mflags, MF_WRITABLE) ? O_RDWR
								 : O_RDONLY;

		if (fd >= 0 && !bitset(MF_LOCKED, map->map_mflags))
			(void) lockfile(fd, buf, ".db", LOCK_UN);
		map->map_mflags |= MF_CLOSING;
		map->map_class->map_close(map);
		map->map_mflags &= ~(MF_OPEN|MF_WRITABLE|MF_CLOSING);
		if (map->map_class->map_open(map, omode))
		{
			map->map_mflags |= MF_OPEN;
			map->map_pid = getpid();
			if ((omode && O_ACCMODE) == O_RDWR)
				map->map_mflags |= MF_WRITABLE;
			db = (DB *) map->map_db2;
			goto lockdb;
		}
		else
		{
			if (!bitset(MF_OPTIONAL, map->map_mflags))
			{
				extern MAPCLASS BogusMapClass;

				*statp = EX_TEMPFAIL;
				map->map_class = &BogusMapClass;
				map->map_mflags |= MF_OPEN;
				map->map_pid = getpid();
				syserr("Cannot reopen DB database %s",
					map->map_file);
			}
			return NULL;
		}
	}

	st = 1;
	if (bitset(MF_TRY0NULL, map->map_mflags))
	{
# if DB_VERSION_MAJOR < 2
		st = db->get(db, &key, &val, 0);
# else /* DB_VERSION_MAJOR < 2 */
		errno = db->get(db, NULL, &key, &val, 0);
		switch (errno)
		{
		  case DB_NOTFOUND:
		  case DB_KEYEMPTY:
			st = 1;
			break;

		  case 0:
			st = 0;
			break;

		  default:
			st = -1;
			break;
		}
# endif /* DB_VERSION_MAJOR < 2 */
		if (st == 0)
			map->map_mflags &= ~MF_TRY1NULL;
	}
	if (st != 0 && bitset(MF_TRY1NULL, map->map_mflags))
	{
		key.size++;
# if DB_VERSION_MAJOR < 2
		st = db->get(db, &key, &val, 0);
# else /* DB_VERSION_MAJOR < 2 */
		errno = db->get(db, NULL, &key, &val, 0);
		switch (errno)
		{
		  case DB_NOTFOUND:
		  case DB_KEYEMPTY:
			st = 1;
			break;

		  case 0:
			st = 0;
			break;

		  default:
			st = -1;
			break;
		}
# endif /* DB_VERSION_MAJOR < 2 */
		if (st == 0)
			map->map_mflags &= ~MF_TRY0NULL;
	}
	save_errno = errno;
	if (fd >= 0 && !bitset(MF_LOCKED, map->map_mflags))
		(void) lockfile(fd, buf, ".db", LOCK_UN);
	if (st != 0)
	{
		errno = save_errno;
		if (st < 0)
			syserr("db_map_lookup: get (%s)", name);
		return NULL;
	}
	if (bitset(MF_MATCHONLY, map->map_mflags))
		return map_rewrite(map, name, strlen(name), NULL);
	else
		return map_rewrite(map, val.data, val.size, av);
}


/*
**  DB_MAP_STORE -- store a datum in the NEWDB database
*/

void
db_map_store(map, lhs, rhs)
	register MAP *map;
	char *lhs;
	char *rhs;
{
	int status;
	DBT key;
	DBT data;
	register DB *db = map->map_db2;
	char keybuf[MAXNAME + 1];

	memset(&key, '\0', sizeof key);
	memset(&data, '\0', sizeof data);

	if (tTd(38, 12))
		dprintf("db_map_store(%s, %s, %s)\n",
			map->map_mname, lhs, rhs);

	key.size = strlen(lhs);
	key.data = lhs;
	if (!bitset(MF_NOFOLDCASE, map->map_mflags))
	{
		if (key.size > sizeof keybuf - 1)
			key.size = sizeof keybuf - 1;
		memmove(keybuf, key.data, key.size);
		keybuf[key.size] = '\0';
		makelower(keybuf);
		key.data = keybuf;
	}

	data.size = strlen(rhs);
	data.data = rhs;

	if (bitset(MF_INCLNULL, map->map_mflags))
	{
		key.size++;
		data.size++;
	}

# if DB_VERSION_MAJOR < 2
	status = db->put(db, &key, &data, R_NOOVERWRITE);
# else /* DB_VERSION_MAJOR < 2 */
	errno = db->put(db, NULL, &key, &data, DB_NOOVERWRITE);
	switch (errno)
	{
	  case DB_KEYEXIST:
		status = 1;
		break;

	  case 0:
		status = 0;
		break;

	  default:
		status = -1;
		break;
	}
# endif /* DB_VERSION_MAJOR < 2 */
	if (status > 0)
	{
		if (!bitset(MF_APPEND, map->map_mflags))
			message("050 Warning: duplicate alias name %s", lhs);
		else
		{
			static char *buf = NULL;
			static int bufsiz = 0;
			DBT old;

			memset(&old, '\0', sizeof old);

			old.data = db_map_lookup(map, key.data,
						 (char **)NULL, &status);
			if (old.data != NULL)
			{
				old.size = strlen(old.data);
				if (data.size + old.size + 2 > (size_t)bufsiz)
				{
					if (buf != NULL)
						sm_free(buf);
					bufsiz = data.size + old.size + 2;
					buf = xalloc(bufsiz);
				}
				snprintf(buf, bufsiz, "%s,%s",
					(char *) data.data, (char *) old.data);
				data.size = data.size + old.size + 1;
				data.data = buf;
				if (tTd(38, 9))
					dprintf("db_map_store append=%s\n",
						(char *) data.data);
			}
		}
# if DB_VERSION_MAJOR < 2
		status = db->put(db, &key, &data, 0);
# else /* DB_VERSION_MAJOR < 2 */
		status = errno = db->put(db, NULL, &key, &data, 0);
# endif /* DB_VERSION_MAJOR < 2 */
	}
	if (status != 0)
		syserr("readaliases: db put (%s)", lhs);
}


/*
**  DB_MAP_CLOSE -- add distinguished entries and close the database
*/

void
db_map_close(map)
	MAP *map;
{
	register DB *db = map->map_db2;

	if (tTd(38, 9))
		dprintf("db_map_close(%s, %s, %lx)\n",
			map->map_mname, map->map_file, map->map_mflags);

	if (bitset(MF_WRITABLE, map->map_mflags))
	{
		/* write out the distinguished alias */
		db_map_store(map, "@", "@");
	}

	(void) db->sync(db, 0);

# if !LOCK_ON_OPEN
	if (map->map_lockfd >= 0)
		(void) close(map->map_lockfd);
# endif /* !LOCK_ON_OPEN */

# if DB_VERSION_MAJOR < 2
	if (db->close(db) != 0)
# else /* DB_VERSION_MAJOR < 2 */
	/*
	**  Berkeley DB can use internal shared memory
	**  locking for its memory pool.  Closing a map
	**  opened by another process will interfere
	**  with the shared memory and locks of the parent
	**  process leaving things in a bad state.
	*/

	/*
	**  If this map was not opened by the current
	**  process, do not close the map but recover
	**  the file descriptor.
	*/
	if (map->map_pid != getpid())
	{
		int fd = -1;

		errno = db->fd(db, &fd);
		if (fd >= 0)
			(void) close(fd);
		return;
	}

	if ((errno = db->close(db, 0)) != 0)
# endif /* DB_VERSION_MAJOR < 2 */
		syserr("db_map_close(%s, %s, %lx): db close failure",
			map->map_mname, map->map_file, map->map_mflags);
}
#endif /* NEWDB */
/*
**  NIS Modules
*/

#ifdef NIS

# ifndef YPERR_BUSY
#  define YPERR_BUSY	16
# endif /* ! YPERR_BUSY */

/*
**  NIS_MAP_OPEN -- open DBM map
*/

bool
nis_map_open(map, mode)
	MAP *map;
	int mode;
{
	int yperr;
	register char *p;
	auto char *vp;
	auto int vsize;

	if (tTd(38, 2))
		dprintf("nis_map_open(%s, %s, %d)\n",
			map->map_mname, map->map_file, mode);

	mode &= O_ACCMODE;
	if (mode != O_RDONLY)
	{
		/* issue a pseudo-error message */
# ifdef ENOSYS
		errno = ENOSYS;
# else /* ENOSYS */
#  ifdef EFTYPE
		errno = EFTYPE;
#  else /* EFTYPE */
		errno = ENXIO;
#  endif /* EFTYPE */
# endif /* ENOSYS */
		return FALSE;
	}

	p = strchr(map->map_file, '@');
	if (p != NULL)
	{
		*p++ = '\0';
		if (*p != '\0')
			map->map_domain = p;
	}

	if (*map->map_file == '\0')
		map->map_file = "mail.aliases";

	if (map->map_domain == NULL)
	{
		yperr = yp_get_default_domain(&map->map_domain);
		if (yperr != 0)
		{
			if (!bitset(MF_OPTIONAL, map->map_mflags))
				syserr("421 4.3.5 NIS map %s specified, but NIS not running",
				       map->map_file);
			return FALSE;
		}
	}

	/* check to see if this map actually exists */
	vp = NULL;
	yperr = yp_match(map->map_domain, map->map_file, "@", 1,
			&vp, &vsize);
	if (tTd(38, 10))
		dprintf("nis_map_open: yp_match(@, %s, %s) => %s\n",
			map->map_domain, map->map_file, yperr_string(yperr));
	if (vp != NULL)
		sm_free(vp);

	if (yperr == 0 || yperr == YPERR_KEY || yperr == YPERR_BUSY)
	{
		/*
		**  We ought to be calling aliaswait() here if this is an
		**  alias file, but powerful HP-UX NIS servers  apparently
		**  don't insert the @:@ token into the alias map when it
		**  is rebuilt, so aliaswait() just hangs.  I hate HP-UX.
		*/

# if 0
		if (!bitset(MF_ALIAS, map->map_mflags) ||
		    aliaswait(map, NULL, TRUE))
# endif /* 0 */
			return TRUE;
	}

	if (!bitset(MF_OPTIONAL, map->map_mflags))
	{
		syserr("421 4.0.0 Cannot bind to map %s in domain %s: %s",
			map->map_file, map->map_domain, yperr_string(yperr));
	}

	return FALSE;
}


/*
**  NIS_MAP_LOOKUP -- look up a datum in a NIS map
*/

/* ARGSUSED3 */
char *
nis_map_lookup(map, name, av, statp)
	MAP *map;
	char *name;
	char **av;
	int *statp;
{
	char *vp;
	auto int vsize;
	int buflen;
	int yperr;
	char keybuf[MAXNAME + 1];

	if (tTd(38, 20))
		dprintf("nis_map_lookup(%s, %s)\n",
			map->map_mname, name);

	buflen = strlen(name);
	if (buflen > sizeof keybuf - 1)
		buflen = sizeof keybuf - 1;
	memmove(keybuf, name, buflen);
	keybuf[buflen] = '\0';
	if (!bitset(MF_NOFOLDCASE, map->map_mflags))
		makelower(keybuf);
	yperr = YPERR_KEY;
	vp = NULL;
	if (bitset(MF_TRY0NULL, map->map_mflags))
	{
		yperr = yp_match(map->map_domain, map->map_file, keybuf, buflen,
			     &vp, &vsize);
		if (yperr == 0)
			map->map_mflags &= ~MF_TRY1NULL;
	}
	if (yperr == YPERR_KEY && bitset(MF_TRY1NULL, map->map_mflags))
	{
		if (vp != NULL)
		{
			sm_free(vp);
			vp = NULL;
		}
		buflen++;
		yperr = yp_match(map->map_domain, map->map_file, keybuf, buflen,
			     &vp, &vsize);
		if (yperr == 0)
			map->map_mflags &= ~MF_TRY0NULL;
	}
	if (yperr != 0)
	{
		if (yperr != YPERR_KEY && yperr != YPERR_BUSY)
			map->map_mflags &= ~(MF_VALID|MF_OPEN);
		if (vp != NULL)
			sm_free(vp);
		return NULL;
	}
	if (bitset(MF_MATCHONLY, map->map_mflags))
		return map_rewrite(map, name, strlen(name), NULL);
	else
	{
		char *ret;

		ret = map_rewrite(map, vp, vsize, av);
		if (vp != NULL)
			sm_free(vp);
		return ret;
	}
}


/*
**  NIS_GETCANONNAME -- look up canonical name in NIS
*/

static bool
nis_getcanonname(name, hbsize, statp)
	char *name;
	int hbsize;
	int *statp;
{
	char *vp;
	auto int vsize;
	int keylen;
	int yperr;
	static bool try0null = TRUE;
	static bool try1null = TRUE;
	static char *yp_domain = NULL;
	char host_record[MAXLINE];
	char cbuf[MAXNAME];
	char nbuf[MAXNAME + 1];

	if (tTd(38, 20))
		dprintf("nis_getcanonname(%s)\n", name);

	if (strlcpy(nbuf, name, sizeof nbuf) >= sizeof nbuf)
	{
		*statp = EX_UNAVAILABLE;
		return FALSE;
	}
	(void) shorten_hostname(nbuf);
	keylen = strlen(nbuf);

	if (yp_domain == NULL)
		(void) yp_get_default_domain(&yp_domain);
	makelower(nbuf);
	yperr = YPERR_KEY;
	vp = NULL;
	if (try0null)
	{
		yperr = yp_match(yp_domain, "hosts.byname", nbuf, keylen,
			     &vp, &vsize);
		if (yperr == 0)
			try1null = FALSE;
	}
	if (yperr == YPERR_KEY && try1null)
	{
		if (vp != NULL)
		{
			sm_free(vp);
			vp = NULL;
		}
		keylen++;
		yperr = yp_match(yp_domain, "hosts.byname", nbuf, keylen,
			     &vp, &vsize);
		if (yperr == 0)
			try0null = FALSE;
	}
	if (yperr != 0)
	{
		if (yperr == YPERR_KEY)
			*statp = EX_NOHOST;
		else if (yperr == YPERR_BUSY)
			*statp = EX_TEMPFAIL;
		else
			*statp = EX_UNAVAILABLE;
		if (vp != NULL)
			sm_free(vp);
		return FALSE;
	}
	(void) strlcpy(host_record, vp, sizeof host_record);
	sm_free(vp);
	if (tTd(38, 44))
		dprintf("got record `%s'\n", host_record);
	if (!extract_canonname(nbuf, NULL, host_record, cbuf, sizeof cbuf))
	{
		/* this should not happen, but.... */
		*statp = EX_NOHOST;
		return FALSE;
	}
	if (hbsize <= strlen(cbuf))
	{
		*statp = EX_UNAVAILABLE;
		return FALSE;
	}
	(void) strlcpy(name, cbuf, hbsize);
	*statp = EX_OK;
	return TRUE;
}

#endif /* NIS */
/*
**  NISPLUS Modules
**
**	This code donated by Sun Microsystems.
*/

#ifdef NISPLUS

# undef NIS		/* symbol conflict in nis.h */
# undef T_UNSPEC	/* symbol conflict in nis.h -> ... -> sys/tiuser.h */
# include <rpcsvc/nis.h>
# include <rpcsvc/nislib.h>

# define EN_col(col)	zo_data.objdata_u.en_data.en_cols.en_cols_val[(col)].ec_value.ec_value_val
# define COL_NAME(res,i)	((res->objects.objects_val)->TA_data.ta_cols.ta_cols_val)[i].tc_name
# define COL_MAX(res)	((res->objects.objects_val)->TA_data.ta_cols.ta_cols_len)
# define PARTIAL_NAME(x)	((x)[strlen(x) - 1] != '.')

/*
**  NISPLUS_MAP_OPEN -- open nisplus table
*/

bool
nisplus_map_open(map, mode)
	MAP *map;
	int mode;
{
	nis_result *res = NULL;
	int retry_cnt, max_col, i;
	char qbuf[MAXLINE + NIS_MAXNAMELEN];

	if (tTd(38, 2))
		dprintf("nisplus_map_open(%s, %s, %d)\n",
			map->map_mname, map->map_file, mode);

	mode &= O_ACCMODE;
	if (mode != O_RDONLY)
	{
		errno = EPERM;
		return FALSE;
	}

	if (*map->map_file == '\0')
		map->map_file = "mail_aliases.org_dir";

	if (PARTIAL_NAME(map->map_file) && map->map_domain == NULL)
	{
		/* set default NISPLUS Domain to $m */
		map->map_domain = newstr(nisplus_default_domain());
		if (tTd(38, 2))
			dprintf("nisplus_map_open(%s): using domain %s\n",
				map->map_file, map->map_domain);
	}
	if (!PARTIAL_NAME(map->map_file))
	{
		map->map_domain = newstr("");
		snprintf(qbuf, sizeof qbuf, "%s", map->map_file);
	}
	else
	{
		/* check to see if this map actually exists */
		snprintf(qbuf, sizeof qbuf, "%s.%s",
			map->map_file, map->map_domain);
	}

	retry_cnt = 0;
	while (res == NULL || res->status != NIS_SUCCESS)
	{
		res = nis_lookup(qbuf, FOLLOW_LINKS);
		switch (res->status)
		{
		  case NIS_SUCCESS:
			break;

		  case NIS_TRYAGAIN:
		  case NIS_RPCERROR:
		  case NIS_NAMEUNREACHABLE:
			if (retry_cnt++ > 4)
			{
				errno = EAGAIN;
				return FALSE;
			}
			/* try not to overwhelm hosed server */
			sleep(2);
			break;

		  default:		/* all other nisplus errors */
# if 0
			if (!bitset(MF_OPTIONAL, map->map_mflags))
				syserr("421 4.0.0 Cannot find table %s.%s: %s",
					map->map_file, map->map_domain,
					nis_sperrno(res->status));
# endif /* 0 */
			errno = EAGAIN;
			return FALSE;
		}
	}

	if (NIS_RES_NUMOBJ(res) != 1 ||
	    (NIS_RES_OBJECT(res)->zo_data.zo_type != TABLE_OBJ))
	{
		if (tTd(38, 10))
			dprintf("nisplus_map_open: %s is not a table\n", qbuf);
# if 0
		if (!bitset(MF_OPTIONAL, map->map_mflags))
			syserr("421 4.0.0 %s.%s: %s is not a table",
				map->map_file, map->map_domain,
				nis_sperrno(res->status));
# endif /* 0 */
		errno = EBADF;
		return FALSE;
	}
	/* default key column is column 0 */
	if (map->map_keycolnm == NULL)
		map->map_keycolnm = newstr(COL_NAME(res,0));

	max_col = COL_MAX(res);

	/* verify the key column exist */
	for (i = 0; i< max_col; i++)
	{
		if (strcmp(map->map_keycolnm, COL_NAME(res,i)) == 0)
			break;
	}
	if (i == max_col)
	{
		if (tTd(38, 2))
			dprintf("nisplus_map_open(%s): can not find key column %s\n",
				map->map_file, map->map_keycolnm);
		errno = ENOENT;
		return FALSE;
	}

	/* default value column is the last column */
	if (map->map_valcolnm == NULL)
	{
		map->map_valcolno = max_col - 1;
		return TRUE;
	}

	for (i = 0; i< max_col; i++)
	{
		if (strcmp(map->map_valcolnm, COL_NAME(res,i)) == 0)
		{
			map->map_valcolno = i;
			return TRUE;
		}
	}

	if (tTd(38, 2))
		dprintf("nisplus_map_open(%s): can not find column %s\n",
			map->map_file, map->map_keycolnm);
	errno = ENOENT;
	return FALSE;
}


/*
**  NISPLUS_MAP_LOOKUP -- look up a datum in a NISPLUS table
*/

char *
nisplus_map_lookup(map, name, av, statp)
	MAP *map;
	char *name;
	char **av;
	int *statp;
{
	char *p;
	auto int vsize;
	char *skp;
	int skleft;
	char search_key[MAXNAME + 4];
	char qbuf[MAXLINE + NIS_MAXNAMELEN];
	nis_result *result;

	if (tTd(38, 20))
		dprintf("nisplus_map_lookup(%s, %s)\n",
			map->map_mname, name);

	if (!bitset(MF_OPEN, map->map_mflags))
	{
		if (nisplus_map_open(map, O_RDONLY))
		{
			map->map_mflags |= MF_OPEN;
			map->map_pid = getpid();
		}
		else
		{
			*statp = EX_UNAVAILABLE;
			return NULL;
		}
	}

	/*
	**  Copy the name to the key buffer, escaping double quote characters
	**  by doubling them and quoting "]" and "," to avoid having the
	**  NIS+ parser choke on them.
	*/

	skleft = sizeof search_key - 4;
	skp = search_key;
	for (p = name; *p != '\0' && skleft > 0; p++)
	{
		switch (*p)
		{
		  case ']':
		  case ',':
			/* quote the character */
			*skp++ = '"';
			*skp++ = *p;
			*skp++ = '"';
			skleft -= 3;
			break;

		  case '"':
			/* double the quote */
			*skp++ = '"';
			skleft--;
			/* FALLTHROUGH */

		  default:
			*skp++ = *p;
			skleft--;
			break;
		}
	}
	*skp = '\0';
	if (!bitset(MF_NOFOLDCASE, map->map_mflags))
		makelower(search_key);

	/* construct the query */
	if (PARTIAL_NAME(map->map_file))
		snprintf(qbuf, sizeof qbuf, "[%s=%s],%s.%s",
			map->map_keycolnm, search_key, map->map_file,
			map->map_domain);
	else
		snprintf(qbuf, sizeof qbuf, "[%s=%s],%s",
			map->map_keycolnm, search_key, map->map_file);

	if (tTd(38, 20))
		dprintf("qbuf=%s\n", qbuf);
	result = nis_list(qbuf, FOLLOW_LINKS | FOLLOW_PATH, NULL, NULL);
	if (result->status == NIS_SUCCESS)
	{
		int count;
		char *str;

		if ((count = NIS_RES_NUMOBJ(result)) != 1)
		{
			if (LogLevel > 10)
				sm_syslog(LOG_WARNING, CurEnv->e_id,
					  "%s: lookup error, expected 1 entry, got %d",
					  map->map_file, count);

			/* ignore second entry */
			if (tTd(38, 20))
				dprintf("nisplus_map_lookup(%s), got %d entries, additional entries ignored\n",
					name, count);
		}

		p = ((NIS_RES_OBJECT(result))->EN_col(map->map_valcolno));
		/* set the length of the result */
		if (p == NULL)
			p = "";
		vsize = strlen(p);
		if (tTd(38, 20))
			dprintf("nisplus_map_lookup(%s), found %s\n",
				name, p);
		if (bitset(MF_MATCHONLY, map->map_mflags))
			str = map_rewrite(map, name, strlen(name), NULL);
		else
			str = map_rewrite(map, p, vsize, av);
		nis_freeresult(result);
		*statp = EX_OK;
		return str;
	}
	else
	{
		if (result->status == NIS_NOTFOUND)
			*statp = EX_NOTFOUND;
		else if (result->status == NIS_TRYAGAIN)
			*statp = EX_TEMPFAIL;
		else
		{
			*statp = EX_UNAVAILABLE;
			map->map_mflags &= ~(MF_VALID|MF_OPEN);
		}
	}
	if (tTd(38, 20))
		dprintf("nisplus_map_lookup(%s), failed\n", name);
	nis_freeresult(result);
	return NULL;
}



/*
**  NISPLUS_GETCANONNAME -- look up canonical name in NIS+
*/

static bool
nisplus_getcanonname(name, hbsize, statp)
	char *name;
	int hbsize;
	int *statp;
{
	char *vp;
	auto int vsize;
	nis_result *result;
	char *p;
	char nbuf[MAXNAME + 1];
	char qbuf[MAXLINE + NIS_MAXNAMELEN];

	if (strlen(name) >= sizeof nbuf)
	{
		*statp = EX_UNAVAILABLE;
		return FALSE;
	}
	(void) strlcpy(nbuf, name, sizeof nbuf);
	(void) shorten_hostname(nbuf);

	p = strchr(nbuf, '.');
	if (p == NULL)
	{
		/* single token */
		snprintf(qbuf, sizeof qbuf, "[name=%s],hosts.org_dir", nbuf);
	}
	else if (p[1] != '\0')
	{
		/* multi token -- take only first token in nbuf */
		*p = '\0';
		snprintf(qbuf, sizeof qbuf, "[name=%s],hosts.org_dir.%s",
			nbuf, &p[1]);
	}
	else
	{
		*statp = EX_NOHOST;
		return FALSE;
	}

	if (tTd(38, 20))
		dprintf("\nnisplus_getcanoname(%s), qbuf=%s\n",
			name, qbuf);

	result = nis_list(qbuf, EXPAND_NAME|FOLLOW_LINKS|FOLLOW_PATH,
		NULL, NULL);

	if (result->status == NIS_SUCCESS)
	{
		int count;
		char *domain;

		if ((count = NIS_RES_NUMOBJ(result)) != 1)
		{
			if (LogLevel > 10)
				sm_syslog(LOG_WARNING, CurEnv->e_id,
					  "nisplus_getcanonname: lookup error, expected 1 entry, got %d",
					  count);

			/* ignore second entry */
			if (tTd(38, 20))
				dprintf("nisplus_getcanoname(%s), got %d entries, all but first ignored\n",
					name, count);
		}

		if (tTd(38, 20))
			dprintf("nisplus_getcanoname(%s), found in directory \"%s\"\n",
				name, (NIS_RES_OBJECT(result))->zo_domain);


		vp = ((NIS_RES_OBJECT(result))->EN_col(0));
		vsize = strlen(vp);
		if (tTd(38, 20))
			dprintf("nisplus_getcanonname(%s), found %s\n",
				name, vp);
		if (strchr(vp, '.') != NULL)
		{
			domain = "";
		}
		else
		{
			domain = macvalue('m', CurEnv);
			if (domain == NULL)
				domain = "";
		}
		if (hbsize > vsize + (int) strlen(domain) + 1)
		{
			if (domain[0] == '\0')
				(void) strlcpy(name, vp, hbsize);
			else
				snprintf(name, hbsize, "%s.%s", vp, domain);
			*statp = EX_OK;
		}
		else
			*statp = EX_NOHOST;
		nis_freeresult(result);
		return TRUE;
	}
	else
	{
		if (result->status == NIS_NOTFOUND)
			*statp = EX_NOHOST;
		else if (result->status == NIS_TRYAGAIN)
			*statp = EX_TEMPFAIL;
		else
			*statp = EX_UNAVAILABLE;
	}
	if (tTd(38, 20))
		dprintf("nisplus_getcanonname(%s), failed, status=%d, nsw_stat=%d\n",
			name, result->status, *statp);
	nis_freeresult(result);
	return FALSE;
}

char *
nisplus_default_domain()
{
	static char default_domain[MAXNAME + 1] = "";
	char *p;

	if (default_domain[0] != '\0')
		return default_domain;

	p = nis_local_directory();
	snprintf(default_domain, sizeof default_domain, "%s", p);
	return default_domain;
}

#endif /* NISPLUS */
/*
**  LDAP Modules
*/

/*
**  LDAPMAP_DEQUOTE - helper routine for ldapmap_parseargs
*/

#if defined(LDAPMAP) || defined(PH_MAP)

# ifdef PH_MAP
#  define ph_map_dequote ldapmap_dequote
# endif /* PH_MAP */

char *
ldapmap_dequote(str)
	char *str;
{
	char *p;
	char *start;

	if (str == NULL)
		return NULL;

	p = str;
	if (*p == '"')
	{
		/* Should probably swallow initial whitespace here */
		start = ++p;
	}
	else
		return str;
	while (*p != '"' && *p != '\0')
		p++;
	if (*p != '\0')
		*p = '\0';
	return start;
}
#endif /* defined(LDAPMAP) || defined(PH_MAP) */

#ifdef LDAPMAP

LDAPMAP_STRUCT *LDAPDefaults = NULL;

/*
**  LDAPMAP_OPEN -- open LDAP map
**
**	Connect to the LDAP server.  Re-use existing connections since a
**	single server connection to a host (with the same host, port,
**	bind DN, and secret) can answer queries for multiple maps.
*/

bool
ldapmap_open(map, mode)
	MAP *map;
	int mode;
{
	LDAPMAP_STRUCT *lmap;
	STAB *s;

	if (tTd(38, 2))
		dprintf("ldapmap_open(%s, %d): ", map->map_mname, mode);

	mode &= O_ACCMODE;

	/* sendmail doesn't have the ability to write to LDAP (yet) */
	if (mode != O_RDONLY)
	{
		/* issue a pseudo-error message */
# ifdef ENOSYS
		errno = ENOSYS;
# else /* ENOSYS */
#  ifdef EFTYPE
		errno = EFTYPE;
#  else /* EFTYPE */
		errno = ENXIO;
#  endif /* EFTYPE */
# endif /* ENOSYS */
		return FALSE;
	}

	/* Comma separate if used as an alias file */
	if (map->map_coldelim == '\0' && bitset(MF_ALIAS, map->map_mflags))
		map->map_coldelim = ',';

	lmap = (LDAPMAP_STRUCT *) map->map_db1;

	s = ldapmap_findconn(lmap);
	if (s->s_lmap != NULL)
	{
		/* Already have a connection open to this LDAP server */
		lmap->ldap_ld = ((LDAPMAP_STRUCT *)s->s_lmap->map_db1)->ldap_ld;

		/* Add this map as head of linked list */
		lmap->ldap_next = s->s_lmap;
		s->s_lmap = map;

		if (tTd(38, 2))
			dprintf("using cached connection\n");
		return TRUE;
	}

	if (tTd(38, 2))
		dprintf("opening new connection\n");

	/* No connection yet, connect */
	if (!ldapmap_start(map))
		return FALSE;

	/* Save connection for reuse */
	s->s_lmap = map;
	return TRUE;
}

/*
**  LDAPMAP_START -- actually connect to an LDAP server
**
**	Parameters:
**		map -- the map being opened.
**
**	Returns:
**		TRUE if connection is successful, FALSE otherwise.
**
**	Side Effects:
**		Populates lmap->ldap_ld.
*/

static jmp_buf	LDAPTimeout;

static bool
ldapmap_start(map)
	MAP *map;
{
	register int bind_result;
	int save_errno;
	register EVENT *ev = NULL;
	LDAPMAP_STRUCT *lmap;
	LDAP *ld;

	if (tTd(38, 2))
		dprintf("ldapmap_start(%s)\n", map->map_mname);

	lmap = (LDAPMAP_STRUCT *) map->map_db1;

	if (tTd(38,9))
		dprintf("ldapmap_start(%s, %d)\n",
			lmap->ldap_host == NULL ? "localhost" : lmap->ldap_host,
			lmap->ldap_port);

# if USE_LDAP_INIT
	ld = ldap_init(lmap->ldap_host, lmap->ldap_port);
	save_errno = errno;
# else /* USE_LDAP_INIT */
	/*
	**  If using ldap_open(), the actual connection to the server
	**  happens now so we need the timeout here.  For ldap_init(),
	**  the connection happens at bind time.
	*/

	/* set the timeout */
	if (lmap->ldap_timeout.tv_sec != 0)
	{
		if (setjmp(LDAPTimeout) != 0)
		{
			if (LogLevel > 1)
				sm_syslog(LOG_NOTICE, CurEnv->e_id,
					  "timeout conning to LDAP server %.100s",
					  lmap->ldap_host == NULL ? "localhost" : lmap->ldap_host);
			return FALSE;
		}
		ev = setevent(lmap->ldap_timeout.tv_sec, ldaptimeout, 0);
	}

	ld = ldap_open(lmap->ldap_host, lmap->ldap_port);
	save_errno = errno;

	/* clear the event if it has not sprung */
	if (ev != NULL)
		clrevent(ev);
# endif /* USE_LDAP_INIT */

	errno = save_errno;
	if (ld == NULL)
	{
		if (!bitset(MF_OPTIONAL, map->map_mflags))
		{
			if (bitset(MF_NODEFER, map->map_mflags))
				syserr("%s failed to %s in map %s",
# if USE_LDAP_INIT
				       "ldap_init",
# else /* USE_LDAP_INIT */
				       "ldap_open",
# endif /* USE_LDAP_INIT */
				       lmap->ldap_host == NULL ? "localhost"
							       : lmap->ldap_host,
				       map->map_mname);
			else
				syserr("421 4.0.0 %s failed to %s in map %s",
# if USE_LDAP_INIT
				       "ldap_init",
# else /* USE_LDAP_INIT */
				       "ldap_open",
# endif /* USE_LDAP_INIT */
				       lmap->ldap_host == NULL ? "localhost"
							       : lmap->ldap_host,
				       map->map_mname);
		}
		return FALSE;
	}

	ldapmap_setopts(ld, lmap);

# if USE_LDAP_INIT
	/*
	**  If using ldap_init(), the actual connection to the server
	**  happens at ldap_bind_s() so we need the timeout here.
	*/

	/* set the timeout */
	if (lmap->ldap_timeout.tv_sec != 0)
	{
		if (setjmp(LDAPTimeout) != 0)
		{
			if (LogLevel > 1)
				sm_syslog(LOG_NOTICE, CurEnv->e_id,
					  "timeout conning to LDAP server %.100s",
					  lmap->ldap_host == NULL ? "localhost"
								  : lmap->ldap_host);
			return FALSE;
		}
		ev = setevent(lmap->ldap_timeout.tv_sec, ldaptimeout, 0);
	}
# endif /* USE_LDAP_INIT */

# ifdef LDAP_AUTH_KRBV4
	if (lmap->ldap_method == LDAP_AUTH_KRBV4 &&
	    lmap->ldap_secret != NULL)
	{
		/*
		**  Need to put ticket in environment here instead of
		**  during parseargs as there may be different tickets
		**  for different LDAP connections.
		*/

		(void) putenv(lmap->ldap_secret);
	}
# endif /* LDAP_AUTH_KRBV4 */

	bind_result = ldap_bind_s(ld, lmap->ldap_binddn,
				  lmap->ldap_secret, lmap->ldap_method);

# if USE_LDAP_INIT
	/* clear the event if it has not sprung */
	if (ev != NULL)
		clrevent(ev);
# endif /* USE_LDAP_INIT */

	if (bind_result != LDAP_SUCCESS)
	{
		errno = bind_result + E_LDAPBASE;
		if (!bitset(MF_OPTIONAL, map->map_mflags))
		{
			syserr("421 4.0.0 Cannot bind to map %s in ldap server %s",
			       map->map_mname,
			       lmap->ldap_host == NULL ? "localhost" : lmap->ldap_host);
		}
		return FALSE;
	}

	/* We need to cast ld into the map structure */
	lmap->ldap_ld = ld;
	return TRUE;
}

/* ARGSUSED */
static void
ldaptimeout(sig_no)
	int sig_no;
{
	/*
	**  NOTE: THIS CAN BE CALLED FROM A SIGNAL HANDLER.  DO NOT ADD
	**	ANYTHING TO THIS ROUTINE UNLESS YOU KNOW WHAT YOU ARE
	**	DOING.
	*/

	errno = ETIMEDOUT;
	longjmp(LDAPTimeout, 1);
}

/*
**  LDAPMAP_CLOSE -- close ldap map
*/

void
ldapmap_close(map)
	MAP *map;
{
	LDAPMAP_STRUCT *lmap;
	STAB *s;

	if (tTd(38, 2))
		dprintf("ldapmap_close(%s)\n", map->map_mname);

	lmap = (LDAPMAP_STRUCT *) map->map_db1;

	/* Check if already closed */
	if (lmap->ldap_ld == NULL)
		return;

	/* Close the LDAP connection */
	ldap_unbind(lmap->ldap_ld);

	/* Mark all the maps that share the connection as closed */
	s = ldapmap_findconn(lmap);

	while (s->s_lmap != NULL)
	{
		MAP *smap = s->s_lmap;

		if (tTd(38, 2) && smap != map)
			dprintf("ldapmap_close(%s): closed %s (shared LDAP connection)\n",
				map->map_mname, smap->map_mname);

		smap->map_mflags &= ~(MF_OPEN|MF_WRITABLE);
		lmap = (LDAPMAP_STRUCT *) smap->map_db1;
		lmap->ldap_ld = NULL;
		s->s_lmap = lmap->ldap_next;
		lmap->ldap_next = NULL;
	}
}

# ifdef SUNET_ID
/*
**  SUNET_ID_HASH -- Convert a string to it's Sunet_id canonical form
**  This only makes sense at Stanford University.
*/

char *
sunet_id_hash(str)
	char *str;
{
	char *p, *p_last;

	p = str;
	p_last = p;
	while (*p != '\0')
	{
		if (islower(*p) || isdigit(*p))
		{
			*p_last = *p;
			p_last++;
		}
		else if (isupper(*p))
		{
			*p_last = tolower(*p);
			p_last++;
		}
		++p;
	}
	if (*p_last != '\0')
		*p_last = '\0';
	return str;
}
# endif /* SUNET_ID */

/*
**  LDAPMAP_LOOKUP -- look up a datum in a LDAP map
*/

char *
ldapmap_lookup(map, name, av, statp)
	MAP *map;
	char *name;
	char **av;
	int *statp;
{
	int i;
	int entries = 0;
	int msgid;
	int ret;
	int vsize;
	char *fp, *vp;
	char *p, *q;
	char *result = NULL;
	LDAPMAP_STRUCT *lmap = NULL;
	char keybuf[MAXNAME + 1];
	char filter[LDAPMAP_MAX_FILTER + 1];

	if (tTd(38, 20))
		dprintf("ldapmap_lookup(%s, %s)\n", map->map_mname, name);

	/* Get ldap struct pointer from map */
	lmap = (LDAPMAP_STRUCT *) map->map_db1;
	ldapmap_setopts(lmap->ldap_ld, lmap);

	(void) strlcpy(keybuf, name, sizeof keybuf);

	if (!bitset(MF_NOFOLDCASE, map->map_mflags))
	{
# ifdef SUNET_ID
		sunet_id_hash(keybuf);
# else /* SUNET_ID */
		makelower(keybuf);
# endif /* SUNET_ID */
	}

	/* substitute keybuf into filter, perhaps multiple times */
	memset(filter, '\0', sizeof filter);
	fp = filter;
	p = lmap->ldap_filter;
	while ((q = strchr(p, '%')) != NULL)
	{
		if (q[1] == 's')
		{
			snprintf(fp, SPACELEFT(filter, fp), "%.*s%s",
				 (int) (q - p), p, keybuf);
			fp += strlen(fp);
			p = q + 2;
		}
		else if (q[1] == '0')
		{
			char *k = keybuf;

			snprintf(fp, SPACELEFT(filter, fp), "%.*s",
				 (int) (q - p), p);
			fp += strlen(fp);
			p = q + 2;

			/* Properly escape LDAP special characters */
			while (SPACELEFT(filter, fp) > 0 &&
			       *k != '\0')
			{
				if (*k == '*' || *k == '(' ||
				    *k == ')' || *k == '\\')
				{
					(void) strlcat(fp,
						       (*k == '*' ? "\\2A" :
							(*k == '(' ? "\\28" :
							 (*k == ')' ? "\\29" :
							  (*k == '\\' ? "\\5C" :
							   "\00")))),
						SPACELEFT(filter, fp));
					fp += strlen(fp);
					k++;
				}
				else
					*fp++ = *k++;
			}
		}
		else
		{
			snprintf(fp, SPACELEFT(filter, fp), "%.*s",
				 (int) (q - p + 1), p);
			p = q + (q[1] == '%' ? 2 : 1);
			fp += strlen(fp);
		}
	}
	snprintf(fp, SPACELEFT(filter, fp), "%s", p);
	if (tTd(38, 20))
		dprintf("ldap search filter=%s\n", filter);

	lmap->ldap_res = NULL;
	msgid = ldap_search(lmap->ldap_ld, lmap->ldap_base, lmap->ldap_scope,
			    filter,
			    (lmap->ldap_attr[0] == NULL ? NULL :
			     lmap->ldap_attr),
			    lmap->ldap_attrsonly);
	if (msgid == -1)
	{
		int save_errno;

		errno = ldapmap_geterrno(lmap->ldap_ld) + E_LDAPBASE;
		save_errno = errno;
		if (!bitset(MF_OPTIONAL, map->map_mflags))
		{
			if (bitset(MF_NODEFER, map->map_mflags))
				syserr("Error in ldap_search using %s in map %s",
				       filter, map->map_mname);
			else
				syserr("421 4.0.0 Error in ldap_search using %s in map %s",
				       filter, map->map_mname);
		}
		*statp = EX_TEMPFAIL;
#ifdef LDAP_SERVER_DOWN
		errno = save_errno;
		if (errno == LDAP_SERVER_DOWN + E_LDAPBASE)
		{
			/* server disappeared, try reopen on next search */
			ldapmap_close(map);
		}
#endif /* LDAP_SERVER_DOWN */
		errno = save_errno;
		return NULL;
	}

	*statp = EX_NOTFOUND;
	vp = NULL;

	/* Get results (all if MF_NOREWRITE, otherwise one by one) */
	while ((ret = ldap_result(lmap->ldap_ld, msgid,
				  bitset(MF_NOREWRITE, map->map_mflags),
				  (lmap->ldap_timeout.tv_sec == 0 ? NULL :
				   &(lmap->ldap_timeout)),
				  &(lmap->ldap_res))) == LDAP_RES_SEARCH_ENTRY)
	{
		LDAPMessage *entry;

		if (bitset(MF_SINGLEMATCH, map->map_mflags))
		{
			entries += ldap_count_entries(lmap->ldap_ld,
						      lmap->ldap_res);
			if (entries > 1)
			{
				*statp = EX_NOTFOUND;
				if (lmap->ldap_res != NULL)
				{
					ldap_msgfree(lmap->ldap_res);
					lmap->ldap_res = NULL;
				}
				(void) ldap_abandon(lmap->ldap_ld, msgid);
				if (vp != NULL)
					sm_free(vp);
				if (tTd(38, 25))
					dprintf("ldap search found multiple on a single match query\n");
				return NULL;
			}
		}

		/* If we don't want multiple values and we have one, break */
		if (map->map_coldelim == '\0' && vp != NULL)
			break;

		/* Cycle through all entries */
		for (entry = ldap_first_entry(lmap->ldap_ld, lmap->ldap_res);
		     entry != NULL;
		     entry = ldap_next_entry(lmap->ldap_ld, lmap->ldap_res))
		{
			BerElement *ber;
			char *attr;
			char **vals = NULL;

			/*
			**  If matching only and found an entry,
			**  no need to spin through attributes
			*/

			if (*statp == EX_OK &&
			    bitset(MF_MATCHONLY, map->map_mflags))
				continue;

# if !defined(LDAP_VERSION_MAX) && !defined(LDAP_OPT_SIZELIMIT)
			/*
			**  Reset value to prevent lingering
			**  LDAP_DECODING_ERROR due to
			**  OpenLDAP 1.X's hack (see below)
			*/

			lmap->ldap_ld->ld_errno = LDAP_SUCCESS;
# endif /* !defined(LDAP_VERSION_MAX) !defined(LDAP_OPT_SIZELIMIT) */

			for (attr = ldap_first_attribute(lmap->ldap_ld, entry,
							 &ber);
			     attr != NULL;
			     attr = ldap_next_attribute(lmap->ldap_ld, entry,
							ber))
			{
				char *tmp, *vp_tmp;

				if (lmap->ldap_attrsonly == LDAPMAP_FALSE)
				{
					vals = ldap_get_values(lmap->ldap_ld,
							       entry,
							       attr);
					if (vals == NULL)
					{
						errno = ldapmap_geterrno(lmap->ldap_ld);
						if (errno == LDAP_SUCCESS)
							continue;

						/* Must be an error */
						errno += E_LDAPBASE;
						if (!bitset(MF_OPTIONAL,
							    map->map_mflags))
						{
							if (bitset(MF_NODEFER,
								   map->map_mflags))
								syserr("Error getting LDAP values in map %s",
								       map->map_mname);
							else
								syserr("421 4.0.0 Error getting LDAP values in map %s",
								       map->map_mname);
						}
						*statp = EX_TEMPFAIL;
# if USING_NETSCAPE_LDAP
						ldap_memfree(attr);
# endif /* USING_NETSCAPE_LDAP */
						if (lmap->ldap_res != NULL)
						{
							ldap_msgfree(lmap->ldap_res);
							lmap->ldap_res = NULL;
						}
						(void) ldap_abandon(lmap->ldap_ld,
								    msgid);
						if (vp != NULL)
							sm_free(vp);
						return NULL;
					}
				}

				*statp = EX_OK;

# if !defined(LDAP_VERSION_MAX) && !defined(LDAP_OPT_SIZELIMIT)
				/*
				**  Reset value to prevent lingering
				**  LDAP_DECODING_ERROR due to
				**  OpenLDAP 1.X's hack (see below)
				*/

				lmap->ldap_ld->ld_errno = LDAP_SUCCESS;
# endif /* !defined(LDAP_VERSION_MAX) !defined(LDAP_OPT_SIZELIMIT) */

				/*
				**  If matching only,
				**  no need to spin through entries
				*/

				if (bitset(MF_MATCHONLY, map->map_mflags))
					continue;

				/*
				**  If we don't want multiple values,
				**  return first found.
				*/

				if (map->map_coldelim == '\0')
				{
					if (lmap->ldap_attrsonly == LDAPMAP_TRUE)
					{
						vp = newstr(attr);
# if USING_NETSCAPE_LDAP
						ldap_memfree(attr);
# endif /* USING_NETSCAPE_LDAP */
						break;
					}

					if (vals[0] == NULL)
					{
						ldap_value_free(vals);
# if USING_NETSCAPE_LDAP
						ldap_memfree(attr);
# endif /* USING_NETSCAPE_LDAP */
						continue;
					}

					vp = newstr(vals[0]);
					ldap_value_free(vals);
# if USING_NETSCAPE_LDAP
					ldap_memfree(attr);
# endif /* USING_NETSCAPE_LDAP */
					break;
				}

				/* attributes only */
				if (lmap->ldap_attrsonly == LDAPMAP_TRUE)
				{
					if (vp == NULL)
						vp = newstr(attr);
					else
					{
						vsize = strlen(vp) +
							strlen(attr) + 2;
						tmp = xalloc(vsize);
						snprintf(tmp, vsize, "%s%c%s",
							 vp, map->map_coldelim,
							 attr);
						sm_free(vp);
						vp = tmp;
					}
# if USING_NETSCAPE_LDAP
					ldap_memfree(attr);
# endif /* USING_NETSCAPE_LDAP */
					continue;
				}

				/*
				**  If there is more than one,
				**  munge then into a map_coldelim
				**  separated string
				*/

				vsize = 0;
				for (i = 0; vals[i] != NULL; i++)
					vsize += strlen(vals[i]) + 1;
				vp_tmp = xalloc(vsize);
				*vp_tmp = '\0';

				p = vp_tmp;
				for (i = 0; vals[i] != NULL; i++)
				{
					p += strlcpy(p, vals[i],
						     vsize - (p - vp_tmp));
					if (p >= vp_tmp + vsize)
						syserr("ldapmap_lookup: Internal error: buffer too small for LDAP values");
					if (vals[i + 1] != NULL)
						*p++ = map->map_coldelim;
				}

				ldap_value_free(vals);
# if USING_NETSCAPE_LDAP
				ldap_memfree(attr);
# endif /* USING_NETSCAPE_LDAP */
				if (vp == NULL)
				{
					vp = vp_tmp;
					continue;
				}
				vsize = strlen(vp) + strlen(vp_tmp) + 2;
				tmp = xalloc(vsize);
				snprintf(tmp, vsize, "%s%c%s",
					 vp, map->map_coldelim, vp_tmp);

				sm_free(vp);
				sm_free(vp_tmp);
				vp = tmp;
			}
			errno = ldapmap_geterrno(lmap->ldap_ld);

			/*
			**  We check errno != LDAP_DECODING_ERROR since
			**  OpenLDAP 1.X has a very ugly *undocumented*
			**  hack of returning this error code from
			**  ldap_next_attribute() if the library freed the
			**  ber attribute.  See:
			**  http://www.openldap.org/lists/openldap-devel/9901/msg00064.html
			*/

			if (errno != LDAP_SUCCESS &&
			    errno != LDAP_DECODING_ERROR)
			{
				/* Must be an error */
				errno += E_LDAPBASE;
				if (!bitset(MF_OPTIONAL, map->map_mflags))
				{
					if (bitset(MF_NODEFER, map->map_mflags))
						syserr("Error getting LDAP attributes in map %s",
						       map->map_mname);
					else
						syserr("421 4.0.0 Error getting LDAP attributes in map %s",
						       map->map_mname);
				}
				*statp = EX_TEMPFAIL;
				if (lmap->ldap_res != NULL)
				{
					ldap_msgfree(lmap->ldap_res);
					lmap->ldap_res = NULL;
				}
				(void) ldap_abandon(lmap->ldap_ld, msgid);
				if (vp != NULL)
					sm_free(vp);
				return NULL;
			}

			/* We don't want multiple values and we have one */
			if (map->map_coldelim == '\0' && vp != NULL)
				break;
		}
		errno = ldapmap_geterrno(lmap->ldap_ld);
		if (errno != LDAP_SUCCESS && errno != LDAP_DECODING_ERROR)
		{
			/* Must be an error */
			errno += E_LDAPBASE;
			if (!bitset(MF_OPTIONAL, map->map_mflags))
			{
				if (bitset(MF_NODEFER, map->map_mflags))
					syserr("Error getting LDAP entries in map %s",
					       map->map_mname);
				else
					syserr("421 4.0.0 Error getting LDAP entries in map %s",
					       map->map_mname);
			}
			*statp = EX_TEMPFAIL;
			if (lmap->ldap_res != NULL)
			{
				ldap_msgfree(lmap->ldap_res);
				lmap->ldap_res = NULL;
			}
			(void) ldap_abandon(lmap->ldap_ld, msgid);
			if (vp != NULL)
				sm_free(vp);
			return NULL;
		}
		ldap_msgfree(lmap->ldap_res);
		lmap->ldap_res = NULL;
	}

	/*
	**  If grabbing all results at once for MF_NOREWRITE and
	**  only want a single match, make sure that's all we have
	*/

	if (ret == LDAP_RES_SEARCH_RESULT &&
	    bitset(MF_NOREWRITE|MF_SINGLEMATCH, map->map_mflags))
	{
		entries += ldap_count_entries(lmap->ldap_ld, lmap->ldap_res);
		if (entries > 1)
		{
			*statp = EX_NOTFOUND;
			if (lmap->ldap_res != NULL)
			{
				ldap_msgfree(lmap->ldap_res);
				lmap->ldap_res = NULL;
			}
			if (vp != NULL)
				sm_free(vp);
			return NULL;
		}
		*statp = EX_OK;
	}

	if (ret == 0)
		errno = ETIMEDOUT;
	else
		errno = ldapmap_geterrno(lmap->ldap_ld);
	if (errno != LDAP_SUCCESS)
	{
		int save_errno;

		/* Must be an error */
		if (ret != 0)
			errno += E_LDAPBASE;
		save_errno = errno;

		if (!bitset(MF_OPTIONAL, map->map_mflags))
		{
			if (bitset(MF_NODEFER, map->map_mflags))
				syserr("Error getting LDAP results in map %s",
				       map->map_mname);
			else
				syserr("421 4.0.0 Error getting LDAP results in map %s",
				       map->map_mname);
		}
		*statp = EX_TEMPFAIL;
		if (vp != NULL)
			sm_free(vp);
#ifdef LDAP_SERVER_DOWN
		errno = save_errno;
		if (errno == LDAP_SERVER_DOWN + E_LDAPBASE)
		{
			/* server disappeared, try reopen on next search */
			ldapmap_close(map);
		}
#endif /* LDAP_SERVER_DOWN */
		errno = save_errno;
		return NULL;
	}

	/* Did we match anything? */
	if (vp == NULL && !bitset(MF_MATCHONLY, map->map_mflags))
		return NULL;

	/*
	**  If MF_NOREWRITE, we are special map which doesn't
	**  actually return a map value.  Instead, we don't free
	**  ldap_res and let the calling function process the LDAP
	**  results.  The caller should ldap_msgfree(lmap->ldap_res).
	*/

	if (bitset(MF_NOREWRITE, map->map_mflags))
	{
		if (vp != NULL)
			sm_free(vp);
		return "";
	}

	if (*statp == EX_OK)
	{
		if (LogLevel > 9)
			sm_syslog(LOG_INFO, CurEnv->e_id,
				  "ldap %.100s => %s", name,
				  vp == NULL ? "<NULL>" : vp);
		if (bitset(MF_MATCHONLY, map->map_mflags))
			result = map_rewrite(map, name, strlen(name), NULL);
		else
		{
			/* vp != NULL according to test above */
			result = map_rewrite(map, vp, strlen(vp), av);
		}
		if (vp != NULL)
			sm_free(vp);
	}
	return result;
}

/*
**  LDAPMAP_FINDCONN -- find an LDAP connection to the server
**
**	Cache LDAP connections based on the host, port, bind DN,
**	secret, and PID so we don't have multiple connections open to
**	the same server for different maps.  Need a separate connection
**	per PID since a parent process may close the map before the
**	child is done with it.
**
**	Parameters:
**		lmap -- LDAP map information
**
**	Returns:
**		Symbol table entry for the LDAP connection.
**
*/

static STAB *
ldapmap_findconn(lmap)
	LDAPMAP_STRUCT *lmap;
{
	int len;
	char *nbuf;
	STAB *s;

	len = (lmap->ldap_host == NULL ? strlen("localhost") :
					 strlen(lmap->ldap_host)) + 1 + 8 + 1 +
		(lmap->ldap_binddn == NULL ? 0 : strlen(lmap->ldap_binddn)) +
		1 +
		(lmap->ldap_secret == NULL ? 0 : strlen(lmap->ldap_secret)) +
		8 + 1;
	nbuf = xalloc(len);
	snprintf(nbuf, len, "%s%c%d%c%s%c%s%d",
		 (lmap->ldap_host == NULL ? "localhost" : lmap->ldap_host),
		 CONDELSE,
		 lmap->ldap_port,
		 CONDELSE,
		 (lmap->ldap_binddn == NULL ? "" : lmap->ldap_binddn),
		 CONDELSE,
		 (lmap->ldap_secret == NULL ? "" : lmap->ldap_secret),
		 (int) getpid());
	s = stab(nbuf, ST_LMAP, ST_ENTER);
	sm_free(nbuf);
	return s;
}
/*
**  LDAPMAP_SETOPTS -- set LDAP options
**
**	Parameters:
**		ld -- LDAP session handle
**		lmap -- LDAP map information
**
**	Returns:
**		None.
**
*/

static void
ldapmap_setopts(ld, lmap)
	LDAP *ld;
	LDAPMAP_STRUCT *lmap;
{
# if USE_LDAP_SET_OPTION
	ldap_set_option(ld, LDAP_OPT_DEREF, &lmap->ldap_deref);
	if (bitset(LDAP_OPT_REFERRALS, lmap->ldap_options))
		ldap_set_option(ld, LDAP_OPT_REFERRALS, LDAP_OPT_ON);
	else
		ldap_set_option(ld, LDAP_OPT_REFERRALS, LDAP_OPT_OFF);
	ldap_set_option(ld, LDAP_OPT_SIZELIMIT, &lmap->ldap_sizelimit);
	ldap_set_option(ld, LDAP_OPT_TIMELIMIT, &lmap->ldap_timelimit);
# else /* USE_LDAP_SET_OPTION */
	/* From here on in we can use ldap internal timelimits */
	ld->ld_deref = lmap->ldap_deref;
	ld->ld_options = lmap->ldap_options;
	ld->ld_sizelimit = lmap->ldap_sizelimit;
	ld->ld_timelimit = lmap->ldap_timelimit;
# endif /* USE_LDAP_SET_OPTION */
}
/*
**  LDAPMAP_GETERRNO -- get ldap errno value
**
**	Parameters:
**		ld -- LDAP session handle
**
**	Returns:
**		LDAP errno.
**
*/

static int
ldapmap_geterrno(ld)
	LDAP *ld;
{
	int err = LDAP_SUCCESS;

# if defined(LDAP_VERSION_MAX) && LDAP_VERSION_MAX >= 3
	(void) ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &err);
# else /* defined(LDAP_VERSION_MAX) && LDAP_VERSION_MAX >= 3 */
#  ifdef LDAP_OPT_SIZELIMIT
	err = ldap_get_lderrno(ld, NULL, NULL);
#  else /* LDAP_OPT_SIZELIMIT */
	err = ld->ld_errno;

	/*
	**  Reset value to prevent lingering LDAP_DECODING_ERROR due to
	**  OpenLDAP 1.X's hack (see above)
	*/

	ld->ld_errno = LDAP_SUCCESS;
#  endif /* LDAP_OPT_SIZELIMIT */
# endif /* defined(LDAP_VERSION_MAX) && LDAP_VERSION_MAX >= 3 */
	return err;
}

/*
**  LDAPX_MAP_PARSEARGS -- print warning about use of ldapx map.
*/

bool
ldapx_map_parseargs(map, args)
	MAP *map;
	char *args;
{
	printf("Warning: The \"ldapx\" map class is deprecated and will be removed in a future\n");
	printf("         version.  Use the \"ldap\" map class instead for map \"%s\".\n",
	       map->map_mname);
	return ldapmap_parseargs(map, args);
}

/*
**  LDAPMAP_PARSEARGS -- parse ldap map definition args.
*/

struct lamvalues LDAPAuthMethods[] =
{
	{	"none",		LDAP_AUTH_NONE		},
	{	"simple",	LDAP_AUTH_SIMPLE	},
# ifdef LDAP_AUTH_KRBV4
	{	"krbv4",	LDAP_AUTH_KRBV4		},
# endif /* LDAP_AUTH_KRBV4 */
	{	NULL,		0			}
};

struct ladvalues LDAPAliasDereference[] =
{
	{	"never",	LDAP_DEREF_NEVER	},
	{	"always",	LDAP_DEREF_ALWAYS	},
	{	"search",	LDAP_DEREF_SEARCHING	},
	{	"find",		LDAP_DEREF_FINDING	},
	{	NULL,		0			}
};

struct lssvalues LDAPSearchScope[] =
{
	{	"base",		LDAP_SCOPE_BASE		},
	{	"one",		LDAP_SCOPE_ONELEVEL	},
	{	"sub",		LDAP_SCOPE_SUBTREE	},
	{	NULL,		0			}
};

bool
ldapmap_parseargs(map, args)
	MAP *map;
	char *args;
{
	bool secretread = TRUE;
	int i;
	register char *p = args;
	LDAPMAP_STRUCT *lmap;
	struct lamvalues *lam;
	struct ladvalues *lad;
	struct lssvalues *lss;
	char m_tmp[MAXPATHLEN + LDAPMAP_MAX_PASSWD];

	/* Get ldap struct pointer from map */
	lmap = (LDAPMAP_STRUCT *) map->map_db1;

	/* Check if setting the initial LDAP defaults */
	if (lmap == NULL || lmap != LDAPDefaults)
	{
		/* We need to alloc an LDAPMAP_STRUCT struct */
		lmap = (LDAPMAP_STRUCT *) xalloc(sizeof *lmap);
		if (LDAPDefaults == NULL)
			ldapmap_clear(lmap);
		else
			STRUCTCOPY(*LDAPDefaults, *lmap);
	}

	/* there is no check whether there is really an argument */
	map->map_mflags |= MF_TRY0NULL|MF_TRY1NULL;
	map->map_spacesub = SpaceSub;	/* default value */
	for (;;)
	{
		while (isascii(*p) && isspace(*p))
			p++;
		if (*p != '-')
			break;
		switch (*++p)
		{
		  case 'N':
			map->map_mflags |= MF_INCLNULL;
			map->map_mflags &= ~MF_TRY0NULL;
			break;

		  case 'O':
			map->map_mflags &= ~MF_TRY1NULL;
			break;

		  case 'o':
			map->map_mflags |= MF_OPTIONAL;
			break;

		  case 'f':
			map->map_mflags |= MF_NOFOLDCASE;
			break;

		  case 'm':
			map->map_mflags |= MF_MATCHONLY;
			break;

		  case 'A':
			map->map_mflags |= MF_APPEND;
			break;

		  case 'q':
			map->map_mflags |= MF_KEEPQUOTES;
			break;

		  case 'a':
			map->map_app = ++p;
			break;

		  case 'T':
			map->map_tapp = ++p;
			break;

		  case 't':
			map->map_mflags |= MF_NODEFER;
			break;

		  case 'S':
			map->map_spacesub = *++p;
			break;

		  case 'D':
			map->map_mflags |= MF_DEFER;
			break;

		  case 'z':
			if (*++p != '\\')
				map->map_coldelim = *p;
			else
			{
				switch (*++p)
				{
				  case 'n':
					map->map_coldelim = '\n';
					break;

				  case 't':
					map->map_coldelim = '\t';
					break;

				  default:
					map->map_coldelim = '\\';
				}
			}
			break;

			/* Start of ldapmap specific args */
		  case 'k':		/* search field */
			while (isascii(*++p) && isspace(*p))
				continue;
			lmap->ldap_filter = p;
			break;

		  case 'v':		/* attr to return */
			while (isascii(*++p) && isspace(*p))
				continue;
			lmap->ldap_attr[0] = p;
			lmap->ldap_attr[1] = NULL;
			break;

		  case '1':
			map->map_mflags |= MF_SINGLEMATCH;
			break;

			/* args stolen from ldapsearch.c */
		  case 'R':		/* don't auto chase referrals */
# ifdef LDAP_REFERRALS
			lmap->ldap_options &= ~LDAP_OPT_REFERRALS;
# else /* LDAP_REFERRALS */
			syserr("compile with -DLDAP_REFERRALS for referral support\n");
# endif /* LDAP_REFERRALS */
			break;

		  case 'n':		/* retrieve attribute names only */
			lmap->ldap_attrsonly = LDAPMAP_TRUE;
			break;

		  case 'r':		/* alias dereferencing */
			while (isascii(*++p) && isspace(*p))
				continue;

			if (strncasecmp(p, "LDAP_DEREF_", 11) == 0)
				p += 11;

			for (lad = LDAPAliasDereference;
			     lad != NULL && lad->lad_name != NULL; lad++)
			{
				if (strncasecmp(p, lad->lad_name,
						strlen(lad->lad_name)) == 0)
					break;
			}
			if (lad->lad_name != NULL)
				lmap->ldap_deref = lad->lad_code;
			else
			{
				/* bad config line */
				if (!bitset(MCF_OPTFILE,
					    map->map_class->map_cflags))
				{
					char *ptr;

					if ((ptr = strchr(p, ' ')) != NULL)
						*ptr = '\0';
					syserr("Deref must be [never|always|search|find] (not %s) in map %s",
						p, map->map_mname);
					if (ptr != NULL)
						*ptr = ' ';
					return FALSE;
				}
			}
			break;

		  case 's':		/* search scope */
			while (isascii(*++p) && isspace(*p))
				continue;

			if (strncasecmp(p, "LDAP_SCOPE_", 11) == 0)
				p += 11;

			for (lss = LDAPSearchScope;
			     lss != NULL && lss->lss_name != NULL; lss++)
			{
				if (strncasecmp(p, lss->lss_name,
						strlen(lss->lss_name)) == 0)
					break;
			}
			if (lss->lss_name != NULL)
				lmap->ldap_scope = lss->lss_code;
			else
			{
				/* bad config line */
				if (!bitset(MCF_OPTFILE,
					    map->map_class->map_cflags))
				{
					char *ptr;

					if ((ptr = strchr(p, ' ')) != NULL)
						*ptr = '\0';
					syserr("Scope must be [base|one|sub] (not %s) in map %s",
						p, map->map_mname);
					if (ptr != NULL)
						*ptr = ' ';
					return FALSE;
				}
			}
			break;

		  case 'h':		/* ldap host */
			while (isascii(*++p) && isspace(*p))
				continue;
			lmap->ldap_host = p;
			break;

		  case 'b':		/* search base */
			while (isascii(*++p) && isspace(*p))
				continue;
			lmap->ldap_base = p;
			break;

		  case 'p':		/* ldap port */
			while (isascii(*++p) && isspace(*p))
				continue;
			lmap->ldap_port = atoi(p);
			break;

		  case 'l':		/* time limit */
			while (isascii(*++p) && isspace(*p))
				continue;
			lmap->ldap_timelimit = atoi(p);
			lmap->ldap_timeout.tv_sec = lmap->ldap_timelimit;
			break;

		  case 'Z':
			while (isascii(*++p) && isspace(*p))
				continue;
			lmap->ldap_sizelimit = atoi(p);
			break;

		  case 'd':		/* Dn to bind to server as */
			while (isascii(*++p) && isspace(*p))
				continue;
			lmap->ldap_binddn = p;
			break;

		  case 'M':		/* Method for binding */
			while (isascii(*++p) && isspace(*p))
				continue;

			if (strncasecmp(p, "LDAP_AUTH_", 10) == 0)
				p += 10;

			for (lam = LDAPAuthMethods;
			     lam != NULL && lam->lam_name != NULL; lam++)
			{
				if (strncasecmp(p, lam->lam_name,
						strlen(lam->lam_name)) == 0)
					break;
			}
			if (lam->lam_name != NULL)
				lmap->ldap_method = lam->lam_code;
			else
			{
				/* bad config line */
				if (!bitset(MCF_OPTFILE,
					    map->map_class->map_cflags))
				{
					char *ptr;

					if ((ptr = strchr(p, ' ')) != NULL)
						*ptr = '\0';
					syserr("Method for binding must be [none|simple|krbv4] (not %s) in map %s",
						p, map->map_mname);
					if (ptr != NULL)
						*ptr = ' ';
					return FALSE;
				}
			}

			break;

			/*
			**  This is a string that is dependent on the
			**  method used defined above.
			*/

		  case 'P':		/* Secret password for binding */
			 while (isascii(*++p) && isspace(*p))
				continue;
			lmap->ldap_secret = p;
			secretread = FALSE;
			break;

		  default:
			syserr("Illegal option %c map %s", *p, map->map_mname);
			break;
		}

		/* need to account for quoted strings here */
		while (*p != '\0' && !(isascii(*p) && isspace(*p)))
		{
			if (*p == '"')
			{
				while (*++p != '"' && *p != '\0')
					continue;
				if (*p != '\0')
					p++;
			}
			else
				p++;
		}

		if (*p != '\0')
			*p++ = '\0';
	}

	if (map->map_app != NULL)
		map->map_app = newstr(ldapmap_dequote(map->map_app));
	if (map->map_tapp != NULL)
		map->map_tapp = newstr(ldapmap_dequote(map->map_tapp));

	/*
	**  We need to swallow up all the stuff into a struct
	**  and dump it into map->map_dbptr1
	*/

	if (lmap->ldap_host != NULL &&
	    (LDAPDefaults == NULL ||
	     LDAPDefaults == lmap ||
	     LDAPDefaults->ldap_host != lmap->ldap_host))
		lmap->ldap_host = newstr(ldapmap_dequote(lmap->ldap_host));
	map->map_domain = lmap->ldap_host;

	if (lmap->ldap_binddn != NULL &&
	    (LDAPDefaults == NULL ||
	     LDAPDefaults == lmap ||
	     LDAPDefaults->ldap_binddn != lmap->ldap_binddn))
		lmap->ldap_binddn = newstr(ldapmap_dequote(lmap->ldap_binddn));

	if (lmap->ldap_secret != NULL &&
	    (LDAPDefaults == NULL ||
	     LDAPDefaults == lmap ||
	     LDAPDefaults->ldap_secret != lmap->ldap_secret))
	{
		FILE *sfd;
		long sff = SFF_OPENASROOT|SFF_ROOTOK|SFF_NOWLINK|SFF_NOWWFILES|SFF_NOGWFILES;

		if (DontLockReadFiles)
			sff |= SFF_NOLOCK;

		/* need to use method to map secret to passwd string */
		switch (lmap->ldap_method)
		{
		  case LDAP_AUTH_NONE:
			/* Do nothing */
			break;

		  case LDAP_AUTH_SIMPLE:

			/*
			**  Secret is the name of a file with
			**  the first line as the password.
			*/

			/* Already read in the secret? */
			if (secretread)
				break;

			sfd = safefopen(ldapmap_dequote(lmap->ldap_secret),
					O_RDONLY, 0, sff);
			if (sfd == NULL)
			{
				syserr("LDAP map: cannot open secret %s",
				       ldapmap_dequote(lmap->ldap_secret));
				return FALSE;
			}
			lmap->ldap_secret = sfgets(m_tmp, LDAPMAP_MAX_PASSWD,
						   sfd, TimeOuts.to_fileopen,
						   "ldapmap_parseargs");
			(void) fclose(sfd);
			if (lmap->ldap_secret != NULL &&
			    strlen(m_tmp) > 0)
			{
				/* chomp newline */
				if (m_tmp[strlen(m_tmp) - 1] == '\n')
					m_tmp[strlen(m_tmp) - 1] = '\0';

				lmap->ldap_secret = m_tmp;
			}
			break;

# ifdef LDAP_AUTH_KRBV4
		  case LDAP_AUTH_KRBV4:

			/*
			**  Secret is where the ticket file is
			**  stashed
			*/

			snprintf(m_tmp, MAXPATHLEN + LDAPMAP_MAX_PASSWD,
				 "KRBTKFILE=%s",
				 ldapmap_dequote(lmap->ldap_secret));
			lmap->ldap_secret = m_tmp;
			break;
# endif /* LDAP_AUTH_KRBV4 */

		  default:	       /* Should NEVER get here */
			syserr("LDAP map: Illegal value in lmap method");
			return FALSE;
			break;
		}
	}

	if (lmap->ldap_secret != NULL &&
	    (LDAPDefaults == NULL ||
	     LDAPDefaults == lmap ||
	     LDAPDefaults->ldap_secret != lmap->ldap_secret))
		lmap->ldap_secret = newstr(ldapmap_dequote(lmap->ldap_secret));

	if (lmap->ldap_base != NULL &&
	    (LDAPDefaults == NULL ||
	     LDAPDefaults == lmap ||
	     LDAPDefaults->ldap_base != lmap->ldap_base))
		lmap->ldap_base = newstr(ldapmap_dequote(lmap->ldap_base));

	/*
	**  Save the server from extra work.  If request is for a single
	**  match, tell the server to only return enough records to
	**  determine if there is a single match or not.  This can not
	**  be one since the server would only return one and we wouldn't
	**  know if there were others available.
	*/

	if (bitset(MF_SINGLEMATCH, map->map_mflags))
		lmap->ldap_sizelimit = 2;

	/* If setting defaults, don't process ldap_filter and ldap_attr */
	if (lmap == LDAPDefaults)
		return TRUE;

	if (lmap->ldap_filter != NULL)
		lmap->ldap_filter = newstr(ldapmap_dequote(lmap->ldap_filter));
	else
	{
		if (!bitset(MCF_OPTFILE, map->map_class->map_cflags))
		{
			syserr("No filter given in map %s", map->map_mname);
			return FALSE;
		}
	}

	if (lmap->ldap_attr[0] != NULL)
	{
		i = 0;
		p = ldapmap_dequote(lmap->ldap_attr[0]);
		lmap->ldap_attr[0] = NULL;

		while (p != NULL)
		{
			char *v;

			while (isascii(*p) && isspace(*p))
				p++;
			if (*p == '\0')
				break;
			v = p;
			p = strchr(v, ',');
			if (p != NULL)
				*p++ = '\0';

			if (i >= LDAPMAP_MAX_ATTR)
			{
				syserr("Too many return attributes in %s (max %d)",
				       map->map_mname, LDAPMAP_MAX_ATTR);
				return FALSE;
			}
			if (*v != '\0')
				lmap->ldap_attr[i++] = newstr(v);
		}
		lmap->ldap_attr[i] = NULL;
	}

	map->map_db1 = (ARBPTR_T) lmap;
	return TRUE;
}

/*
**  LDAPMAP_CLEAR -- set default values for LDAPMAP_STRUCT
**
**	Parameters:
**		lmap -- pointer to LDAPMAP_STRUCT to clear
**
**	Returns:
**		None.
**
*/

static void
ldapmap_clear(lmap)
	LDAPMAP_STRUCT *lmap;
{
	lmap->ldap_host = NULL;
	lmap->ldap_port = LDAP_PORT;
	lmap->ldap_deref = LDAP_DEREF_NEVER;
	lmap->ldap_timelimit = LDAP_NO_LIMIT;
	lmap->ldap_sizelimit = LDAP_NO_LIMIT;
# ifdef LDAP_REFERRALS
	lmap->ldap_options = LDAP_OPT_REFERRALS;
# else /* LDAP_REFERRALS */
	lmap->ldap_options = 0;
# endif /* LDAP_REFERRALS */
	lmap->ldap_binddn = NULL;
	lmap->ldap_secret = NULL;
	lmap->ldap_method = LDAP_AUTH_SIMPLE;
	lmap->ldap_base = NULL;
	lmap->ldap_scope = LDAP_SCOPE_SUBTREE;
	lmap->ldap_attrsonly = LDAPMAP_FALSE;
	lmap->ldap_timeout.tv_sec = 0;
	lmap->ldap_timeout.tv_usec = 0;
	lmap->ldap_ld = NULL;
	lmap->ldap_filter = NULL;
	lmap->ldap_attr[0] = NULL;
	lmap->ldap_res = NULL;
	lmap->ldap_next = NULL;
}
/*
**  LDAPMAP_SET_DEFAULTS -- Read default map spec from LDAPDefaults in .cf
**
**	Parameters:
**		spec -- map argument string from LDAPDefaults option
**
**	Returns:
**		None.
**
*/

void
ldapmap_set_defaults(spec)
	char *spec;
{
	STAB *class;
	MAP map;

	/* Allocate and set the default values */
	if (LDAPDefaults == NULL)
		LDAPDefaults = (LDAPMAP_STRUCT *) xalloc(sizeof *LDAPDefaults);
	ldapmap_clear(LDAPDefaults);

	memset(&map, '\0', sizeof map);

	/* look up the class */
	class = stab("ldap", ST_MAPCLASS, ST_FIND);
	if (class == NULL)
	{
		syserr("readcf: LDAPDefaultSpec: class ldap not available");
		return;
	}
	map.map_class = &class->s_mapclass;
	map.map_db1 = (ARBPTR_T) LDAPDefaults;
	map.map_mname = "O LDAPDefaultSpec";

	(void) ldapmap_parseargs(&map, spec);

	/* These should never be set in LDAPDefaults */
	if (map.map_mflags != (MF_TRY0NULL|MF_TRY1NULL) ||
	    map.map_spacesub != SpaceSub ||
	    map.map_app != NULL ||
	    map.map_tapp != NULL)
	{
		syserr("readcf: option LDAPDefaultSpec: Do not set non-LDAP specific flags");
		if (map.map_app != NULL)
		{
			sm_free(map.map_app);
			map.map_app = NULL;
		}
		if (map.map_tapp != NULL)
		{
			sm_free(map.map_tapp);
			map.map_tapp = NULL;
		}
	}

	if (LDAPDefaults->ldap_filter != NULL)
	{
		syserr("readcf: option LDAPDefaultSpec: Do not set the LDAP search filter");
		/* don't free, it isn't malloc'ed in parseargs */
		LDAPDefaults->ldap_filter = NULL;
	}

	if (LDAPDefaults->ldap_attr[0] != NULL)
	{
		syserr("readcf: option LDAPDefaultSpec: Do not set the requested LDAP attributes");
		/* don't free, they aren't malloc'ed in parseargs */
		LDAPDefaults->ldap_attr[0] = NULL;
	}
}
#endif /* LDAPMAP */
/*
**  PH map
*/

#ifdef PH_MAP

/*
**  Support for the CCSO Nameserver (ph/qi).
**  This code is intended to replace the so-called "ph mailer".
**  Contributed by Mark D. Roth <roth@uiuc.edu>.  Contact him for support.
*/

# include <qiapi.h>
# include <qicode.h>

/*
**  PH_MAP_PARSEARGS -- parse ph map definition args.
*/

bool
ph_map_parseargs(map, args)
	MAP *map;
	char *args;
{
	int i;
	register int done;
	PH_MAP_STRUCT *pmap = NULL;
	register char *p = args;

	pmap = (PH_MAP_STRUCT *) xalloc(sizeof *pmap);

	/* defaults */
	pmap->ph_servers = NULL;
	pmap->ph_field_list = NULL;
	pmap->ph_to_server = NULL;
	pmap->ph_from_server = NULL;
	pmap->ph_sockfd = -1;
	pmap->ph_timeout = 0;

	map->map_mflags |= MF_TRY0NULL|MF_TRY1NULL;
	for (;;)
	{
		while (isascii(*p) && isspace(*p))
			p++;
		if (*p != '-')
			break;
		switch (*++p)
		{
		  case 'N':
			map->map_mflags |= MF_INCLNULL;
			map->map_mflags &= ~MF_TRY0NULL;
			break;

		  case 'O':
			map->map_mflags &= ~MF_TRY1NULL;
			break;

		  case 'o':
			map->map_mflags |= MF_OPTIONAL;
			break;

		  case 'f':
			map->map_mflags |= MF_NOFOLDCASE;
			break;

		  case 'm':
			map->map_mflags |= MF_MATCHONLY;
			break;

		  case 'A':
			map->map_mflags |= MF_APPEND;
			break;

		  case 'q':
			map->map_mflags |= MF_KEEPQUOTES;
			break;

		  case 't':
			map->map_mflags |= MF_NODEFER;
			break;

		  case 'a':
			map->map_app = ++p;
			break;

		  case 'T':
			map->map_tapp = ++p;
			break;

#if _FFR_PHMAP_TIMEOUT
		  case 'l':
			while (isascii(*++p) && isspace(*p))
				continue;
			pmap->ph_timeout = atoi(p);
			break;
#endif /* _FFR_PHMAP_TIMEOUT */

		  case 'S':
			map->map_spacesub = *++p;
			break;

		  case 'D':
			map->map_mflags |= MF_DEFER;
			break;

		  case 'h':		/* PH server list */
			while (isascii(*++p) && isspace(*p))
				continue;
			pmap->ph_servers = p;
			break;

		  case 'v':		/* fields to search for */
			while (isascii(*++p) && isspace(*p))
				continue;
			pmap->ph_field_list = p;
			break;

		  default:
			syserr("ph_map_parseargs: unknown option -%c\n", *p);
		}

		/* try to account for quoted strings */
		done = isascii(*p) && isspace(*p);
		while (*p != '\0' && !done)
		{
			if (*p == '"')
			{
				while (*++p != '"' && *p != '\0')
					continue;
				if (*p != '\0')
					p++;
			}
			else
				p++;
			done = isascii(*p) && isspace(*p);
		}

		if (*p != '\0')
			*p++ = '\0';
	}

	if (map->map_app != NULL)
		map->map_app = newstr(ph_map_dequote(map->map_app));
	if (map->map_tapp != NULL)
		map->map_tapp = newstr(ph_map_dequote(map->map_tapp));

	if (pmap->ph_field_list != NULL)
		pmap->ph_field_list = newstr(ph_map_dequote(pmap->ph_field_list));
	else
		pmap->ph_field_list = DEFAULT_PH_MAP_FIELDS;

	if (pmap->ph_servers != NULL)
		pmap->ph_servers = newstr(ph_map_dequote(pmap->ph_servers));
	else
	{
		syserr("ph_map_parseargs: -h flag is required");
		return FALSE;
	}

	map->map_db1 = (ARBPTR_T) pmap;
	return TRUE;
}

#if _FFR_PHMAP_TIMEOUT
/*
**  PH_MAP_CLOSE -- close the connection to the ph server
*/

static void
ph_map_safeclose(map)
	MAP *map;
{
	int save_errno = errno;
	PH_MAP_STRUCT *pmap;

	pmap = (PH_MAP_STRUCT *)map->map_db1;

	if (pmap->ph_sockfd != -1)
	{
		(void) close(pmap->ph_sockfd);
		pmap->ph_sockfd = -1;
	}
	if (pmap->ph_from_server != NULL)
	{
		(void) fclose(pmap->ph_from_server);
		pmap->ph_from_server = NULL;
	}
	if (pmap->ph_to_server != NULL)
	{
		(void) fclose(pmap->ph_to_server);
		pmap->ph_to_server = NULL;
	}
	map->map_mflags &= ~(MF_OPEN|MF_WRITABLE);
	errno = save_errno;
}

void
ph_map_close(map)
	MAP *map;
{
	PH_MAP_STRUCT *pmap;

	pmap = (PH_MAP_STRUCT *)map->map_db1;
	(void) fprintf(pmap->ph_to_server, "quit\n");
	(void) fflush(pmap->ph_to_server);
	ph_map_safeclose(map);
}

static jmp_buf  PHTimeout;

/* ARGSUSED */
static void
ph_timeout(sig)
	int sig;
{
	/*
	**  NOTE: THIS CAN BE CALLED FROM A SIGNAL HANDLER.  DO NOT ADD
	**	ANYTHING TO THIS ROUTINE UNLESS YOU KNOW WHAT YOU ARE
	**	DOING.
	*/

	errno = ETIMEDOUT;
	longjmp(PHTimeout, 1);
}
#else /* _FFR_PHMAP_TIMEOUT */
/*
**  PH_MAP_CLOSE -- close the connection to the ph server
*/

void
ph_map_close(map)
	MAP *map;
{
	PH_MAP_STRUCT *pmap;

	pmap = (PH_MAP_STRUCT *)map->map_db1;
	CloseQi(pmap->ph_to_server, pmap->ph_from_server);
	pmap->ph_to_server = NULL;
	pmap->ph_from_server = NULL;
}
#endif /* _FFR_PHMAP_TIMEOUT */

/*
**  PH_MAP_OPEN -- sub for opening PH map
*/
bool
ph_map_open(map, mode)
	MAP *map;
	int mode;
{
#if !_FFR_PHMAP_TIMEOUT
	int save_errno = 0;
#endif /* !_FFR_PHMAP_TIMEOUT */
	int j;
	char *hostlist, *tmp;
	QIR *server_data = NULL;
	PH_MAP_STRUCT *pmap;
#if _FFR_PHMAP_TIMEOUT
	register EVENT *ev = NULL;
#endif /* _FFR_PHMAP_TIMEOUT */

	if (tTd(38, 2))
		dprintf("ph_map_open(%s)\n", map->map_mname);

	mode &= O_ACCMODE;
	if (mode != O_RDONLY)
	{
		/* issue a pseudo-error message */
# ifdef ENOSYS
		errno = ENOSYS;
# else /* ENOSYS */
#  ifdef EFTYPE
		errno = EFTYPE;
#  else /* EFTYPE */
		errno = ENXIO;
#  endif /* EFTYPE */
# endif /* ENOSYS */
		return FALSE;
	}

	if (CurEnv != NULL && CurEnv->e_sendmode == SM_DEFER &&
	    bitset(MF_DEFER, map->map_mflags))
	{
		if (tTd(9, 1))
			dprintf("ph_map_open(%s) => DEFERRED\n",
				map->map_mname);

		/*
		** Unset MF_DEFER here so that map_lookup() returns
		** a temporary failure using the bogus map and
		** map->map_tapp instead of the default permanent error.
		*/

		map->map_mflags &= ~MF_DEFER;
		return FALSE;
	}

	pmap = (PH_MAP_STRUCT *)map->map_db1;

	hostlist = newstr(pmap->ph_servers);
	tmp = strtok(hostlist, " ");
	do
	{
#if _FFR_PHMAP_TIMEOUT
		if (pmap->ph_timeout != 0)
		{
			if (setjmp(PHTimeout) != 0)
			{
				ev = NULL;
				if (LogLevel > 1)
					sm_syslog(LOG_NOTICE, CurEnv->e_id,
						  "timeout connecting to PH server %.100s",
						  tmp);
# ifdef ETIMEDOUT
				errno = ETIMEDOUT;
# else /* ETIMEDOUT */
				errno = EAGAIN;
# endif /* ETIMEDOUT */
				goto ph_map_open_abort;
			}
			ev = setevent(pmap->ph_timeout, ph_timeout, 0);
		}
		if (!OpenQiSock(tmp, &(pmap->ph_sockfd)) &&
		    !Sock2FILEs(pmap->ph_sockfd, &(pmap->ph_to_server),
				&(pmap->ph_from_server)) &&
		    fprintf(pmap->ph_to_server, "id sendmail+phmap\n") >= 0 &&
		    fflush(pmap->ph_to_server) == 0 &&
		    (server_data = ReadQi(pmap->ph_from_server, &j)) != NULL &&
		    server_data->code == 200)
		{
			if (ev != NULL)
				clrevent(ev);
			FreeQIR(server_data);
#else /* _FFR_PHMAP_TIMEOUT */
		if (OpenQi(tmp, &(pmap->ph_to_server),
			   &(pmap->ph_from_server)) >= 0)
		{
			if (fprintf(pmap->ph_to_server,
				    "id sendmail+phmap\n") < 0 ||
			    fflush(pmap->ph_to_server) != 0 ||
			    (server_data = ReadQi(pmap->ph_from_server,
						  &j)) == NULL ||
			    server_data->code != 200)
			{
				save_errno = errno;
				CloseQi(pmap->ph_to_server,
					pmap->ph_from_server);
				continue;
			}
			if (server_data != NULL)
				FreeQIR(server_data);
#endif /* _FFR_PHMAP_TIMEOUT */
			sm_free(hostlist);
			return TRUE;
		}
#if _FFR_PHMAP_TIMEOUT
  ph_map_open_abort:
		if (ev != NULL)
			clrevent(ev);
		ph_map_safeclose(map);
		if (server_data != NULL)
		{
			FreeQIR(server_data);
			server_data = NULL;
		}
#else /* _FFR_PHMAP_TIMEOUT */
		save_errno = errno;
#endif /* _FFR_PHMAP_TIMEOUT */
	} while (tmp = strtok(NULL, " "));

#if !_FFR_PHMAP_TIMEOUT
	errno = save_errno;
#endif /* !_FFR_PHMAP_TIMEOUT */
	if (bitset(MF_NODEFER, map->map_mflags))
	{
		if (errno == 0)
			errno = EAGAIN;
		syserr("ph_map_open: %s: cannot connect to PH server",
		       map->map_mname);
	}
	else if (!bitset(MF_OPTIONAL, map->map_mflags) && LogLevel > 1)
		sm_syslog(LOG_NOTICE, CurEnv->e_id,
			  "ph_map_open: %s: cannot connect to PH server",
			  map->map_mname);
	sm_free(hostlist);
	return FALSE;
}

/*
**  PH_MAP_LOOKUP -- look up key from ph server
*/

#if _FFR_PHMAP_TIMEOUT
# define MAX_PH_FIELDS	20
#endif /* _FFR_PHMAP_TIMEOUT */

char *
ph_map_lookup(map, key, args, pstat)
	MAP *map;
	char *key;
	char **args;
	int *pstat;
{
	int j;
	size_t sz;
	char *tmp, *tmp2;
	char *message = NULL, *field = NULL, *fmtkey;
	QIR *server_data = NULL;
	QIR *qirp;
	char keybuf[MAXKEY + 1], fieldbuf[101];
#if _FFR_PHMAP_TIMEOUT
	QIR *hold_data[MAX_PH_FIELDS];
	int hold_data_idx = 0;
	register EVENT *ev = NULL;
#endif /* _FFR_PHMAP_TIMEOUT */
	PH_MAP_STRUCT *pmap;

	pmap = (PH_MAP_STRUCT *)map->map_db1;

	*pstat = EX_OK;

#if _FFR_PHMAP_TIMEOUT
	if (pmap->ph_timeout != 0)
	{
		if (setjmp(PHTimeout) != 0)
		{
			ev = NULL;
			if (LogLevel > 1)
				sm_syslog(LOG_NOTICE, CurEnv->e_id,
					  "timeout during PH lookup of %.100s",
					  key);
# ifdef ETIMEDOUT
			errno = ETIMEDOUT;
# else /* ETIMEDOUT */
			errno = 0;
# endif /* ETIMEDOUT */
			*pstat = EX_TEMPFAIL;
			goto ph_map_lookup_abort;
		}
		ev = setevent(pmap->ph_timeout, ph_timeout, 0);
	}

#endif /* _FFR_PHMAP_TIMEOUT */
	/* check all relevant fields */
	tmp = pmap->ph_field_list;
	do
	{
#if _FFR_PHMAP_TIMEOUT
		server_data = NULL;
#endif /* _FFR_PHMAP_TIMEOUT */
		while (isascii(*tmp) && isspace(*tmp))
			tmp++;
		if (*tmp == '\0')
			break;
		sz = strcspn(tmp, " ") + 1;
		if (sz > sizeof fieldbuf)
			sz = sizeof fieldbuf;
		(void) strlcpy(fieldbuf, tmp, sz);
		field = fieldbuf;
		tmp += sz;

		(void) strlcpy(keybuf, key, sizeof keybuf);
		fmtkey = keybuf;
		if (strcmp(field, "alias") == 0)
		{
			/*
			**  for alias lookups, replace any punctuation
			**  characters with '-'
			*/

			for (tmp2 = fmtkey; *tmp2 !=  '\0'; tmp2++)
			{
				if (isascii(*tmp2) && ispunct(*tmp2))
					*tmp2 = '-';
			}
			tmp2 = field;
		}
		else if (strcmp(field,"spacedname") == 0)
		{
			/*
			**  for "spaced" name lookups, replace any
			**  punctuation characters with a space
			*/

			for (tmp2 = fmtkey; *tmp2 != '\0'; tmp2++)
			{
				if (isascii(*tmp2) && ispunct(*tmp2) &&
				    *tmp2 != '*')
					*tmp2 = ' ';
			}
			tmp2 = &(field[6]);
		}
		else
			tmp2 = field;

		if (LogLevel > 9)
			sm_syslog(LOG_NOTICE, CurEnv->e_id,
				  "ph_map_lookup: query %s=\"%s\" return email",
				  tmp2, fmtkey);
		if (tTd(38, 20))
			dprintf("ph_map_lookup: query %s=\"%s\" return email\n",
				tmp2, fmtkey);

		j = 0;

		if (fprintf(pmap->ph_to_server, "query %s=%s return email\n",
			    tmp2, fmtkey) < 0)
			message = "qi query command failed";
		else if (fflush(pmap->ph_to_server) != 0)
			message = "qi fflush failed";
		else if ((server_data = ReadQi(pmap->ph_from_server,
					       &j)) == NULL)
			message = "ReadQi() returned NULL";

#if _FFR_PHMAP_TIMEOUT
		if ((hold_data[hold_data_idx] = server_data) != NULL)
		{
			/* save pointer for later free() */
			hold_data_idx++;
		}
#endif /* _FFR_PHMAP_TIMEOUT */

		if (server_data == NULL ||
		    (server_data->code >= 400 &&
		     server_data->code < 500))
		{
			/* temporary failure */
			*pstat = EX_TEMPFAIL;
#if _FFR_PHMAP_TIMEOUT
			break;
#else /* _FFR_PHMAP_TIMEOUT */
			if (server_data != NULL)
			{
				FreeQIR(server_data);
				server_data = NULL;
			}
			return NULL;
#endif /* _FFR_PHMAP_TIMEOUT */
		}

		/*
		**  if we found a single match, break out.
		**  otherwise, try the next field.
		*/

		if (j == 1)
			break;

		/*
		**  check for a single response which is an error:
		**  ReadQi() doesn't set j on error responses,
		**  but we should stop here instead of moving on if
		**  it happens (e.g., alias found but email field empty)
		*/

		for (qirp = server_data;
		     qirp != NULL && qirp->code < 0;
		     qirp++)
		{
			if (tTd(38, 20))
				dprintf("ph_map_lookup: QIR: %d:%d:%d:%s\n",
					qirp->code, qirp->subcode, qirp->field,
					(qirp->message ? qirp->message
					 : "[NULL]"));
			if (qirp->code <= -500)
			{
				j = 0;
				goto ph_map_lookup_abort;
			}
		}

#if _FFR_PHMAP_TIMEOUT
	} while (*tmp != '\0' && hold_data_idx < MAX_PH_FIELDS);
#else /* _FFR_PHMAP_TIMEOUT */
	} while (*tmp != '\0');
#endif /* _FFR_PHMAP_TIMEOUT */

  ph_map_lookup_abort:
#if _FFR_PHMAP_TIMEOUT
	if (ev != NULL)
		clrevent(ev);

	/*
	**  Return EX_TEMPFAIL if the timer popped
	**  or we got a temporary PH error
	*/

	if (*pstat == EX_TEMPFAIL)
		ph_map_safeclose(map);

	/* if we didn't find a single match, bail out */
	if (*pstat == EX_OK && j != 1)
		*pstat = EX_UNAVAILABLE;

	if (*pstat == EX_OK)
	{
		/*
		** skip leading whitespace and chop at first address
		*/

		for (tmp = server_data->message;
		     isascii(*tmp) && isspace(*tmp);
		     tmp++)
			continue;

		for (tmp2 = tmp; *tmp2 != '\0'; tmp2++)
		{
			if (isascii(*tmp2) && isspace(*tmp2))
			{
				*tmp2 = '\0';
				break;
			}
		}

		if (tTd(38,20))
			dprintf("ph_map_lookup: %s => %s\n", key, tmp);

		if (bitset(MF_MATCHONLY, map->map_mflags))
			message = map_rewrite(map, key, strlen(key), NULL);
		else
			message = map_rewrite(map, tmp, strlen(tmp), args);
	}

	/*
	**  Deferred free() of returned server_data values
	**  the deferral is to avoid the risk of a free() being
	**  interrupted by the event timer.  By now the timeout event
	**  has been cleared and none of the data is still in use.
	*/

	while (--hold_data_idx >= 0)
	{
		if (hold_data[hold_data_idx] != NULL)
			FreeQIR(hold_data[hold_data_idx]);
	}

	if (*pstat == EX_OK)
		return message;

	return NULL;
#else /* _FFR_PHMAP_TIMEOUT */
	/* if we didn't find a single match, bail out */
	if (j != 1)
	{
		*pstat = EX_UNAVAILABLE;
		if (server_data != NULL)
		{
			FreeQIR(server_data);
			server_data = NULL;
		}
		return NULL;
	}

	/*
	** skip leading whitespace and chop at first address
	*/

	for (tmp = server_data->message;
	     isascii(*tmp) && isspace(*tmp);
	     tmp++)
		continue;

	for (tmp2 = tmp; *tmp2 != '\0'; tmp2++)
	{
		if (isascii(*tmp2) && isspace(*tmp2))
		{
			*tmp2 = '\0';
			break;
		}
	}

	if (tTd(38,20))
		dprintf("ph_map_lookup: %s => %s\n", key, tmp);

	if (bitset(MF_MATCHONLY, map->map_mflags))
		message = map_rewrite(map, key, strlen(key), NULL);
	else
		message = map_rewrite(map, tmp, strlen(tmp), args);
	if (server_data != NULL)
	{
		FreeQIR(server_data);
		server_data = NULL;
	}
	return message;
#endif /* _FFR_PHMAP_TIMEOUT */
}
#endif /* PH_MAP */
/*
**  syslog map
*/

#define map_prio	map_lockfd	/* overload field */

/*
**  SYSLOG_MAP_PARSEARGS -- check for priority level to syslog messages.
*/

bool
syslog_map_parseargs(map, args)
	MAP *map;
	char *args;
{
	char *p = args;
	char *priority = NULL;

	/* there is no check whether there is really an argument */
	while (*p != '\0')
	{
		while (isascii(*p) && isspace(*p))
			p++;
		if (*p != '-')
			break;
		++p;
		if (*p == 'D')
		{
			map->map_mflags |= MF_DEFER;
			++p;
		}
		else if (*p == 'S')
		{
			map->map_spacesub = *++p;
			if (*p != '\0')
				p++;
		}
		else if (*p == 'L')
		{
			while (*++p != '\0' && isascii(*p) && isspace(*p))
				continue;
			if (*p == '\0')
				break;
			priority = p;
			while (*p != '\0' && !(isascii(*p) && isspace(*p)))
				p++;
			if (*p != '\0')
				*p++ = '\0';
		}
		else
		{
			syserr("Illegal option %c map syslog", *p);
			++p;
		}
	}

	if (priority == NULL)
		map->map_prio = LOG_INFO;
	else
	{
		if (strncasecmp("LOG_", priority, 4) == 0)
			priority += 4;

#ifdef LOG_EMERG
		if (strcasecmp("EMERG", priority) == 0)
			map->map_prio = LOG_EMERG;
		else
#endif /* LOG_EMERG */
#ifdef LOG_ALERT
		if (strcasecmp("ALERT", priority) == 0)
			map->map_prio = LOG_ALERT;
		else
#endif /* LOG_ALERT */
#ifdef LOG_CRIT
		if (strcasecmp("CRIT", priority) == 0)
			map->map_prio = LOG_CRIT;
		else
#endif /* LOG_CRIT */
#ifdef LOG_ERR
		if (strcasecmp("ERR", priority) == 0)
			map->map_prio = LOG_ERR;
		else
#endif /* LOG_ERR */
#ifdef LOG_WARNING
		if (strcasecmp("WARNING", priority) == 0)
			map->map_prio = LOG_WARNING;
		else
#endif /* LOG_WARNING */
#ifdef LOG_NOTICE
		if (strcasecmp("NOTICE", priority) == 0)
			map->map_prio = LOG_NOTICE;
		else
#endif /* LOG_NOTICE */
#ifdef LOG_INFO
		if (strcasecmp("INFO", priority) == 0)
			map->map_prio = LOG_INFO;
		else
#endif /* LOG_INFO */
#ifdef LOG_DEBUG
		if (strcasecmp("DEBUG", priority) == 0)
			map->map_prio = LOG_DEBUG;
		else
#endif /* LOG_DEBUG */
		{
			syserr("syslog_map_parseargs: Unknown priority %s\n",
			       priority);
			return FALSE;
		}
	}
	return TRUE;
}

/*
**  SYSLOG_MAP_LOOKUP -- rewrite and syslog message.  Always return empty string
*/

char *
syslog_map_lookup(map, string, args, statp)
	MAP *map;
	char *string;
	char **args;
	int *statp;
{
	char *ptr = map_rewrite(map, string, strlen(string), args);

	if (ptr != NULL)
	{
		if (tTd(38, 20))
			dprintf("syslog_map_lookup(%s (priority %d): %s\n",
				map->map_mname, map->map_prio, ptr);

		sm_syslog(map->map_prio, CurEnv->e_id, "%s", ptr);
	}

	*statp = EX_OK;
	return "";
}

/*
**  HESIOD Modules
*/

#ifdef HESIOD

bool
hes_map_open(map, mode)
	MAP *map;
	int mode;
{
	if (tTd(38, 2))
		dprintf("hes_map_open(%s, %s, %d)\n",
			map->map_mname, map->map_file, mode);

	if (mode != O_RDONLY)
	{
		/* issue a pseudo-error message */
# ifdef ENOSYS
		errno = ENOSYS;
# else /* ENOSYS */
#  ifdef EFTYPE
		errno = EFTYPE;
#  else /* EFTYPE */
		errno = ENXIO;
#  endif /* EFTYPE */
# endif /* ENOSYS */
		return FALSE;
	}

# ifdef HESIOD_INIT
	if (HesiodContext != NULL || hesiod_init(&HesiodContext) == 0)
		return TRUE;

	if (!bitset(MF_OPTIONAL, map->map_mflags))
		syserr("421 4.0.0 cannot initialize Hesiod map (%s)",
			errstring(errno));
	return FALSE;
# else /* HESIOD_INIT */
	if (hes_error() == HES_ER_UNINIT)
		hes_init();
	switch (hes_error())
	{
	  case HES_ER_OK:
	  case HES_ER_NOTFOUND:
		return TRUE;
	}

	if (!bitset(MF_OPTIONAL, map->map_mflags))
		syserr("421 4.0.0 cannot initialize Hesiod map (%d)", hes_error());

	return FALSE;
# endif /* HESIOD_INIT */
}

char *
hes_map_lookup(map, name, av, statp)
	MAP *map;
	char *name;
	char **av;
	int *statp;
{
	char **hp;

	if (tTd(38, 20))
		dprintf("hes_map_lookup(%s, %s)\n", map->map_file, name);

	if (name[0] == '\\')
	{
		char *np;
		int nl;
		int save_errno;
		char nbuf[MAXNAME];

		nl = strlen(name);
		if (nl < sizeof nbuf - 1)
			np = nbuf;
		else
			np = xalloc(strlen(name) + 2);
		np[0] = '\\';
		(void) strlcpy(&np[1], name, (sizeof nbuf) - 1);
# ifdef HESIOD_INIT
		hp = hesiod_resolve(HesiodContext, np, map->map_file);
# else /* HESIOD_INIT */
		hp = hes_resolve(np, map->map_file);
# endif /* HESIOD_INIT */
		save_errno = errno;
		if (np != nbuf)
			sm_free(np);
		errno = save_errno;
	}
	else
	{
# ifdef HESIOD_INIT
		hp = hesiod_resolve(HesiodContext, name, map->map_file);
# else /* HESIOD_INIT */
		hp = hes_resolve(name, map->map_file);
# endif /* HESIOD_INIT */
	}
# ifdef HESIOD_INIT
	if (hp == NULL || *hp == NULL)
	{
		switch (errno)
		{
		  case ENOENT:
			  *statp = EX_NOTFOUND;
			  break;
		  case ECONNREFUSED:
		  case EMSGSIZE:
			  *statp = EX_TEMPFAIL;
			  break;
		  case ENOMEM:
		  default:
			  *statp = EX_UNAVAILABLE;
			  break;
		}
		hesiod_free_list(HesiodContext, hp);
		return NULL;
	}
# else /* HESIOD_INIT */
	if (hp == NULL || hp[0] == NULL)
	{
		switch (hes_error())
		{
		  case HES_ER_OK:
			*statp = EX_OK;
			break;

		  case HES_ER_NOTFOUND:
			*statp = EX_NOTFOUND;
			break;

		  case HES_ER_CONFIG:
			*statp = EX_UNAVAILABLE;
			break;

		  case HES_ER_NET:
			*statp = EX_TEMPFAIL;
			break;
		}
		return NULL;
	}
# endif /* HESIOD_INIT */

	if (bitset(MF_MATCHONLY, map->map_mflags))
		return map_rewrite(map, name, strlen(name), NULL);
	else
		return map_rewrite(map, hp[0], strlen(hp[0]), av);
}

#endif /* HESIOD */
/*
**  NeXT NETINFO Modules
*/

#if NETINFO

# define NETINFO_DEFAULT_DIR		"/aliases"
# define NETINFO_DEFAULT_PROPERTY	"members"

/*
**  NI_MAP_OPEN -- open NetInfo Aliases
*/

bool
ni_map_open(map, mode)
	MAP *map;
	int mode;
{
	if (tTd(38, 2))
		dprintf("ni_map_open(%s, %s, %d)\n",
			map->map_mname, map->map_file, mode);
	mode &= O_ACCMODE;

	if (*map->map_file == '\0')
		map->map_file = NETINFO_DEFAULT_DIR;

	if (map->map_valcolnm == NULL)
		map->map_valcolnm = NETINFO_DEFAULT_PROPERTY;

	if (map->map_coldelim == '\0' && bitset(MF_ALIAS, map->map_mflags))
		map->map_coldelim = ',';

	return TRUE;
}


/*
**  NI_MAP_LOOKUP -- look up a datum in NetInfo
*/

char *
ni_map_lookup(map, name, av, statp)
	MAP *map;
	char *name;
	char **av;
	int *statp;
{
	char *res;
	char *propval;

	if (tTd(38, 20))
		dprintf("ni_map_lookup(%s, %s)\n", map->map_mname, name);

	propval = ni_propval(map->map_file, map->map_keycolnm, name,
			     map->map_valcolnm, map->map_coldelim);

	if (propval == NULL)
		return NULL;

	if (bitset(MF_MATCHONLY, map->map_mflags))
		res = map_rewrite(map, name, strlen(name), NULL);
	else
		res = map_rewrite(map, propval, strlen(propval), av);
	sm_free(propval);
	return res;
}


static bool
ni_getcanonname(name, hbsize, statp)
	char *name;
	int hbsize;
	int *statp;
{
	char *vptr;
	char *ptr;
	char nbuf[MAXNAME + 1];

	if (tTd(38, 20))
		dprintf("ni_getcanonname(%s)\n", name);

	if (strlcpy(nbuf, name, sizeof nbuf) >= sizeof nbuf)
	{
		*statp = EX_UNAVAILABLE;
		return FALSE;
	}
	(void) shorten_hostname(nbuf);

	/* we only accept single token search key */
	if (strchr(nbuf, '.'))
	{
		*statp = EX_NOHOST;
		return FALSE;
	}

	/* Do the search */
	vptr = ni_propval("/machines", NULL, nbuf, "name", '\n');

	if (vptr == NULL)
	{
		*statp = EX_NOHOST;
		return FALSE;
	}

	/* Only want the first machine name */
	if ((ptr = strchr(vptr, '\n')) != NULL)
		*ptr = '\0';

	if (hbsize >= strlen(vptr))
	{
		(void) strlcpy(name, vptr, hbsize);
		sm_free(vptr);
		*statp = EX_OK;
		return TRUE;
	}
	*statp = EX_UNAVAILABLE;
	sm_free(vptr);
	return FALSE;
}


/*
**  NI_PROPVAL -- NetInfo property value lookup routine
**
**	Parameters:
**		keydir -- the NetInfo directory name in which to search
**			for the key.
**		keyprop -- the name of the property in which to find the
**			property we are interested.  Defaults to "name".
**		keyval -- the value for which we are really searching.
**		valprop -- the property name for the value in which we
**			are interested.
**		sepchar -- if non-nil, this can be multiple-valued, and
**			we should return a string separated by this
**			character.
**
**	Returns:
**		NULL -- if:
**			1. the directory is not found
**			2. the property name is not found
**			3. the property contains multiple values
**			4. some error occurred
**		else -- the value of the lookup.
**
**	Example:
**		To search for an alias value, use:
**		  ni_propval("/aliases", "name", aliasname, "members", ',')
**
**	Notes:
**		Caller should free the return value of ni_proval
*/

# include <netinfo/ni.h>

# define LOCAL_NETINFO_DOMAIN	"."
# define PARENT_NETINFO_DOMAIN	".."
# define MAX_NI_LEVELS		256

char *
ni_propval(keydir, keyprop, keyval, valprop, sepchar)
	char *keydir;
	char *keyprop;
	char *keyval;
	char *valprop;
	int sepchar;
{
	char *propval = NULL;
	int i;
	int j, alen, l;
	void *ni = NULL;
	void *lastni = NULL;
	ni_status nis;
	ni_id nid;
	ni_namelist ninl;
	register char *p;
	char keybuf[1024];

	/*
	**  Create the full key from the two parts.
	**
	**	Note that directory can end with, e.g., "name=" to specify
	**	an alternate search property.
	*/

	i = strlen(keydir) + strlen(keyval) + 2;
	if (keyprop != NULL)
		i += strlen(keyprop) + 1;
	if (i >= sizeof keybuf)
		return NULL;
	(void) strlcpy(keybuf, keydir, sizeof keybuf);
	(void) strlcat(keybuf, "/", sizeof keybuf);
	if (keyprop != NULL)
	{
		(void) strlcat(keybuf, keyprop, sizeof keybuf);
		(void) strlcat(keybuf, "=", sizeof keybuf);
	}
	(void) strlcat(keybuf, keyval, sizeof keybuf);

	if (tTd(38, 21))
		dprintf("ni_propval(%s, %s, %s, %s, %d) keybuf='%s'\n",
			keydir, keyprop, keyval, valprop, sepchar, keybuf);
	/*
	**  If the passed directory and property name are found
	**  in one of netinfo domains we need to search (starting
	**  from the local domain moving all the way back to the
	**  root domain) set propval to the property's value
	**  and return it.
	*/

	for (i = 0; i < MAX_NI_LEVELS && propval == NULL; i++)
	{
		if (i == 0)
		{
			nis = ni_open(NULL, LOCAL_NETINFO_DOMAIN, &ni);
			if (tTd(38, 20))
				dprintf("ni_open(LOCAL) = %d\n", nis);
		}
		else
		{
			if (lastni != NULL)
				ni_free(lastni);
			lastni = ni;
			nis = ni_open(lastni, PARENT_NETINFO_DOMAIN, &ni);
			if (tTd(38, 20))
				dprintf("ni_open(PARENT) = %d\n", nis);
		}

		/*
		**  Don't bother if we didn't get a handle on a
		**  proper domain.  This is not necessarily an error.
		**  We would get a positive ni_status if, for instance
		**  we never found the directory or property and tried
		**  to open the parent of the root domain!
		*/

		if (nis != 0)
			break;

		/*
		**  Find the path to the server information.
		*/

		if (ni_pathsearch(ni, &nid, keybuf) != 0)
			continue;

		/*
		**  Find associated value information.
		*/

		if (ni_lookupprop(ni, &nid, valprop, &ninl) != 0)
			continue;

		if (tTd(38, 20))
			dprintf("ni_lookupprop: len=%d\n",
				ninl.ni_namelist_len);

		/*
		**  See if we have an acceptable number of values.
		*/

		if (ninl.ni_namelist_len <= 0)
			continue;

		if (sepchar == '\0' && ninl.ni_namelist_len > 1)
		{
			ni_namelist_free(&ninl);
			continue;
		}

		/*
		**  Calculate number of bytes needed and build result
		*/

		alen = 1;
		for (j = 0; j < ninl.ni_namelist_len; j++)
			alen += strlen(ninl.ni_namelist_val[j]) + 1;
		propval = p = xalloc(alen);
		for (j = 0; j < ninl.ni_namelist_len; j++)
		{
			(void) strlcpy(p, ninl.ni_namelist_val[j], alen);
			l = strlen(p);
			p += l;
			*p++ = sepchar;
			alen -= l + 1;
		}
		*--p = '\0';

		ni_namelist_free(&ninl);
	}

	/*
	**  Clean up.
	*/

	if (ni != NULL)
		ni_free(ni);
	if (lastni != NULL && ni != lastni)
		ni_free(lastni);
	if (tTd(38, 20))
		dprintf("ni_propval returns: '%s'\n", propval);

	return propval;
}

#endif /* NETINFO */
/*
**  TEXT (unindexed text file) Modules
**
**	This code donated by Sun Microsystems.
*/

#define map_sff		map_lockfd	/* overload field */


/*
**  TEXT_MAP_OPEN -- open text table
*/

bool
text_map_open(map, mode)
	MAP *map;
	int mode;
{
	long sff;
	int i;

	if (tTd(38, 2))
		dprintf("text_map_open(%s, %s, %d)\n",
			map->map_mname, map->map_file, mode);

	mode &= O_ACCMODE;
	if (mode != O_RDONLY)
	{
		errno = EPERM;
		return FALSE;
	}

	if (*map->map_file == '\0')
	{
		syserr("text map \"%s\": file name required",
			map->map_mname);
		return FALSE;
	}

	if (map->map_file[0] != '/')
	{
		syserr("text map \"%s\": file name must be fully qualified",
			map->map_mname);
		return FALSE;
	}

	sff = SFF_ROOTOK|SFF_REGONLY;
	if (!bitnset(DBS_LINKEDMAPINWRITABLEDIR, DontBlameSendmail))
		sff |= SFF_NOWLINK;
	if (!bitnset(DBS_MAPINUNSAFEDIRPATH, DontBlameSendmail))
		sff |= SFF_SAFEDIRPATH;
	if ((i = safefile(map->map_file, RunAsUid, RunAsGid, RunAsUserName,
			  sff, S_IRUSR, NULL)) != 0)
	{
		int save_errno = errno;

		/* cannot open this map */
		if (tTd(38, 2))
			dprintf("\tunsafe map file: %d\n", i);
		errno = save_errno;
		if (!bitset(MF_OPTIONAL, map->map_mflags))
			syserr("text map \"%s\": unsafe map file %s",
				map->map_mname, map->map_file);
		return FALSE;
	}

	if (map->map_keycolnm == NULL)
		map->map_keycolno = 0;
	else
	{
		if (!(isascii(*map->map_keycolnm) && isdigit(*map->map_keycolnm)))
		{
			syserr("text map \"%s\", file %s: -k should specify a number, not %s",
				map->map_mname, map->map_file,
				map->map_keycolnm);
			return FALSE;
		}
		map->map_keycolno = atoi(map->map_keycolnm);
	}

	if (map->map_valcolnm == NULL)
		map->map_valcolno = 0;
	else
	{
		if (!(isascii(*map->map_valcolnm) && isdigit(*map->map_valcolnm)))
		{
			syserr("text map \"%s\", file %s: -v should specify a number, not %s",
					map->map_mname, map->map_file,
					map->map_valcolnm);
			return FALSE;
		}
		map->map_valcolno = atoi(map->map_valcolnm);
	}

	if (tTd(38, 2))
	{
		dprintf("text_map_open(%s, %s): delimiter = ",
			map->map_mname, map->map_file);
		if (map->map_coldelim == '\0')
			dprintf("(white space)\n");
		else
			dprintf("%c\n", map->map_coldelim);
	}

	map->map_sff = sff;
	return TRUE;
}


/*
**  TEXT_MAP_LOOKUP -- look up a datum in a TEXT table
*/

char *
text_map_lookup(map, name, av, statp)
	MAP *map;
	char *name;
	char **av;
	int *statp;
{
	char *vp;
	auto int vsize;
	int buflen;
	FILE *f;
	char delim;
	int key_idx;
	bool found_it;
	long sff = map->map_sff;
	char search_key[MAXNAME + 1];
	char linebuf[MAXLINE];
	char buf[MAXNAME + 1];

	found_it = FALSE;
	if (tTd(38, 20))
		dprintf("text_map_lookup(%s, %s)\n", map->map_mname,  name);

	buflen = strlen(name);
	if (buflen > sizeof search_key - 1)
		buflen = sizeof search_key - 1;
	memmove(search_key, name, buflen);
	search_key[buflen] = '\0';
	if (!bitset(MF_NOFOLDCASE, map->map_mflags))
		makelower(search_key);

	f = safefopen(map->map_file, O_RDONLY, FileMode, sff);
	if (f == NULL)
	{
		map->map_mflags &= ~(MF_VALID|MF_OPEN);
		*statp = EX_UNAVAILABLE;
		return NULL;
	}
	key_idx = map->map_keycolno;
	delim = map->map_coldelim;
	while (fgets(linebuf, MAXLINE, f) != NULL)
	{
		char *p;

		/* skip comment line */
		if (linebuf[0] == '#')
			continue;
		p = strchr(linebuf, '\n');
		if (p != NULL)
			*p = '\0';
		p = get_column(linebuf, key_idx, delim, buf, sizeof buf);
		if (p != NULL && strcasecmp(search_key, p) == 0)
		{
			found_it = TRUE;
			break;
		}
	}
	(void) fclose(f);
	if (!found_it)
	{
		*statp = EX_NOTFOUND;
		return NULL;
	}
	vp = get_column(linebuf, map->map_valcolno, delim, buf, sizeof buf);
	if (vp == NULL)
	{
		*statp = EX_NOTFOUND;
		return NULL;
	}
	vsize = strlen(vp);
	*statp = EX_OK;
	if (bitset(MF_MATCHONLY, map->map_mflags))
		return map_rewrite(map, name, strlen(name), NULL);
	else
		return map_rewrite(map, vp, vsize, av);
}

/*
**  TEXT_GETCANONNAME -- look up canonical name in hosts file
*/

static bool
text_getcanonname(name, hbsize, statp)
	char *name;
	int hbsize;
	int *statp;
{
	bool found;
	char *dot;
	FILE *f;
	char linebuf[MAXLINE];
	char cbuf[MAXNAME + 1];
	char nbuf[MAXNAME + 1];

	if (tTd(38, 20))
		dprintf("text_getcanonname(%s)\n", name);

	if (strlen(name) >= (SIZE_T) sizeof nbuf)
	{
		*statp = EX_UNAVAILABLE;
		return FALSE;
	}
	(void) strlcpy(nbuf, name, sizeof nbuf);
	dot = shorten_hostname(nbuf);

	f = fopen(HostsFile, "r");
	if (f == NULL)
	{
		*statp = EX_UNAVAILABLE;
		return FALSE;
	}
	found = FALSE;
	while (!found && fgets(linebuf, MAXLINE, f) != NULL)
	{
		char *p = strpbrk(linebuf, "#\n");

		if (p != NULL)
			*p = '\0';
		if (linebuf[0] != '\0')
			found = extract_canonname(nbuf, dot, linebuf,
						  cbuf, sizeof cbuf);
	}
	(void) fclose(f);
	if (!found)
	{
		*statp = EX_NOHOST;
		return FALSE;
	}

	if ((SIZE_T) hbsize >= strlen(cbuf))
	{
		(void) strlcpy(name, cbuf, hbsize);
		*statp = EX_OK;
		return TRUE;
	}
	*statp = EX_UNAVAILABLE;
	return FALSE;
}
/*
**  STAB (Symbol Table) Modules
*/


/*
**  STAB_MAP_LOOKUP -- look up alias in symbol table
*/

/* ARGSUSED2 */
char *
stab_map_lookup(map, name, av, pstat)
	register MAP *map;
	char *name;
	char **av;
	int *pstat;
{
	register STAB *s;

	if (tTd(38, 20))
		dprintf("stab_lookup(%s, %s)\n",
			map->map_mname, name);

	s = stab(name, ST_ALIAS, ST_FIND);
	if (s != NULL)
		return s->s_alias;
	return NULL;
}


/*
**  STAB_MAP_STORE -- store in symtab (actually using during init, not rebuild)
*/

void
stab_map_store(map, lhs, rhs)
	register MAP *map;
	char *lhs;
	char *rhs;
{
	register STAB *s;

	s = stab(lhs, ST_ALIAS, ST_ENTER);
	s->s_alias = newstr(rhs);
}


/*
**  STAB_MAP_OPEN -- initialize (reads data file)
**
**	This is a wierd case -- it is only intended as a fallback for
**	aliases.  For this reason, opens for write (only during a
**	"newaliases") always fails, and opens for read open the
**	actual underlying text file instead of the database.
*/

bool
stab_map_open(map, mode)
	register MAP *map;
	int mode;
{
	FILE *af;
	long sff;
	struct stat st;

	if (tTd(38, 2))
		dprintf("stab_map_open(%s, %s, %d)\n",
			map->map_mname, map->map_file, mode);

	mode &= O_ACCMODE;
	if (mode != O_RDONLY)
	{
		errno = EPERM;
		return FALSE;
	}

	sff = SFF_ROOTOK|SFF_REGONLY;
	if (!bitnset(DBS_LINKEDMAPINWRITABLEDIR, DontBlameSendmail))
		sff |= SFF_NOWLINK;
	if (!bitnset(DBS_MAPINUNSAFEDIRPATH, DontBlameSendmail))
		sff |= SFF_SAFEDIRPATH;
	af = safefopen(map->map_file, O_RDONLY, 0444, sff);
	if (af == NULL)
		return FALSE;
	readaliases(map, af, FALSE, FALSE);

	if (fstat(fileno(af), &st) >= 0)
		map->map_mtime = st.st_mtime;
	(void) fclose(af);

	return TRUE;
}
/*
**  Implicit Modules
**
**	Tries several types.  For back compatibility of aliases.
*/


/*
**  IMPL_MAP_LOOKUP -- lookup in best open database
*/

char *
impl_map_lookup(map, name, av, pstat)
	MAP *map;
	char *name;
	char **av;
	int *pstat;
{
	if (tTd(38, 20))
		dprintf("impl_map_lookup(%s, %s)\n",
			map->map_mname, name);

#ifdef NEWDB
	if (bitset(MF_IMPL_HASH, map->map_mflags))
		return db_map_lookup(map, name, av, pstat);
#endif /* NEWDB */
#ifdef NDBM
	if (bitset(MF_IMPL_NDBM, map->map_mflags))
		return ndbm_map_lookup(map, name, av, pstat);
#endif /* NDBM */
	return stab_map_lookup(map, name, av, pstat);
}

/*
**  IMPL_MAP_STORE -- store in open databases
*/

void
impl_map_store(map, lhs, rhs)
	MAP *map;
	char *lhs;
	char *rhs;
{
	if (tTd(38, 12))
		dprintf("impl_map_store(%s, %s, %s)\n",
			map->map_mname, lhs, rhs);
#ifdef NEWDB
	if (bitset(MF_IMPL_HASH, map->map_mflags))
		db_map_store(map, lhs, rhs);
#endif /* NEWDB */
#ifdef NDBM
	if (bitset(MF_IMPL_NDBM, map->map_mflags))
		ndbm_map_store(map, lhs, rhs);
#endif /* NDBM */
	stab_map_store(map, lhs, rhs);
}

/*
**  IMPL_MAP_OPEN -- implicit database open
*/

bool
impl_map_open(map, mode)
	MAP *map;
	int mode;
{
	if (tTd(38, 2))
		dprintf("impl_map_open(%s, %s, %d)\n",
			map->map_mname, map->map_file, mode);

	mode &= O_ACCMODE;
#ifdef NEWDB
	map->map_mflags |= MF_IMPL_HASH;
	if (hash_map_open(map, mode))
	{
# ifdef NDBM_YP_COMPAT
		if (mode == O_RDONLY || strstr(map->map_file, "/yp/") == NULL)
# endif /* NDBM_YP_COMPAT */
			return TRUE;
	}
	else
		map->map_mflags &= ~MF_IMPL_HASH;
#endif /* NEWDB */
#ifdef NDBM
	map->map_mflags |= MF_IMPL_NDBM;
	if (ndbm_map_open(map, mode))
	{
		return TRUE;
	}
	else
		map->map_mflags &= ~MF_IMPL_NDBM;
#endif /* NDBM */

#if defined(NEWDB) || defined(NDBM)
	if (Verbose)
		message("WARNING: cannot open alias database %s%s",
			map->map_file,
			mode == O_RDONLY ? "; reading text version" : "");
#else /* defined(NEWDB) || defined(NDBM) */
	if (mode != O_RDONLY)
		usrerr("Cannot rebuild aliases: no database format defined");
#endif /* defined(NEWDB) || defined(NDBM) */

	if (mode == O_RDONLY)
		return stab_map_open(map, mode);
	else
		return FALSE;
}


/*
**  IMPL_MAP_CLOSE -- close any open database(s)
*/

void
impl_map_close(map)
	MAP *map;
{
	if (tTd(38, 9))
		dprintf("impl_map_close(%s, %s, %lx)\n",
			map->map_mname, map->map_file, map->map_mflags);
#ifdef NEWDB
	if (bitset(MF_IMPL_HASH, map->map_mflags))
	{
		db_map_close(map);
		map->map_mflags &= ~MF_IMPL_HASH;
	}
#endif /* NEWDB */

#ifdef NDBM
	if (bitset(MF_IMPL_NDBM, map->map_mflags))
	{
		ndbm_map_close(map);
		map->map_mflags &= ~MF_IMPL_NDBM;
	}
#endif /* NDBM */
}
/*
**  User map class.
**
**	Provides access to the system password file.
*/

/*
**  USER_MAP_OPEN -- open user map
**
**	Really just binds field names to field numbers.
*/

bool
user_map_open(map, mode)
	MAP *map;
	int mode;
{
	if (tTd(38, 2))
		dprintf("user_map_open(%s, %d)\n",
			map->map_mname, mode);

	mode &= O_ACCMODE;
	if (mode != O_RDONLY)
	{
		/* issue a pseudo-error message */
#ifdef ENOSYS
		errno = ENOSYS;
#else /* ENOSYS */
# ifdef EFTYPE
		errno = EFTYPE;
# else /* EFTYPE */
		errno = ENXIO;
# endif /* EFTYPE */
#endif /* ENOSYS */
		return FALSE;
	}
	if (map->map_valcolnm == NULL)
		/* EMPTY */
		/* nothing */ ;
	else if (strcasecmp(map->map_valcolnm, "name") == 0)
		map->map_valcolno = 1;
	else if (strcasecmp(map->map_valcolnm, "passwd") == 0)
		map->map_valcolno = 2;
	else if (strcasecmp(map->map_valcolnm, "uid") == 0)
		map->map_valcolno = 3;
	else if (strcasecmp(map->map_valcolnm, "gid") == 0)
		map->map_valcolno = 4;
	else if (strcasecmp(map->map_valcolnm, "gecos") == 0)
		map->map_valcolno = 5;
	else if (strcasecmp(map->map_valcolnm, "dir") == 0)
		map->map_valcolno = 6;
	else if (strcasecmp(map->map_valcolnm, "shell") == 0)
		map->map_valcolno = 7;
	else
	{
		syserr("User map %s: unknown column name %s",
			map->map_mname, map->map_valcolnm);
		return FALSE;
	}
	return TRUE;
}


/*
**  USER_MAP_LOOKUP -- look up a user in the passwd file.
*/

/* ARGSUSED3 */
char *
user_map_lookup(map, key, av, statp)
	MAP *map;
	char *key;
	char **av;
	int *statp;
{
	struct passwd *pw;
	auto bool fuzzy;

	if (tTd(38, 20))
		dprintf("user_map_lookup(%s, %s)\n",
			map->map_mname, key);

	pw = finduser(key, &fuzzy);
	if (pw == NULL)
		return NULL;
	if (bitset(MF_MATCHONLY, map->map_mflags))
		return map_rewrite(map, key, strlen(key), NULL);
	else
	{
		char *rwval = NULL;
		char buf[30];

		switch (map->map_valcolno)
		{
		  case 0:
		  case 1:
			rwval = pw->pw_name;
			break;

		  case 2:
			rwval = pw->pw_passwd;
			break;

		  case 3:
			snprintf(buf, sizeof buf, "%d", (int) pw->pw_uid);
			rwval = buf;
			break;

		  case 4:
			snprintf(buf, sizeof buf, "%d", (int) pw->pw_gid);
			rwval = buf;
			break;

		  case 5:
			rwval = pw->pw_gecos;
			break;

		  case 6:
			rwval = pw->pw_dir;
			break;

		  case 7:
			rwval = pw->pw_shell;
			break;
		}
		return map_rewrite(map, rwval, strlen(rwval), av);
	}
}
/*
**  Program map type.
**
**	This provides access to arbitrary programs.  It should be used
**	only very sparingly, since there is no way to bound the cost
**	of invoking an arbitrary program.
*/

char *
prog_map_lookup(map, name, av, statp)
	MAP *map;
	char *name;
	char **av;
	int *statp;
{
	int i;
	int save_errno;
	int fd;
	int status;
	auto pid_t pid;
	register char *p;
	char *rval;
	char *argv[MAXPV + 1];
	char buf[MAXLINE];

	if (tTd(38, 20))
		dprintf("prog_map_lookup(%s, %s) %s\n",
			map->map_mname, name, map->map_file);

	i = 0;
	argv[i++] = map->map_file;
	if (map->map_rebuild != NULL)
	{
		snprintf(buf, sizeof buf, "%s", map->map_rebuild);
		for (p = strtok(buf, " \t"); p != NULL; p = strtok(NULL, " \t"))
		{
			if (i >= MAXPV - 1)
				break;
			argv[i++] = p;
		}
	}
	argv[i++] = name;
	argv[i] = NULL;
	if (tTd(38, 21))
	{
		dprintf("prog_open:");
		for (i = 0; argv[i] != NULL; i++)
			dprintf(" %s", argv[i]);
		dprintf("\n");
	}
	(void) blocksignal(SIGCHLD);
	pid = prog_open(argv, &fd, CurEnv);
	if (pid < 0)
	{
		if (!bitset(MF_OPTIONAL, map->map_mflags))
			syserr("prog_map_lookup(%s) failed (%s) -- closing",
				map->map_mname, errstring(errno));
		else if (tTd(38, 9))
			dprintf("prog_map_lookup(%s) failed (%s) -- closing",
				map->map_mname, errstring(errno));
		map->map_mflags &= ~(MF_VALID|MF_OPEN);
		*statp = EX_OSFILE;
		return NULL;
	}
	i = read(fd, buf, sizeof buf - 1);
	if (i < 0)
	{
		syserr("prog_map_lookup(%s): read error %s\n",
			map->map_mname, errstring(errno));
		rval = NULL;
	}
	else if (i == 0)
	{
		if (tTd(38, 20))
			dprintf("prog_map_lookup(%s): empty answer\n",
				map->map_mname);
		rval = NULL;
	}
	else
	{
		buf[i] = '\0';
		p = strchr(buf, '\n');
		if (p != NULL)
			*p = '\0';

		/* collect the return value */
		if (bitset(MF_MATCHONLY, map->map_mflags))
			rval = map_rewrite(map, name, strlen(name), NULL);
		else
			rval = map_rewrite(map, buf, strlen(buf), av);

		/* now flush any additional output */
		while ((i = read(fd, buf, sizeof buf)) > 0)
			continue;
	}

	/* wait for the process to terminate */
	(void) close(fd);
	status = waitfor(pid);
	save_errno = errno;
	(void) releasesignal(SIGCHLD);
	errno = save_errno;

	if (status == -1)
	{
		syserr("prog_map_lookup(%s): wait error %s\n",
			map->map_mname, errstring(errno));
		*statp = EX_SOFTWARE;
		rval = NULL;
	}
	else if (WIFEXITED(status))
	{
		if ((*statp = WEXITSTATUS(status)) != EX_OK)
			rval = NULL;
	}
	else
	{
		syserr("prog_map_lookup(%s): child died on signal %d",
			map->map_mname, status);
		*statp = EX_UNAVAILABLE;
		rval = NULL;
	}
	return rval;
}
/*
**  Sequenced map type.
**
**	Tries each map in order until something matches, much like
**	implicit.  Stores go to the first map in the list that can
**	support storing.
**
**	This is slightly unusual in that there are two interfaces.
**	The "sequence" interface lets you stack maps arbitrarily.
**	The "switch" interface builds a sequence map by looking
**	at a system-dependent configuration file such as
**	/etc/nsswitch.conf on Solaris or /etc/svc.conf on Ultrix.
**
**	We don't need an explicit open, since all maps are
**	opened during startup, including underlying maps.
*/

/*
**  SEQ_MAP_PARSE -- Sequenced map parsing
*/

bool
seq_map_parse(map, ap)
	MAP *map;
	char *ap;
{
	int maxmap;

	if (tTd(38, 2))
		dprintf("seq_map_parse(%s, %s)\n", map->map_mname, ap);
	maxmap = 0;
	while (*ap != '\0')
	{
		register char *p;
		STAB *s;

		/* find beginning of map name */
		while (isascii(*ap) && isspace(*ap))
			ap++;
		for (p = ap;
		     (isascii(*p) && isalnum(*p)) || *p == '_' || *p == '.';
		     p++)
			continue;
		if (*p != '\0')
			*p++ = '\0';
		while (*p != '\0' && (!isascii(*p) || !isalnum(*p)))
			p++;
		if (*ap == '\0')
		{
			ap = p;
			continue;
		}
		s = stab(ap, ST_MAP, ST_FIND);
		if (s == NULL)
		{
			syserr("Sequence map %s: unknown member map %s",
				map->map_mname, ap);
		}
		else if (maxmap == MAXMAPSTACK)
		{
			syserr("Sequence map %s: too many member maps (%d max)",
				map->map_mname, MAXMAPSTACK);
			maxmap++;
		}
		else if (maxmap < MAXMAPSTACK)
		{
			map->map_stack[maxmap++] = &s->s_map;
		}
		ap = p;
	}
	return TRUE;
}


/*
**  SWITCH_MAP_OPEN -- open a switched map
**
**	This looks at the system-dependent configuration and builds
**	a sequence map that does the same thing.
**
**	Every system must define a switch_map_find routine in conf.c
**	that will return the list of service types associated with a
**	given service class.
*/

bool
switch_map_open(map, mode)
	MAP *map;
	int mode;
{
	int mapno;
	int nmaps;
	char *maptype[MAXMAPSTACK];

	if (tTd(38, 2))
		dprintf("switch_map_open(%s, %s, %d)\n",
			map->map_mname, map->map_file, mode);

	mode &= O_ACCMODE;
	nmaps = switch_map_find(map->map_file, maptype, map->map_return);
	if (tTd(38, 19))
	{
		dprintf("\tswitch_map_find => %d\n", nmaps);
		for (mapno = 0; mapno < nmaps; mapno++)
			dprintf("\t\t%s\n", maptype[mapno]);
	}
	if (nmaps <= 0 || nmaps > MAXMAPSTACK)
		return FALSE;

	for (mapno = 0; mapno < nmaps; mapno++)
	{
		register STAB *s;
		char nbuf[MAXNAME + 1];

		if (maptype[mapno] == NULL)
			continue;
		(void) snprintf(nbuf, sizeof nbuf, "%s.%s",
			map->map_mname, maptype[mapno]);
		s = stab(nbuf, ST_MAP, ST_FIND);
		if (s == NULL)
		{
			syserr("Switch map %s: unknown member map %s",
				map->map_mname, nbuf);
		}
		else
		{
			map->map_stack[mapno] = &s->s_map;
			if (tTd(38, 4))
				dprintf("\tmap_stack[%d] = %s:%s\n",
					mapno, s->s_map.map_class->map_cname,
					nbuf);
		}
	}
	return TRUE;
}


/*
**  SEQ_MAP_CLOSE -- close all underlying maps
*/

void
seq_map_close(map)
	MAP *map;
{
	int mapno;

	if (tTd(38, 9))
		dprintf("seq_map_close(%s)\n", map->map_mname);

	for (mapno = 0; mapno < MAXMAPSTACK; mapno++)
	{
		MAP *mm = map->map_stack[mapno];

		if (mm == NULL || !bitset(MF_OPEN, mm->map_mflags))
			continue;
		mm->map_mflags |= MF_CLOSING;
		mm->map_class->map_close(mm);
		mm->map_mflags &= ~(MF_OPEN|MF_WRITABLE|MF_CLOSING);
	}
}


/*
**  SEQ_MAP_LOOKUP -- sequenced map lookup
*/

char *
seq_map_lookup(map, key, args, pstat)
	MAP *map;
	char *key;
	char **args;
	int *pstat;
{
	int mapno;
	int mapbit = 0x01;
	bool tempfail = FALSE;

	if (tTd(38, 20))
		dprintf("seq_map_lookup(%s, %s)\n", map->map_mname, key);

	for (mapno = 0; mapno < MAXMAPSTACK; mapbit <<= 1, mapno++)
	{
		MAP *mm = map->map_stack[mapno];
		char *rv;

		if (mm == NULL)
			continue;
		if (!bitset(MF_OPEN, mm->map_mflags) &&
		    !openmap(mm))
		{
			if (bitset(mapbit, map->map_return[MA_UNAVAIL]))
			{
				*pstat = EX_UNAVAILABLE;
				return NULL;
			}
			continue;
		}
		*pstat = EX_OK;
		rv = mm->map_class->map_lookup(mm, key, args, pstat);
		if (rv != NULL)
			return rv;
		if (*pstat == EX_TEMPFAIL)
		{
			if (bitset(mapbit, map->map_return[MA_TRYAGAIN]))
				return NULL;
			tempfail = TRUE;
		}
		else if (bitset(mapbit, map->map_return[MA_NOTFOUND]))
			break;
	}
	if (tempfail)
		*pstat = EX_TEMPFAIL;
	else if (*pstat == EX_OK)
		*pstat = EX_NOTFOUND;
	return NULL;
}


/*
**  SEQ_MAP_STORE -- sequenced map store
*/

void
seq_map_store(map, key, val)
	MAP *map;
	char *key;
	char *val;
{
	int mapno;

	if (tTd(38, 12))
		dprintf("seq_map_store(%s, %s, %s)\n",
			map->map_mname, key, val);

	for (mapno = 0; mapno < MAXMAPSTACK; mapno++)
	{
		MAP *mm = map->map_stack[mapno];

		if (mm == NULL || !bitset(MF_WRITABLE, mm->map_mflags))
			continue;

		mm->map_class->map_store(mm, key, val);
		return;
	}
	syserr("seq_map_store(%s, %s, %s): no writable map",
		map->map_mname, key, val);
}
/*
**  NULL stubs
*/

/* ARGSUSED */
bool
null_map_open(map, mode)
	MAP *map;
	int mode;
{
	return TRUE;
}

/* ARGSUSED */
void
null_map_close(map)
	MAP *map;
{
	return;
}

char *
null_map_lookup(map, key, args, pstat)
	MAP *map;
	char *key;
	char **args;
	int *pstat;
{
	*pstat = EX_NOTFOUND;
	return NULL;
}

/* ARGSUSED */
void
null_map_store(map, key, val)
	MAP *map;
	char *key;
	char *val;
{
	return;
}


/*
**  BOGUS stubs
*/

char *
bogus_map_lookup(map, key, args, pstat)
	MAP *map;
	char *key;
	char **args;
	int *pstat;
{
	*pstat = EX_TEMPFAIL;
	return NULL;
}

MAPCLASS	BogusMapClass =
{
	"bogus-map",		NULL,		0,
	NULL,		bogus_map_lookup,	null_map_store,
	null_map_open,	null_map_close,
};
/*
**  MACRO modules
*/

char *
macro_map_lookup(map, name, av, statp)
	MAP *map;
	char *name;
	char **av;
	int *statp;
{
	int mid;

	if (tTd(38, 20))
		dprintf("macro_map_lookup(%s, %s)\n", map->map_mname,
			name == NULL ? "NULL" : name);

	if (name == NULL ||
	    *name == '\0' ||
	    (mid = macid(name, NULL)) == '\0')
	{
		*statp = EX_CONFIG;
		return NULL;
	}

	if (av[1] == NULL)
		define(mid, NULL, CurEnv);
	else
		define(mid, newstr(av[1]), CurEnv);

	*statp = EX_OK;
	return "";
}
/*
**  REGEX modules
*/

#ifdef MAP_REGEX

# include <regex.h>

# define DEFAULT_DELIM	CONDELSE

# define END_OF_FIELDS	-1

# define ERRBUF_SIZE	80
# define MAX_MATCH	32

# define xnalloc(s)	memset(xalloc(s), '\0', s);

struct regex_map
{
	regex_t	*regex_pattern_buf;	/* xalloc it */
	int	*regex_subfields;	/* move to type MAP */
	char	*regex_delim;		/* move to type MAP */
};

static int
parse_fields(s, ibuf, blen, nr_substrings)
	char *s;
	int *ibuf;		/* array */
	int blen;		/* number of elements in ibuf */
	int nr_substrings;	/* number of substrings in the pattern */
{
	register char *cp;
	int i = 0;
	bool lastone = FALSE;

	blen--;		/* for terminating END_OF_FIELDS */
	cp = s;
	do
	{
		for (;; cp++)
		{
			if (*cp == ',')
			{
				*cp = '\0';
				break;
			}
			if (*cp == '\0')
			{
				lastone = TRUE;
				break;
			}
		}
		if (i < blen)
		{
			int val = atoi(s);

			if (val < 0 || val >= nr_substrings)
			{
				syserr("field (%d) out of range, only %d substrings in pattern",
				       val, nr_substrings);
				return -1;
			}
			ibuf[i++] = val;
		}
		else
		{
			syserr("too many fields, %d max\n", blen);
			return -1;
		}
		s = ++cp;
	} while (!lastone);
	ibuf[i] = END_OF_FIELDS;
	return i;
}

bool
regex_map_init(map, ap)
	MAP *map;
	char *ap;
{
	int regerr;
	struct regex_map *map_p;
	register char *p;
	char *sub_param = NULL;
	int pflags;
	static char defdstr[] = { (char)DEFAULT_DELIM, '\0' };

	if (tTd(38, 2))
		dprintf("regex_map_init: mapname '%s', args '%s'\n",
			map->map_mname, ap);

	pflags = REG_ICASE | REG_EXTENDED | REG_NOSUB;

	p = ap;

	map_p = (struct regex_map *) xnalloc(sizeof *map_p);
	map_p->regex_pattern_buf = (regex_t *)xnalloc(sizeof(regex_t));

	for (;;)
	{
		while (isascii(*p) && isspace(*p))
			p++;
		if (*p != '-')
			break;
		switch (*++p)
		{
		  case 'n':	/* not */
			map->map_mflags |= MF_REGEX_NOT;
			break;

		  case 'f':	/* case sensitive */
			map->map_mflags |= MF_NOFOLDCASE;
			pflags &= ~REG_ICASE;
			break;

		  case 'b':	/* basic regular expressions */
			pflags &= ~REG_EXTENDED;
			break;

		  case 's':	/* substring match () syntax */
			sub_param = ++p;
			pflags &= ~REG_NOSUB;
			break;

		  case 'd':	/* delimiter */
			map_p->regex_delim = ++p;
			break;

		  case 'a':	/* map append */
			map->map_app = ++p;
			break;

		  case 'm':	/* matchonly */
			map->map_mflags |= MF_MATCHONLY;
			break;

		  case 'S':
			map->map_spacesub = *++p;
			break;

		  case 'D':
			map->map_mflags |= MF_DEFER;
			break;

		}
		while (*p != '\0' && !(isascii(*p) && isspace(*p)))
			p++;
		if (*p != '\0')
			*p++ = '\0';
	}
	if (tTd(38, 3))
		dprintf("regex_map_init: compile '%s' 0x%x\n", p, pflags);

	if ((regerr = regcomp(map_p->regex_pattern_buf, p, pflags)) != 0)
	{
		/* Errorhandling */
		char errbuf[ERRBUF_SIZE];

		(void) regerror(regerr, map_p->regex_pattern_buf,
			 errbuf, ERRBUF_SIZE);
		syserr("pattern-compile-error: %s\n", errbuf);
		sm_free(map_p->regex_pattern_buf);
		sm_free(map_p);
		return FALSE;
	}

	if (map->map_app != NULL)
		map->map_app = newstr(map->map_app);
	if (map_p->regex_delim != NULL)
		map_p->regex_delim = newstr(map_p->regex_delim);
	else
		map_p->regex_delim = defdstr;

	if (!bitset(REG_NOSUB, pflags))
	{
		/* substring matching */
		int substrings;
		int *fields = (int *) xalloc(sizeof(int) * (MAX_MATCH + 1));

		substrings = map_p->regex_pattern_buf->re_nsub + 1;

		if (tTd(38, 3))
			dprintf("regex_map_init: nr of substrings %d\n",
				substrings);

		if (substrings >= MAX_MATCH)
		{
			syserr("too many substrings, %d max\n", MAX_MATCH);
			sm_free(map_p->regex_pattern_buf);
			sm_free(map_p);
			return FALSE;
		}
		if (sub_param != NULL && sub_param[0] != '\0')
		{
			/* optional parameter -sfields */
			if (parse_fields(sub_param, fields,
					 MAX_MATCH + 1, substrings) == -1)
				return FALSE;
		}
		else
		{
			/* set default fields */
			int i;

			for (i = 0; i < substrings; i++)
				fields[i] = i;
			fields[i] = END_OF_FIELDS;
		}
		map_p->regex_subfields = fields;
		if (tTd(38, 3))
		{
			int *ip;

			dprintf("regex_map_init: subfields");
			for (ip = fields; *ip != END_OF_FIELDS; ip++)
				dprintf(" %d", *ip);
			dprintf("\n");
		}
	}
	map->map_db1 = (ARBPTR_T)map_p;	/* dirty hack */

	return TRUE;
}

static char *
regex_map_rewrite(map, s, slen, av)
	MAP *map;
	const char *s;
	size_t slen;
	char **av;
{
	if (bitset(MF_MATCHONLY, map->map_mflags))
		return map_rewrite(map, av[0], strlen(av[0]), NULL);
	else
		return map_rewrite(map, s, slen, av);
}

char *
regex_map_lookup(map, name, av, statp)
	MAP *map;
	char *name;
	char **av;
	int *statp;
{
	int reg_res;
	struct regex_map *map_p;
	regmatch_t pmatch[MAX_MATCH];

	if (tTd(38, 20))
	{
		char **cpp;

		dprintf("regex_map_lookup: key '%s'\n", name);
		for (cpp = av; cpp != NULL && *cpp != NULL; cpp++)
			dprintf("regex_map_lookup: arg '%s'\n", *cpp);
	}

	map_p = (struct regex_map *)(map->map_db1);
	reg_res = regexec(map_p->regex_pattern_buf,
			  name, MAX_MATCH, pmatch, 0);

	if (bitset(MF_REGEX_NOT, map->map_mflags))
	{
		/* option -n */
		if (reg_res == REG_NOMATCH)
			return regex_map_rewrite(map, "", (size_t)0, av);
		else
			return NULL;
	}
	if (reg_res == REG_NOMATCH)
		return NULL;

	if (map_p->regex_subfields != NULL)
	{
		/* option -s */
		static char retbuf[MAXNAME];
		int fields[MAX_MATCH + 1];
		bool first = TRUE;
		int anglecnt = 0, cmntcnt = 0, spacecnt = 0;
		bool quotemode = FALSE, bslashmode = FALSE;
		register char *dp, *sp;
		char *endp, *ldp;
		int *ip;

		dp = retbuf;
		ldp = retbuf + sizeof(retbuf) - 1;

		if (av[1] != NULL)
		{
			if (parse_fields(av[1], fields, MAX_MATCH + 1,
					 (int) map_p->regex_pattern_buf->re_nsub + 1) == -1)
			{
				*statp = EX_CONFIG;
				return NULL;
			}
			ip = fields;
		}
		else
			ip = map_p->regex_subfields;

		for ( ; *ip != END_OF_FIELDS; ip++)
		{
			if (!first)
			{
				for (sp = map_p->regex_delim; *sp; sp++)
				{
					if (dp < ldp)
						*dp++ = *sp;
				}
			}
			else
				first = FALSE;


			if (*ip >= MAX_MATCH ||
			    pmatch[*ip].rm_so < 0 || pmatch[*ip].rm_eo < 0)
				continue;

			sp = name + pmatch[*ip].rm_so;
			endp = name + pmatch[*ip].rm_eo;
			for (; endp > sp; sp++)
			{
				if (dp < ldp)
				{
					if (bslashmode)
					{
						*dp++ = *sp;
						bslashmode = FALSE;
					}
					else if (quotemode && *sp != '"' &&
						*sp != '\\')
					{
						*dp++ = *sp;
					}
					else switch(*dp++ = *sp)
					{
						case '\\':
						bslashmode = TRUE;
						break;

						case '(':
						cmntcnt++;
						break;

						case ')':
						cmntcnt--;
						break;

						case '<':
						anglecnt++;
						break;

						case '>':
						anglecnt--;
						break;

						case ' ':
						spacecnt++;
						break;

						case '"':
						quotemode = !quotemode;
						break;
					}
				}
			}
		}
		if (anglecnt != 0 || cmntcnt != 0 || quotemode ||
		    bslashmode || spacecnt != 0)
		{
			sm_syslog(LOG_WARNING, NOQID,
				  "Warning: regex may cause prescan() failure map=%s lookup=%s",
				  map->map_mname, name);
			return NULL;
		}

		*dp = '\0';

		return regex_map_rewrite(map, retbuf, strlen(retbuf), av);
	}
	return regex_map_rewrite(map, "", (size_t)0, av);
}
#endif /* MAP_REGEX */
/*
**  NSD modules
*/
#ifdef MAP_NSD

# include <ndbm.h>
# define _DATUM_DEFINED
# include <ns_api.h>

typedef struct ns_map_list
{
	ns_map_t *map;
	char *mapname;
	struct ns_map_list *next;
} ns_map_list_t;

static ns_map_t *
ns_map_t_find(mapname)
	char *mapname;
{
	static ns_map_list_t *ns_maps = NULL;
	ns_map_list_t *ns_map;

	/* walk the list of maps looking for the correctly named map */
	for (ns_map = ns_maps; ns_map != NULL; ns_map = ns_map->next)
	{
		if (strcmp(ns_map->mapname, mapname) == 0)
			break;
	}

	/* if we are looking at a NULL ns_map_list_t, then create a new one */
	if (ns_map == NULL)
	{
		ns_map = (ns_map_list_t *) xalloc(sizeof *ns_map);
		ns_map->mapname = newstr(mapname);
		ns_map->map = (ns_map_t *) xalloc(sizeof *ns_map->map);
		ns_map->next = ns_maps;
		ns_maps = ns_map;
	}
	return ns_map->map;
}

char *
nsd_map_lookup(map, name, av, statp)
	MAP *map;
	char *name;
	char **av;
	int *statp;
{
	int buflen, r;
	char *p;
	ns_map_t *ns_map;
	char keybuf[MAXNAME + 1];
	char buf[MAXLINE];

	if (tTd(38, 20))
		dprintf("nsd_map_lookup(%s, %s)\n", map->map_mname, name);

	buflen = strlen(name);
	if (buflen > sizeof keybuf - 1)
		buflen = sizeof keybuf - 1;
	memmove(keybuf, name, buflen);
	keybuf[buflen] = '\0';
	if (!bitset(MF_NOFOLDCASE, map->map_mflags))
		makelower(keybuf);

	ns_map = ns_map_t_find(map->map_file);
	if (ns_map == NULL)
	{
		if (tTd(38, 20))
			dprintf("nsd_map_t_find failed\n");
		*statp = EX_UNAVAILABLE;
		return NULL;
	}
	r = ns_lookup(ns_map, NULL, map->map_file, keybuf, NULL, buf, MAXLINE);
	if (r == NS_UNAVAIL || r == NS_TRYAGAIN)
	{
		*statp = EX_TEMPFAIL;
		return NULL;
	}
	if (r == NS_BADREQ
# ifdef NS_NOPERM
	    || r == NS_NOPERM
# endif /* NS_NOPERM */
	    )
	{
		*statp = EX_CONFIG;
		return NULL;
	}
	if (r != NS_SUCCESS)
	{
		*statp = EX_NOTFOUND;
		return NULL;
	}

	*statp = EX_OK;

	/* Null out trailing \n */
	if ((p = strchr(buf, '\n')) != NULL)
		*p = '\0';

	return map_rewrite(map, buf, strlen(buf), av);
}
#endif /* MAP_NSD */

char *
arith_map_lookup(map, name, av, statp)
	MAP *map;
	char *name;
	char **av;
	int *statp;
{
	long r;
	long v[2];
	bool res = FALSE;
	bool boolres;
	static char result[16];
	char **cpp;

	if (tTd(38, 2))
	{
		dprintf("arith_map_lookup: key '%s'\n", name);
		for (cpp = av; cpp != NULL && *cpp != NULL; cpp++)
			dprintf("arith_map_lookup: arg '%s'\n", *cpp);
	}
	r = 0;
	boolres = FALSE;
	cpp = av;
	*statp = EX_OK;

	/*
	**  read arguments for arith map
	**  - no check is made whether they are really numbers
	**  - just ignores args after the second
	*/
	for (++cpp; cpp != NULL && *cpp != NULL && r < 2; cpp++)
		v[r++] = strtol(*cpp, NULL, 0);

	/* operator and (at least) two operands given? */
	if (name != NULL && r == 2)
	{
		switch(*name)
		{
#if _FFR_ARITH
		  case '|':
			r = v[0] | v[1];
			break;

		  case '&':
			r = v[0] & v[1];
			break;

		  case '%':
			if (v[1] == 0)
				return NULL;
			r = v[0] % v[1];
			break;
#endif /* _FFR_ARITH */

		  case '+':
			r = v[0] + v[1];
			break;

		  case '-':
			r = v[0] - v[1];
			break;

		  case '*':
			r = v[0] * v[1];
			break;

		  case '/':
			if (v[1] == 0)
				return NULL;
			r = v[0] / v[1];
			break;

		  case 'l':
			res = v[0] < v[1];
			boolres = TRUE;
			break;

		  case '=':
			res = v[0] == v[1];
			boolres = TRUE;
			break;

		  default:
			/* XXX */
			*statp = EX_CONFIG;
			if (LogLevel > 10)
				sm_syslog(LOG_WARNING, NOQID,
					  "arith_map: unknown operator %c",
					  isprint(*name) ? *name : '?');
			return NULL;
		}
		if (boolres)
			snprintf(result, sizeof result, res ? "TRUE" : "FALSE");
		else
			snprintf(result, sizeof result, "%ld", r);
		return result;
	}
	*statp = EX_CONFIG;
	return NULL;
}
