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
static char sccsid[] = "@(#)savemail.c	8.28 (Berkeley) 3/11/94";
#endif /* not lint */

# include "sendmail.h"
# include <pwd.h>

/*
**  SAVEMAIL -- Save mail on error
**
**	If mailing back errors, mail it back to the originator
**	together with an error message; otherwise, just put it in
**	dead.letter in the user's home directory (if he exists on
**	this machine).
**
**	Parameters:
**		e -- the envelope containing the message in error.
**
**	Returns:
**		none
**
**	Side Effects:
**		Saves the letter, by writing or mailing it back to the
**		sender, or by putting it in dead.letter in her home
**		directory.
*/

/* defines for state machine */
# define ESM_REPORT	0	/* report to sender's terminal */
# define ESM_MAIL	1	/* mail back to sender */
# define ESM_QUIET	2	/* messages have already been returned */
# define ESM_DEADLETTER	3	/* save in ~/dead.letter */
# define ESM_POSTMASTER	4	/* return to postmaster */
# define ESM_USRTMP	5	/* save in /usr/tmp/dead.letter */
# define ESM_PANIC	6	/* leave the locked queue/transcript files */
# define ESM_DONE	7	/* the message is successfully delivered */

# ifndef _PATH_VARTMP
#  define _PATH_VARTMP	"/usr/tmp/"
# endif


savemail(e)
	register ENVELOPE *e;
{
	register struct passwd *pw;
	register FILE *fp;
	int state;
	auto ADDRESS *q = NULL;
	register char *p;
	MCI mcibuf;
	char buf[MAXLINE+1];
	extern struct passwd *getpwnam();
	extern char *ttypath();
	typedef int (*fnptr)();
	extern bool writable();

	if (tTd(6, 1))
	{
		printf("\nsavemail, errormode = %c, id = %s, ExitStat = %d\n  e_from=",
			e->e_errormode, e->e_id == NULL ? "NONE" : e->e_id,
			ExitStat);
		printaddr(&e->e_from, FALSE);
	}

	if (e->e_id == NULL)
	{
		/* can't return a message with no id */
		return;
	}

	/*
	**  In the unhappy event we don't know who to return the mail
	**  to, make someone up.
	*/

	if (e->e_from.q_paddr == NULL)
	{
		e->e_sender = "Postmaster";
		if (parseaddr(e->e_sender, &e->e_from,
			      RF_COPYPARSE|RF_SENDERADDR, '\0', NULL, e) == NULL)
		{
			syserr("553 Cannot parse Postmaster!");
			ExitStat = EX_SOFTWARE;
			finis();
		}
	}
	e->e_to = NULL;

	/*
	**  Basic state machine.
	**
	**	This machine runs through the following states:
	**
	**	ESM_QUIET	Errors have already been printed iff the
	**			sender is local.
	**	ESM_REPORT	Report directly to the sender's terminal.
	**	ESM_MAIL	Mail response to the sender.
	**	ESM_DEADLETTER	Save response in ~/dead.letter.
	**	ESM_POSTMASTER	Mail response to the postmaster.
	**	ESM_PANIC	Save response anywhere possible.
	*/

	/* determine starting state */
	switch (e->e_errormode)
	{
	  case EM_WRITE:
		state = ESM_REPORT;
		break;

	  case EM_BERKNET:
		/* mail back, but return o.k. exit status */
		ExitStat = EX_OK;

		/* fall through.... */

	  case EM_MAIL:
		state = ESM_MAIL;
		break;

	  case EM_PRINT:
	  case '\0':
		state = ESM_QUIET;
		break;

	  case EM_QUIET:
		/* no need to return anything at all */
		return;

	  default:
		syserr("554 savemail: bogus errormode x%x\n", e->e_errormode);
		state = ESM_MAIL;
		break;
	}

	/* if this is already an error response, send to postmaster */
	if (bitset(EF_RESPONSE, e->e_flags))
	{
		if (e->e_parent != NULL &&
		    bitset(EF_RESPONSE, e->e_parent->e_flags))
		{
			/* got an error sending a response -- can it */
			return;
		}
		state = ESM_POSTMASTER;
	}

	while (state != ESM_DONE)
	{
		if (tTd(6, 5))
			printf("  state %d\n", state);

		switch (state)
		{
		  case ESM_QUIET:
			if (e->e_from.q_mailer == LocalMailer)
				state = ESM_DEADLETTER;
			else
				state = ESM_MAIL;
			break;

		  case ESM_REPORT:

			/*
			**  If the user is still logged in on the same terminal,
			**  then write the error messages back to hir (sic).
			*/

			p = ttypath();
			if (p == NULL || freopen(p, "w", stdout) == NULL)
			{
				state = ESM_MAIL;
				break;
			}

			expand("\201n", buf, &buf[sizeof buf - 1], e);
			printf("\r\nMessage from %s...\r\n", buf);
			printf("Errors occurred while sending mail.\r\n");
			if (e->e_xfp != NULL)
			{
				(void) fflush(e->e_xfp);
				fp = fopen(queuename(e, 'x'), "r");
			}
			else
				fp = NULL;
			if (fp == NULL)
			{
				syserr("Cannot open %s", queuename(e, 'x'));
				printf("Transcript of session is unavailable.\r\n");
			}
			else
			{
				printf("Transcript follows:\r\n");
				while (fgets(buf, sizeof buf, fp) != NULL &&
				       !ferror(stdout))
					fputs(buf, stdout);
				(void) xfclose(fp, "savemail transcript", e->e_id);
			}
			printf("Original message will be saved in dead.letter.\r\n");
			state = ESM_DEADLETTER;
			break;

		  case ESM_MAIL:
			/*
			**  If mailing back, do it.
			**	Throw away all further output.  Don't alias,
			**	since this could cause loops, e.g., if joe
			**	mails to joe@x, and for some reason the network
			**	for @x is down, then the response gets sent to
			**	joe@x, which gives a response, etc.  Also force
			**	the mail to be delivered even if a version of
			**	it has already been sent to the sender.
			**
			**  If this is a configuration or local software
			**	error, send to the local postmaster as well,
			**	since the originator can't do anything
			**	about it anyway.  Note that this is a full
			**	copy of the message (intentionally) so that
			**	the Postmaster can forward things along.
			*/

			if (ExitStat == EX_CONFIG || ExitStat == EX_SOFTWARE)
			{
				(void) sendtolist("postmaster",
					  NULLADDR, &e->e_errorqueue, e);
			}
			if (strcmp(e->e_from.q_paddr, "<>") != 0)
			{
				(void) sendtolist(e->e_from.q_paddr,
					  NULLADDR, &e->e_errorqueue, e);
			}

			/*
			**  Deliver a non-delivery report to the
			**  Postmaster-designate (not necessarily
			**  Postmaster).  This does not include the
			**  body of the message, for privacy reasons.
			**  You really shouldn't need this.
			*/

			e->e_flags |= EF_PM_NOTIFY;

			/* check to see if there are any good addresses */
			for (q = e->e_errorqueue; q != NULL; q = q->q_next)
				if (!bitset(QBADADDR|QDONTSEND, q->q_flags))
					break;
			if (q == NULL)
			{
				/* this is an error-error */
				state = ESM_POSTMASTER;
				break;
			}
			if (returntosender(e->e_message, e->e_errorqueue,
					   (e->e_class >= 0), e) == 0)
			{
				state = ESM_DONE;
				break;
			}

			/* didn't work -- return to postmaster */
			state = ESM_POSTMASTER;
			break;

		  case ESM_POSTMASTER:
			/*
			**  Similar to previous case, but to system postmaster.
			*/

			q = NULL;
			if (sendtolist("postmaster", NULL, &q, e) <= 0)
			{
				syserr("553 cannot parse postmaster!");
				ExitStat = EX_SOFTWARE;
				state = ESM_USRTMP;
				break;
			}
			if (returntosender(e->e_message,
					   q, (e->e_class >= 0), e) == 0)
			{
				state = ESM_DONE;
				break;
			}

			/* didn't work -- last resort */
			state = ESM_USRTMP;
			break;

		  case ESM_DEADLETTER:
			/*
			**  Save the message in dead.letter.
			**	If we weren't mailing back, and the user is
			**	local, we should save the message in
			**	~/dead.letter so that the poor person doesn't
			**	have to type it over again -- and we all know
			**	what poor typists UNIX users are.
			*/

			p = NULL;
			if (e->e_from.q_mailer == LocalMailer)
			{
				if (e->e_from.q_home != NULL)
					p = e->e_from.q_home;
				else if ((pw = getpwnam(e->e_from.q_user)) != NULL)
					p = pw->pw_dir;
			}
			if (p == NULL)
			{
				/* no local directory */
				state = ESM_MAIL;
				break;
			}
			if (e->e_dfp != NULL)
			{
				bool oldverb = Verbose;

				/* we have a home directory; open dead.letter */
				define('z', p, e);
				expand("\201z/dead.letter", buf, &buf[sizeof buf - 1], e);
				Verbose = TRUE;
				message("Saving message in %s", buf);
				Verbose = oldverb;
				e->e_to = buf;
				q = NULL;
				(void) sendtolist(buf, &e->e_from, &q, e);
				if (q != NULL &&
				    !bitset(QBADADDR, q->q_flags) &&
				    deliver(e, q) == 0)
					state = ESM_DONE;
				else
					state = ESM_MAIL;
			}
			else
			{
				/* no data file -- try mailing back */
				state = ESM_MAIL;
			}
			break;

		  case ESM_USRTMP:
			/*
			**  Log the mail in /usr/tmp/dead.letter.
			*/

			if (e->e_class < 0)
			{
				state = ESM_DONE;
				break;
			}

			strcpy(buf, _PATH_VARTMP);
			strcat(buf, "dead.letter");
			if (!writable(buf, NULLADDR, SFF_NOSLINK))
			{
				state = ESM_PANIC;
				break;
			}
			fp = dfopen(buf, O_WRONLY|O_CREAT|O_APPEND, FileMode);
			if (fp == NULL)
			{
				state = ESM_PANIC;
				break;
			}

			bzero(&mcibuf, sizeof mcibuf);
			mcibuf.mci_out = fp;
			mcibuf.mci_mailer = FileMailer;
			if (bitnset(M_7BITS, FileMailer->m_flags))
				mcibuf.mci_flags |= MCIF_7BIT;

			putfromline(&mcibuf, e);
			(*e->e_puthdr)(&mcibuf, e);
			putline("\n", &mcibuf);
			(*e->e_putbody)(&mcibuf, e, NULL);
			putline("\n", &mcibuf);
			(void) fflush(fp);
			state = ferror(fp) ? ESM_PANIC : ESM_DONE;
			(void) xfclose(fp, "savemail", "/usr/tmp/dead.letter");
			break;

		  default:
			syserr("554 savemail: unknown state %d", state);

			/* fall through ... */

		  case ESM_PANIC:
			/* leave the locked queue & transcript files around */
			syserr("!554 savemail: cannot save rejected email anywhere");
		}
	}
}
/*
**  RETURNTOSENDER -- return a message to the sender with an error.
**
**	Parameters:
**		msg -- the explanatory message.
**		returnq -- the queue of people to send the message to.
**		sendbody -- if TRUE, also send back the body of the
**			message; otherwise just send the header.
**		e -- the current envelope.
**
**	Returns:
**		zero -- if everything went ok.
**		else -- some error.
**
**	Side Effects:
**		Returns the current message to the sender via
**		mail.
*/

static bool	SendBody;

#define MAXRETURNS	6	/* max depth of returning messages */
#define ERRORFUDGE	100	/* nominal size of error message text */

returntosender(msg, returnq, sendbody, e)
	char *msg;
	ADDRESS *returnq;
	bool sendbody;
	register ENVELOPE *e;
{
	char buf[MAXNAME];
	extern putheader(), errbody();
	register ENVELOPE *ee;
	ENVELOPE *oldcur = CurEnv;
	ENVELOPE errenvelope;
	static int returndepth;
	register ADDRESS *q;

	if (returnq == NULL)
		return (-1);

	if (msg == NULL)
		msg = "Unable to deliver mail";

	if (tTd(6, 1))
	{
		printf("Return To Sender: msg=\"%s\", depth=%d, e=%x, returnq=",
		       msg, returndepth, e);
		printaddr(returnq, TRUE);
	}

	if (++returndepth >= MAXRETURNS)
	{
		if (returndepth != MAXRETURNS)
			syserr("554 returntosender: infinite recursion on %s", returnq->q_paddr);
		/* don't "unrecurse" and fake a clean exit */
		/* returndepth--; */
		return (0);
	}

	SendBody = sendbody;
	define('g', e->e_from.q_paddr, e);
	define('u', NULL, e);
	ee = newenvelope(&errenvelope, e);
	define('a', "\201b", ee);
	define('r', "internal", ee);
	define('s', "localhost", ee);
	define('_', "localhost", ee);
	ee->e_puthdr = putheader;
	ee->e_putbody = errbody;
	ee->e_flags |= EF_RESPONSE|EF_METOO;
	if (!bitset(EF_OLDSTYLE, e->e_flags))
		ee->e_flags &= ~EF_OLDSTYLE;
	ee->e_sendqueue = returnq;
	ee->e_msgsize = ERRORFUDGE;
	if (!NoReturn)
		ee->e_msgsize += e->e_msgsize;
	initsys(ee);
	for (q = returnq; q != NULL; q = q->q_next)
	{
		if (bitset(QBADADDR, q->q_flags))
			continue;

		if (!bitset(QDONTSEND, q->q_flags))
			ee->e_nrcpts++;

		if (!DontPruneRoutes && pruneroute(q->q_paddr))
			parseaddr(q->q_paddr, q, RF_COPYPARSE, '\0', NULL, e);

		if (q->q_alias == NULL)
			addheader("To", q->q_paddr, ee);
	}

# ifdef LOG
	if (LogLevel > 5)
		syslog(LOG_INFO, "%s: %s: return to sender: %s",
			e->e_id, ee->e_id, msg);
# endif

	(void) sprintf(buf, "Returned mail: %s", msg);
	addheader("Subject", buf, ee);
	if (SendMIMEErrors)
	{
		addheader("MIME-Version", "1.0", ee);
		(void) sprintf(buf, "%s.%ld/%s",
			ee->e_id, curtime(), MyHostName);
		ee->e_msgboundary = newstr(buf);
		(void) sprintf(buf, "multipart/mixed; boundary=\"%s\"",
					ee->e_msgboundary);
		addheader("Content-Type", buf, ee);
	}

	/* fake up an address header for the from person */
	expand("\201n", buf, &buf[sizeof buf - 1], e);
	if (parseaddr(buf, &ee->e_from, RF_COPYALL|RF_SENDERADDR, '\0', NULL, e) == NULL)
	{
		syserr("553 Can't parse myself!");
		ExitStat = EX_SOFTWARE;
		returndepth--;
		return (-1);
	}
	ee->e_sender = ee->e_from.q_paddr;

	/* push state into submessage */
	CurEnv = ee;
	define('f', "\201n", ee);
	define('x', "Mail Delivery Subsystem", ee);
	eatheader(ee, TRUE);

	/* mark statistics */
	markstats(ee, NULLADDR);

	/* actually deliver the error message */
	sendall(ee, SM_DEFAULT);

	/* restore state */
	dropenvelope(ee);
	CurEnv = oldcur;
	returndepth--;

	/* should check for delivery errors here */
	return (0);
}
/*
**  ERRBODY -- output the body of an error message.
**
**	Typically this is a copy of the transcript plus a copy of the
**	original offending message.
**
**	Parameters:
**		mci -- the mailer connection information.
**		e -- the envelope we are working in.
**
**	Returns:
**		none
**
**	Side Effects:
**		Outputs the body of an error message.
*/

errbody(mci, e)
	register MCI *mci;
	register ENVELOPE *e;
{
	register FILE *xfile;
	char *p;
	register ADDRESS *q;
	bool printheader;
	char buf[MAXLINE];

	if (e->e_parent == NULL)
	{
		syserr("errbody: null parent");
		putline("   ----- Original message lost -----\n", mci);
		return;
	}

	/*
	**  Output MIME header.
	*/

	if (e->e_msgboundary != NULL)
	{
		putline("This is a MIME-encapsulated message", mci);
		putline("", mci);
		(void) sprintf(buf, "--%s", e->e_msgboundary);
		putline(buf, mci);
		putline("", mci);
	}

	/*
	**  Output introductory information.
	*/

	for (q = e->e_parent->e_sendqueue; q != NULL; q = q->q_next)
		if (bitset(QBADADDR, q->q_flags))
			break;
	if (q == NULL &&
	    !bitset(EF_FATALERRS|EF_SENDRECEIPT, e->e_parent->e_flags))
	{
		putline("    **********************************************",
			mci);
		putline("    **      THIS IS A WARNING MESSAGE ONLY      **",
			mci);
		putline("    **  YOU DO NOT NEED TO RESEND YOUR MESSAGE  **",
			mci);
		putline("    **********************************************",
			mci);
		putline("", mci);
	}
	sprintf(buf, "The original message was received at %s",
		arpadate(ctime(&e->e_parent->e_ctime)));
	putline(buf, mci);
	expand("from \201_", buf, &buf[sizeof buf - 1], e->e_parent);
	putline(buf, mci);
	putline("", mci);

	/*
	**  Output error message header (if specified and available).
	*/

	if (ErrMsgFile != NULL)
	{
		if (*ErrMsgFile == '/')
		{
			xfile = fopen(ErrMsgFile, "r");
			if (xfile != NULL)
			{
				while (fgets(buf, sizeof buf, xfile) != NULL)
				{
					expand(buf, buf, &buf[sizeof buf - 1], e);
					putline(buf, mci);
				}
				(void) fclose(xfile);
				putline("\n", mci);
			}
		}
		else
		{
			expand(ErrMsgFile, buf, &buf[sizeof buf - 1], e);
			putline(buf, mci);
			putline("", mci);
		}
	}

	/*
	**  Output message introduction
	*/

	printheader = TRUE;
	for (q = e->e_parent->e_sendqueue; q != NULL; q = q->q_next)
	{
		if (bitset(QBADADDR|QREPORT, q->q_flags))
		{
			if (printheader)
			{
				putline("   ----- The following addresses had delivery problems -----",
					mci);
				printheader = FALSE;
			}
			strcpy(buf, q->q_paddr);
			if (bitset(QBADADDR, q->q_flags))
				strcat(buf, "  (unrecoverable error)");
			else
				strcat(buf, "  (transient failure)");
			putline(buf, mci);
			if (q->q_alias != NULL)
			{
				strcpy(buf, "    (expanded from: ");
				strcat(buf, q->q_alias->q_paddr);
				strcat(buf, ")");
				putline(buf, mci);
			}
		}
	}
	if (!printheader)
		putline("\n", mci);

	/*
	**  Output transcript of errors
	*/

	(void) fflush(stdout);
	p = queuename(e->e_parent, 'x');
	if ((xfile = fopen(p, "r")) == NULL)
	{
		syserr("Cannot open %s", p);
		putline("   ----- Transcript of session is unavailable -----\n", mci);
	}
	else
	{
		putline("   ----- Transcript of session follows -----\n", mci);
		if (e->e_xfp != NULL)
			(void) fflush(e->e_xfp);
		while (fgets(buf, sizeof buf, xfile) != NULL)
			putline(buf, mci);
		(void) xfclose(xfile, "errbody xscript", p);
	}
	errno = 0;

	/*
	**  Output text of original message
	*/

	if (NoReturn)
		SendBody = FALSE;
	putline("", mci);
	if (e->e_parent->e_df != NULL)
	{
		if (SendBody)
			putline("   ----- Original message follows -----\n", mci);
		else
			putline("   ----- Message header follows -----\n", mci);
		(void) fflush(mci->mci_out);

		if (e->e_msgboundary != NULL)
		{
			putline("", mci);
			(void) sprintf(buf, "--%s", e->e_msgboundary);
			putline(buf, mci);
			putline("Content-Type: message/rfc822", mci);
			putline("", mci);
		}
		putheader(mci, e->e_parent);
		putline("", mci);
		if (SendBody)
			putbody(mci, e->e_parent, e->e_msgboundary);
		else
			putline("   ----- Message body suppressed -----", mci);
	}
	else
	{
		putline("  ----- No message was collected -----\n", mci);
	}

	if (e->e_msgboundary != NULL)
	{
		putline("", mci);
		(void) sprintf(buf, "--%s--", e->e_msgboundary);
		putline(buf, mci);
	}
	putline("", mci);

	/*
	**  Cleanup and exit
	*/

	if (errno != 0)
		syserr("errbody: I/O error");
}
/*
**  PRUNEROUTE -- prune an RFC-822 source route
** 
**	Trims down a source route to the last internet-registered hop.
**	This is encouraged by RFC 1123 section 5.3.3.
** 
**	Parameters:
**		addr -- the address
** 
**	Returns:
**		TRUE -- address was modified
**		FALSE -- address could not be pruned
** 
**	Side Effects:
**		modifies addr in-place
*/

pruneroute(addr)
	char *addr;
{
#if NAMED_BIND
	char *start, *at, *comma;
	char c;
	int rcode;
	char hostbuf[BUFSIZ];
	char *mxhosts[MAXMXHOSTS + 1];

	/* check to see if this is really a route-addr */
	if (*addr != '<' || addr[1] != '@' || addr[strlen(addr) - 1] != '>')
		return FALSE;
	start = strchr(addr, ':');
	at = strrchr(addr, '@');
	if (start == NULL || at == NULL || at < start)
		return FALSE;

	/* slice off the angle brackets */
	strcpy(hostbuf, at + 1);
	hostbuf[strlen(hostbuf) - 1] = '\0';

	while (start)
	{
		if (getmxrr(hostbuf, mxhosts, FALSE, &rcode) > 0)
		{
			strcpy(addr + 1, start + 1);
			return TRUE;
		}
		c = *start;
		*start = '\0';
		comma = strrchr(addr, ',');
		if (comma && comma[1] == '@')
			strcpy(hostbuf, comma + 2);
		else
			comma = 0;
		*start = c;
		start = comma;
	}
#endif
	return FALSE;
}
