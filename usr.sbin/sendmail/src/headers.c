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
static char sccsid[] = "@(#)headers.c	8.2 (Berkeley) 7/11/93";
#endif /* not lint */

# include <errno.h>
# include "sendmail.h"

/*
**  CHOMPHEADER -- process and save a header line.
**
**	Called by collect and by readcf to deal with header lines.
**
**	Parameters:
**		line -- header as a text line.
**		def -- if set, this is a default value.
**		e -- the envelope including this header.
**
**	Returns:
**		flags for this header.
**
**	Side Effects:
**		The header is saved on the header list.
**		Contents of 'line' are destroyed.
*/

chompheader(line, def, e)
	char *line;
	bool def;
	register ENVELOPE *e;
{
	register char *p;
	register HDR *h;
	HDR **hp;
	char *fname;
	char *fvalue;
	struct hdrinfo *hi;
	bool cond = FALSE;
	BITMAP mopts;

	if (tTd(31, 6))
		printf("chompheader: %s\n", line);

	/* strip off options */
	clrbitmap(mopts);
	p = line;
	if (*p == '?')
	{
		/* have some */
		register char *q = strchr(p + 1, *p);
		
		if (q != NULL)
		{
			*q++ = '\0';
			while (*++p != '\0')
				setbitn(*p, mopts);
			p = q;
		}
		else
			usrerr("553 header syntax error, line \"%s\"", line);
		cond = TRUE;
	}

	/* find canonical name */
	fname = p;
	p = strchr(p, ':');
	if (p == NULL)
	{
		syserr("553 header syntax error, line \"%s\"", line);
		return (0);
	}
	fvalue = &p[1];
	while (isascii(*--p) && isspace(*p))
		continue;
	*++p = '\0';

	/* strip field value on front */
	if (*fvalue == ' ')
		fvalue++;

	/* see if it is a known type */
	for (hi = HdrInfo; hi->hi_field != NULL; hi++)
	{
		if (strcasecmp(hi->hi_field, fname) == 0)
			break;
	}

	/* see if this is a resent message */
	if (!def && bitset(H_RESENT, hi->hi_flags))
		e->e_flags |= EF_RESENT;

	/* if this means "end of header" quit now */
	if (bitset(H_EOH, hi->hi_flags))
		return (hi->hi_flags);

	/* drop explicit From: if same as what we would generate -- for MH */
	p = "resent-from";
	if (!bitset(EF_RESENT, e->e_flags))
		p += 7;
	if (!def && !bitset(EF_QUEUERUN, e->e_flags) && strcasecmp(fname, p) == 0)
	{
		if (tTd(31, 2))
		{
			printf("comparing header from (%s) against default (%s or %s)\n",
				fvalue, e->e_from.q_paddr, e->e_from.q_user);
		}
		if (e->e_from.q_paddr != NULL &&
		    (strcmp(fvalue, e->e_from.q_paddr) == 0 ||
		     strcmp(fvalue, e->e_from.q_user) == 0))
			return (hi->hi_flags);
	}

	/* delete default value for this header */
	for (hp = &e->e_header; (h = *hp) != NULL; hp = &h->h_link)
	{
		if (strcasecmp(fname, h->h_field) == 0 &&
		    bitset(H_DEFAULT, h->h_flags) &&
		    !bitset(H_FORCE, h->h_flags))
			h->h_value = NULL;
	}

	/* create a new node */
	h = (HDR *) xalloc(sizeof *h);
	h->h_field = newstr(fname);
	h->h_value = NULL;
	h->h_link = NULL;
	bcopy((char *) mopts, (char *) h->h_mflags, sizeof mopts);
	*hp = h;
	h->h_flags = hi->hi_flags;
	if (def)
		h->h_flags |= H_DEFAULT;
	if (cond)
		h->h_flags |= H_CHECK;
	if (h->h_value != NULL)
		free((char *) h->h_value);
	h->h_value = newstr(fvalue);

	/* hack to see if this is a new format message */
	if (!def && bitset(H_RCPT|H_FROM, h->h_flags) &&
	    (strchr(fvalue, ',') != NULL || strchr(fvalue, '(') != NULL ||
	     strchr(fvalue, '<') != NULL || strchr(fvalue, ';') != NULL))
	{
		e->e_flags &= ~EF_OLDSTYLE;
	}

	return (h->h_flags);
}
/*
**  ADDHEADER -- add a header entry to the end of the queue.
**
**	This bypasses the special checking of chompheader.
**
**	Parameters:
**		field -- the name of the header field.
**		value -- the value of the field.
**		e -- the envelope to add them to.
**
**	Returns:
**		none.
**
**	Side Effects:
**		adds the field on the list of headers for this envelope.
*/

addheader(field, value, e)
	char *field;
	char *value;
	ENVELOPE *e;
{
	register HDR *h;
	register struct hdrinfo *hi;
	HDR **hp;

	/* find info struct */
	for (hi = HdrInfo; hi->hi_field != NULL; hi++)
	{
		if (strcasecmp(field, hi->hi_field) == 0)
			break;
	}

	/* find current place in list -- keep back pointer? */
	for (hp = &e->e_header; (h = *hp) != NULL; hp = &h->h_link)
	{
		if (strcasecmp(field, h->h_field) == 0)
			break;
	}

	/* allocate space for new header */
	h = (HDR *) xalloc(sizeof *h);
	h->h_field = field;
	h->h_value = newstr(value);
	h->h_link = *hp;
	h->h_flags = hi->hi_flags | H_DEFAULT;
	clrbitmap(h->h_mflags);
	*hp = h;
}
/*
**  HVALUE -- return value of a header.
**
**	Only "real" fields (i.e., ones that have not been supplied
**	as a default) are used.
**
**	Parameters:
**		field -- the field name.
**		e -- the envelope containing the header.
**
**	Returns:
**		pointer to the value part.
**		NULL if not found.
**
**	Side Effects:
**		none.
*/

char *
hvalue(field, e)
	char *field;
	register ENVELOPE *e;
{
	register HDR *h;

	for (h = e->e_header; h != NULL; h = h->h_link)
	{
		if (!bitset(H_DEFAULT, h->h_flags) &&
		    strcasecmp(h->h_field, field) == 0)
			return (h->h_value);
	}
	return (NULL);
}
/*
**  ISHEADER -- predicate telling if argument is a header.
**
**	A line is a header if it has a single word followed by
**	optional white space followed by a colon.
**
**	Parameters:
**		s -- string to check for possible headerness.
**
**	Returns:
**		TRUE if s is a header.
**		FALSE otherwise.
**
**	Side Effects:
**		none.
*/

bool
isheader(s)
	register char *s;
{
	while (*s > ' ' && *s != ':' && *s != '\0')
		s++;

	/* following technically violates RFC822 */
	while (isascii(*s) && isspace(*s))
		s++;

	return (*s == ':');
}
/*
**  EATHEADER -- run through the stored header and extract info.
**
**	Parameters:
**		e -- the envelope to process.
**		full -- if set, do full processing (e.g., compute
**			message priority).
**
**	Returns:
**		none.
**
**	Side Effects:
**		Sets a bunch of global variables from information
**			in the collected header.
**		Aborts the message if the hop count is exceeded.
*/

eatheader(e, full)
	register ENVELOPE *e;
	bool full;
{
	register HDR *h;
	register char *p;
	int hopcnt = 0;
	char *msgid;
	char buf[MAXLINE];

	/*
	**  Set up macros for possible expansion in headers.
	*/

	define('f', e->e_sender, e);
	define('g', e->e_sender, e);

	if (tTd(32, 1))
		printf("----- collected header -----\n");
	msgid = "<none>";
	for (h = e->e_header; h != NULL; h = h->h_link)
	{
		/* do early binding */
		if (bitset(H_DEFAULT, h->h_flags) && h->h_value != NULL)
		{
			expand(h->h_value, buf, &buf[sizeof buf], e);
			if (buf[0] != '\0')
			{
				h->h_value = newstr(buf);
				h->h_flags &= ~H_DEFAULT;
			}
		}

		if (tTd(32, 1))
			printf("%s: %s\n", h->h_field, h->h_value);

		/* count the number of times it has been processed */
		if (bitset(H_TRACE, h->h_flags))
			hopcnt++;

		/* send to this person if we so desire */
		if (GrabTo && bitset(H_RCPT, h->h_flags) &&
		    !bitset(H_DEFAULT, h->h_flags) &&
		    (!bitset(EF_RESENT, e->e_flags) || bitset(H_RESENT, h->h_flags)))
		{
			(void) sendtolist(h->h_value, (ADDRESS *) NULL,
					  &e->e_sendqueue, e);
		}

		/* save the message-id for logging */
		if (full && h->h_value != NULL &&
		    strcasecmp(h->h_field, "message-id") == 0)
		{
			msgid = h->h_value;
			while (isascii(*msgid) && isspace(*msgid))
				msgid++;
		}

		/* see if this is a return-receipt header */
		if (bitset(H_RECEIPTTO, h->h_flags))
			e->e_receiptto = h->h_value;

		/* see if this is an errors-to header */
		if (UseErrorsTo && bitset(H_ERRORSTO, h->h_flags))
			(void) sendtolist(h->h_value, (ADDRESS *) NULL,
					  &e->e_errorqueue, e);
	}
	if (tTd(32, 1))
		printf("----------------------------\n");

	/* if we are just verifying (that is, sendmail -t -bv), drop out now */
	if (OpMode == MD_VERIFY)
		return;

	/* store hop count */
	if (hopcnt > e->e_hopcount)
		e->e_hopcount = hopcnt;

	/* message priority */
	p = hvalue("precedence", e);
	if (p != NULL)
		e->e_class = priencode(p);
	if (full)
		e->e_msgpriority = e->e_msgsize
				 - e->e_class * WkClassFact
				 + e->e_nrcpts * WkRecipFact;

	/* full name of from person */
	p = hvalue("full-name", e);
	if (p != NULL)
		define('x', p, e);

	/* date message originated */
	p = hvalue("posted-date", e);
	if (p == NULL)
		p = hvalue("date", e);
	if (p != NULL)
		define('a', p, e);

	/*
	**  Log collection information.
	*/

# ifdef LOG
	if (full && LogLevel > 4)
	{
		char *name;
		register char *sbp;
		char hbuf[MAXNAME];
		char sbuf[MAXLINE];

		if (bitset(EF_RESPONSE, e->e_flags))
			name = "[RESPONSE]";
		else if ((name = macvalue('_', e)) != NULL)
			;
		else if (RealHostName[0] == '[')
			name = RealHostName;
		else
		{
			name = hbuf;
			(void) sprintf(hbuf, "%.80s", RealHostName);
			if (RealHostAddr.sa.sa_family != 0)
			{
				p = &hbuf[strlen(hbuf)];
				(void) sprintf(p, " (%s)",
					anynet_ntoa(&RealHostAddr));
			}
		}

		/* some versions of syslog only take 5 printf args */
		sbp = sbuf;
		sprintf(sbp, "from=%.200s, size=%ld, class=%d, pri=%ld, nrcpts=%d, msgid=%.100s",
		    e->e_from.q_paddr, e->e_msgsize, e->e_class,
		    e->e_msgpriority, e->e_nrcpts, msgid);
		sbp += strlen(sbp);
		if (e->e_bodytype != NULL)
		{
			(void) sprintf(sbp, ", bodytype=%.20s", e->e_bodytype);
			sbp += strlen(sbp);
		}
		p = macvalue('r', e);
		if (p != NULL)
			(void) sprintf(sbp, ", proto=%.20s", p);
		syslog(LOG_INFO, "%s: %s, relay=%s",
		    e->e_id, sbuf, name);
	}
# endif /* LOG */
}
/*
**  PRIENCODE -- encode external priority names into internal values.
**
**	Parameters:
**		p -- priority in ascii.
**
**	Returns:
**		priority as a numeric level.
**
**	Side Effects:
**		none.
*/

priencode(p)
	char *p;
{
	register int i;

	for (i = 0; i < NumPriorities; i++)
	{
		if (!strcasecmp(p, Priorities[i].pri_name))
			return (Priorities[i].pri_val);
	}

	/* unknown priority */
	return (0);
}
/*
**  CRACKADDR -- parse an address and turn it into a macro
**
**	This doesn't actually parse the address -- it just extracts
**	it and replaces it with "$g".  The parse is totally ad hoc
**	and isn't even guaranteed to leave something syntactically
**	identical to what it started with.  However, it does leave
**	something semantically identical.
**
**	This algorithm has been cleaned up to handle a wider range
**	of cases -- notably quoted and backslash escaped strings.
**	This modification makes it substantially better at preserving
**	the original syntax.
**
**	Parameters:
**		addr -- the address to be cracked.
**
**	Returns:
**		a pointer to the new version.
**
**	Side Effects:
**		none.
**
**	Warning:
**		The return value is saved in local storage and should
**		be copied if it is to be reused.
*/

char *
crackaddr(addr)
	register char *addr;
{
	register char *p;
	register char c;
	int cmtlev;
	int realcmtlev;
	int anglelev, realanglelev;
	int copylev;
	bool qmode;
	bool realqmode;
	bool skipping;
	bool putgmac = FALSE;
	bool quoteit = FALSE;
	register char *bp;
	char *buflim;
	static char buf[MAXNAME];

	if (tTd(33, 1))
		printf("crackaddr(%s)\n", addr);

	/* strip leading spaces */
	while (*addr != '\0' && isascii(*addr) && isspace(*addr))
		addr++;

	/*
	**  Start by assuming we have no angle brackets.  This will be
	**  adjusted later if we find them.
	*/

	bp = buf;
	buflim = &buf[sizeof buf - 5];
	p = addr;
	copylev = anglelev = realanglelev = cmtlev = realcmtlev = 0;
	qmode = realqmode = FALSE;

	while ((c = *p++) != '\0')
	{
		/*
		**  If the buffer is overful, go into a special "skipping"
		**  mode that tries to keep legal syntax but doesn't actually
		**  output things.
		*/

		skipping = bp >= buflim;

		if (copylev > 0 && !skipping)
			*bp++ = c;

		/* check for backslash escapes */
		if (c == '\\')
		{
			/* arrange to quote the address */
			if (cmtlev <= 0 && !qmode)
				quoteit = TRUE;

			if ((c = *p++) == '\0')
			{
				/* too far */
				p--;
				goto putg;
			}
			if (copylev > 0 && !skipping)
				*bp++ = c;
			goto putg;
		}

		/* check for quoted strings */
		if (c == '"')
		{
			qmode = !qmode;
			if (copylev > 0 && !skipping)
				realqmode = !realqmode;
			continue;
		}
		if (qmode)
			goto putg;

		/* check for comments */
		if (c == '(')
		{
			cmtlev++;

			/* allow space for closing paren */
			if (!skipping)
			{
				buflim--;
				realcmtlev++;
				if (copylev++ <= 0)
				{
					*bp++ = ' ';
					*bp++ = c;
				}
			}
		}
		if (cmtlev > 0)
		{
			if (c == ')')
			{
				cmtlev--;
				copylev--;
				if (!skipping)
				{
					realcmtlev--;
					buflim++;
				}
			}
			continue;
		}
		else if (c == ')')
		{
			/* syntax error: unmatched ) */
			if (!skipping)
				bp--;
		}


		/* check for characters that may have to be quoted */
		if (strchr(".'@,;:\\()", c) != NULL)
		{
			/*
			**  If these occur as the phrase part of a <>
			**  construct, but are not inside of () or already
			**  quoted, they will have to be quoted.  Note that
			**  now (but don't actually do the quoting).
			*/

			if (cmtlev <= 0 && !qmode)
				quoteit = TRUE;
		}

		/* check for angle brackets */
		if (c == '<')
		{
			register char *q;

			/* oops -- have to change our mind */
			anglelev++;
			if (!skipping)
				realanglelev++;

			bp = buf;
			if (quoteit)
			{
				*bp++ = '"';

				/* back up over the '<' and any spaces */
				--p;
				while (isascii(*--p) && isspace(*p))
					continue;
				p++;
			}
			for (q = addr; q < p; )
			{
				c = *q++;
				if (bp < buflim)
				{
					if (quoteit && c == '"')
						*bp++ = '\\';
					*bp++ = c;
				}
			}
			if (quoteit)
			{
				*bp++ = '"';
				while ((c = *p++) != '<')
				{
					if (bp < buflim)
						*bp++ = c;
				}
				*bp++ = c;
			}
			copylev = 0;
			putgmac = quoteit = FALSE;
			continue;
		}

		if (c == '>')
		{
			if (anglelev > 0)
			{
				anglelev--;
				if (!skipping)
				{
					realanglelev--;
					buflim++;
				}
			}
			else if (!skipping)
			{
				/* syntax error: unmatched > */
				if (copylev > 0)
					bp--;
				continue;
			}
			if (copylev++ <= 0)
				*bp++ = c;
			continue;
		}

		/* must be a real address character */
	putg:
		if (copylev <= 0 && !putgmac)
		{
			*bp++ = MACROEXPAND;
			*bp++ = 'g';
			putgmac = TRUE;
		}
	}

	/* repair any syntactic damage */
	if (realqmode)
		*bp++ = '"';
	while (realcmtlev-- > 0)
		*bp++ = ')';
	while (realanglelev-- > 0)
		*bp++ = '>';
	*bp++ = '\0';

	if (tTd(33, 1))
		printf("crackaddr=>`%s'\n", buf);

	return (buf);
}
/*
**  PUTHEADER -- put the header part of a message from the in-core copy
**
**	Parameters:
**		fp -- file to put it on.
**		m -- mailer to use.
**		e -- envelope to use.
**
**	Returns:
**		none.
**
**	Side Effects:
**		none.
*/

/*
 * Macro for fast max (not available in e.g. DG/UX, 386/ix).
 */
#ifndef MAX
# define MAX(a,b) (((a)>(b))?(a):(b))
#endif

putheader(fp, m, e)
	register FILE *fp;
	register MAILER *m;
	register ENVELOPE *e;
{
	char buf[MAX(MAXLINE,BUFSIZ)];
	register HDR *h;
	char obuf[MAXLINE];

	if (tTd(34, 1))
		printf("--- putheader, mailer = %s ---\n", m->m_name);

	for (h = e->e_header; h != NULL; h = h->h_link)
	{
		register char *p;
		extern bool bitintersect();

		if (tTd(34, 11))
		{
			printf("  %s: ", h->h_field);
			xputs(h->h_value);
		}

		if (bitset(H_CHECK|H_ACHECK, h->h_flags) &&
		    !bitintersect(h->h_mflags, m->m_flags))
		{
			if (tTd(34, 11))
				printf(" (skipped)\n");
			continue;
		}

		/* handle Resent-... headers specially */
		if (bitset(H_RESENT, h->h_flags) && !bitset(EF_RESENT, e->e_flags))
		{
			if (tTd(34, 11))
				printf(" (skipped (resent))\n");
			continue;
		}
		if (tTd(34, 11))
			printf("\n");

		p = h->h_value;
		if (bitset(H_DEFAULT, h->h_flags))
		{
			/* macro expand value if generated internally */
			expand(p, buf, &buf[sizeof buf], e);
			p = buf;
			if (p == NULL || *p == '\0')
				continue;
		}

		if (bitset(H_FROM|H_RCPT, h->h_flags))
		{
			/* address field */
			bool oldstyle = bitset(EF_OLDSTYLE, e->e_flags);

			if (bitset(H_FROM, h->h_flags))
				oldstyle = FALSE;
			commaize(h, p, fp, oldstyle, m, e);
		}
		else
		{
			/* vanilla header line */
			register char *nlp;

			(void) sprintf(obuf, "%s: ", h->h_field);
			while ((nlp = strchr(p, '\n')) != NULL)
			{
				*nlp = '\0';
				(void) strcat(obuf, p);
				*nlp = '\n';
				putline(obuf, fp, m);
				p = ++nlp;
				obuf[0] = '\0';
			}
			(void) strcat(obuf, p);
			putline(obuf, fp, m);
		}
	}
}
/*
**  COMMAIZE -- output a header field, making a comma-translated list.
**
**	Parameters:
**		h -- the header field to output.
**		p -- the value to put in it.
**		fp -- file to put it to.
**		oldstyle -- TRUE if this is an old style header.
**		m -- a pointer to the mailer descriptor.  If NULL,
**			don't transform the name at all.
**		e -- the envelope containing the message.
**
**	Returns:
**		none.
**
**	Side Effects:
**		outputs "p" to file "fp".
*/

commaize(h, p, fp, oldstyle, m, e)
	register HDR *h;
	register char *p;
	FILE *fp;
	bool oldstyle;
	register MAILER *m;
	register ENVELOPE *e;
{
	register char *obp;
	int opos;
	bool firstone = TRUE;
	char obuf[MAXLINE + 3];

	/*
	**  Output the address list translated by the
	**  mailer and with commas.
	*/

	if (tTd(14, 2))
		printf("commaize(%s: %s)\n", h->h_field, p);

	obp = obuf;
	(void) sprintf(obp, "%s: ", h->h_field);
	opos = strlen(h->h_field) + 2;
	obp += opos;

	/*
	**  Run through the list of values.
	*/

	while (*p != '\0')
	{
		register char *name;
		register int c;
		char savechar;
		int flags;
		auto int stat;

		/*
		**  Find the end of the name.  New style names
		**  end with a comma, old style names end with
		**  a space character.  However, spaces do not
		**  necessarily delimit an old-style name -- at
		**  signs mean keep going.
		*/

		/* find end of name */
		while ((isascii(*p) && isspace(*p)) || *p == ',')
			p++;
		name = p;
		for (;;)
		{
			auto char *oldp;
			char pvpbuf[PSBUFSIZE];

			(void) prescan(p, oldstyle ? ' ' : ',', pvpbuf, &oldp);
			p = oldp;

			/* look to see if we have an at sign */
			while (*p != '\0' && isascii(*p) && isspace(*p))
				p++;

			if (*p != '@')
			{
				p = oldp;
				break;
			}
			p += *p == '@' ? 1 : 2;
			while (*p != '\0' && isascii(*p) && isspace(*p))
				p++;
		}
		/* at the end of one complete name */

		/* strip off trailing white space */
		while (p >= name &&
		       ((isascii(*p) && isspace(*p)) || *p == ',' || *p == '\0'))
			p--;
		if (++p == name)
			continue;
		savechar = *p;
		*p = '\0';

		/* translate the name to be relative */
		flags = RF_HEADERADDR|RF_ADDDOMAIN;
		if (bitset(H_FROM, h->h_flags))
			flags |= RF_SENDERADDR;
		stat = EX_OK;
		name = remotename(name, m, flags, &stat, e);
		if (*name == '\0')
		{
			*p = savechar;
			continue;
		}

		/* output the name with nice formatting */
		opos += strlen(name);
		if (!firstone)
			opos += 2;
		if (opos > 78 && !firstone)
		{
			(void) strcpy(obp, ",\n");
			putline(obuf, fp, m);
			obp = obuf;
			(void) sprintf(obp, "        ");
			opos = strlen(obp);
			obp += opos;
			opos += strlen(name);
		}
		else if (!firstone)
		{
			(void) sprintf(obp, ", ");
			obp += 2;
		}

		while ((c = *name++) != '\0' && obp < &obuf[MAXLINE])
			*obp++ = c;
		firstone = FALSE;
		*p = savechar;
	}
	(void) strcpy(obp, "\n");
	putline(obuf, fp, m);
}
/*
**  COPYHEADER -- copy header list
**
**	This routine is the equivalent of newstr for header lists
**
**	Parameters:
**		header -- list of header structures to copy.
**
**	Returns:
**		a copy of 'header'.
**
**	Side Effects:
**		none.
*/

HDR *
copyheader(header)
	register HDR *header;
{
	register HDR *newhdr;
	HDR *ret;
	register HDR **tail = &ret;

	while (header != NULL)
	{
		newhdr = (HDR *) xalloc(sizeof(HDR));
		STRUCTCOPY(*header, *newhdr);
		*tail = newhdr;
		tail = &newhdr->h_link;
		header = header->h_link;
	}
	*tail = NULL;
	
	return ret;
}
