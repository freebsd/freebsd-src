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


#include <sendmail.h>

#ifndef lint
# if SMTP
static char id[] = "@(#)$Id: srvrsmtp.c,v 8.471.2.2.2.77 2001/05/27 22:20:30 gshapiro Exp $ (with SMTP)";
# else /* SMTP */
static char id[] = "@(#)$Id: srvrsmtp.c,v 8.471.2.2.2.77 2001/05/27 22:20:30 gshapiro Exp $ (without SMTP)";
# endif /* SMTP */
#endif /* ! lint */

#if SMTP
# if SASL || STARTTLS
#  include "sfsasl.h"
# endif /* SASL || STARTTLS */
# if SASL
#  define ENC64LEN(l)	(((l) + 2) * 4 / 3 + 1)
static int saslmechs __P((sasl_conn_t *, char **));
# endif /* SASL */
# if STARTTLS
#  include <sysexits.h>
#   include <openssl/err.h>
#   include <openssl/bio.h>
#   include <openssl/pem.h>
#   ifndef HASURANDOMDEV
#    include <openssl/rand.h>
#   endif /* !HASURANDOMDEV */

static SSL	*srv_ssl = NULL;
static SSL_CTX	*srv_ctx = NULL;
#  if !TLS_NO_RSA
static RSA	*rsa = NULL;
#  endif /* !TLS_NO_RSA */
static bool	tls_ok_srv = FALSE;
static int	tls_verify_cb __P((X509_STORE_CTX *));
#  if !TLS_NO_RSA
#   define RSA_KEYLENGTH	512
#  endif /* !TLS_NO_RSA */
# endif /* STARTTLS */

static time_t	checksmtpattack __P((volatile int *, int, bool,
				     char *, ENVELOPE *));
static void	mail_esmtp_args __P((char *, char *, ENVELOPE *));
static void	printvrfyaddr __P((ADDRESS *, bool, bool));
static void	rcpt_esmtp_args __P((ADDRESS *, char *, char *, ENVELOPE *));
static int	runinchild __P((char *, ENVELOPE *));
static char	*skipword __P((char *volatile, char *));
extern ENVELOPE	BlankEnvelope;

/*
**  SMTP -- run the SMTP protocol.
**
**	Parameters:
**		nullserver -- if non-NULL, rejection message for
**			all SMTP commands.
**		e -- the envelope.
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
	char	*cmd_name;	/* command name */
	int	cmd_code;	/* internal code, see below */
};

/* values for cmd_code */
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
# if SASL
#  define CMDAUTH	13	/* auth -- SASL authenticate */
# endif /* SASL */
# if STARTTLS
#  define CMDSTLS	14	/* STARTTLS -- start TLS session */
# endif /* STARTTLS */
/* non-standard commands */
# define CMDONEX	16	/* onex -- sending one transaction only */
# define CMDVERB	17	/* verb -- go into verbose mode */
# define CMDXUSR	18	/* xusr -- initial (user) submission */
/* unimplemented commands from RFC 821 */
# define CMDUNIMPL	19	/* unimplemented rfc821 commands */
/* use this to catch and log "door handle" attempts on your system */
# define CMDLOGBOGUS	23	/* bogus command that should be logged */
/* debugging-only commands, only enabled if SMTPDEBUG is defined */
# define CMDDBGQSHOW	24	/* showq -- show send queue */
# define CMDDBGDEBUG	25	/* debug -- set debug mode */

/*
**  Note: If you change this list,
**        remember to update 'helpfile'
*/

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
	{ "send",	CMDUNIMPL	},
	{ "saml",	CMDUNIMPL	},
	{ "soml",	CMDUNIMPL	},
	{ "turn",	CMDUNIMPL	},
# if SASL
	{ "auth",	CMDAUTH,	},
# endif /* SASL */
# if STARTTLS
	{ "starttls",	CMDSTLS,	},
# endif /* STARTTLS */
    /* remaining commands are here only to trap and log attempts to use them */
	{ "showq",	CMDDBGQSHOW	},
	{ "debug",	CMDDBGDEBUG	},
	{ "wiz",	CMDLOGBOGUS	},

	{ NULL,		CMDERROR	}
};

static bool	OneXact = FALSE;	/* one xaction only this run */
static char	*CurSmtpClient;		/* who's at the other end of channel */

# define MAXBADCOMMANDS		25	/* maximum number of bad commands */
# define MAXNOOPCOMMANDS	20	/* max "noise" commands before slowdown */
# define MAXHELOCOMMANDS	3	/* max HELO/EHLO commands before slowdown */
# define MAXVRFYCOMMANDS	6	/* max VRFY/EXPN commands before slowdown */
# define MAXETRNCOMMANDS	8	/* max ETRN commands before slowdown */
# define MAXTIMEOUT	(4 * 60)	/* max timeout for bad commands */

/* runinchild() returns */
# define RIC_INCHILD		0	/* in a child process */
# define RIC_INPARENT		1	/* still in parent process */
# define RIC_TEMPFAIL		2	/* temporary failure occurred */

void
smtp(nullserver, d_flags, e)
	char *volatile nullserver;
	BITMAP256 d_flags;
	register ENVELOPE *volatile e;
{
	register char *volatile p;
	register struct cmd *volatile c = NULL;
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
	int ric;
	bool doublequeue;
	volatile bool discard;
	volatile int badcommands = 0;	/* count of bad commands */
	volatile int nverifies = 0;	/* count of VRFY/EXPN commands */
	volatile int n_etrn = 0;	/* count of ETRN commands */
	volatile int n_noop = 0;	/* count of NOOP/VERB/ONEX etc cmds */
	volatile int n_helo = 0;	/* count of HELO/EHLO commands */
	volatile int delay = 1;		/* timeout for bad commands */
	bool ok;
	volatile bool tempfail = FALSE;
# if _FFR_MILTER
	volatile bool milterize = (nullserver == NULL);
# endif /* _FFR_MILTER */
	volatile time_t wt;		/* timeout after too many commands */
	volatile time_t previous;	/* time after checksmtpattack() */
	volatile bool lognullconnection = TRUE;
	register char *q;
	char *addr;
	char *greetcode = "220";
	QUEUE_CHAR *new;
	int argno;
	char *args[MAXSMTPARGS];
	char inp[MAXLINE];
	char cmdbuf[MAXLINE];
# if SASL
	sasl_conn_t *conn;
	volatile bool sasl_ok;
	volatile int n_auth = 0;	/* count of AUTH commands */
	bool ismore;
	int result;
	volatile int authenticating;
	char *hostname;
	char *user;
	char *in, *out, *out2;
	const char *errstr;
	int inlen, out2len;
	unsigned int outlen;
	char *volatile auth_type;
	char *mechlist;
	volatile int n_mechs;
	int len;
	sasl_security_properties_t ssp;
	sasl_external_properties_t ext_ssf;
#  if SFIO
	sasl_ssf_t *ssf;
#  endif /* SFIO */
# endif /* SASL */
# if STARTTLS
	int r;
	int rfd, wfd;
	volatile bool usetls = TRUE;
	volatile bool tls_active = FALSE;
	bool saveQuickAbort;
	bool saveSuprErrs;
# endif /* STARTTLS */

	if (fileno(OutChannel) != fileno(stdout))
	{
		/* arrange for debugging output to go to remote host */
		(void) dup2(fileno(OutChannel), fileno(stdout));
	}

	settime(e);
	(void)sm_getla(e);
	peerhostname = RealHostName;
	if (peerhostname == NULL)
		peerhostname = "localhost";
	CurHostName = peerhostname;
	CurSmtpClient = macvalue('_', e);
	if (CurSmtpClient == NULL)
		CurSmtpClient = CurHostName;

	/* check_relay may have set discard bit, save for later */
	discard = bitset(EF_DISCARD, e->e_flags);

	sm_setproctitle(TRUE, e, "server %s startup", CurSmtpClient);

# if SASL
	sasl_ok = FALSE;	/* SASL can't be used (yet) */
	n_mechs = 0;

	/* SASL server new connection */
	hostname = macvalue('j', e);
#  if SASL > 10505
	/* use empty realm: only works in SASL > 1.5.5 */
	result = sasl_server_new("smtp", hostname, "", NULL, 0, &conn);
#  else /* SASL > 10505 */
	/* use no realm -> realm is set to hostname by SASL lib */
	result = sasl_server_new("smtp", hostname, NULL, NULL, 0, &conn);
#  endif /* SASL > 10505 */
	if (result == SASL_OK)
	{
		sasl_ok = TRUE;

		/*
		**  SASL set properties for sasl
		**  set local/remote IP
		**  XXX only IPv4: Cyrus SASL doesn't support anything else
		**
		**  XXX where exactly are these used/required?
		**  Kerberos_v4
		*/

# if NETINET
		in = macvalue(macid("{daemon_family}", NULL), e);
		if (in != NULL && strcmp(in, "inet") == 0)
		{
			SOCKADDR_LEN_T addrsize;
			struct sockaddr_in saddr_l;
			struct sockaddr_in saddr_r;

			addrsize = sizeof(struct sockaddr_in);
			if (getpeername(fileno(InChannel),
					(struct sockaddr *)&saddr_r,
					&addrsize) == 0)
			{
				sasl_setprop(conn, SASL_IP_REMOTE, &saddr_r);
				addrsize = sizeof(struct sockaddr_in);
				if (getsockname(fileno(InChannel),
						(struct sockaddr *)&saddr_l,
						&addrsize) == 0)
					sasl_setprop(conn, SASL_IP_LOCAL,
						     &saddr_l);
			}
		}
# endif /* NETINET */

		authenticating = SASL_NOT_AUTH;
		auth_type = NULL;
		mechlist = NULL;
		user = NULL;
#  if 0
		define(macid("{auth_author}", NULL), NULL, &BlankEnvelope);
#  endif /* 0 */

		/* set properties */
		(void) memset(&ssp, '\0', sizeof ssp);
#  if SFIO
		/* XXX should these be options settable via .cf ? */
		/* ssp.min_ssf = 0; is default due to memset() */
		{
			ssp.max_ssf = INT_MAX;
			ssp.maxbufsize = MAXOUTLEN;
		}
#  endif /* SFIO */
#  if _FFR_SASL_OPTS
		ssp.security_flags = SASLOpts & SASL_SEC_MASK;
#  endif /* _FFR_SASL_OPTS */
		sasl_ok = sasl_setprop(conn, SASL_SEC_PROPS, &ssp) == SASL_OK;

		if (sasl_ok)
		{
			/*
			**  external security strength factor;
			**  we have none so zero
#   if STARTTLS
			**  we may have to change this for STARTTLS
			**  (dynamically)
#   endif
			*/
			ext_ssf.ssf = 0;
			ext_ssf.auth_id = NULL;
			sasl_ok = sasl_setprop(conn, SASL_SSF_EXTERNAL,
					       &ext_ssf) == SASL_OK;
		}
		if (sasl_ok)
		{
			n_mechs = saslmechs(conn, &mechlist);
			sasl_ok = n_mechs > 0;
		}
	}
	else
	{
		if (LogLevel > 9)
			sm_syslog(LOG_WARNING, NOQID,
				  "SASL error: sasl_server_new failed=%d",
				  result);
	}
# endif /* SASL */

# if STARTTLS
#  if _FFR_TLS_O_T
	saveQuickAbort = QuickAbort;
	saveSuprErrs = SuprErrs;
	SuprErrs = TRUE;
	QuickAbort = FALSE;
	if (rscheck("offer_tls", CurSmtpClient, "", e, TRUE, FALSE, 8,
		    NULL) != EX_OK || Errors > 0)
		usetls = FALSE;
	QuickAbort = saveQuickAbort;
	SuprErrs = saveSuprErrs;
#  endif /* _FFR_TLS_O_T */
# endif /* STARTTLS */

# if _FFR_MILTER
	if (milterize)
	{
		char state;

		/* initialize mail filter connection */
		milter_init(e, &state);
		switch (state)
		{
		  case SMFIR_REJECT:
			greetcode = "554";
			nullserver = "Command rejected";
			milterize = FALSE;
			break;

		  case SMFIR_TEMPFAIL:
			tempfail = TRUE;
			milterize = FALSE;
			break;
		}
	}

	if (milterize && !bitset(EF_DISCARD, e->e_flags))
	{
		char state;

		(void) milter_connect(peerhostname, RealHostAddr,
				      e, &state);
		switch (state)
		{
		  case SMFIR_REPLYCODE:	/* REPLYCODE shouldn't happen */
		  case SMFIR_REJECT:
			greetcode = "554";
			nullserver = "Command rejected";
			milterize = FALSE;
			break;

		  case SMFIR_TEMPFAIL:
			tempfail = TRUE;
			milterize = FALSE;
			break;
		}
	}
# endif /* _FFR_MILTER */

	/* output the first line, inserting "ESMTP" as second word */
	expand(SmtpGreeting, inp, sizeof inp, e);
	p = strchr(inp, '\n');
	if (p != NULL)
		*p++ = '\0';
	id = strchr(inp, ' ');
	if (id == NULL)
		id = &inp[strlen(inp)];
	if (p == NULL)
		snprintf(cmdbuf, sizeof cmdbuf,
			 "%s %%.*s ESMTP%%s", greetcode);
	else
		snprintf(cmdbuf, sizeof cmdbuf,
			 "%s-%%.*s ESMTP%%s", greetcode);
	message(cmdbuf, (int) (id - inp), inp, id);

	/* output remaining lines */
	while ((id = p) != NULL && (p = strchr(id, '\n')) != NULL)
	{
		*p++ = '\0';
		if (isascii(*id) && isspace(*id))
			id++;
		(void) snprintf(cmdbuf, sizeof cmdbuf, "%s-%%s", greetcode);
		message(cmdbuf, id);
	}
	if (id != NULL)
	{
		if (isascii(*id) && isspace(*id))
			id++;
		(void) snprintf(cmdbuf, sizeof cmdbuf, "%s %%s", greetcode);
		message(cmdbuf, id);
	}

	protocol = NULL;
	sendinghost = macvalue('s', e);
	gothello = FALSE;
	gotmail = FALSE;
	for (;;)
	{
		/* arrange for backout */
		(void) setjmp(TopFrame);
		QuickAbort = FALSE;
		HoldErrs = FALSE;
		SuprErrs = FALSE;
		LogUsrErrs = FALSE;
		OnlyOneError = TRUE;
		e->e_flags &= ~(EF_VRFYONLY|EF_GLOBALERRS);

		/* setup for the read */
		e->e_to = NULL;
		Errors = 0;
		FileName = NULL;
		(void) fflush(stdout);

		/* read the input line */
		SmtpPhase = "server cmd read";
		sm_setproctitle(TRUE, e, "server %s cmd read", CurSmtpClient);
# if SASL
		/*
		**  SMTP AUTH requires accepting any length,
		**  at least for challenge/response
		**  XXX
		*/
# endif /* SASL */

		/* handle errors */
		if (ferror(OutChannel) ||
		    (p = sfgets(inp, sizeof inp, InChannel,
				TimeOuts.to_nextcommand, SmtpPhase)) == NULL)
		{
			char *d;

			d = macvalue(macid("{daemon_name}", NULL), e);
			if (d == NULL)
				d = "stdin";
			/* end of file, just die */
			disconnect(1, e);

# if _FFR_MILTER
			/* close out milter filters */
			milter_quit(e);
# endif /* _FFR_MILTER */

			message("421 4.4.1 %s Lost input channel from %s",
				MyHostName, CurSmtpClient);
			if (LogLevel > (gotmail ? 1 : 19))
				sm_syslog(LOG_NOTICE, e->e_id,
					  "lost input channel from %.100s to %s after %s",
					  CurSmtpClient, d,
					  (c == NULL || c->cmd_name == NULL) ? "startup" : c->cmd_name);
			/*
			**  If have not accepted mail (DATA), do not bounce
			**  bad addresses back to sender.
			*/

			if (bitset(EF_CLRQUEUE, e->e_flags))
				e->e_sendqueue = NULL;
			goto doquit;
		}

		/* clean up end of line */
		fixcrlf(inp, TRUE);

# if SASL
		if (authenticating == SASL_PROC_AUTH)
		{
#  if 0
			if (*inp == '\0')
			{
				authenticating = SASL_NOT_AUTH;
				message("501 5.5.2 missing input");
				continue;
			}
#  endif /* 0 */
			if (*inp == '*' && *(inp + 1) == '\0')
			{
				authenticating = SASL_NOT_AUTH;

				/* rfc 2254 4. */
				message("501 5.0.0 AUTH aborted");
				continue;
			}

			/* could this be shorter? XXX */
			out = xalloc(strlen(inp));
			result = sasl_decode64(inp, strlen(inp), out, &outlen);
			if (result != SASL_OK)
			{
				authenticating = SASL_NOT_AUTH;

				/* rfc 2254 4. */
				message("501 5.5.4 cannot decode AUTH parameter %s",
					inp);
				continue;
			}

			result = sasl_server_step(conn,	out, outlen,
						  &out, &outlen, &errstr);

			/* get an OK if we're done */
			if (result == SASL_OK)
			{
  authenticated:
				message("235 2.0.0 OK Authenticated");
				authenticating = SASL_IS_AUTH;
				define(macid("{auth_type}", NULL),
				       newstr(auth_type), &BlankEnvelope);

				result = sasl_getprop(conn, SASL_USERNAME,
						      (void **)&user);
				if (result != SASL_OK)
				{
					user = "";
					define(macid("{auth_authen}", NULL),
					       NULL, &BlankEnvelope);
				}
				else
				{
					define(macid("{auth_authen}", NULL),
					       newstr(user), &BlankEnvelope);
				}

#  if 0
				/* get realm? */
				sasl_getprop(conn, SASL_REALM, (void **) &data);
#  endif /* 0 */


#  if SFIO
				/* get security strength (features) */
				result = sasl_getprop(conn, SASL_SSF,
						      (void **) &ssf);
				if (result != SASL_OK)
				{
					define(macid("{auth_ssf}", NULL),
					       "0", &BlankEnvelope);
					ssf = NULL;
				}
				else
				{
					char pbuf[8];

					snprintf(pbuf, sizeof pbuf, "%u", *ssf);
					define(macid("{auth_ssf}", NULL),
					       newstr(pbuf), &BlankEnvelope);
					if (tTd(95, 8))
						dprintf("SASL auth_ssf: %u\n",
							*ssf);
				}
				/*
				**  only switch to encrypted connection
				**  if a security layer has been negotiated
				*/
				if (ssf != NULL && *ssf > 0)
				{
					/*
					**  convert sfio stuff to use SASL
					**  check return values
					**  if the call fails,
					**  fall back to unencrypted version
					**  unless some cf option requires
					**  encryption then the connection must
					**  be aborted
					*/
					if (sfdcsasl(InChannel, OutChannel,
					    conn) == 0)
					{
						/* restart dialogue */
						gothello = FALSE;
						OneXact = TRUE;
						n_helo = 0;
					}
					else
						syserr("503 5.3.3 SASL TLS failed");
					if (LogLevel > 9)
						sm_syslog(LOG_INFO,
							  NOQID,
							  "SASL: connection from %.64s: mech=%.16s, id=%.64s, bits=%d",
							  CurSmtpClient,
							  auth_type, user,
							  *ssf);
				}
#  else /* SFIO */
				if (LogLevel > 9)
					sm_syslog(LOG_INFO, NOQID,
						  "SASL: connection from %.64s: mech=%.16s, id=%.64s",
						  CurSmtpClient, auth_type,
						  user);
#  endif /* SFIO */
			}
			else if (result == SASL_CONTINUE)
			{
				len = ENC64LEN(outlen);
				out2 = xalloc(len);
				result = sasl_encode64(out, outlen, out2, len,
						       (u_int *)&out2len);
				if (result != SASL_OK)
				{
					/* correct code? XXX */
					/* 454 Temp. authentication failure */
					message("454 4.5.4 Internal error: unable to encode64");
					if (LogLevel > 5)
						sm_syslog(LOG_WARNING, e->e_id,
							  "SASL encode64 error [%d for \"%s\"]",
							  result, out);
					/* start over? */
					authenticating = SASL_NOT_AUTH;
				}
				else
				{
					message("334 %s", out2);
					if (tTd(95, 2))
						dprintf("SASL continue: msg='%s' len=%d\n",
							out2, out2len);
				}
			}
			else
			{
				/* not SASL_OK or SASL_CONT */
				message("500 5.7.0 authentication failed");
				if (LogLevel > 9)
					sm_syslog(LOG_WARNING, e->e_id,
						  "AUTH failure (%s): %s (%d)",
						  auth_type,
						  sasl_errstring(result, NULL,
								 NULL),
						  result);
				authenticating = SASL_NOT_AUTH;
			}
		}
		else
		{
			/* don't want to do any of this if authenticating */
# endif /* SASL */

		/* echo command to transcript */
		if (e->e_xfp != NULL)
			fprintf(e->e_xfp, "<<< %s\n", inp);

		if (LogLevel >= 15)
			sm_syslog(LOG_INFO, e->e_id,
				  "<-- %s",
				  inp);

		if (e->e_id == NULL)
			sm_setproctitle(TRUE, e, "%s: %.80s",
					CurSmtpClient, inp);
		else
			sm_setproctitle(TRUE, e, "%s %s: %.80s",
					qid_printname(e),
					CurSmtpClient, inp);

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
		for (c = CmdTab; c->cmd_name != NULL; c++)
		{
			if (strcasecmp(c->cmd_name, cmdbuf) == 0)
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

		if (nullserver != NULL || bitnset(D_ETRNONLY, d_flags))
		{
			switch (c->cmd_code)
			{
			  case CMDQUIT:
			  case CMDHELO:
			  case CMDEHLO:
			  case CMDNOOP:
			  case CMDRSET:
				/* process normally */
				break;

			  case CMDETRN:
				if (bitnset(D_ETRNONLY, d_flags) &&
				    nullserver == NULL)
					break;
				continue;

			  default:
				if (++badcommands > MAXBADCOMMANDS)
				{
					delay *= 2;
					if (delay >= MAXTIMEOUT)
						delay = MAXTIMEOUT;
					(void) sleep(delay);
				}
				if (nullserver != NULL)
				{
					if (ISSMTPREPLY(nullserver))
						usrerr(nullserver);
					else
						usrerr("550 5.0.0 %s", nullserver);
				}
				else
					usrerr("452 4.4.5 Insufficient disk space; try again later");
				continue;
			}
		}

		/* non-null server */
		switch (c->cmd_code)
		{
		  case CMDMAIL:
		  case CMDEXPN:
		  case CMDVRFY:
		  case CMDETRN:
			lognullconnection = FALSE;
		}

		switch (c->cmd_code)
		{
# if SASL
		  case CMDAUTH: /* sasl */
			if (!sasl_ok)
			{
				message("503 5.3.3 AUTH not available");
				break;
			}
			if (authenticating == SASL_IS_AUTH)
			{
				message("503 5.5.0 Already Authenticated");
				break;
			}
			if (gotmail)
			{
				message("503 5.5.0 AUTH not permitted during a mail transaction");
				break;
			}
			if (tempfail)
			{
				if (LogLevel > 9)
					sm_syslog(LOG_INFO, e->e_id,
						  "SMTP AUTH command (%.100s) from %.100s tempfailed (due to previous checks)",
						  p, CurSmtpClient);
				usrerr("454 4.7.1 Please try again later");
				break;
			}

			ismore = FALSE;

			/* crude way to avoid crack attempts */
			(void) checksmtpattack(&n_auth, n_mechs + 1, TRUE,
					       "AUTH", e);

			/* make sure it's a valid string */
			for (q = p; *q != '\0' && isascii(*q); q++)
			{
				if (isspace(*q))
				{
					*q = '\0';
					while (*++q != '\0' &&
					       isascii(*q) && isspace(*q))
						continue;
					*(q - 1) = '\0';
					ismore = (*q != '\0');
					break;
				}
			}

			/* check whether mechanism is available */
			if (iteminlist(p, mechlist, " ") == NULL)
			{
				message("503 5.3.3 AUTH mechanism %s not available",
					p);
				break;
			}

			if (ismore)
			{
				/* could this be shorter? XXX */
				in = xalloc(strlen(q));
				result = sasl_decode64(q, strlen(q), in,
						       (u_int *)&inlen);
				if (result != SASL_OK)
				{
					message("501 5.5.4 cannot BASE64 decode '%s'",
						q);
					if (LogLevel > 5)
						sm_syslog(LOG_WARNING, e->e_id,
							  "SASL decode64 error [%d for \"%s\"]",
							  result, q);
					/* start over? */
					authenticating = SASL_NOT_AUTH;
					in = NULL;
					inlen = 0;
					break;
				}
#  if 0
				if (tTd(95, 99))
				{
					int i;

					dprintf("AUTH: more \"");
					for (i = 0; i < inlen; i++)
					{
						if (isascii(in[i]) &&
						    isprint(in[i]))
							dprintf("%c", in[i]);
						else
							dprintf("_");
					}
					dprintf("\"\n");
				}
#  endif /* 0 */
			}
			else
			{
				in = NULL;
				inlen = 0;
			}

			/* see if that auth type exists */
			result = sasl_server_start(conn, p, in, inlen,
						   &out, &outlen, &errstr);

			if (result != SASL_OK && result != SASL_CONTINUE)
			{
				message("500 5.7.0 authentication failed");
				if (LogLevel > 9)
					sm_syslog(LOG_ERR, e->e_id,
						  "AUTH failure (%s): %s (%d)",
						  p,
						  sasl_errstring(result, NULL,
								 NULL),
						  result);
				break;
			}
			auth_type = newstr(p);

			if (result == SASL_OK)
			{
				/* ugly, but same code */
				goto authenticated;
				/* authenticated by the initial response */
			}

			/* len is at least 2 */
			len = ENC64LEN(outlen);
			out2 = xalloc(len);
			result = sasl_encode64(out, outlen, out2, len,
					       (u_int *)&out2len);

			if (result != SASL_OK)
			{
				message("454 4.5.4 Temporary authentication failure");
				if (LogLevel > 5)
					sm_syslog(LOG_WARNING, e->e_id,
						  "SASL encode64 error [%d for \"%s\"]",
						  result, out);

				/* start over? */
				authenticating = SASL_NOT_AUTH;
			}
			else
			{
				message("334 %s", out2);
				authenticating = SASL_PROC_AUTH;
			}

			break;
# endif /* SASL */

# if STARTTLS
		  case CMDSTLS: /* starttls */
			if (*p != '\0')
			{
				message("501 5.5.2 Syntax error (no parameters allowed)");
				break;
			}
			if (!usetls)
			{
				message("503 5.5.0 TLS not available");
				break;
			}
			if (!tls_ok_srv)
			{
				message("454 4.3.3 TLS not available after start");
				break;
			}
			if (gotmail)
			{
				message("503 5.5.0 TLS not permitted during a mail transaction");
				break;
			}
			if (tempfail)
			{
				if (LogLevel > 9)
					sm_syslog(LOG_INFO, e->e_id,
						  "SMTP STARTTLS command (%.100s) from %.100s tempfailed (due to previous checks)",
						  p, CurSmtpClient);
				usrerr("454 4.7.1 Please try again later");
				break;
			}
# if TLS_NO_RSA
			/*
			**  XXX do we need a temp key ?
			*/
# else /* TLS_NO_RSA */
			if (SSL_CTX_need_tmp_RSA(srv_ctx) &&
			   !SSL_CTX_set_tmp_rsa(srv_ctx,
				(rsa = RSA_generate_key(RSA_KEYLENGTH, RSA_F4,
							NULL, NULL)))
			  )
			{
				message("454 4.3.3 TLS not available: error generating RSA temp key");
				if (rsa != NULL)
					RSA_free(rsa);
				break;
			}
# endif /* TLS_NO_RSA */
			if (srv_ssl != NULL)
				SSL_clear(srv_ssl);
			else if ((srv_ssl = SSL_new(srv_ctx)) == NULL)
			{
				message("454 4.3.3 TLS not available: error generating SSL handle");
				break;
			}
			rfd = fileno(InChannel);
			wfd = fileno(OutChannel);
			if (rfd < 0 || wfd < 0 ||
			    SSL_set_rfd(srv_ssl, rfd) <= 0 ||
			    SSL_set_wfd(srv_ssl, wfd) <= 0)
			{
				message("454 4.3.3 TLS not available: error set fd");
				SSL_free(srv_ssl);
				srv_ssl = NULL;
				break;
			}
			message("220 2.0.0 Ready to start TLS");
			SSL_set_accept_state(srv_ssl);

#  define SSL_ACC(s)	SSL_accept(s)
			if ((r = SSL_ACC(srv_ssl)) <= 0)
			{
				int i;

				/* what to do in this case? */
				i = SSL_get_error(srv_ssl, r);
				if (LogLevel > 5)
				{
					sm_syslog(LOG_WARNING, e->e_id,
						  "TLS: error: accept failed=%d (%d)",
						  r, i);
					if (LogLevel > 9)
						tlslogerr();
				}
				tls_ok_srv = FALSE;
				SSL_free(srv_ssl);
				srv_ssl = NULL;

				/*
				**  according to the next draft of
				**  RFC 2487 the connection should be dropped
				*/

				/* arrange to ignore any current send list */
				e->e_sendqueue = NULL;
				goto doquit;
			}

			/* ignore return code for now, it's in {verify} */
			(void) tls_get_info(srv_ssl, &BlankEnvelope, TRUE,
					    CurSmtpClient, TRUE);

			/*
			**  call Stls_client to find out whether
			**  to accept the connection from the client
			*/

			saveQuickAbort = QuickAbort;
			saveSuprErrs = SuprErrs;
			SuprErrs = TRUE;
			QuickAbort = FALSE;
			if (rscheck("tls_client",
				     macvalue(macid("{verify}", NULL), e),
				     "STARTTLS", e, TRUE, TRUE, 6, NULL) !=
			    EX_OK || Errors > 0)
			{
				extern char MsgBuf[];

				if (MsgBuf[0] != '\0' && ISSMTPREPLY(MsgBuf))
					nullserver = newstr(MsgBuf);
				else
					nullserver = "503 5.7.0 Authentication required.";
			}
			QuickAbort = saveQuickAbort;
			SuprErrs = saveSuprErrs;

			tls_ok_srv = FALSE;	/* don't offer STARTTLS again */
			gothello = FALSE;	/* discard info */
			n_helo = 0;
			OneXact = TRUE;	/* only one xaction this run */
#  if SASL
			if (sasl_ok)
			{
				char *s;

				if ((s = macvalue(macid("{cipher_bits}", NULL), e)) != NULL &&
				    (ext_ssf.ssf = atoi(s)) > 0)
				{
#  if _FFR_EXT_MECH
					ext_ssf.auth_id = macvalue(macid("{cert_subject}",
									 NULL),
								   e);
#  endif /* _FFR_EXT_MECH */
					sasl_ok = sasl_setprop(conn, SASL_SSF_EXTERNAL,
							       &ext_ssf) == SASL_OK;
					if (mechlist != NULL)
						sm_free(mechlist);
					mechlist = NULL;
					if (sasl_ok)
					{
						n_mechs = saslmechs(conn,
								    &mechlist);
						sasl_ok = n_mechs > 0;
					}
				}
			}
#  endif /* SASL */

			/* switch to secure connection */
#if SFIO
			r = sfdctls(InChannel, OutChannel, srv_ssl);
#else /* SFIO */
# if _FFR_TLS_TOREK
			r = sfdctls(&InChannel, &OutChannel, srv_ssl);
# endif /* _FFR_TLS_TOREK */
#endif /* SFIO */
			if (r == 0)
				tls_active = TRUE;
			else
			{
				/*
				**  XXX this is an internal error
				**  how to deal with it?
				**  we can't generate an error message
				**  since the other side switched to an
				**  encrypted layer, but we could not...
				**  just "hang up"?
				*/
				nullserver = "454 4.3.3 TLS not available: can't switch to encrypted layer";
				syserr("TLS: can't switch to encrypted layer");
			}
			break;
# endif /* STARTTLS */

		  case CMDHELO:		/* hello -- introduce yourself */
		  case CMDEHLO:		/* extended hello */
			if (c->cmd_code == CMDEHLO)
			{
				protocol = "ESMTP";
				SmtpPhase = "server EHLO";
			}
			else
			{
				protocol = "SMTP";
				SmtpPhase = "server HELO";
			}

			/* avoid denial-of-service */
			(void) checksmtpattack(&n_helo, MAXHELOCOMMANDS, TRUE,
					       "HELO/EHLO", e);

			/* check for duplicate HELO/EHLO per RFC 1651 4.2 */
			if (gothello)
			{
				usrerr("503 %s Duplicate HELO/EHLO",
				       MyHostName);
				break;
			}

			/* check for valid domain name (re 1123 5.2.5) */
			if (*p == '\0' && !AllowBogusHELO)
			{
				usrerr("501 %s requires domain address",
					cmdbuf);
				break;
			}

			/* check for long domain name (hides Received: info) */
			if (strlen(p) > MAXNAME)
			{
				usrerr("501 Invalid domain name");
				if (LogLevel > 9)
					sm_syslog(LOG_INFO, CurEnv->e_id,
						  "invalid domain name (too long) from %.100s",
						  CurSmtpClient);
				break;
			}

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

			if (*q == '\0')
			{
				q = "pleased to meet you";
				sendinghost = newstr(p);
			}
			else if (!AllowBogusHELO)
			{
				usrerr("501 Invalid domain name");
				if (LogLevel > 9)
					sm_syslog(LOG_INFO, CurEnv->e_id,
						  "invalid domain name (%.100s) from %.100s",
						  p, CurSmtpClient);
				break;
			}
			else
			{
				q = "accepting invalid domain name";
			}

			gothello = TRUE;

# if _FFR_MILTER
			if (milterize && !bitset(EF_DISCARD, e->e_flags))
			{
				char state;
				char *response;

				response = milter_helo(p, e, &state);
				switch (state)
				{
				  case SMFIR_REPLYCODE:
					nullserver = response;
					milterize = FALSE;
					break;

				  case SMFIR_REJECT:
					nullserver = "Command rejected";
					milterize = FALSE;
					break;

				  case SMFIR_TEMPFAIL:
					tempfail = TRUE;
					milterize = FALSE;
					break;
				}
			}
# endif /* _FFR_MILTER */

			/* print HELO response message */
			if (c->cmd_code != CMDEHLO)
			{
				message("250 %s Hello %s, %s",
					MyHostName, CurSmtpClient, q);
				break;
			}

			message("250-%s Hello %s, %s",
				MyHostName, CurSmtpClient, q);

			/* offer ENHSC even for nullserver */
			if (nullserver != NULL)
			{
				message("250 ENHANCEDSTATUSCODES");
				break;
			}

			/*
			**  print EHLO features list
			**
			**  Note: If you change this list,
			**        remember to update 'helpfile'
			*/


			message("250-ENHANCEDSTATUSCODES");
			if (!bitset(PRIV_NOEXPN, PrivacyFlags))
			{
				message("250-EXPN");
				if (!bitset(PRIV_NOVERB, PrivacyFlags))
					message("250-VERB");
			}
# if MIME8TO7
			message("250-8BITMIME");
# endif /* MIME8TO7 */
			if (MaxMessageSize > 0)
				message("250-SIZE %ld", MaxMessageSize);
			else
				message("250-SIZE");
# if DSN
			if (SendMIMEErrors &&
			    !bitset(PRIV_NORECEIPTS, PrivacyFlags))
				message("250-DSN");
# endif /* DSN */
			message("250-ONEX");
			if (!bitset(PRIV_NOETRN, PrivacyFlags) &&
			    !bitnset(D_NOETRN, d_flags))
				message("250-ETRN");
			message("250-XUSR");

# if SASL
			if (sasl_ok && mechlist != NULL && *mechlist != '\0')
				message("250-AUTH %s", mechlist);
# endif /* SASL */
# if STARTTLS
			if (tls_ok_srv && usetls)
				message("250-STARTTLS");
# endif /* STARTTLS */
			message("250 HELP");
			break;

		  case CMDMAIL:		/* mail -- designate sender */
			SmtpPhase = "server MAIL";

			/* check for validity of this command */
			if (!gothello && bitset(PRIV_NEEDMAILHELO, PrivacyFlags))
			{
				usrerr("503 5.0.0 Polite people say HELO first");
				break;
			}
			if (gotmail)
			{
				usrerr("503 5.5.0 Sender already specified");
				break;
			}
			if (InChild)
			{
				errno = 0;
				syserr("503 5.5.0 Nested MAIL command: MAIL %s", p);
				finis(TRUE, ExitStat);
			}
# if SASL
			if (bitnset(D_AUTHREQ, d_flags) &&
			    authenticating != SASL_IS_AUTH)
			{
				usrerr("530 5.7.0 Authentication required");
				break;
			}
# endif /* SASL */

			p = skipword(p, "from");
			if (p == NULL)
				break;
			if (tempfail)
			{
				if (LogLevel > 9)
					sm_syslog(LOG_INFO, e->e_id,
						  "SMTP MAIL command (%.100s) from %.100s tempfailed (due to previous checks)",
						  p, CurSmtpClient);
				usrerr("451 4.7.1 Please try again later");
				break;
			}

			/* make sure we know who the sending host is */
			if (sendinghost == NULL)
				sendinghost = peerhostname;


			/* fork a subprocess to process this command */
			ric = runinchild("SMTP-MAIL", e);

			/* Catch a problem and stop processing */
			if (ric == RIC_TEMPFAIL && nullserver == NULL)
				nullserver = "452 4.3.0 Internal software error";
			if (ric != RIC_INCHILD)
				break;

			if (Errors > 0)
				goto undo_subproc_no_pm;
			if (!gothello)
			{
				auth_warning(e,
					"%s didn't use HELO protocol",
					CurSmtpClient);
			}
# ifdef PICKY_HELO_CHECK
			if (strcasecmp(sendinghost, peerhostname) != 0 &&
			    (strcasecmp(peerhostname, "localhost") != 0 ||
			     strcasecmp(sendinghost, MyHostName) != 0))
			{
				auth_warning(e, "Host %s claimed to be %s",
					CurSmtpClient, sendinghost);
			}
# endif /* PICKY_HELO_CHECK */

			if (protocol == NULL)
				protocol = "SMTP";
			define('r', protocol, e);
			define('s', sendinghost, e);

			if (Errors > 0)
				goto undo_subproc_no_pm;
			nrcpts = 0;
			define(macid("{ntries}", NULL), "0", e);
			e->e_flags |= EF_CLRQUEUE;
			sm_setproctitle(TRUE, e, "%s %s: %.80s",
					qid_printname(e),
					CurSmtpClient, inp);

			/* child -- go do the processing */
			if (setjmp(TopFrame) > 0)
			{
				/* this failed -- undo work */
 undo_subproc_no_pm:
				e->e_flags &= ~EF_PM_NOTIFY;
 undo_subproc:
				if (InChild)
				{
					QuickAbort = FALSE;
					SuprErrs = TRUE;
					e->e_flags &= ~EF_FATALERRS;

					if (LogLevel > 4 &&
					    bitset(EF_LOGSENDER, e->e_flags))
						logsender(e, NULL);
					e->e_flags &= ~EF_LOGSENDER;

					finis(TRUE, ExitStat);
				}
				break;
			}
			QuickAbort = TRUE;

			/* must parse sender first */
			delimptr = NULL;
			setsender(p, e, &delimptr, ' ', FALSE);
			if (delimptr != NULL && *delimptr != '\0')
				*delimptr++ = '\0';
			if (Errors > 0)
				goto undo_subproc_no_pm;

			/* Successfully set e_from, allow logging */
			e->e_flags |= EF_LOGSENDER;

			/* put resulting triple from parseaddr() into macros */
			if (e->e_from.q_mailer != NULL)
				 define(macid("{mail_mailer}", NULL),
					e->e_from.q_mailer->m_name, e);
			else
				 define(macid("{mail_mailer}", NULL),
					NULL, e);
			if (e->e_from.q_host != NULL)
				define(macid("{mail_host}", NULL),
				       e->e_from.q_host, e);
			else
				define(macid("{mail_host}", NULL),
				       "localhost", e);
			if (e->e_from.q_user != NULL)
				define(macid("{mail_addr}", NULL),
				       e->e_from.q_user, e);
			else
				define(macid("{mail_addr}", NULL),
				       NULL, e);
			if (Errors > 0)
			  goto undo_subproc_no_pm;

			/* check for possible spoofing */
			if (RealUid != 0 && OpMode == MD_SMTP &&
			    !wordinclass(RealUserName, 't') &&
			    (!bitnset(M_LOCALMAILER,
				      e->e_from.q_mailer->m_flags) ||
			     strcmp(e->e_from.q_user, RealUserName) != 0))
			{
				auth_warning(e, "%s owned process doing -bs",
					RealUserName);
			}

			/* now parse ESMTP arguments */
			e->e_msgsize = 0;
			addr = p;
			argno = 0;
			args[argno++] = p;
			p = delimptr;
			while (p != NULL && *p != '\0')
			{
				char *kp;
				char *vp = NULL;
				char *equal = NULL;

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
					equal = p;
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
					dprintf("MAIL: got arg %s=\"%s\"\n", kp,
						vp == NULL ? "<null>" : vp);

				mail_esmtp_args(kp, vp, e);
				if (equal != NULL)
					*equal = '=';
				args[argno++] = kp;
				if (argno >= MAXSMTPARGS - 1)
					usrerr("501 5.5.4 Too many parameters");
				if (Errors > 0)
					goto undo_subproc_no_pm;
			}
			args[argno] = NULL;
			if (Errors > 0)
				goto undo_subproc_no_pm;

			/* do config file checking of the sender */
			if (rscheck("check_mail", addr,
				    NULL, e, TRUE, TRUE, 4, NULL) != EX_OK ||
			    Errors > 0)
				goto undo_subproc_no_pm;

			if (MaxMessageSize > 0 &&
			    (e->e_msgsize > MaxMessageSize ||
			     e->e_msgsize < 0))
			{
				usrerr("552 5.2.3 Message size exceeds fixed maximum message size (%ld)",
					MaxMessageSize);
				goto undo_subproc_no_pm;
			}

			if (!enoughdiskspace(e->e_msgsize, TRUE))
			{
				usrerr("452 4.4.5 Insufficient disk space; try again later");
				goto undo_subproc_no_pm;
			}
			if (Errors > 0)
				goto undo_subproc_no_pm;

# if _FFR_MILTER
			LogUsrErrs = TRUE;
			if (milterize && !bitset(EF_DISCARD, e->e_flags))
			{
				char state;
				char *response;

				response = milter_envfrom(args, e, &state);
				switch (state)
				{
				  case SMFIR_REPLYCODE:
					usrerr(response);
					break;

				  case SMFIR_REJECT:
					usrerr("550 5.7.1 Command rejected");
					break;

				  case SMFIR_DISCARD:
					e->e_flags |= EF_DISCARD;
					break;

				  case SMFIR_TEMPFAIL:
					usrerr("451 4.7.1 Please try again later");
					break;
				}
				if (response != NULL)
					sm_free(response);
			}
# endif /* _FFR_MILTER */
			if (Errors > 0)
				goto undo_subproc_no_pm;

			message("250 2.1.0 Sender ok");
			gotmail = TRUE;
			break;

		  case CMDRCPT:		/* rcpt -- designate recipient */
			if (!gotmail)
			{
				usrerr("503 5.0.0 Need MAIL before RCPT");
				break;
			}
			SmtpPhase = "server RCPT";
			if (setjmp(TopFrame) > 0)
			{
				e->e_flags &= ~(EF_FATALERRS|EF_PM_NOTIFY);
				break;
			}
			QuickAbort = TRUE;
			LogUsrErrs = TRUE;

			/* limit flooding of our machine */
			if (MaxRcptPerMsg > 0 && nrcpts >= MaxRcptPerMsg)
			{
				usrerr("452 4.5.3 Too many recipients");
				break;
			}

			if (e->e_sendmode != SM_DELIVER)
				e->e_flags |= EF_VRFYONLY;

# if _FFR_MILTER
			/*
			**  If the filter will be deleting recipients,
			**  don't expand them at RCPT time (in the call
			**  to recipient()).  If they are expanded, it
			**  is impossible for removefromlist() to figure
			**  out the expanded members of the original
			**  recipient and mark them as QS_DONTSEND.
			*/

			if (milter_can_delrcpts())
				e->e_flags |= EF_VRFYONLY;
# endif /* _FFR_MILTER */

			p = skipword(p, "to");
			if (p == NULL)
				break;
# if _FFR_ADDR_TYPE
			define(macid("{addr_type}", NULL), "e r", e);
# endif /* _FFR_ADDR_TYPE */
			a = parseaddr(p, NULLADDR, RF_COPYALL, ' ', &delimptr, e);
#if _FFR_ADDR_TYPE
			define(macid("{addr_type}", NULL), NULL, e);
#endif /* _FFR_ADDR_TYPE */
			if (Errors > 0)
				break;
			if (a == NULL)
			{
				usrerr("501 5.0.0 Missing recipient");
				break;
			}

			if (delimptr != NULL && *delimptr != '\0')
				*delimptr++ = '\0';

			/* put resulting triple from parseaddr() into macros */
			if (a->q_mailer != NULL)
				define(macid("{rcpt_mailer}", NULL),
				       a->q_mailer->m_name, e);
			else
				define(macid("{rcpt_mailer}", NULL),
				       NULL, e);
			if (a->q_host != NULL)
				define(macid("{rcpt_host}", NULL),
				       a->q_host, e);
			else
				define(macid("{rcpt_host}", NULL),
				       "localhost", e);
			if (a->q_user != NULL)
				define(macid("{rcpt_addr}", NULL),
				       a->q_user, e);
			else
				define(macid("{rcpt_addr}", NULL),
				       NULL, e);
			if (Errors > 0)
				break;

			/* now parse ESMTP arguments */
			addr = p;
			argno = 0;
			args[argno++] = p;
			p = delimptr;
			while (p != NULL && *p != '\0')
			{
				char *kp;
				char *vp = NULL;
				char *equal = NULL;

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
					equal = p;
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
					dprintf("RCPT: got arg %s=\"%s\"\n", kp,
						vp == NULL ? "<null>" : vp);

				rcpt_esmtp_args(a, kp, vp, e);
				if (equal != NULL)
					*equal = '=';
				args[argno++] = kp;
				if (argno >= MAXSMTPARGS - 1)
					usrerr("501 5.5.4 Too many parameters");
				if (Errors > 0)
					break;
			}
			args[argno] = NULL;
			if (Errors > 0)
				break;

			/* do config file checking of the recipient */
			if (rscheck("check_rcpt", addr,
				    NULL, e, TRUE, TRUE, 4, NULL) != EX_OK ||
			    Errors > 0)
				break;

# if _FFR_MILTER
			if (milterize && !bitset(EF_DISCARD, e->e_flags))
			{
				char state;
				char *response;

				response = milter_envrcpt(args, e, &state);
				switch (state)
				{
				  case SMFIR_REPLYCODE:
					usrerr(response);
					break;

				  case SMFIR_REJECT:
					usrerr("550 5.7.1 Command rejected");
					break;

				  case SMFIR_DISCARD:
					e->e_flags |= EF_DISCARD;
					break;

				  case SMFIR_TEMPFAIL:
					usrerr("451 4.7.1 Please try again later");
					break;
				}
				if (response != NULL)
					sm_free(response);
			}
# endif /* _FFR_MILTER */

			define(macid("{rcpt_mailer}", NULL), NULL, e);
			define(macid("{rcpt_relay}", NULL), NULL, e);
			define(macid("{rcpt_addr}", NULL), NULL, e);
			define(macid("{dsn_notify}", NULL), NULL, e);
			if (Errors > 0)
				break;

			/* save in recipient list after ESMTP mods */
			a = recipient(a, &e->e_sendqueue, 0, e);
			if (Errors > 0)
				break;

			/* no errors during parsing, but might be a duplicate */
			e->e_to = a->q_paddr;
			if (!QS_IS_BADADDR(a->q_state))
			{
				if (e->e_queuedir == NOQDIR)
					initsys(e);
				message("250 2.1.5 Recipient ok%s",
					QS_IS_QUEUEUP(a->q_state) ?
						" (will queue)" : "");
				nrcpts++;
			}
			else
			{
				/* punt -- should keep message in ADDRESS.... */
				usrerr("550 5.1.1 Addressee unknown");
			}
			break;

		  case CMDDATA:		/* data -- text of mail */
			SmtpPhase = "server DATA";
			if (!gotmail)
			{
				usrerr("503 5.0.0 Need MAIL command");
				break;
			}
			else if (nrcpts <= 0)
			{
				usrerr("503 5.0.0 Need RCPT (recipient)");
				break;
			}

			/* put back discard bit */
			if (discard)
				e->e_flags |= EF_DISCARD;

			/* check to see if we need to re-expand aliases */
			/* also reset QS_BADADDR on already-diagnosted addrs */
			doublequeue = FALSE;
			for (a = e->e_sendqueue; a != NULL; a = a->q_next)
			{
				if (QS_IS_VERIFIED(a->q_state) &&
				    !bitset(EF_DISCARD, e->e_flags))
				{
					/* need to re-expand aliases */
					doublequeue = TRUE;
				}
				if (QS_IS_BADADDR(a->q_state))
				{
					/* make this "go away" */
					a->q_state = QS_DONTSEND;
				}
			}

			/* collect the text of the message */
			SmtpPhase = "collect";
			buffer_errors();
			collect(InChannel, TRUE, NULL, e);

# if _FFR_MILTER
			if (milterize &&
			    Errors <= 0 &&
			    !bitset(EF_DISCARD, e->e_flags))
			{
				char state;
				char *response;

				response = milter_data(e, &state);
				switch (state)
				{
				  case SMFIR_REPLYCODE:
					usrerr(response);
					break;

				  case SMFIR_REJECT:
					usrerr("554 5.7.1 Command rejected");
					break;

				  case SMFIR_DISCARD:
					e->e_flags |= EF_DISCARD;
					break;

				  case SMFIR_TEMPFAIL:
					usrerr("451 4.7.1 Please try again later");
					break;
				}
				if (response != NULL)
					sm_free(response);
			}

			/* abort message filters that didn't get the body */
			if (milterize)
				milter_abort(e);
# endif /* _FFR_MILTER */

			/* redefine message size */
			if ((q = macvalue(macid("{msg_size}", NULL), e))
			    != NULL)
				sm_free(q);
			snprintf(inp, sizeof inp, "%ld", e->e_msgsize);
			define(macid("{msg_size}", NULL), newstr(inp), e);
			if (Errors > 0)
			{
				/* Log who the mail would have gone to */
				if (LogLevel > 8 &&
				    e->e_message != NULL)
				{
					for (a = e->e_sendqueue;
					     a != NULL;
					     a = a->q_next)
					{
						if (!QS_IS_UNDELIVERED(a->q_state))
							continue;

						e->e_to = a->q_paddr;
						logdelivery(NULL, NULL,
							    a->q_status,
							    e->e_message,
							    NULL,
							    (time_t) 0, e);
					}
					e->e_to = NULL;
				}
				flush_errors(TRUE);
				buffer_errors();
				goto abortmessage;
			}

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
			(void) bftruncate(e->e_xfp);
			id = e->e_id;

			/*
			**  If a header/body check (header checks or milter)
			**  set EF_DISCARD, don't queueup the message --
			**  that would lose the EF_DISCARD bit and deliver
			**  the message.
			*/

			if (bitset(EF_DISCARD, e->e_flags))
				doublequeue = FALSE;

			if (doublequeue)
			{
				/* make sure it is in the queue */
				queueup(e, FALSE);
			}
			else
			{
				/* send to all recipients */
# if NAMED_BIND
				_res.retry = TimeOuts.res_retry[RES_TO_FIRST];
				_res.retrans = TimeOuts.res_retrans[RES_TO_FIRST];
# endif /* NAMED_BIND */
				sendall(e, SM_DEFAULT);
			}
			e->e_to = NULL;

			/* issue success message */
			message("250 2.0.0 %s Message accepted for delivery", id);

			/* if we just queued, poke it */
			if (doublequeue &&
			    e->e_sendmode != SM_QUEUE &&
			    e->e_sendmode != SM_DEFER)
			{
				CurrentLA = sm_getla(e);

				if (!shouldqueue(e->e_msgpriority, e->e_ctime))
				{
					/* close all the queue files */
					closexscript(e);
					if (e->e_dfp != NULL)
						(void) bfclose(e->e_dfp);
					e->e_dfp = NULL;
					unlockqueue(e);

					(void) dowork(e->e_queuedir, id,
						      TRUE, TRUE, e);
				}
			}

  abortmessage:
			if (LogLevel > 4 && bitset(EF_LOGSENDER, e->e_flags))
				logsender(e, NULL);
			e->e_flags &= ~EF_LOGSENDER;

			/* if in a child, pop back to our parent */
			if (InChild)
				finis(TRUE, ExitStat);

			/* clean up a bit */
			gotmail = FALSE;
			dropenvelope(e, TRUE);
			CurEnv = e = newenvelope(e, CurEnv);
			e->e_flags = BlankEnvelope.e_flags;
			break;

		  case CMDRSET:		/* rset -- reset state */
# if _FFR_MILTER
			/* abort milter filters */
			milter_abort(e);
# endif /* _FFR_MILTER */

			if (tTd(94, 100))
				message("451 4.0.0 Test failure");
			else
				message("250 2.0.0 Reset state");

			/* arrange to ignore any current send list */
			e->e_sendqueue = NULL;
			e->e_flags |= EF_CLRQUEUE;

			if (LogLevel > 4 && bitset(EF_LOGSENDER, e->e_flags))
				logsender(e, NULL);
			e->e_flags &= ~EF_LOGSENDER;

			if (InChild)
				finis(TRUE, ExitStat);

			/* clean up a bit */
			gotmail = FALSE;
			SuprErrs = TRUE;
			dropenvelope(e, TRUE);
			CurEnv = e = newenvelope(e, CurEnv);
			break;

		  case CMDVRFY:		/* vrfy -- verify address */
		  case CMDEXPN:		/* expn -- expand address */
			if (tempfail)
			{
				if (LogLevel > 9)
					sm_syslog(LOG_INFO, e->e_id,
						  "SMTP %s command (%.100s) from %.100s tempfailed (due to previous checks)",
						  c->cmd_code == CMDVRFY ? "VRFY" : "EXPN",
						  p, CurSmtpClient);
				usrerr("550 5.7.1 Please try again later");
				break;
			}
			wt = checksmtpattack(&nverifies, MAXVRFYCOMMANDS, FALSE,
				c->cmd_code == CMDVRFY ? "VRFY" : "EXPN", e);
			previous = curtime();
			vrfy = c->cmd_code == CMDVRFY;
			if (bitset(vrfy ? PRIV_NOVRFY : PRIV_NOEXPN,
						PrivacyFlags))
			{
				if (vrfy)
					message("252 2.5.2 Cannot VRFY user; try RCPT to attempt delivery (or try finger)");
				else
					message("502 5.7.0 Sorry, we do not allow this operation");
				if (LogLevel > 5)
					sm_syslog(LOG_INFO, e->e_id,
						  "%.100s: %s [rejected]",
						  CurSmtpClient,
						  shortenstring(inp, MAXSHORTSTR));
				break;
			}
			else if (!gothello &&
				 bitset(vrfy ? PRIV_NEEDVRFYHELO : PRIV_NEEDEXPNHELO,
						PrivacyFlags))
			{
				usrerr("503 5.0.0 I demand that you introduce yourself first");
				break;
			}
			if (runinchild(vrfy ? "SMTP-VRFY" : "SMTP-EXPN", e) > 0)
				break;
			if (Errors > 0)
				goto undo_subproc;
			if (LogLevel > 5)
				sm_syslog(LOG_INFO, e->e_id,
					  "%.100s: %s",
					  CurSmtpClient,
					  shortenstring(inp, MAXSHORTSTR));
			if (setjmp(TopFrame) > 0)
				goto undo_subproc;
			QuickAbort = TRUE;
			vrfyqueue = NULL;
			if (vrfy)
				e->e_flags |= EF_VRFYONLY;
			while (*p != '\0' && isascii(*p) && isspace(*p))
				p++;
			if (*p == '\0')
			{
				usrerr("501 5.5.2 Argument required");
			}
			else
			{
				/* do config file checking of the address */
				if (rscheck(vrfy ? "check_vrfy" : "check_expn",
					    p, NULL, e, TRUE, FALSE, 4, NULL)
				    != EX_OK || Errors > 0)
					goto undo_subproc;
				(void) sendtolist(p, NULLADDR, &vrfyqueue, 0, e);
			}
			if (wt > 0)
			{
				time_t t;

				t = wt - (curtime() - previous);
				if (t > 0)
					(void) sleep(t);
			}
			if (Errors > 0)
				goto undo_subproc;
			if (vrfyqueue == NULL)
			{
				usrerr("554 5.5.2 Nothing to %s", vrfy ? "VRFY" : "EXPN");
			}
			while (vrfyqueue != NULL)
			{
				if (!QS_IS_UNDELIVERED(vrfyqueue->q_state))
				{
					vrfyqueue = vrfyqueue->q_next;
					continue;
				}

				/* see if there is more in the vrfy list */
				a = vrfyqueue;
				while ((a = a->q_next) != NULL &&
				       (!QS_IS_UNDELIVERED(a->q_state)))
					continue;
				printvrfyaddr(vrfyqueue, a == NULL, vrfy);
				vrfyqueue = a;
			}
			if (InChild)
				finis(TRUE, ExitStat);
			break;

		  case CMDETRN:		/* etrn -- force queue flush */
			if (bitset(PRIV_NOETRN, PrivacyFlags) ||
			    bitnset(D_NOETRN, d_flags))
			{
				/* different message for MSA ? */
				message("502 5.7.0 Sorry, we do not allow this operation");
				if (LogLevel > 5)
					sm_syslog(LOG_INFO, e->e_id,
						  "%.100s: %s [rejected]",
						  CurSmtpClient,
						  shortenstring(inp, MAXSHORTSTR));
				break;
			}
			if (tempfail)
			{
				if (LogLevel > 9)
					sm_syslog(LOG_INFO, e->e_id,
						  "SMTP ETRN command (%.100s) from %.100s tempfailed (due to previous checks)",
						  p, CurSmtpClient);
				usrerr("451 4.7.1 Please try again later");
				break;
			}

			if (strlen(p) <= 0)
			{
				usrerr("500 5.5.2 Parameter required");
				break;
			}

			/* crude way to avoid denial-of-service attacks */
			(void) checksmtpattack(&n_etrn, MAXETRNCOMMANDS, TRUE,
					     "ETRN", e);

			/* do config file checking of the parameter */
			if (rscheck("check_etrn", p, NULL, e, TRUE, FALSE, 4,
				    NULL) != EX_OK || Errors > 0)
				break;

			if (LogLevel > 5)
				sm_syslog(LOG_INFO, e->e_id,
					  "%.100s: ETRN %s",
					  CurSmtpClient,
					  shortenstring(p, MAXSHORTSTR));

			id = p;
			if (*id == '@')
				id++;
			else
				*--id = '@';

			new = (QUEUE_CHAR *)xalloc(sizeof(QUEUE_CHAR));
			new->queue_match = id;
			new->queue_next = NULL;
			QueueLimitRecipient = new;
			ok = runqueue(TRUE, FALSE);
			sm_free(QueueLimitRecipient);
			QueueLimitRecipient = NULL;
			if (ok && Errors == 0)
				message("250 2.0.0 Queuing for node %s started", p);
			break;

		  case CMDHELP:		/* help -- give user info */
			help(p, e);
			break;

		  case CMDNOOP:		/* noop -- do nothing */
			(void) checksmtpattack(&n_noop, MAXNOOPCOMMANDS, TRUE,
					       "NOOP", e);
			message("250 2.0.0 OK");
			break;

		  case CMDQUIT:		/* quit -- leave mail */
			message("221 2.0.0 %s closing connection", MyHostName);

			/* arrange to ignore any current send list */
			e->e_sendqueue = NULL;

# if STARTTLS
			/* shutdown TLS connection */
			if (tls_active)
			{
				(void) endtls(srv_ssl, "server");
				tls_active = FALSE;
			}
# endif /* STARTTLS */
# if SASL
			if (authenticating == SASL_IS_AUTH)
			{
				sasl_dispose(&conn);
				authenticating = SASL_NOT_AUTH;
			}
# endif /* SASL */

doquit:
			/* avoid future 050 messages */
			disconnect(1, e);

# if _FFR_MILTER
			/* close out milter filters */
			milter_quit(e);
# endif /* _FFR_MILTER */

			if (InChild)
				ExitStat = EX_QUIT;

			if (LogLevel > 4 && bitset(EF_LOGSENDER, e->e_flags))
				logsender(e, NULL);
			e->e_flags &= ~EF_LOGSENDER;

			if (lognullconnection && LogLevel > 5)
			{
				char *d;

				d = macvalue(macid("{daemon_name}", NULL), e);
				if (d == NULL)
					d = "stdin";
				sm_syslog(LOG_INFO, NULL,
					 "%.100s did not issue MAIL/EXPN/VRFY/ETRN during connection to %s",
					  CurSmtpClient, d);
			}
			finis(TRUE, ExitStat);
			/* NOTREACHED */

		  case CMDVERB:		/* set verbose mode */
			if (bitset(PRIV_NOEXPN, PrivacyFlags) ||
			    bitset(PRIV_NOVERB, PrivacyFlags))
			{
				/* this would give out the same info */
				message("502 5.7.0 Verbose unavailable");
				break;
			}
			(void) checksmtpattack(&n_noop, MAXNOOPCOMMANDS, TRUE,
					       "VERB", e);
			Verbose = 1;
			set_delivery_mode(SM_DELIVER, e);
			message("250 2.0.0 Verbose mode");
			break;

		  case CMDONEX:		/* doing one transaction only */
			(void) checksmtpattack(&n_noop, MAXNOOPCOMMANDS, TRUE,
					       "ONEX", e);
			OneXact = TRUE;
			message("250 2.0.0 Only one transaction");
			break;

		  case CMDXUSR:		/* initial (user) submission */
			(void) checksmtpattack(&n_noop, MAXNOOPCOMMANDS, TRUE,
					       "XUSR", e);
			define(macid("{daemon_flags}", NULL), "c u", CurEnv);
			message("250 2.0.0 Initial submission");
			break;

# if SMTPDEBUG
		  case CMDDBGQSHOW:	/* show queues */
			printf("Send Queue=");
			printaddr(e->e_sendqueue, TRUE);
			break;

		  case CMDDBGDEBUG:	/* set debug mode */
			tTsetup(tTdvect, sizeof tTdvect, "0-99.1");
			tTflag(p);
			message("200 2.0.0 Debug set");
			break;

# else /* SMTPDEBUG */
		  case CMDDBGQSHOW:	/* show queues */
		  case CMDDBGDEBUG:	/* set debug mode */
# endif /* SMTPDEBUG */
		  case CMDLOGBOGUS:	/* bogus command */
			if (LogLevel > 0)
				sm_syslog(LOG_CRIT, e->e_id,
					  "\"%s\" command from %.100s (%.100s)",
					  c->cmd_name, CurSmtpClient,
					  anynet_ntoa(&RealHostAddr));
			/* FALLTHROUGH */

		  case CMDERROR:	/* unknown command */
			if (++badcommands > MAXBADCOMMANDS)
			{
				message("421 4.7.0 %s Too many bad commands; closing connection",
					MyHostName);

				/* arrange to ignore any current send list */
				e->e_sendqueue = NULL;
				goto doquit;
			}

			usrerr("500 5.5.1 Command unrecognized: \"%s\"",
			       shortenstring(inp, MAXSHORTSTR));
			break;

		  case CMDUNIMPL:
			usrerr("502 5.5.1 Command not implemented: \"%s\"",
			       shortenstring(inp, MAXSHORTSTR));
			break;

		  default:
			errno = 0;
			syserr("500 5.5.0 smtp: unknown code %d", c->cmd_code);
			break;
		}
# if SASL
		}
# endif /* SASL */
	}

}
/*
**  CHECKSMTPATTACK -- check for denial-of-service attack by repetition
**
**	Parameters:
**		pcounter -- pointer to a counter for this command.
**		maxcount -- maximum value for this counter before we
**			slow down.
**		waitnow -- sleep now (in this routine)?
**		cname -- command name for logging.
**		e -- the current envelope.
**
**	Returns:
**		time to wait.
**
**	Side Effects:
**		Slows down if we seem to be under attack.
*/

static time_t
checksmtpattack(pcounter, maxcount, waitnow, cname, e)
	volatile int *pcounter;
	int maxcount;
	bool waitnow;
	char *cname;
	ENVELOPE *e;
{
	if (++(*pcounter) >= maxcount)
	{
		time_t s;

		if (*pcounter == maxcount && LogLevel > 5)
		{
			sm_syslog(LOG_INFO, e->e_id,
				  "%.100s: possible SMTP attack: command=%.40s, count=%d",
				  CurSmtpClient, cname, *pcounter);
		}
		s = 1 << (*pcounter - maxcount);
		if (s >= MAXTIMEOUT)
			s = MAXTIMEOUT;
		/* sleep at least 1 second before returning */
		(void) sleep(*pcounter / maxcount);
		s -= *pcounter / maxcount;
		if (waitnow)
		{
			(void) sleep(s);
			return(0);
		}
		return(s);
	}
	return((time_t) 0);
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
	register char *volatile p;
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
		usrerr("501 5.5.2 Syntax error in parameters scanning \"%s\"",
			shortenstring(firstp, MAXSHORTSTR));
		return NULL;
	}
	*p++ = '\0';
	while (isascii(*p) && isspace(*p))
		p++;

	if (*p == '\0')
		goto syntax;

	/* see if the input word matches desired word */
	if (strcasecmp(q, w))
		goto syntax;

	return p;
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

static void
mail_esmtp_args(kp, vp, e)
	char *kp;
	char *vp;
	ENVELOPE *e;
{
	if (strcasecmp(kp, "size") == 0)
	{
		if (vp == NULL)
		{
			usrerr("501 5.5.2 SIZE requires a value");
			/* NOTREACHED */
		}
		define(macid("{msg_size}", NULL), newstr(vp), e);
		e->e_msgsize = strtol(vp, (char **) NULL, 10);
		if (e->e_msgsize == LONG_MAX && errno == ERANGE)
		{
			usrerr("552 5.2.3 Message size exceeds maximum value");
			/* NOTREACHED */
		}
	}
	else if (strcasecmp(kp, "body") == 0)
	{
		if (vp == NULL)
		{
			usrerr("501 5.5.2 BODY requires a value");
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
			usrerr("501 5.5.4 Unknown BODY type %s",
				vp);
			/* NOTREACHED */
		}
		e->e_bodytype = newstr(vp);
	}
	else if (strcasecmp(kp, "envid") == 0)
	{
		if (bitset(PRIV_NORECEIPTS, PrivacyFlags))
		{
			usrerr("504 5.7.0 Sorry, ENVID not supported, we do not allow DSN");
			/* NOTREACHED */
		}
		if (vp == NULL)
		{
			usrerr("501 5.5.2 ENVID requires a value");
			/* NOTREACHED */
		}
		if (!xtextok(vp))
		{
			usrerr("501 5.5.4 Syntax error in ENVID parameter value");
			/* NOTREACHED */
		}
		if (e->e_envid != NULL)
		{
			usrerr("501 5.5.0 Duplicate ENVID parameter");
			/* NOTREACHED */
		}
		e->e_envid = newstr(vp);
		define(macid("{dsn_envid}", NULL), newstr(vp), e);
	}
	else if (strcasecmp(kp, "ret") == 0)
	{
		if (bitset(PRIV_NORECEIPTS, PrivacyFlags))
		{
			usrerr("504 5.7.0 Sorry, RET not supported, we do not allow DSN");
			/* NOTREACHED */
		}
		if (vp == NULL)
		{
			usrerr("501 5.5.2 RET requires a value");
			/* NOTREACHED */
		}
		if (bitset(EF_RET_PARAM, e->e_flags))
		{
			usrerr("501 5.5.0 Duplicate RET parameter");
			/* NOTREACHED */
		}
		e->e_flags |= EF_RET_PARAM;
		if (strcasecmp(vp, "hdrs") == 0)
			e->e_flags |= EF_NO_BODY_RETN;
		else if (strcasecmp(vp, "full") != 0)
		{
			usrerr("501 5.5.2 Bad argument \"%s\" to RET", vp);
			/* NOTREACHED */
		}
		define(macid("{dsn_ret}", NULL), newstr(vp), e);
	}
# if SASL
	else if (strcasecmp(kp, "auth") == 0)
	{
		int len;
		char *q;
		char *auth_param;	/* the value of the AUTH=x */
		bool saveQuickAbort = QuickAbort;
		bool saveSuprErrs = SuprErrs;
		char pbuf[256];

		if (vp == NULL)
		{
			usrerr("501 5.5.2 AUTH= requires a value");
			/* NOTREACHED */
		}
		if (e->e_auth_param != NULL)
		{
			usrerr("501 5.5.0 Duplicate AUTH parameter");
			/* NOTREACHED */
		}
		if ((q = strchr(vp, ' ')) != NULL)
			len = q - vp + 1;
		else
			len = strlen(vp) + 1;
		auth_param = xalloc(len);
		(void) strlcpy(auth_param, vp, len);
		if (!xtextok(auth_param))
		{
			usrerr("501 5.5.4 Syntax error in AUTH parameter value");
			/* just a warning? */
			/* NOTREACHED */
		}

		/* XXX this might be cut off */
		snprintf(pbuf, sizeof pbuf, "%s", xuntextify(auth_param));
		/* xalloc() the buffer instead? */

		/* XXX define this always or only if trusted? */
		define(macid("{auth_author}", NULL), newstr(pbuf), e);

		/*
		**  call Strust_auth to find out whether
		**  auth_param is acceptable (trusted)
		**  we shouldn't trust it if not authenticated
		**  (required by RFC, leave it to ruleset?)
		*/

		SuprErrs = TRUE;
		QuickAbort = FALSE;
		if (strcmp(auth_param, "<>") != 0 &&
		     (rscheck("trust_auth", pbuf, NULL, e, TRUE, FALSE, 10,
			      NULL) != EX_OK || Errors > 0))
		{
			if (tTd(95, 8))
			{
				q = e->e_auth_param;
				dprintf("auth=\"%.100s\" not trusted user=\"%.100s\"\n",
					pbuf, (q == NULL) ? "" : q);
			}
			/* not trusted */
			e->e_auth_param = newstr("<>");
		}
		else
		{
			if (tTd(95, 8))
				dprintf("auth=\"%.100s\" trusted\n", pbuf);
			e->e_auth_param = newstr(auth_param);
		}
		sm_free(auth_param);

		/* reset values */
		Errors = 0;
		QuickAbort = saveQuickAbort;
		SuprErrs = saveSuprErrs;
	}
# endif /* SASL */
	else
	{
		usrerr("555 5.5.4 %s parameter unrecognized", kp);
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

static void
rcpt_esmtp_args(a, kp, vp, e)
	ADDRESS *a;
	char *kp;
	char *vp;
	ENVELOPE *e;
{
	if (strcasecmp(kp, "notify") == 0)
	{
		char *p;

		if (bitset(PRIV_NORECEIPTS, PrivacyFlags))
		{
			usrerr("504 5.7.0 Sorry, NOTIFY not supported, we do not allow DSN");
			/* NOTREACHED */
		}
		if (vp == NULL)
		{
			usrerr("501 5.5.2 NOTIFY requires a value");
			/* NOTREACHED */
		}
		a->q_flags &= ~(QPINGONSUCCESS|QPINGONFAILURE|QPINGONDELAY);
		a->q_flags |= QHASNOTIFY;
		define(macid("{dsn_notify}", NULL), newstr(vp), e);

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
				usrerr("501 5.5.4 Bad argument \"%s\"  to NOTIFY",
					vp);
				/* NOTREACHED */
			}
		}
	}
	else if (strcasecmp(kp, "orcpt") == 0)
	{
		if (bitset(PRIV_NORECEIPTS, PrivacyFlags))
		{
			usrerr("504 5.7.0 Sorry, ORCPT not supported, we do not allow DSN");
			/* NOTREACHED */
		}
		if (vp == NULL)
		{
			usrerr("501 5.5.2 ORCPT requires a value");
			/* NOTREACHED */
		}
		if (strchr(vp, ';') == NULL || !xtextok(vp))
		{
			usrerr("501 5.5.4 Syntax error in ORCPT parameter value");
			/* NOTREACHED */
		}
		if (a->q_orcpt != NULL)
		{
			usrerr("501 5.5.0 Duplicate ORCPT parameter");
			/* NOTREACHED */
		}
		a->q_orcpt = newstr(vp);
	}
	else
	{
		usrerr("555 5.5.4 %s parameter unrecognized", kp);
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
#define OFFF	(3 + 1 + 5 + 1)	/* offset in fmt: SMTP reply + enh. code */

static void
printvrfyaddr(a, last, vrfy)
	register ADDRESS *a;
	bool last;
	bool vrfy;
{
	char fmtbuf[30];

	if (vrfy && a->q_mailer != NULL &&
	    !bitnset(M_VRFY250, a->q_mailer->m_flags))
		(void) strlcpy(fmtbuf, "252", sizeof fmtbuf);
	else
		(void) strlcpy(fmtbuf, "250", sizeof fmtbuf);
	fmtbuf[3] = last ? ' ' : '-';
	(void) strlcpy(&fmtbuf[4], "2.1.5 ", sizeof fmtbuf - 4);
	if (a->q_fullname == NULL)
	{
		if ((a->q_mailer == NULL ||
		     a->q_mailer->m_addrtype == NULL ||
		     strcasecmp(a->q_mailer->m_addrtype, "rfc822") == 0) &&
		    strchr(a->q_user, '@') == NULL)
			(void) strlcpy(&fmtbuf[OFFF], "<%s@%s>",
				       sizeof fmtbuf - OFFF);
		else
			(void) strlcpy(&fmtbuf[OFFF], "<%s>",
				       sizeof fmtbuf - OFFF);
		message(fmtbuf, a->q_user, MyHostName);
	}
	else
	{
		if ((a->q_mailer == NULL ||
		     a->q_mailer->m_addrtype == NULL ||
		     strcasecmp(a->q_mailer->m_addrtype, "rfc822") == 0) &&
		    strchr(a->q_user, '@') == NULL)
			(void) strlcpy(&fmtbuf[OFFF], "%s <%s@%s>",
				       sizeof fmtbuf - OFFF);
		else
			(void) strlcpy(&fmtbuf[OFFF], "%s <%s>",
				       sizeof fmtbuf - OFFF);
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
**		RIC_INCHILD in the child
**		RIC_INPARENT in the parent
**		RIC_TEMPFAIL tempfail condition
**
**	Side Effects:
**		none.
*/

static int
runinchild(label, e)
	char *label;
	register ENVELOPE *e;
{
	pid_t childpid;

	if (!OneXact)
	{
		extern int NumQueues;

		/*
		**  advance state of PRNG
		**  this is necessary because otherwise all child processes
		**  will produce the same PRN sequence and hence the selection
		**  of a queue directory is not "really" random.
		*/
		if (NumQueues > 1)
			(void) get_random();

		/*
		**  Disable child process reaping, in case ETRN has preceded
		**  MAIL command, and then fork.
		*/

		(void) blocksignal(SIGCHLD);


		childpid = dofork();
		if (childpid < 0)
		{
			syserr("451 4.3.0 %s: cannot fork", label);
			(void) releasesignal(SIGCHLD);
			return RIC_INPARENT;
		}
		if (childpid > 0)
		{
			auto int st;

			/* parent -- wait for child to complete */
			sm_setproctitle(TRUE, e, "server %s child wait",
					CurSmtpClient);
			st = waitfor(childpid);
			if (st == -1)
				syserr("451 4.3.0 %s: lost child", label);
			else if (!WIFEXITED(st))
			{
				syserr("451 4.3.0 %s: died on signal %d",
				       label, st & 0177);
				return RIC_TEMPFAIL;
			}

			/* if exited on a QUIT command, complete the process */
			if (WEXITSTATUS(st) == EX_QUIT)
			{
				disconnect(1, e);
				finis(TRUE, ExitStat);
			}

			/* restore the child signal */
			(void) releasesignal(SIGCHLD);

			return RIC_INPARENT;
		}
		else
		{
			/* child */
			InChild = TRUE;
			QuickAbort = FALSE;

			/* Reset global flags */
			RestartRequest = NULL;
			ShutdownRequest = NULL;
			PendingSignal = 0;

			clearstats();
			clearenvelope(e, FALSE);
			assign_queueid(e);
			(void) setsignal(SIGCHLD, SIG_DFL);
			(void) releasesignal(SIGCHLD);
		}
	}
	return RIC_INCHILD;
}

# if SASL

/*
**  SASLMECHS -- get list of possible AUTH mechanisms
**
**	Parameters:
**		conn -- SASL connection info
**		mechlist -- output parameter for list of mechanisms
**
**	Returns:
**		number of mechs
*/

static int
saslmechs(conn, mechlist)
	sasl_conn_t *conn;
	char **mechlist;
{
	int len, num, result;

	/* "user" is currently unused */
	result = sasl_listmech(conn, "user", /* XXX */
			       "", " ", "", mechlist,
			       (u_int *)&len, (u_int *)&num);
	if (result == SASL_OK && num > 0)
	{
		if (LogLevel > 11)
			sm_syslog(LOG_INFO, NOQID,
				  "SASL: available mech=%s, allowed mech=%s",
				  *mechlist, AuthMechanisms);
		*mechlist = intersect(AuthMechanisms, *mechlist);
	}
	else
	{
		if (LogLevel > 9)
			sm_syslog(LOG_WARNING, NOQID,
				  "SASL error: listmech=%d, num=%d",
				  result, num);
		num = 0;
	}
	return num;
}

/*
**  PROXY_POLICY -- define proxy policy for AUTH
**
**	Parameters:
**		conntext -- unused
**		auth_identity -- authentication identity
**		requested_user -- authorization identity
**		user -- allowed user (output)
**		errstr -- possible error string (output)
**
**	Returns:
**		ok?
*/

int
proxy_policy(context, auth_identity, requested_user, user, errstr)
	void *context;
	const char *auth_identity;
	const char *requested_user;
	const char **user;
	const char **errstr;
{
	if (user == NULL || auth_identity == NULL)
		return SASL_FAIL;
	*user = newstr(auth_identity);
	return SASL_OK;
}

# endif /* SASL */

# if STARTTLS
#  if !TLS_NO_RSA
RSA *rsa_tmp;	/* temporary RSA key */
static RSA * tmp_rsa_key __P((SSL *, int, int));
#  endif /* !TLS_NO_RSA */

# if !NO_DH
static DH *get_dh512 __P((void));

static unsigned char dh512_p[] =
{
	0xDA,0x58,0x3C,0x16,0xD9,0x85,0x22,0x89,0xD0,0xE4,0xAF,0x75,
	0x6F,0x4C,0xCA,0x92,0xDD,0x4B,0xE5,0x33,0xB8,0x04,0xFB,0x0F,
	0xED,0x94,0xEF,0x9C,0x8A,0x44,0x03,0xED,0x57,0x46,0x50,0xD3,
	0x69,0x99,0xDB,0x29,0xD7,0x76,0x27,0x6B,0xA2,0xD3,0xD4,0x12,
	0xE2,0x18,0xF4,0xDD,0x1E,0x08,0x4C,0xF6,0xD8,0x00,0x3E,0x7C,
	0x47,0x74,0xE8,0x33
};
static unsigned char dh512_g[] =
{
	0x02
};

static DH *
get_dh512()
{
	DH *dh = NULL;

	if ((dh = DH_new()) == NULL)
		return(NULL);
	dh->p = BN_bin2bn(dh512_p, sizeof(dh512_p), NULL);
	dh->g = BN_bin2bn(dh512_g, sizeof(dh512_g), NULL);
	if ((dh->p == NULL) || (dh->g == NULL))
		return(NULL);
	return(dh);
}
# endif /* !NO_DH */

/*
**  TLS_RAND_INIT -- initialize STARTTLS random generator
**
**	Parameters:
**		randfile -- name of file with random data
**		logl -- loglevel
**
**	Returns:
**		success/failure
**
**	Side Effects:
**		initializes PRNG for tls library.
*/

#define MIN_RAND_BYTES	16	/* 128 bits */

bool
tls_rand_init(randfile, logl)
	char *randfile;
	int logl;
{
# ifndef HASURANDOMDEV
	/* not required if /dev/urandom exists, OpenSSL does it internally */
#define RF_OK		0	/* randfile OK */
#define RF_MISS		1	/* randfile == NULL || *randfile == '\0' */
#define RF_UNKNOWN	2	/* unknown prefix for randfile */

#define RI_NONE		0	/* no init yet */
#define RI_SUCCESS	1	/* init was successful */
#define RI_FAIL		2	/* init failed */

	bool ok;
	int randdef;
	static int done = RI_NONE;

	/*
	**  initialize PRNG
	*/

	/* did we try this before? if yes: return old value */
	if (done != RI_NONE)
		return done == RI_SUCCESS;

	/* set default values */
	ok = FALSE;
	done = RI_FAIL;
	randdef = (randfile == NULL || *randfile == '\0') ? RF_MISS : RF_OK;
#    if EGD
	if (randdef == RF_OK && strncasecmp(randfile, "egd:", 4) == 0)
	{
		randfile += 4;
		if (RAND_egd(randfile) < 0)
		{
			sm_syslog(LOG_WARNING, NOQID,
				  "TLS: RAND_egd(%s) failed: random number generator not seeded",
				   randfile);
		}
		else
			ok = TRUE;
	}
	else
#    endif /* EGD */
	if (randdef == RF_OK && strncasecmp(randfile, "file:", 5) == 0)
	{
		int fd;
		long sff;
		struct stat st;

		randfile += 5;
		sff = SFF_SAFEDIRPATH | SFF_NOWLINK
		      | SFF_NOGWFILES | SFF_NOWWFILES
		      | SFF_NOGRFILES | SFF_NOWRFILES
		      | SFF_MUSTOWN | SFF_ROOTOK | SFF_OPENASROOT;
		if ((fd = safeopen(randfile, O_RDONLY, 0, sff)) >= 0)
		{
			if (fstat(fd, &st) < 0)
			{
				if (LogLevel > logl)
					sm_syslog(LOG_ERR, NOQID,
						  "TLS: can't fstat(%s)",
						  randfile);
			}
			else
			{
				bool use, problem;

				use = TRUE;
				problem = FALSE;
				if (st.st_mtime + 600 < curtime())
				{
					use = bitnset(DBS_INSUFFICIENTENTROPY,
						      DontBlameSendmail);
					problem = TRUE;
					if (LogLevel > logl)
						sm_syslog(LOG_ERR, NOQID,
							  "TLS: RandFile %s too old: %s",
							  randfile,
							  use ? "unsafe" :
								"unusable");
				}
				if (use && st.st_size < MIN_RAND_BYTES)
				{
					use = bitnset(DBS_INSUFFICIENTENTROPY,
						      DontBlameSendmail);
					problem = TRUE;
					if (LogLevel > logl)
						sm_syslog(LOG_ERR, NOQID,
							  "TLS: size(%s) < %d: %s",
							  randfile,
							  MIN_RAND_BYTES,
							  use ? "unsafe" :
								"unusable");
				}
				if (use)
					ok = RAND_load_file(randfile, -1) >=
					     MIN_RAND_BYTES;
				if (use && !ok)
				{
					if (LogLevel > logl)
						sm_syslog(LOG_WARNING,
							  NOQID,
							  "TLS: RAND_load_file(%s) failed: random number generator not seeded",
							  randfile);
				}
				if (problem)
					ok = FALSE;
			}
			if (ok || bitnset(DBS_INSUFFICIENTENTROPY,
					  DontBlameSendmail))
			{
				/* add this even if fstat() failed */
				RAND_seed((void *) &st, sizeof st);
			}
			(void) close(fd);
		}
		else
		{
			if (LogLevel > logl)
				sm_syslog(LOG_WARNING, NOQID,
					  "TLS: Warning: safeopen(%s) failed",
					  randfile);
		}
	}
	else if (randdef == RF_OK)
	{
		if (LogLevel > logl)
			sm_syslog(LOG_WARNING, NOQID,
				  "TLS: Error: no proper random file definition %s",
				  randfile);
		randdef = RF_UNKNOWN;
	}
	if (randdef == RF_MISS)
	{
		if (LogLevel > logl)
			sm_syslog(LOG_WARNING, NOQID,
				  "TLS: Error: missing random file definition");
	}
	if (!ok && bitnset(DBS_INSUFFICIENTENTROPY, DontBlameSendmail))
	{
		int i;
		long r;
		unsigned char buf[MIN_RAND_BYTES];

		/* assert((MIN_RAND_BYTES % sizeof(long)) == 0); */
		for (i = 0; i <= sizeof(buf) - sizeof(long); i += sizeof(long))
		{
			r = get_random();
			(void) memcpy(buf + i, (void *) &r, sizeof(long));
		}
		RAND_seed(buf, sizeof buf);
		if (LogLevel > logl)
			sm_syslog(LOG_WARNING, NOQID,
				  "TLS: Warning: random number generator not properly seeded");
		ok = TRUE;
	}
	done = ok ? RI_SUCCESS : RI_FAIL;
	return ok;
# else /* !HASURANDOMDEV */
	return TRUE;
# endif /* !HASURANDOMDEV */
}

/*
**  status in initialization
**  these flags keep track of the status of the initialization
**  i.e., whether a file exists (_EX) and whether it can be used (_OK)
**  [due to permissions]
*/
#define TLS_S_NONE	0x00000000	/* none yet  */
#define TLS_S_CERT_EX	0x00000001	/* CERT file exists */
#define TLS_S_CERT_OK	0x00000002	/* CERT file is ok */
#define TLS_S_KEY_EX	0x00000004	/* KEY file exists */
#define TLS_S_KEY_OK	0x00000008	/* KEY file is ok */
#define TLS_S_CERTP_EX	0x00000010	/* CA CERT PATH exists */
#define TLS_S_CERTP_OK	0x00000020	/* CA CERT PATH is ok */
#define TLS_S_CERTF_EX	0x00000040	/* CA CERT FILE exists */
#define TLS_S_CERTF_OK	0x00000080	/* CA CERT FILE is ok */

# if _FFR_TLS_1
#define TLS_S_CERT2_EX	0x00001000	/* 2nd CERT file exists */
#define TLS_S_CERT2_OK	0x00002000	/* 2nd CERT file is ok */
#define TLS_S_KEY2_EX	0x00004000	/* 2nd KEY file exists */
#define TLS_S_KEY2_OK	0x00008000	/* 2nd KEY file is ok */
# endif /* _FFR_TLS_1 */

#define TLS_S_DH_OK	0x00200000	/* DH cert is ok */
#define TLS_S_DHPAR_EX	0x00400000	/* DH param file exists */
#define TLS_S_DHPAR_OK	0x00800000	/* DH param file is ok to use */

/*
**  TLS_OK_F -- can var be an absolute filename?
**
**	Parameters:
**		var -- filename
**		fn -- what is the filename used for?
**
**	Returns:
**		ok?
*/

static bool
tls_ok_f(var, fn)
	char *var;
	char *fn;
{
	/* must be absolute pathname */
	if (var != NULL && *var == '/')
		return TRUE;
	if (LogLevel > 12)
		sm_syslog(LOG_WARNING, NOQID, "TLS: file %s missing", fn);
	return FALSE;
}

/*
**  TLS_SAFE_F -- is a file safe to use?
**
**	Parameters:
**		var -- filename
**		sff -- flags for safefile()
**
**	Returns:
**		ok?
*/

static bool
tls_safe_f(var, sff)
	char *var;
	long sff;
{
	int ret;

	if ((ret = safefile(var, RunAsUid, RunAsGid, RunAsUserName, sff,
			    S_IRUSR, NULL)) == 0)
		return TRUE;
	if (LogLevel > 7)
		sm_syslog(LOG_WARNING, NOQID, "TLS: file %s unsafe: %s",
			  var, errstring(ret));
	return FALSE;
}

/*
**  TLS_OK_F -- macro to simplify calls to tls_ok_f
**
**	Parameters:
**		var -- filename
**		fn -- what is the filename used for?
**		req -- is the file required?
**		st -- status bit to set if ok
**
**	Side Effects:
**		uses r, ok; may change ok and status.
**
*/

#define TLS_OK_F(var, fn, req, st) if (ok) \
	{ \
		r = tls_ok_f(var, fn); \
		if (r) \
			status |= st; \
		else if (req) \
			ok = FALSE; \
	}

/*
**  TLS_UNR -- macro to return whether a file should be unreadable
**
**	Parameters:
**		bit -- flag to test
**		req -- flags
**
**	Returns:
**		0/SFF_NORFILES
*/
#define TLS_UNR(bit, req)	(bitset(bit, req) ? SFF_NORFILES : 0)

/*
**  TLS_SAFE_F -- macro to simplify calls to tls_safe_f
**
**	Parameters:
**		var -- filename
**		sff -- flags for safefile()
**		req -- is the file required?
**		ex -- does the file exist?
**		st -- status bit to set if ok
**
**	Side Effects:
**		uses r, ok, ex; may change ok and status.
**
*/

#define TLS_SAFE_F(var, sff, req, ex, st) if (ex && ok) \
	{ \
		r = tls_safe_f(var, sff); \
		if (r) \
			status |= st;	\
		else if (req) \
			ok = FALSE;	\
	}
/*
**  INIT_TLS_LIBRARY -- calls functions which setup TLS library for global use
**
**	Parameters:
**		none.
**
**	Returns:
**		succeeded?
**
**	Side Effects:
**		Sets tls_ok_srv static, even when called from main()
*/

bool
init_tls_library()
{
	/*
	**  basic TLS initialization
	**  ignore result for now
	*/

	SSL_library_init();
	SSL_load_error_strings();
# if 0
	/* this is currently a macro for SSL_library_init */
	SSLeay_add_ssl_algorithms();
# endif /* 0 */

	/* initialize PRNG */
	tls_ok_srv = tls_rand_init(RandFile, 7);

	return tls_ok_srv;
}
/*
**  INITTLS -- initialize TLS
**
**	Parameters:
**		ctx -- pointer to context
**		req -- requirements for initialization (see sendmail.h)
**		srv -- server side?
**		certfile -- filename of certificate
**		keyfile -- filename of private key
**		cacertpath -- path to CAs
**		cacertfile -- file with CA
**		dhparam -- parameters for DH
**
**	Returns:
**		succeeded?
*/

bool
inittls(ctx, req, srv, certfile, keyfile, cacertpath, cacertfile, dhparam)
	SSL_CTX **ctx;
	u_long req;
	bool srv;
	char *certfile, *keyfile, *cacertpath, *cacertfile, *dhparam;
{
# if !NO_DH
	static DH *dh = NULL;
# endif /* !NO_DH */
	int r;
	bool ok;
	long sff, status;
	char *who;
# if _FFR_TLS_1
	char *cf2, *kf2;
# endif /* _FFR_TLS_1 */

	status = TLS_S_NONE;
	who = srv ? "srv" : "clt";
	if (ctx == NULL)
		syserr("TLS: %s:inittls: ctx == NULL", who);

	/* already initialized? (we could re-init...) */
	if (*ctx != NULL)
		return TRUE;

	/* PRNG seeded? */
	if (!tls_rand_init(RandFile, 10))
		return FALSE;

	/* let's start with the assumption it will work */
	ok = TRUE;

# if _FFR_TLS_1
	/*
	**  look for a second filename: it must be separated by a ','
	**  no blanks allowed (they won't be skipped).
	**  we change a global variable here! this change will be undone
	**  before return from the function but only if it returns TRUE.
	**  this isn't a problem since in a failure case this function
	**  won't be called again with the same (overwritten) values.
	**  otherwise each return must be replaced with a goto endinittls.
	*/
	cf2 = NULL;
	kf2 = NULL;
	if (certfile != NULL && (cf2 = strchr(certfile, ',')) != NULL)
	{
		*cf2++ = '\0';
		if (keyfile != NULL && (kf2 = strchr(keyfile, ',')) != NULL)
			*kf2++ = '\0';
	}
# endif /* _FFR_TLS_1 */

	/*
	**  what do we require from the client?
	**  must it have CERTs?
	**  introduce an option and decide based on that
	*/

	TLS_OK_F(certfile, "CertFile", bitset(TLS_I_CERT_EX, req),
		 TLS_S_CERT_EX);
	TLS_OK_F(keyfile, "KeyFile", bitset(TLS_I_KEY_EX, req),
		 TLS_S_KEY_EX);
	TLS_OK_F(cacertpath, "CACERTPath", bitset(TLS_I_CERTP_EX, req),
		 TLS_S_CERTP_EX);
	TLS_OK_F(cacertfile, "CACERTFile", bitset(TLS_I_CERTF_EX, req),
		 TLS_S_CERTF_EX);

# if _FFR_TLS_1
	if (cf2 != NULL)
	{
		TLS_OK_F(cf2, "CertFile", bitset(TLS_I_CERT_EX, req),
			 TLS_S_CERT2_EX);
	}
	if (kf2 != NULL)
	{
		TLS_OK_F(kf2, "KeyFile", bitset(TLS_I_KEY_EX, req),
			 TLS_S_KEY2_EX);
	}
# endif /* _FFR_TLS_1 */

	/*
	**  valid values for dhparam are (only the first char is checked)
	**  none	no parameters: don't use DH
	**  512		generate 512 bit parameters (fixed)
	**  1024	generate 1024 bit parameters
	**  /file/name	read parameters from /file/name
	**  default is: 1024 for server, 512 for client (OK? XXX)
	*/
	if (bitset(TLS_I_TRY_DH, req))
	{
		if (dhparam != NULL)
		{
			char c = *dhparam;

			if (c == '1')
				req |= TLS_I_DH1024;
			else if (c == '5')
				req |= TLS_I_DH512;
			else if (c != 'n' && c != 'N' && c != '/')
			{
				if (LogLevel > 12)
					sm_syslog(LOG_WARNING, NOQID,
						  "TLS: error: illegal value '%s' for DHParam",
						  dhparam);
				dhparam = NULL;
			}
		}
		if (dhparam == NULL)
			dhparam = srv ? "1" : "5";
		else if (*dhparam == '/')
		{
			TLS_OK_F(dhparam, "DHParameters",
				 bitset(TLS_I_DHPAR_EX, req),
				 TLS_S_DHPAR_EX);
		}
	}
	if (!ok)
		return ok;

	/* certfile etc. must be "safe". */
	sff = SFF_REGONLY | SFF_SAFEDIRPATH | SFF_NOWLINK
	     | SFF_NOGWFILES | SFF_NOWWFILES
	     | SFF_MUSTOWN | SFF_ROOTOK | SFF_OPENASROOT;
	if (DontLockReadFiles)
		sff |= SFF_NOLOCK;

	TLS_SAFE_F(certfile, sff | TLS_UNR(TLS_I_CERT_UNR, req),
		   bitset(TLS_I_CERT_EX, req),
		   bitset(TLS_S_CERT_EX, status), TLS_S_CERT_OK);
	TLS_SAFE_F(keyfile, sff | TLS_UNR(TLS_I_KEY_UNR, req),
		   bitset(TLS_I_KEY_EX, req),
		   bitset(TLS_S_KEY_EX, status), TLS_S_KEY_OK);
	TLS_SAFE_F(cacertfile, sff | TLS_UNR(TLS_I_CERTF_UNR, req),
		   bitset(TLS_I_CERTF_EX, req),
		   bitset(TLS_S_CERTF_EX, status), TLS_S_CERTF_OK);
	TLS_SAFE_F(dhparam, sff | TLS_UNR(TLS_I_DHPAR_UNR, req),
		   bitset(TLS_I_DHPAR_EX, req),
		   bitset(TLS_S_DHPAR_EX, status), TLS_S_DHPAR_OK);
	if (!ok)
		return ok;
# if _FFR_TLS_1
	if (cf2 != NULL)
	{
		TLS_SAFE_F(cf2, sff | TLS_UNR(TLS_I_CERT_UNR, req),
			   bitset(TLS_I_CERT_EX, req),
			   bitset(TLS_S_CERT2_EX, status), TLS_S_CERT2_OK);
	}
	if (kf2 != NULL)
	{
		TLS_SAFE_F(kf2, sff | TLS_UNR(TLS_I_KEY_UNR, req),
			   bitset(TLS_I_KEY_EX, req),
			   bitset(TLS_S_KEY2_EX, status), TLS_S_KEY2_OK);
	}
# endif /* _FFR_TLS_1 */

	/* create a method and a new context */
	if (srv)
	{
		if ((*ctx = SSL_CTX_new(SSLv23_server_method())) == NULL)
		{
			if (LogLevel > 7)
				sm_syslog(LOG_WARNING, NOQID,
					  "TLS: error: SSL_CTX_new(SSLv23_server_method()) failed");
			if (LogLevel > 9)
				tlslogerr();
			return FALSE;
		}
	}
	else
	{
		if ((*ctx = SSL_CTX_new(SSLv23_client_method())) == NULL)
		{
			if (LogLevel > 7)
				sm_syslog(LOG_WARNING, NOQID,
					  "TLS: error: SSL_CTX_new(SSLv23_client_method()) failed");
			if (LogLevel > 9)
				tlslogerr();
			return FALSE;
		}
	}

#  if TLS_NO_RSA
	/* turn off backward compatibility, required for no-rsa */
	SSL_CTX_set_options(*ctx, SSL_OP_NO_SSLv2);
#  endif /* TLS_NO_RSA */


#  if !TLS_NO_RSA
	/*
	**  Create a temporary RSA key
	**  XXX  Maybe we shouldn't create this always (even though it
	**  is only at startup).
	**  It is a time-consuming operation and it is not always necessary.
	**  maybe we should do it only on demand...
	*/
	if (bitset(TLS_I_RSA_TMP, req) &&
	    (rsa_tmp = RSA_generate_key(RSA_KEYLENGTH, RSA_F4, NULL,
					NULL)) == NULL
	   )
	{
		if (LogLevel > 7)
		{
			sm_syslog(LOG_WARNING, NOQID,
				  "TLS: error: %s: RSA_generate_key failed",
				  who);
			if (LogLevel > 9)
				tlslogerr();
		}
		return FALSE;
	}
#  endif /* !TLS_NO_RSA */

	/*
	**  load private key
	**  XXX change this for DSA-only version
	*/
	if (bitset(TLS_S_KEY_OK, status) &&
	    SSL_CTX_use_PrivateKey_file(*ctx, keyfile,
					 SSL_FILETYPE_PEM) <= 0)
	{
		if (LogLevel > 7)
		{
			sm_syslog(LOG_WARNING, NOQID,
				  "TLS: error: %s: SSL_CTX_use_PrivateKey_file(%s) failed",
				  who, keyfile);
			if (LogLevel > 9)
				tlslogerr();
		}
		if (bitset(TLS_I_USE_KEY, req))
			return FALSE;
	}

	/* get the certificate file */
	if (bitset(TLS_S_CERT_OK, status) &&
	    SSL_CTX_use_certificate_file(*ctx, certfile,
					 SSL_FILETYPE_PEM) <= 0)
	{
		if (LogLevel > 7)
		{
			sm_syslog(LOG_WARNING, NOQID,
				  "TLS: error: %s: SSL_CTX_use_certificate_file(%s) failed",
				  who, certfile);
			if (LogLevel > 9)
				tlslogerr();
		}
		if (bitset(TLS_I_USE_CERT, req))
			return FALSE;
	}

	/* check the private key */
	if (bitset(TLS_S_KEY_OK, status) &&
	    (r = SSL_CTX_check_private_key(*ctx)) <= 0)
	{
		/* Private key does not match the certificate public key */
		if (LogLevel > 5)
		{
			sm_syslog(LOG_WARNING, NOQID,
				  "TLS: error: %s: SSL_CTX_check_private_key failed(%s): %d",
				  who, keyfile, r);
			if (LogLevel > 9)
				tlslogerr();
		}
		if (bitset(TLS_I_USE_KEY, req))
			return FALSE;
	}

# if _FFR_TLS_1
	/* XXX this code is pretty much duplicated from above! */

	/* load private key */
	if (bitset(TLS_S_KEY2_OK, status) &&
	    SSL_CTX_use_PrivateKey_file(*ctx, kf2, SSL_FILETYPE_PEM) <= 0)
	{
		if (LogLevel > 7)
		{
			sm_syslog(LOG_WARNING, NOQID,
				  "TLS: error: %s: SSL_CTX_use_PrivateKey_file(%s) failed",
				  who, kf2);
			if (LogLevel > 9)
				tlslogerr();
		}
	}

	/* get the certificate file */
	if (bitset(TLS_S_CERT2_OK, status) &&
	    SSL_CTX_use_certificate_file(*ctx, cf2, SSL_FILETYPE_PEM) <= 0)
	{
		if (LogLevel > 7)
		{
			sm_syslog(LOG_WARNING, NOQID,
				  "TLS: error: %s: SSL_CTX_use_certificate_file(%s) failed",
				  who, cf2);
			if (LogLevel > 9)
				tlslogerr();
		}
	}

	/* we should also check the private key: */
	if (bitset(TLS_S_KEY2_OK, status) &&
	    (r = SSL_CTX_check_private_key(*ctx)) <= 0)
	{
		/* Private key does not match the certificate public key */
		if (LogLevel > 5)
		{
			sm_syslog(LOG_WARNING, NOQID,
				  "TLS: error: %s: SSL_CTX_check_private_key 2 failed: %d",
				  who, r);
			if (LogLevel > 9)
				tlslogerr();
		}
	}
# endif /* _FFR_TLS_1 */

	/* SSL_CTX_set_quiet_shutdown(*ctx, 1); violation of standard? */
	SSL_CTX_set_options(*ctx, SSL_OP_ALL);	/* XXX bug compatibility? */

# if !NO_DH
	/* Diffie-Hellman initialization */
	if (bitset(TLS_I_TRY_DH, req))
	{
		if (bitset(TLS_S_DHPAR_OK, status))
		{
			BIO *bio;

			if ((bio = BIO_new_file(dhparam, "r")) != NULL)
			{
				dh = PEM_read_bio_DHparams(bio, NULL, NULL, NULL);
				BIO_free(bio);
				if (dh == NULL && LogLevel > 7)
				{
					u_long err;

					err = ERR_get_error();
					sm_syslog(LOG_WARNING, NOQID,
						  "TLS: error: %s: cannot read DH parameters(%s): %s",
						  who, dhparam,
						  ERR_error_string(err, NULL));
					if (LogLevel > 9)
						tlslogerr();
				}
			}
			else
			{
				if (LogLevel > 5)
				{
					sm_syslog(LOG_WARNING, NOQID,
						  "TLS: error: %s: BIO_new_file(%s) failed",
						  who, dhparam);
					if (LogLevel > 9)
						tlslogerr();
				}
			}
		}
		if (dh == NULL && bitset(TLS_I_DH1024, req))
		{
			DSA *dsa;

			/* this takes a while! (7-130s on a 450MHz AMD K6-2) */
			dsa = DSA_generate_parameters(1024, NULL, 0, NULL,
						      NULL, 0, NULL);
			dh = DSA_dup_DH(dsa);
			DSA_free(dsa);
		}
		else
		if (dh == NULL && bitset(TLS_I_DH512, req))
			dh = get_dh512();

		if (dh == NULL)
		{
			if (LogLevel > 9)
			{
				u_long err;

				err = ERR_get_error();
				sm_syslog(LOG_WARNING, NOQID,
					  "TLS: error: %s: cannot read or set DH parameters(%s): %s",
					  who, dhparam,
					  ERR_error_string(err, NULL));
			}
			if (bitset(TLS_I_REQ_DH, req))
				return FALSE;
		}
		else
		{
			SSL_CTX_set_tmp_dh(*ctx, dh);

			/* important to avoid small subgroup attacks */
			SSL_CTX_set_options(*ctx, SSL_OP_SINGLE_DH_USE);
			if (LogLevel > 12)
				sm_syslog(LOG_INFO, NOQID,
					  "TLS: %s: Diffie-Hellman init, key=%d bit (%c)",
					  who, 8 * DH_size(dh), *dhparam);
			DH_free(dh);
		}
	}
# endif /* !NO_DH */


	/* XXX do we need this cache here? */
	if (bitset(TLS_I_CACHE, req))
		SSL_CTX_sess_set_cache_size(*ctx, 128);
	/* timeout? SSL_CTX_set_timeout(*ctx, TimeOut...); */

	/* load certificate locations and default CA paths */
	if (bitset(TLS_S_CERTP_EX, status) && bitset(TLS_S_CERTF_EX, status))
	{
		if ((r = SSL_CTX_load_verify_locations(*ctx, cacertfile,
						       cacertpath)) == 1)
		{
#  if !TLS_NO_RSA
			if (bitset(TLS_I_RSA_TMP, req))
				SSL_CTX_set_tmp_rsa_callback(*ctx, tmp_rsa_key);
#  endif /* !TLS_NO_RSA */

			/* ask to verify the peer */
			SSL_CTX_set_verify(*ctx, SSL_VERIFY_PEER, NULL);

			/* install verify callback */
			SSL_CTX_set_cert_verify_callback(*ctx, tls_verify_cb,
							 NULL);
			SSL_CTX_set_client_CA_list(*ctx,
				SSL_load_client_CA_file(cacertfile));
		}
		else
		{
			/*
			**  can't load CA data; do we care?
			**  the data is necessary to authenticate the client,
			**  which in turn would be necessary
			**  if we want to allow relaying based on it.
			*/
			if (LogLevel > 5)
			{
				sm_syslog(LOG_WARNING, NOQID,
					  "TLS: error: %s: %d load verify locs %s, %s",
					  who, r, cacertpath, cacertfile);
				if (LogLevel > 9)
					tlslogerr();
			}
			if (bitset(TLS_I_VRFY_LOC, req))
				return FALSE;
		}
	}

	/* XXX: make this dependent on an option? */
	if (tTd(96, 9))
		SSL_CTX_set_info_callback(*ctx, apps_ssl_info_cb);

#  if _FFR_TLS_1
	/*
	**  XXX install our own cipher list: option?
	*/
	if (CipherList != NULL && *CipherList != '\0')
	{
		if (SSL_CTX_set_cipher_list(*ctx, CipherList) <= 0)
		{
			if (LogLevel > 7)
			{
				sm_syslog(LOG_WARNING, NOQID,
					  "TLS: error: %s: SSL_CTX_set_cipher_list(%s) failed, list ignored",
					  who, CipherList);

				if (LogLevel > 9)
					tlslogerr();
			}
			/* failure if setting to this list is required? */
		}
	}
#  endif /* _FFR_TLS_1 */
	if (LogLevel > 12)
		sm_syslog(LOG_INFO, NOQID, "TLS: init(%s)=%d", who, ok);

# if _FFR_TLS_1
#  if 0
	/*
	**  this label is required if we want to have a "clean" exit
	**  see the comments above at the initialization of cf2
	*/
    endinittls:
#  endif /* 0 */

	/* undo damage to global variables */
	if (cf2 != NULL)
		*--cf2 = ',';
	if (kf2 != NULL)
		*--kf2 = ',';
# endif /* _FFR_TLS_1 */

	return ok;
}
/*
**  INITSRVTLS -- initialize server side TLS
**
**	Parameters:
**		none.
**
**	Returns:
**		succeeded?
**
**	Side Effects:
**		sets tls_ok_srv static, even when called from main()
*/

bool
initsrvtls()
{

	tls_ok_srv = inittls(&srv_ctx, TLS_I_SRV, TRUE, SrvCERTfile,
			     Srvkeyfile, CACERTpath, CACERTfile, DHParams);
	return tls_ok_srv;
}
/*
**  TLS_GET_INFO -- get information about TLS connection
**
**	Parameters:
**		ssl -- SSL connection structure
**		e -- current envelope
**		srv -- server or client
**		host -- hostname of other side
**		log -- log connection information?
**
**	Returns:
**		result of authentication.
**
**	Side Effects:
**		sets ${cipher}, ${tls_version}, ${verify}, ${cipher_bits},
**		${cert}
*/

int
tls_get_info(ssl, e, srv, host, log)
	SSL *ssl;
	ENVELOPE *e;
	bool srv;
	char *host;
	bool log;
{
	SSL_CIPHER *c;
	int b, r;
	char *s;
	char bitstr[16];
	X509 *cert;

	c = SSL_get_current_cipher(ssl);
	define(macid("{cipher}", NULL), newstr(SSL_CIPHER_get_name(c)), e);
	b = SSL_CIPHER_get_bits(c, &r);
	(void) snprintf(bitstr, sizeof bitstr, "%d", b);
	define(macid("{cipher_bits}", NULL), newstr(bitstr), e);
# if _FFR_TLS_1
	(void) snprintf(bitstr, sizeof bitstr, "%d", r);
	define(macid("{alg_bits}", NULL), newstr(bitstr), e);
# endif /* _FFR_TLS_1 */
	s = SSL_CIPHER_get_version(c);
	if (s == NULL)
		s = "UNKNOWN";
	define(macid("{tls_version}", NULL), newstr(s), e);

	cert = SSL_get_peer_certificate(ssl);
	if (log && LogLevel >= 14)
		sm_syslog(LOG_INFO, e->e_id,
			  "TLS: get_verify in %s: %ld get_peer: 0x%lx",
			  srv ? "srv" : "clt",
			  SSL_get_verify_result(ssl), (u_long) cert);
	if (cert != NULL)
	{
		char buf[MAXNAME];

		X509_NAME_oneline(X509_get_subject_name(cert),
				  buf, sizeof buf);
		define(macid("{cert_subject}", NULL),
			       newstr(xtextify(buf, "<>\")")), e);
		X509_NAME_oneline(X509_get_issuer_name(cert),
				  buf, sizeof buf);
		define(macid("{cert_issuer}", NULL),
		       newstr(xtextify(buf, "<>\")")), e);
# if _FFR_TLS_1
		X509_NAME_get_text_by_NID(X509_get_subject_name(cert),
					  NID_commonName, buf, sizeof buf);
		define(macid("{cn_subject}", NULL),
		       newstr(xtextify(buf, "<>\")")), e);
		X509_NAME_get_text_by_NID(X509_get_issuer_name(cert),
					  NID_commonName, buf, sizeof buf);
		define(macid("{cn_issuer}", NULL),
		       newstr(xtextify(buf, "<>\")")), e);
# endif /* _FFR_TLS_1 */
	}
	else
	{
		define(macid("{cert_subject}", NULL), "", e);
		define(macid("{cert_issuer}", NULL), "", e);
# if _FFR_TLS_1
		define(macid("{cn_subject}", NULL), "", e);
		define(macid("{cn_issuer}", NULL), "", e);
# endif /* _FFR_TLS_1 */
	}
	switch(SSL_get_verify_result(ssl))
	{
	  case X509_V_OK:
		if (cert != NULL)
		{
			s = "OK";
			r = TLS_AUTH_OK;
		}
		else
		{
			s = "NO";
			r = TLS_AUTH_NO;
		}
		break;
	  default:
		s = "FAIL";
		r = TLS_AUTH_FAIL;
		break;
	}
	define(macid("{verify}", NULL), newstr(s), e);
	if (cert != NULL)
		X509_free(cert);

	/* do some logging */
	if (log && LogLevel > 9)
	{
		char *vers, *s1, *s2, *bits;

		vers = macvalue(macid("{tls_version}", NULL), e);
		bits = macvalue(macid("{cipher_bits}", NULL), e);
		s1 = macvalue(macid("{verify}", NULL), e);
		s2 = macvalue(macid("{cipher}", NULL), e);
		sm_syslog(LOG_INFO, NOQID,
			  "TLS: connection %s %.64s, version=%.16s, verify=%.16s, cipher=%.64s, bits=%.6s",
			  srv ? "from" : "to",
			  host == NULL ? "none" : host,
			  vers == NULL ? "none" : vers,
			  s1 == NULL ? "none" : s1,
			  s2 == NULL ? "none" : s2,
			  bits == NULL ? "0" : bits);
		if (LogLevel > 11)
		{
			/*
			**  maybe run xuntextify on the strings?
			**  that is easier to read but makes it maybe a bit
			**  more complicated to figure out the right values
			**  for the access map...
			*/
			s1 = macvalue(macid("{cert_subject}", NULL), e);
			s2 = macvalue(macid("{cert_issuer}", NULL), e);
			sm_syslog(LOG_INFO, NOQID,
				  "TLS: %s cert subject:%.128s, cert issuer=%.128s",
				  srv ? "client" : "server",
				  s1 == NULL ? "none" : s1,
				  s2 == NULL ? "none" : s2);
		}
	}

	return r;
}

# if !TLS_NO_RSA
/*
**  TMP_RSA_KEY -- return temporary RSA key
**
**	Parameters:
**		s -- SSL connection structure
**		export --
**		keylength --
**
**	Returns:
**		temporary RSA key.
*/

/* ARGUSED0 */
static RSA *
tmp_rsa_key(s, export, keylength)
	SSL *s;
	int export;
	int keylength;
{
	return rsa_tmp;
}
# endif /* !TLS_NO_RSA */
/*
**  APPS_SSL_INFO_CB -- info callback for TLS connections
**
**	Parameters:
**		s -- SSL connection structure
**		where --
**		ret --
**
**	Returns:
**		none.
*/

void
apps_ssl_info_cb(s, where, ret)
	SSL *s;
	int where;
	int ret;
{
	char *str;
	int w;
	BIO *bio_err = NULL;

	if (LogLevel > 14)
		sm_syslog(LOG_INFO, NOQID,
			  "info_callback where 0x%x ret %d", where, ret);

	w = where & ~SSL_ST_MASK;
	if (bio_err == NULL)
		bio_err = BIO_new_fp(stderr, BIO_NOCLOSE);

	if (w & SSL_ST_CONNECT)
		str = "SSL_connect";
	else if (w & SSL_ST_ACCEPT)
		str = "SSL_accept";
	else
		str = "undefined";

	if (where & SSL_CB_LOOP)
	{
		if (LogLevel > 12)
			sm_syslog(LOG_NOTICE, NOQID,
			"%s:%s\n", str, SSL_state_string_long(s));
	}
	else if (where & SSL_CB_ALERT)
	{
		str = (where & SSL_CB_READ) ? "read" : "write";
		if (LogLevel > 12)
			sm_syslog(LOG_NOTICE, NOQID,
		"SSL3 alert %s:%s:%s\n",
			   str, SSL_alert_type_string_long(ret),
			   SSL_alert_desc_string_long(ret));
	}
	else if (where & SSL_CB_EXIT)
	{
		if (ret == 0)
		{
			if (LogLevel > 7)
				sm_syslog(LOG_WARNING, NOQID,
					"%s:failed in %s\n",
					str, SSL_state_string_long(s));
		}
		else if (ret < 0)
		{
			if (LogLevel > 7)
				sm_syslog(LOG_WARNING, NOQID,
					"%s:error in %s\n",
					str, SSL_state_string_long(s));
		}
	}
}
/*
**  TLS_VERIFY_LOG -- log verify error for TLS certificates
**
**	Parameters:
**		ok -- verify ok?
**		ctx -- x509 context
**
**	Returns:
**		0 -- fatal error
**		1 -- ok
*/

static int
tls_verify_log(ok, ctx)
	int ok;
	X509_STORE_CTX *ctx;
{
	SSL *ssl;
	X509 *cert;
	int reason, depth;
	char buf[512];

	cert = X509_STORE_CTX_get_current_cert(ctx);
	reason = X509_STORE_CTX_get_error(ctx);
	depth = X509_STORE_CTX_get_error_depth(ctx);
	ssl = (SSL *)X509_STORE_CTX_get_ex_data(ctx,
			SSL_get_ex_data_X509_STORE_CTX_idx());

	if (ssl == NULL)
	{
		/* internal error */
		sm_syslog(LOG_ERR, NOQID,
			  "TLS: internal error: tls_verify_cb: ssl == NULL");
		return 0;
	}

	X509_NAME_oneline(X509_get_subject_name(cert), buf, sizeof buf);
	sm_syslog(LOG_INFO, NOQID,
		  "TLS cert verify: depth=%d %s, state=%d, reason=%s\n",
		  depth, buf, ok, X509_verify_cert_error_string(reason));
	return 1;
}

/*
**  TLS_VERIFY_CB -- verify callback for TLS certificates
**
**	Parameters:
**		ctx -- x509 context
**
**	Returns:
**		accept connection?
**		currently: always yes.
*/

static int
tls_verify_cb(ctx)
	X509_STORE_CTX *ctx;
{
	int ok;

	ok = X509_verify_cert(ctx);
	if (ok == 0)
	{
		if (LogLevel > 13)
			return tls_verify_log(ok, ctx);
		return 1;	/* override it */
	}
	return ok;
}


/*
**  TLSLOGERR -- log the errors from the TLS error stack
**
**	Parameters:
**		none.
**
**	Returns:
**		none.
*/

void
tlslogerr()
{
	unsigned long l;
	int line, flags;
	unsigned long es;
	char *file, *data;
	char buf[256];
#define CP (const char **)

	es = CRYPTO_thread_id();
	while ((l = ERR_get_error_line_data(CP &file, &line, CP &data, &flags))
		!= 0)
	{
		sm_syslog(LOG_WARNING, NOQID,
			 "TLS: %lu:%s:%s:%d:%s\n", es, ERR_error_string(l, buf),
			 file, line, (flags & ERR_TXT_STRING) ? data : "");
	}
}

# endif /* STARTTLS */
#endif /* SMTP */
/*
**  HELP -- implement the HELP command.
**
**	Parameters:
**		topic -- the topic we want help for.
**		e -- envelope
**
**	Returns:
**		none.
**
**	Side Effects:
**		outputs the help file to message output.
*/
#define HELPVSTR	"#vers	"
#define HELPVERSION	2

void
help(topic, e)
	char *topic;
	ENVELOPE *e;
{
	register FILE *hf;
	register char *p;
	int len;
	bool noinfo;
	bool first = TRUE;
	long sff = SFF_OPENASROOT|SFF_REGONLY;
	char buf[MAXLINE];
	char inp[MAXLINE];
	static int foundvers = -1;
	extern char Version[];

	if (DontLockReadFiles)
		sff |= SFF_NOLOCK;
	if (!bitnset(DBS_HELPFILEINUNSAFEDIRPATH, DontBlameSendmail))
		sff |= SFF_SAFEDIRPATH;

	if (HelpFile == NULL ||
	    (hf = safefopen(HelpFile, O_RDONLY, 0444, sff)) == NULL)
	{
		/* no help */
		errno = 0;
		message("502 5.3.0 Sendmail %s -- HELP not implemented",
			Version);
		return;
	}

	if (topic == NULL || *topic == '\0')
	{
		topic = "smtp";
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
		if (buf[0] == '#')
		{
			if (foundvers < 0 &&
			    strncmp(buf, HELPVSTR, strlen(HELPVSTR)) == 0)
			{
				int h;

				if (sscanf(buf + strlen(HELPVSTR), "%d",
					   &h) == 1)
					foundvers = h;
			}
			continue;
		}
		if (strncmp(buf, topic, len) == 0)
		{
			if (first)
			{
				first = FALSE;

				/* print version if no/old vers# in file */
				if (foundvers < 2 && !noinfo)
					message("214-2.0.0 This is Sendmail version %s", Version);
			}
			p = strpbrk(buf, " \t");
			if (p == NULL)
				p = buf + strlen(buf) - 1;
			else
				p++;
			fixcrlf(p, TRUE);
			if (foundvers >= 2)
			{
				translate_dollars(p);
				expand(p, inp, sizeof inp, e);
				p = inp;
			}
			message("214-2.0.0 %s", p);
			noinfo = FALSE;
		}
	}

	if (noinfo)
		message("504 5.3.0 HELP topic \"%.10s\" unknown", topic);
	else
		message("214 2.0.0 End of HELP info");

	if (foundvers != 0 && foundvers < HELPVERSION)
	{
		if (LogLevel > 1)
			sm_syslog(LOG_WARNING, e->e_id,
				  "%s too old (require version %d)",
				  HelpFile, HELPVERSION);

		/* avoid log next time */
		foundvers = 0;
	}

	(void) fclose(hf);
}
