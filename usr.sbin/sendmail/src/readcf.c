/*
 * Copyright (c) 1983 Eric P. Allman
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

#ifndef lint
static char sccsid[] = "@(#)readcf.c	8.23 (Berkeley) 3/18/94";
#endif /* not lint */

# include "sendmail.h"
# include <pwd.h>
# include <grp.h>
#if NAMED_BIND
# include <arpa/nameser.h>
# include <resolv.h>
#endif

/*
**  READCF -- read control file.
**
**	This routine reads the control file and builds the internal
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
**
**	Parameters:
**		cfname -- control file name.
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

readcf(cfname, safe, e)
	char *cfname;
	bool safe;
	register ENVELOPE *e;
{
	FILE *cf;
	int ruleset = 0;
	char *q;
	struct rewrite *rwp = NULL;
	char *bp;
	auto char *ep;
	int nfuzzy;
	char *file;
	bool optional;
	char buf[MAXLINE];
	register char *p;
	extern char **copyplist();
	struct stat statb;
	char exbuf[MAXLINE];
	char pvpbuf[MAXLINE + MAXATOM];
	extern char *munchstring();
	extern void makemapentry();

	FileName = cfname;
	LineNumber = 0;

	cf = fopen(cfname, "r");
	if (cf == NULL)
	{
		syserr("cannot open");
		exit(EX_OSFILE);
	}

	if (fstat(fileno(cf), &statb) < 0)
	{
		syserr("cannot fstat");
		exit(EX_OSFILE);
	}

	if (!S_ISREG(statb.st_mode))
	{
		syserr("not a plain file");
		exit(EX_OSFILE);
	}

	if (OpMode != MD_TEST && bitset(S_IWGRP|S_IWOTH, statb.st_mode))
	{
		if (OpMode == MD_DAEMON || OpMode == MD_FREEZE)
			fprintf(stderr, "%s: WARNING: dangerous write permissions\n",
				FileName);
#ifdef LOG
		if (LogLevel > 0)
			syslog(LOG_CRIT, "%s: WARNING: dangerous write permissions",
				FileName);
#endif
	}

#ifdef XLA
	xla_zero();
#endif

	while ((bp = fgetfolded(buf, sizeof buf, cf)) != NULL)
	{
		if (bp[0] == '#')
		{
			if (bp != buf)
				free(bp);
			continue;
		}

		/* map $ into \201 for macro expansion */
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
					(void) strcpy(p, p + 1);
					break;

				  default:
					/* delete preceeding white space */
					while (isascii(*p) && isspace(*p) && p > bp)
						p--;
					if ((e = strchr(++p, '\n')) != NULL)
						(void) strcpy(p, e);
					else
						p[0] = p[1] = '\0';
					break;
				}
				continue;
			}

			if (*p != '$')
				continue;

			if (p[1] == '$')
			{
				/* actual dollar sign.... */
				(void) strcpy(p, p + 1);
				continue;
			}

			/* convert to macro expansion character */
			*p = MACROEXPAND;
		}

		/* interpret this line */
		errno = 0;
		switch (bp[0])
		{
		  case '\0':
		  case '#':		/* comment */
			break;

		  case 'R':		/* rewriting rule */
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
			expand(&bp[1], exbuf, &exbuf[sizeof exbuf], e);
			rwp->r_lhs = prescan(exbuf, '\t', pvpbuf,
					     sizeof pvpbuf, NULL);
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

					  case CANONNET:
						botch = "$#";
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

					  case CONDELSE:
						botch = "$|";
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
			}
			else
				syserr("R line: null LHS");

			/* expand and save the RHS */
			while (*++p == '\t')
				continue;
			q = p;
			while (*p != '\0' && *p != '\t')
				p++;
			*p = '\0';
			expand(q, exbuf, &exbuf[sizeof exbuf], e);
			rwp->r_rhs = prescan(exbuf, '\t', pvpbuf,
					     sizeof pvpbuf, NULL);
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
				syserr("R line: null RHS");
			break;

		  case 'S':		/* select rewriting set */
			for (p = &bp[1]; isascii(*p) && isspace(*p); p++)
				continue;
			if (!isascii(*p) || !isdigit(*p))
			{
				syserr("invalid argument to S line: \"%.20s\"", 
					&bp[1]);
				break;
			}
			ruleset = atoi(p);
			if (ruleset >= MAXRWSETS || ruleset < 0)
			{
				syserr("bad ruleset %d (%d max)", ruleset, MAXRWSETS);
				ruleset = 0;
			}
			rwp = NULL;
			break;

		  case 'D':		/* macro definition */
			p = munchstring(&bp[2], NULL);
			define(bp[1], newstr(p), e);
			break;

		  case 'H':		/* required header line */
			(void) chompheader(&bp[1], TRUE, e);
			break;

		  case 'C':		/* word class */
			/* scan the list of words and set class for all */
			expand(&bp[2], exbuf, &exbuf[sizeof exbuf], e);
			for (p = exbuf; *p != '\0'; )
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
					setclass(bp[1], wd);
				*p = delim;
			}
			break;

		  case 'F':		/* word class from file */
			for (p = &bp[2]; isascii(*p) && isspace(*p); )
				p++;
			if (p[0] == '-' && p[1] == 'o')
			{
				optional = TRUE;
				while (*p != '\0' && !(isascii(*p) && isspace(*p)))
					p++;
				while (isascii(*p) && isspace(*p))
					*p++;
			}
			else
				optional = FALSE;
			file = p;
			while (*p != '\0' && !(isascii(*p) && isspace(*p)))
				p++;
			if (*p == '\0')
				p = "%s";
			else
			{
				*p = '\0';
				while (isascii(*++p) && isspace(*p))
					continue;
			}
			fileclass(bp[1], file, p, safe, optional);
			break;

#ifdef XLA
		  case 'L':		/* extended load average description */
			xla_init(&bp[1]);
			break;
#endif

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
			for (p = &bp[1]; *p != '\0' && *p != '=' && *p != '\t'; p++)
				continue;
			if (*p == '\0')
				goto badline;
			*p = '\0';
			Priorities[NumPriorities].pri_name = newstr(&bp[1]);
			Priorities[NumPriorities].pri_val = atoi(++p);
			NumPriorities++;
			break;

		  case 'T':		/* trusted user(s) */
			/* this option is obsolete, but will be ignored */
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
			if (ConfigLevel >= 5)
			{
				/* level 5 configs have short name in $w */
				p = macvalue('w', e);
				if (p != NULL && (p = strchr(p, '.')) != NULL)
					*p = '\0';
			}
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
			makemapentry(&bp[1]);
			break;

		  default:
		  badline:
			syserr("unknown control line \"%s\"", bp);
		}
		if (bp != buf)
			free(bp);
	}
	if (ferror(cf))
	{
		syserr("I/O read error", cfname);
		exit(EX_OSFILE);
	}
	fclose(cf);
	FileName = NULL;

	if (stab("host", ST_MAP, ST_FIND) == NULL)
	{
		/* user didn't initialize: set up host map */
		strcpy(buf, "host host");
#if NAMED_BIND
		if (ConfigLevel >= 2)
			strcat(buf, " -a.");
#endif
		makemapentry(buf);
	}
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

toomany(id, maxcnt)
	char id;
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

fileclass(class, filename, fmt, safe, optional)
	int class;
	char *filename;
	char *fmt;
	bool safe;
	bool optional;
{
	FILE *f;
	struct stat stbuf;
	char buf[MAXLINE];

	if (tTd(37, 2))
		printf("fileclass(%s, fmt=%s)\n", filename, fmt);

	if (filename[0] == '|')
	{
		syserr("fileclass: pipes (F%c%s) not supported due to security problems",
			class, filename);
		return;
	}
	if (stat(filename, &stbuf) < 0)
	{
		if (tTd(37, 2))
			printf("  cannot stat (%s)\n", errstring(errno));
		if (!optional)
			syserr("fileclass: cannot stat %s", filename);
		return;
	}
	if (!S_ISREG(stbuf.st_mode))
	{
		syserr("fileclass: %s not a regular file", filename);
		return;
	}
	if (!safe && access(filename, R_OK) < 0)
	{
		syserr("fileclass: access denied on %s", filename);
		return;
	}
	f = fopen(filename, "r");
	if (f == NULL)
	{
		syserr("fileclass: cannot open %s", filename);
		return;
	}

	while (fgets(buf, sizeof buf, f) != NULL)
	{
		register STAB *s;
		register char *p;
# ifdef SCANF
		char wordbuf[MAXNAME+1];

		if (sscanf(buf, fmt, wordbuf) != 1)
			continue;
		p = wordbuf;
# else /* SCANF */
		p = buf;
# endif /* SCANF */

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
}
/*
**  MAKEMAILER -- define a new mailer.
**
**	Parameters:
**		line -- description of mailer.  This is in labeled
**			fields.  The fields are:
**			   P -- the path to the mailer
**			   F -- the flags associated with the mailer
**			   A -- the argv for this mailer
**			   S -- the sender rewriting set
**			   R -- the recipient rewriting set
**			   E -- the eol string
**			The first word is the canonical name of the mailer.
**
**	Returns:
**		none.
**
**	Side Effects:
**		enters the mailer into the mailer table.
*/

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
	extern char **makeargv();
	extern char *munchstring();
	extern long atol();

	/* allocate a mailer and set up defaults */
	m = (struct mailer *) xalloc(sizeof *m);
	bzero((char *) m, sizeof *m);
	m->m_eol = "\n";

	/* collect the mailer name */
	for (p = line; *p != '\0' && *p != ',' && !(isascii(*p) && isspace(*p)); p++)
		continue;
	if (*p != '\0')
		*p++ = '\0';
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
		p = munchstring(p, &delimptr);

		/* install the field into the mailer struct */
		switch (fcode)
		{
		  case 'P':		/* pathname */
			m->m_mailer = newstr(p);
			break;

		  case 'F':		/* flags */
			for (; *p != '\0'; p++)
				if (!(isascii(*p) && isspace(*p)))
					setbitn(*p, m->m_flags);
			break;

		  case 'S':		/* sender rewriting ruleset */
		  case 'R':		/* recipient rewriting ruleset */
			i = strtol(p, &endp, 10);
			if (i < 0 || i >= MAXRWSETS)
			{
				syserr("invalid rewrite set, %d max", MAXRWSETS);
				return;
			}
			if (fcode == 'S')
				m->m_sh_rwset = m->m_se_rwset = i;
			else
				m->m_rh_rwset = m->m_re_rwset = i;

			p = endp;
			if (*p++ == '/')
			{
				i = strtol(p, NULL, 10);
				if (i < 0 || i >= MAXRWSETS)
				{
					syserr("invalid rewrite set, %d max",
						MAXRWSETS);
					return;
				}
				if (fcode == 'S')
					m->m_sh_rwset = i;
				else
					m->m_rh_rwset = i;
			}
			break;

		  case 'E':		/* end of line string */
			m->m_eol = newstr(p);
			break;

		  case 'A':		/* argument vector */
			m->m_argv = makeargv(p);
			break;

		  case 'M':		/* maximum message size */
			m->m_maxsize = atol(p);
			break;

		  case 'L':		/* maximum line length */
			m->m_linelimit = atoi(p);
			break;

		  case 'D':		/* working directory */
			m->m_execdir = newstr(p);
			break;
		}

		p = delimptr;
	}

	/* do some heuristic cleanup for back compatibility */
	if (bitnset(M_LIMITS, m->m_flags))
	{
		if (m->m_linelimit == 0)
			m->m_linelimit = SMTPLINELIM;
		if (ConfigLevel < 2)
			setbitn(M_7BITS, m->m_flags);
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
**
**	Returns:
**		the munched string.
*/

char *
munchstring(p, delimptr)
	register char *p;
	char **delimptr;
{
	register char *q;
	bool backslash = FALSE;
	bool quotemode = FALSE;
	static char buf[MAXLINE];

	for (q = buf; *p != '\0'; p++)
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
			else if (quotemode || *p != ',')
				*q++ = *p;
			else
				break;
		}
	}

	if (delimptr != NULL)
		*delimptr = p;
	*q++ = '\0';
	return (buf);
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

char **
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
	bcopy((char *) argv, (char *) avp, sizeof *avp * i);

	return (avp);
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

static BITMAP	StickyOpt;		/* set if option is stuck */


#if NAMED_BIND

struct resolverflags
{
	char	*rf_name;	/* name of the flag */
	long	rf_bits;	/* bits to set/clear */
} ResolverFlags[] =
{
	"debug",	RES_DEBUG,
	"aaonly",	RES_AAONLY,
	"usevc",	RES_USEVC,
	"primary",	RES_PRIMARY,
	"igntc",	RES_IGNTC,
	"recurse",	RES_RECURSE,
	"defnames",	RES_DEFNAMES,
	"stayopen",	RES_STAYOPEN,
	"dnsrch",	RES_DNSRCH,
	"true",		0,		/* to avoid error on old syntax */
	NULL,		0
};

#endif

setoption(opt, val, safe, sticky, e)
	char opt;
	char *val;
	bool safe;
	bool sticky;
	register ENVELOPE *e;
{
	register char *p;
	extern bool atobool();
	extern time_t convtime();
	extern int QueueLA;
	extern int RefuseLA;
	extern bool Warn_Q_option;
	extern bool trusteduser();

	if (tTd(37, 1))
		printf("setoption %c=%s", opt, val);

	/*
	**  See if this option is preset for us.
	*/

	if (!sticky && bitnset(opt, StickyOpt))
	{
		if (tTd(37, 1))
			printf(" (ignored)\n");
		return;
	}

	/*
	**  Check to see if this option can be specified by this user.
	*/

	if (!safe && RealUid == 0)
		safe = TRUE;
	if (!safe && strchr("bCdeijLmoprsvw7", opt) == NULL)
	{
		if (opt != 'M' || (val[0] != 'r' && val[0] != 's'))
		{
			if (tTd(37, 1))
				printf(" (unsafe)");
			if (RealUid != geteuid())
			{
				if (tTd(37, 1))
					printf("(Resetting uid)");
				(void) setgid(RealGid);
				(void) setuid(RealUid);
			}
		}
	}
	if (tTd(37, 1))
		printf("\n");

	switch (opt)
	{
	  case '7':		/* force seven-bit input */
		SevenBit = atobool(val);
		break;

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
			e->e_sendmode = SM_DELIVER;
			break;

		  case SM_QUEUE:	/* queue only */
#ifndef QUEUE
			syserr("need QUEUE to set -odqueue");
#endif /* QUEUE */
			/* fall through..... */

		  case SM_DELIVER:	/* do everything */
		  case SM_FORK:		/* fork after verification */
			e->e_sendmode = *val;
			break;

		  default:
			syserr("Unknown delivery mode %c", *val);
			exit(EX_USAGE);
		}
		break;

	  case 'D':		/* rebuild alias database as needed */
		AutoRebuild = atobool(val);
		break;

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
			HoldErrs = TRUE;
			/* fall through... */

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
		if (isascii(*val) && isdigit(*val))
			DefGid = atoi(val);
		else
		{
			register struct group *gr;

			DefGid = -1;
			gr = getgrnam(val);
			if (gr == NULL)
				syserr("readcf: option g: unknown group %s", val);
			else
				DefGid = gr->gr_gid;
		}
		break;

	  case 'H':		/* help file */
		if (val[0] == '\0')
			HelpFile = "sendmail.hf";
		else
			HelpFile = newstr(val);
		break;

	  case 'h':		/* maximum hop count */
		MaxHopCount = atoi(val);
		break;

	  case 'I':		/* use internet domain name server */
#if NAMED_BIND
		UseNameServer = TRUE;
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
			printf("_res.options = %x\n", _res.options);
#else
		usrerr("name server (I option) specified but BIND not compiled in");
#endif
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
		define(val[0], newstr(&val[1]), CurEnv);
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
		setdaemonoptions(val);
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
		settimeouts(val);
		break;

	  case 'S':		/* status file */
		if (val[0] == '\0')
			StatFile = "sendmail.st";
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
			TimeOuts.to_q_warning = convtime(p, 'd');
		}
		TimeOuts.to_q_return = convtime(val, 'h');
		break;

	  case 't':		/* time zone name */
		TimeZoneSpec = newstr(val);
		break;

	  case 'U':		/* location of user database */
		UdbSpec = newstr(val);
		break;

	  case 'u':		/* set default uid */
		if (isascii(*val) && isdigit(*val))
			DefUid = atoi(val);
		else
		{
			register struct passwd *pw;

			DefUid = -1;
			pw = getpwnam(val);
			if (pw == NULL)
				syserr("readcf: option u: unknown user %s", val);
			else
				DefUid = pw->pw_uid;
		}
		setdefuser();
		break;

	  case 'V':		/* fallback MX host */
		FallBackMX = newstr(val);
		break;

	  case 'v':		/* run in verbose mode */
		Verbose = atobool(val);
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

	  default:
		break;
	}
	if (sticky)
		setbitn(opt, StickyOpt);
	return;
}
/*
**  SETCLASS -- set a word into a class
**
**	Parameters:
**		class -- the class to put the word in.
**		word -- the word to enter
**
**	Returns:
**		none.
**
**	Side Effects:
**		puts the word into the symbol table.
*/

setclass(class, word)
	int class;
	char *word;
{
	register STAB *s;

	if (tTd(37, 8))
		printf("setclass(%c, %s)\n", class, word);
	s = stab(word, ST_CLASS, ST_ENTER);
	setbitn(class, s->s_class);
}
/*
**  MAKEMAPENTRY -- create a map entry
**
**	Parameters:
**		line -- the config file line
**
**	Returns:
**		TRUE if it successfully entered the map entry.
**		FALSE otherwise (usually syntax error).
**
**	Side Effects:
**		Enters the map into the dictionary.
*/

void
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
		return;
	}

	mapname = p;
	while (isascii(*++p) && isalnum(*p))
		continue;
	if (*p != '\0')
		*p++ = '\0';
	while (isascii(*p) && isspace(*p))
		p++;
	if (!(isascii(*p) && isalnum(*p)))
	{
		syserr("readcf: config K line, map %s: no map class", mapname);
		return;
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
		return;
	}

	/* enter the map */
	s = stab(mapname, ST_MAP, ST_ENTER);
	s->s_map.map_class = &class->s_mapclass;
	s->s_map.map_mname = newstr(mapname);

	if (class->s_mapclass.map_parse(&s->s_map, p))
		s->s_map.map_mflags |= MF_VALID;

	if (tTd(37, 5))
	{
		printf("map %s, class %s, flags %x, file %s,\n",
			s->s_map.map_mname, s->s_map.map_class->map_cname,
			s->s_map.map_mflags,
			s->s_map.map_file == NULL ? "(null)" : s->s_map.map_file);
		printf("\tapp %s, domain %s, rebuild %s\n",
			s->s_map.map_app == NULL ? "(null)" : s->s_map.map_app,
			s->s_map.map_domain == NULL ? "(null)" : s->s_map.map_domain,
			s->s_map.map_rebuild == NULL ? "(null)" : s->s_map.map_rebuild);
	}
}
/*
**  SETTIMEOUTS -- parse and set timeout values
**
**	Parameters:
**		val -- a pointer to the values.  If NULL, do initial
**			settings.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Initializes the TimeOuts structure
*/

#define SECONDS
#define MINUTES	* 60
#define HOUR	* 3600

settimeouts(val)
	register char *val;
{
	register char *p;
	extern time_t convtime();

	if (val == NULL)
	{
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
		TimeOuts.to_ident = (time_t) 30 SECONDS;
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
			continue;
		}
		else
		{
			register char *q = strchr(val, '=');
			time_t to;

			if (q == NULL)
			{
				/* syntax error */
				continue;
			}
			*q++ = '\0';
			to = convtime(q, 'm');

			if (strcasecmp(val, "initial") == 0)
				TimeOuts.to_initial = to;
			else if (strcasecmp(val, "mail") == 0)
				TimeOuts.to_mail = to;
			else if (strcasecmp(val, "rcpt") == 0)
				TimeOuts.to_rcpt = to;
			else if (strcasecmp(val, "datainit") == 0)
				TimeOuts.to_datainit = to;
			else if (strcasecmp(val, "datablock") == 0)
				TimeOuts.to_datablock = to;
			else if (strcasecmp(val, "datafinal") == 0)
				TimeOuts.to_datafinal = to;
			else if (strcasecmp(val, "command") == 0)
				TimeOuts.to_nextcommand = to;
			else if (strcasecmp(val, "rset") == 0)
				TimeOuts.to_rset = to;
			else if (strcasecmp(val, "helo") == 0)
				TimeOuts.to_helo = to;
			else if (strcasecmp(val, "quit") == 0)
				TimeOuts.to_quit = to;
			else if (strcasecmp(val, "misc") == 0)
				TimeOuts.to_miscshort = to;
			else if (strcasecmp(val, "ident") == 0)
				TimeOuts.to_ident = to;
			else
				syserr("settimeouts: invalid timeout %s", val);
		}
	}
}
