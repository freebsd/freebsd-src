/*
 * Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
 * Copyright (c) 1983, 1995-1997 Eric P. Allman.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 */

# include "sendmail.h"

#ifndef lint
static char sccsid[] = "@(#)alias.c	8.92 (Berkeley) 6/5/98";
#endif /* not lint */


MAP	*AliasFileMap = NULL;	/* the actual aliases.files map */
int	NAliasFileMaps;		/* the number of entries in AliasFileMap */
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
	auto int stat = EX_OK;
	char obuf[MAXNAME + 7];
	extern char *aliaslookup __P((char *, int *, ENVELOPE *));

	if (tTd(27, 1))
		printf("alias(%s)\n", a->q_user);

	/* don't realias already aliased names */
	if (bitset(QDONTSEND|QBADADDR|QVERIFIED, a->q_flags))
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

	p = aliaslookup(a->q_user, &stat, e);
	if (stat == EX_TEMPFAIL || stat == EX_UNAVAILABLE)
	{
		a->q_flags |= QQUEUEUP;
		if (e->e_message == NULL)
			e->e_message = newstr("alias database unavailable");
		return;
	}
	if (p == NULL)
		return;

	/*
	**  Match on Alias.
	**	Deliver to the target list.
	*/

	if (tTd(27, 1))
		printf("%s (%s, %s) aliased to %s\n",
		    a->q_paddr, a->q_host, a->q_user, p);
	if (bitset(EF_VRFYONLY, e->e_flags))
	{
		a->q_flags |= QVERIFIED;
		return;
	}
	message("aliased to %s", shortenstring(p, MAXSHORTSTR));
	if (LogLevel > 9)
		sm_syslog(LOG_INFO, e->e_id,
			"alias %.100s => %s",
			a->q_paddr, shortenstring(p, MAXSHORTSTR));
	a->q_flags &= ~QSELFREF;
	if (tTd(27, 5))
	{
		printf("alias: QDONTSEND ");
		printaddr(a, FALSE);
	}
	a->q_flags |= QDONTSEND;
	(void) sendtolist(p, a, sendq, aliaslevel + 1, e);
	if (bitset(QSELFREF, a->q_flags))
		a->q_flags &= ~QDONTSEND;

	/*
	**  Look for owner of alias
	*/

	(void) strcpy(obuf, "owner-");
	if (strncmp(a->q_user, "owner-", 6) == 0 ||
	    strlen(a->q_user) > (SIZE_T) sizeof obuf - 7)
		(void) strcat(obuf, "owner");
	else
		(void) strcat(obuf, a->q_user);
	owner = aliaslookup(obuf, &stat, e);
	if (owner == NULL)
		return;

	/* reflect owner into envelope sender */
	if (strpbrk(owner, ",:/|\"") != NULL)
		owner = obuf;
	a->q_owner = newstr(owner);

	/* announce delivery to this alias; NORECEIPT bit set later */
	if (e->e_xfp != NULL)
		fprintf(e->e_xfp, "Message delivered to mailing list %s\n",
			a->q_paddr);
	e->e_flags |= EF_SENDRECEIPT;
	a->q_flags |= QDELIVERED|QEXPANDED;
}
/*
**  ALIASLOOKUP -- look up a name in the alias file.
**
**	Parameters:
**		name -- the name to look up.
**		pstat -- a pointer to a place to put the status.
**		e -- the current envelope.
**
**	Returns:
**		the value of name.
**		NULL if unknown.
**
**	Side Effects:
**		none.
**
**	Warnings:
**		The return value will be trashed across calls.
*/

char *
aliaslookup(name, pstat, e)
	char *name;
	int *pstat;
	ENVELOPE *e;
{
	static MAP *map = NULL;

	if (map == NULL)
	{
		STAB *s = stab("aliases", ST_MAP, ST_FIND);

		if (s == NULL)
			return NULL;
		map = &s->s_map;
	}
	if (!bitset(MF_OPEN, map->map_mflags))
		return NULL;

	/* special case POstMastER -- always use lower case */
	if (strcasecmp(name, "postmaster") == 0)
		name = "postmaster";

	return (*map->map_class->map_lookup)(map, name, NULL, pstat);
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
		printf("setalias(%s)\n", spec);

	for (p = spec; p != NULL; )
	{
		char buf[50];

		while (isascii(*p) && isspace(*p))
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
			strcpy(buf, "aliases.files sequence");
			AliasFileMap = makemapentry(buf);
			if (AliasFileMap == NULL)
			{
				syserr("setalias: cannot create aliases.files map");
				return;
			}
		}
		(void) snprintf(buf, sizeof buf, "Alias%d", NAliasFileMaps);
		s = stab(buf, ST_MAP, ST_ENTER);
		map = &s->s_map;
		bzero(map, sizeof *map);
		map->map_mname = s->s_name;

		p = strpbrk(p, " ,/:");
		if (p != NULL && *p == ':')
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
			p = strchr(p, ',');
		if (p != NULL)
			*p++ = '\0';

		if (tTd(27, 20))
			printf("  map %s:%s %s\n", class, s->s_name, spec);

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
			if (map->map_class->map_parse(map, spec))
			{
				map->map_mflags |= MF_VALID|MF_ALIAS;
				AliasFileMap->map_stack[NAliasFileMaps++] = map;
			}
		}
	}
}
/*
**  ALIASWAIT -- wait for distinguished @:@ token to appear.
**
**	This can decide to reopen or rebuild the alias file
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
**		TRUE -- if the database is open when we return.
**		FALSE -- if the database is closed when we return.
*/

bool
aliaswait(map, ext, isopen)
	MAP *map;
	char *ext;
	int isopen;
{
	bool attimeout = FALSE;
	time_t mtime;
	struct stat stb;
	char buf[MAXNAME + 1];

	if (tTd(27, 3))
		printf("aliaswait(%s:%s)\n",
			map->map_class->map_cname, map->map_file);
	if (bitset(MF_ALIASWAIT, map->map_mflags))
		return isopen;
	map->map_mflags |= MF_ALIASWAIT;

	if (SafeAlias > 0)
	{
		auto int st;
		time_t toolong = curtime() + SafeAlias;
		unsigned int sleeptime = 2;

		while (isopen &&
		       map->map_class->map_lookup(map, "@", NULL, &st) == NULL)
		{
			if (curtime() > toolong)
			{
				/* we timed out */
				attimeout = TRUE;
				break;
			}

			/*
			**  Close and re-open the alias database in case
			**  the one is mv'ed instead of cp'ed in.
			*/

			if (tTd(27, 2))
				printf("aliaswait: sleeping for %d seconds\n",
					sleeptime);

			map->map_class->map_close(map);
			map->map_mflags &= ~(MF_OPEN|MF_WRITABLE);
			sleep(sleeptime);
			sleeptime *= 2;
			if (sleeptime > 60)
				sleeptime = 60;
			isopen = map->map_class->map_open(map, O_RDONLY);
		}
	}

	/* see if we need to go into auto-rebuild mode */
	if (!bitset(MCF_REBUILDABLE, map->map_class->map_cflags))
	{
		if (tTd(27, 3))
			printf("aliaswait: not rebuildable\n");
		map->map_mflags &= ~MF_ALIASWAIT;
		return isopen;
	}
	if (stat(map->map_file, &stb) < 0)
	{
		if (tTd(27, 3))
			printf("aliaswait: no source file\n");
		map->map_mflags &= ~MF_ALIASWAIT;
		return isopen;
	}
	mtime = stb.st_mtime;
	snprintf(buf, sizeof buf, "%s%s",
		map->map_file, ext == NULL ? "" : ext);
	if (stat(buf, &stb) < 0 || stb.st_mtime < mtime || attimeout)
	{
		/* database is out of date */
		if (AutoRebuild && stb.st_ino != 0 &&
		    (stb.st_uid == geteuid() ||
		     (geteuid() == 0 && stb.st_uid == TrustedFileUid)))
		{
			bool oldSuprErrs;

			message("auto-rebuilding alias database %s", buf);
			oldSuprErrs = SuprErrs;
			SuprErrs = TRUE;
			if (isopen)
			{
				map->map_class->map_close(map);
				map->map_mflags &= ~(MF_OPEN|MF_WRITABLE);
			}
			(void) rebuildaliases(map, TRUE);
			isopen = map->map_class->map_open(map, O_RDONLY);
			SuprErrs = oldSuprErrs;
		}
		else
		{
			if (LogLevel > 3)
				sm_syslog(LOG_INFO, NOQID,
					"alias database %s out of date",
					buf);
			message("Warning: alias database %s out of date", buf);
		}
	}
	map->map_mflags &= ~MF_ALIASWAIT;
	return isopen;
}
/*
**  REBUILDALIASES -- rebuild the alias database.
**
**	Parameters:
**		map -- the database to rebuild.
**		automatic -- set if this was automatically generated.
**
**	Returns:
**		TRUE if successful; FALSE otherwise.
**
**	Side Effects:
**		Reads the text version of the database, builds the
**		DBM or DB version.
*/

bool
rebuildaliases(map, automatic)
	register MAP *map;
	bool automatic;
{
	FILE *af;
	bool nolock = FALSE;
	bool success = FALSE;
	int sff = SFF_OPENASROOT|SFF_REGONLY|SFF_NOLOCK;	
	sigfunc_t oldsigint, oldsigquit;
#ifdef SIGTSTP
	sigfunc_t oldsigtstp;
#endif

	if (!bitset(MCF_REBUILDABLE, map->map_class->map_cflags))
		return FALSE;

	if (!bitset(DBS_LINKEDALIASFILEINWRITABLEDIR, DontBlameSendmail))
		sff |= SFF_NOWLINK;
	if (!bitset(DBS_GROUPWRITABLEALIASFILE, DontBlameSendmail))
		sff |= SFF_NOGWFILES;
	if (!bitset(DBS_WORLDWRITABLEALIASFILE, DontBlameSendmail))
		sff |= SFF_NOWWFILES;

	/* try to lock the source file */
	if ((af = safefopen(map->map_file, O_RDWR, 0, sff)) == NULL)
	{
		struct stat stb;

		if ((errno != EACCES && errno != EROFS) || automatic ||
		    (af = safefopen(map->map_file, O_RDONLY, 0, sff)) == NULL)
		{
			int saveerr = errno;

			if (tTd(27, 1))
				printf("Can't open %s: %s\n",
					map->map_file, errstring(saveerr));
			if (!automatic && !bitset(MF_OPTIONAL, map->map_mflags))
				message("newaliases: cannot open %s: %s",
					map->map_file, errstring(saveerr));
			errno = 0;
			return FALSE;
		}
		nolock = TRUE;
		if (tTd(27, 1) ||
		    fstat(fileno(af), &stb) < 0 ||
		    bitset(S_IWUSR|S_IWGRP|S_IWOTH, stb.st_mode))
			message("warning: cannot lock %s: %s",
				map->map_file, errstring(errno));
	}

	/* see if someone else is rebuilding the alias file */
	if (!nolock &&
	    !lockfile(fileno(af), map->map_file, NULL, LOCK_EX|LOCK_NB))
	{
		/* yes, they are -- wait until done */
		message("Alias file %s is locked (maybe being rebuilt)",
			map->map_file);
		if (OpMode != MD_INITALIAS)
		{
			/* wait for other rebuild to complete */
			(void) lockfile(fileno(af), map->map_file, NULL,
					LOCK_EX);
		}
		(void) xfclose(af, "rebuildaliases1", map->map_file);
		errno = 0;
		return FALSE;
	}

	oldsigint = setsignal(SIGINT, SIG_IGN);
	oldsigquit = setsignal(SIGQUIT, SIG_IGN);
#ifdef SIGTSTP
	oldsigtstp = setsignal(SIGTSTP, SIG_IGN);
#endif

	if (map->map_class->map_open(map, O_RDWR))
	{
		if (LogLevel > 7)
		{
			sm_syslog(LOG_NOTICE, NOQID,
				"alias database %s %srebuilt by %s",
				map->map_file, automatic ? "auto" : "",
				username());
		}
		map->map_mflags |= MF_OPEN|MF_WRITABLE;
		readaliases(map, af, !automatic, TRUE);
		success = TRUE;
	}
	else
	{
		if (tTd(27, 1))
			printf("Can't create database for %s: %s\n",
				map->map_file, errstring(errno));
		if (!automatic)
			syserr("Cannot create database for alias file %s",
				map->map_file);
	}

	/* close the file, thus releasing locks */
	xfclose(af, "rebuildaliases2", map->map_file);

	/* add distinguished entries and close the database */
	if (bitset(MF_OPEN, map->map_mflags))
	{
		map->map_class->map_close(map);
		map->map_mflags &= ~(MF_OPEN|MF_WRITABLE);
	}

	/* restore the old signals */
	(void) setsignal(SIGINT, oldsigint);
	(void) setsignal(SIGQUIT, oldsigquit);
#ifdef SIGTSTP
	(void) setsignal(SIGTSTP, oldsigtstp);
#endif
	return success;
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
**		announcestats -- anounce statistics regarding number of
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
	FILE *af;
	bool announcestats;
	bool logstats;
{
	register char *p;
	char *rhs;
	bool skipping;
	long naliases, bytes, longest;
	ADDRESS al, bl;
	char line[BUFSIZ];

	/*
	**  Read and interpret lines
	*/

	FileName = map->map_file;
	LineNumber = 0;
	naliases = bytes = longest = 0;
	skipping = FALSE;
	while (fgets(line, sizeof (line), af) != NULL)
	{
		int lhssize, rhssize;
		int c;

		LineNumber++;
		p = strchr(line, '\n');
#if _FFR_BACKSLASH_IN_ALIASES
		while (p != NULL && p > line && p[-1] == '\\')
		{
			p--;
			if (fgets(p, SPACELEFT(line, p), af) == NULL)
				break;
			LineNumber++;
			p = strchr(p, '\n');
		}
#endif
		if (p != NULL)
			*p = '\0';
		else if (!feof(af))
		{
			syserr("554 alias line too long");

			/* flush to end of line */
			while ((c = getc(af)) != EOF && c != '\n')
				continue;

			/* skip any continuation lines */
			skipping = TRUE;
			continue;
		}
		switch (line[0])
		{
		  case '#':
		  case '\0':
			skipping = FALSE;
			continue;

		  case ' ':
		  case '\t':
			if (!skipping)
				syserr("554 Non-continuation line starts with space");
			skipping = TRUE;
			continue;
		}
		skipping = FALSE;

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
			syserr("554 missing colon");
			continue;
		}
		if (parseaddr(line, &al, RF_COPYALL, ':', NULL, CurEnv) == NULL)
		{
			syserr("554 %.40s... illegal alias name", line);
			continue;
		}

		/*
		**  Process the RHS.
		**	'al' is the internal form of the LHS address.
		**	'p' points to the text of the RHS.
		*/

		while (isascii(*p) && isspace(*p))
			p++;
		rhs = p;
		for (;;)
		{
			register char *nlp;

			nlp = &p[strlen(p)];
			if (nlp[-1] == '\n')
				*--nlp = '\0';

			if (CheckAliases)
			{
				/* do parsing & compression of addresses */
				while (*p != '\0')
				{
					auto char *delimptr;

					while ((isascii(*p) && isspace(*p)) ||
								*p == ',')
						p++;
					if (*p == '\0')
						break;
					if (parseaddr(p, &bl, RF_COPYNONE, ',',
						      &delimptr, CurEnv) == NULL)
						usrerr("553 %s... bad address", p);
					p = delimptr;
				}
			}
			else
			{
				p = nlp;
			}

			/* see if there should be a continuation line */
			c = getc(af);
			if (!feof(af))
				(void) ungetc(c, af);
			if (c != ' ' && c != '\t')
				break;

			/* read continuation line */
			if (fgets(p, sizeof line - (p - line), af) == NULL)
				break;
			LineNumber++;

			/* check for line overflow */
			if (strchr(p, '\n') == NULL && !feof(af))
			{
				usrerr("554 alias too long");
				while ((c = fgetc(af)) != EOF && c != '\n')
					continue;
				skipping = TRUE;
				break;
			}
		}

		if (skipping)
			continue;

		if (!bitnset(M_ALIASABLE, al.q_mailer->m_flags))
		{
			syserr("554 %s... cannot alias non-local names",
				al.q_paddr);
			continue;
		}

		/*
		**  Insert alias into symbol table or database file.
		**
		**	Special case pOStmaStER -- always make it lower case.
		*/

		if (strcasecmp(al.q_user, "postmaster") == 0)
			makelower(al.q_user);

		lhssize = strlen(al.q_user);
		rhssize = strlen(rhs);
		map->map_class->map_store(map, al.q_user, rhs);

		if (al.q_paddr != NULL)
			free(al.q_paddr);
		if (al.q_host != NULL)
			free(al.q_host);
		if (al.q_user != NULL)
			free(al.q_user);

		/* statistics */
		naliases++;
		bytes += lhssize + rhssize;
		if (rhssize > longest)
			longest = rhssize;
	}

	CurEnv->e_to = NULL;
	FileName = NULL;
	if (Verbose || announcestats)
		message("%s: %d aliases, longest %d bytes, %d bytes total",
			map->map_file, naliases, longest, bytes);
	if (LogLevel > 7 && logstats)
		sm_syslog(LOG_INFO, NOQID,
			"%s: %d aliases, longest %d bytes, %d bytes total",
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
**			i.e., the q_home field must have been filled
**			in.
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
		printf("forward(%s)\n", user->q_paddr);

	if (!bitnset(M_HASPWENT, user->q_mailer->m_flags) ||
	    bitset(QBADADDR, user->q_flags))
		return;
	if (user->q_home == NULL)
	{
		syserr("554 forward: no home");
		user->q_home = "/no/such/directory";
	}

	/* good address -- look for .forward file in home */
	define('z', user->q_home, e);
	define('u', user->q_user, e);
	define('h', user->q_host, e);
	if (ForwardPath == NULL)
		ForwardPath = newstr("\201z/.forward");

	got_transient = FALSE;
	for (pp = ForwardPath; pp != NULL; pp = ep)
	{
		int err;
		char buf[MAXPATHLEN+1];

		ep = strchr(pp, ':');
		if (ep != NULL)
			*ep = '\0';
		expand(pp, buf, sizeof buf, e);
		if (ep != NULL)
			*ep++ = ':';
		if (buf[0] == '\0')
			continue;
		if (tTd(27, 3))
			printf("forward: trying %s\n", buf);

		err = include(buf, TRUE, user, sendq, aliaslevel, e);
		if (err == 0)
			break;
		else if (transienterror(err))
		{
			/* we may have to suspend this message */
			got_transient = TRUE;
			if (tTd(27, 2))
				printf("forward: transient error on %s\n", buf);
			if (LogLevel > 2)
				sm_syslog(LOG_ERR, e->e_id,
					"forward %s: transient error: %s",
					buf, errstring(err));
		}
		else
		{
			switch (err)
			{
			  case ENOENT:
				break;

#if _FFR_FORWARD_SYSERR
			  case E_SM_NOSLINK:
			  case E_SM_NOHLINK:
			  case E_SM_REGONLY:
			  case E_SM_ISEXEC:
			  case E_SM_WWDIR:
			  case E_SM_GWDIR:
			  case E_SM_WWFILE:
			  case E_SM_GWFILE:
				syserr("forward: %s: %s", buf, errstring(err));
				break;
#endif

			  default:
				if (LogLevel > (RunAsUid == 0 ? 2 : 10))
					sm_syslog(LOG_WARNING, e->e_id,
						"forward %s: %s", buf,
						errstring(err));
				if (Verbose)
					message("forward: %s: %s",
						buf,
						errstring(err));
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
		user->q_flags |= QQUEUEUP;
		return;
	}
}
