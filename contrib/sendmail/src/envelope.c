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
static char id[] = "@(#)$Id: envelope.c,v 8.180.14.3 2000/06/29 05:30:23 gshapiro Exp $";
#endif /* ! lint */

#include <sendmail.h>


/*
**  NEWENVELOPE -- allocate a new envelope
**
**	Supports inheritance.
**
**	Parameters:
**		e -- the new envelope to fill in.
**		parent -- the envelope to be the parent of e.
**
**	Returns:
**		e.
**
**	Side Effects:
**		none.
*/

ENVELOPE *
newenvelope(e, parent)
	register ENVELOPE *e;
	register ENVELOPE *parent;
{
	if (e == parent && e->e_parent != NULL)
		parent = e->e_parent;
	clearenvelope(e, TRUE);
	if (e == CurEnv)
		memmove((char *) &e->e_from,
			(char *) &NullAddress,
			sizeof e->e_from);
	else
		memmove((char *) &e->e_from,
			(char *) &CurEnv->e_from,
			sizeof e->e_from);
	e->e_parent = parent;
	assign_queueid(e);
	e->e_ctime = curtime();
	if (parent != NULL)
		e->e_msgpriority = parent->e_msgsize;
	e->e_puthdr = putheader;
	e->e_putbody = putbody;
	if (CurEnv->e_xfp != NULL)
		(void) fflush(CurEnv->e_xfp);

	return e;
}
/*
**  DROPENVELOPE -- deallocate an envelope.
**
**	Parameters:
**		e -- the envelope to deallocate.
**		fulldrop -- if set, do return receipts.
**
**	Returns:
**		none.
**
**	Side Effects:
**		housekeeping necessary to dispose of an envelope.
**		Unlocks this queue file.
*/

void
dropenvelope(e, fulldrop)
	register ENVELOPE *e;
	bool fulldrop;
{
	bool queueit = FALSE;
	bool message_timeout = FALSE;
	bool failure_return = FALSE;
	bool delay_return = FALSE;
	bool success_return = FALSE;
	bool pmnotify = bitset(EF_PM_NOTIFY, e->e_flags);
	bool done = FALSE;
	register ADDRESS *q;
	char *id = e->e_id;
	char buf[MAXLINE];

	if (tTd(50, 1))
	{
		dprintf("dropenvelope %lx: id=", (u_long) e);
		xputs(e->e_id);
		dprintf(", flags=");
		printenvflags(e);
		if (tTd(50, 10))
		{
			dprintf("sendq=");
			printaddr(e->e_sendqueue, TRUE);
		}
	}

	if (LogLevel > 84)
		sm_syslog(LOG_DEBUG, id,
			  "dropenvelope, e_flags=0x%lx, OpMode=%c, pid=%d",
			  e->e_flags, OpMode, getpid());

	/* we must have an id to remove disk files */
	if (id == NULL)
		return;

	/* if verify-only mode, we can skip most of this */
	if (OpMode == MD_VERIFY)
		goto simpledrop;

	if (LogLevel > 4 && bitset(EF_LOGSENDER, e->e_flags))
		logsender(e, NULL);
	e->e_flags &= ~EF_LOGSENDER;

	/* post statistics */
	poststats(StatFile);

	/*
	**  Extract state information from dregs of send list.
	*/

	if (curtime() > e->e_ctime + TimeOuts.to_q_return[e->e_timeoutclass])
		message_timeout = TRUE;

	if (TimeOuts.to_q_return[e->e_timeoutclass] == NOW &&
	    !bitset(EF_RESPONSE, e->e_flags))
	{
		message_timeout = TRUE;
		e->e_flags |= EF_FATALERRS|EF_CLRQUEUE;
	}

	e->e_flags &= ~EF_QUEUERUN;
	for (q = e->e_sendqueue; q != NULL; q = q->q_next)
	{
		if (QS_IS_UNDELIVERED(q->q_state))
			queueit = TRUE;

		/* see if a notification is needed */
		if (bitset(QPINGONFAILURE, q->q_flags) &&
		    ((message_timeout && QS_IS_QUEUEUP(q->q_state)) ||
		     QS_IS_BADADDR(q->q_state) ||
		     (TimeOuts.to_q_return[e->e_timeoutclass] == NOW &&
		      !bitset(EF_RESPONSE, e->e_flags))))

		{
			failure_return = TRUE;
			if (!done && q->q_owner == NULL &&
			    !emptyaddr(&e->e_from))
			{
				(void) sendtolist(e->e_from.q_paddr, NULLADDR,
						  &e->e_errorqueue, 0, e);
				done = TRUE;
			}
		}
		else if (bitset(QPINGONSUCCESS, q->q_flags) &&
			 ((QS_IS_SENT(q->q_state) &&
			   bitnset(M_LOCALMAILER, q->q_mailer->m_flags)) ||
			  bitset(QRELAYED|QEXPANDED|QDELIVERED, q->q_flags)))
		{
			success_return = TRUE;
		}
	}

	if (e->e_class < 0)
		e->e_flags |= EF_NO_BODY_RETN;

	/*
	**  See if the message timed out.
	*/

	if (!queueit)
		/* EMPTY */
		/* nothing to do */ ;
	else if (message_timeout)
	{
		if (failure_return)
		{
			(void) snprintf(buf, sizeof buf,
					"Cannot send message for %s",
					pintvl(TimeOuts.to_q_return[e->e_timeoutclass], FALSE));
			if (e->e_message != NULL)
				free(e->e_message);
			e->e_message = newstr(buf);
			message(buf);
			e->e_flags |= EF_CLRQUEUE;
		}
		fprintf(e->e_xfp, "Message could not be delivered for %s\n",
			pintvl(TimeOuts.to_q_return[e->e_timeoutclass], FALSE));
		fprintf(e->e_xfp, "Message will be deleted from queue\n");
		for (q = e->e_sendqueue; q != NULL; q = q->q_next)
		{
			if (QS_IS_UNDELIVERED(q->q_state))
			{
				q->q_state = QS_BADADDR;
				q->q_status = "4.4.7";
			}
		}
	}
	else if (TimeOuts.to_q_warning[e->e_timeoutclass] > 0 &&
	    curtime() > e->e_ctime + TimeOuts.to_q_warning[e->e_timeoutclass])
	{
		if (!bitset(EF_WARNING|EF_RESPONSE, e->e_flags) &&
		    e->e_class >= 0 &&
		    e->e_from.q_paddr != NULL &&
		    strcmp(e->e_from.q_paddr, "<>") != 0 &&
		    strncasecmp(e->e_from.q_paddr, "owner-", 6) != 0 &&
		    (strlen(e->e_from.q_paddr) <= (SIZE_T) 8 ||
		     strcasecmp(&e->e_from.q_paddr[strlen(e->e_from.q_paddr) - 8], "-request") != 0))
		{
			for (q = e->e_sendqueue; q != NULL; q = q->q_next)
			{
				if (QS_IS_QUEUEUP(q->q_state) &&
#if _FFR_NODELAYDSN_ON_HOLD
				    !bitnset(M_HOLD, q->q_mailer->m_flags) &&
#endif /* _FFR_NODELAYDSN_ON_HOLD */
				    bitset(QPINGONDELAY, q->q_flags))
				{
					q->q_flags |= QDELAYED;
					delay_return = TRUE;
				}
			}
		}
		if (delay_return)
		{
			(void) snprintf(buf, sizeof buf,
				"Warning: could not send message for past %s",
				pintvl(TimeOuts.to_q_warning[e->e_timeoutclass], FALSE));
			if (e->e_message != NULL)
				free(e->e_message);
			e->e_message = newstr(buf);
			message(buf);
			e->e_flags |= EF_WARNING;
		}
		fprintf(e->e_xfp,
			"Warning: message still undelivered after %s\n",
			pintvl(TimeOuts.to_q_warning[e->e_timeoutclass], FALSE));
		fprintf(e->e_xfp, "Will keep trying until message is %s old\n",
			pintvl(TimeOuts.to_q_return[e->e_timeoutclass], FALSE));
	}

	if (tTd(50, 2))
		dprintf("failure_return=%d delay_return=%d success_return=%d queueit=%d\n",
			failure_return, delay_return, success_return, queueit);

	/*
	**  If we had some fatal error, but no addresses are marked as
	**  bad, mark them _all_ as bad.
	*/

	if (bitset(EF_FATALERRS, e->e_flags) && !failure_return)
	{
		for (q = e->e_sendqueue; q != NULL; q = q->q_next)
		{
			if ((QS_IS_OK(q->q_state) ||
			     QS_IS_VERIFIED(q->q_state)) &&
			    bitset(QPINGONFAILURE, q->q_flags))
			{
				failure_return = TRUE;
				q->q_state = QS_BADADDR;
			}
		}
	}

	/*
	**  Send back return receipts as requested.
	*/

	if (success_return && !failure_return && !delay_return && fulldrop &&
	    !bitset(PRIV_NORECEIPTS, PrivacyFlags) &&
	    strcmp(e->e_from.q_paddr, "<>") != 0)
	{
		auto ADDRESS *rlist = NULL;

		if (tTd(50, 8))
			dprintf("dropenvelope(%s): sending return receipt\n",
				id);
		e->e_flags |= EF_SENDRECEIPT;
		(void) sendtolist(e->e_from.q_paddr, NULLADDR, &rlist, 0, e);
		(void) returntosender("Return receipt", rlist, RTSF_NO_BODY, e);
	}
	e->e_flags &= ~EF_SENDRECEIPT;

	/*
	**  Arrange to send error messages if there are fatal errors.
	*/

	if ((failure_return || delay_return) && e->e_errormode != EM_QUIET)
	{
		if (tTd(50, 8))
			dprintf("dropenvelope(%s): saving mail\n", id);
		savemail(e, !bitset(EF_NO_BODY_RETN, e->e_flags));
	}

	/*
	**  Arrange to send warning messages to postmaster as requested.
	*/

	if ((failure_return || pmnotify) &&
	    PostMasterCopy != NULL &&
	    !bitset(EF_RESPONSE, e->e_flags) &&
	    e->e_class >= 0)
	{
		auto ADDRESS *rlist = NULL;
		char pcopy[MAXNAME];

		if (failure_return)
		{
			expand(PostMasterCopy, pcopy, sizeof pcopy, e);

			if (tTd(50, 8))
				dprintf("dropenvelope(%s): sending postmaster copy to %s\n",
					id, pcopy);
			(void) sendtolist(pcopy, NULLADDR, &rlist, 0, e);
		}
		if (pmnotify)
			(void) sendtolist("postmaster", NULLADDR,
					  &rlist, 0, e);
		(void) returntosender(e->e_message, rlist,
				      RTSF_PM_BOUNCE|RTSF_NO_BODY, e);
	}

	/*
	**  Instantiate or deinstantiate the queue.
	*/

simpledrop:
	if (tTd(50, 8))
		dprintf("dropenvelope(%s): at simpledrop, queueit=%d\n",
			id, queueit);
	if (!queueit || bitset(EF_CLRQUEUE, e->e_flags))
	{
		if (tTd(50, 1))
		{
			dprintf("\n===== Dropping [dq]f%s... queueit=%d, e_flags=",
				e->e_id, queueit);
			printenvflags(e);
		}
		xunlink(queuename(e, 'd'));
		xunlink(queuename(e, 'q'));

		if (e->e_ntries > 0 && LogLevel > 9)
			sm_syslog(LOG_INFO, id, "done; delay=%s, ntries=%d",
				  pintvl(curtime() - e->e_ctime, TRUE),
				  e->e_ntries);
	}
	else if (queueit || !bitset(EF_INQUEUE, e->e_flags))
	{
#if QUEUE
		queueup(e, FALSE);
#else /* QUEUE */
		syserr("554 5.3.0 dropenvelope: queueup");
#endif /* QUEUE */
	}

	/* now unlock the job */
	if (tTd(50, 8))
		dprintf("dropenvelope(%s): unlocking job\n", id);
	closexscript(e);
	unlockqueue(e);

	/* make sure that this envelope is marked unused */
	if (e->e_dfp != NULL)
		(void) bfclose(e->e_dfp);
	e->e_dfp = NULL;
	e->e_id = NULL;
	e->e_flags &= ~EF_HAS_DF;
}
/*
**  CLEARENVELOPE -- clear an envelope without unlocking
**
**	This is normally used by a child process to get a clean
**	envelope without disturbing the parent.
**
**	Parameters:
**		e -- the envelope to clear.
**		fullclear - if set, the current envelope is total
**			garbage and should be ignored; otherwise,
**			release any resources it may indicate.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Closes files associated with the envelope.
**		Marks the envelope as unallocated.
*/

void
clearenvelope(e, fullclear)
	register ENVELOPE *e;
	bool fullclear;
{
	register HDR *bh;
	register HDR **nhp;
	extern ENVELOPE BlankEnvelope;

	if (!fullclear)
	{
		/* clear out any file information */
		if (e->e_xfp != NULL)
			(void) bfclose(e->e_xfp);
		if (e->e_dfp != NULL)
			(void) bfclose(e->e_dfp);
		e->e_xfp = e->e_dfp = NULL;
	}

	/* now clear out the data */
	STRUCTCOPY(BlankEnvelope, *e);
	e->e_message = NULL;
	if (Verbose)
		set_delivery_mode(SM_DELIVER, e);
	bh = BlankEnvelope.e_header;
	nhp = &e->e_header;
	while (bh != NULL)
	{
		*nhp = (HDR *) xalloc(sizeof *bh);
		memmove((char *) *nhp, (char *) bh, sizeof *bh);
		bh = bh->h_link;
		nhp = &(*nhp)->h_link;
	}
}
/*
**  INITSYS -- initialize instantiation of system
**
**	In Daemon mode, this is done in the child.
**
**	Parameters:
**		e -- the envelope to use.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Initializes the system macros, some global variables,
**		etc.  In particular, the current time in various
**		forms is set.
*/

void
initsys(e)
	register ENVELOPE *e;
{
	char cbuf[5];				/* holds hop count */
	char pbuf[10];				/* holds pid */
#ifdef TTYNAME
	static char ybuf[60];			/* holds tty id */
	register char *p;
	extern char *ttyname();
#endif /* TTYNAME */

	/*
	**  Give this envelope a reality.
	**	I.e., an id, a transcript, and a creation time.
	*/

	setnewqueue(e);
	openxscript(e);
	e->e_ctime = curtime();
#if _FFR_QUEUEDELAY
	e->e_queuealg = QueueAlg;
	e->e_queuedelay = QueueInitDelay;
#endif /* _FFR_QUEUEDELAY */

	/*
	**  Set OutChannel to something useful if stdout isn't it.
	**	This arranges that any extra stuff the mailer produces
	**	gets sent back to the user on error (because it is
	**	tucked away in the transcript).
	*/

	if (OpMode == MD_DAEMON && bitset(EF_QUEUERUN, e->e_flags) &&
	    e->e_xfp != NULL)
		OutChannel = e->e_xfp;

	/*
	**  Set up some basic system macros.
	*/

	/* process id */
	(void) snprintf(pbuf, sizeof pbuf, "%d", (int) getpid());
	define('p', newstr(pbuf), e);

	/* hop count */
	(void) snprintf(cbuf, sizeof cbuf, "%d", e->e_hopcount);
	define('c', newstr(cbuf), e);

	/* time as integer, unix time, arpa time */
	settime(e);

	/* Load average */
	(void)sm_getla(e);

#ifdef TTYNAME
	/* tty name */
	if (macvalue('y', e) == NULL)
	{
		p = ttyname(2);
		if (p != NULL)
		{
			if (strrchr(p, '/') != NULL)
				p = strrchr(p, '/') + 1;
			snprintf(ybuf, sizeof ybuf, "%s", p);
			define('y', ybuf, e);
		}
	}
#endif /* TTYNAME */
}
/*
**  SETTIME -- set the current time.
**
**	Parameters:
**		e -- the envelope in which the macros should be set.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Sets the various time macros -- $a, $b, $d, $t.
*/

void
settime(e)
	register ENVELOPE *e;
{
	register char *p;
	auto time_t now;
	char tbuf[20];				/* holds "current" time */
	char dbuf[30];				/* holds ctime(tbuf) */
	register struct tm *tm;

	now = curtime();
	tm = gmtime(&now);
	(void) snprintf(tbuf, sizeof tbuf, "%04d%02d%02d%02d%02d", tm->tm_year + 1900,
			tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min);
	define('t', newstr(tbuf), e);
	(void) strlcpy(dbuf, ctime(&now), sizeof dbuf);
	p = strchr(dbuf, '\n');
	if (p != NULL)
		*p = '\0';
	define('d', newstr(dbuf), e);
	p = arpadate(dbuf);
	p = newstr(p);
	if (macvalue('a', e) == NULL)
		define('a', p, e);
	define('b', p, e);
}
/*
**  OPENXSCRIPT -- Open transcript file
**
**	Creates a transcript file for possible eventual mailing or
**	sending back.
**
**	Parameters:
**		e -- the envelope to create the transcript in/for.
**
**	Returns:
**		none
**
**	Side Effects:
**		Creates the transcript file.
*/

#ifndef O_APPEND
# define O_APPEND	0
#endif /* ! O_APPEND */

void
openxscript(e)
	register ENVELOPE *e;
{
	register char *p;

	if (e->e_xfp != NULL)
		return;

#if 0
	if (e->e_lockfp == NULL && bitset(EF_INQUEUE, e->e_flags))
		syserr("openxscript: job not locked");
#endif /* 0 */

	p = queuename(e, 'x');
	e->e_xfp = bfopen(p, FileMode, XscriptFileBufferSize,
			  SFF_NOTEXCL|SFF_OPENASROOT);

	if (e->e_xfp == NULL)
	{
		syserr("Can't create transcript file %s", p);
		e->e_xfp = fopen("/dev/null", "r+");
		if (e->e_xfp == NULL)
			syserr("!Can't open /dev/null");
	}
#if HASSETVBUF
	(void) setvbuf(e->e_xfp, NULL, _IOLBF, 0);
#else /* HASSETVBUF */
	(void) setlinebuf(e->e_xfp);
#endif /* HASSETVBUF */
	if (tTd(46, 9))
	{
		dprintf("openxscript(%s):\n  ", p);
		dumpfd(fileno(e->e_xfp), TRUE, FALSE);
	}
}
/*
**  CLOSEXSCRIPT -- close the transcript file.
**
**	Parameters:
**		e -- the envelope containing the transcript to close.
**
**	Returns:
**		none.
**
**	Side Effects:
**		none.
*/

void
closexscript(e)
	register ENVELOPE *e;
{
	if (e->e_xfp == NULL)
		return;
#if 0
	if (e->e_lockfp == NULL)
		syserr("closexscript: job not locked");
#endif /* 0 */
	(void) bfclose(e->e_xfp);
	e->e_xfp = NULL;
}
/*
**  SETSENDER -- set the person who this message is from
**
**	Under certain circumstances allow the user to say who
**	s/he is (using -f or -r).  These are:
**	1.  The user's uid is zero (root).
**	2.  The user's login name is in an approved list (typically
**	    from a network server).
**	3.  The address the user is trying to claim has a
**	    "!" character in it (since #2 doesn't do it for
**	    us if we are dialing out for UUCP).
**	A better check to replace #3 would be if the
**	effective uid is "UUCP" -- this would require me
**	to rewrite getpwent to "grab" uucp as it went by,
**	make getname more nasty, do another passwd file
**	scan, or compile the UID of "UUCP" into the code,
**	all of which are reprehensible.
**
**	Assuming all of these fail, we figure out something
**	ourselves.
**
**	Parameters:
**		from -- the person we would like to believe this message
**			is from, as specified on the command line.
**		e -- the envelope in which we would like the sender set.
**		delimptr -- if non-NULL, set to the location of the
**			trailing delimiter.
**		delimchar -- the character that will delimit the sender
**			address.
**		internal -- set if this address is coming from an internal
**			source such as an owner alias.
**
**	Returns:
**		none.
**
**	Side Effects:
**		sets sendmail's notion of who the from person is.
*/

void
setsender(from, e, delimptr, delimchar, internal)
	char *from;
	register ENVELOPE *e;
	char **delimptr;
	int delimchar;
	bool internal;
{
	register char **pvp;
	char *realname = NULL;
	register struct passwd *pw;
	char *bp;
	char buf[MAXNAME + 2];
	char pvpbuf[PSBUFSIZE];
	extern char *FullName;

	if (tTd(45, 1))
		dprintf("setsender(%s)\n", from == NULL ? "" : from);

	/*
	**  Figure out the real user executing us.
	**	Username can return errno != 0 on non-errors.
	*/

	if (bitset(EF_QUEUERUN, e->e_flags) || OpMode == MD_SMTP ||
	    OpMode == MD_ARPAFTP || OpMode == MD_DAEMON)
		realname = from;
	if (realname == NULL || realname[0] == '\0')
		realname = username();

	if (ConfigLevel < 2)
		SuprErrs = TRUE;

#if _FFR_ADDR_TYPE
	define(macid("{addr_type}", NULL), "e s", e);
#endif /* _FFR_ADDR_TYPE */
	/* preset state for then clause in case from == NULL */
	e->e_from.q_state = QS_BADADDR;
	e->e_from.q_flags = 0;
	if (from == NULL ||
	    parseaddr(from, &e->e_from, RF_COPYALL|RF_SENDERADDR,
		      delimchar, delimptr, e) == NULL ||
	    QS_IS_BADADDR(e->e_from.q_state) ||
	    e->e_from.q_mailer == ProgMailer ||
	    e->e_from.q_mailer == FileMailer ||
	    e->e_from.q_mailer == InclMailer)
	{
		/* log garbage addresses for traceback */
		if (from != NULL && LogLevel > 2)
		{
			char *p;
			char ebuf[MAXNAME * 2 + 2];

			p = macvalue('_', e);
			if (p == NULL)
			{
				char *host = RealHostName;

				if (host == NULL)
					host = MyHostName;
				(void) snprintf(ebuf, sizeof ebuf, "%.*s@%.*s",
					MAXNAME, realname,
					MAXNAME, host);
				p = ebuf;
			}
			sm_syslog(LOG_NOTICE, e->e_id,
				  "setsender: %s: invalid or unparsable, received from %s",
				  shortenstring(from, 83), p);
		}
		if (from != NULL)
		{
			if (!QS_IS_BADADDR(e->e_from.q_state))
			{
				/* it was a bogus mailer in the from addr */
				e->e_status = "5.1.7";
				usrerrenh(e->e_status,
					  "553 Invalid sender address");
			}
			SuprErrs = TRUE;
		}
		if (from == realname ||
		    parseaddr(from = newstr(realname), &e->e_from,
			      RF_COPYALL|RF_SENDERADDR, ' ', NULL, e) == NULL)
		{
			char nbuf[100];

			SuprErrs = TRUE;
			expand("\201n", nbuf, sizeof nbuf, e);
			if (parseaddr(from = newstr(nbuf), &e->e_from,
				      RF_COPYALL, ' ', NULL, e) == NULL &&
			    parseaddr(from = "postmaster", &e->e_from,
				      RF_COPYALL, ' ', NULL, e) == NULL)
				syserr("553 5.3.0 setsender: can't even parse postmaster!");
		}
	}
	else
		FromFlag = TRUE;
	e->e_from.q_state = QS_SENDER;
	if (tTd(45, 5))
	{
		dprintf("setsender: QS_SENDER ");
		printaddr(&e->e_from, FALSE);
	}
	SuprErrs = FALSE;

#if USERDB
	if (bitnset(M_CHECKUDB, e->e_from.q_mailer->m_flags))
	{
		register char *p;

		p = udbsender(e->e_from.q_user);
		if (p != NULL)
			from = p;
	}
#endif /* USERDB */

	if (bitnset(M_HASPWENT, e->e_from.q_mailer->m_flags))
	{
		if (!internal)
		{
			/* if the user already given fullname don't redefine */
			if (FullName == NULL)
				FullName = macvalue('x', e);
			if (FullName != NULL && FullName[0] == '\0')
				FullName = NULL;
		}

		if (e->e_from.q_user[0] != '\0' &&
		    (pw = sm_getpwnam(e->e_from.q_user)) != NULL)
		{
			/*
			**  Process passwd file entry.
			*/

			/* extract home directory */
			if (strcmp(pw->pw_dir, "/") == 0)
				e->e_from.q_home = newstr("");
			else
				e->e_from.q_home = newstr(pw->pw_dir);
			define('z', e->e_from.q_home, e);

			/* extract user and group id */
			e->e_from.q_uid = pw->pw_uid;
			e->e_from.q_gid = pw->pw_gid;
			e->e_from.q_flags |= QGOODUID;

			/* extract full name from passwd file */
			if (FullName == NULL && pw->pw_gecos != NULL &&
			    strcmp(pw->pw_name, e->e_from.q_user) == 0 &&
			    !internal)
			{
				buildfname(pw->pw_gecos, e->e_from.q_user, buf, sizeof buf);
				if (buf[0] != '\0')
					FullName = newstr(buf);
			}
		}
		else
		{
			e->e_from.q_home = NULL;
		}
		if (FullName != NULL && !internal)
			define('x', FullName, e);
	}
	else if (!internal && OpMode != MD_DAEMON && OpMode != MD_SMTP)
	{
		if (e->e_from.q_home == NULL)
		{
			e->e_from.q_home = getenv("HOME");
			if (e->e_from.q_home != NULL &&
			    strcmp(e->e_from.q_home, "/") == 0)
				e->e_from.q_home++;
		}
		e->e_from.q_uid = RealUid;
		e->e_from.q_gid = RealGid;
		e->e_from.q_flags |= QGOODUID;
	}

	/*
	**  Rewrite the from person to dispose of possible implicit
	**	links in the net.
	*/

	pvp = prescan(from, delimchar, pvpbuf, sizeof pvpbuf, NULL, NULL);
	if (pvp == NULL)
	{
		/* don't need to give error -- prescan did that already */
		if (LogLevel > 2)
			sm_syslog(LOG_NOTICE, e->e_id,
				  "cannot prescan from (%s)",
				  shortenstring(from, MAXSHORTSTR));
		finis(TRUE, ExitStat);
	}
	(void) rewrite(pvp, 3, 0, e);
	(void) rewrite(pvp, 1, 0, e);
	(void) rewrite(pvp, 4, 0, e);
#if _FFR_ADDR_TYPE
	define(macid("{addr_type}", NULL), NULL, e);
#endif /* _FFR_ADDR_TYPE */
	bp = buf + 1;
	cataddr(pvp, NULL, bp, sizeof buf - 2, '\0');
	if (*bp == '@' && !bitnset(M_NOBRACKET, e->e_from.q_mailer->m_flags))
	{
		/* heuristic: route-addr: add angle brackets */
		(void) strlcat(bp, ">", sizeof buf - 1);
		*--bp = '<';
	}
	e->e_sender = newstr(bp);
	define('f', e->e_sender, e);

	/* save the domain spec if this mailer wants it */
	if (e->e_from.q_mailer != NULL &&
	    bitnset(M_CANONICAL, e->e_from.q_mailer->m_flags))
	{
		char **lastat;

		/* get rid of any pesky angle brackets */
#if _FFR_ADDR_TYPE
		define(macid("{addr_type}", NULL), "e s", e);
#endif /* _FFR_ADDR_TYPE */
		(void) rewrite(pvp, 3, 0, e);
		(void) rewrite(pvp, 1, 0, e);
		(void) rewrite(pvp, 4, 0, e);
#if _FFR_ADDR_TYPE
		define(macid("{addr_type}", NULL), NULL, e);
#endif /* _FFR_ADDR_TYPE */

		/* strip off to the last "@" sign */
		for (lastat = NULL; *pvp != NULL; pvp++)
			if (strcmp(*pvp, "@") == 0)
				lastat = pvp;
		if (lastat != NULL)
		{
			e->e_fromdomain = copyplist(lastat, TRUE);
			if (tTd(45, 3))
			{
				dprintf("Saving from domain: ");
				printav(e->e_fromdomain);
			}
		}
	}
}
/*
**  PRINTENVFLAGS -- print envelope flags for debugging
**
**	Parameters:
**		e -- the envelope with the flags to be printed.
**
**	Returns:
**		none.
*/

struct eflags
{
	char	*ef_name;
	u_long	ef_bit;
};

static struct eflags	EnvelopeFlags[] =
{
	{ "OLDSTYLE",		EF_OLDSTYLE	},
	{ "INQUEUE",		EF_INQUEUE	},
	{ "NO_BODY_RETN",	EF_NO_BODY_RETN	},
	{ "CLRQUEUE",		EF_CLRQUEUE	},
	{ "SENDRECEIPT",	EF_SENDRECEIPT	},
	{ "FATALERRS",		EF_FATALERRS	},
	{ "DELETE_BCC",		EF_DELETE_BCC	},
	{ "RESPONSE",		EF_RESPONSE	},
	{ "RESENT",		EF_RESENT	},
	{ "VRFYONLY",		EF_VRFYONLY	},
	{ "WARNING",		EF_WARNING	},
	{ "QUEUERUN",		EF_QUEUERUN	},
	{ "GLOBALERRS",		EF_GLOBALERRS	},
	{ "PM_NOTIFY",		EF_PM_NOTIFY	},
	{ "METOO",		EF_METOO	},
	{ "LOGSENDER",		EF_LOGSENDER	},
	{ "NORECEIPT",		EF_NORECEIPT	},
	{ "HAS8BIT",		EF_HAS8BIT	},
	{ "NL_NOT_EOL",		EF_NL_NOT_EOL	},
	{ "CRLF_NOT_EOL",	EF_CRLF_NOT_EOL	},
	{ "RET_PARAM",		EF_RET_PARAM	},
	{ "HAS_DF",		EF_HAS_DF	},
	{ "IS_MIME",		EF_IS_MIME	},
	{ "DONT_MIME",		EF_DONT_MIME	},
	{ NULL }
};

void
printenvflags(e)
	register ENVELOPE *e;
{
	register struct eflags *ef;
	bool first = TRUE;

	printf("%lx", e->e_flags);
	for (ef = EnvelopeFlags; ef->ef_name != NULL; ef++)
	{
		if (!bitset(ef->ef_bit, e->e_flags))
			continue;
		if (first)
			printf("<%s", ef->ef_name);
		else
			printf(",%s", ef->ef_name);
		first = FALSE;
	}
	if (!first)
		printf(">\n");
}
