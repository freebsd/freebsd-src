/*
 * Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
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
static char sccsid[] = "@(#)headers.c	8.134 (Berkeley) 11/29/1998";
#endif /* not lint */

# include <errno.h>
# include "sendmail.h"

/*
**  SETUPHEADERS -- initialize headers in symbol table
**
**	Parameters:
**		none
**
**	Returns:
**		none
*/

void
setupheaders()
{
	struct hdrinfo *hi;
	STAB *s;

	for (hi = HdrInfo; hi->hi_field != NULL; hi++)
	{
		s = stab(hi->hi_field, ST_HEADER, ST_ENTER);
		s->s_header.hi_flags = hi->hi_flags;
		s->s_header.hi_ruleset = NULL;
	}
}
/*
**  CHOMPHEADER -- process and save a header line.
**
**	Called by collect and by readcf to deal with header lines.
**
**	Parameters:
**		line -- header as a text line.
**		def -- if set, this is a default value.
**		hdrp -- a pointer to the place to save the header.
**		e -- the envelope including this header.
**
**	Returns:
**		flags for this header.
**
**	Side Effects:
**		The header is saved on the header list.
**		Contents of 'line' are destroyed.
*/

struct hdrinfo	NormalHeader =	{ NULL, 0, NULL };

int
chompheader(line, def, hdrp, e)
	char *line;
	bool def;
	HDR **hdrp;
	register ENVELOPE *e;
{
	register char *p;
	register HDR *h;
	HDR **hp;
	char *fname;
	char *fvalue;
	bool cond = FALSE;
	bool headeronly;
	STAB *s;
	struct hdrinfo *hi;
	BITMAP mopts;

	if (tTd(31, 6))
	{
		printf("chompheader: ");
		xputs(line);
		printf("\n");
	}

	headeronly = hdrp != NULL;
	if (!headeronly)
		hdrp = &e->e_header;

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
			syserr("553 header syntax error, line \"%s\"", line);
		cond = TRUE;
	}

	/* find canonical name */
	fname = p;
	while (isascii(*p) && isgraph(*p) && *p != ':')
		p++;
	fvalue = p;
	while (isascii(*p) && isspace(*p))
		p++;
	if (*p++ != ':' || fname == fvalue)
	{
		syserr("553 header syntax error, line \"%s\"", line);
		return 0;
	}
	*fvalue = '\0';
	fvalue = p;

	/* strip field value on front */
	if (*fvalue == ' ')
		fvalue++;

	/* security scan: long field names are end-of-header */
	if (strlen(fname) > 100)
		return H_EOH;

	/* check to see if it represents a ruleset call */
	if (def)
	{
		char hbuf[50];

		(void) expand(fvalue, hbuf, sizeof hbuf, e);
		for (p = hbuf; isascii(*p) && isspace(*p); )
			p++;
		if ((*p++ & 0377) == CALLSUBR)
		{
			auto char *endp;

			if (strtorwset(p, &endp, ST_ENTER) > 0)
			{
				*endp = '\0';
				s = stab(fname, ST_HEADER, ST_ENTER);
				s->s_header.hi_ruleset = newstr(p);
			}
			return 0;
		}
	}

	/* see if it is a known type */
	s = stab(fname, ST_HEADER, ST_FIND);
	if (s != NULL)
		hi = &s->s_header;
	else
		hi = &NormalHeader;

	if (tTd(31, 9))
	{
		if (s == NULL)
			printf("no header flags match\n");
		else
			printf("header match, flags=%x, ruleset=%s\n", 
				hi->hi_flags,
				hi->hi_ruleset == NULL ? "<NULL>" : hi->hi_ruleset);
	}

	/* see if this is a resent message */
	if (!def && !headeronly && bitset(H_RESENT, hi->hi_flags))
		e->e_flags |= EF_RESENT;

	/* if this is an Errors-To: header keep track of it now */
	if (UseErrorsTo && !def && !headeronly &&
	    bitset(H_ERRORSTO, hi->hi_flags))
		(void) sendtolist(fvalue, NULLADDR, &e->e_errorqueue, 0, e);

	/* if this means "end of header" quit now */
	if (!headeronly && bitset(H_EOH, hi->hi_flags))
		return hi->hi_flags;

	/*
	**  Horrible hack to work around problem with Lotus Notes SMTP
	**  mail gateway, which generates From: headers with newlines in
	**  them and the <address> on the second line.  Although this is
	**  legal RFC 822, many MUAs don't handle this properly and thus
	**  never find the actual address.
	*/

	if (bitset(H_FROM, hi->hi_flags) && SingleLineFromHeader)
	{
		while ((p = strchr(fvalue, '\n')) != NULL)
			*p = ' ';
	}

	/*
	**  If there is a check ruleset, verify it against the header.
	*/

	if (!def && hi->hi_ruleset != NULL)
		(void) rscheck(hi->hi_ruleset, fvalue, NULL, e);

	/*
	**  Drop explicit From: if same as what we would generate.
	**  This is to make MH (which doesn't always give a full name)
	**  insert the full name information in all circumstances.
	*/

	p = "resent-from";
	if (!bitset(EF_RESENT, e->e_flags))
		p += 7;
	if (!def && !headeronly && !bitset(EF_QUEUERUN, e->e_flags) &&
	    strcasecmp(fname, p) == 0)
	{
		if (tTd(31, 2))
		{
			printf("comparing header from (%s) against default (%s or %s)\n",
				fvalue, e->e_from.q_paddr, e->e_from.q_user);
		}
		if (e->e_from.q_paddr != NULL &&
		    (strcmp(fvalue, e->e_from.q_paddr) == 0 ||
		     strcmp(fvalue, e->e_from.q_user) == 0))
			return hi->hi_flags;
	}

	/* delete default value for this header */
	for (hp = hdrp; (h = *hp) != NULL; hp = &h->h_link)
	{
		if (strcasecmp(fname, h->h_field) == 0 &&
		    bitset(H_DEFAULT, h->h_flags) &&
		    !bitset(H_FORCE, h->h_flags))
		{
			h->h_value = NULL;
			if (!cond)
			{
				/* copy conditions from default case */
				bcopy((char *)h->h_mflags, (char *)mopts,
						sizeof mopts);
			}
		}
	}

	/* create a new node */
	h = (HDR *) xalloc(sizeof *h);
	h->h_field = newstr(fname);
	h->h_value = newstr(fvalue);
	h->h_link = NULL;
	bcopy((char *) mopts, (char *) h->h_mflags, sizeof mopts);
	*hp = h;
	h->h_flags = hi->hi_flags;

	/* strip EOH flag if parsing MIME headers */
	if (headeronly)
		h->h_flags &= ~H_EOH;
	if (def)
		h->h_flags |= H_DEFAULT;
	if (cond)
		h->h_flags |= H_CHECK;

	/* hack to see if this is a new format message */
	if (!def && !headeronly && bitset(H_RCPT|H_FROM, h->h_flags) &&
	    (strchr(fvalue, ',') != NULL || strchr(fvalue, '(') != NULL ||
	     strchr(fvalue, '<') != NULL || strchr(fvalue, ';') != NULL))
	{
		e->e_flags &= ~EF_OLDSTYLE;
	}

	return h->h_flags;
}
/*
**  ADDHEADER -- add a header entry to the end of the queue.
**
**	This bypasses the special checking of chompheader.
**
**	Parameters:
**		field -- the name of the header field.
**		value -- the value of the field.
**		hp -- an indirect pointer to the header structure list.
**
**	Returns:
**		none.
**
**	Side Effects:
**		adds the field on the list of headers for this envelope.
*/

void
addheader(field, value, hdrlist)
	char *field;
	char *value;
	HDR **hdrlist;
{
	register HDR *h;
	STAB *s;
	HDR **hp;

	/* find info struct */
	s = stab(field, ST_HEADER, ST_FIND);

	/* find current place in list -- keep back pointer? */
	for (hp = hdrlist; (h = *hp) != NULL; hp = &h->h_link)
	{
		if (strcasecmp(field, h->h_field) == 0)
			break;
	}

	/* allocate space for new header */
	h = (HDR *) xalloc(sizeof *h);
	h->h_field = field;
	h->h_value = newstr(value);
	h->h_link = *hp;
	h->h_flags = H_DEFAULT;
	if (s != NULL)
		h->h_flags |= s->s_header.hi_flags;
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
**		header -- the header list.
**
**	Returns:
**		pointer to the value part.
**		NULL if not found.
**
**	Side Effects:
**		none.
*/

char *
hvalue(field, header)
	char *field;
	HDR *header;
{
	register HDR *h;

	for (h = header; h != NULL; h = h->h_link)
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
**	Header fields beginning with two dashes, although technically
**	permitted by RFC822, are automatically rejected in order
**	to make MIME work out.  Without this we could have a technically
**	legal header such as ``--"foo:bar"'' that would also be a legal
**	MIME separator.
**
**	Parameters:
**		h -- string to check for possible headerness.
**
**	Returns:
**		TRUE if h is a header.
**		FALSE otherwise.
**
**	Side Effects:
**		none.
*/

bool
isheader(h)
	char *h;
{
	register char *s = h;

	if (s[0] == '-' && s[1] == '-')
		return FALSE;

	while (*s > ' ' && *s != ':' && *s != '\0')
		s++;

	if (h == s)
		return FALSE;

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
**			message priority).  This should not be set
**			when reading a queue file because some info
**			needed to compute the priority is wrong.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Sets a bunch of global variables from information
**			in the collected header.
**		Aborts the message if the hop count is exceeded.
*/

void
eatheader(e, full)
	register ENVELOPE *e;
	bool full;
{
	register HDR *h;
	register char *p;
	int hopcnt = 0;
	char *msgid;
	char buf[MAXLINE];
	extern int priencode __P((char *));

	/*
	**  Set up macros for possible expansion in headers.
	*/

	define('f', e->e_sender, e);
	define('g', e->e_sender, e);
	if (e->e_origrcpt != NULL && *e->e_origrcpt != '\0')
		define('u', e->e_origrcpt, e);
	else
		define('u', NULL, e);

	/* full name of from person */
	p = hvalue("full-name", e->e_header);
	if (p != NULL)
	{
		extern bool rfc822_string __P((char *));

		if (!rfc822_string(p))
		{
			extern char *addquotes __P((char *));

			/*
			**  Quote a full name with special characters
			**  as a comment so crackaddr() doesn't destroy
			**  the name portion of the address.
			*/
			p = addquotes(p);
		}
		define('x', p, e);
	}

	if (tTd(32, 1))
		printf("----- collected header -----\n");
	msgid = NULL;
	for (h = e->e_header; h != NULL; h = h->h_link)
	{
		if (tTd(32, 1))
			printf("%s: ", h->h_field);
		if (h->h_value == NULL)
		{
			if (tTd(32, 1))
				printf("<NULL>\n");
			continue;
		}

		/* do early binding */
		if (bitset(H_DEFAULT, h->h_flags))
		{
			if (tTd(32, 1))
			{
				printf("(");
				xputs(h->h_value);
				printf(") ");
			}
			expand(h->h_value, buf, sizeof buf, e);
			if (buf[0] != '\0')
			{
				if (bitset(H_FROM, h->h_flags))
				{
					extern char *crackaddr __P((char *));

					expand(crackaddr(buf), buf, sizeof buf, e);
				}
				h->h_value = newstr(buf);
				h->h_flags &= ~H_DEFAULT;
			}
		}

		if (tTd(32, 1))
		{
			xputs(h->h_value);
			printf("\n");
		}

		/* count the number of times it has been processed */
		if (bitset(H_TRACE, h->h_flags))
			hopcnt++;

		/* send to this person if we so desire */
		if (GrabTo && bitset(H_RCPT, h->h_flags) &&
		    !bitset(H_DEFAULT, h->h_flags) &&
		    (!bitset(EF_RESENT, e->e_flags) || bitset(H_RESENT, h->h_flags)))
		{
#if 0
			int saveflags = e->e_flags;
#endif

			(void) sendtolist(h->h_value, NULLADDR,
					  &e->e_sendqueue, 0, e);

#if 0
			/*
			**  Change functionality so a fatal error on an
			**  address doesn't affect the entire envelope.
			*/
			 
			/* delete fatal errors generated by this address */
			if (!bitset(EF_FATALERRS, saveflags))
				e->e_flags &= ~EF_FATALERRS;
#endif
		}

		/* save the message-id for logging */
		p = "resent-message-id";
		if (!bitset(EF_RESENT, e->e_flags))
			p += 7;
		if (strcasecmp(h->h_field, p) == 0)
		{
			msgid = h->h_value;
			while (isascii(*msgid) && isspace(*msgid))
				msgid++;
		}
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
	p = hvalue("precedence", e->e_header);
	if (p != NULL)
		e->e_class = priencode(p);
	if (e->e_class < 0)
		e->e_timeoutclass = TOC_NONURGENT;
	else if (e->e_class > 0)
		e->e_timeoutclass = TOC_URGENT;
	if (full)
	{
		e->e_msgpriority = e->e_msgsize
				 - e->e_class * WkClassFact
				 + e->e_nrcpts * WkRecipFact;
	}

	/* message timeout priority */
	p = hvalue("priority", e->e_header);
	if (p != NULL)
	{
		/* (this should be in the configuration file) */
		if (strcasecmp(p, "urgent") == 0)
			e->e_timeoutclass = TOC_URGENT;
		else if (strcasecmp(p, "normal") == 0)
			e->e_timeoutclass = TOC_NORMAL;
		else if (strcasecmp(p, "non-urgent") == 0)
			e->e_timeoutclass = TOC_NONURGENT;
	}

	/* date message originated */
	p = hvalue("posted-date", e->e_header);
	if (p == NULL)
		p = hvalue("date", e->e_header);
	if (p != NULL)
		define('a', p, e);

	/* check to see if this is a MIME message */
	if ((e->e_bodytype != NULL &&
	     strcasecmp(e->e_bodytype, "8BITMIME") == 0) ||
	    hvalue("MIME-Version", e->e_header) != NULL)
	{
		e->e_flags |= EF_IS_MIME;
		if (HasEightBits)
			e->e_bodytype = "8BITMIME";
	}
	else if ((p = hvalue("Content-Type", e->e_header)) != NULL)
	{
		/* this may be an RFC 1049 message */
		p = strpbrk(p, ";/");
		if (p == NULL || *p == ';')
		{
			/* yep, it is */
			e->e_flags |= EF_DONT_MIME;
		}
	}

	/*
	**  From person in antiquated ARPANET mode
	**	required by UK Grey Book e-mail gateways (sigh)
	*/

	if (OpMode == MD_ARPAFTP)
	{
		register struct hdrinfo *hi;

		for (hi = HdrInfo; hi->hi_field != NULL; hi++)
		{
			if (bitset(H_FROM, hi->hi_flags) &&
			    (!bitset(H_RESENT, hi->hi_flags) ||
			     bitset(EF_RESENT, e->e_flags)) &&
			    (p = hvalue(hi->hi_field, e->e_header)) != NULL)
				break;
		}
		if (hi->hi_field != NULL)
		{
			if (tTd(32, 2))
				printf("eatheader: setsender(*%s == %s)\n",
					hi->hi_field, p);
			setsender(p, e, NULL, '\0', TRUE);
		}
	}

	/*
	**  Log collection information.
	*/

	if (bitset(EF_LOGSENDER, e->e_flags) && LogLevel > 4)
		logsender(e, msgid);
	e->e_flags &= ~EF_LOGSENDER;
}
/*
**  LOGSENDER -- log sender information
**
**	Parameters:
**		e -- the envelope to log
**		msgid -- the message id
**
**	Returns:
**		none
*/

void
logsender(e, msgid)
	register ENVELOPE *e;
	char *msgid;
{
	char *name;
	register char *sbp;
	register char *p;
	int l;
	char hbuf[MAXNAME + 1];
	char sbuf[MAXLINE + 1];
	char mbuf[MAXNAME + 1];

	/* don't allow newlines in the message-id */
	if (msgid != NULL)
	{
		l = strlen(msgid);
		if (l > sizeof mbuf - 1)
			l = sizeof mbuf - 1;
		bcopy(msgid, mbuf, l);
		mbuf[l] = '\0';
		p = mbuf;
		while ((p = strchr(p, '\n')) != NULL)
			*p++ = ' ';
	}

	if (bitset(EF_RESPONSE, e->e_flags))
		name = "[RESPONSE]";
	else if ((name = macvalue('_', e)) != NULL)
		;
	else if (RealHostName == NULL)
		name = "localhost";
	else if (RealHostName[0] == '[')
		name = RealHostName;
	else
	{
		name = hbuf;
		(void) snprintf(hbuf, sizeof hbuf, "%.80s", RealHostName);
		if (RealHostAddr.sa.sa_family != 0)
		{
			p = &hbuf[strlen(hbuf)];
			(void) snprintf(p, SPACELEFT(hbuf, p), " (%.100s)",
				anynet_ntoa(&RealHostAddr));
		}
	}

	/* some versions of syslog only take 5 printf args */
#  if (SYSLOG_BUFSIZE) >= 256
	sbp = sbuf;
	snprintf(sbp, SPACELEFT(sbuf, sbp),
	    "from=%.200s, size=%ld, class=%d, pri=%ld, nrcpts=%d",
	    e->e_from.q_paddr == NULL ? "<NONE>" : e->e_from.q_paddr,
	    e->e_msgsize, e->e_class, e->e_msgpriority, e->e_nrcpts);
	sbp += strlen(sbp);
	if (msgid != NULL)
	{
		snprintf(sbp, SPACELEFT(sbuf, sbp), ", msgid=%.100s", mbuf);
		sbp += strlen(sbp);
	}
	if (e->e_bodytype != NULL)
	{
		(void) snprintf(sbp, SPACELEFT(sbuf, sbp), ", bodytype=%.20s",
			e->e_bodytype);
		sbp += strlen(sbp);
	}
	p = macvalue('r', e);
	if (p != NULL)
		(void) snprintf(sbp, SPACELEFT(sbuf, sbp), ", proto=%.20s", p);
	sm_syslog(LOG_INFO, e->e_id,
		"%.850s, relay=%.100s",
		sbuf, name);

#  else			/* short syslog buffer */

	sm_syslog(LOG_INFO, e->e_id,
		"from=%s",
		e->e_from.q_paddr == NULL ? "<NONE>"
					  : shortenstring(e->e_from.q_paddr, 83));
	sm_syslog(LOG_INFO, e->e_id,
		"size=%ld, class=%ld, pri=%ld, nrcpts=%d",
		e->e_msgsize, e->e_class, e->e_msgpriority, e->e_nrcpts);
	if (msgid != NULL)
		sm_syslog(LOG_INFO, e->e_id,
			"msgid=%s",
			shortenstring(mbuf, 83));
	sbp = sbuf;
	*sbp = '\0';
	if (e->e_bodytype != NULL)
	{
		snprintf(sbp, SPACELEFT(sbuf, sbp), "bodytype=%.20s, ", e->e_bodytype);
		sbp += strlen(sbp);
	}
	p = macvalue('r', e);
	if (p != NULL)
	{
		snprintf(sbp, SPACELEFT(sbuf, sbp), "proto=%.20s, ", p);
		sbp += strlen(sbp);
	}
	sm_syslog(LOG_INFO, e->e_id,
		"%.400srelay=%.100s", sbuf, name);
#  endif
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

int
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
	int bracklev;
	bool qmode;
	bool realqmode;
	bool skipping;
	bool putgmac = FALSE;
	bool quoteit = FALSE;
	bool gotangle = FALSE;
	bool gotcolon = FALSE;
	register char *bp;
	char *buflim;
	char *bufhead;
	char *addrhead;
	static char buf[MAXNAME + 1];

	if (tTd(33, 1))
		printf("crackaddr(%s)\n", addr);

	/* strip leading spaces */
	while (*addr != '\0' && isascii(*addr) && isspace(*addr))
		addr++;

	/*
	**  Start by assuming we have no angle brackets.  This will be
	**  adjusted later if we find them.
	*/

	bp = bufhead = buf;
	buflim = &buf[sizeof buf - 7];
	p = addrhead = addr;
	copylev = anglelev = realanglelev = cmtlev = realcmtlev = 0;
	bracklev = 0;
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
		if (c == '"' && cmtlev <= 0)
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
					if (bp != bufhead)
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
			if (copylev > 0 && !skipping)
				bp--;
		}

		/* count nesting on [ ... ] (for IPv6 domain literals) */
		if (c == '[')
			bracklev++;
		else if (c == ']')
			bracklev--;

		/* check for group: list; syntax */
		if (c == ':' && anglelev <= 0 && bracklev <= 0 &&
		    !gotcolon && !ColonOkInAddr)
		{
			register char *q;

			/*
			**  Check for DECnet phase IV ``::'' (host::user)
			**  or **  DECnet phase V ``:.'' syntaxes.  The latter
			**  covers ``user@DEC:.tay.myhost'' and
			**  ``DEC:.tay.myhost::user'' syntaxes (bletch).
			*/

			if (*p == ':' || *p == '.')
			{
				if (cmtlev <= 0 && !qmode)
					quoteit = TRUE;
				if (copylev > 0 && !skipping)
				{
					*bp++ = c;
					*bp++ = *p;
				}
				p++;
				goto putg;
			}

			gotcolon = TRUE;

			bp = bufhead;
			if (quoteit)
			{
				*bp++ = '"';

				/* back up over the ':' and any spaces */
				--p;
				while (isascii(*--p) && isspace(*p))
					continue;
				p++;
			}
			for (q = addrhead; q < p; )
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
				if (bp == &bufhead[1])
					bp--;
				else
					*bp++ = '"';
				while ((c = *p++) != ':')
				{
					if (bp < buflim)
						*bp++ = c;
				}
				*bp++ = c;
			}

			/* any trailing white space is part of group: */
			while (isascii(*p) && isspace(*p) && bp < buflim)
				*bp++ = *p++;
			copylev = 0;
			putgmac = quoteit = FALSE;
			bufhead = bp;
			addrhead = p;
			continue;
		}

		if (c == ';' && copylev <= 0 && !ColonOkInAddr)
		{
			if (bp < buflim)
				*bp++ = c;
		}

		/* check for characters that may have to be quoted */
		if (strchr(MustQuoteChars, c) != NULL)
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

			/* assume first of two angles is bogus */
			if (gotangle)
				quoteit = TRUE;
			gotangle = TRUE;

			/* oops -- have to change our mind */
			anglelev = 1;
			if (!skipping)
				realanglelev = 1;

			bp = bufhead;
			if (quoteit)
			{
				*bp++ = '"';

				/* back up over the '<' and any spaces */
				--p;
				while (isascii(*--p) && isspace(*p))
					continue;
				p++;
			}
			for (q = addrhead; q < p; )
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
				if (bp == &buf[1])
					bp--;
				else
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
				quoteit = TRUE;
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
			if (bp > bufhead && bp[-1] == ')')
				*bp++ = ' ';
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
	{
		printf("crackaddr=>`");
		xputs(buf);
		printf("'\n");
	}

	return (buf);
}
/*
**  PUTHEADER -- put the header part of a message from the in-core copy
**
**	Parameters:
**		mci -- the connection information.
**		h -- the header to put.
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

void
putheader(mci, hdr, e)
	register MCI *mci;
	HDR *hdr;
	register ENVELOPE *e;
{
	register HDR *h;
	char buf[MAX(MAXLINE,BUFSIZ)];
	char obuf[MAXLINE];

	if (tTd(34, 1))
		printf("--- putheader, mailer = %s ---\n",
			mci->mci_mailer->m_name);

	/*
	**  If we're in MIME mode, we're not really in the header of the
	**  message, just the header of one of the parts of the body of
	**  the message.  Therefore MCIF_INHEADER should not be turned on.
	*/

	if (!bitset(MCIF_INMIME, mci->mci_flags))
		mci->mci_flags |= MCIF_INHEADER;

	for (h = hdr; h != NULL; h = h->h_link)
	{
		register char *p = h->h_value;
		extern bool bitintersect __P((BITMAP, BITMAP));

		if (tTd(34, 11))
		{
			printf("  %s: ", h->h_field);
			xputs(p);
		}

#if _FFR_MAX_MIME_HEADER_LENGTH
		/* heuristic shortening of MIME fields to avoid MUA overflows */
		if (MaxMimeFieldLength > 0 &&
		    wordinclass(h->h_field,
				macid("{checkMIMEFieldHeaders}", NULL)))
		{
			extern bool fix_mime_header __P((char *));

			if (fix_mime_header(h->h_value))
			{
				sm_syslog(LOG_ALERT, e->e_id,
				  	"Truncated MIME %s header due to field size (possible attack)",
				  	h->h_field);
				if (tTd(34, 11))
				  	printf("  truncated MIME %s header due to field size (possible attack)\n",
					  	h->h_field);
			}
		}

		if (MaxMimeHeaderLength > 0 &&
		    wordinclass(h->h_field,
				macid("{checkMIMETextHeaders}", NULL)))
		{
			if (strlen(h->h_value) > MaxMimeHeaderLength)
			{
				h->h_value[MaxMimeHeaderLength - 1] = '\0';
				sm_syslog(LOG_ALERT, e->e_id,
				  	"Truncated long MIME %s header (possible attack)",
				  	h->h_field);
				if (tTd(34, 11))
				  	printf("  truncated long MIME %s header (possible attack)\n",
					  	h->h_field);
			}
		}

		if (MaxMimeHeaderLength > 0 &&
		    wordinclass(h->h_field,
				macid("{checkMIMEHeaders}", NULL)))
		{
			extern bool shorten_rfc822_string __P((char *, int));

			if (shorten_rfc822_string(h->h_value, MaxMimeHeaderLength))
			{
				sm_syslog(LOG_ALERT, e->e_id,
				  	"Truncated long MIME %s header (possible attack)",
				  	h->h_field);
				if (tTd(34, 11))
				  	printf("  truncated long MIME %s header (possible attack)\n",
					  	h->h_field);
			}
		}
#endif

		/* suppress Content-Transfer-Encoding: if we are MIMEing */
		if (bitset(H_CTE, h->h_flags) &&
		    bitset(MCIF_CVT8TO7|MCIF_CVT7TO8|MCIF_INMIME, mci->mci_flags))
		{
			if (tTd(34, 11))
				printf(" (skipped (content-transfer-encoding))\n");
			continue;
		}

		if (bitset(MCIF_INMIME, mci->mci_flags))
		{
			if (tTd(34, 11))
				printf("\n");
			put_vanilla_header(h, p, mci);
			continue;
		}

		if (bitset(H_CHECK|H_ACHECK, h->h_flags) &&
		    !bitintersect(h->h_mflags, mci->mci_mailer->m_flags))
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

		/* suppress return receipts if requested */
		if (bitset(H_RECEIPTTO, h->h_flags) &&
#if _FFR_DSN_RRT_OPTION
		    (RrtImpliesDsn || bitset(EF_NORECEIPT, e->e_flags)))
#else
		    bitset(EF_NORECEIPT, e->e_flags))
#endif
		{
			if (tTd(34, 11))
				printf(" (skipped (receipt))\n");
			continue;
		}

		/* macro expand value if generated internally */
		if (bitset(H_DEFAULT, h->h_flags))
		{
			expand(p, buf, sizeof buf, e);
			p = buf;
			if (*p == '\0')
			{
				if (tTd(34, 11))
					printf(" (skipped -- null value)\n");
				continue;
			}
		}

		if (bitset(H_BCC, h->h_flags))
		{
			/* Bcc: field -- either truncate or delete */
			if (bitset(EF_DELETE_BCC, e->e_flags))
			{
				if (tTd(34, 11))
					printf(" (skipped -- bcc)\n");
			}
			else
			{
				/* no other recipient headers: truncate value */
				(void) snprintf(obuf, sizeof obuf, "%s:",
					h->h_field);
				putline(obuf, mci);
			}
			continue;
		}

		if (tTd(34, 11))
			printf("\n");

		if (bitset(H_FROM|H_RCPT, h->h_flags))
		{
			/* address field */
			bool oldstyle = bitset(EF_OLDSTYLE, e->e_flags);

			if (bitset(H_FROM, h->h_flags))
				oldstyle = FALSE;
			commaize(h, p, oldstyle, mci, e);
		}
		else
		{
			put_vanilla_header(h, p, mci);
		}
	}

	/*
	**  If we are converting this to a MIME message, add the
	**  MIME headers.
	*/

#if MIME8TO7
	if (bitset(MM_MIME8BIT, MimeMode) &&
	    bitset(EF_HAS8BIT, e->e_flags) &&
	    !bitset(EF_DONT_MIME, e->e_flags) &&
	    !bitnset(M_8BITS, mci->mci_mailer->m_flags) &&
	    !bitset(MCIF_CVT8TO7|MCIF_CVT7TO8, mci->mci_flags))
	{
		if (hvalue("MIME-Version", e->e_header) == NULL)
			putline("MIME-Version: 1.0", mci);
		if (hvalue("Content-Type", e->e_header) == NULL)
		{
			snprintf(obuf, sizeof obuf,
				"Content-Type: text/plain; charset=%s",
				defcharset(e));
			putline(obuf, mci);
		}
		if (hvalue("Content-Transfer-Encoding", e->e_header) == NULL)
			putline("Content-Transfer-Encoding: 8bit", mci);
	}
#endif
}
/*
**  PUT_VANILLA_HEADER -- output a fairly ordinary header
**
**	Parameters:
**		h -- the structure describing this header
**		v -- the value of this header
**		mci -- the connection info for output
**
**	Returns:
**		none.
*/

void
put_vanilla_header(h, v, mci)
	HDR *h;
	char *v;
	MCI *mci;
{
	register char *nlp;
	register char *obp;
	int putflags;
	char obuf[MAXLINE];

	putflags = PXLF_HEADER;
#if _FFR_7BITHDRS
	if (bitnset(M_7BITHDRS, mci->mci_mailer->m_flags))
		putflags |= PXLF_STRIP8BIT;
#endif
	(void) snprintf(obuf, sizeof obuf, "%.200s: ", h->h_field);
	obp = obuf + strlen(obuf);
	while ((nlp = strchr(v, '\n')) != NULL)
	{
		int l;

		l = nlp - v;
		if (SPACELEFT(obuf, obp) - 1 < l)
			l = SPACELEFT(obuf, obp) - 1;

		snprintf(obp, SPACELEFT(obuf, obp), "%.*s", l, v);
		putxline(obuf, strlen(obuf), mci, putflags);
		v += l + 1;
		obp = obuf;
		if (*v != ' ' && *v != '\t')
			*obp++ = ' ';
	}
	snprintf(obp, SPACELEFT(obuf, obp), "%.*s",
		sizeof obuf - (obp - obuf) - 1, v);
	putxline(obuf, strlen(obuf), mci, putflags);
}
/*
**  COMMAIZE -- output a header field, making a comma-translated list.
**
**	Parameters:
**		h -- the header field to output.
**		p -- the value to put in it.
**		oldstyle -- TRUE if this is an old style header.
**		mci -- the connection information.
**		e -- the envelope containing the message.
**
**	Returns:
**		none.
**
**	Side Effects:
**		outputs "p" to file "fp".
*/

void
commaize(h, p, oldstyle, mci, e)
	register HDR *h;
	register char *p;
	bool oldstyle;
	register MCI *mci;
	register ENVELOPE *e;
{
	register char *obp;
	int opos;
	int omax;
	bool firstone = TRUE;
	int putflags = PXLF_HEADER;
	char obuf[MAXLINE + 3];

	/*
	**  Output the address list translated by the
	**  mailer and with commas.
	*/

	if (tTd(14, 2))
		printf("commaize(%s: %s)\n", h->h_field, p);

#if _FFR_7BITHDRS
	if (bitnset(M_7BITHDRS, mci->mci_mailer->m_flags))
		putflags |= PXLF_STRIP8BIT;
#endif

	obp = obuf;
	(void) snprintf(obp, SPACELEFT(obuf, obp), "%.200s: ", h->h_field);
	opos = strlen(h->h_field) + 2;
	if (opos > 202)
		opos = 202;
	obp += opos;
	omax = mci->mci_mailer->m_linelimit - 2;
	if (omax < 0 || omax > 78)
		omax = 78;

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

			(void) prescan(p, oldstyle ? ' ' : ',', pvpbuf,
				       sizeof pvpbuf, &oldp, NULL);
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
#if USERDB
		else if (e->e_from.q_mailer != NULL &&
			 bitnset(M_UDBRECIPIENT, e->e_from.q_mailer->m_flags))
		{
			extern char *udbsender __P((char *));
			char *q;

			q = udbsender(name);
			if (q != NULL)
				name = q;
		}
#endif
		stat = EX_OK;
		name = remotename(name, mci->mci_mailer, flags, &stat, e);
		if (*name == '\0')
		{
			*p = savechar;
			continue;
		}
		name = denlstring(name, FALSE, TRUE);

		/* output the name with nice formatting */
		opos += strlen(name);
		if (!firstone)
			opos += 2;
		if (opos > omax && !firstone)
		{
			snprintf(obp, SPACELEFT(obuf, obp), ",\n");
			putxline(obuf, strlen(obuf), mci, putflags);
			obp = obuf;
			(void) strcpy(obp, "        ");
			opos = strlen(obp);
			obp += opos;
			opos += strlen(name);
		}
		else if (!firstone)
		{
			snprintf(obp, SPACELEFT(obuf, obp), ", ");
			obp += 2;
		}

		while ((c = *name++) != '\0' && obp < &obuf[MAXLINE])
			*obp++ = c;
		firstone = FALSE;
		*p = savechar;
	}
	*obp = '\0';
	putxline(obuf, strlen(obuf), mci, putflags);
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
/*
**  FIX_MIME_HEADER -- possibly truncate/rebalance parameters in a MIME header
**
**	Run through all of the parameters of a MIME header and
**	possibly truncate and rebalance the parameter according
**	to MaxMimeFieldLength.
**
**	Parameters:
**		string -- the full header
**
**	Returns:
**		TRUE if the header was modified, FALSE otherwise
**
**	Side Effects:
**		string modified in place
*/

bool
fix_mime_header(string)
	char *string;
{
	bool modified = FALSE;
	char *begin = string;
	char *end;
	extern char *find_character __P((char *, char));
	extern bool shorten_rfc822_string __P((char *, int));
	
	if (string == NULL || *string == '\0')
		return FALSE;
	
	/* Split on each ';' */
	while ((end = find_character(begin, ';')) != NULL)
	{
		char save = *end;
		char *bp;
		
		*end = '\0';
		
		/* Shorten individual parameter */
		if (shorten_rfc822_string(begin, MaxMimeFieldLength))
			modified = TRUE;
		
		/* Collapse the possibly shortened string with rest */
		bp = begin + strlen(begin);
		if (bp != end)
		{
			char *ep = end;
			
			*end = save;
			end = bp;
			
			/* copy character by character due to overlap */
			while (*ep != '\0')
				*bp++ = *ep++;
			*bp = '\0';
		}
		else
			*end = save;
		if (*end == '\0')
			break;
		
		/* Move past ';' */
		begin = end + 1;
	}
	return modified;
}
