/*
 * Copyright (c) 1983, 1995, 1996 Eric P. Allman
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

# include "sendmail.h"

#ifndef lint
#if SMTP
static char sccsid[] = "@(#)usersmtp.c	8.79 (Berkeley) 12/1/96 (with SMTP)";
#else
static char sccsid[] = "@(#)usersmtp.c	8.79 (Berkeley) 12/1/96 (without SMTP)";
#endif
#endif /* not lint */

# include <sysexits.h>
# include <errno.h>

# if SMTP

/*
**  USERSMTP -- run SMTP protocol from the user end.
**
**	This protocol is described in RFC821.
*/

#define REPLYTYPE(r)	((r) / 100)		/* first digit of reply code */
#define REPLYCLASS(r)	(((r) / 10) % 10)	/* second digit of reply code */
#define SMTPCLOSING	421			/* "Service Shutting Down" */

char	SmtpMsgBuffer[MAXLINE];		/* buffer for commands */
char	SmtpReplyBuffer[MAXLINE];	/* buffer for replies */
char	SmtpError[MAXLINE] = "";	/* save failure error messages */
int	SmtpPid;			/* pid of mailer */
bool	SmtpNeedIntro;			/* need "while talking" in transcript */

extern void	smtpmessage __P((char *f, MAILER *m, MCI *mci, ...));
extern int	reply __P((MAILER *, MCI *, ENVELOPE *, time_t, void (*)()));
/*
**  SMTPINIT -- initialize SMTP.
**
**	Opens the connection and sends the initial protocol.
**
**	Parameters:
**		m -- mailer to create connection to.
**		pvp -- pointer to parameter vector to pass to
**			the mailer.
**
**	Returns:
**		none.
**
**	Side Effects:
**		creates connection and sends initial protocol.
*/

void
smtpinit(m, mci, e)
	MAILER *m;
	register MCI *mci;
	ENVELOPE *e;
{
	register int r;
	register char *p;
	extern void esmtp_check();
	extern void helo_options();

	if (tTd(18, 1))
	{
		printf("smtpinit ");
		mci_dump(mci, FALSE);
	}

	/*
	**  Open the connection to the mailer.
	*/

	SmtpError[0] = '\0';
	CurHostName = mci->mci_host;		/* XXX UGLY XXX */
	if (CurHostName == NULL)
		CurHostName = MyHostName;
	SmtpNeedIntro = TRUE;
	switch (mci->mci_state)
	{
	  case MCIS_ACTIVE:
		/* need to clear old information */
		smtprset(m, mci, e);
		/* fall through */

	  case MCIS_OPEN:
		return;

	  case MCIS_ERROR:
	  case MCIS_SSD:
		/* shouldn't happen */
		smtpquit(m, mci, e);
		/* fall through */

	  case MCIS_CLOSED:
		syserr("451 smtpinit: state CLOSED");
		return;

	  case MCIS_OPENING:
		break;
	}

	mci->mci_state = MCIS_OPENING;

	/*
	**  Get the greeting message.
	**	This should appear spontaneously.  Give it five minutes to
	**	happen.
	*/

	SmtpPhase = mci->mci_phase = "client greeting";
	setproctitle("%s %s: %s", e->e_id, CurHostName, mci->mci_phase);
	r = reply(m, mci, e, TimeOuts.to_initial, esmtp_check);
	if (r < 0)
		goto tempfail1;
	if (REPLYTYPE(r) == 4)
		goto tempfail2;
	if (REPLYTYPE(r) != 2)
		goto unavailable;

	/*
	**  Send the HELO command.
	**	My mother taught me to always introduce myself.
	*/

#if _FFR_LMTP
	if (bitnset(M_ESMTP, m->m_flags) || bitnset(M_LMTP, m->m_flags))
#else
	if (bitnset(M_ESMTP, m->m_flags))
#endif
		mci->mci_flags |= MCIF_ESMTP;

tryhelo:
#if _FFR_LMTP
	if (bitnset(M_LMTP, m->m_flags))
	{
		smtpmessage("LHLO %s", m, mci, MyHostName);
		SmtpPhase = mci->mci_phase = "client LHLO";
	}
	else if (bitset(MCIF_ESMTP, mci->mci_flags))
#else
	if (bitset(MCIF_ESMTP, mci->mci_flags))
#endif
	{
		smtpmessage("EHLO %s", m, mci, MyHostName);
		SmtpPhase = mci->mci_phase = "client EHLO";
	}
	else
	{
		smtpmessage("HELO %s", m, mci, MyHostName);
		SmtpPhase = mci->mci_phase = "client HELO";
	}
	setproctitle("%s %s: %s", e->e_id, CurHostName, mci->mci_phase);
	r = reply(m, mci, e, TimeOuts.to_helo, helo_options);
	if (r < 0)
		goto tempfail1;
	else if (REPLYTYPE(r) == 5)
	{
#if _FFR_LMTP
		if (bitset(MCIF_ESMTP, mci->mci_flags) &&
		    !bitnset(M_LMTP, m->m_flags))
#else
		if (bitset(MCIF_ESMTP, mci->mci_flags))
#endif
		{
			/* try old SMTP instead */
			mci->mci_flags &= ~MCIF_ESMTP;
			goto tryhelo;
		}
		goto unavailable;
	}
	else if (REPLYTYPE(r) != 2)
		goto tempfail2;

	/*
	**  Check to see if we actually ended up talking to ourself.
	**  This means we didn't know about an alias or MX, or we managed
	**  to connect to an echo server.
	*/

	p = strchr(&SmtpReplyBuffer[4], ' ');
	if (p != NULL)
		*p = '\0';
	if (!bitnset(M_NOLOOPCHECK, m->m_flags) &&
#if _FFR_LMTP
	    !bitnset(M_LMTP, m->m_flags) &&
#endif
	    strcasecmp(&SmtpReplyBuffer[4], MyHostName) == 0)
	{
		syserr("553 %s config error: mail loops back to me (MX problem?)",
			mci->mci_host);
		mci_setstat(mci, EX_CONFIG, NULL, NULL);
		mci->mci_errno = 0;
		smtpquit(m, mci, e);
		return;
	}

	/*
	**  If this is expected to be another sendmail, send some internal
	**  commands.
	*/

	if (bitnset(M_INTERNAL, m->m_flags))
	{
		/* tell it to be verbose */
		smtpmessage("VERB", m, mci);
		r = reply(m, mci, e, TimeOuts.to_miscshort, NULL);
		if (r < 0)
			goto tempfail1;
	}

	if (mci->mci_state != MCIS_CLOSED)
	{
		mci->mci_state = MCIS_OPEN;
		return;
	}

	/* got a 421 error code during startup */

  tempfail1:
	if (mci->mci_errno == 0)
		mci->mci_errno = errno;
	mci_setstat(mci, EX_TEMPFAIL, "4.4.2", NULL);
	if (mci->mci_state != MCIS_CLOSED)
		smtpquit(m, mci, e);
	return;

  tempfail2:
	if (mci->mci_errno == 0)
		mci->mci_errno = errno;
	/* XXX should use code from other end iff ENHANCEDSTATUSCODES */
	mci_setstat(mci, EX_TEMPFAIL, "4.5.0", SmtpReplyBuffer);
	if (mci->mci_state != MCIS_CLOSED)
		smtpquit(m, mci, e);
	return;

  unavailable:
	mci->mci_errno = errno;
	mci_setstat(mci, EX_UNAVAILABLE, "5.5.0", SmtpReplyBuffer);
	smtpquit(m, mci, e);
	return;
}
/*
**  ESMTP_CHECK -- check to see if this implementation likes ESMTP protocol
**
**	Parameters:
**		line -- the response line.
**		firstline -- set if this is the first line of the reply.
**		m -- the mailer.
**		mci -- the mailer connection info.
**		e -- the envelope.
**
**	Returns:
**		none.
*/

void
esmtp_check(line, firstline, m, mci, e)
	char *line;
	bool firstline;
	MAILER *m;
	register MCI *mci;
	ENVELOPE *e;
{
	if (strstr(line, "ESMTP") != NULL)
		mci->mci_flags |= MCIF_ESMTP;
	if (strstr(line, "8BIT-OK") != NULL)
		mci->mci_flags |= MCIF_8BITOK;
}
/*
**  HELO_OPTIONS -- process the options on a HELO line.
**
**	Parameters:
**		line -- the response line.
**		firstline -- set if this is the first line of the reply.
**		m -- the mailer.
**		mci -- the mailer connection info.
**		e -- the envelope.
**
**	Returns:
**		none.
*/

void
helo_options(line, firstline, m, mci, e)
	char *line;
	bool firstline;
	MAILER *m;
	register MCI *mci;
	ENVELOPE *e;
{
	register char *p;

	if (firstline)
		return;

	if (strlen(line) < (SIZE_T) 5)
		return;
	line += 4;
	p = strchr(line, ' ');
	if (p != NULL)
		*p++ = '\0';
	if (strcasecmp(line, "size") == 0)
	{
		mci->mci_flags |= MCIF_SIZE;
		if (p != NULL)
			mci->mci_maxsize = atol(p);
	}
	else if (strcasecmp(line, "8bitmime") == 0)
	{
		mci->mci_flags |= MCIF_8BITMIME;
		mci->mci_flags &= ~MCIF_7BIT;
	}
	else if (strcasecmp(line, "expn") == 0)
		mci->mci_flags |= MCIF_EXPN;
	else if (strcasecmp(line, "dsn") == 0)
		mci->mci_flags |= MCIF_DSN;
}
/*
**  SMTPMAILFROM -- send MAIL command
**
**	Parameters:
**		m -- the mailer.
**		mci -- the mailer connection structure.
**		e -- the envelope (including the sender to specify).
*/

int
smtpmailfrom(m, mci, e)
	MAILER *m;
	MCI *mci;
	ENVELOPE *e;
{
	int r;
	int l;
	char *bufp;
	char *bodytype;
	char buf[MAXNAME + 1];
	char optbuf[MAXLINE];

	if (tTd(18, 2))
		printf("smtpmailfrom: CurHost=%s\n", CurHostName);

	/* set up appropriate options to include */
	if (bitset(MCIF_SIZE, mci->mci_flags) && e->e_msgsize > 0)
		snprintf(optbuf, sizeof optbuf, " SIZE=%ld", e->e_msgsize);
	else
		strcpy(optbuf, "");
	l = sizeof optbuf - strlen(optbuf) - 1;

	bodytype = e->e_bodytype;
	if (bitset(MCIF_8BITMIME, mci->mci_flags))
	{
		if (bodytype == NULL &&
		    bitset(MM_MIME8BIT, MimeMode) &&
		    bitset(EF_HAS8BIT, e->e_flags) &&
		    !bitset(EF_DONT_MIME, e->e_flags) &&
		    !bitnset(M_8BITS, m->m_flags))
			bodytype = "8BITMIME";
		if (bodytype != NULL)
		{
			strcat(optbuf, " BODY=");
			strcat(optbuf, bodytype);
			l -= strlen(optbuf);
		}
	}
	else if (bitnset(M_8BITS, m->m_flags) ||
		 !bitset(EF_HAS8BIT, e->e_flags) ||
		 bitset(MCIF_8BITOK, mci->mci_flags))
	{
		/* just pass it through */
	}
#if MIME8TO7
	else if (bitset(MM_CVTMIME, MimeMode) &&
		 !bitset(EF_DONT_MIME, e->e_flags) &&
		 (!bitset(MM_PASS8BIT, MimeMode) ||
		  bitset(EF_IS_MIME, e->e_flags)))
	{
		/* must convert from 8bit MIME format to 7bit encoded */
		mci->mci_flags |= MCIF_CVT8TO7;
	}
#endif
	else if (!bitset(MM_PASS8BIT, MimeMode))
	{
		/* cannot just send a 8-bit version */
		extern char MsgBuf[];

		usrerr("%s does not support 8BITMIME", mci->mci_host);
		mci_setstat(mci, EX_NOTSTICKY, "5.6.3", MsgBuf);
		return EX_DATAERR;
	}

	if (bitset(MCIF_DSN, mci->mci_flags))
	{
		if (e->e_envid != NULL && strlen(e->e_envid) < (SIZE_T) (l - 7))
		{
			strcat(optbuf, " ENVID=");
			strcat(optbuf, e->e_envid);
			l -= strlen(optbuf);
		}

		/* RET= parameter */
		if (bitset(EF_RET_PARAM, e->e_flags) && l >= 9)
		{
			strcat(optbuf, " RET=");
			if (bitset(EF_NO_BODY_RETN, e->e_flags))
				strcat(optbuf, "HDRS");
			else
				strcat(optbuf, "FULL");
			l -= 9;
		}
	}

	/*
	**  Send the MAIL command.
	**	Designates the sender.
	*/

	mci->mci_state = MCIS_ACTIVE;

	if (bitset(EF_RESPONSE, e->e_flags) &&
	    !bitnset(M_NO_NULL_FROM, m->m_flags))
		(void) strcpy(buf, "");
	else
		expand("\201g", buf, sizeof buf, e);
	if (buf[0] == '<')
	{
		/* strip off <angle brackets> (put back on below) */
		bufp = &buf[strlen(buf) - 1];
		if (*bufp == '>')
			*bufp = '\0';
		bufp = &buf[1];
	}
	else
		bufp = buf;
	if (bitnset(M_LOCALMAILER, e->e_from.q_mailer->m_flags) ||
	    !bitnset(M_FROMPATH, m->m_flags))
	{
		smtpmessage("MAIL From:<%s>%s", m, mci, bufp, optbuf);
	}
	else
	{
		smtpmessage("MAIL From:<@%s%c%s>%s", m, mci, MyHostName,
			*bufp == '@' ? ',' : ':', bufp, optbuf);
	}
	SmtpPhase = mci->mci_phase = "client MAIL";
	setproctitle("%s %s: %s", e->e_id, CurHostName, mci->mci_phase);
	r = reply(m, mci, e, TimeOuts.to_mail, NULL);
	if (r < 0)
	{
		/* communications failure */
		mci->mci_errno = errno;
		mci_setstat(mci, EX_TEMPFAIL, "4.4.2", NULL);
		smtpquit(m, mci, e);
		return EX_TEMPFAIL;
	}
	else if (r == 421)
	{
		/* service shutting down */
		mci_setstat(mci, EX_TEMPFAIL, "4.5.0", SmtpReplyBuffer);
		smtpquit(m, mci, e);
		return EX_TEMPFAIL;
	}
	else if (REPLYTYPE(r) == 4)
	{
		mci_setstat(mci, EX_TEMPFAIL, smtptodsn(r), SmtpReplyBuffer);
		return EX_TEMPFAIL;
	}
	else if (REPLYTYPE(r) == 2)
	{
		return EX_OK;
	}
	else if (r == 501)
	{
		/* syntax error in arguments */
		mci_setstat(mci, EX_NOTSTICKY, "5.5.2", SmtpReplyBuffer);
		return EX_DATAERR;
	}
	else if (r == 553)
	{
		/* mailbox name not allowed */
		mci_setstat(mci, EX_NOTSTICKY, "5.1.3", SmtpReplyBuffer);
		return EX_DATAERR;
	}
	else if (r == 552)
	{
		/* exceeded storage allocation */
		mci_setstat(mci, EX_NOTSTICKY, "5.2.2", SmtpReplyBuffer);
		return EX_UNAVAILABLE;
	}
	else if (REPLYTYPE(r) == 5)
	{
		/* unknown error */
		mci_setstat(mci, EX_NOTSTICKY, "5.0.0", SmtpReplyBuffer);
		return EX_UNAVAILABLE;
	}

#ifdef LOG
	if (LogLevel > 1)
	{
		syslog(LOG_CRIT, "%s: %.100s: SMTP MAIL protocol error: %s",
			e->e_id, mci->mci_host,
			shortenstring(SmtpReplyBuffer, 403));
	}
#endif

	/* protocol error -- close up */
	mci_setstat(mci, EX_PROTOCOL, "5.5.1", SmtpReplyBuffer);
	smtpquit(m, mci, e);
	return EX_PROTOCOL;
}
/*
**  SMTPRCPT -- designate recipient.
**
**	Parameters:
**		to -- address of recipient.
**		m -- the mailer we are sending to.
**		mci -- the connection info for this transaction.
**		e -- the envelope for this transaction.
**
**	Returns:
**		exit status corresponding to recipient status.
**
**	Side Effects:
**		Sends the mail via SMTP.
*/

int
smtprcpt(to, m, mci, e)
	ADDRESS *to;
	register MAILER *m;
	MCI *mci;
	ENVELOPE *e;
{
	register int r;
	int l;
	char optbuf[MAXLINE];

	strcpy(optbuf, "");
	l = sizeof optbuf - 1;
	if (bitset(MCIF_DSN, mci->mci_flags))
	{
		/* NOTIFY= parameter */
		if (bitset(QHASNOTIFY, to->q_flags) &&
		    bitset(QPRIMARY, to->q_flags) &&
		    !bitnset(M_LOCALMAILER, m->m_flags))
		{
			bool firstone = TRUE;

			strcat(optbuf, " NOTIFY=");
			if (bitset(QPINGONSUCCESS, to->q_flags))
			{
				strcat(optbuf, "SUCCESS");
				firstone = FALSE;
			}
			if (bitset(QPINGONFAILURE, to->q_flags))
			{
				if (!firstone)
					strcat(optbuf, ",");
				strcat(optbuf, "FAILURE");
				firstone = FALSE;
			}
			if (bitset(QPINGONDELAY, to->q_flags))
			{
				if (!firstone)
					strcat(optbuf, ",");
				strcat(optbuf, "DELAY");
				firstone = FALSE;
			}
			if (firstone)
				strcat(optbuf, "NEVER");
			l -= strlen(optbuf);
		}

		/* ORCPT= parameter */
		if (to->q_orcpt != NULL && strlen(to->q_orcpt) + 7 < l)
		{
			strcat(optbuf, " ORCPT=");
			strcat(optbuf, to->q_orcpt);
			l -= strlen(optbuf);
		}
	}

	smtpmessage("RCPT To:<%s>%s", m, mci, to->q_user, optbuf);

	SmtpPhase = mci->mci_phase = "client RCPT";
	setproctitle("%s %s: %s", e->e_id, CurHostName, mci->mci_phase);
	r = reply(m, mci, e, TimeOuts.to_rcpt, NULL);
	to->q_rstatus = newstr(SmtpReplyBuffer);
	to->q_status = smtptodsn(r);
	to->q_statmta = mci->mci_host;
	if (r < 0 || REPLYTYPE(r) == 4)
		return EX_TEMPFAIL;
	else if (REPLYTYPE(r) == 2)
		return EX_OK;
	else if (r == 550)
	{
		to->q_status = "5.1.1";
		return EX_NOUSER;
	}
	else if (r == 551)
	{
		to->q_status = "5.1.6";
		return EX_NOUSER;
	}
	else if (r == 553)
	{
		to->q_status = "5.1.3";
		return EX_NOUSER;
	}
	else if (REPLYTYPE(r) == 5)
	{
		return EX_UNAVAILABLE;
	}

#ifdef LOG
	if (LogLevel > 1)
	{
		syslog(LOG_CRIT, "%s: %.100s: SMTP RCPT protocol error: %s",
			e->e_id, mci->mci_host,
			shortenstring(SmtpReplyBuffer, 403));
	}
#endif

	mci_setstat(mci, EX_PROTOCOL, "5.5.1", SmtpReplyBuffer);
	return EX_PROTOCOL;
}
/*
**  SMTPDATA -- send the data and clean up the transaction.
**
**	Parameters:
**		m -- mailer being sent to.
**		mci -- the mailer connection information.
**		e -- the envelope for this message.
**
**	Returns:
**		exit status corresponding to DATA command.
**
**	Side Effects:
**		none.
*/

static jmp_buf	CtxDataTimeout;
static void	datatimeout();

int
smtpdata(m, mci, e)
	MAILER *m;
	register MCI *mci;
	register ENVELOPE *e;
{
	register int r;
	register EVENT *ev;
	int rstat;
	time_t timeout;

	/*
	**  Send the data.
	**	First send the command and check that it is ok.
	**	Then send the data.
	**	Follow it up with a dot to terminate.
	**	Finally get the results of the transaction.
	*/

	/* send the command and check ok to proceed */
	smtpmessage("DATA", m, mci);
	SmtpPhase = mci->mci_phase = "client DATA 354";
	setproctitle("%s %s: %s", e->e_id, CurHostName, mci->mci_phase);
	r = reply(m, mci, e, TimeOuts.to_datainit, NULL);
	if (r < 0 || REPLYTYPE(r) == 4)
	{
		smtpquit(m, mci, e);
		return EX_TEMPFAIL;
	}
	else if (REPLYTYPE(r) == 5)
	{
		smtprset(m, mci, e);
		return EX_UNAVAILABLE;
	}
	else if (r != 354)
	{
#ifdef LOG
		if (LogLevel > 1)
		{
			syslog(LOG_CRIT, "%s: %.100s: SMTP DATA-1 protocol error: %s",
				e->e_id, mci->mci_host,
				shortenstring(SmtpReplyBuffer, 403));
		}
#endif
		smtprset(m, mci, e);
		mci_setstat(mci, EX_PROTOCOL, "5.5.1", SmtpReplyBuffer);
		return (EX_PROTOCOL);
	}

	/*
	**  Set timeout around data writes.  Make it at least large
	**  enough for DNS timeouts on all recipients plus some fudge
	**  factor.  The main thing is that it should not be infinite.
	*/

	if (setjmp(CtxDataTimeout) != 0)
	{
		mci->mci_errno = errno;
		mci->mci_state = MCIS_ERROR;
		mci_setstat(mci, EX_TEMPFAIL, "4.4.2", NULL);
		syserr("451 timeout writing message to %s", mci->mci_host);
		smtpquit(m, mci, e);
		return EX_TEMPFAIL;
	}

	timeout = e->e_msgsize / 16;
	if (timeout < (time_t) 600)
		timeout = (time_t) 600;
	timeout += e->e_nrcpts * 300;
	ev = setevent(timeout, datatimeout, 0);

	/*
	**  Output the actual message.
	*/

	(*e->e_puthdr)(mci, e->e_header, e);
	(*e->e_putbody)(mci, e, NULL);

	/*
	**  Cleanup after sending message.
	*/

	clrevent(ev);

	if (ferror(mci->mci_out))
	{
		/* error during processing -- don't send the dot */
		mci->mci_errno = EIO;
		mci->mci_state = MCIS_ERROR;
		mci_setstat(mci, EX_IOERR, "4.4.2", NULL);
		smtpquit(m, mci, e);
		return EX_IOERR;
	}

	/* terminate the message */
	fprintf(mci->mci_out, ".%s", m->m_eol);
	if (TrafficLogFile != NULL)
		fprintf(TrafficLogFile, "%05d >>> .\n", (int) getpid());
	if (Verbose)
		nmessage(">>> .");

	/* check for the results of the transaction */
	SmtpPhase = mci->mci_phase = "client DATA status";
	setproctitle("%s %s: %s", e->e_id, CurHostName, mci->mci_phase);
#if _FFR_LMTP
	if (bitnset(M_LMTP, m->m_flags))
		return EX_OK;
#endif
	r = reply(m, mci, e, TimeOuts.to_datafinal, NULL);
	if (r < 0)
	{
		smtpquit(m, mci, e);
		return EX_TEMPFAIL;
	}
	mci->mci_state = MCIS_OPEN;
	if (REPLYTYPE(r) == 4)
		rstat = EX_TEMPFAIL;
	else if (REPLYCLASS(r) != 5)
		rstat = EX_PROTOCOL;
	else if (REPLYTYPE(r) == 2)
		rstat = EX_OK;
	else if (REPLYTYPE(r) == 5)
		rstat = EX_UNAVAILABLE;
	else
		rstat = EX_PROTOCOL;
	mci_setstat(mci, rstat, smtptodsn(r), SmtpReplyBuffer);
	if (e->e_statmsg != NULL)
		free(e->e_statmsg);
	e->e_statmsg = newstr(&SmtpReplyBuffer[4]);
	if (rstat != EX_PROTOCOL)
		return rstat;
#ifdef LOG
	if (LogLevel > 1)
	{
		syslog(LOG_CRIT, "%s: %.100s: SMTP DATA-2 protocol error: %s",
			e->e_id, mci->mci_host,
			shortenstring(SmtpReplyBuffer, 403));
	}
#endif
	return rstat;
}


static void
datatimeout()
{
	longjmp(CtxDataTimeout, 1);
}
/*
**  SMTPGETSTAT -- get status code from DATA in LMTP
**
**	Parameters:
**		m -- the mailer to which we are sending the message.
**		mci -- the mailer connection structure.
**		e -- the current envelope.
**
**	Returns:
**		The exit status corresponding to the reply code.
*/

#if _FFR_LMTP

int
smtpgetstat(m, mci, e)
	MAILER *m;
	MCI *mci;
	ENVELOPE *e;
{
	int r;
	int stat;

	/* check for the results of the transaction */
	r = reply(m, mci, e, TimeOuts.to_datafinal, NULL);
	if (r < 0)
	{
		smtpquit(m, mci, e);
		return EX_TEMPFAIL;
	}
	if (e->e_statmsg != NULL)
		free(e->e_statmsg);
	e->e_statmsg = newstr(&SmtpReplyBuffer[4]);
	if (REPLYTYPE(r) == 4)
		stat = EX_TEMPFAIL;
	else if (REPLYCLASS(r) != 5)
		stat = EX_PROTOCOL;
	else if (REPLYTYPE(r) == 2)
		stat = EX_OK;
	else if (REPLYTYPE(r) == 5)
		stat = EX_UNAVAILABLE;
	mci_setstat(mci, stat, smtptodsn(r), SmtpReplyBuffer);
#ifdef LOG
	if (LogLevel > 1 && stat == EX_PROTOCOL)
	{
		syslog(LOG_CRIT, "%s: %.100s: SMTP DATA-3 protocol error: %s",
			e->e_id, mci->mci_host,
			shortenstring(SmtpReplyBuffer, 403));
	}
#endif
	return stat;
}

#endif
/*
**  SMTPQUIT -- close the SMTP connection.
**
**	Parameters:
**		m -- a pointer to the mailer.
**		mci -- the mailer connection information.
**		e -- the current envelope.
**
**	Returns:
**		none.
**
**	Side Effects:
**		sends the final protocol and closes the connection.
*/

void
smtpquit(m, mci, e)
	register MAILER *m;
	register MCI *mci;
	ENVELOPE *e;
{
	bool oldSuprErrs = SuprErrs;

	/*
	**	Suppress errors here -- we may be processing a different
	**	job when we do the quit connection, and we don't want the 
	**	new job to be penalized for something that isn't it's
	**	problem.
	*/

	SuprErrs = TRUE;

	/* send the quit message if we haven't gotten I/O error */
	if (mci->mci_state != MCIS_ERROR)
	{
		SmtpPhase = "client QUIT";
		smtpmessage("QUIT", m, mci);
		(void) reply(m, mci, e, TimeOuts.to_quit, NULL);
		SuprErrs = oldSuprErrs;
		if (mci->mci_state == MCIS_CLOSED)
		{
			SuprErrs = oldSuprErrs;
			return;
		}
	}

	/* now actually close the connection and pick up the zombie */
	(void) endmailer(mci, e, NULL);

	SuprErrs = oldSuprErrs;
}
/*
**  SMTPRSET -- send a RSET (reset) command
*/

void
smtprset(m, mci, e)
	register MAILER *m;
	register MCI *mci;
	ENVELOPE *e;
{
	int r;

	SmtpPhase = "client RSET";
	smtpmessage("RSET", m, mci);
	r = reply(m, mci, e, TimeOuts.to_rset, NULL);
	if (r < 0)
		mci->mci_state = MCIS_ERROR;
	else if (REPLYTYPE(r) == 2)
	{
		mci->mci_state = MCIS_OPEN;
		return;
	}
	smtpquit(m, mci, e);
}
/*
**  SMTPPROBE -- check the connection state
*/

int
smtpprobe(mci)
	register MCI *mci;
{
	int r;
	MAILER *m = mci->mci_mailer;
	extern ENVELOPE BlankEnvelope;
	ENVELOPE *e = &BlankEnvelope;

	SmtpPhase = "client probe";
	smtpmessage("RSET", m, mci);
	r = reply(m, mci, e, TimeOuts.to_miscshort, NULL);
	if (r < 0 || REPLYTYPE(r) != 2)
		smtpquit(m, mci, e);
	return r;
}
/*
**  REPLY -- read arpanet reply
**
**	Parameters:
**		m -- the mailer we are reading the reply from.
**		mci -- the mailer connection info structure.
**		e -- the current envelope.
**		timeout -- the timeout for reads.
**		pfunc -- processing function called on each line of response.
**			If null, no special processing is done.
**
**	Returns:
**		reply code it reads.
**
**	Side Effects:
**		flushes the mail file.
*/

int
reply(m, mci, e, timeout, pfunc)
	MAILER *m;
	MCI *mci;
	ENVELOPE *e;
	time_t timeout;
	void (*pfunc)();
{
	register char *bufp;
	register int r;
	bool firstline = TRUE;
	char junkbuf[MAXLINE];

	if (mci->mci_out != NULL)
		(void) fflush(mci->mci_out);

	if (tTd(18, 1))
		printf("reply\n");

	/*
	**  Read the input line, being careful not to hang.
	*/

	for (bufp = SmtpReplyBuffer;; bufp = junkbuf)
	{
		register char *p;
		extern time_t curtime();

		/* actually do the read */
		if (e->e_xfp != NULL)
			(void) fflush(e->e_xfp);	/* for debugging */

		/* if we are in the process of closing just give the code */
		if (mci->mci_state == MCIS_CLOSED)
			return (SMTPCLOSING);

		if (mci->mci_out != NULL)
			fflush(mci->mci_out);

		/* get the line from the other side */
		p = sfgets(bufp, MAXLINE, mci->mci_in, timeout, SmtpPhase);
		mci->mci_lastuse = curtime();

		if (p == NULL)
		{
			bool oldholderrs;
			extern char MsgBuf[];

			/* if the remote end closed early, fake an error */
			if (errno == 0)
# ifdef ECONNRESET
				errno = ECONNRESET;
# else /* ECONNRESET */
				errno = EPIPE;
# endif /* ECONNRESET */

			mci->mci_errno = errno;
			oldholderrs = HoldErrs;
			HoldErrs = TRUE;
			usrerr("451 reply: read error from %s", mci->mci_host);
			mci_setstat(mci, EX_TEMPFAIL, "4.4.2", MsgBuf);

			/* if debugging, pause so we can see state */
			if (tTd(18, 100))
				pause();
			mci->mci_state = MCIS_ERROR;
			smtpquit(m, mci, e);
#if XDEBUG
			{
				char wbuf[MAXLINE];
				char *p = wbuf;
				int wbufleft = sizeof wbuf;

				if (e->e_to != NULL)
				{
					int plen;

					snprintf(p, wbufleft, "%s... ",
						shortenstring(e->e_to, 203));
					plen = strlen(p);
					p += plen;
					wbufleft -= plen;
				}
				snprintf(p, wbufleft, "reply(%.100s) during %s",
					mci->mci_host, SmtpPhase);
				checkfd012(wbuf);
			}
#endif
			HoldErrs = oldholderrs;
			return (-1);
		}
		fixcrlf(bufp, TRUE);

		/* EHLO failure is not a real error */
		if (e->e_xfp != NULL && (bufp[0] == '4' ||
		    (bufp[0] == '5' && strncmp(SmtpMsgBuffer, "EHLO", 4) != 0)))
		{
			/* serious error -- log the previous command */
			if (SmtpNeedIntro)
			{
				/* inform user who we are chatting with */
				fprintf(CurEnv->e_xfp,
					"... while talking to %s:\n",
					CurHostName);
				SmtpNeedIntro = FALSE;
			}
			if (SmtpMsgBuffer[0] != '\0')
				fprintf(e->e_xfp, ">>> %s\n", SmtpMsgBuffer);
			SmtpMsgBuffer[0] = '\0';

			/* now log the message as from the other side */
			fprintf(e->e_xfp, "<<< %s\n", bufp);
		}

		/* display the input for verbose mode */
		if (Verbose)
			nmessage("050 %s", bufp);

		/* process the line */
		if (pfunc != NULL)
			(*pfunc)(bufp, firstline, m, mci, e);

		firstline = FALSE;

		/* if continuation is required, we can go on */
		if (bufp[3] == '-')
			continue;

		/* ignore improperly formated input */
		if (!(isascii(bufp[0]) && isdigit(bufp[0])))
			continue;

		/* decode the reply code */
		r = atoi(bufp);

		/* extra semantics: 0xx codes are "informational" */
		if (r >= 100)
			break;
	}

	/*
	**  Now look at SmtpReplyBuffer -- only care about the first
	**  line of the response from here on out.
	*/

	/* save temporary failure messages for posterity */
	if (SmtpReplyBuffer[0] == '4' && SmtpError[0] == '\0')
		snprintf(SmtpError, sizeof SmtpError, "%s", SmtpReplyBuffer);

	/* reply code 421 is "Service Shutting Down" */
	if (r == SMTPCLOSING && mci->mci_state != MCIS_SSD)
	{
		/* send the quit protocol */
		mci->mci_state = MCIS_SSD;
		smtpquit(m, mci, e);
	}

	return (r);
}
/*
**  SMTPMESSAGE -- send message to server
**
**	Parameters:
**		f -- format
**		m -- the mailer to control formatting.
**		a, b, c -- parameters
**
**	Returns:
**		none.
**
**	Side Effects:
**		writes message to mci->mci_out.
*/

/*VARARGS1*/
void
#ifdef __STDC__
smtpmessage(char *f, MAILER *m, MCI *mci, ...)
#else
smtpmessage(f, m, mci, va_alist)
	char *f;
	MAILER *m;
	MCI *mci;
	va_dcl
#endif
{
	VA_LOCAL_DECL

	VA_START(mci);
	(void) vsnprintf(SmtpMsgBuffer, sizeof SmtpMsgBuffer, f, ap);
	VA_END;

	if (tTd(18, 1) || Verbose)
		nmessage(">>> %s", SmtpMsgBuffer);
	if (TrafficLogFile != NULL)
		fprintf(TrafficLogFile, "%05d >>> %s\n",
			(int) getpid(), SmtpMsgBuffer);
	if (mci->mci_out != NULL)
	{
		fprintf(mci->mci_out, "%s%s", SmtpMsgBuffer,
			m == NULL ? "\r\n" : m->m_eol);
	}
	else if (tTd(18, 1))
	{
		printf("smtpmessage: NULL mci_out\n");
	}
}

# endif /* SMTP */
