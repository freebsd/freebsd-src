/*
 * Copyright (c) 1998-2003 Sendmail, Inc. and its suppliers.
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
#if MILTER
# include <libmilter/mfdef.h>
#endif /* MILTER */

SM_RCSID("@(#)$Id: srvrsmtp.c,v 8.829.2.34 2004/01/14 19:13:46 ca Exp $")

#if SASL || STARTTLS
# include <sys/time.h>
# include "sfsasl.h"
#endif /* SASL || STARTTLS */
#if SASL
# define ENC64LEN(l)	(((l) + 2) * 4 / 3 + 1)
static int saslmechs __P((sasl_conn_t *, char **));
#endif /* SASL */
#if STARTTLS
# include <sysexits.h>

static SSL_CTX	*srv_ctx = NULL;	/* TLS server context */
static SSL	*srv_ssl = NULL;	/* per connection context */

static bool	tls_ok_srv = false;

extern void	tls_set_verify __P((SSL_CTX *, SSL *, bool));
# define TLS_VERIFY_CLIENT() tls_set_verify(srv_ctx, srv_ssl, \
				bitset(SRV_VRFY_CLT, features))
#endif /* STARTTLS */

/* server features */
#define SRV_NONE	0x0000	/* none... */
#define SRV_OFFER_TLS	0x0001	/* offer STARTTLS */
#define SRV_VRFY_CLT	0x0002	/* request a cert */
#define SRV_OFFER_AUTH	0x0004	/* offer AUTH */
#define SRV_OFFER_ETRN	0x0008	/* offer ETRN */
#define SRV_OFFER_VRFY	0x0010	/* offer VRFY (not yet used) */
#define SRV_OFFER_EXPN	0x0020	/* offer EXPN */
#define SRV_OFFER_VERB	0x0040	/* offer VERB */
#define SRV_OFFER_DSN	0x0080	/* offer DSN */
#if PIPELINING
# define SRV_OFFER_PIPE	0x0100	/* offer PIPELINING */
# if _FFR_NO_PIPE
#  define SRV_NO_PIPE	0x0200	/* disable PIPELINING, sleep if used */
# endif /* _FFR_NO_PIPE */
#endif /* PIPELINING */
#define SRV_REQ_AUTH	0x0400	/* require AUTH */
#define SRV_TMP_FAIL	0x1000	/* ruleset caused a temporary failure */

static unsigned int	srvfeatures __P((ENVELOPE *, char *, unsigned int));

static time_t	checksmtpattack __P((volatile unsigned int *, int, bool,
				     char *, ENVELOPE *));
static void	mail_esmtp_args __P((char *, char *, ENVELOPE *));
static void	printvrfyaddr __P((ADDRESS *, bool, bool));
static void	rcpt_esmtp_args __P((ADDRESS *, char *, char *, ENVELOPE *));
static char	*skipword __P((char *volatile, char *));
static void	setup_smtpd_io __P((void));

#if SASL
# if SASL >= 20000
static int reset_saslconn __P((sasl_conn_t **_conn, char *_hostname,
				char *_remoteip, char *_localip,
				char *_auth_id, sasl_ssf_t *_ext_ssf));

# define RESET_SASLCONN	\
	result = reset_saslconn(&conn, hostname, remoteip, localip, auth_id, \
				&ext_ssf);	\
	if (result != SASL_OK)			\
	{					\
		/* This is pretty fatal */	\
		goto doquit;			\
	}

# else /* SASL >= 20000 */
static int reset_saslconn __P((sasl_conn_t **_conn, char *_hostname,
				struct sockaddr_in *_saddr_r,
				struct sockaddr_in *_saddr_l,
				sasl_external_properties_t *_ext_ssf));
# define RESET_SASLCONN	\
	result = reset_saslconn(&conn, hostname, &saddr_r, &saddr_l, &ext_ssf); \
	if (result != SASL_OK)			\
	{					\
		/* This is pretty fatal */	\
		goto doquit;			\
	}

# endif /* SASL >= 20000 */
#endif /* SASL */

extern ENVELOPE	BlankEnvelope;

#define SKIP_SPACE(s)	while (isascii(*s) && isspace(*s))	\
				(s)++

/*
**  SMTP -- run the SMTP protocol.
**
**	Parameters:
**		nullserver -- if non-NULL, rejection message for
**			(almost) all SMTP commands.
**		d_flags -- daemon flags
**		e -- the envelope.
**
**	Returns:
**		never.
**
**	Side Effects:
**		Reads commands from the input channel and processes them.
*/

/*
**  Notice: The smtp server doesn't have a session context like the client
**	side has (mci). Therefore some data (session oriented) is allocated
**	or assigned to the "wrong" structure (esp. STARTTLS, AUTH).
**	This should be fixed in a successor version.
*/

struct cmd
{
	char	*cmd_name;	/* command name */
	int	cmd_code;	/* internal code, see below */
};

/* values for cmd_code */
#define CMDERROR	0	/* bad command */
#define CMDMAIL	1	/* mail -- designate sender */
#define CMDRCPT	2	/* rcpt -- designate recipient */
#define CMDDATA	3	/* data -- send message text */
#define CMDRSET	4	/* rset -- reset state */
#define CMDVRFY	5	/* vrfy -- verify address */
#define CMDEXPN	6	/* expn -- expand address */
#define CMDNOOP	7	/* noop -- do nothing */
#define CMDQUIT	8	/* quit -- close connection and die */
#define CMDHELO	9	/* helo -- be polite */
#define CMDHELP	10	/* help -- give usage info */
#define CMDEHLO	11	/* ehlo -- extended helo (RFC 1425) */
#define CMDETRN	12	/* etrn -- flush queue */
#if SASL
# define CMDAUTH	13	/* auth -- SASL authenticate */
#endif /* SASL */
#if STARTTLS
# define CMDSTLS	14	/* STARTTLS -- start TLS session */
#endif /* STARTTLS */
/* non-standard commands */
#define CMDVERB	17	/* verb -- go into verbose mode */
/* unimplemented commands from RFC 821 */
#define CMDUNIMPL	19	/* unimplemented rfc821 commands */
/* use this to catch and log "door handle" attempts on your system */
#define CMDLOGBOGUS	23	/* bogus command that should be logged */
/* debugging-only commands, only enabled if SMTPDEBUG is defined */
#define CMDDBGQSHOW	24	/* showq -- show send queue */
#define CMDDBGDEBUG	25	/* debug -- set debug mode */

/*
**  Note: If you change this list, remember to update 'helpfile'
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
	{ "send",	CMDUNIMPL	},
	{ "saml",	CMDUNIMPL	},
	{ "soml",	CMDUNIMPL	},
	{ "turn",	CMDUNIMPL	},
#if SASL
	{ "auth",	CMDAUTH,	},
#endif /* SASL */
#if STARTTLS
	{ "starttls",	CMDSTLS,	},
#endif /* STARTTLS */
    /* remaining commands are here only to trap and log attempts to use them */
	{ "showq",	CMDDBGQSHOW	},
	{ "debug",	CMDDBGDEBUG	},
	{ "wiz",	CMDLOGBOGUS	},

	{ NULL,		CMDERROR	}
};

static char	*CurSmtpClient;		/* who's at the other end of channel */

#ifndef MAXBADCOMMANDS
# define MAXBADCOMMANDS 25	/* maximum number of bad commands */
#endif /* ! MAXBADCOMMANDS */
#ifndef MAXNOOPCOMMANDS
# define MAXNOOPCOMMANDS 20	/* max "noise" commands before slowdown */
#endif /* ! MAXNOOPCOMMANDS */
#ifndef MAXHELOCOMMANDS
# define MAXHELOCOMMANDS 3	/* max HELO/EHLO commands before slowdown */
#endif /* ! MAXHELOCOMMANDS */
#ifndef MAXVRFYCOMMANDS
# define MAXVRFYCOMMANDS 6	/* max VRFY/EXPN commands before slowdown */
#endif /* ! MAXVRFYCOMMANDS */
#ifndef MAXETRNCOMMANDS
# define MAXETRNCOMMANDS 8	/* max ETRN commands before slowdown */
#endif /* ! MAXETRNCOMMANDS */
#ifndef MAXTIMEOUT
# define MAXTIMEOUT (4 * 60)	/* max timeout for bad commands */
#endif /* ! MAXTIMEOUT */

#if SM_HEAP_CHECK
static SM_DEBUG_T DebugLeakSmtp = SM_DEBUG_INITIALIZER("leak_smtp",
	"@(#)$Debug: leak_smtp - trace memory leaks during SMTP processing $");
#endif /* SM_HEAP_CHECK */

typedef struct
{
	bool	sm_gotmail;	/* mail command received */
	unsigned int sm_nrcpts;	/* number of successful RCPT commands */
#if _FFR_ADAPTIVE_EOL
WARNING: do NOT use this FFR, it is most likely broken
	bool	sm_crlf;	/* input in CRLF form? */
#endif /* _FFR_ADAPTIVE_EOL */
	bool	sm_discard;
#if MILTER
	bool	sm_milterize;
	bool	sm_milterlist;	/* any filters in the list? */
#endif /* MILTER */
#if _FFR_QUARANTINE
	char	*sm_quarmsg;	/* carry quarantining across messages */
#endif /* _FFR_QUARANTINE */
} SMTP_T;

static void	smtp_data __P((SMTP_T *, ENVELOPE *));

#define MSG_TEMPFAIL "451 4.7.1 Please try again later"

#if MILTER
# define MILTER_ABORT(e)	milter_abort((e))

#if _FFR_MILTER_421
# define MILTER_SHUTDOWN						\
			if (strncmp(response, "421 ", 4) == 0)		\
			{						\
				e->e_sendqueue = NULL;			\
				goto doquit;				\
			}
#else /* _FFR_MILTER_421 */
# define MILTER_SHUTDOWN
#endif /* _FFR_MILTER_421 */

# define MILTER_REPLY(str)						\
	{								\
		int savelogusrerrs = LogUsrErrs;			\
									\
		switch (state)						\
		{							\
		  case SMFIR_REPLYCODE:					\
			if (MilterLogLevel > 3)				\
			{						\
				sm_syslog(LOG_INFO, e->e_id,		\
					  "Milter: %s=%s, reject=%s",	\
					  str, addr, response);		\
				LogUsrErrs = false;			\
			}						\
			usrerr(response);				\
			MILTER_SHUTDOWN					\
			break;						\
									\
		  case SMFIR_REJECT:					\
			if (MilterLogLevel > 3)				\
			{						\
				sm_syslog(LOG_INFO, e->e_id,		\
					  "Milter: %s=%s, reject=550 5.7.1 Command rejected", \
					  str, addr);			\
				LogUsrErrs = false;			\
			}						\
			usrerr("550 5.7.1 Command rejected");		\
			break;						\
									\
		  case SMFIR_DISCARD:					\
			if (MilterLogLevel > 3)				\
				sm_syslog(LOG_INFO, e->e_id,		\
					  "Milter: %s=%s, discard",	\
					  str, addr);			\
			e->e_flags |= EF_DISCARD;			\
			break;						\
									\
		  case SMFIR_TEMPFAIL:					\
			if (MilterLogLevel > 3)				\
			{						\
				sm_syslog(LOG_INFO, e->e_id,		\
					  "Milter: %s=%s, reject=%s",	\
					  str, addr, MSG_TEMPFAIL);	\
				LogUsrErrs = false;			\
			}						\
			usrerr(MSG_TEMPFAIL);				\
			break;						\
		}							\
		LogUsrErrs = savelogusrerrs;				\
		if (response != NULL)					\
			sm_free(response); /* XXX */			\
	}

#else /* MILTER */
# define MILTER_ABORT(e)
#endif /* MILTER */

/* clear all SMTP state (for HELO/EHLO/RSET) */
#define CLEAR_STATE(cmd)					\
{								\
	/* abort milter filters */				\
	MILTER_ABORT(e);					\
								\
	if (smtp.sm_nrcpts > 0)					\
	{							\
		logundelrcpts(e, cmd, 10, false);		\
		smtp.sm_nrcpts = 0;				\
		macdefine(&e->e_macro, A_PERM,			\
			  macid("{nrcpts}"), "0");		\
	}							\
								\
	e->e_sendqueue = NULL;					\
	e->e_flags |= EF_CLRQUEUE;				\
								\
	if (LogLevel > 4 && bitset(EF_LOGSENDER, e->e_flags))	\
		logsender(e, NULL);				\
	e->e_flags &= ~EF_LOGSENDER;				\
								\
	/* clean up a bit */					\
	smtp.sm_gotmail = false;				\
	SuprErrs = true;					\
	dropenvelope(e, true, false);				\
	sm_rpool_free(e->e_rpool);				\
	e = newenvelope(e, CurEnv, sm_rpool_new_x(NULL));	\
	CurEnv = e;						\
}

/* sleep to flatten out connection load */
#define MIN_DELAY_LOG	15	/* wait before logging this again */

/* is it worth setting the process title for 1s? */
#define DELAY_CONN(cmd)						\
	if (DelayLA > 0 && (CurrentLA = getla()) >= DelayLA)	\
	{							\
		time_t dnow;					\
								\
		sm_setproctitle(true, e,			\
				"%s: %s: delaying %s: load average: %d", \
				qid_printname(e), CurSmtpClient,	\
				cmd, DelayLA);	\
		if (LogLevel > 8 && (dnow = curtime()) > log_delay)	\
		{						\
			sm_syslog(LOG_INFO, e->e_id,		\
				  "delaying=%s, load average=%d >= %d",	\
				  cmd, CurrentLA, DelayLA);		\
			log_delay = dnow + MIN_DELAY_LOG;	\
		}						\
		(void) sleep(1);				\
		sm_setproctitle(true, e, "%s %s: %.80s",	\
				qid_printname(e), CurSmtpClient, inp);	\
	}


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
	volatile bool gothello;		/* helo command received */
	bool vrfy;			/* set if this is a vrfy command */
	char *volatile protocol;	/* sending protocol */
	char *volatile sendinghost;	/* sending hostname */
	char *volatile peerhostname;	/* name of SMTP peer or "localhost" */
	auto char *delimptr;
	char *id;
	volatile unsigned int n_badcmds = 0;	/* count of bad commands */
	volatile unsigned int n_badrcpts = 0;	/* number of rejected RCPT */
	volatile unsigned int n_verifies = 0;	/* count of VRFY/EXPN */
	volatile unsigned int n_etrn = 0;	/* count of ETRN */
	volatile unsigned int n_noop = 0;	/* count of NOOP/VERB/etc */
	volatile unsigned int n_helo = 0;	/* count of HELO/EHLO */
	volatile int save_sevenbitinput;
	bool ok;
#if _FFR_BLOCK_PROXIES || _FFR_ADAPTIVE_EOL
	volatile bool first;
#endif /* _FFR_BLOCK_PROXIES || _FFR_ADAPTIVE_EOL */
	volatile bool tempfail = false;
	volatile time_t wt;		/* timeout after too many commands */
	volatile time_t previous;	/* time after checksmtpattack() */
	volatile bool lognullconnection = true;
	register char *q;
	SMTP_T smtp;
	char *addr;
	char *greetcode = "220";
	char *hostname;			/* my hostname ($j) */
	QUEUE_CHAR *new;
	int argno;
	char *args[MAXSMTPARGS];
	char inp[MAXLINE];
	char cmdbuf[MAXLINE];
#if SASL
	sasl_conn_t *conn;
	volatile bool sasl_ok;
	volatile unsigned int n_auth = 0;	/* count of AUTH commands */
	bool ismore;
	int result;
	volatile int authenticating;
	char *user;
	char *in, *out2;
# if SASL >= 20000
	char *auth_id;
	const char *out;
	sasl_ssf_t ext_ssf;
	char localip[60], remoteip[60];
# else /* SASL >= 20000 */
	char *out;
	const char *errstr;
	sasl_external_properties_t ext_ssf;
	struct sockaddr_in saddr_l;
	struct sockaddr_in saddr_r;
# endif /* SASL >= 20000 */
	sasl_security_properties_t ssp;
	sasl_ssf_t *ssf;
	unsigned int inlen, out2len;
	unsigned int outlen;
	char *volatile auth_type;
	char *mechlist;
	volatile unsigned int n_mechs;
	unsigned int len;
#endif /* SASL */
#if STARTTLS
	int r;
	int rfd, wfd;
	volatile bool tls_active = false;
# if _FFR_SMTP_SSL
	volatile bool smtps = false;
# endif /* _FFR_SMTP_SSL */
	bool saveQuickAbort;
	bool saveSuprErrs;
	time_t tlsstart;
#endif /* STARTTLS */
	volatile unsigned int features;
#if PIPELINING
# if _FFR_NO_PIPE
	int np_log = 0;
# endif /* _FFR_NO_PIPE */
#endif /* PIPELINING */
	volatile time_t log_delay = (time_t) 0;

	save_sevenbitinput = SevenBitInput;
	smtp.sm_nrcpts = 0;
#if MILTER
	smtp.sm_milterize = (nullserver == NULL);
	smtp.sm_milterlist = false;
#endif /* MILTER */

	/* setup I/O fd correctly for the SMTP server */
	setup_smtpd_io();

#if SM_HEAP_CHECK
	if (sm_debug_active(&DebugLeakSmtp, 1))
	{
		sm_heap_newgroup();
		sm_dprintf("smtp() heap group #%d\n", sm_heap_group());
	}
#endif /* SM_HEAP_CHECK */

	/* XXX the rpool should be set when e is initialized in main() */
	e->e_rpool = sm_rpool_new_x(NULL);
	e->e_macro.mac_rpool = e->e_rpool;

	settime(e);
	sm_getla();
	peerhostname = RealHostName;
	if (peerhostname == NULL)
		peerhostname = "localhost";
	CurHostName = peerhostname;
	CurSmtpClient = macvalue('_', e);
	if (CurSmtpClient == NULL)
		CurSmtpClient = CurHostName;

	/* check_relay may have set discard bit, save for later */
	smtp.sm_discard = bitset(EF_DISCARD, e->e_flags);

#if PIPELINING
	/* auto-flush output when reading input */
	(void) sm_io_autoflush(InChannel, OutChannel);
#endif /* PIPELINING */

	sm_setproctitle(true, e, "server %s startup", CurSmtpClient);

	/* Set default features for server. */
	features = ((bitset(PRIV_NOETRN, PrivacyFlags) ||
		     bitnset(D_NOETRN, d_flags)) ? SRV_NONE : SRV_OFFER_ETRN)
		| (bitnset(D_AUTHREQ, d_flags) ? SRV_REQ_AUTH : SRV_NONE)
		| (bitset(PRIV_NOEXPN, PrivacyFlags) ? SRV_NONE
			: (SRV_OFFER_EXPN
			  | (bitset(PRIV_NOVERB, PrivacyFlags)
			     ? SRV_NONE : SRV_OFFER_VERB)))
		| (bitset(PRIV_NORECEIPTS, PrivacyFlags) ? SRV_NONE
							 : SRV_OFFER_DSN)
#if SASL
		| (bitnset(D_NOAUTH, d_flags) ? SRV_NONE : SRV_OFFER_AUTH)
#endif /* SASL */
#if PIPELINING
		| SRV_OFFER_PIPE
#endif /* PIPELINING */
#if STARTTLS
		| (bitnset(D_NOTLS, d_flags) ? SRV_NONE : SRV_OFFER_TLS)
		| (bitset(TLS_I_NO_VRFY, TLS_Srv_Opts) ? SRV_NONE
						       : SRV_VRFY_CLT)
#endif /* STARTTLS */
		;
	if (nullserver == NULL)
	{
		features = srvfeatures(e, CurSmtpClient, features);
		if (bitset(SRV_TMP_FAIL, features))
		{
			if (LogLevel > 4)
				sm_syslog(LOG_ERR, NOQID,
					  "ERROR: srv_features=tempfail, relay=%.100s, access temporarily disabled",
					  CurSmtpClient);
			nullserver = "450 4.3.0 Please try again later.";
		}
#if PIPELINING
# if _FFR_NO_PIPE
		else if (bitset(SRV_NO_PIPE, features))
		{
			/* for consistency */
			features &= ~SRV_OFFER_PIPE;
		}
# endif /* _FFR_NO_PIPE */
#endif /* PIPELINING */
	}

	hostname = macvalue('j', e);
#if SASL
	sasl_ok = bitset(SRV_OFFER_AUTH, features);
	n_mechs = 0;
	authenticating = SASL_NOT_AUTH;

	/* SASL server new connection */
	if (sasl_ok)
	{
# if SASL >= 20000
		result = sasl_server_new("smtp", hostname, NULL, NULL, NULL,
					 NULL, 0, &conn);
# elif SASL > 10505
		/* use empty realm: only works in SASL > 1.5.5 */
		result = sasl_server_new("smtp", hostname, "", NULL, 0, &conn);
# else /* SASL >= 20000 */
		/* use no realm -> realm is set to hostname by SASL lib */
		result = sasl_server_new("smtp", hostname, NULL, NULL, 0,
					 &conn);
# endif /* SASL >= 20000 */
		sasl_ok = result == SASL_OK;
		if (!sasl_ok)
		{
			if (LogLevel > 9)
				sm_syslog(LOG_WARNING, NOQID,
					  "AUTH error: sasl_server_new failed=%d",
					  result);
		}
	}
	if (sasl_ok)
	{
		/*
		**  SASL set properties for sasl
		**  set local/remote IP
		**  XXX Cyrus SASL v1 only supports IPv4
		**
		**  XXX where exactly are these used/required?
		**  Kerberos_v4
		*/

# if SASL >= 20000
#  if NETINET || NETINET6
		in = macvalue(macid("{daemon_family}"), e);
		if (in != NULL && (
#   if NETINET6
		    strcmp(in, "inet6") == 0 ||
#   endif /* NETINET6 */
		    strcmp(in, "inet") == 0))
		{
			SOCKADDR_LEN_T addrsize;
			SOCKADDR saddr_l;
			SOCKADDR saddr_r;

			addrsize = sizeof(saddr_r);
			if (getpeername(sm_io_getinfo(InChannel, SM_IO_WHAT_FD,
						      NULL),
					(struct sockaddr *) &saddr_r,
					&addrsize) == 0)
			{
				if (iptostring(&saddr_r, addrsize,
					       remoteip, sizeof remoteip))
				{
					sasl_setprop(conn, SASL_IPREMOTEPORT,
						     remoteip);
				}
				addrsize = sizeof(saddr_l);
				if (getsockname(sm_io_getinfo(InChannel,
							      SM_IO_WHAT_FD,
							      NULL),
						(struct sockaddr *) &saddr_l,
						&addrsize) == 0)
				{
					if (iptostring(&saddr_l, addrsize,
						       localip,
						       sizeof localip))
					{
						sasl_setprop(conn,
							     SASL_IPLOCALPORT,
							     localip);
					}
				}
			}
		}
#  endif /* NETINET || NETINET6 */
# else /* SASL >= 20000 */
#  if NETINET
		in = macvalue(macid("{daemon_family}"), e);
		if (in != NULL && strcmp(in, "inet") == 0)
		{
			SOCKADDR_LEN_T addrsize;

			addrsize = sizeof(struct sockaddr_in);
			if (getpeername(sm_io_getinfo(InChannel, SM_IO_WHAT_FD,
						      NULL),
					(struct sockaddr *)&saddr_r,
					&addrsize) == 0)
			{
				sasl_setprop(conn, SASL_IP_REMOTE, &saddr_r);
				addrsize = sizeof(struct sockaddr_in);
				if (getsockname(sm_io_getinfo(InChannel,
							      SM_IO_WHAT_FD,
							      NULL),
						(struct sockaddr *)&saddr_l,
						&addrsize) == 0)
					sasl_setprop(conn, SASL_IP_LOCAL,
						     &saddr_l);
			}
		}
#  endif /* NETINET */
# endif /* SASL >= 20000 */

		auth_type = NULL;
		mechlist = NULL;
		user = NULL;
# if 0
		macdefine(&BlankEnvelope.e_macro, A_PERM,
			macid("{auth_author}"), NULL);
# endif /* 0 */

		/* set properties */
		(void) memset(&ssp, '\0', sizeof ssp);

		/* XXX should these be options settable via .cf ? */
		/* ssp.min_ssf = 0; is default due to memset() */
# if STARTTLS
# endif /* STARTTLS */
		{
			ssp.max_ssf = MaxSLBits;
			ssp.maxbufsize = MAXOUTLEN;
		}
		ssp.security_flags = SASLOpts & SASL_SEC_MASK;
		sasl_ok = sasl_setprop(conn, SASL_SEC_PROPS, &ssp) == SASL_OK;

		if (sasl_ok)
		{
			/*
			**  external security strength factor;
			**	currently we have none so zero
			*/

# if SASL >= 20000
			ext_ssf = 0;
			auth_id = NULL;
			sasl_ok = ((sasl_setprop(conn, SASL_SSF_EXTERNAL,
						 &ext_ssf) == SASL_OK) &&
				   (sasl_setprop(conn, SASL_AUTH_EXTERNAL,
						 auth_id) == SASL_OK));
# else /* SASL >= 20000 */
			ext_ssf.ssf = 0;
			ext_ssf.auth_id = NULL;
			sasl_ok = sasl_setprop(conn, SASL_SSF_EXTERNAL,
					       &ext_ssf) == SASL_OK;
# endif /* SASL >= 20000 */
		}
		if (sasl_ok)
			n_mechs = saslmechs(conn, &mechlist);
	}
#endif /* SASL */

#if STARTTLS
#endif /* STARTTLS */

#if MILTER
	if (smtp.sm_milterize)
	{
		char state;

		/* initialize mail filter connection */
		smtp.sm_milterlist = milter_init(e, &state);
		switch (state)
		{
		  case SMFIR_REJECT:
			if (MilterLogLevel > 3)
				sm_syslog(LOG_INFO, e->e_id,
					  "Milter: initialization failed, rejecting commands");
			greetcode = "554";
			nullserver = "Command rejected";
			smtp.sm_milterize = false;
			break;

		  case SMFIR_TEMPFAIL:
			if (MilterLogLevel > 3)
				sm_syslog(LOG_INFO, e->e_id,
					  "Milter: initialization failed, temp failing commands");
			tempfail = true;
			smtp.sm_milterize = false;
			break;
		}
	}

	if (smtp.sm_milterlist && smtp.sm_milterize &&
	    !bitset(EF_DISCARD, e->e_flags))
	{
		char state;
		char *response;

		response = milter_connect(peerhostname, RealHostAddr,
					  e, &state);
		switch (state)
		{
		  case SMFIR_REPLYCODE:	/* REPLYCODE shouldn't happen */
		  case SMFIR_REJECT:
			if (MilterLogLevel > 3)
				sm_syslog(LOG_INFO, e->e_id,
					  "Milter: connect: host=%s, addr=%s, rejecting commands",
					  peerhostname,
					  anynet_ntoa(&RealHostAddr));
			greetcode = "554";
			nullserver = "Command rejected";
			smtp.sm_milterize = false;
			break;

		  case SMFIR_TEMPFAIL:
			if (MilterLogLevel > 3)
				sm_syslog(LOG_INFO, e->e_id,
					  "Milter: connect: host=%s, addr=%s, temp failing commands",
					  peerhostname,
					  anynet_ntoa(&RealHostAddr));
			tempfail = true;
			smtp.sm_milterize = false;
			break;

#if _FFR_MILTER_421
		  case SMFIR_SHUTDOWN:
			if (MilterLogLevel > 3)
				sm_syslog(LOG_INFO, e->e_id,
					  "Milter: connect: host=%s, addr=%s, shutdown",
					  peerhostname,
					  anynet_ntoa(&RealHostAddr));
			tempfail = true;
			smtp.sm_milterize = false;
			message("421 4.7.0 %s closing connection",
					MyHostName);

			/* arrange to ignore send list */
			e->e_sendqueue = NULL;
			goto doquit;
#endif /* _FFR_MILTER_421 */
		}
		if (response != NULL)

			sm_free(response); /* XXX */
	}
#endif /* MILTER */

#if STARTTLS
# if _FFR_SMTP_SSL
	/* If this an smtps connection, start TLS now */
	smtps = bitnset(D_SMTPS, d_flags);
	if (smtps)
	{
		Errors = 0;
		goto starttls;
	}

  greeting:

# endif /* _FFR_SMTP_SSL */
#endif /* STARTTLS */

	/* output the first line, inserting "ESMTP" as second word */
	if (*greetcode == '5')
		(void) sm_snprintf(inp, sizeof inp, "%s not accepting messages",
				   hostname);
	else
		expand(SmtpGreeting, inp, sizeof inp, e);

	p = strchr(inp, '\n');
	if (p != NULL)
		*p++ = '\0';
	id = strchr(inp, ' ');
	if (id == NULL)
		id = &inp[strlen(inp)];
	if (p == NULL)
		(void) sm_snprintf(cmdbuf, sizeof cmdbuf,
			 "%s %%.*s ESMTP%%s", greetcode);
	else
		(void) sm_snprintf(cmdbuf, sizeof cmdbuf,
			 "%s-%%.*s ESMTP%%s", greetcode);
	message(cmdbuf, (int) (id - inp), inp, id);

	/* output remaining lines */
	while ((id = p) != NULL && (p = strchr(id, '\n')) != NULL)
	{
		*p++ = '\0';
		if (isascii(*id) && isspace(*id))
			id++;
		(void) sm_strlcpyn(cmdbuf, sizeof cmdbuf, 2, greetcode, "-%s");
		message(cmdbuf, id);
	}
	if (id != NULL)
	{
		if (isascii(*id) && isspace(*id))
			id++;
		(void) sm_strlcpyn(cmdbuf, sizeof cmdbuf, 2, greetcode, " %s");
		message(cmdbuf, id);
	}

	protocol = NULL;
	sendinghost = macvalue('s', e);

#if _FFR_QUARANTINE
	/* If quarantining by a connect/ehlo action, save between messages */
	if (e->e_quarmsg == NULL)
		smtp.sm_quarmsg = NULL;
	else
		smtp.sm_quarmsg = newstr(e->e_quarmsg);
#endif /* _FFR_QUARANTINE */

	/* sendinghost's storage must outlive the current envelope */
	if (sendinghost != NULL)
		sendinghost = sm_strdup_x(sendinghost);
#if _FFR_BLOCK_PROXIES || _FFR_ADAPTIVE_EOL
	first = true;
#endif /* _FFR_BLOCK_PROXIES || _FFR_ADAPTIVE_EOL */
	gothello = false;
	smtp.sm_gotmail = false;
	for (;;)
	{
	    SM_TRY
	    {
		QuickAbort = false;
		HoldErrs = false;
		SuprErrs = false;
		LogUsrErrs = false;
		OnlyOneError = true;
		e->e_flags &= ~(EF_VRFYONLY|EF_GLOBALERRS);

		/* setup for the read */
		e->e_to = NULL;
		Errors = 0;
		FileName = NULL;
		(void) sm_io_flush(smioout, SM_TIME_DEFAULT);

		/* read the input line */
		SmtpPhase = "server cmd read";
		sm_setproctitle(true, e, "server %s cmd read", CurSmtpClient);
#if SASL
		/*
		**  XXX SMTP AUTH requires accepting any length,
		**	at least for challenge/response
		*/
#endif /* SASL */

		/* handle errors */
		if (sm_io_error(OutChannel) ||
		    (p = sfgets(inp, sizeof inp, InChannel,
				TimeOuts.to_nextcommand, SmtpPhase)) == NULL)
		{
			char *d;

			d = macvalue(macid("{daemon_name}"), e);
			if (d == NULL)
				d = "stdin";
			/* end of file, just die */
			disconnect(1, e);

#if MILTER
			/* close out milter filters */
			milter_quit(e);
#endif /* MILTER */

			message("421 4.4.1 %s Lost input channel from %s",
				MyHostName, CurSmtpClient);
			if (LogLevel > (smtp.sm_gotmail ? 1 : 19))
				sm_syslog(LOG_NOTICE, e->e_id,
					  "lost input channel from %s to %s after %s",
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

#if _FFR_BLOCK_PROXIES || _FFR_ADAPTIVE_EOL
		if (first)
		{
#if _FFR_BLOCK_PROXIES
			size_t inplen, cmdlen;
			int idx;
			char *http_cmd;
			static char *http_cmds[] = { "GET", "POST",
						     "CONNECT", "USER", NULL };

			inplen = strlen(inp);
			for (idx = 0; (http_cmd = http_cmds[idx]) != NULL;
			     idx++)
			{
				cmdlen = strlen(http_cmd);
				if (cmdlen < inplen &&
				    sm_strncasecmp(inp, http_cmd, cmdlen) == 0 &&
				    isascii(inp[cmdlen]) && isspace(inp[cmdlen]))
				{
					/* Open proxy, drop it */
					message("421 4.7.0 %s Rejecting open proxy %s",
						MyHostName, CurSmtpClient);
					sm_syslog(LOG_INFO, e->e_id,
						  "%s: probable open proxy: command=%.40s",
						  CurSmtpClient, inp);
					goto doquit;
				}
			}
#endif /* _FFR_BLOCK_PROXIES */
#if _FFR_ADAPTIVE_EOL
			char *p;

			smtp.sm_crlf = true;
			p = strchr(inp, '\n');
			if (p == NULL || p <= inp || p[-1] != '\r')
			{
				smtp.sm_crlf = false;
				if (tTd(66, 1) && LogLevel > 8)
				{
					/* how many bad guys are there? */
					sm_syslog(LOG_INFO, NOQID,
						  "%s did not use CRLF",
						  CurSmtpClient);
				}
			}
#endif /* _FFR_ADAPTIVE_EOL */
			first = false;
		}
#endif /* _FFR_BLOCK_PROXIES || _FFR_ADAPTIVE_EOL */

		/* clean up end of line */
		fixcrlf(inp, true);

#if PIPELINING
# if _FFR_NO_PIPE
		/*
		**  if there is more input and pipelining is disabled:
		**	delay ... (and maybe discard the input?)
		**  XXX this doesn't really work, at least in tests using
		**  telnet SM_IO_IS_READABLE only returns 1 if there were
		**  more than 2 input lines available.
		*/

		if (bitset(SRV_NO_PIPE, features) &&
		    sm_io_getinfo(InChannel, SM_IO_IS_READABLE, NULL) > 0)
		{
			if (++np_log < 3)
				sm_syslog(LOG_INFO, NOQID,
					  "unauthorized PIPELINING, sleeping");
			sleep(1);
		}

# endif /* _FFR_NO_PIPE */
#endif /* PIPELINING */

#if SASL
		if (authenticating == SASL_PROC_AUTH)
		{
# if 0
			if (*inp == '\0')
			{
				authenticating = SASL_NOT_AUTH;
				message("501 5.5.2 missing input");
				RESET_SASLCONN;
				continue;
			}
# endif /* 0 */
			if (*inp == '*' && *(inp + 1) == '\0')
			{
				authenticating = SASL_NOT_AUTH;

				/* rfc 2254 4. */
				message("501 5.0.0 AUTH aborted");
				RESET_SASLCONN;
				continue;
			}

			/* could this be shorter? XXX */
# if SASL >= 20000
			in = xalloc(strlen(inp) + 1);
			result = sasl_decode64(inp, strlen(inp), in,
					       strlen(inp), &inlen);
# else /* SASL >= 20000 */
			out = xalloc(strlen(inp));
			result = sasl_decode64(inp, strlen(inp), out, &outlen);
# endif /* SASL >= 20000 */
			if (result != SASL_OK)
			{
				authenticating = SASL_NOT_AUTH;

				/* rfc 2254 4. */
				message("501 5.5.4 cannot decode AUTH parameter %s",
					inp);
# if SASL >= 20000
				sm_free(in);
# endif /* SASL >= 20000 */
				RESET_SASLCONN;
				continue;
			}

# if SASL >= 20000
			result = sasl_server_step(conn,	in, inlen,
						  &out, &outlen);
			sm_free(in);
# else /* SASL >= 20000 */
			result = sasl_server_step(conn,	out, outlen,
						  &out, &outlen, &errstr);
# endif /* SASL >= 20000 */

			/* get an OK if we're done */
			if (result == SASL_OK)
			{
  authenticated:
				message("235 2.0.0 OK Authenticated");
				authenticating = SASL_IS_AUTH;
				macdefine(&BlankEnvelope.e_macro, A_TEMP,
					macid("{auth_type}"), auth_type);

# if SASL >= 20000
				user = macvalue(macid("{auth_authen}"), e);

				/* get security strength (features) */
				result = sasl_getprop(conn, SASL_SSF,
						      (const void **) &ssf);
# else /* SASL >= 20000 */
				result = sasl_getprop(conn, SASL_USERNAME,
						      (void **)&user);
				if (result != SASL_OK)
				{
					user = "";
					macdefine(&BlankEnvelope.e_macro,
						  A_PERM,
						  macid("{auth_authen}"), NULL);
				}
				else
				{
					macdefine(&BlankEnvelope.e_macro,
						  A_TEMP,
						  macid("{auth_authen}"), user);
				}

# if 0
				/* get realm? */
				sasl_getprop(conn, SASL_REALM, (void **) &data);
# endif /* 0 */

				/* get security strength (features) */
				result = sasl_getprop(conn, SASL_SSF,
						      (void **) &ssf);
# endif /* SASL >= 20000 */
				if (result != SASL_OK)
				{
					macdefine(&BlankEnvelope.e_macro,
						  A_PERM,
						  macid("{auth_ssf}"), "0");
					ssf = NULL;
				}
				else
				{
					char pbuf[8];

					(void) sm_snprintf(pbuf, sizeof pbuf,
							   "%u", *ssf);
					macdefine(&BlankEnvelope.e_macro,
						  A_TEMP,
						  macid("{auth_ssf}"), pbuf);
					if (tTd(95, 8))
						sm_dprintf("AUTH auth_ssf: %u\n",
							   *ssf);
				}

				/*
				**  Only switch to encrypted connection
				**  if a security layer has been negotiated
				*/

				if (ssf != NULL && *ssf > 0)
				{
					/*
					**  Convert I/O layer to use SASL.
					**  If the call fails, the connection
					**  is aborted.
					*/

					if (sfdcsasl(&InChannel, &OutChannel,
						     conn) == 0)
					{
						/* restart dialogue */
						n_helo = 0;
# if PIPELINING
						(void) sm_io_autoflush(InChannel,
								       OutChannel);
# endif /* PIPELINING */
					}
					else
						syserr("503 5.3.3 SASL TLS failed");
				}

				/* NULL pointer ok since it's our function */
				if (LogLevel > 8)
					sm_syslog(LOG_INFO, NOQID,
						  "AUTH=server, relay=%s, authid=%.128s, mech=%.16s, bits=%d",
						  CurSmtpClient,
						  shortenstring(user, 128),
						  auth_type, *ssf);
			}
			else if (result == SASL_CONTINUE)
			{
				len = ENC64LEN(outlen);
				out2 = xalloc(len);
				result = sasl_encode64(out, outlen, out2, len,
						       &out2len);
				if (result != SASL_OK)
				{
					/* correct code? XXX */
					/* 454 Temp. authentication failure */
					message("454 4.5.4 Internal error: unable to encode64");
					if (LogLevel > 5)
						sm_syslog(LOG_WARNING, e->e_id,
							  "AUTH encode64 error [%d for \"%s\"]",
							  result, out);
					/* start over? */
					authenticating = SASL_NOT_AUTH;
				}
				else
				{
					message("334 %s", out2);
					if (tTd(95, 2))
						sm_dprintf("AUTH continue: msg='%s' len=%u\n",
							   out2, out2len);
				}
# if SASL >= 20000
				sm_free(out2);
# endif /* SASL >= 20000 */
			}
			else
			{
				/* not SASL_OK or SASL_CONT */
				message("535 5.7.0 authentication failed");
				if (LogLevel > 9)
					sm_syslog(LOG_WARNING, e->e_id,
						  "AUTH failure (%s): %s (%d) %s",
						  auth_type,
						  sasl_errstring(result, NULL,
								 NULL),
						  result,
# if SASL >= 20000
						  sasl_errdetail(conn));
# else /* SASL >= 20000 */
						  errstr == NULL ? "" : errstr);
# endif /* SASL >= 20000 */
				RESET_SASLCONN;
				authenticating = SASL_NOT_AUTH;
			}
		}
		else
		{
			/* don't want to do any of this if authenticating */
#endif /* SASL */

		/* echo command to transcript */
		if (e->e_xfp != NULL)
			(void) sm_io_fprintf(e->e_xfp, SM_TIME_DEFAULT,
					     "<<< %s\n", inp);

		if (LogLevel > 14)
			sm_syslog(LOG_INFO, e->e_id, "<-- %s", inp);

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
		SKIP_SPACE(p);

		/* decode command */
		for (c = CmdTab; c->cmd_name != NULL; c++)
		{
			if (sm_strcasecmp(c->cmd_name, cmdbuf) == 0)
				break;
		}

		/* reset errors */
		errno = 0;

		/* check whether a "non-null" command has been used */
		switch (c->cmd_code)
		{
#if SASL
		  case CMDAUTH:
			/* avoid information leak; take first two words? */
			q = "AUTH";
			break;
#endif /* SASL */

		  case CMDMAIL:
		  case CMDEXPN:
		  case CMDVRFY:
		  case CMDETRN:
			lognullconnection = false;
			/* FALLTHROUGH */
		  default:
			q = inp;
			break;
		}

		if (e->e_id == NULL)
			sm_setproctitle(true, e, "%s: %.80s",
					CurSmtpClient, q);
		else
			sm_setproctitle(true, e, "%s %s: %.80s",
					qid_printname(e),
					CurSmtpClient, q);

		/*
		**  Process command.
		**
		**	If we are running as a null server, return 550
		**	to almost everything.
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
			  case CMDERROR:
				/* process normally */
				break;

			  case CMDETRN:
				if (bitnset(D_ETRNONLY, d_flags) &&
				    nullserver == NULL)
					break;
				DELAY_CONN("ETRN");
				/* FALLTHROUGH */

			  default:
#if MAXBADCOMMANDS > 0
				/* theoretically this could overflow */
				if (nullserver != NULL &&
				    ++n_badcmds > MAXBADCOMMANDS)
				{
					message("421 4.7.0 %s Too many bad commands; closing connection",
						MyHostName);

					/* arrange to ignore send list */
					e->e_sendqueue = NULL;
					goto doquit;
				}
#endif /* MAXBADCOMMANDS > 0 */
				if (nullserver != NULL)
				{
					if (ISSMTPREPLY(nullserver))
						usrerr(nullserver);
					else
						usrerr("550 5.0.0 %s",
						       nullserver);
				}
				else
					usrerr("452 4.4.5 Insufficient disk space; try again later");
				continue;
			}
		}

		switch (c->cmd_code)
		{
#if SASL
		  case CMDAUTH: /* sasl */
			DELAY_CONN("AUTH");
			if (!sasl_ok || n_mechs <= 0)
			{
				message("503 5.3.3 AUTH not available");
				break;
			}
			if (authenticating == SASL_IS_AUTH)
			{
				message("503 5.5.0 Already Authenticated");
				break;
			}
			if (smtp.sm_gotmail)
			{
				message("503 5.5.0 AUTH not permitted during a mail transaction");
				break;
			}
			if (tempfail)
			{
				if (LogLevel > 9)
					sm_syslog(LOG_INFO, e->e_id,
						  "SMTP AUTH command (%.100s) from %s tempfailed (due to previous checks)",
						  p, CurSmtpClient);
				usrerr("454 4.7.1 Please try again later");
				break;
			}

			ismore = false;

			/* crude way to avoid crack attempts */
			(void) checksmtpattack(&n_auth, n_mechs + 1, true,
					       "AUTH", e);

			/* make sure mechanism (p) is a valid string */
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

			if (*p == '\0')
			{
				message("501 5.5.2 AUTH mechanism must be specified");
				break;
			}

			/* check whether mechanism is available */
			if (iteminlist(p, mechlist, " ") == NULL)
			{
				message("504 5.3.3 AUTH mechanism %.32s not available",
					p);
				break;
			}

			if (ismore)
			{
				/* could this be shorter? XXX */
# if SASL >= 20000
				in = xalloc(strlen(q) + 1);
				result = sasl_decode64(q, strlen(q), in,
						       strlen(q), &inlen);
# else /* SASL >= 20000 */
				in = sm_rpool_malloc(e->e_rpool, strlen(q));
				result = sasl_decode64(q, strlen(q), in,
						       &inlen);
# endif /* SASL >= 20000 */
				if (result != SASL_OK)
				{
					message("501 5.5.4 cannot BASE64 decode '%s'",
						q);
					if (LogLevel > 5)
						sm_syslog(LOG_WARNING, e->e_id,
							  "AUTH decode64 error [%d for \"%s\"]",
							  result, q);
					/* start over? */
					authenticating = SASL_NOT_AUTH;
# if SASL >= 20000
					sm_free(in);
# endif /* SASL >= 20000 */
					in = NULL;
					inlen = 0;
					break;
				}
			}
			else
			{
				in = NULL;
				inlen = 0;
			}

			/* see if that auth type exists */
# if SASL >= 20000
			result = sasl_server_start(conn, p, in, inlen,
						   &out, &outlen);
			if (in != NULL)
				sm_free(in);
# else /* SASL >= 20000 */
			result = sasl_server_start(conn, p, in, inlen,
						   &out, &outlen, &errstr);
# endif /* SASL >= 20000 */

			if (result != SASL_OK && result != SASL_CONTINUE)
			{
				message("535 5.7.0 authentication failed");
				if (LogLevel > 9)
					sm_syslog(LOG_ERR, e->e_id,
						  "AUTH failure (%s): %s (%d) %s",
						  p,
						  sasl_errstring(result, NULL,
								 NULL),
						  result,
# if SASL >= 20000
						  sasl_errdetail(conn));
# else /* SASL >= 20000 */
						  errstr);
# endif /* SASL >= 20000 */
				RESET_SASLCONN;
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
					       &out2len);

			if (result != SASL_OK)
			{
				message("454 4.5.4 Temporary authentication failure");
				if (LogLevel > 5)
					sm_syslog(LOG_WARNING, e->e_id,
						  "AUTH encode64 error [%d for \"%s\"]",
						  result, out);

				/* start over? */
				authenticating = SASL_NOT_AUTH;
				RESET_SASLCONN;
			}
			else
			{
				message("334 %s", out2);
				authenticating = SASL_PROC_AUTH;
			}
# if SASL >= 20000
			sm_free(out2);
# endif /* SASL >= 20000 */
			break;
#endif /* SASL */

#if STARTTLS
		  case CMDSTLS: /* starttls */
			DELAY_CONN("STARTTLS");
			if (*p != '\0')
			{
				message("501 5.5.2 Syntax error (no parameters allowed)");
				break;
			}
			if (!bitset(SRV_OFFER_TLS, features))
			{
				message("503 5.5.0 TLS not available");
				break;
			}
			if (!tls_ok_srv)
			{
				message("454 4.3.3 TLS not available after start");
				break;
			}
			if (smtp.sm_gotmail)
			{
				message("503 5.5.0 TLS not permitted during a mail transaction");
				break;
			}
			if (tempfail)
			{
				if (LogLevel > 9)
					sm_syslog(LOG_INFO, e->e_id,
						  "SMTP STARTTLS command (%.100s) from %s tempfailed (due to previous checks)",
						  p, CurSmtpClient);
				usrerr("454 4.7.1 Please try again later");
				break;
			}
# if _FFR_SMTP_SSL
  starttls:
# endif /* _FFR_SMTP_SSL */
# if TLS_NO_RSA
			/*
			**  XXX do we need a temp key ?
			*/
# else /* TLS_NO_RSA */
# endif /* TLS_NO_RSA */

# if TLS_VRFY_PER_CTX
			/*
			**  Note: this sets the verification globally
			**  (per SSL_CTX)
			**  it's ok since it applies only to one transaction
			*/

			TLS_VERIFY_CLIENT();
# endif /* TLS_VRFY_PER_CTX */

			if (srv_ssl != NULL)
				SSL_clear(srv_ssl);
			else if ((srv_ssl = SSL_new(srv_ctx)) == NULL)
			{
				message("454 4.3.3 TLS not available: error generating SSL handle");
				if (LogLevel > 8)
					tlslogerr("server");
# if _FFR_SMTP_SSL
				goto tls_done;
# else /* _FFR_SMTP_SSL */
				break;
# endif /* _FFR_SMTP_SSL */
			}

# if !TLS_VRFY_PER_CTX
			/*
			**  this could be used if it were possible to set
			**  verification per SSL (connection)
			**  not just per SSL_CTX (global)
			*/

			TLS_VERIFY_CLIENT();
# endif /* !TLS_VRFY_PER_CTX */

			rfd = sm_io_getinfo(InChannel, SM_IO_WHAT_FD, NULL);
			wfd = sm_io_getinfo(OutChannel, SM_IO_WHAT_FD, NULL);

			if (rfd < 0 || wfd < 0 ||
			    SSL_set_rfd(srv_ssl, rfd) <= 0 ||
			    SSL_set_wfd(srv_ssl, wfd) <= 0)
			{
				message("454 4.3.3 TLS not available: error set fd");
				SSL_free(srv_ssl);
				srv_ssl = NULL;
# if _FFR_SMTP_SSL
				goto tls_done;
# else /* _FFR_SMTP_SSL */
				break;
# endif /* _FFR_SMTP_SSL */
			}
# if _FFR_SMTP_SSL
			if (!smtps)
# endif /* _FFR_SMTP_SSL */
				message("220 2.0.0 Ready to start TLS");
# if PIPELINING
			(void) sm_io_flush(OutChannel, SM_TIME_DEFAULT);
# endif /* PIPELINING */

			SSL_set_accept_state(srv_ssl);

#  define SSL_ACC(s)	SSL_accept(s)

			tlsstart = curtime();
  ssl_retry:
			if ((r = SSL_ACC(srv_ssl)) <= 0)
			{
				int i;
				bool timedout;
				time_t left;
				time_t now = curtime();
				struct timeval tv;

				/* what to do in this case? */
				i = SSL_get_error(srv_ssl, r);

				/*
				**  For SSL_ERROR_WANT_{READ,WRITE}:
				**  There is no SSL record available yet
				**  or there is only a partial SSL record
				**  removed from the network (socket) buffer
				**  into the SSL buffer. The SSL_accept will
				**  only succeed when a full SSL record is
				**  available (assuming a "real" error
				**  doesn't happen). To handle when a "real"
				**  error does happen the select is set for
				**  exceptions too.
				**  The connection may be re-negotiated
				**  during this time so both read and write
				**  "want errors" need to be handled.
				**  A select() exception loops back so that
				**  a proper SSL error message can be gotten.
				*/

				left = TimeOuts.to_starttls - (now - tlsstart);
				timedout = left <= 0;
				if (!timedout)
				{
					tv.tv_sec = left;
					tv.tv_usec = 0;
				}

				if (!timedout && FD_SETSIZE > 0 &&
				    (rfd >= FD_SETSIZE ||
				     (i == SSL_ERROR_WANT_WRITE &&
				      wfd >= FD_SETSIZE)))
				{
					if (LogLevel > 5)
					{
						sm_syslog(LOG_ERR, NOQID,
							  "STARTTLS=server, error: fd %d/%d too large",
							  rfd, wfd);
						if (LogLevel > 8)
							tlslogerr("server");
					}
					goto tlsfail;
				}

				/* XXX what about SSL_pending() ? */
				if (!timedout && i == SSL_ERROR_WANT_READ)
				{
					fd_set ssl_maskr, ssl_maskx;

					FD_ZERO(&ssl_maskr);
					FD_SET(rfd, &ssl_maskr);
					FD_ZERO(&ssl_maskx);
					FD_SET(rfd, &ssl_maskx);
					if (select(rfd + 1, &ssl_maskr, NULL,
						   &ssl_maskx, &tv) > 0)
						goto ssl_retry;
				}
				if (!timedout && i == SSL_ERROR_WANT_WRITE)
				{
					fd_set ssl_maskw, ssl_maskx;

					FD_ZERO(&ssl_maskw);
					FD_SET(wfd, &ssl_maskw);
					FD_ZERO(&ssl_maskx);
					FD_SET(rfd, &ssl_maskx);
					if (select(wfd + 1, NULL, &ssl_maskw,
						   &ssl_maskx, &tv) > 0)
						goto ssl_retry;
				}
				if (LogLevel > 5)
				{
					sm_syslog(LOG_WARNING, NOQID,
						  "STARTTLS=server, error: accept failed=%d, SSL_error=%d, timedout=%d, errno=%d",
						  r, i, (int) timedout, errno);
					if (LogLevel > 8)
						tlslogerr("server");
				}
tlsfail:
				tls_ok_srv = false;
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
			(void) tls_get_info(srv_ssl, true,
					    CurSmtpClient,
					    &BlankEnvelope.e_macro,
					    bitset(SRV_VRFY_CLT, features));

			/*
			**  call Stls_client to find out whether
			**  to accept the connection from the client
			*/

			saveQuickAbort = QuickAbort;
			saveSuprErrs = SuprErrs;
			SuprErrs = true;
			QuickAbort = false;
			if (rscheck("tls_client",
				     macvalue(macid("{verify}"), e),
				     "STARTTLS", e,
				     RSF_RMCOMM|RSF_COUNT,
				     5, NULL, NOQID) != EX_OK ||
			    Errors > 0)
			{
				extern char MsgBuf[];

				if (MsgBuf[0] != '\0' && ISSMTPREPLY(MsgBuf))
					nullserver = newstr(MsgBuf);
				else
					nullserver = "503 5.7.0 Authentication required.";
			}
			QuickAbort = saveQuickAbort;
			SuprErrs = saveSuprErrs;

			tls_ok_srv = false;	/* don't offer STARTTLS again */
			n_helo = 0;
# if SASL
			if (sasl_ok)
			{
				char *s;

				s = macvalue(macid("{cipher_bits}"), e);
#  if SASL >= 20000
				if (s != NULL && (ext_ssf = atoi(s)) > 0)
				{
					auth_id = macvalue(macid("{cert_subject}"),
								   e);
					sasl_ok = ((sasl_setprop(conn, SASL_SSF_EXTERNAL,
								 &ext_ssf) == SASL_OK) &&
						   (sasl_setprop(conn, SASL_AUTH_EXTERNAL,
								 auth_id) == SASL_OK));
#  else /* SASL >= 20000 */
				if (s != NULL && (ext_ssf.ssf = atoi(s)) > 0)
				{
					ext_ssf.auth_id = macvalue(macid("{cert_subject}"),
								   e);
					sasl_ok = sasl_setprop(conn, SASL_SSF_EXTERNAL,
							       &ext_ssf) == SASL_OK;
#  endif /* SASL >= 20000 */
					mechlist = NULL;
					if (sasl_ok)
						n_mechs = saslmechs(conn,
								    &mechlist);
				}
			}
# endif /* SASL */

			/* switch to secure connection */
			if (sfdctls(&InChannel, &OutChannel, srv_ssl) == 0)
			{
				tls_active = true;
# if PIPELINING
				(void) sm_io_autoflush(InChannel, OutChannel);
# endif /* PIPELINING */
			}
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
				syserr("STARTTLS: can't switch to encrypted layer");
			}
# if _FFR_SMTP_SSL
		  tls_done:
			if (smtps)
			{
				if (tls_active)
					goto greeting;
				else
					goto doquit;
			}
# endif /* _FFR_SMTP_SSL */
			break;
#endif /* STARTTLS */

		  case CMDHELO:		/* hello -- introduce yourself */
		  case CMDEHLO:		/* extended hello */
			DELAY_CONN("EHLO");
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
			(void) checksmtpattack(&n_helo, MAXHELOCOMMANDS, true,
					       "HELO/EHLO", e);

#if 0
			/* RFC2821 4.1.4 allows duplicate HELO/EHLO */
			/* check for duplicate HELO/EHLO per RFC 1651 4.2 */
			if (gothello)
			{
				usrerr("503 %s Duplicate HELO/EHLO",
				       MyHostName);
				break;
			}
#endif /* 0 */

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
						  "invalid domain name (too long) from %s",
						  CurSmtpClient);
				break;
			}

			ok = true;
			for (q = p; *q != '\0'; q++)
			{
				if (!isascii(*q))
					break;
				if (isalnum(*q))
					continue;
				if (isspace(*q))
				{
					*q = '\0';

					/* only complain if strict check */
					ok = AllowBogusHELO;
					break;
				}
				if (strchr("[].-_#:", *q) == NULL)
					break;
			}

			if (*q == '\0' && ok)
			{
				q = "pleased to meet you";
				sendinghost = sm_strdup_x(p);
			}
			else if (!AllowBogusHELO)
			{
				usrerr("501 Invalid domain name");
				if (LogLevel > 9)
					sm_syslog(LOG_INFO, CurEnv->e_id,
						  "invalid domain name (%s) from %.100s",
						  p, CurSmtpClient);
				break;
			}
			else
			{
				q = "accepting invalid domain name";
			}

			if (gothello)
			{
				CLEAR_STATE(cmdbuf);

#if _FFR_QUARANTINE
				/* restore connection quarantining */
				if (smtp.sm_quarmsg == NULL)
				{
					e->e_quarmsg = NULL;
					macdefine(&e->e_macro, A_PERM,
						  macid("{quarantine}"), "");
				}
				else
				{
					e->e_quarmsg = sm_rpool_strdup_x(e->e_rpool,
									 smtp.sm_quarmsg);
					macdefine(&e->e_macro, A_PERM,
						  macid("{quarantine}"),
						  e->e_quarmsg);
				}
#endif /* _FFR_QUARANTINE */
			}

#if MILTER
			if (smtp.sm_milterlist && smtp.sm_milterize &&
			    !bitset(EF_DISCARD, e->e_flags))
			{
				char state;
				char *response;

				response = milter_helo(p, e, &state);
				switch (state)
				{
				  case SMFIR_REPLYCODE:
					if (MilterLogLevel > 3)
						sm_syslog(LOG_INFO, e->e_id,
							  "Milter: helo=%s, reject=%s",
							  p, response);
					nullserver = newstr(response);
					smtp.sm_milterize = false;
					break;

				  case SMFIR_REJECT:
					if (MilterLogLevel > 3)
						sm_syslog(LOG_INFO, e->e_id,
							  "Milter: helo=%s, reject=Command rejected",
							  p);
					nullserver = "Command rejected";
					smtp.sm_milterize = false;
					break;

				  case SMFIR_TEMPFAIL:
					if (MilterLogLevel > 3)
						sm_syslog(LOG_INFO, e->e_id,
							  "Milter: helo=%s, reject=%s",
							  p, MSG_TEMPFAIL);
					tempfail = true;
					smtp.sm_milterize = false;
					break;
				}
				if (response != NULL)
					sm_free(response);

# if _FFR_QUARANTINE
				/*
				**  If quarantining by a connect/ehlo action,
				**  save between messages
				*/

				if (smtp.sm_quarmsg == NULL &&
				    e->e_quarmsg != NULL)
					smtp.sm_quarmsg = newstr(e->e_quarmsg);
# endif /* _FFR_QUARANTINE */
			}
#endif /* MILTER */
			gothello = true;

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
			**	  remember to update 'helpfile'
			*/

			message("250-ENHANCEDSTATUSCODES");
#if PIPELINING
			if (bitset(SRV_OFFER_PIPE, features))
				message("250-PIPELINING");
#endif /* PIPELINING */
			if (bitset(SRV_OFFER_EXPN, features))
			{
				message("250-EXPN");
				if (bitset(SRV_OFFER_VERB, features))
					message("250-VERB");
			}
#if MIME8TO7
			message("250-8BITMIME");
#endif /* MIME8TO7 */
			if (MaxMessageSize > 0)
				message("250-SIZE %ld", MaxMessageSize);
			else
				message("250-SIZE");
#if DSN
			if (SendMIMEErrors && bitset(SRV_OFFER_DSN, features))
				message("250-DSN");
#endif /* DSN */
			if (bitset(SRV_OFFER_ETRN, features))
				message("250-ETRN");
#if SASL
			if (sasl_ok && mechlist != NULL && *mechlist != '\0')
				message("250-AUTH %s", mechlist);
#endif /* SASL */
#if STARTTLS
			if (tls_ok_srv &&
			    bitset(SRV_OFFER_TLS, features))
				message("250-STARTTLS");
#endif /* STARTTLS */
			if (DeliverByMin > 0)
				message("250-DELIVERBY %ld",
					(long) DeliverByMin);
			else if (DeliverByMin == 0)
				message("250-DELIVERBY");

			/* < 0: no deliver-by */

			message("250 HELP");
			break;

		  case CMDMAIL:		/* mail -- designate sender */
			SmtpPhase = "server MAIL";
			DELAY_CONN("MAIL");

			/* check for validity of this command */
			if (!gothello && bitset(PRIV_NEEDMAILHELO, PrivacyFlags))
			{
				usrerr("503 5.0.0 Polite people say HELO first");
				break;
			}
			if (smtp.sm_gotmail)
			{
				usrerr("503 5.5.0 Sender already specified");
				break;
			}
#if SASL
			if (bitset(SRV_REQ_AUTH, features) &&
			    authenticating != SASL_IS_AUTH)
			{
				usrerr("530 5.7.0 Authentication required");
				break;
			}
#endif /* SASL */

			p = skipword(p, "from");
			if (p == NULL)
				break;
			if (tempfail)
			{
				if (LogLevel > 9)
					sm_syslog(LOG_INFO, e->e_id,
						  "SMTP MAIL command (%.100s) from %s tempfailed (due to previous checks)",
						  p, CurSmtpClient);
				usrerr(MSG_TEMPFAIL);
				break;
			}

			/* make sure we know who the sending host is */
			if (sendinghost == NULL)
				sendinghost = peerhostname;


#if SM_HEAP_CHECK
			if (sm_debug_active(&DebugLeakSmtp, 1))
			{
				sm_heap_newgroup();
				sm_dprintf("smtp() heap group #%d\n",
					sm_heap_group());
			}
#endif /* SM_HEAP_CHECK */

			if (Errors > 0)
				goto undo_no_pm;
			if (!gothello)
			{
				auth_warning(e, "%s didn't use HELO protocol",
					     CurSmtpClient);
			}
#ifdef PICKY_HELO_CHECK
			if (sm_strcasecmp(sendinghost, peerhostname) != 0 &&
			    (sm_strcasecmp(peerhostname, "localhost") != 0 ||
			     sm_strcasecmp(sendinghost, MyHostName) != 0))
			{
				auth_warning(e, "Host %s claimed to be %s",
					     CurSmtpClient, sendinghost);
			}
#endif /* PICKY_HELO_CHECK */

			if (protocol == NULL)
				protocol = "SMTP";
			macdefine(&e->e_macro, A_PERM, 'r', protocol);
			macdefine(&e->e_macro, A_PERM, 's', sendinghost);

			if (Errors > 0)
				goto undo_no_pm;
			smtp.sm_nrcpts = 0;
			n_badrcpts = 0;
			macdefine(&e->e_macro, A_PERM, macid("{ntries}"), "0");
			macdefine(&e->e_macro, A_PERM, macid("{nrcpts}"), "0");
			e->e_flags |= EF_CLRQUEUE;
			sm_setproctitle(true, e, "%s %s: %.80s",
					qid_printname(e),
					CurSmtpClient, inp);

			/* do the processing */
		    SM_TRY
		    {
			extern char *FullName;

			QuickAbort = true;
			SM_FREE_CLR(FullName);

			/* must parse sender first */
			delimptr = NULL;
			setsender(p, e, &delimptr, ' ', false);
			if (delimptr != NULL && *delimptr != '\0')
				*delimptr++ = '\0';
			if (Errors > 0)
				sm_exc_raisenew_x(&EtypeQuickAbort, 1);

			/* Successfully set e_from, allow logging */
			e->e_flags |= EF_LOGSENDER;

			/* put resulting triple from parseaddr() into macros */
			if (e->e_from.q_mailer != NULL)
				 macdefine(&e->e_macro, A_PERM,
					macid("{mail_mailer}"),
					e->e_from.q_mailer->m_name);
			else
				 macdefine(&e->e_macro, A_PERM,
					macid("{mail_mailer}"), NULL);
			if (e->e_from.q_host != NULL)
				macdefine(&e->e_macro, A_PERM,
					macid("{mail_host}"),
					e->e_from.q_host);
			else
				macdefine(&e->e_macro, A_PERM,
					macid("{mail_host}"), "localhost");
			if (e->e_from.q_user != NULL)
				macdefine(&e->e_macro, A_PERM,
					macid("{mail_addr}"),
					e->e_from.q_user);
			else
				macdefine(&e->e_macro, A_PERM,
					macid("{mail_addr}"), NULL);
			if (Errors > 0)
				sm_exc_raisenew_x(&EtypeQuickAbort, 1);

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

			/* reset to default value */
			SevenBitInput = save_sevenbitinput;

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
				SKIP_SPACE(p);
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
					sm_dprintf("MAIL: got arg %s=\"%s\"\n", kp,
						vp == NULL ? "<null>" : vp);

				mail_esmtp_args(kp, vp, e);
				if (equal != NULL)
					*equal = '=';
				args[argno++] = kp;
				if (argno >= MAXSMTPARGS - 1)
					usrerr("501 5.5.4 Too many parameters");
				if (Errors > 0)
					sm_exc_raisenew_x(&EtypeQuickAbort, 1);
			}
			args[argno] = NULL;
			if (Errors > 0)
				sm_exc_raisenew_x(&EtypeQuickAbort, 1);

#if SASL
# if _FFR_AUTH_PASSING
			/* set the default AUTH= if the sender didn't */
			if (e->e_auth_param == NULL)
			{
				/* XXX only do this for an MSA? */
				e->e_auth_param = macvalue(macid("{auth_authen}"),
							   e);
				if (e->e_auth_param == NULL)
					e->e_auth_param = "<>";

				/*
				**  XXX should we invoke Strust_auth now?
				**  authorizing as the client that just
				**  authenticated, so we'll trust implicitly
				*/
			}
# endif /* _FFR_AUTH_PASSING */
#endif /* SASL */

			/* do config file checking of the sender */
			macdefine(&e->e_macro, A_PERM,
				macid("{addr_type}"), "e s");
#if _FFR_MAIL_MACRO
			/* make the "real" sender address available */
			macdefine(&e->e_macro, A_TEMP, macid("{mail_from}"),
				  e->e_from.q_paddr);
#endif /* _FFR_MAIL_MACRO */
			if (rscheck("check_mail", addr,
				    NULL, e, RSF_RMCOMM|RSF_COUNT, 3,
				    NULL, e->e_id) != EX_OK ||
			    Errors > 0)
				sm_exc_raisenew_x(&EtypeQuickAbort, 1);
			macdefine(&e->e_macro, A_PERM,
				  macid("{addr_type}"), NULL);

			if (MaxMessageSize > 0 &&
			    (e->e_msgsize > MaxMessageSize ||
			     e->e_msgsize < 0))
			{
				usrerr("552 5.2.3 Message size exceeds fixed maximum message size (%ld)",
					MaxMessageSize);
				sm_exc_raisenew_x(&EtypeQuickAbort, 1);
			}

			/*
			**  XXX always check whether there is at least one fs
			**  with enough space?
			**  However, this may not help much: the queue group
			**  selection may later on select a FS that hasn't
			**  enough space.
			*/

			if ((NumFileSys == 1 || NumQueue == 1) &&
			    !enoughdiskspace(e->e_msgsize, e)
#if _FFR_ANY_FREE_FS
			    && !filesys_free(e->e_msgsize)
#endif /* _FFR_ANY_FREE_FS */
			   )
			{
				/*
				**  We perform this test again when the
				**  queue directory is selected, in collect.
				*/

				usrerr("452 4.4.5 Insufficient disk space; try again later");
				sm_exc_raisenew_x(&EtypeQuickAbort, 1);
			}
			if (Errors > 0)
				sm_exc_raisenew_x(&EtypeQuickAbort, 1);

			LogUsrErrs = true;
#if MILTER
			if (smtp.sm_milterlist && smtp.sm_milterize &&
			    !bitset(EF_DISCARD, e->e_flags))
			{
				char state;
				char *response;

				response = milter_envfrom(args, e, &state);
				MILTER_REPLY("from");
			}
#endif /* MILTER */
			if (Errors > 0)
				sm_exc_raisenew_x(&EtypeQuickAbort, 1);

			message("250 2.1.0 Sender ok");
			smtp.sm_gotmail = true;
		    }
		    SM_EXCEPT(exc, "[!F]*")
		    {
			/*
			**  An error occurred while processing a MAIL command.
			**  Jump to the common error handling code.
			*/

			sm_exc_free(exc);
			goto undo_no_pm;
		    }
		    SM_END_TRY
			break;

		  undo_no_pm:
			e->e_flags &= ~EF_PM_NOTIFY;
		  undo:
			break;

		  case CMDRCPT:		/* rcpt -- designate recipient */
			DELAY_CONN("RCPT");
			if (BadRcptThrottle > 0 &&
			    n_badrcpts >= BadRcptThrottle)
			{
				if (LogLevel > 5 &&
				    n_badrcpts == BadRcptThrottle)
				{
					sm_syslog(LOG_INFO, e->e_id,
						  "%s: Possible SMTP RCPT flood, throttling.",
						  CurSmtpClient);

					/* To avoid duplicated message */
					n_badrcpts++;
				}

				/*
				**  Don't use exponential backoff for now.
				**  Some servers will open more connections
				**  and actually overload the receiver even
				**  more.
				*/

				(void) sleep(1);
			}
			if (!smtp.sm_gotmail)
			{
				usrerr("503 5.0.0 Need MAIL before RCPT");
				break;
			}
			SmtpPhase = "server RCPT";
		    SM_TRY
		    {
			QuickAbort = true;
			LogUsrErrs = true;

			/* limit flooding of our machine */
			if (MaxRcptPerMsg > 0 &&
			    smtp.sm_nrcpts >= MaxRcptPerMsg)
			{
				/* sleep(1); / * slow down? */
				usrerr("452 4.5.3 Too many recipients");
				goto rcpt_done;
			}

			if (e->e_sendmode != SM_DELIVER)
				e->e_flags |= EF_VRFYONLY;

#if MILTER
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
#endif /* MILTER */

			p = skipword(p, "to");
			if (p == NULL)
				goto rcpt_done;
			macdefine(&e->e_macro, A_PERM,
				macid("{addr_type}"), "e r");
			a = parseaddr(p, NULLADDR, RF_COPYALL, ' ', &delimptr,
				      e, true);
			macdefine(&e->e_macro, A_PERM,
				macid("{addr_type}"), NULL);
			if (Errors > 0)
				goto rcpt_done;
			if (a == NULL)
			{
				usrerr("501 5.0.0 Missing recipient");
				goto rcpt_done;
			}

			if (delimptr != NULL && *delimptr != '\0')
				*delimptr++ = '\0';

			/* put resulting triple from parseaddr() into macros */
			if (a->q_mailer != NULL)
				macdefine(&e->e_macro, A_PERM,
					macid("{rcpt_mailer}"),
					a->q_mailer->m_name);
			else
				macdefine(&e->e_macro, A_PERM,
					macid("{rcpt_mailer}"), NULL);
			if (a->q_host != NULL)
				macdefine(&e->e_macro, A_PERM,
					macid("{rcpt_host}"), a->q_host);
			else
				macdefine(&e->e_macro, A_PERM,
					macid("{rcpt_host}"), "localhost");
			if (a->q_user != NULL)
				macdefine(&e->e_macro, A_PERM,
					macid("{rcpt_addr}"), a->q_user);
			else
				macdefine(&e->e_macro, A_PERM,
					macid("{rcpt_addr}"), NULL);
			if (Errors > 0)
				goto rcpt_done;

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
				SKIP_SPACE(p);
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
					sm_dprintf("RCPT: got arg %s=\"%s\"\n", kp,
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
				goto rcpt_done;

			/* do config file checking of the recipient */
			macdefine(&e->e_macro, A_PERM,
				macid("{addr_type}"), "e r");
			if (rscheck("check_rcpt", addr,
				    NULL, e, RSF_RMCOMM|RSF_COUNT, 3,
				    NULL, e->e_id) != EX_OK ||
			    Errors > 0)
				goto rcpt_done;
			macdefine(&e->e_macro, A_PERM,
				macid("{addr_type}"), NULL);

			/* If discarding, don't bother to verify user */
			if (bitset(EF_DISCARD, e->e_flags))
				a->q_state = QS_VERIFIED;

#if MILTER
			if (smtp.sm_milterlist && smtp.sm_milterize &&
			    !bitset(EF_DISCARD, e->e_flags))
			{
				char state;
				char *response;

				response = milter_envrcpt(args, e, &state);
				MILTER_REPLY("to");
			}
#endif /* MILTER */

			macdefine(&e->e_macro, A_PERM,
				macid("{rcpt_mailer}"), NULL);
			macdefine(&e->e_macro, A_PERM,
				macid("{rcpt_host}"), NULL);
			macdefine(&e->e_macro, A_PERM,
				macid("{rcpt_addr}"), NULL);
			macdefine(&e->e_macro, A_PERM,
				macid("{dsn_notify}"), NULL);
			if (Errors > 0)
				goto rcpt_done;

			/* save in recipient list after ESMTP mods */
			a = recipient(a, &e->e_sendqueue, 0, e);
			if (Errors > 0)
				goto rcpt_done;

			/* no errors during parsing, but might be a duplicate */
			e->e_to = a->q_paddr;
			if (!QS_IS_BADADDR(a->q_state))
			{
				if (smtp.sm_nrcpts == 0)
					initsys(e);
				message("250 2.1.5 Recipient ok%s",
					QS_IS_QUEUEUP(a->q_state) ?
						" (will queue)" : "");
				smtp.sm_nrcpts++;
			}
			else
			{
				/* punt -- should keep message in ADDRESS.... */
				usrerr("550 5.1.1 Addressee unknown");
			}
		    rcpt_done:
			if (Errors > 0)
				++n_badrcpts;
		    }
		    SM_EXCEPT(exc, "[!F]*")
		    {
			/* An exception occurred while processing RCPT */
			e->e_flags &= ~(EF_FATALERRS|EF_PM_NOTIFY);
			++n_badrcpts;
		    }
		    SM_END_TRY
			break;

		  case CMDDATA:		/* data -- text of mail */
			DELAY_CONN("DATA");
			smtp_data(&smtp, e);
			break;

		  case CMDRSET:		/* rset -- reset state */
			if (tTd(94, 100))
				message("451 4.0.0 Test failure");
			else
				message("250 2.0.0 Reset state");
			CLEAR_STATE(cmdbuf);
#if _FFR_QUARANTINE
			/* restore connection quarantining */
			if (smtp.sm_quarmsg == NULL)
			{
				e->e_quarmsg = NULL;
				macdefine(&e->e_macro, A_PERM,
					  macid("{quarantine}"), "");
			}
			else
			{
				e->e_quarmsg = sm_rpool_strdup_x(e->e_rpool,
								 smtp.sm_quarmsg);
				macdefine(&e->e_macro, A_PERM,
					  macid("{quarantine}"), e->e_quarmsg);
			}
#endif /* _FFR_QUARANTINE */
			break;

		  case CMDVRFY:		/* vrfy -- verify address */
		  case CMDEXPN:		/* expn -- expand address */
			vrfy = c->cmd_code == CMDVRFY;
			DELAY_CONN(vrfy ? "VRFY" : "EXPN");
			if (tempfail)
			{
				if (LogLevel > 9)
					sm_syslog(LOG_INFO, e->e_id,
						  "SMTP %s command (%.100s) from %s tempfailed (due to previous checks)",
						  vrfy ? "VRFY" : "EXPN",
						  p, CurSmtpClient);

				/* RFC 821 doesn't allow 4xy reply code */
				usrerr("550 5.7.1 Please try again later");
				break;
			}
			wt = checksmtpattack(&n_verifies, MAXVRFYCOMMANDS,
					     false, vrfy ? "VRFY" : "EXPN", e);
			previous = curtime();
			if ((vrfy && bitset(PRIV_NOVRFY, PrivacyFlags)) ||
			    (!vrfy && !bitset(SRV_OFFER_EXPN, features)))
			{
				if (vrfy)
					message("252 2.5.2 Cannot VRFY user; try RCPT to attempt delivery (or try finger)");
				else
					message("502 5.7.0 Sorry, we do not allow this operation");
				if (LogLevel > 5)
					sm_syslog(LOG_INFO, e->e_id,
						  "%s: %s [rejected]",
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
			if (Errors > 0)
				break;
			if (LogLevel > 5)
				sm_syslog(LOG_INFO, e->e_id, "%s: %s",
					  CurSmtpClient,
					  shortenstring(inp, MAXSHORTSTR));
		    SM_TRY
		    {
			QuickAbort = true;
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
					    p, NULL, e, RSF_RMCOMM,
					    3, NULL, NOQID) != EX_OK ||
				    Errors > 0)
					sm_exc_raisenew_x(&EtypeQuickAbort, 1);
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
				sm_exc_raisenew_x(&EtypeQuickAbort, 1);
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
		    }
		    SM_EXCEPT(exc, "[!F]*")
		    {
			/*
			**  An exception occurred while processing VRFY/EXPN
			*/

			sm_exc_free(exc);
			goto undo;
		    }
		    SM_END_TRY
			break;

		  case CMDETRN:		/* etrn -- force queue flush */
			DELAY_CONN("ETRN");

			/* Don't leak queue information via debug flags */
			if (!bitset(SRV_OFFER_ETRN, features) || UseMSP ||
			    (RealUid != 0 && RealUid != TrustedUid &&
			     OpMode == MD_SMTP))
			{
				/* different message for MSA ? */
				message("502 5.7.0 Sorry, we do not allow this operation");
				if (LogLevel > 5)
					sm_syslog(LOG_INFO, e->e_id,
						  "%s: %s [rejected]",
						  CurSmtpClient,
						  shortenstring(inp, MAXSHORTSTR));
				break;
			}
			if (tempfail)
			{
				if (LogLevel > 9)
					sm_syslog(LOG_INFO, e->e_id,
						  "SMTP ETRN command (%.100s) from %s tempfailed (due to previous checks)",
						  p, CurSmtpClient);
				usrerr(MSG_TEMPFAIL);
				break;
			}

			if (strlen(p) <= 0)
			{
				usrerr("500 5.5.2 Parameter required");
				break;
			}

			/* crude way to avoid denial-of-service attacks */
			(void) checksmtpattack(&n_etrn, MAXETRNCOMMANDS, true,
					     "ETRN", e);

			/*
			**  Do config file checking of the parameter.
			**  Even though we have srv_features now, we still
			**  need this ruleset because the former is called
			**  when the connection has been established, while
			**  this ruleset is called when the command is
			**  actually issued and therefore has all information
			**  available to make a decision.
			*/

			if (rscheck("check_etrn", p, NULL, e,
				    RSF_RMCOMM, 3, NULL, NOQID) != EX_OK ||
			    Errors > 0)
				break;

			if (LogLevel > 5)
				sm_syslog(LOG_INFO, e->e_id,
					  "%s: ETRN %s", CurSmtpClient,
					  shortenstring(p, MAXSHORTSTR));

			id = p;
			if (*id == '#')
			{
				int i, qgrp;

				id++;
				qgrp = name2qid(id);
				if (!ISVALIDQGRP(qgrp))
				{
					usrerr("459 4.5.4 Queue %s unknown",
					       id);
					break;
				}
				for (i = 0; i < NumQueue && Queue[i] != NULL;
				     i++)
					Queue[i]->qg_nextrun = (time_t) -1;
				Queue[qgrp]->qg_nextrun = 0;
				ok = run_work_group(Queue[qgrp]->qg_wgrp,
						    RWG_FORK|RWG_FORCE);
				if (ok && Errors == 0)
					message("250 2.0.0 Queuing for queue group %s started", id);
				break;
			}

			if (*id == '@')
				id++;
			else
				*--id = '@';

			new = (QUEUE_CHAR *) sm_malloc(sizeof(QUEUE_CHAR));
			if (new == NULL)
			{
				syserr("500 5.5.0 ETRN out of memory");
				break;
			}
			new->queue_match = id;
			new->queue_negate = false;
			new->queue_next = NULL;
			QueueLimitRecipient = new;
			ok = runqueue(true, false, false, true);
			sm_free(QueueLimitRecipient); /* XXX */
			QueueLimitRecipient = NULL;
			if (ok && Errors == 0)
				message("250 2.0.0 Queuing for node %s started", p);
			break;

		  case CMDHELP:		/* help -- give user info */
			DELAY_CONN("HELP");
			help(p, e);
			break;

		  case CMDNOOP:		/* noop -- do nothing */
			DELAY_CONN("NOOP");
			(void) checksmtpattack(&n_noop, MAXNOOPCOMMANDS, true,
					       "NOOP", e);
			message("250 2.0.0 OK");
			break;

		  case CMDQUIT:		/* quit -- leave mail */
			message("221 2.0.0 %s closing connection", MyHostName);
#if PIPELINING
			(void) sm_io_flush(OutChannel, SM_TIME_DEFAULT);
#endif /* PIPELINING */

			if (smtp.sm_nrcpts > 0)
				logundelrcpts(e, "aborted by sender", 9, false);

			/* arrange to ignore any current send list */
			e->e_sendqueue = NULL;

#if STARTTLS
			/* shutdown TLS connection */
			if (tls_active)
			{
				(void) endtls(srv_ssl, "server");
				tls_active = false;
			}
#endif /* STARTTLS */
#if SASL
			if (authenticating == SASL_IS_AUTH)
			{
				sasl_dispose(&conn);
				authenticating = SASL_NOT_AUTH;
				/* XXX sasl_done(); this is a child */
			}
#endif /* SASL */

doquit:
			/* avoid future 050 messages */
			disconnect(1, e);

#if MILTER
			/* close out milter filters */
			milter_quit(e);
#endif /* MILTER */

			if (LogLevel > 4 && bitset(EF_LOGSENDER, e->e_flags))
				logsender(e, NULL);
			e->e_flags &= ~EF_LOGSENDER;

			if (lognullconnection && LogLevel > 5 &&
			    nullserver == NULL)
			{
				char *d;

				d = macvalue(macid("{daemon_name}"), e);
				if (d == NULL)
					d = "stdin";

				/*
				**  even though this id is "bogus", it makes
				**  it simpler to "grep" related events, e.g.,
				**  timeouts for the same connection.
				*/

				sm_syslog(LOG_INFO, e->e_id,
					  "%s did not issue MAIL/EXPN/VRFY/ETRN during connection to %s",
					  CurSmtpClient, d);
			}
			if (tTd(93, 100))
			{
				/* return to handle next connection */
				return;
			}
			finis(true, true, ExitStat);
			/* NOTREACHED */

		  case CMDVERB:		/* set verbose mode */
			DELAY_CONN("VERB");
			if (!bitset(SRV_OFFER_EXPN, features) ||
			    !bitset(SRV_OFFER_VERB, features))
			{
				/* this would give out the same info */
				message("502 5.7.0 Verbose unavailable");
				break;
			}
			(void) checksmtpattack(&n_noop, MAXNOOPCOMMANDS, true,
					       "VERB", e);
			Verbose = 1;
			set_delivery_mode(SM_DELIVER, e);
			message("250 2.0.0 Verbose mode");
			break;

#if SMTPDEBUG
		  case CMDDBGQSHOW:	/* show queues */
			(void) sm_io_fprintf(smioout, SM_TIME_DEFAULT,
					     "Send Queue=");
			printaddr(e->e_sendqueue, true);
			break;

		  case CMDDBGDEBUG:	/* set debug mode */
			tTsetup(tTdvect, sizeof tTdvect, "0-99.1");
			tTflag(p);
			message("200 2.0.0 Debug set");
			break;

#else /* SMTPDEBUG */
		  case CMDDBGQSHOW:	/* show queues */
		  case CMDDBGDEBUG:	/* set debug mode */
#endif /* SMTPDEBUG */
		  case CMDLOGBOGUS:	/* bogus command */
			DELAY_CONN("Bogus");
			if (LogLevel > 0)
				sm_syslog(LOG_CRIT, e->e_id,
					  "\"%s\" command from %s (%.100s)",
					  c->cmd_name, CurSmtpClient,
					  anynet_ntoa(&RealHostAddr));
			/* FALLTHROUGH */

		  case CMDERROR:	/* unknown command */
#if MAXBADCOMMANDS > 0
			if (++n_badcmds > MAXBADCOMMANDS)
			{
				message("421 4.7.0 %s Too many bad commands; closing connection",
					MyHostName);

				/* arrange to ignore any current send list */
				e->e_sendqueue = NULL;
				goto doquit;
			}
#endif /* MAXBADCOMMANDS > 0 */

			usrerr("500 5.5.1 Command unrecognized: \"%s\"",
			       shortenstring(inp, MAXSHORTSTR));
			break;

		  case CMDUNIMPL:
			DELAY_CONN("Unimpl");
			usrerr("502 5.5.1 Command not implemented: \"%s\"",
			       shortenstring(inp, MAXSHORTSTR));
			break;

		  default:
			DELAY_CONN("default");
			errno = 0;
			syserr("500 5.5.0 smtp: unknown code %d", c->cmd_code);
			break;
		}
#if SASL
		}
#endif /* SASL */
	    }
	    SM_EXCEPT(exc, "[!F]*")
	    {
		/*
		**  The only possible exception is "E:mta.quickabort".
		**  There is nothing to do except fall through and loop.
		*/
	    }
	    SM_END_TRY
	}
}
/*
**  SMTP_DATA -- implement the SMTP DATA command.
**
**	Parameters:
**		smtp -- status of SMTP connection.
**		e -- envelope.
**
**	Returns:
**		none.
**
**	Side Effects:
**		possibly sends message.
*/

static void
smtp_data(smtp, e)
	SMTP_T *smtp;
	ENVELOPE *e;
{
#if MILTER
	bool milteraccept;
#endif /* MILTER */
	bool aborting;
	bool doublequeue;
	ADDRESS *a;
	ENVELOPE *ee;
	char *id;
	char *oldid;
	char buf[32];

	SmtpPhase = "server DATA";
	if (!smtp->sm_gotmail)
	{
		usrerr("503 5.0.0 Need MAIL command");
		return;
	}
	else if (smtp->sm_nrcpts <= 0)
	{
		usrerr("503 5.0.0 Need RCPT (recipient)");
		return;
	}
	(void) sm_snprintf(buf, sizeof buf, "%u", smtp->sm_nrcpts);
	if (rscheck("check_data", buf, NULL, e,
		    RSF_RMCOMM|RSF_UNSTRUCTURED|RSF_COUNT, 3, NULL,
		    e->e_id) != EX_OK)
		return;

	/* put back discard bit */
	if (smtp->sm_discard)
		e->e_flags |= EF_DISCARD;

	/* check to see if we need to re-expand aliases */
	/* also reset QS_BADADDR on already-diagnosted addrs */
	doublequeue = false;
	for (a = e->e_sendqueue; a != NULL; a = a->q_next)
	{
		if (QS_IS_VERIFIED(a->q_state) &&
		    !bitset(EF_DISCARD, e->e_flags))
		{
			/* need to re-expand aliases */
			doublequeue = true;
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

#if _FFR_ADAPTIVE_EOL
	/* triggers error in collect, disabled for now */
	if (smtp->sm_crlf)
		e->e_flags |= EF_NL_NOT_EOL;
#endif /* _FFR_ADAPTIVE_EOL */

	collect(InChannel, true, NULL, e, true);

	/* redefine message size */
	(void) sm_snprintf(buf, sizeof buf, "%ld", e->e_msgsize);
	macdefine(&e->e_macro, A_TEMP, macid("{msg_size}"), buf);

#if _FFR_CHECK_EOM
	/* rscheck() will set Errors or EF_DISCARD if it trips */
	(void) rscheck("check_eom", buf, NULL, e, RSF_UNSTRUCTURED|RSF_COUNT,
		       3, NULL, e->e_id);
#endif /* _FFR_CHECK_EOM */

#if MILTER
	milteraccept = true;
	if (smtp->sm_milterlist && smtp->sm_milterize &&
	    Errors <= 0 &&
	    !bitset(EF_DISCARD, e->e_flags))
	{
		char state;
		char *response;

		response = milter_data(e, &state);
		switch (state)
		{
		  case SMFIR_REPLYCODE:
			if (MilterLogLevel > 3)
				sm_syslog(LOG_INFO, e->e_id,
					  "Milter: data, reject=%s",
					  response);
			milteraccept = false;
			usrerr(response);
			break;

		  case SMFIR_REJECT:
			milteraccept = false;
			if (MilterLogLevel > 3)
				sm_syslog(LOG_INFO, e->e_id,
					  "Milter: data, reject=554 5.7.1 Command rejected");
			usrerr("554 5.7.1 Command rejected");
			break;

		  case SMFIR_DISCARD:
			if (MilterLogLevel > 3)
				sm_syslog(LOG_INFO, e->e_id,
					  "Milter: data, discard");
			milteraccept = false;
			e->e_flags |= EF_DISCARD;
			break;

		  case SMFIR_TEMPFAIL:
			if (MilterLogLevel > 3)
				sm_syslog(LOG_INFO, e->e_id,
					  "Milter: data, reject=%s",
					  MSG_TEMPFAIL);
			milteraccept = false;
			usrerr(MSG_TEMPFAIL);
			break;
		}
		if (response != NULL)
			sm_free(response);
	}

	/* Milter may have changed message size */
	(void) sm_snprintf(buf, sizeof buf, "%ld", e->e_msgsize);
	macdefine(&e->e_macro, A_TEMP, macid("{msg_size}"), buf);

	/* abort message filters that didn't get the body & log msg is OK */
	if (smtp->sm_milterlist && smtp->sm_milterize)
	{
		milter_abort(e);
		if (milteraccept && MilterLogLevel > 9)
			sm_syslog(LOG_INFO, e->e_id, "Milter accept: message");
	}
#endif /* MILTER */

#if _FFR_QUARANTINE
	/* Check if quarantining stats should be updated */
	if (e->e_quarmsg != NULL)
		markstats(e, NULL, STATS_QUARANTINE);
#endif /* _FFR_QUARANTINE */

	/*
	**  If a header/body check (header checks or milter)
	**  set EF_DISCARD, don't queueup the message --
	**  that would lose the EF_DISCARD bit and deliver
	**  the message.
	*/

	if (bitset(EF_DISCARD, e->e_flags))
		doublequeue = false;

	aborting = Errors > 0;
	if (!(aborting || bitset(EF_DISCARD, e->e_flags)) &&
#if _FFR_QUARANTINE
	    (QueueMode == QM_QUARANTINE || e->e_quarmsg == NULL) &&
#endif /* _FFR_QUARANTINE */
	    !split_by_recipient(e))
		aborting = bitset(EF_FATALERRS, e->e_flags);

	if (aborting)
	{
		/* Log who the mail would have gone to */
		logundelrcpts(e, e->e_message, 8, false);
		flush_errors(true);
		buffer_errors();
		goto abortmessage;
	}

	/* from now on, we have to operate silently */
	buffer_errors();

#if 0
	/*
	**  Clear message, it may contain an error from the SMTP dialogue.
	**  This error must not show up in the queue.
	**	Some error message should show up, e.g., alias database
	**	not available, but others shouldn't, e.g., from check_rcpt.
	*/

	e->e_message = NULL;
#endif /* 0 */

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
	(void) sm_io_setinfo(e->e_xfp, SM_BF_TRUNCATE, NULL);
	id = e->e_id;

#if NAMED_BIND
	_res.retry = TimeOuts.res_retry[RES_TO_FIRST];
	_res.retrans = TimeOuts.res_retrans[RES_TO_FIRST];
#endif /* NAMED_BIND */

	for (ee = e; ee != NULL; ee = ee->e_sibling)
	{
		/* make sure we actually do delivery */
		ee->e_flags &= ~EF_CLRQUEUE;

		/* from now on, operate silently */
		ee->e_errormode = EM_MAIL;

		if (doublequeue)
		{
			/* make sure it is in the queue */
			queueup(ee, false, true);
		}
		else
		{
			/* send to all recipients */
			sendall(ee, SM_DEFAULT);
		}
		ee->e_to = NULL;
	}

	/* put back id for SMTP logging in putoutmsg() */
	oldid = CurEnv->e_id;
	CurEnv->e_id = id;

	/* issue success message */
	message("250 2.0.0 %s Message accepted for delivery", id);
	CurEnv->e_id = oldid;

	/* if we just queued, poke it */
	if (doublequeue)
	{
		bool anything_to_send = false;

		sm_getla();
		for (ee = e; ee != NULL; ee = ee->e_sibling)
		{
			if (WILL_BE_QUEUED(ee->e_sendmode))
				continue;
			if (shouldqueue(ee->e_msgpriority, ee->e_ctime))
			{
				ee->e_sendmode = SM_QUEUE;
				continue;
			}
#if _FFR_QUARANTINE
			else if (QueueMode != QM_QUARANTINE &&
				 ee->e_quarmsg != NULL)
			{
				ee->e_sendmode = SM_QUEUE;
				continue;
			}
#endif /* _FFR_QUARANTINE */
			anything_to_send = true;

			/* close all the queue files */
			closexscript(ee);
			if (ee->e_dfp != NULL)
			{
				(void) sm_io_close(ee->e_dfp, SM_TIME_DEFAULT);
				ee->e_dfp = NULL;
			}
			unlockqueue(ee);
		}
		if (anything_to_send)
		{
#if PIPELINING
			/*
			**  XXX if we don't do this, we get 250 twice
			**	because it is also flushed in the child.
			*/

			(void) sm_io_flush(OutChannel, SM_TIME_DEFAULT);
#endif /* PIPELINING */
			(void) doworklist(e, true, true);
		}
	}

  abortmessage:
	if (LogLevel > 4 && bitset(EF_LOGSENDER, e->e_flags))
		logsender(e, NULL);
	e->e_flags &= ~EF_LOGSENDER;

	/* clean up a bit */
	smtp->sm_gotmail = false;

	/*
	**  Call dropenvelope if and only if the envelope is *not*
	**  being processed by the child process forked by doworklist().
	*/

	if (aborting || bitset(EF_DISCARD, e->e_flags))
		dropenvelope(e, true, false);
	else
	{
		for (ee = e; ee != NULL; ee = ee->e_sibling)
		{
#if _FFR_QUARANTINE
			if (!doublequeue &&
			    QueueMode != QM_QUARANTINE &&
			    ee->e_quarmsg != NULL)
			{
				dropenvelope(ee, true, false);
				continue;
			}
#endif /* _FFR_QUARANTINE */
			if (WILL_BE_QUEUED(ee->e_sendmode))
				dropenvelope(ee, true, false);
		}
	}
	sm_rpool_free(e->e_rpool);

	/*
	**  At this point, e == &MainEnvelope, but if we did splitting,
	**  then CurEnv may point to an envelope structure that was just
	**  freed with the rpool.  So reset CurEnv *before* calling
	**  newenvelope.
	*/

	CurEnv = e;
	newenvelope(e, e, sm_rpool_new_x(NULL));
	e->e_flags = BlankEnvelope.e_flags;

#if _FFR_QUARANTINE
	/* restore connection quarantining */
	if (smtp->sm_quarmsg == NULL)
	{
		e->e_quarmsg = NULL;
		macdefine(&e->e_macro, A_PERM, macid("{quarantine}"), "");
	}
	else
	{
		e->e_quarmsg = sm_rpool_strdup_x(e->e_rpool, smtp->sm_quarmsg);
		macdefine(&e->e_macro, A_PERM,
			  macid("{quarantine}"), e->e_quarmsg);
	}
#endif /* _FFR_QUARANTINE */
}
/*
**  LOGUNDELRCPTS -- log undelivered (or all) recipients.
**
**	Parameters:
**		e -- envelope.
**		msg -- message for Stat=
**		level -- log level.
**		all -- log all recipients.
**
**	Returns:
**		none.
**
**	Side Effects:
**		logs undelivered (or all) recipients
*/

void
logundelrcpts(e, msg, level, all)
	ENVELOPE *e;
	char *msg;
	int level;
	bool all;
{
	ADDRESS *a;

	if (LogLevel <= level || msg == NULL || *msg == '\0')
		return;

	/* Clear $h so relay= doesn't get mislogged by logdelivery() */
	macdefine(&e->e_macro, A_PERM, 'h', NULL);

	/* Log who the mail would have gone to */
	for (a = e->e_sendqueue; a != NULL; a = a->q_next)
	{
		if (!QS_IS_UNDELIVERED(a->q_state) && !all)
			continue;
		e->e_to = a->q_paddr;
		logdelivery(NULL, NULL, a->q_status, msg, NULL,
			    (time_t) 0, e);
	}
	e->e_to = NULL;
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
	volatile unsigned int *pcounter;
	int maxcount;
	bool waitnow;
	char *cname;
	ENVELOPE *e;
{
	if (maxcount <= 0)	/* no limit */
		return (time_t) 0;

	if (++(*pcounter) >= maxcount)
	{
		time_t s;

		if (*pcounter == maxcount && LogLevel > 5)
		{
			sm_syslog(LOG_INFO, e->e_id,
				  "%s: possible SMTP attack: command=%.40s, count=%u",
				  CurSmtpClient, cname, *pcounter);
		}
		s = 1 << (*pcounter - maxcount);
		if (s >= MAXTIMEOUT || s <= 0)
			s = MAXTIMEOUT;

		/* sleep at least 1 second before returning */
		(void) sleep(*pcounter / maxcount);
		s -= *pcounter / maxcount;
		if (waitnow)
		{
			(void) sleep(s);
			return 0;
		}
		return s;
	}
	return (time_t) 0;
}
/*
**  SETUP_SMTPD_IO -- setup I/O fd correctly for the SMTP server
**
**	Parameters:
**		none.
**
**	Returns:
**		nothing.
**
**	Side Effects:
**		may change I/O fd.
*/

static void
setup_smtpd_io()
{
	int inchfd, outchfd, outfd;

	inchfd = sm_io_getinfo(InChannel, SM_IO_WHAT_FD, NULL);
	outchfd  = sm_io_getinfo(OutChannel, SM_IO_WHAT_FD, NULL);
	outfd = sm_io_getinfo(smioout, SM_IO_WHAT_FD, NULL);
	if (outchfd != outfd)
	{
		/* arrange for debugging output to go to remote host */
		(void) dup2(outchfd, outfd);
	}

	/*
	**  if InChannel and OutChannel are stdin/stdout
	**  and connected to ttys
	**  and fcntl(STDIN, F_SETFL, O_NONBLOCKING) also changes STDOUT,
	**  then "chain" them together.
	*/

	if (inchfd == STDIN_FILENO && outchfd == STDOUT_FILENO &&
	    isatty(inchfd) && isatty(outchfd))
	{
		int inmode, outmode;

		inmode = fcntl(inchfd, F_GETFL, 0);
		if (inmode == -1)
		{
			if (LogLevel > 11)
				sm_syslog(LOG_INFO, NOQID,
					"fcntl(inchfd, F_GETFL) failed: %s",
					sm_errstring(errno));
			return;
		}
		outmode = fcntl(outchfd, F_GETFL, 0);
		if (outmode == -1)
		{
			if (LogLevel > 11)
				sm_syslog(LOG_INFO, NOQID,
					"fcntl(outchfd, F_GETFL) failed: %s",
					sm_errstring(errno));
			return;
		}
		if (bitset(O_NONBLOCK, inmode) ||
		    bitset(O_NONBLOCK, outmode) ||
		    fcntl(inchfd, F_SETFL, inmode | O_NONBLOCK) == -1)
			return;
		outmode = fcntl(outchfd, F_GETFL, 0);
		if (outmode != -1 && bitset(O_NONBLOCK, outmode))
		{
			/* changing InChannel also changes OutChannel */
			sm_io_automode(OutChannel, InChannel);
			if (tTd(97, 4) && LogLevel > 9)
				sm_syslog(LOG_INFO, NOQID,
					  "set automode for I (%d)/O (%d) in SMTP server",
					  inchfd, outchfd);
		}

		/* undo change of inchfd */
		(void) fcntl(inchfd, F_SETFL, inmode);
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
	register char *volatile p;
	char *w;
{
	register char *q;
	char *firstp = p;

	/* find beginning of word */
	SKIP_SPACE(p);
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
	SKIP_SPACE(p);

	if (*p == '\0')
		goto syntax;

	/* see if the input word matches desired word */
	if (sm_strcasecmp(q, w))
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
	if (sm_strcasecmp(kp, "size") == 0)
	{
		if (vp == NULL)
		{
			usrerr("501 5.5.2 SIZE requires a value");
			/* NOTREACHED */
		}
		macdefine(&e->e_macro, A_TEMP, macid("{msg_size}"), vp);
		errno = 0;
		e->e_msgsize = strtol(vp, (char **) NULL, 10);
		if (e->e_msgsize == LONG_MAX && errno == ERANGE)
		{
			usrerr("552 5.2.3 Message size exceeds maximum value");
			/* NOTREACHED */
		}
		if (e->e_msgsize < 0)
		{
			usrerr("552 5.2.3 Message size invalid");
			/* NOTREACHED */
		}
	}
	else if (sm_strcasecmp(kp, "body") == 0)
	{
		if (vp == NULL)
		{
			usrerr("501 5.5.2 BODY requires a value");
			/* NOTREACHED */
		}
		else if (sm_strcasecmp(vp, "8bitmime") == 0)
		{
			SevenBitInput = false;
		}
		else if (sm_strcasecmp(vp, "7bit") == 0)
		{
			SevenBitInput = true;
		}
		else
		{
			usrerr("501 5.5.4 Unknown BODY type %s", vp);
			/* NOTREACHED */
		}
		e->e_bodytype = sm_rpool_strdup_x(e->e_rpool, vp);
	}
	else if (sm_strcasecmp(kp, "envid") == 0)
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
		e->e_envid = sm_rpool_strdup_x(e->e_rpool, vp);
		macdefine(&e->e_macro, A_PERM,
			macid("{dsn_envid}"), e->e_envid);
	}
	else if (sm_strcasecmp(kp, "ret") == 0)
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
		if (sm_strcasecmp(vp, "hdrs") == 0)
			e->e_flags |= EF_NO_BODY_RETN;
		else if (sm_strcasecmp(vp, "full") != 0)
		{
			usrerr("501 5.5.2 Bad argument \"%s\" to RET", vp);
			/* NOTREACHED */
		}
		macdefine(&e->e_macro, A_TEMP, macid("{dsn_ret}"), vp);
	}
#if SASL
	else if (sm_strcasecmp(kp, "auth") == 0)
	{
		int len;
		char *q;
		char *auth_param;	/* the value of the AUTH=x */
		bool saveQuickAbort = QuickAbort;
		bool saveSuprErrs = SuprErrs;
		bool saveExitStat = ExitStat;
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
		(void) sm_strlcpy(auth_param, vp, len);
		if (!xtextok(auth_param))
		{
			usrerr("501 5.5.4 Syntax error in AUTH parameter value");
			/* just a warning? */
			/* NOTREACHED */
		}

		/* XXX this might be cut off */
		(void) sm_strlcpy(pbuf, xuntextify(auth_param), sizeof pbuf);
		/* xalloc() the buffer instead? */

		/* XXX define this always or only if trusted? */
		macdefine(&e->e_macro, A_TEMP, macid("{auth_author}"), pbuf);

		/*
		**  call Strust_auth to find out whether
		**  auth_param is acceptable (trusted)
		**  we shouldn't trust it if not authenticated
		**  (required by RFC, leave it to ruleset?)
		*/

		SuprErrs = true;
		QuickAbort = false;
		if (strcmp(auth_param, "<>") != 0 &&
		     (rscheck("trust_auth", pbuf, NULL, e, RSF_RMCOMM,
			      9, NULL, NOQID) != EX_OK || Errors > 0))
		{
			if (tTd(95, 8))
			{
				q = e->e_auth_param;
				sm_dprintf("auth=\"%.100s\" not trusted user=\"%.100s\"\n",
					pbuf, (q == NULL) ? "" : q);
			}

			/* not trusted */
			e->e_auth_param = "<>";
# if _FFR_AUTH_PASSING
			macdefine(&BlankEnvelope.e_macro, A_PERM,
				  macid("{auth_author}"), NULL);
# endif /* _FFR_AUTH_PASSING */
		}
		else
		{
			if (tTd(95, 8))
				sm_dprintf("auth=\"%.100s\" trusted\n", pbuf);
			e->e_auth_param = sm_rpool_strdup_x(e->e_rpool,
							    auth_param);
		}
		sm_free(auth_param); /* XXX */

		/* reset values */
		Errors = 0;
		QuickAbort = saveQuickAbort;
		SuprErrs = saveSuprErrs;
		ExitStat = saveExitStat;
	}
#endif /* SASL */
#define PRTCHAR(c)	((isascii(c) && isprint(c)) ? (c) : '?')

	/*
	**  "by" is only accepted if DeliverByMin >= 0.
	**  We maybe could add this to the list of server_features.
	*/

	else if (sm_strcasecmp(kp, "by") == 0 && DeliverByMin >= 0)
	{
		char *s;

		if (vp == NULL)
		{
			usrerr("501 5.5.2 BY= requires a value");
			/* NOTREACHED */
		}
		errno = 0;
		e->e_deliver_by = strtol(vp, &s, 10);
		if (e->e_deliver_by == LONG_MIN ||
		    e->e_deliver_by == LONG_MAX ||
		    e->e_deliver_by > 999999999l ||
		    e->e_deliver_by < -999999999l)
		{
			usrerr("501 5.5.2 BY=%s out of range", vp);
			/* NOTREACHED */
		}
		if (s == NULL || *s != ';')
		{
			usrerr("501 5.5.2 BY= missing ';'");
			/* NOTREACHED */
		}
		e->e_dlvr_flag = 0;
		++s;	/* XXX: spaces allowed? */
		SKIP_SPACE(s);
		switch (tolower(*s))
		{
		  case 'n':
			e->e_dlvr_flag = DLVR_NOTIFY;
			break;
		  case 'r':
			e->e_dlvr_flag = DLVR_RETURN;
			if (e->e_deliver_by <= 0)
			{
				usrerr("501 5.5.4 mode R requires BY time > 0");
				/* NOTREACHED */
			}
			if (DeliverByMin > 0 && e->e_deliver_by > 0 &&
			    e->e_deliver_by < DeliverByMin)
			{
				usrerr("555 5.5.2 time %ld less than %ld",
					e->e_deliver_by, (long) DeliverByMin);
				/* NOTREACHED */
			}
			break;
		  default:
			usrerr("501 5.5.2 illegal by-mode '%c'", PRTCHAR(*s));
			/* NOTREACHED */
		}
		++s;	/* XXX: spaces allowed? */
		SKIP_SPACE(s);
		switch (tolower(*s))
		{
		  case 't':
			e->e_dlvr_flag |= DLVR_TRACE;
			break;
		  case '\0':
			break;
		  default:
			usrerr("501 5.5.2 illegal by-trace '%c'", PRTCHAR(*s));
			/* NOTREACHED */
		}

		/* XXX: check whether more characters follow? */
	}
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
	if (sm_strcasecmp(kp, "notify") == 0)
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
		macdefine(&e->e_macro, A_TEMP, macid("{dsn_notify}"), vp);

		if (sm_strcasecmp(vp, "never") == 0)
			return;
		for (p = vp; p != NULL; vp = p)
		{
			p = strchr(p, ',');
			if (p != NULL)
				*p++ = '\0';
			if (sm_strcasecmp(vp, "success") == 0)
				a->q_flags |= QPINGONSUCCESS;
			else if (sm_strcasecmp(vp, "failure") == 0)
				a->q_flags |= QPINGONFAILURE;
			else if (sm_strcasecmp(vp, "delay") == 0)
				a->q_flags |= QPINGONDELAY;
			else
			{
				usrerr("501 5.5.4 Bad argument \"%s\"  to NOTIFY",
					vp);
				/* NOTREACHED */
			}
		}
	}
	else if (sm_strcasecmp(kp, "orcpt") == 0)
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
		a->q_orcpt = sm_rpool_strdup_x(e->e_rpool, vp);
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
**		a -- the address to print.
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
		(void) sm_strlcpy(fmtbuf, "252", sizeof fmtbuf);
	else
		(void) sm_strlcpy(fmtbuf, "250", sizeof fmtbuf);
	fmtbuf[3] = last ? ' ' : '-';
	(void) sm_strlcpy(&fmtbuf[4], "2.1.5 ", sizeof fmtbuf - 4);
	if (a->q_fullname == NULL)
	{
		if ((a->q_mailer == NULL ||
		     a->q_mailer->m_addrtype == NULL ||
		     sm_strcasecmp(a->q_mailer->m_addrtype, "rfc822") == 0) &&
		    strchr(a->q_user, '@') == NULL)
			(void) sm_strlcpy(&fmtbuf[OFFF], "<%s@%s>",
				       sizeof fmtbuf - OFFF);
		else
			(void) sm_strlcpy(&fmtbuf[OFFF], "<%s>",
				       sizeof fmtbuf - OFFF);
		message(fmtbuf, a->q_user, MyHostName);
	}
	else
	{
		if ((a->q_mailer == NULL ||
		     a->q_mailer->m_addrtype == NULL ||
		     sm_strcasecmp(a->q_mailer->m_addrtype, "rfc822") == 0) &&
		    strchr(a->q_user, '@') == NULL)
			(void) sm_strlcpy(&fmtbuf[OFFF], "%s <%s@%s>",
				       sizeof fmtbuf - OFFF);
		else
			(void) sm_strlcpy(&fmtbuf[OFFF], "%s <%s>",
				       sizeof fmtbuf - OFFF);
		message(fmtbuf, a->q_fullname, a->q_user, MyHostName);
	}
}

#if SASL
/*
**  SASLMECHS -- get list of possible AUTH mechanisms
**
**	Parameters:
**		conn -- SASL connection info.
**		mechlist -- output parameter for list of mechanisms.
**
**	Returns:
**		number of mechs.
*/

static int
saslmechs(conn, mechlist)
	sasl_conn_t *conn;
	char **mechlist;
{
	int len, num, result;

	/* "user" is currently unused */
# if SASL >= 20000
	result = sasl_listmech(conn, NULL,
			       "", " ", "", (const char **) mechlist,
			       (unsigned int *)&len, &num);
# else /* SASL >= 20000 */
	result = sasl_listmech(conn, "user", /* XXX */
			       "", " ", "", mechlist,
			       (unsigned int *)&len, (unsigned int *)&num);
# endif /* SASL >= 20000 */
	if (result != SASL_OK)
	{
		if (LogLevel > 9)
			sm_syslog(LOG_WARNING, NOQID,
				  "AUTH error: listmech=%d, num=%d",
				  result, num);
		num = 0;
	}
	if (num > 0)
	{
		if (LogLevel > 11)
			sm_syslog(LOG_INFO, NOQID,
				  "AUTH: available mech=%s, allowed mech=%s",
				  *mechlist, AuthMechanisms);
		*mechlist = intersect(AuthMechanisms, *mechlist, NULL);
	}
	else
	{
		*mechlist = NULL;	/* be paranoid... */
		if (result == SASL_OK && LogLevel > 9)
			sm_syslog(LOG_WARNING, NOQID,
				  "AUTH warning: no mechanisms");
	}
	return num;
}

# if SASL >= 20000
/*
**  PROXY_POLICY -- define proxy policy for AUTH
**
**	Parameters:
**		conn -- unused.
**		context -- unused.
**		requested_user -- authorization identity.
**		rlen -- authorization identity length.
**		auth_identity -- authentication identity.
**		alen -- authentication identity length.
**		def_realm -- default user realm.
**		urlen -- user realm length.
**		propctx -- unused.
**
**	Returns:
**		ok?
**
**	Side Effects:
**		sets {auth_authen} macro.
*/

int
proxy_policy(conn, context, requested_user, rlen, auth_identity, alen,
	     def_realm, urlen, propctx)
	sasl_conn_t *conn;
	void *context;
	const char *requested_user;
	unsigned rlen;
	const char *auth_identity;
	unsigned alen;
	const char *def_realm;
	unsigned urlen;
	struct propctx *propctx;
{
	if (auth_identity == NULL)
		return SASL_FAIL;

	macdefine(&BlankEnvelope.e_macro, A_TEMP,
		  macid("{auth_authen}"), (char *) auth_identity);

	return SASL_OK;
}
# else /* SASL >= 20000 */

/*
**  PROXY_POLICY -- define proxy policy for AUTH
**
**	Parameters:
**		context -- unused.
**		auth_identity -- authentication identity.
**		requested_user -- authorization identity.
**		user -- allowed user (output).
**		errstr -- possible error string (output).
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
# endif /* SASL >= 20000 */
#endif /* SASL */

#if STARTTLS
/*
**  INITSRVTLS -- initialize server side TLS
**
**	Parameters:
**		tls_ok -- should tls initialization be done?
**
**	Returns:
**		succeeded?
**
**	Side Effects:
**		sets tls_ok_srv which is a static variable in this module.
**		Do NOT remove assignments to it!
*/

bool
initsrvtls(tls_ok)
	bool tls_ok;
{
	if (!tls_ok)
		return false;

	/* do NOT remove assignment */
	tls_ok_srv = inittls(&srv_ctx, TLS_Srv_Opts, true, SrvCertFile,
			     SrvKeyFile, CACertPath, CACertFile, DHParams);
	return tls_ok_srv;
}
#endif /* STARTTLS */
/*
**  SRVFEATURES -- get features for SMTP server
**
**	Parameters:
**		e -- envelope (should be session context).
**		clientname -- name of client.
**		features -- default features for this invocation.
**
**	Returns:
**		server features.
*/

/* table with options: it uses just one character, how about strings? */
static struct
{
	char		srvf_opt;
	unsigned int	srvf_flag;
} srv_feat_table[] =
{
	{ 'A',	SRV_OFFER_AUTH	},
	{ 'B',	SRV_OFFER_VERB	},	/* FFR; not documented in 8.12 */
	{ 'D',	SRV_OFFER_DSN	},	/* FFR; not documented in 8.12 */
	{ 'E',	SRV_OFFER_ETRN	},	/* FFR; not documented in 8.12 */
	{ 'L',	SRV_REQ_AUTH	},	/* FFR; not documented in 8.12 */
#if PIPELINING
# if _FFR_NO_PIPE
	{ 'N',	SRV_NO_PIPE	},
# endif /* _FFR_NO_PIPE */
	{ 'P',	SRV_OFFER_PIPE	},
#endif /* PIPELINING */
	{ 'R',	SRV_VRFY_CLT	},	/* FFR; not documented in 8.12 */
	{ 'S',	SRV_OFFER_TLS	},
/*	{ 'T',	SRV_TMP_FAIL	},	*/
	{ 'V',	SRV_VRFY_CLT	},
	{ 'X',	SRV_OFFER_EXPN	},	/* FFR; not documented in 8.12 */
/*	{ 'Y',	SRV_OFFER_VRFY	},	*/
	{ '\0',	SRV_NONE	}
};

static unsigned int
srvfeatures(e, clientname, features)
	ENVELOPE *e;
	char *clientname;
	unsigned int features;
{
	int r, i, j;
	char **pvp, c, opt;
	char pvpbuf[PSBUFSIZE];

	pvp = NULL;
	r = rscap("srv_features", clientname, "", e, &pvp, pvpbuf,
		  sizeof(pvpbuf));
	if (r != EX_OK)
		return features;
	if (pvp == NULL || pvp[0] == NULL || (pvp[0][0] & 0377) != CANONNET)
		return features;
	if (pvp[1] != NULL && sm_strncasecmp(pvp[1], "temp", 4) == 0)
		return SRV_TMP_FAIL;

	/*
	**  General rule (see sendmail.h, d_flags):
	**  lower case: required/offered, upper case: Not required/available
	**
	**  Since we can change some features per daemon, we have both
	**  cases here: turn on/off a feature.
	*/

	for (i = 1; pvp[i] != NULL; i++)
	{
		c = pvp[i][0];
		j = 0;
		for (;;)
		{
			if ((opt = srv_feat_table[j].srvf_opt) == '\0')
			{
				if (LogLevel > 9)
					sm_syslog(LOG_WARNING, e->e_id,
						  "srvfeatures: unknown feature %s",
						  pvp[i]);
				break;
			}
			if (c == opt)
			{
				features &= ~(srv_feat_table[j].srvf_flag);
				break;
			}
			if (c == tolower(opt))
			{
				features |= srv_feat_table[j].srvf_flag;
				break;
			}
			++j;
		}
	}
	return features;
}

/*
**  HELP -- implement the HELP command.
**
**	Parameters:
**		topic -- the topic we want help for.
**		e -- envelope.
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
	register SM_FILE_T *hf;
	register char *p;
	int len;
	bool noinfo;
	bool first = true;
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
		noinfo = false;
	}
	else
	{
		makelower(topic);
		noinfo = true;
	}

	len = strlen(topic);

	while (sm_io_fgets(hf, SM_TIME_DEFAULT, buf, sizeof buf) != NULL)
	{
		if (buf[0] == '#')
		{
			if (foundvers < 0 &&
			    strncmp(buf, HELPVSTR, strlen(HELPVSTR)) == 0)
			{
				int h;

				if (sm_io_sscanf(buf + strlen(HELPVSTR), "%d",
						 &h) == 1)
					foundvers = h;
			}
			continue;
		}
		if (strncmp(buf, topic, len) == 0)
		{
			if (first)
			{
				first = false;

				/* print version if no/old vers# in file */
				if (foundvers < 2 && !noinfo)
					message("214-2.0.0 This is Sendmail version %s", Version);
			}
			p = strpbrk(buf, " \t");
			if (p == NULL)
				p = buf + strlen(buf) - 1;
			else
				p++;
			fixcrlf(p, true);
			if (foundvers >= 2)
			{
				translate_dollars(p);
				expand(p, inp, sizeof inp, e);
				p = inp;
			}
			message("214-2.0.0 %s", p);
			noinfo = false;
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

	(void) sm_io_close(hf, SM_TIME_DEFAULT);
}

#if SASL
/*
**  RESET_SASLCONN -- reset SASL connection data
**
**	Parameters:
**		conn -- SASL connection context
**		hostname -- host name
**		various connection data
**
**	Returns:
**		SASL result
*/

static int
reset_saslconn(sasl_conn_t ** conn, char *hostname,
# if SASL >= 20000
	       char *remoteip, char *localip,
	       char *auth_id, sasl_ssf_t * ext_ssf)
# else /* SASL >= 20000 */
	       struct sockaddr_in * saddr_r, struct sockaddr_in * saddr_l,
	       sasl_external_properties_t * ext_ssf)
# endif /* SASL >= 20000 */
{
	int result;

	sasl_dispose(conn);
# if SASL >= 20000
	result = sasl_server_new("smtp", hostname, NULL, NULL, NULL,
				 NULL, 0, conn);
# elif SASL > 10505
	/* use empty realm: only works in SASL > 1.5.5 */
	result = sasl_server_new("smtp", hostname, "", NULL, 0, conn);
# else /* SASL >= 20000 */
	/* use no realm -> realm is set to hostname by SASL lib */
	result = sasl_server_new("smtp", hostname, NULL, NULL, 0,
				 conn);
# endif /* SASL >= 20000 */
	if (result != SASL_OK)
		return result;

# if SASL >= 20000
#  if NETINET || NETINET6
	if (remoteip != NULL)
		result = sasl_setprop(*conn, SASL_IPREMOTEPORT, remoteip);
	if (result != SASL_OK)
		return result;

	if (localip != NULL)
		result = sasl_setprop(*conn, SASL_IPLOCALPORT, localip);
	if (result != SASL_OK)
		return result;
#  endif /* NETINET || NETINET6 */

	result = sasl_setprop(*conn, SASL_SSF_EXTERNAL, ext_ssf);
	if (result != SASL_OK)
		return result;

	result = sasl_setprop(*conn, SASL_AUTH_EXTERNAL, auth_id);
	if (result != SASL_OK)
		return result;
# else /* SASL >= 20000 */
#  if NETINET
	if (saddr_r != NULL)
		result = sasl_setprop(*conn, SASL_IP_REMOTE, saddr_r);
	if (result != SASL_OK)
		return result;

	if (saddr_l != NULL)
		result = sasl_setprop(*conn, SASL_IP_LOCAL, saddr_l);
	if (result != SASL_OK)
		return result;
#  endif /* NETINET */

	result = sasl_setprop(*conn, SASL_SSF_EXTERNAL, ext_ssf);
	if (result != SASL_OK)
		return result;
# endif /* SASL >= 20000 */
	return SASL_OK;
}
#endif /* SASL */
