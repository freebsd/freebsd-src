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

# include "sendmail.h"

#ifndef lint
#ifdef SMTP
static char sccsid[] = "@(#)srvrsmtp.c	8.32 (Berkeley) 3/8/94 (with SMTP)";
#else
static char sccsid[] = "@(#)srvrsmtp.c	8.32 (Berkeley) 3/8/94 (without SMTP)";
#endif
#endif /* not lint */

# include <errno.h>

# ifdef SMTP

/*
**  SMTP -- run the SMTP protocol.
**
**	Parameters:
**		none.
**
**	Returns:
**		never.
**
**	Side Effects:
**		Reads commands from the input channel and processes
**			them.
*/

struct cmd
{
	char	*cmdname;	/* command name */
	int	cmdcode;	/* internal code, see below */
};

/* values for cmdcode */
# define CMDERROR	0	/* bad command */
# define CMDMAIL	1	/* mail -- designate sender */
# define CMDRCPT	2	/* rcpt -- designate recipient */
# define CMDDATA	3	/* data -- send message text */
# define CMDRSET	4	/* rset -- reset state */
# define CMDVRFY	5	/* vrfy -- verify address */
# define CMDEXPN	6	/* expn -- expand address */
# define CMDNOOP	7	/* noop -- do nothing */
# define CMDQUIT	8	/* quit -- close connection and die */
# define CMDHELO	9	/* helo -- be polite */
# define CMDHELP	10	/* help -- give usage info */
# define CMDEHLO	11	/* ehlo -- extended helo (RFC 1425) */
/* non-standard commands */
# define CMDONEX	16	/* onex -- sending one transaction only */
# define CMDVERB	17	/* verb -- go into verbose mode */
/* use this to catch and log "door handle" attempts on your system */
# define CMDLOGBOGUS	23	/* bogus command that should be logged */
/* debugging-only commands, only enabled if SMTPDEBUG is defined */
# define CMDDBGQSHOW	24	/* showq -- show send queue */
# define CMDDBGDEBUG	25	/* debug -- set debug mode */

static struct cmd	CmdTab[] =
{
	"mail",		CMDMAIL,
	"rcpt",		CMDRCPT,
	"data",		CMDDATA,
	"rset",		CMDRSET,
	"vrfy",		CMDVRFY,
	"expn",		CMDEXPN,
	"help",		CMDHELP,
	"noop",		CMDNOOP,
	"quit",		CMDQUIT,
	"helo",		CMDHELO,
	"ehlo",		CMDEHLO,
	"verb",		CMDVERB,
	"onex",		CMDONEX,
	/*
	 * remaining commands are here only
	 * to trap and log attempts to use them
	 */
	"showq",	CMDDBGQSHOW,
	"debug",	CMDDBGDEBUG,
	"wiz",		CMDLOGBOGUS,
	NULL,		CMDERROR,
};

bool	OneXact = FALSE;		/* one xaction only this run */
char	*CurSmtpClient;			/* who's at the other end of channel */

static char	*skipword();
extern char	RealUserName[];


#define MAXBADCOMMANDS	25		/* maximum number of bad commands */

smtp(e)
	register ENVELOPE *e;
{
	register char *p;
	register struct cmd *c;
	char *cmd;
	auto ADDRESS *vrfyqueue;
	ADDRESS *a;
	bool gotmail;			/* mail command received */
	bool gothello;			/* helo command received */
	bool vrfy;			/* set if this is a vrfy command */
	char *protocol;			/* sending protocol */
	char *sendinghost;		/* sending hostname */
	long msize;			/* approximate maximum message size */
	char *peerhostname;		/* name of SMTP peer or "localhost" */
	auto char *delimptr;
	char *id;
	int nrcpts;			/* number of RCPT commands */
	bool doublequeue;
	int badcommands = 0;		/* count of bad commands */
	char inp[MAXLINE];
	char cmdbuf[MAXLINE];
	extern char Version[];
	extern ENVELOPE BlankEnvelope;

	if (fileno(OutChannel) != fileno(stdout))
	{
		/* arrange for debugging output to go to remote host */
		(void) dup2(fileno(OutChannel), fileno(stdout));
	}
	settime(e);
	peerhostname = RealHostName;
	if (peerhostname == NULL)
		peerhostname = "localhost";
	CurHostName = peerhostname;
	CurSmtpClient = macvalue('_', e);
	if (CurSmtpClient == NULL)
		CurSmtpClient = CurHostName;

	setproctitle("server %s startup", CurSmtpClient);
	expand("\201e", inp, &inp[sizeof inp], e);
	if (BrokenSmtpPeers)
	{
		message("220 %s", inp);
	}
	else
	{
		message("220-%s", inp);
		message("220 ESMTP spoken here");
	}
	protocol = NULL;
	sendinghost = macvalue('s', e);
	gothello = FALSE;
	gotmail = FALSE;
	for (;;)
	{
		/* arrange for backout */
		if (setjmp(TopFrame) > 0)
		{
			/* if() nesting is necessary for Cray UNICOS */
			if (InChild)
			{
				QuickAbort = FALSE;
				SuprErrs = TRUE;
				finis();
			}
		}
		QuickAbort = FALSE;
		HoldErrs = FALSE;
		LogUsrErrs = FALSE;
		e->e_flags &= ~(EF_VRFYONLY|EF_GLOBALERRS);

		/* setup for the read */
		e->e_to = NULL;
		Errors = 0;
		(void) fflush(stdout);

		/* read the input line */
		SmtpPhase = "server cmd read";
		setproctitle("server %s cmd read", CurHostName);
		p = sfgets(inp, sizeof inp, InChannel, TimeOuts.to_nextcommand,
				SmtpPhase);

		/* handle errors */
		if (p == NULL)
		{
			/* end of file, just die */
			disconnect(1, e);
			message("421 %s Lost input channel from %s",
				MyHostName, CurSmtpClient);
#ifdef LOG
			if (LogLevel > (gotmail ? 1 : 19))
				syslog(LOG_NOTICE, "lost input channel from %s",
					CurSmtpClient);
#endif
			if (InChild)
				ExitStat = EX_QUIT;
			finis();
		}

		/* clean up end of line */
		fixcrlf(inp, TRUE);

		/* echo command to transcript */
		if (e->e_xfp != NULL)
			fprintf(e->e_xfp, "<<< %s\n", inp);

		if (e->e_id == NULL)
			setproctitle("%s: %.80s", CurSmtpClient, inp);
		else
			setproctitle("%s %s: %.80s", e->e_id, CurSmtpClient, inp);

		/* break off command */
		for (p = inp; isascii(*p) && isspace(*p); p++)
			continue;
		cmd = cmdbuf;
		while (*p != '\0' &&
		       !(isascii(*p) && isspace(*p)) &&
		       cmd < &cmdbuf[sizeof cmdbuf - 2])
			*cmd++ = *p++;
		*cmd = '\0';

		/* throw away leading whitespace */
		while (isascii(*p) && isspace(*p))
			p++;

		/* decode command */
		for (c = CmdTab; c->cmdname != NULL; c++)
		{
			if (!strcasecmp(c->cmdname, cmdbuf))
				break;
		}

		/* reset errors */
		errno = 0;

		/* process command */
		switch (c->cmdcode)
		{
		  case CMDHELO:		/* hello -- introduce yourself */
		  case CMDEHLO:		/* extended hello */
			if (c->cmdcode == CMDEHLO)
			{
				protocol = "ESMTP";
				SmtpPhase = "server EHLO";
			}
			else
			{
				protocol = "SMTP";
				SmtpPhase = "server HELO";
			}
			sendinghost = newstr(p);
			gothello = TRUE;
			if (c->cmdcode != CMDEHLO)
			{
				/* print old message and be done with it */
				message("250 %s Hello %s, pleased to meet you",
					MyHostName, CurSmtpClient);
				break;
			}
			
			/* print extended message and brag */
			message("250-%s Hello %s, pleased to meet you",
				MyHostName, p);
			if (!bitset(PRIV_NOEXPN, PrivacyFlags))
				message("250-EXPN");
			if (MaxMessageSize > 0)
				message("250-SIZE %ld", MaxMessageSize);
			else
				message("250-SIZE");
			message("250 HELP");
			break;

		  case CMDMAIL:		/* mail -- designate sender */
			SmtpPhase = "server MAIL";

			/* check for validity of this command */
			if (!gothello)
			{
				/* set sending host to our known value */
				if (sendinghost == NULL)
					sendinghost = peerhostname;

				if (bitset(PRIV_NEEDMAILHELO, PrivacyFlags))
				{
					message("503 Polite people say HELO first");
					break;
				}
			}
			if (gotmail)
			{
				message("503 Sender already specified");
				if (InChild)
					finis();
				break;
			}
			if (InChild)
			{
				errno = 0;
				syserr("503 Nested MAIL command: MAIL %s", p);
				finis();
			}

			/* fork a subprocess to process this command */
			if (runinchild("SMTP-MAIL", e) > 0)
				break;
			if (!gothello)
			{
				auth_warning(e,
					"Host %s didn't use HELO protocol",
					peerhostname);
			}
#ifdef PICKY_HELO_CHECK
			if (strcasecmp(sendinghost, peerhostname) != 0 &&
			    (strcasecmp(peerhostname, "localhost") != 0 ||
			     strcasecmp(sendinghost, MyHostName) != 0))
			{
				auth_warning(e, "Host %s claimed to be %s",
					peerhostname, sendinghost);
			}
#endif

			if (protocol == NULL)
				protocol = "SMTP";
			define('r', protocol, e);
			define('s', sendinghost, e);
			initsys(e);
			nrcpts = 0;
			e->e_flags |= EF_LOGSENDER;
			setproctitle("%s %s: %.80s", e->e_id, CurSmtpClient, inp);

			/* child -- go do the processing */
			p = skipword(p, "from");
			if (p == NULL)
				break;
			if (setjmp(TopFrame) > 0)
			{
				/* this failed -- undo work */
				if (InChild)
				{
					QuickAbort = FALSE;
					SuprErrs = TRUE;
					e->e_flags &= ~EF_FATALERRS;
					finis();
				}
				break;
			}
			QuickAbort = TRUE;

			/* must parse sender first */
			delimptr = NULL;
			setsender(p, e, &delimptr, FALSE);
			p = delimptr;
			if (p != NULL && *p != '\0')
				*p++ = '\0';

			/* check for possible spoofing */
			if (RealUid != 0 && OpMode == MD_SMTP &&
			    (e->e_from.q_mailer != LocalMailer &&
			     strcmp(e->e_from.q_user, RealUserName) != 0))
			{
				auth_warning(e, "%s owned process doing -bs",
					RealUserName);
			}

			/* now parse ESMTP arguments */
			msize = 0;
			for (; p != NULL && *p != '\0'; p++)
			{
				char *kp;
				char *vp = NULL;

				/* locate the beginning of the keyword */
				while (isascii(*p) && isspace(*p))
					p++;
				if (*p == '\0')
					break;
				kp = p;

				/* skip to the value portion */
				while (isascii(*p) && isalnum(*p) || *p == '-')
					p++;
				if (*p == '=')
				{
					*p++ = '\0';
					vp = p;

					/* skip to the end of the value */
					while (*p != '\0' && *p != ' ' &&
					       !(isascii(*p) && iscntrl(*p)) &&
					       *p != '=')
						p++;
				}

				if (*p != '\0')
					*p++ = '\0';

				if (tTd(19, 1))
					printf("MAIL: got arg %s=%s\n", kp,
						vp == NULL ? "<null>" : vp);

				if (strcasecmp(kp, "size") == 0)
				{
					if (vp == NULL)
					{
						usrerr("501 SIZE requires a value");
						/* NOTREACHED */
					}
					msize = atol(vp);
				}
				else if (strcasecmp(kp, "body") == 0)
				{
					if (vp == NULL)
					{
						usrerr("501 BODY requires a value");
						/* NOTREACHED */
					}
# ifdef MIME
					if (strcasecmp(vp, "8bitmime") == 0)
					{
						e->e_bodytype = "8BITMIME";
						SevenBit = FALSE;
					}
					else if (strcasecmp(vp, "7bit") == 0)
					{
						e->e_bodytype = "7BIT";
						SevenBit = TRUE;
					}
					else
					{
						usrerr("501 Unknown BODY type %s",
							vp);
					}
# endif
				}
				else
				{
					usrerr("501 %s parameter unrecognized", kp);
					/* NOTREACHED */
				}
			}

			if (MaxMessageSize > 0 && msize > MaxMessageSize)
			{
				usrerr("552 Message size exceeds fixed maximum message size (%ld)",
					MaxMessageSize);
				/* NOTREACHED */
			}
				
			if (!enoughspace(msize))
			{
				message("452 Insufficient disk space; try again later");
				break;
			}
			message("250 Sender ok");
			gotmail = TRUE;
			break;

		  case CMDRCPT:		/* rcpt -- designate recipient */
			if (!gotmail)
			{
				usrerr("503 Need MAIL before RCPT");
				break;
			}
			SmtpPhase = "server RCPT";
			if (setjmp(TopFrame) > 0)
			{
				e->e_flags &= ~EF_FATALERRS;
				break;
			}
			QuickAbort = TRUE;
			LogUsrErrs = TRUE;

			if (e->e_sendmode != SM_DELIVER)
				e->e_flags |= EF_VRFYONLY;

			p = skipword(p, "to");
			if (p == NULL)
				break;
			a = parseaddr(p, NULLADDR, RF_COPYALL, ' ', NULL, e);
			if (a == NULL)
				break;
			a->q_flags |= QPRIMARY;
			a = recipient(a, &e->e_sendqueue, e);
			if (Errors != 0)
				break;

			/* no errors during parsing, but might be a duplicate */
			e->e_to = p;
			if (!bitset(QBADADDR, a->q_flags))
			{
				message("250 Recipient ok%s",
					bitset(QQUEUEUP, a->q_flags) ?
						" (will queue)" : "");
				nrcpts++;
			}
			else
			{
				/* punt -- should keep message in ADDRESS.... */
				message("550 Addressee unknown");
			}
			e->e_to = NULL;
			break;

		  case CMDDATA:		/* data -- text of mail */
			SmtpPhase = "server DATA";
			if (!gotmail)
			{
				message("503 Need MAIL command");
				break;
			}
			else if (nrcpts <= 0)
			{
				message("503 Need RCPT (recipient)");
				break;
			}

			/* check to see if we need to re-expand aliases */
			/* also reset QBADADDR on already-diagnosted addrs */
			doublequeue = FALSE;
			for (a = e->e_sendqueue; a != NULL; a = a->q_next)
			{
				if (bitset(QVERIFIED, a->q_flags))
				{
					/* need to re-expand aliases */
					doublequeue = TRUE;
				}
				if (bitset(QBADADDR, a->q_flags))
				{
					/* make this "go away" */
					a->q_flags |= QDONTSEND;
					a->q_flags &= ~QBADADDR;
				}
			}

			/* collect the text of the message */
			SmtpPhase = "collect";
			collect(TRUE, doublequeue, e);
			if (Errors != 0)
				goto abortmessage;
			HoldErrs = TRUE;

			/*
			**  Arrange to send to everyone.
			**	If sending to multiple people, mail back
			**		errors rather than reporting directly.
			**	In any case, don't mail back errors for
			**		anything that has happened up to
			**		now (the other end will do this).
			**	Truncate our transcript -- the mail has gotten
			**		to us successfully, and if we have
			**		to mail this back, it will be easier
			**		on the reader.
			**	Then send to everyone.
			**	Finally give a reply code.  If an error has
			**		already been given, don't mail a
			**		message back.
			**	We goose error returns by clearing error bit.
			*/

			SmtpPhase = "delivery";
			if (nrcpts != 1 && !doublequeue)
			{
				HoldErrs = TRUE;
				e->e_errormode = EM_MAIL;
			}
			e->e_xfp = freopen(queuename(e, 'x'), "w", e->e_xfp);
			id = e->e_id;

			/* send to all recipients */
			sendall(e, doublequeue ? SM_QUEUE : SM_DEFAULT);
			e->e_to = NULL;

			/* issue success if appropriate and reset */
			if (Errors == 0 || HoldErrs)
				message("250 %s Message accepted for delivery", id);

			if (bitset(EF_FATALERRS, e->e_flags) && !HoldErrs)
			{
				/* avoid sending back an extra message */
				e->e_flags &= ~EF_FATALERRS;
				e->e_flags |= EF_CLRQUEUE;
			}
			else
			{
				/* from now on, we have to operate silently */
				HoldErrs = TRUE;
				e->e_errormode = EM_MAIL;

				/* if we just queued, poke it */
				if (doublequeue && e->e_sendmode != SM_QUEUE)
				{
					extern pid_t dowork();

					unlockqueue(e);
					(void) dowork(id, TRUE, TRUE, e);
				}
			}

  abortmessage:
			/* if in a child, pop back to our parent */
			if (InChild)
				finis();

			/* clean up a bit */
			gotmail = FALSE;
			dropenvelope(e);
			CurEnv = e = newenvelope(e, CurEnv);
			e->e_flags = BlankEnvelope.e_flags;
			break;

		  case CMDRSET:		/* rset -- reset state */
			message("250 Reset state");
			e->e_flags |= EF_CLRQUEUE;
			if (InChild)
				finis();

			/* clean up a bit */
			gotmail = FALSE;
			dropenvelope(e);
			CurEnv = e = newenvelope(e, CurEnv);
			break;

		  case CMDVRFY:		/* vrfy -- verify address */
		  case CMDEXPN:		/* expn -- expand address */
			vrfy = c->cmdcode == CMDVRFY;
			if (bitset(vrfy ? PRIV_NOVRFY : PRIV_NOEXPN,
						PrivacyFlags))
			{
				if (vrfy)
					message("252 Who's to say?");
				else
					message("502 Sorry, we do not allow this operation");
#ifdef LOG
				if (LogLevel > 5)
					syslog(LOG_INFO, "%s: %s [rejected]",
						CurSmtpClient, inp);
#endif
				break;
			}
			else if (!gothello &&
				 bitset(vrfy ? PRIV_NEEDVRFYHELO : PRIV_NEEDEXPNHELO,
						PrivacyFlags))
			{
				message("503 I demand that you introduce yourself first");
				break;
			}
			if (runinchild(vrfy ? "SMTP-VRFY" : "SMTP-EXPN", e) > 0)
				break;
#ifdef LOG
			if (LogLevel > 5)
				syslog(LOG_INFO, "%s: %s", CurSmtpClient, inp);
#endif
			vrfyqueue = NULL;
			QuickAbort = TRUE;
			if (vrfy)
				e->e_flags |= EF_VRFYONLY;
			while (*p != '\0' && isascii(*p) && isspace(*p))
				*p++;
			if (*p == '\0')
			{
				message("501 Argument required");
				Errors++;
			}
			else
			{
				(void) sendtolist(p, NULLADDR, &vrfyqueue, e);
			}
			if (Errors != 0)
			{
				if (InChild)
					finis();
				break;
			}
			if (vrfyqueue == NULL)
			{
				message("554 Nothing to %s", vrfy ? "VRFY" : "EXPN");
			}
			while (vrfyqueue != NULL)
			{
				a = vrfyqueue;
				while ((a = a->q_next) != NULL &&
				       bitset(QDONTSEND|QBADADDR, a->q_flags))
					continue;
				if (!bitset(QDONTSEND|QBADADDR, vrfyqueue->q_flags))
					printvrfyaddr(vrfyqueue, a == NULL);
				vrfyqueue = vrfyqueue->q_next;
			}
			if (InChild)
				finis();
			break;

		  case CMDHELP:		/* help -- give user info */
			help(p);
			break;

		  case CMDNOOP:		/* noop -- do nothing */
			message("250 OK");
			break;

		  case CMDQUIT:		/* quit -- leave mail */
			message("221 %s closing connection", MyHostName);

doquit:
			/* avoid future 050 messages */
			disconnect(1, e);

			if (InChild)
				ExitStat = EX_QUIT;
			finis();

		  case CMDVERB:		/* set verbose mode */
			if (bitset(PRIV_NOEXPN, PrivacyFlags))
			{
				/* this would give out the same info */
				message("502 Verbose unavailable");
				break;
			}
			Verbose = TRUE;
			e->e_sendmode = SM_DELIVER;
			message("250 Verbose mode");
			break;

		  case CMDONEX:		/* doing one transaction only */
			OneXact = TRUE;
			message("250 Only one transaction");
			break;

# ifdef SMTPDEBUG
		  case CMDDBGQSHOW:	/* show queues */
			printf("Send Queue=");
			printaddr(e->e_sendqueue, TRUE);
			break;

		  case CMDDBGDEBUG:	/* set debug mode */
			tTsetup(tTdvect, sizeof tTdvect, "0-99.1");
			tTflag(p);
			message("200 Debug set");
			break;

# else /* not SMTPDEBUG */
		  case CMDDBGQSHOW:	/* show queues */
		  case CMDDBGDEBUG:	/* set debug mode */
# endif /* SMTPDEBUG */
		  case CMDLOGBOGUS:	/* bogus command */
# ifdef LOG
			if (LogLevel > 0)
				syslog(LOG_CRIT,
				    "\"%s\" command from %s (%s)",
				    c->cmdname, peerhostname,
				    anynet_ntoa(&RealHostAddr));
# endif
			/* FALL THROUGH */

		  case CMDERROR:	/* unknown command */
			if (++badcommands > MAXBADCOMMANDS)
			{
				message("421 %s Too many bad commands; closing connection",
					MyHostName);
				goto doquit;
			}

			message("500 Command unrecognized");
			break;

		  default:
			errno = 0;
			syserr("500 smtp: unknown code %d", c->cmdcode);
			break;
		}
	}
}
/*
**  SKIPWORD -- skip a fixed word.
**
**	Parameters:
**		p -- place to start looking.
**		w -- word to skip.
**
**	Returns:
**		p following w.
**		NULL on error.
**
**	Side Effects:
**		clobbers the p data area.
*/

static char *
skipword(p, w)
	register char *p;
	char *w;
{
	register char *q;
	char *firstp = p;

	/* find beginning of word */
	while (isascii(*p) && isspace(*p))
		p++;
	q = p;

	/* find end of word */
	while (*p != '\0' && *p != ':' && !(isascii(*p) && isspace(*p)))
		p++;
	while (isascii(*p) && isspace(*p))
		*p++ = '\0';
	if (*p != ':')
	{
	  syntax:
		message("501 Syntax error in parameters scanning \"%s\"",
			firstp);
		Errors++;
		return (NULL);
	}
	*p++ = '\0';
	while (isascii(*p) && isspace(*p))
		p++;

	if (*p == '\0')
		goto syntax;

	/* see if the input word matches desired word */
	if (strcasecmp(q, w))
		goto syntax;

	return (p);
}
/*
**  PRINTVRFYADDR -- print an entry in the verify queue
**
**	Parameters:
**		a -- the address to print
**		last -- set if this is the last one.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Prints the appropriate 250 codes.
*/

printvrfyaddr(a, last)
	register ADDRESS *a;
	bool last;
{
	char fmtbuf[20];

	strcpy(fmtbuf, "250");
	fmtbuf[3] = last ? ' ' : '-';

	if (a->q_fullname == NULL)
	{
		if (strchr(a->q_user, '@') == NULL)
			strcpy(&fmtbuf[4], "<%s@%s>");
		else
			strcpy(&fmtbuf[4], "<%s>");
		message(fmtbuf, a->q_user, MyHostName);
	}
	else
	{
		if (strchr(a->q_user, '@') == NULL)
			strcpy(&fmtbuf[4], "%s <%s@%s>");
		else
			strcpy(&fmtbuf[4], "%s <%s>");
		message(fmtbuf, a->q_fullname, a->q_user, MyHostName);
	}
}
/*
**  HELP -- implement the HELP command.
**
**	Parameters:
**		topic -- the topic we want help for.
**
**	Returns:
**		none.
**
**	Side Effects:
**		outputs the help file to message output.
*/

help(topic)
	char *topic;
{
	register FILE *hf;
	int len;
	char buf[MAXLINE];
	bool noinfo;

	if (HelpFile == NULL || (hf = fopen(HelpFile, "r")) == NULL)
	{
		/* no help */
		errno = 0;
		message("502 HELP not implemented");
		return;
	}

	if (topic == NULL || *topic == '\0')
		topic = "smtp";
	else
		makelower(topic);

	len = strlen(topic);
	noinfo = TRUE;

	while (fgets(buf, sizeof buf, hf) != NULL)
	{
		if (strncmp(buf, topic, len) == 0)
		{
			register char *p;

			p = strchr(buf, '\t');
			if (p == NULL)
				p = buf;
			else
				p++;
			fixcrlf(p, TRUE);
			message("214-%s", p);
			noinfo = FALSE;
		}
	}

	if (noinfo)
		message("504 HELP topic unknown");
	else
		message("214 End of HELP info");
	(void) fclose(hf);
}
/*
**  RUNINCHILD -- return twice -- once in the child, then in the parent again
**
**	Parameters:
**		label -- a string used in error messages
**
**	Returns:
**		zero in the child
**		one in the parent
**
**	Side Effects:
**		none.
*/

runinchild(label, e)
	char *label;
	register ENVELOPE *e;
{
	int childpid;

	if (!OneXact)
	{
		childpid = dofork();
		if (childpid < 0)
		{
			syserr("%s: cannot fork", label);
			return (1);
		}
		if (childpid > 0)
		{
			auto int st;

			/* parent -- wait for child to complete */
			setproctitle("server %s child wait", CurHostName);
			st = waitfor(childpid);
			if (st == -1)
				syserr("%s: lost child", label);
			else if (!WIFEXITED(st))
				syserr("%s: died on signal %d",
					label, st & 0177);

			/* if we exited on a QUIT command, complete the process */
			if (WEXITSTATUS(st) == EX_QUIT)
			{
				disconnect(1, e);
				finis();
			}

			return (1);
		}
		else
		{
			/* child */
			InChild = TRUE;
			QuickAbort = FALSE;
			clearenvelope(e, FALSE);
		}
	}

	/* open alias database */
	initmaps(FALSE, e);

	return (0);
}

# endif /* SMTP */
