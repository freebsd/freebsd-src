/*
 * Copyright (c) 1998-2000 Sendmail, Inc. and its suppliers.
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

#ifndef lint
static char id[] = "@(#)$Id: readcf.c,v 8.382.4.14 2000/07/12 00:00:27 geir Exp $";
#endif /* ! lint */

#include <sendmail.h>


#if NETINET || NETINET6
# include <arpa/inet.h>
#endif /* NETINET || NETINET6 */

#define SECONDS
#define MINUTES	* 60
#define HOUR	* 3600
#define HOURS	HOUR

static void	fileclass __P((int, char *, char *, bool, bool));
static char	**makeargv __P((char *));
static void	settimeout __P((char *, char *, bool));
static void	toomany __P((int, int));

/*
**  READCF -- read configuration file.
**
**	This routine reads the configuration file and builds the internal
**	form.
**
**	The file is formatted as a sequence of lines, each taken
**	atomically.  The first character of each line describes how
**	the line is to be interpreted.  The lines are:
**		Dxval		Define macro x to have value val.
**		Cxword		Put word into class x.
**		Fxfile [fmt]	Read file for lines to put into
**				class x.  Use scanf string 'fmt'
**				or "%s" if not present.  Fmt should
**				only produce one string-valued result.
**		Hname: value	Define header with field-name 'name'
**				and value as specified; this will be
**				macro expanded immediately before
**				use.
**		Sn		Use rewriting set n.
**		Rlhs rhs	Rewrite addresses that match lhs to
**				be rhs.
**		Mn arg=val...	Define mailer.  n is the internal name.
**				Args specify mailer parameters.
**		Oxvalue		Set option x to value.
**		Pname=value	Set precedence name to value.
**		Vversioncode[/vendorcode]
**				Version level/vendor name of
**				configuration syntax.
**		Kmapname mapclass arguments....
**				Define keyed lookup of a given class.
**				Arguments are class dependent.
**		Eenvar=value	Set the environment value to the given value.
**
**	Parameters:
**		cfname -- configuration file name.
**		safe -- TRUE if this is the system config file;
**			FALSE otherwise.
**		e -- the main envelope.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Builds several internal tables.
*/

void
readcf(cfname, safe, e)
	char *cfname;
	bool safe;
	register ENVELOPE *e;
{
	FILE *cf;
	int ruleset = -1;
	char *q;
	struct rewrite *rwp = NULL;
	char *bp;
	auto char *ep;
	int nfuzzy;
	char *file;
	bool optional;
	int mid;
	register char *p;
	long sff = SFF_OPENASROOT;
	struct stat statb;
	char buf[MAXLINE];
	char exbuf[MAXLINE];
	char pvpbuf[MAXLINE + MAXATOM];
	static char *null_list[1] = { NULL };
	extern u_char TokTypeNoC[];

	FileName = cfname;
	LineNumber = 0;

	if (DontLockReadFiles)
		sff |= SFF_NOLOCK;
	cf = safefopen(cfname, O_RDONLY, 0444, sff);
	if (cf == NULL)
	{
		syserr("cannot open");
		finis(FALSE, EX_OSFILE);
	}

	if (fstat(fileno(cf), &statb) < 0)
	{
		syserr("cannot fstat");
		finis(FALSE, EX_OSFILE);
	}

	if (!S_ISREG(statb.st_mode))
	{
		syserr("not a plain file");
		finis(FALSE, EX_OSFILE);
	}

	if (OpMode != MD_TEST && bitset(S_IWGRP|S_IWOTH, statb.st_mode))
	{
		if (OpMode == MD_DAEMON || OpMode == MD_INITALIAS)
			fprintf(stderr, "%s: WARNING: dangerous write permissions\n",
				FileName);
		if (LogLevel > 0)
			sm_syslog(LOG_CRIT, NOQID,
				  "%s: WARNING: dangerous write permissions",
				  FileName);
	}

#ifdef XLA
	xla_zero();
#endif /* XLA */

	while ((bp = fgetfolded(buf, sizeof buf, cf)) != NULL)
	{
		if (bp[0] == '#')
		{
			if (bp != buf)
				free(bp);
			continue;
		}

		/* do macro expansion mappings */
		translate_dollars(bp);

		/* interpret this line */
		errno = 0;
		switch (bp[0])
		{
		  case '\0':
		  case '#':		/* comment */
			break;

		  case 'R':		/* rewriting rule */
			if (ruleset < 0)
			{
				syserr("missing valid ruleset for \"%s\"", bp);
				break;
			}
			for (p = &bp[1]; *p != '\0' && *p != '\t'; p++)
				continue;

			if (*p == '\0')
			{
				syserr("invalid rewrite line \"%s\" (tab expected)", bp);
				break;
			}

			/* allocate space for the rule header */
			if (rwp == NULL)
			{
				RewriteRules[ruleset] = rwp =
					(struct rewrite *) xalloc(sizeof *rwp);
			}
			else
			{
				rwp->r_next = (struct rewrite *) xalloc(sizeof *rwp);
				rwp = rwp->r_next;
			}
			rwp->r_next = NULL;

			/* expand and save the LHS */
			*p = '\0';
			expand(&bp[1], exbuf, sizeof exbuf, e);
			rwp->r_lhs = prescan(exbuf, '\t', pvpbuf,
					     sizeof pvpbuf, NULL,
					     ConfigLevel >= 9 ? TokTypeNoC : NULL);
			nfuzzy = 0;
			if (rwp->r_lhs != NULL)
			{
				register char **ap;

				rwp->r_lhs = copyplist(rwp->r_lhs, TRUE);

				/* count the number of fuzzy matches in LHS */
				for (ap = rwp->r_lhs; *ap != NULL; ap++)
				{
					char *botch;

					botch = NULL;
					switch (**ap & 0377)
					{
					  case MATCHZANY:
					  case MATCHANY:
					  case MATCHONE:
					  case MATCHCLASS:
					  case MATCHNCLASS:
						nfuzzy++;
						break;

					  case MATCHREPL:
						botch = "$0-$9";
						break;

					  case CANONUSER:
						botch = "$:";
						break;

					  case CALLSUBR:
						botch = "$>";
						break;

					  case CONDIF:
						botch = "$?";
						break;

					  case CONDFI:
						botch = "$.";
						break;

					  case HOSTBEGIN:
						botch = "$[";
						break;

					  case HOSTEND:
						botch = "$]";
						break;

					  case LOOKUPBEGIN:
						botch = "$(";
						break;

					  case LOOKUPEND:
						botch = "$)";
						break;
					}
					if (botch != NULL)
						syserr("Inappropriate use of %s on LHS",
							botch);
				}
				rwp->r_line = LineNumber;
			}
			else
			{
				syserr("R line: null LHS");
				rwp->r_lhs = null_list;
			}

			/* expand and save the RHS */
			while (*++p == '\t')
				continue;
			q = p;
			while (*p != '\0' && *p != '\t')
				p++;
			*p = '\0';
			expand(q, exbuf, sizeof exbuf, e);
			rwp->r_rhs = prescan(exbuf, '\t', pvpbuf,
					     sizeof pvpbuf, NULL,
					     ConfigLevel >= 9 ? TokTypeNoC : NULL);
			if (rwp->r_rhs != NULL)
			{
				register char **ap;

				rwp->r_rhs = copyplist(rwp->r_rhs, TRUE);

				/* check no out-of-bounds replacements */
				nfuzzy += '0';
				for (ap = rwp->r_rhs; *ap != NULL; ap++)
				{
					char *botch;

					botch = NULL;
					switch (**ap & 0377)
					{
					  case MATCHREPL:
						if ((*ap)[1] <= '0' || (*ap)[1] > nfuzzy)
						{
							syserr("replacement $%c out of bounds",
								(*ap)[1]);
						}
						break;

					  case MATCHZANY:
						botch = "$*";
						break;

					  case MATCHANY:
						botch = "$+";
						break;

					  case MATCHONE:
						botch = "$-";
						break;

					  case MATCHCLASS:
						botch = "$=";
						break;

					  case MATCHNCLASS:
						botch = "$~";
						break;
					}
					if (botch != NULL)
						syserr("Inappropriate use of %s on RHS",
							botch);
				}
			}
			else
			{
				syserr("R line: null RHS");
				rwp->r_rhs = null_list;
			}
			break;

		  case 'S':		/* select rewriting set */
			expand(&bp[1], exbuf, sizeof exbuf, e);
			ruleset = strtorwset(exbuf, NULL, ST_ENTER);
			if (ruleset < 0)
				break;

			rwp = RewriteRules[ruleset];
			if (rwp != NULL)
			{
				if (OpMode == MD_TEST)
					printf("WARNING: Ruleset %s has multiple definitions\n",
					       &bp[1]);
				if (tTd(37, 1))
					dprintf("WARNING: Ruleset %s has multiple definitions\n",
						&bp[1]);
				while (rwp->r_next != NULL)
					rwp = rwp->r_next;
			}
			break;

		  case 'D':		/* macro definition */
			mid = macid(&bp[1], &ep);
			p = munchstring(ep, NULL, '\0');
			define(mid, newstr(p), e);
			break;

		  case 'H':		/* required header line */
			(void) chompheader(&bp[1], CHHDR_DEF, NULL, e);
			break;

		  case 'C':		/* word class */
		  case 'T':		/* trusted user (set class `t') */
			if (bp[0] == 'C')
			{
				mid = macid(&bp[1], &ep);
				expand(ep, exbuf, sizeof exbuf, e);
				p = exbuf;
			}
			else
			{
				mid = 't';
				p = &bp[1];
			}
			while (*p != '\0')
			{
				register char *wd;
				char delim;

				while (*p != '\0' && isascii(*p) && isspace(*p))
					p++;
				wd = p;
				while (*p != '\0' && !(isascii(*p) && isspace(*p)))
					p++;
				delim = *p;
				*p = '\0';
				if (wd[0] != '\0')
					setclass(mid, wd);
				*p = delim;
			}
			break;

		  case 'F':		/* word class from file */
			mid = macid(&bp[1], &ep);
			for (p = ep; isascii(*p) && isspace(*p); )
				p++;
			if (p[0] == '-' && p[1] == 'o')
			{
				optional = TRUE;
				while (*p != '\0' && !(isascii(*p) && isspace(*p)))
					p++;
				while (isascii(*p) && isspace(*p))
					p++;
			}
			else
				optional = FALSE;

			file = p;
			q = p;
			while (*q != '\0' && !(isascii(*q) && isspace(*q)))
				q++;
			if (*file == '|')
				p = "%s";
			else
			{
				p = q;
				if (*p == '\0')
					p = "%s";
				else
				{
					*p = '\0';
					while (isascii(*++p) && isspace(*p))
						continue;
				}
			}
			fileclass(mid, file, p, safe, optional);
			break;

#ifdef XLA
		  case 'L':		/* extended load average description */
			xla_init(&bp[1]);
			break;
#endif /* XLA */

#if defined(SUN_EXTENSIONS) && defined(SUN_LOOKUP_MACRO)
		  case 'L':		/* lookup macro */
		  case 'G':		/* lookup class */
			/* reserved for Sun -- NIS+ database lookup */
			if (VendorCode != VENDOR_SUN)
				goto badline;
			sun_lg_config_line(bp, e);
			break;
#endif /* defined(SUN_EXTENSIONS) && defined(SUN_LOOKUP_MACRO) */

		  case 'M':		/* define mailer */
			makemailer(&bp[1]);
			break;

		  case 'O':		/* set option */
			setoption(bp[1], &bp[2], safe, FALSE, e);
			break;

		  case 'P':		/* set precedence */
			if (NumPriorities >= MAXPRIORITIES)
			{
				toomany('P', MAXPRIORITIES);
				break;
			}
			for (p = &bp[1]; *p != '\0' && *p != '='; p++)
				continue;
			if (*p == '\0')
				goto badline;
			*p = '\0';
			Priorities[NumPriorities].pri_name = newstr(&bp[1]);
			Priorities[NumPriorities].pri_val = atoi(++p);
			NumPriorities++;
			break;

		  case 'V':		/* configuration syntax version */
			for (p = &bp[1]; isascii(*p) && isspace(*p); p++)
				continue;
			if (!isascii(*p) || !isdigit(*p))
			{
				syserr("invalid argument to V line: \"%.20s\"",
					&bp[1]);
				break;
			}
			ConfigLevel = strtol(p, &ep, 10);

			/*
			**  Do heuristic tweaking for back compatibility.
			*/

			if (ConfigLevel >= 5)
			{
				/* level 5 configs have short name in $w */
				p = macvalue('w', e);
				if (p != NULL && (p = strchr(p, '.')) != NULL)
					*p = '\0';
				define('w', macvalue('w', e), e);
			}
			if (ConfigLevel >= 6)
			{
				ColonOkInAddr = FALSE;
			}

			/*
			**  Look for vendor code.
			*/

			if (*ep++ == '/')
			{
				/* extract vendor code */
				for (p = ep; isascii(*p) && isalpha(*p); )
					p++;
				*p = '\0';

				if (!setvendor(ep))
					syserr("invalid V line vendor code: \"%s\"",
						ep);
			}
			break;

		  case 'K':
			expand(&bp[1], exbuf, sizeof exbuf, e);
			(void) makemapentry(exbuf);
			break;

		  case 'E':
			p = strchr(bp, '=');
			if (p != NULL)
				*p++ = '\0';
			setuserenv(&bp[1], p);
			break;

#if _FFR_MILTER
		  case 'X':		/* mail filter */
			milter_setup(&bp[1]);
			break;
#endif /* _FFR_MILTER */

		  default:
		  badline:
			syserr("unknown configuration line \"%s\"", bp);
		}
		if (bp != buf)
			free(bp);
	}
	if (ferror(cf))
	{
		syserr("I/O read error");
		finis(FALSE, EX_OSFILE);
	}
	(void) fclose(cf);
	FileName = NULL;

	/* initialize host maps from local service tables */
	inithostmaps();

	/* initialize daemon (if not defined yet) */
	initdaemon();

	/* determine if we need to do special name-server frotz */
	{
		int nmaps;
		char *maptype[MAXMAPSTACK];
		short mapreturn[MAXMAPACTIONS];

		nmaps = switch_map_find("hosts", maptype, mapreturn);
		UseNameServer = FALSE;
		if (nmaps > 0 && nmaps <= MAXMAPSTACK)
		{
			register int mapno;

			for (mapno = 0; mapno < nmaps && !UseNameServer; mapno++)
			{
				if (strcmp(maptype[mapno], "dns") == 0)
					UseNameServer = TRUE;
			}
		}

#ifdef HESIOD
		nmaps = switch_map_find("passwd", maptype, mapreturn);
		UseHesiod = FALSE;
		if (nmaps > 0 && nmaps <= MAXMAPSTACK)
		{
			register int mapno;

			for (mapno = 0; mapno < nmaps && !UseHesiod; mapno++)
			{
				if (strcmp(maptype[mapno], "hesiod") == 0)
					UseHesiod = TRUE;
			}
		}
#endif /* HESIOD */
	}
}
/*
**  TRANSLATE_DOLLARS -- convert $x into internal form
**
**	Actually does all appropriate pre-processing of a config line
**	to turn it into internal form.
**
**	Parameters:
**		bp -- the buffer to translate.
**
**	Returns:
**		None.  The buffer is translated in place.  Since the
**		translations always make the buffer shorter, this is
**		safe without a size parameter.
*/

void
translate_dollars(bp)
	char *bp;
{
	register char *p;
	auto char *ep;

	for (p = bp; *p != '\0'; p++)
	{
		if (*p == '#' && p > bp && ConfigLevel >= 3)
		{
			/* this is an on-line comment */
			register char *e;

			switch (*--p & 0377)
			{
			  case MACROEXPAND:
				/* it's from $# -- let it go through */
				p++;
				break;

			  case '\\':
				/* it's backslash escaped */
				(void) strlcpy(p, p + 1, strlen(p));
				break;

			  default:
				/* delete leading white space */
				while (isascii(*p) && isspace(*p) &&
				       *p != '\n' && p > bp)
					p--;
				if ((e = strchr(++p, '\n')) != NULL)
					(void) strlcpy(p, e, strlen(p));
				else
					*p-- = '\0';
				break;
			}
			continue;
		}

		if (*p != '$' || p[1] == '\0')
			continue;

		if (p[1] == '$')
		{
			/* actual dollar sign.... */
			(void) strlcpy(p, p + 1, strlen(p));
			continue;
		}

		/* convert to macro expansion character */
		*p++ = MACROEXPAND;

		/* special handling for $=, $~, $&, and $? */
		if (*p == '=' || *p == '~' || *p == '&' || *p == '?')
			p++;

		/* convert macro name to code */
		*p = macid(p, &ep);
		if (ep != p + 1)
			(void) strlcpy(p + 1, ep, strlen(p + 1));
	}

	/* strip trailing white space from the line */
	while (--p > bp && isascii(*p) && isspace(*p))
		*p = '\0';
}
/*
**  TOOMANY -- signal too many of some option
**
**	Parameters:
**		id -- the id of the error line
**		maxcnt -- the maximum possible values
**
**	Returns:
**		none.
**
**	Side Effects:
**		gives a syserr.
*/

static void
toomany(id, maxcnt)
	int id;
	int maxcnt;
{
	syserr("too many %c lines, %d max", id, maxcnt);
}
/*
**  FILECLASS -- read members of a class from a file
**
**	Parameters:
**		class -- class to define.
**		filename -- name of file to read.
**		fmt -- scanf string to use for match.
**		safe -- if set, this is a safe read.
**		optional -- if set, it is not an error for the file to
**			not exist.
**
**	Returns:
**		none
**
**	Side Effects:
**
**		puts all lines in filename that match a scanf into
**			the named class.
*/

static void
fileclass(class, filename, fmt, safe, optional)
	int class;
	char *filename;
	char *fmt;
	bool safe;
	bool optional;
{
	FILE *f;
	long sff;
	pid_t pid;
	register char *p;
	char buf[MAXLINE];

	if (tTd(37, 2))
		dprintf("fileclass(%s, fmt=%s)\n", filename, fmt);

	if (filename[0] == '|')
	{
		auto int fd;
		int i;
		char *argv[MAXPV + 1];

		i = 0;
		for (p = strtok(&filename[1], " \t"); p != NULL; p = strtok(NULL, " \t"))
		{
			if (i >= MAXPV)
				break;
			argv[i++] = p;
		}
		argv[i] = NULL;
		pid = prog_open(argv, &fd, CurEnv);
		if (pid < 0)
			f = NULL;
		else
			f = fdopen(fd, "r");
	}
	else
	{
		pid = -1;
		sff = SFF_REGONLY;
		if (!bitnset(DBS_CLASSFILEINUNSAFEDIRPATH, DontBlameSendmail))
			sff |= SFF_SAFEDIRPATH;
		if (!bitnset(DBS_LINKEDCLASSFILEINWRITABLEDIR,
			     DontBlameSendmail))
			sff |= SFF_NOWLINK;
		if (safe)
			sff |= SFF_OPENASROOT;
		if (DontLockReadFiles)
			sff |= SFF_NOLOCK;
		f = safefopen(filename, O_RDONLY, 0, sff);
	}
	if (f == NULL)
	{
		if (!optional)
			syserr("fileclass: cannot open '%s'", filename);
		return;
	}

	while (fgets(buf, sizeof buf, f) != NULL)
	{
#if SCANF
		char wordbuf[MAXLINE + 1];
#endif /* SCANF */

		if (buf[0] == '#')
			continue;
#if SCANF
		if (sscanf(buf, fmt, wordbuf) != 1)
			continue;
		p = wordbuf;
#else /* SCANF */
		p = buf;
#endif /* SCANF */

		/*
		**  Break up the match into words.
		*/

		while (*p != '\0')
		{
			register char *q;

			/* strip leading spaces */
			while (isascii(*p) && isspace(*p))
				p++;
			if (*p == '\0')
				break;

			/* find the end of the word */
			q = p;
			while (*p != '\0' && !(isascii(*p) && isspace(*p)))
				p++;
			if (*p != '\0')
				*p++ = '\0';

			/* enter the word in the symbol table */
			setclass(class, q);
		}
	}

	(void) fclose(f);
	if (pid > 0)
		(void) waitfor(pid);
}
/*
**  MAKEMAILER -- define a new mailer.
**
**	Parameters:
**		line -- description of mailer.  This is in labeled
**			fields.  The fields are:
**			   A -- the argv for this mailer
**			   C -- the character set for MIME conversions
**			   D -- the directory to run in
**			   E -- the eol string
**			   F -- the flags associated with the mailer
**			   L -- the maximum line length
**			   M -- the maximum message size
**			   N -- the niceness at which to run
**			   P -- the path to the mailer
**			   R -- the recipient rewriting set
**			   S -- the sender rewriting set
**			   T -- the mailer type (for DSNs)
**			   U -- the uid to run as
**			   W -- the time to wait at the end
**			The first word is the canonical name of the mailer.
**
**	Returns:
**		none.
**
**	Side Effects:
**		enters the mailer into the mailer table.
*/

void
makemailer(line)
	char *line;
{
	register char *p;
	register struct mailer *m;
	register STAB *s;
	int i;
	char fcode;
	auto char *endp;
	extern int NextMailer;

	/* allocate a mailer and set up defaults */
	m = (struct mailer *) xalloc(sizeof *m);
	memset((char *) m, '\0', sizeof *m);

	/* collect the mailer name */
	for (p = line; *p != '\0' && *p != ',' && !(isascii(*p) && isspace(*p)); p++)
		continue;
	if (*p != '\0')
		*p++ = '\0';
	if (line[0] == '\0')
		syserr("name required for mailer");
	m->m_name = newstr(line);

	/* now scan through and assign info from the fields */
	while (*p != '\0')
	{
		auto char *delimptr;

		while (*p != '\0' && (*p == ',' || (isascii(*p) && isspace(*p))))
			p++;

		/* p now points to field code */
		fcode = *p;
		while (*p != '\0' && *p != '=' && *p != ',')
			p++;
		if (*p++ != '=')
		{
			syserr("mailer %s: `=' expected", m->m_name);
			return;
		}
		while (isascii(*p) && isspace(*p))
			p++;

		/* p now points to the field body */
		p = munchstring(p, &delimptr, ',');

		/* install the field into the mailer struct */
		switch (fcode)
		{
		  case 'P':		/* pathname */
			if (*p == '\0')
				syserr("mailer %s: empty path name", m->m_name);
			m->m_mailer = newstr(p);
			break;

		  case 'F':		/* flags */
			for (; *p != '\0'; p++)
				if (!(isascii(*p) && isspace(*p)))
					setbitn(*p, m->m_flags);
			break;

		  case 'S':		/* sender rewriting ruleset */
		  case 'R':		/* recipient rewriting ruleset */
			i = strtorwset(p, &endp, ST_ENTER);
			if (i < 0)
				return;
			if (fcode == 'S')
				m->m_sh_rwset = m->m_se_rwset = i;
			else
				m->m_rh_rwset = m->m_re_rwset = i;

			p = endp;
			if (*p++ == '/')
			{
				i = strtorwset(p, NULL, ST_ENTER);
				if (i < 0)
					return;
				if (fcode == 'S')
					m->m_sh_rwset = i;
				else
					m->m_rh_rwset = i;
			}
			break;

		  case 'E':		/* end of line string */
			if (*p == '\0')
				syserr("mailer %s: null end-of-line string",
					m->m_name);
			m->m_eol = newstr(p);
			break;

		  case 'A':		/* argument vector */
			if (*p == '\0')
				syserr("mailer %s: null argument vector",
					m->m_name);
			m->m_argv = makeargv(p);
			break;

		  case 'M':		/* maximum message size */
			m->m_maxsize = atol(p);
			break;

		  case 'm':		/* maximum messages per connection */
			m->m_maxdeliveries = atoi(p);
			break;

#if _FFR_DYNAMIC_TOBUF
		  case 'r':		/* max recipient per envelope */
			m->m_maxrcpt = atoi(p);
			break;
#endif /* _FFR_DYNAMIC_TOBUF */

		  case 'L':		/* maximum line length */
			m->m_linelimit = atoi(p);
			if (m->m_linelimit < 0)
				m->m_linelimit = 0;
			break;

		  case 'N':		/* run niceness */
			m->m_nice = atoi(p);
			break;

		  case 'D':		/* working directory */
			if (*p == '\0')
				syserr("mailer %s: null working directory",
					m->m_name);
			m->m_execdir = newstr(p);
			break;

		  case 'C':		/* default charset */
			if (*p == '\0')
				syserr("mailer %s: null charset", m->m_name);
			m->m_defcharset = newstr(p);
			break;

		  case 'T':		/* MTA-Name/Address/Diagnostic types */
			/* extract MTA name type; default to "dns" */
			m->m_mtatype = newstr(p);
			p = strchr(m->m_mtatype, '/');
			if (p != NULL)
			{
				*p++ = '\0';
				if (*p == '\0')
					p = NULL;
			}
			if (*m->m_mtatype == '\0')
				m->m_mtatype = "dns";

			/* extract address type; default to "rfc822" */
			m->m_addrtype = p;
			if (p != NULL)
				p = strchr(p, '/');
			if (p != NULL)
			{
				*p++ = '\0';
				if (*p == '\0')
					p = NULL;
			}
			if (m->m_addrtype == NULL || *m->m_addrtype == '\0')
				m->m_addrtype = "rfc822";

			/* extract diagnostic type; default to "smtp" */
			m->m_diagtype = p;
			if (m->m_diagtype == NULL || *m->m_diagtype == '\0')
				m->m_diagtype = "smtp";
			break;

		  case 'U':		/* user id */
			if (isascii(*p) && !isdigit(*p))
			{
				char *q = p;
				struct passwd *pw;

				while (*p != '\0' && isascii(*p) &&
				       (isalnum(*p) || strchr("-_", *p) != NULL))
					p++;
				while (isascii(*p) && isspace(*p))
					*p++ = '\0';
				if (*p != '\0')
					*p++ = '\0';
				if (*q == '\0')
					syserr("mailer %s: null user name",
						m->m_name);
				pw = sm_getpwnam(q);
				if (pw == NULL)
					syserr("readcf: mailer U= flag: unknown user %s", q);
				else
				{
					m->m_uid = pw->pw_uid;
					m->m_gid = pw->pw_gid;
				}
			}
			else
			{
				auto char *q;

				m->m_uid = strtol(p, &q, 0);
				p = q;
				while (isascii(*p) && isspace(*p))
					p++;
				if (*p != '\0')
					p++;
			}
			while (isascii(*p) && isspace(*p))
				p++;
			if (*p == '\0')
				break;
			if (isascii(*p) && !isdigit(*p))
			{
				char *q = p;
				struct group *gr;

				while (isascii(*p) && isalnum(*p))
					p++;
				*p++ = '\0';
				if (*q == '\0')
					syserr("mailer %s: null group name",
						m->m_name);
				gr = getgrnam(q);
				if (gr == NULL)
					syserr("readcf: mailer U= flag: unknown group %s", q);
				else
					m->m_gid = gr->gr_gid;
			}
			else
			{
				m->m_gid = strtol(p, NULL, 0);
			}
			break;

		  case 'W':		/* wait timeout */
			m->m_wait = convtime(p, 's');
			break;

		  case '/':		/* new root directory */
			if (*p == '\0')
				syserr("mailer %s: null root directory",
					m->m_name);
			else
				m->m_rootdir = newstr(p);
			break;

		  default:
			syserr("M%s: unknown mailer equate %c=",
			       m->m_name, fcode);
			break;
		}

		p = delimptr;
	}

	/* do some rationality checking */
	if (m->m_argv == NULL)
	{
		syserr("M%s: A= argument required", m->m_name);
		return;
	}
	if (m->m_mailer == NULL)
	{
		syserr("M%s: P= argument required", m->m_name);
		return;
	}

	if (NextMailer >= MAXMAILERS)
	{
		syserr("too many mailers defined (%d max)", MAXMAILERS);
		return;
	}

#if _FFR_DYNAMIC_TOBUF
	if (m->m_maxrcpt <= 0)
		m->m_maxrcpt = DEFAULT_MAX_RCPT;
#endif /* _FFR_DYNAMIC_TOBUF */

	/* do some heuristic cleanup for back compatibility */
	if (bitnset(M_LIMITS, m->m_flags))
	{
		if (m->m_linelimit == 0)
			m->m_linelimit = SMTPLINELIM;
		if (ConfigLevel < 2)
			setbitn(M_7BITS, m->m_flags);
	}

	if (strcmp(m->m_mailer, "[TCP]") == 0)
	{
#if _FFR_REMOVE_TCP_PATH
		syserr("M%s: P=[TCP] is deprecated, use P=[IPC] instead\n",
		       m->m_name);
#else /* _FFR_REMOVE_TCP_PATH */
		printf("M%s: Warning: P=[TCP] is deprecated, use P=[IPC] instead\n",
		       m->m_name);
#endif /* _FFR_REMOVE_TCP_PATH */
	}

	if (strcmp(m->m_mailer, "[IPC]") == 0 ||
#if !_FFR_REMOVE_TCP_MAILER_PATH
	    strcmp(m->m_mailer, "[TCP]") == 0
#endif /* !_FFR_REMOVE_TCP_MAILER_PATH */
	    )
	{
		/* Use the second argument for host or path to socket */
		if (m->m_argv[0] == NULL || m->m_argv[1] == NULL ||
		    m->m_argv[1][0] == '\0')
		{
			syserr("M%s: too few parameters for %s mailer",
			       m->m_name, m->m_mailer);
		}
		if (strcmp(m->m_argv[0], "TCP") != 0 &&
#if NETUNIX
		    strcmp(m->m_argv[0], "FILE") != 0 &&
#endif /* NETUNIX */
#if !_FFR_DEPRECATE_IPC_MAILER_ARG
		    strcmp(m->m_argv[0], "IPC") != 0
#endif /* !_FFR_DEPRECATE_IPC_MAILER_ARG */
		    )
		{
			printf("M%s: Warning: first argument in %s mailer must be %s\n",
			       m->m_name, m->m_mailer,
#if NETUNIX
			       "TCP or FILE"
#else /* NETUNIX */
			       "TCP"
#endif /* NETUNIX */
			       );
		}

	}
	else if (strcmp(m->m_mailer, "[FILE]") == 0)
	{
		/* Use the second argument for filename */
		if (m->m_argv[0] == NULL || m->m_argv[1] == NULL ||
		    m->m_argv[2] != NULL)
		{
			syserr("M%s: too %s parameters for [FILE] mailer",
			       m->m_name,
			       (m->m_argv[0] == NULL ||
				m->m_argv[1] == NULL) ? "few" : "many");
		}
		else if (strcmp(m->m_argv[0], "FILE") != 0)
		{
			syserr("M%s: first argument in [FILE] mailer must be FILE",
			       m->m_name);
		}
	}

	if (strcmp(m->m_mailer, "[IPC]") == 0 ||
	    strcmp(m->m_mailer, "[TCP]") == 0)
	{
		if (m->m_mtatype == NULL)
			m->m_mtatype = "dns";
		if (m->m_addrtype == NULL)
			m->m_addrtype = "rfc822";
		if (m->m_diagtype == NULL)
		{
			if (m->m_argv[0] != NULL &&
			    strcmp(m->m_argv[0], "FILE") == 0)
				m->m_diagtype = "x-unix";
			else
				m->m_diagtype = "smtp";
		}
	}

	if (m->m_eol == NULL)
	{
		char **pp;

		/* default for SMTP is \r\n; use \n for local delivery */
		for (pp = m->m_argv; *pp != NULL; pp++)
		{
			for (p = *pp; *p != '\0'; )
			{
				if ((*p++ & 0377) == MACROEXPAND && *p == 'u')
					break;
			}
			if (*p != '\0')
				break;
		}
		if (*pp == NULL)
			m->m_eol = "\r\n";
		else
			m->m_eol = "\n";
	}

	/* enter the mailer into the symbol table */
	s = stab(m->m_name, ST_MAILER, ST_ENTER);
	if (s->s_mailer != NULL)
	{
		i = s->s_mailer->m_mno;
		free(s->s_mailer);
	}
	else
	{
		i = NextMailer++;
	}
	Mailer[i] = s->s_mailer = m;
	m->m_mno = i;
}
/*
**  MUNCHSTRING -- translate a string into internal form.
**
**	Parameters:
**		p -- the string to munch.
**		delimptr -- if non-NULL, set to the pointer of the
**			field delimiter character.
**		delim -- the delimiter for the field.
**
**	Returns:
**		the munched string.
**
**	Side Effects:
**		the munched string is a local static buffer.
**		it must be copied before the function is called again.
*/

char *
munchstring(p, delimptr, delim)
	register char *p;
	char **delimptr;
	int delim;
{
	register char *q;
	bool backslash = FALSE;
	bool quotemode = FALSE;
	static char buf[MAXLINE];

	for (q = buf; *p != '\0' && q < &buf[sizeof buf - 1]; p++)
	{
		if (backslash)
		{
			/* everything is roughly literal */
			backslash = FALSE;
			switch (*p)
			{
			  case 'r':		/* carriage return */
				*q++ = '\r';
				continue;

			  case 'n':		/* newline */
				*q++ = '\n';
				continue;

			  case 'f':		/* form feed */
				*q++ = '\f';
				continue;

			  case 'b':		/* backspace */
				*q++ = '\b';
				continue;
			}
			*q++ = *p;
		}
		else
		{
			if (*p == '\\')
				backslash = TRUE;
			else if (*p == '"')
				quotemode = !quotemode;
			else if (quotemode || *p != delim)
				*q++ = *p;
			else
				break;
		}
	}

	if (delimptr != NULL)
		*delimptr = p;
	*q++ = '\0';
	return buf;
}
/*
**  MAKEARGV -- break up a string into words
**
**	Parameters:
**		p -- the string to break up.
**
**	Returns:
**		a char **argv (dynamically allocated)
**
**	Side Effects:
**		munges p.
*/

static char **
makeargv(p)
	register char *p;
{
	char *q;
	int i;
	char **avp;
	char *argv[MAXPV + 1];

	/* take apart the words */
	i = 0;
	while (*p != '\0' && i < MAXPV)
	{
		q = p;
		while (*p != '\0' && !(isascii(*p) && isspace(*p)))
			p++;
		while (isascii(*p) && isspace(*p))
			*p++ = '\0';
		argv[i++] = newstr(q);
	}
	argv[i++] = NULL;

	/* now make a copy of the argv */
	avp = (char **) xalloc(sizeof *avp * i);
	memmove((char *) avp, (char *) argv, sizeof *avp * i);

	return avp;
}
/*
**  PRINTRULES -- print rewrite rules (for debugging)
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
**
**	Side Effects:
**		prints rewrite rules.
*/

void
printrules()
{
	register struct rewrite *rwp;
	register int ruleset;

	for (ruleset = 0; ruleset < 10; ruleset++)
	{
		if (RewriteRules[ruleset] == NULL)
			continue;
		printf("\n----Rule Set %d:", ruleset);

		for (rwp = RewriteRules[ruleset]; rwp != NULL; rwp = rwp->r_next)
		{
			printf("\nLHS:");
			printav(rwp->r_lhs);
			printf("RHS:");
			printav(rwp->r_rhs);
		}
	}
}
/*
**  PRINTMAILER -- print mailer structure (for debugging)
**
**	Parameters:
**		m -- the mailer to print
**
**	Returns:
**		none.
*/

void
printmailer(m)
	register MAILER *m;
{
	int j;

	printf("mailer %d (%s): P=%s S=", m->m_mno, m->m_name, m->m_mailer);
	if (RuleSetNames[m->m_se_rwset] == NULL)
		printf("%d/", m->m_se_rwset);
	else
		printf("%s/", RuleSetNames[m->m_se_rwset]);
	if (RuleSetNames[m->m_sh_rwset] == NULL)
		printf("%d R=", m->m_sh_rwset);
	else
		printf("%s R=", RuleSetNames[m->m_sh_rwset]);
	if (RuleSetNames[m->m_re_rwset] == NULL)
		printf("%d/", m->m_re_rwset);
	else
		printf("%s/", RuleSetNames[m->m_re_rwset]);
	if (RuleSetNames[m->m_rh_rwset] == NULL)
		printf("%d ", m->m_rh_rwset);
	else
		printf("%s ", RuleSetNames[m->m_rh_rwset]);
	printf("M=%ld U=%d:%d F=", m->m_maxsize,
	       (int) m->m_uid, (int) m->m_gid);
	for (j = '\0'; j <= '\177'; j++)
		if (bitnset(j, m->m_flags))
			(void) putchar(j);
	printf(" L=%d E=", m->m_linelimit);
	xputs(m->m_eol);
	if (m->m_defcharset != NULL)
		printf(" C=%s", m->m_defcharset);
	printf(" T=%s/%s/%s",
		m->m_mtatype == NULL ? "<undefined>" : m->m_mtatype,
		m->m_addrtype == NULL ? "<undefined>" : m->m_addrtype,
		m->m_diagtype == NULL ? "<undefined>" : m->m_diagtype);
#if _FFR_DYNAMIC_TOBUF
	printf(" r=%d", m->m_maxrcpt);
#endif /* _FFR_DYNAMIC_TOBUF */
	if (m->m_argv != NULL)
	{
		char **a = m->m_argv;

		printf(" A=");
		while (*a != NULL)
		{
			if (a != m->m_argv)
				printf(" ");
			xputs(*a++);
		}
	}
	printf("\n");
}
/*
**  SETOPTION -- set global processing option
**
**	Parameters:
**		opt -- option name.
**		val -- option value (as a text string).
**		safe -- set if this came from a configuration file.
**			Some options (if set from the command line) will
**			reset the user id to avoid security problems.
**		sticky -- if set, don't let other setoptions override
**			this value.
**		e -- the main envelope.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Sets options as implied by the arguments.
*/

static BITMAP256	StickyOpt;		/* set if option is stuck */

#if NAMED_BIND

static struct resolverflags
{
	char	*rf_name;	/* name of the flag */
	long	rf_bits;	/* bits to set/clear */
} ResolverFlags[] =
{
	{ "debug",	RES_DEBUG	},
	{ "aaonly",	RES_AAONLY	},
	{ "usevc",	RES_USEVC	},
	{ "primary",	RES_PRIMARY	},
	{ "igntc",	RES_IGNTC	},
	{ "recurse",	RES_RECURSE	},
	{ "defnames",	RES_DEFNAMES	},
	{ "stayopen",	RES_STAYOPEN	},
	{ "dnsrch",	RES_DNSRCH	},
	{ "true",	0		},	/* avoid error on old syntax */
	{ NULL,		0		}
};

#endif /* NAMED_BIND */

#define OI_NONE		0	/* no special treatment */
#define OI_SAFE		0x0001	/* safe for random people to use */
#define OI_SUBOPT	0x0002	/* option has suboptions */

static struct optioninfo
{
	char	*o_name;	/* long name of option */
	u_char	o_code;		/* short name of option */
	u_short	o_flags;	/* option flags */
} OptionTab[] =
{
#if defined(SUN_EXTENSIONS) && defined(REMOTE_MODE)
	{ "RemoteMode",			'>',		OI_NONE	},
#endif /* defined(SUN_EXTENSIONS) && defined(REMOTE_MODE) */
	{ "SevenBitInput",		'7',		OI_SAFE	},
#if MIME8TO7
	{ "EightBitMode",		'8',		OI_SAFE	},
#endif /* MIME8TO7 */
	{ "AliasFile",			'A',		OI_NONE	},
	{ "AliasWait",			'a',		OI_NONE	},
	{ "BlankSub",			'B',		OI_NONE	},
	{ "MinFreeBlocks",		'b',		OI_SAFE	},
	{ "CheckpointInterval",		'C',		OI_SAFE	},
	{ "HoldExpensive",		'c',		OI_NONE	},
#if !_FFR_REMOVE_AUTOREBUILD
	{ "AutoRebuildAliases",		'D',		OI_NONE	},
#endif /* !_FFR_REMOVE_AUTOREBUILD */
	{ "DeliveryMode",		'd',		OI_SAFE	},
	{ "ErrorHeader",		'E',		OI_NONE	},
	{ "ErrorMode",			'e',		OI_SAFE	},
	{ "TempFileMode",		'F',		OI_NONE	},
	{ "SaveFromLine",		'f',		OI_NONE	},
	{ "MatchGECOS",			'G',		OI_NONE	},
	{ "HelpFile",			'H',		OI_NONE	},
	{ "MaxHopCount",		'h',		OI_NONE	},
	{ "ResolverOptions",		'I',		OI_NONE	},
	{ "IgnoreDots",			'i',		OI_SAFE	},
	{ "ForwardPath",		'J',		OI_NONE	},
	{ "SendMimeErrors",		'j',		OI_SAFE	},
	{ "ConnectionCacheSize",	'k',		OI_NONE	},
	{ "ConnectionCacheTimeout",	'K',		OI_NONE	},
	{ "UseErrorsTo",		'l',		OI_NONE	},
	{ "LogLevel",			'L',		OI_SAFE	},
	{ "MeToo",			'm',		OI_SAFE	},
	{ "CheckAliases",		'n',		OI_NONE	},
	{ "OldStyleHeaders",		'o',		OI_SAFE	},
	{ "DaemonPortOptions",		'O',		OI_NONE	},
	{ "PrivacyOptions",		'p',		OI_SAFE	},
	{ "PostmasterCopy",		'P',		OI_NONE	},
	{ "QueueFactor",		'q',		OI_NONE	},
	{ "QueueDirectory",		'Q',		OI_NONE	},
	{ "DontPruneRoutes",		'R',		OI_NONE	},
	{ "Timeout",			'r',		OI_SUBOPT },
	{ "StatusFile",			'S',		OI_NONE	},
	{ "SuperSafe",			's',		OI_SAFE	},
	{ "QueueTimeout",		'T',		OI_NONE	},
	{ "TimeZoneSpec",		't',		OI_NONE	},
	{ "UserDatabaseSpec",		'U',		OI_NONE	},
	{ "DefaultUser",		'u',		OI_NONE	},
	{ "FallbackMXhost",		'V',		OI_NONE	},
	{ "Verbose",			'v',		OI_SAFE	},
	{ "TryNullMXList",		'w',		OI_NONE	},
	{ "QueueLA",			'x',		OI_NONE	},
	{ "RefuseLA",			'X',		OI_NONE	},
	{ "RecipientFactor",		'y',		OI_NONE	},
	{ "ForkEachJob",		'Y',		OI_NONE	},
	{ "ClassFactor",		'z',		OI_NONE	},
	{ "RetryFactor",		'Z',		OI_NONE	},
#define O_QUEUESORTORD	0x81
	{ "QueueSortOrder",		O_QUEUESORTORD,	OI_SAFE	},
#define O_HOSTSFILE	0x82
	{ "HostsFile",			O_HOSTSFILE,	OI_NONE	},
#define O_MQA		0x83
	{ "MinQueueAge",		O_MQA,		OI_SAFE	},
#define O_DEFCHARSET	0x85
	{ "DefaultCharSet",		O_DEFCHARSET,	OI_SAFE	},
#define O_SSFILE	0x86
	{ "ServiceSwitchFile",		O_SSFILE,	OI_NONE	},
#define O_DIALDELAY	0x87
	{ "DialDelay",			O_DIALDELAY,	OI_SAFE	},
#define O_NORCPTACTION	0x88
	{ "NoRecipientAction",		O_NORCPTACTION,	OI_SAFE	},
#define O_SAFEFILEENV	0x89
	{ "SafeFileEnvironment",	O_SAFEFILEENV,	OI_NONE	},
#define O_MAXMSGSIZE	0x8a
	{ "MaxMessageSize",		O_MAXMSGSIZE,	OI_NONE	},
#define O_COLONOKINADDR	0x8b
	{ "ColonOkInAddr",		O_COLONOKINADDR, OI_SAFE },
#define O_MAXQUEUERUN	0x8c
	{ "MaxQueueRunSize",		O_MAXQUEUERUN,	OI_SAFE	},
#define O_MAXCHILDREN	0x8d
	{ "MaxDaemonChildren",		O_MAXCHILDREN,	OI_NONE	},
#define O_KEEPCNAMES	0x8e
	{ "DontExpandCnames",		O_KEEPCNAMES,	OI_NONE	},
#define O_MUSTQUOTE	0x8f
	{ "MustQuoteChars",		O_MUSTQUOTE,	OI_NONE	},
#define O_SMTPGREETING	0x90
	{ "SmtpGreetingMessage",	O_SMTPGREETING,	OI_NONE	},
#define O_UNIXFROM	0x91
	{ "UnixFromLine",		O_UNIXFROM,	OI_NONE	},
#define O_OPCHARS	0x92
	{ "OperatorChars",		O_OPCHARS,	OI_NONE	},
#define O_DONTINITGRPS	0x93
	{ "DontInitGroups",		O_DONTINITGRPS,	OI_NONE	},
#define O_SLFH		0x94
	{ "SingleLineFromHeader",	O_SLFH,		OI_SAFE	},
#define O_ABH		0x95
	{ "AllowBogusHELO",		O_ABH,		OI_SAFE	},
#define O_CONNTHROT	0x97
	{ "ConnectionRateThrottle",	O_CONNTHROT,	OI_NONE	},
#define O_UGW		0x99
	{ "UnsafeGroupWrites",		O_UGW,		OI_NONE	},
#define O_DBLBOUNCE	0x9a
	{ "DoubleBounceAddress",	O_DBLBOUNCE,	OI_NONE	},
#define O_HSDIR		0x9b
	{ "HostStatusDirectory",	O_HSDIR,	OI_NONE	},
#define O_SINGTHREAD	0x9c
	{ "SingleThreadDelivery",	O_SINGTHREAD,	OI_NONE	},
#define O_RUNASUSER	0x9d
	{ "RunAsUser",			O_RUNASUSER,	OI_NONE	},
#define O_DSN_RRT	0x9e
	{ "RrtImpliesDsn",		O_DSN_RRT,	OI_NONE	},
#define O_PIDFILE	0x9f
	{ "PidFile",			O_PIDFILE,	OI_NONE	},
#define O_DONTBLAMESENDMAIL	0xa0
	{ "DontBlameSendmail",		O_DONTBLAMESENDMAIL,	OI_NONE	},
#define O_DPI		0xa1
	{ "DontProbeInterfaces",	O_DPI,		OI_NONE	},
#define O_MAXRCPT	0xa2
	{ "MaxRecipientsPerMessage",	O_MAXRCPT,	OI_SAFE	},
#define O_DEADLETTER	0xa3
	{ "DeadLetterDrop",		O_DEADLETTER,	OI_NONE	},
#if _FFR_DONTLOCKFILESFORREAD_OPTION
# define O_DONTLOCK	0xa4
	{ "DontLockFilesForRead",	O_DONTLOCK,	OI_NONE	},
#endif /* _FFR_DONTLOCKFILESFORREAD_OPTION */
#define O_MAXALIASRCSN	0xa5
	{ "MaxAliasRecursion",		O_MAXALIASRCSN,	OI_NONE	},
#define O_CNCTONLYTO	0xa6
	{ "ConnectOnlyTo",		O_CNCTONLYTO,	OI_NONE	},
#define O_TRUSTUSER	0xa7
	{ "TrustedUser",		O_TRUSTUSER,	OI_NONE	},
#define O_MAXMIMEHDRLEN	0xa8
	{ "MaxMimeHeaderLength",	O_MAXMIMEHDRLEN,	OI_NONE	},
#define O_CONTROLSOCKET	0xa9
	{ "ControlSocketName",		O_CONTROLSOCKET,	OI_NONE	},
#define O_MAXHDRSLEN	0xaa
	{ "MaxHeadersLength",		O_MAXHDRSLEN,	OI_NONE	},
#if _FFR_MAX_FORWARD_ENTRIES
# define O_MAXFORWARD	0xab
	{ "MaxForwardEntries",		O_MAXFORWARD,	OI_NONE	},
#endif /* _FFR_MAX_FORWARD_ENTRIES */
#define O_PROCTITLEPREFIX	0xac
	{ "ProcessTitlePrefix",		O_PROCTITLEPREFIX,	OI_NONE	},
#define O_SASLINFO	0xad
#if _FFR_ALLOW_SASLINFO
	{ "DefaultAuthInfo",		O_SASLINFO,	OI_SAFE	},
#else /* _FFR_ALLOW_SASLINFO */
	{ "DefaultAuthInfo",		O_SASLINFO,	OI_NONE	},
#endif /* _FFR_ALLOW_SASLINFO */
#define O_SASLMECH	0xae
	{ "AuthMechanisms",		O_SASLMECH,	OI_NONE	},
#define O_CLIENTPORT	0xaf
	{ "ClientPortOptions",		O_CLIENTPORT,	OI_NONE	},
#define O_DF_BUFSIZE	0xb0
	{ "DataFileBufferSize",		O_DF_BUFSIZE,	OI_NONE	},
#define O_XF_BUFSIZE	0xb1
	{ "XscriptFileBufferSize",	O_XF_BUFSIZE,	OI_NONE	},
# define O_LDAPDEFAULTSPEC	0xb2
	{ "LDAPDefaultSpec",		O_LDAPDEFAULTSPEC,	OI_NONE	},
#if _FFR_QUEUEDELAY
#define O_QUEUEDELAY	0xb3
	{ "QueueDelay",			O_QUEUEDELAY,	OI_NONE	},
#endif /* _FFR_QUEUEDELAY */
# define O_SRVCERTFILE	0xb4
	{ "ServerCertFile",		O_SRVCERTFILE,	OI_NONE	},
# define O_SRVKEYFILE	0xb5
	{ "Serverkeyfile",		O_SRVKEYFILE,	OI_NONE	},
# define O_CLTCERTFILE	0xb6
	{ "ClientCertFile",		O_CLTCERTFILE,	OI_NONE	},
# define O_CLTKEYFILE	0xb7
	{ "Clientkeyfile",		O_CLTKEYFILE,	OI_NONE	},
# define O_CACERTFILE	0xb8
	{ "CACERTFile",			O_CACERTFILE,	OI_NONE	},
# define O_CACERTPATH	0xb9
	{ "CACERTPath",			O_CACERTPATH,	OI_NONE	},
# define O_DHPARAMS	0xba
	{ "DHParameters",		O_DHPARAMS,	OI_NONE	},
#if _FFR_MILTER
#define O_INPUTMILTER	0xbb
	{ "InputMailFilters",		O_INPUTMILTER,	OI_NONE	},
#define O_MILTER	0xbc
	{ "Milter",			O_MILTER,	OI_SUBOPT	},
#endif /* _FFR_MILTER */
#define O_SASLOPTS	0xbd
	{ "AuthOptions",		O_SASLOPTS,	OI_NONE	},
#if _FFR_QUEUE_FILE_MODE
#define O_QUEUE_FILE_MODE	0xbe
	{ "QueueFileMode",		O_QUEUE_FILE_MODE, OI_NONE	},
#endif /* _FFR_QUEUE_FILE_MODE */
# if _FFR_TLS_1
# define O_DHPARAMS5	0xbf
	{ "DHParameters512",		O_DHPARAMS5,	OI_NONE	},
# define O_CIPHERLIST	0xc0
	{ "CipherList",			O_CIPHERLIST,	OI_NONE	},
# endif /* _FFR_TLS_1 */
# define O_RANDFILE	0xc1
	{ "RandFile",			O_RANDFILE,	OI_NONE	},
	{ NULL,				'\0',		OI_NONE	}
};

void
setoption(opt, val, safe, sticky, e)
	int opt;
	char *val;
	bool safe;
	bool sticky;
	register ENVELOPE *e;
{
	register char *p;
	register struct optioninfo *o;
	char *subopt;
	int mid;
	bool can_setuid = RunAsUid == 0;
	auto char *ep;
	char buf[50];
	extern bool Warn_Q_option;
#if _FFR_ALLOW_SASLINFO
	extern int SubmitMode;
#endif /* _FFR_ALLOW_SASLINFO */

	errno = 0;
	if (opt == ' ')
	{
		/* full word options */
		struct optioninfo *sel;

		p = strchr(val, '=');
		if (p == NULL)
			p = &val[strlen(val)];
		while (*--p == ' ')
			continue;
		while (*++p == ' ')
			*p = '\0';
		if (p == val)
		{
			syserr("readcf: null option name");
			return;
		}
		if (*p == '=')
			*p++ = '\0';
		while (*p == ' ')
			p++;
		subopt = strchr(val, '.');
		if (subopt != NULL)
			*subopt++ = '\0';
		sel = NULL;
		for (o = OptionTab; o->o_name != NULL; o++)
		{
			if (strncasecmp(o->o_name, val, strlen(val)) != 0)
				continue;
			if (strlen(o->o_name) == strlen(val))
			{
				/* completely specified -- this must be it */
				sel = NULL;
				break;
			}
			if (sel != NULL)
				break;
			sel = o;
		}
		if (sel != NULL && o->o_name == NULL)
			o = sel;
		else if (o->o_name == NULL)
		{
			syserr("readcf: unknown option name %s", val);
			return;
		}
		else if (sel != NULL)
		{
			syserr("readcf: ambiguous option name %s (matches %s and %s)",
				val, sel->o_name, o->o_name);
			return;
		}
		if (strlen(val) != strlen(o->o_name))
		{
			int oldVerbose = Verbose;

			Verbose = 1;
			message("Option %s used as abbreviation for %s",
				val, o->o_name);
			Verbose = oldVerbose;
		}
		opt = o->o_code;
		val = p;
	}
	else
	{
		for (o = OptionTab; o->o_name != NULL; o++)
		{
			if (o->o_code == opt)
				break;
		}
		subopt = NULL;
	}

	if (subopt != NULL && !bitset(OI_SUBOPT, o->o_flags))
	{
		if (tTd(37, 1))
			dprintf("setoption: %s does not support suboptions, ignoring .%s\n",
				o->o_name == NULL ? "<unknown>" : o->o_name,
				subopt);
		subopt = NULL;
	}

	if (tTd(37, 1))
	{
		dprintf(isascii(opt) && isprint(opt) ?
			"setoption %s (%c)%s%s=" :
			"setoption %s (0x%x)%s%s=",
			o->o_name == NULL ? "<unknown>" : o->o_name,
			opt,
			subopt == NULL ? "" : ".",
			subopt == NULL ? "" : subopt);
		xputs(val);
	}

	/*
	**  See if this option is preset for us.
	*/

	if (!sticky && bitnset(opt, StickyOpt))
	{
		if (tTd(37, 1))
			dprintf(" (ignored)\n");
		return;
	}

	/*
	**  Check to see if this option can be specified by this user.
	*/

	if (!safe && RealUid == 0)
		safe = TRUE;
	if (!safe && !bitset(OI_SAFE, o->o_flags))
	{
		if (opt != 'M' || (val[0] != 'r' && val[0] != 's'))
		{
			int dp;

			if (tTd(37, 1))
				dprintf(" (unsafe)");
			dp = drop_privileges(TRUE);
			setstat(dp);
		}
	}
	if (tTd(37, 1))
		dprintf("\n");

	switch (opt & 0xff)
	{
	  case '7':		/* force seven-bit input */
		SevenBitInput = atobool(val);
		break;

#if MIME8TO7
	  case '8':		/* handling of 8-bit input */
		switch (*val)
		{
		  case 'm':		/* convert 8-bit, convert MIME */
			MimeMode = MM_CVTMIME|MM_MIME8BIT;
			break;

		  case 'p':		/* pass 8 bit, convert MIME */
			MimeMode = MM_CVTMIME|MM_PASS8BIT;
			break;

		  case 's':		/* strict adherence */
			MimeMode = MM_CVTMIME;
			break;

# if 0
		  case 'r':		/* reject 8-bit, don't convert MIME */
			MimeMode = 0;
			break;

		  case 'j':		/* "just send 8" */
			MimeMode = MM_PASS8BIT;
			break;

		  case 'a':		/* encode 8 bit if available */
			MimeMode = MM_MIME8BIT|MM_PASS8BIT|MM_CVTMIME;
			break;

		  case 'c':		/* convert 8 bit to MIME, never 7 bit */
			MimeMode = MM_MIME8BIT;
			break;
# endif /* 0 */

		  default:
			syserr("Unknown 8-bit mode %c", *val);
			finis(FALSE, EX_USAGE);
		}
		break;
#endif /* MIME8TO7 */

	  case 'A':		/* set default alias file */
		if (val[0] == '\0')
			setalias("aliases");
		else
			setalias(val);
		break;

	  case 'a':		/* look N minutes for "@:@" in alias file */
		if (val[0] == '\0')
			SafeAlias = 5 * 60;		/* five minutes */
		else
			SafeAlias = convtime(val, 'm');
		break;

	  case 'B':		/* substitution for blank character */
		SpaceSub = val[0];
		if (SpaceSub == '\0')
			SpaceSub = ' ';
		break;

	  case 'b':		/* min blocks free on queue fs/max msg size */
		p = strchr(val, '/');
		if (p != NULL)
		{
			*p++ = '\0';
			MaxMessageSize = atol(p);
		}
		MinBlocksFree = atol(val);
		break;

	  case 'c':		/* don't connect to "expensive" mailers */
		NoConnect = atobool(val);
		break;

	  case 'C':		/* checkpoint every N addresses */
		CheckpointInterval = atoi(val);
		break;

	  case 'd':		/* delivery mode */
		switch (*val)
		{
		  case '\0':
			set_delivery_mode(SM_DELIVER, e);
			break;

		  case SM_QUEUE:	/* queue only */
		  case SM_DEFER:	/* queue only and defer map lookups */
#if !QUEUE
			syserr("need QUEUE to set -odqueue or -oddefer");
#endif /* !QUEUE */
			/* FALLTHROUGH */

		  case SM_DELIVER:	/* do everything */
		  case SM_FORK:		/* fork after verification */
			set_delivery_mode(*val, e);
			break;

		  default:
			syserr("Unknown delivery mode %c", *val);
			finis(FALSE, EX_USAGE);
		}
		break;

#if !_FFR_REMOVE_AUTOREBUILD
	  case 'D':		/* rebuild alias database as needed */
		AutoRebuild = atobool(val);
		break;
#endif /* !_FFR_REMOVE_AUTOREBUILD */

	  case 'E':		/* error message header/header file */
		if (*val != '\0')
			ErrMsgFile = newstr(val);
		break;

	  case 'e':		/* set error processing mode */
		switch (*val)
		{
		  case EM_QUIET:	/* be silent about it */
		  case EM_MAIL:		/* mail back */
		  case EM_BERKNET:	/* do berknet error processing */
		  case EM_WRITE:	/* write back (or mail) */
		  case EM_PRINT:	/* print errors normally (default) */
			e->e_errormode = *val;
			break;
		}
		break;

	  case 'F':		/* file mode */
		FileMode = atooct(val) & 0777;
		break;

	  case 'f':		/* save Unix-style From lines on front */
		SaveFrom = atobool(val);
		break;

	  case 'G':		/* match recipients against GECOS field */
		MatchGecos = atobool(val);
		break;

	  case 'g':		/* default gid */
  g_opt:
		if (isascii(*val) && isdigit(*val))
			DefGid = atoi(val);
		else
		{
			register struct group *gr;

			DefGid = -1;
			gr = getgrnam(val);
			if (gr == NULL)
				syserr("readcf: option %c: unknown group %s",
					opt, val);
			else
				DefGid = gr->gr_gid;
		}
		break;

	  case 'H':		/* help file */
		if (val[0] == '\0')
			HelpFile = "helpfile";
		else
			HelpFile = newstr(val);
		break;

	  case 'h':		/* maximum hop count */
		MaxHopCount = atoi(val);
		break;

	  case 'I':		/* use internet domain name server */
#if NAMED_BIND
		for (p = val; *p != 0; )
		{
			bool clearmode;
			char *q;
			struct resolverflags *rfp;

			while (*p == ' ')
				p++;
			if (*p == '\0')
				break;
			clearmode = FALSE;
			if (*p == '-')
				clearmode = TRUE;
			else if (*p != '+')
				p--;
			p++;
			q = p;
			while (*p != '\0' && !(isascii(*p) && isspace(*p)))
				p++;
			if (*p != '\0')
				*p++ = '\0';
			if (strcasecmp(q, "HasWildcardMX") == 0)
			{
				HasWildcardMX = !clearmode;
				continue;
			}
			for (rfp = ResolverFlags; rfp->rf_name != NULL; rfp++)
			{
				if (strcasecmp(q, rfp->rf_name) == 0)
					break;
			}
			if (rfp->rf_name == NULL)
				syserr("readcf: I option value %s unrecognized", q);
			else if (clearmode)
				_res.options &= ~rfp->rf_bits;
			else
				_res.options |= rfp->rf_bits;
		}
		if (tTd(8, 2))
			dprintf("_res.options = %x, HasWildcardMX = %d\n",
				(u_int) _res.options, HasWildcardMX);
#else /* NAMED_BIND */
		usrerr("name server (I option) specified but BIND not compiled in");
#endif /* NAMED_BIND */
		break;

	  case 'i':		/* ignore dot lines in message */
		IgnrDot = atobool(val);
		break;

	  case 'j':		/* send errors in MIME (RFC 1341) format */
		SendMIMEErrors = atobool(val);
		break;

	  case 'J':		/* .forward search path */
		ForwardPath = newstr(val);
		break;

	  case 'k':		/* connection cache size */
		MaxMciCache = atoi(val);
		if (MaxMciCache < 0)
			MaxMciCache = 0;
		break;

	  case 'K':		/* connection cache timeout */
		MciCacheTimeout = convtime(val, 'm');
		break;

	  case 'l':		/* use Errors-To: header */
		UseErrorsTo = atobool(val);
		break;

	  case 'L':		/* log level */
		if (safe || LogLevel < atoi(val))
			LogLevel = atoi(val);
		break;

	  case 'M':		/* define macro */
		mid = macid(val, &ep);
		p = newstr(ep);
		if (!safe)
			cleanstrcpy(p, p, MAXNAME);
		define(mid, p, CurEnv);
		sticky = FALSE;
		break;

	  case 'm':		/* send to me too */
		MeToo = atobool(val);
		break;

	  case 'n':		/* validate RHS in newaliases */
		CheckAliases = atobool(val);
		break;

	    /* 'N' available -- was "net name" */

	  case 'O':		/* daemon options */
#if DAEMON
		if (!setdaemonoptions(val))
		{
			syserr("too many daemons defined (%d max)", MAXDAEMONS);
		}
#else /* DAEMON */
		syserr("DaemonPortOptions (O option) set but DAEMON not compiled in");
#endif /* DAEMON */
		break;

	  case 'o':		/* assume old style headers */
		if (atobool(val))
			CurEnv->e_flags |= EF_OLDSTYLE;
		else
			CurEnv->e_flags &= ~EF_OLDSTYLE;
		break;

	  case 'p':		/* select privacy level */
		p = val;
		for (;;)
		{
			register struct prival *pv;
			extern struct prival PrivacyValues[];

			while (isascii(*p) && (isspace(*p) || ispunct(*p)))
				p++;
			if (*p == '\0')
				break;
			val = p;
			while (isascii(*p) && isalnum(*p))
				p++;
			if (*p != '\0')
				*p++ = '\0';

			for (pv = PrivacyValues; pv->pv_name != NULL; pv++)
			{
				if (strcasecmp(val, pv->pv_name) == 0)
					break;
			}
			if (pv->pv_name == NULL)
				syserr("readcf: Op line: %s unrecognized", val);
			PrivacyFlags |= pv->pv_flag;
		}
		sticky = FALSE;
		break;

	  case 'P':		/* postmaster copy address for returned mail */
		PostMasterCopy = newstr(val);
		break;

	  case 'q':		/* slope of queue only function */
		QueueFactor = atoi(val);
		break;

	  case 'Q':		/* queue directory */
		if (val[0] == '\0')
			QueueDir = "mqueue";
		else
			QueueDir = newstr(val);
		if (RealUid != 0 && !safe)
			Warn_Q_option = TRUE;
		break;

	  case 'R':		/* don't prune routes */
		DontPruneRoutes = atobool(val);
		break;

	  case 'r':		/* read timeout */
		if (subopt == NULL)
			inittimeouts(val, sticky);
		else
			settimeout(subopt, val, sticky);
		break;

	  case 'S':		/* status file */
		if (val[0] == '\0')
			StatFile = "statistics";
		else
			StatFile = newstr(val);
		break;

	  case 's':		/* be super safe, even if expensive */
		SuperSafe = atobool(val);
		break;

	  case 'T':		/* queue timeout */
		p = strchr(val, '/');
		if (p != NULL)
		{
			*p++ = '\0';
			settimeout("queuewarn", p, sticky);
		}
		settimeout("queuereturn", val, sticky);
		break;

	  case 't':		/* time zone name */
		TimeZoneSpec = newstr(val);
		break;

	  case 'U':		/* location of user database */
		UdbSpec = newstr(val);
		break;

	  case 'u':		/* set default uid */
		for (p = val; *p != '\0'; p++)
		{
			if (*p == '.' || *p == '/' || *p == ':')
			{
				*p++ = '\0';
				break;
			}
		}
		if (isascii(*val) && isdigit(*val))
		{
			DefUid = atoi(val);
			setdefuser();
		}
		else
		{
			register struct passwd *pw;

			DefUid = -1;
			pw = sm_getpwnam(val);
			if (pw == NULL)
				syserr("readcf: option u: unknown user %s", val);
			else
			{
				DefUid = pw->pw_uid;
				DefGid = pw->pw_gid;
				DefUser = newstr(pw->pw_name);
			}
		}

#ifdef UID_MAX
		if (DefUid > UID_MAX)
		{
			syserr("readcf: option u: uid value (%ld) > UID_MAX (%ld); ignored",
				DefUid, UID_MAX);
		}
#endif /* UID_MAX */

		/* handle the group if it is there */
		if (*p == '\0')
			break;
		val = p;
		goto g_opt;

	  case 'V':		/* fallback MX host */
		if (val[0] != '\0')
			FallBackMX = newstr(val);
		break;

	  case 'v':		/* run in verbose mode */
		Verbose = atobool(val) ? 1 : 0;
		break;

	  case 'w':		/* if we are best MX, try host directly */
		TryNullMXList = atobool(val);
		break;

	    /* 'W' available -- was wizard password */

	  case 'x':		/* load avg at which to auto-queue msgs */
		QueueLA = atoi(val);
		break;

	  case 'X':		/* load avg at which to auto-reject connections */
		RefuseLA = atoi(val);
		break;

	  case 'y':		/* work recipient factor */
		WkRecipFact = atoi(val);
		break;

	  case 'Y':		/* fork jobs during queue runs */
		ForkQueueRuns = atobool(val);
		break;

	  case 'z':		/* work message class factor */
		WkClassFact = atoi(val);
		break;

	  case 'Z':		/* work time factor */
		WkTimeFact = atoi(val);
		break;


	  case O_QUEUESORTORD:	/* queue sorting order */
		switch (*val)
		{
		  case 'h':	/* Host first */
		  case 'H':
			QueueSortOrder = QSO_BYHOST;
			break;

		  case 'p':	/* Priority order */
		  case 'P':
			QueueSortOrder = QSO_BYPRIORITY;
			break;

		  case 't':	/* Submission time */
		  case 'T':
			QueueSortOrder = QSO_BYTIME;
			break;

		  case 'f':	/* File Name */
		  case 'F':
			QueueSortOrder = QSO_BYFILENAME;
			break;

		  default:
			syserr("Invalid queue sort order \"%s\"", val);
		}
		break;

#if _FFR_QUEUEDELAY
	  case O_QUEUEDELAY:	/* queue delay algorithm */
		switch (*val)
		{
		  case 'e':	/* exponential */
		  case 'E':
			QueueAlg = QD_EXP;
			QueueInitDelay = 10 MINUTES;
			QueueMaxDelay = 2 HOURS;
			p = strchr(val, '/');
			if (p != NULL)
			{
				char *q;

				*p++ = '\0';
				q = strchr(p, '/');
				if (q != NULL)
					*q++ = '\0';
				QueueInitDelay = convtime(p, 's');
				if (q != NULL)
				{
					QueueMaxDelay = convtime(q, 's');
				}
			}
			break;

		  case 'l':	/* linear */
		  case 'L':
			QueueAlg = QD_LINEAR;
			break;

		  default:
			syserr("Invalid queue delay algorithm \"%s\"", val);
		}
		break;
#endif /* _FFR_QUEUEDELAY */

	  case O_HOSTSFILE:	/* pathname of /etc/hosts file */
		HostsFile = newstr(val);
		break;

	  case O_MQA:		/* minimum queue age between deliveries */
		MinQueueAge = convtime(val, 'm');
		break;

	  case O_DEFCHARSET:	/* default character set for mimefying */
		DefaultCharSet = newstr(denlstring(val, TRUE, TRUE));
		break;

	  case O_SSFILE:	/* service switch file */
		ServiceSwitchFile = newstr(val);
		break;

	  case O_DIALDELAY:	/* delay for dial-on-demand operation */
		DialDelay = convtime(val, 's');
		break;

	  case O_NORCPTACTION:	/* what to do if no recipient */
		if (strcasecmp(val, "none") == 0)
			NoRecipientAction = NRA_NO_ACTION;
		else if (strcasecmp(val, "add-to") == 0)
			NoRecipientAction = NRA_ADD_TO;
		else if (strcasecmp(val, "add-apparently-to") == 0)
			NoRecipientAction = NRA_ADD_APPARENTLY_TO;
		else if (strcasecmp(val, "add-bcc") == 0)
			NoRecipientAction = NRA_ADD_BCC;
		else if (strcasecmp(val, "add-to-undisclosed") == 0)
			NoRecipientAction = NRA_ADD_TO_UNDISCLOSED;
		else
			syserr("Invalid NoRecipientAction: %s", val);
		break;

	  case O_SAFEFILEENV:	/* chroot() environ for writing to files */
		SafeFileEnv = newstr(val);
		break;

	  case O_MAXMSGSIZE:	/* maximum message size */
		MaxMessageSize = atol(val);
		break;

	  case O_COLONOKINADDR:	/* old style handling of colon addresses */
		ColonOkInAddr = atobool(val);
		break;

	  case O_MAXQUEUERUN:	/* max # of jobs in a single queue run */
		MaxQueueRun = atol(val);
		break;

	  case O_MAXCHILDREN:	/* max # of children of daemon */
		MaxChildren = atoi(val);
		break;

#if _FFR_MAX_FORWARD_ENTRIES
	  case O_MAXFORWARD:	/* max # of forward entries */
		MaxForwardEntries = atoi(val);
		break;
#endif /* _FFR_MAX_FORWARD_ENTRIES */

	  case O_KEEPCNAMES:	/* don't expand CNAME records */
		DontExpandCnames = atobool(val);
		break;

	  case O_MUSTQUOTE:	/* must quote these characters in phrases */
		(void) strlcpy(buf, "@,;:\\()[]", sizeof buf);
		if (strlen(val) < (SIZE_T) sizeof buf - 10)
			(void) strlcat(buf, val, sizeof buf);
		else
			printf("Warning: MustQuoteChars too long, ignored.\n");
		MustQuoteChars = newstr(buf);
		break;

	  case O_SMTPGREETING:	/* SMTP greeting message (old $e macro) */
		SmtpGreeting = newstr(munchstring(val, NULL, '\0'));
		break;

	  case O_UNIXFROM:	/* UNIX From_ line (old $l macro) */
		UnixFromLine = newstr(munchstring(val, NULL, '\0'));
		break;

	  case O_OPCHARS:	/* operator characters (old $o macro) */
		if (OperatorChars != NULL)
			printf("Warning: OperatorChars is being redefined.\n         It should only be set before ruleset definitions.\n");
		OperatorChars = newstr(munchstring(val, NULL, '\0'));
		break;

	  case O_DONTINITGRPS:	/* don't call initgroups(3) */
		DontInitGroups = atobool(val);
		break;

	  case O_SLFH:		/* make sure from fits on one line */
		SingleLineFromHeader = atobool(val);
		break;

	  case O_ABH:		/* allow HELO commands with syntax errors */
		AllowBogusHELO = atobool(val);
		break;

	  case O_CONNTHROT:	/* connection rate throttle */
		ConnRateThrottle = atoi(val);
		break;

	  case O_UGW:		/* group writable files are unsafe */
		if (!atobool(val))
		{
			setbitn(DBS_GROUPWRITABLEFORWARDFILESAFE,
				DontBlameSendmail);
			setbitn(DBS_GROUPWRITABLEINCLUDEFILESAFE,
				DontBlameSendmail);
		}
		break;

	  case O_DBLBOUNCE:	/* address to which to send double bounces */
		if (val[0] != '\0')
			DoubleBounceAddr = newstr(val);
		else
			syserr("readcf: option DoubleBounceAddress: value required");
		break;

	  case O_HSDIR:		/* persistent host status directory */
		if (val[0] != '\0')
			HostStatDir = newstr(val);
		break;

	  case O_SINGTHREAD:	/* single thread deliveries (requires hsdir) */
		SingleThreadDelivery = atobool(val);
		break;

	  case O_RUNASUSER:	/* run bulk of code as this user */
		for (p = val; *p != '\0'; p++)
		{
			if (*p == '.' || *p == '/' || *p == ':')
			{
				*p++ = '\0';
				break;
			}
		}
		if (isascii(*val) && isdigit(*val))
		{
			if (can_setuid)
				RunAsUid = atoi(val);
		}
		else
		{
			register struct passwd *pw;

			pw = sm_getpwnam(val);
			if (pw == NULL)
				syserr("readcf: option RunAsUser: unknown user %s", val);
			else if (can_setuid)
			{
				if (*p == '\0')
					RunAsUserName = newstr(val);
				RunAsUid = pw->pw_uid;
				RunAsGid = pw->pw_gid;
			}
		}
#ifdef UID_MAX
		if (RunAsUid > UID_MAX)
		{
			syserr("readcf: option RunAsUser: uid value (%ld) > UID_MAX (%ld); ignored",
				RunAsUid, UID_MAX);
		}
#endif /* UID_MAX */
		if (*p != '\0')
		{
			if (isascii(*p) && isdigit(*p))
			{
				if (can_setuid)
					RunAsGid = atoi(p);
			}
			else
			{
				register struct group *gr;

				gr = getgrnam(p);
				if (gr == NULL)
					syserr("readcf: option RunAsUser: unknown group %s",
						p);
				else if (can_setuid)
					RunAsGid = gr->gr_gid;
			}
		}
		if (tTd(47, 5))
			dprintf("readcf: RunAsUser = %d:%d\n",
				(int)RunAsUid, (int)RunAsGid);
		break;

	  case O_DSN_RRT:
		RrtImpliesDsn = atobool(val);
		break;

	  case O_PIDFILE:
		if (PidFile != NULL)
			free(PidFile);
		PidFile = newstr(val);
		break;

	case O_DONTBLAMESENDMAIL:
		p = val;
		for (;;)
		{
			register struct dbsval *dbs;
			extern struct dbsval DontBlameSendmailValues[];

			while (isascii(*p) && (isspace(*p) || ispunct(*p)))
				p++;
			if (*p == '\0')
				break;
			val = p;
			while (isascii(*p) && isalnum(*p))
				p++;
			if (*p != '\0')
				*p++ = '\0';

			for (dbs = DontBlameSendmailValues;
			     dbs->dbs_name != NULL; dbs++)
			{
				if (strcasecmp(val, dbs->dbs_name) == 0)
					break;
			}
			if (dbs->dbs_name == NULL)
				syserr("readcf: DontBlameSendmail option: %s unrecognized", val);
			else if (dbs->dbs_flag == DBS_SAFE)
				clrbitmap(DontBlameSendmail);
			else
				setbitn(dbs->dbs_flag, DontBlameSendmail);
		}
		sticky = FALSE;
		break;

	  case O_DPI:
		DontProbeInterfaces = atobool(val);
		break;

	  case O_MAXRCPT:
		MaxRcptPerMsg = atoi(val);
		break;

	  case O_DEADLETTER:
		if (DeadLetterDrop != NULL)
			free(DeadLetterDrop);
		DeadLetterDrop = newstr(val);
		break;

#if _FFR_DONTLOCKFILESFORREAD_OPTION
	  case O_DONTLOCK:
		DontLockReadFiles = atobool(val);
		break;
#endif /* _FFR_DONTLOCKFILESFORREAD_OPTION */

	  case O_MAXALIASRCSN:
		MaxAliasRecursion = atoi(val);
		break;

	  case O_CNCTONLYTO:
		/* XXX should probably use gethostbyname */
#if NETINET || NETINET6
# if NETINET6
		if (inet_addr(val) == INADDR_NONE)
		{
			ConnectOnlyTo.sa.sa_family = AF_INET6;
			if (inet_pton(AF_INET6, val,
				      &ConnectOnlyTo.sin6.sin6_addr) != 1)
				syserr("readcf: option ConnectOnlyTo: invalid IP address %s",
				       val);
		}
		else
# endif /* NETINET6 */
		{
			ConnectOnlyTo.sa.sa_family = AF_INET;
			ConnectOnlyTo.sin.sin_addr.s_addr = inet_addr(val);
		}
#endif /* NETINET || NETINET6 */
		break;

	  case O_TRUSTUSER:
#if HASFCHOWN
		if (isascii(*val) && isdigit(*val))
			TrustedUid = atoi(val);
		else
		{
			register struct passwd *pw;

			TrustedUid = 0;
			pw = sm_getpwnam(val);
			if (pw == NULL)
				syserr("readcf: option TrustedUser: unknown user %s", val);
			else
				TrustedUid = pw->pw_uid;
		}

# ifdef UID_MAX
		if (TrustedUid > UID_MAX)
		{
			syserr("readcf: option TrustedUser: uid value (%ld) > UID_MAX (%ld)",
				TrustedUid, UID_MAX);
			TrustedUid = 0;
		}
# endif /* UID_MAX */
#else /* HASFCHOWN */
		syserr("readcf: option TrustedUser: can not be used on systems which do not support fchown()");
#endif /* HASFCHOWN */
		break;

	  case O_MAXMIMEHDRLEN:
		p = strchr(val, '/');
		if (p != NULL)
			*p++ = '\0';
		MaxMimeHeaderLength = atoi(val);
		if (p != NULL && *p != '\0')
			MaxMimeFieldLength = atoi(p);
		else
			MaxMimeFieldLength = MaxMimeHeaderLength / 2;

		if (MaxMimeHeaderLength < 0)
			MaxMimeHeaderLength = 0;
		else if (MaxMimeHeaderLength < 128)
			printf("Warning: MaxMimeHeaderLength: header length limit set lower than 128\n");

		if (MaxMimeFieldLength < 0)
			MaxMimeFieldLength = 0;
		else if (MaxMimeFieldLength < 40)
			printf("Warning: MaxMimeHeaderLength: field length limit set lower than 40\n");
		break;

	  case O_CONTROLSOCKET:
		if (ControlSocketName != NULL)
			free(ControlSocketName);
		ControlSocketName = newstr(val);
		break;

	  case O_MAXHDRSLEN:
		MaxHeadersLength = atoi(val);

		if (MaxHeadersLength > 0 &&
		    MaxHeadersLength < (MAXHDRSLEN / 2))
			printf("Warning: MaxHeadersLength: headers length limit set lower than %d\n", (MAXHDRSLEN / 2));
		break;

	  case O_PROCTITLEPREFIX:
		if (ProcTitlePrefix != NULL)
			free(ProcTitlePrefix);
		ProcTitlePrefix = newstr(val);
		break;

#if SASL
	  case O_SASLINFO:
#if _FFR_ALLOW_SASLINFO
		/*
		**  Allow users to select their own authinfo file.
		**  However, this is not a "perfect" solution.
		**  If mail is queued, the authentication info
		**  will not be used in subsequent delivery attempts.
		**  If we really want to support this, then it has
		**  to be stored in the queue file.
		*/
		if (!bitset(SUBMIT_MSA, SubmitMode) && RealUid != 0 &&
		    RunAsUid != RealUid)
		{
			errno = 0;
			syserr("Error: %s only allowed with -U\n",
				o->o_name == NULL ? "<unknown>" : o->o_name);
			ExitStat = EX_USAGE;
			break;
		}
#endif /* _FFR_ALLOW_SASLINFO */
		if (SASLInfo != NULL)
			free(SASLInfo);
		SASLInfo = newstr(val);
		break;

	  case O_SASLMECH:
		if (AuthMechanisms != NULL)
			free(AuthMechanisms);
		if (*val != '\0')
			AuthMechanisms = newstr(val);
		else
			AuthMechanisms = NULL;
		break;

	  case O_SASLOPTS:
		while (val != NULL && *val != '\0')
		{
			switch(*val)
			{
			  case 'A':
				SASLOpts |= SASL_AUTH_AUTH;
				break;
# if _FFR_SASL_OPTS
			  case 'a':
				SASLOpts |= SASL_SEC_NOACTIVE;
				break;
			  case 'c':
				SASLOpts |= SASL_SEC_PASS_CREDENTIALS;
				break;
			  case 'd':
				SASLOpts |= SASL_SEC_NODICTIONARY;
				break;
			  case 'f':
				SASLOpts |= SASL_SEC_FORWARD_SECRECY;
				break;
			  case 'p':
				SASLOpts |= SASL_SEC_NOPLAINTEXT;
				break;
			  case 'y':
				SASLOpts |= SASL_SEC_NOANONYMOUS;
				break;
# endif /* _FFR_SASL_OPTS */
			  default:
				printf("Warning: Option: %s unknown parameter '%c'\n",
					o->o_name == NULL ? "<unknown>"
							  : o->o_name,
					(isascii(*val) && isprint(*val)) ? *val
									 : '?');
				break;
			}
			++val;
			val = strpbrk(val, ", \t");
			if (val != NULL)
				++val;
		}
		break;

#else /* SASL */
	  case O_SASLINFO:
	  case O_SASLMECH:
	  case O_SASLOPTS:
		printf("Warning: Option: %s requires SASL support (-DSASL)\n",
			o->o_name == NULL ? "<unknown>" : o->o_name);
		break;
#endif /* SASL */

#if STARTTLS
	  case O_SRVCERTFILE:
		if (SrvCERTfile != NULL)
			free(SrvCERTfile);
		SrvCERTfile = newstr(val);
		break;

	  case O_SRVKEYFILE:
		if (Srvkeyfile != NULL)
			free(Srvkeyfile);
		Srvkeyfile = newstr(val);
		break;

	  case O_CLTCERTFILE:
		if (CltCERTfile != NULL)
			free(CltCERTfile);
		CltCERTfile = newstr(val);
		break;

	  case O_CLTKEYFILE:
		if (Cltkeyfile != NULL)
			free(Cltkeyfile);
		Cltkeyfile = newstr(val);
		break;

	  case O_CACERTFILE:
		if (CACERTfile != NULL)
			free(CACERTfile);
		CACERTfile = newstr(val);
		break;

	  case O_CACERTPATH:
		if (CACERTpath != NULL)
			free(CACERTpath);
		CACERTpath = newstr(val);
		break;

	  case O_DHPARAMS:
		if (DHParams != NULL)
			free(DHParams);
		DHParams = newstr(val);
		break;

#  if _FFR_TLS_1
	  case O_DHPARAMS5:
		if (DHParams5 != NULL)
			free(DHParams5);
		DHParams5 = newstr(val);
		break;

	  case O_CIPHERLIST:
		if (CipherList != NULL)
			free(CipherList);
		CipherList = newstr(val);
		break;
#  endif /* _FFR_TLS_1 */

	  case O_RANDFILE:
		if (RandFile != NULL)
			free(RandFile);
		RandFile= newstr(val);
		break;

# else /* STARTTLS */
	  case O_SRVCERTFILE:
	  case O_SRVKEYFILE:
	  case O_CLTCERTFILE:
	  case O_CLTKEYFILE:
	  case O_CACERTFILE:
	  case O_CACERTPATH:
	  case O_DHPARAMS:
#  if _FFR_TLS_1
	  case O_DHPARAMS5:
	  case O_CIPHERLIST:
#  endif /* _FFR_TLS_1 */
	  case O_RANDFILE:
		printf("Warning: Option: %s requires TLS support\n",
			o->o_name == NULL ? "<unknown>" : o->o_name);
		break;

# endif /* STARTTLS */

	  case O_CLIENTPORT:
#if DAEMON
		setclientoptions(val);
#else /* DAEMON */
		syserr("ClientPortOptions (O option) set but DAEMON not compiled in");
#endif /* DAEMON */
		break;

	  case O_DF_BUFSIZE:
		DataFileBufferSize = atoi(val);
		break;

	  case O_XF_BUFSIZE:
		XscriptFileBufferSize = atoi(val);
		break;

	  case O_LDAPDEFAULTSPEC:
#ifdef LDAPMAP
		ldapmap_set_defaults(val);
#else /* LDAPMAP */
		printf("Warning: Option: %s requires LDAP support (-DLDAPMAP)\n",
			o->o_name == NULL ? "<unknown>" : o->o_name);
#endif /* LDAPMAP */
		break;

#if _FFR_MILTER
	  case O_INPUTMILTER:
		InputFilterList = newstr(val);
		break;

	  case O_MILTER:
		milter_set_option(subopt, val, sticky);
		break;
#endif /* _FFR_MILTER */

#if _FFR_QUEUE_FILE_MODE
	  case O_QUEUE_FILE_MODE:	/* queue file mode */
		QueueFileMode = atooct(val) & 0777;
		break;
#endif /* _FFR_QUEUE_FILE_MODE */

	  default:
		if (tTd(37, 1))
		{
			if (isascii(opt) && isprint(opt))
				dprintf("Warning: option %c unknown\n", opt);
			else
				dprintf("Warning: option 0x%x unknown\n", opt);
		}
		break;
	}

	/*
	**  Options with suboptions are responsible for taking care
	**  of sticky-ness (e.g., that a command line setting is kept
	**  when reading in the sendmail.cf file).  This has to be done
	**  when the suboptions are parsed since each suboption must be
	**  sticky, not the root option.
	*/

	if (sticky && !bitset(OI_SUBOPT, o->o_flags))
		setbitn(opt, StickyOpt);
}
/*
**  SETCLASS -- set a string into a class
**
**	Parameters:
**		class -- the class to put the string in.
**		str -- the string to enter
**
**	Returns:
**		none.
**
**	Side Effects:
**		puts the word into the symbol table.
*/

void
setclass(class, str)
	int class;
	char *str;
{
	register STAB *s;

	if ((*str & 0377) == MATCHCLASS)
	{
		int mid;

		str++;
		mid = macid(str, NULL);
		if (mid == '\0')
			return;

		if (tTd(37, 8))
			dprintf("setclass(%s, $=%s)\n",
				macname(class), macname(mid));
		copy_class(mid, class);
	}
	else
	{
		if (tTd(37, 8))
			dprintf("setclass(%s, %s)\n", macname(class), str);

		s = stab(str, ST_CLASS, ST_ENTER);
		setbitn(class, s->s_class);
	}
}
/*
**  MAKEMAPENTRY -- create a map entry
**
**	Parameters:
**		line -- the config file line
**
**	Returns:
**		A pointer to the map that has been created.
**		NULL if there was a syntax error.
**
**	Side Effects:
**		Enters the map into the dictionary.
*/

MAP *
makemapentry(line)
	char *line;
{
	register char *p;
	char *mapname;
	char *classname;
	register STAB *s;
	STAB *class;

	for (p = line; isascii(*p) && isspace(*p); p++)
		continue;
	if (!(isascii(*p) && isalnum(*p)))
	{
		syserr("readcf: config K line: no map name");
		return NULL;
	}

	mapname = p;
	while ((isascii(*++p) && isalnum(*p)) || *p == '_' || *p == '.')
		continue;
	if (*p != '\0')
		*p++ = '\0';
	while (isascii(*p) && isspace(*p))
		p++;
	if (!(isascii(*p) && isalnum(*p)))
	{
		syserr("readcf: config K line, map %s: no map class", mapname);
		return NULL;
	}
	classname = p;
	while (isascii(*++p) && isalnum(*p))
		continue;
	if (*p != '\0')
		*p++ = '\0';
	while (isascii(*p) && isspace(*p))
		p++;

	/* look up the class */
	class = stab(classname, ST_MAPCLASS, ST_FIND);
	if (class == NULL)
	{
		syserr("readcf: map %s: class %s not available", mapname, classname);
		return NULL;
	}

	/* enter the map */
	s = stab(mapname, ST_MAP, ST_ENTER);
	s->s_map.map_class = &class->s_mapclass;
	s->s_map.map_mname = newstr(mapname);

	if (class->s_mapclass.map_parse(&s->s_map, p))
		s->s_map.map_mflags |= MF_VALID;

	if (tTd(37, 5))
	{
		dprintf("map %s, class %s, flags %lx, file %s,\n",
			s->s_map.map_mname, s->s_map.map_class->map_cname,
			s->s_map.map_mflags,
			s->s_map.map_file == NULL ? "(null)" : s->s_map.map_file);
		dprintf("\tapp %s, domain %s, rebuild %s\n",
			s->s_map.map_app == NULL ? "(null)" : s->s_map.map_app,
			s->s_map.map_domain == NULL ? "(null)" : s->s_map.map_domain,
			s->s_map.map_rebuild == NULL ? "(null)" : s->s_map.map_rebuild);
	}

	return &s->s_map;
}
/*
**  STRTORWSET -- convert string to rewriting set number
**
**	Parameters:
**		p -- the pointer to the string to decode.
**		endp -- if set, store the trailing delimiter here.
**		stabmode -- ST_ENTER to create this entry, ST_FIND if
**			it must already exist.
**
**	Returns:
**		The appropriate ruleset number.
**		-1 if it is not valid (error already printed)
*/

int
strtorwset(p, endp, stabmode)
	char *p;
	char **endp;
	int stabmode;
{
	int ruleset;
	static int nextruleset = MAXRWSETS;

	while (isascii(*p) && isspace(*p))
		p++;
	if (!isascii(*p))
	{
		syserr("invalid ruleset name: \"%.20s\"", p);
		return -1;
	}
	if (isdigit(*p))
	{
		ruleset = strtol(p, endp, 10);
		if (ruleset >= MAXRWSETS / 2 || ruleset < 0)
		{
			syserr("bad ruleset %d (%d max)",
				ruleset, MAXRWSETS / 2);
			ruleset = -1;
		}
	}
	else
	{
		STAB *s;
		char delim;
		char *q = NULL;

		q = p;
		while (*p != '\0' && isascii(*p) &&
		       (isalnum(*p) || *p == '_'))
			p++;
		if (q == p || !(isascii(*q) && isalpha(*q)))
		{
			/* no valid characters */
			syserr("invalid ruleset name: \"%.20s\"", q);
			return -1;
		}
		while (isascii(*p) && isspace(*p))
			*p++ = '\0';
		delim = *p;
		if (delim != '\0')
			*p = '\0';
		s = stab(q, ST_RULESET, stabmode);
		if (delim != '\0')
			*p = delim;

		if (s == NULL)
			return -1;

		if (stabmode == ST_ENTER && delim == '=')
		{
			while (isascii(*++p) && isspace(*p))
				continue;
			if (!(isascii(*p) && isdigit(*p)))
			{
				syserr("bad ruleset definition \"%s\" (number required after `=')", q);
				ruleset = -1;
			}
			else
			{
				ruleset = strtol(p, endp, 10);
				if (ruleset >= MAXRWSETS / 2 || ruleset < 0)
				{
					syserr("bad ruleset number %d in \"%s\" (%d max)",
						ruleset, q, MAXRWSETS / 2);
					ruleset = -1;
				}
			}
		}
		else
		{
			if (endp != NULL)
				*endp = p;
			if (s->s_ruleset >= 0)
				ruleset = s->s_ruleset;
			else if ((ruleset = --nextruleset) < MAXRWSETS / 2)
			{
				syserr("%s: too many named rulesets (%d max)",
					q, MAXRWSETS / 2);
				ruleset = -1;
			}
		}
		if (s->s_ruleset >= 0 &&
		    ruleset >= 0 &&
		    ruleset != s->s_ruleset)
		{
			syserr("%s: ruleset changed value (old %d, new %d)",
				q, s->s_ruleset, ruleset);
			ruleset = s->s_ruleset;
		}
		else if (ruleset >= 0)
		{
			s->s_ruleset = ruleset;
		}
		if (stabmode == ST_ENTER)
		{
			char *h = NULL;

			if (RuleSetNames[ruleset] != NULL)
				free(RuleSetNames[ruleset]);
			if (delim != '\0' && (h = strchr(q, delim)) != NULL)
				*h = '\0';
			RuleSetNames[ruleset] = newstr(q);
			if (delim == '/' && h != NULL)
				*h = delim;	/* put back delim */
		}
	}
	return ruleset;
}
/*
**  SETTIMEOUT -- set an individual timeout
**
**	Parameters:
**		name -- the name of the timeout.
**		val -- the value of the timeout.
**		sticky -- if set, don't let other setoptions override
**			this value.
**
**	Returns:
**		none.
*/

/* set if Timeout sub-option is stuck */
static BITMAP256	StickyTimeoutOpt;

static struct timeoutinfo
{
	char	*to_name;	/* long name of timeout */
	u_char	to_code;	/* code for option */
} TimeOutTab[] =
{
#define TO_INITIAL			0x01
	{ "initial",			TO_INITIAL			},
#define TO_MAIL				0x02
	{ "mail",			TO_MAIL				},
#define TO_RCPT				0x03
	{ "rcpt",			TO_RCPT				},
#define TO_DATAINIT			0x04
	{ "datainit",			TO_DATAINIT			},
#define TO_DATABLOCK			0x05
	{ "datablock",			TO_DATABLOCK			},
#define TO_DATAFINAL			0x06
	{ "datafinal",			TO_DATAFINAL			},
#define TO_COMMAND			0x07
	{ "command",			TO_COMMAND			},
#define TO_RSET				0x08
	{ "rset",			TO_RSET				},
#define TO_HELO				0x09
	{ "helo",			TO_HELO				},
#define TO_QUIT				0x0A
	{ "quit",			TO_QUIT				},
#define TO_MISC				0x0B
	{ "misc",			TO_MISC				},
#define TO_IDENT			0x0C
	{ "ident",			TO_IDENT			},
#define TO_FILEOPEN			0x0D
	{ "fileopen",			TO_FILEOPEN			},
#define TO_CONNECT			0x0E
	{ "connect",			TO_CONNECT			},
#define TO_ICONNECT			0x0F
	{ "iconnect",			TO_ICONNECT			},
#define TO_QUEUEWARN			0x10
	{ "queuewarn",			TO_QUEUEWARN			},
	{ "queuewarn.*",		TO_QUEUEWARN			},
#define TO_QUEUEWARN_NORMAL		0x11
	{ "queuewarn.normal",		TO_QUEUEWARN_NORMAL		},
#define TO_QUEUEWARN_URGENT		0x12
	{ "queuewarn.urgent",		TO_QUEUEWARN_URGENT		},
#define TO_QUEUEWARN_NON_URGENT		0x13
	{ "queuewarn.non-urgent",	TO_QUEUEWARN_NON_URGENT		},
#define TO_QUEUERETURN			0x14
	{ "queuereturn",		TO_QUEUERETURN			},
	{ "queuereturn.*",		TO_QUEUERETURN			},
#define TO_QUEUERETURN_NORMAL		0x15
	{ "queuereturn.normal",		TO_QUEUERETURN_NORMAL		},
#define TO_QUEUERETURN_URGENT		0x16
	{ "queuereturn.urgent",		TO_QUEUERETURN_URGENT		},
#define TO_QUEUERETURN_NON_URGENT	0x17
	{ "queuereturn.non-urgent",	TO_QUEUERETURN_NON_URGENT	},
#define TO_HOSTSTATUS			0x18
	{ "hoststatus",			TO_HOSTSTATUS			},
#define TO_RESOLVER_RETRANS		0x19
	{ "resolver.retrans",		TO_RESOLVER_RETRANS		},
#define TO_RESOLVER_RETRANS_NORMAL	0x1A
	{ "resolver.retrans.normal",	TO_RESOLVER_RETRANS_NORMAL	},
#define TO_RESOLVER_RETRANS_FIRST	0x1B
	{ "resolver.retrans.first",	TO_RESOLVER_RETRANS_FIRST	},
#define TO_RESOLVER_RETRY		0x1C
	{ "resolver.retry",		TO_RESOLVER_RETRY		},
#define TO_RESOLVER_RETRY_NORMAL	0x1D
	{ "resolver.retry.normal",	TO_RESOLVER_RETRY_NORMAL	},
#define TO_RESOLVER_RETRY_FIRST		0x1E
	{ "resolver.retry.first",	TO_RESOLVER_RETRY_FIRST		},
#define TO_CONTROL			0x1F
	{ "control",			TO_CONTROL			},
	{ NULL,				0				},
};


static void
settimeout(name, val, sticky)
	char *name;
	char *val;
	bool sticky;
{
	register struct timeoutinfo *to;
	int i;
	time_t toval;

	if (tTd(37, 2))
		dprintf("settimeout(%s = %s)", name, val);

	for (to = TimeOutTab; to->to_name != NULL; to++)
	{
		if (strcasecmp(to->to_name, name) == 0)
			break;
	}

	if (to->to_name == NULL)
		syserr("settimeout: invalid timeout %s", name);

	/*
	**  See if this option is preset for us.
	*/

	if (!sticky && bitnset(to->to_code, StickyTimeoutOpt))
	{
		if (tTd(37, 2))
			dprintf(" (ignored)\n");
		return;
	}

	if (tTd(37, 2))
		dprintf("\n");

	toval = convtime(val, 'm');

	switch (to->to_code)
	{
	  case TO_INITIAL:
		TimeOuts.to_initial = toval;
		break;

	  case TO_MAIL:
		TimeOuts.to_mail = toval;
		break;

	  case TO_RCPT:
		TimeOuts.to_rcpt = toval;
		break;

	  case TO_DATAINIT:
		TimeOuts.to_datainit = toval;
		break;

	  case TO_DATABLOCK:
		TimeOuts.to_datablock = toval;
		break;

	  case TO_DATAFINAL:
		TimeOuts.to_datafinal = toval;
		break;

	  case TO_COMMAND:
		TimeOuts.to_nextcommand = toval;
		break;

	  case TO_RSET:
		TimeOuts.to_rset = toval;
		break;

	  case TO_HELO:
		TimeOuts.to_helo = toval;
		break;

	  case TO_QUIT:
		TimeOuts.to_quit = toval;
		break;

	  case TO_MISC:
		TimeOuts.to_miscshort = toval;
		break;

	  case TO_IDENT:
		TimeOuts.to_ident = toval;
		break;

	  case TO_FILEOPEN:
		TimeOuts.to_fileopen = toval;
		break;

	  case TO_CONNECT:
		TimeOuts.to_connect = toval;
		break;

	  case TO_ICONNECT:
		TimeOuts.to_iconnect = toval;
		break;

	  case TO_QUEUEWARN:
		toval = convtime(val, 'h');
		TimeOuts.to_q_warning[TOC_NORMAL] = toval;
		TimeOuts.to_q_warning[TOC_URGENT] = toval;
		TimeOuts.to_q_warning[TOC_NONURGENT] = toval;
		break;

	  case TO_QUEUEWARN_NORMAL:
		toval = convtime(val, 'h');
		TimeOuts.to_q_warning[TOC_NORMAL] = toval;
		break;

	  case TO_QUEUEWARN_URGENT:
		toval = convtime(val, 'h');
		TimeOuts.to_q_warning[TOC_URGENT] = toval;
		break;

	  case TO_QUEUEWARN_NON_URGENT:
		toval = convtime(val, 'h');
		TimeOuts.to_q_warning[TOC_NONURGENT] = toval;
		break;

	  case TO_QUEUERETURN:
		toval = convtime(val, 'd');
		TimeOuts.to_q_return[TOC_NORMAL] = toval;
		TimeOuts.to_q_return[TOC_URGENT] = toval;
		TimeOuts.to_q_return[TOC_NONURGENT] = toval;
		break;

	  case TO_QUEUERETURN_NORMAL:
		toval = convtime(val, 'd');
		TimeOuts.to_q_return[TOC_NORMAL] = toval;
		break;

	  case TO_QUEUERETURN_URGENT:
		toval = convtime(val, 'd');
		TimeOuts.to_q_return[TOC_URGENT] = toval;
		break;

	  case TO_QUEUERETURN_NON_URGENT:
		toval = convtime(val, 'd');
		TimeOuts.to_q_return[TOC_NONURGENT] = toval;
		break;


	  case TO_HOSTSTATUS:
		MciInfoTimeout = toval;
		break;

	  case TO_RESOLVER_RETRANS:
		toval = convtime(val, 's');
		TimeOuts.res_retrans[RES_TO_DEFAULT] = toval;
		TimeOuts.res_retrans[RES_TO_FIRST] = toval;
		TimeOuts.res_retrans[RES_TO_NORMAL] = toval;
		break;

	  case TO_RESOLVER_RETRY:
		i = atoi(val);
		TimeOuts.res_retry[RES_TO_DEFAULT] = i;
		TimeOuts.res_retry[RES_TO_FIRST] = i;
		TimeOuts.res_retry[RES_TO_NORMAL] = i;
		break;

	  case TO_RESOLVER_RETRANS_NORMAL:
		TimeOuts.res_retrans[RES_TO_NORMAL] = convtime(val, 's');
		break;

	  case TO_RESOLVER_RETRY_NORMAL:
		TimeOuts.res_retry[RES_TO_NORMAL] = atoi(val);
		break;

	  case TO_RESOLVER_RETRANS_FIRST:
		TimeOuts.res_retrans[RES_TO_FIRST] = convtime(val, 's');
		break;

	  case TO_RESOLVER_RETRY_FIRST:
		TimeOuts.res_retry[RES_TO_FIRST] = atoi(val);
		break;

	  case TO_CONTROL:
		TimeOuts.to_control = toval;
		break;

	  default:
		syserr("settimeout: invalid timeout %s", name);
		break;
	}

	if (sticky)
		setbitn(to->to_code, StickyTimeoutOpt);
}
/*
**  INITTIMEOUTS -- parse and set timeout values
**
**	Parameters:
**		val -- a pointer to the values.  If NULL, do initial
**			settings.
**		sticky -- if set, don't let other setoptions override
**			this suboption value.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Initializes the TimeOuts structure
*/

void
inittimeouts(val, sticky)
	register char *val;
	bool sticky;
{
	register char *p;

	if (tTd(37, 2))
		dprintf("inittimeouts(%s)\n", val == NULL ? "<NULL>" : val);
	if (val == NULL)
	{
		TimeOuts.to_connect = (time_t) 0 SECONDS;
		TimeOuts.to_initial = (time_t) 5 MINUTES;
		TimeOuts.to_helo = (time_t) 5 MINUTES;
		TimeOuts.to_mail = (time_t) 10 MINUTES;
		TimeOuts.to_rcpt = (time_t) 1 HOUR;
		TimeOuts.to_datainit = (time_t) 5 MINUTES;
		TimeOuts.to_datablock = (time_t) 1 HOUR;
		TimeOuts.to_datafinal = (time_t) 1 HOUR;
		TimeOuts.to_rset = (time_t) 5 MINUTES;
		TimeOuts.to_quit = (time_t) 2 MINUTES;
		TimeOuts.to_nextcommand = (time_t) 1 HOUR;
		TimeOuts.to_miscshort = (time_t) 2 MINUTES;
#if IDENTPROTO
		TimeOuts.to_ident = (time_t) 5 SECONDS;
#else /* IDENTPROTO */
		TimeOuts.to_ident = (time_t) 0 SECONDS;
#endif /* IDENTPROTO */
		TimeOuts.to_fileopen = (time_t) 60 SECONDS;
		TimeOuts.to_control = (time_t) 2 MINUTES;
		if (tTd(37, 5))
		{
			dprintf("Timeouts:\n");
			dprintf("  connect = %ld\n", (long)TimeOuts.to_connect);
			dprintf("  initial = %ld\n", (long)TimeOuts.to_initial);
			dprintf("  helo = %ld\n", (long)TimeOuts.to_helo);
			dprintf("  mail = %ld\n", (long)TimeOuts.to_mail);
			dprintf("  rcpt = %ld\n", (long)TimeOuts.to_rcpt);
			dprintf("  datainit = %ld\n", (long)TimeOuts.to_datainit);
			dprintf("  datablock = %ld\n", (long)TimeOuts.to_datablock);
			dprintf("  datafinal = %ld\n", (long)TimeOuts.to_datafinal);
			dprintf("  rset = %ld\n", (long)TimeOuts.to_rset);
			dprintf("  quit = %ld\n", (long)TimeOuts.to_quit);
			dprintf("  nextcommand = %ld\n", (long)TimeOuts.to_nextcommand);
			dprintf("  miscshort = %ld\n", (long)TimeOuts.to_miscshort);
			dprintf("  ident = %ld\n", (long)TimeOuts.to_ident);
			dprintf("  fileopen = %ld\n", (long)TimeOuts.to_fileopen);
			dprintf("  control = %ld\n", (long)TimeOuts.to_control);
		}
		return;
	}

	for (;; val = p)
	{
		while (isascii(*val) && isspace(*val))
			val++;
		if (*val == '\0')
			break;
		for (p = val; *p != '\0' && *p != ','; p++)
			continue;
		if (*p != '\0')
			*p++ = '\0';

		if (isascii(*val) && isdigit(*val))
		{
			/* old syntax -- set everything */
			TimeOuts.to_mail = convtime(val, 'm');
			TimeOuts.to_rcpt = TimeOuts.to_mail;
			TimeOuts.to_datainit = TimeOuts.to_mail;
			TimeOuts.to_datablock = TimeOuts.to_mail;
			TimeOuts.to_datafinal = TimeOuts.to_mail;
			TimeOuts.to_nextcommand = TimeOuts.to_mail;
			if (sticky)
			{
				setbitn(TO_MAIL, StickyTimeoutOpt);
				setbitn(TO_RCPT, StickyTimeoutOpt);
				setbitn(TO_DATAINIT, StickyTimeoutOpt);
				setbitn(TO_DATABLOCK, StickyTimeoutOpt);
				setbitn(TO_DATAFINAL, StickyTimeoutOpt);
				setbitn(TO_COMMAND, StickyTimeoutOpt);
			}
			continue;
		}
		else
		{
			register char *q = strchr(val, ':');

			if (q == NULL && (q = strchr(val, '=')) == NULL)
			{
				/* syntax error */
				continue;
			}
			*q++ = '\0';
			settimeout(val, q, sticky);
		}
	}
}
