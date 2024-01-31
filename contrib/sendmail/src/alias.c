/*
 * Copyright (c) 1998-2003 Proofpoint, Inc. and its suppliers.
 *	All rights reserved.
 * Copyright (c) 1983, 1995-1997 Eric P. Allman.  All rights reserved.
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 */

#include <sendmail.h>

SM_RCSID("@(#)$Id: alias.c,v 8.221 2013-11-22 20:51:54 ca Exp $")

#include <sm/sendmail.h>

#define SEPARATOR ':'
# define ALIAS_SPEC_SEPARATORS	" ,/:"

static MAP	*AliasFileMap = NULL;	/* the actual aliases.files map */
static int	NAliasFileMaps;	/* the number of entries in AliasFileMap */

static char	*aliaslookup __P((char *, int *, char *));

/*
**  ALIAS -- Compute aliases.
**
**	Scans the alias file for an alias for the given address.
**	If found, it arranges to deliver to the alias list instead.
**	Uses libdbm database if -DDBM.
**
**	Parameters:
**		a -- address to alias.
**		sendq -- a pointer to the head of the send queue
**			to put the aliases in.
**		aliaslevel -- the current alias nesting depth.
**		e -- the current envelope.
**
**	Returns:
**		none
**
**	Side Effects:
**		Aliases found are expanded.
**
**	Deficiencies:
**		It should complain about names that are aliased to
**			nothing.
*/

void
alias(a, sendq, aliaslevel, e)
	register ADDRESS *a;
	ADDRESS **sendq;
	int aliaslevel;
	register ENVELOPE *e;
{
	register char *p;
	char *owner;
	auto int status = EX_OK;
	char obuf[MAXNAME_I + 7];

	if (tTd(27, 1))
		sm_dprintf("alias(%s)\n", a->q_user);

	/* don't realias already aliased names */
	if (!QS_IS_OK(a->q_state))
		return;

	if (NoAlias)
		return;

	e->e_to = a->q_paddr;

	/*
	**  Look up this name.
	**
	**	If the map was unavailable, we will queue this message
	**	until the map becomes available; otherwise, we could
	**	bounce messages inappropriately.
	*/

#if _FFR_REDIRECTEMPTY
	/*
	**  envelope <> can't be sent to mailing lists, only owner-
	**  send spam of this type to owner- of the list
	**  ----  to stop spam from going to mailing lists!
	*/

	if (e->e_sender != NULL && *e->e_sender == '\0')
	{
		/* Look for owner of alias */
		(void) sm_strlcpyn(obuf, sizeof(obuf), 2, "owner-", a->q_user);
		if (aliaslookup(obuf, &status, a->q_host) != NULL)
		{
			if (LogLevel > 8)
				sm_syslog(LOG_WARNING, e->e_id,
				       "possible spam from <> to list: %s, redirected to %s\n",
				       a->q_user, obuf);
			a->q_user = sm_rpool_strdup_x(e->e_rpool, obuf);
		}
	}
#endif /* _FFR_REDIRECTEMPTY */

	p = aliaslookup(a->q_user, &status, a->q_host);
	if (status == EX_TEMPFAIL || status == EX_UNAVAILABLE)
	{
		a->q_state = QS_QUEUEUP;
		if (e->e_message == NULL)
			e->e_message = sm_rpool_strdup_x(e->e_rpool,
						"alias database unavailable");

		/* XXX msg only per recipient? */
		if (a->q_message == NULL)
			a->q_message = "alias database unavailable";
		return;
	}
	if (p == NULL)
		return;

	/*
	**  Match on Alias.
	**	Deliver to the target list.
	*/

	if (tTd(27, 1))
		sm_dprintf("%s (%s, %s) aliased to %s\n",
			   a->q_paddr, a->q_host, a->q_user, p);
	if (bitset(EF_VRFYONLY, e->e_flags))
	{
		a->q_state = QS_VERIFIED;
		return;
	}
	message("aliased to %s", shortenstring(p, MAXSHORTSTR));
	if (LogLevel > 10)
		sm_syslog(LOG_INFO, e->e_id,
			  "alias %.100s => %s",
			  a->q_paddr, shortenstring(p, MAXSHORTSTR));
	a->q_flags &= ~QSELFREF;
	if (tTd(27, 5))
	{
		sm_dprintf("alias: QS_EXPANDED ");
		printaddr(sm_debug_file(), a, false);
	}
	a->q_state = QS_EXPANDED;

	/*
	**  Always deliver aliased items as the default user.
	**  Setting q_gid to 0 forces deliver() to use DefUser
	**  instead of the alias name for the call to initgroups().
	*/

	a->q_uid = DefUid;
	a->q_gid = 0;
	a->q_fullname = NULL;
	a->q_flags |= QGOODUID|QALIAS;

	(void) sendtolist(p, a, sendq, aliaslevel + 1, e);

	if (bitset(QSELFREF, a->q_flags) && QS_IS_EXPANDED(a->q_state))
		a->q_state = QS_OK;

	/*
	**  Look for owner of alias
	*/

	if (strncmp(a->q_user, "owner-", 6) == 0 ||
	    strlen(a->q_user) > sizeof(obuf) - 7)
		(void) sm_strlcpy(obuf, "owner-owner", sizeof(obuf));
	else
		(void) sm_strlcpyn(obuf, sizeof(obuf), 2, "owner-", a->q_user);
	owner = aliaslookup(obuf, &status, a->q_host);
	if (owner == NULL)
		return;

	/* reflect owner into envelope sender */
	if (strpbrk(owner, ",:/|\"") != NULL)
		owner = obuf;
	a->q_owner = sm_rpool_strdup_x(e->e_rpool, owner);

	/* announce delivery to this alias; NORECEIPT bit set later */
	if (e->e_xfp != NULL)
		(void) sm_io_fprintf(e->e_xfp, SM_TIME_DEFAULT,
				"Message delivered to mailing list %s\n",
				a->q_paddr);
	e->e_flags |= EF_SENDRECEIPT;
	a->q_flags |= QDELIVERED|QEXPANDED;
}

/*
**  ALIASLOOKUP -- look up a name in the alias file.
**
**	Parameters:
**		name -- the name to look up [i]
**		pstat -- a pointer to a place to put the status.
**		av -- argument for %1 expansion.
**
**	Returns:
**		the value of name.
**		NULL if unknown.
**
**	Warnings:
**		The return value will be trashed across calls.
*/

static char *
aliaslookup(name, pstat, av)
	char *name;
	int *pstat;
	char *av;
{
	static MAP *map = NULL;
	char *res;
#if _FFR_ALIAS_DETAIL
	int i;
	char *argv[4];
#else
# define argv NULL
#endif
#if _FFR_8BITENVADDR
	char buf[MAXNAME];	/* EAI:ok */
#endif

	if (map == NULL)
	{
		STAB *s = stab("aliases", ST_MAP, ST_FIND);

		if (s == NULL)
			return NULL;
		map = &s->s_map;
	}
	DYNOPENMAP(map);

	/* special case POstMastER -- always use lower case */
	if (SM_STRCASEEQ(name, "postmaster"))
		name = "postmaster";
#if _FFR_8BITENVADDR
	(void) dequote_internal_chars(name, buf, sizeof(buf));
	/* check length? */
	name = buf;
#endif /* _FFR_8BITENVADDR */

#if _FFR_ALIAS_DETAIL
	i = 0;
	argv[i++] = name;
	argv[i++] = av;

	/* XXX '+' is hardwired here as delimiter! */
	if (av != NULL && *av == '+')
		argv[i++] = av + 1;
	argv[i++] = NULL;
#endif /* _FFR_ALIAS_DETAIL */
	res = (*map->map_class->map_lookup)(map, name, argv, pstat);
#if _FFR_8BITENVADDR
	/* map_lookup() does a map_rewrite(), so no quoting here */
#endif
	return res;
}

/*
**  SETALIAS -- set up an alias map
**
**	Called when reading configuration file.
**
**	Parameters:
**		spec -- the alias specification
**
**	Returns:
**		none.
*/

void
setalias(spec)
	char *spec;
{
	register char *p;
	register MAP *map;
	char *class;
	STAB *s;

	if (tTd(27, 8))
		sm_dprintf("setalias(%s)\n", spec);

	for (p = spec; p != NULL; )
	{
		char buf[50];

		while (SM_ISSPACE(*p))
			p++;
		if (*p == '\0')
			break;
		spec = p;

		if (NAliasFileMaps >= MAXMAPSTACK)
		{
			syserr("Too many alias databases defined, %d max",
				MAXMAPSTACK);
			return;
		}
		if (AliasFileMap == NULL)
		{
			(void) sm_strlcpy(buf, "aliases.files sequence",
					  sizeof(buf));
			AliasFileMap = makemapentry(buf);
			if (AliasFileMap == NULL)
			{
				syserr("setalias: cannot create aliases.files map");
				return;
			}
		}
		(void) sm_snprintf(buf, sizeof(buf), "Alias%d", NAliasFileMaps);
		s = stab(buf, ST_MAP, ST_ENTER);
		map = &s->s_map;
		memset(map, '\0', sizeof(*map));
		map->map_mname = s->s_name;
		p = strpbrk(p, ALIAS_SPEC_SEPARATORS);
		if (p != NULL && *p == SEPARATOR)
		{
			/* map name */
			*p++ = '\0';
			class = spec;
			spec = p;
		}
		else
		{
			class = "implicit";
			map->map_mflags = MF_INCLNULL;
		}

		/* find end of spec */
		if (p != NULL)
		{
			bool quoted = false;

			for (; *p != '\0'; p++)
			{
				/*
				**  Don't break into a quoted string.
				**  Needed for ldap maps which use
				**  commas in their specifications.
				*/

				if (*p == '"')
					quoted = !quoted;
				else if (*p == ',' && !quoted)
					break;
			}

			/* No more alias specifications follow */
			if (*p == '\0')
				p = NULL;
		}
		if (p != NULL)
			*p++ = '\0';

		if (tTd(27, 20))
			sm_dprintf("  map %s:%s %s\n", class, s->s_name, spec);

		/* look up class */
		s = stab(class, ST_MAPCLASS, ST_FIND);
		if (s == NULL)
		{
			syserr("setalias: unknown alias class %s", class);
		}
		else if (!bitset(MCF_ALIASOK, s->s_mapclass.map_cflags))
		{
			syserr("setalias: map class %s can't handle aliases",
				class);
		}
		else
		{
			map->map_class = &s->s_mapclass;
			map->map_mflags |= MF_ALIAS;
			if (map->map_class->map_parse(map, spec))
			{
				map->map_mflags |= MF_VALID;
				AliasFileMap->map_stack[NAliasFileMaps++] = map;
			}
		}
	}
}

/*
**  ALIASWAIT -- wait for distinguished @:@ token to appear.
**
**	This can decide to reopen the alias file
**
**	Parameters:
**		map -- a pointer to the map descriptor for this alias file.
**		ext -- the filename extension (e.g., ".db") for the
**			database file.
**		isopen -- if set, the database is already open, and we
**			should check for validity; otherwise, we are
**			just checking to see if it should be created.
**
**	Returns:
**		true -- if the database is open when we return.
**		false -- if the database is closed when we return.
*/

bool
aliaswait(map, ext, isopen)
	MAP *map;
	const char *ext;
	bool isopen;
{
	bool attimeout = false;
	time_t mtime;
	struct stat stb;
	char buf[MAXPATHLEN];

	if (tTd(27, 3))
		sm_dprintf("aliaswait(%s:%s), open=%d, wait=%d\n",
			   map->map_class->map_cname, map->map_file,
			   isopen, bitset(MF_ALIASWAIT, map->map_mflags));
	if (bitset(MF_ALIASWAIT, map->map_mflags))
		return isopen;
	map->map_mflags |= MF_ALIASWAIT;

	if (isopen && SafeAlias > 0)
	{
		auto int st;
		unsigned int sleeptime = 2;
		unsigned int loopcount = 0;	/* only used for debugging */
		time_t toolong = curtime() + SafeAlias;

		while (isopen &&
		       map->map_class->map_lookup(map, "@", NULL, &st) == NULL)
		{
			if (curtime() > toolong)
			{
				/* we timed out */
				attimeout = true;
				break;
			}

			/*
			**  Close and re-open the alias database in case
			**  the one is mv'ed instead of cp'ed in.
			*/

			if (tTd(27, 2))
			{
				loopcount++;
				sm_dprintf("aliaswait: sleeping for %u seconds (loopcount = %u)\n",
					   sleeptime, loopcount);
			}

			map->map_mflags |= MF_CLOSING;
			map->map_class->map_close(map);
			map->map_mflags &= ~(MF_OPEN|MF_WRITABLE|MF_CLOSING|MF_CHKED_CHGD);
			(void) sleep(sleeptime);
			sleeptime *= 2;
			if (sleeptime > 60)
				sleeptime = 60;
			isopen = map->map_class->map_open(map, O_RDONLY);
		}
	}
	map->map_mflags &= ~MF_CHKED_CHGD;

	/* see if we need to go into auto-rebuild mode */
	if (!bitset(MCF_REBUILDABLE, map->map_class->map_cflags))
	{
		if (tTd(27, 3))
			sm_dprintf("aliaswait: not rebuildable\n");
		map->map_mflags &= ~MF_ALIASWAIT;
		return isopen;
	}
	if (stat(map->map_file, &stb) < 0)
	{
		if (tTd(27, 3))
			sm_dprintf("aliaswait: no source file\n");
		map->map_mflags &= ~MF_ALIASWAIT;
		return isopen;
	}
	mtime = stb.st_mtime;
	if (sm_strlcpyn(buf, sizeof(buf), 2,
			map->map_file, ext == NULL ? "" : ext) >= sizeof(buf))
	{
		if (LogLevel > 3)
			sm_syslog(LOG_INFO, NOQID,
				  "alias database %s%s name too long",
				  map->map_file, ext == NULL ? "" : ext);
		message("alias database %s%s name too long",
			map->map_file, ext == NULL ? "" : ext);
	}

	if (stat(buf, &stb) < 0 || stb.st_mtime < mtime || attimeout)
	{
		if (LogLevel > 3)
			sm_syslog(LOG_INFO, NOQID,
				  "alias database %s out of date", buf);
		message("Warning: alias database %s out of date", buf);
	}
	map->map_mflags &= ~MF_ALIASWAIT;
	return isopen;
}
/*
**  REBUILDALIASES -- rebuild the alias database.
**
**	Parameters:
**		map -- the database to rebuild.
**
**	Returns:
**		true if successful; false otherwise.
**
**	Side Effects:
**		Reads the text version of the database, builds the map.
*/

bool
rebuildaliases(map)
	register MAP *map;
{
	SM_FILE_T *af;
	bool nolock = false;
	bool success = false;
	long sff = SFF_OPENASROOT|SFF_REGONLY|SFF_NOLOCK;
	sigfunc_t oldsigint, oldsigquit;
#ifdef SIGTSTP
	sigfunc_t oldsigtstp;
#endif

	if (!bitset(MCF_REBUILDABLE, map->map_class->map_cflags))
		return false;

	if (!bitnset(DBS_LINKEDALIASFILEINWRITABLEDIR, DontBlameSendmail))
		sff |= SFF_NOWLINK;
	if (!bitnset(DBS_GROUPWRITABLEALIASFILE, DontBlameSendmail))
		sff |= SFF_NOGWFILES;
	if (!bitnset(DBS_WORLDWRITABLEALIASFILE, DontBlameSendmail))
		sff |= SFF_NOWWFILES;

	/* try to lock the source file */
	if ((af = safefopen(map->map_file, O_RDWR, 0, sff)) == NULL)
	{
		struct stat stb;

		if ((errno != EACCES && errno != EROFS) ||
		    (af = safefopen(map->map_file, O_RDONLY, 0, sff)) == NULL)
		{
			int saveerr = errno;

			if (tTd(27, 1))
				sm_dprintf("Can't open %s: %s\n",
					map->map_file, sm_errstring(saveerr));
			if (!bitset(MF_OPTIONAL, map->map_mflags))
				message("newaliases: cannot open %s: %s",
					map->map_file, sm_errstring(saveerr));
			errno = 0;
			return false;
		}
		nolock = true;
		if (tTd(27, 1) ||
		    fstat(sm_io_getinfo(af, SM_IO_WHAT_FD, NULL), &stb) < 0 ||
		    bitset(S_IWUSR|S_IWGRP|S_IWOTH, stb.st_mode))
			message("warning: cannot lock %s: %s",
				map->map_file, sm_errstring(errno));
	}

	/* see if someone else is rebuilding the alias file */
	if (!nolock &&
	    !lockfile(sm_io_getinfo(af, SM_IO_WHAT_FD, NULL), map->map_file,
		      NULL, LOCK_EX|LOCK_NB))
	{
		/* yes, they are -- wait until done */
		message("Alias file %s is locked (maybe being rebuilt)",
			map->map_file);
		if (OpMode != MD_INITALIAS)
		{
			/* wait for other rebuild to complete */
			(void) lockfile(sm_io_getinfo(af, SM_IO_WHAT_FD, NULL),
					map->map_file, NULL, LOCK_EX);
		}
		(void) sm_io_close(af, SM_TIME_DEFAULT);
		errno = 0;
		return false;
	}

	oldsigint = sm_signal(SIGINT, SIG_IGN);
	oldsigquit = sm_signal(SIGQUIT, SIG_IGN);
#ifdef SIGTSTP
	oldsigtstp = sm_signal(SIGTSTP, SIG_IGN);
#endif

	if (map->map_class->map_open(map, O_RDWR))
	{
		if (LogLevel > 7)
		{
			sm_syslog(LOG_NOTICE, NOQID,
				"alias database %s rebuilt by %s",
				map->map_file, username());
		}
		map->map_mflags |= MF_OPEN|MF_WRITABLE;
		map->map_pid = CurrentPid;
		readaliases(map, af, true, true);
		success = true;
	}
	else
	{
		if (tTd(27, 1))
			sm_dprintf("Can't create database for %s: %s\n",
				map->map_file, sm_errstring(errno));
		syserr("Cannot create database for alias file %s",
			map->map_file);
	}

	/* close the file, thus releasing locks */
	(void) sm_io_close(af, SM_TIME_DEFAULT);

	/* add distinguished entries and close the database */
	if (bitset(MF_OPEN, map->map_mflags))
	{
#if _FFR_TESTS
		if (tTd(78, 101))
		{
			int sl;

			sl = tTdlevel(78) - 100;
			sm_dprintf("rebuildaliases: sleep=%d, file=%s\n",
				sl, map->map_file);
			sleep(sl);
			sm_dprintf("rebuildaliases: done\n");
		}
#endif
		map->map_mflags |= MF_CLOSING;
		map->map_class->map_close(map);
		map->map_mflags &= ~(MF_OPEN|MF_WRITABLE|MF_CLOSING);
	}

	/* restore the old signals */
	(void) sm_signal(SIGINT, oldsigint);
	(void) sm_signal(SIGQUIT, oldsigquit);
#ifdef SIGTSTP
	(void) sm_signal(SIGTSTP, oldsigtstp);
#endif
	return success;
}

/*
**  CONTLINE -- handle potential continuation line
**
**	Parameters:
**		fp -- file to read
**		line -- current line
**
**	Returns:
**		pointer to end of current line if there is a continuation line
**		NULL otherwise
**
**	Side Effects:
**		Modifies line if it is a continuation line
*/

static char *contline __P((SM_FILE_T *, char *));
static char *
contline(fp, line)
	SM_FILE_T *fp;
	char *line;
{
	char *p;
	int c;

	if ((p = strchr(line, '\n')) != NULL && p > line && p[-1] == '\\')
	{
		*p = '\0';
		*--p = '\0';
		return p;
	}

	c = sm_io_getc(fp, SM_TIME_DEFAULT);
	if (!sm_io_eof(fp))
		(void) sm_io_ungetc(fp, SM_TIME_DEFAULT, c);
	if (c == ' ' || c == '\t')
	{
		char *nlp;

		p = line;
		nlp = &p[strlen(p)];
		if (nlp > p && nlp[-1] == '\n')
			*--nlp = '\0';
		return nlp;
	}
	return NULL;
}

/*
**  READALIASES -- read and process the alias file.
**
**	This routine implements the part of initaliases that occurs
**	when we are not going to use the DBM stuff.
**
**	Parameters:
**		map -- the alias database descriptor.
**		af -- file to read the aliases from.
**		announcestats -- announce statistics regarding number of
**			aliases, longest alias, etc.
**		logstats -- lot the same info.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Reads aliasfile into the symbol table.
**		Optionally, builds the .dir & .pag files.
*/

void
readaliases(map, af, announcestats, logstats)
	register MAP *map;
	SM_FILE_T *af;
	bool announcestats;
	bool logstats;
{
	register char *p;
	char *rhs;
	bool skipping;
	long naliases, bytes, longest;
	ADDRESS al, bl;
	char lbuf[BUFSIZ];
	char *line;
#if _FFR_8BITENVADDR
	char lhsbuf[MAXNAME];	/* EAI:ok */
	char rhsbuf[BUFSIZ];
	int len;
#endif

	/*
	**  Read and interpret lines
	*/

	FileName = map->map_file;
	LineNumber = 0;
	naliases = bytes = longest = 0;
	skipping = false;
	line = NULL;

	while (sm_io_fgets(af, SM_TIME_DEFAULT, lbuf, sizeof(lbuf)) >= 0)
	{
		int lhssize, rhssize;
		int c;
		char *newp;

		LineNumber++;

		/* XXX what if line="a\\" ? */
		line = lbuf;
		p = line;
		while ((newp = contline(af, line)) != NULL)
		{
			p = newp;
			if ((c = sm_io_fgets(af, SM_TIME_DEFAULT, p,
					SPACELEFT(lbuf, p))) < 0)
			{
				break;
			}
			LineNumber++;
		}
#if _FFR_8BITENVADDR
		if (SMTP_UTF8 || EightBitAddrOK)
		{
			if (line != lbuf)
				SM_FREE(line);
			line = quote_internal_chars(lbuf, NULL, &len, NULL);
		}
		else
#endif
		/* "else" in #if code above */
		line = lbuf;

		p = strchr(line, '\n');
		if (p != NULL)
			*p = '\0';
		else if (!sm_io_eof(af))
		{
			int prev;
			bool cl;

			errno = 0;
			syserr("554 5.3.0 alias line too long");

			prev = '\0';
			cl = false;

			do {
				/* flush to end of "virtual" line */
				while ((c = sm_io_getc(af, SM_TIME_DEFAULT)) !=
					SM_IO_EOF && c != '\n')
				{
					prev = c;
				}
				cl = ('\\' == prev && '\n' == c);
				if (!cl)
				{
					c = sm_io_getc(af, SM_TIME_DEFAULT);
					if (!sm_io_eof(af))
						(void) sm_io_ungetc(af, SM_TIME_DEFAULT, c);
					cl = (c == ' ' || c == '\t');
				}
			} while (cl);

			continue;
		}

		switch (line[0])
		{
		  case '#':
		  case '\0':
			skipping = false;
			continue;

		  case ' ':
		  case '\t':
			if (!skipping)
				syserr("554 5.3.5 Non-continuation line starts with space");
			skipping = true;
			continue;
		}
		skipping = false;

		/*
		**  Process the LHS
		**	Find the colon separator, and parse the address.
		**	It should resolve to a local name -- this will
		**	be checked later (we want to optionally do
		**	parsing of the RHS first to maximize error
		**	detection).
		*/

		for (p = line; *p != '\0' && *p != ':' && *p != '\n'; p++)
			continue;
		if (*p++ != ':')
		{
			syserr("554 5.3.5 missing colon");
			continue;
		}
/* XXX line must be [i] */
		if (parseaddr(line, &al, RF_COPYALL, ':', NULL, CurEnv, true)
		    == NULL)
		{
			syserr("554 5.3.5 %.40s... illegal alias name", line);
			continue;
		}

		/*
		**  Process the RHS.
		**	'al' is the internal form of the LHS address.
		**	'p' points to the text of the RHS.
		*/

		while (SM_ISSPACE(*p))
			p++;
		rhs = p;
		{
			register char *nlp;

			nlp = &p[strlen(p)];
			if (nlp > p && nlp[-1] == '\n')
				*--nlp = '\0';

			if (CheckAliases)
			{
				/* do parsing & compression of addresses */
				while (*p != '\0')
				{
					auto char *delimptr;

					while ((SM_ISSPACE(*p)) || *p == ',')
						p++;
					if (*p == '\0')
						break;
/* XXX p must be [i] */
					if (parseaddr(p, &bl, RF_COPYNONE, ',',
						      &delimptr, CurEnv, true)
					    == NULL)
						usrerr("553 5.3.5 %s... bad address", p);
					p = delimptr;
				}
			}
			else
			{
				p = nlp;
			}
		} while (0);

		if (skipping)
			continue;

		if (!bitnset(M_ALIASABLE, al.q_mailer->m_flags))
		{
			syserr("554 5.3.5 %s... cannot alias non-local names",
				al.q_paddr);
			continue;
		}

		/*
		**  Insert alias into symbol table or database file.
		**
		**	Special case pOStmaStER -- always make it lower case.
		*/

		if (SM_STRCASEEQ(al.q_user, "postmaster"))
			makelower_a(&al.q_user, CurEnv->e_rpool);

		lhssize = strlen(al.q_user);
		rhssize = strlen(rhs);
		if (rhssize > 0)
		{
			/* is RHS empty (just spaces)? */
			p = rhs;
			while (SM_ISSPACE(*p))
				p++;
		}
		if (rhssize == 0 || *p == '\0')
		{
			syserr("554 5.3.5 %.40s... missing value for alias",
			       line);
		}
		else
		{
#if _FFR_8BITENVADDR
			if (SMTP_UTF8 || EightBitAddrOK)
			{
				dequote_internal_chars(al.q_user, lhsbuf, sizeof(lhsbuf));
				dequote_internal_chars(rhs, rhsbuf, sizeof(rhsbuf));
				map->map_class->map_store(map, lhsbuf, rhsbuf);
			}
			else
#endif
			/* "else" in #if code above */
			map->map_class->map_store(map, al.q_user, rhs);

			/* statistics */
			naliases++;
			bytes += lhssize + rhssize;
			if (rhssize > longest)
				longest = rhssize;
		}
	}

	CurEnv->e_to = NULL;
	FileName = NULL;
	if (Verbose || announcestats)
		message("%s: %ld aliases, longest %ld bytes, %ld bytes total",
			map->map_file, naliases, longest, bytes);
	if (LogLevel > 7 && logstats)
		sm_syslog(LOG_INFO, NOQID,
			"%s: %ld aliases, longest %ld bytes, %ld bytes total",
			map->map_file, naliases, longest, bytes);
}
/*
**  FORWARD -- Try to forward mail
**
**	This is similar but not identical to aliasing.
**
**	Parameters:
**		user -- the name of the user who's mail we would like
**			to forward to.  It must have been verified --
**			i.e., the q_home field must have been filled in.
**		sendq -- a pointer to the head of the send queue to
**			put this user's aliases in.
**		aliaslevel -- the current alias nesting depth.
**		e -- the current envelope.
**
**	Returns:
**		none.
**
**	Side Effects:
**		New names are added to send queues.
*/

void
forward(user, sendq, aliaslevel, e)
	ADDRESS *user;
	ADDRESS **sendq;
	int aliaslevel;
	register ENVELOPE *e;
{
	char *pp;
	char *ep;
	bool got_transient;

	if (tTd(27, 1))
		sm_dprintf("forward(%s)\n", user->q_paddr);

	if (!bitnset(M_HASPWENT, user->q_mailer->m_flags) ||
	    !QS_IS_OK(user->q_state))
		return;
	if (ForwardPath != NULL && *ForwardPath == '\0')
		return;
	if (user->q_home == NULL)
	{
		syserr("554 5.3.0 forward: no home");
		user->q_home = "/no/such/directory";
	}

	/* good address -- look for .forward file in home */
	macdefine(&e->e_macro, A_PERM, 'z', user->q_home);
	macdefine(&e->e_macro, A_PERM, 'u', user->q_user);
	pp = user->q_host;
#if _FFR_8BITENVADDR
	if (NULL != pp)
	{
		int len;

		pp = quote_internal_chars(pp, NULL, &len, NULL);
	}
#endif
	macdefine(&e->e_macro, A_PERM, 'h', pp);
	if (ForwardPath == NULL)
		ForwardPath = newstr("\201z/.forward");

	got_transient = false;
	for (pp = ForwardPath; pp != NULL; pp = ep)
	{
		int err;
		char buf[MAXPATHLEN];
		struct stat st;

		ep = strchr(pp, SEPARATOR);
		if (ep != NULL)
			*ep = '\0';
		expand(pp, buf, sizeof(buf), e);
		if (ep != NULL)
			*ep++ = SEPARATOR;
		if (buf[0] == '\0')
			continue;
		if (tTd(27, 3))
			sm_dprintf("forward: trying %s\n", buf);

		err = include(buf, true, user, sendq, aliaslevel, e);
		if (err == 0)
			break;
		else if (transienterror(err))
		{
			/* we may have to suspend this message */
			got_transient = true;
			if (tTd(27, 2))
				sm_dprintf("forward: transient error on %s\n",
					   buf);
			if (LogLevel > 2)
			{
				char *curhost = CurHostName;

				CurHostName = NULL;
				sm_syslog(LOG_ERR, e->e_id,
					  "forward %s: transient error: %s",
					  buf, sm_errstring(err));
				CurHostName = curhost;
			}

		}
		else
		{
			switch (err)
			{
			  case ENOENT:
				break;

			  case E_SM_WWDIR:
			  case E_SM_GWDIR:
				/* check if it even exists */
				if (stat(buf, &st) < 0 && errno == ENOENT)
				{
					if (bitnset(DBS_DONTWARNFORWARDFILEINUNSAFEDIRPATH,
						    DontBlameSendmail))
						break;
				}
				/* FALLTHROUGH */

#if _FFR_FORWARD_SYSERR
			  case E_SM_NOSLINK:
			  case E_SM_NOHLINK:
			  case E_SM_REGONLY:
			  case E_SM_ISEXEC:
			  case E_SM_WWFILE:
			  case E_SM_GWFILE:
				syserr("forward: %s: %s", buf, sm_errstring(err));
				break;
#endif /* _FFR_FORWARD_SYSERR */

			  default:
				if (LogLevel > (RunAsUid == 0 ? 2 : 10))
					sm_syslog(LOG_WARNING, e->e_id,
						  "forward %s: %s", buf,
						  sm_errstring(err));
				if (Verbose)
					message("forward: %s: %s",
						buf, sm_errstring(err));
				break;
			}
		}
	}
	if (pp == NULL && got_transient)
	{
		/*
		**  There was no successful .forward open and at least one
		**  transient open.  We have to defer this address for
		**  further delivery.
		*/

		message("transient .forward open error: message queued");
		user->q_state = QS_QUEUEUP;
		return;
	}
}
