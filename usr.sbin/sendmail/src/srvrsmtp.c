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
#ifdef SMTP
static char sccsid[] = "@(#)srvrsmtp.c	8.123 (Berkeley) 10/12/96 (with SMTP)";
#else
static char sccsid[] = "@(#)srvrsmtp.c	8.123 (Berkeley) 10/12/96 (without SMTP)";
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
# define CMDETRN	12	/* etrn -- flush queue */
/* non-standard commands */
# define CMDONEX	16	/* onex -- sending one transaction only */
# define CMDVERB	17	/* verb -- go into verbose mode */
# define CMDXUSR	18	/* xusr -- initial (user) submission */
/* use this to catch and log "door handle" attempts on your system */
# define CMDLOGBOGUS	23	/* bogus command that should be logged */
/* debugging-only commands, only enabled if SMTPDEBUG is defined */
# define CMDDBGQSHOW	24	/* showq -- show send queue */
# define CMDDBGDEBUG	25	/* debug -- set debug mode */

static struct cmd	CmdTab[] =
{
	{ "mail",	CMDMAIL		},
	{ "rcpt",	CMDRCPT		},
	{ "data",	CMDDATA		},
	{ "rset",	CMDRSET		},
	{ "vrfy",	CMDVRFY		},
	{ "expn",	CMDEXPN		},
	{ "help",	CMDHELP		},
	{ "noop",	CMDNOOP		},
	{ "quit",	CMDQUIT		},
	{ "helo",	CMDHELO		},
	{ "ehlo",	CMDEHLO		},
	{ "etrn",	CMDETRN		},
	{ "verb",	CMDVERB		},
	{ "onex",	CMDONEX		},
	{ "xusr",	CMDXUSR		},
    /* remaining commands are here only to trap and log attempts to use them */
	{ "showq",	CMDDBGQSHOW	},
	{ "debug",	CMDDBGDEBUG	},
	{ "wiz",	CMDLOGBOGUS	},

	{ NULL,		CMDERROR	}
};

bool	OneXact = FALSE;		/* one xaction only this run */
char	*CurSmtpClient;			/* who's at the other end of channel */

static char	*skipword();


#define MAXBADCOMMANDS	25		/* maximum number of bad commands */

void
smtp(nullserver, e)
	bool nullserver;
	register ENVELOPE *volatile e;
{
	register char *p;
	register struct cmd *c;
	char *cmd;
	auto ADDRESS *vrfyqueue;
	ADDRESS *a;
	volatile bool gotmail;		/* mail command received */
	volatile bool gothello;		/* helo command received */
	bool vrfy;			/* set if this is a vrfy command */
	char *volatile protocol;	/* sending protocol */
	char *volatile sendinghost;	/* sending hostname */
	char *volatile peerhostname;	/* name of SMTP peer or "localhost" */
	auto char *delimptr;
	char *id;
	volatile int nrcpts = 0;	/* number of RCPT commands */
	bool doublequeue;
	volatile int badcommands = 0;	/* count of bad commands */
	volatile int nverifies = 0;	/* count of VRFY/EXPN commands */
	volatile int n_etrn = 0;	/* count of ETRN commands */
	char inp[MAXLINE];
	char cmdbuf[MAXLINE];
	extern ENVELOPE BlankEnvelope;
	extern void help __P((char *));
	extern void settime __P((ENVELOPE *));
	extern bool enoughdiskspace __P((long));
	extern int runinchild __P((char *, ENVELOPE *));

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
#ifdef LOG
	if (LogLevel > 11)
	{
		/* log connection information */
		syslog(LOG_INFO, "SMTP connect from %.100s (%.100s)",
			CurSmtpClient, anynet_ntoa(&RealHostAddr));
	}
#endif

	/* output the first line, inserting "ESMTP" as second word */
	expand(SmtpGreeting, inp, sizeof inp, e);
	p = strchr(inp, '\n');
	if (p != NULL)
		*p++ = '\0';
	id = strchr(inp, ' ');
	if (id == NULL)
		id = &inp[strlen(inp)];
	cmd = p == NULL ? "220 %.*s ESMTP%s" : "220-%.*s ESMTP%s";
	message(cmd, id - inp, inp, id);

	/* output remaining lines */
	while ((id = p) != NULL && (p = strchr(id, '\n')) != NULL)
	{
		*p++ = '\0';
		if (isascii(*id) && isspace(*id))
			id++;
		message("220-%s", id);
	}
	if (id != NULL)
	{
		if (isascii(*id) && isspace(*id))
			id++;
		message("220 %s", id);
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
		setproctitle("server %s cmd read", CurSmtpClient);
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
				syslog(LOG_NOTICE, "lost input channel from %.100s",
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

#ifdef LOG
		if (LogLevel >= 15)
			syslog(LOG_INFO, "<-- %s", inp);
#endif

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

		/*
		**  Process command.
		**
		**	If we are running as a null server, return 550
		**	to everything.
		*/

		if (nullserver && c->cmdcode != CMDQUIT)
		{
			message("550 Access denied");
			continue;
		}

		/* non-null server */
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

			/* check for valid domain name (re 1123 5.2.5) */
			if (*p == '\0' && !AllowBogusHELO)
			{
				message("501 %s requires domain address",
					cmdbuf);
				break;
			}
			else
			{
				register char *q;

				for (q = p; *q != '\0'; q++)
				{
					if (!isascii(*q))
						break;
					if (isalnum(*q))
						continue;
					if (isspace(*q))
					{
						*q = '\0';
						break;
					}
					if (strchr("[].-_#", *q) == NULL)
						break;
				}
				if (*q != '\0')
				{
					if (!AllowBogusHELO)
						message("501 Invalid domain name");
					else
						message("250 %s Invalid domain name, accepting anyway",
							MyHostName);
					break;
				}
			}

			/* check for duplicate HELO/EHLO per RFC 1651 4.2 */
			if (gothello)
			{
				message("503 %s Duplicate HELO/EHLO",
					MyHostName);
				break;
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
				MyHostName, CurSmtpClient);
			if (!bitset(PRIV_NOEXPN, PrivacyFlags))
			{
				message("250-EXPN");
				message("250-VERB");
			}
#if MIME8TO7
			message("250-8BITMIME");
#endif
			if (MaxMessageSize > 0)
				message("250-SIZE %ld", MaxMessageSize);
			else
				message("250-SIZE");
#if DSN
			if (SendMIMEErrors)
				message("250-DSN");
#endif
			message("250-ONEX");
			message("250-ETRN");
			message("250-XUSR");
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

			p = skipword(p, "from");
			if (p == NULL)
				break;

			/* fork a subprocess to process this command */
			if (runinchild("SMTP-MAIL", e) > 0)
				break;
			if (!gothello)
			{
				auth_warning(e,
					"%s didn't use HELO protocol",
					CurSmtpClient);
			}
#ifdef PICKY_HELO_CHECK
			if (strcasecmp(sendinghost, peerhostname) != 0 &&
			    (strcasecmp(peerhostname, "localhost") != 0 ||
			     strcasecmp(sendinghost, MyHostName) != 0))
			{
				auth_warning(e, "Host %s claimed to be %s",
					CurSmtpClient, sendinghost);
			}
#endif

			if (protocol == NULL)
				protocol = "SMTP";
			define('r', protocol, e);
			define('s', sendinghost, e);
			initsys(e);
			nrcpts = 0;
			e->e_flags |= EF_LOGSENDER|EF_CLRQUEUE;
			setproctitle("%s %s: %.80s", e->e_id, CurSmtpClient, inp);

			/* child -- go do the processing */
			if (setjmp(TopFrame) > 0)
			{
				/* this failed -- undo work */
 undo_subproc:
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
			if (delimptr != NULL && *delimptr != '\0')
				*delimptr++ = '\0';

			/* do config file checking of the sender */
			if (rscheck("check_mail", p, NULL, e) != EX_OK)
				goto undo_subproc;

			/* check for possible spoofing */
			if (RealUid != 0 && OpMode == MD_SMTP &&
			    !wordinclass(RealUserName, 't') &&
			    !bitnset(M_LOCALMAILER, e->e_from.q_mailer->m_flags) &&
			    strcmp(e->e_from.q_user, RealUserName) != 0)
			{
				auth_warning(e, "%s owned process doing -bs",
					RealUserName);
			}

			/* now parse ESMTP arguments */
			e->e_msgsize = 0;
			p = delimptr;
			while (p != NULL && *p != '\0')
			{
				char *kp;
				char *vp = NULL;
				extern void mail_esmtp_args __P((char *, char *, ENVELOPE *));

				/* locate the beginning of the keyword */
				while (isascii(*p) && isspace(*p))
					p++;
				if (*p == '\0')
					break;
				kp = p;

				/* skip to the value portion */
				while ((isascii(*p) && isalnum(*p)) || *p == '-')
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
					printf("MAIL: got arg %s=\"%s\"\n", kp,
						vp == NULL ? "<null>" : vp);

				mail_esmtp_args(kp, vp, e);
			}

			if (MaxMessageSize > 0 && e->e_msgsize > MaxMessageSize)
			{
				usrerr("552 Message size exceeds fixed maximum message size (%ld)",
					MaxMessageSize);
				/* NOTREACHED */
			}
				
			if (!enoughdiskspace(e->e_msgsize))
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
			a = parseaddr(p, NULLADDR, RF_COPYALL, ' ', &delimptr, e);
			if (a == NULL)
				break;
			if (delimptr != NULL && *delimptr != '\0')
				*delimptr++ = '\0';

			/* do config file checking of the recipient */
			if (rscheck("check_rcpt", p, NULL, e) != EX_OK)
				break;

			/* now parse ESMTP arguments */
			p = delimptr;
			while (p != NULL && *p != '\0')
			{
				char *kp;
				char *vp = NULL;
				extern void rcpt_esmtp_args __P((ADDRESS *, char *, char *, ENVELOPE *));

				/* locate the beginning of the keyword */
				while (isascii(*p) && isspace(*p))
					p++;
				if (*p == '\0')
					break;
				kp = p;

				/* skip to the value portion */
				while ((isascii(*p) && isalnum(*p)) || *p == '-')
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
					printf("RCPT: got arg %s=\"%s\"\n", kp,
						vp == NULL ? "<null>" : vp);

				rcpt_esmtp_args(a, kp, vp, e);
			}

			/* save in recipient list after ESMTP mods */
			a = recipient(a, &e->e_sendqueue, 0, e);

			if (Errors != 0)
				break;

			/* no errors during parsing, but might be a duplicate */
			e->e_to = a->q_paddr;
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
			buffer_errors();
			collect(InChannel, TRUE, doublequeue, NULL, e);
			flush_errors(TRUE);
			if (Errors != 0)
				goto abortmessage;

			/* make sure we actually do delivery */
			e->e_flags &= ~EF_CLRQUEUE;

			/* from now on, we have to operate silently */
			buffer_errors();
			e->e_errormode = EM_MAIL;

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
			e->e_xfp = freopen(queuename(e, 'x'), "w", e->e_xfp);
			id = e->e_id;

			if (doublequeue)
			{
				/* make sure it is in the queue */
				queueup(e, FALSE);
			}
			else
			{
				/* send to all recipients */
				sendall(e, SM_DEFAULT);
			}
			e->e_to = NULL;

			/* issue success message */
			message("250 %s Message accepted for delivery", id);

			/* if we just queued, poke it */
			if (doublequeue && e->e_sendmode != SM_QUEUE &&
			    e->e_sendmode != SM_DEFER)
			{
				extern pid_t dowork();

				unlockqueue(e);
				(void) dowork(id, TRUE, TRUE, e);
			}

  abortmessage:
			/* if in a child, pop back to our parent */
			if (InChild)
				finis();

			/* clean up a bit */
			gotmail = FALSE;
			dropenvelope(e, TRUE);
			CurEnv = e = newenvelope(e, CurEnv);
			e->e_flags = BlankEnvelope.e_flags;
			break;

		  case CMDRSET:		/* rset -- reset state */
			message("250 Reset state");

			/* arrange to ignore any current send list */
			e->e_sendqueue = NULL;
			e->e_flags |= EF_CLRQUEUE;
			if (InChild)
				finis();

			/* clean up a bit */
			gotmail = FALSE;
			dropenvelope(e, TRUE);
			CurEnv = e = newenvelope(e, CurEnv);
			break;

		  case CMDVRFY:		/* vrfy -- verify address */
		  case CMDEXPN:		/* expn -- expand address */
			if (++nverifies >= MAXBADCOMMANDS)
			{
#ifdef LOG
				if (nverifies == MAXBADCOMMANDS &&
				    LogLevel > 5)
				{
					syslog(LOG_INFO, "%.100s: VRFY attack?",
					       CurSmtpClient);
				}
#endif
				sleep(1);
			}
			vrfy = c->cmdcode == CMDVRFY;
			if (bitset(vrfy ? PRIV_NOVRFY : PRIV_NOEXPN,
						PrivacyFlags))
			{
				if (vrfy)
					message("252 Cannot VRFY user; try RCPT to attempt delivery (or try finger)");
				else
					message("502 Sorry, we do not allow this operation");
#ifdef LOG
				if (LogLevel > 5)
					syslog(LOG_INFO, "%.100s: %s [rejected]",
						CurSmtpClient,
						shortenstring(inp, 203));
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
				syslog(LOG_INFO, "%.100s: %s",
					CurSmtpClient,
					shortenstring(inp, 203));
#endif
			vrfyqueue = NULL;
			QuickAbort = TRUE;
			if (vrfy)
				e->e_flags |= EF_VRFYONLY;
			while (*p != '\0' && isascii(*p) && isspace(*p))
				p++;
			if (*p == '\0')
			{
				message("501 Argument required");
				Errors++;
			}
			else
			{
				(void) sendtolist(p, NULLADDR, &vrfyqueue, 0, e);
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
				extern void printvrfyaddr __P((ADDRESS *, bool, bool));

				a = vrfyqueue;
				while ((a = a->q_next) != NULL &&
				       bitset(QDONTSEND|QBADADDR, a->q_flags))
					continue;
				if (!bitset(QDONTSEND|QBADADDR, vrfyqueue->q_flags))
					printvrfyaddr(vrfyqueue, a == NULL, vrfy);
				vrfyqueue = vrfyqueue->q_next;
			}
			if (InChild)
				finis();
			break;

		  case CMDETRN:		/* etrn -- force queue flush */
			if (strlen(p) <= 0)
			{
				message("500 Parameter required");
				break;
			}

			/* crude way to avoid denial-of-service attacks */
			if (n_etrn++ >= 3)
				sleep(3);
			id = p;
			if (*id == '@')
				id++;
			else
				*--id = '@';
#ifdef LOG
			if (LogLevel > 5)
				syslog(LOG_INFO, "%.100s: ETRN %s",
					CurSmtpClient,
					shortenstring(id, 203));
#endif
			QueueLimitRecipient = id;
			runqueue(TRUE);
			QueueLimitRecipient = NULL;
			message("250 Queuing for node %s started", p);
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
			/* arrange to ignore any current send list */
			e->e_sendqueue = NULL;

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

		  case CMDXUSR:		/* initial (user) submission */
			UserSubmission = TRUE;
			message("250 Initial submission");
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
				    "\"%s\" command from %.100s (%.100s)",
				    c->cmdname, CurSmtpClient,
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
			shortenstring(firstp, 203));
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
**  MAIL_ESMTP_ARGS -- process ESMTP arguments from MAIL line
**
**	Parameters:
**		kp -- the parameter key.
**		vp -- the value of that parameter.
**		e -- the envelope.
**
**	Returns:
**		none.
*/

void
mail_esmtp_args(kp, vp, e)
	char *kp;
	char *vp;
	ENVELOPE *e;
{
	if (strcasecmp(kp, "size") == 0)
	{
		if (vp == NULL)
		{
			usrerr("501 SIZE requires a value");
			/* NOTREACHED */
		}
# if defined(__STDC__) && !defined(BROKEN_ANSI_LIBRARY)
		e->e_msgsize = strtoul(vp, (char **) NULL, 10);
# else
		e->e_msgsize = strtol(vp, (char **) NULL, 10);
# endif
	}
	else if (strcasecmp(kp, "body") == 0)
	{
		if (vp == NULL)
		{
			usrerr("501 BODY requires a value");
			/* NOTREACHED */
		}
		else if (strcasecmp(vp, "8bitmime") == 0)
		{
			SevenBitInput = FALSE;
		}
		else if (strcasecmp(vp, "7bit") == 0)
		{
			SevenBitInput = TRUE;
		}
		else
		{
			usrerr("501 Unknown BODY type %s",
				vp);
			/* NOTREACHED */
		}
		e->e_bodytype = newstr(vp);
	}
	else if (strcasecmp(kp, "envid") == 0)
	{
		if (vp == NULL)
		{
			usrerr("501 ENVID requires a value");
			/* NOTREACHED */
		}
		if (!xtextok(vp))
		{
			usrerr("501 Syntax error in ENVID parameter value");
			/* NOTREACHED */
		}
		if (e->e_envid != NULL)
		{
			usrerr("501 Duplicate ENVID parameter");
			/* NOTREACHED */
		}
		e->e_envid = newstr(vp);
	}
	else if (strcasecmp(kp, "ret") == 0)
	{
		if (vp == NULL)
		{
			usrerr("501 RET requires a value");
			/* NOTREACHED */
		}
		if (bitset(EF_RET_PARAM, e->e_flags))
		{
			usrerr("501 Duplicate RET parameter");
			/* NOTREACHED */
		}
		e->e_flags |= EF_RET_PARAM;
		if (strcasecmp(vp, "hdrs") == 0)
			e->e_flags |= EF_NO_BODY_RETN;
		else if (strcasecmp(vp, "full") != 0)
		{
			usrerr("501 Bad argument \"%s\" to RET", vp);
			/* NOTREACHED */
		}
	}
	else
	{
		usrerr("501 %s parameter unrecognized", kp);
		/* NOTREACHED */
	}
}
/*
**  RCPT_ESMTP_ARGS -- process ESMTP arguments from RCPT line
**
**	Parameters:
**		a -- the address corresponding to the To: parameter.
**		kp -- the parameter key.
**		vp -- the value of that parameter.
**		e -- the envelope.
**
**	Returns:
**		none.
*/

void
rcpt_esmtp_args(a, kp, vp, e)
	ADDRESS *a;
	char *kp;
	char *vp;
	ENVELOPE *e;
{
	if (strcasecmp(kp, "notify") == 0)
	{
		char *p;

		if (vp == NULL)
		{
			usrerr("501 NOTIFY requires a value");
			/* NOTREACHED */
		}
		a->q_flags &= ~(QPINGONSUCCESS|QPINGONFAILURE|QPINGONDELAY);
		a->q_flags |= QHASNOTIFY;
		if (strcasecmp(vp, "never") == 0)
			return;
		for (p = vp; p != NULL; vp = p)
		{
			p = strchr(p, ',');
			if (p != NULL)
				*p++ = '\0';
			if (strcasecmp(vp, "success") == 0)
				a->q_flags |= QPINGONSUCCESS;
			else if (strcasecmp(vp, "failure") == 0)
				a->q_flags |= QPINGONFAILURE;
			else if (strcasecmp(vp, "delay") == 0)
				a->q_flags |= QPINGONDELAY;
			else
			{
				usrerr("501 Bad argument \"%s\"  to NOTIFY",
					vp);
				/* NOTREACHED */
			}
		}
	}
	else if (strcasecmp(kp, "orcpt") == 0)
	{
		if (vp == NULL)
		{
			usrerr("501 ORCPT requires a value");
			/* NOTREACHED */
		}
		if (strchr(vp, ';') == NULL || !xtextok(vp))
		{
			usrerr("501 Syntax error in ORCPT parameter value");
			/* NOTREACHED */
		}
		if (a->q_orcpt != NULL)
		{
			usrerr("501 Duplicate ORCPT parameter");
			/* NOTREACHED */
		}
		a->q_orcpt = newstr(vp);
	}
	else
	{
		usrerr("501 %s parameter unrecognized", kp);
		/* NOTREACHED */
	}
}
/*
**  PRINTVRFYADDR -- print an entry in the verify queue
**
**	Parameters:
**		a -- the address to print
**		last -- set if this is the last one.
**		vrfy -- set if this is a VRFY command.
**
**	Returns:
**		none.
**
**	Side Effects:
**		Prints the appropriate 250 codes.
*/

void
printvrfyaddr(a, last, vrfy)
	register ADDRESS *a;
	bool last;
	bool vrfy;
{
	char fmtbuf[20];

	if (vrfy && a->q_mailer != NULL &&
	    !bitnset(M_VRFY250, a->q_mailer->m_flags))
		strcpy(fmtbuf, "252");
	else
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

int
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
			syserr("451 %s: cannot fork", label);
			return (1);
		}
		if (childpid > 0)
		{
			auto int st;

			/* parent -- wait for child to complete */
			setproctitle("server %s child wait", CurSmtpClient);
			st = waitfor(childpid);
			if (st == -1)
				syserr("451 %s: lost child", label);
			else if (!WIFEXITED(st))
				syserr("451 %s: died on signal %d",
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

void
help(topic)
	char *topic;
{
	register FILE *hf;
	int len;
	bool noinfo;
	char buf[MAXLINE];
	extern char Version[];


	if (HelpFile == NULL || (hf = fopen(HelpFile, "r")) == NULL)
	{
		/* no help */
		errno = 0;
		message("502 Sendmail %s -- HELP not implemented", Version);
		return;
	}

	if (topic == NULL || *topic == '\0')
	{
		topic = "smtp";
		message("214-This is Sendmail version %s", Version);
		noinfo = FALSE;
	}
	else
	{
		makelower(topic);
		noinfo = TRUE;
	}

	len = strlen(topic);

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
		message("504 HELP topic \"%.10s\" unknown", topic);
	else
		message("214 End of HELP info");
	(void) fclose(hf);
}
