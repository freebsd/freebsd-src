/*
 * Copyright (c) 1998-2001 Sendmail, Inc. and its suppliers.
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
static char id[] = "@(#)$Id: recipient.c,v 8.231.14.11 2001/05/03 17:24:14 gshapiro Exp $";
#endif /* ! lint */

#include <sendmail.h>


static void	includetimeout __P((void));
static ADDRESS	*self_reference __P((ADDRESS *));

/*
**  SENDTOLIST -- Designate a send list.
**
**	The parameter is a comma-separated list of people to send to.
**	This routine arranges to send to all of them.
**
**	Parameters:
**		list -- the send list.
**		ctladdr -- the address template for the person to
**			send to -- effective uid/gid are important.
**			This is typically the alias that caused this
**			expansion.
**		sendq -- a pointer to the head of a queue to put
**			these people into.
**		aliaslevel -- the current alias nesting depth -- to
**			diagnose loops.
**		e -- the envelope in which to add these recipients.
**
**	Returns:
**		The number of addresses actually on the list.
**
**	Side Effects:
**		none.
*/

/* q_flags bits inherited from ctladdr */
#define QINHERITEDBITS	(QPINGONSUCCESS|QPINGONFAILURE|QPINGONDELAY|QHASNOTIFY)

int
sendtolist(list, ctladdr, sendq, aliaslevel, e)
	char *list;
	ADDRESS *ctladdr;
	ADDRESS **sendq;
	int aliaslevel;
	register ENVELOPE *e;
{
	register char *p;
	register ADDRESS *al;	/* list of addresses to send to */
	char delimiter;		/* the address delimiter */
	int naddrs;
	int i;
	char *oldto = e->e_to;
	char *bufp;
	char buf[MAXNAME + 1];

	if (list == NULL)
	{
		syserr("sendtolist: null list");
		return 0;
	}

	if (tTd(25, 1))
	{
		dprintf("sendto: %s\n   ctladdr=", list);
		printaddr(ctladdr, FALSE);
	}

	/* heuristic to determine old versus new style addresses */
	if (ctladdr == NULL &&
	    (strchr(list, ',') != NULL || strchr(list, ';') != NULL ||
	     strchr(list, '<') != NULL || strchr(list, '(') != NULL))
		e->e_flags &= ~EF_OLDSTYLE;
	delimiter = ' ';
	if (!bitset(EF_OLDSTYLE, e->e_flags) || ctladdr != NULL)
		delimiter = ',';

	al = NULL;
	naddrs = 0;

	/* make sure we have enough space to copy the string */
	i = strlen(list) + 1;
	if (i <= sizeof buf)
	{
		bufp = buf;
		i = sizeof buf;
	}
	else
		bufp = xalloc(i);
	(void) strlcpy(bufp, denlstring(list, FALSE, TRUE), i);

#if _FFR_ADDR_TYPE
	define(macid("{addr_type}", NULL), "e r", e);
#endif /* _FFR_ADDR_TYPE */
	for (p = bufp; *p != '\0'; )
	{
		auto char *delimptr;
		register ADDRESS *a;

		/* parse the address */
		while ((isascii(*p) && isspace(*p)) || *p == ',')
			p++;
		a = parseaddr(p, NULLADDR, RF_COPYALL, delimiter, &delimptr, e);
		p = delimptr;
		if (a == NULL)
			continue;
		a->q_next = al;
		a->q_alias = ctladdr;

		/* arrange to inherit attributes from parent */
		if (ctladdr != NULL)
		{
			ADDRESS *b;

			/* self reference test */
			if (sameaddr(ctladdr, a))
			{
				if (tTd(27, 5))
				{
					dprintf("sendtolist: QSELFREF ");
					printaddr(ctladdr, FALSE);
				}
				ctladdr->q_flags |= QSELFREF;
			}

			/* check for address loops */
			b = self_reference(a);
			if (b != NULL)
			{
				b->q_flags |= QSELFREF;
				if (tTd(27, 5))
				{
					dprintf("sendtolist: QSELFREF ");
					printaddr(b, FALSE);
				}
				if (a != b)
				{
					if (tTd(27, 5))
					{
						dprintf("sendtolist: QS_DONTSEND ");
						printaddr(a, FALSE);
					}
					a->q_state = QS_DONTSEND;
					b->q_flags |= a->q_flags & QNOTREMOTE;
					continue;
				}
			}

			/* full name */
			if (a->q_fullname == NULL)
				a->q_fullname = ctladdr->q_fullname;

			/* various flag bits */
			a->q_flags &= ~QINHERITEDBITS;
			a->q_flags |= ctladdr->q_flags & QINHERITEDBITS;

			/* original recipient information */
			a->q_orcpt = ctladdr->q_orcpt;
		}

		al = a;
	}

	/* arrange to send to everyone on the local send list */
	while (al != NULL)
	{
		register ADDRESS *a = al;

		al = a->q_next;
		a = recipient(a, sendq, aliaslevel, e);
		naddrs++;
	}

	e->e_to = oldto;
	if (bufp != buf)
		sm_free(bufp);
#if _FFR_ADDR_TYPE
	define(macid("{addr_type}", NULL), NULL, e);
#endif /* _FFR_ADDR_TYPE */
	return naddrs;
}
/*
**  REMOVEFROMLIST -- Remove addresses from a send list.
**
**	The parameter is a comma-separated list of recipients to remove.
**	Note that it only deletes matching addresses.  If those addresses
**	have been expended already in the sendq, it won't mark the
**	expanded recipients as QS_REMOVED.
**
**	Parameters:
**		list -- the list to remove.
**		sendq -- a pointer to the head of a queue to remove
**			these addresses from.
**		e -- the envelope in which to remove these recipients.
**
**	Returns:
**		The number of addresses removed from the list.
**
*/

int
removefromlist(list, sendq, e)
	char *list;
	ADDRESS **sendq;
	ENVELOPE *e;
{
	char delimiter;		/* the address delimiter */
	int naddrs;
	int i;
	char *p;
	char *oldto = e->e_to;
	char *bufp;
	char buf[MAXNAME + 1];

	if (list == NULL)
	{
		syserr("removefromlist: null list");
		return 0;
	}

	if (tTd(25, 1))
		dprintf("removefromlist: %s\n", list);

	/* heuristic to determine old versus new style addresses */
	if (strchr(list, ',') != NULL || strchr(list, ';') != NULL ||
	    strchr(list, '<') != NULL || strchr(list, '(') != NULL)
		e->e_flags &= ~EF_OLDSTYLE;
	delimiter = ' ';
	if (!bitset(EF_OLDSTYLE, e->e_flags))
		delimiter = ',';

	naddrs = 0;

	/* make sure we have enough space to copy the string */
	i = strlen(list) + 1;
	if (i <= sizeof buf)
	{
		bufp = buf;
		i = sizeof buf;
	}
	else
		bufp = xalloc(i);
	(void) strlcpy(bufp, denlstring(list, FALSE, TRUE), i);

#if _FFR_ADDR_TYPE
	define(macid("{addr_type}", NULL), "e r", e);
#endif /* _FFR_ADDR_TYPE */
	for (p = bufp; *p != '\0'; )
	{
		ADDRESS a;		/* parsed address to be removed */
		ADDRESS *q;
		ADDRESS **pq;
		char *delimptr;

		/* parse the address */
		while ((isascii(*p) && isspace(*p)) || *p == ',')
			p++;
		if (parseaddr(p, &a, RF_COPYALL,
			      delimiter, &delimptr, e) == NULL)
		{
			p = delimptr;
			continue;
		}
		p = delimptr;
		for (pq = sendq; (q = *pq) != NULL; pq = &q->q_next)
		{
			if (!QS_IS_DEAD(q->q_state) &&
			    sameaddr(q, &a))
			{
				if (tTd(25, 5))
				{
					dprintf("removefromlist: QS_REMOVED ");
					printaddr(&a, FALSE);
				}
				q->q_state = QS_REMOVED;
				naddrs++;
				break;
			}
		}
	}

	e->e_to = oldto;
	if (bufp != buf)
		sm_free(bufp);
#if _FFR_ADDR_TYPE
	define(macid("{addr_type}", NULL), NULL, e);
#endif /* _FFR_ADDR_TYPE */
	return naddrs;
}
/*
**  RECIPIENT -- Designate a message recipient
**
**	Saves the named person for future mailing.
**
**	Parameters:
**		a -- the (preparsed) address header for the recipient.
**		sendq -- a pointer to the head of a queue to put the
**			recipient in.  Duplicate suppression is done
**			in this queue.
**		aliaslevel -- the current alias nesting depth.
**		e -- the current envelope.
**
**	Returns:
**		The actual address in the queue.  This will be "a" if
**		the address is not a duplicate, else the original address.
**
**	Side Effects:
**		none.
*/

ADDRESS *
recipient(a, sendq, aliaslevel, e)
	register ADDRESS *a;
	register ADDRESS **sendq;
	int aliaslevel;
	register ENVELOPE *e;
{
	register ADDRESS *q;
	ADDRESS **pq;
	register struct mailer *m;
	register char *p = NULL;
	bool quoted = FALSE;		/* set if the addr has a quote bit */
	int findusercount = 0;
	bool initialdontsend = QS_IS_DEAD(a->q_state);
	int i, buflen;
	char *buf;
	char buf0[MAXNAME + 1];		/* unquoted image of the user name */

	e->e_to = a->q_paddr;
	m = a->q_mailer;
	errno = 0;
	if (aliaslevel == 0)
		a->q_flags |= QPRIMARY;
	if (tTd(26, 1))
	{
		dprintf("\nrecipient (%d): ", aliaslevel);
		printaddr(a, FALSE);
	}

	/* if this is primary, add it to the original recipient list */
	if (a->q_alias == NULL)
	{
		if (e->e_origrcpt == NULL)
			e->e_origrcpt = a->q_paddr;
		else if (e->e_origrcpt != a->q_paddr)
			e->e_origrcpt = "";
	}

#if _FFR_GEN_ORCPT
	/* set ORCPT DSN arg if not already set */
	if (a->q_orcpt == NULL)
	{
		for (q = a; q->q_alias != NULL; q = q->q_alias)
			continue;

		/* check for an existing ORCPT */
		if (q->q_orcpt != NULL)
			a->q_orcpt = q->q_orcpt;
		else
		{
			/* make our own */
			bool b = FALSE;
			char *qp;
			char obuf[MAXLINE];

			if (e->e_from.q_mailer != NULL)
				p = e->e_from.q_mailer->m_addrtype;
			if (p == NULL)
				p = "rfc822";
			(void) strlcpy(obuf, p, sizeof obuf);
			(void) strlcat(obuf, ";", sizeof obuf);

			qp = q->q_paddr;

			/* FFR: Needs to strip comments from stdin addrs */

			/* strip brackets from address */
			if (*qp == '<')
			{
				b = qp[strlen(qp) - 1] == '>';
				if (b)
					qp[strlen(qp) - 1] = '\0';
				qp++;
			}

			p = xtextify(denlstring(qp, TRUE, FALSE), NULL);

			if (strlcat(obuf, p, sizeof obuf) >= sizeof obuf)
			{
				/* if too big, don't use it */
				obuf[0] = '\0';
			}

			/* undo damage */
			if (b)
				qp[strlen(qp)] = '>';

			if (obuf[0] != '\0')
				a->q_orcpt = newstr(obuf);
		}
	}
#endif /* _FFR_GEN_ORCPT */

	/* break aliasing loops */
	if (aliaslevel > MaxAliasRecursion)
	{
		a->q_state = QS_BADADDR;
		a->q_status = "5.4.6";
		usrerrenh(a->q_status,
			  "554 aliasing/forwarding loop broken (%d aliases deep; %d max)",
			  aliaslevel, MaxAliasRecursion);
		return a;
	}

	/*
	**  Finish setting up address structure.
	*/

	/* get unquoted user for file, program or user.name check */
	i = strlen(a->q_user);
	if (i >= sizeof buf0)
	{
		buflen = i + 1;
		buf = xalloc(buflen);
	}
	else
	{
		buf = buf0;
		buflen = sizeof buf0;
	}
	(void) strlcpy(buf, a->q_user, buflen);
	for (p = buf; *p != '\0' && !quoted; p++)
	{
		if (*p == '\\')
			quoted = TRUE;
	}
	stripquotes(buf);

	/* check for direct mailing to restricted mailers */
	if (m == ProgMailer)
	{
		if (a->q_alias == NULL)
		{
			a->q_state = QS_BADADDR;
			a->q_status = "5.7.1";
			usrerrenh(a->q_status,
				  "550 Cannot mail directly to programs");
		}
		else if (bitset(QBOGUSSHELL, a->q_alias->q_flags))
		{
			a->q_state = QS_BADADDR;
			a->q_status = "5.7.1";
			if (a->q_alias->q_ruser == NULL)
				usrerrenh(a->q_status,
					  "550 UID %d is an unknown user: cannot mail to programs",
					  a->q_alias->q_uid);
			else
				usrerrenh(a->q_status,
					  "550 User %s@%s doesn't have a valid shell for mailing to programs",
					  a->q_alias->q_ruser, MyHostName);
		}
		else if (bitset(QUNSAFEADDR, a->q_alias->q_flags))
		{
			a->q_state = QS_BADADDR;
			a->q_status = "5.7.1";
			a->q_rstatus = newstr("550 Unsafe for mailing to programs");
			usrerrenh(a->q_status,
				  "550 Address %s is unsafe for mailing to programs",
				  a->q_alias->q_paddr);
		}
	}

	/*
	**  Look up this person in the recipient list.
	**	If they are there already, return, otherwise continue.
	**	If the list is empty, just add it.  Notice the cute
	**	hack to make from addresses suppress things correctly:
	**	the QS_DUPLICATE state will be set in the send list.
	**	[Please note: the emphasis is on "hack."]
	*/

	for (pq = sendq; (q = *pq) != NULL; pq = &q->q_next)
	{
		if (sameaddr(q, a) &&
		    (bitset(QRCPTOK, q->q_flags) ||
		     !bitset(QPRIMARY, q->q_flags)))
		{
			if (tTd(26, 1))
			{
				dprintf("%s in sendq: ", a->q_paddr);
				printaddr(q, FALSE);
			}
			if (!bitset(QPRIMARY, q->q_flags))
			{
				if (!QS_IS_DEAD(a->q_state))
					message("duplicate suppressed");
				else
					q->q_state = QS_DUPLICATE;
				q->q_flags |= a->q_flags;
			}
			else if (bitset(QSELFREF, q->q_flags)
#if _FFR_MILTER
				 || q->q_state == QS_REMOVED
#endif /* _FFR_MILTER */
				 )
			{
#if _FFR_MILTER
				/*
				**  If an earlier milter removed the address,
				**  a later one can still add it back.
				*/
#endif /* _FFR_MILTER */
				q->q_state = a->q_state;
				q->q_flags |= a->q_flags;
			}
			a = q;
			goto done;
		}
	}

	/* add address on list */
	if (pq != NULL)
	{
		*pq = a;
		a->q_next = NULL;
	}

	/*
	**  Alias the name and handle special mailer types.
	*/

  trylocaluser:
	if (tTd(29, 7))
	{
		dprintf("at trylocaluser: ");
		printaddr(a, FALSE);
	}

	if (!QS_IS_OK(a->q_state))
		goto testselfdestruct;

	if (m == InclMailer)
	{
		a->q_state = QS_INCLUDED;
		if (a->q_alias == NULL)
		{
			a->q_state = QS_BADADDR;
			a->q_status = "5.7.1";
			usrerrenh(a->q_status,
				  "550 Cannot mail directly to :include:s");
		}
		else
		{
			int ret;

			message("including file %s", a->q_user);
			ret = include(a->q_user, FALSE, a, sendq, aliaslevel, e);
			if (transienterror(ret))
			{
				if (LogLevel > 2)
					sm_syslog(LOG_ERR, e->e_id,
						  "include %s: transient error: %s",
						  shortenstring(a->q_user, MAXSHORTSTR),
						  errstring(ret));
				a->q_state = QS_QUEUEUP;
				usrerr("451 4.2.4 Cannot open %s: %s",
					shortenstring(a->q_user, MAXSHORTSTR),
					errstring(ret));
			}
			else if (ret != 0)
			{
				a->q_state = QS_BADADDR;
				a->q_status = "5.2.4";
				usrerrenh(a->q_status,
					  "550 Cannot open %s: %s",
					  shortenstring(a->q_user, MAXSHORTSTR),
					  errstring(ret));
			}
		}
	}
	else if (m == FileMailer)
	{
		/* check if writable or creatable */
		if (a->q_alias == NULL)
		{
			a->q_state = QS_BADADDR;
			a->q_status = "5.7.1";
			usrerrenh(a->q_status,
				  "550 Cannot mail directly to files");
		}
		else if (bitset(QBOGUSSHELL, a->q_alias->q_flags))
		{
			a->q_state = QS_BADADDR;
			a->q_status = "5.7.1";
			if (a->q_alias->q_ruser == NULL)
				usrerrenh(a->q_status,
					  "550 UID %d is an unknown user: cannot mail to files",
					  a->q_alias->q_uid);
			else
				usrerrenh(a->q_status,
					  "550 User %s@%s doesn't have a valid shell for mailing to files",
					  a->q_alias->q_ruser, MyHostName);
		}
		else if (bitset(QUNSAFEADDR, a->q_alias->q_flags))
		{
			a->q_state = QS_BADADDR;
			a->q_status = "5.7.1";
			a->q_rstatus = newstr("550 Unsafe for mailing to files");
			usrerrenh(a->q_status,
				  "550 Address %s is unsafe for mailing to files",
				  a->q_alias->q_paddr);
		}
	}

	/* try aliasing */
	if (!quoted && QS_IS_OK(a->q_state) &&
	    bitnset(M_ALIASABLE, m->m_flags))
		alias(a, sendq, aliaslevel, e);

#if USERDB
	/* if not aliased, look it up in the user database */
	if (!bitset(QNOTREMOTE, a->q_flags) &&
	    QS_IS_SENDABLE(a->q_state) &&
	    bitnset(M_CHECKUDB, m->m_flags))
	{
		if (udbexpand(a, sendq, aliaslevel, e) == EX_TEMPFAIL)
		{
			a->q_state = QS_QUEUEUP;
			if (e->e_message == NULL)
				e->e_message = newstr("Deferred: user database error");
			if (LogLevel > 8)
				sm_syslog(LOG_INFO, e->e_id,
					  "deferred: udbexpand: %s",
					  errstring(errno));
			message("queued (user database error): %s",
				errstring(errno));
			e->e_nrcpts++;
			goto testselfdestruct;
		}
	}
#endif /* USERDB */

	/*
	**  If we have a level two config file, then pass the name through
	**  Ruleset 5 before sending it off.  Ruleset 5 has the right
	**  to send rewrite it to another mailer.  This gives us a hook
	**  after local aliasing has been done.
	*/

	if (tTd(29, 5))
	{
		dprintf("recipient: testing local?  cl=%d, rr5=%lx\n\t",
			ConfigLevel, (u_long) RewriteRules[5]);
		printaddr(a, FALSE);
	}
	if (ConfigLevel >= 2 && RewriteRules[5] != NULL &&
	    bitnset(M_TRYRULESET5, m->m_flags) &&
	    !bitset(QNOTREMOTE, a->q_flags) &&
	    QS_IS_OK(a->q_state))
	{
		maplocaluser(a, sendq, aliaslevel + 1, e);
	}

	/*
	**  If it didn't get rewritten to another mailer, go ahead
	**  and deliver it.
	*/

	if (QS_IS_OK(a->q_state) &&
	    bitnset(M_HASPWENT, m->m_flags))
	{
		auto bool fuzzy;
		register struct passwd *pw;

		/* warning -- finduser may trash buf */
		pw = finduser(buf, &fuzzy);
		if (pw == NULL || strlen(pw->pw_name) > MAXNAME)
		{
			{
				a->q_state = QS_BADADDR;
				a->q_status = "5.1.1";
				a->q_rstatus = newstr("550 5.1.1 User unknown");
				giveresponse(EX_NOUSER, a->q_status, m, NULL,
					     a->q_alias, (time_t) 0, e);
			}
		}
		else
		{
			char nbuf[MAXNAME + 1];

			if (fuzzy)
			{
				/* name was a fuzzy match */
				a->q_user = newstr(pw->pw_name);
				if (findusercount++ > 3)
				{
					a->q_state = QS_BADADDR;
					a->q_status = "5.4.6";
					usrerrenh(a->q_status,
						  "554 aliasing/forwarding loop for %s broken",
						  pw->pw_name);
					goto done;
				}

				/* see if it aliases */
				(void) strlcpy(buf, pw->pw_name, buflen);
				goto trylocaluser;
			}
			if (*pw->pw_dir == '\0')
				a->q_home = NULL;
			else if (strcmp(pw->pw_dir, "/") == 0)
				a->q_home = "";
			else
				a->q_home = newstr(pw->pw_dir);
			a->q_uid = pw->pw_uid;
			a->q_gid = pw->pw_gid;
			a->q_ruser = newstr(pw->pw_name);
			a->q_flags |= QGOODUID;
			buildfname(pw->pw_gecos, pw->pw_name, nbuf, sizeof nbuf);
			if (nbuf[0] != '\0')
				a->q_fullname = newstr(nbuf);
			if (!usershellok(pw->pw_name, pw->pw_shell))
			{
				a->q_flags |= QBOGUSSHELL;
			}
			if (bitset(EF_VRFYONLY, e->e_flags))
			{
				/* don't do any more now */
				a->q_state = QS_VERIFIED;
			}
			else if (!quoted)
				forward(a, sendq, aliaslevel, e);
		}
	}
	if (!QS_IS_DEAD(a->q_state))
		e->e_nrcpts++;

  testselfdestruct:
	a->q_flags |= QTHISPASS;
	if (tTd(26, 8))
	{
		dprintf("testselfdestruct: ");
		printaddr(a, FALSE);
		if (tTd(26, 10))
		{
			dprintf("SENDQ:\n");
			printaddr(*sendq, TRUE);
			dprintf("----\n");
		}
	}
	if (a->q_alias == NULL && a != &e->e_from &&
	    QS_IS_DEAD(a->q_state))
	{
		for (q = *sendq; q != NULL; q = q->q_next)
		{
			if (!QS_IS_DEAD(q->q_state))
				break;
		}
		if (q == NULL)
		{
			a->q_state = QS_BADADDR;
			a->q_status = "5.4.6";
			usrerrenh(a->q_status,
				  "554 aliasing/forwarding loop broken");
		}
	}

  done:
	a->q_flags |= QTHISPASS;
	if (buf != buf0)
		sm_free(buf);

	/*
	**  If we are at the top level, check to see if this has
	**  expanded to exactly one address.  If so, it can inherit
	**  the primaryness of the address.
	**
	**  While we're at it, clear the QTHISPASS bits.
	*/

	if (aliaslevel == 0)
	{
		int nrcpts = 0;
		ADDRESS *only = NULL;

		for (q = *sendq; q != NULL; q = q->q_next)
		{
			if (bitset(QTHISPASS, q->q_flags) &&
			    QS_IS_SENDABLE(q->q_state))
			{
				nrcpts++;
				only = q;
			}
			q->q_flags &= ~QTHISPASS;
		}
		if (nrcpts == 1)
		{
			/* check to see if this actually got a new owner */
			q = only;
			while ((q = q->q_alias) != NULL)
			{
				if (q->q_owner != NULL)
					break;
			}
			if (q == NULL)
				only->q_flags |= QPRIMARY;
		}
		else if (!initialdontsend && nrcpts > 0)
		{
			/* arrange for return receipt */
			e->e_flags |= EF_SENDRECEIPT;
			a->q_flags |= QEXPANDED;
			if (e->e_xfp != NULL &&
			    bitset(QPINGONSUCCESS, a->q_flags))
				fprintf(e->e_xfp,
					"%s... expanded to multiple addresses\n",
					a->q_paddr);
		}
	}
	a->q_flags |= QRCPTOK;
	return a;
}
/*
**  FINDUSER -- find the password entry for a user.
**
**	This looks a lot like getpwnam, except that it may want to
**	do some fancier pattern matching in /etc/passwd.
**
**	This routine contains most of the time of many sendmail runs.
**	It deserves to be optimized.
**
**	Parameters:
**		name -- the name to match against.
**		fuzzyp -- an outarg that is set to TRUE if this entry
**			was found using the fuzzy matching algorithm;
**			set to FALSE otherwise.
**
**	Returns:
**		A pointer to a pw struct.
**		NULL if name is unknown or ambiguous.
**
**	Side Effects:
**		may modify name.
*/

struct passwd *
finduser(name, fuzzyp)
	char *name;
	bool *fuzzyp;
{
	register struct passwd *pw;
	register char *p;
	bool tryagain;

	if (tTd(29, 4))
		dprintf("finduser(%s): ", name);

	*fuzzyp = FALSE;

#ifdef HESIOD
	/* DEC Hesiod getpwnam accepts numeric strings -- short circuit it */
	for (p = name; *p != '\0'; p++)
		if (!isascii(*p) || !isdigit(*p))
			break;
	if (*p == '\0')
	{
		if (tTd(29, 4))
			dprintf("failed (numeric input)\n");
		return NULL;
	}
#endif /* HESIOD */

	/* look up this login name using fast path */
	if ((pw = sm_getpwnam(name)) != NULL)
	{
		if (tTd(29, 4))
			dprintf("found (non-fuzzy)\n");
		return pw;
	}

	/* try mapping it to lower case */
	tryagain = FALSE;
	for (p = name; *p != '\0'; p++)
	{
		if (isascii(*p) && isupper(*p))
		{
			*p = tolower(*p);
			tryagain = TRUE;
		}
	}
	if (tryagain && (pw = sm_getpwnam(name)) != NULL)
	{
		if (tTd(29, 4))
			dprintf("found (lower case)\n");
		*fuzzyp = TRUE;
		return pw;
	}

#if MATCHGECOS
	/* see if fuzzy matching allowed */
	if (!MatchGecos)
	{
		if (tTd(29, 4))
			dprintf("not found (fuzzy disabled)\n");
		return NULL;
	}

	/* search for a matching full name instead */
	for (p = name; *p != '\0'; p++)
	{
		if (*p == (SpaceSub & 0177) || *p == '_')
			*p = ' ';
	}
	(void) setpwent();
	while ((pw = getpwent()) != NULL)
	{
		char buf[MAXNAME + 1];

# if 0
		if (strcasecmp(pw->pw_name, name) == 0)
		{
			if (tTd(29, 4))
				dprintf("found (case wrapped)\n");
			break;
		}
# endif /* 0 */

		buildfname(pw->pw_gecos, pw->pw_name, buf, sizeof buf);
		if (strchr(buf, ' ') != NULL && strcasecmp(buf, name) == 0)
		{
			if (tTd(29, 4))
				dprintf("fuzzy matches %s\n", pw->pw_name);
			message("sending to login name %s", pw->pw_name);
			break;
		}
	}
	if (pw != NULL)
		*fuzzyp = TRUE;
	else if (tTd(29, 4))
		dprintf("no fuzzy match found\n");
# if DEC_OSF_BROKEN_GETPWENT	/* DEC OSF/1 3.2 or earlier */
	endpwent();
# endif /* DEC_OSF_BROKEN_GETPWENT */
	return pw;
#else /* MATCHGECOS */
	if (tTd(29, 4))
		dprintf("not found (fuzzy disabled)\n");
	return NULL;
#endif /* MATCHGECOS */
}
/*
**  WRITABLE -- predicate returning if the file is writable.
**
**	This routine must duplicate the algorithm in sys/fio.c.
**	Unfortunately, we cannot use the access call since we
**	won't necessarily be the real uid when we try to
**	actually open the file.
**
**	Notice that ANY file with ANY execute bit is automatically
**	not writable.  This is also enforced by mailfile.
**
**	Parameters:
**		filename -- the file name to check.
**		ctladdr -- the controlling address for this file.
**		flags -- SFF_* flags to control the function.
**
**	Returns:
**		TRUE -- if we will be able to write this file.
**		FALSE -- if we cannot write this file.
**
**	Side Effects:
**		none.
*/

bool
writable(filename, ctladdr, flags)
	char *filename;
	ADDRESS *ctladdr;
	long flags;
{
	uid_t euid = 0;
	gid_t egid = 0;
	char *user = NULL;

	if (tTd(44, 5))
		dprintf("writable(%s, 0x%lx)\n", filename, flags);

	/*
	**  File does exist -- check that it is writable.
	*/

	if (geteuid() != 0)
	{
		euid = geteuid();
		egid = getegid();
		user = NULL;
	}
	else if (ctladdr != NULL)
	{
		euid = ctladdr->q_uid;
		egid = ctladdr->q_gid;
		user = ctladdr->q_user;
	}
	else if (bitset(SFF_RUNASREALUID, flags))
	{
		euid = RealUid;
		egid = RealGid;
		user = RealUserName;
	}
	else if (FileMailer != NULL && !bitset(SFF_ROOTOK, flags))
	{
		euid = FileMailer->m_uid;
		egid = FileMailer->m_gid;
		user = NULL;
	}
	else
	{
		euid = egid = 0;
		user = NULL;
	}
	if (!bitset(SFF_ROOTOK, flags))
	{
		if (euid == 0)
		{
			euid = DefUid;
			user = DefUser;
		}
		if (egid == 0)
			egid = DefGid;
	}
	if (geteuid() == 0 &&
	    (ctladdr == NULL || !bitset(QGOODUID, ctladdr->q_flags)))
		flags |= SFF_SETUIDOK;

	if (!bitnset(DBS_FILEDELIVERYTOSYMLINK, DontBlameSendmail))
		flags |= SFF_NOSLINK;
	if (!bitnset(DBS_FILEDELIVERYTOHARDLINK, DontBlameSendmail))
		flags |= SFF_NOHLINK;

	errno = safefile(filename, euid, egid, user, flags, S_IWRITE, NULL);
	return errno == 0;
}
/*
**  INCLUDE -- handle :include: specification.
**
**	Parameters:
**		fname -- filename to include.
**		forwarding -- if TRUE, we are reading a .forward file.
**			if FALSE, it's a :include: file.
**		ctladdr -- address template to use to fill in these
**			addresses -- effective user/group id are
**			the important things.
**		sendq -- a pointer to the head of the send queue
**			to put these addresses in.
**		aliaslevel -- the alias nesting depth.
**		e -- the current envelope.
**
**	Returns:
**		open error status
**
**	Side Effects:
**		reads the :include: file and sends to everyone
**		listed in that file.
**
**	Security Note:
**		If you have restricted chown (that is, you can't
**		give a file away), it is reasonable to allow programs
**		and files called from this :include: file to be to be
**		run as the owner of the :include: file.  This is bogus
**		if there is any chance of someone giving away a file.
**		We assume that pre-POSIX systems can give away files.
**
**		There is an additional restriction that if you
**		forward to a :include: file, it will not take on
**		the ownership of the :include: file.  This may not
**		be necessary, but shouldn't hurt.
*/

static jmp_buf	CtxIncludeTimeout;

int
include(fname, forwarding, ctladdr, sendq, aliaslevel, e)
	char *fname;
	bool forwarding;
	ADDRESS *ctladdr;
	ADDRESS **sendq;
	int aliaslevel;
	ENVELOPE *e;
{
	FILE *volatile fp = NULL;
	char *oldto = e->e_to;
	char *oldfilename = FileName;
	int oldlinenumber = LineNumber;
	register EVENT *ev = NULL;
	int nincludes;
	int mode;
	volatile bool maxreached = FALSE;
	register ADDRESS *ca;
	volatile uid_t saveduid;
	volatile gid_t savedgid;
	volatile uid_t uid;
	volatile gid_t gid;
	char *volatile user;
	int rval = 0;
	volatile long sfflags = SFF_REGONLY;
	register char *p;
	bool safechown = FALSE;
	volatile bool safedir = FALSE;
	struct stat st;
	char buf[MAXLINE];

	if (tTd(27, 2))
		dprintf("include(%s)\n", fname);
	if (tTd(27, 4))
		dprintf("   ruid=%d euid=%d\n",
			(int) getuid(), (int) geteuid());
	if (tTd(27, 14))
	{
		dprintf("ctladdr ");
		printaddr(ctladdr, FALSE);
	}

	if (tTd(27, 9))
		dprintf("include: old uid = %d/%d\n",
			(int) getuid(), (int) geteuid());

#if _FFR_UNSAFE_WRITABLE_INCLUDE
	if (forwarding)
	{
		if (!bitnset(DBS_GROUPWRITABLEFORWARDFILE, DontBlameSendmail))
			sfflags |= SFF_NOGWFILES;
		if (!bitnset(DBS_WORLDWRITABLEFORWARDFILE, DontBlameSendmail))
			sfflags |= SFF_NOWWFILES;
	}
	else
	{
		if (!bitnset(DBS_GROUPWRITABLEINCLUDEFILE, DontBlameSendmail))
			sfflags |= SFF_NOGWFILES;
		if (!bitnset(DBS_WORLDWRITABLEINCLUDEFILE, DontBlameSendmail))
			sfflags |= SFF_NOWWFILES;
	}
#endif /* _FFR_UNSAFE_WRITABLE_INCLUDE */

	if (forwarding)
		sfflags |= SFF_MUSTOWN|SFF_ROOTOK|SFF_NOWLINK;

	/*
	**  If RunAsUser set, won't be able to run programs as user
	**  so mark them as unsafe unless the administrator knows better.
	*/

	if ((geteuid() != 0 || RunAsUid != 0) &&
	    !bitnset(DBS_NONROOTSAFEADDR, DontBlameSendmail))
	{
		if (tTd(27, 4))
			dprintf("include: not safe (euid=%d, RunAsUid=%d)\n",
				(int) geteuid(), (int) RunAsUid);
		ctladdr->q_flags |= QUNSAFEADDR;
	}

	ca = getctladdr(ctladdr);
	if (ca == NULL ||
	    (ca->q_uid == DefUid && ca->q_gid == 0))
	{
		uid = DefUid;
		gid = DefGid;
		user = DefUser;
	}
	else
	{
		uid = ca->q_uid;
		gid = ca->q_gid;
		user = ca->q_user;
	}
#if MAILER_SETUID_METHOD != USE_SETUID
	saveduid = geteuid();
	savedgid = getegid();
	if (saveduid == 0)
	{
		if (!DontInitGroups)
		{
			if (initgroups(user, gid) == -1)
			{
				rval = EAGAIN;
				syserr("include: initgroups(%s, %d) failed",
					user, gid);
				goto resetuid;
			}
		}
		else
		{
			GIDSET_T gidset[1];

			gidset[0] = gid;
			if (setgroups(1, gidset) == -1)
			{
				rval = EAGAIN;
				syserr("include: setgroups() failed");
				goto resetuid;
			}
		}

		if (gid != 0 && setgid(gid) < -1)
		{
			rval = EAGAIN;
			syserr("setgid(%d) failure", gid);
			goto resetuid;
		}
		if (uid != 0)
		{
# if MAILER_SETUID_METHOD == USE_SETEUID
			if (seteuid(uid) < 0)
			{
				rval = EAGAIN;
				syserr("seteuid(%d) failure (real=%d, eff=%d)",
					uid, getuid(), geteuid());
				goto resetuid;
			}
# endif /* MAILER_SETUID_METHOD == USE_SETEUID */
# if MAILER_SETUID_METHOD == USE_SETREUID
			if (setreuid(0, uid) < 0)
			{
				rval = EAGAIN;
				syserr("setreuid(0, %d) failure (real=%d, eff=%d)",
					uid, getuid(), geteuid());
				goto resetuid;
			}
# endif /* MAILER_SETUID_METHOD == USE_SETREUID */
		}
	}
#endif /* MAILER_SETUID_METHOD != USE_SETUID */

	if (tTd(27, 9))
		dprintf("include: new uid = %d/%d\n",
			(int) getuid(), (int) geteuid());

	/*
	**  If home directory is remote mounted but server is down,
	**  this can hang or give errors; use a timeout to avoid this
	*/

	if (setjmp(CtxIncludeTimeout) != 0)
	{
		ctladdr->q_state = QS_QUEUEUP;
		errno = 0;

		/* return pseudo-error code */
		rval = E_SM_OPENTIMEOUT;
		goto resetuid;
	}
	if (TimeOuts.to_fileopen > 0)
		ev = setevent(TimeOuts.to_fileopen, includetimeout, 0);
	else
		ev = NULL;


	/* check for writable parent directory */
	p = strrchr(fname, '/');
	if (p != NULL)
	{
		int ret;

		*p = '\0';
		ret = safedirpath(fname, uid, gid, user,
				  sfflags|SFF_SAFEDIRPATH, 0, 0);
		if (ret == 0)
		{
			/* in safe directory: relax chown & link rules */
			safedir = TRUE;
			sfflags |= SFF_NOPATHCHECK;
		}
		else
		{
			if (bitnset((forwarding ?
				     DBS_FORWARDFILEINUNSAFEDIRPATH :
				     DBS_INCLUDEFILEINUNSAFEDIRPATH),
				    DontBlameSendmail))
				sfflags |= SFF_NOPATHCHECK;
			else if (bitnset((forwarding ?
					  DBS_FORWARDFILEINGROUPWRITABLEDIRPATH :
					  DBS_INCLUDEFILEINGROUPWRITABLEDIRPATH),
					 DontBlameSendmail) &&
				 ret == E_SM_GWDIR)
			{
				setbitn(DBS_GROUPWRITABLEDIRPATHSAFE,
					DontBlameSendmail);
				ret = safedirpath(fname, uid, gid, user,
						  sfflags|SFF_SAFEDIRPATH,
						  0, 0);
				clrbitn(DBS_GROUPWRITABLEDIRPATHSAFE,
					DontBlameSendmail);
				if (ret == 0)
					sfflags |= SFF_NOPATHCHECK;
				else
					sfflags |= SFF_SAFEDIRPATH;
			}
			else
				sfflags |= SFF_SAFEDIRPATH;
			if (ret > E_PSEUDOBASE &&
			    !bitnset((forwarding ?
				      DBS_FORWARDFILEINUNSAFEDIRPATHSAFE :
				      DBS_INCLUDEFILEINUNSAFEDIRPATHSAFE),
				     DontBlameSendmail))
			{
				if (LogLevel >= 12)
					sm_syslog(LOG_INFO, e->e_id,
						  "%s: unsafe directory path, marked unsafe",
						  shortenstring(fname, MAXSHORTSTR));
				ctladdr->q_flags |= QUNSAFEADDR;
			}
		}
		*p = '/';
	}

	/* allow links only in unwritable directories */
	if (!safedir &&
	    !bitnset((forwarding ?
		      DBS_LINKEDFORWARDFILEINWRITABLEDIR :
		      DBS_LINKEDINCLUDEFILEINWRITABLEDIR),
		     DontBlameSendmail))
		sfflags |= SFF_NOLINK;

	rval = safefile(fname, uid, gid, user, sfflags, S_IREAD, &st);
	if (rval != 0)
	{
		/* don't use this :include: file */
		if (tTd(27, 4))
			dprintf("include: not safe (uid=%d): %s\n",
				(int) uid, errstring(rval));
	}
	else if ((fp = fopen(fname, "r")) == NULL)
	{
		rval = errno;
		if (tTd(27, 4))
			dprintf("include: open: %s\n", errstring(rval));
	}
	else if (filechanged(fname, fileno(fp), &st))
	{
		rval = E_SM_FILECHANGE;
		if (tTd(27, 4))
			dprintf("include: file changed after open\n");
	}
	if (ev != NULL)
		clrevent(ev);

resetuid:

#if HASSETREUID || USESETEUID
	if (saveduid == 0)
	{
		if (uid != 0)
		{
# if USESETEUID
			if (seteuid(0) < 0)
				syserr("!seteuid(0) failure (real=%d, eff=%d)",
					getuid(), geteuid());
# else /* USESETEUID */
			if (setreuid(-1, 0) < 0)
				syserr("!setreuid(-1, 0) failure (real=%d, eff=%d)",
					getuid(), geteuid());
			if (setreuid(RealUid, 0) < 0)
				syserr("!setreuid(%d, 0) failure (real=%d, eff=%d)",
					RealUid, getuid(), geteuid());
# endif /* USESETEUID */
		}
		if (setgid(savedgid) < 0)
			syserr("!setgid(%d) failure (real=%d eff=%d)",
			       savedgid, getgid(), getegid());
	}
#endif /* HASSETREUID || USESETEUID */

	if (tTd(27, 9))
		dprintf("include: reset uid = %d/%d\n",
			(int) getuid(), (int) geteuid());

	if (rval == E_SM_OPENTIMEOUT)
		usrerr("451 4.4.1 open timeout on %s", fname);

	if (fp == NULL)
		return rval;

	if (fstat(fileno(fp), &st) < 0)
	{
		rval = errno;
		syserr("Cannot fstat %s!", fname);
		(void) fclose(fp);
		return rval;
	}

	/* if path was writable, check to avoid file giveaway tricks */
	safechown = chownsafe(fileno(fp), safedir);
	if (tTd(27, 6))
		dprintf("include: parent of %s is %s, chown is %ssafe\n",
			fname,
			safedir ? "safe" : "dangerous",
			safechown ? "" : "un");

	/* if no controlling user or coming from an alias delivery */
	if (safechown &&
	    (ca == NULL ||
	     (ca->q_uid == DefUid && ca->q_gid == 0)))
	{
		ctladdr->q_uid = st.st_uid;
		ctladdr->q_gid = st.st_gid;
		ctladdr->q_flags |= QGOODUID;
	}
	if (ca != NULL && ca->q_uid == st.st_uid)
	{
		/* optimization -- avoid getpwuid if we already have info */
		ctladdr->q_flags |= ca->q_flags & QBOGUSSHELL;
		ctladdr->q_ruser = ca->q_ruser;
	}
	else if (!forwarding)
	{
		register struct passwd *pw;

		pw = sm_getpwuid(st.st_uid);
		if (pw == NULL)
		{
			ctladdr->q_uid = st.st_uid;
			ctladdr->q_flags |= QBOGUSSHELL;
		}
		else
		{
			char *sh;

			ctladdr->q_ruser = newstr(pw->pw_name);
			if (safechown)
				sh = pw->pw_shell;
			else
				sh = "/SENDMAIL/ANY/SHELL/";
			if (!usershellok(pw->pw_name, sh))
			{
				if (LogLevel >= 12)
					sm_syslog(LOG_INFO, e->e_id,
						  "%s: user %s has bad shell %s, marked %s",
						  shortenstring(fname, MAXSHORTSTR),
						  pw->pw_name, sh,
						  safechown ? "bogus" : "unsafe");
				if (safechown)
					ctladdr->q_flags |= QBOGUSSHELL;
				else
					ctladdr->q_flags |= QUNSAFEADDR;
			}
		}
	}

	if (bitset(EF_VRFYONLY, e->e_flags))
	{
		/* don't do any more now */
		ctladdr->q_state = QS_VERIFIED;
		e->e_nrcpts++;
		(void) fclose(fp);
		return rval;
	}

	/*
	**  Check to see if some bad guy can write this file
	**
	**	Group write checking could be more clever, e.g.,
	**	guessing as to which groups are actually safe ("sys"
	**	may be; "user" probably is not).
	*/

	mode = S_IWOTH;
	if (!bitnset((forwarding ?
		      DBS_GROUPWRITABLEFORWARDFILESAFE :
		      DBS_GROUPWRITABLEINCLUDEFILESAFE),
		     DontBlameSendmail))
		mode |= S_IWGRP;

	if (bitset(mode, st.st_mode))
	{
		if (tTd(27, 6))
			dprintf("include: %s is %s writable, marked unsafe\n",
				shortenstring(fname, MAXSHORTSTR),
				bitset(S_IWOTH, st.st_mode) ? "world" : "group");
		if (LogLevel >= 12)
			sm_syslog(LOG_INFO, e->e_id,
				  "%s: %s writable %s file, marked unsafe",
				  shortenstring(fname, MAXSHORTSTR),
				  bitset(S_IWOTH, st.st_mode) ? "world" : "group",
				  forwarding ? "forward" : ":include:");
		ctladdr->q_flags |= QUNSAFEADDR;
	}

	/* read the file -- each line is a comma-separated list. */
	FileName = fname;
	LineNumber = 0;
	ctladdr->q_flags &= ~QSELFREF;
	nincludes = 0;
	while (fgets(buf, sizeof buf, fp) != NULL && !maxreached)
	{
		fixcrlf(buf, TRUE);
		LineNumber++;
		if (buf[0] == '#' || buf[0] == '\0')
			continue;

		/* <sp>#@# introduces a comment anywhere */
		/* for Japanese character sets */
		for (p = buf; (p = strchr(++p, '#')) != NULL; )
		{
			if (p[1] == '@' && p[2] == '#' &&
			    isascii(p[-1]) && isspace(p[-1]) &&
			    (p[3] == '\0' || (isascii(p[3]) && isspace(p[3]))))
			{
				--p;
				while (p > buf && isascii(p[-1]) &&
				       isspace(p[-1]))
					--p;
				p[0] = '\0';
				break;
			}
		}
		if (buf[0] == '\0')
			continue;

		e->e_to = NULL;
		message("%s to %s",
			forwarding ? "forwarding" : "sending", buf);
		if (forwarding && LogLevel > 10)
			sm_syslog(LOG_INFO, e->e_id,
				  "forward %.200s => %s",
				  oldto, shortenstring(buf, MAXSHORTSTR));

		nincludes += sendtolist(buf, ctladdr, sendq, aliaslevel + 1, e);

		if (forwarding &&
		    MaxForwardEntries > 0 &&
		    nincludes >= MaxForwardEntries)
		{
			/* just stop reading and processing further entries */
			/* additional: (?)
			ctladdr->q_state = QS_DONTSEND;
			**/
			syserr("Attempt to forward to more then %d addresses (in %s)!",
				MaxForwardEntries,fname);
			maxreached = TRUE;
		}
	}

	if (ferror(fp) && tTd(27, 3))
		dprintf("include: read error: %s\n", errstring(errno));
	if (nincludes > 0 && !bitset(QSELFREF, ctladdr->q_flags))
	{
		if (tTd(27, 5))
		{
			dprintf("include: QS_DONTSEND ");
			printaddr(ctladdr, FALSE);
		}
		ctladdr->q_state = QS_DONTSEND;
	}

	(void) fclose(fp);
	FileName = oldfilename;
	LineNumber = oldlinenumber;
	e->e_to = oldto;
	return rval;
}

static void
includetimeout()
{
	/*
	**  NOTE: THIS CAN BE CALLED FROM A SIGNAL HANDLER.  DO NOT ADD
	**	ANYTHING TO THIS ROUTINE UNLESS YOU KNOW WHAT YOU ARE
	**	DOING.
	*/

	errno = ETIMEDOUT;
	longjmp(CtxIncludeTimeout, 1);
}
/*
**  SENDTOARGV -- send to an argument vector.
**
**	Parameters:
**		argv -- argument vector to send to.
**		e -- the current envelope.
**
**	Returns:
**		none.
**
**	Side Effects:
**		puts all addresses on the argument vector onto the
**			send queue.
*/

void
sendtoargv(argv, e)
	register char **argv;
	register ENVELOPE *e;
{
	register char *p;

	while ((p = *argv++) != NULL)
	{
		(void) sendtolist(p, NULLADDR, &e->e_sendqueue, 0, e);
	}
}
/*
**  GETCTLADDR -- get controlling address from an address header.
**
**	If none, get one corresponding to the effective userid.
**
**	Parameters:
**		a -- the address to find the controller of.
**
**	Returns:
**		the controlling address.
**
**	Side Effects:
**		none.
*/

ADDRESS *
getctladdr(a)
	register ADDRESS *a;
{
	while (a != NULL && !bitset(QGOODUID, a->q_flags))
		a = a->q_alias;
	return a;
}
/*
**  SELF_REFERENCE -- check to see if an address references itself
**
**	The check is done through a chain of aliases.  If it is part of
**	a loop, break the loop at the "best" address, that is, the one
**	that exists as a real user.
**
**	This is to handle the case of:
**		awc:		Andrew.Chang
**		Andrew.Chang:	awc@mail.server
**	which is a problem only on mail.server.
**
**	Parameters:
**		a -- the address to check.
**
**	Returns:
**		The address that should be retained.
*/

static ADDRESS *
self_reference(a)
	ADDRESS *a;
{
	ADDRESS *b;		/* top entry in self ref loop */
	ADDRESS *c;		/* entry that point to a real mail box */

	if (tTd(27, 1))
		dprintf("self_reference(%s)\n", a->q_paddr);

	for (b = a->q_alias; b != NULL; b = b->q_alias)
	{
		if (sameaddr(a, b))
			break;
	}

	if (b == NULL)
	{
		if (tTd(27, 1))
			dprintf("\t... no self ref\n");
		return NULL;
	}

	/*
	**  Pick the first address that resolved to a real mail box
	**  i.e has a pw entry.  The returned value will be marked
	**  QSELFREF in recipient(), which in turn will disable alias()
	**  from marking it as QS_IS_DEAD(), which mean it will be used
	**  as a deliverable address.
	**
	**  The 2 key thing to note here are:
	**	1) we are in a recursive call sequence:
	**		alias->sentolist->recipient->alias
	**	2) normally, when we return back to alias(), the address
	**	   will be marked QS_EXPANDED, since alias() assumes the
	**	   expanded form will be used instead of the current address.
	**	   This behaviour is turned off if the address is marked
	**	   QSELFREF We set QSELFREF when we return to recipient().
	*/

	c = a;
	while (c != NULL)
	{
		if (tTd(27, 10))
			dprintf("  %s", c->q_user);
		if (bitnset(M_HASPWENT, c->q_mailer->m_flags))
		{
			if (tTd(27, 2))
				dprintf("\t... getpwnam(%s)... ", c->q_user);
			if (sm_getpwnam(c->q_user) != NULL)
			{
				if (tTd(27, 2))
					dprintf("found\n");

				/* ought to cache results here */
				if (sameaddr(b, c))
					return b;
				else
					return c;
			}
			if (tTd(27, 2))
				dprintf("failed\n");
		}
		else
		{
			/* if local delivery, compare usernames */
			if (bitnset(M_LOCALMAILER, c->q_mailer->m_flags) &&
			    b->q_mailer == c->q_mailer)
			{
				if (tTd(27, 2))
					dprintf("\t... local match (%s)\n",
						c->q_user);
				if (sameaddr(b, c))
					return b;
				else
					return c;
			}
		}
		if (tTd(27, 10))
			dprintf("\n");
		c = c->q_alias;
	}

	if (tTd(27, 1))
		dprintf("\t... cannot break loop for \"%s\"\n", a->q_paddr);

	return NULL;
}
